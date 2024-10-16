#pragma once

#include "containers/hashtable.h"
#include "core_render_types.h"
#include "defines.h"
#include "renderer/renderer_types.h"
#include "resources/resource_types.h"

typedef struct shader_system_config
{
    // The maximum number of shaders held in the system. NOTE: Should be at least 512
    u16 max_shader_count;
    // @brief The maximum number of uniforms allowed in a single shader
    u8 max_uniform_count;
    // The maximum number of per-frame textures allowed in a single shader
    u8 max_per_frame_textures;
    // The maximum number of per-group textures allowed in a single shader
    u8 max_per_group_textures;
} shader_system_config;

typedef enum shader_state
{
    // The shader has not yet gone through the creation process, and is unusable
    SHADER_STATE_NOT_CREATED,
    // The shader has gone through the creation process, but not initialization. It is unusable
    SHADER_STATE_UNINITIALIZED,
    // The shader is created and initialized, and is ready for use
    SHADER_STATE_INITIALIZED,
} shader_state;

typedef struct shader_uniform
{
    // The offset in bytes from the beginning of the uniform set (global/instance/local)
    u64 offset;
    // The location to be used as a lookup
    u16 location;
    // Index into the internal uniform array
    u16 index;
    // The size of the uniform, or 0 for samplers
    u16 size;
    // The index of the descriptor set the uniform belongs to (0=per_frame, 1=per_group, INVALID_ID=per_draw)
    u8 set_index;
    // The update frequency of the uniform
    shader_update_frequency frequency;
    // The type of uniform
    shader_uniform_type type;
    u32 array_length;
} shader_uniform;

/**
 * @brief Represents a single shader vertex attribute
 */
typedef struct shader_attribute
{
    // The attribute name
    char* name;
    // The attribute type
    shader_attribute_type type;
    // The attribute size in bytes
    u32 size;
} shader_attribute;

typedef enum shader_flags
{
    SHADER_FLAG_NONE = 0x00,
    SHADER_FLAG_DEPTH_TEST = 0x01,
    SHADER_FLAG_DEPTH_WRITE = 0x02,
    SHADER_FLAG_WIREFRAME = 0x04,
    SHADER_FLAG_STENCIL_TEST = 0x08,
    SHADER_FLAG_STENCIL_WRITE = 0x10,
    SHADER_FLAG_COLOR_READ = 0x20,
    SHADER_FLAG_COLOR_WRITE = 0x40
} shader_flags;

typedef u32 shader_flag_bits;

/**
 * @brief Represents a shader on the frontend
 */
typedef struct shader
{
    // Shader identifier
    u32 id;

    char* name;

    shader_flag_bits flags;

    u32 topology_types;

    u64 required_ubo_alignment;

    // The actual size of the per_frame uniform buffer object
    u64 per_frame_ubo_size;
    // The stride of the per_frame uniform buffer object
    u64 per_frame_ubo_stride;

    u64 per_frame_ubo_offset;

    // The actual size of the per_group uniform buffer object
    u64 per_group_ubo_size;

    // The stride of the per_group uniform buffer object
    u64 per_group_ubo_stride;

    u64 per_draw_ubo_offset;
    u64 per_draw_ubo_size;
    u64 per_draw_ubo_stride;

    // An array of per_frame texture map pointers
    bresource_texture_map** per_frame_texture_maps;

    // The number of per_group textures
    u8 per_group_texture_count;

    // The identifier of the currently bound group
    u32 bound_per_group_id;

    // The number of per_draw textures
    u8 per_draw_texture_count;

    // The identifier of the currently bound per_draw resource
    u32 bound_per_draw_id;

    // The block of memory used by the uniform hashtable
    void* hashtable_block;
    // A hashtable to store uniform index/locations by name
    hashtable uniform_lookup;

    // An array of uniforms in this shader. Darray
    shader_uniform* uniforms;

    // The number of per_frame non-sampler uniforms
    u8 per_frame_uniform_count;
    // The number of per_frame sampler uniforms
    u8 per_frame_uniform_sampler_count;
    // darray Keeps the uniform indices of per_frame samplers for fast lookups
    u32* per_frame_sampler_indices;
    // The number of per_group non-sampler uniforms
    u8 per_group_uniform_count;
    // The number of per_group sampler uniforms
    u8 per_group_uniform_sampler_count;
    // darray Keeps the uniform indices of per_group samplers for fast lookups
    u32* per_group_sampler_indices;
    // The number of per_draw non-sampler uniforms
    u8 per_draw_uniform_count;
    // The number of per_draw sampler uniforms
    u8 per_draw_uniform_sampler_count;
    // darray Keeps the uniform indices of per_draw samplers for fast lookups
    u32* per_draw_sampler_indices;

    // An array of attributes. Darray
    shader_attribute* attributes;

    // The internal state of the shader
    shader_state state;

    // The size of all attributes combined, a.k.a. the size of a vertex
    u16 attribute_stride;

    u8 shader_stage_count;
    shader_stage_config* stage_configs;

    b8 is_wireframe;

    // An opaque pointer to hold renderer API specific data. Renderer is responsible for creation and destruction of this
    void* internal_data;

#ifdef _DEBUG
    u32* module_watch_ids;
#endif
} shader;

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
 * @brief Creates a new shader with the given config.
 * 
 * @param pass Pointer to the renderpass to be used with this shader.
 * @param config The configuration to be used when creating the shader.
 * @return True on success; otherwise false.
 */
BAPI b8 shader_system_create(const shader_config* config);

/**
 * @brief Reloads the given shader
 *
 * @param shader_id The id of the shader to reload
 * @return True on success; otherwise false
 */
BAPI b8 shader_system_reload(u32 shader_id);

/**
 * @brief Gets the identifier of a shader by name.
 * 
 * @param shader_name The name of the shader.
 * @return The shader id, if found; otherwise INVALID_ID.
 */
BAPI u32 shader_system_get_id(const char* shader_name);

/**
 * @brief Returns a pointer to a shader with the given identifier.
 * 
 * @param shader_id The shader identifier.
 * @return A pointer to a shader, if found; otherwise 0.
 */
BAPI shader* shader_system_get_by_id(u32 shader_id);

/**
 * @brief Returns a pointer to a shader with the given name.
 * Attempts to load the shader if not already loaded.
 * 
 * @param shader_id The id of the shader to set wireframe mode for
 * @return A pointer to a shader, if found/loaded; otherwise 0.
 */
BAPI shader* shader_system_get(const char* shader_name);

BAPI b8 shader_system_set_wireframe(u32 shader_id, b8 wireframe_enabled);

/**
 * @brief Uses the shader with the given identifier.
 * 
 * @param shader_id The identifier of the shader to be used.
 * @return True on success; otherwise false.
 */
BAPI b8 shader_system_use_by_id(u32 shader_id);

/**
 * @brief Returns the uniform location for a uniform with the given name, if found.
 * 
 * @param shader_id The id of the shader to obtain the location from
 * @param uniform_name The name of the uniform to search for.
 * @return The uniform location, if found; otherwise INVALID_ID_U16.
 */
BAPI u16 shader_system_uniform_location(u32 shader_id, const char* uniform_name);

/**
 * @brief Sets the value of a uniform with the given name to the supplied value.
 * 
 * @param shader_id The identifier of the shader to update.
 * @param uniform_name The name of the uniform to be set.
 * @param value The value to be set.
 * @return True on success; otherwise false.
 */
BAPI b8 shader_system_uniform_set(u32 shader_id, const char* uniform_name, const void* value);

/**
 * @brief Sets the value of an arrayed uniform with the given name to the supplied value.
 *
 * @param shader_id The identifier of the shader to update.
 * @param uniform_name The name of the uniform to be set.
 * @param array_index The index into the uniform array, if the uniform is in fact an array. Otherwise ignored.
 * @param value The value to be set.
 * @return True on success; otherwise false.
 */
BAPI b8 shader_system_uniform_set_arrayed(u32 shader_id, const char* uniform_name, u32 array_index, const void* value);

/**
 * @brief Sets the texture of a sampler with the given name to the supplied texture.
 * 
 * @param shader_id The identifier of the shader to update.
 * @param uniform_name The name of the uniform to be set.
 * @param t A pointer to the texture to be set.
 * @return True on success; otherwise false.
 */
BAPI b8 shader_system_sampler_set(u32 shader_id, const char* sampler_name, const bresource_texture* t);

/**
 * @brief Sets the texture of an arrayed sampler with the given name to the supplied texture.
 *
 * @param shader_id The identifier of the shader to update.
 * @param uniform_name The name of the uniform to be set.
 * @param array_index The index into the uniform array, if the uniform is in fact an array. Otherwise ignored.
 * @param t A pointer to the texture to be set.
 * @return True on success; otherwise false.
 */
BAPI b8 shader_system_sampler_set_arrayed(u32 shader_id, const char* sampler_name, u32 array_index, const bresource_texture* t);

/**
 * @brief Sets a uniform value by location.
 *
 * @param shader_id The identifier of the shader to update.
 * @param index The location of the uniform.
 * @param value The value of the uniform.
 * @return True on success; otherwise false.
 */
BAPI b8 shader_system_uniform_set_by_location(u32 shader_id, u16 location, const void* value);
BAPI b8 shader_system_uniform_set_by_location_arrayed(u32 shader_id, u16 location, u32 array_index, const void* value);

BAPI b8 shader_system_sampler_set_by_location(u32 shader_id, u16 location, const struct bresource_texture* t);
BAPI b8 shader_system_sampler_set_by_location_arrayed(u32 shader_id, u16 location, u32 array_index, const struct bresource_texture* t);

BAPI b8 shader_system_bind_group(u32 shader_id, u32 instance_id);
BAPI b8 shader_system_bind_draw_id(u32 shader_id, u32 local_id);
BAPI b8 shader_system_apply_per_frame(u32 shader_id);
BAPI b8 shader_system_apply_per_group(u32 shader_id);
BAPI b8 shader_system_apply_per_draw(u32 shader_id);

BAPI b8 shader_system_shader_group_acquire(u32 shader_id, u32 map_count, bresource_texture_map** maps, u32* out_group_id);
BAPI b8 shader_system_shader_group_release(u32 shader_id, u32 instance_id, u32 map_count, bresource_texture_map* maps);
BAPI b8 shader_system_shader_per_draw_acquire(u32 shader_id, u32 map_count, bresource_texture_map** maps, u32* out_per_draw_id);
BAPI b8 shader_system_shader_per_draw_release(u32 shader_id, u32 per_draw_id, u32 map_count, bresource_texture_map* maps);
