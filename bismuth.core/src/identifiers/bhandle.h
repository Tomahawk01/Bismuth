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

/**
 * @brief bhandle16 - a 16-bit implementation of the bhandle that uses one u16 for the
 * index and a second for the uniqueid. This results in a much smaller handle, although
 * coming with a limitation of a maximum of 65534 values (65535 is INVALID_ID_U16) as a
 * maximum array size for anything this references
 */
typedef struct bhandle16
{
    /** @brief Index into a resource table. Considered invalid if == INVALID_ID_U16 */
    u16 handle_index;
    /**
     * @brief A generation used to indicate if a handle is stale. Typically incremented
     * when a resource is updated. Considered invalid if == INVALID_ID_U16
     */
    u16 generation;
} bhandle16;

/** @brief Creates and returns a handle with the given handle index. Also creates a new unique identifier */
BAPI bhandle16 bhandle16_create(u16 handle_index);

/** @brief Creates and returns a handle based on the handle index provided, using the given u64 to create an identifier */
BAPI bhandle16 bhandle16_create_with_u16_generation(u16 handle_index, u16 generation);

/** @brief Creates and returns an invalid handle */
BAPI bhandle16 bhandle16_invalid(void);

/** @brief Indicates if the provided handle is valid */
BAPI b8 bhandle16_is_valid(bhandle16 handle);

/** @brief Indicates if the provided handle is invalid */
BAPI b8 bhandle16_is_invalid(bhandle16 handle);

/** @brief Updates the provided handle, incrementing the generation */
BAPI void bhandle16_update(bhandle16* handle);

/** @brief Invalidates the provided handle */
BAPI void bhandle16_invalidate(bhandle16* handle);

/** @brief Indicates if the handle is stale/outdated) */
BAPI b8 bhandle16_is_stale(bhandle16 handle, u16 uniqueid);

/** @brief Indicates if the handle is pristine (i.e. not stale/outdated) */
BAPI b8 bhandle16_is_pristine(bhandle16 handle, u16 uniqueid);
