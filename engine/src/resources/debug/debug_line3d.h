#pragma once

#include "defines.h"
#include "math/math_types.h"
#include "resources/resource_types.h"

typedef struct debug_line3d
{
    u32 unique_id;
    char *name;
    vec3 point_0;
    vec3 point_1;
    vec4 color;
    transform xform;

    u32 vertex_count;
    color_vertex_3d *vertices;

    geometry geo;
} debug_line3d;

BAPI b8 debug_line3d_create(vec3 point_0, vec3 point_1, transform *parent, debug_line3d *out_line);
BAPI void debug_line3d_destroy(debug_line3d *line);

BAPI void debug_line3d_parent_set(debug_line3d *line, transform *parent);
BAPI void debug_line3d_color_set(debug_line3d *line, vec4 color);
BAPI void debug_line3d_points_set(debug_line3d *line, vec3 point_0, vec3 point_1);

BAPI b8 debug_line3d_initialize(debug_line3d *line);
BAPI b8 debug_line3d_load(debug_line3d *line);
BAPI b8 debug_line3d_unload(debug_line3d *line);

BAPI b8 debug_line3d_update(debug_line3d *line);
