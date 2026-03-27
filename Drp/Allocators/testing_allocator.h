//
// Copyright © 2022-2023, David Priver <david@davidpriver.com>
//
#ifndef TESTING_ALLOCATOR_H
#define TESTING_ALLOCATOR_H
#include <stddef.h>
#include <stdint.h>
#include "allocator.h"
#include "recording_allocator.h"
#include "../thread_utils.h"

#ifdef __clang__
#pragma clang assume_nonnull begin
#endif

typedef struct TestingAllocator TestingAllocator;
struct TestingAllocator {
    int64_t fail_at;
    int64_t nallocs;
    RecordingAllocator recorder;
    LOCK_T lock;
};

static
void*_Nullable
testing_alloc(TestingAllocator* ta, size_t size){
    void* result = NULL;
    LOCK_T_lock(&ta->lock);
    if(ta->fail_at < 0){
        if(++ta->nallocs >= -ta->fail_at)
            goto done;
    }
    else {
        if(++ta->nallocs == ta->fail_at)
            goto done;
    }
    result = recording_alloc(&ta->recorder, size);
    done:
    LOCK_T_unlock(&ta->lock);
    return result;
}

static
void*_Nullable
testing_zalloc(TestingAllocator* ta, size_t size){
    void* result = NULL;
    LOCK_T_lock(&ta->lock);
    if(ta->fail_at < 0){
        if(++ta->nallocs >= -ta->fail_at)
            goto done;
    }
    else {
        if(++ta->nallocs == ta->fail_at)
            goto done;
    }
    result = recording_zalloc(&ta->recorder, size);
    done:
    LOCK_T_unlock(&ta->lock);
    return result;
}

static
void
testing_free(TestingAllocator* ta, const void*_Nullable data, size_t size){
    LOCK_T_lock(&ta->lock);
    recording_free(&ta->recorder, data, size);
    LOCK_T_unlock(&ta->lock);
}

static
void
testing_free_all(TestingAllocator* ta){
    LOCK_T_lock(&ta->lock);
    recording_free_all(&ta->recorder);
    LOCK_T_unlock(&ta->lock);
}
static inline
void*_Nullable
testing_realloc(TestingAllocator* ta, void*_Nullable data, size_t orig_size, size_t new_size){
    void* result = NULL;
    LOCK_T_lock(&ta->lock);
    if(new_size > orig_size){
        if(ta->fail_at < 0){
            if(++ta->nallocs >= -ta->fail_at)
                goto done;
        }
        else {
            if(++ta->nallocs == ta->fail_at)
                goto done;
        }
    }
    result = recording_realloc(&ta->recorder, data, orig_size, new_size);
    done:
    LOCK_T_unlock(&ta->lock);
    return result;
}


#ifdef REPLACE_MALLOCATOR

#ifdef MALLOCATOR
#error "Mallocator was already defined, you included this too late"
#endif

TestingAllocator THE_TestingAllocator = {.fail_at=0};
#define MALLOCATOR ((Allocator){.type=ALLOCATOR_TESTING, ._data=&THE_TestingAllocator})
#define MALLOCATORI {.type=ALLOCATOR_TESTING, ._data=&THE_TestingAllocator}
#define THE_TESTING_ALLOCATOR  ((Allocator){.type=ALLOCATOR_TESTING, ._data=&THE_TestingAllocator})

static inline
void
reset_the_testing_allocator(void){
    recording_cleanup(&THE_TestingAllocator.recorder);
    THE_TestingAllocator.nallocs = 0;
}

static inline
void
testing_assert_all_freed(void){
    LOCK_T_lock(&THE_TestingAllocator.lock);
    recording_assert_all_freed(&THE_TestingAllocator.recorder);
    recording_cleanup(&THE_TestingAllocator.recorder);
    LOCK_T_unlock(&THE_TestingAllocator.lock);
}

static inline
void
testing_reset(void){
    LOCK_T_lock(&THE_TestingAllocator.lock);
    recording_cleanup(&THE_TestingAllocator.recorder);
    THE_TestingAllocator.nallocs = 0;
    LOCK_T_unlock(&THE_TestingAllocator.lock);
}

static inline
void
testing_allocator_init(void){
    LOCK_T_init(&THE_TestingAllocator.lock);
}
#endif

#ifdef __clang__
#pragma clang assume_nonnull end
#endif

#endif
