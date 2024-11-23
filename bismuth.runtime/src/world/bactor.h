#pragma once

#include <strings/bname.h>

#include "bresources/bresource_types.h"
#include "systems/static_mesh_system.h"

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
struct bactor_staticmesh_comp_system_state;

typedef struct bactor_staticmesh_system_config
{
    u32 max_components;
} bactor_staticmesh_system_config;

BAPI b8 bactor_comp_staticmesh_system_initialize(u64* memory_requirement, void* state, const bactor_staticmesh_system_config* config);
BAPI void bactor_comp_staticmesh_system_shutdown(struct bactor_staticmesh_comp_system_state* state);

BAPI u32 bactor_comp_staticmesh_create(struct bactor_staticmesh_comp_system_state* state, u64 actor_id, bname name, bname static_mesh_resource_name);
BAPI u32 bactor_comp_staticmesh_get_id(struct bactor_staticmesh_comp_system_state* state, u64 actor_id, bname name);

BAPI bname bactor_comp_staticmesh_name_get(struct bactor_staticmesh_comp_system_state* state, u32 comp_id);
BAPI b8 bactor_comp_staticmesh_name_set(struct bactor_staticmesh_comp_system_state* state, u32 comp_id, bname name);

BAPI vec4 bactor_comp_staticmesh_tint_get(struct bactor_staticmesh_comp_system_state* state, u32 comp_id);
BAPI b8 bactor_comp_staticmesh_tint_set(struct bactor_staticmesh_comp_system_state* state, u32 comp_id, vec4 tint);

BAPI b8 bactor_comp_staticmesh_get_ids_for_actor(struct bactor_staticmesh_comp_system_state* state, u64 actor_id, u32* out_count, u32* out_comp_ids);

BAPI void bactor_comp_staticmesh_destroy(struct bactor_staticmesh_comp_system_state* state, u32 comp_id);

BAPI b8 bactor_comp_staticmesh_load(struct bactor_staticmesh_comp_system_state* state, u32 comp_id);
BAPI b8 bactor_comp_staticmesh_unload(struct bactor_staticmesh_comp_system_state* state, u32 comp_id);

BAPI static_mesh_instance* bactor_comp_staticmesh_get_mesh_instance(struct bactor_staticmesh_comp_system_state* state, u32 comp_id);
