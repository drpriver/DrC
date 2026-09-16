//
// Copyright © 2026-2026, David Priver <david@davidpriver.com>
//
// Like an AtomMap, but you can push anonymous members into it.
// So it is sort of a hybrid of a dynamic array and a hashtable.
// If the thing has a name, you can look it up by name.
// If it doesn't, it is still stored and can be found
// when you scan over the table.
// This is useful for tracking things like local variables
// when you can have unnamed ones like unnamed params or compound
// literals.
//
#ifndef CC_ANON_ATOM_MAP_H
#define CC_ANON_ATOM_MAP_H 1
#include <stdint.h>
#include "../Drp/Allocators/allocator.h"
#include "../Drp/atom.h"
#include "../Drp/hash_func.h"
#include "../Drp/atom_map.h"
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

typedef struct CcAnonAtomMap CcAnonAtomMap;
struct CcAnonAtomMap {
    void* data;
    uint32_t count;
    uint32_t cap;
};



#define CcAnonAtomMap(T) CcAnonAtomMap

static inline
AtomMapItems
CcAnonAM_items(const CcAnonAtomMap* am){
    return (AtomMapItems){am->data, am->count};
}

// Returns 0 on success and 1 on error.
warn_unused
static inline
int
CcAnonAM_put(CcAnonAtomMap* am, Allocator al, Atom key, const void*_Nonnull value_){
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
        AtomMapItem* items = data;
        uint32_t* idxes = (uint32_t*)(void*)(new_cap*sizeof(AtomMapItem)+(char*)(data));
        memset(idxes, 0, sizeof(uint32_t)*2*new_cap);
        for(uint32_t i = 0; i < count; i++){
            Atom k = items[i].atom;
            if(k == nil_atom) continue;
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
    AtomMapItem* items = am->data;
    if(key == nil_atom){
        items[am->count].atom = key;
        items[am->count++].p = value;
        return 0;
    }
    uint32_t hash = key->hash;
    uint32_t cap = am->cap;
    uint32_t idx = fast_reduce32(hash, 2*cap);
    void* data = am->data;
    uint32_t* idxes = (uint32_t*)(void*)(cap*(sizeof(AtomMapItem))+(char*)(data));
    for(;;){
        uint32_t i = idxes[idx];
        if(!i){
            i = am->count++;
            items[i].atom = key;
            items[i].p = value;
            idxes[idx] = i+1;
            return 0;
        }
        i--;
        Atom a = items[i].atom;
        if(a == key){
            items[i].p = value;
            return 0;
        }
        idx++;
        if(idx >= 2*cap) idx = 0;
    }
}

static inline
void*_Nullable
CcAnonAM_get(const CcAnonAtomMap* am, Atom key){
    if(!am->count) return NULL;
    if(key == nil_atom) return NULL;
    uint32_t hash = key->hash;
    uint32_t cap = am->cap;
    uint32_t idx = fast_reduce32(hash, 2*cap);
    void* data = am->data;
    AtomMapItem *items = data;
    uint32_t* idxes = (uint32_t*)(void*)(cap*sizeof(AtomMapItem)+(char*)(data));
    for(;;){
        uint32_t i = idxes[idx];
        if(!i) return NULL;
        i -= 1;
        Atom a = items[i].atom;
        if(a == key) return items[i].p;
        idx++;
        if(idx >= 2*cap) idx = 0;
    }
}

static
void
CcAnonAM_clear(CcAnonAtomMap* am){
    if(!am->count) return;
    am->count = 0;
    if(am->data){
        uint32_t* idxes = (uint32_t*)(void*)(am->cap*sizeof(AtomMapItem)+(char*)(am->data));
        memset(idxes, 0, sizeof(uint32_t)*2*am->cap);
    }
}

#ifdef __clang__
#pragma clang assume_nonnull end
#endif
#endif
