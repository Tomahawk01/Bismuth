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

typedef struct vec3_u
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

typedef struct vertex_3d
{
    vec3 position;
} vertex_3d;