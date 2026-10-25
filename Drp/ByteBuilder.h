#ifndef DRP_BYTE_BUILDER_H
#define DRP_BYTE_BUILDER_H
//
// Copyright © 2026-2026, David Priver <david@davidpriver.com>
//

#include <stddef.h>
#include <string.h>
#include <stdint.h>
#include "Allocators/allocator.h"
#include "ckdint.h"
#include "ByteBuffer.h"

#ifdef __clang__
#pragma clang assume_nonnull begin
#else
#ifndef _Null_unspecified
#define _Null_unspecified
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

#ifndef unreachable
#if defined __GNUC__ || defined __clang__ || defined __DRC__
#define unreachable() __builtin_unreachable()
#else
#define unreachable() __assume(0)
#endif
#endif

#ifndef warn_unused
#if defined __GNUC__ || defined __clang__
#define warn_unused __attribute__((warn_unused_result))
#elif defined _MSC_VER
#define warn_unused
#else
#define warn_unused
#endif
#endif

typedef struct ByteBuilder ByteBuilder;
struct ByteBuilder {
    size_t cursor,
           capacity;
    void *_Null_unspecified data;
    Allocator allocator;
    uint32_t alignment;
    int errored;
};

static inline
void
bb_destroy(ByteBuilder *bb){
    Allocator_free(bb->allocator, bb->data, bb->capacity, 1, bb->alignment? bb->alignment : MALLOC_ALIGNMENT);
    bb->data      = 0;
    bb->cursor    = 0;
    bb->capacity  = 0;
    bb->errored   = 0;
    bb->alignment = 0;
}

// read-only view
force_inline
ByteView
bb_borrow_view(const ByteBuilder *bb){
    return (ByteView){bb->cursor, bb->data};
}

static inline void bb_shrink_to_size(ByteBuilder* bb);

force_inline
ByteBuffer
bb_detach(ByteBuilder *bb){
    bb_shrink_to_size(bb);
    ByteBuffer result = {bb->cursor, bb->data};
    bb->data = NULL;
    bb->cursor = 0;
    bb->capacity = 0;
    bb->errored = 0;
    bb->alignment = 0;
    return result;
}

// writable buffer
force_inline
ByteBuffer
bb_borrow_buffer(const ByteBuilder *bb){
    return (ByteBuffer){bb->capacity, bb->data};
}

force_inline
ByteBuffer
bb_borrow_remainder(const ByteBuilder *bb){
    return (ByteBuffer){bb->capacity - bb->cursor, bb->cursor? (char*)bb->data + bb->cursor : NULL};
}

static inline
int
_bb_resize(ByteBuilder *bb, size_t size){
    if(bb->errored) return 1;
    size_t alignment = bb->alignment? bb->alignment : MALLOC_ALIGNMENT;
    void *data = Allocator_realloc(bb->allocator, bb->data, bb->capacity, size, 1, alignment);
    if(!data){
        bb->errored = 1;
        return 1;
    }
    bb->data = data;
    bb->capacity = size;
    return 0;
}

force_inline
warn_unused
int
bb_ensure_additional(ByteBuilder *bb, size_t len){
    if(bb->errored) return 1;
    size_t required;
    if(add_overflow(bb->cursor, len, &required))
        goto overflow;
    if(required > bb->capacity){
        size_t new_size = 16;
        if(bb->capacity){
            if(add_overflow(bb->capacity, bb->capacity / 2, &new_size))
                goto overflow;
        }
        while(new_size < required){
            if(mul_overflow(new_size, (size_t)2, &new_size))
                goto overflow;
        }
        if(add_overflow(new_size, (size_t)15, &new_size))
            goto overflow;
        new_size &= ~(size_t)15;
        return _bb_resize(bb, new_size);
    }
    return 0;
    overflow:
    bb->errored = 1;
    return 1;
}

static inline
void
bb_shrink_to_size(ByteBuilder *bb){
    if(bb->errored) return;
    if(bb->cursor == bb->capacity) return;
    _bb_resize(bb, bb->cursor);
}


static inline
void
bb_write(ByteBuilder *bb, const void * restrict data, size_t length){
    int err = bb_ensure_additional(bb, length);
    if(err) return;
    memcpy((char*)bb->data+bb->cursor, data, length);
    bb->cursor += length;
}


static inline
void
bb_write_unchecked(ByteBuilder *bb, const void * restrict data, size_t length){
    memcpy((char*)bb->data+bb->cursor, data, length);
    bb->cursor += length;
}

force_inline
void
bb_put_uint8_assume(ByteBuilder *bb, uint8_t val){
    *(uint8_t*)((char*)bb->data+bb->cursor) = val;
    bb->cursor += sizeof val;
}
force_inline
void
bb_put_uint16_assume(ByteBuilder *bb, uint16_t val){
    *(uint16_t*)((char*)bb->data+bb->cursor) = val;
    bb->cursor += sizeof val;
}
force_inline
void
bb_put_uint32_assume(ByteBuilder *bb, uint32_t val){
    *(uint32_t*)((char*)bb->data+bb->cursor) = val;
    bb->cursor += sizeof val;
}
force_inline
void
bb_put_uint64_assume(ByteBuilder *bb, uint64_t val){
    *(uint64_t*)((char*)bb->data+bb->cursor) = val;
    bb->cursor += sizeof val;
}

static inline
void
bb_put_uint8(ByteBuilder *bb, uint8_t val){
    int err = bb_ensure_additional(bb, sizeof val);
    if(err) return;
    memcpy((char*)bb->data+bb->cursor, &val, sizeof val);
    bb->cursor += sizeof val;
}

static inline
void
bb_put_uint16(ByteBuilder *bb, uint16_t val){
    int err = bb_ensure_additional(bb, sizeof val);
    if(err) return;
    memcpy((char*)bb->data+bb->cursor, &val, sizeof val);
    bb->cursor += sizeof val;
}

static inline
void
bb_put_uint32(ByteBuilder *bb, uint32_t val){
    int err = bb_ensure_additional(bb, sizeof val);
    if(err) return;
    memcpy((char*)bb->data+bb->cursor, &val, sizeof val);
    bb->cursor += sizeof val;
}

static inline
void
bb_put_uint64(ByteBuilder *bb, uint64_t val){
    int err = bb_ensure_additional(bb, sizeof val);
    if(err) return;
    memcpy((char*)bb->data+bb->cursor, &val, sizeof val);
    bb->cursor += sizeof val;
}

static inline
void
bb_put_uint128(ByteBuilder *bb, const void* restrict val){
    int err = bb_ensure_additional(bb, 16);
    if(err) return;
    memcpy((char*)bb->data+bb->cursor, val, 16);
    bb->cursor += 16;
}

static inline
void
bb_write_ptr(ByteBuilder *bb, const void * val){
    int err = bb_ensure_additional(bb, sizeof val);
    if(err) return;
    memcpy((char*)bb->data+bb->cursor, (const void*)&val, sizeof val);
    bb->cursor += sizeof val;
}

#ifdef __clang__
#pragma clang assume_nonnull end
#endif
#endif
