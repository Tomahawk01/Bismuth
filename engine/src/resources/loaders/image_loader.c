#include "image_loader.h"

#include "core/logger.h"
#include "core/bmemory.h"
#include "core/bstring.h"
#include "math/bmath.h"
#include "platform/filesystem.h"
#include "resources/resource_types.h"
#include "systems/resource_system.h"
#include "loader_utils.h"

#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_STDIO // use own filesystem
#include "vendor/stb_image.h"

static b8 image_loader_load(struct resource_loader* self, const char* name, void* params, resource* out_resource)
{
    if (!self || !name || !out_resource)
        return false;

    image_resource_params* typed_params = (image_resource_params*)params;

    char* format_str = "%s/%s/%s%s";
    const i32 required_channel_count = 4;
    stbi_set_flip_vertically_on_load_thread(typed_params->flip_y);
    char full_file_path[512];

    // Try different extensions
    #define IMAGE_EXTENSION_COUND 4
    b8 found = false;
    char* extensions[IMAGE_EXTENSION_COUND] = {".tga", ".png", ".jpg", ".bmp"};
    for (u32 i = 0; i < IMAGE_EXTENSION_COUND; ++i)
    {
        string_format(full_file_path, format_str, resource_system_base_path(), self->type_path, name, extensions[i]);
        if (filesystem_exists(full_file_path))
        {
            found = true;
            break;
        }
    }

    // Take copy of resource full path and name
    out_resource->full_path = string_duplicate(full_file_path);
    out_resource->name = name;

    if (!found)
    {
        BERROR("Image resource loader failed to find file '%s' or file extension is not supported", full_file_path);
        return false;
    }

    file_handle f;
    if (!filesystem_open(full_file_path, FILE_MODE_READ, true, &f))
    {
        BERROR("Unable to read file: %s", full_file_path);
        filesystem_close(&f);
        return false;
    }

    u64 file_size = 0;
    if (!filesystem_size(&f, &file_size))
    {
        BERROR("Unable to get size of file: %s", full_file_path);
        filesystem_close(&f);
        return false;
    }

    i32 width;
    i32 height;
    i32 channel_count;
    // Final result of all operations
    b8 final_result = false;

    u8* raw_data = ballocate(file_size, MEMORY_TAG_TEXTURE);
    if (!raw_data)
    {
        BERROR("Unable to read file '%s'", full_file_path);
        filesystem_close(&f);
        goto image_loader_load_return;
    }

    u64 bytes_read = 0;
    b8 read_result = filesystem_read_all_bytes(&f, raw_data, &bytes_read);
    filesystem_close(&f);

    if (!read_result)
    {
        BERROR("Unable to read file: '%s'", full_file_path);
        goto image_loader_load_return;
    }

    if (bytes_read != file_size)
    {
        BERROR("File size if %llu does not match expected: %llu", bytes_read, file_size);
        goto image_loader_load_return;
    }

    u8* data = stbi_load_from_memory(raw_data, file_size, &width, &height, &channel_count, required_channel_count);
    if (!data)
    {
        BERROR("Image resource loader failed to load file '%s'", full_file_path);
        goto image_loader_load_return;
    }

    image_resource_data* resource_data = ballocate(sizeof(image_resource_data), MEMORY_TAG_TEXTURE);
    resource_data->pixels = data;
    resource_data->width = width;
    resource_data->height = height;
    resource_data->channel_count = required_channel_count;
    resource_data->mip_levels = (u32)(bfloor(blog2(BMAX(width, height))) + 1);

    out_resource->data = resource_data;
    out_resource->data_size = sizeof(image_resource_data);
    final_result = true;

    // No matter what result, clean and return
image_loader_load_return:
    if (raw_data)
        bfree(raw_data, file_size, MEMORY_TAG_TEXTURE);
    return final_result;
}

static void image_loader_unload(struct resource_loader* self, resource* resource)
{
    stbi_image_free(((image_resource_data*)resource->data)->pixels);
    if (!resource_unload(self, resource, MEMORY_TAG_TEXTURE))
        BWARN("image_loader_unload called with nullptr for self or resource");
}

resource_loader image_resource_loader_create(void)
{
    resource_loader loader;
    loader.type = RESOURCE_TYPE_IMAGE;
    loader.custom_type = 0;
    loader.load = image_loader_load;
    loader.unload = image_loader_unload;
    loader.type_path = "textures";

    return loader;
}
