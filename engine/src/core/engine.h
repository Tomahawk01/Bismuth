#pragma once

#include "defines.h"
#include "systems/font_system.h"
#include "renderer/renderer_types.inl"

struct application;

// Application configuration
typedef struct application_config
{
    i16 start_pos_x;
    i16 start_pos_y;
    i16 start_width;
    i16 start_height;
    char* name;
    font_system_config font_config;
    render_view_config* render_views;

    renderer_plugin renderer_plugin;
} application_config;

BAPI b8 engine_create(struct application* game_inst);

BAPI b8 engine_run(struct application* game_inst);

void engine_on_event_system_initialized();
