#include "oal_plugin.h"

#include "defines.h"
#include "parsers/bson_parser.h"
#ifdef BPLATFORM_WINDOWS
#include <malloc.h>
#else
#include <alloca.h>
#endif
#include <threads/bmutex.h>
#include <threads/bthread.h>
#include <math/bmath.h>
#include <platform/platform.h>

#include "defines.h"
#include "audio/audio_types.h"
#include "containers/darray.h"
#include "logger.h"
#include "memory/bmemory.h"
#include "resources/loaders/audio_loader.h"
#include "resources/resource_types.h"
#include "systems/audio_system.h"
#include "systems/job_system.h"
#include "systems/resource_system.h"

// OpenAL
#ifdef BPLATFORM_WINDOWS
#include <al.h>
#include <alc.h>
#else
#include <AL/al.h>
#include <AL/alc.h>
#endif
// Number of buffers used for streaming music file data
#define OAL_PLUGIN_MUSIC_BUFFER_COUNT 2

typedef struct audio_file_plugin_data
{
    // The current buffer being used to play sound effect types
    ALuint buffer;
    // The internal buffers used for streaming music file data
    ALuint buffers[OAL_PLUGIN_MUSIC_BUFFER_COUNT];
    // Indicates if the music file should loop
    b8 is_looping;

} audio_file_plugin_data;

// Sources are used to play sounds, potentially in 3D
typedef struct audio_plugin_source
{
    // Internal OpenAL source
    ALCuint id;
    // Volume
    f32 gain;
    // Pitch, generally left at 1
    f32 pitch;
    // Position of the sound
    vec3 position;
    // Indicates if the source is looping
    b8 looping;
    // Indicates if this souce is in use
    b8 in_use;

    // Worker thread for this source
    bthread thread;

    bmutex data_mutex;  // everything from here down should be accessed/changed during lock
    struct audio_file* current;
    b8 trigger_play;
    b8 trigger_exit;
} audio_plugin_source;

typedef struct audio_plugin_state
{
    /** @brief The maximum number of buffers available. Default: 256 */
    u32 max_buffers;
    /** @brief The maximum number of sources available. Default: 8 */
    u32 max_sources;
    /** @brief The frequency to output audio at */
    u32 frequency;
    /** @brief The number of audio channels to support (i.e. 2 for stereo, 1 for mono) */
    u32 channel_count;

    /** @brief The size to chunk streamed audio data in */
    u32 chunk_size;

    // Selected audio device
    ALCdevice* device;
    // Current audio context
    ALCcontext* context;
    // A pool of buffers to be used for all kinds of audio/music playback
    ALuint* buffers;
    // Total number of buffers available
    u32 buffer_count;

    // Listener's current position in the world
    vec3 listener_position;
    // Listener's current forward vector
    vec3 listener_forward;
    // Listener's current up vector
    vec3 listener_up;

    // Collection of available sources. config.max_sources has the count of this
    audio_plugin_source* sources;

    // An array to keep free/available buffer ids
    u32* free_buffers;
} audio_plugin_state;

static b8 oal_plugin_check_error(void);
static b8 oal_plugin_source_create(struct audio_backend_interface* plugin, audio_plugin_source* out_source);
static void oal_plugin_source_destroy(struct audio_backend_interface* plugin, audio_plugin_source* source);
static u32 oal_plugin_find_free_buffer(struct audio_backend_interface* plugin);
static b8 plugin_deserialize_config(const char* config_str, audio_plugin_state* state);

static b8 oal_plugin_stream_music_data(audio_backend_interface* plugin, ALuint buffer, audio_file* audio)
{
    if (!plugin || !audio)
        return false;

    // Figure out how many samples can be taken
    u64 size = audio->load_samples(audio, plugin->internal_state->chunk_size, plugin->internal_state->chunk_size);
    if (size == INVALID_ID_U64)
    {
        BERROR("Error streaming data. Check logs for more info");
        return false;
    }

    // 0 means the end of the file has been reached, and either the stream stops or needs to start over
    if (size == 0)
        return false;
    oal_plugin_check_error();
    // Load data into the buffer
    void* streamed_data = audio->stream_buffer_data(audio);
    if (streamed_data)
    {
        alBufferData(buffer, audio->format, streamed_data, size * sizeof(ALshort), audio->sample_rate);
        oal_plugin_check_error();
    }
    else
    {
        BERROR("Error streaming data. Check logs for more info");
        return false;
    }

    // Update the samples remaining
    audio->total_samples_left -= size;

    return true;
}
static b8 oal_plugin_stream_update(audio_backend_interface* plugin, audio_file* audio, audio_plugin_source* source)
{
    if (!plugin || !audio)
        return false;

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
        if (!oal_plugin_stream_music_data(plugin, buffer_id, audio))
        {
            b8 done = true;

            // If set to loop, start over at the beginning
            if (audio->plugin_data->is_looping)
            {
                // Loop
                audio->rewind(audio);
                done = !oal_plugin_stream_music_data(plugin, buffer_id, audio);
            }

            // If not set to loop, sound is done playing
            if (done)
                return false;
        }
        // Queue up the next buffer
        alSourceQueueBuffers(source->id, 1, &buffer_id);
    }

    return true;
}

typedef struct source_work_thread_params
{
    struct audio_backend_interface* plugin;
    audio_plugin_source* source;
} source_work_thread_params;

static u32 source_work_thread(void* params)
{
    source_work_thread_params* typed_params = params;
    audio_backend_interface* plugin = typed_params->plugin;
    audio_plugin_source* source = typed_params->source;

    // Release this right away since it's no longer needed
    bfree(params, sizeof(source_work_thread_params), MEMORY_TAG_AUDIO);

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

        if (source->current && source->current->type == AUDIO_FILE_TYPE_MUSIC_STREAM)
        {
            // If currently playing stream, try updating the stream
            oal_plugin_stream_update(plugin, source->current, source);
        }

        platform_sleep(2);
    }

    BDEBUG("Audio source thread shutting down");
    return 0;
}

b8 oal_plugin_initialize(struct audio_backend_interface* plugin, const audio_system_config* config, const char* plugin_config)
{
    if (plugin)
    {
        plugin->internal_state = ballocate(sizeof(audio_plugin_state), MEMORY_TAG_AUDIO);

        // Parse the plugin config string
        if (!plugin_deserialize_config(plugin_config, plugin->internal_state))
        {
            BERROR("Failed to initialize OpenAL backend");
            return false;
        }
        // Copy over the relevant frontend config properties
        plugin->internal_state->max_sources = config->audio_channel_count; // MAX_AUDIO_CHANNELS;
        plugin->internal_state->chunk_size = config->chunk_size;
        plugin->internal_state->frequency = config->frequency;
        plugin->internal_state->channel_count = config->channel_count;

        if (plugin->internal_state->max_sources < 1)
        {
            BWARN("Audio plugin config.max_sources was configured as 0. Defaulting to 8");
            plugin->internal_state->max_sources = 8;
        }

        plugin->internal_state->free_buffers = darray_create(u32);

        // Get default device
        plugin->internal_state->device = alcOpenDevice(0);
        oal_plugin_check_error();
        if (!plugin->internal_state->device)
        {
            BERROR("Unable to obtain OpenAL device. Plugin initialize failed");
            return false;
        }
        else
        {
            BINFO("OpenAL Device acquired");
        }

        // Get context and make it current
        plugin->internal_state->context = alcCreateContext(plugin->internal_state->device, 0);
        oal_plugin_check_error();
        if (!alcMakeContextCurrent(plugin->internal_state->context))
            oal_plugin_check_error();

        // Configure listener with some defaults
        oal_plugin_listener_position_set(plugin, vec3_zero());
        oal_plugin_listener_orientation_set(plugin, vec3_forward(), vec3_up());

        alListener3f(AL_VELOCITY, 0, 0, 0);
        oal_plugin_check_error();

        plugin->internal_state->sources = ballocate(sizeof(audio_plugin_source) * plugin->internal_state->max_sources, MEMORY_TAG_AUDIO);
        // Create all sources
        for (u32 i = 0; i < plugin->internal_state->max_sources; ++i)
        {
            if (!oal_plugin_source_create(plugin, &plugin->internal_state->sources[i]))
            {
                BERROR("Unable to create audio source in OpenAL plugin");
                return false;
            }
        }

        // Buffers
        plugin->internal_state->buffers = ballocate(sizeof(u32) * plugin->internal_state->buffer_count, MEMORY_TAG_ARRAY);
        alGenBuffers(plugin->internal_state->buffer_count, plugin->internal_state->buffers);
        oal_plugin_check_error();

        // Make sure all buffers are marked as free
        for (u32 i = 0; i < plugin->internal_state->buffer_count; ++i)
            darray_push(plugin->internal_state->free_buffers, plugin->internal_state->buffers[i]);

        BINFO("OpenAL plugin intialized");

        return true;
    }

    BERROR("oal_plugin_initialize requires a valid pointer to a plugin");
    return false;
}

void oal_plugin_shutdown(struct audio_backend_interface* plugin)
{
    if (plugin)
    {
        if (plugin->internal_state)
        {
            // Destroy sources
            for (u32 i = 0; i < plugin->internal_state->max_sources; ++i)
                oal_plugin_source_destroy(plugin, &plugin->internal_state->sources[i]);
            if (plugin->internal_state->device)
            {
                alcCloseDevice(plugin->internal_state->device);
                plugin->internal_state->device = 0;
            }
            bfree(plugin->internal_state, sizeof(audio_plugin_state), MEMORY_TAG_AUDIO);
            plugin->internal_state = 0;
        }

        bzero_memory(plugin, sizeof(audio_backend_interface));
    }
}

b8 oal_plugin_update(struct audio_backend_interface* plugin, struct frame_data* p_frame_data)
{
    if (!plugin)
        return false;

    return true;
}

b8 oal_plugin_listener_position_query(struct audio_backend_interface* plugin, vec3* out_position)
{
    if (!plugin || !out_position)
    {
        BERROR("oal_plugin_listener_position_query requires valid pointers to a plugin and out_position");
        return false;
    }

    *out_position = plugin->internal_state->listener_position;
    return true;
}

b8 oal_plugin_listener_position_set(struct audio_backend_interface* plugin, vec3 position)
{
    if (!plugin)
    {
        BERROR("oal_plugin_listener_position_set requires a valid pointer to a plugin");
        return false;
    }

    plugin->internal_state->listener_position = position;
    alListener3f(AL_POSITION, position.x, position.y, position.z);
    oal_plugin_check_error();

    return true;
}

b8 oal_plugin_listener_orientation_query(struct audio_backend_interface* plugin, vec3* out_forward, vec3* out_up)
{
    if (!plugin || !out_forward || !out_up)
    {
        BERROR("oal_plugin_listener_orientation_query requires valid pointers to a plugin, out_forward and out_up");
        return false;
    }

    *out_forward = plugin->internal_state->listener_forward;
    *out_up = plugin->internal_state->listener_up;
    return true;
}

b8 oal_plugin_listener_orientation_set(struct audio_backend_interface* plugin, vec3 forward, vec3 up)
{
    if (!plugin)
    {
        BERROR("oal_plugin_listener_orientation_set requires a valid pointer to a plugin");
        return false;
    }

    plugin->internal_state->listener_forward = forward;
    plugin->internal_state->listener_up = up;
    ALfloat listener_orientation[] = {forward.x, forward.y, forward.z, up.x, up.y, up.z};
    alListenerfv(AL_ORIENTATION, listener_orientation);
    return oal_plugin_check_error();
}

static b8 source_set_defaults(struct audio_backend_interface* plugin, audio_plugin_source* source, b8 reset_use)
{
    // Mark it as not in use
    if (reset_use)
        source->in_use = false;

    // Set some defaults
    if (!oal_plugin_source_gain_set(plugin, source->id - 1, 1.0f))
    {
        BERROR("Failed to set source default gain");
        return false;
    }
    if (!oal_plugin_source_pitch_set(plugin, source->id - 1, 1.0f))
    {
        BERROR("Failed to set source default pitch");
        return false;
    }
    if (!oal_plugin_source_position_set(plugin, source->id - 1, vec3_zero()))
    {
        BERROR("Failed to set source default position");
        return false;
    }
    if (!oal_plugin_source_looping_set(plugin, source->id - 1, false))
    {
        BERROR("Failed to set source default looping");
        return false;
    }

    return true;
}

static b8 oal_plugin_source_create(struct audio_backend_interface* plugin, audio_plugin_source* out_source)
{
    if (!plugin || !out_source)
    {
        BERROR("oal_plugin_source_create requires valid pointers to a plugin and out_source");
        return false;
    }

    alGenSources((ALuint)1, &out_source->id);
    if (!oal_plugin_check_error())
    {
        BERROR("Failed to create source");
        return false;
    }

    if (!source_set_defaults(plugin, out_source, true))
        BERROR("Failed to set source defaults, and thus failed to create source");

    // Create source worker thread's mutex
    bmutex_create(&out_source->data_mutex);

    // Also create worker thread itself for this source
    source_work_thread_params* params = ballocate(sizeof(source_work_thread_params), MEMORY_TAG_AUDIO);
    params->source = out_source;
    params->plugin = plugin;
    bthread_create(source_work_thread, params, true, &out_source->thread);

    return true;
}

static void oal_plugin_source_destroy(struct audio_backend_interface* plugin, audio_plugin_source* source)
{
    if (plugin && source)
    {
        alDeleteSources(1, &source->id);
        bzero_memory(source, sizeof(audio_plugin_source));
        source->id = INVALID_ID;
    }
}

static void oal_plugin_find_playing_sources(struct audio_backend_interface* plugin, u32 playing[], u32* count)
{
    if (!plugin || !count)
        return;

    ALint state = 0;
    for (u32 i = 0; i < plugin->internal_state->max_sources; ++i)
    {
        alGetSourcei(plugin->internal_state->sources[i - 1].id, AL_SOURCE_STATE, &state);
        if (state == AL_PLAYING)
        {
            playing[(*count)] = plugin->internal_state->sources[i - 1].id;
            (*count)++;
        }
    }
}

static void clear_buffer(struct audio_backend_interface* plugin, u32* buf_ptr, u32 amount)
{
    if (plugin)
    {
        for (u32 a = 0; a < amount; ++a)
        {
            for (u32 i = 0; i < plugin->internal_state->buffer_count; ++i)
            {
                if (buf_ptr[a] == plugin->internal_state->buffers[i])
                {
                    darray_push(plugin->internal_state->free_buffers, i);
                    return;
                }
            }
        }
    }
    BWARN("Buffer could not be cleared");
}

static u32 oal_plugin_find_free_buffer(struct audio_backend_interface* plugin)
{
    if (plugin)
    {
        u32 free_count = darray_length(plugin->internal_state->free_buffers);

        // If there are no free buffers, attempt to free one first
        if (free_count == 0)
        {
            BINFO("oal_plugin_find_free_buffer() - no free buffers, attempting to free an existing one");
            if (!oal_plugin_check_error())
                return false;

            u32 playing_source_count = 0;
            u32* playing_sources = ballocate(sizeof(u32) * plugin->internal_state->max_sources, MEMORY_TAG_ARRAY);
            oal_plugin_find_playing_sources(plugin, playing_sources, &playing_source_count);
            // Avoid a crash when calling alGetSourcei while checking for freeable buffers
            for (u32 i = 0; i < playing_source_count; ++i)
            {
                alSourcePause(plugin->internal_state->sources[i - 1].id);
                oal_plugin_check_error();
            }

            ALint to_be_freed = 0;
            ALuint buffers_freed = 0;
            for (u32 i = 0; i < plugin->internal_state->buffer_count; ++i)
            {
                // Get number of buffers to be freed for this source
                alGetSourcei(plugin->internal_state->sources[i - 1].id, AL_BUFFERS_PROCESSED, &to_be_freed);
                oal_plugin_check_error();
                if (to_be_freed > 0)
                {
                    // If there are buffers to be freed, free them
                    oal_plugin_check_error();
                    alSourceUnqueueBuffers(plugin->internal_state->sources[i - 1].id, to_be_freed, &buffers_freed);
                    oal_plugin_check_error();

                    clear_buffer(plugin, &buffers_freed, to_be_freed);
                }
            }

            // Resume paused sources
            for (u32 i = 0; i < playing_source_count; ++i)
            {
                alSourcePlay(plugin->internal_state->sources[i - 1].id);
                oal_plugin_check_error();
            }
            bfree(playing_sources, sizeof(u32) * plugin->internal_state->max_sources, MEMORY_TAG_ARRAY);
        }

        // Check free count again, this time there must be at least one or there is an error condition
        free_count = darray_length(plugin->internal_state->free_buffers);
        if (free_count < 1)
        {
            BERROR("Could not find or clear a buffer. This means too many things are being played at once");
            return INVALID_ID;
        }

        u32 out_buffer_id;
        darray_pop_at(plugin->internal_state->free_buffers, 0, &out_buffer_id);

        free_count = darray_length(plugin->internal_state->free_buffers);
        BTRACE("Found free buffer id %u", out_buffer_id);
        BDEBUG("There are now %u free buffers remaining", free_count);
        return out_buffer_id;
    }

    return INVALID_ID;
}

b8 oal_plugin_source_gain_query(struct audio_backend_interface* plugin, u32 source_index, f32* out_gain)
{
    if (plugin && out_gain && source_index <= plugin->internal_state->max_sources)
    {
        *out_gain = plugin->internal_state->sources[source_index].gain;
        return true;
    }

    BERROR("Plugin pointer invalid or source id is invalid: %u", source_index);
    return false;
}

b8 oal_plugin_source_gain_set(struct audio_backend_interface* plugin, u32 source_index, f32 gain)
{
    if (plugin && source_index <= plugin->internal_state->max_sources)
    {
        audio_plugin_source* source = &plugin->internal_state->sources[source_index];
        source->gain = gain;
        alSourcef(source->id, AL_GAIN, gain);
        return oal_plugin_check_error();
    }

    BERROR("Plugin pointer invalid or source id is invalid: %u", source_index);
    return false;
}

b8 oal_plugin_source_pitch_query(struct audio_backend_interface* plugin, u32 source_index, f32* out_pitch)
{
    if (plugin && out_pitch && source_index <= plugin->internal_state->max_sources)
    {
        *out_pitch = plugin->internal_state->sources[source_index].pitch;
        return true;
    }

    BERROR("Plugin pointer invalid or source id is invalid: %u", source_index);
    return false;
}

b8 oal_plugin_source_pitch_set(struct audio_backend_interface* plugin, u32 source_index, f32 pitch)
{
    if (plugin && source_index <= plugin->internal_state->max_sources)
    {
        audio_plugin_source* source = &plugin->internal_state->sources[source_index];
        source->pitch = pitch;
        alSourcef(source->id, AL_PITCH, pitch);
        return oal_plugin_check_error();
    }

    BERROR("Plugin pointer invalid or source id is invalid: %u", source_index);
    return false;
}

b8 oal_plugin_source_position_query(struct audio_backend_interface* plugin, u32 source_index, vec3* out_position)
{
    if (plugin && out_position && source_index <= plugin->internal_state->max_sources)
    {
        *out_position = plugin->internal_state->sources[source_index].position;
        return true;
    }

    BERROR("Plugin pointer invalid or source id is invalid: %u", source_index);
    return false;
}

b8 oal_plugin_source_position_set(struct audio_backend_interface* plugin, u32 source_index, vec3 position)
{
    if (plugin && source_index <= plugin->internal_state->max_sources)
    {
        audio_plugin_source* source = &plugin->internal_state->sources[source_index];
        source->position = position;
        alSource3f(source->id, AL_POSITION, position.x, position.y, position.z);
        return oal_plugin_check_error();
    }

    BERROR("Plugin pointer invalid or source id is invalid: %u", source_index);
    return false;
}

b8 oal_plugin_source_looping_query(struct audio_backend_interface* plugin, u32 source_index, b8* out_looping)
{
    if (plugin && out_looping && source_index <= plugin->internal_state->max_sources)
    {
        *out_looping = plugin->internal_state->sources[source_index].looping;
        return true;
    }

    BERROR("Plugin pointer invalid or source id is invalid: %u", source_index);
    return false;
}

b8 oal_plugin_source_looping_set(struct audio_backend_interface* plugin, u32 source_index, b8 looping)
{
    if (plugin && source_index <= plugin->internal_state->max_sources)
    {
        audio_plugin_source* source = &plugin->internal_state->sources[source_index];
        source->looping = looping;
        alSourcei(source->id, AL_LOOPING, looping ? AL_TRUE : AL_FALSE);
        return oal_plugin_check_error();
    }

    BERROR("Plugin pointer invalid or source id is invalid: %u", source_index);
    return false;
}

static const char* oal_plugin_error_str(ALCenum err)
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

static b8 oal_plugin_check_error(void)
{
    ALCenum error = alGetError();
    if (error != AL_NO_ERROR)
    {
        BERROR("OpenAL error %u: '%s'", error, oal_plugin_error_str(error));
        return false;
    }
    return true;
}

struct audio_file* oal_plugin_stream_load(struct audio_backend_interface* plugin, const char* name)
{
    if (!plugin)
        return 0;

    if (!plugin || !name)
    {
        BERROR("oal_plugin_open_music_file requires a valid pointer to plugin and name");
        return 0;
    }

    // Load up the resource
    audio_file* out_file = 0;
    audio_resource_loader_params params = {0};
    params.type = AUDIO_FILE_TYPE_MUSIC_STREAM;
    params.chunk_size = plugin->internal_state->chunk_size;
    resource audio_resource;
    if (!resource_system_load(name, RESOURCE_TYPE_AUDIO, &params, &audio_resource))
    {
        BERROR("Failed to open audio resource. Load failed");
        return 0;
    }

    out_file = audio_resource.data;

    // Setup plugin state
    out_file->plugin_data = ballocate(sizeof(audio_file_plugin_data), MEMORY_TAG_AUDIO);

    // Get some buffers to be used back to back
    for (u32 i = 0; i < OAL_PLUGIN_MUSIC_BUFFER_COUNT; ++i)
    {
        out_file->plugin_data->buffers[i] = oal_plugin_find_free_buffer(plugin);
        if (out_file->plugin_data->buffers[i] == INVALID_ID)
        {
            BERROR("Unable to open music file due to no buffers being available");
            return 0;
        }
    }
    oal_plugin_check_error();

    out_file->format = AL_FORMAT_MONO16;
    if (out_file->channels == 2)
        out_file->format = AL_FORMAT_STEREO16;

    out_file->plugin_data->is_looping = true;

    return out_file;
}

struct audio_file* oal_plugin_chunk_load(struct audio_backend_interface* plugin, const char* name)
{
    if (!plugin)
        return 0;

    // Load up the sound file. This also loads the data into a buffer
    if (!plugin || !name)
    {
        BERROR("oal_plugin_open_sound_file requires valid pointers to plugin and name");
        return 0;
    }

    // Load up the resource
    audio_file* out_file = 0;
    audio_resource_loader_params params = {0};
    params.type = AUDIO_FILE_TYPE_SOUND_EFFECT;
    params.chunk_size = plugin->internal_state->chunk_size;
    resource* audio_resource = ballocate(sizeof(resource), MEMORY_TAG_RESOURCE);
    if (!resource_system_load(name, RESOURCE_TYPE_AUDIO, &params, audio_resource))
    {
        BERROR("Failed to open audio resource. Load failed");
        return 0;
    }

    out_file = audio_resource->data;

    // Setup plugin state
    out_file->plugin_data = ballocate(sizeof(audio_file_plugin_data), MEMORY_TAG_AUDIO);

    // Get a buffer
    out_file->plugin_data->buffer = oal_plugin_find_free_buffer(plugin);
    if (out_file->plugin_data->buffer == INVALID_ID)
    {
        bfree(out_file->plugin_data, sizeof(audio_file_plugin_data), MEMORY_TAG_AUDIO);
        resource_system_unload(audio_resource);
        BERROR("Unable to open audio file due to no buffers being available");
        return false;
    }
    oal_plugin_check_error();

    // Format
    out_file->format = AL_FORMAT_MONO16;
    if (out_file->channels == 2)
        out_file->format = AL_FORMAT_STEREO16;

    if (out_file->total_samples_left > 0)
    {
        // Load whole thing into the buffer
        void* pcm = out_file->stream_buffer_data(out_file);
        oal_plugin_check_error();
        alBufferData(out_file->plugin_data->buffer, out_file->format, (i16*)pcm, out_file->total_samples_left, out_file->sample_rate);
        oal_plugin_check_error();
        return out_file;
    }

    // Error condition, free up everything and return
    if (out_file && out_file->plugin_data)
        bfree(out_file->plugin_data, sizeof(audio_file_plugin_data), MEMORY_TAG_AUDIO);
    resource_system_unload(audio_resource);
    return 0;
}

void oal_plugin_audio_file_close(struct audio_backend_interface* plugin, struct audio_file* file)
{
    if (!plugin || !file)
    {
        BERROR("oal_plugin_audio_file_close requires valid pointers to plugin and file");
        return;
    }

    clear_buffer(plugin, &file->plugin_data->buffer, 0);

    // Clear plugin data
    bfree(file->plugin_data, sizeof(audio_file_plugin_data), MEMORY_TAG_AUDIO);

    resource* r = file->audio_resource;
    resource_system_unload(r);
}

b8 oal_plugin_source_play(struct audio_backend_interface* plugin, i8 source_index)
{
    if (!plugin || source_index < 0)
        return false;

    audio_plugin_source* source = &plugin->internal_state->sources[source_index];
    bmutex_lock(&source->data_mutex);
    if (source->current)
    {
        source->trigger_play = true;
        source->in_use = true;
    }
    bmutex_unlock(&source->data_mutex);

    return true;
}

b8 oal_plugin_play_on_source(struct audio_backend_interface* plugin, struct audio_file* file, i8 source_index)
{
    if (!plugin || !file || source_index < 0)
        return false;
    BTRACE("Play on source %d", source_index);

    // Assign sound's buffer to the source
    audio_plugin_source* source = &plugin->internal_state->sources[source_index];
    bmutex_lock(&source->data_mutex);

    if (file->type == AUDIO_FILE_TYPE_SOUND_EFFECT)
    {
        // Queue up sound buffer
        alSourceQueueBuffers(source->id, 1, &file->plugin_data->buffer);
        oal_plugin_check_error();
    }
    else
    {
        // Load data into all buffers initially
        b8 result = true;
        for (u32 i = 0; i < OAL_PLUGIN_MUSIC_BUFFER_COUNT; ++i)
        {
            if (!oal_plugin_stream_music_data(plugin, file->plugin_data->buffers[i], file))
            {
                BERROR("Failed to stream data to buffer &u in music file. File load failed", i);
                result = false;
                break;
            }
        }
        // Queue up new buffers
        alSourceQueueBuffers(source->id, OAL_PLUGIN_MUSIC_BUFFER_COUNT, file->plugin_data->buffers);
        oal_plugin_check_error();
        if (!result)
        {
            // ...
        }
    }

    // Assign current, set flags, play, etc
    source->current = file;
    source->in_use = true;
    alSourcePlay(source->id);
    bmutex_unlock(&source->data_mutex);

    return true;
}

b8 oal_plugin_source_stop(struct audio_backend_interface* plugin, i8 source_index)
{
    if (!plugin || source_index < 0)
        return false;

    audio_plugin_source* source = &plugin->internal_state->sources[source_index];

    alSourceStop(source->id);

    // Detach all buffers
    alSourcei(source->id, AL_BUFFER, 0);
    oal_plugin_check_error();

    alSourceRewind(source->id);

    source->in_use = false;

    return true;
}

b8 oal_plugin_source_pause(struct audio_backend_interface* plugin, i8 source_index)
{
    if (!plugin || source_index < 0)
        return false;

    // Trigger a pause if source is currently playing
    audio_plugin_source* source = &plugin->internal_state->sources[source_index];
    ALint source_state;
    alGetSourcei(source->id, AL_SOURCE_STATE, &source_state);
    if (source_state == AL_PLAYING)
        alSourcePause(source->id);

    return true;
}

b8 oal_plugin_source_resume(struct audio_backend_interface* plugin, i8 source_index)
{
    if (!plugin || source_index < 0)
        return false;

    // Trigger a resume if source is currently paused
    audio_plugin_source* source = &plugin->internal_state->sources[source_index];
    ALint source_state;
    alGetSourcei(source->id, AL_SOURCE_STATE, &source_state);
    if (source_state == AL_PAUSED)
        alSourcePlay(source->id);

    return true;
}

static b8 plugin_deserialize_config(const char* config_str, audio_plugin_state* state)
{
    if (!config_str || !state)
    {
        BERROR("plugin_deserialize_config requires a valid pointer to out_config and state");
        return false;
    }

    bson_tree tree = {0};
    if (!bson_tree_from_string(config_str, &tree))
    {
        BERROR("Failed to parse audio plugin config");
        return false;
    }

    i64 max_buffers = 0;
    if (!bson_object_property_value_get_int(&tree.root, "max_buffers", &max_buffers))
        max_buffers = 256;

    if (max_buffers < 20)
    {
        BWARN("Audio plugin config.max_buffers was configured to be less than 20, the recommended minimum. Defaulting to 256");
        state->max_buffers = 256;
    }
    state->buffer_count = max_buffers;

    bson_tree_cleanup(&tree);

    return true;
}
