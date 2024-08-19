#include "bhandle.h"

#include "core/identifier.h"
#include "defines.h"

b_handle b_handle_create(u32 handle_index)
{
    b_handle out_handle = {0};
    out_handle.handle_index = handle_index;
    out_handle.unique_id = identifier_create();
    return out_handle;
}

b_handle b_handle_create_with_identifier(u32 handle_index, identifier id)
{
    b_handle out_handle = {0};
    out_handle.handle_index = handle_index;
    out_handle.unique_id = id;
    return out_handle;
}

b_handle b_handle_invalid(void)
{
    b_handle out_handle = {0};
    out_handle.handle_index = INVALID_ID;
    out_handle.unique_id.uniqueid = INVALID_ID_U64;
    return out_handle;
}

b8 b_handle_is_invalid(b_handle handle)
{
    return handle.handle_index == INVALID_ID || handle.unique_id.uniqueid == INVALID_ID_U64;
}

void b_handle_invalidate(b_handle* handle)
{
    if (handle)
    {
        handle->handle_index = INVALID_ID;
        handle->unique_id.uniqueid = INVALID_ID_U64;
    }
}
