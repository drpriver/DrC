//
// Copyright © 2026-2026, David Priver <david@davidpriver.com>
//
// Like Atom, but for arbitrary blobs instead of just text.
//
#ifndef BLOB_ATOM_H
#define BLOB_ATOM_H
#include <stdint.h>
#ifdef __clang__
#pragma clang assume_nonnull begin
#else
#ifndef _Nullable
#define _Nullable
#endif
#endif

typedef struct BlobAtom_ BlobAtom_;
typedef const BlobAtom_ *BlobAtom;
struct BlobAtom_ {
    _Alignas(16) uint64_t length;
    uint64_t hash;
    uint64_t data[];
};
_Alignas(BlobAtom_) static const uint64_t _nil_blob[2];
#define nil_blob (BlobAtom)_nil_blob

#ifdef __clang__
#pragma clang assume_nonnull end
#endif
#endif
