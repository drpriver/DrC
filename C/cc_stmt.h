#ifndef C_CC_STMT_H
#define C_CC_STMT_H
//
// Copyright © 2026-2026, David Priver <david@davidpriver.com>
//
#include <stdint.h>
#include "srcloc.h"
#include "../Drp/typed_enum.h"
#include "cc_expr.h"
#ifdef __clang__
#pragma clang assume_nonnull begin
#endif
enum CcStmtKind TYPED_ENUM(uint32_t){
    CC_STMT_NULL,       // ;
    CC_STMT_EXPR,       // expr;
    CC_STMT_IF,         // if (cond) then [else]
    CC_STMT_WHILE,      // while (cond) body
    CC_STMT_DOWHILE,    // do body while (cond);
    CC_STMT_FOR,        // for (init; cond; inc) body
    CC_STMT_SWITCH,     // switch (expr) { ... }
    CC_STMT_CASE,       // case expr:
    CC_STMT_DEFAULT,    // default:
    CC_STMT_RETURN,     // return [expr];
    CC_STMT_BREAK,      // break;
    CC_STMT_CONTINUE,   // continue;
    CC_STMT_GOTO,       // goto label;
    CC_STMT_LABEL,      // label:
};
TYPEDEF_ENUM(CcStmtKind, uint32_t);

// All statements in a function are stored in a flat array.
// Control flow references targets by index into that array.
typedef struct CcStatement CcStatement;
struct CcStatement {
    CcStmtKind kind;
    uint32_t targets[3];
    SrcLoc loc;
    // Interpreted based on kind:
    // CC_STMT_EXPR:    exprs[0] = expr
    // CC_STMT_IF:      exprs[0] = cond
    // CC_STMT_WHILE:   exprs[0] = cond
    // CC_STMT_DOWHILE: exprs[0] = cond
    // CC_STMT_FOR:     exprs[0] = init, exprs[1] = cond, exprs[2] = inc (all nullable)
    // CC_STMT_SWITCH:  exprs[0] = expr
    // CC_STMT_CASE:    exprs[0] = case value expr
    // CC_STMT_RETURN:  exprs[0] = expr (nullable)
    CcExpr* _Nullable exprs[3];
};

#ifdef __clang__
#pragma clang assume_nonnull end
#endif
#endif
