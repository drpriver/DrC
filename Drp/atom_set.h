//
// Copyright © 2024-2025, David Priver <david@davidpriver.com>
//
#ifndef ATOM_SET_H
#define ATOM_SET_H 1

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

typedef struct AtomSet AtomSet;
struct AtomSet {
    void* data;
    uint32_t count;
    uint32_t cap;
};

typedef struct AtomSetItems AtomSetItems;
struct AtomSetItems {
    Atom _Null_unspecified *_Null_unspecified data;
    size_t count;
};

static inline
AtomSetItems
AS_items(const AtomSet* as){
    return (AtomSetItems){(Atom*)as->data, as->count};
}

static inline
size_t
AS_alloc_size(size_t cap){
    return sizeof(Atom)*cap + 2*cap*sizeof(uint32_t);
}

static inline
void
AS_del(AtomSet* as, Atom atom){
    if(!as->count) return;
    uint32_t hash = atom->hash;
    uint32_t cap = as->cap;
    uint32_t idx = fast_reduce32(hash, 2*cap);
    void* data = as->data;
    Atom* items = data;
    uint32_t* idxes = (uint32_t*)(void*)(cap*sizeof(Atom)+(char*)(data));
    for(;;){
        uint32_t i = idxes[idx];
        if(!i) return;
        i--;
        Atom a = items[i];
        if(a == atom){
            // Lazy deletion: just set to NULL
            // Future lookups will compare not equal to this slot and keep probing.
            items[i] = NULL;
            return;
        }
        idx++;
        if(idx >= 2*cap) idx = 0;
    }
}

warn_unused
static inline
int
AS_add(AtomSet* as, Allocator al, Atom atom){
    if(as->count >= as->cap){
        uint32_t old_cap = as->cap;
        uint32_t old_size = (uint32_t)AS_alloc_size(old_cap);
        uint32_t new_cap = old_cap?old_cap*2:4;
        uint32_t new_size = (uint32_t)AS_alloc_size(new_cap);
        uint32_t count = as->count;
        void* data = Allocator_realloc(al, as->data, old_size, new_size);
        if(!data) return 1;
        Atom* items = data;
        uint32_t* idxes = (uint32_t*)(void*)(new_cap*sizeof(Atom)+(char*)(data));
        memset(idxes, 0, sizeof(uint32_t)*2*new_cap);
        uint32_t new_count = 0;
        // Compact: skip NULL (deleted) entries
        for(uint32_t i = 0; i < count; i++){
            if(!items[i]) continue;
            Atom k = items[i];
            if(i != new_count){
                items[new_count] = k;
            }
            uint32_t hash = k->hash;
            uint32_t idx = fast_reduce32(hash, 2*new_cap);
            while(idxes[idx]){
                idx++;
                if(idx >= 2*new_cap) idx = 0;
            }
            idxes[idx] = ++new_count;
        }
        as->count = new_count;
        as->data = data;
        as->cap = new_cap;
    }
    uint32_t hash = atom->hash;
    uint32_t cap = as->cap;
    uint32_t idx = fast_reduce32(hash, 2*cap);
    void* data = as->data;
    Atom* items = data;
    uint32_t* idxes = (uint32_t*)(void*)(cap*sizeof(Atom)+(char*)(data));

    int32_t first_zombie = -1;  // Track first NULL (zombie) slot we encounter

    for(;;){
        uint32_t i = idxes[idx];
        if(!i){
            // Empty slot - reuse zombie
            if(first_zombie >= 0){
                items[first_zombie] = atom;
                return 0;
            }
            i = as->count++;
            items[i] = atom;
            idxes[idx] = i+1;
            return 0;
        }
        i--;
        Atom a = items[i];
        if(!a && first_zombie < 0)
            // Found a zombie (NULL atom), remember it for potential reuse
            first_zombie = (int32_t)i;
        else if(a == atom)
            return 0;
        idx++;
        if(idx >= 2*cap) idx = 0;
    }
}

static inline
_Bool
AS_has(const AtomSet* as, Atom atom){
    if(!as->count) return 0;
    uint32_t hash = atom->hash;
    uint32_t cap = as->cap;
    uint32_t idx = fast_reduce32(hash, 2*cap);
    void* data = as->data;
    Atom* items = data;
    uint32_t* idxes = (uint32_t*)(void*)(cap*sizeof(Atom)+(char*)(data));
    for(;;){
        uint32_t i = idxes[idx];
        if(!i) return 0;
        i -= 1;
        Atom a = items[i];
        if(a == atom) return 1;
        idx++;
        if(idx >= 2*cap) idx = 0;
    }
}

// Set presence of atom in set based on boolean parameter
// If should_be_present is true, adds the atom. If false, removes it.
// Returns 0 on success, 1 on allocation failure (only possible when adding).
warn_unused
static inline
int
AS_set(AtomSet* as, Allocator al, Atom atom, _Bool should_be_present){
    if(should_be_present)
        return AS_add(as, al, atom);
    AS_del(as, atom);
    return 0;
}

static
void
AS_clear(AtomSet* as){
    if(!as->count) return;
    as->count = 0;
    if(as->data){
        uint32_t* idxes = (uint32_t*)(void*)(as->cap*sizeof(Atom)+(char*)(as->data));
        memset(idxes, 0, sizeof(uint32_t)*2*as->cap);
    }
}

#ifdef __clang__
#pragma clang assume_nonnull end
#endif
#endif
