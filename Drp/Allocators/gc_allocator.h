#ifndef DRP_GC_ALLOCATOR_H
#define DRP_GC_ALLOCATOR_H

#ifndef USE_GC_ALLOCATOR
#ifdef ALLOCATOR_H
#error "didn't define USE_GC_ALLOCATOR"
#endif
#define USE_GC_ALLOCATOR
#endif

#include "allocator.h"
#include <string.h>
#ifdef DEBUG_ALLOCATIONS
#include <stdio.h>
#endif

#ifdef __clang__
#pragma clang assume_nonnull begin
#else
#ifndef _Nullable
#define _Nullable
#endif
#endif

#ifndef force_inline
#if defined(__GNUC__) || defined(__clang__)
#define force_inline static inline __attribute__((always_inline))
#elif defined(_MSC_VER)
#define force_inline static inline __forceinline
#else
#define force_inline static inline
#endif
#endif

typedef struct GcHeader GcHeader;
struct GcHeader {
    GcHeader*_Null_unspecified next;
    size_t sz: 62;
    size_t is_free: 1;
    size_t mark: 1;
    size_t data[];
};

typedef struct GcAllocator GcAllocator;
struct GcAllocator {
    GcHeader*_Null_unspecified freebuckets[16];
    GcHeader*_Null_unspecified livelist;
    _Bool mark;
    Allocator allocator;
};

force_inline
Allocator
allocator_from_gc(GcAllocator* gc){
    return (Allocator){.type=ALLOCATOR_GC, ._data=gc};
}

static
size_t
gc_get_bucket(size_t sz){
    if(!sz) return 0;
    sz--;
    sz >>= 5;
    if(!sz)
        return 0;
    size_t result;
    #ifdef __GNUC__
        result = (sizeof(size_t) * 8) - __builtin_clzll((unsigned long long)sz);
    #elif defined(_MSC_VER)
        unsigned long index;
        if(sizeof sz == 8) // 64-bit size_t
            _BitScanReverse64(&index, (unsigned __int64)sz);
        else
            _BitScanReverse(&index, (unsigned long)sz);
        result = (size_t)index+1;
    #else
        #error "fixme"
    #endif
    return result;
}
static
size_t
gc_bucket_to_size(size_t bucket){
    return (size_t)1 << (bucket+5);
}

static
void* _Nullable
gc_alloc(GcAllocator* gc, size_t sz){
    if(!sz) return NULL;
    sz += sizeof(GcHeader);
    size_t bucket = gc_get_bucket(sz);
    if(bucket < sizeof gc->freebuckets / sizeof gc->freebuckets[0]){
        GcHeader* h;
        if(gc->freebuckets[bucket]){
            h = gc->freebuckets[bucket];
            gc->freebuckets[bucket] = h->next;
        }
        else {
            size_t allocsz = gc_bucket_to_size(bucket);
            h = Allocator_alloc(gc->allocator, allocsz);
            if(!h) return NULL;
            h->sz = allocsz;
        }
        h->mark = gc->mark;
        h->next = gc->livelist;
        h->is_free = 0;
        gc->livelist = h;
        return h+1;
    }
    GcHeader* h;
    size_t allocsz = sz + sizeof(GcHeader);
    h = Allocator_alloc(gc->allocator, allocsz);
    if(!h) return NULL;
    h->sz = allocsz;
    h->mark = gc->mark;
    h->next = gc->livelist;
    h->is_free = 0;
    gc->livelist = h;
    return h+1;
}

static
void* _Nullable
gc_zalloc(GcAllocator* gc, size_t sz){
    if(!sz) return NULL;
    sz += sizeof(GcHeader);
    size_t bucket = gc_get_bucket(sz);
    if(bucket < sizeof gc->freebuckets / sizeof gc->freebuckets[0]){
        GcHeader* h;
        size_t allocsz = gc_bucket_to_size(bucket);
        if(gc->freebuckets[bucket]){
            h = gc->freebuckets[bucket];
            gc->freebuckets[bucket] = h->next;
            memset(h, 0, allocsz);
        }
        else {
            h = Allocator_zalloc(gc->allocator, allocsz);
            if(!h) return NULL;
            h->sz = allocsz;
        }
        h->mark = gc->mark;
        h->next = gc->livelist;
        h->is_free = 0;
        gc->livelist = h;
        return h+1;
    }
    GcHeader* h;
    size_t allocsz = sz + sizeof(GcHeader);
    h = Allocator_zalloc(gc->allocator, allocsz);
    if(!h) return NULL;
    h->sz = allocsz;
    h->mark = gc->mark;
    h->next = gc->livelist;
    h->is_free = 0;
    gc->livelist = h;
    return h+1;
}

static
void
gc_free(GcAllocator* gc, const void* _Nullable data, size_t orig_size){
    if(!data || !orig_size) return;
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
    GcHeader* h = (GcHeader*)data;
    #if defined(__clang__) || defined(__GNUC__)
        #pragma GCC diagnostic pop
    #elif defined(_MSC_VER)
        #pragma warning(pop)
    #endif
    h--;
    if(h->is_free) __builtin_trap();
    if(h == gc->livelist){
        h->is_free = 1;
        while(h && h->is_free){
            gc->livelist = h->next;
            size_t bucket = gc_get_bucket(orig_size);
            if(bucket < sizeof gc->freebuckets / sizeof gc->freebuckets[0]){
                h->is_free = 1;
                h->next = gc->freebuckets[bucket];
                gc->freebuckets[bucket] = h;
            }
            else {
                Allocator_free(gc->allocator, h, h->sz);
            }
            h = gc->livelist;
        }
        return;
    }
    size_t bucket = gc_get_bucket(orig_size);
    if(bucket < sizeof gc->freebuckets / sizeof gc->freebuckets[0]){
        h->is_free = 1;
        h->next = gc->freebuckets[bucket];
        gc->freebuckets[bucket] = h;
    }
    else {
        Allocator_free(gc->allocator, h, h->sz);
    }
}

static
void* _Nullable
gc_realloc(GcAllocator* gc, void* _Nullable data, size_t orig_size, size_t new_size){
    if(!new_size){
        gc_free(gc, data, orig_size);
        return NULL;
    }
    if(!data) return gc_alloc(gc, new_size);
    if(!orig_size) return gc_alloc(gc, new_size);
    GcHeader* h = data;
    h--;
    if(h->sz - sizeof *h > new_size) return data;
    void* result = gc_alloc(gc, new_size);
    if(!result) return NULL;
    size_t cpy_size = orig_size < new_size? orig_size : new_size;
    memcpy(result, data, cpy_size);
    gc_free(gc, data, orig_size);
    return result;
}

static
void
gc_flip_mark(GcAllocator* gc){
    gc->mark ^= 1;
}

static
void
gc_mark_one(GcAllocator* gc, void* _Nullable data){
    if(!data) return;
    GcHeader* h = data;
    h--;
    h->mark = gc->mark;
}

static
void
gc_sweep(GcAllocator* gc){
    GcHeader** prev = &gc->livelist;
    GcHeader* h = gc->livelist;
    for(;h;){
        GcHeader* next = h->next;
        if(h->is_free){
            *prev = next;
            size_t bucket = gc_get_bucket(h->sz);
            if(bucket < sizeof gc->freebuckets / sizeof gc->freebuckets[0]){
                h->next = gc->freebuckets[bucket];
                gc->freebuckets[bucket] = h;
            }
            else {
                Allocator_free(gc->allocator, h, h->sz);
            }
            h = next;
            continue;
        }
        if(h->mark != gc->mark){
            gc_free(gc, h+1, h->sz);
            *prev = next;
            h = next;
            continue;
        }
        prev = &(*prev)->next;
        h = next;
    }
}

static
void
gc_free_all(GcAllocator* gc){
    if(Allocator_supports_free_all(gc->allocator)){
        Allocator_free_all(gc->allocator);
        gc->livelist = NULL;
        memset(gc->freebuckets, 0, sizeof gc->freebuckets);
        return;
    }
    for(GcHeader* h = gc->livelist, *temp;;){
        if(!h) return;
        temp = h->next;
        gc_free(gc, h, h->sz);
        h = temp;
    }
}

#ifdef DEBUG_ALLOCATIONS
static
void
gc_stats(GcAllocator* gc){
    for(size_t i = 0; i < sizeof gc->freebuckets / sizeof gc->freebuckets[0]; i++){
        size_t count = 0;
        size_t sum = 0;
        for(GcHeader* h = gc->freebuckets[i]; h; h = h->next){
            count++;
            sum += h->sz;
        }
        fprintf(stderr, "%zu] %zu totalling %zu bytes\n", i, count, sum);
    }
    size_t count = 0;
    size_t sum = 0;
    size_t freed = 0;
    for(GcHeader* h = gc->livelist; h; h = h->next){
        count++;
        if(h->is_free)
            freed++;
        sum += h->sz;
    }
    fprintf(stderr, "%zu live totalling %zu bytes, %zu freed\n", count, sum, freed);
}
#else
static
void
gc_stats(GcAllocator* gc){
    (void)gc;
}
#endif


#ifdef __clang__
#pragma clang assume_nonnull end
#endif
#endif
