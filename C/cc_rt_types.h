#ifndef C_RT_TYPES_H
#define C_RT_TYPES_H
//
// Copyright © 2026-2026, David Priver <david@davidpriver.com>
//
#include <stdint.h>
#include <stddef.h>
#include "cc_tok.h"
#if !defined __clang__ && !defined _Null_unspecified
#define _Null_unspecified
#endif

// Type definitions that are the same in the target and the host.
// This makes it easier to manipulate.

// NOTE: these structs are designed so they match the layout on
// any of our targets.
typedef struct CiRtSlice CiRtSlice;
struct CiRtSlice {
    size_t count;
    void*_Null_unspecified data;
};

typedef struct CiRtField CiRtField;
struct CiRtField {
    CcQualType type;
    CiRtSlice name;
    unsigned offset,
             bitwidth,
             bitoffset,
             is_bitfield;
};


typedef struct CiRtModuleMember CiRtModuleMember;
struct CiRtModuleMember {
    CcQualType type;
    CiRtSlice name;
    void* _Null_unspecified address;
    SrcLoc loc;
};

typedef struct CiRtEnumerator CiRtEnumerator;
struct CiRtEnumerator {
    CiRtSlice name;
    int64_t value;
};

typedef struct CiRtAny CiRtAny;
struct CiRtAny {
    CcQualType type;
    _Alignas(8) unsigned char payload[8];
};
_Static_assert(sizeof(CiRtAny) == 16, "");
_Static_assert(offsetof(CiRtAny, type) == 0, "");
_Static_assert(offsetof(CiRtAny, payload) == 8, "");
#endif
