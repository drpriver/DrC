#ifndef C_CC_EXPR_H
#define C_CC_EXPR_H
//
// Copyright © 2026-2026, David Priver <david@davidpriver.com>
//
#include <stdint.h>
#include "srcloc.h"
#include "../Drp/typed_enum.h"
#include "cc_type.h"
#include "cc_memory_order.h"
#ifdef __clang__
#pragma clang assume_nonnull begin
#else
#ifndef _Nonnull
#define _Nonnull
#endif
#ifndef _Null_unspecified
#define _Null_unspecified
#endif
#endif

enum CcExprKind TYPED_ENUM(uint32_t){
    CC_EXPR_VALUE, // Literals, sizeof, alignof, etc. get desugared to this
    CC_EXPR_SIZEOF_VMT, // sizeof of vla
    CC_EXPR_VARIABLE, // Reference to a variable, eagerly resolved
    CC_EXPR_FUNCTION, // Reference to a function, eagerly resolved
    CC_EXPR_COMPOUND_LITERAL,
    CC_EXPR_INIT_LIST,
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
    CC_EXPR_ATOMIC, // atomic builtin operation, op in extra field
    CC_EXPR_VA, // va_start, va_end, va_arg, va_copy; op in extra field
    CC_EXPR_BUILTIN, // __builtin_unreachable, etc.; op in extra field
    CC_EXPR_ADD_OVERFLOW,
    CC_EXPR_MUL_OVERFLOW,
    CC_EXPR_SUB_OVERFLOW,
    CC_EXPR_POPCOUNT,
    CC_EXPR_CLZ,
    CC_EXPR_CTZ,
    CC_EXPR_ALLOCA,
};

TYPEDEF_ENUM(CcExprKind, uint32_t);

enum CcAtomicOp TYPED_ENUM(uint32_t) {
    CC_ATOMIC_FETCH_ADD,
    CC_ATOMIC_FETCH_SUB,
    CC_ATOMIC_LOAD_N,
    CC_ATOMIC_LOAD,
    CC_ATOMIC_STORE_N,
    CC_ATOMIC_STORE,
    CC_ATOMIC_EXCHANGE_N,
    CC_ATOMIC_EXCHANGE,
    CC_ATOMIC_COMPARE_EXCHANGE_N,
    CC_ATOMIC_COMPARE_EXCHANGE,
    CC_ATOMIC_THREAD_FENCE,
    CC_ATOMIC_SIGNAL_FENCE,
};
TYPEDEF_ENUM(CcAtomicOp, uint32_t);

enum CcVaOp TYPED_ENUM(uint32_t) {
    CC_VA_START,
    CC_VA_END,
    CC_VA_ARG,
    CC_VA_COPY,
};
TYPEDEF_ENUM(CcVaOp, uint32_t);

enum CcBuiltinOp TYPED_ENUM(uint32_t) {
    CC_BUILTIN_UNREACHABLE,
    CC_BUILTIN_TRAP,
    CC_BUILTIN_DEBUGTRAP,
    CC_BUILTIN_ABORT,
    CC_BUILTIN_BACKTRACE,
};
TYPEDEF_ENUM(CcBuiltinOp, uint32_t);

typedef struct CcStatement CcStatement;
typedef struct CcVariable CcVariable;
typedef struct CcFunc CcFunc;
typedef struct CcExpr CcExpr;

typedef struct CcFieldLoc CcFieldLoc;
struct CcFieldLoc {
    uint64_t byte_offset: 48;
    uint64_t bit_offset: 8; // bit offset within storage unit (bitfields)
    uint64_t bit_width: 8;  // 0 = not a bitfield
};

// Each entry is a scalar store at a byte offset into the aggregate.
typedef struct CcInitEntry CcInitEntry;
struct CcInitEntry {
    CcFieldLoc field_loc;
    CcExpr*_Null_unspecified value;
};

typedef struct CcInitList CcInitList;
struct CcInitList {
    SrcLoc loc;
    uint32_t count;
    CcInitEntry entries[];
};

struct CcExpr {
    union {
        uint32_t bits[2];
        struct {
            CcExprKind kind: 8;
            uint32_t _padding: 23;
            uint32_t is_lvalue: 1;
            uint32_t _pad;
        };
        struct {
            CcExprKind kind: 8;
            CcVaOp op: 8;
            uint32_t _bitpadding: 15;
            uint32_t is_lvalue: 1;
            uint32_t _pad;
        } va;
        struct {
            CcExprKind kind: 8;
            uint32_t _pad: 23;
            uint32_t is_lvalue: 1;
            uint32_t nargs;
        } call;
        struct {
            CcExprKind kind: 8;
            uint32_t _pad: 23;
            uint32_t is_lvalue: 1;
            uint32_t length; //
        } str;
        struct {
            CcExprKind kind: 8;
            CcAtomicOp op: 8;
            CcMemoryOrder memorder: 4;
            CcMemoryOrder fail_memorder: 4; // compare_exchange only
            uint32_t weak: 1;          // weak flag (compare_exchange only)
            uint32_t _bitpadding: 6;
            uint32_t is_lvalue: 1;
            uint32_t _pad;
        } atomic;
        struct {
            CcExprKind kind: 8;
            CcBuiltinOp op: 8;
            uint32_t _padding:15;
            uint32_t is_lvalue: 1;
            uint32_t _pad;
        } builtin;
    };
    SrcLoc loc;
    CcQualType type;
    // For binary ops: lhs is the left operand, values[0] is the right operand.
    // For unary ops/casts: lhs is the operand.
    // For CC_EXPR_VALUE: reinterpret based on type (uinteger, float_, etc).
    union {
        CcExpr* lhs;
        _Bool boolean;
        uint64_t uinteger;
        int64_t integer;
        float float_;
        double double_;
        const char* text;
        CcStatement* stmt;
        CcVariable* var;
        CcFunc* func;
        CcInitList* init_list;
        CcFieldLoc field_loc; // CC_EXPR_DOT, CC_EXPR_ARROW: resolved field offset+bitfield info
    };
    CcExpr*_Nonnull values[];
};
_Static_assert(__builtin_offsetof(CcExpr, loc) ==8, "");


#ifdef __clang__
#pragma clang assume_nonnull end
#endif
#endif
