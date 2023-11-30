#pragma once

#include "defines.h"
#include "audio/audio_types.h"
#include "systems/font_system.h"
#include "renderer/renderer_types.h"

struct application;
struct frame_data;
struct systems_manager_state;

// Application configuration
typedef struct application_config
{
    i16 start_pos_x;
    i16 start_pos_y;
    i16 start_width;
    i16 start_height;
    char* name;

    font_system_config font_config;
    render_view* views;

    renderer_plugin renderer_plugin;
    audio_plugin audio_plugin;

    u64 frame_allocator_size;

    u64 app_frame_data_size;
} application_config;

BAPI b8 engine_create(struct application* game_inst);
BAPI b8 engine_run(struct application* game_inst);

void engine_on_event_system_initialized(void);

BAPI const struct frame_data* engine_frame_data_get(struct application* game_inst);

BAPI struct systems_manager_state* engine_systems_manager_state_get(struct application* game_inst);
