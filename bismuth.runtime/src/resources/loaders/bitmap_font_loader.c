#include "bitmap_font_loader.h"

#include "logger.h"
#include "memory/bmemory.h"
#include "resources/font_types.h"
#include "resources/resource_types.h"
#include "systems/resource_system.h"
#include "strings/bstring.h"
#include "loader_utils.h"
#include "platform/filesystem.h"

#include <stdio.h> //sscanf

typedef enum bitmap_font_file_type
{
    BITMAP_FONT_FILE_TYPE_NOT_FOUND,
    BITMAP_FONT_FILE_TYPE_BBF,
    BITMAP_FONT_FILE_TYPE_FNT
} bitmap_font_file_type;

typedef struct supported_bitmap_font_filetype
{
    char* extension;
    bitmap_font_file_type type;
    b8 is_binary;
} supported_bitmap_font_filetype;

static b8 import_fnt_file(file_handle* fnt_file, const char* out_bbf_filename, bitmap_font_resource_data* out_data);
static b8 read_bbf_file(file_handle* bbf_file, bitmap_font_resource_data* data);
static b8 write_bbf_file(const char* path, bitmap_font_resource_data* data);

static b8 bitmap_font_loader_load(struct resource_loader* self, const char* name, void* params, resource* out_resource)
{
    if (!self || !name || !out_resource)
        return false;

    char* format_str = "%s/%s/%s%s";
    file_handle f;
// Supported extensions. NOTE: they are in order of priority when looked up
#define SUPPORTED_FILETYPE_COUNT 2
    supported_bitmap_font_filetype supported_filetypes[SUPPORTED_FILETYPE_COUNT];
    supported_filetypes[0] = (supported_bitmap_font_filetype){".bbf", BITMAP_FONT_FILE_TYPE_BBF, true};
    supported_filetypes[1] = (supported_bitmap_font_filetype){".fnt", BITMAP_FONT_FILE_TYPE_FNT, false};

    char full_file_path[512];
    bitmap_font_file_type type = BITMAP_FONT_FILE_TYPE_NOT_FOUND;
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

    if (type == BITMAP_FONT_FILE_TYPE_NOT_FOUND)
    {
        BERROR("Unable to find bitmap font of supported type called '%s'", name);
        return false;
    }

    out_resource->full_path = string_duplicate(full_file_path);

    bitmap_font_resource_data resource_data;
    resource_data.data.type = FONT_TYPE_BITMAP;

    b8 result = false;
    switch (type)
    {
        case BITMAP_FONT_FILE_TYPE_FNT:
        {
            // Generate BBF filename
            char bbf_file_name[512];
            string_format_unsafe(bbf_file_name, "%s/%s/%s%s", resource_system_base_path(), self->type_path, name, ".bbf");
            result = import_fnt_file(&f, bbf_file_name, &resource_data);
            break;
        }
        case BITMAP_FONT_FILE_TYPE_BBF:
            result = read_bbf_file(&f, &resource_data);
            break;
        case BITMAP_FONT_FILE_TYPE_NOT_FOUND:
            BERROR("Unable to find bitmap font of supported type called '%s'", name);
            result = false;
            break;
    }

    filesystem_close(&f);

    if (!result)
    {
        BERROR("Failed to process bitmap font file '%s'", full_file_path);
        string_free(out_resource->full_path);
        out_resource->data = 0;
        out_resource->data_size = 0;
        return false;
    }

    out_resource->data = ballocate(sizeof(bitmap_font_resource_data), MEMORY_TAG_RESOURCE);
    bcopy_memory(out_resource->data, &resource_data, sizeof(bitmap_font_resource_data));
    out_resource->data_size = sizeof(bitmap_font_resource_data);

    return true;
}

static void bitmap_font_loader_unload(struct resource_loader* self, resource* resource)
{
    if (self && resource)
    {
        if (resource->data)
        {
            bitmap_font_resource_data* data = (bitmap_font_resource_data*)resource->data;
            if (data->data.glyph_count && data->data.glyphs)
            {
                bfree(data->data.glyphs, sizeof(font_glyph) * data->data.glyph_count, MEMORY_TAG_ARRAY);
                data->data.glyphs = 0;
            }

            if (data->data.kerning_count && data->data.kernings)
            {
                bfree(data->data.kernings, sizeof(font_kerning) * data->data.kerning_count, MEMORY_TAG_ARRAY);
                data->data.kernings = 0;
            }

            if (data->page_count && data->pages)
            {
                bfree(data->pages, sizeof(bitmap_font_page) * data->page_count, MEMORY_TAG_ARRAY);
                data->pages = 0;
            }

            bfree(resource->data, sizeof(bitmap_font_resource_data), MEMORY_TAG_RESOURCE);
            resource->data = 0;
            resource->data_size = 0;
            resource->loader_id = INVALID_ID;

            if (resource->full_path)
            {
                u32 length = string_length(resource->full_path);
                bfree(resource->full_path, sizeof(char) * length, MEMORY_TAG_STRING);
                resource->full_path = 0;
            }
        }
    }
}

#define VERIFY_LINE(line_type, line_num, expected, actual)                                                                                    \
    if (actual != expected)                                                                                                                   \
    {                                                                                                                                         \
        BERROR("Error in file format reading type '%s', line %u. Expected %d element(s) but read %d", line_type, line_num, expected, actual); \
        return false;                                                                                                                         \
    }

static b8 import_fnt_file(file_handle* fnt_file, const char* out_bbf_filename, bitmap_font_resource_data* out_data)
{
    bzero_memory(out_data, sizeof(bitmap_font_resource_data));
    char line_buf[512] = "";
    char* p = &line_buf[0];
    u64 line_length = 0;
    u32 line_num = 0;
    u32 glyphs_read = 0;
    u8 pages_read = 0;
    u32 kernings_read = 0;
    while (true)
    {
        ++line_num;
        if (!filesystem_read_line(fnt_file, 511, &p, &line_length))
            break;

        // Skip blank lines
        if (line_length < 1)
            continue;

        char first_char = line_buf[0];
        switch (first_char)
        {
            case 'i':
            {
                // 'info' line

                // NOTE: only extract face and size, ignore rest
                i32 elements_read = sscanf(
                    line_buf,
                    "info face=\"%[^\"]\" size=%u",
                    out_data->data.face,
                    &out_data->data.size);
                VERIFY_LINE("info", line_num, 2, elements_read);
                break;
            }
            case 'c':
            {
                // 'common', 'char' or 'chars' line
                if (line_buf[1] == 'o')
                {
                    // common
                    i32 elements_read = sscanf(
                        line_buf,
                        "common lineHeight=%d base=%u scaleW=%d scaleH=%d pages=%d",  // ignore everything else
                        &out_data->data.line_height,
                        &out_data->data.baseline,
                        &out_data->data.atlas_size_x,
                        &out_data->data.atlas_size_y,
                        &out_data->page_count);

                    VERIFY_LINE("common", line_num, 5, elements_read);

                    // Allocate pages array
                    if (out_data->page_count > 0)
                    {
                        if (!out_data->pages)
                            out_data->pages = ballocate(sizeof(bitmap_font_page) * out_data->page_count, MEMORY_TAG_ARRAY);
                    }
                    else
                    {
                        BERROR("Pages is 0, which should not be possible. Font file reading aborted");
                        return false;
                    }
                }
                else if (line_buf[1] == 'h')
                {
                    if (line_buf[4] == 's')
                    {
                        // chars line
                        i32 elements_read = sscanf(line_buf, "chars count=%u", &out_data->data.glyph_count);
                        VERIFY_LINE("chars", line_num, 1, elements_read);

                        // Allocate glyphs array
                        if (out_data->data.glyph_count > 0)
                        {
                            if (!out_data->data.glyphs)
                                out_data->data.glyphs = ballocate(sizeof(font_glyph) * out_data->data.glyph_count, MEMORY_TAG_ARRAY);
                        }
                        else
                        {
                            BERROR("Glyph count is 0, which should not be possible. Font file reading aborted");
                            return false;
                        }
                    }
                    else
                    {
                        // Assume 'char' line
                        font_glyph* g = &out_data->data.glyphs[glyphs_read];

                        i32 elements_read = sscanf(
                            line_buf,
                            "char id=%d x=%hu y=%hu width=%hu height=%hu xoffset=%hd yoffset=%hd xadvance=%hd page=%hhu chnl=%*u",
                            &g->codepoint,
                            &g->x,
                            &g->y,
                            &g->width,
                            &g->height,
                            &g->x_offset,
                            &g->y_offset,
                            &g->x_advance,
                            &g->page_id);

                        VERIFY_LINE("char", line_num, 9, elements_read);

                        glyphs_read++;
                    }
                }
                else
                {
                    // invalid, ignore
                }
                break;
            }
            case 'p':
            {
                // 'page' line
                bitmap_font_page* page = &out_data->pages[pages_read];
                i32 elements_read = sscanf(
                    line_buf,
                    "page id=%hhi file=\"%[^\"]\"",
                    &page->id,
                    page->file);

                // Strip extension
                string_filename_no_extension_from_path(page->file, page->file);

                VERIFY_LINE("page", line_num, 2, elements_read);

                break;
            }
            case 'k':
            {
                // 'kernings' or 'kerning' line
                if (line_buf[7] == 's')
                {
                    // Kernings
                    i32 elements_read = sscanf(line_buf, "kernings count=%u", &out_data->data.kerning_count);

                    VERIFY_LINE("kernings", line_num, 1, elements_read);

                    // Allocate kernings array
                    if (!out_data->data.kernings)
                        out_data->data.kernings = ballocate(sizeof(font_kerning) * out_data->data.kerning_count, MEMORY_TAG_ARRAY);
                }
                else if (line_buf[7] == ' ')
                {
                    // Kerning record
                    font_kerning* k = &out_data->data.kernings[kernings_read];
                    i32 elements_read = sscanf(
                        line_buf,
                        "kerning first=%i  second=%i amount=%hi",
                        &k->codepoint_0,
                        &k->codepoint_1,
                        &k->amount);

                    VERIFY_LINE("kerning", line_num, 3, elements_read);
                }
                break;
            }
            default:
                // Skip the line
                break;
        }
    }

    // Write binary bitmap font file
    return write_bbf_file(out_bbf_filename, out_data);
}

static b8 read_bbf_file(file_handle* file, bitmap_font_resource_data* data)
{
    bzero_memory(data, sizeof(bitmap_font_resource_data));

    u64 bytes_read = 0;
    u32 read_size = 0;

    // Write resource header first
    resource_header header;
    read_size = sizeof(resource_header);
    CLOSE_IF_FAILED(filesystem_read(file, read_size, &header, &bytes_read), file);

    // Verify header contents
    if (header.magic_number != RESOURCE_MAGIC && header.resource_type == RESOURCE_TYPE_BITMAP_FONT)
    {
        BERROR("BBF file header is invalid and cannot be read");
        filesystem_close(file);
        return false;
    }

    // TODO: read file version

    // Length of face string
    u32 face_length;
    read_size = sizeof(u32);
    CLOSE_IF_FAILED(filesystem_read(file, read_size, &face_length, &bytes_read), file);

    // Face string
    read_size = sizeof(char) * face_length + 1;
    CLOSE_IF_FAILED(filesystem_read(file, read_size, data->data.face, &bytes_read), file);
    // Ensure zero-termination
    data->data.face[face_length] = 0;

    // Font size
    read_size = sizeof(u32);
    CLOSE_IF_FAILED(filesystem_read(file, read_size, &data->data.size, &bytes_read), file);

    // Line height
    read_size = sizeof(i32);
    CLOSE_IF_FAILED(filesystem_read(file, read_size, &data->data.line_height, &bytes_read), file);

    // Baseline
    read_size = sizeof(i32);
    CLOSE_IF_FAILED(filesystem_read(file, read_size, &data->data.baseline, &bytes_read), file);

    // scale x
    read_size = sizeof(i32);
    CLOSE_IF_FAILED(filesystem_read(file, read_size, &data->data.atlas_size_x, &bytes_read), file);

    // scale y
    read_size = sizeof(i32);
    CLOSE_IF_FAILED(filesystem_read(file, read_size, &data->data.atlas_size_y, &bytes_read), file);

    // page count
    read_size = sizeof(u32);
    CLOSE_IF_FAILED(filesystem_read(file, read_size, &data->page_count, &bytes_read), file);

    // Allocate pages array
    data->pages = ballocate(sizeof(bitmap_font_page) * data->page_count, MEMORY_TAG_ARRAY);

    // Read pages
    for (u32 i = 0; i < data->page_count; ++i)
    {
        // Page id
        read_size = sizeof(i8);
        CLOSE_IF_FAILED(filesystem_read(file, read_size, &data->pages[i].id, &bytes_read), file);

        // File name length
        u32 filename_length = string_length(data->pages[i].file) + 1;
        read_size = sizeof(u32);
        CLOSE_IF_FAILED(filesystem_read(file, read_size, &filename_length, &bytes_read), file);

        // The file name
        read_size = sizeof(char) * filename_length + 1;
        CLOSE_IF_FAILED(filesystem_read(file, read_size, data->pages[i].file, &bytes_read), file);
        // Ensure zero-termination
        data->pages[i].file[filename_length] = 0;
    }

    // Glyph count
    read_size = sizeof(u32);
    CLOSE_IF_FAILED(filesystem_read(file, read_size, &data->data.glyph_count, &bytes_read), file);

    // Allocate glyphs array
    data->data.glyphs = ballocate(sizeof(font_glyph) * data->data.glyph_count, MEMORY_TAG_ARRAY);

    // Read glyphs. These don't contain any strings, so can just read in the entire block
    read_size = sizeof(font_glyph) * data->data.glyph_count;
    CLOSE_IF_FAILED(filesystem_read(file, read_size, data->data.glyphs, &bytes_read), file);

    // Kerning Count
    read_size = sizeof(u32);
    CLOSE_IF_FAILED(filesystem_read(file, read_size, &data->data.kerning_count, &bytes_read), file);

    // It's possible to have font with no kernings. If this is the case, nothing can be read
    if (data->data.kerning_count > 0)
    {
        // Allocate kernings array
        data->data.kernings = ballocate(sizeof(font_glyph) * data->data.kerning_count, MEMORY_TAG_ARRAY);

        // No strings for kernings, so read entire block
        read_size = sizeof(font_kerning) * data->data.kerning_count;
        CLOSE_IF_FAILED(filesystem_read(file, read_size, data->data.kernings, &bytes_read), file);
    }

    filesystem_close(file);

    return true;
}

static b8 write_bbf_file(const char* path, bitmap_font_resource_data* data)
{
    // Header info first
    file_handle file;
    if (!filesystem_open(path, FILE_MODE_WRITE, true, &file))
    {
        BERROR("Failed to open file for writing: %s", path);
        return false;
    }

    u64 bytes_written = 0;
    u32 write_size = 0;

    // Write the resource header first
    resource_header header;
    header.magic_number = RESOURCE_MAGIC;
    header.resource_type = RESOURCE_TYPE_BITMAP_FONT;
    header.version = 0x01U; // Version 1 for now
    header.reserved = 0;
    write_size = sizeof(resource_header);
    CLOSE_IF_FAILED(filesystem_write(&file, write_size, &header, &bytes_written), &file);

    // Length of face string
    u32 face_length = string_length(data->data.face);
    write_size = sizeof(u32);
    CLOSE_IF_FAILED(filesystem_write(&file, write_size, &face_length, &bytes_written), &file);

    // Face string
    write_size = sizeof(char) * face_length + 1;
    CLOSE_IF_FAILED(filesystem_write(&file, write_size, data->data.face, &bytes_written), &file);

    // Font size
    write_size = sizeof(u32);
    CLOSE_IF_FAILED(filesystem_write(&file, write_size, &data->data.size, &bytes_written), &file);

    // Line height
    write_size = sizeof(i32);
    CLOSE_IF_FAILED(filesystem_write(&file, write_size, &data->data.line_height, &bytes_written), &file);

    // Baseline
    write_size = sizeof(i32);
    CLOSE_IF_FAILED(filesystem_write(&file, write_size, &data->data.baseline, &bytes_written), &file);

    // scale x
    write_size = sizeof(i32);
    CLOSE_IF_FAILED(filesystem_write(&file, write_size, &data->data.atlas_size_x, &bytes_written), &file);

    // scale y
    write_size = sizeof(i32);
    CLOSE_IF_FAILED(filesystem_write(&file, write_size, &data->data.atlas_size_y, &bytes_written), &file);

    // page count
    write_size = sizeof(u32);
    CLOSE_IF_FAILED(filesystem_write(&file, write_size, &data->page_count, &bytes_written), &file);

    // Write pages
    for (u32 i = 0; i < data->page_count; ++i)
    {
        // Page id
        write_size = sizeof(i8);
        CLOSE_IF_FAILED(filesystem_write(&file, write_size, &data->pages[i].id, &bytes_written), &file);

        // File name length
        u32 filename_length = string_length(data->pages[i].file) + 1;
        write_size = sizeof(u32);
        CLOSE_IF_FAILED(filesystem_write(&file, write_size, &filename_length, &bytes_written), &file);

        // The file name
        write_size = sizeof(char) * filename_length + 1;
        CLOSE_IF_FAILED(filesystem_write(&file, write_size, data->pages[i].file, &bytes_written), &file);
    }

    // Glyph count
    write_size = sizeof(u32);
    CLOSE_IF_FAILED(filesystem_write(&file, write_size, &data->data.glyph_count, &bytes_written), &file);

    // Write glyphs
    write_size = sizeof(font_glyph) * data->data.glyph_count;
    CLOSE_IF_FAILED(filesystem_write(&file, write_size, data->data.glyphs, &bytes_written), &file);

    // Kerning Count
    write_size = sizeof(u32);
    CLOSE_IF_FAILED(filesystem_write(&file, write_size, &data->data.kerning_count, &bytes_written), &file);

    // It's possible to have font with no kernings. If this is the case, nothing can be written
    if (data->data.kerning_count > 0)
    {
        write_size = sizeof(font_kerning) * data->data.kerning_count;
        CLOSE_IF_FAILED(filesystem_write(&file, write_size, data->data.kernings, &bytes_written), &file);
    }

    filesystem_close(&file);

    return true;
}

resource_loader bitmap_font_resource_loader_create(void)
{
    resource_loader loader;
    loader.type = RESOURCE_TYPE_BITMAP_FONT;
    loader.custom_type = 0;
    loader.load = bitmap_font_loader_load;
    loader.unload = bitmap_font_loader_unload;
    loader.type_path = "fonts";

    return loader;
}
