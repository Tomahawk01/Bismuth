#pragma once

#include <defines.h>
#include <identifiers/bhandle.h>
#include <math/math_types.h>
#include <strings/bname.h>

#include "audio/baudio_types.h"
#include "core_audio_types.h"

#define AUDIO_CHANNEL_MAX_COUNT 16

struct baudio_system_state;
struct frame_data;

b8 baudio_system_initialize(u64* memory_requirement, void* memory, const char* config_str);
void baudio_system_shutdown(struct baudio_system_state* state);
b8 baudio_system_update(struct baudio_system_state* state, struct frame_data* p_frame_data);

BAPI void baudio_system_listener_orientation_set(struct baudio_system_state* state, vec3 position, vec3 forward, vec3 up);

BAPI void baudio_master_volume_set(struct baudio_system_state* state, f32 volume);
BAPI f32 baudio_system_master_volume_get(struct baudio_system_state* state);

BAPI b8 baudio_acquire(struct baudio_system_state* state, bname resource_name, bname package_name, b8 is_streaming, baudio_space audio_space, audio_instance* out_audio_instance);
BAPI void baudio_release(struct baudio_system_state* state, audio_instance* instance);

BAPI vec3 baudio_position_get(struct baudio_system_state* state, audio_instance instance);
BAPI b8 baudio_position_set(struct baudio_system_state* state, audio_instance instance, vec3 position);
BAPI f32 baudio_inner_radius_get(struct baudio_system_state* state, audio_instance instance);
BAPI b8 baudio_inner_radius_set(struct baudio_system_state* state, audio_instance instance, f32 inner_radius);
BAPI f32 baudio_outer_radius_get(struct baudio_system_state* state, audio_instance instance);
BAPI b8 baudio_outer_radius_set(struct baudio_system_state* state, audio_instance instance, f32 outer_radius);
BAPI f32 baudio_falloff_get(struct baudio_system_state* state, audio_instance instance);
BAPI b8 baudio_falloff_set(struct baudio_system_state* state, audio_instance instance, f32 falloff);

BAPI b8 baudio_play(struct baudio_system_state* state, audio_instance instance, i8 channel_index);
BAPI b8 baudio_stop(struct baudio_system_state* state, audio_instance instance);
BAPI b8 baudio_pause(struct baudio_system_state* state, audio_instance instance);
BAPI b8 baudio_resume(struct baudio_system_state* state, audio_instance instance);
BAPI b8 baudio_is_valid(struct baudio_system_state* state, audio_instance instance);
BAPI f32 baudio_pitch_get(struct baudio_system_state* state, audio_instance instance);
BAPI b8 baudio_pitch_set(struct baudio_system_state* state, audio_instance instance, f32 pitch);
BAPI f32 baudio_volume_get(struct baudio_system_state* state, audio_instance instance);
BAPI b8 baudio_volume_set(struct baudio_system_state* state, audio_instance instance, f32 volume);
BAPI b8 baudio_looping_get(struct baudio_system_state* state, audio_instance instance);
BAPI b8 baudio_looping_set(struct baudio_system_state* state, audio_instance instance, b8 looping);

// BAPI b8 baudio_seek(struct baudio_system_state* state, audio_instance instance, f32 seconds);
// BAPI f32 baudio_time_played_get(struct baudio_system_state* state, audio_instance instance);
// BAPI f32 baudio_time_length_get(struct baudio_system_state* state, audio_instance instance);

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
