#include "bresource_system.h"
#include "debug/bassert.h"
#include "defines.h"
#include "bresources/bresource_types.h"
#include "logger.h"
// TODO: test, remove
#include "containers/stackarray.h"

typedef struct bresource_system_state
{
    bresource_handler handlers[BRESOURCE_TYPE_COUNT];
} bresource_system_state;

b8 bresource_system_initialize(u64* memory_requirement, struct bresource_system_state* state, const bresource_system_config* config)
{
    BASSERT_MSG(memory_requirement, "Valid pointer to memory_requirement is required");

    *memory_requirement = sizeof(bresource_system_state);

    if (!state)
        return true;

    // TODO: configure state, etc

    return true;
}

void bresource_system_shutdown(struct bresource_system_state* state)
{
    if (state)
    {
        // TODO: release resources, etc
    }
}

b8 bresource_system_request(struct bresource_system_state* state, bname name, bresource_type type, bresource_request_info info, bresource* out_resource)
{
    BASSERT_MSG(state && out_resource, "Valid pointers to state and out_resource are required");

    out_resource->name = name;
    out_resource->type = type;
    out_resource->state = BRESOURCE_STATE_UNINITIALIZED;
    out_resource->generation = INVALID_ID;
    out_resource->tag_count = 0;
    out_resource->tags = 0;

    bresource_handler* h = &state->handlers[type];
    if (!h->request)
    {
        BERROR("There is no handler setup for the asset type");
        return false;
    }

    b8 result = h->request(h, out_resource, info);
    if (result)
    {
        // TODO: Increment reference count
    }
    return result;
}

void bresource_system_release(struct bresource_system_state* state, bresource* resource)
{
    BASSERT_MSG(state && resource, "bresource_system_release requires valid pointers to state and resource");

    // TODO: Decrement reference count. If this reaches 0, release resources/unload, etc

    bresource_handler* h = &state->handlers[resource->type];
    if (!h->release)
    {
        BERROR("There is no handler setup for the asset type");
    }
    else
    {
        h->release(h, resource);
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

    h->request = handler.request;
    h->release = handler.release;

    return true;
}
