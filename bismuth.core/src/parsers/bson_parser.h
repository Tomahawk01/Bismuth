#ifndef _BSON_H_
#define _BSON_H_

#include "defines.h"

typedef enum bson_token_type
{
    BSON_TOKEN_TYPE_UNKNOWN,
    BSON_TOKEN_TYPE_WHITESPACE,
    BSON_TOKEN_TYPE_COMMENT,
    BSON_TOKEN_TYPE_IDENTIFIER,
    BSON_TOKEN_TYPE_OPERATOR_EQUAL,
    BSON_TOKEN_TYPE_OPERATOR_MINUS,
    BSON_TOKEN_TYPE_OPERATOR_PLUS,
    BSON_TOKEN_TYPE_OPERATOR_SLASH,
    BSON_TOKEN_TYPE_OPERATOR_ASTERISK,
    BSON_TOKEN_TYPE_OPERATOR_DOT,
    BSON_TOKEN_TYPE_STRING_LITERAL,
    BSON_TOKEN_TYPE_NUMERIC_LITERAL,
    BSON_TOKEN_TYPE_BOOLEAN,
    BSON_TOKEN_TYPE_CURLY_BRACE_OPEN,
    BSON_TOKEN_TYPE_CURLY_BRACE_CLOSE,
    BSON_TOKEN_TYPE_BRACKET_OPEN,
    BSON_TOKEN_TYPE_BRACKET_CLOSE,
    BSON_TOKEN_TYPE_NEWLINE,
    BSON_TOKEN_TYPE_EOF
} bson_token_type;

typedef struct bson_token
{
    bson_token_type type;
    u32 start;
    u32 end;
#ifdef BISMUTH_DEBUG
    const char* content;
#endif
} bson_token;

typedef struct bson_parser
{
    const char* file_content;
    u32 position;

    // darray
    bson_token* tokens;
} bson_parser;

typedef enum bson_property_type
{
    BSON_PROPERTY_TYPE_UNKNOWN,
    BSON_PROPERTY_TYPE_INT,
    BSON_PROPERTY_TYPE_FLOAT,
    BSON_PROPERTY_TYPE_STRING,
    BSON_PROPERTY_TYPE_OBJECT,
    BSON_PROPERTY_TYPE_ARRAY,
    BSON_PROPERTY_TYPE_BOOLEAN,
} bson_property_type;

struct bson_property;

typedef enum bson_object_type
{
    BSON_OBJECT_TYPE_OBJECT,
    BSON_OBJECT_TYPE_ARRAY
} bson_object_type;

// An object which can contain properties. Objects represent both "object" types as well as "array" types.
// These types are identical with one key difference: An object's properties are required to be named, whereas array properties are unnamed
typedef struct bson_object
{
    bson_object_type type;
    // darray
    struct bson_property* properties;
} bson_object;

// An alias to represent bson arrays, which are really just bson_objects that contain properties without names
typedef bson_object bson_array;

// Represents a property value for a bson property
typedef union bson_property_value
{
    // Signed 64-bit int value
    i64 i;
    // 32-bit float value
    f32 f;
    // String value
    const char* s;
    // Array or object value
    bson_object o;
    // Boolean value
    b8 b;
} bson_property_value;

// Represents a singe property for a bson object or array
typedef struct bson_property
{
    // The type of property
    bson_property_type type;
    // The name of the property. If this belongs to an array, it should be null
    char* name;
    // The property value
    bson_property_value value;
} bson_property;

// Represents a hierarchy of bson objects
typedef struct bson_tree
{
    // The root object, which always must exist
    bson_object root;
} bson_tree;

/**
 * @brief Creates a bson parser.
 * NOTE: It is recommended to use the bson_tree_from_string() and bson_tree_to_string() functions
 * instead of invoking this manually, as these also handle cleanup of the parser object.
 *
 * @param out_parser A pointer to hold the newly-created parser
 * @returns True on success; otherwise false
 */
BAPI b8 bson_parser_create(bson_parser* out_parser);

/**
 * @brief Destroys the provided parser
 *
 * @param parser A pointer to the parser to be destroyed
 */
BAPI void bson_parser_destroy(bson_parser* parser);

/**
 * @brief Uses the given parser to tokenize the provided source string.
 * NOTE: It is recommended to use the bson_tree_from_string() function instead.
 *
 * @param parser A pointer to the parser to use. Required. Must be a valid parser.
 * @param source A constant pointer to the source string to tokenize.
 * @returns True on success; otherwise false.
 */
BAPI b8 bson_parser_tokenize(bson_parser* parser, const char* source);

/**
 * @brief Uses the given parser to build a bson_tree using the tokens previously
 * parsed. This means that bson_parser_tokenize() must have been called and completed
 * successfully for this function to work. It is recommended to use bson_tree_from_string() instead.
 *
 * @param parser A pointer to the parser to use. Required. Must be a valid parser that has already had bson_parser_tokenize() called against it successfully.
 * @param out_tree A pointer to hold the generated bson_tree. Required.
 * @returns True on success; otherwise false.
 */
BAPI b8 bson_parser_parse(bson_parser* parser, bson_tree* out_tree);

/**
 * @brief Takes the provided source and tokenizes, then parses it in order to create a tree of bson_objects.
 *
 * @param source A pointer to the source string to be tokenized and parsed. Required.
 * @param out_tree A pointer to hold the generated bson_tree. Required.
 * @returns True on success; otherwise false.
 */
BAPI b8 bson_tree_from_string(const char* source, bson_tree* out_tree);

/**
 * Takes the provided bson_tree and writes it to a bson-formatted string.
 *
 * @param tree A pointer to the bson_tree to use. Required.
 * @returns A string on success; otherwise false.
 */
BAPI const char* bson_tree_to_string(bson_tree* tree);

/**
 * @brief Performs cleanup operations on the given tree, freeing memory and resources held by it.
 *
 * @param tree A pointer to the tree to cleanup. Required.
 */
BAPI void bson_tree_cleanup(bson_tree* tree);

/**
 * @brief Adds an unnamed signed 64-bit integer value to the provided array.
 *
 * @param array A pointer to the array to add the property to.
 * @param value The value to be set.
 * @return True on success; otherwise false.
 */
BAPI b8 bson_array_value_add_int(bson_array* array, i64 value);

/**
 * @brief Adds an unnamed floating-point value to the provided array.
 *
 * @param array A pointer to the array to add the property to.
 * @param value The value to be set.
 * @return True on success; otherwise false.
 */
BAPI b8 bson_array_value_add_float(bson_array* array, f32 value);

/**
 * @brief Adds an unnamed boolean value to the provided array.
 *
 * @param array A pointer to the array to add the property to.
 * @param value The value to be set.
 * @return True on success; otherwise false.
 */
BAPI b8 bson_array_value_add_boolean(bson_array* array, b8 value);

/**
 * @brief Adds an unnamed string value to the provided array.
 *
 * @param array A pointer to the array to add the property to.
 * @param value The value to be set. Required. Must not be null.
 * @return True on success; otherwise false.
 */
BAPI b8 bson_array_value_add_string(bson_array* array, const char* value);

/**
 * @brief Adds an unnamed object value to the provided array.
 *
 * @param array A pointer to the array to add the property to.
 * @param value The value to be set.
 * @return True on success; otherwise false.
 */
BAPI b8 bson_array_value_add_object(bson_array* array, bson_object value);

/**
 * @brief Adds an unnamed empty object value to the provided array.
 *
 * @param array A pointer to the array to add the property to.
 * @return True on success; otherwise false.
 */
BAPI b8 bson_array_value_add_object_empty(bson_array* array);

/**
 * @brief Adds an unnamed array value to the provided array.
 *
 * @param array A pointer to the array to add the property to.
 * @param value The value to be set.
 * @return True on success; otherwise false.
 */
BAPI b8 bson_array_value_add_array(bson_array* array, bson_array value);

/**
 * @brief Adds an unnamed empty array value to the provided array.
 *
 * @param array A pointer to the array to add the property to.
 * @return True on success; otherwise false.
 */
BAPI b8 bson_array_value_add_array_empty(bson_array* array);

// Object functions.

/**
 * @brief Adds a named signed 64-bit integer value to the provided object.
 *
 * @param object A pointer to the object to add the property to.
 * @param name A constant pointer to the name to be used. Required.
 * @param value The value to be set.
 * @return True on success; otherwise false.
 */
BAPI b8 bson_object_value_add_int(bson_object* object, const char* name, i64 value);

/**
 * @brief Adds a named floating-point value to the provided object.
 *
 * @param object A pointer to the object to add the property to.
 * @param name A constant pointer to the name to be used. Required.
 * @param value The value to be set.
 * @return True on success; otherwise false.
 */
BAPI b8 bson_object_value_add_float(bson_object* object, const char* name, f32 value);

/**
 * @brief Adds a named boolean value to the provided object.
 *
 * @param object A pointer to the object to add the property to.
 * @param name A constant pointer to the name to be used. Required.
 * @param value The value to be set.
 * @return True on success; otherwise false.
 */
BAPI b8 bson_object_value_add_boolean(bson_object* object, const char* name, b8 value);

/**
 * @brief Adds a named string value to the provided object.
 *
 * @param object A pointer to the object to add the property to.
 * @param name A constant pointer to the name to be used. Required.
 * @param value The value to be set. Required. Must not be null.
 * @return True on success; otherwise false.
 */
BAPI b8 bson_object_value_add_string(bson_object* object, const char* name, const char* value);

/**
 * @brief Adds a named object value to the provided object.
 *
 * @param object A pointer to the object to add the property to.
 * @param name A constant pointer to the name to be used. Required.
 * @param value The value to be set.
 * @return True on success; otherwise false.
 */
BAPI b8 bson_object_value_add_object(bson_object* object, const char* name, bson_object value);

/**
 * @brief Adds a named empty object value to the provided object.
 *
 * @param object A pointer to the object to add the property to.
 * @param name A constant pointer to the name to be used. Required.
 * @return True on success; otherwise false.
 */
BAPI b8 bson_object_value_add_object_empty(bson_object* object, const char* name);

/**
 * @brief Adds a named array value to the provided object.
 *
 * @param object A pointer to the object to add the property to.
 * @param name A constant pointer to the name to be used. Required.
 * @param value The value to be set.
 * @return True on success; otherwise false.
 */
BAPI b8 bson_object_value_add_array(bson_object* object, const char* name, bson_array value);

/**
 * @brief Adds a named empty array value to the provided object.
 *
 * @param object A pointer to the object to add the property to.
 * @param name A constant pointer to the name to be used. Required.
 * @return True on success; otherwise false.
 */
BAPI b8 bson_object_value_add_array_empty(bson_object* object, const char* name);

/**
 * @brief Obtains the length of the given array.
 *
 * @param array The array to retrieve the length of.
 * @param count A pointer to hold the array element count,
 * @returns True on success; otherwise false.
 */
BAPI b8 bson_array_element_count_get(bson_array* array, u32* out_count);

/**
 * @brief Obtains the element type at the provided index of the given array. Fails if out of range.
 *
 * @param array The array to retrieve the type from.
 * @param index The index into the array to check the type of. Must be in range.
 * @param count A pointer to hold the array element type,
 * @returns True on success; otherwise false.
 */
BAPI b8 bson_array_element_type_at(bson_array* array, u32 index, bson_property_type* out_type);

/**
 * @brief Attempts to retrieve the array element's value at the provided index as a signed integer. Fails if out of range.
 * or on type mismatch.
 *
 * @param array A constant pointer to the array to search. Required.
 * @param index The array index to search for. Required.
 * @param out_value A pointer to hold the object property's value.
 * @return True on success; otherwise false.
 */
BAPI b8 bson_array_element_value_get_int(const bson_array* array, u32 index, i64* out_value);

/**
 * @brief Attempts to retrieve the array element's value at the provided index as a floating-point number. Fails if out of range.
 * or on type mismatch.
 *
 * @param array A constant pointer to the array to search. Required.
 * @param index The array index to search for. Required.
 * @param out_value A pointer to hold the object property's value.
 * @return True on success; otherwise false.
 */
BAPI b8 bson_array_element_value_get_float(const bson_array* array, u32 index, f32* out_value);

/**
 * @brief Attempts to retrieve the array element's value at the provided index as a boolean. Fails if out of range.
 * or on type mismatch.
 *
 * @param array A constant pointer to the array to search. Required.
 * @param index The array index to search for. Required.
 * @param out_value A pointer to hold the object property's value.
 * @return True on success; otherwise false.
 */
BAPI b8 bson_array_element_value_get_bool(const bson_array* array, u32 index, b8* out_value);

/**
 * @brief Attempts to retrieve the array element's value at the provided index as a string. Fails if out of range.
 * or on type mismatch.
 *
 * @param array A constant pointer to the array to search. Required.
 * @param index The array index to search for. Required.
 * @param out_value A pointer to hold the object property's value.
 * @return True on success; otherwise false.
 */
BAPI b8 bson_array_element_value_get_string(const bson_array* array, u32 index, const char** out_value);

/**
 * @brief Attempts to retrieve the array element's value at the provided index as an object. Fails if out of range.
 * or on type mismatch.
 *
 * @param array A constant pointer to the array to search. Required.
 * @param index The array index to search for. Required.
 * @param out_value A pointer to hold the object property's value.
 * @return True on success; otherwise false.
 */
BAPI b8 bson_array_element_value_get_object(const bson_array* array, u32 index, bson_object* out_value);

/**
 * Obtains the type of the property with the given name. Fails if the name is not found.
 *
 * @param object The object to retrieve the type from.
 * @param name The name of the property to retrieve.
 * @param out_type A pointer to hold the object property type,
 * @returns True on success; otherwise false.
 */
BAPI b8 bson_object_property_type_get(const bson_object* object, const char* name, bson_property_type* out_type);

/**
 * Obtains the count of properties of the given object.
 *
 * @param object The object to retrieve the property count of.
 * @param out_count A pointer to hold the object property count,
 * @returns True on success; otherwise false.
 */
BAPI b8 bson_object_property_count_get(const bson_object* object, u32* out_count);

/**
 * @brief Attempts to retrieve the given object's property value by name as a signed integer. Fails if not found
 * or on type mismatch.
 *
 * @param A constant pointer to the object to search. Required.
 * @param name The property name to search for. Required.
 * @param out_value A pointer to hold the object property's value.
 * @return True on success; otherwise false.
 */
BAPI b8 bson_object_property_value_get_int(const bson_object* object, const char* name, i64* out_value);

/**
 * @brief Attempts to retrieve the given object's property value by name as a floating-point number. Fails if not found
 * or on type mismatch.
 *
 * @param A constant pointer to the object to search. Required.
 * @param name The property name to search for. Required.
 * @param out_value A pointer to hold the object property's value.
 * @return True on success; otherwise false.
 */
BAPI b8 bson_object_property_value_get_float(const bson_object* object, const char* name, f32* out_value);

/**
 * @brief Attempts to retrieve the given object's property value by name as a boolean. Fails if not found
 * or on type mismatch.
 *
 * @param A constant pointer to the object to search. Required.
 * @param name The property name to search for. Required.
 * @param out_value A pointer to hold the object property's value.
 * @return True on success; otherwise false.
 */
BAPI b8 bson_object_property_value_get_bool(const bson_object* object, const char* name, b8* out_value);

/**
 * @brief Attempts to retrieve the given object's property value by name as a string. Fails if not found
 * or on type mismatch. NOTE: This function always allocates new memory, so the string should be released afterward.
 *
 * @param A constant pointer to the object to search. Required.
 * @param name The property name to search for. Required.
 * @param out_value A pointer to hold the object property's value.
 * @return True on success; otherwise false.
 */
BAPI b8 bson_object_property_value_get_string(const bson_object* object, const char* name, const char** out_value);

/**
 * @brief Attempts to retrieve the given object's property value by name as an object. Fails if not found
 * or on type mismatch.
 *
 * @param A constant pointer to the object to search. Required.
 * @param name The property name to search for. Required.
 * @param out_value A pointer to hold the object property's value.
 * @return True on success; otherwise false.
 */
BAPI b8 bson_object_property_value_get_object(const bson_object* object, const char* name, bson_object* out_value);

/**
 * Creates and returns a new property of the object type.
 * @param name The name of the property. Pass 0 if later adding to an array.
 * @returns The newly created object property.
 */
BAPI bson_property bson_object_property_create(const char* name);

/**
 * Creates and returns a new property of the array type.
 * @param name The name of the property. Pass 0 if later adding to an array.
 * @returns The newly created array property.
 */
BAPI bson_property bson_array_property_create(const char* name);

/** @brief Creates and returns a new bson object */
BAPI bson_object bson_object_create(void);

/** @brief Creates and returns a new bson array */
BAPI bson_array bson_array_create(void);

#endif
