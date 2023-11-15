#include "editor_gizmo.h"

#include "core/bmemory.h"
#include "math/math_types.h"

#include <core/logger.h>
#include <defines.h>
#include <math/bmath.h>
#include <math/transform.h>
#include <renderer/renderer_frontend.h>

static void create_gizmo_mode_none(editor_gizmo* gizmo);
static void create_gizmo_mode_move(editor_gizmo* gizmo);
static void create_gizmo_mode_scale(editor_gizmo* gizmo);
static void create_gizmo_mode_rotate(editor_gizmo* gizmo);

b8 editor_gizmo_create(editor_gizmo* out_gizmo)
{
    if (!out_gizmo)
    {
        BERROR("Unable to create gizmo with an invalid out pointer");
        return false;
    }

    out_gizmo->mode = EDITOR_GIZMO_MODE_NONE;
    out_gizmo->xform = transform_create();

    // Initialize default values for all modes
    for (u32 i = 0; i < EDITOR_GIZMO_MODE_MAX + 1; ++i)
    {
        out_gizmo->mode_data[i].vertex_count = 0;
        out_gizmo->mode_data[i].vertices = 0;

        out_gizmo->mode_data[i].index_count = 0;
        out_gizmo->mode_data[i].indices = 0;
    }

    return true;
}

void editor_gizmo_destroy(editor_gizmo* gizmo)
{
    if (gizmo)
    {
    }
}

b8 editor_gizmo_initialize(editor_gizmo* gizmo)
{
    if (!gizmo)
        return false;

    gizmo->mode = EDITOR_GIZMO_MODE_NONE;

    create_gizmo_mode_none(gizmo);
    create_gizmo_mode_move(gizmo);
    create_gizmo_mode_scale(gizmo);
    create_gizmo_mode_rotate(gizmo);

    return true;
}

b8 editor_gizmo_load(editor_gizmo* gizmo)
{
    if (!gizmo)
        return false;

    for (u32 i = 0; i < EDITOR_GIZMO_MODE_MAX + 1; ++i)
    {
        if (!renderer_geometry_create(&gizmo->mode_data[i].geo, sizeof(color_vertex_3d), gizmo->mode_data[i].vertex_count, gizmo->mode_data[i].vertices, 0, 0, 0))
        {
            BERROR("Failed to create gizmo geometry type: '%u'", i);
            return false;
        }
        if (!renderer_geometry_upload(&gizmo->mode_data[i].geo))
        {
            BERROR("Failed to upload gizmo geometry type: '%u'", i);
            return false;
        }
        if (gizmo->mode_data[i].geo.generation == INVALID_ID_U16)
            gizmo->mode_data[i].geo.generation = 0;
        else
            gizmo->mode_data[i].geo.generation++;
    }

    return true;
}

b8 editor_gizmo_unload(editor_gizmo* gizmo)
{
    if (gizmo)
    {
    }
    return true;
}

void editor_gizmo_update(editor_gizmo* gizmo)
{
    if (gizmo)
    {
    }
}

void editor_gizmo_mode_set(editor_gizmo* gizmo, editor_gizmo_mode mode)
{
    if (gizmo)
        gizmo->mode = mode;
}

static void create_gizmo_mode_none(editor_gizmo* gizmo)
{
    editor_gizmo_mode_data* data = &gizmo->mode_data[EDITOR_GIZMO_MODE_NONE];

    data->vertex_count = 6;  // 2 per line, 3 lines
    data->vertices = ballocate(sizeof(color_vertex_3d) * data->vertex_count, MEMORY_TAG_ARRAY);
    vec4 grey = (vec4){0.5f, 0.5f, 0.5f, 1.0f};

    // x
    data->vertices[0].color = grey;  // First vert is at origin, no pos needed
    data->vertices[1].color = grey;
    data->vertices[1].position.x = 1.0f;

    // y
    data->vertices[2].color = grey;  // First vert is at origin, no pos needed
    data->vertices[3].color = grey;
    data->vertices[3].position.y = 1.0f;

    // z
    data->vertices[4].color = grey;  // First vert is at origin, no pos needed
    data->vertices[5].color = grey;
    data->vertices[5].position.z = 1.0f;
}

static void create_gizmo_mode_move(editor_gizmo* gizmo)
{
    editor_gizmo_mode_data* data = &gizmo->mode_data[EDITOR_GIZMO_MODE_MOVE];

    data->vertex_count = 18;  // 2 per line, 3 lines + 6 lines
    data->vertices = ballocate(sizeof(color_vertex_3d) * data->vertex_count, MEMORY_TAG_ARRAY);

    vec4 r = (vec4){1.0f, 0.0f, 0.0f, 1.0f};
    vec4 g = (vec4){0.0f, 1.0f, 0.0f, 1.0f};
    vec4 b = (vec4){0.0f, 0.0f, 1.0f, 1.0f};
    // x
    data->vertices[0].color = r;
    data->vertices[0].position.x = 0.2f;
    data->vertices[1].color = r;
    data->vertices[1].position.x = 1.0f;

    // y
    data->vertices[2].color = g;
    data->vertices[2].position.y = 0.2f;
    data->vertices[3].color = g;
    data->vertices[3].position.y = 1.0f;

    // z
    data->vertices[4].color = b;
    data->vertices[4].position.z = 0.2f;
    data->vertices[5].color = b;
    data->vertices[5].position.z = 1.0f;

    // x "box" lines
    data->vertices[6].color = r;
    data->vertices[6].position.x = 0.4f;
    data->vertices[7].color = r;
    data->vertices[7].position.x = 0.4f;
    data->vertices[7].position.y = 0.4f;

    data->vertices[8].color = r;
    data->vertices[8].position.x = 0.4f;
    data->vertices[9].color = r;
    data->vertices[9].position.x = 0.4f;
    data->vertices[9].position.z = 0.4f;

    // y "box" lines
    data->vertices[10].color = g;
    data->vertices[10].position.y = 0.4f;
    data->vertices[11].color = g;
    data->vertices[11].position.y = 0.4f;
    data->vertices[11].position.z = 0.4f;

    data->vertices[12].color = g;
    data->vertices[12].position.y = 0.4f;
    data->vertices[13].color = g;
    data->vertices[13].position.y = 0.4f;
    data->vertices[13].position.x = 0.4f;

    // z "box" lines
    data->vertices[14].color = b;
    data->vertices[14].position.z = 0.4f;
    data->vertices[15].color = b;
    data->vertices[15].position.z = 0.4f;
    data->vertices[15].position.y = 0.4f;

    data->vertices[16].color = b;
    data->vertices[16].position.z = 0.4f;
    data->vertices[17].color = b;
    data->vertices[17].position.z = 0.4f;
    data->vertices[17].position.x = 0.4f;
}

static void create_gizmo_mode_scale(editor_gizmo* gizmo)
{
    editor_gizmo_mode_data* data = &gizmo->mode_data[EDITOR_GIZMO_MODE_SCALE];

    data->vertex_count = 12;  // 2 per line, 3 lines + 3 lines
    data->vertices = ballocate(sizeof(color_vertex_3d) * data->vertex_count, MEMORY_TAG_ARRAY);

    vec4 r = (vec4){1.0f, 0.0f, 0.0f, 1.0f};
    vec4 g = (vec4){0.0f, 1.0f, 0.0f, 1.0f};
    vec4 b = (vec4){0.0f, 0.0f, 1.0f, 1.0f};

    // x
    data->vertices[0].color = r;  // First vert is at origin, no pos needed
    data->vertices[1].color = r;
    data->vertices[1].position.x = 1.0f;

    // y
    data->vertices[2].color = g;  // First vert is at origin, no pos needed
    data->vertices[3].color = g;
    data->vertices[3].position.y = 1.0f;

    // z
    data->vertices[4].color = b;  // First vert is at origin, no pos needed
    data->vertices[5].color = b;
    data->vertices[5].position.z = 1.0f;

    // x/y outer line
    data->vertices[6].position.x = 0.8f;
    data->vertices[6].color = r;
    data->vertices[7].position.y = 0.8f;
    data->vertices[7].color = g;

    // z/y outer line
    data->vertices[8].position.z = 0.8f;
    data->vertices[8].color = b;
    data->vertices[9].position.y = 0.8f;
    data->vertices[9].color = g;

    // x/z outer line
    data->vertices[10].position.x = 0.8f;
    data->vertices[10].color = r;
    data->vertices[11].position.z = 0.8f;
    data->vertices[11].color = b;
}

static void create_gizmo_mode_rotate(editor_gizmo* gizmo)
{
    editor_gizmo_mode_data* data = &gizmo->mode_data[EDITOR_GIZMO_MODE_ROTATE];
    const u8 segments = 32;
    const f32 radius = 1.0f;

    data->vertex_count = 12 + (segments * 2 * 3);  // 2 per line, 3 lines + 3 lines
    data->vertices = ballocate(sizeof(color_vertex_3d) * data->vertex_count, MEMORY_TAG_ARRAY);

    vec4 r = (vec4){1.0f, 0.0f, 0.0f, 1.0f};
    vec4 g = (vec4){0.0f, 1.0f, 0.0f, 1.0f};
    vec4 b = (vec4){0.0f, 0.0f, 1.0f, 1.0f};

    // Start with the center, draw small axes.
    // x
    data->vertices[0].color = r;  // First vert is at origin, no pos needed
    data->vertices[1].color = r;
    data->vertices[1].position.x = 0.2f;

    // y
    data->vertices[2].color = g;  // First vert is at origin, no pos needed
    data->vertices[3].color = g;
    data->vertices[3].position.y = 0.2f;

    // z
    data->vertices[4].color = b;  // First vert is at origin, no pos needed
    data->vertices[5].color = b;
    data->vertices[5].position.z = 0.2f;

    // For each axis, generate points in a circle
    u32 j = 6;
    // z
    for (u32 i = 0; i < segments; ++i, j += 2)
    {
        // 2 at a time to form a line
        f32 theta = (f32)i / segments * B_2PI;
        data->vertices[j].position.x = radius * bcos(theta);
        data->vertices[j].position.y = radius * bsin(theta);
        data->vertices[j].color = b;

        theta = (f32)((i + 1) % segments) / segments * B_2PI;
        data->vertices[j + 1].position.x = radius * bcos(theta);
        data->vertices[j + 1].position.y = radius * bsin(theta);
        data->vertices[j + 1].color = b;
    }

    // y
    for (u32 i = 0; i < segments; ++i, j += 2)
    {
        // 2 at a time to form a line
        f32 theta = (f32)i / segments * B_2PI;
        data->vertices[j].position.x = radius * bcos(theta);
        data->vertices[j].position.z = radius * bsin(theta);
        data->vertices[j].color = g;

        theta = (f32)((i + 1) % segments) / segments * B_2PI;
        data->vertices[j + 1].position.x = radius * bcos(theta);
        data->vertices[j + 1].position.z = radius * bsin(theta);
        data->vertices[j + 1].color = g;
    }

    // x
    for (u32 i = 0; i < segments; ++i, j += 2)
    {
        // 2 at a time to form a line
        f32 theta = (f32)i / segments * B_2PI;
        data->vertices[j].position.y = radius * bcos(theta);
        data->vertices[j].position.z = radius * bsin(theta);
        data->vertices[j].color = r;

        theta = (f32)((i + 1) % segments) / segments * B_2PI;
        data->vertices[j + 1].position.y = radius * bcos(theta);
        data->vertices[j + 1].position.z = radius * bsin(theta);
        data->vertices[j + 1].color = r;
    }
}
