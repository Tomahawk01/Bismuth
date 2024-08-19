#pragma once

#include <renderer/renderer_types.h>

BAPI b8 plugin_create(renderer_plugin* out_plugin);
BAPI void plugin_destroy(renderer_plugin* plugin);
