#pragma once

#include "core/input.h"
#include "core/camera.h"
#include "core/camera_controller.h"
#include "geometry/mesh.h"
#include "core/math.h"
#include "scene/scene.h"
#include "rendering/main_renderer.h"
#include "particles/fire_particle_system.h"
#include "particles/smoke_particle_system.h"
#include "particles/boid_particle_system.h"
#include "particles/debris_particle_system.h"
#include "rendering/raytracing.h"
#include "editor/editor.h"
#include "learning/learned_locomotion.h"




struct application
{
	void loadCustomShaders();
	void initialize(main_renderer* renderer, editor_panels* editorPanels);
	void update(const user_input& input, float dt);

	void handleFileDrop(const fs::path& filename);

private:

	void resetRenderPasses();
	void submitRendererParams(uint32 numSpotLightShadowPasses, uint32 numPointLightShadowPasses);


	raytracing_tlas raytracingTLAS;



	ref<dx_buffer> pointLightBuffer[NUM_BUFFERED_FRAMES];
	ref<dx_buffer> spotLightBuffer[NUM_BUFFERED_FRAMES];
	ref<dx_buffer> decalBuffer[NUM_BUFFERED_FRAMES];

	ref<dx_buffer> spotLightShadowInfoBuffer[NUM_BUFFERED_FRAMES];
	ref<dx_buffer> pointLightShadowInfoBuffer[NUM_BUFFERED_FRAMES];

	std::vector<pbr_decal_cb> decals;

	ref<dx_texture> decalTexture;

	fire_particle_system fireParticleSystem;
	smoke_particle_system smokeParticleSystem;
	boid_particle_system boidParticleSystem;
	debris_particle_system debrisParticleSystem;

	main_renderer* renderer;

	editor_scene scene;
	scene_editor editor;

	memory_arena stackArena;

	learned_locomotion learnedLocomotion;


	uint32 numSpotShadowRenderPasses;
	uint32 numPointShadowRenderPasses;

	opaque_render_pass opaqueRenderPass;
	transparent_render_pass transparentRenderPass;
	sun_shadow_render_pass sunShadowRenderPass;
	spot_shadow_render_pass spotShadowRenderPasses[16];
	point_shadow_render_pass pointShadowRenderPasses[16];
	ldr_render_pass ldrRenderPass;
	compute_pass computePass;


};
