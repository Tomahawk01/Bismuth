#pragma once

#include "math/math_types.h"

// Pre-defined resource types
typedef enum resource_type
{
    RESOURCE_TYPE_TEXT,
    RESOURCE_TYPE_BINARY,
    RESOURCE_TYPE_IMAGE,
    RESOURCE_TYPE_MATERIAL,
    RESOURCE_TYPE_STATIC_MESH,
    RESOURCE_TYPE_SHADER,
    RESOURCE_TYPE_CUSTOM
} resource_type;

typedef struct resource
{
    u32 loader_id;
    const char* name;
    char* full_path;
    u64 data_size;
    void* data;
} resource;

typedef struct image_resource_data
{
    u8 channel_count;
    u32 width;
    u32 height;
    u8* pixels;
} image_resource_data;

#define TEXTURE_NAME_MAX_LENGTH 512
typedef struct texture
{
    u32 id;
    u32 width;
    u32 height;
    u8 channel_count;
    b8 has_transparency;
    u32 generation;
    char name[TEXTURE_NAME_MAX_LENGTH];
    void* internal_data;
} texture;

typedef enum texture_use
{
    TEXTURE_USE_UNKNOWN = 0x00,
    TEXTURE_USE_MAP_DIFFUSE = 0x01,
    TEXTURE_USE_MAP_SPECULAR = 0x02,
    TEXTURE_USE_MAP_NORMAL = 0x03
} texture_use;

typedef struct texture_map
{
    texture* texture;
    texture_use use;
} texture_map;

#define MATERIAL_NAME_MAX_LENGTH 256

typedef struct material_config
{
    char name[MATERIAL_NAME_MAX_LENGTH];
    char* shader_name;
    b8 auto_release;
    vec4 diffuse_color;
    f32 shininess;
    char diffuse_map_name[TEXTURE_NAME_MAX_LENGTH];
    char specular_map_name[TEXTURE_NAME_MAX_LENGTH];
    char normal_map_name[TEXTURE_NAME_MAX_LENGTH];
} material_config;

typedef struct material
{
    u32 id;
    u32 generation;
    u32 internal_id;
    char name[MATERIAL_NAME_MAX_LENGTH];
    vec4 diffuse_color;
    texture_map diffuse_map;
    texture_map specular_map;
    texture_map normal_map;

    f32 shininess;

    u32 shader_id;
} material;

#define GEOMETRY_NAME_MAX_LENGTH 256
/**
 * @brief Represents actual geometry in the world.
 * Typically (depending on use) paired with a material.
 */
typedef struct geometry
{
    u32 id;
    u32 internal_id;
    u32 generation;
    char name[GEOMETRY_NAME_MAX_LENGTH];
    material* material;
} geometry;

// Shader stages available in the system
typedef enum shader_stage
{
    SHADER_STAGE_VERTEX = 0x00000001,
    SHADER_STAGE_GEOMETRY = 0x00000002,
    SHADER_STAGE_FRAGMENT = 0x00000004,
    SHADER_STAGE_COMPUTE = 0x0000008
} shader_stage;

// Available attribute types
typedef enum shader_attribute_type
{
    SHADER_ATTRIB_TYPE_FLOAT32 = 0U,
    SHADER_ATTRIB_TYPE_FLOAT32_2 = 1U,
    SHADER_ATTRIB_TYPE_FLOAT32_3 = 2U,
    SHADER_ATTRIB_TYPE_FLOAT32_4 = 3U,
    SHADER_ATTRIB_TYPE_MATRIX_4 = 4U,
    SHADER_ATTRIB_TYPE_INT8 = 5U,
    SHADER_ATTRIB_TYPE_UINT8 = 6U,
    SHADER_ATTRIB_TYPE_INT16 = 7U,
    SHADER_ATTRIB_TYPE_UINT16 = 8U,
    SHADER_ATTRIB_TYPE_INT32 = 9U,
    SHADER_ATTRIB_TYPE_UINT32 = 10U,
} shader_attribute_type;

// Available uniform types
typedef enum shader_uniform_type
{
    SHADER_UNIFORM_TYPE_FLOAT32 = 0U,
    SHADER_UNIFORM_TYPE_FLOAT32_2 = 1U,
    SHADER_UNIFORM_TYPE_FLOAT32_3 = 2U,
    SHADER_UNIFORM_TYPE_FLOAT32_4 = 3U,
    SHADER_UNIFORM_TYPE_INT8 = 4U,
    SHADER_UNIFORM_TYPE_UINT8 = 5U,
    SHADER_UNIFORM_TYPE_INT16 = 6U,
    SHADER_UNIFORM_TYPE_UINT16 = 7U,
    SHADER_UNIFORM_TYPE_INT32 = 8U,
    SHADER_UNIFORM_TYPE_UINT32 = 9U,
    SHADER_UNIFORM_TYPE_MATRIX_4 = 10U,
    SHADER_UNIFORM_TYPE_SAMPLER = 11U,
    SHADER_UNIFORM_TYPE_CUSTOM = 255U
} shader_uniform_type;

// Defines shader scope, which indicates how often it gets updated
typedef enum shader_scope
{
    // Global shader scope, generally updated once per frame
    SHADER_SCOPE_GLOBAL = 0,
    // Instance shader scope, generally updated "per-instance" of the shader
    SHADER_SCOPE_INSTANCE = 1,
    // Local shader scope, generally updated per-object
    SHADER_SCOPE_LOCAL = 2
} shader_scope;

// Configuration for an attribute
typedef struct shader_attribute_config
{
    // The length of the name
    u8 name_length;
    // The name of the attribute
    char* name;
    // The size of the attribute
    u8 size;
    // The type of the attribute
    shader_attribute_type type;
} shader_attribute_config;

// Configuration for a uniform
typedef struct shader_uniform_config
{
    // The length of the name
    u8 name_length;
    // The name of the uniform
    char* name;
    // The size of the uniform
    u8 size;
    // The location of the uniform
    u32 location;
    // The type of the uniform
    shader_uniform_type type;
    // The scope of the uniform
    shader_scope scope;
} shader_uniform_config;

// Configuration for a shader
typedef struct shader_config
{
    // The name of the shader to be created
    char* name;

    // Indicates if the shader uses instance-level uniforms
    b8 use_instances;
    // Indicates if the shader uses local-level uniforms
    b8 use_local;

    // The count of attributes
    u8 attribute_count;
    // The collection of attributes. Darray
    shader_attribute_config* attributes;

    // The count of uniforms
    u8 uniform_count;
    // The collection of uniforms. Darray
    shader_uniform_config* uniforms;

    // The name of the renderpass used by this shader
    char* renderpass_name;

    // The number of stages present in the shader
    u8 stage_count;
    // The collection of stages. Darray
    shader_stage* stages;
    // The collection of stage names. Must align with stages array. Darray
    char** stage_names;
    // The collection of stage file names to be loaded (one per stage). Must align with stages array. Darray
    char** stage_filenames;
} shader_config;
