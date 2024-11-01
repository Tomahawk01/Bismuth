#include "bactor.h"
#include "defines.h"
#include "bresources/bresource_types.h"
#include "math/geometry.h"
#include <debug/bassert.h>
#include <logger.h>
#include <memory/bmemory.h>

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
        bzero_memory(state, sizeof(bactor_staticmesh_system_state));
    }
}

static u32 get_free_index(struct bactor_staticmesh_system_state* state)
{
    if (!state)
        return INVALID_ID;

    for (u32 i = 0; i < state->max_components; ++i)
    {
        if (state->materials[i].per_draw_id == INVALID_ID && state->geometries[i].id == INVALID_ID)
            return i;
    }

    BERROR("Failed to find free slot for static mesh load. Increase system config->max_components. Current=%u", state->max_components);
    return INVALID_ID;
}

u32 bactor_comp_staticmesh_create(struct bactor_staticmesh_system_state* state, u64 actor_id, bname name, geometry g, bresource_material_instance material)
{
    u32 index = get_free_index(state);
    if (index != INVALID_ID)
    {
        state->geometries[index] = g;
        state->materials[index] = material;
    }

    return index;
}

u32 bactor_comp_staticmesh_get_id(struct bactor_staticmesh_system_state* state, u64 actor_id, bname name)
{
}

void bactor_comp_staticmesh_destroy(struct bactor_staticmesh_system_state* state, u32 id)
{
}

b8 bactor_comp_staticmesh_load(u32 actor_id)
{
}

b8 bactor_comp_staticmesh_unload(u32 actor_id)
{
}

geometry* bactor_comp_staticmesh_get_geometry(struct bactor_staticmesh_system_state* state, u32 id)
{
    if (!state || id == INVALID_ID)
        return 0;

    return &state->geometries[id];
}

bresource_material_instance* bactor_comp_staticmesh_get_material(struct bactor_staticmesh_system_state* state, u32 id)
{
    if (!state || id == INVALID_ID)
        return 0;

    return &state->materials[id];
}

b8 bactor_comp_staticmesh_get_geometry_material(struct bactor_staticmesh_system_state* state, u32 id, geometry** out_geometry, bresource_material_instance** out_material)
{
    if (!state || id == INVALID_ID)
        return false;

    *out_material = &state->materials[id];
    *out_geometry = &state->geometries[id];

    return true;
}

b8 bactor_comp_staticmesh_get_render_data(struct bactor_staticmesh_system_state* state, u32 id, struct bactor_comp_staticmesh_render_data* out_render_data)
{
    if (!state || !out_render_data || id == INVALID_ID)
        return false;

    out_render_data->material = &state->materials[id];

    geometry* g = &state->geometries[id];

    out_render_data->vertex_count = g->vertex_count;
    out_render_data->vertex_buffer_offset = g->vertex_buffer_offset;
    out_render_data->index_count = g->index_count;
    out_render_data->index_buffer_offset = g->index_buffer_offset;

    return true;
}
