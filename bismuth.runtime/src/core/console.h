#pragma once

#include "defines.h"
#include "logger.h"

typedef b8 (*PFN_console_consumer_write)(void* inst, log_level level, const char* message);

typedef struct console_command_argument
{
    const char* value;
} console_command_argument;

typedef struct console_command_context
{
    u8 argument_count;
    console_command_argument* arguments;
    void* listener;
} console_command_context;

typedef void (*PFN_console_command)(console_command_context context);

/** @brief Opaque console system state */
struct console_state;

b8 console_initialize(u64* memory_requirement, struct console_state* memory, void* config);
void console_shutdown(struct console_state* state);

BAPI void console_consumer_register(void* inst, PFN_console_consumer_write callback, u8* out_consumer_id);
BAPI void console_consumer_update(u8 consumer_id, void* inst, PFN_console_consumer_write callback);

void console_write(log_level level, const char* message);

BAPI b8 console_command_register(const char* command, u8 arg_count, void* listener, PFN_console_command func);
BAPI b8 console_command_unregister(const char* command);

BAPI b8 console_command_execute(const char* command);

typedef enum console_object_type
{
    CONSOLE_OBJECT_TYPE_INT32,
    CONSOLE_OBJECT_TYPE_UINT32,
    CONSOLE_OBJECT_TYPE_F32,
    CONSOLE_OBJECT_TYPE_BOOL,
    CONSOLE_OBJECT_TYPE_STRUCT
} console_object_type;

BAPI b8 console_object_register(const char* object_name, void* object, console_object_type type);
BAPI b8 console_object_unregister(const char* object_name);

BAPI b8 console_object_add_property(const char* object_name, const char* property_name, void* property, console_object_type type);
BAPI b8 console_object_remove_property(const char* object_name, const char* property_name);
