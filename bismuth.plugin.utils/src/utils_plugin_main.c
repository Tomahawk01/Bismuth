#include "utils_plugin_main.h"

#include "importers/basset_importer_bitmap_font_fnt.h"
#include "importers/basset_importer_image.h"
#include "importers/basset_importer_static_mesh_obj.h"

#include <assets/basset_importer_registry.h>
#include <assets/basset_types.h>
#include <logger.h>
#include <memory/bmemory.h>
#include <plugins/plugin_types.h>

b8 bplugin_create(struct bruntime_plugin* out_plugin)
{
    if (!out_plugin)
    {
        BERROR("Cannot create a plugin without a pointer to hold it!");
        return false;
    }

    // NOTE: This plugin has no state
    out_plugin->plugin_state_size = 0;
    out_plugin->plugin_state = 0;

    // TODO: Register known importer types

    // Images - one per file extension
    {
        const char* image_types[] = {"tga", "png", "jpg", "bmp"};
        for (u8 i = 0; i < 4; ++i)
        {
            basset_importer image_importer = {0};
            image_importer.import = basset_importer_image_import;
            if (!basset_importer_registry_register(BASSET_TYPE_IMAGE, image_types[i], image_importer))
            {
                BERROR("Failed to register image asset importer!");
                return false;
            }
        }
    }

    // Static mesh - Wavefront OBJ
    {
        basset_importer obj_importer = {0};
        obj_importer.import = basset_importer_static_mesh_obj_import;
        if (!basset_importer_registry_register(BASSET_TYPE_STATIC_MESH, "obj", obj_importer))
        {
            BERROR("Failed to register static mesh Wavefront OBJ asset importer!");
            return false;
        }
    }

    // Bitmaps fonts - FNT
    {
        basset_importer fnt_importer = {0};
        fnt_importer.import = basset_importer_bitmap_font_fnt;
        if (!basset_importer_registry_register(BASSET_TYPE_BITMAP_FONT, "fnt", fnt_importer))
        {
            BERROR("Failed to register bitmap font FNT asset importer!");
            return false;
        }
    }

    // Audio - one per file extension
    {
        const char* audio_types[] = {"mp3", "ogg", "wav"};
        for (u8 i = 0; i < 3; ++i)
        {
            basset_importer image_importer = {0};
            image_importer.import = basset_importer_image_import;
            if (!basset_importer_registry_register(BASSET_TYPE_IMAGE, audio_types[i], image_importer))
            {
                BERROR("Failed to register image asset importer!");
                return false;
            }
        }
    }

    BINFO("Bismuth Utils Plugin Creation successful");

    return true;
}

b8 bplugin_initialize(struct bruntime_plugin* plugin)
{
    if (!plugin)
    {
        BERROR("Cannot initialize a plugin without a pointer to it");
        return false;
    }

    BINFO("Bismuth Utils plugin initialized successfully");

    return true;
}

void bplugin_destroy(struct bruntime_plugin* plugin)
{
    if (plugin)
    {
        // A no-op for this plugin since there is no state
    }
}
