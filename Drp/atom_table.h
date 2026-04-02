//
// Copyright © 2024-2025, David Priver <david@davidpriver.com>
//
#ifndef DRP_ATOM_TABLE_H
#define DRP_ATOM_TABLE_H 1
#include <stddef.h>
#include <string.h>
#include "Allocators/allocator.h"
#include "atom.h"
#include "hash_func.h"
#ifndef __builtin_trap
#if defined _MSC_VER && !defined __clang__
#define __builtin_trap() __fastfail(7)
#endif
#endif

#ifdef __clang__
#pragma clang assume_nonnull begin
// #pragma clang diagnostic ignored "-Wcast-align"
#else
#ifndef _Nullable
#define _Nullable
#endif
#endif

typedef struct AtomTable AtomTable;
struct AtomTable {
    Allocator allocator;
    void* data;
    size_t count;
    size_t cap;
};

//
// If txt is in the table, returns corresponding atom.
// Otherwise, copies the txt, stores copy in the table and returns
// corresponding atom.
//
// Can return NULL on OOM.
//
static inline
Atom _Nullable
AT_atomize(AtomTable* at, const char* txt, size_t len);
#define AT_ATOMIZE(at, txt) AT_atomize(at, txt, sizeof "" txt -1)

//
// Makes an atom without storing it in the table.
//
// Can return NULL on OOM.
static inline
Atom _Nullable
AT_raw_atomize(AtomTable* at, const char* txt, size_t len);

//
// This is for storing atoms you've created yourself.
// They should be generated ahead of time as constant data.
//
// Returns 0 on success, 1 on OOM and 2 if already in the table.
//
static inline
int
AT_store_atom(AtomTable* at, Atom atom);

// Returns the corresponding atom, or NULL if not found.
static inline
Atom _Nullable
AT_get_atom(AtomTable* at, const char* txt, size_t len);

static inline
Atom _Nullable
AT_raw_atomize(AtomTable* at, const char* txt, size_t len){
    if(!len) return nil_atom;
    if(len + 1 > UINT32_MAX/2) return NULL;
    Atom_* atom = Allocator_alloc(at->allocator, 1+len+sizeof *atom);
    if(!atom) return NULL;
    atom->flags = ATOM_ALLOCATED;
    atom->length = (uint32_t)len;
    memcpy(atom->data, txt, len);
    atom->data[len] = 0;
    atom->hash = hash_align1(txt, len);
    return atom;
}

static inline
Atom _Nullable
AT_atomize(AtomTable* at, const char* txt, size_t len){
    Atom atom = AT_get_atom(at, txt, len);
    if(atom) return atom;
    if(!atom) atom = AT_raw_atomize(at, txt, len);
    if(!atom) return NULL;
    int err = AT_store_atom(at, atom);
    if(err){
        if(err == 2) __builtin_trap();
        Allocator_free(at->allocator, atom, 1+atom->length+sizeof *atom);
        return NULL;
    }
    return atom;
}

static inline
int
AT_grow_table(AtomTable* at){
    size_t count = at->count;
    size_t old_cap = at->cap;
    size_t new_cap = old_cap?old_cap*2:64;
    size_t new_size = sizeof(uint32_t)*new_cap*2+new_cap*sizeof(Atom);
    size_t old_size = sizeof(uint32_t)*old_cap*2+old_cap*sizeof(Atom);
    void* new_data = Allocator_realloc(at->allocator, at->data, old_size, new_size);
    if(!new_data) return 1;
    Atom* atoms = new_data;
    uint32_t* idxes = (uint32_t*)(void*)(sizeof(Atom)*new_cap+(char*)new_data);
    memset(idxes, 0, 2*new_cap*sizeof *idxes);
    for(size_t i = 1; i < count; i++){
        Atom a = atoms[i];
        uint32_t hash = a->hash;
        uint32_t idx = fast_reduce32(hash, (uint32_t)new_cap*2);
        while(idxes[idx]){
            idx++;
            if(idx >= 2 * new_cap) idx = 0;
        }
        idxes[idx] = (uint32_t)i;
    }
    if(!count){
        at->count = 1;
        atoms[0] = nil_atom;
    }
    at->cap = new_cap;
    at->data = new_data;
    return 0;
}

static inline
int
AT_store_atom(AtomTable* at, Atom atom){
    int e;
    if(at->count >= at->cap){
        e = AT_grow_table(at);
        if(e) return e;
    }
    uint32_t* idxes = (uint32_t*)(void*)(sizeof(Atom)*at->cap+(char*)at->data);
    uint32_t hash = atom->hash;
    uint32_t idx = fast_reduce32(hash, (uint32_t)at->cap*2);
    Atom* atoms = at->data;
    for(;;){
        uint32_t i = idxes[idx];
        if(!i){
            i = (uint32_t)(at->count++);
            atoms[i] = atom;
            idxes[idx] = i;
            return 0;
        }
        if(atoms[i]->hash == atom->hash && atoms[i]->length == atom->length && memcmp(atoms[i]->data, atom->data, atom->length) == 0)
            return 2;
        if(atoms[i] == atom) return 2;
        idx++;
        if(idx >= at->cap*2) idx = 0;
    }
}

static inline
Atom _Nullable
AT_get_atom(AtomTable* at, const char* txt, size_t len){
    if(!len) return nil_atom;
    if(!at->count) return NULL;
    uint32_t* idxes = (uint32_t*)(void*)(sizeof(Atom)*at->cap+(char*)at->data);
    uint32_t hash = hash_align1(txt, len);
    uint32_t idx = fast_reduce32(hash, (uint32_t)at->cap*2);
    Atom* atoms = at->data;
    for(;;){
        uint32_t i = idxes[idx];
        if(!i) return NULL;
        Atom atom = atoms[i];
        if(atom->length == len && atom->hash == hash && memcmp(atom->data, txt, len)==0){
            return atom;
        }
        idx++;
        if(idx >= at->cap*2) idx = 0;
    }
}

#ifdef __clang__
#pragma clang assume_nonnull end
#endif
#endif
