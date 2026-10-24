//
// Copyright © 2021-2026, David Priver <david@davidpriver.com>
//
#ifndef ARENA_ALLOCATOR_H
#define ARENA_ALLOCATOR_H
// size_t
#include <stddef.h>
// memcpy, memset
#include <string.h>
#include <assert.h>
#include <stdint.h>
#include "../ckdint.h"
#include "allocator.h"
#ifndef ARENA_EXPLICIT_ALLOCATOR
#include "mallocator.h"
#endif


#ifdef __clang__
#pragma clang assume_nonnull begin
#else
#ifndef _Nullable
#define _Nullable
#endif
#endif

#ifndef force_inline
#if defined __GNUC__ || defined __clang__ || defined __DRC__
#define force_inline static inline __attribute__((always_inline))
#elif defined(_MSC_VER)
#define force_inline static inline __forceinline
#else
#define force_inline static inline
#endif
#endif

//
// Fairly basic arena allocator. If it can fit an allocation, it just bumps a
// pointer.  If it can't, it allocates a new block of memory and maintains the
// arenas as a linked list.
//
// If the allocation is bigger than would fit in an arena, it allocates it
// independently and maintains a linked list of these big allocations.
//
typedef struct ArenaAllocator ArenaAllocator;
static inline Allocator ArenaAllocator_base_allocator(const ArenaAllocator* aa);

typedef struct ArenaAllocatorStats ArenaAllocatorStats;
struct ArenaAllocatorStats {
    size_t used, capacity, big_used, big_count, arena_count;
};



typedef struct Arena Arena;
typedef struct BigAllocation BigAllocation;

enum {ARENA_PAGE_SIZE = 4096, ARENA_SIZE = ARENA_PAGE_SIZE * 128};
// Allocations bigger than this will be put in the big allocation list instead.
enum {BIG_ALLOC_THRESH=ARENA_SIZE/2};

// The actual amount of data available is smaller due to the arena's header.
enum {ARENA_BUFFER_SIZE = ARENA_SIZE - sizeof (void*){0} - sizeof (size_t){0}};

struct Arena {
    Arena*_Nullable prev;
    size_t used;
    unsigned char buff[ARENA_BUFFER_SIZE];
};
_Static_assert(sizeof(Arena) == ARENA_SIZE, "");

struct BigAllocation {
    BigAllocation*_Nullable next;
    BigAllocation*_Nullable prev;
    size_t size;
    size_t alignment;
};

struct ArenaAllocator {
    #ifdef ARENA_EXPLICIT_ALLOCATOR
    _Alignas(8) Allocator base;
    #endif
    _Alignas(8) Arena*_Nullable arena;
    BigAllocation*_Nullable big_allocations;
};

force_inline
Allocator
allocator_from_arena(ArenaAllocator *aa){
    return Allocator_make(ALLOCATOR_ARENA, aa);
}

static inline
Allocator
ArenaAllocator_base_allocator(const ArenaAllocator *aa){
    #ifdef ARENA_EXPLICIT_ALLOCATOR
    return aa->base;
    #else
    (void)aa;
    return MALLOCATOR;
    #endif
}


static inline
_Bool
ArenaAllocator_is_big(size_t size, size_t alignment){
    return size > BIG_ALLOC_THRESH || alignment - 1 > ARENA_BUFFER_SIZE - size;
}

static inline
size_t
Big_alignment(size_t alignment){
    return alignment < _Alignof(BigAllocation) ? _Alignof(BigAllocation) : alignment;
}

static inline
size_t
Big_prefix(size_t alignment){
    alignment = Big_alignment(alignment);
    return (sizeof (BigAllocation){0} + alignment - 1) & ~(alignment - 1);
}

static
void*_Nullable
Big_alloc(ArenaAllocator *aa, size_t size, size_t alignment){
    size_t prefix = Big_prefix(alignment);
    size_t total;
    if(add_overflow(prefix, size, &total)) return NULL;
    BigAllocation* ba = Allocator_alloc(ArenaAllocator_base_allocator(aa), total, 1, Big_alignment(alignment));
    if(!ba) return NULL;
    *ba = (BigAllocation){.next = aa->big_allocations, .size = size, .alignment = alignment};
    if(ba->next) ba->next->prev = ba;
    aa->big_allocations = ba;
    return (char*)ba + prefix;
}

static
void
Big_free_header(ArenaAllocator *aa, const BigAllocation *ba){
    if(ba->prev)
        ba->prev->next = ba->next;
    else
        aa->big_allocations = ba->next;
    if(ba->next)
        ba->next->prev = ba->prev;
    Allocator_free(ArenaAllocator_base_allocator(aa), ba, Big_prefix(ba->alignment) + ba->size, 1, Big_alignment(ba->alignment));
}

static
void*_Nullable
Big_realloc(ArenaAllocator *aa, void *ptr, size_t size, size_t alignment){
    size_t prefix = Big_prefix(alignment);
    size_t total;
    if(add_overflow(prefix, size, &total)) return NULL;
    BigAllocation *ba = (BigAllocation*)((char*)ptr - prefix);
    BigAllocation *prev = ba->prev;
    BigAllocation *next = ba->next;
    ba = Allocator_realloc(ArenaAllocator_base_allocator(aa), ba, prefix + ba->size, total, 1, Big_alignment(alignment));
    if(!ba) return NULL;
    if(prev)
        prev->next = ba;
    else
        aa->big_allocations = ba;
    if(next) next->prev = ba;
    ba->size = size;
    return (char*)ba + prefix;
}

static
int
ArenaAllocator_alloc_arena(ArenaAllocator* aa){
    Arena* arena = Allocator_alloc(ArenaAllocator_base_allocator(aa), 1, sizeof *arena, _Alignof(Arena));
    if(!arena) return 1;
    arena->prev = aa->arena;
    arena->used = 0;
    aa->arena = arena;
    return 0;
}

static
void*_Nullable
ArenaAllocator_alloc(ArenaAllocator* aa, size_t count, size_t size, size_t alignment){
    if(!count || !size) return ALLOCATOR_SUCCESS_BUT_NO_DATA;
    if(mul_overflow(count, size, &size)) return NULL;
    if(ArenaAllocator_is_big(size, alignment))
        return Big_alloc(aa, size, alignment);
    if(!aa->arena && ArenaAllocator_alloc_arena(aa))
        return NULL;
    Arena* arena = aa->arena;
    size_t padding = -(uintptr_t)(arena->buff + arena->used) & (alignment - 1);
    size_t remaining = ARENA_BUFFER_SIZE - arena->used;
    if(padding > remaining || size > remaining - padding){
        if(ArenaAllocator_alloc_arena(aa)) return NULL;
        arena = aa->arena;
        padding = -(uintptr_t)arena->buff & (alignment - 1);
    }
    void* result = arena->buff + arena->used + padding;
    arena->used += padding + size;
    return result;
}

static
void*_Nullable
ArenaAllocator_zalloc(ArenaAllocator *aa, size_t count, size_t size, size_t alignment){
    void* result = ArenaAllocator_alloc(aa, count, size, alignment);
    if(!result || result == ALLOCATOR_SUCCESS_BUT_NO_DATA) return result;
    memset(result, 0, count * size);
    return result;
}

static
void
ArenaAllocator_free(ArenaAllocator *aa, const void *_Nullable ptr, size_t count, size_t size, size_t alignment){
    if(!ptr || ptr == ALLOCATOR_SUCCESS_BUT_NO_DATA) return;
    size *= count;
    if(ArenaAllocator_is_big(size, alignment)){
        const BigAllocation* ba = (const BigAllocation*)((const char*)ptr - Big_prefix(alignment));
        Big_free_header(aa, ba);
        return;
    }
    Arena* arena = aa->arena;
    assert(arena);
    if((const unsigned char*)ptr + size == arena->buff + arena->used)
        arena->used -= size;
}

static
void*_Nullable
ArenaAllocator_realloc(ArenaAllocator *aa, void *_Nullable ptr, size_t orig_count, size_t count, size_t size, size_t alignment){
    _Bool empty = !ptr || ptr == ALLOCATOR_SUCCESS_BUT_NO_DATA;
    if(!size) return empty ? ALLOCATOR_SUCCESS_BUT_NO_DATA : NULL;
    if(empty != (orig_count == 0)) return NULL;
    size_t old_size, new_size;
    if(mul_overflow(orig_count, size, &old_size)) return NULL;
    if(mul_overflow(count, size, &new_size)) return NULL;
    if(!orig_count)
        return ArenaAllocator_alloc(aa, count, size, alignment);
    if(!count){
        ArenaAllocator_free(aa, ptr, orig_count, size, alignment);
        return ALLOCATOR_SUCCESS_BUT_NO_DATA;
    }
    _Bool old_big = ArenaAllocator_is_big(old_size, alignment);
    _Bool new_big = ArenaAllocator_is_big(new_size, alignment);
    if(old_big && new_big) return Big_realloc(aa, (void*)ptr, new_size, alignment);
    if(!old_big && !new_big){
        if(old_size == new_size) return ptr;
        Arena* arena = aa->arena;
        assert(arena);
        if((unsigned char*)ptr + old_size == arena->buff + arena->used){
            size_t start = arena->used - old_size;
            if(new_size <= ARENA_BUFFER_SIZE - start){
                arena->used = start + new_size;
                return ptr;
            }
        }
    }
    void *result = ArenaAllocator_alloc(aa, count, size, alignment);
    if(!result) return NULL;
    memcpy(result, ptr, old_size < new_size ? old_size : new_size);
    ArenaAllocator_free(aa, ptr, orig_count, size, alignment);
    return result;
}

static
void
ArenaAllocator_free_all(ArenaAllocator*_Nullable aa){
    if(!aa) return;
    while(aa->arena){
        Arena* arena = aa->arena;
        aa->arena = arena->prev;
        Allocator_free(ArenaAllocator_base_allocator((ArenaAllocator*)aa), arena, 1, sizeof *arena, _Alignof(Arena));
    }
    while(aa->big_allocations) Big_free_header((ArenaAllocator*)aa, (BigAllocation*)aa->big_allocations);
}

static
void
ArenaAllocator_reset(ArenaAllocator* aa){
    while(aa->big_allocations) Big_free_header(aa, (BigAllocation*)aa->big_allocations);
    while(aa->arena){
        Arena* arena = aa->arena;
        if(!arena->prev){
            arena->used = 0;
            return;
        }
        aa->arena = arena->prev;
        Allocator_free(ArenaAllocator_base_allocator(aa), arena, 1, sizeof *arena, _Alignof(Arena));
    }
}

static inline
ArenaAllocatorStats
ArenaAllocator_stats(const ArenaAllocator* aa){
    ArenaAllocatorStats result = {0};
    for(Arena* arena = aa->arena; arena; arena = arena->prev){
        result.used += arena->used;
        result.capacity += sizeof(arena->buff);
        result.arena_count++;
    }
    for(BigAllocation* ba = aa->big_allocations; ba; ba = ba->next){
        result.big_used += ba->size;
        result.big_count++;
    }
    return result;
}

#ifdef __clang__
#pragma clang assume_nonnull end
#endif
#endif
