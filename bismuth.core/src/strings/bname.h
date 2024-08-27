#pragma once

#include "defines.h"

/** @brief Represents an invalid bname, which is essentially used to represent "no name" */
#define INVALID_BNAME 0

typedef u64 bname;

BAPI bname bname_create(const char* str);
BAPI const char* bname_string_get(bname name);
