//
// Copyright © 2021-2025, David Priver <david@davidpriver.com>
//
#ifndef MARRAY_H
#define MARRAY_H
//
// Marray.h
// --------
// Usage:
// ------
//   #define MARRAY_T int
//   #include "Marray.h"
//
// ------
// Will generate a resizable dynamically allocated array type, using allocators
// and corresponding functions to go along with it.
//
// By default it will generate both declarations and definitions of the data
// types and the functions. Two macros control what is generated
//
//   #define MARRAY_DECL_ONLY
//   #define MARRAY_IMPL_ONLY
//
// MARRAY_DECL_ONLY means to only declare the data type and forward declare
// the functions.
//
// MARRAY_IMPL_ONLY means to only provide the function definitions.
//
// This macro defaults to static inline if not set.
//   #define MARRAY_LINKAGE static inline
//
// Define it to extern, extern inline, dllimport, whatever you need if you want.
//

#include <stddef.h> // size_t
#include <string.h> // memmove, memcpy, memset
#include <assert.h> // assert
#include "bit_util.h" //
#include "Allocators/allocator.h" // Allocator

#if defined(__GNUC__) || defined(__clang__)
#define ma_memmove __builtin_memmove
#define ma_memcpy __builtin_memcpy
#else
#define ma_memmove memmove
#define ma_memcpy memcpy
#endif

static inline
size_t
marray_resize_to_some_weird_number(size_t x){
//
// If given a power of two number, gives that number roughly * 1.5
// Any other number will give the next largest power of 2.
// This leads to a growth rate of sort of sqrt(2)
//
#if UINTPTR_MAX != 0xFFFFFFFF
    _Static_assert(sizeof(size_t) == 8, "");
    _Static_assert(sizeof(size_t) == sizeof(unsigned long long), "fuu");
    if(x < 2)
        return 2;
    if(x < 4)
        return 4;
    if(x == 4)
        return 8;
    if(x <= 8)
        return 16;
    // grow by factor of approx sqrt(2)
    // I have no idea if this is ideal, but it has a nice elegance to it
    int cnt = popcount_64(x);
    size_t result;
    if(cnt == 1){
        result =  x | (x >> 1);
    }
    else {
        int clz = clz_64(x);
        result = 1ull << (64 - clz);
    }
    return result;
#else
    _Static_assert(sizeof(size_t) == sizeof(unsigned), "fuu");
    if(x < 2)
        return 2;
    if(x < 4)
        return 4;
    if(x == 4)
        return 8;
    if(x <= 8)
        return 16;
    // grow by factor of approx sqrt(2)
    // I have no idea if this is ideal, but it has a nice elegance to it
    int cnt = popcount_32(x);
    size_t result;
    if(cnt == 1){
        result =  x | (x >> 1);
    }
    else {
        int clz = clz_32(x);
        result = 1u << (32 - clz);
    }
    return result;
#endif
}

#if !defined(likely) && !defined(unlikely)
#if defined(__GNUC__) || defined(__clang__)
#define likely(x)      __builtin_expect(!!(x), 1)
#define unlikely(x)    __builtin_expect(!!(x), 0)
#else
#define likely(x) (x)
#define unlikely(x) (x)
#endif
#endif

#ifndef warn_unused
#if defined(__GNUC__) || defined(__clang__)
#define warn_unused __attribute__((warn_unused_result))
#elif defined(_MSC_VER)
#define warn_unused
#else
#define warn_unused
#endif
#endif

// Macros require level of indirection
#define MARRAYIMPL(meth, type) ma##_##meth##__##type
#define Marray(type) MarrayI(type)
#define MarrayI(type) ma__##type
#define ma_push(type) MARRAYIMPL(push, type)
#define ma_cleanup(type) MARRAYIMPL(cleanup, type)
#define ma_ensure_total(type) MARRAYIMPL(ensure_total, type)
#define ma_ensure_additional(type) MARRAYIMPL(ensure_additional, type)
#define ma_extend(type) MARRAYIMPL(extend, type)
#define ma_insert(type) MARRAYIMPL(insert, type)
#define ma_remove_at(type) MARRAYIMPL(remove_at, type)
#define ma_alloc(type) MARRAYIMPL(alloc, type)
#define ma_zalloc(type) MARRAYIMPL(zalloc, type)
#define ma_alloc_index(type) MARRAYIMPL(alloc_index, type)

//
// MARRAY_FOR_EACH
// ----------------
// Convenience macro to loop over an marray. Only use if you will not resize
// the marray.
//
// iter will be a pointer to the current element.
//
// Note that it evaluates `marray` at least twice, sometimes three times.
// There's not much getting around that without requiring extra braces.
//
// Usage:
//    Marray(some_type) marray = {};
//    ... fill marray with stuff ...
//    MARRAY_FOR_EACH(it, marray){
//        some_function(it, 3);
//    }
//
//
#define MARRAY_FOR_EACH(type, iter, marray) \
for(type \
     *iter = (marray).data, \
     *iter##end__ = (marray).data?((marray).data+(marray).count):NULL; \
  iter != iter##end__; \
  ++iter)
//
// NULL UB note
// ------------
// The above looks like it could be simplified, but note that in C it is
// stupidly undefined behavior to add 0 to NULL, so you have to use a ternary
// to check for NULL instead of just having `iterend = marray.data+marray.count`.
// That is legal in C++ though, but you would use range-based-for there.
//

//
// MARRAY_FOR_EACH_VALUE
// ----------------
// Like MARRAY_FOR_EACH, but iter will be a value instead of a pointer.
//
#define MARRAY_FOR_EACH_VALUE(type, iter, marray) \
for(type \
    iter = {0}, \
     *iter##iter__ = (marray).data, \
     *iter##end__ = (marray).data?((marray).data+(marray).count):NULL; \
  iter##iter__ != iter##end__?((void)(iter=*iter##iter__), 1):0; \
  ++iter##iter__)

#define ma_tail(ma) ((ma).data[(ma).count-1])
#define ma_head(ma) ((ma).data[0])

#endif

#ifdef __clang__
#pragma clang assume_nonnull begin
#else
#ifndef _Null_unspecified
#define _Null_unspecified
#endif
#ifndef _Nullable
#define _Nullable
#endif
#ifndef _Nonnull
#define _Nonnull
#endif
#endif

#if defined(MARRAY_IMPL_ONLY) && defined(MARRAY_DECL_ONLY)
#error "Only one of MARRAY_IMPL_ONLY and MARRAY_DECL_ONLY can be defined"
#endif

#ifndef MARRAY_LINKAGE
#define MARRAY_LINKAGE static inline
#endif

#ifndef MARRAY_T
#error "Must define MARRAY_T"
#endif
// slightly less typing in the function signature
#ifndef MARRAY
#define MARRAY Marray(MARRAY_T)
#endif

#ifndef MARRAY_IMPL_ONLY
typedef struct MARRAY MARRAY;
struct MARRAY {
    size_t count; // First so you can pun this structure with small buffers.
    size_t capacity;
    // This will be NULL if capacity is 0, otherwise it is a valid pointer.
    // Labeling that as nullable is too annoying though.
    MARRAY_T*_Null_unspecified data;
};

//
// Allocation Note
// ---------------
// Note: it is easy to tell which functions might allocate - they take an
// allocator as an argument.
//
// Make sure you always pass the same allocator!
//
// If you need to append a lot of items to an marray, it is generally better
// to either use `ma_extend` to insert in bulk, or to do an `ma_ensure_*`
// and then directly write into the buffer yourself by doing something like
// `marray.data[marray.count++] = item;`
//
// ma_push is mostly for convenience.
//
// There is no `ma_pop` as you would need to check the `count` field anyway
// and it would just turn into `item = marray.data[--marray.count];`
//

MARRAY_LINKAGE
warn_unused
int
ma_push(MARRAY_T)(MARRAY*, Allocator, MARRAY_T);
// -----------
// Appends to the end of the marray, reallocating if necessary.
//
// Returns 0 on success and 1 on out of memory.

MARRAY_LINKAGE
void
ma_cleanup(MARRAY_T)(MARRAY*, Allocator);
// --------------
// Frees the array and zeros out the members. The marray can then be re-used.

MARRAY_LINKAGE
warn_unused
int
ma_ensure_total(MARRAY_T)(MARRAY*, Allocator, size_t);
// -------------------
// Makes the marray at least this capacity.
//
// Returns 0 on success and 1 on out of memory.

MARRAY_LINKAGE
warn_unused
int
ma_ensure_additional(MARRAY_T)(MARRAY*, Allocator, size_t);
// ------------------------
// Ensures space for n additional items.
//
// Returns 0 on success, 1 on oom.

//
// ma_extend
// -------------
// Appends the n items at the given pointer to the end of the marray,
// reallocing if necessary.
//
MARRAY_LINKAGE
warn_unused
int
ma_extend(MARRAY_T)(MARRAY*, Allocator, const MARRAY_T*, size_t);

MARRAY_LINKAGE
warn_unused
int
ma_insert(MARRAY_T)(MARRAY*, Allocator, size_t, MARRAY_T);
// --------------
// Inserts the element at the given index, shifting the remaining elements
// backwards.
//
// Returns 0 on success, 1 on oom.

MARRAY_LINKAGE
void
ma_remove_at(MARRAY_T)(MARRAY*, size_t);
// -------------
// Removes an element by index and shifts the remaining elements forward.

MARRAY_LINKAGE
warn_unused
int
ma_alloc(MARRAY_T)(MARRAY*, Allocator, MARRAY_T*_Nullable*_Nonnull);
// ------------
// Writers a pointer via the out param to an uninitialized element at the end
// of the marray, reallocing if space is needed.
// Conceptually similar to push.
//
// Returns 1 on oom, 0 otherwise.
//
MARRAY_LINKAGE
warn_unused
int
ma_zalloc(MARRAY_T)(MARRAY*, Allocator, MARRAY_T*_Nullable*_Nonnull);
// ------------
// like `ma_alloc`, but zeros the memory.
//

//
MARRAY_LINKAGE
warn_unused
int
ma_alloc_index(MARRAY_T)(MARRAY*, Allocator, size_t*);
// ------------------
// Returns an index to an uninitialized element at the end of the marray,
// reallocing if space is needed.
// Conceptually similar to push.
//
// Returns (size_t)-1 on oom.

#endif

#ifndef MARRAY_DECL_ONLY

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-function"

#elif defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"

#endif

#ifndef MARRAY_DEFAULT_CAPACITY
#define MARRAY_DEFAULT_CAPACITY 8
#endif

MARRAY_LINKAGE
warn_unused
int
ma_ensure_additional(MARRAY_T)(MARRAY* marray, Allocator a, size_t n_additional){
    size_t required_capacity = marray->count + n_additional;
    if(marray->capacity >= required_capacity)
        return 0;
    size_t new_capacity;
    if(required_capacity < MARRAY_DEFAULT_CAPACITY)
        new_capacity = MARRAY_DEFAULT_CAPACITY;
    else {
        new_capacity = marray_resize_to_some_weird_number(marray->capacity);
        while(new_capacity < required_capacity) {
            new_capacity = marray_resize_to_some_weird_number(new_capacity);
        }
    }
    size_t old_size = marray->capacity*sizeof(MARRAY_T);
    size_t new_size = new_capacity*sizeof(MARRAY_T);
    void* p = Allocator_realloc(a, marray->data, old_size, new_size);
    if(unlikely(!p))
        return 1;
    marray->data = p;
    marray->capacity = new_capacity;
    return 0;
}

MARRAY_LINKAGE
warn_unused
int
ma_push(MARRAY_T)(MARRAY* marray, Allocator a, MARRAY_T value){
    int err = ma_ensure_additional(MARRAY_T)(marray, a, 1);
    if(unlikely(err))
        return err;
    #if defined(__GNUC__) && !defined(__clang__)
    // False positive diagnostic, this function doesn't read what
    // value points to and the analysis is wrong anyway.
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
    #endif
    marray->data[marray->count++] = value;
    #if defined(__GNUC__) && !defined(__clang__)
    #pragma GCC diagnostic pop
    #endif
    return 0;
}

MARRAY_LINKAGE
warn_unused
int
ma_alloc(MARRAY_T)(MARRAY* marray, Allocator a, MARRAY_T*_Nullable*_Nonnull result){
    int err = ma_ensure_additional(MARRAY_T)(marray, a, 1);
    if(unlikely(err)) return 1;
    *result = &marray->data[marray->count++];
    return 0;
}

MARRAY_LINKAGE
warn_unused
int
ma_zalloc(MARRAY_T)(MARRAY* marray, Allocator a, MARRAY_T*_Nullable*_Nonnull result){
    int err = ma_ensure_additional(MARRAY_T)(marray, a, 1);
    if(unlikely(err)) return 1;
    *result = &marray->data[marray->count++];
    memset(*result, 0, sizeof **result);
    return 0;
}

MARRAY_LINKAGE
warn_unused
int
ma_alloc_index(MARRAY_T)(MARRAY* marray, Allocator a, size_t* result){
    int err = ma_ensure_additional(MARRAY_T)(marray, a, 1);
    if(unlikely(err)) return err;
    *result =  marray->count++;
    return 0;
}

MARRAY_LINKAGE
warn_unused
int
ma_insert(MARRAY_T)(MARRAY* marray, Allocator a, size_t index, MARRAY_T value){
    assert(index < marray->count+1);
    if(index == marray->count){
        return ma_push(MARRAY_T)(marray, a, value);
    }
    int err = ma_ensure_additional(MARRAY_T)(marray, a, 1);
    if(unlikely(err))
        return err;
    size_t n_move = marray->count - index;
    ma_memmove(marray->data+index+1, marray->data+index, n_move*sizeof(marray->data[0]));
    marray->data[index] = value;
    marray->count++;
    return 0;
}

MARRAY_LINKAGE
void
ma_remove_at(MARRAY_T)(MARRAY* marray, size_t index){
    assert(index < marray->count);
    if(index == marray->count-1){
        marray->count--;
        return;
    }
    size_t n_move = marray->count - index - 1;
    ma_memmove(marray->data+index, marray->data+index+1, n_move*(sizeof(marray->data[0])));
    marray->count--;
}

MARRAY_LINKAGE
warn_unused
int
ma_extend(MARRAY_T)(MARRAY* marray, Allocator a, const MARRAY_T* values, size_t n_values){
    int err = ma_ensure_additional(MARRAY_T)(marray, a, n_values);
    if(unlikely(err))
        return err;
    ma_memcpy(marray->data+marray->count, values, n_values*(sizeof(MARRAY_T)));
    marray->count+=n_values;
    return 0;
}

MARRAY_LINKAGE
warn_unused
int
ma_ensure_total(MARRAY_T)(MARRAY* marray, Allocator a, size_t total_capacity){
    if (total_capacity <= marray->capacity)
        return 0;
    size_t old_size = marray->capacity * sizeof(MARRAY_T);
    size_t new_size = total_capacity * sizeof(MARRAY_T);
    void* p = Allocator_realloc(a, marray->data, old_size, new_size);
    if(unlikely(!p))
        return 1;
    marray->capacity = total_capacity;
    marray->data = p;
    return 0;
}

MARRAY_LINKAGE
void
ma_cleanup(MARRAY_T)(MARRAY* marray, Allocator a){
    Allocator_free(a, marray->data, marray->capacity*sizeof(MARRAY_T));
    marray->data = NULL;
    marray->count = 0;
    marray->capacity = 0;
}

#if defined(__clang__) && defined(MARRAY_USE_OVERLOADS)
#define MARRAY_OVERLOAD static inline __attribute__((always_inline)) __attribute__((overloadable))

MARRAY_OVERLOAD
warn_unused
int
ensure_additional(MARRAY* marray, Allocator a, size_t n_additional){
    return ma_ensure_additional(MARRAY_T)(marray, a, n_additional);
}

MARRAY_OVERLOAD
warn_unused
int
push(MARRAY* marray, Allocator a, MARRAY_T value){
    return ma_push(MARRAY_T)(marray, a, value);
}

MARRAY_OVERLOAD
warn_unused
int
alloc(MARRAY* marray, Allocator a, MARRAY_T*_Nullable*_Nonnull result){
    return ma_alloc(MARRAY_T)(marray, a, result);
}

MARRAY_OVERLOAD
warn_unused
int
alloc_index(MARRAY* marray, Allocator a, size_t* result){
    return ma_alloc_index(MARRAY_T)(marray, a, result);
}

MARRAY_OVERLOAD
warn_unused
int
insert(MARRAY* marray, Allocator a, size_t index, MARRAY_T value){
    return ma_insert(MARRAY_T)(marray, a, index, value);
}

MARRAY_OVERLOAD
void
remove_at(MARRAY* marray, size_t index){
    ma_remove_at(MARRAY_T)(marray, index);
}

MARRAY_OVERLOAD
warn_unused
int
extend(MARRAY* marray, Allocator a, const MARRAY_T* values, size_t n_values){
    return ma_extend(MARRAY_T)(marray, a, values, n_values);
}

warn_unused
MARRAY_OVERLOAD
int
ensure_total(MARRAY* marray, Allocator a, size_t total_capacity){
    return ma_ensure_total(MARRAY_T)(marray, a, total_capacity);
}

MARRAY_OVERLOAD
void
cleanup(MARRAY* marray, Allocator a){
    ma_cleanup(MARRAY_T)(marray, a);
}
#endif

#ifdef __clang__
#pragma clang diagnostic pop

#elif defined(__GNUC__)
#pragma GCC diagnostic pop

#endif

#endif

#ifdef MARRAY_IMPL_ONLY
#undef MARRAY_IMPL_ONLY
#endif

#ifdef MARRAY_DECL_ONLY
#undef MARRAY_DECL_ONLY
#endif

#ifdef MARRAY_LINKAGE
#undef MARRAY_LINKAGE
#endif

#ifdef MARRAY_DEFAULT_CAPACITY
#undef MARRAY_DEFAULT_CAPACITY
#endif

#undef MARRAY

#undef MARRAY_T

#ifdef __clang__
#pragma clang assume_nonnull end
#endif
