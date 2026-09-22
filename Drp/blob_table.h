//
// Copyright © 2026-2026, David Priver <david@davidpriver.com>
//
#ifndef DRP_BLOB_TABLE_H
#define DRP_BLOB_TABLE_H 1
#include <stddef.h>
#include <string.h>
#include "Allocators/arena_allocator.h"
#include "blob_atom.h"
#include "../Vendored/rapidhash.h"
#ifndef __builtin_trap
#if defined _MSC_VER && !defined __clang__
#define __builtin_trap() __fastfail(7)
#endif
#endif

#ifdef __clang__
#pragma clang assume_nonnull begin
#else
#ifndef _Nullable
#define _Nullable
#endif
#endif

force_inline
uint64_t
fast_reduce64(uint64_t x, uint64_t y){
    #ifdef __SIZEOF_INT128__
    return (uint64_t)(((__uint128_t)x * (__uint128_t)y) >> 64);
    #elif defined _MSC_VER
    _umul128(x,y,&y);
    return y;
    #else
    uint64_t x0 = (uint32_t)x;
    uint64_t x1 = x >> 32;
    uint64_t y0 = (uint32_t)y;
    uint64_t y1 = y >> 32;
    uint64_t p00 = x0 * y0;
    uint64_t p01 = x0 * y1;
    uint64_t p10 = x1 * y0;
    uint64_t p11 = x1 * y1;
    uint64_t middle = (p00 >> 32) + (uint32_t)p01 + (uint32_t)p10;
    return p11 + (p01 >> 32) + (p10 >> 32) + (middle >> 32);
    #endif
}

typedef struct BlobTable BlobTable;
struct BlobTable {
    ArenaAllocator arena;
    void* data;
    size_t count, cap;
};

//
// If blob is in the table, returns corresponding atom.
// Otherwise, copies the blob, stores copy in the table and returns
// corresponding atom.
//
// Can return NULL on OOM.
//
static inline
BlobAtom _Nullable
BT_atomize(BlobTable* bt, const void*, size_t len);

//
// Makes an atom without storing it in the table.
//
// Can return NULL on OOM.
static inline
BlobAtom _Nullable
BT_raw_atomize(BlobTable* bt, const void*, size_t len);

// Returns the corresponding atom, or NULL if not found.
static inline
BlobAtom _Nullable
BT_get_atom(BlobTable* bt, const void*, size_t len);

static inline
BlobAtom _Nullable
BT_raw_atomize(BlobTable* bt, const void* blob, size_t len){
    if(!len) return nil_blob;
    // Include padding and the atom header in the maximum object size.
    if(len > (size_t)PTRDIFF_MAX - sizeof(BlobAtom_) - 15) return NULL;
    size_t blen = (len + 0xf) & ~(size_t)0xf;
    BlobAtom_* atom = ArenaAllocator_zalloc(&bt->arena, blen+sizeof *atom);
    if(!atom) return NULL;
    atom->length = blen;
    memcpy(atom->data, blob, len);
    atom->hash = rapidhash(atom->data, blen);
    return atom;
}

static inline int BT_store_atom(BlobTable* bt, BlobAtom atom);

static inline
BlobAtom _Nullable
BT_atomize(BlobTable* bt, const void* blob, size_t len){
    BlobAtom atom = BT_get_atom(bt, blob, len);
    if(atom) return atom;
    if(!atom) atom = BT_raw_atomize(bt, blob, len);
    if(!atom) return NULL;
    int err = BT_store_atom(bt, atom);
    if(err){
        ArenaAllocator_free(&bt->arena, atom, atom->length+sizeof *atom);
        return NULL;
    }
    return atom;
}

static inline
int
BT_grow_table(BlobTable* bt){
    if(bt->count >= UINT32_MAX) return 1;
    size_t count = bt->count;
    size_t old_cap = bt->cap;
    size_t new_cap = old_cap?old_cap*2:64;
    size_t new_size = sizeof(uint32_t)*new_cap*2+new_cap*sizeof(BlobAtom);
    size_t old_size = sizeof(uint32_t)*old_cap*2+old_cap*sizeof(BlobAtom);
    void* new_data = ArenaAllocator_realloc(&bt->arena, bt->data, old_size, new_size);
    if(!new_data) return 1;
    BlobAtom* atoms = new_data;
    uint32_t* idxes = (uint32_t*)(void*)(sizeof(BlobAtom)*new_cap+(char*)new_data);
    memset(idxes, 0, 2*new_cap*sizeof *idxes);
    for(size_t i = 1; i < count; i++){
        BlobAtom a = atoms[i];
        uint64_t hash = a->hash;
        uint64_t idx = fast_reduce64(hash, (uint64_t)new_cap*2);
        while(idxes[idx]){
            idx++;
            if(idx >= 2 * new_cap) idx = 0;
        }
        idxes[idx] = (uint32_t)i;
    }
    if(!count){
        bt->count = 1;
        atoms[0] = nil_blob;
    }
    bt->cap = new_cap;
    bt->data = new_data;
    return 0;
}


static inline
int
BT_store_atom(BlobTable* bt, BlobAtom atom){
    int e;
    if(bt->count >= bt->cap){
        e = BT_grow_table(bt);
        if(e) return e;
    }
    uint32_t* idxes = (uint32_t*)(void*)(sizeof(BlobAtom)*bt->cap+(char*)bt->data);
    uint64_t hash = atom->hash;
    uint64_t idx = fast_reduce64(hash, (uint64_t)bt->cap*2);
    BlobAtom* atoms = bt->data;
    for(;;){
        uint32_t i = idxes[idx];
        if(!i){
            i = (uint32_t)(bt->count++);
            atoms[i] = atom;
            idxes[idx] = i;
            return 0;
        }
        if(atoms[i]->hash == atom->hash && atoms[i]->length == atom->length && memcmp(atoms[i]->data, atom->data, atom->length) == 0) // TODO: 16-byte aligned memeq
            return 2;
        if(atoms[i] == atom) return 2;
        idx++;
        if(idx >= bt->cap*2) idx = 0;
    }
}

static inline
BlobAtom _Nullable
BT_get_atom(BlobTable* bt, const void* blob, size_t len){
    if(!len) return nil_blob;
    if(len > (size_t)PTRDIFF_MAX - sizeof(BlobAtom_) - 15) return NULL;
    if(!bt->count) return NULL;
    uint32_t* idxes = (uint32_t*)(void*)(sizeof(BlobAtom)*bt->cap+(char*)bt->data);
    uint64_t hash = rapidhash_padded(blob, len);
    uint64_t idx = fast_reduce64(hash, (uint64_t)bt->cap*2);
    size_t trunc = len & ~(size_t)15;
    size_t up = (len+15)&~(size_t)15;
    BlobAtom* atoms = bt->data;
    for(;;){
        uint32_t i = idxes[idx];
        if(!i) return NULL;
        BlobAtom atom = atoms[i];
        if(atom->length == up && atom->hash == hash && memcmp(atom->data, blob, trunc)==0){
            if(trunc != len){
                uint64_t remainder[2] = {0};
                memcpy(remainder, (const char*)blob+trunc, len-trunc);
                if(memcmp((const char*)atom->data+trunc, remainder, sizeof remainder))
                    goto Continue;
            }
            return atom;
        }
        Continue:;
        idx++;
        if(idx >= bt->cap*2) idx = 0;
    }
}

#ifdef __clang__
#pragma clang assume_nonnull end
#endif
#endif
