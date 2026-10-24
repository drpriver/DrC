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
#include "cc_expr.h"
#include "cc_type_check.h"
#include "../Drp/hash_func.h"
#include "../Drp/ckdint.h"
#ifdef __clang__
#pragma clang assume_nonnull begin
#endif

// Hashing helpers

static inline
uint32_t
cctc_hash_pointer(CcQualType pointee, _Bool restrict_, _Bool is_block){
    uint64_t v = pointee.bits;
    v = v * 0x9e3779b97f4a7c15ULL + restrict_ + (is_block? 2u : 0u);
    return drp_hash(&v, sizeof v);
}

static inline
uint32_t
cctc_hash_sequence(CcQualType element, size_t length, uint32_t extra){
    uint64_t buf[3] = { element.bits, length, extra};
    return drp_hash(buf, sizeof buf);
}

static inline
uint32_t
cctc_hash_function(CcQualType return_type, const CcQualType*_Null_unspecified params, uint32_t param_count, uint32_t flags){
    uint64_t buf[2] = { return_type.bits, (uint64_t)param_count | ((uint64_t)flags << 32)};
    uint32_t h = drp_hash(buf, sizeof buf);
    for(uint32_t i = 0; i < param_count; i++){
        CcQualType value = cc_parameter_value_type(params[i]);
        h = drp_hash(&value, sizeof value) + 0x9e3779b9 + (h << 6) + (h >> 2);
    }
    return h;
}


// Table grow — grows the single allocation and rebuilds the index.
// Caller must re-insert all indices after this returns (hash is type-specific).
static
warn_unused
int
cctc_table_grow(CcTypeTable* t, Allocator al){
    uint32_t old_cap = t->cap;
    uint32_t new_cap = 4, index_count;
    if(old_cap && mul_overflow(old_cap, 2u, &new_cap)) return 1;
    if(mul_overflow(new_cap, 2u, &index_count)) return 1;
    size_t old_size, new_size;
    if(mul_overflow((size_t)new_cap, cctc_alloc_size(1), &new_size)) return 1;
    old_size = cctc_alloc_size(old_cap);
    void* data = Allocator_realloc(al, t->data, old_size, new_size, 1, _Alignof(void*));
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
cctc_pointer_eq(const CcPointer* a, CcQualType pointee, _Bool restrict_, _Bool is_block){
    return a->pointee.bits == pointee.bits && a->restrict_ == (uint32_t)restrict_ && (a->kind == CC_BLOCK_POINTER) == is_block;
}

static inline
_Bool
cctc_slice_eq(const CcSlice* a, CcQualType pointee, _Bool restrict_){
    return a->pointee.bits == pointee.bits && a->restrict_ == (uint32_t)restrict_;
}

static inline
uint32_t
cctc_hash_aligned(CcQualType underlying, uint32_t alignment){
    uint64_t buf[2] = {underlying.bits, alignment};
    return drp_hash(buf, sizeof buf);
}

static inline
void
cctc_rebuild_aligned(CcTypeTable* t){
    void** items = t->data;
    uint32_t* idxes = cctc_idxes(t);
    for(uint32_t i = 0; i < t->count; i++){
        CcAligned* q = items[i];
        uint32_t h = cctc_hash_aligned(q->underlying, q->alignment);
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
CcAligned* _Nullable
cc_intern_aligned(CcTypeCache* cache, Allocator al, CcQualType underlying, uint32_t alignment){
    underlying = ccqt_unaligned(underlying);
    underlying.quals = 0;
    CcTypeTable* t = &cache->aligned;
    uint32_t hash = cctc_hash_aligned(underlying, alignment);
    if(t->count){
        void** items = t->data;
        uint32_t* idxes = cctc_idxes(t);
        uint32_t idx = fast_reduce32(hash, 2 * t->cap);
        for(;;){
            uint32_t i = idxes[idx];
            if(!i) break;
            i--;
            CcAligned* p = items[i];
            if(p->underlying.bits == underlying.bits && p->alignment == alignment)
                return p;
            idx++;
            if(idx >= 2 * t->cap) idx = 0;
        }
    }
    if(t->count >= t->cap){
        if(cctc_table_grow(t, al) != 0) return NULL;
        cctc_rebuild_aligned(t);
    }
    CcAligned* p = Allocator_zalloc(al, 1, sizeof *p, _Alignof(CcAligned));
    if(!p) return NULL;
    p->kind = CC_ALIGNED;
    p->alignment = alignment;
    p->underlying = underlying;
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

static inline
void
cctc_rebuild_pointers(CcTypeTable* t){
    void** items = t->data;
    uint32_t* idxes = cctc_idxes(t);
    for(uint32_t i = 0; i < t->count; i++){
        CcPointer* q = items[i];
        uint32_t h = cctc_hash_pointer(q->pointee, q->restrict_, q->kind == CC_BLOCK_POINTER);
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
cc_intern_pointer(CcTypeCache* cache, Allocator al, CcQualType pointee, _Bool restrict_, _Bool is_block){
    CcTypeTable* t = &cache->pointers;
    uint32_t hash = cctc_hash_pointer(pointee, restrict_, is_block);
    if(t->count){
        void** items = t->data;
        uint32_t* idxes = cctc_idxes(t);
        uint32_t idx = fast_reduce32(hash, 2 * t->cap);
        for(;;){
            uint32_t i = idxes[idx];
            if(!i) break;
            i--;
            CcPointer* p = items[i];
            if(cctc_pointer_eq(p, pointee, restrict_, is_block))
                return p;
            idx++;
            if(idx >= 2 * t->cap) idx = 0;
        }
    }
    if(t->count >= t->cap){
        if(cctc_table_grow(t, al) != 0) return NULL;
        cctc_rebuild_pointers(t);
    }
    CcPointer* p = Allocator_zalloc(al, 1, sizeof *p, _Alignof(CcPointer));
    if(!p) return NULL;
    p->kind = is_block ? CC_BLOCK_POINTER : CC_POINTER;
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

warn_unused
static inline
CcSlice* _Nullable
cc_intern_slice(CcTypeCache* cache, Allocator al, CcQualType pointee, _Bool restrict_){
    CcTypeTable* t = &cache->slices;
    uint32_t hash = cctc_hash_pointer(pointee, restrict_, 0);
    if(t->count){
        void** items = t->data;
        uint32_t* idxes = cctc_idxes(t);
        uint32_t idx = fast_reduce32(hash, 2 * t->cap);
        for(;;){
            uint32_t i = idxes[idx];
            if(!i) break;
            i--;
            CcSlice* s = items[i];
            if(cctc_slice_eq(s, pointee, restrict_))
                return s;
            idx++;
            if(idx >= 2 * t->cap) idx = 0;
        }
    }
    if(t->count >= t->cap){
        if(cctc_table_grow(t, al) != 0) return NULL;
        cctc_rebuild_pointers(t);
    }
    CcSlice* s = Allocator_zalloc(al, 1, sizeof *s, _Alignof(CcSlice));
    if(!s) return NULL;
    s->kind = CC_SLICE;
    s->restrict_ = restrict_;
    s->pointee = pointee;
    void** items = t->data;
    uint32_t* idxes = cctc_idxes(t);
    uint32_t slot = t->count++;
    items[slot] = s;
    uint32_t idx = fast_reduce32(hash, 2 * t->cap);
    while(idxes[idx]){
        idx++;
        if(idx >= 2 * t->cap) idx = 0;
    }
    idxes[idx] = slot + 1;
    return s;
}

// Array interning (fixed-size and incomplete only, not VLAs)

static inline
_Bool
cctc_array_eq(const CcArray* a, CcQualType element, size_t length, _Bool is_incomplete){
    return a->element.bits == element.bits
        && a->length == length
        && a->is_incomplete == (uint32_t)is_incomplete;
}

static inline
void
cctc_rebuild_arrays(CcTypeTable* t){
    void** items = t->data;
    uint32_t* idxes = cctc_idxes(t);
    for(uint32_t i = 0; i < t->count; i++){
        CcArray* q = items[i];
        uint32_t f = (uint32_t)q->is_incomplete;
        uint32_t h = cctc_hash_sequence(q->element, q->length, f);
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
cc_intern_array(CcTypeCache* cache, Allocator al, CcQualType element, size_t length, _Bool is_incomplete){
    CcTypeTable* t = &cache->arrays;
    uint32_t flags = (uint32_t)is_incomplete;
    uint32_t hash = cctc_hash_sequence(element, length, flags);
    if(t->count){
        void** items = t->data;
        uint32_t* idxes = cctc_idxes(t);
        uint32_t idx = fast_reduce32(hash, 2 * t->cap);
        for(;;){
            uint32_t i = idxes[idx];
            if(!i) break;
            i--;
            CcArray* a = items[i];
            if(cctc_array_eq(a, element, length, is_incomplete))
                return a;
            idx++;
            if(idx >= 2 * t->cap) idx = 0;
        }
    }
    if(t->count >= t->cap){
        if(cctc_table_grow(t, al) != 0) return NULL;
        cctc_rebuild_arrays(t);
    }
    CcArray* a = Allocator_zalloc(al, 1, sizeof *a, _Alignof(CcArray));
    if(!a) return NULL;
    a->kind = CC_ARRAY;
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

static inline
uint32_t
cctc_hash_vla(const CcVla* vla, CcQualType element){
    _Bool constant = vla->bound->kind == CC_EXPR_VALUE;
    _Bool capture = vla->bound->kind == CC_EXPR_VARIABLE && vla->bound->var->vla_bound;
    uint64_t key[4] = {
        element.bits,
        constant ? vla->bound->uinteger
        : capture ? (uintptr_t)vla->bound->var
        : (uintptr_t)vla->bound,
        constant ? vla->bound->type.bits : 0,
        constant | (vla->unspecified << 1) | (capture << 2)
    };
    return drp_hash(key, sizeof key);
}

static inline
_Bool
cctc_vla_eq(const CcVla* a, const CcVla* b, CcQualType element){
    return a->element.bits == element.bits && cc_vla_bounds_same(a, b);
}

warn_unused
static inline
CcVla* _Nullable
cc_intern_vla(CcTypeCache* cache, Allocator al, const CcVla* source, CcQualType element){
    CcTypeTable* t = &cache->vlas;
    uint32_t hash = cctc_hash_vla(source, element);
    if(t->count){
        void** items = t->data;
        uint32_t* idxes = cctc_idxes(t);
        uint32_t idx = fast_reduce32(hash, 2*t->cap);
        for(;;){
            uint32_t i = idxes[idx];
            if(!i) break;
            CcVla* vla = items[i-1];
            if(cctc_vla_eq(vla, source, element)) return vla;
            if(++idx >= 2*t->cap) idx = 0;
        }
    }
    if(t->count >= t->cap){
        if(cctc_table_grow(t, al)) return NULL;
        void** items = t->data;
        uint32_t* idxes = cctc_idxes(t);
        for(uint32_t i = 0; i < t->count; i++){
            CcVla* vla = items[i];
            uint32_t idx = fast_reduce32(cctc_hash_vla(vla, vla->element), 2*t->cap);
            while(idxes[idx]) if(++idx >= 2*t->cap) idx = 0;
            idxes[idx] = i+1;
        }
    }
    CcVla* vla = Allocator_alloc(al, 1, sizeof *vla, _Alignof(CcVla));
    if(!vla) return NULL;
    *vla = *source;
    vla->parameter_quals = 0;
    vla->element = element;
    void** items = t->data;
    uint32_t* idxes = cctc_idxes(t);
    uint32_t slot = t->count++;
    items[slot] = vla;
    uint32_t idx = fast_reduce32(hash, 2*t->cap);
    while(idxes[idx]) if(++idx >= 2*t->cap) idx = 0;
    idxes[idx] = slot+1;
    return vla;
}

// Vector interning

static inline
void
cctc_rebuild_vectors(CcTypeTable* t){
    void** items = t->data;
    uint32_t* idxes = cctc_idxes(t);
    for(uint32_t i = 0; i < t->count; i++){
        CcVector* q = items[i];
        uint32_t f = q->vector_size;
        uint32_t h = cctc_hash_sequence(q->element, q->length, f);
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
cc_intern_vector(CcTypeCache* cache, Allocator al, CcQualType element, size_t length, uint32_t vector_size){
    element = ccqt_unaligned(element);
    element.quals = 0;
    CcTypeTable* t = &cache->vectors;
    uint32_t hash = cctc_hash_sequence(element, length, vector_size);
    if(t->count){
        void** items = t->data;
        uint32_t* idxes = cctc_idxes(t);
        uint32_t idx = fast_reduce32(hash, 2 * t->cap);
        for(;;){
            uint32_t i = idxes[idx];
            if(!i) break;
            i--;
            CcVector* a = items[i];
            if(a->element.bits == element.bits && a->length == length && a->vector_size == vector_size)
                return a;
            idx++;
            if(idx >= 2 * t->cap) idx = 0;
        }
    }
    if(t->count >= t->cap){
        if(cctc_table_grow(t, al) != 0) return NULL;
        cctc_rebuild_vectors(t);
    }
    CcVector* a = Allocator_zalloc(al, 1, sizeof *a, _Alignof(CcVector));
    if(!a) return NULL;
    a->kind = CC_VECTOR;
    a->vector_size = vector_size;
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
cctc_function_eq(const CcFunction* a, CcQualType return_type, const CcQualType*_Null_unspecified params, uint32_t param_count, uint32_t fixed_param_count, _Bool is_variadic, _Bool no_prototype){
    if(a->return_type.bits != return_type.bits) return 0;
    if(a->param_count != param_count) return 0;
    if(a->fixed_param_count != fixed_param_count) return 0;
    if(a->is_variadic != (uint32_t)is_variadic) return 0;
    if(a->no_prototype != (uint32_t)no_prototype) return 0;
    for(uint32_t i = 0; i < param_count; i++)
        if(a->params[i].bits != cc_parameter_value_type(params[i]).bits)
            return 0;
    return 1;
}

static inline
void
cctc_rebuild_functions(CcTypeTable* t){
    void** items = t->data;
    uint32_t* idxes = cctc_idxes(t);
    for(uint32_t i = 0; i < t->count; i++){
        CcFunction* q = items[i];
        uint32_t fl = (uint32_t)q->is_variadic | ((uint32_t)q->no_prototype << 1) | (q->fixed_param_count << 2);
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
cc_intern_function(CcTypeCache* cache, Allocator al, CcQualType return_type, const CcQualType*_Null_unspecified params, uint32_t param_count, uint32_t fixed_param_count, _Bool is_variadic, _Bool no_prototype){
    if(fixed_param_count > CC_MAX_PARAMS || fixed_param_count > param_count) return NULL;
    CcTypeTable* t = &cache->functions;
    uint32_t flags = (uint32_t)is_variadic | ((uint32_t)no_prototype << 1) | (fixed_param_count << 2);
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
            if(cctc_function_eq(f, return_type, params, param_count, fixed_param_count, is_variadic, no_prototype))
                return f;
            idx++;
            if(idx >= 2 * t->cap) idx = 0;
        }
    }
    if(t->count >= t->cap){
        if(cctc_table_grow(t, al) != 0) return NULL;
        cctc_rebuild_functions(t);
    }
    CcFunction* f = Allocator_zalloc_extra(al, param_count, sizeof *f->params, sizeof *f, _Alignof(CcFunction));
    if(!f) return NULL;
    f->kind = CC_FUNCTION;
    f->is_variadic = is_variadic;
    f->no_prototype = no_prototype;
    f->return_type = return_type;
    f->param_count = param_count;
    f->fixed_param_count = fixed_param_count;
    for(uint32_t i = 0; i < param_count; i++)
        f->params[i] = cc_parameter_value_type(params[i]);
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



#ifdef __clang__
#pragma clang assume_nonnull end
#endif
#endif
