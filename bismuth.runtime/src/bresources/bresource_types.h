#pragma once

#include <containers/array.h>
#include <math/math_types.h>
#include <strings/bname.h>

#include "assets/basset_types.h"
#include "core_render_types.h"
#include "identifiers/bhandle.h"
#include "math/geometry.h"

/** @brief Pre-defined resource types */
typedef enum bresource_type
{
    /** @brief Unassigned resource type */
    BRESOURCE_TYPE_UNKNOWN,
    /** @brief Plain text resource type */
    BRESOURCE_TYPE_TEXT,
    /** @brief Plain binary resource type */
    BRESOURCE_TYPE_BINARY,
    /** @brief Texture resource type */
    BRESOURCE_TYPE_TEXTURE,
    /** @brief Material resource type */
    BRESOURCE_TYPE_MATERIAL,
    /** @brief Shader resource type */
    BRESOURCE_TYPE_SHADER,
    /** @brief Static Mesh resource type (collection of geometries) */
    BRESOURCE_TYPE_STATIC_MESH,
    /** @brief Skeletal Mesh resource type (collection of geometries) */
    BRESOURCE_TYPE_SKELETAL_MESH,
    /** @brief Bitmap font resource type */
    BRESOURCE_TYPE_BITMAP_FONT,
    /** @brief System font resource type */
    BRESOURCE_TYPE_SYSTEM_FONT,
    /** @brief Scene resource type */
    BRESOURCE_TYPE_SCENE,
    /** @brief Heightmap-based terrain resource type */
    BRESOURCE_TYPE_HEIGHTMAP_TERRAIN,
    /** @brief Voxel-based terrain resource type */
    BRESOURCE_TYPE_VOXEL_TERRAIN,
    /** @brief Audio resource type, used for both sound effects and music */
    BRESOURCE_TYPE_AUDIO,
    BRESOURCE_TYPE_COUNT,
    // Anything beyond 128 is user-defined types
    BRESOURCE_KNOWN_TYPE_MAX = 128
} bresource_type;

/** @brief Indicates where a resource is in its lifecycle */
typedef enum bresource_state
{
    /**
     * @brief No load operations have happened whatsoever for the resource
     * The resource is NOT in a drawable state.
     */
    BRESOURCE_STATE_UNINITIALIZED,
    /**
     * @brief The CPU-side of the resources have been loaded, but no GPU uploads have happened
     * The resource is NOT in a drawable state.
     */
    BRESOURCE_STATE_INITIALIZED,
    /**
     * @brief The GPU-side of the resources are in the process of being uploaded, but the upload is not yet complete.
     * The resource is NOT in a drawable state.
     */
    BRESOURCE_STATE_LOADING,
    /**
     * @brief The GPU-side of the resources are finished with the process of being uploaded
     * The resource IS in a drawable state.
     */
    BRESOURCE_STATE_LOADED
} bresource_state;

typedef struct bresource
{
    bname name;
    bresource_type type;
    bresource_state state;
    u32 generation;

    /** @brief The number of tags */
    u32 tag_count;

    /** @brief An array of tags */
    bname* tags;
} bresource;

typedef struct bresource_asset_info
{
    bname asset_name;
    bname package_name;
    basset_type type;
    b8 watch_for_hot_reload;
} bresource_asset_info;

ARRAY_TYPE(bresource_asset_info);

typedef void (*PFN_resource_loaded_user_callback)(bresource* resource, void* listener);

typedef struct bresource_request_info
{
    bresource_type type;
    // The list of assets to be loaded
    array_bresource_asset_info assets;
    // The callback made whenever all listed assets are loaded
    PFN_resource_loaded_user_callback user_callback;
    // Listener user data
    void* listener_inst;
    // Force the request to be synchronous, returning a loaded and ready resource immediately
    // NOTE: This should be used sparingly, as it is a blocking operation
    b8 synchronous;
} bresource_request_info;

/** @brief Represents various types of textures */
typedef enum texture_type
{
    /** @brief A standard two-dimensional texture */
    TEXTURE_TYPE_2D,
    /** @brief A 2d array texture */
    TEXTURE_TYPE_2D_ARRAY,
    /** @brief A cube texture, used for cubemaps */
    TEXTURE_TYPE_CUBE,
    /** @brief A cube array texture, used for arrays of cubemaps */
    TEXTURE_TYPE_CUBE_ARRAY,
    TEXTURE_TYPE_COUNT
} texture_type;

typedef enum texture_format
{
    TEXTURE_FORMAT_UNKNOWN,
    TEXTURE_FORMAT_RGBA8,
    TEXTURE_FORMAT_RGB8,
} texture_format;

typedef enum texture_flag
{
    /** @brief Indicates if the texture has transparency */
    TEXTURE_FLAG_HAS_TRANSPARENCY = 0x01,
    /** @brief Indicates if the texture can be written (rendered) to */
    TEXTURE_FLAG_IS_WRITEABLE = 0x02,
    /** @brief Indicates if the texture was created via wrapping vs traditional creation */
    TEXTURE_FLAG_IS_WRAPPED = 0x04,
    /** @brief Indicates the texture is a depth texture */
    TEXTURE_FLAG_DEPTH = 0x08,
    /** @brief Indicates the texture is a stencil texture */
    TEXTURE_FLAG_STENCIL = 0x10,
    /** @brief Indicates that this texture should account for renderer buffering (i.e. double/triple buffering) */
    TEXTURE_FLAG_RENDERER_BUFFERING = 0x20,
} texture_flag;

/** @brief Holds bit flags for textures */
typedef u8 texture_flag_bits;

#define BRESOURCE_TYPE_NAME_TEXTURE "Texture"

typedef struct bresource_texture
{
    bresource base;
    /** @brief The texture type */
    texture_type type;
    /** @brief The texture width */
    u32 width;
    /** @brief The texture height */
    u32 height;
    /** @brief The format of the texture data */
    texture_format format;
    /** @brief For arrayed textures, how many "layers" there are. Otherwise this is 1 */
    u16 array_size;
    /** @brief Holds various flags for this texture */
    texture_flag_bits flags;
    /** @brief The number of mip maps the internal texture has. Must always be at least 1 */
    u8 mip_levels;
    /** @brief The the handle to renderer-specific texture data */
    bhandle renderer_texture_handle;
} bresource_texture;

typedef struct bresource_texture_pixel_data
{
    u8* pixels;
    u32 pixel_array_size;
    u32 width;
    u32 height;
    u32 channel_count;
    texture_format format;
    u8 mip_levels;
} bresource_texture_pixel_data;

ARRAY_TYPE(bresource_texture_pixel_data);

typedef struct bresource_texture_request_info
{
    bresource_request_info base;

    texture_type texture_type;
    u8 array_size;
    texture_flag_bits flags;

    // Optionally provide pixel data per layer. Must match array_size in length.
    // Only used where asset at index has type of undefined
    array_bresource_texture_pixel_data pixel_data;

    // Texture width in pixels. Ignored unless there are no assets or pixel data
    u32 width;
    // Texture height in pixels. Ignored unless there are no assets or pixel data
    u32 height;

    // Texture format. Ignored unless there are no assets or pixel data
    texture_format format;

    // The number of mip levels. Ignored unless there are no assets or pixel data
    u8 mip_levels;

    // Indicates if loaded image assets should be flipped on the y-axis when loaded. Ignored for non-asset-based textures
    b8 flip_y;
} bresource_texture_request_info;

/** @brief A shader resource */
typedef struct bresource_shader
{
    bresource base;

    /** @brief The face cull mode to be used. Default is BACK if not supplied */
    face_cull_mode cull_mode;

    /** @brief The topology types for the shader pipeline. See primitive_topology_type. Defaults to "triangle list" if unspecified */
    primitive_topology_types topology_types;

    /** @brief The count of attributes */
    u8 attribute_count;
    /** @brief The collection of attributes */
    shader_attribute_config* attributes;

    /** @brief The count of uniforms */
    u8 uniform_count;
    /** @brief The collection of uniforms */
    shader_uniform_config* uniforms;

    /** @brief The number of stages present in the shader */
    u8 stage_count;
    /** @brief The collection of stage configs */
    shader_stage_config* stage_configs;

    /** @brief The maximum number of groups allowed */
    u32 max_groups;

    /** @brief The maximum number of per-draw instances allowed */
    u32 max_per_draw_count;

    /** @brief The flags set for this shader */
    shader_flags flags;
} bresource_shader;

/** @brief Used to request a shader resource */
typedef struct bresource_shader_request_info
{
    bresource_request_info base;
    // Optionally include shader config source text to be used as if it resided in a .bsc file
    const char* shader_config_source_text;
} bresource_shader_request_info;

/**
 * @brief A bresource_material is a configuration of a material to hand off to the material system.
 * Once a material is loaded, this can just be released
 */
typedef struct bresource_material
{
    bresource base;
    
    bmaterial_type type;
    // Shading model
    bmaterial_model model;

    b8 has_transparency;
    b8 double_sided;
    b8 recieves_shadow;
    b8 casts_shadow;
    b8 use_vertex_color_as_base_color;

    // The asset name for a custom shader. Optional
    bname custom_shader_name;

    vec4 base_color;
    bmaterial_texture_input base_color_map;

    b8 normal_enabled;
    vec3 normal;
    bmaterial_texture_input normal_map;

    f32 metallic;
    bmaterial_texture_input metallic_map;
    texture_channel metallic_map_source_channel;

    f32 roughness;
    bmaterial_texture_input roughness_map;
    texture_channel roughness_map_source_channel;

    b8 ambient_occlusion_enabled;
    f32 ambient_occlusion;
    bmaterial_texture_input ambient_occlusion_map;
    texture_channel ambient_occlusion_map_source_channel;

    // Combined metallic/roughness/ao value
    vec3 mra;
    bmaterial_texture_input mra_map;
    // Indicates if the mra combined value/map should be used instead of the separate ones
    b8 use_mra;

    b8 emissive_enabled;
    vec4 emissive;
    bmaterial_texture_input emissive_map;

    // Derivative (dudv) map. Only used for water materials
    bmaterial_texture_input dudv_map;

    f32 tiling;
    f32 wave_strength;
    f32 wave_speed;
    
    u32 custom_sampler_count;
    bmaterial_sampler_config* custom_samplers;
} bresource_material;

/** @brief Used to request a material resource */
typedef struct bresource_material_request_info
{
    bresource_request_info base;
    // Optionally include source text to be used as if it resided in a .bmt file
    const char* material_source_text;
} bresource_material_request_info;

/*
 * ==================================================
 *                  Static mesh
 * ==================================================
 */

/** Represents a single static mesh, which contains geometry */
typedef struct static_mesh_submesh
{
    /** @brief The geometry data for this mesh */
    bgeometry geometry;
    /** @brief The name of the material associated with this mesh */
    bname material_name;
} static_mesh_submesh;

/** @brief A mesh resource that is static in nature (i.e. it does not change over time) */
typedef struct bresource_static_mesh
{
    bresource base;

    /** @brief The number of submeshes in this static mesh resource */
    u16 submesh_count;
    /** @brief The array of submeshes in this static mesh resource */
    static_mesh_submesh* submeshes;
} bresource_static_mesh;

typedef struct bresource_static_mesh_request_info
{
    bresource_request_info base;
} bresource_static_mesh_request_info;

#define BRESOURCE_TYPE_NAME_TEXT "Text"

typedef struct bresource_text
{
    bresource base;
    
    const char* text;

    u32 asset_file_watch_id;
} bresource_text;

#define BRESOURCE_TYPE_NAME_BINARY "Binary"

typedef struct bresource_binary
{
    bresource base;

    u32 size;
    void* bytes;
} bresource_binary;

#define BRESOURCE_TYPE_NAME_FONT "Font"

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

typedef struct font_page
{
    bname image_asset_name;
} font_page;

ARRAY_TYPE(font_glyph);
ARRAY_TYPE(font_kerning);
ARRAY_TYPE(font_page);

/** @brief Represents a bitmap font resource */
typedef struct bresource_bitmap_font
{
    bresource base;

    bname face;
    // The font size
    u32 size;
    i32 line_height;
    i32 baseline;
    u32 atlas_size_x;
    u32 atlas_size_y;

    array_font_glyph glyphs;
    array_font_kerning kernings;
    array_font_page pages;
} bresource_bitmap_font;

typedef struct bresource_bitmap_font_request_info
{
    bresource_request_info base;
} bresource_bitmap_font_request_info;

/** @brief Represents a system font resource */
typedef struct bresource_system_font
{
    bresource base;
    bname ttf_asset_name;
    bname ttf_asset_package_name;
    u32 face_count;
    bname* faces;
    u32 font_binary_size;
    void* font_binary;
} bresource_system_font;

typedef struct bresource_system_font_request_info
{
    bresource_request_info base;
} bresource_system_font_request_info;

typedef struct bresource_scene
{
    bresource base;
    const char* description;
    u32 node_count;
    scene_node_config* nodes;
    b8 physics_enabled;
    vec3 physics_gravity;
} bresource_scene;

typedef struct bresource_scene_request_info
{
    bresource_request_info base;
} bresource_scene_request_info;

/** @brief Represents a heightmap terrain resource */
typedef struct bresource_heightmap_terrain
{
    bresource base;
    bname heightmap_asset_name;
    bname heightmap_asset_package_name;
    u16 chunk_size;
    vec3 tile_scale;
    u8 material_count;
    bname* material_names;
} bresource_heightmap_terrain;

typedef struct bresource_heightmap_terrain_request_info
{
    bresource_request_info base;
} bresource_heightmap_terrain_request_info;

/** @brief Represents a Bismuth Audio resource */
typedef struct bresource_audio
{
    bresource base;
    // The number of channels (i.e. 1 for mono or 2 for stereo)
    i32 channels;
    // The sample rate of the sound/music (i.e. 44100)
    u32 sample_rate;

    // Total samples in the audio resource
    u32 total_sample_count;

    // The size of the pcm data
    u64 pcm_data_size;
    // Pulse-code modulation buffer, or raw data to be fed into a buffer
    i16* pcm_data;
    // The size of the downmixed pcm data if the asset was stereo, or 0 if the asset was already mono (use ) or just a pointer to pcm data if the asset was already mono (use pcm_data_size instead)
    u64 downmixed_size;
    // Downmixed pcm data if the asset was stereo, or just a pointer to pcm data if the asset was already mono
    i16* mono_pcm_data;
    
    // A handle to the audio internal resource
    bhandle internal_resource;
} bresource_audio;

typedef struct bresource_audio_request_info
{
    bresource_request_info base;
} bresource_audio_request_info;
