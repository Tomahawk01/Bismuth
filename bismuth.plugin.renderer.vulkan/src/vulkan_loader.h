#pragma once

#include "platform/vulkan_platform.h"

b8 vulkan_loader_initialize(brhi_vulkan* rhi);
b8 vulkan_loader_load_core(brhi_vulkan* rhi);
b8 vulkan_loader_load_instance(brhi_vulkan* rhi, VkInstance instance);
b8 vulkan_loader_load_device(brhi_vulkan* rhi, VkDevice device);
