#include "debug_line3d.h"

#include "identifiers/identifier.h"
#include "identifiers/bhandle.h"
#include "math/geometry.h"
#include "math/bmath.h"
#include "math/math_types.h"
#include "memory/bmemory.h"
#include "renderer/renderer_frontend.h"
#include "strings/bname.h"
#include "systems/xform_system.h"

static void recalculate_points(debug_line3d* line);
static void update_vert_color(debug_line3d* line);

b8 debug_line3d_create(vec3 point_0, vec3 point_1, bhandle parent_xform, debug_line3d* out_line)
{
    if (!out_line)
        return false;

    out_line->xform = xform_create();
    out_line->xform_parent = parent_xform;
    out_line->point_0 = point_0;
    out_line->point_1 = point_1;
    out_line->id = identifier_create();
    out_line->color = vec4_one();  // white

    out_line->geometry.type = BGEOMETRY_TYPE_3D_STATIC_COLOR_ONLY;
    out_line->geometry.generation = INVALID_ID_U16;
    out_line->is_dirty = true;

    return true;
}

void debug_line3d_destroy(debug_line3d* line)
{
    if (line)
    {
        geometry_destroy(&line->geometry);
        bzero_memory(line, sizeof(debug_line3d));
        line->id.uniqueid = INVALID_ID_U64;
    }
}

void debug_line3d_parent_set(debug_line3d* line, bhandle parent_xform)
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
        if (line->geometry.generation != INVALID_ID_U16 && line->geometry.vertex_count && line->geometry.vertices)
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
        if (line->geometry.generation != INVALID_ID_U16 && line->geometry.vertex_count && line->geometry.vertices)
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
    renderer_geometry_vertex_update(&line->geometry, 0, line->geometry.vertex_count, line->geometry.vertices, true);

    line->geometry.generation++;

    // Roll this over to zero so we don't lock ourselves out of updating
    if (line->geometry.generation == INVALID_ID_U16)
        line->geometry.generation = 0;

    line->is_dirty = false;
}

b8 debug_line3d_initialize(debug_line3d* line)
{
    if (!line)
        return false;

    line->geometry = geometry_generate_line3d(line->point_0, line->point_1, INVALID_BNAME);

    recalculate_points(line);
    update_vert_color(line);

    return true;
}

b8 debug_line3d_load(debug_line3d* line)
{
    // Send geometry off to renderer to be uploaded to the GPU
    if (!renderer_geometry_upload(&line->geometry))
        return false;

    line->geometry.generation++;
    return true;
}

b8 debug_line3d_unload(debug_line3d* line)
{
    renderer_geometry_destroy(&line->geometry);

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
        ((color_vertex_3d*)line->geometry.vertices)[0].position = vec4_from_vec3(line->point_0, 1.0f);
        ((color_vertex_3d*)line->geometry.vertices)[1].position = vec4_from_vec3(line->point_1, 1.0f);
    }
}

static void update_vert_color(debug_line3d* line)
{
    if (line)
    {
        if (line->geometry.vertex_count && line->geometry.vertices)
        {
            for (u32 i = 0; i < line->geometry.vertex_count; ++i)
                ((color_vertex_3d*)line->geometry.vertices)[i].color = line->color;
        }
    }
}
