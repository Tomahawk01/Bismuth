#include "bvar.h"
#include "core/bmemory.h"
#include "core/logger.h"
#include "core/bstring.h"
#include "core/event.h"
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

static void bvar_console_commands_register(void);

b8 bvar_initialize(u64* memory_requirement, void* memory, void* config)
{
    *memory_requirement = sizeof(bvar_system_state);

    if (!memory)
        return true;

    state_ptr = memory;

    bzero_memory(state_ptr, sizeof(bvar_system_state));

    bvar_console_commands_register();

    return true;
}
void bvar_shutdown(void* state)
{
    if (state_ptr)
        bzero_memory(state_ptr, sizeof(bvar_system_state));
}

b8 bvar_int_create(const char* name, i32 value)
{
    if (!state_ptr || !name)
        return false;
    
    for (u32 i = 0; i < BVAR_INT_MAX_COUNT; ++i)
    {
        bvar_int_entry* entry = &state_ptr->ints[i];
        if (entry->name && strings_equali(entry->name, name))
        {
            BERROR("A int bvar named '%s' already exists", name);
            return false;
        }
    }

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

    BERROR("bvar_int_create could not find a free slot to store an entry in");
    return false;
}

b8 bvar_int_get(const char* name, i32* out_value)
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

    BERROR("bvar_int_get could not find a bvar named '%s'", name);
    return false;
}

b8 bvar_int_set(const char* name, i32 value)
{
    if (!state_ptr || !name)
        return false;

    for (u32 i = 0; i < BVAR_INT_MAX_COUNT; ++i)
    {
        bvar_int_entry* entry = &state_ptr->ints[i];
        if (entry->name && strings_equali(name, entry->name))
        {
            entry->value = value;
            event_context context = {0};
            string_ncopy(context.data.c, name, 16);
            event_fire(EVENT_CODE_BVAR_CHANGED, 0, context);
            return true;
        }
    }

    BERROR("bvar_int_set could not find a bvar named '%s'", name);
    return false;
}

static void bvar_console_command_int_create(console_command_context context)
{
    if (context.argument_count != 2)
    {
        BERROR("bvar_console_command_int_create requires a context arg count of 2");
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

    if (!bvar_int_create(name, value))
        BERROR("Failed to create int bvar");
}

static void bvar_console_command_int_print(console_command_context context)
{
    if (context.argument_count != 1)
    {
        BERROR("bvar_console_command_int_print requires a context arg count of 1");
        return;
    }

    const char* name = context.arguments[0].value;
    i32 value = 0;
    if (!bvar_int_get(name, &value))
    {
        BERROR("Failed to find bvar called '%s'", name);
        return;
    }

    char val_str[50] = {0};
    string_format(val_str, "%i", value);

    console_write_line(LOG_LEVEL_INFO, val_str);
}

static void bvar_console_command_int_set(console_command_context context)
{
    if (context.argument_count != 2)
    {
        BERROR("bvar_console_command_int_set requires a context arg count of 2");
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

    if (!bvar_int_set(name, value))
        BERROR("Failed to set int bvar called '%s' because it doesn't exist", name);

    char out_str[500] = {0};
    string_format(out_str, "%s = %i", name, value);
    console_write_line(LOG_LEVEL_INFO, val_str);
}

static void bvar_console_command_print_all(console_command_context context)
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

static void bvar_console_commands_register(void)
{
    console_command_register("bvar_create_int", 2, bvar_console_command_int_create);
    console_command_register("bvar_print_int", 1, bvar_console_command_int_print);
    console_command_register("bvar_set_int", 2, bvar_console_command_int_set);
    console_command_register("bvar_print_all", 0, bvar_console_command_print_all);
}
