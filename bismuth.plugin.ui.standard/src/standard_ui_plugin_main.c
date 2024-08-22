#include "standard_ui_plugin_main.h"

#include <core/frame_data.h>
#include <logger.h>
#include <memory/bmemory.h>
#include <plugins/plugin_types.h>

#include "containers/darray.h"
#include "renderer/rendergraph.h"
#include "rendergraph_nodes/ui_rendergraph_node.h"
#include "standard_ui_system.h"

b8 bplugin_create(struct bruntime_plugin* out_plugin)
{
    if (!out_plugin) {
        BERROR("Cannot create a plugin without a pointer to hold it!");
        return false;
    }

    out_plugin->plugin_state_size = sizeof(standard_ui_plugin_state);
    out_plugin->plugin_state = ballocate(out_plugin->plugin_state_size, MEMORY_TAG_PLUGIN);

    return true;
}

b8 bplugin_initialize(struct bruntime_plugin* plugin)
{
    if (!plugin)
    {
        BERROR("Cannot initialize a plugin without a pointer to it!");
        return false;
    }

    standard_ui_plugin_state* plugin_state = plugin->plugin_state;

    standard_ui_system_config standard_ui_cfg = {0};
    standard_ui_cfg.max_control_count = 1024;
    standard_ui_system_initialize(&plugin_state->sui_state_memory_requirement, 0, &standard_ui_cfg);
    plugin_state->state = ballocate(plugin_state->sui_state_memory_requirement, MEMORY_TAG_PLUGIN);
    if (!standard_ui_system_initialize(&plugin_state->sui_state_memory_requirement, plugin_state->state, &standard_ui_cfg))
    {
        BERROR("Failed to initialize standard ui system");
        return false;
    }

    // Also register the rendergraph node
    if (!ui_rendergraph_node_register_factory())
    {
        BERROR("Failed to register standard ui rendergraph node!");
        return false;
    }

    return true;
}

void bplugin_destroy(struct bruntime_plugin* plugin)
{
    if (plugin)
    {
        standard_ui_plugin_state* plugin_state = plugin->plugin_state;
        standard_ui_system_shutdown(plugin_state->state);
    }
}

b8 bplugin_update(struct bruntime_plugin* plugin, struct frame_data* p_frame_data)
{
    if (!plugin)
        return false;

    standard_ui_plugin_state* plugin_state = plugin->plugin_state;
    return standard_ui_system_update(plugin_state->state, p_frame_data);
}

b8 bplugin_frame_prepare(struct bruntime_plugin* plugin, struct frame_data* p_frame_data)
{
    if (!plugin)
        return false;

    standard_ui_plugin_state* plugin_state = plugin->plugin_state;
    standard_ui_system_render_prepare_frame(plugin_state->state, p_frame_data);

    plugin_state->render_data = p_frame_data->allocator.allocate(sizeof(standard_ui_render_data));
    plugin_state->render_data->renderables = darray_create_with_allocator(standard_ui_renderable, &p_frame_data->allocator);
    plugin_state->render_data->ui_atlas = &plugin_state->state->ui_atlas;

    // NOTE: The time at which this is called is actually imperative to proper operation.
    // This is because the UI typically should be drawn as the last thing in the frame.
    // Might not be able to use this entry point
    return standard_ui_system_render(plugin_state->state, 0, p_frame_data, plugin_state->render_data);
}

void bplugin_on_window_resized(void* plugin_state, struct bwindow* window, u16 width, u16 height)
{
    // TODO: resize logic
}
