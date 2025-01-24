#pragma once

#include "defines.h"
#include "math/math_types.h"
#include "strings/bname.h"

typedef struct directional_light_data
{
    vec4 color;
    vec4 direction;

    f32 shadow_distance;
    f32 shadow_fade_distance;
    f32 shadow_split_mult;
    f32 padding;
} directional_light_data;

typedef struct directional_light
{
    bname name;
    // Generation of the light, incremented on change. Can be used to tell when a shader upload is required
    u32 generation;
    directional_light_data data;
    void* debug_data;
} directional_light;

typedef struct point_light_data
{
    vec4 color;
    vec4 position;
    // Usually 1, make sure denominator never gets smaller than 1
    f32 constant_f;
    // Reduces light intensity linearly
    f32 linear;
    // Makes the light fall off slower at longer distances
    f32 quadratic;
    f32 padding;
} point_light_data;

typedef struct point_light
{
    bname name;
    // The generation of the light, incremented on every update. Can be used to detect when a shader upload is required
    u32 generation;
    point_light_data data;
    void* debug_data;

    vec4 position;
} point_light;

b8 light_system_initialize(u64* memory_requirement, void* memory, void* config);
void light_system_shutdown(void* state);

BAPI b8 light_system_directional_add(directional_light* light);
BAPI b8 light_system_point_add(point_light* light);

BAPI b8 light_system_directional_remove(directional_light* light);
BAPI b8 light_system_point_remove(point_light* light);

BAPI directional_light* light_system_directional_light_get(void);

BAPI u32 light_system_point_light_count(void);
BAPI b8 light_system_point_lights_get(point_light* p_lights);
