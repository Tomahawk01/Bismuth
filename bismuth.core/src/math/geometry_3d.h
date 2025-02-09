#include "identifiers/bhandle.h"
#include "math_types.h"

typedef struct ray
{
    vec3 origin;
    vec3 direction;
} ray;

typedef enum raycast_hit_type
{
    RAYCAST_HIT_TYPE_OBB,
    RAYCAST_HIT_TYPE_SURFACE
} raycast_hit_type;

typedef struct raycast_hi
 {
    raycast_hit_type type;
    bhandle xform_handle;
    bhandle node_handle;
    bhandle xform_parent_handle;
    vec3 position;
    f32 distance;
} raycast_hit;

typedef struct raycast_result
{
    // Darray - only created if hit exists otherwise null
    raycast_hit* hits;
} raycast_result;

BAPI ray ray_create(vec3 position, vec3 direction);
BAPI ray ray_from_screen(vec2 screen_pos, rect_2d viewport_rect, vec3 origin, mat4 view, mat4 projection);

BAPI b8 raycast_aabb(extents_3d bb_extents, const ray* r, vec3* out_point);
BAPI b8 raycast_oriented_extents(extents_3d bb_extents, mat4 model, const ray* r, f32* out_dist);

BAPI b8 raycast_plane_3d(const ray* r, const plane_3d* p, vec3* out_point, f32* out_distance);

BAPI b8 raycast_disc_3d(const ray* r, vec3 center, vec3 normal, f32 outer_radius, f32 inner_radius, vec3* out_point, f32* out_distance);
