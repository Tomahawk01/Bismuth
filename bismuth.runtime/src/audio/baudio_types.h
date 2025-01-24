#pragma once

#include <defines.h>
#include <identifiers/bhandle.h>
#include <math/math_types.h>

#include "bresources/bresource_types.h"

struct frame_data;
struct baudio_backend_state;

/** @brief The configuration for an audio backend */
typedef struct baudio_backend_config
{
    /** @brief The frequency to output audio at (i.e. 44100) */
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
} baudio_backend_config;

typedef struct baudio_backend_interface
{
    struct baudio_backend_state* internal_state;

    b8 (*initialize)(struct baudio_backend_interface* backend, const baudio_backend_config* config);
    void (*shutdown)(struct baudio_backend_interface* backend);
    b8 (*update)(struct baudio_backend_interface* backend, struct frame_data* p_frame_data);

    b8 (*listener_position_set)(struct baudio_backend_interface* backend, vec3 position);
    b8 (*listener_orientation_set)(struct baudio_backend_interface* backend, vec3 forward, vec3 up);
    
    b8 (*channel_gain_set)(struct baudio_backend_interface* backend, u8 channel_id, f32 gain);
    b8 (*channel_pitch_set)(struct baudio_backend_interface* backend, u8 channel_id, f32 pitch);
    b8 (*channel_position_set)(struct baudio_backend_interface* backend, u8 channel_id, vec3 position);
    b8 (*channel_looping_set)(struct baudio_backend_interface* backend, u8 channel_id, b8 looping);

    b8 (*resource_load)(struct baudio_backend_interface* backend, const bresource_audio* resource, bhandle resource_handle);
    void (*resource_unload)(struct baudio_backend_interface* backend, bhandle resource_handle);

    b8 (*channel_play)(struct baudio_backend_interface* backend, u8 channel_id);
    b8 (*channel_play_resource)(struct baudio_backend_interface* backend, bhandle resource_handle, u8 channel_id);
    
    b8 (*channel_stop)(struct baudio_backend_interface* backend, u8 channel_id);
    b8 (*channel_pause)(struct baudio_backend_interface* backend, u8 channel_id);
    b8 (*channel_resume)(struct baudio_backend_interface* backend, u8 channel_id);
} baudio_backend_interface;
