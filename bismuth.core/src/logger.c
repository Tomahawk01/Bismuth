#include "logger.h"

#include "debug/bassert.h"
#include "memory/bmemory.h"
#include "platform/filesystem.h"
#include "platform/platform.h"
#include "strings/bstring.h"

#include <stdarg.h>

// A console hook function pointer
static PFN_console_write console_hook = 0;

void logger_console_write_hook_set(PFN_console_write hook)
{
    console_hook = hook;
}

void _log_output(log_level level, const char* message, ...)
{
    const char* level_strs[6] = {"[FATAL]: ", "[ERROR]: ", "[WARN]:  ", "[INFO]:  ", "[DEBUG]: ", "[TRACE]: "};

    // Format original message
    __builtin_va_list arg_ptr;
    va_start(arg_ptr, message);
    char* formatted = string_format_v(message, arg_ptr);
    va_end(arg_ptr);

    // Add level and newline around message
    char* out_message = string_format("%s%s\n", level_strs[level], formatted);
    string_free(formatted);

    // If the console hook is defined, make sure to forward messages to it, and it will pass along to consumers.
    // Otherwise the platform layer will be used directly
    if (console_hook)
        console_hook(level, out_message);
    else
        platform_console_write(0, level, out_message);

    string_free(out_message);

    // Trigger "debug break" for fatal errors
    if (level == LOG_LEVEL_FATAL)
        bdebug_break();
}

void report_assertion_failure(const char* expression, const char* message, const char* file, i32 line)
{
    _log_output(LOG_LEVEL_FATAL, "Assertion Failure: %s, message: '%s', in file: %s, line: %d\n", expression, message, file, line);
}
