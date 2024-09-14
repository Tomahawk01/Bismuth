#pragma once

#include "math/math_types.h"
#include "resources/resource_types.h"
#include "renderer/renderer_types.h"
#include "systems/geometry_system.h"

typedef struct skybox_config
{
    const char* cubemap_name;
} skybox_config;

typedef enum skybox_state
{
    SKYBOX_STATE_UNDEFINED,
    SKYBOX_STATE_CREATED,
    SKYBOX_STATE_INITIALIZED,
    SKYBOX_STATE_LOADING,
    SKYBOX_STATE_LOADED
} skybox_state;

typedef struct skybox
{
    skybox_state state;

    bname cubemap_name;
    bresource_texture_map cubemap;

    geometry_config g_config;
    geometry* g;
    u32 instance_id;
    u64 render_frame_number;
    u8 draw_index;
} skybox;

BAPI b8 skybox_create(skybox_config config, skybox* out_skybox);

BAPI b8 skybox_initialize(skybox* sb);

BAPI b8 skybox_load(skybox* sb);

BAPI b8 skybox_unload(skybox* sb);

BAPI void skybox_destroy(skybox* sb);
