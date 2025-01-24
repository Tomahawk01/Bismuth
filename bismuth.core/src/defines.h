#pragma once

// Unsigned int types
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef unsigned long long u64;

// Signed int types
typedef signed char i8;
typedef signed short i16;
typedef signed int i32;
typedef signed long long i64;

// Floating point types
typedef float f32;
typedef double f64;

// Boolean types
typedef int b32;
typedef _Bool b8;

typedef struct range
{
    // offset in bytes
    u64 offset;
    // size in bytes
    u64 size;
} range;

typedef struct range32
{
    // offset in bytes
    i32 offset;
    // size in bytes
    i32 size;
} range32;

// Properly define static assertions
#if defined(__clang__) || defined(__GNUC__)
#define STATIC_ASSERT _Static_assert
#else
#define STATIC_ASSERT static_assert
#endif

// Ensure all types are of the correct size
STATIC_ASSERT(sizeof(u8) == 1, "Expected u8 to be 1 byte.");
STATIC_ASSERT(sizeof(u16) == 2, "Expected u16 to be 2 bytes.");
STATIC_ASSERT(sizeof(u32) == 4, "Expected u32 to be 4 bytes.");
STATIC_ASSERT(sizeof(u64) == 8, "Expected u64 to be 8 bytes.");

STATIC_ASSERT(sizeof(i8) == 1, "Expected i8 to be 1 byte.");
STATIC_ASSERT(sizeof(i16) == 2, "Expected i16 to be 2 bytes.");
STATIC_ASSERT(sizeof(i32) == 4, "Expected i32 to be 4 bytes.");
STATIC_ASSERT(sizeof(i64) == 8, "Expected i64 to be 8 bytes.");

STATIC_ASSERT(sizeof(f32) == 4, "Expected f32 to be 4 bytes.");
STATIC_ASSERT(sizeof(f64) == 8, "Expected f64 to be 8 bytes.");

#define true 1
#define false 0

/**
 * @brief Any id set to this should be considered invalid,
 * and not actually pointing to a real object. 
 */
#define INVALID_ID_U64 18446744073709551615UL
#define INVALID_ID 4294967295U
#define INVALID_ID_U32 INVALID_ID
#define INVALID_ID_U16 65535U
#define INVALID_ID_U8 255U

#define U64_MAX 18446744073709551615UL
#define U32_MAX 4294967295U
#define U16_MAX 65535U
#define U8_MAX 255U
#define U64_MIN 0UL
#define U32_MIN 0U
#define U16_MIN 0U
#define U8_MIN 0U

#define I8_MAX 127
#define I16_MAX 32767
#define I32_MAX 2147483647
#define I64_MAX 9223372036854775807L
#define I8_MIN (-I8_MAX - 1)
#define I16_MIN (-I16_MAX - 1)
#define I32_MIN (-I32_MAX - 1)
#define I64_MIN (-I64_MAX - 1)

// Platform detection
#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) 
#define BPLATFORM_WINDOWS 1
#ifndef _WIN64
#error "64-bit is required on Windows!"
#endif
#else
#error "Your platform is unsupported!"
#endif

#ifdef BEXPORT
// Exports
#ifdef _MSC_VER
#define BAPI __declspec(dllexport)
#else
#define BAPI __attribute__((visibility("default")))
#endif
#else
// Imports
#ifdef _MSC_VER
#define BAPI __declspec(dllimport)
#else
#define BAPI
#endif
#endif

#ifdef _DEBUG
#define BISMUTH_DEBUG
#else
#define BISMUTH_RELEASE
#endif

#define BCLAMP(value, min, max) ((value <= min) ? min : (value >= max) ? max : value)

// Inlining
#if defined(__clang__) || defined(__gcc__)
#define BINLINE __attribute__((always_inline)) inline
#define BNOINLINE __attribute__((noinline))
#elif defined(_MSC_VER)
#define BINLINE __forceinline
#define BNOINLINE __declspec(noinline)
#else
#define BINLINE static inline
#define BNOINLINE
#endif

// Deprecation
#if defined(__clang__) || defined(__gcc__)
/** @brief Mark something (i.e. a function) as deprecated */
#define BDEPRECATED(message) __attribute__((deprecated(message)))
#elif defined(_MSC_VER)
/** @brief Mark something (i.e. a function) as deprecated */
#define BDEPRECATED(message) __declspec(deprecated(message))
#else
#error "Unsupported compiler - don't know how to define deprecations!"
#endif

// Gets the number of bytes from amount of gibibytes (GiB) (1024*1024*1024)
#define GIBIBYTES(amount) (amount * 1024ULL * 1024ULL * 1024ULL)
// Gets the number of bytes from amount of mebibytes (MiB) (1024*1024)
#define MEBIBYTES(amount) (amount * 1024ULL * 1024ULL)
// Gets the number of bytes from amount of kibibytes (KiB) (1024)
#define KIBIBYTES(amount) (amount * 1024ULL)

// Gets the number of bytes from amount of gigabytes (GB) (1000*1000*1000)
#define GIGABYTES(amount) (amount * 1000ULL * 1000ULL * 1000ULL)
// Gets the number of bytes from amount of megabytes (MB) (1000*1000)
#define MEGABYTES(amount) (amount * 1000ULL * 1000ULL)
// Gets the number of bytes from amount of kilobytes (KB) (1000)
#define KILOBYTES(amount) (amount * 1000ULL)

BINLINE u64 get_aligned(u64 operand, u64 granularity)
{
    return ((operand + (granularity - 1)) & ~(granularity - 1));
}

BINLINE range get_aligned_range(u64 offset, u64 size, u64 granularity)
{
    return (range){get_aligned(offset, granularity), get_aligned(size, granularity)};
}

#define BMIN(x, y) (x < y ? x : y)
#define BMAX(x, y) (x > y ? x : y)

/** @brief Indicates if the provided flag is set in the given flags int */
#define FLAG_GET(flags, flag) ((flags & flag) == flag)

/**
 * @brief Sets a flag within the flags int to enabled/disabled
 *
 * @param flags The flags int to write to
 * @param flag The flag to set
 * @param enabled Indicates if the flag is enabled or not
 */
#define FLAG_SET(flags, flag, enabled) (flags = (enabled ? (flags | flag) : (flags & ~flag)))
