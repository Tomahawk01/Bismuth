#pragma once

#include <plugins/plugin_types.h>

BAPI b8 bplugin_create(bruntime_plugin* out_plugin);
BAPI void bplugin_destroy(bruntime_plugin* plugin);
