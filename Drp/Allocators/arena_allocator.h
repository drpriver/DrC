//
// Copyright © 2021-2023, David Priver <david@davidpriver.com>
//
#ifndef ARENA_ALLOCATOR_H
#define ARENA_ALLOCATOR_H
// size_t
#include <stddef.h>
// memcpy, memset
#include <string.h>
#include <assert.h>
#include <stdint.h>
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
#if defined(__GNUC__) || defined(__clang__)
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


force_inline
Allocator
allocator_from_arena(ArenaAllocator* aa){
    return (Allocator){.type=ALLOCATOR_ARENA, ._data=aa};
}

//
// TODO: Currently we back the arena allocator with malloc, but we could just
// call the OS apis (mmap, VirtualAlloc, etc.) ourselves. Would need to see how
// slow they are compared to malloc, but we are allocating relatively large
// amounts anyway...
// It kind of doesn't matter though? The point of allocating a large block and
// slicing it up is to avoid calling slow alloc functions.

//
// Header for a large allocation. Used to maintain a linked list of allocations.

typedef struct BigListNode BigListNode;
struct BigListNode {
    BigListNode*_Nullable next;
    BigListNode*_Nullable prev;
};
typedef struct BigAllocation BigAllocation;
struct BigAllocation {
    union {
        struct {
            BigListNode*_Nullable next;
            BigListNode*_Nullable prev;
        };
        BigListNode node;
    };
    size_t size;
    // Padding to align to a cache line.
    // These are the header of a pretty big allocation anyway, so
    // it's not actually that much waste.
    uintptr_t pad[5];
};

static inline
void
Big_init(BigListNode* prev, BigAllocation* ba){
    ba->next = prev->next;
    if(ba->next) ba->next->prev = &ba->node;
    ba->prev = prev;
    prev->next = &ba->node;
}

static inline
void*_Nullable
Big_alloc(ArenaAllocator* aa, BigListNode* prev, size_t size){
    BigAllocation* ba = Allocator_alloc(ArenaAllocator_base_allocator(aa), size + sizeof(*ba));
    if(!ba) return NULL;
    Big_init(prev, ba);
    ba->size = size;
    return ba+1;
}

static inline
void*_Nullable
Big_zalloc(ArenaAllocator* aa, BigListNode* prev, size_t size){
    BigAllocation* ba = Allocator_zalloc(ArenaAllocator_base_allocator(aa), size + sizeof(*ba));
    if(!ba) return NULL;
    Big_init(prev, ba);
    ba->size = size;
    return ba+1;
}

static inline
void*_Nullable
Big_realloc(ArenaAllocator*aa, void* a, size_t old, size_t size){
    BigAllocation* ba = (BigAllocation*)a - 1;
    BigListNode* prev = ba->prev;
    BigListNode* next = ba->next;
    ba = Allocator_realloc(ArenaAllocator_base_allocator(aa), ba, old+sizeof(*ba), size+sizeof(*ba));
    if(!ba) return NULL;
    if(prev) prev->next = &ba->node;
    if(next) next->prev = &ba->node;
    ba->size = size;
    return ba+1;
}

static inline
void
Big_free(ArenaAllocator* aa, const void*_Nullable a, size_t size){
    if(!a) return;
    const BigAllocation* ba = (const BigAllocation*)a-1;
    BigListNode* prev = ba->prev;
    BigListNode* next = ba->next;
    Allocator_free(ArenaAllocator_base_allocator(aa), ba, size+sizeof(*ba));
    if(prev) prev->next = next;
    if(next) next->prev = prev;
}

typedef struct Arena Arena;

struct ArenaAllocator {
    #ifdef ARENA_EXPLICIT_ALLOCATOR
    Allocator base;
    #endif
    Arena*_Nullable arena;
    BigListNode big_allocations;
};



enum {ARENA_PAGE_SIZE=4096};

// Arenas are in 64 page chunks. This might be excessive, idk.
enum {
// #ifdef __wasm__
    // ARENA_SIZE=ARENA_PAGE_SIZE*8
// #else
    ARENA_SIZE=ARENA_PAGE_SIZE*128
// #endif
};

// Allocations bigger than this will be put in the big allocation list instead.
enum {BIG_ALLOC_THRESH=ARENA_SIZE/2};

// The actual amount of data available is smaller due to the arena's header.
enum {ARENA_BUFFER_SIZE = ARENA_SIZE-sizeof(void*)-sizeof(size_t)};

//
// An arena that is linearly allocated from. Supports fast realloc/free if
// operating on the last allocation.
//
struct Arena {
    struct Arena*_Nullable prev; // The previous, exhausted arena.
    size_t used; // How much of this arena has been used.
    char buff[ARENA_BUFFER_SIZE];
};

_Static_assert(sizeof(Arena) == ARENA_SIZE, "");


//
// Rounds up to the nearest power of 8.
//
force_inline
size_t
ArenaAllocator_round_size_up(size_t size){
    size_t remainder = size & 7;
    if(remainder)
        size += 8 - remainder;
    return size;
}

static inline
warn_unused
int
ArenaAllocator_alloc_arena(ArenaAllocator* aa){
    Arena* arena = Allocator_alloc(ArenaAllocator_base_allocator(aa), sizeof(*arena));
    if(!arena) return 1;
    arena->prev = aa->arena;
    arena->used = 0;
    aa->arena = arena;
    return 0;
}

static
void
ArenaAllocator_free(ArenaAllocator*aa, const void*_Nullable ptr, size_t size);

//
// Allocates an uninitialized chunk of memory from the arena.
//
static
void*_Nullable
ArenaAllocator_alloc(ArenaAllocator* aa, size_t size){
    size = ArenaAllocator_round_size_up(size);
    if(size > BIG_ALLOC_THRESH)
        return Big_alloc(aa, &aa->big_allocations, size);
    if(!aa->arena){
        if(ArenaAllocator_alloc_arena(aa) != 0){
            return NULL;
        }
    }
    else {
        size_t remainder = ARENA_BUFFER_SIZE - aa->arena->used;
        if(size > remainder){
            if(ArenaAllocator_alloc_arena(aa) != 0){
                return NULL;
            }
        }
    }
    void* result = aa->arena->buff + aa->arena->used;
    aa->arena->used += size;
    return result;
}
//
// Allocates a zeroed chunk of memory from the arena.
//
static
void*_Nullable
ArenaAllocator_zalloc(ArenaAllocator* aa, size_t size){
    size = ArenaAllocator_round_size_up(size);
    if(size > BIG_ALLOC_THRESH)
        return Big_zalloc(aa, &aa->big_allocations, size);
    if(!aa->arena){
        if(ArenaAllocator_alloc_arena(aa) != 0){
            return NULL;
        }
    }
    else {
        size_t remainder = ARENA_BUFFER_SIZE - aa->arena->used;
        if(size > remainder){
            if(ArenaAllocator_alloc_arena(aa) != 0){
                return NULL;
            }
        }
    }
    void* result = aa->arena->buff + aa->arena->used;
    aa->arena->used += size;
    memset(result, 0, size);
    return result;
}

//
// Reallocs an allocation from the arena, attempting to do it in place if it
// was the last allocation. If not, is forced to just alloc + memcpy.
//
static
void*_Nullable
ArenaAllocator_realloc(ArenaAllocator* aa, void*_Nullable ptr, size_t old_size, size_t new_size){
    if(!old_size && !new_size)
        return ptr;
    if(!old_size){
        assert(!ptr);
        assert(new_size);
        return ArenaAllocator_alloc(aa, new_size);
    }
    if(!new_size){
        ArenaAllocator_free(aa, ptr, old_size);
        return NULL;
    }
    old_size = ArenaAllocator_round_size_up(old_size);
    new_size = ArenaAllocator_round_size_up(new_size);
    if(old_size == new_size) return ptr;
    if(old_size > BIG_ALLOC_THRESH){
        if(new_size > BIG_ALLOC_THRESH)
            return Big_realloc(aa, (void*)ptr, old_size, new_size);
        // reallocing from a big allocation to an arena allocation.
        void* result = ArenaAllocator_alloc(aa, new_size);
        if(!result) return NULL;
        assert(old_size > new_size);
        memcpy(result, ptr, new_size);
        Big_free(aa, ptr, old_size);
        return result;
    }
    if(new_size > BIG_ALLOC_THRESH){
        assert(old_size <= BIG_ALLOC_THRESH);
        // reallocing from arena allocation to big allocation
        void* result = Big_alloc(aa, &aa->big_allocations, new_size);
        if(!result) return NULL;
        if(old_size){
            memcpy(result, ptr, old_size);
            ArenaAllocator_free(aa, ptr, old_size);
        }
        return result;
    }
    {
        Arena* arena = aa->arena;
        assert(arena); // must be an arena at this point if old_size was non-zero
        if((char*)ptr+old_size == arena->buff+arena->used){
            size_t new_used = arena->used - old_size + new_size;
            if(new_used <= ARENA_BUFFER_SIZE){
                // fits, in-place realloc
                arena->used = new_used;
                return ptr;
            }
        }
    }
    assert(aa->arena);
    if(ARENA_BUFFER_SIZE - aa->arena->used < new_size){
        if(ArenaAllocator_alloc_arena(aa) != 0)
            return NULL;
    }
    void* result = aa->arena->buff + aa->arena->used;
    aa->arena->used += new_size;
    if(old_size < new_size)
        memcpy(result, ptr, old_size);
    else
        memcpy(result, ptr, new_size);
    // no point in freeing ptr as it was an arena allocation we couldn't realloc into.
    // We could do it with the old arena, but we never go back once we've alloced a new
    // arena. Maybe we should? idk.
    return result;
}
//
// Free all allocations from the arenas. Deallocs the arenas themselves and
// frees the big allocation linked list as well.
//
static
void
ArenaAllocator_free_all(ArenaAllocator*_Nullable aa){
    if(!aa) return;
    Arena* arena = aa->arena;
    while(arena){
        Arena* to_free = arena;
        arena = arena->prev;
        Allocator_free(ArenaAllocator_base_allocator((ArenaAllocator*)aa), to_free, sizeof(*to_free));
    }
    BigAllocation* ba = (BigAllocation*)aa->big_allocations.next;
    assert(aa->big_allocations.prev == NULL);
    while(ba){
        BigAllocation* to_free = ba;
        ba = (BigAllocation*)ba->next;
        Allocator_free(ArenaAllocator_base_allocator((ArenaAllocator*)aa), to_free, sizeof(*to_free)+to_free->size);
    }
    aa->arena = NULL;
    aa->big_allocations.next = NULL;
    aa->big_allocations.prev = NULL;
    return;
}

static
void
ArenaAllocator_reset(ArenaAllocator* aa){
    BigAllocation* ba = (BigAllocation*)aa->big_allocations.next;
    assert(aa->big_allocations.prev == NULL);
    while(ba){
        BigAllocation* to_free = ba;
        ba = (BigAllocation*)ba->next;
        Allocator_free(ArenaAllocator_base_allocator(aa), to_free, sizeof(*to_free)+to_free->size);
    }
    aa->big_allocations.next = NULL;
    aa->big_allocations.prev = NULL;
    Arena* arena = aa->arena;
    while(arena){
        if(!arena->prev) break;
        Arena* to_free = arena;
        arena = arena->prev;
        Allocator_free(ArenaAllocator_base_allocator(aa), to_free, sizeof *to_free);
    }
    if(arena) arena->used = 0;
    aa->arena = arena;
}

static
void
ArenaAllocator_free(ArenaAllocator*aa, const void*_Nullable ptr, size_t size){
    if(!ptr) return;
    if(!size) return;
    size = ArenaAllocator_round_size_up(size);
    if(size > BIG_ALLOC_THRESH){
        Big_free(aa, ptr, size);
        return;
    }
    Arena* arena = aa->arena;
    assert(arena);
    if((const char*)ptr+size == arena->buff+arena->used)
        arena->used -= size;
}

typedef struct ArenaAllocatorStats ArenaAllocatorStats;
struct ArenaAllocatorStats {
    size_t used, capacity, big_used, big_count, arena_count;
};

static inline
ArenaAllocatorStats
ArenaAllocator_stats(const ArenaAllocator* aa){
    ArenaAllocatorStats result = {0};
    for(Arena* arena = aa->arena; arena; arena = arena->prev){
        result.used += arena->used;
        result.capacity += sizeof(arena->buff);
        result.arena_count++;
    }
    for(BigListNode* ba = aa->big_allocations.next; ba; ba = ba->next){
        result.big_used += ((BigAllocation*)ba)->size;
        result.big_count++;
    }
    return result;
}

static inline
Allocator
ArenaAllocator_base_allocator(const ArenaAllocator* aa){
    #ifdef ARENA_EXPLICIT_ALLOCATOR
    return aa->base;
    #else
    (void)aa;
    return MALLOCATOR;
    #endif
}

#ifdef __clang__
#pragma clang assume_nonnull end
#endif

#endif
