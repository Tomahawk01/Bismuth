#pragma once

#include "identifiers/identifier.h"
#include "defines.h"

#define INVALID_BHANDLE INVALID_ID_U64

typedef struct bhandle
{
    u32 handle_index;
    identifier unique_id;
} bhandle;

BAPI bhandle bhandle_create(u32 handle_index);
BAPI bhandle bhandle_create_with_identifier(u32 handle_index, identifier id);

BAPI bhandle bhandle_invalid(void);
BAPI b8 bhandle_is_invalid(bhandle handle);
BAPI void bhandle_invalidate(bhandle* handle);
