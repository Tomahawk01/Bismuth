#pragma once

#include "defines.h"

struct linear_allocator;

typedef struct frame_data
{
    // Time in seconds since last frame
    f32 delta_time;

    // Total amount of time in seconds application has been running
    f64 total_time;

    // Number of meshes drawn in the last frame
    u32 drawn_mesh_count;

    // Pointer to engine's frame allocator
    struct linear_allocator* frame_allocator;

    // Application level frame specific data. Optional, up to the app to know how to use this if needed
    void* application_frame_data;
} frame_data;
