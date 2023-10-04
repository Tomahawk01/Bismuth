#pragma once

#include "defines.h"
#include "math/math_types.h"
#include "renderer/renderer_types.inl"

typedef struct render_view_system_config
{
    u16 max_view_count;
} render_view_system_config;

b8 render_view_system_initialize(u64* memory_requirement, void* state, render_view_system_config config);
void render_view_system_shutdown(void* state);

b8 render_view_system_create(const render_view_config* config);

void render_view_system_on_window_resize(u32 width, u32 height);

render_view* render_view_system_get(const char* name);

b8 render_view_system_build_packet(const render_view* view, void* data, struct render_view_packet* out_packet);

b8 render_view_system_on_render(const render_view* view, const render_view_packet* packet, u64 frame_number, u64 render_target_index);

void render_view_system_regenerate_render_targets(render_view* view);
