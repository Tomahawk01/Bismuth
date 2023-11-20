#pragma once

#include "vulkan_types.h"

void vulkan_swapchain_create(
    vulkan_context* context,
    u32 width,
    u32 height,
    renderer_config_flags flags,
    vulkan_swapchain* out_swapchain);

void vulkan_swapchain_recreate(
    vulkan_context* context,
    u32 width,
    u32 height,
    vulkan_swapchain* swapchain);

void vulkan_swapchain_destroy(
    vulkan_context* context,
    vulkan_swapchain* swapchain);
