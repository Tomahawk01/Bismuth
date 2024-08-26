#pragma once

#include "defines.h"
#include "math/math_types.h"

#include "assets/basset_heightmap_terrain.h"
#include "assets/basset_types.h"

#define HEIGHTMAP_TERRAIN_MAX_MATERIAL_COUNT

/** @brief Represents a single vertex of a heightmap terrain */
typedef struct bheightmap_terrain_vertex
{
    /** @brief The position of the vertex */
    vec3 position;
    /** @brief The normal of the vertex */
    vec3 normal;
    /** @brief The texture coordinate of the vertex */
    vec2 texcoord;
    /** @brief The color of the vertex */
    vec4 color;
    /** @brief The tangent of the vertex */
    vec4 tangent;
    /** @brief A collection of material weights for this vertex */
    f32 material_weights[HEIGHTMAP_TERRAIN_MAX_MATERIAL_COUNT];
} bheightmap_terrain_vertex;

/** @brief Represents a Level Of Detail for a single heightmap terrain chunk */
typedef struct heightmap_terrain_chunk_lod
{
    /** @brief The index count for the chunk surface */
    u32 surface_index_count;
    /** @brief The total index count, including those for side skirts */
    u32 total_index_count;
    /** @brief The index data */
    u32* indices;
    /** @brief The offset from the beginning of the index buffer */
    u64 index_buffer_offset;
} heightmap_terrain_chunk_lod;

typedef struct heightmap_terrain_chunk
{
    /** @brief The chunk generation. Incremented every time the geometry changes */
    u16 generation;
    u32 surface_vertex_count;
    u32 total_vertex_count;

    // The vertex data
    bheightmap_terrain_vertex* vertices;
    // The offset in bytes into the vertex buffer
    u64 vertex_buffer_offset;

    heightmap_terrain_chunk_lod* lods;

    /** @brief The center of the geometry in local coordinates */
    vec3 center;
    /** @brief The extents of the geometry in local coordinates */
    extents_3d extents;

    /** @brief A pointer to the material associated with this geometry */
    struct material* material;
    /** @brief The current level of detail for this chunk */
    u8 current_lod;
} heightmap_terrain_chunk;
