#pragma once

#include "core/engine.h"
#include "memory/linear_allocator.h"
#include "platform/platform.h"

struct render_packet;

typedef struct app_frame_data
{
    // darray of world geometries to be rendered this frame
    geometry_render_data* world_geometries;
} app_frame_data;

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

typedef struct application
{
    // The application configuration
    application_config app_config;

    // Function pointer to application's boot sequence
    b8 (*boot)(struct application* app_inst);

    // Function pointer to application's initialize function
    b8 (*initialize)(struct application* app_inst);

    // Function pointer to application's update function
    b8 (*update)(struct application* app_inst, f32 delta_time);

    // Function pointer to application's render function
    b8 (*render)(struct application* app_inst, struct render_packet* packet, f32 delta_time);

    // Function pointer to handle resizes, if applicable
    void (*on_resize)(struct application* app_inst, u32 width, u32 height);

    // Shuts down the application, prompting release of resources
    void (*shutdown)(struct application* app_inst);

    void (*lib_on_unload)(struct application* game_inst);
    void (*lib_on_load)(struct application* game_inst);

    application_stage stage;

    // application-specific state. Created and managed by the application
    void* state;

    // Application state
    void* engine_state;

    // Allocator used for allocations needing to be made every frame. Contents are wiped at the beginning of the frame
    linear_allocator frame_allocator;

    // Data which is built up, used and discarded every frame
    app_frame_data frame_data;

    dynamic_library renderer_library;
    renderer_plugin render_plugin;

    dynamic_library game_library;
} application;
