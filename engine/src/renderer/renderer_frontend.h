#pragma once

#include "renderer_types.inl"

struct shader;
struct shader_uniform;

b8 renderer_system_initialize(u64* memory_requirement, void* state, const char* application_name);
void renderer_system_shutdown(void* state);

void renderer_on_resized(u16 width, u16 height);

b8 renderer_draw_frame(render_packet* packet);

void renderer_viewport_set(vec4 rect);
void renderer_viewport_reset();

void renderer_scissor_set(vec4 rect);
void renderer_scissor_reset();

void renderer_texture_create(const u8* pixels, struct texture* texture);
void renderer_texture_destroy(struct texture* texture);

void renderer_texture_create_writeable(texture* t);

void renderer_texture_resize(texture* t, u32 new_width, u32 new_height);

void renderer_texture_write_data(texture* t, u32 offset, u32 size, const u8* pixels);

void renderer_texture_read_data(texture* t, u32 offset, u32 size, void** out_memory);
void renderer_texture_read_pixel(texture* t, u32 x, u32 y, u8** out_rgba);

b8 renderer_create_geometry(geometry* geometry, u32 vertex_size, u32 vertex_count, const void* vertices, u32 index_size, u32 index_count, const void* indices);
void renderer_destroy_geometry(geometry* geometry);

void renderer_draw_geometry(geometry_render_data* data);

b8 renderer_renderpass_begin(renderpass* pass, render_target* target);
b8 renderer_renderpass_end(renderpass* pass);

b8 renderer_shader_create(struct shader* s, const shader_config* config, renderpass* pass, u8 stage_count, const char** stage_filenames, shader_stage* stages);
void renderer_shader_destroy(struct shader* s);

b8 renderer_shader_initialize(struct shader* s);

b8 renderer_shader_use(struct shader* s);

b8 renderer_shader_bind_globals(struct shader* s);
b8 renderer_shader_bind_instance(struct shader* s, u32 instance_id);

b8 renderer_shader_apply_globals(struct shader* s);
b8 renderer_shader_apply_instance(struct shader* s, b8 needs_update);

b8 renderer_shader_acquire_instance_resources(struct shader* s, texture_map** maps, u32* out_instance_id);
b8 renderer_shader_release_instance_resources(struct shader* s, u32 instance_id);

b8 renderer_set_uniform(struct shader* s, struct shader_uniform* uniform, const void* value);

b8 renderer_texture_map_acquire_resources(struct texture_map* map);
void renderer_texture_map_release_resources(struct texture_map* map);

void renderer_render_target_create(u8 attachment_count, render_target_attachment* attachments, renderpass* pass, u32 width, u32 height, render_target* out_target);
void renderer_render_target_destroy(render_target* target, b8 free_internal_memory);

texture* renderer_window_attachment_get(u8 index);
texture* renderer_depth_attachment_get(u8 index);

BAPI u8 renderer_window_attachment_index_get();
BAPI u8 renderer_window_attachment_count_get();

b8 renderer_renderpass_create(const renderpass_config* config, renderpass* out_renderpass);
void renderer_renderpass_destroy(renderpass* pass);

b8 renderer_is_multithreaded();

BAPI b8 renderer_flag_enabled(renderer_config_flags flag);
BAPI void renderer_flag_set_enabled(renderer_config_flags flag, b8 enabled);

b8 renderer_renderbuffer_create(renderbuffer_type type, u64 total_size, b8 use_freelist, renderbuffer* out_buffer);
void renderer_renderbuffer_destroy(renderbuffer* buffer);

b8 renderer_renderbuffer_bind(renderbuffer* buffer, u64 offset);
b8 renderer_renderbuffer_unbind(renderbuffer* buffer);

void* renderer_renderbuffer_map_memory(renderbuffer* buffer, u64 offset, u64 size);
void renderer_renderbuffer_unmap_memory(renderbuffer* buffer, u64 offset, u64 size);

b8 renderer_renderbuffer_flush(renderbuffer* buffer, u64 offset, u64 size);

b8 renderer_renderbuffer_read(renderbuffer* buffer, u64 offset, u64 size, void** out_memory);

b8 renderer_renderbuffer_resize(renderbuffer* buffer, u64 new_total_size);

b8 renderer_renderbuffer_allocate(renderbuffer* buffer, u64 size, u64* out_offset);
b8 renderer_renderbuffer_free(renderbuffer* buffer, u64 size, u64 offset);

b8 renderer_renderbuffer_load_range(renderbuffer* buffer, u64 offset, u64 size, const void* data);

b8 renderer_renderbuffer_copy_range(renderbuffer* source, u64 source_offset, renderbuffer* dest, u64 dest_offset, u64 size);

b8 renderer_renderbuffer_draw(renderbuffer* buffer, u64 offset, u32 element_count, b8 bind_only);
