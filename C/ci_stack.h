//
// Copyright © 2026-2026, David Priver <david@davidpriver.com>
//
#ifndef CI_STACK_H
#define CI_STACK_H
#include <stddef.h>
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
ci_stack_alloc(CiStack* stack, size_t size){
    size_t rounded;
    if(add_overflow(size ? size : 1, (size_t)15, &rounded)) return NULL;
    rounded &= ~(size_t)15;
    CiStackSegment* segment = stack->top.segment;
    if(segment && rounded <= segment->capacity - stack->top.used){
        void* result = segment->data + stack->top.used;
        stack->top.used += rounded;
        return result;
    }
    // Segments after the current one are inactive and retained for reuse.
    // Never resize a segment: native code may hold pointers into live storage.
    segment = segment ? segment->next : stack->first;
    while(segment && segment->capacity < rounded) segment = segment->next;
    if(!segment){
        size_t capacity = rounded > CI_STACK_SEGMENT_SIZE ? rounded : CI_STACK_SEGMENT_SIZE;
        size_t allocation_size;
        if(add_overflow(sizeof *segment, capacity, &allocation_size)) return NULL;
        segment = Allocator_alloc(stack->allocator, allocation_size);
        if(!segment) return NULL;
        *segment = (CiStackSegment){.capacity = capacity};
        if(stack->last) stack->last->next = segment;
        else stack->first = segment;
        stack->last = segment;
    }
    stack->top = (CiStackMark){.segment = segment, .used = rounded};
    return segment->data;
}

static
void*_Nullable
ci_stack_zalloc(CiStack* stack, size_t size){
    void* result = ci_stack_alloc(stack, size);
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
