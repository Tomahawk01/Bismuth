#pragma once

#include <defines.h>
#include <math/math_types.h>
#include <resources/resource_types.h>

#ifdef _DEBUG
#include <resources/debug/debug_line3d.h>
#endif

struct ray;
struct camera;

typedef enum editor_gizmo_mode
{
    EDITOR_GIZMO_MODE_NONE = 0,
    EDITOR_GIZMO_MODE_MOVE = 1,
    EDITOR_GIZMO_MODE_ROTATE = 2,
    EDITOR_GIZMO_MODE_SCALE = 3,
    EDITOR_GIZMO_MODE_MAX = EDITOR_GIZMO_MODE_SCALE,
} editor_gizmo_mode;

typedef enum editor_gizmo_interaction_type
{
    EDITOR_GIZMO_INTERACTION_TYPE_NONE,
    EDITOR_GIZMO_INTERACTION_TYPE_MOUSE_HOVER,
    EDITOR_GIZMO_INTERACTION_TYPE_MOUSE_DOWN,
    EDITOR_GIZMO_INTERACTION_TYPE_MOUSE_DRAG,
    EDITOR_GIZMO_INTERACTION_TYPE_MOUSE_UP,
    EDITOR_GIZMO_INTERACTION_TYPE_CANCEL
} editor_gizmo_interaction_type;

typedef struct editor_gizmo_mode_data
{
    u32 vertex_count;
    color_vertex_3d* vertices;

    u32 index_count;
    u32* indices;

    geometry geo;

    u32 extents_count;
    extents_3d* mode_extents;

    u8 current_axis_index;
    plane_3d interaction_plane;
    plane_3d interaction_plane_back;

    vec3 interaction_start_pos;
    vec3 last_interaction_pos;
} editor_gizmo_mode_data;

typedef enum editor_gizmo_orientation
{
    EDITOR_GIZMO_ORIENTATION_GLOBAL = 0,
    EDITOR_GIZMO_ORIENTATION_LOCAL = 1,
    // EDITOR_GIZMO_ORIENTATION_VIEW = 2,
    EDITOR_GIZMO_ORIENTATION_MAX = EDITOR_GIZMO_ORIENTATION_LOCAL
} editor_gizmo_orientation;

typedef struct editor_gizmo
{
    transform xform;
    transform* selected_xform;
    editor_gizmo_mode mode;

    f32 scale_scalar;

    editor_gizmo_orientation orientation;

    editor_gizmo_mode_data mode_data[EDITOR_GIZMO_MODE_MAX + 1];

    editor_gizmo_interaction_type interaction;

#ifdef _DEBUG
    debug_line3d plane_normal_line;
#endif
} editor_gizmo;

BAPI b8 editor_gizmo_create(editor_gizmo* out_gizmo);
BAPI void editor_gizmo_destroy(editor_gizmo* gizmo);

BAPI b8 editor_gizmo_initialize(editor_gizmo* gizmo);
BAPI b8 editor_gizmo_load(editor_gizmo* gizmo);
BAPI b8 editor_gizmo_unload(editor_gizmo* gizmo);

BAPI void editor_gizmo_refresh(editor_gizmo* gizmo);
BAPI editor_gizmo_orientation editor_gizmo_orientation_get(editor_gizmo* gizmo);
BAPI void editor_gizmo_orientation_set(editor_gizmo* gizmo, editor_gizmo_orientation orientation);
BAPI void editor_gizmo_selected_transform_set(editor_gizmo* gizmo, transform* xform);

BAPI void editor_gizmo_update(editor_gizmo* gizmo);

BAPI void editor_gizmo_mode_set(editor_gizmo* gizmo, editor_gizmo_mode mode);

BAPI void editor_gizmo_interaction_begin(editor_gizmo* gizmo, struct camera* c, struct ray* r, editor_gizmo_interaction_type interaction_type);
BAPI void editor_gizmo_interaction_end(editor_gizmo* gizmo);
BAPI void editor_gizmo_handle_interaction(editor_gizmo* gizmo, struct camera* c, struct ray* r, editor_gizmo_interaction_type interaction_type);
