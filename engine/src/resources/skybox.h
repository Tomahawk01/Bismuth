#pragma once

#include "math/math_types.h"
#include "resources/resource_types.h"
#include "renderer/renderer_types.inl"

typedef struct skybox
{
    texture_map cubemap;
    geometry* g;
    u32 instance_id;
    u64 render_frame_number;
} skybox;

BAPI b8 skybox_create(const char* cubemap_name, skybox* out_skybox);
BAPI void skybox_destroy(skybox* sb);
