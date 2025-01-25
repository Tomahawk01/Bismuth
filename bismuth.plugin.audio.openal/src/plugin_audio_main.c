#include "plugin_audio_main.h"

#include <audio/audio_types.h>
#include <logger.h>
#include <memory/bmemory.h>
#include <plugins/plugin_types.h>
#include <defines.h>

#include "version.h"
#include "openal_backend.h"

// Plugin entry point
b8 bplugin_create(bruntime_plugin* out_plugin)
{
    out_plugin->plugin_state_size = sizeof(audio_backend_interface);
    out_plugin->plugin_state = ballocate(out_plugin->plugin_state_size, MEMORY_TAG_AUDIO);

    baudio_backend_interface* backend = out_plugin->plugin_state;

    // Assign function pointers
    backend->initialize = openal_backend_initialize;
    backend->shutdown = openal_backend_shutdown;
    backend->update = openal_backend_update;

     backend->listener_position_set = openal_backend_listener_position_set;
    backend->listener_orientation_set = openal_backend_listener_orientation_set;
    backend->channel_gain_set = openal_backend_channel_gain_set;
    backend->channel_pitch_set = openal_backend_channel_pitch_set;
    backend->channel_position_set = openal_backend_channel_position_set;
    backend->channel_looping_set = openal_backend_channel_looping_set;

    backend->resource_load = openal_backend_resource_load;
    backend->resource_unload = openal_backend_resource_unload;

    backend->channel_play = openal_backend_channel_play;
    backend->channel_play_resource = openal_backend_channel_play_resource;

    backend->channel_stop = openal_backend_channel_stop;
    backend->channel_pause = openal_backend_channel_pause;
    backend->channel_resume = openal_backend_channel_resume;

    BINFO("OpenAL Plugin Creation successful (%s)", BVERSION);
    return true;
}

void bplugin_destroy(bruntime_plugin* plugin)
{
    if (plugin && plugin->plugin_state)
        bfree(plugin->plugin_state, plugin->plugin_state_size, MEMORY_TAG_AUDIO);

    bzero_memory(plugin, sizeof(bruntime_plugin));
}
