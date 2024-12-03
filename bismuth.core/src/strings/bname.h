#pragma once

#include "defines.h"

/** @brief Represents an invalid bname, which is essentially used to represent "no name" */
#define INVALID_BNAME 0

/** @brief A bname is a string hash made for quick comparisons versus traditional string comparisons */
typedef u64 bname;

BAPI bname bname_create(const char* str);
BAPI const char* bname_string_get(bname name);
