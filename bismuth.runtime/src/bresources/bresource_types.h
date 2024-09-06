#pragma once

#include <containers/array.h>
#include <math/math_types.h>
#include <strings/bname.h>

#include "assets/basset_types.h"
#include "identifiers/bhandle.h"

/** @brief Pre-defined resource types */
typedef enum bresource_type
{
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
    /** @brief Sound effect resource type */
    BRESOURCE_TYPE_SOUND_EFFECT,
    /** @brief Music resource type */
    BRESOURCE_TYPE_MUSIC,
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
} bresource_request_info;

/** @brief Represents various types of textures */
typedef enum bresource_texture_type
{
    /** @brief A standard two-dimensional texture */
    BRESOURCE_TEXTURE_TYPE_2D,
    /** @brief A 2d array texture */
    BRESOURCE_TEXTURE_TYPE_2D_ARRAY,
    /** @brief A cube texture, used for cubemaps */
    BRESOURCE_TEXTURE_TYPE_CUBE,
    /** @brief A cube array texture, used for arrays of cubemaps */
    BRESOURCE_TEXTURE_TYPE_CUBE_ARRAY,
    BRESOURCE_TEXTURE_TYPE_COUNT
} bresource_texture_type;

typedef enum bresource_texture_format
{
    BRESOURCE_TEXTURE_FORMAT_UNKNOWN,
    BRESOURCE_TEXTURE_FORMAT_RGBA8,
    BRESOURCE_TEXTURE_FORMAT_RGB8,
} bresource_texture_format;

typedef enum bresource_texture_flag
{
    /** @brief Indicates if the texture has transparency */
    BRESOURCE_TEXTURE_FLAG_HAS_TRANSPARENCY = 0x01,
    /** @brief Indicates if the texture can be written (rendered) to */
    BRESOURCE_TEXTURE_FLAG_IS_WRITEABLE = 0x02,
    /** @brief Indicates if the texture was created via wrapping vs traditional creation */
    BRESOURCE_TEXTURE_FLAG_IS_WRAPPED = 0x04,
    /** @brief Indicates the texture is a depth texture */
    BRESOURCE_TEXTURE_FLAG_DEPTH = 0x08,
    /** @brief Indicates that this texture should account for renderer buffering (i.e. double/triple buffering) */
    BRESOURCE_TEXTURE_FLAG_RENDERER_BUFFERING = 0x10,
} bresource_texture_flag;

/** @brief Holds bit flags for textures */
typedef u32 bresource_texture_flag_bits;

#define BRESOURCE_TYPE_NAME_TEXTURE "Texture"

typedef struct bresource_texture
{
    bresource base;
    /** @brief The texture type */
    bresource_texture_type type;
    /** @brief The texture width */
    u32 width;
    /** @brief The texture height */
    u32 height;
    /** @brief The format of the texture data */
    bresource_texture_format format;
    /** @brief For arrayed textures, how many "layers" there are. Otherwise this is 1 */
    u16 array_size;
    /** @brief Holds various flags for this texture */
    bresource_texture_flag_bits flags;
    /** @brief The number of mip maps the internal texture has. Must always be at least 1 */
    u8 mip_levels;
    /** @brief The the handle to renderer-specific texture data */
    b_handle renderer_texture_handle;
} bresource_texture;

typedef struct bresource_texture_pixel_data
{
    u8* pixels;
    u32 pixel_array_size;
    u32 width;
    u32 height;
    u32 channel_count;
    bresource_texture_format format;
    u8 mip_levels;
} bresource_texture_pixel_data;

ARRAY_TYPE(bresource_texture_pixel_data);

typedef struct bresource_texture_request_info
{
    bresource_request_info base;

    bresource_texture_type texture_type;
    u8 array_size;
    bresource_texture_flag_bits flags;

    // Optionally provide pixel data per layer. Must match array_size in length.
    // Only used where asset at index has type of undefined
    array_bresource_texture_pixel_data pixel_data;
} bresource_texture_request_info;
