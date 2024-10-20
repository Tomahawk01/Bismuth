#include "bactor.h"
#include "defines.h"
#include "bresources/bresource_types.h"
#include "math/geometry.h"
#include <debug/bassert.h>

typedef struct bactor_staticmesh_system_state
{
    u32 max_components;
    geometry* geometries;
    bresource_material_instance* materials;
} bactor_staticmesh_system_state;

b8 bactor_comp_staticmesh_system_initialize(u64* memory_requirement, void* state_block, const bactor_staticmesh_system_config* config)
{
    BASSERT_MSG(memory_requirement, "bactor_comp_staticmesh_system_initialize requires a valid pointer to memory_requirement");

    *memory_requirement = sizeof(bactor_staticmesh_system_state) + (sizeof(geometry) * config->max_components) + (sizeof(bresource_material_instance) * config->max_components);

    if (!state_block)
        return true;

    bactor_staticmesh_system_state* state = (bactor_staticmesh_system_state*)state_block;
    state->max_components = config->max_components;
    state->geometries = state_block + (sizeof(bactor_staticmesh_system_state));
    state->materials = (bresource_material_instance*)(((u8*)state->geometries) + (sizeof(geometry) * config->max_components));

    // Invalidate all entries in the system
    for (u32 i = 0; i < state->max_components; ++i)
    {
        state->geometries[i].id = INVALID_ID;
        state->materials[i].per_draw_id = INVALID_ID;
    }

    return true;
}

void bactor_comp_staticmesh_system_shutdown(struct bactor_staticmesh_system_state* state)
{
    if (state)
    {
        // TODO: things
    }
}

u64 bactor_comp_staticmesh_create(struct bactor_staticmesh_system_state* state, u64 actor_id, bname name, geometry g, bresource_material_instance material)
{
}

u64 bactor_comp_staticmesh_get_id(struct bactor_staticmesh_system_state* state, u64 actor_id, bname name)
{
}

void bactor_comp_staticmesh_destroy(struct bactor_staticmesh_system_state* state, u64 id)
{
}

b8 bactor_comp_staticmesh_get_geometry(struct bactor_staticmesh_system_state* state, u64 id, geometry* out_geometry)
{
}

b8 bactor_comp_staticmesh_get_material(struct bactor_staticmesh_system_state* state, u64 id, bresource_material_instance* out_material)
{
}

b8 bactor_comp_staticmesh_get_geometry_material(struct bactor_staticmesh_system_state* state, u64 id, geometry* out_geometry, bresource_material_instance* out_material)
{
}
