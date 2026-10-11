//
// Copyright © 2026-2026, David Priver <david@davidpriver.com>
//
#ifndef CI_STACK_H
#define CI_STACK_H
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "../Drp/Allocators/allocator.h"
#include "../Drp/ckdint.h"

#ifdef __clang__
#pragma clang assume_nonnull begin
#endif

typedef struct CiStackSegment CiStackSegment;
struct CiStackSegment {
    CiStackSegment*_Nullable next;
    size_t capacity;
    _Alignas(16) unsigned char data[];
};

typedef struct CiStackMark CiStackMark;
struct CiStackMark {
    CiStackSegment*_Nullable segment;
    size_t used;
};

typedef struct CiStack CiStack;
struct CiStack {
    Allocator allocator;
    CiStackSegment*_Nullable first;
    CiStackSegment*_Nullable last;
    CiStackMark top;
};

enum {CI_STACK_SEGMENT_SIZE = 64 * 1024};
_Static_assert(offsetof(CiStackSegment, data) % 16 == 0, "stack storage must be 16-aligned");

static inline
CiStackMark
ci_stack_mark(const CiStack* stack){ return stack->top; }

static inline
void
ci_stack_rewind(CiStack* stack, CiStackMark mark){ stack->top = mark; }

static
void*_Nullable
ci_stack_alloc(CiStack* stack, size_t size, size_t alignment){
    if(!alignment || (alignment & (alignment - 1))) return NULL;
    CiStackSegment* segment = stack->top.segment;
    size_t used = segment ? stack->top.used : 0;
    if(!segment) segment = stack->first;
    if(!size && segment) return segment->data + used;
    for(; segment; segment = segment->next, used = 0){
        size_t padding = -(uintptr_t)(segment->data + used) & (alignment - 1);
        size_t available = segment->capacity - used;
        if(padding <= available && size <= available - padding){
            size_t offset = used + padding;
            stack->top = (CiStackMark){.segment = segment, .used = offset + size};
            return segment->data + offset;
        }
    }
    size_t required = size;
    if(size && add_overflow(size, alignment - 1, &required)) return NULL;
    size_t capacity = required > CI_STACK_SEGMENT_SIZE ? required : CI_STACK_SEGMENT_SIZE;
    size_t allocation_size;
    if(add_overflow(sizeof *segment, capacity, &allocation_size)) return NULL;
    segment = Allocator_alloc(stack->allocator, allocation_size);
    if(!segment) return NULL;
    *segment = (CiStackSegment){.capacity = capacity};
    if(stack->last) stack->last->next = segment;
    else stack->first = segment;
    stack->last = segment;
    size_t padding = size ? -(uintptr_t)segment->data & (alignment - 1) : 0;
    stack->top = (CiStackMark){.segment = segment, .used = padding + size};
    return segment->data + padding;
}

static
void*_Nullable
ci_stack_zalloc(CiStack* stack, size_t size, size_t alignment){
    void* result = ci_stack_alloc(stack, size, alignment);
    if(result) memset(result, 0, size);
    return result;
}

static
void
ci_stack_destroy(CiStack* stack){
    CiStackSegment* segment = stack->first;
    while(segment){
        CiStackSegment* next = segment->next;
        Allocator_free(stack->allocator, segment, sizeof *segment + segment->capacity);
        segment = next;
    }
    stack->first = stack->last = NULL;
    stack->top = (CiStackMark){0};
}

#ifdef __clang__
#pragma clang assume_nonnull end
#endif
#endif
