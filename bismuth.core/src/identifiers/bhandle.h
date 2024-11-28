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

/** @brief Indicates if the provided handle is valid */
BAPI b8 bhandle_is_valid(bhandle handle);

/** @brief Indicates if the provided handle is invalid */
BAPI b8 bhandle_is_invalid(bhandle handle);

/** @brief Invalidates the provided handle */
BAPI void bhandle_invalidate(bhandle* handle);

/** @brief Indicates if the handle is stale/outdated) */
BAPI b8 bhandle_is_stale(bhandle handle, u64 uniqueid);

/** @brief Indicates if the handle is pristine (i.e. not stale/outdated) */
BAPI b8 bhandle_is_pristine(bhandle handle, u64 uniqueid);
