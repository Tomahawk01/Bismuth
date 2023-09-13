#pragma once

#include "defines.h"

BAPI u64 string_length(const char* str);

BAPI char* string_duplicate(const char* str);

// Case-sensitive string comparison.
BAPI b8 strings_equal(const char* str0, const char* str1);
