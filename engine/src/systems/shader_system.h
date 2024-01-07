#pragma once

#include "defines.h"
#include "renderer/renderer_types.h"
#include "containers/hashtable.h"
#include "resources/resource_types.h"

struct frame_data;

typedef struct shader_system_config
{
    // The maximum number of shaders held in the system. NOTE: Should be at least 512
    u16 max_shader_count;
    // @brief The maximum number of uniforms allowed in a single shader
    u8 max_uniform_count;
    // The maximum number of global-scope textures allowed in a single shader
    u8 max_global_textures;
    // The maximum number of instance-scope textures allowed in a single shader
    u8 max_instance_textures;
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
    // The index of the descriptor set the uniform belongs to (0=global, 1=instance, INVALID_ID=local)
    u8 set_index;
    // The scope of the uniform
    shader_scope scope;
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
    SHADER_FLAG_STENCIL_WRITE = 0x10
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

    // The actual size of the global uniform buffer object
    u64 global_ubo_size;
    // The stride of the global uniform buffer object
    u64 global_ubo_stride;

    u64 global_ubo_offset;

    // The actual size of the instance uniform buffer object
    u64 ubo_size;

    // The stride of the instance uniform buffer object
    u64 ubo_stride;

    u64 local_ubo_offset;
    u64 local_ubo_size;
    u64 local_ubo_stride;

    // An array of global texture map pointers
    texture_map** global_texture_maps;

    // The number of instance textures
    u8 instance_texture_count;

    shader_scope bound_scope;

    // The identifier of the currently bound instance
    u32 bound_instance_id;
    // The currently bound instance's ubo offset
    u32 bound_ubo_offset;

    // The block of memory used by the uniform hashtable
    void* hashtable_block;
    // A hashtable to store uniform index/locations by name
    hashtable uniform_lookup;

    // An array of uniforms in this shader. Darray
    shader_uniform* uniforms;

    // The number of global non-sampler uniforms
    u8 global_uniform_count;
    // The number of global sampler uniforms
    u8 global_uniform_sampler_count;
    // darray. Keeps the uniform indices of global samplers for fast lookups
    u32* global_sampler_indices;
    // The number of instance non-sampler uniforms
    u8 instance_uniform_count;
    // The number of instance sampler uniforms
    u8 instance_uniform_sampler_count;
    // darray. Keeps the uniform indices of instance samplers for fast lookups
    u32* instance_sampler_indices;
    // The number of local non-sampler uniforms
    u8 local_uniform_count;

    // An array of attributes. Darray
    shader_attribute* attributes;

    // The internal state of the shader
    shader_state state;

    // The size of all attributes combined, a.k.a. the size of a vertex
    u16 attribute_stride;

    // Used to ensure shader's globals are only updated once per frame
    u64 render_frame_number;

    u8 draw_index;

    u8 shader_stage_count;
    shader_stage_config* stage_configs;

    // An opaque pointer to hold renderer API specific data. Renderer is responsible for creation and destruction of this
    void* internal_data;
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
BAPI b8 shader_system_create(renderpass* pass, const shader_config* config);

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
 * 
 * @param shader_name The name to search for. Case sensitive.
 * @return A pointer to a shader, if found; otherwise 0.
 */
BAPI shader* shader_system_get(const char* shader_name);

/**
 * @brief Uses the shader with the given name.
 * 
 * @param shader_name The name of the shader to use. Case sensitive.
 * @return True on success; otherwise false.
 */
BAPI b8 shader_system_use(const char* shader_name);

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
 * @param s A pointer to the shader to obtain the location from.
 * @param uniform_name The name of the uniform to search for.
 * @return The uniform location, if found; otherwise INVALID_ID_U16.
 */
BAPI u16 shader_system_uniform_location(shader* s, const char* uniform_name);

/**
 * @brief Sets the value of a uniform with the given name to the supplied value.
 * NOTE: Operates against the currently-used shader.
 * 
 * @param uniform_name The name of the uniform to be set.
 * @param value The value to be set.
 * @return True on success; otherwise false.
 */
BAPI b8 shader_system_uniform_set(const char* uniform_name, const void* value);

/**
 * @brief Sets the value of an arrayed uniform with the given name to the supplied value.
 * NOTE: Operates against the currently-used shader.
 *
 * @param uniform_name The name of the uniform to be set.
 * @param array_index The index into the uniform array, if the uniform is in fact an array. Otherwise ignored.
 * @param value The value to be set.
 * @return True on success; otherwise false.
 */
BAPI b8 shader_system_uniform_set_arrayed(const char* uniform_name, u32 array_index, const void* value);

/**
 * @brief Sets the texture of a sampler with the given name to the supplied texture.
 * NOTE: Operates against the currently-used shader.
 * 
 * @param uniform_name The name of the uniform to be set.
 * @param t A pointer to the texture to be set.
 * @return True on success; otherwise false.
 */
BAPI b8 shader_system_sampler_set(const char* sampler_name, const texture* t);

/**
 * @brief Sets the texture of an arrayed sampler with the given name to the supplied texture.
 * NOTE: Operates against the currently-used shader.
 *
 * @param uniform_name The name of the uniform to be set.
 * @param array_index The index into the uniform array, if the uniform is in fact an array. Otherwise ignored.
 * @param t A pointer to the texture to be set.
 * @return True on success; otherwise false.
 */
BAPI b8 shader_system_sampler_set_arrayed(const char* sampler_name, u32 array_index, const texture* t);

/**
 * @brief Sets a uniform value by location.
 * NOTE: Operates against the currently-used shader.
 *
 * @param index The location of the uniform.
 * @param value The value of the uniform.
 * @return True on success; otherwise false.
 */
BAPI b8 shader_system_uniform_set_by_location(u16 location, const void* value);

BAPI b8 shader_system_uniform_set_by_location_arrayed(u16 location, u32 array_index, const void* value);

BAPI b8 shader_system_sampler_set_by_location(u16 location, const struct texture* t);

BAPI b8 shader_system_sampler_set_by_location_arrayed(u16 location, u32 array_index, const struct texture* t);

/**
 * @brief Applies global-scoped uniforms.
 * NOTE: Operates against the currently-used shader.
 * 
 * @return True on success; otherwise false.
 */
BAPI b8 shader_system_apply_global(b8 needs_update, struct frame_data* p_frame_data);

/**
 * @brief Applies instance-scoped uniforms.
 * NOTE: Operates against the currently-used shader.
 * @param needs_update Indicates if shader needs uniform updates or just needs to be bound.
 * 
 * @return True on success; otherwise false.
 */
BAPI b8 shader_system_apply_instance(b8 needs_update, struct frame_data* p_frame_data);

/**
 * @brief Binds the instance with the given id for use. Must be done before setting
 * instance-scoped uniforms.
 * NOTE: Operates against the currently-used shader.
 * 
 * @param instance_id The identifier of the instance to bind.
 * @return True on success; otherwise false. 
 */
BAPI b8 shader_system_bind_instance(u32 instance_id);

/**
 * @brief Applies local-scoped uniforms.
 * NOTE: Operates against the currently-used shader.
 *
 * @return True on success; otherwise false.
 */
BAPI b8 shader_system_apply_local(struct frame_data* p_frame_data);

/**
 * @brief Binds the instance with the given id for use. Must be done before setting local-scoped uniforms.
 * NOTE: Operates against the currently-used shader.
 *
 * @return True on success; otherwise false.
 */
BAPI b8 shader_system_bind_local(void);
