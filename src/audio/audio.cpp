#include "pch.h"
#include "audio.h"
#include "audio_file.h"
#include "core/log.h"

#include <xaudio2.h>
#include <x3daudio.h>

#include <unordered_map>



static IXAudio2MasteringVoice* masterVoice;
static com<IXAudio2> xaudio;

float masterAudioVolume = 0.1f;
static float oldMasterAudioVolume;

struct voice_slot
{
	IXAudio2SourceVoice* voice;
	void* deleteOnPlaybackEnd;
	uint32 voiceKey;

	uint32 prev;
	uint32 next;

	uint32 generation;
};

#define NUM_VOICE_SLOTS 128
static voice_slot voiceSlots[NUM_VOICE_SLOTS];
static uint32 firstFreeVoiceSlot = 0;
static uint32 firstRunningVoiceSlot = -1;


static uint32 getFormatTag(const WAVEFORMATEX& wfx) noexcept
{
	if (wfx.wFormatTag == WAVE_FORMAT_EXTENSIBLE)
	{
		if (wfx.cbSize < (sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX)))
		{
			return 0;
		}

		static const GUID s_wfexBase = { 0x00000000, 0x0000, 0x0010, { 0x80, 0x00, 0x00, 0xAA, 0x00, 0x38, 0x9B, 0x71 } };

		const WAVEFORMATEXTENSIBLE& wfex = (const WAVEFORMATEXTENSIBLE&)wfx;

		if (memcmp(reinterpret_cast<const BYTE*>(&wfex.SubFormat) + sizeof(DWORD),
			reinterpret_cast<const BYTE*>(&s_wfexBase) + sizeof(DWORD), sizeof(GUID) - sizeof(DWORD)) != 0)
		{
			return 0;
		}

		return wfex.SubFormat.Data1;
	}
	else
	{
		return wfx.wFormatTag;
	}
}

static uint32 getDefaultChannelMask(int channels)
{
	switch (channels)
	{
		case 1: return SPEAKER_MONO;
		case 2: return SPEAKER_STEREO;
		case 3: return SPEAKER_2POINT1;
		case 4: return SPEAKER_QUAD;
		case 5: return SPEAKER_4POINT1;
		case 6: return SPEAKER_5POINT1;
		case 7: return SPEAKER_5POINT1 | SPEAKER_BACK_CENTER;
		case 8: return SPEAKER_7POINT1;
		default: return 0;
	}
}

static uint32 getFormatKey(const WAVEFORMATEX& x)
{
	union key_gen
	{
		struct
		{
			uint32 tag : 9;
			uint32 channels : 7;
			uint32 bitsPerSample : 8;
		} pcm;

		struct
		{
			uint32 tag : 9;
			uint32 channels : 7;
			uint32 samplesPerBlock : 16;
		} adpcm;

		uint32 key;
	} result;

	result.key = 0;

	static_assert(sizeof(key_gen) == sizeof(uint32), "KeyGen is invalid");

	if (x.wFormatTag == WAVE_FORMAT_EXTENSIBLE)
	{
		WAVEFORMATEXTENSIBLE xx = *(const WAVEFORMATEXTENSIBLE*)&x;

		if (xx.Samples.wValidBitsPerSample != 0 && xx.Samples.wValidBitsPerSample != x.wBitsPerSample)
		{
			return 0;
		}

		if (xx.dwChannelMask != 0 && xx.dwChannelMask != getDefaultChannelMask(x.nChannels))
		{
			return 0;
		}
	}

	uint32 tag = getFormatTag(x);
	switch (tag)
	{
		case WAVE_FORMAT_PCM:
			static_assert(WAVE_FORMAT_PCM < 0x1ff, "KeyGen tag is too small");
			result.pcm.tag = WAVE_FORMAT_PCM;
			result.pcm.channels = x.nChannels;
			result.pcm.bitsPerSample = x.wBitsPerSample;
			break;

		case WAVE_FORMAT_IEEE_FLOAT:
			static_assert(WAVE_FORMAT_IEEE_FLOAT < 0x1ff, "KeyGen tag is too small");

			if (x.wBitsPerSample != 32)
				return 0;

			result.pcm.tag = WAVE_FORMAT_IEEE_FLOAT;
			result.pcm.channels = x.nChannels;
			result.pcm.bitsPerSample = 32;
			break;

		case WAVE_FORMAT_ADPCM:
			static_assert(WAVE_FORMAT_ADPCM < 0x1ff, "KeyGen tag is too small");
			result.adpcm.tag = WAVE_FORMAT_ADPCM;
			result.adpcm.channels = x.nChannels;

			{
				const ADPCMWAVEFORMAT& wfadpcm = (const ADPCMWAVEFORMAT&)x;
				result.adpcm.samplesPerBlock = wfadpcm.wSamplesPerBlock;
			}
			break;

		default:
			return 0;
	}

	return result.key;
}

static void setVolume(IXAudio2SourceVoice* voice, float volume)
{
	checkResult(voice->SetVolume(volume));
}

static void setPitch(IXAudio2SourceVoice* voice, float pitch)
{
	checkResult(voice->SetFrequencyRatio(pitch));
}

struct voice_callback : IXAudio2VoiceCallback
{
	HANDLE playEndEvent;

	voice_callback() : playEndEvent(CreateEventEx(0, 0, 0, EVENT_MODIFY_STATE | SYNCHRONIZE)) {}
	~voice_callback() { CloseHandle(playEndEvent); }

	virtual void __stdcall OnVoiceProcessingPassStart(uint32 bytesRequired) override {}
	virtual void __stdcall OnVoiceProcessingPassEnd() override {}
	virtual void __stdcall OnStreamEnd() override
	{
		SetEvent(playEndEvent); // This for some reason only gets called for the non-streaming clips (where only one buffer is submitted).
	}
	virtual void __stdcall OnVoiceError(void* bufferContext, HRESULT error) override {}
	virtual void __stdcall OnBufferStart(void* bufferContext) override {}
	virtual void __stdcall OnBufferEnd(void* bufferContext) override
	{
		if (bufferContext)
		{
			SetEvent((HANDLE)bufferContext);
		}
	}
	virtual void __stdcall OnLoopEnd(void* bufferContext) override {}
};

static voice_callback voiceCallback;
static std::unordered_multimap<uint32, IXAudio2SourceVoice*> freeVoices;
static std::mutex mutex;

// Only call from mutexed code.
static void addToList(uint32 slot, uint32& first)
{
	if (first != -1)
	{
		assert(voiceSlots[first].prev == -1);
		voiceSlots[first].prev = slot;
	}
	voiceSlots[slot].next = first;
	voiceSlots[slot].prev = -1;
	first = slot;
}

static void removeFromList(uint32 slot, uint32& first)
{
	if (voiceSlots[slot].prev != -1)
	{
		voiceSlots[voiceSlots[slot].prev].next = voiceSlots[slot].next;
	}
	else
	{
		assert(first == slot);
		first = voiceSlots[slot].next;
	}

	if (voiceSlots[slot].next != -1)
	{
		voiceSlots[voiceSlots[slot].next].prev = voiceSlots[slot].prev;
	}
}

static std::pair<uint32, uint32> getFreeVoiceSlot()
{
	mutex.lock();

	uint32 result = firstFreeVoiceSlot;
	uint32 generation = 0;
	if (firstFreeVoiceSlot != -1)
	{
		removeFromList(result, firstFreeVoiceSlot);
		addToList(result, firstRunningVoiceSlot);

		generation = voiceSlots[result].generation;
	}

	mutex.unlock();

	if (result == -1)
	{
		LOG_WARNING("No free voice slot found");
	}

	return { result, generation };
}

static void retireVoiceSlotNoMutex(uint32 slot)
{
	++voiceSlots[slot].generation;

	removeFromList(slot, firstRunningVoiceSlot);
	addToList(slot, firstFreeVoiceSlot);
}

static void retireVoiceSlot(uint32 slot)
{
	const std::lock_guard<std::mutex> lock(mutex);
	retireVoiceSlotNoMutex(slot);
}

static std::pair<IXAudio2SourceVoice*, uint32> getFreeVoice(const WAVEFORMATEX& wfx)
{
	IXAudio2SourceVoice* result;
	uint32 key = getFormatKey(wfx);

	mutex.lock();
	auto it = freeVoices.find(key);
	if (it == freeVoices.end())
	{
		checkResult(xaudio->CreateSourceVoice(&result, (WAVEFORMATEX*)&wfx, 0, XAUDIO2_DEFAULT_FREQ_RATIO, &voiceCallback));
	}
	else
	{
		result = it->second;
		result->SetSourceSampleRate(wfx.nSamplesPerSec);
		freeVoices.erase(it);
	}
	mutex.unlock();

	assert(result);
	checkResult(result->Start());

	return { result, key };
}

struct file_stream_context
{
	fs::path path;
	uint32 slotIndex;
	float volume;
	float pitch;
	bool loop;
};

struct procedural_stream_context
{
	audio_generator* generator;
	uint32 slotIndex;
	bool loop;
};

static DWORD WINAPI streamFileAudio(void* parameter)
{
	file_stream_context* context = (file_stream_context*)parameter;

	audio_file file = openAudioFile(context->path);
	if (file.valid())
	{
		auto [sourceVoice, voiceKey] = getFreeVoice((const WAVEFORMATEX&)file.wfx);
		voice_slot& slot = voiceSlots[context->slotIndex];
		slot.voiceKey = voiceKey;
		slot.voice = sourceVoice;
		slot.deleteOnPlaybackEnd = 0;




		constexpr uint32 MAX_BUFFER_COUNT = 3;
		constexpr uint32 STREAMING_BUFFER_SIZE = (1024 * 8 * 6);

		BYTE buffers[MAX_BUFFER_COUNT][STREAMING_BUFFER_SIZE];

		uint32 currentBufferIndex = 0;


		HANDLE bufferEndEvent = CreateEvent(NULL, FALSE, FALSE, NULL);

		setVolume(sourceVoice, context->volume);
		setPitch(sourceVoice, context->pitch);

		bool error = false;

		while (!error)
		{
			uint32 currentPosition = 0;

			if (SetFilePointer(file.fileHandle, file.dataChunkPosition, NULL, FILE_BEGIN) == INVALID_SET_FILE_POINTER)
			{
				LOG_ERROR("Could not set file pointer");
				break;
			}

			while (currentPosition < file.dataChunkSize)
			{
				DWORD size = STREAMING_BUFFER_SIZE;
				if (ReadFile(file.fileHandle, buffers[currentBufferIndex], size, &size, 0) == 0)
				{
					error = true;
					break;
				}

				currentPosition += size;

				XAUDIO2_BUFFER buffer = { 0 };
				buffer.AudioBytes = size;
				buffer.pAudioData = buffers[currentBufferIndex];
				buffer.pContext = bufferEndEvent;

				checkResult(sourceVoice->SubmitSourceBuffer(&buffer));

				XAUDIO2_VOICE_STATE state;
				sourceVoice->GetState(&state, XAUDIO2_VOICE_NOSAMPLESPLAYED);
				while (state.BuffersQueued > MAX_BUFFER_COUNT - 1)
				{
					WaitForSingleObject(bufferEndEvent, INFINITE);
					sourceVoice->GetState(&state);
				}
				currentBufferIndex++;
				currentBufferIndex %= MAX_BUFFER_COUNT;
			}

			if (!context->loop)
			{
				break;
			}
		}

		XAUDIO2_VOICE_STATE state;
		while (sourceVoice->GetState(&state, XAUDIO2_VOICE_NOSAMPLESPLAYED), state.BuffersQueued > 0)
		{
			WaitForSingleObject(bufferEndEvent, INFINITE);
		}

		CloseHandle(bufferEndEvent);

		SetEvent(voiceCallback.playEndEvent); // Call play end ourselves. See comment in voice_callback.
		CloseHandle(file.fileHandle);
	}
	else
	{
		retireVoiceSlot(context->slotIndex);
	}

	delete context;

	return 0;
}

static DWORD WINAPI streamProceduralAudio(void* parameter)
{
	procedural_stream_context* context = (procedural_stream_context*)parameter;

	constexpr uint32 MAX_BUFFER_COUNT = 3;
	constexpr uint32 STREAMING_BUFFER_SIZE = (1024 * 8 * 6);

	float buffers[MAX_BUFFER_COUNT][STREAMING_BUFFER_SIZE];

	uint32 currentBufferIndex = 0;


	HANDLE bufferEndEvent = CreateEvent(NULL, FALSE, FALSE, NULL);

	voice_slot& slot = voiceSlots[context->slotIndex];
	IXAudio2SourceVoice* sourceVoice = slot.voice;

	bool error = false;

	while (!error)
	{
		uint32 currentPosition = 0;

		while (true)
		{
			uint32 size = STREAMING_BUFFER_SIZE;
			size = context->generator->getNextSamples(buffers[currentBufferIndex], size);

			if (size == 0)
			{
				break;
			}

			currentPosition += size;

			XAUDIO2_BUFFER buffer = { 0 };
			buffer.AudioBytes = size * sizeof(float);
			buffer.pAudioData = (BYTE*)buffers[currentBufferIndex];
			buffer.pContext = bufferEndEvent;

			checkResult(sourceVoice->SubmitSourceBuffer(&buffer));

			XAUDIO2_VOICE_STATE state;
			sourceVoice->GetState(&state, XAUDIO2_VOICE_NOSAMPLESPLAYED);
			while (state.BuffersQueued > MAX_BUFFER_COUNT - 1)
			{
				WaitForSingleObject(bufferEndEvent, INFINITE);
				sourceVoice->GetState(&state);
			}
			currentBufferIndex++;
			currentBufferIndex %= MAX_BUFFER_COUNT;
		}

		if (!context->loop)
		{
			break;
		}
	}

	XAUDIO2_VOICE_STATE state;
	while (sourceVoice->GetState(&state, XAUDIO2_VOICE_NOSAMPLESPLAYED), state.BuffersQueued > 0)
	{
		WaitForSingleObject(bufferEndEvent, INFINITE);
	}

	CloseHandle(bufferEndEvent);

	SetEvent(voiceCallback.playEndEvent); // Call play end ourselves. See comment in voice_callback.

	delete context;

	return 0;
}

static DWORD WINAPI retireStoppedVoices(void* parameter)
{
	while (true)
	{
		if (WaitForSingleObject(voiceCallback.playEndEvent, INFINITE) == WAIT_OBJECT_0)
		{
			mutex.lock();

			for (uint32 i = firstRunningVoiceSlot; i != -1;)
			{
				voice_slot& slot = voiceSlots[i];

				XAUDIO2_VOICE_STATE state;
				slot.voice->GetState(&state, XAUDIO2_VOICE_NOSAMPLESPLAYED);

				uint32 next = slot.next;

				if (!state.BuffersQueued)
				{
					slot.voice->Stop();
					if (slot.deleteOnPlaybackEnd)
					{
						delete slot.deleteOnPlaybackEnd;
						slot.deleteOnPlaybackEnd = 0;
					}

					if (slot.voiceKey != 0)
					{
						freeVoices.insert({ slot.voiceKey, slot.voice });
					}
					else
					{
						slot.voice->DestroyVoice();
					}

					retireVoiceSlotNoMutex(i);
				}

				i = next;
			}

			mutex.unlock();
		}
	}

	return 0;
}

bool initializeAudio()
{
	oldMasterAudioVolume = masterAudioVolume;

	uint32 flags = 0;
#ifdef _DEBUG
	flags |= XAUDIO2_DEBUG_ENGINE;
#endif
	checkResult(XAudio2Create(xaudio.GetAddressOf(), flags));
	checkResult(xaudio->CreateMasteringVoice(&masterVoice));

	masterVoice->SetVolume(masterAudioVolume);

	DWORD channelMask;
	masterVoice->GetChannelMask(&channelMask);

	X3DAUDIO_HANDLE X3DInstance;
	X3DAudioInitialize(channelMask, X3DAUDIO_SPEED_OF_SOUND, X3DInstance);

	for (uint32 i = 0; i < NUM_VOICE_SLOTS; ++i)
	{
		voiceSlots[i].prev = (i == 0) ? -1 : (i - 1);
		voiceSlots[i].next = (i == NUM_VOICE_SLOTS - 1) ? -1 : (i + 1);
	}

	CreateThread(0, 0, retireStoppedVoices, 0, 0, 0);

	return true;
}

void shutdownAudio()
{
	xaudio->StopEngine();
}

audio_handle playAudioFromFile(const fs::path& path, float volume, float pitch, bool stream, bool loop)
{
	auto [slotIndex, slotGeneration] = getFreeVoiceSlot();
	if (slotIndex == -1)
	{
		return {};
	}

	if (!stream)
	{
		audio_file file = openAudioFile(path);
		if (!file.valid())
		{
			retireVoiceSlot(slotIndex);
			return {};
		}

		auto [sourceVoice, voiceKey] = getFreeVoice((const WAVEFORMATEX&)file.wfx);

		voice_slot& slot = voiceSlots[slotIndex];
		slot.voiceKey = voiceKey;
		slot.voice = sourceVoice;
		slot.deleteOnPlaybackEnd = 0;


		// Fill out the audio data buffer with the contents of the fourccDATA chunk.
		BYTE* dataBuffer = new BYTE[file.dataChunkSize];
		if (!readChunkData(file, dataBuffer, file.dataChunkSize, file.dataChunkPosition))
		{
			CloseHandle(file.fileHandle);
			delete[] dataBuffer;
			retireVoiceSlot(slotIndex);
			return {};
		}

		XAUDIO2_BUFFER buffer = { 0 };
		buffer.AudioBytes = file.dataChunkSize;
		buffer.pAudioData = dataBuffer;
		if (loop)
		{
			buffer.LoopCount = XAUDIO2_LOOP_INFINITE;
		}
		else
		{
			buffer.Flags = XAUDIO2_END_OF_STREAM;
		}


		setVolume(sourceVoice, volume);
		setPitch(sourceVoice, pitch);

		checkResult(sourceVoice->SubmitSourceBuffer(&buffer));

		CloseHandle(file.fileHandle);
	}
	else
	{
		voiceSlots[slotIndex].voice = 0;

		HANDLE threadHandle = CreateThread(0, 0, streamFileAudio, new file_stream_context{ path, slotIndex, volume, pitch, loop }, 0, 0);
		if (!threadHandle)
		{
			retireVoiceSlot(slotIndex);
			return {};
		}

		CloseHandle(threadHandle);
	}

	return { slotIndex, slotGeneration };
}

audio_handle playAudioFromData(float* data, uint32 totalNumSamples, uint32 numChannels, uint32 sampleHz, float volume, float pitch, bool loop, bool deleteBufferAfterPlayback)
{
	auto [slotIndex, slotGeneration] = getFreeVoiceSlot();
	if (slotIndex == -1)
	{
		return {};
	}

	WAVEFORMATEX wfx = {};
	wfx.wFormatTag = WAVE_FORMAT_IEEE_FLOAT;
	wfx.nChannels = numChannels;
	wfx.nSamplesPerSec = sampleHz;
	wfx.wBitsPerSample = numChannels * (uint32)sizeof(float) * 8;
	wfx.nBlockAlign = numChannels * (uint32)sizeof(float);
	wfx.nAvgBytesPerSec = sampleHz * numChannels * (uint32)sizeof(float);
	wfx.cbSize = 0; // Set to zero for PCM or IEEE float.


	auto [sourceVoice, voiceKey] = getFreeVoice(wfx);

	voice_slot& slot = voiceSlots[slotIndex];
	slot.voiceKey = voiceKey;
	slot.voice = sourceVoice;
	slot.deleteOnPlaybackEnd = deleteBufferAfterPlayback ? data : 0;


	XAUDIO2_BUFFER buffer = { 0 };
	buffer.AudioBytes = totalNumSamples * (uint32)sizeof(float);
	buffer.pAudioData = (BYTE*)data;
	if (loop)
	{
		buffer.LoopCount = XAUDIO2_LOOP_INFINITE;
	}
	else
	{
		buffer.Flags = XAUDIO2_END_OF_STREAM;
	}

	setVolume(sourceVoice, volume);
	setPitch(sourceVoice, pitch);

	checkResult(sourceVoice->SubmitSourceBuffer(&buffer));

	return { slotIndex, slotGeneration };
}

audio_handle playAudioFromGenerator(audio_generator* generator, float volume, float pitch, bool stream, bool loop)
{
	auto [slotIndex, slotGeneration] = getFreeVoiceSlot();
	if (slotIndex == -1)
	{
		return {};
	}

	WAVEFORMATEX wfx = {};
	wfx.wFormatTag = WAVE_FORMAT_IEEE_FLOAT;
	wfx.nChannels = generator->numChannels;
	wfx.nSamplesPerSec = generator->sampleHz;
	wfx.wBitsPerSample = generator->numChannels * (uint32)sizeof(float) * 8;
	wfx.nBlockAlign = generator->numChannels * (uint32)sizeof(float);
	wfx.nAvgBytesPerSec = generator->sampleHz * generator->numChannels * (uint32)sizeof(float);
	wfx.cbSize = 0; // Set to zero for PCM or IEEE float.


	auto [sourceVoice, voiceKey] = getFreeVoice(wfx);

	voice_slot& slot = voiceSlots[slotIndex];
	slot.voiceKey = voiceKey;
	slot.voice = sourceVoice;
	slot.deleteOnPlaybackEnd = 0;


	if (!stream)
	{
		float* dataBuffer = new float[generator->totalNumSamples];

		generator->getNextSamples(dataBuffer, generator->totalNumSamples);

		XAUDIO2_BUFFER buffer = { 0 };
		buffer.AudioBytes = generator->totalNumSamples * (uint32)sizeof(float);
		buffer.pAudioData = (BYTE*)dataBuffer;
		if (loop)
		{
			buffer.LoopCount = XAUDIO2_LOOP_INFINITE;
		}
		else
		{
			buffer.Flags = XAUDIO2_END_OF_STREAM;
		}

		setVolume(sourceVoice, volume);
		setPitch(sourceVoice, pitch);

		checkResult(sourceVoice->SubmitSourceBuffer(&buffer));
	}
	else
	{
		setVolume(sourceVoice, volume);
		setPitch(sourceVoice, pitch);

		HANDLE threadHandle = CreateThread(0, 0, streamProceduralAudio, new procedural_stream_context{ generator, slotIndex, loop }, 0, 0);
		if (!threadHandle)
		{
			retireVoiceSlot(slotIndex);
			return {};
		}

		CloseHandle(threadHandle);
	}

	return { slotIndex, slotGeneration };
}

void updateAudio()
{
	if (masterAudioVolume != oldMasterAudioVolume)
	{
		masterVoice->SetVolume(masterAudioVolume);
		oldMasterAudioVolume = masterAudioVolume;
	}
}

bool audio_handle::valid()
{
	return slotIndex != -1 && voiceSlots[slotIndex].generation == generation;
}
