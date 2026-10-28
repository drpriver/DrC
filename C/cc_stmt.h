#ifndef C_CC_STMT_H
#define C_CC_STMT_H
//
// Copyright © 2026-2026, David Priver <david@davidpriver.com>
//
#include <stdint.h>
#include <stddef.h>
#include "srcloc.h"
#include "../Drp/typed_enum.h"
#include "../Drp/parray.h"
#include "../Drp/atom.h"
typedef struct CcExpr CcExpr;
typedef struct CcVariable CcVariable;
#ifdef __clang__
#pragma clang assume_nonnull begin
#pragma clang diagnostic push
#pragma clang diagnostic error "-Wpadded"
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
    CC_STMT_DEFER,
    CC_STMT_VLA,
};
TYPEDEF_ENUM(CcStmtKind, uint32_t);

typedef struct CcStmtNode CcStmtNode;
typedef struct CcDefer CcDefer;
struct CcDefer {
    CcDefer* _Nullable prev;
    CcStmtNode* _Nullable body; // borrowed from CC_STMT_DEFER
    CcVariable* _Nullable stack_mark; // hidden saved mark. NULL for a VM-only marker
    _Bool vm_scope;
    unsigned char _pad[7]; // free real estate
};
typedef struct CcDeferContext CcDeferContext;
struct CcDeferContext {
    CcDefer* _Nullable active,
           * _Nullable deferred_body;
};
_Static_assert(sizeof(CcDeferContext) == 2*sizeof(uint64_t), "");
typedef struct CcCleanupRange CcCleanupRange;
struct CcCleanupRange {
    CcDefer* _Nullable first,
           * _Nullable stop; // exclusive
};
struct CcStmtNode {
    CcStmtKind kind:          8;
    uint32_t jump_levels:     8, // BREAK/CONTINUE: enclosing loops/switches, target included
             no_fallthrough:  1, // conservative
             has_jump_target: 1, // subtree contains a runtime label, case, or default
             case_empty:      1, // CC_STMT_CASE: no dispatch entry, but retain the body
             zero_initialize: 1, // CC_STMT_VLA: initializer requests zeroed storage
             _bit_padding:   12; // free real estate
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
    // CC_STMT_CASE:     case_ = inclusive folded bounds, stmts[0] = stmt
    // CC_STMT_DEFAULT:  stmts[0] = stmt
    // CC_STMT_RETURN:   return_.expr, return_.first (all active cleanups)
    // CC_STMT_BREAK:    cleanup
    // CC_STMT_CONTINUE: cleanup
    // CC_STMT_GOTO:     goto_.label, goto_.cleanup (nullable)
    // CC_STMT_LABEL:    label, stmts[0] = stmt
    // CC_STMT_DEFER:    stmts[0] = deferred body (owned, emitted via cleanup)
    union {
        struct { // CC_STMT_VLA
            CcVariable* object;
            CcVariable* mark;
        } vla;
        CcExpr* _Null_unspecified exprs[2];
        struct {
            uint64_t low, high;
        } case_;
        CcCleanupRange cleanup; // CC_STMT_NULL/COMPOUND/BREAK/CONTINUE only
        struct {
            CcExpr* _Nullable expr; // also exprs[0]
            CcDefer* _Nullable first; // returns always unwind to NULL
        } return_;
        struct {
            Atom label;
            CcCleanupRange* _Nullable cleanup;
        } goto_;
        struct {
            union {
                CcExpr* expr; // CC_STMT_EXPR
                Atom label;          // CC_STMT_GOTO, CC_STMT_LABEL
            };
            uint64_t _8_pad;
        };
    };
    CcStmtNode* _Null_unspecified stmts[];
};
_Static_assert(offsetof(CcStmtNode, count) == sizeof(uint32_t), "");

typedef struct CcSwitchEntry CcSwitchEntry;
struct CcSwitchEntry {
    uint64_t low, high; // inclusive range
    uint32_t target;
    uint32_t _pad; // free real estate
};

#ifdef __clang__
#pragma clang diagnostic pop
#pragma clang assume_nonnull end
#endif
#endif
