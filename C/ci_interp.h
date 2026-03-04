#ifndef C_CI_INTERP_H
#define C_CI_INTERP_H
//
// Copyright © 2026-2026, David Priver <david@davidpriver.com>
//
#include <stddef.h>
#include "cc_stmt.h"
#include "cc_expr.h"
#include "ci_interp.h"
#include "cc_parser.h"
#include "../Drp/pointer_map.h"
#ifdef __clang__
#pragma clang assume_nonnull begin
#endif

typedef struct CiInterpFrame CiInterpFrame;
struct CiInterpFrame {
    CiInterpFrame* parent;
    PointerMap locals;
    size_t pc;
    size_t stmt_count;
    CcStatement* stmts;
    size_t data_length; // after this is the data, but we can't use a FLA and also embed in CcInterpreter
};

typedef struct CiInterpreter CiInterpreter;
struct CiInterpreter {
    CcParser parser;
    CiInterpFrame top_frame;
    CiInterpFrame *current_frame;
};

// Execute one statement at current pc. Advances pc.
// Returns 0 on success, >0 on error.
static int ci_interp_step(CiInterpreter* p);

// Evaluate an expression, writing sizeof(expr->type) bytes into result.
static int ci_interp_expr(CiInterpreter* p, CcExpr* expr, void* result, size_t size);

// Evaluate an expression as an lvalue, returning pointer to its storage.
static int ci_interp_lvalue(CiInterpreter* p, CcExpr* expr, void*_Nullable*_Nonnull out, size_t* size);

#ifdef __clang__
#pragma clang assume_nonnull end
#endif
#endif
