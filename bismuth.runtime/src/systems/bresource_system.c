#include "bresource_system.h"

#include "containers/u64_bst.h"
#include "core/engine.h"
#include "debug/bassert.h"
#include "defines.h"
#include "bresources/handlers/bresource_handler_audio.h"
#include "bresources/handlers/bresource_handler_binary.h"
#include "bresources/handlers/bresource_handler_bitmap_font.h"
#include "bresources/handlers/bresource_handler_heightmap_terrain.h"
#include "bresources/handlers/bresource_handler_material.h"
#include "bresources/handlers/bresource_handler_scene.h"
#include "bresources/handlers/bresource_handler_shader.h"
#include "bresources/handlers/bresource_handler_static_mesh.h"
#include "bresources/handlers/bresource_handler_system_font.h"
#include "bresources/handlers/bresource_handler_text.h"
#include "bresources/handlers/bresource_handler_texture.h"
#include "bresources/bresource_types.h"
#include "bresources/bresource_utils.h"
#include "logger.h"
#include "memory/bmemory.h"
#include "strings/bname.h"
#include "systems/asset_system.h"

#include <core/event.h>
#include <containers/darray.h>

struct asset_system_state;

typedef struct resource_lookup
{
    // The resource itself, owned by this lookup
    bresource* r;
    // The current number of references to the resource
    i32 reference_count;
    // Indicates if the resource will be released when the reference_count reaches 0
    b8 auto_release;
} resource_lookup;

typedef struct bresource_system_state
{
    struct asset_system_state* asset_system;
    bresource_handler handlers[BRESOURCE_TYPE_COUNT];

    // Max number of resources that can be loaded at any given time
    u32 max_resource_count;
    // An array of lookups which contain reference and release data
    resource_lookup* lookups;
    // A BST to use for lookups of resources by bname
    bt_node* lookup_tree;

    // A BST to use for lookups of resources by file watch id
    bt_node* file_watch_lookup;
} bresource_system_state;

static void bresource_system_release_internal(struct bresource_system_state* state, bname resource_name, b8 force_release);
static void on_asset_system_hot_reload(void* listener, basset* asset);

b8 bresource_system_initialize(u64* memory_requirement, struct bresource_system_state* state, const bresource_system_config* config)
{
    BASSERT_MSG(memory_requirement, "Valid pointer to memory_requirement is required");

    *memory_requirement = sizeof(bresource_system_state);

    if (!state)
        return true;

    state->max_resource_count = config->max_resource_count;
    state->lookups = ballocate(sizeof(resource_lookup) * state->max_resource_count, MEMORY_TAG_ARRAY);
    state->lookup_tree = 0;
    state->file_watch_lookup = 0;

    state->asset_system = engine_systems_get()->asset_state;

    // Register known handler types

    // Text handler
    {
        bresource_handler handler = {0};
        handler.size = sizeof(bresource_text);
        handler.release = bresource_handler_text_release;
        handler.request = bresource_handler_text_request;
        handler.handle_hot_reload = bresource_handler_text_handle_hot_reload;
        if (!bresource_system_handler_register(state, BRESOURCE_TYPE_TEXT, handler))
        {
            BERROR("Failed to register text resource handler");
            return false;
        }
    }

    // Binary handler
    {
        bresource_handler handler = {0};
        handler.size = sizeof(bresource_binary);
        handler.release = bresource_handler_binary_release;
        handler.request = bresource_handler_binary_request;
        handler.handle_hot_reload = bresource_handler_binary_handle_hot_reload;
        if (!bresource_system_handler_register(state, BRESOURCE_TYPE_BINARY, handler))
        {
            BERROR("Failed to register binary resource handler");
            return false;
        }
    }

    // Texture handler
    {
        bresource_handler handler = {0};
        handler.size = sizeof(bresource_texture);
        handler.release = bresource_handler_texture_release;
        handler.request = bresource_handler_texture_request;
        if (!bresource_system_handler_register(state, BRESOURCE_TYPE_TEXTURE, handler))
        {
            BERROR("Failed to register texture resource handler");
            return false;
        }
    }

    // Material handler
    {
        bresource_handler handler = {0};
        handler.size = sizeof(bresource_material);
        handler.release = bresource_handler_material_release;
        handler.request = bresource_handler_material_request;
        if (!bresource_system_handler_register(state, BRESOURCE_TYPE_MATERIAL, handler))
        {
            BERROR("Failed to register material resource handler");
            return false;
        }
    }

    // Static mesh handler
    {
        bresource_handler handler = {0};
        handler.size = sizeof(bresource_static_mesh);
        handler.release = bresource_handler_static_mesh_release;
        handler.request = bresource_handler_static_mesh_request;
        if (!bresource_system_handler_register(state, BRESOURCE_TYPE_STATIC_MESH, handler))
        {
            BERROR("Failed to register static mesh resource handler");
            return false;
        }
    }

    // Shader handler
    {
        bresource_handler handler = {0};
        handler.size = sizeof(bresource_shader);
        handler.release = bresource_handler_shader_release;
        handler.request = bresource_handler_shader_request;
        if (!bresource_system_handler_register(state, BRESOURCE_TYPE_SHADER, handler))
        {
            BERROR("Failed to register shader resource handler");
            return false;
        }
    }

    // Bitmap font handler
    {
        bresource_handler handler = {0};
        handler.size = sizeof(bresource_bitmap_font);
        handler.release = bresource_handler_bitmap_font_release;
        handler.request = bresource_handler_bitmap_font_request;
        if (!bresource_system_handler_register(state, BRESOURCE_TYPE_BITMAP_FONT, handler))
        {
            BERROR("Failed to register bitmap font resource handler");
            return false;
        }
    }

    // System font handler
    {
        bresource_handler handler = {0};
        handler.size = sizeof(bresource_system_font);
        handler.release = bresource_handler_system_font_release;
        handler.request = bresource_handler_system_font_request;
        if (!bresource_system_handler_register(state, BRESOURCE_TYPE_SYSTEM_FONT, handler))
        {
            BERROR("Failed to register system font resource handler");
            return false;
        }
    }

    // Scene handler
    {
        bresource_handler handler = {0};
        handler.size = sizeof(bresource_scene);
        handler.release = bresource_handler_scene_release;
        handler.request = bresource_handler_scene_request;
        if (!bresource_system_handler_register(state, BRESOURCE_TYPE_SCENE, handler))
        {
            BERROR("Failed to register scene resource handler");
            return false;
        }
    }

    // Heightmap terrain handler
    {
        bresource_handler handler = {0};
        handler.size = sizeof(bresource_heightmap_terrain);
        handler.release = bresource_handler_heightmap_terrain_release;
        handler.request = bresource_handler_heightmap_terrain_request;
        if (!bresource_system_handler_register(state, BRESOURCE_TYPE_HEIGHTMAP_TERRAIN, handler))
        {
            BERROR("Failed to register heightmap terrain resource handler");
            return false;
        }
    }

    // Audio handler
    {
        bresource_handler handler = {0};
        handler.size = sizeof(bresource_audio);
        handler.release = bresource_handler_audio_release;
        handler.request = bresource_handler_audio_request;
        if (!bresource_system_handler_register(state, BRESOURCE_TYPE_AUDIO, handler))
        {
            BERROR("Failed to register audio resource handler");
            return false;
        }
    }

    // Register a callback with the asset system to get notified when an asset has been hot-reloaded
    asset_system_register_hot_reload_callback(state->asset_system, state, on_asset_system_hot_reload);

    BINFO("Resource system (new) initialized");
    return true;
}

void bresource_system_shutdown(struct bresource_system_state* state)
{
    if (state)
    {
        for (u32 i = 0; i < state->max_resource_count; ++i)
        {
            if (state->lookups[i].r)
                bresource_system_release_internal(state, state->lookups[i].r->name, true);
        }

        // Destroy the bst
        u64_bst_cleanup(state->lookup_tree);
        u64_bst_cleanup(state->file_watch_lookup);

        bfree(state->lookups, sizeof(resource_lookup) * state->max_resource_count, MEMORY_TAG_ARRAY);

        bzero_memory(state, sizeof(bresource_system_state));
    }
}

bresource* bresource_system_request(struct bresource_system_state* state, bname name, const struct bresource_request_info* info)
{
    BASSERT_MSG(state && info, "Valid pointers to state and info are required");

    // Attempt to find the resource by bname
    u32 lookup_index = INVALID_ID;
    const bt_node* node = u64_bst_find(state->lookup_tree, name);
    if (node)
        lookup_index = node->value.u32;

    if (lookup_index != INVALID_ID && state->lookups[lookup_index].r)
    {
        resource_lookup* lookup = &state->lookups[lookup_index];
        lookup->reference_count++;
        // Immediately issue the callback if setup
        if (info->user_callback)
            info->user_callback(lookup->r, info->listener_inst);

        // Return a pointer to the resource
        return lookup->r;
    }
    else
    {
        // Resource doesn't exist. Create a new one and its lookup
        // Look for an empty slot
        for (u32 i = 0; i < state->max_resource_count; ++i)
        {
            resource_lookup* lookup = &state->lookups[i];
            if (!lookup->r)
            {
                // Grab a handler for the resource type, if there is one
                bresource_handler* handler = &state->handlers[info->type];
                
                // Allocate memory for the resource
                lookup->r = ballocate(handler->size, MEMORY_TAG_RESOURCE);
                if (!lookup->r)
                {
                    BERROR("Resource handler failed to allocate resource. Null/0 will be returned");
                    return 0;
                }

                // Add an entry to the bst for this node
                bt_node_value v;
                v.u32 = i;
                bt_node* new_node = u64_bst_insert(state->lookup_tree, name, v);
                // Save as root if this is the first resource. Otherwise it'll be part of the tree automatically
                if (!state->lookup_tree)
                    state->lookup_tree = new_node;

                // Setup the resource
                lookup->r->name = name;
                lookup->r->type = info->type;
                lookup->r->state = BRESOURCE_STATE_UNINITIALIZED;
                lookup->r->generation = INVALID_ID;
                lookup->r->tag_count = 0;
                lookup->r->tags = 0;
                lookup->reference_count = 0;
                // Only allow auto-release for resources which aren't hot-reloadable
                lookup->auto_release = handler->handle_hot_reload == 0;

                // Make the actual request
                b8 result = handler->request(handler, lookup->r, info);
                if (result)
                {
                    // Increment reference count
                    lookup->reference_count++;

                    // Return a pointer to the resource, even if it's not yet ready
                    return lookup->r;
                }

                // This means the handler failed
                BERROR("Resource handler failed to fulfill request. See logs for details. Null/0 will be returned");
                return 0;
            }
        }

        BFATAL("Max configured resource count of %u has been exceeded and all slots are full. Increase this count in configuration", state->max_resource_count);
        return 0;
    }
}

void bresource_system_release(struct bresource_system_state* state, bname resource_name)
{
    BASSERT_MSG(state, "bresource_system_release requires a valid pointer to state");

    bresource_system_release_internal(state, resource_name, false);
}

void bresource_system_register_for_hot_reload(struct bresource_system_state* state, bresource* resource, u32 file_watch_id)
{
    // TODO: also handle unload and removing from this lookup when destroying

    // Add an entry to the bst for this node
    u32 lookup_index = INVALID_ID;
    const bt_node* node = u64_bst_find(state->lookup_tree, resource->name);
    if (node)
        lookup_index = node->value.u32;

    if (lookup_index != INVALID_ID)
    {
        bt_node_value v;
        v.u32 = lookup_index;
        bt_node* new_node = u64_bst_insert(state->file_watch_lookup, file_watch_id, v);
        // Save as root if this is the first resource. Otherwise it'll be part of the tree automatically
        if (!state->file_watch_lookup)
            state->file_watch_lookup = new_node;
    }
    else
    {
        BERROR("Failed to register resource for hot reload watch");
    }
}

b8 bresource_system_handler_register(struct bresource_system_state* state, bresource_type type, bresource_handler handler)
{
    if (!state)
    {
        BERROR("bresource_system_handler_register requires valid pointer to state");
        return false;
    }

    bresource_handler* h = &state->handlers[type];
    if (h->request || h->release)
    {
        BERROR("A handler already exists for this type");
        return false;
    }

    h->asset_system = state->asset_system;
    h->size = handler.size;
    h->request = handler.request;
    h->release = handler.release;
    h->handle_hot_reload = handler.handle_hot_reload;

    return true;
}

static void bresource_system_release_internal(struct bresource_system_state* state, bname resource_name, b8 force_release)
{
    BASSERT_MSG(state, "bresource_system_release requires a valid pointer to state");

    u32 lookup_index = INVALID_ID;
    const bt_node* node = u64_bst_find(state->lookup_tree, resource_name);
    if (node)
        lookup_index = node->value.u32;
    if (lookup_index != INVALID_ID)
    {
        // Valid entry found, decrement the reference count
        resource_lookup* lookup = &state->lookups[lookup_index];
        b8 do_release = false;
        if (force_release)
        {
            lookup->reference_count = 0;
            do_release = true;
        }
        else
        {
            lookup->reference_count--;
            do_release = lookup->auto_release && lookup->reference_count < 1;
        }
        if (do_release && lookup->r)
        {
            // Auto release set and criteria met, so call resource handler's 'release' function
            bresource_handler* handler = &state->handlers[lookup->r->type];
            if (!handler->release)
            {
                BTRACE("No release setup on handler for resource type %d, name='%s'", lookup->r->type, bname_string_get(lookup->r->name));
            }
            else
            {
                // Release the resource-specific data
                handler->release(handler, lookup->r);
            }

            // Release tags, if they exist
            if (lookup->r->tags)
            {
                BFREE_TYPE_CARRAY(lookup->r->tags, bname, lookup->r->tag_count);
                lookup->r->tags = 0;
            }

            // Free the resource structure itself
            bfree(lookup->r, handler->size, MEMORY_TAG_RESOURCE);

            // Ensure the lookup is invalidated
            lookup->r = 0;
            lookup->reference_count = 0;
            lookup->auto_release = false;

            // Remove the entry from the bst too
            bt_node* deleted = u64_bst_delete(state->lookup_tree, resource_name);
            if (!deleted)
                state->lookup_tree = 0;
        }
    }
    else
    {
        // Entry not found, nothing to do
        BWARN("bresource_system_release: Attempted to release resource '%s', which does not exist or is not already loaded. Nothing to do", bname_string_get(resource_name));
    }
}

static void on_asset_system_hot_reload(void* listener, basset* asset)
{
    bresource_system_state* state = (bresource_system_state*)listener;

    // Find the resource from a lookup table based on file_watch_id
    const bt_node* node = u64_bst_find(state->file_watch_lookup, asset->file_watch_id);
    u32 lookup_index = INVALID_ID;
    if (node)
        lookup_index = node->value.u32;

    if (lookup_index != INVALID_ID)
    {
        resource_lookup* lookup = &state->lookups[lookup_index];
        bresource* resource = lookup->r;

        // Increment the resource generation
        resource->generation++;
        
        // If the handler for this type handles hot-reloads, do it
        bresource_handler* handler = &state->handlers[resource->type];
        if (handler->handle_hot_reload)
            handler->handle_hot_reload(handler, resource, asset, asset->file_watch_id);

        // Fire off a message about the hot reload for anything that might be interested
        event_context evt = {0};
        evt.data.u32[0] = asset->file_watch_id; // Pass through the asset file watch id
        event_fire(EVENT_CODE_RESOURCE_HOT_RELOADED, resource, evt);
    }
    else
    {
        BWARN("Resource system was notified of a file watch update for a resource not being watched");
    }
}
