#pragma once

#include "geometry/mesh.h"
#include "shadow_map_cache.h"

struct particle_draw_info
{
	ref<dx_buffer> particleBuffer;
	ref<dx_buffer> aliveList;
	ref<dx_buffer> commandBuffer;
	uint32 aliveListOffset;
	uint32 commandBufferOffset;
	uint32 rootParameterOffset;
};


template <typename render_data_t>
struct default_render_command
{
	mat4 transform;
	dx_vertex_buffer_group_view vertexBuffer;
	dx_index_buffer_view indexBuffer;
	submesh_info submesh;

	render_data_t data;
};

template <typename render_data_t>
struct particle_render_command
{
	dx_vertex_buffer_group_view vertexBuffer;
	dx_index_buffer_view indexBuffer;
	particle_draw_info drawInfo;

	render_data_t data;
};

struct static_depth_only_render_command
{
	mat4 transform;
	dx_vertex_buffer_view vertexBuffer;
	dx_index_buffer_view indexBuffer;
	submesh_info submesh;
	uint32 objectID;
};

struct dynamic_depth_only_render_command
{
	mat4 transform;
	mat4 prevFrameTransform;
	dx_vertex_buffer_view vertexBuffer;
	dx_index_buffer_view indexBuffer;
	submesh_info submesh;
	uint32 objectID;
};

struct animated_depth_only_render_command
{
	mat4 transform;
	mat4 prevFrameTransform;
	dx_vertex_buffer_view vertexBuffer;
	D3D12_GPU_VIRTUAL_ADDRESS prevFrameVertexBufferAddress;
	dx_index_buffer_view indexBuffer;
	submesh_info submesh;
	uint32 objectID;
};

struct outline_render_command
{
	mat4 transform;
	dx_vertex_buffer_view vertexBuffer;
	dx_index_buffer_view indexBuffer;
	submesh_info submesh;
};

struct shadow_render_command
{
	mat4 transform;
	dx_vertex_buffer_view vertexBuffer;
	dx_index_buffer_view indexBuffer;
	submesh_info submesh;
};




