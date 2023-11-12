#pragma once

#include "defines.h"
#include "resources/resource_types.h"

#define DEFAULT_MATERIAL_NAME "default"
#define DEFAULT_UI_MATERIAL_NAME "default_ui"
#define DEFAULT_TERRAIN_MATERIAL_NAME "default_terrain"

typedef struct material_system_config
{
    u32 max_material_count;
} material_system_config;

b8 material_system_initialize(u64* memory_requirement, void* state, void* config);
void material_system_shutdown(void* state);

BAPI material* material_system_acquire(const char* name);
BAPI material* material_system_acquire_terrain_material(const char* material_name, u32 material_count, const char** material_names, b8 auto_release);
BAPI material* material_system_acquire_from_config(material_config* config);
BAPI void material_system_release(const char* name);

BAPI material* material_system_get_default(void);
BAPI material* material_system_get_default_ui(void);
BAPI material* material_system_get_default_terrain(void);

BAPI b8 material_system_apply_global(u32 shader_id, u64 renderer_frame_number, const mat4* projection, const mat4* view, const vec4* ambient_color, const vec3* view_position, u32 render_mode);
BAPI b8 material_system_apply_instance(material* m, b8 needs_update);
BAPI b8 material_system_apply_local(material* m, const mat4* model);

/**
 * @brief Dumps all of the registered materials and their reference counts/handles
 */
BAPI void material_system_dump(void);
