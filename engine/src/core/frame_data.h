#pragma once

#include "defines.h"

struct linear_allocator;

typedef struct frame_allocator_int
{
    void* (*allocate)(u64 size);
    void (*free)(void* block, u64 size);
    void (*free_all)(void);
} frame_allocator_int;

typedef struct frame_data
{
    // Time in seconds since last frame
    f32 delta_time;

    // Total amount of time in seconds application has been running
    f64 total_time;

    // Number of meshes drawn in the last frame
    u32 drawn_mesh_count;

    // Number of meshes drawn in the shadow pass in the last frame
    u32 drawn_shadow_mesh_count;

    // An allocator used for per-frame allocations
    frame_allocator_int allocator;

    // Current renderer frame number (used for data synchronization)
    u64 renderer_frame_number;

    u8 draw_index;

    // Current render target index for renderers that use multiple render targets at once
    u64 render_target_index;

    // Application level frame specific data. Optional, up to the app to know how to use this if needed
    void* application_frame_data;
} frame_data;
