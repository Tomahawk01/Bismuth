#pragma once

#include "core/application.h"

typedef struct game
{
    // The application configuration
    application_config app_config;

    // Function pointer to game's initialize function
    b8 (*initialize)(struct game* game_inst);

    // Function pointer to game's update function
    b8 (*update)(struct game* game_inst, f32 delta_time);

    // Function pointer to game's render function
    b8 (*render)(struct game* game_inst, f32 delta_time);

    // Function pointer to handle resizes, if applicable
    void (*on_resize)(struct game* game_inst, u32 width, u32 height);

    // Required size for the game state
    u64 state_memory_requirement;

    // Game-specific game state. Created and managed by the game
    void* state;

    // Application state
    void* application_state;
} game;
