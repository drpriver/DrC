//
// Copyright © 2021-2025, David Priver <david@davidpriver.com>
//
#ifndef MSTRING_BUILDER16_H
#define MSTRING_BUILDER16_H
#include <stddef.h>
#include <string.h>
#include <assert.h>
#include <stdint.h>
#include "long_string.h"
#include "stringview.h"
#include "Allocators/allocator.h"

#if 0
#include "debugging.h"
#endif

#ifdef __clang__
#pragma clang assume_nonnull begin
#else
#ifndef _Null_unspecified
#define _Null_unspecified
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

#if !defined(likely) && !defined(unlikely)
#if defined(__GNUC__) || defined(__clang__)
#define likely(x)      __builtin_expect(!!(x), 1)
#define unlikely(x)    __builtin_expect(!!(x), 0)
#else
#define likely(x) (x)
#define unlikely(x) (x)
#endif
#endif

//
// A type for building up a string without having to deal with things like sprintf
// or strcat.
typedef struct MStringBuilder16 MStringBuilder16;
struct MStringBuilder16 {
    size_t cursor;
    size_t capacity;
    uint16_t*_Null_unspecified data;
    Allocator allocator;
    int errored;
};

//
// Dealloc the data and zeros out the builder.
// Unneeded if you called `msb16_detach_ls` or `msb16_detach_sv`.
static inline
void
msb16_destroy(MStringBuilder16* msb16){
    Allocator_free(msb16->allocator, msb16->data, msb16->capacity);
    msb16->data=0;
    msb16->cursor=0;
    msb16->capacity=0;
    msb16->errored=0;
}

force_inline
warn_unused
int
_check_msb16_remaining_size(MStringBuilder16*, size_t);

static inline
warn_unused
int
_msb16_resize(MStringBuilder16*, size_t);


//
// Nul-terminates the builder without actually increasing the length
// of the string.
static inline
void
msb16_nul_terminate(MStringBuilder16* msb16){
    int err = _check_msb16_remaining_size(msb16, 1);
    if(unlikely(err)) return;
    msb16->data[msb16->cursor] = u'\0';
}


//
// Ensures additional capacity is present in the builder.
// Avoids re-allocs and thus potential copies
static inline
warn_unused
int
msb16_ensure_additional(MStringBuilder16* msb16, size_t additional_capacity){
    return _check_msb16_remaining_size(msb16, additional_capacity);
}

//
// Moves the ownership of the sring from the builder to the caller.
// Ensures nul-termination.
// Builder can be reused afterwards; its fields are zeroed.
static inline
LongStringUtf16
msb16_detach_ls(MStringBuilder16* msb16){
#ifdef DEBUGGING_H
    if(msb16->errored) bt();
    if(!msb16->data) bt();
#endif
    assert(msb16->data);
    assert(!msb16->errored);
    msb16_nul_terminate(msb16);
    int err = _msb16_resize(msb16, msb16->cursor+1);
    (void)err;
    assert(!err);
    LongStringUtf16 result = {0};
    result.text = msb16->data;
    result.length = msb16->cursor;
    msb16->data = NULL;
    msb16->capacity = 0;
    msb16->cursor = 0;
    return result;
}

static inline
StringViewUtf16
msb16_detach_sv(MStringBuilder16* msb16){
#ifdef DEBUGGING_H
    if(msb16->errored) bt();
#endif
    assert(!msb16->errored);
    StringViewUtf16 result = {0};
    int err = _msb16_resize(msb16, msb16->cursor);
    (void)err;
    assert(!err);
    result.text = msb16->data;
    result.length = msb16->cursor;
    msb16->data = NULL;
    msb16->capacity = 0;
    msb16->cursor = 0;
    return result;
}

//
// "Borrows" the current contents of the builder and returns a nul-terminated
// string view to those contents.  Keep uses of the borrowed string tightly
// scoped as any further use of the builder can cause a reallocation.  It's
// also confusing to have the contents of the string view change under you.
static inline
StringViewUtf16
msb16_borrow_sv(MStringBuilder16* msb16){
#ifdef DEBUGGING_H
    if(msb16->errored) bt();
#endif
    assert(!msb16->errored);
    return (StringViewUtf16) {
        .text = msb16->data,
        .length = msb16->cursor,
    };
}

// "Borrows" a nul-terminated string
static inline
LongStringUtf16
msb16_borrow_ls(MStringBuilder16* msb16){
    msb16_nul_terminate(msb16);
#ifdef DEBUGGING_H
    if(msb16->errored) bt();
#endif
    assert(!msb16->errored);
    assert(msb16->data);
    return (LongStringUtf16) {
        .text = msb16->data,
        .length = msb16->cursor,
    };
}

//
// "Resets" the builder. Logically clears the contents of the builder
// (although it avoids actually touching the data) and sets the length to 0.
// Does not dealloc the data, so you can build up a string, borrow it,
// reset and do that again. This is particularly useful for creating strings
// that are then consumed by normal c-apis that take a c str as they almost
// always will copy the string themselves.
static inline
void
msb16_reset(MStringBuilder16* msb16){
    msb16->cursor = 0;
}

// Internal function, resizes the builder to the new size.
static inline
warn_unused
int
_msb16_resize(MStringBuilder16* msb16, size_t size){
    if(msb16->errored) return 1;
    uint16_t* new_data = Allocator_realloc(msb16->allocator, msb16->data, msb16->capacity, size*2);
    if(!new_data){
        msb16->errored = 1;
        return 1;
    }
    msb16->data = new_data;
    msb16->capacity = size;
    return 0;
}

// Internal function, ensures there is enough additional capacity.
force_inline
warn_unused
int
_check_msb16_remaining_size(MStringBuilder16* msb16, size_t len){
    if(msb16->cursor + len > msb16->capacity){
        size_t new_size = msb16->capacity?(msb16->capacity*3)/2:16;
        while(new_size < msb16->cursor+len){
            new_size *= 2;
        }
        if(new_size & 15){
            new_size += (16- (new_size&15));
        }
        return _msb16_resize(msb16, new_size);
    }
    return 0;
}

//
// Writes a string into the builder. You must know the length.
static inline
void
msb16_write_str(MStringBuilder16* restrict msb16, const uint16_t*_Null_unspecified restrict str, size_t len){
    if(!len)
        return;
    int err = _check_msb16_remaining_size(msb16, len);
    if(unlikely(err))
        return;
    (memcpy)(msb16->data + msb16->cursor, str, len*2);
    msb16->cursor += len;
}

//
// Write a single uint16_t into the builder.
// This is actually kind of slow, relatively speaking, as it checks
// the size every time.
// It often will be better to write an extension method that reserves enough
// space and then writes to the data buffer directly.
force_inline
void
msb16_write_uint16_t(MStringBuilder16* msb16, uint16_t c){
    int err = _check_msb16_remaining_size(msb16, 1);
    if(unlikely(err))
        return;
    msb16->data[msb16->cursor++] = c;
}

//
// Write a repeated pattern of uint16_tacters into the builder.
// Pay attention to the order of arguments, as C will allow implicit
// conversions!
force_inline
void
msb16_write_nuint16_t(MStringBuilder16* msb16, uint16_t c, size_t n){
    if(n == 0)
        return;
    int err = _check_msb16_remaining_size(msb16, n);
    if(unlikely(err))
        return;
    memset(msb16->data + msb16->cursor, c, n);
    msb16->cursor += n;
}

//
// Erases the given number of uint16_tacters from the end of the builder.
static inline
void
msb16_erase(MStringBuilder16* msb16, size_t len){
    if(!msb16->cursor) return;
    if(len > msb16->cursor){
        msb16->cursor = 0;
        msb16->data[0] = '\0';
        return;
    }
    msb16->cursor -= len;
    msb16->data[msb16->cursor] = '\0';
}

static inline
uint16_t
msb16_peek(MStringBuilder16* msb16){
    if(!msb16->cursor) return 0;
    return msb16->data[msb16->cursor-1];
}

static
void
msb16_write_utf8(MStringBuilder16* msb16, const char* str, size_t len){
    int err = _check_msb16_remaining_size(msb16, len*2);
    if(err) return;
    const uint8_t* utf8 = (const uint8_t*)str;
    uint16_t* utf16 = msb16->data + msb16->cursor;
    for(size_t i = 0; i < len; ){
        uint32_t codepoint = 0;
        size_t num_bytes = 0;
        uint32_t b = utf8[i];
        if((b & 0x80u) == 0){
            codepoint = b;
            num_bytes = 1;
        }
        else if((b & 0xe0u) == 0xc0u){
            codepoint = b & 0x1fu;
            num_bytes = 2;
        }
        else if((b & 0xf0u) == 0xe0u){
            codepoint = b & 0x0fu;
            num_bytes = 3;
        }
        else if((b & 0xf8u) == 0xf0u){
            codepoint = b & 0x07u;
            num_bytes = 4;
        }
        else
            // invalid start b
            goto fail;
        if(i + num_bytes > len)
            goto fail;
        for(size_t j = 1; j < num_bytes; j++){
            b = utf8[i + j];
            if((b & 0xc0u) != 0x80u){
                msb16->errored = 1;
                return;
            }
            codepoint <<= 6;
            codepoint |= (b & 0x3fu);
        }
        switch(num_bytes){
            case 2:
                if(codepoint < 0x80u) goto fail;
                break;
            case 3:
                if(codepoint < 0x800u) goto fail;
                break;
            case 4:
                if(codepoint < 0x1000u) goto fail;
                break;
            default: break;
        }
        if(codepoint >= 0xd800u && codepoint <= 0xdfffu) goto fail;
        if(codepoint >  0x10ffffu) goto fail;
        if(codepoint <= 0xffffu){
            utf16++[0] = (uint16_t)codepoint;
        }
        else {
            codepoint -= 0x10000u;
            uint32_t hi = 0xd800u + (codepoint >> 10);
            uint32_t lo = 0xdc00u + (codepoint & 0x3ffu);
            utf16++[0] = (uint16_t)hi;
            utf16++[0] = (uint16_t)lo;
        }
        i += num_bytes;
    }
    msb16->cursor = utf16 - msb16->data;
    return;

    fail:
    msb16->errored = 1;
    return;

}

#ifdef __clang__
#pragma clang assume_nonnull end
#endif

#endif
