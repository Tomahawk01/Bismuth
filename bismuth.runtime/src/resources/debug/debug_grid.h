#include "defines.h"
#include "identifiers/identifier.h"
#include "math/geometry.h"
#include "math/math_types.h"

typedef struct debug_grid_config
{
    bname name;
    grid_orientation orientation;
    u32 segment_count_dim_0;
    u32 segment_count_dim_1;
    f32 segment_size;

    b8 use_third_axis;
} debug_grid_config;

typedef struct debug_grid
{
    identifier id;
    bname name;
    grid_orientation orientation;
    u32 segment_count_dim_0;
    u32 segment_count_dim_1;
    f32 segment_size;

    b8 use_third_axis;

    vec3 origin;

    bgeometry geometry;
} debug_grid;

BAPI b8 debug_grid_create(const debug_grid_config *config, debug_grid *out_grid);
BAPI void debug_grid_destroy(debug_grid *grid);

BAPI b8 debug_grid_initialize(debug_grid *grid);
BAPI b8 debug_grid_load(debug_grid *grid);
BAPI b8 debug_grid_unload(debug_grid *grid);

BAPI b8 debug_grid_update(debug_grid *grid);
