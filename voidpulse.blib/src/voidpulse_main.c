#include "voidpulse_main.h"
#include "version.h"

#include "core/keymap.h"
#include "defines.h"
#include "identifiers/bhandle.h"
#include "input_types.h"
#include "math/geometry.h"
#include "voidpulse_types.h"
#include "physics/bphysics_system.h"
#include "physics/physics_types.h"
#include "renderer/renderer_types.h"
#include "strings/bname.h"
#include "systems/bresource_system.h"

#include <application/application_types.h>
#include <containers/darray.h>
#include <core/console.h>
#include <core/engine.h>
#include <core/event.h>
#include <core/frame_data.h>
#include <core/input.h>
#include <core/bvar.h>
#include <core/metrics.h>
#include <editor/editor_gizmo_rendergraph_node.h>
#include <logger.h>
#include <math/bmath.h>
#include <memory/bmemory.h>
#include <renderer/camera.h>
#include <renderer/renderer_frontend.h>
#include <renderer/rendergraph.h>
#include <renderer/rendergraph_nodes/debug_rendergraph_node.h>
#include <renderer/rendergraph_nodes/forward_rendergraph_node.h>
#include <renderer/rendergraph_nodes/shadow_rendergraph_node.h>
#include <renderer/viewport.h>
#include <rendergraph_nodes/ui_rendergraph_node.h>
#include <resources/debug/debug_box3d.h>
#include <resources/scene.h>
#include <resources/skybox.h>
#include <resources/water_plane.h>
#include <strings/bstring.h>
#include <systems/camera_system.h>
#include <systems/light_system.h>
#include <systems/plugin_system.h>
#include <systems/texture_system.h>
#include <systems/timeline_system.h>
#include <time/bclock.h>
#include <time/time_utils.h>

// Standard UI
#include <controls/sui_button.h>
#include <controls/sui_label.h>
#include <controls/sui_panel.h>
#include <standard_ui_plugin_main.h>
#include <standard_ui_system.h>

// Audio
#include <audio/audio_frontend.h>

// TODO: debug only stuff, change to debug-only when this isn't as critical to have
#include <debug_console.h>

// Utils plugin
#include <editor/editor_gizmo.h>

// Game files
#include "systems/xform_system.h"
#include "track.h"

struct baudio_system_state;

static void game_on_escape_callback(keys key, keymap_entry_bind_type type, keymap_modifier modifiers, void* user_data);
static void setup_keymaps(application* app);
static void remove_keymaps(application* app);
static void change_current_camera(keys key, keymap_entry_bind_type type, keymap_modifier modifiers, void* user_data);
static void game_on_yaw(keys key, keymap_entry_bind_type type, keymap_modifier modifiers, void* user_data);
static void game_on_pitch(keys key, keymap_entry_bind_type type, keymap_modifier modifiers, void* user_data);
static void game_on_move_forward(keys key, keymap_entry_bind_type type, keymap_modifier modifiers, void* user_data);
static void game_on_move_backward(keys key, keymap_entry_bind_type type, keymap_modifier modifiers, void* user_data);
static void game_on_move_left(keys key, keymap_entry_bind_type type, keymap_modifier modifiers, void* user_data);
static void game_on_move_right(keys key, keymap_entry_bind_type type, keymap_modifier modifiers, void* user_data);
static void game_on_move_up(keys key, keymap_entry_bind_type type, keymap_modifier modifiers, void* user_data);
static void game_on_move_down(keys key, keymap_entry_bind_type type, keymap_modifier modifiers, void* user_data);
static void game_on_console_change_visibility(keys key, keymap_entry_bind_type type, keymap_modifier modifiers, void* user_data);
static void game_on_set_render_mode_default(keys key, keymap_entry_bind_type type, keymap_modifier modifiers, void* user_data);
static void game_on_set_render_mode_lighting(keys key, keymap_entry_bind_type type, keymap_modifier modifiers, void* user_data);
static void game_on_set_render_mode_normals(keys key, keymap_entry_bind_type type, keymap_modifier modifiers, void* user_data);
static void game_on_set_render_mode_cascades(keys key, keymap_entry_bind_type type, keymap_modifier modifiers, void* user_data);
static void game_on_set_render_mode_wireframe(keys key, keymap_entry_bind_type type, keymap_modifier modifiers, void* user_data);
static void game_on_set_gizmo_mode(keys key, keymap_entry_bind_type type, keymap_modifier modifiers, void* user_data);
static void game_on_gizmo_orientation_set(keys key, keymap_entry_bind_type type, keymap_modifier modifiers, void* user_data);
static void game_on_load_scene(keys key, keymap_entry_bind_type type, keymap_modifier modifiers, void* user_data);
static void game_on_save_scene(keys key, keymap_entry_bind_type type, keymap_modifier modifiers, void* user_data);
static void game_on_unload_scene(keys key, keymap_entry_bind_type type, keymap_modifier modifiers, void* user_data);
static void game_on_play_sound(keys key, keymap_entry_bind_type type, keymap_modifier modifiers, void* user_data);
static void game_on_toggle_sound(keys key, keymap_entry_bind_type type, keymap_modifier modifiers, void* user_data);
static void game_on_console_scroll(keys key, keymap_entry_bind_type type, keymap_modifier modifiers, void* user_data);
static void game_on_console_scroll_hold(keys key, keymap_entry_bind_type type, keymap_modifier modifiers, void* user_data);
static void game_on_console_history_back(keys key, keymap_entry_bind_type type, keymap_modifier modifiers, void* user_data);
static void game_on_console_history_forward(keys key, keymap_entry_bind_type type, keymap_modifier modifiers, void* user_data);
static void game_on_debug_vsync_toggle(keys key, keymap_entry_bind_type type, keymap_modifier modifiers, void* user_data);
static f32 get_engine_delta_time(void);
static f32 get_engine_total_time(void);

u64 application_state_size(void)
{
    return sizeof(game_state);
}

game_state* game_state_get(struct application* app)
{
    return (game_state*)app->state;
}

game_settings* game_settings_get(struct application* app)
{
    return &(game_state_get(app)->settings);
}

b8 application_boot(struct application* app)
{
    BINFO("Booting Void Pulse (%s)...", BVERSION);

    // Allocate the game state
    app->state = ballocate(sizeof(game_state), MEMORY_TAG_GAME);
    game_state* state = app->state;
    state->running = false;

    application_config* config = &app->app_config;

    config->frame_allocator_size = MEBIBYTES(64);
    config->app_frame_data_size = sizeof(voidpulse_frame_data);

    // Register custom rendergraph nodes, systems, etc
    // TODO: only do this in debug builds
    if (!editor_gizmo_rendergraph_node_register_factory())
    {
        BERROR("Failed to register editor_gizmo rendergraph node");
        return false;
    }

    // Keymaps
    setup_keymaps(app);

    input_keymap_push(&state->global_keymap);

    // Set default game mode and keymap
    state->mode = GAME_MODE_WORLD;
    state->current_camera = state->vehicle_camera;
    input_keymap_push(&state->world_keymap);

    // TODO: Console commands

    return true;
}

b8 application_initialize(struct application* app)
{
    BINFO("Initializing application");

    game_state* state = app->state;

    state->audio_system = engine_systems_get()->audio_system;

    // Get the standard ui plugin
    state->sui_plugin = plugin_system_get(engine_systems_get()->plugin_system, "bismuth.plugin.ui.standard");
    state->sui_plugin_state = state->sui_plugin->plugin_state;
    state->sui_state = state->sui_plugin_state->state;
    standard_ui_state* sui_state = state->sui_state;

#ifdef BISMUTH_DEBUG
    if (!debug_console_create(state->sui_state, &((game_state*)app->state)->debug_console))
        BERROR("Failed to create debug console");
#endif

    // TODO: register for events here

    // Pick out rendergraph(s) config from app config, create/init them from here, save off to state
    application_config* config = &app->app_config;
    u32 rendergraph_count = darray_length(config->rendergraphs);
    if (rendergraph_count < 1)
    {
        BERROR("At least one rendergraph is required in order to run this application");
        return false;
    }

    b8 rendergraph_found = false;
    for (u32 i = 0; i < rendergraph_count; ++i)
    {
        application_rendergraph_config* rg_config = &config->rendergraphs[i];
        if (strings_equali("forward_graph", rg_config->name))
        {
            // Get colorbuffer and depthbuffer from the currently active window
            bwindow* current_window = engine_active_window_get();
            bresource_texture* global_colorbuffer = current_window->renderer_state->colorbuffer;
            bresource_texture* global_depthbuffer = current_window->renderer_state->depthbuffer;

            // Create the rendergraph
            if (!rendergraph_create(rg_config->configuration_str, global_colorbuffer, global_depthbuffer, &state->forward_graph))
            {
                BERROR("Failed to create forward_graph. See logs for details");
                return false;
            }
            rendergraph_found = true;
            break;
        }
    }
    if (!rendergraph_found)
    {
        BERROR("No rendergraph config named 'forward_graph' was found, but is required for this application");
        return false;
    }

    // TODO: Internalize this step? Might need to happen after the rg acquires its resources
    if (!rendergraph_finalize(&state->forward_graph))
    {
        BERROR("Failed to finalize rendergraph. See logs for details");
        return false;
    }

#ifdef BISMUTH_DEBUG
    debug_console_load(&state->debug_console);
#endif

    // Viewport setup
    // World Viewport
    rect_2d world_vp_rect = vec4_create(20.0f, 20.0f, 1280.0f - 40.0f, 720.0f - 40.0f);
    if (!viewport_create(world_vp_rect, deg_to_rad(45.0f), 0.1f, 1000.0f, RENDERER_PROJECTION_MATRIX_TYPE_PERSPECTIVE, &state->world_viewport))
    {
        BERROR("Failed to create world viewport. Cannot start application");
        return false;
    }

    // UI Viewport
    rect_2d ui_vp_rect = vec4_create(0.0f, 0.0f, 1280.0f, 720.0f);
    if (!viewport_create(ui_vp_rect, 0.0f, 0.0f, 100.0f, RENDERER_PROJECTION_MATRIX_TYPE_ORTHOGRAPHIC, &state->ui_viewport))
    {
        BERROR("Failed to create UI viewport. Cannot start application");
        return false;
    }

    // Setup the clear color
    renderer_clear_color_set(engine_systems_get()->renderer_system, (vec4){0.2f, 0.0f, 0.2f, 1.0f});

    // TODO: Only do for debug builds
    // Setup editor gizmo
    if (!editor_gizmo_create(&state->gizmo))
    {
        BERROR("Failed to create editor gizmo!");
        return false;
    }
    if (!editor_gizmo_initialize(&state->gizmo))
    {
        BERROR("Failed to initialize editor gizmo!");
        return false;
    }
    if (!editor_gizmo_load(&state->gizmo))
    {
        BERROR("Failed to load editor gizmo!");
        return false;
    }

    // Setup some UI elements

    // Create test ui text objects
    // black background text
    if (!sui_label_control_create(sui_state, "voidpulse_mono_test_text_black", FONT_TYPE_BITMAP, bname_create("Open Sans 21px"), 21, "test text 123,\n\thello!", &state->debug_text_shadow))
    {
        BERROR("Failed to load basic ui bitmap text");
        return false;
    }
    else
    {
        sui_label_color_set(sui_state, &state->debug_text_shadow, (vec4){0, 0, 0, 1});
        if (!sui_label_control_load(sui_state, &state->debug_text_shadow))
        {
            BERROR("Failed to load test text");
        }
        else
        {
            if (!standard_ui_system_register_control(sui_state, &state->debug_text_shadow))
            {
                BERROR("Unable to register control");
            }
            else
            {
                if (!standard_ui_system_control_add_child(sui_state, 0, &state->debug_text_shadow))
                {
                    BERROR("Failed to parent test text");
                }
                else
                {
                    state->debug_text_shadow.is_active = true;
                    if (!standard_ui_system_update_active(sui_state, &state->debug_text_shadow))
                    {
                        BERROR("Unable to update active state");
                    }
                }
            }
        }
    }
    if (!sui_label_control_create(sui_state, "voidpulse_mono_test_text", FONT_TYPE_BITMAP, bname_create("Open Sans 21px"), 21, "test text 123,\n\thello!", &state->debug_text))
    {
        BERROR("Failed to load basic ui bitmap text");
        return false;
    }
    else
    {
        if (!sui_label_control_load(sui_state, &state->debug_text))
        {
            BERROR("Failed to load test text");
        }
        else
        {
            if (!standard_ui_system_register_control(sui_state, &state->debug_text))
            {
                BERROR("Unable to register control");
            }
            else
            {
                if (!standard_ui_system_control_add_child(sui_state, 0, &state->debug_text))
                {
                    BERROR("Failed to parent test text");
                }
                else
                {
                    state->debug_text.is_active = true;
                    if (!standard_ui_system_update_active(sui_state, &state->debug_text))
                    {
                        BERROR("Unable to update active state");
                    }
                }
            }
        }
    }
    // Move debug text to new bottom of screen
    sui_control_position_set(sui_state, &state->debug_text_shadow, vec3_create(20, state->height - 75, 0));
    sui_control_position_set(sui_state, &state->debug_text, vec3_create(21, state->height - 74, 0));

    // Cameras
    state->vehicle_camera = camera_system_acquire("vehicle");
    camera_position_set(state->vehicle_camera, (vec3){-3.95f, 4.25f, 15.8f});
    camera_rotation_euler_set(state->vehicle_camera, (vec3){-11.5f, -75.0f, 0.0f});
    // Set the active/current camera to the world camera by default
    state->current_camera = state->vehicle_camera;

    // TODO: debug only
    state->editor_camera = camera_system_acquire("editor");
    camera_position_set(state->editor_camera, (vec3){-10.0f, 10.0f, -10.0f});
    camera_rotation_euler_set(state->editor_camera, (vec3){-35.0f, 225.0f, 0.0f});
    state->editor_camera_forward_move_speed = 5.0f * 5.0f;
    state->editor_camera_backward_move_speed = 2.5f * 5.0f;

    // Clocks
    bzero_memory(&state->update_clock, sizeof(bclock));
    bzero_memory(&state->prepare_clock, sizeof(bclock));
    bzero_memory(&state->render_clock, sizeof(bclock));

    // Audio
    // Set some channel volumes
    baudio_master_volume_set(state->audio_system, 0.9f);
    baudio_channel_volume_set(state->audio_system, 0, 1.0f);
    baudio_channel_volume_set(state->audio_system, 1, 1.0f);
    baudio_channel_volume_set(state->audio_system, 2, 1.0f);
    baudio_channel_volume_set(state->audio_system, 3, 1.0f);
    baudio_channel_volume_set(state->audio_system, 4, 1.0f);
    baudio_channel_volume_set(state->audio_system, 7, 0.9f);

    // Finish rendergraph
    if (!rendergraph_initialize(&state->forward_graph))
    {
        BERROR("Failed to initialize rendergraph. See logs for details");
        return false;
    }

    if (!rendergraph_load_resources(&state->forward_graph))
    {
        BERROR("Failed to load resources for rendergraph. See logs for details");
        return false;
    }

    // Setup default game settings
    game_settings* settings = game_settings_get(app);
    settings->chase_camera_delay = 0.85f;
    settings->chase_camera_distance = 10.0f;
    settings->chase_camera_vertical_offset = 3.0f;

    // TODO: Load saved game settings

    state->running = true;

    return true;
}

b8 application_update(struct application* app, struct frame_data* p_frame_data)
{
    voidpulse_frame_data* app_frame_data = (voidpulse_frame_data*)p_frame_data->application_frame_data;
    if (!app_frame_data)
        return true;

    game_state* state = (game_state*)app->state;
    if (!state->running)
        return true;

    bclock_start(&state->update_clock);

    // Update the debug text with camera position
    vec3 pos = camera_position_get(state->current_camera);
    vec3 rot = camera_rotation_euler_get(state->current_camera);

    viewport* view_viewport = &state->world_viewport;

    f32 near_clip = view_viewport->near_clip;
    f32 far_clip = view_viewport->far_clip;

    if (state->track_scene.state == SCENE_STATE_LOADED)
    {
        if (!scene_update(&state->track_scene, p_frame_data))
            BWARN("Failed to update main scene");

        // Update LODs for the scene based on distance from the camera
        scene_update_lod_from_view_position(&state->track_scene, p_frame_data, pos, near_clip, far_clip);

        editor_gizmo_update(&state->gizmo);

        if (bhandle_is_valid(state->test_vehicle_xform))
        {
            mat4 vehicle_xform = xform_local_get(state->test_vehicle_xform);
            vec3 vehicle_position = mat4_position_get(&vehicle_xform);
            vec3 forward = mat4_forward(vehicle_xform);
            f32 delta = get_engine_delta_time();

            // HACK: Should be stored elsewhere
            f32 vehicle_jet_power = 30.0f;
            f32 vehicle_turn_speed = 2.5f;
            
            quat rotation = quat_identity();
            if (state->mode == GAME_MODE_WORLD)
            {
                // Move the vehicle
                if (input_is_key_down(KEY_W))
                {
                    // Apply forward force
                    vec3 vehicle_jet_force = vec3_mul_scalar(forward, vehicle_jet_power);
                    bphysics_body_set_force(engine_systems_get()->physics_system, state->test_vehicle_physics_body, vehicle_jet_force);
                }
                else if (input_is_key_down(KEY_S))
                {
                    // TODO: apply braking force
                }
                else
                {
                    // No force applied
                    // Coasting - maybe apply drag?
                }

                if (input_is_key_down(KEY_A))
                {
                    rotation = quat_mul(rotation, quat_from_axis_angle((vec3){0, 1, 0}, delta * vehicle_turn_speed, false));
                }
                if (input_is_key_down(KEY_D))
                {
                    rotation = quat_mul(rotation, quat_from_axis_angle((vec3){0, 1, 0}, delta * -vehicle_turn_speed, false));
                }
            }
            // Use the physics system to rotate
            bphysics_body_rotate(engine_systems_get()->physics_system, state->test_vehicle_physics_body, rotation);
            
            vehicle_xform = xform_local_get(state->test_vehicle_xform);
            vehicle_position = mat4_position_get(&vehicle_xform);

            xform_calculate_local(state->test_vehicle_mesh_xform);

            xform_calculate_local(state->test_vehicle_xform);

            vehicle_xform = xform_local_get(state->test_vehicle_xform);

            game_settings* settings = game_settings_get(app);

            // Update chase camera to follow
            vec3 backward_offset = vec3_mul_scalar(forward, -settings->chase_camera_distance);
            vec3 upward_offset = vec3_create(0.0f, settings->chase_camera_vertical_offset, 0.0f);
            vec3 target_camera_position = vec3_add(vec3_add(vehicle_position, backward_offset), upward_offset);

            // Direction from point to focus object
            vec3 to_obj = vec3_sub(vehicle_position, target_camera_position);
            vec3 f = vec3_normalized(to_obj);
            f32 yaw = batan2(-f.x, -f.z);
            f32 pitch = basin(f.y);
            vec3 target_euler = {pitch, yaw, 0.0f};

            // Smooth interpolation when yaw is close to 180 or -180
            f32 cyaw = target_euler.y;

            f32 chase_camera_delay = game_settings_get(app)->chase_camera_delay;
            f32 smoothing = (1.0f - chase_camera_delay) * 60.0f; // HACK: should be target FPS
            f32 alpha = 1.0f - bexp(-get_engine_delta_time() * smoothing);

            vec3 camera_position = vec3_lerp(state->vehicle_camera->position, target_camera_position, alpha);
            if (babs(cyaw - target_euler.y) > 3.14159f) // crossing 180° boundary
            {
                BTRACE("crossing");
                cyaw = (cyaw > 0.0f) ? cyaw - 2 * 3.14159f : cyaw + 2 * 3.14159f;
            }

            vec3 camera_rotation = vec3_lerp(state->vehicle_camera->euler_rotation, target_euler, alpha);

            camera_rotation.y = cyaw;

            camera_position_set(state->vehicle_camera, camera_position);
            camera_rotation_euler_set_radians(state->vehicle_camera, camera_rotation);
        }

        // // Perform a small rotation on the first mesh
        // quat rotation = quat_from_axis_angle((vec3){0, 1, 0}, -0.5f * p_frame_data->delta_time, false);
        // transform_rotate(&state->meshes[0].transform, rotation);

        // // Perform a similar rotation on the second mesh, if it exists
        // transform_rotate(&state->meshes[1].transform, rotation);

        // // Perform a similar rotation on the third mesh, if it exists
        // transform_rotate(&state->meshes[2].transform, rotation);
        /* if (state->p_light_1)
        {
            state->p_light_1->data.color = (vec4){
                BCLAMP(bsin(get_engine_delta_time()) * 75.0f + 50.0f, 0.0f, 100.0f),
                BCLAMP(bsin(get_engine_delta_time() - (B_2PI / 3)) * 75.0f + 50.0f, 0.0f, 100.0f),
                BCLAMP(bsin(get_engine_delta_time() - (B_4PI / 3)) * 75.0f + 50.0f, 0.0f, 100.0f),
                1.0f};
            state->p_light_1->data.position.z = 20.0f + bsin(get_engine_delta_time());

            // Make the audio emitter follow it
            // TODO: Get emitter from scene and change its position
            state->test_emitter.position = vec3_from_vec4(state->p_light_1->data.position);
        } */
    }
    else if (state->track_scene.state == SCENE_STATE_UNLOADING)
    {
        // A final update call is required to unload the scene in this state
        scene_update(&state->track_scene, p_frame_data);
    }
    else if (state->track_scene.state == SCENE_STATE_UNLOADED)
    {
        BTRACE("Destroying track scene");
        // Unloading complete, destroy it
        scene_destroy(&state->track_scene);
    }

    // Only track these things once actually running
    if (state->running)
    {
        // Also tack on current mouse state
        b8 left_down = input_is_button_down(MOUSE_BUTTON_LEFT);
        b8 right_down = input_is_button_down(MOUSE_BUTTON_RIGHT);
        i32 mouse_x, mouse_y;
        input_get_mouse_position(&mouse_x, &mouse_y);

        // Convert to NDC
        f32 mouse_x_ndc = range_convert_f32((f32)mouse_x, 0.0f, (f32)state->width, -1.0f, 1.0f);
        f32 mouse_y_ndc = range_convert_f32((f32)mouse_y, 0.0f, (f32)state->height, -1.0f, 1.0f);

        f64 fps, frame_time;
        metrics_frame(&fps, &frame_time);

        // Keep a running average of update and render timers over the last ~1 second
        static f64 accumulated_ms = 0;
        static f32 total_update_seconds = 0;
        static f32 total_prepare_seconds = 0;
        static f32 total_render_seconds = 0;

        static f32 total_update_avg_us = 0;
        static f32 total_prepare_avg_us = 0;
        static f32 total_render_avg_us = 0;
        static f32 total_avg = 0; // total average across the frame

        total_update_seconds += state->last_update_elapsed;
        total_prepare_seconds += state->prepare_clock.elapsed;
        total_render_seconds += state->render_clock.elapsed;
        accumulated_ms += frame_time;

        // Once ~1 second has gone by, calculate the average and wipe the accumulators
        if (accumulated_ms >= 1000.0f)
        {
            total_update_avg_us = (total_update_seconds / accumulated_ms) * B_SEC_TO_US_MULTIPLIER;
            total_prepare_avg_us = (total_prepare_seconds / accumulated_ms) * B_SEC_TO_US_MULTIPLIER;
            total_render_avg_us = (total_render_seconds / accumulated_ms) * B_SEC_TO_US_MULTIPLIER;
            total_avg = total_update_avg_us + total_prepare_avg_us + total_render_avg_us;
            total_render_seconds = 0;
            total_prepare_seconds = 0;
            total_update_seconds = 0;
            accumulated_ms = 0;
        }

        char* vsync_text = renderer_flag_enabled_get(RENDERER_CONFIG_FLAG_VSYNC_ENABLED_BIT) ? "YES" : " NO";
        const char* time_str = time_as_string_from_seconds(get_engine_total_time());
        const char* game_mode_text = "WORLD";
        switch (state->mode)
        {
        case GAME_MODE_WORLD:
            game_mode_text = "WORLD";
            break;
        case GAME_MODE_EDITOR:
            game_mode_text = "EDITOR";
            break;
        case MAIN_MENU:
            game_mode_text = "MAIN_MENU";
            break;
        case PAUSED_MENU:
            game_mode_text = "PAUSE";
            break;
        }

        char* text_buffer = string_format(
            "\
FPS: %5.1f(%4.1fms)        Pos=[%7.3f %7.3f %7.3f] Rot=[%7.3f, %7.3f, %7.3f]\n\
Upd: %8.3fus, Prep: %8.3fus, Rend: %8.3fus, Total: %8.3fus \n\
Mouse: X=%-5d Y=%-5d   L=%s R=%s   NDC: X=%.6f, Y=%.6f\n\
VSync: %s Drawn: %-5u (%-5u shadow pass), Mode: %s, Run time: %s",
            fps,
            frame_time,
            pos.x, pos.y, pos.z,
            rad_to_deg(rot.x), rad_to_deg(rot.y), rad_to_deg(rot.z),
            total_update_avg_us,
            total_prepare_avg_us,
            total_render_avg_us,
            total_avg,
            mouse_x, mouse_y,
            left_down ? "Y" : "N",
            right_down ? "Y" : "N",
            mouse_x_ndc,
            mouse_y_ndc,
            vsync_text,
            p_frame_data->drawn_mesh_count,
            p_frame_data->drawn_shadow_mesh_count,
            game_mode_text,
            time_str);

        // Update the text control
        sui_label_text_set(state->sui_state, &state->debug_text, text_buffer);
        sui_label_text_set(state->sui_state, &state->debug_text_shadow, text_buffer);
        string_free(text_buffer);
        string_free(time_str);
    }

#ifdef BISMUTH_DEBUG
    debug_console_update(&((game_state*)app->state)->debug_console);
#endif

    vec3 sound_pos, sound_forward, sound_up;
    if (state->mode == GAME_MODE_EDITOR)
    {
        // In editor mode, the sound follows the camera
        sound_pos = pos;
        sound_forward = camera_forward(state->current_camera);
        sound_up = camera_up(state->current_camera);
    }
    else if (state->mode == GAME_MODE_WORLD)
    {
        // In world mode, the sound follows the vehicle
        mat4 vehicle_xform = xform_local_get(state->test_vehicle_xform);
        sound_pos = mat4_position_get(&vehicle_xform);
        sound_forward = mat4_forward(vehicle_xform);
        sound_up = mat4_up(vehicle_xform);
    }
    else
    {
        // The fallback case acts like editor mode, where the sound follows the camera
        sound_pos = pos;
        sound_forward = camera_forward(state->current_camera);
        sound_up = camera_up(state->current_camera);
    }
    // Update the listener orientation
    baudio_system_listener_orientation_set(engine_systems_get()->audio_system, sound_pos, sound_forward, sound_up);

    bclock_update(&state->update_clock);
    state->last_update_elapsed = state->update_clock.elapsed;

    return true;
}

b8 application_prepare_frame(struct application* app, struct frame_data* p_frame_data)
{
    game_state* state = (game_state*)app->state;
    if (!state->running)
        return false;

    bclock_start(&state->prepare_clock);

    /* scene* scene = &state->main_scene; */
    scene* scene = &state->track_scene;
    viewport* current_viewport = &state->world_viewport;

    // HACK: Using the first light in the collection for now
    // TODO: Support for multiple directional lights with priority sorting
    directional_light* dir_light = scene->dir_lights ? &scene->dir_lights[0] : 0;

    // Global setup
    f32 near = current_viewport->near_clip;
    f32 far = dir_light ? dir_light->data.shadow_distance + dir_light->data.shadow_fade_distance : 0;
    f32 clip_range = far - near;

    f32 min_z = near;
    f32 max_z = near + clip_range;
    f32 range = max_z - min_z;
    f32 ratio = max_z / min_z;

    f32 cascade_split_multiplier = dir_light ? dir_light->data.shadow_split_mult : 0.95f;

    // Calculate splits based on view camera frustum
    vec4 splits;
    for (u32 c = 0; c < MATERIAL_MAX_SHADOW_CASCADES; c++)
    {
        f32 p = (c + 1) / (f32)MATERIAL_MAX_SHADOW_CASCADES;
        f32 log = min_z * bpow(ratio, p);
        f32 uniform = min_z + range * p;
        f32 d = cascade_split_multiplier * (log - uniform) + uniform;
        splits.elements[c] = (d - near) / clip_range;
    }

    // Default values to use in the event there is no directional light
    // These are required because the scene pass needs them
    mat4 shadow_camera_view_projections[MATERIAL_MAX_SHADOW_CASCADES];
    for (u32 i = 0; i < MATERIAL_MAX_SHADOW_CASCADES; ++i)
    {
        shadow_camera_view_projections[i] = mat4_identity();
    }

    // TODO: Anything to do here?
    // FIXME: Cache this instead of looking up every frame
    u32 node_count = state->forward_graph.node_count;
    for (u32 i = 0; i < node_count; ++i)
    {
        rendergraph_node* node = &state->forward_graph.nodes[i];
        if (strings_equali(node->name, "sui"))
        {
            ui_rendergraph_node_set_atlas(node, state->sui_state->atlas_texture);

            // We have the one
            ui_rendergraph_node_set_viewport_and_matrices(node, state->ui_viewport, mat4_identity(), state->ui_viewport.projection);

            // Gather SUI render data
            standard_ui_render_data render_data = {0};

            // Renderables
            render_data.renderables = darray_create_with_allocator(standard_ui_renderable, &p_frame_data->allocator);
            if (!standard_ui_system_render(state->sui_state, 0, p_frame_data, &render_data))
                BERROR("The standard ui system failed to render");
            ui_rendergraph_node_set_render_data(node, render_data);
        }
        else if (strings_equali(node->name, "forward"))
        {
            // Ensure internal lists, etc. are reset
            forward_rendergraph_node_reset(node);
            forward_rendergraph_node_viewport_set(node, state->world_viewport);
            forward_rendergraph_node_camera_projection_set(node, state->current_camera, current_viewport->projection);

            // Tell our scene to generate relevant render data if it is loaded
            if (scene->state == SCENE_STATE_LOADED)
            {
                // Only render if the scene is loaded

                // SKYBOX
                // HACK: Just use the first one for now
                // TODO: Support for multiple skyboxes, possibly transition between them
                u32 skybox_count = darray_length(scene->skyboxes);
                forward_rendergraph_node_set_skybox(node, skybox_count ? &scene->skyboxes[0] : 0);

                // SCENE
                scene_render_frame_prepare(scene, p_frame_data);

                // Pass over shadow map "camera" view and projection matrices (one per cascade)
                for (u32 c = 0; c < MATERIAL_MAX_SHADOW_CASCADES; c++)
                {
                    forward_rendergraph_node_cascade_data_set(
                        node,
                        (near + splits.elements[c] * clip_range) * 1.0f, // splits.elements[c]
                        shadow_camera_view_projections[c],
                        c);
                }
                // Ensure the render mode is set
                forward_rendergraph_node_render_mode_set(node, state->render_mode);

                // Tell it about the directional light
                forward_rendergraph_node_directional_light_set(node, dir_light);

                // HACK: use the skybox cubemap as the irradiance texture for now
                // HACK: #2 Support for multiple skyboxes, but using the first one for now
                // TODO: Support multiple skyboxes/irradiance maps
                forward_rendergraph_node_irradiance_texture_set(node, p_frame_data, scene->skyboxes ? scene->skyboxes[0].cubemap : texture_system_request(bname_create(DEFAULT_CUBE_TEXTURE_NAME), INVALID_BNAME, 0, 0));

                // Camera frustum culling and count
                viewport* v = current_viewport;
                vec3 forward = camera_forward(state->current_camera);
                vec3 target = vec3_add(state->current_camera->position, vec3_mul_scalar(forward, far));
                vec3 up = camera_up(state->current_camera);
                // TODO: move frustum to be managed by camera it is attached to
                frustum camera_frustum = frustum_create(&state->current_camera->position, &target,
                                                        &up, v->rect.width / v->rect.height, v->fov, v->near_clip, v->far_clip);

                p_frame_data->drawn_mesh_count = 0;

                u32 geometry_count = 0;
                geometry_render_data* geometries = darray_reserve_with_allocator(geometry_render_data, 512, &p_frame_data->allocator);

                // Query the scene for static meshes using the camera frustum
                if (!scene_mesh_render_data_query(
                        scene,
                        0, // &camera_frustum, // HACK: Frustum culling isn't working right. Disabling for now
                        state->current_camera->position,
                        p_frame_data,
                        &geometry_count, &geometries))
                {
                    BERROR("Failed to query scene pass meshes");
                }

                // HACK: geometry render data for the collision_track
                u32 track_segment_count = darray_length(state->collision_track.segments);
                for (u32 s = 0; s < track_segment_count; ++s)
                {
                    bgeometry* g = &state->collision_track.segments[s].geometry;
                    geometry_render_data data = {0};
                    data.model = mat4_identity();
                    data.material = state->collision_track.material;
                    data.vertex_count = g->vertex_count;
                    data.vertex_buffer_offset = g->vertex_buffer_offset;
                    data.vertex_element_size = g->vertex_element_size;
                    data.index_count = g->index_count;
                    data.index_buffer_offset = g->index_buffer_offset;
                    data.index_element_size = g->index_element_size;
                    data.unique_id = 0;
                    data.winding_inverted = false;
                    data.diffuse_color = vec4_one();
                    darray_insert_at(geometries, 0, data);
                    geometry_count++;
                }

                // Track the number of meshes drawn in the forward pass
                p_frame_data->drawn_mesh_count = geometry_count;
                // Tell the node about them
                forward_rendergraph_node_static_geometries_set(node, p_frame_data, geometry_count, geometries);

                // Add terrain(s)
                u32 terrain_geometry_count = 0;
                geometry_render_data* terrain_geometries = darray_reserve_with_allocator(geometry_render_data, 16, &p_frame_data->allocator);

                // Query the scene for terrain meshes using the camera frustum
                if (!scene_terrain_render_data_query(
                        scene,
                        0, // &camera_frustum, // HACK: Frustum culling isn't working right. Disabling for now
                        state->current_camera->position,
                        p_frame_data,
                        &terrain_geometry_count, &terrain_geometries))
                {
                    BERROR("Failed to query scene pass terrain geometries");
                }

                // TODO: Separate counter for terrain geometries
                p_frame_data->drawn_mesh_count += terrain_geometry_count;
                // Tell the node about them
                forward_rendergraph_node_terrain_geometries_set(node, p_frame_data, terrain_geometry_count, terrain_geometries);

                // Get the count of planes, then the planes themselves
                u32 water_plane_count = 0;
                if (!scene_water_plane_query(scene, &camera_frustum, state->current_camera->position, p_frame_data, &water_plane_count, 0))
                    BERROR("Failed to query scene for water planes");
                water_plane** planes = water_plane_count ? darray_reserve_with_allocator(water_plane* , water_plane_count, &p_frame_data->allocator) : 0;
                if (!scene_water_plane_query(scene, &camera_frustum, state->current_camera->position, p_frame_data, &water_plane_count, &planes))
                    BERROR("Failed to query scene for water planes");

                // Pass the planes to the node
                if (!forward_rendergraph_node_water_planes_set(node, p_frame_data, water_plane_count, planes))
                {
                    // NOTE: Not going to abort the whole graph for this failure, but will bleat about it
                    BERROR("Failed to set water planes for water_plane rendergraph node");
                }
            }
            else
            {
                // Scene not loaded
                forward_rendergraph_node_set_skybox(node, 0);
                forward_rendergraph_node_irradiance_texture_set(node, p_frame_data, 0);

                // Do not run these passes if the scene is not loaded
                // graph->scene_pass.pass_data.do_execute = false;
                // graph->shadowmap_pass.pass_data.do_execute = false;
                forward_rendergraph_node_water_planes_set(node, p_frame_data, 0, 0);
                forward_rendergraph_node_static_geometries_set(node, p_frame_data, 0, 0);
                forward_rendergraph_node_terrain_geometries_set(node, p_frame_data, 0, 0);
            }
        }
        else if (strings_equali(node->name, "shadow"))
        {
            // Shadowmap pass - only runs if there is a directional light
            // TODO: Will also need to run for point lights when implemented
            if (dir_light)
            {
                f32 last_split_dist = 0.0f;

                // Obtain the light direction
                vec3 light_dir = vec3_normalized(vec3_from_vec4(dir_light->data.direction));

                // Tell it about the directional light
                shadow_rendergraph_node_directional_light_set(node, dir_light);

                // frustum culling_frustum;
                vec3 culling_center;
                f32 culling_radius;

                // Get the view-projection matrix
                mat4 shadow_dist_projection = mat4_perspective(
                    current_viewport->fov,
                    current_viewport->rect.width / current_viewport->rect.height,
                    near,
                    far);
                mat4 cam_view_proj = mat4_transposed(mat4_mul(camera_view_get(state->current_camera), shadow_dist_projection));

                // Pass over shadow map "camera" view and projection matrices (one per cascade)
                for (u32 c = 0; c < MATERIAL_MAX_SHADOW_CASCADES; c++)
                {
                    // Get the world-space corners of the view frustum
                    vec4 corners[8] = {
                        {-1.0f, +1.0f, 0.0f, 1.0f},
                        {+1.0f, +1.0f, 0.0f, 1.0f},
                        {+1.0f, -1.0f, 0.0f, 1.0f},
                        {-1.0f, -1.0f, 0.0f, 1.0f},

                        {-1.0f, +1.0f, 1.0f, 1.0f},
                        {+1.0f, +1.0f, 1.0f, 1.0f},
                        {+1.0f, -1.0f, 1.0f, 1.0f},
                        {-1.0f, -1.0f, 1.0f, 1.0f}};

                    mat4 inv_cam = mat4_inverse(cam_view_proj);
                    for (u32 j = 0; j < 8; ++j)
                    {
                        vec4 inv_corner = mat4_mul_vec4(inv_cam, corners[j]);
                        corners[j] = (vec4_div_scalar(inv_corner, inv_corner.w));
                    }

                    // Adjust the corners by pulling/pushing the near/far according to the current split
                    f32 split_dist = splits.elements[c];
                    for (u32 i = 0; i < 4; ++i)
                    {
                        // far - near
                        vec4 dist = vec4_sub(corners[i + 4], corners[i]);
                        corners[i + 4] = vec4_add(corners[i], vec4_mul_scalar(dist, split_dist));
                        corners[i] = vec4_add(corners[i], vec4_mul_scalar(dist, last_split_dist));
                    }

                    // Calculate the center of the camera's frustum by averaging the points
                    // This is also used as the lookat point for the shadow "camera"
                    vec3 center = vec3_zero();
                    for (u32 i = 0; i < 8; ++i)
                        center = vec3_add(center, vec3_from_vec4(corners[i]));
                    center = vec3_div_scalar(center, 8.0f); // size
                    if (c == MATERIAL_MAX_SHADOW_CASCADES - 1)
                        culling_center = center;

                    // Get the furthest-out point from the center and use that as the extents
                    f32 radius = 0.0f;
                    for (u32 i = 0; i < 8; ++i)
                    {
                        f32 distance = vec3_distance(vec3_from_vec4(corners[i]), center);
                        radius = BMAX(radius, distance);
                    }
                    radius = bceil(radius * 16.0f) / 16.0f;

                    if (c == MATERIAL_MAX_SHADOW_CASCADES - 1)
                        culling_radius = radius;

                    // Calculate the extents by using the radius from above
                    extents_3d extents;
                    extents.max = vec3_create(radius, radius, radius);
                    extents.min = vec3_mul_scalar(extents.max, -1.0f);

                    // "Pull" the min inward and "push" the max outward on the z axis to make sure shadow casters outside the view are captured as well (think trees above the player)
                    // TODO: This should be adjustable/tuned per scene
                    f32 z_multiplier = 10.0f;
                    if (extents.min.z < 0)
                        extents.min.z *= z_multiplier;
                    else
                        extents.min.z /= z_multiplier;

                    if (extents.max.z < 0)
                        extents.max.z /= z_multiplier;
                    else
                        extents.max.z *= z_multiplier;

                    // Generate lookat by moving along the opposite direction of the directional light by the minimum extents
                    // This is negated because the directional light points "down" and the camera needs to be "up"
                    vec3 shadow_camera_position = vec3_sub(center, vec3_mul_scalar(light_dir, -extents.min.z));
                    mat4 light_view = mat4_look_at(shadow_camera_position, center, vec3_up());

                    // Generate ortho projection based on extents
                    mat4 light_ortho = mat4_orthographic(extents.min.x, extents.max.x, extents.min.y, extents.max.y, 0.0f, extents.max.z - extents.min.z);

                    // combined view/projection
                    shadow_camera_view_projections[c] = (mat4_mul(light_view, light_ortho));

                    // Build out cascade data to set in shadow rg node
                    shadow_cascade_data cdata = {0};
                    cdata.cascade_index = c;
                    cdata.split_depth = (near + split_dist * clip_range) * -1.0f;
                    cdata.view_projection = shadow_camera_view_projections[c];
                    shadow_rendergraph_node_cascade_data_set(node, cdata, c);

                    last_split_dist = split_dist;
                }

                // Gather the geometries to be rendered
                // NOTE: this only needs to happen once, since all geometries visible by the furthest-out cascase
                // must also be drawn on the nearest cascade to ensure objects outside the view cast shadows into the view properly
                u32 geometry_count = 0;
                geometry_render_data* geometries = darray_reserve_with_allocator(geometry_render_data, 512, &p_frame_data->allocator);
                if (!scene_mesh_render_data_query_from_line(
                        scene,
                        light_dir,
                        culling_center,
                        culling_radius,
                        p_frame_data,
                        &geometry_count, &geometries))
                {
                    BERROR("Failed to query shadow map pass meshes");
                }
                // Track the number of meshes drawn in the shadow pass
                p_frame_data->drawn_shadow_mesh_count = geometry_count;
                // Tell the node about them
                shadow_rendergraph_node_static_geometries_set(node, p_frame_data, geometry_count, geometries);

                // Gather terrain geometries
                u32 terrain_geometry_count = 0;
                geometry_render_data* terrain_geometries = darray_reserve_with_allocator(geometry_render_data, 16, &p_frame_data->allocator);
                if (!scene_terrain_render_data_query_from_line(
                        scene,
                        light_dir,
                        culling_center,
                        culling_radius,
                        p_frame_data,
                        &terrain_geometry_count, &terrain_geometries))
                {
                    BERROR("Failed to query shadow map pass terrain geometries");
                }

                // TODO: Counter for terrain geometries
                p_frame_data->drawn_shadow_mesh_count += terrain_geometry_count;
                // Tell the node about them
                shadow_rendergraph_node_terrain_geometries_set(node, p_frame_data, terrain_geometry_count, terrain_geometries);
            }
        }
        else if (strings_equali(node->name, "debug"))
        {

            debug_rendergraph_node_viewport_set(node, state->world_viewport);
            debug_rendergraph_node_view_projection_set(
                node,
                camera_view_get(state->current_camera),
                camera_position_get(state->current_camera),
                current_viewport->projection);

            u32 debug_geometry_count = 0;
            if (!scene_debug_render_data_query(scene, &debug_geometry_count, 0))
            {
                BERROR("Failed to obtain count of debug render objects");
                return false;
            }
            geometry_render_data* debug_geometries = 0;
            if (debug_geometry_count)
            {
                debug_geometries = darray_reserve_with_allocator(geometry_render_data, debug_geometry_count, &p_frame_data->allocator);

                if (!scene_debug_render_data_query(scene, &debug_geometry_count, &debug_geometries))
                {
                    BERROR("Failed to obtain debug render objects");
                    return false;
                }

                // Make sure the count is correct before pushing
                darray_length_set(debug_geometries, debug_geometry_count);
            }
            else
            {
                debug_geometries = darray_create_with_allocator(geometry_render_data, &p_frame_data->allocator);
            }

            /*
            // TODO: Move this to the scene

            u32 line_count = darray_length(state->test_lines);
            for (u32 i = 0; i < line_count; ++i)
            {
                debug_line3d* line = &state->test_lines[i];
                debug_line3d_render_frame_prepare(line, p_frame_data);
                geometry_render_data rd = {0};
                rd.model = xform_world_get(line->xform);
                bgeometry* g = &line->geometry;
                rd.vertex_count = g->vertex_count;
                rd.vertex_buffer_offset = g->vertex_buffer_offset;
                rd.vertex_element_size = g->vertex_element_size;
                rd.index_count = g->index_count;
                rd.index_buffer_offset = g->index_buffer_offset;
                rd.index_element_size = g->index_element_size;
                rd.unique_id = INVALID_ID_U16;
                darray_push(debug_geometries, rd);
                debug_geometry_count++;
            }
            u32 box_count = darray_length(state->test_boxes);
            for (u32 i = 0; i < box_count; ++i)
            {
                debug_box3d* box = &state->test_boxes[i];
                debug_box3d_render_frame_prepare(box, p_frame_data);
                geometry_render_data rd = {0};
                rd.model = xform_world_get(box->xform);
                bgeometry* g = &box->geometry;
                rd.vertex_count = g->vertex_count;
                rd.vertex_buffer_offset = g->vertex_buffer_offset;
                rd.vertex_element_size = g->vertex_element_size;
                rd.index_count = g->index_count;
                rd.index_buffer_offset = g->index_buffer_offset;
                rd.index_element_size = g->index_element_size;
                rd.unique_id = INVALID_ID_U16;
                darray_push(debug_geometries, rd);
                debug_geometry_count++;
            } */

            // Set geometries in the debug rg node
            if (!debug_rendergraph_node_debug_geometries_set(node, p_frame_data, debug_geometry_count, debug_geometries))
            {
                // NOTE: Not going to abort the whole graph for this failure, but will bleat about it loudly
                BERROR("Failed to set geometries for debug rendergraph node");
            }
        }
        else if (strings_equali(node->name, "editor_gizmo"))
        {
            editor_gizmo_rendergraph_node_viewport_set(node, state->world_viewport);
            editor_gizmo_rendergraph_node_view_projection_set(
                node,
                camera_view_get(state->current_camera),
                camera_position_get(state->current_camera),
                current_viewport->projection);
            if (!editor_gizmo_rendergraph_node_gizmo_set(node, &state->gizmo))
            {
                // NOTE: Not going to abort the whole graph for this failure, but will bleat about it loudly
                BERROR("Failed to set gizmo for editor_gizmo rendergraph node");
            }

            // Only draw if loaded. TODO: re-enable the on-scene-loaded check
            editor_gizmo_rendergraph_node_enabled_set(node, /*scene->state == SCENE_STATE_LOADED*/ false);
        }
    }

    bclock_update(&state->prepare_clock);
    return true;
}

b8 application_render_frame(struct application* app, struct frame_data* p_frame_data)
{
    // Start the frame
    game_state* state = (game_state*)app->state;
    if (!state->running)
        return true;

    bclock_start(&state->render_clock);

    // Execute the rendergraph
    if (!rendergraph_execute_frame(&state->forward_graph, p_frame_data))
    {
        BERROR("Rendergraph failed to execute frame, see logs for details");
        return false;
    }

    bclock_update(&state->render_clock);

    return true;
}

void application_on_window_resize(struct application* app, const struct bwindow* window)
{
    if (!app->state)
        return;

    game_state* state = (game_state*)app->state;

    state->width = window->width;
    state->height = window->height;
    if (!window->width || !window->height)
        return;

    /* f32 half_width = state->width * 0.5f; */

    // Resize viewports
    // World Viewport - right side
    rect_2d world_vp_rect = vec4_create(0.0f, 0.0f, state->width, state->height); // vec4_create(half_width + 20.0f, 20.0f, half_width - 40.0f, state->height - 40.0f);
    viewport_resize(&state->world_viewport, world_vp_rect);

    // UI Viewport
    rect_2d ui_vp_rect = vec4_create(0.0f, 0.0f, state->width, state->height);
    viewport_resize(&state->ui_viewport, ui_vp_rect);

    // Move debug text to new bottom of screen
    sui_control_position_set(state->sui_state, &state->debug_text, vec3_create(20, state->height - 95, 0));
    sui_control_position_set(state->sui_state, &state->debug_text_shadow, vec3_create(21, state->height - 94, 0));
}

void application_shutdown(struct application* app)
{
    game_state* state = (game_state*)app->state;
    state->running = false;

    if (state->track_scene.state == SCENE_STATE_LOADED)
    {
        BDEBUG("Unloading scene...");

        scene_unload(&state->track_scene, true);
        /* clear_debug_objects(app); */
        scene_destroy(&state->track_scene);

        BDEBUG("Done");
    }

    rendergraph_destroy(&state->forward_graph);

#ifdef BISMUTH_DEBUG
    debug_console_unload(&state->debug_console);
#endif
}

void application_lib_on_unload(struct application* app)
{
    // TODO: re-enable
    /* application_unregister_events(app); */
#ifdef BISMUTH_DEBUG
    debug_console_on_lib_unload(&((game_state*)app->state)->debug_console);
#endif
    // TODO: re-enable
    /* game_remove_commands(app); */
    /* game_remove_keymaps(app); */
}

void application_lib_on_load(struct application* app)
{
    // TODO: re-enable
    /* application_register_events(app); */
#ifdef BISMUTH_DEBUG
    debug_console_on_lib_load(&((game_state*)app->state)->debug_console, app->stage >= APPLICATION_STAGE_BOOT_COMPLETE);
#endif
    if (app->stage >= APPLICATION_STAGE_BOOT_COMPLETE)
    {
        // TODO: re-enable
        /* game_setup_commands(app); */
        /* game_setup_keymaps(app); */
    }
}

static void setup_keymaps(application* app)
{
    game_state* state = ((game_state*)app->state);

    // Global keymap
    state->global_keymap = keymap_create();
    keymap_binding_add(&state->global_keymap, KEY_ESCAPE, KEYMAP_BIND_TYPE_PRESS, KEYMAP_MODIFIER_NONE_BIT, app, game_on_escape_callback);
    keymap_binding_add(&state->global_keymap, KEY_V, KEYMAP_BIND_TYPE_PRESS, KEYMAP_MODIFIER_NONE_BIT, app, game_on_debug_vsync_toggle);
    keymap_binding_add(&state->global_keymap, KEY_GRAVE, KEYMAP_BIND_TYPE_PRESS, KEYMAP_MODIFIER_NONE_BIT, app, game_on_console_change_visibility);
    keymap_binding_add(&state->global_keymap, KEY_L, KEYMAP_BIND_TYPE_PRESS, KEYMAP_MODIFIER_NONE_BIT, app, game_on_load_scene);
    keymap_binding_add(&state->global_keymap, KEY_U, KEYMAP_BIND_TYPE_PRESS, KEYMAP_MODIFIER_NONE_BIT, app, game_on_unload_scene);

    // World mode keymap
    state->world_keymap = keymap_create();
    keymap_binding_add(&state->world_keymap, KEY_C, KEYMAP_BIND_TYPE_PRESS, KEYMAP_MODIFIER_NONE_BIT, app, change_current_camera);

    // Editor mode keymap
    state->editor_keymap = keymap_create();
    /* state->editor_keymap.overrides_all = true; */
    keymap_binding_add(&state->editor_keymap, KEY_C, KEYMAP_BIND_TYPE_PRESS, KEYMAP_MODIFIER_NONE_BIT, app, change_current_camera);
    keymap_binding_add(&state->editor_keymap, KEY_W, KEYMAP_BIND_TYPE_HOLD, KEYMAP_MODIFIER_NONE_BIT, app, game_on_move_forward);
    keymap_binding_add(&state->editor_keymap, KEY_S, KEYMAP_BIND_TYPE_HOLD, KEYMAP_MODIFIER_NONE_BIT, app, game_on_move_backward);
    keymap_binding_add(&state->editor_keymap, KEY_A, KEYMAP_BIND_TYPE_HOLD, KEYMAP_MODIFIER_NONE_BIT, app, game_on_move_left);
    keymap_binding_add(&state->editor_keymap, KEY_D, KEYMAP_BIND_TYPE_HOLD, KEYMAP_MODIFIER_NONE_BIT, app, game_on_move_right);
    keymap_binding_add(&state->editor_keymap, KEY_E, KEYMAP_BIND_TYPE_HOLD, KEYMAP_MODIFIER_NONE_BIT, app, game_on_move_up);
    keymap_binding_add(&state->editor_keymap, KEY_Q, KEYMAP_BIND_TYPE_HOLD, KEYMAP_MODIFIER_NONE_BIT, app, game_on_move_down);
    keymap_binding_add(&state->editor_keymap, KEY_LEFT, KEYMAP_BIND_TYPE_HOLD, KEYMAP_MODIFIER_NONE_BIT, app, game_on_yaw);
    keymap_binding_add(&state->editor_keymap, KEY_RIGHT, KEYMAP_BIND_TYPE_HOLD, KEYMAP_MODIFIER_NONE_BIT, app, game_on_yaw);
    keymap_binding_add(&state->editor_keymap, KEY_UP, KEYMAP_BIND_TYPE_HOLD, KEYMAP_MODIFIER_NONE_BIT, app, game_on_pitch);
    keymap_binding_add(&state->editor_keymap, KEY_DOWN, KEYMAP_BIND_TYPE_HOLD, KEYMAP_MODIFIER_NONE_BIT, app, game_on_pitch);
    keymap_binding_add(&state->editor_keymap, KEY_0, KEYMAP_BIND_TYPE_PRESS, KEYMAP_MODIFIER_CONTROL_BIT, app, game_on_set_render_mode_default);
    keymap_binding_add(&state->editor_keymap, KEY_1, KEYMAP_BIND_TYPE_PRESS, KEYMAP_MODIFIER_CONTROL_BIT, app, game_on_set_render_mode_lighting);
    keymap_binding_add(&state->editor_keymap, KEY_2, KEYMAP_BIND_TYPE_PRESS, KEYMAP_MODIFIER_CONTROL_BIT, app, game_on_set_render_mode_normals);
    keymap_binding_add(&state->editor_keymap, KEY_3, KEYMAP_BIND_TYPE_PRESS, KEYMAP_MODIFIER_CONTROL_BIT, app, game_on_set_render_mode_cascades);
    keymap_binding_add(&state->editor_keymap, KEY_4, KEYMAP_BIND_TYPE_PRESS, KEYMAP_MODIFIER_CONTROL_BIT, app, game_on_set_render_mode_wireframe);
    keymap_binding_add(&state->editor_keymap, KEY_1, KEYMAP_BIND_TYPE_PRESS, KEYMAP_MODIFIER_NONE_BIT, app, game_on_set_gizmo_mode);
    keymap_binding_add(&state->editor_keymap, KEY_2, KEYMAP_BIND_TYPE_PRESS, KEYMAP_MODIFIER_NONE_BIT, app, game_on_set_gizmo_mode);
    keymap_binding_add(&state->editor_keymap, KEY_3, KEYMAP_BIND_TYPE_PRESS, KEYMAP_MODIFIER_NONE_BIT, app, game_on_set_gizmo_mode);
    keymap_binding_add(&state->editor_keymap, KEY_4, KEYMAP_BIND_TYPE_PRESS, KEYMAP_MODIFIER_NONE_BIT, app, game_on_set_gizmo_mode);
    keymap_binding_add(&state->editor_keymap, KEY_G, KEYMAP_BIND_TYPE_PRESS, KEYMAP_MODIFIER_NONE_BIT, app, game_on_gizmo_orientation_set);

    // ctrl s
    keymap_binding_add(&state->editor_keymap, KEY_S, KEYMAP_BIND_TYPE_PRESS, KEYMAP_MODIFIER_CONTROL_BIT, app, game_on_save_scene);

    // A console-specific keymap. Is not pushed by default
    state->console_keymap = keymap_create();
    state->console_keymap.overrides_all = true;
    keymap_binding_add(&state->console_keymap, KEY_GRAVE, KEYMAP_BIND_TYPE_PRESS, KEYMAP_MODIFIER_NONE_BIT, app, game_on_console_change_visibility);
    keymap_binding_add(&state->console_keymap, KEY_ESCAPE, KEYMAP_BIND_TYPE_PRESS, KEYMAP_MODIFIER_NONE_BIT, app, game_on_console_change_visibility);
    keymap_binding_add(&state->console_keymap, KEY_PAGEUP, KEYMAP_BIND_TYPE_PRESS, KEYMAP_MODIFIER_NONE_BIT, app, game_on_console_scroll);
    keymap_binding_add(&state->console_keymap, KEY_PAGEDOWN, KEYMAP_BIND_TYPE_PRESS, KEYMAP_MODIFIER_NONE_BIT, app, game_on_console_scroll);
    keymap_binding_add(&state->console_keymap, KEY_PAGEUP, KEYMAP_BIND_TYPE_HOLD, KEYMAP_MODIFIER_NONE_BIT, app, game_on_console_scroll_hold);
    keymap_binding_add(&state->console_keymap, KEY_PAGEDOWN, KEYMAP_BIND_TYPE_HOLD, KEYMAP_MODIFIER_NONE_BIT, app, game_on_console_scroll_hold);
    keymap_binding_add(&state->console_keymap, KEY_UP, KEYMAP_BIND_TYPE_PRESS, KEYMAP_MODIFIER_NONE_BIT, app, game_on_console_history_back);
    keymap_binding_add(&state->console_keymap, KEY_DOWN, KEYMAP_BIND_TYPE_PRESS, KEYMAP_MODIFIER_NONE_BIT, app, game_on_console_history_forward);

// If this was done with the console open, push its keymap
#if BISMUTH_DEBUG
    b8 console_visible = debug_console_visible(&state->debug_console);
    if (console_visible)
        input_keymap_push(&state->console_keymap);
#endif
}

static void remove_keymaps(application* app)
{
    //
}

static void game_on_escape_callback(keys key, keymap_entry_bind_type type, keymap_modifier modifiers, void* user_data)
{
    BDEBUG("game_on_escape_callback");
    event_fire(EVENT_CODE_APPLICATION_QUIT, 0, (event_context){});
}

static void change_current_camera(keys key, keymap_entry_bind_type type, keymap_modifier modifiers, void* user_data)
{
    application* game_inst = (application*)user_data;
    game_state* state = (game_state*)game_inst->state;

    if (state->mode == GAME_MODE_WORLD)
    {
        state->mode = GAME_MODE_EDITOR;
        state->current_camera = state->editor_camera;
        // BTRACE("Editor camera: %f %f %f", state->current_camera->position.x, state->current_camera->euler_rotation.y, state->current_camera->euler_rotation.z);
        if (!input_keymap_pop())
            BERROR("No keymap was popped during world->editor");
        input_keymap_push(&state->editor_keymap);
    }
    else if (state->mode == GAME_MODE_EDITOR)
    {
        state->mode = GAME_MODE_WORLD;
        state->current_camera = state->vehicle_camera;
        // BTRACE("Vehicle camera: %f %f %f", state->current_camera->position.x, state->current_camera->euler_rotation.y, state->current_camera->euler_rotation.z);
        if (!input_keymap_pop())
            BERROR("No keymap was popped during editor->world");
        input_keymap_push(&state->world_keymap);
    }
    else
    {
        BERROR("Stuck in unknown state, changing to vehicle");
        state->mode = GAME_MODE_WORLD;
        state->current_camera = state->vehicle_camera;
        if (!input_keymap_pop())
            BFATAL("No keymap was popped during unknown->world");
        input_keymap_push(&state->world_keymap);
    }
}

static void game_on_yaw(keys key, keymap_entry_bind_type type, keymap_modifier modifiers, void* user_data)
{
    application* game_inst = (application*)user_data;
    game_state* state = (game_state*)game_inst->state;

    f32 f = 0.0f;
    if (key == KEY_LEFT || key == KEY_A)
    {
        f = 1.0f;
    }
    else if (key == KEY_RIGHT || key == KEY_D)
    {
        f = -1.0f;
    }
    
    camera_yaw(state->editor_camera, f * get_engine_delta_time());
}

static void game_on_pitch(keys key, keymap_entry_bind_type type, keymap_modifier modifiers, void* user_data)
{
    application* game_inst = (application*)user_data;
    game_state* state = (game_state*)game_inst->state;

    f32 f = 0.0f;
    if (key == KEY_UP)
    {
        f = 1.0f;
    }
    else if (key == KEY_DOWN)
    {
        f = -1.0f;
    }

    camera_pitch(state->editor_camera, f * get_engine_delta_time());
}

static void game_on_move_forward(keys key, keymap_entry_bind_type type, keymap_modifier modifiers, void* user_data)
{
    application* game_inst = (application*)user_data;
    game_state* state = (game_state*)game_inst->state;

    camera_move_forward(state->editor_camera, state->editor_camera_forward_move_speed * get_engine_delta_time());
}

static void game_on_move_backward(keys key, keymap_entry_bind_type type, keymap_modifier modifiers, void* user_data)
{
    application* game_inst = (application*)user_data;
    game_state* state = (game_state*)game_inst->state;

    camera_move_backward(state->editor_camera, state->editor_camera_backward_move_speed * get_engine_delta_time());
}

static void game_on_move_left(keys key, keymap_entry_bind_type type, keymap_modifier modifiers, void* user_data)
{
    application* game_inst = (application*)user_data;
    game_state* state = (game_state*)game_inst->state;

    camera_move_left(state->editor_camera, state->editor_camera_forward_move_speed * get_engine_delta_time());
}

static void game_on_move_right(keys key, keymap_entry_bind_type type, keymap_modifier modifiers, void* user_data)
{
    application* game_inst = (application*)user_data;
    game_state* state = (game_state*)game_inst->state;

    camera_move_right(state->editor_camera, state->editor_camera_forward_move_speed * get_engine_delta_time());
}

static void game_on_move_up(keys key, keymap_entry_bind_type type, keymap_modifier modifiers, void* user_data)
{
    application* game_inst = (application*)user_data;
    game_state* state = (game_state*)game_inst->state;

    camera_move_up(state->editor_camera, state->editor_camera_forward_move_speed * get_engine_delta_time());
}

static void game_on_move_down(keys key, keymap_entry_bind_type type, keymap_modifier modifiers, void* user_data)
{
    application* game_inst = (application*)user_data;
    game_state* state = (game_state*)game_inst->state;

    camera_move_down(state->editor_camera, state->editor_camera_forward_move_speed * get_engine_delta_time());
}

static void game_on_console_change_visibility(keys key, keymap_entry_bind_type type, keymap_modifier modifiers, void* user_data)
{
#if BISMUTH_DEBUG
    application* game_inst = (application*)user_data;
    game_state* state = (game_state*)game_inst->state;

    b8 console_visible = debug_console_visible(&state->debug_console);
    console_visible = !console_visible;

    debug_console_visible_set(&state->debug_console, console_visible);
    if (console_visible)
        input_keymap_push(&state->console_keymap);
    else
        input_keymap_pop();
#endif
}

static void game_on_set_render_mode_default(keys key, keymap_entry_bind_type type, keymap_modifier modifiers, void* user_data)
{
    application* game_inst = (application*)user_data;
    game_state* state = (game_state*)game_inst->state;
    state->render_mode = RENDERER_VIEW_MODE_DEFAULT;
}

static void game_on_set_render_mode_lighting(keys key, keymap_entry_bind_type type, keymap_modifier modifiers, void* user_data)
{
    application* game_inst = (application*)user_data;
    game_state* state = (game_state*)game_inst->state;
    state->render_mode = RENDERER_VIEW_MODE_LIGHTING;
}

static void game_on_set_render_mode_normals(keys key, keymap_entry_bind_type type, keymap_modifier modifiers, void* user_data)
{
    application* game_inst = (application*)user_data;
    game_state* state = (game_state*)game_inst->state;
    state->render_mode = RENDERER_VIEW_MODE_NORMALS;
}

static void game_on_set_render_mode_cascades(keys key, keymap_entry_bind_type type, keymap_modifier modifiers, void* user_data)
{
    application* game_inst = (application*)user_data;
    game_state* state = (game_state*)game_inst->state;
    state->render_mode = RENDERER_VIEW_MODE_CASCADES;
}

static void game_on_set_render_mode_wireframe(keys key, keymap_entry_bind_type type, keymap_modifier modifiers, void* user_data)
{
    application* game_inst = (application*)user_data;
    game_state* state = (game_state*)game_inst->state;
    state->render_mode = RENDERER_VIEW_MODE_WIREFRAME;
}

static void game_on_set_gizmo_mode(keys key, keymap_entry_bind_type type, keymap_modifier modifiers, void* user_data)
{
    application* game_inst = (application*)user_data;
    game_state* state = (game_state*)game_inst->state;

    editor_gizmo_mode mode;
    switch (key)
    {
    case KEY_1:
    default:
        mode = EDITOR_GIZMO_MODE_NONE;
        break;
    case KEY_2:
        mode = EDITOR_GIZMO_MODE_MOVE;
        break;
    case KEY_3:
        mode = EDITOR_GIZMO_MODE_ROTATE;
        break;
    case KEY_4:
        mode = EDITOR_GIZMO_MODE_SCALE;
        break;
    }
    editor_gizmo_mode_set(&state->gizmo, mode);
}

static void game_on_gizmo_orientation_set(keys key, keymap_entry_bind_type type, keymap_modifier modifiers, void* user_data)
{
    application* game_inst = (application*)user_data;
    game_state* state = (game_state*)game_inst->state;

    editor_gizmo_orientation orientation = editor_gizmo_orientation_get(&state->gizmo);
    orientation++;
    if (orientation > EDITOR_GIZMO_ORIENTATION_MAX)
        orientation = 0;

    editor_gizmo_orientation_set(&state->gizmo, orientation);
}

static void game_on_load_scene(keys key, keymap_entry_bind_type type, keymap_modifier modifiers, void* user_data)
{
    application* game_inst = (application*)user_data;
    game_state* state = (game_state*)game_inst->state;
    if (state->track_scene.state == SCENE_STATE_UNINITIALIZED)
    {
        BDEBUG("Loading track scene...");

        bresource_scene_request_info request_info = {0};
        request_info.base.type = BRESOURCE_TYPE_SCENE;
        request_info.base.synchronous = true; // HACK: use a callback instead
        request_info.base.assets = array_bresource_asset_info_create(1);
        bresource_asset_info* asset = &request_info.base.assets.data[0];
        asset->type = BASSET_TYPE_SCENE;
        asset->asset_name = bname_create("track_00");
        asset->package_name = bname_create("VoidPulse");

        bresource_scene* scene_resource = (bresource_scene*)bresource_system_request(engine_systems_get()->bresource_state, bname_create("test_scene"), (bresource_request_info*)&request_info);
        if (!scene_resource)
        {
            BERROR("Failed to request track scene resource. See logs for details");
            return;
        }

        // Create the scene
        scene_flags scene_load_flags = 0;
        /* scene_load_flags |= SCENE_FLAG_READONLY;  // NOTE: to enable "editor mode", turn this flag off */
        if (!scene_create(scene_resource, scene_load_flags, &state->track_scene))
        {
            BERROR("Failed to create track scene");
            return;
        }

        // HACK: Track configuration
        track_config cfg = {0};
        cfg.loops = true;
        cfg.point_count = 10;
        cfg.points = BALLOC_TYPE_CARRAY(track_point_config, cfg.point_count);
        cfg.segment_resolution = 10;
        cfg.points[0] = (track_point_config){.position = {-10.0f, -0.5f, 0.0f}, .rotation_y = 0.0f, .left = {10.0f, 0.0f, 3.0f, 7.0f}, .right = {12.0f, 0.25f, 3.0f, 1.0f}};
        cfg.points[1] = (track_point_config){.position = {10.0f, 2.0f, 0.0f}, .rotation_y = 45.0f, .left = {8.0f, 0.25f, 0.0f, 5.0f}, .right = {3.0f, 0.5f, 0.0f, 2.0f}};
        cfg.points[2] = (track_point_config){.position = {50.0f, 5.0f, 100.0f}, .rotation_y = 90.0f, .left = {9.0f, -0.5f, 0.0f, 3.0f}, .right = {6.0f, 1.0f, 0.0f, 2.0f}};
        cfg.points[3] = (track_point_config){.position = {75.0f, 6.0f, 200.0f}, .rotation_y = 135.0f, .left = {6.0f, 1.0f, 0.0f, 2.0f}, .right = {10.0f, 1.5f, 0.0f, 2.0f}};
        cfg.points[4] = (track_point_config){.position = {20.0f, 6.0f, 230.0f}, .rotation_y = 180.0f, .left = {5.0f, 1.0f, 0.0f, 2.0f}, .right = {15.0f, 1.5f, 0.0f, 2.0f}};
        cfg.points[5] = (track_point_config){.position = {-50.0f, 5.0f, 200.0f}, .rotation_y = 270.0f, .left = {4.0f, 1.0f, 0.0f, 2.0f}, .right = {15.0f, 1.5f, 0.0f, 2.0f}};
        cfg.points[6] = (track_point_config){.position = {-50.0f, 10.0f, 159.0f}, .rotation_y = 270.0f, .left = {4.0f, 1.0f, 0.0f, 2.0f}, .right = {15.0f, 1.5f, 0.0f, 2.0f}};
        cfg.points[7] = (track_point_config){.position = {-50.0f, -1.0f, 158.0f}, .rotation_y = 270.0f, .left = {4.0f, 1.0f, 0.0f, 11.0f}, .right = {15.0f, 1.5f, 0.0f, 11.0f}};
        cfg.points[8] = (track_point_config){.position = {-50.0f, 2.0f, 100.0f}, .rotation_y = 270.0f, .left = {8.0f, 1.0f, 0.0f, 2.0f}, .right = {8.0f, 1.5f, 0.0f, 2.0f}};
        cfg.points[9] = (track_point_config){.position = {-25.0f, 2.0f, 10.0f}, .rotation_y = 270.0f, .left = {8.0f, 1.0f, 0.0f, 2.0f}, .right = {8.0f, 1.5f, 0.0f, 2.0f}};

        // HACK: create track
        if (!track_create(&state->collision_track, &cfg))
        {
            BERROR("Failed to create collision track");
            return;
        }

        // Initialize
        if (!scene_initialize(&state->track_scene))
        {
            BERROR("Failed initialize track scene, aborting game");
            return;
        }

        // HACK: initialize track
        if (!track_initialize(&state->collision_track))
        {
            BERROR("Failed to initialize collision track");
            return;
        }

        if (!scene_node_xform_get_by_name(&state->track_scene, bname_create("test_vehicle"), &state->test_vehicle_xform))
        {
            BERROR("Unable to get test vehicle");
        }

        if (!scene_node_xform_get_by_name(&state->track_scene, bname_create("vehicle_mesh"), &state->test_vehicle_mesh_xform))
        {
            BERROR("Unable to get test vehicle mesh");
        }

        // Actually load the scene
        if (!scene_load(&state->track_scene))
        {
            BERROR("Error loading track scene");
        }

        // Physics objects must be obtained after load
        bphysics_world* scene_physics_world = scene_physics_world_get(&state->track_scene);
        if (!scene_physics_body_get_by_name(&state->track_scene, bname_create("test_vehicle_physics_body"), &state->test_vehicle_physics_body))
            BERROR("Unable to get test vehicle physics body");

        // HACK: load track
        if (!track_load(&state->collision_track, scene_physics_world))
        {
            BERROR("Failed to load collision track");
            return;
        }
    }
}

static void game_on_save_scene(keys key, keymap_entry_bind_type type, keymap_modifier modifiers, void* user_data)
{
    application* game_inst = (application*)user_data;
    game_state* state = (game_state*)game_inst->state;
    if (state->track_scene.state == SCENE_STATE_LOADED)
    {
        BDEBUG("Saving track scene...");
        if (!scene_save(&state->track_scene))
            BERROR("Error saving track scene");
    }
}

static void game_on_unload_scene(keys key, keymap_entry_bind_type type, keymap_modifier modifiers, void* user_data)
{
    application* game_inst = (application*)user_data;
    game_state* state = (game_state*)game_inst->state;
    if (state->track_scene.state == SCENE_STATE_LOADED)
    {
        BDEBUG("Unloading track scene...");
        scene_unload(&state->track_scene, false);
        /* clear_debug_objects(game_inst); */
    }
}

static void game_on_play_sound(keys key, keymap_entry_bind_type type, keymap_modifier modifiers, void* user_data)
{
    event_fire(EVENT_CODE_DEBUG3, (application*)user_data, (event_context){});
}

static void game_on_toggle_sound(keys key, keymap_entry_bind_type type, keymap_modifier modifiers, void* user_data)
{
    event_fire(EVENT_CODE_DEBUG4, (application*)user_data, (event_context){});
}

static void game_on_console_scroll(keys key, keymap_entry_bind_type type, keymap_modifier modifiers, void* user_data)
{
#if BISMUTH_DEBUG
    application* app = (application*)user_data;
    game_state* state = (game_state*)app->state;
    debug_console_state* console_state = &state->debug_console;

    if (key == KEY_PAGEUP)
        debug_console_move_up(console_state);
    else if (key == KEY_PAGEDOWN)
        debug_console_move_down(console_state);
#endif
}

static void game_on_console_scroll_hold(keys key, keymap_entry_bind_type type, keymap_modifier modifiers, void* user_data)
{
#if BISMUTH_DEBUG
    application* app = (application*)user_data;
    game_state* state = (game_state*)app->state;
    debug_console_state* console_state = &state->debug_console;

    static f32 accumulated_time = 0.0f;
    accumulated_time += get_engine_delta_time();

    if (accumulated_time >= 0.1f)
    {
        if (key == KEY_PAGEUP)
            debug_console_move_up(console_state);
        else if (key == KEY_PAGEDOWN)
            debug_console_move_down(console_state);
        accumulated_time = 0.0f;
    }
#endif
}

static void game_on_console_history_back(keys key, keymap_entry_bind_type type, keymap_modifier modifiers, void* user_data)
{
#if BISMUTH_DEBUG
    application* game_inst = (application*)user_data;
    game_state* state = (game_state*)game_inst->state;
    debug_console_history_back(&state->debug_console);
#endif
}

static void game_on_console_history_forward(keys key, keymap_entry_bind_type type, keymap_modifier modifiers, void* user_data)
{
#if BISMUTH_DEBUG
    application* game_inst = (application*)user_data;
    game_state* state = (game_state*)game_inst->state;
    debug_console_history_forward(&state->debug_console);
#endif
}

static void game_on_debug_vsync_toggle(keys key, keymap_entry_bind_type type, keymap_modifier modifiers, void* user_data)
{
    char cmd[30];
    string_ncopy(cmd, "bvar_set_int vsync 0", 29);
    b8 vsync_enabled = renderer_flag_enabled_get(RENDERER_CONFIG_FLAG_VSYNC_ENABLED_BIT);
    u32 length = string_length(cmd);
    cmd[length - 1] = vsync_enabled ? '1' : '0';
    console_command_execute(cmd);
}

static f32 get_engine_delta_time(void)
{
    bhandle engine = timeline_system_get_engine();
    return timeline_system_delta_get(engine);
}

static f32 get_engine_total_time(void)
{
    bhandle engine = timeline_system_get_engine();
    return timeline_system_total_get(engine);
}
