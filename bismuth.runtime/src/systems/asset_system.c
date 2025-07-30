#include "asset_system.h"

#include "core/engine.h"
// Known handler types
#include "assets/handlers/asset_handler_audio.h"
#include "assets/handlers/asset_handler_binary.h"
#include "assets/handlers/asset_handler_bitmap_font.h"
#include "assets/handlers/asset_handler_heightmap_terrain.h"
#include "assets/handlers/asset_handler_image.h"
#include "assets/handlers/asset_handler_bson.h"
#include "assets/handlers/asset_handler_material.h"
#include "assets/handlers/asset_handler_scene.h"
#include "assets/handlers/asset_handler_shader.h"
#include "assets/handlers/asset_handler_static_mesh.h"
#include "assets/handlers/asset_handler_system_font.h"
#include "assets/handlers/asset_handler_text.h"
#include "platform/vfs.h"
#include "serializers/basset_image_serializer.h"

#include <assets/asset_handler_types.h>
#include <assets/basset_types.h>
#include <assets/basset_utils.h>
#include <containers/darray.h>
#include <containers/u64_bst.h>
#include <core/event.h>
#include <debug/bassert.h>
#include <defines.h>
#include <identifiers/identifier.h>
#include <logger.h>
#include <memory/bmemory.h>
#include <parsers/bson_parser.h>
#include <strings/bname.h>
#include <strings/bstring.h>

typedef struct asset_lookup
{
    // The asset itself, owned by this lookup
    basset* asset;
    // The current number of references to the asset
    i32 reference_count;
    // Indicates if the asset will be released when the reference_count reaches 0
    b8 auto_release;

    u32 file_watch_id;

    void* hot_reload_context;
} asset_lookup;

typedef struct asset_system_state
{
    vfs_state* vfs;

    // The name of the default package to use (i.e, the game's package name)
    bname application_package_name;
    const char* application_package_name_str;

    // Max number of assets that can be loaded at any given time
    u32 max_asset_count;
    // An array of lookups which contain reference and release data
    asset_lookup* lookups;
    // A BST to use for lookups of assets by name
    bt_node* lookup_tree;

    // An array of handlers for various asset types
    asset_handler handlers[BASSET_TYPE_MAX];
} asset_system_state;

b8 asset_system_deserialize_config(const char* config_str, asset_system_config* out_config)
{
    if (!config_str || !out_config)
    {
        BERROR("asset_system_deserialize_config requires a valid string and a pointer to hold the config");
        return false;
    }

    bson_tree tree = {0};
    if (!bson_tree_from_string(config_str, &tree))
    {
        BERROR("Failed to parse asset system configuration");
        return false;
    }

    // max_asset_count
    if (!bson_object_property_value_get_int(&tree.root, "max_asset_count", (i64*)&out_config->max_asset_count))
    {
        BERROR("max_asset_count is a required field and was not provided");
        return false;
    }

    // application_package_name
    if (!bson_object_property_value_get_string(&tree.root, "application_package_name", &out_config->application_package_name_str))
    {
        BERROR("application_package_name is a required field and was not provided");
        return false;
    }
    out_config->application_package_name = bname_create(out_config->application_package_name_str);

    return true;
}

b8 asset_system_initialize(u64* memory_requirement, struct asset_system_state* state, const asset_system_config* config)
{
    if (!memory_requirement)
    {
        BERROR("asset_system_initialize requires a valid pointer to memory_requirement");
        return false;
    }

    *memory_requirement = sizeof(asset_system_state);

    // Just doing a memory size lookup, don't count as a failure
    if (!state)
    {
        return true;
    }
    else if (!config)
    {
        BERROR("asset_system_initialize: A pointer to valid configuration is required. Initialization failed");
        return false;
    }

    state->application_package_name = config->application_package_name;
    state->application_package_name_str = string_duplicate(config->application_package_name_str);

    state->max_asset_count = config->max_asset_count;
    state->lookups = ballocate(sizeof(asset_lookup) * state->max_asset_count, MEMORY_TAG_ENGINE);

    // Asset lookup tree
    {
        // NOTE: BST node created when first asset is requested
        state->lookup_tree = 0;

        // Invalidate all lookups
        for (u32 i = 0; i < state->max_asset_count; ++i)
            state->lookups[i].asset = 0;
    }

    state->vfs = engine_systems_get()->vfs_system_state;

    // Setup handlers for known types
    asset_handler_heightmap_terrain_create(&state->handlers[BASSET_TYPE_HEIGHTMAP_TERRAIN], state->vfs);
    asset_handler_image_create(&state->handlers[BASSET_TYPE_IMAGE], state->vfs);
    asset_handler_static_mesh_create(&state->handlers[BASSET_TYPE_STATIC_MESH], state->vfs);
    asset_handler_material_create(&state->handlers[BASSET_TYPE_MATERIAL], state->vfs);
    asset_handler_text_create(&state->handlers[BASSET_TYPE_TEXT], state->vfs);
    asset_handler_bson_create(&state->handlers[BASSET_TYPE_BSON], state->vfs);
    asset_handler_binary_create(&state->handlers[BASSET_TYPE_BINARY], state->vfs);
    asset_handler_scene_create(&state->handlers[BASSET_TYPE_SCENE], state->vfs);
    asset_handler_shader_create(&state->handlers[BASSET_TYPE_SHADER], state->vfs);
    asset_handler_system_font_create(&state->handlers[BASSET_TYPE_SYSTEM_FONT], state->vfs);
    asset_handler_bitmap_font_create(&state->handlers[BASSET_TYPE_BITMAP_FONT], state->vfs);
    asset_handler_audio_create(&state->handlers[BASSET_TYPE_AUDIO], state->vfs);

    // Register for hot-reload/deleted events
    vfs_hot_reload_callbacks_register(state->vfs, state, asset_hot_reloaded_callback, state, asset_deleted_callback);

    return true;
}

void asset_system_shutdown(struct asset_system_state* state)
{
    if (state)
    {
        if (state->lookups)
        {
            // Unload all currently-held lookups
            for (u32 i = 0; i < state->max_asset_count; ++i)
            {
                asset_lookup* lookup = &state->lookups[i];
                if (lookup->asset)
                {
                    // Force release the asset
                    asset_system_release_internal(state, lookup->asset->name, lookup->asset->package_name, true);
                }
            }
            bfree(state->lookups, sizeof(asset_lookup) * state->max_asset_count, MEMORY_TAG_ARRAY);
        }

        // Destroy the BST
        u64_bst_cleanup(state->lookup_tree);

        bzero_memory(state, sizeof(asset_system_state));
    }
}

// -----------------------------------
// ========== BINARY ASSETS ==========
// -----------------------------------

typedef struct basset_binary_vfs_context
{
    void* listener;
    PFN_basset_binary_loaded_callback callback;
    basset_binary* asset;
} basset_binary_vfs_context;

static void vfs_on_binary_asset_loaded_callback(struct vfs_state* vfs, vfs_asset_data asset_data)
{
    basset_binary_vfs_context* context = asset_data.context;
    basset_binary* out_asset = context->asset;
    out_asset->size = asset_data.size;
    void* content = ballocate(out_asset->size, MEMORY_TAG_ASSET);
    bcopy_memory(content, asset_data.bytes, out_asset->size);
    out_asset->content = content;

    BFREE_TYPE(context, basset_binary_vfs_context, MEMORY_TAG_ASSET);
}
                
// async load from game package.
basset_binary* asset_system_request_binary(struct asset_system_state* state, const char* name, void* listener, PFN_basset_binary_loaded_callback callback)
{
    return asset_system_request_binary_from_package(state, state->application_package_name_str, name, listener, callback);
}

// sync load from game package
basset_binary* asset_system_request_binary_sync(struct asset_system_state* state, const char* name)
{
    return asset_system_request_binary_from_package_sync(state, state->application_package_name_str, name);
}

// async load from specific package.
basset_binary* asset_system_request_binary_from_package(struct asset_system_state* state, const char* package_name, const char* name, void* listener, PFN_basset_binary_loaded_callback callback)
{
    if (!state || !name || !string_length(name))
    {
        BERROR("%s requires valid pointers to state and name", __FUNCTION__);
        return 0;
    }

    basset_binary* out_asset = BALLOC_TYPE(basset_binary, MEMORY_TAG_ASSET);

    basset_binary_vfs_context* context = BALLOC_TYPE(basset_binary_vfs_context, MEMORY_TAG_ASSET);
    context->asset = out_asset;
    context->callback = callback;
    context->listener = listener;

    vfs_request_info info = {
        .asset_name = bname_create(name),
        .package_name = state->application_package_name,
        .get_source = false,
        .is_binary = true,
        .watch_for_hot_reload = false,
        .vfs_callback = vfs_on_binary_asset_loaded_callback,
        .context = context,
        .context_size = sizeof(basset_binary_vfs_context)};
    vfs_request_asset(state->vfs, info);

    return out_asset;
}

// sync load from specific package.
basset_binary* asset_system_request_binary_from_package_sync(struct asset_system_state* state, const char* package_name, const char* name)
{
    if (!state || !name || !string_length(name))
    {
        BERROR("%s requires valid pointers to state and name", __FUNCTION__);
        return 0;
    }

    basset_binary* out_asset = BALLOC_TYPE(basset_binary, MEMORY_TAG_ASSET);
    vfs_request_info info = {
        .asset_name = bname_create(name),
        .package_name = bname_create(package_name),
        .get_source = false,
        .is_binary = true,
        .watch_for_hot_reload = false,
    };
    vfs_asset_data data = vfs_request_asset_sync(state->vfs, info);

    out_asset->size = data.size;
    void* content = ballocate(out_asset->size, MEMORY_TAG_ASSET);
    bcopy_memory(content, data.bytes, out_asset->size);
    out_asset->content = content;

    return out_asset;
}

void asset_system_release_binary(struct asset_system_state* state, basset_binary* asset)
{
    if (state && asset)
    {
        if (asset->content && asset->size)
            bfree((void*)asset->content, asset->size, MEMORY_TAG_ASSET);

        BFREE_TYPE(asset, basset_binary, MEMORY_TAG_ASSET);
    }
}

// ----------------------------------
// ========== IMAGE ASSETS ==========
// ----------------------------------

typedef struct basset_image_vfs_context
{
    void* listener;
    PFN_basset_image_loaded_callback callback;
    basset_image* asset;
} basset_image_vfs_context;

static void vfs_on_image_asset_loaded_callback(struct vfs_state* vfs, vfs_asset_data asset_data)
{
    basset_image_vfs_context* context = asset_data.context;
    basset_image* out_asset = context->asset;
    b8 result = basset_image_deserialize(asset_data.size, asset_data.bytes, out_asset);
    if (!result)
        BERROR("Failed to deserialize image asset. See logs for details");

    BFREE_TYPE(context, basset_image_vfs_context, MEMORY_TAG_ASSET);
}

// async load from game package
basset_image* asset_system_request_image(struct asset_system_state* state, const char* name, b8 flip_y, void* listener, PFN_basset_image_loaded_callback callback)
{
    return asset_system_request_image_from_package(state, state->application_package_name_str, name, flip_y, listener, callback);
}

// sync load from game package
basset_image* asset_system_request_image_sync(struct asset_system_state* state, const char* name, b8 flip_y)
{
    return asset_system_request_image_from_package_sync(state, state->application_package_name_str, name, flip_y);
}

// async load from specific package
basset_image* asset_system_request_image_from_package(struct asset_system_state* state, const char* package_name, const char* name, b8 flip_y, void* listener, PFN_basset_image_loaded_callback callback)
{
    if (!state || !name || !string_length(name))
    {
        BERROR("%s requires valid pointers to state and name", __FUNCTION__);
        return 0;
    }

    basset_image* out_asset = BALLOC_TYPE(basset_image, MEMORY_TAG_ASSET);

    basset_image_vfs_context* context = BALLOC_TYPE(basset_image_vfs_context, MEMORY_TAG_ASSET);
    context->asset = out_asset;
    context->callback = callback;
    context->listener = listener;

    vfs_request_info info = {
        .asset_name = bname_create(name),
        .package_name = state->application_package_name,
        .get_source = false,
        .is_binary = true,
        .watch_for_hot_reload = false,
        .vfs_callback = vfs_on_image_asset_loaded_callback,
        .context = context,
        .context_size = sizeof(basset_image_vfs_context)};
    vfs_request_asset(state->vfs, info);

    return out_asset;
}

// sync load from specific package
basset_image* asset_system_request_image_from_package_sync(struct asset_system_state* state, const char* package_name, const char* name, b8 flip_y)
{
    if (!state || !name || !string_length(name))
    {
        BERROR("%s requires valid pointers to state and name", __FUNCTION__);
        return 0;
    }

    basset_image* out_asset = BALLOC_TYPE(basset_image, MEMORY_TAG_ASSET);
    vfs_request_info info = {
        .asset_name = bname_create(name),
        .package_name = bname_create(package_name),
        .get_source = false,
        .is_binary = true,
        .watch_for_hot_reload = false,
    };
    vfs_asset_data data = vfs_request_asset_sync(state->vfs, info);

    b8 result = basset_image_deserialize(data.size, data.bytes, out_asset);
    if (!result)
    {
        BERROR("Failed to deserialize image asset. See logs for details");
        BFREE_TYPE(out_asset, basset_image, MEMORY_TAG_ASSET);
        return 0;
    }

    return out_asset;
}

void asset_system_release_image(struct asset_system_state* state, basset_image* asset)
{
    if (state && asset)
    {
        if (asset->pixel_array_size && asset->pixels)
            bfree((void*)asset->pixels, asset->pixel_array_size, MEMORY_TAG_ASSET);

        BFREE_TYPE(asset, basset_image, MEMORY_TAG_ASSET);
    }
}
