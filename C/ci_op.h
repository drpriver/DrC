#ifndef C_CI_OP_H
#define C_CI_OP_H
//
// Copyright © 2026-2026, David Priver <david@davidpriver.com>
//
#include <stdint.h>
#include "srcloc.h"
#include "cc_stmt.h"
#include "cc_expr.h"
#include "../Drp/typed_enum.h"
#include "../Drp/atom.h"

#if !defined __clang__ && !defined _Null_unspecified
#define _Null_unspecified
#endif

enum CiOpKind TYPED_ENUM(uint32_t){
    CI_OP_EVAL,             // evaluate expr, discard result
    CI_OP_EVAL_INTO,        // evaluate expr into slots[slot..slot+slot_size)
    CI_OP_JUMP,             // pc = jump
    CI_OP_JUMP_FALSE,       // if !truthy(slots[slot]) pc = jump; expr provides the type
    CI_OP_JUMP_TRUE,        // if truthy(slots[slot]) pc = jump; expr provides the type
    CI_OP_RETURN,           // evaluate expr (nullable) into return_buf; pc = end
    CI_OP_SWITCH,           // multi-way conditional jump on slots[slot]; binary
                            // search sw.table, no match: pc = jump (default/exit)
};
TYPEDEF_ENUM(CiOpKind, uint32_t);

typedef struct CiOp CiOp;
struct CiOp {
    CiOpKind kind;
    uint32_t jump;
    uint32_t slot, slot_size;
    SrcLoc loc;
    CcExpr* _Null_unspecified expr;
    struct {
        CcSwitchEntry* _Null_unspecified table;
        size_t count;
    } sw;
};

#ifndef MARRAY_CIOP
#define MARRAY_CIOP
#define MARRAY_T CiOp
#include "../Drp/Marray.h"
#endif

// The lowered code of one function, hung off CcFunc.interp_ops.
typedef struct CiFuncOps CiFuncOps;
struct CiFuncOps {
    Marray(CiOp) code;
};

#endif
