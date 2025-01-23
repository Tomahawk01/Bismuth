#include "bpackage.h"

#include "containers/darray.h"
#include "debug/bassert.h"
#include "defines.h"
#include "logger.h"
#include "memory/bmemory.h"
#include "parsers/bson_parser.h"
#include "platform/filesystem.h"
#include "platform/platform.h"
#include "platform/vfs.h"
#include "strings/bname.h"
#include "strings/bstring.h"

typedef struct asset_entry
{
    bname name;
    // If loaded from binary, this will be null
    const char* path;

    // Should be populated if the asset was imported
    const char* source_path;

    // If loaded from binary, these define where the asset is in the blob
    u64 offset;
    u64 size;
} asset_entry;

typedef struct bpackage_internal
{
    // darray of all asset entries
    asset_entry* entries;
} bpackage_internal;

b8 bpackage_create_from_manifest(const asset_manifest* manifest, bpackage* out_package)
{
    if (!manifest || !out_package)
    {
        BERROR("bpackage_create_from_manifest requires valid pointers to manifest and out_package");
        return false;
    }

    bzero_memory(out_package, sizeof(bpackage));

    if (manifest->name)
    {
        out_package->name = manifest->name;
    }
    else
    {
        BERROR("Manifest must contain a name");
        return false;
    }

    out_package->is_binary = false;

    out_package->internal_data = ballocate(sizeof(bpackage_internal), MEMORY_TAG_RESOURCE);

    out_package->watch_ids = darray_create(u32);

    // Process manifest
    u32 asset_count = darray_length(manifest->assets);
    for (u32 i = 0; i < asset_count; ++i)
    {
        asset_manifest_asset* asset = &manifest->assets[i];

        asset_entry new_entry = {0};
        new_entry.name = asset->name;
        new_entry.path = string_duplicate(asset->path);
        if (asset->source_path)
            new_entry.source_path = string_duplicate(asset->source_path);
        // NOTE: Size and offset don't get filled out/used with a manifest version of a package
        // Allocate the entry type array if it isn't already
        if (!out_package->internal_data->entries)
            out_package->internal_data->entries = darray_create(asset_entry);

        // Push the asset to it
        darray_push(out_package->internal_data->entries, new_entry);
    }

    return true;
}

b8 bpackage_create_from_binary(u64 size, void* bytes, bpackage* out_package)
{
    if (!size || !bytes || !out_package)
    {
        BERROR("bpackage_create_from_binary requires valid pointers to bytes and out_package, and size must be nonzero");
        return false;
    }

    out_package->is_binary = false;

    // Process manifest

    BERROR("bpackage_create_from_binary not yet supported");
    return false;
}

void bpackage_destroy(bpackage* package)
{
    if (package)
    {
        if (package->internal_data->entries)
        {
            u32 entry_count = darray_length(package->internal_data->entries);
            for (u32 j = 0; j < entry_count; ++j)
            {
                asset_entry* entry = &package->internal_data->entries[j];
                if (entry->path)
                    string_free(entry->path);
            }
            darray_destroy(package->internal_data->entries);
        }

        // Unwatch watched files
        if (package->watch_ids)
        {
            u32 watch_count = darray_length(package->watch_ids);
            for (u32 i = 0; i < watch_count; ++i)
                platform_unwatch_file(package->watch_ids[i]);

            darray_destroy(package->watch_ids);
            package->watch_ids = 0;
        }

        if (package->internal_data)
            bfree(package->internal_data, sizeof(bpackage_internal), MEMORY_TAG_RESOURCE);

        bzero_memory(package, sizeof(bpackage_internal));
    }
}

static asset_entry* asset_entry_get(const bpackage* package, bname name)
{
    // Search the type lookup's entries for the matching name
    u32 entry_count = darray_length(package->internal_data->entries);
    for (u32 j = 0; j < entry_count; ++j)
    {
        asset_entry* entry = &package->internal_data->entries[j];
        if (entry->name == name)
            return entry;
    }

    BTRACE("Package '%s': No entry called '%s' exists", bname_string_get(package->name), bname_string_get(name));
    return 0;
}

static bpackage_result asset_get_data(const bpackage* package, b8 is_binary, bname name, b8 get_source, u64* out_size, const void** out_data)
{
    const char* package_name = bname_string_get(package->name);
    const char* name_str = bname_string_get(name);
    asset_entry* entry = asset_entry_get(package, name);
    if (!entry)
        return get_source ? BPACKAGE_RESULT_SOURCE_GET_FAILURE : BPACKAGE_RESULT_PRIMARY_GET_FAILURE;

    if (package->is_binary)
    {
        BERROR("binary packages not yet supported");
        return BPACKAGE_RESULT_INTERNAL_FAILURE;
    }
    else
    {
        bpackage_result result = BPACKAGE_RESULT_INTERNAL_FAILURE;

        // Validate asset path
        const char* asset_path = get_source ? entry->source_path : entry->path;
        if (!asset_path)
        {
            BERROR("Package '%s': No %s asset path exists for asset '%s'. Load operation failed", package_name, get_source ? "source" : "primary", name_str);
            result = get_source ? BPACKAGE_RESULT_SOURCE_GET_FAILURE : BPACKAGE_RESULT_PRIMARY_GET_FAILURE;
            return result;
        }

        // Validate that the file exists
        if (!filesystem_exists(asset_path))
        {
            BERROR("Package '%s': Invalid %s asset path ('%s') for asset '%s'. Load operation failed", package_name, get_source ? "source" : "primary", asset_path, name_str);
            result = get_source ? BPACKAGE_RESULT_SOURCE_GET_FAILURE : BPACKAGE_RESULT_PRIMARY_GET_FAILURE;
            return result;
        }
        void* data = 0;

        // load the file content from disk
        file_handle f = {0};
        if (!filesystem_open(asset_path, FILE_MODE_READ, is_binary, &f))
        {
            BERROR("Package '%s': Failed to open asset '%s' file at path: '%s'", package_name, name_str, asset_path);
            result = get_source ? BPACKAGE_RESULT_SOURCE_GET_FAILURE : BPACKAGE_RESULT_PRIMARY_GET_FAILURE;
            goto get_data_cleanup;
        }

        // Get the file size
        u64 original_file_size = 0;
        if (!filesystem_size(&f, &original_file_size))
        {
            BERROR("Package '%s': Failed to get size for asset '%s' file at path: '%s'", package_name, name_str, asset_path);
            result = get_source ? BPACKAGE_RESULT_SOURCE_GET_FAILURE : BPACKAGE_RESULT_PRIMARY_GET_FAILURE;
            goto get_data_cleanup;
        }

        // Account for the null terminator for text files
        u64 actual_file_size = original_file_size;
        if (!is_binary)
            actual_file_size++;

        data = ballocate(actual_file_size, MEMORY_TAG_ASSET);

        u64 read_size = 0;
        if (is_binary)
        {
            // Load as binary
            if (!filesystem_read_all_bytes(&f, data, &read_size))
            {
                BERROR("Package '%s': Failed to read asset '%s' as binary, at file at path: '%s'", package_name, name_str, asset_path);
                goto get_data_cleanup;
            }
        }
        else
        {
            // Load as text
            if (!filesystem_read_all_text(&f, data, &read_size))
            {
                BERROR("Package '%s': Failed to read asset '%s' as text, at file at path: '%s'", package_name, name_str, asset_path);
                goto get_data_cleanup;
            }
        }

        // Sanity check to make sure the bounds haven't been breached
        BASSERT_MSG(read_size <= actual_file_size, "File read exceeded bounds of data allocation based on file size");
        
        // This means that data is bigger than it needs to be, and that a smaller block of memory can be used
        if (read_size < original_file_size)
        {
            BTRACE("Package '%s': asset '%s', file at path: '%s' - Read size/file size mismatch (%llu, %llu)", package_name, name_str, asset_path, read_size, original_file_size);
            void* temp = ballocate(read_size + (is_binary ? 1 : 0), MEMORY_TAG_ASSET);
            bcopy_memory(temp, data, read_size);
            bfree(data, actual_file_size, MEMORY_TAG_ASSET);
            data = temp;
            actual_file_size = read_size;
            // Account for the null terminator for text files
            if (!is_binary)
            {
                actual_file_size++;
                ((char*)data)[actual_file_size - 1] = 0;
            }
        }

        // Set the output
        *out_data = data;
        *out_size = actual_file_size;

        // Success!
        result = BPACKAGE_RESULT_SUCCESS;

    get_data_cleanup:
        filesystem_close(&f);

        if (result != BPACKAGE_RESULT_SUCCESS)
        {
            if (data)
                bfree(data, original_file_size, MEMORY_TAG_ASSET);
        }
        else
        {
            // BERROR("Package '%s' does not contain asset '%s'", package_name, name_str);
        }
        return result;
    }
}

bpackage_result bpackage_asset_bytes_get(const bpackage* package, bname name, b8 get_source, u64* out_size, const void** out_data)
{
    if (!package || !name || !out_size || !out_data)
    {
        BERROR("bpackage_asset_bytes_get requires valid pointers to package, name, out_size, and out_data");
        return 0;
    }

    return asset_get_data(package, true, name, get_source, out_size, out_data);
}

bpackage_result bpackage_asset_text_get(const bpackage* package, bname name, b8 get_source, u64* out_size, const char** out_text)
{
    if (!package || !name || !out_size || !out_text)
    {
        BERROR("bpackage_asset_text_get requires valid pointers to package, name, out_size, and out_text");
        return 0;
    }

    return asset_get_data(package, false, name, get_source, out_size, (const void**)out_text);
}

b8 bpackage_asset_watch(bpackage* package, const char* asset_path, u32* out_watch_id)
{
    if (!platform_watch_file(asset_path, out_watch_id))
    {
        BWARN("Failed to watch package '%s' asset file '%s'", bname_string_get(package->name), asset_path);
        return false;
    }
    // Register the watch
    darray_push(package->watch_ids, *out_watch_id);
    return true;
}

void bpackage_asset_unwatch(bpackage* package, u32 watch_id)
{
    if (package && package->watch_ids && watch_id != INVALID_ID)
    {
        if (!platform_unwatch_file(watch_id))
            BWARN("Failed to unwatch file watch id %u", watch_id);

        // Remove from the watch list
        u32 watch_count = darray_length(package->watch_ids);
        for (u32 i = 0; i < watch_count; ++i)
        {
            if (package->watch_ids[i] == watch_id)
            {
                u32 out_val = 0;
                darray_pop_at(package->watch_ids, i, &out_val);
                return;
            }
        }
    }
}

const char* bpackage_path_for_asset(const bpackage* package, bname name)
{
    u32 entry_count = darray_length(package->internal_data->entries);
    for (u32 j = 0; j < entry_count; ++j)
    {
        asset_entry* entry = &package->internal_data->entries[j];
        if (entry->name == name)
        {
            if (package->is_binary)
            {
                BERROR("binary packages not yet supported");
                return 0;
            }
            else
            {
                return string_duplicate(entry->path);
            }
        }
    }
    return 0;
}

const char* bpackage_source_path_for_asset(const bpackage* package, bname name)
{
    u32 entry_count = darray_length(package->internal_data->entries);
    for (u32 j = 0; j < entry_count; ++j)
    {
        asset_entry* entry = &package->internal_data->entries[j];
        if (entry->name == name)
        {
            if (package->is_binary)
            {
                BERROR("binary packages not yet supported");
                return 0;
            }
            else
            {
                return string_duplicate(entry->source_path);
            }
        }
    }
    return 0;
}

// Writes file to disk for packages using the asset manifest, not binary packages
static b8 bpackage_asset_write_file_internal(bpackage* package, bname name, u64 size, const void* bytes, b8 is_binary)
{
    file_handle f = {0};
    // FIXME: Brute-force lookup, add a hash table or something better...
    u32 entry_count = darray_length(package->internal_data->entries);
    for (u32 i = 0; i < entry_count; ++i)
    {
        asset_entry* entry = &package->internal_data->entries[i];
        if (entry->name == name)
        {
            // Found a match
            if (!filesystem_open(entry->path, FILE_MODE_WRITE, is_binary, &f))
            {
                BERROR("Unable to open asset file for writing: '%s'", entry->path);
                return false;
            }

            u64 bytes_written = 0;
            if (!filesystem_write(&f, size, bytes, &bytes_written))
            {
                BERROR("Unable to write to asset file: '%s'", entry->path);
                filesystem_close(&f);
                return false;
            }

            if (bytes_written != size)
                BWARN("Asset bytes written/size mismatch: %llu/%llu", bytes_written, size);

            filesystem_close(&f);

            return true;
        }
    }

    // New asset file, write out
    BERROR("bpackage_asset_bytes_write attempted to write to an asset that is not in the manifest");
    return false;
}

b8 bpackage_asset_bytes_write(bpackage* package, bname name, u64 size, const void* bytes)
{
    if (!package || !name || !size || !bytes)
    {
        BERROR("bpackage_asset_bytes_write requires valid pointers to package, name and bytes, and a nonzero size");
        return false;
    }

    if (package->is_binary)
    {
        // FIXME: do the thing
        BASSERT_MSG(false, "not yet supported");
        return false;
    }

    if (!bpackage_asset_write_file_internal(package, name, size, bytes, true))
    {
        BERROR("Failed to write asset");
        return false;
    }

    return true;
}

b8 bpackage_asset_text_write(bpackage* package, bname name, u64 size, const char* text)
{
    if (!package || !name || !size || !text)
    {
        BERROR("bpackage_asset_text_write requires valid pointers to package, name and bytes, and a nonzero size");
        return false;
    }

    if (package->is_binary)
    {
        // FIXME: do the thing
        BASSERT_MSG(false, "not yet supported");
        return false;
    }

    if (!bpackage_asset_write_file_internal(package, name, size, (void*)text, false))
    {
        BERROR("Failed to write asset");
        return false;
    }

    return true;
}

b8 bpackage_parse_manifest_file_content(const char* path, asset_manifest* out_manifest)
{
    if (!path || !out_manifest)
    {
        BERROR("bpackage_parse_manifest_file_content requires valid pointers to path and out_manifest");
        return false;
    }

    b8 success = false;
    const char* file_content = filesystem_read_entire_text_file(path);
    if (!file_content)
    {
        BERROR("Failed to load asset manifest '%s'", path);
        return false;
    }

    // Parse manifest
    bson_tree tree;
    if (!bson_tree_from_string(file_content, &tree))
    {
        BERROR("Failed to parse asset manifest file '%s'. See logs for details", path);
        return false;
    }

    // Extract properties from file
    if (!bson_object_property_value_get_string_as_bname(&tree.root, "package_name", &out_manifest->name))
    {
        BERROR("Asset manifest format - 'package_name' is required but not found");
        goto bpackage_parse_cleanup;
    }

    // Take a copy of the file path
    out_manifest->file_path = string_duplicate(path);

    // Take a copy of the directory to the file path
    char base_path[512];
    bzero_memory(base_path, sizeof(char) * 512);
    string_directory_from_path(base_path, path);
    string_trim(base_path);
    out_manifest->path = string_duplicate(base_path);

    // Process references
    bson_array references = {0};
    b8 contains_references = bson_object_property_value_get_array(&tree.root, "references", &references);
    if (contains_references)
    {
        u32 reference_array_count = 0;
        if (!bson_array_element_count_get(&references, &reference_array_count))
        {
            BWARN("Failed to get array count for references. Skipping...");
        }
        else
        {
            // Stand up a darray for references
            out_manifest->references = darray_create(asset_manifest_reference);

            for (u32 i = 0; i < reference_array_count; ++i)
            {
                bson_object ref_obj = {0};
                if (!bson_array_element_value_get_object(&references, i, &ref_obj))
                {
                    BWARN("Failed to get object at array index %u. Skipping..", i);
                    continue;
                }

                asset_manifest_reference ref = {0};
                // Reference name
                const char* ref_name;
                if (!bson_object_property_value_get_string(&ref_obj, "name", &ref_name))
                {
                    BWARN("Failed to get reference name at array index %u. Skipping...", i);
                    string_free(ref_name);
                    continue;
                }
                ref.name = bname_create(ref_name);
                string_free(ref_name);

                // Reference path
                if (!bson_object_property_value_get_string(&ref_obj, "path", &ref.path))
                {
                    BWARN("Failed to get reference path at array index %u. Skipping...", i);
                    continue;
                }

                // Add to references
                darray_push(out_manifest->references, ref);
            }
        }
    }

    // Process assets
    bson_array assets = {0};
    b8 contains_assets = bson_object_property_value_get_array(&tree.root, "assets", &assets);
    if (contains_assets)
    {
        u32 asset_array_count = 0;
        if (!bson_array_element_count_get(&assets, &asset_array_count))
        {
            BWARN("Failed to get array count for assets. Skipping.");
        }
        else
        {
            // Stand up a darray for assets
            out_manifest->assets = darray_create(asset_manifest_asset);

            for (u32 i = 0; i < asset_array_count; ++i)
            {
                bson_object asset_obj = {0};
                if (!bson_array_element_value_get_object(&assets, i, &asset_obj))
                {
                    BWARN("Failed to get object at array index %u. Skipping...", i);
                    continue;
                }

                asset_manifest_asset asset = {0};
                // Asset name.
                const char* asset_name = 0;
                if (!bson_object_property_value_get_string(&asset_obj, "name", &asset_name))
                {
                    BWARN("Failed to get asset name at array index %u. Skipping...", i);
                    string_free(asset_name);
                    continue;
                }
                asset.name = bname_create(asset_name);

                // Path
                const char* asset_path_temp = 0;
                if (!bson_object_property_value_get_string(&asset_obj, "path", &asset_path_temp))
                {
                    BWARN("Failed to get asset path at array index %u. Skipping...", i);
                    continue;
                }
                // Full path of the asset
                asset.path = string_format("%s/%s", out_manifest->path, asset_path_temp);
                string_free(asset_path_temp);

                // Source Path - optional
                const char* asset_source_path_temp = 0;
                if (bson_object_property_value_get_string(&asset_obj, "source_path", &asset_source_path_temp))
                {
                    // Full source path of the asset
                    asset.source_path = string_format("%s/%s", out_manifest->path, asset_source_path_temp);
                    string_free(asset_source_path_temp);
                }

                // Add to assets
                darray_push(out_manifest->assets, asset);
            }
        }
    }

    success = true;
bpackage_parse_cleanup:
    if (file_content)
        string_free(file_content);
    bson_tree_cleanup(&tree);
    if (!success)
    {
        // Clean up out_manifest
        if (out_manifest->references)
        {
            u32 ref_count = darray_length(out_manifest->references);
            for (u32 i = 0; i < ref_count; ++i)
            {
                if (out_manifest->references[i].path)
                    string_free(out_manifest->references[i].path);
            }
            darray_destroy(out_manifest->references);
            out_manifest->references = 0;
        }
    }
    return success;
}

void bpackage_manifest_destroy(asset_manifest* manifest)
{
    if (manifest)
    {
        if (manifest->path)
            string_free(manifest->path);

        if (manifest->references)
        {
            u32 ref_count = darray_length(manifest->references);
            for (u32 i = 0; i < ref_count; ++i)
            {
                asset_manifest_reference* ref = &manifest->references[i];
                if (ref->path)
                    string_free(ref->path);
            }
            darray_destroy(manifest->references);
        }

        if (manifest->assets)
        {
            u32 ref_count = darray_length(manifest->assets);
            for (u32 i = 0; i < ref_count; ++i)
            {
                asset_manifest_asset* asset = &manifest->assets[i];
                if (asset->path)
                    string_free(asset->path);
                if (asset->source_path)
                    string_free(asset->source_path);
            }
            darray_destroy(manifest->assets);
        }
        bzero_memory(manifest, sizeof(asset_manifest));
    }
}
