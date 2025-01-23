#pragma once

#include "core_render_types.h"
#include "debug/bassert.h"
#include "defines.h"
#include "identifiers/bhandle.h"
#include "bresources/bresource_types.h"
#include "renderer/renderer_types.h"
#include "vulkan/vulkan_core.h"

#include <vulkan/vulkan.h>

// Checks the given expression's return value against VK_SUCCESS
#define VK_CHECK(expr) BASSERT(expr == VK_SUCCESS)

struct vulkan_context;

typedef struct vulkan_buffer
{
    VkBuffer handle;
    VkBufferUsageFlagBits usage;
    b8 is_locked;
    VkDeviceMemory memory;
    VkMemoryRequirements memory_requirements;
    i32 memory_index;
    u32 memory_property_flags;
} vulkan_buffer;

typedef struct vulkan_swapchain_support_info
{
    VkSurfaceCapabilitiesKHR capabilities;
    u32 format_count;
    VkSurfaceFormatKHR* formats;
    u32 present_mode_count;
    VkPresentModeKHR* present_modes;
} vulkan_swapchain_support_info;

typedef enum vulkan_device_support_flag_bits
{
    VULKAN_DEVICE_SUPPORT_FLAG_NONE_BIT = 0x00,

    /** @brief Indicates if this device supports native dynamic state (Vulkan API >= 1.3) */
    VULKAN_DEVICE_SUPPORT_FLAG_NATIVE_DYNAMIC_STATE_BIT = 0x01,
    /** @brief Indicates if this device supports dynamic state. If not, renderer will need to generate a separate pipeline per topology type */
    VULKAN_DEVICE_SUPPORT_FLAG_DYNAMIC_STATE_BIT = 0x02,
    VULKAN_DEVICE_SUPPORT_FLAG_LINE_SMOOTH_RASTERISATION_BIT = 0x04
} vulkan_device_support_flag_bits;

/** @brief Bitwise flags for device support. @see vulkan_device_support_flag_bits. */
typedef u32 vulkan_device_support_flags;

typedef struct vulkan_device
{
    /** @brief Supported device-level api major version */
    u32 api_major;
    /** @brief Supported device-level api minor version */
    u32 api_minor;
    /** @brief Supported device-level api patch version */
    u32 api_patch;

    VkPhysicalDevice physical_device;
    VkDevice logical_device;
    vulkan_swapchain_support_info swapchain_support;

    i32 graphics_queue_index;
    i32 present_queue_index;
    i32 transfer_queue_index;
    b8 supports_device_local_host_visible;

    VkQueue graphics_queue;
    VkQueue present_queue;
    VkQueue transfer_queue;

    VkCommandPool graphics_command_pool;

    VkPhysicalDeviceProperties properties;
    VkPhysicalDeviceFeatures features;
    VkPhysicalDeviceMemoryProperties memory;

    VkFormat depth_format;
    u8 depth_channel_count;

    /** @brief Indicates support for various features */
    vulkan_device_support_flags support_flags;
} vulkan_device;

typedef struct vulkan_image
{
    VkImage handle;
    VkDeviceMemory memory;

    VkImageCreateInfo image_create_info;
    VkImageView view;
    VkImageSubresourceRange view_subresource_range;
    VkImageViewCreateInfo view_create_info;

    VkImageView* layer_views;
    VkImageSubresourceRange* layer_view_subresource_ranges;
    VkImageViewCreateInfo* layer_view_create_infos;

    VkMemoryRequirements memory_requirements;
    VkMemoryPropertyFlags memory_flags;

    VkFormat format;
    u32 width;
    u32 height;
    u16 layer_count;
    char* name;
    u32 mip_levels;
    b8 has_view;
} vulkan_image;

// Struct definition for renderer-specific framebuffer data
typedef struct framebuffer_internal_data
{
    // The number of VkFramebuffers in the array. Typically 1 unless the attachment requires the frame_count to be taken into account
    u32 framebuffer_count;
    // Array of framebuffers
    VkFramebuffer* framebuffers;
} framebuffer_internal_data;

typedef enum vulkan_render_pass_state
{
    READY,
    RECORDING,
    IN_RENDER_PASS,
    RECORDING_ENDED,
    SUBMITTED,
    NOT_ALLOCATED
} vulkan_render_pass_state;

typedef struct vulkan_renderpass
{
    VkRenderPass handle;

    vulkan_render_pass_state state;

    // darray
    VkClearValue* clear_values;
    /* u32 clear_value_count; */
} vulkan_renderpass;

typedef struct vulkan_swapchain
{
    VkSurfaceFormatKHR image_format;

    renderer_config_flags flags;

    VkSwapchainKHR handle;
    u32 image_count;

    // Track the owning window in case something is needed from it
    struct bwindow* owning_window;

    /** @brief Supports being used as a blit destination */
    b8 supports_blit_dest;

    /** @brief Supports being used as a blit source */
    b8 supports_blit_src;
} vulkan_swapchain;

typedef enum vulkan_command_buffer_state
{
    COMMAND_BUFFER_STATE_READY,
    COMMAND_BUFFER_STATE_RECORDING,
    COMMAND_BUFFER_STATE_IN_RENDER_PASS,
    COMMAND_BUFFER_STATE_RECORDING_ENDED,
    COMMAND_BUFFER_STATE_SUBMITTED,
    COMMAND_BUFFER_STATE_NOT_ALLOCATED
} vulkan_command_buffer_state;

typedef struct vulkan_command_buffer
{
    VkCommandBuffer handle;

#ifdef BISMUTH_DEBUG
    // Name, kept for debugging purposes
    const char* name;
#endif

    // Command buffer state
    vulkan_command_buffer_state state;

    // Indicates if this is a primary or secondary command buffer
    b8 is_primary;

    // The number of secondary buffers that are children to this one. Primary buffer use only
    u16 secondary_count;
    // An array of secondary buffers that are children to this one. Primary buffer use only
    struct vulkan_command_buffer* secondary_buffers;

    // The currently selected secondary buffer index
    u16 secondary_buffer_index;
    // Indicates if a secondary command buffer is currently being recorded to
    b8 in_secondary;

    // A pointer to the parent (primary) command buffer, if there is one. Only applies to secondary buffers
    struct vulkan_command_buffer* parent;
} vulkan_command_buffer;

typedef struct vulkan_shader_stage
{
    VkShaderModuleCreateInfo create_info;
    VkShaderModule handle;
    VkPipelineShaderStageCreateInfo shader_stage_create_info;
} vulkan_shader_stage;

typedef enum vulkan_topology_class
{
    VULKAN_TOPOLOGY_CLASS_POINT = 0,
    VULKAN_TOPOLOGY_CLASS_LINE = 1,
    VULKAN_TOPOLOGY_CLASS_TRIANGLE = 2,
    VULKAN_TOPOLOGY_CLASS_MAX = VULKAN_TOPOLOGY_CLASS_TRIANGLE + 1
} vulkan_topology_class;

typedef struct vulkan_pipeline_config
{
    char* name;
    u32 stride;
    u32 attribute_count;
    VkVertexInputAttributeDescription* attributes;
    u32 descriptor_set_layout_count;
    VkDescriptorSetLayout* descriptor_set_layouts;
    u32 stage_count;
    VkPipelineShaderStageCreateInfo* stages;
    VkViewport viewport;
    VkRect2D scissor;
    face_cull_mode cull_mode;
    u32 shader_flags;
    u32 push_constant_range_count;
    range* push_constant_ranges;
    u32 topology_types;
    renderer_winding winding;

    u32 color_attachment_count;
    VkFormat* color_attachment_formats;
    VkFormat depth_attachment_format;
    VkFormat stencil_attachment_format;
} vulkan_pipeline_config;

typedef struct vulkan_pipeline
{
    VkPipeline handle;
    VkPipelineLayout pipeline_layout;
    u32 supported_topology_types;
} vulkan_pipeline;

#define VULKAN_SHADER_MAX_STAGES 8
#define VULKAN_SHADER_MAX_TEXTURE_BINDINGS 16
#define VULKAN_SHADER_MAX_SAMPLER_BINDINGS 16
#define VULKAN_SHADER_MAX_ATTRIBUTES 16

#define VULKAN_SHADER_MAX_UNIFORMS 128

#define VULKAN_SHADER_MAX_PUSH_CONST_RANGES 32

// Max number of descriptor sets based on frequency (0=per-frame, 1=per-group, 2=per-draw)
#define VULKAN_SHADER_DESCRIPTOR_SET_LAYOUT_COUNT 3

typedef struct vulkan_descriptor_set_config
{
    u8 binding_count;
    VkDescriptorSetLayoutBinding* bindings;
} vulkan_descriptor_set_config;

typedef struct vulkan_descriptor_state
{
    /** @brief The renderer frame number on which this descriptor was last updated. One per swapchain image. INVALID_ID_U16 if never loaded */
    u16* renderer_frame_number;
} vulkan_descriptor_state;

typedef struct vulkan_uniform_sampler_state
{
    shader_uniform uniform;

    bhandle* sampler_handles;

    vulkan_descriptor_state* descriptor_states;
} vulkan_uniform_sampler_state;

typedef struct vulkan_uniform_texture_state
{
    shader_uniform uniform;

    /** @brief An array of handles to texture resources */
    bhandle* texture_handles;

    /** @brief A descriptor state per sampler. Count matches uniform array_count */
    vulkan_descriptor_state* descriptor_states;
} vulkan_uniform_texture_state;

// Frequency-level state for a shader (i.e. per-frame, per-group, per-draw)
typedef struct vulkan_shader_frequency_state
{
    u32 id;
    u64 offset;

    VkDescriptorSet* descriptor_sets;

    // UBO descriptor state
    vulkan_descriptor_state ubo_descriptor_state;

    // A mapping of sampler uniforms to descriptors
    vulkan_uniform_sampler_state* sampler_states;
    // A mapping of texture uniforms to descriptors
    vulkan_uniform_texture_state* texture_states;
#ifdef BISMUTH_DEBUG
    u32 descriptor_set_index;
    shader_update_frequency frequency;
#endif
} vulkan_shader_frequency_state;

/** @brief Contains vulkan shader frequency specific info for UBOs */
typedef struct vulkan_shader_frequency_info
{
    /** @brief The actual size of the uniform buffer object for this frequency */
    u64 ubo_size;
    /** @brief The stride of the uniform buffer object for this frequency */
    u64 ubo_stride;
    /** @brief The offset in bytes for the UBO from the beginning of the uniform buffer for this frequency */
    u64 ubo_offset;

    /** @brief The number of non-sampler and non-texture uniforms for this frequency */
    u8 uniform_count;
    /** @brief The number of sampler uniforms for this frequency */
    u8 uniform_sampler_count;
    /** @brief Darray. Keeps the uniform indices of samplers for fast lookups */
    u32* sampler_indices;
    /** @brief The number of texture uniforms for this frequency */
    u8 uniform_texture_count;
    /** @brief Darray. Keeps the uniform indices of textures for fast lookups */
    u32* texture_indices;

    /** @brief The currently-bound id for this frequency */
    u32 bound_id;
} vulkan_shader_frequency_info;

typedef struct vulkan_shader
{
    // The name of the shader (mostly kept for debugging purposes)
    bname name;
    /** @brief The block of memory mapped to the each per-swapchain-image uniform buffer */
    void** mapped_uniform_buffer_blocks;
    /** @brief The block of memory used for push constants, 128B */
    void* per_draw_push_constant_block;

    u32 id;

    u16 max_descriptor_set_count;

    u8 descriptor_set_count;
    /** @brief Descriptor sets, max of 3. Index 0=per_frame, 1=per_group, 2=per_draw */
    vulkan_descriptor_set_config descriptor_sets[VULKAN_SHADER_DESCRIPTOR_SET_LAYOUT_COUNT];
    
    /** @brief The number of vertex attributes in the shader */
    u8 attribute_count;
    VkVertexInputAttributeDescription attributes[VULKAN_SHADER_MAX_ATTRIBUTES];

    /** @brief The number of uniforms in the shader */
    u32 uniform_count;

    /** @brief An array of uniforms in the shader */
    shader_uniform* uniforms;

    /** @brief The size of all attributes combined, a.k.a. the size of a vertex */
    u32 attribute_stride;

    face_cull_mode cull_mode;

    /** @brief The topology types for the shader pipeline. See primitive_topology_type. Defaults to "triangle list" if unspecified */
    u32 topology_types;

    u32 max_groups;

    u32 max_per_draw_count;

    u8 stage_count;

    vulkan_shader_stage stages[VULKAN_SHADER_MAX_STAGES];

    u32 pool_size_count;

    VkDescriptorPoolSize pool_sizes[3];

    VkDescriptorPool descriptor_pool;

    VkDescriptorSetLayout descriptor_set_layouts[VULKAN_SHADER_DESCRIPTOR_SET_LAYOUT_COUNT];

    /** @brief The uniform buffers used by this shader, one per swapchain image */
    renderbuffer* uniform_buffers;
    u32 uniform_buffer_count;

    vulkan_pipeline** pipelines;
    vulkan_pipeline** wireframe_pipelines;

    u8 bound_pipeline_index;
    VkPrimitiveTopology current_topology;

    /** @brief The per-frame frequency state */
    vulkan_shader_frequency_state per_frame_state;

    /** @brief The per-group frequency states for all groups */
    vulkan_shader_frequency_state* group_states;

    vulkan_shader_frequency_state* per_draw_states;

    /** @brief The amount of bytes that are required for UBO alignment */
    u64 required_ubo_alignment;

    vulkan_shader_frequency_info per_frame_info;
    vulkan_shader_frequency_info per_group_info;
    vulkan_shader_frequency_info per_draw_info;

    // Shader flags
    shader_flags flags;
} vulkan_shader;

// Forward declare shaderc compiler
struct shaderc_compiler;

typedef struct bwindow_renderer_backend_state
{
    /** @brief The internal Vulkan surface for the window to be drawn to */
    VkSurfaceKHR surface;
    /** @brief The swapchain */
    vulkan_swapchain swapchain;

    /** @brief The current image index */
    u32 image_index;
    /** @brief The current frame index ( % by max_frames_in_flight) */
    u32 current_frame;

    /** @brief Indicates the max number of frames in flight. 1 for double-buffering, 2 for triple-buffering */
    u8 max_frames_in_flight;

    /** @brief Indicates if the swapchain is currently being recreated */
    b8 recreating_swapchain;

    /** @brief The graphics command buffers, one per swapchain image */
    vulkan_command_buffer* graphics_command_buffers;

    VkSemaphore* image_available_semaphores;
    VkSemaphore* queue_complete_semaphores;

    /** @brief The in-flight fences, used to indicate to the application when a frame is busy/ready. One per frame in flight */
    VkFence* in_flight_fences;

    /** @brief Resusable staging buffers (one per frame in flight) to transfer data from a resource to a GPU-only buffer */
    renderbuffer* staging;

    /** @brief Array of darrays of handles to textures that were updated as part of a frame's workload. One list per frame in flight */
    bhandle** frame_texture_updated_list;

    u64 framebuffer_size_generation;
    u64 framebuffer_previous_size_generation;

    u8 skip_frames;
} bwindow_renderer_backend_state;

typedef struct vulkan_sampler_handle_data
{
    // Used for handle validation
    u64 handle_uniqueid;
    // The generation of the internal sampler. Incremented every time the sampler is changed
    u16 generation;
    // Sampler name for named lookups and serialization
    bname name;
    // The underlying sampler handle
    VkSampler sampler;
} vulkan_sampler_handle_data;

/** @brief Represents Vulkan-specific texture data */
typedef struct vulkan_texture_handle_data
{
    // Unique identifier for this texture
    u64 uniqueid;

    // The generation of the internal texture. Incremented every time the texture is changed
    u16 generation;

    // Number of vulkan_images in the array. This is typically 1 unless the texture requires the frame_count to be taken into account
    u32 image_count;
    // Array of images. See image_count
    vulkan_image* images;
} vulkan_texture_handle_data;

typedef struct vulkan_context
{
    /** @brief The instance-level api major version */
    u32 api_major;
    /** @brief The instance-level api minor version */
    u32 api_minor;
    /** @brief The instance-level api patch version */
    u32 api_patch;

    renderer_config_flags flags;

    /** @brief The currently cached color buffer clear value */
    VkClearColorValue color_clear_value;
    /** @brief The currently cached depth/stencil buffer clear value */
    VkClearDepthStencilValue depth_stencil_clear_value;

    vec4 viewport_rect;
    vec4 scissor_rect;

    VkInstance instance;
    VkAllocationCallbacks* allocator;

#if defined(_DEBUG)
    VkDebugUtilsMessengerEXT debug_messenger;

    /** @brief Function pointer to set debug object names. */
    PFN_vkSetDebugUtilsObjectNameEXT pfnSetDebugUtilsObjectNameEXT;

    /** @brief Function pointer to set free-form debug object tag data. */
    PFN_vkSetDebugUtilsObjectTagEXT pfnSetDebugUtilsObjectTagEXT;

    PFN_vkCmdBeginDebugUtilsLabelEXT pfnCmdBeginDebugUtilsLabelEXT;
    PFN_vkCmdEndDebugUtilsLabelEXT pfnCmdEndDebugUtilsLabelEXT;
#endif

    vulkan_device device;

    // A pointer to the current window whose resources should be used as default to render to
    struct bwindow* current_window;

    b8 render_flag_changed;

    b8 validation_enabled;

    b8 multithreading_enabled;

    /** @brief Indicates if triple-buffering is enabled (requested) */
    b8 triple_buffering_enabled;

    // Collection of samplers. darray
    vulkan_sampler_handle_data* samplers;
    // Collection of textures. darray
    vulkan_texture_handle_data* textures;

    /** @brief Collection of vulkan shaders (internal shader data). Matches size of shader array in shader system */
    vulkan_shader* shaders;

    i32 (*find_memory_index)(struct vulkan_context* context, u32 type_filter, u32 property_flags);

    PFN_vkCmdSetPrimitiveTopologyEXT vkCmdSetPrimitiveTopologyEXT;
    PFN_vkCmdSetFrontFaceEXT vkCmdSetFrontFaceEXT;
    PFN_vkCmdSetStencilTestEnableEXT vkCmdSetStencilTestEnableEXT;
    PFN_vkCmdSetDepthTestEnableEXT vkCmdSetDepthTestEnableEXT;
    PFN_vkCmdSetDepthWriteEnableEXT vkCmdSetDepthWriteEnableEXT;
    PFN_vkCmdSetStencilOpEXT vkCmdSetStencilOpEXT;

    PFN_vkCmdBeginRenderingKHR vkCmdBeginRenderingKHR;
    PFN_vkCmdEndRenderingKHR vkCmdEndRenderingKHR;

    // Pointer to the currently bound vulkan shader
    vulkan_shader* bound_shader;

    // Used for dynamic compilation of vulkan shaders (using the shaderc lib)
    struct shaderc_compiler* shader_compiler;
} vulkan_context;
