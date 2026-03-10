//
// Copyright © 2026-2026, David Priver <david@davidpriver.com>
//
#ifndef POINTER_MAP_H
#define POINTER_MAP_H 1
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

#if defined(__GNUC__) || defined(__clang__)
#define warn_unused __attribute__((warn_unused_result))
#elif defined(_MSC_VER)
#define warn_unused
#else
#define warn_unused
#endif

typedef struct PointerMap PointerMap;
struct PointerMap {
    void* data;
    uint32_t count;
    uint32_t cap;
};

#define PointerMap(K, V) PointerMap

typedef struct PointerMapItem PointerMapItem;
struct PointerMapItem {
    const void* key;
    void*_Null_unspecified value;
};

typedef struct PointerMapItems PointerMapItems;
struct PointerMapItems {
    PointerMapItem* data;
    size_t count;
};

static inline
PointerMapItems
PM_items(const PointerMap* pm){
    return (PointerMapItems){pm->data, pm->count};
}

static inline
size_t
PM_alloc_size(size_t cap){
    return (sizeof(void*)+sizeof(void*))*cap + 2*cap*sizeof(uint32_t);
}

static inline
uint32_t
PM_hash_ptr(const void* p){
    uintptr_t v = (uintptr_t)p;
    return hash_align8(&v, sizeof v);
}

static inline
void
PM_del(PointerMap* pm, const void* key){
    if(!pm->count) return;
    uint32_t hash = PM_hash_ptr(key);
    uint32_t cap = pm->cap;
    uint32_t idx = fast_reduce32(hash, 2*cap);
    void* data = pm->data;
    void** items = data;
    uint32_t* idxes = (uint32_t*)(void*)(cap*(sizeof(void*)+sizeof(void*))+(char*)(data));
    for(;;){
        uint32_t i = idxes[idx];
        if(!i) return;
        i--;
        const void* k = items[2*i];
        if(k == key){
            items[2*i+1] = NULL;
            return;
        }
        idx++;
        if(idx >= 2*cap) idx = 0;
    }
}

warn_unused
static inline
int
PM_put(PointerMap* pm, Allocator al, const void* key, const void*_Nullable value_){
    if(!value_){
        PM_del(pm, key);
        return 0;
    }
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
        uint32_t old_size = (uint32_t)PM_alloc_size(old_cap);
        uint32_t new_cap = old_cap?old_cap*2:4;
        uint32_t new_size = (uint32_t)PM_alloc_size(new_cap);
        uint32_t count = pm->count;
        void* data = Allocator_realloc(al, pm->data, old_size, new_size);
        if(!data) return 1;
        void** items = data;
        uint32_t* idxes = (uint32_t*)(void*)(new_cap*(sizeof(void*)+sizeof(void*))+(char*)(data));
        memset(idxes, 0, sizeof(uint32_t)*2*new_cap);
        uint32_t new_count = 0;
        for(uint32_t i = 0; i < count; i++){
            if(!items[i*2+1]) continue;
            const void* k = items[i*2];
            if(i != new_count){
                items[new_count*2] = (void*)(uintptr_t)k;
                items[new_count*2+1] = items[i*2+1];
            }
            uint32_t hash = PM_hash_ptr(k);
            uint32_t idx2 = fast_reduce32(hash, 2*new_cap);
            while(idxes[idx2]){
                idx2++;
                if(idx2 >= 2*new_cap) idx2 = 0;
            }
            idxes[idx2] = ++new_count;
        }
        pm->count = new_count;
        pm->data = data;
        pm->cap = new_cap;
    }
    uint32_t hash = PM_hash_ptr(key);
    uint32_t cap = pm->cap;
    uint32_t idx = fast_reduce32(hash, 2*cap);
    void* data = pm->data;
    void** items = data;
    uint32_t* idxes = (uint32_t*)(void*)(cap*(sizeof(void*)+sizeof(void*))+(char*)(data));
    for(;;){
        uint32_t i = idxes[idx];
        if(!i){
            i = pm->count++;
            items[2*i] = (void*)(uintptr_t)key;
            items[2*i+1] = value;
            idxes[idx] = i+1;
            return 0;
        }
        i--;
        const void* k = items[2*i];
        if(k == key){
            items[2*i+1] = value;
            return 0;
        }
        idx++;
        if(idx >= 2*cap) idx = 0;
    }
}

static inline
void*_Nullable
PM_get(const PointerMap* pm, const void* key){
    if(!pm->count) return NULL;
    uint32_t hash = PM_hash_ptr(key);
    uint32_t cap = pm->cap;
    uint32_t idx = fast_reduce32(hash, 2*cap);
    void* data = pm->data;
    void** items = data;
    uint32_t* idxes = (uint32_t*)(void*)(cap*(sizeof(void*)+sizeof(void*))+(char*)(data));
    for(;;){
        uint32_t i = idxes[idx];
        if(!i){
            return NULL;
        }
        i -= 1;
        const void* k = items[2*i];
        if(k == key) return items[2*i+1];
        idx++;
        if(idx >= 2*cap) idx = 0;
    }
}

static
void
PM_clear(PointerMap* pm){
    if(!pm->count) return;
    pm->count = 0;
    if(pm->data){
        uint32_t* idxes = (uint32_t*)(void*)(pm->cap*(sizeof(void*)+sizeof(void*))+(char*)(pm->data));
        memset(idxes, 0, sizeof(uint32_t)*2*pm->cap);
    }
}

#ifdef __clang__
#pragma clang assume_nonnull end
#endif
#endif
