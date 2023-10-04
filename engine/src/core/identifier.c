#include "identifier.h"
#include "containers/darray.h"
#include "core/logger.h"

static void** owners = 0;

u32 identifier_aquire_new_id(void* owner)
{
    if (!owners)
    {
        owners = darray_reserve(void*, 100);
        darray_push(owners, INVALID_ID_U64);
    }
    u64 length = darray_length(owners);
    for (u64 i = 0; i < length; ++i)
    {
        if (owners[i] == 0)
        {
            owners[i] = owner;
            return i;
        }
    }

    darray_push(owners, owner);
    length = darray_length(owners);
    return length - 1;
}

void identifier_release_id(u32 id)
{
    if (!owners)
    {
        BERROR("identifier_release_id called before initialization. identifier_aquire_new_id should have been called first. Nothing was done");
        return;
    }

    u64 length = darray_length(owners);
    if (id > length)
    {
        BERROR("identifier_release_id: id '%u' out of range (max=%llu). Nothing was done", id, length);
        return;
    }

    owners[id] = 0;
}
