#pragma once

#include "renderer/renderer_backend.h"
#include "resources/resource_types.h"

struct shader;
struct shader_uniform;

b8 vulkan_renderer_backend_initialize(renderer_backend* backend, const char* application_name);
void vulkan_renderer_backend_shutdown(renderer_backend* backend);

void vulkan_renderer_backend_on_resized(renderer_backend* backend, u16 width, u16 height);

b8 vulkan_renderer_backend_begin_frame(renderer_backend* backend, f32 delta_time);
b8 vulkan_renderer_backend_end_frame(renderer_backend* backend, f32 delta_time);

b8 vulkan_renderer_begin_renderpass(struct renderer_backend* backend, u8 renderpass_id);
b8 vulkan_renderer_end_renderpass(struct renderer_backend* backend, u8 renderpass_id);

void vulkan_backend_draw_geometry(geometry_render_data data);

void vulkan_renderer_create_texture(const u8* pixels, texture* texture);
void vulkan_renderer_destroy_texture(texture* texture);

b8 vulkan_renderer_create_geometry(geometry* geometry, u32 vertex_size, u32 vertex_count, const void* vertices, u32 index_size, u32 index_count, const void* indices);
void vulkan_renderer_destroy_geometry(geometry* geometry);

b8 vulkan_renderer_shader_create(struct shader* shader, u8 renderpass_id, u8 stage_count, const char** stage_filenames, shader_stage* stages);
void vulkan_renderer_shader_destroy(struct shader* shader);

b8 vulkan_renderer_shader_initialize(struct shader* shader);
b8 vulkan_renderer_shader_use(struct shader* shader);
b8 vulkan_renderer_shader_bind_globals(struct shader* s);
b8 vulkan_renderer_shader_bind_instance(struct shader* s, u32 instance_id);
b8 vulkan_renderer_shader_apply_globals(struct shader* s);
b8 vulkan_renderer_shader_apply_instance(struct shader* s, b8 needs_update);
b8 vulkan_renderer_shader_acquire_instance_resources(struct shader* s, u32* out_instance_id);
b8 vulkan_renderer_shader_release_instance_resources(struct shader* s, u32 instance_id);
b8 vulkan_renderer_set_uniform(struct shader* frontend_shader, struct shader_uniform* uniform, const void* value);
