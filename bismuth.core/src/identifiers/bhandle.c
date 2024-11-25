#include "identifiers/bhandle.h"

#include "defines.h"
#include "identifiers/identifier.h"

bhandle bhandle_create(u32 handle_index)
{
    bhandle out_handle = {0};
    out_handle.handle_index = handle_index;
    out_handle.unique_id = identifier_create();
    return out_handle;
}

bhandle bhandle_create_with_identifier(u32 handle_index, identifier id)
{
    bhandle out_handle = {0};
    out_handle.handle_index = handle_index;
    out_handle.unique_id = id;
    return out_handle;
}

bhandle bhandle_create_with_u64_identifier(u32 handle_index, u64 uniqueid)
{
    bhandle out_handle = {0};
    out_handle.handle_index = handle_index;
    out_handle.unique_id.uniqueid = uniqueid;
    return out_handle;
}

bhandle bhandle_invalid(void)
{
    bhandle out_handle = {0};
    out_handle.handle_index = INVALID_ID;
    out_handle.unique_id.uniqueid = INVALID_ID_U64;
    return out_handle;
}

b8 bhandle_is_invalid(bhandle handle)
{
    return handle.handle_index == INVALID_ID || handle.unique_id.uniqueid == INVALID_ID_U64;
}

void bhandle_invalidate(bhandle* handle)
{
    if (handle)
    {
        handle->handle_index = INVALID_ID;
        handle->unique_id.uniqueid = INVALID_ID_U64;
    }
}
