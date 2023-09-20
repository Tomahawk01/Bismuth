#include "game.h"

#include <core/logger.h>
#include <core/bmemory.h>
#include <core/input.h>
#include <core/event.h>
#include <math/bmath.h>

// NOTE: this should not be available outside the engine
#include <renderer/renderer_frontend.h>

void recalculate_view_matrix(game_state* state)
{
    if (state->camera_view_dirty)
    {
        mat4 rotation = mat4_euler_xyz(state->camera_euler.x, state->camera_euler.y, state->camera_euler.z);
        mat4 translation = mat4_translation(state->camera_position);

        state->view = mat4_mul(rotation, translation);
        state->view = mat4_inverse(state->view);

        state->camera_view_dirty = false;
    }
}

void camera_yaw(game_state* state, f32 amount)
{
    state->camera_euler.y += amount;
    state->camera_view_dirty = true;
}

void camera_pitch(game_state* state, f32 amount)
{
    state->camera_euler.x += amount;

    f32 limit = deg_to_rad(89.0f);
    state->camera_euler.x = BCLAMP(state->camera_euler.x, -limit, limit);

    state->camera_view_dirty = true;
}

b8 game_initialize(game* game_inst)
{
    BDEBUG("game_initialize() called!");

    game_state* state = ((game_state*)game_inst->state);

    state->camera_position = (vec3){0, 0, 30.0f};
    state->camera_euler = vec3_zero();

    state->view = mat4_translation(state->camera_position);
    state->view = mat4_inverse(state->view);
    state->camera_view_dirty = true;

    return true;
}

b8 game_update(game* game_inst, f32 delta_time)
{
    static u64 alloc_count = 0;
    u64 prev_alloc_count = alloc_count;
    alloc_count = get_memory_alloc_count();
    if (input_is_key_up('M') && input_was_key_down('M'))
        BDEBUG("Allocations: %llu (%llu this frame)", alloc_count, alloc_count - prev_alloc_count);

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
        camera_yaw(state, 1.0f * camera_multiplier * delta_time);
    }

    if (input_is_key_down(KEY_RIGHT))
    {
        camera_yaw(state, -1.0f * camera_multiplier * delta_time);
    }

    if (input_is_key_down(KEY_UP))
    {
        camera_pitch(state, 1.0f * camera_multiplier * delta_time);
    }

    if (input_is_key_down(KEY_DOWN))
    {
        camera_pitch(state, -1.0f * camera_multiplier * delta_time);
    }

    f32 temp_move_speed = 50.0f;
    vec3 velocity = vec3_zero();

    if (input_is_key_down('W'))
    {
        vec3 forward = mat4_forward(state->view);
        velocity = vec3_add(velocity, forward);
    }

    if (input_is_key_down('S'))
    {
        vec3 backward = mat4_backward(state->view);
        velocity = vec3_add(velocity, backward);
    }

    if (input_is_key_down('A'))
    {
        vec3 left = mat4_left(state->view);
        velocity = vec3_add(velocity, left);
    }

    if (input_is_key_down('D'))
    {
        vec3 right = mat4_right(state->view);
        velocity = vec3_add(velocity, right);
    }

    if (input_is_key_down('E'))
    {
        velocity.y += 0.5f;
    }
    
    if (input_is_key_down('Q'))
    {
        velocity.y -= 0.5f;
    }

    vec3 z = vec3_zero();
    if (!vec3_compare(z, velocity, 0.0002f))
    {
        vec3_normalize(&velocity);
        state->camera_position.x += velocity.x * temp_move_speed * delta_time;
        state->camera_position.y += velocity.y * temp_move_speed * delta_time;
        state->camera_position.z += velocity.z * temp_move_speed * delta_time;
        state->camera_view_dirty = true;
    }

    recalculate_view_matrix(state);

    // NOTE: this should not be available outside the engine
    renderer_set_view(state->view, state->camera_position);

    // TODO: temporary
    if (input_is_key_up('P') && input_was_key_down('P'))
    {
        BDEBUG(
            "Pos:[%.2f, %.2f, %.2f]",
            state->camera_position.x,
            state->camera_position.y,
            state->camera_position.z);
    }

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

    return true;
}

b8 game_render(game* game_inst, f32 delta_time)
{
    return true;
}

void game_on_resize(game* game_inst, u32 width, u32 height)
{

}
