#pragma once

#include "defines.h"
#include "math/math_types.h"
#include "resources/resource_types.h"

struct texture_map;

typedef struct water_plane_vertex
{
    vec4 position;
} water_plane_vertex;

typedef struct water_plane
{
    mat4 model;
    water_plane_vertex vertices[4];
    u32 indices[6];
    u64 index_buffer_offset;
    u64 vertex_buffer_offset;
    u32 group_id;

    f32 tiling;
    f32 wave_strength;
    f32 wave_speed;

    // Refraction target textures
    bresource_texture* refraction_color;
    bresource_texture* refraction_depth;
    // Reflection target textures
    bresource_texture* reflection_color;
    bresource_texture* reflection_depth;

    // Pointer to dudv texture
    bresource_texture* dudv_texture;

    // Pointer to normal texture
    bresource_texture* normal_texture;
} water_plane;

BAPI b8 water_plane_create(water_plane* out_plane);
BAPI void water_plane_destroy(water_plane* plane);

BAPI b8 water_plane_initialize(water_plane* plane);
BAPI b8 water_plane_load(water_plane* plane);
BAPI b8 water_plane_unload(water_plane* plane);

BAPI b8 water_plane_update(water_plane* plane);
