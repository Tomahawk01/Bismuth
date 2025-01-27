#include "debug_sphere3d.h"

#include "defines.h"
#include "identifiers/identifier.h"
#include "identifiers/bhandle.h"
#include "math/geometry.h"
#include "math/bmath.h"
#include "math/math_types.h"
#include "renderer/renderer_frontend.h"
#include "systems/xform_system.h"

static void update_vert_color(debug_sphere3d* sphere);

b8 debug_sphere3d_create(f32 radius, vec4 color, bhandle parent_xform, debug_sphere3d* out_sphere)
{
    if (!out_sphere)
        return false;

    out_sphere->xform = xform_create();
    out_sphere->parent_xform = parent_xform;
    // out_sphere->name // TODO: name?
    out_sphere->radius = radius;
    out_sphere->color = color;
    out_sphere->id = identifier_create();
    out_sphere->geometry.type = BGEOMETRY_TYPE_3D_STATIC_COLOR_ONLY;
    out_sphere->geometry.generation = INVALID_ID_U16;
    out_sphere->is_dirty = true;

    return true;
}

void debug_sphere3d_destroy(debug_sphere3d* sphere)
{
    // TODO: zero out, etc
    sphere->id.uniqueid = INVALID_ID_U64;
}

void debug_sphere3d_parent_set(debug_sphere3d* sphere, bhandle parent_xform)
{
    if (sphere)
        sphere->parent_xform = parent_xform;
}

void debug_sphere3d_color_set(debug_sphere3d* sphere, vec4 color)
{
    if (sphere)
    {
        if (color.a == 0)
            color.a = 1.0f;

        sphere->color = color;
        vec4_clamp(&sphere->color, 0.0f, 1.0f);
        if (sphere->geometry.generation != INVALID_ID_U16 && sphere->geometry.vertex_count && sphere->geometry.vertices)
        {
            update_vert_color(sphere);
            sphere->is_dirty = true;
        }
    }
}

void debug_sphere3d_render_frame_prepare(debug_sphere3d* sphere, const struct frame_data* p_frame_data)
{
    if (!sphere || !sphere->is_dirty)
        return;

    // Upload the new vertex data
    renderer_geometry_vertex_update(&sphere->geometry, 0, sphere->geometry.vertex_count, sphere->geometry.vertices, true);

    sphere->geometry.generation++;

    // Roll this over to zero so we don't lock ourselves out of updating
    if (sphere->geometry.generation == INVALID_ID_U16)
        sphere->geometry.generation = 0;

    sphere->is_dirty = false;
}

b8 debug_sphere3d_initialize(debug_sphere3d* sphere)
{
    if (!sphere)
        return false;

    sphere->geometry = geometry_generate_line_sphere3d(sphere->radius, 32, sphere->color, sphere->name);

    return true;
}

b8 debug_sphere3d_load(debug_sphere3d* sphere)
{
    // Send the geometry off to the renderer to be uploaded to the GPU
    if (!renderer_geometry_upload(&sphere->geometry))
        return false;

    if (sphere->geometry.generation == INVALID_ID_U16)
    {
        sphere->geometry.generation = 0;
    }
    else
    {
        sphere->geometry.generation++;
    }

    return true;
}

b8 debug_sphere3d_unload(debug_sphere3d* sphere)
{
    renderer_geometry_destroy(&sphere->geometry);
    return true;
}

b8 debug_sphere3d_update(debug_sphere3d* sphere)
{
    return true;
}

static void update_vert_color(debug_sphere3d* sphere)
{
    if (sphere)
    {
        if (sphere->geometry.vertex_count && sphere->geometry.vertices)
        {
            color_vertex_3d* verts = (color_vertex_3d*)sphere->geometry.vertices;
            for (u32 i = 0; i < sphere->geometry.vertex_count; ++i)
                verts[i].color = sphere->color;

            sphere->is_dirty = true;
        }
    }
}
