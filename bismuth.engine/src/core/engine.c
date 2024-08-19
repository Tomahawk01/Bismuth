#include "engine.h"

#include "application_types.h"
#include "containers/darray.h"
#include "core/event.h"
#include "core/frame_data.h"
#include "core/input.h"
#include "core/bclock.h"
#include "core/bhandle.h"
#include "core/bmemory.h"
#include "core/bstring.h"
#include "core/logger.h"
#include "core/metrics.h"
#include "core/uuid.h"
#include "memory/linear_allocator.h"
#include "platform/platform.h"
#include "renderer/renderer_frontend.h"

// Systems
#include "core/systems_manager.h"
#include "systems/timeline_system.h"

typedef struct engine_state_t
{
    application* game_inst;
    b8 is_running;
    b8 is_suspended;
    i16 width;
    i16 height;
    bclock clock;
    f64 last_time;

    b8 resizing;
    u8 frames_since_resize;

    systems_manager_state sys_manager_state;

    linear_allocator frame_allocator;

    frame_data p_frame_data;
} engine_state_t;

static engine_state_t* engine_state;

// Frame allocator functions
static void* frame_allocator_allocate(u64 size)
{
    if (!engine_state)
        return 0;

    return linear_allocator_allocate(&engine_state->frame_allocator, size);
}

static void frame_allocator_free(void* block, u64 size)
{
    // NOTE: Linear allocator doesn't free
}

static void frame_allocator_free_all(void)
{
    if (engine_state)
    {
        // Don't wipe the memory each time, to save on performance
        linear_allocator_free_all(&engine_state->frame_allocator, false);
    }
}

// Event handlers
static b8 engine_on_event(u16 code, void* sender, void* listener_inst, event_context context);
static b8 engine_on_resized(u16 code, void* sender, void* listener_inst, event_context context);

b8 engine_create(application* game_inst)
{
    if (game_inst->engine_state)
    {
        BERROR("engine_create called more than once");
        return false;
    }

    // Memory system must be the first thing to be stood up
    memory_system_configuration memory_system_config = {};
    memory_system_config.total_alloc_size = GIBIBYTES(2);
    if (!memory_system_initialize(memory_system_config))
    {
        BERROR("Failed to initialize memory system. shutting down");
        return false;
    }

    // Seed the uuid generator
    // TODO: better seed here
    uuid_seed(101);

    // Metrics
    metrics_initialize();

    // Stand up the engine state
    game_inst->engine_state = ballocate(sizeof(engine_state_t), MEMORY_TAG_ENGINE);
    engine_state = game_inst->engine_state;
    engine_state->game_inst = game_inst;
    engine_state->is_running = false;
    engine_state->is_suspended = false;
    engine_state->resizing = false;
    engine_state->frames_since_resize = 0;

    game_inst->app_config.renderer_plugin = game_inst->render_plugin;
    game_inst->app_config.audio_plugin = game_inst->audio_plugin;

    if (!systems_manager_initialize(&engine_state->sys_manager_state, &game_inst->app_config))
    {
        BFATAL("Systems manager failed to initialize. Aborting process...");
        return false;
    }

    // Perform game's boot sequence
    game_inst->stage = APPLICATION_STAGE_BOOTING;
    if (!game_inst->boot(game_inst))
    {
        BFATAL("Game boot sequence failed. Aborting application...");
        return false;
    }

    // Setup frame allocator
    linear_allocator_create(game_inst->app_config.frame_allocator_size, 0, &engine_state->frame_allocator);
    engine_state->p_frame_data.allocator.allocate = frame_allocator_allocate;
    engine_state->p_frame_data.allocator.free = frame_allocator_free;
    engine_state->p_frame_data.allocator.free_all = frame_allocator_free_all;

    // Allocate for application's frame data
    if (game_inst->app_config.app_frame_data_size > 0)
        engine_state->p_frame_data.application_frame_data = ballocate(game_inst->app_config.app_frame_data_size, MEMORY_TAG_GAME);
    else
        engine_state->p_frame_data.application_frame_data = 0;

    game_inst->stage = APPLICATION_STAGE_BOOT_COMPLETE;

    if (!systems_manager_post_boot_initialize(&engine_state->sys_manager_state, &game_inst->app_config))
    {
        BFATAL("Post-boot system manager initialization failed");
        return false;
    }

    // Initialize the game
    game_inst->stage = APPLICATION_STAGE_INITIALIZING;
    if (!engine_state->game_inst->initialize(engine_state->game_inst))
    {
        BFATAL("Game failed to initialize");
        return false;
    }
    game_inst->stage = APPLICATION_STAGE_INITIALIZED;

    return true;
}

b8 engine_run(application* game_inst)
{
    game_inst->stage = APPLICATION_STAGE_RUNNING;
    engine_state->is_running = true;
    bclock_start(&engine_state->clock);
    bclock_update(&engine_state->clock);
    engine_state->last_time = engine_state->clock.elapsed;
    // f64 running_time = 0;
    // u8 frame_count = 0;
    f64 target_frame_seconds = 1.0f / 60;
    f64 frame_elapsed_time = 0;

    BINFO(get_memory_usage_str());
    
    void* timeline_state = systems_manager_get_state(B_SYSTEM_TYPE_TIMELINE);
    while (engine_state->is_running)
    {
        if (!platform_pump_messages())
            engine_state->is_running = false;

        if (!engine_state->is_suspended)
        {
            // Update clock and get delta time
            bclock_update(&engine_state->clock);
            f64 current_time = engine_state->clock.elapsed;
            f64 delta = (current_time - engine_state->last_time);
            f64 frame_start_time = platform_get_absolute_time();

            // Reset frame allocator
            engine_state->p_frame_data.allocator.free_all();

            // Update systems
            systems_manager_update(&engine_state->sys_manager_state, &engine_state->p_frame_data);
            // Update timelines
            timeline_system_update(timeline_state, delta);

            // Update metrics
            metrics_update(frame_elapsed_time);

            // Make sure the window is not currently being resized by waiting a designated number of frames after the last resize operation
            if (engine_state->resizing)
            {
                engine_state->frames_since_resize++;

                // If required number of frames have passed since the resize, perform actual updates
                if (engine_state->frames_since_resize >= 30)
                {
                    renderer_on_resized(engine_state->width, engine_state->height);

                    renderer_frame_prepare(&engine_state->p_frame_data);

                    // Notify application of the resize
                    engine_state->game_inst->on_resize(engine_state->game_inst, engine_state->width, engine_state->height);

                    engine_state->frames_since_resize = 0;
                    engine_state->resizing = false;
                }
                else
                {
                    // Skip rendering the frame and try again next time
                    // NOTE: Simulate a frame being "drawn" at 60 FPS
                    // platform_sleep(16);
                }

                // Either way, don't process this frame any further while resizing
                // Try again next frame
                continue;
            }
            if (!renderer_frame_prepare(&engine_state->p_frame_data))
            {
                engine_state->game_inst->on_resize(engine_state->game_inst, engine_state->width, engine_state->height);
                continue;
            }

            if (!engine_state->game_inst->update(engine_state->game_inst, &engine_state->p_frame_data))
            {
                BFATAL("Game update failed, shutting down");
                engine_state->is_running = false;
                break;
            }

            if (!renderer_begin(&engine_state->p_frame_data))
            {
                BFATAL("Failed to begin renderer. Shutting down...");
                engine_state->is_running = false;
                break;
            }

            // Begin "prepare_frame" render event grouping
            renderer_begin_debug_label("prepare_frame", (vec3){1.0f, 1.0f, 0.0f});

            systems_manager_renderer_frame_prepare(&engine_state->sys_manager_state, &engine_state->p_frame_data);

            // Have the application generate render packet
            b8 prepare_result = engine_state->game_inst->prepare_frame(engine_state->game_inst, &engine_state->p_frame_data);
            // End "prepare_frame" render event grouping
            renderer_end_debug_label();

            if (!prepare_result)
                continue;

            // Call game's render routine
            if (!engine_state->game_inst->render_frame(engine_state->game_inst, &engine_state->p_frame_data))
            {
                BFATAL("Game render failed, shutting down");
                engine_state->is_running = false;
                break;
            }

            // End the frame
            renderer_end(&engine_state->p_frame_data);

            // Present the frame
            if (!renderer_present(&engine_state->p_frame_data))
            {
                BERROR("The call to renderer_present failed. This is likely unrecoverable. Shutting down...");
                engine_state->is_running = false;
                break;
            }

            // Calculate how long the frame took
            f64 frame_end_time = platform_get_absolute_time();
            frame_elapsed_time = frame_end_time - frame_start_time;
            //running_time += frame_elapsed_time;
            f64 remaining_seconds = target_frame_seconds - frame_elapsed_time;

            if (remaining_seconds > 0)
            {
                u64 remaining_ms = (remaining_seconds * 1000);

                // If there is time left, give it back to the OS.
                b8 limit_frames = false;
                if (remaining_ms > 0 && limit_frames)
                    platform_sleep(remaining_ms - 1);

                // frame_count++;
            }

            input_update(&engine_state->p_frame_data);

            // Update last time
            engine_state->last_time = current_time;
        }
    }

    engine_state->is_running = false;
    game_inst->stage = APPLICATION_STAGE_SHUTTING_DOWN;

    // Shutdown the game
    engine_state->game_inst->shutdown(engine_state->game_inst);

    // Unregister from events
    event_unregister(EVENT_CODE_APPLICATION_QUIT, 0, engine_on_event);

    // Shut down all systems
    systems_manager_shutdown(&engine_state->sys_manager_state);

    game_inst->stage = APPLICATION_STAGE_UNINITIALIZED;

    return true;
}

void engine_on_event_system_initialized(void)
{
    // Register for engine-level events
    event_register(EVENT_CODE_APPLICATION_QUIT, 0, engine_on_event);
    event_register(EVENT_CODE_RESIZED, 0, engine_on_resized);
}

const struct frame_data* engine_frame_data_get(struct application* game_inst)
{
    return &((engine_state_t*)game_inst->engine_state)->p_frame_data;
}

systems_manager_state* engine_systems_manager_state_get(struct application* game_inst)
{
    return &((engine_state_t*)game_inst->engine_state)->sys_manager_state;
}

static b8 engine_on_event(u16 code, void* sender, void* listener_inst, event_context context)
{
    switch (code)
    {
        case EVENT_CODE_APPLICATION_QUIT:
        {
            BINFO("EVENT_CODE_APPLICATION_QUIT recieved, shutting down\n");
            engine_state->is_running = false;
            return true;
        }
    }

    return false;
}

static b8 engine_on_resized(u16 code, void* sender, void* listener_inst, event_context context)
{
    if (code == EVENT_CODE_RESIZED)
    {
        // Flag as resizing and store the change, but wait to regenerate
        engine_state->resizing = true;
        // Also reset the frame count since last resize operation
        engine_state->frames_since_resize = 0;

        u16 width = context.data.u16[0];
        u16 height = context.data.u16[1];

        // Check if different. If so, trigger a resize event
        if (width != engine_state->width || height != engine_state->height)
        {
            engine_state->width = width;
            engine_state->height = height;

            BDEBUG("Window resize: %i, %i", width, height);

            // Handle minimization
            if (width == 0 || height == 0)
            {
                BINFO("Window minimized, suspending application");
                engine_state->is_suspended = true;
                return true;
            }
            else
            {
                if (engine_state->is_suspended)
                {
                    BINFO("Window restored, resuming application");
                    engine_state->is_suspended = false;
                }
            }
        }
    }

    return false;
}
