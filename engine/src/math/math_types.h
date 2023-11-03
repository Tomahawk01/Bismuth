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
                f32 z, b, p;
            };
            union
            {
                // Fourth element
                f32 w, a, q;
            };
        };
    };
} vec4;

typedef vec4 quat;

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
    vec3 tangent;
} vertex_3d;

typedef struct vertex_2d
{
    vec2 position;
    vec2 texcoord;
} vertex_2d;

typedef struct transform
{
    vec3 position;
    quat rotation;
    vec3 scale;
    b8 is_dirty;
    mat4 local;
    struct transform* parent;
} transform;

typedef struct plane_3d
{
    vec3 normal;
    f32 distance;
} plane_3d;

typedef struct frustum
{
    // Top, bottom, right, left, far, near
    plane_3d sides[6];
} frustum;
