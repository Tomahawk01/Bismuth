#include "bresource_system.h"

#include "containers/u64_bst.h"
#include "core/engine.h"
#include "debug/bassert.h"
#include "defines.h"
#include "bresources/handlers/bresource_handler_texture.h"
#include "bresources/bresource_types.h"
#include "logger.h"
#include "memory/bmemory.h"
#include "strings/bname.h"

struct asset_system_state;

typedef struct resource_lookup
{
    // The resource itself, owned by this lookup
    bresource r;
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
} bresource_system_state;

static void bresource_system_release_internal(struct bresource_system_state* state, bresource* resource, b8 force_release);

b8 bresource_system_initialize(u64* memory_requirement, struct bresource_system_state* state, const bresource_system_config* config)
{
    BASSERT_MSG(memory_requirement, "Valid pointer to memory_requirement is required");

    *memory_requirement = sizeof(bresource_system_state);

    if (!state)
        return true;

    state->max_resource_count = config->max_resource_count;
    state->lookups = ballocate(sizeof(resource_lookup) * state->max_resource_count, MEMORY_TAG_ARRAY);
    state->lookup_tree = 0;

    state->asset_system = engine_systems_get()->asset_state;

    // Register known handler types
    bresource_handler texture_handler = {0};
    texture_handler.release = bresource_handler_texture_release;
    texture_handler.request = bresource_handler_texture_request;
    if (!bresource_system_handler_register(state, BRESOURCE_TYPE_TEXTURE, texture_handler))
    {
        BERROR("Failed to register texture resource handler");
        return false;
    }

    BINFO("Resource system (new) initialized");
    return true;
}

void bresource_system_shutdown(struct bresource_system_state* state)
{
    if (state)
    {
        for (u32 i = 0; i < state->max_resource_count; ++i)
        {
            if (state->lookups[i].r.name != INVALID_BNAME)
                bresource_system_release_internal(state, &state->lookups[i].r, true);
        }

        // Destroy the bst
        u64_bst_cleanup(state->lookup_tree);

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

    if (lookup_index != INVALID_ID)
    {
        resource_lookup* lookup = &state->lookups[lookup_index];
        lookup->reference_count++;
        // Immediately issue the callback if setup
        if (info->user_callback)
            info->user_callback(&lookup->r, info->listener_inst);

        // Return a pointer to the resource
        return &lookup->r;
    }
    else
    {
        // Resource doesn't exist. Create a new one and its lookup
        // Look for an empty slot
        for (u32 i = 0; i < state->max_resource_count; ++i)
        {
            resource_lookup* lookup = &state->lookups[lookup_index];
            if (lookup->r.name == INVALID_BNAME)
            {
                // Add an entry to the bst for this node
                bt_node_value v;
                v.u32 = i;
                bt_node* new_node = u64_bst_insert(state->lookup_tree, name, v);
                // Save as root if this is the first resource. Otherwise it'll be part of the tree automatically
                if (!state->lookup_tree)
                    state->lookup_tree = new_node;

                // Setup the resource
                lookup->r.name = name;
                lookup->r.type = info->type;
                lookup->r.state = BRESOURCE_STATE_UNINITIALIZED;
                lookup->r.generation = INVALID_ID;
                lookup->r.tag_count = 0;
                lookup->r.tags = 0;
                lookup->reference_count = 0;

                // Grab a handler for the resource type, if there is one
                bresource_handler* h = &state->handlers[info->type];
                if (!h->request)
                {
                    BERROR("There is no handler setup for the asset type. Null/0 will be returned");
                    return 0;
                }

                // Make the actual request
                b8 result = h->request(h, &lookup->r, info);
                if (result)
                {
                    // Increment reference count
                    lookup->reference_count++;

                    // Return a pointer to the resource, even if it's not yet ready
                    return &lookup->r;
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

void bresource_system_release(struct bresource_system_state* state, bresource* resource)
{
    BASSERT_MSG(state && resource, "bresource_system_release requires valid pointers to state and resource");

    bresource_system_release_internal(state, resource, false);
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
    h->request = handler.request;
    h->release = handler.release;

    return true;
}

static void bresource_system_release_internal(struct bresource_system_state* state, bresource* resource, b8 force_release)
{
    BASSERT_MSG(state && resource, "bresource_system_release requires valid pointers to state and resource");

    u32 lookup_index = INVALID_ID;
    const bt_node* node = u64_bst_find(state->lookup_tree, resource->name);
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
        if (do_release)
        {
            // Auto release set and criteria met, so call resource handler's 'release' function
            bresource_handler* handler = &state->handlers[lookup->r.type];
            if (!handler->release)
            {
                BTRACE("No release setup on handler for resource type %d, name='%s'", lookup->r.type, bname_string_get(lookup->r.name));
            }
            else
            {
                // Release the resource-specific data
                handler->release(handler, &lookup->r);
            }

            // Ensure the lookup is invalidated
            lookup->r.type = BRESOURCE_TYPE_UNKNOWN;
            lookup->r.name = INVALID_BNAME;
            lookup->r.generation = INVALID_ID;
            lookup->r.state = BRESOURCE_STATE_UNINITIALIZED;
            lookup->r.tags = 0;
            lookup->r.tag_count = 0;
            lookup->reference_count = 0;
            lookup->auto_release = false;
        }
    }
    else
    {
        // Entry not found, nothing to do
        BWARN("bresource_system_release: Attempted to release resource '%s', which does not exist or is not already loaded. Nothing to do", bname_string_get(resource->name));
    }
}
