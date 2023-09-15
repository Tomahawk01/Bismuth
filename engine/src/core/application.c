#include "application.h"
#include "game_types.h"
#include "logger.h"
#include "platform/platform.h"
#include "core/bmemory.h"
#include "core/event.h"
#include "core/input.h"
#include "core/clock.h"
#include "renderer/renderer_frontend.h"

typedef struct application_state
{
    game* game_inst;
    b8 is_running;
    b8 is_suspended;
    platform_state platform;
    i16 width;
    i16 height;
    clock clock;
    f64 last_time;
} application_state;

static b8 initialized = false;
static application_state app_state;

// Event handlers
b8 application_on_event(u16 code, void* sender, void* listener_inst, event_context context);
b8 application_on_key(u16 code, void* sender, void* listener_inst, event_context context);
b8 application_on_resized(u16 code, void* sender, void* listener_inst, event_context context);

b8 application_create(game* game_inst)
{
    if (initialized)
    {
        BERROR("application_create called more than once");
        return false;
    }

    app_state.game_inst = game_inst;

    // Initialize subsystems
    initialize_logging();
    input_initialize();

    // TODO: Remove this later
    BFATAL("Test message: %f", 3.14f);
    BERROR("Test message: %f", 3.14f);
    BWARN("Test message: %f", 3.14f);
    BINFO("Test message: %f", 3.14f);
    BDEBUG("Test message: %f", 3.14f);
    BTRACE("Test message: %f", 3.14f);

    app_state.is_running = true;
    app_state.is_suspended = false;

    if (!event_initialize())
    {
        BERROR("Event system failed initialization. Application cannot continue)");
        return false;
    }

    event_register(EVENT_CODE_APPLICATION_QUIT, 0, application_on_event);
    event_register(EVENT_CODE_KEY_PRESSED, 0, application_on_key);
    event_register(EVENT_CODE_KEY_RELEASED, 0, application_on_key);
    event_register(EVENT_CODE_RESIZED, 0, application_on_resized);

    if (!platform_startup(
        &app_state.platform,
        game_inst->app_config.name,
        game_inst->app_config.start_pos_x,
        game_inst->app_config.start_pos_y,
        game_inst->app_config.start_width,
        game_inst->app_config.start_height))
    {
        return false;
    }

    // Renderer startup
    if (!renderer_initialize(game_inst->app_config.name, &app_state.platform))
    {
        BFATAL("Failed to initialize renderer. Application shutting down...");
        return false;
    }

    // Initialize the game
    if (!app_state.game_inst->initialize(app_state.game_inst))
    {
        BFATAL("Game failed to initialize");
        return false;
    }
    
    app_state.game_inst->on_resize(app_state.game_inst, app_state.width, app_state.height);

    initialized = true;
    return true;
}

b8 application_run()
{
    clock_start(&app_state.clock);
    clock_update(&app_state.clock);
    app_state.last_time = app_state.clock.elapsed;
    f64 running_time = 0;
    u8 frame_count = 0;
    f64 target_frame_seconds = 1.0f / 60;

    BINFO(get_memory_usage_str());
    
    while (app_state.is_running)
    {
        if (!platform_pump_messages(&app_state.platform))
            app_state.is_running = false;

        if (!app_state.is_suspended)
        {
            // Update clock and get delta time
            clock_update(&app_state.clock);
            f64 current_time = app_state.clock.elapsed;
            f64 delta = (current_time - app_state.last_time);
            f64 frame_start_time = platform_get_absolute_time();

            if (!app_state.game_inst->update(app_state.game_inst, (f32)delta))
            {
                BFATAL("Game update failed, shutting down");
                app_state.is_running = false;
                break;
            }
            
            if (!app_state.game_inst->render(app_state.game_inst, (f32)delta))
            {
                BFATAL("Game render failed, shutting down");
                app_state.is_running = false;
                break;
            }

            // TODO: refactor packet creation
            render_packet packet;
            packet.delta_time = delta;
            renderer_draw_frame(&packet);

            // Calculate how long the frame took
            f64 frame_end_time = platform_get_absolute_time();
            f64 frame_elapsed_time = frame_end_time - frame_start_time;
            running_time += frame_elapsed_time;
            f64 remaining_seconds = target_frame_seconds - frame_elapsed_time;

            if (remaining_seconds > 0)
            {
                u64 remaining_ms = (remaining_seconds * 1000);

                // If there is time left, give it back to the OS.
                b8 limit_frames = false;
                if (remaining_ms > 0 && limit_frames)
                    platform_sleep(remaining_ms - 1);

                frame_count++;
            }

            input_update(delta);

            // Update last time
            app_state.last_time = current_time;
        }
    }

    app_state.is_running = false;

    // Shutdown event system
    event_unregister(EVENT_CODE_APPLICATION_QUIT, 0, application_on_event);
    event_unregister(EVENT_CODE_KEY_PRESSED, 0, application_on_key);
    event_unregister(EVENT_CODE_KEY_RELEASED, 0, application_on_key);
    
    event_shutdown();
    input_shutdown();

    renderer_shutdown();

    platform_shutdown(&app_state.platform);

    return true;
}

void application_get_framebuffer_size(u32* width, u32* height)
{
    *width = app_state.width;
    *height = app_state.height;
}

b8 application_on_event(u16 code, void* sender, void* listener_inst, event_context context)
{
    switch (code)
    {
        case EVENT_CODE_APPLICATION_QUIT:
        {
            BINFO("EVENT_CODE_APPLICATION_QUIT recieved, shutting down\n");
            app_state.is_running = false;
            return true;
        }
    }

    return false;
}

b8 application_on_key(u16 code, void* sender, void* listener_inst, event_context context)
{
    if (code == EVENT_CODE_KEY_PRESSED)
    {
        u16 key_code = context.data.u16[0];
        if (key_code == KEY_ESCAPE)
        {
            event_context data = {};
            event_fire(EVENT_CODE_APPLICATION_QUIT, 0, data);

            return true;
        }
        else if (key_code == KEY_A)
        {
            // NOTE: Test code
            BDEBUG("Explicit - A key pressed");
        }
        else
        {
            BDEBUG("'%c' key pressed in window", key_code);
        }
    }
    else if (code == EVENT_CODE_KEY_RELEASED)
    {
        u16 key_code = context.data.u16[0];
        if (key_code == KEY_B)
        {
            // NOTE: Test code
            BDEBUG("Explicit - B key released");
        }
        else
        {
            BDEBUG("'%c' key released in window", key_code);
        }
    }
    return false;    
}

b8 application_on_resized(u16 code, void* sender, void* listener_inst, event_context context)
{
    if (code == EVENT_CODE_RESIZED)
    {
        u16 width = context.data.u16[0];
        u16 height = context.data.u16[1];

        // Check if different. If so, trigger a resize event
        if (width != app_state.width || height != app_state.height)
        {
            app_state.width = width;
            app_state.height = height;

            BDEBUG("Window resize: %i, %i", width, height);

            // Handle minimization
            if (width == 0 || height == 0)
            {
                BINFO("Window minimized, suspending application");
                app_state.is_suspended = true;
                return true;
            }
            else
            {
                if (app_state.is_suspended)
                {
                    BINFO("Window restored, resuming application");
                    app_state.is_suspended = false;
                }
                app_state.game_inst->on_resize(app_state.game_inst, width, height);
                renderer_on_resized(width, height);
            }
        }
    }
    
    return false;
}
