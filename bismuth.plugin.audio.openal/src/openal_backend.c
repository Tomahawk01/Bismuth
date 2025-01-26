#include "openal_backend.h"

// OpenAL
#ifdef BPLATFORM_WINDOWS
#include <al.h>
#include <alc.h>
#else
#include <AL/al.h>
#include <AL/alc.h>
#endif

// Core
#include <defines.h>
#include <identifiers/bhandle.h>
#include <containers/darray.h>
#include <logger.h>
#include <memory/bmemory.h>
#include <math/bmath.h>
#include <platform/platform.h>
#include <threads/bmutex.h>
#include <threads/bthread.h>

// #ifdef BPLATFORM_WINDOWS
// #include <malloc.h>
// #else
// #include <alloca.h>
// #endif

// Runtime
#include <bresources/bresource_types.h>
#include <audio/baudio_types.h>
#include <systems/job_system.h>

// The number of buffers used for streaming music file data
#define OPENAL_BACKEND_STREAM_MAX_BUFFER_COUNT 2

// This corresponds to a resource instance on the frontend
typedef struct baudio_resource_data
{
    // The openal sound format (i.e. 16-bit mono/stereo)
    u32 format;
    // The current buffer being used to play sound effect types
    ALuint buffer;
    // The internal buffers used for streaming music file data
    ALuint buffers[OPENAL_BACKEND_STREAM_MAX_BUFFER_COUNT];
    // Indicates if the music file should loop
    b8 is_looping;
    // Indicates if the internal resource should be streamed or all loaded at once
    b8 is_stream;

    // Used to track samples in streaming type files
    u32 total_samples_left;

    // A pointer to the audio resource used here
    const bresource_audio* resource;
} baudio_resource_data;

// Sources are used to play sounds, potentially at a space in 3D
typedef struct baudio_plugin_source
{
    // Internal OpenAL source
    ALCuint id;

    // Worker thread for this source
    bthread thread;

    bmutex data_mutex; // everything from here down should be accessed/changed during lock
    // Currently playing resource data. Is null if not in use
    baudio_resource_data* current;
    b8 trigger_play;
    b8 trigger_exit;
} baudio_plugin_source;

// The internal state for this audio backend
typedef struct baudio_backend_state
{
    /** @brief The maximum number of buffers available. Default: 256 */
    u32 max_buffers;
    /** @brief The maximum number of sources available.
     * These map to "channels" on the frontend.
     * Default: 8
     * */
    u32 max_sources;

    /** @brief The frequency to output audio at */
    u32 frequency;
    /** @brief The number of audio channels to support (i.e. 2 for stereo, 1 for mono) */
    u32 channel_count;

    /** @brief The size to chunk streamed audio data in */
    u32 chunk_size;
    
    // The selected audio device
    ALCdevice* device;
    // The current audio context
    ALCcontext* context;
    // A pool of buffers to be used for all kinds of audio/music playback
    ALuint* buffers;
    // The total number of buffers available
    u32 buffer_count;

    // The listener's current position in the world
    vec3 listener_position;
    // The listener's current forward vector
    vec3 listener_forward;
    // The listener's current up vector
    vec3 listener_up;

    // A collection of available sources. config.max_sources has the count of this
    baudio_plugin_source* sources;

    // An array to keep free/available buffer ids
    u32* free_buffers;

    // The max number of resources that can be loaded at any one time. Synced with frontend
    u32 max_resource_count;

    // Resource array aligning with that of the frontend
    baudio_resource_data* resources;
} baudio_backend_state;

typedef struct bsource_work_thread_params
{
    baudio_backend_interface* backend;
    baudio_plugin_source* source;
} bsource_work_thread_params;

static b8 openal_backend_check_error(void);
static b8 openal_backend_channel_create(baudio_backend_interface* backend, baudio_plugin_source* out_source);
static void openal_backend_channel_destroy(baudio_backend_interface* backend, baudio_plugin_source* source);
static u32 openal_backend_find_free_buffer(baudio_backend_interface* backend);

static b8 stream_resource_data(baudio_backend_interface* plugin, ALuint buffer, baudio_resource_data* resource);
static b8 openal_backend_stream_update(baudio_backend_interface* plugin, baudio_resource_data* resource, baudio_plugin_source* source);
static u32 source_work_thread(void* params);
static b8 source_set_defaults(baudio_backend_interface* backend, baudio_plugin_source* source, b8 reset_use);
static b8 openal_backend_channel_create(baudio_backend_interface* backend, baudio_plugin_source* out_source);
static void openal_backend_channel_destroy(baudio_backend_interface* backend, baudio_plugin_source* source);
static void openal_backend_find_playing_sources(baudio_backend_interface* backend, u32 playing[], u32* count);
static void clear_buffer(baudio_backend_interface* backend, u32* buf_ptr, u32 amount);
static u32 openal_backend_find_free_buffer(baudio_backend_interface* backend);
static const char* openal_backend_error_str(ALCenum err);
static b8 openal_backend_check_error(void);
static b8 channel_id_valid(baudio_backend_state* state, u8 channel_id);

b8 openal_backend_initialize(baudio_backend_interface* backend, const baudio_backend_config* config)
{
    if (backend)
    {
        backend->internal_state = ballocate(sizeof(baudio_backend_state), MEMORY_TAG_AUDIO);
        baudio_backend_state* state = backend->internal_state;

        // Copy over the relevant frontend config properties
        state->max_sources = config->audio_channel_count; // MAX_AUDIO_CHANNELS;
        state->chunk_size = config->chunk_size;
        state->frequency = config->frequency;
        state->channel_count = config->channel_count;
        state->max_resource_count = config->max_resource_count;
        state->resources = BALLOC_TYPE_CARRAY(baudio_resource_data, state->max_resource_count);

        state->buffer_count = 256;

        if (state->max_sources < 1)
        {
            BWARN("Audio plugin config.max_sources was configured as 0. Defaulting to 8");
            state->max_sources = 8;
        }

        // Get the default device. TODO: enumerate devices and pick via ALC_ENUMERATION_EXT?
        state->device = alcOpenDevice(0);
        openal_backend_check_error();
        if (!state->device)
        {
            BERROR("Unable to obtain OpenAL device. Plugin initialize failed");
            return false;
        }
        else
        {
            BINFO("OpenAL Device acquired");
        }

        // Get context and make it current
        state->context = alcCreateContext(state->device, 0);
        openal_backend_check_error();
        if (!alcMakeContextCurrent(state->context))
            openal_backend_check_error();

        // Configure the listener with some defaults
        openal_backend_listener_position_set(backend, vec3_zero());
        openal_backend_listener_orientation_set(backend, vec3_forward(), vec3_up());

        // NOTE: zeroing out velocity - not sure if we ever need to bother setting this
        alListener3f(AL_VELOCITY, 0, 0, 0);
        openal_backend_check_error();

        state->sources = ballocate(sizeof(baudio_plugin_source) * state->max_sources, MEMORY_TAG_AUDIO);
        // Create all sources
        for (u32 i = 0; i < state->max_sources; ++i)
        {
            if (!openal_backend_channel_create(backend, &state->sources[i]))
            {
                BERROR("Unable to create audio source in OpenAL plugin");
                return false;
            }
        }

        // Buffers
        // TODO: Should make a pool for this
        state->buffers = ballocate(sizeof(u32) * state->buffer_count, MEMORY_TAG_ARRAY);
        alGenBuffers(state->buffer_count, state->buffers);
        openal_backend_check_error();

        state->free_buffers = darray_create(u32);

        // Make sure all buffers are marked as free. Note that the array of buffers retrieved above must be used directly,
        // as there is no guarantee as to what the buffer ids will be
        for (u32 i = 0; i < state->buffer_count; ++i)
            darray_push(state->free_buffers, state->buffers[i]);

        // NOTE: source generation, which is basically a sound emitter
        BINFO("OpenAL plugin intialized");

        return true;
    }

    BERROR("openal_backend_initialize requires a valid pointer to a backend");
    return false;
}

void openal_backend_shutdown(baudio_backend_interface* backend)
{
    if (backend)
    {
        if (backend->internal_state)
        {
            // Destroy sources
            for (u32 i = 0; i < backend->internal_state->max_sources; ++i)
                openal_backend_channel_destroy(backend, &backend->internal_state->sources[i]);

            if (backend->internal_state->device)
            {
                alcCloseDevice(backend->internal_state->device);
                backend->internal_state->device = 0;
            }
            bfree(backend->internal_state, sizeof(baudio_backend_state), MEMORY_TAG_AUDIO);
            backend->internal_state = 0;
        }

        bzero_memory(backend, sizeof(baudio_backend_interface));
    }
}

b8 openal_backend_update(baudio_backend_interface* backend, struct frame_data* p_frame_data)
{
    if (!backend)
        return false;

    return true;
}

b8 openal_backend_resource_load(baudio_backend_interface* backend, const bresource_audio* resource, b8 is_stream, bhandle resource_handle)
{
    if (!backend || !backend->internal_state)
    {
        BERROR("openal_backend_resource_load requires a valid pointer to backend");
        return false;
    }
    if (bhandle_is_invalid(resource_handle))
    {
        BERROR("openal_backend_resource_load requires a valid handle");
        return false;
    }

    baudio_backend_state* state = backend->internal_state;

    // Get the internal resource data
    baudio_resource_data* data = &state->resources[resource_handle.handle_index];
    data->is_stream = is_stream;

    data->format = AL_FORMAT_MONO16;
    if (resource->channels == 2)
        data->format = AL_FORMAT_STEREO16;

    data->resource = resource;
    data->total_samples_left = data->resource->total_sample_count;

    if (is_stream)
    {
        // Streams need buffers to be used back to back
        for (u32 i = 0; i < OPENAL_BACKEND_STREAM_MAX_BUFFER_COUNT; ++i)
        {
            data->buffers[i] = openal_backend_find_free_buffer(backend);
            if (data->buffers[i] == INVALID_ID)
            {
                BERROR("Unable to load streaming audio resource due to no buffers being available");
                return 0;
            }

            // Streams loop by default
            data->is_looping = true;
        }
        openal_backend_check_error();
    }
    else
    {
        // Non-streams only need one buffer
        data->buffer = openal_backend_find_free_buffer(backend);
        if (data->buffer == INVALID_ID)
        {
            BERROR("Unable to open audio file due to no buffers being available");
            return false;
        }
        openal_backend_check_error();

        if (data->total_samples_left > 0)
        {
            // Load the whole thing into the buffer
            alBufferData(data->buffer, data->format, (i16*)resource->pcm_data, data->total_samples_left, resource->sample_rate);
            openal_backend_check_error();
        }

        // Non-streams do not loop by default
        data->is_looping = false;
    }

    return true;
}

void openal_backend_resource_unload(baudio_backend_interface* backend, bhandle resource_handle)
{
    if (!backend || bhandle_is_invalid(resource_handle))
    {
        BERROR("openal_backend_resource_unload requires a valid pointer to plugin and a valid resource_handle");
        return;
    }

    baudio_backend_state* state = backend->internal_state;

    // Get the internal resource data
    baudio_resource_data* data = &state->resources[resource_handle.handle_index];

    clear_buffer(backend, &data->buffer, 0);

    // FIXME: Mark resource entry as available for use

    // TODO: release resource?
}

b8 openal_backend_listener_position_set(baudio_backend_interface* backend, vec3 position)
{
    if (!backend)
    {
        BERROR("openal_backend_listener_position_set requires a valid pointer to a plugin");
        return false;
    }

    alListener3f(AL_POSITION, position.x, position.y, position.z);
    openal_backend_check_error();

    return true;
}

b8 openal_backend_listener_orientation_set(baudio_backend_interface* backend, vec3 forward, vec3 up)
{
    if (!backend)
    {
        BERROR("openal_backend_listener_orientation_set requires a valid pointer to a plugin");
        return false;
    }

    ALfloat listener_orientation[] = {forward.x, forward.y, forward.z, up.x, up.y, up.z};
    alListenerfv(AL_ORIENTATION, listener_orientation);
    return openal_backend_check_error();
}

b8 openal_backend_channel_gain_set(baudio_backend_interface* backend, u8 channel_id, f32 gain)
{
    if (!backend)
        return false;

    baudio_backend_state* state = backend->internal_state;

    if (channel_id_valid(state, channel_id))
    {
        baudio_plugin_source* source = &state->sources[channel_id];
        alSourcef(source->id, AL_GAIN, gain);
        return openal_backend_check_error();
    }

    BERROR("Plugin pointer invalid or source id is invalid: %u", channel_id);
    return false;
}

b8 openal_backend_channel_pitch_set(baudio_backend_interface* backend, u8 channel_id, f32 pitch)
{
    if (!backend)
        return false;

    baudio_backend_state* state = backend->internal_state;

    if (channel_id_valid(state, channel_id))
    {
        baudio_plugin_source* source = &state->sources[channel_id];
        alSourcef(source->id, AL_PITCH, pitch);
        return openal_backend_check_error();
    }

    BERROR("Plugin pointer invalid or source id is invalid: %u", channel_id);
    return false;
}

b8 openal_backend_channel_position_set(baudio_backend_interface* backend, u8 channel_id, vec3 position)
{
    if (!backend)
        return false;

    baudio_backend_state* state = backend->internal_state;

    if (channel_id_valid(state, channel_id))
    {
        baudio_plugin_source* source = &state->sources[channel_id];
        alSource3f(source->id, AL_POSITION, position.x, position.y, position.z);
        return openal_backend_check_error();
    }

    BERROR("Plugin pointer invalid or source id is invalid: %u", channel_id);
    return false;
}

b8 openal_backend_channel_looping_set(baudio_backend_interface* backend, u8 channel_id, b8 looping)
{
    if (!backend)
        return false;

    baudio_backend_state* state = backend->internal_state;

    if (channel_id_valid(state, channel_id))
    {
        baudio_plugin_source* source = &state->sources[channel_id];
        alSourcei(source->id, AL_LOOPING, looping ? AL_TRUE : AL_FALSE);
        return openal_backend_check_error();
    }

    BERROR("Plugin pointer invalid or source id is invalid: %u", channel_id);
    return false;
}

b8 openal_backend_channel_play(baudio_backend_interface* backend, u8 channel_id)
{
    if (!backend)
        return false;

    baudio_backend_state* state = backend->internal_state;
    if (channel_id_valid(state, channel_id))
    {
        baudio_plugin_source* source = &state->sources[channel_id];
        bmutex_lock(&source->data_mutex);
        if (source->current)
            source->trigger_play = true;
        bmutex_unlock(&source->data_mutex);
    }

    return true;
}

b8 openal_backend_channel_play_resource(baudio_backend_interface* backend, bhandle resource_handle, u8 channel_id)
{
    if (!backend || bhandle_is_invalid(resource_handle) || !channel_id_valid(backend->internal_state, channel_id))
        return false;

    BTRACE("Play on channel %d", channel_id);
    baudio_backend_state* state = backend->internal_state;
    baudio_resource_data* data = &state->resources[resource_handle.handle_index];

    // Assign the sound's buffer to the source
    baudio_plugin_source* source = &state->sources[channel_id];
    bmutex_lock(&source->data_mutex);

    if (data->is_stream)
    {
        // Load data into all buffers initially
        b8 result = true;
        for (u32 i = 0; i < OPENAL_BACKEND_STREAM_MAX_BUFFER_COUNT; ++i)
        {
            if (!stream_resource_data(backend, data->buffers[i], data))
            {
                BERROR("Failed to stream data to buffer &u in music file. File load failed", i);
                result = false;
                break;
            }
        }
        // Queue up new buffers
        alSourceQueueBuffers(source->id, OPENAL_BACKEND_STREAM_MAX_BUFFER_COUNT, data->buffers);
        openal_backend_check_error();
        if (!result)
        {
            BERROR("Failed to stream resource data. See logs for details");
            return false;
        }
    }
    else
    {
        // Queue up sound buffer
        alSourceQueueBuffers(source->id, 1, &data->buffer);
        openal_backend_check_error();
    }
    
    // Assign current, set flags, play, etc
    source->current = data;
    alSourcePlay(source->id);
    bmutex_unlock(&source->data_mutex);

    return true;
}

b8 openal_backend_channel_stop(baudio_backend_interface* backend, u8 channel_id)
{
    if (!backend)
        return false;

    baudio_backend_state* state = backend->internal_state;
    if (channel_id_valid(state, channel_id))
    {
        baudio_plugin_source* source = &state->sources[channel_id];

        alSourceStop(source->id);

        // Detach all buffers
        alSourcei(source->id, AL_BUFFER, 0);
        openal_backend_check_error();

        // Rewind
        alSourceRewind(source->id);

        source->current = 0;

        return true;
    }

    return false;
}

b8 openal_backend_channel_pause(baudio_backend_interface* backend, u8 channel_id)
{
    if (!backend)
        return false;

    baudio_backend_state* state = backend->internal_state;

    if (channel_id_valid(state, channel_id))
    {
        // Trigger a pause if the source is currently playing
        baudio_plugin_source* source = &state->sources[channel_id];
        ALint source_state;
        alGetSourcei(source->id, AL_SOURCE_STATE, &source_state);
        if (source_state == AL_PLAYING)
            alSourcePause(source->id);
        return true;
    }

    return false;
}

b8 openal_backend_channel_resume(baudio_backend_interface* backend, u8 channel_id)
{
    if (!backend)
        return false;

    baudio_backend_state* state = backend->internal_state;

    if (channel_id_valid(state, channel_id))
    {
        // Trigger a resume if the source is currently paused
        baudio_plugin_source* source = &state->sources[channel_id];
        ALint source_state;
        alGetSourcei(source->id, AL_SOURCE_STATE, &source_state);
        if (source_state == AL_PAUSED)
            alSourcePlay(source->id);
        return true;
    }

    return false;
}

b8 openal_backend_channel_is_playing(baudio_backend_interface* backend, u8 channel_id)
{
    if (!backend)
        return false;

    baudio_backend_state* state = backend->internal_state;
    if (channel_id_valid(state, channel_id))
    {
        ALint source_state;
        alGetSourcei(state->sources[channel_id].id, AL_SOURCE_STATE, &source_state);
        return source_state == AL_PLAYING;
    }

    return false;
}

b8 openal_backend_channel_is_paused(baudio_backend_interface* backend, u8 channel_id)
{
    if (!backend)
        return false;

    baudio_backend_state* state = backend->internal_state;
    if (channel_id_valid(state, channel_id))
    {
        ALint source_state;
        alGetSourcei(state->sources[channel_id].id, AL_SOURCE_STATE, &source_state);
        return source_state == AL_PAUSED;
    }

    return false;
}

b8 openal_backend_channel_is_stopped(baudio_backend_interface* backend, u8 channel_id)
{
    if (!backend)
        return false;

    baudio_backend_state* state = backend->internal_state;
    if (channel_id_valid(state, channel_id))
    {
        ALint source_state;
        alGetSourcei(state->sources[channel_id].id, AL_SOURCE_STATE, &source_state);
        return source_state == AL_STOPPED || source_state == AL_INITIAL;
    }

    return false;
}

static b8 stream_resource_data(baudio_backend_interface* backend, ALuint buffer, baudio_resource_data* resource)
{
    if (!backend || !resource)
        return false;

    baudio_backend_state* state = backend->internal_state;

    // Figure out how many samples can be taken
    // TODO: This might be _way_ too much between chunk size and samples (maybe samples left * channels?)
    u64 sample_count = BMIN(resource->total_samples_left, state->chunk_size);
    // 0 means the end of the file has been reached, and either the stream stops or needs to start over
    if (sample_count == 0)
    {
        BTRACE("End of file reached. Returning false");
        return false;
    }

    openal_backend_check_error();
    // Load the data into the buffer. Just a pointer into the pcm_data at an offset
    u64 pos = resource->resource->total_sample_count - resource->total_samples_left;
    i16* streamed_data = resource->resource->pcm_data + pos;
    if (streamed_data)
    {
        alBufferData(buffer, resource->format, streamed_data, sample_count * sizeof(ALshort), resource->resource->sample_rate);
        openal_backend_check_error();
    }
    else
    {
        BERROR("Error streaming data. Check logs for more info");
        return false;
    }

    // Update the samples remaining
    resource->total_samples_left -= sample_count;
    return true;
}

static b8 openal_backend_stream_update(baudio_backend_interface* backend, baudio_resource_data* resource, baudio_plugin_source* source)
{
    if (!backend || !resource)
        return false;

    // It's possible sometimes for this to not be playing, even with buffers queued up.
    // Make sure to handle this case
    ALint source_state;
    alGetSourcei(source->id, AL_SOURCE_STATE, &source_state);
    if (source_state != AL_PLAYING)
    {
        BTRACE("Stream update, play needed for source id: %u", source->id);
        alSourcePlay(source->id);
    }

    // Check for processed buffers that can be popped off
    ALint processed_buffer_count = 0;
    alGetSourcei(source->id, AL_BUFFERS_PROCESSED, &processed_buffer_count);

    while (processed_buffer_count--)
    {
        ALuint buffer_id = 0;
        alSourceUnqueueBuffers(source->id, 1, &buffer_id);

        // If this returns false, there was nothing further to read (i.e at the end of the file)
        if (!stream_resource_data(backend, buffer_id, resource))
        {
            BTRACE("stream_resource_data returned false");
            b8 done = true;

            // If set to loop, start over at the beginning
            if (resource->is_looping)
            {
                BTRACE("Resource set to loop. Rewinding and starting over");
                // Loop around
                resource->total_samples_left = resource->resource->total_sample_count;
                /* audio->rewind(audio); */
                done = !stream_resource_data(backend, buffer_id, resource);
            }

            // If not set to loop, the sound is done playing
            if (done)
            {
                BTRACE("Sound is done playing");
                return false;
            }
        }

        // Queue up the next buffer
        alSourceQueueBuffers(source->id, 1, &buffer_id);
    }

    return true;
}

static u32 source_work_thread(void* params)
{
    bsource_work_thread_params* typed_params = params;
    baudio_backend_interface* backend = typed_params->backend;
    baudio_plugin_source* source = typed_params->source;

    // Release this right away since it's no longer needed
    bfree(params, sizeof(bsource_work_thread_params), MEMORY_TAG_AUDIO);

    BDEBUG("Audio source thread starting...");

    b8 do_break = false;
    while (!do_break)
    {
        if (!source->data_mutex.internal_data)
        {
            // This can happen during unexpected shutdown, and if so kill the thread
            return 0;
        }
        bmutex_lock(&source->data_mutex);
        if (source->trigger_exit)
            do_break = true;
        if (source->trigger_play)
        {
            alSourcePlay(source->id);
            source->trigger_play = false;
        }
        bmutex_unlock(&source->data_mutex);

        if (source->current && source->current->is_stream)
        {
            // If currently playing stream, try updating the stream
            openal_backend_stream_update(backend, source->current, source);
        }

        platform_sleep(2);
    }

    BDEBUG("Audio source thread shutting down");
    return 0;
}

static b8 source_set_defaults(baudio_backend_interface* backend, baudio_plugin_source* source, b8 reset_use)
{
    // Mark it as not in use
    if (reset_use)
        source->current = 0;

    // Set some defaults. FIXME: Define these instead of having magic numbers
    if (!openal_backend_channel_gain_set(backend, source->id - 1, 1.0f))
    {
        BERROR("Failed to set source default gain");
        return false;
    }
    if (!openal_backend_channel_pitch_set(backend, source->id - 1, 1.0f))
    {
        BERROR("Failed to set source default pitch");
        return false;
    }
    if (!openal_backend_channel_position_set(backend, source->id - 1, vec3_zero()))
    {
        BERROR("Failed to set source default position");
        return false;
    }
    if (!openal_backend_channel_looping_set(backend, source->id - 1, false))
    {
        BERROR("Failed to set source default looping");
        return false;
    }

    return true;
}

static b8 openal_backend_channel_create(baudio_backend_interface* backend, baudio_plugin_source* out_source)
{
    if (!backend || !out_source)
    {
        BERROR("openal_backend_channel_create requires valid pointers to a plugin and out_source");
        return false;
    }

    alGenSources((ALuint)1, &out_source->id);
    if (!openal_backend_check_error())
    {
        BERROR("Failed to create source");
        return false;
    }

    if (!source_set_defaults(backend, out_source, true))
        BERROR("Failed to set source defaults, and thus failed to create source");

    // Create the source worker thread's mutex
    bmutex_create(&out_source->data_mutex);

    // Also create the worker thread itself for this source
    bsource_work_thread_params* params = ballocate(sizeof(bsource_work_thread_params), MEMORY_TAG_AUDIO);
    params->source = out_source;
    params->backend = backend;
    bthread_create(source_work_thread, params, true, &out_source->thread);

    return true;
}

static void openal_backend_channel_destroy(baudio_backend_interface* backend, baudio_plugin_source* source)
{
    if (backend && source)
    {
        alDeleteSources(1, &source->id);
        bzero_memory(source, sizeof(baudio_plugin_source));
        source->id = INVALID_ID;
    }
}

static void openal_backend_find_playing_sources(baudio_backend_interface* backend, u32 playing[], u32* count)
{
    if (!backend || !count)
        return;

    baudio_backend_state* state = backend->internal_state;

    ALint source_state = 0;
    for (u32 i = 0; i < state->max_sources; ++i)
    {
        alGetSourcei(state->sources[i - 1].id, AL_SOURCE_STATE, &source_state);
        if (source_state == AL_PLAYING)
        {
            playing[(*count)] = state->sources[i - 1].id;
            (*count)++;
        }
    }
}

static void clear_buffer(baudio_backend_interface* backend, u32* buf_ptr, u32 amount)
{
    if (backend)
    {
        baudio_backend_state* state = backend->internal_state;

        for (u32 a = 0; a < amount; ++a)
        {
            for (u32 i = 0; i < state->buffer_count; ++i)
            {
                if (buf_ptr[a] == state->buffers[i])
                {
                    darray_push(state->free_buffers, i);
                    return;
                }
            }
        }
    }

    BWARN("Buffer could not be cleared");
}

static u32 openal_backend_find_free_buffer(baudio_backend_interface* backend)
{
    if (backend)
    {
        baudio_backend_state* state = backend->internal_state;
        u32 free_count = darray_length(state->free_buffers);

        // If there are no free buffers, attempt to free one first
        if (free_count == 0)
        {
            BINFO("openal_backend_find_free_buffer() - no free buffers, attempting to free an existing one");
            if (!openal_backend_check_error())
                return false;

            u32 playing_source_count = 0;
            u32* playing_sources = ballocate(sizeof(u32) * state->max_sources, MEMORY_TAG_ARRAY);
            openal_backend_find_playing_sources(backend, playing_sources, &playing_source_count);
            // Avoid a crash when calling alGetSourcei while checking for freeable buffers. Resumed below
            for (u32 i = 0; i < playing_source_count; ++i)
            {
                alSourcePause(state->sources[i - 1].id);
                openal_backend_check_error();
            }

            ALint to_be_freed = 0;
            ALuint buffers_freed = 0;
            for (u32 i = 0; i < state->buffer_count; ++i)
            {
                // Get number of buffers to be freed for this source
                alGetSourcei(state->sources[i - 1].id, AL_BUFFERS_PROCESSED, &to_be_freed);
                openal_backend_check_error();
                if (to_be_freed > 0)
                {
                    // If there are buffers to be freed, free them
                    openal_backend_check_error();
                    alSourceUnqueueBuffers(state->sources[i - 1].id, to_be_freed, &buffers_freed);
                    openal_backend_check_error();

                    clear_buffer(backend, &buffers_freed, to_be_freed);
                    /* alSourcePlay(state->sources[i - 1].id); */
                }
            }

            // Resume the paused sources
            for (u32 i = 0; i < playing_source_count; ++i)
            {
                alSourcePlay(state->sources[i - 1].id);
                openal_backend_check_error();
            }
            bfree(playing_sources, sizeof(u32) * state->max_sources, MEMORY_TAG_ARRAY);
        }

        // Check free count again, this time there must be at least one or there is an error condition
        free_count = darray_length(state->free_buffers);
        if (free_count < 1)
        {
            BERROR("Could not find or clear a buffer. This means too many things are being played at once");
            return INVALID_ID;
        }

        // Nab the first one off the top
        u32 out_buffer_id;
        darray_pop_at(state->free_buffers, 0, &out_buffer_id);

        free_count = darray_length(state->free_buffers);
        BTRACE("Found free buffer id %u", out_buffer_id);
        BDEBUG("There are now %u free buffers remaining", free_count);
        return out_buffer_id;
    }

    return INVALID_ID;
}

static const char* openal_backend_error_str(ALCenum err)
{
    switch (err)
    {
    case AL_INVALID_VALUE:
        return "AL_INVALID_VALUE";
    case AL_INVALID_NAME:
        return "AL_INVALID_NAME or ALC_INVALID_DEVICE";
    case AL_INVALID_OPERATION:
        return "AL_INVALID_OPERATION";
    case AL_NO_ERROR:
        return "AL_NO_ERROR";
    case AL_OUT_OF_MEMORY:
        return "AL_OUT_OF_MEMORY or could not find audio device";
    default:
        return "Unknown/unhandled error";
    }
}

static b8 openal_backend_check_error(void)
{
    ALCenum error = alGetError();
    
    if (error != AL_NO_ERROR)
    {
        BERROR("OpenAL error %u: '%s'", error, openal_backend_error_str(error));
        return false;
    }

    return true;
}

static b8 channel_id_valid(baudio_backend_state* state, u8 channel_id)
{
    return state && channel_id < state->max_sources;
}
