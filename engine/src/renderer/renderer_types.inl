#pragma once

#include "defines.h"
#include "math/math_types.h"
#include "resources/resource_types.h"

#define BUILTIN_SHADER_NAME_MATERIAL "Shader.Builtin.Material"
#define BUILTIN_SHADER_NAME_UI "Shader.Builtin.UI"

struct shader;
struct shader_uniform;

typedef enum renderer_backend_type
{
    RENDERER_BACKEND_TYPE_VULKAN,
    RENDERER_BACKEND_TYPE_OPENGL,
    RENDERER_BACKEND_TYPE_DIRECTX
} renderer_backend_type;

typedef struct geometry_render_data
{
    mat4 model;
    geometry* geometry;
} geometry_render_data;

typedef enum builtin_renderpass
{
    BUILTIN_RENDERPASS_WORLD = 0x01,
    BUILTIN_RENDERPASS_UI = 0x02,
} builtin_renderpass;

typedef enum renderer_debug_view_mode
{
    RENDERER_VIEW_MODE_DEFAULT = 0,
    RENDERER_VIEW_MODE_LIGHTING = 1,
    RENDERER_VIEW_MODE_NORMALS = 2
} renderer_debug_view_mode;

typedef struct renderer_backend
{
    u64 frame_number;

    b8 (*initialize)(struct renderer_backend* backend, const char* application_name);

    void (*shutdown)(struct renderer_backend* backend);

    void (*resized)(struct renderer_backend* backend, u16 width, u16 height);

    b8 (*begin_frame)(struct renderer_backend* backend, f32 delta_time);
    b8 (*end_frame)(struct renderer_backend* backend, f32 delta_time);

    b8 (*begin_renderpass)(struct renderer_backend* backend, u8 renderpass_id);
    b8 (*end_renderpass)(struct renderer_backend* backend, u8 renderpass_id);

    void (*draw_geometry)(geometry_render_data data);

    void (*texture_create)(const u8* pixels, struct texture* texture);
    void (*texture_destroy)(struct texture* texture);

    void (*texture_create_writeable)(texture* t);
    
    void (*texture_resize)(texture* t, u32 new_width, u32 new_height);

    void (*texture_write_data)(texture* t, u32 offset, u32 size, const u8* pixels);

    b8 (*create_geometry)(geometry* geometry, u32 vertex_size, u32 vertex_count, const void* vertices, u32 index_size, u32 index_count, const void* indices);
    void (*destroy_geometry)(geometry* geometry);

    b8 (*shader_create)(struct shader* shader, u8 renderpass_id, u8 stage_count, const char** stage_filenames, shader_stage* stages);
    void (*shader_destroy)(struct shader* shader);

    b8 (*shader_initialize)(struct shader* shader);

    b8 (*shader_use)(struct shader* shader);

    b8 (*shader_bind_globals)(struct shader* s);

    b8 (*shader_bind_instance)(struct shader* s, u32 instance_id);

    b8 (*shader_apply_globals)(struct shader* s);

    b8 (*shader_apply_instance)(struct shader* s, b8 needs_update);

    b8 (*shader_acquire_instance_resources)(struct shader* s, texture_map** maps, u32* out_instance_id);

    b8 (*shader_release_instance_resources)(struct shader* s, u32 instance_id);

    b8 (*shader_set_uniform)(struct shader* frontend_shader, struct shader_uniform* uniform, const void* value);

    b8 (*texture_map_acquire_resources)(struct texture_map* map);

    void (*texture_map_release_resources)(struct texture_map* map);
} renderer_backend;

typedef struct render_packet
{
    f32 delta_time;

    u32 geometry_count;
    geometry_render_data* geometries;

    u32 ui_geometry_count;
    geometry_render_data* ui_geometries;
} render_packet;
