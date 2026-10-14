//
// Copyright © 2021-2025, David Priver <david@davidpriver.com>
//
#ifndef BYTE_BUFFER_H
#define BYTE_BUFFER_H
// for size_t
#include <stddef.h>

#ifdef __clang__
#pragma clang assume_nonnull begin
#endif
typedef struct ByteBuffer ByteBuffer;
struct ByteBuffer {
    size_t n_bytes;
    void* buff;
};
typedef struct ByteView ByteView;
struct ByteView {
    size_t n_bytes;
    const void* buff;
};

#ifdef __clang__
#pragma clang assume_nonnull end
#endif

#endif
