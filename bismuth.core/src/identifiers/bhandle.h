#ifndef _B_HANDLE_H_
#define _B_HANDLE_H_

#include "identifiers/identifier.h"
#include "defines.h"

#define INVALID_B_HANDLE INVALID_ID_U64

typedef struct b_handle
{
    u32 handle_index;
    identifier unique_id;
} b_handle;

BAPI b_handle b_handle_create(u32 handle_index);
BAPI b_handle b_handle_create_with_identifier(u32 handle_index, identifier id);

BAPI b_handle b_handle_invalid(void);
BAPI b8 b_handle_is_invalid(b_handle handle);
BAPI void b_handle_invalidate(b_handle* handle);

#endif