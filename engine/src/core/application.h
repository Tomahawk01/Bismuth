#pragma once

#include "defines.h"
#include "systems/font_system.h"
#include "renderer/renderer_types.inl"

struct game;

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
} application_config;

BAPI b8 application_create(struct game* game_inst);

BAPI b8 application_run();

void application_get_framebuffer_size(u32* width, u32* height);
