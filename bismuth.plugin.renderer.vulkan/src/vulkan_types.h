#pragma once

#include "defines.h"
#include "debug/bassert.h"
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

// Struct definition for renderer-specific texture data
typedef struct texture_internal_data
{
    // Number of vulkan_images in the array. This is typically 1 unless the texture requires the frame_count to be taken into account
    u32 image_count;
    // Array of images. Used when image_count > 1
    vulkan_image* images;
} texture_internal_data;

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

    u8 max_frames_in_flight;

    renderer_config_flags flags;

    VkSwapchainKHR handle;
    u32 image_count;

    // Track the owning window in case something is needed from it
    struct bwindow* owning_window;
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

    // Command buffer state
    vulkan_command_buffer_state state;
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
#define VULKAN_SHADER_MAX_TEXTURE_BINDINGS 31
#define VULKAN_SHADER_MAX_ATTRIBUTES 16

#define VULKAN_SHADER_MAX_UNIFORMS 128

#define VULKAN_SHADER_MAX_PUSH_CONST_RANGES 32

typedef struct vulkan_descriptor_set_config
{
    u8 binding_count;
    VkDescriptorSetLayoutBinding* bindings;
    u8 sampler_binding_index_start;
} vulkan_descriptor_set_config;

typedef struct vulkan_descriptor_state
{
    u8* generations;
    u32* ids;
    u64* frame_numbers;
} vulkan_descriptor_state;

typedef struct vulkan_uniform_sampler_state
{
    struct shader_uniform* uniform;

    struct texture_map** uniform_texture_maps;

    vulkan_descriptor_state* descriptor_states;
} vulkan_uniform_sampler_state;

// Instance-level state for a shader
typedef struct vulkan_shader_instance_state
{
    u32 id;
    u64 offset;

    VkDescriptorSet* descriptor_sets;

    // UBO descriptor
    vulkan_descriptor_state ubo_descriptor_state;

    // A mapping of sampler uniforms to descriptors and texture maps
    vulkan_uniform_sampler_state* sampler_uniforms;
} vulkan_shader_instance_state;

typedef struct vulkan_shader
{
    /** @brief The block of memory mapped to the each per-swapchain-image uniform buffer */
    void** mapped_uniform_buffer_blocks;
    /** @brief The block of memory used for push constants, 128B */
    void* local_push_constant_block;

    u32 id;

    u16 max_descriptor_set_count;

    u8 descriptor_set_count;
    vulkan_descriptor_set_config descriptor_sets[2];

    VkVertexInputAttributeDescription attributes[VULKAN_SHADER_MAX_ATTRIBUTES];

    face_cull_mode cull_mode;

    u32 max_instances;

    u8 stage_count;

    vulkan_shader_stage stages[VULKAN_SHADER_MAX_STAGES];

    u32 pool_size_count;

    VkDescriptorPoolSize pool_sizes[2];

    VkDescriptorPool descriptor_pool;

    VkDescriptorSetLayout descriptor_set_layouts[2];

    VkDescriptorSet* global_descriptor_sets;

    // UBO descriptor
    vulkan_descriptor_state global_ubo_descriptor_state;

    // A mapping of sampler uniforms to descriptors and texture maps
    vulkan_uniform_sampler_state* global_sampler_uniforms;

    /** @brief The uniform buffers used by this shader, one per swapchain image */
    renderbuffer* uniform_buffers;
    u32 uniform_buffer_count;

    vulkan_pipeline** pipelines;
    vulkan_pipeline** wireframe_pipelines;

    u8 bound_pipeline_index;
    VkPrimitiveTopology current_topology;

    vulkan_shader_instance_state* instance_states;
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

    u64 framebuffer_size_generation;
    u64 framebuffer_previous_size_generation;

    u8 skip_frames;
} bwindow_renderer_backend_state;

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

    // Collection of samplers. darray
    VkSampler* samplers;

    i32 (*find_memory_index)(struct vulkan_context* context, u32 type_filter, u32 property_flags);

    PFN_vkCmdSetPrimitiveTopologyEXT vkCmdSetPrimitiveTopologyEXT;
    PFN_vkCmdSetFrontFaceEXT vkCmdSetFrontFaceEXT;
    PFN_vkCmdSetStencilTestEnableEXT vkCmdSetStencilTestEnableEXT;
    PFN_vkCmdSetDepthTestEnableEXT vkCmdSetDepthTestEnableEXT;
    PFN_vkCmdSetDepthWriteEnableEXT vkCmdSetDepthWriteEnableEXT;
    PFN_vkCmdSetStencilOpEXT vkCmdSetStencilOpEXT;

    PFN_vkCmdBeginRenderingKHR vkCmdBeginRenderingKHR;
    PFN_vkCmdEndRenderingKHR vkCmdEndRenderingKHR;

    // Pointer to the currently bound shader
    struct shader* bound_shader;

    // Used for dynamic compilation of vulkan shaders (using the shaderc lib)
    struct shaderc_compiler* shader_compiler;
} vulkan_context;
