#pragma once

#include "light_source.h"
#include "dx/dx_buffer.h"
#include "dx/dx_texture.h"

struct dx_command_list;

struct common_render_data
{
	ref<dx_texture> sky;
	ref<dx_texture> irradiance;
	ref<dx_texture> prefilteredRadiance;

	ref<dx_texture> aoTexture;
	ref<dx_texture> sssTexture;
	ref<dx_texture> ssrTexture;

	ref<dx_texture> lightProbeIrradiance;
	ref<dx_texture> lightProbeDepth;

	ref<dx_texture> tiledCullingGrid;
	ref<dx_buffer> tiledObjectsIndexList;
	ref<dx_buffer> tiledSpotLightIndexList;
	ref<dx_buffer> pointLightBuffer;
	ref<dx_buffer> spotLightBuffer;
	ref<dx_buffer> decalBuffer;
	ref<dx_buffer> pointLightShadowInfoBuffer;
	ref<dx_buffer> spotLightShadowInfoBuffer;

	ref<dx_texture> decalTextureAtlas;

	ref<dx_texture> shadowMap;

	ref<dx_texture> volumetricsTexture;

	// These two are only set, if the material is rendered after the opaque pass.
	ref<dx_texture> opaqueDepth;
	ref<dx_texture> worldNormalsAndRoughness;

	dx_dynamic_constant_buffer cameraCBV;
	dx_dynamic_constant_buffer lightingCBV;

	float globalIlluminationIntensity;
	float skyIntensity;
	bool proceduralSky;

	vec2 cameraJitter;
	vec2 prevFrameCameraJitter;
};



#define PIPELINE_SETUP_DECL					static void setup(dx_command_list* cl, const common_render_data& common)
#define PIPELINE_SETUP_IMPL(name)			void name::setup(dx_command_list* cl, const common_render_data& common)

#define DEPTH_ONLY_RENDER_DECL				static void render(dx_command_list* cl, const mat4& viewProj, const mat4& prevFrameViewProj, const depth_only_render_command<render_data_t>& rc)
#define DEPTH_ONLY_RENDER_IMPL(name)		void name::render(dx_command_list* cl, const mat4& viewProj, const mat4& prevFrameViewProj, const depth_only_render_command<render_data_t>& rc)

#define PIPELINE_RENDER_DECL				static void render(dx_command_list* cl, const mat4& viewProj, const render_command<render_data_t>& rc)
#define PIPELINE_RENDER_IMPL(name)			void name::render(dx_command_list* cl, const mat4& viewProj, const render_command<render_data_t>& rc)

#define PARTICLE_PIPELINE_RENDER_DECL		static void render(dx_command_list* cl, const mat4& viewProj, const particle_render_command<render_data_t>& rc)
#define PARTICLE_PIPELINE_RENDER_IMPL(name)	void name::render(dx_command_list* cl, const mat4& viewProj, const particle_render_command<render_data_t>& rc)

