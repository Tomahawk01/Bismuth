#include "system_font_loader.h"

#include "containers/darray.h"
#include "loader_utils.h"
#include "logger.h"
#include "math/bmath.h"
#include "memory/bmemory.h"
#include "resources/font_types.h"
#include "resources/resource_types.h"
#include "strings/bstring.h"
#include "systems/resource_system.h"
#include "platform/filesystem.h"

typedef enum system_font_file_type
{
    SYSTEM_FONT_FILE_TYPE_NOT_FOUND,
    SYSTEM_FONT_FILE_TYPE_BSF,
    SYSTEM_FONT_FILE_TYPE_FONTCONFIG
} system_font_file_type;

typedef struct supported_system_font_filetype
{
    char* extension;
    system_font_file_type type;
    b8 is_binary;
} supported_system_font_filetype;

static b8 import_fontconfig_file(file_handle* f, const char* type_path, const char* out_bsf_filename, system_font_resource_data* out_resource);
static b8 read_bsf_file(file_handle* file, system_font_resource_data* data);
static b8 write_bsf_file(const char* out_bsf_filename, system_font_resource_data* resource);

static b8 system_font_loader_load(struct resource_loader* self, const char* name, void* params, resource* out_resource)
{
    if (!self || !name || !out_resource)
        return false;

    char* format_str = "%s/%s/%s%s";
    file_handle f;
    // Supported extensions. These are in order of priority when looked up
#define SUPPORTED_FILETYPE_COUNT 2
    supported_system_font_filetype supported_filetypes[SUPPORTED_FILETYPE_COUNT];
    supported_filetypes[0] = (supported_system_font_filetype){".bsf", SYSTEM_FONT_FILE_TYPE_BSF, true};
    supported_filetypes[1] = (supported_system_font_filetype){".fontcfg", SYSTEM_FONT_FILE_TYPE_FONTCONFIG, false};

    char full_file_path[512];
    system_font_file_type type = SYSTEM_FONT_FILE_TYPE_NOT_FOUND;
    // Try each supported extension
    for (u32 i = 0; i < SUPPORTED_FILETYPE_COUNT; ++i)
    {
        string_format_unsafe(full_file_path, format_str, resource_system_base_path(), self->type_path, name, supported_filetypes[i].extension);
        // If file exists, open it and stop looking
        if (filesystem_exists(full_file_path))
        {
            if (filesystem_open(full_file_path, FILE_MODE_READ, supported_filetypes[i].is_binary, &f))
            {
                type = supported_filetypes[i].type;
                break;
            }
        }
    }

    if (type == SYSTEM_FONT_FILE_TYPE_NOT_FOUND)
    {
        BERROR("Unable to find system font of supported type called '%s'", name);
        return false;
    }

    out_resource->full_path = string_duplicate(full_file_path);

    system_font_resource_data resource_data;

    b8 result = false;
    switch (type)
    {
        case SYSTEM_FONT_FILE_TYPE_FONTCONFIG:
        {
            // Generate bsf filename
            char bsf_file_name[512];
            string_format_unsafe(bsf_file_name, "%s/%s/%s%s", resource_system_base_path(), self->type_path, name, ".bsf");
            result = import_fontconfig_file(&f, self->type_path, bsf_file_name, &resource_data);
            break;
        }
        case SYSTEM_FONT_FILE_TYPE_BSF:
            result = read_bsf_file(&f, &resource_data);
            break;
        case SYSTEM_FONT_FILE_TYPE_NOT_FOUND:
            BERROR("Unable to find system font of supported type called '%s'", name);
            result = false;
            break;
    }

    filesystem_close(&f);

    if (!result)
    {
        BERROR("Failed to process system font file '%s'", full_file_path);
        out_resource->data = 0;
        out_resource->data_size = 0;
        return false;
    }

    out_resource->data = ballocate(sizeof(system_font_resource_data), MEMORY_TAG_RESOURCE);
    bcopy_memory(out_resource->data, &resource_data, sizeof(system_font_resource_data));
    out_resource->data_size = sizeof(system_font_resource_data);

    return true;
}

static b8 import_fontconfig_file(file_handle* f, const char* type_path, const char* out_bsf_filename, system_font_resource_data* out_resource)
{
    out_resource->fonts = darray_create(system_font_face);
    out_resource->binary_size = 0;
    out_resource->font_binary = 0;

    // Read each line of the file
    char line_buf[512] = "";
    char* p = &line_buf[0];
    u64 line_length = 0;
    u32 line_number = 1;
    while (filesystem_read_line(f, 511, &p, &line_length))
    {
        // Trim string
        char* trimmed = string_trim(line_buf);

        // Get trimmed length
        line_length = string_length(trimmed);

        // Skip blank lines and comments.
        if (line_length < 1 || trimmed[0] == '#')
        {
            line_number++;
            continue;
        }

        // Split into var/value
        i32 equal_index = string_index_of(trimmed, '=');
        if (equal_index == -1)
        {
            BWARN("Potential formatting issue found in file: '=' token not found. Skipping line %u", line_number);
            line_number++;
            continue;
        }

        // Assume max of 64 characters for variable name
        char raw_var_name[64];
        bzero_memory(raw_var_name, sizeof(char) * 64);
        string_mid(raw_var_name, trimmed, 0, equal_index);
        char* trimmed_var_name = string_trim(raw_var_name);

        // Assume max of 511-65 (446) for the max length of the value to account for the variable name and the '='
        char raw_value[446];
        bzero_memory(raw_value, sizeof(char) * 446);
        string_mid(raw_value, trimmed, equal_index + 1, -1);  // Read rest of the line
        char* trimmed_value = string_trim(raw_value);

        // Process variable
        if (strings_equali(trimmed_var_name, "version"))
        {
            // TODO: version
        }
        else if (strings_equali(trimmed_var_name, "file"))
        {
            char* format_str = "%s/%s/%s";
            char full_file_path[512];
            string_format_unsafe(full_file_path, format_str, resource_system_base_path(), type_path, trimmed_value);

            // Open and read font file as binary, and save into an allocated buffer on resource itself
            file_handle font_binary_handle;
            if (!filesystem_open(full_file_path, FILE_MODE_READ, true, &font_binary_handle))
            {
                BERROR("Unable to open binary font file. Load process failed");
                return false;
            }
            u64 file_size;
            if (!filesystem_size(&font_binary_handle, &file_size))
            {
                BERROR("Unable to get file size of binary font file. Load process failed");
                return false;
            }
            out_resource->font_binary = ballocate(file_size, MEMORY_TAG_RESOURCE);
            if (!filesystem_read_all_bytes(&font_binary_handle, out_resource->font_binary, &out_resource->binary_size))
            {
                BERROR("Unable to perform binary read on font file. Load process failed");
                return false;
            }

            if (out_resource->binary_size != file_size)
                BWARN("Mismatch between filesize and bytes read in font file. File may be corrupt");

            filesystem_close(&font_binary_handle);
        }
        else if (strings_equali(trimmed_var_name, "face"))
        {
            // Read in the font face and store it for later
            system_font_face new_face;
            string_ncopy(new_face.name, trimmed_value, 255);
            darray_push(out_resource->fonts, new_face);
        }

        // Clear line buffer
        bzero_memory(line_buf, sizeof(char) * 512);
        line_number++;
    }

    filesystem_close(f);

    // Check here to make sure a binary was loaded, and at least one font face was found
    if (!out_resource->font_binary || darray_length(out_resource->fonts) < 1)
    {
        BERROR("Font configuration did not provide a binary and at least one font face. Load process failed");
        return false;
    }

    return write_bsf_file(out_bsf_filename, out_resource);
}

static b8 read_bsf_file(file_handle* file, system_font_resource_data* data)
{
    bzero_memory(data, sizeof(system_font_resource_data));

    u64 bytes_read = 0;
    u32 read_size = 0;

    // Write resource header first
    resource_header header;
    read_size = sizeof(resource_header);
    CLOSE_IF_FAILED(filesystem_read(file, read_size, &header, &bytes_read), file);

    // Verify header contents
    if (header.magic_number != RESOURCE_MAGIC && header.resource_type == RESOURCE_TYPE_SYSTEM_FONT)
    {
        BERROR("BSF file header is invalid and cannot be read");
        filesystem_close(file);
        return false;
    }

    // TODO: read file version

    // Size of font binary
    read_size = sizeof(u64);
    CLOSE_IF_FAILED(filesystem_read(file, read_size, &data->binary_size, &bytes_read), file);

    // Font binary itself
    read_size = data->binary_size;
    CLOSE_IF_FAILED(filesystem_read(file, read_size, &data->font_binary, &bytes_read), file);

    // Number of fonts
    u32 font_count = darray_length(data->fonts);
    read_size = sizeof(u32);
    CLOSE_IF_FAILED(filesystem_read(file, read_size, &font_count, &bytes_read), file);

    // Iterate faces metadata and output that as well
    for (u32 i = 0; i < font_count; ++i)
    {
        // Length of face name string
        u32 face_length = string_length(data->fonts[i].name);
        read_size = sizeof(u32);
        CLOSE_IF_FAILED(filesystem_read(file, read_size, &face_length, &bytes_read), file);

        // Face string
        read_size = sizeof(char) * face_length + 1;
        CLOSE_IF_FAILED(filesystem_read(file, read_size, data->fonts[i].name, &bytes_read), file);
    }

    return true;
}

static b8 write_bsf_file(const char* out_bsf_filename, system_font_resource_data* resource)
{
    // TODO: Implement binary system font
    return true;
}

static void system_font_loader_unload(struct resource_loader* self, resource* resource)
{
    if (self && resource)
    {
        system_font_resource_data* data = (system_font_resource_data*)resource->data;
        if (data->fonts)
        {
            darray_destroy(data->fonts);
            data->fonts = 0;
        }

        if (data->font_binary)
        {
            bfree(data->font_binary, data->binary_size, MEMORY_TAG_RESOURCE);
            data->font_binary = 0;
            data->binary_size = 0;
        }
    }
}

resource_loader system_font_resource_loader_create(void)
{
    resource_loader loader;
    loader.type = RESOURCE_TYPE_SYSTEM_FONT;
    loader.custom_type = 0;
    loader.load = system_font_loader_load;
    loader.unload = system_font_loader_unload;
    loader.type_path = "fonts";

    return loader;
}
