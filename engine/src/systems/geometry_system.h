#pragma once

#include "renderer/renderer_types.h"

typedef struct geometry_system_config
{
    // Max number of geometries that can be loaded at once.
    // NOTE: Should be significantly greater than the number of static meshes
    // because there can and will be more than one of these per mesh.
    // Take other systems into account as well
    u32 max_geometry_count;
} geometry_system_config;

typedef struct geometry_config
{
    u32 vertex_size;
    u32 vertex_count;
    void* vertices;
    u32 index_size;
    u32 index_count;
    void* indices;

    vec3 center;
    vec3 min_extents;
    vec3 max_extents;

    char name[GEOMETRY_NAME_MAX_LENGTH];
    char material_name[MATERIAL_NAME_MAX_LENGTH];
} geometry_config;

#define DEFAULT_GEOMETRY_NAME "default"

b8 geometry_system_initialize(u64* memory_requirement, void* state, void* config);
void geometry_system_shutdown(void* state);

/**
 * @brief Acquires an existing geometry by id.
 * 
 * @param id The geometry identifier to acquire by.
 * @return A pointer to the acquired geometry or nullptr if failed.
 */
BAPI geometry* geometry_system_acquire_by_id(u32 id);

/**
 * @brief Registers and acquires a new geometry using the given config.
 * 
 * @param config The geometry configuration.
 * @param auto_release Indicates if the acquired geometry should be unloaded when its reference count reaches 0
 * @return A pointer to the acquired geometry or nullptr if failed. 
 */
BAPI geometry* geometry_system_acquire_from_config(geometry_config config, b8 auto_release);

/**
 * @brief Frees resources held by the provided configuration.
 *
 * @param config A pointer to the configuration to be disposed.
 */
BAPI void geometry_system_config_dispose(geometry_config* config);

/**
 * @brief Releases a reference to the provided geometry.
 * 
 * @param geometry The geometry to be released.
 */
BAPI void geometry_system_release(geometry* geometry);

/**
 * @brief Obtains a pointer to the default geometry.
 * 
 * @return A pointer to the default geometry. 
 */
BAPI geometry* geometry_system_get_default(void);

/**
 * @brief Obtains a pointer to the default geometry.
 * 
 * @return A pointer to the default geometry. 
 */
BAPI geometry* geometry_system_get_default_2d(void);

BAPI geometry_config geometry_system_generate_plane_config(f32 width, f32 height, u32 x_segment_count, u32 y_segment_count, f32 tile_x, f32 tile_y, const char* name, const char* material_name);
BAPI geometry_config geometry_system_generate_cube_config(f32 width, f32 height, f32 depth, f32 tile_x, f32 tile_y, const char* name, const char* material_name);
