#pragma once

#include "defines.h"
#include "resources/resource_types.h"

#define DEFAULT_MATERIAL_NAME "default"

typedef struct material_system_config
{
    u32 max_material_count;
} material_system_config;

b8 material_system_initialize(u64* memory_requirement, void* state, void* config);
void material_system_shutdown(void* state);

BAPI material* material_system_acquire(const char* name);
BAPI material* material_system_acquire_from_config(material_config config);
BAPI void material_system_release(const char* name);

BAPI material* material_system_get_default(void);

BAPI b8 material_system_apply_global(u32 shader_id, u64 renderer_frame_number, const mat4* projection, const mat4* view, const vec4* ambient_color, const vec3* view_position, u32 render_mode);

BAPI b8 material_system_apply_instance(material* m, b8 needs_update);

BAPI b8 material_system_apply_local(material* m, const mat4* model);

/**
 * @brief Dumps all of the registered materials and their reference counts/handles
 */
BAPI void material_system_dump(void);
