#pragma once

#include "defines.h"
#include "resources/resource_types.h"

#define DEFAULT_PBR_MATERIAL_NAME "default_pbr"
#define DEFAULT_TERRAIN_MATERIAL_NAME "default_terrain"

typedef struct material_system_config
{
    u32 max_material_count;
} material_system_config;

struct frame_data;

b8 material_system_initialize(u64* memory_requirement, void* state, void* config);
void material_system_shutdown(void* state);

BAPI material* material_system_acquire(const char* name);
BAPI material* material_system_acquire_terrain_material(const char* material_name, u32 material_count, const char** material_names, b8 auto_release);
BAPI material* material_system_acquire_from_config(material_config* config);
BAPI void material_system_release(const char* name);

BAPI material* material_system_get_default(void);
BAPI material* material_system_get_default_pbr(void);
BAPI material* material_system_get_default_terrain(void);

BAPI b8 material_system_apply_global(u32 shader_id, struct frame_data* p_frame_data, const mat4* projection, const mat4* view, const vec4* ambient_color, const vec3* view_position, u32 render_mode);
BAPI b8 material_system_apply_instance(material* m, struct frame_data* p_frame_data, b8 needs_update);
BAPI b8 material_system_apply_local(material* m, const mat4* model, struct frame_data* p_frame_data);

BAPI b8 material_system_shadow_map_set(texture* shadow_texture, u8 index);

BAPI b8 material_system_irradiance_set(texture* irradiance_cube_texture);

BAPI void material_system_directional_light_space_set(mat4 directional_light_space, u8 index);

/**
 * @brief Dumps all of the registered materials and their reference counts/handles
 */
BAPI void material_system_dump(void);
