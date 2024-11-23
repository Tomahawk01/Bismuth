#include "bactor.h"

#include <debug/bassert.h>
#include <defines.h>
#include <bresources/bresource_types.h>
#include <logger.h>
#include <math/bmath.h>
#include <memory/bmemory.h>
#include <strings/bname.h>

typedef struct bactor_staticmesh_comp_system_state
{
    u32 max_components;
    // Owning actor ids
    u64* actor_ids;
    // Array of static mesh instances
    static_mesh_instance* mesh_instances;
    bname* names;
    vec4* tints;
    bname* resource_names;
} bactor_staticmesh_comp_system_state;

b8 bactor_comp_staticmesh_system_initialize(u64* memory_requirement, void* state_block, const bactor_staticmesh_system_config* config)
{
    BASSERT_MSG(memory_requirement, "bactor_comp_staticmesh_system_initialize requires a valid pointer to memory_requirement");

    *memory_requirement = sizeof(bactor_staticmesh_comp_system_state) +
                          (sizeof(u64) * config->max_components) +
                          (sizeof(static_mesh_instance) * config->max_components) +
                          (sizeof(material_instance) * config->max_components) +
                          (sizeof(bname) * config->max_components) +
                          (sizeof(vec4) * config->max_components) +
                          (sizeof(bname) * config->max_components);

    if (!state_block)
        return true;

    bactor_staticmesh_comp_system_state* state = (bactor_staticmesh_comp_system_state*)state_block;
    state->max_components = config->max_components;
    state->actor_ids = state_block + (sizeof(bactor_staticmesh_comp_system_state));
    state->mesh_instances = (static_mesh_instance*)(((u8*)state->actor_ids) + (sizeof(state->actor_ids[0]) * config->max_components));
    state->names = (bname*)(((u8*)state->mesh_instances) + (sizeof(state->mesh_instances[0]) * config->max_components));
    state->tints = (vec4*)(((u8*)state->names) + (sizeof(state->names[0]) * config->max_components));
    state->resource_names = (bname*)(((u8*)state->tints) + (sizeof(state->tints[0]) * config->max_components));

    // Invalidate all entries in the system
    for (u32 i = 0; i < state->max_components; ++i)
    {
        state->actor_ids[i] = INVALID_ID_U64;
        state->mesh_instances[i].material_instances = 0;
        state->mesh_instances[i].instance_id = INVALID_ID_U64;
        state->mesh_instances[i].mesh_resource = 0;
        state->names[i] = INVALID_BNAME;
        state->resource_names[i] = INVALID_BNAME;
        state->tints[i] = vec4_one(); // default tint is white
    }

    return true;
}

void bactor_comp_staticmesh_system_shutdown(struct bactor_staticmesh_comp_system_state* state)
{
    if (state)
    {
        // TODO: things
        bzero_memory(state, sizeof(bactor_staticmesh_comp_system_state));
    }
}

static u32 get_free_index(struct bactor_staticmesh_comp_system_state* state)
{
    if (!state)
        return INVALID_ID;

    for (u32 i = 0; i < state->max_components; ++i)
    {
        if (!state->mesh_instances[i].mesh_resource)
            return i;
    }

    BERROR("Failed to find free slot for static mesh load. Increase system config->max_components. Current=%u", state->max_components);
    return INVALID_ID;
}

u32 bactor_comp_staticmesh_create(struct bactor_staticmesh_comp_system_state* state, u64 actor_id, bname name, bname mesh_resource_name)
{
    u32 index = get_free_index(state);
    if (index != INVALID_ID)
    {
        state->resource_names[index] = mesh_resource_name;
        state->tints[index] = vec4_one(); // default to white
        state->actor_ids[index] = actor_id;
        state->names[index] = name;
    }

    return index;
}

u32 bactor_comp_staticmesh_get_id(struct bactor_staticmesh_comp_system_state* state, u64 actor_id, bname name)
{
    BASSERT_MSG(state, "state is required");
    if (actor_id == INVALID_ID_U64)
    {
        BERROR("Cannot get the id of a static mesh with an invalid actor id. INVALID_ID will be returned");
        return INVALID_ID;
    }
    if (name == INVALID_BNAME)
    {
        BERROR("Cannot get the id of a static mesh by name when the name is invalid");
        return INVALID_ID;
    }

    // NOTE: There might be a quicker way to do this, but generally these lookups probably shouldn't constantly be done anyway
    for (u32 i = 0; i < state->max_components; ++i)
    {
        if (state->names[i] == name)
            return i;
    }

    return INVALID_ID;
}

bname bactor_comp_staticmesh_name_get(struct bactor_staticmesh_comp_system_state* state, u32 comp_id)
{
    if (!state || comp_id == INVALID_ID)
        return INVALID_BNAME;

    BASSERT_MSG(comp_id < state->max_components, "bactor_comp_staticmesh_name_get - comp_id out of range");
    return state->names[comp_id];
}

b8 bactor_comp_staticmesh_name_set(struct bactor_staticmesh_comp_system_state* state, u32 comp_id, bname name)
{
    if (!state || comp_id == INVALID_ID || name == INVALID_BNAME)
        return false;

    BASSERT_MSG(comp_id < state->max_components, "bactor_comp_staticmesh_name_set - comp_id out of range");
    state->names[comp_id] = name;
    return true;
}

vec4 bactor_comp_staticmesh_tint_get(struct bactor_staticmesh_comp_system_state* state, u32 comp_id)
{
    if (!state || comp_id == INVALID_ID)
        return vec4_one();

    BASSERT_MSG(comp_id < state->max_components, "bactor_comp_staticmesh_tint_get - comp_id out of range");
    return state->tints[comp_id];
}

b8 bactor_comp_staticmesh_tint_set(struct bactor_staticmesh_comp_system_state* state, u32 comp_id, vec4 tint)
{
    if (!state || comp_id == INVALID_ID)
        return false;

    BASSERT_MSG(comp_id < state->max_components, "bactor_comp_staticmesh_tint_set - comp_id out of range");
    state->tints[comp_id] = tint;
    return true;
}

void bactor_comp_staticmesh_destroy(struct bactor_staticmesh_comp_system_state* state, u32 comp_id)
{
    BASSERT_MSG(false, "Not yet implemented");
    // TODO: release resources, then free up "slot" in system
}

b8 bactor_comp_staticmesh_load(struct bactor_staticmesh_comp_system_state* state, u32 comp_id)
{
    if (!state || comp_id == INVALID_ID)
        return false;

    // TODO: Reach out to the static mesh system to get a static_mesh_instance
    BASSERT_MSG(false, "Not yet implemented");

    return true;
}

b8 bactor_comp_staticmesh_unload(struct bactor_staticmesh_comp_system_state* state, u32 comp_id)
{
    BASSERT_MSG(false, "Not yet implemented");
    return false;
}

static_mesh_instance* bactor_comp_staticmesh_get_mesh_instance(struct bactor_staticmesh_comp_system_state* state, u32 comp_id)
{
    if (!state || comp_id == INVALID_ID)
        return 0;

    return &state->mesh_instances[comp_id];
}
