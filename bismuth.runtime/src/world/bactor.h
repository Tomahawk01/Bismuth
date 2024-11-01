#pragma once

#include "bresources/bresource_types.h"
#include "math/geometry.h"
#include "strings/bname.h"

/**
 * Actor is a in-world representation of something which exists in or can be spawned in the world. 
 * It may contain actor-components, which can be used to control how actors are rendered, move about in the world, sound, etc. 
 * Each actor component has reference to at least one resource, which is generally what gets rendered (i.e. a static mesh resource), but not always (i.e. sound effect)
 *
 * When used with a scene, these may be parented to one another via the scene's hierarchy view and xform graph, when attached to a scene node.
 */
typedef struct bactor
{
    u64 id;
    bname name;
    b_handle xform;
} bactor;

// staticmesh system
struct bactor_staticmesh_system_state;

typedef struct bactor_staticmesh_system_config
{
    u32 max_components;
} bactor_staticmesh_system_config;

typedef enum staticmesh_render_data_flag
{
    /** @brief Indicates that the winding order for the given static mesh should be inverted */
    STATICMESH_RENDER_DATA_FLAG_WINDING_INVERTED_BIT = 0x0001
} staticmesh_render_data_flag;

/** @brief Collection of flags for a static mesh to be rendered */
typedef u32 staticmesh_render_data_flag_bits;

typedef struct bactor_comp_staticmesh_render_data
{
    const bresource_material_instance* material;

    u64 unique_id;

    /** @brief Flags for the static mesh to be rendered */
    staticmesh_render_data_flag_bits flags;

    /** @brief Additional tint to be applied to the static mesh when rendered */
    vec4 tint;

    /** @brief The vertex count */
    u32 vertex_count;
    /** @brief The offset from the beginning of the vertex buffer */
    u64 vertex_buffer_offset;

    /** @brief The index count */
    u32 index_count;
    /** @brief The offset from the beginning of the index buffer */
    u64 index_buffer_offset;

    /** @brief The index of the IBL probe to use */
    u32 ibl_probe_index;
} bactor_comp_staticmesh_render_data;

BAPI b8 bactor_comp_staticmesh_system_initialize(u64* memory_requirement, void* state, const bactor_staticmesh_system_config* config);
BAPI void bactor_comp_staticmesh_system_shutdown(struct bactor_staticmesh_system_state* state);

BAPI u32 bactor_comp_staticmesh_create(struct bactor_staticmesh_system_state* state, u64 actor_id, bname name, geometry g, bresource_material_instance material);
BAPI u32 bactor_comp_staticmesh_get_id(struct bactor_staticmesh_system_state* state, u64 actor_id, bname name);
BAPI void bactor_comp_staticmesh_destroy(struct bactor_staticmesh_system_state* state, u32 id);

BAPI b8 bactor_comp_staticmesh_load(u32 actor_id);
BAPI b8 bactor_comp_staticmesh_unload(u32 actor_id);

BAPI geometry* bactor_comp_staticmesh_get_geometry(struct bactor_staticmesh_system_state* state, u32 id);
BAPI bresource_material_instance* bactor_comp_staticmesh_get_material(struct bactor_staticmesh_system_state* state, u32 id);
BAPI b8 bactor_comp_staticmesh_get_geometry_material(struct bactor_staticmesh_system_state* state, u32 id, geometry** out_geometry, bresource_material_instance** out_material);

BAPI b8 bactor_comp_staticmesh_get_render_data(struct bactor_staticmesh_system_state* state, u32 id, struct bactor_comp_staticmesh_render_data* out_render_data);
