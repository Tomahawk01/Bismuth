#pragma once

#include "assets/basset_types.h"
#include "core_render_types.h"
#include "math/math_types.h"

/** @brief Returns the string representation of the given texture repeat */
BAPI const char* texture_repeat_to_string(texture_repeat repeat);

/** @brief Returns the string representation of the given texture filter */
BAPI const char* texture_filter_mode_to_string(texture_filter filter);

/** @brief Returns the string representation of the given shader uniform type */
BAPI const char* shader_uniform_type_to_string(shader_uniform_type type);

/** @brief Returns the string representation of the given shader attribute type */
BAPI const char* shader_attribute_type_to_string(shader_attribute_type type);

/** @brief Returns the string representation of the given shader stage */
BAPI const char* shader_stage_to_string(shader_stage stage);

/** @brief Returns the string representation of the given shader scope */
BAPI const char* shader_scope_to_string(shader_scope stage);

/** @brief Converts the given string into a texture repeat. Case-insensitive */
BAPI texture_repeat string_to_texture_repeat(const char* str);

/** @brief Converts the given string into a texture filter. Case-insensitive */
BAPI texture_filter string_to_texture_filter_mode(const char* str);

/** @brief Converts the given string into a shader uniform type. Case-insensitive */
BAPI shader_uniform_type string_to_shader_uniform_type(const char* str);

/** @brief Converts the given string into a shader attribute type. Case-insensitive */
BAPI shader_attribute_type string_to_shader_attribute_type(const char* str);

/** @brief Converts the given string into a shader stage. Case-insensitive */
BAPI shader_stage string_to_shader_stage(const char* str);

/** @brief Converts the given string into a shader scope. Case-insensitive */
BAPI shader_scope string_to_shader_scope(const char* str);

/** @brief Returns the string representation of the given material type */
BAPI const char* bmaterial_type_to_string(bmaterial_type type);

/** @brief Converts the given string into a material type. Case-insensitive */
BAPI bmaterial_type string_to_bmaterial_type(const char* str);

BAPI const char* material_map_channel_to_string(basset_material_map_channel channel);

BAPI basset_material_map_channel string_to_material_map_channel(const char* str);
