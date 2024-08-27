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
    bresource_request_info request_info;
    array_bimage_ptr assets;
    u32 loaded_count;
} texture_resource_handler_info;

static void texture_basset_on_result(asset_request_result result, const struct basset* asset, void* listener_inst);

b8 bresource_handler_texture_request(struct bresource_handler* self, bresource* resource, bresource_request_info info)
{
    if (!self || !resource)
    {
        BERROR("bresource_handler_texture_request requires valid pointers to self and resource");
        return false;
    }

    bresource_texture* typed_resource = (bresource_texture*)resource;

    // NOTE: dynamically allocating this so lifetime isn't a concern
    texture_resource_handler_info* listener_inst = ballocate(sizeof(texture_resource_handler_info), MEMORY_TAG_RESOURCE);
    listener_inst->request_info = info;
    listener_inst->typed_resource = typed_resource;
    listener_inst->handler = self;
    listener_inst->loaded_count = 0;
    listener_inst->assets = array_bimage_ptr_create(info.assets.base.length);

    // Load all assets (might only be one)
    for (array_iterator it = info.assets.begin(&info.assets.base); !it.end(&it); it.next(&it))
    {
        bresource_asset_info* asset_info = it.value(&it);
        asset_system_request(
            self->asset_system,
            asset_info->type,
            asset_info->package_name,
            asset_info->asset_name,
            true,
            listener_inst,
            texture_basset_on_result);
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
        if (listener->loaded_count == listener->request_info.assets.base.length)
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
                listener->typed_resource->type,
                width,
                height,
                channel_count,
                mip_levels,
                listener->assets.base.length, // TODO: maybe configured instead? Or listener->typed_resource->array_size
                listener->typed_resource->flags,
                &listener->typed_resource->renderer_texture_handle);
            if (!result)
            {
                BWARN("Failed to acquire GPU resources for resource '%s'. Resource will not be available for use", bname_string_get(listener->typed_resource->base.name));
            }
            else
            {
                // Save off the properties of the first asset
                listener->typed_resource->width = width;
                listener->typed_resource->height = height;
                listener->typed_resource->format = image_format_to_texture_format(((basset_image*)listener->assets.data[0])->format);
                listener->typed_resource->mip_levels = mip_levels;
                /* listener->typed_resource->type = // TODO: should be part of the request */
                /* listener->typed_resource->array_size = // TODO: should be part of the request */
                listener->typed_resource->flags = 0; // TODO: Should be part of request/maybe determined by image format

                // Iterate the assets and ensure the dimensions are all the same. This is because a texture that is using multiple assets is either
                // using them one-per-layer OR is combining multiple image assets into one (i.e. the "combined" image for materials). In either case, all dimensions must be the same
                u32 asset_loaded_count = 0;
                for (array_iterator it = listener->assets.begin(&listener->assets.base); !it.end(&it); it.next(&it))
                {
                    b8 mismatch = false;
                    basset_image* image = it.value(&it);

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
                    if (image->channel_count != channel_count)
                    {
                        BERROR("Channel count mismatch at index %u. Expected: %u, Actual: %u", it.pos, channel_count, image->channel_count);
                        mismatch = true;
                    }
                    if (image->mip_levels != mip_levels)
                    {
                        BERROR("Mip levels mismatch at index %u. Expected: %u, Actual: %u", it.pos, mip_levels, image->mip_levels);
                        mismatch = true;
                    }

                    if (!mismatch)
                    {
                        u32 offset = 0; // TODO: Should this be configured?
                        b8 write_result = renderer_texture_write_data(renderer, listener->typed_resource->renderer_texture_handle, offset, image->pixel_array_size, image->pixels);
                        if (!write_result)
                        {
                            BERROR("Failed to write texture data from asset '%s'", bname_string_get(image->base.name));
                            continue;
                        }

                        // Release the asset reference as we are done with it
                        asset_system_release(asset_system, image->base.package_name, image->base.name);

                        asset_loaded_count++;
                    }
                }

                // If all assets uploaded successfully, the resource can be have its state updated
                if (asset_loaded_count == listener->assets.base.length)
                {
                    listener->typed_resource->base.state = BRESOURCE_STATE_LOADED;
                    // Increase the generation also
                    listener->typed_resource->base.generation++;
                }
            }
        }
    }
    else
    {
        BERROR("Failed to load a required asset for texture resource '%s'. Resource may not appear correctly when rendered", bname_string_get(listener->typed_resource->base.name));
    }
}
