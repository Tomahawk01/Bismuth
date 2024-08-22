#ifndef _BISMUTH_PLUGIN_TYPES_H_
#define _BISMUTH_PLUGIN_TYPES_H_

#include "defines.h"
#include "platform/platform.h"

struct frame_data;
struct bwindow;
struct bruntime_plugin;

typedef b8 (*PFN_bruntime_plugin_create)(struct bruntime_plugin* out_plugin);
typedef b8 (*PFN_bruntime_plugin_boot)(struct bruntime_plugin* plugin);
typedef b8 (*PFN_bruntime_plugin_initialize)(struct bruntime_plugin* plugin);
typedef void (*PFN_bruntime_plugin_destroy)(struct bruntime_plugin* plugin);

typedef b8 (*PFN_bruntime_plugin_update)(struct bruntime_plugin* plugin, struct frame_data* p_frame_data);
typedef b8 (*PFN_bruntime_plugin_frame_prepare)(struct bruntime_plugin* plugin, struct frame_data* p_frame_data);
typedef b8 (*PFN_bruntime_plugin_render)(struct bruntime_plugin* plugin, struct frame_data* p_frame_data);

typedef void (*PFN_bruntime_plugin_on_window_resized)(void* plugin_state, struct bwindow* window, u16 width, u16 height);

/**
 * A generic structure to hold function pointers for a given plugin. These serve as
 * the plugin's hook into the system at various points of its lifecycle. Only the
 * 'create' and 'destroy' are required, all others are optional. Also note that the "create"
 * isn't saved because it is only called the first time the plugin is loaded
 */
typedef struct bruntime_plugin
{
    /** @brief The plugin's name. Just for display. Serves no purpose */
    const char* name;

    /** @brief The plugin's configuration in string format */
    const char* config_str;

    /** @brief The dynamically loaded library for the plugin */
    dynamic_library library;

    /**
     * @brief A pointer to the plugin's `bplugin_boot` function. Optional.
     * This function is for plugins which require boot-time setup (i.e the renderer).
     * If exists, this is invoked at boot time
     */
    PFN_bruntime_plugin_boot bplugin_boot;

    PFN_bruntime_plugin_initialize bplugin_initialize;
    PFN_bruntime_plugin_destroy bplugin_destroy;
    PFN_bruntime_plugin_update bplugin_update;
    PFN_bruntime_plugin_frame_prepare bplugin_frame_prepare;
    PFN_bruntime_plugin_render bplugin_render;
    PFN_bruntime_plugin_on_window_resized bplugin_on_window_resized;

    /** @brief The size of the plugin's internal state */
    u64 plugin_state_size;

    /** @brief The block of memory holding the plugin's internal state */
    void* plugin_state;
} bruntime_plugin;

#endif
