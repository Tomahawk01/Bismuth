#include "vfs.h"

#include "assets/basset_types.h"
#include "containers/darray.h"
#include "debug/bassert.h"
#include "logger.h"
#include "memory/bmemory.h"
#include "platform/filesystem.h"
#include "platform/bpackage.h"
#include "strings/bname.h"
#include "strings/bstring.h"

static b8 process_manifest_refs(vfs_state* state, const asset_manifest* manifest);

b8 vfs_initialize(u64* memory_requirement, vfs_state* state, const vfs_config* config)
{
    if (!memory_requirement)
    {
        BERROR("vfs_initialize requires a valid pointer to memory_requirement");
        return false;
    }

    *memory_requirement = sizeof(vfs_state);
    if (!state)
    {
        return true;
    }
    else if (!config)
    {
        BERROR("vfs_initialize requires a valid pointer to config");
        return false;
    }

    state->packages = darray_create(bpackage);

    // TODO: For release builds, look at binary file
    // FIXME: hardcoded rubbish. Add to app config, pass to config and read in here
    const char* file_path = "../testbed.bapp/asset_manifest.bson";
    asset_manifest manifest = {0};
    if (!bpackage_parse_manifest_file_content(file_path, &manifest))
    {
        BERROR("Failed to parse primary asset manifest. See logs for details");
        return false;
    }

    bpackage primary_package = {0};
    if (!bpackage_create_from_manifest(&manifest, &primary_package))
    {
        BERROR("Failed to create package from primary asset manifest. See logs for details");
        return false;
    }

    darray_push(state->packages, primary_package);

    // Examine primary package references and load them as needed
    if (!process_manifest_refs(state, &manifest))
    {
        BERROR("Failed to process manifest reference. See logs for details");
        bpackage_manifest_destroy(&manifest);
        return false;
    }

    bpackage_manifest_destroy(&manifest);

    return true;
}

void vfs_shutdown(vfs_state* state)
{
    if (state)
    {
        if (state->packages)
        {
            u32 package_count = darray_length(state->packages);
            for (u32 i = 0; i < package_count; ++i)
                bpackage_destroy(&state->packages[i]);
            darray_destroy(state->packages);
            state->packages = 0;
        }
    }
}

void vfs_request_asset(vfs_state* state, bname package_name, bname asset_name, b8 is_binary, b8 get_source, u32 context_size, const void* context, PFN_on_asset_loaded_callback callback)
{
    if (!state || !callback)
        BERROR("vfs_request_asset requires state and callback to be provided");

    // TODO: Jobify this call.
    vfs_asset_data data = {0};
    vfs_request_asset_sync(state, package_name, asset_name, is_binary, get_source, context_size, context, &data);

    // TODO: This should be the job result
    // Issue the callback with the data
    callback(state, data);

    // Cleanup the context
    if (data.context && data.context_size)
    {
        bfree(data.context, data.context_size, MEMORY_TAG_PLATFORM);
        data.context = 0;
        data.context_size = 0;
    }
}

void vfs_request_asset_sync(vfs_state* state, bname package_name, bname asset_name, b8 is_binary, b8 get_source, u32 context_size, const void* context, vfs_asset_data* out_data)
{
    if (!out_data)
    {
        BERROR("vfs_request_asset_sync requires a valid pointer to out_data. Nothing can or will be done");
        return;
    }

    if (!state)
    {
        BERROR("vfs_request_asset_sync requires state to be provided");
        out_data->result = VFS_REQUEST_RESULT_INTERNAL_FAILURE;
        return;
    }

    bzero_memory(out_data, sizeof(vfs_asset_data));

    out_data->asset_name = asset_name;
    out_data->package_name = package_name;

    // Take a copy of the context if provided. This will need to be freed by the caller
    if (context_size)
    {
        BASSERT_MSG(context, "Called vfs_request_asset with a context_size, but not a context. Check yourself before you wreck yourself");
        out_data->context_size = context_size;
        out_data->context = ballocate(context_size, MEMORY_TAG_PLATFORM);
        bcopy_memory(out_data->context, context, out_data->context_size);
    }
    else
    {
        out_data->context_size = 0;
        out_data->context = 0;
    }

    const char* package_name_str = bname_string_get(package_name);
    const char* asset_name_str = bname_string_get(asset_name);
    BDEBUG("Loading asset '%s' from package '%s'...", asset_name_str, package_name_str);

    u32 package_count = darray_length(state->packages);
    for (u32 i = 0; i < package_count; ++i)
    {
        bpackage* package = &state->packages[i];

        if (package->name == package_name)
        {
            bzero_memory(out_data, sizeof(vfs_asset_data));

            // Determine if the asset type is text
            bpackage_result result = BPACKAGE_RESULT_INTERNAL_FAILURE;
            if (is_binary)
            {
                result = bpackage_asset_bytes_get(package, asset_name, get_source, &out_data->size, &out_data->bytes);
                out_data->flags |= VFS_ASSET_FLAG_BINARY_BIT;
            }
            else
            {
                result = bpackage_asset_text_get(package, asset_name, get_source, &out_data->size, &out_data->text);
            }

            // Indicate this was loaded from source, if appropriate
            if (get_source)
                out_data->flags |= VFS_ASSET_FLAG_FROM_SOURCE;

            // Translate the result to VFS layer and send on up
            if (result != BPACKAGE_RESULT_SUCCESS)
            {
                BERROR("Failed to load binary asset. See logs for details");
                switch (result)
                {
                case BPACKAGE_RESULT_PRIMARY_GET_FAILURE:
                    out_data->result = VFS_REQUEST_RESULT_FILE_DOES_NOT_EXIST;
                    break;
                case BPACKAGE_RESULT_SOURCE_GET_FAILURE:
                    out_data->result = VFS_REQUEST_RESULT_SOURCE_FILE_DOES_NOT_EXIST;
                    break;
                default:
                case BPACKAGE_RESULT_INTERNAL_FAILURE:
                    out_data->result = VFS_REQUEST_RESULT_INTERNAL_FAILURE;
                    break;
                }
            }
            else
            {
                out_data->result = VFS_REQUEST_RESULT_SUCCESS;
                // Include a copy of the asset path
                if (get_source)
                    out_data->path = bpackage_source_string_for_asset(package, asset_name);
                else
                    out_data->path = bpackage_path_for_asset(package, asset_name);
            }

            return;
        }
    }

    BERROR("No package named '%s' exists. Nothing was done", package_name_str);
    out_data->result = VFS_REQUEST_RESULT_PACKAGE_DOES_NOT_EXIST;
    return;
}

const char* vfs_path_for_asset(vfs_state* state, bname package_name, bname asset_name)
{
    u32 package_count = darray_length(state->packages);
    for (u32 i = 0; i < package_count; ++i)
    {
        bpackage* package = &state->packages[i];

        if (package->name == package_name)
            return bpackage_path_for_asset(package, asset_name);
    }

    return 0;
}

const char* vfs_source_path_for_asset(vfs_state* state, bname package_name, bname asset_name)
{
    u32 package_count = darray_length(state->packages);
    for (u32 i = 0; i < package_count; ++i)
    {
        bpackage* package = &state->packages[i];

        if (package->name == package_name)
            return bpackage_source_string_for_asset(package, asset_name);
    }

    return 0;
}

void vfs_request_direct_from_disk(vfs_state* state, const char* path, b8 is_binary, u32 context_size, const void* context, PFN_on_asset_loaded_callback callback)
{
    if (!state || !path || !callback)
        BERROR("vfs_request_direct_from_disk requires state, path and callback to be provided");

    // TODO: Jobify this call
    vfs_asset_data data = {0};
    vfs_request_direct_from_disk_sync(state, path, is_binary, context_size, context, &data);

    // TODO: This should be the job result
    // Issue the callback with the data
    callback(state, data);

    // Cleanup the context
    if (data.context && data.context_size)
    {
        bfree(data.context, data.context_size, MEMORY_TAG_PLATFORM);
        data.context = 0;
        data.context_size = 0;
    }
}

void vfs_request_direct_from_disk_sync(vfs_state* state, const char* path, b8 is_binary, u32 context_size, const void* context, vfs_asset_data* out_data)
{
    if (!out_data)
    {
        BERROR("vfs_request_direct_from_disk_sync requires a valid pointer to out_data. Nothing can or will be done");
        return;
    }
    if (!state || !path)
    {
        BERROR("VFS request direct from disk requires valid pointers to state, path and out_data");
        return;
    }

    bzero_memory(out_data, sizeof(vfs_asset_data));

    char filename[512] = {0};
    string_filename_no_extension_from_path(filename, path);
    out_data->asset_name = bname_create(filename);
    out_data->package_name = 0;
    out_data->path = string_duplicate(path);

    // Take a copy of the context if provided. This will need to be freed by the caller
    if (context_size)
    {
        BASSERT_MSG(context, "Called vfs_request_asset with a context_size, but not a context. Check yourself before you wreck yourself");
        out_data->context_size = context_size;
        out_data->context = ballocate(context_size, MEMORY_TAG_PLATFORM);
        bcopy_memory(out_data->context, context, out_data->context_size);
    }
    else
    {
        out_data->context_size = 0;
        out_data->context = 0;
    }

    if (!filesystem_exists(path))
    {
        BERROR("vfs_request_direct_from_disk_sync: File does not exist: '%s'", path);
        out_data->result = VFS_REQUEST_RESULT_FILE_DOES_NOT_EXIST;
        return;
    }

    if (is_binary)
    {
        out_data->bytes = filesystem_read_entire_binary_file(path, &out_data->size);
        if (!out_data->bytes)
        {
            out_data->size = 0;
            BERROR("vfs_request_direct_from_disk_sync: Error reading from file: '%s'", path);
            out_data->result = VFS_REQUEST_RESULT_READ_ERROR;
            return;
        }
        out_data->flags |= VFS_ASSET_FLAG_BINARY_BIT;
    }
    else
    {
        out_data->text = filesystem_read_entire_text_file(path);
        if (!out_data->text)
        {
            out_data->size = 0;
            BERROR("vfs_request_direct_from_disk_sync: Error reading from file: '%s'", path);
            out_data->result = VFS_REQUEST_RESULT_READ_ERROR;
            return;
        }
        out_data->size = sizeof(char) * (string_length(out_data->text) + 1);
    }

    // Take a copy of the context if provided. This will be freed immediately after the callback is made below.
    // This means the context should be immediately consumed by the callback before any async work is done
    if (context_size)
    {
        BASSERT_MSG(context, "Called vfs_request_asset with a context_size, but not a context. Check yourself before you wreck yourself");
        out_data->context_size = context_size;
        out_data->context = ballocate(context_size, MEMORY_TAG_PLATFORM);
        bcopy_memory(out_data->context, context, out_data->context_size);
    }
    else
    {
        out_data->context_size = 0;
        out_data->context = 0;
    }

    out_data->result = VFS_REQUEST_RESULT_SUCCESS;
}

b8 vfs_asset_write(vfs_state* state, const basset* asset, b8 is_binary, u64 size, const void* data)
{
    u32 package_count = darray_length(state->packages);
    if (asset->package_name == 0)
    {
        BERROR("Unable to write asset because it does not have a package name: '%s'", bname_string_get(asset->name));
        return false;
    }
    for (u32 i = 0; i < package_count; ++i)
    {
        bpackage* package = &state->packages[i];
        if (package->name == asset->package_name)
        {
            if (is_binary)
                return bpackage_asset_bytes_write(package, asset->name, size, data);
            else
                return bpackage_asset_text_write(package, asset->name, size, data);
        }
    }

    BERROR("vfs_asset_write: Unable to find package named '%s'", bname_string_get(asset->package_name));
    return false;
}

static b8 process_manifest_refs(vfs_state* state, const asset_manifest* manifest)
{
    b8 success = true;
    if (manifest->references)
    {
        u32 ref_count = darray_length(manifest->references);
        for (u32 i = 0; i < ref_count; ++i)
        {
            asset_manifest_reference* ref = &manifest->references[i];

            // Don't load the same package more than once
            b8 exists = false;
            u32 package_count = darray_length(state->packages);
            for (u32 j = 0; j < package_count; ++j)
            {
                if (state->packages[j].name == ref->name)
                {
                    BTRACE("Package '%s' already loaded, skipping...", bname_string_get(ref->name));
                    exists = true;
                    break;
                }
            }
            if (exists)
                continue;

            asset_manifest new_manifest = {0};
            if (!bpackage_parse_manifest_file_content(ref->path, &new_manifest))
            {
                BERROR("Failed to parse asset manifest. See logs for details");
                return false;
            }

            bpackage package = {0};
            if (!bpackage_create_from_manifest(&new_manifest, &package))
            {
                BERROR("Failed to create package from asset manifest. See logs for details");
                return false;
            }

            darray_push(state->packages, package);

            // Process references
            if (!process_manifest_refs(state, &new_manifest))
            {
                BERROR("Failed to process manifest reference. See logs for details");
                bpackage_manifest_destroy(&new_manifest);
                success = false;
                break;
            }
            else
            {
                bpackage_manifest_destroy(&new_manifest);
            }
        }
    }

    return success;
}
