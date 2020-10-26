#include "pch.h"
#include "dx_render_primitives.h"
#include "colliders.h"
#include "geometry.h"

#include <assimp/Importer.hpp>
#include <assimp/Exporter.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include <filesystem>
#include <iostream>

namespace fs = std::filesystem;

#define CACHE_FORMAT "fbx" // We don't export as assbin, because the exporter kills the mesh names, which we use to identify LODs.

const aiScene* loadAssimpSceneFile(const char* filepathRaw)
{
	fs::path filepath = filepathRaw;
	fs::path extension = filepath.extension();

	fs::path cachedFilename = filepath;
	cachedFilename.replace_extension(".cache." CACHE_FORMAT);

	fs::path cacheFilepath = L"bin_cache" / cachedFilename;

	Assimp::Importer importer;
	const aiScene* scene = 0;

	{
		// Look for cached.

		WIN32_FILE_ATTRIBUTE_DATA cachedData;
		if (GetFileAttributesExW(cacheFilepath.c_str(), GetFileExInfoStandard, &cachedData))
		{
			FILETIME cachedFiletime = cachedData.ftLastWriteTime;

			WIN32_FILE_ATTRIBUTE_DATA originalData;
			assert(GetFileAttributesExW(filepath.c_str(), GetFileExInfoStandard, &originalData));
			FILETIME originalFiletime = originalData.ftLastWriteTime;

			if (CompareFileTime(&cachedFiletime, &originalFiletime) >= 0)
			{
				// Cached file is newer than original, so load this.
				scene = importer.ReadFile(cacheFilepath.string(), 0);
			}
		}
	}

	if (!scene)
	{
		importer.SetPropertyFloat(AI_CONFIG_PP_GSN_MAX_SMOOTHING_ANGLE, 80.f);
		importer.SetPropertyInteger(AI_CONFIG_PP_SBP_REMOVE, aiPrimitiveType_POINT | aiPrimitiveType_LINE);
		importer.SetPropertyInteger(AI_CONFIG_PP_SLM_VERTEX_LIMIT, UINT16_MAX);

		//importer.SetPropertyInteger(AI_CONFIG_PP_SLM_TRIANGLE_LIMIT, 2000);

		uint32 importFlags = aiProcessPreset_TargetRealtime_MaxQuality | aiProcess_FlipUVs;
		uint32 exportFlags = 0;

		scene = importer.ReadFile(filepath.string(), importFlags);

		fs::create_directories(cacheFilepath.parent_path());
		Assimp::Exporter exporter;
#if 0
		uint32 exporterCount = exporter.GetExportFormatCount();
		for (uint32 i = 0; i < exporterCount; ++i)
		{
			const aiExportFormatDesc* desc = exporter.GetExportFormatDescription(i);
			int a = 0;
		}
#endif

		exporter.Export(scene, CACHE_FORMAT, cacheFilepath.string(), exportFlags);
	}

	if (scene)
	{
		scene = importer.GetOrphanedScene();
	}

	return scene;
}

void freeAssimpScene(const aiScene* scene)
{
	if (scene)
	{
		scene->~aiScene(); // TODO: I am not sure, if this deletes everything. Probably not.
	}
}


// Suzanne 0
// Suzanne 1
// Suzanne 2

// Cube 0
// Cube 1

// Mesh [5]
// Components
//	- LOD Mesh [3]
//		- Suzanne 0 + Cube 0
//		- Suzanne 1 + Cube 1
//		- Suzanne 2




// Sponza
// 24 Parts






struct single_mesh
{
	submesh_info submesh;
	aabb_collider boundingBox;
	std::string name;
};

struct lod_mesh
{
	uint32 firstMesh;
	uint32 numMeshes;
};

struct composite_mesh
{
	std::vector<single_mesh> singleMeshes;
	std::vector<lod_mesh> lods;
};


void analyzeAssimpScene(const aiScene* scene)
{
	struct mesh_info
	{
		int32 lod;
		submesh_info submesh;
		aabb_collider aabb;
		std::string name;
	};

	cpu_mesh cpuMesh(mesh_creation_flags_with_positions | mesh_creation_flags_with_uvs | mesh_creation_flags_with_normals);

	std::vector<mesh_info> infos(scene->mNumMeshes);

	int32 maxLOD = 0;
	uint32 numMeshesWithoutLOD = 0;

	for (uint32 i = 0; i < scene->mNumMeshes; ++i)
	{
		const aiMesh* mesh = scene->mMeshes[i];
		std::string name = mesh->mName.C_Str();

		int32 lod = -1;

		if (name.length() >= 4)
		{
			uint32 j;
			for (j = (uint32)name.length() - 1; j >= 3; --j)
			{
				if (name[j] < '0' || name[j] > '9')
				{
					break;
				}
			}
			if (j < (uint32)name.length() - 1)
			{
				if (   (name[j - 2] == 'L' || name[j - 2] == 'l')
					&& (name[j - 1] == 'O' || name[j - 1] == 'o')
					&& (name[j]     == 'D' || name[j]     == 'd'))
				{
					lod = atoi(name.c_str() + j + 1);
				}
			}
		}

		maxLOD = max(maxLOD, lod);

		if (lod == -1)
		{
			++numMeshesWithoutLOD;
		}

		infos[i].lod = lod;
		infos[i].submesh = cpuMesh.pushAssimpMesh(mesh, 1.f, &infos[i].aabb);
		infos[i].name = name;
	}

	std::sort(infos.begin(), infos.end(), [](const mesh_info& a, const mesh_info& b)
	{
		return a.lod < b.lod;
	});



	composite_mesh result;
	result.lods.resize(maxLOD + 1);

	uint32 currentInfoOffset = numMeshesWithoutLOD;
	int32 lodReduce = 0;

	for (int32 i = 0; i <= maxLOD; ++i)
	{
		lod_mesh& lod = result.lods[i];
		lod.firstMesh = (uint32)result.singleMeshes.size();
		lod.numMeshes = 0;

		if (currentInfoOffset < infos.size())
		{
			while (infos[currentInfoOffset].lod - lodReduce != i)
			{
				++lodReduce;
			}
		}

		for (uint32 j = 0; j < numMeshesWithoutLOD; ++j, ++lod.numMeshes)
		{
			result.singleMeshes.push_back({ infos[j].submesh, infos[j].aabb, infos[j].name });
		}

		if (currentInfoOffset < infos.size())
		{
			while (currentInfoOffset < infos.size() && infos[currentInfoOffset].lod - lodReduce == i)
			{
				result.singleMeshes.push_back({ infos[currentInfoOffset].submesh, infos[currentInfoOffset].aabb, infos[currentInfoOffset].name });
				++currentInfoOffset;
				++lod.numMeshes;
			}
		}
	}

	result.lods.resize(result.lods.size() - lodReduce);

	if (lodReduce > 0)
	{
		std::cout << "There is a gap in the LOD declarations." << std::endl;
	}

	std::cout << "There are " << result.lods.size() << " LODs in this file" << std::endl;
	
	uint32 l = 0;
	for (lod_mesh& lod : result.lods)
	{
		std::cout << "LOD " << l++ << ": " << std::endl;
		for (uint32 i = lod.firstMesh; i < lod.firstMesh + lod.numMeshes; ++i)
		{
			std::cout << "   " << result.singleMeshes[i].name << std::endl;
		}

	}

}