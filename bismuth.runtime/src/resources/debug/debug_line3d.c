#include "debug_line3d.h"

#include "identifiers/identifier.h"
#include "identifiers/bhandle.h"
#include "memory/bmemory.h"
#include "math/bmath.h"
#include "renderer/renderer_frontend.h"
#include "systems/xform_system.h"

static void recalculate_points(debug_line3d* line);
static void update_vert_color(debug_line3d* line);

b8 debug_line3d_create(vec3 point_0, vec3 point_1, b_handle parent_xform, debug_line3d* out_line)
{
    if (!out_line)
        return false;
    out_line->vertex_count = 0;
    out_line->vertices = 0;
    out_line->xform = xform_create();
    out_line->xform_parent = parent_xform;
    out_line->point_0 = point_0;
    out_line->point_1 = point_1;
    out_line->id = identifier_create();
    out_line->color = vec4_one();  // white

    out_line->geo.id = INVALID_ID;
    out_line->geo.generation = INVALID_ID_U16;
    out_line->is_dirty = true;

    return true;
}

void debug_line3d_destroy(debug_line3d* line)
{
    // TODO: zero out
    line->id.uniqueid = INVALID_ID_U64;
}

void debug_line3d_parent_set(debug_line3d* line, b_handle parent_xform)
{
    if (line)
        line->xform_parent = parent_xform;
}

void debug_line3d_color_set(debug_line3d* line, vec4 color)
{
    if (line)
    {
        if (color.a == 0)
            color.a = 1.0f;
        line->color = color;
        if (line->geo.generation != INVALID_ID_U16 && line->vertex_count && line->vertices)
        {
            update_vert_color(line);
            line->is_dirty = true;
        }
    }
}

void debug_line3d_points_set(debug_line3d* line, vec3 point_0, vec3 point_1)
{
    if (line)
    {
        if (line->geo.generation != INVALID_ID_U16 && line->vertex_count && line->vertices)
        {
            line->point_0 = point_0;
            line->point_1 = point_1;
            recalculate_points(line);
            line->is_dirty = true;
        }
    }
}

void debug_line3d_render_frame_prepare(debug_line3d* line, const struct frame_data* p_frame_data)
{
    if (!line || !line->is_dirty)
        return;

    // Upload the new vertex data
    renderer_geometry_vertex_update(&line->geo, 0, line->vertex_count, line->vertices, true);

    line->geo.generation++;

    // Roll this over to zero so we don't lock ourselves out of updating
    if (line->geo.generation == INVALID_ID_U16)
        line->geo.generation = 0;

    line->is_dirty = false;
}

b8 debug_line3d_initialize(debug_line3d* line)
{
    if (!line)
        return false;

    line->vertex_count = 2;  // 2 points for a line
    line->vertices = ballocate(sizeof(color_vertex_3d) * line->vertex_count, MEMORY_TAG_ARRAY);

    recalculate_points(line);
    update_vert_color(line);

    return true;
}

b8 debug_line3d_load(debug_line3d* line)
{
    if (!renderer_geometry_create(&line->geo, sizeof(color_vertex_3d), line->vertex_count, line->vertices, 0, 0, 0))
        return false;
    // Send geometry off to renderer to be uploaded to the GPU
    if (!renderer_geometry_upload(&line->geo))
        return false;

    if (line->geo.generation == INVALID_ID_U16)
        line->geo.generation = 0;
    else
        line->geo.generation++;
    return true;
}

b8 debug_line3d_unload(debug_line3d* line)
{
    renderer_geometry_destroy(&line->geo);

    return true;
}

b8 debug_line3d_update(debug_line3d* line)
{
    return true;
}

static void recalculate_points(debug_line3d* line)
{
    if (line)
    {
        line->vertices[0].position = (vec4){line->point_0.x, line->point_0.y, line->point_0.z, 1.0f};
        line->vertices[1].position = (vec4){line->point_1.x, line->point_1.y, line->point_1.z, 1.0f};
    }
}

static void update_vert_color(debug_line3d* line)
{
    if (line)
    {
        if (line->vertex_count && line->vertices)
        {
            for (u32 i = 0; i < line->vertex_count; ++i)
                line->vertices[i].color = line->color;
        }
    }
}
