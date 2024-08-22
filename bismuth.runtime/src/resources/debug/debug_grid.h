#include "defines.h"
#include "identifiers/identifier.h"
#include "math/geometry.h"
#include "math/math_types.h"
#include "resources/resource_types.h"

typedef enum debug_grid_orientation
{
    DEBUG_GRID_ORIENTATION_XZ = 0,
    DEBUG_GRID_ORIENTATION_XY = 1,
    DEBUG_GRID_ORIENTATION_YZ = 2
} debug_grid_orientation;

typedef struct debug_grid_config
{
    char *name;
    debug_grid_orientation orientation;
    u32 tile_count_dim_0;
    u32 tile_count_dim_1;
    f32 tile_scale;

    b8 use_third_axis;

    u32 vertex_length;
    color_vertex_3d *vertices;
} debug_grid_config;

typedef struct debug_grid
{
    identifier id;
    char *name;
    debug_grid_orientation orientation;
    u32 tile_count_dim_0;
    u32 tile_count_dim_1;
    f32 tile_scale;

    b8 use_third_axis;

    extents_3d extents;
    vec3 origin;

    u32 vertex_count;
    color_vertex_3d *vertices;

    geometry geo;
} debug_grid;

BAPI b8 debug_grid_create(const debug_grid_config *config, debug_grid *out_grid);
BAPI void debug_grid_destroy(debug_grid *grid);

BAPI b8 debug_grid_initialize(debug_grid *grid);
BAPI b8 debug_grid_load(debug_grid *grid);
BAPI b8 debug_grid_unload(debug_grid *grid);

BAPI b8 debug_grid_update(debug_grid *grid);
