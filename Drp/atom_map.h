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
        uint32_t old_cap = am->cap;
        uint32_t old_size = (uint32_t)AM_alloc_size(old_cap);
        uint32_t new_cap = old_cap?old_cap*2:4;
        uint32_t new_size = (uint32_t)AM_alloc_size(new_cap);
        uint32_t count = am->count;
        void* data = Allocator_realloc(al, am->data, old_size, new_size);
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
            uint32_t idx = fast_reduce32(hash, 2*new_cap);
            while(idxes[idx]){
                idx++;
                if(idx >= 2*new_cap) idx = 0;
            }
            idxes[idx] = ++new_count;
        }
        am->count = new_count;
        am->data = data;
        am->cap = new_cap;
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

/*
static inline
void*_Nullable
AM_getsert(AtomMap* am, Allocator al, Atom key){
    if(am->count >= am->cap){
        uint32_t old_cap = am->cap;
        uint32_t old_size = (sizeof(Atom)+sizeof(void*))*old_cap + 2*old_cap*sizeof(uint32_t);
        uint32_t new_cap = old_cap?old_cap*2:4;
        uint32_t new_size = (sizeof(Atom)+sizeof(void*))*new_cap + 2*new_cap*sizeof(uint32_t);
        uint32_t count = am->count;
        void* data = Allocator_realloc(al, am->data, old_size, new_size);
        if(!data) return NULL;
        void** items = data;
        uint32_t* idxes = (uint32_t*)(void*)(new_cap*(sizeof(Atom)+(sizeof(void*)))+(char*)(data));
        memset(idxes, 0, sizeof(uint32_t)*2*new_cap);
        for(uint32_t i = 0; i < count; i++){
            Atom k = items[i*2];
            uint32_t hash = k->hash;
            uint32_t idx = fast_reduce32(hash, 2*new_cap);
            while(idxes[idx]){
                idx++;
                if(idx >= 2*new_cap) idx = 0;
            }
            idxes[idx] = i+1;
        }
        am->data = data;
        am->cap = new_cap;
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
            idxes[idx] = i+1;
            return &items[2*i+1];
        }
        i--;
        Atom a = items[2*i];
        if(a == key){
            return &items[2*i+1];
            return 0;
        }
        idx++;
        if(idx >= 2*cap) idx = 0;
    }
}
*/

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

#ifdef __clang__
#pragma clang assume_nonnull end
#endif
#endif
