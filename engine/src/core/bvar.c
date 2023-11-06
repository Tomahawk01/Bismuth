#include "bvar.h"
#include "core/bmemory.h"
#include "core/logger.h"
#include "core/bstring.h"
#include "core/console.h"

typedef struct bvar_int_entry
{
    const char* name;
    i32 value;
} bvar_int_entry;

#define BVAR_INT_MAX_COUNT 200

typedef struct bvar_system_state
{
    bvar_int_entry ints[BVAR_INT_MAX_COUNT];
} bvar_system_state;

static bvar_system_state* state_ptr;

void bvar_register_console_commands();

b8 bvar_initialize(u64* memory_requirement, void* memory)
{
    *memory_requirement = sizeof(bvar_system_state);

    if (!memory)
        return true;

    state_ptr = memory;

    bzero_memory(state_ptr, sizeof(bvar_system_state));

    bvar_register_console_commands();

    return true;
}
void bvar_shutdown(void* state)
{
    if (state_ptr)
        bzero_memory(state_ptr, sizeof(bvar_system_state));
}

b8 bvar_create_int(const char* name, i32 value)
{
    if (!state_ptr || !name)
        return false;

    for (u32 i = 0; i < BVAR_INT_MAX_COUNT; ++i)
    {
        bvar_int_entry* entry = &state_ptr->ints[i];
        if (!entry->name)
        {
            entry->name = string_duplicate(name);
            entry->value = value;
            return true;
        }
    }

    BERROR("bvar_create_int could not find a free slot to store an entry in");
    return false;
}

b8 bvar_get_int(const char* name, i32* out_value)
{
    if (!state_ptr || !name)
        return false;

    for (u32 i = 0; i < BVAR_INT_MAX_COUNT; ++i)
    {
        bvar_int_entry* entry = &state_ptr->ints[i];
        if (entry->name && strings_equali(name, entry->name))
        {
            *out_value = entry->value;
            return true;
        }
    }

    BERROR("bvar_get_int could not find a bvar named '%s'", name);
    return false;
}

b8 bvar_set_int(const char* name, i32 value)
{
    if (!state_ptr || !name)
        return false;

    for (u32 i = 0; i < BVAR_INT_MAX_COUNT; ++i)
    {
        bvar_int_entry* entry = &state_ptr->ints[i];
        if (entry->name && strings_equali(name, entry->name))
        {
            entry->value = value;
            return true;
        }
    }

    BERROR("bvar_set_int could not find a bvar named '%s'", name);
    return false;
}

void bvar_console_command_create_int(console_command_context context)
{
    if (context.argument_count != 2)
    {
        BERROR("bvar_console_command_create_int requires a context arg count of 2");
        return;
    }

    const char* name = context.arguments[0].value;
    const char* val_str = context.arguments[1].value;
    i32 value = 0;
    if (!string_to_i32(val_str, &value))
    {
        BERROR("Failed to convert argument 1 to i32: '%s'", val_str);
        return;
    }

    if (!bvar_create_int(name, value))
        BERROR("Failed to create int bvar");
}

void bvar_console_command_print_int(console_command_context context)
{
    if (context.argument_count != 1)
    {
        BERROR("bvar_console_command_print_int requires a context arg count of 1");
        return;
    }

    const char* name = context.arguments[0].value;
    i32 value = 0;
    if (!bvar_get_int(name, &value))
    {
        BERROR("Failed to find bvar called '%s'", name);
        return;
    }

    char val_str[50] = {0};
    string_format(val_str, "%i", value);

    console_write_line(LOG_LEVEL_INFO, val_str);
}

void bvar_console_command_set_int(console_command_context context)
{
    if (context.argument_count != 2)
    {
        BERROR("bvar_console_command_set_int requires a context arg count of 2");
        return;
    }

    const char* name = context.arguments[0].value;
    const char* val_str = context.arguments[1].value;
    i32 value = 0;
    if (!string_to_i32(val_str, &value))
    {
        BERROR("Failed to convert argument 1 to i32: '%s'", val_str);
        return;
    }

    if (!bvar_set_int(name, value))
        BERROR("Failed to set int bvar called '%s' because it doesn't exist", name);

    char out_str[500] = {0};
    string_format(out_str, "%s = %i", name, value);
    console_write_line(LOG_LEVEL_INFO, val_str);
}

void bvar_console_command_print_all(console_command_context context)
{
    // Int bvars
    for (u32 i = 0; i < BVAR_INT_MAX_COUNT; ++i)
    {
        bvar_int_entry* entry = &state_ptr->ints[i];
        if (entry->name)
        {
            char val_str[500] = {0};
            string_format(val_str, "%s = %i", entry->name, entry->value);
            console_write_line(LOG_LEVEL_INFO, val_str);
        }
    }

    // TODO: Other variable types
}

void bvar_register_console_commands()
{
    console_register_command("bvar_create_int", 2, bvar_console_command_create_int);
    console_register_command("bvar_print_int", 1, bvar_console_command_print_int);
    console_register_command("bvar_set_int", 2, bvar_console_command_set_int);
    console_register_command("bvar_print_all", 0, bvar_console_command_print_all);
}
