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
    CC_STMT_COMPOUND,   // { ... } (tree form only, flattened away by lowering)
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

// Statement tree representation (abstract)
typedef struct CcStmtNode CcStmtNode;
struct CcStmtNode {
    CcStmtKind kind;
    uint32_t count; // number of allocated entries in stmts[]
    SrcLoc loc;
    // Interpreted based on kind:
    // CC_STMT_NULL:     (nothing)
    // CC_STMT_EXPR:     exprs[0] = expr
    // CC_STMT_COMPOUND: stmts[0..count) = children
    // CC_STMT_IF:       exprs[0] = cond, stmts[0] = then, stmts[1] = else (nullable)
    // CC_STMT_WHILE:    exprs[0] = cond, stmts[0] = body
    // CC_STMT_DOWHILE:  exprs[0] = cond, stmts[0] = body
    // CC_STMT_FOR:      exprs[0] = cond, exprs[1] = inc (both nullable),
    //                   stmts[0] = init (nullable), stmts[1] = body
    // CC_STMT_SWITCH:   exprs[0] = expr, stmts[0] = body
    // CC_STMT_CASE:     case_value = folded constant, stmts[0] = stmt
    // CC_STMT_DEFAULT:  stmts[0] = stmt
    // CC_STMT_RETURN:   exprs[0] = expr (nullable)
    // CC_STMT_BREAK:    (nothing; lowering patches to a goto)
    // CC_STMT_CONTINUE: (nothing; lowering patches to a goto)
    // CC_STMT_GOTO:     label (unresolved; lowering emits it for cc_resolve_gotos)
    // CC_STMT_LABEL:    label, stmts[0] = stmt
    union {
        CcExpr* _Null_unspecified exprs[2];
        Atom label;          // CC_STMT_GOTO, CC_STMT_LABEL
        uint64_t case_value; // CC_STMT_CASE
    };
    CcStmtNode* _Null_unspecified stmts[];
};

typedef struct CcSwitchEntry CcSwitchEntry;
struct CcSwitchEntry {
    uint64_t value;
    uint32_t target;
};

#ifdef __clang__
#pragma clang assume_nonnull end
#endif
#endif
