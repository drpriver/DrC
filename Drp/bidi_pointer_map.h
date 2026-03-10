//
// Copyright © 2026-2026, David Priver <david@davidpriver.com>
//
#ifndef BIDI_POINTER_MAP_H
#define BIDI_POINTER_MAP_H 1
#include <stdint.h>
#include <string.h>
#include "Allocators/allocator.h"
#include "hash_func.h"
#ifdef __clang__
#pragma clang assume_nonnull begin
#else
#ifndef _Nullable
#define _Nullable
#endif
#ifndef _Null_unspecified
#define _Null_unspecified
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

// Bidirectional pointer map (bijection). Same item storage as PointerMap
// but with two index tables so you can look up by value as well as by key.
// Insert-only: no deletion, no overwrite. Duplicate keys or values are errors.
//
// Layout in memory:
//   [items: cap pairs of (void* key, void* value)]
//   [key_idxes: 2*cap uint32_t]
//   [val_idxes: 2*cap uint32_t]

typedef struct BidiPointerMap BidiPointerMap;
struct BidiPointerMap {
    void* data;
    uint32_t count;
    uint32_t cap;
};

#define BidiPointerMap(K, V) BidiPointerMap

typedef struct BidiPointerMapItem BidiPointerMapItem;
struct BidiPointerMapItem {
    const void* key;
    void* value;
};

typedef struct BidiPointerMapItems BidiPointerMapItems;
struct BidiPointerMapItems {
    BidiPointerMapItem* data;
    size_t count;
};

static inline
BidiPointerMapItems
BPM_items(const BidiPointerMap* pm){
    return (BidiPointerMapItems){pm->data, pm->count};
}

static inline
size_t
BPM_alloc_size(size_t cap){
    return (sizeof(void*)+sizeof(void*))*cap + 4*cap*sizeof(uint32_t);
}

static inline
uint32_t
BPM_hash_ptr(const void* p){
    uintptr_t v = (uintptr_t)p;
    return hash_align8(&v, sizeof v);
}

static inline
uint32_t*
BPM_key_idxes_(void* data, uint32_t cap){
    return (uint32_t*)(void*)(cap*(sizeof(void*)+sizeof(void*))+(char*)(data));
}

static inline
uint32_t*
BPM_val_idxes_(void* data, uint32_t cap){
    return (uint32_t*)(void*)(cap*(sizeof(void*)+sizeof(void*)) + 2*cap*sizeof(uint32_t) + (char*)(data));
}

// Returns 1 on OOM, 2 on duplicate key, 3 on duplicate value.
warn_unused
static inline
int
BPM_put(BidiPointerMap* pm, Allocator al, const void* key, const void* value_){
    #if defined(__clang__) || defined(__GNUC__)
        #pragma GCC diagnostic push
        #pragma GCC diagnostic ignored "-Wcast-qual"
    #elif defined(_MSC_VER)
        #pragma warning(push)
        #pragma warning(disable: 4090)
    #else
    #endif
    void* value = (void*)value_;
    #if defined(__clang__) || defined(__GNUC__)
        #pragma GCC diagnostic pop
    #elif defined(_MSC_VER)
        #pragma warning(pop)
    #endif
    if(pm->count >= pm->cap){
        uint32_t old_cap = pm->cap;
        uint32_t old_size = (uint32_t)BPM_alloc_size(old_cap);
        uint32_t new_cap = old_cap?old_cap*2:4;
        uint32_t new_size = (uint32_t)BPM_alloc_size(new_cap);
        uint32_t count = pm->count;
        void* data = Allocator_realloc(al, pm->data, old_size, new_size);
        if(!data) return 1;
        void** items = data;
        uint32_t* key_idxes = BPM_key_idxes_(data, new_cap);
        uint32_t* val_idxes = BPM_val_idxes_(data, new_cap);
        memset(key_idxes, 0, sizeof(uint32_t)*2*new_cap);
        memset(val_idxes, 0, sizeof(uint32_t)*2*new_cap);
        for(uint32_t i = 0; i < count; i++){
            const void* k = items[i*2];
            const void* v = items[i*2+1];
            uint32_t kidx = fast_reduce32(BPM_hash_ptr(k), 2*new_cap);
            while(key_idxes[kidx]){
                kidx++;
                if(kidx >= 2*new_cap) kidx = 0;
            }
            key_idxes[kidx] = i + 1;
            uint32_t vidx = fast_reduce32(BPM_hash_ptr(v), 2*new_cap);
            while(val_idxes[vidx]){
                vidx++;
                if(vidx >= 2*new_cap) vidx = 0;
            }
            val_idxes[vidx] = i + 1;
        }
        pm->data = data;
        pm->cap = new_cap;
    }
    uint32_t cap = pm->cap;
    void** items = pm->data;
    uint32_t* key_idxes = BPM_key_idxes_(pm->data, cap);
    uint32_t* val_idxes = BPM_val_idxes_(pm->data, cap);
    // Check for duplicate key.
    uint32_t kidx = fast_reduce32(BPM_hash_ptr(key), 2*cap);
    for(;;){
        uint32_t i = key_idxes[kidx];
        if(!i) break;
        i--;
        if(items[2*i] == (void*)(uintptr_t)key) return 2;
        kidx++;
        if(kidx >= 2*cap) kidx = 0;
    }
    // Check for duplicate value.
    uint32_t vidx = fast_reduce32(BPM_hash_ptr(value), 2*cap);
    for(;;){
        uint32_t i = val_idxes[vidx];
        if(!i) break;
        i--;
        if(items[2*i+1] == value) return 3;
        vidx++;
        if(vidx >= 2*cap) vidx = 0;
    }
    // Insert.
    uint32_t slot = pm->count++;
    items[2*slot] = (void*)(uintptr_t)key;
    items[2*slot+1] = value;
    key_idxes[kidx] = slot+1;
    val_idxes[vidx] = slot+1;
    return 0;
}

static inline
void*_Nullable
BPM_get(const BidiPointerMap* pm, const void* key){
    if(!pm->count) return NULL;
    uint32_t cap = pm->cap;
    uint32_t idx = fast_reduce32(BPM_hash_ptr(key), 2*cap);
    void** items = pm->data;
    uint32_t* key_idxes = BPM_key_idxes_(pm->data, cap);
    for(;;){
        uint32_t i = key_idxes[idx];
        if(!i) return NULL;
        i--;
        if(items[2*i] == (void*)(uintptr_t)key) return items[2*i+1];
        idx++;
        if(idx >= 2*cap) idx = 0;
    }
}

static inline
void*_Nullable
BPM_rget(const BidiPointerMap* pm, const void* value){
    if(!pm->count) return NULL;
    uint32_t cap = pm->cap;
    uint32_t idx = fast_reduce32(BPM_hash_ptr(value), 2*cap);
    void** items = pm->data;
    uint32_t* val_idxes = BPM_val_idxes_(pm->data, cap);
    for(;;){
        uint32_t i = val_idxes[idx];
        if(!i) return NULL;
        i--;
        if(items[2*i+1] == (void*)(uintptr_t)value) return items[2*i];
        idx++;
        if(idx >= 2*cap) idx = 0;
    }
}

static
void
BPM_clear(BidiPointerMap* pm){
    if(!pm->count) return;
    pm->count = 0;
    if(pm->data){
        uint32_t* key_idxes = BPM_key_idxes_(pm->data, pm->cap);
        uint32_t* val_idxes = BPM_val_idxes_(pm->data, pm->cap);
        memset(key_idxes, 0, sizeof(uint32_t)*2*pm->cap);
        memset(val_idxes, 0, sizeof(uint32_t)*2*pm->cap);
    }
}

#ifdef __clang__
#pragma clang assume_nonnull end
#endif
#endif
