#include "bson_parser.h"

#include "containers/darray.h"
#include "containers/stack.h"
#include "debug/bassert.h"
#include "logger.h"
#include "memory/bmemory.h"
#include "strings/bname.h"
#include "strings/bstring.h"
#include "strings/bstring_id.h"

const char* bson_property_type_to_string(bson_property_type type)
{
    switch (type)
    {
    default:
    case BSON_PROPERTY_TYPE_UNKNOWN:
        return "unknown";
    case BSON_PROPERTY_TYPE_INT:
        return "int";
    case BSON_PROPERTY_TYPE_FLOAT:
        return "float";
    case BSON_PROPERTY_TYPE_STRING:
        return "string";
    case BSON_PROPERTY_TYPE_OBJECT:
        return "object";
    case BSON_PROPERTY_TYPE_ARRAY:
        return "array";
    case BSON_PROPERTY_TYPE_BOOLEAN:
        return "boolean";
    }
}

b8 bson_parser_create(bson_parser* out_parser)
{
    if (!out_parser)
    {
        BERROR("bson_parser_create requires valid pointer to out_parser");
        return false;
    }

    out_parser->position = 0;
    out_parser->tokens = darray_create(bson_token);
    out_parser->file_content = 0;

    return true;
}

void bson_parser_destroy(bson_parser* parser)
{
    if (parser)
    {
        if (parser->file_content)
        {
            string_free((char*)parser->file_content);
            parser->file_content = 0;
        }
        if (parser->tokens)
        {
            darray_destroy(parser->tokens);
            parser->tokens = 0;
        }
        parser->position = 0;
    }
}

typedef enum bson_tokenize_mode
{
    BSON_TOKENIZE_MODE_UNKNOWN,
    BSON_TOKENIZE_MODE_DEFINING_IDENTIFIER,
    BSON_TOKENIZE_MODE_WHITESPACE,
    BSON_TOKENIZE_MODE_STRING_LITERAL,
    BSON_TOKENIZE_MODE_NUMERIC_LITERAL,
    BSON_TOKENIZE_MODE_BOOLEAN,
    BSON_TOKENIZE_MODE_OPERATOR
} bson_tokenize_mode;

// Resets both the current token type and the tokenize mode to unknown
static void reset_current_token_and_mode(bson_token* current_token, bson_tokenize_mode* mode)
{
    current_token->type = BSON_TOKEN_TYPE_UNKNOWN;
    current_token->start = 0;
    current_token->end = 0;
#ifdef BISMUTH_DEBUG
    current_token->content = 0;
#endif

    *mode = BSON_TOKENIZE_MODE_UNKNOWN;
}

#ifdef BISMUTH_DEBUG
static void _populate_token_content(bson_token* t, const char* source)
{
    BASSERT_MSG(t->start <= t->end, "Token start comes after token end!");
    char buffer[512] = {0};
    BASSERT_MSG((t->end - t->start) < 512, "token won't fit in buffer");
    string_mid(buffer, source, t->start, t->end - t->start);
    t->content = string_duplicate(buffer);
}
#define POPULATE_TOKEN_CONTENT(t, source) _populate_token_content(t, source)
#else
// No-op
#define POPULATE_TOKEN_CONTENT(t, source)
#endif

// Pushes the current token, if not of unknown type
static void push_token(bson_token* t, bson_parser* parser)
{
    if (t->type != BSON_TOKEN_TYPE_UNKNOWN && (t->end - t->start > 0))
    {
        POPULATE_TOKEN_CONTENT(t, parser->file_content);
        darray_push(parser->tokens, *t);
    }
}

b8 bson_parser_tokenize(bson_parser* parser, const char* source)
{
    if (!parser)
    {
        BERROR("bson_parser_tokenize requires valid pointer to out_parser!");
        return false;
    }
    if (!source)
    {
        BERROR("bson_parser_tokenize requires valid pointer to source!");
        return false;
    }

    if (parser->file_content)
        string_free((char*)parser->file_content);
    parser->file_content = string_duplicate(source);

    // Ensure the parser's tokens array is empty
    darray_clear(parser->tokens);

    u32 char_length = string_length(source);

    bson_tokenize_mode mode = BSON_TOKENIZE_MODE_DEFINING_IDENTIFIER;
    bson_token current_token = {0};
    // The previous codepoint
    i32 prev_codepoint = 0;
    // The codepoint from 2 iterations ago
    i32 prev_codepoint2 = 0;

    b8 eof_reached = false;

    // Take the length in chars and get the correct codepoint from it
    i32 codepoint = 0;
    for (u32 c = 0; c < char_length; prev_codepoint2 = prev_codepoint, prev_codepoint = codepoint)
    {
        if (eof_reached)
            break;

        codepoint = source[c];
        // How many bytes to advance
        u8 advance = 1;
        // NOTE: UTF-8 codepoint handling
        if (!bytes_to_codepoint(source, c, &codepoint, &advance))
        {
            BWARN("Invalid UTF-8 found in string, using unknown codepoint of -1");
            codepoint = -1;
        }

        if (mode == BSON_TOKENIZE_MODE_STRING_LITERAL)
        {
            // Handle string literal parsing.
            // End the string if only if the previous codepoint was NOT a backslash OR
            // previous codepoint was a backslash AND the one before that was also a backslash.
            // I.e. it needs to be confirmed that the backslash is NOT already escaped and that the quote is also not escaped.
            if (codepoint == '"' && (prev_codepoint != '\\' || prev_codepoint2 == '\\'))
            {
                // Terminate the string, push the token onto the array, and revert modes
                push_token(&current_token, parser);
                reset_current_token_and_mode(&current_token, &mode);
            }
            else
            {
                // Handle other characters as part of the string
                current_token.end += advance;
            }
            // TODO: Handle other escape sequences like \t, \n, etc.

            // At this point, this codepoint has been handled so continue early
            c += advance;
            continue;
        }

        // Not part of a string, identifier, numeric, etc., so try to figure out what to do next
        switch (codepoint)
        {
            case '\n':
            {
                push_token(&current_token, parser);

                // Just create a new token and insert it
                bson_token newline_token = {BSON_TOKEN_TYPE_NEWLINE, c, c + advance};

                push_token(&newline_token, parser);

                reset_current_token_and_mode(&current_token, &mode);
            } break;
            case '\t':
            case '\r':
            case ' ':
            {
                if (mode == BSON_TOKENIZE_MODE_WHITESPACE)
                {
                    // Tack it onto the whitespace
                    current_token.end++;
                }
                else
                {
                    // Before switching to whitespace mode, push the current token
                    push_token(&current_token, parser);
                    mode = BSON_TOKENIZE_MODE_WHITESPACE;
                    current_token.type = BSON_TOKEN_TYPE_WHITESPACE;
                    current_token.start = c;
                    current_token.end = c + advance;
                }
            } break;
            case '{':
            {
                push_token(&current_token, parser);

                // Create and push a new token for this
                bson_token open_brace_token = {BSON_TOKEN_TYPE_CURLY_BRACE_OPEN, c, c + advance};
                push_token(&open_brace_token, parser);

                reset_current_token_and_mode(&current_token, &mode);
            } break;
            case '}':
            {
                push_token(&current_token, parser);

                // Create and push a new token for this
                bson_token close_brace_token = {BSON_TOKEN_TYPE_CURLY_BRACE_CLOSE, c, c + advance};
                push_token(&close_brace_token, parser);

                reset_current_token_and_mode(&current_token, &mode);
            } break;
            case '[':
            {
                push_token(&current_token, parser);

                // Create and push a new token for this
                bson_token open_bracket_token = {BSON_TOKEN_TYPE_BRACKET_OPEN, c, c + advance};
                push_token(&open_bracket_token, parser);

                reset_current_token_and_mode(&current_token, &mode);
            } break;
            case ']':
            {
                push_token(&current_token, parser);

                // Create and push a new token for this
                bson_token close_bracket_token = {BSON_TOKEN_TYPE_BRACKET_CLOSE, c, c + advance};
                push_token(&close_bracket_token, parser);

                reset_current_token_and_mode(&current_token, &mode);
            } break;
            case '"':
            {
                push_token(&current_token, parser);

                reset_current_token_and_mode(&current_token, &mode);

                // Change to string parsing mode
                mode = BSON_TOKENIZE_MODE_STRING_LITERAL;
                current_token.type = BSON_TOKEN_TYPE_STRING_LITERAL;
                current_token.start = c + advance;
                current_token.end = c + advance;
            } break;
            case '0':
            case '1':
            case '2':
            case '3':
            case '4':
            case '5':
            case '6':
            case '7':
            case '8':
            case '9':
            {
                if (mode == BSON_TOKENIZE_MODE_NUMERIC_LITERAL)
                {
                    current_token.end++;
                }
                else
                {
                    // Push the existing token
                    push_token(&current_token, parser);

                    // Switch to numeric parsing mode
                    mode = BSON_TOKENIZE_MODE_NUMERIC_LITERAL;
                    current_token.type = BSON_TOKEN_TYPE_NUMERIC_LITERAL;
                    current_token.start = c;
                    current_token.end = c + advance;
                }
            } break;
            case '-':
            {
                push_token(&current_token, parser);

                // Create and push a new token for this
                bson_token minus_token = {BSON_TOKEN_TYPE_OPERATOR_MINUS, c, c + advance};
                push_token(&minus_token, parser);

                reset_current_token_and_mode(&current_token, &mode);
            } break;
            case '+':
            {
                push_token(&current_token, parser);

                // Create and push a new token for this
                bson_token plus_token = {BSON_TOKEN_TYPE_OPERATOR_PLUS, c, c + advance};
                push_token(&plus_token, parser);

                reset_current_token_and_mode(&current_token, &mode);
            } break;
            case '/':
            {
                push_token(&current_token, parser);
                reset_current_token_and_mode(&current_token, &mode);

                // Look ahead and see if another slash follows. If so, the rest of the line is a comment.
                // Skip forward until a newline is found.
                if (source[c + 1] == '/')
                {
                    i32 cm = c + 2;
                    char ch = source[cm];
                    while (ch != '\n' && ch != '\0')
                    {
                        cm++;
                        ch = source[cm];
                    }
                    if (cm > 0)
                    {
                        // Skip to one char before the newline so the newline gets processed.
                        // This is done because the comment shouldn't be tokenized, but should instead be ignored.
                        c = cm;
                    }
                    continue;
                }
                else
                {
                    // Otherwise it should be treated as a slash operator
                    bson_token slash_token = {BSON_TOKEN_TYPE_OPERATOR_SLASH, c, c + advance};
                    push_token(&slash_token, parser);
                }

                reset_current_token_and_mode(&current_token, &mode);
            } break;
            case '*':
            {
                push_token(&current_token, parser);

                // Create and push a new token for this
                bson_token asterisk_token = {BSON_TOKEN_TYPE_OPERATOR_ASTERISK, c, c + advance};
                push_token(&asterisk_token, parser);

                reset_current_token_and_mode(&current_token, &mode);
            } break;
            case '=':
            {
                push_token(&current_token, parser);

                // Create and push a new token for this
                bson_token equal_token = {BSON_TOKEN_TYPE_OPERATOR_EQUAL, c, c + advance};
                push_token(&equal_token, parser);

                reset_current_token_and_mode(&current_token, &mode);
            } break;
            case '.':
            {
                push_token(&current_token, parser);

                // Create and push a new token for this
                bson_token dot_token = {BSON_TOKEN_TYPE_OPERATOR_DOT, c, c + advance};
                push_token(&dot_token, parser);

                reset_current_token_and_mode(&current_token, &mode);
            } break;
            case '\0':
            {
                // Reached the end of the file
                push_token(&current_token, parser);

                // Create and push a new token for this
                bson_token eof_token = {BSON_TOKEN_TYPE_EOF, c, c + advance};
                push_token(&eof_token, parser);

                reset_current_token_and_mode(&current_token, &mode);

                eof_reached = true;
            } break;

            default:
            {
                // Identifiers may be made up of upper/lowercase a-z, underscores and numbers (number cannot be the first character of an identifier).
                // NOTE: Number cases are handled above as numeric literals, and will be combined into identifiers if there are identifiers without whitespace next to numerics
                if ((codepoint >= 'A' && codepoint <= 'z') || codepoint == '_')
                {
                    if (mode == BSON_TOKENIZE_MODE_DEFINING_IDENTIFIER)
                    {
                        // Start a new identifier token
                        if (current_token.type == BSON_TOKEN_TYPE_UNKNOWN)
                        {
                            current_token.type = BSON_TOKEN_TYPE_IDENTIFIER;
                            current_token.start = c;
                            current_token.end = c;
                        }
                        // Tack onto the existing identifier
                        current_token.end += advance;
                    }
                    else
                    {
                        // Check first to see if it's possibly a boolean definition
                        const char* str = source + c;
                        u8 bool_advance = 0;
                        if (strings_nequali(str, "true", 4))
                            bool_advance = 4;
                        else if (strings_nequali(str, "false", 5))
                            bool_advance = 5;

                        if (bool_advance)
                        {
                            push_token(&current_token, parser);

                            // Create and push boolean token
                            bson_token bool_token = {BSON_TOKEN_TYPE_BOOLEAN, c, c + bool_advance};
                            push_token(&bool_token, parser);

                            reset_current_token_and_mode(&current_token, &mode);

                            // Move forward by the size of the token
                            advance = bool_advance;
                        }
                        else
                        {
                            // Treat as the start of an identifier definition
                            // Push the existing token
                            push_token(&current_token, parser);

                            // Switch to identifier parsing mode
                            mode = BSON_TOKENIZE_MODE_DEFINING_IDENTIFIER;
                            current_token.type = BSON_TOKEN_TYPE_IDENTIFIER;
                            current_token.start = c;
                            current_token.end = c + advance;
                        }
                    }
                }
                else
                {
                    // If any other character is come across here that isn't part of a string, it's unknown what should happen here.
                    // Throw an error regarding this and boot if this is the case.
                    BERROR("Unexpected character '%c' at position %u. Tokenization failed", codepoint, c + advance);
                    // Clear the tokens array, as there is nothing that can be done with them in this case
                    darray_clear(parser->tokens);
                    return false;
                }

            } break;
        }

        // Now advance c
        c += advance;
    }
    push_token(&current_token, parser);
    // Create and push a new token for this
    bson_token eof_token = {BSON_TOKEN_TYPE_EOF, char_length, char_length + 1};
    push_token(&eof_token, parser);

    return true;
}

#define NEXT_TOKEN()                            \
    {                                           \
        index++;                                \
        current_token = &parser->tokens[index]; \
    }

static b8 ensure_identifier(b8 expect_identifier, bson_token* current_token, const char* token_string)
{
    if (expect_identifier)
    {
        BERROR("Expected identifier, instead found '%s'. Position: %u", token_string, current_token->start);
        return false;
    }

    return  true;
}

static bson_token* get_last_non_whitespace_token(bson_parser* parser, u32 current_index)
{
    if (current_index == 0)
        return 0;

    bson_token* t = &parser->tokens[current_index - 1];
    while (current_index > 0 && t && t->type == BSON_TOKEN_TYPE_WHITESPACE)
    {
        current_index--;
        t = &parser->tokens[current_index];
    }

    // This means that the last token available in the file is whitespace.
    // Impossible to return non-whitespace token
    if (t->type == BSON_TOKEN_TYPE_WHITESPACE)
        return 0;

    return t;
}

#define NUMERIC_LITERAL_STR_MAX_LENGTH 25

static char* string_from_bson_token(const char* file_content, const bson_token* token)
{
    i32 length = (i32)token->end - (i32)token->start;
    BASSERT_MSG(length > 0, "Token length should be at one!");
    char* mid = ballocate(sizeof(char) * (length + 1), MEMORY_TAG_STRING);
    string_mid(mid, file_content, token->start, length);
    mid[length] = 0;

    return mid;
}

b8 bson_parser_parse(bson_parser* parser, bson_tree* out_tree)
{
    if (!parser)
    {
        BERROR("bson_parser_parse requires a valid pointer to a parser");
        return false;
    }
    if (!out_tree)
    {
        BERROR("bson_parser_parse requires a valid pointer to a tree");
        return false;
    }

    if (!parser->tokens)
    {
        BERROR("Cannot parse an empty set of tokens");
        return false;
    }

    bson_token* current_token = 0;

    stack scope;
    stack_create(&scope, sizeof(bson_object*));

    // The first thing expected is an identifier
    b8 expect_identifier = true;
    b8 expect_value = false;
    b8 expect_operator = false;
    b8 expect_numeric = false;

    char numeric_literal_str[NUMERIC_LITERAL_STR_MAX_LENGTH] = {0};
    u32 numeric_literal_str_pos = 0;
    i32 numeric_decimal_pos = -1;

    u32 index = 0;
    current_token = &parser->tokens[index];

    // Setup the tree
    out_tree->root = (bson_object){0};
    out_tree->root.type = BSON_OBJECT_TYPE_OBJECT;
    out_tree->root.properties = darray_create(bson_property);

    // Set it as the current object
    bson_object* current_object = &out_tree->root;
    if (!stack_push(&scope, &current_object))
    {
        BERROR("Failed to push base object onto stack");
        return false;
    }
    bson_property* current_property = 0;

    while (current_token && current_token->type != BSON_TOKEN_TYPE_EOF)
    {
        switch (current_token->type)
        {
            case BSON_TOKEN_TYPE_CURLY_BRACE_OPEN:
            {
                // TODO: may be needed to verify object starts at correct place
                // ENSURE_IDENTIFIER("{")
                // Starting a block
                bson_object new_obj = {0};
                new_obj.type = BSON_OBJECT_TYPE_OBJECT;
                new_obj.properties = darray_create(bson_property);

                if (current_object->type == BSON_OBJECT_TYPE_ARRAY)
                {
                    // Apply the value directly to a newly-created, non-named property that gets added to current_object
                    bson_property unnamed_array_prop = {0};
                    unnamed_array_prop.type = BSON_PROPERTY_TYPE_OBJECT;
                    unnamed_array_prop.value.o = new_obj;
                    unnamed_array_prop.name = INVALID_BSTRING_ID;
                    // Add the array property to the current object
                    darray_push(current_object->properties, unnamed_array_prop);
                    // The current object is now new_obj
                    u32 prop_length = darray_length(current_object->properties);
                    current_object = &current_object->properties[prop_length - 1].value.o;
                }
                else
                {
                    // The object becomes the value of the current property
                    current_property->value.o = new_obj;
                    // The current object is now new_obj
                    current_object = &current_property->value.o;

                    // This also means that the current property is being assigned an object as its value, so mark the property as type object
                    current_property->type = BSON_PROPERTY_TYPE_OBJECT;
                }

                // Add the newly-updated current_object to the stack.
                stack_push(&scope, &current_object);

                expect_identifier = true;
            } break;
            case BSON_TOKEN_TYPE_CURLY_BRACE_CLOSE:
            {
                // TODO: may be needed to verify object ends at correct place
                // ENSURE_IDENTIFIER("}")
                // Ending a block
                bson_object* popped_obj = 0;
                if (!stack_pop(&scope, &popped_obj))
                {
                    BERROR("Failed to pop from scope stack");
                    return false;
                }

                // Peek the next object on the stack and make it the current object
                if (!stack_peek(&scope, &current_object))
                {
                    BERROR("Failed to peek scope stack");
                    return false;
                }

                expect_value = current_object->type == BSON_OBJECT_TYPE_ARRAY;
            } break;
            case BSON_TOKEN_TYPE_BRACKET_OPEN:
            {
                // TODO: may be needed to verify array starts at correct place
                // ENSURE_IDENTIFIER("[")

                // Starting an array
                bson_object new_arr = {0};
                new_arr.type = BSON_OBJECT_TYPE_ARRAY;
                new_arr.properties = darray_create(bson_property);

                if (current_object->type == BSON_OBJECT_TYPE_ARRAY)
                {
                    // Apply the value directly to a newly-created, non-named property that gets added to current_object
                    bson_property unnamed_array_prop = {0};
                    unnamed_array_prop.type = BSON_PROPERTY_TYPE_ARRAY;
                    unnamed_array_prop.value.o = new_arr;
                    unnamed_array_prop.name = INVALID_BSTRING_ID;
                    // Add the property to the current object
                    darray_push(current_object->properties, unnamed_array_prop);
                    // The current object is now new_arr. This will always be the first entry in that array
                    u32 prop_length = darray_length(current_object->properties);
                    current_object = &current_object->properties[prop_length - 1].value.o;
                }
                else
                {
                    // The object becomes the value of the current property
                    current_property->value.o = new_arr;
                    // The current object is now new_obj
                    current_object = &current_property->value.o;

                    // This also means that the current property is being assigned an array as its value, so mark the property as type array
                    current_property->type = BSON_PROPERTY_TYPE_ARRAY;
                }

                // Add the object to the stack
                stack_push(&scope, &current_object);

                expect_value = true;

            } break;
            case BSON_TOKEN_TYPE_BRACKET_CLOSE:
            {
                // TODO: may be needed to verify array ends at correct place
                // ENSURE_IDENTIFIER("]")
                // Ending an array
                bson_object* popped_obj = 0;
                if (!stack_pop(&scope, &popped_obj))
                {
                    BERROR("Failed to pop from scope stack");
                    return false;
                }

                // Peek the next object on the stack and make it the current object
                if (!stack_peek(&scope, &current_object))
                {
                    BERROR("Failed to peek scope stack");
                    return false;
                }

                expect_value = current_object->type == BSON_OBJECT_TYPE_ARRAY;
            } break;
            case BSON_TOKEN_TYPE_IDENTIFIER:
            {
                char buf[512] = {0};
                string_mid(buf, parser->file_content, current_token->start, current_token->end - current_token->start);
                if (!expect_identifier)
                {
                    BERROR("Unexpected identifier '%s' at position %u", buf, current_token->start);
                    return false;
                }
                // Start a new property
                bson_property prop = {0};
                prop.type = BSON_PROPERTY_TYPE_UNKNOWN;
                prop.name = bstring_id_create(buf);
#ifdef BISMUTH_DEBUG
                prop.name_str = string_duplicate(buf);
#endif

                // Push the new property and set the current property to it
                if (!current_object->properties)
                    current_object->properties = darray_create(bson_property);

                darray_push(current_object->properties, prop);
                u32 prop_count = darray_length(current_object->properties);
                current_property = &current_object->properties[prop_count - 1];

                // No longer expecting an identifier
                expect_identifier = false;
                expect_operator = true;
            } break;
            case BSON_TOKEN_TYPE_WHITESPACE:
            case BSON_TOKEN_TYPE_COMMENT:
            {
                NEXT_TOKEN();
                continue;
            }
            case BSON_TOKEN_TYPE_UNKNOWN:
            default:
            {
                BERROR("Unexpected and unknown token found. Parse failed");
                return false;
            }
            case BSON_TOKEN_TYPE_OPERATOR_EQUAL:
            {
                if (!ensure_identifier(expect_identifier, current_token, "="))
                    return false;

                // Previous token must be an identifier
                bson_token* t = get_last_non_whitespace_token(parser, index);
                if (!t)
                {
                    BERROR("Unexpected token before assignment operator. Position: %u", current_token->start);
                    return false;
                }
                else if (t->type != BSON_TOKEN_TYPE_IDENTIFIER)
                {
                    BERROR("Expected identifier before assignment operator. Position: %u", current_token->start);
                    return false;
                }

                expect_operator = false;

                // The next non-whitespace token should be a value of some kind
                expect_value = true;
            } break;
            case BSON_TOKEN_TYPE_OPERATOR_MINUS:
            {
                if (expect_numeric)
                {
                    BERROR("Already parsing a numeric, negatives are invalid within a numeric. Position: %u", current_token->start);
                    return false;
                }

                // If the next token is a numeric literal, process this as a numeric
                // NOTE: Negative is only valid for the first character of a numeric literal
                if (parser->tokens[index + 1].type == BSON_TOKEN_TYPE_NUMERIC_LITERAL ||
                    (parser->tokens[index + 1].type == BSON_TOKEN_TYPE_OPERATOR_DOT && parser->tokens[index + 2].type == BSON_TOKEN_TYPE_NUMERIC_LITERAL))
                {
                    // Start of a numeric process
                    expect_numeric = true;
                    bzero_memory(numeric_literal_str, sizeof(char) * NUMERIC_LITERAL_STR_MAX_LENGTH);

                    numeric_literal_str[0] = '-';
                    numeric_literal_str_pos++;
                }
                else
                {
                    // TODO: This should be treated as a subtraction operator. Ensure previous token is valid, etc.
                    BERROR("subtraction is not supported at this time");
                    return false;
                }
            } break;
            case BSON_TOKEN_TYPE_OPERATOR_PLUS:
                BERROR("Addition is not supported at this time");
                return false;
                break;
            case BSON_TOKEN_TYPE_OPERATOR_DOT:
                // This could be the first in a string of tokens of a numeric literal
                if (!expect_numeric)
                {
                    // Check the next token to see if it is a numeric. It must be in order for this to be part of it.
                    // Whitespace in between is not supported
                    if (parser->tokens[index + 1].type == BSON_TOKEN_TYPE_NUMERIC_LITERAL)
                    {
                        // Start a numeric literal
                        numeric_literal_str[0] = '.';
                        expect_numeric = true;
                        bzero_memory(numeric_literal_str, sizeof(char) * NUMERIC_LITERAL_STR_MAX_LENGTH);
                        numeric_decimal_pos = 0;
                        numeric_literal_str_pos++;
                    }
                    else
                    {
                        // TODO: Support named object properties such as "sponza.name"
                        BERROR("Dot property operator not supported. Position: %u", current_token->start);
                        return false;
                    }
                }
                else
                {
                    // Just verify that a decimal doesn't already exist
                    if (numeric_decimal_pos != -1)
                    {
                        BERROR("Cannot include more than once decimal in a numeric literal. First occurrance: %i, Position: %u", numeric_decimal_pos, current_token->start);
                        return false;
                    }

                    // Append it to the string
                    numeric_literal_str[numeric_literal_str_pos] = '.';
                    numeric_decimal_pos = numeric_literal_str_pos;
                    numeric_literal_str_pos++;
                }
                break;
            case BSON_TOKEN_TYPE_OPERATOR_ASTERISK:
            case BSON_TOKEN_TYPE_OPERATOR_SLASH:
                BERROR("Unexpected token at position %u. Parse failed", current_token->start);
                return false;
            case BSON_TOKEN_TYPE_NUMERIC_LITERAL:
            {
                if (!expect_numeric)
                {
                    expect_numeric = true;
                    bzero_memory(numeric_literal_str, sizeof(char) * NUMERIC_LITERAL_STR_MAX_LENGTH);
                }
                u32 length = current_token->end - current_token->start;
                string_ncopy(numeric_literal_str + numeric_literal_str_pos, parser->file_content + current_token->start, length);
                numeric_literal_str_pos += length;
            } break;
            case BSON_TOKEN_TYPE_STRING_LITERAL:
                if (!expect_value)
                {
                    BERROR("Unexpected string token at position: %u", current_token->start);
                    return false;
                }

                if (current_object->type == BSON_OBJECT_TYPE_ARRAY)
                {
                    // Apply the value directly to a newly-created, non-named property that gets added to current_object
                    bson_property p = {0};
                    p.type = BSON_PROPERTY_TYPE_STRING;
                    p.value.s = string_from_bson_token(parser->file_content, current_token);
                    p.name = INVALID_BSTRING_ID;
                    darray_push(current_object->properties, p);
                }
                else
                {
                    current_property->type = BSON_PROPERTY_TYPE_STRING;
                    current_property->value.s = string_from_bson_token(parser->file_content, current_token);
                }

                expect_value = current_object->type == BSON_OBJECT_TYPE_ARRAY;
                break;
            case BSON_TOKEN_TYPE_BOOLEAN:
            {
                if (!expect_value)
                {
                    BERROR("Unexpected boolean token at position: %u", current_token->start);
                    return false;
                }

                char* token_string = string_from_bson_token(parser->file_content, current_token);
                b8 bool_value = false;
                if (!string_to_bool(token_string, &bool_value))
                    BERROR("Failed to parse boolean from token. Position: %u", current_token->start);
                string_free(token_string);

                if (current_object->type == BSON_OBJECT_TYPE_ARRAY)
                {
                    // Apply the value directly to a newly-created, non-named property that gets added to current_object
                    bson_property p = {0};
                    p.type = BSON_PROPERTY_TYPE_BOOLEAN;
                    p.value.b = bool_value;
                    p.name = INVALID_BSTRING_ID;
                    darray_push(current_object->properties, p);
                }
                else
                {
                    current_property->type = BSON_PROPERTY_TYPE_BOOLEAN;
                    current_property->value.b = bool_value;
                }

                expect_value = current_object->type == BSON_OBJECT_TYPE_ARRAY;
            } break;
            case BSON_TOKEN_TYPE_NEWLINE:
                if (expect_numeric)
                {
                    // Terminate the numeric and set the current property's value to it
                    bson_property p = {0};
                    p.name = INVALID_BSTRING_ID;
                    // Determine whether it is a float or a int
                    if (string_index_of(numeric_literal_str, '.') != -1)
                    {
                        f32 f_value = 0;
                        if (!string_to_f32(numeric_literal_str, &f_value))
                        {
                            BERROR("Failed to parse string to float: '%s', Position: %u", numeric_literal_str, current_token->start);
                            return false;
                        }
                        p.value.f = f_value;
                        p.type = BSON_PROPERTY_TYPE_FLOAT;
                    }
                    else
                    {
                        i64 i_value = 0;
                        if (!string_to_i64(numeric_literal_str, &i_value))
                        {
                            BERROR("Failed to parse string to signed int: '%s', Position: %u", numeric_literal_str, current_token->start);
                            return false;
                        }
                        p.value.i = i_value;
                        p.type = BSON_PROPERTY_TYPE_INT;
                    }

                    if (current_object->type == BSON_OBJECT_TYPE_ARRAY)
                    {
                        // Apply the value directly to a newly-created, non-named property that gets added to current_object
                        darray_push(current_object->properties, p);
                    }
                    else
                    {
                        current_property->type = p.type;
                        current_property->value = p.value;
                    }

                    // Reset the numeric parse string state
                    u32 num_lit_len = string_length(numeric_literal_str);
                    bzero_memory(numeric_literal_str, sizeof(char) * num_lit_len);
                    expect_numeric = false;
                    numeric_decimal_pos = -1;
                    numeric_literal_str_pos = 0;

                    // Current value is set, so now expect another identifier or array element
                }

                // Don't expect a value after a newline
                expect_value = current_object->type == BSON_OBJECT_TYPE_ARRAY;
                expect_identifier = !expect_value;
                break;
            case BSON_TOKEN_TYPE_EOF:
            {
                b8 valid = true;
                // Verify that we are not in the middle of assignment
                if (expect_value || expect_operator || expect_numeric)
                    valid = false;
                // Verify that the current depth is now 1 (to account for the base object)
                if (scope.element_count > 1)
                    valid = false;

                if (!valid)
                {
                    BERROR("Unexpected end of file at position: %u", current_token->start);
                    return false;
                }

            } break;
        }
        index++;
        current_token = &parser->tokens[index];
    }

    return true;
}

b8 bson_tree_from_string(const char* source, bson_tree* out_tree)
{
    if (!source)
    {
        BERROR("bson_tree_from_string requires valid source");
        return false;
    }
    if (!out_tree)
    {
        BERROR("bson_tree_from_string requires a valid pointer to out_tree");
        return false;
    }

    // String is empty, return empty tree
    if (string_length(source) < 1)
    {
        out_tree->root.type = BSON_OBJECT_TYPE_OBJECT;
        out_tree->root.properties = 0;
        return true;
    }

    // Create a parser to use
    bson_parser parser;
    if (!bson_parser_create(&parser))
    {
        BERROR("Failed to create BSON parser");
        return false;
    }

    b8 result = true;

    // Start tokenizing
    if (!bson_parser_tokenize(&parser, source))
    {
        BERROR("Tokenization failed. See logs for details");
        result = false;
        goto bson_tree_from_string_parser_cleanup;
    }

    // Parse the tokens
    if (!bson_parser_parse(&parser, out_tree))
    {
        BERROR("Parsing failed. See logs for details");
        result = false;
        goto bson_tree_from_string_parser_cleanup;
    }

bson_tree_from_string_parser_cleanup:
    bson_parser_destroy(&parser);
    if (!result && out_tree->root.properties)
        bson_tree_cleanup(out_tree);
    return result;
}

static void write_spaces(char* out_source, u32* position, u16 count)
{
    if (out_source)
    {
        for (u32 s = 0; s < count; ++s)
        {
            out_source[(*position)] = ' ';
            (*position)++;
        }
    }
    else
    {
        (*position) += count;
    }
}

static void write_string(char* out_source, u32* position, const char* str)
{
    u32 len = string_length(str);
    if (out_source)
    {
        for (u32 s = 0; s < len; ++s)
        {
            out_source[(*position)] = str[s];
            (*position)++;
        }
    }
    else
    {
        (*position) += len;
    }
}

static void bson_tree_object_to_string(const bson_object* obj, char* out_source, u32* position, i16 indent_level, u8 indent_spaces)
{
    indent_level++;

    if (obj && obj->properties)
    {
        u32 prop_count = darray_length(obj->properties);
        for (u32 i = 0; i < prop_count; ++i)
        {
            bson_property* p = &obj->properties[i];
            // Write indent
            write_spaces(out_source, position, indent_level * indent_spaces);

            // If named, it is a property being defined. Otherwise it is an array element
            if (p->name)
            {
                // Try as a stringid first, then bname if nothing is returned
                const char* name_str = bstring_id_string_get(p->name);
                if (!name_str)
                    name_str = bname_string_get(p->name);

                // Only actually write out the name if it exists
                if (name_str)
                {
                    // write the name, then a space, then =, then another space
                    write_string(out_source, position, name_str);
                    write_spaces(out_source, position, 1);
                    write_string(out_source, position, "=");
                    write_spaces(out_source, position, 1);
                }
            }

            // Write the value
            switch (p->type)
            {
                case BSON_PROPERTY_TYPE_OBJECT:
                case BSON_PROPERTY_TYPE_ARRAY:
                {
                    // Opener/closer and newline based on type
                    const char* opener = p->type == BSON_PROPERTY_TYPE_OBJECT ? "{\n" : "[\n";
                    const char* closer = p->type == BSON_PROPERTY_TYPE_OBJECT ? "}\n" : "]\n";
                    write_string(out_source, position, opener);

                    bson_tree_object_to_string(&p->value.o, out_source, position, indent_level, indent_spaces);

                    // Indent the closer
                    write_spaces(out_source, position, indent_level * indent_spaces);
                    write_string(out_source, position, closer);
                } break;
                case BSON_PROPERTY_TYPE_STRING:
                {
                    if (p->value.s)
                    {
                        // Surround the string with quotes and put a newline after
                        write_string(out_source, position, "\"");
                        write_string(out_source, position, p->value.s);
                        write_string(out_source, position, "\"\n");
                    }
                    else
                    {
                        // Write an empty string
                        write_string(out_source, position, "\"\"\n");
                    }
                } break;
                case BSON_PROPERTY_TYPE_BOOLEAN:
                {
                    write_string(out_source, position, p->value.b ? "true" : "false");
                    write_string(out_source, position, "\n");
                } break;
                case BSON_PROPERTY_TYPE_INT:
                {
                    char buffer[30] = {0};
                    string_append_int(buffer, "", p->value.i);
                    write_string(out_source, position, buffer);
                    write_string(out_source, position, "\n");
                } break;
                case BSON_PROPERTY_TYPE_FLOAT:
                {
                    char buffer[30] = {0};
                    string_append_float(buffer, "", p->value.f);
                    write_string(out_source, position, buffer);
                    write_string(out_source, position, "\n");
                } break;
                default:
                case BSON_PROPERTY_TYPE_UNKNOWN:
                {
                    BWARN("bson_tree_object_to_string encountered an unknown property type");
                } break;
            }
        }
    }
}

const char* bson_tree_to_string(bson_tree* tree)
{
    if (!tree || !tree->root.properties)
        return 0;

    u32 length = 0;
    bson_tree_object_to_string(&tree->root, 0, &length, -1, 4);
    char* out_string = ballocate(sizeof(char) * (length + 1), MEMORY_TAG_STRING);

    length = 0;
    bson_tree_object_to_string(&tree->root, out_string, &length, -1, 4);

    return out_string;
}

void bson_object_cleanup(bson_object* obj)
{
    if (obj && obj->properties)
    {
        u32 prop_count = darray_length(obj->properties);
        for (u32 i = 0; i < prop_count; ++i)
        {
            bson_property* p = &obj->properties[i];
            switch (p->type)
            {
                case BSON_PROPERTY_TYPE_OBJECT:
                {
                    bson_object_cleanup(&p->value.o);
                } break;
                case BSON_PROPERTY_TYPE_ARRAY:
                {
                    bson_object_cleanup(&p->value.o);
                } break;
                case BSON_PROPERTY_TYPE_STRING:
                {
                    if (p->value.s)
                    {
                        string_free((char*)p->value.s);
                        p->value.s = 0;
                    }
                } break;
                case BSON_PROPERTY_TYPE_BOOLEAN:
                case BSON_PROPERTY_TYPE_FLOAT:
                case BSON_PROPERTY_TYPE_INT:
                {
                    // no-op
                } break;
                default:
                case BSON_PROPERTY_TYPE_UNKNOWN:
                {
                    BWARN("bson_tree_object_cleanup encountered an unknown property type");
                    BWARN("Ensure the same object wasn't added more than once somewhere in code");
                } break;
            }
        }
        darray_destroy(obj->properties);
        bzero_memory(obj, sizeof(bson_object));
    }
}

void bson_tree_cleanup(bson_tree* tree)
{
    if (tree && tree->root.properties)
        bson_object_cleanup(&tree->root);
}

static b8 bson_object_property_add(bson_object* obj, bson_property_type type, const char* name, bson_property_value value)
{
    if (!obj)
    {
        BERROR("bson_object_property_add requires a valid pointer to a bson_object of object type");
        return false;
    }

    if (!name)
    {
        BERROR("bson_object_property_add requires a valid pointer to a name");
        return false;
    }

    if (obj->type != BSON_OBJECT_TYPE_OBJECT)
    {
        BERROR("Cannot use bson_object_property_add on a non-object");
        if (obj->type == BSON_OBJECT_TYPE_ARRAY)
        {
            BERROR("Passed object is an array. Use bson_array_value_add_[type] instead");
        }
        return false;
    }

    bstring_id new_name = bstring_id_create(name);

    if (!obj->properties)
    {
        obj->properties = darray_create(bson_property);
    }
    else
    {
        // Check the object's properties and see if an object with that name already exists. If it does, replace it
        u32 property_count = darray_length(obj->properties);
        for (u32 i = 0; i < property_count; ++i)
        {
            bson_property* p = &obj->properties[i];
            if (p->name == new_name)
            {
                BTRACE("Property '%s' already exists in object, and will be overwritten. Was this intentional?", name);
                // Replace the property. Start by cleaning up the old one
                switch (p->type)
                {
                case BSON_PROPERTY_TYPE_STRING:
                    if (p->value.s)
                    {
                        string_free(p->value.s);
                        p->value.s = 0;
                    }
                    break;
                case BSON_PROPERTY_TYPE_OBJECT:
                case BSON_PROPERTY_TYPE_ARRAY:
                    bson_object_cleanup(&p->value.o);
                    break;
                default:
                    // Nothing to cleanup for other types
                    break;
                }
                bzero_memory(&p->value, sizeof(bson_property_value));
                // Assign new values
                p->type = type;
                p->name = new_name;
#ifdef BISMUTH_DEBUG
                p->name_str = string_duplicate(name);
#endif
                p->value = value;
                return true;
            }
        }
    }

    bson_property new_prop = {0};
    new_prop.type = type;
    new_prop.name = new_name;
#ifdef BISMUTH_DEBUG
    new_prop.name_str = string_duplicate(name);
#endif
    new_prop.value = value;

    darray_push(obj->properties, new_prop);

    return true;
}

// Internal for now since this API might not make sense externally
static b8 bson_array_value_add_unnamed_property(bson_array* array, bson_property_type type, bson_property_value value)
{
    if (!array)
    {
        BERROR("bson_array_value_add_unnamed_property requires a valid pointer to a bson_object of array type");
        return false;
    }

    if (array->type != BSON_OBJECT_TYPE_ARRAY)
    {
        BERROR("Cannot use bson_array_property_add on a non-array");
        if (array->type == BSON_OBJECT_TYPE_OBJECT)
            BERROR("Passed object is an object. Use bson_object_property_add instead");
        return false;
    }

    if (!array->properties)
        array->properties = darray_create(bson_property);

    bson_property new_prop = {0};
    new_prop.type = type;
    new_prop.name = INVALID_BSTRING_ID;
    new_prop.value = value;

    darray_push(array->properties, new_prop);

    return true;
}

b8 bson_array_value_add_int(bson_array* array, i64 value)
{
    bson_property_value pv = {0};
    pv.i = value;

    return bson_array_value_add_unnamed_property(array, BSON_PROPERTY_TYPE_INT, pv);
}

b8 bson_array_value_add_float(bson_array* array, f32 value)
{
    bson_property_value pv = {0};
    pv.f = value;

    return bson_array_value_add_unnamed_property(array, BSON_PROPERTY_TYPE_FLOAT, pv);
}

b8 bson_array_value_add_boolean(bson_array* array, b8 value)
{
    bson_property_value pv = {0};
    pv.b = value;

    return bson_array_value_add_unnamed_property(array, BSON_PROPERTY_TYPE_BOOLEAN, pv);
}

b8 bson_array_value_add_string(bson_array* array, const char* value)
{
    if (!value)
    {
        BERROR("bson_array_value_add_string requires a valid pointer to a string value");
        return false;
    }

    bson_property_value pv = {0};
    pv.s = string_duplicate(value);

    return bson_array_value_add_unnamed_property(array, BSON_PROPERTY_TYPE_STRING, pv);
}

b8 bson_array_value_add_mat4(bson_array* array, mat4 value)
{
    const char* temp_str = mat4_to_string(value);
    if (!temp_str)
    {
        BWARN("bson_array_value_add_mat4 failed to convert value to string");
        return false;
    }
    b8 result = bson_array_value_add_string(array, temp_str);
    string_free(temp_str);
    return result;
}

b8 bson_array_value_add_vec4(bson_array* array, vec4 value)
{
    const char* temp_str = vec4_to_string(value);
    if (!temp_str)
    {
        BWARN("bson_array_value_add_vec4 failed to convert value to string");
        return false;
    }
    b8 result = bson_array_value_add_string(array, temp_str);
    string_free(temp_str);
    return result;
}

b8 bson_array_value_add_vec3(bson_array* array, vec3 value)
{
    const char* temp_str = vec3_to_string(value);
    if (!temp_str)
    {
        BWARN("bson_array_value_add_vec3 failed to convert value to string");
        return false;
    }
    b8 result = bson_array_value_add_string(array, temp_str);
    string_free(temp_str);
    return result;
}

b8 bson_array_value_add_vec2(bson_array* array, vec2 value)
{
    const char* temp_str = vec2_to_string(value);
    if (!temp_str)
    {
        BWARN("bson_array_value_add_vec2 failed to convert value to string");
        return false;
    }
    b8 result = bson_array_value_add_string(array, temp_str);
    string_free(temp_str);
    return result;
}

b8 bson_array_value_add_bname_as_string(bson_array* array, bname value)
{
    const char* temp_str = bname_string_get(value);
    if (!temp_str)
    {
        BWARN("bson_array_value_add_bname_as_string failed to convert value to string");
        return false;
    }
    b8 result = bson_array_value_add_string(array, temp_str);
    return result;
}

b8 bson_array_value_add_bstring_id_as_string(bson_array* array, bstring_id value)
{
    const char* temp_str = bstring_id_string_get(value);
    if (!temp_str)
    {
        BWARN("bson_array_value_add_bstring_id_as_string failed to convert value to string");
        return false;
    }
    b8 result = bson_array_value_add_string(array, temp_str);
    return result;
}

b8 bson_array_value_add_object(bson_array* array, bson_object value)
{
    bson_property_value pv = {0};
    pv.o = value;

    return bson_array_value_add_unnamed_property(array, BSON_PROPERTY_TYPE_OBJECT, pv);
}

b8 bson_array_value_add_object_empty(bson_array* array)
{
    bson_object new_obj = {0};
    new_obj.type = BSON_OBJECT_TYPE_OBJECT;
    new_obj.properties = 0;

    bson_property_value pv = {0};
    pv.o = new_obj;

    return bson_array_value_add_unnamed_property(array, BSON_PROPERTY_TYPE_OBJECT, pv);
}

b8 bson_array_value_add_array(bson_array* array, bson_object value)
{
    bson_property_value pv = {0};
    pv.o = value;

    return bson_array_value_add_unnamed_property(array, BSON_PROPERTY_TYPE_ARRAY, pv);
}

b8 bson_array_value_add_array_empty(bson_array* array)
{
    bson_object new_arr = {0};
    new_arr.type = BSON_OBJECT_TYPE_ARRAY;
    new_arr.properties = 0;

    bson_property_value pv = {0};
    pv.o = new_arr;

    return bson_array_value_add_unnamed_property(array, BSON_PROPERTY_TYPE_ARRAY, pv);
}

// Object functions

b8 bson_object_value_add_int(bson_object* object, const char* name, i64 value)
{
    bson_property_value pv = {0};
    pv.i = value;

    return bson_object_property_add(object, BSON_PROPERTY_TYPE_INT, name, pv);
}

b8 bson_object_value_add_float(bson_object* object, const char* name, f32 value)
{
    bson_property_value pv = {0};
    pv.f = value;

    return bson_object_property_add(object, BSON_PROPERTY_TYPE_FLOAT, name, pv);
}

b8 bson_object_value_add_boolean(bson_object* object, const char* name, b8 value)
{
    bson_property_value pv = {0};
    pv.b = value;

    return bson_object_property_add(object, BSON_PROPERTY_TYPE_BOOLEAN, name, pv);
}

b8 bson_object_value_add_string(bson_object* object, const char* name, const char* value)
{
    if (!value)
    {
        BERROR("bson_object_value_add_string requires a valid pointer to value");
        return false;
    }

    bson_property_value pv = {0};
    pv.s = string_duplicate(value);

    return bson_object_property_add(object, BSON_PROPERTY_TYPE_STRING, name, pv);
}

b8 bson_object_value_add_mat4(bson_object* object, const char* name, mat4 value)
{
    const char* temp_str = mat4_to_string(value);
    if (!temp_str)
    {
        BWARN("bson_object_value_add_mat4 failed to convert value to string");
        return false;
    }
    b8 result = bson_object_value_add_string(object, name, temp_str);
    string_free(temp_str);
    return result;
}

b8 bson_object_value_add_vec4(bson_object* object, const char* name, vec4 value)
{
    const char* temp_str = vec4_to_string(value);
    if (!temp_str)
    {
        BWARN("bson_object_value_add_vec4 failed to convert value to string");
        return false;
    }
    b8 result = bson_object_value_add_string(object, name, temp_str);
    string_free(temp_str);
    return result;
}

b8 bson_object_value_add_vec3(bson_object* object, const char* name, vec3 value)
{
    const char* temp_str = vec3_to_string(value);
    if (!temp_str)
    {
        BWARN("bson_object_value_add_vec3 failed to convert value to string");
        return false;
    }
    b8 result = bson_object_value_add_string(object, name, temp_str);
    string_free(temp_str);
    return result;
}

b8 bson_object_value_add_vec2(bson_object* object, const char* name, vec2 value)
{
    const char* temp_str = vec2_to_string(value);
    if (!temp_str)
    {
        BWARN("bson_object_value_add_vec2 failed to convert value to string");
        return false;
    }
    b8 result = bson_object_value_add_string(object, name, temp_str);
    string_free(temp_str);
    return result;
}

b8 bson_object_value_add_bname_as_string(bson_object* object, const char* name, bname value)
{
    const char* temp_str = bname_string_get(value);
    if (!temp_str)
    {
        BWARN("bson_object_value_add_bname_as_string failed to convert value to string");
        return false;
    }
    b8 result = bson_object_value_add_string(object, name, temp_str);
    return result;
}

b8 bson_object_value_add_bstring_id_as_string(bson_object* object, const char* name, bstring_id value)
{
    const char* temp_str = bstring_id_string_get(value);
    if (!temp_str)
    {
        BWARN("bson_object_value_add_bstring_id_as_string failed to convert value to string");
        return false;
    }
    b8 result = bson_object_value_add_string(object, name, temp_str);
    return result;
}

b8 bson_object_value_add_object(bson_object* object, const char* name, bson_object value)
{
    bson_property_value pv = {0};
    pv.o = value;

    return bson_object_property_add(object, BSON_PROPERTY_TYPE_OBJECT, name, pv);
}

b8 bson_object_value_add_object_empty(bson_object* object, const char* name)
{
    bson_object new_obj = {0};
    new_obj.type = BSON_OBJECT_TYPE_OBJECT;
    new_obj.properties = 0;

    bson_property_value pv = {0};
    pv.o = new_obj;

    return bson_object_property_add(object, BSON_PROPERTY_TYPE_OBJECT, name, pv);
}

b8 bson_object_value_add_array(bson_object* object, const char* name, bson_object value)
{
    bson_property_value pv = {0};
    pv.o = value;

    return bson_object_property_add(object, BSON_PROPERTY_TYPE_ARRAY, name, pv);
}

b8 bson_object_value_add_array_empty(bson_object* object, const char* name)
{
    bson_object new_arr = {0};
    new_arr.type = BSON_OBJECT_TYPE_ARRAY;
    new_arr.properties = 0;

    bson_property_value pv = {0};
    pv.o = new_arr;

    return bson_object_property_add(object, BSON_PROPERTY_TYPE_ARRAY, name, pv);
}

b8 bson_array_element_count_get(bson_array* array, u32* out_count)
{
    if (!array || array->type != BSON_OBJECT_TYPE_ARRAY || !out_count)
    {
        BERROR("bson_array_element_count_get requires a valid pointer to an array object and out_count");
        return false;
    }

    if (!array->properties)
    {
        *out_count = 0;
        return true;
    }

    *out_count = darray_length(array->properties);
    return true;
}

b8 bson_array_element_type_at(bson_array* array, u32 index, bson_property_type* out_type)
{
    if (!array || array->type != BSON_OBJECT_TYPE_ARRAY || !out_type)
    {
        BERROR("bson_array_element_count_get requires a valid pointer to an array object and out_type");
        return false;
    }

    if (!array->properties)
    {
        BWARN("bson_array_element_type_at called on an empty array. Any index would be out of bounds");
        *out_type = BSON_PROPERTY_TYPE_UNKNOWN;
        return false;
    }

    u32 count = darray_length(array->properties);
    if (index >= count)
    {
        BWARN("bson_array_element_type_at index %u is out of range [0-%u]", index, count);
        *out_type = BSON_PROPERTY_TYPE_UNKNOWN;
        return false;
    }

    *out_type = array->properties[index].type;
    return true;
}

static b8 bson_array_index_in_range(const bson_array* array, u32 index)
{
    if (!array || array->type != BSON_OBJECT_TYPE_ARRAY)
    {
        BERROR("bson_array_index_in_range requires a valid pointer to an array object");
        return false;
    }

    if (!array->properties)
    {
        BWARN("bson_array_index_in_range called on an empty array. Any index would be out of bounds");
        return false;
    }

    u32 count = darray_length(array->properties);
    return index < count;
}

b8 bson_array_element_value_get_int(const bson_array* array, u32 index, i64* out_value)
{
    if (!out_value || !bson_array_index_in_range(array, index))
        return false;

    BASSERT_MSG(array->properties[index].type == BSON_PROPERTY_TYPE_INT, "Array element is not an int");

    *out_value = array->properties[index].value.i;
    return true;
}

b8 bson_array_element_value_get_float(const bson_array* array, u32 index, f32* out_value)
{
    if (!out_value || !bson_array_index_in_range(array, index))
        return false;

    BASSERT_MSG(array->properties[index].type == BSON_PROPERTY_TYPE_FLOAT, "Array element is not a float");

    *out_value = array->properties[index].value.f;
    return true;
}

b8 bson_array_element_value_get_bool(const bson_array* array, u32 index, b8* out_value)
{
    if (!out_value || !bson_array_index_in_range(array, index))
        return false;

    BASSERT_MSG(array->properties[index].type == BSON_PROPERTY_TYPE_BOOLEAN, "Array element is not a boolean");

    *out_value = array->properties[index].value.b;
    return true;
}

b8 bson_array_element_value_get_string(const bson_array* array, u32 index, const char** out_value)
{
    if (!out_value || !bson_array_index_in_range(array, index))
        return false;

    bson_property* p = &array->properties[index];
    if (p->type != BSON_PROPERTY_TYPE_STRING)
    {
        BERROR("Error parsing array element value as '%s' - it is instead stored as (type='%s')", bson_property_type_to_string(BSON_PROPERTY_TYPE_STRING), bson_property_type_to_string(p->type));
        return 0;
    }

    *out_value = array->properties[index].value.s;
    return true;
}

b8 bson_array_element_value_get_mat4(const bson_array* array, u32 index, mat4* out_value)
{
    if (!out_value || !bson_array_index_in_range(array, index))
        return false;

    BASSERT_MSG(array->properties[index].type == BSON_PROPERTY_TYPE_STRING, "Array element is not stored as a string");

    const char* str = array->properties[index].value.s;
    return string_to_mat4(str, out_value);
}

b8 bson_array_element_value_get_vec4(const bson_array* array, u32 index, vec4* out_value)
{
    if (!out_value || !bson_array_index_in_range(array, index))
        return false;

    BASSERT_MSG(array->properties[index].type == BSON_PROPERTY_TYPE_STRING, "Array element is not stored as a string");

    const char* str = array->properties[index].value.s;
    return string_to_vec4(str, out_value);
}

b8 bson_array_element_value_get_vec3(const bson_array* array, u32 index, vec3* out_value)
{
    if (!out_value || !bson_array_index_in_range(array, index))
        return false;

    BASSERT_MSG(array->properties[index].type == BSON_PROPERTY_TYPE_STRING, "Array element is not stored as a string");

    const char* str = array->properties[index].value.s;
    return string_to_vec3(str, out_value);
}

b8 bson_array_element_value_get_vec2(const bson_array* array, u32 index, vec2* out_value)
{
    if (!out_value || !bson_array_index_in_range(array, index))
        return false;

    BASSERT_MSG(array->properties[index].type == BSON_PROPERTY_TYPE_STRING, "Array element is not stored as a string");

    const char* str = array->properties[index].value.s;
    return string_to_vec2(str, out_value);
}

b8 bson_array_element_value_get_string_as_bname(const bson_array* array, u32 index, bname* out_value)
{
    if (!out_value || !bson_array_index_in_range(array, index))
        return false;

    BASSERT_MSG(array->properties[index].type == BSON_PROPERTY_TYPE_STRING, "Array element is not stored as a string");

    const char* str = array->properties[index].value.s;
    *out_value = bname_create(str);
    return true;
}

b8 bson_array_element_value_get_string_as_bstring_id(const bson_array* array, u32 index, bstring_id* out_value)
{
    if (!out_value || !bson_array_index_in_range(array, index))
        return false;

    BASSERT_MSG(array->properties[index].type == BSON_PROPERTY_TYPE_STRING, "Array element is not stored as a string");

    const char* str = array->properties[index].value.s;
    *out_value = bstring_id_create(str);
    return true;
}

b8 bson_array_element_value_get_object(const bson_array* array, u32 index, bson_object* out_value)
{
    if (!out_value || !bson_array_index_in_range(array, index))
        return false;

    bson_property* p = &array->properties[index];
    if (p->type != BSON_PROPERTY_TYPE_OBJECT)
    {
        BERROR("Error parsing array element value as '%s' - property is instead stored as (type='%s').", bson_property_type_to_string(BSON_PROPERTY_TYPE_OBJECT), bson_property_type_to_string(p->type));
        return false;
    }
    *out_value = p->value.o;
    return true;
}

b8 bson_array_element_value_get_array(const bson_array* array, u32 index, bson_array* out_value)
{
    if (!out_value || !bson_array_index_in_range(array, index))
        return false;

    bson_property* p = &array->properties[index];
    if (p->type != BSON_PROPERTY_TYPE_ARRAY)
    {
        BERROR("Error parsing array element value as '%s' - property is instead stored as (type='%s')", bson_property_type_to_string(BSON_PROPERTY_TYPE_ARRAY), bson_property_type_to_string(p->type));
        return false;
    }

    *out_value = p->value.o;
    return true;
}

b8 bson_object_property_type_get(const bson_object* object, const char* name, bson_property_type* out_type)
{
    if (!object)
    {
        BERROR("bson_object_property_type_get requires a valid pointer to an object");
        *out_type = BSON_PROPERTY_TYPE_UNKNOWN;
        return false;
    }

    if (!object->properties)
    {
        BERROR("bson_object_property_type_get cannot get a property type for an object with no properties");
        *out_type = BSON_PROPERTY_TYPE_UNKNOWN;
        return false;
    }

    u32 count = darray_length(object->properties);
    bstring_id search_name = bstring_id_create(name);
    for (u32 i = 0; i < count; ++i)
    {
        if (object->properties[i].name == search_name)
        {
            *out_type = object->properties[i].type;
            return true;
        }
    }

    BERROR("Failed to find object property named '%s'", name);
    *out_type = BSON_PROPERTY_TYPE_UNKNOWN;
    return false;
}

b8 bson_object_property_count_get(const bson_object* object, u32* out_count)
{
    if (!object)
    {
        BERROR("bson_object_property_type_get requires a valid pointer to object");
        *out_count = 0;
        return false;
    }

    if (!object->properties)
    {
        *out_count = 0;
        return true;
    }

    *out_count = darray_length(object->properties);
    return true;
}

static i32 bson_object_property_index_get(const bson_object* object, const char* name)
{
    if (!object || !name)
        return -1;
    if (!object->properties)
        return -1;

    u32 count = darray_length(object->properties);
    bstring_id search_name = bstring_id_create(name);
    for (u32 i = 0; i < count; ++i)
    {
        if (object->properties[i].name == search_name)
            return i;
    }

    return -1;
}

b8 bson_object_property_value_type_get(const bson_object* object, const char* name, bson_property_type* out_type)
{
    i32 index = bson_object_property_index_get(object, name);
    if (index == -1)
    {
        *out_type = BSON_PROPERTY_TYPE_UNKNOWN;
        return false;
    }

    *out_type = object->properties[index].type;
    return true;
}

b8 bson_object_property_value_get_int(const bson_object* object, const char* name, i64* out_value)
{
    i32 index = bson_object_property_index_get(object, name);
    if (index == -1)
    {
        *out_value = 0;
        return false;
    }

    bson_property* p = &object->properties[index];

    // NOTE: Try some automatic type conversions
    if (p->type == BSON_PROPERTY_TYPE_INT)
        *out_value = p->value.i;
    else if (p->type == BSON_PROPERTY_TYPE_BOOLEAN)
        *out_value = p->value.b ? 1 : 0;
    else if (p->type == BSON_PROPERTY_TYPE_FLOAT)
        *out_value = (i64)p->value.f;
    else
    {
        BERROR("Attempted to get property '%s' as type '%s' when it is of type '%s'", name, bson_property_type_to_string(BSON_PROPERTY_TYPE_INT), bson_property_type_to_string(p->type));
        return false;
    }

    return true;
}

b8 bson_object_property_value_get_float(const bson_object* object, const char* name, f32* out_value)
{
    i32 index = bson_object_property_index_get(object, name);
    if (index == -1)
    {
        *out_value = 0;
        return false;
    }
    bson_property* p = &object->properties[index];

    // If the property is an int, cast to a float
    if (p->type == BSON_PROPERTY_TYPE_FLOAT)
        *out_value = p->value.f;
    else if (p->type == BSON_PROPERTY_TYPE_INT)
        *out_value = (f32)p->value.i;
    else if (p->type == BSON_PROPERTY_TYPE_BOOLEAN)
        *out_value = (f32)p->value.b;
    else
    {
        BERROR("Attempted to get property '%s' as type '%s' when it is of type '%s'", name, bson_property_type_to_string(BSON_PROPERTY_TYPE_FLOAT), bson_property_type_to_string(p->type));
        return false;
    }

    return true;
}

b8 bson_object_property_value_get_bool(const bson_object* object, const char* name, b8* out_value)
{
    i32 index = bson_object_property_index_get(object, name);
    if (index == -1)
    {
        *out_value = 0;
        return false;
    }

    bson_property* p = &object->properties[index];

    // NOTE: Try some type conversions
    if (p->type == BSON_PROPERTY_TYPE_BOOLEAN)
        *out_value = p->value.b;
    else if (p->type == BSON_PROPERTY_TYPE_INT)
        *out_value = p->value.i == 0 ? false : true;
    else if (p->type == BSON_PROPERTY_TYPE_FLOAT)
        *out_value = p->value.f == 0 ? false : true;
    else
    {
        BERROR("Attempted to get property '%s' as type '%s' when it is of type '%s'", name, bson_property_type_to_string(BSON_PROPERTY_TYPE_BOOLEAN), bson_property_type_to_string(p->type));
        return false;
    }

    return true;
}

b8 bson_object_property_value_get_string(const bson_object* object, const char* name, const char** out_value)
{
    i32 index = bson_object_property_index_get(object, name);
    if (index == -1)
    {
        *out_value = 0;
        return false;
    }

    bson_property* p = &object->properties[index];

    // NOTE: Try some type conversions
    if (p->type == BSON_PROPERTY_TYPE_INT)
    {
        *out_value = string_format("%i", p->value.i);
    }
    else if (p->type == BSON_PROPERTY_TYPE_FLOAT)
    {
        *out_value = string_format("%f", p->value.f);
    }
    else if (p->type == BSON_PROPERTY_TYPE_BOOLEAN)
    {
        *out_value = string_format("%s", p->value.b ? "true" : "false");
    }
    else if (p->type == BSON_PROPERTY_TYPE_OBJECT)
    {
        *out_value = string_duplicate("[Object]");
    }
    else if (p->type == BSON_PROPERTY_TYPE_ARRAY)
    {
        *out_value = string_duplicate("[Array]");
    }
    else if (p->type == BSON_PROPERTY_TYPE_STRING)
    {
        *out_value = string_duplicate(p->value.s);
    }
    else
    {
        BERROR("Attempted to get property '%s' as type '%s' when it is of type '%s'", name, bson_property_type_to_string(BSON_PROPERTY_TYPE_STRING), bson_property_type_to_string(p->type));
        *out_value = 0;
        return false;
    }

    return true;
}

static const char* bson_object_property_value_get_string_reference(const bson_object* object, const char* name, const char* target_type)
{
    i32 index = bson_object_property_index_get(object, name);
    if (index == -1)
        return 0;

    // These should always be stored as a string
    bson_property* p = &object->properties[index];
    if (p->type != BSON_PROPERTY_TYPE_STRING)
    {
        BERROR("Error parsing value as '%s' - property is instead stored as (type='%s')", bson_property_type_to_string(BSON_PROPERTY_TYPE_STRING), bson_property_type_to_string(p->type));
        return 0;
    }
    return p->value.s;
}

b8 bson_object_property_value_get_mat4(const bson_object* object, const char* name, mat4* out_value)
{
    if (!out_value)
        return false;

    const char* str = bson_object_property_value_get_string_reference(object, name, "mat4");
    return string_to_mat4(str, out_value);
}

b8 bson_object_property_value_get_vec4(const bson_object* object, const char* name, vec4* out_value)
{
    if (!out_value)
        return false;

    const char* str = bson_object_property_value_get_string_reference(object, name, "vec4");
    return string_to_vec4(str, out_value);
}

b8 bson_object_property_value_get_vec3(const bson_object* object, const char* name, vec3* out_value)
{
    if (!out_value)
        return false;

    const char* str = bson_object_property_value_get_string_reference(object, name, "vec3");
    return string_to_vec3(str, out_value);
}

b8 bson_object_property_value_get_vec2(const bson_object* object, const char* name, vec2* out_value)
{
    if (!out_value)
        return false;

    const char* str = bson_object_property_value_get_string_reference(object, name, "vec2");
    return string_to_vec2(str, out_value);
}

b8 bson_object_property_value_get_string_as_bname(const bson_object* object, const char* name, bname* out_value)
{
    if (!out_value)
        return false;

    const char* str = bson_object_property_value_get_string_reference(object, name, "bname");
    if (!str)
        return false;

    *out_value = bname_create(str);
    return true;
}

b8 bson_object_property_value_get_string_as_bstring_id(const bson_object* object, const char* name, bstring_id* out_value)
{
    if (!out_value)
        return false;

    const char* str = bson_object_property_value_get_string_reference(object, name, "bstring_id");
    if (!str)
        return false;

    *out_value = bstring_id_create(str);
    return true;
}

b8 bson_object_property_value_get_object(const bson_object* object, const char* name, bson_object* out_value)
{
    i32 index = bson_object_property_index_get(object, name);
    if (index == -1)
        return false;

    bson_property* p = &object->properties[index];
    // Allow both object and array here
    if (p->type != BSON_PROPERTY_TYPE_OBJECT)
    {
        BERROR("Error parsing value as '%s' - property is instead stored as (type='%s')", bson_property_type_to_string(BSON_PROPERTY_TYPE_OBJECT), bson_property_type_to_string(p->type));
        return false;
    }

    *out_value = p->value.o;
    return true;
}

b8 bson_object_property_value_get_array(const bson_object* object, const char* name, bson_array* out_value)
{
    i32 index = bson_object_property_index_get(object, name);
    if (index == -1)
        return false;

    bson_property* p = &object->properties[index];
    // Allow both object and array here
    if (p->type != BSON_PROPERTY_TYPE_ARRAY)
    {
        BERROR("Error parsing value as '%s' - property is instead stored as (type='%s')", bson_property_type_to_string(BSON_PROPERTY_TYPE_ARRAY), bson_property_type_to_string(p->type));
        return false;
    }

    *out_value = p->value.o;
    return true;
}

bson_property bson_object_property_create(const char* name)
{
    bson_property obj = {0};
    obj.type = BSON_PROPERTY_TYPE_OBJECT;
    obj.name = INVALID_BSTRING_ID;
    if (name)
    {
        obj.name = bstring_id_create(name);
#ifdef BISMUTH_DEBUG
        obj.name_str = string_duplicate(name);
#endif
    }
    obj.value.o.type = BSON_OBJECT_TYPE_OBJECT;
    obj.value.o.properties = darray_create(bson_property);

    return obj;
}

bson_property bson_array_property_create(const char* name)
{
    bson_property arr = {0};
    arr.type = BSON_PROPERTY_TYPE_ARRAY;
    arr.name = INVALID_BSTRING_ID;
    if (name)
    {
        arr.name = bstring_id_create(name);
#ifdef BISMUTH_DEBUG
        arr.name_str = string_duplicate(name);
#endif
    }
    arr.value.o.type = BSON_OBJECT_TYPE_ARRAY;
    arr.value.o.properties = darray_create(bson_property);

    return arr;
}

bson_object bson_object_create(void)
{
    bson_object new_obj = {0};
    new_obj.type = BSON_OBJECT_TYPE_OBJECT;
    new_obj.properties = 0;
    return new_obj;
}

bson_array bson_array_create(void)
{
    bson_array new_arr = {0};
    new_arr.type = BSON_OBJECT_TYPE_ARRAY;
    new_arr.properties = 0;
    return new_arr;
}
