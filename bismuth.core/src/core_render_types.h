#pragma once

#include "defines.h"
#include "strings/bname.h"
#include "strings/bstring_id.h"

/** @brief Determines face culling mode during rendering */
typedef enum face_cull_mode
{
    /** @brief No faces are culled */
    FACE_CULL_MODE_NONE = 0x0,
    /** @brief Only front faces are culled */
    FACE_CULL_MODE_FRONT = 0x1,
    /** @brief Only back faces are culled */
    FACE_CULL_MODE_BACK = 0x2,
    /** @brief Both front and back faces are culled */
    FACE_CULL_MODE_FRONT_AND_BACK = 0x3
} face_cull_mode;

/** @brief Various topology type flag bit fields */
typedef enum primitive_topology_type_bits
{
    /** Topology type not defined. Not valid for shader creation */
    PRIMITIVE_TOPOLOGY_TYPE_NONE_BIT = 0x00,
    /** A list of triangles. The default if nothing is defined */
    PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE_LIST_BIT = 0x01,
    PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE_STRIP_BIT = 0x02,
    PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE_FAN_BIT = 0x04,
    PRIMITIVE_TOPOLOGY_TYPE_LINE_LIST_BIT = 0x08,
    PRIMITIVE_TOPOLOGY_TYPE_LINE_STRIP_BIT = 0x10,
    PRIMITIVE_TOPOLOGY_TYPE_POINT_LIST_BIT = 0x20,
    PRIMITIVE_TOPOLOGY_TYPE_MAX_BIT = PRIMITIVE_TOPOLOGY_TYPE_POINT_LIST_BIT << 1
} primitive_topology_type_bits;

/** @brief A combination of topology bit flags */
typedef u32 primitive_topology_types;

/** @brief Represents supported texture filtering modes */
typedef enum texture_filter
{
    /** @brief Nearest-neighbor filtering */
    TEXTURE_FILTER_MODE_NEAREST = 0x0,
    /** @brief Linear (i.e. bilinear) filtering */
    TEXTURE_FILTER_MODE_LINEAR = 0x1
} texture_filter;

typedef enum texture_repeat
{
    TEXTURE_REPEAT_REPEAT = 0x0,
    TEXTURE_REPEAT_MIRRORED_REPEAT = 0x1,
    TEXTURE_REPEAT_CLAMP_TO_EDGE = 0x2,
    TEXTURE_REPEAT_CLAMP_TO_BORDER = 0x3,
    TEXTURE_REPEAT_COUNT
} texture_repeat;

typedef enum texture_channel
{
    TEXTURE_CHANNEL_R,
    TEXTURE_CHANNEL_G,
    TEXTURE_CHANNEL_B,
    TEXTURE_CHANNEL_A
} texture_channel;

/** @brief Shader stages available in the system */
typedef enum shader_stage
{
    SHADER_STAGE_VERTEX = 0x00000001,
    SHADER_STAGE_GEOMETRY = 0x00000002,
    SHADER_STAGE_FRAGMENT = 0x00000004,
    SHADER_STAGE_COMPUTE = 0x0000008
} shader_stage;

typedef enum shader_update_frequency
{
    /** @brief The uniform is updated once per frame */
    SHADER_UPDATE_FREQUENCY_PER_FRAME = 0,
    /** @brief The uniform is updated once per "group", it is up to the shader using this to determine what this means */
    SHADER_UPDATE_FREQUENCY_PER_GROUP = 1,
    /** @brief The uniform is updated once per draw call (i.e. "instance" of an object in the world) */
    SHADER_UPDATE_FREQUENCY_PER_DRAW = 2
} shader_update_frequency;

/** @brief Available attribute types */
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

/** @brief Available uniform types */
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
    // Struct uniform type. Requires size to be used
    SHADER_UNIFORM_TYPE_STRUCT = 11U,
    SHADER_UNIFORM_TYPE_TEXTURE_1D = 12U,
    SHADER_UNIFORM_TYPE_TEXTURE_2D = 13U,
    SHADER_UNIFORM_TYPE_TEXTURE_3D = 14U,
    SHADER_UNIFORM_TYPE_TEXTURE_CUBE = 15U,
    SHADER_UNIFORM_TYPE_TEXTURE_1D_ARRAY = 16U,
    SHADER_UNIFORM_TYPE_TEXTURE_2D_ARRAY = 17U,
    SHADER_UNIFORM_TYPE_TEXTURE_CUBE_ARRAY = 18U,
    SHADER_UNIFORM_TYPE_SAMPLER = 19U,

    SHADER_UNIFORM_TYPE_CUSTOM = 255U
} shader_uniform_type;

typedef enum shader_generic_sampler
{
    SHADER_GENERIC_SAMPLER_LINEAR_REPEAT,
    SHADER_GENERIC_SAMPLER_LINEAR_REPEAT_MIRRORED,
    SHADER_GENERIC_SAMPLER_LINEAR_CLAMP,
    SHADER_GENERIC_SAMPLER_LINEAR_CLAMP_BORDER,
    SHADER_GENERIC_SAMPLER_NEAREST_REPEAT,
    SHADER_GENERIC_SAMPLER_NEAREST_REPEAT_MIRRORED,
    SHADER_GENERIC_SAMPLER_NEAREST_CLAMP,
    SHADER_GENERIC_SAMPLER_NEAREST_CLAMP_BORDER,
    
    SHADER_GENERIC_SAMPLER_LINEAR_REPEAT_NO_ANISOTROPY,
    SHADER_GENERIC_SAMPLER_LINEAR_REPEAT_MIRRORED_NO_ANISOTROPY,
    SHADER_GENERIC_SAMPLER_LINEAR_CLAMP_NO_ANISOTROPY,
    SHADER_GENERIC_SAMPLER_LINEAR_CLAMP_BORDER_NO_ANISOTROPY,
    SHADER_GENERIC_SAMPLER_NEAREST_REPEAT_NO_ANISOTROPY,
    SHADER_GENERIC_SAMPLER_NEAREST_REPEAT_MIRRORED_NO_ANISOTROPY,
    SHADER_GENERIC_SAMPLER_NEAREST_CLAMP_NO_ANISOTROPY,
    SHADER_GENERIC_SAMPLER_NEAREST_CLAMP_BORDER_NO_ANISOTROPY,

    SHADER_GENERIC_SAMPLER_COUNT,
} shader_generic_sampler;

/** @brief Represents a single entry in the internal uniform array */
typedef struct shader_uniform
{
    bname name;
    /** @brief The offset in bytes from the beginning of the uniform set (global/instance/local) */
    u64 offset;
    /**
     * @brief The location to be used as a lookup. Typically the same as the index except for samplers,
     * which is used to lookup texture index within the internal array at the given scope (global/instance)
     */
    u16 location;
    /** @brief Index into the internal uniform array */
    u16 index;
    /** @brief The size of the uniform, or 0 for samplers */
    u16 size;
    /** @brief The index of the descriptor set the uniform belongs to (0=per_frame, 1=per_group, INVALID_ID=per_draw) */
    u8 set_index;
    /** @brief The update frequency of the uniform */
    shader_update_frequency frequency;
    /** @brief The type of uniform */
    shader_uniform_type type;
    /** @brief The length of the array if it is one; otherwise 0 */
    u32 array_length;
} shader_uniform;

/** @brief Represents a single shader vertex attribute */
typedef struct shader_attribute
{
    /** @brief The attribute name */
    bname name;
    /** @brief The attribute type */
    shader_attribute_type type;
    /** @brief The attribute size in bytes */
    u32 size;
} shader_attribute;

/** @brief Various shader flag bit fields */
typedef enum shader_flags_bits
{
    SHADER_FLAG_NONE_BIT = 0x0000,
    // Reads from depth buffer
    SHADER_FLAG_DEPTH_TEST_BIT = 0x0001,
    // Writes to depth buffer
    SHADER_FLAG_DEPTH_WRITE_BIT = 0x0002,
    SHADER_FLAG_WIREFRAME_BIT = 0x0004,
    // Reads from depth buffer
    SHADER_FLAG_STENCIL_TEST_BIT = 0x0008,
    // Writes to depth buffer
    SHADER_FLAG_STENCIL_WRITE_BIT = 0x0010,
    // Reads from color buffer
    SHADER_FLAG_COLOR_READ_BIT = 0x0020,
    // Writes to color buffer
    SHADER_FLAG_COLOR_WRITE_BIT = 0x0040
} shader_flags_bits;

/** @brief A combination of topology bit flags */
typedef u32 shader_flags;

/** @brief Represents data required for a particular update frequency within a shader */
typedef struct shader_frequency_data
{
    /** @brief The number of non-sampler and non-texture uniforms for this frequency */
    u8 uniform_count;
    /** @brief The number of sampler uniforms for this frequency */
    u8 uniform_sampler_count;
    /** @brief Keeps the uniform indices of samplers for fast lookups */
    u32* sampler_indices;
    /** @brief The number of texture uniforms for this frequency */
    u8 uniform_texture_count;
    /** @brief Keeps the uniform indices of textures for fast lookups */
    u32* texture_indices;
    /** @brief The actual size of the uniform buffer object for this frequency */
    u64 ubo_size;

    /** @brief The identifier of the currently bound group/per_draw. Ignored for per_frame */
    u32 bound_id;
} shader_frequency_data;

/** @brief Represents the current state of a given shader */
typedef enum shader_state
{
    /** @brief The shader has not yet gone through the creation process, and is unusable */
    SHADER_STATE_NOT_CREATED,
    /** @brief The shader has gone through the creation process, but not initialization. It is unusable */
    SHADER_STATE_UNINITIALIZED,
    /** @brief The shader is created and initialized, and is ready for use */
    SHADER_STATE_INITIALIZED,
} shader_state;

typedef struct shader_stage_config
{
    shader_stage stage;
    bname resource_name;
    bname package_name;
    char* source;
} shader_stage_config;

/** @brief Configuration for an attribute */
typedef struct shader_attribute_config
{
    /** @brief The name of the attribute */
    bname name;
    /** @brief The size of the attribute */
    u8 size;
    /** @brief The type of the attribute */
    shader_attribute_type type;
} shader_attribute_config;

/** @brief Configuration for a uniform */
typedef struct shader_uniform_config
{
    /** @brief The length of the name */
    u8 name_length;
    /** @brief The name of the uniform */
    bname name;
    /** @brief The size of the uniform. If arrayed, this is the per-element size */
    u16 size;
    /** @brief The location of the uniform */
    u32 location;
    /** @brief The type of the uniform */
    shader_uniform_type type;
    /** @brief The array length, if uniform is an array */
    u32 array_length;
    /** @brief The update frequency of the uniform */
    shader_update_frequency frequency;
} shader_uniform_config;

typedef enum bmaterial_type
{
    BMATERIAL_TYPE_UNKNOWN = 0,
    BMATERIAL_TYPE_STANDARD,
    BMATERIAL_TYPE_WATER,
    BMATERIAL_TYPE_BLENDED,
    BMATERIAL_TYPE_COUNT,
    BMATERIAL_TYPE_CUSTOM = 99
} bmaterial_type;

typedef enum bmaterial_model
{
    BMATERIAL_MODEL_UNLIT = 0,
    BMATERIAL_MODEL_PBR,
    BMATERIAL_MODEL_PHONG,
    BMATERIAL_MODEL_COUNT,
    BMATERIAL_MODEL_CUSTOM = 99
} bmaterial_model;

typedef enum bmaterial_texture_map
{
    BMATERIAL_TEXTURE_MAP_BASE_COLOR,
    BMATERIAL_TEXTURE_MAP_NORMAL,
    BMATERIAL_TEXTURE_MAP_METALLIC,
    BMATERIAL_TEXTURE_MAP_ROUGHNESS,
    BMATERIAL_TEXTURE_MAP_AO,
    BMATERIAL_TEXTURE_MAP_MRA,
    BMATERIAL_TEXTURE_MAP_EMISSIVE,
} bmaterial_texture_map;

typedef enum bmaterial_flag_bits
{
    // Material is marked as having transparency. If not set, alpha of albedo will not be used
    BMATERIAL_FLAG_HAS_TRANSPARENCY_BIT = 0x0001,
    // Material is double-sided
    BMATERIAL_FLAG_DOUBLE_SIDED_BIT = 0x0002,
    // Material recieves shadows
    BMATERIAL_FLAG_RECIEVES_SHADOW_BIT = 0x0004,
    // Material casts shadows
    BMATERIAL_FLAG_CASTS_SHADOW_BIT = 0x0008,
    // Material normal map enabled. A default z-up value will be used if not set
    BMATERIAL_FLAG_NORMAL_ENABLED_BIT = 0x0010,
    // Material AO map is enabled. A default of 1.0 (white) will be used if not set
    BMATERIAL_FLAG_AO_ENABLED_BIT = 0x0020,
    // Material emissive map is enabled. Emissive map is ignored if not set
    BMATERIAL_FLAG_EMISSIVE_ENABLED_BIT = 0x0040,
    // Material combined MRA (metallic/roughness/ao) map is enabled. MRA map is ignored if not set
    BMATERIAL_FLAG_MRA_ENABLED_BIT = 0x0080,
    // Material refraction map is enabled. Refraction map is ignored if not set
    BMATERIAL_FLAG_REFRACTION_ENABLED_BIT = 0x0100,
    // Material uses vertex color data as the base color
    BMATERIAL_FLAG_USE_VERTEX_COLOR_AS_BASE_COLOR_BIT = 0x0200
} bmaterial_flag_bits;

typedef u32 bmaterial_flags;

// Configuration for a material texture input
typedef struct bmaterial_texture_input
{
    // Name of the resource
    bname resource_name;
    // Name of the package containing the resource
    bname package_name;
    // Name of the custom sampler, if one
    bname sampler_name;
    // The texture channel to sample, if relevant
    texture_channel channel;
} bmaterial_texture_input;

// The configuration for a custom material sampler
typedef struct bmaterial_sampler_config
{
    bname name;
    texture_filter filter_min;
    texture_filter filter_mag;
    texture_repeat repeat_u;
    texture_repeat repeat_v;
    texture_repeat repeat_w;
} bmaterial_sampler_config;
