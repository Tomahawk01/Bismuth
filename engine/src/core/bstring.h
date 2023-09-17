#pragma once

#include "defines.h"

BAPI u64 string_length(const char* str);

BAPI char* string_duplicate(const char* str);

// Case-sensitive string comparison. Return true if same, otherwise false
BAPI b8 strings_equal(const char* str0, const char* str1);

// Case-insensitive string comparison. Return true if same, otherwise false
BAPI b8 strings_equali(const char* str0, const char* str1);

// Performs string formatting to dest given format string and parameters
BAPI i32 string_format(char* dest, const char* format, ...);

/**
 * Performs variadic string formatting to dest given format string and va_list.
 * @param dest The destination for the formatted string.
 * @param format The string to be formatted.
 * @param va_list The variadic argument list.
 * @returns The size of the data written.
 */
BAPI i32 string_format_v(char* dest, const char* format, void* va_list);
