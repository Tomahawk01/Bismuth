#include "defines.h"
#include "core/identifier.h"
#include "math/math_types.h"
#include "resources/resource_types.h"

typedef struct debug_box3d
{
    identifier id;
    char *name;
    vec3 size;
    vec4 color;
    transform xform;

    u32 vertex_count;
    color_vertex_3d *vertices;

    geometry geo;
} debug_box3d;

BAPI b8 debug_box3d_create(vec3 size, transform* parent, debug_box3d* out_box);
BAPI void debug_box3d_destroy(debug_box3d* box);

BAPI void debug_box3d_parent_set(debug_box3d* box, transform* parent);
BAPI void debug_box3d_color_set(debug_box3d* box, vec4 color);
BAPI void debug_box3d_extents_set(debug_box3d* box, extents_3d extents);

BAPI b8 debug_box3d_initialize(debug_box3d* box);
BAPI b8 debug_box3d_load(debug_box3d* box);
BAPI b8 debug_box3d_unload(debug_box3d* box);

BAPI b8 debug_box3d_update(debug_box3d* box);
