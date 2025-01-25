#pragma once

#include "defines.h"

// Disable assertions by commenting out the below line
#define BASSERTION_ENABLED

// Always define bdebug_break in case it is ever needed outside assertions (i.e fatal log errors)
// Try via __has_builtin first
#if defined(__has_builtin) && !defined(__ibmxl__)
#    if __has_builtin(__builtin_debugtrap)
#        define bdebug_break() __builtin_debugtrap()
#    elif __has_builtin(__debugbreak)
#        define bdebug_break() __debugbreak()
#    endif
#endif

// If not setup, try the old way
#if !defined(bdebug_break)
#    if defined(__clang__) || defined(__gcc__)
/** @brief Causes a debug breakpoint to be hit */
#        define bdebug_break() __builtin_trap()
#    elif defined(_MSC_VER)
#        include <intrin.h>
/** @brief Causes a debug breakpoint to be hit */
#        define bdebug_break() __debugbreak()
#    else
// Fall back to x86/x86_64
#        define bdebug_break() asm { int 3 }
#    endif
#endif

#ifdef BASSERTION_ENABLED

BAPI void report_assertion_failure(const char* expression, const char* message, const char* file, i32 line);

#define BASSERT(expr)                                                \
    {                                                                \
        if (expr) {                                                  \
        } else {                                                     \
            report_assertion_failure(#expr, "", __FILE__, __LINE__); \
            bdebug_break();                                          \
        }                                                            \
    }

#define BASSERT_MSG(expr, message)                                        \
    {                                                                     \
        if (expr) {                                                       \
        } else {                                                          \
            report_assertion_failure(#expr, message, __FILE__, __LINE__); \
            bdebug_break();                                               \
        }                                                                 \
    }

#if BISMUTH_DEBUG

#define BASSERT_DEBUG(expr)                                          \
    {                                                                \
        if (expr) {                                                  \
        } else {                                                     \
            report_assertion_failure(#expr, "", __FILE__, __LINE__); \
            bdebug_break();                                          \
        }                                                            \
    }
#else
#define BASSERT_DEBUG(expr) // Does nothing at all
#endif

#else
#define BASSERT(expr)              // Does nothing at all
#define BASSERT_MSG(expr, message) // Does nothing at all
#define BASSERT_DEBUG(expr)        // Does nothing at all
#endif
