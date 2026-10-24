//
// Copyright © 2021-2026, David Priver <david@davidpriver.com>
//
#ifndef ALLOCATOR_C
#define ALLOCATOR_C
#include <stddef.h>
// abort, free, malloc, calloc
#include <stdlib.h>
#include <assert.h>
#include <string.h>

#ifdef __APPLE__
#include <malloc/malloc.h>
#endif
#ifdef _WIN32
#include <malloc.h>
#endif
#include "../ckdint.h"

#include "allocator.h"
#include "mallocator.h"
#include "arena_allocator.h"

#ifdef USE_RECORDED_ALLOCATOR
#include "recording_allocator.h"
#endif

#ifdef USE_TESTING_ALLOCATOR
#include "testing_allocator.h"
#endif

#ifdef __clang__
#pragma clang assume_nonnull begin
#endif

#ifndef unreachable
#if defined __GNUC__ || defined __clang__
#define unreachable() __builtin_unreachable()
#else
#define unreachable() __assume(0)
#endif
#endif

#ifndef ALLOC_BAD
#ifdef DEBUGGING_H
#define ALLOC_BAD() bt(), abort()
#else
#define ALLOC_BAD() abort()
#endif
#endif


static inline
_Bool
Allocator_supports_free_all(Allocator a){
    switch(a.type){
        case ALLOCATOR_UNSET:
            ALLOC_BAD();
        case ALLOCATOR_MALLOC:
        case ALLOCATOR_NULL:
            return 0;
        case ALLOCATOR_ARENA:
            return 1;
    #ifdef USE_RECORDED_ALLOCATOR
        case ALLOCATOR_RECORDED:
            return 1;
    #endif
    #ifdef USE_TESTING_ALLOCATOR
        case ALLOCATOR_TESTING:
            return 1;
    #endif
    }
    ALLOC_BAD();
}

static inline
void
Allocator_free_all(Allocator a){
    switch(a.type){
        case ALLOCATOR_UNSET:
            ALLOC_BAD();
            return;
        case ALLOCATOR_MALLOC:
            ALLOC_BAD();
            return;
        case ALLOCATOR_ARENA:
            ArenaAllocator_free_all(Allocator_get_ptr(a));
            return;
        case ALLOCATOR_NULL:
            ALLOC_BAD();
            return;
    #ifdef USE_RECORDED_ALLOCATOR
        case ALLOCATOR_RECORDED:
            recording_free_all(Allocator_get_ptr(a));
            return;
    #endif
    #ifdef USE_TESTING_ALLOCATOR
        case ALLOCATOR_TESTING:
            testing_free_all(Allocator_get_ptr(a));
            return;
    #endif
    }
    ALLOC_BAD();
}

enum {
    #if defined _MSC_VER && defined _WIN64
    MALLOC_ALIGNMENT = 16,
    #elif defined _MSC_VER && defined _WIN32
    MALLOC_ALIGNMENT = 8,
    #else
    MALLOC_ALIGNMENT = _Alignof(max_align_t),
    #endif
};

MALLOC_FUNC
static inline
warn_unused
// force_inline
void*_Nullable
Allocator_alloc(Allocator a, size_t count, size_t size, size_t alignment){
    if(!count || !size) return ALLOCATOR_SUCCESS_BUT_NO_DATA;
    switch(a.type){
        case ALLOCATOR_UNSET:
            ALLOC_BAD();
            break;
        case ALLOCATOR_MALLOC:
            if(mul_overflow(count, size, &size)) return NULL;
            if(alignment <= MALLOC_ALIGNMENT)
                return malloc(size);
            else {
                #ifdef _WIN32
                return _aligned_malloc(size, alignment);
                #else
                void* result;
                if(posix_memalign(&result, alignment, size)) return NULL;
                return result;
                #endif
            }
        case ALLOCATOR_ARENA:
            return ArenaAllocator_alloc(Allocator_get_ptr(a), count, size, alignment);
        case ALLOCATOR_NULL:
            return NULL;
    #ifdef USE_RECORDED_ALLOCATOR
        case ALLOCATOR_RECORDED:
            return recording_alloc(Allocator_get_ptr(a), count, size, alignment);
    #endif
    #ifdef USE_TESTING_ALLOCATOR
        case ALLOCATOR_TESTING:
            return testing_alloc(Allocator_get_ptr(a), count, size, alignment);
    #endif
    }
    ALLOC_BAD();
    unreachable();
}

MALLOC_FUNC
static inline
warn_unused
// force_inline
void*_Nullable
Allocator_zalloc(Allocator a, size_t count, size_t size, size_t alignment){
    if(!count || !size) return ALLOCATOR_SUCCESS_BUT_NO_DATA;
    switch(a.type){
        case ALLOCATOR_UNSET:
            ALLOC_BAD();
            break;
        case ALLOCATOR_MALLOC:
            if(mul_overflow(count, size, &size)) return NULL;
            if(alignment <= MALLOC_ALIGNMENT)
                return calloc(1, size);
            else {
                void *result;
                #ifdef _WIN32
                    result = _aligned_malloc(size, alignment);
                #else
                    if(posix_memalign(&result, alignment, size)) return NULL;
                #endif
                if(result) memset(result, 0, size);
                return result;
            }
        case ALLOCATOR_ARENA:
            return ArenaAllocator_zalloc(Allocator_get_ptr(a), count, size, alignment);
        case ALLOCATOR_NULL:
            return NULL;
    #ifdef USE_RECORDED_ALLOCATOR
        case ALLOCATOR_RECORDED:
            return recording_zalloc(Allocator_get_ptr(a), count, size, alignment);
    #endif
    #ifdef USE_TESTING_ALLOCATOR
        case ALLOCATOR_TESTING:
            return testing_zalloc(Allocator_get_ptr(a), count, size, alignment);
    #endif
    }
    ALLOC_BAD();
    unreachable();
}

static inline
// force_inline
warn_unused
void*_Nullable
Allocator_realloc(Allocator a, void*_Nullable data, size_t orig_count, size_t count, size_t size, size_t alignment){
    _Bool empty = !data || data == ALLOCATOR_SUCCESS_BUT_NO_DATA;
    if(!size) return empty ? ALLOCATOR_SUCCESS_BUT_NO_DATA : NULL;
    if(empty != (orig_count == 0)) return NULL;
    size_t orig_size;
    if(mul_overflow(orig_count, size, &orig_size)) return NULL;
    if(!orig_count){
        if(!count) return ALLOCATOR_SUCCESS_BUT_NO_DATA;
        return Allocator_alloc(a, count, size, alignment);
    }
    if(!count){
        Allocator_free(a, data, orig_count, size, alignment);
        return ALLOCATOR_SUCCESS_BUT_NO_DATA;
    }
    switch(a.type){
        case ALLOCATOR_UNSET:
            ALLOC_BAD();
            break;
        case ALLOCATOR_MALLOC:
            if(mul_overflow(count, size, &size)) return NULL;
            if(alignment <= MALLOC_ALIGNMENT)
                return realloc(data, size);
            else {
                #ifdef _WIN32
                    return _aligned_realloc(data, size, alignment);
                #else
                    void* p = Allocator_alloc(a, 1, size, alignment);
                    if(!p) return NULL;
                    memcpy(p, data, orig_size < size ? orig_size : size);
                    free(data);
                    return p;
                #endif
            }
        case ALLOCATOR_ARENA:
            return ArenaAllocator_realloc(Allocator_get_ptr(a), data, orig_count, count, size, alignment);
        case ALLOCATOR_NULL:
            return NULL;
    #ifdef USE_RECORDED_ALLOCATOR
        case ALLOCATOR_RECORDED:
            return recording_realloc(Allocator_get_ptr(a), data, orig_count, count, size, alignment);
    #endif
    #ifdef USE_TESTING_ALLOCATOR
        case ALLOCATOR_TESTING:
            return testing_realloc(Allocator_get_ptr(a), data, orig_count, count, size, alignment);
    #endif
    }
    ALLOC_BAD();
    unreachable();
}

static inline
warn_unused
void*_Nullable
Allocator_rezalloc(Allocator a, void*_Nullable data, size_t orig_count, size_t count, size_t size, size_t alignment){
    void *result = Allocator_realloc(a, data, orig_count, count, size, alignment);
    if(!result || result == ALLOCATOR_SUCCESS_BUT_NO_DATA) return result;
    if(count > orig_count)
        memset((char*)result + orig_count * size, 0, (count - orig_count) * size);
    return result;
}

static inline
// force_inline
void
Allocator_free(Allocator a, const void*_Nullable data, size_t count, size_t size, size_t alignment){
    if(!data || data == ALLOCATOR_SUCCESS_BUT_NO_DATA)
        return;
    switch(a.type){
        case ALLOCATOR_UNSET:
            ALLOC_BAD();
            return;
        case ALLOCATOR_MALLOC:
        #if defined(__clang__) || defined(__GNUC__)
            #pragma GCC diagnostic push
            #pragma GCC diagnostic ignored "-Wcast-qual"
            #if !defined(__clang__)
                #pragma GCC diagnostic ignored "-Wdiscarded-qualifiers"
            #endif
        #elif defined(_MSC_VER)
            #pragma warning(push)
            #pragma warning(disable: 4090)
        #else
        #endif
        #ifdef _WIN32
            if(alignment <= MALLOC_ALIGNMENT)
                free((void*)data);
            else
                _aligned_free((void*)data);
        #else
            free((void*)data);
        #endif
        #if defined(__clang__) || defined(__GNUC__)
            #pragma GCC diagnostic pop
        #elif defined(_MSC_VER)
            #pragma warning(pop)
        #endif
            return;
        case ALLOCATOR_ARENA:
            ArenaAllocator_free(Allocator_get_ptr(a), data, count, size, alignment);
            return;
        case ALLOCATOR_NULL:
            return;
    #ifdef USE_RECORDED_ALLOCATOR
        case ALLOCATOR_RECORDED:
            recording_free(Allocator_get_ptr(a), data, count, size, alignment);
            return;
    #endif
    #ifdef USE_TESTING_ALLOCATOR
        case ALLOCATOR_TESTING:
            testing_free(Allocator_get_ptr(a), data, count, size, alignment);
            return;
    #endif
    }
    ALLOC_BAD();
}

static inline
warn_unused
// force_inline
void*_Nullable
Allocator_dupe(Allocator allocator, const void* data, size_t count, size_t size, size_t alignment){
    size_t cp_sz;
    if(mul_overflow(count, size, &cp_sz))
        return NULL;
    void* result = Allocator_alloc(allocator, count, size, alignment);
    if(!result || result == ALLOCATOR_SUCCESS_BUT_NO_DATA) return result;
    if(cp_sz) memcpy(result, data, cp_sz);
    return result;
}

static inline
warn_unused
void*_Nullable
Allocator_alloc_extra(Allocator a, size_t count, size_t size, size_t extra, size_t alignment){
    if(fma_overflow(count, size, extra, &extra)) return NULL;
    return Allocator_alloc(a, extra, 1, alignment);
}

static inline
warn_unused
void*_Nullable
Allocator_zalloc_extra(Allocator a, size_t count, size_t size, size_t extra, size_t alignment){
    if(fma_overflow(count, size, extra, &extra)) return NULL;
    return Allocator_zalloc(a, extra, 1, alignment);
}

static inline
warn_unused
void*_Nullable
Allocator_realloc_extra(Allocator a, void*_Nullable data, size_t orig_count, size_t count, size_t size, size_t extra, size_t alignment){
    size_t orig_total, total;
    if(fma_overflow(orig_count, size, extra, &orig_total)
    || fma_overflow(count, size, extra, &total)) return NULL;
    return Allocator_realloc(a, data, orig_total, total, 1, alignment);
}

static inline
void
Allocator_free_extra(Allocator a, const void*_Nullable data, size_t count, size_t size, size_t extra, size_t alignment){
    if(!data || data == ALLOCATOR_SUCCESS_BUT_NO_DATA) return;
    if(fma_overflow(count, size, extra, &extra)){
        ALLOC_BAD();
        return;
    }
    Allocator_free(a, data, extra, 1, alignment);
}

#ifdef __clang__
#pragma clang assume_nonnull end
#endif
#endif
