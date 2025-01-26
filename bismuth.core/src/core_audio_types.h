#pragma once

#define AUDIO_INNER_RADIUS_MIN 0.0f
#define AUDIO_INNER_RADIUS_MAX 65535.0f
#define AUDIO_INNER_RADIUS_DEFAULT 1.0f

#define AUDIO_OUTER_RADIUS_MIN 0.1f
#define AUDIO_OUTER_RADIUS_MAX 65536.0f
#define AUDIO_OUTER_RADIUS_DEFAULT 2.0f

#define AUDIO_FALLOFF_MIN 0.1f
#define AUDIO_FALLOFF_MAX 10.0f
#define AUDIO_FALLOFF_DEFAULT 1.0f

#define AUDIO_PITCH_MIN 0.5f
#define AUDIO_PITCH_MAX 2.0f
#define AUDIO_PITCH_DEFAULT 1.0f

#define AUDIO_VOLUME_MIN 0.0f
#define AUDIO_VOLUME_MAX 1.0f
#define AUDIO_VOLUME_DEFAULT 1.0f

typedef enum baudio_space
{
    /** @brief The audio does not exist in the world, but perhaps in an overlay such as UI or music overlay */
    BAUDIO_SPACE_2D,
    /** @brief The audio exists in the world and is attenuated based on 3d space */
    BAUDIO_SPACE_3D
} baudio_space;

typedef enum baudio_attenuation_model
{
    /** @brief Simple, constant rate of attenuation. Sound falls off linearly between inner and outer radii. Falloff parameter is ignored */
    BAUDIO_ATTENUATION_MODEL_LINEAR,
    /** @brief Sharper or softer depending on falloff factor. Sound falls off exponentially as determined by falloff parameter. 1.0 essentially makes it linear */
    BAUDIO_ATTENUATION_MODEL_EXPONENTIAL,
    /** @brief Gentle falloff of sound. Sound falls off logarithmically. Falloff parameter is ignored */
    BAUDIO_ATTENUATION_MODEL_LOGARITHMIC,
    /** @brief Smooth, gradual transitions. Sound falls off in a smooth step fashion. Falloff parameter is ignored */
    BAUDIO_ATTENUATION_MODEL_SMOOTHERSTEP
} baudio_attenuation_model;
