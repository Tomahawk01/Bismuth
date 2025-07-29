#pragma once

#include "defines.h"
#include "math/math_types.h"

BAPI u64 string_length(const char* str);
BAPI u32 string_utf8_length(const char* str);

BAPI u64 string_nlength(const char* str, u32 max_len);
BAPI u32 string_utf8_nlength(const char* str, u32 max_len);

BAPI b8 bytes_to_codepoint(const char* bytes, u32 offset, i32* out_codepoint, u8* out_advance);

BAPI b8 char_is_whitespace(char c);

BAPI b8 codepoint_is_whitespace(i32 codepoint);

BAPI char* string_duplicate(const char* str);

BAPI void string_free(const char* str);

BAPI i64 bstr_ncmp(const char* str0, const char* str1, u32 max_len);
BAPI i64 bstr_ncmpi(const char* str0, const char* str1, u32 max_len);

// Case-sensitive string comparison. Return true if same, otherwise false
BAPI b8 strings_equal(const char* str0, const char* str1);

// Case-insensitive string comparison. Return true if same, otherwise false
BAPI b8 strings_equali(const char* str0, const char* str1);

// Case-sensitive string comparison, where comparison stops at max_len
BAPI b8 strings_nequal(const char* str0, const char* str1, u32 max_len);

// Case-insensitive string comparison, where comparison stops at max_len
BAPI b8 strings_nequali(const char* str0, const char* str1, u32 max_len);

// Performs string formatting against the given format string and parameters
BAPI char* string_format(const char* format, ...);

// Performs variadic string formatting against the given format string and va_list
BAPI char* string_format_v(const char* format, void* va_list);

// Performs string formatting to dest given format string and parameters. This version of the function is unsafe. Use string_format() instead
BDEPRECATED("This version of string format is legacy, and unsafe. Use string_format() instead")
BAPI i32 string_format_unsafe(char* dest, const char* format, ...);

/**
 * Performs variadic string formatting to dest given format string and va_list.
 * @deprecated
 * @note This version of the function is unsafe. Use string_format() instead
 * @param dest The destination for the formatted string.
 * @param format The string to be formatted.
 * @param va_list The variadic argument list.
 * @returns The size of the data written.
 */
BDEPRECATED("This version of string format variadic is legacy, and unsafe. Use string_format_v() instead")
BAPI i32 string_format_v_unsafe(char* dest, const char* format, void* va_list);

/**
 * @brief Empties the provided string by setting the first character to 0
 * 
 * @param str The string to be emptied.
 * @return A pointer to str. 
 */
BAPI char* string_empty(char* str);

BAPI char* string_copy(char* dest, const char* source);

BAPI char* string_ncopy(char* dest, const char* source, u32 max_len);

BAPI char* string_trim(char* str);

BAPI void string_mid(char* dest, const char* source, i32 start, i32 length);

/**
 * @brief Returns the index of the first occurance of c in str; otherwise -1
 * 
 * @param str The string to be scanned.
 * @param c The character to search for.
 * @return The index of the first occurance of c; otherwise -1 if not found. 
 */
BAPI i32 string_index_of(const char* str, char c);

/**
 * @brief Returns the index of the last occurance of c in str; otherwise -1
 *
 * @param str The string to be scanned.
 * @param c The character to search for.
 * @return The index of the last occurance of c; otherwise -1 if not found.
 */
BAPI i32 string_last_index_of(const char* str, char c);

/**
 * @brief Returns the index of the first occurance of str_1 in str_0; otherwise -1
 *
 * @param str_0 The string to be scanned
 * @param str_1 The substring to search for
 * @return The index of the first occurance of str_1; otherwise -1 if not found
 */
BAPI i32 string_index_of_str(const char* str_0, const char* str_1);

/**
 * @brief Indicates if str_0 starts with str_1. Case-sensitive.
 *
 * @param str_0 The string to be scanned.
 * @param str_1 The substring to search for.
 * @return True if str_0 starts with str_1; otherwise false.
 */
BAPI b8 string_starts_with(const char* str_0, const char* str_1);

/**
 * @brief Indicates if str_0 starts with str_1. Case-insensitive.
 *
 * @param str_0 The string to be scanned.
 * @param str_1 The substring to search for.
 * @return True if str_0 starts with str_1; otherwise false.
 */
BAPI b8 string_starts_withi(const char* str_0, const char* str_1);

BAPI void string_insert_char_at(char* dest, const char* src, u32 pos, char c);
BAPI void string_insert_str_at(char* dest, const char* src, u32 pos, const char* str);
BAPI void string_remove_at(char* dest, const char* src, u32 pos, u32 length);

/**
 * @brief Attempts to parse a 4x4 matrix from the provided string.
 *
 * @param str The string to parse from. Should be space delimited. (i.e "1.0 1.0 ... 1.0")
 * @param out_mat A pointer to the matrix to write to.
 * @return True if parsed successfully; otherwise false.
 */
BAPI b8 string_to_mat4(const char* str, mat4* out_mat);

/**
 * @brief Creates a string representation of the provided matrix.
 *
 * @param m The matrix to convert to string.
 * @return The string representation of the matrix.
 */
BAPI const char* mat4_to_string(mat4 m);

/**
 * @brief Attempts to parse a vector from the provided string.
 * 
 * @param str The string to parse from. Should be space-delimited. (i.e. "1.0 2.0 3.0 4.0")
 * @param out_vector A pointer to the vector to write to.
 * @return True if parsed successfully; otherwise false.
 */
BAPI b8 string_to_vec4(const char* str, vec4* out_vector);

/**
 * @brief Creates a string representation of the provided vector.
 *
 * @param v The vector to convert to string.
 * @return The string representation of the vector.
 */
BAPI const char* vec4_to_string(vec4 v);

/**
 * @brief Attempts to parse a vector from the provided string.
 * 
 * @param str The string to parse from. Should be space-delimited. (i.e. "1.0 2.0 3.0")
 * @param out_vector A pointer to the vector to write to.
 * @return True if parsed successfully; otherwise false.
 */
BAPI b8 string_to_vec3(const char* str, vec3* out_vector);

/**
 * @brief Creates a string representation of the provided vector.
 *
 * @param v The vector to convert to string.
 * @return The string representation of the vector.
 */
BAPI const char* vec3_to_string(vec3 v);

/**
 * @brief Attempts to parse a vector from the provided string.
 * 
 * @param str The string to parse from. Should be space-delimited. (i.e. "1.0 2.0")
 * @param out_vector A pointer to the vector to write to.
 * @return True if parsed successfully; otherwise false.
 */
BAPI b8 string_to_vec2(const char* str, vec2* out_vector);

/**
 * @brief Creates a string representation of the provided vector.
 *
 * @param v The vector to convert to string.
 * @return The string representation of the vector.
 */
BAPI const char* vec2_to_string(vec2 v);

/**
 * @brief Attempts to parse a 32-bit floating-point number from the provided string.
 * 
 * @param str The string to parse from. Should *not* be postfixed with 'f'.
 * @param f A pointer to the float to write to.
 * @return True if parsed successfully; otherwise false.
 */
BAPI b8 string_to_f32(const char* str, f32* f);

/**
 * @brief Creates a string representation of the provided float.
 *
 * @param f The float to convert to string.
 * @return The string representation of the provided float.
 */
BAPI const char* f32_to_string(f32 f);

/**
 * @brief Attempts to parse a 64-bit floating-point number from the provided string.
 * 
 * @param str The string to parse from.
 * @param f A pointer to the float to write to.
 * @return True if parsed successfully; otherwise false.
 */
BAPI b8 string_to_f64(const char* str, f64* f);

/**
 * @brief Creates a string representation of the provided 64-bit float.
 *
 * @param f The 64-bit float to convert to string.
 * @return The string representation of the provided 64-bit float.
 */
BAPI const char* f64_to_string(f64 f);

/**
 * @brief Attempts to parse an 8-bit signed integer from the provided string.
 * 
 * @param str The string to parse from.
 * @param i A pointer to the int to write to.
 * @return True if parsed successfully; otherwise false.
 */
BAPI b8 string_to_i8(const char* str, i8* i);

/**
 * @brief Creates a string representation of the provided integer.
 *
 * @param i The integer to create a string from.
 * @return The string representation of the provided integer.
 */
BAPI const char* i8_to_string(i8 i);

/**
 * @brief Attempts to parse a 16-bit signed integer from the provided string.
 * 
 * @param str The string to parse from.
 * @param i A pointer to the int to write to.
 * @return True if parsed successfully; otherwise false.
 */
BAPI b8 string_to_i16(const char* str, i16* i);

/**
 * @brief Creates a string representation of the provided integer.
 *
 * @param i The integer to create a string from.
 * @return The string representation of the provided integer.
 */
BAPI const char* i16_to_string(i16 i);

/**
 * @brief Attempts to parse a 32-bit signed integer from the provided string.
 * 
 * @param str The string to parse from.
 * @param i A pointer to the int to write to.
 * @return True if parsed successfully; otherwise false.
 */
BAPI b8 string_to_i32(const char* str, i32* i);

/**
 * @brief Creates a string representation of the provided integer.
 *
 * @param i The integer to create a string from.
 * @return The string representation of the provided integer.
 */
BAPI const char* i32_to_string(i32 i);

/**
 * @brief Attempts to parse a 64-bit signed integer from the provided string.
 * 
 * @param str The string to parse from.
 * @param i A pointer to the int to write to.
 * @return True if parsed successfully; otherwise false.
 */
BAPI b8 string_to_i64(const char* str, i64* i);

/**
 * @brief Creates a string representation of the provided integer.
 *
 * @param i The integer to create a string from.
 * @return The string representation of the provided integer.
 */
BAPI const char* i64_to_string(i64 i);

/**
 * @brief Attempts to parse an 8-bit unsigned integer from the provided string.
 * 
 * @param str The string to parse from.
 * @param u A pointer to the int to write to.
 * @return True if parsed successfully; otherwise false.
 */
BAPI b8 string_to_u8(const char* str, u8* u);

/**
 * @brief Creates a string representation of the provided integer.
 *
 * @param u The integer to create a string from.
 * @return The string representation of the provided integer.
 */
BAPI const char* u8_to_string(u8 u);

/**
 * @brief Attempts to parse a 16-bit unsigned integer from the provided string.
 * 
 * @param str The string to parse from.
 * @param u A pointer to the int to write to.
 * @return True if parsed successfully; otherwise false.
 */
BAPI b8 string_to_u16(const char* str, u16* u);

/**
 * @brief Creates a string representation of the provided integer.
 *
 * @param u The integer to create a string from.
 * @return The string representation of the provided integer.
 */
BAPI const char* u16_to_string(u16 u);

/**
 * @brief Attempts to parse a 32-bit unsigned integer from the provided string.
 * 
 * @param str The string to parse from.
 * @param u A pointer to the int to write to.
 * @return True if parsed successfully; otherwise false.
 */
BAPI b8 string_to_u32(const char* str, u32* u);

/**
 * @brief Creates a string representation of the provided integer.
 *
 * @param u The integer to create a string from.
 * @return The string representation of the provided integer.
 */
BAPI const char* u32_to_string(u32 u);

/**
 * @brief Attempts to parse a 64-bit unsigned integer from the provided string.
 * 
 * @param str The string to parse from.
 * @param u A pointer to the int to write to.
 * @return True if parsed successfully; otherwise false.
 */
BAPI b8 string_to_u64(const char* str, u64* u);

/**
 * @brief Creates a string representation of the provided integer.
 *
 * @param u The integer to create a string from.
 * @return The string representation of the provided integer.
 */
BAPI const char* u64_to_string(u64 u);

/**
 * @brief Attempts to parse a boolean from the provided string.
 * "true" or "1" are considered true; anything else is false.
 * 
 * @param str The string to parse from. "true" or "1" are considered true; anything else is false.
 * @param b A pointer to the boolean to write to.
 * @return True if parsed successfully; otherwise false.
 */
BAPI b8 string_to_bool(const char* str, b8* b);

/**
 * @brief Creates a string representation of the provided boolean, i.e. "false" for false/0 and "true" for everything else.
 *
 * @param b The boolean to create a string from.
 * @return The string representation of the provided boolean.
 */
BAPI const char* bool_to_string(b8 b);

/**
 * @brief Splits the given string by the delimiter provided and stores in the
 * provided darray. Optionally trims each entry.
 * NOTE: A string allocation occurs for each entry, and MUST be freed by the caller
 *
 * @param str The string to be split
 * @param delimiter The character to split by
 * @param str_darray A pointer to a darray of char arrays to hold the entries. NOTE: must be a darray
 * @param trim_entries Trims each entry if true
 * @param include_empty Indicates if empty entries should be included
 * @return The number of entries yielded by the split operation
 */
BAPI u32 string_split(const char* str, char delimiter, char*** str_darray, b8 trim_entries, b8 include_empty);

/**
 * @brief Cleans up string allocations in str_darray, but does not free the darray itself.
 *
 * @param str_darray The darray to be cleaned up.
 */
BAPI void string_cleanup_split_darray(char** str_darray);

BAPI u32 string_nsplit(const char* str, char delimiter, u32 max_count, char** str_array, b8 trim_entries, b8 include_empty);

BAPI void string_cleanup_split_array(char** str_array, u32 max_count);

BAPI void string_append_string(char* dest, const char* source, const char* append);

BAPI void string_append_int(char* dest, const char* source, i64 i);

BAPI void string_append_float(char* dest, const char* source, f32 f);

BAPI void string_append_bool(char* dest, const char* source, b8 b);

BAPI void string_append_char(char* dest, const char* source, char c);

BAPI char* string_join(const char** strings, u32 count, char delimiter);

BAPI void string_directory_from_path(char* dest, const char* path);

BAPI void string_filename_from_path(char* dest, const char* path);

BAPI void string_filename_no_extension_from_path(char* dest, const char* path);

/**
 * @brief Attempts to get the file extension from the given path. Allocates a new string which should be freed
 *
 * @param path The full path to extract from.
 * @param include_dot Indicates if the '.' should be included in the output.
 * @returns The extension on success; otherwise 0.
 */
BAPI const char* string_extension_from_path(const char* path, b8 include_dot);

/**
 * @brief Attempts to extract an array length from a given string. Ex: a string of sampler2D[4] will return True and set out_length to 4.
 * @param str The string to examine.
 * @param out_length A pointer to hold the length, if extracted successfully.
 * @returns True if an array length was found and parsed; otherwise false.
 */
BAPI b8 string_parse_array_length(const char* str, u32* out_length);

BAPI b8 string_line_get(const char* source_str, u16 max_line_length, u32 start_from, char** out_buffer, u32* out_line_length, u8* out_addl_advance);

/** Indicates if provided codepoint is lower-case. Regular ASCII and western European high-ascii characters only */
BAPI b8 codepoint_is_lower(i32 codepoint);
/** Indicates if provided codepoint is upper-case. Regular ASCII and western European high-ascii characters only */
BAPI b8 codepoint_is_upper(i32 codepoint);
/** Indicates if provided codepoint is alpha-numeric. Regular ASCII and western European high-ascii characters only */
BAPI b8 codepoint_is_alpha(i32 codepoint);
/** Indicates if provided codepoint is numeric. Regular ASCII and western European high-ascii characters only */
BAPI b8 codepoint_is_numeric(i32 codepoint);
/** Indicates if the given codepoint is considered to be a space. Includes ' ', \f \r \n \t and \v */
BAPI b8 codepoint_is_space(i32 codepoint);

/** Converts string in-place to uppercase. Regular ASCII and western European high-ascii characters only */
BAPI void string_to_lower(char* str);
/** Converts string in-place to uppercase. Regular ASCII and western European high-ascii characters only */
BAPI void string_to_upper(char* str);

// -----------------------------
// ========== BString ==========
// -----------------------------

typedef struct bstring
{
    // Current length of the string in bytes
    u32 length;
    // Amount of currently allocated memory
    u32 allocated;
    // TRaw string data
    char* data;
} bstring;

BAPI void bstring_create(bstring* out_string);
BAPI void bstring_from_cstring(const char* source, bstring* out_string);
BAPI void bstring_destroy(bstring* string);

BAPI u32 bstring_length(const bstring* string);
BAPI u32 bstring_utf8_length(const bstring* string);
BAPI u64 string_nlength(const char* str, u32 max_len);
BAPI u32 string_utf8_nlength(const char* str, u32 max_len);

BAPI void bstring_append_str(bstring* string, const char* s);
BAPI void bstring_append_bstring(bstring* string, const bstring* other);
