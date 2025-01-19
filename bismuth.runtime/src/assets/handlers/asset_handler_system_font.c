#include "asset_handler_system_font.h"
#include "assets/basset_types.h"
#include "strings/bname.h"

#include <assets/asset_handler_types.h>
#include <assets/basset_utils.h>
#include <core/engine.h>
#include <debug/bassert.h>
#include <logger.h>
#include <math/bmath.h>
#include <memory/bmemory.h>
#include <parsers/bson_parser.h>
#include <platform/vfs.h>
#include <serializers/basset_system_font_serializer.h>
#include <strings/bstring.h>

static void asset_handler_system_font_on_asset_loaded(struct vfs_state* vfs, vfs_asset_data asset_data);

void asset_handler_system_font_create(struct asset_handler* self, struct vfs_state* vfs)
{
    BASSERT_MSG(self && vfs, "Valid pointers are required for 'self' and 'vfs'");

    self->vfs = vfs;
    self->is_binary = false;
    self->request_asset = asset_handler_system_font_request_asset;
    self->release_asset = asset_handler_system_font_release_asset;
    self->type = BASSET_TYPE_SYSTEM_FONT;
    self->type_name = BASSET_TYPE_NAME_SYSTEM_FONT;
    self->binary_serialize = 0;
    self->binary_deserialize = 0;
    self->text_serialize = basset_system_font_serialize;
    self->text_deserialize = basset_system_font_deserialize;
}

void asset_handler_system_font_request_asset(struct asset_handler* self, struct basset* asset, void* listener_instance, PFN_basset_on_result user_callback)
{
    // Create and pass along a context
    // NOTE: The VFS takes a copy of this context, so the lifecycle doesn't matter
    asset_handler_request_context context = {0};
    context.asset = asset;
    context.handler = self;
    context.listener_instance = listener_instance;
    context.user_callback = user_callback;
    vfs_request_asset(self->vfs, asset->name, asset->package_name, false, false, sizeof(asset_handler_request_context), &context, 0, 0, asset_handler_system_font_on_asset_loaded);

    vfs_request_info request_info = {0};
    request_info.package_name = asset->package_name;
    request_info.asset_name = asset->name;
    request_info.is_binary = false;
    request_info.get_source = false;
    request_info.context_size = sizeof(asset_handler_request_context);
    request_info.context = &context;
    request_info.import_params = 0;
    request_info.import_params_size = 0;
    request_info.vfs_callback = asset_handler_system_font_on_asset_loaded;
    request_info.watch_for_hot_reload = false; // Fonts don't need hot reloading
    vfs_request_asset(self->vfs, request_info);
}

void asset_handler_system_font_release_asset(struct asset_handler* self, struct basset* asset)
{
    basset_system_font* typed_asset = (basset_system_font*)asset;
    if (typed_asset->face_count && typed_asset->faces)
    {
        bfree(typed_asset->faces, sizeof(const char*) * typed_asset->face_count, MEMORY_TAG_ARRAY);
        typed_asset->faces = 0;
        typed_asset->face_count = 0;
    }
}

static void asset_handler_system_font_on_asset_loaded(struct vfs_state* vfs, vfs_asset_data asset_data)
{
    /* asset_handler_base_on_asset_loaded(vfs, name, asset_data); */

    // This handler requires context
    BASSERT_MSG(asset_data.context_size && asset_data.context, "asset_handler_base_on_asset_loaded requires valid context");

    // Take a copy of the context first as it gets freed immediately upon return of this function
    asset_handler_request_context context = *((asset_handler_request_context*)asset_data.context);

    // Process -
    // 0. Try to load binary asset first. If this succeeds then this is done
    // 1. If binary load fails, check if there is a source_path defined for the asset. If not, this fails
    // 2. If a source_path exists, check if there is an importer for this asset type/source file type. If not, this fails
    // 3. If an importer exists, run it. If it fails, this fails
    // 4. On success, attempt to load the binary asset again. Return result of that load request. NOTE: not currently doing this

    if (asset_data.result == VFS_REQUEST_RESULT_SUCCESS)
    {
        BTRACE("Asset '%s' load from VFS successful", bname_string_get(asset_data.asset_name));

        // Default to an internal failure
        asset_request_result result = ASSET_REQUEST_RESULT_INTERNAL_FAILURE;

        // Check if the file was loaded as primary or from source
        b8 from_source = (asset_data.flags & VFS_ASSET_FLAG_FROM_SOURCE) ? true : false;
        if (from_source)
        {
            BERROR("There is no import process for system fonts. Secondary asset should not be used");
        }
        else
        {
            BTRACE("Primary asset '%s' loaded", bname_string_get(asset_data.asset_name));
            // From primary file
            // Deserialize directly. This either means that the primary asset already existed or was imported successfully
            if (context.handler->binary_deserialize)
            {
                BTRACE("Using binary deserialization to read primary asset");
                // Binary deserializaton
                if (!context.handler->binary_deserialize(asset_data.size, asset_data.bytes, context.asset))
                {
                    BERROR("Failed to deserialize binary asset data. Unable to fulfull asset request");
                    result = ASSET_REQUEST_RESULT_PARSE_FAILED;
                }
                else
                {
                    result = ASSET_REQUEST_RESULT_SUCCESS;
                }
            }
            else if (context.handler->text_deserialize)
            {
                BTRACE("Using text deserialization to read primary asset");
                // Text deserializaton
                if (!context.handler->text_deserialize(asset_data.text, context.asset))
                {
                    BERROR("Failed to deserialize text asset data. Unable to fulfull asset request");
                    result = ASSET_REQUEST_RESULT_PARSE_FAILED;
                }
                else
                {
                    result = ASSET_REQUEST_RESULT_SUCCESS;
                }
            }
        }
        
        // If successful thus far, attempt to load the font binary
        if (result == ASSET_REQUEST_RESULT_SUCCESS)
        {
            // Load the ttf_asset_name (aka the font binary file)
            vfs_asset_data font_file_data = {0};
            basset_system_font* typed_asset = (basset_system_font*)context.asset;
            // Request the asset synchronously
            vfs_request_info request_info = {0};
            request_info.package_name = context.asset->package_name;
            request_info.asset_name = typed_asset->ttf_asset_name;
            request_info.is_binary = true;
            request_info.get_source = false;
            request_info.context_size = 0;
            request_info.context = 0;
            request_info.import_params = 0;
            request_info.import_params_size = 0;
            request_info.vfs_callback = asset_handler_system_font_on_asset_loaded;
            request_info.watch_for_hot_reload = false; // Fonts don't need hot reloading
            font_file_data = vfs_request_asset_sync(vfs, request_info);
            if (font_file_data.result == VFS_REQUEST_RESULT_SUCCESS)
            {
                // Take a copy of the font binary data
                typed_asset->font_binary_size = font_file_data.size;
                typed_asset->font_binary = ballocate(typed_asset->font_binary_size, MEMORY_TAG_ASSET);
                bcopy_memory(typed_asset->font_binary, font_file_data.bytes, font_file_data.size);
            }
            else
            {
                BERROR("Failed to read system font binary data (package='%s', name='%s'). Asset load failed", bname_string_get(typed_asset->ttf_asset_name), bname_string_get(typed_asset->ttf_asset_package_name));
                result = ASSET_REQUEST_RESULT_VFS_REQUEST_FAILED;
            }

            // Release VFS asset resources
            if (font_file_data.bytes && font_file_data.size)
            {
                bfree((void*)font_file_data.bytes, font_file_data.size, MEMORY_TAG_ASSET);
                font_file_data.bytes = 0;
                font_file_data.size = 0;
            }
        }

        // Send over the result
        context.user_callback(result, context.asset, context.listener_instance);
    }
    else
    {
        // If primary file doesn't exist, try importing the source file instead
        if (asset_data.result == VFS_REQUEST_RESULT_FILE_DOES_NOT_EXIST)
        {
            BERROR("Failed to load primary asset. Operation failed");
            context.user_callback(ASSET_REQUEST_RESULT_VFS_REQUEST_FAILED, context.asset, context.listener_instance);
        }
    }
}
