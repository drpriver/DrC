//
// Copyright © 2024-2025, David Priver <david@davidpriver.com>
//
#ifndef ATOM_H
#define ATOM_H 1
#include <stdint.h>
#ifdef __clang__
#pragma clang assume_nonnull begin
#else
#ifndef _Nullable
#define _Nullable
#endif
#endif
enum AtomFlags {
    ATOM_NONE = 0,
    ATOM_ALLOCATED = 0x1,
};
typedef struct Atom_ Atom_;
struct Atom_ {
    uint32_t length:31; // 31 bits is enough for anybody
    uint32_t flags: 1;
    uint32_t hash;
    char data[];
};
typedef const Atom_* Atom;
_Alignas(Atom_) static const char _nil_atom[] = "\0\0\0\0\0\0\0\0\0";
#define nil_atom (Atom)_nil_atom

#ifdef __clang__
#pragma clang assume_nonnull end
#endif
#endif
