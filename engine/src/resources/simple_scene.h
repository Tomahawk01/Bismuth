#pragma once

#include "defines.h"
#include "math/math_types.h"
#include "resources/debug/debug_grid.h"

struct frame_data;
struct render_packet;
struct directional_light;
struct point_light;
struct mesh;
struct skybox;
struct geometry_config;
struct camera;
struct simple_scene_config;
struct terrain;
struct ray;
struct raycast_result;
struct transform;
struct viewport;
struct geometry_render_data;

typedef enum simple_scene_state
{
    SIMPLE_SCENE_STATE_UNINITIALIZED,
    SIMPLE_SCENE_STATE_INITIALIZED,
    SIMPLE_SCENE_STATE_LOADING,
    SIMPLE_SCENE_STATE_LOADED,
    SIMPLE_SCENE_STATE_UNLOADING,
    SIMPLE_SCENE_STATE_UNLOADED
} simple_scene_state;

typedef struct pending_mesh
{
    struct mesh* m;

    const char* mesh_resource_name;

    u32 geometry_config_count;
    struct geometry_config** g_configs;
} pending_mesh;

typedef struct simple_scene
{
    u32 id;
    simple_scene_state state;
    b8 enabled;

    char* name;
    char* description;

    transform scene_transform;

    // Singlular pointer to a directional light
    struct directional_light* dir_light;

    // darray of point lights
    struct point_light* point_lights;

    // darray of meshes
    struct mesh* meshes;

    // darray of terrains
    struct terrain* terrains;

    // darray of meshes to be loaded
    pending_mesh* pending_meshes;

    // Singlular pointer to a skybox
    struct skybox* sb;

    // A grid for the scene
    debug_grid grid;

    // A pointer to the scene configuration, if provided
    struct simple_scene_config* config;

} simple_scene;

BAPI b8 simple_scene_create(void* config, simple_scene* out_scene);
BAPI b8 simple_scene_initialize(simple_scene* scene);
BAPI b8 simple_scene_load(simple_scene* scene);
BAPI b8 simple_scene_unload(simple_scene* scene, b8 immediate);
BAPI b8 simple_scene_update(simple_scene* scene, const struct frame_data* p_frame_data);

BAPI void simple_scene_render_frame_prepare(simple_scene* scene, const struct frame_data* p_frame_data);

BAPI void simple_scene_update_lod_from_view_position(simple_scene* scene, const struct frame_data* p_frame_data, vec3 view_position, f32 near_clip, f32 far_clip);

BAPI b8 simple_scene_raycast(simple_scene* scene, const struct ray* r, struct raycast_result* out_result);

BAPI b8 simple_scene_directional_light_add(simple_scene* scene, const char* name, struct directional_light* light);
BAPI b8 simple_scene_point_light_add(simple_scene* scene, const char* name, struct point_light* light);
BAPI b8 simple_scene_mesh_add(simple_scene* scene, const char* name, struct mesh* m);
BAPI b8 simple_scene_skybox_add(simple_scene* scene, const char* name, struct skybox* sb);
BAPI b8 simple_scene_terrain_add(simple_scene* scene, const char* name, struct terrain* t);

BAPI b8 simple_scene_directional_light_remove(simple_scene* scene, const char* name);
BAPI b8 simple_scene_point_light_remove(simple_scene* scene, const char* name);
BAPI b8 simple_scene_mesh_remove(simple_scene* scene, const char* name);
BAPI b8 simple_scene_skybox_remove(simple_scene* scene, const char* name);
BAPI b8 simple_scene_terrain_remove(simple_scene* scene, const char* name);

BAPI struct directional_light* simple_scene_directional_light_get(simple_scene* scene, const char* name);
BAPI struct point_light* simple_scene_point_light_get(simple_scene* scene, const char* name);
BAPI struct mesh* simple_scene_mesh_get(simple_scene* scene, const char* name);
BAPI struct skybox* simple_scene_skybox_get(simple_scene* scene, const char* name);
BAPI struct terrain* simple_scene_terrain_get(simple_scene* scene, const char* name);

BAPI struct transform* simple_scene_transform_get_by_id(simple_scene* scene, u64 unique_id);

BAPI b8 simple_scene_debug_render_data_query(simple_scene* scene, u32* data_count, struct geometry_render_data** debug_geometries);

BAPI b8 simple_scene_mesh_render_data_query(const simple_scene* scene, const frustum* f, vec3 center, struct frame_data* p_frame_data, u32* out_count, struct geometry_render_data** out_geometries);
BAPI b8 simple_scene_mesh_render_data_query_from_line(const simple_scene* scene, vec3 direction, vec3 center, f32 radius, struct frame_data* p_frame_data, u32* out_count, struct geometry_render_data** out_geometries);

BAPI b8 simple_scene_terrain_render_data_query(const simple_scene* scene, const frustum* f, vec3 center, struct frame_data* p_frame_data, u32* out_count, struct geometry_render_data** out_terrain_geometries);
BAPI b8 simple_scene_terrain_render_data_query_from_line(const simple_scene* scene, vec3 direction, vec3 center, f32 radius, struct frame_data* p_frame_data, u32* out_count, struct geometry_render_data** out_geometries);
