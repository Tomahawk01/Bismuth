#pragma once

#include "defines.h"

typedef union vec2_u
{
    // Array of x, y
    f32 elements[2];
    struct
    {
        union
        {
            // First element
            f32 x, r, s, u;
        };
        union
        {
            // Second element
            f32 y, g, t, v;
        };
    };
} vec2;

typedef union vec3_u
{
    // Array of x, y, z
    f32 elements[3];
    struct
    {
        union
        {
            // First element
            f32 x, r, s, u;
        };
        union
        {
            // Second element
            f32 y, g, t, v;
        };
        union
        {
            // Third element
            f32 z, b, p, w;
        };
    };
} vec3;

typedef union vec4_u
{
    // Array of x, y, z, w
    f32 elements[4];
    union
    {
        struct
        {
            union
            {
                // First element
                f32 x, r, s;
            };
            union
            {
                // Second element
                f32 y, g, t;
            };
            union
            {
                // Third element
                f32 z, b, p, width;
            };
            union
            {
                // Fourth element
                f32 w, a, q, height;
            };
        };
    };
} vec4;

typedef vec4 quat;

typedef vec4 rect_2d;

/** @brief A 4-element vector of unsigned ints */
typedef union uvec4_u
{
    /** @brief An array of x, y, z, w */
    u32 elements[4];
    union
    {
        struct
        {
            union
            {
                /** @brief The first element */
                u32 x,
                    /** @brief The first element */
                    r,
                    /** @brief The first element */
                    s;
            };
            union
            {
                /** @brief The second element */
                u32 y,
                    /** @brief The third element */
                    g,
                    /** @brief The third element */
                    t;
            };
            union
            {
                /** @brief The third element */
                u32 z,
                    /** @brief The third element */
                    b,
                    /** @brief The third element */
                    p,
                    /** @brief The third element */
                    width;
            };
            union
            {
                /** @brief The fourth element */
                u32 w,
                    /** @brief The fourth element */
                    a,
                    /** @brief The fourth element */
                    q,
                    /** @brief The fourth element */
                    height;
            };
        };
    };
} uvec4;

/** @brief 3x3 matrix */
typedef union mat3_u
{
    // Matrix elements
    f32 data[12];
} mat3;

typedef union mat4_u
{
    f32 data[16];
} mat4;

typedef struct extents_2d
{
    vec2 min;
    vec2 max;
} extents_2d;

typedef struct extents_3d
{
    vec3 min;
    vec3 max;
} extents_3d;

typedef struct vertex_3d
{
    vec3 position;
    vec3 normal;
    vec2 texcoord;
    vec4 color;
    vec4 tangent;
} vertex_3d;

typedef struct vertex_2d
{
    vec2 position;
    vec2 texcoord;
} vertex_2d;

typedef struct color_vertex_3d
{
    /** @brief The position of the vertex. w is ignored */
    vec4 position;
    /** @brief The color of the vertex */
    vec4 color;
} color_vertex_3d;

typedef struct plane_3d
{
    vec3 normal;
    f32 distance;
} plane_3d;

#define FRUSTUM_SIDE_COUNT 6

typedef enum frustum_side
{
    FRUSTUM_SIDE_TOP = 0,
    FRUSTUM_SIDE_BOTTOM = 1,
    FRUSTUM_SIDE_RIGHT = 2,
    FRUSTUM_SIDE_LEFT = 3,
    FRUSTUM_SIDE_FAR = 4,
    FRUSTUM_SIDE_NEAR = 5
} frustum_side;

typedef struct frustum
{
    // Top, bottom, right, left, far, near
    plane_3d sides[FRUSTUM_SIDE_COUNT];
} frustum;

typedef union vec2i_t
{
    i32 elements[2];
    struct
    {
        union
        {
            // First element
            i32 x, r, s, u;
        };
        union
        {
            // Second element
            i32 y, g, t, v;
        };
    };
} vec2i;

typedef union vec4i_t
{
    i32 elements[4];
    union
    {
        struct
        {
            union
            {
                // First element
                i32 x, r, s;
            };
            union
            {
                // Second element
                i32 y, g, t;
            };
            union
            {
                // Third element
                i32 z, b, p, width;
            };
            union
            {
                // Fourth element
                i32 w, a, q, height;
            };
        };
    };
} vec4i;

typedef struct triangle
{
    vec3 verts[3];
} triangle;
