#include "text_loader.h"
#include "core/logger.h"
#include "core/bmemory.h"
#include "core/bstring.h"
#include "resources/resource_types.h"
#include "systems/resource_system.h"
#include "math/bmath.h"
#include "platform/filesystem.h"

b8 text_loader_load(struct resource_loader* self, const char* name, resource* out_resource)
{
    if (!self || !name || !out_resource)
        return false;

    char* format_str = "%s/%s/%s%s";
    char full_file_path[512];
    string_format(full_file_path, format_str, resource_system_base_path(), self->type_path, name, "");

    // TODO: Use allocator here
    out_resource->full_path = string_duplicate(full_file_path);

    file_handle f;
    if (!filesystem_open(full_file_path, FILE_MODE_READ, false, &f))
    {
        BERROR("text_loader_load - unable to open file for text reading: '%s'", full_file_path);
        return false;
    }

    u64 file_size = 0;
    if (!filesystem_size(&f, &file_size)) {
        BERROR("Unable to text read file: %s", full_file_path);
        filesystem_close(&f);
        return false;
    }

    // TODO: Use allocator here
    char* resource_data = ballocate(sizeof(char) * file_size, MEMORY_TAG_ARRAY);
    u64 read_size = 0;
    if (!filesystem_read_all_text(&f, resource_data, &read_size))
    {
        BERROR("Unable to text read file: %s", full_file_path);
        filesystem_close(&f);
        return false;
    }

    filesystem_close(&f);

    out_resource->data = resource_data;
    out_resource->data_size = read_size;
    out_resource->name = name;

    return true;
}

void text_loader_unload(struct resource_loader* self, resource* resource)
{
    if (!self || !resource)
    {
        BWARN("text_loader_unload called with nullptr for self or resource");
        return;
    }

    u32 path_length = string_length(resource->full_path);
    if (path_length)
        bfree(resource->full_path, sizeof(char) * path_length + 1, MEMORY_TAG_STRING);

    if (resource->data)
    {
        bfree(resource->data, resource->data_size, MEMORY_TAG_ARRAY);
        resource->data = 0;
        resource->data_size = 0;
        resource->loader_id = INVALID_ID;
    }
}

resource_loader text_resource_loader_create()
{
    resource_loader loader;
    loader.type = RESOURCE_TYPE_TEXT;
    loader.custom_type = 0;
    loader.load = text_loader_load;
    loader.unload = text_loader_unload;
    loader.type_path = "";

    return loader;
}
