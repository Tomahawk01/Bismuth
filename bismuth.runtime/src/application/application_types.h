#pragma once

#include "application_config.h"
#include "core/engine.h"
#include "platform/platform.h"

struct render_packet;
struct frame_data;
struct application_config;

// Represents various stages of application lifecycle
typedef enum application_stage
{
    APPLICATION_STAGE_UNINITIALIZED,
    APPLICATION_STAGE_BOOTING,
    APPLICATION_STAGE_BOOT_COMPLETE,
    APPLICATION_STAGE_INITIALIZING,
    APPLICATION_STAGE_INITIALIZED,
    APPLICATION_STAGE_RUNNING,
    APPLICATION_STAGE_SHUTTING_DOWN
} application_stage;

struct application_state;

typedef struct application
{
    // The application configuration
    application_config app_config;

    // Function pointer to application's boot sequence
    b8 (*boot)(struct application* app_inst);

    // Function pointer to application's initialize function
    b8 (*initialize)(struct application* app_inst);

    // Function pointer to application's update function
    b8 (*update)(struct application* app_inst, struct frame_data* p_frame_data);

    // Function pointer to application's prepare_frame function
    b8 (*prepare_frame)(struct application* app_inst, struct frame_data* p_frame_data);

    // Function pointer to application's render_frame function
    b8 (*render_frame)(struct application* app_inst, struct frame_data* p_frame_data);

    // Function pointer to handle resizes, if applicable
    void (*on_window_resize)(struct application* app_inst, const struct bwindow* window);

    // Shuts down the application, prompting release of resources
    void (*shutdown)(struct application* app_inst);

    void (*lib_on_unload)(struct application* game_inst);
    void (*lib_on_load)(struct application* game_inst);

    application_stage stage;

    // application-specific state. Created and managed by the application
    struct application_state* state;

    // Application state
    void* engine_state;

    dynamic_library game_library;
} application;
