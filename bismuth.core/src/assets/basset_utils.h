#pragma once

#include "assets/basset_types.h"
#include "defines.h"
#include "platform/vfs.h"

struct basset_name;

/**
 * @brief Attempts to convert the provided type string to the appropriate enumeration value
 *
 * @param type_str The type string to be examined
 * @return The converted type if successful; otherwise BASSET_TYPE_UNKNOWN
 */
BAPI basset_type basset_type_from_string(const char* type_str);

/**
 * @brief Converts the given asset type enumeration value to its string representation.
 * NOTE: Returns a copy of the string, which should be freed when no longer used
 *
 * @param type The type to be converted
 * @return The string representation of the type
 */
const char* basset_type_to_string(basset_type type);

/**
 * @brief A generic asset "on loaded" handler which can be used (almost) always
 *
 * @param vfs A pointer to the VFS state
 * @param name The name of the asset
 * @param vfs_asset_data The VFS asset data containing the result of the VFS load operation and potential asset data
 */
BAPI void asset_handler_base_on_asset_loaded(struct vfs_state* vfs, vfs_asset_data asset_data);

BAPI u8 channel_count_from_image_format(basset_image_format format);

BAPI const char* basset_material_texture_map_channel_to_string(basset_material_texture_map_channel channel);

BAPI basset_material_texture_map_channel string_to_basset_material_texture_map_channel(const char* str);

/** @brief Returns the string representation of the given material type */
BAPI const char* basset_material_type_to_string(basset_material_type type);

/** @brief Converts the given string into a material type. Case-insensitive */
BAPI basset_material_type string_to_basset_material_type(const char* str);

/** @brief Returns the string representation of the given material model */
BAPI const char* basset_material_model_to_string(basset_material_model model);

/** @brief Converts the given string into a material model. Case-insensitive */
BAPI basset_material_model string_to_basset_material_model(const char* str);
