//
// Copyright © 2021-2023, David Priver <david@davidpriver.com>
//
/*
 * An Allocator wrapper allocator that tracks all allocations
 * and frees done through it so as to provide a free_all function
 * for allocators that don't normally support it.
 *
 * This is currently super unoptimized, but that's ok.
 */
#ifndef RECORDING_ALLOCATOR_H
#define RECORDING_ALLOCATOR_H
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include "allocator.h"
#include "../ckdint.h"
#include "../debugging.h"

// This stuff is for debugging alloc/free errors.
// Probably worse than using HEAVY_RECORDING and dumping the stack traces
#if 0
#define DEBUG_ALLOCATIONS
#endif

#ifdef DEBUG_ALLOCATIONS
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


#ifndef __builtin_debugtrap
    #if defined __clang__ || defined __DRC__
        #define __builtin_debugtrap() __builtin_debugtrap()
    #elif defined __has_builtin
        #if __has_builtin(__builtin_debugtrap)
            #define __builtin_debugtrap() __builtin_debugtrap()
        #endif
    #endif
    #if !defined __builtin_debugtrap && defined __GNUC__
        #if defined __x86_64__ || defined __i386__
            #define __builtin_debugtrap() __asm__ volatile("int3")
        #elif defined __aarch64__
            #define __builtin_debugtrap() __asm__ volatile("brk #0xf000")
        #else
            #define __builtin_debugtrap() __builtin_trap()
        #endif
    #endif
    #if !defined __builtin_debugtrap && defined _MSC_VER
        #define __builtin_debugtrap() __debugbreak()
    #endif
#endif

#if defined __has_attribute
#if __has_attribute(cold)
__attribute__((cold))
#endif
#endif
static inline
_Noreturn
void
_rec_abort(const char * fmt, ...){
    bt();
    va_list va;
    va_start(va, fmt);
    vfprintf(stderr, fmt, va);
    va_end(va);
    __builtin_debugtrap();
    abort();
}

typedef struct RaRecord RaRecord;
struct RaRecord {
    size_t count,
           size,
           align;
    #ifdef HEAVY_RECORDING
    BacktraceArray *_Null_unspecified backtrace;
    #endif
};

typedef struct RecordingAllocator RecordingAllocator;
struct RecordingAllocator{
    _Alignas(8) void *_Null_unspecified *_Null_unspecified allocations;
    RaRecord *_Null_unspecified records;
    size_t count,
           capacity;
};

static inline
int
recording_ensure_capacity(RecordingAllocator* r){
    if(r->count < r->capacity)
        return 0;
    size_t cap = 32;
    size_t old_cap = r->capacity;
    if(r->capacity && mul_overflow(r->capacity, 2, &cap))
        return 1;
    void *block = Allocator_realloc((Allocator){.type=ALLOCATOR_MALLOC}, r->allocations, old_cap, cap, sizeof(RaRecord)+sizeof(void*), _Alignof(RaRecord));
    if(!block) return 1;
    r->allocations = block;
    r->records = (RaRecord*)(r->allocations+cap);
    if(r->count)
        memmove(r->records, r->allocations + old_cap, r->count * sizeof *r->records);
    r->capacity = cap;
    return 0;
}

MALLOC_FUNC
static
warn_unused
void *_Nullable
recording_alloc(RecordingAllocator *r, size_t count, size_t size, size_t alignment){
    RA_LOGIT("Allocation requested: [%zu count, %zu size, %zu alignment]", count, size, alignment);
    void *result = Allocator_alloc((Allocator){.type = ALLOCATOR_MALLOC}, count, size, alignment);
    if(!result || result == ALLOCATOR_SUCCESS_BUT_NO_DATA){
        RA_LOGIT("Allocation returned no storage: %p, [%zu count, %zu size, %zu alignment]", result, count, size, alignment);
        return result;
    }
    int err = recording_ensure_capacity(r);
    if(err){
        RA_LOGIT("Allocation recording failed: %p, [%zu count, %zu size, %zu alignment]", result, count, size, alignment);
        Allocator_free((Allocator){.type = ALLOCATOR_MALLOC}, result, count, size, alignment);
        return NULL;
    }
    r->allocations[r->count] = result;
    r->records[r->count++] = (RaRecord){
        .count = count,
        .size = size,
        .align = alignment,
        #ifdef HEAVY_RECORDING
        .backtrace = get_bt(),
        #endif
    };
    RA_LOGIT("Allocation succeeded: %p, [%zu count, %zu size, %zu alignment]", result, count, size, alignment);
    return result;
}

MALLOC_FUNC
static
warn_unused
void *_Nullable
recording_zalloc(RecordingAllocator *r, size_t count, size_t size, size_t alignment){
    RA_LOGIT("Zeroed allocation requested: [%zu count, %zu size, %zu alignment]", count, size, alignment);
    void *result = Allocator_zalloc((Allocator){.type = ALLOCATOR_MALLOC}, count, size, alignment);
    if(!result || result == ALLOCATOR_SUCCESS_BUT_NO_DATA){
        RA_LOGIT("Zeroed allocation returned no storage: %p, [%zu count, %zu size, %zu alignment]", result, count, size, alignment);
        return result;
    }
    int err = recording_ensure_capacity(r);
    if(err){
        RA_LOGIT("Zeroed allocation recording failed: %p, [%zu count, %zu size, %zu alignment]", result, count, size, alignment);
        Allocator_free((Allocator){.type = ALLOCATOR_MALLOC}, result, count, size, alignment);
        return NULL;
    }
    r->allocations[r->count] = result;
    r->records[r->count++] = (RaRecord){
        count,
        size,
        alignment,
        #ifdef HEAVY_RECORDING
        get_bt(),
        #endif
    };
    RA_LOGIT("Zeroed allocation succeeded: %p, [%zu count, %zu size, %zu alignment]", result, count, size, alignment);
    return result;
}

static
void
recording_free(RecordingAllocator *r, const void *_Nullable data, size_t count, size_t size, size_t alignment){
    if(!data || data == ALLOCATOR_SUCCESS_BUT_NO_DATA){
        RA_LOGIT("Free with no data: %p, [%zu count, %zu size, %zu alignment]", data, count, size, alignment);
        if(count && size){
            _rec_abort(
                "recording_free: empty pointer with nonzero count and element size\n"
                "  pointer=%p (%s) [%zu count, %zu size, %zu alignment]\n"
                "  expected count=0 or element_size=0\n",
                data, data ? "ALLOCATOR_SUCCESS_BUT_NO_DATA" : "NULL",
                count, size, alignment
            );
        }
        return;
    }
    RA_LOGIT("Free: %p, [%zu count, %zu size, %zu alignment]", data, count, size, alignment);
    // inefficient, but whatever
    for(size_t i = r->count; i--;){
        void *allocation = r->allocations[i];
        if(data == allocation){
            RaRecord* rec = &r->records[i];
            RA_LOGIT(
                "Recorded allocation: %p, [%zu count, %zu size, %zu alignment]",
                data,
                rec->count, rec->size, rec->align
            );
            if(count != rec->count || size != rec->size || alignment != rec->align){
                #ifdef HEAVY_RECORDING
                fprintf(stderr, "Recorded allocation backtrace:\n");
                dump_bt(rec->backtrace);
                #endif
                _rec_abort(
                    "recording_free: allocation argument mismatch\n"
                    "  pointer=%p slot=%zu\n"
                    "  supplied: [%zu count, %zu size, %zu alignment]\n"
                    "  recorded: [%zu count, %zu size, %zu alignment]\n",
                    data, i,
                    count, size, alignment,
                    rec->count, rec->size, rec->align
                );
            }
            Allocator_free((Allocator){.type = ALLOCATOR_MALLOC}, data, count, size, alignment);
            #ifdef HEAVY_RECORDING
            free(rec->backtrace);
            #endif
            r->allocations[i] = NULL;
            *rec = (RaRecord){0};
            RA_LOGIT("Free succeeded: %p, [%zu count, %zu size, %zu alignment]", data, count, size, alignment);
            while(r->count && r->allocations[r->count-1] == NULL)
                r->count--;
            return;
        }
    }
    _rec_abort(
        "recording_free: pointer not tracked by this recorder\n"
        "  pointer=%p [%zu count, %zu size, %zu alignment]\n"
        "  searched %zu slots, %zu capacity\n",
        data,
        count, size, alignment,
        r->count, r->capacity
    );
}

static
void
recording_free_all(RecordingAllocator* r){
    RA_LOGIT("Freeing all");
    for(size_t i = 0; i < r->count; i++){
        void *allocation = r->allocations[i];
        if(!allocation) continue;
        RaRecord *rec = &r->records[i];
        RA_LOGIT("Freeing recorded allocation: %p, [%zu count, %zu size, %zu alignment]", allocation, rec->count, rec->size, rec->align);
        #ifdef HEAVY_RECORDING
        free(rec->backtrace);
        #endif
        Allocator_free((Allocator){.type = ALLOCATOR_MALLOC}, allocation, rec->count, rec->size, rec->align);
    }
    r->count = 0;
}

static inline
void *_Nullable
recording_realloc(RecordingAllocator* r, void*_Nullable data, size_t orig_count, size_t count, size_t size, size_t alignment){
    RA_LOGIT("Realloc requested: %p, [%zu original count, %zu count, %zu size, %zu alignment]", data, orig_count, count, size, alignment);
    _Bool empty = !data || data == ALLOCATOR_SUCCESS_BUT_NO_DATA;
    if(!size) return empty ? ALLOCATOR_SUCCESS_BUT_NO_DATA : NULL;
    if(empty != (orig_count == 0)) return NULL;
    size_t orig_size, new_size;
    if(mul_overflow(orig_count, size, &orig_size)
    || mul_overflow(count, size, &new_size))
        return NULL;
    if(!orig_count) return recording_alloc(r, count, size, alignment);
    if(!count){
        recording_free(r, data, orig_count, size, alignment);
        return ALLOCATOR_SUCCESS_BUT_NO_DATA;
    }
    for(size_t i = r->count; i--;){
        void *allocation = r->allocations[i];
        if(data == allocation){
            RaRecord *rec = &r->records[i];
            RA_LOGIT("Recorded allocation: %p, [%zu count, %zu size, %zu alignment]", allocation, rec->count, rec->size, rec->align);
            if(orig_count != rec->count || size != rec->size || alignment != rec->align){
                #ifdef HEAVY_RECORDING
                fprintf(stderr, "Recorded allocation backtrace:\n");
                dump_bt(rec->backtrace);
                #endif
                _rec_abort(
                    "recording_realloc: allocation argument mismatch\n"
                    "  pointer=%p slot=%zu new_count=%zu\n"
                    "  supplied: [%zu count, %zu size, %zu alignment]\n"
                    "  recorded: [%zu count, %zu size, %zu alignment]\n",
                    data, i, count,
                    orig_count, size, alignment,
                    rec->count, rec->size, rec->align);
            }
            void *result = Allocator_realloc((Allocator){.type = ALLOCATOR_MALLOC}, data, orig_count, count, size, alignment);
            if(!result){
                RA_LOGIT("Realloc failed: %p, [%zu original count, %zu count, %zu size, %zu alignment]", data, orig_count, count, size, alignment);
                return NULL;
            }
            RA_LOGIT("Realloc succeeded: %p, [%zu count, %zu size, %zu alignment]", result, count, size, alignment);
            #ifdef HEAVY_RECORDING
                free(rec->backtrace);
            #endif
            r->allocations[i] = result;
            *rec = (RaRecord){
                .count = count,
                .size = size,
                .align = alignment,
                #ifdef HEAVY_RECORDING
                .backtrace = get_bt()
                #endif
            };
            return result;
        }
    }
    _rec_abort(
        "recording_realloc: pointer not tracked by this recorder\n"
        "  pointer=%p original_count=%zu new_count=%zu element_size=%zu alignment=%zu\n"
        "  searched %zu slots, %zu capacity\n",
        data, orig_count, count, size, alignment, r->count, r->capacity);
}

static
void
recording_cleanup(RecordingAllocator *r){
    RA_LOGIT("Cleaning up the recorder itself");
    Allocator_free((Allocator){.type=ALLOCATOR_MALLOC}, r->allocations, r->capacity, sizeof(RaRecord)+sizeof(void*), _Alignof(RaRecord));
    memset(r, 0, sizeof *r);
}

static inline
void
recording_assert_all_freed(RecordingAllocator *r){
    size_t leaked = 0;
    for(size_t i = 0; i < r->count; i++){
        void* allocation = r->allocations[i];
        if(!allocation) continue;
        RaRecord* rec = &r->records[i];
        fprintf(
            stderr,
            "recording_assert_all_freed: leaked allocation\n"
            "  pointer=%p slot=%zu [%zu count, %zu size, %zu alignment]\n",
             allocation, i,
             rec->count, rec->size, rec->align
        );
    #ifdef HEAVY_RECORDING
        dump_bt(rec->backtrace);
    #endif
        leaked++;
    }
    if(leaked)
        _rec_abort(
            "recording_assert_all_freed: %zu live allocations remain\n"
            "  %zu slots %zu capacity\n",
            leaked, r->count, r->capacity
        );
}

#ifdef __clang__
#pragma clang assume_nonnull end
#endif

#endif
