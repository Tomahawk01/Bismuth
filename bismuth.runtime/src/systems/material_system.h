#pragma once

#include "defines.h"
#include "bresources/bresource_types.h"

struct material_system_state;

typedef struct material_system_config
{
    u32 max_material_count;
} material_system_config;

typedef struct material_instance
{
    const bresource_material* material;

    u32 per_draw_id;
} material_instance;

b8 material_system_initialize(u64* memory_requirement, struct material_system_state* state, const material_system_config* config);
void material_system_shutdown(struct material_system_state* state);

BAPI b8 material_system_acquire(struct material_system_state* state, bname name, material_instance* out_instance);
BAPI void material_system_release_instance(struct material_system_state* state, material_instance* instance);
BAPI material_instance material_system_get_default_unlit(struct material_system_state* state);

BAPI material_instance material_system_get_default_phong(struct material_system_state* state);
BAPI material_instance material_system_get_default_pbr(struct material_system_state* state);
BAPI material_instance material_system_get_default_terrain_pbr(struct material_system_state* state);

// Dumps all of the registered materials and their reference counts/handles
BAPI void material_system_dump(struct material_system_state* state);
