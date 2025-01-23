#pragma once

#include "bresources/bresource_types.h"
#include "math/geometry.h"

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
    bresource_texture* cubemap;

    bgeometry geometry;
    u32 group_id;
    
    u32 draw_id;
} skybox;

BAPI b8 skybox_create(skybox_config config, skybox* out_skybox);

BAPI b8 skybox_initialize(skybox* sb);

BAPI b8 skybox_load(skybox* sb);

BAPI b8 skybox_unload(skybox* sb);

BAPI void skybox_destroy(skybox* sb);
