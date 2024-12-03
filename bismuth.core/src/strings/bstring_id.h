#pragma once

#include "defines.h"

/**
 * @brief A bstring_id is a string hash made for quick comparisons versus traditional string comparisons
 *
 * A bname's hash is generated from a case-sensitive version of a string.
 * The original string may be looked up and retrieved at any time using bstring_id_string_get()
 */
typedef u64 bstring_id;

/** @brief Represents an invalid bstring_id, which is essentially used to represent a null or empty string */
#define INVALID_BSTRING_ID 0

BAPI bstring_id bstring_id_create(const char* str);
BAPI const char* bstring_id_string_get(bstring_id stringid);
