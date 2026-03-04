#ifndef C_CC_VAR_H
#define C_CC_VAR_H
//
// Copyright © 2026-2026, David Priver <david@davidpriver.com>
//
#include <stdint.h>
#include "../Drp/atom.h"
#include "srcloc.h"
#include "cc_type.h"
#include "cc_expr.h"
#ifdef __clang__
#pragma clang assume_nonnull begin
#endif

typedef struct CcVariable CcVariable;
struct CcVariable {
    Atom name;
    Atom _Nullable mangle;
    SrcLoc loc;
    CcQualType type;
    uint32_t extern_: 1,
             static_: 1,
             tenative: 1,
             _padding: 13,
             alignment: 16; // 0 means default alignment
    CcExpr* _Nullable initializer;
    void* _Nullable interp_val; // runtime storage for globals (sizeof(type) bytes)
};

#ifdef __clang__
#pragma clang assume_nonnull end
#endif
#endif
