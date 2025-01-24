#pragma once

#include "identifiers/bhandle.h"
#include <audio/baudio_types.h>
#include <defines.h>

b8 openal_backend_initialize(baudio_backend_interface* backend, const baudio_backend_config* config);
void openal_backend_shutdown(baudio_backend_interface* backend);

b8 openal_backend_update(baudio_backend_interface* backend, struct frame_data* p_frame_data);

b8 openal_backend_resource_load(baudio_backend_interface* backend, const kresource_audio* resource, b8 is_stream, bhandle resource_handle);
void openal_backend_resource_unload(baudio_backend_interface* backend, bhandle resource_handle);

b8 openal_backend_listener_position_set(baudio_backend_interface* backend, vec3 position);
b8 openal_backend_listener_orientation_set(baudio_backend_interface* backend, vec3 forward, vec3 up);
b8 openal_backend_channel_gain_set(baudio_backend_interface* backend, u8 channel_id, f32 gain);
b8 openal_backend_channel_pitch_set(baudio_backend_interface* backend, u8 channel_id, f32 pitch);
b8 openal_backend_channel_position_set(baudio_backend_interface* backend, u8 channel_id, vec3 position);
b8 openal_backend_channel_looping_set(baudio_backend_interface* backend, u8 channel_id, b8 looping);

b8 openal_backend_channel_play(baudio_backend_interface* backend, u8 channel_id);
b8 openal_backend_channel_play_resource(baudio_backend_interface* backend, bhandle resource_handle, u8 channel_id);

b8 openal_backend_channel_stop(baudio_backend_interface* backend, u8 channel_id);
b8 openal_backend_channel_pause(baudio_backend_interface* backend, u8 channel_id);
b8 openal_backend_channel_resume(baudio_backend_interface* backend, u8 channel_id);

b8 openal_backend_channel_is_playing(baudio_backend_interface* backend, u8 channel_id);
b8 openal_backend_channel_is_paused(baudio_backend_interface* backend, u8 channel_id);
b8 openal_backend_channel_is_stopped(baudio_backend_interface* backend, u8 channel_id);
