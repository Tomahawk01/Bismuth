#pragma once

#include "core_render_types.h"
#include "defines.h"
#include "bresources/bresource_types.h"

typedef struct shader_system_config
{
    // The maximum number of shaders held in the system. NOTE: Should be at least 512
    u16 max_shader_count;
    // The maximum number of uniforms allowed in a single shader
    u8 max_uniform_count;
} shader_system_config;

/**
 * @brief Initializes the shader system using the supplied configuration.
 * NOTE: Call this twice, once to obtain memory requirement (memory = 0) and a second time
 * including allocated memory.
 * 
 * @param memory_requirement A pointer to hold the memory requirement of this system in bytes.
 * @param memory A memory block to be used to hold the state of this system. Pass 0 on the first call to get memory requirement.
 * @param config The configuration to be used when initializing the system.
 * @return b8 True on success; otherwise false.
 */
b8 shader_system_initialize(u64* memory_requirement, void* memory, void* config);

/**
 * @brief Shuts down the shader system.
 * 
 * @param state A pointer to the system state.
 */
void shader_system_shutdown(void* state);

/**
 * @brief Reloads the given shader
 *
 * @param shader A handle to the shader to reload
 * @return True on success; otherwise false
 */
BAPI b8 shader_system_reload(bhandle shader);

/**
 * @brief Returns a handle to a shader with the given name.
 * Attempts to load the shader if not already loaded
 * 
 * @param shader_name The bname to search for
 * @param package_name The package to get the shader from if not already loaded. Pass INVALID_BNAME to search all packages
 * @return A handle to a shader, if found/loaded; otherwise an invalid handle
 */
BAPI bhandle shader_system_get(bname name, bname package_name);

/**
 * @brief Returns a handle to a shader with the given name based on the provided config source.
 * Attempts to load the shader if not already loaded
 *
 * @param shader_name The name of the new shader
 * @param shader_config_source A string containing the shader's configuration source as if it were loaded from an asset
 * @return A handle to a shader, if loaded; otherwise an invalid handle
 */
BAPI bhandle shader_system_get_from_source(bname name, const char* shader_config_source);

/**
 * @brief Attempts to destroy the shader with the given handle. Handle will be invalidated
 * 
 * @param shader_name A pointer to a handle to the shader to destroy. Handle will be invalidated
 */
BAPI void shader_system_destroy(bhandle* shader);

/**
 * @brief Attempts to set wireframe mode on the given shader. If the renderer backend, or the shader
 * does not support this , it will fail when attempting to enable. Disabling will always succeed.
 * 
 * @param shader A handle to the shader to set wireframe mode for
 * @param wireframe_enabled Indicates if wireframe mode should be enabled
 * @return True on success; otherwise false
 */
BAPI b8 shader_system_set_wireframe(bhandle shader, b8 wireframe_enabled);

/**
 * @brief Uses the shader with the given handle
 * 
 * @param shader A handle to the shader to be used
 * @return True on success; otherwise false
 */
BAPI b8 shader_system_use(bhandle shader);

/**
 * @brief Returns the uniform location for a uniform with the given name, if found.
 * 
 * @param shader A handle to the shader to obtain the location from
 * @param uniform_name The name of the uniform to search for.
 * @return The uniform location, if found; otherwise INVALID_ID_U16.
 */
BAPI u16 shader_system_uniform_location(bhandle shader, bname uniform_name);

/**
 * @brief Sets the value of a uniform with the given name to the supplied value.
 * 
 * @param shader A handle to the shader to update.
 * @param uniform_name The name of the uniform to be set.
 * @param value The value to be set.
 * @return True on success; otherwise false.
 */
BAPI b8 shader_system_uniform_set(bhandle shader, bname uniform_name, const void* value);

/**
 * @brief Sets the value of an arrayed uniform with the given name to the supplied value.
 *
 * @param shader A handle to the shader to update.
 * @param uniform_name The name of the uniform to be set.
 * @param array_index The index into the uniform array, if the uniform is in fact an array. Otherwise ignored.
 * @param value The value to be set.
 * @return True on success; otherwise false.
 */
BAPI b8 shader_system_uniform_set_arrayed(bhandle shader, bname uniform_name, u32 array_index, const void* value);

/**
 * @brief Sets the texture uniform with the given name to the supplied texture.
 * 
 * @param shader A handle to the shader to update.
 * @param uniform_name The name of the uniform to be set.
 * @param t A pointer to the texture to be set.
 * @return True on success; otherwise false.
 */
BAPI b8 shader_system_texture_set(bhandle shader, bname sampler_name, const bresource_texture* t);

/**
 * @brief Sets the arrayed texture uniform with the given name to the supplied texture at the given index
 *
 * @param shader A handle to the shader to update.
 * @param uniform_name The name of the uniform to be set.
 * @param array_index The index into the uniform array, if the uniform is in fact an array. Otherwise ignored.
 * @param t A pointer to the texture to be set.
 * @return True on success; otherwise false.
 */
BAPI b8 shader_system_texture_set_arrayed(bhandle shader, bname uniform_name, u32 array_index, const bresource_texture* t);

/**
 * @brief Sets a uniform value by location
 *
 * @param shader A handle to the shader to update
 * @param index The location of the uniform
 * @param value The value of the uniform
 * @return True on success; otherwise false
 */
BAPI b8 shader_system_uniform_set_by_location(bhandle shader, u16 location, const void* value);
BAPI b8 shader_system_uniform_set_by_location_arrayed(bhandle shader, u16 location, u32 array_index, const void* value);

BAPI b8 shader_system_texture_set_by_location(bhandle shader, u16 location, const struct bresource_texture* t);
BAPI b8 shader_system_texture_set_by_location_arrayed(bhandle shader, u16 location, u32 array_index, const struct bresource_texture* value);
BAPI b8 shader_system_sampler_set_by_location_arrayed(bhandle shader, u16 location, u32 array_index, const struct bresource_texture* t);

BAPI b8 shader_system_bind_frame(bhandle shader);
BAPI b8 shader_system_bind_group(bhandle shader, u32 group_id);
BAPI b8 shader_system_bind_draw_id(bhandle shader, u32 draw_id);

BAPI b8 shader_system_apply_per_frame(bhandle shader);
BAPI b8 shader_system_apply_per_group(bhandle shader);
BAPI b8 shader_system_apply_per_draw(bhandle shader);

BAPI b8 shader_system_shader_group_acquire(bhandle shader, u32* out_group_id);
BAPI b8 shader_system_shader_group_release(bhandle shader, u32 instance_id);
BAPI b8 shader_system_shader_per_draw_acquire(bhandle shader, u32* out_per_draw_id);
BAPI b8 shader_system_shader_per_draw_release(bhandle shader, u32 per_draw_id);
