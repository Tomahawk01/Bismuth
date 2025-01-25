#pragma once

#include "identifiers/bhandle.h"
#include "math/math_types.h"
#include "strings/bname.h"

#include <defines.h>

#define AUDIO_CHANNEL_MAX_COUNT 16

struct baudio_system_state;
struct frame_data;

b8 baudio_system_initialize(u64* memory_requirement, void* memory, const char* config_str);
void baudio_system_shutdown(struct baudio_system_state* state);
b8 baudio_system_update(struct baudio_system_state* state, struct frame_data* p_frame_data);

BAPI b8 baudio_system_listener_orientation_set(struct baudio_system_state* state, vec3 position, vec3 forward, vec3 up);

BAPI void baudio_system_master_volume_set(struct baudio_system_state* state, f32 volume);
BAPI f32 baudio_system_master_volume_get(struct baudio_system_state* state);

BAPI void baudio_system_channel_volume_set(struct baudio_system_state* state, u8 channel_index, f32 volume);
BAPI f32 baudio_system_channel_volume_get(struct baudio_system_state* state, u8 channel_index);

BAPI b8 baudio_acquire(struct baudio_system_state* state, bname resource_name, bname package_name, b8 is_streaming, bhandle* out_audio);
BAPI void baudio_release(struct baudio_system_state* state, bhandle* instance);

BAPI b8 baudio_play(struct baudio_system_state* state, bhandle audio, u8 channel_index);

BAPI b8 baudio_is_valid(struct baudio_system_state* state, bhandle audio);

BAPI f32 baudio_pitch_get(struct baudio_system_state* state, bhandle audio);
BAPI b8 baudio_pitch_set(struct baudio_system_state* state, bhandle audio, f32 pitch);
BAPI f32 baudio_volume_get(struct baudio_system_state* state, bhandle audio);
BAPI b8 baudio_volume_set(struct baudio_system_state* state, bhandle audio, f32 volume);

// BAPI b8 baudio_seek(struct baudio_system_state* state, bhandle audio, f32 seconds);
// BAPI f32 baudio_time_played_get(struct baudio_system_state* state, bhandle audio);
// BAPI f32 baudio_time_length_get(struct baudio_system_state* state, bhandle audio);

BAPI b8 baudio_channel_play(struct baudio_system_state* state, u8 channel_index);
BAPI b8 baudio_channel_pause(struct baudio_system_state* state, u8 channel_index);
BAPI b8 baudio_channel_resume(struct baudio_system_state* state, u8 channel_index);
BAPI b8 baudio_channel_stop(struct baudio_system_state* state, u8 channel_index);
BAPI b8 baudio_channel_is_playing(struct baudio_system_state* state, u8 channel_index);
BAPI b8 baudio_channel_is_paused(struct baudio_system_state* state, u8 channel_index);
BAPI b8 baudio_channel_is_stopped(struct baudio_system_state* state, u8 channel_index);
BAPI b8 baudio_channel_looping_get(struct baudio_system_state* state, u8 channel_index);
BAPI b8 baudio_channel_looping_set(struct baudio_system_state* state, u8 channel_index, b8 looping);
BAPI f32 baudio_channel_volume_get(struct baudio_system_state* state, u8 channel_index);
BAPI b8 baudio_channel_volume_set(struct baudio_system_state* state, u8 channel_index, f32 volume);
BAPI b8 baudio_looping_get(struct baudio_system_state* state, bhandle audio);
BAPI b8 baudio_looping_set(struct baudio_system_state* state, bhandle audio, b8 looping);
