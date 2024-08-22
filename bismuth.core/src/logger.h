#pragma once

#include "defines.h"

#define LOG_WARN_ENABLED 1
#define LOG_INFO_ENABLED 1

// Disable debug and trace logging for release builds
#if BRELEASE == 1
#define LOG_DEBUG_ENABLED 0
#define LOG_TRACE_ENABLED 0
#else
#define LOG_DEBUG_ENABLED 1
#define LOG_TRACE_ENABLED 1
#endif

typedef enum log_level
{
    LOG_LEVEL_FATAL = 0,
    LOG_LEVEL_ERROR = 1,
    LOG_LEVEL_WARN = 2,
    LOG_LEVEL_INFO = 3,
    LOG_LEVEL_DEBUG = 4,
    LOG_LEVEL_TRACE = 5
} log_level;

// A function pointer for a console to hook into the logger
typedef void (*PFN_console_write)(log_level level, const char* message);

/**
 * @brief Provides a hook to a console that logging system can forward messages to.
 * If not set, logs go straight to the platform layer.
 * If set, messages go to the hook instead, so it would be responsible for passing
 * messages to the platform layer.
 * NOTE: May only be set once - additional calls will overwrite
 *
 * @param hook Function pointer from the console hook
 */
BAPI void logger_console_write_hook_set(PFN_console_write hook);

BAPI void _log_output(log_level level, const char* message, ...);

// Logs a fatal-level message
#define BFATAL(message, ...) _log_output(LOG_LEVEL_FATAL, message, ##__VA_ARGS__);

#ifndef BERROR
// Logs a error-level message
#define BERROR(message, ...) _log_output(LOG_LEVEL_ERROR, message, ##__VA_ARGS__);
#endif

#if LOG_WARN_ENABLED == 1
// Logs a warning-level message
#define BWARN(message, ...) _log_output(LOG_LEVEL_WARN, message, ##__VA_ARGS__);
#else
// Does nothing when LOG_WARN_ENABLED != 1
#define BWARN(message, ...)
#endif

#if LOG_INFO_ENABLED == 1
// Logs a info-level message
#define BINFO(message, ...) _log_output(LOG_LEVEL_INFO, message, ##__VA_ARGS__);
#else
// Does nothing when LOG_INFO_ENABLED != 1
#define BINFO(message, ...)
#endif

#if LOG_DEBUG_ENABLED == 1
// Logs a debug-level message
#define BDEBUG(message, ...) _log_output(LOG_LEVEL_DEBUG, message, ##__VA_ARGS__);
#else
// Does nothing when LOG_DEBUG_ENABLED != 1
#define BDEBUG(message, ...)
#endif

#if LOG_TRACE_ENABLED == 1
// Logs a trace-level message
#define BTRACE(message, ...) _log_output(LOG_LEVEL_TRACE, message, ##__VA_ARGS__);
#else
// Does nothing when LOG_TRACE_ENABLED != 1
#define BTRACE(message, ...)
#endif