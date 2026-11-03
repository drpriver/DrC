//
// Copyright © 2026-2026, David Priver <david@davidpriver.com>
//
#ifndef CI_STACK_H
#define CI_STACK_H
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "nc_native_call.h"
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
typedef struct CiStackSave CiStackSave;
struct CiStackSave {
    CiStackMark position;
    size_t boundary;
};

typedef struct CiStack CiStack;
struct CiStack {
    NcCallState registers; // Caller-clobbered ABI registers shared by this execution stack.
    Allocator allocator;
    CiStackSegment*_Nullable first;
    CiStackSegment*_Nullable last;
    CiStackMark top;
    CiStackSave* _Nullable marks;
    size_t mark_count, mark_capacity;
    size_t boundary;
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
int
ci_stack_save(CiStack* stack, _Bool boundary){
    if(stack->mark_count == stack->mark_capacity){
        size_t capacity = stack->mark_capacity ? stack->mark_capacity * 2 : 16;
        if(capacity < stack->mark_capacity || capacity > SIZE_MAX / sizeof(CiStackSave)) return 1;
        CiStackSave* marks = Allocator_realloc(stack->allocator, stack->marks, stack->mark_capacity, capacity, sizeof *marks, _Alignof(CiStackSave));
        if(!marks) return 1;
        stack->marks = marks;
        stack->mark_capacity = capacity;
    }
    stack->marks[stack->mark_count++] = (CiStackSave){stack->top, stack->boundary};
    if(boundary) stack->boundary = stack->mark_count;
    return 0;
}

static inline
void
ci_stack_restore(CiStack* stack){
    CiStackSave mark = stack->marks[--stack->mark_count];
    stack->top = mark.position;
    stack->boundary = mark.boundary;
}

static inline
void
ci_stack_unwind_frame(CiStack* stack){
    stack->mark_count = stack->boundary;
    ci_stack_restore(stack);
}

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
    segment = Allocator_alloc(stack->allocator, allocation_size, 1, _Alignof(CiStackSegment));
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
    if(stack->marks) Allocator_free(stack->allocator, stack->marks, stack->mark_capacity, sizeof(CiStackSave), _Alignof(CiStackSave));
    stack->marks = NULL;
    stack->mark_count = stack->mark_capacity = stack->boundary = 0;
    CiStackSegment* segment = stack->first;
    while(segment){
        CiStackSegment* next = segment->next;
        Allocator_free(stack->allocator, segment, sizeof *segment + segment->capacity, 1, _Alignof(CiStackSegment));
        segment = next;
    }
    stack->first = stack->last = NULL;
    stack->top = (CiStackMark){0};
}

#ifdef __clang__
#pragma clang assume_nonnull end
#endif
#endif
