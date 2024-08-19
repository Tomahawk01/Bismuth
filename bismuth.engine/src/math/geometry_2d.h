#ifndef _BISMUTH_MATH_GEOMETRY_2D_H_
#define _BISMUTH_MATH_GEOMETRY_2D_H_

#include "defines.h"
#include "bmath.h"
#include "math_types.h"

typedef struct circle_2d
{
    vec2 center;
    f32 radius;
} circle_2d;

/**
 * @brief Indicates if the provided point is within the given rectangle
 * @param point Point to check
 * @param rect Rectangle to check against
 * @returns True if the point is within the rectangle; otherwise false
 */
BINLINE b8 point_in_rect_2d(vec2 point, rect_2d rect)
{
    return point.x >= rect.x && point.x <= rect.x + rect.width && point.y >= rect.y && point.y <= rect.y + rect.height;
}

/**
 * @brief Indicates if the provided point is within the given circle
 * @param point Point to check
 * @param rect Rectangle to check against
 * @returns True if the point is within the rectangle; otherwise false
 */
BINLINE b8 point_in_circle_2d(vec2 point, circle_2d circle)
{
    f32 r_squared = circle.radius * circle.radius;
    return vec2_distance_squared(point, circle.center) <= r_squared;
}

#endif
