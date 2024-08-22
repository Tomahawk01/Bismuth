#pragma once

#include "identifiers/identifier.h"
#include "defines.h"
#include "math/math_types.h"
#include "resources/resource_types.h"

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
    char* resource_name;
} terrain_config;

typedef struct terrain_resource
{
    char* name;
    u32 chunk_size;
    u32 tile_count_x;
    u32 tile_count_z;
    // How large each tile is on x axis
    f32 tile_scale_x;
    // How large each tile is on z axis
    f32 tile_scale_z;
    // Max height of generated terrain
    f32 scale_y;

    u32 vertex_data_length;
    terrain_vertex_data* vertex_datas;

    u32 material_count;
    char** material_names;
} terrain_resource;

typedef struct terrain_chunk_lod
{
    // The index count for the chunk surface
    u32 surface_index_count;

    // The total index count, including those for side skirts
    u32 total_index_count;
    // The index data
    u32* indices;
    // The offset from the beginning of the index buffer
    u64 index_buffer_offset;
} terrain_chunk_lod;

typedef struct terrain_chunk
{
    // The chunk generation. Incremented every time the geometry changes
    u16 generation;
    u32 surface_vertex_count;
    u32 total_vertex_count;
    terrain_vertex* vertices;
    u64 vertex_buffer_offset;

    terrain_chunk_lod* lods;

    // The center of the geometry in local coordinates
    vec3 center;
    // The extents of the geometry in local coordinates
    extents_3d extents;

    // A pointer to the material associated with this geometry.
    struct material* material;

    u8 current_lod;
} terrain_chunk;

typedef enum terrain_state
{
    TERRAIN_STATE_UNDEFINED,
    TERRAIN_STATE_CREATED,
    TERRAIN_STATE_INITIALIZED,
    TERRAIN_STATE_LOADING,
    TERRAIN_STATE_LOADED
} terrain_state;

typedef struct terrain
{
    identifier id;
    u32 generation;
    terrain_state state;
    char* name;
    char* resource_name;
    u32 tile_count_x;
    u32 tile_count_z;
    // How large each tile is on x axis
    f32 tile_scale_x;
    // How large each tile is on z axis
    f32 tile_scale_z;
    // Max height of generated terrain
    f32 scale_y;

    u32 chunk_size;

    u32 vertex_data_length;
    terrain_vertex_data* vertex_datas;

    extents_3d extents;
    vec3 origin;

    u32 chunk_count;
    // row by row, then column
    // 0, 1, 2, 3
    // 4, 5, 6, 7
    // 8, 9, ...
    terrain_chunk* chunks;

    u8 lod_count;

    u32 material_count;
    char** material_names;
} terrain;

BAPI b8 terrain_create(const terrain_config* config, terrain* out_terrain);
BAPI void terrain_destroy(terrain* t);

BAPI b8 terrain_initialize(terrain* t);
BAPI b8 terrain_load(terrain* t);
BAPI b8 terrain_chunk_load(terrain* t, terrain_chunk* chunk);
BAPI b8 terrain_unload(terrain* t);
BAPI b8 terrain_chunk_unload(terrain* t, terrain_chunk* chunk);

BAPI b8 terrain_update(terrain* t);

BAPI void terrain_geometry_generate_normals(u32 vertex_count, struct terrain_vertex* vertices, u32 index_count, u32* indices);
BAPI void terrain_geometry_generate_tangents(u32 vertex_count, struct terrain_vertex* vertices, u32 index_count, u32* indices);
