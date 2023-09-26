#include "game.h"

#include <core/logger.h>
#include <core/bmemory.h>
#include <core/bstring.h>
#include <core/input.h>
#include <core/event.h>
#include <math/bmath.h>
#include <renderer/renderer_types.inl>

b8 game_initialize(game* game_inst)
{
    BDEBUG("game_initialize() called!");

    game_state* state = ((game_state*)game_inst->state);

    state->world_camera = camera_system_get_default();
    camera_position_set(state->world_camera, (vec3){10.0f, 5.0f, 9.0f});

    return true;
}

b8 game_update(game* game_inst, f32 delta_time)
{
    static u64 alloc_count = 0;
    u64 prev_alloc_count = alloc_count;
    alloc_count = get_memory_alloc_count();
    if (input_is_key_up('M') && input_was_key_down('M'))
    {
        char* usage = get_memory_usage_str();
        BINFO(usage);
        string_free(usage);
        BDEBUG("Allocations: %llu (%llu this frame)", alloc_count, alloc_count - prev_alloc_count);
    }

    // TODO: temporary
    if (input_is_key_up('T') && input_was_key_down('T'))
    {
        BDEBUG("Swapping texture");
        event_context context = {};
        event_fire(EVENT_CODE_DEBUG0, game_inst, context);
    }
    // TODO: end temporary

    game_state* state = (game_state*)game_inst->state;
    static f32 camera_multiplier = 1.5f;

    // NOTE: temp hack to move camera around
    if (input_is_key_down(KEY_LEFT))
    {
        camera_yaw(state->world_camera, 1.0f * camera_multiplier * delta_time);
    }

    if (input_is_key_down(KEY_RIGHT))
    {
        camera_yaw(state->world_camera, -1.0f * camera_multiplier * delta_time);
    }

    if (input_is_key_down(KEY_UP))
    {
        camera_pitch(state->world_camera, 1.0f * camera_multiplier * delta_time);
    }

    if (input_is_key_down(KEY_DOWN))
    {
        camera_pitch(state->world_camera, -1.0f * camera_multiplier * delta_time);
    }

    static const f32 temp_move_speed = 50.0f;

    if (input_is_key_down('W'))
    {
        camera_move_forward(state->world_camera, temp_move_speed * delta_time);
    }

    if (input_is_key_down('S'))
    {
        camera_move_backward(state->world_camera, temp_move_speed * delta_time);
    }

    if (input_is_key_down('A'))
    {
        camera_move_left(state->world_camera, temp_move_speed * delta_time);
    }

    if (input_is_key_down('D'))
    {
        camera_move_right(state->world_camera, temp_move_speed * delta_time);
    }

    if (input_is_key_down('E'))
    {
        camera_move_up(state->world_camera, temp_move_speed * delta_time);
    }

    if (input_is_key_down('Q'))
    {
        camera_move_down(state->world_camera, temp_move_speed * delta_time);
    }

    // TODO: temporary
    // RENDERER DEBUG FUNCTIONS
    if (input_is_key_up('1') && input_was_key_down('1'))
    {
        event_context data = {};
        data.data.i32[0] = RENDERER_VIEW_MODE_LIGHTING;
        event_fire(EVENT_CODE_SET_RENDER_MODE, game_inst, data);
    }

    if (input_is_key_up('2') && input_was_key_down('2'))
    {
        event_context data = {};
        data.data.i32[0] = RENDERER_VIEW_MODE_NORMALS;
        event_fire(EVENT_CODE_SET_RENDER_MODE, game_inst, data);
    }

    if (input_is_key_up('0') && input_was_key_down('0'))
    {
        event_context data = {};
        data.data.i32[0] = RENDERER_VIEW_MODE_DEFAULT;
        event_fire(EVENT_CODE_SET_RENDER_MODE, game_inst, data);
    }

    // Bind key to load up some data
    if (input_is_key_up('L') && input_was_key_down('L'))
    {
        event_context context = {};
        event_fire(EVENT_CODE_DEBUG1, game_inst, context);
    }
    // TODO: end temporary

    return true;
}

b8 game_render(game* game_inst, f32 delta_time)
{
    return true;
}

void game_on_resize(game* game_inst, u32 width, u32 height)
{

}
