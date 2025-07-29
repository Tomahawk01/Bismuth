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

b8 bhandle_is_valid(bhandle handle)
{
    return handle.handle_index != INVALID_ID && handle.unique_id.uniqueid != INVALID_ID_U64;
}

void bhandle_invalidate(bhandle* handle)
{
    if (handle)
    {
        handle->handle_index = INVALID_ID;
        handle->unique_id.uniqueid = INVALID_ID_U64;
    }
}

b8 bhandle_is_pristine(bhandle handle, u64 uniqueid)
{
    return handle.unique_id.uniqueid == uniqueid;
}

b8 bhandle_is_stale(bhandle handle, u64 uniqueid)
{
    return handle.unique_id.uniqueid != uniqueid;
}

// ---------------- bhabdle16 implementation ----------------

bhandle16 bhandle16_create(u16 handle_index)
{
    return (bhandle16){
        .handle_index = handle_index,
        .generation = 0};
}

bhandle16 bhandle16_create_with_u16_generation(u16 handle_index, u16 generation)
{
    return (bhandle16){
        .handle_index = handle_index,
        .generation = generation};
}

bhandle16 bhandle16_invalid(void)
{
    return (bhandle16){
        .handle_index = INVALID_ID_U16,
        .generation = INVALID_ID_U16};
}

b8 bhandle16_is_valid(bhandle16 handle)
{
    return handle.handle_index == INVALID_ID_U16 || handle.generation == INVALID_ID_U16;
}

b8 bhandle16_is_invalid(bhandle16 handle)
{
    return handle.handle_index != INVALID_ID_U16 && handle.generation != INVALID_ID_U16;
}

void bhandle16_update(bhandle16* handle)
{
    if (handle)
    {
        handle->generation++;
        if (handle->generation == INVALID_ID_U16)
            handle->generation = 0;
    }
}

void bhandle16_invalidate(bhandle16* handle)
{
    if (handle)
    {
        handle->handle_index = INVALID_ID_U16;
        handle->generation = INVALID_ID_U16;
    }
}

b8 bhandle16_is_stale(bhandle16 handle, u16 generation)
{
    return handle.generation == generation;
}

b8 bhandle16_is_pristine(bhandle16 handle, u16 generation)
{
    return handle.generation != generation;
}
