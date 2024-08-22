#include "plugin_system.h"

#include "containers/darray.h"
#include "logger.h"
#include "parsers/bson_parser.h"
#include "platform/platform.h"
#include "plugins/plugin_types.h"
#include "strings/bstring.h"

typedef struct plugin_system_state
{
    // darray
    bruntime_plugin* plugins;
} plugin_system_state;

b8 plugin_system_deserialize_config(const char* config_str, plugin_system_config* out_config)
{
    if (!config_str || !out_config)
    {
        BERROR("plugin_system_deserialize_config requires a valid string and a pointer to hold the config");
        return false;
    }

    bson_tree tree = {0};
    if (!bson_tree_from_string(config_str, &tree))
    {
        BERROR("Failed to parse plugin system configuration");
        return false;
    }

    out_config->plugins = darray_create(plugin_system_plugin_config);

    // Get plugin configs
    bson_array plugin_configs = {0};
    if (!bson_object_property_value_get_object(&tree.root, "plugins", &plugin_configs))
    {
        BERROR("No plugins are configured");
        return false;
    }

    u32 plugin_count = 0;
    if (!bson_array_element_count_get(&plugin_configs, &plugin_count))
    {
        BERROR("Failed to get plugin count");
        return false;
    }

    // Each plugin
    for (u32 i = 0; i < plugin_count; ++i)
    {
        bson_object plugin_config_obj = {0};
        if (!bson_array_element_value_get_object(&plugin_configs, i, &plugin_config_obj))
        {
            BERROR("Failed to get plugin config at index %u", i);
            continue;
        }

        // Name is required
        plugin_system_plugin_config plugin = {0};
        if (!bson_object_property_value_get_string(&plugin_config_obj, "name", &plugin.name))
        {
            BERROR("Unable to get name for plugin at index %u", i);
            continue;
        }

        // Config is optional at this level. Attempt to extract the object first
        bson_object plugin_config = {0};
        if (!bson_object_property_value_get_object(&plugin_config_obj, "config", &plugin_config))
        {
            // If one doesn't exist, zero it out and move on
            plugin.config_str = 0;
        }
        else
        {
            // If it does exist, convert it back to a string and store it
            bson_tree config_tree = {0};
            config_tree.root = plugin_config;
            plugin.config_str = bson_tree_to_string(&config_tree);
        }

        // Push into the array
        darray_push(out_config->plugins, plugin);
    }

    return true;
}

b8 plugin_system_intialize(u64* memory_requirement, struct plugin_system_state* state, struct plugin_system_config* config)
{
    if (!memory_requirement)
        return false;

    *memory_requirement = sizeof(plugin_system_state);

    if (!state)
        return true;

    state->plugins = darray_create(bruntime_plugin);

    // Stand up all plugins in config. Don't initialize them yet, just create them
    u32 plugin_count = darray_length(config->plugins);
    for (u32 i = 0; i < plugin_count; ++i)
    {
        plugin_system_plugin_config* plugin = &config->plugins[i];

        if (!plugin_system_load_plugin(state, plugin->name, plugin->config_str))
        {
            // Warn about it, but move on
            BERROR("Plugin '%s' creation failed during plugin system boot", plugin->name);
        }
    }

    return true;
}

void plugin_system_shutdown(struct plugin_system_state* state)
{
    if (state)
    {
        if (state->plugins)
        {
            u32 plugin_count = darray_length(state->plugins);
            for (u32 i = 0; i < plugin_count; ++i)
            {
                bruntime_plugin* plugin = &state->plugins[i];
                if (plugin->bplugin_destroy) {
                    plugin->bplugin_destroy(plugin);
                }
            }
        }
        darray_destroy(state->plugins);
        state->plugins = 0;
    }
}

b8 plugin_system_initialize_plugins(struct plugin_system_state* state)
{
    if (state && state->plugins)
    {
        u32 plugin_count = darray_length(state->plugins);
        for (u32 i = 0; i < plugin_count; ++i)
        {
            bruntime_plugin* plugin = &state->plugins[i];
            // Invoke post-boot-time initialization of the plugin
            if (plugin->bplugin_initialize)
            {
                if (!plugin->bplugin_initialize(plugin))
                {
                    BERROR("Failed to initialize new plugin");
                    return false;
                }
            }
        }
    }
    return true;
}

b8 plugin_system_update_plugins(struct plugin_system_state* state, struct frame_data* p_frame_data)
{
    if (state && state->plugins)
    {
        u32 plugin_count = darray_length(state->plugins);
        for (u32 i = 0; i < plugin_count; ++i)
        {
            bruntime_plugin* plugin = &state->plugins[i];
            if (plugin->bplugin_update)
            {
                if (!plugin->bplugin_update(plugin, p_frame_data))
                    BERROR("Plugin '%s' failed update. See logs for details", plugin->name);
            }
        }
    }
    return true;
}

b8 plugin_system_frame_prepare_plugins(struct plugin_system_state* state, struct frame_data* p_frame_data)
{
    if (state && state->plugins)
    {
        u32 plugin_count = darray_length(state->plugins);
        for (u32 i = 0; i < plugin_count; ++i)
        {
            bruntime_plugin* plugin = &state->plugins[i];
            if (plugin->bplugin_frame_prepare)
            {
                if (!plugin->bplugin_frame_prepare(plugin, p_frame_data))
                    BERROR("Plugin '%s' failed frame_prepare. See logs for details", plugin->name);
            }
        }
    }
    return true;
}

b8 plugin_system_render_plugins(struct plugin_system_state* state, struct frame_data* p_frame_data)
{
    if (state && state->plugins)
    {
        u32 plugin_count = darray_length(state->plugins);
        for (u32 i = 0; i < plugin_count; ++i)
        {
            bruntime_plugin* plugin = &state->plugins[i];
            if (plugin->bplugin_render)
            {
                if (!plugin->bplugin_render(plugin, p_frame_data))
                    BERROR("Plugin '%s' failed render. See logs for details", plugin->name);
            }
        }
    }
    return true;
}

b8 plugin_system_on_window_resize_plugins(struct plugin_system_state* state, struct bwindow* window, u16 width, u16 height)
{
    if (state && state->plugins)
    {
        u32 plugin_count = darray_length(state->plugins);
        for (u32 i = 0; i < plugin_count; ++i)
        {
            bruntime_plugin* plugin = &state->plugins[i];
            if (plugin->bplugin_render)
                plugin->bplugin_on_window_resized(plugin, window, width, height);
        }
    }
    return true;
}

b8 plugin_system_load_plugin(struct plugin_system_state* state, const char* name, const char* config_str)
{
    if (!state)
        return false;

    if (!name)
    {
        BERROR("plugin_system_load_plugin requires a name!");
        return false;
    }

    bruntime_plugin new_plugin = {0};
    new_plugin.name = string_duplicate(name);

    // Load the plugin library
    if (!platform_dynamic_library_load(name, &new_plugin.library))
    {
        BERROR("Failed to load library for plugin '%s'. See logs for details", name);
        return false;
    }

    // bplugin_create is required. This should fail if it does not exist
    PFN_bruntime_plugin_create plugin_create = platform_dynamic_library_load_function("bplugin_create", &new_plugin.library);
    if (!plugin_create)
    {
        BERROR("Required function bplugin_create does not exist in library '%s'. Plugin load failed", name);
        return false;
    }

    // bplugin_destroy is required. This should fail if it does not exist
    new_plugin.bplugin_destroy = platform_dynamic_library_load_function("bplugin_destroy", &new_plugin.library);
    if (!new_plugin.bplugin_destroy)
    {
        BERROR("Required function bplugin_destroy does not exist in library '%s'. Plugin load failed", name);
        return false;
    }

    // Load optional hook functions
    new_plugin.bplugin_boot = platform_dynamic_library_load_function("bplugin_boot", &new_plugin.library);
    new_plugin.bplugin_initialize = platform_dynamic_library_load_function("bplugin_initialize", &new_plugin.library);
    new_plugin.bplugin_update = platform_dynamic_library_load_function("bplugin_update", &new_plugin.library);
    new_plugin.bplugin_frame_prepare = platform_dynamic_library_load_function("bplugin_frame_prepare", &new_plugin.library);
    new_plugin.bplugin_render = platform_dynamic_library_load_function("bplugin_render", &new_plugin.library);
    new_plugin.bplugin_on_window_resized = platform_dynamic_library_load_function("bplugin_on_window_resized", &new_plugin.library);

    // Invoke plugin creation
    if (!plugin_create(&new_plugin))
    {
        BERROR("plugin_create call failed for plugin '%s'. Plugin load failed", name);
        return false;
    }

    // Invoke boot-time initialization of the plugin
    if (new_plugin.bplugin_boot)
    {
        if (!new_plugin.bplugin_boot(&new_plugin))
        {
            BERROR("Failed to boot new plugin during creation");
            return false;
        }
    }

    // Take a copy of the config string if it exists
    if (config_str)
        new_plugin.config_str = string_duplicate(config_str);

    // Register the plugin
    darray_push(state->plugins, new_plugin);

    BINFO("Plugin '%s' successfully loaded", name);
    return true;
}

bruntime_plugin* plugin_system_get(struct plugin_system_state* state, const char* name)
{
    if (!state || !name)
        return 0;

    u32 plugin_count = darray_length(state->plugins);
    for (u32 i = 0; i < plugin_count; ++i)
    {
        bruntime_plugin* plugin = &state->plugins[i];
        if (strings_equali(name, plugin->name))
            return plugin;
    }

    BERROR("No plugin named '%s' found. 0/null is returned", name);
    return 0;
}
