#pragma once

#include "defines.h"
#include "core/identifier.h"
#include "math/math_types.h"
#include "resources/resource_types.h"
#include "core/frame_data.h"

typedef struct terrain_vertex
{
    /** @brief Position of the vertex */
    vec3 position;
    /** @brief Normal of the vertex */
    vec3 normal;
    /** @brief Texture coordinate of the vertex */
    vec2 texcoord;
    /** @brief Color of the vertex */
    vec4 color;
    /** @brief Tangent of the vertex */
    vec4 tangent;

    /** @brief Collection of material weights for this vertex */
    f32 material_weights[TERRAIN_MAX_MATERIAL_COUNT];
} terrain_vertex;

typedef struct terrain_vertex_data
{
  f32 height;
} terrain_vertex_data;

typedef struct terrain_config
{
    char* name;
    u32 tile_count_x;
    u32 tile_count_z;
    // How large each tile is on x axis
    f32 tile_scale_x;
    // How large each tile is on z axis
    f32 tile_scale_z;
    // Max height of generated terrain
    f32 scale_y;

    transform xform;

    u32 vertex_data_length;
    terrain_vertex_data *vertex_datas;

    u32 material_count;
    char** material_names;
} terrain_config;

typedef struct terrain
{
    identifier id;
    char* name;
    transform xform;
    u32 tile_count_x;
    u32 tile_count_z;
    // How large each tile is on x axis
    f32 tile_scale_x;
    // How large each tile is on z axis
    f32 tile_scale_z;
    // Max height of generated terrain
    f32 scale_y;

    u32 vertex_data_length;
    terrain_vertex_data *vertex_datas;

    extents_3d extents;
    vec3 origin;

    u32 vertex_count;
    terrain_vertex* vertices;

    u32 index_count;
    u32* indices;

    geometry geo;

    u32 material_count;
    char **material_names;
} terrain;

BAPI b8 terrain_create(const terrain_config* config, terrain* out_terrain);
BAPI void terrain_destroy(terrain* t);

BAPI b8 terrain_initialize(terrain* t);
BAPI b8 terrain_load(terrain* t);
BAPI b8 terrain_unload(terrain* t);

BAPI b8 terrain_update(terrain* t);
