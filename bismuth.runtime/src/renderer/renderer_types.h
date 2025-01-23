#pragma once

#include <containers/freelist.h>
#include <core_render_types.h>
#include <defines.h>
#include <bresources/bresource_types.h>
#include <math/math_types.h>
#include <strings/bname.h>

#include "resources/resource_types.h"
#include "systems/material_system.h"

struct shader_uniform;
struct frame_data;
struct terrain;
struct viewport;
struct camera;
struct material;
struct bwindow_renderer_backend_state;

typedef struct renderbuffer_data
{
    /** @brief The element count */
    u32 element_count;
    /** @brief The size of each element */
    u32 element_size;
    /** @brief The element data */
    void* elements;
    /** @brief The offset from the beginning of the buffer */
    u64 buffer_offset;
} renderbuffer_data;

BDEPRECATED("geometry_render_data should be phased out")
typedef struct geometry_render_data
{
    mat4 model;
    material_instance material;

    // The per-draw id to be used when applying this data. Used for draws that don't use materials
    u32 draw_id;

    u64 unique_id;
    b8 winding_inverted;
    vec4 diffuse_color;

    u32 vertex_count;
    u32 vertex_element_size;
    u64 vertex_buffer_offset;

    u32 index_count;
    u32 index_element_size;
    u64 index_buffer_offset;

    u32 ibl_probe_index;
} geometry_render_data;

typedef enum renderer_debug_view_mode
{
    RENDERER_VIEW_MODE_DEFAULT = 0,
    RENDERER_VIEW_MODE_LIGHTING = 1,
    RENDERER_VIEW_MODE_NORMALS = 2,
    RENDERER_VIEW_MODE_CASCADES = 3,
    RENDERER_VIEW_MODE_WIREFRAME = 4
} renderer_debug_view_mode;

typedef enum renderer_projection_matrix_type
{
    RENDERER_PROJECTION_MATRIX_TYPE_PERSPECTIVE = 0x0,
    RENDERER_PROJECTION_MATRIX_TYPE_ORTHOGRAPHIC = 0x1,
    RENDERER_PROJECTION_MATRIX_TYPE_ORTHOGRAPHIC_CENTERED = 0x2
} renderer_projection_matrix_type;

typedef enum renderer_stencil_op
{
    RENDERER_STENCIL_OP_KEEP = 0,
    RENDERER_STENCIL_OP_ZERO = 1,
    RENDERER_STENCIL_OP_REPLACE = 2,
    RENDERER_STENCIL_OP_INCREMENT_AND_CLAMP = 3,
    RENDERER_STENCIL_OP_DECREMENT_AND_CLAMP = 4,
    RENDERER_STENCIL_OP_INVERT = 5,
    RENDERER_STENCIL_OP_INCREMENT_AND_WRAP = 6,
    RENDERER_STENCIL_OP_DECREMENT_AND_WRAP = 7
} renderer_stencil_op;

typedef enum renderer_compare_op
{
    RENDERER_COMPARE_OP_NEVER = 0,
    RENDERER_COMPARE_OP_LESS = 1,
    RENDERER_COMPARE_OP_EQUAL = 2,
    RENDERER_COMPARE_OP_LESS_OR_EQUAL = 3,
    RENDERER_COMPARE_OP_GREATER = 4,
    RENDERER_COMPARE_OP_NOT_EQUAL = 5,
    RENDERER_COMPARE_OP_GREATER_OR_EQUAL = 6,
    RENDERER_COMPARE_OP_ALWAYS = 7
} renderer_compare_op;

typedef enum renderer_attachment_type_flag_bits
{
    RENDERER_ATTACHMENT_TYPE_FLAG_COLOR_BIT = 0x1,
    RENDERER_ATTACHMENT_TYPE_FLAG_DEPTH_BIT = 0x2,
    RENDERER_ATTACHMENT_TYPE_FLAG_STENCIL_BIT = 0x4
} renderer_attachment_type_flag_bits;

typedef u32 renderer_attachment_type_flags;

typedef enum renderer_attachment_load_operation
{
    RENDERER_ATTACHMENT_LOAD_OPERATION_DONT_CARE = 0x0,
    RENDERER_ATTACHMENT_LOAD_OPERATION_LOAD = 0x1
} renderer_attachment_load_operation;

typedef enum renderer_attachment_store_operation
{
    RENDERER_ATTACHMENT_STORE_OPERATION_DONT_CARE = 0x0,
    RENDERER_ATTACHMENT_STORE_OPERATION_STORE = 0x1
} renderer_attachment_store_operation;

typedef enum renderer_attachment_use
{
    RENDERER_ATTACHMENT_USE_DONT_CARE,
    RENDERER_ATTACHMENT_USE_COLOR_ATTACHMENT,
    RENDERER_ATTACHMENT_USE_COLOR_PRESENT,
    RENDERER_ATTACHMENT_USE_COLOR_SHADER_READ,
    RENDERER_ATTACHMENT_USE_COLOR_SHADER_WRITE,
    RENDERER_ATTACHMENT_USE_DEPTH_STENCIL_ATTACHMENT,
    RENDERER_ATTACHMENT_USE_DEPTH_STENCIL_SHADER_READ,
    RENDERER_ATTACHMENT_USE_DEPTH_STENCIL_SHADER_WRITE
} renderer_attachment_use;

typedef enum renderbuffer_type
{
    RENDERBUFFER_TYPE_UNKNOWN,
    RENDERBUFFER_TYPE_VERTEX,
    RENDERBUFFER_TYPE_INDEX,
    RENDERBUFFER_TYPE_UNIFORM,
    RENDERBUFFER_TYPE_STAGING,
    RENDERBUFFER_TYPE_READ,
    RENDERBUFFER_TYPE_STORAGE
} renderbuffer_type;

typedef enum renderbuffer_track_type
{
    RENDERBUFFER_TRACK_TYPE_NONE = 0,
    RENDERBUFFER_TRACK_TYPE_FREELIST = 1,
    RENDERBUFFER_TRACK_TYPE_LINEAR = 2
} renderbuffer_track_type;

typedef struct renderbuffer
{
    char* name;
    renderbuffer_type type;
    u64 total_size;
    renderbuffer_track_type track_type;
    u64 freelist_memory_requirement;
    freelist buffer_freelist;
    void* freelist_block;
    void* internal_data;
    u64 offset;
} renderbuffer;

typedef enum renderer_config_flag_bits
{
    RENDERER_CONFIG_FLAG_VSYNC_ENABLED_BIT = 0x1,
    // Configures the renderer backend in a way that conserves power where possible. (Usefull for mobile)
    RENDERER_CONFIG_FLAG_POWER_SAVING_BIT = 0x2,
    // Enables advanced validation in the renderer backend, if supported
    RENDERER_CONFIG_FLAG_ENABLE_VALIDATION = 0x4
} renderer_config_flag_bits;

typedef u32 renderer_config_flags;

typedef struct renderer_backend_config
{
    const char* application_name;
    renderer_config_flags flags;

    u16 max_shader_count;
    b8 use_triple_buffering;
} renderer_backend_config;

typedef enum renderer_winding
{
    RENDERER_WINDING_COUNTER_CLOCKWISE = 0,
    RENDERER_WINDING_CLOCKWISE = 1
} renderer_winding;

typedef struct bwindow_renderer_state
{
    // Pointer back to main window
    struct bwindow* window;
    // The viewport information for the given window
    struct viewport* active_viewport;

    // This is technically the swapchain images, which should be wrapped into a single texture
    bresource_texture* colorbuffer;
    // This is technically the per-frame depth image, which should be wrapped into a single texture
    bresource_texture* depthbuffer;

    // The internal state of the window containing renderer backend data
    struct bwindow_renderer_backend_state* backend_state;
} bwindow_renderer_state;

typedef struct renderer_backend_interface
{
    // A pointer to the frontend state in case the backend needs to communicate with it
    struct renderer_system_state* frontend_state;

    u64 internal_context_size;
    void* internal_context;

    b8 (*initialize)(struct renderer_backend_interface* backend, const renderer_backend_config* config);
    void (*shutdown)(struct renderer_backend_interface* backend);

    void (*begin_debug_label)(struct renderer_backend_interface* backend, const char* label_text, vec3 color);
    void (*end_debug_label)(struct renderer_backend_interface* backend);

    b8 (*window_create)(struct renderer_backend_interface* backend, struct bwindow* window);

    void (*window_destroy)(struct renderer_backend_interface* backend, struct bwindow* window);
    void (*window_resized)(struct renderer_backend_interface* backend, const struct bwindow* window);

    b8 (*frame_prepare)(struct renderer_backend_interface* backend, struct frame_data* p_frame_data);
    b8 (*frame_prepare_window_surface)(struct renderer_backend_interface* backend, struct bwindow* window, struct frame_data* p_frame_data);
    b8 (*frame_commands_begin)(struct renderer_backend_interface* backend, struct frame_data* p_frame_data);
    b8 (*frame_commands_end)(struct renderer_backend_interface* backend, struct frame_data* p_frame_data);
    b8 (*frame_submit)(struct renderer_backend_interface* backend, struct frame_data* p_frame_data);
    b8 (*frame_present)(struct renderer_backend_interface* backend, struct bwindow* window, struct frame_data* p_frame_data);

    void (*viewport_set)(struct renderer_backend_interface* backend, vec4 rect);
    void (*viewport_reset)(struct renderer_backend_interface* backend);

    void (*scissor_set)(struct renderer_backend_interface* backend, vec4 rect);
    void (*scissor_reset)(struct renderer_backend_interface* backend);

    void (*winding_set)(struct renderer_backend_interface* backend, renderer_winding winding);

    void (*set_stencil_test_enabled)(struct renderer_backend_interface* backend, b8 enabled);
    void (*set_depth_test_enabled)(struct renderer_backend_interface* backend, b8 enabled);
    void (*set_depth_write_enabled)(struct renderer_backend_interface* backend, b8 enabled);

    void (*set_stencil_reference)(struct renderer_backend_interface* backend, u32 reference);
    void (*set_stencil_op)(struct renderer_backend_interface* backend, renderer_stencil_op fail_op, renderer_stencil_op pass_op, renderer_stencil_op depth_fail_op, renderer_compare_op compare_op);

    void (*begin_rendering)(struct renderer_backend_interface* backend, struct frame_data* p_frame_data, rect_2d render_area, u32 color_target_count, bhandle* color_targets, bhandle depth_stencil_target, u32 depth_stencil_layer);
    void (*end_rendering)(struct renderer_backend_interface* backend, struct frame_data* p_frame_data);

    void (*set_stencil_compare_mask)(struct renderer_backend_interface* backend, u32 compare_mask);
    void (*set_stencil_write_mask)(struct renderer_backend_interface* backend, u32 write_mask);

    void (*clear_color_set)(struct renderer_backend_interface* backend, vec4 clear_color);
    void (*clear_depth_set)(struct renderer_backend_interface* backend, f32 depth);
    void (*clear_stencil_set)(struct renderer_backend_interface* backend, u32 stencil);
    void (*clear_color)(struct renderer_backend_interface* backend, bhandle renderer_texture_handle);
    void (*clear_depth_stencil)(struct renderer_backend_interface* backend, bhandle renderer_texture_handle);
    void (*color_texture_prepare_for_present)(struct renderer_backend_interface* backend, bhandle renderer_texture_handle);
    void (*texture_prepare_for_sampling)(struct renderer_backend_interface* backend, bhandle renderer_texture_handle, texture_flag_bits flags);

    b8 (*texture_resources_acquire)(struct renderer_backend_interface* backend, const char* name, bresource_texture_type type, u32 width, u32 height, u8 channel_count, u8 mip_levels, u16 array_size, bresource_texture_flag_bits flags, bhandle* out_renderer_texture_handle);
    void (*texture_resources_release)(struct renderer_backend_interface* backend, bhandle* renderer_texture_handle);

    b8 (*texture_resize)(struct renderer_backend_interface* backend, bhandle renderer_texture_handle, u32 new_width, u32 new_height);
    b8 (*texture_write_data)(struct renderer_backend_interface* backend, bhandle renderer_texture_handle, u32 offset, u32 size, const u8* pixels, b8 include_in_frame_workload);
    b8 (*texture_read_data)(struct renderer_backend_interface* backend, bhandle renderer_texture_handle, u32 offset, u32 size, u8** out_pixels);
    b8 (*texture_read_pixel)(struct renderer_backend_interface* backend, bhandle renderer_texture_handle, u32 x, u32 y, u8** out_rgba);

    b8 (*shader_create)(struct renderer_backend_interface* backend, bhandle shader, const bresource_shader* shader_resource);
    void (*shader_destroy)(struct renderer_backend_interface* backend, bhandle shader);
    b8 (*shader_reload)(struct renderer_backend_interface* backend, bhandle s, u32 shader_stage_count, shader_stage_config* shader_stages);

    b8 (*shader_use)(struct renderer_backend_interface* backend, bhandle shader);

    b8 (*shader_supports_wireframe)(const struct renderer_backend_interface* backend, bhandle shader);

    b8 (*shader_flag_get)(const struct renderer_backend_interface* backend, bhandle shader, shader_flags flag);
    void (*shader_flag_set)(struct renderer_backend_interface* backend, bhandle shader, shader_flags flag, b8 enabled);

    b8 (*shader_bind_per_frame)(struct renderer_backend_interface* backend, bhandle shader);
    b8 (*shader_bind_per_group)(struct renderer_backend_interface* backend, bhandle shader, u32 group_id);
    b8 (*shader_bind_per_draw)(struct renderer_backend_interface* backend, bhandle shader, u32 draw_id);

    b8 (*shader_apply_per_frame)(struct renderer_backend_interface* backend, bhandle shader, u16 renderer_frame_number);
    b8 (*shader_apply_per_group)(struct renderer_backend_interface* backend, bhandle shader, u16 generation);
    b8 (*shader_apply_per_draw)(struct renderer_backend_interface* backend, bhandle shader, u16 generation);

    b8 (*shader_per_group_resources_acquire)(struct renderer_backend_interface* backend, bhandle shader, u32* out_instance_id);
    b8 (*shader_per_group_resources_release)(struct renderer_backend_interface* backend, bhandle shader, u32 instance_id);

    b8 (*shader_per_draw_resources_acquire)(struct renderer_backend_interface* backend, bhandle shader, u32* out_local_id);
    b8 (*shader_per_draw_resources_release)(struct renderer_backend_interface* backend, bhandle shader, u32 local_id);

    b8 (*shader_uniform_set)(struct renderer_backend_interface* backend, bhandle frontend_shader, struct shader_uniform* uniform, u32 array_index, const void* value);

    bhandle (*sampler_acquire)(struct renderer_backend_interface* backend, texture_filter filter, texture_repeat repeat, f32 anisotropy);
    void (*sampler_release)(struct renderer_backend_interface* backend, bhandle* sampler);
    b8 (*sampler_refresh)(struct renderer_backend_interface* backend, bhandle* sampler, texture_filter filter, texture_repeat repeat, f32 anisotropy, u32 mip_levels);

    bname (*sampler_name_get)(struct renderer_backend_interface* backend, bhandle sampler);

    b8 (*is_multithreaded)(struct renderer_backend_interface* backend);

    b8 (*flag_enabled_get)(struct renderer_backend_interface* backend, renderer_config_flags flag);
    void (*flag_enabled_set)(struct renderer_backend_interface* backend, renderer_config_flags flag, b8 enabled);

    f32 (*max_anisotropy_get)(struct renderer_backend_interface* backend);

    b8 (*renderbuffer_internal_create)(struct renderer_backend_interface* backend, renderbuffer* buffer);
    void (*renderbuffer_internal_destroy)(struct renderer_backend_interface* backend, renderbuffer* buffer);

    b8 (*renderbuffer_bind)(struct renderer_backend_interface* backend, renderbuffer* buffer, u64 offset);
    b8 (*renderbuffer_unbind)(struct renderer_backend_interface* backend, renderbuffer* buffer);

    void* (*renderbuffer_map_memory)(struct renderer_backend_interface* backend, renderbuffer* buffer, u64 offset, u64 size);
    void (*renderbuffer_unmap_memory)(struct renderer_backend_interface* backend, renderbuffer* buffer, u64 offset, u64 size);

    b8 (*renderbuffer_flush)(struct renderer_backend_interface* backend, renderbuffer* buffer, u64 offset, u64 size);
    b8 (*renderbuffer_read)(struct renderer_backend_interface* backend, renderbuffer* buffer, u64 offset, u64 size, void** out_memory);
    b8 (*renderbuffer_resize)(struct renderer_backend_interface* backend, renderbuffer* buffer, u64 new_total_size);

    b8 (*renderbuffer_load_range)(struct renderer_backend_interface* backend, renderbuffer* buffer, u64 offset, u64 size, const void* data, b8 include_in_frame_workload);
    b8 (*renderbuffer_copy_range)(struct renderer_backend_interface* backend, renderbuffer* source, u64 source_offset, renderbuffer* dest, u64 dest_offset, u64 size, b8 include_in_frame_workload);

    b8 (*renderbuffer_draw)(struct renderer_backend_interface* backend, renderbuffer* buffer, u64 offset, u32 element_count, b8 bind_only);

    void (*wait_for_idle)(struct renderer_backend_interface* backend);
} renderer_backend_interface;
