#pragma once

#include "defines.h"
#include "identifiers/identifier.h"

#define INVALID_BHANDLE INVALID_ID_U64

typedef struct bhandle
{
    u32 handle_index;
    identifier unique_id;
} bhandle;

BAPI bhandle bhandle_create(u32 handle_index);
BAPI bhandle bhandle_create_with_identifier(u32 handle_index, identifier id);
/** @brief Creates and returns a handle based on the handle index provided, using the given u64 to create an identifier */
BAPI bhandle bhandle_create_with_u64_identifier(u32 handle_index, u64 uniqueid);

BAPI bhandle bhandle_invalid(void);
BAPI b8 bhandle_is_invalid(bhandle handle);
BAPI void bhandle_invalidate(bhandle* handle);
