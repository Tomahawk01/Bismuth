#pragma once

#include "identifiers/identifier.h"
#include "bresources/bresource_types.h"
#include "math/math_types.h"
#include "strings/bname.h"

#include <core_render_types.h>

#define TERRAIN_MAX_MATERIAL_COUNT 4

// Pre-defined resource types
typedef enum resource_type
{
    RESOURCE_TYPE_TEXT,
    RESOURCE_TYPE_BINARY,
    RESOURCE_TYPE_IMAGE,
    RESOURCE_TYPE_MATERIAL,
    RESOURCE_TYPE_SHADER,
    RESOURCE_TYPE_MESH,
    RESOURCE_TYPE_BITMAP_FONT,
    RESOURCE_TYPE_SYSTEM_FONT,
    RESOURCE_TYPE_scene,
    RESOURCE_TYPE_TERRAIN,
    RESOURCE_TYPE_AUDIO,
    RESOURCE_TYPE_CUSTOM
} resource_type;

// Magic number indicating the file is a bismuth binary file
#define RESOURCE_MAGIC 0xdeadbeef

typedef struct resource_header
{
    u32 magic_number;
    u8 resource_type;
    u8 version;
    u16 reserved;
} resource_header;

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
    u32 mip_levels;
} image_resource_data;

typedef struct image_resource_params
{
    b8 flip_y;
} image_resource_params;

typedef enum texture_flag
{
    TEXTURE_FLAG_HAS_TRANSPARENCY = 0x01,
    TEXTURE_FLAG_IS_WRITEABLE = 0x02,
    TEXTURE_FLAG_IS_WRAPPED = 0x04,
    TEXTURE_FLAG_DEPTH = 0x08,
    TEXTURE_FLAG_RENDERER_BUFFERING = 0x10
} texture_flag;

typedef u8 texture_flag_bits;

typedef enum texture_type
{
    TEXTURE_TYPE_2D,
    TEXTURE_TYPE_2D_ARRAY,
    TEXTURE_TYPE_CUBE,
    TEXTURE_TYPE_CUBE_ARRAY, // NOTE: Cube array texture, used for arrays of cubemaps
    TEXTURE_TYPE_COUNT
} texture_type;

#define TEXTURE_NAME_MAX_LENGTH 512
#define MATERIAL_NAME_MAX_LENGTH 256

struct material;

struct geometry_config;
typedef struct mesh_config
{
    char* resource_name;
    u16 geometry_count;
    struct geometry_config* g_configs;
} mesh_config;

typedef enum mesh_state
{
    MESH_STATE_UNDEFINED,
    MESH_STATE_CREATED,
    MESH_STATE_INITIALIZED,
    MESH_STATE_LOADING,
    MESH_STATE_LOADED
} mesh_state;

struct geometry;
typedef struct mesh
{
    char* name;
    char* resource_name;
    mesh_state state;
    identifier id;
    u8 generation;
    u16 geometry_count;
    struct geometry_config* g_configs;
    struct geometry** geometries;
    extents_3d extents;
    void* debug_data;
} mesh;

typedef enum material_type
{
    MATERIAL_TYPE_UNKNOWN = 0,
    MATERIAL_TYPE_STANDARD,
    MATERIAL_TYPE_WATER,
    MATERIAL_TYPE_BLENDED,
    MATERIAL_TYPE_COUNT,
    MATERIAL_TYPE_CUSTOM = 99
} material_type;

typedef enum material_model
{
    MATERIAL_MODEL_UNLIT = 0,
    MATERIAL_MODEL_PBR,
    MATERIAL_MODEL_PHONG,
    MATERIAL_MODEL_COUNT,
    MATERIAL_MODEL_CUSTOM = 99
} material_model;

typedef struct material_config_prop
{
    char* name;
    shader_uniform_type type;
    u32 size;
    // TODO: Seems like a colossal waste of memory...
    vec4 value_v4;
    vec3 value_v3;
    vec2 value_v2;
    f32 value_f32;
    u32 value_u32;
    u16 value_u16;
    u8 value_u8;
    i32 value_i32;
    i16 value_i16;
    i8 value_i8;
    mat4 value_mat4;
} material_config_prop;

typedef struct material_map
{
    char* name;
    char* texture_name;
    texture_filter filter_min;
    texture_filter filter_mag;
    texture_repeat repeat_u;
    texture_repeat repeat_v;
    texture_repeat repeat_w;
} material_map;

typedef struct material_config
{
    u8 version;
    char* name;
    material_type type;
    material_model model;
    char* shader_name;
    // darray
    material_config_prop* properties;
    // darray
    material_map* maps;
    // Indicates if material should be automatically released when no references to it remain
    b8 auto_release;
} material_config;

typedef struct material
{
    u32 id;
    u32 generation;
    u32 internal_id;
    bname name;
    /** @brief The name of the package containing this material */
    bname package_name;

    struct bresource_texture_map* maps;

    u32 property_struct_size;

    /** @brief array of material property structures, which varies based on material type. e.g. material_phong_properties */
    void* properties;

    /** @brief Explicitly-set irradiance texture for this material */
    bresource_texture* irradiance_texture;

    u32 shader_id;
} material;

typedef enum scene_node_attachment_type
{
    SCENE_NODE_ATTACHMENT_TYPE_UNKNOWN,
    SCENE_NODE_ATTACHMENT_TYPE_STATIC_MESH,
    SCENE_NODE_ATTACHMENT_TYPE_TERRAIN,
    SCENE_NODE_ATTACHMENT_TYPE_SKYBOX,
    SCENE_NODE_ATTACHMENT_TYPE_DIRECTIONAL_LIGHT,
    SCENE_NODE_ATTACHMENT_TYPE_POINT_LIGHT,
    SCENE_NODE_ATTACHMENT_TYPE_WATER_PLANE
} scene_node_attachment_type;

// Static mesh attachment
typedef struct scene_node_attachment_static_mesh
{
    char* resource_name;
} scene_node_attachment_static_mesh;

// Terrain attachment
typedef struct scene_node_attachment_terrain
{
    char* name;
    char* resource_name;
} scene_node_attachment_terrain;

// Skybox attachment
typedef struct scene_node_attachment_skybox
{
    char* cubemap_name;
} scene_node_attachment_skybox;

// Directional light attachment
typedef struct scene_node_attachment_directional_light
{
    vec4 color;
    vec4 direction;
    f32 shadow_distance;
    f32 shadow_fade_distance;
    f32 shadow_split_mult;
} scene_node_attachment_directional_light;

typedef struct scene_node_attachment_point_light
{
    vec4 color;
    vec4 position;
    f32 constant_f;
    f32 linear;
    f32 quadratic;
} scene_node_attachment_point_light;

// Skybox attachment
typedef struct scene_node_attachment_water_plane
{
    u32 reserved;
} scene_node_attachment_water_plane;

typedef struct scene_node_attachment_config
{
    scene_node_attachment_type type;
    void* attachment_data;
} scene_node_attachment_config;

typedef struct scene_xform_config
{
    vec3 position;
    quat rotation;
    vec3 scale;
} scene_xform_config;

typedef struct scene_node_config
{
    char* name;

    // Pointer to a config if one exists, otherwise 0
    scene_xform_config* xform;
    // darray
    scene_node_attachment_config* attachments;
    // darray
    struct scene_node_config* children;
} scene_node_config;

typedef struct scene_config
{
    u32 version;
    char* name;
    char* description;
    char* resource_name;
    char* resource_full_path;

    // darray
    scene_node_config* nodes;
} scene_config;
