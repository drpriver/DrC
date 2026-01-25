//
// Copyright © 2024-2025, David Priver <david@davidpriver.com>
//
// template-style header
#ifndef V2I_MAP_COMMON_H
#define V2I_MAP_COMMON_H
#include <stdint.h>
#include "hash_func.h"
#include "Allocators/allocator.h"
#include "v2.h"

#ifndef warn_unused
#if defined(__GNUC__) || defined(__clang__)
#define warn_unused __attribute__((warn_unused_result))
#elif defined(_MSC_VER)
#define warn_unused
#else
#define warn_unused
#endif
#endif

#define MAP_FOR_EACH(type, iter, dict) \
for(type \
     *iter = (dict).items, \
     *iter##end__ = (dict).items?((dict).items+(dict).count):NULL; \
  iter != iter##end__; \
  ++iter)
#define MAP_FOR_EACH_VALUE(type, iter, dict) \
for(type \
     *iter = (dict).count? &(dict).items[0].value:NULL, \
     *iter##end__ = (dict).count?&(dict.items[dict.count].value):NULL; \
  iter != iter##end__; \
  iter = (type*)((char*)iter+sizeof *(dict).items))
#define MAP_FOR_EACH_KEY(type, iter, dict) \
for(type \
     *iter = (dict).count? &(dict).items[0].key:NULL, \
     *iter##end__ = (dict).count?&(dict.items[dict.count].key):NULL; \
  iter != iter##end__; \
  iter = (type*)((char*)iter+sizeof *(dict).items))
#endif

#ifndef V2IMAP_V
#error "define V2IMAP_V"
#endif

#ifndef V2IMAP_NAME
#error "Define V2IMAP_NAME"
#endif


#ifdef __clang__
#pragma clang assume_nonnull begin
#else
#ifndef _Null_unspecified
#define _Null_unspecified
#endif
#ifndef _Nullable
#define _Nullable
#endif
#endif

#define IDENTCAT2(a, b) a##b
#define IDENTCAT(a, b) IDENTCAT2(a, b)
#define V2IMAP_ITEM              IDENTCAT(V2IMAP_NAME, _Item)
#define V2IMAP_ensure_additional IDENTCAT(V2IMAP_NAME, _ensure_additional)
#define V2IMAP_getsert           IDENTCAT(V2IMAP_NAME, _getsert)
#define V2IMAP_get               IDENTCAT(V2IMAP_NAME, _get)
#define V2IMAP_clear             IDENTCAT(V2IMAP_NAME, _clear)
#define V2IMAP_cleanup           IDENTCAT(V2IMAP_NAME, _cleanup)
#define V2IMAP_put               IDENTCAT(V2IMAP_NAME, _put)

typedef struct V2IMAP_ITEM V2IMAP_ITEM;
struct V2IMAP_ITEM {
    v2i key;
    V2IMAP_V value;
};

typedef struct V2IMAP_NAME V2IMAP_NAME;
struct V2IMAP_NAME {
    Allocator a;
    V2IMAP_ITEM*_Null_unspecified items;
    size_t count;
    size_t cap;
};

static inline
warn_unused
int
V2IMAP_ensure_additional(V2IMAP_NAME* dict, size_t n){
    if(100*(dict->count+n) < 70*(dict->cap)) return 0;
    size_t old_cap = dict->cap;
    size_t new_cap = old_cap?old_cap*2: 8;
    while(new_cap < n)
        new_cap *= 2;
    size_t old_size = old_cap* sizeof *dict->items + 2*old_cap*sizeof(uint32_t);
    size_t new_size = new_cap* sizeof *dict->items + 2*new_cap*sizeof(uint32_t);
    void* data = Allocator_realloc(dict->a, dict->items, old_size, new_size);
    if(!data) return 1;
    V2IMAP_ITEM* items = data;
    uint32_t* idxes = (uint32_t*)((char*)data + new_cap * sizeof *items);
    memset(idxes, 0, 2*new_cap*sizeof *idxes);
    for(size_t i = 0; i < dict->count; i++){
        V2IMAP_ITEM* item = &items[i];
        uint32_t hash = hash_align8(&item->key, sizeof item->key);
        uint32_t idx = fast_reduce32(hash, (uint32_t)new_cap*2);
        while(idxes[idx]){
            idx++;
            if(idx >= 2 * new_cap) idx = 0;
        }
        idxes[idx] = (uint32_t)(i+1);
    }
    dict->cap = new_cap;
    dict->items = data;
    return 0;
}

static inline
warn_unused
V2IMAP_V*_Nullable
V2IMAP_getsert(V2IMAP_NAME* dict, v2i key){
    if(V2IMAP_ensure_additional(dict, 1) != 0)
        return NULL;
    uint32_t hash = hash_align8(&key, sizeof key);
    uint32_t idx = fast_reduce32(hash, (uint32_t)dict->cap*2);
    V2IMAP_ITEM* items = dict->items;
    uint32_t* idxes = (uint32_t*)((char*)items + dict->cap* sizeof *items);
    for(;;){
        uint32_t i = idxes[idx];
        if(!i){
            i = (uint32_t)(dict->count++);
            items[i].key = key;
            idxes[idx] = (uint32_t)i+1;
            return &items[i].value;
        }
        else if(v2i_eq(items[i-1].key, key))
            return &items[i-1].value;
        idx++;
        if(idx >= dict->cap*2) idx = 0;
    }
}

static inline
warn_unused
int
V2IMAP_put(V2IMAP_NAME* dict, v2i key, V2IMAP_V val){
    V2IMAP_V* v = V2IMAP_getsert(dict, key);
    if(!v) return 1;
    *v = val;
    return 0;
}

static inline
V2IMAP_V*_Nullable
V2IMAP_get(V2IMAP_NAME* dict, v2i key){
    uint32_t hash = hash_align8(&key, sizeof key);
    uint32_t idx = fast_reduce32(hash, (uint32_t)dict->cap*2);
    V2IMAP_ITEM* items = dict->items;
    uint32_t* idxes = (uint32_t*)((char*)items + dict->cap * sizeof *items);
    for(;;){
        uint32_t i = idxes[idx];
        if(!i){
            return NULL;
        }
        else if(v2i_eq(items[i-1].key, key))
            return &items[i-1].value;
        idx++;
        if(idx >= dict->cap*2) idx = 0;
    }
    return NULL;
}

static inline
void
V2IMAP_clear(V2IMAP_NAME* dict){
    if(dict->count){
        V2IMAP_ITEM* items = dict->items;
        uint32_t* idxes = (uint32_t*)((char*)items + dict->cap * sizeof *items);
        memset(idxes, 0, 2*dict->cap*sizeof *idxes);
    }
    dict->count = 0;
}

static inline
void
V2IMAP_cleanup(V2IMAP_NAME* dict){
    if(dict->cap){
        size_t cap = dict->cap;
        size_t size = cap* sizeof *dict->items + 2*cap*sizeof(uint32_t);
        Allocator_free(dict->a, dict->items, size);
        dict->cap = 0;
        dict->items = NULL;
        dict->count = 0;
    }
}

#if defined(__clang__) && defined(V2IMAP_USE_OVERLOADS)
#define V2IMAP_OVERLOAD static inline /*__attribute__((always_inline)) */ __attribute__((overloadable))
// overloads
V2IMAP_OVERLOAD
warn_unused
int
ensure_additional(V2IMAP_NAME* dict, size_t n){
    return V2IMAP_ensure_additional(dict, n);
}

V2IMAP_OVERLOAD
warn_unused
V2IMAP_V*_Nullable
getsert(V2IMAP_NAME* dict, v2i key){
    return V2IMAP_getsert(dict, key);
}

V2IMAP_OVERLOAD
V2IMAP_V*_Nullable
get(V2IMAP_NAME* dict, v2i key){
    return V2IMAP_get(dict, key);
}

static inline
warn_unused
int
put(V2IMAP_NAME* dict, v2i key, V2IMAP_V val){
    return V2IMAP_put(dict, key, val);
}

V2IMAP_OVERLOAD
void
clear(V2IMAP_NAME* dict){
    V2IMAP_clear(dict);
}

V2IMAP_OVERLOAD
void
cleanup(V2IMAP_NAME* dict){
    V2IMAP_cleanup(dict);
}

#undef V2IMAP_OVERLOAD
#endif
#undef IDENTCAT2
#undef IDENTCAT
#undef V2IMAP_V
#undef V2IMAP_NAME
#undef V2IMAP_ITEM
#undef V2IMAP_ensure_additional
#undef V2IMAP_getsert
#undef V2IMAP_get
#undef V2IMAP_clear
#undef V2IMAP_cleanup

#ifdef __clang__
#pragma clang assume_nonnull end
#endif
