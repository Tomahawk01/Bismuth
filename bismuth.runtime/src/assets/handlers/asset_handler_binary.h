#pragma once

#include <assets/asset_handler_types.h>

BAPI void asset_handler_binary_create(struct asset_handler* self, struct vfs_state* vfs);
BAPI void asset_handler_binary_release_asset(struct asset_handler* self, struct basset* asset);
