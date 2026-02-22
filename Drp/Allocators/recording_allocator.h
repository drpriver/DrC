//
// Copyright © 2021-2023, David Priver <david@davidpriver.com>
//
#ifndef RECORDING_ALLOCATOR_H
#define RECORDING_ALLOCATOR_H
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "allocator.h"
#include "../debugging.h"

// This stuff is for debugging alloc/free errors.
// Probably worse than using HEAVY_RECORDING and dumping the stack traces
#if 0
#define DEBUG_ALLOCATIONS
#endif

#ifdef DEBUG_ALLOCATIONS
#include <stdio.h>
#define RA_LOGIT(fmt, ...) fprintf(stderr, "%s:%d:" fmt "\n", __func__, __LINE__, ##__VA_ARGS__)
#else
#define RA_LOGIT(...) (void)0
#endif

#ifdef __clang__
#pragma clang assume_nonnull begin
#else
#ifndef _Nullable
#define _Nullable
#endif
#ifndef _Nonnull
#define _Nonnull
#endif
#endif

/*
 * An Allocator wrapper allocator that tracks all allocations
 * and frees done through it so as to provide a free_all function
 * for allocators that don't normally support it.
 *
 * This is currently super unoptimized, but that's ok.
 */

#ifndef _rec_realloc
#ifndef __wasm__
#ifdef USE_SDL_MALLOC
#define _rec_realloc(ptr, orig_size, size) SDL_realloc(ptr, size)
#else
#define _rec_realloc(ptr, orig_size, size) realloc(ptr, size)
#endif
#else
static void* _rec_realloc(void* ptr, size_t orig_size, size_t size);
#endif
#endif

#ifdef USE_SDL_MALLOC
#define _rec_malloc SDL_malloc
#define _rec_calloc SDL_calloc
#else
#define _rec_malloc malloc
#define _rec_calloc calloc
#endif

static inline
void
_rec_free(const void*_Nullable data){
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

    #ifdef USE_SDL_MALLOC
        SDL_free((void*)data);
    #else
        free((void*)data);
    #endif
    #if defined(__clang__) || defined(__GNUC__)
        #pragma GCC diagnostic pop
    #elif defined(_MSC_VER)
        #pragma warning(pop)
    #endif
}


typedef struct RecordingAllocator RecordingAllocator;
struct RecordingAllocator{
    // We need a dynamic array to record all of the allocations.
    // We specialize it to be SOA
    void*_Nullable*_Nonnull allocations;
    size_t* allocation_sizes;
    size_t count;
    size_t capacity;
#ifdef HEAVY_RECORDING
    BacktraceArray*_Null_unspecified*_Null_unspecified backtraces;
#endif
};

static inline
void
recording_ensure_capacity(RecordingAllocator* r){
    if(r->count < r->capacity)
        return;
    if(!r->capacity){
        enum {INITIAL_CAPACITY=32};
        r->capacity = INITIAL_CAPACITY;
        r->allocations = _rec_malloc(INITIAL_CAPACITY*sizeof(*r->allocations));
        r->allocation_sizes = _rec_malloc(INITIAL_CAPACITY*sizeof(*r->allocation_sizes));
        #ifdef HEAVY_RECORDING
        r->backtraces = _rec_malloc(INITIAL_CAPACITY*sizeof(*r->backtraces));
        #endif
        return;
    }
    size_t old_cap = r->capacity;
    size_t new_cap = old_cap * 2;
    r->allocations = _rec_realloc(r->allocations, old_cap * sizeof(*r->allocations), new_cap*sizeof(*r->allocations));
    r->allocation_sizes = _rec_realloc(r->allocation_sizes, old_cap*sizeof(*r->allocation_sizes), new_cap*sizeof(*r->allocation_sizes));
    #ifdef HEAVY_RECORDING
    r->backtraces = _rec_realloc(r->backtraces, old_cap*sizeof(*r->backtraces), new_cap*sizeof(*r->backtraces));
    #endif
    r->capacity = new_cap;
}

MALLOC_FUNC
static
warn_unused
void*_Nullable
recording_alloc(RecordingAllocator* r, size_t size){
    RA_LOGIT("Allocation for %zu requested.", size);
    void* result = _rec_malloc(size);
    RA_LOGIT("Allocation for %zu granted: %p", size, result);
    if(!result)
        return result;
    recording_ensure_capacity(r);
    size_t index = r->count++;
    r->allocations[index] = result;
    r->allocation_sizes[index] = size;
#ifdef HEAVY_RECORDING
    r->backtraces[index] = get_bt();
#endif
    return result;
}

MALLOC_FUNC
static
warn_unused
void*_Nullable
recording_zalloc(RecordingAllocator* r, size_t size){
    RA_LOGIT("ZAllocation for %zu requested.", size);
    void* result = _rec_calloc(1, size);
    RA_LOGIT("ZAllocation for %zu granted: %p", size, result);
    if(!result)
        return result;
    recording_ensure_capacity(r);
    size_t index = r->count++;
    r->allocations[index] = result;
    r->allocation_sizes[index] = size;
#ifdef HEAVY_RECORDING
    r->backtraces[index] = get_bt();
#endif
    return result;
}

static
void
recording_free(RecordingAllocator* r, const void*_Nullable data, size_t size){
    if(!data){
        RA_LOGIT("Free with no data: %p, %zu", data, size);
        return;
    }
    RA_LOGIT("Free: %p, %zu", data, size);
    // inefficient, but whatever
    size_t count = r->count;
    for(size_t i = count; i--;){
        if(data == r->allocations[i]){
            RA_LOGIT("old ptr, size: %p, %zu", r->allocations[i], r->allocation_sizes[i]);
            if(size != r->allocation_sizes[i]){
                bt();
                assert(!(_Bool)"Freeing with the wrong size");
            }
            _rec_free(data);
            r->allocations[i] = NULL;
            r->allocation_sizes[i] = 0;
            #ifdef HEAVY_RECORDING
                _rec_free(r->backtraces[i]);
                r->backtraces[i] = 0;
            #endif
            RA_LOGIT("Free succeeded: %p, %zu", data, size);
            while(r->count && r->allocation_sizes[r->count-1] == 0)
                r->count--;
            return;
        }
    }
    assert(!(_Bool)"Freeing pointer not tracked by this allocator.");
}

// The money function, the reason we did this in the first
// place.
static
void
recording_free_all(RecordingAllocator* r){
    RA_LOGIT("Freeing all");
    for(size_t i = 0; i < r->count; i++){
        if(!r->allocations[i])
            continue;
        _rec_free(r->allocations[i]);
#ifdef HEAVY_RECORDING
        _rec_free(r->backtraces[i]);
#endif
    }
    r->count = 0;
}

static inline
void*_Nullable
recording_realloc(RecordingAllocator* r, void*_Nullable data, size_t orig_size, size_t new_size){
    RA_LOGIT("realloc request: old ptr: %p, orig_size: %zu, new_size: %zu", data, orig_size, new_size);
    if(!data)
        goto Lrealloc;
    size_t count = r->count;
    for(size_t i = count; i--;){
        assert(i < count);
        if(data == r->allocations[i]){
            assert(orig_size == r->allocation_sizes[i]);
            r->allocations[i] = NULL;
            r->allocation_sizes[i] = 0;
            #ifdef HEAVY_RECORDING
                _rec_free(r->backtraces[i]);
                r->backtraces[i] = 0;
            #endif
            while(r->count && r->allocation_sizes[r->count-1] == 0)
                r->count--;
            goto Lrealloc;
        }
    }
    assert(!(_Bool)"Reallocing an unknown pointer");

    Lrealloc:;
    RA_LOGIT("Falling back to actual realloc");
    void* result = _rec_realloc(data, orig_size, new_size);
    recording_ensure_capacity(r);
    size_t index = r->count++;
    r->allocations[index] = result;
    r->allocation_sizes[index] = new_size;
#ifdef HEAVY_RECORDING
    r->backtraces[index] = get_bt();
#endif
    RA_LOGIT("Realloced %p, size %zu into %p, size %zu", data, orig_size, result, new_size);
    return result;
}

static
void
recording_cleanup(RecordingAllocator* r){
    RA_LOGIT("Cleaning up the recorder itself");
    _rec_free(r->allocation_sizes);
    _rec_free(r->allocations);
#ifdef HEAVY_RECORDING
    _rec_free(r->backtraces);
#endif
    memset(r, 0, sizeof(*r));
}

#ifdef USE_RECORDED_ALLOCATOR
static inline
void
shallow_free_recorded_mallocator(Allocator a){
    assert(a.type == ALLOCATOR_RECORDED);
    RecordingAllocator* r = a._data;
    recording_cleanup(r);
    _rec_free(r);
}

static inline
Allocator
new_recorded_mallocator(void){
    RecordingAllocator* ra = _rec_calloc(1, sizeof(*ra));
    return (Allocator){
        ._data = ra,
        .type = ALLOCATOR_RECORDED,
    };
}
#endif

static inline
void
recording_assert_all_freed(RecordingAllocator* r){
    int leaked = 0;
    for(size_t i = 0; i < r->count; i++){
    #ifdef HEAVY_RECORDING
        if(r->allocation_sizes[i]){
            // fprintf(stderr, "%zu) leak: %zu bytes\n", i, r->allocation_sizes[i]);
            dump_bt(r->backtraces[i]);
            leaked++;
        }
    #else
        assert(r->allocation_sizes[i] == 0);
    #endif
    }
    assert(leaked == 0);
}

#ifdef __clang__
#pragma clang assume_nonnull end
#endif

#endif
