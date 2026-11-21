#ifndef DRP_BUILDER_GROWTH_H
#define DRP_BUILDER_GROWTH_H
//
// Copyright © 2026-2026, David Priver <david@davidpriver.com>
//

#include "Allocators/allocator.h"
#include "ckdint.h"

//
// Shared out-of-line buffer resizer to avoid excessive inlining in codegen.
//
#ifdef __clang__
#pragma clang assume_nonnull begin
#endif
#if defined(_MSC_VER)
__declspec(noinline)
#elif defined(__GNUC__) || defined(__clang__)
__attribute__((noinline))
#endif
static
void *_Nullable
drp_builder_grow(Allocator allocator, void *_Nullable data, size_t *capacity, size_t required, size_t element_size, size_t alignment){
    size_t n = 16;
    if(*capacity && add_overflow(*capacity, *capacity / 2, &n))
        return NULL;
    while(n < required){
        if(mul_overflow(n, (size_t)2, &n))
            return NULL;
    }
    if(add_overflow(n, (size_t)15, &n))
        return NULL;
    n &= ~(size_t)15;
    void *result = Allocator_realloc(allocator, data, *capacity, n, element_size, alignment);
    if(result)
        *capacity = n;
    return result;
}
#ifdef __clang__
#pragma clang assume_nonnull end
#endif
#endif
