#include "audio_frontend.h"

#include "containers/darray.h"
#include "core/engine.h"
#include "defines.h"
#include "identifiers/bhandle.h"
#include "bresources/bresource_types.h"
#include "logger.h"
#include "math/bmath.h"
#include "memory/bmemory.h"
#include "parsers/bson_parser.h"
#include "plugins/plugin_types.h"
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

typedef struct baudio_channel
{
    f32 volume;

    // Pitch, generally left at 1
    f32 pitch;
    // Position of the sound
    vec3 position;
    // Indicates if the source is looping
    b8 looping;
    // Indicates if this souce is in use
    b8 in_use;

    // Handle into the sound backend/plugin resource array
    bhandle resource_handle;
} baudio_channel;

// The frontend-specific data for an instance of an audio resource
typedef struct baudio_resource_instance_handle_data
{
    u64 uniqueid;
} baudio_resource_instance_handle_data;

// The frontend-specific data for an audio resource
typedef struct baudio_resource_handle_data
{
    u64 uniqueid;

    // A pointer to the underlying audio resource
    bresource_audio* resource;

    // darray of instances of this resource
    baudio_resource_instance_handle_data* instances;
} baudio_resource_handle_data;

typedef struct baudio_system_state
{
    // TODO: backend interface
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
} baudio_system_state;

static b8 deserialize_config(const char* config_str, baudio_system_config* out_config);

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
        data->uniqueid = INVALID_ID_U64;
        data->instances = 0;
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
        channel->in_use = false;
        channel->looping = false;
        channel->resource_handle = bhandle_invalid();
    }

    // Load the plugin
    state->plugin = plugin_system_get(engine_systems_get()->plugin_system, config.backend_plugin_name);
    if (!state->plugin)
    {
        BERROR("Failed to load required audio backend plugin '%s'. See logs for details. Audio system init failed", config.backend_plugin_name);
        return false;
    }

    // TODO: Get the interface to the backend, then initialize()
    // TODO: setup console commands to load/play sounds/music, etc
}

void baudio_system_shutdown(struct baudio_system_state* state)
{
    // TODO: release all sources, device, etc.
}

b8 baudio_system_update(void* state, struct frame_data* p_frame_data)
{
}

b8 baudio_system_listener_orientation_set(struct baudio_system_state* state, vec3 position, vec3 forward, vec3 up)
{
}

void baudio_system_master_volume_set(struct baudio_system_state* state, f32 volume)
{
}

f32 baudio_system_master_volume_get(struct baudio_system_state* state)
{
}

void baudio_system_channel_volume_set(struct baudio_system_state* state, u8 channel_index, f32 volume)
{
}

f32 baudio_system_channel_volume_get(struct baudio_system_state* state, u8 channel_index)
{
}

b8 baudio_sound_effect_acquire(struct baudio_system_state* state, bname resource_name, bname package_name, bsound_effect_instance* out_instance) {}

void baudio_sound_effect_release(struct baudio_system_state* state, bsound_effect_instance* instance) {}

b8 baudio_sound_play(struct baudio_system_state* state, bsound_effect_instance instance) {}

b8 baudio_sound_pause(struct baudio_system_state* state, bsound_effect_instance instance) {}

b8 baudio_sound_resume(struct baudio_system_state* state, bsound_effect_instance instance) {}

b8 baudio_sound_stop(struct baudio_system_state* state, bsound_effect_instance instance) {}

b8 baudio_sound_is_playing(struct baudio_system_state* state, bsound_effect_instance instance) {}

b8 baudio_sound_is_valid(struct baudio_system_state* state, bsound_effect_instance instance) {}

f32 baudio_sound_pan_get(struct baudio_system_state* state, bsound_effect_instance instance) {}

b8 baudio_sound_pan_set(struct baudio_system_state* state, bsound_effect_instance instance, f32 pan) {}

b8 baudio_sound_pitch_get(struct baudio_system_state* state, bsound_effect_instance instance) {}

b8 baudio_sound_pitch_set(struct baudio_system_state* state, bsound_effect_instance instance, f32 pitch) {}

b8 baudio_sound_volume_get(struct baudio_system_state* state, bsound_effect_instance instance) {}

b8 baudio_sound_volume_set(struct baudio_system_state* state, bsound_effect_instance instance, f32 volume) {}

b8 baudio_music_acquire(struct baudio_system_state* state, bname resource_name, bname package_name, bmusic_instance* out_instance) {}

void baudio_music_release(struct baudio_system_state* state, bmusic_instance* instance) {}

b8 baudio_music_play(struct baudio_system_state* state, bmusic_instance instance) {}

b8 baudio_music_pause(struct baudio_system_state* state, bmusic_instance instance) {}

b8 baudio_music_resume(struct baudio_system_state* state, bmusic_instance instance) {}

b8 baudio_music_stop(struct baudio_system_state* state, bmusic_instance instance) {}

b8 baudio_music_seek(struct baudio_system_state* state, bmusic_instance instance, f32 seconds) {}

f32 baudio_music_time_length_get(struct baudio_system_state* state, bmusic_instance instance) {}

f32 baudio_music_time_played_get(struct baudio_system_state* state, bmusic_instance instance) {}

b8 baudio_music_is_playing(struct baudio_system_state* state, bmusic_instance instance) {}

b8 baudio_music_is_valid(struct baudio_system_state* state, bmusic_instance instance) {}

b8 baudio_music_looping_get(struct baudio_system_state* state, bmusic_instance instance) {}

b8 baudio_music_looping_set(struct baudio_system_state* state, bmusic_instance instance, b8 looping) {}

// 0=left, 0.5=center, 1.0=right
f32 baudio_music_pan_get(struct baudio_system_state* state, bmusic_instance instance) {}

// 0=left, 0.5=center, 1.0=right
b8 baudio_music_pan_set(struct baudio_system_state* state, bmusic_instance instance, f32 pan) {}

b8 baudio_music_pitch_get(struct baudio_system_state* state, bmusic_instance instance) {}

b8 baudio_music_pitch_set(struct baudio_system_state* state, bmusic_instance instance, f32 pitch) {}

b8 baudio_music_volume_get(struct baudio_system_state* state, bmusic_instance instance)
{
}

b8 baudio_music_volume_set(struct baudio_system_state* state, bmusic_instance instance, f32 volume)
{
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
        chunk_size = 8;
    }
    if (chunk_size == 0) {
        chunk_size = 4096 * 16;
    }
    out_config->chunk_size = chunk_size;

    bson_tree_cleanup(&tree);
    
    return true;
}
