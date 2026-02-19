#ifndef C_CC_TYPE_CACHE_C
#define C_CC_TYPE_CACHE_C
//
// Copyright © 2026-2026, David Priver <david@davidpriver.com>
//
// Layout per table (same as AtomMap):
//   [ void* items[cap] | uint32_t idxes[2*cap] ]
//   Single allocation. Index table is 2x cap for ~50% load factor.
//   Indices are 1-based (0 = empty).
//
#include <string.h>
#include "cc_type_cache.h"
#include "../Drp/hash_func.h"
#ifdef __clang__
#pragma clang assume_nonnull begin
#endif

// Hashing helpers

static inline
uint32_t
cctc_hash_pointer(CcQualType pointee, _Bool restrict_){
    uint64_t v = pointee.bits;
    v = v * 0x9e3779b97f4a7c15ULL + restrict_;
    return hash_align8(&v, sizeof v);
}

static inline
uint32_t
cctc_hash_array(CcQualType element, size_t length, uint32_t flags){
    uint64_t buf[2] = { element.bits, length ^ ((uint64_t)flags << 56)};
    return hash_align8(buf, sizeof buf);
}

static inline
uint32_t
cctc_hash_function(CcQualType return_type, const CcQualType* params, uint32_t param_count, uint32_t flags){
    uint64_t buf[2] = { return_type.bits, (uint64_t)param_count | ((uint64_t)flags << 32)};
    uint32_t h = hash_align8(buf, sizeof buf);
    if(param_count)
        h = hash_align8(params, sizeof *params * param_count) + 0x9e3779b9 + (h << 6) + (h >> 2);
    return h;
}

static inline
uint32_t
cctc_hash_vector(CcQualType element, uint32_t vector_size){
    uint64_t buf[2] = {element.bits, vector_size};
    return hash_align8(buf, sizeof buf);
}

// Table grow — grows the single allocation and rebuilds the index.
// Caller must re-insert all indices after this returns (hash is type-specific).
static
warn_unused
int
cctc_table_grow(CcTypeTable* t, Allocator al){
    uint32_t old_cap = t->cap;
    uint32_t new_cap = old_cap ? old_cap * 2 : 4;
    size_t old_size = cctc_alloc_size(old_cap);
    size_t new_size = cctc_alloc_size(new_cap);
    void* data = Allocator_realloc(al, t->data, old_size, new_size);
    if(!data) return 1;
    t->data = data;
    t->cap = new_cap;
    uint32_t* idxes = cctc_idxes(t);
    memset(idxes, 0, sizeof(uint32_t) * 2 * new_cap);
    return 0;
}

// Pointer interning

static inline
_Bool
cctc_pointer_eq(const CcPointer* a, CcQualType pointee, _Bool restrict_){
    return a->pointee.bits == pointee.bits && a->restrict_ == (uint32_t)restrict_;
}

static inline
void
cctc_rebuild_pointers(CcTypeTable* t){
    void** items = t->data;
    uint32_t* idxes = cctc_idxes(t);
    for(uint32_t i = 0; i < t->count; i++){
        CcPointer* q = items[i];
        uint32_t h = cctc_hash_pointer(q->pointee, q->restrict_);
        uint32_t idx = fast_reduce32(h, 2 * t->cap);
        while(idxes[idx]){
            idx++;
            if(idx >= 2 * t->cap) idx = 0;
        }
        idxes[idx] = i + 1;
    }
}

warn_unused
static inline
CcPointer* _Nullable
cc_intern_pointer(CcTypeCache* cache, Allocator al, CcQualType pointee, _Bool restrict_){
    CcTypeTable* t = &cache->pointers;
    uint32_t hash = cctc_hash_pointer(pointee, restrict_);
    if(t->count){
        void** items = t->data;
        uint32_t* idxes = cctc_idxes(t);
        uint32_t idx = fast_reduce32(hash, 2 * t->cap);
        for(;;){
            uint32_t i = idxes[idx];
            if(!i) break;
            i--;
            CcPointer* p = items[i];
            if(cctc_pointer_eq(p, pointee, restrict_))
                return p;
            idx++;
            if(idx >= 2 * t->cap) idx = 0;
        }
    }
    if(t->count >= t->cap){
        if(cctc_table_grow(t, al) != 0) return NULL;
        cctc_rebuild_pointers(t);
    }
    CcPointer* p = Allocator_zalloc(al, sizeof *p);
    if(!p) return NULL;
    p->kind = CC_POINTER;
    p->restrict_ = restrict_;
    p->pointee = pointee;
    void** items = t->data;
    uint32_t* idxes = cctc_idxes(t);
    uint32_t slot = t->count++;
    items[slot] = p;
    uint32_t idx = fast_reduce32(hash, 2 * t->cap);
    while(idxes[idx]){
        idx++;
        if(idx >= 2 * t->cap) idx = 0;
    }
    idxes[idx] = slot + 1;
    return p;
}

// Array interning (fixed-size and incomplete only, not VLAs)

static inline
_Bool
cctc_array_eq(const CcArray* a, CcQualType element, size_t length, _Bool is_static, _Bool is_incomplete){
    return a->element.bits == element.bits
        && a->length == length
        && a->is_static == (uint32_t)is_static
        && a->is_incomplete == (uint32_t)is_incomplete;
}

static inline
void
cctc_rebuild_arrays(CcTypeTable* t){
    void** items = t->data;
    uint32_t* idxes = cctc_idxes(t);
    for(uint32_t i = 0; i < t->count; i++){
        CcArray* q = items[i];
        uint32_t f = (uint32_t)q->is_static | ((uint32_t)q->is_incomplete << 1);
        uint32_t h = cctc_hash_array(q->element, q->length, f);
        uint32_t idx = fast_reduce32(h, 2 * t->cap);
        while(idxes[idx]){
            idx++;
            if(idx >= 2 * t->cap) idx = 0;
        }
        idxes[idx] = i + 1;
    }
}

warn_unused
static inline
CcArray* _Nullable
cc_intern_array(CcTypeCache* cache, Allocator al, CcQualType element, size_t length, _Bool is_static, _Bool is_incomplete){
    CcTypeTable* t = &cache->arrays;
    uint32_t flags = (uint32_t)is_static | ((uint32_t)is_incomplete << 1);
    uint32_t hash = cctc_hash_array(element, length, flags);
    if(t->count){
        void** items = t->data;
        uint32_t* idxes = cctc_idxes(t);
        uint32_t idx = fast_reduce32(hash, 2 * t->cap);
        for(;;){
            uint32_t i = idxes[idx];
            if(!i) break;
            i--;
            CcArray* a = items[i];
            if(cctc_array_eq(a, element, length, is_static, is_incomplete))
                return a;
            idx++;
            if(idx >= 2 * t->cap) idx = 0;
        }
    }
    if(t->count >= t->cap){
        if(cctc_table_grow(t, al) != 0) return NULL;
        cctc_rebuild_arrays(t);
    }
    CcArray* a = Allocator_zalloc(al, sizeof *a);
    if(!a) return NULL;
    a->kind = CC_ARRAY;
    a->is_static = is_static;
    a->is_incomplete = is_incomplete;
    a->element = element;
    a->length = length;
    void** items = t->data;
    uint32_t* idxes = cctc_idxes(t);
    uint32_t slot = t->count++;
    items[slot] = a;
    uint32_t idx = fast_reduce32(hash, 2 * t->cap);
    while(idxes[idx]){
        idx++;
        if(idx >= 2 * t->cap) idx = 0;
    }
    idxes[idx] = slot + 1;
    return a;
}

// Function interning

static inline
_Bool
cctc_function_eq(const CcFunction* a, CcQualType return_type, const CcQualType* params, uint32_t param_count, _Bool is_variadic, _Bool no_prototype){
    if(a->return_type.bits != return_type.bits) return 0;
    if(a->param_count != param_count) return 0;
    if(a->is_variadic != (uint32_t)is_variadic) return 0;
    if(a->no_prototype != (uint32_t)no_prototype) return 0;
    return memcmp(a->params, params, sizeof *params * param_count) == 0;
}

static inline
void
cctc_rebuild_functions(CcTypeTable* t){
    void** items = t->data;
    uint32_t* idxes = cctc_idxes(t);
    for(uint32_t i = 0; i < t->count; i++){
        CcFunction* q = items[i];
        uint32_t fl = (uint32_t)q->is_variadic | ((uint32_t)q->no_prototype << 1);
        uint32_t h = cctc_hash_function(q->return_type, q->params, q->param_count, fl);
        uint32_t idx = fast_reduce32(h, 2 * t->cap);
        while(idxes[idx]){
            idx++;
            if(idx >= 2 * t->cap) idx = 0;
        }
        idxes[idx] = i + 1;
    }
}

warn_unused
static inline
CcFunction* _Nullable
cc_intern_function(CcTypeCache* cache, Allocator al, CcQualType return_type, const CcQualType* params, uint32_t param_count, _Bool is_variadic, _Bool no_prototype){
    CcTypeTable* t = &cache->functions;
    uint32_t flags = (uint32_t)is_variadic | ((uint32_t)no_prototype << 1);
    uint32_t hash = cctc_hash_function(return_type, params, param_count, flags);
    if(t->count){
        void** items = t->data;
        uint32_t* idxes = cctc_idxes(t);
        uint32_t idx = fast_reduce32(hash, 2 * t->cap);
        for(;;){
            uint32_t i = idxes[idx];
            if(!i) break;
            i--;
            CcFunction* f = items[i];
            if(cctc_function_eq(f, return_type, params, param_count, is_variadic, no_prototype))
                return f;
            idx++;
            if(idx >= 2 * t->cap) idx = 0;
        }
    }
    if(t->count >= t->cap){
        if(cctc_table_grow(t, al) != 0) return NULL;
        cctc_rebuild_functions(t);
    }
    size_t sz = sizeof(CcFunction) + sizeof *params * param_count;
    CcFunction* f = Allocator_zalloc(al, sz);
    if(!f) return NULL;
    f->kind = CC_FUNCTION;
    f->is_variadic = is_variadic;
    f->no_prototype = no_prototype;
    f->return_type = return_type;
    f->param_count = param_count;
    memcpy(f->params, params, sizeof *params * param_count);
    void** items = t->data;
    uint32_t* idxes = cctc_idxes(t);
    uint32_t slot = t->count++;
    items[slot] = f;
    uint32_t idx = fast_reduce32(hash, 2 * t->cap);
    while(idxes[idx]){
        idx++;
        if(idx >= 2 * t->cap) idx = 0;
    }
    idxes[idx] = slot + 1;
    return f;
}

// Vector interning

static inline
_Bool
cctc_vector_eq(const CcVector* a, CcQualType element, uint32_t vector_size){
    return a->element.bits == element.bits && a->vector_size == vector_size;
}

static inline
void
cctc_rebuild_vectors(CcTypeTable* t){
    void** items = t->data;
    uint32_t* idxes = cctc_idxes(t);
    for(uint32_t i = 0; i < t->count; i++){
        CcVector* q = items[i];
        uint32_t h = cctc_hash_vector(q->element, q->vector_size);
        uint32_t idx = fast_reduce32(h, 2 * t->cap);
        while(idxes[idx]){
            idx++;
            if(idx >= 2 * t->cap) idx = 0;
        }
        idxes[idx] = i + 1;
    }
}

warn_unused
static inline
CcVector* _Nullable
cc_intern_vector(CcTypeCache* cache, Allocator al, CcQualType element, uint32_t vector_size){
    CcTypeTable* t = &cache->vectors;
    uint32_t hash = cctc_hash_vector(element, vector_size);
    if(t->count){
        void** items = t->data;
        uint32_t* idxes = cctc_idxes(t);
        uint32_t idx = fast_reduce32(hash, 2 * t->cap);
        for(;;){
            uint32_t i = idxes[idx];
            if(!i) break;
            i--;
            CcVector* v = items[i];
            if(cctc_vector_eq(v, element, vector_size))
                return v;
            idx++;
            if(idx >= 2 * t->cap) idx = 0;
        }
    }
    if(t->count >= t->cap){
        if(cctc_table_grow(t, al) != 0) return NULL;
        cctc_rebuild_vectors(t);
    }
    CcVector* v = Allocator_zalloc(al, sizeof *v);
    if(!v) return NULL;
    v->kind = CC_VECTOR;
    v->element = element;
    v->vector_size = vector_size;
    void** items = t->data;
    uint32_t* idxes = cctc_idxes(t);
    uint32_t slot = t->count++;
    items[slot] = v;
    uint32_t idx = fast_reduce32(hash, 2 * t->cap);
    while(idxes[idx]){
        idx++;
        if(idx >= 2 * t->cap) idx = 0;
    }
    idxes[idx] = slot + 1;
    return v;
}


#ifdef __clang__
#pragma clang assume_nonnull end
#endif
#endif
