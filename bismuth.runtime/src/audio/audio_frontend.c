#include "audio_frontend.h"

#include <containers/darray.h>
#include <core_audio_types.h>
#include <defines.h>
#include <identifiers/bhandle.h>
#include <logger.h>
#include <math/bmath.h>
#include <memory/bmemory.h>
#include <parsers/bson_parser.h>
#include <strings/bname.h>
#include <utils/audio_utils.h>

#include "assets/basset_types.h"
#include "audio/baudio_types.h"
#include "core/engine.h"
#include "bresources/bresource_types.h"
#include "plugins/plugin_types.h"
#include "strings/bstring.h"
#include "systems/bresource_system.h"
#include "systems/plugin_system.h"

typedef struct baudio_category_config
{
    bname name;
    f32 volume;
    baudio_space audio_space;
    u32 channel_id_count;
    u32* channel_ids;
} baudio_category_config;

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

    u32 category_count;
    baudio_category_config* categories;

    /** @brief The name of the plugin to be loaded for the audio backend */
    const char* backend_plugin_name;
} baudio_system_config;

typedef struct baudio_resource_instance_data
{
    // The unique id matching an associated handle. INVALID_ID_U64 means this slot is unused
    u64 uniqueid;

    // Range: [0.5f - 2.0f] Default: 1.0f
    f32 pitch;
    // Range: 0-1
    f32 volume;

    // Position of the sound
    vec3 position;

    // Indicates if the sound loops
    b8 looping;

    // The radius around the position where the sound plays at full volume
    f32 inner_radius;
    // The max distance from the position where the sound is still audible
    f32 outer_radius;

    // The rate of falloff/how quickly the sound drops in volume as it is moved away from. Only used in exponential attenuation; otherwise ignored
    f32 falloff;

    // The model to use for falloff of sound as the listener moves away
    baudio_attenuation_model attenuation_model;

    // The space in which the sound exists
    baudio_space audio_space;

    // A flag set when a play is requested. Remains on until the asset is valid and a play kicks off or if stopped
    b8 trigger_play;
} baudio_resource_instance_data;

typedef struct baudio_resource_handle_data
{
    // The unique id matching an associated handle. INVALID_ID_U64 means this slot is unused
    u64 uniqueid;

    // A pointer to the underlying audio resource
    bresource_audio* resource;

    bname resource_name;
    bname package_name;

    // Indicates if the audio should be streamed in small bits (large files) or loaded all at once (small files)
    b8 is_streaming;

    // darray of instances of this resource
    baudio_resource_instance_data* instances;
} baudio_resource_handle_data;

typedef struct baudio_emitter_handle_data
{
    u64 uniqueid;

    // Handle to underlying resource instance
    audio_instance instance;
    // Emitter-specific volume
    f32 volume;

    /** @brief inner_radius The inner radius around the sound's center point. A listener inside this radius experiences the volume at 100% */
    f32 inner_radius;
    /** @brief outer_radius The outer radius around the sound's center point. A listener outside this radius experiences the volume at 0% */
    f32 outer_radius;
    /** @brief The falloff factor to use for distance-based sound falloff. Only used for exponential falloff */
    f32 falloff;
    /** @brief The attenuation model to use for distance-based sound falloff */
    baudio_attenuation_model attenuation_model;
    vec3 world_position;

    b8 is_looping;
    b8 is_streaming;

    // Only changed by audio system when within range
    b8 playing_in_range;

    bname resource_name;
    bname package_name;

    vec3 velocity;
} baudio_emitter_handle_data;

typedef struct baudio_channel
{
    // The channel index
    u8 index;
    // The channel volume
    f32 volume;

    // A pointer to the currently bound resource handle data, if in use; otherwise 0/null (i.e. not in use)
    baudio_resource_handle_data* bound_resource;
    // A pointer to the currently bound instance handle data, if in use; otherwise 0/null (i.e. not in use)
    baudio_resource_instance_data* bound_instance;
} baudio_channel;

typedef struct baudio_category
{
    bname name;
    f32 volume;
    baudio_space audio_space;
    u32 channel_id_count;
    u32* channel_ids;
} baudio_category;

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

    u32 category_count;
    baudio_category categories[AUDIO_CHANNEL_MAX_COUNT];

    // The max number of audio resources that can be loaded at any time
    u32 max_resource_count;

    // Array of internal resources for audio data in the system's frontend
    baudio_resource_handle_data* resources;

    // darray of audio emitters
    baudio_emitter_handle_data* emitters;

    vec3 listener_position;
    vec3 listener_up;
    vec3 listener_forward;

    // The backend plugin
    bruntime_plugin* plugin;

    // Pointer to the backend interface
    baudio_backend_interface* backend;
} baudio_system_state;

typedef struct audio_asset_request_listener
{
    baudio_system_state* state;
    audio_instance* instance;
} audio_asset_request_listener;

static b8 deserialize_config(const char* config_str, baudio_system_config* out_config);
static bhandle get_base_handle(baudio_system_state* state, bname resource_name, bname package_name);
static void on_audio_asset_loaded(bresource* resource, void* listener);
static b8 base_resource_handle_is_valid_and_pristine(baudio_system_state* state, bhandle handle);
static b8 instance_handle_is_valid_and_pristine(baudio_system_state* state, baudio_resource_handle_data* base, bhandle handle);
static baudio_resource_handle_data* get_base(baudio_system_state* state, bhandle base_resource);
static baudio_resource_instance_data* get_instance(baudio_system_state* state, baudio_resource_handle_data* base, bhandle instance);
static u32 get_active_instance_count(baudio_resource_handle_data* base);
static baudio_channel* get_channel(baudio_system_state* state, i8 channel_index);
static baudio_channel* get_available_channel_from_category(baudio_system_state* state, u8 category_index);
static void baudio_emitter_update(struct baudio_system_state* state, baudio_emitter_handle_data* emitter);

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
    for (i8 i = 0; i < (i8)state->audio_channel_count; ++i)
    {
        baudio_channel* channel = &state->channels[i];
        channel->index = i;
        channel->volume = 1.0f;
        // Also set some other reasonable defaults
        channel->bound_resource = 0;
    }

    // Categories.
    state->category_count = config.category_count;
    for (u32 i = 0; i < config.category_count; ++i)
    {
        state->categories[i].name = config.categories[i].name;
        state->categories[i].audio_space = config.categories[i].audio_space;
        state->categories[i].volume = config.categories[i].volume;
        state->categories[i].channel_id_count = config.categories[i].channel_id_count;
        state->categories[i].channel_ids = BALLOC_TYPE_CARRAY(u32, state->categories[i].channel_id_count);
        bcopy_memory(state->categories[i].channel_ids, config.categories[i].channel_ids, sizeof(u32) * state->categories[i].channel_id_count);
    }

    // Darray for audio emitters
    state->emitters = darray_create(baudio_emitter_handle_data);

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
        // Listener updates
        state->backend->listener_position_set(state->backend, state->listener_position);
        state->backend->listener_orientation_set(state->backend, state->listener_forward, state->listener_up);

        // Update the registered emitters
        u32 emitter_count = darray_length(state->emitters);
        for (u32 i = 0; i < emitter_count; ++i)
        {
            if (state->emitters[i].uniqueid != INVALID_ID_U64)
                baudio_emitter_update(state, &state->emitters[i]);
        }

        // Adjust each channel's properties based on what is bound to them (if anything)
        for (u32 i = 0; i < state->audio_channel_count; ++i)
        {
            baudio_channel* channel = &state->channels[i];
            if (channel->bound_resource && channel->bound_instance)
            {
                b8 is_valid = channel->bound_resource->resource && channel->bound_resource->uniqueid != INVALID_ID_U64 && channel->bound_resource->resource->base.state == BRESOURCE_STATE_LOADED;
                baudio_resource_instance_data* instance = channel->bound_instance;

                // If a play has been triggered and the resource is valid/ready for playing, do it
                if (channel->bound_instance->trigger_play && is_valid)
                {
                    b8 play_result = state->backend->channel_play_resource(
                        state->backend,
                        channel->bound_resource->resource->internal_resource,
                        channel->bound_instance->audio_space,
                        channel->index);

                    if (!play_result)
                    {
                        BERROR("Failed to play resource on channel index %i", channel->index);
                    }
                    else
                    {
                        // Unset the flag on success
                        channel->bound_instance->trigger_play = false;
                    }
                }

                // Volume
                f32 gain = 1.0f;
                // Apply the volume at various levels by mixing them
                f32 mixed_volume = channel->bound_instance->volume * channel->volume * state->master_volume;
                if (channel->bound_instance->audio_space == BAUDIO_SPACE_3D)
                {
                    // Perform custom attenuation for sounds based on distance and falloff method. This is only done for mono sounds
                    f32 distance = vec3_distance(channel->bound_instance->position, state->listener_position);
                    gain = calculate_spatial_gain(distance, instance->inner_radius, instance->outer_radius, instance->falloff, instance->attenuation_model);
                    state->backend->channel_position_set(state->backend, i, channel->bound_instance->position);
                }
                else
                {
                    // Treat as 2D, even if mono, by syncing the position of the sound/channel with the listener.
                    state->backend->channel_position_set(state->backend, i, state->listener_position);
                    // NOTE: gain is left at 1.0 here, effectively meaning "zero-distance"
                }

                // Apply the mixed volume
                gain *= mixed_volume;

                state->backend->channel_gain_set(state->backend, i, gain);

                // Pitch
                state->backend->channel_pitch_set(state->backend, i, channel->bound_instance->pitch);
                // Looping setting
                b8 looping = channel->bound_instance->looping;
                if (channel->bound_resource->is_streaming)
                {
                    // Audio channels for streams should never loop directly, but are checked internally instead.
                    // Always force these to be false for streams
                    looping = false;
                }
                state->backend->channel_looping_set(state->backend, i, looping);

                // Position is only applied for mono sounds, because only those can be spatial/use position
                if (channel->bound_resource->resource->channels == 1)
                    state->backend->channel_position_set(state->backend, i, channel->bound_instance->position);
            }
        }

        return state->backend->update(state->backend, p_frame_data);
    }

    return false;
}

void baudio_system_listener_orientation_set(struct baudio_system_state* state, vec3 position, vec3 forward, vec3 up)
{
    if (state)
    {
        state->listener_up = up;
        state->listener_forward = forward;
        state->listener_position = position;
    }
}

void baudio_master_volume_set(struct baudio_system_state* state, f32 volume)
{
    if (state)
        state->master_volume = BCLAMP(volume, 0.0f, 1.0f);
}

f32 baudio_master_volume_get(struct baudio_system_state* state)
{
    if (state)
        return state->master_volume;

    return 0.0f;
}

b8 baudio_acquire(struct baudio_system_state* state, bname resource_name, bname package_name, b8 is_streaming, baudio_space audio_space, audio_instance* out_audio_instance)
{
    if (!state)
        return false;

    // Get/create a new handle for the resource
    out_audio_instance->base_resource = get_base_handle(state, resource_name, package_name);
    baudio_resource_handle_data* data = &state->resources[out_audio_instance->base_resource.handle_index];
    if (!data->resource)
    {
        // New handle was created, need to request resource
        data->resource_name = resource_name;
        data->package_name = package_name;

        data->is_streaming = is_streaming;

        // Listener for the request
        audio_asset_request_listener* listener = BALLOC_TYPE(audio_asset_request_listener, MEMORY_TAG_RESOURCE);
        listener->state = state;
        listener->instance = out_audio_instance;

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
        if (!bresource_system_request(engine_systems_get()->bresource_state, resource_name, (bresource_request_info*)&request))
        {
            BERROR("Failed to request audio resource. See logs for details.");
            bfree(listener, sizeof(audio_asset_request_listener), MEMORY_TAG_RESOURCE);
            return false;
        }
        
        // Create the darray for the instances
        data->instances = darray_create(baudio_resource_instance_data);
    }

    // Setup instance
    // Check to see if there is a free instance slot first. Otherwise push a new one
    u32 resource_index = INVALID_ID;
    u32 instance_count = darray_length(data->instances);
    for (u32 i = 0; i < instance_count; ++i)
    {
        baudio_resource_instance_data* instance = &data->instances[i];
        if (instance->uniqueid == INVALID_ID_U64)
        {
            // available
            instance_count = i;
            break;
        }
    }
    // Push a new one one was not found
    if (resource_index == INVALID_ID)
    {
        darray_push(data->instances, (baudio_resource_instance_data){0});
        resource_index = instance_count;
    }

    baudio_resource_instance_data* instance = &data->instances[resource_index];
    out_audio_instance->instance = bhandle_create(resource_index);
    instance->uniqueid = out_audio_instance->instance.unique_id.uniqueid;

    // Set reasonable defaults for an instance
    instance->looping = is_streaming; // Streaming sounds automatically loop
    instance->pitch = AUDIO_PITCH_DEFAULT;
    instance->volume = AUDIO_VOLUME_DEFAULT;
    instance->position = vec3_zero();
    instance->inner_radius = AUDIO_INNER_RADIUS_DEFAULT;
    instance->outer_radius = AUDIO_OUTER_RADIUS_DEFAULT;
    instance->falloff = AUDIO_FALLOFF_DEFAULT;
    // Set the instance's audio space accordingly
    instance->audio_space = audio_space;

    return true;
}

void baudio_release(struct baudio_system_state* state, audio_instance* instance)
{
    if (state && instance)
    {
        // Check both instance and base handle
        baudio_resource_handle_data* base_resource = get_base(state, instance->base_resource);
        if (!base_resource)
        {
            BERROR("baudio_release was passed a base resource handle that is either invalid or stale. Nothing to be done");
            return;
        }
        baudio_resource_instance_data* instance_data = get_instance(state, base_resource, instance->instance);
        if (!instance_data)
        {
            BERROR("baudio_release was passed an instance resource handle that is either invalid or stale. Nothing to be done");
            return;
        }

        // Invalidate the instance data
        bzero_memory(instance_data, sizeof(baudio_resource_instance_data));
        instance_data->uniqueid = INVALID_ID_U64;

        // Invalidate the handles.
        bhandle_invalidate(&instance->base_resource);
        bhandle_invalidate(&instance->instance);

        // See how many active instances there are left. If none, release
        u32 active_instance_count = get_active_instance_count(base_resource);
        if (!active_instance_count)
        {
            BTRACE("Audio resource '%s' has no more instances and will be released", bname_string_get(base_resource->resource->base.name));

            // Release from backend
            state->backend->resource_unload(state->backend, instance->base_resource);

            // Release the resource
            bresource_system_release(engine_systems_get()->bresource_state, base_resource->resource->base.name);

            // Release instance array
            darray_destroy(base_resource->instances);

            // Reset the handle data and make the slot available for use
            bzero_memory(base_resource, sizeof(baudio_resource_handle_data));
            base_resource->uniqueid = INVALID_ID_U64;
        }
    }
}

i8 baudio_category_id_get(struct baudio_system_state* state, bname name)
{
    for (i8 i = 0; i < (i8)state->category_count; ++i)
    {
        if (state->categories[i].name == name)
            return i;
    }

    // Not found
    return -1;
}

b8 baudio_play_in_category_by_name(struct baudio_system_state* state, audio_instance instance, bname category_name)
{
    i8 category_index = baudio_category_id_get(state, category_name);
    if (category_index < 0)
        return false;

    return baudio_play_in_category(state, instance, (u8)category_index);
}

b8 baudio_play_in_category(struct baudio_system_state* state, audio_instance instance, u8 category_index)
{
    if (!state || category_index >= state->category_count)
        return false;

    // Get a channel belonging to the category
    baudio_channel* channel = get_available_channel_from_category(state, category_index);
    if (!channel)
    {
        BWARN("No channel available to auto-select - perhaps increase number of channels for category? index=%u", category_index);
        // Pick the first channel in the category and clobber it's sound
        channel = &state->channels[state->categories[category_index].channel_ids[0]];
        baudio_channel_stop(state, channel->index);
    }

    // Play it on that channel
    return baudio_play(state, instance, channel->index);
}

b8 baudio_play(struct baudio_system_state* state, audio_instance instance, i8 channel_index)
{
    baudio_resource_handle_data* base_resource = get_base(state, instance.base_resource);
    if (!base_resource)
    {
        BERROR("%s was called with an invalid or stale base_resource handle", __FUNCTION__);
        return false;
    }
    baudio_resource_instance_data* instance_data = get_instance(state, base_resource, instance.instance);
    if (!instance_data)
    {
        BERROR("%s was called with an invalid or stale instance handle", __FUNCTION__);
        return false;
    }
    baudio_channel* channel = get_channel(state, channel_index);
    if (!channel)
    {
        if (channel_index >= 0)
            BERROR("%s was called with an out of bounds channel_index of %hhu (range = 0-%u)", __FUNCTION__, channel_index, state->audio_channel_count);
        return false;
    }

    // Bind the base resource
    channel->bound_resource = base_resource;
    channel->bound_instance = instance_data;

    // Trigger a play on the next update if/when the bound resource is valid for playing
    instance_data->trigger_play = true;

    // NOTE: deliberately not playing here as it's possible the sound isn't ready yet
    return true;
}

b8 baudio_stop(struct baudio_system_state* state, audio_instance instance)
{
    baudio_resource_handle_data* base_resource = get_base(state, instance.base_resource);
    if (!base_resource)
    {
        BERROR("%s was called with an invalid or stale base_resource handle", __FUNCTION__);
        return false;
    }
    baudio_resource_instance_data* instance_data = get_instance(state, base_resource, instance.instance);
    if (!instance_data)
    {
        BERROR("%s was called with an invalid or stale instance handle", __FUNCTION__);
        return false;
    }

    for (u32 i = 0; i < state->audio_channel_count; ++i)
    {
        baudio_channel* channel = &state->channels[i];
        if (channel->bound_resource == base_resource && channel->bound_instance == instance_data)
        {
            // Found the channel it's bound to, stop
            return baudio_channel_stop(state, i);
        }
    }

    // Return false if the audio wasn't bound to any channel
    return false;
}

b8 baudio_pause(struct baudio_system_state* state, audio_instance instance)
{
    baudio_resource_handle_data* base_resource = get_base(state, instance.base_resource);
    if (!base_resource)
    {
        BERROR("%s was called with an invalid or stale base_resource handle", __FUNCTION__);
        return false;
    }
    baudio_resource_instance_data* instance_data = get_instance(state, base_resource, instance.instance);
    if (!instance_data)
    {
        BERROR("%s was called with an invalid or stale instance handle", __FUNCTION__);
        return false;
    }

    for (u32 i = 0; i < state->audio_channel_count; ++i)
    {
        baudio_channel* channel = &state->channels[i];
        if (channel->bound_resource == base_resource && channel->bound_instance == instance_data)
        {
            // Found the channel it's bound to, stop
            return baudio_channel_pause(state, i);
        }
    }

    // Return false if the audio wasn't bound to any channel
    return false;
}

b8 baudio_resume(struct baudio_system_state* state, audio_instance instance)
{
    baudio_resource_handle_data* base_resource = get_base(state, instance.base_resource);
    if (!base_resource)
    {
        BERROR("%s was called with an invalid or stale base_resource handle", __FUNCTION__);
        return false;
    }
    baudio_resource_instance_data* instance_data = get_instance(state, base_resource, instance.instance);
    if (!instance_data)
    {
        BERROR("%s was called with an invalid or stale instance handle", __FUNCTION__);
        return false;
    }

    for (u32 i = 0; i < state->audio_channel_count; ++i)
    {
        baudio_channel* channel = &state->channels[i];
        if (channel->bound_resource == base_resource && channel->bound_instance == instance_data)
        {
            // Found the channel it's bound to, stop
            return baudio_channel_resume(state, i);
        }
    }

    // Return false if the audio wasn't bound to any channel
    return false;
}

b8 baudio_is_valid(struct baudio_system_state* state, audio_instance instance)
{
    baudio_resource_handle_data* base_resource = get_base(state, instance.base_resource);
    if (!base_resource)
    {
        BERROR("%s was called with an invalid or stale base_resource handle", __FUNCTION__);
        return false;
    }

    return base_resource->uniqueid != INVALID_ID_U64 && base_resource->resource && base_resource->resource->base.state == BRESOURCE_STATE_LOADED;
}

f32 baudio_pitch_get(struct baudio_system_state* state, audio_instance instance)
{
    baudio_resource_handle_data* base_resource = get_base(state, instance.base_resource);
    if (!base_resource)
    {
        BERROR("%s was called with an invalid or stale base_resource handle", __FUNCTION__);
        return false;
    }
    baudio_resource_instance_data* instance_data = get_instance(state, base_resource, instance.instance);
    if (!instance_data)
    {
        BERROR("%s was called with an invalid or stale instance handle", __FUNCTION__);
        return false;
    }
    return instance_data->pitch;
}

b8 baudio_pitch_set(struct baudio_system_state* state, audio_instance instance, f32 pitch)
{
    baudio_resource_handle_data* base_resource = get_base(state, instance.base_resource);
    if (!base_resource)
    {
        BERROR("%s was called with an invalid or stale base_resource handle", __FUNCTION__);
        return false;
    }
    baudio_resource_instance_data* instance_data = get_instance(state, base_resource, instance.instance);
    if (!instance_data)
    {
        BERROR("%s was called with an invalid or stale instance handle", __FUNCTION__);
        return false;
    }
    // Clamp to a valid range
    instance_data->pitch = BCLAMP(pitch, AUDIO_PITCH_MIN, AUDIO_PITCH_MAX);
    return true;
}

f32 baudio_volume_get(struct baudio_system_state* state, audio_instance instance)
{
    baudio_resource_handle_data* base_resource = get_base(state, instance.base_resource);
    if (!base_resource)
    {
        BERROR("%s was called with an invalid or stale base_resource handle", __FUNCTION__);
        return false;
    }
    baudio_resource_instance_data* instance_data = get_instance(state, base_resource, instance.instance);
    if (!instance_data)
    {
        BERROR("%s was called with an invalid or stale instance handle", __FUNCTION__);
        return false;
    }

    return instance_data->volume;
}

b8 baudio_volume_set(struct baudio_system_state* state, audio_instance instance, f32 volume)
{
    baudio_resource_handle_data* base_resource = get_base(state, instance.base_resource);
    if (!base_resource)
    {
        BERROR("%s was called with an invalid or stale base_resource handle", __FUNCTION__);
        return false;
    }
    baudio_resource_instance_data* instance_data = get_instance(state, base_resource, instance.instance);
    if (!instance_data)
    {
        BERROR("%s was called with an invalid or stale instance handle", __FUNCTION__);
        return false;
    }

    // Clamp to a valid range
    instance_data->volume = BCLAMP(volume, AUDIO_VOLUME_MIN, AUDIO_VOLUME_MAX);
    return true;
}

b8 baudio_looping_get(struct baudio_system_state* state, audio_instance instance)
{
    baudio_resource_handle_data* base_resource = get_base(state, instance.base_resource);
    if (!base_resource)
    {
        BERROR("%s was called with an invalid or stale base_resource handle", __FUNCTION__);
        return false;
    }
    baudio_resource_instance_data* instance_data = get_instance(state, base_resource, instance.instance);
    if (!instance_data)
    {
        BERROR("%s was called with an invalid or stale instance handle", __FUNCTION__);
        return false;
    }

    return instance_data->looping;
}

b8 baudio_looping_set(struct baudio_system_state* state, audio_instance instance, b8 looping)
{
    baudio_resource_handle_data* base_resource = get_base(state, instance.base_resource);
    if (!base_resource)
    {
        BERROR("%s was called with an invalid or stale base_resource handle", __FUNCTION__);
        return false;
    }
    baudio_resource_instance_data* instance_data = get_instance(state, base_resource, instance.instance);
    if (!instance_data)
    {
        BERROR("%s was called with an invalid or stale instance handle", __FUNCTION__);
        return false;
    }

    instance_data->looping = looping;
    return true;
}

vec3 baudio_position_get(struct baudio_system_state* state, audio_instance instance)
{
    baudio_resource_handle_data* base_resource = get_base(state, instance.base_resource);
    if (!base_resource)
    {
        BERROR("%s was called with an invalid or stale base_resource handle", __FUNCTION__);
        return vec3_zero();
    }
    baudio_resource_instance_data* instance_data = get_instance(state, base_resource, instance.instance);
    if (!instance_data)
    {
        BERROR("%s was called with an invalid or stale instance handle", __FUNCTION__);
        return vec3_zero();
    }

    return instance_data->position;
}

b8 baudio_position_set(struct baudio_system_state* state, audio_instance instance, vec3 position)
{
    baudio_resource_handle_data* base_resource = get_base(state, instance.base_resource);
    if (!base_resource)
    {
        BERROR("%s was called with an invalid or stale base_resource handle", __FUNCTION__);
        return false;
    }

    baudio_resource_instance_data* instance_data = get_instance(state, base_resource, instance.instance);
    if (!instance_data)
    {
        BERROR("%s was called with an invalid or stale instance handle", __FUNCTION__);
        return false;
    }

    instance_data->position = position;
    return true;
}

f32 baudio_inner_radius_get(struct baudio_system_state* state, audio_instance instance)
{
    baudio_resource_handle_data* base_resource = get_base(state, instance.base_resource);
    if (!base_resource)
    {
        BERROR("%s was called with an invalid or stale base_resource handle", __FUNCTION__);
        return 0;
    }
    baudio_resource_instance_data* instance_data = get_instance(state, base_resource, instance.instance);
    if (!instance_data)
    {
        BERROR("%s was called with an invalid or stale instance handle", __FUNCTION__);
        return 0;
    }

    return instance_data->inner_radius;
}

b8 baudio_inner_radius_set(struct baudio_system_state* state, audio_instance instance, f32 inner_radius)
{
    baudio_resource_handle_data* base_resource = get_base(state, instance.base_resource);
    if (!base_resource)
    {
        BERROR("%s was called with an invalid or stale base_resource handle", __FUNCTION__);
        return false;
    }

    baudio_resource_instance_data* instance_data = get_instance(state, base_resource, instance.instance);
    if (!instance_data)
    {
        BERROR("%s was called with an invalid or stale instance handle", __FUNCTION__);
        return false;
    }

    instance_data->inner_radius = BCLAMP(inner_radius, AUDIO_INNER_RADIUS_MIN, AUDIO_INNER_RADIUS_MAX);
    return true;
}

f32 baudio_outer_radius_get(struct baudio_system_state* state, audio_instance instance)
{
    baudio_resource_handle_data* base_resource = get_base(state, instance.base_resource);
    if (!base_resource)
    {
        BERROR("%s was called with an invalid or stale base_resource handle", __FUNCTION__);
        return 0;
    }

    baudio_resource_instance_data* instance_data = get_instance(state, base_resource, instance.instance);
    if (!instance_data)
    {
        BERROR("%s was called with an invalid or stale instance handle", __FUNCTION__);
        return 0;
    }

    return instance_data->outer_radius;
}

b8 baudio_outer_radius_set(struct baudio_system_state* state, audio_instance instance, f32 outer_radius)
{
    baudio_resource_handle_data* base_resource = get_base(state, instance.base_resource);
    if (!base_resource)
    {
        BERROR("%s was called with an invalid or stale base_resource handle", __FUNCTION__);
        return false;
    }

    baudio_resource_instance_data* instance_data = get_instance(state, base_resource, instance.instance);
    if (!instance_data)
    {
        BERROR("%s was called with an invalid or stale instance handle", __FUNCTION__);
        return false;
    }

    instance_data->outer_radius = BCLAMP(outer_radius, AUDIO_OUTER_RADIUS_MIN, AUDIO_OUTER_RADIUS_MAX);
    return true;
}

f32 baudio_falloff_get(struct baudio_system_state* state, audio_instance instance)
{
    baudio_resource_handle_data* base_resource = get_base(state, instance.base_resource);
    if (!base_resource)
    {
        BERROR("%s was called with an invalid or stale base_resource handle", __FUNCTION__);
        return 0;
    }
    baudio_resource_instance_data* instance_data = get_instance(state, base_resource, instance.instance);
    if (!instance_data)
    {
        BERROR("%s was called with an invalid or stale instance handle", __FUNCTION__);
        return 0;
    }

    return instance_data->falloff;
}

b8 baudio_falloff_set(struct baudio_system_state* state, audio_instance instance, f32 falloff)
{
    baudio_resource_handle_data* base_resource = get_base(state, instance.base_resource);
    if (!base_resource)
    {
        BERROR("%s was called with an invalid or stale base_resource handle", __FUNCTION__);
        return false;
    }

    baudio_resource_instance_data* instance_data = get_instance(state, base_resource, instance.instance);
    if (!instance_data)
    {
        BERROR("%s was called with an invalid or stale instance handle", __FUNCTION__);
        return false;
    }

    instance_data->falloff = BCLAMP(falloff, AUDIO_FALLOFF_MIN, AUDIO_FALLOFF_MAX);
    return true;
}

b8 baudio_channel_play(struct baudio_system_state* state, u8 channel_index)
{
    baudio_channel* channel = get_channel(state, channel_index);
    if (!channel)
    {
        BERROR("%s called with channel_index %hhu out of range (range = 0-%u)", __FUNCTION__, channel_index, state->audio_channel_count);
        return false;
    }

    // Attempt to play the already bound resource if one exists. Otherwise this fails
    if (channel->bound_resource)
        return state->backend->channel_play(state->backend, channel_index);

    return false;
}

b8 baudio_channel_pause(struct baudio_system_state* state, u8 channel_index)
{
    baudio_channel* channel = get_channel(state, channel_index);
    if (!channel)
    {
        BERROR("%s called with channel_index %hhu out of range (range = 0-%u)", __FUNCTION__, channel_index, state->audio_channel_count);
        return false;
    }

    return state->backend->channel_pause(state->backend, channel_index);
}

b8 baudio_channel_resume(struct baudio_system_state* state, u8 channel_index)
{
    baudio_channel* channel = get_channel(state, channel_index);
    if (!channel)
    {
        BERROR("%s called with channel_index %hhu out of range (range = 0-%u)", __FUNCTION__, channel_index, state->audio_channel_count);
        return false;
    }

    return state->backend->channel_resume(state->backend, channel_index);
}

b8 baudio_channel_stop(struct baudio_system_state* state, u8 channel_index)
{
    baudio_channel* channel = get_channel(state, channel_index);
    if (!channel)
    {
        BERROR("%s called with channel_index %hhu out of range (range = 0-%u)", __FUNCTION__, channel_index, state->audio_channel_count);
        return false;
    }

    // Unbind the resource and instance on stop
    channel->bound_resource = 0;
    channel->bound_instance = 0;
    return state->backend->channel_stop(state->backend, channel_index);
}

b8 baudio_channel_is_playing(struct baudio_system_state* state, u8 channel_index)
{
    baudio_channel* channel = get_channel(state, channel_index);
    if (!channel)
    {
        BERROR("%s called with channel_index %hhu out of range (range = 0-%u)", __FUNCTION__, channel_index, state->audio_channel_count);
        return false;
    }

    return state->backend->channel_is_playing(state->backend, channel_index);
}

b8 baudio_channel_is_paused(struct baudio_system_state* state, u8 channel_index)
{
    baudio_channel* channel = get_channel(state, channel_index);
    if (!channel)
    {
        BERROR("%s called with channel_index %hhu out of range (range = 0-%u)", __FUNCTION__, channel_index, state->audio_channel_count);
        return false;
    }

    return state->backend->channel_is_paused(state->backend, channel_index);
}

b8 baudio_channel_is_stopped(struct baudio_system_state* state, u8 channel_index)
{
    baudio_channel* channel = get_channel(state, channel_index);
    if (!channel)
    {
        BERROR("%s called with channel_index %hhu out of range (range = 0-%u)", __FUNCTION__, channel_index, state->audio_channel_count);
        return false;
    }

    return state->backend->channel_is_stopped(state->backend, channel_index);
}

f32 baudio_channel_volume_get(struct baudio_system_state* state, u8 channel_index)
{
    baudio_channel* channel = get_channel(state, channel_index);
    if (!channel)
    {
        BERROR("%s called with channel_index %hhu out of range (range = 0-%u)", __FUNCTION__, channel_index, state->audio_channel_count);
        return false;
    }

    return state->channels[channel_index].volume;
}

b8 baudio_channel_volume_set(struct baudio_system_state* state, u8 channel_index, f32 volume)
{
    baudio_channel* channel = get_channel(state, channel_index);
    if (!channel)
    {
        BERROR("%s called with channel_index %hhu out of range (range = 0-%u)", __FUNCTION__, channel_index, state->audio_channel_count);
        return false;
    }

    state->channels[channel_index].volume = volume;
    return true;
}

b8 baudio_emitter_create(struct baudio_system_state* state, f32 inner_radius, f32 outer_radius, f32 volume, f32 falloff, b8 is_looping, b8 is_streaming, bname audio_resource_name, bname package_name, bhandle* out_emitter)
{
    if (!state || !out_emitter)
        return false;

    *out_emitter = bhandle_invalid();

    // Look for a free slot, or push a new one if needed
    baudio_emitter_handle_data* emitter = 0;
    u32 length = darray_length(state->emitters);
    for (u32 i = 0; i < length; ++i)
    {
        if (state->emitters[i].uniqueid == INVALID_ID_U64)
        {
            emitter = &state->emitters[i];
            *out_emitter = bhandle_create(i);
            break;
        }
    }

    if (!emitter)
    {
        baudio_emitter_handle_data new_emitter = {0};
        new_emitter.uniqueid = INVALID_ID_U64;
        darray_push(state->emitters, new_emitter);
        emitter = &state->emitters[length];
        *out_emitter = bhandle_create(length);
    }

    emitter->uniqueid = out_emitter->unique_id.uniqueid;
    emitter->volume = volume;
    emitter->inner_radius = inner_radius;
    emitter->outer_radius = outer_radius;
    emitter->falloff = falloff;
    emitter->is_looping = is_looping;
    emitter->is_streaming = is_streaming;
    emitter->resource_name = audio_resource_name;
    emitter->package_name = package_name;

    return true;
}

b8 baudio_emitter_load(struct baudio_system_state* state, bhandle emitter_handle)
{
    if (!state)
        return false;

    if (bhandle_is_valid(emitter_handle) && bhandle_is_pristine(emitter_handle, state->emitters[emitter_handle.handle_index].uniqueid))
    {
        baudio_emitter_handle_data* emitter = &state->emitters[emitter_handle.handle_index];
        // NOTE: always use 3d space for emitters
        if (!baudio_acquire(state, emitter->resource_name, emitter->package_name, emitter->is_streaming, BAUDIO_SPACE_3D, &emitter->instance))
        {
            BWARN("Failed to acquire audio resource from audio system");
            return false;
        }

        // Apply properties to audio
        baudio_looping_set(state, emitter->instance, emitter->is_looping);
        baudio_outer_radius_set(state, emitter->instance, emitter->outer_radius);
        baudio_inner_radius_set(state, emitter->instance, emitter->inner_radius);
        baudio_falloff_set(state, emitter->instance, emitter->falloff);
        baudio_position_set(state, emitter->instance, emitter->world_position);
        baudio_volume_set(state, emitter->instance, emitter->volume);

        return true;
    }

    return false;
}

b8 baudio_emitter_unload(struct baudio_system_state* state, bhandle emitter_handle)
{
    if (!state)
        return false;

    if (bhandle_is_valid(emitter_handle) && bhandle_is_pristine(emitter_handle, state->emitters[emitter_handle.handle_index].uniqueid))
    {
        baudio_emitter_handle_data* emitter = &state->emitters[emitter_handle.handle_index];
        if (emitter->playing_in_range)
        {
            // Stop playing
            baudio_stop(state, emitter->instance);
            emitter->playing_in_range = false;
        }

        baudio_release(state, &emitter->instance);

        // Take a copy of the invalidated instance
        audio_instance invalid_inst = emitter->instance;

        bzero_memory(&emitter->instance, sizeof(baudio_emitter_handle_data));

        // Invalidate the handle data
        emitter->uniqueid = INVALID_ID_U64;
        emitter->instance = invalid_inst;

        return true;
    }

    return false;
}

b8 baudio_emitter_world_position_set(struct baudio_system_state* state, bhandle emitter_handle, vec3 world_position)
{
    if (!state)
        return false;

    if (bhandle_is_valid(emitter_handle) && bhandle_is_pristine(emitter_handle, state->emitters[emitter_handle.handle_index].uniqueid))
    {
        baudio_emitter_handle_data* emitter = &state->emitters[emitter_handle.handle_index];
        emitter->world_position = world_position;
        baudio_position_set(state, emitter->instance, emitter->world_position);
        return true;
    }

    return false;
}

static void baudio_emitter_update(struct baudio_system_state* state, baudio_emitter_handle_data* emitter)
{
    if (emitter->playing_in_range)
    {
        // Check if still in range. If not, need to stop
        if (vec3_distance(state->listener_position, emitter->world_position) > emitter->outer_radius)
        {
            BTRACE("Audio emitter no longer in listener range. Stopping...");
            // Stop playing
            baudio_stop(state, emitter->instance);
            emitter->playing_in_range = false;
        }
        else
        {
            // Continue
        }
    }
    else
    {
        // Check if in range. If so, need to start playing
        if (vec3_distance(state->listener_position, emitter->world_position) <= emitter->outer_radius)
        {
            BTRACE("Audio emitter came into listener range. Playing...");
            // HACK: Don't hardcode this. Config? Define family group, or index
            baudio_play(state, emitter->instance, -1);
            emitter->playing_in_range = true;
        }
    }
    
    // If still playing, apply audio properties
    if (emitter->playing_in_range)
    {
        baudio_looping_set(state, emitter->instance, emitter->is_looping);
        baudio_outer_radius_set(state, emitter->instance, emitter->outer_radius);
        baudio_inner_radius_set(state, emitter->instance, emitter->inner_radius);
        baudio_falloff_set(state, emitter->instance, emitter->falloff);
        baudio_position_set(state, emitter->instance, emitter->world_position);
        baudio_volume_set(state, emitter->instance, emitter->volume);
    }
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

    bson_array category_obj_array = {0};
    if (bson_object_property_value_get_array(&tree.root, "categories", &category_obj_array))
    {
        if (bson_array_element_count_get(&category_obj_array, &out_config->category_count))
        {
            out_config->categories = BALLOC_TYPE_CARRAY(baudio_category_config, out_config->category_count);

            // Each category
            for (u32 i = 0; i < out_config->category_count; ++i)
            {
                baudio_category_config* cat = &out_config->categories[i];
                bson_object cat_obj = {0};
                if (!bson_array_element_value_get_object(&category_obj_array, i, &cat_obj))
                {
                    BERROR("Possible format error reading object at index %u in 'categories' array. Skipping...", i);
                    continue;
                }

                // Name - required
                if (!bson_object_property_value_get_string_as_bname(&cat_obj, "name", &cat->name))
                {
                    BERROR("Unable to find required category property 'name' at index %u. Skipping...", i);
                    continue;
                }

                // Volume - optional
                if (!bson_object_property_value_get_float(&cat_obj, "volume", &cat->volume))
                {
                    // Default
                    cat->volume = 1.0f;
                }

                // Audio space - optional
                const char* audio_space_str = 0;
                if (!bson_object_property_value_get_string(&cat_obj, "audio_space", &audio_space_str))
                {
                    cat->audio_space = BAUDIO_SPACE_2D; // default to 2d if not provided
                }
                else
                {
                    cat->audio_space = string_to_audio_space(audio_space_str);
                    string_free(audio_space_str);
                }

                // Channel ids - required, must have at least one
                bson_array channel_ids_array = {0};
                if (!bson_object_property_value_get_array(&cat_obj, "channel_ids", &channel_ids_array))
                {
                    BERROR("'channel_ids', a required field for a cateregory, does not exist for cateregory index %u. Skipping...", i);
                    continue;
                }
                bson_array_element_count_get(&channel_ids_array, &cat->channel_id_count);
                if (!cat->channel_id_count)
                {
                    BERROR("Channel cateregory must have at least one channel id listed. Skipping index %u", i);
                    continue;
                }

                cat->channel_ids = BALLOC_TYPE_CARRAY(u32, cat->channel_id_count);

                for (u32 c = 0; c < cat->channel_id_count; c++)
                {
                    i64 val = 0;
                    bson_array_element_value_get_int(&channel_ids_array, c, &val);
                    cat->channel_ids[c] = (u32)val;
                }
            }
        }
    }

    bson_tree_cleanup(&tree);
    
    return true;
}

static bhandle get_base_handle(baudio_system_state* state, bname resource_name, bname package_name)
{
    // Search for name/package_name combo and return if found
    for (u32 i = 0; i < state->max_resource_count; ++i)
    {
        baudio_resource_handle_data* data = &state->resources[i];
        if (data->resource_name == resource_name && data->package_name == package_name)
        {
            // Found a match, return
            return bhandle_create_with_u64_identifier(i, data->uniqueid);
        }
    }

    // Resource with name/package_name combo not found, need to request new
    for (u32 i = 0; i < state->max_resource_count; ++i)
    {
        baudio_resource_handle_data* data = &state->resources[i];
        if (data->uniqueid == INVALID_ID_U64)
        {
            // Found one
            bhandle h = bhandle_create(i);
            // Mark as in use by syncing the uniqueid
            data->uniqueid = h.unique_id.uniqueid;
            data->resource = 0;
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

    baudio_resource_handle_data* data = get_base(listener_inst->state, listener_inst->instance->base_resource);
    if (!data)
    {
        BFATAL("Data handle is invalid during audio asset load completion. Check application logic");
    }
    else
    {
        data->resource = (bresource_audio*)resource;
        // Sync the resource's "internal" handle to the base resource handle we track in this system
        data->resource->internal_resource = listener_inst->instance->base_resource;

        // Send over to the backend to be loaded
        if (!listener_inst->state->backend->resource_load(listener_inst->state->backend, data->resource, data->is_streaming, listener_inst->instance->base_resource))
        {
            BERROR("Failed to load audio resource into audio system backend. Resource will be released and handle unusable");

            bresource_system_release(engine_systems_get()->bresource_state, resource->name);

            bzero_memory(data, sizeof(baudio_resource_handle_data));
            data->uniqueid = INVALID_ID_U64;
        }
    }
    
    // Cleanup the listener
    BFREE_TYPE(listener, audio_asset_request_listener, MEMORY_TAG_RESOURCE);
}

static b8 base_resource_handle_is_valid_and_pristine(baudio_system_state* state, bhandle handle)
{
    return state && bhandle_is_valid(handle) && handle.handle_index < state->max_resource_count && bhandle_is_pristine(handle, state->resources[handle.handle_index].uniqueid);
}

static b8 instance_handle_is_valid_and_pristine(baudio_system_state* state, baudio_resource_handle_data* base, bhandle handle)
{
    u32 instance_count = darray_length(base->instances);
    return state && base && base->instances && bhandle_is_valid(handle) && handle.handle_index < instance_count && bhandle_is_pristine(handle, base->instances[handle.handle_index].uniqueid);
}

static baudio_resource_handle_data* get_base(baudio_system_state* state, bhandle base_resource)
{
    if (!base_resource_handle_is_valid_and_pristine(state, base_resource))
        return 0;

    return &state->resources[base_resource.handle_index];
}

static baudio_resource_instance_data* get_instance(baudio_system_state* state, baudio_resource_handle_data* base, bhandle instance)
{
    if (!instance_handle_is_valid_and_pristine(state, base, instance))
        return 0;

    return &base->instances[instance.handle_index];
}

static u32 get_active_instance_count(baudio_resource_handle_data* base)
{
    u32 count = 0;
    u32 length = darray_length(base->instances);
    for (u32 i = 0; i < length; ++i)
        count += (base->instances[i].uniqueid != INVALID_ID_U64);

    return count;
}

static baudio_channel* get_channel(baudio_system_state* state, i8 channel_index)
{
    if (!state)
        return 0;

    if (channel_index < 0)
    {
        // First available
        for (u32 i = 0; i < state->audio_channel_count; ++i)
        {
            baudio_channel* channel = &state->channels[i];
            if (!channel->bound_instance && !channel->bound_resource)
            {
                // Available, use it
                return channel;
            }
        }

        BWARN("No channel is available for auto-selection");
        return 0;
    }
    else if (channel_index < (i8)state->audio_channel_count)
    {
        // Explicit channel id must be within range
        return &state->channels[channel_index];
    }

    return 0;
}

static baudio_channel* get_available_channel_from_category(baudio_system_state* state, u8 category_index)
{
    if (!state)
        return 0;
    if (category_index >= state->category_count)
        return 0;

    baudio_category* cat = &state->categories[category_index];
    
    // First available
    for (u32 i = 0; i < cat->channel_id_count; ++i)
    {
        u32 channel_id = cat->channel_ids[i];
        baudio_channel* channel = &state->channels[channel_id];
        if (!channel->bound_instance && !channel->bound_resource)
        {
            // Available, use it
            return channel;
        }
    }

    BWARN("No channel is available for auto-selection via category, index=%u", category_index);
    return 0;
}
