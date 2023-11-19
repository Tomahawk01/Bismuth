#include "render_view_system.h"

#include "containers/hashtable.h"
#include "core/frame_data.h"
#include "core/logger.h"
#include "core/bmemory.h"
#include "core/bstring.h"
#include "renderer/renderer_frontend.h"
#include "renderer/renderer_types.h"

typedef struct render_view_system_state
{
    hashtable lookup;
    void* table_block;
    u32 max_view_count;
    render_view** registered_views;
} render_view_system_state;

static render_view_system_state* state_ptr = 0;

b8 render_view_system_initialize(u64* memory_requirement, void* state, void* config)
{
    render_view_system_config* typed_config = (render_view_system_config*)config;
    if (typed_config->max_view_count == 0)
    {
        BFATAL("render_view_system_initialize - typed_config->max_view_count must be > 0");
        return false;
    }

    // Block of memory will contain state structure, then block for hashtable, then block for array
    u64 struct_requirement = sizeof(render_view_system_state);
    u64 hashtable_requirement = sizeof(u16) * typed_config->max_view_count;
    u64 array_requirement = sizeof(render_view*) * typed_config->max_view_count;
    *memory_requirement = struct_requirement + hashtable_requirement + array_requirement;

     if (!state)
        return true;

    state_ptr = state;
    state_ptr->max_view_count = typed_config->max_view_count;

    // Hashtable block is after the state. Already allocated, so just set the pointer
    u64 addr = (u64)state_ptr;
    state_ptr->table_block = (void*)((u64)addr + struct_requirement);

    // Array block is after hashtable
    state_ptr->registered_views = (void*)((u64)state_ptr->table_block + hashtable_requirement);

    // Create hashtable for view lookups
    hashtable_create(sizeof(u16), state_ptr->max_view_count, state_ptr->table_block, false, &state_ptr->lookup);
    // Fill hashtable with invalid ids
    u16 invalid_id = INVALID_ID_U16;
    hashtable_fill(&state_ptr->lookup, &invalid_id);

    // Make sure array is zeroed
    bzero_memory(state_ptr->registered_views, sizeof(render_view*) * state_ptr->max_view_count);

    return true;
}

void render_view_system_shutdown(void* state)
{
    // Destroy all views in the system
    for (u32 i = 0; i < state_ptr->max_view_count; ++i)
    {
        render_view* view = state_ptr->registered_views[i];
        if(view)
        {
            view->on_destroy(view);

            // Destroy its renderpasses
            for (u32 p = 0; p < view->renderpass_count; ++p)
                renderer_renderpass_destroy(&view->passes[p]);
        }
    }

    state_ptr = 0;
}

b8 render_view_system_register(render_view* view)
{
    if (!view)
    {
        BERROR("render_view_system_register requires a pointer to a valid view");
        return false;
    }

    if (!view->name || string_length(view->name) < 1)
    {
        BERROR("render_view_system_register: name is required");
        return false;
    }

    if (view->renderpass_count < 1)
    {
        BERROR("render_view_system_register - View must have at least one renderpass");
        return false;
    }

    u16 id = INVALID_ID_U16;
    // Make sure there is not already an entry with this name already registered
    hashtable_get(&state_ptr->lookup, view->name, &id);
    if (id != INVALID_ID_U16)
    {
        BERROR("render_view_system_register - A view named '%s' already exists. A new one will not be registered", view->name);
        return false;
    }

    // Find new id
    for (u32 i = 0; i < state_ptr->max_view_count; ++i)
    {
        if (!state_ptr->registered_views[i])
        {
            id = i;
            break;
        }
    }

    // Make sure a valid entry was found
    if (id == INVALID_ID_U16)
    {
        BERROR("render_view_system_register - No available space for a new view. Change system config to account for more");
        return false;
    }

    // Update hashtable entry
    hashtable_set(&state_ptr->lookup, view->name, &id);

    // Set array element's pointer
    state_ptr->registered_views[id] = view;

    // Call the on register
    if (!view->on_registered(view))
    {
        BERROR("Failed to register view '%s'", view->name);
        // bfree(view->passes, sizeof(renderpass*) * view->renderpass_count, MEMORY_TAG_ARRAY);
        // bzero_memory(&state_ptr->registered_views[id], sizeof(render_view));
        return false;
    }

    render_view_system_render_targets_regenerate(view);

    return true;
}

void render_view_system_on_window_resize(u32 width, u32 height)
{
    // Send to all views
    for (u32 i = 0; i < state_ptr->max_view_count; ++i)
    {
        if (state_ptr->registered_views[i])
            state_ptr->registered_views[i]->on_resize(state_ptr->registered_views[i], width, height);
    }
}

render_view* render_view_system_get(const char* name)
{
    if (state_ptr)
    {
        u16 id = INVALID_ID_U16;
        hashtable_get(&state_ptr->lookup, name, &id);
        if (id != INVALID_ID_U16)
            return state_ptr->registered_views[id];
    }
    return 0;
}

b8 render_view_system_packet_build(const render_view* view, struct frame_data* p_frame_data, struct viewport* v, struct camera* c, void* data, struct render_view_packet* out_packet)
{
    if (view && out_packet)
        return view->on_packet_build(view, p_frame_data, v, c, data, out_packet);

    BERROR("render_view_system_packet_build requires valid pointers to a view and a packet");
    return false;
}

b8 render_view_system_on_render(const render_view* view, const render_view_packet* packet, u64 frame_number, u64 render_target_index, const struct frame_data* p_frame_data)
{
    if (view && packet)
        return view->on_render(view, packet, p_frame_data);

    BERROR("render_view_system_on_render requires valid pointer to data");
    return false;
}

void render_view_system_render_targets_regenerate(render_view* view)
{
    for (u64 r = 0; r < view->renderpass_count; ++r)
    {
        renderpass* pass = &view->passes[r];

        for (u8 i = 0; i < pass->render_target_count; ++i)
        {
            render_target* target = &pass->targets[i];
            // Destroy the old first if it exists
            renderer_render_target_destroy(target, false);

            for (u32 a = 0; a < target->attachment_count; ++a)
            {
                render_target_attachment* attachment = &target->attachments[a];
                if (attachment->source == RENDER_TARGET_ATTACHMENT_SOURCE_DEFAULT)
                {
                    if (attachment->type == RENDER_TARGET_ATTACHMENT_TYPE_COLOR)
                    {
                        attachment->texture = renderer_window_attachment_get(i);
                    }
                    else if (attachment->type == RENDER_TARGET_ATTACHMENT_TYPE_DEPTH)
                    {
                        attachment->texture = renderer_depth_attachment_get(i);
                    }
                    else
                    {
                        BFATAL("Unsupported attachment type: 0x%x", attachment->type);
                        continue;
                    }
                }
                else if (attachment->source == RENDER_TARGET_ATTACHMENT_SOURCE_VIEW)
                {
                    if (!view->attachment_target_regenerate)
                    {
                        BFATAL("RENDER_TARGET_ATTACHMENT_SOURCE_VIEW configured for an attachment whose view does not support this operation");
                        continue;
                    }
                    else
                    {
                        if (!view->attachment_target_regenerate(view, r, attachment))
                            BERROR("View failed to regenerate attachment target for attachment type: 0x%x", attachment->type);
                    }
                }
            }

            // Create render target
            renderer_render_target_create(
                target->attachment_count,
                target->attachments,
                pass,
                target->attachments[0].texture->width,
                target->attachments[0].texture->height,
                &pass->targets[i]);
        }
    }
}
