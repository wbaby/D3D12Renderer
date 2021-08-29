#pragma once

#include "light_source.h"
#include "dx/dx_buffer.h"
#include "dx/dx_texture.h"

struct dx_command_list;

struct common_material_info
{
	ref<dx_texture> sky;
	ref<dx_texture> irradiance;
	ref<dx_texture> environment;
	ref<dx_texture> brdf;

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
	ref<dx_texture> worldNormals;

	dx_dynamic_constant_buffer cameraCBV;
	dx_dynamic_constant_buffer sunCBV;

	float environmentIntensity;
	float skyIntensity;
};

