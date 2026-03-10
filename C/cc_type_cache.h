#ifndef C_CC_TYPE_CACHE_H
#define C_CC_TYPE_CACHE_H
//
// Copyright © 2026-2026, David Priver <david@davidpriver.com>
//
// Type interning cache for derived types (pointers, arrays, functions).
// Ensures pointer equality for structurally identical types.
//
#include <stdint.h>
#include "cc_type.h"
#include "../Drp/Allocators/allocator.h"

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
#endif

typedef struct CcTypeTable CcTypeTable;
struct CcTypeTable {
    void* data;
    uint32_t count;
    uint32_t cap;
};

static inline
size_t
cctc_alloc_size(uint32_t cap){
    return sizeof(void*) * cap + sizeof(uint32_t) * 2 * cap;
}

static inline
uint32_t*
cctc_idxes(CcTypeTable* t){
    return (uint32_t*)(void*)((char*)t->data + sizeof(void*) * t->cap);
}

typedef struct CcTypeCache CcTypeCache;
struct CcTypeCache {
    CcTypeTable pointers;
    CcTypeTable arrays;
    CcTypeTable functions;
};

warn_unused static inline CcPointer* _Nullable cc_intern_pointer(CcTypeCache*, Allocator, CcQualType pointee, _Bool restrict_);
warn_unused static inline CcArray* _Nullable cc_intern_array(CcTypeCache*, Allocator, CcQualType element, size_t length, _Bool is_static, _Bool is_incomplete, _Bool is_vector, uint32_t vector_size);
warn_unused static inline CcFunction* _Nullable cc_intern_function(CcTypeCache*, Allocator, CcQualType return_type, const CcQualType* params, uint32_t param_count, _Bool is_variadic, _Bool no_prototype);

#ifdef __clang__
#pragma clang assume_nonnull end
#endif
#endif
