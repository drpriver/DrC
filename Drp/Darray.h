//
// Copyright © 2021-2025, David Priver <david@davidpriver.com>
//
#ifndef DARRAY_H
#define DARRAY_H
//
// Darray.h
// --------
// Usage:
// ------
//   #define DARRAY_T int
//   #include "Darray.h"
//
// ------
// Will generate a resizable dynamically allocated array type, using allocators
// and corresponding functions to go along with it.
//
// By default it will generate both declarations and definitions of the data
// types and the functions. Two macros control what is generated
//
//   #define DARRAY_DECL_ONLY
//   #define DARRAY_IMPL_ONLY
//
// DARRAY_DECL_ONLY means to only declare the data type and forward declare
// the functions.
//
// DARRAY_IMPL_ONLY means to only provide the function definitions.
//
// This macro defaults to static inline if not set.
//   #define DARRAY_LINKAGE static inline
//
// Define it to extern, extern inline, dllimport, whatever you need if you want.
//

#include <stddef.h> // size_t
#include <string.h> // memmove, memcpy
#include <assert.h> // assert
#include "bit_util.h" //
#include "Allocators/allocator.h" // Allocator

#if defined(__GNUC__) || defined(__clang__)
#define da_memmove __builtin_memmove
#define da_memcpy __builtin_memcpy
#else
#define da_memmove memmove
#define da_memcpy memcpy
#endif

static inline
size_t
darray_resize_to_some_weird_number(size_t x){
//
// If given a power of two number, gives that number roughly * 1.5
// Any other number will give the next largest power of 2.
// This leads to a growth rate of sort of sqrt(2)
//
#if UINTPTR_MAX != 0xFFFFFFFF
    _Static_assert(sizeof(size_t) == 8, "");
    _Static_assert(sizeof(size_t) == sizeof(unsigned long long), "fuu");
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

#define DARRAYIMPL(meth, type) da##_##meth##__##type // Macros require level of indirection
#define Darray(type) DarrayI(type)
#define DarrayI(type) da__##type
#define da_push(type) DARRAYIMPL(push, type)
#define da_cleanup(type) DARRAYIMPL(cleanup, type)
#define da_ensure_total(type) DARRAYIMPL(ensure_total, type)
#define da_ensure_additional(type) DARRAYIMPL(ensure_additional, type)
#define da_extend(type) DARRAYIMPL(extend, type)
#define da_insert(type) DARRAYIMPL(insert, type)
#define da_remove_at(type) DARRAYIMPL(remove_at, type)
#define da_alloc(type) DARRAYIMPL(alloc, type)
#define da_alloc_index(type) DARRAYIMPL(alloc_index, type)

//
// DARRAY_FOR_EACH
// ----------------
// Convenience macro to loop over an darray. Only use if you will not resize
// the darray.
//
// iter will be a pointer to the current element.
//
// Note that it evaluates `darray` at least twice, sometimes three times.
// There's not much getting around that without requiring extra braces.
//
// Usage:
//    Darray(some_type) darray = {};
//    ... fill darray with stuff ...
//    DARRAY_FOR_EACH(it, darray){
//        some_function(it, 3);
//    }
//
//
#define DARRAY_FOR_EACH(type, iter, darray) \
for(type \
     *iter = (darray).data, \
     *iter##end__ = (darray).data?((darray).data+(darray).count):NULL; \
  iter != iter##end__; \
  ++iter)
//
// NULL UB note
// ------------
// The above looks like it could be simplified, but note that in C it is
// stupidly undefined behavior to add 0 to NULL, so you have to use a ternary
// to check for NULL instead of just having `iterend = darray.data+darray.count`.
// That is legal in C++ though, but you would use range-based-for there.
//

//
// DARRAY_FOR_EACH_VALUE
// ----------------
// Like DARRAY_FOR_EACH, but iter will be a value instead of a pointer.
//
#define DARRAY_FOR_EACH_VALUE(type, iter, darray) \
for(type \
    iter = {0}, \
     *iter##iter__ = (darray).data, \
     *iter##end__ = (darray).data?((darray).data+(darray).count):NULL; \
  iter##iter__ != iter##end__?(iter=*iter##iter__, 1):0; \
  ++iter##iter__)

#define da_tail(da) ((da).data[(da).count-1])
#define da_head(da) ((da).data[0])

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
#endif

#if defined(DARRAY_IMPL_ONLY) && defined(DARRAY_DECL_ONLY)
#error "Only one of DARRAY_IMPL_ONLY and DARRAY_DECL_ONLY can be defined"
#endif

#ifndef DARRAY_LINKAGE
#define DARRAY_LINKAGE static inline
#endif

#ifndef DARRAY_T
#error "Must define DARRAY_T"
#endif

#define DARRAY Darray(DARRAY_T) // slightly less typing in the function signature

#ifndef DARRAY_IMPL_ONLY
typedef struct DARRAY DARRAY;
struct DARRAY {
    size_t count; // First so you can pun this structure with small buffers.
    size_t capacity;
    // This will be NULL if capacity is 0, otherwise it is a valid pointer.
    // Labeling that as nullable is too annoying though.
    DARRAY_T*_Null_unspecified data;
    Allocator a;
};

//
// Allocation Note
// ---------------
// Note: it is easy to tell which functions might allocate - they take an
// allocator as an argument.
//
// Make sure you always pass the same allocator!
//
// If you need to append a lot of items to an darray, it is generally better
// to either use `da_extend` to insert in bulk, or to do an `da_ensure_*`
// and then directly write into the buffer yourself by doing something like
// `darray.data[darray.count++] = item;`
//
// da_push is mostly for convenience.
//
// There is no `da_pop` as you would need to check the `count` field anyway
// and it would just turn into `item = darray.data[--darray.count];`
//

DARRAY_LINKAGE
warn_unused
int
da_push(DARRAY_T)(DARRAY*, DARRAY_T);
// -----------
// Appends to the end of the darray, reallocating if necessary.
//
// Returns 0 on success and 1 on out of memory.

DARRAY_LINKAGE
void
da_cleanup(DARRAY_T)(DARRAY*);
// --------------
// Frees the array and zeros out the members. The darray can then be re-used.

DARRAY_LINKAGE
warn_unused
int
da_ensure_total(DARRAY_T)(DARRAY*, size_t);
// -------------------
// Makes the darray at least this capacity.
//
// Returns 0 on success and 1 on out of memory.

DARRAY_LINKAGE
warn_unused
int
da_ensure_additional(DARRAY_T)(DARRAY*, size_t);
// ------------------------
// Ensures space for n additional items.
//
// Returns 0 on success, 1 on oom.

//
// da_extend
// -------------
// Appends the n items at the given pointer to the end of the darray,
// reallocing if necessary.
//
DARRAY_LINKAGE
warn_unused
int
da_extend(DARRAY_T)(DARRAY*, const DARRAY_T*, size_t);

DARRAY_LINKAGE
warn_unused
int
da_insert(DARRAY_T)(DARRAY*, size_t, DARRAY_T);
// --------------
// Inserts the element at the given index, shifting the remaining elements
// backwards.
//
// Returns 0 on success, 1 on oom.

DARRAY_LINKAGE
void
da_remove_at(DARRAY_T)(DARRAY*, size_t);
// -------------
// Removes an element by index and shifts the remaining elements forward.

DARRAY_LINKAGE
warn_unused
int
da_alloc(DARRAY_T)(DARRAY*, DARRAY_T*_Nullable*_Nonnull);
// ------------
// Writers a pointer via the out param to an uninitialized element at the end
// of the darray, reallocing if space is needed.
// Conceptually similar to push.
//
// Returns 1 on oom, 0 otherwise.

//
DARRAY_LINKAGE
warn_unused
int
da_alloc_index(DARRAY_T)(DARRAY*, size_t*);
// ------------------
// Returns an index to an uninitialized element at the end of the darray,
// reallocing if space is needed.
// Conceptually similar to push.
//
// Returns (size_t)-1 on oom.

#endif

#ifndef DARRAY_DECL_ONLY

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-function"

#elif defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"

#endif

DARRAY_LINKAGE
warn_unused
int
da_ensure_additional(DARRAY_T)(DARRAY* darray, size_t n_additional){
    size_t required_capacity = darray->count + n_additional;
    if(darray->capacity >= required_capacity)
        return 0;
    size_t new_capacity;
    if(required_capacity < 8)
        new_capacity = 8;
    else {
        new_capacity = darray_resize_to_some_weird_number(darray->capacity);
        while(new_capacity < required_capacity) {
            new_capacity = darray_resize_to_some_weird_number(new_capacity);
        }
    }
    size_t old_size = darray->capacity*sizeof(DARRAY_T);
    size_t new_size = new_capacity*sizeof(DARRAY_T);
    void* p = Allocator_realloc(darray->a, darray->data, old_size, new_size);
    if(unlikely(!p))
        return 1;
    darray->data = p;
    darray->capacity = new_capacity;
    return 0;
}

DARRAY_LINKAGE
warn_unused
int
da_push(DARRAY_T)(DARRAY* darray, DARRAY_T value){
    int err = da_ensure_additional(DARRAY_T)(darray, 1);
    if(unlikely(err))
        return err;
    darray->data[darray->count++] = value;
    return 0;
}

DARRAY_LINKAGE
warn_unused
int
da_alloc(DARRAY_T)(DARRAY* darray, DARRAY_T*_Nullable*_Nonnull result){
    int err = da_ensure_additional(DARRAY_T)(darray, 1);
    if(unlikely(err)) return 1;
    *result = &darray->data[darray->count++];
    return 0;
}

DARRAY_LINKAGE
warn_unused
int
da_alloc_index(DARRAY_T)(DARRAY* darray, size_t* result){
    int err = da_ensure_additional(DARRAY_T)(darray, 1);
    if(unlikely(err)) return err;
    *result =  darray->count++;
    return 0;
}

DARRAY_LINKAGE
warn_unused
int
da_insert(DARRAY_T)(DARRAY* darray, size_t index, DARRAY_T value){
    assert(index < darray->count+1);
    if(index == darray->count){
        return da_push(DARRAY_T)(darray, value);
    }
    int err = da_ensure_additional(DARRAY_T)(darray, 1);
    if(unlikely(err))
        return err;
    size_t n_move = darray->count - index;
    da_memmove(darray->data+index+1, darray->data+index, n_move*sizeof(darray->data[0]));
    darray->data[index] = value;
    darray->count++;
    return 0;
}

DARRAY_LINKAGE
void
da_remove_at(DARRAY_T)(DARRAY* darray, size_t index){
    assert(index < darray->count);
    if(index == darray->count-1){
        darray->count--;
        return;
    }
    size_t n_move = darray->count - index - 1;
    da_memmove(darray->data+index, darray->data+index+1, n_move*(sizeof(darray->data[0])));
    darray->count--;
}

DARRAY_LINKAGE
warn_unused
int
da_extend(DARRAY_T)(DARRAY* darray, const DARRAY_T* values, size_t n_values){
    int err = da_ensure_additional(DARRAY_T)(darray, n_values);
    if(unlikely(err))
        return err;
    da_memcpy(darray->data+darray->count, values, n_values*(sizeof(DARRAY_T)));
    darray->count+=n_values;
    return 0;
}

DARRAY_LINKAGE
warn_unused
int
da_ensure_total(DARRAY_T)(DARRAY* darray, size_t total_capacity){
    if (total_capacity <= darray->capacity)
        return 0;
    size_t old_size = darray->capacity * sizeof(DARRAY_T);
    size_t new_size = total_capacity * sizeof(DARRAY_T);
    void* p = Allocator_realloc(darray->a, darray->data, old_size, new_size);
    if(unlikely(!p))
        return 1;
    darray->capacity = total_capacity;
    darray->data = p;
    return 0;
}

DARRAY_LINKAGE
void
da_cleanup(DARRAY_T)(DARRAY* darray){
    Allocator_free(darray->a, darray->data, darray->capacity*sizeof(DARRAY_T));
    darray->data = NULL;
    darray->count = 0;
    darray->capacity = 0;
}

#if defined(__clang__) && defined(DARRAY_USE_OVERLOADS)
#define DARRAY_OVERLOAD static inline __attribute__((always_inline)) __attribute__((overloadable))

DARRAY_OVERLOAD
warn_unused
int
ensure_additional(DARRAY* darray, size_t n_additional){
    return da_ensure_additional(DARRAY_T)(darray, n_additional);
}

DARRAY_OVERLOAD
warn_unused
int
push(DARRAY* darray, DARRAY_T value){
    return da_push(DARRAY_T)(darray, value);
}

DARRAY_OVERLOAD
warn_unused
int
alloc(DARRAY* darray, DARRAY_T*_Nullable*_Nonnull result){
    return da_alloc(DARRAY_T)(darray, result);
}

DARRAY_OVERLOAD
warn_unused
int
alloc_index(DARRAY* darray, size_t* result){
    return da_alloc_index(DARRAY_T)(darray, result);
}

DARRAY_OVERLOAD
warn_unused
int
insert(DARRAY* darray, size_t index, DARRAY_T value){
    return da_insert(DARRAY_T)(darray, index, value);
}

DARRAY_OVERLOAD
void
remove_at(DARRAY* darray, size_t index){
    da_remove_at(DARRAY_T)(darray, index);
}

DARRAY_OVERLOAD
warn_unused
int
extend(DARRAY* darray, const DARRAY_T* values, size_t n_values){
    return da_extend(DARRAY_T)(darray, values, n_values);
}

warn_unused
DARRAY_OVERLOAD
int
ensure_total(DARRAY* darray, size_t total_capacity){
    return da_ensure_total(DARRAY_T)(darray, total_capacity);
}

DARRAY_OVERLOAD
void
cleanup(DARRAY* darray){
    da_cleanup(DARRAY_T)(darray);
}
#endif

#ifdef __clang__
#pragma clang diagnostic pop

#elif defined(__GNUC__)
#pragma GCC diagnostic pop

#endif

#endif

#ifdef DARRAY_IMPL_ONLY
#undef DARRAY_IMPL_ONLY
#endif

#ifdef DARRAY_DECL_ONLY
#undef DARRAY_DECL_ONLY
#endif

#ifdef DARRAY_LINKAGE
#undef DARRAY_LINKAGE
#endif

#undef DARRAY

#undef DARRAY_T

#ifdef __clang__
#pragma clang assume_nonnull end
#endif
