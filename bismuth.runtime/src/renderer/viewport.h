#pragma once

#include "math/math_types.h"
#include "renderer/renderer_types.h"

typedef struct viewport
{
    rect_2d rect;
    f32 fov;
    f32 near_clip;
    f32 far_clip;
    renderer_projection_matrix_type projection_matrix_type;
    mat4 projection;
} viewport;

BAPI b8 viewport_create(vec4 rect, f32 fov, f32 near_clip, f32 far_clip, renderer_projection_matrix_type projection_matrix_type, viewport* out_viewport);
BAPI void viewport_destroy(viewport* v);

BAPI void viewport_resize(viewport* v, vec4 rect);
