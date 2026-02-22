#ifndef C_CC_EXPR_H
#define C_CC_EXPR_H
//
// Copyright © 2026-2026, David Priver <david@davidpriver.com>
//
#include <stdint.h>
#include "srcloc.h"
#include "../Drp/typed_enum.h"
#include "cc_type.h"
#ifdef __clang__
#pragma clang assume_nonnull begin
#else
#ifndef _Nonnull
#define _Nonnull
#endif
#endif

enum CcExprKind TYPED_ENUM(uint32_t){
    CC_EXPR_VALUE, // Literals, sizeof, alignof, etc. get desugared to this
    CC_EXPR_SIZEOF_VMT, // sizeof of vla
    CC_EXPR_VARIABLE, // Reference to a variable, eagerly resolved
    CC_EXPR_FUNCTION, // Reference to a function, eagerly resolved
    CC_EXPR_IDENTIFIER, // maybe unneeded?
    CC_EXPR_COMPOUND_LITERAL,
    CC_EXPR_NEG,
    CC_EXPR_POS,
    CC_EXPR_BITNOT,
    CC_EXPR_LOGNOT,
    CC_EXPR_DEREF,
    CC_EXPR_ADDR,
    CC_EXPR_PREINC,
    CC_EXPR_PREDEC,
    CC_EXPR_POSTINC,
    CC_EXPR_POSTDEC,
    CC_EXPR_ADD,
    CC_EXPR_SUB,
    CC_EXPR_MUL,
    CC_EXPR_DIV,
    CC_EXPR_MOD,
    CC_EXPR_BITAND,
    CC_EXPR_BITOR,
    CC_EXPR_BITXOR,
    CC_EXPR_LSHIFT,
    CC_EXPR_RSHIFT,
    CC_EXPR_LOGAND,
    CC_EXPR_LOGOR,
    CC_EXPR_EQ,
    CC_EXPR_NE,
    CC_EXPR_LT,
    CC_EXPR_GT,
    CC_EXPR_LE,
    CC_EXPR_GE,
    CC_EXPR_ASSIGN,
    CC_EXPR_ADDASSIGN,
    CC_EXPR_SUBASSIGN,
    CC_EXPR_MULASSIGN,
    CC_EXPR_DIVASSIGN,
    CC_EXPR_MODASSIGN,
    CC_EXPR_BITANDASSIGN,
    CC_EXPR_BITORASSIGN,
    CC_EXPR_BITXORASSIGN,
    CC_EXPR_LSHIFTASSIGN,
    CC_EXPR_RSHIFTASSIGN,
    CC_EXPR_TERNARY,
    CC_EXPR_CAST,
    CC_EXPR_CALL,
    CC_EXPR_SUBSCRIPT,
    CC_EXPR_DOT,
    CC_EXPR_ARROW,
    CC_EXPR_COMMA,
    CC_EXPR_STATEMENT_EXPRESSION, // gnu statement expression
};

TYPEDEF_ENUM(CcExprKind, uint32_t);

typedef struct CcStatement CcStatement;
typedef struct CcVariable CcVariable;
typedef struct CcFunc CcFunc;
typedef struct CcExpr CcExpr;
struct CcExpr {
    union {
        uint32_t bits;
        struct {
            CcExprKind kind: 6;
            uint32_t extra: 26;
            uint32_t _pad;
        };
        struct {
            CcExprKind kind: 6;
            uint32_t _pad: 26; // number of args
            uint32_t nargs;
        } call;
        struct {
            CcExprKind kind: 6;
            uint32_t _pad: 26;
            uint32_t length; // 
        } str;
    };
    SrcLoc loc;
    CcQualType type;
    // `values` is interpreted based on `kind`.
    // For most nodes, it is just the referenced exprs, like the rhs and lhs of CC_EXPR_ADD
    // the lhs will be in value0;
    // For CC_EXPR_VALUE, it actually should be reinterpreted based on type. 
    union {
        CcExpr* value0;
        _Bool boolean;
        size_t field; // index into struct's fields
        uint64_t uinteger;
        int64_t integer;
        float float_;
        double double_;
        const char* text;
        CcStatement* stmt;
        CcVariable* var;
        CcFunc* func;
    };
    CcExpr*_Nonnull values[];
};


#ifdef __clang__
#pragma clang assume_nonnull end
#endif
#endif
