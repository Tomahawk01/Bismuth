#pragma once

#include "core_audio_types.h"
#include "defines.h"

BAPI baudio_space string_to_audio_space(const char* str);
BAPI const char* audio_space_to_string(baudio_space space);

BAPI baudio_attenuation_model string_to_attenuation_model(const char* str);
BAPI const char* attenuation_model_to_string(baudio_attenuation_model model);

BAPI f32 calculate_spatial_gain(f32 distance, f32 inner_radius, f32 outer_radius, f32 falloff_factor, baudio_attenuation_model model);

BAPI i16* baudio_downmix_stereo_to_mono(const i16* stereo_data, u32 sample_count);
