#pragma once

#include <application/application_types.h>
#include <defines.h>
#include <math/math_types.h>
#include <renderer/rendergraph.h>
#include <systems/camera_system.h>

#include "audio/audio_types.h"
#include "editor/editor_gizmo.h"
#include "identifiers/bhandle.h"
#include "renderer/viewport.h"
#include "resources/scene.h"

// TODO: temp
#include <core/keymap.h>
#include <resources/debug/debug_box3d.h>
#include <resources/skybox.h>
#include <standard_ui_system.h>
#include <systems/light_system.h>
#include <time/bclock.h>

#include "debug_console.h"

struct debug_line3d;
struct debug_box3d;

typedef struct selected_object
{
    bhandle xform_handle;
    bhandle node_handle;
    bhandle xform_parent_handle;
} selected_object;

typedef struct testbed_game_state
{
    b8 running;
    camera* world_camera;
    // TODO: temp
    camera* world_camera_2;

    u16 width, height;

    frustum camera_frustum;

    bclock update_clock;
    bclock prepare_clock;
    bclock render_clock;
    f64 last_update_elapsed;

    // TODO: temp
    rendergraph forward_graph;
    scene main_scene;
    b8 main_scene_unload_triggered;

    mesh meshes[10];

    point_light* p_light_1;

    mesh ui_meshes[10];
    sui_control test_text;
    sui_control test_text_black;
    sui_control test_sys_text;

    debug_console_state debug_console;

    u32 hovered_object_id;

    keymap console_keymap;

    u64 alloc_count;
    u64 prev_alloc_count;

    f32 forward_move_speed;
    f32 backward_move_speed;

    editor_gizmo gizmo;

    // Used for visualization of casts/collisions
    struct debug_line3d* test_lines;
    struct debug_box3d* test_boxes;

    viewport world_viewport;
    viewport ui_viewport;

    viewport world_viewport2;

    selected_object selection;
    b8 using_gizmo;

    u32 render_mode;

    struct bruntime_plugin* sui_plugin;
    struct standard_ui_plugin_state* sui_plugin_state;
    struct standard_ui_state* sui_state;

    struct sui_control test_panel;
    struct sui_control test_button;

    struct audio_file* test_audio_file;
    struct audio_file* test_loop_audio_file;
    struct audio_file* test_music;
    audio_emitter test_emitter;

    u32 proj_box_index;
    u32 cam_proj_line_indices[24];
    // TODO: end temp
} testbed_game_state;

typedef struct testbed_application_frame_data
{
    i32 dummy;
} testbed_application_frame_data;
