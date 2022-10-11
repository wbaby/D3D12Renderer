#include "pch.h"
#include "terrain.h"

#include "dx/dx_command_list.h"

#include "rendering/render_utils.h"
#include "rendering/render_pass.h"

#include "core/random.h"

#include "terrain_rs.hlsli"


static dx_pipeline terrainPipeline;
static ref<dx_index_buffer> terrainIndexBuffers[TERRAIN_MAX_LOD + 1];

terrain_component::terrain_component(uint32 chunksPerDim, float chunkSize, float amplitudeScale)
{
	this->chunksPerDim = chunksPerDim;
	this->chunkSize = chunkSize;
	this->amplitudeScale = amplitudeScale;
	this->chunks.resize(chunksPerDim * chunksPerDim);





	uint16* heights = new uint16[TERRAIN_LOD_0_VERTICES_PER_DIMENSION * TERRAIN_LOD_0_VERTICES_PER_DIMENSION];


	uint32 numSegmentsPerDim = TERRAIN_LOD_0_VERTICES_PER_DIMENSION - 1;
	float norm = chunkSize / (float)numSegmentsPerDim;

	for (uint32 z = 0; z < TERRAIN_LOD_0_VERTICES_PER_DIMENSION; ++z)
	{
		for (uint32 x = 0; x < TERRAIN_LOD_0_VERTICES_PER_DIMENSION; ++x)
		{
			vec2 position(x * norm, z * norm);
			float height = fbm(position * 0.05f);

			assert(height >= 0 && height <= 1.f);

			heights[z * TERRAIN_LOD_0_VERTICES_PER_DIMENSION + x] = (uint16)(height * UINT16_MAX);
		}
	}

	testheightmap = createTexture(heights, TERRAIN_LOD_0_VERTICES_PER_DIMENSION, TERRAIN_LOD_0_VERTICES_PER_DIMENSION, DXGI_FORMAT_R16_UNORM);

	delete[] heights;
}

void terrain_component::render(opaque_render_pass* renderPass, vec3 positionOffset)
{
	auto testLOD = [](int32 x, int32 z)
	{
		return ((x & 1) != (z & 1)) ? 2 : 0;
	};

	float xzOffset = -(chunkSize * chunksPerDim) * 0.5f;
	vec3 offset = vec3(xzOffset, 0.f, xzOffset) + positionOffset;

	for (int32 z = 0; z < (int32)chunksPerDim; ++z)
	{
		for (int32 x = 0; x < (int32)chunksPerDim; ++x)
		{
			auto& c = chunk(x, z);

			if (c.active)
			{
				int32 lod = testLOD(x, z);

				vec3 minCorner = vec3(x * chunkSize, 0.f, z * chunkSize) + offset;

				terrain_render_data data = { minCorner, lod, chunkSize, amplitudeScale, testLOD(x - 1, z), testLOD(x + 1, z), testLOD(x, z - 1), testLOD(x, z + 1),
					testheightmap };
				renderPass->renderStaticObject<terrain_pipeline>(mat4::identity, {}, {}, {}, data, -1, false, false);
			}
		}
	}
}

void terrain_pipeline::initialize()
{
	auto desc = CREATE_GRAPHICS_PIPELINE
		.renderTargets(opaqueLightPassFormats, arraysize(opaqueLightPassFormats), depthStencilFormat)
		.wireframe();

	terrainPipeline = createReloadablePipeline(desc, { "terrain_vs", "terrain_ps" });



	uint32 numSegmentsPerDim = TERRAIN_LOD_0_VERTICES_PER_DIMENSION - 1;
	uint32 numTrisLod0 = numSegmentsPerDim * numSegmentsPerDim * 2;

	indexed_triangle16* tris = new indexed_triangle16[numTrisLod0];

	for (uint32 lod = 0; lod < TERRAIN_MAX_LOD; ++lod)
	{
		uint32 numSegmentsPerDim = (TERRAIN_LOD_0_VERTICES_PER_DIMENSION - 1) >> lod;
		uint32 stride = numSegmentsPerDim + 1;
		uint32 numTris = numSegmentsPerDim * numSegmentsPerDim * 2;

		uint32 i = 0;
		for (uint32 z = 0; z < numSegmentsPerDim; ++z)
		{
			for (uint32 x = 0; x < numSegmentsPerDim; ++x)
			{
				tris[i++] = { (uint16)(stride * z + x), (uint16)(stride * (z + 1) + x), (uint16)(stride * z + x + 1) };
				tris[i++] = { (uint16)(stride * z + x + 1), (uint16)(stride * (z + 1) + x), (uint16)(stride * (z + 1) + x + 1) };
			}
		}

		terrainIndexBuffers[lod] = createIndexBuffer(sizeof(uint16), numTris * 3, tris);
	}

	delete[] tris;
}

PIPELINE_SETUP_IMPL(terrain_pipeline)
{
	cl->setPipelineState(*terrainPipeline.pipeline);
	cl->setGraphicsRootSignature(*terrainPipeline.rootSignature);

	cl->setPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}

PIPELINE_RENDER_IMPL(terrain_pipeline)
{
	uint32 numSegmentsPerDim = (TERRAIN_LOD_0_VERTICES_PER_DIMENSION - 1) >> rc.data.lod;
	uint32 numTris = numSegmentsPerDim * numSegmentsPerDim * 2;

	uint32 lod_negX = max(0, rc.data.lod_negX - rc.data.lod);
	uint32 lod_posX = max(0, rc.data.lod_posX - rc.data.lod);
	uint32 lod_negZ = max(0, rc.data.lod_negZ - rc.data.lod);
	uint32 lod_posZ = max(0, rc.data.lod_posZ - rc.data.lod);

	uint32 scaleDownByLODs = SCALE_DOWN_BY_LODS(lod_negX, lod_posX, lod_negZ, lod_posZ);

	cl->setGraphics32BitConstants(TERRAIN_RS_TRANSFORM, viewProj);
	cl->setGraphics32BitConstants(TERRAIN_RS_CB, terrain_cb{ rc.data.minCorner, (uint32)rc.data.lod, rc.data.amplitudeScale, rc.data.chunkSize, scaleDownByLODs });
	cl->setDescriptorHeapSRV(TERRAIN_RS_HEIGHTMAP, 0, rc.data.heightmap);

	cl->setIndexBuffer(terrainIndexBuffers[rc.data.lod]);
	cl->drawIndexed(numTris * 3, 1, 0, 0, 0);
}
