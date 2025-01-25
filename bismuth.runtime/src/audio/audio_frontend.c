#include "audio_frontend.h"

#include "assets/basset_types.h"
#include "audio/baudio_types.h"
#include "core/engine.h"
#include "defines.h"
#include "identifiers/bhandle.h"
#include "bresources/bresource_types.h"
#include "logger.h"
#include "math/bmath.h"
#include "memory/bmemory.h"
#include "parsers/bson_parser.h"
#include "plugins/plugin_types.h"
#include "strings/bname.h"
#include "systems/bresource_system.h"
#include "systems/plugin_system.h"

typedef struct baudio_system_config
{
    /** @brief The frequency to output audio at */
    u32 frequency;
    /** @brief The number of audio channels to support (i.e. 2 for stereo, 1 for mono) */
    u32 channel_count;

    /** @brief The size to chunk streamed audio data in */
    u32 chunk_size;

    /**
     * @brief The number of separately-controlled channels used for mixing purposes.
     * Each channel can have its volume independently controlled. Not to be confused with channel_count above
     */
    u32 audio_channel_count;

    /** @brief The maximum number of audio resources (sounds or music) that can be loaded at once */
    u32 max_resource_count;

    /** @brief The name of the plugin to be loaded for the audio backend */
    const char* backend_plugin_name;
} baudio_system_config;

typedef struct baudio_resource_handle_data
{
    // The unique id matching an associated handle. INVALID_ID_U64 means this slot is unused
    u64 uniqueid;

    // A pointer to the underlying audio resource
    bresource_audio* resource;

    // Indicates if the audio should be streamed in small bits (large files) or loaded all at once (small files)
    b8 is_streaming;

    // Range: [0.5f - 2.0f] Default: 1.0f
    f32 pitch;

    // Range: 0-1
    f32 volume;

    b8 looping;
} baudio_resource_handle_data;

typedef struct baudio_channel
{
    f32 volume;

    // Pitch, generally left at 1
    f32 pitch;
    // Position of the sound
    vec3 position;
    // Indicates if the source is looping
    b8 looping;
    // A pointer to the currently bound resource handle data, if in use; otherwise 0/null (i.e. not in use)
    baudio_resource_handle_data* bound_data;
} baudio_channel;

typedef struct baudio_system_state
{
    f32 master_volume;
    /** @brief The frequency to output audio at */
    u32 frequency;
    /** @brief The number of audio channels to support (i.e. 2 for stereo, 1 for mono) */
    u32 channel_count;

    /** @brief The size to chunk streamed audio data in */
    u32 chunk_size;

    /**
     * @brief The number of separately-controlled channels used for mixing purposes. Each channel
     * can have its volume independently controlled. Not to be confused with channel_count above.
     */
    u32 audio_channel_count;

    // Channels which can play audio
    baudio_channel channels[AUDIO_CHANNEL_MAX_COUNT];

    // The max number of audio resources that can be loaded at any time
    u32 max_resource_count;

    // Array of internal resources for audio data in the system's frontend
    baudio_resource_handle_data* resources;

    // The backend plugin
    bruntime_plugin* plugin;

    // Pointer to the backend interface
    baudio_backend_interface* backend;
} baudio_system_state;

typedef struct audio_asset_request_listener
{
    baudio_system_state* state;
    bhandle audio;
} audio_asset_request_listener;

static b8 deserialize_config(const char* config_str, baudio_system_config* out_config);
static bhandle get_new_handle(baudio_system_state* state);
static void on_audio_asset_loaded(bresource* resource, void* listener);
static b8 handle_is_valid_and_pristine(baudio_system_state* state, bhandle handle);

b8 baudio_system_initialize(u64* memory_requirement, void* memory, const char* config_str)
{
    *memory_requirement = sizeof(baudio_system_state);
    if (!memory)
        return true;

    baudio_system_state* state = (baudio_system_state*)memory;

    // Get config
    baudio_system_config config = {0};
    if (!deserialize_config(config_str, &config))
    {
        BWARN("Failed to parse audio system config. See logs for details. Using reasonable defaults instead");
        config.audio_channel_count = 8;
        config.backend_plugin_name = "bismuth.plugin.audio.openal";
        config.frequency = 44100;
        config.channel_count = 2;
        config.chunk_size = 4096 * 16;
        config.max_resource_count = 32;
    }

    state->chunk_size = config.chunk_size;
    state->channel_count = config.channel_count;
    state->audio_channel_count = config.audio_channel_count;
    state->frequency = config.frequency;
    state->max_resource_count = config.max_resource_count;
    state->resources = BALLOC_TYPE_CARRAY(baudio_resource_handle_data, state->max_resource_count);

    // Invalidate all entries
    for (u32 i = 0; i < state->max_resource_count; ++i)
    {
        baudio_resource_handle_data* data = &state->resources[i];
        data->resource = 0;
        // This marks it as unused
        data->uniqueid = INVALID_ID_U64;
    }

    // Default volumes for master and all channels to 1.0 (max)
    state->master_volume = 1.0f;
    for (u32 i = 0; i < state->audio_channel_count; ++i)
    {
        baudio_channel* channel = &state->channels[i];
        channel->volume = 1.0f;
        // Also set some other reasonable defaults
        channel->pitch = 1.0f;
        channel->position = vec3_zero();
        channel->looping = false;
        channel->bound_data = 0;
    }

    // Load the plugin
    state->plugin = plugin_system_get(engine_systems_get()->plugin_system, config.backend_plugin_name);
    if (!state->plugin)
    {
        BERROR("Failed to load required audio backend plugin '%s'. See logs for details. Audio system init failed", config.backend_plugin_name);
        return false;
    }

    state->backend = state->plugin->plugin_state;

    // TODO: setup console commands to load/play sounds/music, etc

    baudio_backend_config backend_config = {0};
    backend_config.frequency = config.frequency;
    backend_config.chunk_size = config.chunk_size;
    backend_config.channel_count = config.channel_count;
    backend_config.max_resource_count = config.max_resource_count;
    backend_config.audio_channel_count = config.audio_channel_count;
    return state->backend->initialize(state->backend, &backend_config);
}

void baudio_system_shutdown(struct baudio_system_state* state)
{
    if (state)
    {
        // TODO: release all sources, device, etc
        state->backend->shutdown(state->backend);
    }
}

b8 baudio_system_update(struct baudio_system_state* state, struct frame_data* p_frame_data)
{
    if (state)
    {
        // Adjust each channel's properties based on what is bound to them (if anything)
        for (u32 i = 0; i < state->audio_channel_count; ++i)
        {
            baudio_channel* channel = &state->channels[i];
            if (channel->bound_data)
            {
                // Volume
                f32 mixed_volume = channel->bound_data->volume * channel->volume * state->master_volume;
                state->backend->channel_gain_set(state->backend, i, mixed_volume);
                // Pitch
                f32 mixed_pitch = channel->pitch * channel->bound_data->pitch;
                state->backend->channel_pitch_set(state->backend, i, mixed_pitch);
                // Looping setting
                b8 looping = channel->bound_data->looping;
                if (channel->bound_data->is_streaming)
                {
                    // Audio channels for streams should never loop directly, but are checked internally instead.
                    // Always force these to be false for streams
                    looping = false;
                }
                state->backend->channel_looping_set(state->backend, i, looping);

                // Position is only applied for mono sounds, because only those can be spatial/use position
                if (channel->bound_data->resource->channels == 1)
                    state->backend->channel_position_set(state->backend, i, channel->position);
            }
        }

        return state->backend->update(state->backend, p_frame_data);
    }

    return false;
}

b8 baudio_system_listener_orientation_set(struct baudio_system_state* state, vec3 position, vec3 forward, vec3 up)
{
    if (!state)
        return false;

    state->backend->listener_position_set(state->backend, position);
    state->backend->listener_orientation_set(state->backend, forward, up);
    return true;
}

void baudio_system_master_volume_set(struct baudio_system_state* state, f32 volume)
{
    if (state)
        state->master_volume = BCLAMP(volume, 0.0f, 1.0f);
}

f32 baudio_system_master_volume_get(struct baudio_system_state* state)
{
    if (state)
        return state->master_volume;

    return 0.0f;
}

void baudio_system_channel_volume_set(struct baudio_system_state* state, u8 channel_index, f32 volume)
{
    if (state)
    {
        if (channel_index >= state->audio_channel_count)
        {
            BERROR("baudio_system_channel_volume_set - channel_index %u is out of range (0-%u). Nothing will be done", channel_index, state->audio_channel_count);
            return;
        }
        
        // Clamp volume to a sane range.
        state->channels[channel_index].volume = BCLAMP(volume, 0.0f, 1.0f);
    }
}

f32 baudio_system_channel_volume_get(struct baudio_system_state* state, u8 channel_index)
{
    if (state)
    {
        if (channel_index >= state->audio_channel_count)
        {
            BERROR("baudio_system_channel_volume_get - channel_index %u is out of range (0-%u). 0 will be returned", channel_index, state->audio_channel_count);
            return 0.0f;
        }

        return state->channels[channel_index].volume;
    }

    return 0.0f;
}

b8 baudio_acquire(struct baudio_system_state* state, bname resource_name, bname package_name, b8 is_streaming, bhandle* out_audio)
{
    if (!state)
        return false;

    // Get/create a new handle for the resource
    *out_audio = get_new_handle(state);

    // Mark the slot as "in-use" by syncing the uniqueid
    baudio_resource_handle_data* data = &state->resources[out_audio->handle_index];
    data->uniqueid = out_audio->unique_id.uniqueid;
    data->is_streaming = is_streaming;
    // Set reasonable defaults
    data->looping = is_streaming ? true : false;
    data->pitch = 1.0f;
    data->volume = 1.0f;

    // Listener for the request
    audio_asset_request_listener* listener = BALLOC_TYPE(audio_asset_request_listener, MEMORY_TAG_RESOURCE);
    listener->state = state;
    listener->audio = *out_audio;

    // Request the resource. If it already exists it will return immediately and be in a ready/loaded state.
    // If not, it will be handled asynchronously. Either way, it'll go through the same callback
    bresource_audio_request_info request = {0};
    request.base.type = BRESOURCE_TYPE_AUDIO;
    request.base.assets = array_bresource_asset_info_create(1);
    request.base.user_callback = on_audio_asset_loaded;
    request.base.listener_inst = listener;
    bresource_asset_info* asset = &request.base.assets.data[0];
    asset->type = BASSET_TYPE_AUDIO;
    asset->asset_name = resource_name;
    asset->package_name = package_name;
    asset->watch_for_hot_reload = false; // Hot-reloading not supported for audio

    return bresource_system_request(engine_systems_get()->bresource_state, resource_name, (bresource_request_info*)&request);
}

void baudio_release(struct baudio_system_state* state, bhandle* audio)
{
    if (state && audio)
    {
        if (!handle_is_valid_and_pristine(state, *audio))
        {
            BERROR("baudio_release was passed a handle that is either invalid or stale. Nothing to be done");
            return;
        }

        baudio_resource_handle_data* data = &state->resources[audio->handle_index];

        // Release from backend
        state->backend->resource_unload(state->backend, *audio);

        // Release the resource
        bresource_system_release(engine_systems_get()->bresource_state, data->resource->base.name);

        // Reset the handle data and make the slot available for use
        data->is_streaming = false;
        data->resource = 0;
        data->uniqueid = INVALID_ID_U64;
    }
}

b8 baudio_play(struct baudio_system_state* state, bhandle audio, u8 channel_index)
{
    if (!handle_is_valid_and_pristine(state, audio))
    {
        BERROR("%s was called with an invalid or stale handle", __FUNCTION__);
        return false;
    }
    if (channel_index >= state->audio_channel_count)
    {
        BERROR("%s was called with an out of bounds channel_index of %hhu (range = 0-%u)", __FUNCTION__, channel_index, state->audio_channel_count);
        return false;
    }

    // Bind the resource
    state->channels[channel_index].bound_data = &state->resources[audio.handle_index];

    return state->backend->channel_play_resource(state->backend, audio, channel_index);
}

b8 baudio_is_valid(struct baudio_system_state* state, bhandle audio)
{
    if (!handle_is_valid_and_pristine(state, audio))
    {
        BERROR("%s was called with an invalid or stale handle", __FUNCTION__);
        return false;
    }

    baudio_resource_handle_data* data = &state->resources[audio.handle_index];

    return data->uniqueid != INVALID_ID_U64 && data->resource && data->resource->base.state == BRESOURCE_STATE_LOADED;
}

f32 baudio_pitch_get(struct baudio_system_state* state, bhandle audio)
{
    if (!handle_is_valid_and_pristine(state, audio))
    {
        BERROR("%s was called with an invalid or stale handle", __FUNCTION__);
        return 0.0f;
    }

    baudio_resource_handle_data* data = &state->resources[audio.handle_index];

    return data->pitch;
}

b8 baudio_pitch_set(struct baudio_system_state* state, bhandle audio, f32 pitch)
{
    if (!handle_is_valid_and_pristine(state, audio))
    {
        BERROR("%s was called with an invalid or stale handle", __FUNCTION__);
        return false;
    }

    baudio_resource_handle_data* data = &state->resources[audio.handle_index];

    // Clamp to a valid range
    data->pitch = BCLAMP(pitch, 0.5f, 2.0f);

    return true;
}

f32 baudio_volume_get(struct baudio_system_state* state, bhandle audio)
{
    if (!handle_is_valid_and_pristine(state, audio))
    {
        BERROR("%s was called with an invalid or stale handle", __FUNCTION__);
        return 0.0f;
    }

    baudio_resource_handle_data* data = &state->resources[audio.handle_index];

    return data->volume;
}

b8 baudio_volume_set(struct baudio_system_state* state, bhandle audio, f32 volume)
{
    if (!handle_is_valid_and_pristine(state, audio))
    {
        BERROR("%s was called with an invalid or stale handle", __FUNCTION__);
        return false;
    }

    baudio_resource_handle_data* data = &state->resources[audio.handle_index];

    // Clamp to a valid range
    data->volume = BCLAMP(volume, 0.0f, 1.0f);

    return true;
}

b8 baudio_looping_get(struct baudio_system_state* state, bhandle audio)
{
    if (!handle_is_valid_and_pristine(state, audio))
    {
        BERROR("%s was called with an invalid or stale handle", __FUNCTION__);
        return false;
    }

    baudio_resource_handle_data* data = &state->resources[audio.handle_index];

    return data->looping;
}

b8 baudio_looping_set(struct baudio_system_state* state, bhandle audio, b8 looping)
{
    if (!handle_is_valid_and_pristine(state, audio))
    {
        BERROR("%s was called with an invalid or stale handle", __FUNCTION__);
        return false;
    }

    baudio_resource_handle_data* data = &state->resources[audio.handle_index];
    data->looping = looping;

    return true;
}

b8 baudio_channel_play(struct baudio_system_state* state, u8 channel_index)
{
    if (!state)
        return false;

    if (channel_index >= state->audio_channel_count)
    {
        BERROR("%s called with channel_index %hhu out of range (range = 0-%u)", __FUNCTION__, channel_index, state->audio_channel_count);
        return false;
    }

    // Attempt to play the already bound resource if one exists. Otherwise this fails
    if (state->channels[channel_index].bound_data)
    {
        state->backend->channel_play(state->backend, channel_index);
        return true;
    }

    return false;
}

b8 baudio_channel_pause(struct baudio_system_state* state, u8 channel_index)
{
    if (!state)
        return false;

    if (channel_index >= state->audio_channel_count)
    {
        BERROR("%s called with channel_index %hhu out of range (range = 0-%u)", __FUNCTION__, channel_index, state->audio_channel_count);
        return false;
    }

    return state->backend->channel_pause(state->backend, channel_index);
}

b8 baudio_channel_resume(struct baudio_system_state* state, u8 channel_index)
{
    if (!state)
        return false;

    if (channel_index >= state->audio_channel_count)
    {
        BERROR("%s called with channel_index %hhu out of range (range = 0-%u)", __FUNCTION__, channel_index, state->audio_channel_count);
        return false;
    }

    return state->backend->channel_resume(state->backend, channel_index);
}

b8 baudio_channel_stop(struct baudio_system_state* state, u8 channel_index)
{
    if (!state)
        return false;

    if (channel_index >= state->audio_channel_count)
    {
        BERROR("%s called with channel_index %hhu out of range (range = 0-%u)", __FUNCTION__, channel_index, state->audio_channel_count);
        return false;
    }

    // Unbind the source on stop
    state->channels[channel_index].bound_data = 0;

    return state->backend->channel_stop(state->backend, channel_index);
}

b8 baudio_channel_is_playing(struct baudio_system_state* state, u8 channel_index)
{
    if (!state)
        return false;

    if (channel_index >= state->audio_channel_count)
    {
        BERROR("%s called with channel_index %hhu out of range (range = 0-%u)", __FUNCTION__, channel_index, state->audio_channel_count);
        return false;
    }

    return state->backend->channel_is_playing(state->backend, channel_index);
}

b8 baudio_channel_is_paused(struct baudio_system_state* state, u8 channel_index)
{
    if (!state)
        return false;

    if (channel_index >= state->audio_channel_count)
    {
        BERROR("%s called with channel_index %hhu out of range (range = 0-%u)", __FUNCTION__, channel_index, state->audio_channel_count);
        return false;
    }

    return state->backend->channel_is_paused(state->backend, channel_index);
}

b8 baudio_channel_is_stopped(struct baudio_system_state* state, u8 channel_index)
{
    if (!state)
        return false;

    if (channel_index >= state->audio_channel_count)
    {
        BERROR("%s called with channel_index %hhu out of range (range = 0-%u)", __FUNCTION__, channel_index, state->audio_channel_count);
        return false;
    }

    return state->backend->channel_is_stopped(state->backend, channel_index);
}

b8 baudio_channel_looping_get(struct baudio_system_state* state, u8 channel_index)
{
    if (!state)
        return false;

    if (channel_index >= state->audio_channel_count)
    {
        BERROR("%s called with channel_index %hhu out of range (range = 0-%u)", __FUNCTION__, channel_index, state->audio_channel_count);
        return false;
    }

    return state->channels[channel_index].looping;
}

b8 baudio_channel_looping_set(struct baudio_system_state* state, u8 channel_index, b8 looping)
{
    if (!state)
        return false;

    if (channel_index >= state->audio_channel_count)
    {
        BERROR("%s called with channel_index %hhu out of range (range = 0-%u)", __FUNCTION__, channel_index, state->audio_channel_count);
        return false;
    }

    state->channels[channel_index].looping = looping;
    return true;
}

f32 baudio_channel_volume_get(struct baudio_system_state* state, u8 channel_index)
{
    if (!state)
        return false;

    if (channel_index >= state->audio_channel_count)
    {
        BERROR("%s called with channel_index %hhu out of range (range = 0-%u)", __FUNCTION__, channel_index, state->audio_channel_count);
        return false;
    }

    return state->channels[channel_index].volume;
}

b8 baudio_channel_volume_set(struct baudio_system_state* state, u8 channel_index, f32 volume)
{
    if (!state)
        return false;

    if (channel_index >= state->audio_channel_count)
    {
        BERROR("%s called with channel_index %hhu out of range (range = 0-%u)", __FUNCTION__, channel_index, state->audio_channel_count);
        return false;
    }

    state->channels[channel_index].volume = volume;
    return true;
}

static b8 deserialize_config(const char* config_str, baudio_system_config* out_config)
{
    if (!config_str || !out_config)
    {
        BERROR("audio_system_deserialize_config requires a valid pointer to out_config and config_str");
        return false;
    }

    bson_tree tree = {0};
    if (!bson_tree_from_string(config_str, &tree))
    {
        BERROR("Failed to parse audio system config");
        return false;
    }

    // backend_plugin_name is required
    if (!bson_object_property_value_get_string(&tree.root, "backend_plugin_name", &out_config->backend_plugin_name))
    {
        BERROR("audio_system_deserialize_config: config does not contain backend_plugin_name, which is required");
        return false;
    }

    i64 audio_channel_count = 0;
    if (!bson_object_property_value_get_int(&tree.root, "audio_channel_count", &audio_channel_count))
    {
        audio_channel_count = 8;
    }
    if (audio_channel_count < 4)
    {
        BWARN("Invalid audio system config - audio_channel_count must be at least 4. Defaulting to 4");
        audio_channel_count = 4;
    }
    out_config->audio_channel_count = audio_channel_count;

    i64 max_resource_count = 0;
    if (!bson_object_property_value_get_int(&tree.root, "max_resource_count", &max_resource_count))
    {
        max_resource_count = 32;
    }
    if (max_resource_count < 32)
    {
        BWARN("Invalid audio system config - max_resource_count must be at least 32. Defaulting to 32");
        max_resource_count = 32;
    }
    out_config->max_resource_count = max_resource_count;

    // FIXME: This is currently unused
    i64 frequency;
    if (!bson_object_property_value_get_int(&tree.root, "frequency", &frequency))
    {
        frequency = 44100;
    }
    out_config->frequency = frequency;

    i64 channel_count;
    if (!bson_object_property_value_get_int(&tree.root, "channel_count", &channel_count))
    {
        channel_count = 2;
    }
    if (channel_count < 1)
    {
        channel_count = 1;
    }
    else if (channel_count > 2)
    {
        channel_count = 2;
    }
    out_config->channel_count = channel_count;

    i64 chunk_size;
    if (!bson_object_property_value_get_int(&tree.root, "chunk_size", &chunk_size))
    {
        chunk_size = 4096 * 16;
    }
    if (chunk_size == 0) {
        chunk_size = 4096 * 16;
    }
    out_config->chunk_size = chunk_size;

    bson_tree_cleanup(&tree);
    
    return true;
}

static bhandle get_new_handle(baudio_system_state* state)
{
    for (u32 i = 0; i < state->max_resource_count; ++i)
    {
        baudio_resource_handle_data* data = &state->resources[i];
        if (data->uniqueid == INVALID_ID_U64)
        {
            // Found one
            bhandle h = bhandle_create(i);
            data->uniqueid = h.unique_id.uniqueid;
            return h;
        }
    }

    BFATAL("No more room to allocate a new handle for a sound. Expand the max_resource_count in configuration to load more at once");
    return bhandle_invalid();
}

static void on_audio_asset_loaded(bresource* resource, void* listener)
{
    audio_asset_request_listener* listener_inst = listener;
    BTRACE("Audio resource loaded: '%s'", bname_string_get(resource->name));

    baudio_resource_handle_data* data = &listener_inst->state->resources[listener_inst->audio.handle_index];
    data->resource = (bresource_audio*)resource;

    // Send over to the backend to be loaded
    if (!listener_inst->state->backend->resource_load(listener_inst->state->backend, data->resource, data->is_streaming, listener_inst->audio))
    {
        BERROR("Failed to load audio resource into audio system backend. Resource will be released and handle unusable");

        bresource_system_release(engine_systems_get()->bresource_state, resource->name);
        data->is_streaming = false;
        data->resource = 0;
        data->uniqueid = INVALID_ID_U64;
    }
    
    // Cleanup the listener
    BFREE_TYPE(listener, audio_asset_request_listener, MEMORY_TAG_RESOURCE);
}

static b8 handle_is_valid_and_pristine(baudio_system_state* state, bhandle handle)
{
    return state && bhandle_is_valid(handle) && handle.handle_index < state->max_resource_count && bhandle_is_pristine(handle, state->resources[handle.handle_index].uniqueid);
}
