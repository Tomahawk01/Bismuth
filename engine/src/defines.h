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
typedef char b8;

// Properly define static assertions
#if defined(__clang__) || defined(__gcc__)
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
#define INVALID_ID 4294967295U

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

#define BCLAMP(value, min, max) (value <= min) ? min : (value >= max) ? max : value

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

// Gets the number of bytes from amount of gibibytes (GiB) (1024*1024*1024)
#define GIBIBYTES(amount) amount * 1024 * 1024 * 1024
// Gets the number of bytes from amount of mebibytes (MiB) (1024*1024)
#define MEBIBYTES(amount) amount * 1024 * 1024
// Gets the number of bytes from amount of kibibytes (KiB) (1024)
#define KIBIBYTES(amount) amount * 1024

// Gets the number of bytes from amount of gigabytes (GB) (1000*1000*1000)
#define GIGABYTES(amount) amount * 1000 * 1000 * 1000
// Gets the number of bytes from amount of megabytes (MB) (1000*1000)
#define MEGABYTES(amount) amount * 1000 * 1000
// Gets the number of bytes from amount of kilobytes (KB) (1000)
#define KILOBYTES(amount) amount * 1000
