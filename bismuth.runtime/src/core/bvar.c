#include "bvar.h"

#include "core/event.h"
#include "logger.h"
#include "memory/bmemory.h"
#include "strings/bstring.h"

#include "core/console.h"

typedef struct bvar_entry
{
    bvar_types type;
    const char* name;
    const char* description;
    bvar_value value;
} bvar_entry;

// Max number of bvars
#define BVAR_MAX_COUNT 256

typedef struct bvar_state
{
    bvar_entry values[BVAR_MAX_COUNT];
} bvar_state;

static bvar_state* state_ptr;

static void bvar_console_commands_register(void);

b8 bvar_system_initialize(u64* memory_requirement, struct bvar_state* memory, void* config)
{
    *memory_requirement = sizeof(bvar_state);

    if (!memory)
        return true;

    state_ptr = memory;

    bzero_memory(state_ptr, sizeof(bvar_state));

    bvar_console_commands_register();

    return true;
}

void bvar_system_shutdown(struct bvar_state* state)
{
    if (state)
    {
        // Free resources
        for (u32 i = 0; i < BVAR_MAX_COUNT; ++i)
        {
            bvar_entry* entry = &state->values[i];
            if (entry->name)
                string_free(entry->name);

            // If a string, free it
            if (entry->type == BVAR_TYPE_STRING)
                string_free(entry->value.s);
        }
        bzero_memory(state, sizeof(bvar_state));
    }
    state_ptr = 0;
}

static bvar_entry* get_entry_by_name(bvar_state* state, const char* name)
{
    // Check if bvar exists with the name
    for (u32 i = 0; i < BVAR_MAX_COUNT; ++i)
    {
        bvar_entry* entry = &state->values[i];
        if (entry->name && strings_equali(name, entry->name))
        {
            // Name matches, return
            return entry;
        }
    }

    // No match found. Try getting a new one
    for (u32 i = 0; i < BVAR_MAX_COUNT; ++i)
    {
        bvar_entry* entry = &state->values[i];
        if (!entry->name)
        {
            // No name means this one is free. Set its name here
            entry->name = string_duplicate(name);
            return entry;
        }
    }

    BERROR("Unable to find existing bvar named '%s' and cannot create new bvar because the table has no room left", name);
    return 0;
}

static b8 bvar_entry_set_desc_value(bvar_entry* entry, bvar_types type, const char* description, void* value)
{
    // If a string, value will need to be freed before proceeding
    if (entry->type == BVAR_TYPE_STRING)
    {
        if (entry->value.s)
            string_free(entry->value.s);
    }

    bvar_types old_type = entry->type;

    // Update the type
    entry->type = type;

    // Update the value
    switch (entry->type)
    {
        case BVAR_TYPE_STRING:
            entry->value.s = string_duplicate(value);
            break;
        case BVAR_TYPE_FLOAT:
            entry->value.f = *((f32*)value);
            break;
        case BVAR_TYPE_INT:
            entry->value.i = *((i32*)value);
            break;
        default:
            BFATAL("Trying to set a bvar with an unknown type. This should not happen unless a new type has been added");
            return false;
    }

    // If a description was provided, update it
    if (description)
    {
        if (entry->description)
            string_free(entry->description);
        entry->description = string_duplicate(description);
    }

    // Send out a notification that the variable was changed
    event_context context = {0};
    context.data.custom_data.size = sizeof(bvar_change);
    context.data.custom_data.data = ballocate(context.data.custom_data.size, MEMORY_TAG_UNKNOWN);  // FIXME: event tag
    bvar_change* change_data = context.data.custom_data.data;
    change_data->name = entry->name;
    change_data->new_type = type;
    change_data->old_type = old_type;
    change_data->new_value = entry->value;

    event_fire(EVENT_CODE_BVAR_CHANGED, 0, context);
    return true;
}

b8 bvar_i32_get(const char* name, i32* out_value)
{
    if (!state_ptr || !name)
        return false;

    bvar_entry* entry = get_entry_by_name(state_ptr, name);
    if (!entry)
    {
        BERROR("bvar_int_get could not find a bvar named '%s'", name);
        return false;
    }

    switch (entry->type)
    {
        case BVAR_TYPE_INT:
            // If int, set output as-is
            *out_value = entry->value.i;
            return true;
        case BVAR_TYPE_FLOAT:
            // For float, just cast it, but warn about truncation
            BWARN("The bvar '%s' is of type f32 but its value was requested as i32. This will result in a truncated value. Get the value as a float instead", name);
            *out_value = (i32)entry->value.f;
            return true;
        case BVAR_TYPE_STRING:
            if (!string_to_i32(entry->value.s, out_value))
            {
                BERROR("The bvar '%s' is of type string and could not successfully be parsed to i32. Get the value as a string instead", name);
                return false;
            }
            return true;
        default:
            BERROR("The bvar '%s' is was found but is of an unknown type. This means an unsupported type exists or indicates memory corruption", name);
            return false;
    }
}

b8 bvar_i32_set(const char* name, const char* desc, i32 value)
{
    if (!state_ptr || !name)
        return false;

    bvar_entry* entry = get_entry_by_name(state_ptr, name);
    if (!entry)
        return false;

    // Create/set the bvar
    b8 result = bvar_entry_set_desc_value(entry, BVAR_TYPE_INT, desc, &value);
    if (!result)
        BERROR("Failed to set bvar entry for bvar named '%s'. See logs for details", name);

    return result;
}

b8 bvar_f32_get(const char* name, f32* out_value)
{
    if (!state_ptr || !name)
        return false;

    bvar_entry* entry = get_entry_by_name(state_ptr, name);
    if (!entry)
    {
        BERROR("bvar_int_get could not find a bvar named '%s'", name);
        return false;
    }

    switch (entry->type)
    {
        case BVAR_TYPE_INT:
            BWARN("The bvar '%s' is of type i32 but its value was requested as f32. It is recommended to get the value as int instead", name);
            *out_value = (f32)entry->value.i;
            return true;
        case BVAR_TYPE_FLOAT:
            *out_value = entry->value.f;
            return true;
        case BVAR_TYPE_STRING:
            if (!string_to_f32(entry->value.s, out_value))
            {
                BERROR("The bvar '%s' is of type string and could not successfully be parsed to f32. Get the value as a string instead", name);
                return false;
            }
            return true;
        default:
            BERROR("The bvar '%s' is was found but is of an unknown type. This means an unsupported type exists or indicates memory corruption", name);
            return false;
    }
}

b8 bvar_f32_set(const char* name, const char* desc, f32 value)
{
    if (!state_ptr || !name)
        return false;

    bvar_entry* entry = get_entry_by_name(state_ptr, name);
    if (!entry)
        return false;

    // Create/set the bvar
    b8 result = bvar_entry_set_desc_value(entry, BVAR_TYPE_FLOAT, desc, &value);
    if (!result)
        BERROR("Failed to set bvar entry for bvar named '%s'. See logs for details", name);

    return result;
}

const char* bvar_string_get(const char* name)
{
    if (!state_ptr || !name)
        return 0;

    bvar_entry* entry = get_entry_by_name(state_ptr, name);
    if (!entry)
    {
        BERROR("bvar_int_get could not find a bvar named '%s'", name);
        return 0;
    }

    switch (entry->type)
    {
        case BVAR_TYPE_INT:
            BWARN("The bvar '%s' is of type i32 but its value was requested as string. It is recommended to get the value as int instead", name);
            return i32_to_string(entry->value.i);
        case BVAR_TYPE_FLOAT:
            BWARN("The bvar '%s' is of type i32 but its value was requested as string. It is recommended to get the value as float instead", name);
            return f32_to_string(entry->value.f);
        case BVAR_TYPE_STRING:
            return string_duplicate(entry->value.s);
        default:
            BERROR("The bvar '%s' is was found but is of an unknown type. This means an unsupported type exists or indicates memory corruption", name);
            return 0;
    }
}

b8 bvar_string_set(const char* name, const char* desc, const char* value)
{
    if (!state_ptr || !name)
        return false;

    bvar_entry* entry = get_entry_by_name(state_ptr, name);
    if (!entry)
        return false;

    // Create/set the bvar
    b8 result = bvar_entry_set_desc_value(entry, BVAR_TYPE_STRING, desc, &value);
    if (!result)
        BERROR("Failed to set bvar entry for bvar named '%s'. See logs for details", name);

    return result;
}

static void bvar_print(bvar_entry* entry, b8 include_name)
{
    if (!entry) {
        char name_equals[512] = {0};
        if (include_name)
            string_format_unsafe(name_equals, "%s = ", entry->name);

        switch (entry->type)
        {
            case BVAR_TYPE_INT:
                BINFO("%s%i", name_equals, entry->value.i);
                break;
            case BVAR_TYPE_FLOAT:
                BINFO("%s%f", name_equals, entry->value.f);
                break;
            case BVAR_TYPE_STRING:
                BINFO("%s%s", name_equals, entry->value.s);
                break;
            default:
                BERROR("bvar '%s' has an unknown type. Possible corruption?");
                break;
        }
    }
}

static void bvar_console_command_print(console_command_context context)
{
    if (context.argument_count != 1)
    {
        BERROR("bvar_console_command_print requires a context arg count of 1");
        return;
    }

    const char* name = context.arguments[0].value;
    bvar_entry* entry = get_entry_by_name(state_ptr, name);
    if (!entry)
    {
        BERROR("Unable to find bvar named '%s'", name);
        return;
    }

    bvar_print(entry, false);
}

static void bvar_set_by_str(const char* name, const char* value_str, const char* desc, bvar_types type)
{
    switch (type)
    {
        case BVAR_TYPE_INT:
        {
            // Try to convert to int and set the value
            i32 value = 0;
            if (!string_to_i32(value_str, &value))
            {
                BERROR("Failed to convert argument 1 to i32: '%s'", value_str);
                return;
            }
            if (!bvar_i32_set(name, desc, value))
            {
                BERROR("Failed to set int bvar called '%s'. See logs for details", name);
                return;
            }
            // Print out the result to the console
            BINFO("%s = %i", name, value);
        } break;
        case BVAR_TYPE_FLOAT:
        {
            // Try to convert to float and set the value
            f32 value = 0;
            if (!string_to_f32(value_str, &value))
            {
                BERROR("Failed to convert argument 1 to f32: '%s'", value_str);
                return;
            }
            if (!bvar_f32_set(name, desc, value))
            {
                BERROR("Failed to set float bvar called '%s'. See logs for details", name);
                return;
            }
            // Print out the result to the console
            BINFO("%s = %f", name, value);
        } break;
        case BVAR_TYPE_STRING:
        {
            // Set as string.
            if (!bvar_string_set(name, desc, value_str))
            {
                BERROR("Failed to set string bvar called '%s'. See logs for details", name);
                return;
            }
            // Print out the result to the console
            BINFO("%s = '%s'", name, value_str);
        } break;
        default:
            BERROR("Unable to set bvar of unknown type: %u", type);
            break;
    }
}

static void bvar_console_command_i32_set(console_command_context context)
{
    // NOTE: argument count is verified by the console
    const char* name = context.arguments[0].value;
    const char* val_str = context.arguments[1].value;
    // TODO: Description support
    bvar_set_by_str(name, val_str, 0, BVAR_TYPE_INT);
}

static void bvar_console_command_f32_set(console_command_context context)
{
    // NOTE: argument count is verified by the console
    const char* name = context.arguments[0].value;
    const char* val_str = context.arguments[1].value;
    // TODO: Description support
    bvar_set_by_str(name, val_str, 0, BVAR_TYPE_FLOAT);
}

static void bvar_console_command_string_set(console_command_context context)
{
    // NOTE: argument count is verified by the console
    const char* name = context.arguments[0].value;
    const char* val_str = context.arguments[1].value;
    // TODO: Description support
    bvar_set_by_str(name, val_str, 0, BVAR_TYPE_STRING);
}

static void bvar_console_command_print_all(console_command_context context)
{
    // All bvars
    for (u32 i = 0; i < BVAR_MAX_COUNT; ++i)
    {
        bvar_entry* entry = &state_ptr->values[i];
        if (entry->name)
        {
            char val_str[1024] = {0};
            switch (entry->type)
            {
                case BVAR_TYPE_INT:
                    string_format_unsafe(val_str, "i32 %s = %i, desc='%s'", entry->name, entry->value.i, entry->description ? entry->description : "");
                    break;
                case BVAR_TYPE_FLOAT:
                    string_format_unsafe(val_str, "f32 %s = %f, desc='%s'", entry->name, entry->value.f, entry->description ? entry->description : "");
                    break;
                case BVAR_TYPE_STRING:
                    string_format_unsafe(val_str, "str %s = '%s', desc='%s'", entry->name, entry->value.s, entry->description ? entry->description : "");
                    break;
                default:
                    // Unknown type found. Bleat about it, but try printing it out anyway
                    console_write(LOG_LEVEL_WARN, "bvar of unknown type found. Possible corruption?");
                    string_format_unsafe(val_str, "??? %s = ???, desc='%s'", entry->name, entry->description ? entry->description : "");
                    break;
            }
            console_write(LOG_LEVEL_INFO, val_str);
        }
    }
}

static void bvar_console_commands_register(void)
{
    // Print a var by name
    console_command_register("bvar_print", 1, bvar_console_command_print);
    // Print all bvars
    console_command_register("bvar_print_all", 0, bvar_console_command_print_all);

    // Create/set an int-type bvar by name
    console_command_register("bvar_set_int", 2, bvar_console_command_i32_set);
    console_command_register("bvar_set_i32", 2, bvar_console_command_i32_set);  // alias
    // Create/set a float-type bvar by name
    console_command_register("bvar_set_float", 2, bvar_console_command_f32_set);
    console_command_register("bvar_set_f32", 2, bvar_console_command_f32_set);  // alias
    // Create/set a string-type bvar by name
    console_command_register("bvar_set_string", 2, bvar_console_command_string_set);
}
