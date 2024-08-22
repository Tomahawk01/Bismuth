#pragma once

#include "defines.h"
#include "memory/bmemory.h"

struct linear_allocator;

typedef struct frame_data
{
    // Number of meshes drawn in the last frame
    u32 drawn_mesh_count;

    // Number of meshes drawn in the shadow pass in the last frame
    u32 drawn_shadow_mesh_count;

    // An allocator used for per-frame allocations
    frame_allocator_int allocator;

    // Application level frame specific data. Optional, up to the app to know how to use this if needed
    void* application_frame_data;
} frame_data;
