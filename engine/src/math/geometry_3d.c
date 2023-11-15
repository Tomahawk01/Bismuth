#include "geometry_3d.h"

#include"math/bmath.h"
#include "math/math_types.h"

ray ray_create(vec3 position, vec3 direction)
{
    ray r = {0};
    r.origin = position;
    r.direction = direction;
    return r;
}

ray ray_from_screen(vec2 screen_pos, vec2 viewport_size, vec3 origin, mat4 view, mat4 projection)
{
    ray r = {0};
    r.origin = origin;

    // Get normalized device coordinates (i.e. -1:1 range)
    vec3 ray_ndc;
    ray_ndc.x = (2.0f * screen_pos.x) / viewport_size.x - 1.0f;
    ray_ndc.y = 1.0f - (2.0f * screen_pos.y) / viewport_size.y;
    ray_ndc.z = 1.0f;

    // Clip space
    vec4 ray_clip = vec4_create(ray_ndc.x, ray_ndc.y, -1.0f, 1.0f);

    // Eye/Camera space
    vec4 ray_eye = mat4_mul_vec4(mat4_inverse(projection), ray_clip);

    // Unproject xy, change wz to "forward"
    ray_eye = vec4_create(ray_eye.x, ray_eye.y, -1.0f, 0.0f);

    // Convert to world coordinates
    r.direction = vec3_from_vec4(mat4_mul_vec4(view, ray_eye));
    vec3_normalize(&r.direction);

    return r;
}

b8 raycast_oriented_extents(extents_3d bb_extents, const mat4* bb_model, const ray* r, f32* out_dist)
{
    // Nearest "far" intersection (within x, y and z plane pairs)
    f32 nearest_far_intersection = 0.0f;

    // Farthest "near" intersection (withing x, y and z plane pairs)
    f32 farthest_near_intersection = 100000.0f;

    // Pick world position from the model matrix
    vec3 oriented_pos_world = vec3_create(bb_model->data[12], bb_model->data[13], bb_model->data[14]);

    // Transform the extents - This will orient/scale them to the model matrix
    bb_extents.min = mat4_mul_vec3(*bb_model, bb_extents.min);
    bb_extents.max = mat4_mul_vec3(*bb_model, bb_extents.max);

    // Distance between the world position and the ray's origin
    vec3 delta = vec3_sub(oriented_pos_world, r->origin);

    // Test for intersection with other planes perpendicular to each axis
    vec3 x_axis = mat4_right(*bb_model);
    vec3 y_axis = mat4_up(*bb_model);
    vec3 z_axis = mat4_backward(*bb_model);
    vec3 axes[3] = {x_axis, y_axis, z_axis};
    for (i32 i = 0; i < 3; ++i)
    {
        f32 e = vec3_dot(axes[i], delta);
        f32 f = vec3_dot(r->direction, axes[i]);

        if (babs(f) > 0.0001f)
        {
            // Store distances between ray origin and ray-plane intersections in t1, and t2
            // Intersection with "left" plane
            f32 t1 = (e + bb_extents.min.elements[i]) / f;
            // Intersection with "right" plane
            f32 t2 = (e + bb_extents.max.elements[i]) / f;

            // Ensure that t1 is nearest intersection, and swap if needed
            if (t1 > t2)
            {
                f32 temp = t1;
                t1 = t2;
                t2 = temp;
            }

            if (t2 < farthest_near_intersection)
                farthest_near_intersection = t2;

            if (t1 > nearest_far_intersection)
                nearest_far_intersection = t1;

            // If "far" is closer than "near" then there is no intersection
            if (farthest_near_intersection < nearest_far_intersection)
                return false;
        }
        else
        {
            // Where ray is almost parallel to the planes then they don't have any intersection
            if (-e + bb_extents.min.elements[i] > 0.0f || -e + bb_extents.max.elements[i] < 0.0f)
                return false;
        }
    }

    // This prevents interections from within a bounding box if the ray originates there
    if (nearest_far_intersection == 0.0f)
        return false;

    *out_dist = nearest_far_intersection;
    return true;
}
