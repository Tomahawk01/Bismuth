#pragma once

#include "core/identifier.h"
#include "math/math_types.h"

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
    RESOURCE_TYPE_SIMPLE_SCENE,
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

typedef enum face_cull_mode
{
    FACE_CULL_MODE_NONE = 0x0,
    FACE_CULL_MODE_FRONT = 0x1,
    FACE_CULL_MODE_BACK = 0x2,
    FACE_CULL_MODE_FRONT_AND_BACK = 0x3
} face_cull_mode;

typedef enum primitive_topology_type
{
    PRIMITIVE_TOPOLOGY_TYPE_NONE = 0x00,
    PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE_LIST = 0x01, // Default if nothing is defined
    PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE_STRIP = 0x02,
    PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE_FAN = 0x04,
    PRIMITIVE_TOPOLOGY_TYPE_LINE_LIST = 0x08,
    PRIMITIVE_TOPOLOGY_TYPE_LINE_STRIP = 0x10,
    PRIMITIVE_TOPOLOGY_TYPE_POINT_LIST = 0x20,
    PRIMITIVE_TOPOLOGY_TYPE_MAX = PRIMITIVE_TOPOLOGY_TYPE_POINT_LIST << 1
} primitive_topology_type;

typedef enum texture_flag
{
    TEXTURE_FLAG_HAS_TRANSPARENCY = 0x1,
    TEXTURE_FLAG_IS_WRITEABLE = 0x2,
    TEXTURE_FLAG_IS_WRAPPED = 0x4,
    TEXTURE_FLAG_DEPTH = 0x8
} texture_flag;

typedef u8 texture_flag_bits;

typedef enum texture_type
{
    TEXTURE_TYPE_2D,
    TEXTURE_TYPE_CUBE
} texture_type;

#define TEXTURE_NAME_MAX_LENGTH 512
typedef struct texture
{
    u32 id;
    texture_type type;
    u32 width;
    u32 height;
    u8 channel_count;
    texture_flag_bits flags;
    u32 generation;
    char name[TEXTURE_NAME_MAX_LENGTH];
    void* internal_data;
    u32 mip_levels;
} texture;

typedef enum texture_filter
{
    TEXTURE_FILTER_MODE_NEAREST = 0x0,
    TEXTURE_FILTER_MODE_LINEAR = 0x1
} texture_filter;

typedef enum texture_repeat
{
    TEXTURE_REPEAT_REPEAT = 0x1,
    TEXTURE_REPEAT_MIRRORED_REPEAT = 0x2,
    TEXTURE_REPEAT_CLAMP_TO_EDGE = 0x3,
    TEXTURE_REPEAT_CLAMP_TO_BORDER = 0x4
} texture_repeat;

typedef struct texture_map
{
    u32 generation;
    u32 mip_levels;
    texture* texture;
    texture_filter filter_minify;
    texture_filter filter_magnify;
    texture_repeat repeat_u; // X
    texture_repeat repeat_v; // Y
    texture_repeat repeat_w; // Z
    // Identifier used for internal resource lookups/management
    u32 internal_id;
} texture_map;

typedef struct font_glyph
{
    i32 codepoint;
    u16 x;
    u16 y;
    u16 width;
    u16 height;
    i16 x_offset;
    i16 y_offset;
    i16 x_advance;
    u8 page_id;
} font_glyph;

typedef struct font_kerning
{
    i32 codepoint_0;
    i32 codepoint_1;
    i16 amount;
} font_kerning;

typedef enum font_type
{
    FONT_TYPE_BITMAP,
    FONT_TYPE_SYSTEM
} font_type;

typedef struct font_data
{
    font_type type;
    char face[256];
    u32 size;
    i32 line_height;
    i32 baseline;
    i32 atlas_size_x;
    i32 atlas_size_y;
    texture_map atlas;
    u32 glyph_count;
    font_glyph* glyphs;
    u32 kerning_count;
    font_kerning* kernings;
    f32 tab_x_advance;
    u32 internal_data_size;
    void* internal_data;
} font_data;

typedef struct bitmap_font_page
{
    i8 id;
    char file[256];
} bitmap_font_page;

typedef struct bitmap_font_resource_data
{
    font_data data;
    u32 page_count;
    bitmap_font_page* pages;
} bitmap_font_resource_data;

typedef struct system_font_face
{
    char name[256];
} system_font_face;

typedef struct system_font_resource_data
{
    // darray
    system_font_face* fonts;
    u64 binary_size;
    void* font_binary;
} system_font_resource_data;

#define MATERIAL_NAME_MAX_LENGTH 256

struct material;

#define GEOMETRY_NAME_MAX_LENGTH 256
/**
 * @brief Represents actual geometry in the world.
 * Typically (depending on use) paired with a material.
 */
typedef struct geometry
{
    u32 id;
    u16 generation;
    vec3 center;
    extents_3d extents;

    u32 vertex_count;
    u32 vertex_element_size;
    void* vertices;
    u64 vertex_buffer_offset;

    u32 index_count;
    u32 index_element_size;
    void* indices;
    u64 index_buffer_offset;

    char name[GEOMETRY_NAME_MAX_LENGTH];
    struct material* material;
} geometry;

struct geometry_config;
typedef struct mesh_config
{
    char* name;
    char* parent_name;
    char* resource_name;
    u16 geometry_count;
    struct geometry_config* g_configs;
} mesh_config;

typedef struct mesh
{
    char* name;
    mesh_config config;
    identifier id;
    u8 generation;
    u16 geometry_count;
    geometry** geometries;
    transform transform;
    extents_3d extents;
    void *debug_data;
} mesh;

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
    u16 size;
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

    // The face cull mode to be used. NOTE: default is BACK if not supplied
    face_cull_mode cull_mode;

    // Topology types for shader pipeline. Defaults to "triangle list" if unspecified
    u32 topology_types;

    // The count of attributes
    u8 attribute_count;
    // The collection of attributes. Darray
    shader_attribute_config* attributes;

    // The count of uniforms
    u8 uniform_count;
    // The collection of uniforms. Darray
    shader_uniform_config* uniforms;

    // The number of stages present in the shader
    u8 stage_count;
    // The collection of stages. Darray
    shader_stage* stages;
    // The collection of stage names. Must align with stages array. Darray
    char** stage_names;
    // The collection of stage file names to be loaded (one per stage). Must align with stages array. Darray
    char** stage_filenames;

    // Flags set for this shader
    u32 flags;
} shader_config;

typedef enum material_type
{
    // Invalid
    MATERIAL_TYPE_UNKNOWN = 0,
    MATERIAL_TYPE_PHONG = 1,
    MATERIAL_TYPE_PBR = 2,
    MATERIAL_TYPE_UI = 3,
    MATERIAL_TYPE_TERRAIN = 4,
    MATERIAL_TYPE_CUSTOM = 99
} material_type;

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
    char* shader_name;
    // darray
    material_config_prop* properties;
    // darray
    material_map* maps;
    // Indicates if material should be automatically released when no references to it remain
    b8 auto_release;
} material_config;

typedef struct material_phong_properties
{
    vec4 diffuse_color;
    vec3 padding;
    f32 shininess;
} material_phong_properties;

typedef struct material_ui_properties
{
    vec4 diffuse_color;
} material_ui_properties;

typedef struct material_terrain_properties
{
    material_phong_properties materials[4];
    vec3 padding;
    i32 num_materials;
    vec4 padding2;
} material_terrain_properties;

typedef struct material
{
    u32 id;
    material_type type;
    u32 generation;
    u32 internal_id;
    char name[MATERIAL_NAME_MAX_LENGTH];

    texture_map *maps;

    u32 property_struct_size;

    /** @brief array of material property structures, which varies based on material type. e.g. material_phong_properties */
    void* properties;

    u32 shader_id;

    u64 render_frame_number;
    u64 render_draw_index;
} material;

typedef struct skybox_simple_scene_config
{
    char* name;
    char* cubemap_name;
} skybox_simple_scene_config;

typedef struct directional_light_simple_scene_config
{
    char* name;
    vec4 color;
    vec4 direction;
} directional_light_simple_scene_config;

typedef struct point_light_simple_scene_config
{
    char* name;
    vec4 color;
    vec4 position;
    f32 constant_f;
    f32 linear;
    f32 quadratic;
} point_light_simple_scene_config;

typedef struct mesh_simple_scene_config
{
    char* name;
    char* resource_name;
    transform transform;
    char* parent_name;  // optional
} mesh_simple_scene_config;

typedef struct terrain_simple_scene_config
{
    char* name;
    char* resource_name;
    transform xform;
} terrain_simple_scene_config;

typedef struct simple_scene_config
{
    char* name;
    char* description;
    skybox_simple_scene_config skybox_config;
    directional_light_simple_scene_config directional_light_config;

    // darray
    point_light_simple_scene_config* point_lights;

    // darray
    mesh_simple_scene_config* meshes;

    // darray
    terrain_simple_scene_config* terrains;
} simple_scene_config;
