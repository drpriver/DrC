//
// Copyright © 2021-2025, David Priver <david@davidpriver.com>
//
#ifndef ALLOCATOR_H
#define ALLOCATOR_H
// size_t
#include <stddef.h>

#ifndef warn_unused

#if defined(__GNUC__) || defined(__clang__)
#define warn_unused __attribute__((warn_unused_result))
#elif defined(_MSC_VER)
#define warn_unused
#else
#define warn_unused
#endif

#endif

#ifdef __clang__
#pragma clang assume_nonnull begin
#else
#ifndef _Nullable
#define _Nullable
#endif
#ifndef _Null_unspecified
#define _Null_unspecified
#endif
#endif

#ifndef MALLOC_FUNC
#if defined(__GNUC__) || defined(__clang__)
#define MALLOC_FUNC __attribute__((malloc))
#else
#define MALLOC_FUNC
#endif
#endif

#ifndef force_inline
#if defined(__GNUC__) || defined(__clang__)
#define force_inline static inline __attribute__((always_inline))
#elif defined(_MSC_VER)
#define force_inline static inline __forceinline
#else
#define force_inline static inline
#endif
#endif

#ifdef __clang__
enum AllocatorType: int {
#else
enum AllocatorType {
#endif
    ALLOCATOR_UNSET  = 0,
    // aborts on any usage

    ALLOCATOR_MALLOC,
    // wrapper around malloc, calloc, realloc, free

    ALLOCATOR_ARENA,
    // Allocates from a linear section of memory, falls back to malloc

    ALLOCATOR_NULL,
    // always returns NULL, does not error on free

#ifdef USE_RECORDED_ALLOCATOR
    ALLOCATOR_RECORDED,
    // Stores allocations and sizes, catches double frees, leaks, etc.
#endif

#ifdef USE_TESTING_ALLOCATOR
    ALLOCATOR_TESTING,
    // Like recorded, but uses the global testing allocator.
    // Call testing_allocator_init early on.
    // Can also be configured to fail after a certain number of allocations.
#endif
#ifdef USE_GC_ALLOCATOR
    ALLOCATOR_GC,
#endif
};

typedef struct Allocator Allocator;

struct Allocator {
    enum AllocatorType type;
    // 4 bytes of padding
    void* _data;
};


MALLOC_FUNC
static inline
warn_unused
void*_Nullable
Allocator_alloc(Allocator allocator, size_t size);

MALLOC_FUNC
static inline
warn_unused
void*_Nullable
Allocator_zalloc(Allocator allocator, size_t size);

static inline
warn_unused
void*_Nullable
Allocator_realloc(Allocator allocator, void*_Nullable data, size_t orig_size, size_t size);

static inline
warn_unused
void*_Nullable
Allocator_dupe(Allocator allocator, const void* data, size_t size);

static inline
void
Allocator_free(Allocator allocator, const void*_Nullable data, size_t size);

static inline
int
Allocator_supports_free_all(Allocator a);

static inline
void
Allocator_free_all(Allocator a);

MALLOC_FUNC
static inline
warn_unused
char*_Nullable
Allocator_strndup(Allocator allocator, const char* str, size_t length);

static inline
size_t
Allocator_good_size(Allocator allocator, size_t size);

#ifdef __clang__
#pragma clang assume_nonnull end
#endif

#endif
