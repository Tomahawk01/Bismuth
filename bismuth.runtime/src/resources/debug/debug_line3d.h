#pragma once

#include "defines.h"
#include "identifiers/identifier.h"
#include "identifiers/bhandle.h"
#include "math/geometry.h"
#include "math/math_types.h"

typedef struct debug_line3d
{
    identifier id;
    char* name;
    vec3 point_0;
    vec3 point_1;
    vec4 color;
    b_handle xform;
    b_handle xform_parent;
    b8 is_dirty;

    bgeometry geometry;
} debug_line3d;

struct frame_data;

BAPI b8 debug_line3d_create(vec3 point_0, vec3 point_1, b_handle parent_xform, debug_line3d* out_line);
BAPI void debug_line3d_destroy(debug_line3d *line);

BAPI void debug_line3d_parent_set(debug_line3d* line, b_handle parent_xform);
BAPI void debug_line3d_color_set(debug_line3d* line, vec4 color);
BAPI void debug_line3d_points_set(debug_line3d* line, vec3 point_0, vec3 point_1);

BAPI void debug_line3d_render_frame_prepare(debug_line3d* line, const struct frame_data* p_frame_data);

BAPI b8 debug_line3d_initialize(debug_line3d* line);
BAPI b8 debug_line3d_load(debug_line3d* line);
BAPI b8 debug_line3d_unload(debug_line3d* line);

BAPI b8 debug_line3d_update(debug_line3d* line);
