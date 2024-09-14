#pragma once

#include "defines.h"
#include "resources/resource_types.h"

#define DEFAULT_PBR_MATERIAL_NAME "default_pbr"
#define DEFAULT_TERRAIN_MATERIAL_NAME "default_terrain"

struct material_system_state;

typedef struct material_system_config
{
    u32 max_material_count;
} material_system_config;

struct frame_data;

b8 material_system_initialize(u64* memory_requirement, struct material_system_state* state, const material_system_config* config);
void material_system_shutdown(struct material_system_state* state);

BAPI material* material_system_acquire(const char* name);
BAPI material* material_system_acquire_terrain_material(const char* material_name, u32 material_count, const char** material_names, b8 auto_release);
BAPI material* material_system_acquire_from_config(material_config* config);
BAPI void material_system_release(const char* name);

BAPI material* material_system_get_default(void);
BAPI material* material_system_get_default_pbr(void);
BAPI material* material_system_get_default_terrain(void);

// Dumps all of the registered materials and their reference counts/handles
BAPI void material_system_dump(void);
