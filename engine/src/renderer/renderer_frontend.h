#pragma once

#include "renderer_types.inl"

struct shader;
struct shader_uniform;

b8 renderer_system_initialize(u64* memory_requirement, void* state, const char* application_name);
void renderer_system_shutdown(void* state);

void renderer_on_resized(u16 width, u16 height);

b8 renderer_draw_frame(render_packet* packet);

// NOTE: this should not be exposed outside the engine (until camera system will be implemented)
BAPI void renderer_set_view(mat4 view, vec3 view_position);

void renderer_create_texture(const u8* pixels, struct texture* texture);

void renderer_destroy_texture(struct texture* texture);

b8 renderer_create_geometry(geometry* geometry, u32 vertex_size, u32 vertex_count, const void* vertices, u32 index_size, u32 index_count, const void* indices);
void renderer_destroy_geometry(geometry* geometry);

b8 renderer_renderpass_id(const char* name, u8* out_renderpass_id);

b8 renderer_shader_create(struct shader* s, u8 renderpass_id, u8 stage_count, const char** stage_filenames, shader_stage* stages);
void renderer_shader_destroy(struct shader* s);

b8 renderer_shader_initialize(struct shader* s);

b8 renderer_shader_use(struct shader* s);

b8 renderer_shader_bind_globals(struct shader* s);

b8 renderer_shader_bind_instance(struct shader* s, u32 instance_id);

b8 renderer_shader_apply_globals(struct shader* s);

b8 renderer_shader_apply_instance(struct shader* s, b8 needs_update);

b8 renderer_shader_acquire_instance_resources(struct shader* s, u32* out_instance_id);

b8 renderer_shader_release_instance_resources(struct shader* s, u32 instance_id);

b8 renderer_set_uniform(struct shader* s, struct shader_uniform* uniform, const void* value);
