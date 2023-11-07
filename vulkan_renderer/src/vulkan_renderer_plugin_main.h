#pragma once

#include <renderer/renderer_types.inl>

BAPI b8 vulkan_renderer_plugin_create(renderer_plugin* out_plugin);
BAPI void vulkan_renderer_plugin_destroy(renderer_plugin* plugin);
