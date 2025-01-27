#pragma once

#include "defines.h"
#include "identifiers/identifier.h"
#include "identifiers/bhandle.h"
#include "math/geometry.h"
#include "math/math_types.h"

typedef struct debug_sphere3d
{
    identifier id;
    bname name;
    f32 radius;
    vec4 color;
    bhandle xform;
    bhandle parent_xform;

    b8 is_dirty;

    bgeometry geometry;
} debug_sphere3d;

struct frame_data;

BAPI b8 debug_sphere3d_create(f32 radius, vec4 color, bhandle parent_xform, debug_sphere3d* out_sphere);
BAPI void debug_sphere3d_destroy(debug_sphere3d* sphere);

BAPI void debug_sphere3d_parent_set(debug_sphere3d* sphere, bhandle parent_xform);
BAPI void debug_sphere3d_color_set(debug_sphere3d* sphere, vec4 color);

BAPI void debug_sphere3d_render_frame_prepare(debug_sphere3d* sphere, const struct frame_data* p_frame_data);

BAPI b8 debug_sphere3d_initialize(debug_sphere3d* sphere);
BAPI b8 debug_sphere3d_load(debug_sphere3d* sphere);
BAPI b8 debug_sphere3d_unload(debug_sphere3d* sphere);

BAPI b8 debug_sphere3d_update(debug_sphere3d* sphere);
