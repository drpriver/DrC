#ifndef ALLOCATOR_H
#define ALLOCATOR_H
//
// Copyright © 2021-2026, David Priver <david@davidpriver.com>
//

// size_t
#include <stddef.h>
#include <stdint.h>
#include <limits.h>
#include <assert.h>
#include "../typed_enum.h"

#ifndef warn_unused

#if defined __GNUC__ || defined __clang__
#define warn_unused __attribute__((warn_unused_result))
#elif defined _MSC_VER
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
#if defined __GNUC__ || defined __clang__
#define MALLOC_FUNC __attribute__((malloc))
#else
#define MALLOC_FUNC
#endif
#endif

#ifndef force_inline
#if defined __GNUC__ || defined __clang__ || defined __DRC__
#define force_inline static inline __attribute__((always_inline))
#elif defined _MSC_VER
#define force_inline static inline __forceinline
#else
#define force_inline static inline
#endif
#endif

enum {
    #if defined _MSC_VER && defined _WIN64
    MALLOC_ALIGNMENT = 16,
    #elif defined _MSC_VER && defined _WIN32
    MALLOC_ALIGNMENT = 8,
    #else
    MALLOC_ALIGNMENT = _Alignof(max_align_t),
    #endif
};


enum AllocatorType TYPED_ENUM(uintptr_t){
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
};
TYPEDEF_ENUM(AllocatorType, uintptr_t);

typedef struct Allocator Allocator;

struct Allocator {
    union {
        uintptr_t bits;
        const char*_Null_unspecified tagged_ptr;
        struct {
        #if defined __BYTE_ORDER__ && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
            uintptr_t     ptr: sizeof (uintptr_t){0} * CHAR_BIT - 3;
            AllocatorType type: 3;
        #else
            AllocatorType type: 3;
            uintptr_t     ptr: sizeof (uintptr_t){0} * CHAR_BIT - 3;
        #endif
        };
    };
};

force_inline
void* _Null_unspecified
Allocator_get_ptr(Allocator al){
    return (void*)((uintptr_t)al.ptr << 3);
}
force_inline
Allocator
Allocator_make(AllocatorType type, const void*_Nullable ptr){
    assert(((uintptr_t)ptr & 7) == 0);
    return (Allocator){.type = type, .ptr = (uintptr_t)ptr >> 3};
}

#define ALLOCATOR_SUCCESS_BUT_NO_DATA ((void*)(uintptr_t)1)
// ALLOCATOR_SUCCESS_BUT_NO_DATA
// -----------------------------
// This sentinel value is returned for the APIs that need to return a non-zero
// pointer to indicate success. It is treated as equivalent to `NULL` when used
// as an argument to `Allocator_free()` or `Allocator_re[z]alloc()`.
//

MALLOC_FUNC
static inline
warn_unused
void*_Nullable
Allocator_alloc(Allocator allocator, size_t count, size_t size, size_t alignment);
// Allocator_alloc()
// -----------------
// Allocates uninitialized data, for `count` objects of `size` each, with
// alignment `alignment`.
//
// Returns:
// --------
// `NULL` on overflow or if the allocator cannot meet the request.
// A zero `count` or `size` results in `ALLOCATOR_SUCCESS_BUT_NO_DATA`.
//

MALLOC_FUNC
static inline
warn_unused
void*_Nullable
Allocator_zalloc(Allocator allocator, size_t count, size_t size, size_t alignment);
// Allocator_zalloc()
// ------------------
// Allocates zero-initialized data, for `count` objects of `size` each, with
// alignment `alignment`.
// The size and alignment requirements are the same as Allocator_alloc().
//
// Returns:
// --------
// `NULL` on overflow or if the allocator cannot meet the request.
// A zero `count` or `size` results in `ALLOCATOR_SUCCESS_BUT_NO_DATA`.
//


static inline
warn_unused
void*_Nullable
Allocator_realloc(Allocator allocator, void*_Nullable data, size_t orig_count, size_t count, size_t size, size_t alignment);
// Allocator_realloc()
// -------------------
// Resizes the allocation of a `orig_count`, with size `size` and alignment
// `alignment` to `count`.
// If the resulting allocation is larger, the new extent of the buffer is
// uninitialized
//
// If you are resizing different sized objects, pass 1 for `size` and do the
// arithmetic yourself. Use `"ckdint.h"` helpers to avoid overflow.
// `Allocator_realloc_extra()` can help with the common case of
// a header + trailing data.
//
// Unlike libc realloc, we define the semantics in all cases. They are as follows:
//   - `NULL` is returned on overflow or if the allocator cannot meet the request.
//     The allocation pointed to by `data` will not be changed. It is also returned
//     on detectable misuse of this function, like if `data` is NULL but
//     both `orig_count` and `size` are nonzero.
//   - If `size` is zero, `data` must be `NULL` or `ALLOCATOR_SUCCESS_BUT_NO_DATA`.
//     Either returns `ALLOCATOR_SUCCESS_BUT_NO_DATA` for any original/new counts.
//   - If the original allocation is empty (`orig_count` == 0), then this is the
//     same as calling `Allocator_alloc()`.
//   - If the original allocation is non-empty, but `count` is 0, then this
//     is the same as calling `Allocator_free()`.
//     `ALLOCATOR_SUCCESS_BUT_NO_DATA` will be returned.
//   - Otherwise it has normal `realloc()` semantics.
//
// The size and alignment requirements are the same as Allocator_alloc().
// Changing alignment is not supported.
//
// Returns:
// --------
// `NULL` on overflow or if the allocator cannot meet the request.
// Otherwise either a valid pointer to the newly sized region or
// `ALLOCATOR_SUCCESS_BUT_NO_DATA` for cases documented above.
//
//
static inline
warn_unused
void*_Nullable
Allocator_rezalloc(Allocator allocator, void*_Nullable data, size_t orig_count, size_t count, size_t size, size_t alignment);
// Allocator_rezalloc()
// --------------------
// Like `Allocator_realloc()`, but the new extent of the buffer is zero-initialized.
//

static inline
warn_unused
void*_Nullable
Allocator_dupe(Allocator allocator, const void* data, size_t count, size_t size, size_t alignment);
// Allocator_dupe()
// ----------------
// Allocates like with `Allocator_alloc()` but initializes it with a copy of
// `data` instead of uninit or zero-initialized.
//

static inline
void
Allocator_free(Allocator allocator, const void*_Nullable data, size_t count, size_t size, size_t alignment);
// Allocator_free()
// ----------------
// Releases the memory region pointer to by `data`. `data` may be `NULL` or
// `ALLOCATOR_SUCCESS_BUT_NO_DATA` if `count` or `size` is zero.
// `count`, `size` and `alignment` must be accurate to how the data was allocated.
//

MALLOC_FUNC
static inline
warn_unused
void*_Nullable
Allocator_alloc_extra(Allocator a, size_t count, size_t size, size_t extra, size_t alignment);
// Allocator_alloc_extra()
// -----------------------
// Like `Allocator_alloc()`, but the returned storage has `extra` bytes.
// This is for the common case of header + array or array+sentinel.
// The point of this function is that you overflow is handled for you,
// returning `NULL` instead of a smaller allocation than you expect.
//

MALLOC_FUNC
static inline
warn_unused
void*_Nullable
Allocator_zalloc_extra(Allocator a, size_t count, size_t size, size_t extra, size_t alignment);
// Allocator_zalloc_extra()
// ------------------------
// Like `Allocator_alloc_extra()`, but the returned storage is zero-initialized
//

static inline
warn_unused
void*_Nullable
Allocator_realloc_extra(Allocator a, void*_Nullable data, size_t orig_count, size_t count, size_t size, size_t extra, size_t alignment);
// Allocator_realloc_extra()
// -------------------------
// Like `Allocator_realloc()`, but includes `extra` bytes
// Handles overflow checking for you.

static inline
void
Allocator_free_extra(Allocator a, const void*_Nullable data, size_t count, size_t size, size_t extra, size_t alignment);
// Allocator_free_extra()
// ----------------
// Like `Allocator_free()`, but with `extra` bytes.
//

static inline
_Bool
Allocator_supports_free_all(Allocator a);
// Allocator_supports_free_all()
// -----------------------------
// Returns true if `Allocator_free_all()` can be called and will release all of the
// allocations in the allocator.
//

static inline
void
Allocator_free_all(Allocator a);
// Allocator_free_all()
// --------------------
// Releases all allocations in the allocator.
// This can `abort()` if called with an improper allocator.
// Check the allocators kind of check with `Allocator_supports_free_all()`.

#ifdef __clang__
#pragma clang assume_nonnull end
#endif

#endif
