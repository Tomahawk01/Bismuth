#pragma once

#include "vulkan_types.h"

struct bwindow;
struct renderer_backend_interface;

b8 vulkan_swapchain_create(
    struct renderer_backend_interface* backend,
    struct bwindow* window,
    renderer_config_flags flags,
    vulkan_swapchain* out_swapchain);

b8 vulkan_swapchain_recreate(
    struct renderer_backend_interface* backend,
    struct bwindow* window,
    vulkan_swapchain* swapchain);

void vulkan_swapchain_destroy(struct renderer_backend_interface* backend, vulkan_swapchain* swapchain);
