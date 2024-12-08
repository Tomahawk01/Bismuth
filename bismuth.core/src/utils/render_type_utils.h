#pragma once

#include "core_render_types.h"

/** @brief Indicates if the given shader uniform type is a sampler */
BAPI b8 uniform_type_is_sampler(shader_uniform_type type);

/** @brief Indicates if the given shader uniform type is a texture */
BAPI b8 uniform_type_is_texture(shader_uniform_type type);

/** @brief Returns the string representation of the given texture repeat */
BAPI const char* texture_repeat_to_string(texture_repeat repeat);

/** @brief Converts the given string into a texture repeat. Case-insensitive */
BAPI texture_repeat string_to_texture_repeat(const char* str);

/** @brief Returns the string representation of the given texture filter */
BAPI const char* texture_filter_mode_to_string(texture_filter filter);

/** @brief Converts the given string into a texture filter. Case-insensitive */
BAPI texture_filter string_to_texture_filter_mode(const char* str);

BAPI const char* texture_channel_to_string(texture_channel channel);

BAPI texture_channel string_to_texture_channel(const char* str);

/** @brief Returns the string representation of the given shader uniform type */
BAPI const char* shader_uniform_type_to_string(shader_uniform_type type);

/** @brief Converts the given string into a shader uniform type. Case-insensitive */
BAPI shader_uniform_type string_to_shader_uniform_type(const char* str);

/** @brief Returns the string representation of the given shader attribute type */
BAPI const char* shader_attribute_type_to_string(shader_attribute_type type);

/** @brief Converts the given string into a shader attribute type. Case-insensitive */
BAPI shader_attribute_type string_to_shader_attribute_type(const char* str);

/** @brief Returns the string representation of the given shader stage */
BAPI const char* shader_stage_to_string(shader_stage stage);

/** @brief Converts the given string into a shader stage. Case-insensitive */
BAPI shader_stage string_to_shader_stage(const char* str);

/** @brief Returns the string representation of the given shader update frequency */
BAPI const char* shader_update_frequency_to_string(shader_update_frequency frequency);

/** @brief Converts the given string into a shader update frequency. Case-insensitive */
BAPI shader_update_frequency string_to_shader_update_frequency(const char* str);

/** @brief Returns the string representation of the given cull mode */
BAPI const char* face_cull_mode_to_string(face_cull_mode mode);

/** @brief Converts the given string to a face cull mode */
BAPI face_cull_mode string_to_face_cull_mode(const char* str);

/** @brief Returns the string representation of the given primitive topology type */
BAPI const char* topology_type_to_string(primitive_topology_type_bits type);

/** @brief Converts the given string to a primitive topology type */
BAPI primitive_topology_type_bits string_to_topology_type(const char* str);

/** @brief Returns the size in bytes of the attribute type */
BAPI u16 size_from_shader_attribute_type(shader_attribute_type type);

/** @brief Returns the size in bytes of the uniform type */
BAPI u16 size_from_shader_uniform_type(shader_uniform_type type);

/** @brief Returns the string representation of the given material type */
BAPI const char* bmaterial_type_to_string(bmaterial_type type);

/** @brief Converts the given string into a material type. Case-insensitive */
BAPI bmaterial_type string_to_bmaterial_type(const char* str);

/** @brief Returns the string representation of the given material model */
BAPI const char* bmaterial_model_to_string(bmaterial_model model);

/** @brief Converts the given string into a material model. Case-insensitive */
BAPI bmaterial_model string_to_bmaterial_model(const char* str);
