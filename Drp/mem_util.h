//
// Copyright © 2021-2025, David Priver <david@davidpriver.com>
//
#ifndef MEM_UTIL_H
#define MEM_UTIL_H

#include <string.h>
#include <assert.h>

#ifndef warn_unused
#if defined(__GNUC__) || defined(__clang__)
#define warn_unused __attribute__((warn_unused_result))
#elif defined(_MSC_VER)
#define warn_unused
#else
#define warn_unused
#endif
#endif

#ifdef __clang__
#pragma clang assume_nonnull begin
#endif

// mem_util.h
// ----------
// Provides additional functions in the spirit of memcpy/memmove, that can
// simplify or avoid pointer arithmetic when manipulating buffers.

//
// Inserts a buffer into an array at the given position.
//
// Arguments:
// ----------
// whence:
//   The offset into the dst buffer to insert at (in bytes).
//
// dst:
//   The destination buffer to insert into.
//
// capacity:
//   The length of the destination buffer (in bytes).
//
// used:
//   How much of the destination buffer has actually been used / should be
//   preserved (in bytes).
//
// src:
//   The buffer to copy from.
//
// length:
//   The length of the src buffer (in bytes).
//
// Returns:
// --------
// 0 on success, 1 on failure.
//
static inline
warn_unused
int
meminsert(
    size_t whence,
    void* restrict dst, size_t capacity, size_t used,
    const void* restrict src, size_t length
){
    if(capacity - used < length) return 1;
    if(whence > used) return 1;
    if(used == whence){
        memmove(((char*)dst)+whence, src, length);
        return 0;
    }
    size_t tail = used - whence;
    memmove(((char*)dst)+whence+length, ((char*)dst)+whence, tail);
    memmove(((char*)dst)+whence, src, length);
    return 0;
}

static inline
warn_unused
int
memappend(
    void* restrict dst, size_t capacity, size_t used,
    const void* restrict src, size_t length
){
    if(capacity - used < length) return 1;
    memcpy(((char*)dst)+used, src, length);
    return 0;
}


//
// Logically "removes" a section of an array by shifting the stuff after
// it forward.
//
// This is less error prone than doing the pointer arithmetic at each call
// site. You can pass a buffer + length and where and how much to remove.
//
// Arguments:
// ----------
// whence:
//   The offset to the beginning of the section to remove, in bytes.
//
// buff:
//   The buffer to remove the section from.
//
// bufflen:
//   The length of the buffer, in bytes.
//
// nremove:
//   Length of the section to remove, in bytes.
//
static inline
void
memremove(size_t whence, void* buff, size_t bufflen, size_t nremove){
    assert(nremove + whence <= bufflen);
    size_t tail = bufflen - whence - nremove;
    char* p = buff;
    if(tail) memmove(p+whence, p+whence+nremove, tail);
}
#ifdef __clang__
#pragma clang assume_nonnull end
#endif

#endif
