#ifndef DRP_PARRAY_H
#define DRP_PARRAY_H
#include <stddef.h>
#include "Allocators/allocator.h"
#ifdef __clang__
#pragma clang assume_nonnull begin
#else
#ifndef _Null_unspecified
#define _Null_unspecified
#endif
#ifndef _Nonnull
#define _Nonnull
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

typedef struct Parray Parray;
struct Parray {
    size_t count, capacity;
    void*_Null_unspecified*_Null_unspecified data;
};

static
warn_unused
int
pa_ensure_additional(Parray* pa, Allocator a, size_t n_additional){
    size_t required_capacity = pa->count + n_additional;
    if(pa->capacity >= required_capacity)
        return 0;
    size_t new_capacity;
    if(required_capacity < 8)
        new_capacity = 8;
    else {
        new_capacity = pa->capacity ? pa->capacity* 2:8;
        while(new_capacity < required_capacity)
            new_capacity *= 2;
    }
    size_t old_size = pa->capacity*sizeof *pa->data;
    size_t new_size = new_capacity*sizeof *pa->data;
    void* p = Allocator_realloc(a, pa->data, old_size, new_size);
    if(!p)
        return 1;
    pa->data = p;
    pa->capacity = new_capacity;
    return 0;
}

static
warn_unused
int
pa_push(Parray* pa, Allocator a, void*_Null_unspecified value){
    int err = pa_ensure_additional(pa, a, 1);
    if(err)
        return err;
    #if defined(__GNUC__) && !defined(__clang__)
    // False positive diagnostic, this function doesn't read what
    // value points to and the analysis is wrong anyway.
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
    #endif
    pa->data[pa->count++] = value;
    #if defined(__GNUC__) && !defined(__clang__)
    #pragma GCC diagnostic pop
    #endif
    return 0;
}

static
void
pa_cleanup(Parray* pa, Allocator a){
    Allocator_free(a, pa->data, pa->capacity*sizeof *pa->data);
    pa->data = NULL;
    pa->count = 0;
    pa->capacity = 0;
}

static
warn_unused
int
pa_shrink_to_size(Parray* pa, Allocator a){
    if(pa->count == pa->capacity) return 0;
    void* p = Allocator_realloc(a, pa->data, pa->capacity * sizeof *pa->data, pa->count * sizeof *pa->data);
    if(!p) return 1;
    pa->data = p;
    pa->capacity = pa->count;
    return 0;
}
#ifdef __clang__
#pragma clang assume_nonnull end
#endif
#endif
