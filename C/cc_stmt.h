#ifndef C_CC_STMT_H
#define C_CC_STMT_H
//
// Copyright © 2026-2026, David Priver <david@davidpriver.com>
//
#include <stdint.h>
#include "srcloc.h"
#include "../Drp/typed_enum.h"
#include "../Drp/parray.h"
typedef struct CcExpr CcExpr;
typedef struct CcVariable CcVariable;
#ifdef __clang__
#pragma clang assume_nonnull begin
#endif
enum CcStmtKind TYPED_ENUM(uint32_t){
    CC_STMT_NULL,
    CC_STMT_EXPR,
    CC_STMT_COMPOUND,
    CC_STMT_IF,
    CC_STMT_WHILE,
    CC_STMT_DOWHILE,
    CC_STMT_FOR,
    CC_STMT_SWITCH,
    CC_STMT_CASE,
    CC_STMT_DEFAULT,
    CC_STMT_RETURN,
    CC_STMT_BREAK,
    CC_STMT_CONTINUE,
    CC_STMT_GOTO,
    CC_STMT_LABEL,
};
TYPEDEF_ENUM(CcStmtKind, uint32_t);

// Statement tree representation (abstract)
typedef struct CcStmtNode CcStmtNode;
struct CcStmtNode {
    CcStmtKind kind;
    uint32_t count; // number of allocated entries in stmts[]
    SrcLoc loc;
    Parray(CcVariable) decls; // variables scoped to this statement
    // CC_STMT_NULL:
    // CC_STMT_EXPR:     exprs[0] = expr
    // CC_STMT_COMPOUND: stmts[0..count) = children
    // CC_STMT_IF:       exprs[0] = cond, stmts[0] = then, stmts[1] = else (nullable)
    // CC_STMT_WHILE:    exprs[0] = cond, stmts[0] = body
    // CC_STMT_DOWHILE:  exprs[0] = cond, stmts[0] = body
    // CC_STMT_FOR:      exprs[0] = cond, exprs[1] = inc,
    //                   stmts[0] = init, stmts[1] = body
    // CC_STMT_SWITCH:   exprs[0] = expr, stmts[0] = body
    // CC_STMT_CASE:     case_value = folded constant, stmts[0] = stmt
    // CC_STMT_DEFAULT:  stmts[0] = stmt
    // CC_STMT_RETURN:   exprs[0] = expr
    // CC_STMT_BREAK:
    // CC_STMT_CONTINUE:
    // CC_STMT_GOTO:     label
    // CC_STMT_LABEL:    label, stmts[0] = stmt
    union {
        CcExpr* expr; // CC_STMT_EXPR
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
