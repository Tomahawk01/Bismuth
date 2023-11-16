#pragma once

#include "defines.h"
#include "core/asserts.h"
#include "renderer/renderer_types.inl"
#include "containers/freelist.h"
#include "containers/hashtable.h"

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

    /** @brief Indicates if this device supports native dynamic topology (Vulkan API >= 1.3) */
    VULKAN_DEVICE_SUPPORT_FLAG_NATIVE_DYNAMIC_TOPOLOGY_BIT = 0x01,
    /** @brief Indicates if this device supports dynamic topology. If not, renderer will need to generate a separate pipeline per topology type */
    VULKAN_DEVICE_SUPPORT_FLAG_DYNAMIC_TOPOLOGY_BIT = 0x02,

    VULKAN_DEVICE_SUPPORT_FLAG_LINE_SMOOTH_RASTERISATION_BIT = 0x04,

    VULKAN_DEVICE_SUPPORT_FLAG_NATIVE_DYNAMIC_FRONT_FACE_BIT = 0x08,
    VULKAN_DEVICE_SUPPORT_FLAG_DYNAMIC_FRONT_FACE_BIT = 0x10
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
    VkImageView view;
    VkMemoryRequirements memory_requirements;
    VkMemoryPropertyFlags memory_flags;
    u32 width;
    u32 height;
    char* name;
} vulkan_image;

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

    f32 depth;
    u32 stencil;

    vulkan_render_pass_state state;
} vulkan_renderpass;

typedef struct vulkan_swapchain
{
    VkSurfaceFormatKHR image_format;

    u8 max_frames_in_flight;

    renderer_config_flags flags;

    VkSwapchainKHR handle;
    u32 image_count;
    texture* render_textures;

    texture* depth_textures;

    render_target render_targets[3];
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
    vulkan_renderpass* renderpass;
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
    b8 is_wireframe;
    u32 shader_flags;
    u32 push_constant_range_count;
    range* push_constant_ranges;
    u32 topology_types;
    renderer_winding winding;
} vulkan_pipeline_config;

typedef struct vulkan_pipeline
{
    VkPipeline handle;
    VkPipelineLayout pipeline_layout;
    u32 supported_topology_types;
} vulkan_pipeline;

// Max number of material instances
// TODO: make configurable
#define VULKAN_MAX_MATERIAL_COUNT 1024

// Max number of simultaneously uploaded geometries
// TODO: make configurable
#define VULKAN_MAX_GEOMETRY_COUNT 4096

/**
 * @brief Internal buffer data for geometry
 */
typedef struct vulkan_geometry_data
{
    u32 id;
    u32 generation;

    u64 vertex_buffer_offset;
    u64 index_buffer_offset;
} vulkan_geometry_data;

#define VULKAN_MAX_UI_COUNT 1024

#define VULKAN_SHADER_MAX_STAGES 8
#define VULKAN_SHADER_MAX_GLOBAL_TEXTURES 31
#define VULKAN_SHADER_MAX_INSTANCE_TEXTURES 31
#define VULKAN_SHADER_MAX_ATTRIBUTES 16

#define VULKAN_SHADER_MAX_UNIFORMS 128

#define VULKAN_SHADER_MAX_BINDINGS 2
#define VULKAN_SHADER_MAX_PUSH_CONST_RANGES 32

typedef struct vulkan_shader_stage_config
{
    VkShaderStageFlagBits stage;
    char file_name[255];
} vulkan_shader_stage_config;

typedef struct vulkan_descriptor_set_config
{
    u8 binding_count;
    VkDescriptorSetLayoutBinding bindings[VULKAN_SHADER_MAX_BINDINGS];
    u8 sampler_binding_index;
} vulkan_descriptor_set_config;

typedef struct vulkan_shader_config
{
    u8 stage_count;
    vulkan_shader_stage_config stages[VULKAN_SHADER_MAX_STAGES];
    VkDescriptorPoolSize pool_sizes[2];
    u16 max_descriptor_set_count;

    u8 descriptor_set_count;
    vulkan_descriptor_set_config descriptor_sets[2];

    VkVertexInputAttributeDescription attributes[VULKAN_SHADER_MAX_ATTRIBUTES];

    face_cull_mode cull_mode;
} vulkan_shader_config;

typedef struct vulkan_descriptor_state
{
    u8 generations[3];
    u32 ids[3];
} vulkan_descriptor_state;

typedef struct vulkan_shader_descriptor_set_state
{
    VkDescriptorSet descriptor_sets[3];

    vulkan_descriptor_state descriptor_states[VULKAN_SHADER_MAX_BINDINGS];
} vulkan_shader_descriptor_set_state;

// Instance-level state for a shader
typedef struct vulkan_shader_instance_state
{
    u32 id;
    u64 offset;

    vulkan_shader_descriptor_set_state descriptor_set_state;

    struct texture_map** instance_texture_maps;
} vulkan_shader_instance_state;

typedef struct vulkan_shader
{
    void* mapped_uniform_buffer_block;

    u32 id;

    vulkan_shader_config config;

    vulkan_renderpass* renderpass;

    vulkan_shader_stage stages[VULKAN_SHADER_MAX_STAGES];

    VkDescriptorPool descriptor_pool;

    VkDescriptorSetLayout descriptor_set_layouts[2];
    VkDescriptorSet global_descriptor_sets[3];
    renderbuffer uniform_buffer;

    vulkan_pipeline** pipelines;

    vulkan_pipeline** clockwise_pipelines;

    u8 bound_pipeline_index;
    VkPrimitiveTopology current_topology;

    // TODO: make dynamic
    u32 instance_count;
    vulkan_shader_instance_state instance_states[VULKAN_MAX_MATERIAL_COUNT];

    u8 global_uniform_count;
    u8 global_uniform_sampler_count;
    u8 instance_uniform_count;
    u8 instance_uniform_sampler_count;
    u8 local_uniform_count;
} vulkan_shader;

typedef struct vulkan_context
{
    /** @brief Instance-level api major version */
    u32 api_major;
    /** @brief Instance-level api minor version */
    u32 api_minor;
    /** @brief Instance-level api patch version */
    u32 api_patch;

    u32 framebuffer_width;
    u32 framebuffer_height;

    u64 framebuffer_size_generation;
    u64 framebuffer_size_last_generation;

    vec4 viewport_rect;
    vec4 scissor_rect;

    VkInstance instance;
    VkAllocationCallbacks* allocator;
    VkSurfaceKHR surface;

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

    vulkan_swapchain swapchain;

    renderbuffer object_vertex_buffer;
    renderbuffer object_index_buffer;

    // darray
    vulkan_command_buffer* graphics_command_buffers;

    // darray
    VkSemaphore* image_available_semaphores;

    // darray
    VkSemaphore* queue_complete_semaphores;

    u32 in_flight_fence_count;
    VkFence in_flight_fences[2];

    // Holds pointers to fences which exist and are owned elsewhere, one per frame
    VkFence images_in_flight[3];

    u32 image_index;
    u32 current_frame;

    b8 recreating_swapchain;

    b8 render_flag_changed;

    // TODO: make dynamic
    vulkan_geometry_data geometries[VULKAN_MAX_GEOMETRY_COUNT];

    render_target world_render_targets[3];

    b8 multithreading_enabled;

    i32 (*find_memory_index)(struct vulkan_context* context, u32 type_filter, u32 property_flags);

    PFN_vkCmdSetPrimitiveTopologyEXT vkCmdSetPrimitiveTopologyEXT;
    PFN_vkCmdSetFrontFaceEXT vkCmdSetFrontFaceEXT;

    // Pointer to the currently bound shader
    struct shader* bound_shader;
} vulkan_context;
