#include "debug_box3d.h"

#include "defines.h"
#include "identifiers/identifier.h"
#include "identifiers/bhandle.h"
#include "math/geometry.h"
#include "math/bmath.h"
#include "math/math_types.h"
#include "renderer/renderer_frontend.h"
#include "systems/xform_system.h"

static void update_vert_color(debug_box3d* box);

b8 debug_box3d_create(vec3 size, b_handle parent_xform, debug_box3d* out_box)
{
    if (!out_box)
        return false;
    out_box->xform = xform_create();
    out_box->parent_xform = parent_xform;
    out_box->size = size;
    out_box->id = identifier_create();
    out_box->color = vec4_one();  // white

    out_box->geometry.generation = INVALID_ID_U16;
    out_box->is_dirty = true;

    return true;
}

void debug_box3d_destroy(debug_box3d* box)
{
    // TODO: zero out
    box->id.uniqueid = INVALID_ID_U64;
}

void debug_box3d_parent_set(debug_box3d* box, b_handle parent_xform)
{
    if (box)
        box->parent_xform = parent_xform;
}

void debug_box3d_color_set(debug_box3d* box, vec4 color)
{
    if (box)
    {
        if (color.a == 0)
            color.a = 1.0f;
        box->color = color;
        if (box->geometry.generation != INVALID_ID_U16 && box->geometry.vertex_count && ((vertex_3d*)box->geometry.vertices))
        {
            update_vert_color(box);
            box->is_dirty = true;
        }
    }
}

void debug_box3d_extents_set(debug_box3d* box, extents_3d extents)
{
    if (box)
    {
        if (box->geometry.generation != INVALID_ID_U16 && box->geometry.vertex_count && ((vertex_3d*)box->geometry.vertices))
        {
            geometry_recalculate_line_box3d_by_extents(&box->geometry, extents);
            box->is_dirty = true;
        }
    }
}

void debug_box3d_points_set(debug_box3d* box, vec3 points[8])
{
    if (box && points)
    {
        if (box->geometry.generation != INVALID_ID_U16 && box->geometry.vertex_count && box->geometry.vertices)
        {
            geometry_recalculate_line_box3d_by_points(&box->geometry, points);

            box->is_dirty = true;
        }
    }
}

void debug_box3d_render_frame_prepare(debug_box3d* box, const struct frame_data* p_frame_data)
{
    if (!box || !box->is_dirty)
        return;

    // Upload the new vertex data
    renderer_geometry_vertex_update(&box->geometry, 0, box->geometry.vertex_count, box->geometry.vertices, true);

     box->geometry.generation++;

    // Roll this over to zero so we don't lock ourselves out of updating
    if (box->geometry.generation == INVALID_ID_U16)
        box->geometry.generation = 0;

    box->is_dirty = false;
}

b8 debug_box3d_initialize(debug_box3d* box)
{
    if (!box)
        return false;

    box->geometry = geometry_generate_line_box3d(box->size, box->name);

    update_vert_color(box);

    return true;
}

b8 debug_box3d_load(debug_box3d* box)
{
    // Send geometry off to renderer to be uploaded to the GPU
    if (!renderer_geometry_upload(&box->geometry))
        return false;

    if (box->geometry.generation == INVALID_ID_U16)
        box->geometry.generation = 0;
    else
        box->geometry.generation++;
    return true;
}

b8 debug_box3d_unload(debug_box3d* box)
{
    renderer_geometry_destroy(&box->geometry);

    return true;
}

b8 debug_box3d_update(debug_box3d* box)
{
    return true;
}

static void update_vert_color(debug_box3d* box)
{
    if (box)
    {
        if (box->geometry.vertex_count && box->geometry.vertices)
        {
            for (u32 i = 0; i < box->geometry.vertex_count; ++i)
                ((vertex_3d*)box->geometry.vertices)[i].color = box->color;
        }
    }
}
