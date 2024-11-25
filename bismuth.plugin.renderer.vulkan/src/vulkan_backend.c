#include "vulkan_backend.h"

#include "containers/darray.h"
#include "core/engine.h"
#include "core/event.h"
#include "core/frame_data.h"
#include "core_render_types.h"
#include "debug/bassert.h"
#include "defines.h"
#include "identifiers/bhandle.h"
#include "bresources/bresource_types.h"
#include "logger.h"
#include "math/bmath.h"
#include "math/math_types.h"
#include "memory/bmemory.h"
#include "platform/platform.h"
#include "platform/vulkan_platform.h"
#include "renderer/renderer_frontend.h"
#include "renderer/renderer_utils.h"
#include "resources/resource_types.h"
#include "strings/bname.h"
#include "strings/bstring.h"
#include "systems/texture_system.h"
#include "vulkan_command_buffer.h"
#include "vulkan_device.h"
#include "vulkan_image.h"
#include "vulkan_swapchain.h"
#include "vulkan_types.h"
#include "vulkan_utils.h"

// For runtime shader compilation
#include <shaderc/shaderc.h>
#include <shaderc/status.h>

#include <renderer/renderer_types.h>
#include <utils/render_type_utils.h>
#include <vulkan/vulkan_core.h>

// NOTE: If wanting to trace allocations, uncomment this
// #ifndef BVULKAN_ALLOCATOR_TRACE
// #define BVULKAN_ALLOCATOR_TRACE 1
// #endif

// NOTE: To disable custom allocator, comment this out or set to 0
#ifndef BVULKAN_USE_CUSTOM_ALLOCATOR
#define BVULKAN_USE_CUSTOM_ALLOCATOR 1
#endif

VKAPI_ATTR VkBool32 VKAPI_CALL vk_debug_callback(
    VkDebugUtilsMessageSeverityFlagBitsEXT message_severity,
    VkDebugUtilsMessageTypeFlagsEXT message_types,
    const VkDebugUtilsMessengerCallbackDataEXT* callback_data, void* user_data);

static i32 find_memory_index(vulkan_context* context, u32 type_filter, u32 property_flags);

static void create_command_buffers(vulkan_context* context, bwindow* window);
static b8 recreate_swapchain(renderer_backend_interface* backend, bwindow* window);
static b8 create_shader_module(vulkan_context* context, bshader* s, shader_stage stage, const char* source, const char* filename, vulkan_shader_stage* out_stage);
static b8 vulkan_buffer_copy_range_internal(vulkan_context* context, VkBuffer source, u64 source_offset, VkBuffer dest, u64 dest_offset, u64 size, b8 queue_wait);
static vulkan_command_buffer* get_current_command_buffer(vulkan_context* context);
static u32 get_current_image_index(vulkan_context* context);
static u32 get_current_frame_index(vulkan_context* context);
static u32 get_image_count(vulkan_context* context);

static b8 vulkan_graphics_pipeline_create(vulkan_context* context, const vulkan_pipeline_config* config, vulkan_pipeline* out_pipeline);
static void vulkan_pipeline_destroy(vulkan_context* context, vulkan_pipeline* pipeline);
static void vulkan_pipeline_bind(vulkan_command_buffer* command_buffer, VkPipelineBindPoint bind_point, vulkan_pipeline* pipeline);
static b8 setup_frequency_state(vulkan_context* context, bshader* s, shader_update_frequency frequency, u32* out_frequency_id);
static b8 release_frequency_state(vulkan_context* context, bshader* s, shader_update_frequency frequency, u32 frequency_id);

// FIXME: May want to have this as a configurable option instead
// Forward declarations of custom vulkan allocator functions
#if BVULKAN_USE_CUSTOM_ALLOCATOR == 1
static void* vulkan_alloc_allocation(void* user_data, size_t size, size_t alignment, VkSystemAllocationScope allocation_scope);
static void vulkan_alloc_free(void* user_data, void* memory);
static void* vulkan_alloc_reallocation(void* user_data, void* original, size_t size, size_t alignment, VkSystemAllocationScope allocation_scope);
static void vulkan_alloc_internal_alloc(void* pUserData, size_t size, VkInternalAllocationType allocationType, VkSystemAllocationScope allocationScope);
static void vulkan_alloc_internal_free(void* pUserData, size_t size, VkInternalAllocationType allocationType, VkSystemAllocationScope allocationScope);
static b8 create_vulkan_allocator(vulkan_context* context, VkAllocationCallbacks* callbacks);
#endif // BVULKAN_USE_CUSTOM_ALLOCATOR == 1

b8 vulkan_renderer_backend_initialize(renderer_backend_interface* backend, const renderer_backend_config* config)
{
    backend->internal_context_size = sizeof(vulkan_context);
    backend->internal_context = ballocate(backend->internal_context_size, MEMORY_TAG_RENDERER);

    vulkan_context* context = (vulkan_context*)backend->internal_context;
    if (config->flags & RENDERER_CONFIG_FLAG_ENABLE_VALIDATION)
        context->validation_enabled = true;
    context->flags = config->flags;

    // Function pointers
    context->find_memory_index = find_memory_index;
    context->render_flag_changed = false;

    // NOTE: custom allocator
#if BVULKAN_USE_CUSTOM_ALLOCATOR == 1
    context->allocator = ballocate(sizeof(VkAllocationCallbacks), MEMORY_TAG_RENDERER);
    if (!create_vulkan_allocator(context, context->allocator))
    {
        // If this fails fall back to default allocator
        BFATAL("Failed to create custom Vulkan allocator. Continuing using the driver's default allocator");
        bfree(context->allocator, sizeof(VkAllocationCallbacks), MEMORY_TAG_RENDERER);
        context->allocator = 0;
    }
#else
    context->allocator = 0;
#endif

    // Get currently-installed instance version. Use this to create instance
    u32 api_version = 0;
    vkEnumerateInstanceVersion(&api_version);
    context->api_major = VK_VERSION_MAJOR(api_version);
    context->api_minor = VK_VERSION_MINOR(api_version);
    context->api_patch = VK_VERSION_PATCH(api_version);

    // Setup Vulkan instance
    VkApplicationInfo app_info = {VK_STRUCTURE_TYPE_APPLICATION_INFO};
    app_info.apiVersion = VK_MAKE_API_VERSION(0, context->api_major, context->api_minor, context->api_patch);
    app_info.pApplicationName = config->application_name;
    app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.pEngineName = "Bismuth Engine";
    app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);

    VkInstanceCreateInfo create_info = {VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
    create_info.pApplicationInfo = &app_info;

    // Obtain a list of required extensions
    const char** required_extensions = darray_create(const char*);
    darray_push(required_extensions, &VK_KHR_SURFACE_EXTENSION_NAME);   // Generic surface extension
    vulkan_platform_get_required_extension_names(&required_extensions); // Platform-specific extension(s)
    u32 required_extension_count = 0;
#if defined(_DEBUG)
    darray_push(required_extensions, &VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

    BDEBUG("Required extensions:");
    required_extension_count = darray_length(required_extensions);
    for (u32 i = 0; i < required_extension_count; ++i)
    {
        BDEBUG(required_extensions[i]);
    }
#endif

    create_info.enabledExtensionCount = darray_length(required_extensions);
    create_info.ppEnabledExtensionNames = required_extensions;

    u32 available_extension_count = 0;
    vkEnumerateInstanceExtensionProperties(0, &available_extension_count, 0);
    VkExtensionProperties* available_extensions = darray_reserve(VkExtensionProperties, available_extension_count);
    vkEnumerateInstanceExtensionProperties(0, &available_extension_count, available_extensions);

    // Verify required extensions are available.
    for (u32 i = 0; i < required_extension_count; ++i)
    {
        b8 found = false;
        for (u32 j = 0; j < available_extension_count; ++j)
        {
            if (strings_equal(required_extensions[i], available_extensions[j].extensionName))
            {
                found = true;
                BINFO("Required exension found: %s", required_extensions[i]);
                break;
            }
        }

        if (!found)
        {
            BFATAL("Required extension is missing: %s", required_extensions[i]);
            return false;
        }
    }

    // Validation layers
    const char** required_validation_layer_names = 0;
    u32 required_validation_layer_count = 0;

    // If validation should be done, get a list of the required validation layer names and make sure they exist
    // Validation layers should only be enabled on non-release builds
    if (context->validation_enabled)
    {
        BINFO("Validation layers enabled. Enumerating...");

        // The list of validation layers required
        required_validation_layer_names = darray_create(const char*);
        darray_push(required_validation_layer_names, &"VK_LAYER_KHRONOS_validation");
        required_validation_layer_count = darray_length(required_validation_layer_names);

        // Obtain a list of available validation layers
        u32 available_layer_count = 0;
        VK_CHECK(vkEnumerateInstanceLayerProperties(&available_layer_count, 0));
        VkLayerProperties* available_layers = darray_reserve(VkLayerProperties, available_layer_count);
        VK_CHECK(vkEnumerateInstanceLayerProperties(&available_layer_count, available_layers));

        // Verify all required layers are available
        for (u32 i = 0; i < required_validation_layer_count; ++i)
        {
            b8 found = false;
            for (u32 j = 0; j < available_layer_count; ++j)
            {
                if (strings_equal(required_validation_layer_names[i], available_layers[j].layerName))
                {
                    found = true;
                    BINFO("Found validation layer: %s...", required_validation_layer_names[i]);
                    break;
                }
            }

            if (!found)
            {
                BFATAL("Required validation layer is missing: %s", required_validation_layer_names[i]);
                return false;
            }
        }

        // Clean up
        darray_destroy(available_extensions);
        darray_destroy(available_layers);

        BINFO("All required validation layers are present");
    }
    else
    {
        BINFO("Vulkan validation layers are not enabled");
    }

    create_info.enabledLayerCount = required_validation_layer_count;
    create_info.ppEnabledLayerNames = required_validation_layer_names;

    VkResult instance_result = vkCreateInstance(&create_info, context->allocator, &context->instance);
    if (!vulkan_result_is_success(instance_result))
    {
        const char* result_string = vulkan_result_string(instance_result, true);
        BFATAL("Vulkan instance creation failed with result: '%s'", result_string);
        return false;
    }

    darray_destroy(required_extensions);

    BINFO("Vulkan instance created");

    // Clean up
    if (required_validation_layer_names)
        darray_destroy(required_validation_layer_names);

    // TODO: implement multithreading
    context->multithreading_enabled = false;

// Debugger
#if defined(_DEBUG)
    BDEBUG("Creating Vulkan debugger...");
    u32 log_severity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT |
                       VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                       VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
                       VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT;

    VkDebugUtilsMessengerCreateInfoEXT debug_create_info = {VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT};
    debug_create_info.messageSeverity = log_severity;
    debug_create_info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT;
    debug_create_info.pfnUserCallback = vk_debug_callback;

    PFN_vkCreateDebugUtilsMessengerEXT func =
        (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(context->instance, "vkCreateDebugUtilsMessengerEXT");
    BASSERT_MSG(func, "Failed to create debug messenger!");
    VK_CHECK(func(context->instance, &debug_create_info, context->allocator, &context->debug_messenger));
    BDEBUG("Vulkan debugger created");

    // Load up debug function pointers
    context->pfnSetDebugUtilsObjectNameEXT = (PFN_vkSetDebugUtilsObjectNameEXT)vkGetInstanceProcAddr(context->instance, "vkSetDebugUtilsObjectNameEXT");
    if (!context->pfnSetDebugUtilsObjectNameEXT)
        BWARN("Unable to load function pointer for vkSetDebugUtilsObjectNameEXT. Debug functions associated with this will not work");

    context->pfnSetDebugUtilsObjectTagEXT = (PFN_vkSetDebugUtilsObjectTagEXT)vkGetInstanceProcAddr(context->instance, "vkSetDebugUtilsObjectTagEXT");
    if (!context->pfnSetDebugUtilsObjectTagEXT)
        BWARN("Unable to load function pointer for vkSetDebugUtilsObjectTagEXT. Debug functions associated with this will not work");

    context->pfnCmdBeginDebugUtilsLabelEXT = (PFN_vkCmdBeginDebugUtilsLabelEXT)vkGetInstanceProcAddr(context->instance, "vkCmdBeginDebugUtilsLabelEXT");
    if (!context->pfnCmdBeginDebugUtilsLabelEXT)
        BWARN("Unable to load function pointer for vkCmdBeginDebugUtilsLabelEXT. Debug functions associated with this will not work");

    context->pfnCmdEndDebugUtilsLabelEXT = (PFN_vkCmdEndDebugUtilsLabelEXT)vkGetInstanceProcAddr(context->instance, "vkCmdEndDebugUtilsLabelEXT");
    if (!context->pfnCmdEndDebugUtilsLabelEXT)
        BWARN("Unable to load function pointer for vkCmdEndDebugUtilsLabelEXT. Debug functions associated with this will not work");
#endif

    // Device creation
    if (!vulkan_device_create(context))
    {
        BERROR("Failed to create device");
        return false;
    }

    // Samplers array
    context->samplers = darray_create(vulkan_sampler_handle_data);

    // Create a shader compiler
    context->shader_compiler = shaderc_compiler_initialize();

    BINFO("Vulkan renderer initialized successfully");
    return true;
}

void vulkan_renderer_backend_shutdown(renderer_backend_interface* backend)
{
    vulkan_context* context = (vulkan_context*)backend->internal_context;
    vkDeviceWaitIdle(context->device.logical_device);

    // Destroy runtime shader compiler
    if (context->shader_compiler)
    {
        shaderc_compiler_release(context->shader_compiler);
        context->shader_compiler = 0;
    }

    BDEBUG("Destroying Vulkan device...");
    vulkan_device_destroy(context);

    if (backend->internal_context)
    {
        bfree(backend->internal_context, backend->internal_context_size, MEMORY_TAG_RENDERER);
        backend->internal_context_size = 0;
        backend->internal_context = 0;
    }

#if defined(_DEBUG)
    BDEBUG("Destroying Vulkan debugger...");
    if (context->debug_messenger)
    {
        PFN_vkDestroyDebugUtilsMessengerEXT func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(context->instance, "vkDestroyDebugUtilsMessengerEXT");
        func(context->instance, context->debug_messenger, context->allocator);
    }
#endif

    BDEBUG("Destroying Vulkan instance...");
    vkDestroyInstance(context->instance, context->allocator);

    // Destroy allocator callbacks if set
    if (context->allocator)
    {
        bfree(context->allocator, sizeof(VkAllocationCallbacks), MEMORY_TAG_RENDERER);
        context->allocator = 0;
    }
}

b8 vulkan_renderer_on_window_created(renderer_backend_interface* backend, bwindow* window)
{
    BASSERT(backend && window);

    vulkan_context* context = (vulkan_context*)backend->internal_context;
    bwindow_renderer_state* window_internal = window->renderer_state;

    // Setup backend-specific state for the window
    window_internal->backend_state = ballocate(sizeof(bwindow_renderer_backend_state), MEMORY_TAG_RENDERER);
    bwindow_renderer_backend_state* window_backend = window_internal->backend_state;

    // Create the surface
    BDEBUG("Creating Vulkan surface for window '%s'...", window->name);
    if (!vulkan_platform_create_vulkan_surface(context, window))
    {
        BERROR("Failed to create platform surface for window '%s'!", window->name);
        return false;
    }
    BDEBUG("Vulkan surface created for window '%s'", window->name);

    // Create swapchain. This also handles colorbuffer creation
    if (!vulkan_swapchain_create(backend, window, context->flags, &window_backend->swapchain))
    {
        BERROR("Failed to create Vulkan swapchain during creation of window '%s'. See logs for details", window->name);
        return false;
    }

    // Re-detect supported device depth format
    if (!vulkan_device_detect_depth_format(&context->device))
    {
        context->device.depth_format = VK_FORMAT_UNDEFINED;
        BFATAL("Failed to find a supported format!");
        return false;
    }

    // Create per-frame-in-flight resources
    {
        u8 max_frames_in_flight = window_backend->swapchain.max_frames_in_flight;

        // Sync objects are owned by the window since they go hand-in-hand with the swapchain and window resources
        window_backend->image_available_semaphores = BALLOC_TYPE_CARRAY(VkSemaphore, max_frames_in_flight);
        window_backend->queue_complete_semaphores = BALLOC_TYPE_CARRAY(VkSemaphore, max_frames_in_flight);
        window_backend->in_flight_fences = BALLOC_TYPE_CARRAY(VkFence, max_frames_in_flight);

        window_backend->frame_texture_updated_list = BALLOC_TYPE_CARRAY(bhandle*, max_frames_in_flight);

        // The staging buffer also goes here since it is tied to the frame
        // TODO: Reduce this to a single buffer split by max_frames_in_flight
        const u64 staging_buffer_size = MEBIBYTES(768); // FIXME: Queue updates per frame in flight to shrink this down
        window_backend->staging = ballocate(sizeof(renderbuffer) * max_frames_in_flight, MEMORY_TAG_ARRAY);

        for (u8 i = 0; i < max_frames_in_flight; ++i)
        {
            VkSemaphoreCreateInfo semaphore_create_info = {VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
            vkCreateSemaphore(context->device.logical_device, &semaphore_create_info, context->allocator, &window_backend->image_available_semaphores[i]);
            vkCreateSemaphore(context->device.logical_device, &semaphore_create_info, context->allocator, &window_backend->queue_complete_semaphores[i]);

            // Create the fence in a signaled state, indicating that the first frame has
            // already been "rendered". This will prevent the application from waiting
            // indefinitely for the first frame to render since it cannot be rendered
            // until a frame is "rendered" before it
            VkFenceCreateInfo fence_create_info = {VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
            fence_create_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;
            VK_CHECK(vkCreateFence(context->device.logical_device, &fence_create_info, context->allocator, &window_backend->in_flight_fences[i]));

            // Staging buffer
            // TODO: Reduce this to a single buffer split by max_frames_in_flight
            if (!renderer_renderbuffer_create("staging", RENDERBUFFER_TYPE_STAGING, staging_buffer_size, RENDERBUFFER_TRACK_TYPE_LINEAR, &window_backend->staging[i]))
            {
                BERROR("Failed to create staging buffer");
                return false;
            }
            renderer_renderbuffer_bind(&window_backend->staging[i], 0);

            // Create the per-frame list of updated texture handles
            window_backend->frame_texture_updated_list[i] = darray_create(bhandle);
        }
    }

    // Create command buffers
    create_command_buffers(context, window);

    // Create the depthbuffer
    BDEBUG("Creating Vulkan depthbuffer for window '%s'...", window->name);
    if (bhandle_is_invalid(window_internal->depthbuffer->renderer_texture_handle))
    {
        // If invalid, then a new one needs to be created. This does not reach out to the
        // texture system to create this, but handles it internally instead. This is because
        // the process for this varies greatly between backends.
        if (!renderer_bresource_texture_resources_acquire(
                backend->frontend_state,
                bname_create(window->name),
                BRESOURCE_TEXTURE_TYPE_2D,
                window->width,
                window->height,
                4,
                1,
                1,
                // NOTE: This should be a wrapped texture, so the frontend does not try to acquire the resources we already have here
                // Also flag as a depth texture
                TEXTURE_FLAG_IS_WRAPPED | TEXTURE_FLAG_IS_WRITEABLE | TEXTURE_FLAG_RENDERER_BUFFERING | TEXTURE_FLAG_DEPTH,
                &window_internal->depthbuffer->renderer_texture_handle))
        {
            BFATAL("Failed to acquire internal texture resources for window.depthbuffer");
            return false;
        }
    }

    // Get the texture_internal_data based on the existing or newly-created handle above
    // Use that to setup the internal images/views for the colorbuffer texture
    vulkan_texture_handle_data* texture_data = &context->textures[window_internal->depthbuffer->renderer_texture_handle.handle_index];
    if (!texture_data)
    {
        BFATAL("Unable to get internal data for depthbuffer image. Window creation failed");
        return false;
    }

    // Name is meaningless here, but might be useful for debugging
    if (window_internal->depthbuffer->base.name == INVALID_BNAME)
        window_internal->depthbuffer->base.name = bname_create("__window_depthbuffer_texture__");

    texture_data->image_count = window_backend->swapchain.image_count;
    // Create the array if it doesn't exist
    if (!texture_data->images)
    {
        // Also have to setup the internal data
        texture_data->images = ballocate(sizeof(vulkan_image) * texture_data->image_count, MEMORY_TAG_TEXTURE);
    }

    // Update the parameters and setup a view for each image
    for (u32 i = 0; i < texture_data->image_count; ++i)
    {
        vulkan_image* image = &texture_data->images[i];

        // Construct a unique name for each image
        char* formatted_name = string_format("__window_%s_depth_stencil_texture_%u", window->name, i);

        // Create the actual backing image
        vulkan_image_create(
            context,
            BRESOURCE_TEXTURE_TYPE_2D,
            window->width,
            window->height,
            1,
            context->device.depth_format,
            VK_IMAGE_TILING_OPTIMAL,
            VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            true,
            VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT,
            formatted_name,
            1,
            image);

        string_free(formatted_name);

        // window->renderer_state->depthbuffer->format = context->device.depth_channel_count;

        // Setup a debug name for the image
        VK_SET_DEBUG_OBJECT_NAME(context, VK_OBJECT_TYPE_IMAGE, image->handle, image->name);
    }

    BINFO("Vulkan depthbuffer created successfully");

    // If there is not yet a current window, assign it now
    if (!context->current_window)
        context->current_window = window;

    return true;
}

void vulkan_renderer_on_window_destroyed(renderer_backend_interface* backend, bwindow* window)
{
    vulkan_context* context = (vulkan_context*)backend->internal_context;
    bwindow_renderer_state* window_internal = window->renderer_state;
    bwindow_renderer_backend_state* window_backend = window_internal->backend_state;

    u32 max_frames_in_flight = window_backend->swapchain.max_frames_in_flight;

    // Destroy per-frame-in-flight resources
    {
        for (u32 i = 0; i < window_backend->swapchain.max_frames_in_flight; ++i)
        {
            // Destroy staging buffers
            renderer_renderbuffer_destroy(&window_backend->staging[i]);

            // Sync objects
            if (window_backend->image_available_semaphores[i])
            {
                vkDestroySemaphore(context->device.logical_device, window_backend->image_available_semaphores[i], context->allocator);
                window_backend->image_available_semaphores[i] = 0;
            }
            if (window_backend->queue_complete_semaphores[i])
            {
                vkDestroySemaphore(context->device.logical_device, window_backend->queue_complete_semaphores[i], context->allocator);
                window_backend->queue_complete_semaphores[i] = 0;
            }

            vkDestroyFence(context->device.logical_device, window_backend->in_flight_fences[i], context->allocator);
        }
        bfree(window_backend->image_available_semaphores, sizeof(VkSemaphore) * max_frames_in_flight, MEMORY_TAG_ARRAY);
        window_backend->image_available_semaphores = 0;

        bfree(window_backend->queue_complete_semaphores, sizeof(VkSemaphore) * max_frames_in_flight, MEMORY_TAG_ARRAY);
        window_backend->queue_complete_semaphores = 0;

        bfree(window_backend->in_flight_fences, sizeof(VkFence) * max_frames_in_flight, MEMORY_TAG_ARRAY);
        window_backend->in_flight_fences = 0;

        bfree(window_backend->staging, sizeof(renderbuffer) * max_frames_in_flight, MEMORY_TAG_ARRAY);
        window_backend->staging = 0;
    }

    // Destroy per-swapchain-image resources
    {
        for (u32 i = 0; i < window_backend->swapchain.image_count; ++i)
        {
            // Command buffers
            if (window_backend->graphics_command_buffers[i].handle)
            {
                vulkan_command_buffer_free(context, context->device.graphics_command_pool, &window_backend->graphics_command_buffers[i]);
                window_backend->graphics_command_buffers[i].handle = 0;
            }
        }
        bfree(window_backend->graphics_command_buffers, sizeof(vulkan_command_buffer) * window_backend->swapchain.image_count, MEMORY_TAG_ARRAY);
        window_backend->graphics_command_buffers = 0;

        // Destroy depthbuffer images/views
        vulkan_texture_handle_data* texture_data = &context->textures[window_internal->depthbuffer->renderer_texture_handle.handle_index];
        if (!texture_data)
        {
            BWARN("Unable to get internal data for depthbuffer image. Underlying resources may not be properly destroyed");
        }
        else
        {
            // Free the name
            window_internal->depthbuffer->base.name = INVALID_BNAME;

            // Destroy each backing image
            if (texture_data->images)
            {
                for (u32 i = 0; i < texture_data->image_count; ++i)
                    vulkan_image_destroy(context, &texture_data->images[i]);
                // Free the internal data
                // bfree(texture_data->images, sizeof(vulkan_image) * texture_data->image_count, MEMORY_TAG_TEXTURE);
            }

            // Releasing the resources for the default depthbuffer should destroy backing resources too
            renderer_texture_resources_release(backend->frontend_state, &window->renderer_state->depthbuffer->renderer_texture_handle);
        }
    }

    // Swapchain
    BDEBUG("Destroying Vulkan swapchain for window '%s'...", window->name);
    vulkan_swapchain_destroy(backend, &window_backend->swapchain);

    BDEBUG("Destroying Vulkan surface for window '%s'...", window->name);
    if (window_backend->surface)
    {
        vkDestroySurfaceKHR(context->instance, window_backend->surface, context->allocator);
        window_backend->surface = 0;
    }

    // Free the backend state
    bfree(window_internal->backend_state, sizeof(bwindow_renderer_backend_state), MEMORY_TAG_RENDERER);
    window_internal->backend_state = 0;
}

void vulkan_renderer_backend_on_window_resized(renderer_backend_interface* backend, const bwindow* window)
{
    // vulkan_context* context = (vulkan_context*)backend->internal_context;
    bwindow_renderer_backend_state* backend_window = window->renderer_state->backend_state;
    // Update framebuffer size generation, a counter which indicates when the framebuffer size has been updated
    backend_window->framebuffer_size_generation++;

    BINFO("Vulkan renderer backend->resized: w/h/gen: %i/%i/%llu", window->width, window->height, backend_window->framebuffer_size_generation);
}

void vulkan_renderer_begin_debug_label(renderer_backend_interface* backend, const char* label_text, vec3 color)
{
#ifdef _DEBUG
    vulkan_context* context = (vulkan_context*)backend->internal_context;
    vulkan_command_buffer* command_buffer = get_current_command_buffer(context);
    vec4 rgba = (vec4){color.r, color.g, color.b, 1.0f};
#endif
    VK_BEGIN_DEBUG_LABEL(context, command_buffer->handle, label_text, rgba);
}

void vulkan_renderer_end_debug_label(renderer_backend_interface* backend)
{
#ifdef _DEBUG
    vulkan_context* context = (vulkan_context*)backend->internal_context;
    vulkan_command_buffer* command_buffer = get_current_command_buffer(context);
#endif
    VK_END_DEBUG_LABEL(context, command_buffer->handle);
}

b8 vulkan_renderer_frame_prepare(renderer_backend_interface* backend, struct frame_data* p_frame_data)
{
    // NOTE: this is an intentional no-op in this backend
    return true;
}

b8 vulkan_renderer_frame_prepare_window_surface(renderer_backend_interface* backend, struct bwindow* window, struct frame_data* p_frame_data)
{
    vulkan_context* context = (vulkan_context*)backend->internal_context;
    vulkan_device* device = &context->device;

    bwindow_renderer_state* window_internal = window->renderer_state;
    bwindow_renderer_backend_state* window_backend = window_internal->backend_state;

    // Check if recreating swap chain and boot out
    if (window_backend->recreating_swapchain)
    {
        VkResult result = vkDeviceWaitIdle(device->logical_device);
        if (!vulkan_result_is_success(result))
        {
            BERROR("vulkan_renderer_backend_frame_begin vkDeviceWaitIdle (1) failed: '%s'", vulkan_result_string(result, true));
            return false;
        }
        BINFO("Recreating swapchain, booting.");
        return false;
    }

    // Check if the framebuffer has been resized. If so, a new swapchain must be created.
    // Also include vsync changed check
    if (window_backend->framebuffer_size_generation != window_backend->framebuffer_previous_size_generation || context->render_flag_changed)
    {
        VkResult result = vkDeviceWaitIdle(device->logical_device);
        if (!vulkan_result_is_success(result))
        {
            BERROR("vulkan_renderer_backend_frame_begin vkDeviceWaitIdle (2) failed: '%s'", vulkan_result_string(result, true));
            return false;
        }

        if (context->render_flag_changed)
            context->render_flag_changed = false;

        // If the swapchain recreation failed (for example the window was minimized) boot out before unsetting the flag
        if (window_backend->skip_frames == 0)
        {
            if (!recreate_swapchain(backend, window))
                return false;
        }

        window_backend->skip_frames++;

        // Resize depth buffer image
        if (window_backend->skip_frames == window_backend->swapchain.max_frames_in_flight)
        {
            if (!bhandle_is_invalid(window->renderer_state->depthbuffer->renderer_texture_handle))
            {
                /* vkQueueWaitIdle(context->device.graphics_queue); */
                if (!renderer_texture_resize(backend->frontend_state, window->renderer_state->depthbuffer->renderer_texture_handle, window->width, window->height))
                    BERROR("Failed to resize depth buffer for window '%s'. See logs for details", window->name);
            }
            // Sync the framebuffer size generation
            window_backend->framebuffer_previous_size_generation = window_backend->framebuffer_size_generation;

            window_backend->skip_frames = 0;
        }

        BINFO("Resized, booting...");
        return false;
    }

    // Wait for the execution of the current frame to complete. The fence being free will allow this one to move on
    VkResult result = vkWaitForFences(context->device.logical_device, 1, &window_backend->in_flight_fences[window_backend->current_frame], true, U64_MAX);
    if (!vulkan_result_is_success(result))
    {
        BFATAL("In-flight fence wait failure! error: %s", vulkan_result_string(result, true));
        return false;
    }

    // Increment textures in list of handles updated within frame workload
    bhandle* updated_textures = context->current_window->renderer_state->backend_state->frame_texture_updated_list[window_backend->current_frame];
    u32 updated_texture_count = 0;
    for (u32 i = 0; i < updated_texture_count; ++i)
        context->textures[updated_textures[i].handle_index].generation++;
    // Clear the list
    darray_clear(updated_textures);

    result = vkAcquireNextImageKHR(
        context->device.logical_device,
        window_backend->swapchain.handle,
        U64_MAX,
        window_backend->image_available_semaphores[window_backend->current_frame],
        0,
        &window_backend->image_index);

    if (result == VK_ERROR_OUT_OF_DATE_KHR)
    {
        // Trigger swapchain recreation, then boot out of the render loop
        if (!vulkan_swapchain_recreate(backend, window, &window_backend->swapchain))
            BFATAL("Failed to recreate swapchain");
        return false;
    }
    else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
    {
        BFATAL("Failed to acquire swapchain image");
        return false;
    }

    // Reset fence for use on the next frame
    VK_CHECK(vkResetFences(context->device.logical_device, 1, &window_backend->in_flight_fences[window_backend->current_frame]));

    // Reset staging buffer
    if (!renderer_renderbuffer_clear(&window_backend->staging[window_backend->current_frame], false))
    {
        BERROR("Failed to clear staging buffer");
        return false;
    }

    return true;
}

b8 vulkan_renderer_frame_command_list_begin(renderer_backend_interface* backend, struct frame_data* p_frame_data)
{
    vulkan_context* context = (vulkan_context*)backend->internal_context;

    // Begin recording commands
    vulkan_command_buffer* command_buffer = get_current_command_buffer(context);

    vulkan_command_buffer_reset(command_buffer);
    vulkan_command_buffer_begin(command_buffer, false, false, false);

    return true;
}

b8 vulkan_renderer_frame_command_list_end(renderer_backend_interface* backend, struct frame_data* p_frame_data)
{
    vulkan_context* context = (vulkan_context*)backend->internal_context;
    // bwindow_renderer_backend_state* window_backend = context->current_window->renderer_state->backend_state;
    vulkan_command_buffer* command_buffer = get_current_command_buffer(context);

    // Just end the command buffer
    vulkan_command_buffer_end(command_buffer);

    return true;
}

b8 vulkan_renderer_frame_submit(struct renderer_backend_interface* backend, struct frame_data* p_frame_data)
{
    vulkan_context* context = (vulkan_context*)backend->internal_context;
    bwindow_renderer_backend_state* window_backend = context->current_window->renderer_state->backend_state;
    vulkan_command_buffer* command_buffer = get_current_command_buffer(context);

    // Submit the queue and wait for the operation to complete
    // Begin queue submission
    VkSubmitInfo submit_info = {VK_STRUCTURE_TYPE_SUBMIT_INFO};

    // Command buffer(s) to be executed
    submit_info.commandBufferCount = 1;
    // Update the state of the secondary buffers
    for (u32 i = 0; i < command_buffer->secondary_count; ++i)
    {
        vulkan_command_buffer* secondary = &command_buffer->secondary_buffers[i];
        if (secondary->state == COMMAND_BUFFER_STATE_RECORDING_ENDED)
            secondary->state = COMMAND_BUFFER_STATE_SUBMITTED;
    }
    submit_info.pCommandBuffers = &command_buffer->handle;

    // The semaphore(s) to be signaled when the queue is complete
    submit_info.signalSemaphoreCount = 1;
    submit_info.pSignalSemaphores = &window_backend->queue_complete_semaphores[window_backend->current_frame];

    // Wait semaphore ensures that the operation cannot begin until the image is available
    submit_info.waitSemaphoreCount = 1;
    submit_info.pWaitSemaphores = &window_backend->image_available_semaphores[window_backend->current_frame];

    // Each semaphore waits on the corresponding pipeline stage to complete. 1:1 ratio
    VkPipelineStageFlags flags[1] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    submit_info.pWaitDstStageMask = flags;

    VkResult result = vkQueueSubmit(
        context->device.graphics_queue,
        1,
        &submit_info,
        window_backend->in_flight_fences[window_backend->current_frame]);
    if (result != VK_SUCCESS)
    {
        BERROR("vkQueueSubmit failed with result: %s", vulkan_result_string(result, true));
        return false;
    }

    vulkan_command_buffer_update_submitted(command_buffer);

    // Loop back to the first index
    command_buffer->secondary_buffer_index = 0;
    // End queue submission

    return true;
}

b8 vulkan_renderer_frame_present(renderer_backend_interface* backend, struct bwindow* window, struct frame_data* p_frame_data)
{
    // Cold-cast the context
    vulkan_context* context = (vulkan_context*)backend->internal_context;
    /* bwindow_renderer_backend_state* window_backend = context->current_window->renderer_state->backend_state; */
    bwindow_renderer_backend_state* window_backend = window->renderer_state->backend_state;

    // Return the image to the swapchain for presentation
    VkPresentInfoKHR present_info = {VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
    present_info.waitSemaphoreCount = 1;
    present_info.pWaitSemaphores = &window_backend->queue_complete_semaphores[window_backend->current_frame];
    present_info.swapchainCount = 1;
    present_info.pSwapchains = &window_backend->swapchain.handle;
    present_info.pImageIndices = &window_backend->image_index;
    present_info.pResults = 0;

    VkResult result = vkQueuePresentKHR(context->device.present_queue, &present_info);
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR)
    {
        // Swapchain is out of date, suboptimal or a framebuffer resize has occurred. Trigger swapchain recreation
        if (!vulkan_swapchain_recreate(backend, window, &window_backend->swapchain))
            BFATAL("Failed to recreate swapchain after presentation");

        BDEBUG("Swapchain recreated because swapchain returned out of date or suboptimal");
    }
    else if (result != VK_SUCCESS)
    {
        BFATAL("Failed to present swap chain image");
    }

    // Increment (and loop) the index
    window_backend->current_frame = (window_backend->current_frame + 1) % window_backend->swapchain.max_frames_in_flight;

    return true;
}

void vulkan_renderer_viewport_set(renderer_backend_interface* backend, vec4 rect)
{
    vulkan_context* context = (vulkan_context*)backend->internal_context;
    // Dynamic state
    VkViewport viewport;
    viewport.x = rect.x;
    viewport.y = rect.y;
    viewport.width = rect.z;
    viewport.height = rect.w;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    vulkan_command_buffer* command_buffer = get_current_command_buffer(context);

    vkCmdSetViewport(command_buffer->handle, 0, 1, &viewport);
}

void vulkan_renderer_viewport_reset(renderer_backend_interface* backend)
{
    vulkan_context* context = (vulkan_context*)backend->internal_context;
    vulkan_renderer_viewport_set(backend, context->viewport_rect);
}

void vulkan_renderer_scissor_set(renderer_backend_interface* backend, vec4 rect)
{
    vulkan_context* context = (vulkan_context*)backend->internal_context;
    VkRect2D scissor;
    scissor.offset.x = rect.x;
    scissor.offset.y = rect.y;
    scissor.extent.width = rect.z;
    scissor.extent.height = rect.w;

    vulkan_command_buffer* command_buffer = get_current_command_buffer(context);

    vkCmdSetScissor(command_buffer->handle, 0, 1, &scissor);
}

void vulkan_renderer_scissor_reset(renderer_backend_interface* backend)
{
    vulkan_context* context = (vulkan_context*)backend->internal_context;
    vulkan_renderer_scissor_set(backend, context->scissor_rect);
}

void vulkan_renderer_winding_set(struct renderer_backend_interface* backend, renderer_winding winding)
{
    vulkan_context* context = (vulkan_context*)backend->internal_context;
    vulkan_command_buffer* command_buffer = get_current_command_buffer(context);

    VkFrontFace vk_winding = winding == RENDERER_WINDING_COUNTER_CLOCKWISE ? VK_FRONT_FACE_COUNTER_CLOCKWISE : VK_FRONT_FACE_CLOCKWISE;
    if (context->device.support_flags & VULKAN_DEVICE_SUPPORT_FLAG_NATIVE_DYNAMIC_STATE_BIT)
        vkCmdSetFrontFace(command_buffer->handle, vk_winding);
    else if (context->device.support_flags & VULKAN_DEVICE_SUPPORT_FLAG_DYNAMIC_STATE_BIT)
        context->vkCmdSetFrontFaceEXT(command_buffer->handle, vk_winding);
    else
        BFATAL("renderer_winding_set cannot be used on a device without dynamic state support");
}

static VkStencilOp vulkan_renderer_get_stencil_op(renderer_stencil_op op)
{
    switch (op)
    {
        case RENDERER_STENCIL_OP_KEEP:
            return VK_STENCIL_OP_KEEP;
        case RENDERER_STENCIL_OP_ZERO:
            return VK_STENCIL_OP_ZERO;
        case RENDERER_STENCIL_OP_REPLACE:
            return VK_STENCIL_OP_REPLACE;
        case RENDERER_STENCIL_OP_INCREMENT_AND_CLAMP:
            return VK_STENCIL_OP_INCREMENT_AND_CLAMP;
        case RENDERER_STENCIL_OP_DECREMENT_AND_CLAMP:
            return VK_STENCIL_OP_DECREMENT_AND_CLAMP;
        case RENDERER_STENCIL_OP_INCREMENT_AND_WRAP:
            return VK_STENCIL_OP_DECREMENT_AND_WRAP;
        case RENDERER_STENCIL_OP_DECREMENT_AND_WRAP:
            return VK_STENCIL_OP_DECREMENT_AND_WRAP;
        case RENDERER_STENCIL_OP_INVERT:
            return VK_STENCIL_OP_INVERT;
        default:
            BWARN("Unsupported stencil op, defaulting to keep");
            return VK_STENCIL_OP_KEEP;
    }
}

static VkCompareOp vulkan_renderer_get_compare_op(renderer_compare_op op)
{
    switch (op)
    {
        case RENDERER_COMPARE_OP_NEVER:
            return VK_COMPARE_OP_NEVER;
        case RENDERER_COMPARE_OP_LESS:
            return VK_COMPARE_OP_LESS;
        case RENDERER_COMPARE_OP_EQUAL:
            return VK_COMPARE_OP_EQUAL;
        case RENDERER_COMPARE_OP_LESS_OR_EQUAL:
            return VK_COMPARE_OP_LESS_OR_EQUAL;
        case RENDERER_COMPARE_OP_GREATER:
            return VK_COMPARE_OP_GREATER;
        case RENDERER_COMPARE_OP_NOT_EQUAL:
            return VK_COMPARE_OP_NOT_EQUAL;
        case RENDERER_COMPARE_OP_GREATER_OR_EQUAL:
            return VK_COMPARE_OP_GREATER_OR_EQUAL;
        case RENDERER_COMPARE_OP_ALWAYS:
            return VK_COMPARE_OP_ALWAYS;
        default:
            BWARN("Unsupported compare op, using always");
            return VK_COMPARE_OP_ALWAYS;
    }
}

void vulkan_renderer_set_stencil_test_enabled(struct renderer_backend_interface* backend, b8 enabled)
{
    vulkan_context* context = (vulkan_context*)backend->internal_context;
    vulkan_command_buffer* command_buffer = get_current_command_buffer(context);

    if (context->device.support_flags & VULKAN_DEVICE_SUPPORT_FLAG_NATIVE_DYNAMIC_STATE_BIT)
        vkCmdSetStencilTestEnable(command_buffer->handle, (VkBool32)enabled);
    else if (context->device.support_flags & VULKAN_DEVICE_SUPPORT_FLAG_DYNAMIC_STATE_BIT)
        context->vkCmdSetStencilTestEnableEXT(command_buffer->handle, (VkBool32)enabled);
    else
        BFATAL("renderer_set_stencil_test_enabled cannot be used on a device without dynamic state support");
}

void vulkan_renderer_set_depth_test_enabled(struct renderer_backend_interface* backend, b8 enabled)
{
    vulkan_context* context = (vulkan_context*)backend->internal_context;
    vulkan_command_buffer* command_buffer = get_current_command_buffer(context);

    if (context->device.support_flags & VULKAN_DEVICE_SUPPORT_FLAG_NATIVE_DYNAMIC_STATE_BIT)
        vkCmdSetDepthTestEnable(command_buffer->handle, (VkBool32)enabled);
    else if (context->device.support_flags & VULKAN_DEVICE_SUPPORT_FLAG_DYNAMIC_STATE_BIT)
        context->vkCmdSetDepthTestEnableEXT(command_buffer->handle, (VkBool32)enabled);
    else
        BFATAL("renderer_set_depth_test_enabled cannot be used on a device without dynamic state support");
}

void vulkan_renderer_set_depth_write_enabled(struct renderer_backend_interface* backend, b8 enabled)
{
    vulkan_context* context = (vulkan_context*)backend->internal_context;
    vulkan_command_buffer* command_buffer = get_current_command_buffer(context);

    if (context->device.support_flags & VULKAN_DEVICE_SUPPORT_FLAG_NATIVE_DYNAMIC_STATE_BIT)
        vkCmdSetDepthWriteEnable(command_buffer->handle, (VkBool32)enabled);
    else if (context->device.support_flags & VULKAN_DEVICE_SUPPORT_FLAG_DYNAMIC_STATE_BIT)
        context->vkCmdSetDepthWriteEnableEXT(command_buffer->handle, (VkBool32)enabled);
    else
        BFATAL("renderer_set_depth_write_enabled cannot be used on a device without dynamic state support");
}

void vulkan_renderer_set_stencil_reference(struct renderer_backend_interface* backend, u32 reference)
{
    vulkan_context* context = (vulkan_context*)backend->internal_context;
    vulkan_command_buffer* command_buffer = get_current_command_buffer(context);

    vkCmdSetStencilReference(command_buffer->handle, VK_STENCIL_FACE_FRONT_AND_BACK, reference);
}

void vulkan_renderer_set_stencil_op(struct renderer_backend_interface* backend, renderer_stencil_op fail_op, renderer_stencil_op pass_op, renderer_stencil_op depth_fail_op, renderer_compare_op compare_op)
{
    vulkan_context* context = (vulkan_context*)backend->internal_context;
    vulkan_command_buffer* command_buffer = get_current_command_buffer(context);

    if (context->device.support_flags & VULKAN_DEVICE_SUPPORT_FLAG_NATIVE_DYNAMIC_STATE_BIT)
    {
        vkCmdSetStencilOp(
            command_buffer->handle,
            VK_STENCIL_FACE_FRONT_AND_BACK,
            vulkan_renderer_get_stencil_op(fail_op),
            vulkan_renderer_get_stencil_op(pass_op),
            vulkan_renderer_get_stencil_op(depth_fail_op),
            vulkan_renderer_get_compare_op(compare_op));
    }
    else if (context->device.support_flags & VULKAN_DEVICE_SUPPORT_FLAG_DYNAMIC_STATE_BIT)
    {
        context->vkCmdSetStencilOpEXT(
            command_buffer->handle,
            VK_STENCIL_FACE_FRONT_AND_BACK,
            vulkan_renderer_get_stencil_op(fail_op),
            vulkan_renderer_get_stencil_op(pass_op),
            vulkan_renderer_get_stencil_op(depth_fail_op),
            vulkan_renderer_get_compare_op(compare_op));
    }
    else
    {
        BFATAL("renderer_set_stencil_op cannot be used on a device without dynamic state support");
    }
}

void vulkan_renderer_begin_rendering(struct renderer_backend_interface* backend, frame_data* p_frame_data, rect_2d render_area, u32 color_target_count, bhandle* color_targets, bhandle depth_stencil_target, u32 depth_stencil_layer)
{
    vulkan_context* context = (vulkan_context*)backend->internal_context;
    vulkan_command_buffer* primary = get_current_command_buffer(context);
    u32 image_index = context->current_window->renderer_state->backend_state->image_index;

    // Anytime we "begin" a render, update the "in-render" state and get the appropriate secondary
    primary->in_render = true;
    vulkan_command_buffer* secondary = get_current_command_buffer(context);
    vulkan_command_buffer_begin(secondary, false, false, false);
    VkRenderingInfo render_info = {VK_STRUCTURE_TYPE_RENDERING_INFO};
    render_info.renderArea.offset.x = render_area.x;
    render_info.renderArea.offset.y = render_area.y;
    render_info.renderArea.extent.width = render_area.width;
    render_info.renderArea.extent.height = render_area.height;

    // TODO: This may be a problem for layered images/cubemaps
    render_info.layerCount = 1;

    // Depth
    VkRenderingAttachmentInfoKHR depth_attachment_info = {VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
    if (!bhandle_is_invalid(depth_stencil_target))
    {
        vulkan_texture_handle_data* depth_stencil_data = &context->textures[depth_stencil_target.handle_index];
        vulkan_image* image = &depth_stencil_data->images[image_index];

        depth_attachment_info.imageView = image->view;
        if (image->layer_count > 1)
            depth_attachment_info.imageView = image->layer_views[depth_stencil_layer];
        depth_attachment_info.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        depth_attachment_info.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;     // Always load
        depth_attachment_info.storeOp = VK_ATTACHMENT_STORE_OP_STORE;  // Always store
        depth_attachment_info.resolveMode = VK_RESOLVE_MODE_NONE;
        depth_attachment_info.resolveImageView = 0;
        render_info.pDepthAttachment = &depth_attachment_info;
        render_info.pStencilAttachment = &depth_attachment_info;
    }
    else
    {
        render_info.pDepthAttachment = 0;
        render_info.pStencilAttachment = 0;
    }

    render_info.colorAttachmentCount = color_target_count;
    if (color_target_count)
    {
        // NOTE: this memory won't be leaked because it uses the frame allocator, which is reset per frame
        VkRenderingAttachmentInfo* color_attachments = p_frame_data->allocator.allocate(sizeof(VkRenderingAttachmentInfo) * color_target_count);
        for (u32 i = 0; i < color_target_count; ++i)
        {
            vulkan_texture_handle_data* color_target_data = &context->textures[color_targets[i].handle_index];
            VkRenderingAttachmentInfo* attachment_info = &color_attachments[i];
            attachment_info->sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
            attachment_info->imageView = color_target_data->images[image_index].view;
            attachment_info->imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            attachment_info->loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;    // Always load.
            attachment_info->storeOp = VK_ATTACHMENT_STORE_OP_STORE; // Always store
            bzero_memory(attachment_info->clearValue.color.float32, sizeof(f32) * 4);
            attachment_info->resolveMode = VK_RESOLVE_MODE_NONE;
            attachment_info->resolveImageView = 0;
            attachment_info->resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            attachment_info->pNext = 0;
        }
        render_info.pColorAttachments = color_attachments;
    }
    else
    {
        render_info.pColorAttachments = 0;
    }

    // Kick off the render using the secondary buffer
    if (context->device.support_flags & VULKAN_DEVICE_SUPPORT_FLAG_NATIVE_DYNAMIC_STATE_BIT)
        vkCmdBeginRendering(secondary->handle, &render_info);
    else
        context->vkCmdBeginRenderingKHR(secondary->handle, &render_info);
}

void vulkan_renderer_end_rendering(struct renderer_backend_interface* backend, frame_data* p_frame_data)
{
    vulkan_context* context = (vulkan_context*)backend->internal_context;
    // Since ending a rendering, will be in a secondary buffer
    vulkan_command_buffer* secondary = get_current_command_buffer(context);
    vulkan_command_buffer* primary = secondary->parent;

    if (context->device.support_flags & VULKAN_DEVICE_SUPPORT_FLAG_NATIVE_DYNAMIC_STATE_BIT)
        vkCmdEndRendering(secondary->handle);
    else
        context->vkCmdEndRenderingKHR(secondary->handle);
    
    // End the secondary buffer
    vulkan_command_buffer_end(secondary);
    // Execute the secondary command buffer via the primary buffer
    vkCmdExecuteCommands(primary->handle, 1, &secondary->handle);

    // Move on to the next buffer index
    primary->secondary_buffer_index++;
    primary->in_render = false;
}

void vulkan_renderer_set_stencil_compare_mask(struct renderer_backend_interface* backend, u32 compare_mask)
{
    vulkan_context* context = (vulkan_context*)backend->internal_context;
    vulkan_command_buffer* command_buffer = get_current_command_buffer(context);

    // Supported as of vulkan 1.0, so no need to check for dynamic state support
    vkCmdSetStencilCompareMask(command_buffer->handle, VK_STENCIL_FACE_FRONT_AND_BACK, compare_mask);
}

void vulkan_renderer_set_stencil_write_mask(struct renderer_backend_interface* backend, u32 write_mask)
{
    vulkan_context* context = (vulkan_context*)backend->internal_context;
    vulkan_command_buffer* command_buffer = get_current_command_buffer(context);

    // Supported as of vulkan 1.0, so no need to check for dynamic state support
    vkCmdSetStencilWriteMask(command_buffer->handle, VK_STENCIL_FACE_FRONT_AND_BACK, write_mask);
}

void vulkan_renderer_clear_color_set(renderer_backend_interface* backend, vec4 color)
{
    vulkan_context* context = (vulkan_context*)backend->internal_context;

    // Clamp values
    for (u8 i = 0; i < 4; ++i)
        color.elements[i] = BCLAMP(color.elements[i], 0.0f, 1.0f);

    // Cache the clear color for the next color clear operation
    bcopy_memory(context->color_clear_value.float32, color.elements, sizeof(f32) * 4);
}

void vulkan_renderer_clear_depth_set(renderer_backend_interface* backend, f32 depth)
{
    vulkan_context* context = (vulkan_context*)backend->internal_context;

    // Ensure the value is blamped
    depth = BCLAMP(depth, 0.0f, 1.0f);
    // Cache the depth for the next depth clear operation
    context->depth_stencil_clear_value.depth = depth;
}

void vulkan_renderer_clear_stencil_set(renderer_backend_interface* backend, u32 stencil)
{
    vulkan_context* context = (vulkan_context*)backend->internal_context;
    // Cache the depth for the next stencil clear operation
    context->depth_stencil_clear_value.stencil = stencil;
}

void vulkan_renderer_clear_color_texture(renderer_backend_interface* backend, bhandle renderer_texture_handle)
{
    // Cold-cast the context
    vulkan_context* context = (vulkan_context*)backend->internal_context;
    vulkan_command_buffer* command_buffer = get_current_command_buffer(context);
    vulkan_texture_handle_data* tex_internal = &context->textures[renderer_texture_handle.handle_index];

    // If a per-frame texture, get the appropriate image index. Otherwise it's just the first one
    vulkan_image* image = tex_internal->image_count == 1 ? &tex_internal->images[0] : &tex_internal->images[get_current_image_index(context)];

    // Transition the layout
    VkImageMemoryBarrier barrier = {0};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.srcQueueFamilyIndex = context->device.graphics_queue_index;
    barrier.dstQueueFamilyIndex = context->device.graphics_queue_index;
    barrier.image = image->handle;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    // Mips
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = image->mip_levels;

    // Transition all layers at once
    barrier.subresourceRange.layerCount = image->layer_count;

    // Start at the first layer
    barrier.subresourceRange.baseArrayLayer = 0;

    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

    vkCmdPipelineBarrier(
        command_buffer->handle,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        0,
        0, 0,
        0, 0,
        1, &barrier);

    // Clear the image
    vkCmdClearColorImage(
        command_buffer->handle,
        image->handle,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        &context->color_clear_value,
        image->layer_count,
        image->layer_count == 1 ? &image->view_subresource_range : image->layer_view_subresource_ranges);
}

void vulkan_renderer_clear_depth_stencil(renderer_backend_interface* backend, bhandle renderer_texture_handle)
{
    // Cold-cast the context
    vulkan_context* context = (vulkan_context*)backend->internal_context;
    vulkan_command_buffer* command_buffer = get_current_command_buffer(context);

    vulkan_texture_handle_data* tex_internal = &context->textures[renderer_texture_handle.handle_index];

    // If a per-frame texture, get the appropriate image index. Otherwise it's just the first one
    vulkan_image* image = tex_internal->image_count == 1 ? &tex_internal->images[0] : &tex_internal->images[get_current_image_index(context)];

    // Transition the layout
    VkImageMemoryBarrier barrier = {0};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.srcQueueFamilyIndex = context->device.graphics_queue_index;
    barrier.dstQueueFamilyIndex = context->device.graphics_queue_index;
    barrier.image = image->handle;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
    // Mips
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = image->mip_levels;

    // Transition all layers at once
    barrier.subresourceRange.layerCount = image->layer_count;

    // Start at the first layer
    barrier.subresourceRange.baseArrayLayer = 0;

    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

    vkCmdPipelineBarrier(
        command_buffer->handle,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        0,
        0, 0,
        0, 0,
        1, &barrier);

    // Clear the image
    vkCmdClearDepthStencilImage(
        command_buffer->handle,
        image->handle,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        &context->depth_stencil_clear_value,
        image->layer_count,
        image->layer_count == 1 ? &image->view_subresource_range : image->layer_view_subresource_ranges);
}

void vulkan_renderer_color_texture_prepare_for_present(renderer_backend_interface* backend, bhandle renderer_texture_handle)
{
    // Cold-cast the context
    vulkan_context* context = (vulkan_context*)backend->internal_context;
    vulkan_command_buffer* command_buffer = get_current_command_buffer(context);

    vulkan_texture_handle_data* tex_internal = &context->textures[renderer_texture_handle.handle_index];

    // If a per-frame texture, get the appropriate image index. Otherwise it's just the first one
    vulkan_image* image = tex_internal->image_count == 1 ? &tex_internal->images[0] : &tex_internal->images[get_current_image_index(context)];

    // Transition the layout
    VkImageMemoryBarrier barrier = {0};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    barrier.srcQueueFamilyIndex = context->device.graphics_queue_index;
    barrier.dstQueueFamilyIndex = context->device.graphics_queue_index;
    barrier.image = image->handle;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    // Mips
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = image->mip_levels;

    // Transition all layers at once
    barrier.subresourceRange.layerCount = image->layer_count;

    // Start at the first layer
    barrier.subresourceRange.baseArrayLayer = 0;

    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;

    vkCmdPipelineBarrier(
        command_buffer->handle,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        0,
        0, 0,
        0, 0,
        1, &barrier);
}

void vulkan_renderer_texture_prepare_for_sampling(renderer_backend_interface* backend, bhandle renderer_texture_handle, texture_flag_bits flags)
{
    // Cold-cast the context
    vulkan_context* context = (vulkan_context*)backend->internal_context;
    vulkan_command_buffer* command_buffer = get_current_command_buffer(context);

    vulkan_texture_handle_data* tex_internal = &context->textures[renderer_texture_handle.handle_index];

    // If a per-frame texture, get the appropriate image index. Otherwise it's just the first one
    vulkan_image* image = tex_internal->image_count == 1 ? &tex_internal->images[0] : &tex_internal->images[get_current_image_index(context)];

    b8 is_depth = (flags & TEXTURE_FLAG_DEPTH) != 0;

    // Transition the layout
    VkImageMemoryBarrier barrier = {0};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcQueueFamilyIndex = context->device.graphics_queue_index;
    barrier.dstQueueFamilyIndex = context->device.graphics_queue_index;
    barrier.image = image->handle;
    barrier.subresourceRange.aspectMask = is_depth ? (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT) : VK_IMAGE_ASPECT_COLOR_BIT;
    // Mips
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = image->mip_levels;

    // Transition all layers at once
    barrier.subresourceRange.layerCount = image->layer_count;

    // Start at the first layer
    barrier.subresourceRange.baseArrayLayer = 0;

    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_TRANSFER_READ_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | (is_depth ? VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT : VK_ACCESS_COLOR_ATTACHMENT_READ_BIT);

    vkCmdPipelineBarrier(
        command_buffer->handle,
        VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,     // VK_PIPELINE_STAGE_TRANSFER_BIT
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,  // VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
        0,
        0, 0,
        0, 0,
        1, &barrier);
}

VKAPI_ATTR VkBool32 VKAPI_CALL vk_debug_callback(
    VkDebugUtilsMessageSeverityFlagBitsEXT message_severity,
    VkDebugUtilsMessageTypeFlagsEXT message_types,
    const VkDebugUtilsMessengerCallbackDataEXT* callback_data,
    void* user_data)
{
    switch (message_severity)
    {
        default:
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
            BERROR(callback_data->pMessage);
            break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
            BWARN(callback_data->pMessage);
            break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
            BINFO(callback_data->pMessage);
            break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
            BTRACE(callback_data->pMessage);
            break;
    }
    return VK_FALSE;
}

static i32 find_memory_index(vulkan_context* context, u32 type_filter, u32 property_flags)
{
    VkPhysicalDeviceMemoryProperties memory_properties;
    vkGetPhysicalDeviceMemoryProperties(context->device.physical_device, &memory_properties);

    for (u32 i = 0; i < memory_properties.memoryTypeCount; ++i)
    {
        // Check each memory type to see if its bit is set to 1.
        if (type_filter & (1 << i) && (memory_properties.memoryTypes[i].propertyFlags & property_flags) == property_flags)
            return i;
    }

    BWARN("Unable to find suitable memory type");
    return -1;
}

static void create_command_buffers(vulkan_context* context, bwindow* window)
{
    bwindow_renderer_backend_state* window_backend = window->renderer_state->backend_state;

    // Create new command buffers according to the new swapchain image count
    u32 new_image_count = window_backend->swapchain.image_count;
    window_backend->graphics_command_buffers = ballocate(sizeof(vulkan_command_buffer) * new_image_count, MEMORY_TAG_ARRAY);
    for (u32 i = 0; i < new_image_count; ++i)
    {
        vulkan_command_buffer* primary_buffer = &window_backend->graphics_command_buffers[i];
        bzero_memory(primary_buffer, sizeof(vulkan_command_buffer));

        // Allocate a new buffer
        char* name = string_format("%s_command_buffer_%d", window->name, i);
        vulkan_command_buffer_allocate(context, context->device.graphics_command_pool, true, name, primary_buffer);
        string_free(name);

        // Allocate new secondary command buffers
        // TODO: should this be configurable
        primary_buffer->secondary_count = 16;
        primary_buffer->secondary_buffers = ballocate(sizeof(vulkan_command_buffer) * primary_buffer->secondary_count, MEMORY_TAG_ARRAY);
        for (u32 j = 0; j < primary_buffer->secondary_count; ++j)
        {
            vulkan_command_buffer* secondary_buffer = &primary_buffer->secondary_buffers[j];
            char* secondary_name = string_format("%s_command_buffer_%d_secondary_%d", window->name, i, j);
            vulkan_command_buffer_allocate(context, context->device.graphics_command_pool, false, secondary_name, secondary_buffer);
            string_free(secondary_name);
            // Set the primary buffer pointer
            secondary_buffer->parent = primary_buffer;
        }

        primary_buffer->secondary_buffer_index = 0; // Start at the first secondary buffer
        primary_buffer->in_render = false;          // start off as "not in render"
    }

    BDEBUG("Vulkan command buffers created");
}

static b8 recreate_swapchain(renderer_backend_interface* backend, bwindow* window)
{
    vulkan_context* context = backend->internal_context;
    bwindow_renderer_state* window_internal = window->renderer_state;
    bwindow_renderer_backend_state* window_backend = window_internal->backend_state;

    // If already being recreated, do not try again
    if (window_backend->recreating_swapchain)
    {
        BDEBUG("recreate_swapchain called when already recreating. Booting...");
        return false;
    }

    // Detect if the window is too small to be drawn to
    if (window->width == 0 || window->height == 0)
    {
        BDEBUG("recreate_swapchain called when window is < 1 in a dimension. Booting...");
        return false;
    }

    // Mark as recreating if the dimensions are valid
    window_backend->recreating_swapchain = true;

    // Use the old swapchain count to free swapchain-image-count related items
    u32 old_swapchain_image_count = window_backend->swapchain.image_count;

    // Wait for any operations to complete
    vkDeviceWaitIdle(context->device.logical_device);

    // Redetect the depth format
    vulkan_device_detect_depth_format(&context->device);

    // Recreate the swapchain
    if (!vulkan_swapchain_recreate(backend, window, &window_backend->swapchain))
    {
        BERROR("Failed to recreate swapchain. See logs for details");
        return false;
    }

    // Free old command buffers
    if (window_backend->graphics_command_buffers)
    {
        // Free the old command buffers first. Use the old image count for this, if it changed
        for (u32 i = 0; i < old_swapchain_image_count; ++i)
        {
            if (window_backend->graphics_command_buffers[i].handle)
                vulkan_command_buffer_free(context, context->device.graphics_command_pool, &window_backend->graphics_command_buffers[i]);
        }

        bfree(window_backend->graphics_command_buffers, sizeof(vulkan_command_buffer) * old_swapchain_image_count, MEMORY_TAG_ARRAY);
        window_backend->graphics_command_buffers = 0;
    }

    // Indicate to listeners that render target refresh is required
    event_fire(EVENT_CODE_DEFAULT_RENDERTARGET_REFRESH_REQUIRED, 0, (event_context){0});

    create_command_buffers(context, window);

    // Clear recreating flag
    window_backend->recreating_swapchain = false;

    return true;
}

static VkFormat channel_count_to_format(u8 channel_count, VkFormat default_format)
{
    switch (channel_count)
    {
        case 1:
            return VK_FORMAT_R8_UNORM;
        case 2:
            return VK_FORMAT_R8G8_UNORM;
        case 3:
            return VK_FORMAT_R8G8B8_UNORM;
        case 4:
            return VK_FORMAT_R8G8B8A8_UNORM;
        default:
            return default_format;
    }
}

b8 vulkan_renderer_texture_resources_acquire(renderer_backend_interface* backend, const char* name, bresource_texture_type type, u32 width, u32 height, u8 channel_count, u8 mip_levels, u16 array_size, bresource_texture_flag_bits flags, bhandle* out_renderer_texture_handle)
{
    vulkan_context* context = (vulkan_context*)backend->internal_context;

    if (!context->textures) {
        context->textures = darray_create(vulkan_texture_handle_data);
    }

    // Get an entry into the lookup table
    vulkan_texture_handle_data* texture_data = 0;
    u32 texture_count = darray_length(context->textures);
    for (u32 i = 0; i < texture_count; ++i)
    {
        texture_data = &context->textures[i];
        if (texture_data->uniqueid == INVALID_ID_U64)
        {
            // Found a free "slot", use it
            bhandle new_handle = bhandle_create(i);
            texture_data->uniqueid = new_handle.unique_id.uniqueid;
            *out_renderer_texture_handle = new_handle;
            break;
        }
    }

    if (khandle_is_invalid(*out_renderer_texture_handle))
    {
        // No free "slots", add one
        vulkan_texture_handle_data new_lookup = {0};
        bhandle new_handle = bhandle_create(texture_count);
        new_lookup.uniqueid = new_handle.unique_id.uniqueid;
        darray_push(context->textures, new_lookup);
        *out_renderer_texture_handle = new_handle;
        texture_data = &context->textures[texture_count];
    }

    if (flags & BRESOURCE_TEXTURE_FLAG_IS_WRAPPED)
    {
        // If the texure is considered "wrapped" (i.e. internal resources are created somwhere else, such as swapchain images),
        // then nothing further is required. Just return the handle
        return true;
    }

    // Internal data creation
    if (flags & TEXTURE_FLAG_RENDERER_BUFFERING)
    {
        texture_data->image_count = get_image_count(context);
    }
    else
    {
        // Only one needed
        texture_data->image_count = 1;
    }
    texture_data->images = ballocate(sizeof(vulkan_image) * texture_data->image_count, MEMORY_TAG_TEXTURE);

    VkImageUsageFlagBits usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    VkImageAspectFlagBits aspect;
    VkFormat image_format;
    if (flags & TEXTURE_FLAG_DEPTH)
    {
        usage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        aspect = VK_IMAGE_ASPECT_DEPTH_BIT;
        image_format = context->device.depth_format;
    }
    else
    {
        usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        aspect = VK_IMAGE_ASPECT_COLOR_BIT;
        if (flags & TEXTURE_FLAG_IS_WRITEABLE)
            image_format = context->current_window->renderer_state->backend_state->swapchain.image_format.format;
        else
            image_format = channel_count_to_format(channel_count, VK_FORMAT_R8G8B8A8_UNORM);
    }

    // Create one image per swapchain image (or just one image)

    for (u32 i = 0; i < texture_data->image_count; ++i)
    {
        char* image_name = string_format("%s_vkimage_%d", name, i);
        vulkan_image_create(
            context, type, width, height, array_size, image_format,
            VK_IMAGE_TILING_OPTIMAL, usage,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, true, aspect,
            image_name, mip_levels, &texture_data->images[i]);
        string_free(image_name);
    }

    return true;
}

void vulkan_renderer_texture_resources_release(renderer_backend_interface* backend, bhandle* renderer_texture_handle)
{
    vulkan_context* context = (vulkan_context*)backend->internal_context;

    vulkan_texture_handle_data* texture_data = &context->textures[renderer_texture_handle->handle_index];
    if (texture_data->uniqueid != renderer_texture_handle->unique_id.uniqueid)
    {
        BWARN("Stale handle passed while trying to release renderer texture resources");
        return;
    }

    // Invalidate the handle first
    texture_data->uniqueid = INVALID_ID_U64;
    *renderer_texture_handle = bhandle_invalid();

    // Release/destroy the internal data
    if (texture_data->images)
    {
        for (u32 i = 0; i < texture_data->image_count; ++i)
            vulkan_image_destroy(context, &texture_data->images[i]);

        bfree(texture_data->images, sizeof(vulkan_image) * texture_data->image_count, MEMORY_TAG_TEXTURE);
        texture_data->images = 0;
    }
}

b8 vulkan_renderer_texture_resize(renderer_backend_interface* backend, bhandle renderer_texture_handle, u32 new_width, u32 new_height)
{
    vulkan_context* context = (vulkan_context*)backend->internal_context;
    
    // Ensure the handle isn't stale
    vulkan_texture_handle_data* texture_data = &context->textures[renderer_texture_handle.handle_index];
    if (texture_data->uniqueid != renderer_texture_handle.unique_id.uniqueid)
    {
        BERROR("Stale handle passed while trying to resize a texture");
        return false;
    }

    for (u32 i = 0; i < texture_data->image_count; ++i)
    {
        // Resizing is really just destroying the old image and creating a new one
        // Data is not preserved because there's no reliable way to map the old data to the new since the amount of data differs
        vulkan_image* image = &texture_data->images[i];
        image->image_create_info.extent.width = new_width;
        image->image_create_info.extent.height = new_height;
        // Recalculate mip levels if anything other than 1
        if (image->mip_levels > 1)
        {
            // Recalculate the number of levels
            // The number of mip levels is calculated by first taking the largest dimension
            // (either width or height), figuring out how many times that number can be divided
            // by 2, taking the floor value (rounding down) and adding 1 to represent the
            // base level. This always leaves a value of at least 1.
            image->mip_levels = (u32)(bfloor(blog2(BMAX(new_width, new_height))) + 1);
        }

        vulkan_image_recreate(context, image);
    }

    return true;
}

b8 vulkan_renderer_texture_write_data(renderer_backend_interface* backend, bhandle renderer_texture_handle, u32 offset, u32 size, const u8* pixels, b8 include_in_frame_workload)
{
    vulkan_context* context = (vulkan_context*)backend->internal_context;

    // Ensure the handle isn't stale
    vulkan_texture_handle_data* texture_data = &context->textures[renderer_texture_handle.handle_index];
    if (texture_data->uniqueid != renderer_texture_handle.unique_id.uniqueid)
    {
        BERROR("Stale handle passed while trying to write data to a texture");
        return false;
    }

    // If no window, can't include in a frame workload
    if (!context->current_window)
        include_in_frame_workload = false;

    // Temporary staging renderbuffer, if needed
    renderbuffer temp;
    // Temporary command buffer, if needed
    vulkan_command_buffer temp_command_buffer;

    // A pointer to the staging buffer to be used
    renderbuffer* staging = 0;
    // A pointer to the command buffer to be used
    vulkan_command_buffer* command_buffer = 0;
    if (include_in_frame_workload)
    {
        // Including in the frame workload means the current window's current-frame staging buffer can be used
        u32 current_frame = context->current_window->renderer_state->backend_state->current_frame;
        staging = &context->current_window->renderer_state->backend_state->staging[current_frame];
        command_buffer = get_current_command_buffer(context);
    }
    else
    {
        // Not including in the frame workload means a temporary staging buffer needs to be created and bound
        // This buffer is the exact size required for the operation, so no allocation is needed later
        renderer_renderbuffer_create("temp_staging", RENDERBUFFER_TYPE_STAGING, size * texture_data->image_count, RENDERBUFFER_TRACK_TYPE_NONE, &temp);
        renderer_renderbuffer_bind(&temp, 0);
        // Set the temp buffer as the staging buffer to be used
        staging = &temp;
    }

    for (u32 i = 0; i < texture_data->image_count; ++i)
    {
        vulkan_image* image = &texture_data->images[i];

        // Staging buffer
        u64 staging_offset = 0;
        if (include_in_frame_workload)
        {
            // If including in frame workload, space needs to be allocated from the buffer
            renderer_renderbuffer_allocate(staging, size, &staging_offset);
        }

        // Results in a wait if not included in frame workload
        vulkan_buffer_load_range(backend, staging, staging_offset, size, pixels, include_in_frame_workload);

        // Need a temp command buffer if not included in frame workload
        if (!include_in_frame_workload)
        {
            vulkan_command_buffer_allocate_and_begin_single_use(context, context->device.graphics_command_pool, &temp_command_buffer);
            command_buffer = &temp_command_buffer;
        }

        // Transition the layout from whatever it is currently to optimal for recieving data
        vulkan_image_transition_layout(context, command_buffer, image, image->format, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

        // Copy the data from the buffer
        vulkan_image_copy_from_buffer(context, image, ((vulkan_buffer*)staging->internal_data)->handle, staging_offset, command_buffer);

        if (image->mip_levels <= 1 || !vulkan_image_mipmaps_generate(context, image, command_buffer))
        {
            // If mip generation isn't needed or fails, fall back to ordinary transition
            // Transition from optimal for data reciept to shader-read-only optimal layout
            vulkan_image_transition_layout(context, command_buffer, image, image->format, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        }

        // Need to submit temp command buffer
        if (!include_in_frame_workload)
        {
            vulkan_command_buffer_end_single_use(context, context->device.graphics_command_pool, command_buffer, context->device.graphics_queue);
            command_buffer = 0;
        }
    }
    
    if (!include_in_frame_workload)
    {
        renderer_renderbuffer_destroy(&temp);
        // Counts as a texture update. The texture generation here can only really be updated if we don't include the upload in the frame workload, since that results in a wait
        // If we include it in the frame workload, then we must also wait until that frame's queue is complete
        texture_data->generation++;
    }
    else
    {
        // Add handle to post-frame-queue-completion list. These will be updated at the end of the frame
        u32 current_frame = get_current_frame_index(context);
        darray_push(context->current_window->renderer_state->backend_state->frame_texture_updated_list[current_frame], renderer_texture_handle);
    }

    return true;
}

static b8 texture_read_offset_range(
    renderer_backend_interface* backend,
    vulkan_texture_handle_data* texture_data,
    u32 offset,
    u32 size,
    u32 x,
    u32 y,
    u32 width,
    u32 height,
    u8** out_memory)
{
    vulkan_context* context = (vulkan_context*)backend->internal_context;
    if (texture_data)
    {
        // Always just use the first image for this operaton
        vulkan_image* image = &texture_data->images[0];

        // NOTE: If offset or size are nonzero, read the entire image and select the offset and size in the range
        if (offset || size)
        {
            x = y = 0;
            width = image->width;
            height = image->height;
        }
        else
        {
            // NOTE: Assuming RGBA/8bpp
            size = image->width * image->height * 4 * sizeof(u8);
        }

        // Create a staging buffer and load data into it
        // TODO: global read buffer w/freelist (like staging), but for reading
        renderbuffer staging;
        if (!renderer_renderbuffer_create("renderbuffer_texture_read_staging", RENDERBUFFER_TYPE_READ, size, RENDERBUFFER_TRACK_TYPE_NONE, &staging))
        {
            BERROR("Failed to create staging buffer for texture read");
            return false;
        }
        renderer_renderbuffer_bind(&staging, 0);

        vulkan_command_buffer temp_buffer;
        VkCommandPool pool = context->device.graphics_command_pool;
        VkQueue queue = context->device.graphics_queue;
        vulkan_command_buffer_allocate_and_begin_single_use(context, pool, &temp_buffer);

        // NOTE: transition to VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL
        // Transition the layout from whatever it is currently to optimal for handing out data
        vulkan_image_transition_layout(context, &temp_buffer, image, image->format, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

        // Copy the data to the buffer
        vulkan_image_copy_region_to_buffer(context, image, ((vulkan_buffer*)staging.internal_data)->handle, x, y, width, height, &temp_buffer);

        // Transition from optimal for data reading to shader-read-only optimal layout
        // TODO: Should probably cache the previous layout and transfer back to that instead
        vulkan_image_transition_layout(context, &temp_buffer, image, image->format, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

        vulkan_command_buffer_end_single_use(context, pool, &temp_buffer, queue);

        if (!vulkan_buffer_read(backend, &staging, offset, size, (void**)out_memory))
            BERROR("vulkan_buffer_read failed");

        renderer_renderbuffer_unbind(&staging);
        renderer_renderbuffer_destroy(&staging);
        return true;
    }

    return false;
}

b8 vulkan_renderer_texture_read_data(renderer_backend_interface* backend, bhandle renderer_texture_handle, u32 offset, u32 size, u8** out_pixels)
{
    vulkan_context* context = (vulkan_context*)backend->internal_context;

    // Ensure the handle isn't stale
    vulkan_texture_handle_data* texture_data = &context->textures[renderer_texture_handle.handle_index];
    if (texture_data->uniqueid != renderer_texture_handle.unique_id.uniqueid)
    {
        BERROR("Stale handle passed while trying to reading data from a texture");
        return false;
    }
    return texture_read_offset_range(backend, texture_data, offset, size, 0, 0, 0, 0, out_pixels);
}

b8 vulkan_renderer_texture_read_pixel(renderer_backend_interface* backend, bhandle renderer_texture_handle, u32 x, u32 y, u8** out_rgba)
{
    vulkan_context* context = (vulkan_context*)backend->internal_context;

    // Ensure the handle isn't stale
    vulkan_texture_handle_data* texture_data = &context->textures[renderer_texture_handle.handle_index];
    if (texture_data->uniqueid != renderer_texture_handle.unique_id.uniqueid)
    {
        BERROR("Stale handle passed while trying to reading pixel data from a texture");
        return false;
    }
    return texture_read_offset_range(backend, texture_data, 0, 0, x, y, 1, 1, out_rgba);
}

b8 vulkan_renderer_shader_create(renderer_backend_interface* backend, bshader* s, const shader_config* config)
{
    // Verify stage support
    for (u8 i = 0; i < config->stage_count; ++i)
    {
        switch (config->stage_configs[i].stage)
        {
            case SHADER_STAGE_FRAGMENT:
            case SHADER_STAGE_VERTEX:
                break;
            case SHADER_STAGE_GEOMETRY:
                BWARN("vulkan_renderer_shader_create: VK_SHADER_STAGE_GEOMETRY_BIT is set but not yet supported");
                break;
            case SHADER_STAGE_COMPUTE:
                BWARN("vulkan_renderer_shader_create: SHADER_STAGE_COMPUTE is set but not yet supported");
                break;
            default:
                BERROR("Unsupported stage type: %d", shader_stage_to_string(config->stage_configs[i].stage));
                break;
        }
    }

    s->internal_data = ballocate(sizeof(vulkan_shader), MEMORY_TAG_RENDERER);
    vulkan_context* context = (vulkan_context*)backend->internal_context;

    // Setup the internal shader
    vulkan_shader* internal_shader = (vulkan_shader*)s->internal_data;
    internal_shader->per_draw_push_constant_block = ballocate(128, MEMORY_TAG_RENDERER);

    internal_shader->stage_count = config->stage_count;

    // Need a max of 2 descriptor sets, one for global and one for instance
    // Note that this can mean that only one (or potentially none) exist as well
    internal_shader->descriptor_set_count = 0;
    b8 has_per_frame = s->per_frame.uniform_count > 0 || s->per_frame.uniform_sampler_count > 0;
    b8 has_per_group = s->per_group.uniform_count > 0 || s->per_group.uniform_sampler_count > 0;
    b8 has_per_draw = s->per_draw.uniform_sampler_count > 0;
    bzero_memory(internal_shader->descriptor_sets, sizeof(vulkan_descriptor_set_config) * 2);
    u8 set_count = 0;
    if (has_per_frame)
    {
        internal_shader->descriptor_sets[set_count].sampler_binding_index_start = INVALID_ID_U8;
        set_count++;
    }
    if (has_per_group)
    {
        internal_shader->descriptor_sets[set_count].sampler_binding_index_start = INVALID_ID_U8;
        set_count++;
    }

    // Attributes array
    bzero_memory(internal_shader->attributes, sizeof(VkVertexInputAttributeDescription) * VULKAN_SHADER_MAX_ATTRIBUTES);
    // Calculate the total number of descriptors needed
    u32 image_count = context->current_window->renderer_state->backend_state->swapchain.image_count;
    // 1 set of globals * framecount + x samplers per instance, per frame
    u32 max_sampler_count = (s->per_frame.uniform_sampler_count * image_count) +
                            (config->max_groups * s->per_group.uniform_sampler_count * image_count) +
                            (config->max_per_draw_count * s->per_draw.uniform_sampler_count * image_count);
    // 1 global (1*framecount) + 1 per instance, per frame
    u32 max_ubo_count = image_count + (config->max_groups * image_count);
    // Total number of descriptors needed
    u32 max_descriptor_allocate_count = max_ubo_count + max_sampler_count;

    internal_shader->max_descriptor_set_count = max_descriptor_allocate_count;
    internal_shader->max_groups = config->max_groups;
    internal_shader->max_per_draw_count = config->max_per_draw_count;

    // For now, shaders will only ever have these 2 types of descriptor pools
    internal_shader->pool_size_count = 0;
    if (max_ubo_count > 0)
    {
        internal_shader->pool_sizes[internal_shader->pool_size_count] = (VkDescriptorPoolSize){VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, max_ubo_count};
        internal_shader->pool_size_count++;
    }
    if (max_sampler_count > 0)
    {
        internal_shader->pool_sizes[internal_shader->pool_size_count] = (VkDescriptorPoolSize){VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, max_sampler_count};
        internal_shader->pool_size_count++;
        internal_shader->pool_sizes[internal_shader->pool_size_count] = (VkDescriptorPoolSize){VK_DESCRIPTOR_TYPE_SAMPLER, max_sampler_count};
        internal_shader->pool_size_count++;
        internal_shader->pool_sizes[internal_shader->pool_size_count] = (VkDescriptorPoolSize){VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, max_sampler_count};
        internal_shader->pool_size_count++;
    }

    if (has_per_frame)
    {
        vulkan_descriptor_set_config* set_config = &internal_shader->descriptor_sets[internal_shader->descriptor_set_count];

        // Total bindings are 1 UBO for per_frame (if needed), plus per_frame sampler count
        u32 ubo_count = s->per_frame.uniform_count ? 1 : 0;
        set_config->binding_count = ubo_count + s->per_frame.uniform_sampler_count;
        set_config->bindings = ballocate(sizeof(VkDescriptorSetLayoutBinding) * set_config->binding_count, MEMORY_TAG_ARRAY);

        // per_frame UBO binding is first, if present
        u8 per_frame_binding_index = 0;
        if (s->per_frame.uniform_count > 0)
        {
            set_config->bindings[per_frame_binding_index].binding = per_frame_binding_index;
            set_config->bindings[per_frame_binding_index].descriptorCount = 1;  // NOTE: the whole UBO is one binding
            set_config->bindings[per_frame_binding_index].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            set_config->bindings[per_frame_binding_index].stageFlags = VK_SHADER_STAGE_ALL;
            per_frame_binding_index++;
        }

        // Set the index where the sampler bindings start. This will be used later to figure out what index to begin binding sampler descriptors at
        set_config->sampler_binding_index_start = s->per_frame.uniform_count ? 1 : 0;

        // Add a binding for each configured sampler
        if (s->per_frame.uniform_sampler_count > 0)
        {
            for (u32 i = 0; i < s->per_frame.uniform_sampler_count; ++i)
            {
                // Look up by the sampler indices collected above
                shader_uniform_config* u = &config->uniforms[s->per_frame.sampler_indices[i]];
                set_config->bindings[per_frame_binding_index].binding = per_frame_binding_index;
                set_config->bindings[per_frame_binding_index].descriptorCount = BMAX(u->array_length, 1);  // Either treat as an array or a single texture, depending on what is passed in
                set_config->bindings[per_frame_binding_index].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                set_config->bindings[per_frame_binding_index].stageFlags = VK_SHADER_STAGE_ALL;
                per_frame_binding_index++;
            }
        }

        // Increment the set counter
        internal_shader->descriptor_set_count++;
    }

    // If using per_group uniforms, add UBO descriptor set
    if (has_per_group)
    {
        vulkan_descriptor_set_config* set_config = &internal_shader->descriptor_sets[internal_shader->descriptor_set_count];

        // Total bindings are 1 UBO for per_group (if needed), plus instance sampler count
        u32 ubo_count = s->per_group.uniform_count ? 1 : 0;
        set_config->binding_count = ubo_count + s->per_group.uniform_sampler_count;
        set_config->bindings = ballocate(sizeof(VkDescriptorSetLayoutBinding) * set_config->binding_count, MEMORY_TAG_ARRAY);

        // per_group UBO binding is first, if present
        u8 per_group_binding_index = 0;
        if (s->per_group.uniform_count > 0)
        {
            set_config->bindings[per_group_binding_index].binding = per_group_binding_index;
            set_config->bindings[per_group_binding_index].descriptorCount = 1;
            set_config->bindings[per_group_binding_index].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            set_config->bindings[per_group_binding_index].stageFlags = VK_SHADER_STAGE_ALL;
            per_group_binding_index++;
        }

        // Set the index where the sampler bindings start
        set_config->sampler_binding_index_start = s->per_group.uniform_count ? 1 : 0;

        // Add a binding for each configured sampler
        if (s->per_group.uniform_sampler_count > 0)
        {
            for (u32 i = 0; i < s->per_group.uniform_sampler_count; ++i)
            {
                // Look up by the sampler indices collected above
                shader_uniform_config* u = &config->uniforms[s->per_group.sampler_indices[i]];
                set_config->bindings[per_group_binding_index].binding = per_group_binding_index;
                set_config->bindings[per_group_binding_index].descriptorCount = BMAX(u->array_length, 1);
                set_config->bindings[per_group_binding_index].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                set_config->bindings[per_group_binding_index].stageFlags = VK_SHADER_STAGE_ALL;
                per_group_binding_index++;
            }
        }

        // Increment the set counter
        internal_shader->descriptor_set_count++;
    }

    // If using per_draw uniform samplers, sampler descriptor set
    if (has_per_draw)
    {
        // In that set, add a binding for UBO if used
        vulkan_descriptor_set_config* set_config = &internal_shader->descriptor_sets[internal_shader->descriptor_set_count];

        // Total bindings = per_draw sampler count
        set_config->binding_count = s->per_draw.uniform_sampler_count;
        set_config->bindings = ballocate(sizeof(VkDescriptorSetLayoutBinding) * set_config->binding_count, MEMORY_TAG_ARRAY);

        u8 per_draw_binding_index = 0;
        // Set the index where the sampler bindings start
        set_config->sampler_binding_index_start = 0;
        // Add a binding for each configured sampler
        for (u32 i = 0; i < s->per_draw.uniform_sampler_count; ++i)
        {
            // Look up by the sampler indices collected above
            shader_uniform_config* u = &config->uniforms[s->per_draw.sampler_indices[i]];
            set_config->bindings[per_draw_binding_index].binding = per_draw_binding_index;
            set_config->bindings[per_draw_binding_index].descriptorCount = BMAX(u->array_length, 1); // Either treat as an array or a single texture, depending on what is passed in
            set_config->bindings[per_draw_binding_index].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            set_config->bindings[per_draw_binding_index].stageFlags = VK_SHADER_STAGE_ALL;
            per_draw_binding_index++;
        }

        // Increment the set counter
        internal_shader->descriptor_set_count++;
    }

    // Invalidate per-frame state.
    bzero_memory(&internal_shader->per_frame_state, sizeof(vulkan_shader_frequency_state));
    internal_shader->per_frame_state.id = INVALID_ID;

    // Invalidate all per-group states
    internal_shader->group_states = ballocate(sizeof(vulkan_shader_frequency_state) * internal_shader->max_groups, MEMORY_TAG_ARRAY);
    for (u32 i = 0; i < internal_shader->max_groups; ++i)
        internal_shader->group_states[i].id = INVALID_ID;

    // Invalidate per-draw states
    internal_shader->per_draw_states = ballocate(sizeof(vulkan_shader_frequency_state) * internal_shader->max_per_draw_count, MEMORY_TAG_ARRAY);
    for (u32 i = 0; i < internal_shader->max_per_draw_count; ++i)
        internal_shader->per_draw_states[i].id = INVALID_ID;

    // Keep copy of cull mode
    internal_shader->cull_mode = config->cull_mode;

    // Keep copy of topology types
    s->topology_types = config->topology_types;

    return true;
}

void vulkan_renderer_shader_destroy(renderer_backend_interface* backend, bshader* s)
{
    if (s && s->internal_data)
    {
        vulkan_shader* internal_shader = s->internal_data;
        if (!internal_shader)
        {
            BERROR("vulkan_renderer_shader_destroy requires a valid pointer to a shader");
            return;
        }

        vulkan_context* context = (vulkan_context*)backend->internal_context;
        VkDevice logical_device = context->device.logical_device;
        VkAllocationCallbacks* vk_allocator = context->allocator;

        u32 image_count = internal_shader->uniform_buffer_count;

        // Descriptor set layouts
        for (u32 i = 0; i < internal_shader->descriptor_set_count; ++i)
        {
            if (internal_shader->descriptor_set_layouts[i])
            {
                bfree(internal_shader->descriptor_sets[i].bindings, sizeof(VkDescriptorSetLayoutBinding) * internal_shader->descriptor_sets[i].binding_count, MEMORY_TAG_ARRAY);
                vkDestroyDescriptorSetLayout(logical_device, internal_shader->descriptor_set_layouts[i], vk_allocator);
                internal_shader->descriptor_set_layouts[i] = 0;
            }
        }

        // Global descriptor sets
        BFREE_TYPE_CARRAY(internal_shader->per_frame_state.descriptor_sets, VkDescriptorSet, image_count);

        // Descriptor pool
        if (internal_shader->descriptor_pool)
            vkDestroyDescriptorPool(logical_device, internal_shader->descriptor_pool, vk_allocator);
        
        // Destroy the instance states
        for (u32 i = 0; i < internal_shader->max_groups; ++i)
        {
            vulkan_shader_frequency_state* instance = &internal_shader->group_states[i];
            if (instance->descriptor_sets)
            {
                bfree(instance->descriptor_sets, sizeof(VkDescriptorSet) * image_count, MEMORY_TAG_ARRAY);
                instance->descriptor_sets = 0;
            }
            if (instance->sampler_states)
            {
                bfree(instance->sampler_states, sizeof(vulkan_uniform_sampler_state) * s->per_group.uniform_sampler_count, MEMORY_TAG_ARRAY);
                instance->sampler_states = 0;
            }
        }
        bfree(internal_shader->group_states, sizeof(vulkan_shader_frequency_state) * internal_shader->max_groups, MEMORY_TAG_ARRAY);

        // Destroy the local states
        for (u32 i = 0; i < internal_shader->max_per_draw_count; ++i)
        {
            vulkan_shader_frequency_state* local = &internal_shader->per_draw_states[i];
            if (local->descriptor_sets)
            {
                bfree(local->descriptor_sets, sizeof(VkDescriptorSet) * image_count, MEMORY_TAG_ARRAY);
                local->descriptor_sets = 0;
            }
            if (local->sampler_states)
            {
                bfree(local->sampler_states, sizeof(vulkan_uniform_sampler_state) * s->per_group.uniform_sampler_count, MEMORY_TAG_ARRAY);
                local->sampler_states = 0;
            }
        }
        bfree(internal_shader->per_draw_states, sizeof(vulkan_shader_frequency_state) * internal_shader->max_per_draw_count, MEMORY_TAG_ARRAY);

        // Uniform buffer
        for (u32 i = 0; i < image_count; ++i)
        {
            vulkan_buffer_unmap_memory(backend, &internal_shader->uniform_buffers[i], 0, VK_WHOLE_SIZE);
            internal_shader->mapped_uniform_buffer_blocks[i] = 0;
            renderer_renderbuffer_destroy(&internal_shader->uniform_buffers[i]);
        }
        bfree(internal_shader->mapped_uniform_buffer_blocks, sizeof(void*) * image_count, MEMORY_TAG_ARRAY);
        bfree(internal_shader->uniform_buffers, sizeof(renderbuffer) * image_count, MEMORY_TAG_ARRAY);

        // Pipelines
        for (u32 i = 0; i < VULKAN_TOPOLOGY_CLASS_MAX; ++i)
        {
            if (internal_shader->pipelines[i])
                vulkan_pipeline_destroy(context, internal_shader->pipelines[i]);
            if (internal_shader->wireframe_pipelines && internal_shader->wireframe_pipelines[i])
                vulkan_pipeline_destroy(context, internal_shader->wireframe_pipelines[i]);
        }

        // Shader modules
        for (u32 i = 0; i < internal_shader->stage_count; ++i)
            vkDestroyShaderModule(context->device.logical_device, internal_shader->stages[i].handle, context->allocator);

        // Free internal data memory
        bfree(s->internal_data, sizeof(vulkan_shader), MEMORY_TAG_RENDERER);
        s->internal_data = 0;
    }
}

static b8 shader_create_modules_and_pipelines(renderer_backend_interface* backend, bshader* s)
{
    vulkan_context* context = (vulkan_context*)backend->internal_context;
    vulkan_shader* internal_shader = (vulkan_shader*)s->internal_data;

    b8 has_error = false;

    // Only dynamic topology is supported. Create one pipeline per topology class
    u32 pipeline_count = 3;

    vulkan_pipeline* new_pipelines = ballocate(sizeof(vulkan_pipeline) * pipeline_count, MEMORY_TAG_ARRAY);
    vulkan_pipeline* new_wireframe_pipelines = 0;
    if (internal_shader->wireframe_pipelines)
        new_wireframe_pipelines = ballocate(sizeof(vulkan_pipeline) * pipeline_count, MEMORY_TAG_ARRAY);

    // Create module for each stage
    vulkan_shader_stage* new_stages = ballocate(sizeof(vulkan_shader_stage) * VULKAN_SHADER_MAX_STAGES, MEMORY_TAG_ARRAY);
    for (u32 i = 0; i < internal_shader->stage_count; ++i)
    {
        shader_stage_config* sc = &s->stage_configs[i];
        if (!create_shader_module(context, s, sc->stage, sc->source, sc->filename, &new_stages[i]))
        {
            BERROR("Unable to create %s shader module for '%s'. Shader will be destroyed", s->stage_configs[i].filename, s->name);
            has_error = true;
            goto shader_module_pipeline_cleanup;
        }
    }

    u32 framebuffer_width = context->current_window->width;
    u32 framebuffer_height = context->current_window->height;

    // Default viewport/scissor, can be dynamically overidden
    VkViewport viewport;
    viewport.x = 0.0f;
    viewport.y = (f32)framebuffer_height;
    viewport.width = (f32)framebuffer_width;
    viewport.height = -(f32)framebuffer_height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    // Scissor
    VkRect2D scissor;
    scissor.offset.x = scissor.offset.y = 0;
    scissor.extent.width = framebuffer_width;
    scissor.extent.height = framebuffer_height;

    VkPipelineShaderStageCreateInfo stage_create_infos[VULKAN_SHADER_MAX_STAGES];
    bzero_memory(stage_create_infos, sizeof(VkPipelineShaderStageCreateInfo) * VULKAN_SHADER_MAX_STAGES);
    for (u32 i = 0; i < internal_shader->stage_count; ++i)
        stage_create_infos[i] = new_stages[i].shader_stage_create_info;

    // Loop through and config/create one pipeline per class. Null entries are skipped
    for (u32 i = 0; i < pipeline_count; ++i)
    {
        if (!internal_shader->pipelines[i])
            continue;

        // Make sure the supported types are noted in the temp array pipelines
        new_pipelines[i].supported_topology_types = internal_shader->pipelines[i]->supported_topology_types;
        if (internal_shader->wireframe_pipelines)
            new_wireframe_pipelines[i].supported_topology_types = internal_shader->wireframe_pipelines[i]->supported_topology_types;
        
        vulkan_pipeline_config pipeline_config = {0};
        pipeline_config.stride = s->attribute_stride;
        pipeline_config.attribute_count = darray_length(s->attributes);
        pipeline_config.attributes = internal_shader->attributes;
        pipeline_config.descriptor_set_layout_count = internal_shader->descriptor_set_count;
        pipeline_config.descriptor_set_layouts = internal_shader->descriptor_set_layouts;
        pipeline_config.stage_count = internal_shader->stage_count;
        pipeline_config.stages = stage_create_infos;
        pipeline_config.viewport = viewport;
        pipeline_config.scissor = scissor;
        pipeline_config.cull_mode = internal_shader->cull_mode;

        // Strip the wireframe flag if it's there
        shader_flag_bits flags = s->flags;
        flags &= ~(SHADER_FLAG_WIREFRAME);
        pipeline_config.shader_flags = flags;
        // NOTE: Always one block for the push constant
        pipeline_config.push_constant_range_count = 1;
        range push_constant_range;
        push_constant_range.offset = 0;
        push_constant_range.size = s->per_draw.ubo_stride;
        pipeline_config.push_constant_ranges = &push_constant_range;
        pipeline_config.name = string_duplicate(s->name);
        pipeline_config.topology_types = s->topology_types;

        if ((s->flags & SHADER_FLAG_COLOR_READ) || (s->flags & SHADER_FLAG_COLOR_WRITE))
        {
            // TODO: Figure out the format(s) of the color attachments (if they exist) and pass them along here.
            // This just assumes the same format as the default render target/swapchain. This will work
            // until there is a shader with more than 1 color attachment, in which case either the
            // shader configuration itself will have to be amended to indicate this directly and/or the
            // shader configuration can specify some known "pipeline type" (i.e. "forward"), and that
            // type contains the image format information needed here. Putting a pin in this for now
            // until the eventual shader refactoring.
            pipeline_config.color_attachment_count = 1;
            pipeline_config.color_attachment_formats = &context->current_window->renderer_state->backend_state->swapchain.image_format.format;
        }
        else
        {
            pipeline_config.color_attachment_count = 0;
            pipeline_config.color_attachment_formats = 0;
        }

        if ((s->flags & SHADER_FLAG_DEPTH_TEST) || (s->flags & SHADER_FLAG_DEPTH_WRITE) || (s->flags & SHADER_FLAG_STENCIL_TEST) || (s->flags & SHADER_FLAG_STENCIL_WRITE))
        {
            pipeline_config.depth_attachment_format = context->device.depth_format;
            pipeline_config.stencil_attachment_format = context->device.depth_format;
        }
        else
        {
            pipeline_config.depth_attachment_format = VK_FORMAT_UNDEFINED;
            pipeline_config.stencil_attachment_format = VK_FORMAT_UNDEFINED;
        }

        b8 pipeline_result = vulkan_graphics_pipeline_create(context, &pipeline_config, &new_pipelines[i]);

        // Create the wireframe version
        if (pipeline_result && new_wireframe_pipelines)
        {
            // Use the same config, but make sure the wireframe flag is set
            pipeline_config.shader_flags |= SHADER_FLAG_WIREFRAME;
            pipeline_result = vulkan_graphics_pipeline_create(context, &pipeline_config, &new_wireframe_pipelines[i]);
        }

        bfree(pipeline_config.name, string_length(pipeline_config.name) + 1, MEMORY_TAG_STRING);

        if (!pipeline_result)
        {
            BERROR("Failed to load graphics pipeline for shader: '%s'", s->name);
            has_error = true;
            break;
        }
    }

    // If failed, cleanup
    if (has_error)
    {
        for (u32 i = 0; i < pipeline_count; ++i)
        {
            vulkan_pipeline_destroy(context, &new_pipelines[i]);
            if (new_wireframe_pipelines)
                vulkan_pipeline_destroy(context, &new_wireframe_pipelines[i]);
        }
        for (u32 i = 0; i < internal_shader->stage_count; ++i)
            vkDestroyShaderModule(context->device.logical_device, new_stages[i].handle, context->allocator);
        goto shader_module_pipeline_cleanup;
    }

    // In success, destroy the old pipelines and move the new pipelines over
    vkDeviceWaitIdle(context->device.logical_device);
    for (u32 i = 0; i < pipeline_count; ++i)
    {
        if (internal_shader->pipelines[i])
        {
            vulkan_pipeline_destroy(context, internal_shader->pipelines[i]);
            bcopy_memory(internal_shader->pipelines[i], &new_pipelines[i], sizeof(vulkan_pipeline));
        }
        if (new_wireframe_pipelines)
        {
            if (internal_shader->wireframe_pipelines[i])
            {
                vulkan_pipeline_destroy(context, internal_shader->wireframe_pipelines[i]);
                bcopy_memory(internal_shader->wireframe_pipelines[i], &new_wireframe_pipelines[i], sizeof(vulkan_pipeline));
            }
        }
    }

    // Destroy the old shader modules and copy over the new ones
    for (u32 i = 0; i < internal_shader->stage_count; ++i)
    {
        vkDestroyShaderModule(context->device.logical_device, internal_shader->stages[i].handle, context->allocator);
        bcopy_memory(&internal_shader->stages[i], &new_stages[i], sizeof(vulkan_shader_stage));
    }

shader_module_pipeline_cleanup:
    bfree(new_pipelines, sizeof(vulkan_pipeline) * pipeline_count, MEMORY_TAG_ARRAY);
    if (new_wireframe_pipelines)
        bfree(new_wireframe_pipelines, sizeof(vulkan_pipeline) * pipeline_count, MEMORY_TAG_ARRAY);

    bfree(new_stages, sizeof(vulkan_shader_stage) * VULKAN_SHADER_MAX_STAGES, MEMORY_TAG_ARRAY);

    return !has_error;
}

b8 vulkan_renderer_shader_initialize(renderer_backend_interface* backend, bshader* s)
{
    vulkan_context* context = (vulkan_context*)backend->internal_context;
    VkDevice logical_device = context->device.logical_device;
    VkAllocationCallbacks* vk_allocator = context->allocator;
    vulkan_shader* internal_shader = (vulkan_shader*)s->internal_data;

    b8 needs_wireframe = (s->flags & SHADER_FLAG_WIREFRAME) != 0;
    // Determine if the implementation supports this and set to false if not
    if (!context->device.features.fillModeNonSolid)
    {
        BINFO("Renderer backend does not support fillModeNonSolid. Wireframe mode is not possible, but was requested for the shader '%s'", s->name);
        needs_wireframe = false;
    }

    // Static lookup table for types->Vulkan
    static VkFormat* types = 0;
    static VkFormat t[11];
    if (!types)
    {
        t[SHADER_ATTRIB_TYPE_FLOAT32] = VK_FORMAT_R32_SFLOAT;
        t[SHADER_ATTRIB_TYPE_FLOAT32_2] = VK_FORMAT_R32G32_SFLOAT;
        t[SHADER_ATTRIB_TYPE_FLOAT32_3] = VK_FORMAT_R32G32B32_SFLOAT;
        t[SHADER_ATTRIB_TYPE_FLOAT32_4] = VK_FORMAT_R32G32B32A32_SFLOAT;
        t[SHADER_ATTRIB_TYPE_INT8] = VK_FORMAT_R8_SINT;
        t[SHADER_ATTRIB_TYPE_UINT8] = VK_FORMAT_R8_UINT;
        t[SHADER_ATTRIB_TYPE_INT16] = VK_FORMAT_R16_SINT;
        t[SHADER_ATTRIB_TYPE_UINT16] = VK_FORMAT_R16_UINT;
        t[SHADER_ATTRIB_TYPE_INT32] = VK_FORMAT_R32_SINT;
        t[SHADER_ATTRIB_TYPE_UINT32] = VK_FORMAT_R32_UINT;
        types = t;
    }

    // Process attributes
    u32 attribute_count = darray_length(s->attributes);
    u32 offset = 0;
    for (u32 i = 0; i < attribute_count; ++i)
    {
        // Setup new attribute
        VkVertexInputAttributeDescription attribute;
        attribute.location = i;
        attribute.binding = 0;
        attribute.offset = offset;
        attribute.format = types[s->attributes[i].type];

        // Push into the config's attribute collection and add to the stride
        internal_shader->attributes[i] = attribute;

        offset += s->attributes[i].size;
    }

    // Descriptor pool
    VkDescriptorPoolCreateInfo pool_info = {VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    pool_info.poolSizeCount = internal_shader->pool_size_count;
    pool_info.pPoolSizes = internal_shader->pool_sizes;
    pool_info.maxSets = internal_shader->max_descriptor_set_count;
    pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;

    // Create descriptor pool
    VkResult result = vkCreateDescriptorPool(logical_device, &pool_info, vk_allocator, &internal_shader->descriptor_pool);
    if (!vulkan_result_is_success(result))
    {
        BERROR("vulkan_shader_initialize failed creating descriptor pool: '%s'", vulkan_result_string(result, true));
        return false;
    }

    // Create descriptor set layouts
    bzero_memory(internal_shader->descriptor_set_layouts, internal_shader->descriptor_set_count);
    for (u32 i = 0; i < internal_shader->descriptor_set_count; ++i)
    {
        VkDescriptorSetLayoutCreateInfo layout_info = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
        layout_info.bindingCount = internal_shader->descriptor_sets[i].binding_count;
        layout_info.pBindings = internal_shader->descriptor_sets[i].bindings;

        result = vkCreateDescriptorSetLayout(logical_device, &layout_info, vk_allocator, &internal_shader->descriptor_set_layouts[i]);
        if (!vulkan_result_is_success(result))
        {
            BERROR("vulkan_shader_initialize failed descriptor set layout: '%s'", vulkan_result_string(result, true));
            return false;
        }
    }
    
    // Only dynamic topology is supported. Create one pipeline per topology class
    u32 pipeline_count = 3;

    // Create an array of pointers to pipelines, one per topology class. Null means not supported for this shader
    internal_shader->pipelines = ballocate(sizeof(vulkan_pipeline*) * pipeline_count, MEMORY_TAG_ARRAY);

    // Do the same as above, but a wireframe version
    if (needs_wireframe)
        internal_shader->wireframe_pipelines = ballocate(sizeof(vulkan_pipeline*) * pipeline_count, MEMORY_TAG_ARRAY);
    else
        internal_shader->wireframe_pipelines = 0;

    // Create one pipeline per topology class
    // Point class
    if (s->topology_types & PRIMITIVE_TOPOLOGY_TYPE_POINT_LIST)
    {
        internal_shader->pipelines[VULKAN_TOPOLOGY_CLASS_POINT] = ballocate(sizeof(vulkan_pipeline), MEMORY_TAG_VULKAN);
        // Set supported types for this class
        internal_shader->pipelines[VULKAN_TOPOLOGY_CLASS_POINT]->supported_topology_types |= PRIMITIVE_TOPOLOGY_TYPE_POINT_LIST;

        // Wireframe versions
        if (needs_wireframe)
        {
            internal_shader->wireframe_pipelines[VULKAN_TOPOLOGY_CLASS_POINT] = ballocate(sizeof(vulkan_pipeline), MEMORY_TAG_VULKAN);
            // Set supported types for this class
            internal_shader->wireframe_pipelines[VULKAN_TOPOLOGY_CLASS_POINT]->supported_topology_types |= PRIMITIVE_TOPOLOGY_TYPE_POINT_LIST;
        }
    }

    // Line class
    if (s->topology_types & PRIMITIVE_TOPOLOGY_TYPE_LINE_LIST || s->topology_types & PRIMITIVE_TOPOLOGY_TYPE_LINE_STRIP)
    {
        internal_shader->pipelines[VULKAN_TOPOLOGY_CLASS_LINE] = ballocate(sizeof(vulkan_pipeline), MEMORY_TAG_VULKAN);
        // Set supported types for this class
        internal_shader->pipelines[VULKAN_TOPOLOGY_CLASS_LINE]->supported_topology_types |= PRIMITIVE_TOPOLOGY_TYPE_LINE_LIST;
        internal_shader->pipelines[VULKAN_TOPOLOGY_CLASS_LINE]->supported_topology_types |= PRIMITIVE_TOPOLOGY_TYPE_LINE_STRIP;

        // Wireframe versions
        if (needs_wireframe)
        {
            internal_shader->wireframe_pipelines[VULKAN_TOPOLOGY_CLASS_LINE] = ballocate(sizeof(vulkan_pipeline), MEMORY_TAG_VULKAN);
            // Set supported types for this class
            internal_shader->wireframe_pipelines[VULKAN_TOPOLOGY_CLASS_LINE]->supported_topology_types |= PRIMITIVE_TOPOLOGY_TYPE_LINE_LIST;
            internal_shader->wireframe_pipelines[VULKAN_TOPOLOGY_CLASS_LINE]->supported_topology_types |= PRIMITIVE_TOPOLOGY_TYPE_LINE_STRIP;
        }
    }

    // Triangle class
    if (s->topology_types & PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE_LIST ||
        s->topology_types & PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE_STRIP ||
        s->topology_types & PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE_FAN)
    {
        internal_shader->pipelines[VULKAN_TOPOLOGY_CLASS_TRIANGLE] = ballocate(sizeof(vulkan_pipeline), MEMORY_TAG_VULKAN);
        // Set supported types for this class
        internal_shader->pipelines[VULKAN_TOPOLOGY_CLASS_TRIANGLE]->supported_topology_types |= PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE_LIST;
        internal_shader->pipelines[VULKAN_TOPOLOGY_CLASS_TRIANGLE]->supported_topology_types |= PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE_STRIP;
        internal_shader->pipelines[VULKAN_TOPOLOGY_CLASS_TRIANGLE]->supported_topology_types |= PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE_FAN;

        // Wireframe versions
        if (needs_wireframe)
        {
            internal_shader->wireframe_pipelines[VULKAN_TOPOLOGY_CLASS_TRIANGLE] = ballocate(sizeof(vulkan_pipeline), MEMORY_TAG_VULKAN);
            // Set the supported types for this class
            internal_shader->wireframe_pipelines[VULKAN_TOPOLOGY_CLASS_TRIANGLE]->supported_topology_types |= PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE_LIST;
            internal_shader->wireframe_pipelines[VULKAN_TOPOLOGY_CLASS_TRIANGLE]->supported_topology_types |= PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE_STRIP;
            internal_shader->wireframe_pipelines[VULKAN_TOPOLOGY_CLASS_TRIANGLE]->supported_topology_types |= PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE_FAN;
        }
    }

    if (!shader_create_modules_and_pipelines(backend, s))
    {
        BERROR("Failed initial load on shader '%s'. See logs for details", s->name);
        return false;
    }

    // TODO: Figure out what default should be here
    internal_shader->bound_pipeline_index = 0;
    b8 pipeline_found = false;
    for (u32 i = 0; i < pipeline_count; ++i)
    {
        if (internal_shader->pipelines[i])
        {
            internal_shader->bound_pipeline_index = i;

            // Extract first type from pipeline
            for (u32 j = 1; j < PRIMITIVE_TOPOLOGY_TYPE_MAX; j = j << 1)
            {
                if (internal_shader->pipelines[i]->supported_topology_types & j)
                {
                    switch (j)
                    {
                        case PRIMITIVE_TOPOLOGY_TYPE_POINT_LIST:
                            internal_shader->current_topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
                            break;
                        case PRIMITIVE_TOPOLOGY_TYPE_LINE_LIST:
                            internal_shader->current_topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
                            break;
                        case PRIMITIVE_TOPOLOGY_TYPE_LINE_STRIP:
                            internal_shader->current_topology = VK_PRIMITIVE_TOPOLOGY_LINE_STRIP;
                            break;
                        case PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE_LIST:
                            internal_shader->current_topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
                            break;
                        case PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE_STRIP:
                            internal_shader->current_topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
                            break;
                        case PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE_FAN:
                            internal_shader->current_topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN;
                            break;
                        default:
                            BWARN("primitive topology '%u' not supported. Skipping", j);
                            break;
                    }

                    break;
                }
            }
            pipeline_found = true;
            break;
        }
    }

    if (!pipeline_found)
    {
        // Getting here means that all of the pipelines are null, which they definitely should not be
        BERROR("No available topology classes are available, so a pipeline cannot be bound. Check shader configuration");
        return false;
    }

    // Grab UBO alignment requirement from device
    s->required_ubo_alignment = context->device.properties.limits.minUniformBufferOffsetAlignment;

    // Make sure UBO is aligned according to device requirements
    s->per_frame.ubo_stride = get_aligned(s->per_frame.ubo_size, s->required_ubo_alignment);
    s->per_group.ubo_stride = get_aligned(s->per_group.ubo_size, s->required_ubo_alignment);
    u32 image_count = get_image_count(context);

    internal_shader->mapped_uniform_buffer_blocks = BALLOC_TYPE_CARRAY(void*, image_count);
    internal_shader->uniform_buffers = BALLOC_TYPE_CARRAY(renderbuffer, image_count);
    internal_shader->uniform_buffer_count = image_count;

    // Uniform  buffers, one per swapchain image
    u64 total_buffer_size = s->per_frame.ubo_stride + (s->per_group.ubo_stride * internal_shader->max_groups);
    for (u32 i = 0; i < image_count; ++i)
    {
        const char* buffer_name = string_format("renderbuffer_uniform_%s_idx_%d", s->name, i);
        if (!renderer_renderbuffer_create(buffer_name, RENDERBUFFER_TYPE_UNIFORM, total_buffer_size, RENDERBUFFER_TRACK_TYPE_FREELIST, &internal_shader->uniform_buffers[i]))
        {
            BERROR("Vulkan buffer creation failed for object shader!");
            string_free(buffer_name);
            return false;
        }
        string_free(buffer_name);
        renderer_renderbuffer_bind(&internal_shader->uniform_buffers[i], 0);
        // Map the entire buffer's memory
        internal_shader->mapped_uniform_buffer_blocks[i] = vulkan_buffer_map_memory(backend, &internal_shader->uniform_buffers[i], 0, VK_WHOLE_SIZE);
    }

    return setup_frequency_state(context, s, SHADER_UPDATE_FREQUENCY_PER_FRAME, 0);
}

b8 vulkan_renderer_shader_reload(renderer_backend_interface* backend, bshader* s)
{
    return shader_create_modules_and_pipelines(backend, s);
}

b8 vulkan_renderer_shader_use(renderer_backend_interface* backend, bshader* s)
{
    vulkan_context* context = (vulkan_context*)backend->internal_context;
    vulkan_shader* internal = s->internal_data;
    vulkan_command_buffer* command_buffer = get_current_command_buffer(context);

    // Pick correct pipeline
    vulkan_pipeline** pipeline_array = s->is_wireframe ? internal->wireframe_pipelines : internal->pipelines;
    vulkan_pipeline_bind(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_array[internal->bound_pipeline_index]);

    context->bound_shader = s;

    // Make sure to use current bound type as well
    if (context->device.support_flags & VULKAN_DEVICE_SUPPORT_FLAG_NATIVE_DYNAMIC_STATE_BIT)
        vkCmdSetPrimitiveTopology(command_buffer->handle, internal->current_topology);
    else if (context->device.support_flags & VULKAN_DEVICE_SUPPORT_FLAG_DYNAMIC_STATE_BIT)
        context->vkCmdSetPrimitiveTopologyEXT(command_buffer->handle, internal->current_topology);

    return true;
}

b8 vulkan_renderer_shader_supports_wireframe(const renderer_backend_interface* backend, const bshader* s)
{
    const vulkan_shader* internal = s->internal_data;

    // If the array exists, this is supported
    if (internal->wireframe_pipelines)
        return true;

    return false;
}

static b8 vulkan_descriptorset_update_and_bind(renderer_backend_interface* backend, u64 renderer_frame_number, bshader* s, VkDescriptorSet descriptor_set,
                                        u32 descriptor_set_index, vulkan_descriptor_state* descriptor_state, u64 ubo_offset, u64 ubo_stride,
                                        u32 uniform_count, vulkan_uniform_sampler_state* samplers, u32 sampler_count, vulkan_uniform_texture_state* textures, u32 texture_count)
{
    vulkan_context* context = (vulkan_context*)backend->internal_context;
    u32 image_index = get_current_image_index(context);
    vulkan_shader* internal = s->internal_data;

    const frame_data* p_frame_data = engine_frame_data_get();

    // The descriptor_state holds frame number, which is compared against the current renderer frame number. If no match, it gets an update. Otherwise, it's bind-only
    b8 needs_update = descriptor_state->frame_numbers[image_index] != renderer_frame_number;
    if (needs_update)
    {
        // Allocate enough descriptor writes to handle the max allowed bound textures
        VkWriteDescriptorSet descriptor_writes[1 + VULKAN_SHADER_MAX_TEXTURE_BINDINGS];
        bzero_memory(descriptor_writes, sizeof(VkWriteDescriptorSet) * (1 + VULKAN_SHADER_MAX_TEXTURE_BINDINGS));

        u32 descriptor_write_count = 0;
        u32 binding_index = 0;

        VkDescriptorBufferInfo ubo_buffer_info = {0};

        // Descriptor 0 - Uniform buffer
        if (uniform_count > 0)
        {
            // Only do this if descriptor has not yet been updated
            u8* ubo_generation = &(descriptor_state->generations[image_index]);
            if (*ubo_generation == INVALID_ID_U8)
            {
                ubo_buffer_info.buffer = ((vulkan_buffer*)internal->uniform_buffers[image_index].internal_data)->handle;
                BASSERT_MSG((ubo_offset % context->device.properties.limits.minUniformBufferOffsetAlignment) == 0, "Ubo offset must be a multiple of device.properties.limits.minUniformBufferOffsetAlignment");
                ubo_buffer_info.offset = ubo_offset;
                ubo_buffer_info.range = ubo_stride;

                VkWriteDescriptorSet ubo_descriptor = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
                ubo_descriptor.dstSet = descriptor_set;
                ubo_descriptor.dstBinding = binding_index;
                ubo_descriptor.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
                ubo_descriptor.descriptorCount = 1;
                ubo_descriptor.pBufferInfo = &ubo_buffer_info;

                descriptor_writes[descriptor_write_count] = ubo_descriptor;
                descriptor_write_count++;

                // Update frame generation
                *ubo_generation = 1;
            }
            binding_index++;
        }

        // Iterate samplers
        if (sampler_count > 0)
        {
            vulkan_descriptor_set_config set_config = internal->descriptor_sets[descriptor_set_index];

            // Allocate enough space to hold all the descriptor image infos needed for this scope (one array per binding)
            VkDescriptorImageInfo** binding_image_infos = p_frame_data->allocator.allocate(sizeof(VkDescriptorImageInfo*) * sampler_count);

            // Iterate each sampler binding
            for (u32 sb = 0; sb < sampler_count; ++sb)
            {
                vulkan_uniform_sampler_state* binding_sampler_state = &samplers[sb];

                u32 binding_descriptor_count = set_config.bindings[binding_index].descriptorCount;
                u32 update_sampler_count = 0;
                
                // Allocate enough space to build all image infos
                binding_image_infos[sb] = p_frame_data->allocator.allocate(sizeof(VkDescriptorImageInfo) * binding_descriptor_count);

                // Each sampler descriptor within the binding
                for (u32 d = 0; d < binding_descriptor_count; ++d)
                {
                    bhandle* sampler_handle = &binding_sampler_state->sampler_handles[d];
                    vulkan_sampler_handle_data* sampler = &context->samplers[sampler_handle->handle_index];

                    // Not using image
                    binding_image_infos[sb][d].imageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
                    binding_image_infos[sb][d].imageView = 0;

                    // NOTE: Only the sampler is set here
                    binding_image_infos[sb][d].sampler = sampler->sampler;

                    update_sampler_count++;
                }

                VkWriteDescriptorSet sampler_descriptor = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
                sampler_descriptor.dstSet = descriptor_set;
                sampler_descriptor.dstBinding = binding_index;
                sampler_descriptor.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
                sampler_descriptor.descriptorCount = update_sampler_count;
                sampler_descriptor.pImageInfo = binding_image_infos[sb];

                descriptor_writes[descriptor_write_count] = sampler_descriptor;
                descriptor_write_count++;

                binding_index++;
            }
        }

        // Iterate textures
        if (texture_count > 0)
        {
            vulkan_descriptor_set_config set_config = internal->descriptor_sets[descriptor_set_index];

            // Allocate enough space to hold all the descriptor image infos needed for this scope (one array per binding)
            // NOTE: Using the frame allocator, so this does not have to be freed as it's handled automatically at the end of the frame on allocator reset
            VkDescriptorImageInfo** binding_image_infos = p_frame_data->allocator.allocate(sizeof(VkDescriptorImageInfo*) * texture_count);

            // Iterate each texture binding
            for (u32 tb = 0; tb < texture_count; ++tb)
            {
                vulkan_uniform_texture_state* binding_texture_state = &textures[tb];

                u32 binding_descriptor_count = set_config.bindings[binding_index].descriptorCount;

                u32 update_texture_count = 0;

                // Allocate enough space to build all image infos
                binding_image_infos[tb] = p_frame_data->allocator.allocate(sizeof(VkDescriptorImageInfo) * binding_descriptor_count);

                // Each texture descriptor within the binding
                for (u32 d = 0; d < binding_descriptor_count; ++d)
                {
                    // TODO: only update in the list if actually needing an update
                    bhandle t = binding_texture_state->texture_handles[d];
                    vulkan_texture_handle_data* texture_data = &context->textures[t.handle_index];

                    if (bhandle_is_invalid(t))
                    {
                        BERROR("Invalid handle found while trying to update/bind descriptor set");
                        return false;
                    }

                    u32 image_index = texture_data->image_count > 1 ? get_current_image_index(context) : 0;
                    vulkan_image* image = &texture_data->images[image_index];

                    binding_image_infos[tb][d].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                    binding_image_infos[tb][d].imageView = image->view;
                    // NOTE: Not using sampler in this descriptor
                    binding_image_infos[tb][d].sampler = 0;

                    update_texture_count++;
                }

                VkWriteDescriptorSet sampler_descriptor = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
                sampler_descriptor.dstSet = descriptor_set;
                sampler_descriptor.dstBinding = binding_index;
                sampler_descriptor.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
                sampler_descriptor.descriptorCount = update_texture_count;
                sampler_descriptor.pImageInfo = binding_image_infos[tb];

                descriptor_writes[descriptor_write_count] = sampler_descriptor;
                descriptor_write_count++;

                binding_index++;
            }
        }

        // Immediately update the descriptor set's data
        if (descriptor_write_count > 0)
        {
            // TODO: Should be split out to a separate frame_prepare step from the bind below
            vkUpdateDescriptorSets(context->device.logical_device, descriptor_write_count, descriptor_writes, 0, 0);
        }

        // Sync the frame number
        descriptor_state->frame_numbers[image_index] = renderer_frame_number;
    }
    
    // Pick the correct pipeline
    vulkan_pipeline** pipeline_array = s->is_wireframe ? internal->wireframe_pipelines : internal->pipelines;

    VkCommandBuffer command_buffer = get_current_command_buffer(context)->handle;
    // Bind descriptor set to be updated, or in case the shader changed
    vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_array[internal->bound_pipeline_index]->pipeline_layout, descriptor_set_index, 1, &descriptor_set, 0, 0);

    return true;
}

b8 vulkan_renderer_shader_apply_per_frame(renderer_backend_interface* backend, bshader* s, u64 renderer_frame_number)
{
    // Don't do anything if there are no updatable per-frame uniforms
    b8 has_per_frame = s->per_frame.uniform_count > 0 || s->per_frame.uniform_sampler_count > 0;
    if (!has_per_frame)
        return true;

    vulkan_context* context = (vulkan_context*)backend->internal_context;
    u32 image_index = get_current_image_index(context);
    vulkan_shader* internal = s->internal_data;

    vulkan_shader_frequency_state* per_frame_state = &internal->per_frame_state;

    // Global is always first, if it exists
    u32 descriptor_set_index = 0;

    if (!vulkan_descriptorset_update_and_bind(
            backend,
            renderer_frame_number,
            s,
            per_frame_state->descriptor_sets[image_index],
            descriptor_set_index,
            &per_frame_state->ubo_descriptor_state,
            s->per_frame.ubo_offset,
            s->per_frame.ubo_stride,
            s->per_frame.uniform_count,
            per_frame_state->sampler_states,
            s->per_frame.uniform_sampler_count,
            per_frame_state->texture_states,
            s->per_frame.uniform_texture_count))
    {
        BERROR("Failed to update/bind per-frame descriptor set");
        return false;
    }

    return true;
}

b8 vulkan_renderer_shader_apply_per_group(renderer_backend_interface* backend, bshader* s, u64 renderer_frame_number)
{
    // Bleat if there are no groups for this shader
    if (s->per_group.uniform_count < 1 && s->per_group.uniform_sampler_count < 1)
    {
        BERROR("This shader does not use groups");
        return false;
    }
    vulkan_context* context = (vulkan_context*)backend->internal_context;
    u32 image_index = get_current_image_index(context);
    vulkan_shader* internal = s->internal_data;

    // Obtain group data
    vulkan_shader_frequency_state* group_state = &internal->group_states[s->per_group.bound_id];

    // Determine the descriptor set index which will be first. If there are no per-frame uniforms, this will be 0. If there are per-frame uniforms, will be 1
    b8 has_per_frame = s->per_frame.uniform_count > 0 || s->per_frame.uniform_sampler_count > 0;
    u32 descriptor_set_index = has_per_frame ? 1 : 0;

    if (!vulkan_descriptorset_update_and_bind(
            backend,
            renderer_frame_number,
            s,
            group_state->descriptor_sets[image_index],
            descriptor_set_index,
            &group_state->ubo_descriptor_state,
            group_state->offset,
            s->per_group.ubo_stride,
            s->per_group.uniform_count,
            group_state->sampler_states,
            s->per_group.uniform_sampler_count,
            group_state->texture_states,
            s->per_group.uniform_texture_count))
    {
        BERROR("Failed to update/bind per-frame uniforms descriptor set");
        return false;
    }

    return true;
}

b8 vulkan_renderer_shader_apply_per_draw(renderer_backend_interface* backend, bshader* s, u64 renderer_frame_number)
{
    vulkan_context* context = (vulkan_context*)backend->internal_context;
    vulkan_shader* internal = s->internal_data;
    VkCommandBuffer command_buffer = get_current_command_buffer(context)->handle;

    // Pick the correct pipeline
    vulkan_pipeline** pipeline_array = s->is_wireframe ? internal->wireframe_pipelines : internal->pipelines;

    // Update the non-sampler uniforms via push constants
    vkCmdPushConstants(
        command_buffer,
        pipeline_array[internal->bound_pipeline_index]->pipeline_layout,
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        0, 128, internal->per_draw_push_constant_block);
    
    // Update local descriptor set if there are local samplers to be updated
    if (s->per_draw.uniform_sampler_count > 0)
    {
        u32 image_index = get_current_image_index(context);

        // Obtain local data
        vulkan_shader_frequency_state* per_draw_state = &internal->per_draw_states[s->per_draw.bound_id];

        // Determine the descriptor set index which will be first. If there are no per-frame uniforms and no per-group uniforms, for example this will be 0
        // If there are per-frame uniforms but not per-group, this will be 1, if there are both this will be 2
        b8 has_per_frame = s->per_frame.uniform_count > 0 || s->per_frame.uniform_sampler_count > 0;
        b8 has_group = s->per_group.uniform_count > 0 || s->per_group.uniform_sampler_count > 0;
        u32 descriptor_set_index = 0;
        descriptor_set_index += has_per_frame ? 1 : 0;
        descriptor_set_index += has_group ? 1 : 0;
        if (!vulkan_descriptorset_update_and_bind(
                backend,
                renderer_frame_number,
                s,
                per_draw_state->descriptor_sets[image_index],
                descriptor_set_index,
                &per_draw_state->ubo_descriptor_state,
                0,  // No UBO
                0,  // No UBO
                0,  // No UBO
                per_draw_state->sampler_states,
                s->per_draw.uniform_sampler_count,
                per_draw_state->texture_states,
                s->per_draw.uniform_texture_count))
        {
            BERROR("Failed to update/bind per_draw sampler descriptor set");
            return false;
        }
    }

    return true;
}

static b8 sampler_create_internal(vulkan_context* context, texture_filter filter, texture_repeat repeat, f32 anisotropy, vulkan_sampler_handle_data* out_sampler_handle_data)
{
    // Create a sampler for the texture
    VkSamplerCreateInfo sampler_info = {VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    sampler_info.minFilter = filter == TEXTURE_FILTER_MODE_LINEAR ? VK_FILTER_LINEAR : VK_FILTER_NEAREST;
    sampler_info.magFilter = filter == TEXTURE_FILTER_MODE_LINEAR ? VK_FILTER_LINEAR : VK_FILTER_NEAREST;
    VkSamplerAddressMode mode;

    switch (repeat)
    {
        case TEXTURE_REPEAT_CLAMP_TO_EDGE:
            mode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            break;
        case TEXTURE_REPEAT_CLAMP_TO_BORDER:
            mode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
            break;
        case TEXTURE_REPEAT_MIRRORED_REPEAT:
            mode = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
            break;
        default:
            mode = VK_SAMPLER_ADDRESS_MODE_REPEAT;
            break;
    }

    sampler_info.addressModeU = mode;
    sampler_info.addressModeV = mode;
    sampler_info.addressModeW = mode;

    // TODO: Fix this anywhere it's being used for a depth texture
    b8 use_anisotropy = context->device.features.samplerAnisotropy;
    if (false)
    {
        sampler_info.anisotropyEnable = VK_FALSE;
        sampler_info.maxAnisotropy = 0;
    }
    else
    {
        /* sampler_info.anisotropyEnable = VK_TRUE;
        sampler_info.maxAnisotropy = 16; */
        sampler_info.anisotropyEnable = VK_TRUE;
        sampler_info.maxAnisotropy = anisotropy;
    }
    sampler_info.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
    // sampler_info.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    sampler_info.unnormalizedCoordinates = VK_FALSE;
    sampler_info.compareEnable = VK_FALSE;
    sampler_info.compareOp = VK_COMPARE_OP_ALWAYS;
    sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    sampler_info.mipLodBias = 0.0f;
    // Use the full range of mips available
    sampler_info.minLod = 0.0f;
    // NOTE: Uncomment the following line to test the lowest mip level
    /* sampler_info.minLod = map->texture->mip_levels > 1 ? map->texture->mip_levels : 0.0f; */
    sampler_info.maxLod = VK_LOD_CLAMP_NONE; // Don't clamp

    VkResult result = vkCreateSampler(context->device.logical_device, &sampler_info, context->allocator, &out_sampler_handle_data->sampler);
    if (!vulkan_result_is_success(VK_SUCCESS))
    {
        BERROR("Error creating sampler: %s", vulkan_result_string(result, true));
        return false;
    }

    return true;
}

bhandle vulkan_renderer_sampler_acquire(renderer_backend_interface* backend, texture_filter filter, texture_repeat repeat, f32 anisotropy)
{
    vulkan_context* context = (vulkan_context*)backend->internal_context;
    
    // Find a free sampler slot
    u32 length = darray_length(context->samplers);
    u32 selected_id = INVALID_ID;
    for (u32 i = 0; i < length; ++i)
    {
        if (context->samplers[i].sampler == 0)
        {
            selected_id = i;
            break;
        }
    }
    if (selected_id == INVALID_ID)
    {
        // Push an empty entry into the array
        vulkan_sampler_handle_data empty = (vulkan_sampler_handle_data){INVALID_ID_U64, 0};
        darray_push(context->samplers, empty);
        selected_id = length;
    }

    if (!sampler_create_internal(context, filter, repeat, anisotropy, &context->samplers[selected_id]))
        return bhandle_invalid();

    bhandle h = bhandle_create(selected_id);
    // Save off the uniqueid for handle validation
    context->samplers[selected_id].handle_uniqueid = h.unique_id.uniqueid;

    return h;
}

void vulkan_renderer_sampler_release(renderer_backend_interface* backend, bhandle* sampler)
{
    vulkan_context* context = (vulkan_context*)backend->internal_context;
    if (!bhandle_is_invalid(*sampler))
    {
        vulkan_sampler_handle_data* s = &context->samplers[sampler->handle_index];
        if (s->sampler && s->handle_uniqueid == sampler->unique_id.uniqueid)
        {
            // Make sure there's no way this is in use
            vkDeviceWaitIdle(context->device.logical_device);
            vkDestroySampler(context->device.logical_device, s->sampler, context->allocator);
            // Invalidate the entry and the handle
            s->sampler = 0;
            s->handle_uniqueid = INVALID_ID_U64;
            bhandle_invalidate(sampler);
        }
    }
}

b8 vulkan_renderer_sampler_refresh(renderer_backend_interface* backend, bhandle* sampler, texture_filter filter, texture_repeat repeat, f32 anisotropy, u32 mip_levels)
{
    vulkan_context* context = (vulkan_context*)backend->internal_context;
    if (bhandle_is_invalid(*sampler))
    {
        BERROR("Attempted to refresh a sampler via an invalid handler");
        return false;
    }

    vulkan_sampler_handle_data* s = &context->samplers[sampler->handle_index];
    if (s->sampler && s->handle_uniqueid == sampler->unique_id.uniqueid)
    {
        // Take a copy of the old sampler
        VkSampler old = s->sampler;

        // Make sure there's no way this is in use
        vkDeviceWaitIdle(context->device.logical_device);

        // Create/assign the new
        if (!sampler_create_internal(context, filter, repeat, anisotropy, s))
        {
            BERROR("Sampler refresh failed to create new internal sampler");
            return false;
        }

        // Destroy the old
        vkDestroySampler(context->device.logical_device, old, context->allocator);

        // Update the handle and handle data
        sampler->unique_id = identifier_create();
        s->handle_uniqueid = sampler->unique_id.uniqueid;
    }

    return true;
}

b8 vulkan_renderer_shader_per_group_resources_acquire(renderer_backend_interface* backend, struct shader* s, u32* out_group_id)
{
    vulkan_context* context = (vulkan_context*)backend->internal_context;
    return setup_frequency_state(context, s, SHADER_UPDATE_FREQUENCY_PER_GROUP, out_group_id);
}

b8 vulkan_renderer_shader_per_draw_resources_acquire(renderer_backend_interface* backend, struct shader* s, u32* out_per_draw_id)
{
    vulkan_context* context = (vulkan_context*)backend->internal_context;
    return setup_frequency_state(context, s, SHADER_UPDATE_FREQUENCY_PER_DRAW, out_per_draw_id);
}

b8 vulkan_renderer_shader_per_group_resources_release(renderer_backend_interface* backend, bshader* s, u32 per_group_id)
{
    vulkan_context* context = (vulkan_context*)backend->internal_context;
    return release_frequency_state(context, s, SHADER_UPDATE_FREQUENCY_PER_GROUP, per_group_id);
}

b8 vulkan_renderer_shader_per_draw_resources_release(renderer_backend_interface* backend, bshader* s, u32 per_draw_id)
{
    vulkan_context* context = (vulkan_context*)backend->internal_context;
    return release_frequency_state(context, s, SHADER_UPDATE_FREQUENCY_PER_DRAW, per_draw_id);
}

static b8 texture_state_try_set(vulkan_uniform_texture_state* texture_uniforms, u32 texture_count, u16 uniform_location, u32 array_index, bhandle value)
{
    // Find the texture uniform state to update
    for (u32 i = 0; i < texture_count; ++i)
    {
        vulkan_uniform_texture_state* texture_state = &texture_uniforms[i];
        if (texture_state->uniform.location == uniform_location)
        {
            u32 index = (texture_state->uniform.array_length > 1) ? array_index : 0;
            if (index >= texture_state->uniform.array_length)
            {
                BERROR("vulkan_renderer_uniform_set error: index (%u) is out of range (0-%u)", index, texture_state->uniform.array_length);
                return false;
            }
            if (!texture_state->texture_handles)
            {
                BFATAL("Textures array not setup. Check implementation");
            }
            texture_state->texture_handles[array_index] = value;
            return true;
        }
    }
    BERROR("sampler_state_try_set: Unable to find uniform location %u. Sampler uniform not set", uniform_location);
    return false;
}

b8 vulkan_renderer_uniform_set(renderer_backend_interface* backend, bshader* s, shader_uniform* uniform, u32 array_index, const void* value)
{
    vulkan_shader* internal = s->internal_data;
    if (uniform_type_is_texture(uniform->type))
    {
        vulkan_shader_frequency_state* frequency_state = 0;
        u32 uniform_texture_count = 0;
        switch (uniform->frequency) {
        case SHADER_UPDATE_FREQUENCY_PER_FRAME: {
            frequency_state = &internal->per_frame_state;
            uniform_texture_count = s->per_frame.uniform_texture_count;
        }
        case SHADER_UPDATE_FREQUENCY_PER_GROUP: {
            if (s->per_group.bound_id == INVALID_ID)
            {
                BERROR("Trying to set an per_group-level uniform without having bound an per-group first");
                return false;
            }
            frequency_state = &internal->group_states[s->per_group.bound_id];
            uniform_texture_count = s->per_group.uniform_texture_count;
        }
        case SHADER_UPDATE_FREQUENCY_PER_DRAW: {
            if (s->per_group.bound_id == INVALID_ID)
            {
                BERROR("Trying to set a per_draw-level uniform without having bound an per-draw id first");
                return false;
            }
            frequency_state = &internal->per_draw_states[s->per_draw.bound_id];
            uniform_texture_count = s->per_draw.uniform_texture_count;
        }
        }

        bresource_texture* tex_value = (bresource_texture*)value;
        return texture_state_try_set(frequency_state->texture_states, uniform_texture_count, uniform->location, array_index, tex_value->renderer_texture_handle);
    }
    else if (uniform_type_is_sampler(uniform->type))
    {
        BERROR("vulkan_renderer_uniform_set - cannot set sampler uniform directly");
        return false;
    }
    else
    {
        u64 addr;
        u64 ubo_offset = 0;
        u32 image_index = ((vulkan_context*)backend->internal_context)->current_window->renderer_state->backend_state->image_index;
        switch (uniform->frequency)
        {
        case SHADER_UPDATE_FREQUENCY_PER_DRAW:
            if (s->per_draw.bound_id == INVALID_ID)
            {
                BERROR("An per_draw id must be bound before setting a per_draw uniform");
                return false;
            }
            addr = (u64)internal->per_draw_push_constant_block;
            break;
        case SHADER_UPDATE_FREQUENCY_PER_GROUP:
            if (s->per_draw.bound_id == INVALID_ID)
            {
                BERROR("An per-group must be bound before setting an per-group uniform");
                return false;
            }
            addr = (u64)internal->mapped_uniform_buffer_blocks[image_index];
            vulkan_shader_frequency_state* group_state = &internal->group_states[s->per_draw.bound_id];
            ubo_offset = group_state->offset;
            break;
        case SHADER_UPDATE_FREQUENCY_PER_FRAME:
        default:
            addr = (u64)internal->mapped_uniform_buffer_blocks[image_index];
            ubo_offset = s->per_frame.ubo_offset;
            break;
        }
        addr += ubo_offset + uniform->offset + (uniform->size * array_index);
        bcopy_memory((void*)addr, value, uniform->size);
    }
    return true;
}

static b8 create_shader_module(vulkan_context* context, bshader* s, shader_stage stage, const char* source, const char* filename, vulkan_shader_stage* out_stage)
{
    shaderc_shader_kind shader_kind;
    VkShaderStageFlagBits vulkan_stage;
    switch (stage)
    {
    case SHADER_STAGE_VERTEX:
        shader_kind = shaderc_glsl_default_vertex_shader;
        vulkan_stage = VK_SHADER_STAGE_VERTEX_BIT;
        break;
    case SHADER_STAGE_FRAGMENT:
        shader_kind = shaderc_glsl_default_fragment_shader;
        vulkan_stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        break;
    case SHADER_STAGE_COMPUTE:
        shader_kind = shaderc_glsl_default_compute_shader;
        vulkan_stage = VK_SHADER_STAGE_COMPUTE_BIT;
        break;
    case SHADER_STAGE_GEOMETRY:
        shader_kind = shaderc_glsl_default_geometry_shader;
        vulkan_stage = VK_SHADER_STAGE_GEOMETRY_BIT;
        break;
    default:
        BERROR("Unsupported shader kind. Unable to create module");
        return false;
    }

    BDEBUG("Compiling stage '%s' for shader '%s'...", shader_stage_to_string(stage), s->name);

    // Attempt to compile the shader
    shaderc_compilation_result_t compilation_result = shaderc_compile_into_spv(
        context->shader_compiler,
        source,
        string_length(source),
        shader_kind,
        filename,
        "main",
        0);

    if (!compilation_result)
    {
        BERROR("An unknown error occurred while trying to compile the shader. Unable to process futher");
        return false;
    }
    shaderc_compilation_status status = shaderc_result_get_compilation_status(compilation_result);

    // Handle errors
    if (status != shaderc_compilation_status_success)
    {
        const char* error_message = shaderc_result_get_error_message(compilation_result);
        u64 error_count = shaderc_result_get_num_errors(compilation_result);
        BERROR("Error compiling shader with %llu errors", error_count);
        BERROR("Error(s):\n%s", error_message);
        shaderc_result_release(compilation_result);
        return false;
    }

    BDEBUG("Shader compiled successfully");

    // Output warnings if there are any
    u64 warning_count = shaderc_result_get_num_warnings(compilation_result);
    if (warning_count)
        BWARN("%llu warnings were generated during shader compilation:\n%s", warning_count, shaderc_result_get_error_message(compilation_result));

    // Extract the data from result
    const char* bytes = shaderc_result_get_bytes(compilation_result);
    size_t result_length = shaderc_result_get_length(compilation_result);
    // Take a copy of the result data and cast it to a u32* as is required by Vulkan
    u32* code = ballocate(result_length, MEMORY_TAG_RENDERER);
    bcopy_memory(code, bytes, result_length);

    // Release the compilation result
    shaderc_result_release(compilation_result);

    bzero_memory(&out_stage->create_info, sizeof(VkShaderModuleCreateInfo));
    out_stage->create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    out_stage->create_info.codeSize = result_length;
    out_stage->create_info.pCode = code;

    VK_CHECK(vkCreateShaderModule(context->device.logical_device, &out_stage->create_info, context->allocator, &out_stage->handle));

    // Release copy of the code
    bfree(code, result_length, MEMORY_TAG_RENDERER);

    // Shader stage info
    bzero_memory(&out_stage->shader_stage_create_info, sizeof(VkPipelineShaderStageCreateInfo));
    out_stage->shader_stage_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    out_stage->shader_stage_create_info.stage = vulkan_stage;
    out_stage->shader_stage_create_info.module = out_stage->handle;
    out_stage->shader_stage_create_info.pName = "main";

    return true;
}

b8 vulkan_renderer_is_multithreaded(renderer_backend_interface* backend)
{
    vulkan_context* context = (vulkan_context*)backend->internal_context;
    return context->multithreading_enabled;
}

b8 vulkan_renderer_flag_enabled_get(renderer_backend_interface* backend, renderer_config_flags flag)
{
    vulkan_context* context = (vulkan_context*)backend->internal_context;
    return (context->current_window->renderer_state->backend_state->swapchain.flags & flag);
}

void vulkan_renderer_flag_enabled_set(renderer_backend_interface* backend, renderer_config_flags flag, b8 enabled)
{
    vulkan_context* context = (vulkan_context*)backend->internal_context;
    vulkan_swapchain* swapchain = &context->current_window->renderer_state->backend_state->swapchain;
    swapchain->flags = (enabled ? (swapchain->flags | flag) : (swapchain->flags & ~flag));
    context->render_flag_changed = true;
}

// NOTE: Begin vulkan buffer
static b8 vulkan_buffer_is_device_local(renderer_backend_interface* backend, vulkan_buffer* buffer)
{
    return (buffer->memory_property_flags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) == VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
}

static b8 vulkan_buffer_is_host_visible(renderer_backend_interface* backend, vulkan_buffer* buffer)
{
    return (buffer->memory_property_flags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) == VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
}

static b8 vulkan_buffer_is_host_coherent(renderer_backend_interface* backend, vulkan_buffer* buffer)
{
    return (buffer->memory_property_flags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) == VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
}

b8 vulkan_buffer_create_internal(renderer_backend_interface* backend, renderbuffer* buffer)
{
    vulkan_context* context = (vulkan_context*)backend->internal_context;
    if (!buffer)
    {
        BERROR("vulkan_buffer_create_internal requires a valid pointer to a buffer");
        return false;
    }

    vulkan_buffer internal_buffer;

    switch (buffer->type)
    {
        case RENDERBUFFER_TYPE_VERTEX:
            internal_buffer.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
            internal_buffer.memory_property_flags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
            break;
        case RENDERBUFFER_TYPE_INDEX:
            internal_buffer.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
            internal_buffer.memory_property_flags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
            break;
        case RENDERBUFFER_TYPE_UNIFORM:
        {
            u32 device_local_bits = context->device.supports_device_local_host_visible ? VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT : 0;
            internal_buffer.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            internal_buffer.memory_property_flags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | device_local_bits;
        } break;
        case RENDERBUFFER_TYPE_STAGING:
            internal_buffer.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
            internal_buffer.memory_property_flags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
            break;
        case RENDERBUFFER_TYPE_READ:
            internal_buffer.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
            internal_buffer.memory_property_flags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
            break;
        case RENDERBUFFER_TYPE_STORAGE:
            BERROR("Storage buffer not yet supported");
            return false;
        default:
            BERROR("Unsupported buffer type: %i", buffer->type);
            return false;
    }

    VkBufferCreateInfo buffer_info = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    buffer_info.size = buffer->total_size;
    buffer_info.usage = internal_buffer.usage;
    buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;  // NOTE: Only used in one queue

    VK_CHECK(vkCreateBuffer(context->device.logical_device, &buffer_info, context->allocator, &internal_buffer.handle));

    // Gather memory requirements
    vkGetBufferMemoryRequirements(context->device.logical_device, internal_buffer.handle, &internal_buffer.memory_requirements);
    internal_buffer.memory_index = context->find_memory_index(context, internal_buffer.memory_requirements.memoryTypeBits, internal_buffer.memory_property_flags);
    if (internal_buffer.memory_index == -1)
    {
        BERROR("Unable to create vulkan buffer because the required memory type index was not found");
        return false;
    }

    // Allocate memory info
    VkMemoryAllocateInfo allocate_info = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    allocate_info.allocationSize = internal_buffer.memory_requirements.size;
    allocate_info.memoryTypeIndex = (u32)internal_buffer.memory_index;

    // Allocate memory
    VkResult result = vkAllocateMemory(context->device.logical_device, &allocate_info, context->allocator, &internal_buffer.memory);
    if (!vulkan_result_is_success(result))
    {
        BERROR("Failed to allocate memory for buffer with error: %s", vulkan_result_string(result, true));
        return false;
    }
    VK_SET_DEBUG_OBJECT_NAME(context, VK_OBJECT_TYPE_DEVICE_MEMORY, internal_buffer.memory, buffer->name);

    // Determine if memory is on device heap
    b8 is_device_memory = (internal_buffer.memory_property_flags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) == VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

    // Report memory as in-use
    ballocate_report(internal_buffer.memory_requirements.size, is_device_memory ? MEMORY_TAG_GPU_LOCAL : MEMORY_TAG_VULKAN);

    if (result != VK_SUCCESS)
    {
        BERROR("Unable to create vulkan buffer because the required memory allocation failed. Error: %i", result);
        return false;
    }

    // Allocate internal state block of memory at the end once we are sure everything was created successfully
    buffer->internal_data = ballocate(sizeof(vulkan_buffer), MEMORY_TAG_VULKAN);
    *((vulkan_buffer*)buffer->internal_data) = internal_buffer;

    return true;
}

void vulkan_buffer_destroy_internal(renderer_backend_interface* backend, renderbuffer* buffer)
{
    vulkan_context* context = (vulkan_context*)backend->internal_context;
    vkDeviceWaitIdle(context->device.logical_device);
    if (buffer)
    {
        vulkan_buffer* internal_buffer = (vulkan_buffer*)buffer->internal_data;
        if (internal_buffer)
        {
            if (internal_buffer->memory)
            {
                vkFreeMemory(context->device.logical_device, internal_buffer->memory, context->allocator);
                internal_buffer->memory = 0;
            }
            if (internal_buffer->handle)
            {
                vkDestroyBuffer(context->device.logical_device, internal_buffer->handle, context->allocator);
                internal_buffer->handle = 0;
            }

            // Report free memory
            b8 is_device_memory = (internal_buffer->memory_property_flags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) == VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
            bfree_report(internal_buffer->memory_requirements.size, is_device_memory ? MEMORY_TAG_GPU_LOCAL : MEMORY_TAG_VULKAN);
            bzero_memory(&internal_buffer->memory_requirements, sizeof(VkMemoryRequirements));

            internal_buffer->usage = 0;
            internal_buffer->is_locked = false;

            // Free up internal buffer
            bfree(buffer->internal_data, sizeof(vulkan_buffer), MEMORY_TAG_VULKAN);
            buffer->internal_data = 0;
        }
    }
}

b8 vulkan_buffer_resize(renderer_backend_interface* backend, renderbuffer* buffer, u64 new_size)
{
    vulkan_context* context = (vulkan_context*)backend->internal_context;
    if (!buffer || !buffer->internal_data)
        return false;

    vulkan_buffer* internal_buffer = (vulkan_buffer*)buffer->internal_data;

    // Create new buffer
    VkBufferCreateInfo buffer_info = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    buffer_info.size = new_size;
    buffer_info.usage = internal_buffer->usage;
    buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;  // NOTE: Only used in one queue

    VkBuffer new_buffer;
    VK_CHECK(vkCreateBuffer(context->device.logical_device, &buffer_info, context->allocator, &new_buffer));

    // Gather memory requirements
    VkMemoryRequirements requirements;
    vkGetBufferMemoryRequirements(context->device.logical_device, new_buffer, &requirements);

    // Allocate memory info
    VkMemoryAllocateInfo allocate_info = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    allocate_info.allocationSize = requirements.size;
    allocate_info.memoryTypeIndex = (u32)internal_buffer->memory_index;

    // Allocate memory
    VkDeviceMemory new_memory;
    VkResult result = vkAllocateMemory(context->device.logical_device, &allocate_info, context->allocator, &new_memory);
    if (result != VK_SUCCESS)
    {
        BERROR("Unable to resize vulkan buffer because the required memory allocation failed. Error: %i", result);
        return false;
    }
    VK_SET_DEBUG_OBJECT_NAME(context, VK_OBJECT_TYPE_DEVICE_MEMORY, new_memory, buffer->name);

    // Bind the new buffer's memory
    VK_CHECK(vkBindBufferMemory(context->device.logical_device, new_buffer, new_memory, 0));

    // Copy over data
    vulkan_buffer_copy_range_internal(context, internal_buffer->handle, 0, new_buffer, 0, buffer->total_size, false);

    // Make sure anything potentially using these is finished
    vkDeviceWaitIdle(context->device.logical_device);

    // Destroy old
    if (internal_buffer->memory)
    {
        vkFreeMemory(context->device.logical_device, internal_buffer->memory, context->allocator);
        internal_buffer->memory = 0;
    }
    if (internal_buffer->handle)
    {
        vkDestroyBuffer(context->device.logical_device, internal_buffer->handle, context->allocator);
        internal_buffer->handle = 0;
    }

    // Report free of old, allocate of new
    b8 is_device_memory = (internal_buffer->memory_property_flags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) == VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

    bfree_report(internal_buffer->memory_requirements.size, is_device_memory ? MEMORY_TAG_GPU_LOCAL : MEMORY_TAG_VULKAN);
    internal_buffer->memory_requirements = requirements;
    ballocate_report(internal_buffer->memory_requirements.size, is_device_memory ? MEMORY_TAG_GPU_LOCAL : MEMORY_TAG_VULKAN);

    // Set new properties
    internal_buffer->memory = new_memory;
    internal_buffer->handle = new_buffer;

    return true;
}

b8 vulkan_buffer_bind(renderer_backend_interface* backend, renderbuffer* buffer, u64 offset)
{
    vulkan_context* context = (vulkan_context*)backend->internal_context;
    if (!buffer || !buffer->internal_data)
    {
        BERROR("vulkan_buffer_bind requires valid pointer to a buffer");
        return false;
    }
    vulkan_buffer* internal_buffer = (vulkan_buffer*)buffer->internal_data;
    VK_CHECK(vkBindBufferMemory(context->device.logical_device, internal_buffer->handle, internal_buffer->memory, offset));
    return true;
}

b8 vulkan_buffer_unbind(renderer_backend_interface* backend, renderbuffer* buffer)
{
    if (!buffer || !buffer->internal_data)
    {
        BERROR("vulkan_buffer_unbind requires valid pointer to a buffer");
        return false;
    }

    return true;
}

void* vulkan_buffer_map_memory(renderer_backend_interface* backend, renderbuffer* buffer, u64 offset, u64 size)
{
    vulkan_context* context = (vulkan_context*)backend->internal_context;
    if (!buffer || !buffer->internal_data)
    {
        BERROR("vulkan_buffer_map_memory requires a valid pointer to a buffer");
        return 0;
    }
    vulkan_buffer* internal_buffer = (vulkan_buffer*)buffer->internal_data;
    void* data;
    VK_CHECK(vkMapMemory(context->device.logical_device, internal_buffer->memory, offset, size, 0, &data));
    return data;
}

void vulkan_buffer_unmap_memory(renderer_backend_interface* backend, renderbuffer* buffer, u64 offset, u64 size)
{
    vulkan_context* context = (vulkan_context*)backend->internal_context;
    if (!buffer || !buffer->internal_data)
    {
        BERROR("vulkan_buffer_unmap_memory requires a valid pointer to a buffer");
        return;
    }
    vulkan_buffer* internal_buffer = (vulkan_buffer*)buffer->internal_data;
    vkUnmapMemory(context->device.logical_device, internal_buffer->memory);
}

b8 vulkan_buffer_flush(renderer_backend_interface* backend, renderbuffer* buffer, u64 offset, u64 size)
{
    vulkan_context* context = (vulkan_context*)backend->internal_context;
    if (!buffer || !buffer->internal_data)
    {
        BERROR("vulkan_buffer_flush requires a valid pointer to a buffer");
        return false;
    }
    // NOTE: If not host-coherent, flush mapped memory range
    vulkan_buffer* internal_buffer = (vulkan_buffer*)buffer->internal_data;
    if (!vulkan_buffer_is_host_coherent(backend, internal_buffer))
    {
        VkMappedMemoryRange range = {VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE};
        range.memory = internal_buffer->memory;
        range.offset = offset;
        range.size = size;
        VK_CHECK(vkFlushMappedMemoryRanges(context->device.logical_device, 1, &range));
    }

    return true;
}

b8 vulkan_buffer_read(renderer_backend_interface* backend, renderbuffer* buffer, u64 offset, u64 size, void** out_memory)
{
    vulkan_context* context = (vulkan_context*)backend->internal_context;
    if (!buffer || !buffer->internal_data || !out_memory)
    {
        BERROR("vulkan_buffer_read requires a valid pointer to a buffer and out_memory, and the size must be nonzero");
        return false;
    }

    vulkan_buffer* internal_buffer = (vulkan_buffer*)buffer->internal_data;
    if (vulkan_buffer_is_device_local(backend, internal_buffer) && !vulkan_buffer_is_host_visible(backend, internal_buffer))
    {
        // Create host-visible staging buffer to copy to. Mark it as destination of the transfer
        renderbuffer read;
        if (!renderer_renderbuffer_create("renderbuffer_read", RENDERBUFFER_TYPE_READ, size, RENDERBUFFER_TRACK_TYPE_NONE, &read))
        {
            BERROR("vulkan_buffer_read() - Failed to create read buffer");
            return false;
        }
        renderer_renderbuffer_bind(&read, 0);
        vulkan_buffer* read_internal = (vulkan_buffer*)read.internal_data;

        // Perform copy from device local to read buffer
        vulkan_buffer_copy_range(backend, buffer, offset, &read, 0, size, true);

        // Map/copy/unmap
        void* mapped_data;
        VK_CHECK(vkMapMemory(context->device.logical_device, read_internal->memory, 0, size, 0, &mapped_data));
        bcopy_memory(*out_memory, mapped_data, size);
        vkUnmapMemory(context->device.logical_device, read_internal->memory);

        // Clean up read buffer
        renderer_renderbuffer_unbind(&read);
        renderer_renderbuffer_destroy(&read);
    }
    else
    {
        // If no staging buffer is needed, map/copy/unmap
        void* data_ptr;
        VK_CHECK(vkMapMemory(context->device.logical_device, internal_buffer->memory, offset, size, 0, &data_ptr));
        bcopy_memory(*out_memory, data_ptr, size);
        vkUnmapMemory(context->device.logical_device, internal_buffer->memory);
    }

    return true;
}

b8 vulkan_buffer_load_range(renderer_backend_interface* backend, renderbuffer* buffer, u64 offset, u64 size, const void* data, b8 include_in_frame_workload)
{
    vulkan_context* context = (vulkan_context*)backend->internal_context;
    if (!buffer || !buffer->internal_data || !size || !data)
    {
        BERROR("vulkan_buffer_load_range requires a valid pointer to a buffer, a nonzero size and a valid pointer to data");
        return false;
    }

    vulkan_buffer* internal_buffer = (vulkan_buffer*)buffer->internal_data;
    if (vulkan_buffer_is_device_local(backend, internal_buffer) && !vulkan_buffer_is_host_visible(backend, internal_buffer))
    {
        // Load data into staging buffer
        u64 staging_offset = 0;
        renderbuffer* staging = &context->current_window->renderer_state->backend_state->staging[get_current_frame_index(context)];
        renderer_renderbuffer_allocate(staging, size, &staging_offset);
        vulkan_buffer_load_range(backend, staging, staging_offset, size, data, include_in_frame_workload);

        // Perform copy from staging to device local buffer
        vulkan_buffer_copy_range(backend, staging, staging_offset, buffer, offset, size, include_in_frame_workload);
    }
    else
    {
        // If no staging buffer is needed, map/copy/unmap
        void* data_ptr;
        VK_CHECK(vkMapMemory(context->device.logical_device, internal_buffer->memory, offset, size, 0, &data_ptr));
        bcopy_memory(data_ptr, data, size);
        vkUnmapMemory(context->device.logical_device, internal_buffer->memory);
    }

    return true;
}

static b8 vulkan_buffer_copy_range_internal(vulkan_context* context, VkBuffer source, u64 source_offset, VkBuffer dest, u64 dest_offset, u64 size, b8 include_in_frame_workload)
{
    VkQueue queue = context->device.graphics_queue;
    vulkan_command_buffer temp_command_buffer;
    vulkan_command_buffer* command_buffer = 0;

    // If not including in frame workload, then utilize a new temp command buffer as well. Otherwise this should be done as part of the current frame's work
    if (!include_in_frame_workload)
    {
        vkQueueWaitIdle(queue);
        // Create a one-time-use command buffer
        vulkan_command_buffer_allocate_and_begin_single_use(context, context->device.graphics_command_pool, &temp_command_buffer);
        command_buffer = &temp_command_buffer;
    }
    else
    {
        command_buffer = get_current_command_buffer(context);
    }

    // Prepare copy command and add it to command buffer
    VkBufferCopy copy_region;
    copy_region.srcOffset = source_offset;
    copy_region.dstOffset = dest_offset;
    copy_region.size = size;
    vkCmdCopyBuffer(command_buffer->handle, source, dest, 1, &copy_region);

    if (!include_in_frame_workload)
    {
        // Submit buffer for execution and wait for it to complete
        vulkan_command_buffer_end_single_use(context, context->device.graphics_command_pool, &temp_command_buffer, queue);
    }
    // NOTE: if not waiting, submission will be handled later

    return true;
}

b8 vulkan_buffer_copy_range(renderer_backend_interface* backend, renderbuffer* source, u64 source_offset, renderbuffer* dest, u64 dest_offset, u64 size, b8 include_in_frame_workload)
{
    vulkan_context* context = (vulkan_context*)backend->internal_context;
    if (!source || !source->internal_data || !dest || !dest->internal_data || !size)
    {
        BERROR("vulkan_buffer_copy_range requires a valid pointers to source and destination buffers as well as a nonzero size");
        return false;
    }

    return vulkan_buffer_copy_range_internal(
        context,
        ((vulkan_buffer*)source->internal_data)->handle, source_offset,
        ((vulkan_buffer*)dest->internal_data)->handle, dest_offset, size, include_in_frame_workload);
    return true;
}

b8 vulkan_buffer_draw(renderer_backend_interface* backend, renderbuffer* buffer, u64 offset, u32 element_count, b8 bind_only)
{
    vulkan_context* context = (vulkan_context*)backend->internal_context;
    vulkan_command_buffer* command_buffer = get_current_command_buffer(context);

    if (buffer->type == RENDERBUFFER_TYPE_VERTEX)
    {
        // Bind vertex buffer at offset
        VkDeviceSize offsets[1] = {offset};
        vkCmdBindVertexBuffers(command_buffer->handle, 0, 1, &((vulkan_buffer*)buffer->internal_data)->handle, offsets);
        if (!bind_only)
            vkCmdDraw(command_buffer->handle, element_count, 1, 0, 0);
        return true;
    }
    else if (buffer->type == RENDERBUFFER_TYPE_INDEX)
    {
        // Bind index buffer at offset
        vkCmdBindIndexBuffer(command_buffer->handle, ((vulkan_buffer*)buffer->internal_data)->handle, offset, VK_INDEX_TYPE_UINT32);
        if (!bind_only)
            vkCmdDrawIndexed(command_buffer->handle, element_count, 1, 0, 0, 0);
        return true;
    }
    else
    {
        BERROR("Cannot draw buffer of type: %i", buffer->type);
        return false;
    }
}

void vulkan_renderer_wait_for_idle(renderer_backend_interface* backend)
{
    if (backend)
    {
        vulkan_context* context = backend->internal_context;
        VK_CHECK(vkDeviceWaitIdle(context->device.logical_device));
    }
}

static vulkan_command_buffer* get_current_command_buffer(vulkan_context* context)
{
    bwindow_renderer_backend_state* window_backend = context->current_window->renderer_state->backend_state;
    vulkan_command_buffer* primary = &window_backend->graphics_command_buffers[window_backend->image_index];

    // If inside a "render", return the secondary buffer at the current index
    if (primary->in_render)
    {
        if (!primary->secondary_buffers)
        {
            BWARN("get_current_command_buffer requested draw index, but no secondary buffers exist");
            return primary;
        }
        else
        {
            if (primary->secondary_buffer_index >= primary->secondary_count)
            {
                BWARN("get_current_command_buffer specified a draw index (%d) outside the bounds of 0-%d. Returning the first one, which may result in errors", primary->secondary_buffer_index, primary->secondary_count - 1);
                return &primary->secondary_buffers[0];
            }
            else
            {
                return &primary->secondary_buffers[primary->secondary_buffer_index];
            }
        }
    }
    else
    {
        return primary;
    }
}

static u32 get_current_image_index(vulkan_context* context)
{
    return context->current_window->renderer_state->backend_state->image_index;
}

static u32 get_current_frame_index(vulkan_context* context)
{
    return context->current_window->renderer_state->backend_state->current_frame;
}

static u32 get_image_count(vulkan_context* context)
{
    return context->current_window->renderer_state->backend_state->swapchain.image_count;
}

static b8 vulkan_graphics_pipeline_create(vulkan_context* context, const vulkan_pipeline_config* config, vulkan_pipeline* out_pipeline)
{
    // Viewport state
    VkPipelineViewportStateCreateInfo viewport_state = {VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
    viewport_state.viewportCount = 1;
    viewport_state.pViewports = &config->viewport;
    viewport_state.scissorCount = 1;
    viewport_state.pScissors = &config->scissor;

    // Rasterizer
    VkPipelineRasterizationStateCreateInfo rasterizer_create_info = {VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
    rasterizer_create_info.depthClampEnable = VK_FALSE;
    rasterizer_create_info.rasterizerDiscardEnable = VK_FALSE;
    rasterizer_create_info.polygonMode = (config->shader_flags & SHADER_FLAG_WIREFRAME) ? VK_POLYGON_MODE_LINE : VK_POLYGON_MODE_FILL;
    rasterizer_create_info.lineWidth = 1.0f;
    switch (config->cull_mode)
    {
    case FACE_CULL_MODE_NONE:
        rasterizer_create_info.cullMode = VK_CULL_MODE_NONE;
        break;
    case FACE_CULL_MODE_FRONT:
        rasterizer_create_info.cullMode = VK_CULL_MODE_FRONT_BIT;
        break;
    default:
    case FACE_CULL_MODE_BACK:
        rasterizer_create_info.cullMode = VK_CULL_MODE_BACK_BIT;
        break;
    case FACE_CULL_MODE_FRONT_AND_BACK:
        rasterizer_create_info.cullMode = VK_CULL_MODE_FRONT_AND_BACK;
        break;
    }

    if (config->winding == RENDERER_WINDING_CLOCKWISE)
    {
        rasterizer_create_info.frontFace = VK_FRONT_FACE_CLOCKWISE;
    }
    else if (config->winding == RENDERER_WINDING_COUNTER_CLOCKWISE)
    {
        rasterizer_create_info.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    }
    else
    {
        BWARN("Invalid front-face winding order specified, default to counter-clockwise");
        rasterizer_create_info.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    }
    rasterizer_create_info.depthBiasEnable = VK_FALSE;
    rasterizer_create_info.depthBiasConstantFactor = 0.0f;
    rasterizer_create_info.depthBiasClamp = 0.0f;
    rasterizer_create_info.depthBiasSlopeFactor = 0.0f;

    // Smooth line rasterisation, if supported
    VkPipelineRasterizationLineStateCreateInfoEXT line_rasterization_ext = {0};
    if (context->device.support_flags & VULKAN_DEVICE_SUPPORT_FLAG_LINE_SMOOTH_RASTERISATION_BIT)
    {
        line_rasterization_ext.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_LINE_STATE_CREATE_INFO_EXT;
        line_rasterization_ext.lineRasterizationMode = VK_LINE_RASTERIZATION_MODE_RECTANGULAR_SMOOTH_EXT;
        rasterizer_create_info.pNext = &line_rasterization_ext;
    }

    // Multisampling
    VkPipelineMultisampleStateCreateInfo multisampling_create_info = {VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
    multisampling_create_info.sampleShadingEnable = VK_FALSE;
    multisampling_create_info.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    multisampling_create_info.minSampleShading = 1.0f;
    multisampling_create_info.pSampleMask = 0;
    multisampling_create_info.alphaToCoverageEnable = VK_FALSE;
    multisampling_create_info.alphaToOneEnable = VK_FALSE;

    // Depth and stencil testing
    VkPipelineDepthStencilStateCreateInfo depth_stencil = {VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
    if (config->shader_flags & SHADER_FLAG_DEPTH_TEST)
    {
        depth_stencil.depthTestEnable = VK_TRUE;
        if (config->shader_flags & SHADER_FLAG_DEPTH_WRITE)
            depth_stencil.depthWriteEnable = VK_TRUE;
        depth_stencil.depthCompareOp = VK_COMPARE_OP_LESS;
        depth_stencil.depthBoundsTestEnable = VK_FALSE;
    }
    depth_stencil.stencilTestEnable = (config->shader_flags & SHADER_FLAG_STENCIL_TEST) ? VK_TRUE : VK_FALSE;
    if (config->shader_flags & SHADER_FLAG_STENCIL_TEST)
    {
        // equivalent to glStencilFunc(func, ref, mask)
        depth_stencil.back.compareOp = VK_COMPARE_OP_ALWAYS;
        depth_stencil.back.reference = 1;
        depth_stencil.back.compareMask = 0xFF;

        // equivalent of glStencilOp(stencilFail, depthFail, depthPass)pipeline
        depth_stencil.back.failOp = VK_STENCIL_OP_ZERO;
        depth_stencil.back.depthFailOp = VK_STENCIL_OP_ZERO;
        depth_stencil.back.passOp = VK_STENCIL_OP_REPLACE;
        // equivalent of glStencilMask(mask)

        // Back face
        depth_stencil.back.writeMask = (config->shader_flags & SHADER_FLAG_STENCIL_WRITE) ? 0xFF : 0x00;

        // Front face. Just use the same settings for front/back
        depth_stencil.front = depth_stencil.back;
    }

    VkPipelineColorBlendAttachmentState color_blend_attachment_state;
    bzero_memory(&color_blend_attachment_state, sizeof(VkPipelineColorBlendAttachmentState));
    color_blend_attachment_state.blendEnable = VK_TRUE;
    color_blend_attachment_state.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    color_blend_attachment_state.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    color_blend_attachment_state.colorBlendOp = VK_BLEND_OP_ADD;
    color_blend_attachment_state.srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    color_blend_attachment_state.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    color_blend_attachment_state.alphaBlendOp = VK_BLEND_OP_ADD;

    color_blend_attachment_state.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    VkPipelineColorBlendStateCreateInfo color_blend_state_create_info = {VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
    color_blend_state_create_info.logicOpEnable = VK_FALSE;
    color_blend_state_create_info.logicOp = VK_LOGIC_OP_COPY;
    color_blend_state_create_info.attachmentCount = 1;
    color_blend_state_create_info.pAttachments = &color_blend_attachment_state;

    // Dynamic state
    VkDynamicState* dynamic_states = darray_create(VkDynamicState);
    darray_push(dynamic_states, VK_DYNAMIC_STATE_VIEWPORT);
    darray_push(dynamic_states, VK_DYNAMIC_STATE_SCISSOR);
    // Dynamic state, if supported
    if ((context->device.support_flags & VULKAN_DEVICE_SUPPORT_FLAG_NATIVE_DYNAMIC_STATE_BIT) || (context->device.support_flags & VULKAN_DEVICE_SUPPORT_FLAG_DYNAMIC_STATE_BIT))
    {
        darray_push(dynamic_states, VK_DYNAMIC_STATE_PRIMITIVE_TOPOLOGY);
        darray_push(dynamic_states, VK_DYNAMIC_STATE_FRONT_FACE);
        darray_push(dynamic_states, VK_DYNAMIC_STATE_STENCIL_OP);
        darray_push(dynamic_states, VK_DYNAMIC_STATE_STENCIL_TEST_ENABLE_EXT);
        darray_push(dynamic_states, VK_DYNAMIC_STATE_STENCIL_WRITE_MASK);
        darray_push(dynamic_states, VK_DYNAMIC_STATE_STENCIL_COMPARE_MASK);
        darray_push(dynamic_states, VK_DYNAMIC_STATE_DEPTH_TEST_ENABLE);
        darray_push(dynamic_states, VK_DYNAMIC_STATE_DEPTH_WRITE_ENABLE);
        darray_push(dynamic_states, VK_DYNAMIC_STATE_STENCIL_REFERENCE);
        /* darray_push(dynamic_states, VK_DYNAMIC_STATE_COLOR_WRITE_ENABLE_EXT);
        darray_push(dynamic_states, VK_DYNAMIC_STATE_COLOR_WRITE_MASK_EXT); */
    }

    VkPipelineDynamicStateCreateInfo dynamic_state_create_info = {VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
    dynamic_state_create_info.dynamicStateCount = darray_length(dynamic_states);
    dynamic_state_create_info.pDynamicStates = dynamic_states;

    // Vertex input
    VkVertexInputBindingDescription binding_description;
    binding_description.binding = 0; // Binding index
    binding_description.stride = config->stride;
    binding_description.inputRate = VK_VERTEX_INPUT_RATE_VERTEX; // Move to next data entry for each vertex

    // Attributes
    VkPipelineVertexInputStateCreateInfo vertex_input_info = {VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
    vertex_input_info.vertexBindingDescriptionCount = 1;
    vertex_input_info.pVertexBindingDescriptions = &binding_description;
    vertex_input_info.vertexAttributeDescriptionCount = config->attribute_count;
    vertex_input_info.pVertexAttributeDescriptions = config->attributes;

    // Input assembly
    VkPipelineInputAssemblyStateCreateInfo input_assembly = {VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
    // The pipeline being created already has available types, so just grab the first one
    for (u32 i = 1; i < PRIMITIVE_TOPOLOGY_TYPE_MAX; i = i << 1)
    {
        if (out_pipeline->supported_topology_types & i)
        {
            primitive_topology_type ptt = i;

            switch (ptt)
            {
            case PRIMITIVE_TOPOLOGY_TYPE_POINT_LIST:
                input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
                break;
            case PRIMITIVE_TOPOLOGY_TYPE_LINE_LIST:
                input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
                break;
            case PRIMITIVE_TOPOLOGY_TYPE_LINE_STRIP:
                input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_LINE_STRIP;
                break;
            case PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE_LIST:
                input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
                break;
            case PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE_STRIP:
                input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
                break;
            case PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE_FAN:
                input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN;
                break;
            default:
                BWARN("primitive topology '%u' not supported. Skipping...", ptt);
                break;
            }

            break;
        }
    }
    input_assembly.primitiveRestartEnable = VK_FALSE;

    // Pipeline layout
    VkPipelineLayoutCreateInfo pipeline_layout_create_info = {VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};

    // Push constants
    VkPushConstantRange ranges[32];
    if (config->push_constant_range_count > 0)
    {
        if (config->push_constant_range_count > 32)
        {
            BERROR("vulkan_graphics_pipeline_create: cannot have more than 32 push constant ranges. Passed count: %i", config->push_constant_range_count);
            return false;
        }

        // NOTE: 32 is the max number of ranges we can ever have, since spec only guarantees 128 bytes with 4-byte alignment
        bzero_memory(ranges, sizeof(VkPushConstantRange) * 32);
        for (u32 i = 0; i < config->push_constant_range_count; ++i)
        {
            ranges[i].stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
            ranges[i].offset = config->push_constant_ranges[i].offset;
            ranges[i].size = config->push_constant_ranges[i].size;
        }
        pipeline_layout_create_info.pushConstantRangeCount = config->push_constant_range_count;
        pipeline_layout_create_info.pPushConstantRanges = ranges;
    }
    else
    {
        pipeline_layout_create_info.pushConstantRangeCount = 0;
        pipeline_layout_create_info.pPushConstantRanges = 0;
    }

    // Descriptor set layouts
    pipeline_layout_create_info.setLayoutCount = config->descriptor_set_layout_count;
    pipeline_layout_create_info.pSetLayouts = config->descriptor_set_layouts;

    // Create the pipeline layout
    VK_CHECK(vkCreatePipelineLayout(
        context->device.logical_device,
        &pipeline_layout_create_info,
        context->allocator,
        &out_pipeline->pipeline_layout));

#if _DEBUG
    char* pipeline_layout_name_buf = string_format("pipeline_layout_shader_%s", config->name);
    VK_SET_DEBUG_OBJECT_NAME(context, VK_OBJECT_TYPE_PIPELINE_LAYOUT, out_pipeline->pipeline_layout, pipeline_layout_name_buf);
    string_free(pipeline_layout_name_buf);
#endif

    // Pipeline create
    VkGraphicsPipelineCreateInfo pipeline_create_info = {VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
    pipeline_create_info.stageCount = config->stage_count;
    pipeline_create_info.pStages = config->stages;
    pipeline_create_info.pVertexInputState = &vertex_input_info;
    pipeline_create_info.pInputAssemblyState = &input_assembly;

    pipeline_create_info.pViewportState = &viewport_state;
    pipeline_create_info.pRasterizationState = &rasterizer_create_info;
    pipeline_create_info.pMultisampleState = &multisampling_create_info;
    pipeline_create_info.pDepthStencilState = ((config->shader_flags & SHADER_FLAG_DEPTH_TEST) || (config->shader_flags & SHADER_FLAG_STENCIL_TEST)) ? &depth_stencil : 0;
    pipeline_create_info.pColorBlendState = &color_blend_state_create_info;
    pipeline_create_info.pDynamicState = &dynamic_state_create_info;
    pipeline_create_info.pTessellationState = 0;

    pipeline_create_info.layout = out_pipeline->pipeline_layout;

    pipeline_create_info.renderPass = VK_NULL_HANDLE;
    pipeline_create_info.subpass = 0;
    pipeline_create_info.basePipelineHandle = VK_NULL_HANDLE;
    pipeline_create_info.basePipelineIndex = -1;

    // dynamic rendering
    VkPipelineRenderingCreateInfoKHR pipeline_rendering_create_info = {VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR};
    pipeline_rendering_create_info.pNext = VK_NULL_HANDLE;
    pipeline_rendering_create_info.colorAttachmentCount = config->color_attachment_count;
    pipeline_rendering_create_info.pColorAttachmentFormats = config->color_attachment_formats;
    pipeline_rendering_create_info.depthAttachmentFormat = config->depth_attachment_format;
    pipeline_rendering_create_info.stencilAttachmentFormat = config->stencil_attachment_format;

    pipeline_create_info.pNext = &pipeline_rendering_create_info;

    VkResult result = vkCreateGraphicsPipelines(
        context->device.logical_device,
        VK_NULL_HANDLE,
        1,
        &pipeline_create_info,
        context->allocator,
        &out_pipeline->handle);

    // Cleanup
    darray_destroy(dynamic_states);

#if _DEBUG
    char* pipeline_name_buf = string_format("pipeline_shader_%s", config->name);
    VK_SET_DEBUG_OBJECT_NAME(context, VK_OBJECT_TYPE_PIPELINE, out_pipeline->handle, pipeline_name_buf);
    string_free(pipeline_name_buf);
#endif

    if (vulkan_result_is_success(result))
    {
        BDEBUG("Graphics pipeline created!");
        return true;
    }

    BERROR("vkCreateGraphicsPipelines failed with %s.", vulkan_result_string(result, true));
    return false;
}

static void vulkan_pipeline_destroy(vulkan_context* context, vulkan_pipeline* pipeline)
{
    if (pipeline)
    {
        // Destroy pipeline
        if (pipeline->handle)
        {
            vkDestroyPipeline(context->device.logical_device, pipeline->handle, context->allocator);
            pipeline->handle = 0;
        }

        // Destroy layout
        if (pipeline->pipeline_layout)
        {
            vkDestroyPipelineLayout(context->device.logical_device, pipeline->pipeline_layout, context->allocator);
            pipeline->pipeline_layout = 0;
        }
    }
}

static void vulkan_pipeline_bind(vulkan_command_buffer* command_buffer, VkPipelineBindPoint bind_point, vulkan_pipeline* pipeline)
{
    vkCmdBindPipeline(command_buffer->handle, bind_point, pipeline->handle);
}

static b8 setup_frequency_state(vulkan_context* context, bshader* s, shader_update_frequency frequency, u32* out_frequency_id)
{
    vulkan_shader* internal = s->internal_data;

    u32 image_count = get_image_count(context);

    vulkan_shader_frequency_state* frequency_states = 0;
    u32 max_frequency_count = 0;
    u32 uniform_texture_count = 0;
    u32 uniform_sampler_count = 0;
    u32* sampler_indices = 0;
    u32* texture_indices = 0;
    const char* frequency_text = 0;
    b8 do_ubo_setup = false;
    u8 descriptor_set_index = 0;
    u64 ubo_stride = 0;
    vulkan_shader_frequency_state* frequency_state = 0;

    b8 has_per_frame = s->per_frame.uniform_count > 0 || s->per_frame.uniform_sampler_count > 0;
    b8 has_group = s->per_group.uniform_count > 0 || s->per_group.uniform_sampler_count > 0;
    
    switch (frequency)
    {
    case SHADER_UPDATE_FREQUENCY_PER_FRAME:
        // NOTE: treat single entry as an "array" so the same logic below can be used for it as well
        frequency_states = 0;
        max_frequency_count = 1;
        frequency_text = "per-frame";
        uniform_texture_count = s->per_draw.uniform_texture_count;
        uniform_sampler_count = s->per_frame.uniform_sampler_count;
        sampler_indices = s->per_frame.sampler_indices;
        texture_indices = s->per_frame.texture_indices;
        do_ubo_setup = true;
        descriptor_set_index = 0;
        ubo_stride = s->per_frame.ubo_stride;

    case SHADER_UPDATE_FREQUENCY_PER_GROUP:
        frequency_states = internal->group_states;
        max_frequency_count = internal->max_groups;
        frequency_text = "per-group";
        uniform_texture_count = s->per_group.uniform_texture_count;
        uniform_sampler_count = s->per_group.uniform_sampler_count;
        sampler_indices = s->per_group.sampler_indices;
        texture_indices = s->per_group.texture_indices;
        do_ubo_setup = true;
        descriptor_set_index = has_per_frame ? 1 : 0;
        ubo_stride = s->per_group.ubo_stride;
        break;

    case SHADER_UPDATE_FREQUENCY_PER_DRAW:
        frequency_states = internal->per_draw_states;
        max_frequency_count = internal->max_per_draw_count;
        frequency_text = "per-draw";
        uniform_texture_count = s->per_draw.uniform_texture_count;
        uniform_sampler_count = s->per_draw.uniform_sampler_count;
        sampler_indices = s->per_draw.sampler_indices;
        texture_indices = s->per_draw.texture_indices;
        do_ubo_setup = false;
        descriptor_set_index += has_per_frame ? 1 : 0;
        descriptor_set_index += has_group ? 1 : 0;
        ubo_stride = s->per_draw.ubo_stride;
        break;
    }

    if (frequency != SHADER_UPDATE_FREQUENCY_PER_FRAME)
    {
        frequency_state = &internal->per_frame_state;
    }
    else
    {
        // Obtain an id for the given frequency. An id is not required for the per-frame scope
        *out_frequency_id = INVALID_ID;
        for (u32 i = 0; i < max_frequency_count; ++i)
        {
            if (frequency_states[i].id == INVALID_ID)
            {
                frequency_states[i].id = i;
                *out_frequency_id = i;
                break;
            }
        }
        if (*out_frequency_id == INVALID_ID)
        {
            BERROR("setup_frequency_state failed to acquire new %s id for shader '%s', max %s count=%u", frequency_text, s->name, frequency_text, max_frequency_count);
            return false;
        }
        frequency_state = &frequency_states[*out_frequency_id];
    }

    const bresource_texture* default_bresource_texture = texture_system_get_default_bresource_texture(engine_systems_get()->texture_system);

    // Setup sampler uniform states. Only setup if the shader actually requires it
    if (uniform_sampler_count > 0)
    {
        frequency_state->sampler_states = BALLOC_TYPE_CARRAY(vulkan_uniform_sampler_state, uniform_sampler_count);

        // Assign uniforms to each of the sampler states
        for (u32 ii = 0; ii < uniform_sampler_count; ++ii)
        {
            vulkan_uniform_sampler_state* sampler_state = &frequency_state->sampler_states[ii];
            sampler_state->uniform = s->uniforms[sampler_indices[ii]];

            u32 array_length = BMAX(sampler_state->uniform.array_length, 1);
            // Setup the array for the samplers
            sampler_state->sampler_handles = BALLOC_TYPE_CARRAY(bhandle, array_length);
            // Setup descriptor states
            sampler_state->descriptor_states = BALLOC_TYPE_CARRAY(vulkan_descriptor_state, array_length);
            // Per descriptor
            for (u32 d = 0; d < array_length; ++d)
            {
                // TODO: use a default sampler
                sampler_state->sampler_handles[d] = tc->bresource_texture_maps[d];

                sampler_state->descriptor_states[d].generations = BALLOC_TYPE_CARRAY(u8, image_count);
                sampler_state->descriptor_states[d].ids = BALLOC_TYPE_CARRAY(u32, image_count);
                sampler_state->descriptor_states[d].frame_numbers = BALLOC_TYPE_CARRAY(u64, image_count);
                // Per swapchain image
                for (u32 j = 0; j < image_count; ++j)
                {
                    sampler_state->descriptor_states[d].generations[j] = INVALID_ID_U8;
                    sampler_state->descriptor_states[d].ids[j] = INVALID_ID;
                    sampler_state->descriptor_states[d].frame_numbers[j] = INVALID_ID_U64;
                }
            }
        }
    }

    // Setup texture uniform states. Only setup if the shader actually requires it
    if (uniform_texture_count > 0)
    {
        frequency_state->texture_states = BALLOC_TYPE_CARRAY(vulkan_uniform_texture_state, uniform_texture_count);

        // Assign uniforms to each of the texture states
        for (u32 ii = 0; ii < uniform_texture_count; ++ii)
        {
            vulkan_uniform_texture_state* texture_state = &frequency_state->texture_states[ii];
            texture_state->uniform = s->uniforms[texture_indices[ii]];

            u32 array_length = BMAX(texture_state->uniform.array_length, 1);
            // Setup the array for the textures
            texture_state->texture_handles = BALLOC_TYPE_CARRAY(bhandle, array_length);
            // Setup descriptor states
            texture_state->descriptor_states = BALLOC_TYPE_CARRAY(vulkan_descriptor_state, array_length);
            // Per descriptor
            for (u32 d = 0; d < array_length; ++d)
            {
                // TODO: get default textures
                texture_state->texture_handles[d] = tc->kresource_texture_maps[d];

                texture_state->descriptor_states[d].generations = BALLOC_TYPE_CARRAY(u8, image_count);
                texture_state->descriptor_states[d].ids = BALLOC_TYPE_CARRAY(u32, image_count);
                texture_state->descriptor_states[d].frame_numbers = BALLOC_TYPE_CARRAY(u64, image_count);
                // Per swapchain image
                for (u32 j = 0; j < image_count; ++j)
                {
                    texture_state->descriptor_states[d].generations[j] = INVALID_ID_U8;
                    texture_state->descriptor_states[d].ids[j] = INVALID_ID;
                    texture_state->descriptor_states[d].frame_numbers[j] = INVALID_ID_U64;
                }
            }
        }
    }

    // frequency-level UBO binding, if needed
    VkDescriptorSetLayout* layouts = 0;
    if (do_ubo_setup)
    {
        // Allocate some space in the UBO - by the stride, not the size
        u64 size = ubo_stride;
        if (size > 0)
        {
            for (u32 i = 0; i < internal->uniform_buffer_count; ++i)
            {
                if (!renderer_renderbuffer_allocate(&internal->uniform_buffers[i], size, &frequency_state->offset))
                {
                    BERROR("setup_frequency_state failed to acquire %s ubo space", frequency_text);
                    return false;
                }
            }
        }

        // NOTE: really only matters where there are frequency uniforms, but set them anyway
        frequency_state->ubo_descriptor_state.generations = BALLOC_TYPE_CARRAY(u8, image_count);
        frequency_state->ubo_descriptor_state.ids = BALLOC_TYPE_CARRAY(u32, image_count);
        frequency_state->ubo_descriptor_state.frame_numbers = BALLOC_TYPE_CARRAY(u64, image_count);
        frequency_state->descriptor_sets = BALLOC_TYPE_CARRAY(VkDescriptorSet, image_count);

        // Temp array for descriptor set layouts
        layouts = BALLOC_TYPE_CARRAY(VkDescriptorSetLayout, image_count);

        // Per swapchain image
        for (u32 j = 0; j < image_count; ++j)
        {
            // Invalidate descriptor state
            frequency_state->ubo_descriptor_state.generations[j] = INVALID_ID_U8;
            frequency_state->ubo_descriptor_state.ids[j] = INVALID_ID_U8;
            frequency_state->ubo_descriptor_state.frame_numbers[j] = INVALID_ID_U64;

            // Set descriptor set layout for this index
            layouts[j] = internal->descriptor_set_layouts[descriptor_set_index];
        }
    }

    b8 final_result = true;
    VkDescriptorSetAllocateInfo alloc_info = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    alloc_info.descriptorPool = internal->descriptor_pool;
    alloc_info.descriptorSetCount = image_count;
    alloc_info.pSetLayouts = layouts;
    VkResult result = vkAllocateDescriptorSets(context->device.logical_device, &alloc_info, frequency_state->descriptor_sets);
    if (result != VK_SUCCESS)
    {
        BERROR("Error allocating %s descriptor sets in shader: '%s'", frequency_text, vulkan_result_string(result, true));
        final_result = false;
    }

#ifdef BISMUTH_DEBUG
    // Assign a debug name to the descriptor set
    for (u32 i = 0; i < image_count; ++i)
    {
        char* desc_set_object_name = string_format("desc_set_shader_%s_%s_%u_frame_%u", s->name, frequency_text, *out_frequency_id, i);
        VK_SET_DEBUG_OBJECT_NAME(context, VK_OBJECT_TYPE_DESCRIPTOR_SET, frequency_state->descriptor_sets[i], desc_set_object_name);
        string_free(desc_set_object_name);
    }
#endif

    if (layouts)
    {
        // Clean up temp array
        BFREE_TYPE_CARRAY(layouts, VkDescriptorSetLayout, image_count);
    }

    // Report failures
    if (!final_result)
    {
        BERROR("Failed to setup %s frequency level state", frequency_text);
    }
    
    return final_result;
}

static b8 release_frequency_state(vulkan_context* context, bshader* s, shader_update_frequency frequency, u32 frequency_id)
{
    vulkan_shader* internal = s->internal_data;

    vulkan_shader_frequency_state* frequency_state = 0;
    const char* frequency_text = 0;
    b8 do_ubo_destroy = false;
    u8 uniform_sampler_count = 0;
    u8 uniform_texture_count = 0;
    u64 ubo_stride = 0;

    switch (frequency)
    {
    case SHADER_UPDATE_FREQUENCY_PER_FRAME:
        frequency_text = "per-frame";
        frequency_state = &internal->per_frame_state;
        do_ubo_destroy = true;
        uniform_sampler_count = s->per_frame.uniform_sampler_count;
        uniform_texture_count = s->per_frame.uniform_texture_count;
        ubo_stride = s->per_frame.ubo_stride;
        return false;
    case SHADER_UPDATE_FREQUENCY_PER_GROUP:
        frequency_text = "per-group";
        frequency_state = &internal->group_states[frequency_id];
        do_ubo_destroy = true;
        uniform_sampler_count = s->per_group.uniform_sampler_count;
        uniform_texture_count = s->per_group.uniform_texture_count;
        ubo_stride = s->per_group.ubo_stride;
        break;
    case SHADER_UPDATE_FREQUENCY_PER_DRAW:
        frequency_text = "per-draw";
        frequency_state = &internal->per_draw_states[frequency_id];
        do_ubo_destroy = false;
        uniform_sampler_count = s->per_draw.uniform_sampler_count;
        uniform_texture_count = s->per_draw.uniform_texture_count;
        ubo_stride = s->per_draw.ubo_stride;
        break;
    }

    // Wait for any pending operations using the descriptor set to finish
    vkDeviceWaitIdle(context->device.logical_device);

    u32 image_count = get_image_count(context);

    // Free descriptor sets (one per frame)
    VkResult result = vkFreeDescriptorSets(context->device.logical_device, internal->descriptor_pool, image_count, frequency_state->descriptor_sets);
    if (result != VK_SUCCESS)
        BERROR("Error freeing %s shader descriptor sets!", frequency_text);

    // Destroy bindings and their descriptor states/uniforms. UBO, if one exists
    if (do_ubo_destroy)
    {
        // Destroy UBO descriptor state
        BFREE_TYPE_CARRAY(frequency_state->ubo_descriptor_state.generations, u8, image_count);
        frequency_state->ubo_descriptor_state.generations = 0;
        BFREE_TYPE_CARRAY(frequency_state->ubo_descriptor_state.ids, u32, image_count);
        frequency_state->ubo_descriptor_state.ids = 0;
        BFREE_TYPE_CARRAY(frequency_state->ubo_descriptor_state.frame_numbers, u64, image_count);
        frequency_state->ubo_descriptor_state.frame_numbers = 0;

        // Release renderbuffer ranges
        if (ubo_stride != 0)
        {
            for (u32 i = 0; i < internal->uniform_buffer_count; ++i)
            {
                if (!renderer_renderbuffer_free(&internal->uniform_buffers[i], ubo_stride, frequency_state->offset))
                {
                    BERROR("release_frequency_state failed to free range from renderbuffer");
                }
            }
        }
    }

    // Samplers
    if (frequency_state->sampler_states)
    {
        for (u32 a = 0; a < uniform_sampler_count; ++a)
        {
            vulkan_uniform_sampler_state* sampler_state = &frequency_state->sampler_states[a];
            u32 array_length = BMAX(sampler_state->uniform.array_length, 1);
            BFREE_TYPE_CARRAY(sampler_state->descriptor_states, vulkan_descriptor_state, array_length);
            sampler_state->descriptor_states = 0;
            if (sampler_state->sampler_handles)
            {
                BFREE_TYPE_CARRAY(sampler_state->sampler_handles, bhandle, array_length);
                sampler_state->sampler_handles = 0;
            }
        }

        BFREE_TYPE_CARRAY(frequency_state->sampler_states, vulkan_uniform_texture_state, image_count);
        frequency_state->texture_states = 0;
    }

    // Textures
    if (frequency_state->texture_states)
    {
        for (u32 a = 0; a < uniform_texture_count; ++a)
        {
            vulkan_uniform_texture_state* texture_state = &frequency_state->texture_states[a];
            u32 array_length = BMAX(texture_state->uniform.array_length, 1);
            BFREE_TYPE_CARRAY(texture_state->descriptor_states, vulkan_descriptor_state, array_length);
            texture_state->descriptor_states = 0;
            if (texture_state->texture_handles)
            {
                BFREE_TYPE_CARRAY(texture_state->texture_handles, bhandle, array_length);
                texture_state->texture_handles = 0;
            }
        }
        
        BFREE_TYPE_CARRAY(frequency_state->texture_states, vulkan_uniform_texture_state, image_count);
        frequency_state->texture_states = 0;
    }

    frequency_state->offset = INVALID_ID;
    frequency_state->id = INVALID_ID;

    return true;
}

/**
 * =================== VULKAN ALLOCATOR ===================
 */

#if BVULKAN_USE_CUSTOM_ALLOCATOR == 1
/**
 * @brief Implementation of PFN_vkAllocationFunction
 * @link
 * https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/PFN_vkAllocationFunction.html
 *
 * @param user_data User data specified in the allocator by the application.
 * @param size The size in bytes of the requested allocation.
 * @param alignment The requested alignment of the allocation in bytes. Must be a power of two.
 * @param allocationScope The allocation scope and lifetime.
 * @return A memory block if successful; otherwise 0.
 */
static void* vulkan_alloc_allocation(void* user_data, size_t size, size_t alignment, VkSystemAllocationScope allocation_scope)
{
    // Null MUST be returned if this fails
    if (size == 0)
        return 0;

    void* result = ballocate_aligned(size, (u16)alignment, MEMORY_TAG_VULKAN);
#ifdef BVULKAN_ALLOCATOR_TRACE
    BTRACE("Allocated block %p. Size=%llu, Alignment=%llu", result, size, alignment);
#endif
    return result;
}

/**
 * @brief Implementation of PFN_vkFreeFunction
 * @link
 * https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/PFN_vkFreeFunction.html
 *
 * @param user_data User data specified in the allocator by the application.
 * @param memory The allocation to be freed.
 */
static void vulkan_alloc_free(void* user_data, void* memory)
{
    if (!memory)
    {
#ifdef BVULKAN_ALLOCATOR_TRACE
        BTRACE("Block is null, nothing to free: %p", memory);
#endif
        return;
    }

#ifdef BVULKAN_ALLOCATOR_TRACE
    BTRACE("Attempting to free block %p...", memory);
#endif
    u64 size;
    u16 alignment;
    b8 result = bmemory_get_size_alignment(memory, &size, &alignment);
    if (result)
    {
#ifdef BVULKAN_ALLOCATOR_TRACE
        BTRACE("Block %p found with size/alignment: %llu/%u. Freeing aligned block...", memory, size, alignment);
#endif
        bfree_aligned(memory, size, alignment, MEMORY_TAG_VULKAN);
    }
    else
    {
        BERROR("vulkan_alloc_free failed to get alignment lookup for block %p.", memory);
    }
}

/**
 * @brief Implementation of PFN_vkReallocationFunction
 * @link
 * https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/PFN_vkReallocationFunction.html
 *
 * @param user_data User data specified in the allocator by the application.
 * @param original Either NULL or a pointer previously returned by vulkan_alloc_allocation.
 * @param size The size in bytes of the requested allocation.
 * @param alignment The requested alignment of the allocation in bytes. Must be a power of two.
 * @param allocation_scope The scope and lifetime of the allocation.
 * @return A memory block if successful; otherwise 0.
 */
static void* vulkan_alloc_reallocation(void* user_data, void* original, size_t size, size_t alignment, VkSystemAllocationScope allocation_scope)
{
    if (!original)
        return vulkan_alloc_allocation(user_data, size, alignment, allocation_scope);

    if (size == 0)
    {
        vulkan_alloc_free(user_data, original);
        return 0;
    }

    // NOTE: if pOriginal is not null, the same alignment must be used for the new allocation as original
    u64 alloc_size;
    u16 alloc_alignment;
    b8 is_aligned = bmemory_get_size_alignment(original, &alloc_size, &alloc_alignment);
    if (!is_aligned)
    {
        BERROR("vulkan_alloc_reallocation of unaligned block %p", original);
        return 0;
    }

    if (alloc_alignment != alignment)
    {
        BERROR("Attempted realloc using a different alignment of %llu than the original of %hu", alignment, alloc_alignment);
        return 0;
    }

#ifdef BVULKAN_ALLOCATOR_TRACE
    BTRACE("Attempting to realloc block %p...", original);
#endif

    void* result = vulkan_alloc_allocation(user_data, size, alloc_alignment, allocation_scope);
    if (result)
    {
#ifdef BVULKAN_ALLOCATOR_TRACE
        BTRACE("Block %p reallocated to %p, copying data...", original, result);
#endif

        // Copy over the original memory.
        bcopy_memory(result, original, alloc_size);
#ifdef BVULKAN_ALLOCATOR_TRACE
        BTRACE("Freeing original aligned block %p...", original);
#endif
        // Free the original memory only if the new allocation was successful
        bfree_aligned(original, alloc_size, alloc_alignment, MEMORY_TAG_VULKAN);
    }
    else
    {
#ifdef BVULKAN_ALLOCATOR_TRACE
        BERROR("Failed to realloc %p", original);
#endif
    }

    return result;
}

/**
 * @brief Implementation of PFN_vkInternalAllocationNotification. Purely informational, nothing can really be done with this except to track it
 * @link
 * https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/PFN_vkInternalAllocationNotification.html
 *
 * @param pUserData User data specified in the allocator by the application.
 * @param size The size of the allocation in bytes.
 * @param allocationType The type of internal allocation.
 * @param allocationScope The scope and lifetime of the allocation.
 */
static void vulkan_alloc_internal_alloc(void* pUserData, size_t size, VkInternalAllocationType allocationType, VkSystemAllocationScope allocationScope)
{
#ifdef BVULKAN_ALLOCATOR_TRACE
    BTRACE("External allocation of size: %llu", size);
#endif
    ballocate_report((u64)size, MEMORY_TAG_VULKAN_EXT);
}

/**
 * @brief Implementation of PFN_vkInternalFreeNotification. Purely informational, nothing can really be done with this except to track it
 * @link
 * https://www.khronos.org/registry/vulkan/specs/1.3-extensions/man/html/PFN_vkInternalFreeNotification.html
 *
 * @param pUserData User data specified in the allocator by the application.
 * @param size The size of the allocation to be freed in bytes.
 * @param allocationType The type of internal allocation.
 * @param allocationScope The scope and lifetime of the allocation.
 */
static void vulkan_alloc_internal_free(void* pUserData, size_t size, VkInternalAllocationType allocationType, VkSystemAllocationScope allocationScope)
{
#ifdef BVULKAN_ALLOCATOR_TRACE
    BTRACE("External free of size: %llu", size);
#endif
    bfree_report((u64)size, MEMORY_TAG_VULKAN_EXT);
}

/**
 * @brief Create a vulkan allocator object, filling out the function pointers in the provided struct
 *
 * @param callbacks A pointer to the allocation callbacks structure to be filled out.
 * @return b8 True on success; otherwise false.
 */
static b8 create_vulkan_allocator(vulkan_context* context, VkAllocationCallbacks* callbacks)
{
    if (callbacks)
    {
        callbacks->pfnAllocation = vulkan_alloc_allocation;
        callbacks->pfnReallocation = vulkan_alloc_reallocation;
        callbacks->pfnFree = vulkan_alloc_free;
        callbacks->pfnInternalAllocation = vulkan_alloc_internal_alloc;
        callbacks->pfnInternalFree = vulkan_alloc_internal_free;
        callbacks->pUserData = context;
        return true;
    }

    return false;
}

#endif // BVULKAN_USE_CUSTOM_ALLOCATOR == 1
