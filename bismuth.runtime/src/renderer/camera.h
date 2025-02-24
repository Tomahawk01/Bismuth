#pragma once

#include "defines.h"
#include "math/math_types.h"

typedef struct camera
{
    vec3 position;
    vec3 euler_rotation;
    b8 is_dirty;

    mat4 view_matrix;

    mat4 transform;
} camera;

BAPI camera camera_create(void);
BAPI camera camera_copy(camera source);
BAPI void camera_reset(camera* c);

BAPI vec3 camera_position_get(const camera* c);
BAPI void camera_position_set(camera* c, vec3 position);

BAPI vec3 camera_rotation_euler_get(const camera* c);
BAPI void camera_rotation_euler_set(camera* c, vec3 rotation);
/* BAPI void camera_rotation_set(camera* c, quat rotation); */
BAPI void camera_rotation_euler_set_radians(camera* c, vec3 rotation_radians);

BAPI mat4 camera_view_get(camera* c);
BAPI mat4 camera_inverse_view_get(camera* c);

BAPI vec3 camera_forward(camera* c);
BAPI vec3 camera_backward(camera* c);
BAPI vec3 camera_left(camera* c);
BAPI vec3 camera_right(camera* c);
BAPI vec3 camera_up(camera* c);

BAPI void camera_move_forward(camera* c, f32 amount);
BAPI void camera_move_backward(camera* c, f32 amount);
BAPI void camera_move_left(camera* c, f32 amount);
BAPI void camera_move_right(camera* c, f32 amount);
BAPI void camera_move_up(camera* c, f32 amount);
BAPI void camera_move_down(camera* c, f32 amount);

BAPI void camera_yaw(camera* c, f32 amount);
BAPI void camera_pitch(camera* c, f32 amount);
