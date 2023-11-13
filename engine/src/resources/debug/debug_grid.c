#include "debug_grid.h"

#include "core/identifier.h"
#include "core/bstring.h"
#include "math/bmath.h"
#include "renderer/renderer_frontend.h"

b8 debug_grid_create(const debug_grid_config *config, debug_grid *out_grid)
{
    if (!config || !out_grid)
        return false;

    bzero_memory(out_grid, sizeof(debug_grid));

    out_grid->tile_count_dim_0 = config->tile_count_dim_0;
    out_grid->tile_count_dim_1 = config->tile_count_dim_1;
    out_grid->tile_scale = config->tile_scale;
    out_grid->name = string_duplicate(config->name);
    out_grid->orientation = config->orientation;
    out_grid->use_third_axis = config->use_third_axis;

    f32 max_0 = out_grid->tile_count_dim_0 * out_grid->tile_scale;
    f32 min_0 = -max_0;
    f32 max_1 = out_grid->tile_count_dim_1 * out_grid->tile_scale;
    f32 min_1 = -max_1;
    out_grid->extents.min = vec3_zero();
    out_grid->extents.max = vec3_zero();
    switch (out_grid->orientation)
    {
        default:
        case DEBUG_GRID_ORIENTATION_XZ:
            out_grid->extents.min.x = min_0;
            out_grid->extents.max.x = max_0;
            out_grid->extents.min.z = min_1;
            out_grid->extents.max.z = max_1;
            break;
        case DEBUG_GRID_ORIENTATION_XY:
            out_grid->extents.min.x = min_0;
            out_grid->extents.max.x = max_0;
            out_grid->extents.min.y = min_1;
            out_grid->extents.max.y = max_1;
            break;
        case DEBUG_GRID_ORIENTATION_YZ:
            out_grid->extents.min.y = min_0;
            out_grid->extents.max.y = max_0;
            out_grid->extents.min.z = min_1;
            out_grid->extents.max.z = max_1;
            break;
    }
    out_grid->origin = vec3_zero();
    out_grid->unique_id = identifier_aquire_new_id(out_grid);

    // 2 verts per line, 1 line per tile in each direction, plus one in the middle for each direction. Adding 2 more for third axis
    out_grid->vertex_count = ((out_grid->tile_count_dim_0 * 2 + 1) * 2) + ((out_grid->tile_count_dim_1 * 2 + 1) * 2) + 2;

    return true;
}

void debug_grid_destroy(debug_grid *grid)
{
    // TODO: zero out
    identifier_release_id(grid->unique_id);
}

b8 debug_grid_initialize(debug_grid *grid)
{
    if (!grid)
        return false;

    grid->vertices = ballocate(sizeof(color_vertex_3d) * grid->vertex_count, MEMORY_TAG_ARRAY);

    // Grid line lengths are the amount of spaces in the opposite direction
    i32 line_length_0 = grid->tile_count_dim_1 * grid->tile_scale;
    i32 line_length_1 = grid->tile_count_dim_0 * grid->tile_scale;
    i32 line_length_2 = line_length_0 > line_length_1 ? line_length_0 : line_length_1;

    u32 element_index_0, element_index_1, element_index_2;

    switch (grid->orientation)
    {
        default:
        case DEBUG_GRID_ORIENTATION_XZ:
            element_index_0 = 0;  // x
            element_index_1 = 2;  // z
            element_index_2 = 1;  // y
            break;
        case DEBUG_GRID_ORIENTATION_XY:
            element_index_0 = 0;  // x
            element_index_1 = 1;  // y
            element_index_2 = 2;  // z
            break;
        case DEBUG_GRID_ORIENTATION_YZ:
            element_index_0 = 1;  // y
            element_index_1 = 2;  // z
            element_index_2 = 0;  // x
            break;
    }

    // First axis line
    grid->vertices[0].position.elements[element_index_0] = -line_length_1;
    grid->vertices[0].position.elements[element_index_1] = 0;
    grid->vertices[1].position.elements[element_index_0] = line_length_1;
    grid->vertices[1].position.elements[element_index_1] = 0;
    grid->vertices[0].color.elements[element_index_0] = 1.0f;
    grid->vertices[0].color.a = 1.0f;
    grid->vertices[1].color.elements[element_index_0] = 1.0f;
    grid->vertices[1].color.a = 1.0f;

    // Second axis line
    grid->vertices[2].position.elements[element_index_0] = 0;
    grid->vertices[2].position.elements[element_index_1] = -line_length_0;
    grid->vertices[3].position.elements[element_index_0] = 0;
    grid->vertices[3].position.elements[element_index_1] = line_length_0;
    grid->vertices[2].color.elements[element_index_1] = 1.0f;
    grid->vertices[2].color.a = 1.0f;
    grid->vertices[3].color.elements[element_index_1] = 1.0f;
    grid->vertices[3].color.a = 1.0f;

    if (grid->use_third_axis)
    {
        // Third axis line
        grid->vertices[4].position.elements[element_index_0] = 0;
        grid->vertices[4].position.elements[element_index_2] = -line_length_2;
        grid->vertices[5].position.elements[element_index_0] = 0;
        grid->vertices[5].position.elements[element_index_2] = line_length_2;
        grid->vertices[4].color.elements[element_index_2] = 1.0f;
        grid->vertices[4].color.a = 1.0f;
        grid->vertices[5].color.elements[element_index_2] = 1.0f;
        grid->vertices[5].color.a = 1.0f;
    }

    vec4 alt_line_color = (vec4){1.0f, 1.0f, 1.0f, 0.5f};
    // calculate 4 lines at a time, 2 in each direction, min/max
    i32 j = 1;

    u32 start_index = grid->use_third_axis ? 6 : 4;

    for (u32 i = start_index; i < grid->vertex_count; i += 8)
    {
        // First line (max)
        grid->vertices[i + 0].position.elements[element_index_0] = j * grid->tile_scale;
        grid->vertices[i + 0].position.elements[element_index_1] = line_length_0;
        grid->vertices[i + 0].color = alt_line_color;
        grid->vertices[i + 1].position.elements[element_index_0] = j * grid->tile_scale;
        grid->vertices[i + 1].position.elements[element_index_1] = -line_length_0;
        grid->vertices[i + 1].color = alt_line_color;

        // Second line (min)
        grid->vertices[i + 2].position.elements[element_index_0] = -j * grid->tile_scale;
        grid->vertices[i + 2].position.elements[element_index_1] = line_length_0;
        grid->vertices[i + 2].color = alt_line_color;
        grid->vertices[i + 3].position.elements[element_index_0] = -j * grid->tile_scale;
        grid->vertices[i + 3].position.elements[element_index_1] = -line_length_0;
        grid->vertices[i + 3].color = alt_line_color;

        // Third line (max)
        grid->vertices[i + 4].position.elements[element_index_0] = -line_length_1;
        grid->vertices[i + 4].position.elements[element_index_1] = -j * grid->tile_scale;
        grid->vertices[i + 4].color = alt_line_color;
        grid->vertices[i + 5].position.elements[element_index_0] = line_length_1;
        grid->vertices[i + 5].position.elements[element_index_1] = -j * grid->tile_scale;
        grid->vertices[i + 5].color = alt_line_color;

        // Fourth line (min)
        grid->vertices[i + 6].position.elements[element_index_0] = -line_length_1;
        grid->vertices[i + 6].position.elements[element_index_1] = j * grid->tile_scale;
        grid->vertices[i + 6].color = alt_line_color;
        grid->vertices[i + 7].position.elements[element_index_0] = line_length_1;
        grid->vertices[i + 7].position.elements[element_index_1] = j * grid->tile_scale;
        grid->vertices[i + 7].color = alt_line_color;

        j++;
    }

    grid->geo.internal_id = INVALID_ID;

    return true;
}

b8 debug_grid_load(debug_grid *grid)
{
    // Send geometry off to renderer to be uploaded to the GPU
    if (!renderer_geometry_create(&grid->geo, sizeof(color_vertex_3d), grid->vertex_count, grid->vertices, 0, 0, 0))
        return false;
    return true;
}

b8 debug_grid_unload(debug_grid *grid)
{
    renderer_geometry_destroy(&grid->geo);

    grid->unique_id = INVALID_ID;

    return true;
}

b8 debug_grid_update(debug_grid *grid)
{
    return true;
}
