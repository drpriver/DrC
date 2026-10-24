//
// Copyright © 2024-2025, David Priver <david@davidpriver.com>
//
#ifndef ATOM_MAP_H
#define ATOM_MAP_H 1
#include <stdint.h>
#include "Allocators/allocator.h"
#include "atom.h"
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

typedef struct AtomMap AtomMap;
struct AtomMap {
    void* data;
    uint32_t count;
    uint32_t cap;
};



#define AtomMap(T) AtomMap

typedef struct AtomMapItem AtomMapItem;
struct AtomMapItem {
    Atom atom;
    void*_Null_unspecified p;
};

typedef struct AtomMapItems AtomMapItems;
struct AtomMapItems{
    AtomMapItem* data;
    size_t count;
};

static inline
AtomMapItems
AM_items(const AtomMap* am){
    return (AtomMapItems){am->data, am->count};
}
static
inline
size_t
AM_alloc_size(size_t cap){
    return (sizeof(Atom)+sizeof(void*))*cap + 2*cap*sizeof(uint32_t);
}

// Reserve room for at least capacity entries. Growing also removes deleted entries.
warn_unused
static inline
int
AM_reserve(AtomMap* am, Allocator al, size_t capacity){
    if(capacity <= am->cap) return 0;
    uint32_t old_cap = am->cap;
    size_t new_cap = old_cap?old_cap:4;
    while(new_cap < capacity){
        if(new_cap > UINT32_MAX/4) return 1;
        new_cap *= 2;
    }
    if(new_cap > SIZE_MAX/(sizeof(Atom)+sizeof(void*)+2*sizeof(uint32_t))) return 1;
    size_t old_size = AM_alloc_size(old_cap);
    size_t new_size = AM_alloc_size(new_cap);
    uint32_t count = am->count;
    void* data = Allocator_realloc(al, am->data, old_size, new_size, 1, _Alignof(AtomMapItem));
    if(!data) return 1;
    const void** items = data;
    uint32_t* idxes = (uint32_t*)(void*)(new_cap*(sizeof(Atom)+(sizeof(void*)))+(char*)(data));
    memset(idxes, 0, sizeof(uint32_t)*2*new_cap);
    uint32_t new_count = 0;
    for(uint32_t i = 0; i < count; i++){
        if(!items[i*2+1]) continue;
        Atom k = items[i*2];
        if(i != new_count){
            items[new_count*2] = k;
            items[new_count*2+1] = items[i*2+1];
        }
        uint32_t hash = k->hash;
        uint32_t idx = fast_reduce32(hash, (uint32_t)(2*new_cap));
        while(idxes[idx]){
            idx++;
            if(idx >= 2*new_cap) idx = 0;
        }
        idxes[idx] = ++new_count;
    }
    am->count = new_count;
    am->data = data;
    am->cap = (uint32_t)new_cap;
    return 0;
}

// There is no `AM_cleanup()` to force you to think about the lifetime of the pointer to items.
static inline
void
AM_del(AtomMap* am, Atom key){
    if(!am->count) return;
    uint32_t hash = key->hash;
    uint32_t cap = am->cap;
    uint32_t idx = fast_reduce32(hash, 2*cap);
    void* data = am->data;
    void** items = data;
    uint32_t* idxes = (uint32_t*)(void*)(cap*(sizeof(Atom)+(sizeof(void*)))+(char*)(data));
    for(;;){
        uint32_t i = idxes[idx];
        if(!i) return;
        i--;
        Atom a = items[2*i];
        if(a == key){
            items[2*i+1] = NULL;
            return;
        }
        idx++;
        if(idx >= 2*cap) idx = 0;
    }
}


//
// Put a NULL value to delete the key (or call `AM_del()`).
// It will get removed when map gets resized.
//
// Returns 0 on success and 1 on error.
warn_unused
static inline
int
AM_put(AtomMap* am, Allocator al, Atom key, const void*_Nullable value_){
    if(!value_){
        AM_del(am, key);
        return 0;
    }
    // Takes a const void* similar to how memchr has to take const void*
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
    void* value = (void*)value_;
    #if defined(__clang__) || defined(__GNUC__)
        #pragma GCC diagnostic pop
    #elif defined(_MSC_VER)
        #pragma warning(pop)
    #endif
    if(am->count >= am->cap){
        if(AM_reserve(am, al, (size_t)am->cap + 1)) return 1;
    }
    uint32_t hash = key->hash;
    uint32_t cap = am->cap;
    uint32_t idx = fast_reduce32(hash, 2*cap);
    void* data = am->data;
    void** items = data;
    uint32_t* idxes = (uint32_t*)(void*)(cap*(sizeof(Atom)+(sizeof(void*)))+(char*)(data));
    for(;;){
        uint32_t i = idxes[idx];
        if(!i){
            i = am->count++;
            // const-wash
            items[2*i] = (void*)(uintptr_t)key;
            items[2*i+1] = value;
            idxes[idx] = i+1;
            return 0;
        }
        i--;
        Atom a = items[2*i];
        if(a == key){
            items[2*i+1] = value;
            return 0;
        }
        idx++;
        if(idx >= 2*cap) idx = 0;
    }
}

// Returns the value slot, inserting a NULL value for a new key.
// Resizing invalids the return value.
// Returns NULL on allocation failure.
static inline
void* _Nullable* _Nullable
AM_getsert(AtomMap* am, Allocator al, Atom key){
    if(!am->cap && AM_reserve(am, al, 4)) return NULL;
    for(;;){
        uint32_t cap = am->cap;
        uint32_t idx = fast_reduce32(key->hash, 2*cap);
        void** items = am->data;
        uint32_t* idxes = (uint32_t*)(void*)(cap*(sizeof(Atom)+sizeof(void*))+(char*)am->data);
        while(idxes[idx]){
            uint32_t i = idxes[idx]-1;
            if(items[2*i] == (const void*)key) return &items[2*i+1];
            idx++;
            if(idx >= 2*cap) idx = 0;
        }
        if(am->count >= cap){
            if(AM_reserve(am, al, (size_t)cap + 1)) return NULL;
            continue;
        }
        uint32_t i = am->count++;
        items[2*i] = (void*)(uintptr_t)key;
        items[2*i+1] = NULL;
        idxes[idx] = i+1;
        return &items[2*i+1];
    }
}

static inline
void*_Nullable
AM_get(const AtomMap* am, Atom key){
    if(!am->count) return NULL;
    uint32_t hash = key->hash;
    uint32_t cap = am->cap;
    uint32_t idx = fast_reduce32(hash, 2*cap);
    void* data = am->data;
    void** items = data;
    uint32_t* idxes = (uint32_t*)(void*)(cap*(sizeof(Atom)+(sizeof(void*)))+(char*)(data));
    for(;;){
        uint32_t i = idxes[idx];
        if(!i){
            return NULL;
        }
        i -= 1;
        Atom a = items[2*i];
        if(a == key) return items[2*i+1];
        idx++;
        if(idx >= 2*cap) idx = 0;
    }
}

static
void
AM_clear(AtomMap* am){
    if(!am->count) return;
    am->count = 0;
    if(am->data){
        uint32_t* idxes = (uint32_t*)(void*)(am->cap*(sizeof(Atom)+(sizeof(void*)))+(char*)(am->data));
        memset(idxes, 0, sizeof(uint32_t)*2*am->cap);
    }
}

// Compact the map by removing NULL entries and rebuilding the hash table
static inline
void
AM_compact(AtomMap* am){
    if(!am->count || !am->data) return;

    uint32_t cap = am->cap;
    uint32_t count = am->count;
    void* data = am->data;
    void** items = data;
    uint32_t* idxes = (uint32_t*)(void*)(cap*(sizeof(Atom)+(sizeof(void*)))+(char*)(data));

    // Clear index table
    memset(idxes, 0, sizeof(uint32_t)*2*cap);

    // Compact items array and rebuild index table
    uint32_t new_count = 0;
    for(uint32_t i = 0; i < count; i++){
        if(!items[i*2+1]) continue;  // Skip NULL entries

        Atom k = items[i*2];
        if(i != new_count){
            // const-wash
            items[new_count*2] = (void*)(uintptr_t)k;
            items[new_count*2+1] = items[i*2+1];
        }

        uint32_t hash = k->hash;
        uint32_t idx = fast_reduce32(hash, 2*cap);
        while(idxes[idx]){
            idx++;
            if(idx >= 2*cap) idx = 0;
        }
        idxes[idx] = ++new_count;
    }
    am->count = new_count;
}

// Returns 0 on success. On failure, dst is unchanged.
warn_unused
static inline
int
AM_clone_into(AtomMap* dst, Allocator al, const AtomMap* src){
    if(dst == src) return 0;
    if(dst->cap < src->cap){
        void* data = Allocator_realloc(al, dst->data, AM_alloc_size(dst->cap), AM_alloc_size(src->cap), 1, _Alignof(AtomMapItem));
        if(!data) return 1;
        dst->data = data;
        dst->cap = src->cap;
    }
    if(src->count)
        memcpy(dst->data, src->data, src->count*sizeof(AtomMapItem));
    dst->count = src->count;
    if(!dst->cap) return 0;
    uint32_t* indexes = (uint32_t*)((char*)dst->data + dst->cap*sizeof(AtomMapItem));
    if(dst->cap == src->cap){
        const uint32_t* source_indexes = (const uint32_t*)((const char*)src->data + src->cap*sizeof(AtomMapItem));
        memcpy(indexes, source_indexes, 2*dst->cap*sizeof(uint32_t));
        return 0;
    }
    memset(indexes, 0, 2*dst->cap*sizeof(uint32_t));
    const AtomMapItem* items = dst->data;
    for(uint32_t i = 0; i < dst->count; i++){
        Atom key = items[i].atom;
        uint32_t index = fast_reduce32(key->hash, 2*dst->cap);
        while(indexes[index]){
            index++;
            if(index == 2*dst->cap) index = 0;
        }
        indexes[index] = i+1;
    }
    return 0;
}

#ifdef __clang__
#pragma clang assume_nonnull end
#endif
#endif
