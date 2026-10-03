#ifndef C_CC_EXPR_H
#define C_CC_EXPR_H
//
// Copyright © 2026-2026, David Priver <david@davidpriver.com>
//
#include <stdint.h>
#include <stddef.h>
#include "srcloc.h"
#include "../Drp/typed_enum.h"
#include "cc_type.h"
#include "cc_memory_order.h"
#include "ci_softnum.h"
#include "cc_bit_builtin.h"
#include "../Drp/Allocators/allocator.h"
#include "../Drp/ckdint.h"
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
    // Owns lhs: a typed view of its storage, preserving padding and symbols.
    CC_EXPR_OBJECT_VIEW,
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
    CC_EXPR_BIT_BUILTIN,
    CC_EXPR_BSWAP,
    CC_EXPR_ALLOCA,
    CC_EXPR_INTERN, // __builtin_intern(const char*) -> const char*
    CC_EXPR_HOTSWAP, // __hotswap(function-pointer, function-pointer) -> int
    CC_EXPR_SRCLOC_REFLECT, // _SrcLoc properties; op in srcloc field
    CC_EXPR_COMPILE, // __compile(const char*) -> _Module
    CC_EXPR_MODULE_REFLECT, // _Module reflection methods/properties; op in extra field
    CC_EXPR_TYPE_INTROSPECTION, // _Type method; op in extra field, lhs = _Type expr
    CC_EXPR_UMUL128, // _umul128(a, b, &high) -> low
    CC_EXPR_SLICE_ALL, // [:]
    CC_EXPR_SLICE, // [lo:hi]
    CC_EXPR_SLICE_LO, // [lo:]
    CC_EXPR_SLICE_HI, // [:hi]
};

TYPEDEF_ENUM(CcExprKind, uint32_t);

enum CcAtomicOp TYPED_ENUM(uint32_t) {
    CC_ATOMIC_FETCH_ADD,
    CC_ATOMIC_FETCH_SUB,
    CC_ATOMIC_ADD_FETCH,
    CC_ATOMIC_SUB_FETCH,
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
    CC_ATOMIC_FETCH_AND,
    CC_ATOMIC_FETCH_OR,
    CC_ATOMIC_FETCH_XOR,
    CC_ATOMIC_INTERLOCKED_COMPARE_EXCHANGE,
    CC_ATOMIC_INTERLOCKED_COMPARE_EXCHANGE128,
    CC_ATOMIC_INTERLOCKED_INCREMENT,
    CC_ATOMIC_INTERLOCKED_DECREMENT,
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

enum CcTypeIntrospectionOp TYPED_ENUM(uint32_t) {
    CC_TYPE_NONE = 0,
    // Properties (no parens, lhs = _Type expr)
    CC_TYPE_NAME,
    CC_TYPE_TAG,
    CC_TYPE_IS_VALID,
    CC_TYPE_IS_INVALID,
    CC_TYPE_IS_INTEGER,
    CC_TYPE_IS_FLOAT,
    CC_TYPE_IS_ARITHMETIC,
    CC_TYPE_IS_POINTER,
    CC_TYPE_IS_STRUCT,
    CC_TYPE_IS_UNION,
    CC_TYPE_IS_ARRAY,
    CC_TYPE_IS_VECTOR,
    CC_TYPE_IS_SLICE,
    CC_TYPE_IS_FUNCTION,
    CC_TYPE_IS_ENUM,
    CC_TYPE_IS_CONST,
    CC_TYPE_IS_VOLATILE,
    CC_TYPE_IS_ATOMIC,
    CC_TYPE_IS_UNSIGNED,
    CC_TYPE_IS_SIGNED,
    CC_TYPE_IS_CALLABLE,
    CC_TYPE_IS_INCOMPLETE,
    CC_TYPE_IS_VARIADIC,
    CC_TYPE_SIZEOF,
    CC_TYPE_ALIGNOF,
    CC_TYPE_POINTEE,
    CC_TYPE_UNQUAL,
    CC_TYPE_COUNT,
    CC_TYPE_LOC,
    // Methods (with parens, lhs = _Type expr, values[0] = arg _Type expr)
    CC_TYPE_IS_CALLABLE_WITH,
    CC_TYPE_IS_CALLABLE_THROUGH,
    CC_TYPE_CASTABLE_TO,
    CC_TYPE_MAKE_ANY,
    CC_TYPE_FIELD,
    CC_TYPE_FIELDS,
    CC_TYPE_METHOD,
    CC_TYPE_METHODS,
    CC_TYPE_HAS_FIELD,
    CC_TYPE_HAS_METHOD,
    CC_TYPE_PUSH_METHOD,
    CC_TYPE_ENUMERATORS,
    CC_TYPE_ENUMERATOR,
    CC_TYPE_RETURN_TYPE,
    CC_TYPE_PARAM_COUNT,
    CC_TYPE_PARAM_TYPE,
    CC_TYPE_ELEMENT_TYPE,
    CC_TYPE_UNDERLYING_TYPE,
};
TYPEDEF_ENUM(CcTypeIntrospectionOp, uint32_t);

enum CcModuleOp TYPED_ENUM(uint32_t) {
    CC_MODULE_NONE = 0,
    CC_MODULE_FUNC_COUNT,
    CC_MODULE_FUNC,
    CC_MODULE_FUNC_INFO,
    CC_MODULE_VAR_COUNT,
    CC_MODULE_VAR,
    CC_MODULE_TYPE_COUNT,
    CC_MODULE_TYPE,

    CC_MODULE_PARSE_TYPE,
    CC_MODULE_RUN,
    CC_MODULE_SYMBOL,
};
TYPEDEF_ENUM(CcModuleOp, uint32_t);

enum CcSrcLocOp TYPED_ENUM(uint32_t) {
    CC_SRCLOC_FILE,
    CC_SRCLOC_LINE,
    CC_SRCLOC_COL,
};
TYPEDEF_ENUM(CcSrcLocOp, uint32_t);

typedef struct CcStmtNode CcStmtNode;
typedef struct CcVariable CcVariable;
typedef struct CcFunc CcFunc;
typedef struct CcExpr CcExpr;

typedef struct CcFieldPath CcFieldPath;
struct CcFieldPath {
    union {
        uint64_t _bits;
        struct {
            uint64_t is_extended: 1, // if true, actually a pointer to CcFieldPathEx
                     ptr: 63; // (uintptr_t)CcFieldPathEx* >> 1
        };
        struct {
            uint64_t _inline_is_extended: 1,
                     n_components: 3, // 0..6 inline components; 7 means one large index
                     idx0: 10,
                     idx1: 10,
                     idx2: 10,
                     idx3: 10,
                     idx4: 10,
                     idx5: 10;
        };
        struct {
            uint64_t _large_is_extended: 1,
                     _large_n_components: 3,
                     large_index: 60;
        };
    };
};
typedef struct CcFieldPathEx CcFieldPathEx;
struct CcFieldPathEx {
    uint32_t n_components;
    uint32_t components[];
};
_Static_assert(sizeof(CcFieldPath) == sizeof(uint64_t), "field paths must fit in eight bytes");

static inline
CcFieldPathEx*
cc_field_path_extended(CcFieldPath path){
    return (CcFieldPathEx*)((uintptr_t)path.ptr << 1);
}
static inline
uint32_t
cc_field_path_count(CcFieldPath path){
    if(path.is_extended) return cc_field_path_extended(path)->n_components;
    return path.n_components == 7 ? 1 : (uint32_t)path.n_components;
}
static inline
uint32_t
cc_field_path_component(CcFieldPath path, uint32_t i){
    if(path.is_extended) return cc_field_path_extended(path)->components[i];
    if(path.n_components == 7) return i == 0 ? (uint32_t)path.large_index : 0;
    switch(i){
        case 0: return (uint32_t)path.idx0;
        case 1: return (uint32_t)path.idx1;
        case 2: return (uint32_t)path.idx2;
        case 3: return (uint32_t)path.idx3;
        case 4: return (uint32_t)path.idx4;
        case 5: return (uint32_t)path.idx5;
        default: return 0;
    }
}
static inline
_Bool
cc_field_path_is_prefix(CcFieldPath prefix, CcFieldPath path){
    uint32_t count = cc_field_path_count(prefix);
    if(count > cc_field_path_count(path)) return 0;
    for(uint32_t i = 0; i < count; i++)
        if(cc_field_path_component(prefix, i) != cc_field_path_component(path, i)) return 0;
    return 1;
}
static inline
_Bool
cc_field_path_equal(CcFieldPath a, CcFieldPath b){
    return cc_field_path_count(a) == cc_field_path_count(b) && cc_field_path_is_prefix(a, b);
}
// Ancestors replace their children. Distinct union members share storage;
// distinct array elements and struct members identify independent subobjects.
static inline
_Bool
cc_field_paths_overlap(CcQualType type, CcFieldPath a, CcFieldPath b){
    uint32_t ac = cc_field_path_count(a), bc = cc_field_path_count(b);
    for(uint32_t i = 0; i < ac && i < bc; i++){
        uint32_t ai = cc_field_path_component(a, i), bi = cc_field_path_component(b, i);
        CcTypeKind kind = ccqt_kind(type);
        if(ai != bi) return kind == CC_UNION;
        if(kind == CC_ARRAY) type = ccqt_as_array(type)->element;
        else if(kind == CC_STRUCT) type = ccqt_as_struct(type)->fields[ai].type;
        else if(kind == CC_UNION) type = ccqt_as_union(type)->fields[ai].type;
        else return 1;
    }
    return 1;
}
static inline
int
cc_field_path_make(Allocator al, const uint32_t*_Nullable components, uint32_t count, CcFieldPath* out){
    CcFieldPath path = {0};
    if(count == 1 && components[0] > 1023){
        path.n_components = 7;
        path.large_index = components[0];
        *out = path;
        return 0;
    }
    _Bool extended = count > 6;
    for(uint32_t i = 0; i < count && !extended; i++) extended = components[i] > 1023;
    if(extended){
        size_t size;
        if(mul_overflow((size_t)count, sizeof(uint32_t), &size) || add_overflow(sizeof(CcFieldPathEx), size, &size))
            return 1;
        CcFieldPathEx* ex = Allocator_alloc(al, size);
        if(!ex) return 1;
        ex->n_components = count;
        for(uint32_t i = 0; i < count; i++) ex->components[i] = components[i];
        path.is_extended = 1;
        path.ptr = (uintptr_t)ex >> 1;
    }
    else {
        path.n_components = count;
        if(count > 0) path.idx0 = components[0];
        if(count > 1) path.idx1 = components[1];
        if(count > 2) path.idx2 = components[2];
        if(count > 3) path.idx3 = components[3];
        if(count > 4) path.idx4 = components[4];
        if(count > 5) path.idx5 = components[5];
    }
    *out = path;
    return 0;
}
static inline
void
cc_field_path_free(Allocator al, CcFieldPath path){
    if(path.is_extended){
        CcFieldPathEx* ex = cc_field_path_extended(path);
        Allocator_free(al, ex, sizeof *ex + (size_t)ex->n_components * sizeof(uint32_t));
    }
}
static inline
int
cc_field_path_concat(Allocator al, CcFieldPath prefix, CcFieldPath suffix, CcFieldPath* out){
    uint32_t a = cc_field_path_count(prefix), b = cc_field_path_count(suffix);
    uint32_t count;
    if(add_overflow(a, b, &count)) return 1;
    uint32_t small[6];
    if(count <= 6){
        for(uint32_t i = 0; i < a; i++) small[i] = cc_field_path_component(prefix, i);
        for(uint32_t i = 0; i < b; i++) small[a+i] = cc_field_path_component(suffix, i);
        return cc_field_path_make(al, small, count, out);
    }
    size_t size;
    if(mul_overflow((size_t)count, sizeof(uint32_t), &size) || add_overflow(sizeof(CcFieldPathEx), size, &size))
        return 1;
    CcFieldPathEx* ex = Allocator_alloc(al, size);
    if(!ex) return 1;
    ex->n_components = count;
    for(uint32_t i = 0; i < a; i++) ex->components[i] = cc_field_path_component(prefix, i);
    for(uint32_t i = 0; i < b; i++) ex->components[a+i] = cc_field_path_component(suffix, i);
    *out = (CcFieldPath){.is_extended=1, .ptr=(uintptr_t)ex >> 1};
    return 0;
}
static inline
int
cc_field_path_drop(Allocator al, CcFieldPath path, uint32_t count, CcFieldPath* out){
    uint32_t length = cc_field_path_count(path);
    if(count > length) return 1;
    uint32_t small[6];
    uint32_t rest = length - count;
    if(rest <= 6){
        for(uint32_t i = 0; i < rest; i++) small[i] = cc_field_path_component(path, count+i);
        return cc_field_path_make(al, small, rest, out);
    }
    // Only the extended representation can have more than six components.
    return cc_field_path_make(al, cc_field_path_extended(path)->components + count, rest, out);
}

static inline
int
cc_field_path_take(Allocator al, CcFieldPath path, uint32_t count, CcFieldPath* out){
    if(count > cc_field_path_count(path)) return 1;
    if(path.is_extended) return cc_field_path_make(al, cc_field_path_extended(path)->components, count, out);
    uint32_t small[6];
    for(uint32_t i = 0; i < count; i++) small[i] = cc_field_path_component(path, i);
    return cc_field_path_make(al, small, count, out);
}

// Resolve member identity without computing a byte layout.
static inline
CcField*_Nullable
cc_field_path_field(CcQualType type, CcFieldPath path){
    CcField* field = NULL;
    for(uint32_t i = 0, count = cc_field_path_count(path); i < count; i++){
        uint32_t index = cc_field_path_component(path, i);
        switch(ccqt_kind(type)){
            case CC_STRUCT: field = &ccqt_as_struct(type)->fields[index]; break;
            case CC_UNION: field = &ccqt_as_union(type)->fields[index]; break;
            case CC_ARRAY:
                field = NULL;
                type = ccqt_as_array(type)->element;
                continue;
            default: return NULL; // Slice and _Any pseudo-members aren't bitfields.
        }
        type = field->type;
    }
    return field;
}

static inline
uint32_t
cc_field_path_bit_width(CcQualType type, CcFieldPath path){
    CcField* field = cc_field_path_field(type, path);
    return field && field->is_bitfield ? field->bitwidth : 0;
}

static inline
uint32_t
cc_field_path_bit_offset(CcQualType type, CcFieldPath path){
    CcField* field = cc_field_path_field(type, path);
    return field && field->is_bitfield ? field->bitoffset : 0;
}

// An initializer for the subobject at path.
// A nested initializer list preserves whole-subobject initialization/zeroing.
typedef struct CcInitEntry CcInitEntry;
struct CcInitEntry {
    CcFieldPath path;
    CcExpr*_Null_unspecified value;
};

typedef struct CcInitList CcInitList;
struct CcInitList {
    SrcLoc loc;
    uint32_t count;
    uint32_t rc;
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
            CcBitBuiltinOp op: 8;
            uint32_t _padding: 15;
            uint32_t is_lvalue: 1;
            uint32_t nargs;
        } bit_builtin;
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
        struct {
            CcExprKind kind: 8;
            CcTypeIntrospectionOp op: 8;
            uint32_t _padding:15;
            uint32_t is_lvalue: 1;
            uint32_t _pad;
        } type_introspection;
        struct {
            CcExprKind kind: 8;
            CcModuleOp op: 8;
            uint32_t _padding:15;
            uint32_t is_lvalue: 1;
            uint32_t _pad;
        } module;
        struct {
            CcExprKind kind: 8;
            CcSrcLocOp op: 8;
            uint32_t _padding:15;
            uint32_t is_lvalue: 1;
            uint32_t _pad;
        } srcloc;
    };
    SrcLoc loc;
    CcQualType type;
    // For binary ops: lhs is the left operand, values[0] is the right operand.
    // For unary ops/casts: lhs is the operand.
    // For CC_EXPR_VALUE: reinterpret based on type (uinteger, float_, etc).
    union {
        CcExpr* lhs;
        struct { CcExpr* lhs; CcQualType type; } compound; // arithmetic type before storing
        _Bool boolean;
        uint64_t uinteger;
        int64_t integer;
        float float_;
        double double_;
        const char* text;
        CcVariable* var;
        CcFunc* func;
        CcInitList* init_list;
        CcFieldPath field_path; // CC_EXPR_DOT, CC_EXPR_ARROW: member path from values[0]
        CcQualType type_value; // for expressions of type type
        CcStmtNode* stmt_body; // CC_EXPR_STATEMENT_EXPRESSION: a CC_STMT_COMPOUND;
                               // value/type = trailing CC_STMT_EXPR's expr
        CiFloat80 x87;
        CiFloat128 quad;
        CiUint128 uinteger128;
        CiInt128 integer128;
        SrcLoc loc_value;
        unsigned char data[16];
    };
    CcExpr*_Nonnull values[];
};
static inline
CcQualType
cc_expr_field_owner(const CcExpr* e){
    CcQualType type = e->values[0]->type;
    return e->kind == CC_EXPR_ARROW ? ccqt_as_ptr(type)->pointee : type;
}

static inline
uint32_t
cc_expr_field_bit_width(const CcExpr* e){
    // Comma expressions preserve the right operand's lvalue designation.
    while(e->kind == CC_EXPR_COMMA) e = e->values[0];
    if(e->kind != CC_EXPR_DOT && e->kind != CC_EXPR_ARROW) return 0;
    return cc_field_path_bit_width(cc_expr_field_owner(e), e->field_path);
}

static inline
uint32_t
cc_expr_field_bit_offset(const CcExpr* e){
    while(e->kind == CC_EXPR_COMMA) e = e->values[0];
    if(e->kind != CC_EXPR_DOT && e->kind != CC_EXPR_ARROW) return 0;
    return cc_field_path_bit_offset(cc_expr_field_owner(e), e->field_path);
}

_Static_assert(offsetof(CcExpr, loc) ==8, "");

#ifdef __clang__
#pragma clang assume_nonnull end
#endif
#endif
