#include "testbed_main.h"

#include <containers/darray.h>
#include <core/clock.h>
#include <core/console.h>
#include <core/event.h>
#include <core/frame_data.h>
#include <core/input.h>
#include <core/bmemory.h>
#include <core/bstring.h>
#include <core/logger.h>
#include <core/metrics.h>
#include <math/geometry_2d.h>
#include <math/geometry_3d.h>
#include <math/bmath.h>
#include <renderer/camera.h>
#include <renderer/renderer_frontend.h>
#include <renderer/renderer_types.h>
#include <resources/terrain.h>

#include "core/engine.h"
#include "defines.h"
#include "game_state.h"
#include "math/math_types.h"
#include "renderer/viewport.h"
#include "resources/loaders/simple_scene_loader.h"
#include "systems/camera_system.h"
#include "testbed_types.h"

// Standard UI
#include <controls/sui_button.h>
#include <controls/sui_label.h>
#include <controls/sui_panel.h>
#include <passes/ui_pass.h>
#include <standard_ui_system.h>

// Rendergraph and passes
#include "passes/editor_pass.h"
#include "passes/scene_pass.h"
#include "passes/skybox_pass.h"
#include "renderer/rendergraph.h"

// Views
// #include "editor/render_view_wireframe.h"
// #include "views/render_view_pick.h"
// #include "views/render_view_ui.h"
// #include "views/render_view_world.h"

// TODO: Editor temp
#include <resources/debug/debug_box3d.h>
#include <resources/debug/debug_line3d.h>

#include "editor/editor_gizmo.h"
// #include "editor/render_view_editor_world.h"

// TODO: temp
#include <core/identifier.h>
#include <math/transform.h>
#include <resources/loaders/audio_loader.h>
#include <resources/mesh.h>
#include <resources/simple_scene.h>
#include <resources/skybox.h>
#include <systems/audio_system.h>
#include <systems/geometry_system.h>
#include <systems/light_system.h>
#include <systems/material_system.h>
#include <systems/resource_system.h>
// Standard ui
#include <core/systems_manager.h>
#include <standard_ui_system.h>

#include "debug_console.h"
// Game code
#include "game_commands.h"
#include "game_keybinds.h"
// TODO: end temp

typedef struct geometry_distance
{
    geometry_render_data g;
    f32 distance;
} geometry_distance;

b8 configure_render_views(application_config* config);
void application_register_events(struct application* game_inst);
void application_unregister_events(struct application* game_inst);
static b8 load_main_scene(struct application* game_inst);
static b8 configure_rendergraph(application* app);

static void clear_debug_objects(struct application* game_inst)
{
    testbed_game_state* state = (testbed_game_state*)game_inst->state;

    if (state->test_boxes)
    {
        u32 box_count = darray_length(state->test_boxes);
        for (u32 i = 0; i < box_count; ++i)
        {
            debug_box3d* box = &state->test_boxes[i];
            debug_box3d_unload(box);
            debug_box3d_destroy(box);
        }
        darray_clear(state->test_boxes);
    }

    if (state->test_lines)
    {
        u32 line_count = darray_length(state->test_lines);
        for (u32 i = 0; i < line_count; ++i)
        {
            debug_line3d* line = &state->test_lines[i];
            debug_line3d_unload(line);
            debug_line3d_destroy(line);
        }
        darray_clear(state->test_lines);
    }
}

b8 game_on_event(u16 code, void* sender, void* listener_inst, event_context context)
{
    application* game_inst = (application*)listener_inst;
    testbed_game_state* state = (testbed_game_state*)game_inst->state;

    switch (code)
    {
        case EVENT_CODE_OBJECT_HOVER_ID_CHANGED:
        {
            state->hovered_object_id = context.data.u32[0];
            return true;
        }
        case EVENT_CODE_SET_RENDER_MODE:
        {
            i32 mode = context.data.i32[0];
            switch (mode)
            {
                default:
                case RENDERER_VIEW_MODE_DEFAULT:
                    BDEBUG("Renderer mode set to default");
                    state->render_mode = RENDERER_VIEW_MODE_DEFAULT;
                    break;
                case RENDERER_VIEW_MODE_LIGHTING:
                    BDEBUG("Renderer mode set to lighting");
                    state->render_mode = RENDERER_VIEW_MODE_LIGHTING;
                    break;
                case RENDERER_VIEW_MODE_NORMALS:
                    BDEBUG("Renderer mode set to normals");
                    state->render_mode = RENDERER_VIEW_MODE_NORMALS;
                    break;
            }
            return true;
        }
    }

    return false;
}

b8 game_on_debug_event(u16 code, void* sender, void* listener_inst, event_context data)
{
    application* game_inst = (application*)listener_inst;
    testbed_game_state* state = (testbed_game_state*)game_inst->state;

    if (code == EVENT_CODE_DEBUG0)
    {
        const char* names[3] =
        {
            "rocks",
            "stone",
            "wood"
        };
        static i8 choice = 2;

        // Save old names
        const char* old_name = names[choice];

        choice++;
        choice %= 3;

        // Swap out material on the first mesh if it exists
        geometry* g = state->meshes[0].geometries[0];
        if (g)
        {
            // Acquire new material
            g->material = material_system_acquire(names[choice]);
            if (!g->material)
            {
                BWARN("event_on_debug_event no material found! Using default material");
                g->material = material_system_get_default();
            }

            // Release old diffuse material
            material_system_release(old_name);
        }
        return true;
    }
    else if (code == EVENT_CODE_DEBUG1)
    {
        if (state->main_scene.state < SIMPLE_SCENE_STATE_LOADING)
        {
            BDEBUG("Loading main scene...");
            if (!load_main_scene(game_inst))
                BERROR("Error loading main scene");
        }
        return true;
    }
    else if (code == EVENT_CODE_DEBUG2)
    {
        if (state->main_scene.state == SIMPLE_SCENE_STATE_LOADED)
        {
            BDEBUG("Unloading scene...");
            simple_scene_unload(&state->main_scene, false);
            clear_debug_objects(game_inst);
            BDEBUG("Done");
        }
        return true;
    }
    else if (code == EVENT_CODE_DEBUG3)
    {
        if (state->test_audio_file)
        {
            // Cycle between first 5 channels
            static i8 channel_id = -1;
            channel_id++;
            channel_id = channel_id % 5;
            BTRACE("Playing sound on channel %u", channel_id);
            audio_system_channel_play(channel_id, state->test_audio_file, false);
        }
    }
    else if (code == EVENT_CODE_DEBUG4)
    {
        if (state->test_loop_audio_file)
        {
            static b8 playing = true;
            playing = !playing;
            if (playing)
            {
                // Play on channel 6
                if (!audio_system_channel_emitter_play(6, &state->test_emitter))
                    BERROR("Failed to play test emitter");
            }
            else
            {
                // Stop channel 6
                audio_system_channel_stop(6);
            }
        }
    }

    return false;
}

b8 game_on_key(u16 code, void* sender, void* listener_inst, event_context context)
{
    application* game_inst = (application*)listener_inst;
    testbed_game_state* state = (testbed_game_state*)game_inst->state;
    if (code == EVENT_CODE_KEY_RELEASED)
    {
        u16 key_code = context.data.u16[0];
        // Change gizmo orientation
        if (key_code == KEY_G)
        {
            editor_gizmo_orientation orientation = editor_gizmo_orientation_get(&state->gizmo);
            orientation++;
            if (orientation > EDITOR_GIZMO_ORIENTATION_MAX)
                orientation = 0;
            editor_gizmo_orientation_set(&state->gizmo, orientation);
        }
    }
    return false;
}

static b8 game_on_drag(u16 code, void* sender, void* listener_inst, event_context context)
{
    i16 x = context.data.i16[0];
    i16 y = context.data.i16[1];
    u16 drag_button = context.data.u16[2];
    testbed_game_state* state = (testbed_game_state*)listener_inst;

    // Left button drags
    if (drag_button == BUTTON_LEFT)
    {
        mat4 view = camera_view_get(state->world_camera);
        vec3 origin = camera_position_get(state->world_camera);

        viewport* v = &state->world_viewport;
        ray r = ray_from_screen(
            vec2_create((f32)x, (f32)y),
            (v->rect),
            origin,
            view,
            v->projection);

        if (code == EVENT_CODE_MOUSE_DRAG_BEGIN)
        {
            state->using_gizmo = true;
            // Drag start -- change interaction mode to "dragging"
            editor_gizmo_interaction_begin(&state->gizmo, state->world_camera, &r, EDITOR_GIZMO_INTERACTION_TYPE_MOUSE_DRAG);
        }
        else if (code == EVENT_CODE_MOUSE_DRAGGED)
        {
            editor_gizmo_handle_interaction(&state->gizmo, state->world_camera, &r, EDITOR_GIZMO_INTERACTION_TYPE_MOUSE_DRAG);
        }
        else if (code == EVENT_CODE_MOUSE_DRAG_END)
        {
            editor_gizmo_interaction_end(&state->gizmo);
            state->using_gizmo = false;
        }
    }

    return false; // Let other handlers handle
}

b8 game_on_button(u16 code, void* sender, void* listener_inst, event_context context)
{
    if (code == EVENT_CODE_BUTTON_PRESSED)
    {
        // ...
    }
    else if (code == EVENT_CODE_BUTTON_RELEASED)
    {
        u16 button = context.data.u16[0];
        switch (button)
        {
            case BUTTON_LEFT:
            {
                i16 x = context.data.i16[1];
                i16 y = context.data.i16[2];
                testbed_game_state* state = (testbed_game_state*)listener_inst;

                // If scene isn't loaded, don't do anything else
                if (state->main_scene.state < SIMPLE_SCENE_STATE_LOADED)
                    return false;
                
                // If "manipulating gizmo", don't do below logic
                if (state->using_gizmo)
                    return false;

                mat4 view = camera_view_get(state->world_camera);
                vec3 origin = camera_position_get(state->world_camera);

                viewport* v = &state->world_viewport;
                // Only allow this action in the "primary" viewport
                if (point_in_rect_2d((vec2){(f32)x, (f32)y}, v->rect))
                {
                    ray r = ray_from_screen(
                        vec2_create((f32)x, (f32)y),
                        (v->rect),
                        origin,
                        view,
                        v->projection);

                    raycast_result r_result;
                    if (simple_scene_raycast(&state->main_scene, &r, &r_result))
                    {
                        u32 hit_count = darray_length(r_result.hits);
                        for (u32 i = 0; i < hit_count; ++i)
                        {
                            raycast_hit* hit = &r_result.hits[i];
                            BINFO("Hit! id: %u, dist: %f", hit->unique_id, hit->distance);

                            // Create a debug line where the ray cast starts and ends (at the intersection)
                            debug_line3d test_line;
                            debug_line3d_create(r.origin, hit->position, 0, &test_line);
                            debug_line3d_initialize(&test_line);
                            debug_line3d_load(&test_line);
                            // Yellow for hits
                            debug_line3d_color_set(&test_line, (vec4){1.0f, 1.0f, 0.0f, 1.0f});

                            darray_push(state->test_lines, test_line);

                            // Create a debug box to show the intersection point
                            debug_box3d test_box;

                            debug_box3d_create((vec3){0.1f, 0.1f, 0.1f}, 0, &test_box);
                            debug_box3d_initialize(&test_box);
                            debug_box3d_load(&test_box);

                            extents_3d ext;
                            ext.min = vec3_create(hit->position.x - 0.05f, hit->position.y - 0.05f, hit->position.z - 0.05f);
                            ext.max = vec3_create(hit->position.x + 0.05f, hit->position.y + 0.05f, hit->position.z + 0.05f);
                            debug_box3d_extents_set(&test_box, ext);

                            darray_push(state->test_boxes, test_box);

                            // Object selection
                            if (i == 0)
                            {
                                state->selection.unique_id = hit->unique_id;
                                state->selection.xform = simple_scene_transform_get_by_id(&state->main_scene, hit->unique_id);
                                if (state->selection.xform)
                                {
                                    BINFO("Selected object id %u", hit->unique_id);
                                    editor_gizmo_selected_transform_set(&state->gizmo, state->selection.xform);
                                }
                            }
                        }
                    }
                    else
                    {
                        BINFO("No hit");

                        // Create a debug line where the ray cast starts and continues to
                        debug_line3d test_line;
                        debug_line3d_create(r.origin, vec3_add(r.origin, vec3_mul_scalar(r.direction, 100.0f)), 0, &test_line);
                        debug_line3d_initialize(&test_line);
                        debug_line3d_load(&test_line);
                        // Magenta for non-hits
                        debug_line3d_color_set(&test_line, (vec4){1.0f, 0.0f, 1.0f, 1.0f});

                        darray_push(state->test_lines, test_line);

                        if (state->selection.xform)
                        {
                            BINFO("Object deselected");
                            state->selection.xform = 0;
                            state->selection.unique_id = INVALID_ID;

                            editor_gizmo_selected_transform_set(&state->gizmo, 0);
                        }

                        // TODO: hide gizmo, disable input, etc
                    }
                }

            } break;
        }
    }

    return false;
}

static b8 game_on_mouse_move(u16 code, void* sender, void* listener_inst, event_context context)
{
    if (code == EVENT_CODE_MOUSE_MOVED && !input_is_button_dragging(BUTTON_LEFT))
    {
        i16 x = context.data.i16[0];
        i16 y = context.data.i16[1];

        testbed_game_state* state = (testbed_game_state*)listener_inst;

        mat4 view = camera_view_get(state->world_camera);
        vec3 origin = camera_position_get(state->world_camera);

        viewport* v = &state->world_viewport;
        ray r = ray_from_screen(
            vec2_create((f32)x, (f32)y),
            (v->rect),
            origin,
            view,
            v->projection);

        editor_gizmo_handle_interaction(&state->gizmo, state->world_camera, &r, EDITOR_GIZMO_INTERACTION_TYPE_MOUSE_HOVER);
    }
    return false; // Allow other event handlers to recieve this event
}

static void sui_test_button_on_click(struct sui_control* self, struct sui_mouse_event event)
{
    if (self)
        BDEBUG("Clicked '%s'!", self->name);
}

u64 application_state_size(void)
{
    return sizeof(testbed_game_state);
}

b8 application_boot(struct application* game_inst)
{
    BINFO("Booting sandbox...");

    // Allocate game state
    game_inst->state = ballocate(sizeof(testbed_game_state), MEMORY_TAG_GAME);
    ((testbed_game_state*)game_inst->state)->running = false;

    debug_console_create(&((testbed_game_state*)game_inst->state)->debug_console);

    application_config* config = &game_inst->app_config;

    config->frame_allocator_size = MEBIBYTES(64);
    config->app_frame_data_size = sizeof(testbed_application_frame_data);

    // Configure fonts
    config->font_config.auto_release = false;
    config->font_config.default_bitmap_font_count = 1;

    bitmap_font_config bmp_font_config = {};
    // OpenSans21px NotoSans
    bmp_font_config.name = "Open Sans 21px";
    bmp_font_config.resource_name = "OpenSans21px";
    bmp_font_config.size = 21;
    config->font_config.bitmap_font_configs = ballocate(sizeof(bitmap_font_config) * 1, MEMORY_TAG_ARRAY);
    config->font_config.bitmap_font_configs[0] = bmp_font_config;

    system_font_config sys_font_config;
    sys_font_config.default_size = 20;
    sys_font_config.name = "Noto Sans";
    sys_font_config.resource_name = "NotoSansCJK";

    config->font_config.default_system_font_count = 1;
    config->font_config.system_font_configs = ballocate(sizeof(sys_font_config) * 1, MEMORY_TAG_ARRAY);
    config->font_config.system_font_configs[0] = sys_font_config;

    config->font_config.max_bitmap_font_count = 101;
    config->font_config.max_system_font_count = 101;

    // Configure render views
    // if (!configure_render_views(config))
    // {
    //     BERROR("Failed to configure renderer views. Aborting application...");
    //     return false;
    // }
    if (!configure_rendergraph(game_inst))
    {
        BERROR("Failed to setup render graph. Aboring application...");
        return false;
    }

    // Keymaps
    game_setup_keymaps(game_inst);
    // Console commands
    game_setup_commands(game_inst);

    return true;
}

b8 application_initialize(struct application* game_inst)
{
    BDEBUG("application_initialize() called!");

    systems_manager_state* sys_mgr_state = engine_systems_manager_state_get(game_inst);
    standard_ui_system_config standard_ui_cfg = {0};
    standard_ui_cfg.max_control_count = 1024;
    if (!systems_manager_register(sys_mgr_state, B_SYSTEM_TYPE_STANDARD_UI_EXT, standard_ui_system_initialize, standard_ui_system_shutdown, standard_ui_system_update, &standard_ui_cfg))
    {
        BERROR("Failed to register standard ui system");
        return false;
    }

    application_register_events(game_inst);

    // Register resource loaders
    resource_system_loader_register(simple_scene_resource_loader_create());
    resource_system_loader_register(audio_resource_loader_create());

    testbed_game_state* state = ((testbed_game_state*)game_inst->state);
    state->selection.unique_id = INVALID_ID;
    state->selection.xform = 0;

    debug_console_load(&state->debug_console);

    state->test_lines = darray_create(debug_line3d);
    state->test_boxes = darray_create(debug_box3d);

    // Viewport setup
    // World Viewport
    rect_2d world_vp_rect = vec4_create(20.0f, 20.0f, 1280.0f - 40.0f, 720.0f - 40.0f);
    if (!viewport_create(world_vp_rect, deg_to_rad(45.0f), 0.1f, 4000.0f, RENDERER_PROJECTION_MATRIX_TYPE_PERSPECTIVE, &state->world_viewport))
    {
        BERROR("Failed to create world viewport. Cannot start application");
        return false;
    }

    // UI Viewport
    rect_2d ui_vp_rect = vec4_create(0.0f, 0.0f, 1280.0f, 720.0f);
    if (!viewport_create(ui_vp_rect, 0.0f, -100.0f, 100.0f, RENDERER_PROJECTION_MATRIX_TYPE_ORTHOGRAPHIC, &state->ui_viewport))
    {
        BERROR("Failed to create UI viewport. Cannot start application");
        return false;
    }

    // TODO: For test
    rect_2d world_vp_rect2 = vec4_create(20.0f, 20.0f, 128.8f, 72.0f);
    if (!viewport_create(world_vp_rect2, 0.015f, -4000.0f, 4000.0f, RENDERER_PROJECTION_MATRIX_TYPE_ORTHOGRAPHIC_CENTERED, &state->world_viewport2))
    {
        BERROR("Failed to create wireframe viewport. Cannot start application");
        return false;
    }

    state->forward_move_speed = 5.0f;
    state->backward_move_speed = 2.5f;

    // Setup editor gizmo
    if (!editor_gizmo_create(&state->gizmo))
    {
        BERROR("Failed to create editor gizmo");
        return false;
    }
    if (!editor_gizmo_initialize(&state->gizmo))
    {
        BERROR("Failed to initialize editor gizmo");
        return false;
    }
    if (!editor_gizmo_load(&state->gizmo))
    {
        BERROR("Failed to load editor gizmo");
        return false;
    }

    // World meshes
    // Invalidate all meshes
    for (u32 i = 0; i < 10; ++i)
    {
        state->meshes[i].generation = INVALID_ID_U8;
        state->ui_meshes[i].generation = INVALID_ID_U8;
    }

    // Create test ui text objects
    if (!sui_label_control_create("testbed_mono_test_text", FONT_TYPE_BITMAP, "Open Sans 21px", 21, "Some test text 123,\n\thello!", &state->test_text))
    {
        BERROR("Failed to load basic ui bitmap text");
        return false;
    }
    else
    {
        if (!sui_label_control_load(&state->test_text))
        {
            BERROR("Failed to load test text");
        }
        else
        {
            void* sui_state = systems_manager_get_state(B_SYSTEM_TYPE_STANDARD_UI_EXT);
            if (!standard_ui_system_register_control(sui_state, &state->test_text))
            {
                BERROR("Unable to register control");
            }
            else
            {
                if (!standard_ui_system_control_add_child(sui_state, 0, &state->test_text))
                {
                    BERROR("Failed to parent test text");
                }
                else
                {
                    state->test_text.is_active = true;
                    if (!standard_ui_system_update_active(sui_state, &state->test_text))
                        BERROR("Unable to update active state");
                }
            }
        }
    }
    // Move debug text to new bottom of screen
    sui_control_position_set(&state->test_text, vec3_create(20, game_inst->app_config.start_height - 75, 0));

    // Standard ui
    if (!sui_panel_control_create("test_panel", (vec2){300.0f, 300.0f}, (vec4){0.0f, 0.0f, 0.0f, 0.5f}, &state->test_panel))
    {
        BERROR("Failed to create test panel");
    }
    else
    {
        if (!sui_panel_control_load(&state->test_panel))
        {
            BERROR("Failed to load test panel");
        }
        else
        {
            transform_translate(&state->test_panel.xform, (vec3){950, 350});
            void* sui_state = systems_manager_get_state(B_SYSTEM_TYPE_STANDARD_UI_EXT);
            if (!standard_ui_system_register_control(sui_state, &state->test_panel))
            {
                BERROR("Unable to register control");
            }
            else
            {
                if (!standard_ui_system_control_add_child(sui_state, 0, &state->test_panel))
                {
                    BERROR("Failed to parent test panel");
                }
                else
                {
                    state->test_panel.is_active = true;
                    if (!standard_ui_system_update_active(sui_state, &state->test_panel))
                        BERROR("Unable to update active state");
                }
            }
        }
    }

    if (!sui_button_control_create("test_button", &state->test_button))
    {
        BERROR("Failed to create test button");
    }
    else
    {
        // Assign a click handler
        state->test_button.on_click = sui_test_button_on_click;

        if (!sui_button_control_load(&state->test_button))
        {
            BERROR("Failed to load test button");
        }
        else
        {
            void* sui_state = systems_manager_get_state(B_SYSTEM_TYPE_STANDARD_UI_EXT);
            if (!standard_ui_system_register_control(sui_state, &state->test_button))
            {
                BERROR("Unable to register control");
            }
            else
            {
                if (!standard_ui_system_control_add_child(sui_state, &state->test_panel, &state->test_button))
                {
                    BERROR("Failed to parent test button");
                }
                else
                {
                    state->test_button.is_active = true;
                    if (!standard_ui_system_update_active(sui_state, &state->test_button))
                        BERROR("Unable to update active state");
                }
            }
        }
    }

    if (!sui_label_control_create("testbed_UTF_test_sys_text", FONT_TYPE_SYSTEM, "Noto Sans CJK JP", 31, "Press 'L' to load scene, \n\thello!\n\n\tこんにちは", &state->test_sys_text))
    {
        BERROR("Failed to load basic ui system text");
        return false;
    }
    else
    {
        if (!sui_label_control_load(&state->test_sys_text))
        {
            BERROR("Failed to load test system text");
        }
        else
        {
            void* sui_state = systems_manager_get_state(B_SYSTEM_TYPE_STANDARD_UI_EXT);
            if (!standard_ui_system_register_control(sui_state, &state->test_sys_text))
            {
                BERROR("Unable to register control");
            }
            else
            {
                if (!standard_ui_system_control_add_child(sui_state, 0, &state->test_sys_text))
                {
                    BERROR("Failed to parent test system text");
                }
                else
                {
                    state->test_sys_text.is_active = true;
                    if (!standard_ui_system_update_active(sui_state, &state->test_sys_text))
                        BERROR("Unable to update active state");
                }
            }
        }
    }
    sui_control_position_set(&state->test_sys_text, vec3_create(950, 450, 0));
    // TODO: end temp load/prepare stuff

    state->world_camera = camera_system_acquire("world");
    camera_position_set(state->world_camera, (vec3){-24.5, 19.3f, 30.2f});
    camera_rotation_euler_set(state->world_camera, (vec3){-24.0f, -42.5f, 0.0f});

    // TODO: temp test camera
    state->world_camera_2 = camera_system_acquire("world_2");
    camera_position_set(state->world_camera_2, (vec3){8.0f, 0.0f, 10.0f});
    camera_rotation_euler_set(state->world_camera_2, (vec3){0.0f, -90.0f, 0.0f});

    // bzero_memory(&game_inst->frame_data, sizeof(app_frame_data));

    bzero_memory(&state->update_clock, sizeof(clock));
    bzero_memory(&state->prepare_clock, sizeof(clock));
    bzero_memory(&state->render_clock, sizeof(clock));
    bzero_memory(&state->present_clock, sizeof(clock));

    // Load up a test audio file
    state->test_audio_file = audio_system_chunk_load("Test.ogg");
    if (!state->test_audio_file)
        BERROR("Failed to load test audio file");

    // Looping audio file
    state->test_loop_audio_file = audio_system_chunk_load("Fire Loop.mp3");
    // Test music
    state->test_music = audio_system_stream_load("bg_song1.mp3");
    if (!state->test_music)
        BERROR("Failed to load test music file");

    // Setup a test emitter
    state->test_emitter.file = state->test_loop_audio_file;
    state->test_emitter.volume = 1.0f;
    state->test_emitter.looping = true;
    state->test_emitter.falloff = 1.0f;
    state->test_emitter.position = vec3_create(10.0f, 0.8f, 20.0f);

    // Set some channel volumes
    audio_system_master_volume_set(0.7f);
    audio_system_channel_volume_set(0, 1.0f);
    audio_system_channel_volume_set(1, 0.75f);
    audio_system_channel_volume_set(2, 0.50f);
    audio_system_channel_volume_set(3, 0.25);
    audio_system_channel_volume_set(4, 0.05f);

    audio_system_channel_volume_set(7, 0.6f);

    // Try playing the emitter
    // if (!audio_system_channel_emitter_play(6, &state->test_emitter))
    //     BERROR("Failed to play test emitter");

    // audio_system_channel_play(7, state->test_music, true);

    state->running = true;

    return true;
}

b8 application_update(struct application* game_inst, struct frame_data* p_frame_data)
{
    testbed_application_frame_data* app_frame_data = (testbed_application_frame_data*)p_frame_data->application_frame_data;
    if (!app_frame_data)
        return true;

    testbed_game_state* state = (testbed_game_state*)game_inst->state;
    if (!state->running)
        return true;
    
    clock_start(&state->update_clock);

    // TODO: testing resize
    static f32 button_height = 50.0f;
    button_height = 50.0f + (bsin(p_frame_data->total_time) * 20.0f);
    sui_button_control_height_set(&state->test_button, (i32)button_height);

    if (state->main_scene.state >= SIMPLE_SCENE_STATE_LOADED)
    {
        if (!simple_scene_update(&state->main_scene, p_frame_data))
            BWARN("Failed to update main scene");
        
        editor_gizmo_update(&state->gizmo);

        // // Perform small rotation on first mesh
        // quat rotation = quat_from_axis_angle((vec3){0, 1, 0}, -0.5f * p_frame_data->delta_time, false);
        // transform_rotate(&state->meshes[0].transform, rotation);

        // // Perform similar rotation on second mesh, if it exists
        // transform_rotate(&state->meshes[1].transform, rotation);

        // // Perform similar rotation on third mesh, if it exists
        // transform_rotate(&state->meshes[2].transform, rotation);

        if (state->p_light_1)
        {
            state->p_light_1->data.color = (vec4){
                BCLAMP(bsin(p_frame_data->total_time) * 0.75f + 0.5f, 0.0f, 1.0f),
                BCLAMP(bsin(p_frame_data->total_time - (B_2PI / 3)) * 0.75f + 0.5f, 0.0f, 1.0f),
                BCLAMP(bsin(p_frame_data->total_time - (B_4PI / 3)) * 0.75f + 0.5f, 0.0f, 1.0f),
                1.0f
            };
            state->p_light_1->data.position.z = 20.0f + bsin(p_frame_data->total_time);

            // Make audio emitter follow it
            state->test_emitter.position = vec3_from_vec4(state->p_light_1->data.position);
        }
    }

    // Track allocation differences
    state->prev_alloc_count = state->alloc_count;
    state->alloc_count = get_memory_alloc_count();

    // Update bitmap text with camera position
    vec3 pos = camera_position_get(state->world_camera);
    vec3 rot = camera_rotation_euler_get(state->world_camera);

    // Also tack on current mouse state
    b8 left_down = input_is_button_down(BUTTON_LEFT);
    b8 right_down = input_is_button_down(BUTTON_RIGHT);
    i32 mouse_x, mouse_y;
    input_get_mouse_position(&mouse_x, &mouse_y);

    // Convert to NDC(Native Device Coordinates)
    f32 mouse_x_ndc = range_convert_f32((f32)mouse_x, 0.0f, (f32)state->width, -1.0f, 1.0f);
    f32 mouse_y_ndc = range_convert_f32((f32)mouse_y, 0.0f, (f32)state->height, -1.0f, 1.0f);

    f64 fps, frame_time;
    metrics_frame(&fps, &frame_time);

    // Keep a running average of update and render timers over the last ~1 second
    static f64 accumulated_ms = 0;
    static f32 total_update_seconds = 0;
    static f32 total_prepare_seconds = 0;
    static f32 total_render_seconds = 0;
    static f32 total_present_seconds = 0;

    static f32 total_update_avg_us = 0;
    static f32 total_prepare_avg_us = 0;
    static f32 total_render_avg_us = 0;
    static f32 total_present_avg_us = 0;
    static f32 total_avg = 0;  // total average across the frame

    total_update_seconds += state->last_update_elapsed;
    total_prepare_seconds += state->prepare_clock.elapsed;
    total_render_seconds += state->render_clock.elapsed;
    total_present_seconds += state->present_clock.elapsed;
    accumulated_ms += frame_time;

    // Once ~1 second has gone by, calculate average and wipe accumulators
    if (accumulated_ms >= 1000.0f)
    {
        total_update_avg_us = (total_update_seconds / accumulated_ms) * B_SEC_TO_US_MULTIPLIER;
        total_prepare_avg_us = (total_prepare_seconds / accumulated_ms) * B_SEC_TO_US_MULTIPLIER;
        total_render_avg_us = (total_render_seconds / accumulated_ms) * B_SEC_TO_US_MULTIPLIER;
        total_present_avg_us = (total_present_seconds / accumulated_ms) * B_SEC_TO_US_MULTIPLIER;
        total_avg = total_update_avg_us + total_prepare_avg_us + total_render_avg_us + total_present_avg_us;
        total_render_seconds = 0;
        total_prepare_seconds = 0;
        total_update_seconds = 0;
        total_present_seconds = 0;
        accumulated_ms = 0;
    }

    char* vsync_text = renderer_flag_enabled_get(RENDERER_CONFIG_FLAG_VSYNC_ENABLED_BIT) ? "YES" : " NO";
    char text_buffer[2048];
    string_format(
        text_buffer,
        "\
FPS: %5.1f(%4.1fms)        Pos=[%7.3f %7.3f %7.3f] Rot=[%7.3f, %7.3f, %7.3f]\n\
Upd: %8.3fus, Prep: %8.3fus, Rend: %8.3fus, Pres: %8.3fus, Tot: %8.3fus \n\
Mouse: X=%-5d Y=%-5d   L=%s R=%s   NDC: X=%.6f, Y=%.6f\n\
VSync: %s Drawn: %-5u Hovered: %s%u",
        fps,
        frame_time,
        pos.x, pos.y, pos.z,
        rad_to_deg(rot.x), rad_to_deg(rot.y), rad_to_deg(rot.z),
        total_update_avg_us,
        total_prepare_avg_us,
        total_render_avg_us,
        total_present_avg_us,
        total_avg,
        mouse_x, mouse_y,
        left_down ? "Y" : "N",
        right_down ? "Y" : "N",
        mouse_x_ndc,
        mouse_y_ndc,
        vsync_text,
        p_frame_data->drawn_mesh_count,
        state->hovered_object_id == INVALID_ID ? "none" : "",
        state->hovered_object_id == INVALID_ID ? 0 : state->hovered_object_id);
    if (state->running)
        sui_label_text_set(&state->test_text, text_buffer);

    debug_console_update(&((testbed_game_state*)game_inst->state)->debug_console);

    vec3 forward = camera_forward(state->world_camera);
    vec3 up = camera_up(state->world_camera);
    audio_system_listener_orientation_set(pos, forward, up);

    clock_update(&state->update_clock);
    state->last_update_elapsed = state->update_clock.elapsed;

    return true;
}

// Quicksort for geometry_distance
static void gdistance_swap(geometry_distance* a, geometry_distance* b)
{
    geometry_distance temp = *a;
    *a = *b;
    *b = temp;
}

static i32 gdistance_partition(geometry_distance arr[], i32 low_index, i32 high_index, b8 ascending)
{
    geometry_distance pivot = arr[high_index];
    i32 i = (low_index - 1);

    for (i32 j = low_index; j <= high_index - 1; ++j)
    {
        if (ascending)
        {
            if (arr[j].distance < pivot.distance)
            {
                ++i;
                gdistance_swap(&arr[i], &arr[j]);
            }
        }
        else
        {
            if (arr[j].distance > pivot.distance)
            {
                ++i;
                gdistance_swap(&arr[i], &arr[j]);
            }
        }
    }

    gdistance_swap(&arr[i + 1], &arr[high_index]);
    return i + 1;
}

static void gdistance_quick_sort(geometry_distance arr[], i32 low_index, i32 high_index, b8 ascending)
{
    if (low_index < high_index)
    {
        i32 partition_index = gdistance_partition(arr, low_index, high_index, ascending);

        // Independently sort elements before and after partition index
        gdistance_quick_sort(arr, low_index, partition_index - 1, ascending);
        gdistance_quick_sort(arr, partition_index + 1, high_index, ascending);
    }
}

static void geometry_swap(geometry_render_data* a, geometry_render_data* b)
{
    geometry_render_data temp = *a;
    *a = *b;
    *b = temp;
}

static i32 geometry_partition(geometry_render_data arr[], i32 low_index, i32 high_index, b8 ascending)
{
    geometry_render_data pivot = arr[high_index];
    i32 i = (low_index - 1);

    for (i32 j = low_index; j <= high_index - 1; ++j)
    {
        if (ascending)
        {
            if (arr[j].material->internal_id < pivot.material->internal_id)
            {
                ++i;
                geometry_swap(&arr[i], &arr[j]);
            }
        }
        else
        {
            if (arr[j].material->internal_id > pivot.material->internal_id)
            {
                ++i;
                geometry_swap(&arr[i], &arr[j]);
            }
        }
    }
    geometry_swap(&arr[i + 1], &arr[high_index]);
    return i + 1;
}

static void geometry_quick_sort_by_material(geometry_render_data arr[], i32 low_index, i32 high_index, b8 ascending) {
    if (low_index < high_index) {
        i32 partition_index = geometry_partition(arr, low_index, high_index, ascending);

        // Independently sort elements before and after the partition index.
        geometry_quick_sort_by_material(arr, low_index, partition_index - 1, ascending);
        geometry_quick_sort_by_material(arr, partition_index + 1, high_index, ascending);
    }
}

b8 application_prepare_frame(struct application* app_inst, struct frame_data* p_frame_data)
{
    testbed_game_state* state = (testbed_game_state*)app_inst->state;
    if (!state->running)
        return false;

    clock_start(&state->prepare_clock);

    // Skybox pass
    // This pass must always run, as it is what clears the screen
    skybox_pass_extended_data* skybox_pass_ext_data = state->skybox_pass.pass_data.ext_data;
    state->skybox_pass.pass_data.vp = &state->world_viewport;
    camera* current_camera = state->world_camera;
    state->skybox_pass.pass_data.view_matrix = camera_view_get(current_camera);
    state->skybox_pass.pass_data.view_position = camera_position_get(current_camera);
    state->skybox_pass.pass_data.projection_matrix = state->world_viewport.projection;
    state->skybox_pass.pass_data.do_execute = true;
    skybox_pass_ext_data->sb = 0;

    // Tell our scene to generate relevant packet data. NOTE: Generates skybox and world packets
    if (state->main_scene.state == SIMPLE_SCENE_STATE_LOADED)
    {
        {
            skybox_pass_ext_data->sb = state->main_scene.sb;
        }

        {
            // Enable this pass for this frame
            state->scene_pass.pass_data.do_execute = true;
            state->scene_pass.pass_data.vp = &state->world_viewport;
            camera* current_camera = state->world_camera;
            state->scene_pass.pass_data.view_matrix = camera_view_get(current_camera);
            state->scene_pass.pass_data.view_position = camera_position_get(current_camera);
            state->scene_pass.pass_data.projection_matrix = state->world_viewport.projection;

            scene_pass_extended_data* ext_data = state->scene_pass.pass_data.ext_data;
            // TODO: Get from scene
            ext_data->ambient_color = (vec4){0.25f, 0.25f, 0.25f, 1.0f};
            ext_data->render_mode = state->render_mode;

            // Populate scene pass data
            viewport* v = &state->world_viewport;
            simple_scene* scene = &state->main_scene;

            // Update frustum
            vec3 forward = camera_forward(current_camera);
            vec3 right = camera_right(current_camera);
            vec3 up = camera_up(current_camera);
            frustum f = frustum_create(&current_camera->position, &forward, &right, &up, v->rect.width / v->rect.height, v->fov, v->near_clip, v->far_clip);

            p_frame_data->drawn_mesh_count = 0;

            ext_data->geometries = darray_reserve_with_allocator(geometry_render_data, 512, &p_frame_data->allocator);
            geometry_distance* transparent_geometries = darray_create_with_allocator(geometry_distance, &p_frame_data->allocator);

            u32 mesh_count = darray_length(scene->meshes);
            for (u32 i = 0; i < mesh_count; ++i)
            {
                mesh* m = &scene->meshes[i];
                if (m->generation != INVALID_ID_U8)
                {
                    mat4 model = transform_world_get(&m->transform);
                    b8 winding_inverted = m->transform.determinant < 0;

                    for (u32 j = 0; j < m->geometry_count; ++j)
                    {
                        geometry* g = m->geometries[j];
                        // AABB calculation
                        {
                            // Translate/scale extents
                            vec3 extents_max = vec3_mul_mat4(g->extents.max, model);

                            // Translate/scale center
                            vec3 center = vec3_mul_mat4(g->center, model);
                            vec3 half_extents = {
                                babs(extents_max.x - center.x),
                                babs(extents_max.y - center.y),
                                babs(extents_max.z - center.z),
                            };

                            if (frustum_intersects_aabb(&f, &center, &half_extents))
                            {
                                // Add it to the list to be rendered
                                geometry_render_data data = {0};
                                data.model = model;
                                data.material = g->material;
                                data.vertex_count = g->vertex_count;
                                data.vertex_buffer_offset = g->vertex_buffer_offset;
                                data.index_count = g->index_count;
                                data.index_buffer_offset = g->index_buffer_offset;
                                data.unique_id = m->id.uniqueid;
                                data.winding_inverted = winding_inverted;

                                // Check if transparent. If so, put into a separate, temp array to be sorted by distance from the camera
                                b8 has_transparency = false;
                                if (g->material->type == MATERIAL_TYPE_PHONG)
                                {
                                    // Check diffuse map (slot 0)
                                    has_transparency = ((g->material->maps[0].texture->flags & TEXTURE_FLAG_HAS_TRANSPARENCY) != 0);
                                }

                                if (has_transparency)
                                {
                                    // For meshes with transparency, add them to separate list to be sorted by distance later
                                    vec3 center = vec3_transform(g->center, 1.0f, model);
                                    f32 distance = vec3_distance(center, current_camera->position);

                                    geometry_distance gdist;
                                    gdist.distance = babs(distance);
                                    gdist.g = data;
                                    darray_push(transparent_geometries, gdist);
                                }
                                else
                                {
                                    darray_push(ext_data->geometries, data);
                                }
                                p_frame_data->drawn_mesh_count++;
                            }
                        }
                    }
                }
            }

            // Sort opaque geometries by material
            geometry_quick_sort_by_material(ext_data->geometries, 0, darray_length(ext_data->geometries) - 1, true);

            // Sort transparent geometries, then add them to the ext_data->geometries array
            u32 geometry_count = darray_length(transparent_geometries);
            gdistance_quick_sort(transparent_geometries, 0, geometry_count - 1, false);
            for (u32 i = 0; i < geometry_count; ++i)
                darray_push(ext_data->geometries, transparent_geometries[i].g);

            ext_data->geometry_count = darray_length(ext_data->geometries);

            // Add terrain(s)
            u32 terrain_count = darray_length(scene->terrains);
            ext_data->terrain_geometries = darray_reserve_with_allocator(geometry_render_data, 16, &p_frame_data->allocator);
            ext_data->terrain_geometry_count = 0;
            for (u32 i = 0; i < terrain_count; ++i)
            {
                // TODO: Frustum culling
                geometry_render_data data = {0};
                data.model = transform_world_get(&scene->terrains[i].xform);
                geometry* g = &scene->terrains[i].geo;
                data.material = g->material;
                data.vertex_count = g->vertex_count;
                data.vertex_buffer_offset = g->vertex_buffer_offset;
                data.index_count = g->index_count;
                data.index_buffer_offset = g->index_buffer_offset;
                data.unique_id = scene->terrains[i].id.uniqueid;

                darray_push(ext_data->terrain_geometries, data);

                // TODO: Counter for terrain geometries
                p_frame_data->drawn_mesh_count++;
            }
            ext_data->terrain_geometry_count = darray_length(ext_data->terrain_geometries);

            // Debug geometry
            if (!simple_scene_debug_render_data_query(scene, &ext_data->debug_geometry_count, 0))
            {
                BERROR("Failed to obtain count of debug render objects");
                return false;
            }
            ext_data->debug_geometries = darray_reserve_with_allocator(geometry_render_data, ext_data->debug_geometry_count, &p_frame_data->allocator);

            if (!simple_scene_debug_render_data_query(scene, &ext_data->debug_geometry_count, &ext_data->debug_geometries))
            {
                BERROR("Failed to obtain debug render objects");
                return false;
            }
            // Make sure the count is correct before pushing
            darray_length_set(ext_data->debug_geometries, ext_data->debug_geometry_count);

            // HACK: Inject raycast debug geometries into scene pass data
            if (state->main_scene.state == SIMPLE_SCENE_STATE_LOADED)
            {
                u32 line_count = darray_length(state->test_lines);
                for (u32 i = 0; i < line_count; ++i)
                {
                    geometry_render_data rd = {0};
                    rd.model = transform_world_get(&state->test_lines[i].xform);
                    geometry* g = &state->test_lines[i].geo;
                    rd.material = g->material;
                    rd.vertex_count = g->vertex_count;
                    rd.vertex_buffer_offset = g->vertex_buffer_offset;
                    rd.index_count = g->index_count;
                    rd.index_buffer_offset = g->index_buffer_offset;
                    rd.unique_id = INVALID_ID_U16;
                    darray_push(ext_data->debug_geometries, rd);
                    ext_data->debug_geometry_count++;
                }
                u32 box_count = darray_length(state->test_boxes);
                for (u32 i = 0; i < box_count; ++i)
                {
                    geometry_render_data rd = {0};
                    rd.model = transform_world_get(&state->test_boxes[i].xform);
                    geometry* g = &state->test_boxes[i].geo;
                    rd.material = g->material;
                    rd.vertex_count = g->vertex_count;
                    rd.vertex_buffer_offset = g->vertex_buffer_offset;
                    rd.index_count = g->index_count;
                    rd.index_buffer_offset = g->index_buffer_offset;
                    rd.unique_id = INVALID_ID_U16;
                    darray_push(ext_data->debug_geometries, rd);
                    ext_data->debug_geometry_count++;
                }
            }
        }
        // Scene loaded!

        // Editor pass
        {
            // Enable this pass for this frame
            state->editor_pass.pass_data.do_execute = true;
            state->editor_pass.pass_data.vp = &state->world_viewport;
            state->editor_pass.pass_data.view_matrix = camera_view_get(current_camera);
            state->editor_pass.pass_data.view_position = camera_position_get(current_camera);
            state->editor_pass.pass_data.projection_matrix = state->world_viewport.projection;

            editor_pass_extended_data* ext_data = state->editor_pass.pass_data.ext_data;

            geometry* g = &state->gizmo.mode_data[state->gizmo.mode].geo;

            mat4 model = transform_world_get(&state->gizmo.xform);
            f32 scale_scalar = 1.0f;
            state->gizmo.scale_scalar = scale_scalar;  // Keep copy of this for hit detection
            mat4 scale = mat4_scale((vec3){scale_scalar, scale_scalar, scale_scalar});
            model = mat4_mul(model, scale);

            geometry_render_data render_data = {0};
            render_data.model = model;
            render_data.material = g->material;
            render_data.vertex_count = g->vertex_count;
            render_data.vertex_buffer_offset = g->vertex_buffer_offset;
            render_data.index_count = g->index_count;
            render_data.index_buffer_offset = g->index_buffer_offset;
            render_data.unique_id = INVALID_ID;

            ext_data->debug_geometries = darray_create_with_allocator(geometry_render_data, &p_frame_data->allocator);
            darray_push(ext_data->debug_geometries, render_data);

#ifdef _DEBUG
            {
                geometry_render_data plane_normal_render_data = {0};
                plane_normal_render_data.model = transform_world_get(&state->gizmo.plane_normal_line.xform);
                geometry* g = &state->gizmo.plane_normal_line.geo;
                plane_normal_render_data.material = 0;
                plane_normal_render_data.material = g->material;
                plane_normal_render_data.vertex_count = g->vertex_count;
                plane_normal_render_data.vertex_buffer_offset = g->vertex_buffer_offset;
                plane_normal_render_data.index_count = g->index_count;
                plane_normal_render_data.index_buffer_offset = g->index_buffer_offset;
                plane_normal_render_data.unique_id = INVALID_ID;
                darray_push(ext_data->debug_geometries, plane_normal_render_data);
            }
#endif
            ext_data->debug_geometry_count = darray_length(ext_data->debug_geometries);
        }
    }
    else
    {
        // Don't run these passes if scene is not loaded
        state->scene_pass.pass_data.do_execute = false;
        state->scene_pass.pass_data.do_execute = false;
    }

    // UI
    {
        ui_pass_extended_data* ext_data = state->ui_pass.pass_data.ext_data;
        state->ui_pass.pass_data.vp = &state->ui_viewport;
        state->ui_pass.pass_data.view_matrix = mat4_identity();
        state->ui_pass.pass_data.projection_matrix = state->ui_viewport.projection;
        state->ui_pass.pass_data.do_execute = true;

        // Renderables
        ext_data->sui_render_data.renderables = darray_create_with_allocator(standard_ui_renderable, &p_frame_data->allocator);
        void* sui_state = systems_manager_get_state(B_SYSTEM_TYPE_STANDARD_UI_EXT);
        if (!standard_ui_system_render(sui_state, 0, p_frame_data, &ext_data->sui_render_data))
            BERROR("The standard ui system failed to render");
    }

    // Pick
    // {
    //     render_view_packet* view_packet = &packet->views[TESTBED_PACKET_VIEW_PICK];
    //     const render_view* view = view_packet->view;

    //     // Pick uses both world and ui packet data
    //     pick_packet_data pick_packet = {0};
    //     pick_packet.ui_mesh_data = ui_packet.mesh_data;
    //     pick_packet.world_mesh_data = packet->views[TESTBED_PACKET_VIEW_WORLD].geometries;
    //     pick_packet.terrain_mesh_data = packet->views[TESTBED_PACKET_VIEW_WORLD].terrain_geometries;
    //     pick_packet.texts = ui_packet.texts;
    //     pick_packet.text_count = ui_packet.text_count;

    //     if (!render_view_system_packet_build(view, p_frame_data->frame_allocator, &pick_packet, view_packet))
    //     {
    //         BERROR("Failed to build packet for view 'pick'");
    //         return false;
    //     }
    // }
    // TODO: end temp

    clock_update(&state->prepare_clock);
    return true;
}

b8 application_render_frame(struct application* game_inst, struct frame_data* p_frame_data)
{
    // Start the frame
    testbed_game_state* state = (testbed_game_state*)game_inst->state;
    if (!state->running)
        return true;

    clock_start(&state->render_clock);
    if (!renderer_begin(p_frame_data))
    {
        // ...
    }

    if (!rendergraph_execute_frame(&state->frame_graph, p_frame_data))
    {
        BERROR("Failed to execute rendergraph frame");
        return false;
    }

    renderer_end(p_frame_data);

    // NOTE: Stopping the timer before presentation since that can greatly impact this timing
    clock_update(&state->render_clock);

    clock_start(&state->present_clock);
    if (!renderer_present(p_frame_data))
    {
        BERROR("The call to renderer_present failed. This is unrecoverable. Shutting down");
        return false;
    }
    clock_update(&state->present_clock);

    return true;
}

void application_on_resize(struct application* game_inst, u32 width, u32 height)
{
    if (!game_inst->state)
        return;

    testbed_game_state* state = (testbed_game_state*)game_inst->state;

    state->width = width;
    state->height = height;

    if (!width || !height)
        return;

    f32 half_width = state->width * 0.5f;

    // Resize viewports
    // World Viewport (right side)
    rect_2d world_vp_rect = vec4_create(0.0f, 0.0f, state->width, state->height);
    viewport_resize(&state->world_viewport, world_vp_rect);

    // UI Viewport
    rect_2d ui_vp_rect = vec4_create(0.0f, 0.0f, state->width, state->height);
    viewport_resize(&state->ui_viewport, ui_vp_rect);

    // World viewport 2
    rect_2d world_vp_rect2 = vec4_create(20.0f, 20.0f, half_width - 40.0f, state->height - 40.0f);
    viewport_resize(&state->world_viewport2, world_vp_rect2);

    // TODO: temp
    // Move debug text to new bottom of screen
    sui_control_position_set(&state->test_text, vec3_create(20, state->height - 95, 0));

    // Pass resize onto the rendergraph
    rendergraph_on_resize(&state->frame_graph, state->width, state->height);
    // TODO: end temp
}

void application_shutdown(struct application* game_inst)
{
    testbed_game_state* state = (testbed_game_state*)game_inst->state;
    state->running = false;

    if (state->main_scene.state == SIMPLE_SCENE_STATE_LOADED)
    {
        BDEBUG("Unloading scene...");

        simple_scene_unload(&state->main_scene, true);
        clear_debug_objects(game_inst);

        BDEBUG("Done");
    }

    debug_console_unload(&state->debug_console);

    // Destroy rendergraph(s)
    rendergraph_destroy(&state->frame_graph);
}

void application_lib_on_unload(struct application* game_inst)
{
    application_unregister_events(game_inst);
    debug_console_on_lib_unload(&((testbed_game_state*)game_inst->state)->debug_console);
    game_remove_commands(game_inst);
    game_remove_keymaps(game_inst);
}

void application_lib_on_load(struct application* game_inst)
{
    application_register_events(game_inst);
    debug_console_on_lib_load(&((testbed_game_state*)game_inst->state)->debug_console, game_inst->stage >= APPLICATION_STAGE_BOOT_COMPLETE);
    if (game_inst->stage >= APPLICATION_STAGE_BOOT_COMPLETE)
    {
        game_setup_commands(game_inst);
        game_setup_keymaps(game_inst);
    }
}

static void toggle_vsync(void)
{
    b8 vsync_enabled = renderer_flag_enabled_get(RENDERER_CONFIG_FLAG_VSYNC_ENABLED_BIT);
    vsync_enabled = !vsync_enabled;
    renderer_flag_enabled_set(RENDERER_CONFIG_FLAG_VSYNC_ENABLED_BIT, vsync_enabled);
}

static b8 game_on_bvar_changed(u16 code, void* sender, void* listener_inst, event_context data)
{
    if (code == EVENT_CODE_BVAR_CHANGED && strings_equali(data.data.c, "vsync"))
        toggle_vsync();
    return false;
}

void application_register_events(struct application* game_inst)
{
    if (game_inst->stage >= APPLICATION_STAGE_BOOT_COMPLETE)
    {
        // TODO: temp
        event_register(EVENT_CODE_DEBUG0, game_inst, game_on_debug_event);
        event_register(EVENT_CODE_DEBUG1, game_inst, game_on_debug_event);
        event_register(EVENT_CODE_DEBUG2, game_inst, game_on_debug_event);
        event_register(EVENT_CODE_DEBUG3, game_inst, game_on_debug_event);
        event_register(EVENT_CODE_DEBUG4, game_inst, game_on_debug_event);
        event_register(EVENT_CODE_OBJECT_HOVER_ID_CHANGED, game_inst, game_on_event);
        event_register(EVENT_CODE_SET_RENDER_MODE, game_inst, game_on_event);
        event_register(EVENT_CODE_BUTTON_RELEASED, game_inst->state, game_on_button);
        event_register(EVENT_CODE_MOUSE_MOVED, game_inst->state, game_on_mouse_move);
        event_register(EVENT_CODE_MOUSE_DRAG_BEGIN, game_inst->state, game_on_drag);
        event_register(EVENT_CODE_MOUSE_DRAG_END, game_inst->state, game_on_drag);
        event_register(EVENT_CODE_MOUSE_DRAGGED, game_inst->state, game_on_drag);
        // TODO: end temp

        event_register(EVENT_CODE_KEY_PRESSED, game_inst, game_on_key);
        event_register(EVENT_CODE_KEY_RELEASED, game_inst, game_on_key);

        event_register(EVENT_CODE_BVAR_CHANGED, 0, game_on_bvar_changed);
    }
}

void application_unregister_events(struct application* game_inst)
{
    // TODO: temp
    event_unregister(EVENT_CODE_DEBUG0, game_inst, game_on_debug_event);
    event_unregister(EVENT_CODE_DEBUG1, game_inst, game_on_debug_event);
    event_unregister(EVENT_CODE_DEBUG2, game_inst, game_on_debug_event);
    event_unregister(EVENT_CODE_DEBUG3, game_inst, game_on_debug_event);
    event_unregister(EVENT_CODE_DEBUG4, game_inst, game_on_debug_event);
    event_unregister(EVENT_CODE_OBJECT_HOVER_ID_CHANGED, game_inst, game_on_event);
    event_unregister(EVENT_CODE_SET_RENDER_MODE, game_inst, game_on_event);
    event_unregister(EVENT_CODE_BUTTON_RELEASED, game_inst->state, game_on_button);
    event_unregister(EVENT_CODE_MOUSE_MOVED, game_inst->state, game_on_mouse_move);
    event_unregister(EVENT_CODE_MOUSE_DRAG_BEGIN, game_inst->state, game_on_drag);
    event_unregister(EVENT_CODE_MOUSE_DRAG_END, game_inst->state, game_on_drag);
    event_unregister(EVENT_CODE_MOUSE_DRAGGED, game_inst->state, game_on_drag);
    // TODO: end temp

    event_unregister(EVENT_CODE_KEY_PRESSED, game_inst, game_on_key);
    event_unregister(EVENT_CODE_KEY_RELEASED, game_inst, game_on_key);

    event_unregister(EVENT_CODE_BVAR_CHANGED, 0, game_on_bvar_changed);
}

#define RG_CHECK(expr)                            \
    if (!expr)                                    \
    {                                             \
        BERROR("Failed to execute: '%s'", #expr); \
        return false;                             \
    }

static void refresh_rendergraph_pfns(application* app)
{
    testbed_game_state* state = (testbed_game_state*)app->state;

    state->skybox_pass.initialize = skybox_pass_initialize;
    state->skybox_pass.execute = skybox_pass_execute;
    state->skybox_pass.destroy = skybox_pass_destroy;

    state->scene_pass.initialize = scene_pass_initialize;
    state->scene_pass.execute = scene_pass_execute;
    state->scene_pass.destroy = scene_pass_destroy;

    state->editor_pass.initialize = editor_pass_initialize;
    state->editor_pass.execute = editor_pass_execute;
    state->editor_pass.destroy = editor_pass_destroy;

    state->ui_pass.initialize = ui_pass_initialize;
    state->ui_pass.execute = ui_pass_execute;
    state->ui_pass.destroy = ui_pass_destroy;
}

static b8 configure_rendergraph(application* app)
{
    testbed_game_state* state = (testbed_game_state*)app->state;

    if (!rendergraph_create("testbed_frame_rendergraph", app, &state->frame_graph))
    {
        BERROR("Failed to create rendergraph");
        return false;
    }

    // Add global sources
    if (!rendergraph_global_source_add(&state->frame_graph, "colorbuffer", RENDERGRAPH_SOURCE_TYPE_RENDER_TARGET_COLOR, RENDERGRAPH_SOURCE_ORIGIN_GLOBAL))
    {
        BERROR("Failed to add global colorbuffer source");
        return false;
    }
    if (!rendergraph_global_source_add(&state->frame_graph, "depthbuffer", RENDERGRAPH_SOURCE_TYPE_RENDER_TARGET_DEPTH_STENCIL, RENDERGRAPH_SOURCE_ORIGIN_GLOBAL))
    {
        BERROR("Failed to add global depthbuffer source");
        return false;
    }

    // Skybox pass
    RG_CHECK(rendergraph_pass_create(&state->frame_graph, "skybox", skybox_pass_create, &state->skybox_pass));
    RG_CHECK(rendergraph_pass_sink_add(&state->frame_graph, "skybox", "colorbuffer"));
    RG_CHECK(rendergraph_pass_source_add(&state->frame_graph, "skybox", "colorbuffer", RENDERGRAPH_SOURCE_TYPE_RENDER_TARGET_COLOR, RENDERGRAPH_SOURCE_ORIGIN_OTHER));
    RG_CHECK(rendergraph_pass_set_sink_linkage(&state->frame_graph, "skybox", "colorbuffer", 0, "colorbuffer"));

    // Scene pass
    RG_CHECK(rendergraph_pass_create(&state->frame_graph, "scene", scene_pass_create, &state->scene_pass));
    RG_CHECK(rendergraph_pass_sink_add(&state->frame_graph, "scene", "colorbuffer"));
    RG_CHECK(rendergraph_pass_sink_add(&state->frame_graph, "scene", "depthbuffer"));
    RG_CHECK(rendergraph_pass_source_add(&state->frame_graph, "scene", "colorbuffer", RENDERGRAPH_SOURCE_TYPE_RENDER_TARGET_COLOR, RENDERGRAPH_SOURCE_ORIGIN_OTHER));
    RG_CHECK(rendergraph_pass_source_add(&state->frame_graph, "scene", "depthbuffer", RENDERGRAPH_SOURCE_TYPE_RENDER_TARGET_DEPTH_STENCIL, RENDERGRAPH_SOURCE_ORIGIN_GLOBAL));
    RG_CHECK(rendergraph_pass_set_sink_linkage(&state->frame_graph, "scene", "colorbuffer", "skybox", "colorbuffer"));
    RG_CHECK(rendergraph_pass_set_sink_linkage(&state->frame_graph, "scene", "depthbuffer", 0, "depthbuffer"));

    // Editor pass
    RG_CHECK(rendergraph_pass_create(&state->frame_graph, "editor", editor_pass_create, &state->editor_pass));
    RG_CHECK(rendergraph_pass_sink_add(&state->frame_graph, "editor", "colorbuffer"));
    RG_CHECK(rendergraph_pass_sink_add(&state->frame_graph, "editor", "depthbuffer"));
    RG_CHECK(rendergraph_pass_source_add(&state->frame_graph, "editor", "colorbuffer", RENDERGRAPH_SOURCE_TYPE_RENDER_TARGET_COLOR, RENDERGRAPH_SOURCE_ORIGIN_OTHER));
    RG_CHECK(rendergraph_pass_source_add(&state->frame_graph, "editor", "depthbuffer", RENDERGRAPH_SOURCE_TYPE_RENDER_TARGET_DEPTH_STENCIL, RENDERGRAPH_SOURCE_ORIGIN_OTHER));
    RG_CHECK(rendergraph_pass_set_sink_linkage(&state->frame_graph, "editor", "colorbuffer", "scene", "colorbuffer"));
    RG_CHECK(rendergraph_pass_set_sink_linkage(&state->frame_graph, "editor", "depthbuffer", "scene", "depthbuffer"));

    // UI pass
    RG_CHECK(rendergraph_pass_create(&state->frame_graph, "ui", ui_pass_create, &state->ui_pass));
    RG_CHECK(rendergraph_pass_sink_add(&state->frame_graph, "ui", "colorbuffer"));
    RG_CHECK(rendergraph_pass_sink_add(&state->frame_graph, "ui", "depthbuffer"));
    RG_CHECK(rendergraph_pass_source_add(&state->frame_graph, "ui", "colorbuffer", RENDERGRAPH_SOURCE_TYPE_RENDER_TARGET_COLOR, RENDERGRAPH_SOURCE_ORIGIN_OTHER));
    RG_CHECK(rendergraph_pass_source_add(&state->frame_graph, "ui", "depthbuffer", RENDERGRAPH_SOURCE_TYPE_RENDER_TARGET_DEPTH_STENCIL, RENDERGRAPH_SOURCE_ORIGIN_GLOBAL));
    RG_CHECK(rendergraph_pass_set_sink_linkage(&state->frame_graph, "ui", "colorbuffer", "editor", "colorbuffer"));
    RG_CHECK(rendergraph_pass_set_sink_linkage(&state->frame_graph, "ui", "depthbuffer", 0, "depthbuffer"));

    refresh_rendergraph_pfns(app);

    if (!rendergraph_finalize(&state->frame_graph))
    {
        BERROR("Failed to finalize rendergraph. See log for details");
        return false;
    }

    return true;
}

/* b8 configure_render_views(application_config* config)
{
    config->views = darray_create(render_view);

    // World view
    {
        render_view world_view = {0};
        world_view.name = "world";
        world_view.renderpass_count = 2;
        world_view.passes = ballocate(sizeof(renderpass) * world_view.renderpass_count, MEMORY_TAG_ARRAY);

        // Renderpass config (SKYBOX)
        renderpass_config skybox_pass = {0};
        skybox_pass.name = "Renderpass.Builtin.Skybox";
        skybox_pass.clear_color = (vec4){0.0f, 0.0f, 0.2f, 1.0f};
        skybox_pass.clear_flags = RENDERPASS_CLEAR_COLOR_BUFFER_FLAG;
        skybox_pass.depth = 1.0f;
        skybox_pass.stencil = 0;
        skybox_pass.target.attachment_count = 1;
        skybox_pass.target.attachments = ballocate(sizeof(render_target_attachment_config) * skybox_pass.target.attachment_count, MEMORY_TAG_ARRAY);
        skybox_pass.render_target_count = renderer_window_attachment_count_get();

        // Color attachment
        render_target_attachment_config* skybox_target_color = &skybox_pass.target.attachments[0];
        skybox_target_color->type = RENDER_TARGET_ATTACHMENT_TYPE_COLOR;
        skybox_target_color->source = RENDER_TARGET_ATTACHMENT_SOURCE_DEFAULT;
        skybox_target_color->load_operation = RENDER_TARGET_ATTACHMENT_LOAD_OPERATION_DONT_CARE;
        skybox_target_color->store_operation = RENDER_TARGET_ATTACHMENT_STORE_OPERATION_STORE;
        skybox_target_color->present_after = false;

        if (!renderer_renderpass_create(&skybox_pass, &world_view.passes[0]))
        {
            BERROR("Skybox view - Failed to create renderpass '%s'", world_view.passes[0].name);
            return false;
        }

        // Renderpass config (WORLD)
        renderpass_config world_pass = {0};
        world_pass.name = "Renderpass.Builtin.World";
        world_pass.clear_color = (vec4){0.0f, 0.0f, 0.2f, 1.0f};
        world_pass.clear_flags = RENDERPASS_CLEAR_DEPTH_BUFFER_FLAG | RENDERPASS_CLEAR_STENCIL_BUFFER_FLAG;
        world_pass.depth = 1.0f;
        world_pass.stencil = 0;
        world_pass.target.attachment_count = 2;
        world_pass.target.attachments = ballocate(sizeof(render_target_attachment_config) * world_pass.target.attachment_count, MEMORY_TAG_ARRAY);
        world_pass.render_target_count = renderer_window_attachment_count_get();

        // Color attachment
        render_target_attachment_config* world_target_color = &world_pass.target.attachments[0];
        world_target_color->type = RENDER_TARGET_ATTACHMENT_TYPE_COLOR;
        world_target_color->source = RENDER_TARGET_ATTACHMENT_SOURCE_DEFAULT;
        world_target_color->load_operation = RENDER_TARGET_ATTACHMENT_LOAD_OPERATION_LOAD;
        world_target_color->store_operation = RENDER_TARGET_ATTACHMENT_STORE_OPERATION_STORE;
        world_target_color->present_after = false;

        // Depth attachment
        render_target_attachment_config* world_target_depth = &world_pass.target.attachments[1];
        world_target_depth->type = RENDER_TARGET_ATTACHMENT_TYPE_DEPTH;
        world_target_depth->source = RENDER_TARGET_ATTACHMENT_SOURCE_DEFAULT;
        world_target_depth->load_operation = RENDER_TARGET_ATTACHMENT_LOAD_OPERATION_DONT_CARE;
        world_target_depth->store_operation = RENDER_TARGET_ATTACHMENT_STORE_OPERATION_STORE;
        world_target_depth->present_after = false;

        if (!renderer_renderpass_create(&world_pass, &world_view.passes[1]))
        {
            BERROR("World view - Failed to create renderpass '%s'", world_view.passes[1].name);
            return false;
        }

        // Assign function pointers
        world_view.on_packet_build = render_view_world_on_packet_build;
        world_view.on_packet_destroy = render_view_world_on_packet_destroy;
        world_view.on_render = render_view_world_on_render;
        world_view.on_registered = render_view_world_on_registered;
        world_view.on_destroy = render_view_world_on_destroy;
        world_view.on_resize = render_view_world_on_resize;
        world_view.attachment_target_regenerate = 0;

        darray_push(config->views, world_view);
    }

    // TODO: Editor temp
    // Editor World view
    {
        render_view editor_world_view = {0};
        editor_world_view.name = "editor_world";
        editor_world_view.renderpass_count = 1;
        editor_world_view.passes = ballocate(sizeof(renderpass) * editor_world_view.renderpass_count, MEMORY_TAG_ARRAY);

        // Renderpass config
        renderpass_config editor_world_pass = {0};
        editor_world_pass.name = "Renderpass.Testbed.EditorWorld";
        editor_world_pass.clear_color = (vec4){0.0f, 0.0f, 0.0f, 1.0f};
        editor_world_pass.clear_flags = RENDERPASS_CLEAR_DEPTH_BUFFER_FLAG | RENDERPASS_CLEAR_STENCIL_BUFFER_FLAG;
        editor_world_pass.depth = 1.0f;
        editor_world_pass.stencil = 0;
        editor_world_pass.target.attachment_count = 2;
        editor_world_pass.target.attachments = ballocate(sizeof(render_target_attachment_config) * editor_world_pass.target.attachment_count, MEMORY_TAG_ARRAY);
        editor_world_pass.render_target_count = renderer_window_attachment_count_get();

        // Color attachment
        render_target_attachment_config* editor_world_target_color = &editor_world_pass.target.attachments[0];
        editor_world_target_color->type = RENDER_TARGET_ATTACHMENT_TYPE_COLOR;
        editor_world_target_color->source = RENDER_TARGET_ATTACHMENT_SOURCE_DEFAULT;
        editor_world_target_color->load_operation = RENDER_TARGET_ATTACHMENT_LOAD_OPERATION_LOAD;
        editor_world_target_color->store_operation = RENDER_TARGET_ATTACHMENT_STORE_OPERATION_STORE;
        editor_world_target_color->present_after = false;

        // Depth attachment
        render_target_attachment_config* editor_world_target_depth = &editor_world_pass.target.attachments[1];
        editor_world_target_depth->type = RENDER_TARGET_ATTACHMENT_TYPE_DEPTH;
        editor_world_target_depth->source = RENDER_TARGET_ATTACHMENT_SOURCE_DEFAULT;
        editor_world_target_depth->load_operation = RENDER_TARGET_ATTACHMENT_LOAD_OPERATION_DONT_CARE;
        editor_world_target_depth->store_operation = RENDER_TARGET_ATTACHMENT_STORE_OPERATION_STORE;
        editor_world_target_depth->present_after = false;

        if (!renderer_renderpass_create(&editor_world_pass, &editor_world_view.passes[0]))
        {
            BERROR("World view - Failed to create renderpass '%s'", editor_world_view.passes[0].name);
            return false;
        }

        // Assign function pointers
        editor_world_view.on_packet_build = render_view_editor_world_on_packet_build;
        editor_world_view.on_packet_destroy = render_view_editor_world_on_packet_destroy;
        editor_world_view.on_render = render_view_editor_world_on_render;
        editor_world_view.on_registered = render_view_editor_world_on_registered;
        editor_world_view.on_destroy = render_view_editor_world_on_destroy;
        editor_world_view.on_resize = render_view_editor_world_on_resize;
        editor_world_view.attachment_target_regenerate = 0;

        darray_push(config->views, editor_world_view);
    }

    // Wireframe view
    {
        render_view wireframe_view = {0};
        wireframe_view.name = "wireframe";
        wireframe_view.renderpass_count = 1;
        wireframe_view.passes = ballocate(sizeof(renderpass) * wireframe_view.renderpass_count, MEMORY_TAG_ARRAY);

        // Renderpass config
        renderpass_config wireframe_pass = {0};
        wireframe_pass.name = "Renderpass.Testbed.Wireframe";
        wireframe_pass.clear_color = (vec4){0.2f, 0.2f, 0.2f, 1.0f};
        wireframe_pass.clear_flags = RENDERPASS_CLEAR_COLOR_BUFFER_FLAG | RENDERPASS_CLEAR_DEPTH_BUFFER_FLAG | RENDERPASS_CLEAR_STENCIL_BUFFER_FLAG;
        wireframe_pass.depth = 1.0f;
        wireframe_pass.stencil = 0;
        wireframe_pass.target.attachment_count = 2;
        wireframe_pass.target.attachments = ballocate(sizeof(render_target_attachment_config) * wireframe_pass.target.attachment_count, MEMORY_TAG_ARRAY);
        wireframe_pass.render_target_count = renderer_window_attachment_count_get();

        // Color attachment
        render_target_attachment_config* wireframe_target_color = &wireframe_pass.target.attachments[0];
        wireframe_target_color->type = RENDER_TARGET_ATTACHMENT_TYPE_COLOR;
        wireframe_target_color->source = RENDER_TARGET_ATTACHMENT_SOURCE_DEFAULT;
        wireframe_target_color->load_operation = RENDER_TARGET_ATTACHMENT_LOAD_OPERATION_LOAD;
        wireframe_target_color->store_operation = RENDER_TARGET_ATTACHMENT_STORE_OPERATION_STORE;
        wireframe_target_color->present_after = false;

        // Depth attachment
        render_target_attachment_config* wireframe_target_depth = &wireframe_pass.target.attachments[1];
        wireframe_target_depth->type = RENDER_TARGET_ATTACHMENT_TYPE_DEPTH;
        wireframe_target_depth->source = RENDER_TARGET_ATTACHMENT_SOURCE_DEFAULT;
        wireframe_target_depth->load_operation = RENDER_TARGET_ATTACHMENT_LOAD_OPERATION_DONT_CARE;
        wireframe_target_depth->store_operation = RENDER_TARGET_ATTACHMENT_STORE_OPERATION_STORE;
        wireframe_target_depth->present_after = false;

        if (!renderer_renderpass_create(&wireframe_pass, &wireframe_view.passes[0]))
        {
            BERROR("World view - Failed to create renderpass '%s'", wireframe_view.passes[0].name);
            return false;
        }

        // Assign function pointers
        wireframe_view.on_packet_build = render_view_wireframe_on_packet_build;
        wireframe_view.on_packet_destroy = render_view_wireframe_on_packet_destroy;
        wireframe_view.on_render = render_view_wireframe_on_render;
        wireframe_view.on_registered = render_view_wireframe_on_registered;
        wireframe_view.on_destroy = render_view_wireframe_on_destroy;
        wireframe_view.on_resize = render_view_wireframe_on_resize;
        wireframe_view.attachment_target_regenerate = 0;

        darray_push(config->views, wireframe_view);
    }

    // UI view
    {
        render_view ui_view = {0};
        ui_view.name = "ui";
        ui_view.renderpass_count = 1;
        ui_view.passes = ballocate(sizeof(renderpass) * ui_view.renderpass_count, MEMORY_TAG_ARRAY);

        // Renderpass config
        renderpass_config ui_pass;
        ui_pass.name = "Renderpass.Builtin.UI";
        ui_pass.clear_color = (vec4){0.0f, 0.0f, 0.2f, 1.0f};
        ui_pass.clear_flags = RENDERPASS_CLEAR_NONE_FLAG;
        ui_pass.depth = 1.0f;
        ui_pass.stencil = 0;
        ui_pass.target.attachment_count = 1;
        ui_pass.target.attachments = ballocate(sizeof(render_target_attachment_config) * ui_pass.target.attachment_count, MEMORY_TAG_ARRAY);
        ui_pass.render_target_count = renderer_window_attachment_count_get();

        render_target_attachment_config* ui_target_attachment = &ui_pass.target.attachments[0];
        // Color attachment
        ui_target_attachment->type = RENDER_TARGET_ATTACHMENT_TYPE_COLOR;
        ui_target_attachment->source = RENDER_TARGET_ATTACHMENT_SOURCE_DEFAULT;
        ui_target_attachment->load_operation = RENDER_TARGET_ATTACHMENT_LOAD_OPERATION_LOAD;
        ui_target_attachment->store_operation = RENDER_TARGET_ATTACHMENT_STORE_OPERATION_STORE;
        ui_target_attachment->present_after = true;

        if (!renderer_renderpass_create(&ui_pass, &ui_view.passes[0]))
        {
            BERROR("UI view - Failed to create renderpass '%s'", ui_view.passes[0].name);
            return false;
        }

        // Assign function pointers
        ui_view.on_packet_build = render_view_ui_on_packet_build;
        ui_view.on_packet_destroy = render_view_ui_on_packet_destroy;
        ui_view.on_render = render_view_ui_on_render;
        ui_view.on_registered = render_view_ui_on_registered;
        ui_view.on_destroy = render_view_ui_on_destroy;
        ui_view.on_resize = render_view_ui_on_resize;
        ui_view.attachment_target_regenerate = 0;

        darray_push(config->views, ui_view);
    }

    // Pick pass
    // TODO: Split into 2 views and reenable
    {
        render_view pick_view = {};
        pick_view.name = "pick";
        pick_view.renderpass_count = 2;
        pick_view.passes = ballocate(sizeof(renderpass) * pick_view.renderpass_count, MEMORY_TAG_ARRAY);

        // World pick pass
        renderpass_config world_pick_pass = {0};
        world_pick_pass.name = "Renderpass.Builtin.WorldPick";
        world_pick_pass.clear_color = (vec4){1.0f, 1.0f, 1.0f, 1.0f};  // TODO: Clear to black, as 0 is invalid id
        world_pick_pass.clear_flags = RENDERPASS_CLEAR_COLOR_BUFFER_FLAG | RENDERPASS_CLEAR_DEPTH_BUFFER_FLAG;
        world_pick_pass.depth = 1.0f;
        world_pick_pass.stencil = 0;
        world_pick_pass.render_target_count = 1;  // NOTE: Not triple-buffering this
        world_pick_pass.target.attachment_count = 2;
        world_pick_pass.target.attachments = ballocate(sizeof(render_target_attachment_config) * world_pick_pass.target.attachment_count, MEMORY_TAG_ARRAY);

        // Color attachment
        render_target_attachment_config* world_pick_pass_color = &world_pick_pass.target.attachments[0];
        world_pick_pass_color->type = RENDER_TARGET_ATTACHMENT_TYPE_COLOR;
        world_pick_pass_color->source = RENDER_TARGET_ATTACHMENT_SOURCE_VIEW;  // Obtain attachment from the view
        world_pick_pass_color->load_operation = RENDER_TARGET_ATTACHMENT_LOAD_OPERATION_DONT_CARE;
        world_pick_pass_color->store_operation = RENDER_TARGET_ATTACHMENT_STORE_OPERATION_STORE;
        world_pick_pass_color->present_after = false;

        // Depth attachment
        render_target_attachment_config* world_pick_pass_depth = &world_pick_pass.target.attachments[1];
        world_pick_pass_depth->type = RENDER_TARGET_ATTACHMENT_TYPE_DEPTH;
        world_pick_pass_depth->source = RENDER_TARGET_ATTACHMENT_SOURCE_VIEW;  // Obtain attachment from the view
        world_pick_pass_depth->load_operation = RENDER_TARGET_ATTACHMENT_LOAD_OPERATION_DONT_CARE;
        world_pick_pass_depth->store_operation = RENDER_TARGET_ATTACHMENT_STORE_OPERATION_STORE;
        world_pick_pass_depth->present_after = false;

        if (!renderer_renderpass_create(&world_pick_pass, &pick_view.passes[0]))
        {
            BERROR("Pick view - Failed to create renderpass '%s'", pick_view.passes[0].name);
            return false;
        }

        // UI pick pass
        renderpass_config ui_pick_pass = {0};
        ui_pick_pass.name = "Renderpass.Builtin.UIPick";
        ui_pick_pass.clear_color = (vec4){1.0f, 1.0f, 1.0f, 1.0f};
        ui_pick_pass.clear_flags = RENDERPASS_CLEAR_NONE_FLAG;
        ui_pick_pass.depth = 1.0f;
        ui_pick_pass.stencil = 0;
        ui_pick_pass.target.attachment_count = 1;
        ui_pick_pass.target.attachments = ballocate(sizeof(render_target_attachment_config) * ui_pick_pass.target.attachment_count, MEMORY_TAG_ARRAY);
        ui_pick_pass.render_target_count = 1;  // NOTE: Not triple-buffering this

        render_target_attachment_config* ui_pick_pass_attachment = &ui_pick_pass.target.attachments[0];
        ui_pick_pass_attachment->type = RENDER_TARGET_ATTACHMENT_TYPE_COLOR;
        // Obtain the attachment from the view
        ui_pick_pass_attachment->source = RENDER_TARGET_ATTACHMENT_SOURCE_VIEW;
        ui_pick_pass_attachment->load_operation = RENDER_TARGET_ATTACHMENT_LOAD_OPERATION_LOAD;
        // Need to store it so it can be sampled afterward
        ui_pick_pass_attachment->store_operation = RENDER_TARGET_ATTACHMENT_STORE_OPERATION_STORE;
        ui_pick_pass_attachment->present_after = false;

        if (!renderer_renderpass_create(&ui_pick_pass, &pick_view.passes[1]))
        {
            BERROR("Pick view - Failed to create renderpass '%s'", pick_view.passes[1].name);
            return false;
        }

        // Assign function pointers
        pick_view.on_packet_build = render_view_pick_on_packet_build;
        pick_view.on_packet_destroy = render_view_pick_on_packet_destroy;
        pick_view.on_render = render_view_pick_on_render;
        pick_view.on_registered = render_view_pick_on_registered;
        pick_view.on_destroy = render_view_pick_on_destroy;
        pick_view.on_resize = render_view_pick_on_resize;
        pick_view.attachment_target_regenerate = render_view_pick_attachment_target_regenerate;

        darray_push(config->views, pick_view);
    }

    return true;
} */

static b8 load_main_scene(struct application* game_inst)
{
    testbed_game_state* state = (testbed_game_state*)game_inst->state;

    // Load config file
    // TODO: clean up resource
    resource simple_scene_resource;
    if (!resource_system_load("test_scene", RESOURCE_TYPE_SIMPLE_SCENE, 0, &simple_scene_resource))
    {
        BERROR("Failed to load scene file, check logs");
        return false;
    }

    simple_scene_config* scene_config = (simple_scene_config*)simple_scene_resource.data;

    // TODO: temp load/prepare stuff
    if (!simple_scene_create(scene_config, &state->main_scene))
    {
        BERROR("Failed to create main scene");
        return false;
    }

    // Initialize
    if (!simple_scene_initialize(&state->main_scene))
    {
        BERROR("Failed initialize main scene, aborting game");
        return false;
    }

    state->p_light_1 = simple_scene_point_light_get(&state->main_scene, "point_light_1");

    return simple_scene_load(&state->main_scene);
}
