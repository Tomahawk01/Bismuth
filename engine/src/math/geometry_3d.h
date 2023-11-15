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
    u32 unique_id;
    vec3 position;
    f32 distance;
} raycast_hit;

typedef struct raycast_result
{
    // Darray - only created if hit exists otherwise null
    raycast_hit* hits;
} raycast_result;

BAPI ray ray_create(vec3 position, vec3 direction);
BAPI ray ray_from_screen(vec2 screen_pos, vec2 viewport_size, vec3 origin, mat4 view, mat4 projection);

BAPI b8 raycast_oriented_extents(extents_3d bb_extents, const mat4* bb_model, const ray* r, f32* out_dist);
