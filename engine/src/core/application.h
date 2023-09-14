#pragma once

#include "defines.h"

struct game;

// Application configuration
typedef struct application_config
{
    i16 start_pos_x;
    i16 start_pos_y;
    i16 start_width;
    i16 start_height;
    char* name;
} application_config;

BAPI b8 application_create(struct game* game_inst);

BAPI b8 application_run();

void application_get_framebuffer_size(u32* width, u32* height);
