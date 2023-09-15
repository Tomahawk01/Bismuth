#include "logger.h"
#include "asserts.h"
#include "platform/platform.h"

// TODO: TEMPORARY
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

typedef struct logger_system_state
{
    b8 initialized;
} logger_system_state;

static logger_system_state* state_ptr;

b8 initialize_logging(u64* memory_requirment, void* state)
{
    *memory_requirment = sizeof(logger_system_state);
    if (state == 0)
        return true;

    state_ptr = state;
    state_ptr->initialized = true;

    // TODO: Remove this later
    BFATAL("Test message: %f", 3.14f);
    BERROR("Test message: %f", 3.14f);
    BWARN("Test message: %f", 3.14f);
    BINFO("Test message: %f", 3.14f);
    BDEBUG("Test message: %f", 3.14f);
    BTRACE("Test message: %f", 3.14f);

    // TODO: create log file.
    return true;
}

void shutdown_logging(void* state)
{
    // TODO: cleanup logging/write queued entries.
    state_ptr = 0;
}

void log_output(log_level level, const char* message, ...)
{
    const char* level_strings[6] = {"[FATAL]: ", "[ERROR]: ", "[WARN]:  ", "[INFO]:  ", "[DEBUG]: ", "[TRACE]: "};
    b8 is_error = level < LOG_LEVEL_WARN;

    const i32 msg_length = 32000;
    char out_message[msg_length];
    memset(out_message, 0, sizeof(out_message));

    __builtin_va_list arg_ptr;
    va_start(arg_ptr, message);
    vsnprintf(out_message, msg_length, message, arg_ptr);
    va_end(arg_ptr);

    char out_message2[msg_length];
    sprintf(out_message2, "%s%s\n", level_strings[level], out_message);

    if (is_error)
        platform_console_write_error(out_message2, level);
    else
        platform_console_write(out_message2, level);
}

void report_assertion_failure(const char* expression, const char* message, const char* file, i32 line)
{
    log_output(LOG_LEVEL_FATAL, "Assertion Failure: %s, message: '%s', in file: %s, line: %s\n", expression, message, file, line);
}