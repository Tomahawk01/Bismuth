#pragma once

#include <defines.h>

struct bwindow;
struct vulkan_context;
struct VkPhysicalDevice_T;

b8 vulkan_platform_create_vulkan_surface(struct vulkan_context* context, struct bwindow* window);

/**
 * Appends the names of required extensions for this platform to
 * the names_darray, which should be created and passed in.
 */
void vulkan_platform_get_required_extension_names(const char*** names_darray);

/**
 * Indicates if the given device/queue family index combo supports presentation
 */
b8 vulkan_platform_presentation_support(struct vulkan_context* context, struct VkPhysicalDevice_T* physical_device, u32 queue_family_index);
