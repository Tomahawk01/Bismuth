#pragma once

#include "math/math_types.h"
#include "resources/resource_types.h"
#include "renderer/renderer_types.inl"
#include "systems/geometry_system.h"

typedef struct skybox_config
{
    const char* cubemap_name;
    geometry_config g_config;
} skybox_config;

typedef struct skybox
{
    skybox_config config;
    texture_map cubemap;
    geometry* g;
    u32 instance_id;
    u64 render_frame_number;
} skybox;

BAPI b8 skybox_create(skybox_config config, skybox* out_skybox);

BAPI b8 skybox_initialize(skybox* sb);

BAPI b8 skybox_load(skybox* sb);

BAPI b8 skybox_unload(skybox* sb);

BAPI void skybox_destroy(skybox* sb);
