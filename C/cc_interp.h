#ifndef C_CC_INTERP_H
#define C_CC_INTERP_H
//
// Copyright © 2026-2026, David Priver <david@davidpriver.com>
//
#include <stddef.h>
#include "cc_stmt.h"
#include "cc_expr.h"
#include "../Drp/pointer_map.h"
#include "../Drp/free_list.h"
#ifdef __clang__
#pragma clang assume_nonnull begin
#endif

struct CcParser;

typedef struct CcInterpFrame CcInterpFrame;
struct CcInterpFrame {
    CcInterpFrame* parent;
    PointerMap locals;
    size_t pc;
    size_t stmt_count;
    CcStatement* stmts;
    size_t data_length; // after this is the data, but we can't use a FLA and also embed in CcInterpreter
};

// Execute one statement at current pc. Advances pc.
// Returns 0 on success, >0 on error.
static int cc_interp_step(struct CcParser* p);

// Evaluate an expression, writing sizeof(expr->type) bytes into result.
static int cc_interp_expr(struct CcParser* p, CcExpr* expr, void* result, size_t size);

// Evaluate an expression as an lvalue, returning pointer to its storage.
static int cc_interp_lvalue(struct CcParser* p, CcExpr* expr, void*_Nullable*_Nonnull out, size_t* size);

#ifdef __clang__
#pragma clang assume_nonnull end
#endif
#endif
