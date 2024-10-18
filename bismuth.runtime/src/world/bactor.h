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

BAPI b8 bactor_comp_staticmesh_system_initialize(u64* memory_requirement, void* state, const bactor_staticmesh_system_config* config);
BAPI void bactor_comp_staticmesh_system_shutdown(struct bactor_staticmesh_system_state* state);

BAPI u64 bactor_comp_staticmesh_create(struct bactor_staticmesh_system_state* state, u64 actor_id, bname name, geometry g, bresource_material_instance material);
BAPI u64 bactor_comp_staticmesh_get_id(struct bactor_staticmesh_system_state* state, u64 actor_id, bname name);
BAPI void bactor_comp_staticmesh_destroy(struct bactor_staticmesh_system_state* state, u64 id);

BAPI b8 bactor_comp_staticmesh_get_geometry(struct bactor_staticmesh_system_state* state, u64 id, geometry* out_geometry);
BAPI b8 bactor_comp_staticmesh_get_material(struct bactor_staticmesh_system_state* state, u64 id, bresource_material_instance* out_material);
BAPI b8 bactor_comp_staticmesh_get_geometry_material(struct bactor_staticmesh_system_state* state, u64 id, geometry* out_geometry, bresource_material_instance* out_material);
