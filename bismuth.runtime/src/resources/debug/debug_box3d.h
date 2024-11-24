#pragma once

#include "defines.h"
#include "identifiers/identifier.h"
#include "identifiers/bhandle.h"
#include "math/geometry.h"
#include "math/math_types.h"

typedef struct debug_box3d
{
    identifier id;
    bname name;
    vec3 size;
    vec4 color;
    bhandle xform;
    bhandle parent_xform;

    b8 is_dirty;

    bgeometry geometry;
} debug_box3d;

struct frame_data;

BAPI b8 debug_box3d_create(vec3 size, bhandle parent_xform, debug_box3d* out_box);
BAPI void debug_box3d_destroy(debug_box3d* box);

BAPI void debug_box3d_parent_set(debug_box3d* box, bhandle parent_xform);
BAPI void debug_box3d_color_set(debug_box3d* box, vec4 color);
BAPI void debug_box3d_extents_set(debug_box3d* box, extents_3d extents);
BAPI void debug_box3d_points_set(debug_box3d *box, vec3 points[8]);

BAPI void debug_box3d_render_frame_prepare(debug_box3d* box, const struct frame_data* p_frame_data);

BAPI b8 debug_box3d_initialize(debug_box3d* box);
BAPI b8 debug_box3d_load(debug_box3d* box);
BAPI b8 debug_box3d_unload(debug_box3d* box);

BAPI b8 debug_box3d_update(debug_box3d* box);
