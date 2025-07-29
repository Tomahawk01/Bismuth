#pragma once

#include "audio/baudio_types.h"
#include "core_resource_types.h"
#include "defines.h"
#include "graphs/hierarchy_graph.h"
#include "identifiers/bhandle.h"
#include "bresources/bresource_types.h"
#include "math/math_types.h"
#include "resources/debug/debug_grid.h"
#include "resources/resource_types.h"
#include "systems/static_mesh_system.h"

struct frame_data;
struct render_packet;
struct directional_light;
struct point_light;
struct skybox;
struct water_plane;
struct camera;
struct scene_config;
struct terrain;
struct ray;
struct raycast_result;
struct transform;
struct viewport;
struct geometry_render_data;

typedef enum scene_state
{
    // created, but nothing more
    SCENE_STATE_UNINITIALIZED,
    // Configuration parsed, not yet loaded hierarchy setup
    SCENE_STATE_INITIALIZED,
    // In the process of loading the hierarchy
    SCENE_STATE_LOADING,
    // Everything is loaded, ready to play
    SCENE_STATE_LOADED,
    // In the process of unloading, not ready to play
    SCENE_STATE_UNLOADING,
    // Unloaded and ready to be destroyed
    SCENE_STATE_UNLOADED
} scene_state;

typedef struct scene_attachment
{
    scene_node_attachment_type attachment_type;
    // Handle into the hierarchy graph
    bhandle hierarchy_node_handle;
    // A handle indexing into the resource array of the given type (i.e. meshes)
    bhandle resource_handle;
} scene_attachment;

typedef enum scene_flag
{
    SCENE_FLAG_NONE = 0,
    // Indicates if the scene can be saved once modified (i.e. read-only would be used for runtime, writing would be used in editor, etc.)
    SCENE_FLAG_READONLY = 1
} scene_flag;

// Bitwise flags to be used on scene load, etc.
typedef u32 scene_flags;

typedef struct scene_node_metadata
{
    u32 index;
    // Metadata considered stale/non-existant if INVALID_ID_U64
    u64 uniqueid;

    // The name of the node
    bname name;
} scene_node_metadata;

typedef struct scene_static_mesh_metadata
{
    bname resource_name;
    bname package_name;
} scene_static_mesh_metadata;

typedef struct scene_terrain_metadata
{
    bname name;
    bname resource_name;
    bname package_name;
} scene_terrain_metadata;

typedef struct scene_skybox_metadata
{
    bname cubemap_name;
    bname package_name;
} scene_skybox_metadata;

typedef struct scene_water_plane_metadata
{
    u32 reserved;
} scene_water_plane_metadata;

struct scene_audio_emitter;
struct scene_volume;

typedef struct scene
{
    u32 id;
    scene_flags flags;

    scene_state state;
    b8 enabled;

    bname name;
    char* description;

    // darray of directional lights
    struct directional_light* dir_lights;
    // Array of scene attachments for directional lights
    scene_attachment* directional_light_attachments;

    // darray of point lights
    struct point_light* point_lights;
    // Array of scene attachments for point lights
    scene_attachment* point_light_attachments;

    // darray of audio emitters
    struct scene_audio_emitter* audio_emitters;
    // Array of scene attachments for audio_emitters
    scene_attachment* audio_emitter_attachments;

    // darray of static meshes
    static_mesh_instance* static_meshes;
    // Array of scene attachments for meshes
    scene_attachment* mesh_attachments;
    // Array of mesh metadata
    scene_static_mesh_metadata* mesh_metadata;

    // darray of terrains
    struct terrain* terrains;
    // Array of scene attachments for terrains
    scene_attachment* terrain_attachments;
    // Array of terrain metadata
    scene_terrain_metadata* terrain_metadata;

    // darray of skyboxes
    struct skybox* skyboxes;
    // Array of scene attachments for skyboxes
    scene_attachment* skybox_attachments;
    // Array of skybox metadata
    scene_skybox_metadata* skybox_metadata;

    // darray of water planes
    struct water_plane* water_planes;
    // Array of scene attachments for water planes
    scene_attachment* water_plane_attachments;
    // Array of water plane metadata
    scene_water_plane_metadata* water_plane_metadata;

    // darray of volumes
    struct scene_volume* volumes;
    // Array of scene attachments for volumes
    scene_attachment* volume_attachments;

    // A grid for the scene
    debug_grid grid;

    // A pointer to the scene configuration resource
    bresource_scene* config;

    hierarchy_graph hierarchy;

    // An array of node metadata, indexed by hierarchy graph handle
    // Marked as unused by id == INVALID_ID
    // Size of this array is always highest id+1. Does not shrink on node destruction
    scene_node_metadata* node_metadata;

    // The number of node_metadatas currently allocated
    u32 node_metadata_count;

} scene;

/**
 * @brief Creates a new scene with the given config with default values.
 * No resources are allocated. Config is not yet processed.
 *
 * @param config A pointer to the configuration resource. Optional.
 * @param flags Flags to be used during creation (i.e. read-only, etc.).
 * @param out_scene A pointer to hold the newly created scene. Required.
 * @return True on success; otherwise false.
 */
BAPI b8 scene_create(bresource_scene* config, scene_flags flags, scene* out_scene);

/**
 * @brief Performs initialization routines on the scene, including processing
 * configuration (if provided) and scaffolding heirarchy.
 *
 * @param scene A pointer to the scene to be initialized.
 * @return True on success; otherwise false.
 */
BAPI b8 scene_initialize(scene* scene);

/**
 * @brief Performs loading routines and resource allocation on the given scene.
 *
 * @param scene A pointer to the scene to be loaded.
 * @return True on success; otherwise false.
 */
BAPI b8 scene_load(scene* scene);

/**
 * @brief Performs unloading routines and resource de-allocation on the given scene.
 * A scene is also destroyed when unloading.
 *
 * @param scene A pointer to the scene to be unloaded.
 * @param immediate Unload immediately instead of the next frame. NOTE: can have unintended side effects if used improperly.
 * @return True on success; otherwise false.
 */
BAPI b8 scene_unload(scene* scene, b8 immediate);

/**
 * @brief Destroys the scene, releasing any remaining resources held by it. 
 * Automatically triggers unload if scene is currently loaded
 * 
 * @param s A pointer to the scene to be destroyed
 */
BAPI void scene_destroy(scene* s);

/**
 * @brief Performs any required scene updates for the given frame.
 *
 * @param scene A pointer to the scene to be updated.
 * @param p_frame_data A constant pointer to the current frame's data.
 * @return True on success; otherwise false.
 */
BAPI b8 scene_update(scene* scene, const struct frame_data* p_frame_data);

BAPI void scene_render_frame_prepare(scene* scene, const struct frame_data* p_frame_data);

/**
 * @brief Updates LODs of items in the scene based on the given position and clipping distances.
 *
 * @param scene A pointer to the scene to be updated.
 * @param p_frame_data A constant pointer to the current frame's data.
 * @param view_position The view position to use for LOD calculation.
 * @param near_clip The near clipping distance from the view position.
 * @param far_clip The far clipping distance from the view position.
 */
BAPI void scene_update_lod_from_view_position(scene* scene, const struct frame_data* p_frame_data, vec3 view_position, f32 near_clip, f32 far_clip);

BAPI b8 scene_raycast(scene* scene, const struct ray* r, struct raycast_result* out_result);

BAPI b8 scene_debug_render_data_query(scene* scene, u32* data_count, struct geometry_render_data** debug_geometries);

BAPI b8 scene_mesh_render_data_query(const scene* scene, const frustum* f, vec3 center, struct frame_data* p_frame_data, u32* out_count, struct geometry_render_data** out_geometries);
BAPI b8 scene_mesh_render_data_query_from_line(const scene* scene, vec3 direction, vec3 center, f32 radius, struct frame_data* p_frame_data, u32* out_count, struct geometry_render_data** out_geometries);

BAPI b8 scene_terrain_render_data_query(const scene* scene, const frustum* f, vec3 center, struct frame_data* p_frame_data, u32* out_count, struct geometry_render_data** out_terrain_geometries);
BAPI b8 scene_terrain_render_data_query_from_line(const scene* scene, vec3 direction, vec3 center, f32 radius, struct frame_data* p_frame_data, u32* out_count, struct geometry_render_data** out_geometries);

BAPI b8 scene_water_plane_query(const scene* scene, const frustum* f, vec3 center, struct frame_data* p_frame_data, u32* out_count, struct water_plane*** out_water_planes);

BAPI b8 scene_node_xform_get_by_name(const scene* scene, bname name, bhandle* out_xform_handle);
BAPI b8 scene_node_xform_get(const scene* scene, bhandle node_handle, bhandle* out_xform_handle);
BAPI b8 scene_node_local_matrix_get(const scene* scene, bhandle node_handle, mat4* out_matrix);
BAPI b8 scene_node_local_matrix_get_by_name(const scene* scene, bname name, mat4* out_matrix);

BAPI b8 scene_node_exists(const scene* s, bname name);
BAPI b8 scene_node_child_count_get(const scene* s, bname name, u32* out_child_count);
BAPI b8 scene_node_child_name_get_by_index(const scene* s, bname name, u32 index, bname* out_child_name);

BAPI b8 scene_save(scene* s);
