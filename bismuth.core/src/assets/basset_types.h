#pragma once

#include "containers/array.h"
#include "core_render_types.h"
#include "defines.h"
#include "identifiers/identifier.h"
#include "math/math_types.h"
#include "parsers/bson_parser.h"
#include "strings/bname.h"

/** @brief A magic number indicating the file as a bismuth binary asset file */
#define ASSET_MAGIC 0xcafebabe
#define ASSET_MAGIC_U64 0xcafebabebadc0ffee

// The maximum length of the string representation of an asset type
#define BASSET_TYPE_MAX_LENGTH 64
// The maximum name of an asset
#define BASSET_NAME_MAX_LENGTH 256
// The maximum name length for a bpackage
#define BPACKAGE_NAME_MAX_LENGTH 128

// The maximum length of a fully-qualified asset name, including the '.' between parts
#define BASSET_FULLY_QUALIFIED_NAME_MAX_LENGTH = (BPACKAGE_NAME_MAX_LENGTH + BASSET_TYPE_MAX_LENGTH + BASSET_NAME_MAX_LENGTH + 2)

typedef enum basset_type
{
    BASSET_TYPE_UNKNOWN,
    /** An image, typically (but not always) used as a texture */
    BASSET_TYPE_IMAGE,
    BASSET_TYPE_MATERIAL,
    BASSET_TYPE_STATIC_MESH,
    BASSET_TYPE_HEIGHTMAP_TERRAIN,
    BASSET_TYPE_SCENE,
    BASSET_TYPE_BITMAP_FONT,
    BASSET_TYPE_SYSTEM_FONT,
    BASSET_TYPE_TEXT,
    BASSET_TYPE_BINARY,
    BASSET_TYPE_BSON,
    BASSET_TYPE_VOXEL_TERRAIN,
    BASSET_TYPE_SKELETAL_MESH,
    BASSET_TYPE_AUDIO,
    BASSET_TYPE_MUSIC,
    BASSET_TYPE_SHADER,

    BASSET_TYPE_MAX
} basset_type;

typedef struct binary_asset_header
{
    // A magic number used to identify the binary block as a Bismuth asset
    u32 magic;
    // Indicates the asset type. Cast to basset_type
    u32 type;
    // The asset type version, used for feature support checking for asset versions
    u32 version;
    // The size of the data region of  the asset in bytes
    u32 data_block_size;
} binary_asset_header;

struct basset;
struct basset_importer;

typedef enum asset_request_result
{
    /** The asset load was a success, including any GPU operations (if required) */
    ASSET_REQUEST_RESULT_SUCCESS,
    /** The specified package name was invalid or not found */
    ASSET_REQUEST_RESULT_INVALID_PACKAGE,
    /** The specified asset type was invalid or not found */
    ASSET_REQUEST_RESULT_INVALID_ASSET_TYPE,
    /** The specified asset name was invalid or not found */
    ASSET_REQUEST_RESULT_INVALID_NAME,
    /** The asset was found, but failed to load during the parsing stage */
    ASSET_REQUEST_RESULT_PARSE_FAILED,
    /** The asset was found, but failed to load during the GPU upload stage */
    ASSET_REQUEST_RESULT_GPU_UPLOAD_FAILED,
    /** An internal system failure has occurred. See logs for details */
    ASSET_REQUEST_RESULT_INTERNAL_FAILURE,
    /** No handler exists for the given asset. See logs for details */
    ASSET_REQUEST_RESULT_NO_HANDLER,
    /** There was a failure at the VFS level, probably a request for an asset that doesn't exist */
    ASSET_REQUEST_RESULT_VFS_REQUEST_FAILED,
    /** Returned by handlers who attempt (and fail) an auto-import of source asset data when the binary does not exist */
    ASSET_REQUEST_RESULT_AUTO_IMPORT_FAILED,
    /** The total number of result options in this enumeration. Not an actual result value */
    ASSET_REQUEST_RESULT_COUNT
} asset_request_result;

typedef void (*PFN_basset_on_result)(asset_request_result result, const struct basset* asset, void* listener_inst);
typedef b8 (*PFN_basset_importer_import)(const struct basset_importer* self, u64 data_size, const void* data, void* params, struct basset* out_asset);

/** @brief Represents the interface point for an importer */
typedef struct basset_importer
{
    /** @brief The file type supported by the importer */
    const char* source_type;
    /**
     * @brief Imports an asset according to the provided params and the importer's internal logic.
     * NOTE: Some importers (i.e. .obj for static meshes) can also trigger imports of other assets. Those assets are immediately serialized to disk/package and not returned here though
     *
     * @param self A pointer to the importer itself
     * @param data_size The size of the data being imported
     * @param data A block of memory containing the data being imported
     * @param params A block of memory containing parameters for the import. Optional in general, but required by some importers
     * @param out_asset A pointer to the asset being imported
     * @returns True on success; otherwise false
     */
    PFN_basset_importer_import import;
} basset_importer;

/** @brief Various metadata included with the asset */
typedef struct basset_metadata
{
    // The asset version
    u32 version;
    /** @brief The path of the originally imported file used to create this asset, stored as a bname */
    bname source_asset_path;

    /** @brief The number of tags */
    u32 tag_count;
    /** @brief An array of tags */
    bname* tags;
    // TODO: Listing of asset-type-specific metadata
} basset_metadata;

/** @brief A structure meant to be included as the first member in the struct of all asset types for quick casting purposes */
typedef struct basset
{
    /** @brief A system-wide unique identifier for the asset */
    identifier id;
    /** @brief Increments every time the asset is loaded/reloaded. Otherwise INVALID_ID */
    u32 generation;
    // Size of the asset
    u64 size;
    // Asset name stored as a bname
    bname name;
    // Package name stored as a bname
    bname package_name;
    /** @brief The asset type */
    basset_type type;
    /** @brief Metadata for the asset */
    basset_metadata meta;
} basset;

#define BASSET_TYPE_NAME_HEIGHTMAP_TERRAIN "HeightmapTerrain"

typedef struct basset_heightmap_terrain
{
    basset base;
    bname heightmap_asset_name;
    u16 chunk_size;
    vec3 tile_scale;
    u8 material_count;
    bname* material_names;
} basset_heightmap_terrain;

typedef enum basset_image_format
{
    BASSET_IMAGE_FORMAT_UNDEFINED = 0,
    // 4 channel, 8 bits per channel
    BASSET_IMAGE_FORMAT_RGBA8 = 1
    // TODO: additional formats
} basset_image_format;

/** @brief Import options for images */
typedef struct basset_image_import_options
{
    /** @brief Indicates if the image should be flipped on the y-axis when imported */
    b8 flip_y;
    /** @brief The expected format of the image */
    basset_image_format format;
} basset_image_import_options;

#define BASSET_TYPE_NAME_IMAGE "Image"

typedef struct basset_image
{
    basset base;
    u32 width;
    u32 height;
    u8 channel_count;
    u8 mip_levels;
    basset_image_format format;
    u64 pixel_array_size;
    u8* pixels;
} basset_image;

#define BASSET_TYPE_NAME_STATIC_MESH "StaticMesh"

typedef struct basset_static_mesh_geometry
{
    bname name;
    bname material_asset_name;
    u32 vertex_count;
    vertex_3d* vertices;
    u32 index_count;
    u32* indices;
    extents_3d extents;
    vec3 center;
} basset_static_mesh_geometry;

/** @brief Represents a static mesh asset */
typedef struct basset_static_mesh
{
    basset base;
    u16 geometry_count;
    basset_static_mesh_geometry* geometries;
    extents_3d extents;
    vec3 center;
} basset_static_mesh;

#define BASSET_TYPE_NAME_MATERIAL "Material"

typedef enum bmaterial_type
{
    BMATERIAL_TYPE_UNKNOWN = 0,
    BMATERIAL_TYPE_PBR,
    BMATERIAL_TYPE_PBR_TERRAIN,
    BMATERIAL_TYPE_PBR_WATER,
    BMATERIAL_TYPE_UNLIT,
    BMATERIAL_TYPE_PHONG,
    BMATERIAL_TYPE_COUNT,
    BMATERIAL_TYPE_CUSTOM = 99
} bmaterial_type;

typedef enum basset_material_map_channel
{
    BASSET_MATERIAL_MAP_CHANNEL_ALBEDO,
    BASSET_MATERIAL_MAP_CHANNEL_NORMAL,
    BASSET_MATERIAL_MAP_CHANNEL_METALLIC,
    BASSET_MATERIAL_MAP_CHANNEL_ROUGHNESS,
    BASSET_MATERIAL_MAP_CHANNEL_AO,
    BASSET_MATERIAL_MAP_CHANNEL_EMISSIVE,
    BASSET_MATERIAL_MAP_CHANNEL_CLEAR_COAT,
    BASSET_MATERIAL_MAP_CHANNEL_CLEAR_COAT_ROUGHNESS,
    BASSET_MATERIAL_MAP_CHANNEL_WATER_DUDV,
    BASSET_MATERIAL_MAP_CHANNEL_DIFFUSE,
    BASSET_MATERIAL_MAP_CHANNEL_SPECULAR,
} basset_material_map_channel;

typedef struct basset_material_map
{
    // Material map name
    bname name;
    // Image asset name
    bname image_asset_name;
    // Name of the package containing the image asset
    bname image_asset_package_name;
    basset_material_map_channel channel;
    texture_filter filter_min;
    texture_filter filter_mag;
    texture_repeat repeat_u;
    texture_repeat repeat_v;
    texture_repeat repeat_w;
} basset_material_map;

typedef struct basset_material_property
{
    bname name;
    shader_uniform_type type;
    u32 size;
    union
    {
        vec4 v4;
        vec3 v3;
        vec2 v2;
        f32 f32;
        u32 u32;
        u16 u16;
        u8 u8;
        i32 i32;
        i16 i16;
        i8 i8;
        mat4 mat4;
    } value;
} basset_material_property;

typedef struct basset_material
{
    basset base;
    bmaterial_type type;
    // The asset name for a custom shader. Optional
    char* custom_shader_name;

    u32 map_count;
    basset_material_map* maps;

    u32 property_count;
    basset_material_property* properties;
} basset_material;

#define BASSET_TYPE_NAME_TEXT "Text"

typedef struct basset_text
{
    basset base;
    const char* content;
} basset_text;

#define BASSET_TYPE_NAME_BINARY "Binary"

typedef struct basset_binary
{
    basset base;
    u64 size;
    const void* content;
} basset_binary;

#define BASSET_TYPE_NAME_BSON "Bson"

typedef struct basset_bson
{
    basset base;
    const char* source_text;
    bson_tree tree;
} basset_bson;

#define BASSET_TYPE_NAME_SCENE "Scene"

typedef enum basset_scene_node_attachment_type
{
    BASSET_SCENE_NODE_ATTACHMENT_TYPE_SKYBOX,
    BASSET_SCENE_NODE_ATTACHMENT_TYPE_DIRECTIONAL_LIGHT,
    BASSET_SCENE_NODE_ATTACHMENT_TYPE_POINT_LIGHT,
    BASSET_SCENE_NODE_ATTACHMENT_TYPE_STATIC_MESH,
    BASSET_SCENE_NODE_ATTACHMENT_TYPE_HEIGHTMAP_TERRAIN,
    BASSET_SCENE_NODE_ATTACHMENT_TYPE_WATER_PLANE,
    BASSET_SCENE_NODE_ATTACHMENT_TYPE_COUNT
} basset_scene_node_attachment_type;

static const char* basset_scene_node_attachment_type_strings[BASSET_SCENE_NODE_ATTACHMENT_TYPE_COUNT] = {
    "Skybox",           // BASSET_SCENE_NODE_ATTACHMENT_TYPE_SKYBOX,
    "DirectionalLight", // BASSET_SCENE_NODE_ATTACHMENT_TYPE_DIRECTIONAL_LIGHT,
    "PointLight",       // BASSET_SCENE_NODE_ATTACHMENT_TYPE_POINT_LIGHT,
    "StaticMesh",       // BASSET_SCENE_NODE_ATTACHMENT_TYPE_STATIC_MESH,
    "HeightmapTerrain", // BASSET_SCENE_NODE_ATTACHMENT_TYPE_STATIC_HEIGHTMAP_TERRAIN,
    "WaterPlane"        // BASSET_SCENE_NODE_ATTACHMENT_TYPE_WATER_PLANE,
};

// Ensure changes to scene attachment types break this if it isn't also updated
STATIC_ASSERT(BASSET_SCENE_NODE_ATTACHMENT_TYPE_COUNT == (sizeof(basset_scene_node_attachment_type_strings) / sizeof(*basset_scene_node_attachment_type_strings)), "Scene attachment type count does not match string lookup table count");

//////////////////////

typedef struct basset_scene_node_attachment
{
    basset_scene_node_attachment_type type;
    const char* name;
} basset_scene_node_attachment;

typedef struct basset_scene_node_attachment_skybox
{
    basset_scene_node_attachment base;
    const char* cubemap_image_asset_name;
} basset_scene_node_attachment_skybox;

typedef struct basset_scene_node_attachment_directional_light
{
    basset_scene_node_attachment base;
    vec4 color;
    vec4 direction;
    f32 shadow_distance;
    f32 shadow_fade_distance;
    f32 shadow_split_mult;
} basset_scene_node_attachment_directional_light;

typedef struct basset_scene_node_attachment_point_light
{
    basset_scene_node_attachment base;
    vec4 color;
    vec4 position;
    f32 constant_f;
    f32 linear;
    f32 quadratic;
} basset_scene_node_attachment_point_light;

typedef struct basset_scene_node_attachment_static_mesh
{
    basset_scene_node_attachment base;
    const char* asset_name;
} basset_scene_node_attachment_static_mesh;

typedef struct basset_scene_node_attachment_heightmap_terrain
{
    basset_scene_node_attachment base;
    const char* asset_name;
} basset_scene_node_attachment_heightmap_terrain;

typedef struct basset_scene_node_attachment_water_plane
{
    basset_scene_node_attachment base;
    // TODO: expand configurable properties
} basset_scene_node_attachment_water_plane;

typedef struct basset_scene_node
{
    const char* name;
    u32 attachment_count;
    basset_scene_node_attachment* attachments;
    u32 child_count;
    struct basset_scene_node* children;
    // String representation of xform, processed by the scene when needed
    const char* xform_source;
} basset_scene_node;

typedef struct basset_scene
{
    basset base;
    const char* description;
    u32 node_count;
    basset_scene_node* nodes;
} basset_scene;

#define BASSET_TYPE_NAME_SHADER "Shader"

typedef struct basset_shader_stage
{
    shader_stage type;
    const char* source_asset_name;
    const char* package_name;
} basset_shader_stage;

typedef struct basset_shader_attribute
{
    const char* name;
    shader_attribute_type type;
} basset_shader_attribute;

typedef struct basset_shader_uniform
{
    const char* name;
    shader_uniform_type type;
    shader_scope scope;
} basset_shader_uniform;

typedef struct basset_shader
{
    basset base;
    u32 stage_count;
    basset_shader_stage* stages;
    b8 depth_test;
    b8 depth_write;
    b8 stencil_test;
    b8 stencil_write;
    u16 max_instances;

    u32 attribute_count;
    basset_shader_attribute* attributes;

    u32 uniform_count;
    basset_shader_uniform* uniforms;
} basset_shader;

#define BASSET_TYPE_NAME_SYSTEM_FONT "SystemFont"

typedef struct basset_system_font_face
{
    bname name;
} basset_system_font_face;

typedef struct basset_system_font
{
    basset base;
    bname ttf_asset_name;
    bname ttf_asset_package_name;
    u32 face_count;
    basset_system_font_face* faces;
    u32 font_binary_size;
    void* font_binary;
} basset_system_font;

#define BASSET_TYPE_NAME_BITMAP_FONT "BitmapFont"

typedef struct basset_bitmap_font_glyph
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
} basset_bitmap_font_glyph;

typedef struct basset_bitmap_font_kerning
{
    i32 codepoint_0;
    i32 codepoint_1;
    i16 amount;
} basset_bitmap_font_kerning;

typedef struct basset_bitmap_font_page
{
    i8 id;
    bname image_asset_name;
} basset_bitmap_font_page;

ARRAY_TYPE(basset_bitmap_font_glyph);
ARRAY_TYPE(basset_bitmap_font_kerning);
ARRAY_TYPE(basset_bitmap_font_page);

typedef struct basset_bitmap_font
{
    basset base;
    bname face;
    u32 size;
    i32 line_height;
    i32 baseline;
    i32 atlas_size_x;
    i32 atlas_size_y;
    basset_bitmap_font_glyph_array glyphs;
    basset_bitmap_font_kerning_array kernings;
    f32 tab_x_advance;
    basset_bitmap_font_page_array pages;
} basset_bitmap_font;
