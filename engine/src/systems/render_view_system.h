#pragma once

#include "defines.h"
#include "math/math_types.h"
#include "renderer/renderer_types.h"

struct frame_data;

typedef struct render_view_system_config
{
    u16 max_view_count;
} render_view_system_config;

b8 render_view_system_initialize(u64* memory_requirement, void* state, void* config);
void render_view_system_shutdown(void* state);

BAPI b8 render_view_system_register(render_view* view);

BAPI void render_view_system_on_window_resize(u32 width, u32 height);

BAPI render_view* render_view_system_get(const char* name);

BAPI b8 render_view_system_packet_build(const render_view* view, struct linear_allocator* frame_allocator, void* data, struct render_view_packet* out_packet);

BAPI b8 render_view_system_on_render(const render_view* view, const render_view_packet* packet, u64 frame_number, u64 render_target_index, const struct frame_data* p_frame_data);

BAPI void render_view_system_render_targets_regenerate(render_view* view);
