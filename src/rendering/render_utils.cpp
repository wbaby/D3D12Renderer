#include "pch.h"
#include "render_utils.h"
#include "core/random.h"
#include "dx/dx_context.h"
#include "animation/skinning.h"
#include "texture_preprocessing.h"
#include "render_resources.h"
#include "render_algorithms.h"
#include "pbr.h"
#include "particles/particles.h"
#include "particles/particle_systems.h"
#include "bitonic_sort.h"
#include "debug_visualization.h"
#include "light_probe.h"
#include "raytraced_reflections.h"
#include "shadow_map.h"
#include "terrain/terrain.h"
#include "terrain/proc_placement.h"


// Defined in render_algorithms.cpp.
void loadCommonShaders();


static vec2 haltonSequence[128];

void initializeRenderUtils()
{
	initializeTexturePreprocessing();
	initializeDepthPrepassPipelines();
	initializeSkinning();
	initializeShadowPipelines();
	initializeTerrainPipelines();
	initializeLightProbePipelines();
	initializeRTReflectionsPipelines();
	initializeProceduralPlacementPipelines();
	loadCommonShaders();

	debug_simple_pipeline::initialize();
	debug_unlit_pipeline::initialize();
	debug_unlit_line_pipeline::initialize();
	pbr_pipeline::initialize();
	particle_system::initializePipeline();
	initializeBitonicSort();
	loadAllParticleSystemPipelines();


	createAllPendingReloadablePipelines();
	render_resources::initializeGlobalResources();

	for (uint32 i = 0; i < arraysize(haltonSequence); ++i)
	{
		haltonSequence[i] = halton23(i) * 2.f - vec2(1.f);
	}
}

void endFrameCommon()
{
	checkForChangedPipelines();
}

void buildCameraConstantBuffer(const render_camera& camera, float cameraJitterStrength, camera_cb& outCB)
{
	if (cameraJitterStrength > 0.f)
	{
		vec2 jitter = haltonSequence[dxContext.frameID % arraysize(haltonSequence)] / vec2((float)camera.width, (float)camera.height) * cameraJitterStrength;
		buildCameraConstantBuffer(camera.getJitteredVersion(jitter), jitter, outCB);
	}
	else
	{
		buildCameraConstantBuffer(camera, vec2(0.f, 0.f), outCB);
	}
}

void buildCameraConstantBuffer(const render_camera& camera, vec2 jitter, camera_cb& outCB)
{
	outCB.prevFrameView = outCB.view;
	outCB.prevFrameViewProj = outCB.viewProj;
	outCB.viewProj = camera.viewProj;
	outCB.view = camera.view;
	outCB.proj = camera.proj;
	outCB.invViewProj = camera.invViewProj;
	outCB.invView = camera.invView;
	outCB.invProj = camera.invProj;
	outCB.position = vec4(camera.position, 1.f);
	outCB.forward = vec4(camera.rotation * vec3(0.f, 0.f, -1.f), 0.f);
	outCB.right = vec4(camera.rotation * vec3(1.f, 0.f, 0.f), 0.f);
	outCB.up = vec4(camera.rotation * vec3(0.f, 1.f, 0.f), 0.f);
	outCB.projectionParams = vec4(camera.nearPlane, camera.farPlane, camera.farPlane / camera.nearPlane, 1.f - camera.farPlane / camera.nearPlane);
	outCB.viewSpaceTopLeftFrustumVector = vec4(camera.restoreViewSpacePosition(vec2(0.f, 0.f), 0.f) / camera.nearPlane, 0.f);
	outCB.extentAtDistanceOne = abs(camera.restoreViewSpacePosition(vec2(1.f, 1.f), 0.f).xy / camera.nearPlane - outCB.viewSpaceTopLeftFrustumVector.xy);
	outCB.screenDims = vec2((float)camera.width, (float)camera.height);
	outCB.invScreenDims = vec2(1.f / camera.width, 1.f / camera.height);
	outCB.prevFrameJitter = outCB.jitter;
	outCB.jitter = jitter;
}
