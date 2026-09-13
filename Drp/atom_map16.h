//
// Copyright © 2026-2026, David Priver <david@davidpriver.com>
//
#ifndef ATOM_MAP16_H
#define ATOM_MAP16_H 1
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

typedef struct AtomMap16 AtomMap16;
struct AtomMap16 {
    void* data;
    uint32_t count;
    uint32_t cap;
};



#define AtomMap16(T) AtomMap16

typedef struct AtomMap16Item AtomMap16Item;
struct AtomMap16Item {
    union {
        _Alignas(uint64_t) Atom atom;
        uint64_t key;
    };
    uint64_t payload[2];
};
_Static_assert(sizeof(uint64_t) >= sizeof(uintptr_t), "");
_Static_assert(sizeof(AtomMap16Item) == sizeof(uint64_t)*3, "");

typedef struct AtomMap16Items AtomMap16Items;
struct AtomMap16Items{
    AtomMap16Item* data;
    size_t count;
};

static inline
AtomMap16Items
AM16_items(const AtomMap16* am){
    return (AtomMap16Items){am->data, am->count};
}
static
inline
size_t
AM16_alloc_size(size_t cap){
    return (sizeof(AtomMap16Item))*cap + 2*cap*sizeof(uint32_t);
}
// There is no `AM16_cleanup()` to force you to think about the lifetime of the pointer to items.
static inline
void
AM16_del(AtomMap16* am, Atom key){
    if(!am->count) return;
    uint32_t hash = key->hash;
    uint32_t cap = am->cap;
    uint32_t idx = fast_reduce32(hash, 2*cap);
    void* data = am->data;
    AtomMap16Item* items = data;
    uint32_t* idxes = (uint32_t*)(void*)(cap*(sizeof(AtomMap16Item))+(char*)(data));
    for(;;){
        uint32_t i = idxes[idx];
        if(!i) return;
        i--;
        if(items[i].atom == key){
            items[i].payload[0] = 0;
            items[i].payload[1] = 0;
            return;
        }
        idx++;
        if(idx >= 2*cap) idx = 0;
    }
}


//
// Put an all-zero payload to delete the key (or call `AM16_del()`).
// It will get removed when map gets resized.
//
// Returns 0 on success and 1 on error.
warn_unused
static inline
int
AM16_put(AtomMap16* am, Allocator al, Atom key, uint64_t payload[_Nonnull static 2]){
    if(!payload[0] && !payload[1]){
        AM16_del(am, key);
        return 0;
    }
    if(am->count >= am->cap){
        uint32_t old_cap = am->cap;
        uint32_t old_size = (uint32_t)AM16_alloc_size(old_cap);
        uint32_t new_cap = old_cap?old_cap*2:4;
        uint32_t new_size = (uint32_t)AM16_alloc_size(new_cap);
        uint32_t count = am->count;
        void* data = Allocator_realloc(al, am->data, old_size, new_size);
        if(!data) return 1;
        AtomMap16Item* items = data;
        uint32_t* idxes = (uint32_t*)(void*)(new_cap*(sizeof(AtomMap16Item))+(char*)(data));
        memset(idxes, 0, sizeof(uint32_t)*2*new_cap);
        uint32_t new_count = 0;
        for(uint32_t i = 0; i < count; i++){
            if(!items[i].payload[0] && !items[i].payload[1]) continue;
            Atom k = items[i].atom;
            if(i != new_count){
                items[new_count].atom = k;
                items[new_count].payload[0] = items[i].payload[0];
                items[new_count].payload[1] = items[i].payload[1];
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
    AtomMap16Item *items = data;
    uint32_t* idxes = (uint32_t*)(void*)(cap*(sizeof(AtomMap16Item))+(char*)(data));
    for(;;){
        uint32_t i = idxes[idx];
        if(!i){
            i = am->count++;
            items[i] = (AtomMap16Item){
                .atom = key,
                .payload = {payload[0], payload[1]},
            };
            idxes[idx] = i+1;
            return 0;
        }
        i--;
        if(items[i].atom == key){
            items[i].payload[0] = payload[0];
            items[i].payload[0] = payload[1];
            return 0;
        }
        idx++;
        if(idx >= 2*cap) idx = 0;
    }
}

static inline
void*_Nullable
AM16_get(const AtomMap16* am, Atom key){
    if(!am->count) return NULL;
    uint32_t hash = key->hash;
    uint32_t cap = am->cap;
    uint32_t idx = fast_reduce32(hash, 2*cap);
    void* data = am->data;
    AtomMap16Item *items = data;
    uint32_t* idxes = (uint32_t*)(void*)(cap*(sizeof(AtomMap16Item))+(char*)(data));
    for(;;){
        uint32_t i = idxes[idx];
        if(!i){
            return NULL;
        }
        i -= 1;
        if(items[i].atom == key)
            return items[i].payload[0] || items[i].payload[1]?items[i].payload:NULL;
        idx++;
        if(idx >= 2*cap) idx = 0;
    }
}

static
void
AM16_clear(AtomMap16* am){
    if(!am->count) return;
    am->count = 0;
    if(am->data){
        uint32_t* idxes = (uint32_t*)(void*)(am->cap*(sizeof(AtomMap16Item))+(char*)(am->data));
        memset(idxes, 0, sizeof(uint32_t)*2*am->cap);
    }
}

static inline
void
AM16_compact(AtomMap16* am){
    if(!am->count || !am->data) return;
    uint32_t cap = am->cap;
    uint32_t count = am->count;
    void* data = am->data;
    AtomMap16Item *items = data;
    uint32_t* idxes = (uint32_t*)(void*)(cap*(sizeof(AtomMap16Item))+(char*)(data));
    memset(idxes, 0, sizeof(uint32_t)*2*cap);
    uint32_t new_count = 0;
    for(uint32_t i = 0; i < count; i++){
        if(!items[i].payload[0] && !items[i].payload[1]) continue;
        Atom k = items[i].atom;
        if(i != new_count){
            items[new_count].atom = k;
            items[new_count].payload[0] = items[i].payload[0];
            items[new_count].payload[1] = items[i].payload[1];
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
