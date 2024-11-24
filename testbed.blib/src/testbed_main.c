#include "testbed_main.h"

#include <containers/darray.h>
#include <core/console.h>
#include <core/engine.h>
#include <core/event.h>
#include <core/frame_data.h>
#include <core/input.h>
#include <core/bvar.h>
#include <core/metrics.h>
#include <defines.h>
#include <identifiers/bhandle.h>
#include <input_types.h>
#include <logger.h>
#include <math/geometry_2d.h>
#include <math/geometry_3d.h>
#include <math/bmath.h>
#include <math/math_types.h>
#include <memory/bmemory.h>
#include <renderer/camera.h>
#include <renderer/renderer_frontend.h>
#include <renderer/renderer_types.h>
#include <renderer/rendergraph.h>
#include <renderer/viewport.h>
#include <resources/terrain.h>
#include <strings/bstring.h>
#include <systems/camera_system.h>
#include <systems/texture_system.h>
#include <time/bclock.h>

#include "application/application_config.h"
#include "game_state.h"

// Standard UI
#include <controls/sui_button.h>
#include <controls/sui_label.h>
#include <controls/sui_panel.h>
#include <standard_ui_plugin_main.h>
#include <standard_ui_system.h>

// Audio plugin
#include <resources/loaders/audio_loader.h>

// TODO: Editor temp
#include "editor/editor_gizmo.h"
#include "editor/editor_gizmo_rendergraph_node.h"
#include <resources/debug/debug_box3d.h>
#include <resources/debug/debug_line3d.h>
#include <resources/water_plane.h>

// TODO: temp
#include <identifiers/identifier.h>
#include <resources/mesh.h>
#include <resources/scene.h>
#include <resources/skybox.h>
#include <systems/audio_system.h>
#include <systems/geometry_system.h>
#include <systems/light_system.h>
#include <systems/material_system.h>
#include <systems/resource_system.h>
#include <systems/shader_system.h>
// Standard ui
#include <core/systems_manager.h>
#include <standard_ui_system.h>

#include "debug_console.h"
// Game code
#include "game_commands.h"
#include "game_keybinds.h"
// TODO: end temp

#include "platform/platform.h"
#include "plugins/plugin_types.h"
#include "renderer/rendergraph_nodes/debug_rendergraph_node.h"
#include "renderer/rendergraph_nodes/forward_rendergraph_node.h"
#include "renderer/rendergraph_nodes/shadow_rendergraph_node.h"
#include "rendergraph_nodes/ui_rendergraph_node.h"
#include "systems/plugin_system.h"
#include "systems/timeline_system.h"

typedef struct geometry_distance
{
    geometry_render_data g;
    f32 distance;
} geometry_distance;

void application_register_events(struct application* game_inst);
void application_unregister_events(struct application* game_inst);
static b8 load_main_scene(struct application* game_inst);
static b8 save_main_scene(struct application* game_inst);

static f32 get_engine_delta_time(void)
{
    bhandle engine = timeline_system_get_engine();
    return timeline_system_delta_get(engine);
}

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
            case RENDERER_VIEW_MODE_CASCADES:
                BDEBUG("Renderer mode set to cascades");
                state->render_mode = RENDERER_VIEW_MODE_CASCADES;
                break;
            case RENDERER_VIEW_MODE_WIREFRAME:
                BDEBUG("Renderer mode set to wireframe");
                state->render_mode = RENDERER_VIEW_MODE_WIREFRAME;
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
        if (state->main_scene.state < SCENE_STATE_LOADING)
        {
            BDEBUG("Loading main scene...");
            if (!load_main_scene(game_inst))
                BERROR("Error loading main scene");
        }
        return true;
    }
    else if (code == EVENT_CODE_DEBUG5)
    {
        if (state->main_scene.state >= SCENE_STATE_LOADING)
        {
            BDEBUG("Saving main scene...");
            if (!save_main_scene(game_inst))
                BERROR("Error saving main scene");
        }
        return true;
    }
    else if (code == EVENT_CODE_DEBUG2)
    {
        if (state->main_scene.state == SCENE_STATE_LOADED)
        {
            BDEBUG("Unloading scene...");
            scene_unload(&state->main_scene, false);
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

static b8 game_on_drag(u16 code, void* sender, void* listener_inst, event_context context)
{
    i16 x = context.data.i16[0];
    i16 y = context.data.i16[1];
    u16 drag_button = context.data.u16[2];
    testbed_game_state* state = (testbed_game_state*)listener_inst;

    // Left button drags
    if (drag_button == MOUSE_BUTTON_LEFT)
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
            case MOUSE_BUTTON_LEFT:
            {
                i16 x = context.data.i16[1];
                i16 y = context.data.i16[2];
                testbed_game_state* state = (testbed_game_state*)listener_inst;

                // If scene isn't loaded, don't do anything else
                if (state->main_scene.state < SCENE_STATE_LOADED)
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
                    if (scene_raycast(&state->main_scene, &r, &r_result))
                    {
                        u32 hit_count = darray_length(r_result.hits);
                        for (u32 i = 0; i < hit_count; ++i)
                        {
                            raycast_hit* hit = &r_result.hits[i];
                            BINFO("Hit! id: %u, dist: %f", hit->node_handle.handle_index, hit->distance);

                            // Create a debug line where the ray cast starts and ends (at the intersection)
                            debug_line3d test_line;
                            debug_line3d_create(r.origin, hit->position, bhandle_invalid(), &test_line);
                            debug_line3d_initialize(&test_line);
                            debug_line3d_load(&test_line);
                            // Yellow for hits
                            debug_line3d_color_set(&test_line, (vec4){1.0f, 1.0f, 0.0f, 1.0f});

                            darray_push(state->test_lines, test_line);

                            // Create a debug box to show the intersection point
                            debug_box3d test_box;

                            debug_box3d_create((vec3){0.1f, 0.1f, 0.1f}, bhandle_invalid(), &test_box);
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
                                state->selection.node_handle = hit->node_handle;
                                state->selection.xform_handle = hit->xform_handle;
                                state->selection.xform_parent_handle = hit->xform_parent_handle;
                                if (!bhandle_is_invalid(state->selection.xform_handle))
                                {
                                    BINFO("Selected object id %u", hit->node_handle.handle_index);
                                    editor_gizmo_selected_transform_set(&state->gizmo, state->selection.xform_handle, state->selection.xform_parent_handle);
                                }
                            }
                        }
                    }
                    else
                    {
                        BINFO("No hit");

                        // Create a debug line where the ray cast starts and continues to
                        debug_line3d test_line;
                        debug_line3d_create(r.origin, vec3_add(r.origin, vec3_mul_scalar(r.direction, 100.0f)), bhandle_invalid(), &test_line);
                        debug_line3d_initialize(&test_line);
                        debug_line3d_load(&test_line);
                        // Magenta for non-hits
                        debug_line3d_color_set(&test_line, (vec4){1.0f, 0.0f, 1.0f, 1.0f});

                        darray_push(state->test_lines, test_line);

                        if (bhandle_is_invalid(state->selection.xform_handle))
                        {
                            BINFO("Object deselected");
                            state->selection.xform_handle = bhandle_invalid();
                            state->selection.node_handle = bhandle_invalid();
                            state->selection.xform_parent_handle = bhandle_invalid();

                            editor_gizmo_selected_transform_set(&state->gizmo, state->selection.xform_handle, state->selection.xform_parent_handle);
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
    if (code == EVENT_CODE_MOUSE_MOVED && !input_is_button_dragging(MOUSE_BUTTON_LEFT))
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

static void sui_test_button_on_click(struct standard_ui_state* state, struct sui_control* self, struct sui_mouse_event event)
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
    testbed_game_state* state = game_inst->state;
    state->running = false;

    application_config* config = &game_inst->app_config;

    config->frame_allocator_size = MEBIBYTES(64);
    config->app_frame_data_size = sizeof(testbed_application_frame_data);

    // Register custom rendergraph nodes, systems, etc
    if (!editor_gizmo_rendergraph_node_register_factory())
    {
        BERROR("Failed to register editor_gizmo rendergraph node");
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

    testbed_game_state* state = (testbed_game_state*)game_inst->state;

    // Get the standard ui plugin
    state->sui_plugin = plugin_system_get(engine_systems_get()->plugin_system, "bismuth.plugin.ui.standard");
    state->sui_plugin_state = state->sui_plugin->plugin_state;
    state->sui_state = state->sui_plugin_state->state;
    standard_ui_state* sui_state = state->sui_state;

    debug_console_create(state->sui_state, &((testbed_game_state*)game_inst->state)->debug_console);

    application_register_events(game_inst);

    // Register resource loaders
    resource_system_loader_register(audio_resource_loader_create());

    // Pick out rendergraph(s) config from app config, create/init them from here, save off to state
    application_config* config = &game_inst->app_config;
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

    // TODO: Internalize this step?
    // Might need to happen after the rg acquires its resources
    if (!rendergraph_finalize(&state->forward_graph))
    {
        BERROR("Failed to finalize rendergraph. See logs for details");
        return false;
    }

    // Invalid handle == no selection
    state->selection.xform_handle = bhandle_invalid();

    debug_console_load(&state->debug_console);

    state->test_lines = darray_create(debug_line3d);
    state->test_boxes = darray_create(debug_box3d);

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
    if (!viewport_create(ui_vp_rect, 0.0f, -100.0f, 100.0f, RENDERER_PROJECTION_MATRIX_TYPE_ORTHOGRAPHIC, &state->ui_viewport))
    {
        BERROR("Failed to create UI viewport. Cannot start application");
        return false;
    }

    // TODO: For test
    rect_2d world_vp_rect2 = vec4_create(20.0f, 20.0f, 1280.0f - 40.0f, 720.0f - 40.0f);
    if (!viewport_create(world_vp_rect2, deg_to_rad(45.0f), 0.01f, 10.0f, RENDERER_PROJECTION_MATRIX_TYPE_PERSPECTIVE, &state->world_viewport2))
    {
        BERROR("Failed to create world viewport 2. Cannot start application");
        return false;
    }

    // Setup the clear color
    renderer_clear_color_set(engine_systems_get()->renderer_system, (vec4){0.0f, 0.0f, 0.2f, 1.0f});

    state->forward_move_speed = 5.0f * 5.0f;
    state->backward_move_speed = 2.5f * 5.0f;

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

    // FIXME: set in debug3d rg node
    // editor_rendergraph_gizmo_set(&state->editor_graph, &state->gizmo);

    // World meshes

    // Invalidate all meshes
    for (u32 i = 0; i < 10; ++i)
    {
        state->meshes[i].generation = INVALID_ID_U8;
        state->ui_meshes[i].generation = INVALID_ID_U8;
    }

    // Create test ui text objects
    // black background text
    if (!sui_label_control_create(sui_state, "testbed_mono_test_text_black", FONT_TYPE_BITMAP, "Open Sans 21px", 21, "test text 123,\n\tyo!", &state->test_text_black))
    {
        BERROR("Failed to load basic ui bitmap text");
        return false;
    }
    else
    {
        sui_label_color_set(sui_state, &state->test_text_black, (vec4){0, 0, 0, 1});
        if (!sui_label_control_load(sui_state, &state->test_text_black))
        {
            BERROR("Failed to load test text");
        }
        else
        {
            if (!standard_ui_system_register_control(sui_state, &state->test_text_black))
            {
                BERROR("Unable to register control");
            }
            else
            {
                if (!standard_ui_system_control_add_child(sui_state, 0, &state->test_text_black))
                {
                    BERROR("Failed to parent test text");
                }
                else
                {
                    state->test_text_black.is_active = true;
                    if (!standard_ui_system_update_active(sui_state, &state->test_text_black))
                        BERROR("Unable to update active state");
                }
            }
        }
    }
    if (!sui_label_control_create(sui_state, "testbed_mono_test_text", FONT_TYPE_BITMAP, "Open Sans 21px", 21, "Some test text 123,\n\thello!", &state->test_text))
    {
        BERROR("Failed to load basic ui bitmap text");
        return false;
    }
    else
    {
        if (!sui_label_control_load(sui_state, &state->test_text))
        {
            BERROR("Failed to load test text");
        }
        else
        {
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
    sui_control_position_set(sui_state, &state->test_text, vec3_create(20, state->height - 75, 0));
    sui_control_position_set(sui_state, &state->test_text, vec3_create(21, state->height - 74, 0));

    // Standard ui
    if (!sui_panel_control_create(sui_state, "test_panel", (vec2){300.0f, 300.0f}, (vec4){0.0f, 0.0f, 0.0f, 0.5f}, &state->test_panel))
    {
        BERROR("Failed to create test panel");
    }
    else
    {
        if (!sui_panel_control_load(sui_state, &state->test_panel))
        {
            BERROR("Failed to load test panel");
        }
        else
        {
            xform_translate(state->test_panel.xform, (vec3){950, 350});
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

    if (!sui_button_control_create(sui_state, "test_button", &state->test_button))
    {
        BERROR("Failed to create test button");
    }
    else
    {
        // Assign a click handler
        state->test_button.on_click = sui_test_button_on_click;

        if (!sui_button_control_load(sui_state, &state->test_button))
        {
            BERROR("Failed to load test button");
        }
        else
        {
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

    if (!sui_label_control_create(sui_state, "testbed_UTF_test_sys_text", FONT_TYPE_SYSTEM, "Noto Sans CJK JP", 31, "Press 'L' to load scene, \n\thello!\n\n\tこんにちは", &state->test_sys_text))
    {
        BERROR("Failed to load basic ui system text");
        return false;
    }
    else
    {
        if (!sui_label_control_load(sui_state, &state->test_sys_text))
        {
            BERROR("Failed to load test system text");
        }
        else
        {
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
    sui_control_position_set(sui_state, &state->test_sys_text, vec3_create(950, 450, 0));
    // TODO: end temp load/prepare stuff

    state->world_camera = camera_system_acquire("world");
    camera_position_set(state->world_camera, (vec3){-3.95f, 4.25f, 15.80f});
    camera_rotation_euler_set(state->world_camera, (vec3){-11.50f, -75.00f, 0.0f});

    // TODO: temp test camera
    state->world_camera_2 = camera_system_acquire("world_2");
    camera_position_set(state->world_camera_2, (vec3){5.83f, 4.35f, 18.68f});
    camera_rotation_euler_set(state->world_camera_2, (vec3){-29.43f, -42.41f, 0.0f});

    // bzero_memory(&game_inst->frame_data, sizeof(app_frame_data));

    bzero_memory(&state->update_clock, sizeof(bclock));
    bzero_memory(&state->prepare_clock, sizeof(bclock));
    bzero_memory(&state->render_clock, sizeof(bclock));

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

    audio_system_channel_volume_set(7, 0.2f);

    // Try playing the emitter
    /* if (!audio_system_channel_emitter_play(6, &state->test_emitter))
        BERROR("Failed to play test emitter");

    audio_system_channel_play(7, state->test_music, true); */

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
    
    bclock_start(&state->update_clock);

    // TODO: testing resize
    static f32 button_height = 50.0f;
    button_height = 50.0f + (bsin(get_engine_delta_time()) * 20.0f);
    sui_button_control_height_set(state->sui_state, &state->test_button, (i32)button_height);

    // Update bitmap text with camera position. NOTE: just using the default camera for now
    vec3 pos = camera_position_get(state->world_camera);
    vec3 rot = camera_rotation_euler_get(state->world_camera);

    viewport* view_viewport = &state->world_viewport;

    f32 near_clip = view_viewport->near_clip;
    f32 far_clip = view_viewport->far_clip;

    if (state->main_scene.state >= SCENE_STATE_LOADED)
    {
        if (!scene_update(&state->main_scene, p_frame_data))
            BWARN("Failed to update main scene");
        
        // Update LODs for the scene based on distance from the camera
        scene_update_lod_from_view_position(&state->main_scene, p_frame_data, pos, near_clip, far_clip);
        
        editor_gizmo_update(&state->gizmo);

        if (state->p_light_1)
        {
            state->p_light_1->data.color = (vec4){
                BCLAMP(bsin(get_engine_delta_time()) * 75.0f + 50.0f, 0.0f, 100.0f),
                BCLAMP(bsin(get_engine_delta_time() - (B_2PI / 3)) * 75.0f + 50.0f, 0.0f, 100.0f),
                BCLAMP(bsin(get_engine_delta_time() - (B_4PI / 3)) * 75.0f + 50.0f, 0.0f, 100.0f),
                1.0f
            };
            state->p_light_1->data.position.z = 20.0f + bsin(get_engine_delta_time());

            // Make audio emitter follow it
            state->test_emitter.position = vec3_from_vec4(state->p_light_1->data.position);
        }
    }

    // Track allocation differences
    state->prev_alloc_count = state->alloc_count;
    state->alloc_count = get_memory_alloc_count();

    // Only track these things once actually running
    if (state->running)
    {
        // Also tack on current mouse state
        b8 left_down = input_is_button_down(MOUSE_BUTTON_LEFT);
        b8 right_down = input_is_button_down(MOUSE_BUTTON_RIGHT);
        i32 mouse_x, mouse_y;
        input_get_mouse_position(&mouse_x, &mouse_y);

        // Convert to NDC (Native Device Coordinates)
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
        char* text_buffer = string_format(
            "\
FPS: %5.1f(%4.1fms)        Pos=[%7.3f %7.3f %7.3f] Rot=[%7.3f, %7.3f, %7.3f]\n\
Upd: %8.3fus, Prep: %8.3fus, Rend: %8.3fus, Total: %8.3fus \n\
Mouse: X=%-5d Y=%-5d   L=%s R=%s   NDC: X=%.6f, Y=%.6f\n\
VSync: %s Drawn: %-5u (%-5u shadow pass) Hovered: %s%u",
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
            state->hovered_object_id == INVALID_ID ? "none" : "",
            state->hovered_object_id == INVALID_ID ? 0 : state->hovered_object_id);

        // Update text control
        sui_label_text_set(state->sui_state, &state->test_text, text_buffer);
        sui_label_text_set(state->sui_state, &state->test_text_black, text_buffer);
        string_free(text_buffer);
    }

    debug_console_update(&((testbed_game_state*)game_inst->state)->debug_console);

    vec3 forward = camera_forward(state->world_camera);
    vec3 up = camera_up(state->world_camera);
    audio_system_listener_orientation_set(pos, forward, up);

    bclock_update(&state->update_clock);
    state->last_update_elapsed = state->update_clock.elapsed;

    return true;
}

b8 application_prepare_frame(struct application* app_inst, struct frame_data* p_frame_data)
{
    testbed_game_state* state = (testbed_game_state*)app_inst->state;
    if (!state->running)
        return false;

    bclock_start(&state->prepare_clock);

    scene* scene = &state->main_scene;
    camera* current_camera = state->world_camera;
    viewport* current_viewport = &state->world_viewport;

    // HACK: Using the first light in the collection for now.
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
    for (u32 c = 0; c < MAX_SHADOW_CASCADE_COUNT; c++)
    {
        f32 p = (c + 1) / (f32)MAX_SHADOW_CASCADE_COUNT;
        f32 log = min_z * bpow(ratio, p);
        f32 uniform = min_z + range * p;
        f32 d = cascade_split_multiplier * (log - uniform) + uniform;
        splits.elements[c] = (d - near) / clip_range;
    }

    // Default values to use in the event there is no directional light.
    // These are required because the scene pass needs them
    mat4 shadow_camera_lookats[MAX_SHADOW_CASCADE_COUNT];
    mat4 shadow_camera_projections[MAX_SHADOW_CASCADE_COUNT];
    vec3 shadow_camera_positions[MAX_SHADOW_CASCADE_COUNT];
    for (u32 i = 0; i < MAX_SHADOW_CASCADE_COUNT; ++i)
    {
        shadow_camera_lookats[i] = mat4_identity();
        shadow_camera_projections[i] = mat4_identity();
        shadow_camera_positions[i] = vec3_zero();
    }

    // FIXME: Cache this instead of looking up every frame
    u32 node_count = state->forward_graph.node_count;
    for (u32 i = 0; i < node_count; ++i)
    {
        rendergraph_node* node = &state->forward_graph.nodes[i];
        if (strings_equali(node->name, "sui"))
        {
            ui_rendergraph_node_set_atlas(node, &state->sui_state->atlas);

            // We have the one
            ui_rendergraph_node_set_viewport_and_matrices(
                node,
                state->ui_viewport,
                mat4_identity(),
                state->ui_viewport.projection);

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
            forward_rendergraph_node_camera_projection_set(
                node,
                current_camera,
                current_viewport->projection);

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
                for (u32 c = 0; c < MAX_SHADOW_CASCADE_COUNT; c++)
                {
                    forward_rendergraph_node_cascade_data_set(
                        node,
                        (near + splits.elements[c] * clip_range) * 1.0f, // splits.elements[c]
                        shadow_camera_lookats[c],
                        shadow_camera_projections[c],
                        c);
                }
                // Ensure the render mode is set
                forward_rendergraph_node_render_mode_set(node, state->render_mode);

                // Tell it about the directional light
                forward_rendergraph_node_directional_light_set(node, dir_light);

                // HACK: use the skybox cubemap as the irradiance texture for now.
                // HACK: #2 Support for multiple skyboxes, but using the first one for now.
                // TODO: Support multiple skyboxes/irradiance maps
                forward_rendergraph_node_irradiance_texture_set(node, p_frame_data, scene->skyboxes ? scene->skyboxes[0].cubemap.texture : texture_system_get_default_bresource_cube_texture(engine_systems_get()->texture_system));

                // Camera frustum culling and count
                viewport* v = current_viewport;
                vec3 forward = camera_forward(current_camera);
                vec3 right = camera_right(current_camera);
                vec3 up = camera_up(current_camera);
                frustum camera_frustum = frustum_create(&current_camera->position, &forward, &right, &up, v->rect.width / v->rect.height, v->fov, v->near_clip, v->far_clip);

                p_frame_data->drawn_mesh_count = 0;

                u32 geometry_count = 0;
                geometry_render_data* geometries = darray_reserve_with_allocator(geometry_render_data, 512, &p_frame_data->allocator);

                // Query the scene for static meshes using the camera frustum
                if (!scene_mesh_render_data_query(
                        scene,
                        &camera_frustum,
                        current_camera->position,
                        p_frame_data,
                        &geometry_count, &geometries))
                {
                    BERROR("Failed to query scene pass meshes");
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
                        &camera_frustum,
                        current_camera->position,
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
                if (!scene_water_plane_query(scene, &camera_frustum, current_camera->position, p_frame_data, &water_plane_count, 0))
                    BERROR("Failed to query scene for water planes");
                water_plane** planes = darray_reserve_with_allocator(water_plane*, water_plane_count, &p_frame_data->allocator);
                if (!scene_water_plane_query(scene, &camera_frustum, current_camera->position, p_frame_data, &water_plane_count, &planes))
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

                // Do not run these passes if the scene is not loaded
                // graph->scene_pass.pass_data.do_execute = false;
                // graph->shadowmap_pass.pass_data.do_execute = false;
                forward_rendergraph_node_water_planes_set(node, p_frame_data, 0, 0);
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
                mat4 cam_view_proj = mat4_transposed(mat4_mul(camera_view_get(current_camera), shadow_dist_projection));

                // Pass over shadow map "camera" view and projection matrices (one per cascade)
                for (u32 c = 0; c < MAX_SHADOW_CASCADE_COUNT; c++)
                {
                    // NOTE: Each pass for cascades will need to do the following process.
                    // The only real difference will be that the near/far clips will be adjusted for each

                    // Get the world-space corners of the view frustum
                    vec4 corners[8] = {0};
                    frustum_corner_points_world_space(cam_view_proj, corners);

                    // Adjust the corners by pulling/pushing the near/far according to the current split
                    f32 split_dist = splits.elements[c];
                    for (u32 i = 0; i < 4; ++i)
                    {
                        // far - near
                        vec4 dist = vec4_sub(corners[i + 4], corners[i]);
                        corners[i + 4] = vec4_add(corners[i], vec4_mul_scalar(dist, split_dist));
                        corners[i] = vec4_add(corners[i], vec4_mul_scalar(dist, last_split_dist));
                    }

                    // Calculate the center of the camera's frustum by averaging the points.
                    // This is also used as the lookat point for the shadow "camera"
                    vec3 center = vec3_zero();
                    for (u32 i = 0; i < 8; ++i)
                        center = vec3_add(center, vec3_from_vec4(corners[i]));
                    center = vec3_div_scalar(center, 8.0f); // size
                    if (c == MAX_SHADOW_CASCADE_COUNT - 1)
                        culling_center = center;

                    // Get the furthest-out point from the center and use that as the extents
                    f32 radius = 0.0f;
                    for (u32 i = 0; i < 8; ++i)
                    {
                        f32 distance = vec3_distance(vec3_from_vec4(corners[i]), center);
                        radius = BMAX(radius, distance);
                    }
                    if (c == MAX_SHADOW_CASCADE_COUNT - 1)
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

                    // Generate lookat by moving along the opposite direction of the directional light by the minimum extents.
                    // This is negated because the directional light points "down" and the camera needs to be "up"
                    shadow_camera_positions[c] = vec3_sub(center, vec3_mul_scalar(light_dir, -extents.min.z));
                    shadow_camera_lookats[c] = mat4_look_at(shadow_camera_positions[c], center, vec3_up());

                    // Generate ortho projection based on extents
                    shadow_camera_projections[c] = mat4_orthographic(extents.min.x, extents.max.x, extents.min.y, extents.max.y, extents.min.z, extents.max.z - extents.min.z);

                    // Build out cascade data to set in shadow rg node
                    shadow_cascade_data cdata = {0};
                    cdata.cascade_index = c;
                    cdata.split_depth = (near + split_dist * clip_range) * 1.0f;
                    cdata.view = shadow_camera_lookats[c];
                    cdata.projection = shadow_camera_projections[c];
                    shadow_rendergraph_node_cascade_data_set(node, cdata, c);

                    last_split_dist = split_dist;
                }

                // Gather the geometries to be rendered
                // Note that this only needs to happen once, since all geometries visible by the furthest-out cascase
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
                camera_view_get(current_camera),
                camera_position_get(current_camera),
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

            // TODO: Move this to the scene
            // HACK: Inject raycast debug geometries into scene pass data
            // FIXME: Add this to the debug node below
            u32 line_count = darray_length(state->test_lines);
            for (u32 i = 0; i < line_count; ++i)
            {
                geometry_render_data rd = {0};
                rd.model = xform_world_get(state->test_lines[i].xform);
                geometry* g = &state->test_lines[i].geo;
                rd.material = g->material;
                rd.vertex_count = g->vertex_count;
                rd.vertex_buffer_offset = g->vertex_buffer_offset;
                rd.index_count = g->index_count;
                rd.index_buffer_offset = g->index_buffer_offset;
                rd.unique_id = INVALID_ID_U16;
                darray_push(debug_geometries, rd);
                debug_geometry_count++;
            }
            u32 box_count = darray_length(state->test_boxes);
            for (u32 i = 0; i < box_count; ++i)
            {
                geometry_render_data rd = {0};
                rd.model = xform_world_get(state->test_boxes[i].xform);
                geometry* g = &state->test_boxes[i].geo;
                rd.material = g->material;
                rd.vertex_count = g->vertex_count;
                rd.vertex_buffer_offset = g->vertex_buffer_offset;
                rd.index_count = g->index_count;
                rd.index_buffer_offset = g->index_buffer_offset;
                rd.unique_id = INVALID_ID_U16;
                darray_push(debug_geometries, rd);
                debug_geometry_count++;
            }

            // TODO: set geometries
            if (!debug_rendergraph_node_debug_geometries_set(node, p_frame_data, debug_geometry_count, debug_geometries))
            {
                // NOTE: Not going to abort the whole graph for this failure, but will bleat about it
                BERROR("Failed to set geometries for debug rendergraph node");
            }
        }
        else if (strings_equali(node->name, "editor_gizmo"))
        {
            editor_gizmo_rendergraph_node_viewport_set(node, state->world_viewport);
            editor_gizmo_rendergraph_node_view_projection_set(
                node,
                camera_view_get(current_camera),
                camera_position_get(current_camera),
                current_viewport->projection);
            if (!editor_gizmo_rendergraph_node_gizmo_set(node, &state->gizmo))
            {
                // NOTE: Not going to abort the whole graph for this failure, but will bleat about it loudly
                BERROR("Failed to set gizmo for editor_gizmo rendergraph node");
            }

            // Only draw if loaded
            editor_gizmo_rendergraph_node_enabled_set(node, scene->state == SCENE_STATE_LOADED);
        }
    }

    bclock_update(&state->prepare_clock);
    return true;
}

b8 application_render_frame(struct application* game_inst, struct frame_data* p_frame_data)
{
    // Start the frame
    testbed_game_state* state = (testbed_game_state*)game_inst->state;
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

void application_on_window_resize(struct application* game_inst, const struct bwindow* window)
{
    if (!game_inst->state)
        return;

    testbed_game_state* state = (testbed_game_state*)game_inst->state;

    state->width = window->width;
    state->height = window->height;
    if (!window->width || !window->height)
        return;

    // f32 half_width = state->width * 0.5f;

    // Resize viewports
    // World Viewport (right side)
    rect_2d world_vp_rect = vec4_create(0.0f, 0.0f, state->width, state->height);
    viewport_resize(&state->world_viewport, world_vp_rect);

    // UI Viewport
    rect_2d ui_vp_rect = vec4_create(0.0f, 0.0f, state->width, state->height);
    viewport_resize(&state->ui_viewport, ui_vp_rect);

    // World viewport 2
    rect_2d world_vp_rect2 = vec4_create(0.0f, 0.0f, state->width, state->height);
    viewport_resize(&state->world_viewport2, world_vp_rect2);

    // TODO: temp
    // Move debug text to new bottom of screen
    // FIXME: This should be handled by the standard ui system resize event handler (that doesn't exist yet)
    sui_control_position_set(state->sui_state, &state->test_text, vec3_create(20, state->height - 95, 0));
    sui_control_position_set(state->sui_state, &state->test_text_black, vec3_create(21, state->height - 94, 0));
    // TODO: end temp
}

void application_shutdown(struct application* game_inst)
{
    testbed_game_state* state = (testbed_game_state*)game_inst->state;
    state->running = false;

    if (state->main_scene.state == SCENE_STATE_LOADED)
    {
        BDEBUG("Unloading scene...");

        scene_unload(&state->main_scene, true);
        clear_debug_objects(game_inst);

        BDEBUG("Done");
    }

    rendergraph_destroy(&state->forward_graph);

    // Destroy ui texts
    debug_console_unload(&state->debug_console);
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

static b8 game_on_bvar_changed(u16 code, void* sender, void* listener_inst, event_context context)
{
    if (code == EVENT_CODE_BVAR_CHANGED)
    {
        bvar_change* change = context.data.custom_data.data;
        if (strings_equali("vsync", change->name))
        {
            toggle_vsync();
            return true;
        }
    }
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
        event_register(EVENT_CODE_DEBUG5, game_inst, game_on_debug_event);
        event_register(EVENT_CODE_OBJECT_HOVER_ID_CHANGED, game_inst, game_on_event);
        event_register(EVENT_CODE_SET_RENDER_MODE, game_inst, game_on_event);
        event_register(EVENT_CODE_BUTTON_RELEASED, game_inst->state, game_on_button);
        event_register(EVENT_CODE_MOUSE_MOVED, game_inst->state, game_on_mouse_move);
        event_register(EVENT_CODE_MOUSE_DRAG_BEGIN, game_inst->state, game_on_drag);
        event_register(EVENT_CODE_MOUSE_DRAG_END, game_inst->state, game_on_drag);
        event_register(EVENT_CODE_MOUSE_DRAGGED, game_inst->state, game_on_drag);
        // TODO: end temp

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

    event_unregister(EVENT_CODE_BVAR_CHANGED, 0, game_on_bvar_changed);
}

static b8 load_main_scene(struct application* game_inst)
{
    testbed_game_state* state = (testbed_game_state*)game_inst->state;

    // Load config file
    // TODO: clean up resource
    resource scene_resource;
    if (!resource_system_load("test_scene", RESOURCE_TYPE_scene, 0, &scene_resource))
    {
        BERROR("Failed to load scene file, check logs");
        return false;
    }

    scene_config* scene_cfg = (scene_config*)scene_resource.data;
    scene_cfg->resource_name = string_duplicate(scene_resource.name);
    scene_cfg->resource_full_path = string_duplicate(scene_resource.full_path);

    // Create the scene
    scene_flags scene_load_flags = 0;
    /* scene_load_flags |= SCENE_FLAG_READONLY;  // NOTE: to enable "editor mode", turn this flag off. */
    if (!scene_create(scene_cfg, scene_load_flags, &state->main_scene))
    {
        BERROR("Failed to create main scene");
        return false;
    }

    // Initialize
    if (!scene_initialize(&state->main_scene))
    {
        BERROR("Failed initialize main scene, aborting game");
        return false;
    }

    state->p_light_1 = 0;

    return scene_load(&state->main_scene);
}

static b8 save_main_scene(struct application* game_inst)
{
    if (!game_inst)
        return false;

    testbed_game_state* state = (testbed_game_state*)game_inst->state;

    return scene_save(&state->main_scene);
}
