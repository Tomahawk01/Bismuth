#pragma once

#include "identifiers/bhandle.h"
#include "math/math_types.h"
#include "strings/bname.h"

#include <defines.h>

#define AUDIO_CHANNEL_MAX_COUNT 16

struct baudio_system_state;
struct frame_data;

typedef struct bsound_effect_instance
{
    // A handle to the underlying resource
    bhandle resource_handle;
    // The instance handle
    bhandle instance;
} bsound_effect_instance;

typedef struct bmusic_instance
{
    // A handle to the underlying resource
    bhandle resource_handle;
    // The instance handle
    bhandle instance;
} bmusic_instance;

b8 baudio_system_initialize(u64* memory_requirement, void* memory, const char* config_str);
void baudio_system_shutdown(struct baudio_system_state* state);
b8 baudio_system_update(void* state, struct frame_data* p_frame_data);

BAPI b8 baudio_system_listener_orientation_set(struct baudio_system_state* state, vec3 position, vec3 forward, vec3 up);

BAPI void baudio_system_master_volume_set(struct baudio_system_state* state, f32 volume);
BAPI f32 baudio_system_master_volume_get(struct baudio_system_state* state);

BAPI void baudio_system_channel_volume_set(struct baudio_system_state* state, u8 channel_index, f32 volume);
BAPI f32 baudio_system_channel_volume_get(struct baudio_system_state* state, u8 channel_index);

BAPI b8 baudio_sound_effect_acquire(struct baudio_system_state* state, bname resource_name, bname package_name, bsound_effect_instance* out_instance);
BAPI void baudio_sound_effect_release(struct baudio_system_state* state, bsound_effect_instance* instance);

BAPI b8 baudio_sound_play(struct baudio_system_state* state, bsound_effect_instance instance);
BAPI b8 baudio_sound_pause(struct baudio_system_state* state, bsound_effect_instance instance);
BAPI b8 baudio_sound_resume(struct baudio_system_state* state, bsound_effect_instance instance);
BAPI b8 baudio_sound_stop(struct baudio_system_state* state, bsound_effect_instance instance);

BAPI b8 baudio_sound_is_playing(struct baudio_system_state* state, bsound_effect_instance instance);
BAPI b8 baudio_sound_is_valid(struct baudio_system_state* state, bsound_effect_instance instance);

// 0=left, 0.5=center, 1.0=right
BAPI f32 baudio_sound_pan_get(struct baudio_system_state* state, bsound_effect_instance instance);
// 0=left, 0.5=center, 1.0=right
BAPI b8 baudio_sound_pan_set(struct baudio_system_state* state, bsound_effect_instance instance, f32 pan);

BAPI b8 baudio_sound_pitch_get(struct baudio_system_state* state, bsound_effect_instance instance);
BAPI b8 baudio_sound_pitch_set(struct baudio_system_state* state, bsound_effect_instance instance, f32 pitch);

BAPI b8 baudio_sound_volume_get(struct baudio_system_state* state, bsound_effect_instance instance);
BAPI b8 baudio_sound_volume_set(struct baudio_system_state* state, bsound_effect_instance instance, f32 volume);

BAPI b8 baudio_music_acquire(struct baudio_system_state* state, bname resource_name, bname package_name, bmusic_instance* out_instance);
BAPI void baudio_music_release(struct baudio_system_state* state, bmusic_instance* instance);

BAPI b8 baudio_music_play(struct baudio_system_state* state, bmusic_instance instance);
BAPI b8 baudio_music_pause(struct baudio_system_state* state, bmusic_instance instance);
BAPI b8 baudio_music_resume(struct baudio_system_state* state, bmusic_instance instance);
BAPI b8 baudio_music_stop(struct baudio_system_state* state, bmusic_instance instance);

BAPI b8 baudio_music_seek(struct baudio_system_state* state, bmusic_instance instance, f32 seconds);

BAPI f32 baudio_music_time_length_get(struct baudio_system_state* state, bmusic_instance instance);
BAPI f32 baudio_music_time_played_get(struct baudio_system_state* state, bmusic_instance instance);
BAPI b8 baudio_music_is_playing(struct baudio_system_state* state, bmusic_instance instance);
BAPI b8 baudio_music_is_valid(struct baudio_system_state* state, bmusic_instance instance);
BAPI b8 baudio_music_looping_get(struct baudio_system_state* state, bmusic_instance instance);
BAPI b8 baudio_music_looping_set(struct baudio_system_state* state, bmusic_instance instance, b8 looping);

// 0=left, 0.5=center, 1.0=right
BAPI f32 baudio_music_pan_get(struct baudio_system_state* state, bmusic_instance instance);
// 0=left, 0.5=center, 1.0=right
BAPI b8 baudio_music_pan_set(struct baudio_system_state* state, bmusic_instance instance, f32 pan);

BAPI b8 baudio_music_pitch_get(struct baudio_system_state* state, bmusic_instance instance);
BAPI b8 baudio_music_pitch_set(struct baudio_system_state* state, bmusic_instance instance, f32 pitch);

BAPI b8 baudio_music_volume_get(struct baudio_system_state* state, bmusic_instance instance);
BAPI b8 baudio_music_volume_set(struct baudio_system_state* state, bmusic_instance instance, f32 volume);
