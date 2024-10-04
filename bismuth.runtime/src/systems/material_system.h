#pragma once

#include "defines.h"
#include "bresources/bresource_types.h"

struct material_system_state;

typedef struct material_system_config
{
    u32 max_material_count;
} material_system_config;

b8 material_system_initialize(u64* memory_requirement, struct material_system_state* state, const material_system_config* config);
void material_system_shutdown(struct material_system_state* state);

BAPI bresource_material* material_system_acquire(struct material_system_state* state, bname name);
BAPI void material_system_release(struct material_system_state* state, bresource_material* material);
BAPI const bresource_material* material_system_get_default_unlit(struct material_system_state* state);

BAPI const bresource_material* material_system_get_default_phong(struct material_system_state* state);
BAPI const bresource_material* material_system_get_default_pbr(struct material_system_state* state);
BAPI const bresource_material* material_system_get_default_terrain_pbr(struct material_system_state* state);

// Dumps all of the registered materials and their reference counts/handles
BAPI void material_system_dump(struct material_system_state* state);
