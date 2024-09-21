#pragma once

#include <defines.h>
#include <identifiers/bhandle.h>
#include <bresources/bresource_types.h>
#include <strings/bname.h>

#include "core/frame_data.h"
#include "renderer_types.h"
#include "resources/resource_types.h"

struct shader;
struct shader_uniform;
struct frame_data;
struct viewport;

typedef struct renderer_system_config
{
    const char* application_name;
    const char* backend_plugin_name;
    b8 vsync;
    b8 enable_validation;
    b8 power_saving;
} renderer_system_config;

struct renderer_system_state;
struct bwindow;

b8 renderer_system_deserialize_config(const char* config_str, renderer_system_config* out_config);

BAPI b8 renderer_system_initialize(u64* memory_requirement, struct renderer_system_state* state, const renderer_system_config* config);
BAPI void renderer_system_shutdown(struct renderer_system_state* state);

BAPI u64 renderer_system_frame_number_get(struct renderer_system_state* state);

BAPI b8 renderer_on_window_created(struct renderer_system_state* state, struct bwindow* window);
BAPI void renderer_on_window_destroyed(struct renderer_system_state* state, struct bwindow* window);

BAPI void renderer_on_window_resized(struct renderer_system_state* state, const struct bwindow* window);

BAPI void renderer_begin_debug_label(const char* label_text, vec3 color);
BAPI void renderer_end_debug_label(void);

BAPI b8 renderer_frame_prepare(struct renderer_system_state* state, struct frame_data* p_frame_data);
BAPI b8 renderer_frame_prepare_window_surface(struct renderer_system_state* state, struct bwindow* window, struct frame_data* p_frame_data);
BAPI b8 renderer_frame_command_list_begin(struct renderer_system_state* state, struct frame_data* p_frame_data);
BAPI b8 renderer_frame_command_list_end(struct renderer_system_state* state, struct frame_data* p_frame_data);
BAPI b8 renderer_frame_submit(struct renderer_system_state* state, struct frame_data* p_frame_data);
BAPI b8 renderer_frame_present(struct renderer_system_state* state, struct bwindow* window, struct frame_data* p_frame_data);

BAPI void renderer_viewport_set(vec4 rect);
BAPI void renderer_viewport_reset(void);

BAPI void renderer_scissor_set(vec4 rect);
BAPI void renderer_scissor_reset(void);

BAPI void renderer_winding_set(renderer_winding winding);

BAPI void renderer_set_stencil_test_enabled(b8 enabled);

BAPI void renderer_set_stencil_reference(u32 reference);

BAPI void renderer_set_depth_test_enabled(b8 enabled);
BAPI void renderer_set_depth_write_enabled(b8 enabled);

BAPI void renderer_set_stencil_op(renderer_stencil_op fail_op, renderer_stencil_op pass_op, renderer_stencil_op depth_fail_op, renderer_compare_op compare_op);

BAPI void renderer_begin_rendering(struct renderer_system_state* state, struct frame_data* p_frame_data, rect_2d render_area, u32 color_target_count, b_handle* color_targets, b_handle depth_stencil_target, u32 depth_stencil_layer);
BAPI void renderer_end_rendering(struct renderer_system_state* state, struct frame_data* p_frame_data);

BAPI void renderer_set_stencil_compare_mask(u32 compare_mask);
BAPI void renderer_set_stencil_write_mask(u32 write_mask);

BAPI b8 renderer_bresource_texture_resources_acquire(struct renderer_system_state* state, bname name, bresource_texture_type type, u32 width, u32 height, u8 channel_count, u8 mip_levels, u16 array_size, bresource_texture_flag_bits flags, b_handle* out_renderer_texture_handle);
BAPI void renderer_texture_resources_release(struct renderer_system_state* state, b_handle* handle);
BAPI struct texture_internal_data* renderer_texture_resources_get(struct renderer_system_state* state, b_handle renderer_texture_handle);

BAPI b8 renderer_texture_resize(struct renderer_system_state* state, b_handle renderer_texture_handle, u32 new_width, u32 new_height);
BAPI b8 renderer_texture_write_data(struct renderer_system_state* state, b_handle renderer_texture_handle, u32 offset, u32 size, const u8* pixels);

BAPI b8 renderer_texture_read_data(struct renderer_system_state* state, b_handle renderer_texture_handle, u32 offset, u32 size, u8** out_pixels);
BAPI b8 renderer_texture_read_pixel(struct renderer_system_state* state, b_handle renderer_texture_handle, u32 x, u32 y, u8** out_rgba);
BAPI struct texture_internal_data* renderer_texture_internal_get(struct renderer_system_state* state, b_handle renderer_texture_handle);

BAPI renderbuffer* renderer_renderbuffer_get(renderbuffer_type type);

BDEPRECATED("The renderer frontend geometry functions will be removed in a future pass. Upload directly to renderbuffers instead")
BAPI b8 renderer_geometry_create(struct geometry* geometry, u32 vertex_size, u32 vertex_count, const void* vertices, u32 index_size, u32 index_count, const void* indices);

BDEPRECATED("The renderer frontend geometry functions will be removed in a future pass. Upload directly to renderbuffers instead")
BAPI b8 renderer_geometry_upload(struct geometry* geometry);

BDEPRECATED("The renderer frontend geometry functions will be removed in a future pass. Upload directly to renderbuffers instead")
BAPI void renderer_geometry_vertex_update(struct geometry* g, u32 offset, u32 vertex_count, void* vertices, b8 include_in_frame_workload);

BDEPRECATED("The renderer frontend geometry functions will be removed in a future pass. Upload directly to renderbuffers instead")
BAPI void renderer_geometry_destroy(struct geometry* geometry);

BDEPRECATED("The renderer frontend geometry functions will be removed in a future pass. Upload directly to renderbuffers instead")
BAPI void renderer_geometry_draw(geometry_render_data* data);

BAPI void renderer_clear_color_set(struct renderer_system_state* state, vec4 color);
BAPI void renderer_clear_depth_set(struct renderer_system_state* state, f32 depth);
BAPI void renderer_clear_stencil_set(struct renderer_system_state* state, u32 stencil);

BAPI b8 renderer_clear_color(struct renderer_system_state* state, b_handle texture_handle);
BAPI b8 renderer_clear_depth_stencil(struct renderer_system_state* state, b_handle texture_handle);

BAPI void renderer_color_texture_prepare_for_present(struct renderer_system_state* state, b_handle texture_handle);
BAPI void renderer_texture_prepare_for_sampling(struct renderer_system_state* state, b_handle texture_handle, texture_flag_bits flags);

BAPI b8 renderer_shader_create(struct renderer_system_state* state, struct shader* s, const shader_config* config);
BAPI void renderer_shader_destroy(struct renderer_system_state* state, struct shader* s);

BAPI b8 renderer_shader_initialize(struct renderer_system_state* state, struct shader* s);
BAPI b8 renderer_shader_reload(struct renderer_system_state* state, struct shader* s);

BAPI b8 renderer_shader_use(struct renderer_system_state* state, struct shader* s);

BAPI b8 renderer_shader_set_wireframe(struct renderer_system_state* state, struct shader* s, b8 wireframe_enabled);

BAPI b8 renderer_shader_apply_globals(struct renderer_system_state* state, struct shader* s);

BAPI b8 renderer_shader_apply_instance(struct renderer_system_state* state, struct shader* s);
BAPI b8 renderer_shader_apply_local(struct renderer_system_state* state, struct shader* s);

BAPI b8 renderer_shader_instance_resources_acquire(struct renderer_system_state* state, struct shader* s, const shader_instance_resource_config* config, u32* out_instance_id);
BAPI b8 renderer_shader_instance_resources_release(struct renderer_system_state* state, struct shader* s, u32 instance_id);

BAPI b8 renderer_shader_uniform_set(struct renderer_system_state* state, struct shader* s, struct shader_uniform* uniform, u32 array_index, const void* value);

BAPI b8 renderer_bresource_texture_map_resources_acquire(struct renderer_system_state* state, struct bresource_texture_map* map);
BAPI void renderer_bresource_texture_map_resources_release(struct renderer_system_state* state, struct bresource_texture_map* map);

BAPI b8 renderer_is_multithreaded(void);

BAPI b8 renderer_flag_enabled_get(renderer_config_flags flag);
BAPI void renderer_flag_enabled_set(renderer_config_flags flag, b8 enabled);

BAPI b8 renderer_renderbuffer_create(const char* name, renderbuffer_type type, u64 total_size, renderbuffer_track_type track_type, renderbuffer* out_buffer);
BAPI void renderer_renderbuffer_destroy(renderbuffer* buffer);

BAPI b8 renderer_renderbuffer_bind(renderbuffer* buffer, u64 offset);
BAPI b8 renderer_renderbuffer_unbind(renderbuffer* buffer);

BAPI void* renderer_renderbuffer_map_memory(renderbuffer* buffer, u64 offset, u64 size);
BAPI void renderer_renderbuffer_unmap_memory(renderbuffer* buffer, u64 offset, u64 size);

BAPI b8 renderer_renderbuffer_flush(renderbuffer* buffer, u64 offset, u64 size);

BAPI b8 renderer_renderbuffer_read(renderbuffer* buffer, u64 offset, u64 size, void** out_memory);

BAPI b8 renderer_renderbuffer_resize(renderbuffer* buffer, u64 new_total_size);

BAPI b8 renderer_renderbuffer_allocate(renderbuffer* buffer, u64 size, u64* out_offset);
BAPI b8 renderer_renderbuffer_free(renderbuffer* buffer, u64 size, u64 offset);
BAPI b8 renderer_renderbuffer_clear(renderbuffer* buffer, b8 zero_memory);

BAPI b8 renderer_renderbuffer_load_range(renderbuffer* buffer, u64 offset, u64 size, const void* data, b8 include_in_frame_workload);
BAPI b8 renderer_renderbuffer_copy_range(renderbuffer* source, u64 source_offset, renderbuffer* dest, u64 dest_offset, u64 size, b8 include_in_frame_workload);

BAPI b8 renderer_renderbuffer_draw(renderbuffer* buffer, u64 offset, u32 element_count, b8 bind_only);

BAPI struct viewport* renderer_active_viewport_get(void);
BAPI void renderer_active_viewport_set(struct viewport* v);

BAPI void renderer_wait_for_idle(void);

BAPI b8 renderer_pcf_enabled(struct renderer_system_state* state);
