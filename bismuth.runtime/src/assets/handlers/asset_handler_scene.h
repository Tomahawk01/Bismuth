#pragma once

#include <assets/asset_handler_types.h>

BAPI void asset_handler_scene_create(struct asset_handler* self, struct vfs_state* vfs);
BAPI void asset_handler_scene_request_asset(struct asset_handler* self, struct basset* asset, void* listener_instance, PFN_basset_on_result user_callback);
BAPI void asset_handler_scene_release_asset(struct asset_handler* self, struct basset* asset);
