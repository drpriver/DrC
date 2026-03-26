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
             automatic: 1,
             interp_initialized: 1,
             interp_preinit: 1,
             constexpr_: 1,
             _padding: 10,
             alignment: 16; // 0 means default alignment
    CcExpr* _Null_unspecified initializer;
    union {
        void* _Null_unspecified interp_val; // runtime storage for extern/static vars
        uintptr_t frame_offset;      // offset into CiInterpFrame trailing data (automatic vars)
    };
};

#ifdef __clang__
#pragma clang assume_nonnull end
#endif
#endif
