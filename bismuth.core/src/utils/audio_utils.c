#include "audio_utils.h"

#include "core_audio_types.h"
#include "math/bmath.h"
#include "memory/bmemory.h"
#include "strings/bstring.h"

baudio_space string_to_audio_space(const char* str)
{
    if (strings_equali(str, "2d"))
        return BAUDIO_SPACE_2D;
    else if (strings_equali(str, "3d"))
        return BAUDIO_SPACE_3D;

    return BAUDIO_SPACE_2D;
}

const char* audio_space_to_string(baudio_space space)
{
    switch (space)
    {
    case BAUDIO_SPACE_2D:
        return "2D";
    case BAUDIO_SPACE_3D:
        return "3D";
    }
}

baudio_attenuation_model string_to_attenuation_model(const char* str)
{
    if (strings_equali(str, "linear"))
        return BAUDIO_ATTENUATION_MODEL_LINEAR;
    else if (strings_equali(str, "exponential"))
        return BAUDIO_ATTENUATION_MODEL_EXPONENTIAL;
    else if (strings_equali(str, "logarithmic"))
        return BAUDIO_ATTENUATION_MODEL_LOGARITHMIC;
    else if (strings_equali(str, "smootherstep"))
        return BAUDIO_ATTENUATION_MODEL_SMOOTHERSTEP;

    return BAUDIO_ATTENUATION_MODEL_LINEAR;
}

const char* attenuation_model_to_string(baudio_attenuation_model model)
{
    switch (model)
    {
    case BAUDIO_ATTENUATION_MODEL_LINEAR:
        return "linear";
    case BAUDIO_ATTENUATION_MODEL_EXPONENTIAL:
        return "exponential";
    case BAUDIO_ATTENUATION_MODEL_LOGARITHMIC:
        return "logarithmic";
    case BAUDIO_ATTENUATION_MODEL_SMOOTHERSTEP:
        return "smootherstep";
    }
}

f32 calculate_spatial_gain(f32 distance, f32 inner_radius, f32 outer_radius, f32 falloff_factor, baudio_attenuation_model model)
{
    if (distance <= inner_radius)
        return 1.0f; // Play at full volume
    if (distance >= outer_radius)
        return 0.0f; // Completely faded out/zero volume

    f32 gain = 0.0f;

    switch (model)
    {
    default:
    case BAUDIO_ATTENUATION_MODEL_LINEAR:
    {
        // Linear attenuation (ignores falloff)
        f32 normalized_distance = (distance - inner_radius) / (outer_radius - inner_radius);
        gain = 1.0f - normalized_distance;
    } break;
    case BAUDIO_ATTENUATION_MODEL_EXPONENTIAL:
    {
        // Exponential attenuation
        f32 normalized_distance = (distance - inner_radius) / (outer_radius - inner_radius);
        gain = bpow(1.0f - normalized_distance, falloff_factor);
    } break;
    case BAUDIO_ATTENUATION_MODEL_LOGARITHMIC:
    {
        // Logarithmic attenuation
        gain = blog(outer_radius / distance) / blog(outer_radius / inner_radius);
    } break;
    case BAUDIO_ATTENUATION_MODEL_SMOOTHERSTEP:
    {
        // Smoother step attenuation
        f32 normalized_distance = (distance - inner_radius) / (outer_radius - inner_radius);
        gain = 1.0f - (6.0f * bpow(normalized_distance, 5.0f) - 15.0f * bpow(normalized_distance, 4.0f) + 10.0f * bpow(normalized_distance, 3.0f));
    } break;
    }

    return gain;
}

i16* baudio_downmix_stereo_to_mono(const i16* stereo_data, u32 sample_count)
{
    if (!stereo_data || !sample_count)
        return 0;

    u32 mono_sample_count = sample_count / 2;
    i16* mono_data = ballocate(mono_sample_count * sizeof(u16), MEMORY_TAG_AUDIO);
    if (!mono_data)
        return 0;

    for (u32 i = 0; i < mono_sample_count; ++i)
    {
        i16 left = stereo_data[2 * i];
        i16 right = stereo_data[2 * i + 1];

        // Put them together using an int in case both sides are loud
        i32 combined_mono = left + right;

        // Scale it by half to bring it back into range
        mono_data[i] = (i16)(combined_mono / 2);
    }
    
    return mono_data;
}
