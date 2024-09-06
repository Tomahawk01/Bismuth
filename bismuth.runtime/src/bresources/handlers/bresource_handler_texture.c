#include "bresource_handler_texture.h"

#include "assets/basset_types.h"
#include "containers/array.h"
#include "core/engine.h"
#include "bresources/bresource_types.h"
#include "bresources/bresource_utils.h"
#include "logger.h"
#include "memory/bmemory.h"
#include "renderer/renderer_frontend.h"
#include "renderer/renderer_types.h"
#include "strings/bname.h"
#include "systems/asset_system.h"
#include "systems/bresource_system.h"

// TODO: move this to basset_types
ARRAY_TYPE_NAMED(const basset_image*, bimage_ptr);

typedef struct texture_resource_handler_info
{
    bresource_texture* typed_resource;
    bresource_handler* handler;
    bresource_texture_request_info* request_info;
    array_bimage_ptr assets;
    u32 loaded_count;
} texture_resource_handler_info;

static void texture_basset_on_result(asset_request_result result, const struct basset* asset, void* listener_inst);

b8 bresource_handler_texture_request(struct bresource_handler* self, bresource* resource, const struct bresource_request_info* info)
{
    if (!self || !resource)
    {
        BERROR("bresource_handler_texture_request requires valid pointers to self and resource");
        return false;
    }

    bresource_texture* typed_resource = (bresource_texture*)resource;
    bresource_texture_request_info* typed_request = (bresource_texture_request_info*)info;
    struct renderer_system_state* renderer = engine_systems_get()->renderer_system;

    b8 assets_required = true;

    // Assets are not required for these texture types
    if (typed_request->flags & BRESOURCE_TEXTURE_FLAG_IS_WRITEABLE || typed_request->flags & BRESOURCE_TEXTURE_FLAG_DEPTH)
        assets_required = false;

    // Some type-specific validation
    if (assets_required)
    {
        if (typed_request->texture_type == BRESOURCE_TEXTURE_TYPE_2D && typed_request->base.assets.base.length != 1)
        {
            BERROR("Non-writeable 2d textures must have exactly one texture asset. Instead, %u was provided", typed_request->base.assets.base.length);
            return false;
        }
        else if (typed_request->texture_type == BRESOURCE_TEXTURE_TYPE_CUBE && typed_request->base.assets.base.length != 6)
        {
            BERROR("Non-writeable cube textures must have exactly 6 texture assets. Instead, %u was provided", typed_request->base.assets.base.length);
            return false;
        }
        else if (assets_required && typed_request->base.assets.base.length < 1)
        {
            BERROR("A texture resource request requires at least one asset for textures that are not depth or writeable textures");
            return false;
        }
    }

    // NOTE: dynamically allocating this so lifetime isn't a concern
    texture_resource_handler_info* listener_inst = ballocate(sizeof(texture_resource_handler_info), MEMORY_TAG_RESOURCE);
    // Take a copy of the typed request info
    listener_inst->request_info = ballocate(sizeof(bresource_texture_request_info), MEMORY_TAG_RESOURCE);
    bcopy_memory(listener_inst->request_info, typed_request, sizeof(bresource_texture_request_info));
    listener_inst->typed_resource = typed_resource;
    listener_inst->handler = self;
    listener_inst->loaded_count = 0;
    if (assets_required)
        listener_inst->assets = array_bimage_ptr_create(info->assets.base.length);

    // Load all assets (might only be one)
    if (info->assets.data)
    {
        for (array_iterator it = info->assets.begin(&info->assets.base); !it.end(&it); it.next(&it))
        {
            bresource_asset_info* asset_info = it.value(&it);
            if (asset_info->type == BASSET_TYPE_IMAGE)
            {
                asset_system_request(
                    self->asset_system,
                    asset_info->type,
                    asset_info->package_name,
                    asset_info->asset_name,
                    true,
                    listener_inst,
                    texture_basset_on_result);
            }
            else if (asset_info->type == BASSET_TYPE_UNKNOWN)
            {
                // This means load pixel data
                bresource_texture_pixel_data* px = &typed_request->pixel_data.data[it.pos];
                u32 texture_data_offset = 0; // NOTE: The only time this potentially could be nonzero is when explicitly loading a layer of texture data
                b8 write_result = renderer_texture_write_data(renderer, typed_resource->renderer_texture_handle, texture_data_offset, px->pixel_array_size, px->pixels);
                if (!write_result)
                    BERROR("Failed to write renderer texture data resource '%s'", bname_string_get(typed_resource->base.name));
            }
        }
    }
    else if (typed_request->pixel_data.data)
    {
        // Pixel data is available immediately and can be loaded thusly

        struct renderer_system_state* renderer = engine_systems_get()->renderer_system;

        // Flip to a "loading" state
        typed_resource->base.state = BRESOURCE_STATE_LOADING;

        // Apply properties taken from request
        typed_resource->type = typed_request->texture_type;
        typed_resource->array_size = typed_request->array_size;
        typed_resource->flags = typed_request->flags;

        // Save off the properties of the first asset
        // Start by taking the dimensions of just the first pixel data
        bresource_texture_pixel_data* first_px_data = &typed_request->pixel_data.data[0];
        typed_resource->width = first_px_data->width;
        typed_resource->height = first_px_data->height;
        typed_resource->format = first_px_data->format;
        typed_resource->mip_levels = first_px_data->mip_levels;
        typed_resource->array_size = typed_request->pixel_data.base.length;

        // Acquire the resources for the texture
        b8 acquisition_result = renderer_bresource_texture_resources_acquire(
            renderer,
            resource->name,
            typed_resource->type,
            typed_resource->width,
            typed_resource->height,
            channel_count_from_texture_format(typed_resource->format),
            typed_resource->mip_levels,
            typed_resource->array_size,
            typed_resource->flags,
            &typed_resource->renderer_texture_handle);

        if (!acquisition_result)
        {
            BERROR("Failed to acquire renderer texture resources (from pixel data) for resource '%s'", bname_string_get(typed_resource->base.name));
            return false;
        }

        // TODO: offsets per layer. Each pixel data would be a layer of its own
        for (array_iterator it = typed_request->pixel_data.begin(&typed_request->pixel_data.base); !it.end(&it); it.next(&it))
        {
            bresource_texture_pixel_data* px = it.value(&it);
            u32 texture_data_offset = 0; // NOTE: The only time this potentially could be nonzero is when explicitly loading a layer of texture data
            b8 write_result = renderer_texture_write_data(renderer, typed_resource->renderer_texture_handle, texture_data_offset, px->pixel_array_size, px->pixels);
            if (!write_result)
                BERROR("Failed to write renderer texture data resource '%s'", bname_string_get(typed_resource->base.name));
        }

        // Flip to a "loaded" state
        typed_resource->base.state = BRESOURCE_STATE_LOADED;
    }
    else
    {
        // No assets, no pixel data. Must be writeable or depth texture
        // Nothing to upload, so this is available immediately

        struct renderer_system_state* renderer = engine_systems_get()->renderer_system;

        // Flip to a "loading" state
        typed_resource->base.state = BRESOURCE_STATE_LOADING;

        // Apply properties taken from request
        typed_resource->type = typed_request->texture_type;
        typed_resource->array_size = typed_request->array_size;
        typed_resource->flags = typed_request->flags;

        // Save off the properties of the first asset
        // Start by taking the dimensions of just the first pixel data
        typed_resource->width = typed_request->width;
        typed_resource->height = typed_request->height;
        typed_resource->format = typed_request->format;
        typed_resource->mip_levels = typed_request->mip_levels;
        typed_resource->array_size = typed_request->array_size;

        // Acquire the resources for the texture
        b8 acquisition_result = renderer_bresource_texture_resources_acquire(
            renderer,
            resource->name,
            typed_resource->type,
            typed_resource->width,
            typed_resource->height,
            channel_count_from_texture_format(typed_resource->format),
            typed_resource->mip_levels,
            typed_resource->array_size,
            typed_resource->flags,
            &typed_resource->renderer_texture_handle);

        if (!acquisition_result)
        {
            BERROR("Failed to acquire renderer texture resources (from pixel data) for resource '%s'", bname_string_get(typed_resource->base.name));
            return false;
        }

        typed_resource->base.state = BRESOURCE_STATE_LOADED;
        // Increase the generation also
        typed_resource->base.generation++;
    }

    return true;
}

void bresource_handler_texture_release(struct bresource_handler* self, bresource* resource)
{
    if (resource)
    {
        //
    }
}

static void texture_basset_on_result(asset_request_result result, const struct basset* asset, void* listener_inst)
{
    texture_resource_handler_info* listener = (texture_resource_handler_info*)listener_inst;
    if (result == ASSET_REQUEST_RESULT_SUCCESS)
    {
        // Save off the asset pointer to the array
        listener->assets.data[listener->loaded_count] = (basset_image*)asset;
        listener->loaded_count++;

        // If all required assets are loaded, proceed with uploading of resource data
        if (listener->loaded_count == listener->request_info->base.assets.base.length)
        {
            listener->typed_resource->base.state = BRESOURCE_STATE_INITIALIZED;
            BTRACE("All required assets loaded for resource '%s'. Proceeding to upload to GPU...", bname_string_get(listener->typed_resource->base.name));

            // Start by taking the dimensions of just the first image
            u32 width = listener->assets.data[0]->width;
            u32 height = listener->assets.data[0]->height;
            u8 channel_count = listener->assets.data[0]->channel_count;
            u8 mip_levels = listener->assets.data[0]->mip_levels;

            struct renderer_system_state* renderer = engine_systems_get()->renderer_system;
            struct asset_system_state* asset_system = engine_systems_get()->asset_state;

            // Flip to a "loading" state
            listener->typed_resource->base.state = BRESOURCE_STATE_LOADING;

            // Acquire GPU resources for the texture resource
            b8 result = renderer_bresource_texture_resources_acquire(
                renderer,
                listener->typed_resource->base.name,
                listener->request_info->texture_type,
                width,
                height,
                channel_count,
                mip_levels,
                listener->request_info->array_size, // TODO: maybe configured instead? Or listener->typed_resource->array_size
                listener->request_info->flags,
                &listener->typed_resource->renderer_texture_handle);
            if (!result)
            {
                BWARN("Failed to acquire GPU resources for resource '%s'. Resource will not be available for use", bname_string_get(listener->typed_resource->base.name));
            }
            else
            {
                // Apply properties taken from request
                listener->typed_resource->type = listener->request_info->texture_type;
                listener->typed_resource->array_size = listener->request_info->array_size;
                listener->typed_resource->flags = listener->request_info->flags;

                // Save off the properties of the first asset
                listener->typed_resource->width = width;
                listener->typed_resource->height = height;
                listener->typed_resource->format = image_format_to_texture_format(((basset_image*)listener->assets.data[0])->format);
                listener->typed_resource->mip_levels = mip_levels;
                
                // Take a copy of all the pixel data from the assets so they may be released
                u32 all_pixel_size = 0;
                u8* all_pixels = 0;

                // Iterate the assets and ensure the dimensions are all the same. This is because a texture that is using multiple assets is either
                // using them one-per-layer OR is combining multiple image assets into one (i.e. the "combined" image for materials). In either case, all dimensions must be the same
                array_b8 mismatches = array_b8_create(listener->assets.base.length);
                for (array_iterator it = listener->assets.begin((const array_base*)&listener->assets); !it.end(&it); it.next(&it))
                {
                    b8 mismatch = false;
                    const basset_image* image =  listener->assets.data[it.pos]; // it.value(&it);

                    // Verify and report any mismatches
                    if (image->width != width)
                    {
                        BERROR("Width mismatch at index %u. Expected: %u, Actual: %u", it.pos, width, image->width);
                        mismatch = true;
                    }
                    if (image->height != height)
                    {
                        BERROR("Height mismatch at index %u. Expected: %u, Actual: %u", it.pos, height, image->height);
                        mismatch = true;
                    }

                    mismatches.data[it.pos] = mismatch;

                    // Keep a running size of pixels required
                    // TODO: Check if only utilizing a single channel, or maybe not all of the channels and load that way instead
                    if (!mismatch)
                        all_pixel_size += image->pixel_array_size;
                }

                if (all_pixel_size > 0)
                {
                    // Allocate the large array
                    all_pixels = ballocate(all_pixel_size, MEMORY_TAG_RESOURCE);

                    // Iterate again, this time copying data from each image into the appropriate layer
                    u32 pixel_array_offset = 0;
                    for (array_iterator it = listener->assets.begin(&listener->assets.base); !it.end(&it); it.next(&it))
                    {
                        // Skip mismatched textures
                        if (mismatches.data[it.pos])
                            continue;
                        // basset_image* image = it.value(&it);
                        const basset_image* image = listener->assets.data[it.pos];
                        bcopy_memory(all_pixels + pixel_array_offset, image->pixels, image->pixel_array_size);
                        pixel_array_offset += image->pixel_array_size;

                        // Release the asset reference as we are done with it
                        asset_system_release(asset_system, image->base.package_name, image->base.name);
                    }

                    array_b8_destroy(&mismatches);

                    // Perform the actual texture data upload
                    // TODO: Jobify this, renderer multithreading
                    u32 texture_data_offset = 0; // NOTE: The only time this potentially could be nonzero is when explicitly loading a layer of texture data
                    b8 write_result = renderer_texture_write_data(renderer, listener->typed_resource->renderer_texture_handle, texture_data_offset, all_pixel_size, all_pixels);
                    if (!write_result)
                        BERROR("Failed to write renderer texture data resource '%s'", bname_string_get(listener->typed_resource->base.name));

                    bfree(all_pixels, all_pixel_size, MEMORY_TAG_RESOURCE);

                    BTRACE("Renderer finished uploading texture data, texture is ready");
                }
                else
                {
                    BTRACE("Nothing to be uploaded, texture is ready");
                }

                // If uploaded successfully, the resource can be have its state updated
                listener->typed_resource->base.state = BRESOURCE_STATE_LOADED;
                // Increase the generation also
                listener->typed_resource->base.generation++;
            }
        }
    }
    else
    {
        BERROR("Failed to load a required asset for texture resource '%s'. Resource may not appear correctly when rendered", bname_string_get(listener->typed_resource->base.name));
    }

    // Destroy the request
    array_bimage_ptr_destroy(&listener->assets);
    array_bresource_asset_info_destroy(&listener->request_info->base.assets);
    bfree(listener->request_info, sizeof(bresource_texture_request_info), MEMORY_TAG_RESOURCE);
    // Free the listener itself
    bfree(listener, sizeof(texture_resource_handler_info), MEMORY_TAG_RESOURCE);
}
