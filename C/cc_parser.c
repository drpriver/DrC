//
// Copyright © 2026-2026, David Priver <david@davidpriver.com>
//
#ifndef C_CC_PARSER_C
#define C_CC_PARSER_C
#include <stdarg.h>
#include <float.h>
#include <math.h>
#include "../Drp/msb_atomize.h"
#include "../Drp/bit_util.h"
#include "../Drp/parray.h"
#include "../Drp/merge_sort.h"
#include "../Drp/ckdint.h"
#include "../Drp/switch_macros.h"
#include "cc_expr.h"
#include "cc_parser.h"
#include "cpp_preprocessor.h"
#include "cc_errors.h"
#include "cc_printer.h"

#ifndef MARRAY_T_CCFUNCPARAM
#define MARRAY_T_CCFUNCPARAM
#define MARRAY_T CcFuncParam
#include "../Drp/Marray.h"
#endif

#ifdef __clang__
#pragma clang assume_nonnull begin
#endif

typedef struct CcParsedParams CcParsedParams;
struct CcParsedParams {
    Marray(CcFuncParam) names;
    CcScope* _Nullable scope;
    SrcLoc typed_pack_loc;
};

static int cc_parse_expr(CcParser* p, CcValueClass, CcExpr* _Nullable* _Nonnull out);
static int cc_parse_assignment_expr(CcParser* p, CcValueClass, CcExpr* _Nullable* _Nonnull out, CcQualType);
static int cc_parse_ternary_expr(CcParser* p, CcValueClass, CcExpr* _Nullable* _Nonnull out);
static int cc_parse_infix(CcParser* p, CcValueClass, CcExpr* left, int min_prec, CcExpr* _Nullable* _Nonnull out);
static int cc_parse_prefix(CcParser* p, CcValueClass, CcExpr* _Nullable* _Nonnull out);
static int cc_parse_primary(CcParser* p, CcValueClass, CcExpr* _Nullable* _Nonnull out);
static int cc_parse_postfix(CcParser* p, CcValueClass, CcExpr* operand, CcExpr* _Nullable* _Nonnull out);
static int cc_parse_lambda(CcParser* p, CcValueClass, SrcLoc loc, CcExpr* _Nullable* _Nonnull out);
static int cc_parse_lambda_body(CcParser* p, CcValueClass, SrcLoc loc, CcQualType type, CcParsedParams* param_names, CcExpr* _Nullable* _Nonnull out);
static int cc_skip_to_next_comma_or_paren(CcParser* p, const char* context);
static int cc_parse_Generic(CcParser* p, CcValueClass, CcExpr* _Nullable* _Nonnull out);
static int cc_next_token(CcParser* p, CcToken* tok);
static int cc_unget(CcParser* p, CcToken* tok);
static int cc_peek(CcParser* p, CcToken* tok);
static int cc_expect_punct(CcParser* p, CcPunct punct);
static int cc_expect_punct_supp(CcParser* p, CcPunct punct, const char*);
static void _cc_release_expr(CcParser* p, CcExpr*, size_t nvalues);
static void cc_release_expr(CcParser* p, CcExpr*);
static CcExpr*_Nullable _cc_alloc_expr(CcParser* p, size_t nvalues);
static CcExpr*_Nullable cc_make_expr(CcParser* p, CcExprKind kind, SrcLoc loc, CcQualType type, size_t nvalues);
warn_unused static int cc_pointer_of(CcParser* p, CcQualType pointee, CcQualType* out);
warn_unused static int cc_slice_of(CcParser* p, CcQualType pointee, CcQualType* out);
warn_unused static int cc_block_pointer_of(CcParser* p, CcQualType pointee, CcQualType* out);
LOG_PRINTF(3, 4) static int cc_error(CcParser*, SrcLoc, const char*, ...);
LOG_PRINTF(3, 4) static MStringBuilder* cc_start_error(CcParser*, SrcLoc, const char*, ...);
static int cc_finish_error(CcParser*, SrcLoc);
LOG_PRINTF(3, 4) static void cc_warn(CcParser*, SrcLoc, const char*, ...);
LOG_PRINTF(3, 4) static void cc_info(CcParser*, SrcLoc, const char*, ...);
LOG_PRINTF(3, 4) static void cc_debug(CcParser*, SrcLoc, const char*, ...);
#define cc_unimplemented(p, loc, msg) (cc_error(p, loc, "UNIMPLEMENTED: " msg " at %s:%d", __FILE__, __LINE__), CC_UNIMPLEMENTED_ERROR)
#define cc_unreachable(p, loc, msg) (cc_error(p, loc, "UNREACHABLE code reached: " msg " at %s:%d", __FILE__, __LINE__), CC_UNREACHABLE_ERROR)
#define cc_ice(cc, loc, fmt, ...) (cc_error(p, loc, "ICE: " fmt " at %s:%d", __VA_ARGS__, __FILE__, __LINE__), CC_UNREACHABLE_ERROR)
static _Bool cc_binop_lookup(CcPunct punct, CcExprKind* kind, int* prec);
static int cc_va_list_to_ptr(CcParser* p, SrcLoc loc, CcExpr*_Nonnull*_Nonnull e);
typedef struct CcEvalCtx CcEvalCtx;
typedef struct CcEvalVisit CcEvalVisit;
struct CcEvalVisit {
    CcEvalVisit* previous;
    CcExpr* expr;
    uint32_t offset, size;
};
struct CcEvalCtx {
    CcParser* parser;
    _Bool allow_const;
    unsigned variable_depth;
    unsigned evaluation_depth;
    CcEvalVisit* expressions;
    CcEvalVisit* objects;
};
static int cc_eval_expr(CcEvalCtx*, CcExpr*, CcExpr*_Nullable*_Nonnull);
static int cc_eval_integer(CcEvalCtx*, CcExpr*, int64_t*);
static CiUint128 cc_eval_u128(CcParser*, CcExpr*);
static int cc_eval_truthy(CcEvalCtx*, CcExpr*, _Bool*);
static int cc_check_linktime_expr(CcParser*, CcExpr*, _Bool, unsigned);
static _Bool cc_assign_lookup(CcPunct punct, CcExprKind* kind);
static Marray(CcToken)*_Nullable cc_get_scratch(CcParser* p);
static void cc_release_scratch(CcParser* p, Marray(CcToken)*);
static Allocator cc_allocator(CcParser*p);
static Allocator cc_scratch_allocator(CcParser*p);
static int cc_parse_declarator(CcParser* p, CcQualType* out_head, CcQualType*_Nonnull*_Nonnull out_tail, Atom _Nullable * _Nullable out_name, SrcLoc* _Nullable out_name_loc, CcParsedParams *_Nullable out_param_names);
static CcQualType cc_intern_qualtype(CcParser* p, CcQualType t);
static _Bool cc_is_type_start(CcParser* p, CcToken* tok);
static int cc_parse_func_body_inner(CcParser* p, CcFunc* f, _Bool terminate_on_rbrace);
static int cc_parse_local_methods(CcParser* p);
static CcLabelCtx* cc_label_ctx(CcParser* p);
static int cc_check_gotos(CcParser* p, CcLabelCtx* ctx);
static int cc_parse_type_name(CcParser* p, CcQualType* out, CcParsedParams* _Nullable param_names);
static int cc_sizeof_as_expr(CcParser* p, CcQualType t, SrcLoc loc, CcExpr* _Nullable* _Nonnull out);
static int cc_alignof_as_expr(CcParser* p, CcQualType t, SrcLoc loc, CcExpr* _Nullable* _Nonnull out);
static int cc_sizeof_as_uint(CcParser* p, CcQualType t, SrcLoc loc, uint32_t* out);
static int cc_alignof_as_uint(CcParser* p, CcQualType t, SrcLoc loc, uint32_t* out);
static int cc_check_cast(CcParser* _Nullable p, CcQualType from, CcQualType to, SrcLoc loc);
static CcExpr* _Nullable cc_value_expr(CcParser* p, SrcLoc loc, CcQualType type);
static CcExpr* _Nullable cc_int64_expr(CcParser* p, SrcLoc loc, CcQualType type, int64_t);
static CcExpr* _Nullable cc_integer_bits_expr(CcParser*, SrcLoc, CcQualType, CiUint128);
static CcExpr* _Nullable cc_uint64_expr(CcParser* p, SrcLoc loc, CcQualType type, uint64_t);
static CcExpr* _Nullable cc_unary_expr(CcParser* p, CcExprKind kind, SrcLoc loc, CcQualType type, CcExpr* operand);
static CcExpr* _Nullable cc_binary_expr(CcParser* p, CcExprKind kind, SrcLoc loc, CcQualType type, CcExpr* left, CcExpr* right);
typedef struct CcDeclBase CcDeclBase;
static int cc_check_func_compat(CcParser* p, CcFunc* existing, const CcDeclBase* declbase, CcQualType new_type, _Bool definition, SrcLoc loc);
static int cc_merge_compatible_decl_types(CcParser* p, CcQualType old, CcQualType new_, CcQualType* out);
static int cc_parse_attributes(CcParser* p, CcAttributes* attrs);
static _Bool cc_is_c23_attribute_start(CcParser* p);
static int cc_parse_c23_attributes(CcParser* p, CcAttributes* attrs);
static int cc_parse_declspec(CcParser* p, CcAttributes* attrs);
static int cc_parse_struct_or_union(CcParser* p, SrcLoc loc, _Bool is_union, CcQualType* base_type);
static int cc_check_anon_member_duplicates(CcParser* p, CcField* existing, uint32_t existing_count, CcQualType anon_type, SrcLoc loc);
static _Bool cc_has_field(CcParser*, CcField*_Nullable, uint32_t, Atom);
static int cc_lookup_field(CcParser*, CcField*_Nullable, uint32_t, Atom, CcFieldPath*_Nullable, CcQualType*, CcQualType*_Nullable, CcField*_Nullable*_Nonnull);
static int cc_lookup_field_offset(CcParser*, CcQualType, Atom, uint64_t*, CcQualType*, CcField*_Nullable*_Nonnull);
static int cc_compute_struct_layout(CcParser* p, CcStruct* s, uint16_t pack_value);
static int cc_compute_union_layout(CcParser* p, CcUnion* u, uint16_t pack_value);
static int cc_parse_init_list(CcParser* p, CcValueClass vc, CcExpr* _Nullable* _Nonnull out, CcQualType target_type);
static int cc_desugar_compound_literal(CcParser* p, CcExpr* compound_lit, CcExpr*_Nullable*_Nonnull out);
static const CcTargetConfig* cc_target(const CcParser*);
static int cc_handle_static_assert(CcParser*);
static CcStmtNode*_Nullable cc_stmt_node(CcParser*, CcStmtKind, SrcLoc, uint32_t count);
static int cc_stmt_scope_vars(CcParser*, CcStmtNode*);
static void cc_free_stmt_tree(CcParser*, CcStmtNode*_Nullable);
static CcStmtSink*_Nullable cc_push_stmt_sink(CcParser*);
static void cc_pop_stmt_sink(CcParser*, CcStmtSink*);
static int cc_sink_push(CcParser*, CcStmtNode*);
static int cc_finalize_stmt_list(CcParser*, SrcLoc, Parray(CcStmtNode)*, _Bool always_compound, CcStmtNode*_Nullable*_Nonnull out);
static int cc_has_builtin(void* _Null_unspecified ctx, CppPreprocessor* cpp, SrcLoc, CppTokens* outtoks, const CppTokens* args, const Marray(size_t)* arg_seps);
static int cc_check_printf_format(CcParser* p, CcFunc* func, CcExpr*_Nonnull*_Nonnull args, uint32_t nargs, SrcLoc loc);

enum {
    CC_NO_ERROR             = _cc_no_error,
    CC_OOM_ERROR            = _cc_oom_error,
    CC_SYNTAX_ERROR         = _cc_syntax_error,
    CC_UNREACHABLE_ERROR    = _cc_unreachable_error,
    CC_UNIMPLEMENTED_ERROR  = _cc_unimplemented_error,
    CC_FILE_NOT_FOUND_ERROR = _cc_file_not_found_error,
    CC_VALUE_ERROR          = _cc_invalid_value_error,
    CC_OVERFLOW_ERROR       = _cc_overflow_error,
    CC_NOT_CONSTANT_ERROR   = _cc_not_constant_error,
};

struct CcStmtSink {
    CcStmtSink*_Null_unspecified prev;
    Parray(CcStmtNode) stmts;
};

#ifdef __clang__
#pragma clang assume_nonnull end
#endif
#ifndef MARRAY_CCQUALTYPE
#define MARRAY_CCQUALTYPE
#define MARRAY_T CcQualType
#include "../Drp/Marray.h"
#endif
#ifndef MARRAY_CCFIELD
#define MARRAY_CCFIELD
#define MARRAY_T CcField
#include "../Drp/Marray.h"
#endif
#ifndef MARRAY_CCINITENTRY
#define MARRAY_CCINITENTRY
#define MARRAY_T CcInitEntry
#include "../Drp/Marray.h"
#endif
#ifndef MARRAY_CCSWITCHENTRY
#define MARRAY_CCSWITCHENTRY
#define MARRAY_T CcSwitchEntry
#include "../Drp/Marray.h"
#endif
#ifdef __clang__
#pragma clang assume_nonnull begin
#endif

static int cc_parse_init(CcParser* p, CcValueClass vc, CcQualType target, _Bool braced, SrcLoc loc, Marray(CcInitEntry)* buf, uint32_t*_Nullable out_max_index, CcExpr*_Nullable first_value, Marray(uint32_t)* path);

typedef struct CcSpecifier CcSpecifier;
struct CcSpecifier {
    union {
        uint32_t bits;
        struct {
            uint32_t sp_typebits: 9,
                     sp_storagebits: 6,
                     _sp_typedef: 1,
                     sp_funcbits: 2,
                     sp_qualbits: 4,

                     _sp_infer: 1,
                     _padding1: 32-23;
        };
        struct {
            uint32_t
                     sp___auto_type:  1,
                     sp_unsigned:     1,
                     sp_signed:       1,
                     sp_long:         2,
                     sp_short:        1,
                     sp_int:          1,
                     sp_char:         1,
                     sp_int128:       1,

                     sp_auto:         1,
                     sp_constexpr:    1,
                     sp_extern:       1,
                     sp_register:     1,
                     sp_static:       1,
                     sp_thread_local: 1,
                     sp_typedef:      1,

                     sp_inline:       1,
                     sp_noreturn:     1,

                     sp_const:        1,
                     sp_volatile:     1,
                     sp_atomic:       1,
                     sp_restrict:     1,

                     sp_infer_type:    1,

                     _padding2:       32-23;
        };
    };
};
_Static_assert(sizeof(CcSpecifier) == sizeof(uint32_t), "");
static int cc_parse_declaration_specifier(CcParser* p, CcDeclBase* base);
static int cc_parse_enum(CcParser* p, SrcLoc loc, CcQualType* base_type);

typedef struct CcDeclBase CcDeclBase;
struct CcDeclBase {
    CcQualType type;
    CcSpecifier spec;
    SrcLoc loc;
    uint16_t alignment; // from _Alignas or prefix __attribute__((aligned))
};

static int cc_resolve_specifiers(CcParser* p, CcDeclBase* declbase);
static int cc_parse_decls(CcParser* p, const CcDeclBase* declbase);
static int cc_parse_statement(CcParser* p, CcStmtNode*_Nullable*_Nonnull out);
static int cc_parse_one(CcParser* p);
static int cc_parse_one_inner(CcParser* p);
static int cc_skip_braced_block(CcParser* p);

static
int
cc_push_scope(CcParser* p){
    CcScope* s = fl_pop(&p->scratch_scopes);
    if(s) cc_scope_clear(s);
    else {
        s = Allocator_zalloc(cc_allocator(p), sizeof *s);
        if(!s) return 1;
    }
    s->parent = p->current;
    p->current = s;
    return 0;
}

static
void
cc_pop_scope(CcParser* p){
    CcScope* s = p->current;
    p->current = s->parent;
    fl_push(&p->scratch_scopes, s);
}

static
CcQualType
cc_arithmetic_operand_type(CcParser* p, CcExpr* e){
    CcQualType type = e->type;
    while(ccqt_kind(type) == CC_ENUM) type = ccqt_as_enum(type)->underlying;
    // Array qualifiers also qualify their elements; preserve them until decay.
    if(!ccqt_is_basic(type) || !ccbt_is_arithmetic(type.basic.kind)) return e->type;
    uint32_t width = cc_expr_field_bit_width(e);
    if(width && ccqt_is_integer(type)
        && ccbt_int_rank(type.basic.kind) <= ccbt_int_rank(CCBT_int)
        && (!ccqt_is_unsigned(type, !cc_target(p)->char_is_signed)
            || width < cc_target(p)->sizeof_[CCBT_int]*8))
        return ccqt_basic(CCBT_int);
    return (CcQualType){.unqual=e->type.unqual};
}

static
int
cc_integer_promote(CcParser* p, CcQualType t, CcQualType* out, SrcLoc loc){
    if(!ccqt_is_basic(t) && ccqt_kind(t) == CC_ENUM)
        t = ccqt_as_enum(t)->underlying;
    t = (CcQualType){.unqual=t.unqual};
    if(!ccqt_is_basic(t))
        return cc_error(p, loc, "integer promotion requires arithmetic type");
    CcBasicTypeKind k = t.basic.kind;
    if(k == CCBT_void)
        return cc_error(p, loc, "integer promotion of void");
    if(ccbt_is_float(k)){
        *out = t;
        return 0;
    }
    if(!ccbt_is_integer(k))
        return cc_error(p, loc, "integer promotion requires integer type");
    if(ccbt_int_rank(k) < ccbt_int_rank(CCBT_int))
        *out = ccqt_basic(CCBT_int);
    else
        *out = t;
    return 0;
}

static
int
cc_usual_arithmetic(CcParser* p, CcExpr* lhs, CcExpr* rhs, CcQualType* out, SrcLoc loc){
    CcQualType a = cc_arithmetic_operand_type(p, lhs);
    CcQualType b = cc_arithmetic_operand_type(p, rhs);
    if(ccqt_kind(a) == CC_ENUM)
        a = ccqt_as_enum(a)->underlying;
    if(ccqt_kind(b) == CC_ENUM)
        b = ccqt_as_enum(b)->underlying;
    if(!ccqt_is_basic(a) || !ccqt_is_basic(b))
        return cc_error(p, loc, "usual arithmetic conversions require arithmetic types");
    CcBasicTypeKind ak = a.basic.kind, bk = b.basic.kind;
    if(ak == CCBT_void || bk == CCBT_void)
        return cc_error(p, loc, "usual arithmetic conversions on void");
    if(ak == CCBT_float128 || bk == CCBT_float128){
        *out = ccqt_basic(CCBT_float128); return 0;
    }
    if(ak == CCBT_long_double || bk == CCBT_long_double){
        *out = ccqt_basic(CCBT_long_double); return 0;
    }
    if(ak == CCBT_double || bk == CCBT_double){
        *out = ccqt_basic(CCBT_double); return 0;
    }
    if(ak == CCBT_float || bk == CCBT_float){
        *out = ccqt_basic(CCBT_float); return 0;
    }
    CcQualType ap, bp;
    int err = cc_integer_promote(p, a, &ap, loc);
    if(err) return err;
    err = cc_integer_promote(p, b, &bp, loc);
    if(err) return err;
    ak = ap.basic.kind;
    bk = bp.basic.kind;
    if(ak == bk){ *out = ap; return 0; }
    _Bool char_is_unsigned = !cc_target(p)->char_is_signed;
    _Bool a_unsigned = ccbt_is_unsigned(ak, char_is_unsigned);
    _Bool b_unsigned = ccbt_is_unsigned(bk, char_is_unsigned);
    if(a_unsigned == b_unsigned){
        *out = ccbt_int_rank(ak) >= ccbt_int_rank(bk) ? ap : bp;
        return 0;
    }
    CcBasicTypeKind u = a_unsigned ? ak : bk;
    CcBasicTypeKind s = a_unsigned ? bk : ak;
    if(ccbt_int_rank(u) >= ccbt_int_rank(s)){
        *out = ccqt_basic(u);
        return 0;
    }
    if(cc_target(p)->sizeof_[s] > cc_target(p)->sizeof_[u]){
        *out = ccqt_basic(s);
        return 0;
    }
    *out = ccqt_basic(ccbt_to_unsigned(s));
    return 0;
}

static
int
cc_check_atomic_object_access(CcParser* p, CcQualType type, SrcLoc loc){
    if(!type.is_atomic) return 0;
    uint32_t sz;
    int err = cc_sizeof_as_uint(p, type, loc, &sz);
    if(err) return err;
    if(!sz || (sz & (sz - 1)))
        return cc_error(p, loc, "atomic operand size %u is not a power of 2", sz);
    if(sz > cc_target(p)->atomic_lock_free_max)
        return cc_error(p, loc, "atomic operand size %u exceeds target's maximum lock-free size %u", sz, cc_target(p)->atomic_lock_free_max);
    switch(sz){
        case 1: case 2: case 4: case 8: case 16:
            return 0;
        default:
            return cc_error(p, loc, "unsupported atomic operand size %u", sz);
    }
}

static
int
cc_check_atomic_rmw(CcParser* p, CcQualType type, SrcLoc loc){
    if(!type.is_atomic) return 0;
    CcQualType base = type;
    base.is_atomic = 0;
    if(!ccqt_is_basic(base) && ccqt_kind(base) == CC_ENUM)
        base = ccqt_as_enum(base)->underlying;
    if((!ccqt_is_basic(base) || (!ccbt_is_integer(base.basic.kind) && !ccbt_is_float(base.basic.kind))) && ccqt_kind(base) != CC_POINTER)
        return cc_error(p, loc, "atomic read-modify-write requires scalar atomic type");
    return cc_check_atomic_object_access(p, type, loc);
}

static
int
cc_check_atomic_type_specifier_type(CcParser* p, CcQualType type, SrcLoc loc){
    CcTypeKind kind = ccqt_kind(type);
    if(kind == CC_ARRAY)
        return cc_error(p, loc, "_Atomic type name shall not refer to an array type");
    if(kind == CC_FUNCTION)
        return cc_error(p, loc, "_Atomic type name shall not refer to a function type");
    if(type.is_atomic)
        return cc_error(p, loc, "_Atomic type name shall not refer to an atomic type");
    if(type.is_const || type.is_volatile)
        return cc_error(p, loc, "_Atomic type name shall not refer to a qualified type");
    return 0;
}

static
int
cc_check_atomic_memory_order(CcParser* p, CcAtomicOp op, unsigned order, SrcLoc loc, _Bool failure_order){
    if(order >= CC_MO_COUNT)
        return cc_error(p, loc, "invalid memory order value %u", order);
    if(failure_order){
        if(order == CC_MO_RELEASE || order == CC_MO_ACQ_REL)
            return cc_error(p, loc, "atomic compare-exchange failure order cannot be release or acq_rel");
        return 0;
    }
    if(op == CC_ATOMIC_LOAD || op == CC_ATOMIC_LOAD_N){
        if(order == CC_MO_RELEASE || order == CC_MO_ACQ_REL)
            return cc_error(p, loc, "atomic load memory order cannot be release or acq_rel");
    }
    if(op == CC_ATOMIC_STORE || op == CC_ATOMIC_STORE_N){
        if(order == CC_MO_CONSUME || order == CC_MO_ACQUIRE || order == CC_MO_ACQ_REL)
            return cc_error(p, loc, "atomic store memory order cannot be consume, acquire, or acq_rel");
    }
    return 0;
}

static
CcQualType
cc_array_element_type(CcQualType array){
    CcQualType element = ccqt_as_array(array)->element;
    element.quals |= array.quals;
    return element;
}

static
int
cc_deref_type(CcParser* p, CcQualType t, CcQualType* out, SrcLoc loc, _Bool subscript){
    if(!ccqt_is_basic(t)){
        CcTypeKind kind = ccqt_kind(t);
        if(kind == CC_POINTER){
            *out = ccqt_as_ptr(t)->pointee;
            return 0;
        }
        if(kind == CC_ARRAY && (subscript || !ccqt_as_array(t)->is_vector)){
            *out = ccqt_as_array(t)->element;
            out->quals |= t.quals;
            return 0;
        }
        if(kind == CC_SLICE){
            *out = ccqt_as_slice(t)->pointee;
            return 0;
        }
    }
    return cc_error(p, loc, "dereferencing non-pointer type");
}

static
int
cc_check_pointer_arithmetic(CcParser* p, CcQualType type, SrcLoc loc){
    CcQualType element;
    int err = cc_deref_type(p, type, &element, loc, 1);
    if(err) return err;
    while(ccqt_kind(element) == CC_ARRAY){
        CcArray* array = ccqt_as_array(element);
        if(array->is_incomplete)
            return cc_error(p, loc, "pointer arithmetic requires a complete pointee type");
        element = array->element;
    }
    CcTypeKind kind = ccqt_kind(element);
    if(((kind == CC_STRUCT || kind == CC_UNION) && ccqt_as_struct(element)->is_incomplete)
        || (kind == CC_ENUM && ccqt_as_enum(element)->is_incomplete))
        return cc_error(p, loc, "pointer arithmetic requires a complete pointee type");
    return 0;
}

static
int
cc_check_incdec_type(CcParser* p, CcQualType type, SrcLoc loc){
    CcQualType value = type;
    if(ccqt_kind(value) == CC_ENUM) value = ccqt_as_enum(value)->underlying;
    if(ccqt_kind(value) == CC_POINTER){
        int err = cc_check_pointer_arithmetic(p, value, loc);
        if(err) return err;
    }
    else if(!ccqt_is_basic(value) || !ccbt_is_arithmetic(value.basic.kind))
        return cc_error(p, loc, "increment/decrement requires arithmetic or pointer type");
    return cc_check_atomic_rmw(p, type, loc);
}

static
_Bool
cc_any_payload_type(CcParser* p, CcQualType t){
    switch(ccqt_kind(t)){
        case CC_BASIC:
            if(t.basic.kind == CCBT_INVALID || t.basic.kind == CCBT_void) return 0;
            return cc_target(p)->sizeof_[t.basic.kind] <= sizeof(((CiRtAny*)0)->payload);
        case CC_POINTER: case CC_BLOCK_POINTER:
            return 1;
        case CC_STRUCT:
            return !ccqt_as_struct(t)->is_incomplete && ccqt_as_struct(t)->size <= sizeof(((CiRtAny*)0)->payload);
        case CC_UNION:
            return !ccqt_as_union(t)->is_incomplete && ccqt_as_union(t)->size <= sizeof(((CiRtAny*)0)->payload);
        case CC_ENUM:
            return !ccqt_as_enum(t)->is_incomplete && cc_any_payload_type(p, ccqt_as_enum(t)->underlying);
        case CC_ARRAY: {
            CcArray* a = ccqt_as_array(t);
            return a->is_vector && a->vector_size <= sizeof ((CiRtAny*)0)->payload;
        }
        case CC_FUNCTION: case CC_SLICE:
            return 0;
    }
    return 0;
}

static
_Bool
cc_any_convertible(CcParser* p, CcQualType from){
    CcTypeKind k = ccqt_kind(from);
    if(k == CC_FUNCTION) return 1;
    if(k == CC_ARRAY){
        CcArray* a = ccqt_as_array(from);
        if(!a->is_vector) return 1;
        return a->vector_size <= sizeof ((CiRtAny*)0)->payload;
    }
    return cc_any_payload_type(p, from);
}

static
_Bool
cc_plan9_base(CcQualType from, CcQualType to){
    CcTypeKind fk = ccqt_kind(from), tk = ccqt_kind(to);
    if((fk != CC_STRUCT && fk != CC_UNION) || (tk != CC_STRUCT && tk != CC_UNION)) return 0;
    if(from.unqual == to.unqual){
        if(from.quals & ~to.quals)
            return 0;
        return 1;
    }
    CcStruct* s = ccqt_as_struct(from);
    for(uint32_t i = 0; i < s->field_count; i++){
        CcField* f = &s->fields[i];
        if(f->is_method || f->name || f->is_bitfield) continue;
        CcQualType sub = f->type;
        sub.quals |= from.quals;
        if(cc_plan9_base(sub, to)){
            return 1;
        }
    }
    return 0;
}

static
int
cc_member_path(CcParser* p, CcField* fields, uint32_t count, Atom name, _Bool owner, CcFieldPath* out){
    CcQualType type;
    CcField* field;
    CcFieldPath path;
    int err = cc_lookup_field(p, fields, count, name, &path, &type, NULL, &field);
    if(err) return err;
    if(!field) return CC_NOT_CONSTANT_ERROR;
    if(owner && field->is_method){
        err = cc_field_path_take(cc_allocator(p), path, cc_field_path_count(path)-1, out);
        cc_field_path_free(cc_allocator(p), path);
        return err ? CC_OOM_ERROR : 0;
    }
    *out = path;
    return 0;
}

static
int
cc_base_path(CcParser* p, CcQualType from, CcQualType to, CcFieldPath* out){
    if(from.unqual == to.unqual){ *out = (CcFieldPath){0}; return 0; }
    CcStruct* s = ccqt_as_struct(from);
    for(uint32_t i = 0; i < s->field_count; i++){
        CcField* field = &s->fields[i];
        if(field->is_method || field->name || field->is_bitfield) continue;
        CcQualType sub = field->type;
        sub.quals |= from.quals;
        if(!cc_plan9_base(sub, to)) continue;
        CcFieldPath suffix;
        int err = cc_base_path(p, sub, to, &suffix);
        if(err) return err;
        CcFieldPath prefix = {0};
        err = cc_field_path_make(cc_allocator(p), &i, 1, &prefix);
        if(!err) err = cc_field_path_concat(cc_allocator(p), prefix, suffix, out);
        cc_field_path_free(cc_allocator(p), prefix);
        cc_field_path_free(cc_allocator(p), suffix);
        return err ? CC_OOM_ERROR : 0;
    }
    return CC_NOT_CONSTANT_ERROR;
}

static
int
cc_set_member_path(CcParser* p, CcExpr* e, CcQualType owner_type, Atom name, _Bool owner, uint32_t builtin_index){
    if(ccqt_kind(owner_type) == CC_STRUCT || ccqt_kind(owner_type) == CC_UNION){
        CcStruct* s = ccqt_as_struct(owner_type);
        return cc_member_path(p, s->fields, s->field_count, name, owner, &e->field_path);
    }
    e->field_path = (CcFieldPath){.n_components=1, .idx0=builtin_index};
    return 0;
}

static
_Bool
cc_types_compatible(CcQualType a, CcQualType b, unsigned depth){
    if(a.bits == b.bits) return 1;
    if(depth >= 256 || a.quals != b.quals || ccqt_kind(a) != ccqt_kind(b)) return 0;
    switch(ccqt_kind(a)){
        case CC_POINTER: case CC_BLOCK_POINTER:
            return ccqt_as_ptr(a)->restrict_ == ccqt_as_ptr(b)->restrict_
                && cc_types_compatible(ccqt_as_ptr(a)->pointee, ccqt_as_ptr(b)->pointee, depth+1);
        case CC_SLICE:
            return ccqt_as_slice(a)->restrict_ == ccqt_as_slice(b)->restrict_
                && cc_types_compatible(ccqt_as_slice(a)->pointee, ccqt_as_slice(b)->pointee, depth+1);
        case CC_ARRAY: {
            CcArray* x = ccqt_as_array(a);
            CcArray* y = ccqt_as_array(b);
            return x->is_vector == y->is_vector && x->vector_size == y->vector_size
                && (x->is_incomplete || y->is_incomplete || x->is_vla || y->is_vla || x->length == y->length)
                && cc_types_compatible(x->element, y->element, depth+1);
        }
        case CC_FUNCTION: {
            CcFunction* x = ccqt_as_function(a);
            CcFunction* y = ccqt_as_function(b);
            if(!cc_types_compatible(x->return_type, y->return_type, depth+1)) return 0;
            if(x->no_prototype || y->no_prototype){
                CcFunction* proto = x->no_prototype ? y : x;
                if(proto->is_variadic) return 0;
                for(uint32_t i = 0; i < proto->param_count; i++){
                    CcQualType t = proto->params[i];
                    if(ccqt_kind(t) == CC_ENUM) t = ccqt_as_enum(t)->underlying;
                    if(ccqt_is_basic(t) && (t.basic.kind == CCBT_float
                        || (ccbt_is_integer(t.basic.kind) && ccbt_int_rank(t.basic.kind) < ccbt_int_rank(CCBT_int))))
                        return 0;
                }
                return 1;
            }
            if(x->param_count != y->param_count || x->is_variadic != y->is_variadic) return 0;
            for(uint32_t i = 0; i < x->param_count; i++){
                CcQualType at = {.unqual=x->params[i].unqual};
                CcQualType bt = {.unqual=y->params[i].unqual};
                if(!cc_types_compatible(at, bt, depth+1)) return 0;
            }
            return 1;
        }
        case CC_BASIC: case CC_STRUCT: case CC_UNION: case CC_ENUM: return 0;
        DRP_CASES_EXHAUSTED;
    }
}

static
_Bool
cc_pointer_pointee_convertible(CcQualType from, CcQualType to){
    if(cc_types_compatible((CcQualType){.unqual=from.unqual}, (CcQualType){.unqual=to.unqual}, 0)
        || ccqt_bt_eq(from, CCBT_void) || ccqt_bt_eq(to, CCBT_void))
        return (from.quals & ~to.quals) == 0;
    return cc_plan9_base(from, to);
}

static
_Bool
cc_implicit_convertible(CcParser* p, CcQualType from, CcQualType to){
    if(from.bits == to.bits) return 1;
    if(ccqt_bt_eq(to, CCBT__Any))
        return ccqt_bt_eq(from, CCBT__Any) || cc_any_convertible(p, from);
    CcTypeKind fk = ccqt_kind(from), tk = ccqt_kind(to);
    if(fk == CC_BASIC && tk == CC_BASIC && from.basic.kind == to.basic.kind) return 1;
    _Bool f_arith = (fk == CC_BASIC && ccbt_is_arithmetic(from.basic.kind)) || fk == CC_ENUM;
    _Bool t_arith = (tk == CC_BASIC && ccbt_is_arithmetic(to.basic.kind)) || tk == CC_ENUM;
    if(f_arith && t_arith) return 1;
    if(fk == CC_STRUCT && tk == CC_STRUCT) return from.unqual == to.unqual;
    if(fk == CC_UNION && tk == CC_UNION) return from.unqual == to.unqual;
    if(fk == CC_POINTER && tk == CC_POINTER){
        CcQualType fp = ccqt_as_ptr(from)->pointee;
        CcQualType tp = ccqt_as_ptr(to)->pointee;
        return cc_pointer_pointee_convertible(fp, tp);
    }
    if(fk == CC_SLICE && tk == CC_SLICE){
        CcQualType fp = ccqt_as_slice(from)->pointee;
        CcQualType tp = ccqt_as_slice(to)->pointee;
        if(fp.unqual == tp.unqual)
            return (fp.quals & ~tp.quals) == 0;
        return 0;
    }
    // Complete array decays to a slice of its element type, like x[:].
    if(fk == CC_ARRAY && tk == CC_SLICE){
        CcArray* a = ccqt_as_array(from);
        if(a->is_incomplete || a->is_vector) return 0;
        CcQualType ep = a->element;
        ep.quals |= from.quals;
        CcQualType tp = ccqt_as_slice(to)->pointee;
        if(ep.unqual != tp.unqual) return 0;
        return (ep.quals & ~tp.quals) == 0;
    }
    if(fk == CC_ARRAY && tk == CC_POINTER && !ccqt_as_array(from)->is_vector){
        CcQualType ep = ccqt_as_array(from)->element;
        ep.quals |= from.quals;
        CcQualType tp = ccqt_as_ptr(to)->pointee;
        return cc_pointer_pointee_convertible(ep, tp);
    }
    if(fk == CC_FUNCTION && tk == CC_POINTER)
        return cc_pointer_pointee_convertible(from, ccqt_as_ptr(to)->pointee);
    if(fk == CC_BASIC && from.basic.kind == CCBT_nullptr_t && tk == CC_POINTER) return 1;
    if(fk == CC_BASIC && from.basic.kind == CCBT_nullptr_t && tk == CC_BASIC && to.basic.kind == CCBT_nullptr_t) return 1;
    if(tk == CC_BASIC && to.basic.kind == CCBT_bool){
        if(fk == CC_POINTER) return 1;
        if(fk == CC_ARRAY && !ccqt_as_array(from)->is_vector) return 1;
        if(fk == CC_FUNCTION) return 1;
        if(fk == CC_BASIC && from.basic.kind == CCBT_nullptr_t) return 1;
    }
    if(fk == CC_ARRAY && tk == CC_ARRAY && ccqt_as_array(from)->is_vector && ccqt_as_array(to)->is_vector)
        return from.unqual == to.unqual;
    return 0;
}

static
_Bool
cc_explicit_castable(CcParser* p, CcQualType from, CcQualType to){
    if(ccqt_bt_eq(to, CCBT__Any))
        return ccqt_bt_eq(from, CCBT__Any) || cc_any_convertible(p, from);
    return cc_check_cast(0, from, to, (SrcLoc){0}) == 0;
}

// Compare ABI representations, not C assignment compatibility. In particular,
// do not infer aggregate register classification from size and alignment alone.
static
_Bool
cc_call_abi_type_equal(const CcTargetConfig* target, CcQualType a, CcQualType b){
    while(ccqt_kind(a) == CC_ENUM) a = ccqt_as_enum(a)->underlying;
    while(ccqt_kind(b) == CC_ENUM) b = ccqt_as_enum(b)->underlying;
    if(a.unqual == b.unqual) return 1;
    // All supported targets use the same representation for pointer types.
    if(ccqt_kind(a) == CC_POINTER) a = ccqt_basic(CCBT_nullptr_t);
    if(ccqt_kind(b) == CC_POINTER) b = ccqt_basic(CCBT_nullptr_t);
    if(a.unqual == b.unqual) return 1;
    if(!ccqt_is_basic(a) || !ccqt_is_basic(b)) return 0;
    CcBasicTypeKind ak = a.basic.kind, bk = b.basic.kind;
    if(ak == CCBT_bool || bk == CCBT_bool) return 0;
    if((ccbt_is_integer(ak) || ak == CCBT_nullptr_t)
        && (ccbt_is_integer(bk) || bk == CCBT_nullptr_t)){
        unsigned size = target->sizeof_[ak];
        if(size != target->sizeof_[bk]) return 0;
        // Narrow arguments/returns can require sign or zero extension.
        return size >= target->sizeof_[CCBT_int]
            || ccbt_is_unsigned(ak, !target->char_is_signed)
                == ccbt_is_unsigned(bk, !target->char_is_signed);
    }
    // Long double is an alias for double on some targets, binary128 on others.
    if(target->long_double_format == CC_LONG_DOUBLE_BINARY64){
        if(ak == CCBT_long_double) ak = CCBT_double;
        if(bk == CCBT_long_double) bk = CCBT_double;
        if(ak == CCBT_long_double_complex) ak = CCBT_double_complex;
        if(bk == CCBT_long_double_complex) bk = CCBT_double_complex;
    }
    else if(target->long_double_format == CC_LONG_DOUBLE_BINARY128){
        if(ak == CCBT_long_double) ak = CCBT_float128;
        if(bk == CCBT_long_double) bk = CCBT_float128;
    }
    return ak == bk;
}

static
_Bool
cc_is_callable_through(CcParser* p, CcQualType from, CcQualType through){
    if(ccqt_kind(from) == CC_POINTER) from = ccqt_as_ptr(from)->pointee;
    if(ccqt_kind(from) != CC_FUNCTION || ccqt_kind(through) != CC_FUNCTION) return 0;
    CcFunction* f = ccqt_as_function(from);
    CcFunction* t = ccqt_as_function(through);
    if(f == t) return 1;
    if(f->no_prototype || t->no_prototype
        || f->is_variadic != t->is_variadic || f->param_count != t->param_count) return 0;
    const CcTargetConfig* target = cc_target(p);
    if(!cc_call_abi_type_equal(target, f->return_type, t->return_type)) return 0;
    for(uint32_t i = 0; i < f->param_count; i++)
        if(!cc_call_abi_type_equal(target, f->params[i], t->params[i])) return 0;
    return 1;
}

static
int
cc_is_null_pointer_constant(CcParser* p, CcExpr* e, _Bool* out){
    *out = ccqt_bt_eq(e->type, CCBT_nullptr_t);
    if(*out || !ccqt_is_integer(e->type)) return 0;
    int64_t value;
    int err = cc_eval_integer(&(CcEvalCtx){.parser=p}, e, &value);
    if(err == CC_OOM_ERROR) return err;
    *out = !err && value == 0;
    return 0;
}

static
int
cc_implicit_cast(CcParser* p, CcExpr* e, CcQualType target, CcExpr* _Nullable* _Nonnull out){
    if(ccqt_bt_eq(target, CCBT_void)){
        *out = e;
        return 0;
    }
    if(e->type.bits == target.bits){
        *out = e;
        return 0;
    }
    _Bool is_null_pointer_constant = 0;
    if(ccqt_kind(target) == CC_POINTER
           || ccqt_bt_eq(target, CCBT_nullptr_t)
           || ccqt_kind(target) == CC_BLOCK_POINTER
           || ccqt_bt_eq(target, CCBT__Type)){
        int err = cc_is_null_pointer_constant(p, e, &is_null_pointer_constant);
        if(err) return err;
    }
    if(!is_null_pointer_constant && !cc_implicit_convertible(p, e->type, target)){
        MStringBuilder* sb = cc_start_error(p, e->loc, "cannot implicitly convert from '");
        cc_print_type(sb, e->type);
        msb_write_literal(sb, "' to '");
        cc_print_type(sb, target);
        msb_write_char(sb, '\'');
        return cc_finish_error(p, e->loc);
    }
    if(ccqt_kind(target) == CC_SLICE && ccqt_kind(e->type) == CC_ARRAY && e->kind == CC_EXPR_VALUE && e->text && e->str.length){
        CcQualType pointer;
        int err = cc_pointer_of(p, ccqt_as_slice(target)->pointee, &pointer);
        if(err) return err;
        CcInitList* il = Allocator_zalloc(cc_allocator(p), sizeof *il + 2 * sizeof(CcInitEntry));
        if(!il) return CC_OOM_ERROR;
        CcExpr* count = cc_uint64_expr(p, e->loc, ccqt_basic(cc_target(p)->size_type), e->str.length - 1);
        CcExpr* data = cc_make_expr(p, CC_EXPR_CAST, e->loc, pointer, 0);
        CcExpr* slice = cc_make_expr(p, CC_EXPR_INIT_LIST, e->loc, target, 0);
        if(!count || !data || !slice){
            if(count) cc_release_expr(p, count);
            if(data) _cc_release_expr(p, data, 0);
            if(slice) _cc_release_expr(p, slice, 0);
            Allocator_free(cc_allocator(p), il, sizeof *il + 2 * sizeof(CcInitEntry));
            return CC_OOM_ERROR;
        }
        data->lhs = e;
        il->loc = e->loc;
        il->count = 2;
        il->entries[0].path = (CcFieldPath){.n_components=1, .idx0=0};
        il->entries[0].value = count;
        il->entries[1].path = (CcFieldPath){.n_components=1, .idx0=1};
        il->entries[1].value = data;
        slice->init_list = il;
        *out = slice;
        return 0;
    }
    if(e->kind == CC_EXPR_COMPOUND_LITERAL){
        int err = cc_desugar_compound_literal(p, e, &e);
        if(err) return err;
    }
    if(ccqt_bt_eq(target, CCBT__Any) && !ccqt_bt_eq(e->type, CCBT__Any)){
        CcTypeKind k = ccqt_kind(e->type);
        if((k == CC_ARRAY && !ccqt_as_array(e->type)->is_vector) || k == CC_FUNCTION){
            CcQualType ptr;
            int err = cc_pointer_of(p, e->type, &ptr);
            if(err) return err;
            CcExpr* addr = cc_unary_expr(p, k == CC_FUNCTION ? CC_EXPR_CAST : CC_EXPR_ADDR, e->loc, ptr, e);
            if(!addr) return CC_OOM_ERROR;
            e = addr;
        }
    }
    if(ccqt_kind(e->type) == CC_ARRAY && !ccqt_as_array(e->type)->is_vector && ccqt_kind(target) == CC_POINTER){
        CcQualType element = cc_array_element_type(e->type);
        CcQualType to = ccqt_as_ptr(target)->pointee;
        if(element.unqual != to.unqual && cc_plan9_base(element, to)){
            CcQualType decay;
            int err = cc_pointer_of(p, element, &decay);
            if(err) return err;
            CcExpr* cast = cc_unary_expr(p, CC_EXPR_CAST, e->loc, decay, e);
            if(!cast) return CC_OOM_ERROR;
            e = cast;
        }
    }
    if(ccqt_kind(e->type) == CC_POINTER && ccqt_kind(target) == CC_POINTER){
        CcQualType from = ccqt_as_ptr(e->type)->pointee;
        CcQualType to = ccqt_as_ptr(target)->pointee;
        if(from.ptr != to.ptr && cc_plan9_base(from, to)){
            CcExpr* member = cc_make_expr(p, CC_EXPR_ARROW, e->loc, to, 1);
            if(!member) return CC_OOM_ERROR;
            member->is_lvalue = 1;
            int err = cc_base_path(p, from, to, &member->field_path);
            if(err){ _cc_release_expr(p, member, 1); return err; }
            member->values[0] = e;
            CcExpr* addr = cc_unary_expr(p, CC_EXPR_ADDR, e->loc, target, member);
            if(!addr){
                cc_field_path_free(cc_allocator(p), member->field_path);
                _cc_release_expr(p, member, 1);
                return CC_OOM_ERROR;
            }
            *out = addr;
            return 0;
        }
    }
    CcExpr* cast = cc_make_expr(p, CC_EXPR_CAST, e->loc, target, 0);
    if(!cast) return CC_OOM_ERROR;
    cast->lhs = e;
    *out = cast;
    return 0;
}

static
int
cc_require_scalar(CcParser* p, CcExpr*_Nullable*_Nonnull expr, SrcLoc loc, const char* context){
    CcExpr* e = *expr;
    CcTypeKind k = ccqt_kind(e->type);
    // Scalar contexts use array/function decay, not the object's contents.
    if((k == CC_ARRAY && !ccqt_as_array(e->type)->is_vector) || k == CC_FUNCTION){
        CcQualType pointer;
        CcQualType pointee = k == CC_ARRAY ? cc_array_element_type(e->type) : e->type;
        int err = cc_pointer_of(p, pointee, &pointer);
        if(err) return err;
        return cc_implicit_cast(p, e, pointer, expr);
    }
    if(k == CC_POINTER || k == CC_ENUM) return 0;
    if(ccqt_is_basic(e->type)
        && (ccbt_is_arithmetic(e->type.basic.kind) || ccqt_bt_eq(e->type, CCBT_nullptr_t)))
        return 0;
    return cc_error(p, loc, "%s requires scalar type", context);
}

static
int
cc_implicit_cast_to_index(CcParser* p, CcExpr* e, CcExpr* _Nullable* _Nonnull out){
    CcQualType src = cc_arithmetic_operand_type(p, e);
    if(ccqt_kind(src) == CC_ENUM)
        src = ccqt_as_enum(src)->underlying;
    if(!ccqt_is_basic(src) || !ccbt_is_integer(src.basic.kind))
        return cc_error(p, e->loc, "index requires integer type");
    const CcTargetConfig* t = cc_target(p);
    CcQualType target = ccqt_basic(ccbt_is_unsigned(src.basic.kind, !t->char_is_signed)?  t->size_type : ccbt_to_signed(t->size_type));
    return cc_implicit_cast(p, e, target, out);
}

static
_Bool
cc_is_type_start(CcParser* p, CcToken* tok){
    if(tok->type == CC_KEYWORD){
        switch(tok->kw.kw){
            case CC_void: case CC_char: case CC_short: case CC_int:
            case CC_long: case CC_float: case CC_double:
            case CC_signed: case CC_unsigned: case CC_bool:
            case CC___int128:
            case CC_struct: case CC_union: case CC_enum:
            case CC_typeof: case CC_typeof_unqual:
            case CC___auto_type:
            case CC__Complex: case CC__Imaginary:
            case CC__Float16: case CC__Float32: case CC__Float64: case CC__Float128: case CC__Float32x: case CC__Float64x:
            case CC__Decimal32: case CC__Decimal64: case CC__Decimal128:
            case CC__BitInt:
            case CC__Atomic:
            case CC_const: case CC_volatile: case CC_restrict:
            case CC__Any:
            case CC__Type:
            case CC__Self:
                return 1;
            case CC_do:
            case CC_if:
            case CC_for:
            case CC_asm:
            case CC_true:
            case CC_auto:
            case CC_else:
            case CC_case:
            case CC_goto:
            case CC_false:
            case CC_break:
            case CC_while:
            case CC_extern:
            case CC_inline:
            case CC_return:
            case CC_sizeof:
            case CC_static:
            case CC_switch:
            case CC_alignas:
            case CC_alignof:
            case CC_default:
            case CC_typedef:
            case CC_nullptr:
            case CC_continue:
            case CC_register:
            case CC__Generic:
            case CC_constexpr:
            case CC__Noreturn:
            case CC_thread_local:
            case CC_static_assert:
            case CC__Countof:
            case CC___attribute__:
            case CC___declspec:
                return 0;
        }
    }
    if(tok->type == CC_IDENTIFIER){
        CcSymbol sym;
        return cc_scope_lookup_symbol(p->current, tok->ident.ident, CC_SCOPE_WALK_CHAIN, &sym) && sym.kind == CC_SYM_TYPEDEF;
    }
    return 0;
}

static
int
cc_parse_type_name(CcParser* p, CcQualType* out, CcParsedParams* _Nullable param_names){
    CcDeclBase base = {0};
    int err = cc_parse_declaration_specifier(p, &base);
    if(err) return err;
    if(!base.spec.bits && !base.type.bits){
        CcToken peek;
        err = cc_peek(p, &peek);
        if(err) return err;
        return cc_error(p, peek.loc, "Expected type name");
    }
    err = cc_resolve_specifiers(p, &base);
    if(err) return err;
    if(base.spec.sp_infer_type)
        return cc_error(p, base.loc, "Expected type name, got only qualifiers/storage class");
    CcQualType head = {0};
    CcQualType* tail = &head;
    err = cc_parse_declarator(p, &head, &tail, NULL, NULL, param_names);
    if(err) return err;
    *tail = base.type;
    *out = cc_intern_qualtype(p, head);
    return 0;
}

static
int
cc_parse_abstract_type_name(CcParser* p, CcQualType* out){
    CcDeclBase base = {0};
    int err = cc_parse_declaration_specifier(p, &base);
    if(err) return err;
    if(!base.spec.bits && !base.type.bits){
        CcToken peek;
        err = cc_peek(p, &peek);
        if(err) return err;
        return cc_error(p, peek.loc, "Expected type name");
    }
    err = cc_resolve_specifiers(p, &base);
    if(err) return err;
    if(base.spec.sp_infer_type)
        return cc_error(p, base.loc, "Expected type name, got only qualifiers/storage class");
    CcQualType head = {0};
    CcQualType* tail = &head;
    Atom name = NULL;
    err = cc_parse_declarator(p, &head, &tail, &name, NULL, NULL);
    if(err) return err;
    if(name)
        return cc_error(p, base.loc, "unexpected declarator name '%.*s' in type expression", name->length, name->data);
    *tail = base.type;
    *out = cc_intern_qualtype(p, head);
    return 0;
}

static
int
cc_parse_type_string(CcParser* p, CcScope* scope, SrcLoc loc, StringView source, CcQualType* out){
    *out = (CcQualType){0};
    CcScope* old_current = p->current;
    int err = 0;
    p->current = scope;
    cc_parser_discard_input(p);
    CppFrame frame = {
        .txt = source,
        .file_id = loc.is_actually_a_pointer ? ((SrcLocExp*)((uintptr_t)loc.pointer.bits << 1))->file_id : loc.file_id,
        .line = loc.is_actually_a_pointer ? ((SrcLocExp*)((uintptr_t)loc.pointer.bits << 1))->line : loc.line,
        .column = loc.is_actually_a_pointer ? ((SrcLocExp*)((uintptr_t)loc.pointer.bits << 1))->column : loc.column,
    };
    err = ma_push(CppFrame)(&p->cpp.frames, p->cpp.allocator, frame);
    if(err){ err = CC_OOM_ERROR; goto done; }
    err = cc_parse_abstract_type_name(p, out);
    if(err) goto done;
    CcToken tok;
    err = cc_next_token(p, &tok);
    if(err) goto done;
    if(tok.type != CC_EOF)
        err = cc_error(p, tok.loc, "unexpected token after type name");
    done:
    p->current = old_current;
    cc_parser_discard_input(p);
    return err;
}

static
int
cc_parse_lambda_body(CcParser* p, CcValueClass vc, SrcLoc loc, CcQualType type, CcParsedParams* param_names, CcExpr* _Nullable* _Nonnull out){
    int err;
    CcToken tok;
    err = cc_next_token(p, &tok); // consume '{'
    if(err){ ma_cleanup(CcFuncParam)(&param_names->names, cc_allocator(p)); return err; }
    CcFunction* ftype = ccqt_as_function(type);
    CcFunc* func = Allocator_zalloc(cc_allocator(p), sizeof *func);
    if(!func){ ma_cleanup(CcFuncParam)(&param_names->names, cc_allocator(p)); return CC_OOM_ERROR; }
    func->name = NULL;
    func->type = ftype;
    func->loc = loc;
    func->defined = 1;
    func->params.count = param_names->names.count;
    func->params.data = param_names->names.data;
    func->param_scope = param_names->scope;
    func->enclosing = p->current_func;
    err = cc_parse_func_body_inner(p, func, 1);
    if(err) return err;
    err = cc_expect_punct(p, CC_rbrace);
    if(err) return err;
    CcExpr* node = cc_make_expr(p, CC_EXPR_FUNCTION, loc, (CcQualType){.bits=(uintptr_t)func->type}, 0);
    if(!node) return CC_OOM_ERROR;
    node->func = func;
    return cc_parse_postfix(p, vc, node, out);
}

static
int
cc_parse_lambda(CcParser* p, CcValueClass vc, SrcLoc loc, CcExpr* _Nullable* _Nonnull out){
    CcDeclBase base = {0};
    int err = cc_parse_declaration_specifier(p, &base);
    if(err) return err;
    if(!base.spec.bits && !base.type.bits)
        return cc_error(p, loc, "Expected type in lambda expression");
    err = cc_resolve_specifiers(p, &base);
    if(err) return err;
    if(base.spec.sp_infer_type)
        return cc_error(p, loc, "Expected type in expression, got only qualifiers/storage class");
    CcQualType head = {0};
    CcQualType* tail = &head;
    CcParsedParams param_names = {0};
    Atom name = NULL;
    err = cc_parse_declarator(p, &head, &tail, &name, NULL, &param_names);
    if(err){ ma_cleanup(CcFuncParam)(&param_names.names, cc_allocator(p)); return err; }
    if(name){
        ma_cleanup(CcFuncParam)(&param_names.names, cc_allocator(p));
        return cc_error(p, loc, "unexpected declarator name '%.*s' in type expression", name->length, name->data);
    }
    *tail = base.type;
    CcQualType type = cc_intern_qualtype(p, head);
    CcToken peek;
    err = cc_peek(p, &peek);
    if(err){
        ma_cleanup(CcFuncParam)(&param_names.names, cc_allocator(p));
        return err;
    }
    if(ccqt_kind(type) != CC_FUNCTION){
        ma_cleanup(CcFuncParam)(&param_names.names, cc_allocator(p));
        if(peek.type == CC_PUNCTUATOR && peek.punct.punct == CC_lbrace)
            return cc_error(p, loc, "Lambda requires a function type, got non-function type");
    }
    if(peek.type != CC_PUNCTUATOR || peek.punct.punct != CC_lbrace){
        ma_cleanup(CcFuncParam)(&param_names.names, cc_allocator(p));
        CcExpr* type_val = cc_value_expr(p, loc, ccqt_basic(CCBT__Type));
        if(!type_val) return CC_OOM_ERROR;
        type_val->uinteger = type.bits;
        return cc_parse_postfix(p, vc, type_val, out);
    }
    return cc_parse_lambda_body(p, vc, loc, type, &param_names, out);
}

static
int
cc_desugar_compound_literal(CcParser* p, CcExpr* cl, CcExpr*_Nullable*_Nonnull out){
    int err;
    CcQualType type = cl->type;
    SrcLoc loc = cl->loc;
    CcVariable* anon = Allocator_zalloc(cc_allocator(p), sizeof *anon);
    if(!anon) return CC_OOM_ERROR;
    *anon = (CcVariable){
        .name = nil_atom,
        .loc = loc,
        .type = type,
        .automatic = p->current_func != NULL,
        .initializer = cl,
    };
    err = cc_scope_insert_var(cc_allocator(p), p->current, nil_atom, anon);
    if(err) return err;
    cl->kind = CC_EXPR_INIT_LIST; // demote to plain init list for the assignment RHS
    CcExpr* var_ref = cc_make_expr(p, CC_EXPR_VARIABLE, loc, type, 0);
    if(!var_ref) return CC_OOM_ERROR;
    var_ref->is_lvalue = 1;
    var_ref->var = anon;
    CcExpr* assign = cc_binary_expr(p, CC_EXPR_ASSIGN, loc, type, var_ref, cl);
    if(!assign) return CC_OOM_ERROR;
    CcExpr* var_ref2 = cc_make_expr(p, CC_EXPR_VARIABLE, loc, type, 0);
    if(!var_ref2) return CC_OOM_ERROR;
    var_ref2->is_lvalue = 1;
    var_ref2->var = anon;
    CcExpr* comma = cc_binary_expr(p, CC_EXPR_COMMA, loc, type, assign, var_ref2);
    if(!comma) return CC_OOM_ERROR;
    comma->is_lvalue = var_ref2->is_lvalue;
    *out = comma;
    return 0;
}

static
int
cc_wrap_to_desugared_compound_literal(CcParser* p, CcExpr* operand, CcExpr*_Nullable*_Nonnull out){
    CcQualType type = operand->type;
    SrcLoc loc = operand->loc;
    int err;
    CcVariable* anon = Allocator_zalloc(cc_allocator(p), sizeof *anon);
    if(!anon) return CC_OOM_ERROR;
    *anon = (CcVariable){
        .name = nil_atom,
        .loc = loc,
        .type = type,
        .automatic = p->current_func != NULL,
        .initializer = operand,
    };
    err = cc_scope_insert_var(cc_allocator(p), p->current, nil_atom, anon);
    if(err) return err;
    CcExpr* var_ref = cc_make_expr(p, CC_EXPR_VARIABLE, loc, type, 0);
    if(!var_ref) return CC_OOM_ERROR;
    var_ref->is_lvalue = 1;
    var_ref->var = anon;
    CcExpr* assign = cc_binary_expr(p, CC_EXPR_ASSIGN, loc, type, var_ref, operand);
    if(!assign) return CC_OOM_ERROR;
    CcExpr* var_ref2 = cc_make_expr(p, CC_EXPR_VARIABLE, loc, type, 0);
    if(!var_ref2) return CC_OOM_ERROR;
    var_ref2->is_lvalue = 1;
    var_ref2->var = anon;
    CcExpr* comma = cc_binary_expr(p, CC_EXPR_COMMA, loc, type, assign, var_ref2);
    if(!comma) return CC_OOM_ERROR;
    comma->is_lvalue = var_ref2->is_lvalue;
    *out = comma;
    return 0;
}

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wnullable-to-nonnull-conversion"
#endif
static
int
cc_check_cast(CcParser* _Nullable p, CcQualType from, CcQualType to, SrcLoc loc){
    if(from.bits == to.bits) return 0;
    if(p && ccqt_bt_eq(to, CCBT__Any) && (ccqt_bt_eq(from, CCBT__Any) || cc_any_convertible(p, from))) return 0;
    if(ccqt_is_basic(to) && to.basic.kind == CCBT_void) return 0;
    CcTypeKind fk = ccqt_kind(from), tk = ccqt_kind(to);
    if(fk == CC_ENUM){ from = ccqt_as_enum(from)->underlying; fk = ccqt_kind(from); }
    if(tk == CC_ENUM){ to   = ccqt_as_enum(to)->underlying;   tk = ccqt_kind(to);   }
    if(tk == CC_ARRAY){
        if(p) cc_error(p, loc, "cannot cast to array type");
        return CC_SYNTAX_ERROR;
    }
    if(tk == CC_FUNCTION){
        if(p) cc_error(p, loc, "cannot cast to function type");
        return CC_SYNTAX_ERROR;
    }
    if(tk == CC_STRUCT || tk == CC_UNION){
        if(from.ptr == to.ptr) return 0;
        if(p) cc_error(p, loc, "cannot cast to struct or union type");
        return CC_SYNTAX_ERROR;
    }
    if(fk == CC_STRUCT || fk == CC_UNION){
        if(p) cc_error(p, loc, "cannot cast from struct or union type");
        return CC_SYNTAX_ERROR;
    }
    if(fk == CC_BASIC && from.basic.kind == CCBT_void){
        if(p) cc_error(p, loc, "cannot cast from void");
        return CC_SYNTAX_ERROR;
    }
    if(fk == CC_SLICE && tk == CC_SLICE){
        if(ccqt_as_slice(from)->pointee.ptr == ccqt_as_slice(to)->pointee.ptr)
            return 0;
        if(p) cc_error(p, loc, "cannot cast to slice of different type");
        return CC_SYNTAX_ERROR;
    }
    if(fk == CC_ARRAY && tk == CC_SLICE){
        CcArray* a = ccqt_as_array(from);
        if(a->is_incomplete || a->is_vector){
            if(p) cc_error(p, loc, "cannot cast to slice from incomplete array or vector");
            return CC_SYNTAX_ERROR;
        }
        if(a->element.ptr == ccqt_as_slice(to)->pointee.ptr)
            return 0;
        if(p) cc_error(p, loc, "cannot cast to slice of different type");
        return CC_SYNTAX_ERROR;
    }
    _Bool f_arith = fk == CC_BASIC && ccbt_is_arithmetic(from.basic.kind);
    _Bool t_arith = tk == CC_BASIC && ccbt_is_arithmetic(to.basic.kind);
    if(f_arith && t_arith) return 0;
    _Bool f_ptr = fk == CC_POINTER || (fk == CC_ARRAY && !ccqt_as_array(from)->is_vector) || fk == CC_FUNCTION || (fk == CC_BASIC && from.basic.kind == CCBT_nullptr_t);
    _Bool t_ptr = tk == CC_POINTER || (tk == CC_BASIC && to.basic.kind == CCBT_nullptr_t);
    if(f_ptr && t_ptr) return 0;
    if(f_ptr && tk == CC_BASIC && ccbt_is_integer(to.basic.kind)) return 0;
    if(t_ptr && fk == CC_BASIC && ccbt_is_integer(from.basic.kind)) return 0;
    if(p) cc_error(p, loc, "invalid cast");
    return CC_SYNTAX_ERROR;
}
#ifdef __clang__
#pragma clang diagnostic pop
#endif

static
int
cc_check_func_compat(CcParser* p, CcFunc* existing, const CcDeclBase* declbase, CcQualType new_ftype, _Bool definition, SrcLoc loc){
    CcFunction* new_type = ccqt_as_function(new_ftype);
    CcFunction* old_type = existing->type;
    if(existing->static_ && declbase->spec.sp_extern)
        return cc_error(p, loc, "non-static declaration of '%.*s' follows static declaration", existing->name->length, existing->name->data);
    if(!existing->static_ && declbase->spec.sp_static)
        return cc_error(p, loc, "static declaration of '%.*s' follows non-static declaration", existing->name->length, existing->name->data);
    if(!cc_types_compatible(old_type->return_type, new_type->return_type, 0))
        return cc_error(p, loc, "conflicting return type for '%.*s'", existing->name->length, existing->name->data);
    if(old_type->no_prototype || new_type->no_prototype){
        CcFunction* prototype = new_type->no_prototype ? old_type : new_type;
        if(prototype->is_variadic)
            return cc_error(p, loc, "conflicting variadic specifier for '%.*s'", existing->name->length, existing->name->data);
        // An empty parameter list in a definition specifies zero parameters.
        if(((existing->defined && old_type->no_prototype) || (definition && new_type->no_prototype))
            && prototype->param_count)
            return cc_error(p, loc, "conflicting number of parameters for '%.*s'", existing->name->length, existing->name->data);
        for(uint32_t i = 0; i < prototype->param_count; i++){
            CcQualType param = prototype->params[i];
            if(ccqt_kind(param) == CC_ENUM) param = ccqt_as_enum(param)->underlying;
            if(ccqt_is_basic(param) && (param.basic.kind == CCBT_float
                || (ccbt_is_integer(param.basic.kind) && ccbt_int_rank(param.basic.kind) < ccbt_int_rank(CCBT_int))))
                return cc_error(p, loc, "conflicting type for parameter %u of '%.*s'", i+1, existing->name->length, existing->name->data);
        }
        return 0;
    }
    if(old_type->is_variadic != new_type->is_variadic)
        return cc_error(p, loc, "conflicting variadic specifier for '%.*s'", existing->name->length, existing->name->data);
    if(old_type->param_count != new_type->param_count)
        return cc_error(p, loc, "conflicting number of parameters for '%.*s'", existing->name->length, existing->name->data);
    for(uint32_t i = 0; i < old_type->param_count; i++){
        CcQualType old_param = {.unqual=old_type->params[i].unqual};
        CcQualType new_param = {.unqual=new_type->params[i].unqual};
        if(!cc_types_compatible(old_param, new_param, 0))
            return cc_error(p, loc, "conflicting type for parameter %u of '%.*s'", i + 1, existing->name->length, existing->name->data);
    }
    return 0;
}

// Returns CC_SYNTAX_ERROR for incompatible declarations, without emitting a
// diagnostic
static
int
cc_merge_compatible_decl_types(CcParser* p, CcQualType old, CcQualType new_, CcQualType* out){
    *out = new_;
    if(old.bits == new_.bits) return 0;
    CcTypeKind kind = ccqt_kind(old);
    if(kind != ccqt_kind(new_) || old.quals != new_.quals) return CC_SYNTAX_ERROR;
    switch(kind){
        case CC_ARRAY: {
            CcArray* a = ccqt_as_array(old);
            CcArray* b = ccqt_as_array(new_);
            if(a->is_vector != b->is_vector || a->vector_size != b->vector_size)
                return CC_SYNTAX_ERROR;
            if(!a->is_vla && !b->is_vla && !a->is_incomplete && !b->is_incomplete && a->length != b->length)
                return CC_SYNTAX_ERROR;
            CcQualType element;
            int err = cc_merge_compatible_decl_types(p, a->element, b->element, &element);
            if(err) return err;
            CcArray* bound = b->is_incomplete ? a : b;
            if(bound->is_vla){
                CcArray* arr = Allocator_alloc(cc_allocator(p), sizeof *arr);
                if(!arr) return CC_OOM_ERROR;
                *arr = *bound;
                arr->element = element;
                *out = (CcQualType){.bits = (uintptr_t)arr | new_.quals};
                return 0;
            }
            CcArray* arr = cc_intern_array(&p->type_cache, cc_allocator(p), element,
                bound->length, bound->is_static, bound->is_incomplete, bound->is_vector, bound->vector_size);
            if(!arr) return CC_OOM_ERROR;
            *out = (CcQualType){.bits = (uintptr_t)arr | new_.quals};
            return 0;
        }
        case CC_POINTER:
        case CC_BLOCK_POINTER: {
            CcPointer* a = ccqt_as_ptr(old);
            CcPointer* b = ccqt_as_ptr(new_);
            if(a->restrict_ != b->restrict_) return CC_SYNTAX_ERROR;
            CcQualType pointee;
            int err = cc_merge_compatible_decl_types(p, a->pointee, b->pointee, &pointee);
            if(err) return err;
            CcPointer* ptr = cc_intern_pointer(&p->type_cache, cc_allocator(p), pointee, b->restrict_, kind == CC_BLOCK_POINTER);
            if(!ptr) return CC_OOM_ERROR;
            *out = (CcQualType){.bits = (uintptr_t)ptr | new_.quals};
            return 0;
        }
        case CC_SLICE: {
            CcSlice* a = ccqt_as_slice(old);
            CcSlice* b = ccqt_as_slice(new_);
            if(a->restrict_ != b->restrict_) return CC_SYNTAX_ERROR;
            CcQualType pointee;
            int err = cc_merge_compatible_decl_types(p, a->pointee, b->pointee, &pointee);
            if(err) return err;
            CcSlice* slice = cc_intern_slice(&p->type_cache, cc_allocator(p), pointee, b->restrict_);
            if(!slice) return CC_OOM_ERROR;
            *out = (CcQualType){.bits = (uintptr_t)slice | new_.quals};
            return 0;
        }
        case CC_FUNCTION: {
            CcFunction* a = ccqt_as_function(old);
            CcFunction* b = ccqt_as_function(new_);
            CcQualType ret;
            int err = cc_merge_compatible_decl_types(p, a->return_type, b->return_type, &ret);
            if(err) return err;
            CcFunction* proto = b->no_prototype ? a : b;
            if(a->no_prototype || b->no_prototype){
                if(proto->is_variadic) return CC_SYNTAX_ERROR;
                for(uint32_t i = 0; i < proto->param_count; i++){
                    CcQualType pt = proto->params[i];
                    if(ccqt_kind(pt) == CC_ENUM) pt = ccqt_as_enum(pt)->underlying;
                    if(ccqt_is_basic(pt) && (pt.basic.kind == CCBT_float
                        || (ccbt_is_integer(pt.basic.kind) && ccbt_int_rank(pt.basic.kind) < ccbt_int_rank(CCBT_int))))
                        return CC_SYNTAX_ERROR;
                }
            }
            else if(a->param_count != b->param_count || a->is_variadic != b->is_variadic)
                return CC_SYNTAX_ERROR;
            Marray(CcQualType) params = {0};
            for(uint32_t i = 0; i < proto->param_count; i++){
                CcQualType pt = proto->params[i];
                pt.quals = 0;
                if(!a->no_prototype && !b->no_prototype){
                    CcQualType ap = a->params[i], bp = b->params[i];
                    ap.quals = bp.quals = 0;
                    err = cc_merge_compatible_decl_types(p, ap, bp, &pt);
                    if(err) break;
                }
                err = ma_push(CcQualType)(&params, cc_scratch_allocator(p), pt);
                if(err){ err = CC_OOM_ERROR; break; }
            }
            if(!err){
                CcFunction* f = cc_intern_function(&p->type_cache, cc_allocator(p), ret, params.data, proto->param_count, proto->param_count, proto->is_variadic, proto->no_prototype);
                if(!f) err = CC_OOM_ERROR;
                else *out = (CcQualType){.bits = (uintptr_t)f | new_.quals};
            }
            ma_cleanup(CcQualType)(&params, cc_scratch_allocator(p));
            return err;
        }
        case CC_BASIC:
        case CC_STRUCT:
        case CC_UNION:
        case CC_ENUM:
            return CC_SYNTAX_ERROR;
        DRP_CASES_EXHAUSTED;
    }
}

static
int
cc_merge_func_decl_type(CcParser* p, CcFunction* old, CcQualType* type){
    CcFunction* declared = ccqt_as_function(*type);
    CcQualType composite;
    int err = cc_merge_compatible_decl_types(p, (CcQualType){.bits=(uintptr_t)old}, *type, &composite);
    if(err) return err;
    CcFunction* merged = ccqt_as_function(composite);
    // Composite signatures ignore parameter qualifiers for compatibility, but
    // retain the new declaration's qualifiers for reflection and definitions.
    _Bool restore_quals = 0;
    if(!declared->no_prototype){
        for(uint32_t i = 0; i < declared->param_count; i++)
            restore_quals |= declared->params[i].quals != merged->params[i].quals;
    }
    if(restore_quals){
        Marray(CcQualType) params = {0};
        for(uint32_t i = 0; i < merged->param_count; i++){
            CcQualType pt = merged->params[i];
            pt.quals = declared->params[i].quals;
            err = ma_push(CcQualType)(&params, cc_scratch_allocator(p), pt);
            if(err){ err = CC_OOM_ERROR; break; }
        }
        if(!err){
            CcFunction* f = cc_intern_function(&p->type_cache, cc_allocator(p), merged->return_type,
                params.data, merged->param_count, merged->fixed_param_count, merged->is_variadic, merged->no_prototype);
            if(!f) err = CC_OOM_ERROR;
            else composite = (CcQualType){.bits=(uintptr_t)f | type->quals};
        }
        ma_cleanup(CcQualType)(&params, cc_scratch_allocator(p));
        if(err) return err;
    }
    *type = composite;
    return 0;
}

static
int
cc_check_var_type(CcParser* p, CcVariable* var, CcQualType* type, SrcLoc loc){
    CcQualType composite;
    int err = cc_merge_compatible_decl_types(p, var->type, *type, &composite);
    if(!err){ *type = composite; return 0; }
    if(err != CC_SYNTAX_ERROR) return err;
    MStringBuilder* sb;
    if(ccqt_kind(var->type) == CC_ARRAY && ccqt_kind(*type) == CC_ARRAY
        && !ccqt_as_array(var->type)->is_incomplete && !ccqt_as_array(*type)->is_incomplete
        && !ccqt_as_array(var->type)->is_vla && !ccqt_as_array(*type)->is_vla
        && ccqt_as_array(var->type)->length != ccqt_as_array(*type)->length){
        sb = cc_start_error(p, loc, "conflicting array length for '%s': %zu; previous declaration has length %zu ('",
            var->name->data, ccqt_as_array(*type)->length, ccqt_as_array(var->type)->length);
        cc_print_type(sb, *type);
        msb_write_literal(sb, "' vs '");
        cc_print_type(sb, var->type);
        msb_write_literal(sb, "')");
    }
    else {
        sb = cc_start_error(p, loc, "conflicting type for '%s': '", var->name->data);
        cc_print_type(sb, *type);
        msb_write_literal(sb, "'; previous declaration has type '");
        cc_print_type(sb, var->type);
        msb_write_char(sb, '\'');
    }
    return cc_finish_error(p, loc);
}

static
int
cc_sizeof_as_expr(CcParser* p, CcQualType t, SrcLoc loc, CcExpr* _Nullable* _Nonnull out){
    const CcTargetConfig* tgt = cc_target(p);
    CcQualType size_type = ccqt_basic(tgt->size_type);
    switch(ccqt_kind(t)){
        case CC_BASIC:{
            if(t.basic.kind >= CCBT_COUNT)
                return cc_error(p, loc, "sizeof applied to invalid kind");
            CcExpr* node = cc_value_expr(p, loc, size_type);
            if(!node) return CC_OOM_ERROR;
            node->uinteger = tgt->sizeof_[t.basic.kind];
            *out = node;
            return 0;
        }
        case CC_BLOCK_POINTER:
        case CC_POINTER:{
            CcExpr* node = cc_value_expr(p, loc, size_type);
            if(!node) return CC_OOM_ERROR;
            node->uinteger = tgt->sizeof_[CCBT_nullptr_t];
            *out = node;
            return 0;
        }
        case CC_SLICE:{
            CcExpr* node = cc_value_expr(p, loc, size_type);
            if(!node) return CC_OOM_ERROR;
            node->uinteger = 2*tgt->sizeof_[CCBT_nullptr_t]; // struct {uintptr len; T* data;}
            *out = node;
            return 0;
        }
        case CC_ARRAY:{
            CcArray* arr = ccqt_as_array(t);
            if(arr->is_vector){
                CcExpr* node = cc_value_expr(p, loc, size_type);
                if(!node) return CC_OOM_ERROR;
                node->uinteger = arr->vector_size;
                *out = node;
                return 0;
            }
            if(arr->is_incomplete)
                return cc_error(p, loc, "sizeof applied to incomplete array type");
            CcExpr* elem_size;
            int err = cc_sizeof_as_expr(p, arr->element, loc, &elem_size);
            if(err) return err;
            if(arr->is_vla){
                CcExpr* dim = arr->vla_expr;
                if(!dim) return cc_error(p, loc, "sizeof applied to VLA with no dimension");
                CcExpr* cast_dim;
                err = cc_implicit_cast(p, dim, size_type, &cast_dim);
                if(err) return err;
                CcExpr* mul = cc_binary_expr(p, CC_EXPR_MUL, loc, size_type, cast_dim, elem_size);
                if(!mul) return CC_OOM_ERROR;
                *out = mul;
                return 0;
            }
            if(elem_size->kind == CC_EXPR_VALUE){
                uint32_t size;
                if(cc_layout_array_size(elem_size->uinteger, arr->length, &size)){
                    cc_release_expr(p, elem_size);
                    return cc_error(p, loc, "object size exceeds 32-bit layout limit");
                }
                elem_size->uinteger = size;
                *out = elem_size;
                return 0;
            }
            CcExpr* len = cc_value_expr(p, loc, size_type);
            if(!len) return CC_OOM_ERROR;
            len->uinteger = (uint64_t)arr->length;
            CcExpr* mul = cc_binary_expr(p, CC_EXPR_MUL, loc, size_type, len, elem_size);
            if(!mul) return CC_OOM_ERROR;
            *out = mul;
            return 0;
        }
        case CC_STRUCT:{
            CcStruct* s = ccqt_as_struct(t);
            if(s->is_incomplete)
                return cc_error(p, loc, "sizeof applied to incomplete struct type");
            CcExpr* node = cc_value_expr(p, loc, size_type);
            if(!node) return CC_OOM_ERROR;
            node->uinteger = s->size;
            *out = node;
            return 0;
        }
        case CC_UNION:{
            CcUnion* u = ccqt_as_union(t);
            if(u->is_incomplete)
                return cc_error(p, loc, "sizeof applied to incomplete union type");
            CcExpr* node = cc_value_expr(p, loc, size_type);
            if(!node) return CC_OOM_ERROR;
            node->uinteger = u->size;
            *out = node;
            return 0;
        }
        case CC_ENUM:{
            CcEnum* e = ccqt_as_enum(t);
            if(e->is_incomplete) return cc_error(p, loc, "sizeof applied to incomplete enum type");
            return cc_sizeof_as_expr(p, e->underlying, loc, out);
        }
        case CC_FUNCTION:
            return cc_error(p, loc, "sizeof applied to function type");
    }
    #ifdef __GNUC__
    __builtin_unreachable();
    #elif defined _MSC_VER
    __assume(0);
    #endif
}

static
int
cc_alignof_as_expr(CcParser* p, CcQualType t, SrcLoc loc, CcExpr* _Nullable* _Nonnull out){
    const CcTargetConfig* cfg = cc_target(p);
    CcQualType size_type = ccqt_basic(cfg->size_type);
    uint64_t align;
    switch(ccqt_kind(t)){
        DRP_CASES_EXHAUSTED;
        case CC_BASIC:{
            if(t.basic.kind >= CCBT_COUNT)
                return cc_error(p, loc, "alignof applied to invalid kind");
            CcExpr* node = cc_value_expr(p, loc, size_type);
            if(!node) return CC_OOM_ERROR;
            node->uinteger = cfg->alignof_[t.basic.kind];
            *out = node;
            return 0;
        }
        case CC_BLOCK_POINTER:
        case CC_POINTER:
        case CC_SLICE:
            align = cfg->alignof_[CCBT_nullptr_t];
            break;
        case CC_ARRAY: {
            CcArray* arr = ccqt_as_array(t);
            if(arr->is_vector){
                align = arr->vector_size > cfg->max_align ? cfg->max_align:arr->vector_size;
                break;
            }
            return cc_alignof_as_expr(p, arr->element, loc, out);
        }
        case CC_STRUCT: {
            CcStruct* s = ccqt_as_struct(t);
            if(s->is_incomplete)
                return cc_error(p, loc, "alignof applied to incomplete struct type");
            align = s->alignment;
            break;
        }
        case CC_UNION: {
            CcUnion* u = ccqt_as_union(t);
            if(u->is_incomplete)
                return cc_error(p, loc, "alignof applied to incomplete union type");
            align = u->alignment;
            break;
        }
        case CC_ENUM: {
            CcEnum* e = ccqt_as_enum(t);
            if(e->is_incomplete) return cc_error(p, loc, "alignof applied to incomplete enum type");
            return cc_alignof_as_expr(p, e->underlying, loc, out);
        }
        case CC_FUNCTION:
            return cc_error(p, loc, "alignof applied to function type");
    }
    CcExpr* node = cc_value_expr(p, loc, size_type);
    if(!node) return CC_OOM_ERROR;
    node->uinteger = align;
    *out = node;
    return 0;
}

static
int
cc_sizeof_as_uint(CcParser* p, CcQualType t, SrcLoc loc, uint32_t* out){
    const CcTargetConfig* tgt = cc_target(p);
    switch(ccqt_kind(t)){
        case CC_BASIC:{
            if(t.basic.kind >= CCBT_COUNT)
                return ((void)cc_error(p, loc, "basic kind out of bounds"), CC_UNREACHABLE_ERROR);
            *out = tgt->sizeof_[t.basic.kind];
            return 0;
        }
        case CC_BLOCK_POINTER:
        case CC_POINTER: {
            *out = tgt->sizeof_[CCBT_nullptr_t];
            return 0;
        }
        case CC_SLICE: {
            *out = 2*tgt->sizeof_[CCBT_nullptr_t];
            return 0;
        }
        case CC_ARRAY: {
            CcArray* arr = ccqt_as_array(t);
            if(arr->is_vector){
                *out = arr->vector_size;
                return 0;
            }
            if(arr->is_incomplete)
                return ((void)cc_error(p, loc, "Taking sizeof of an incomplete type"), CC_UNREACHABLE_ERROR);
            if(arr->is_vla)
                return ((void)cc_error(p, loc, "Taking sizeof of a VLA when needing it as a constant"), CC_UNREACHABLE_ERROR);
            uint32_t elem_size;
            int err = cc_sizeof_as_uint(p, arr->element, loc, &elem_size);
            if(err) return err;
            if(cc_layout_array_size(elem_size, arr->length, out))
                return cc_error(p, loc, "object size exceeds 32-bit layout limit");
            return 0;
        }
        case CC_STRUCT: {
            CcStruct* s = ccqt_as_struct(t);
            if(s->is_incomplete)
                return ((void)cc_error(p, loc, "Taking sizeof of an incomplete type"), CC_UNREACHABLE_ERROR);
            *out = s->size;
            return 0;
        }
        case CC_UNION: {
            CcUnion* u = ccqt_as_union(t);
            if(u->is_incomplete)
                return ((void)cc_error(p, loc, "Taking sizeof of an incomplete type"), CC_UNREACHABLE_ERROR);
            *out = u->size;
            return 0;
        }
        case CC_ENUM: {
            CcEnum* e = ccqt_as_enum(t);
            if(e->is_incomplete) return cc_error(p, loc, "sizeof applied to incomplete enum type");
            return cc_sizeof_as_uint(p, e->underlying, loc, out);
        }
        case CC_FUNCTION:
            return ((void)cc_error(p, loc, "Taking sizeof of a function type (not function pointer type)"), CC_UNREACHABLE_ERROR);
    }
    #ifdef __GNUC__
    __builtin_unreachable();
    #elif defined _MSC_VER
    __assume(0);
    #endif
}

static
int
cc_alignof_as_uint(CcParser* p, CcQualType t, SrcLoc loc, uint32_t* out){
    const CcTargetConfig* tgt = cc_target(p);
    switch(ccqt_kind(t)){
        case CC_BASIC:
            if(t.basic.kind >= CCBT_COUNT)
                return ((void)cc_error(p, loc, "basic kind out of bounds"), CC_UNREACHABLE_ERROR);
            *out = tgt->alignof_[t.basic.kind];
            return 0;
        case CC_BLOCK_POINTER:
        case CC_POINTER:
        case CC_SLICE:
            *out = tgt->alignof_[CCBT_nullptr_t];
            return 0;
        case CC_ARRAY: {
            CcArray* arr = ccqt_as_array(t);
            if(arr->is_vector){
                *out = arr->vector_size > tgt->max_align ? tgt->max_align:arr->vector_size;
                return 0;
            }
            return cc_alignof_as_uint(p, arr->element, loc, out);
        }
        case CC_STRUCT: {
            CcStruct* s = ccqt_as_struct(t);
            if(s->is_incomplete)
                return ((void)cc_error(p, loc, "taking alignof an incomplete type"), CC_UNREACHABLE_ERROR);
            *out = s->alignment;
            return 0;
        }
        case CC_UNION: {
            CcUnion* u = ccqt_as_union(t);
            if(u->is_incomplete)
                return ((void)cc_error(p, loc, "taking alignof an incomplete type"), CC_UNREACHABLE_ERROR);
            *out = u->alignment;
            return 0;
        }
        case CC_ENUM: {
            CcEnum* e = ccqt_as_enum(t);
            if(e->is_incomplete) return cc_error(p, loc, "alignof applied to incomplete enum type");
            return cc_alignof_as_uint(p, e->underlying, loc, out);
        }
        case CC_FUNCTION:
            return ((void)cc_error(p, loc, "Taking alignof of a function type (not function pointer type)"), CC_UNREACHABLE_ERROR);
    }
    #ifdef __GNUC__
    __builtin_unreachable();
    #elif defined _MSC_VER
    __assume(0);
    #endif
}

static
_Bool
cc_binop_lookup(CcPunct punct, CcExprKind* kind, int* prec){
    static const struct {
        CcPunct punct;
        CcExprKind kind;
        int prec;
    } binop_table[] = {
        {CC_or,      CC_EXPR_LOGOR,  4},
        {CC_and,     CC_EXPR_LOGAND, 5},
        {CC_pipe,    CC_EXPR_BITOR,  6},
        {CC_xor,     CC_EXPR_BITXOR, 7},
        {CC_amp,     CC_EXPR_BITAND, 8},
        {CC_eq,      CC_EXPR_EQ,     9},
        {CC_ne,      CC_EXPR_NE,     9},
        {CC_lt,      CC_EXPR_LT,     10},
        {CC_gt,      CC_EXPR_GT,     10},
        {CC_le,      CC_EXPR_LE,     10},
        {CC_ge,      CC_EXPR_GE,     10},
        {CC_lshift,  CC_EXPR_LSHIFT, 11},
        {CC_rshift,  CC_EXPR_RSHIFT, 11},
        {CC_plus,    CC_EXPR_ADD,    12},
        {CC_minus,   CC_EXPR_SUB,    12},
        {CC_star,    CC_EXPR_MUL,    13},
        {CC_slash,   CC_EXPR_DIV,    13},
        {CC_percent, CC_EXPR_MOD,    13},
    };
    for(size_t i = 0; i < sizeof binop_table / sizeof binop_table[0]; i++){
        if(binop_table[i].punct == punct){
            *kind = binop_table[i].kind;
            *prec = binop_table[i].prec;
            return 1;
        }
    }
    return 0;
}

static
_Bool
cc_assign_lookup(CcPunct punct, CcExprKind* kind){
    switch((uint32_t)punct){
        case CC_assign:         *kind = CC_EXPR_ASSIGN;       return 1;
        case CC_plus_assign:    *kind = CC_EXPR_ADDASSIGN;    return 1;
        case CC_minus_assign:   *kind = CC_EXPR_SUBASSIGN;    return 1;
        case CC_star_assign:    *kind = CC_EXPR_MULASSIGN;    return 1;
        case CC_slash_assign:   *kind = CC_EXPR_DIVASSIGN;    return 1;
        case CC_percent_assign: *kind = CC_EXPR_MODASSIGN;    return 1;
        case CC_amp_assign:     *kind = CC_EXPR_BITANDASSIGN; return 1;
        case CC_pipe_assign:    *kind = CC_EXPR_BITORASSIGN;  return 1;
        case CC_xor_assign:     *kind = CC_EXPR_BITXORASSIGN; return 1;
        case CC_lshift_assign:  *kind = CC_EXPR_LSHIFTASSIGN; return 1;
        case CC_rshift_assign:  *kind = CC_EXPR_RSHIFTASSIGN; return 1;
        default: return 0;
    }
}

static
int
cc_parse_expr(CcParser* p, CcValueClass vc, CcExpr* _Nullable* _Nonnull out){
    CcExpr* left;
    int err = cc_parse_assignment_expr(p, vc, &left, CCQT_NONE);
    if(err) return err;
    for(;;){
        CcToken tok;
        err = cc_next_token(p, &tok);
        if(err) return err;
        if(tok.type == CC_PUNCTUATOR && tok.punct.punct == CC_comma){
            CcExpr* right;
            err = cc_parse_assignment_expr(p, vc, &right, CCQT_NONE);
            if(err) return err;
            CcExpr* node = cc_make_expr(p, CC_EXPR_COMMA, tok.loc, right->type, 1);
            if(!node) return CC_OOM_ERROR;
            node->is_lvalue = right->is_lvalue;
            node->lhs = left;
            node->values[0] = right;
            left = node;
        }
        else {
            cc_unget(p, &tok);
            break;
        }
    }
    *out = left;
    return 0;
}

static
int
cc_parse_assignment_expr(CcParser* p, CcValueClass vc, CcExpr* _Nullable* _Nonnull out, CcQualType optional_expected){
    if(optional_expected.bits){
        CcToken tok;
        int err = cc_peek(p, &tok);
        if(err) return err;
        if(tok.type == CC_PUNCTUATOR && tok.punct.punct == '{'){
            err = cc_parse_init_list(p, vc, out, optional_expected);
            return err;
        }
    }
    CcExpr* left;
    int err = cc_parse_ternary_expr(p, vc, &left);
    if(err) return err;
    CcToken tok;
    err = cc_next_token(p, &tok);
    if(err) return err;
    if(tok.type == CC_PUNCTUATOR){
        CcExprKind kind;
        if(cc_assign_lookup(tok.punct.punct, &kind)){
            if(vc > CC_RUNTIME_VALUE)
                return cc_error(p, tok.loc, "assignment in constant expression");
            if(left->kind == CC_EXPR_COMPOUND_LITERAL){
                err = cc_desugar_compound_literal(p, left, &left);
                if(err) return err;
            }
            if(!left->is_lvalue)
                return cc_error(p, tok.loc, "expression is not assignable");
            if(left->type.is_const)
                return cc_error(p, tok.loc, "cannot assign to variable with const-qualified type");
            CcExpr* right;
            CcToken peek_assign;
            err = cc_peek(p, &peek_assign);
            if(err) return err;
            err = cc_parse_assignment_expr(p, vc, &right, kind == CC_EXPR_ASSIGN ? left->type : CCQT_NONE);
            if(err) return err;
            if(kind != CC_EXPR_ASSIGN && kind != CC_EXPR_ADDASSIGN && kind != CC_EXPR_SUBASSIGN){
                CcQualType lt = left->type;
                lt.is_atomic = 0;
                if(ccqt_kind(lt) == CC_ENUM) lt = ccqt_as_enum(lt)->underlying;
                if(!ccqt_is_basic(lt) || !ccbt_is_arithmetic(lt.basic.kind)){
                    return cc_error(p, tok.loc, "compound assignment requires arithmetic operands");
                }
            }
            if(kind == CC_EXPR_ASSIGN){
                err = cc_check_atomic_object_access(p, left->type, tok.loc);
                if(err) return err;
            }
            else {
                err = cc_check_atomic_rmw(p, left->type, tok.loc);
                if(err) return err;
            }
            if(kind != CC_EXPR_ASSIGN && ccqt_bt_eq(left->type, CCBT__Any))
                return cc_error(p, tok.loc, "compound assignment requires arithmetic or pointer type");
            if(kind == CC_EXPR_MODASSIGN || kind == CC_EXPR_BITANDASSIGN
            || kind == CC_EXPR_BITORASSIGN || kind == CC_EXPR_BITXORASSIGN
            || kind == CC_EXPR_LSHIFTASSIGN || kind == CC_EXPR_RSHIFTASSIGN){
                CcQualType lt = left->type;
                lt.is_atomic = 0;
                if(ccqt_kind(lt) == CC_ENUM) lt = ccqt_as_enum(lt)->underlying;
                if(!ccqt_is_basic(lt) || !ccbt_is_integer(lt.basic.kind))
                    return cc_error(p, tok.loc, "operator requires integer operands");
            }
            CcQualType operation_type = CCQT_NONE;
            if((kind == CC_EXPR_ADDASSIGN || kind == CC_EXPR_SUBASSIGN) && ccqt_kind(left->type) == CC_POINTER){
                err = cc_check_pointer_arithmetic(p, left->type, tok.loc);
                if(err) return err;
                if(!ccqt_is_integer(right->type))
                    return cc_error(p, tok.loc, "pointer arithmetic requires integer operand");
                err = cc_implicit_cast_to_index(p, right, &right);
                if(err) return err;
            }
            else if(kind == CC_EXPR_ASSIGN){
                err = cc_check_atomic_object_access(p, right->type, right->loc);
                if(err) return err;
                err = cc_implicit_cast(p, right, (CcQualType){.unqual=left->type.unqual}, &right);
                if(err) return err;
            }
            else {
                err = cc_check_atomic_object_access(p, right->type, right->loc);
                if(err) return err;
                if(kind == CC_EXPR_MODASSIGN || kind == CC_EXPR_BITANDASSIGN
                || kind == CC_EXPR_BITORASSIGN || kind == CC_EXPR_BITXORASSIGN
                || kind == CC_EXPR_LSHIFTASSIGN || kind == CC_EXPR_RSHIFTASSIGN){
                    if(!ccqt_is_integer(right->type))
                        return cc_error(p, tok.loc, "operator requires integer operands");
                }
                if(kind == CC_EXPR_LSHIFTASSIGN || kind == CC_EXPR_RSHIFTASSIGN){
                    err = cc_integer_promote(p, cc_arithmetic_operand_type(p, left), &operation_type, tok.loc);
                }
                else err = cc_usual_arithmetic(p, left, right, &operation_type, tok.loc);
                if(err) return err;
                err = cc_implicit_cast(p, right, operation_type, &right);
                if(err) return err;
            }
            if(kind == CC_EXPR_ASSIGN && (right->kind == CC_EXPR_COMPOUND_LITERAL || right->kind == CC_EXPR_INIT_LIST)){
                err = cc_desugar_compound_literal(p, right, &right);
                if(err) return err;
            }
            CcExpr* node = cc_make_expr(p, kind, tok.loc, (CcQualType){.unqual=left->type.unqual}, 1);
            if(!node) return CC_OOM_ERROR;
            node->lhs = left;
            if(kind != CC_EXPR_ASSIGN) node->compound.type = operation_type;
            node->values[0] = right;
            *out = node;
            return 0;
        }
    }
    cc_unget(p, &tok);
    *out = left;
    return 0;
}

static
int
cc_parse_ternary_expr(CcParser* p, CcValueClass vc, CcExpr* _Nullable* _Nonnull out){
    CcExpr* cond;
    int err = cc_parse_prefix(p, vc, &cond);
    if(err) return err;
    err = cc_parse_infix(p, vc, cond, 4, &cond);
    if(err) return err;
    CcToken tok;
    err = cc_next_token(p, &tok);
    if(err) return err;
    if(tok.type == CC_PUNCTUATOR && tok.punct.punct == CC_question){
        err = cc_require_scalar(p, &cond, tok.loc, "'?:'");
        if(err) return err;
        CcExpr* then_expr;
        err = cc_parse_expr(p, vc, &then_expr);
        if(err) return err;
        err = cc_expect_punct(p, CC_colon);
        if(err) return err;
        CcExpr* else_expr;
        err = cc_parse_ternary_expr(p, vc, &else_expr);
        if(err) return err;
        CcQualType common;
        CcTypeKind tk = ccqt_kind(then_expr->type);
        CcTypeKind ek = ccqt_kind(else_expr->type);
        _Bool tptr = (ccqt_is_pointer_like(then_expr->type) && !ccqt_bt_eq(then_expr->type, CCBT_nullptr_t)) || tk == CC_FUNCTION;
        _Bool eptr = (ccqt_is_pointer_like(else_expr->type) && !ccqt_bt_eq(else_expr->type, CCBT_nullptr_t)) || ek == CC_FUNCTION;
        if(tptr && eptr){
            CcQualType ttype = then_expr->type;
            CcQualType etype = else_expr->type;
            if(tk == CC_ARRAY && !ccqt_as_array(ttype)->is_vector){
                err = cc_pointer_of(p, cc_array_element_type(ttype), &ttype);
                if(err) return err;
                err = cc_implicit_cast(p, then_expr, ttype, &then_expr);
                if(err) return err;
            }
            if(tk == CC_FUNCTION){
                err = cc_pointer_of(p, ttype, &ttype);
                if(err) return err;
                err = cc_implicit_cast(p, then_expr, ttype, &then_expr);
                if(err) return err;
            }
            if(ek == CC_ARRAY && !ccqt_as_array(etype)->is_vector){
                err = cc_pointer_of(p, cc_array_element_type(etype), &etype);
                if(err) return err;
                err = cc_implicit_cast(p, else_expr, etype, &else_expr);
                if(err) return err;
            }
            if(ek == CC_FUNCTION){
                err = cc_pointer_of(p, etype, &etype);
                if(err) return err;
                err = cc_implicit_cast(p, else_expr, etype, &else_expr);
                if(err) return err;
            }
            CcPointer* tp = ccqt_as_ptr(ttype);
            CcPointer* ep = ccqt_as_ptr(etype);
            CcQualType tpointee = tp->pointee;
            CcQualType epointee = ep->pointee;
            _Bool tvoid = ccqt_is_basic(tpointee) && tpointee.basic.kind == CCBT_void;
            _Bool evoid = ccqt_is_basic(epointee) && epointee.basic.kind == CCBT_void;
            CcQualType pointee;
            if(tvoid || evoid){
                pointee = ccqt_basic(CCBT_void);
                pointee.quals = tpointee.quals | epointee.quals;
            }
            else if(cc_types_compatible((CcQualType){.unqual=tpointee.unqual}, (CcQualType){.unqual=epointee.unqual}, 0)){
                uintptr_t quals = tpointee.quals | epointee.quals;
                tpointee.quals = epointee.quals = quals;
                err = cc_merge_compatible_decl_types(p, tpointee, epointee, &pointee);
                if(err) return err;
            }
            else {
                pointee = tpointee;
                pointee.quals |= epointee.quals;
            }
            err = cc_pointer_of(p, pointee, &common);
            if(err) return err;
        }
        else if(tptr && (ccqt_is_basic(else_expr->type) || ek == CC_ENUM)){
            _Bool is_npc;
            err = cc_is_null_pointer_constant(p, else_expr, &is_npc);
            if(err) return err;
            if(!is_npc)
                return cc_error(p, tok.loc, "incompatible operand types for ternary");
            if(tk == CC_ARRAY && !ccqt_as_array(then_expr->type)->is_vector){
                err = cc_pointer_of(p, cc_array_element_type(then_expr->type), &common);
                if(err) return err;
            }
            else if(tk == CC_FUNCTION){
                err = cc_pointer_of(p, then_expr->type, &common);
                if(err) return err;
            }
            else common = then_expr->type;
        }
        else if(eptr && (ccqt_is_basic(then_expr->type) || tk == CC_ENUM)){
            _Bool is_npc;
            err = cc_is_null_pointer_constant(p, then_expr, &is_npc);
            if(err) return err;
            if(!is_npc)
                return cc_error(p, tok.loc, "incompatible operand types for ternary");
            if(ek == CC_ARRAY && !ccqt_as_array(else_expr->type)->is_vector){
                err = cc_pointer_of(p, cc_array_element_type(else_expr->type), &common);
                if(err) return err;
            }
            else if(ek == CC_FUNCTION){
                err = cc_pointer_of(p, else_expr->type, &common);
                if(err) return err;
            }
            else common = else_expr->type;
        }
        else if(ccqt_bt_eq(then_expr->type, CCBT_nullptr_t) && ccqt_bt_eq(else_expr->type, CCBT_nullptr_t)){
            common = ccqt_basic(CCBT_nullptr_t);
        }
        else if(ccqt_is_basic(then_expr->type) && then_expr->type.basic.kind == CCBT_void
             && ccqt_is_basic(else_expr->type) && else_expr->type.basic.kind == CCBT_void){
            common = then_expr->type;
        }
        else if(ccqt_bt_eq(then_expr->type, CCBT__Any) && ccqt_bt_eq(else_expr->type, CCBT__Any)){
            common = ccqt_basic(CCBT__Any);
        }
        else if((tk == CC_STRUCT || tk == CC_UNION) && then_expr->type.ptr == else_expr->type.ptr){
            common = then_expr->type;
        }
        else if(tk == CC_SLICE && ek == CC_SLICE){
            CcQualType tpointee = ccqt_as_slice(then_expr->type)->pointee;
            CcQualType epointee = ccqt_as_slice(else_expr->type)->pointee;
            if(tpointee.unqual != epointee.unqual)
                return cc_error(p, tok.loc, "incompatible operand types for ternary");
            CcQualType pointee = {.quals = tpointee.quals | epointee.quals, .unqual = tpointee.unqual};
            err = cc_slice_of(p, pointee, &common);
            if(err) return err;
        }
        else {
            err = cc_usual_arithmetic(p, then_expr, else_expr, &common, tok.loc);
            if(err) return err;
        }
        common = (CcQualType){.unqual=common.unqual};
        err = cc_implicit_cast(p, then_expr, common, &then_expr);
        if(err) return err;
        err = cc_implicit_cast(p, else_expr, common, &else_expr);
        if(err) return err;
        CcExpr* node = cc_make_expr(p, CC_EXPR_TERNARY, tok.loc, common, 2);
        if(!node) return CC_OOM_ERROR;
        node->lhs = cond;
        node->values[0] = then_expr;
        node->values[1] = else_expr;
        *out = node;
        return 0;
    }
    cc_unget(p, &tok);
    *out = cond;
    return 0;
}

static
int
cc_parse_infix(CcParser* p, CcValueClass vc, CcExpr* left, int min_prec, CcExpr* _Nullable* _Nonnull out){
    for(;;){
        CcToken tok;
        int err = cc_next_token(p, &tok);
        if(err) return err;
        if(tok.type != CC_PUNCTUATOR){
            cc_unget(p, &tok);
            break;
        }
        CcExprKind kind;
        int prec;
        if(!cc_binop_lookup(tok.punct.punct, &kind, &prec)){
            cc_unget(p, &tok);
            break;
        }
        if(prec < min_prec){
            cc_unget(p, &tok);
            break;
        }
        CcExpr* right;
        err = cc_parse_prefix(p, vc, &right);
        if(err) return err;
        err = cc_parse_infix(p, vc, right, prec + 1, &right);
        if(err) return err;
        if((kind == CC_EXPR_ADD || kind == CC_EXPR_SUB)
            && (ccqt_bt_eq(left->type, CCBT_nullptr_t) || ccqt_bt_eq(right->type, CCBT_nullptr_t)))
            return cc_error(p, tok.loc, "pointer arithmetic does not accept nullptr_t");
        CcQualType result_type = {0};
        switch(kind){
            case CC_EXPR_LOGAND: case CC_EXPR_LOGOR:
                err = cc_require_scalar(p, &left, tok.loc, kind == CC_EXPR_LOGAND ? "'&&'" : "'||'");
                if(err) return err;
                err = cc_require_scalar(p, &right, tok.loc, kind == CC_EXPR_LOGAND ? "'&&'" : "'||'");
                if(err) return err;
                result_type = ccqt_basic(CCBT_int);
                break;
            case CC_EXPR_EQ: case CC_EXPR_NE:
            case CC_EXPR_LT: case CC_EXPR_GT:
            case CC_EXPR_LE: case CC_EXPR_GE: {
                if(ccqt_is_basic(left->type) && left->type.basic.kind == CCBT__Type && ccqt_is_basic(right->type) && right->type.basic.kind == CCBT__Type){
                    if(kind != CC_EXPR_EQ && kind != CC_EXPR_NE)
                        return cc_error(p, tok.loc, "ordered comparison of _Type values");
                    result_type = ccqt_basic(CCBT_int);
                    break;
                }
                _Bool lp = ccqt_is_pointer_like(left->type) || ccqt_kind(left->type) == CC_FUNCTION;
                _Bool rp = ccqt_is_pointer_like(right->type) || ccqt_kind(right->type) == CC_FUNCTION;
                if(!lp && !rp){
                    CcQualType common;
                    err = cc_usual_arithmetic(p, left, right, &common, tok.loc);
                    if(err) return err;
                    err = cc_implicit_cast(p, left, common, &left);
                    if(err) return err;
                    err = cc_implicit_cast(p, right, common, &right);
                    if(err) return err;
                }
                else if(lp && rp){
                    if(ccqt_kind(left->type) == CC_ARRAY && !ccqt_as_array(left->type)->is_vector){
                        CcQualType ptr_type;
                        err = cc_pointer_of(p, cc_array_element_type(left->type), &ptr_type);
                        if(err) return err;
                        err = cc_implicit_cast(p, left, ptr_type, &left);
                        if(err) return err;
                    }
                    if(ccqt_kind(right->type) == CC_ARRAY && !ccqt_as_array(right->type)->is_vector){
                        CcQualType ptr_type;
                        err = cc_pointer_of(p, cc_array_element_type(right->type), &ptr_type);
                        if(err) return err;
                        err = cc_implicit_cast(p, right, ptr_type, &right);
                        if(err) return err;
                    }
                    if(ccqt_kind(left->type) == CC_FUNCTION){
                        CcQualType ptr_type;
                        err = cc_pointer_of(p, left->type, &ptr_type);
                        if(err) return err;
                        err = cc_implicit_cast(p, left, ptr_type, &left);
                        if(err) return err;
                    }
                    if(ccqt_kind(right->type) == CC_FUNCTION){
                        CcQualType ptr_type;
                        err = cc_pointer_of(p, right->type, &ptr_type);
                        if(err) return err;
                        err = cc_implicit_cast(p, right, ptr_type, &right);
                        if(err) return err;
                    }
                    if(!ccqt_bt_eq(left->type, CCBT_nullptr_t) && !ccqt_bt_eq(right->type, CCBT_nullptr_t)){
                        CcQualType lpointee, rpointee;
                        err = cc_deref_type(p, left->type, &lpointee, tok.loc, 0);
                        if(err) return cc_unreachable(p, tok.loc, "Error dereferencing lhs pointer");
                        err = cc_deref_type(p, right->type, &rpointee, tok.loc, 0);
                        if(err) return cc_unreachable(p, tok.loc, "Error dereferencing rhs pointer");
                        if(!cc_types_compatible((CcQualType){.unqual=lpointee.unqual}, (CcQualType){.unqual=rpointee.unqual}, 0)
                        && !(ccqt_is_basic(lpointee) && lpointee.basic.kind == CCBT_void)
                        && !(ccqt_is_basic(rpointee) && rpointee.basic.kind == CCBT_void))
                            return cc_error(p, tok.loc, "comparison of incompatible pointer types");
                    }
                }
                else if(lp){
                    err = cc_implicit_cast(p, right, left->type, &right);
                    if(err) return err;
                }
                else {
                    err = cc_implicit_cast(p, left, right->type, &left);
                    if(err) return err;
                }
                result_type = ccqt_basic(CCBT_int);
                break;
            }
            case CC_EXPR_LSHIFT: case CC_EXPR_RSHIFT: {
                CcQualType lp, rp;
                err = cc_integer_promote(p, cc_arithmetic_operand_type(p, left), &lp, tok.loc);
                if(err) return err;
                err = cc_integer_promote(p, cc_arithmetic_operand_type(p, right), &rp, tok.loc);
                if(err) return err;
                if((ccqt_is_basic(lp) && ccbt_is_float(lp.basic.kind))
                || (ccqt_is_basic(rp) && ccbt_is_float(rp.basic.kind)))
                    return cc_error(p, tok.loc, "shift operands require integer type");
                err = cc_implicit_cast(p, left, lp, &left);
                if(err) return err;
                err = cc_implicit_cast(p, right, rp, &right);
                if(err) return err;
                result_type = lp;
                break;
            }
            case CC_EXPR_ADD: {
                _Bool lptr = ccqt_is_pointer_like(left->type);
                _Bool rptr = ccqt_is_pointer_like(right->type);
                if(lptr && rptr)
                    return cc_error(p, tok.loc, "addition of two pointers");
                if(lptr || rptr) {
                    CcExpr** ptr_operand = lptr ? &left : &right;
                    CcExpr* int_operand = lptr ? right : left;
                    err = cc_check_pointer_arithmetic(p, (*ptr_operand)->type, tok.loc);
                    if(err) return err;
                    if(!ccqt_is_integer(int_operand->type))
                        return cc_error(p, tok.loc, "pointer arithmetic requires integer operand");
                    if(rptr){
                        err = cc_implicit_cast_to_index(p, left, &left);
                        if(err) return err;
                    }
                    else {
                        err = cc_implicit_cast_to_index(p, right, &right);
                        if(err) return err;
                    }
                    if(ccqt_kind((*ptr_operand)->type) == CC_ARRAY){
                        err = cc_pointer_of(p, cc_array_element_type((*ptr_operand)->type), &result_type);
                        if(err) return err;
                        err = cc_implicit_cast(p, *ptr_operand, result_type, ptr_operand);
                        if(err) return err;
                    }
                    else
                        result_type = (CcQualType){.unqual=(*ptr_operand)->type.unqual};
                }
                else {
                    err = cc_usual_arithmetic(p, left, right, &result_type, tok.loc);
                    if(err) return err;
                    err = cc_implicit_cast(p, left, result_type, &left);
                    if(err) return err;
                    err = cc_implicit_cast(p, right, result_type, &right);
                    if(err) return err;
                }
                break;
            }
            case CC_EXPR_SUB: {
                _Bool lptr = ccqt_is_pointer_like(left->type);
                _Bool rptr = ccqt_is_pointer_like(right->type);
                if(lptr && rptr){
                    err = cc_check_pointer_arithmetic(p, left->type, tok.loc);
                    if(err) return err;
                    err = cc_check_pointer_arithmetic(p, right->type, tok.loc);
                    if(err) return err;
                    CcQualType lp, rp;
                    err = cc_deref_type(p, left->type, &lp, tok.loc, 0);
                    if(err) return err;
                    err = cc_deref_type(p, right->type, &rp, tok.loc, 0);
                    if(err) return err;
                    if(_ccqt_to_type_ptr(lp) != _ccqt_to_type_ptr(rp)
                    && !(ccqt_is_basic(lp) && lp.basic.kind == CCBT_void)
                    && !(ccqt_is_basic(rp) && rp.basic.kind == CCBT_void))
                        return cc_error(p, tok.loc, "pointer subtraction with incompatible types");
                    if(ccqt_kind(left->type) == CC_ARRAY){
                        CcQualType ptr_type;
                        err = cc_pointer_of(p, cc_array_element_type(left->type), &ptr_type);
                        if(err) return err;
                        err = cc_implicit_cast(p, left, ptr_type, &left);
                        if(err) return err;
                    }
                    if(ccqt_kind(right->type) == CC_ARRAY){
                        CcQualType ptr_type;
                        err = cc_pointer_of(p, cc_array_element_type(right->type), &ptr_type);
                        if(err) return err;
                        err = cc_implicit_cast(p, right, ptr_type, &right);
                        if(err) return err;
                    }
                    result_type = ccqt_basic(cc_target(p)->ptrdiff_type);
                }
                else if(lptr){
                    err = cc_check_pointer_arithmetic(p, left->type, tok.loc);
                    if(err) return err;
                    if(!ccqt_is_integer(right->type))
                        return cc_error(p, tok.loc, "pointer arithmetic requires integer operand");
                    err = cc_implicit_cast_to_index(p, right, &right);
                    if(err) return err;
                    if(ccqt_kind(left->type) == CC_ARRAY){
                        err = cc_pointer_of(p, cc_array_element_type(left->type), &result_type);
                        if(err) return err;
                        err = cc_implicit_cast(p, left, result_type, &left);
                        if(err) return err;
                    }
                    else
                        result_type = (CcQualType){.unqual=left->type.unqual};
                }
                else {
                    err = cc_usual_arithmetic(p, left, right, &result_type, tok.loc);
                    if(err) return err;
                    err = cc_implicit_cast(p, left, result_type, &left);
                    if(err) return err;
                    err = cc_implicit_cast(p, right, result_type, &right);
                    if(err) return err;
                }
                break;
            }
            case CC_EXPR_MUL:
            case CC_EXPR_DIV:
            case CC_EXPR_MOD:
            case CC_EXPR_BITAND:
            case CC_EXPR_BITOR:
            case CC_EXPR_BITXOR: {
                err = cc_usual_arithmetic(p, left, right, &result_type, tok.loc);
                if(err) return err;
                if(kind == CC_EXPR_MOD || kind == CC_EXPR_BITAND
                || kind == CC_EXPR_BITOR || kind == CC_EXPR_BITXOR){
                    if(ccqt_is_basic(result_type) && ccbt_is_float(result_type.basic.kind))
                        return cc_error(p, tok.loc, "operator requires integer operands");
                }
                err = cc_implicit_cast(p, left, result_type, &left);
                if(err) return err;
                err = cc_implicit_cast(p, right, result_type, &right);
                if(err) return err;
                break;
            }
            case CC_EXPR_VALUE:
            case CC_EXPR_SIZEOF_VMT:
            case CC_EXPR_VARIABLE:
            case CC_EXPR_FUNCTION:
            case CC_EXPR_COMPOUND_LITERAL:
            case CC_EXPR_INIT_LIST:
            case CC_EXPR_OBJECT_VIEW:
            case CC_EXPR_NEG:
            case CC_EXPR_POS:
            case CC_EXPR_BITNOT:
            case CC_EXPR_LOGNOT:
            case CC_EXPR_DEREF:
            case CC_EXPR_ADDR:
            case CC_EXPR_PREINC:
            case CC_EXPR_PREDEC:
            case CC_EXPR_POSTINC:
            case CC_EXPR_POSTDEC:
            case CC_EXPR_ASSIGN:
            case CC_EXPR_ADDASSIGN:
            case CC_EXPR_SUBASSIGN:
            case CC_EXPR_MULASSIGN:
            case CC_EXPR_DIVASSIGN:
            case CC_EXPR_MODASSIGN:
            case CC_EXPR_BITANDASSIGN:
            case CC_EXPR_BITORASSIGN:
            case CC_EXPR_BITXORASSIGN:
            case CC_EXPR_LSHIFTASSIGN:
            case CC_EXPR_RSHIFTASSIGN:
            case CC_EXPR_TERNARY:
            case CC_EXPR_CAST:
            case CC_EXPR_CALL:
            case CC_EXPR_SUBSCRIPT:
            case CC_EXPR_DOT:
            case CC_EXPR_ARROW:
            case CC_EXPR_COMMA:
            case CC_EXPR_STATEMENT_EXPRESSION:
            case CC_EXPR_ATOMIC:
            case CC_EXPR_VA:
            case CC_EXPR_BUILTIN:
            case CC_EXPR_ADD_OVERFLOW:
            case CC_EXPR_MUL_OVERFLOW:
            case CC_EXPR_SUB_OVERFLOW:
            case CC_EXPR_BIT_BUILTIN:
            case CC_EXPR_ALLOCA:
            case CC_EXPR_INTERN:
            case CC_EXPR_HOTSWAP:
            case CC_EXPR_SRCLOC_REFLECT:
            case CC_EXPR_COMPILE:
            case CC_EXPR_MODULE_REFLECT:
            case CC_EXPR_TYPE_INTROSPECTION:
            case CC_EXPR_UMUL128:
            case CC_EXPR_SLICE:
            case CC_EXPR_SLICE_LO:
            case CC_EXPR_SLICE_HI:
            case CC_EXPR_SLICE_ALL:
            case CC_EXPR_BSWAP:
                return CC_UNREACHABLE_ERROR;
        }
        CcExpr* node = cc_make_expr(p, kind, tok.loc, result_type, 1);
        if(!node) return CC_OOM_ERROR;
        node->lhs = left;
        node->values[0] = right;
        left = node;
    }
    *out = left;
    return 0;
}

static
int
cc_parse_prefix(CcParser* p, CcValueClass vc, CcExpr* _Nullable* _Nonnull out){
    CcToken tok;
    int err = cc_next_token(p, &tok);
    if(err) return err;
    if(tok.type == CC_PUNCTUATOR){
        if(tok.punct.punct == CC_lparen){
            CcToken peek;
            err = cc_peek(p, &peek);
            if(err) return err;
            if(cc_is_type_start(p, &peek)){
                CcQualType cast_type;
                CcParsedParams param_names = {0};
                err = cc_parse_type_name(p, &cast_type, &param_names);
                if(err){ ma_cleanup(CcFuncParam)(&param_names.names, cc_allocator(p)); return err; }
                CcToken peek_after;
                err = cc_peek(p, &peek_after);
                if(err){ ma_cleanup(CcFuncParam)(&param_names.names, cc_allocator(p)); return err; }
                if(peek_after.type == CC_PUNCTUATOR && peek_after.punct.punct == CC_lbrace
                   && ccqt_kind(cast_type) == CC_FUNCTION){
                    CcExpr* lambda;
                    err = cc_parse_lambda_body(p, vc, tok.loc, cast_type, &param_names, &lambda);
                    if(err) return err;
                    err = cc_expect_punct(p, CC_rparen);
                    if(err) return err;
                    return cc_parse_postfix(p, vc, lambda, out);
                }
                ma_cleanup(CcFuncParam)(&param_names.names, cc_allocator(p));
                err = cc_expect_punct(p, CC_rparen);
                if(err) return err;
                CcToken peek2;
                err = cc_peek(p, &peek2);
                if(err) return err;
                if(peek2.type == CC_PUNCTUATOR && peek2.punct.punct == CC_lbrace){
                    CcExpr* result;
                    err = cc_parse_init_list(p, vc, &result, cast_type);
                    if(err) return err;
                    result->kind = CC_EXPR_COMPOUND_LITERAL;
                    return cc_parse_postfix(p, vc, result, out);
                }
                if(peek2.type == CC_PUNCTUATOR && (
                    peek2.punct.punct == CC_dot
                    || peek2.punct.punct == CC_rparen
                    || peek2.punct.punct == CC_semi
                    || peek2.punct.punct == CC_comma
                    || peek2.punct.punct == CC_eq
                    || peek2.punct.punct == CC_ne
                )){
                    CcExpr* type_val = cc_value_expr(p, tok.loc, ccqt_basic(CCBT__Type));
                    if(!type_val) return CC_OOM_ERROR;
                    type_val->uinteger = cast_type.bits;
                    return cc_parse_postfix(p, vc, type_val, out);
                }
                CcExpr* operand;
                err = cc_parse_prefix(p, vc, &operand);
                if(err) return err;
                err = cc_check_cast(p, operand->type, cast_type, tok.loc);
                if(err) return err;
                if(ccqt_bt_eq(cast_type, CCBT__Any))
                    return cc_implicit_cast(p, operand, cast_type, out);
                CcExpr* cast = cc_unary_expr(p, CC_EXPR_CAST, tok.loc, cast_type, operand);
                if(!cast) return CC_OOM_ERROR;
                *out = cast;
                return 0;
            }
        }
        CcExprKind kind = CC_EXPR_VALUE;
        _Bool is_prefix = 1;
        switch(tok.punct.punct){
            case CC_minus:     kind = CC_EXPR_NEG;    break;
            case CC_plus:      kind = CC_EXPR_POS;    break;
            case CC_tilde:     kind = CC_EXPR_BITNOT; break;
            case CC_bang:      kind = CC_EXPR_LOGNOT; break;
            case CC_star:      kind = CC_EXPR_DEREF;  break;
            case CC_amp:       kind = CC_EXPR_ADDR;   break;
            case CC_plusplus:   kind = CC_EXPR_PREINC; break;
            case CC_minusminus:kind = CC_EXPR_PREDEC; break;
            case CC_lbracket:
            case CC_rbracket:
            case CC_lparen:
            case CC_rparen:
            case CC_lbrace:
            case CC_rbrace:
            case CC_dot:
            case CC_slash:
            case CC_percent:
            case CC_lt:
            case CC_gt:
            case CC_xor:
            case CC_pipe:
            case CC_question:
            case CC_colon:
            case CC_semi:
            case CC_assign:
            case CC_comma:
            case CC_arrow:
            case CC_lshift:
            case CC_rshift:
            case CC_le:
            case CC_ge:
            case CC_eq:
            case CC_ne:
            case CC_and:
            case CC_or:
            case CC_double_colon:
            case CC_ellipsis:
            case CC_star_assign:
            case CC_slash_assign:
            case CC_percent_assign:
            case CC_plus_assign:
            case CC_minus_assign:
            case CC_lshift_assign:
            case CC_rshift_assign:
            case CC_amp_assign:
            case CC_xor_assign:
            case CC_pipe_assign:
                is_prefix = 0; break;
        }
        if(is_prefix){
            if((kind == CC_EXPR_PREINC || kind == CC_EXPR_PREDEC) && vc > CC_RUNTIME_VALUE)
                return cc_error(p, tok.loc, "increment/decrement in constant expression");
            CcExpr* operand;
            CcValueClass operand_vc = (kind == CC_EXPR_ADDR) ? CC_RUNTIME_VALUE : vc;
            err = cc_parse_prefix(p, operand_vc, &operand);
            if(err) return err;
            CcQualType result_type;
            switch(kind){
                case CC_EXPR_NEG: case CC_EXPR_POS: {
                    err = cc_integer_promote(p, cc_arithmetic_operand_type(p, operand), &result_type, tok.loc);
                    if(err) return err;
                    err = cc_implicit_cast(p, operand, result_type, &operand);
                    if(err) return err;
                    break;
                }
                case CC_EXPR_BITNOT: {
                    if(!ccqt_is_integer(operand->type))
                        return cc_error(p, tok.loc, "'~' requires integer type");
                    err = cc_integer_promote(p, cc_arithmetic_operand_type(p, operand), &result_type, tok.loc);
                    if(err) return err;
                    err = cc_implicit_cast(p, operand, result_type, &operand);
                    if(err) return err;
                    break;
                }
                case CC_EXPR_LOGNOT:
                    err = cc_require_scalar(p, &operand, tok.loc, "'!'");
                    if(err) return err;
                    result_type = ccqt_basic(CCBT_int);
                    break;
                case CC_EXPR_DEREF:
                    if(ccqt_kind(operand->type) == CC_FUNCTION){
                        // decays back to same function
                        *out = operand;
                        return 0;
                    }
                    if(ccqt_kind(operand->type) == CC_ARRAY && !ccqt_as_array(operand->type)->is_vector){
                        CcQualType ptr_type;
                        err = cc_pointer_of(p, cc_array_element_type(operand->type), &ptr_type);
                        if(err) return err;
                        err = cc_implicit_cast(p, operand, ptr_type, &operand);
                        if(err) return err;
                    }
                    if(ccqt_kind(operand->type) == CC_SLICE)
                        return cc_error(p, tok.loc, "dereferencing non-pointer type");
                    err = cc_deref_type(p, operand->type, &result_type, tok.loc, 0);
                    if(err) return err;
                    break;
                case CC_EXPR_ADDR: {
                    CcExpr* val = operand;
                    while(val->kind == CC_EXPR_CAST){
                        val = val->lhs;
                    }
                    if(val->kind == CC_EXPR_VALUE && ccqt_kind(val->type) != CC_ARRAY){
                        // eg &3. Desugar to &(int){3}.
                        err = cc_wrap_to_desugared_compound_literal(p, operand, &operand);
                        if(err) return err;
                    }
                    if(operand->kind == CC_EXPR_COMPOUND_LITERAL){
                        err = cc_desugar_compound_literal(p, operand, &operand);
                        if(err) return err;
                    }
                    if(cc_expr_field_bit_width(operand))
                        return cc_error(p, tok.loc, "cannot take address of bitfield");
                    if(ccqt_kind(operand->type) == CC_FUNCTION){
                        err = cc_pointer_of(p, operand->type, &result_type);
                        if(err) return err;
                        return cc_implicit_cast(p, operand, result_type, out);
                    }
                    if(!operand->is_lvalue)
                        return cc_error(p, tok.loc, "cannot take address of rvalue");
                    if(vc >= CC_LINKTIME_VALUE && operand->kind == CC_EXPR_VARIABLE && operand->var->automatic)
                        return cc_error(p, tok.loc, "address of automatic variable in constant expression");
                    err = cc_pointer_of(p, operand->type, &result_type);
                    if(err) return err;
                    break;
                }
                case CC_EXPR_PREINC:
                case CC_EXPR_PREDEC:
                    if(operand->kind == CC_EXPR_COMPOUND_LITERAL){
                        err = cc_desugar_compound_literal(p, operand, &operand);
                        if(err) return err;
                    }
                    if(!operand->is_lvalue)
                        return cc_error(p, tok.loc, "expression is not an lvalue");
                    if(operand->type.is_const)
                        return cc_error(p, tok.loc, "cannot modify const-qualified variable");
                    err = cc_check_incdec_type(p, operand->type, tok.loc);
                    if(err) return err;
                    result_type = (CcQualType){.unqual=operand->type.unqual};
                    break;
                case CC_EXPR_VALUE:
                case CC_EXPR_SIZEOF_VMT:
                case CC_EXPR_VARIABLE:
                case CC_EXPR_FUNCTION:
                case CC_EXPR_COMPOUND_LITERAL:
                case CC_EXPR_INIT_LIST:
                case CC_EXPR_OBJECT_VIEW:
                case CC_EXPR_POSTINC:
                case CC_EXPR_POSTDEC:
                case CC_EXPR_ADD:
                case CC_EXPR_SUB:
                case CC_EXPR_MUL:
                case CC_EXPR_DIV:
                case CC_EXPR_MOD:
                case CC_EXPR_BITAND:
                case CC_EXPR_BITOR:
                case CC_EXPR_BITXOR:
                case CC_EXPR_LSHIFT:
                case CC_EXPR_RSHIFT:
                case CC_EXPR_LOGAND:
                case CC_EXPR_LOGOR:
                case CC_EXPR_EQ:
                case CC_EXPR_NE:
                case CC_EXPR_LT:
                case CC_EXPR_GT:
                case CC_EXPR_LE:
                case CC_EXPR_GE:
                case CC_EXPR_ASSIGN:
                case CC_EXPR_ADDASSIGN:
                case CC_EXPR_SUBASSIGN:
                case CC_EXPR_MULASSIGN:
                case CC_EXPR_DIVASSIGN:
                case CC_EXPR_MODASSIGN:
                case CC_EXPR_BITANDASSIGN:
                case CC_EXPR_BITORASSIGN:
                case CC_EXPR_BITXORASSIGN:
                case CC_EXPR_LSHIFTASSIGN:
                case CC_EXPR_RSHIFTASSIGN:
                case CC_EXPR_TERNARY:
                case CC_EXPR_CAST:
                case CC_EXPR_CALL:
                case CC_EXPR_SUBSCRIPT:
                case CC_EXPR_DOT:
                case CC_EXPR_ARROW:
                case CC_EXPR_COMMA:
                case CC_EXPR_STATEMENT_EXPRESSION:
                case CC_EXPR_ATOMIC:
                case CC_EXPR_VA:
                case CC_EXPR_BUILTIN:
                case CC_EXPR_ADD_OVERFLOW:
                case CC_EXPR_MUL_OVERFLOW:
                case CC_EXPR_SUB_OVERFLOW:
                case CC_EXPR_BIT_BUILTIN:
                case CC_EXPR_ALLOCA:
                case CC_EXPR_INTERN:
                case CC_EXPR_HOTSWAP:
                case CC_EXPR_SRCLOC_REFLECT:
                case CC_EXPR_COMPILE:
                case CC_EXPR_MODULE_REFLECT:
                case CC_EXPR_TYPE_INTROSPECTION:
                case CC_EXPR_UMUL128:
                case CC_EXPR_SLICE:
                case CC_EXPR_SLICE_ALL:
                case CC_EXPR_SLICE_LO:
                case CC_EXPR_SLICE_HI:
                case CC_EXPR_BSWAP:
                    return CC_UNREACHABLE_ERROR;
            }
            CcExpr* node = cc_make_expr(p, kind, tok.loc, result_type, 0);
            if(!node) return CC_OOM_ERROR;
            node->is_lvalue = kind == CC_EXPR_DEREF;
            node->lhs = operand;
            *out = node;
            return 0;
        }
    }
    cc_unget(p, &tok);
    CcExpr* primary;
    err = cc_parse_primary(p, vc, &primary);
    if(err) return err;
    return cc_parse_postfix(p, vc, primary, out);
}

static
int
cc_parse_primary(CcParser* p, CcValueClass vc, CcExpr* _Nullable* _Nonnull out){
    CcToken tok;
    int err = cc_next_token(p, &tok);
    if(err) return err;
    switch(tok.type){
        case CC_CONSTANT: {
            CcExpr* node = cc_make_expr(p, CC_EXPR_VALUE, tok.loc, (CcQualType){0}, 0);
            if(!node) return CC_OOM_ERROR;
            switch(tok.constant.ctype){
                case CC_FLOAT:
                    node->type.basic.kind = CCBT_float;
                    node->float_ = tok.constant.float_value;
                    break;
                case CC_DOUBLE:
                    node->type.basic.kind = CCBT_double;
                    node->double_ = tok.constant.double_value;
                    break;
                case CC_LONG_DOUBLE:
                    node->type.basic.kind = CCBT_long_double;
                    switch(p->cpp.target.long_double_format){
                        case CC_LONG_DOUBLE_BINARY64:
                            node->double_ = tok.constant.double_value;
                            break;
                        case CC_LONG_DOUBLE_X87:
                            node->x87 = tok.constant.x87_value;
                            break;
                        case CC_LONG_DOUBLE_BINARY128:
                            node->quad = tok.constant.quad_value;
                            break;
                    }
                    break;
                case CC_INT:
                    node->type.basic.kind = CCBT_int;
                    node->uinteger = tok.constant.integer_value;
                    break;
                case CC_UNSIGNED:
                    node->type.basic.kind = CCBT_unsigned;
                    node->uinteger = tok.constant.integer_value;
                    break;
                case CC_LONG:
                    node->type.basic.kind = CCBT_long;
                    node->uinteger = tok.constant.integer_value;
                    break;
                case CC_UNSIGNED_LONG:
                    node->type.basic.kind = CCBT_unsigned_long;
                    node->uinteger = tok.constant.integer_value;
                    break;
                case CC_LONG_LONG:
                    node->type.basic.kind = CCBT_long_long;
                    node->uinteger = tok.constant.integer_value;
                    break;
                case CC_UNSIGNED_LONG_LONG:
                    node->type.basic.kind = CCBT_unsigned_long_long;
                    node->uinteger = tok.constant.integer_value;
                    break;
                case CC_INT128:
                case CC_UNSIGNED_INT128:
                    node->type.basic.kind = tok.constant.ctype == CC_INT128 ? CCBT_int128 : CCBT_unsigned_int128;
                    node->uinteger128 = tok.constant.integer128_value;
                    break;
                case CC_WCHAR:
                    node->type.basic.kind = cc_target(p)->wchar_type;
                    node->uinteger = tok.constant.integer_value;
                    break;
                case CC_CHAR16:
                    node->type.basic.kind = cc_target(p)->char16_type;
                    node->uinteger = tok.constant.integer_value;
                    break;
                case CC_CHAR32:
                    node->type.basic.kind = cc_target(p)->char32_type;
                    node->uinteger = tok.constant.integer_value;
                    break;
                case CC_UCHAR:
                    node->type.basic.kind = CCBT_unsigned_char;
                    node->uinteger = tok.constant.integer_value;
                    break;
            }
            *out = node;
            return 0;
        }
        case CC_STRING_LITERAL: {
            CcExpr* node = cc_make_expr(p, CC_EXPR_VALUE, tok.loc, (CcQualType){0}, 0);
            if(!node) return CC_OOM_ERROR;
            node->str.length = tok.str.length;
            node->text = tok.str.utf8;
            CcBasicTypeKind elem_type;
            switch(tok.str.stype){
                case CC_STRING:   elem_type = CCBT_char; break;
                case CC_LSTRING:  elem_type = cc_target(p)->wchar_type; break;
                case CC_uSTRING:  elem_type = cc_target(p)->char16_type; break;
                case CC_USTRING:  elem_type = cc_target(p)->char32_type; break;
                case CC_U8STRING: elem_type = CCBT_unsigned_char; break;
                DRP_CASES_EXHAUSTED;
            }
            CcArray* sa = cc_intern_array(&p->type_cache, cc_allocator(p), ccqt_basic(elem_type), tok.str.length, 0, 0, 0, 0);
            if(!sa) return CC_OOM_ERROR;
            node->type = (CcQualType){.bits = (uintptr_t)sa};
            node->is_lvalue = 1;
            *out = node;
            return 0;
        }
        case CC_IDENTIFIER: {
            CcBuiltinFunc builtin = (CcBuiltinFunc)AM_get(&p->builtins, tok.ident.ident);
            switch(builtin){
                case CC_BUILTIN_NONE:
                    break;
                case CC__builtin_constant_p: {
                    err = cc_expect_punct(p, CC_lparen);
                    if(err) return err;
                    CcExpr* arg;
                    err = cc_parse_assignment_expr(p, CC_RUNTIME_VALUE, &arg, CCQT_NONE);
                    if(err) return err;
                    err = cc_expect_punct(p, CC_rparen);
                    if(err) return err;
                    CcExpr* ev;
                    err = cc_eval_expr(&(CcEvalCtx){.parser = p}, arg, &ev);
                    if(!err) cc_release_expr(p, ev);
                    if(err == CC_OVERFLOW_ERROR) err = CC_NOT_CONSTANT_ERROR;
                    if(err && err != CC_NOT_CONSTANT_ERROR) return err;
                    cc_release_expr(p, arg);
                    CcExpr* node = cc_int64_expr(p, tok.loc, ccqt_basic(CCBT_int), err?0:1);
                    if(!node) return CC_OOM_ERROR;
                    *out = node;
                    return 0;
                }
                case CC__builtin_offsetof:{
                    err = cc_expect_punct(p, CC_lparen);
                    if(err) return err;
                    CcQualType type;
                    err = cc_parse_type_name(p, &type, NULL);
                    if(err) return err;
                    err = cc_expect_punct(p, CC_comma);
                    if(err) return err;
                    uint64_t offset = 0;
                    CcQualType cur = type;
                    for(;;){
                        CcToken member;
                        err = cc_next_token(p, &member);
                        if(err) return err;
                        if(member.type != CC_IDENTIFIER)
                            return cc_error(p, member.loc, "expected member name in __builtin_offsetof");
                        CcTypeKind tk = ccqt_kind(cur);
                        uint64_t floc = 0;
                        CcQualType member_type = {0};
                        CcField* found = NULL;
                        if(tk == CC_STRUCT){
                            err = cc_lookup_field_offset(p, cur, member.ident.ident, &floc, &member_type, &found);
                            if(err) return err;
                        }
                        else if(tk == CC_UNION){
                            err = cc_lookup_field_offset(p, cur, member.ident.ident, &floc, &member_type, &found);
                            if(err) return err;
                        }
                        if(!found)
                            return cc_error(p, member.loc, "no member named '%s' in type", member.ident.ident->data);
                        offset += floc;
                        cur = member_type;
                        for(;;){
                            CcToken next;
                            err = cc_peek(p, &next);
                            if(err) return err;
                            if(next.type != CC_PUNCTUATOR || next.punct.punct != '[')
                                break;
                            cc_next_token(p, &next);
                            if(ccqt_kind(cur) != CC_ARRAY)
                                return cc_error(p, next.loc, "subscript in __builtin_offsetof requires array type");
                            CcArray* arr = ccqt_as_array(cur);
                            CcExpr* idx_expr = NULL;
                            err = cc_parse_assignment_expr(p, vc, &idx_expr, CCQT_NONE);
                            if(err) return err;
                            int64_t idx;
                            err = cc_eval_integer(&(CcEvalCtx){p}, idx_expr, &idx);
                            cc_release_expr(p, idx_expr);
                            if(err && err != CC_NOT_CONSTANT_ERROR) return err;
                            if(err)
                                return cc_error(p, next.loc, "array index in __builtin_offsetof must be a constant integer");
                            uint32_t elem_size;
                            err = cc_sizeof_as_uint(p, arr->element, next.loc, &elem_size);
                            if(err) return err;
                            offset += (uint64_t)idx * elem_size;
                            cur = arr->element;
                            err = cc_expect_punct(p, CC_rbracket);
                            if(err) return err;
                        }
                        CcToken next;
                        err = cc_peek(p, &next);
                        if(err) return err;
                        if(next.type == CC_PUNCTUATOR && next.punct.punct == '.'){
                            cc_next_token(p, &next);
                            continue;
                        }
                        break;
                    }
                    err = cc_expect_punct(p, CC_rparen);
                    if(err) return err;
                    CcQualType size_type = ccqt_basic(cc_target(p)->size_type);
                    CcExpr* node = cc_value_expr(p, tok.loc, size_type);
                    if(!node) return CC_OOM_ERROR;
                    node->uinteger = offset;
                    *out = node;
                    return 0;
                }
                case CC__builtin_types_compatible_p:{
                    err = cc_expect_punct(p, CC_lparen);
                    if(err) return err;
                    CcQualType type1, type2;
                    err = cc_parse_type_name(p, &type1, NULL);
                    if(err) return err;
                    err = cc_expect_punct(p, CC_comma);
                    if(err) return err;
                    err = cc_parse_type_name(p, &type2, NULL);
                    if(err) return err;
                    err = cc_expect_punct(p, CC_rparen);
                    if(err) return err;
                    type1 = (CcQualType){.unqual=type1.unqual};
                    type2 = (CcQualType){.unqual=type2.unqual};
                    _Bool is_same = type1.bits == type2.bits;
                    if(!is_same && ccqt_kind(type1) == CC_ARRAY && ccqt_kind(type2) == CC_ARRAY){
                        CcArray* a = ccqt_as_array(type1);
                        CcArray* b = ccqt_as_array(type2);
                        if(!a->is_vector && !b->is_vector && a->element.unqual == b->element.unqual){
                            if(a->is_incomplete || b->is_incomplete || a->length == b->length){
                                is_same = 1;
                            }
                        }
                    }
                    // TODO: function types

                    CcExpr* node = cc_value_expr(p, tok.loc, ccqt_basic(CCBT_int));
                    if(!node) return CC_OOM_ERROR;
                    node->integer = is_same;
                    *out = node;
                    return 0;
                }
                case CC__builtin_choose_expr:{
                    err = cc_expect_punct(p, CC_lparen);
                    if(err) return err;
                    CcExpr* cond;
                    err = cc_parse_assignment_expr(p, CC_CONSTEXPR_VALUE, &cond, CCQT_NONE);
                    if(err) return err;
                    _Bool b;
                    err = cc_eval_truthy(&(CcEvalCtx){p}, cond, &b);
                    if(err) return err;
                    err = cc_expect_punct(p, CC_comma);
                    if(err) return err;
                    CcExpr* result;
                    if(b){
                        err = cc_parse_assignment_expr(p, vc, &result, CCQT_NONE);
                        if(err) return err;
                        err = cc_expect_punct(p, CC_comma);
                        if(err) return err;
                        err = cc_skip_to_next_comma_or_paren(p, "__builtin_choose_expr");
                        if(err) return err;
                    }
                    else {
                        err = cc_skip_to_next_comma_or_paren(p, "__builtin_choose_expr");
                        if(err) return err;
                        err = cc_expect_punct(p, CC_comma);
                        if(err) return err;
                        err = cc_parse_assignment_expr(p, vc, &result, CCQT_NONE);
                        if(err) return err;
                    }
                    err = cc_expect_punct(p, CC_rparen);
                    if(err) return err;
                    *out = result;
                    return 0;
                }
                case CC__func__:{
                    Atom name = p->current_func ? p->current_func->name : NULL;
                    const char* s = name ? name->data : "";
                    uint32_t len = name ? name->length : 0;
                    CcArray* sa = cc_intern_array(&p->type_cache, cc_allocator(p), ccqt_basic(CCBT_char), len + 1, 0, 0, 0, 0);
                    if(!sa) return CC_OOM_ERROR;
                    CcQualType type = {.bits = (uintptr_t)sa};
                    CcExpr* node = cc_value_expr(p, tok.loc, type);
                    if(!node) return CC_OOM_ERROR;
                    node->str.length = len + 1;
                    node->text = s;
                    *out = node;
                    return 0;
                }
                case CC__atomic_load: {
                    err = cc_expect_punct(p, '(');
                    if(err) return err;
                    CcExpr* ptr_expr;
                    err = cc_parse_assignment_expr(p, vc, &ptr_expr, CCQT_NONE);
                    if(err) return err;
                    CcQualType ptr_type = ptr_expr->type;
                    if(ccqt_kind(ptr_type) != CC_POINTER)
                        return cc_error(p, tok.loc, "first argument to __atomic_load must be a pointer");
                    CcQualType pointee = ccqt_as_ptr(ptr_type)->pointee;
                    err = cc_expect_punct(p, ',');
                    if(err) return err;
                    CcExpr* ret_expr;
                    err = cc_parse_assignment_expr(p, vc, &ret_expr, CCQT_NONE);
                    if(err) return err;
                    err = cc_expect_punct(p, ',');
                    if(err) return err;
                    // memorder (discard)
                    CcExpr* mo;
                    err = cc_parse_assignment_expr(p, vc, &mo, CCQT_NONE);
                    if(err) return err;
                    cc_release_expr(p, mo);
                    err = cc_expect_punct(p, ')');
                    if(err) return err;
                    uint32_t pointee_sz;
                    err = cc_sizeof_as_uint(p, pointee, tok.loc, &pointee_sz);
                    if(err) return err;
                    CcExpr* node = cc_make_expr(p, CC_EXPR_ATOMIC, tok.loc, ccqt_basic(CCBT_void), 2);
                    if(!node) return CC_OOM_ERROR;
                    node->atomic.op = CC_ATOMIC_LOAD;
                    node->lhs = ptr_expr;
                    node->values[0] = ret_expr;
                    *out = node;
                    return 0;
                }
                case CC__atomic_store: {
                    err = cc_expect_punct(p, '(');
                    if(err) return err;
                    CcExpr* ptr_expr;
                    err = cc_parse_assignment_expr(p, vc, &ptr_expr, CCQT_NONE);
                    if(err) return err;
                    if(ccqt_kind(ptr_expr->type) != CC_POINTER)
                        return cc_error(p, tok.loc, "first argument to __atomic_store must be a pointer");
                    err = cc_expect_punct(p, ',');
                    if(err) return err;
                    CcExpr* val_expr;
                    err = cc_parse_assignment_expr(p, vc, &val_expr, CCQT_NONE);
                    if(err) return err;
                    err = cc_expect_punct(p, ',');
                    if(err) return err;
                    CcExpr* mo;
                    err = cc_parse_assignment_expr(p, vc, &mo, CCQT_NONE);
                    if(err) return err;
                    cc_release_expr(p, mo);
                    err = cc_expect_punct(p, ')');
                    if(err) return err;
                    CcExpr* node = cc_make_expr(p, CC_EXPR_ATOMIC, tok.loc, ccqt_basic(CCBT_void), 1);
                    if(!node) return CC_OOM_ERROR;
                    node->atomic.op = CC_ATOMIC_STORE;
                    node->lhs = ptr_expr;
                    node->values[0] = val_expr;
                    *out = node;
                    return 0;
                }
                case CC__atomic_exchange: {
                    err = cc_expect_punct(p, '(');
                    if(err) return err;
                    CcExpr* ptr_expr;
                    err = cc_parse_assignment_expr(p, vc, &ptr_expr, CCQT_NONE);
                    if(err) return err;
                    if(ccqt_kind(ptr_expr->type) != CC_POINTER)
                        return cc_error(p, tok.loc, "first argument to __atomic_exchange must be a pointer");
                    err = cc_expect_punct(p, ',');
                    if(err) return err;
                    CcExpr* val_expr;
                    err = cc_parse_assignment_expr(p, vc, &val_expr, CCQT_NONE);
                    if(err) return err;
                    err = cc_expect_punct(p, ',');
                    if(err) return err;
                    CcExpr* ret_expr;
                    err = cc_parse_assignment_expr(p, vc, &ret_expr, CCQT_NONE);
                    if(err) return err;
                    err = cc_expect_punct(p, ',');
                    if(err) return err;
                    CcExpr* mo;
                    err = cc_parse_assignment_expr(p, vc, &mo, CCQT_NONE);
                    if(err) return err;
                    cc_release_expr(p, mo);
                    err = cc_expect_punct(p, ')');
                    if(err) return err;
                    CcExpr* node = cc_make_expr(p, CC_EXPR_ATOMIC, tok.loc, ccqt_basic(CCBT_void), 2);
                    if(!node) return CC_OOM_ERROR;
                    node->atomic.op = CC_ATOMIC_EXCHANGE;
                    node->lhs = ptr_expr;
                    node->values[0] = val_expr;
                    node->values[1] = ret_expr;
                    *out = node;
                    return 0;
                }
                case CC__atomic_compare_exchange:{
                    CcAtomicOp op;
                    op = CC_ATOMIC_COMPARE_EXCHANGE;
                    goto atomic_op;
                case CC__atomic_fetch_add:
                    op = CC_ATOMIC_FETCH_ADD;
                    goto atomic_op;
                case CC__atomic_fetch_sub:
                    op = CC_ATOMIC_FETCH_SUB;
                    goto atomic_op;
                case CC__atomic_add_fetch:
                    op = CC_ATOMIC_ADD_FETCH;
                    goto atomic_op;
                case CC__atomic_sub_fetch:
                    op = CC_ATOMIC_SUB_FETCH;
                    goto atomic_op;
                case CC__atomic_fetch_and:
                    op = CC_ATOMIC_FETCH_AND;
                    goto atomic_op;
                case CC__atomic_fetch_or:
                    op = CC_ATOMIC_FETCH_OR;
                    goto atomic_op;
                case CC__atomic_fetch_xor:
                    op = CC_ATOMIC_FETCH_XOR;
                    goto atomic_op;
                case CC__atomic_load_n:
                    op = CC_ATOMIC_LOAD_N;
                    goto atomic_op;
                case CC__atomic_store_n:
                    op = CC_ATOMIC_STORE_N;
                    goto atomic_op;
                case CC__atomic_exchange_n:
                    op = CC_ATOMIC_EXCHANGE_N;
                    goto atomic_op;
                case CC__atomic_compare_exchange_n:
                    op = CC_ATOMIC_COMPARE_EXCHANGE_N;
                    goto atomic_op;
                atomic_op:; {
                    err = cc_expect_punct(p, '(');
                    if(err) return err;
                    CcExpr* ptr_expr;
                    err = cc_parse_assignment_expr(p, vc, &ptr_expr, CCQT_NONE);
                    if(err) return err;
                    CcQualType ptr_type = ptr_expr->type;
                    if(ccqt_kind(ptr_type) != CC_POINTER)
                        return cc_error(p, tok.loc, "first argument to atomic builtin must be a pointer");
                    CcQualType pointee = ccqt_as_ptr(ptr_type)->pointee;
                    uint32_t pointee_sz;
                    err = cc_sizeof_as_uint(p, pointee, tok.loc, &pointee_sz);
                    if(err) return err;
                    if(!pointee_sz || (pointee_sz & (pointee_sz - 1)))
                        return cc_error(p, tok.loc, "atomic operand size %u is not a power of 2", pointee_sz);
                    if(pointee_sz > cc_target(p)->atomic_lock_free_max)
                        return cc_error(p, tok.loc, "atomic operand size %u exceeds target's maximum lock-free size %u", pointee_sz, cc_target(p)->atomic_lock_free_max);
                    if((op == CC_ATOMIC_FETCH_ADD || op == CC_ATOMIC_FETCH_SUB || op == CC_ATOMIC_ADD_FETCH || op == CC_ATOMIC_SUB_FETCH || op == CC_ATOMIC_FETCH_AND || op == CC_ATOMIC_FETCH_OR || op == CC_ATOMIC_FETCH_XOR) && pointee_sz > 8)
                        return cc_error(p, tok.loc, "atomic arithmetic not supported for operand size %u", pointee_sz);
                    CcQualType result_type;
                    CcQualType arg_types[2];
                    CcQualType unatomic_pointee = pointee;
                    unatomic_pointee.is_atomic = 0;
                    _Bool pointer_fetch_addsub = (op == CC_ATOMIC_FETCH_ADD || op == CC_ATOMIC_FETCH_SUB || op == CC_ATOMIC_ADD_FETCH || op == CC_ATOMIC_SUB_FETCH)
                                               && ccqt_kind(unatomic_pointee) == CC_POINTER;
                    CcQualType rmw_base = unatomic_pointee;
                    if(!ccqt_is_basic(rmw_base) && ccqt_kind(rmw_base) == CC_ENUM)
                        rmw_base = ccqt_as_enum(rmw_base)->underlying;
                    _Bool integer_rmw_base = ccqt_is_basic(rmw_base) && ccbt_is_integer(rmw_base.basic.kind);
                    if((op == CC_ATOMIC_FETCH_ADD || op == CC_ATOMIC_FETCH_SUB || op == CC_ATOMIC_ADD_FETCH || op == CC_ATOMIC_SUB_FETCH) && !pointer_fetch_addsub && !integer_rmw_base)
                        return cc_error(p, tok.loc, "atomic fetch add/sub requires integer or pointer type");
                    if((op == CC_ATOMIC_FETCH_AND || op == CC_ATOMIC_FETCH_OR || op == CC_ATOMIC_FETCH_XOR) && !integer_rmw_base)
                        return cc_error(p, tok.loc, "atomic fetch bitwise operation requires integer type");
                    int nargs, nconst;
                    switch(op){
                        case CC_ATOMIC_LOAD_N:
                            result_type = pointee;
                            nargs = 0;
                            nconst = 1;
                            break;
                        case CC_ATOMIC_STORE_N:
                            result_type = ccqt_basic(CCBT_void);
                            arg_types[0] = pointee;
                            nargs = 1;
                            nconst = 1;
                            break;
                        case CC_ATOMIC_FETCH_ADD:
                        case CC_ATOMIC_FETCH_SUB:
                        case CC_ATOMIC_ADD_FETCH:
                        case CC_ATOMIC_SUB_FETCH:
                        case CC_ATOMIC_FETCH_AND:
                        case CC_ATOMIC_FETCH_OR:
                        case CC_ATOMIC_FETCH_XOR:
                        case CC_ATOMIC_EXCHANGE_N:
                            result_type = pointee;
                            arg_types[0] = pointer_fetch_addsub ? ccqt_basic(cc_target(p)->ptrdiff_type) : pointee;
                            nargs = 1;
                            nconst = 1;
                            break;
                        case CC_ATOMIC_COMPARE_EXCHANGE_N:
                            result_type = ccqt_basic(CCBT_bool);
                            arg_types[0] = ptr_type;
                            arg_types[1] = pointee;
                            nargs = 2;
                            nconst = 3;
                            break;
                        case CC_ATOMIC_COMPARE_EXCHANGE:
                            result_type = ccqt_basic(CCBT_bool);
                            arg_types[0] = ptr_type;
                            arg_types[1] = ptr_type;
                            nargs = 2;
                            nconst = 3;
                            break;
                        case CC_ATOMIC_LOAD:
                        case CC_ATOMIC_STORE:
                        case CC_ATOMIC_EXCHANGE:
                        case CC_ATOMIC_THREAD_FENCE:
                        case CC_ATOMIC_SIGNAL_FENCE:
                        case CC_ATOMIC_INTERLOCKED_COMPARE_EXCHANGE:
                        case CC_ATOMIC_INTERLOCKED_COMPARE_EXCHANGE128:
                        case CC_ATOMIC_INTERLOCKED_INCREMENT:
                        case CC_ATOMIC_INTERLOCKED_DECREMENT:
                            return CC_UNREACHABLE_ERROR;
                    }
                    CcExpr* node = cc_make_expr(p, CC_EXPR_ATOMIC, tok.loc, result_type, nargs);
                    if(!node) return CC_OOM_ERROR;
                    node->atomic.op = op;
                    node->lhs = ptr_expr;
                    for(int i = 0; i < nargs; i++){
                        err = cc_expect_punct(p, ',');
                        if(err) return err;
                        err = cc_parse_assignment_expr(p, vc, &node->values[i], CCQT_NONE);
                        if(err) return err;
                        err = cc_implicit_cast(p, node->values[i], arg_types[i], &node->values[i]);
                        if(err) return err;
                    }
                    unsigned const_vals[3];
                    for(int i = 0; i < nconst; i++){
                        err = cc_expect_punct(p, ',');
                        if(err) return err;
                        CcExpr* const_expr;
                        err = cc_parse_assignment_expr(p, vc, &const_expr, CCQT_NONE);
                        if(err) return err;
                        int64_t ev;
                        err = cc_eval_integer(&(CcEvalCtx){p}, const_expr, &ev);
                        SrcLoc eloc = const_expr->loc;
                        cc_release_expr(p, const_expr);
                        if(err && err != CC_NOT_CONSTANT_ERROR) return err;
                        if(err)
                            return cc_error(p, eloc, "memory order must be a constant expression");
                        const_vals[i] = (unsigned)ev;
                        if(const_vals[i] >= CC_MO_COUNT)
                            return cc_error(p, eloc, "invalid memory order value %u", const_vals[i]);
                    }
                    if(op == CC_ATOMIC_COMPARE_EXCHANGE || op == CC_ATOMIC_COMPARE_EXCHANGE_N){
                        err = cc_check_atomic_memory_order(p, op, const_vals[1], tok.loc, 0);
                        if(err) return err;
                        err = cc_check_atomic_memory_order(p, op, const_vals[2], tok.loc, 1);
                        if(err) return err;
                        node->atomic.weak = const_vals[0];
                        node->atomic.memorder = const_vals[1];
                        node->atomic.fail_memorder = const_vals[2];
                    }
                    else {
                        err = cc_check_atomic_memory_order(p, op, const_vals[0], tok.loc, 0);
                        if(err) return err;
                        node->atomic.memorder = const_vals[0];
                    }
                    err = cc_expect_punct(p, ')');
                    if(err) return err;
                    *out = node;
                    return 0;
                }
                }
                case CC__atomic_thread_fence:
                case CC__atomic_signal_fence: {
                    enum CcAtomicOp op = (builtin == CC__atomic_thread_fence) ? CC_ATOMIC_THREAD_FENCE : CC_ATOMIC_SIGNAL_FENCE;
                    err = cc_expect_punct(p, '(');
                    if(err) return err;
                    CcExpr* const_expr;
                    err = cc_parse_assignment_expr(p, vc, &const_expr, CCQT_NONE);
                    if(err) return err;
                    int64_t ev;
                    err = cc_eval_integer(&(CcEvalCtx){p}, const_expr, &ev);
                    if(err && err != CC_NOT_CONSTANT_ERROR) return err;
                    if(err)
                        return cc_error(p, const_expr->loc, "memory order must be a constant expression");
                    unsigned order = (unsigned)ev;
                    if(order >= CC_MO_COUNT)
                        return cc_error(p, const_expr->loc, "invalid memory order value %u", order);
                    cc_release_expr(p, const_expr);
                    err = cc_expect_punct(p, ')');
                    if(err) return err;
                    CcExpr* node = cc_make_expr(p, CC_EXPR_ATOMIC, tok.loc, ccqt_basic(CCBT_void), 0);
                    if(!node) return CC_OOM_ERROR;
                    node->atomic.op = op;
                    node->atomic.memorder = order;
                    *out = node;
                    return 0;
                }
                case CC_InterlockedExchange:
                case CC_InterlockedExchange8:
                case CC_InterlockedExchange16:
                case CC_InterlockedExchange64:
                case CC_InterlockedCompareExchange:
                case CC_InterlockedCompareExchange8:
                case CC_InterlockedCompareExchange16:
                case CC_InterlockedCompareExchange64:
                case CC_InterlockedExchangeAdd:
                case CC_InterlockedExchangeAdd8:
                case CC_InterlockedExchangeAdd16:
                case CC_InterlockedExchangeAdd64:
                case CC_InterlockedAnd:
                case CC_InterlockedAnd8:
                case CC_InterlockedAnd16:
                case CC_InterlockedAnd64:
                case CC_InterlockedOr:
                case CC_InterlockedOr8:
                case CC_InterlockedOr16:
                case CC_InterlockedOr64:
                case CC_InterlockedXor:
                case CC_InterlockedXor8:
                case CC_InterlockedXor16:
                case CC_InterlockedXor64: {
                    enum CcAtomicOp op;
                    int nargs;
                    CcBasicTypeKind val_kind;
                    switch(builtin){
                        case CC_InterlockedExchange:      op = CC_ATOMIC_EXCHANGE_N; nargs = 1; val_kind = CCBT_long; break;
                        case CC_InterlockedExchange8:     op = CC_ATOMIC_EXCHANGE_N; nargs = 1; val_kind = CCBT_char; break;
                        case CC_InterlockedExchange16:    op = CC_ATOMIC_EXCHANGE_N; nargs = 1; val_kind = CCBT_short; break;
                        case CC_InterlockedExchange64:    op = CC_ATOMIC_EXCHANGE_N; nargs = 1; val_kind = CCBT_long_long; break;
                        case CC_InterlockedCompareExchange:   op = CC_ATOMIC_INTERLOCKED_COMPARE_EXCHANGE; nargs = 2; val_kind = CCBT_long; break;
                        case CC_InterlockedCompareExchange8:  op = CC_ATOMIC_INTERLOCKED_COMPARE_EXCHANGE; nargs = 2; val_kind = CCBT_char; break;
                        case CC_InterlockedCompareExchange16: op = CC_ATOMIC_INTERLOCKED_COMPARE_EXCHANGE; nargs = 2; val_kind = CCBT_short; break;
                        case CC_InterlockedCompareExchange64: op = CC_ATOMIC_INTERLOCKED_COMPARE_EXCHANGE; nargs = 2; val_kind = CCBT_long_long; break;
                        case CC_InterlockedExchangeAdd:   op = CC_ATOMIC_FETCH_ADD; nargs = 1; val_kind = CCBT_long; break;
                        case CC_InterlockedExchangeAdd8:  op = CC_ATOMIC_FETCH_ADD; nargs = 1; val_kind = CCBT_char; break;
                        case CC_InterlockedExchangeAdd16: op = CC_ATOMIC_FETCH_ADD; nargs = 1; val_kind = CCBT_short; break;
                        case CC_InterlockedExchangeAdd64: op = CC_ATOMIC_FETCH_ADD; nargs = 1; val_kind = CCBT_long_long; break;
                        case CC_InterlockedAnd:   op = CC_ATOMIC_FETCH_AND; nargs = 1; val_kind = CCBT_long; break;
                        case CC_InterlockedAnd8:  op = CC_ATOMIC_FETCH_AND; nargs = 1; val_kind = CCBT_char; break;
                        case CC_InterlockedAnd16: op = CC_ATOMIC_FETCH_AND; nargs = 1; val_kind = CCBT_short; break;
                        case CC_InterlockedAnd64: op = CC_ATOMIC_FETCH_AND; nargs = 1; val_kind = CCBT_long_long; break;
                        case CC_InterlockedOr:    op = CC_ATOMIC_FETCH_OR;  nargs = 1; val_kind = CCBT_long; break;
                        case CC_InterlockedOr8:   op = CC_ATOMIC_FETCH_OR;  nargs = 1; val_kind = CCBT_char; break;
                        case CC_InterlockedOr16:  op = CC_ATOMIC_FETCH_OR;  nargs = 1; val_kind = CCBT_short; break;
                        case CC_InterlockedOr64:  op = CC_ATOMIC_FETCH_OR;  nargs = 1; val_kind = CCBT_long_long; break;
                        case CC_InterlockedXor:   op = CC_ATOMIC_FETCH_XOR; nargs = 1; val_kind = CCBT_long; break;
                        case CC_InterlockedXor8:  op = CC_ATOMIC_FETCH_XOR; nargs = 1; val_kind = CCBT_char; break;
                        case CC_InterlockedXor16: op = CC_ATOMIC_FETCH_XOR; nargs = 1; val_kind = CCBT_short; break;
                        case CC_InterlockedXor64: op = CC_ATOMIC_FETCH_XOR; nargs = 1; val_kind = CCBT_long_long; break;
                        default: return CC_UNREACHABLE_ERROR;
                    }
                    CcQualType val_type = ccqt_basic(val_kind);
                    err = cc_expect_punct(p, '(');
                    if(err) return err;
                    CcExpr* ptr_expr;
                    err = cc_parse_assignment_expr(p, vc, &ptr_expr, CCQT_NONE);
                    if(err) return err;
                    if(ccqt_kind(ptr_expr->type) == CC_ARRAY && !ccqt_as_array(ptr_expr->type)->is_vector){
                        CcQualType t;
                        err = cc_pointer_of(p, cc_array_element_type(ptr_expr->type), &t);
                        if(err) return err;
                        err = cc_implicit_cast(p, ptr_expr, t, &ptr_expr);
                        if(err) return err;
                    }
                    else if(ccqt_kind(ptr_expr->type) != CC_POINTER)
                        return cc_error(p, tok.loc, "first argument to interlocked builtin must be a pointer");
                    CcExpr* node = cc_make_expr(p, CC_EXPR_ATOMIC, tok.loc, val_type, nargs);
                    if(!node) return CC_OOM_ERROR;
                    node->atomic.op = op;
                    node->atomic.memorder = CC_MO_SEQ_CST;
                    node->lhs = ptr_expr;
                    for(int i = 0; i < nargs; i++){
                        err = cc_expect_punct(p, ',');
                        if(err) return err;
                        err = cc_parse_assignment_expr(p, vc, &node->values[i], CCQT_NONE);
                        if(err) return err;
                        err = cc_implicit_cast(p, node->values[i], val_type, &node->values[i]);
                        if(err) return err;
                    }
                    err = cc_expect_punct(p, ')');
                    if(err) return err;
                    *out = node;
                    return 0;
                }
                case CC_InterlockedIncrement:
                case CC_InterlockedIncrement16:
                case CC_InterlockedIncrement64:
                case CC_InterlockedDecrement:
                case CC_InterlockedDecrement16:
                case CC_InterlockedDecrement64: {
                    enum CcAtomicOp op;
                    CcBasicTypeKind val_kind;
                    switch(builtin){
                        case CC_InterlockedIncrement:   op = CC_ATOMIC_INTERLOCKED_INCREMENT; val_kind = CCBT_long; break;
                        case CC_InterlockedIncrement16: op = CC_ATOMIC_INTERLOCKED_INCREMENT; val_kind = CCBT_short; break;
                        case CC_InterlockedIncrement64: op = CC_ATOMIC_INTERLOCKED_INCREMENT; val_kind = CCBT_long_long; break;
                        case CC_InterlockedDecrement:   op = CC_ATOMIC_INTERLOCKED_DECREMENT; val_kind = CCBT_long; break;
                        case CC_InterlockedDecrement16: op = CC_ATOMIC_INTERLOCKED_DECREMENT; val_kind = CCBT_short; break;
                        case CC_InterlockedDecrement64: op = CC_ATOMIC_INTERLOCKED_DECREMENT; val_kind = CCBT_long_long; break;
                        default: return CC_UNREACHABLE_ERROR;
                    }
                    err = cc_expect_punct(p, '(');
                    if(err) return err;
                    CcExpr* ptr_expr;
                    err = cc_parse_assignment_expr(p, vc, &ptr_expr, CCQT_NONE);
                    if(err) return err;
                    if(ccqt_kind(ptr_expr->type) == CC_ARRAY && !ccqt_as_array(ptr_expr->type)->is_vector){
                        CcQualType t;
                        err = cc_pointer_of(p, cc_array_element_type(ptr_expr->type), &t);
                        if(err) return err;
                        err = cc_implicit_cast(p, ptr_expr, t, &ptr_expr);
                        if(err) return err;
                    }
                    else if(ccqt_kind(ptr_expr->type) != CC_POINTER)
                        return cc_error(p, tok.loc, "first argument to interlocked builtin must be a pointer");
                    err = cc_expect_punct(p, ')');
                    if(err) return err;
                    CcExpr* node = cc_make_expr(p, CC_EXPR_ATOMIC, tok.loc, ccqt_basic(val_kind), 0);
                    if(!node) return CC_OOM_ERROR;
                    node->atomic.op = op;
                    node->atomic.memorder = CC_MO_SEQ_CST;
                    node->lhs = ptr_expr;
                    *out = node;
                    return 0;
                }
                case CC_InterlockedCompareExchange128: {
                    err = cc_expect_punct(p, '(');
                    if(err) return err;
                    CcExpr* ptr_expr;
                    err = cc_parse_assignment_expr(p, vc, &ptr_expr, CCQT_NONE);
                    if(err) return err;
                    if(ccqt_kind(ptr_expr->type) == CC_ARRAY && !ccqt_as_array(ptr_expr->type)->is_vector){
                        CcQualType t;
                        err = cc_pointer_of(p, cc_array_element_type(ptr_expr->type), &t);
                        if(err) return err;
                        err = cc_implicit_cast(p, ptr_expr, t, &ptr_expr);
                        if(err) return err;
                    }
                    else if(ccqt_kind(ptr_expr->type) != CC_POINTER)
                        return cc_error(p, tok.loc, "first argument to _InterlockedCompareExchange128 must be a pointer");
                    CcQualType ll_type = ccqt_basic(CCBT_long_long);
                    CcQualType ll_ptr_type;
                    err = cc_pointer_of(p, ll_type, &ll_ptr_type);
                    if(err) return err;
                    CcExpr* node = cc_make_expr(p, CC_EXPR_ATOMIC, tok.loc, ccqt_basic(CCBT_unsigned_char), 3);
                    if(!node) return CC_OOM_ERROR;
                    node->atomic.op = CC_ATOMIC_INTERLOCKED_COMPARE_EXCHANGE128;
                    node->atomic.memorder = CC_MO_SEQ_CST;
                    node->lhs = ptr_expr;
                    err = cc_expect_punct(p, ',');
                    if(err) return err;
                    err = cc_parse_assignment_expr(p, vc, &node->values[0], CCQT_NONE);
                    if(err) return err;
                    err = cc_implicit_cast(p, node->values[0], ll_type, &node->values[0]);
                    if(err) return err;
                    err = cc_expect_punct(p, ',');
                    if(err) return err;
                    err = cc_parse_assignment_expr(p, vc, &node->values[1], CCQT_NONE);
                    if(err) return err;
                    err = cc_implicit_cast(p, node->values[1], ll_type, &node->values[1]);
                    if(err) return err;
                    err = cc_expect_punct(p, ',');
                    if(err) return err;
                    err = cc_parse_assignment_expr(p, vc, &node->values[2], CCQT_NONE);
                    if(err) return err;
                    err = cc_implicit_cast(p, node->values[2], ll_ptr_type, &node->values[2]);
                    if(err) return err;
                    err = cc_expect_punct(p, ')');
                    if(err) return err;
                    *out = node;
                    return 0;
                }
                case CC__umul128: {
                    CcQualType ull_type = ccqt_basic(CCBT_unsigned_long_long);
                    CcQualType ull_ptr_type;
                    err = cc_pointer_of(p, ull_type, &ull_ptr_type);
                    if(err) return err;
                    err = cc_expect_punct(p, '(');
                    if(err) return err;
                    CcExpr* a;
                    err = cc_parse_assignment_expr(p, vc, &a, CCQT_NONE);
                    if(err) return err;
                    err = cc_implicit_cast(p, a, ull_type, &a);
                    if(err) return err;
                    err = cc_expect_punct(p, ',');
                    if(err) return err;
                    CcExpr* b;
                    err = cc_parse_assignment_expr(p, vc, &b, CCQT_NONE);
                    if(err) return err;
                    err = cc_implicit_cast(p, b, ull_type, &b);
                    if(err) return err;
                    err = cc_expect_punct(p, ',');
                    if(err) return err;
                    CcExpr* high_ptr;
                    err = cc_parse_assignment_expr(p, vc, &high_ptr, CCQT_NONE);
                    if(err) return err;
                    err = cc_implicit_cast(p, high_ptr, ull_ptr_type, &high_ptr);
                    if(err) return err;
                    err = cc_expect_punct(p, ')');
                    if(err) return err;
                    CcExpr* node = cc_make_expr(p, CC_EXPR_UMUL128, tok.loc, ull_type, 2);
                    if(!node) return CC_OOM_ERROR;
                    node->lhs = a;
                    node->values[0] = b;
                    node->values[1] = high_ptr;
                    *out = node;
                    return 0;
                }
                case CC__builtin_va_start: {
                    err = cc_expect_punct(p, '(');
                    if(err) return err;
                    CcExpr* ap_expr;
                    err = cc_parse_assignment_expr(p, vc, &ap_expr, CCQT_NONE);
                    if(err) return err;
                    err = cc_va_list_to_ptr(p, tok.loc, &ap_expr);
                    if(err) return err;
                    CcToken peek;
                    err = cc_peek(p, &peek);
                    if(err) return err;
                    if(peek.type == CC_PUNCTUATOR && peek.punct.punct == ','){
                        err = cc_next_token(p, &peek); // consume comma
                        if(err) return err;
                        CcExpr* ignored;
                        err = cc_parse_assignment_expr(p, vc, &ignored, CCQT_NONE);
                        if(err) return err;
                        cc_release_expr(p, ignored);
                    }
                    err = cc_expect_punct(p, ')');
                    if(err) return err;
                    CcExpr* node = cc_make_expr(p, CC_EXPR_VA, tok.loc, ccqt_basic(CCBT_void), 0);
                    if(!node) return CC_OOM_ERROR;
                    node->va.op = CC_VA_START;
                    node->lhs = ap_expr;
                    *out = node;
                    return 0;
                }
                case CC__builtin_va_end: {
                    err = cc_expect_punct(p, '(');
                    if(err) return err;
                    CcExpr* ap_expr;
                    err = cc_parse_assignment_expr(p, vc, &ap_expr, CCQT_NONE);
                    if(err) return err;
                    err = cc_expect_punct(p, ')');
                    if(err) return err;
                    err = cc_va_list_to_ptr(p, tok.loc, &ap_expr);
                    if(err) return err;
                    CcExpr* node = cc_make_expr(p, CC_EXPR_VA, tok.loc, ccqt_basic(CCBT_void), 0);
                    if(!node) return CC_OOM_ERROR;
                    node->va.op = CC_VA_END;
                    node->lhs = ap_expr;
                    *out = node;
                    return 0;
                }
                case CC__builtin_va_arg: {
                    err = cc_expect_punct(p, '(');
                    if(err) return err;
                    CcExpr* ap_expr;
                    err = cc_parse_assignment_expr(p, vc, &ap_expr, CCQT_NONE);
                    if(err) return err;
                    err = cc_va_list_to_ptr(p, tok.loc, &ap_expr);
                    if(err) return err;
                    err = cc_expect_punct(p, ',');
                    if(err) return err;
                    CcQualType arg_type;
                    err = cc_parse_type_name(p, &arg_type, NULL);
                    if(err) return err;
                    err = cc_expect_punct(p, ')');
                    if(err) return err;
                    CcExpr* node = cc_make_expr(p, CC_EXPR_VA, tok.loc, arg_type, 0);
                    if(!node) return CC_OOM_ERROR;
                    node->va.op = CC_VA_ARG;
                    node->lhs = ap_expr;
                    *out = node;
                    return 0;
                }
                case CC__builtin_va_copy: {
                    err = cc_expect_punct(p, '(');
                    if(err) return err;
                    CcExpr* dest_expr;
                    err = cc_parse_assignment_expr(p, vc, &dest_expr, CCQT_NONE);
                    if(err) return err;
                    err = cc_va_list_to_ptr(p, tok.loc, &dest_expr);
                    if(err) return err;
                    err = cc_expect_punct(p, ',');
                    if(err) return err;
                    CcExpr* src_expr;
                    err = cc_parse_assignment_expr(p, vc, &src_expr, CCQT_NONE);
                    if(err) return err;
                    err = cc_expect_punct(p, ')');
                    if(err) return err;
                    err = cc_va_list_to_ptr(p, tok.loc, &src_expr);
                    if(err) return err;
                    CcExpr* node = cc_make_expr(p, CC_EXPR_VA, tok.loc, ccqt_basic(CCBT_void), 1);
                    if(!node) return CC_OOM_ERROR;
                    node->va.op = CC_VA_COPY;
                    node->lhs = dest_expr;
                    node->values[0] = src_expr;
                    *out = node;
                    return 0;
                }
                case CC__builtin_expect:{
                    err = cc_expect_punct(p, '(');
                    if(err) return err;
                    err = cc_parse_assignment_expr(p, vc, out, CCQT_NONE);
                    if(err) return err;
                    err = cc_expect_punct(p, ',');
                    if(err) return err;
                    CcExpr* unused;
                    err = cc_parse_assignment_expr(p, vc, &unused, CCQT_NONE);
                    if(err) return err;
                    cc_release_expr(p, unused);
                    err = cc_expect_punct(p, ')');
                    if(err) return err;
                    return 0;
                }
                case CC__builtin_unreachable:
                case CC__builtin_trap:
                case CC__builtin_debugtrap:
                case CC__builtin_abort:
                case CC__bt:{
                    err = cc_expect_punct(p, '(');
                    if(err) return err;
                    err = cc_expect_punct(p, ')');
                    if(err) return err;
                    CcBuiltinOp op = CC_BUILTIN_UNREACHABLE;
                    switch(builtin){
                        case CC__builtin_unreachable: op = CC_BUILTIN_UNREACHABLE; break;
                        case CC__builtin_trap:        op = CC_BUILTIN_TRAP; break;
                        case CC__builtin_debugtrap:   op = CC_BUILTIN_DEBUGTRAP; break;
                        case CC__builtin_abort:       op = CC_BUILTIN_ABORT; break;
                        case CC__bt:                  op = CC_BUILTIN_BACKTRACE; break;
                        default:
                        return CC_UNREACHABLE_ERROR;
                    }
                    CcExpr* node = cc_make_expr(p, CC_EXPR_BUILTIN, tok.loc, ccqt_basic(CCBT_void), 0);
                    if(!node) return CC_OOM_ERROR;
                    node->builtin.op = op;
                    *out = node;
                    return 0;
                }
                case CC__builtin_sub_overflow:{
                    CcExprKind kind;
                    kind = CC_EXPR_SUB_OVERFLOW;
                    goto builtin_overflow;
                case CC__builtin_mul_overflow:
                    kind = CC_EXPR_MUL_OVERFLOW;
                    goto builtin_overflow;
                case CC__builtin_add_overflow:
                    kind = CC_EXPR_ADD_OVERFLOW;
                    builtin_overflow:;
                    err = cc_expect_punct(p, '(');
                    if(err) return err;
                    CcExpr* a;
                    err = cc_parse_assignment_expr(p, vc, &a, CCQT_NONE);
                    if(err) return err;
                    if(!ccqt_is_basic(a->type) || !ccbt_is_integer(a->type.basic.kind))
                        return cc_error(p, a->loc, "first argument to overflow builtin must be an integer type");
                    if(a->type.basic.kind == CCBT_int128 || a->type.basic.kind == CCBT_unsigned_int128)
                        return cc_error(p, a->loc, "__int128 is not supported for overflow builtins");
                    err = cc_expect_punct(p, ',');
                    if(err) return err;
                    CcExpr* b;
                    err = cc_parse_assignment_expr(p, vc, &b, CCQT_NONE);
                    if(err) return err;
                    if(!ccqt_is_basic(b->type) || !ccbt_is_integer(b->type.basic.kind))
                        return cc_error(p, b->loc, "second argument to overflow builtin must be an integer type");
                    if(b->type.basic.kind == CCBT_int128 || b->type.basic.kind == CCBT_unsigned_int128)
                        return cc_error(p, b->loc, "__int128 is not supported for overflow builtins");
                    err = cc_expect_punct(p, ',');
                    if(err) return err;
                    CcExpr* res;
                    err = cc_parse_assignment_expr(p, vc, &res, CCQT_NONE);
                    if(err) return err;
                    if(ccqt_kind(res->type) != CC_POINTER
                    || !ccqt_is_basic(ccqt_as_ptr(res->type)->pointee)
                    || !ccbt_is_integer(ccqt_as_ptr(res->type)->pointee.basic.kind))
                        return cc_error(p, res->loc, "third argument to overflow builtin must be a pointer to integer type");
                    if(ccqt_as_ptr(res->type)->pointee.basic.kind == CCBT_int128 || ccqt_as_ptr(res->type)->pointee.basic.kind == CCBT_unsigned_int128)
                        return cc_error(p, res->loc, "__int128 is not supported for overflow builtins");
                    err = cc_expect_punct(p, ')');
                    if(err) return err;
                    CcExpr* node = cc_make_expr(p, kind, tok.loc, ccqt_basic(CCBT_bool), 2);
                    if(!node) return CC_OOM_ERROR;
                    node->lhs = a;
                    node->values[0] = b;
                    node->values[1] = res;
                    *out = node;
                    return 0;
                }
                {
                    CcBitBuiltinOp op;
                case CC__builtin_popcount: op = CC_BIT_POPCOUNT; goto bit_builtin;
                case CC__builtin_popcountl: op = CC_BIT_POPCOUNT; goto bit_builtin;
                case CC__builtin_popcountll: op = CC_BIT_POPCOUNT; goto bit_builtin;
                case CC__builtin_clz: op = CC_BIT_CLZ; goto bit_builtin;
                case CC__builtin_clzl: op = CC_BIT_CLZ; goto bit_builtin;
                case CC__builtin_clzll: op = CC_BIT_CLZ; goto bit_builtin;
                case CC__builtin_ctz: op = CC_BIT_CTZ; goto bit_builtin;
                case CC__builtin_ctzl: op = CC_BIT_CTZ; goto bit_builtin;
                case CC__builtin_ctzll: op = CC_BIT_CTZ; goto bit_builtin;
                case CC__builtin_ffs: op = CC_BIT_FFS; goto bit_builtin;
                case CC__builtin_ffsl: op = CC_BIT_FFS; goto bit_builtin;
                case CC__builtin_ffsll: op = CC_BIT_FFS; goto bit_builtin;
                case CC__builtin_clrsb: op = CC_BIT_CLRSB; goto bit_builtin;
                case CC__builtin_clrsbl: op = CC_BIT_CLRSB; goto bit_builtin;
                case CC__builtin_clrsbll: op = CC_BIT_CLRSB; goto bit_builtin;
                case CC__builtin_parity: op = CC_BIT_PARITY; goto bit_builtin;
                case CC__builtin_parityl: op = CC_BIT_PARITY; goto bit_builtin;
                case CC__builtin_parityll: op = CC_BIT_PARITY; goto bit_builtin;
                case CC__builtin_ffsg: op = CC_BIT_FFS; goto bit_builtin;
                case CC__builtin_clzg: op = CC_BIT_CLZ; goto bit_builtin;
                case CC__builtin_ctzg: op = CC_BIT_CTZ; goto bit_builtin;
                case CC__builtin_clrsbg: op = CC_BIT_CLRSB; goto bit_builtin;
                case CC__builtin_popcountg: op = CC_BIT_POPCOUNT; goto bit_builtin;
                case CC__builtin_parityg: op = CC_BIT_PARITY; goto bit_builtin;
                case CC__builtin_stdc_bit_ceil: op = CC_BIT_BIT_CEIL; goto bit_builtin;
                case CC__builtin_stdc_bit_floor: op = CC_BIT_BIT_FLOOR; goto bit_builtin;
                case CC__builtin_stdc_bit_width: op = CC_BIT_BIT_WIDTH; goto bit_builtin;
                case CC__builtin_stdc_count_ones: op = CC_BIT_COUNT_ONES; goto bit_builtin;
                case CC__builtin_stdc_count_zeros: op = CC_BIT_COUNT_ZEROS; goto bit_builtin;
                case CC__builtin_stdc_first_leading_one: op = CC_BIT_FIRST_LEADING_ONE; goto bit_builtin;
                case CC__builtin_stdc_first_leading_zero: op = CC_BIT_FIRST_LEADING_ZERO; goto bit_builtin;
                case CC__builtin_stdc_first_trailing_one: op = CC_BIT_FIRST_TRAILING_ONE; goto bit_builtin;
                case CC__builtin_stdc_first_trailing_zero: op = CC_BIT_FIRST_TRAILING_ZERO; goto bit_builtin;
                case CC__builtin_stdc_has_single_bit: op = CC_BIT_HAS_SINGLE_BIT; goto bit_builtin;
                case CC__builtin_stdc_leading_ones: op = CC_BIT_LEADING_ONES; goto bit_builtin;
                case CC__builtin_stdc_leading_zeros: op = CC_BIT_LEADING_ZEROS; goto bit_builtin;
                case CC__builtin_stdc_trailing_ones: op = CC_BIT_TRAILING_ONES; goto bit_builtin;
                case CC__builtin_stdc_trailing_zeros: op = CC_BIT_TRAILING_ZEROS; goto bit_builtin;
                case CC__builtin_stdc_rotate_left: op = CC_BIT_ROTATE_LEFT; goto bit_builtin;
                case CC__builtin_stdc_rotate_right: op = CC_BIT_ROTATE_RIGHT; goto bit_builtin;
                    bit_builtin:;
                    _Bool fixed = (builtin >= CC__builtin_popcount && builtin <= CC__builtin_clzll) || (builtin >= CC__builtin_ffs && builtin <= CC__builtin_parityll);
                    _Bool stdc = builtin >= CC__builtin_stdc_bit_ceil && builtin <= CC__builtin_stdc_rotate_right;
                    _Bool rotate = op == CC_BIT_ROTATE_LEFT || op == CC_BIT_ROTATE_RIGHT;
                    err = cc_expect_punct(p, '(');
                    if(err) return err;
                    CcExpr* arg;
                    err = cc_parse_assignment_expr(p, vc, &arg, CCQT_NONE);
                    if(err) return err;
                    if(fixed){
                        CcBasicTypeKind param;
                        switch(builtin){
                            case CC__builtin_popcount: param = CCBT_unsigned; break;
                            case CC__builtin_popcountl: param = CCBT_unsigned_long; break;
                            case CC__builtin_popcountll: param = CCBT_unsigned_long_long; break;
                            case CC__builtin_clz: param = CCBT_unsigned; break;
                            case CC__builtin_clzl: param = CCBT_unsigned_long; break;
                            case CC__builtin_clzll: param = CCBT_unsigned_long_long; break;
                            case CC__builtin_ctz: param = CCBT_unsigned; break;
                            case CC__builtin_ctzl: param = CCBT_unsigned_long; break;
                            case CC__builtin_ctzll: param = CCBT_unsigned_long_long; break;
                            case CC__builtin_ffsl: case CC__builtin_clrsbl: param = CCBT_long; break;
                            case CC__builtin_ffsll: case CC__builtin_clrsbll: param = CCBT_long_long; break;
                            case CC__builtin_parity: param = CCBT_unsigned; break;
                            case CC__builtin_parityl: param = CCBT_unsigned_long; break;
                            case CC__builtin_parityll: param = CCBT_unsigned_long_long; break;
                            default: param = CCBT_int; break;
                        }
                        err = cc_implicit_cast(p, arg, ccqt_basic(param), &arg);
                        if(err) return err;
                    }
                    else {
                        _Bool signed_arg = op == CC_BIT_FFS || op == CC_BIT_CLRSB;
                        if(!ccqt_is_integer(arg->type) || ccqt_is_bool(arg->type) || (ccqt_is_unsigned(arg->type, !cc_target(p)->char_is_signed) == signed_arg))
                            return cc_error(p, arg->loc, "bit builtin requires %s integer argument", signed_arg ? "a signed" : "an unsigned");
                        // Load lvalues without applying integral promotions.
                        err = cc_implicit_cast(p, arg, arg->type, &arg);
                        if(err) return err;
                    }
                    CcExpr* second = NULL;
                    CcToken next;
                    err = cc_peek(p, &next);
                    if(err) return err;
                    if(rotate || (!fixed && (op == CC_BIT_CLZ || op == CC_BIT_CTZ) && next.type == CC_PUNCTUATOR && next.punct.punct == ',')){
                        err = cc_expect_punct(p, ',');
                        if(err) return err;
                        err = cc_parse_assignment_expr(p, vc, &second, CCQT_NONE);
                        if(err) return err;
                        if(rotate){
                            if(!ccqt_is_integer(second->type))
                                return cc_error(p, second->loc, "rotation count must have integer type");
                            err = cc_implicit_cast(p, second, second->type, &second);
                        }
                        else {
                            if(!ccqt_bt_eq(second->type, CCBT_int))
                                return cc_error(p, second->loc, "zero fallback must have int type");
                            err = cc_implicit_cast(p, second, ccqt_basic(CCBT_int), &second);
                        }
                        if(err) return err;
                    }
                    err = cc_expect_punct(p, ')');
                    if(err) return err;
                    CcQualType result_type = stdc ? ccqt_basic(CCBT_unsigned) : ccqt_basic(CCBT_int);
                    if(op == CC_BIT_BIT_CEIL || op == CC_BIT_BIT_FLOOR || rotate)
                        result_type = (CcQualType){.unqual = arg->type.unqual};
                    CcExpr* node = cc_make_expr(p, CC_EXPR_BIT_BUILTIN, tok.loc, result_type, second != NULL);
                    if(!node) return CC_OOM_ERROR;
                    node->bit_builtin.op = op;
                    node->bit_builtin.nargs = second != NULL;
                    node->lhs = arg;
                    if(second) node->values[0] = second;
                    *out = node;
                    return 0;
                }

                case CC__builtin_huge_val:
                case CC__builtin_huge_valf:
                case CC__builtin_huge_vall:{
                    err = cc_expect_punct(p, '(');
                    if(err) return err;
                    err = cc_expect_punct(p, ')');
                    if(err) return err;
                    CcExpr* node;
                    if(builtin == CC__builtin_huge_valf){
                        node = cc_make_expr(p, CC_EXPR_VALUE, tok.loc, ccqt_basic(CCBT_float), 0);
                        if(!node) return CC_OOM_ERROR;
                        node->float_ = INFINITY;
                    }
                    else if(builtin == CC__builtin_huge_val){
                        node = cc_make_expr(p, CC_EXPR_VALUE, tok.loc, ccqt_basic(CCBT_double), 0);
                        if(!node) return CC_OOM_ERROR;
                        node->double_ = (double)INFINITY;
                    }
                    else {
                        node = cc_make_expr(p, CC_EXPR_VALUE, tok.loc, ccqt_basic(CCBT_long_double), 0);
                        if(!node) return CC_OOM_ERROR;
                        node->double_ = (double)INFINITY;
                    }
                    *out = node;
                    return 0;
                }
                case CC__builtin_alloca:{
                    err = cc_expect_punct(p, '(');
                    if(err) return err;
                    CcExpr* sz;
                    err = cc_parse_assignment_expr(p, vc, &sz, CCQT_NONE);
                    if(err) return err;
                    err = cc_implicit_cast(p, sz, ccqt_basic(cc_target(p)->size_type), &sz);
                    if(err) return err;
                    err = cc_expect_punct(p, ')');
                    if(err) return err;
                    CcExpr* node = cc_make_expr(p, CC_EXPR_ALLOCA, tok.loc, p->void_star, 0);
                    if(!node) return CC_OOM_ERROR;
                    node->lhs = sz;
                    *out = node;
                    return 0;
                }
                case CC__builtin_intern:{
                    err = cc_expect_punct(p, '(');
                    if(err) return err;
                    CcExpr* arg;
                    err = cc_parse_assignment_expr(p, vc, &arg, CCQT_NONE);
                    if(err) return err;
                    if(!cc_implicit_convertible(p, arg->type, p->const_char_star))
                        return cc_error(p, arg->loc, "__builtin_intern argument must be a char pointer");
                    err = cc_implicit_cast(p, arg, p->const_char_star, &arg);
                    if(err) return err;
                    err = cc_expect_punct(p, ')');
                    if(err) return err;
                    CcExpr* node = cc_make_expr(p, CC_EXPR_INTERN, tok.loc, p->const_char_star, 0);
                    if(!node) return CC_OOM_ERROR;
                    node->lhs = arg;
                    *out = node;
                    return 0;
                }
                case CC__compile:{
                    err = cc_expect_punct(p, '(');
                    if(err) return err;
                    CcExpr* arg = NULL, *arg2 = NULL;
                    if(0){
                        __compile_fail:
                        if(arg) cc_release_expr(p, arg);
                        if(arg2) cc_release_expr(p, arg2);
                        return err;
                    }
                    err = cc_parse_assignment_expr(p, vc, &arg, CCQT_NONE);
                    if(err) return err;
                    if(!cc_implicit_convertible(p, arg->type, p->const_char_star)){
                        err = cc_error(p, arg->loc, "__compile argument must be convertible to const char*");
                        goto __compile_fail;
                    }
                    err = cc_implicit_cast(p, arg, p->const_char_star, &arg);
                    if(err) goto __compile_fail;

                    err = cc_expect_punct(p, ',');
                    if(err) goto __compile_fail;
                    err = cc_parse_assignment_expr(p, vc, &arg2, CCQT_NONE);
                    if(err) goto __compile_fail;
                    if(!cc_implicit_convertible(p, arg2->type, p->const_char_star)){
                        err = cc_error(p, arg2->loc, "__compile argument must be convertible to const char*");
                        goto __compile_fail;
                    }
                    err = cc_implicit_cast(p, arg2, p->const_char_star, &arg2);
                    if(err) goto __compile_fail;

                    err = cc_expect_punct(p, ')');
                    if(err)  goto __compile_fail;
                    CcExpr* node = cc_make_expr(p, CC_EXPR_COMPILE, tok.loc, p->builtin_module, 1);
                    if(!node) { err = CC_OOM_ERROR; goto __compile_fail;}
                    node->lhs = arg;
                    node->values[0] = arg2;
                    *out = node;
                    return 0;
                }
                case CC__root_module:{
                    err = cc_expect_punct(p, '(');
                    if(err) return err;
                    err = cc_expect_punct(p, ')');
                    if(err) return err;
                    CcExpr* node = cc_value_expr(p, tok.loc, p->builtin_module);
                    if(!node) return CC_OOM_ERROR;
                    node->uinteger = 0;
                    *out = node;
                    return 0;
                }
                case CC__hotswap:{
                    err = cc_expect_punct(p, '(');
                    if(err) return err;
                    CcExpr* old_func;
                    err = cc_parse_assignment_expr(p, vc, &old_func, CCQT_NONE);
                    if(err) return err;
                    if(ccqt_kind(old_func->type) == CC_FUNCTION){
                        CcQualType ptr_type;
                        err = cc_pointer_of(p, old_func->type, &ptr_type);
                        if(err) return err;
                        err = cc_implicit_cast(p, old_func, ptr_type, &old_func);
                        if(err) return err;
                    }
                    if(ccqt_kind(old_func->type) != CC_POINTER
                    || ccqt_kind(ccqt_as_ptr(old_func->type)->pointee) != CC_FUNCTION)
                        return cc_error(p, old_func->loc, "__hotswap first argument must be a function pointer");
                    err = cc_expect_punct(p, ',');
                    if(err) return err;
                    CcExpr* new_func;
                    err = cc_parse_assignment_expr(p, vc, &new_func, CCQT_NONE);
                    if(err) return err;
                    if(ccqt_kind(new_func->type) == CC_FUNCTION){
                        CcQualType ptr_type;
                        err = cc_pointer_of(p, new_func->type, &ptr_type);
                        if(err) return err;
                        err = cc_implicit_cast(p, new_func, ptr_type, &new_func);
                        if(err) return err;
                    }
                    if(new_func->type.bits != old_func->type.bits)
                        return cc_error(p, new_func->loc, "__hotswap function pointer types must match");
                    err = cc_expect_punct(p, ')');
                    if(err) return err;
                    CcExpr* node = cc_make_expr(p, CC_EXPR_HOTSWAP, tok.loc, ccqt_basic(CCBT_int), 1);
                    if(!node) return CC_OOM_ERROR;
                    node->lhs = old_func;
                    node->values[0] = new_func;
                    *out = node;
                    return 0;
                }
                case CC__builtin_nanf:
                case CC__builtin_nan:
                case CC__nan:{
                    err = cc_expect_punct(p, '(');
                    if(err) return err;
                    CcExpr* arg;
                    err = cc_parse_assignment_expr(p, vc, &arg, CCQT_NONE);
                    if(err) return err;
                    cc_release_expr(p, arg);
                    err = cc_expect_punct(p, ')');
                    if(err) return err;
                    CcExpr* node;
                    if(builtin == CC__builtin_nanf){
                        node = cc_make_expr(p, CC_EXPR_VALUE, tok.loc, ccqt_basic(CCBT_float), 0);
                        if(!node) return CC_OOM_ERROR;
                        node->float_ = NAN;
                    }
                    else {
                        node = cc_make_expr(p, CC_EXPR_VALUE, tok.loc, ccqt_basic(CCBT_double), 0);
                        if(!node) return CC_OOM_ERROR;
                        node->double_ = (double)NAN;
                    }
                    *out = node;
                    return 0;
                }
                case CC__builtin_bswap16:{
                    CcQualType t;
                    t = ccqt_basic(CCBT_unsigned_short);
                    goto bswap;
                case CC__builtin_bswap32:
                    t = ccqt_basic(CCBT_unsigned);
                    goto bswap;
                case CC__builtin_bswap64:
                    t = ccqt_basic(ccbt_to_unsigned(cc_target(p)->int64_type));
                    bswap:
                    err = cc_expect_punct(p, '(');
                    if(err) return err;
                    CcExpr* arg;
                    err = cc_parse_assignment_expr(p, vc, &arg, CCQT_NONE);
                    if(err) return err;
                    err = cc_implicit_cast(p, arg, t, &arg);
                    if(err) return err;
                    err = cc_expect_punct(p, ')');
                    if(err) return err;
                    CcExpr* node = cc_make_expr(p, CC_EXPR_BSWAP, tok.loc, t, 0);
                    if(!node) return CC_OOM_ERROR;
                    node->lhs = arg;
                    *out = node;
                    return 0;
                }
            }
            CcSymbol sym;
            if(!cc_scope_lookup_symbol(p->current, tok.ident.ident, CC_SCOPE_WALK_CHAIN, &sym)){
                return cc_error(p, tok.loc, "undeclared identifier '%.*s'", tok.ident.ident->length, tok.ident.ident->data);
            }
            switch(sym.kind){
                case CC_SYM_VAR:{
                    if(vc == CC_CONSTEXPR_VALUE && !sym.var->constexpr_ && ccqt_kind(sym.var->type) != CC_ARRAY)
                        return cc_error(p, tok.loc, "expression '%s' is not a constant expression", tok.ident.ident->data);
                    if(vc == CC_LINKTIME_VALUE && sym.var->automatic)
                        return cc_error(p, tok.loc, "expression '%s' is not a constant expression", tok.ident.ident->data);
                    CcExpr* node = cc_make_expr(p, CC_EXPR_VARIABLE, tok.loc, sym.var->type, 0);
                    if(!node) return CC_OOM_ERROR;
                    node->is_lvalue = 1;
                    node->var = sym.var;
                    *out = node;
                    return 0;
                }
                case CC_SYM_FUNC:{
                    CcExpr* node = cc_make_expr(p, CC_EXPR_FUNCTION, tok.loc, (CcQualType){.bits = (uintptr_t)sym.func->type}, 0);
                    if(!node) return CC_OOM_ERROR;
                    node->func = sym.func;
                    *out = node;
                    return 0;
                }
                case CC_SYM_ENUMERATOR:{
                    CcExpr* node = cc_integer_bits_expr(p, tok.loc, sym.enumerator->type, sym.enumerator->value);
                    if(!node) return CC_OOM_ERROR;
                    *out = node;
                    return 0;
                }
                case CC_SYM_TYPEDEF:
                    err = cc_unget(p, &tok);
                    if(err) return err;
                    return cc_parse_lambda(p, vc, tok.loc, out);
            }
            return cc_error(p, tok.loc, "unexpected symbol kind");
        }
        case CC_PUNCTUATOR:
            if(tok.punct.punct == CC_lparen){
                CcToken peek;
                err = cc_peek(p, &peek);
                if(err) return err;
                if(peek.type == CC_PUNCTUATOR && peek.punct.punct == '{'){
                    if(vc == CC_CONSTEXPR_VALUE)
                        return cc_error(p, tok.loc, "statement expression in constant expression");
                    err = cc_next_token(p, &tok);
                    if(err) return err;
                    // statement expression
                    err = cc_push_scope(p);
                    if(err) return err;
                    CcStmtSink* sink = cc_push_stmt_sink(p);
                    if(!sink){
                        cc_pop_scope(p);
                        return CC_OOM_ERROR;
                    }
                    for(;;){
                        err = cc_peek(p, &peek);
                        if(err) goto end_block;
                        if(peek.type == CC_EOF){
                            err = cc_error(p, tok.loc, "Unterminated statement expression");
                            goto end_block;
                        }
                        if(peek.type == CC_PUNCTUATOR && peek.punct.punct == '}'){
                            cc_next_token(p, &peek); // consume '}'
                            break;
                        }
                        err = cc_parse_one(p);
                        if(err) goto end_block;
                    }
                    end_block:;
                    CcStmtNode* node = NULL;
                    if(!err) err = cc_finalize_stmt_list(p, tok.loc, &sink->stmts, 1, &node);
                    if(!err) err = cc_stmt_scope_vars(p, node);
                    cc_pop_scope(p);
                    cc_pop_stmt_sink(p, sink);
                    if(err){
                        cc_free_stmt_tree(p, node);
                        return err;
                    }
                    err = cc_expect_punct(p, CC_rparen);
                    if(err){
                        cc_free_stmt_tree(p, node);
                        return err;
                    }
                    CcQualType t = ccqt_basic(CCBT_void);
                    if(node->count){
                        CcStmtNode* last = node->stmts[node->count-1];
                        if(last->kind == CC_STMT_EXPR){
                            t = last->exprs[0]->type;
                        }
                    }
                    CcExpr* val = cc_make_expr(p, CC_EXPR_STATEMENT_EXPRESSION, tok.loc, t, 0);
                    if(!val){
                        cc_free_stmt_tree(p, node);
                        return CC_OOM_ERROR;
                    }
                    val->stmt_body = node;
                    *out = val;
                    return 0;
                }
                CcExpr* inner;
                err = cc_parse_expr(p, vc, &inner);
                if(err) return err;
                err = cc_expect_punct(p, CC_rparen);
                if(err) return err;
                *out = inner;
                return 0;
            }
            if(tok.punct.punct == CC_lbrace){
                // Just treat similar to (), but without comma expression
                CcExpr* inner;
                err = cc_parse_assignment_expr(p, vc, &inner, CCQT_NONE);
                if(err) return err;
                err = cc_expect_punct_supp(p, CC_rbrace, " to end {}-enclosed expression without inferred type");
                if(err) return err;
                *out = inner;
                return 0;
            }
            return cc_error(p, tok.loc, "Unexpected punctuator in expression");
        case CC_KEYWORD:
            switch(tok.kw.kw){
            case CC_sizeof:{
                CcToken peek;
                err = cc_peek(p, &peek);
                if(err) return err;
                if(peek.type == CC_PUNCTUATOR && peek.punct.punct == CC_lparen){
                    err = cc_next_token(p, &peek);
                    if(err) return err;
                    CcToken peek2;
                    err = cc_peek(p, &peek2);
                    if(err) return err;
                    if(cc_is_type_start(p, &peek2)){
                        CcQualType type;
                        err = cc_parse_type_name(p, &type, NULL);
                        if(err) return err;
                        err = cc_expect_punct(p, CC_rparen);
                        if(err) return err;
                        CcToken peek3;
                        err = cc_peek(p, &peek3);
                        if(err) return err;
                        if(peek3.type == CC_PUNCTUATOR && peek3.punct.punct == CC_lbrace){
                            CcExpr* result;
                            err = cc_parse_init_list(p, vc, &result, type);
                            if(err) return err;
                            result->kind = CC_EXPR_COMPOUND_LITERAL;
                            CcExpr* postfixed;
                            err = cc_parse_postfix(p, vc, result, &postfixed);
                            if(err) return err;
                            CcExpr* sz;
                            err = cc_sizeof_as_expr(p, postfixed->type, tok.loc, &sz);
                            if(err) return err;
                            cc_release_expr(p, postfixed);
                            *out = sz;
                            return 0;
                        }
                        CcExpr* sz;
                        err = cc_sizeof_as_expr(p, type, tok.loc, &sz);
                        if(err) return err;
                        *out = sz;
                        return 0;
                    }
                    err = cc_unget(p, &peek);
                    if(err) return err;
                }
                CcExpr* operand;
                err = cc_parse_prefix(p, CC_RUNTIME_VALUE, &operand);
                if(err) return err;
                if(!operand->type.ptr)
                    return cc_error(p, tok.loc, "cannot take sizeof incomplete type");
                CcExpr* sz;
                err = cc_sizeof_as_expr(p, operand->type, tok.loc, &sz);
                if(err) return err;
                cc_release_expr(p, operand);
                *out = sz;
                return 0;
            }
            case CC_alignof:{
                CcToken peek;
                err = cc_peek(p, &peek);
                if(err) return err;
                if(peek.type == CC_PUNCTUATOR && peek.punct.punct == CC_lparen){
                    err = cc_next_token(p, &peek);
                    if(err) return err;
                    CcToken peek2;
                    err = cc_peek(p, &peek2);
                    if(err) return err;
                    if(cc_is_type_start(p, &peek2)){
                        CcQualType type;
                        err = cc_parse_type_name(p, &type, NULL);
                        if(err) return err;
                        err = cc_expect_punct(p, CC_rparen);
                        if(err) return err;
                        CcExpr* al;
                        err = cc_alignof_as_expr(p, type, tok.loc, &al);
                        if(err) return err;
                        *out = al;
                        return 0;
                    }
                    err = cc_unget(p, &peek);
                    if(err) return err;
                }
                CcExpr* operand;
                err = cc_parse_prefix(p, CC_RUNTIME_VALUE, &operand);
                if(err) return err;
                CcExpr* al;
                if(operand->kind == CC_EXPR_VARIABLE && operand->var->alignment){
                    al = cc_value_expr(p, tok.loc, ccqt_basic(cc_target(p)->size_type));
                    if(!al) return CC_OOM_ERROR;
                    al->uinteger = operand->var->alignment;
                }
                else {
                    err = cc_alignof_as_expr(p, operand->type, tok.loc, &al);
                    if(err) return err;
                }
                cc_release_expr(p, operand);
                *out = al;
                return 0;
            }
            case CC__Countof:{
                CcToken peek;
                err = cc_peek(p, &peek);
                if(err) return err;
                CcQualType arr_type;
                if(peek.type == CC_PUNCTUATOR && peek.punct.punct == CC_lparen){
                    err = cc_next_token(p, &peek);
                    if(err) return err;
                    CcToken peek2;
                    err = cc_peek(p, &peek2);
                    if(err) return err;
                    if(cc_is_type_start(p, &peek2)){
                        err = cc_parse_type_name(p, &arr_type, NULL);
                        if(err) return err;
                        err = cc_expect_punct(p, CC_rparen);
                        if(err) return err;
                        CcToken peek3;
                        err = cc_peek(p, &peek3);
                        if(err) return err;
                        if(peek3.type == CC_PUNCTUATOR && peek3.punct.punct == CC_lbrace){
                            CcExpr* result;
                            err = cc_parse_init_list(p, vc, &result, arr_type);
                            if(err) return err;
                            result->kind = CC_EXPR_COMPOUND_LITERAL;
                            CcExpr* postfixed;
                            err = cc_parse_postfix(p, vc, result, &postfixed);
                            if(err) return err;
                            arr_type = postfixed->type;
                            cc_release_expr(p, postfixed);
                        }
                        goto countof_have_type;
                    }
                    err = cc_unget(p, &peek);
                    if(err) return err;
                }
                {
                    CcExpr* expr = NULL;
                    err = cc_parse_prefix(p, CC_RUNTIME_VALUE, &expr);
                    if(err) return err;
                    if(ccqt_kind(expr->type) == CC_SLICE){
                        if(vc != CC_RUNTIME_VALUE)
                            return cc_error(p, expr->loc, "Can only _Countof a slice at runtime");
                        CcExpr* node = cc_make_expr(p, CC_EXPR_DOT, expr->loc, ccqt_basic(cc_target(p)->size_type), 1);
                        if(!node) return CC_OOM_ERROR;
                        node->field_path = (CcFieldPath){.n_components=1, .idx0=0};
                        node->values[0] = expr;
                        *out = node;
                        return 0;
                    }
                    arr_type = expr->type;
                    cc_release_expr(p, expr);
                }
                countof_have_type:;
                if(ccqt_kind(arr_type) != CC_ARRAY)
                    return cc_error(p, tok.loc, "_Countof requires an array type");
                CcArray* arr = ccqt_as_array(arr_type);
                if(arr->is_incomplete)
                    return cc_error(p, tok.loc, "_Countof applied to incomplete array type");
                CcQualType size_type = ccqt_basic(cc_target(p)->size_type);
                if(arr->is_vla){
                    CcExpr* dim = arr->vla_expr;
                    if(!dim) return cc_error(p, tok.loc, "_Countof: VLA has no dimension expression");
                    err = cc_implicit_cast(p, dim, size_type, out);
                    if(err) return err;
                    return 0;
                }
                CcExpr* node = cc_value_expr(p, tok.loc, size_type);
                if(!node) return CC_OOM_ERROR;
                node->uinteger = (uint64_t)arr->length;
                *out = node;
                return 0;
            }
            case CC_true:
            case CC_false:{
                CcExpr* node = cc_value_expr(p, tok.loc, ccqt_basic(CCBT_bool));
                if(!node) return CC_OOM_ERROR;
                node->uinteger = tok.kw.kw == CC_true ? 1 : 0;
                *out = node;
                return 0;
            }
            case CC_nullptr:{
                CcExpr* node = cc_value_expr(p, tok.loc, ccqt_basic(CCBT_nullptr_t));
                if(!node) return CC_OOM_ERROR;
                node->uinteger = 0;
                *out = node;
                return 0;
            }
            case CC__Generic:
                return cc_parse_Generic(p, vc, out);
            case CC__Atomic:
            case CC__BitInt:
            case CC__Complex:
            case CC__Decimal128:
            case CC__Decimal32:
            case CC__Decimal64:
            case CC__Float128:
            case CC__Float16:
            case CC__Float32:
            case CC__Float32x:
            case CC__Float64:
            case CC__Float64x:
            case CC__Imaginary:
            case CC__Any:
            case CC__Type:
            case CC__Self:
            case CC___auto_type:
            case CC___int128:
            case CC_bool:
            case CC_char:
            case CC_const:
            case CC_double:
            case CC_enum:
            case CC_float:
            case CC_int:
            case CC_long:
            case CC_restrict:
            case CC_short:
            case CC_signed:
            case CC_struct:
            case CC_typeof:
            case CC_typeof_unqual:
            case CC_union:
            case CC_unsigned:
            case CC_void:
            case CC_volatile:
                err = cc_unget(p, &tok);
                if(err) return err;
                return cc_parse_lambda(p, vc, tok.loc, out);
            case CC___attribute__:
            case CC___declspec:
            case CC__Noreturn:
            case CC_alignas:
            case CC_auto:
            case CC_asm:
            case CC_break:
            case CC_case:
            case CC_constexpr:
            case CC_continue:
            case CC_default:
            case CC_do:
            case CC_else:
            case CC_extern:
            case CC_for:
            case CC_goto:
            case CC_if:
            case CC_inline:
            case CC_register:
            case CC_return:
            case CC_static:
            case CC_static_assert:
            case CC_switch:
            case CC_thread_local:
            case CC_typedef:
            case CC_while:
                return cc_error(p, tok.loc, "Unexpected keyword in expression");
                DRP_CASES_EXHAUSTED;
            }
        case CC_EOF:
            return cc_error(p, tok.loc, "Unexpected end of input in expression");
    }
    return cc_error(p, tok.loc, "Unexpected token in expression");
}

static
int
cc_skip_to_next_comma_or_paren(CcParser* p, const char* context){
    int err;
    int depth = 0;
    CcToken tok;
    for(;;){
        err = cc_next_token(p, &tok);
        if(err) return err;
        if(tok.type == CC_EOF)
            return cc_error(p, tok.loc, "unterminated %s", context);
        if(tok.type != CC_PUNCTUATOR) continue;
        switch(tok.punct.punct){
            case '(': case '[': case '{': depth++; break;
            case ')': case ']': case '}':
                if(depth == 0){
                    err = cc_unget(p, &tok);
                    if(err) return err;
                    goto done_skip;
                }
                depth--;
                break;
            case ',':
                if(depth == 0){
                    err = cc_unget(p, &tok);
                    if(err) return err;
                    goto done_skip;
                }
                break;
            case CC_amp:
            case CC_amp_assign:
            case CC_and:
            case CC_arrow:
            case CC_assign:
            case CC_bang:
            case CC_colon:
            case CC_dot:
            case CC_double_colon:
            case CC_ellipsis:
            case CC_eq:
            case CC_ge:
            case CC_gt:
            case CC_le:
            case CC_lshift:
            case CC_lshift_assign:
            case CC_lt:
            case CC_minus:
            case CC_minus_assign:
            case CC_minusminus:
            case CC_ne:
            case CC_or:
            case CC_percent:
            case CC_percent_assign:
            case CC_pipe:
            case CC_pipe_assign:
            case CC_plus:
            case CC_plus_assign:
            case CC_plusplus:
            case CC_question:
            case CC_rshift:
            case CC_rshift_assign:
            case CC_semi:
            case CC_slash:
            case CC_slash_assign:
            case CC_star:
            case CC_star_assign:
            case CC_tilde:
            case CC_xor:
            case CC_xor_assign:
            break;
        }
    }
    done_skip:;
    return 0;
}
static
int
cc_parse_Generic(CcParser* p, CcValueClass vc, CcExpr*_Nullable*_Nonnull out){
    int err;
    err = cc_expect_punct(p, '(');
    if(err) return err;
    CcQualType tswitch;
    CcToken tok;
    err = cc_peek(p, &tok);
    if(err) return err;
    SrcLoc loc = tok.loc;
    // Extension: allow a type in first slot
    if(cc_is_type_start(p, &tok)){
        err = cc_parse_type_name(p, &tswitch, NULL);
        if(err) return err;
    }
    else {
        CcExpr* condition;
        err = cc_parse_assignment_expr(p, vc, &condition, CCQT_NONE);
        if(err) return err;
        tswitch = condition->type;
        cc_release_expr(p, condition);
        tswitch.quals = 0;
        CcTypeKind tk = ccqt_kind(tswitch);
        switch(tk){
        case CC_ARRAY:{
            CcArray* arr = ccqt_as_array(tswitch);
            if(!arr->is_vector){
                err = cc_pointer_of(p, arr->element, &tswitch);
                if(err) return err;
            }
            break;
        }
        case CC_FUNCTION:{
            err = cc_pointer_of(p, tswitch, &tswitch);
            if(err) return err;
            break;
        }
        case CC_BASIC:
        case CC_ENUM:
        case CC_POINTER:
        case CC_BLOCK_POINTER:
        case CC_STRUCT:
        case CC_UNION:
        case CC_SLICE:
            break;
        }
    }
    err = cc_expect_punct(p, ',');
    if(err) return err;
    CcExpr* result = NULL;
    CcExpr* default_result = NULL;
    for(;;){
        err = cc_peek(p, &tok);
        if(err) return err;
        _Bool is_default = 0;
        _Bool is_match = 0;
        if(tok.type == CC_KEYWORD && tok.kw.kw == CC_default){
            if(default_result)
                return cc_error(p, tok.loc, "more than one default in _Generic");
            err = cc_next_token(p, &tok);
            if(err) return err;
            is_default = 1;
        }
        else {
            CcQualType assoc_type;
            err = cc_parse_type_name(p, &assoc_type, NULL);
            if(err) return err;
            is_match = (assoc_type.bits == tswitch.bits);
        }
        err = cc_expect_punct(p, ':');
        if(err) return err;
        if(is_match && !result){
            err = cc_parse_assignment_expr(p, vc, &result, CCQT_NONE);
            if(err) return err;
        }
        else if(is_default){
            err = cc_parse_assignment_expr(p, vc, &default_result, CCQT_NONE);
            if(err) return err;
        }
        else {
            err = cc_skip_to_next_comma_or_paren(p, "_Generic");
            if(err) return err;
        }
        err = cc_next_token(p, &tok);
        if(err) return err;
        if(tok.type == CC_PUNCTUATOR && tok.punct.punct == ')')
            break;
        if(tok.type != CC_PUNCTUATOR || tok.punct.punct != ',')
            return cc_error(p, tok.loc, "expected ',' or ')' in _Generic");
    }
    if(result){
        *out = result;
        if(default_result)
            cc_release_expr(p, default_result);
    }
    else if(default_result)
        *out = default_result;
    else
        return cc_error(p, loc, "no matching type in _Generic and no default");
    return 0;
}

static
int
cc_pack_call_args(CcParser* p, SrcLoc loc, CcQualType slice, Parray(CcExpression)* args, uint32_t start){
    uint32_t count = (uint32_t)args->count - start;
    CcQualType element = ccqt_as_slice(slice)->pointee;
    CcArray* array = cc_intern_array(&p->type_cache, cc_allocator(p), element, count, 0, 0, 0, 0);
    if(!array) return CC_OOM_ERROR;
    for(uint32_t i = start; i < args->count; i++){
        CcExpr** arg = (CcExpr**)&args->data[i];
        int err = cc_implicit_cast(p, *arg, element, arg);
        if(err) return err;
    }
    CcInitList* list = Allocator_zalloc(cc_allocator(p), sizeof *list + count * sizeof(CcInitEntry));
    if(!list) return CC_OOM_ERROR;
    CcExpr* literal = cc_make_expr(p, CC_EXPR_COMPOUND_LITERAL, loc, (CcQualType){.bits=(uintptr_t)array}, 0);
    if(!literal){
        Allocator_free(cc_allocator(p), list, sizeof *list + count * sizeof(CcInitEntry));
        return CC_OOM_ERROR;
    }
    list->loc = loc;
    list->count = count;
    for(uint32_t i = 0; i < count; i++)
        list->entries[i] = (CcInitEntry){.path={.n_components=1, .idx0=i}, .value=(CcExpr*)args->data[start+i]};
    literal->init_list = list;
    CcExpr* packed;
    int err = cc_implicit_cast(p, literal, slice, &packed);
    if(err) return err;
    args->count = start;
    if(pa_push(args, cc_scratch_allocator(p), packed)) return CC_OOM_ERROR;
    return 0;
}

static
int
cc_parse_postfix(CcParser* p, CcValueClass vc, CcExpr* operand, CcExpr* _Nullable* _Nonnull out){
    CcExpr* _Nullable receiver = NULL;
    for(;;){
        CcToken tok;
        int err = cc_next_token(p, &tok);
        if(err) return err;
        if(tok.type != CC_PUNCTUATOR){
            cc_unget(p, &tok);
            break;
        }
        if(tok.punct.punct != CC_lparen && tok.punct.punct != CC_dot && tok.punct.punct != CC_arrow)
            receiver = NULL;
        if(operand->kind == CC_EXPR_COMPOUND_LITERAL){
            switch((uint32_t)tok.punct.punct){
                case CC_plusplus: case CC_minusminus:
                case CC_lbracket: case CC_dot: case CC_arrow:
                    err = cc_desugar_compound_literal(p, operand, &operand);
                    if(err) return err;
                    break;
                default: break;
            }
        }
        switch((uint32_t)tok.punct.punct){
            case CC_plusplus: {
                if(vc > CC_RUNTIME_VALUE)
                    return cc_error(p, tok.loc, "increment/decrement in constant expression");
                if(!operand->is_lvalue)
                    return cc_error(p, tok.loc, "expression is not an lvalue");
                if(operand->type.is_const)
                    return cc_error(p, tok.loc, "cannot modify const-qualified variable");
                err = cc_check_incdec_type(p, operand->type, tok.loc);
                if(err) return err;
                CcExpr* node = cc_make_expr(p, CC_EXPR_POSTINC, tok.loc, (CcQualType){.unqual=operand->type.unqual}, 0);
                if(!node) return CC_OOM_ERROR;
                node->lhs = operand;
                operand = node;
                continue;
            }
            case CC_minusminus: {
                if(vc > CC_RUNTIME_VALUE)
                    return cc_error(p, tok.loc, "increment/decrement in constant expression");
                if(!operand->is_lvalue)
                    return cc_error(p, tok.loc, "expression is not an lvalue");
                if(operand->type.is_const)
                    return cc_error(p, tok.loc, "cannot modify const-qualified variable");
                err = cc_check_incdec_type(p, operand->type, tok.loc);
                if(err) return err;
                CcExpr* node = cc_make_expr(p, CC_EXPR_POSTDEC, tok.loc, (CcQualType){.unqual=operand->type.unqual}, 0);
                if(!node) return CC_OOM_ERROR;
                node->lhs = operand;
                operand = node;
                continue;
            }
            case CC_lbracket:{
                CcToken peek;
                err = cc_peek(p, &peek);
                if(err) return err;
                if(peek.type == CC_PUNCTUATOR && peek.punct.punct == ':'){
                    CcQualType elem_type;
                    err = cc_check_pointer_arithmetic(p, operand->type, tok.loc);
                    if(err) return err;
                    err = cc_deref_type(p, operand->type, &elem_type, tok.loc, 0);
                    if(err) return err;
                    CcQualType slice_type;
                    err = cc_slice_of(p, elem_type, &slice_type);
                    if(err) return err;
                    err = cc_next_token(p, &peek);
                    if(err) return err;
                    err = cc_peek(p, &peek);
                    if(err) return err;
                    if(peek.type == CC_PUNCTUATOR && peek.punct.punct == ']'){
                        if(ccqt_kind(operand->type) == CC_POINTER)
                            return cc_error(p, tok.loc, "slice of pointer requires upper bound");
                        if(ccqt_kind(operand->type) == CC_ARRAY && ccqt_as_array(operand->type)->is_incomplete)
                            return cc_error(p, tok.loc, "slice of incomplete array requires upper bound");
                        err = cc_next_token(p, &peek);
                        if(err) return err;
                        CcExpr* node = cc_make_expr(p, CC_EXPR_SLICE_ALL, tok.loc, slice_type, 0);
                        if(!node) return CC_OOM_ERROR;
                        node->lhs = operand;
                        operand = node;
                        continue;
                    }
                    CcExpr* hi;
                    err = cc_parse_expr(p, vc, &hi);
                    if(err) return err;
                    err = cc_implicit_cast_to_index(p, hi, &hi);
                    if(err) return err;
                    err = cc_expect_punct(p, CC_rbracket);
                    if(err) return err;
                    CcExpr* node = cc_make_expr(p, CC_EXPR_SLICE_HI, tok.loc, slice_type, 1);
                    if(!node) return CC_OOM_ERROR;
                    node->lhs = operand;
                    node->values[0] = hi;
                    operand = node;
                    continue;
                }
                CcExpr* index;
                err = cc_parse_expr(p, vc, &index);
                if(err) return err;
                err = cc_peek(p, &peek);
                if(err) return err;
                if(peek.type == CC_PUNCTUATOR && peek.punct.punct == ':'){
                    CcQualType elem_type;
                    err = cc_check_pointer_arithmetic(p, operand->type, tok.loc);
                    if(err) return err;
                    err = cc_deref_type(p, operand->type, &elem_type, tok.loc, 0);
                    if(err) return err;
                    CcQualType slice_type;
                    err = cc_slice_of(p, elem_type, &slice_type);
                    if(err) return err;
                    err = cc_implicit_cast_to_index(p, index, &index);
                    if(err) return err;
                    err = cc_next_token(p, &peek);
                    if(err) return err;
                    err = cc_peek(p, &peek);
                    if(err) return err;
                    if(peek.type == CC_PUNCTUATOR && peek.punct.punct == ']'){
                        if(ccqt_kind(operand->type) == CC_POINTER)
                            return cc_error(p, tok.loc, "slice of pointer requires upper bound");
                        if(ccqt_kind(operand->type) == CC_ARRAY && ccqt_as_array(operand->type)->is_incomplete)
                            return cc_error(p, tok.loc, "slice of incomplete array requires upper bound");
                        err = cc_next_token(p, &peek);
                        if(err) return err;
                        CcExpr* node = cc_make_expr(p, CC_EXPR_SLICE_LO, tok.loc, slice_type, 1);
                        if(!node) return CC_OOM_ERROR;
                        node->lhs = operand;
                        node->values[0] = index;
                        operand = node;
                        continue;
                    }
                    CcExpr* hi;
                    err = cc_parse_expr(p, vc, &hi);
                    if(err) return err;
                    err = cc_implicit_cast_to_index(p, hi, &hi);
                    if(err) return err;
                    err = cc_expect_punct(p, CC_rbracket);
                    if(err) return err;
                    CcExpr* node = cc_make_expr(p, CC_EXPR_SLICE, tok.loc, slice_type, 2);
                    if(!node) return CC_OOM_ERROR;
                    node->lhs = operand;
                    node->values[0] = index;
                    node->values[1] = hi;
                    operand = node;
                    continue;
                }
                err = cc_expect_punct(p, CC_rbracket);
                if(err) return err;
                {
                    CcQualType idx_type = index->type;
                    if(ccqt_kind(idx_type) == CC_ENUM)
                        idx_type = ccqt_as_enum(idx_type)->underlying;
                    _Bool idx_int = ccqt_is_basic(idx_type) && ccbt_is_integer(idx_type.basic.kind);
                    _Bool idx_ptr = ccqt_is_pointer_like(idx_type);
                    if(!idx_int && !idx_ptr)
                        return cc_error(p, tok.loc, "array subscript requires integer or pointer type");
                }
                if(!ccqt_is_pointer_like(operand->type) && ccqt_is_pointer_like(index->type)){
                    CcExpr* tmp = operand;
                    operand = index;
                    index = tmp;
                }
                {
                    err = cc_implicit_cast_to_index(p, index, &index);
                    if(err) return err;
                }
                CcQualType elem_type;
                err = cc_check_pointer_arithmetic(p, operand->type, tok.loc);
                if(err) return err;
                err = cc_deref_type(p, operand->type, &elem_type, tok.loc, 1);
                if(err) return err;
                CcExpr* node = cc_make_expr(p, CC_EXPR_SUBSCRIPT, tok.loc, elem_type, 1);
                if(!node) return CC_OOM_ERROR;
                node->is_lvalue = 1;
                node->lhs = operand;
                node->values[0] = index;
                operand = node;
                continue;
            }
            case CC_dot:
            case CC_arrow: {
                CcExprKind mkind = tok.punct.punct == CC_dot ? CC_EXPR_DOT : CC_EXPR_ARROW;
                CcToken member;
                err = cc_next_token(p, &member);
                if(err) return err;
                if(member.type != CC_IDENTIFIER)
                    return cc_error(p, member.loc, "Expected identifier after '%s'", mkind == CC_EXPR_DOT ? "." : "->");
                Atom member_name = member.ident.ident;
                CcQualType agg_type = operand->type;
                if(ccqt_kind(agg_type) == CC_POINTER){
                    CcPointer* ptr = ccqt_as_ptr(agg_type);
                    agg_type = ptr->pointee;
                    mkind = CC_EXPR_ARROW;
                }
                else {
                    mkind = CC_EXPR_DOT;
                }
                uint64_t floc = 0;
                CcQualType member_type = {0};
                CcQualType member_owner = {0};
                CcFunc* _Null_unspecified method = NULL;
                CcTypeKind tk = ccqt_kind(agg_type);
                if(tk == CC_STRUCT && p->builtin_src_loc.bits && agg_type.ptr == ccqt_as_ptr(p->builtin_src_loc)->pointee.ptr){
                    StringView name = {member_name->length, member_name->data};
                    CcSrcLocOp op;
                    CcQualType type = ccqt_basic(cc_target(p)->size_type);
                    if(sv_equals(name, SV("file"))){
                        op = CC_SRCLOC_FILE;
                        type = p->const_char_slice;
                    }
                    else if(sv_equals(name, SV("line")))
                        op = CC_SRCLOC_LINE;
                    else if(sv_equals(name, SV("col")))
                        op = CC_SRCLOC_COL;
                    else
                        goto fucs;
                    CcExpr* node = cc_unary_expr(p, CC_EXPR_SRCLOC_REFLECT, tok.loc, type, operand);
                    if(!node) return CC_OOM_ERROR;
                    node->srcloc.op = op;
                    operand = node;
                    continue;
                }
                if(tk == CC_STRUCT && p->builtin_module.bits && agg_type.ptr == ccqt_as_ptr(p->builtin_module)->pointee.ptr){
                    StringView mname = {member_name->length, member_name->data};
                    if(sv_equals(mname, SV("symbol"))){
                        err = cc_expect_punct(p, '(');
                        if(err) return err;
                        CcExpr* name;
                        err = cc_parse_assignment_expr(p, vc, &name, CCQT_NONE);
                        if(err) return err;
                        if(!cc_implicit_convertible(p, name->type, p->const_char_star))
                            return cc_error(p, name->loc, "_Module.symbol first argument must be convertible to const char*");
                        err = cc_implicit_cast(p, name, p->const_char_star, &name);
                        if(err) return err;
                        err = cc_expect_punct(p, ',');
                        if(err) return err;
                        CcQualType symbol_type;
                        err = cc_parse_type_name(p, &symbol_type, NULL);
                        if(err) return err;
                        CcQualType result_type;
                        err = cc_pointer_of(p, symbol_type, &result_type);
                        if(err) return err;
                        err = cc_expect_punct(p, ')');
                        if(err) return err;
                        CcExpr* node = cc_make_expr(p, CC_EXPR_MODULE_REFLECT, tok.loc, result_type, 1);
                        if(!node) return CC_OOM_ERROR;
                        node->lhs = operand;
                        node->module.op = CC_MODULE_SYMBOL;
                        node->values[0] = name;
                        operand = node;
                        continue;
                    }
                    if(sv_equals(mname, SV("run"))){
                        err = cc_expect_punct(p, '(');
                        if(err) return err;
                        err = cc_expect_punct(p, ')');
                        if(err) return err;
                        CcExpr* node = cc_make_expr(p, CC_EXPR_MODULE_REFLECT, tok.loc, ccqt_basic(CCBT_int), 0);
                        if(!node) return CC_OOM_ERROR;
                        node->module.op = CC_MODULE_RUN;
                        node->lhs = operand;
                        operand = node;
                        continue;
                    }
                    if(sv_equals(mname, SV("parse_type"))){
                        err = cc_expect_punct(p, '(');
                        if(err) return err;
                        CcExpr* name;
                        err = cc_parse_assignment_expr(p, vc, &name, CCQT_NONE);
                        if(err) return err;
                        if(!cc_implicit_convertible(p, name->type, p->const_char_star))
                            return cc_error(p, name->loc, "_Module.parse_type first argument must be convertible to const char*");
                        err = cc_implicit_cast(p, name, p->const_char_star, &name);
                        if(err) return err;
                        err = cc_expect_punct(p, ')');
                        if(err) return err;
                        CcExpr* node = cc_make_expr(p, CC_EXPR_MODULE_REFLECT, tok.loc, ccqt_basic(CCBT__Type), 1);
                        if(!node) return CC_OOM_ERROR;
                        node->module.op = CC_MODULE_PARSE_TYPE;
                        node->lhs = operand;
                        node->values[0] = name;
                        operand = node;
                        continue;
                    }
                    CcModuleOp module_op = CC_MODULE_NONE;
                    CcQualType module_result_type = ccqt_basic(cc_target(p)->size_type);
                    _Bool module_method = 0;
                    if(sv_equals(mname, SV("func_count")))
                        module_op = CC_MODULE_FUNC_COUNT;
                    else if(sv_equals(mname, SV("func"))){
                        module_op = CC_MODULE_FUNC;
                        module_result_type = p->builtin_module_member;
                        module_method = 1;
                    }
                    else if(sv_equals(mname, SV("func_decl"))){
                        module_op = CC_MODULE_FUNC_INFO;
                        module_result_type = p->builtin_module_member;
                        module_method = 1;
                    }
                    else if(sv_equals(mname, SV("var_count")))
                        module_op = CC_MODULE_VAR_COUNT;
                    else if(sv_equals(mname, SV("var"))){
                        module_op = CC_MODULE_VAR;
                        module_result_type = p->builtin_module_member;
                        module_method = 1;
                    }
                    else if(sv_equals(mname, SV("type_count"))){
                        module_op = CC_MODULE_TYPE_COUNT;
                    }
                    else if(sv_equals(mname, SV("type"))){
                        module_op = CC_MODULE_TYPE;
                        module_result_type = p->builtin_module_member;
                        module_method = 1;
                    }
                    if(module_op != CC_MODULE_NONE){
                        CcExpr* node = cc_make_expr(p, CC_EXPR_MODULE_REFLECT, tok.loc, module_result_type, module_method ? 1 : 0);
                        if(!node) return CC_OOM_ERROR;
                        node->lhs = operand;
                        node->module.op = module_op;
                        if(module_method){
                            err = cc_expect_punct(p, '(');
                            if(err) return err;
                            CcExpr* idx;
                            err = cc_parse_assignment_expr(p, vc, &idx, CCQT_NONE);
                            if(err) return err;
                            if(!cc_implicit_convertible(p, idx->type, ccqt_basic(cc_target(p)->size_type)))
                                return cc_error(p, idx->loc, "_Module reflection index must be convertible to size_t");
                            err = cc_implicit_cast(p, idx, ccqt_basic(cc_target(p)->size_type), &idx);
                            if(err) return err;
                            err = cc_expect_punct(p, ')');
                            if(err) return err;
                            node->values[0] = idx;
                        }
                        operand = node;
                        continue;
                    }
                    goto fucs;
                }
                if(tk == CC_STRUCT){
                    CcStruct* s = ccqt_as_struct(agg_type);
                    CcField* field;
                    err = cc_lookup_field(p, s->fields, s->field_count, member_name, NULL, &member_type, &member_owner, &field);
                    if(err) return err;
                    if(field && field->is_method) method = field->method;
                    if(member_type.bits) member_type.quals |= agg_type.quals;
                }
                else if(tk == CC_UNION){
                    CcUnion* u = ccqt_as_union(agg_type);
                    CcField* field;
                    err = cc_lookup_field(p, u->fields, u->field_count, member_name, NULL, &member_type, &member_owner, &field);
                    if(err) return err;
                    if(field && field->is_method) method = field->method;
                    if(member_type.bits) member_type.quals |= agg_type.quals;
                }
                else if(tk == CC_BASIC && agg_type.basic.kind == CCBT__Type){
                    CcQualType result_type;
                    _Bool is_method = 0;
                    CcTypeIntrospectionOp ti_op = (CcTypeIntrospectionOp)(uintptr_t)AM_get(&p->type_intro, member_name);
                    switch(ti_op){
                    case CC_TYPE_NONE:
                        goto fucs;
                    case CC_TYPE_PUSH_METHOD: {
                        if(p->current != &p->global && p->current != p->file_scope)
                            return cc_error(p, member.loc, "push method only allowed at global scope");
                        CcExpr* tv;
                        err = cc_eval_expr(&(CcEvalCtx){.parser = p}, operand, &tv);
                        if(err == CC_OVERFLOW_ERROR) err = CC_NOT_CONSTANT_ERROR;
                        if(err && err != CC_NOT_CONSTANT_ERROR) return err;
                        if(err || !ccqt_bt_eq(tv->type, CCBT__Type))
                            return cc_error(p, member.loc, "push_method requires a constant type");
                        CcQualType qt = tv->type_value;
                        CcTypeKind k = ccqt_kind(qt);
                        if(k != CC_STRUCT && k != CC_UNION)
                            return cc_error(p, member.loc, "push_method requires a struct or union type");
                        err = cc_expect_punct(p, '(');
                        if(err) return err;
                        CcToken mname_tok;
                        err = cc_next_token(p, &mname_tok);
                        if(err) return err;
                        if(mname_tok.type != CC_IDENTIFIER)
                            return cc_error(p, mname_tok.loc, "push_method: expected method name");
                        Atom method_name = mname_tok.ident.ident;
                        err = cc_expect_punct(p, CC_comma);
                        if(err) return err;
                        CcExpr* func_expr;
                        err = cc_parse_assignment_expr(p, vc, &func_expr, CCQT_NONE);
                        if(err) return err;
                        CcFunc* func;
                        if(func_expr->kind == CC_EXPR_FUNCTION)
                            func = func_expr->func;
                        else
                            return cc_error(p, func_expr->loc, "push_method: expected function");
                        func->name = method_name;
                        cc_release_expr(p, func_expr);
                        err = cc_expect_punct(p, CC_rparen);
                        if(err) return err;
                        CcStruct* s = ccqt_as_struct(qt);
                        Allocator al = cc_allocator(p);
                        CcField* new_fields = Allocator_realloc(al, s->fields, s->field_count * sizeof *new_fields, (s->field_count + 1)*sizeof *new_fields);
                        if(!new_fields) return CC_OOM_ERROR;
                        s->fields = new_fields;
                        s->fields[s->field_count++] = (CcField){
                            .type = (CcQualType){.bits=(uintptr_t)func->type},
                            .method = func,
                            .is_method = 1,
                            .loc = operand->loc,
                        };
                        cc_release_expr(p, operand);
                        CcExpr* dummy = cc_value_expr(p, tok.loc, ccqt_basic(CCBT_void));
                        if(!dummy) return CC_OOM_ERROR;
                        operand = dummy;
                        continue;
                    }
                    case CC_TYPE_FIELD:
                        result_type = p->builtin_field;
                        is_method = 1;
                        break;
                    case CC_TYPE_METHOD:
                        result_type = p->builtin_method;
                        is_method = 1;
                        break;
                    case CC_TYPE_HAS_FIELD:
                    case CC_TYPE_HAS_METHOD:
                        result_type = ccqt_basic(CCBT_bool);
                        is_method = 1;
                        break;
                    case CC_TYPE_ENUMERATOR:
                        result_type = p->builtin_enumerator;
                        is_method = 1;
                        break;
                    case CC_TYPE_ENUMERATORS:
                    case CC_TYPE_FIELDS:
                    case CC_TYPE_METHODS:
                        result_type = ccqt_basic(cc_target(p)->size_type);
                        break;
                    case CC_TYPE_NAME:
                    case CC_TYPE_TAG:
                        result_type = p->const_char_slice;
                        break;
                    case CC_TYPE_IS_VALID:
                    case CC_TYPE_IS_INVALID:
                    case CC_TYPE_IS_INTEGER:
                    case CC_TYPE_IS_FLOAT:
                    case CC_TYPE_IS_ARITHMETIC:
                    case CC_TYPE_IS_POINTER:
                    case CC_TYPE_IS_STRUCT:
                    case CC_TYPE_IS_UNION:
                    case CC_TYPE_IS_ARRAY:
                    case CC_TYPE_IS_VECTOR:
                    case CC_TYPE_IS_SLICE:
                    case CC_TYPE_IS_FUNCTION:
                    case CC_TYPE_IS_ENUM:
                    case CC_TYPE_IS_CONST:
                    case CC_TYPE_IS_VOLATILE:
                    case CC_TYPE_IS_ATOMIC:
                    case CC_TYPE_IS_UNSIGNED:
                    case CC_TYPE_IS_SIGNED:
                    case CC_TYPE_IS_CALLABLE:
                    case CC_TYPE_IS_VARIADIC:
                    case CC_TYPE_IS_INCOMPLETE:
                        result_type = ccqt_basic(CCBT_bool);
                        break;
                    case CC_TYPE_SIZEOF:
                    case CC_TYPE_ALIGNOF:
                    case CC_TYPE_COUNT:
                    case CC_TYPE_PARAM_COUNT:
                        result_type = ccqt_basic(cc_target(p)->size_type);
                        break;
                    case CC_TYPE_LOC:
                        result_type = p->builtin_src_loc;
                        break;
                    case CC_TYPE_POINTEE:
                    case CC_TYPE_UNQUAL:
                    case CC_TYPE_RETURN_TYPE:
                    case CC_TYPE_ELEMENT_TYPE:
                    case CC_TYPE_UNDERLYING_TYPE:
                        result_type = ccqt_basic(CCBT__Type);
                        break;
                    case CC_TYPE_PARAM_TYPE:
                        result_type = ccqt_basic(CCBT__Type);
                        is_method = 1;
                        break;
                    case CC_TYPE_IS_CALLABLE_WITH:
                    case CC_TYPE_IS_CALLABLE_THROUGH:
                    case CC_TYPE_CASTABLE_TO:
                        result_type = ccqt_basic(CCBT_bool);
                        is_method = 1;
                        break;
                    case CC_TYPE_MAKE_ANY:
                        result_type = ccqt_basic(CCBT__Any);
                        is_method= 1;
                        break;
                    }
                    if(is_method){
                        err = cc_expect_punct(p, '(');
                        if(err) return err;
                        CcExpr* arg_val;
                        if(ti_op == CC_TYPE_FIELD || ti_op == CC_TYPE_METHOD
                            || ti_op == CC_TYPE_HAS_FIELD || ti_op == CC_TYPE_HAS_METHOD
                            || ti_op == CC_TYPE_PARAM_TYPE || ti_op == CC_TYPE_ENUMERATOR){
                            CcExpr* arg_expr;
                            err = cc_parse_assignment_expr(p, vc, &arg_expr, CCQT_NONE);
                            if(err) return err;
                            CcQualType size_type = ccqt_basic(cc_target(p)->size_type);
                            CcQualType arg_type = ti_op == CC_TYPE_HAS_FIELD || ti_op == CC_TYPE_HAS_METHOD
                                || ((ti_op == CC_TYPE_FIELD || ti_op == CC_TYPE_METHOD)
                                    && cc_implicit_convertible(p, arg_expr->type, p->const_char_slice))
                                ? p->const_char_slice : size_type;
                            err = cc_implicit_cast(p, arg_expr, arg_type, &arg_expr);
                            if(err) return err;
                            arg_val = arg_expr;
                        }
                        else if(ti_op == CC_TYPE_MAKE_ANY){
                            CcExpr* arg_expr;
                            err = cc_parse_assignment_expr(p, vc, &arg_expr, CCQT_NONE);
                            if(err) return err;
                            err = cc_implicit_cast(p, arg_expr, p->const_void_star, &arg_expr);
                            if(err) return err;
                            arg_val = arg_expr;
                        }
                        else if(ti_op == CC_TYPE_IS_CALLABLE_THROUGH){
                            err = cc_parse_assignment_expr(p, vc, &arg_val, CCQT_NONE);
                            if(err) return err;
                            if(!ccqt_bt_eq(arg_val->type, CCBT__Type)){
                                cc_release_expr(p, arg_val);
                                return cc_error(p, tok.loc, "is_callable_through requires a function type");
                            }
                        }
                        else {
                            CcQualType arg_type;
                            CcToken peek2;
                            err = cc_peek(p, &peek2);
                            if(err) return err;
                            if(cc_is_type_start(p, &peek2)){
                                err = cc_parse_type_name(p, &arg_type, NULL);
                                if(err) return err;
                            }
                            else {
                                CcExpr* arg_expr;
                                err = cc_parse_assignment_expr(p, vc, &arg_expr, CCQT_NONE);
                                if(err) return err;
                                arg_type = arg_expr->type;
                                cc_release_expr(p, arg_expr);
                            }
                            arg_val = cc_make_expr(p, CC_EXPR_VALUE, tok.loc, ccqt_basic(CCBT__Type), 0);
                            if(!arg_val) return CC_OOM_ERROR;
                            arg_val->uinteger = arg_type.bits;
                        }
                        err = cc_expect_punct(p, CC_rparen);
                        if(err) return err;
                        CcExpr* node = cc_make_expr(p, CC_EXPR_TYPE_INTROSPECTION, tok.loc, result_type, 1);
                        if(!node) return CC_OOM_ERROR;
                        node->type_introspection.op = ti_op;
                        node->lhs = operand;
                        node->values[0] = arg_val;
                        operand = node;
                    }
                    else {
                        CcExpr* node = cc_make_expr(p, CC_EXPR_TYPE_INTROSPECTION, tok.loc, result_type, 0);
                        if(!node) return CC_OOM_ERROR;
                        node->type_introspection.op = ti_op;
                        node->lhs = operand;
                        operand = node;
                    }
                    continue;
                }
                else if(ccqt_bt_eq(agg_type, CCBT__Any)){
                    StringView mname = {member_name->length, member_name->data};
                    _Bool payload = sv_equals(mname, SV("payload"));
                    if(payload){
                        member_type = ccqt_basic(CCBT_void);
                        floc = offsetof(CiRtAny, payload);
                    }
                    else if(sv_equals(mname, SV("type")))
                        member_type = ccqt_basic(CCBT__Type);
                    else if(sv_equals(mname, SV("as"))){
                        err = cc_expect_punct(p, CC_lparen);
                        if(err) return err;
                        CcExpr* te;
                        err = cc_parse_assignment_expr(p, CC_CONSTEXPR_VALUE, &te, CCQT_NONE);
                        if(err) return err;
                        CcExpr* tv = NULL;
                        err = cc_eval_expr(&(CcEvalCtx){.parser = p}, te, &tv);
                        cc_release_expr(p, te);
                        if(err) return err == CC_NOT_CONSTANT_ERROR ? cc_error(p, member.loc, "_Any.as requires a constant type") : err;
                        if(!ccqt_bt_eq(tv->type, CCBT__Type)){
                            cc_release_expr(p, tv);
                            return cc_error(p, member.loc, "_Any.as requires a complete, non-array object type fitting its payload");
                        }
                        member_type = tv->type_value;
                        cc_release_expr(p, tv);
                        if(ccqt_kind(member_type) == CC_FUNCTION){ // auto-decay to pointer for convenience.
                            err = cc_pointer_of(p, member_type, &member_type);
                            if(err) return err;
                        }
                        if(!cc_any_payload_type(p, member_type))
                            return cc_error(p, member.loc, "_Any.as requires a complete, non-array object type fitting its payload");
                        err = cc_expect_punct(p, CC_rparen);
                        if(err) return err;
                        floc = offsetof(CiRtAny, payload);
                    }
                    if(member_type.bits){
                        member_type.quals |= agg_type.quals;
                        CcExpr* field = cc_make_expr(p, mkind, tok.loc, member_type, 1);
                        if(!field) return CC_OOM_ERROR;
                        field->is_lvalue = mkind == CC_EXPR_ARROW || operand->is_lvalue;
                        err = cc_set_member_path(p, field, agg_type, member_name, 0, floc != 0);
                        if(err){ _cc_release_expr(p, field, 1); return err; }
                        field->values[0] = operand;
                        operand = field;
                        if(payload){
                            CcQualType ptr;
                            err = cc_pointer_of(p, member_type, &ptr);
                            if(err) return err;
                            CcExpr* addr = cc_unary_expr(p, CC_EXPR_ADDR, tok.loc, ptr, field);
                            if(!addr) return CC_OOM_ERROR;
                            operand = addr;
                        }
                        continue;
                    }
                }
                else if(tk == CC_SLICE){
                    StringView mname = {member_name->length, member_name->data};
                    if(sv_equals(mname, SV("count")) || sv_equals(mname, SV("length"))){
                        member_type = ccqt_basic(cc_target(p)->size_type);
                        floc = offsetof(CiRtSlice, count);
                    }
                    else if(sv_equals(mname, SV("data"))){
                        err = cc_pointer_of(p, ccqt_as_slice(agg_type)->pointee, &member_type);
                        if(err) return err;
                        // XXX: should this use target?
                        // All of our supported platforms match the host though, so idk.
                        floc = offsetof(CiRtSlice, data);
                    }
                }
                else {
                }
                fucs:;
                if(!member_type.bits){
                    // FUCS: x.foo(args) -> foo(x, args)
                    CcFunc* fucs_func = cc_scope_lookup_func(p->current, member_name, CC_SCOPE_WALK_CHAIN);
                    if(!fucs_func){
                        MStringBuilder* sb = cc_start_error(p, member.loc, "No member named '%s' for '", member_name->data);
                        cc_print_type(sb, agg_type);
                        if(tk == CC_STRUCT || tk == CC_UNION)
                            msb_sprintf(sb, "'");
                        else msb_sprintf(sb, "' (not a struct or union)");
                        return cc_finish_error(p, member.loc);
                    }
                    CcExpr* fnode = cc_make_expr(p, CC_EXPR_FUNCTION, tok.loc, (CcQualType){.bits = (uintptr_t)fucs_func->type}, 0);
                    if(!fnode) return CC_OOM_ERROR;
                    fnode->func = fucs_func;
                    // Prefer normal argument conversion (including decay and boxing)
                    // before adapting a receiver by address or dereference.
                    if(fucs_func->type->param_count > 0 && cc_implicit_convertible(p, operand->type, fucs_func->type->params[0])){
                        receiver = operand;
                    }
                    else if(fucs_func->type->param_count > 0 && ccqt_kind(fucs_func->type->params[0]) == CC_POINTER && ccqt_kind(operand->type) != CC_POINTER && operand->is_lvalue){
                        CcQualType addr_type;
                        err = cc_pointer_of(p, operand->type, &addr_type);
                        if(err) return err;
                        CcExpr* addr = cc_make_expr(p, CC_EXPR_ADDR, tok.loc, addr_type, 0);
                        if(!addr) return CC_OOM_ERROR;
                        addr->lhs = operand;
                        receiver = addr;
                    }
                    else if(fucs_func->type->param_count > 0 && ccqt_kind(operand->type) == CC_POINTER && ccqt_kind(fucs_func->type->params[0]) != CC_POINTER){
                        CcQualType deref_type;
                        err = cc_deref_type(p, operand->type, &deref_type, tok.loc, 0);
                        if(err) return err;
                        CcExpr* deref = cc_make_expr(p, CC_EXPR_DEREF, tok.loc, deref_type, 0);
                        if(!deref) return CC_OOM_ERROR;
                        deref->lhs = operand;
                        deref->is_lvalue = 1;
                        receiver = deref;
                    }
                    else {
                        receiver = operand;
                    }
                    operand = fnode;
                    continue;
                }
                if(agg_type.is_atomic && (tk == CC_STRUCT || tk == CC_UNION))
                    return cc_error(p, member.loc, "member access on atomic struct or union is undefined behavior");
                if(method){
                    if(member_owner.bits){
                        member_owner.quals |= agg_type.quals;
                        CcExpr* subobject = cc_make_expr(p, mkind, tok.loc, member_owner, 1);
                        if(!subobject) return CC_OOM_ERROR;
                        subobject->is_lvalue = mkind == CC_EXPR_ARROW || operand->is_lvalue;
                        err = cc_set_member_path(p, subobject, agg_type, member_name, 1, 0);
                        if(err){ _cc_release_expr(p, subobject, 1); return err; }
                        subobject->values[0] = operand;
                        operand = subobject;
                    }
                    CcExpr* mnode = cc_make_expr(p, CC_EXPR_FUNCTION, tok.loc, member_type, 0);
                    if(!mnode) return CC_OOM_ERROR;
                    mnode->func = method;
                    if(method->type->param_count > 0 && ccqt_kind(method->type->params[0]) == CC_POINTER && ccqt_kind(operand->type) != CC_POINTER && operand->is_lvalue){
                        CcQualType addr_type;
                        err = cc_pointer_of(p, operand->type, &addr_type);
                        if(err) return err;
                        CcExpr* addr = cc_make_expr(p, CC_EXPR_ADDR, tok.loc, addr_type, 0);
                        if(!addr) return CC_OOM_ERROR;
                        addr->lhs = operand;
                        receiver = addr;
                    }
                    else if(method->type->param_count > 0 && ccqt_kind(operand->type) == CC_POINTER && ccqt_kind(method->type->params[0]) != CC_POINTER){
                        CcQualType deref_type;
                        err = cc_deref_type(p, operand->type, &deref_type, tok.loc, 0);
                        if(err) return err;
                        CcExpr* deref = cc_make_expr(p, CC_EXPR_DEREF, tok.loc, deref_type, 0);
                        if(!deref) return CC_OOM_ERROR;
                        deref->lhs = operand;
                        deref->is_lvalue = 1;
                        receiver = deref;
                    }
                    else {
                        receiver = operand;
                    }
                    operand = mnode;
                    continue;
                }
                CcExpr* mnode = cc_make_expr(p, mkind, tok.loc, member_type, 1);
                if(!mnode) return CC_OOM_ERROR;
                mnode->is_lvalue = mkind == CC_EXPR_ARROW || operand->is_lvalue;
                err = cc_set_member_path(p, mnode, agg_type, member_name, 0, floc != 0);
                if(err){ _cc_release_expr(p, mnode, 1); return err; }
                mnode->values[0] = operand;
                operand = mnode;
                continue;
            }
            case CC_lparen: {
                if(vc > CC_RUNTIME_VALUE)
                    return cc_error(p, tok.loc, "function call in constant expression");
                CcToken peek;
                err = cc_next_token(p, &peek);
                if(err) return err;
                _Bool typed_pack = operand->kind == CC_EXPR_FUNCTION
                                && operand->func->params.count
                                && operand->func->params.data[operand->func->params.count-1].typed_pack;
                if(peek.type == CC_PUNCTUATOR && peek.punct.punct == CC_rparen && !receiver && !typed_pack){
                    CcQualType ct = operand->type;
                    if(ccqt_kind(ct) == CC_POINTER)
                        ct = ccqt_as_ptr(ct)->pointee;
                    if(ccqt_kind(ct) != CC_FUNCTION)
                        return cc_error(p, tok.loc, "Called object is not a function or function pointer");
                    CcFunction* ftype = ccqt_as_function(ct);
                    if(!ftype->no_prototype && ftype->param_count != 0){
                        if(ftype->is_variadic)
                            return cc_error(p, tok.loc, "Too few arguments: expected at least %u, got 0", (unsigned)ftype->param_count);
                        return cc_error(p, tok.loc, "Expected %u arguments, got 0", (unsigned)ftype->param_count);
                    }
                    CcExpr* node = cc_make_expr(p, CC_EXPR_CALL, tok.loc, ftype->return_type, 0);
                    if(!node) return CC_OOM_ERROR;
                    node->lhs = operand;
                    operand = node;
                    continue;
                }
                _Bool rparen_consumed = 0;
                if(peek.type == CC_PUNCTUATOR && peek.punct.punct == CC_rparen)
                    rparen_consumed = 1;
                else
                    cc_unget(p, &peek);
                CcQualType ct = operand->type;
                if(ccqt_kind(ct) == CC_POINTER)
                    ct = ccqt_as_ptr(ct)->pointee;
                if(ccqt_kind(ct) != CC_FUNCTION)
                    return cc_error(p, tok.loc, "Called object is not a function or function pointer");
                CcFunction* ftype = ccqt_as_function(ct);
                CcFuncParam*_Null_unspecified param_names = NULL;
                size_t param_names_count = 0;
                if(operand->kind == CC_EXPR_FUNCTION && operand->func->params.count){
                    param_names = operand->func->params.data;
                    param_names_count = operand->func->params.count;
                }
                Allocator call_al = cc_scratch_allocator(p);
                Parray(CcExpression) args = {0};
                _Bool has_receiver = receiver != NULL;
                if(receiver){
                    err = pa_push(&args, call_al, receiver);
                    if(err){ return CC_OOM_ERROR; }
                    receiver = NULL;
                }
                _Bool has_named = 0;
                _Bool explicit_pack = 0;
                uint32_t pack_index = typed_pack ? ftype->param_count-1 : UINT32_MAX;
                uint32_t positional_index = has_receiver ? 1 : 0;
                if(rparen_consumed) goto call_args_done;
                for(;;){
                    CcToken dot;
                    err = cc_peek(p, &dot);
                    if(err) goto call_cleanup;
                    if(dot.type == CC_PUNCTUATOR && dot.punct.punct == CC_dot){
                        cc_next_token(p, &dot);
                        CcToken name_tok;
                        err = cc_next_token(p, &name_tok);
                        if(err) goto call_cleanup;
                        if(name_tok.type != CC_IDENTIFIER){
                            err = cc_error(p, name_tok.loc, "expected parameter name after '.'");
                            goto call_cleanup;
                        }
                        CcToken eq;
                        err = cc_next_token(p, &eq);
                        if(err) goto call_cleanup;
                        if(eq.type != CC_PUNCTUATOR || eq.punct.punct != CC_assign){
                            err = cc_error(p, eq.loc, "expected '=' after parameter name");
                            goto call_cleanup;
                        }
                        if(!param_names){
                            err = cc_error(p, dot.loc, "named arguments require a function with known parameter names");
                            goto call_cleanup;
                        }
                        Atom name = name_tok.ident.ident;
                        uint32_t idx = UINT32_MAX;
                        for(size_t j = 0; j < param_names_count; j++){
                            if(param_names[j].name == name){ idx = (uint32_t)j; break; }
                        }
                        if(idx == UINT32_MAX){
                            err = cc_error(p, name_tok.loc, "no parameter named '%.*s'", name->length, name->data);
                            goto call_cleanup;
                        }
                        // Parse the value
                        CcExpr* arg;
                        err = cc_parse_assignment_expr(p, vc, &arg, ftype->params[idx]);
                        if(err) goto call_cleanup;
                        // Ensure args array is big enough
                        if(args.count <= idx){
                            err = pa_zextend(&args, call_al, idx-args.count+1);
                            if(err) goto call_cleanup;
                        }
                        if(args.data[idx] != NULL){
                            err = cc_error(p, name_tok.loc, "duplicate argument for parameter '%.*s'", name->length, name->data);
                            goto call_cleanup;
                        }
                        args.data[idx] = arg;
                        if(idx == pack_index) explicit_pack = 1;
                        has_named = 1;
                    }
                    else if(dot.type == CC_PUNCTUATOR && dot.punct.punct == CC_lbracket){
                        cc_next_token(p, &dot);
                        CcExpr* idx_expr;
                        err = cc_parse_assignment_expr(p, vc, &idx_expr, CCQT_NONE);
                        if(err) goto call_cleanup;
                        int64_t idx_signed;
                        err = cc_eval_integer(&(CcEvalCtx){p}, idx_expr, &idx_signed);
                        cc_release_expr(p, idx_expr);
                        if(err){
                            if(err == CC_NOT_CONSTANT_ERROR)
                                err = cc_error(p, dot.loc, "positional designator must be a constant integer expression");
                            goto call_cleanup;
                        }
                        if(idx_signed < 0 || (uint64_t)idx_signed >= ftype->param_count){
                            err = cc_error(p, dot.loc, "positional designator value out of range");
                            goto call_cleanup;
                        }
                        uint32_t idx = (uint32_t)idx_signed;
                        err = cc_expect_punct(p, CC_rbracket);
                        if(err) goto call_cleanup;
                        CcToken eq;
                        err = cc_next_token(p, &eq);
                        if(err) goto call_cleanup;
                        if(eq.type != CC_PUNCTUATOR || eq.punct.punct != CC_assign){
                            err = cc_error(p, eq.loc, "expected '=' after positional designator");
                            goto call_cleanup;
                        }
                        CcExpr* arg;
                        err = cc_parse_assignment_expr(p, vc, &arg, ftype->params[idx]);
                        if(err) goto call_cleanup;
                        if(args.count <= idx){
                            err = pa_zextend(&args, call_al, idx-args.count+1);
                            if(err) goto call_cleanup;
                        }
                        if(args.data[idx] != NULL){
                            err = cc_error(p, dot.loc, "duplicate argument for position %u", (unsigned)idx);
                            goto call_cleanup;
                        }
                        args.data[idx] = arg;
                        if(idx == pack_index) explicit_pack = 1;
                        has_named = 1;
                    }
                    else {
                        if(has_named){
                            while(positional_index < args.count && args.data[positional_index] != NULL)
                                positional_index++;
                        }
                        CcExpr* arg;
                        if(explicit_pack && positional_index >= pack_index){
                            err = cc_error(p, dot.loc, "cannot combine an explicit typed pack with trailing arguments");
                            goto call_cleanup;
                        }
                        CcQualType hint = positional_index < ftype->param_count ? ftype->params[positional_index] : CCQT_NONE;
                        if(typed_pack && positional_index >= pack_index)
                            hint = ccqt_as_slice(ftype->params[pack_index])->pointee;
                        err = cc_parse_assignment_expr(p, vc, &arg, hint);
                        if(err) goto call_cleanup;
                        if(has_named){
                            if(args.count <= positional_index){
                                err = pa_zextend(&args, call_al, positional_index-args.count+1);
                                if(err) goto call_cleanup;
                            }
                            if(args.data[positional_index] != NULL){
                                err = cc_error(p, arg->loc, "argument position %u already filled by named argument", (unsigned)positional_index);
                                goto call_cleanup;
                            }
                            args.data[positional_index] = arg;
                        }
                        else {
                            err = pa_push(&args, call_al, arg);
                            if(err){ err = CC_OOM_ERROR; goto call_cleanup; }
                        }
                        positional_index++;
                    }
                    CcToken sep;
                    err = cc_next_token(p, &sep);
                    if(err) goto call_cleanup;
                    if(sep.type == CC_PUNCTUATOR && sep.punct.punct == CC_rparen)
                        break;
                    if(sep.type != CC_PUNCTUATOR || sep.punct.punct != CC_comma){
                        err = cc_error(p, sep.loc, "Expected ',' or ')' in function call");
                        goto call_cleanup;
                    }
                    err = cc_peek(p, &sep);
                    if(err) goto call_cleanup;
                    if(sep.type == CC_PUNCTUATOR && sep.punct.punct == CC_rparen){
                        cc_next_token(p, &sep);
                        break;
                    }
                }
                call_args_done:;
                if(typed_pack && !explicit_pack){
                    if(args.count < pack_index){
                        err = cc_error(p, tok.loc, "Too few arguments: expected at least %u, got %u", pack_index, (unsigned)args.count);
                        goto call_cleanup;
                    }
                    err = cc_pack_call_args(p, tok.loc, ftype->params[pack_index], &args, pack_index);
                    if(err) goto call_cleanup;
                }
                uint32_t nargs = (uint32_t)args.count;
                if(has_named){
                    for(uint32_t i = 0; i < nargs && i < ftype->param_count; i++){
                        if(!args.data[i]){
                            Atom pn = (param_names && i < param_names_count) ? param_names[i].name : NULL;
                            err = cc_error(p, tok.loc, "missing argument for parameter '%.*s'",
                                pn ? (int)pn->length : 1, pn ? pn->data : "?");
                            goto call_cleanup;
                        }
                    }
                }
                if(!ftype->is_variadic && !ftype->no_prototype && nargs != ftype->param_count){
                    err = cc_error(p, tok.loc, "Expected %u arguments, got %u", (unsigned)ftype->param_count, (unsigned)nargs);
                    goto call_cleanup;
                }
                if(ftype->is_variadic && nargs < ftype->param_count){
                    err = cc_error(p, tok.loc, "Too few arguments: expected at least %u, got %u", (unsigned)ftype->param_count, (unsigned)nargs);
                    goto call_cleanup;
                }
                for(uint32_t i = 0; i < nargs; i++){
                    CcExpr** argp = (CcExpr**)&args.data[i];
                    if(!ftype->no_prototype && i < ftype->param_count){
                        err = cc_implicit_cast(p, *argp, ftype->params[i], argp);
                        if(err) goto call_cleanup;
                    }
                    else {
                        CcQualType at = cc_arithmetic_operand_type(p, *argp);
                        if(ccqt_kind(at) == CC_ARRAY && !ccqt_as_array(at)->is_vector){
                            CcQualType ptr_type;
                            err = cc_pointer_of(p, cc_array_element_type(at), &ptr_type);
                            if(err) goto call_cleanup;
                            err = cc_implicit_cast(p, *argp, ptr_type, argp);
                            if(err) goto call_cleanup;
                        }
                        else {
                            if(ccqt_kind(at) == CC_ENUM)
                                at = ccqt_as_enum(at)->underlying;
                            if(ccqt_is_basic(at)){
                                CcBasicTypeKind k = at.basic.kind;
                                if(k == CCBT_float){
                                    err = cc_implicit_cast(p, *argp, ccqt_basic(CCBT_double), argp);
                                    if(err) goto call_cleanup;
                                }
                                else if(ccbt_is_integer(k)){
                                    CcQualType promoted;
                                    err = cc_integer_promote(p, at, &promoted, (*argp)->loc);
                                    if(err) goto call_cleanup;
                                    err = cc_implicit_cast(p, *argp, promoted, argp);
                                    if(err) goto call_cleanup;
                                }
                            }
                        }
                    }
                }
                if(operand->kind == CC_EXPR_FUNCTION && operand->func->printf_like){
                    uint32_t fi = operand->func->type->param_count - 1;
                    SrcLoc fmt_loc = fi < nargs ? ((CcExpr*)args.data[fi])->loc : tok.loc;
                    err = cc_check_printf_format(p, operand->func, (CcExpr*_Nonnull*_Nonnull)args.data, nargs, fmt_loc);
                    if(err) goto call_cleanup;
                }
                CcExpr* node = cc_make_expr(p, CC_EXPR_CALL, tok.loc, ftype->return_type, nargs);
                if(!node){ err = CC_OOM_ERROR; goto call_cleanup; }
                node->call.nargs = nargs;
                node->lhs = operand;
                memcpy(node->values, args.data, nargs * sizeof(CcExpr*));
                operand = node;
                err = 0;
                call_cleanup:
                pa_cleanup(&args, call_al);
                if(err) return err;
                continue;
            }
            default:
                cc_unget(p, &tok);
                goto done;
        }
    }
done:
    *out = operand;
    return 0;
}

static
int
cc_skip_static_else_chain(CcParser* p){
    int err;
    CcToken tok;
    for(;;){
        err = cc_peek(p, &tok);
        if(err) return err;
        if(tok.type != CC_KEYWORD || tok.kw.kw != CC_else)
            return 0;
        cc_next_token(p, &tok);
        err = cc_peek(p, &tok);
        if(err) return err;
        if(tok.type == CC_KEYWORD && tok.kw.kw == CC_if){
            cc_next_token(p, &tok);
            err = cc_expect_punct(p, '(');
            if(err) return err;
            int depth = 1;
            while(depth > 0){
                err = cc_next_token(p, &tok);
                if(err) return err;
                if(tok.type == CC_EOF) return cc_error(p, tok.loc, "unterminated static if condition");
                if(tok.type == CC_PUNCTUATOR){
                    if(tok.punct.punct == '(') depth++;
                    else if(tok.punct.punct == ')') depth--;
                }
            }
            err = cc_skip_braced_block(p);
            if(err) return err;
            continue;
        }
        return cc_skip_braced_block(p);
    }
}

static
int
cc_parse_static_if_body(CcParser* p, SrcLoc loc){
    int err = cc_expect_punct(p, '{');
    if(err) return err;
    for(;;){
        CcToken peek;
        err = cc_peek(p, &peek);
        if(err) return err;
        if(peek.type == CC_EOF) return cc_error(p, loc, "unterminated static if block");
        if(peek.type == CC_PUNCTUATOR && peek.punct.punct == '}'){
            cc_next_token(p, &peek);
            break;
        }
        err = cc_parse_one(p);
        if(err) return err;
    }
    return 0;
}

static
int
cc_parse_static_if(CcParser* p, SrcLoc loc){
    int err;
    CcToken tok;
    _Bool predicate;
    {
        err = cc_expect_punct(p, '(');
        if(err) return err;
        CcExpr* cond;
        err = cc_parse_expr(p, CC_CONSTEXPR_VALUE, &cond);
        if(err) return err;
        err = cc_expect_punct(p, ')');
        if(err) return err;
        SrcLoc cond_loc = cond->loc;
        err = cc_eval_truthy(&(CcEvalCtx){p}, cond, &predicate);
        cc_release_expr(p, cond);
        if(err && err != CC_NOT_CONSTANT_ERROR) return err;
        if(err)
            return cc_error(p, cond_loc, "static if condition must be a constant expression");
    }
    if(predicate){
        err = cc_parse_static_if_body(p, loc);
        if(err) return err;
        return cc_skip_static_else_chain(p);
    }
    err = cc_skip_braced_block(p);
    if(err) return err;
    err = cc_peek(p, &tok);
    if(err) return err;
    if(tok.type != CC_KEYWORD || tok.kw.kw != CC_else)
        return 0;
    cc_next_token(p, &tok);
    err = cc_peek(p, &tok);
    if(err) return err;
    if(tok.type == CC_KEYWORD && tok.kw.kw == CC_if){
        cc_next_token(p, &tok);
        return cc_parse_static_if(p, loc);
    }
    return cc_parse_static_if_body(p, loc);
}

static
int
cc_parse_one(CcParser* p){
    int err = cc_parse_one_inner(p);
    if(err) return err;
    return cc_parse_local_methods(p);
}

static
int
cc_parse_one_inner(CcParser* p){
    int err;
    CcToken tok;
    err = cc_peek(p, &tok);
    if(err) return err;
    if(tok.type == CC_KEYWORD && tok.kw.kw == CC_asm){
        cc_next_token(p, &tok);
        err = cc_peek(p, &tok);
        if(err) return err;
        if(tok.type == CC_PUNCTUATOR && tok.punct.punct == '('){
            cc_next_token(p, &tok);
            err = cc_peek(p, &tok);
            if(err) return err;
            if(tok.type == CC_STRING_LITERAL && tok.str.length <= 1){
                cc_next_token(p, &tok);
                err = cc_peek(p, &tok);
                if(err) return err;
            }
            if(tok.type == CC_PUNCTUATOR && tok.punct.punct == ')'){
                cc_next_token(p, &tok);
            }
            else {
                return cc_error(p, tok.loc, "Unable to handle this asm statement: expected empty string and then closing paren");
            }
            err = cc_expect_punct(p, ';');
            if(err) return err;
            return 0;
        }
        else {
            return cc_error(p, tok.loc, "Unable to handle this asm statement");
        }
    }
    if(tok.type == CC_KEYWORD && tok.kw.kw == CC_static){
        CcToken if_tok;
        cc_next_token(p, &tok);
        err = cc_peek(p, &if_tok);
        if(err) return err;
        if(if_tok.type == CC_KEYWORD && if_tok.kw.kw == CC_if){
            cc_next_token(p, &if_tok);
            return cc_parse_static_if(p, tok.loc);
        }
        cc_unget(p, &tok);
    }
    if(cc_is_c23_attribute_start(p)){
        err = cc_parse_c23_attributes(p, &p->attributes);
        if(err) return err;
        err = cc_peek(p, &tok);
        if(err) return err;
        if(tok.type == CC_PUNCTUATOR && tok.punct.punct == ';'){
            err = cc_next_token(p, &tok); // consume ';'
            if(err) return err;
            cc_clear_attributes(&p->attributes);
            return 0;
        }
    }
    CcDeclBase b = {0};
    err = cc_parse_declaration_specifier(p, &b);
    if(err) return err;
    if(b.spec.bits || b.type.bits){
        if(b.spec.sp_typedef && !b.spec.sp_typebits && !b.type.bits){
            CcToken peek;
            err = cc_peek(p, &peek);
            if(err) return err;
            return cc_error(p, peek.loc, "typedef requires a type");
        }
        err = cc_resolve_specifiers(p, &b);
        if(err) return err;
        err = cc_parse_decls(p, &b);
        if(err) return err;
        return 0;
    }

    err = cc_peek(p, &tok);
    if(err) return err;
    switch(tok.type){
        case CC_KEYWORD:
            if(tok.kw.kw == CC_static_assert){
                return cc_handle_static_assert(p);
            }
            goto Ldefault;
        case CC_CONSTANT:
        case CC_EOF:
        case CC_IDENTIFIER:
        case CC_PUNCTUATOR:
        case CC_STRING_LITERAL:
            Ldefault:;
            break;
    }
    {
        CcStmtNode*_Null_unspecified node = NULL;
        err = cc_parse_statement(p, &node);
        if(err) return err;
        return cc_sink_push(p, node);
    }
}

static
int
cc_parse_all(CcParser* p){
    int err = 0;
    CcToken tok;
    CcLabelCtx* lctx = cc_label_ctx(p);
    uint32_t goto_mark = lctx->gotos.count;
    CcStmtSink* sink = cc_push_stmt_sink(p);
    if(!sink) return CC_OOM_ERROR;
    CcScope* saved_file_scope = p->file_scope;
    p->file_scope = p->current;
    for(;;){
        err = cc_peek(p, &tok);
        if(err) break;
        if(tok.type == CC_EOF)
            break;
        err = cc_parse_one(p);
        if(err) break;
    }
    if(!err)
        err = cc_check_gotos(p, lctx);
    if(err){
        // this batch's goto nodes may be freed with the failed batch
        lctx->gotos.count = goto_mark;
    }
    if(!err){
        err = pa_extend(&p->toplevel_nodes, cc_allocator(p), sink->stmts.count, sink->stmts.data);
        if(err) err = CC_OOM_ERROR;
        if(!err) sink->stmts.count = 0;
    }
    cc_pop_stmt_sink(p, sink);
    p->file_scope = saved_file_scope;
    return err;
}

static
void
cc_parser_discard_input(CcParser* p){
    p->pending.count = 0;
    cpp_discard_all_input(&p->cpp);
}

static
CcLabelCtx*
cc_label_ctx(CcParser* p){
    if(p->current_func)
        return &((CcFunc*_Nonnull)p->current_func)->label_ctx;
    return &p->toplevel_label_ctx;
}

static
int
cc_check_gotos(CcParser* p, CcLabelCtx* ctx){
    for(size_t i = 0; i < ctx->gotos.count; i++){
        CcStmtNode* g = ctx->gotos.data[i];
        if(!AM_get(&ctx->labels, g->label))
            return cc_error(p, g->loc, "Use of undeclared label '%.*s'", g->label->length, g->label->data);
    }
    ctx->gotos.count = 0;
    return 0;
}

static
MStringBuilder*
cc_start_error(CcParser* p, SrcLoc loc, const char* fmt, ...){
    va_list va;
    va_start(va, fmt);
    cpp_include_backtrace(&p->cpp, LOG_PRINT_ERROR);
    cpp_msg_preamble(&p->cpp, loc, "error");
    MStringBuilder* sb = &p->cpp.logger->buff;
    msb_vsprintf(sb, fmt, va);
    va_end(va);
    return sb;
}

static
int
cc_finish_error(CcParser* p, SrcLoc loc){
    log_flush(p->cpp.logger, LOG_PRINT_ERROR);
    cpp_msg_postamble(&p->cpp, loc, LOG_PRINT_ERROR);
    return CC_SYNTAX_ERROR;
}

static
int
cc_error(CcParser* p, SrcLoc loc, const char* fmt, ...){
    va_list va;
    va_start(va, fmt);
    cpp_msg(&p->cpp, loc, LOG_PRINT_ERROR, "error", fmt, va);
    va_end(va);
    return CC_SYNTAX_ERROR;
}

static
void
cc_warn(CcParser* p, SrcLoc loc, const char* fmt, ...){
    va_list va;
    va_start(va, fmt);
    cpp_msg(&p->cpp, loc, LOG_PRINT_ERROR, "warning", fmt, va);
    va_end(va);
}

static
void
cc_info(CcParser* p, SrcLoc loc, const char* fmt, ...){
    va_list va;
    va_start(va, fmt);
    cpp_msg(&p->cpp, loc, LOG_PRINT_ERROR, "info", fmt, va);
    va_end(va);
}

static
void
cc_debug(CcParser* p, SrcLoc loc, const char* fmt, ...){
    va_list va;
    va_start(va, fmt);
    cpp_msg(&p->cpp, loc, LOG_PRINT_ERROR, "debug", fmt, va);
    va_end(va);
}

static
int
cc_get_string_literal(CcExpr* e, const char*_Nullable*_Nonnull text, uint32_t* length){
    for(;;){
        if(e->kind == CC_EXPR_CAST){
            e = e->lhs;
            continue;
        }
        if(e->kind == CC_EXPR_VARIABLE && e->var->constexpr_ && e->var->initializer){
            e = e->var->initializer;
            continue;
        }
        break;
    }
    if(e->kind != CC_EXPR_VALUE || ccqt_kind(e->type) != CC_ARRAY){
        *text = NULL;
        return 0;
    }
    CcArray* a = ccqt_as_array(e->type);
    if(!ccqt_is_basic(a->element) || a->element.basic.kind != CCBT_char){
        *text = NULL;
        return 0;
    }
    *length = e->str.length;
    *text = e->text;
    return 0;
}

static
int
cc_printf_format_error(CcParser* p, SrcLoc loc, const char* spec, int spec_len, uint32_t arg_num, CcQualType expected, CcQualType actual){
    cpp_msg_preamble(&p->cpp, loc, "error");
    MStringBuilder* buff = &p->cpp.logger->buff;
    msb_sprintf(buff, "format specifier '%%");
    msb_write_str(buff, spec, spec_len);
    msb_sprintf(buff, "' (argument %u) expects '", (unsigned)arg_num);
    cc_print_type(buff, expected);
    msb_write_literal(buff, "', but argument has type '");
    cc_print_type(buff, actual);
    msb_write_char(buff, '\'');
    log_flush(p->cpp.logger, LOG_PRINT_ERROR);
    cpp_msg_postamble(&p->cpp, loc, LOG_PRINT_ERROR);
    return CC_SYNTAX_ERROR;
}

static
int
cc_check_printf_format(CcParser* p, CcFunc* func, CcExpr*_Nonnull*_Nonnull args, uint32_t nargs, SrcLoc loc){
    uint32_t fmt_idx = func->type->param_count - 1;
    uint32_t first_vararg = func->type->param_count;
    if(fmt_idx >= nargs) return 0;
    const char* fmt = NULL;
    uint32_t slen = 0;
    int err = cc_get_string_literal(args[fmt_idx], &fmt, &slen);
    if(err) return err;
    if(!fmt) return 0;
    if(slen == 0) return 0;
    slen--;
    const CcTargetConfig* tc = cc_target(p);
    uint32_t arg_idx = first_vararg;
    uint32_t expected_args = 0;
    for(uint32_t i = 0; i < slen; i++){
        if(fmt[i] != '%') continue;
        i++;
        if(i >= slen)
            return cc_error(p, loc, "incomplete format specifier at end of string");
        if(fmt[i] == '%') continue;
        while(i < slen && (fmt[i] == '-' || fmt[i] == '+' || fmt[i] == ' ' || fmt[i] == '#' || fmt[i] == '0' || fmt[i] == '\'' || fmt[i] == '$' || fmt[i] == '_'))
            i++;
        if(i >= slen)
            return cc_error(p, loc, "incomplete format specifier at end of string");
        if(fmt[i] == '*'){
            expected_args++;
            if(arg_idx < nargs){
                CcQualType actual = args[arg_idx]->type;
                if(!ccqt_is_basic(actual) || (actual.basic.kind != CCBT_int && actual.basic.kind != CCBT_unsigned))
                    return cc_printf_format_error(p, loc, "*", 1, arg_idx - first_vararg + 1, ccqt_basic(CCBT_int), actual);
            }
            arg_idx++;
            i++;
        }
        else {
            while(i < slen && fmt[i] >= '0' && fmt[i] <= '9') i++;
        }
        if(i >= slen)
            return cc_error(p, loc, "incomplete format specifier at end of string");
        if(fmt[i] == '.'){
            i++;
            if(i >= slen)
                return cc_error(p, loc, "incomplete format specifier at end of string");
            if(fmt[i] == '*'){
                expected_args++;
                if(arg_idx < nargs){
                    CcQualType actual = args[arg_idx]->type;
                    if(!ccqt_is_basic(actual) || (actual.basic.kind != CCBT_int && actual.basic.kind != CCBT_unsigned))
                        return cc_printf_format_error(p, loc, ".*", 2, arg_idx - first_vararg + 1, ccqt_basic(CCBT_int), actual);
                }
                arg_idx++;
                i++;
            }
            else {
                while(i < slen && fmt[i] >= '0' && fmt[i] <= '9') i++;
            }
        }
        if(i >= slen)
            return cc_error(p, loc, "incomplete format specifier at end of string");
        const char* spec_start = fmt + i;
        enum {
            LEN_NONE,
            LEN_h,    // h
            LEN_hh,   // hh
            LEN_l,    // l
            LEN_ll,   // ll
            LEN_L,    // L (long double)
            LEN_z,    // z
            LEN_j,    // j
            LEN_t,    // t
            LEN_I,    // I   (microslop, pointer-sized)
            LEN_I32,  // I32 (microslop)
            LEN_I64,  // I64 (microslop)
        } len_mod = LEN_NONE;
        if(fmt[i] == 'h'){
            i++;
            if(i < slen && fmt[i] == 'h'){ len_mod = LEN_hh; i++; }
            else len_mod = LEN_h;
        }
        else if(fmt[i] == 'l'){
            i++;
            if(i < slen && fmt[i] == 'l'){ len_mod = LEN_ll; i++; }
            else len_mod = LEN_l;
        }
        else if(fmt[i] == 'L'){ len_mod = LEN_L; i++; }
        else if(fmt[i] == 'z'){ len_mod = LEN_z; i++; }
        else if(fmt[i] == 'j'){ len_mod = LEN_j; i++; }
        else if(fmt[i] == 't'){ len_mod = LEN_t; i++; }
        else if(fmt[i] == 'I'){
            i++;
            if(i + 1 < slen && fmt[i] == '6' && fmt[i+1] == '4'){ len_mod = LEN_I64; i += 2; }
            else if(i + 1 < slen && fmt[i] == '3' && fmt[i+1] == '2'){ len_mod = LEN_I32; i += 2; }
            else len_mod = LEN_I;
        }
        if(i >= slen)
            return cc_error(p, loc, "incomplete format specifier at end of string");
        char conv = fmt[i];
        int spec_len = (int)(fmt + i + 1 - spec_start);
        CcQualType expected = {0};
        if(len_mod != LEN_NONE){
            _Bool valid = 1;
            switch(len_mod){
                case LEN_NONE: break;
                case LEN_h: case LEN_hh:
                case LEN_ll: case LEN_j: case LEN_z: case LEN_t:
                    switch(conv){
                        case 'b': case 'B': case 'd': case 'i':
                        case 'o': case 'u': case 'x': case 'X':
                        case 'n': break;
                        default: valid = 0;
                    }
                    break;
                case LEN_l:
                    switch(conv){
                        case 'b': case 'B': case 'd': case 'i':
                        case 'o': case 'u': case 'x': case 'X':
                        case 'n': case 'c': case 's':
                        case 'a': case 'A': case 'e': case 'E':
                        case 'f': case 'F': case 'g': case 'G':
                            break;
                        default: valid = 0;
                    }
                    break;
                case LEN_L:
                    switch(conv){
                        case 'a': case 'A': case 'e': case 'E':
                        case 'f': case 'F': case 'g': case 'G':
                            break;
                        default: valid = 0;
                    }
                    break;
                case LEN_I: case LEN_I32: case LEN_I64:
                    switch(conv){
                        case 'b': case 'B': case 'd': case 'i':
                        case 'o': case 'u': case 'x': case 'X':
                            break;
                        default: valid = 0;
                    }
                    break;
            }
            if(!valid)
                return cc_error(p, loc, "invalid length modifier for '%%%c' format specifier", conv);
        }
        switch(conv){
            case 'd': case 'i':
                switch(len_mod){
                    DRP_CASES_EXHAUSTED;
                    case LEN_NONE: expected = ccqt_basic(CCBT_int); break;
                    case LEN_h:    expected = ccqt_basic(CCBT_int); break;
                    case LEN_hh:   expected = ccqt_basic(CCBT_int); break;
                    case LEN_l:    expected = ccqt_basic(CCBT_long); break;
                    case LEN_ll:   expected = ccqt_basic(CCBT_long_long); break;
                    case LEN_z:    expected = ccqt_basic(ccbt_to_signed(tc->size_type)); break;
                    case LEN_j:    expected = ccqt_basic(CCBT_long_long); break;
                    case LEN_t:    expected = ccqt_basic(tc->ptrdiff_type); break;
                    case LEN_I:    expected = ccqt_basic(ccbt_to_signed(tc->size_type)); break;
                    case LEN_I32:  expected = ccqt_basic(CCBT_int); break;
                    case LEN_I64:  expected = ccqt_basic(CCBT_long_long); break;
                    case LEN_L: break;
                }
                break;
            case 'u': case 'x': case 'X': case 'o':
            case 'b': case 'B':
                switch(len_mod){
                    DRP_CASES_EXHAUSTED;
                    case LEN_NONE: expected = ccqt_basic(CCBT_unsigned); break;
                    case LEN_h:    expected = ccqt_basic(CCBT_unsigned); break;
                    case LEN_hh:   expected = ccqt_basic(CCBT_unsigned); break;
                    case LEN_l:    expected = ccqt_basic(CCBT_unsigned_long); break;
                    case LEN_ll:   expected = ccqt_basic(CCBT_unsigned_long_long); break;
                    case LEN_z:    expected = ccqt_basic(tc->size_type); break;
                    case LEN_j:    expected = ccqt_basic(CCBT_unsigned_long_long); break;
                    case LEN_t:    expected = ccqt_basic(ccbt_to_unsigned(tc->ptrdiff_type)); break;
                    case LEN_I:    expected = ccqt_basic(tc->size_type); break;
                    case LEN_I32:  expected = ccqt_basic(CCBT_unsigned); break;
                    case LEN_I64:  expected = ccqt_basic(CCBT_unsigned_long_long); break;
                    case LEN_L: break;
                }
                break;
            case 'f': case 'F': case 'e': case 'E':
            case 'g': case 'G': case 'a': case 'A':
                if(len_mod == LEN_L)
                    expected = ccqt_basic(CCBT_long_double);
                else
                    expected = ccqt_basic(CCBT_double);
                break;
            case 'c':
                if(len_mod == LEN_l)
                    expected = ccqt_basic(tc->wint_type);
                else
                    expected = ccqt_basic(CCBT_int);
                break;
            case 's':
                if(len_mod == LEN_l){
                    CcQualType wchar_qt = ccqt_basic(tc->wchar_type);
                    err = cc_pointer_of(p, wchar_qt, &expected);
                    if(err) return err;
                }
                else
                    expected = p->char_star;
                break;
            case 'p':
                expected = p->void_star;
                break;
            case 'n':
                expected = (CcQualType){0};
                break;
            default:
                return cc_error(p, loc, "invalid format specifier '%%%c'", conv);
        }
        expected_args++;
        if(expected.bits && arg_idx < nargs){
            CcQualType actual = args[arg_idx]->type;
            uint32_t arg_num = arg_idx - first_vararg + 1;
            if(conv == 's'){
                CcBasicTypeKind expect_elem = ccqt_as_ptr(expected)->pointee.basic.kind;
                _Bool ok = ccqt_kind(actual) == CC_POINTER
                    && ccqt_is_basic(ccqt_as_ptr(actual)->pointee)
                    && ccqt_as_ptr(actual)->pointee.basic.kind == expect_elem;
                if(!ok)
                    return cc_printf_format_error(p, loc, spec_start, spec_len, arg_num, expected, actual);
            }
            else if(conv == 'p'){
                if(!ccqt_is_pointer_like(actual)
                    && !(ccqt_is_basic(actual) && actual.basic.kind == CCBT_nullptr_t))
                    return cc_printf_format_error(p, loc, spec_start, spec_len, arg_num, expected, actual);
            }
            else {
                _Bool ok = ccqt_is_basic(actual) && actual.basic.kind == expected.basic.kind;
                if(!ok && ccqt_is_basic(actual)){
                    CcBasicTypeKind ek = expected.basic.kind;
                    CcBasicTypeKind ak = actual.basic.kind;
                    ok = ccbt_is_integer(ek) && ccbt_is_integer(ak)
                        && tc->sizeof_[ek] == tc->sizeof_[ak];
                }
                if(!ok)
                    return cc_printf_format_error(p, loc, spec_start, spec_len, arg_num, expected, actual);
            }
        }
        arg_idx++;
    }
    uint32_t provided = nargs - first_vararg;
    if(expected_args > provided)
        return cc_error(p, loc, "format specifies %u argument%s, but only %u provided",
            expected_args, expected_args == 1 ? "" : "s", provided);
    if(expected_args < provided)
        return cc_error(p, loc, "format specifies %u argument%s, but %u provided",
            expected_args, expected_args == 1 ? "" : "s", provided);
    return 0;
}

static
int
cc_next_token(CcParser* p, CcToken* tok){
    if(p->pending.count){
        *tok = ma_pop(CcToken)(&p->pending);
        return 0;
    }
    return cpp_next_c_token(&p->cpp, tok);
}

static
int
cc_unget(CcParser* p, CcToken* tok){
    return ma_push(CcToken)(&p->pending, cc_allocator(p), *tok);
}

static
int
cc_peek(CcParser* p, CcToken* tok){
    int err = cc_next_token(p, tok);
    if(err) return err;
    return cc_unget(p, tok);
}

static int cc_expect_punct(CcParser* p, CcPunct punct){ return cc_expect_punct_supp(p, punct, "");}
static
int
cc_expect_punct_supp(CcParser* p, CcPunct punct, const char* supp){
    CcToken tok;
    int err = cc_next_token(p, &tok);
    if(err) return err;
    if(tok.type != CC_PUNCTUATOR || tok.punct.punct != punct){
        if(punct == ';' && tok.type == CC_EOF)
            return 0;
        char buf[4];
        int len = 0;
        uint32_t v = (uint32_t)punct;
        if(v > 0xFFFF){
            buf[len++] = (char)(v >> 16);
            buf[len++] = (char)(v >> 8);
            buf[len++] = (char)v;
        }
        else if(v > 0xFF){
            buf[len++] = (char)(v >> 8);
            buf[len++] = (char)v;
        }
        else {
            buf[len++] = (char)v;
        }
        buf[len] = 0;
        return cc_error(p, tok.loc, "Expected '%s'%s", buf, supp);
    }
    return 0;
}

static
CcExpr* _Nullable
_cc_alloc_expr(CcParser* p, size_t nvalues){
    size_t size = sizeof(CcExpr) + nvalues * sizeof(CcExpr*);
    #if CC_RECYCLE_EXPRS
    if(nvalues < sizeof p->exprs / sizeof p->exprs[0]){
        CcExpr* n = fl_pop(&p->exprs[nvalues]);
        if(n){
            memset(n, 0, size);
            return n;
        }
    }
    #endif
    return Allocator_zalloc(cc_allocator(p), size);
}
static
void
_cc_release_expr(CcParser* p, CcExpr* e, size_t nvalues){
    #if CC_RECYCLE_EXPRS
    if(nvalues < sizeof p->exprs / sizeof p->exprs[0]){
        fl_push(&p->exprs[nvalues], e);
        return;
    }
    #endif
    size_t size = sizeof(CcExpr) + nvalues * sizeof(CcExpr*);
    Allocator_free(cc_allocator(p), e, size);
}
static
size_t
cc_expr_nvalues(CcExpr* e){
    switch(e->kind){
        case CC_EXPR_BIT_BUILTIN:
            return e->bit_builtin.nargs;
        case CC_EXPR_OBJECT_VIEW:
        case CC_EXPR_NEG:
        case CC_EXPR_POS:
        case CC_EXPR_BITNOT:
        case CC_EXPR_LOGNOT:
        case CC_EXPR_DEREF:
        case CC_EXPR_ADDR:
        case CC_EXPR_PREINC:
        case CC_EXPR_PREDEC:
        case CC_EXPR_POSTINC:
        case CC_EXPR_POSTDEC:
        case CC_EXPR_CAST:
        case CC_EXPR_SIZEOF_VMT:
        case CC_EXPR_BSWAP:
        case CC_EXPR_ALLOCA:
        case CC_EXPR_INTERN:
        case CC_EXPR_SRCLOC_REFLECT:
        case CC_EXPR_STATEMENT_EXPRESSION:
            return 0;
        case CC_EXPR_COMPILE:
            return 1;
        case CC_EXPR_MODULE_REFLECT:
            switch(e->module.op){
                case CC_MODULE_NONE:
                case CC_MODULE_FUNC_COUNT:
                case CC_MODULE_FUNC:
                case CC_MODULE_FUNC_INFO:
                case CC_MODULE_VAR_COUNT:
                case CC_MODULE_VAR:
                case CC_MODULE_TYPE_COUNT:
                case CC_MODULE_TYPE:
                case CC_MODULE_RUN:
                     return 0;
                case CC_MODULE_PARSE_TYPE:
                case CC_MODULE_SYMBOL:
                    return 1;
                DRP_CASES_EXHAUSTED;
            }
        case CC_EXPR_HOTSWAP:
            return 1;
        case CC_EXPR_VALUE:
        case CC_EXPR_VARIABLE:
        case CC_EXPR_FUNCTION:
        case CC_EXPR_BUILTIN:
            return 0;
        case CC_EXPR_ADD:
        case CC_EXPR_SUB:
        case CC_EXPR_MUL:
        case CC_EXPR_DIV:
        case CC_EXPR_MOD:
        case CC_EXPR_BITAND:
        case CC_EXPR_BITOR:
        case CC_EXPR_BITXOR:
        case CC_EXPR_LSHIFT:
        case CC_EXPR_RSHIFT:
        case CC_EXPR_LOGAND:
        case CC_EXPR_LOGOR:
        case CC_EXPR_EQ:
        case CC_EXPR_NE:
        case CC_EXPR_LT:
        case CC_EXPR_GT:
        case CC_EXPR_LE:
        case CC_EXPR_GE:
        case CC_EXPR_ASSIGN:
        case CC_EXPR_ADDASSIGN:
        case CC_EXPR_SUBASSIGN:
        case CC_EXPR_MULASSIGN:
        case CC_EXPR_DIVASSIGN:
        case CC_EXPR_MODASSIGN:
        case CC_EXPR_BITANDASSIGN:
        case CC_EXPR_BITORASSIGN:
        case CC_EXPR_BITXORASSIGN:
        case CC_EXPR_LSHIFTASSIGN:
        case CC_EXPR_RSHIFTASSIGN:
        case CC_EXPR_COMMA:
        case CC_EXPR_SUBSCRIPT:
        case CC_EXPR_DOT:
        case CC_EXPR_ARROW:
            return 1;
        case CC_EXPR_TERNARY:
        case CC_EXPR_ADD_OVERFLOW:
        case CC_EXPR_MUL_OVERFLOW:
        case CC_EXPR_SUB_OVERFLOW:
        case CC_EXPR_UMUL128:
            return 2;
        case CC_EXPR_CALL:
            return e->call.nargs;
        case CC_EXPR_VA:
            return (e->va.op == CC_VA_COPY) ? 1 : 0;
        case CC_EXPR_ATOMIC:
            switch(e->atomic.op){
                case CC_ATOMIC_LOAD_N:
                case CC_ATOMIC_THREAD_FENCE:
                case CC_ATOMIC_SIGNAL_FENCE:
                case CC_ATOMIC_INTERLOCKED_INCREMENT:
                case CC_ATOMIC_INTERLOCKED_DECREMENT:
                    return 0;
                case CC_ATOMIC_STORE_N:
                case CC_ATOMIC_FETCH_ADD:
                case CC_ATOMIC_FETCH_SUB:
                case CC_ATOMIC_ADD_FETCH:
                case CC_ATOMIC_SUB_FETCH:
                case CC_ATOMIC_FETCH_AND:
                case CC_ATOMIC_FETCH_OR:
                case CC_ATOMIC_FETCH_XOR:
                case CC_ATOMIC_EXCHANGE_N:
                case CC_ATOMIC_LOAD:
                case CC_ATOMIC_STORE:
                    return 1;
                case CC_ATOMIC_EXCHANGE:
                case CC_ATOMIC_COMPARE_EXCHANGE_N:
                case CC_ATOMIC_COMPARE_EXCHANGE:
                case CC_ATOMIC_INTERLOCKED_COMPARE_EXCHANGE:
                    return 2;
                case CC_ATOMIC_INTERLOCKED_COMPARE_EXCHANGE128:
                    return 3;
            }
            return 0;
        case CC_EXPR_TYPE_INTROSPECTION:
            switch(e->type_introspection.op){
                case CC_TYPE_IS_CALLABLE_WITH:
                case CC_TYPE_IS_CALLABLE_THROUGH:
                case CC_TYPE_CASTABLE_TO:
                case CC_TYPE_MAKE_ANY:
                case CC_TYPE_FIELD:
                case CC_TYPE_METHOD:
                case CC_TYPE_HAS_FIELD:
                case CC_TYPE_HAS_METHOD:
                case CC_TYPE_PARAM_TYPE:
                case CC_TYPE_ENUMERATOR:
                    return 1;
                case CC_TYPE_ALIGNOF:
                case CC_TYPE_COUNT:
                case CC_TYPE_LOC:
                case CC_TYPE_ELEMENT_TYPE:
                case CC_TYPE_ENUMERATORS:
                case CC_TYPE_FIELDS:
                case CC_TYPE_METHODS:
                case CC_TYPE_IS_ARITHMETIC:
                case CC_TYPE_IS_ARRAY:
                case CC_TYPE_IS_VECTOR:
                case CC_TYPE_IS_SLICE:
                case CC_TYPE_IS_ATOMIC:
                case CC_TYPE_IS_CALLABLE:
                case CC_TYPE_IS_CONST:
                case CC_TYPE_IS_ENUM:
                case CC_TYPE_IS_FLOAT:
                case CC_TYPE_IS_FUNCTION:
                case CC_TYPE_IS_INCOMPLETE:
                case CC_TYPE_IS_VALID:
                case CC_TYPE_IS_INVALID:
                case CC_TYPE_IS_INTEGER:
                case CC_TYPE_IS_POINTER:
                case CC_TYPE_IS_SIGNED:
                case CC_TYPE_IS_STRUCT:
                case CC_TYPE_IS_UNION:
                case CC_TYPE_IS_UNSIGNED:
                case CC_TYPE_IS_VARIADIC:
                case CC_TYPE_IS_VOLATILE:
                case CC_TYPE_NAME:
                case CC_TYPE_NONE:
                case CC_TYPE_PARAM_COUNT:
                case CC_TYPE_POINTEE:
                case CC_TYPE_PUSH_METHOD:
                case CC_TYPE_RETURN_TYPE:
                case CC_TYPE_SIZEOF:
                case CC_TYPE_TAG:
                case CC_TYPE_UNDERLYING_TYPE:
                case CC_TYPE_UNQUAL:
                    return 0;
                DRP_CASES_EXHAUSTED;
            }
        case CC_EXPR_COMPOUND_LITERAL:
        case CC_EXPR_INIT_LIST:
            return 0;
        case CC_EXPR_SLICE:
            return 2;
        case CC_EXPR_SLICE_LO:
            return 1;
        case CC_EXPR_SLICE_HI:
            return 1;
        case CC_EXPR_SLICE_ALL:
            return 0;
    }
    return 0;
}

static
void
cc_release_expr(CcParser* p, CcExpr* e){
    size_t nvalues = cc_expr_nvalues(e);
    for(size_t i = 0; i < nvalues; i++)
        cc_release_expr(p, e->values[i]);
    switch(e->kind){
        case CC_EXPR_VALUE:
        case CC_EXPR_VARIABLE:
        case CC_EXPR_FUNCTION:
        case CC_EXPR_BUILTIN:
            break;
        case CC_EXPR_ARROW:
        case CC_EXPR_DOT:
            cc_field_path_free(cc_allocator(p), e->field_path);
            break;
        case CC_EXPR_COMPOUND_LITERAL:
        case CC_EXPR_INIT_LIST:
            if(!e->init_list->rc--){
                for(uint32_t i = 0; i < e->init_list->count; i++){
                    cc_field_path_free(cc_allocator(p), e->init_list->entries[i].path);
                    if(e->init_list->entries[i].value)
                        cc_release_expr(p, e->init_list->entries[i].value);
                }
                Allocator_free(cc_allocator(p), e->init_list, sizeof(CcInitList) + e->init_list->count * sizeof(CcInitEntry));
            }
            break;
        case CC_EXPR_STATEMENT_EXPRESSION:
            cc_free_stmt_tree(p, e->stmt_body);
            break;
        case CC_EXPR_OBJECT_VIEW:
        case CC_EXPR_ADD:
        case CC_EXPR_ADDASSIGN:
        case CC_EXPR_ADDR:
        case CC_EXPR_ADD_OVERFLOW:
        case CC_EXPR_ALLOCA:
        case CC_EXPR_ASSIGN:
        case CC_EXPR_ATOMIC:
        case CC_EXPR_BITAND:
        case CC_EXPR_BITANDASSIGN:
        case CC_EXPR_BITNOT:
        case CC_EXPR_BITOR:
        case CC_EXPR_BITORASSIGN:
        case CC_EXPR_BITXOR:
        case CC_EXPR_BITXORASSIGN:
        case CC_EXPR_CALL:
        case CC_EXPR_CAST:
        case CC_EXPR_COMMA:
        case CC_EXPR_BSWAP:
        case CC_EXPR_DEREF:
        case CC_EXPR_DIV:
        case CC_EXPR_DIVASSIGN:
        case CC_EXPR_EQ:
        case CC_EXPR_GE:
        case CC_EXPR_GT:
        case CC_EXPR_INTERN:
        case CC_EXPR_LE:
        case CC_EXPR_LOGAND:
        case CC_EXPR_LOGNOT:
        case CC_EXPR_LOGOR:
        case CC_EXPR_LSHIFT:
        case CC_EXPR_LSHIFTASSIGN:
        case CC_EXPR_LT:
        case CC_EXPR_MOD:
        case CC_EXPR_MODASSIGN:
        case CC_EXPR_MUL:
        case CC_EXPR_MULASSIGN:
        case CC_EXPR_MUL_OVERFLOW:
        case CC_EXPR_NE:
        case CC_EXPR_NEG:
        case CC_EXPR_BIT_BUILTIN:
        case CC_EXPR_POS:
        case CC_EXPR_POSTDEC:
        case CC_EXPR_POSTINC:
        case CC_EXPR_PREDEC:
        case CC_EXPR_PREINC:
        case CC_EXPR_RSHIFT:
        case CC_EXPR_RSHIFTASSIGN:
        case CC_EXPR_SIZEOF_VMT:
        case CC_EXPR_SUB:
        case CC_EXPR_SUBASSIGN:
        case CC_EXPR_SUBSCRIPT:
        case CC_EXPR_SUB_OVERFLOW:
        case CC_EXPR_HOTSWAP:
        case CC_EXPR_SRCLOC_REFLECT:
        case CC_EXPR_COMPILE:
        case CC_EXPR_MODULE_REFLECT:
        case CC_EXPR_TERNARY:
        case CC_EXPR_TYPE_INTROSPECTION:
        case CC_EXPR_UMUL128:
        case CC_EXPR_VA:
        case CC_EXPR_SLICE:
        case CC_EXPR_SLICE_LO:
        case CC_EXPR_SLICE_HI:
        case CC_EXPR_SLICE_ALL:
            if(e->lhs)
                cc_release_expr(p, e->lhs);
            break;
    }
    _cc_release_expr(p, e, nvalues);
}

static
CcExpr*_Nullable
cc_make_expr(CcParser* p, CcExprKind kind, SrcLoc loc, CcQualType type, size_t nvalues){
    CcExpr* node = _cc_alloc_expr(p, nvalues);
    if(node){
        node->kind = kind;
        node->loc = loc;
        node->type = type;
    }
    return node;
}

static
CcExpr* _Nullable
cc_value_expr(CcParser* p, SrcLoc loc, CcQualType type){
    CcExpr* node = _cc_alloc_expr(p, 0);
    if(!node) return NULL;
    node->kind = CC_EXPR_VALUE;
    node->loc = loc;
    node->type = type;
    return node;
}

static
CcExpr*_Nullable
cc_integer_bits_expr(CcParser* p, SrcLoc loc, CcQualType type, CiUint128 bits){
    CcExpr* node = cc_value_expr(p, loc, type);
    if(node){
        CcQualType t = type;
        while(ccqt_kind(t) == CC_ENUM) t = ccqt_as_enum(t)->underlying;
        if(ccqt_bt_eq(t, CCBT_int128) || ccqt_bt_eq(t, CCBT_unsigned_int128))
            node->uinteger128 = bits;
        else node->uinteger = ci_uint128_lo(bits);
    }
    return node;
}

static
CcExpr*_Nullable
cc_int64_expr(CcParser* p, SrcLoc loc, CcQualType type, int64_t v){
    CcExpr* node = cc_value_expr(p, loc, type);
    if(node){
        CcQualType t = type;
        while(ccqt_kind(t) == CC_ENUM) t = ccqt_as_enum(t)->underlying;
        if(ccqt_bt_eq(t, CCBT_int128) || ccqt_bt_eq(t, CCBT_unsigned_int128))
            node->uinteger128 = ci_uint128_from_int64(v);
        else node->integer = v;
    }
    return node;
}

static
CcExpr*_Nullable
cc_uint64_expr(CcParser* p, SrcLoc loc, CcQualType type, uint64_t v){
    CcExpr* node = cc_value_expr(p, loc, type);
    if(node){
        CcQualType t = type;
        while(ccqt_kind(t) == CC_ENUM) t = ccqt_as_enum(t)->underlying;
        if(ccqt_bt_eq(t, CCBT_int128) || ccqt_bt_eq(t, CCBT_unsigned_int128))
            node->uinteger128 = ci_uint128_from_uint64(v);
        else node->uinteger = v;
    }
    return node;
}

static
CcExpr*_Nullable
cc_constexpr_string_slice_expr(CcParser* p, SrcLoc loc, Atom name, _Bool include_nul){
    CcInitList* il = Allocator_zalloc(cc_allocator(p), sizeof *il + 2 * sizeof(CcInitEntry));
    if(!il) return NULL;
    CcExpr *count = NULL, *data = NULL, *literal = NULL, *node = NULL;
    if(0){
        oom:
        if(node) cc_release_expr(p, node);
        if(data) cc_release_expr(p, data);
        if(literal) cc_release_expr(p, literal);
        if(count) cc_release_expr(p, count);
        Allocator_free(cc_allocator(p), il, sizeof *il+2*sizeof(CcInitEntry));
        return NULL;
    }
    il->loc = loc;
    il->count = 2;
    count = cc_uint64_expr(p, loc, ccqt_basic(cc_target(p)->size_type), name->length+include_nul);
    if(!count) goto oom;
    CcArray* array = cc_intern_array(&p->type_cache, cc_allocator(p),
        ccqt_as_ptr(p->const_char_star)->pointee, name->length + 1, 0, 0, 0, 0);
    if(!array) goto oom;
    literal = cc_value_expr(p, loc, (CcQualType){.bits=(uintptr_t)array});
    if(!literal) goto oom;
    literal->text = name->data;
    literal->str.length = name->length + 1;
    data = cc_make_expr(p, CC_EXPR_CAST, loc, p->const_char_star, 0);
    if(!data) goto oom;
    data->lhs = literal;
    literal = NULL;
    node = cc_make_expr(p, CC_EXPR_INIT_LIST, loc, p->const_char_slice, 0);
    if(!node) goto oom;
    il->entries[0].path = (CcFieldPath){.n_components=1, .idx0=0};
    il->entries[0].value = count;
    il->entries[1].path = (CcFieldPath){.n_components=1, .idx0=1};
    il->entries[1].value = data;
    node->init_list = il;
    return node;
}

static
CcExpr* _Nullable
cc_unary_expr(CcParser* p, CcExprKind kind, SrcLoc loc, CcQualType type, CcExpr* operand){
    CcExpr* node = _cc_alloc_expr(p, 0);
    if(!node) return NULL;
    node->kind = kind;
    node->loc = loc;
    node->type = type;
    node->lhs = operand;
    return node;
}

static
CcExpr* _Nullable
cc_binary_expr(CcParser* p, CcExprKind kind, SrcLoc loc, CcQualType type, CcExpr* left, CcExpr* right){
    CcExpr* node = _cc_alloc_expr(p, 1);
    if(!node) return NULL;
    node->kind = kind;
    node->loc = loc;
    node->type = type;
    node->lhs = left;
    node->values[0] = right;
    return node;
}
static
int
cc_pointer_of(CcParser* p, CcQualType pointee, CcQualType* out){
    CcPointer* ptr = cc_intern_pointer(&p->type_cache, cc_allocator(p), pointee, 0, 0);
    if(!ptr) return CC_OOM_ERROR;
    *out = (CcQualType){.bits = (uintptr_t)ptr};
    return 0;
}

static
int
cc_slice_of(CcParser* p, CcQualType pointee, CcQualType* out){
    CcSlice* slice = cc_intern_slice(&p->type_cache, cc_allocator(p), pointee, 0);
    if(!slice) return CC_OOM_ERROR;
    *out = (CcQualType){.bits = (uintptr_t)slice};
    return 0;
}

static
int
cc_block_pointer_of(CcParser* p, CcQualType pointee, CcQualType* out){
    CcPointer* ptr = cc_intern_pointer(&p->type_cache, cc_allocator(p), pointee, 0, 1);
    if(!ptr) return CC_OOM_ERROR;
    *out = (CcQualType){.bits = (uintptr_t)ptr};
    return 0;
}

static
int
cc_va_list_to_ptr(CcParser* p, SrcLoc loc, CcExpr*_Nonnull*_Nonnull e){
    CcExpr* expr = *e;
    if(expr->type.bits == p->builtin_va_list_ptr.bits)
        return 0;
    if(expr->type.bits != p->builtin_va_list.bits)
        return cc_error(p, loc, "expression does not have type va_list");
    if(ccqt_kind(expr->type) == CC_ARRAY && !ccqt_as_array(expr->type)->is_vector){
        return cc_implicit_cast(p, expr, p->builtin_va_list_ptr, e);
    }
    if(!expr->is_lvalue)
        return cc_error(p, loc, "va_list argument must be an lvalue");
    CcExpr* addr = cc_make_expr(p, CC_EXPR_ADDR, loc, p->builtin_va_list_ptr, 0);
    if(!addr) return CC_OOM_ERROR;
    addr->lhs = expr;
    *e = addr;
    return 0;
}

static
Marray(CcToken)*_Nullable
cc_get_scratch(CcParser* p){
    Marray(CcToken)* toks = fl_pop(&p->scratch_tokens);
    if(!toks) toks = Allocator_zalloc(cc_allocator(p), sizeof *toks);
    if(!toks) return toks;
    toks->count = 0;
    return toks;
}

static
void
cc_release_scratch(CcParser* p, Marray(CcToken)* toks){
    fl_push(&p->scratch_tokens, toks);
}

static
Allocator
cc_allocator(CcParser*p){
    return p->cpp.allocator;
}
static
Allocator
cc_scratch_allocator(CcParser*p){
    return allocator_from_arena(&p->scratch_arena);
}

// cc_match_gnu_attribute
// ----------------------
// Tries to match a gnu-style attribute.
// On match, parses any arguments and updates *attrs.
// For unrecognized names, skips any argument list.
//
// Arguments:
// ----------
// p:
//     The parser.
//
// loc:
//     Source location for error reporting.
//
// attr_name:
//     Canonicalized attribute name (without any __).
//
// attrs:
//     Attribute flags to update.
//
// Returns:
// --------
// 0 on success. Error code on failure.
static
int
cc_match_gnu_attribute(CcParser* p, SrcLoc loc, StringView attr_name, CcAttributes* attrs){
    int err = 0;
    CcToken tok;
    if(sv_equals(attr_name, SV("packed"))){
        attrs->packed = 1;
    }
    else if(sv_equals(attr_name, SV("transparent_union"))){
        attrs->transparent_union = 1;
    }
    else if(sv_equals(attr_name, SV("aligned"))){
        err = cc_peek(p, &tok);
        if(err) return err;
        if(tok.type == CC_PUNCTUATOR && tok.punct.punct == CC_lparen){
            err = cc_next_token(p, &tok); // consume '('
            if(err) return err;
            CcExpr* expr = NULL;
            err = cc_parse_assignment_expr(p, CC_CONSTEXPR_VALUE, &expr, CCQT_NONE);
            if(err) return err;
            if(!expr)
                return cc_error(p, loc, "expected constant expression for aligned attribute");
            int64_t align_i;
            err = cc_eval_integer(&(CcEvalCtx){p}, expr, &align_i);
            cc_release_expr(p, expr);
            if(err && err != CC_NOT_CONSTANT_ERROR) return err;
            if(err)
                return cc_error(p, loc, "aligned attribute requires a constant integral expression");
            uint64_t align = (uint64_t)align_i;
            if(align == 0 || (align & (align - 1)) != 0)
                return cc_error(p, loc, "alignment must be a positive power of 2");
            if(align > UINT16_MAX)
                return cc_error(p, loc, "alignment too large");
            attrs->aligned = (uint16_t)align;
            attrs->has_aligned = 1;
            err = cc_expect_punct(p, CC_rparen);
            if(err) return err;
        }
        else {
            attrs->aligned = cc_target(p)->max_align;
            attrs->has_aligned = 1;
        }
    }
    else if(sv_equals(attr_name, SV("vector_size"))){
        err = cc_expect_punct(p, CC_lparen);
        if(err) return err;
        CcExpr* expr = NULL;
        err = cc_parse_assignment_expr(p, CC_CONSTEXPR_VALUE, &expr, CCQT_NONE);
        if(err) return err;
        if(!expr)
            return cc_error(p, loc, "expected constant expression for vector_size attribute");
        int64_t vs_i;
        err = cc_eval_integer(&(CcEvalCtx){p}, expr, &vs_i);
        cc_release_expr(p, expr);
        if(err && err != CC_NOT_CONSTANT_ERROR) return err;
        if(err)
            return cc_error(p, loc, "vector_size attribute requires a constant integral expression");
        uint64_t vs = (uint64_t)vs_i;
        if(vs == 0 || (vs & (vs - 1)) != 0)
            return cc_error(p, loc, "vector_size must be a power of 2");
        if(vs > UINT16_MAX)
            return cc_error(p, loc, "vector_size too large");
        attrs->vector_size = (uint16_t)vs;
        err = cc_expect_punct(p, CC_rparen);
        if(err) return err;
    }
    else if(sv_equals(attr_name, SV("format"))){
        err = cc_expect_punct(p, CC_lparen);
        if(err) return err;
        CcToken arch_tok;
        err = cc_next_token(p, &arch_tok);
        if(err) return err;
        if(arch_tok.type != CC_IDENTIFIER)
            return cc_error(p, arch_tok.loc, "expected format archetype (e.g. 'printf')");
        StringView arch = {.text = arch_tok.ident.ident->data, .length = arch_tok.ident.ident->length};
        if(sv_startswith(arch, SV("__")))
            arch = sv_slice(arch, 2, arch.length);
        if(sv_endswith(arch, SV("__")))
            arch = sv_slice(arch, 0, arch.length-2);
        if(sv_equals(arch, SV("printf")))
            attrs->printf_like = 1;
        err = cc_expect_punct(p, CC_comma);
        if(err) return err;
        CcExpr* expr = NULL;
        err = cc_parse_assignment_expr(p, CC_CONSTEXPR_VALUE, &expr, CCQT_NONE);
        if(err) return err;
        if(expr) cc_release_expr(p, expr);
        err = cc_expect_punct(p, CC_comma);
        if(err) return err;
        expr = NULL;
        err = cc_parse_assignment_expr(p, CC_CONSTEXPR_VALUE, &expr, CCQT_NONE);
        if(err) return err;
        if(expr) cc_release_expr(p, expr);
        err = cc_expect_punct(p, CC_rparen);
        if(err) return err;
    }
    else {
        err = cc_peek(p, &tok);
        if(err) return err;
        if(tok.type == CC_PUNCTUATOR && tok.punct.punct == CC_lparen){
            err = cc_next_token(p, &tok);
            if(err) return err;
            int depth = 1;
            while(depth > 0){
                err = cc_next_token(p, &tok);
                if(err) return err;
                if(tok.type == CC_EOF)
                    return cc_error(p, loc, "unterminated attribute argument list");
                if(tok.type == CC_PUNCTUATOR){
                    if(tok.punct.punct == CC_lparen) depth++;
                    else if(tok.punct.punct == CC_rparen) depth--;
                }
            }
        }
    }
    return 0;
}

static
int
cc_parse_attributes(CcParser* p, CcAttributes* attrs){
    int err = 0;
    CcToken tok;
    for(;;){
        err = cc_parse_c23_attributes(p, attrs);
        if(err) return err;
        err = cc_peek(p, &tok);
        if(err) return err;
        if(tok.type != CC_KEYWORD || tok.kw.kw != CC___attribute__)
            return 0;
        err = cc_next_token(p, &tok);
        if(err) return err;
        err = cc_expect_punct(p, CC_lparen);
        if(err) return err;
        err = cc_expect_punct(p, CC_lparen);
        if(err) return err;
        err = cc_peek(p, &tok);
        if(err) return err;
        if(tok.type == CC_PUNCTUATOR && tok.punct.punct == CC_rparen)
            goto close_parens;
        for(;;){
            err = cc_next_token(p, &tok);
            if(err) return err;
            if(tok.type != CC_IDENTIFIER && tok.type != CC_KEYWORD)
                return cc_error(p, tok.loc, "expected attribute name");
            StringView attr_name;
            if(tok.type == CC_IDENTIFIER)
                attr_name = (StringView){.text = tok.ident.ident->data, .length = tok.ident.ident->length};
            else
                attr_name = SV("");
            if(sv_startswith(attr_name, SV("__")))
                attr_name = sv_slice(attr_name, 2, attr_name.length);
            if(sv_endswith(attr_name, SV("__")))
                attr_name = sv_slice(attr_name, 0, attr_name.length-2);
            err = cc_match_gnu_attribute(p, tok.loc, attr_name, attrs);
            if(err) return err;
            err = cc_peek(p, &tok);
            if(err) return err;
            if(tok.type == CC_PUNCTUATOR && tok.punct.punct == CC_comma){
                err = cc_next_token(p, &tok);
                if(err) return err;
                continue;
            }
            break;
        }
        close_parens:
        err = cc_expect_punct(p, CC_rparen);
        if(err) return err;
        err = cc_expect_punct(p, CC_rparen);
        if(err) return err;
    }
}

static
_Bool
cc_is_c23_attribute_start(CcParser* p){
    CcToken t1, t2;
    int err = cc_peek(p, &t1);
    if(err) return 0;
    if(t1.type != CC_PUNCTUATOR || t1.punct.punct != CC_lbracket)
        return 0;
    err = cc_next_token(p, &t1);
    if(err) return 0;
    err = cc_peek(p, &t2);
    cc_unget(p, &t1);
    if(err) return 0;
    return t2.type == CC_PUNCTUATOR && t2.punct.punct == CC_lbracket;
}

static
int
cc_parse_c23_attributes(CcParser* p, CcAttributes* attrs){
    int err = 0;
    CcToken tok;
    for(;;){
        if(!cc_is_c23_attribute_start(p))
            return 0;
        err = cc_next_token(p, &tok);
        if(err) return err;
        SrcLoc attr_loc = tok.loc;
        err = cc_next_token(p, &tok);
        if(err) return err;
        err = cc_peek(p, &tok);
        if(err) return err;
        if(tok.type == CC_PUNCTUATOR && tok.punct.punct == CC_rbracket)
            goto close_brackets;
        for(;;){
            err = cc_peek(p, &tok);
            if(err) return err;
            if(tok.type == CC_PUNCTUATOR && tok.punct.punct == CC_rbracket)
                break;
            err = cc_next_token(p, &tok);
            if(err) return err;
            if(tok.type != CC_IDENTIFIER && tok.type != CC_KEYWORD)
                return cc_error(p, tok.loc, "expected attribute name");
            if(tok.type == CC_KEYWORD){
                if(tok.kw.kw == CC__Noreturn)
                    attrs->is_noreturn = 1;
                goto c23_skip_args;
            }
            StringView attr_name = {.text = tok.ident.ident->data, .length = tok.ident.ident->length};
            _Bool is_gnu = 0;
            err = cc_peek(p, &tok);
            if(err) return err;
            if(tok.type == CC_PUNCTUATOR && tok.punct.punct == CC_double_colon){
                err = cc_next_token(p, &tok);
                if(err) return err;
                StringView prefix = attr_name;
                if(sv_startswith(prefix, SV("__")))
                    prefix = sv_slice(prefix, 2, prefix.length);
                if(sv_endswith(prefix, SV("__")))
                    prefix = sv_slice(prefix, 0, prefix.length-2);
                is_gnu = sv_equals(prefix, SV("gnu"));
                err = cc_next_token(p, &tok);
                if(err) return err;
                if(tok.type != CC_IDENTIFIER && tok.type != CC_KEYWORD)
                    return cc_error(p, tok.loc, "expected attribute name after '::'");
                if(tok.type == CC_IDENTIFIER)
                    attr_name = (StringView){.text = tok.ident.ident->data, .length = tok.ident.ident->length};
                else {
                    goto c23_skip_args;
                }
                if(!is_gnu)
                    goto c23_skip_args;
            }
            if(sv_startswith(attr_name, SV("__")))
                attr_name = sv_slice(attr_name, 2, attr_name.length);
            if(sv_endswith(attr_name, SV("__")))
                attr_name = sv_slice(attr_name, 0, attr_name.length-2);
            if(sv_equals(attr_name, SV("noreturn")))
                attrs->is_noreturn = 1;
            else if(is_gnu){
                err = cc_match_gnu_attribute(p, tok.loc, attr_name, attrs);
                if(err) return err;
                goto c23_skip_comma;
            }
            c23_skip_args:
            err = cc_peek(p, &tok);
            if(err) return err;
            if(tok.type == CC_PUNCTUATOR && tok.punct.punct == CC_lparen){
                err = cc_next_token(p, &tok);
                if(err) return err;
                int depth = 1;
                while(depth > 0){
                    err = cc_next_token(p, &tok);
                    if(err) return err;
                    if(tok.type == CC_EOF)
                        return cc_error(p, attr_loc, "unterminated attribute argument list");
                    if(tok.type == CC_PUNCTUATOR){
                        if(tok.punct.punct == CC_lparen) depth++;
                        else if(tok.punct.punct == CC_rparen) depth--;
                    }
                }
            }
            c23_skip_comma:
            err = cc_peek(p, &tok);
            if(err) return err;
            if(tok.type == CC_PUNCTUATOR && tok.punct.punct == ','){
                err = cc_next_token(p, &tok);
                if(err) return err;
                continue;
            }
            break;
        }
        close_brackets:
        err = cc_expect_punct(p, CC_rbracket);
        if(err) return err;
        err = cc_expect_punct(p, CC_rbracket);
        if(err) return err;
    }
}

static
int
cc_parse_declspec(CcParser* p, CcAttributes* attrs){
    int err = 0;
    CcToken tok;
    for(;;){
        err = cc_peek(p, &tok);
        if(err) return err;
        if(tok.type != CC_KEYWORD || tok.kw.kw != CC___declspec)
            return 0;
        err = cc_next_token(p, &tok);
        if(err) return err;
        SrcLoc declspec_loc = tok.loc;
        err = cc_expect_punct(p, CC_lparen);
        if(err) return err;
        for(;;){
            err = cc_peek(p, &tok);
            if(err) return err;
            if(tok.type == CC_PUNCTUATOR && tok.punct.punct == CC_rparen)
                break;
            err = cc_next_token(p, &tok);
            if(err) return err;
            if(tok.type == CC_KEYWORD){
                if(tok.kw.kw == CC__Noreturn)
                    attrs->is_noreturn = 1;
                else {
                    err = cc_peek(p, &tok);
                    if(err) return err;
                    if(tok.type == CC_PUNCTUATOR && tok.punct.punct == CC_lparen){
                        err = cc_next_token(p, &tok);
                        if(err) return err;
                        int depth = 1;
                        while(depth > 0){
                            err = cc_next_token(p, &tok);
                            if(err) return err;
                            if(tok.type == CC_EOF)
                                return cc_error(p, declspec_loc, "unterminated __declspec argument list");
                            if(tok.type == CC_PUNCTUATOR){
                                if(tok.punct.punct == CC_lparen) depth++;
                                else if(tok.punct.punct == CC_rparen) depth--;
                            }
                        }
                    }
                }
                continue;
            }
            if(tok.type != CC_IDENTIFIER)
                return cc_error(p, tok.loc, "expected __declspec specifier name");
            StringView name = {.text = tok.ident.ident->data, .length = tok.ident.ident->length};
            if(sv_equals(name, SV("align"))){
                err = cc_expect_punct(p, CC_lparen);
                if(err) return err;
                CcExpr* expr = NULL;
                err = cc_parse_assignment_expr(p, CC_CONSTEXPR_VALUE, &expr, CCQT_NONE);
                if(err) return err;
                if(!expr)
                    return cc_error(p, tok.loc, "expected constant expression for __declspec(align)");
                int64_t align_i;
                err = cc_eval_integer(&(CcEvalCtx){p}, expr, &align_i);
                cc_release_expr(p, expr);
                if(err && err != CC_NOT_CONSTANT_ERROR) return err;
                if(err)
                    return cc_error(p, tok.loc, "__declspec(align) requires a constant integral expression");
                uint64_t align = (uint64_t)align_i;
                if(align == 0 || (align & (align - 1)) != 0)
                    return cc_error(p, tok.loc, "alignment must be a positive power of 2");
                if(align > UINT16_MAX)
                    return cc_error(p, tok.loc, "alignment too large");
                attrs->aligned = (uint16_t)align;
                attrs->has_aligned = 1;
                err = cc_expect_punct(p, CC_rparen);
                if(err) return err;
            }
            else if(sv_equals(name, SV("thread"))){
                attrs->is_thread_local = 1;
            }
            else {
                err = cc_peek(p, &tok);
                if(err) return err;
                if(tok.type == CC_PUNCTUATOR && tok.punct.punct == CC_lparen){
                    err = cc_next_token(p, &tok);
                    if(err) return err;
                    int depth = 1;
                    while(depth > 0){
                        err = cc_next_token(p, &tok);
                        if(err) return err;
                        if(tok.type == CC_EOF)
                            return cc_error(p, declspec_loc, "unterminated __declspec argument list");
                        if(tok.type == CC_PUNCTUATOR){
                            if(tok.punct.punct == CC_lparen) depth++;
                            else if(tok.punct.punct == CC_rparen) depth--;
                        }
                    }
                }
            }
        }
        err = cc_expect_punct(p, CC_rparen);
        if(err) return err;
    }
}

static inline int
cc_align_to(CcParser* p, SrcLoc loc, uint32_t offset, uint32_t alignment, uint32_t* out){
    uint32_t padding = (0u - offset) & (alignment - 1);
    if(add_overflow(offset, padding, out))
        return cc_error(p, loc, "object size exceeds 32-bit layout limit");
    return 0;
}

static _Bool cc_sysv_classify_type(const CcTargetConfig* tc, CcQualType type, uint32_t off, CcSysVEightByte cls[_Nonnull 2]);
static CcBasicTypeKind cc_arm64_hfa_check(const CcTargetConfig* tc, CcQualType type, CcBasicTypeKind base, uint32_t* count);

static
_Bool
cc_sysv_classify_fields(const CcTargetConfig* tc, const CcField* _Null_unspecified fields, uint32_t count, uint32_t base, CcSysVEightByte cls[_Nonnull 2]){
    for(uint32_t i = 0; i < count; i++){
        const CcField* f = &fields[i];
        if(f->is_method) continue;
        if(f->is_bitfield){
            if(f->bitoffset) continue;
            uint32_t offset = base + f->offset;
            CcQualType bt = f->type;
            if(ccqt_kind(bt) == CC_ENUM) bt = ccqt_as_enum(bt)->underlying;
            CcBasicTypeKind bk = bt.basic.kind;
            if(offset % tc->alignof_[bk]) return 1;
            uint32_t eb = offset / 8;
            if(eb < 2) cls[eb] = CC_SYSV_INTEGER;
            continue;
        }
        if(cc_sysv_classify_type(tc, f->type, base + f->offset, cls))
            return 1;
    }
    return 0;
}

static
_Bool
cc_sysv_classify_type(const CcTargetConfig* tc, CcQualType type, uint32_t off, CcSysVEightByte cls[_Nonnull 2]){
    switch(ccqt_kind(type)){
        case CC_BASIC:{
            CcBasicTypeKind bk = type.basic.kind;
            uint32_t al = tc->alignof_[bk];
            if(off % al) return 1;
            if(bk == CCBT_long_double || bk == CCBT_float128 || bk == CCBT_long_double_complex)
                return 1;
            CcSysVEightByte c;
            if(ccbt_is_float(bk) || bk == CCBT_float_complex || bk == CCBT_double_complex)
                c = CC_SYSV_SSE;
            else
                c = CC_SYSV_INTEGER;
            uint32_t sz = tc->sizeof_[bk];
            uint32_t eb_lo = off / 8;
            uint32_t eb_hi = (off + sz - 1) / 8;
            for(uint32_t eb = eb_lo; eb <= eb_hi && eb < 2; eb++)
                if(c == CC_SYSV_INTEGER) cls[eb] = CC_SYSV_INTEGER;
            return 0;
        }
        case CC_BLOCK_POINTER:
        case CC_POINTER:
        case CC_FUNCTION:
            if(off % tc->alignof_[CCBT_nullptr_t]) return 1;
            if(off / 8 < 2) cls[off / 8] = CC_SYSV_INTEGER;
            return 0;
        case CC_SLICE: // XXX: idk if this is right.
            if(off % tc->alignof_[CCBT_nullptr_t]) return 1;
            if(off / 8 < 2) cls[off / 8] = CC_SYSV_INTEGER;
            off += 8;
            if(off % tc->alignof_[CCBT_nullptr_t]) return 1;
            if(off / 8 < 2) cls[off / 8] = CC_SYSV_INTEGER;
            return 0;
        case CC_ENUM:
            return cc_sysv_classify_type(tc, ccqt_as_enum(type)->underlying, off, cls);
        case CC_STRUCT:
            return cc_sysv_classify_fields(tc, ccqt_as_struct(type)->fields, ccqt_as_struct(type)->field_count, off, cls);
        case CC_UNION:
            return cc_sysv_classify_fields(tc, ccqt_as_union(type)->fields, ccqt_as_union(type)->field_count, off, cls);
        case CC_ARRAY:{
            CcArray* arr = ccqt_as_array(type);
            if(arr->is_incomplete) return 0;
            if(arr->is_vector) return 1; // TODO: vector classification
            uint32_t elem_sz;
            if(cc_type_sizeof_complete(tc, arr->element, &elem_sz)) return 1;
            for(uint32_t i = 0; i < (uint32_t)arr->length; i++){
                if(cc_sysv_classify_type(tc, arr->element, off + i * elem_sz, cls))
                    return 1;
            }
            return 0;
        }
    }
    return 0;
}

static
CcBasicTypeKind
cc_arm64_hfa_check(const CcTargetConfig* tc, CcQualType type, CcBasicTypeKind base, uint32_t* count){
    switch(ccqt_kind(type)){
        case CC_BASIC:{
            CcBasicTypeKind bk = type.basic.kind;
            CcBasicTypeKind effective = bk;
            uint32_t n = 1;
            if(bk == CCBT_float_complex){ effective = CCBT_float; n = 2; }
            else if(bk == CCBT_double_complex){ effective = CCBT_double; n = 2; }
            else if(bk == CCBT_long_double_complex){ effective = CCBT_long_double; n = 2; }
            else if(!ccbt_is_float(bk)) return CCBT_INVALID;
            if(base == CCBT_INVALID) base = effective;
            else if(base != effective) return CCBT_INVALID;
            *count += n;
            return base;
        }
        case CC_BLOCK_POINTER:
        case CC_POINTER:
        case CC_FUNCTION:
        case CC_ENUM:
        case CC_SLICE:
            return CCBT_INVALID;
        case CC_STRUCT:{
            CcStruct* s = ccqt_as_struct(type);
            for(uint32_t i = 0; i < s->field_count; i++){
                if(s->fields[i].is_method) continue;
                if(s->fields[i].is_bitfield) return CCBT_INVALID;
                base = cc_arm64_hfa_check(tc, s->fields[i].type, base, count);
                if(base == CCBT_INVALID) return CCBT_INVALID;
            }
            return base;
        }
        case CC_UNION:{
            CcUnion* u = ccqt_as_union(type);
            for(uint32_t i = 0; i < u->field_count; i++){
                if(u->fields[i].is_method) continue;
                if(u->fields[i].is_bitfield) return CCBT_INVALID;
                uint32_t dummy = 0;
                base = cc_arm64_hfa_check(tc, u->fields[i].type, base, &dummy);
                if(base == CCBT_INVALID) return CCBT_INVALID;
            }
            *count += u->size / tc->sizeof_[base];
            return base;
        }
        case CC_ARRAY:{
            CcArray* arr = ccqt_as_array(type);
            if(arr->is_incomplete) return base;
            uint32_t elem_count = 0;
            base = cc_arm64_hfa_check(tc, arr->element, base, &elem_count);
            if(base == CCBT_INVALID) return CCBT_INVALID;
            *count += elem_count * (uint32_t)arr->length;
            return base;
        }
    }
    return CCBT_INVALID;
}

static
int
cc_compute_struct_layout(CcParser* p, CcStruct* s, uint16_t pack_value){
    int err = 0;
    uint32_t offset = 0;
    uint32_t max_align = 1;
    uint32_t bitfield_offset = 0;        // bit offset within current storage unit
    uint32_t bitfield_storage_end = 0;   // byte offset of end of current storage unit
    uint32_t bitfield_storage_start = 0; // byte offset of start of current storage unit
    CcQualType bitfield_type = {0};      // type of current bitfield run (for microslop ABI)
    CcBitfieldABI bf_abi = cc_target(p)->bitfield_abi;
    for(uint32_t i = 0; i < s->field_count; i++){
        CcField* f = &s->fields[i];
        if(f->is_method) continue;
        if(ccqt_kind(f->type) == CC_ARRAY){
            CcArray* arr = ccqt_as_array(f->type);
            if(arr->is_incomplete){
                if(i + 1 < s->field_count){
                    _Bool has_later = 0;
                    for(uint32_t j = i + 1; j < s->field_count; j++){
                        if(!s->fields[j].is_method){ has_later = 1; break; }
                    }
                    if(has_later)
                        return cc_error(p, f->loc, "flexible array member must be last field");
                }
                if(bitfield_offset > 0){
                    offset = bitfield_storage_end;
                    bitfield_offset = 0;
                }
                uint32_t field_align;
                err = cc_alignof_as_uint(p, f->type, f->loc, &field_align);
                if(err) return err;
                if(s->packed) field_align = 1;
                else if(pack_value > 0 && field_align > pack_value) field_align = pack_value;
                err = cc_align_to(p, f->loc, offset, field_align, &offset);
                if(err) return err;
                f->offset = offset;
                if(field_align > max_align) max_align = field_align;
                s->has_fam = 1;
                continue;
            }
        }
        if(ccqt_kind(f->type) == CC_STRUCT){
            CcStruct* inner = ccqt_as_struct(f->type);
            if(inner->has_fam){
                _Bool is_last = 1;
                for(uint32_t j = i + 1; j < s->field_count; j++)
                    if(!s->fields[j].is_method){ is_last = 0; break; }
                if(!f->name && is_last)
                    s->has_fam = 1;
                else
                    return cc_error(p, f->loc, "struct with flexible array member cannot be embedded");
            }
        }
        uint32_t field_size, field_align;
        err = cc_sizeof_as_uint(p, f->type, f->loc, &field_size);
        if(err) return err;
        err = cc_alignof_as_uint(p, f->type, f->loc, &field_align);
        if(err) return err;
        if(s->packed)
            field_align = 1;
        else if(pack_value > 0 && field_align > pack_value)
            field_align = pack_value;
        // An explicit _Alignas/aligned on the field overrides packing.
        if(f->alignment > field_align)
            field_align = f->alignment;
        enum {char_bit=8};
        if(f->is_bitfield){
            uint32_t bw = f->bitwidth;
            uint32_t storage_bits = field_size * char_bit;
            if(bw == 0){
                if(bitfield_offset > 0){
                    offset = bitfield_storage_end;
                    bitfield_offset = 0;
                    bitfield_storage_end = 0;
                    bitfield_type = (CcQualType){0};
                }
                f->offset = offset;
                f->bitoffset = 0;
                continue;
            }
            _Bool fits;
            uint32_t f_offset = 0, f_bitoffset = 0;
            CcQualType ft = f->type;
            while(ccqt_kind(ft) == CC_ENUM) ft = ccqt_as_enum(ft)->underlying;
            if(bf_abi == CC_BITFIELD_MSVC){
                fits = bitfield_type.bits == ft.bits && bitfield_offset + bw <= storage_bits;
                f_offset = bitfield_storage_start;
                f_bitoffset = bitfield_offset;
            }
            else if(!bitfield_storage_end){
                fits = 0;
            }
            else {
                uint64_t abs_bit = (uint64_t)bitfield_storage_start * char_bit + bitfield_offset;
                uint32_t su_start = (uint32_t)(((abs_bit / char_bit) / field_align) * field_align);
                uint32_t bit_in_su = (uint32_t)(abs_bit - (uint64_t)su_start * char_bit);
                fits = bit_in_su + bw <= field_size * char_bit;
                f_offset = su_start;
                f_bitoffset = bit_in_su;
            }
            if(fits){
                f->offset = f_offset;
                f->bitoffset = f_bitoffset;
                bitfield_offset += bw;
                uint32_t end;
                if(add_overflow(f_offset, field_size, &end))
                    return cc_error(p, f->loc, "object size exceeds 32-bit layout limit");
                if(end > bitfield_storage_end) bitfield_storage_end = end;
            }
            else {
                if(bitfield_offset > 0)
                    offset = bitfield_storage_end;
                err = cc_align_to(p, f->loc, offset, field_align, &offset);
                if(err) return err;
                f->offset = offset;
                f->bitoffset = 0;
                bitfield_offset = bw;
                bitfield_storage_start = offset;
                if(add_overflow(offset, field_size, &bitfield_storage_end))
                    return cc_error(p, f->loc, "object size exceeds 32-bit layout limit");

                bitfield_type = ft;
            }
            if(field_align > max_align)
                max_align = field_align;
            continue;
        }
        if(bitfield_offset > 0){
            offset = bitfield_storage_end;
            bitfield_offset = 0;
            bitfield_storage_end = 0;

            bitfield_type = (CcQualType){0};
        }
        err = cc_align_to(p, f->loc, offset, field_align, &offset);
        if(err) return err;
        f->offset = offset;
        if(add_overflow(offset, field_size, &offset))
            return cc_error(p, f->loc, "object size exceeds 32-bit layout limit");
        if(field_align > max_align)
            max_align = field_align;
    }
    if(bitfield_offset > 0)
        offset = bitfield_storage_end;
    if(s->alignment > max_align)
        max_align = s->alignment;
    s->alignment = max_align;
    err = cc_align_to(p, s->loc, offset, max_align, &s->size);
    if(err) return err;
    const CcTargetConfig* tc = cc_target(p);
    switch(tc->target){
        case CC_TARGET_X86_64_LINUX:
        case CC_TARGET_X86_64_MACOS:{
            if(s->size > 16){
                s->sysv.is_memory_class = 1;
                break;
            }
            CcSysVEightByte cls[2] = {CC_SYSV_SSE, CC_SYSV_SSE};
            if(cc_sysv_classify_fields(tc, s->fields, s->field_count, 0, cls)){
                s->sysv.is_memory_class = 1;
                break;
            }
            s->sysv.class0 = cls[0];
            s->sysv.class1 = cls[1];
            break;
        }
        case CC_TARGET_AARCH64_LINUX:
        case CC_TARGET_AARCH64_MACOS:{
            if(s->size > 64 || s->alignment > 16) break;
            CcBasicTypeKind hfa_base = CCBT_INVALID;
            uint32_t hfa_count = 0;
            for(uint32_t i = 0; i < s->field_count; i++){
                if(s->fields[i].is_method) continue;
                if(s->fields[i].is_bitfield){ hfa_base = CCBT_INVALID; break; }
                hfa_base = cc_arm64_hfa_check(tc, s->fields[i].type, hfa_base, &hfa_count);
                if(hfa_base == CCBT_INVALID) break;
            }
            if(hfa_base != CCBT_INVALID && hfa_count >= 1 && hfa_count <= 4){
                s->arm64.hfa_type = (uint32_t)hfa_base;
                s->arm64.hfa_count = hfa_count;
            }
            break;
        }
        case CC_TARGET_X86_64_WINDOWS:
        case CC_TARGET_TEST:
        case CC_TARGET_COUNT:
            break;
    }
    return 0;
}

static
int
cc_compute_union_layout(CcParser* p, CcUnion* u, uint16_t pack_value){
    uint32_t max_size = 0;
    uint32_t max_align = 1;
    int err = 0;
    for(uint32_t i = 0; i < u->field_count; i++){
        CcField* f = &u->fields[i];
        if(f->is_method) continue;
        if(ccqt_kind(f->type) == CC_ARRAY && ccqt_as_array(f->type)->is_incomplete){
            uint32_t field_align;
            err = cc_alignof_as_uint(p, f->type, f->loc, &field_align);
            if(err) return err;
            if(pack_value > 0 && field_align > pack_value)
                field_align = pack_value;
            f->offset = 0;
            if(field_align > max_align) max_align = field_align;
            continue;
        }
        uint32_t field_size;
        err = cc_sizeof_as_uint(p, f->type, f->loc, &field_size);
        if(err) return err;
        uint32_t field_align;
        err = cc_alignof_as_uint(p, f->type, f->loc, &field_align);
        if(err) return err;
        if(pack_value > 0 && field_align > pack_value)
            field_align = pack_value;
        // An explicit _Alignas/aligned on the field overrides packing.
        if(f->alignment > field_align)
            field_align = f->alignment;
        f->offset = 0;
        if(f->is_bitfield){
            f->bitoffset = 0;
            if(field_size > max_size) max_size = field_size;
        }
        else {
            if(field_size > max_size) max_size = field_size;
        }
        if(field_align > max_align) max_align = field_align;
    }
    if(u->alignment > max_align)
        max_align = u->alignment;
    u->alignment = max_align;
    err = cc_align_to(p, u->loc, max_size, max_align, &u->size);
    if(err) return err;
    const CcTargetConfig* tc = cc_target(p);
    switch(tc->target){
        case CC_TARGET_X86_64_LINUX:
        case CC_TARGET_X86_64_MACOS:{
            if(u->size > 16){
                u->sysv.is_memory_class = 1;
                break;
            }
            CcSysVEightByte cls[2] = {CC_SYSV_SSE, CC_SYSV_SSE};
            if(cc_sysv_classify_fields(tc, u->fields, u->field_count, 0, cls)){
                u->sysv.is_memory_class = 1;
                break;
            }
            u->sysv.class0 = cls[0];
            u->sysv.class1 = cls[1];
            break;
        }
        case CC_TARGET_AARCH64_LINUX:
        case CC_TARGET_AARCH64_MACOS:{
            if(u->size > 64 || u->alignment > 16) break;
            CcBasicTypeKind hfa_base = CCBT_INVALID;
            for(uint32_t i = 0; i < u->field_count; i++){
                if(u->fields[i].is_method) continue;
                if(u->fields[i].is_bitfield){ hfa_base = CCBT_INVALID; break; }
                uint32_t dummy = 0;
                hfa_base = cc_arm64_hfa_check(tc, u->fields[i].type, hfa_base, &dummy);
                if(hfa_base == CCBT_INVALID) break;
            }
            if(hfa_base != CCBT_INVALID){
                uint32_t hfa_count = u->size / tc->sizeof_[hfa_base];
                if(hfa_count >= 1 && hfa_count <= 4){
                    u->arm64.hfa_type = (uint32_t)hfa_base;
                    u->arm64.hfa_count = hfa_count;
                }
            }
            break;
        }
        case CC_TARGET_X86_64_WINDOWS:
        case CC_TARGET_TEST:
        case CC_TARGET_COUNT:
            break;
    }
    return 0;
}

static
int
cc_pragma_pack(void* _Null_unspecified ctx, CppPreprocessor* cpp, SrcLoc loc, const CppToken*_Null_unspecified toks, size_t ntoks){
    CcParser* p = (CcParser*)ctx;
    if(!ntoks || toks[0].type != CPP_PUNCTUATOR || toks[0].punct != '(')
        return ((void)cc_warn(p, loc, "#pragma pack expects '('"), 0);
    if(ntoks < 2)
        return ((void)cc_warn(p, loc, "#pragma pack expects at least ()"), 0);
    const CppToken* end = toks+ntoks;
    for(const CppToken* t = end; --t != toks;){
        if(t->type == CPP_PUNCTUATOR && t->punct == ')'){
            end = t+1;
            break;
        }
    }
    int err = 0;
    CppTokens* expanded = cpp_get_scratch(cpp);
    if(end - toks > 2){
        err = cpp_expand_argument(cpp, toks+1, end-toks-2, expanded);
        if(err) goto finally;
        toks = expanded->data;
        end = toks + expanded->count;
    }
    else {
        toks = toks+1;
        end = end-1;
    }
    while(toks < end && toks->type == CPP_WHITESPACE) toks++;
    while(toks < end && end[-1].type == CPP_WHITESPACE) end--;
    if(toks == end){ // pack()
        p->pragma_pack = 0; // default alignment
        goto finally;
    }
    const CppToken* number = NULL;
    if(toks->type == CPP_NUMBER){
        number = toks++;
        while(toks < end && toks->type == CPP_WHITESPACE) toks++;
    }
    else if(toks->type == CPP_IDENTIFIER){
        StringView word = toks->txt;
        toks++;
        while(toks < end && toks->type == CPP_WHITESPACE) toks++;
        if(sv_equals(word, SV("show"))){
            if(p->pragma_pack)
                cc_info(p, loc, "#pragma pack(show): %d", (int)p->pragma_pack);
            else
                cc_info(p, loc, "#pragma pack(show): default");
            if(toks != end) cc_warn(p, toks->loc, "Extra tokens after show");
            goto finally;
        }
        else if(sv_equals(word, SV("push"))){
            // #pragma pack( push [ , identifier ] [ , n ] )
            const CppToken* ident = NULL;
            if(toks != end && toks->type == CPP_PUNCTUATOR && toks->punct == ','){
                toks++;
                while(toks < end && toks->type == CPP_WHITESPACE) toks++;
                if(toks->type == CPP_IDENTIFIER){
                    ident = toks++;
                    while(toks < end && toks->type == CPP_WHITESPACE) toks++;
                    if(toks != end && toks->type == CPP_PUNCTUATOR && toks->punct == ','){
                        toks++;
                        while(toks < end && toks->type == CPP_WHITESPACE) toks++;
                    }
                }
                // technically this allows [,identifer] [n] instead of [, identifer] [, n] but whatever
                if(toks->type == CPP_NUMBER){
                    number = toks++;
                    while(toks < end && toks->type == CPP_WHITESPACE) toks++;
                }
            }
            CcPackRecord r = {
                .ident = ident?ident->txt:(StringView){0},
                .pack = p->pragma_pack,
            };
            err = ma_push(CcPackRecord)(&p->pack_stack, cc_allocator(p), r);
            if(err) goto finally;
        }
        else if(sv_equals(word, SV("pop"))){
            // #pragma pack( pop [ , { identifier | n } ] )
            const CppToken* ident = NULL;
            if(toks != end && toks->type == CPP_PUNCTUATOR && toks->punct == ','){
                toks++;
                while(toks < end && toks->type == CPP_WHITESPACE) toks++;
                if(toks->type == CPP_IDENTIFIER){
                    ident = toks++;
                    while(toks < end && toks->type == CPP_WHITESPACE) toks++;
                }
                else if(toks->type == CPP_NUMBER){
                    number = toks++;
                    while(toks < end && toks->type == CPP_WHITESPACE) toks++;
                }
            }
            if(!p->pack_stack.count){
                cc_warn(p, loc, "pack stack empty");
            }
            else {
                if(ident){
                    for(size_t i = p->pack_stack.count; i--; ){
                        if(sv_equals(p->pack_stack.data[i].ident, ident->txt)){
                            p->pragma_pack = p->pack_stack.data[i].pack;
                            p->pack_stack.count = i;
                            break;
                        }
                        if(i == 0){
                            cc_warn(p, ident->loc, "'%.*s' not found in pack stack", sv_p(ident->txt));
                        }
                    }
                }
                else {
                    p->pragma_pack = ma_tail(p->pack_stack).pack;
                    p->pack_stack.count--;
                }
            }
        }
        else {
            cc_warn(p, loc, "Unrecognized pragma pack() command");
            goto finally;
        }
    }
    if(number){
        int64_t pack = 8;
        err = cpp_eval_parse_number(cpp, *number, &pack);
        if(err) goto finally;
        if(pack < 0 || pack > UINT16_MAX){
            cc_warn(p, number->loc, "pack value too big, treating as 8");
            pack = 8;
        }
        if(pack != 1 && pack != 2 && pack != 4 && pack != 8 && pack != 16){
            cc_warn(p, number->loc, "value %lld invalid, treating as 8", (long long)pack);
            pack = 8;
        }
        p->pragma_pack = (uint16_t)pack;
    }
    if(toks != end){
        cc_warn(p, toks->loc, "Extra tokens in pack()");
    }
    finally:
    if(expanded)
        cpp_release_scratch(cpp, expanded);
    return err;
}

static
int
cc_pragma_typedef(void* _Null_unspecified ctx, CppPreprocessor* cpp, SrcLoc loc, const CppToken*_Null_unspecified toks, size_t ntoks){
    CcParser* p = (CcParser*)ctx;
    (void)cpp;
    const CppToken* end = toks+ntoks;
    while(toks < end && toks->type == CPP_WHITESPACE) toks++;
    while(toks < end && end[-1].type == CPP_WHITESPACE) end--;
    if(toks == end || toks->type != CPP_IDENTIFIER){
        return cc_error(p, loc, "#pragma typedef requires 'on' or 'off'");
    }
    if(sv_equals(toks->txt, SV("on"))){
        p->auto_typedef = 1;
    }
    else if(sv_equals(toks->txt, SV("off"))){
        p->auto_typedef = 0;
    }
    else {
        return cc_error(p, loc, "#pragma typedef requires 'on' or 'off'");
    }
    return 0;
}

static
int
cc_register_pragmas(CcParser* p){
    int err = cpp_register_pragma(&p->cpp, SV("pack"), cc_pragma_pack, p);
    if(err) return err;
    err = cpp_register_pragma(&p->cpp, SV("typedef"), cc_pragma_typedef, p);
    if(err) return err;
    return 0;
}

static
int
cc_lookup_field(CcParser* p, CcField*_Nullable fields, uint32_t count, Atom name,
    CcFieldPath*_Nullable out_path, CcQualType* out_type, CcQualType*_Nullable out_owner, CcField*_Nullable*_Nonnull out_field){
    *out_field = NULL;
    *out_type = CCQT_NONE;
    if(out_path) *out_path = (CcFieldPath){0};
    if(out_owner) *out_owner = CCQT_NONE;
    for(uint32_t i = 0; i < count; i++){
        CcField* field = &fields[i];
        if((field->is_method ? field->method->name : field->name) == name){
            if(out_path){
                CcFieldPath path;
                if(cc_field_path_make(cc_allocator(p), &i, 1, &path)) return CC_OOM_ERROR;
                *out_path = path;
            }
            *out_field = field;
            *out_type = field->type;
            return 0;
        }
        if(field->name || field->is_method || (ccqt_kind(field->type) != CC_STRUCT && ccqt_kind(field->type) != CC_UNION))
            continue;
        CcStruct* sub = ccqt_as_struct(field->type);
        CcFieldPath suffix = {0};
        int err = cc_lookup_field(p, sub->fields, sub->field_count, name, out_path ? &suffix : NULL,
            out_type, out_owner, out_field);
        if(err) return err;
        if(!*out_field) continue;
        if(out_path){
            CcFieldPath prefix = {0};
            CcFieldPath path = {0};
            err = cc_field_path_make(cc_allocator(p), &i, 1, &prefix);
            if(!err) err = cc_field_path_concat(cc_allocator(p), prefix, suffix, &path);
            cc_field_path_free(cc_allocator(p), prefix);
            cc_field_path_free(cc_allocator(p), suffix);
            if(err) return CC_OOM_ERROR;
            *out_path = path;
        }
        if(out_owner){
            if(!out_owner->bits) *out_owner = field->type;
            else out_owner->quals |= field->type.quals;
        }
        return 0;
    }
    return 0;
}

static
_Bool
cc_has_field(CcParser* p, CcField*_Nullable fields, uint32_t count, Atom name){
    CcQualType type;
    CcField* field;
    // A name probe doesn't construct or allocate a path.
    (void)cc_lookup_field(p, fields, count, name, NULL, &type, NULL, &field);
    return field != NULL;
}

static
uint32_t
cc_find_field_index(CcParser* p, CcField*_Nullable fields, uint32_t count, Atom name, CcQualType* out_type, Marray(uint32_t)* path){
    CcFieldPath member;
    CcField* field;
    int err = cc_lookup_field(p, fields, count, name, &member, out_type, NULL, &field);
    if(err) return UINT32_MAX;
    if(!field || field->is_method){
        cc_field_path_free(cc_allocator(p), member);
        return count;
    }
    uint32_t first = cc_field_path_component(member, 0);
    for(uint32_t i = 0, n = cc_field_path_count(member); i < n; i++){
        if(ma_push(uint32_t)(path, cc_allocator(p), cc_field_path_component(member, i))){ err = CC_OOM_ERROR; break; }
    }
    cc_field_path_free(cc_allocator(p), member);
    return err ? UINT32_MAX : first;
}

static
int
cc_get_fields(CcQualType t, CcField*_Nullable*_Nonnull out_fields, uint32_t* out_count){
    CcTypeKind tk = ccqt_kind(t);
    if(tk == CC_STRUCT){
        CcStruct* s = ccqt_as_struct(t);
        *out_fields = s->fields;
        *out_count = s->field_count;
        return 0;
    }
    if(tk == CC_UNION){
        CcUnion* u = ccqt_as_union(t);
        *out_fields = u->fields;
        *out_count = u->field_count;
        return 0;
    }
    return 1;
}

static
int
cc_lookup_field_offset(CcParser* p, CcQualType type, Atom name, uint64_t* out_offset, CcQualType* out_type, CcField*_Nullable*_Nonnull out_field){
    CcField* fields;
    uint32_t count;
    if(cc_get_fields(type, &fields, &count)){ *out_field = NULL; return 0; }
    CcFieldPath path;
    int err = cc_lookup_field(p, fields, count, name, &path, out_type, NULL, out_field);
    if(err || !*out_field) return err;
    err = cc_field_path_resolve(cc_target(p), type, path, out_offset);
    cc_field_path_free(cc_allocator(p), path);
    return err;
}

static
int
cc_push_scalar(CcParser* p, CcExpr* value, CcQualType target, Marray(CcInitEntry)* buf, Marray(uint32_t)* path){
    CcQualType t = {.unqual=target.unqual};
    CcExpr* casted;
    int err = cc_implicit_cast(p, value, t, &casted);
    if(err) return err;
    CcInitEntry entry = {.value = casted};
    if(path->count > UINT32_MAX || cc_field_path_make(cc_allocator(p), path->data, (uint32_t)path->count, &entry.path)){
        cc_release_expr(p, casted);
        return CC_OOM_ERROR;
    }
    err = ma_push(CcInitEntry)(buf, cc_allocator(p), entry);
    if(err){
        cc_field_path_free(cc_allocator(p), entry.path);
        cc_release_expr(p, casted);
        return CC_OOM_ERROR;
    }
    return 0;
}

static int
cc_init_list_comma(CcParser* p){
    CcToken peek;
    int err = cc_peek(p, &peek);
    if(err) return err;
    if(peek.type == CC_PUNCTUATOR && peek.punct.punct == ','){
        cc_next_token(p, &peek);
        return 0;
    }
    if(peek.type == CC_PUNCTUATOR && peek.punct.punct == CC_rbrace)
        return 0;
    return cc_error(p, peek.loc, "expected ',' or '}' in initializer list");
}

static
int
cc_parse_scalar_value(CcParser* p, CcValueClass vc, CcQualType target, CcExpr*_Nullable*_Nonnull out){
    CcToken peek;
    int err = cc_peek(p, &peek);
    if(err) return err;
    if(peek.type == CC_PUNCTUATOR && peek.punct.punct == CC_lbrace){
        uint32_t depth = 0;
        while(peek.type == CC_PUNCTUATOR && peek.punct.punct == CC_lbrace){
            cc_next_token(p, &peek);
            depth++;
            err = cc_peek(p, &peek);
            if(err) return err;
        }
        if(peek.type == CC_PUNCTUATOR && peek.punct.punct == CC_rbrace){
            CcInitList* list = Allocator_zalloc(cc_allocator(p), sizeof *list);
            if(!list) return CC_OOM_ERROR;
            list->loc = peek.loc;
            CcExpr* node = cc_make_expr(p, CC_EXPR_INIT_LIST, peek.loc, (CcQualType){.unqual = target.unqual}, 0);
            if(!node){
                Allocator_free(cc_allocator(p), list, sizeof *list);
                return CC_OOM_ERROR;
            }
            node->init_list = list;
            *out = node;
        }
        else {
            err = cc_parse_assignment_expr(p, vc, out, CCQT_NONE);
            if(err) return err;
        }
        for(uint32_t i = 0; i < depth; i++){
            err = cc_peek(p, &peek);
            if(err) return err;
            if(peek.type == CC_PUNCTUATOR && peek.punct.punct == ','){
                cc_next_token(p, &peek);
                err = cc_peek(p, &peek);
                if(err) return err;
                if(!(peek.type == CC_PUNCTUATOR && peek.punct.punct == CC_rbrace))
                    return cc_error(p, peek.loc, "excess elements in scalar initializer");
            }
            err = cc_expect_punct(p, CC_rbrace);
            if(err) return err;
        }
        return 0;
    }
    return cc_parse_assignment_expr(p, vc, out, CCQT_NONE);
}

static
int
cc_parse_desig_tail(CcParser* p, CcQualType* sub, Marray(uint32_t)* path){
    int err;
    CcToken peek;
    for(;;){
        err = cc_peek(p, &peek);
        if(err) return err;
        if(peek.type != CC_PUNCTUATOR) break;
        if(peek.punct.punct == '.'){
            cc_next_token(p, &peek);
            CcToken field_tok;
            err = cc_next_token(p, &field_tok);
            if(err) return err;
            if(field_tok.type != CC_IDENTIFIER)
                return cc_error(p, field_tok.loc, "expected field name after '.'");
            CcField*_Nullable sub_fields;
            uint32_t sub_count;
            if(cc_get_fields(*sub, &sub_fields, &sub_count))
                return cc_error(p, peek.loc, "member designator into non-struct/union type");
            CcQualType inner_type;
            uint32_t index = cc_find_field_index(p, sub_fields, sub_count, field_tok.ident.ident, &inner_type, path);
            if(index == UINT32_MAX) return CC_OOM_ERROR;
            if(index >= sub_count)
                return cc_error(p, peek.loc, "no member named '%.*s'", field_tok.ident.ident->length, field_tok.ident.ident->data);
            *sub = inner_type;
        }
        else if(peek.punct.punct == CC_lbracket){
            cc_next_token(p, &peek);
            if(ccqt_kind(*sub) != CC_ARRAY)
                return cc_error(p, peek.loc, "index designator into non-array type");
            CcArray* a = ccqt_as_array(*sub);
            CcExpr* idx_expr;
            err = cc_parse_assignment_expr(p, CC_CONSTEXPR_VALUE, &idx_expr, CCQT_NONE);
            if(err) return err;
            int64_t idx_signed;
            err = cc_eval_integer(&(CcEvalCtx){p}, idx_expr, &idx_signed);
            cc_release_expr(p, idx_expr);
            if(err && err != CC_NOT_CONSTANT_ERROR) return err;
            if(err)
                return cc_error(p, peek.loc, "array designator must be a constant integer expression");
            if(idx_signed < 0 || idx_signed > UINT32_MAX)
                return cc_error(p, peek.loc, "array designator value out of range");
            uint32_t idx = (uint32_t)idx_signed;
            if(ma_push(uint32_t)(path, cc_allocator(p), idx)) return CC_OOM_ERROR;
            err = cc_expect_punct(p, CC_rbracket);
            if(err) return err;
            if(!a->is_incomplete && idx >= a->length)
                return cc_error(p, peek.loc, "array index %u out of bounds (size %zu)", idx, a->length);
            uint32_t esz = 0;
            err = cc_sizeof_as_uint(p, a->element, peek.loc, &esz);
            if(err) return err;
            *sub = a->element;
        }
        else break;
    }
    return cc_expect_punct(p, CC_assign);
}

static
int
cc_init_apply_value(CcParser* p, CcValueClass vc, CcQualType field_type, CcExpr* value, SrcLoc loc, Marray(CcInitEntry)* buf, Marray(uint32_t)* path){
    if(value->kind == CC_EXPR_COMPOUND_LITERAL)
        value->kind = CC_EXPR_INIT_LIST;
    CcQualType unqual = {.unqual=field_type.unqual};
    CcTypeKind ftk = ccqt_kind(unqual);
    if(ftk == CC_STRUCT || ftk == CC_UNION || ftk == CC_ARRAY){
        if(cc_implicit_convertible(p, value->type, unqual))
            return cc_push_scalar(p, value, unqual, buf, path);
        return cc_parse_init(p, vc,field_type, 0, loc, buf, NULL, value, path);
    }
    return cc_push_scalar(p, value, field_type, buf, path);
}

static
_Bool
cc_string_array_element(CcParser* p, CcQualType element){
    CcBasicTypeKind kind = ccqt_is_basic(element) ? element.basic.kind : CCBT_COUNT;
    return kind == CCBT_char || kind == CCBT_signed_char || kind == CCBT_unsigned_char
        || kind == cc_target(p)->wchar_type || kind == cc_target(p)->char16_type
        || kind == cc_target(p)->char32_type;
}

static
int
cc_check_string_array(CcParser* p, CcQualType target_type, CcExpr* value){
    if(value->kind != CC_EXPR_VALUE || ccqt_kind(value->type) != CC_ARRAY || !value->text)
        return cc_error(p, value->loc, "array initializer requires a string literal");
    CcQualType target = ccqt_as_array(target_type)->element;
    CcQualType source = ccqt_as_array(value->type)->element;
    if(target.unqual == source.unqual) return 0;
    _Bool source_char = ccqt_bt_eq(source, CCBT_char) || ccqt_bt_eq(source, CCBT_signed_char) || ccqt_bt_eq(source, CCBT_unsigned_char);
    _Bool target_char = ccqt_bt_eq(target, CCBT_char) || ccqt_bt_eq(target, CCBT_signed_char) || ccqt_bt_eq(target, CCBT_unsigned_char);
    if(source_char && target_char) return 0;
    return cc_error(p, value->loc, "string literal has incompatible array element type");
}

static
int
cc_parse_init_value(CcParser* p, CcValueClass vc, CcQualType field_type, _Bool positional, SrcLoc loc, Marray(CcInitEntry)* buf, Marray(uint32_t)* path){
    int err;
    CcTypeKind ftk = ccqt_kind(field_type);
    if(ftk == CC_STRUCT || ftk == CC_UNION || ftk == CC_ARRAY){
        CcToken peek;
        err = cc_peek(p, &peek);
        if(err) return err;
        if(peek.type == CC_PUNCTUATOR && peek.punct.punct == CC_lbrace){
            CcExpr* value;
            err = cc_parse_init_list(p, vc, &value, field_type);
            if(err) return err;
            return cc_push_scalar(p, value, field_type, buf, path);
        }
        if(ftk == CC_ARRAY && peek.type == CC_STRING_LITERAL){
            CcArray* arr = ccqt_as_array(field_type);
            if(cc_string_array_element(p, arr->element)){
                CcExpr* v;
                err = cc_parse_assignment_expr(p, vc, &v, CCQT_NONE);
                if(err) return err;
                if(v->kind != CC_EXPR_VALUE || ccqt_kind(v->type) != CC_ARRAY || !v->text)
                    return cc_init_apply_value(p, vc, field_type, v, loc, buf, path);
                err = cc_check_string_array(p, field_type, v);
                if(err) return err;
                if(!arr->is_incomplete && arr->length < v->str.length - 1)
                    return cc_error(p, v->loc, "initializer string too long for array");
                // Truncate string literal if needed.
                if(!arr->is_incomplete && arr->length < ccqt_as_array(v->type)->length)
                    v->type = field_type;
                if(!arr->is_incomplete){
                    CcInitList* list = Allocator_zalloc(cc_allocator(p), sizeof(CcInitList) + sizeof(CcInitEntry));
                    if(!list) return CC_OOM_ERROR;
                    CcExpr* node = cc_make_expr(p, CC_EXPR_INIT_LIST, v->loc, field_type, 0);
                    if(!node){
                        Allocator_free(cc_allocator(p), list, sizeof(CcInitList) + sizeof(CcInitEntry));
                        return CC_OOM_ERROR;
                    }
                    list->loc = v->loc;
                    list->count = 1;
                    list->entries[0].value = v;
                    node->init_list = list;
                    v = node;
                }
                return cc_push_scalar(p, v, field_type, buf, path);
            }
        }
        {
            CcExpr* v;
            err = cc_parse_assignment_expr(p, vc, &v, CCQT_NONE);
            if(err) return err;
            if(positional)
                return cc_init_apply_value(p, vc,field_type, v, loc, buf, path);
            return cc_push_scalar(p, v, field_type, buf, path);
        }
    }
    // Scalar
    CcExpr* v;
    err = cc_parse_scalar_value(p, vc, field_type, &v);
    if(err) return err;
    return cc_push_scalar(p, v, field_type, buf, path);
}

static
int
cc_is_parent_token(CcParser* p, _Bool* out){
    CcToken peek;
    int err = cc_peek(p, &peek);
    if(err) return err;
    if(peek.type == CC_PUNCTUATOR){
        CcPunct pp = peek.punct.punct;
        if(pp == CC_rbrace || pp == '.' || pp == CC_lbracket){
            *out = 1;
            return 0;
        }
    }
    if(peek.type == CC_EOF){
        *out = 1;
        return 0;
    }
    *out = 0;
    return 0;
}

static
int
cc_parse_init(CcParser* p, CcValueClass vc, CcQualType target, _Bool braced, SrcLoc loc, Marray(CcInitEntry)* buf, uint32_t*_Nullable out_max_index, CcExpr*_Nullable first_value, Marray(uint32_t)* path){
    int err;
    size_t path_depth = path->count;
    CcQualType unqual = {.unqual=target.unqual};
    CcTypeKind tk = ccqt_kind(unqual);
    switch(tk){
    case CC_STRUCT: {
        CcStruct* s = ccqt_as_struct(unqual);
        if(s->is_incomplete)
            return cc_error(p, loc, "initializer for incomplete struct type");
        uint32_t fi = 0;
        for(;;){
            path->count = path_depth;
            if(first_value){
                CcExpr* fv = first_value;
                while(fi < s->field_count && (s->fields[fi].is_method || (!s->fields[fi].name && s->fields[fi].is_bitfield)))
                    fi++;
                if(fi >= s->field_count) break;
                CcField* fld = &s->fields[fi];
                if(ma_push(uint32_t)(path, cc_allocator(p), fi)) return CC_OOM_ERROR;
                err = cc_init_apply_value(p, vc,fld->type, fv, loc, buf, path);
                if(err) return err;
                first_value = NULL;
                fi++;
                while(fi < s->field_count && (s->fields[fi].is_method || (!s->fields[fi].name && s->fields[fi].is_bitfield)))
                    fi++;
                if(fi >= s->field_count) break;
                CcToken peek;
                err = cc_peek(p, &peek);
                if(err) return err;
                if(peek.type == CC_PUNCTUATOR && peek.punct.punct == ',')
                    cc_next_token(p, &peek);
                else break;
                continue;
            }
            CcToken peek;
            err = cc_peek(p, &peek);
            if(err) return err;
            if(braced){
                if(peek.type == CC_PUNCTUATOR && peek.punct.punct == CC_rbrace){
                    cc_next_token(p, &peek);
                    break;
                }
                if(peek.type == CC_EOF)
                    return cc_error(p, loc, "unterminated initializer list");
                if(peek.type == CC_PUNCTUATOR && peek.punct.punct == CC_lbracket)
                    return cc_error(p, peek.loc, "array designator in struct initializer");
                if(peek.type == CC_PUNCTUATOR && peek.punct.punct == '.'){
                    cc_next_token(p, &peek);
                    SrcLoc desig_loc = peek.loc;
                    CcToken field_tok;
                    err = cc_next_token(p, &field_tok);
                    if(err) return err;
                    if(field_tok.type != CC_IDENTIFIER)
                        return cc_error(p, field_tok.loc, "expected field name after '.'");
                    CcQualType sub;
                    uint32_t idx = cc_find_field_index(p, s->fields, s->field_count, field_tok.ident.ident, &sub, path);
                    if(idx == UINT32_MAX) return CC_OOM_ERROR;
                    if(idx >= s->field_count)
                        return cc_error(p, desig_loc, "no member named '%.*s'", field_tok.ident.ident->length, field_tok.ident.ident->data);
                    fi = idx + 1;
                    err = cc_parse_desig_tail(p, &sub, path);
                    if(err) return err;
                    err = cc_parse_init_value(p, vc,sub, 0, desig_loc, buf, path);
                    if(err) return err;
                    err = cc_init_list_comma(p);
                    if(err) return err;
                    continue;
                }
            }
            else {
                _Bool stop;
                err = cc_is_parent_token(p, &stop);
                if(err) return err;
                if(stop) break;
            }
            while(fi < s->field_count && (s->fields[fi].is_method || (!s->fields[fi].name && s->fields[fi].is_bitfield)))
                fi++;
            if(fi >= s->field_count){
                if(braced){
                    if(peek.type == CC_PUNCTUATOR && peek.punct.punct == CC_rbrace){
                        cc_next_token(p, &peek);
                        break;
                    }
                    return cc_error(p, peek.loc, "excess elements in struct initializer");
                }
                break;
            }
            CcField* fld = &s->fields[fi];
            if(ma_push(uint32_t)(path, cc_allocator(p), fi)) return CC_OOM_ERROR;
            err = cc_parse_init_value(p, vc,fld->type, 1, loc, buf, path);
            if(err) return err;
            fi++;
            while(fi < s->field_count && (s->fields[fi].is_method || (!s->fields[fi].name && s->fields[fi].is_bitfield)))
                fi++;
            if(braced){
                err = cc_init_list_comma(p);
                if(err) return err;
            }
            else {
                if(fi >= s->field_count) break;
                err = cc_peek(p, &peek);
                if(err) return err;
                if(peek.type == CC_PUNCTUATOR && peek.punct.punct == ',')
                    cc_next_token(p, &peek);
                else break;
            }
        }
    } break;
    case CC_UNION: {
        CcUnion* u = ccqt_as_union(unqual);
        if(u->is_incomplete)
            return cc_error(p, loc, "initializer for incomplete union type");
        CcToken peek;
        err = cc_peek(p, &peek);
        if(err) return err;
        if(braced && peek.type == CC_PUNCTUATOR && peek.punct.punct == CC_rbrace){
            cc_next_token(p, &peek);
            return 0;
        }
        if(braced){
            if(peek.type == CC_EOF)
                return cc_error(p, loc, "unterminated initializer list");
            if(peek.type == CC_PUNCTUATOR && peek.punct.punct == CC_lbracket)
                return cc_error(p, peek.loc, "array designator in union initializer");
            if(peek.type == CC_PUNCTUATOR && peek.punct.punct == '.'){
                cc_next_token(p, &peek);
                SrcLoc desig_loc = peek.loc;
                CcToken field_tok;
                err = cc_next_token(p, &field_tok);
                if(err) return err;
                if(field_tok.type != CC_IDENTIFIER)
                    return cc_error(p, field_tok.loc, "expected field name after '.'");
                CcQualType sub;
                uint32_t ufi = cc_find_field_index(p, u->fields, u->field_count, field_tok.ident.ident, &sub, path);
                if(ufi == UINT32_MAX) return CC_OOM_ERROR;
                if(ufi >= u->field_count)
                    return cc_error(p, desig_loc, "no member named '%.*s'", field_tok.ident.ident->length, field_tok.ident.ident->data);
                err = cc_parse_desig_tail(p, &sub, path);
                if(err) return err;
                err = cc_parse_init_value(p, vc,sub, 0, desig_loc, buf, path);
                if(err) return err;
                for(;;){
                    path->count = path_depth;
                    err = cc_init_list_comma(p);
                    if(err) return err;
                    err = cc_peek(p, &peek);
                    if(err) return err;
                    if(peek.type == CC_PUNCTUATOR && peek.punct.punct == CC_rbrace)
                        break;
                    if(peek.type != CC_PUNCTUATOR || peek.punct.punct != '.')
                        return cc_error(p, peek.loc, "expected '.' or '}' in union initializer");
                    cc_next_token(p, &peek);
                    desig_loc = peek.loc;
                    err = cc_next_token(p, &field_tok);
                    if(err) return err;
                    if(field_tok.type != CC_IDENTIFIER)
                        return cc_error(p, field_tok.loc, "expected field name after '.'");
                    ufi = cc_find_field_index(p, u->fields, u->field_count, field_tok.ident.ident, &sub, path);
                    if(ufi == UINT32_MAX) return CC_OOM_ERROR;
                    if(ufi >= u->field_count)
                        return cc_error(p, desig_loc, "no member named '%.*s'", field_tok.ident.ident->length, field_tok.ident.ident->data);
                    err = cc_parse_desig_tail(p, &sub, path);
                    if(err) return err;
                    err = cc_parse_init_value(p, vc,sub, 0, desig_loc, buf, path);
                    if(err) return err;
                }
                return cc_expect_punct(p, CC_rbrace);
            }
        }
        CcField* field = NULL;
        for(uint32_t fi = 0; fi < u->field_count; fi++){
            if(u->fields[fi].name || (!u->fields[fi].is_bitfield && !u->fields[fi].is_method)){
                field = &u->fields[fi];
                break;
            }
        }
        if(!field)
            return cc_error(p, loc, "initializer for empty union");
        if(ma_push(uint32_t)(path, cc_allocator(p), (uint32_t)(field - u->fields))) return CC_OOM_ERROR;
        if(first_value){
            CcExpr* fv = first_value;
            first_value = NULL;
            err = cc_init_apply_value(p, vc,field->type, fv, loc, buf, path);
        }
        else
            err = cc_parse_init_value(p, vc,field->type, 1, loc, buf, path);
        if(err) return err;
        if(braced){
            err = cc_init_list_comma(p);
            if(err) return err;
            err = cc_peek(p, &peek);
            if(err) return err;
            // A positional initializer selects the first member, but later
            // designators may select another member of this same union.
            if(peek.type == CC_PUNCTUATOR && peek.punct.punct == '.'){
                path->count = path_depth;
                return cc_parse_init(p, vc, target, 1, loc, buf, NULL, NULL, path);
            }
            err = cc_expect_punct(p, CC_rbrace);
            if(err) return err;
        }
    } break;
    case CC_ARRAY: {
        CcArray* arr = ccqt_as_array(unqual);
        CcQualType elem = arr->element;
        uint32_t elem_size = 0;
        err = cc_sizeof_as_uint(p, elem, loc, &elem_size);
        if(err) return err;
        if(!arr->is_incomplete && !arr->is_vector){
            uint32_t size;
            err = cc_sizeof_as_uint(p, unqual, loc, &size);
            if(err) return err;
        }
        if(arr->is_vector){
            uint32_t length = (uint32_t)arr->length;
            uint32_t ai = 0;
            for(;;){
                path->count = path_depth;
                CcToken peek;
                err = cc_peek(p, &peek);
                if(err) return err;
                if(braced){
                    if(peek.type == CC_PUNCTUATOR && peek.punct.punct == CC_rbrace){
                        cc_next_token(p, &peek);
                        break;
                    }
                    if(peek.type == CC_EOF)
                        return cc_error(p, loc, "unterminated initializer list");
                    if(peek.type == CC_PUNCTUATOR && (peek.punct.punct == '.' || peek.punct.punct == CC_lbracket))
                        return cc_error(p, peek.loc, "designators not allowed in vector initializer");
                }
                else {
                    _Bool stop;
                    err = cc_is_parent_token(p, &stop);
                    if(err) return err;
                    if(stop) break;
                }
                if(ai >= length){
                    if(braced)
                        return cc_error(p, peek.loc, "excess elements in vector initializer");
                    break;
                }
                CcExpr* v;
                err = cc_parse_scalar_value(p, vc, elem, &v);
                if(err) return err;
                if(ma_push(uint32_t)(path, cc_allocator(p), ai)) return CC_OOM_ERROR;
                err = cc_push_scalar(p, v, elem, buf, path);
                if(err) return err;
                ai++;
                if(braced){
                    err = cc_init_list_comma(p);
                    if(err) return err;
                }
                else {
                    if(ai >= length) break;
                    err = cc_peek(p, &peek);
                    if(err) return err;
                    if(peek.type == CC_PUNCTUATOR && peek.punct.punct == ',')
                        cc_next_token(p, &peek);
                    else break;
                }
            }
            break;
        }
        uint32_t ai = 0, max_ai = 0;
        for(;;){
            path->count = path_depth;
            if(first_value){
                CcExpr* fv = first_value;
                if(!arr->is_incomplete && ai >= arr->length) break;
                if(ma_push(uint32_t)(path, cc_allocator(p), ai)) return CC_OOM_ERROR;
                err = cc_init_apply_value(p, vc,elem, fv, loc, buf, path);
                if(err) return err;
                first_value = NULL;
                ai++;
                if(ai > max_ai) max_ai = ai;
                if(braced){
                    err = cc_init_list_comma(p);
                    if(err) return err;
                    continue;
                }
                if(!arr->is_incomplete && ai >= arr->length) break;
                CcToken peek;
                err = cc_peek(p, &peek);
                if(err) return err;
                if(peek.type == CC_PUNCTUATOR && peek.punct.punct == ',')
                    cc_next_token(p, &peek);
                else break;
                continue;
            }
            CcToken peek;
            err = cc_peek(p, &peek);
            if(err) return err;
            if(braced){
                if(peek.type == CC_PUNCTUATOR && peek.punct.punct == CC_rbrace){
                    cc_next_token(p, &peek);
                    break;
                }
                if(peek.type == CC_EOF)
                    return cc_error(p, loc, "unterminated initializer list");
                if(peek.type == CC_PUNCTUATOR && peek.punct.punct == '.')
                    return cc_error(p, peek.loc, "field designator in array initializer");
                if(peek.type == CC_PUNCTUATOR && peek.punct.punct == CC_lbracket){
                    cc_next_token(p, &peek);
                    SrcLoc desig_loc = peek.loc;
                    CcExpr* idx_expr;
                    err = cc_parse_assignment_expr(p, CC_CONSTEXPR_VALUE, &idx_expr, CCQT_NONE);
                    if(err) return err;
                    int64_t idx_signed;
                    err = cc_eval_integer(&(CcEvalCtx){p}, idx_expr, &idx_signed);
                    cc_release_expr(p, idx_expr);
                    if(err && err != CC_NOT_CONSTANT_ERROR) return err;
                    if(err)
                        return cc_error(p, desig_loc, "array designator must be a constant integer expression");
                    if(idx_signed < 0 || idx_signed > UINT32_MAX)
                        return cc_error(p, desig_loc, "array designator value out of range");
                    uint32_t idx = (uint32_t)idx_signed;
                    err = cc_expect_punct(p, CC_rbracket);
                    if(err) return err;
                    if(!arr->is_incomplete && idx >= arr->length)
                        return cc_error(p, desig_loc, "array index %u out of bounds (size %zu)", idx, arr->length);
                    uint32_t size;
                    if(add_overflow(idx, 1u, &ai) || cc_layout_array_size(elem_size, ai, &size))
                        return cc_error(p, desig_loc, "object size exceeds 32-bit layout limit");
                    if(ai > max_ai) max_ai = ai;
                    CcQualType sub = elem;
                    if(ma_push(uint32_t)(path, cc_allocator(p), idx)) return CC_OOM_ERROR;
                    err = cc_parse_desig_tail(p, &sub, path);
                    if(err) return err;
                    err = cc_parse_init_value(p, vc,sub, 0, desig_loc, buf, path);
                    if(err) return err;
                    err = cc_init_list_comma(p);
                    if(err) return err;
                    continue;
                }
            }
            else {
                _Bool stop;
                err = cc_is_parent_token(p, &stop);
                if(err) return err;
                if(stop) break;
            }
            if(!arr->is_incomplete && ai >= arr->length){
                if(braced){
                    err = cc_peek(p, &peek);
                    if(err) return err;
                    if(peek.type == CC_PUNCTUATOR && peek.punct.punct == CC_rbrace){
                        cc_next_token(p, &peek);
                        break;
                    }
                    return cc_error(p, peek.loc, "excess elements in array initializer");
                }
                break;
            }
            uint32_t size, next;
            if(add_overflow(ai, 1u, &next) || cc_layout_array_size(elem_size, next, &size))
                return cc_error(p, peek.loc, "object size exceeds 32-bit layout limit");
            if(ma_push(uint32_t)(path, cc_allocator(p), ai)) return CC_OOM_ERROR;
            err = cc_parse_init_value(p, vc,elem, 1, loc, buf, path);
            if(err) return err;
            ai = next;
            if(ai > max_ai) max_ai = ai;
            if(braced){
                err = cc_init_list_comma(p);
                if(err) return err;
            }
            else {
                if(!arr->is_incomplete && ai >= arr->length) break;
                err = cc_peek(p, &peek);
                if(err) return err;
                if(peek.type == CC_PUNCTUATOR && peek.punct.punct == ',')
                    cc_next_token(p, &peek);
                else break;
            }
        }
        if(out_max_index) *out_max_index = max_ai;
    } break;
    case CC_BASIC:
    case CC_ENUM:
    case CC_POINTER:
    case CC_BLOCK_POINTER:
    case CC_FUNCTION:
    case CC_SLICE:
        return cc_error(p, loc, "cannot initialize type with initializer list");
    }
    return 0;
}

static
int
cc_parse_init_list(CcParser* p, CcValueClass vc, CcExpr* _Nullable* _Nonnull out, CcQualType target_type){
    CcToken brace;
    int err = cc_next_token(p, &brace);
    if(err) return err;
    SrcLoc loc = brace.loc;
    CcInitList* list = NULL;
    CcQualType resolved_type = target_type;
    CcTypeKind tk = ccqt_kind(target_type);
    CcExpr* first_value = NULL;
    if(tk == CC_ARRAY && cc_string_array_element(p, ccqt_as_array(target_type)->element)){
        CcToken peek;
        err = cc_peek(p, &peek);
        if(err) return err;
        if(peek.type == CC_STRING_LITERAL){
            CcExpr* value;
            err = cc_parse_assignment_expr(p, vc, &value, CCQT_NONE);
            if(err) return err;
            if(value->kind != CC_EXPR_VALUE || ccqt_kind(value->type) != CC_ARRAY || !value->text){
                first_value = value;
                goto parse_aggregate;
            }
            err = cc_check_string_array(p, target_type, value);
            if(err) return err;
            CcArray* target = ccqt_as_array(target_type);
            CcArray* source = ccqt_as_array(value->type);
            if(target->is_incomplete){
                CcArray* array = cc_intern_array(&p->type_cache, cc_allocator(p), target->element,
                    source->length, target->is_static, 0, 0, 0);
                if(!array) return CC_OOM_ERROR;
                resolved_type = (CcQualType){.bits = (uintptr_t)array | target_type.quals};
            }
            else if(target->length < source->length - 1)
                return cc_error(p, value->loc, "initializer string too long for array");
            else if(target->length < source->length)
                value->type = target_type;
            err = cc_peek(p, &peek);
            if(err) return err;
            if(peek.type == CC_PUNCTUATOR && peek.punct.punct == ',')
                cc_next_token(p, &peek);
            err = cc_expect_punct(p, CC_rbrace);
            if(err) return err;
            list = Allocator_zalloc(cc_allocator(p), sizeof(CcInitList) + sizeof(CcInitEntry));
            if(!list) return CC_OOM_ERROR;
            list->loc = loc;
            list->count = 1;
            list->entries[0] = (CcInitEntry){.value = value};
            goto make_node;
        }
    }
    if(tk == CC_BASIC || tk == CC_POINTER || tk == CC_ENUM || tk == CC_BLOCK_POINTER || tk == CC_SLICE){
        CcToken peek;
        err = cc_peek(p, &peek);
        if(err) return err;
        if(peek.type == CC_PUNCTUATOR && peek.punct.punct == CC_rbrace){
            cc_next_token(p, &peek);
            list = Allocator_zalloc(cc_allocator(p), sizeof(CcInitList));
            if(!list) return CC_OOM_ERROR;
            list->loc = loc;
            list->count = 0;
        }
        else {
            if(peek.type == CC_PUNCTUATOR && (peek.punct.punct == '.' || peek.punct.punct == CC_lbracket))
                return cc_error(p, peek.loc, "designators not allowed in scalar initializer");
            CcExpr* v;
            err = cc_parse_scalar_value(p, vc, target_type, &v);
            if(err) return err;
            CcQualType t = {.unqual=target_type.unqual};
            err = cc_implicit_cast(p, v, t, &v);
            if(err) return err;
            err = cc_peek(p, &peek);
            if(err) return err;
            if(peek.type == CC_PUNCTUATOR && peek.punct.punct == ','){
                cc_next_token(p, &peek);
                err = cc_peek(p, &peek);
                if(err) return err;
                if(!(peek.type == CC_PUNCTUATOR && peek.punct.punct == CC_rbrace))
                    return cc_error(p, peek.loc, "excess elements in scalar initializer");
            }
            err = cc_expect_punct(p, CC_rbrace);
            if(err) return err;
            list = Allocator_zalloc(cc_allocator(p), sizeof(CcInitList) + sizeof(CcInitEntry));
            if(!list) return CC_OOM_ERROR;
            list->loc = loc;
            list->count = 1;
            list->entries[0] = (CcInitEntry){.value = v};
        }
    }
    else {
        parse_aggregate:;
        Marray(CcInitEntry) entries = {0};
        Marray(uint32_t) path = {0};
        uint32_t max_index = 0;
        err = cc_parse_init(p, vc,target_type, 1, loc, &entries, &max_index, first_value, &path);
        ma_cleanup(uint32_t)(&path, cc_allocator(p));
        if(err){
            for(size_t i = 0; i < entries.count; i++){
                cc_field_path_free(cc_allocator(p), entries.data[i].path);
                if(entries.data[i].value) cc_release_expr(p, entries.data[i].value);
            }
            ma_cleanup(CcInitEntry)(&entries, cc_allocator(p));
            return err;
        }
        list = Allocator_zalloc(cc_allocator(p), sizeof(CcInitList) + entries.count * sizeof(CcInitEntry));
        if(!list){
            for(size_t i = 0; i < entries.count; i++){
                cc_field_path_free(cc_allocator(p), entries.data[i].path);
                if(entries.data[i].value) cc_release_expr(p, entries.data[i].value);
            }
            ma_cleanup(CcInitEntry)(&entries, cc_allocator(p));
            return CC_OOM_ERROR;
        }
        list->loc = loc;
        list->count = (uint32_t)entries.count;
        for(size_t i = 0; i < entries.count; i++) list->entries[i] = entries.data[i];
        ma_cleanup(CcInitEntry)(&entries, cc_allocator(p));
        if(tk == CC_ARRAY){
            CcArray* arr = ccqt_as_array(target_type);
            if(arr->is_incomplete){
                CcArray* new_arr = cc_intern_array(&p->type_cache, cc_allocator(p), arr->element, max_index, arr->is_static, 0, 0, 0);
                if(!new_arr){ err = CC_OOM_ERROR; goto release_list; }
                resolved_type = (CcQualType){.bits = (uintptr_t)new_arr | (target_type.quals)};
            }
        }
    }
    make_node:;
    // Validate subobject paths without storing a byte layout.
    for(uint32_t i = 0; i < list->count; i++){
        uint64_t offset;
        err = cc_field_path_resolve(cc_target(p), resolved_type, list->entries[i].path, &offset);
        if(err) goto release_list;
    }
    CcExpr* node = cc_make_expr(p, CC_EXPR_INIT_LIST, loc, resolved_type, 0);
    if(!node){ err = CC_OOM_ERROR; goto release_list; }
    node->init_list = list;
    *out = node;
    return 0;
    release_list:
    for(uint32_t i = 0; i < list->count; i++){
        cc_field_path_free(cc_allocator(p), list->entries[i].path);
        if(list->entries[i].value) cc_release_expr(p, list->entries[i].value);
    }
    Allocator_free(cc_allocator(p), list, sizeof *list + (size_t)list->count * sizeof(CcInitEntry));
    return err;
}

static
int
cc_check_anon_member_duplicates(CcParser* p, CcField* existing, uint32_t existing_count, CcQualType anon_type, SrcLoc loc){
    CcField* inner_fields;
    uint32_t inner_count;
    CcTypeKind tk = ccqt_kind(anon_type);
    if(tk == CC_STRUCT){
        CcStruct* s = ccqt_as_struct(anon_type);
        inner_fields = s->fields;
        inner_count = s->field_count;
    }
    else if(tk == CC_UNION){
        CcUnion* u = ccqt_as_union(anon_type);
        inner_fields = u->fields;
        inner_count = u->field_count;
    }
    else {
        return ((void)cc_error(p, loc, "ICE: bad assumption about anonymous field"), CC_UNREACHABLE_ERROR);
    }
    for(uint32_t i = 0; i < inner_count; i++){
        CcField* f = &inner_fields[i];
        if(f->is_method){
            if(cc_has_field(p, existing, existing_count, f->method->name))
                return cc_error(p, loc, "duplicate member '%s'", f->method->name->data);
        }
        else if(f->name){
            if(cc_has_field(p, existing, existing_count, f->name))
                return cc_error(p, loc, "duplicate member '%s'", f->name->data);
        }
        else {
            CcTypeKind ftk = ccqt_kind(f->type);
            if(ftk == CC_STRUCT || ftk == CC_UNION){
                int err = cc_check_anon_member_duplicates(p, existing, existing_count, f->type, loc);
                if(err) return err;
            }
        }
    }
    return 0;
}

static
int
cc_parse_struct_or_union(CcParser* p, SrcLoc loc, _Bool is_union, CcQualType* base_type){
    int err = 0;
    CcToken tok;
    CcAttributes attrs = p->attributes;
    cc_clear_attributes(&p->attributes);
    err = cc_parse_attributes(p, &attrs);
    if(err) return err;
    err = cc_parse_declspec(p, &attrs);
    if(err) return err;
    Atom name = NULL;
    err = cc_peek(p, &tok);
    if(err) return err;
    if(tok.type == CC_IDENTIFIER){
        err = cc_next_token(p, &tok);
        if(err) return err;
        name = tok.ident.ident;
    }
    err = cc_peek(p, &tok);
    if(err) return err;
    if(tok.type == CC_PUNCTUATOR && tok.punct.punct == '{'){
        err = cc_next_token(p, &tok);
        if(err) return err;
        void* existing = NULL;
        if(name){
            if(is_union){
                CcUnion* u = existing = cc_scope_lookup_union_tag(p->current, name, CC_SCOPE_NO_WALK);
                if(u && !u->is_incomplete) return cc_error(p, loc, "Redefinition of %s '%s'", is_union ? "union" : "struct", name->data);
                if(!u){
                    // Register incomplete tag before parsing body so self-references resolve.
                    u = Allocator_zalloc(cc_allocator(p), sizeof *u);
                    if(!u) return CC_OOM_ERROR;
                    *u = (CcUnion){.kind = CC_UNION, .name = name, .loc = loc, .is_incomplete = 1};
                    err = cc_scope_insert_union_tag(cc_allocator(p), p->current, name, u);
                    if(err) return CC_OOM_ERROR;
                    existing = u;
                    if(p->auto_typedef){
                        err = cc_scope_insert_typedef(cc_allocator(p), p->current, name, (CcQualType){.bits = (uintptr_t)u}, (SrcLoc){0});
                        if(err) return err;
                    }
                }
            }
            else {
                CcStruct* s = existing = cc_scope_lookup_struct_tag(p->current, name, CC_SCOPE_NO_WALK);
                if(s && !s->is_incomplete) return cc_error(p, loc, "Redefinition of %s '%s'", is_union ? "union" : "struct", name->data);
                if(!s){
                    // Register incomplete tag before parsing body so self-references resolve.
                    s = Allocator_zalloc(cc_allocator(p), sizeof *s);
                    if(!s) return CC_OOM_ERROR;
                    *s = (CcStruct){.kind = CC_STRUCT, .name = name, .loc = loc, .is_incomplete = 1};
                    err = cc_scope_insert_struct_tag(cc_allocator(p), p->current, name, s);
                    if(err) return CC_OOM_ERROR;
                    existing = s;
                    if(p->auto_typedef){
                        err = cc_scope_insert_typedef(cc_allocator(p), p->current, name, (CcQualType){.bits = (uintptr_t)s}, (SrcLoc){0});
                        if(err) return err;
                    }
                }
            }
        }
        else {
            // Anonymous struct/union: allocate incomplete placeholder so _Self
            // inside the body can refer to it.
            if(is_union){
                CcUnion* u = Allocator_zalloc(cc_allocator(p), sizeof *u);
                if(!u) return CC_OOM_ERROR;
                *u = (CcUnion){.kind = CC_UNION, .loc = loc, .is_incomplete = 1};
                existing = u;
            }
            else {
                CcStruct* s = Allocator_zalloc(cc_allocator(p), sizeof *s);
                if(!s) return CC_OOM_ERROR;
                *s = (CcStruct){.kind = CC_STRUCT, .loc = loc, .is_incomplete = 1};
                existing = s;
            }
        }
        CcQualType saved_tag_type = p->current_tag_type;
        p->current_tag_type = (CcQualType){.bits = (uintptr_t)existing};
        Marray(CcField) fields_arr = {0};
        for(;;){
            err = cc_peek(p, &tok);
            if(err) goto struct_err;
            if(tok.type == CC_PUNCTUATOR && tok.punct.punct == '}')
                break;
            if(tok.type == CC_KEYWORD && tok.kw.kw == CC_static_assert){
                err = cc_handle_static_assert(p);
                if(err) goto struct_err;
                continue;
            }
            CcAttributes member_attrs = {0};
            cc_clear_attributes(&p->attributes);
            CcDeclBase member_base = {0};
            err = cc_parse_declaration_specifier(p, &member_base);
            if(err) goto struct_err;
            member_attrs = p->attributes;
            cc_clear_attributes(&p->attributes);
            if(member_base.spec.sp_storagebits || member_base.spec.sp_typedef){
                err = cc_error(p, loc, "Storage class specifiers not allowed in struct/union members");
                goto struct_err;
            }
            if(!member_base.spec.bits && !member_base.type.bits){
                err = cc_error(p, tok.loc, "Expected type specifier in struct/union member");
                goto struct_err;
            }
            err = cc_resolve_specifiers(p, &member_base);
            if(err) goto struct_err;
            if(member_base.spec.sp_infer_type){
                err = cc_error(p, tok.loc, "Expected type specifier in struct/union member");
                goto struct_err;
            }
            err = cc_peek(p, &tok);
            if(err) goto struct_err;
            if(tok.type == CC_PUNCTUATOR && tok.punct.punct == ';'){
                err = cc_next_token(p, &tok); // consume ';'
                if(err) goto struct_err;
                // This is either an anonymous struct/union or a Plan9 extension
                // (or just a forward decl).
                CcTypeKind member_tk = ccqt_kind(member_base.type);
                if(member_tk == CC_STRUCT || member_tk == CC_UNION){
                    if(member_tk == CC_STRUCT && ccqt_as_struct(member_base.type)->is_incomplete)
                        continue;
                    if(member_tk == CC_UNION && ccqt_as_union(member_base.type)->is_incomplete)
                        continue;
                    err = cc_check_anon_member_duplicates(p, fields_arr.data, (uint32_t)fields_arr.count, member_base.type, tok.loc);
                    if(err) goto struct_err;
                    err = ma_push(CcField)(&fields_arr, cc_allocator(p), ((CcField){
                        .type = member_base.type,
                        .name = NULL, // anonymous
                        .loc = tok.loc,
                    }));
                    if(err){ err = CC_OOM_ERROR; goto struct_err; }
                    continue;
                }
                if(member_tk == CC_ENUM) // enum decl, it's fine
                    continue;
                cc_warn(p, tok.loc, "Declaration does not declare anything");
                continue;
            }
            // Parse member declarators: name [: bitwidth] [, name [: bitwidth]]* ;
            for(;;){
                Atom member_name = NULL;
                CcQualType member_type;
                uint64_t bitwidth = 0;
                _Bool is_bitfield = 0;
                // Check for anonymous bitfield: `: bitwidth`
                err = cc_peek(p, &tok);
                if(err) goto struct_err;
                if(tok.type == CC_PUNCTUATOR && tok.punct.punct == ':'){
                    // Anonymous bitfield
                    err = cc_next_token(p, &tok); // consume ':'
                    if(err) goto struct_err;
                    CcExpr* bw_expr = NULL;
                    err = cc_parse_assignment_expr(p, CC_CONSTEXPR_VALUE, &bw_expr, CCQT_NONE);
                    if(err) goto struct_err;
                    if(!bw_expr){
                        err = cc_error(p, tok.loc, "expected constant expression for bitfield width");
                        goto struct_err;
                    }
                    int64_t bw_i;
                    err = cc_eval_integer(&(CcEvalCtx){p}, bw_expr, &bw_i);
                    cc_release_expr(p, bw_expr);
                    if(err){
                        if(err == CC_NOT_CONSTANT_ERROR)
                            err = cc_error(p, tok.loc, "bitfield width must be a constant integral expression");
                        goto struct_err;
                    }
                    bitwidth = (uint64_t)bw_i;
                    member_type = member_base.type;
                    is_bitfield = 1;
                    if(member_type.is_atomic){
                        err = cc_error(p, tok.loc, "atomic bitfields are not supported");
                        goto struct_err;
                    }
                    if(!(ccqt_is_basic(member_type) && ccbt_is_integer(member_type.basic.kind))
                       && ccqt_kind(member_type) != CC_ENUM){
                        err = cc_error(p, tok.loc, "bitfield must have integer or enum type");
                        goto struct_err;
                    }
                    uint32_t type_size;
                    err = cc_sizeof_as_uint(p, member_type, tok.loc, &type_size);
                    if(err) goto struct_err;
                    uint32_t max_width = ccqt_is_bool(member_type) ? 1 : type_size * 8;
                    if(bitwidth > max_width){
                        err = cc_error(p, tok.loc, "bitfield width (%llu) exceeds size of type (%u bits)", (unsigned long long)bitwidth, max_width);
                        goto struct_err;
                    }
                }
                else {
                    // Parse declarator
                    CcQualType head = {0};
                    CcQualType* tail = &head;
                    CcParsedParams param_names = {0};
                    SrcLoc name_loc = {0};
                    err = cc_parse_declarator(p, &head, &tail, &member_name, &name_loc, &param_names);
                    if(err){
                        ma_cleanup(CcFuncParam)(&param_names.names, cc_allocator(p));
                        goto struct_err;
                    }
                    *tail = member_base.type;
                    member_type = cc_intern_qualtype(p, head);
                    // Method: member type is a function type (not pointer to function)
                    if(ccqt_kind(member_type) == CC_FUNCTION){
                        if(!member_name){
                            ma_cleanup(CcFuncParam)(&param_names.names, cc_allocator(p));
                            err = cc_error(p, tok.loc, "expected method name");
                            goto struct_err;
                        }
                        CcFunc* func = Allocator_zalloc(cc_allocator(p), sizeof *func);
                        if(!func){ ma_cleanup(CcFuncParam)(&param_names.names, cc_allocator(p)); err = CC_OOM_ERROR; goto struct_err; }
                        func->name = member_name;
                        func->type = ccqt_as_function(member_type);
                        func->loc = name_loc;
                        func->params.count = param_names.names.count;
                        func->params.data = param_names.names.data;
                        func->param_scope = param_names.scope;
                        // Check for method body
                        // If tail == &head, the function type came from the
                        // base type (e.g. a typedef), not the declarator.
                        // In that case, a body is not allowed.
                        err = cc_peek(p, &tok);
                        if(err) goto struct_err;
                        if(tail == &head && tok.type == CC_PUNCTUATOR && tok.punct.punct == '{'){
                            err = cc_error(p, tok.loc, "cannot define method with typedef function type");
                            goto struct_err;
                        }
                        if(tok.type == CC_PUNCTUATOR && tok.punct.punct == '{'){
                            err = cc_next_token(p, &tok); // consume '{'
                            if(err) goto struct_err;
                            Marray(CcToken)* body_tokens = cc_get_scratch(p);
                            if(!body_tokens){ err = CC_OOM_ERROR; goto struct_err; }
                            int depth = 1;
                            while(depth > 0){
                                CcToken t;
                                err = cc_next_token(p, &t);
                                if(err) goto struct_err;
                                if(t.type == CC_EOF){
                                    err = cc_error(p, tok.loc, "Unexpected EOF in method body");
                                    goto struct_err;
                                }
                                if(t.type == CC_PUNCTUATOR){
                                    if(t.punct.punct == '{') depth++;
                                    else if(t.punct.punct == '}') depth--;
                                }
                                if(depth > 0){
                                    err = ma_push(CcToken)(body_tokens, cc_allocator(p), t);
                                    if(err) goto struct_err;
                                }
                            }
                            func->_Self_type = p->current_tag_type;
                            func->tokens = body_tokens;
                            func->defined = 1;
                            if(p->current_func){
                                func->enclosing = p->current_func;
                                err = pa_push(&p->current->deferred_methods, cc_allocator(p), func);
                                if(err){ err = CC_OOM_ERROR; goto struct_err; }
                            }
                        }
                        // Parse optional attributes after method
                        err = cc_parse_attributes(p, &member_attrs);
                        if(err) goto struct_err;
                        cc_clear_attributes(&p->attributes);
                        if(cc_has_field(p, fields_arr.data, (uint32_t)fields_arr.count, func->name)){
                            err = cc_error(p, tok.loc, "duplicate member '%s'", func->name->data);
                            goto struct_err;
                        }
                        err = ma_push(CcField)(&fields_arr, cc_allocator(p), ((CcField){
                            .type = member_type,
                            .method = func,
                            .is_method = 1,
                            .loc = tok.loc,
                        }));
                        if(err){ err = CC_OOM_ERROR; goto struct_err; }
                        // Method definitions with body don't need ';'
                        if(func->defined){
                            err = cc_peek(p, &tok);
                            if(err) goto struct_err;
                            // Allow optional ';' after method body
                            if(tok.type == CC_PUNCTUATOR && tok.punct.punct == ';'){
                                err = cc_next_token(p, &tok);
                                if(err) goto struct_err;
                            }
                            goto next_member;
                        }
                        break; // fall through to ';' expect
                    }
                    ma_cleanup(CcFuncParam)(&param_names.names, cc_allocator(p));

                    // Check for bitfield
                    err = cc_peek(p, &tok);
                    if(err) goto struct_err;
                    if(tok.type == CC_PUNCTUATOR && tok.punct.punct == ':'){
                        err = cc_next_token(p, &tok); // consume ':'
                        if(err) goto struct_err;
                        CcExpr* bw_expr = NULL;
                        err = cc_parse_assignment_expr(p, CC_CONSTEXPR_VALUE, &bw_expr, CCQT_NONE);
                        if(err) goto struct_err;
                        if(!bw_expr){
                            err = cc_error(p, tok.loc, "expected constant expression for bitfield width");
                            goto struct_err;
                        }
                        int64_t bw_i;
                        err = cc_eval_integer(&(CcEvalCtx){p}, bw_expr, &bw_i);
                        cc_release_expr(p, bw_expr);
                        if(err){
                            if(err == CC_NOT_CONSTANT_ERROR)
                                err = cc_error(p, tok.loc, "bitfield width must be a constant integral expression");
                            goto struct_err;
                        }
                        bitwidth = (uint64_t)bw_i;
                        is_bitfield = 1;
                        if(member_type.is_atomic){
                            err = cc_error(p, tok.loc, "atomic bitfields are not supported");
                            goto struct_err;
                        }
                        if(!(ccqt_is_basic(member_type) && ccbt_is_integer(member_type.basic.kind))
                           && ccqt_kind(member_type) != CC_ENUM){
                            err = cc_error(p, tok.loc, "bitfield must have integer or enum type");
                            goto struct_err;
                        }
                        uint32_t type_size;
                        err = cc_sizeof_as_uint(p, member_type, tok.loc, &type_size);
                        if(err) goto struct_err;
                        if(bitwidth == 0){
                            err = cc_error(p, tok.loc, "named bitfield '%s' cannot have zero width", member_name->data);
                            goto struct_err;
                        }
                        uint32_t max_width = ccqt_is_bool(member_type) ? 1 : type_size * 8;
                        if(bitwidth > max_width){
                            err = cc_error(p, tok.loc, "bitfield width (%llu) exceeds size of type (%u bits)", (unsigned long long)bitwidth, max_width);
                            goto struct_err;
                        }
                    }
                }
                // Parse optional attributes after the declarator
                err = cc_parse_attributes(p, &member_attrs);
                if(err) goto struct_err;
                cc_clear_attributes(&p->attributes);
                // Create field
                if(member_name && cc_has_field(p, fields_arr.data, (uint32_t)fields_arr.count, member_name)){
                    err = cc_error(p, tok.loc, "duplicate member '%s'", member_name->data);
                    goto struct_err;
                }
                err = ma_push(CcField)(&fields_arr, cc_allocator(p), ((CcField){
                    .type = member_type,
                    .name = member_name,
                    .bitwidth = (uint32_t)bitwidth,
                    .is_bitfield = is_bitfield,
                    .alignment = member_base.alignment,
                    .loc = tok.loc,
                }));
                if(err){ err = CC_OOM_ERROR; goto struct_err; }
                // Check for comma or semicolon
                err = cc_peek(p, &tok);
                if(err) goto struct_err;
                if(tok.type == CC_PUNCTUATOR && tok.punct.punct == ','){
                    err = cc_next_token(p, &tok); // consume ','
                    if(err) goto struct_err;
                    continue;
                }
                break;
            }
            err = cc_expect_punct(p, CC_semi);
            if(err) goto struct_err;
            next_member:;
        }
        err = cc_expect_punct(p, CC_rbrace);
        if(err) goto struct_err;
        p->current_tag_type = saved_tag_type;
        // Parse optional trailing attributes
        err = cc_parse_attributes(p, &attrs);
        if(err) goto struct_err;
        // Finalize
        err = ma_shrink_to_size(CcField)(&fields_arr, cc_allocator(p));
        if(err) return err;
        uint32_t field_count = (uint32_t)fields_arr.count;
        CcField* flat_fields = fields_arr.data;
        if(!is_union){
            CcStruct* s = (CcStruct*)existing;
            if(!s){
                s = Allocator_zalloc(cc_allocator(p), sizeof *s);
                if(!s) return CC_OOM_ERROR;
            }
            *s = (CcStruct){
                .kind = CC_STRUCT,
                .name = name,
                .loc = loc,
                .field_count = field_count,
                .fields = flat_fields,
                .packed = attrs.packed,
            };
            if(attrs.has_aligned)
                s->alignment = attrs.aligned;
            err = cc_compute_struct_layout(p, s, p->pragma_pack);
            if(err) return err;
            if(name && !existing){
                err = cc_scope_insert_struct_tag(cc_allocator(p), p->current, name, s);
                if(err) return CC_OOM_ERROR;
            }
            *base_type = (CcQualType){.bits = (uintptr_t)s};
        }
        else {
            CcUnion* u = (CcUnion*)existing;
            if(!u){
                u = Allocator_zalloc(cc_allocator(p), sizeof *u);
                if(!u) return CC_OOM_ERROR;
            }
            *u = (CcUnion){
                .kind = CC_UNION,
                .name = name,
                .loc = loc,
                .field_count = field_count,
                .fields = flat_fields,
            };
            if(attrs.has_aligned)
                u->alignment = attrs.aligned;
            err = cc_compute_union_layout(p, u, p->pragma_pack);
            if(err) return err;
            if(name && !existing){
                err = cc_scope_insert_union_tag(cc_allocator(p), p->current, name, u);
                if(err) return CC_OOM_ERROR;
            }
            *base_type = (CcQualType){.bits = (uintptr_t)u};
        }
        return 0;

        struct_err:
        p->current_tag_type = saved_tag_type;
        ma_cleanup(CcField)(&fields_arr, cc_allocator(p));
        return err;
    }

    // No body — just a reference: struct/union name
    if(!name)
        return cc_error(p, loc, "expected %s name or '{'", !is_union ? "struct" : "union");

    if(!is_union){
        CcStruct* s = cc_scope_lookup_struct_tag(p->current, name, CC_SCOPE_WALK_CHAIN);
        if(!s){
            // Forward declaration — create incomplete struct
            s = Allocator_zalloc(cc_allocator(p), sizeof *s);
            if(!s) return CC_OOM_ERROR;
            *s = (CcStruct){
                .kind = CC_STRUCT,
                .name = name,
                .loc = loc,
                .is_incomplete = 1,
            };
            err = cc_scope_insert_struct_tag(cc_allocator(p), p->current, name, s);
            if(err) return CC_OOM_ERROR;
        }
        *base_type = (CcQualType){.bits = (uintptr_t)s};
    }
    else {
        CcUnion* u = cc_scope_lookup_union_tag(p->current, name, CC_SCOPE_WALK_CHAIN);
        if(!u){
            u = Allocator_zalloc(cc_allocator(p), sizeof *u);
            if(!u) return CC_OOM_ERROR;
            *u = (CcUnion){
                .kind = CC_UNION,
                .name = name,
                .loc = loc,
                .is_incomplete = 1,
            };
            err = cc_scope_insert_union_tag(cc_allocator(p), p->current, name, u);
            if(err) return CC_OOM_ERROR;
        }
        *base_type = (CcQualType){.bits = (uintptr_t)u};
    }
    return 0;
}

static
_Bool
cc_enum_negative(CcParser* p, CcQualType type, CiUint128 bits){
    return !ccqt_is_unsigned(type, !cc_target(p)->char_is_signed)
        && (ci_uint128_hi(bits) >> 63);
}

static
_Bool
cc_enum_value_fits(CcParser* p, CcQualType type, CiUint128 bits, _Bool negative){
    while(ccqt_kind(type) == CC_ENUM) type = ccqt_as_enum(type)->underlying;
    if(ccqt_bt_eq(type, CCBT_bool))
        return !negative && ci_uint128_le(bits, ci_uint128_from_uint64(1));
    uint32_t width = cc_target(p)->sizeof_[type.basic.kind] * 8;
    _Bool unsigned_ = ccqt_is_unsigned(type, !cc_target(p)->char_is_signed);
    CiUint128 one = ci_uint128_from_uint64(1);
    if(negative){
        if(unsigned_) return 0;
        CiUint128 min = ci_uint128_sub(ci_uint128_from_uint64(0), ci_uint128_shl(one, width-1));
        return ci_uint128_ge(bits, min);
    }
    CiUint128 max = unsigned_ && width == 128 ? ci_uint128_not(ci_uint128_from_uint64(0))
        : ci_uint128_sub(ci_uint128_shl(one, width - !unsigned_), one);
    return ci_uint128_le(bits, max);
}

static const CcBasicTypeKind cc_enum_signed_types[] = {
    CCBT_signed_char, CCBT_short, CCBT_int, CCBT_long, CCBT_long_long, CCBT_int128,
};
static const CcBasicTypeKind cc_enum_unsigned_types[] = {
    CCBT_unsigned_char, CCBT_unsigned_short, CCBT_unsigned, CCBT_unsigned_long,
    CCBT_unsigned_long_long, CCBT_unsigned_int128,
};

static
int
cc_finalize_enum(CcParser* p, CcEnum* e, _Bool fixed, _Bool packed){
    if(fixed) return 0;
    _Bool all_int = 1, negative = 0;
    for(size_t i = 0; i < e->enumerator_count; i++){
        CcEnumerator* en = e->enumerators[i];
        _Bool neg = cc_enum_negative(p, en->type, en->value);
        negative |= neg;
        all_int &= cc_enum_value_fits(p, ccqt_basic(CCBT_int), en->value, neg);
    }
    CcQualType underlying = CCQT_NONE;
    if(all_int && !packed) underlying = ccqt_basic(CCBT_int);
    else {
        const CcBasicTypeKind* types = negative ? cc_enum_signed_types : cc_enum_unsigned_types;
        uint32_t min_size = packed ? 1 : cc_target(p)->sizeof_[CCBT_int];
        for(size_t i = 0; i < sizeof cc_enum_signed_types / sizeof *cc_enum_signed_types; i++){
            CcQualType candidate = ccqt_basic(types[i]);
            if(cc_target(p)->sizeof_[types[i]] < min_size) continue;
            _Bool fits = 1;
            for(size_t j = 0; j < e->enumerator_count; j++){
                CcEnumerator* en = e->enumerators[j];
                if(!cc_enum_value_fits(p, candidate, en->value, cc_enum_negative(p, en->type, en->value))){
                    fits = 0;
                    break;
                }
            }
            if(fits){ underlying = candidate; break; }
        }
    }
    if(!underlying.bits)
        return cc_error(p, e->loc, "no integer type can represent all enumerator values");
    e->underlying = underlying;
    CcQualType member_type = all_int ? ccqt_basic(CCBT_int) : (CcQualType){.bits=(uintptr_t)e};
    for(size_t i = 0; i < e->enumerator_count; i++) e->enumerators[i]->type = member_type;
    return 0;
}

static
int
cc_parse_enum(CcParser* p, SrcLoc loc, CcQualType* base_type){
    int err = 0;
    CcToken tok;
    Atom name = NULL;
    CcQualType underlying = ccqt_basic(CCBT_int);
    _Bool has_fixed_underlying = 0;
    // Optional attributes after 'enum'
    CcAttributes enum_attrs = {0};
    err = cc_parse_attributes(p, &enum_attrs);
    if(err) return err;
    // Optional tag name
    err = cc_peek(p, &tok);
    if(err) return err;
    if(tok.type == CC_IDENTIFIER){
        err = cc_next_token(p, &tok);
        if(err) return err;
        name = tok.ident.ident;
    }
    err = cc_parse_attributes(p, &enum_attrs);
    if(err) return err;
    // Optional fixed underlying type: enum name : int { ... }
    err = cc_peek(p, &tok);
    if(err) return err;
    if(tok.type == CC_PUNCTUATOR && tok.punct.punct == ':'){
        err = cc_next_token(p, &tok); // consume ':'
        if(err) return err;
        has_fixed_underlying = 1;
        CcDeclBase ub = {0};
        err = cc_parse_declaration_specifier(p, &ub);
        if(err) return err;
        CcSpecifier type_spec = ub.spec;
        type_spec.sp_const = type_spec.sp_volatile = type_spec.sp_atomic = 0;
        if(type_spec.sp_typebits != type_spec.bits)
            return cc_error(p, loc, "Underlying type does not allow non-type specifiers");
        if(ub.spec.sp_infer_type)
            return cc_error(p, loc, "__auto_type not allowed as underlying type of enum");
        if(!ub.spec.bits && !ub.type.bits)
            return cc_error(p, loc, "Expected type specifier for enum underlying type");
        err = cc_resolve_specifiers(p, &ub);
        if(err) return err;
        if(!ccqt_is_basic(ub.type) || !ccbt_is_integer(ub.type.basic.kind))
            return cc_error(p, loc, "enum underlying type must be an integer type");
        underlying = (CcQualType){.unqual=ub.type.unqual};
    }
    // Check for enum body
    err = cc_peek(p, &tok);
    if(err) return err;
    if(tok.type == CC_PUNCTUATOR && tok.punct.punct == '{'){
        err = cc_next_token(p, &tok); // consume '{'
        if(err) return err;
        CcEnum* e = NULL;
        if(name){
            CcEnum* existing = cc_scope_lookup_enum_tag(p->current, name, CC_SCOPE_NO_WALK);
            if(existing){
                if(existing->has_definition)
                    return cc_error(p, loc, "Redefinition of enum '%s'", name->data);
                if(existing->has_fixed_underlying && has_fixed_underlying && existing->underlying.bits != underlying.bits)
                    return cc_error(p, loc, "Redefinition of enum '%s' with differing underlying types", name->data);
                if(existing->has_fixed_underlying){
                    underlying = existing->underlying;
                    has_fixed_underlying = 1;
                }
                e = existing;
                e->loc = loc;
                e->underlying = underlying;
            }
        }
        if(!e){
            e = Allocator_zalloc(cc_allocator(p), sizeof *e);
            if(!e) return CC_OOM_ERROR;
            *e = (CcEnum){
                .kind = CC_ENUM,
                .name = name,
                .loc = loc,
                .underlying = underlying,
            };
            if(name){
                err = cc_scope_insert_enum_tag(cc_allocator(p), p->current, name, e);
                if(err)  return CC_OOM_ERROR;
            }
        }
        e->has_fixed_underlying = has_fixed_underlying;
        e->has_definition = 1;
        e->is_incomplete = !has_fixed_underlying;
        CcQualType enum_type = {.bits = (uintptr_t)e};
        // Parse enumerator list
        Parray(CcEnumerator) enumerators = {0};
        CiUint128 next_value = ci_uint128_from_uint64(0);
        CcQualType next_type = ccqt_basic(CCBT_int);
        _Bool next_overflow = 0;
        for(;;){
            err = cc_peek(p, &tok);
            if(err) goto enum_err;
            if(tok.type == CC_PUNCTUATOR && tok.punct.punct == '}')
                break;
            err = cc_next_token(p, &tok);
            if(err) goto enum_err;
            if(tok.type != CC_IDENTIFIER){
                err = cc_error(p, tok.loc, "expected enumerator name");
                goto enum_err;
            }
            Atom ename = tok.ident.ident;
            CcSymbol sym;
            _Bool found = cc_scope_lookup_symbol(p->current, ename, CC_SCOPE_NO_WALK, &sym);
            if(found){
                if(sym.kind == CC_SYM_ENUMERATOR)
                    err = cc_error(p, tok.loc, "Redefinition of enumerator '%s'", ename->data);
                else
                    err = cc_error(p, tok.loc, "Redefinition of '%s' as a different kind of symbol", ename->data);
                goto enum_err;
            }
            SrcLoc eloc = tok.loc;
            // Optional attributes after enumerator name
            {
                CcAttributes enumerator_attrs = {0};
                err = cc_parse_attributes(p, &enumerator_attrs);
                if(err) goto enum_err;
            }
            // Optional = constant-expression
            err = cc_peek(p, &tok);
            if(err) goto enum_err;
            if(tok.type == CC_PUNCTUATOR && tok.punct.punct == '='){
                err = cc_next_token(p, &tok); // consume '='
                if(err) goto enum_err;
                CcExpr* expr = NULL;
                err = cc_parse_assignment_expr(p, CC_CONSTEXPR_VALUE, &expr, CCQT_NONE);
                if(err) goto enum_err;
                if(!expr){
                    err = cc_error(p, tok.loc, "expected constant expression");
                    goto enum_err;
                }
                CcExpr* value;
                err = cc_eval_expr(&(CcEvalCtx){p}, expr, &value);
                cc_release_expr(p, expr);
                if(!err){
                    if(!ccqt_is_integer(value->type)) err = CC_NOT_CONSTANT_ERROR;
                    else {
                        next_value = cc_eval_u128(p, value);
                        next_type = value->type;
                        if(!has_fixed_underlying && cc_enum_value_fits(p, ccqt_basic(CCBT_int), next_value,
                            cc_enum_negative(p, next_type, next_value))) next_type = ccqt_basic(CCBT_int);
                    }
                    cc_release_expr(p, value);
                }
                if(err == CC_OVERFLOW_ERROR) err = CC_NOT_CONSTANT_ERROR;
                if(err){
                    if(err == CC_NOT_CONSTANT_ERROR)
                        err = cc_error(p, tok.loc, "enumerator value must be a constant integer expression");
                    goto enum_err;
                }
                next_overflow = 0;
            }
            else if(next_overflow){
                err = cc_error(p, eloc, has_fixed_underlying
                    ? "implicit enumerator value exceeds underlying type range"
                    : "implicit enumerator value exceeds supported integer range");
                goto enum_err;
            }
            _Bool negative = cc_enum_negative(p, next_type, next_value);
            if(has_fixed_underlying && !cc_enum_value_fits(p, underlying, next_value, negative)){
                err = cc_error(p, eloc, ccqt_bt_eq(underlying, CCBT_bool)
                    ? "enumerator value is out of range for bool underlying type"
                    : "enumerator value is out of range for underlying type");
                goto enum_err;
            }
            if(has_fixed_underlying) next_type = enum_type;
            CcEnumerator* enumerator = Allocator_zalloc(cc_allocator(p), sizeof *enumerator);
            if(!enumerator){ err = CC_OOM_ERROR; goto enum_err; }
            *enumerator = (CcEnumerator){
                .name = ename,
                .value = next_value,
                .type = next_type,
                .loc = eloc,
            };
            err = cc_scope_insert_enumerator(cc_allocator(p), p->current, ename, enumerator);
            if(err){ err = CC_OOM_ERROR; goto enum_err; }
            err = pa_push(&enumerators, cc_allocator(p), enumerator);
            if(err){ err = CC_OOM_ERROR; goto enum_err; }
            next_overflow = !negative && ci_uint128_eq(next_value, ci_uint128_not(ci_uint128_from_uint64(0)));
            next_value = ci_uint128_add(next_value, ci_uint128_from_uint64(1));
            _Bool next_negative = negative && ci_uint128_nonzero(next_value);
            if(!next_overflow && !cc_enum_value_fits(p, next_type, next_value, next_negative)){
                next_overflow = 1;
                if(!has_fixed_underlying){
                    _Bool unsigned_ = ccqt_is_unsigned(next_type, !cc_target(p)->char_is_signed);
                    const CcBasicTypeKind* types = unsigned_ ? cc_enum_unsigned_types : cc_enum_signed_types;
                    for(size_t i = 0; i < sizeof cc_enum_signed_types / sizeof *cc_enum_signed_types; i++){
                        CcQualType candidate = ccqt_basic(types[i]);
                        if(cc_enum_value_fits(p, candidate, next_value, next_negative)){
                            next_type = candidate;
                            next_overflow = 0;
                            break;
                        }
                    }
                }
            }
            err = cc_peek(p, &tok);
            if(err) goto enum_err;
            if(tok.type == CC_PUNCTUATOR && tok.punct.punct == ','){
                err = cc_next_token(p, &tok); // consume ','
                if(err) goto enum_err;
                continue;
            }
            break;
        }
        err = cc_expect_punct(p, CC_rbrace);
        if(err) goto enum_err;
        err = cc_parse_attributes(p, &enum_attrs);
        if(err) goto enum_err;
        // Finalize enumerators
        err = pa_shrink_to_size(&enumerators, cc_allocator(p));
        if(err) goto enum_err;
        e->enumerators = (CcEnumerator**)enumerators.data;
        e->enumerator_count = enumerators.count;
        err = cc_finalize_enum(p, e, has_fixed_underlying, enum_attrs.packed);
        if(err) goto enum_err;
        e->is_incomplete = 0;
        *base_type = enum_type;
        return 0;

        enum_err:
        pa_cleanup(&enumerators, cc_allocator(p));
        return err;
    }

    // No body — just a reference: enum name
    if(!name)
        return cc_error(p, loc, "expected enum name or '{'");
    CcEnum* e = cc_scope_lookup_enum_tag(p->current, name,
        has_fixed_underlying ? CC_SCOPE_NO_WALK : CC_SCOPE_WALK_CHAIN);
    if(e && has_fixed_underlying){
        if(e->has_fixed_underlying && e->underlying.bits != underlying.bits)
            return cc_error(p, loc, "Redefinition of enum '%s' with differing underlying types", name->data);
        if(e->has_definition && !e->has_fixed_underlying)
            return cc_error(p, loc, "Redefinition of enum '%s' with fixed underlying type", name->data);
        e->underlying = underlying;
        e->has_fixed_underlying = 1;
        e->is_incomplete = 0;
    }
    if(!e){
        // A fixed underlying type completes the layout even without a body.
        e = Allocator_zalloc(cc_allocator(p), sizeof *e);
        if(!e) return CC_OOM_ERROR;
        *e = (CcEnum){
            .kind = CC_ENUM,
            .name = name,
            .loc = loc,
            .underlying = underlying,
            .is_incomplete = !has_fixed_underlying,
            .has_fixed_underlying = has_fixed_underlying,
        };
        err = cc_scope_insert_enum_tag(cc_allocator(p), p->current, name, e);
        if(err) return CC_OOM_ERROR;
    }
    *base_type = (CcQualType){.bits = (uintptr_t)e};
    return 0;
}

static
int
cc_parse_declaration_specifier(CcParser* p, CcDeclBase* base){
    CcSpecifier* spec = &base->spec;
    CcQualType* base_type = &base->type;
    if(base_type->bits) return cc_unreachable(p, base->loc, "parsing decl specifier with base type set");
    if(spec->bits) return cc_unreachable(p, base->loc, "parsing decl specifier with spec set");
    int err = 0;
    CcToken tok;
    for(int i = 0; ; i++){
        err = cc_next_token(p, &tok);
        if(err) return err;
        if(i == 0) base->loc = tok.loc;
        switch(tok.type){
            case CC_KEYWORD:
                switch(tok.kw.kw){
                    case CC_else:
                    case CC_asm:
                    case CC_true:
                    case CC_do:
                    case CC_if:
                    case CC_case:
                    case CC_goto:
                    case CC_for:
                    case CC_false:
                    case CC_break:
                    case CC_while:
                    case CC_return:
                    case CC_sizeof:
                    case CC_switch:
                    case CC_alignof:
                    case CC_default:
                    case CC_nullptr:
                    case CC_continue:
                    case CC__Generic:
                    case CC__Countof:
                    case CC_static_assert:
                        if(i == 0) return cc_unget(p, &tok);
                        return cc_error(p, tok.loc, "Unexpected keyword when parsing declaration");
                    case CC_int:
                        if(base_type->bits)
                            return cc_error(p, tok.loc, "Second type in declaration");
                        if(spec->sp_int)
                            return cc_error(p, tok.loc, "Duplicate int in declaration");
                        if(spec->sp_char)
                            return cc_error(p, tok.loc, "int after char");
                        if(spec->sp___auto_type)
                            return cc_error(p, tok.loc, "int after __auto_type");
                        if(spec->sp_int128)
                            return cc_error(p, tok.loc, "int after __int128");
                        spec->sp_int = 1;
                        continue;
                    case CC_long:
                        if(base_type->bits)
                            return cc_error(p, tok.loc, "Second type in declaration");
                        if(spec->sp_long > 2)
                            return cc_error(p, tok.loc, "Duplicate long after long long long in declaration");
                        if(spec->sp_char)
                            return cc_error(p, tok.loc, "long after char");
                        if(spec->sp_short)
                            return cc_error(p, tok.loc, "long after short");
                        if(spec->sp___auto_type)
                            return cc_error(p, tok.loc, "long after __auto_type");
                        if(spec->sp_int128)
                            return cc_error(p, tok.loc, "long after __int128");
                        spec->sp_long++;
                        continue;
                    case CC_char:
                        if(base_type->bits)
                            return cc_error(p, tok.loc, "Second type in declaration");
                        if(spec->sp_char)
                            return cc_error(p, tok.loc, "Duplicate char in declaration");
                        if(spec->sp_long)
                            return cc_error(p, tok.loc, "char after long");
                        if(spec->sp_short)
                            return cc_error(p, tok.loc, "char after short");
                        if(spec->sp___auto_type)
                            return cc_error(p, tok.loc, "char after __auto_type");
                        if(spec->sp_int)
                            return cc_error(p, tok.loc, "char after int");
                        if(spec->sp_int128)
                            return cc_error(p, tok.loc, "char after __int128");
                        spec->sp_char = 1;
                        continue;
                    case CC___auto_type:
                        if(base_type->bits)
                            return cc_error(p, tok.loc, "Second type in declaration");
                        if(spec->sp_typebits)
                            return cc_error(p, tok.loc, "Second type in declaration");
                        spec->sp___auto_type = 1;
                        continue;
                    case CC___int128:
                        if(base_type->bits)
                            return cc_error(p, tok.loc, "Second type in declaration");
                        if(spec->sp_int128)
                            return cc_error(p, tok.loc, "Duplicate __int128 in declaration");
                        if(spec->sp_char)
                            return cc_error(p, tok.loc, "__int128 after char");
                        if(spec->sp_short)
                            return cc_error(p, tok.loc, "__int128 after short");
                        if(spec->sp_long)
                            return cc_error(p, tok.loc, "__int128 after long");
                        if(spec->sp_int)
                            return cc_error(p, tok.loc, "__int128 after int");
                        if(spec->sp___auto_type)
                            return cc_error(p, tok.loc, "__int128 after __auto_type");
                        spec->sp_int128 = 1;
                        continue;
                    case CC_auto:
                        if(spec->sp_typedef)
                            return cc_error(p, tok.loc, "auto after typedef");
                        spec->sp_auto = 1;
                        continue;
                    case CC__Any:
                        if(base_type->bits || spec->sp_typebits){
                            Atom a = AT_atomize(p->cpp.at, "_Any", 4);
                            if(!a) return CC_OOM_ERROR;
                            tok = (CcToken){.ident = {.type = CC_IDENTIFIER, .ident = a, .loc = tok.loc}};
                            return cc_unget(p, &tok);
                        }
                        *base_type = ccqt_basic(CCBT__Any);
                        continue;
                    case CC__Type:
                        if(base_type->bits || spec->sp_typebits){
                            Atom a = AT_atomize(p->cpp.at, "_Type", 5);
                            if(!a) return CC_OOM_ERROR;
                            tok = (CcToken){.ident = {.type = CC_IDENTIFIER, .ident = a, .loc = tok.loc}};
                            return cc_unget(p, &tok);
                        }
                        *base_type = ccqt_basic(CCBT__Type);
                        continue;
                    case CC__Self:
                        if(base_type->bits || spec->sp_typebits || !p->current_tag_type.bits){
                            Atom a = AT_atomize(p->cpp.at, "_Self", 5);
                            if(!a) return CC_OOM_ERROR;
                            tok = (CcToken){.ident = {.type = CC_IDENTIFIER, .ident = a, .loc = tok.loc}};
                            return cc_unget(p, &tok);
                        }
                        *base_type = p->current_tag_type;
                        continue;
                    case CC_bool:
                        if(base_type->bits)
                            return cc_error(p, tok.loc, "Second type in declaration");
                        if(spec->sp_typebits)
                            return cc_error(p, tok.loc, "Second type in declaration");
                        *base_type = ccqt_basic(CCBT_bool);
                        continue;
                    case CC_enum: {
                        if(base_type->bits)
                            return cc_error(p, tok.loc, "Second type in declaration");
                        if(spec->sp_typebits)
                            return cc_error(p, tok.loc, "enum with other type specifiers");
                        err = cc_parse_enum(p, tok.loc, base_type);
                        if(err) return err;
                        continue;
                    }
                    case CC_void:
                        if(base_type->bits)
                            return cc_error(p, tok.loc, "Second type in declaration");
                        if(spec->sp_typebits)
                            return cc_error(p, tok.loc, "Second type in declaration");
                        *base_type = ccqt_basic(CCBT_void);
                        continue;
                    case CC_float:
                        if(base_type->bits)
                            return cc_error(p, tok.loc, "Second type in declaration");
                        if(spec->sp_typebits)
                            return cc_error(p, tok.loc, "Second type in declaration");
                        *base_type = ccqt_basic(CCBT_float);
                        continue;
                    case CC_const:
                        spec->sp_const = 1;
                        continue;
                    case CC_short:
                        if(base_type->bits)
                            return cc_error(p, tok.loc, "Second type in declaration");
                        if(spec->sp_short)
                            return cc_error(p, tok.loc, "Duplicate short in declaration");
                        if(spec->sp_long)
                            return cc_error(p, tok.loc, "short after long");
                        if(spec->sp_char)
                            return cc_error(p, tok.loc, "short after char");
                        if(spec->sp___auto_type)
                            return cc_error(p, tok.loc, "short after __auto_type");
                        if(spec->sp_int128)
                            return cc_error(p, tok.loc, "short after __int128");
                        spec->sp_short = 1;
                        continue;
                    case CC_union: {
                        if(base_type->bits)
                            return cc_error(p, tok.loc, "Second type in declaration");
                        if(spec->sp_typebits)
                            return cc_error(p, tok.loc, "union with other type specifiers");
                        err = cc_parse_struct_or_union(p, tok.loc, 1, base_type);
                        if(err) return err;
                        continue;
                    }
                    case CC_double:
                        if(base_type->bits)
                            return cc_error(p, tok.loc, "Second type in declaration");
                        {
                            uint32_t count = popcount_32(spec->sp_typebits);
                            if(count > 1 || (count == 1 && spec->sp_long != 1))
                                return cc_error(p, tok.loc, "double with other types");
                        }
                        *base_type = ccqt_basic(CCBT_double);
                        continue;
                    case CC_extern:
                        if(spec->sp_static)
                            return cc_error(p, tok.loc, "extern after static");
                        if(spec->sp_typedef)
                            return cc_error(p, tok.loc, "extern after typedef");
                        if(spec->sp_register)
                            return cc_error(p, tok.loc, "extern after register");
                        if(spec->sp_constexpr)
                            return cc_error(p, tok.loc, "extern after constexpr");
                        spec->sp_extern = 1;
                        continue;
                    case CC_inline:
                        if(spec->sp_typedef)
                            return cc_error(p, tok.loc, "inline after typedef");
                        spec->sp_inline = 1;
                        continue;
                    case CC_signed:
                        if(base_type->bits)
                            return cc_error(p, tok.loc, "Second type in declaration");
                        if(spec->sp_unsigned)
                            return cc_error(p, tok.loc, "signed after unsigned");
                        if(spec->sp___auto_type)
                            return cc_error(p, tok.loc, "signed after __auto_type");
                        spec->sp_signed = 1;
                        continue;
                    case CC_static:
                        if(spec->sp_extern)
                            return cc_error(p, tok.loc, "static after extern");
                        if(spec->sp_typedef)
                            return cc_error(p, tok.loc, "static after typedef");
                        if(spec->sp_register)
                            return cc_error(p, tok.loc, "static after register");
                        spec->sp_static = 1;
                        continue;
                    case CC_struct: {
                        if(base_type->bits)
                            return cc_error(p, tok.loc, "Second type in declaration");
                        if(spec->sp_typebits)
                            return cc_error(p, tok.loc, "struct with other type specifiers");
                        err = cc_parse_struct_or_union(p, tok.loc, 0, base_type);
                        if(err) return err;
                        continue;
                    }
                    case CC_typeof:
                        goto do_typeof;
                    case CC_alignas: {
                        err = cc_expect_punct(p, CC_lparen);
                        if(err) return err;
                        CcToken peek;
                        err = cc_peek(p, &peek);
                        if(err) return err;
                        uint32_t align_val;
                        if(cc_is_type_start(p, &peek)){
                            CcQualType align_type;
                            err = cc_parse_type_name(p, &align_type, NULL);
                            if(err) return err;
                            switch(ccqt_kind(align_type)){
                                case CC_BASIC:
                                    align_val = cc_target(p)->alignof_[align_type.basic.kind];
                                    break;
                                case CC_STRUCT: {
                                    CcStruct* s = ccqt_as_struct(align_type);
                                    if(s->is_incomplete)
                                        return cc_error(p, tok.loc, "_Alignas applied to incomplete struct type");
                                    align_val = s->alignment;
                                    break;
                                }
                                case CC_UNION: {
                                    CcUnion* u = ccqt_as_union(align_type);
                                    if(u->is_incomplete)
                                        return cc_error(p, tok.loc, "_Alignas applied to incomplete union type");
                                    align_val = u->alignment;
                                    break;
                                }
                                case CC_ARRAY: {
                                    CcArray* arr = ccqt_as_array(align_type);
                                    if(arr->is_vector){
                                        align_val = arr->vector_size > cc_target(p)->max_align ? cc_target(p)->max_align : arr->vector_size;
                                    }
                                    else {
                                        CcQualType elem = arr->element;
                                        if(ccqt_is_basic(elem))
                                            align_val = cc_target(p)->alignof_[elem.basic.kind];
                                        else
                                            return cc_error(p, tok.loc, "_Alignas with complex array element type not yet supported");
                                    }
                                    break;
                                }
                                case CC_SLICE:
                                case CC_BLOCK_POINTER:
                                case CC_POINTER:
                                    align_val = cc_target(p)->alignof_[CCBT_nullptr_t];
                                    break;
                                case CC_ENUM:
                                case CC_FUNCTION:
                                    return cc_error(p, tok.loc, "_Alignas with this type not yet supported");
                                DRP_CASES_EXHAUSTED;
                            }
                        }
                        else {
                            CcExpr* expr = NULL;
                            err = cc_parse_assignment_expr(p, CC_CONSTEXPR_VALUE, &expr, CCQT_NONE);
                            if(err) return err;
                            if(!expr)
                                return cc_error(p, tok.loc, "expected expression in _Alignas");
                            int64_t av;
                            err = cc_eval_integer(&(CcEvalCtx){p}, expr, &av);
                            cc_release_expr(p, expr);
                            if(err && err != CC_NOT_CONSTANT_ERROR) return err;
                            if(err)
                                return cc_error(p, tok.loc, "_Alignas requires a constant integer expression");
                            if(av < 0)
                                return cc_error(p, tok.loc, "_Alignas value must be non-negative");
                            if(av != 0 && (av & (av - 1)) != 0)
                                return cc_error(p, tok.loc, "_Alignas value must be zero or a power of 2");
                            if(av > UINT16_MAX)
                                return cc_error(p, tok.loc, "alignment too large");
                            align_val = (uint32_t)av;
                        }
                        err = cc_expect_punct(p, CC_rparen);
                        if(err) return err;
                        if(align_val > 0){
                            if(align_val > base->alignment)
                                base->alignment = (uint16_t)align_val;
                        }
                        continue;
                    }
                    case CC_typedef:
                        if(spec->sp_storagebits)
                            return cc_error(p, tok.loc, "typedef after storage class");
                        if(spec->sp_funcbits)
                            return cc_error(p, tok.loc, "typedef after function specifier");
                        spec->sp_typedef = 1;
                        continue;
                    case CC__Atomic: {
                        CcToken peek;
                        err = cc_peek(p, &peek);
                        if(err) return err;
                        if(peek.type == CC_PUNCTUATOR && peek.punct.punct == CC_lparen){
                            if(base_type->bits)
                                return cc_error(p, tok.loc, "Second type in declaration");
                            if(spec->sp_typebits)
                                return cc_error(p, tok.loc, "Second type in declaration");
                            err = cc_next_token(p, &peek); // consume '('
                            if(err) return err;
                            err = cc_parse_type_name(p, base_type, NULL);
                            if(err) return err;
                            err = cc_check_atomic_type_specifier_type(p, *base_type, tok.loc);
                            if(err) return err;
                            err = cc_expect_punct(p, CC_rparen);
                            if(err) return err;
                            base_type->is_atomic = 1;
                        }
                        spec->sp_atomic = 1;
                        continue;
                    }
                    case CC__BitInt:
                        return cc_unimplemented(p, tok.loc, "_BitInt parsing in declaration");
                    case CC__Complex:
                        return cc_unimplemented(p, tok.loc, "_Complex parsing in declaration");
                    case CC_register:
                        if(spec->sp_typedef)
                            return cc_error(p, tok.loc, "register after typedef");
                        if(spec->sp_static)
                            return cc_error(p, tok.loc, "register after static");
                        if(spec->sp_extern)
                            return cc_error(p, tok.loc, "register after extern");
                        if(spec->sp_thread_local)
                            return cc_error(p, tok.loc, "register after thread_local");
                        spec->sp_register = 1;
                        continue;
                    case CC_restrict:
                        spec->sp_restrict = 1;
                        continue;
                    case CC_unsigned:
                        if(base_type->bits)
                            return cc_error(p, tok.loc, "Second type in declaration");
                        if(spec->sp_signed)
                            return cc_error(p, tok.loc, "unsigned after signed");
                        if(spec->sp___auto_type)
                            return cc_error(p, tok.loc, "unsigned after __auto_type");
                        spec->sp_unsigned = 1;
                        continue;
                    case CC_volatile:
                        spec->sp_volatile = 1;
                        continue;
                    case CC_constexpr:
                        if(spec->sp_extern)
                            return cc_error(p, tok.loc, "constexpr after extern");
                        if(spec->sp_typedef)
                            return cc_error(p, tok.loc, "constexpr after typedef");
                        if(spec->sp_thread_local)
                            return cc_error(p, tok.loc, "constexpr after thread_local");
                        if(spec->sp_atomic)
                            return cc_error(p, tok.loc, "constexpr after _Atomic");
                        spec->sp_constexpr = 1;
                        continue;
                    case CC__Float16:
                        if(base_type->bits)
                            return cc_error(p, tok.loc, "Second type in declaration");
                        if(spec->sp_typebits)
                            return cc_error(p, tok.loc, "Second type in declaration");
                        *base_type = ccqt_basic(CCBT_float16);
                        continue;
                    case CC__Float32:
                        if(base_type->bits)
                            return cc_error(p, tok.loc, "Second type in declaration");
                        if(spec->sp_typebits)
                            return cc_error(p, tok.loc, "Second type in declaration");
                        *base_type = ccqt_basic(CCBT_float);
                        continue;
                    case CC__Float64:
                        if(base_type->bits)
                            return cc_error(p, tok.loc, "Second type in declaration");
                        if(spec->sp_typebits)
                            return cc_error(p, tok.loc, "Second type in declaration");
                        *base_type = ccqt_basic(CCBT_double);
                        continue;
                    case CC__Float128:
                        if(base_type->bits)
                            return cc_error(p, tok.loc, "Second type in declaration");
                        if(spec->sp_typebits)
                            return cc_error(p, tok.loc, "Second type in declaration");
                        *base_type = ccqt_basic(CCBT_float128);
                        continue;
                    case CC__Float32x:
                        if(base_type->bits)
                            return cc_error(p, tok.loc, "Second type in declaration");
                        if(spec->sp_typebits)
                            return cc_error(p, tok.loc, "Second type in declaration");
                        *base_type = ccqt_basic(CCBT_double);
                        continue;
                    case CC__Float64x:
                        if(base_type->bits)
                            return cc_error(p, tok.loc, "Second type in declaration");
                        if(spec->sp_typebits)
                            return cc_error(p, tok.loc, "Second type in declaration");
                        *base_type = ccqt_basic(CCBT_long_double);
                        continue;
                    case CC__Imaginary:
                        return cc_unimplemented(p, tok.loc, "_Imaginary parsing in declaration");
                    case CC__Noreturn:
                        if(spec->sp_typedef)
                            return cc_error(p, tok.loc, "noreturn after typedef");
                        spec->sp_noreturn = 1;
                        continue;
                    case CC__Decimal32:
                    case CC__Decimal64:
                    case CC__Decimal128:
                        return cc_unimplemented(p, tok.loc, "_DecimalNN parsing in declaration");
                    case CC_thread_local:
                        if(spec->sp_typedef)
                            return cc_error(p, tok.loc, "thread_local after typedef");
                        if(spec->sp_register)
                            return cc_error(p, tok.loc, "thread_local after register");
                        if(spec->sp_constexpr) // standard says no, but why not?
                            return cc_error(p, tok.loc, "thread_local after constexpr");
                        spec->sp_thread_local = 1;
                        continue;
                    case CC___attribute__:
                        err = cc_unget(p, &tok);
                        if(err) return err;
                        err = cc_parse_attributes(p, &p->attributes);
                        if(err) return err;
                        goto transfer_attrs;
                    case CC___declspec:
                        err = cc_unget(p, &tok);
                        if(err) return err;
                        err = cc_parse_declspec(p, &p->attributes);
                        if(err) return err;
                        goto transfer_attrs;
                    case CC_typeof_unqual:
                    do_typeof: {
                        if(base_type->bits)
                            return cc_error(p, tok.loc, "typeof after type");
                        if(spec->sp_typebits)
                            return cc_error(p, tok.loc, "typeof after type specifiers");
                        _Bool unqual = tok.kw.kw == CC_typeof_unqual;
                        err = cc_expect_punct(p, CC_lparen);
                        if(err) return err;
                        // Determine if argument is a type-name or expression.
                        CcToken peek;
                        err = cc_peek(p, &peek);
                        if(err) return err;
                        _Bool is_typename = cc_is_type_start(p, &peek);
                        if(is_typename){
                            err = cc_parse_type_name(p, base_type, NULL);
                            if(err) return err;
                        }
                        else {
                            CcExpr* expr = NULL;
                            err = cc_parse_expr(p, CC_RUNTIME_VALUE, &expr);
                            if(err) return err;
                            if(!expr)
                                return cc_error(p, tok.loc, "Expected expression in typeof");
                            *base_type = expr->type;
                            cc_release_expr(p, expr);
                        }
                        if(unqual) base_type->quals = 0;
                        err = cc_expect_punct(p, CC_rparen);
                        if(err) return err;
                        continue;
                    }
                }
                break;
            case CC_IDENTIFIER: {
                if(spec->sp_typebits || base_type->bits){
                    // Already have a type — this identifier is not a type name.
                    return cc_unget(p, &tok);
                }
                CcSymbol sym;
                if(!cc_scope_lookup_symbol(p->current, tok.ident.ident, CC_SCOPE_WALK_CHAIN, &sym) || sym.kind != CC_SYM_TYPEDEF){
                    // Not a typedef (or shadowed by var/func) — end of specifiers.
                    return cc_unget(p, &tok);
                }
                *base_type = sym.type;
                continue;
            }
            case CC_EOF:
            case CC_CONSTANT:
            case CC_STRING_LITERAL:
                return cc_unget(p, &tok);
            case CC_PUNCTUATOR:
                if(tok.punct.punct == CC_lbracket){
                    CcToken peek;
                    err = cc_peek(p, &peek);
                    if(err) return err;
                    if(peek.type == CC_PUNCTUATOR && peek.punct.punct == CC_lbracket){
                        err = cc_unget(p, &tok);
                        if(err) return err;
                        err = cc_parse_c23_attributes(p, &p->attributes);
                        if(err) return err;
                        goto transfer_attrs;
                    }
                }
                return cc_unget(p, &tok);
            transfer_attrs:
                if(p->attributes.has_aligned){
                    if(p->attributes.aligned > base->alignment)
                        base->alignment = p->attributes.aligned;
                }
                if(p->attributes.is_noreturn){
                    spec->sp_noreturn = 1;
                    p->attributes.is_noreturn = 0;
                }
                if(p->attributes.is_thread_local){
                    spec->sp_thread_local = 1;
                    p->attributes.is_thread_local = 0;
                }
                continue;
        }
    }
    return 0;
}

static
CcStmtNode*_Nullable
cc_stmt_node(CcParser* p, CcStmtKind k, SrcLoc loc, uint32_t count){
    CcStmtNode* n = Allocator_zalloc(cc_allocator(p), sizeof(CcStmtNode) + count * sizeof(CcStmtNode*));
    if(!n) return NULL;
    n->kind = k;
    n->count = count;
    n->loc = loc;
    return n;
}

static
void
cc_free_stmt_tree(CcParser* p, CcStmtNode*_Nullable n){
    if(!n) return;
    switch(n->kind){
        case CC_STMT_EXPR:
        case CC_STMT_IF:
        case CC_STMT_WHILE:
        case CC_STMT_DOWHILE:
        case CC_STMT_SWITCH:
        case CC_STMT_RETURN:
            if(n->exprs[0]) cc_release_expr(p, n->exprs[0]);
            break;
        case CC_STMT_FOR:
            if(n->exprs[0]) cc_release_expr(p, n->exprs[0]);
            if(n->exprs[1]) cc_release_expr(p, n->exprs[1]);
            break;
        case CC_STMT_NULL:
        case CC_STMT_COMPOUND:
        case CC_STMT_CASE:
        case CC_STMT_DEFAULT:
        case CC_STMT_BREAK:
        case CC_STMT_CONTINUE:
        case CC_STMT_GOTO:
        case CC_STMT_LABEL:
            break;
    }
    for(uint32_t i = 0; i < n->count; i++)
        cc_free_stmt_tree(p, n->stmts[i]);
    pa_cleanup(&n->decls, cc_allocator(p));
    Allocator_free(cc_allocator(p), n, sizeof(CcStmtNode) + n->count * sizeof(CcStmtNode*));
}

static
CcStmtSink*_Nullable
cc_push_stmt_sink(CcParser* p){
    CcStmtSink* sink = fl_pop(&p->scratch_stmt_sinks);
    if(!sink) sink = Allocator_zalloc(cc_allocator(p), sizeof *sink);
    if(!sink) return NULL;
    sink->stmts.count = 0;
    sink->prev = p->stmt_sink;
    p->stmt_sink = sink;
    return sink;
}

static
void
cc_pop_stmt_sink(CcParser* p, CcStmtSink* sink){
    p->stmt_sink = sink->prev;
    for(size_t i = 0; i < sink->stmts.count; i++)
        cc_free_stmt_tree(p, sink->stmts.data[i]);
    fl_push(&p->scratch_stmt_sinks, sink);
}

static
int
cc_stmt_scope_vars(CcParser* p, CcStmtNode* stmt){
    AtomMapItems items = CcAnonAM_items(&p->current->variables);
    for(size_t i = 0; i < items.count; i++){
        CcVariable* var = items.data[i].p;
        if(!var || !var->automatic) continue;
        int err = pa_push(&stmt->decls, cc_allocator(p), var);
        if(err) return CC_OOM_ERROR;
    }
    return 0;
}

static
int
cc_sink_push(CcParser* p, CcStmtNode* n){
    int err;
    if(!p->stmt_sink){
        SrcLoc loc = n->loc;
        cc_free_stmt_tree(p, n);
        return cc_unreachable(p, loc, "statement with no active statement sink");
    }
    err = pa_push(&p->stmt_sink->stmts, cc_allocator(p), n);
    if(err){
        cc_free_stmt_tree(p, n);
        return CC_OOM_ERROR;
    }
    return 0;
}

static
int
cc_finalize_stmt_list(CcParser* p, SrcLoc loc, Parray(CcStmtNode)* list, _Bool always_compound, CcStmtNode*_Nullable*_Nonnull out){
    *out = NULL;
    if(!always_compound && list->count == 1){
        *out = list->data[0];
        list->count = 0;
        return 0;
    }
    CcStmtNode* n = cc_stmt_node(p, CC_STMT_COMPOUND, loc, (uint32_t)list->count);
    if(!n) return CC_OOM_ERROR;
    for(size_t i = 0; i < list->count; i++)
        n->stmts[i] = list->data[i];
    list->count = 0;
    *out = n;
    return 0;
}

typedef struct CcSwitchCtx CcSwitchCtx;
struct CcSwitchCtx {
    CcQualType type;
    _Bool has_default;
    Marray(CcSwitchEntry) cases; // for duplicate detection; targets unused
};

// Skip a balanced `{ ... }` block. Assumes the opening `{` has NOT
// been consumed yet.
static
int
cc_skip_braced_block(CcParser* p){
    int err;
    CcToken tok;
    err = cc_expect_punct(p, '{');
    if(err) return err;
    int depth = 1;
    while(depth > 0){
        err = cc_next_token(p, &tok);
        if(err) return err;
        if(tok.type == CC_EOF)
            return cc_error(p, tok.loc, "unterminated block in static if");
        if(tok.type == CC_PUNCTUATOR){
            if(tok.punct.punct == '{') depth++;
            else if(tok.punct.punct == '}') depth--;
        }
    }
    return 0;
}

static
int
cc_parse_statement(CcParser* p, CcStmtNode*_Nullable*_Nonnull out){
    int err;
    CcToken tok;
    if(cc_is_c23_attribute_start(p)){
        CcAttributes stmt_attrs = {0};
        err = cc_parse_c23_attributes(p, &stmt_attrs);
        if(err) return err;
        err = cc_peek(p, &tok);
        if(err) return err;
        if(tok.type == CC_PUNCTUATOR && tok.punct.punct == ';'){
            // bare [[attr]];
            err = cc_next_token(p, &tok); // consume ';'
            if(err) return err;
            CcStmtNode* node = cc_stmt_node(p, CC_STMT_NULL, tok.loc, 0);
            if(!node) return CC_OOM_ERROR;
            *out = node;
            return 0;
        }
        // Merge for the next declaration/statement
        p->attributes.bits |= stmt_attrs.bits;
        // Fall through
    }
    err = cc_next_token(p, &tok);
    if(err) return err;
    switch(tok.type){
        case CC_EOF: {
            CcStmtNode* node = cc_stmt_node(p, CC_STMT_NULL, tok.loc, 0);
            if(!node) return CC_OOM_ERROR;
            *out = node;
            return 0;
        }
        case CC_KEYWORD:
            switch(tok.kw.kw){
                case CC_for: {
                    // for(init; cond; inc) body
                    err = cc_expect_punct(p, '(');
                    if(err) return err;
                    // for introduces a new scope for init declarations
                    err = cc_push_scope(p);
                    if(err) return err;
                    CcStmtNode*_Nullable init = NULL;
                    CcStmtNode*_Nullable body = NULL;
                    CcExpr* _Null_unspecified cond_expr = NULL;
                    CcExpr* _Null_unspecified inc_expr = NULL;
                    // Parse init
                    {
                        CcToken peek;
                        err = cc_peek(p, &peek);
                        if(err) goto for_end;
                        if(peek.type == CC_PUNCTUATOR && peek.punct.punct == ';'){
                            // empty init
                            cc_next_token(p, &peek); // consume ';'
                        }
                        else {
                            CcStmtSink* sink = cc_push_stmt_sink(p);
                            if(!sink){ err = CC_OOM_ERROR; goto for_end; }
                            CcDeclBase b = {0};
                            err = cc_parse_declaration_specifier(p, &b);
                            if(err) goto init_done;
                            if(b.spec.bits || b.type.bits){
                                // Declaration init: cc_parse_decls handles
                                // multiple declarators and consumes the ';'
                                err = cc_resolve_specifiers(p, &b);
                                if(!err) err = cc_parse_decls(p, &b);
                                if(err) goto init_done;
                            }
                            else {
                                // Expression init
                                CcExpr* init_expr;
                                err = cc_parse_expr(p, CC_RUNTIME_VALUE, &init_expr);
                                if(err) goto init_done;
                                err = cc_expect_punct(p, ';');
                                if(err){
                                    cc_release_expr(p, init_expr);
                                    goto init_done;
                                }
                                CcStmtNode* e = cc_stmt_node(p, CC_STMT_EXPR, tok.loc, 0);
                                if(!e){
                                    cc_release_expr(p, init_expr);
                                    err = CC_OOM_ERROR;
                                    goto init_done;
                                }
                                e->exprs[0] = init_expr;
                                err = cc_sink_push(p, e);
                            }
                            init_done:
                            if(!err) err = cc_finalize_stmt_list(p, tok.loc, &sink->stmts, 0, &init);
                            cc_pop_stmt_sink(p, sink);
                            if(err) goto for_end;
                        }
                    }
                    // Parse cond
                    {
                        CcToken peek;
                        err = cc_peek(p, &peek);
                        if(err) goto for_end;
                        if(!(peek.type == CC_PUNCTUATOR && peek.punct.punct == ';')){
                            err = cc_parse_expr(p, CC_RUNTIME_VALUE, &cond_expr);
                            if(err) goto for_end;
                            err = cc_require_scalar(p, &cond_expr, tok.loc, "'for' condition");
                            if(err) goto for_end;
                        }
                        err = cc_expect_punct(p, ';');
                        if(err) goto for_end;
                    }
                    // Parse inc
                    {
                        CcToken peek;
                        err = cc_peek(p, &peek);
                        if(err) goto for_end;
                        if(!(peek.type == CC_PUNCTUATOR && peek.punct.punct == ')')){
                            err = cc_parse_expr(p, CC_RUNTIME_VALUE, &inc_expr);
                            if(err) goto for_end;
                        }
                        err = cc_expect_punct(p, ')');
                        if(err) goto for_end;
                    }
                    // Parse body
                    err = cc_parse_local_methods(p);
                    if(err) goto for_end;
                    p->loop_depth++;
                    err = cc_parse_statement(p, &body);
                    p->loop_depth--;
                    if(err) goto for_end;
                    {
                        CcStmtNode* node = cc_stmt_node(p, CC_STMT_FOR, tok.loc, 2);
                        if(!node){ err = CC_OOM_ERROR; goto for_end; }
                        err = cc_stmt_scope_vars(p, node);
                        if(err){
                            cc_free_stmt_tree(p, node);
                            goto for_end;
                        }
                        node->exprs[0] = cond_expr;
                        node->exprs[1] = inc_expr;
                        node->stmts[0] = init;
                        node->stmts[1] = body;
                        *out = node;
                    }
                    for_end:
                    cc_pop_scope(p);
                    if(err){
                        cc_free_stmt_tree(p, init);
                        cc_free_stmt_tree(p, body);
                        if(cond_expr) cc_release_expr(p, cond_expr);
                        if(inc_expr) cc_release_expr(p, inc_expr);
                    }
                    return err;
                }
                case CC_while: {
                    // while(cond) body
                    err = cc_expect_punct(p, '(');
                    if(err) return err;
                    CcExpr* cond;
                    err = cc_parse_expr(p, CC_RUNTIME_VALUE, &cond);
                    if(err) return err;
                    err = cc_require_scalar(p, &cond, tok.loc, "'while' condition");
                    if(!err) err = cc_expect_punct(p, ')');
                    if(err){
                        cc_release_expr(p, cond);
                        return err;
                    }
                    CcStmtNode* node = cc_stmt_node(p, CC_STMT_WHILE, tok.loc, 1);
                    if(!node){
                        cc_release_expr(p, cond);
                        return CC_OOM_ERROR;
                    }
                    node->exprs[0] = cond;
                    CcStmtNode*_Nullable body = NULL;
                    p->loop_depth++;
                    err = cc_parse_statement(p, &body);
                    p->loop_depth--;
                    if(err){
                        cc_free_stmt_tree(p, node);
                        return err;
                    }
                    node->stmts[0] = body;
                    *out = node;
                    return 0;
                }
                case CC_do: {
                    // do body while(cond);
                    CcStmtNode*_Nullable body = NULL;
                    p->loop_depth++;
                    err = cc_parse_statement(p, &body);
                    p->loop_depth--;
                    if(err) return err;
                    {
                        CcToken wtok;
                        err = cc_next_token(p, &wtok);
                        if(err){
                            cc_free_stmt_tree(p, body);
                            return err;
                        }
                        if(wtok.type != CC_KEYWORD || wtok.kw.kw != CC_while){
                            cc_free_stmt_tree(p, body);
                            return cc_error(p, wtok.loc, "Expected 'while' after do body");
                        }
                    }
                    err = cc_expect_punct(p, '(');
                    if(err){
                        cc_free_stmt_tree(p, body);
                        return err;
                    }
                    CcExpr* cond;
                    err = cc_parse_expr(p, CC_RUNTIME_VALUE, &cond);
                    if(err){
                        cc_free_stmt_tree(p, body);
                        return err;
                    }
                    err = cc_require_scalar(p, &cond, tok.loc, "'do-while' condition");
                    if(!err) err = cc_expect_punct(p, ')');
                    if(!err) err = cc_expect_punct(p, ';');
                    if(err){
                        cc_release_expr(p, cond);
                        cc_free_stmt_tree(p, body);
                        return err;
                    }
                    CcStmtNode* node = cc_stmt_node(p, CC_STMT_DOWHILE, tok.loc, 1);
                    if(!node){
                        cc_release_expr(p, cond);
                        cc_free_stmt_tree(p, body);
                        return CC_OOM_ERROR;
                    }
                    node->exprs[0] = cond;
                    node->stmts[0] = body;
                    *out = node;
                    return 0;
                }
                case CC_if: {
                    // if(cond) then-body [else else-body]
                    err = cc_expect_punct(p, '(');
                    if(err) return err;
                    CcExpr* cond;
                    err = cc_parse_expr(p, CC_RUNTIME_VALUE, &cond);
                    if(err) return err;
                    err = cc_require_scalar(p, &cond, tok.loc, "'if' condition");
                    if(!err) err = cc_expect_punct(p, ')');
                    if(err){
                        cc_release_expr(p, cond);
                        return err;
                    }
                    CcStmtNode* node = cc_stmt_node(p, CC_STMT_IF, tok.loc, 2);
                    if(!node){
                        cc_release_expr(p, cond);
                        return CC_OOM_ERROR;
                    }
                    node->exprs[0] = cond;
                    CcStmtNode*_Nullable then = NULL;
                    err = cc_parse_statement(p, &then);
                    if(err){
                        cc_free_stmt_tree(p, node);
                        return err;
                    }
                    node->stmts[0] = then;
                    // Check for else
                    CcToken peek;
                    err = cc_peek(p, &peek);
                    if(err){
                        cc_free_stmt_tree(p, node);
                        return err;
                    }
                    if(peek.type == CC_KEYWORD && peek.kw.kw == CC_else){
                        cc_next_token(p, &peek); // consume 'else'
                        CcStmtNode*_Nullable els = NULL;
                        err = cc_parse_statement(p, &els);
                        if(err){
                            cc_free_stmt_tree(p, node);
                            return err;
                        }
                        node->stmts[1] = els;
                    }
                    *out = node;
                    return 0;
                }
                case CC_break: {
                    if(!p->loop_depth)
                        return cc_error(p, tok.loc, "'break' statement not in loop or switch statement");
                    err = cc_expect_punct(p, ';');
                    if(err) return err;
                    CcStmtNode* node = cc_stmt_node(p, CC_STMT_BREAK, tok.loc, 0);
                    if(!node) return CC_OOM_ERROR;
                    *out = node;
                    return 0;
                }
                case CC_continue: {
                    if(p->loop_depth <= p->switch_depth)
                        return cc_error(p, tok.loc, "'continue' statement not in loop statement");
                    err = cc_expect_punct(p, ';');
                    if(err) return err;
                    CcStmtNode* node = cc_stmt_node(p, CC_STMT_CONTINUE, tok.loc, 0);
                    if(!node) return CC_OOM_ERROR;
                    *out = node;
                    return 0;
                }
                case CC_switch: {
                    // switch(expr) { case V: ... default: ... }
                    err = cc_expect_punct(p, '(');
                    if(err) return err;
                    CcExpr* switch_expr;
                    err = cc_parse_expr(p, CC_RUNTIME_VALUE, &switch_expr);
                    if(err) return err;
                    {
                        CcQualType st = switch_expr->type;
                        if(!ccqt_is_basic(st) && ccqt_kind(st) == CC_ENUM)
                            st = ccqt_as_enum(st)->underlying;
                        if(!ccqt_is_basic(st) || (!ccbt_is_integer(st.basic.kind) && st.basic.kind != CCBT__Type)){
                            cc_release_expr(p, switch_expr);
                            return cc_error(p, tok.loc, "switch requires integer or _Type expression");
                        }
                        if(st.basic.kind == CCBT_int128 || st.basic.kind == CCBT_unsigned_int128){
                            cc_release_expr(p, switch_expr);
                            return cc_error(p, tok.loc, "__int128 is not supported in switch");
                        }
                        if(st.basic.kind != CCBT__Type){
                            CcQualType promoted;
                            err = cc_integer_promote(p, cc_arithmetic_operand_type(p, switch_expr), &promoted, tok.loc);
                            if(!err) err = cc_implicit_cast(p, switch_expr, promoted, &switch_expr);
                            if(err){ cc_release_expr(p, switch_expr); return err; }
                        }
                    }
                    err = cc_expect_punct(p, ')');
                    if(err){
                        cc_release_expr(p, switch_expr);
                        return err;
                    }
                    CcStmtNode* node = cc_stmt_node(p, CC_STMT_SWITCH, tok.loc, 1);
                    if(!node){
                        cc_release_expr(p, switch_expr);
                        return CC_OOM_ERROR;
                    }
                    node->exprs[0] = switch_expr;
                    // Parse switch body
                    {
                        CcSwitchCtx ctx = {.type = switch_expr->type};
                        CcSwitchCtx* prev_ctx = p->switch_ctx;
                        p->switch_ctx = &ctx;
                        p->loop_depth++; // for break
                        p->switch_depth++;
                        CcStmtNode*_Nullable body = NULL;
                        err = cc_parse_statement(p, &body);
                        p->switch_depth--;
                        p->loop_depth--;
                        p->switch_ctx = prev_ctx;
                        ma_cleanup(CcSwitchEntry)(&ctx.cases, cc_allocator(p));
                        if(err){
                            cc_free_stmt_tree(p, node);
                            return err;
                        }
                        node->stmts[0] = body;
                    }
                    *out = node;
                    return 0;
                }
                case CC_case: {
                    if(!p->switch_ctx)
                        return cc_error(p, tok.loc, "'case' label not within a switch statement");
                    CcExpr* case_expr;
                    err = cc_parse_expr(p, CC_CONSTEXPR_VALUE, &case_expr);
                    if(err) return err;
                    err = cc_expect_punct(p, ':');
                    if(err){
                        cc_release_expr(p, case_expr);
                        return err;
                    }
                    uint64_t case_val;
                    if(ccqt_bt_eq(p->switch_ctx->type, CCBT__Type)){
                        CcExpr* case_val_expr;
                        err = cc_eval_expr(&(CcEvalCtx){.parser = p}, case_expr, &case_val_expr);
                        if(err == CC_OVERFLOW_ERROR) err = CC_NOT_CONSTANT_ERROR;
                        cc_release_expr(p, case_expr);
                        if(err && err != CC_NOT_CONSTANT_ERROR) return err;
                        if(err)
                            return cc_error(p, tok.loc, "case label must be a constant _Type expression");
                        if(!ccqt_bt_eq(case_val_expr->type, CCBT__Type)){
                            cc_release_expr(p, case_val_expr);
                            return cc_error(p, tok.loc, "case label type does not match _Type switch expression");
                        }
                        case_val = case_val_expr->type_value.bits;
                        cc_release_expr(p, case_val_expr);
                    }
                    else {
                        int64_t case_i = 0;
                        // Validate the original expression before inserting a
                        // cast, which could otherwise make floating cases integral.
                        if(!ccqt_is_integer(case_expr->type)) err = CC_NOT_CONSTANT_ERROR;
                        else {
                            err = cc_implicit_cast(p, case_expr, p->switch_ctx->type, &case_expr);
                            if(!err) err = cc_eval_integer(&(CcEvalCtx){p}, case_expr, &case_i);
                        }
                        cc_release_expr(p, case_expr);
                        if(err && err != CC_NOT_CONSTANT_ERROR) return err;
                        if(err)
                            return cc_error(p, tok.loc, "case label must be a constant integer expression");
                        case_val = (uint64_t)case_i;
                    }
                    for(size_t i = 0; i < p->switch_ctx->cases.count; i++){
                        if(p->switch_ctx->cases.data[i].value == case_val)
                            return cc_error(p, tok.loc, "duplicate case value '%lld'", (long long)case_val);
                    }
                    {
                        CcSwitchEntry entry = {.value = case_val};
                        err = ma_push(CcSwitchEntry)(&p->switch_ctx->cases, cc_allocator(p), entry);
                        if(err) return CC_OOM_ERROR;
                    }
                    CcStmtNode* node = cc_stmt_node(p, CC_STMT_CASE, tok.loc, 1);
                    if(!node) return CC_OOM_ERROR;
                    node->case_value = case_val;
                    CcStmtNode*_Nullable child = NULL;
                    err = cc_parse_statement(p, &child);
                    if(err){
                        cc_free_stmt_tree(p, node);
                        return err;
                    }
                    node->stmts[0] = child;
                    *out = node;
                    return 0;
                }
                case CC_default: {
                    if(!p->switch_ctx)
                        return cc_error(p, tok.loc, "'default' label not within a switch statement");
                    err = cc_expect_punct(p, ':');
                    if(err) return err;
                    if(p->switch_ctx->has_default)
                        return cc_error(p, tok.loc, "Multiple default labels in switch");
                    p->switch_ctx->has_default = 1;
                    CcStmtNode* node = cc_stmt_node(p, CC_STMT_DEFAULT, tok.loc, 1);
                    if(!node) return CC_OOM_ERROR;
                    CcStmtNode*_Nullable child = NULL;
                    err = cc_parse_statement(p, &child);
                    if(err){
                        cc_free_stmt_tree(p, node);
                        return err;
                    }
                    node->stmts[0] = child;
                    *out = node;
                    return 0;
                }
                case CC_return: {
                    CcExpr* _Null_unspecified ret_expr = NULL;
                    CcToken peek;
                    err = cc_peek(p, &peek);
                    if(err) return err;
                    CcQualType ret_type = ccqt_basic(CCBT_int);
                    if(p->current_func)
                        ret_type = p->current_func->type->return_type;

                    if(!(peek.type == CC_PUNCTUATOR && peek.punct.punct == ';')){
                        if(peek.type == CC_PUNCTUATOR && peek.punct.punct == CC_lbrace){
                            err = cc_parse_init_list(p, CC_RUNTIME_VALUE, &ret_expr, ret_type);
                        }
                        else {
                            err = cc_parse_expr(p, CC_RUNTIME_VALUE, &ret_expr);
                        }
                        if(err) return err;
                        err = cc_implicit_cast(p, ret_expr, ret_type, &ret_expr); // technically this casts to void if returning from void func, but that's an ok extension
                        if(err) return err;
                    }
                    else if(!(ccqt_is_basic(ret_type) && ret_type.basic.kind == CCBT_void)){
                        return cc_error(p, peek.loc, "Returning void from non-void function");
                    }
                    err = cc_expect_punct(p, ';');
                    if(err){
                        if(ret_expr) cc_release_expr(p, ret_expr);
                        return err;
                    }
                    CcStmtNode* node = cc_stmt_node(p, CC_STMT_RETURN, tok.loc, 0);
                    if(!node){
                        if(ret_expr) cc_release_expr(p, ret_expr);
                        return CC_OOM_ERROR;
                    }
                    node->exprs[0] = ret_expr;
                    *out = node;
                    return 0;
                }
                case CC_goto: {
                    CcToken label_tok;
                    err = cc_next_token(p, &label_tok);
                    if(err) return err;
                    if(label_tok.type != CC_IDENTIFIER)
                        return cc_error(p, label_tok.loc, "Expected identifier after 'goto'");
                    err = cc_expect_punct(p, ';');
                    if(err) return err;
                    CcStmtNode* node = cc_stmt_node(p, CC_STMT_GOTO, tok.loc, 0);
                    if(!node) return CC_OOM_ERROR;
                    node->label = label_tok.ident.ident;
                    err = pa_push(&cc_label_ctx(p)->gotos, cc_allocator(p), node);
                    if(err){
                        cc_free_stmt_tree(p, node);
                        return CC_OOM_ERROR;
                    }
                    *out = node;
                    return 0;
                }
                case CC_sizeof:
                case CC_true:
                case CC_alignof:
                case CC_nullptr:
                case CC__Generic:
                case CC__Countof:
                case CC_false:
                    goto expression_statement;
                case CC_int:
                case CC_asm:
                case CC_long:
                case CC_char:
                case CC_auto:
                case CC_bool:
                case CC_else:
                case CC_enum:
                case CC_void:
                case CC_float:
                case CC_const:
                case CC_short:
                case CC_union:
                case CC_double:
                case CC_extern:
                case CC_inline:
                case CC_signed:
                case CC_static:
                case CC_struct:
                case CC_typeof:
                case CC_alignas:
                case CC_typedef:
                case CC__Atomic:
                case CC__BitInt:
                case CC__Complex:
                case CC_register:
                case CC_restrict:
                case CC_unsigned:
                case CC_volatile:
                case CC__Float16:
                case CC__Float32:
                case CC__Float64:
                case CC_constexpr:
                case CC__Float128:
                case CC__Float32x:
                case CC__Float64x:
                case CC__Imaginary:
                case CC__Noreturn:
                case CC__Decimal32:
                case CC__Decimal64:
                case CC__Decimal128:
                case CC___auto_type:
                case CC___int128:
                case CC_thread_local:
                case CC_static_assert:
                case CC_typeof_unqual:
                case CC__Any:
                case CC__Type:
                case CC__Self:
                    return cc_error(p, tok.loc, "Unexpected keyword in this position");
                case CC___attribute__: {
                    // __attribute__((fallthrough)); etc.
                    err = cc_unget(p, &tok);
                    if(err) return err;
                    CcAttributes stmt_attrs = {0};
                    err = cc_parse_attributes(p, &stmt_attrs);
                    if(err) return err;
                    err = cc_expect_punct(p, ';');
                    if(err) return err;
                    CcStmtNode* node = cc_stmt_node(p, CC_STMT_NULL, tok.loc, 0);
                    if(!node) return CC_OOM_ERROR;
                    *out = node;
                    return 0;
                }
                case CC___declspec:
                    return cc_error(p, tok.loc, "Unexpected keyword in this position");
            }
            break;
        case CC_PUNCTUATOR:
            if(tok.punct.punct == '{'){
                CcStmtSink* sink = cc_push_stmt_sink(p);
                if(!sink) return CC_OOM_ERROR;
                err = cc_push_scope(p);
                if(err){
                    cc_pop_stmt_sink(p, sink);
                    return err;
                }
                for(;;){
                    CcToken peek;
                    err = cc_peek(p, &peek);
                    if(err) goto end_block;
                    if(peek.type == CC_EOF){
                        err = cc_error(p, tok.loc, "Unterminated block");
                        goto end_block;
                    }
                    if(peek.type == CC_PUNCTUATOR && peek.punct.punct == '}'){
                        cc_next_token(p, &peek); // consume '}'
                        break;
                    }
                    err = cc_parse_one(p);
                    if(err) goto end_block;
                }
                end_block:
                if(!err){
                    CcStmtNode* node = NULL;
                    err = cc_finalize_stmt_list(p, tok.loc, &sink->stmts, 1, &node);
                    if(!err) err = cc_stmt_scope_vars(p, node);
                    if(!err) *out = node;
                    else cc_free_stmt_tree(p, node);
                }
                cc_pop_scope(p);
                cc_pop_stmt_sink(p, sink);
                return err;
            }
            if(tok.punct.punct == ';'){
                CcStmtNode* node = cc_stmt_node(p, CC_STMT_NULL, tok.loc, 0);
                if(!node) return CC_OOM_ERROR;
                *out = node;
                return 0;
            }
            goto expression_statement;
        case CC_IDENTIFIER:{
            CcToken peek;
            err = cc_peek(p, &peek);
            if(err) return err;
            if(peek.type == CC_PUNCTUATOR && peek.punct.punct == ':'){
                cc_next_token(p, &peek); // consume ':'
                Atom label_name = tok.ident.ident;
                CcLabelCtx* lctx = cc_label_ctx(p);
                if(AM_get(&lctx->labels, label_name))
                    return cc_error(p, tok.loc, "Duplicate label '%.*s'", label_name->length, label_name->data);
                CcStmtNode* node = cc_stmt_node(p, CC_STMT_LABEL, tok.loc, 1);
                if(!node) return CC_OOM_ERROR;
                node->label = label_name;
                err = AM_put(&lctx->labels, cc_allocator(p), label_name, node);
                if(err){
                    cc_free_stmt_tree(p, node);
                    return CC_OOM_ERROR;
                }
                CcStmtNode*_Nullable child = NULL;
                err = cc_parse_statement(p, &child);
                if(err){
                    cc_free_stmt_tree(p, node);
                    return err;
                }
                node->stmts[0] = child;
                *out = node;
                return 0;
            }
            goto expression_statement;
        }
        case CC_CONSTANT:
        case CC_STRING_LITERAL:{
            expression_statement:;
            err = cc_unget(p, &tok);
            if(err) return err;
            CcExpr* expr;
            err = cc_parse_expr(p, CC_RUNTIME_VALUE, &expr);
            if(err) return err;
            err = cc_expect_punct(p, ';');
            if(err){
                cc_release_expr(p, expr);
                return err;
            }
            CcStmtNode* node = cc_stmt_node(p, CC_STMT_EXPR, tok.loc, 0);
            if(!node){
                cc_release_expr(p, expr);
                return CC_OOM_ERROR;
            }
            node->exprs[0] = expr;
            *out = node;
            return 0;
        }
    }
    return CC_UNREACHABLE_ERROR;
}

static
int
cc_resolve_specifiers(CcParser* p, CcDeclBase* declbase){
    CcDeclBase b = *declbase;
    if(!b.spec.bits && !b.type.bits) return cc_unreachable(p, declbase->loc, "Resolving specifier with no spec and no type");
    if(!b.spec.sp_typebits && !b.type.bits && b.spec.sp_typedef)
        return cc_unreachable(p, declbase->loc, "typedef with no type in resolve_specifiers");
    if(!b.spec.sp_typebits && !b.type.bits)
        b.spec.sp_infer_type = 1;
    if(b.spec.sp___auto_type)
        b.spec.sp_infer_type = 1;
    if(!b.type.bits && !b.spec.sp_infer_type){
        // construct type from keywords
        if(b.spec.sp_char){
            b.type = ccqt_basic(b.spec.sp_signed? CCBT_signed_char: b.spec.sp_unsigned? CCBT_unsigned_char : CCBT_char);
        }
        else if(b.spec.sp_int128){
            b.type = ccqt_basic(b.spec.sp_unsigned? CCBT_unsigned_int128 : CCBT_int128);
        }
        else {
            CcBasicTypeKind k = CCBT_int;
            if(b.spec.sp_short)
                k -= 2;
            if(b.spec.sp_unsigned)
                k++;
            k += b.spec.sp_long * 2;
            b.type = ccqt_basic(k);
        }
    }
    if(ccqt_is_basic(b.type) && b.type.basic.kind == CCBT_double && b.spec.sp_long)
        b.type.basic.kind = CCBT_long_double;
    if(b.spec.sp_const) b.type.is_const = 1;
    if(b.spec.sp_volatile) b.type.is_volatile = 1;
    if(b.spec.sp_atomic) b.type.is_atomic = 1;
    if(b.type.is_atomic){
        CcTypeKind tk = ccqt_kind(b.type);
        if(tk == CC_ARRAY)
            return cc_error(p, b.loc, "_Atomic qualifier shall not modify an array type");
        if(tk == CC_FUNCTION)
            return cc_error(p, b.loc, "_Atomic qualifier shall not modify a function type");
    }
    *declbase = b;
    return 0;
}

// Recursive declarator parser using double-pointer technique.
// out_head/out_tail thread the type chain: after return, set
// *out_tail = base_type to complete the type.
// This is kind of crazy but based on the technique in the last section of
// https://mkukri.xyz/2022/05/01/declarators.html
static
int
cc_parse_declarator_inner(CcParser* p, CcQualType* out_head, CcQualType*_Nonnull*_Nonnull out_tail, Atom _Nullable * _Nullable out_name, SrcLoc* _Nullable out_name_loc, CcParsedParams *_Nullable out_param_names){
    int err = 0;
    CcToken tok;
    err = cc_next_token(p, &tok);
    if(err) return err;
    if(tok.type == CC_PUNCTUATOR && (tok.punct.punct == '*' || tok.punct.punct == '^')){
        // Pointer or block-pointer (^) declarator: parse qualifiers, recurse, then splice.
        CcTypeKind ptr_kind = tok.punct.punct == '^' ? CC_BLOCK_POINTER : CC_POINTER;
        _Bool const_ = 0, volatile_ = 0, atomic_ = 0;
        for(;;){
            err = cc_next_token(p, &tok);
            if(err) return err;
            if(tok.type == CC_KEYWORD){
                switch(tok.kw.kw){
                    case CC_restrict: continue;
                    case CC_const:    const_    = 1; continue;
                    case CC_volatile: volatile_ = 1; continue;
                    case CC__Atomic:  atomic_   = 1; continue;
                    default: break;
                }
            }
            err = cc_unget(p, &tok);
            if(err) return err;
            {
                CcAttributes ptr_attrs = {0};
                err = cc_parse_attributes(p, &ptr_attrs);
                if(err) return err;
                if(ptr_attrs.bits) continue;
            }
            break;
        }
        err = cc_parse_declarator_inner(p, out_head, out_tail, out_name, out_name_loc, out_param_names);
        if(err) return err;
        CcPointer* ptr = Allocator_zalloc(cc_scratch_allocator(p), sizeof *ptr);
        if(!ptr) return CC_OOM_ERROR;
        ptr->kind = ptr_kind;
        ptr->pointee = **out_tail;
        CcQualType qt = {.bits = (uintptr_t)ptr};
        qt.is_const = const_;
        qt.is_volatile = volatile_;
        qt.is_atomic = atomic_;
        **out_tail = qt;
        *out_tail = &ptr->pointee;
        return 0;
    }
    if(tok.type == CC_PUNCTUATOR && tok.punct.punct == '('){
        // Disambiguate: grouped declarator vs function parameter list.
        // '(' followed by '*', '^', '(' or non-typedef identifier -> grouped declarator.
        // '(' followed by type keyword, ')', '...', or typedef name -> function params.
        CcToken peek;
        err = cc_peek(p, &peek);
        if(err) return err;
        _Bool grouped = peek.type == CC_PUNCTUATOR && (peek.punct.punct == '*' || peek.punct.punct == '^' || peek.punct.punct == '(');
        if(!grouped && peek.type == CC_IDENTIFIER){
            // Identifier after '(' — grouped declarator unless it's a typedef
            // (not shadowed by a var/func in a closer scope).
            CcSymbol sym;
            grouped = !cc_scope_lookup_symbol(p->current, peek.ident.ident, CC_SCOPE_WALK_CHAIN, &sym) || sym.kind != CC_SYM_TYPEDEF;
        }
        if(grouped){
            err = cc_parse_declarator_inner(p, out_head, out_tail, out_name, out_name_loc, out_param_names);
            if(err) return err;
            err = cc_expect_punct(p, CC_rparen);
            if(err) return err;
        }
        else {
            // Put back '(' so the postfix loop handles it as function params.
            err = cc_unget(p, &tok);
            if(err) return err;
        }
    }
    else if(tok.type == CC_IDENTIFIER){
        if(out_name)
            *out_name = tok.ident.ident;
        if(out_name_loc)
            *out_name_loc = tok.loc;
    }
    else {
        // Abstract declarator or end of declarator
        err = cc_unget(p, &tok);
        if(err) return err;
    }
    // Postfix: arrays and function params
    for(;;){
        err = cc_next_token(p, &tok);
        if(err) return err;
        if(tok.type == CC_PUNCTUATOR && tok.punct.punct == '['){
            // Disambiguate: '[' '[' is C23 attribute, not array
            CcToken peek;
            err = cc_peek(p, &peek);
            if(err) return err;
            if(peek.type == CC_PUNCTUATOR && peek.punct.punct == '['){
                err = cc_unget(p, &tok);
                if(err) return err;
                err = cc_parse_c23_attributes(p, &p->attributes);
                if(err) return err;
                continue;
            }
            if(peek.type == CC_PUNCTUATOR && peek.punct.punct == ':'){
                err = cc_next_token(p, &peek);
                if(err) return err;
                err = cc_expect_punct(p, ']');
                if(err) return err;
                CcSlice* s = Allocator_zalloc(cc_scratch_allocator(p), sizeof *s);
                if(!s) return CC_OOM_ERROR;
                s->kind = CC_SLICE;
                s->pointee = **out_tail;
                **out_tail = (CcQualType){.bits = (uintptr_t)s};
                *out_tail = &s->pointee;
                continue;
            }
            CcArray* arr = Allocator_zalloc(cc_scratch_allocator(p), sizeof *arr);
            if(!arr) return CC_OOM_ERROR;
            arr->kind = CC_ARRAY;
            // Parse optional qualifiers and 'static' inside [] (C99 §6.7.6.2)
            // e.g. int a[static restrict 10], int a[const], int a[restrict]
            for(;;){
                err = cc_peek(p, &peek);
                if(err) return err;
                if(peek.type != CC_KEYWORD) break;
                if(peek.kw.kw == CC_static){
                    cc_next_token(p, &peek);
                    arr->is_static = 1;
                }
                else if(peek.kw.kw == CC_const
                     || peek.kw.kw == CC_volatile
                     || peek.kw.kw == CC_restrict){
                    cc_next_token(p, &peek);
                    // qualifiers on array parameter (apply to decayed pointer)
                    // ignored for now
                }
                else break;
            }
            err = cc_peek(p, &peek);
            if(err) return err;
            if(peek.type == CC_PUNCTUATOR && peek.punct.punct == ']'){
                // []
                arr->is_incomplete = 1;
            }
            else {
                CcExpr* dim = NULL;
                err = cc_parse_assignment_expr(p, CC_CONSTEXPR_VALUE, &dim, CCQT_NONE);
                if(err) return err;
                if(!dim) return cc_error(p, tok.loc, "Expected array dimension");
                int64_t length;
                err = cc_eval_integer(&(CcEvalCtx){p}, dim, &length);
                cc_release_expr(p, dim);
                if(err && err != CC_NOT_CONSTANT_ERROR) return err;
                if(err) return cc_error(p, tok.loc, "array dimension must be a constant integer expression");
                if(length < 0) return cc_error(p, tok.loc, "Negative array length");
                arr->length = (size_t)length;
            }
            err = cc_expect_punct(p, CC_rbracket);
            if(err) return err;
            arr->element = **out_tail;
            **out_tail = (CcQualType){.bits = (uintptr_t)arr};
            *out_tail = &arr->element;
            continue;
        }

        if(tok.type == CC_PUNCTUATOR && tok.punct.punct == '('){
            // Function parameters
            if(out_param_names && out_param_names->scope) out_param_names = NULL;
            err = cc_push_scope(p);
            if(err) return CC_OOM_ERROR;
            if(out_param_names) out_param_names->scope = p->current;
            Marray(CcQualType) param_types = {0};
            _Bool variadic = 0;
            _Bool no_prototype = 0;
            CcToken peek;
            err = cc_peek(p, &peek);
            if(err) goto param_err;
            if(peek.type == CC_PUNCTUATOR && peek.punct.punct == ')'){
                // () — no prototype
                no_prototype = 1;
                goto param_done;
            }
            // Check for (void)
            if(peek.type == CC_KEYWORD && peek.kw.kw == CC_void){
                CcToken ahead;
                err = cc_next_token(p, &ahead);
                if(err) goto param_err;
                err = cc_peek(p, &peek);
                if(err) goto param_err;
                if(peek.type == CC_PUNCTUATOR && peek.punct.punct == ')'){
                    goto param_done;
                }
                err = cc_unget(p, &ahead);
                if(err) goto param_err;
            }
            for(;;){
                err = cc_peek(p, &peek);
                if(err) goto param_err;
                // ...
                if(peek.type == CC_PUNCTUATOR && peek.punct.punct == CC_ellipsis){
                    err = cc_next_token(p, &peek);
                    if(err) goto param_err;
                    variadic = 1;
                    break;
                }
                // Parse parameter: declaration-specifiers [declarator]
                CcDeclBase param_base = {0};
                err = cc_parse_declaration_specifier(p, &param_base);
                if(err) goto param_err;
                if(!param_base.spec.bits && !param_base.type.bits){
                    err = cc_error(p, peek.loc, "Expected type specifier in function parameter");
                    goto param_err;
                }
                if(param_base.spec.sp_typedef){
                    err = cc_error(p, peek.loc, "typedef not allowed in function parameter");
                    goto param_err;
                }
                if(param_base.spec.sp_thread_local){
                    err = cc_error(p, peek.loc, "thread_local is not valid on parameters");
                    goto param_err;
                }
                err = cc_resolve_specifiers(p, &param_base);
                if(err) goto param_err;
                if(param_base.spec.sp_infer_type){
                    err = cc_error(p, peek.loc, "Expected type in function parameter");
                    goto param_err;
                }

                CcQualType param_head = {0};
                CcQualType* param_tail = &param_head;
                Atom param_name = NULL;
                err = cc_parse_declarator(p, &param_head, &param_tail, &param_name, NULL, NULL);
                if(err) goto param_err;
                *param_tail = param_base.type;
                if(ccqt_is_basic(param_head) && param_head.basic.kind == CCBT_void){
                    err = cc_error(p, peek.loc, "parameter cannot have void type");
                    goto param_err;
                }
                // consume trailing __attribute__
                {
                    CcAttributes param_attrs = {0};
                    err = cc_parse_attributes(p, &param_attrs);
                    if(err) goto param_err;
                }

                SrcLoc param_loc = peek.loc;
                _Bool typed_pack = 0;
                err = cc_peek(p, &peek);
                if(err) goto param_err;
                if(peek.type == CC_PUNCTUATOR && peek.punct.punct == CC_ellipsis){
                    if(!out_param_names){
                        err = cc_error(p, peek.loc, "typed varargs require a direct function declaration");
                        goto param_err;
                    }
                    err = cc_next_token(p, &peek);
                    if(err) goto param_err;
                    out_param_names->typed_pack_loc = peek.loc;
                    CcQualType element = cc_intern_qualtype(p, param_head);
                    uint32_t size;
                    if(ccqt_bt_eq(element, CCBT_void) || ccqt_kind(element) == CC_FUNCTION
                        || cc_type_sizeof_complete(cc_target(p), element, &size)){
                        err = cc_error(p, peek.loc, "typed varargs require a complete object element type");
                        goto param_err;
                    }
                    err = cc_slice_of(p, element, &param_head);
                    if(err) goto param_err;
                    typed_pack = 1;
                    err = cc_peek(p, &peek);
                    if(err) goto param_err;
                    if(peek.type != CC_PUNCTUATOR || peek.punct.punct != CC_rparen){
                        err = cc_error(p, peek.loc, "typed varargs must be the last parameter");
                        goto param_err;
                    }
                }

                err = ma_push(CcQualType)(&param_types, cc_scratch_allocator(p), param_head);
                if(err){ err = CC_OOM_ERROR; goto param_err; }

                if(param_name){
                    if(cc_scope_lookup_var(p->current, param_name, CC_SCOPE_NO_WALK)){
                        err = cc_error(p, param_loc, "duplicate parameter name '%.*s'", param_name->length, param_name->data);
                        goto param_err;
                    }
                }
                CcQualType param_type = cc_intern_qualtype(p, param_head);
                if(ccqt_kind(param_type) == CC_ARRAY && !ccqt_as_array(param_type)->is_vector){
                    err = cc_pointer_of(p, ccqt_as_array(param_type)->element, &param_type);
                    if(err) goto param_err;
                }
                else if(ccqt_kind(param_type) == CC_FUNCTION){
                    err = cc_pointer_of(p, param_type, &param_type);
                    if(err) goto param_err;
                }
                CcVariable* var = Allocator_zalloc(cc_allocator(p), sizeof *var);
                if(!var){ err = CC_OOM_ERROR; goto param_err; }
                if(!param_name) param_name = nil_atom;
                *var = (CcVariable){.name = param_name, .loc = param_loc, .type = param_type, .automatic = 1};
                err = cc_scope_insert_var(cc_allocator(p), p->current, param_name, var);
                if(err){ err = CC_OOM_ERROR; goto param_err; }
                if(out_param_names){
                    Marray(CcFuncParam)* pn = &out_param_names->names;
                    err = ma_push(CcFuncParam)(pn, cc_allocator(p), (CcFuncParam){.name = param_name, .typed_pack = typed_pack});
                    if(err){ err = CC_OOM_ERROR; goto param_err; }
                }

                err = cc_peek(p, &peek);
                if(err) goto param_err;
                if(peek.type == CC_PUNCTUATOR && peek.punct.punct == ','){
                    err = cc_next_token(p, &peek);
                    if(err) goto param_err;
                    continue;
                }
                break;
            }
            param_done:;
            {
                size_t sz = sizeof(CcFunction) + sizeof(CcQualType) * param_types.count;
                CcFunction* func = Allocator_zalloc(cc_scratch_allocator(p), sz);
                if(!func){ err = CC_OOM_ERROR; goto param_err; }
                func->kind = CC_FUNCTION;
                func->is_variadic = variadic;
                func->no_prototype = no_prototype;
                func->param_count = (uint32_t)param_types.count;
                for(uint32_t i = 0; i < param_types.count; i++)
                    func->params[i] = param_types.data[i];
                ma_cleanup(CcQualType)(&param_types, cc_scratch_allocator(p));
                err = cc_expect_punct(p, CC_rparen);
                if(err) return err;
                func->return_type = **out_tail;
                **out_tail = (CcQualType){.bits = (uintptr_t)func};
                *out_tail = &func->return_type;
                // Only collect param names for the first (outermost) function.
                out_param_names = NULL;
                continue;
            }
            param_err:
            ma_cleanup(CcQualType)(&param_types, cc_scratch_allocator(p));
            return err;
        }
        err = cc_unget(p, &tok);
        if(err) return err;
        break;
    }
    return 0;
}

static
int
cc_parse_declarator(CcParser* p, CcQualType* out_head, CcQualType*_Nonnull*_Nonnull out_tail, Atom _Nullable * _Nullable out_name, SrcLoc* _Nullable out_name_loc, CcParsedParams *_Nullable out_param_names){
    CcScope* saved_scope = p->current;
    int err = cc_parse_declarator_inner(p, out_head, out_tail, out_name, out_name_loc, out_param_names);
    if(!err && out_param_names && out_param_names->names.count
        && out_param_names->names.data[out_param_names->names.count-1].typed_pack
        && (!out_name || ccqt_kind(*out_head) != CC_FUNCTION))
        err = cc_error(p, out_param_names->typed_pack_loc, "typed varargs require a direct function declaration");
    while(p->current != saved_scope){
        CcScope* scope = p->current;
        if(out_param_names && scope == out_param_names->scope){
            if(!err){
                p->current = scope->parent;
                continue;
            }
            out_param_names->scope = NULL;
        }
        cc_pop_scope(p);
    }
    return err;
}

static
int
cc_const_object_type(CcParser* p, CcQualType type, CcQualType* out){
    if(ccqt_kind(type) != CC_ARRAY){
        type.is_const = 1;
        *out = type;
        return 0;
    }
    CcArray* old = ccqt_as_array(type);
    CcQualType element;
    int err = cc_const_object_type(p, old->element, &element);
    if(err) return err;
    CcArray* array = cc_intern_array(&p->type_cache, cc_allocator(p), element,
        old->length, old->is_static, old->is_incomplete, old->is_vector, old->vector_size);
    if(!array) return CC_OOM_ERROR;
    *out = (CcQualType){.bits=(uintptr_t)array | type.quals};
    if(old->is_vector) out->is_const = 1;
    return 0;
}

static
CcQualType
cc_intern_qualtype(CcParser* p, CcQualType t){
    uintptr_t quals = t.quals;
    switch(ccqt_kind(t)){
        case CC_BASIC:
            return t;
        case CC_POINTER: {
            CcPointer* old = ccqt_as_ptr(t);
            CcQualType pointee = cc_intern_qualtype(p, old->pointee);
            CcQualType ptr_type;
            if(cc_pointer_of(p, pointee, &ptr_type)) return t;
            ptr_type.quals |= quals;
            return ptr_type;
        }
        case CC_SLICE: {
            CcSlice* old = ccqt_as_slice(t);
            CcQualType pointee = cc_intern_qualtype(p, old->pointee);
            CcQualType ptr_type;
            if(cc_slice_of(p, pointee, &ptr_type)) return t;
            ptr_type.quals |= quals;
            return ptr_type;
        }
        case CC_BLOCK_POINTER: {
            CcPointer* old = ccqt_as_ptr(t);
            CcQualType pointee = cc_intern_qualtype(p, old->pointee);
            CcQualType ptr_type;
            if(cc_block_pointer_of(p, pointee, &ptr_type)) return t;
            ptr_type.quals |= quals;
            return ptr_type;
        }
        case CC_ARRAY: {
            CcArray* old = ccqt_as_array(t);
            CcQualType elem = cc_intern_qualtype(p, old->element);
            CcArray* arr = cc_intern_array(&p->type_cache, cc_allocator(p), elem, old->length, old->is_static, old->is_incomplete, old->is_vector, old->vector_size);
            if(!arr) return t;
            return (CcQualType){.bits = (uintptr_t)arr | quals};
        }
        case CC_FUNCTION: {
            CcFunction* old = ccqt_as_function(t);
            // Adjust and intern param types — the old node is throwaway.
            for(uint32_t i = 0; i < old->param_count; i++){
                CcQualType pt = old->params[i];
                // C11 6.7.6.3p7: array params decay to pointers.
                if(ccqt_kind(pt) == CC_ARRAY && !ccqt_as_array(pt)->is_vector){
                    uintptr_t pq = pt.quals;
                    if(cc_pointer_of(p, ccqt_as_array(pt)->element, &pt)) return t;
                    pt.bits |= pq;
                }
                // C11 6.7.6.3p7: function params decay to function pointers.
                else if(ccqt_kind(pt) == CC_FUNCTION){
                    if(cc_pointer_of(p, pt, &pt)) return t;
                }
                old->params[i] = cc_intern_qualtype(p, pt);
            }
            CcQualType ret = cc_intern_qualtype(p, old->return_type);
            CcFunction* func = cc_intern_function(&p->type_cache, cc_allocator(p), ret, old->params, old->param_count, old->param_count, old->is_variadic, old->no_prototype);
            if(!func) return t;
            return (CcQualType){.bits = (uintptr_t)func | quals};
        }
        case CC_ENUM:
        case CC_STRUCT:
        case CC_UNION:
            return t;
        DRP_CASES_EXHAUSTED;
    }
}

static
int
cc_check_pack_decl(CcParser* p, CcFunc* func, CcParsedParams* params, CcQualType type, SrcLoc loc){
    if(func->type->no_prototype || ccqt_as_function(type)->no_prototype) return 0;
    _Bool old_pack = func->params.count && func->params.data[func->params.count-1].typed_pack;
    _Bool new_pack = params->names.count && params->names.data[params->names.count-1].typed_pack;
    if(old_pack != new_pack)
        return cc_error(p, loc, "conflicting typed varargs specifier for '%.*s'", func->name->length, func->name->data);
    return 0;
}

static
int
cc_parse_decls(CcParser* p, const CcDeclBase* declbase){
    int err = 0;
    CcToken tok;
    if(p->auto_typedef && !declbase->spec.sp_typedef && !declbase->spec.sp_infer_type){
        Atom tag_name = NULL;
        CcQualType base = declbase->type;
        if(!ccqt_is_basic(base)){
            CcTypeKind tk = ccqt_kind(base);
            if(tk == CC_STRUCT) tag_name = ccqt_as_struct(base)->name;
            else if(tk == CC_UNION) tag_name = ccqt_as_union(base)->name;
            else if(tk == CC_ENUM) tag_name = ccqt_as_enum(base)->name;
        }
        if(tag_name){
            CcQualType existing_td = cc_scope_lookup_typedef(p->current, tag_name, CC_SCOPE_NO_WALK);
            if(!existing_td.bits){
                err = cc_scope_insert_typedef(cc_allocator(p), p->current, tag_name, base, (SrcLoc){0});
                if(err) return err;
            }
        }
    }
    for(_Bool first = 1;;first=0){
        Atom name = NULL;
        SrcLoc name_loc = {0};
        CcQualType type;
        _Bool is_fndef = 0;
        CcExpr* initializer = NULL;
        CcParsedParams param_names = {0};
        if(declbase->spec.sp_infer_type){
            err = cc_next_token(p, &tok);
            if(err) return err;
            if(tok.type != CC_IDENTIFIER)
                return cc_error(p, tok.loc, "Expected identifier for type-inferred declaration");
            name = tok.ident.ident;
            name_loc = tok.loc;
            type = declbase->type;
        }
        else {
            CcQualType head = {0};
            CcQualType* tail = &head;
            err = cc_parse_declarator(p, &head, &tail, &name, &name_loc, &param_names);
            if(err){
                ma_cleanup(CcFuncParam)(&param_names.names, cc_allocator(p));
                return err;
            }
            // tail != &head means the declarator itself built derived types.
            // Only allow function bodies when the declarator introduced the
            // function type, not when it came from a typedef.
            is_fndef = name && tail != &head && ccqt_kind(head) == CC_FUNCTION;
            *tail = declbase->type;
            type = cc_intern_qualtype(p, head);
            if(declbase->spec.sp_constexpr){
                err = cc_const_object_type(p, type, &type);
                if(err) return err;
            }
            // Validate type: no arrays of void/functions, no functions returning arrays/functions
            {
                CcTypeKind tk = ccqt_kind(type);
                if(tk == CC_ARRAY){
                    CcQualType elem = ccqt_as_array(type)->element;
                    if(ccqt_is_basic(elem) && elem.basic.kind == CCBT_void)
                        return cc_error(p, declbase->loc, "array of void is not allowed");
                    if(!ccqt_is_basic(elem) && ccqt_kind(elem) == CC_FUNCTION)
                        return cc_error(p, declbase->loc, "array of functions is not allowed");
                }
                else if(tk == CC_SLICE){
                    CcQualType elem = ccqt_as_slice(type)->pointee;
                    if(ccqt_is_basic(elem) && elem.basic.kind == CCBT_void)
                        return cc_error(p, declbase->loc, "slice of void is not allowed"); // maybe we should allow this actually for bounds-checked untyped buffers?
                    if(!ccqt_is_basic(elem) && ccqt_kind(elem) == CC_FUNCTION)
                        return cc_error(p, declbase->loc, "slice of functions is not allowed");
                }
                else if(tk == CC_FUNCTION){
                    CcQualType ret = ccqt_as_function(type)->return_type;
                    if(!ccqt_is_basic(ret)){
                        CcTypeKind rk = ccqt_kind(ret);
                        if(rk == CC_ARRAY)
                            return cc_error(p, declbase->loc, "function cannot return array type");
                        if(rk == CC_FUNCTION)
                            return cc_error(p, declbase->loc, "function cannot return function type");
                    }
                }
            }
        }
        // trailing __attribute__
        err = cc_parse_attributes(p, &p->attributes);
        if(err) return err;
        if(p->attributes.vector_size){
            CcQualType base = type;
            if(!ccqt_is_basic(base))
                return cc_error(p, declbase->loc, "vector_size attribute requires a scalar type");
            uint32_t elem_size = cc_target(p)->sizeof_[base.basic.kind];
            if(p->attributes.vector_size < elem_size)
                return cc_error(p, declbase->loc, "vector_size is smaller than the element type");
            if(p->attributes.vector_size % elem_size != 0)
                return cc_error(p, declbase->loc, "vector_size must be a multiple of the element size");
            uint32_t vs = p->attributes.vector_size;
            CcArray* v = cc_intern_array(&p->type_cache, cc_allocator(p), base, vs / elem_size, 0, 0, 1, vs);
            if(!v) return CC_OOM_ERROR;
            type = (CcQualType){.bits = (uintptr_t)v | base.quals};
        }
        if(p->attributes.has_aligned || p->attributes.packed || p->attributes.transparent_union){
            CcTypeKind tk = ccqt_kind(type);
            if(p->attributes.has_aligned && tk != CC_STRUCT && tk != CC_UNION){
                _Bool natural_alignment = 0;
                if(declbase->spec.sp_typedef){
                    uint32_t align;
                    err = cc_alignof_as_uint(p, type, declbase->loc, &align);
                    if(err) return err;
                    natural_alignment = align == p->attributes.aligned;
                }
                if(natural_alignment || declbase->alignment >= p->attributes.aligned){
                    // Redundant or already applied (e.g. from __declspec(align)).
                    p->attributes.has_aligned = 0;
                    p->attributes.aligned = 0;
                }
                else
                    return cc_error(p, declbase->loc, "aligned attribute on non-struct/union type is not supported");
            }
            if(p->attributes.packed && tk != CC_STRUCT)
                return cc_error(p, declbase->loc, "packed attribute on non-struct type is not supported");
            if(p->attributes.transparent_union && tk != CC_UNION)
                return cc_error(p, declbase->loc, "transparent_union attribute on non-union type is not supported");
        }
        _Bool is_printf_like = p->attributes.printf_like;
        cc_clear_attributes(&p->attributes);
        // asm label: asm("symbol")
        Atom asm_label = NULL;
        {
            CcToken peek_asm;
            err = cc_peek(p, &peek_asm);
            if(err) return err;
            if(peek_asm.type == CC_KEYWORD && peek_asm.kw.kw == CC_asm){
                cc_next_token(p, &tok); // consume asm
                err = cc_next_token(p, &tok);
                if(err) return err;
                if(tok.type != CC_PUNCTUATOR || tok.punct.punct != '(')
                    return cc_error(p, tok.loc, "Expected '(' after asm");
                err = cc_next_token(p, &tok);
                if(err) return err;
                if(tok.type != CC_STRING_LITERAL)
                    return cc_error(p, tok.loc, "Expected string literal in asm label");
                asm_label = AT_atomize(p->cpp.at, tok.str.utf8, tok.str.length - 1);
                if(!asm_label) return CC_OOM_ERROR;
                err = cc_next_token(p, &tok);
                if(err) return err;
                if(tok.type != CC_PUNCTUATOR || tok.punct.punct != ')')
                    return cc_error(p, tok.loc, "Expected ')' after asm label");
            }
        }
        // trailing __attribute__ after asm label
        err = cc_parse_attributes(p, &p->attributes);
        if(err) return err;
        is_printf_like = is_printf_like || p->attributes.printf_like;
        // postfix processing
        _Bool stop = 0;
        err = cc_next_token(p, &tok);
        if(err) return err;
        if(first && tok.type == CC_PUNCTUATOR && tok.punct.punct == '{'){
            if(!is_fndef)
                return cc_error(p, tok.loc, "Expected ',' or ';'");
            if(declbase->spec.sp_thread_local)
                return cc_error(p, name_loc, "thread_local is only valid on variables");
            _Bool eager = p->eager_parsing || p->current_func;
            CcSymbol sym;
            CcFunc* func = NULL;
            _Bool found = cc_scope_lookup_symbol(p->current, name, CC_SCOPE_NO_WALK, &sym);
            if(found){
                switch(sym.kind){
                    case CC_SYM_FUNC:
                        func = sym.func;
                        break;
                    case CC_SYM_VAR:
                    case CC_SYM_TYPEDEF:
                    case CC_SYM_ENUMERATOR:
                        return cc_error(p, tok.loc, "Redefinition of '%.*s' as a different kind of symbol", name->length, name->data);
                }
            }
            if(func){
                if(func->defined)
                    return cc_error(p, tok.loc, "Redefinition of function '%.*s'", name->length, name->data);
                err = cc_check_func_compat(p, func, declbase, type, 1, tok.loc);
                if(err) return err;
                err = cc_check_pack_decl(p, func, &param_names, type, tok.loc);
                if(err) return err;
                err = cc_merge_func_decl_type(p, func->type, &type);
                if(err) return err;
            }
            else {
                func = Allocator_zalloc(cc_allocator(p), sizeof *func);
                if(!func) return CC_OOM_ERROR;
                func->name = name;
                err = cc_scope_insert_func(cc_allocator(p), p->current, name, func);
                if(err) return err;
            }
            func->type = ccqt_as_function(type);
            func->loc = name_loc;
            func->mangle = asm_label;
            func->defined = 1;
            if(declbase->spec.sp_extern || declbase->spec.sp_static){
                func->extern_ = declbase->spec.sp_extern;
                func->static_ = declbase->spec.sp_static;
            }
            func->inline_ = declbase->spec.sp_inline;
            func->printf_like = func->printf_like || is_printf_like;
            func->params.count = param_names.names.count;
            func->params.data = param_names.names.data;
            func->param_scope = param_names.scope;
            if(eager){
                func->_Self_type = p->current_tag_type;
                func->enclosing = p->current_func;
                err = cc_parse_func_body_inner(p, func, 1);
                if(err) return err;
                err = cc_expect_punct(p, CC_rbrace);
                if(err) return err;
            }
            else {
                Marray(CcToken)* body_tokens = cc_get_scratch(p);
                if(!body_tokens) return CC_OOM_ERROR;
                int depth = 1;
                while(depth > 0){
                    CcToken t;
                    err = cc_next_token(p, &t);
                    if(err) return err;
                    if(t.type == CC_EOF)
                        return cc_error(p, tok.loc, "Unexpected EOF in function body");
                    if(t.type == CC_PUNCTUATOR){
                        if(t.punct.punct == '{') depth++;
                        else if(t.punct.punct == '}') depth--;
                    }
                    if(depth > 0){
                        err = ma_push(CcToken)(body_tokens, cc_allocator(p), t);
                        if(err) return err;
                    }
                }
                func->_Self_type = p->current_tag_type;
                func->tokens = body_tokens;
            }
            return 0;
        }
        // There is no function body to inherit this prototype scope.
        CcScope* prototype_scope = param_names.scope;
        if(prototype_scope){
            CcScope* saved_scope = p->current;
            p->current = prototype_scope;
            err = cc_parse_local_methods(p);
            p->current = saved_scope;
            if(err) return err;
            fl_push(&p->scratch_scopes, prototype_scope);
            param_names.scope = NULL;
        }
        // For non-typedef, non-function variable declarations, insert
        // the variable into scope before parsing the initializer so that
        // self-referential expressions like sizeof(*var) work.
        CcVariable* _Null_unspecified var = NULL;
        _Bool redecl = 0;
        _Bool is_func_decl = is_fndef || (type.ptr && ccqt_kind(type) == CC_FUNCTION);
        if(declbase->spec.sp_thread_local && (is_func_decl || declbase->spec.sp_typedef))
            return cc_error(p, name_loc, "thread_local is only valid on variables");
        if(declbase->spec.sp_thread_local && p->current_func && !declbase->spec.sp_static && !declbase->spec.sp_extern)
            return cc_error(p, name_loc, "block-scope thread_local requires static or extern");
        if(name && !declbase->spec.sp_typedef && !is_func_decl){
            if(declbase->spec.sp_inline)
                return cc_error(p, tok.loc, "'inline' is only valid on functions");
            if(declbase->spec.sp_noreturn)
                return cc_error(p, tok.loc, "'_Noreturn' is only valid on functions");
            CcSymbol sym;
            _Bool found = cc_scope_lookup_symbol(p->current, name, CC_SCOPE_NO_WALK, &sym);
            _Bool inherited_tls = 0;
            if(!found && p->current_func && declbase->spec.sp_extern){
                CcSymbol outer;
                if(cc_scope_lookup_symbol(p->current, name, CC_SCOPE_WALK_CHAIN, &outer)
                    && outer.kind == CC_SYM_VAR && (outer.var->thread_local_ || declbase->spec.sp_thread_local)){
                    sym = outer;
                    found = inherited_tls = 1;
                }
            }
            if(found){
                switch(sym.kind){
                    case CC_SYM_VAR:
                        if(p->current != &p->global && p->current != p->file_scope
                            && !(declbase->spec.sp_extern && (sym.var->thread_local_ || declbase->spec.sp_thread_local)))
                            return cc_error(p, tok.loc, "redefinition of '%.*s'", name->length, name->data);
                        // Validate declarations before constructing their composite type.
                        var = sym.var;
                        redecl = 1;
                        if(var->thread_local_ != declbase->spec.sp_thread_local)
                            return cc_error(p, name_loc, "conflicting thread-local storage for '%s'", name->data);
                        if((declbase->spec.sp_static && !var->static_) || (var->static_ && !declbase->spec.sp_static && !declbase->spec.sp_extern)){
                            return cc_error(p, tok.loc, "conflicting storage class for '%s': %s; previous declaration has %s",
                                name->data, declbase->spec.sp_static ? "'static'" : "no storage class",
                                var->static_ ? "'static'" : var->extern_ ? "'extern'" : "no storage class"
                            );
                        }
                        if(!declbase->spec.sp_infer_type){
                            err = cc_check_var_type(p, var, &type, tok.loc);
                            if(err) return err;
                            var->type = type;
                        }
                        if(inherited_tls){
                            err = cc_scope_insert_var(cc_allocator(p), p->current, name, var);
                            if(err) return err;
                        }
                        goto skip_var_alloc;
                    case CC_SYM_FUNC:
                    case CC_SYM_TYPEDEF:
                    case CC_SYM_ENUMERATOR:
                        return cc_error(p, tok.loc, "redefinition of '%.*s' as a different kind of symbol", name->length, name->data);
                }
            }
            var = Allocator_zalloc(cc_allocator(p), sizeof *var);
            if(!var) return CC_OOM_ERROR;
            *var = (CcVariable){
                .name = name,
                .mangle = asm_label,
                .loc = name_loc,
                .type = type,
                .alignment = declbase->alignment,
                .extern_ = declbase->spec.sp_extern,
                .static_ = declbase->spec.sp_static,
                .constexpr_ = declbase->spec.sp_constexpr,
                .thread_local_ = declbase->spec.sp_thread_local,
                .automatic = p->current_func != NULL && !declbase->spec.sp_static && !declbase->spec.sp_extern,
            };
            err = cc_scope_insert_var(cc_allocator(p), p->current, name, var);
            if(err) return err;
            skip_var_alloc:;
        }
        if(tok.type == CC_PUNCTUATOR && tok.punct.punct == '='){
            CcValueClass init_vc = CC_RUNTIME_VALUE;
            if(declbase->spec.sp_constexpr)
                init_vc = CC_CONSTEXPR_VALUE;
            else if(var && !var->automatic && (var->static_ || var->thread_local_))
                init_vc = CC_LINKTIME_VALUE;
            err = cc_parse_assignment_expr(p, init_vc, &initializer, type);
            if(err) return err;
            if(!initializer) return cc_error(p, tok.loc, "Expected expression after '='");
            err = cc_next_token(p, &tok);
            if(err) return err;
        }
        if(initializer){
            if(initializer->kind == CC_EXPR_INIT_LIST){
                // Init list type-checking is done during parsing.
                // For incomplete arrays, update the variable type from the resolved init list type.
                if(ccqt_kind(type) == CC_ARRAY && ccqt_as_array(type)->is_incomplete){
                    uintptr_t quals = type.quals;
                    type = initializer->type;
                    type.quals |= quals;
                }
            }
            else if(declbase->spec.sp_infer_type){
                // Infer type from initializer
                type = initializer->type;
                // Decay array to pointer, function to function pointer
                if(ccqt_kind(type) == CC_ARRAY && !ccqt_as_array(type)->is_vector){
                    err = cc_pointer_of(p, cc_array_element_type(type), &type);
                    if(err) return err;
                    err = cc_implicit_cast(p, initializer, type, &initializer);
                    if(err) return err;
                }
                else if(ccqt_kind(type) == CC_FUNCTION){
                    err = cc_pointer_of(p, type, &type);
                    if(err) return err;
                    err = cc_implicit_cast(p, initializer, type, &initializer);
                    if(err) return err;
                }
                // Apply qualifiers from specifier
                if(declbase->spec.sp_const || declbase->spec.sp_constexpr) type.is_const = 1;
                if(declbase->spec.sp_volatile) type.is_volatile = 1;
                if(declbase->spec.sp_atomic) type.is_atomic = 1;
            }
            else if(ccqt_kind(type) == CC_ARRAY
                    && ccqt_kind(initializer->type) == CC_ARRAY
                    && initializer->kind == CC_EXPR_VALUE && initializer->text){
                // String literal initializing a char array.
                CcArray* target_arr = ccqt_as_array(type);
                CcArray* init_arr = ccqt_as_array(initializer->type);
                err = cc_check_string_array(p, type, initializer);
                if(err) return err;
                if(target_arr->is_incomplete){
                    // char s[] = "abc" -> size from string literal.
                    // Intern a new complete array with the original element type.
                    CcArray* arr = cc_intern_array(&p->type_cache, cc_allocator(p),
                        target_arr->element, init_arr->length,
                        target_arr->is_static, 0, 0, 0);
                    if(!arr) return CC_OOM_ERROR;
                    type = (CcQualType){.bits = (uintptr_t)arr | type.quals};
                }
                else if(target_arr->length < init_arr->length - 1)
                    return cc_error(p, tok.loc, "initializer string too long for array");
                else if(target_arr->length < init_arr->length)
                    // Truncate string literal if needed.
                    initializer->type = type;
            }
            else {
                // Check compatibility and insert implicit cast
                err = cc_check_atomic_object_access(p, initializer->type, initializer->loc);
                if(err) return err;
                err = cc_check_atomic_object_access(p, type, tok.loc);
                if(err) return err;
                CcQualType target = {.unqual=type.unqual};
                err = cc_implicit_cast(p, initializer, target, &initializer);
                if(err) return err;
            }
        }
        else if(declbase->spec.sp_infer_type){
            return cc_error(p, tok.loc, "type-inferred declaration requires initializer");
        }
        if(tok.type == CC_EOF)
            stop = 1;
        else if(tok.type == CC_PUNCTUATOR && tok.punct.punct == ';')
            stop = 1;
        else if(tok.type == CC_PUNCTUATOR && tok.punct.punct == ','){
        }
        else
            return cc_error(p, tok.loc, "Expected ',' or ';'");
        if(!name){
            if(stop) break;
            continue;
        }
        if(declbase->spec.sp_typedef){
            CcSymbol sym;
            if(param_names.names.count && param_names.names.data[param_names.names.count-1].typed_pack)
                return cc_error(p, param_names.typed_pack_loc, "typed varargs require a direct function declaration");
            _Bool found = cc_scope_lookup_symbol(p->current, name, CC_SCOPE_NO_WALK, &sym);
            if(found){
                switch(sym.kind){
                    case CC_SYM_TYPEDEF:
                        if(sym.type.bits != type.bits){
                            cpp_include_backtrace(&p->cpp, LOG_PRINT_ERROR);
                            cpp_msg_preamble(&p->cpp, tok.loc, "error");
                            MStringBuilder* buff = &p->cpp.logger->buff;
                            msb_sprintf(buff, "redefinition of typedef '%.*s' as '", name->length, name->data);
                            cc_print_type(buff, type);
                            msb_write_literal(buff, "'; previously defined as '");
                            cc_print_type(buff, sym.type);
                            msb_write_char(buff, '\'');
                            log_flush(p->cpp.logger, LOG_PRINT_ERROR);
                            cpp_msg_postamble(&p->cpp, tok.loc, LOG_PRINT_ERROR);
                            return CC_SYNTAX_ERROR;
                        }
                        break;
                    case CC_SYM_VAR:
                    case CC_SYM_FUNC:
                    case CC_SYM_ENUMERATOR:
                        return cc_error(p, tok.loc, "redefinition of '%.*s' as a different kind of symbol", name->length, name->data);
                }
            }
            if(!found){
                err = cc_scope_insert_typedef(cc_allocator(p), p->current, name, type, name_loc);
                if(err) return err;
            }
        }
        else if(is_func_decl){
            CcSymbol sym;
            CcFunc* func = NULL;
            _Bool found = cc_scope_lookup_symbol(p->current, name, CC_SCOPE_NO_WALK, &sym);
            if(found){
                switch(sym.kind){
                    case CC_SYM_FUNC:
                        func = sym.func;
                        break;
                    case CC_SYM_VAR:
                    case CC_SYM_TYPEDEF:
                    case CC_SYM_ENUMERATOR:
                        return cc_error(p, tok.loc, "redefinition of '%.*s' as a different kind of symbol", name->length, name->data);
                }
            }
            _Bool keep_prototype = func && !func->type->no_prototype && ccqt_as_function(type)->no_prototype;
            if(func){
                err = cc_check_func_compat(p, func, declbase, type, 0, tok.loc);
                if(err) return err;
                err = cc_check_pack_decl(p, func, &param_names, type, tok.loc);
                if(err) return err;
                err = cc_merge_func_decl_type(p, func->type, &type);
                if(err) return err;
            }
            else {
                func = Allocator_zalloc(cc_allocator(p), sizeof *func);
                if(!func) return CC_OOM_ERROR;
                func->name = name;
                func->loc = name_loc;
                err = cc_scope_insert_func(cc_allocator(p), p->current, name, func);
                if(err) return err;
            }
            func->type = ccqt_as_function(type);
            func->mangle = asm_label;
            if(declbase->spec.sp_extern || declbase->spec.sp_static){
                func->extern_ = declbase->spec.sp_extern;
                func->static_ = declbase->spec.sp_static;
            }
            func->inline_ = declbase->spec.sp_inline;
            func->printf_like = func->printf_like || is_printf_like;
            if(!func->defined && !keep_prototype){
                func->params.count = param_names.names.count;
                func->params.data = param_names.names.data;
            }
        }
        else {
            if(!initializer && !declbase->spec.sp_extern){
                CcTypeKind tk = ccqt_kind(type);
                if(tk == CC_BASIC && type.basic.kind == CCBT_void)
                    return cc_error(p, tok.loc, "variable has incomplete type 'void'");
                if(tk == CC_ARRAY && ccqt_as_array(type)->is_incomplete && p->current_func)
                    return cc_error(p, tok.loc, "variable '%.*s' has incomplete array type", name->length, name->data);
                if(tk == CC_STRUCT && ccqt_as_struct(type)->is_incomplete)
                    return cc_error(p, tok.loc, "variable has incomplete type 'struct %s'", ccqt_as_struct(type)->name ? ccqt_as_struct(type)->name->data : "(anonymous)");
                if(tk == CC_UNION && ccqt_as_union(type)->is_incomplete)
                    return cc_error(p, tok.loc, "variable has incomplete type 'union %s'", ccqt_as_union(type)->name ? ccqt_as_union(type)->name->data : "(anonymous)");
            }
            if(var){
                if(initializer && var->initializer)
                    return cc_error(p, tok.loc, "redefinition of '%.*s'", name->length, name->data);
                if(redecl && declbase->spec.sp_infer_type){
                    err = cc_check_var_type(p, var, &type, tok.loc);
                    if(err) return err;
                }
                // An initialized definition supersedes tentative definitions.
                // Otherwise keep the first declaration that provides storage.
                if(initializer || (var->extern_ && !var->initializer && !declbase->spec.sp_extern))
                    var->loc = name_loc;
                if(!declbase->spec.sp_extern)
                    var->extern_ = 0;
                var->type = type;
                if(initializer)
                    var->initializer = initializer;
                if(initializer && (var->static_ || var->constexpr_ || var->thread_local_)){
                    err = cc_check_linktime_expr(p, initializer, 0, 0);
                    if(err == CC_NOT_CONSTANT_ERROR)
                        return cc_error(p, initializer->loc, "%s initializer requires a link-time constant", var->constexpr_ ? "constexpr" : var->thread_local_ ? "thread_local" : "static");
                    if(err) return err;
                }
                if(var->constexpr_ && ccqt_bt_eq(type, CCBT__Type)){
                    CcExpr* value;
                    err = cc_eval_expr(&(CcEvalCtx){.parser = p}, initializer, &value);
                    if(err) return err == CC_NOT_CONSTANT_ERROR ? cc_error(p, initializer->loc, "constexpr _Type initializer is not a constant expression") : err;
                    CcQualType defined_type = value->type_value;
                    cc_release_expr(p, value);
                    if(defined_type.bits){
                        err = cc_scope_insert_typedef(cc_allocator(p), p->current, var->name, defined_type, var->loc);
                        if(err) return err;
                    }
                }
            }
            if(initializer){
                if(var && (var->static_ || var->thread_local_)){
                    var->interp_preinit = 1;
                    if(initializer->kind == CC_EXPR_COMPOUND_LITERAL)
                        initializer->kind = CC_EXPR_INIT_LIST;
                }
                else {
                    CcExpr* var_ref = cc_make_expr(p, CC_EXPR_VARIABLE, tok.loc, type, 0);
                    if(!var_ref) return CC_OOM_ERROR;
                    var_ref->is_lvalue = 1;
                    var_ref->var = var;
                    if(initializer->kind == CC_EXPR_COMPOUND_LITERAL)
                        initializer->kind = CC_EXPR_INIT_LIST;
                    CcExpr* assign = cc_binary_expr(p, CC_EXPR_ASSIGN, tok.loc, type, var_ref, initializer);
                    if(!assign) return CC_OOM_ERROR;
                    CcStmtNode* node = cc_stmt_node(p, CC_STMT_EXPR, tok.loc, 0);
                    if(!node){
                        cc_release_expr(p, assign);
                        return CC_OOM_ERROR;
                    }
                    node->exprs[0] = assign;
                    err = cc_sink_push(p, node);
                    if(err) return err;
                }
            }
        }
        if(stop) break;
    }
    return cc_parse_local_methods(p);
}
static
const CcTargetConfig*
cc_target(const CcParser* p){
    return &p->cpp.target;
}

static
int
cc_handle_static_assert(CcParser* p){
    CcToken tok;
    int err;
    err = cc_next_token(p, &tok);
    if(err) return err;
    if(tok.type != CC_KEYWORD || tok.kw.kw != CC_static_assert)
        return ((void)cc_error(p, tok.loc, "ICE, handling static assert, but not on a static assert token"), CC_UNREACHABLE_ERROR);
    SrcLoc assert_loc = tok.loc;
    err = cc_expect_punct(p, CC_lparen);
    if(err) return err;
    CcExpr* expr = NULL;
    err = cc_parse_assignment_expr(p, CC_CONSTEXPR_VALUE, &expr, CCQT_NONE);
    if(err) return err;
    if(!expr)
        return cc_error(p, assert_loc, "expected expression in static_assert");
    _Bool sa_truthy;
    err = cc_eval_truthy(&(CcEvalCtx){p}, expr, &sa_truthy);
    if(err){
        cc_release_expr(p, expr);
        if(err != CC_NOT_CONSTANT_ERROR) return err;
        return cc_error(p, assert_loc, "static_assert expression is not a constant expression");
    }
    // Check for optional message.
    const char* msg = NULL;
    size_t msg_len = 0;
    err = cc_peek(p, &tok);
    if(err) return err;
    if(tok.type == CC_PUNCTUATOR && tok.punct.punct == CC_comma){
        cc_next_token(p, &tok); // consume comma
        err = cc_next_token(p, &tok);
        if(err) return err;
        if(tok.type != CC_STRING_LITERAL)
            return cc_error(p, tok.loc, "expected string literal in static_assert");
        msg = tok.str.utf8;
        msg_len = tok.str.length;
    }
    err = cc_expect_punct(p, CC_rparen);
    if(err) return err;
    err = cc_expect_punct(p, CC_semi);
    if(err) return err;
    if(!sa_truthy){
        MStringBuilder tmp = {.allocator = allocator_from_arena(&p->scratch_arena)};
        msb_write_literal(&tmp, "static assertion failed: ");
        cc_print_expr(&tmp, expr);
        cc_release_expr(p, expr);
        if(msg){
            msb_write_literal(&tmp, ": \"");
            msb_write_str(&tmp, msg, msg_len - 1);
            msb_write_literal(&tmp, "\"");
        }
        StringView sv = msb_borrow_sv(&tmp);
        cc_error(p, assert_loc, "%.*s", sv_p(sv));
        return CC_SYNTAX_ERROR;
    }
    cc_release_expr(p, expr);
    return 0;
}

static
int
cc_register_extern_var(CcParser* p, StringView name, CcQualType type){
    Allocator al = cc_allocator(p);
    Atom a = AT_atomize(p->cpp.at, name.text, name.length);
    if(!a) return CC_OOM_ERROR;
    CcVariable* var = Allocator_zalloc(al, sizeof *var);
    if(!var) return CC_OOM_ERROR;
    var->name = a;
    var->type = type;
    var->extern_ = 1;
    int err = cc_scope_insert_var(al, &p->global, a, var);
    if(err) Allocator_free(al, var, sizeof *var);
    return err;
}

static
int
cc_has_builtin(void* _Null_unspecified ctx, CppPreprocessor* cpp, SrcLoc loc, CppTokens* outtoks, const CppTokens* args, const Marray(size_t)* arg_seps){
    (void)arg_seps;
    CcParser* p = ctx;
    // Find the identifier token in the args.
    StringView name = {0};
    for(size_t i = 0; i < args->count; i++){
        CppToken tok = args->data[i];
        if(tok.type == CPP_WHITESPACE || tok.type == CPP_NEWLINE) continue;
        if(tok.type == CPP_IDENTIFIER){
            name = tok.txt;
            break;
        }
        return cpp_error(cpp, tok.loc, "Expected identifier in __has_builtin");
    }
    if(!name.text)
        return cpp_error(cpp, loc, "Expected identifier in __has_builtin");
    _Bool found = 0;
    Atom a = AT_get_atom(cpp->at, name.text, name.length);
    if(a)
        found = AM_get(&p->builtins, a) != 0;
    CppToken result = {
        .type = CPP_NUMBER,
        .loc = loc,
        .txt = found ? SV("1") : SV("0"),
    };
    return cpp_push_tok(cpp, outtoks, result);
}

static
int
cc_define_builtin_types(CcParser* p){
    int err;
    Allocator al = cc_allocator(p);
    CcTargetConfig t = p->cpp.target;

    {
        err = cc_pointer_of(p, ccqt_basic(CCBT_void), &p->void_star);
        if(err) return err;
        CcQualType const_void = ccqt_basic(CCBT_void);
        const_void.is_const = 1;
        err = cc_pointer_of(p, const_void, &p->const_void_star);
        if(err) return err;
        err = cc_pointer_of(p, ccqt_basic(CCBT_char), &p->char_star);
        if(err) return err;
        CcQualType const_char = ccqt_basic(CCBT_char);
        const_char.is_const = 1;
        err = cc_pointer_of(p, const_char, &p->const_char_star);
        if(err) return err;
        err = cc_slice_of(p, ccqt_basic(CCBT_char), &p->char_slice);
        if(err) return err;
        err = cc_slice_of(p, const_char, &p->const_char_slice);
        if(err) return err;
    }
    Atom va_list_name = AT_atomize(p->cpp.at, "__builtin_va_list", sizeof "__builtin_va_list"-1);
    if(!va_list_name) return CC_OOM_ERROR;
    Atom gnu_va_list = AT_atomize(p->cpp.at, "__gnuc_va_list", sizeof "__gnuc_va_list" - 1);
    if(!gnu_va_list) return CC_OOM_ERROR;
    CcQualType va_list_type;

    switch(t.target){
        case CC_TARGET_X86_64_LINUX:
        case CC_TARGET_X86_64_MACOS: {
            // struct __va_list_tag { unsigned gp_offset; unsigned fp_offset;
            //                       void *overflow_arg_area; void *reg_save_area; };
            Atom tag_name = AT_atomize(p->cpp.at, "__va_list_tag", sizeof "__va_list_tag" -1);
            if(!tag_name) return CC_OOM_ERROR;

            Atom gp_name = AT_atomize(p->cpp.at, "gp_offset", sizeof "gp_offset" - 1);
            Atom fp_name = AT_atomize(p->cpp.at, "fp_offset", sizeof "fp_offset" - 1);
            Atom oa_name = AT_atomize(p->cpp.at, "overflow_arg_area", sizeof "overflow_arg_area" -1);
            Atom rs_name = AT_atomize(p->cpp.at, "reg_save_area", sizeof "reg_save_area"-1);
            if(!gp_name || !fp_name || !oa_name || !rs_name) return CC_OOM_ERROR;

            CcField* fields = Allocator_alloc(al, 4 * sizeof(CcField));
            if(!fields) return CC_OOM_ERROR;
            fields[0] = (CcField){.type = ccqt_basic(CCBT_unsigned), .name = gp_name};
            fields[1] = (CcField){.type = ccqt_basic(CCBT_unsigned), .name = fp_name};
            fields[2] = (CcField){.type = p->void_star, .name = oa_name};
            fields[3] = (CcField){.type = p->void_star, .name = rs_name};

            CcStruct* s = Allocator_zalloc(al, sizeof *s);
            if(!s) return CC_OOM_ERROR;
            *s = (CcStruct){
                .kind = CC_STRUCT,
                .name = tag_name,
                .field_count = 4,
                .fields = fields,
            };
            err = cc_compute_struct_layout(p, s, 0);
            if(err) return err;
            err = cc_scope_insert_struct_tag(al, &p->global, tag_name, s);
            if(err) return CC_OOM_ERROR;

            // typedef __va_list_tag __builtin_va_list[1];
            CcQualType struct_type = {.bits = (uintptr_t)s};
            CcArray* arr = cc_intern_array(&p->type_cache, al, struct_type, 1, 0, 0, 0, 0);
            if(!arr) return CC_OOM_ERROR;
            va_list_type = (CcQualType){.bits = (uintptr_t)arr};
            break;
        }
        case CC_TARGET_AARCH64_LINUX: {
            // struct __va_list { void *__stack; void *__gr_top; void *__vr_top;
            //                    int __gr_offs; int __vr_offs; };
            Atom tag_name = AT_atomize(p->cpp.at, "__va_list", sizeof "__va_list" - 1);
            if(!tag_name) return CC_OOM_ERROR;

            Atom stack_name = AT_atomize(p->cpp.at, "__stack", sizeof "__stack" - 1);
            Atom gr_top_name = AT_atomize(p->cpp.at, "__gr_top", sizeof "__gr_top" - 1);
            Atom vr_top_name = AT_atomize(p->cpp.at, "__vr_top", sizeof "__vr_top" - 1);
            Atom gr_offs_name = AT_atomize(p->cpp.at, "__gr_offs", sizeof "__gr_offs" - 1);
            Atom vr_offs_name = AT_atomize(p->cpp.at, "__vr_offs", sizeof "__vr_offs" - 1);
            if(!stack_name || !gr_top_name || !vr_top_name || !gr_offs_name || !vr_offs_name)
                return CC_OOM_ERROR;

            CcField* fields = Allocator_alloc(al, 5 * sizeof *fields);
            if(!fields) return CC_OOM_ERROR;
            fields[0] = (CcField){.type = p->void_star, .name = stack_name};
            fields[1] = (CcField){.type = p->void_star, .name = gr_top_name};
            fields[2] = (CcField){.type = p->void_star, .name = vr_top_name};
            fields[3] = (CcField){.type = ccqt_basic(CCBT_int), .name = gr_offs_name};
            fields[4] = (CcField){.type = ccqt_basic(CCBT_int), .name = vr_offs_name};

            CcStruct* s = Allocator_zalloc(al, sizeof *s);
            if(!s) return CC_OOM_ERROR;
            *s = (CcStruct){
                .kind = CC_STRUCT,
                .name = tag_name,
                .field_count = 5,
                .fields = fields,
            };
            err = cc_compute_struct_layout(p, s, 0);
            if(err) return err;
            err = cc_scope_insert_struct_tag(al, &p->global, tag_name, s);
            if(err) return CC_OOM_ERROR;

            // typedef struct __va_list __builtin_va_list;
            va_list_type = (CcQualType){.bits = (uintptr_t)s};
            break;
        }
        case CC_TARGET_AARCH64_MACOS:
        case CC_TARGET_TEST: {
            // typedef void *__builtin_va_list;
            va_list_type = p->void_star;
            break;
        }
        case CC_TARGET_X86_64_WINDOWS: {
            // typedef char *__builtin_va_list
            va_list_type = p->char_star;
            break;
        }
        case CC_TARGET_COUNT:
            return CC_UNREACHABLE_ERROR;
        DRP_CASES_EXHAUSTED;
    }
    err = cc_scope_insert_typedef(al, &p->global, va_list_name, va_list_type, (SrcLoc){0});
    if(err) return CC_OOM_ERROR;
    err = cc_scope_insert_typedef(al, &p->global, gnu_va_list, va_list_type, (SrcLoc){0});
    if(err) return CC_OOM_ERROR;
    p->builtin_va_list = va_list_type;
    if(ccqt_kind(va_list_type) == CC_ARRAY){
        // Array decays to pointer-to-element.
        err = cc_pointer_of(p, ccqt_as_array(va_list_type)->element, &p->builtin_va_list_ptr);
        if(err) return err;
    }
    else {
        // Non-array: pointer to the va_list type.
        err = cc_pointer_of(p, va_list_type, &p->builtin_va_list_ptr);
        if(err) return err;
    }

    {
        struct f {StringView name; CcQualType type; size_t offset;} fieldinfos[] = {
            {SVI("type"), {.basic.kind=CCBT__Type}, offsetof(CiRtField, type)},
            {SVI("name"), p->const_char_slice, offsetof(CiRtField, name)},
            {SVI("offset"), {.basic.kind=CCBT_unsigned}, offsetof(CiRtField, offset)},
            {SVI("bitwidth"), {.basic.kind=CCBT_unsigned}, offsetof(CiRtField, bitwidth)},
            {SVI("bitoffset"), {.basic.kind=CCBT_unsigned}, offsetof(CiRtField, bitoffset)},
            {SVI("is_bitfield"), {.basic.kind=CCBT_unsigned}, offsetof(CiRtField, is_bitfield)},
        };
        CcField* fields = Allocator_zalloc(al, (sizeof fieldinfos / sizeof fieldinfos[0]) * sizeof *fields);
        if(!fields) return CC_OOM_ERROR;
        for(size_t i = 0; i < sizeof fieldinfos / sizeof fieldinfos[0]; i++){
            struct f* f = &fieldinfos[i];
            Atom a = AT_atomize(p->cpp.at, f->name.text, f->name.length);
            if(!a) return CC_OOM_ERROR;
            CcField* field = &fields[i];
            field->type = f->type;
            field->name = a;
            field->offset = (unsigned)f->offset;
        }
        Atom name = AT_ATOMIZE(p->cpp.at, "__builtin_Field");
        if(!name) return CC_OOM_ERROR;
        CcStruct* s = Allocator_zalloc(al, sizeof *s);
        if(!s) return CC_OOM_ERROR;
        *s = (CcStruct){
            .kind = CC_STRUCT,
            .name = name,
            .field_count = sizeof fieldinfos / sizeof fieldinfos[0],
            .fields = fields,
        };
        err = cc_compute_struct_layout(p, s, 0);
        if(err) return err;
        err = cc_scope_insert_struct_tag(al, &p->global, name, s);
        if(err) return CC_OOM_ERROR;
        p->builtin_field = (CcQualType){.bits = (uintptr_t)s};
        err = cc_scope_insert_typedef(al, &p->global, name, p->builtin_field, (SrcLoc){0});
        if(err) return CC_OOM_ERROR;
    }

    {
        struct f {StringView name; CcQualType type; size_t offset;} methodinfos[] = {
            {SVI("type"), {.basic.kind=CCBT__Type}, offsetof(CiRtMethod, type)},
            {SVI("name"), p->const_char_slice, offsetof(CiRtMethod, name)},
            {SVI("offset"), ccqt_basic(cc_target(p)->size_type), offsetof(CiRtMethod, offset)},
            {SVI("address"), ccqt_basic(cc_target(p)->size_type), offsetof(CiRtMethod, address)},
        };
        CcField* fields = Allocator_zalloc(al, (sizeof methodinfos / sizeof methodinfos[0]) * sizeof *fields);
        if(!fields) return CC_OOM_ERROR;
        for(size_t i = 0; i < sizeof methodinfos / sizeof methodinfos[0]; i++){
            struct f* f = &methodinfos[i];
            Atom a = AT_atomize(p->cpp.at, f->name.text, f->name.length);
            if(!a) return CC_OOM_ERROR;
            fields[i] = (CcField){.type = f->type, .name = a, .offset = (unsigned)f->offset};
        }
        Atom name = AT_ATOMIZE(p->cpp.at, "__builtin_Method");
        if(!name) return CC_OOM_ERROR;
        CcStruct* s = Allocator_zalloc(al, sizeof *s);
        if(!s) return CC_OOM_ERROR;
        *s = (CcStruct){
            .kind = CC_STRUCT,
            .name = name,
            .field_count = sizeof methodinfos / sizeof methodinfos[0],
            .fields = fields,
        };
        err = cc_compute_struct_layout(p, s, 0);
        if(err) return err;
        err = cc_scope_insert_struct_tag(al, &p->global, name, s);
        if(err) return CC_OOM_ERROR;
        p->builtin_method = (CcQualType){.bits = (uintptr_t)s};
        err = cc_scope_insert_typedef(al, &p->global, name, p->builtin_method, (SrcLoc){0});
        if(err) return CC_OOM_ERROR;
    }

    {
        struct f {StringView name; CcQualType type; size_t offset;} enuminfos[] = {
            {SVI("name"), p->const_char_slice, offsetof(CiRtEnumerator, name)},
            {SVI("value"), ccqt_basic(CCBT_unsigned_int128), offsetof(CiRtEnumerator, value)},
            {SVI("type"), ccqt_basic(CCBT__Type), offsetof(CiRtEnumerator, type)},
        };
        CcField* fields = Allocator_zalloc(al, (sizeof enuminfos / sizeof enuminfos[0]) * sizeof *fields);
        if(!fields) return CC_OOM_ERROR;
        for(size_t i = 0; i < sizeof enuminfos / sizeof enuminfos[0]; i++){
            struct f* f = &enuminfos[i];
            Atom a = AT_atomize(p->cpp.at, f->name.text, f->name.length);
            if(!a) return CC_OOM_ERROR;
            CcField* field = &fields[i];
            field->type = f->type;
            field->name = a;
            field->offset = (unsigned)f->offset;
        }
        Atom name = AT_ATOMIZE(p->cpp.at, "__builtin_Enumerator");
        if(!name) return CC_OOM_ERROR;
        CcStruct* s = Allocator_zalloc(al, sizeof *s);
        if(!s) return CC_OOM_ERROR;
        *s = (CcStruct){
            .kind = CC_STRUCT,
            .name = name,
            .field_count = sizeof enuminfos / sizeof enuminfos[0],
            .fields = fields,
        };
        err = cc_compute_struct_layout(p, s, 0);
        if(err) return err;
        err = cc_scope_insert_struct_tag(al, &p->global, name, s);
        if(err) return CC_OOM_ERROR;
        p->builtin_enumerator = (CcQualType){.bits = (uintptr_t)s};
        err = cc_scope_insert_typedef(al, &p->global, name, p->builtin_enumerator, (SrcLoc){0});
        if(err) return CC_OOM_ERROR;
    }
    {
        Atom name = AT_ATOMIZE(p->cpp.at, "__builtin_SrcLoc");
        if(!name) return CC_OOM_ERROR;
        CcStruct* s = Allocator_zalloc(al, sizeof *s);
        if(!s) return CC_OOM_ERROR;
        *s = (CcStruct){
            .kind = CC_STRUCT,
            .is_incomplete = 1,
            .name = name,
        };
        err = cc_scope_insert_struct_tag(al, &p->global, name, s);
        if(err) return CC_OOM_ERROR;
        CcQualType struct_ = {.bits = (uintptr_t)s};
        err = cc_pointer_of(p, struct_, &p->builtin_src_loc);
        if(err) return err;
        Atom typedef_ = AT_ATOMIZE(p->cpp.at, "_SrcLoc");
        if(!typedef_) return CC_OOM_ERROR;
        err = cc_scope_insert_typedef(al, &p->global, typedef_, p->builtin_src_loc, (SrcLoc){0});
        if(err) return CC_OOM_ERROR;
    }
    {
        struct f {StringView name; CcQualType type; size_t offset;} memberinfos[] = {
            {SVI("type"), ccqt_basic(CCBT__Type), offsetof(CiRtModuleMember, type)},
            {SVI("name"), p->const_char_slice, offsetof(CiRtModuleMember, name)},
            {SVI("address"), p->void_star, offsetof(CiRtModuleMember, address)},
            {SVI("srcloc"), p->builtin_src_loc, offsetof(CiRtModuleMember, loc)},
        };
        CcField* fields = Allocator_zalloc(al, (sizeof memberinfos / sizeof memberinfos[0]) * sizeof *fields);
        if(!fields) return CC_OOM_ERROR;
        for(size_t i = 0; i < sizeof memberinfos / sizeof memberinfos[0]; i++){
            struct f* f = &memberinfos[i];
            Atom a = AT_atomize(p->cpp.at, f->name.text, f->name.length);
            if(!a) return CC_OOM_ERROR;
            CcField* field = &fields[i];
            field->type = f->type;
            field->name = a;
            field->offset = (unsigned)f->offset;
        }
        Atom name = AT_ATOMIZE(p->cpp.at, "__builtin_ModuleMember");
        if(!name) return CC_OOM_ERROR;
        CcStruct* s = Allocator_zalloc(al, sizeof *s);
        if(!s) return CC_OOM_ERROR;
        *s = (CcStruct){
            .kind = CC_STRUCT,
            .name = name,
            .field_count = sizeof memberinfos / sizeof memberinfos[0],
            .fields = fields,
        };
        err = cc_compute_struct_layout(p, s, 0);
        if(err) return err;
        err = cc_scope_insert_struct_tag(al, &p->global, name, s);
        if(err) return CC_OOM_ERROR;
        p->builtin_module_member = (CcQualType){.bits = (uintptr_t)s};
        err = cc_scope_insert_typedef(al, &p->global, name, p->builtin_module_member, (SrcLoc){0});
        if(err) return CC_OOM_ERROR;
        Atom public_name = AT_ATOMIZE(p->cpp.at, "_ModuleMember");
        if(!public_name) return CC_OOM_ERROR;
        err = cc_scope_insert_typedef(al, &p->global, public_name, p->builtin_module_member, (SrcLoc){0});
        if(err) return CC_OOM_ERROR;
    }

    {
        Atom name = AT_ATOMIZE(p->cpp.at, "__builtin_Module");
        if(!name) return CC_OOM_ERROR;
        CcStruct* s = Allocator_zalloc(al, sizeof *s);
        if(!s) return CC_OOM_ERROR;
        *s = (CcStruct){
            .kind = CC_STRUCT,
            .is_incomplete = 1,
            .name = name,
        };
        err = cc_scope_insert_struct_tag(al, &p->global, name, s);
        if(err) return CC_OOM_ERROR;
        CcQualType module_struct = {.bits = (uintptr_t)s};
        err = cc_pointer_of(p, module_struct, &p->builtin_module);
        if(err) return err;
        Atom module_typedef = AT_ATOMIZE(p->cpp.at, "_Module");
        if(!module_typedef) return CC_OOM_ERROR;
        err = cc_scope_insert_typedef(al, &p->global, module_typedef, p->builtin_module, (SrcLoc){0});
        if(err) return CC_OOM_ERROR;
    }
    {
        // integer typedefs
        const struct {
            StringView name;
            CcBasicTypeKind kind;
        } to_register[] = {
            { SVI("__int128_t"), CCBT_int128, },
            { SVI("__uint128_t"), CCBT_unsigned_int128, },
            { SVI("int128_t"), CCBT_int128},
            { SVI("uint128_t"), CCBT_unsigned_int128},
            { SVI("int64_t"), t.int64_type},
            { SVI("uint64_t"), ccbt_to_unsigned(t.int64_type)},
            { SVI("intptr_t"), t.intptr_type},
            { SVI("uintptr_t"), ccbt_to_unsigned(t.intptr_type)},
            { SVI("size_t"), t.size_type},
            { SVI("uint32_t"), CCBT_unsigned},
            { SVI("int32_t"), CCBT_int},
            { SVI("uint16_t"), CCBT_unsigned_short},
            { SVI("int16_t"), CCBT_short},
            { SVI("uint8"), CCBT_unsigned_char},
            { SVI("int8_t"), CCBT_signed_char},
        };
        for(size_t i = 0; i < sizeof to_register / sizeof to_register[0]; i++){
            Atom name = AT_atomize(p->cpp.at, to_register[i].name.text, to_register[i].name.length);
            if(!name) return CC_OOM_ERROR;
            err = cc_scope_insert_typedef(al, &p->global, name, ccqt_basic(to_register[i].kind), (SrcLoc){0});
            if(err) return CC_OOM_ERROR;
        }
    }

    // Register builtin functions
    {
        static const struct { StringView name; CcBuiltinFunc id; } builtins[] = {
            {SVI("__builtin_constant_p"), CC__builtin_constant_p},
            {SVI("__builtin_offsetof"), CC__builtin_offsetof},
            {SVI("__builtin_types_compatible_p"), CC__builtin_types_compatible_p},
            {SVI("__builtin_choose_expr"), CC__builtin_choose_expr},
            {SVI("__func__"), CC__func__},
            {SVI("__FUNCTION__"), CC__func__},
            {SVI("__atomic_fetch_add"), CC__atomic_fetch_add},
            {SVI("__atomic_fetch_sub"), CC__atomic_fetch_sub},
            {SVI("__atomic_add_fetch"), CC__atomic_add_fetch},
            {SVI("__atomic_sub_fetch"), CC__atomic_sub_fetch},
            {SVI("__atomic_fetch_and"), CC__atomic_fetch_and},
            {SVI("__atomic_fetch_or"),  CC__atomic_fetch_or},
            {SVI("__atomic_fetch_xor"), CC__atomic_fetch_xor},
            {SVI("__atomic_load_n"), CC__atomic_load_n},
            {SVI("__atomic_load"), CC__atomic_load},
            {SVI("__atomic_store_n"), CC__atomic_store_n},
            {SVI("__atomic_exchange_n"), CC__atomic_exchange_n},
            {SVI("__atomic_compare_exchange_n"), CC__atomic_compare_exchange_n},
            {SVI("__atomic_compare_exchange"), CC__atomic_compare_exchange},
            {SVI("__atomic_store"), CC__atomic_store},
            {SVI("__atomic_exchange"), CC__atomic_exchange},
            {SVI("__atomic_thread_fence"), CC__atomic_thread_fence},
            {SVI("__atomic_signal_fence"), CC__atomic_signal_fence},
            {SVI("__builtin_va_start"), CC__builtin_va_start},
            {SVI("__va_start"), CC__builtin_va_start},
            {SVI("__builtin_va_end"),   CC__builtin_va_end},
            {SVI("__va_end"),   CC__builtin_va_end},
            {SVI("__builtin_va_arg"),   CC__builtin_va_arg},
            {SVI("__builtin_va_copy"),  CC__builtin_va_copy},
            {SVI("__builtin_expect"),  CC__builtin_expect},
            {SVI("__builtin_unreachable"), CC__builtin_unreachable},
            {SVI("__builtin_trap"), CC__builtin_trap},
            {SVI("__builtin_debugtrap"), CC__builtin_debugtrap},
            {SVI("__builtin_abort"), CC__builtin_abort},
            {SVI("__builtin_mul_overflow"), CC__builtin_mul_overflow},
            {SVI("__builtin_add_overflow"), CC__builtin_add_overflow},
            {SVI("__builtin_sub_overflow"), CC__builtin_sub_overflow},
            {SVI("__builtin_popcount"), CC__builtin_popcount},
            {SVI("__builtin_ffs"), CC__builtin_ffs},
            {SVI("__builtin_ffsl"), CC__builtin_ffsl},
            {SVI("__builtin_ffsll"), CC__builtin_ffsll},
            {SVI("__builtin_clrsb"), CC__builtin_clrsb},
            {SVI("__builtin_clrsbl"), CC__builtin_clrsbl},
            {SVI("__builtin_clrsbll"), CC__builtin_clrsbll},
            {SVI("__builtin_parity"), CC__builtin_parity},
            {SVI("__builtin_parityl"), CC__builtin_parityl},
            {SVI("__builtin_parityll"), CC__builtin_parityll},
            {SVI("__builtin_ffsg"), CC__builtin_ffsg},
            {SVI("__builtin_clzg"), CC__builtin_clzg},
            {SVI("__builtin_ctzg"), CC__builtin_ctzg},
            {SVI("__builtin_clrsbg"), CC__builtin_clrsbg},
            {SVI("__builtin_popcountg"), CC__builtin_popcountg},
            {SVI("__builtin_parityg"), CC__builtin_parityg},
            {SVI("__builtin_stdc_bit_ceil"), CC__builtin_stdc_bit_ceil},
            {SVI("__builtin_stdc_bit_floor"), CC__builtin_stdc_bit_floor},
            {SVI("__builtin_stdc_bit_width"), CC__builtin_stdc_bit_width},
            {SVI("__builtin_stdc_count_ones"), CC__builtin_stdc_count_ones},
            {SVI("__builtin_stdc_count_zeros"), CC__builtin_stdc_count_zeros},
            {SVI("__builtin_stdc_first_leading_one"), CC__builtin_stdc_first_leading_one},
            {SVI("__builtin_stdc_first_leading_zero"), CC__builtin_stdc_first_leading_zero},
            {SVI("__builtin_stdc_first_trailing_one"), CC__builtin_stdc_first_trailing_one},
            {SVI("__builtin_stdc_first_trailing_zero"), CC__builtin_stdc_first_trailing_zero},
            {SVI("__builtin_stdc_has_single_bit"), CC__builtin_stdc_has_single_bit},
            {SVI("__builtin_stdc_leading_ones"), CC__builtin_stdc_leading_ones},
            {SVI("__builtin_stdc_leading_zeros"), CC__builtin_stdc_leading_zeros},
            {SVI("__builtin_stdc_trailing_ones"), CC__builtin_stdc_trailing_ones},
            {SVI("__builtin_stdc_trailing_zeros"), CC__builtin_stdc_trailing_zeros},
            {SVI("__builtin_stdc_rotate_left"), CC__builtin_stdc_rotate_left},
            {SVI("__builtin_stdc_rotate_right"), CC__builtin_stdc_rotate_right},
            {SVI("__builtin_popcountl"), CC__builtin_popcountl},
            {SVI("__builtin_popcountll"), CC__builtin_popcountll},
            {SVI("__builtin_ctz"), CC__builtin_ctz},
            {SVI("__builtin_ctzl"), CC__builtin_ctzl},
            {SVI("__builtin_ctzll"), CC__builtin_ctzll},
            {SVI("__builtin_clz"), CC__builtin_clz},
            {SVI("__builtin_clzl"), CC__builtin_clzl},
            {SVI("__builtin_clzll"), CC__builtin_clzll},
            {SVI("__builtin_huge_val"), CC__builtin_huge_val},
            {SVI("__builtin_huge_valf"), CC__builtin_huge_valf},
            {SVI("__builtin_huge_vall"), CC__builtin_huge_vall},
            {SVI("__builtin_inf"), CC__builtin_huge_val},
            {SVI("__builtin_inff"), CC__builtin_huge_valf},
            {SVI("__builtin_infl"), CC__builtin_huge_vall},
            {SVI("__builtin_nan"), CC__builtin_nan},
            {SVI("__builtin_nanf"), CC__builtin_nanf},
            {SVI("__nan"), CC__nan},
            {SVI("__nan"), CC__nan},
            {SVI("__builtin_bswap16"), CC__builtin_bswap16},
            {SVI("__builtin_bswap32"), CC__builtin_bswap32},
            {SVI("__builtin_bswap64"), CC__builtin_bswap64},
            {SVI("__builtin_alloca"), CC__builtin_alloca},
            {SVI("_alloca"), CC__builtin_alloca},
            {SVI("alloca"), CC__builtin_alloca},
            {SVI("__builtin_intern"), CC__builtin_intern},
            {SVI("__bt"), CC__bt},
            {SVI("_InterlockedExchange"), CC_InterlockedExchange},
            {SVI("_InterlockedExchange8"), CC_InterlockedExchange8},
            {SVI("_InterlockedExchange16"), CC_InterlockedExchange16},
            {SVI("_InterlockedExchange64"), CC_InterlockedExchange64},
            {SVI("_InterlockedCompareExchange"), CC_InterlockedCompareExchange},
            {SVI("_InterlockedCompareExchange8"), CC_InterlockedCompareExchange8},
            {SVI("_InterlockedCompareExchange16"), CC_InterlockedCompareExchange16},
            {SVI("_InterlockedCompareExchange64"), CC_InterlockedCompareExchange64},
            {SVI("_InterlockedCompareExchange128"), CC_InterlockedCompareExchange128},
            {SVI("_InterlockedIncrement"), CC_InterlockedIncrement},
            {SVI("_InterlockedIncrement16"), CC_InterlockedIncrement16},
            {SVI("_InterlockedIncrement64"), CC_InterlockedIncrement64},
            {SVI("_InterlockedDecrement"), CC_InterlockedDecrement},
            {SVI("_InterlockedDecrement16"), CC_InterlockedDecrement16},
            {SVI("_InterlockedDecrement64"), CC_InterlockedDecrement64},
            {SVI("_InterlockedExchangeAdd"), CC_InterlockedExchangeAdd},
            {SVI("_InterlockedExchangeAdd8"), CC_InterlockedExchangeAdd8},
            {SVI("_InterlockedExchangeAdd16"), CC_InterlockedExchangeAdd16},
            {SVI("_InterlockedExchangeAdd64"), CC_InterlockedExchangeAdd64},
            {SVI("_InterlockedAnd"), CC_InterlockedAnd},
            {SVI("_InterlockedAnd8"), CC_InterlockedAnd8},
            {SVI("_InterlockedAnd16"), CC_InterlockedAnd16},
            {SVI("_InterlockedAnd64"), CC_InterlockedAnd64},
            {SVI("_InterlockedOr"), CC_InterlockedOr},
            {SVI("_InterlockedOr8"), CC_InterlockedOr8},
            {SVI("_InterlockedOr16"), CC_InterlockedOr16},
            {SVI("_InterlockedOr64"), CC_InterlockedOr64},
            {SVI("_InterlockedXor"), CC_InterlockedXor},
            {SVI("_InterlockedXor8"), CC_InterlockedXor8},
            {SVI("_InterlockedXor16"), CC_InterlockedXor16},
            {SVI("_InterlockedXor64"), CC_InterlockedXor64},
            {SVI("_byteswap_ushort"), CC__builtin_bswap16},
            {SVI("_byteswap_ulong"), CC__builtin_bswap32},
            {SVI("_byteswap_uint64"), CC__builtin_bswap64},
            {SVI("_umul128"), CC__umul128},
            {SVI("__root_module"), CC__root_module},
            {SVI("__hotswap"), CC__hotswap},
            {SVI("__compile"), CC__compile},
        };
        for(size_t i = 0; i < sizeof builtins / sizeof builtins[0]; i++){
            Atom a = AT_atomize(p->cpp.at, builtins[i].name.text, builtins[i].name.length);
            if(!a) return CC_OOM_ERROR;
            err = AM_put(&p->builtins, al, a, (void*)builtins[i].id);
            if(err) return CC_OOM_ERROR;
        }
    }
    err = cpp_define_builtin_func_macro(&p->cpp, SV("__has_builtin"), cc_has_builtin, p, 1, 0, 1);
    if(err) return err;

    // Register type methods/fields
    {
        static const struct { StringView name; CcTypeIntrospectionOp op; } typeintro[] = {
            {SVI("name"), CC_TYPE_NAME},
            {SVI("tag"), CC_TYPE_TAG},
            {SVI("is_valid"), CC_TYPE_IS_VALID},
            {SVI("is_invalid"), CC_TYPE_IS_INVALID},
            {SVI("is_integer"), CC_TYPE_IS_INTEGER},
            {SVI("is_float"), CC_TYPE_IS_FLOAT},
            {SVI("is_arithmetic"), CC_TYPE_IS_ARITHMETIC},
            {SVI("is_pointer"), CC_TYPE_IS_POINTER},
            {SVI("is_struct"), CC_TYPE_IS_STRUCT},
            {SVI("is_union"), CC_TYPE_IS_UNION},
            {SVI("is_array"), CC_TYPE_IS_ARRAY},
            {SVI("is_vector"), CC_TYPE_IS_VECTOR},
            {SVI("is_slice"), CC_TYPE_IS_SLICE},
            {SVI("is_function"), CC_TYPE_IS_FUNCTION},
            {SVI("is_enum"), CC_TYPE_IS_ENUM},
            {SVI("is_const"), CC_TYPE_IS_CONST},
            {SVI("is_volatile"), CC_TYPE_IS_VOLATILE},
            {SVI("is_atomic"), CC_TYPE_IS_ATOMIC},
            {SVI("is_unsigned"), CC_TYPE_IS_UNSIGNED},
            {SVI("is_signed"), CC_TYPE_IS_SIGNED},
            {SVI("is_callable"), CC_TYPE_IS_CALLABLE},
            {SVI("is_variadic"), CC_TYPE_IS_VARIADIC},
            {SVI("is_incomplete"), CC_TYPE_IS_INCOMPLETE},
            {SVI("sizeof_"), CC_TYPE_SIZEOF},
            {SVI("alignof_"), CC_TYPE_ALIGNOF},
            {SVI("pointee"), CC_TYPE_POINTEE},
            {SVI("unqual"), CC_TYPE_UNQUAL},
            {SVI("count"), CC_TYPE_COUNT},
            {SVI("loc"), CC_TYPE_LOC},
            {SVI("is_callable_with"), CC_TYPE_IS_CALLABLE_WITH},
            {SVI("is_callable_through"), CC_TYPE_IS_CALLABLE_THROUGH},
            {SVI("is_castable_to"), CC_TYPE_CASTABLE_TO},
            {SVI("make_any"), CC_TYPE_MAKE_ANY},
            {SVI("field"), CC_TYPE_FIELD}, // field name or index;
            {SVI("fields"), CC_TYPE_FIELDS},
            {SVI("method"), CC_TYPE_METHOD},
            {SVI("methods"), CC_TYPE_METHODS},
            {SVI("has_field"), CC_TYPE_HAS_FIELD},
            {SVI("has_method"), CC_TYPE_HAS_METHOD},
            {SVI("push_method"), CC_TYPE_PUSH_METHOD},
            {SVI("enumerators"), CC_TYPE_ENUMERATORS},
            {SVI("enumerator"), CC_TYPE_ENUMERATOR},
            {SVI("return_type"), CC_TYPE_RETURN_TYPE},
            {SVI("param_count"), CC_TYPE_PARAM_COUNT},
            {SVI("param_type"), CC_TYPE_PARAM_TYPE},
            {SVI("element_type"), CC_TYPE_ELEMENT_TYPE},
            {SVI("underlying_type"), CC_TYPE_UNDERLYING_TYPE},
        };
        for(size_t i = 0; i < sizeof typeintro / sizeof typeintro[0]; i++){
            Atom a = AT_atomize(p->cpp.at, typeintro[i].name.text, typeintro[i].name.length);
            if(!a) return CC_OOM_ERROR;
            err = AM_put(&p->type_intro, al, a, (void*)(uintptr_t)typeintro[i].op);
            if(err) return CC_OOM_ERROR;
        }
    }
    // Register __builtin_ libc functions
    {
        struct b {StringView name; CcQualType ret; int nargs; CcQualType params[3]; _Bool variadic; _Bool printf_like;} builtins[] = {
            {SVI("memcpy"), p->void_star, 3, {p->void_star, p->const_void_star, {.basic.kind=t.size_type}}, .variadic=0},
            {SVI("memcmp"), {.basic.kind=CCBT_int}, 3, {p->const_void_star, p->const_void_star, {.basic.kind=t.size_type}}, .variadic=0},
            {SVI("strcmp"), {.basic.kind=CCBT_int}, 2, {p->const_char_star, p->const_char_star}, .variadic=0},
            {SVI("memmove"), p->void_star, 3, {p->void_star, p->const_void_star, {.basic.kind=t.size_type}}, .variadic=0},
            {SVI("memset"), p->void_star, 3, {p->void_star, {.basic.kind=CCBT_int}, {.basic.kind=t.size_type}}, .variadic=0},
            {SVI("malloc"), p->void_star, 1, {{.basic.kind=t.size_type}}, .variadic=0},
            {SVI("realloc"), p->void_star, 2, {p->void_star, {.basic.kind=t.size_type}}, .variadic=0},
            {SVI("calloc"), p->void_star, 2, {{.basic.kind=t.size_type},{.basic.kind=t.size_type}}, .variadic=0},
            {SVI("free"), {.basic.kind=CCBT_void}, 1, {p->void_star}, .variadic=0},
            {SVI("bzero"), {.basic.kind=CCBT_void}, 2, {p->void_star, {.basic.kind=t.size_type}}, .variadic=0},
            {SVI("snprintf"), {.basic.kind=CCBT_int}, 3, {p->char_star, {.basic.kind=t.size_type}, p->const_char_star}, .variadic=1, .printf_like=1},
            {SVI("printf"), {.basic.kind=CCBT_int}, 1, {p->const_char_star}, .variadic=1, .printf_like=1},
            {SVI("fabsf"), {.basic.kind=CCBT_float}, 1, {{.basic.kind=CCBT_float}}},
            {SVI("fabs"), {.basic.kind=CCBT_double}, 1, {{.basic.kind=CCBT_double}}},
            {SVI("fabsl"), {.basic.kind=CCBT_long_double}, 1, {{.basic.kind=CCBT_long_double}}},
        };
        for(size_t i = 0; i < sizeof builtins / sizeof builtins[0]; i++){
            struct b* b = &builtins[i];
            CcFunction* ftype = cc_intern_function(&p->type_cache, al, b->ret, b->params, b->nargs, b->nargs, b->variadic, 0);
            if(!ftype) return CC_OOM_ERROR;
            CcFunc* func = Allocator_zalloc(al, sizeof *func);
            if(!func) return CC_OOM_ERROR;
            Atom key = cpp_atomizef(&p->cpp, "__builtin_%.*s", (int)b->name.length, b->name.text);
            if(!key) return CC_OOM_ERROR;
            Atom name = AT_atomize(p->cpp.at, b->name.text, b->name.length);
            if(!name) return CC_OOM_ERROR;
            func->name = name;
            func->type = ftype;
            func->extern_ = 1;
            func->libc_builtin = 1;
            func->printf_like = b->printf_like;
            err = cc_scope_insert_func(al, &p->global, key, func);
            if(err) return CC_OOM_ERROR;
            err = cc_scope_insert_func(al, &p->global, name, func);
            if(err) return CC_OOM_ERROR;
        }
    }
    return 0;
}

static
int
cc_parse_local_methods(CcParser* p){
    if(!p->current_func) return 0;
    CcScope* scope = p->current;
    while(scope->deferred_methods.count){
        CcFunc* f = scope->deferred_methods.data[--scope->deferred_methods.count];
        int err = cc_parse_func_body(p, f);
        if(err) return err;
    }
    return 0;
}

static
int
cc_parse_func_body_inner(CcParser* p, CcFunc* f, _Bool terminate_on_rbrace){
    int err = 0;
    CcFunc* prev = p->current_func;
    CcQualType prev__Self = p->current_tag_type;
    uint32_t prev_loop_depth = p->loop_depth;
    uint32_t prev_switch_depth = p->switch_depth;
    CcSwitchCtx* prev_switch_ctx = p->switch_ctx;
    CcAttributes prev_attributes = p->attributes;
    p->loop_depth = 0;
    p->switch_depth = 0;
    p->switch_ctx = NULL;
    cc_clear_attributes(&p->attributes);
    p->current_func = f;
    p->current_tag_type = f->_Self_type;
    CcScope* param_scope = f->param_scope;
    if(param_scope){
        param_scope->parent = p->current;
        p->current = param_scope;
        f->param_scope = NULL;
        err = 0;
    }
    else err = cc_push_scope(p);
    if(err) goto restore_context;
    err = cc_parse_local_methods(p);
    if(err) goto end_scope;
    {
        CcStmtSink* sink = cc_push_stmt_sink(p);
        if(!sink){ err = CC_OOM_ERROR; goto end_scope; }
        for(;;){
            CcToken peek;
            err = cc_peek(p, &peek);
            if(err) break;
            if(terminate_on_rbrace){
                if(peek.type == CC_PUNCTUATOR && peek.punct.punct == CC_rbrace) break;
                if(peek.type == CC_EOF){
                    err = cc_error(p, f->loc, "Unexpected EOF in function body");
                    break;
                }
            }
            else {
                if(peek.type == CC_EOF) break;
            }
            err = cc_parse_one(p);
            if(err) break;
        }
        if(!err)
            err = cc_finalize_stmt_list(p, f->loc, &sink->stmts, 1, &f->body_tree);
        if(!err) err = cc_stmt_scope_vars(p, (CcStmtNode*)f->body_tree);
        cc_pop_stmt_sink(p, sink);
        if(err) goto end_scope;
    }
    err = cc_check_gotos(p, &f->label_ctx);
    if(err) goto end_scope;
    f->parsed = 1;
    end_scope:
    pa_cleanup(&f->label_ctx.gotos, cc_allocator(p));
    cc_pop_scope(p);
    restore_context:
    p->current_func = prev;
    p->current_tag_type = prev__Self;
    p->loop_depth = prev_loop_depth;
    p->switch_depth = prev_switch_depth;
    p->switch_ctx = prev_switch_ctx;
    p->attributes = prev_attributes;
    return err;
}

static
int
cc_parse_func_body(CcParser* p, CcFunc* f){
    if(!f->defined) return CC_UNREACHABLE_ERROR;
    if(f->parsed) return 0;
    if(f->parse_failed) return CC_SYNTAX_ERROR;
    Marray(CcToken)* tokens = f->tokens;
    if(!tokens){ f->parse_failed = 1; return CC_SYNTAX_ERROR; }
    // Append EOF sentinel so parsing doesn't fall through to the main stream.
    CcToken eof_tok = {.type = CC_EOF};
    int eof_err = ma_push(CcToken)(tokens, cc_allocator(p), eof_tok);
    if(eof_err){ return CC_OOM_ERROR; }
    // Reverse the token array so it works as LIFO pending
    for(size_t i = 0, j = tokens->count; i < j; ){
        j--;
        CcToken tmp = tokens->data[i];
        tokens->data[i] = tokens->data[j];
        tokens->data[j] = tmp;
        i++;
    }
    // Swap into pending
    Marray(CcToken) saved_pending = p->pending;
    p->pending = *tokens;
    int err = cc_parse_func_body_inner(p, f, 0);
    // Restore pending (tokens array was consumed into p->pending)
    *tokens = p->pending;
    p->pending = saved_pending;
    cc_release_scratch(p, tokens);
    f->tokens = NULL;
    if(err) f->parse_failed = 1;
    return err;
}

// Wide constants retain the target representation in the expression payload.
static
_Bool
cc_eval_wide(CcQualType t){
    while(ccqt_kind(t) == CC_ENUM) t = ccqt_as_enum(t)->underlying;
    return ccqt_bt_eq(t, CCBT_long_double) || ccqt_bt_eq(t, CCBT_float128)
        || ccqt_bt_eq(t, CCBT_int128) || ccqt_bt_eq(t, CCBT_unsigned_int128);
}

static
CiUint128
cc_eval_u128(CcParser* p, CcExpr* v){
    if(cc_eval_wide(v->type) && ccqt_is_integer(v->type))
        return v->uinteger128;
    if(ccqt_kind(v->type) == CC_POINTER)
        return ci_uint128_from_uint64(v->uinteger);
    if(ccqt_is_unsigned(v->type, !cc_target(p)->char_is_signed))
        return ci_uint128_from_uint64(v->uinteger);
    return ci_uint128_from_int64(v->integer);
}

static
CiFloat128
cc_eval_quad(CcParser* p, CcExpr* v){
    if(ccqt_bt_eq(v->type, CCBT_float128)) return v->quad;
    if(ccqt_bt_eq(v->type, CCBT_long_double)){
        switch(cc_target(p)->long_double_format){
            case CC_LONG_DOUBLE_X87: return ci_float128_from_float80(v->x87);
            case CC_LONG_DOUBLE_BINARY128: return v->quad;
            case CC_LONG_DOUBLE_BINARY64: return ci_float128_from_double(v->double_);
        }
    }
    if(ccqt_bt_eq(v->type, CCBT_float)) return ci_float128_from_float(v->float_);
    if(ccqt_bt_eq(v->type, CCBT_double)) return ci_float128_from_double(v->double_);
    return ci_float128_from_uint128(cc_eval_u128(p, v), ccqt_is_unsigned(v->type, !cc_target(p)->char_is_signed));
}

static
void
cc_eval_store_quad(CcParser* p, CcExpr* v, CiFloat128 q){
    if(ccqt_bt_eq(v->type, CCBT_long_double)){
        switch(cc_target(p)->long_double_format){
            case CC_LONG_DOUBLE_X87: v->x87 = ci_float80_from_float128(q); return;
            case CC_LONG_DOUBLE_BINARY64: v->double_ = ci_float128_to_double(q); return;
            case CC_LONG_DOUBLE_BINARY128: break;
        }
    }
    v->quad = q;
}

static
_Bool
cc_eval_value_truth(CcParser* p, CcExpr* v){
    if(ccqt_bt_eq(v->type, CCBT_long_double) || ccqt_bt_eq(v->type, CCBT_float128))
        return ci_float128_nonzero(cc_eval_quad(p, v));
    if(ccqt_bt_eq(v->type, CCBT_float)) return v->float_ != 0;
    if(ccqt_bt_eq(v->type, CCBT_double)) return v->double_ != 0;
    if(cc_eval_wide(v->type)) return ci_uint128_nonzero(v->uinteger128);
    return v->uinteger != 0;
}

static
int
cc_eval_wide_cast(CcParser* p, CcExpr* v, CcExpr* out){
    CcQualType t = out->type;
    if(ccqt_bt_eq(t, CCBT_float16) || ccqt_bt_eq(v->type, CCBT_float16))
        return CC_UNIMPLEMENTED_ERROR;
    if(ccqt_bt_eq(t, CCBT_bool)){
        out->integer = cc_eval_value_truth(p, v);
        return 0;
    }
    _Bool from_float = ccqt_is_basic(v->type) && ccbt_is_float(v->type.basic.kind);
    if(ccqt_is_basic(t) && ccbt_is_float(t.basic.kind)){
        CiFloat128 q = cc_eval_quad(p, v);
        if(ccqt_bt_eq(t, CCBT_float)) out->float_ = ci_float128_to_float(q);
        else if(ccqt_bt_eq(t, CCBT_double)) out->double_ = ci_float128_to_double(q);
        else cc_eval_store_quad(p, out, q);
        CiFloat128 back = cc_eval_quad(p, out);
        if(!ci_float128_eq(back, q)) return CC_NOT_CONSTANT_ERROR;
        if(!from_float && !ci_uint128_eq(ci_float128_to_uint128(back), cc_eval_u128(p, v)))
            return CC_NOT_CONSTANT_ERROR;
        return 0;
    }
    if(!ccqt_is_integer(t)) return CC_NOT_CONSTANT_ERROR;
    CiUint128 u;
    if(from_float){
        CiFloat128 q = cc_eval_quad(p, v);
        uint32_t size;
        int err = cc_sizeof_as_uint(p, t, out->loc, &size);
        if(err) return err;
        _Bool uns = ccqt_is_unsigned(t, !cc_target(p)->char_is_signed);
        CiFloat128 bound = ci_float128_from_uint128(ci_uint128_shl(ci_uint128_from_uint64(1), size*8-1), 1);
        if(uns) bound = ci_float128_add(bound, bound);
        CiFloat128 low = uns ? ci_float128_from_int64(0) : ci_float128_neg(bound);
        if(!ci_float128_le(low, q) || !ci_float128_lt(q, bound)) return CC_OVERFLOW_ERROR;
        u = ci_float128_to_uint128(q);
    }
    else u = cc_eval_u128(p, v);
    if(cc_eval_wide(t)) out->uinteger128 = u;
    else out->uinteger = ci_uint128_lo(u);
    return 0;
}

static
int
cc_eval_wide_binary(CcParser* p, CcExprKind op, CcExpr* l, CcExpr* r, CcExpr* out){
    if(ccqt_bt_eq(l->type, CCBT_long_double) || ccqt_bt_eq(l->type, CCBT_float128)){
        // Perform arithmetic in the target format, avoiding double rounding.
        #define FLOAT_OPS(prefix, a, b, field) \
            switch((unsigned)op){ \
                case CC_EXPR_ADD: out->field = prefix##_add(a,b); return 0; \
                case CC_EXPR_SUB: out->field = prefix##_sub(a,b); return 0; \
                case CC_EXPR_MUL: out->field = prefix##_mul(a,b); return 0; \
                case CC_EXPR_DIV: out->field = prefix##_div(a,b); return 0; \
                case CC_EXPR_EQ: out->integer = prefix##_eq(a,b); return 0; \
                case CC_EXPR_NE: out->integer = !prefix##_eq(a,b); return 0; \
                case CC_EXPR_LT: out->integer = prefix##_lt(a,b); return 0; \
                case CC_EXPR_GT: out->integer = prefix##_lt(b,a); return 0; \
                case CC_EXPR_LE: out->integer = prefix##_le(a,b); return 0; \
                case CC_EXPR_GE: out->integer = prefix##_le(b,a); return 0; \
                default: return CC_NOT_CONSTANT_ERROR; \
            }
        if(ccqt_bt_eq(l->type, CCBT_long_double) && cc_target(p)->long_double_format == CC_LONG_DOUBLE_X87){
            FLOAT_OPS(ci_float80, l->x87, r->x87, x87);
        }
        if(ccqt_bt_eq(l->type, CCBT_long_double) && cc_target(p)->long_double_format == CC_LONG_DOUBLE_BINARY64){
            // Binary64 operands/results are rounded by the host double operations.
            switch((unsigned)op){
                case CC_EXPR_ADD: out->double_ = l->double_ + r->double_; return 0;
                case CC_EXPR_SUB: out->double_ = l->double_ - r->double_; return 0;
                case CC_EXPR_MUL: out->double_ = l->double_ * r->double_; return 0;
                case CC_EXPR_DIV: out->double_ = l->double_ / r->double_; return 0;
                default: break;
            }
        }
        CiFloat128 a = cc_eval_quad(p,l), b = cc_eval_quad(p,r);
        FLOAT_OPS(ci_float128, a, b, quad);
        #undef FLOAT_OPS
    }
    CiUint128 a = cc_eval_u128(p,l), b = cc_eval_u128(p,r), z = ci_uint128_from_uint64(0), u = z;
    CiInt128 sa = ci_int128_from_uint128(a), sb = ci_int128_from_uint128(b);
    _Bool uns = ccqt_is_unsigned(l->type, !cc_target(p)->char_is_signed);
    _Bool an = ci_uint128_hi(a)>>63, bn = ci_uint128_hi(b)>>63;
    switch((unsigned)op){
        case CC_EXPR_ADD:
            u = ci_uint128_add(a,b);
            if(!uns && an == bn && (ci_uint128_hi(u)>>63) != an) return CC_OVERFLOW_ERROR;
            break;
        case CC_EXPR_SUB:
            u = ci_uint128_sub(a,b);
            if(!uns && an != bn && (ci_uint128_hi(u)>>63) != an) return CC_OVERFLOW_ERROR;
            break;
        case CC_EXPR_MUL:{
            if(!uns){
                CiUint128 aa = an ? ci_uint128_sub(z,a) : a, bb = bn ? ci_uint128_sub(z,b) : b;
                CiUint128 limit = ci_uint128_shl(ci_uint128_from_uint64(1),127);
                if(an == bn) limit = ci_uint128_sub(limit,ci_uint128_from_uint64(1));
                if(ci_uint128_nonzero(bb) && ci_uint128_gt(aa,ci_uint128_div(limit,bb))) return CC_OVERFLOW_ERROR;
            }
            u = ci_uint128_mul(a,b); break;
        }
        case CC_EXPR_DIV: case CC_EXPR_MOD:
            if(!ci_uint128_nonzero(b)) return CC_OVERFLOW_ERROR;
            if(!uns && ci_uint128_eq(a,ci_uint128_shl(ci_uint128_from_uint64(1),127))
                && ci_uint128_eq(b,ci_uint128_sub(z,ci_uint128_from_uint64(1)))) return CC_OVERFLOW_ERROR;
            if(uns) u = op == CC_EXPR_DIV ? ci_uint128_div(a,b) : ci_uint128_mod(a,b);
            else u = ci_uint128_from_int128(op == CC_EXPR_DIV ? ci_int128_div(sa,sb) : ci_int128_mod(sa,sb));
            break;
        case CC_EXPR_BITAND: u = ci_uint128_and(a,b); break;
        case CC_EXPR_BITOR: u = ci_uint128_or(a,b); break;
        case CC_EXPR_BITXOR: u = ci_uint128_xor(a,b); break;
        case CC_EXPR_LSHIFT: case CC_EXPR_RSHIFT:
            if(ci_uint128_hi(b) || ci_uint128_lo(b)>=128) return CC_OVERFLOW_ERROR;
            if(op == CC_EXPR_LSHIFT) u = ci_uint128_shl(a,ci_uint128_lo(b));
            else if(uns) u = ci_uint128_shr(a,ci_uint128_lo(b));
            else u = ci_uint128_from_int128(ci_int128_shr(sa,ci_uint128_lo(b)));
            break;
        case CC_EXPR_EQ: out->integer = ci_uint128_eq(a,b); return 0;
        case CC_EXPR_NE: out->integer = ci_uint128_ne(a,b); return 0;
        case CC_EXPR_LT: out->integer = uns ? ci_uint128_lt(a,b) : ci_int128_lt(sa,sb); return 0;
        case CC_EXPR_GT: out->integer = uns ? ci_uint128_gt(a,b) : ci_int128_gt(sa,sb); return 0;
        case CC_EXPR_LE: out->integer = uns ? ci_uint128_le(a,b) : ci_int128_le(sa,sb); return 0;
        case CC_EXPR_GE: out->integer = uns ? ci_uint128_ge(a,b) : ci_int128_ge(sa,sb); return 0;
        default: return CC_NOT_CONSTANT_ERROR;
    }
    out->uinteger128 = u;
    return 0;
}

static
int
cc_eval_to_i(CcParser* p, CcExpr* v, int64_t* out){
    if(ccqt_kind(v->type) == CC_POINTER){
        *out = (int64_t)v->uinteger;
        return 0;
    }
    if(cc_eval_wide(v->type)){
        CcExpr converted = {.type = ccqt_basic(CCBT_long_long)};
        int err = cc_eval_wide_cast(p, v, &converted);
        if(!err) *out = converted.integer;
        return err;
    }
    (void)p;
    CcQualType t = v->type;
    while(ccqt_kind(t) == CC_ENUM)
        t = ccqt_as_enum(t)->underlying;
    if(!ccqt_is_basic(t)) return CC_NOT_CONSTANT_ERROR;
    CcBasicTypeKind k = t.basic.kind;
    switch(k){
        DRP_CASES_EXHAUSTED;
        case CCBT_double:{
            double d = v->double_;
            if(d != d || d >= 9223372036854775808.0 || d < -9223372036854775808.0) return CC_NOT_CONSTANT_ERROR;
            *out = (int64_t)d;
            return 0;
        }
        case CCBT_float:{
            float f = v->float_;
            if(f != f || f >= 9223372036854775808.0f || f < -9223372036854775808.0f) return CC_NOT_CONSTANT_ERROR;
            *out = (int64_t)f;
            return 0;
        }
        case CCBT__Any:{
            return CC_UNIMPLEMENTED_ERROR;
        }
        case CCBT_char:
        case CCBT_bool:
        case CCBT_signed_char:
        case CCBT_short:
        case CCBT_int:
        case CCBT_long:
        case CCBT_long_long:
        case CCBT_unsigned_char:
        case CCBT_unsigned_short:
        case CCBT_unsigned:
        case CCBT_unsigned_long:
        case CCBT_unsigned_long_long:
            *out = v->integer;
            return 0;
        case CCBT_float16:
        case CCBT_long_double:
        case CCBT_float128:
        case CCBT_int128:
        case CCBT_unsigned_int128:
        case CCBT_float_complex:
        case CCBT_double_complex:
        case CCBT_long_double_complex:
            return CC_UNIMPLEMENTED_ERROR;
        case CCBT_INVALID:
        case CCBT_void:
        case CCBT_nullptr_t:
        case CCBT__Type:
        case CCBT_COUNT:
            return CC_UNREACHABLE_ERROR;
    }
}

static
int
cc_eval_to_u(CcParser* p, CcExpr* v, uint64_t* out){
    if(ccqt_kind(v->type) == CC_POINTER){
        *out = v->uinteger;
        return 0;
    }
    if(cc_eval_wide(v->type)){
        CcExpr converted = {.type = ccqt_basic(CCBT_unsigned_long_long)};
        int err = cc_eval_wide_cast(p, v, &converted);
        if(!err) *out = converted.uinteger;
        return err;
    }
    (void)p;
    CcQualType t = v->type;
    while(ccqt_kind(t) == CC_ENUM)
        t = ccqt_as_enum(t)->underlying;
    if(!ccqt_is_basic(t)) return CC_NOT_CONSTANT_ERROR;
    CcBasicTypeKind k = t.basic.kind;
    switch(k){
        DRP_CASES_EXHAUSTED;
        case CCBT_double:{
            double d = v->double_;
            if(d != d || d < 0.0 || d >= 18446744073709551616.0) return CC_OVERFLOW_ERROR;
            *out = (uint64_t)d;
            return 0;
        }
        case CCBT_float:{
            float f = v->float_;
            if(f != f || f < 0.0f || f >= 18446744073709551616.0f) return CC_OVERFLOW_ERROR;
            *out = (uint64_t)f;
            return 0;
        }
        case CCBT__Any:
            return CC_UNIMPLEMENTED_ERROR;
        case CCBT_char:
        case CCBT_bool:
        case CCBT_signed_char:
        case CCBT_short:
        case CCBT_int:
        case CCBT_long:
        case CCBT_long_long:
        case CCBT_unsigned_char:
        case CCBT_unsigned_short:
        case CCBT_unsigned:
        case CCBT_unsigned_long:
        case CCBT_unsigned_long_long:
            *out = v->uinteger;
            return 0;
        case CCBT_float16:
        case CCBT_long_double:
        case CCBT_float128:
        case CCBT_int128:
        case CCBT_unsigned_int128:
        case CCBT_float_complex:
        case CCBT_double_complex:
        case CCBT_long_double_complex:
            return CC_UNIMPLEMENTED_ERROR;
        case CCBT_INVALID:
        case CCBT_void:
        case CCBT_nullptr_t:
        case CCBT__Type:
        case CCBT_COUNT:
            return CC_UNREACHABLE_ERROR;
    }
}

static
int
cc_eval_to_f(CcParser* p, CcExpr* v, float* out){
    if(cc_eval_wide(v->type)){
        CcExpr converted = {.type = ccqt_basic(CCBT_float)};
        int err = cc_eval_wide_cast(p, v, &converted);
        if(!err) *out = converted.float_;
        return err;
    }
    CcQualType t = v->type;
    while(ccqt_kind(t) == CC_ENUM)
        t = ccqt_as_enum(t)->underlying;
    if(!ccqt_is_basic(t)) return CC_NOT_CONSTANT_ERROR;
    CcBasicTypeKind k = t.basic.kind;
    switch(k){
        DRP_CASES_EXHAUSTED;
        case CCBT_double:{
            double d = v->double_;
            if(d != d) return CC_NOT_CONSTANT_ERROR;
            if(d > (double)FLT_MAX || d < -(double)FLT_MAX) return CC_NOT_CONSTANT_ERROR;
            float f = (float)d;
            if((double)f != d) return CC_NOT_CONSTANT_ERROR;
            *out = f;
            return 0;
        }
        case CCBT_float:
            *out = v->float_;
            return 0;
        case CCBT_char:
            if(cc_target(p)->char_is_signed)
                goto signed_;
            goto unsigned_;
        case CCBT_bool:
        case CCBT_signed_char:
        case CCBT_short:
        case CCBT_int:
        case CCBT_long:
        case CCBT_long_long:
            signed_:;{
            float f = (float)v->integer;
            if(f >= 9223372036854775808.0f || f < -9223372036854775808.0f) return CC_NOT_CONSTANT_ERROR;
            if((int64_t)f != v->integer) return CC_NOT_CONSTANT_ERROR;
            *out = f;
            return 0;
        }
        case CCBT_unsigned_char:
        case CCBT_unsigned_short:
        case CCBT_unsigned:
        case CCBT_unsigned_long:
        case CCBT_unsigned_long_long:
            unsigned_:;{
            float f = (float)v->uinteger;
            if(f < 0.0f || f >= 18446744073709551616.0f) return CC_NOT_CONSTANT_ERROR;
            if((uint64_t)f != v->uinteger) return CC_NOT_CONSTANT_ERROR;
            *out = f;
            return 0;
        }
        case CCBT__Any:
            return CC_UNIMPLEMENTED_ERROR;
        case CCBT_int128:
        case CCBT_unsigned_int128:
        case CCBT_float16:
        case CCBT_long_double:
        case CCBT_float128:
        case CCBT_float_complex:
        case CCBT_double_complex:
        case CCBT_long_double_complex:
            return CC_UNIMPLEMENTED_ERROR;
        case CCBT_nullptr_t:
        case CCBT_INVALID:
        case CCBT_void:
        case CCBT__Type:
        case CCBT_COUNT:
            return CC_UNREACHABLE_ERROR;
    }
}

static
int
cc_eval_to_d(CcParser* p, CcExpr* v, double* out){
    if(cc_eval_wide(v->type)){
        CcExpr converted = {.type = ccqt_basic(CCBT_double)};
        int err = cc_eval_wide_cast(p, v, &converted);
        if(!err) *out = converted.double_;
        return err;
    }
    CcQualType t = v->type;
    while(ccqt_kind(t) == CC_ENUM)
        t = ccqt_as_enum(t)->underlying;
    if(!ccqt_is_basic(t)) return CC_NOT_CONSTANT_ERROR;
    CcBasicTypeKind k = t.basic.kind;
    switch(k){
        DRP_CASES_EXHAUSTED;
        case CCBT_double:
            *out = v->double_;
            return 0;
        case CCBT_float:
            *out = (double)v->float_;
            return 0;
        case CCBT_char:
            if(cc_target(p)->char_is_signed)
                goto signed_;
            goto unsigned_;
        case CCBT_bool:
        case CCBT_signed_char:
        case CCBT_short:
        case CCBT_int:
        case CCBT_long:
        case CCBT_long_long:
            signed_:;{
            double d = (double)v->integer;
            if(d >= 9223372036854775808.0 || d < -9223372036854775808.0) return CC_NOT_CONSTANT_ERROR;
            if((int64_t)d != v->integer) return CC_NOT_CONSTANT_ERROR;
            *out = d;
            return 0;
        }
        case CCBT_unsigned_char:
        case CCBT_unsigned_short:
        case CCBT_unsigned:
        case CCBT_unsigned_long:
        case CCBT_unsigned_long_long:
            unsigned_:;{
            double d = (double)v->uinteger;
            if(d < 0.0 || d >= 18446744073709551616.0) return CC_NOT_CONSTANT_ERROR;
            if((uint64_t)d != v->uinteger) return CC_NOT_CONSTANT_ERROR;
            *out = d;
            return 0;
        }
        case CCBT_int128:
        case CCBT_unsigned_int128:
            return CC_UNIMPLEMENTED_ERROR;
        case CCBT__Any:
            return CC_UNIMPLEMENTED_ERROR;
        case CCBT_float16:
        case CCBT_long_double:
        case CCBT_float128:
        case CCBT_float_complex:
        case CCBT_double_complex:
        case CCBT_long_double_complex:
            return CC_UNIMPLEMENTED_ERROR;
        case CCBT_nullptr_t:
        case CCBT_INVALID:
        case CCBT_void:
        case CCBT__Type:
        case CCBT_COUNT:
            return CC_UNREACHABLE_ERROR;
    }
}

static
void
cc_eval_truncate(CcParser* p, CcExpr* node){
    CcQualType t = node->type;
    while(ccqt_kind(t) == CC_ENUM)
        t = ccqt_as_enum(t)->underlying;
    if(!ccqt_is_basic(t)) return;
    CcBasicTypeKind k = t.basic.kind;
    if(!ccbt_is_integer(k)) return;
    uint32_t sz;
    if(cc_sizeof_as_uint(p, t, node->loc, &sz)) return;
    if(sz >= 8) return;
    uint32_t bits = sz * 8;
    uint64_t mask = ((uint64_t)1 << bits) - 1;
    if(ccbt_is_unsigned(k, !cc_target(p)->char_is_signed)){
        node->uinteger &= mask;
    }
    else {
        int64_t val = node->integer & (int64_t)mask;
        if(val & ((int64_t)1 << (bits - 1)))
            val |= ~(int64_t)mask;
        node->integer = val;
    }
}

static int cc_eval_object_scalar(CcEvalCtx*, CcExpr*, uint32_t, CcQualType, SrcLoc, const unsigned char*_Nullable, CcExpr*_Nullable*_Nonnull);

// Shift operands are promoted independently; the count may be wider than
// the value being shifted. Check it before any width-specific narrowing.
static
int
cc_eval_check_shift_count(CcParser* p, CcQualType left, CcExpr* right){
    uint32_t size;
    int err = cc_sizeof_as_uint(p, left, right->loc, &size);
    if(err) return err;
    CiUint128 count = cc_eval_u128(p, right);
    return ci_uint128_hi(count) || ci_uint128_lo(count) >= (uint64_t)size * 8 ? CC_OVERFLOW_ERROR : 0;
}

static
int
cc_eval_check_any_view(CcEvalCtx* ctx, CcExpr* e){
    CcParser* p = ctx->parser;
    if(!ccqt_bt_eq(e->values[0]->type, CCBT__Any)
    || cc_field_path_count(e->field_path) != 1
    || cc_field_path_component(e->field_path, 0) != 1) return 0;
    CcExpr* tag;
    int err = cc_eval_object_scalar(ctx, e->values[0], 0, ccqt_basic(CCBT__Type), e->loc, NULL, &tag);
    if(err) return err;
    _Bool matches = tag->type_value.bits && tag->type_value.unqual == e->type.unqual;
    cc_release_expr(p, tag);
    return matches ? 0 : CC_NOT_CONSTANT_ERROR;
}

static
_Bool
cc_linktime_const_variable(CcExpr* e){
    CcQualType type = e->type;
    while(ccqt_kind(type) == CC_ARRAY) type = ccqt_as_array(type)->element;
    return e->var->initializer && (e->var->constexpr_ || type.is_const)
        && !type.is_volatile && !type.is_atomic;
}

// Addresses remain symbolic until an operation cancels the storage identity.
// In particular, offsets of different subobjects of one variable may be
// subtracted: this is intentionally more permissive than C's array-only rule.
typedef struct CcEvalAddress CcEvalAddress;
struct CcEvalAddress {
    enum { CC_EVAL_ABSOLUTE, CC_EVAL_VAR, CC_EVAL_FUNC, CC_EVAL_LITERAL } kind;
    const void* _Nullable symbol;
    int64_t offset;
    uint64_t literal_size;
    CcQualType literal_type;
    int64_t range_start;
    uint64_t range_size;
    _Bool has_range;
};

static
_Bool
cc_eval_address_nonnull(CcEvalAddress address){
    if(address.kind == CC_EVAL_ABSOLUTE) return 0;
    if(!address.offset) return 1;
    // An address within its known object, including one-past, cannot be null.
    return address.has_range && address.range_start >= 0
        && address.offset >= address.range_start
        && (uint64_t)address.offset - (uint64_t)address.range_start <= address.range_size;
}

static _Bool
cc_eval_address_type(CcQualType type){
    return ccqt_kind(type) == CC_POINTER || ccqt_kind(type) == CC_BLOCK_POINTER
        || ccqt_bt_eq(type, CCBT_nullptr_t);
}

static
_Bool
cc_eval_pointer_binary_expr(CcExpr* e){
    switch((unsigned)e->kind){
        case CC_EXPR_EQ: case CC_EXPR_NE:
            return cc_eval_address_type(e->lhs->type) && cc_eval_address_type(e->values[0]->type);
        case CC_EXPR_SUB:
        case CC_EXPR_LT: case CC_EXPR_LE: case CC_EXPR_GT: case CC_EXPR_GE:
            return ccqt_kind(e->lhs->type) == CC_POINTER
                && ccqt_kind(e->values[0]->type) == CC_POINTER;
        default: return 0;
    }
}

static int cc_eval_address(CcEvalCtx*, CcExpr*, _Bool, unsigned, CcEvalAddress*);
static int cc_eval_slice(CcEvalCtx*, CcExpr*, unsigned, uint64_t*, CcEvalAddress*);
static int cc_eval_object_view_select(CcEvalCtx*, CcExpr*, _Bool, _Bool*, CcExpr*_Nullable*_Nonnull);
static int cc_eval_object_view_range(CcEvalCtx*, CcExpr*, uint64_t, CcEvalObjectView*);
static int cc_check_linktime_expr(CcParser*, CcExpr*, _Bool, unsigned);

static
_Bool
cc_eval_object_access(CcExpr* e){
    return !cc_expr_field_bit_width(e) && (e->kind == CC_EXPR_DOT || e->kind == CC_EXPR_ARROW
        || e->kind == CC_EXPR_SUBSCRIPT || e->kind == CC_EXPR_DEREF);
}

static
int
cc_eval_comma_source(CcEvalCtx* ctx, CcExpr* e, _Bool storage, CcExpr*_Nonnull*_Nonnull out){
    CcExpr* assign = e->lhs;
    CcExpr* ref = e->values[0];
    if(assign->kind == CC_EXPR_ASSIGN && assign->lhs->kind == CC_EXPR_VARIABLE
        && ref->kind == CC_EXPR_VARIABLE && assign->lhs->var == ref->var
        && ref->var->name == nil_atom && !ref->var->automatic){
        *out = storage ? ref : assign->values[0];
        return 0;
    }
    CcExpr* discard;
    int err = cc_eval_expr(ctx, assign, &discard);
    if(err == CC_NOT_CONSTANT_ERROR){
        // A discarded relocation needs no numeric address. Still validate
        // the expression so comma cannot hide effects or invalid arithmetic.
        err = cc_check_linktime_expr(ctx->parser, assign, 0, ctx->evaluation_depth);
        if(err) return err;
    }
    else {
        if(err) return err;
        cc_release_expr(ctx->parser, discard);
    }
    *out = ref;
    return 0;
}

// Resolve storage-preserving wrappers once for all kinds of object reads.
// The result borrows the original expression, preserving cycle identity.
static
int
cc_eval_object_source(CcEvalCtx* ctx, CcExpr* e, unsigned depth, CcExpr*_Nonnull*_Nonnull out){
    for(;;){
        if(depth++ >= 256 || (e->is_lvalue && (e->type.is_volatile || e->type.is_atomic)))
            return CC_NOT_CONSTANT_ERROR;
        if(e->kind == CC_EXPR_VARIABLE){
            if(!e->var->initializer || !(e->var->constexpr_ || (ctx->allow_const && cc_linktime_const_variable(e))))
                return CC_NOT_CONSTANT_ERROR;
            e = e->var->initializer;
        }
        else if(e->kind == CC_EXPR_OBJECT_VIEW
            || (e->kind == CC_EXPR_CAST && e->type.unqual == e->lhs->type.unqual))
            e = e->lhs;
        else if(e->kind == CC_EXPR_TERNARY){
            _Bool truth;
            int err = cc_eval_truthy(ctx, e->lhs, &truth);
            if(err) return err;
            e = e->values[truth ? 0 : 1];
        }
        else if(e->kind == CC_EXPR_COMMA){
            int err = cc_eval_comma_source(ctx, e, 0, &e);
            if(err) return err;
        }
        else { *out = e; return 0; }
    }
}

static
_Bool
cc_eval_slice_expr(CcExpr* e){
    return ccqt_kind(e->type) == CC_SLICE
        && (e->kind == CC_EXPR_SLICE || e->kind == CC_EXPR_SLICE_LO || e->kind == CC_EXPR_SLICE_HI
            || e->kind == CC_EXPR_SLICE_ALL || (e->kind == CC_EXPR_CAST && e->type.unqual != e->lhs->type.unqual));
}

// Select a pointer-sized subobject from a constant initializer without turning
// a symbolic pointer into host bytes. The last overlapping initializer wins;
// partial overwrites cannot retain the original pointer's symbolic identity.
static
int
cc_eval_object_address(CcEvalCtx* ctx, CcExpr* e, uint64_t offset, uint32_t size, unsigned depth, CcEvalAddress* out){
    if(depth > 256 || e->type.is_volatile || e->type.is_atomic) return CC_NOT_CONSTANT_ERROR;
    CcParser* p = ctx->parser;
    uint32_t object_size;
    int err = cc_sizeof_as_uint(p, e->type, e->loc, &object_size);
    if(err) return err;
    if(offset > object_size || size > object_size - offset) return CC_NOT_CONSTANT_ERROR;
    CcExpr* source;
    err = cc_eval_object_source(ctx, e, depth, &source);
    if(err) return err;
    if(source != e) return cc_eval_object_address(ctx, source, offset, size, depth + 1, out);
    if(e->kind == CC_EXPR_TYPE_INTROSPECTION){
        CcExpr* value;
        err = cc_eval_expr(ctx, e, &value);
        if(err) return err;
        err = cc_eval_object_address(ctx, value, offset, size, depth + 1, out);
        cc_release_expr(p, value);
        return err;
    }
    if(cc_eval_object_access(e)){
        CcExpr storage = {0};
        CcEvalObjectView view = {.object=e, .storage=&storage, .depth=depth};
        err = cc_eval_object_view_range(ctx, e, offset, &view);
        if(err) return err;
        err = cc_eval_object_address(ctx, view.object, view.byte_offset, size, depth + 1, out);
        cc_field_path_free(cc_allocator(p), view.path);
        return err;
    }
    if(e->kind == CC_EXPR_CAST && ccqt_bt_eq(e->type, CCBT__Any)){
        if(ccqt_bt_eq(e->lhs->type, CCBT__Any))
            return cc_eval_object_address(ctx, e->lhs, offset, size, depth + 1, out);
        if(offset < offsetof(CiRtAny, payload)) return CC_NOT_CONSTANT_ERROR;
        return cc_eval_object_address(ctx, e->lhs, offset - offsetof(CiRtAny, payload), size, depth + 1, out);
    }
    if(cc_eval_slice_expr(e)){
        uint32_t ptr_size = cc_target(p)->sizeof_[CCBT_nullptr_t];
        if(offset != ptr_size || size != ptr_size) return CC_NOT_CONSTANT_ERROR;
        uint64_t count;
        return cc_eval_slice(ctx, e, depth + 1, &count, out);
    }
    if(e->kind == CC_EXPR_INIT_LIST || e->kind == CC_EXPR_COMPOUND_LITERAL){
        CcInitList* list = e->init_list;
        for(uint32_t i = list->count; i; i--){
            CcInitEntry* entry = &list->entries[i-1];
            uint64_t entry_offset;
            err = cc_field_path_resolve(cc_target(p), e->type, entry->path, &entry_offset);
            if(err) return err;
            if(!entry->value) continue;
            uint32_t entry_size;
            err = cc_sizeof_as_uint(p, entry->value->type, entry->value->loc, &entry_size);
            if(err) return err;
            uint64_t start = entry_offset;
            uint64_t end = start + entry_size;
            if(start >= offset + size || end <= offset) continue;
            if(cc_field_path_bit_width(e->type, entry->path) || start > offset || end < offset + size)
                return CC_NOT_CONSTANT_ERROR;
            return cc_eval_object_address(ctx, entry->value, offset - start, size, depth + 1, out);
        }
        *out = (CcEvalAddress){0};
        return 0;
    }
    if(!offset && size == object_size
        && cc_eval_address_type(e->type))
        return cc_eval_address(ctx, e, 0, depth + 1, out);
    return CC_NOT_CONSTANT_ERROR;
}

static
int
cc_eval_slice(CcEvalCtx* ctx, CcExpr* e, unsigned depth, uint64_t* count, CcEvalAddress* address){
    if(depth > 256 || e->type.is_volatile || e->type.is_atomic) return CC_NOT_CONSTANT_ERROR;
    CcParser* p = ctx->parser;
    uint32_t ptr_size = cc_target(p)->sizeof_[CCBT_nullptr_t];
    if(!cc_eval_slice_expr(e)){
        CcExpr* length;
        int err = cc_eval_object_scalar(ctx, e, 0, ccqt_basic(cc_target(p)->size_type), e->loc, NULL, &length);
        if(err) return err;
        *count = length->uinteger;
        cc_release_expr(p, length);
        return cc_eval_object_address(ctx, e, ptr_size, ptr_size, depth + 1, address);
    }
    CcExpr* base = e->lhs;
    CcTypeKind kind = ccqt_kind(base->type);
    uint64_t length = 0;
    _Bool have_length = kind == CC_ARRAY || kind == CC_SLICE;
    int err;
    if(kind == CC_SLICE){
        err = cc_eval_slice(ctx, base, depth + 1, &length, address);
        if(err) return err;
    }
    else {
        err = cc_eval_address(ctx, base, kind == CC_ARRAY, depth + 1, address);
        if(err) return err;
        if(kind == CC_ARRAY){
            if(ccqt_as_array(base->type)->is_incomplete) return CC_NOT_CONSTANT_ERROR;
            length = ccqt_as_array(base->type)->length;
        }
    }
    if(length > INT64_MAX) return CC_NOT_CONSTANT_ERROR;
    int64_t lo = 0, hi = (int64_t)length;
    if(e->kind == CC_EXPR_SLICE || e->kind == CC_EXPR_SLICE_LO){
        err = cc_eval_integer(ctx, e->values[0], &lo);
        if(err) return err;
    }
    if(e->kind == CC_EXPR_SLICE || e->kind == CC_EXPR_SLICE_HI){
        err = cc_eval_integer(ctx, e->values[e->kind == CC_EXPR_SLICE ? 1 : 0], &hi);
        if(err) return err;
    }
    else if(!have_length) return CC_NOT_CONSTANT_ERROR;
    if(lo < 0 || hi < lo || (have_length && (uint64_t)hi > length)) return CC_NOT_CONSTANT_ERROR;
    uint32_t element_size;
    err = cc_sizeof_as_uint(p, ccqt_as_slice(e->type)->pointee, e->loc, &element_size);
    if(err) return err;
    int64_t delta;
    if(mul_overflow(lo, (int64_t)element_size, &delta) || add_overflow(address->offset, delta, &address->offset))
        return CC_OVERFLOW_ERROR;
    *count = (uint64_t)(hi - lo);
    return 0;
}

static
int
cc_eval_address_value(CcEvalCtx* ctx, CcExpr* e, CcEvalAddress* out){
    if(ctx->evaluation_depth >= 256 || e->type.is_volatile || e->type.is_atomic)
        return CC_NOT_CONSTANT_ERROR;
    ctx->evaluation_depth++;
    _Bool handled;
    CcExpr* selected = NULL;
    int err = cc_eval_object_view_select(ctx, e, 1, &handled, &selected);
    if(err || handled){
        if(!err) err = cc_eval_address(ctx, selected, 0, 0, out);
        if(selected) cc_release_expr(ctx->parser, selected);
        ctx->evaluation_depth--;
        return err;
    }
    CcExpr* value;
    err = cc_eval_expr(ctx, e, &value);
    if(!err){
        if(value->kind == CC_EXPR_VALUE
            && (ccqt_kind(value->type) == CC_POINTER || ccqt_bt_eq(value->type, CCBT_nullptr_t)))
            *out = (CcEvalAddress){.offset = (int64_t)value->uinteger};
        else err = CC_NOT_CONSTANT_ERROR;
        cc_release_expr(ctx->parser, value);
    }
    if(err == CC_NOT_CONSTANT_ERROR){
        uint32_t size;
        err = cc_sizeof_as_uint(ctx->parser, e->type, e->loc, &size);
        if(!err) err = cc_eval_object_address(ctx, e, 0, size, 0, out);
    }
    ctx->evaluation_depth--;
    return err;
}

static
int
cc_eval_address(CcEvalCtx* ctx, CcExpr* e, _Bool lvalue, unsigned depth, CcEvalAddress* out){
    if(depth > 256) return CC_NOT_CONSTANT_ERROR;
    CcParser* p = ctx->parser;
    int err;
    switch((unsigned)e->kind){
        case CC_EXPR_VARIABLE:
            if(lvalue || ccqt_kind(e->type) == CC_ARRAY){
                if(e->var->automatic || e->var->thread_local_) return CC_NOT_CONSTANT_ERROR;
                *out = (CcEvalAddress){.kind = CC_EVAL_VAR, .symbol = e->var};
                uint32_t size;
                if(!cc_type_sizeof_complete(cc_target(p), e->type, &size)){
                    out->has_range = 1;
                    out->range_size = size;
                }
                return 0;
            }
            if(e->var->initializer && !e->type.is_volatile && !e->type.is_atomic
                && (e->var->constexpr_ || (ctx->allow_const && cc_linktime_const_variable(e))))
                return cc_eval_address(ctx, e->var->initializer, 0, depth + 1, out);
            return CC_NOT_CONSTANT_ERROR;
        case CC_EXPR_FUNCTION:
            *out = (CcEvalAddress){.kind = CC_EVAL_FUNC, .symbol = e->func};
            return 0;
        case CC_EXPR_VALUE:
            if(ccqt_kind(e->type) == CC_ARRAY && e->text){
                uint32_t size;
                err = cc_sizeof_as_uint(p, e->type, e->loc, &size);
                if(err) return err;
                *out = (CcEvalAddress){.kind = CC_EVAL_LITERAL, .symbol = e->text,
                    .literal_size = size, .literal_type=e->type,
                    .has_range=1, .range_size=size};
                return 0;
            }
            if(!lvalue && cc_eval_address_type(e->type)){
                *out = (CcEvalAddress){.offset = (int64_t)e->uinteger};
                return 0;
            }
            return CC_NOT_CONSTANT_ERROR;
        case CC_EXPR_INIT_LIST: case CC_EXPR_COMPOUND_LITERAL:
            if(!lvalue && cc_eval_address_type(e->type)){
                uint32_t size;
                err = cc_sizeof_as_uint(p, e->type, e->loc, &size);
                if(err) return err;
                return cc_eval_object_address(ctx, e, 0, size, depth+1, out);
            }
            return CC_NOT_CONSTANT_ERROR;
        case CC_EXPR_ADDR:
            return cc_eval_address(ctx, e->lhs, 1, depth + 1, out);
        case CC_EXPR_DEREF:
            if(!lvalue && cc_eval_address_type(e->type))
                return cc_eval_address_value(ctx, e, out);
            return lvalue || ccqt_kind(e->type) == CC_ARRAY
                ? cc_eval_address(ctx, e->lhs, 0, depth + 1, out) : CC_NOT_CONSTANT_ERROR;
        case CC_EXPR_DOT: case CC_EXPR_ARROW:
            if(!lvalue && cc_eval_address_type(e->type))
                return cc_eval_address_value(ctx, e, out);
            if((!lvalue && ccqt_kind(e->type) != CC_ARRAY) || cc_expr_field_bit_width(e))
                return CC_NOT_CONSTANT_ERROR;
            err = cc_eval_address(ctx, e->values[0], e->kind == CC_EXPR_DOT, depth + 1, out);
            if(err) return err;
            if(cc_expr_field_offset(cc_target(p), e) > INT64_MAX
                || add_overflow(out->offset, (int64_t)cc_expr_field_offset(cc_target(p), e), &out->offset))
                return CC_OVERFLOW_ERROR;
            if(!(ccqt_kind(e->type) == CC_ARRAY && ccqt_as_array(e->type)->is_incomplete)){
                out->has_range = 1;
                out->range_start = out->offset;
                uint32_t size;
                err = cc_sizeof_as_uint(p, e->type, e->loc, &size);
                if(err) return err;
                out->range_size = size;
            }
            return 0;
        case CC_EXPR_SUBSCRIPT:{
            if(!lvalue && (ccqt_kind(e->type) == CC_POINTER || ccqt_bt_eq(e->type, CCBT_nullptr_t)))
                return cc_eval_address_value(ctx, e, out);
            if(!lvalue && ccqt_kind(e->type) != CC_ARRAY) return CC_NOT_CONSTANT_ERROR;
            int64_t index, offset;
            err = cc_eval_integer(ctx, e->values[0], &index);
            if(err) return err;
            uint32_t size;
            err = cc_sizeof_as_uint(p, e->type, e->loc, &size);
            if(err) return err;
            if(ccqt_kind(e->lhs->type) == CC_SLICE){
                uint64_t count;
                err = cc_eval_slice(ctx, e->lhs, depth + 1, &count, out);
                if(!err && (index < 0 || (uint64_t)index > count)) err = CC_NOT_CONSTANT_ERROR;
            }
            else err = cc_eval_address(ctx, e->lhs, ccqt_kind(e->lhs->type) == CC_ARRAY, depth + 1, out);
            if(err) return err;
            if(ccqt_kind(e->lhs->type) == CC_ARRAY && !ccqt_as_array(e->lhs->type)->is_incomplete){
                out->has_range = 1;
                out->range_start = out->offset;
                uint32_t array_size;
                err = cc_sizeof_as_uint(p, e->lhs->type, e->loc, &array_size);
                if(err) return err;
                out->range_size = array_size;
            }
            if(mul_overflow(index, (int64_t)size, &offset) || add_overflow(out->offset, offset, &out->offset))
                return CC_OVERFLOW_ERROR;
            return 0;
        }
        case CC_EXPR_CAST:
            if(!cc_eval_address_type(e->type)) return CC_NOT_CONSTANT_ERROR;
            if(cc_eval_address_type(e->lhs->type) || ccqt_kind(e->lhs->type) == CC_ARRAY
                || ccqt_kind(e->lhs->type) == CC_FUNCTION)
                return cc_eval_address(ctx, e->lhs, ccqt_kind(e->lhs->type) == CC_ARRAY, depth + 1, out);
            if(ccqt_is_integer(e->lhs->type)){
                CcExpr* value;
                err = cc_eval_expr(ctx, e, &value);
                if(err) return err;
                if(value->kind == CC_EXPR_VALUE && cc_eval_address_type(value->type))
                    *out = (CcEvalAddress){.offset = (int64_t)value->uinteger};
                else err = CC_NOT_CONSTANT_ERROR;
                cc_release_expr(p, value);
                return err;
            }
            return CC_NOT_CONSTANT_ERROR;
        case CC_EXPR_ADD: case CC_EXPR_SUB:{
            if(ccqt_kind(e->type) != CC_POINTER) return CC_NOT_CONSTANT_ERROR;
            CcExpr* base = e->lhs;
            CcExpr* index = e->values[0];
            if(e->kind == CC_EXPR_ADD && ccqt_kind(base->type) != CC_POINTER){
                base = e->values[0];
                index = e->lhs;
            }
            int64_t n, offset;
            err = cc_eval_integer(ctx, index, &n);
            if(err) return err;
            uint32_t size = 1;
            CcQualType pointee = ccqt_as_ptr(base->type)->pointee;
            if(!ccqt_bt_eq(pointee, CCBT_void)){
                err = cc_sizeof_as_uint(p, pointee, e->loc, &size);
                if(err) return err;
            }
            err = cc_eval_address(ctx, base, 0, depth + 1, out);
            if(err) return err;
            if(mul_overflow(n, (int64_t)size, &offset)) return CC_OVERFLOW_ERROR;
            if(e->kind == CC_EXPR_ADD)
                return add_overflow(out->offset, offset, &out->offset) ? CC_OVERFLOW_ERROR : 0;
            return sub_overflow(out->offset, offset, &out->offset) ? CC_OVERFLOW_ERROR : 0;
        }
        case CC_EXPR_TERNARY:{
            _Bool truth;
            err = cc_eval_truthy(ctx, e->lhs, &truth);
            if(err) return err;
            return cc_eval_address(ctx, e->values[truth ? 0 : 1], lvalue, depth + 1, out);
        }
        case CC_EXPR_COMMA:{
            CcExpr* source;
            err = cc_eval_comma_source(ctx, e, lvalue || ccqt_kind(e->type) == CC_ARRAY, &source);
            if(err) return err;
            return cc_eval_address(ctx, source, lvalue, depth + 1, out);
        }
        default: return CC_NOT_CONSTANT_ERROR;
    }
}

static
int
cc_eval_pointer_binary(CcEvalCtx* ctx, CcExpr* e, int64_t* result){
    CcEvalAddress l, r;
    int err = cc_eval_address(ctx, e->lhs, 0, 0, &l);
    if(err) return err;
    err = cc_eval_address(ctx, e->values[0], 0, 0, &r);
    if(err) return err;
    if(l.kind != r.kind || l.symbol != r.symbol){
        // A symbol's address within its known object cannot be null. Other comparisons
        // between unrelated bases may depend on placement or literal merging.
        if(e->kind == CC_EXPR_EQ || e->kind == CC_EXPR_NE){
            _Bool left_null = l.kind == CC_EVAL_ABSOLUTE && !l.offset;
            _Bool right_null = r.kind == CC_EVAL_ABSOLUTE && !r.offset;
            if((left_null && cc_eval_address_nonnull(r))
                || (right_null && cc_eval_address_nonnull(l))){
                *result = e->kind == CC_EXPR_NE;
                return 0;
            }
        }
        return CC_NOT_CONSTANT_ERROR;
    }
    switch((unsigned)e->kind){
        case CC_EXPR_EQ: *result = l.offset == r.offset; return 0;
        case CC_EXPR_NE: *result = l.offset != r.offset; return 0;
        case CC_EXPR_LT: *result = l.kind == CC_EVAL_ABSOLUTE ? (uint64_t)l.offset < (uint64_t)r.offset : l.offset < r.offset; return 0;
        case CC_EXPR_LE: *result = l.kind == CC_EVAL_ABSOLUTE ? (uint64_t)l.offset <= (uint64_t)r.offset : l.offset <= r.offset; return 0;
        case CC_EXPR_GT: *result = l.kind == CC_EVAL_ABSOLUTE ? (uint64_t)l.offset > (uint64_t)r.offset : l.offset > r.offset; return 0;
        case CC_EXPR_GE: *result = l.kind == CC_EVAL_ABSOLUTE ? (uint64_t)l.offset >= (uint64_t)r.offset : l.offset >= r.offset; return 0;
        case CC_EXPR_SUB:{
            if(l.kind != CC_EVAL_VAR && l.kind != CC_EVAL_LITERAL) return CC_NOT_CONSTANT_ERROR;
            uint32_t size = 1;
            CcQualType pointee = ccqt_as_ptr(e->lhs->type)->pointee;
            if(!ccqt_bt_eq(pointee, CCBT_void)){
                err = cc_sizeof_as_uint(ctx->parser, pointee, e->loc, &size);
                if(err) return err;
            }
            int64_t delta;
            if(sub_overflow(l.offset, r.offset, &delta)) return CC_OVERFLOW_ERROR;
            if(!size || delta % size) return CC_NOT_CONSTANT_ERROR;
            delta /= size;
            uint32_t result_size;
            err = cc_sizeof_as_uint(ctx->parser, e->type, e->loc, &result_size);
            if(err) return err;
            if(result_size < 8){
                int64_t max = ((int64_t)1 << (result_size * 8 - 1)) - 1;
                if(delta < -max - 1 || delta > max) return CC_OVERFLOW_ERROR;
            }
            *result = delta;
            return 0;
        }
        default: return CC_NOT_CONSTANT_ERROR;
    }
}

static
int
cc_eval_symbolic_binary(CcParser* p, CcExpr* e, _Bool allow_const, int64_t* result){
    return cc_eval_pointer_binary(&(CcEvalCtx){.parser = p, .allow_const = allow_const}, e, result);
}

static
int
cc_eval_linktime_expr(CcParser* p, CcExpr* e, CcExpr*_Nullable*_Nonnull result){
    return cc_eval_expr(&(CcEvalCtx){.parser = p, .allow_const = 1}, e, result);
}

static
int
cc_eval_linktime_scalar(CcParser* p, CcExpr* e, CcExpr*_Nullable*_Nonnull result){
    CcExpr* value;
    int err = cc_eval_linktime_expr(p, e, &value);
    if(err) return err;
    if(value->kind != CC_EXPR_VALUE){
        cc_release_expr(p, value);
        return CC_NOT_CONSTANT_ERROR;
    }
    *result = value;
    return 0;
}

// Integer relocations use the same symbolic base and checked byte offset as
// pointer constants. A numeric operand may adjust a symbol, but cannot replace
// its coefficient with -1 or combine it with a second symbol.
static
int
cc_eval_integer_address(CcEvalCtx* ctx, CcExpr* e, unsigned depth, CcEvalAddress* out){
    if(depth >= 256 || !ccqt_is_integer(e->type)) return CC_NOT_CONSTANT_ERROR;
    CcParser* p = ctx->parser;
    uint32_t size;
    int err = cc_sizeof_as_uint(p, e->type, e->loc, &size);
    if(err) return err;
    if(size < cc_target(p)->sizeof_[CCBT_nullptr_t] || size > sizeof(uint64_t)) return CC_NOT_CONSTANT_ERROR;
    CcExpr* source;
    err = cc_eval_object_source(ctx, e, depth, &source);
    if(err) return err;
    if(source != e) return cc_eval_integer_address(ctx, source, depth+1, out);
    if(e->kind == CC_EXPR_CAST){
        if(cc_eval_address_type(e->lhs->type) || ccqt_kind(e->lhs->type) == CC_ARRAY
            || ccqt_kind(e->lhs->type) == CC_FUNCTION)
            return cc_eval_address(ctx, e->lhs, ccqt_kind(e->lhs->type) == CC_ARRAY, depth+1, out);
        return cc_eval_integer_address(ctx, e->lhs, depth+1, out);
    }
    if(e->kind == CC_EXPR_INIT_LIST || e->kind == CC_EXPR_COMPOUND_LITERAL){
        CcInitList* list = e->init_list;
        if(list->count == 1 && !cc_field_path_count(list->entries[0].path) && list->entries[0].value)
            return cc_eval_integer_address(ctx, list->entries[0].value, depth+1, out);
        return CC_NOT_CONSTANT_ERROR;
    }
    if(e->kind == CC_EXPR_DOT || e->kind == CC_EXPR_ARROW || e->kind == CC_EXPR_SUBSCRIPT || e->kind == CC_EXPR_DEREF){
        _Bool handled;
        CcExpr* value = NULL;
        err = cc_eval_object_view_select(ctx, e, 1, &handled, &value);
        if(!err) err = handled ? cc_eval_integer_address(ctx, (CcExpr*_Nonnull)value, depth+1, out) : CC_NOT_CONSTANT_ERROR;
        if(value) cc_release_expr(p, value);
        return err;
    }
    if(e->kind != CC_EXPR_ADD && e->kind != CC_EXPR_SUB) return CC_NOT_CONSTANT_ERROR;
    CcExpr* base = e->lhs;
    CcExpr* index = e->values[0];
    CcExpr* value;
    err = cc_eval_expr(ctx, index, &value);
    if(err == CC_NOT_CONSTANT_ERROR && e->kind == CC_EXPR_ADD){
        base = e->values[0];
        index = e->lhs;
        err = cc_eval_expr(ctx, index, &value);
    }
    if(err) return err;
    if(value->kind != CC_EXPR_VALUE || !ccqt_is_integer(value->type)){
        cc_release_expr(p, value);
        return CC_NOT_CONSTANT_ERROR;
    }
    CiUint128 bits = cc_eval_u128(p, value);
    _Bool positive = !ci_uint128_hi(bits) && ci_uint128_lo(bits) <= INT64_MAX;
    _Bool negative = !ccqt_is_unsigned(value->type, !cc_target(p)->char_is_signed)
        && ci_uint128_hi(bits) == UINT64_MAX && ci_uint128_lo(bits) > INT64_MAX;
    cc_release_expr(p, value);
    if(!positive && !negative) return CC_NOT_CONSTANT_ERROR;
    int64_t offset = (int64_t)ci_uint128_lo(bits);
    err = cc_eval_integer_address(ctx, base, depth+1, out);
    if(err) return err;
    return (e->kind == CC_EXPR_ADD ? add_overflow(out->offset, offset, &out->offset)
        : sub_overflow(out->offset, offset, &out->offset)) ? CC_OVERFLOW_ERROR : 0;
}

static int cc_eval_object_bytes(CcEvalCtx*, CcExpr*, uint32_t, uint32_t, unsigned char*, const unsigned char*_Nullable);

// Opaque values may cross an aggregate view only into a matching typed slot.
// _Any has a metadata tag and can carry either opaque class in its payload.
static
_Bool
cc_eval_opaque_region(CcParser* p, CcQualType type, uint64_t offset, uint32_t size, _Bool metadata, unsigned depth){
    if(depth >= 256 || size != cc_target(p)->sizeof_[metadata ? CCBT__Type : CCBT_nullptr_t]) return 0;
    if(metadata && ccqt_bt_eq(type, CCBT__Type)) return !offset;
    if(!metadata && cc_eval_address_type(type)) return !offset;
    if(!metadata && ccqt_kind(type) == CC_SLICE)
        return offset == cc_target(p)->sizeof_[CCBT_nullptr_t];
    if(ccqt_bt_eq(type, CCBT__Any))
        return (metadata && !offset) || offset == offsetof(CiRtAny, payload);
    CcTypeKind kind = ccqt_kind(type);
    if(kind == CC_ARRAY){
        CcArray* array = ccqt_as_array(type);
        if(array->is_incomplete || array->is_vla) return 0;
        uint32_t stride;
        if(cc_type_sizeof_complete(cc_target(p), array->element, &stride)) return 0;
        if(!stride || offset/stride >= array->length) return 0;
        return cc_eval_opaque_region(p, array->element, offset%stride, size, metadata, depth+1);
    }
    if(kind == CC_STRUCT || kind == CC_UNION){
        CcStruct* aggregate = ccqt_as_struct(type);
        for(uint32_t i = 0; i < aggregate->field_count; i++){
            CcField* field = &aggregate->fields[i];
            if(field->is_method || field->is_bitfield || offset < field->offset) continue;
            if(cc_eval_opaque_region(p, field->type, offset-field->offset, size, metadata, depth+1)) return 1;
        }
    }
    return 0;
}

static
int
cc_check_linktime_opaque_view(CcEvalCtx* ctx, CcExpr* root, uint32_t offset, uint32_t size){
    if(cc_eval_opaque_region(ctx->parser, root->type, offset, size, 1, 0)){
        CcExpr* value;
        int err = cc_eval_object_scalar(ctx, root, offset, ccqt_basic(CCBT__Type), root->loc, NULL, &value);
        if(!err) cc_release_expr(ctx->parser, value);
        if(err != CC_NOT_CONSTANT_ERROR) return err;
    }
    if(cc_eval_opaque_region(ctx->parser, root->type, offset, size, 0, 0)){
        CcEvalAddress address;
        int err = cc_eval_object_address(ctx, root, offset, size, 0, &address);
        if(err != CC_NOT_CONSTANT_ERROR) return err;
    }
    unsigned char bytes[16];
    if(size > sizeof bytes) return CC_NOT_CONSTANT_ERROR;
    return cc_eval_object_bytes(ctx, root, offset, size, bytes, NULL);
}

// Omitted members still have a typed zero value: an omitted pointer is not
// integer storage merely because its representation happens to be zero.
// A NULL root selects numeric-only checking of the unwritten bits in mask.
typedef struct CcEvalDefaultPath CcEvalDefaultPath;
struct CcEvalDefaultPath {
    const CcEvalDefaultPath*_Nullable parent;
    uint32_t index, count;
};

// The first designator reaching a union determines its initial zeroed member.
// Subsequent writes are merged separately, including writes to other members.
static
uint32_t
cc_eval_default_union_member(CcInitList* list, const CcEvalDefaultPath*_Nullable path){
    uint32_t count = path ? path->count : 0;
    for(uint32_t i = 0; i < list->count; i++){
        CcInitEntry* entry = &list->entries[i];
        if(!entry->value || cc_field_path_count(entry->path) <= count) continue;
        const CcEvalDefaultPath*_Nullable component = path;
        while(component && cc_field_path_component(entry->path, component->count-1) == component->index)
            component = component->parent;
        if(!component) return cc_field_path_component(entry->path, count);
    }
    return UINT32_MAX;
}

static
int
cc_check_linktime_default_view(CcEvalCtx* ctx, CcExpr*_Nullable root, CcQualType type,
    uint64_t offset, uint32_t size, uint32_t root_offset, const unsigned char*_Nullable mask, unsigned depth,
    CcInitList*_Nullable list, const CcEvalDefaultPath*_Nullable path){
    if(!size) return 0;
    if(mask){
        _Bool needed = 0;
        for(uint32_t i = 0; i < size; i++) needed |= mask[i] != 0;
        if(!needed) return 0;
    }
    if(depth >= 256) return CC_NOT_CONSTANT_ERROR;
    CcParser* p = ctx->parser;
    if(ccqt_bt_eq(type, CCBT__Type) || cc_eval_address_type(type)){
        if(!root) return CC_NOT_CONSTANT_ERROR;
        return cc_check_linktime_opaque_view(ctx, (CcExpr*_Nonnull)root, root_offset, size);
    }
    if(ccqt_bt_eq(type, CCBT__Any) || ccqt_kind(type) == CC_SLICE){
        _Bool any = ccqt_bt_eq(type, CCBT__Any);
        uint32_t start = any ? 0 : cc_target(p)->sizeof_[CCBT_nullptr_t];
        uint32_t end = start + cc_target(p)->sizeof_[any ? CCBT__Type : CCBT_nullptr_t];
        uint64_t lo = offset > start ? offset : start;
        uint64_t hi = offset+size < end ? offset+size : end;
        if(lo < hi){
            _Bool needed = !mask;
            if(mask) for(uint32_t i = (uint32_t)(lo-offset); i < hi-offset; i++) needed |= mask[i] != 0;
            if(needed){
                if(!root) return CC_NOT_CONSTANT_ERROR;
                return cc_check_linktime_opaque_view(ctx, (CcExpr*_Nonnull)root, root_offset+(uint32_t)(lo-offset), (uint32_t)(hi-lo));
            }
        }
        return 0;
    }
    CcTypeKind kind = ccqt_kind(type);
    if(kind == CC_ARRAY){
        CcArray* array = ccqt_as_array(type);
        uint32_t stride;
        int err = cc_type_sizeof_complete(cc_target(p), array->element, &stride);
        if(err) return err;
        if(!stride) return CC_NOT_CONSTANT_ERROR;
        uint64_t first = offset/stride, last = (offset+size-1)/stride;
        for(uint64_t i = first; i <= last; i++){
            uint64_t start = i*stride, end = start+stride;
            uint64_t lo = offset > start ? offset : start;
            uint64_t hi = offset+size < end ? offset+size : end;
            CcEvalDefaultPath child = {.parent=path, .index=(uint32_t)i, .count=path ? path->count+1 : 1};
            err = cc_check_linktime_default_view(ctx, root, array->element, lo-start,
                (uint32_t)(hi-lo), root_offset+(uint32_t)(lo-offset), mask ? mask+lo-offset : NULL, depth+1, list, &child);
            if(err) return err;
        }
    }
    else if(kind == CC_STRUCT || kind == CC_UNION){
        CcStruct* aggregate = ccqt_as_struct(type);
        uint32_t selected = kind == CC_UNION && list ? cc_eval_default_union_member((CcInitList*_Nonnull)list, path) : UINT32_MAX;
        for(uint32_t i = 0; i < aggregate->field_count; i++){
            if(selected != UINT32_MAX && i != selected) continue;
            CcField* field = &aggregate->fields[i];
            if(field->is_method) continue;
            if(field->is_bitfield){
                if(kind == CC_UNION && field->name) return 0;
                continue;
            }
            // Flexible members contribute no storage, including tail padding.
            if(ccqt_kind(field->type) == CC_ARRAY && ccqt_as_array(field->type)->is_incomplete) continue;
            uint64_t start = field->offset;
            uint32_t field_size;
            int err = cc_type_sizeof_complete(cc_target(p), field->type, &field_size);
            if(err) return err;
            uint64_t end = start + field_size;
            uint64_t lo = offset > start ? offset : start;
            uint64_t hi = offset+size < end ? offset+size : end;
            if(lo < hi){
                CcEvalDefaultPath child = {.parent=path, .index=i, .count=path ? path->count+1 : 1};
                err = cc_check_linktime_default_view(ctx, root, field->type, lo-start,
                    (uint32_t)(hi-lo), root_offset+(uint32_t)(lo-offset), mask ? mask+lo-offset : NULL, depth+1, list, &child);
                if(err) return err;
            }
            if(kind == CC_UNION) break;
        }
    }
    return 0;
}

// Follow the source representation of an aggregate view. A relocation may be
// copied whole, but a boundary through it requires concrete final bytes. Read
// those bytes from the original view so later designators can overwrite an
// earlier relocation, including writes in a containing initializer.
static
int
cc_check_linktime_view(CcEvalCtx* ctx, CcExpr* root, CcExpr* e, uint64_t offset, uint32_t size, uint32_t root_offset, unsigned depth){
    if(depth > 256) return CC_NOT_CONSTANT_ERROR;
    CcParser* p = ctx->parser;
    uint32_t object_size;
    int err = cc_sizeof_as_uint(p, e->type, e->loc, &object_size);
    if(err) return err;
    if(offset > object_size || size > object_size - offset) return CC_NOT_CONSTANT_ERROR;
    if(!size) return 0;
    CcExpr* source;
    err = cc_eval_object_source(ctx, e, depth, &source);
    if(err) return err;
    if(source != e) return cc_check_linktime_view(ctx, root, source, offset, size, root_offset, depth + 1);
    if(cc_eval_object_access(e)){
        CcExpr storage = {0};
        CcEvalObjectView view = {.object=e, .storage=&storage, .depth=depth};
        err = cc_eval_object_view_range(ctx, e, offset, &view);
        if(err) return err;
        err = cc_check_linktime_view(ctx, root, view.object, view.byte_offset, size, root_offset, depth + 1);
        cc_field_path_free(cc_allocator(p), view.path);
        return err;
    }
    if(e->kind == CC_EXPR_CAST && ccqt_bt_eq(e->type, CCBT__Any)){
        if(ccqt_bt_eq(e->lhs->type, CCBT__Any))
            return cc_check_linktime_view(ctx, root, e->lhs, offset, size, root_offset, depth + 1);
        uint32_t tag_size = cc_target(p)->sizeof_[CCBT__Type];
        if(offset < tag_size){
            uint32_t n = size < tag_size-offset ? size : tag_size-(uint32_t)offset;
            err = cc_check_linktime_opaque_view(ctx, root, root_offset, n);
            if(err) return err;
        }
        uint32_t payload_size;
        err = cc_sizeof_as_uint(p, e->lhs->type, e->loc, &payload_size);
        if(err) return err;
        uint64_t start = offsetof(CiRtAny, payload), end = start + payload_size;
        uint64_t lo = offset > start ? offset : start;
        uint64_t hi = offset + size < end ? offset + size : end;
        if(lo >= hi) return 0;
        return cc_check_linktime_view(ctx, root, e->lhs, lo - start, (uint32_t)(hi - lo), root_offset + (uint32_t)(lo - offset), depth + 1);
    }
    if(e->kind == CC_EXPR_INIT_LIST || e->kind == CC_EXPR_COMPOUND_LITERAL){
        CcInitList* list = e->init_list;
        for(uint32_t i = 0; i < list->count; i++){
            CcInitEntry* entry = &list->entries[i];
            uint64_t entry_offset;
            err = cc_field_path_resolve(cc_target(p), e->type, entry->path, &entry_offset);
            if(err) return err;
            if(!entry->value || cc_field_path_bit_width(e->type, entry->path)) continue;
            uint32_t entry_size;
            err = cc_sizeof_as_uint(p, entry->value->type, entry->value->loc, &entry_size);
            if(err) return err;
            uint64_t start = entry_offset, end = start + entry_size;
            uint64_t lo = offset > start ? offset : start;
            uint64_t hi = offset + size < end ? offset + size : end;
            if(lo >= hi) continue;
            err = cc_check_linktime_view(ctx, root, entry->value, lo - start, (uint32_t)(hi - lo), root_offset + (uint32_t)(lo - offset), depth + 1);
            if(err) return err;
        }
        return cc_check_linktime_default_view(ctx, root, e->type, offset, size, root_offset, NULL, depth+1, list, NULL);
    }
    if(e->kind == CC_EXPR_TYPE_INTROSPECTION){
        CcExpr* value;
        err = cc_eval_expr(ctx, e, &value);
        if(err) return err;
        err = cc_check_linktime_view(ctx, root, value, offset, size, root_offset, depth + 1);
        cc_release_expr(p, value);
        return err;
    }
    if(ccqt_bt_eq(e->type, CCBT__Type))
        return cc_check_linktime_opaque_view(ctx, root, root_offset, size);
    if(cc_eval_address_type(e->type))
        return cc_check_linktime_opaque_view(ctx, root, root_offset, size);
    if(ccqt_kind(e->type) == CC_SLICE
        && (e->kind == CC_EXPR_SLICE || e->kind == CC_EXPR_SLICE_LO || e->kind == CC_EXPR_SLICE_HI
            || e->kind == CC_EXPR_SLICE_ALL || e->kind == CC_EXPR_CAST)){
        uint32_t ptr_size = cc_target(p)->sizeof_[CCBT_nullptr_t];
        uint64_t lo = offset > ptr_size ? offset : ptr_size;
        uint64_t hi = offset + size < 2 * ptr_size ? offset + size : 2 * ptr_size;
        if(lo >= hi) return 0;
        return cc_check_linktime_opaque_view(ctx, root, root_offset+(uint32_t)(lo-offset), (uint32_t)(hi-lo));
    }
    CcExpr* address_expr = e;
    if(e->kind == CC_EXPR_CAST && ccqt_is_integer(e->type) && ccqt_kind(e->lhs->type) == CC_POINTER)
        address_expr = e->lhs;
    if(ccqt_kind(address_expr->type) == CC_POINTER && (offset || size != object_size)){
        CcEvalAddress address;
        err = cc_eval_address(ctx, address_expr, 0, 0, &address);
        if(err) return err;
        if(address.kind != CC_EVAL_ABSOLUTE){
            unsigned char bytes[16];
            if(size > sizeof bytes) return CC_NOT_CONSTANT_ERROR;
            return cc_eval_object_bytes(ctx, root, root_offset, size, bytes, NULL);
        }
    }
    return 0;
}

static int cc_check_linktime_expr_inner(CcParser*, CcExpr*, _Bool, _Bool, unsigned);
static _Bool cc_eval_aggregate_type(CcQualType);

static
int
cc_check_linktime_subobject(CcParser* p, CcExpr* e, unsigned depth){
    if(depth >= 256) return CC_NOT_CONSTANT_ERROR;
    CcEvalCtx ctx = {.parser=p, .allow_const=1};
    _Bool handled;
    CcExpr* selected = NULL;
    int selection_err = cc_eval_object_view_select(&ctx, e, 1, &handled, &selected);
    if(selection_err || handled){
        if(!selection_err) selection_err = cc_check_linktime_expr_inner(p, (CcExpr*_Nonnull)selected, 0,
            cc_eval_aggregate_type(selected->type), depth + 1);
        if(selected) cc_release_expr(p, selected);
        return selection_err;
    }
    _Bool metadata = ccqt_bt_eq(e->type, CCBT__Type);
    if(cc_eval_address_type(e->type)){
        CcEvalAddress address;
        return cc_eval_address(&(CcEvalCtx){.parser = p, .allow_const = 1}, e, 0, 0, &address);
    }
    CcTypeKind kind = ccqt_kind(e->type);
    if(kind == CC_STRUCT || kind == CC_UNION || kind == CC_SLICE || ccqt_bt_eq(e->type, CCBT__Any)){
        uint32_t size;
        int err = cc_sizeof_as_uint(p, e->type, e->loc, &size);
        if(err) return err;
        return cc_check_linktime_view(&(CcEvalCtx){.parser = p, .allow_const = 1}, e, e, 0, size, 0, 0);
    }
    _Bool floating = ccqt_is_basic(e->type) && ccbt_is_float(e->type.basic.kind);
    if(!metadata && !floating && !ccqt_is_integer(e->type)) return 0;
    uint32_t size;
    int err = cc_sizeof_as_uint(p, e->type, e->loc, &size);
    if(err) return err;
    // Numeric subobjects require actual numeric bytes. Opaque pointer and
    // metadata copies are handled separately above.
    CcExpr* value;
    err = cc_eval_linktime_scalar(p, e, &value);
    if(!err) cc_release_expr(p, value);
    return err;
}

static
int
cc_check_linktime_expr(CcParser* p, CcExpr* e, _Bool address, unsigned depth){
    return cc_check_linktime_expr_inner(p, e, address, 0, depth);
}

static
int
cc_check_linktime_expr_inner(CcParser* p, CcExpr* e, _Bool address, _Bool selected, unsigned depth){
    if(depth > 256) return CC_NOT_CONSTANT_ERROR;
    if(!address && e->is_lvalue && (e->type.is_volatile || e->type.is_atomic))
        return CC_NOT_CONSTANT_ERROR;
    if(cc_eval_pointer_binary_expr(e)){
        int64_t value;
        return cc_eval_pointer_binary(&(CcEvalCtx){.parser = p, .allow_const = 1}, e, &value);
    }
    int err;
    switch(e->kind){
        case CC_EXPR_OBJECT_VIEW:
            return cc_check_linktime_expr_inner(p, e->lhs, address, selected, depth + 1);
        case CC_EXPR_VALUE: case CC_EXPR_FUNCTION:
            return 0;
        case CC_EXPR_VARIABLE: {
            if(address) return (e->var->automatic || e->var->thread_local_) ? CC_NOT_CONSTANT_ERROR : 0;
            if(!cc_linktime_const_variable(e)) return CC_NOT_CONSTANT_ERROR;
            return cc_check_linktime_expr_inner(p, e->var->initializer, 0, selected, depth + 1);
        }
        case CC_EXPR_INIT_LIST: case CC_EXPR_COMPOUND_LITERAL:
            for(uint32_t i = 0; i < e->init_list->count; i++){
                if(!e->init_list->entries[i].value) continue;
                err = cc_check_linktime_expr_inner(p, e->init_list->entries[i].value, 0, selected, depth + 1);
                if(err) return err;
            }
            return 0;
        case CC_EXPR_ADDR:
            return cc_check_linktime_expr(p, e->lhs, 1, depth + 1);
        case CC_EXPR_DEREF:
            if(!address){
                err = cc_check_linktime_subobject(p, e, depth);
                if(err) return err;
            }
            return cc_check_linktime_expr(p, e->lhs, 0, depth + 1);
        case CC_EXPR_DOT:
            if(!address){
                err = cc_eval_check_any_view(&(CcEvalCtx){.parser = p, .allow_const = 1}, e);
                if(err == CC_NOT_CONSTANT_ERROR)
                    return cc_error(p, e->loc, "constant _Any.as requires the stored type, ignoring top-level qualifiers");
                if(err) return err;
                if(!selected){
                    err = cc_check_linktime_subobject(p, e, depth);
                    if(err) return err;
                }
            }
            // Selecting a field does not read its entire containing aggregate.
            return cc_check_linktime_expr_inner(p, e->values[0], address, !address, depth + 1);
        case CC_EXPR_ARROW:
            if(!address && !selected){
                err = cc_check_linktime_subobject(p, e, depth);
                if(err) return err;
            }
            return cc_check_linktime_expr(p, e->values[0], 0, depth + 1);
        case CC_EXPR_SUBSCRIPT:
            if(address && ccqt_kind(e->lhs->type) == CC_SLICE){
                CcEvalAddress value;
                return cc_eval_address(&(CcEvalCtx){.parser = p, .allow_const = 1}, e, 1, depth + 1, &value);
            }
            if(!address && !selected){
                err = cc_check_linktime_subobject(p, e, depth);
                if(err) return err;
            }
            err = cc_check_linktime_expr_inner(p, e->lhs, address, !address, depth + 1);
            if(err) return err;
            return cc_check_linktime_expr(p, e->values[0], 0, depth + 1);
        case CC_EXPR_CAST:
            err = cc_check_linktime_expr_inner(p, e->lhs,
                (ccqt_kind(e->type) == CC_POINTER || ccqt_kind(e->type) == CC_SLICE)
                    && ccqt_kind(e->lhs->type) == CC_ARRAY,
                selected && e->type.unqual == e->lhs->type.unqual, depth + 1);
            if(err) return err;
            if(!selected && ccqt_is_integer(e->type) && !ccqt_is_bool(e->type)
                && (ccqt_is_integer(e->lhs->type) || cc_eval_address_type(e->lhs->type)
                    || ccqt_kind(e->lhs->type) == CC_ARRAY || ccqt_kind(e->lhs->type) == CC_FUNCTION)){
                CcExpr* value;
                err = cc_eval_linktime_scalar(p, e, &value);
                if(!err) cc_release_expr(p, value);
                if(err == CC_NOT_CONSTANT_ERROR){
                    CcEvalAddress relocation;
                    err = cc_eval_integer_address(&(CcEvalCtx){.parser=p, .allow_const=1}, e, 0, &relocation);
                }
                return err == CC_OVERFLOW_ERROR ? CC_NOT_CONSTANT_ERROR : err;
            }
            if(e->type.unqual != e->lhs->type.unqual
                && ((ccqt_is_basic(e->type) && ccbt_is_float(e->type.basic.kind))
                    || (ccqt_is_basic(e->lhs->type) && ccbt_is_float(e->lhs->type.basic.kind)))
                && (ccqt_is_integer(e->type) || (ccqt_is_basic(e->type) && ccbt_is_float(e->type.basic.kind)))){
                CcExpr* value;
                err = cc_eval_expr(&(CcEvalCtx){.parser = p, .allow_const = 1}, e->lhs, &value);
                if(!err) cc_release_expr(p, value);
                if(err) return err;
            }
            if(ccqt_is_integer(e->type) && !ccqt_bt_eq(e->type, CCBT_bool)
                && ccqt_is_basic(e->lhs->type) && ccbt_is_float(e->lhs->type.basic.kind)){
                CcExpr* value;
                err = cc_eval_expr(&(CcEvalCtx){.parser = p, .allow_const = 1}, e->lhs, &value);
                if(err && err != CC_NOT_CONSTANT_ERROR) return err;
                if(!err){
                    uint32_t size;
                    err = cc_sizeof_as_uint(p, e->type, e->loc, &size);
                    if(err){ cc_release_expr(p, value); return err; }
                    _Bool uns = ccqt_is_unsigned(e->type, !cc_target(p)->char_is_signed);
                    CiFloat128 q = cc_eval_quad(p, value);
                    CiFloat128 bound = ci_float128_from_uint128(ci_uint128_shl(ci_uint128_from_uint64(1), size * 8 - 1), 1);
                    if(uns) bound = ci_float128_add(bound, bound);
                    CiFloat128 low = uns ? ci_float128_from_int64(0) : ci_float128_neg(bound);
                    CiFloat128 below = ci_float128_sub(low, ci_float128_from_int64(1));
                    _Bool in_range = (ci_float128_le(low, q) || ci_float128_lt(below, q)) && ci_float128_lt(q, bound);
                    cc_release_expr(p, value);
                    if(!in_range) return cc_error(p, e->loc, "static initializer conversion is out of range");
                }
            }
            return 0;
        case CC_EXPR_COMMA:
            // The parser represents a compound literal's anonymous storage as
            // an assignment followed by a reference to that storage.
            if(e->lhs->kind == CC_EXPR_ASSIGN && e->lhs->lhs->kind == CC_EXPR_VARIABLE
                && e->values[0]->kind == CC_EXPR_VARIABLE
                && e->lhs->lhs->var == e->values[0]->var
                && e->values[0]->var->name == nil_atom && !e->values[0]->var->automatic)
                return cc_check_linktime_expr(p, e->lhs->values[0], 0, depth + 1);
            err = cc_check_linktime_expr(p, e->lhs, 0, depth + 1);
            if(err) return err;
            return cc_check_linktime_expr_inner(p, e->values[0], address, selected, depth + 1);
        case CC_EXPR_SLICE: case CC_EXPR_SLICE_ALL: case CC_EXPR_SLICE_LO: case CC_EXPR_SLICE_HI:
            err = cc_check_linktime_expr(p, e->lhs, ccqt_kind(e->lhs->type) == CC_ARRAY, depth + 1);
            if(err) return err;
            break;
        case CC_EXPR_LOGAND: case CC_EXPR_LOGOR: case CC_EXPR_TERNARY: {
            err = cc_check_linktime_expr(p, e->lhs, 0, depth + 1);
            if(err) return err;
            _Bool truth;
            err = cc_eval_truthy(&(CcEvalCtx){.parser = p, .allow_const = 1}, e->lhs, &truth);
            if(err) return err;
            if(e->kind == CC_EXPR_TERNARY)
                return cc_check_linktime_expr_inner(p, e->values[truth ? 0 : 1], address, selected, depth + 1);
            if((e->kind == CC_EXPR_LOGAND && !truth) || (e->kind == CC_EXPR_LOGOR && truth))
                return 0;
            return cc_check_linktime_expr(p, e->values[0], 0, depth + 1);
        }
        case CC_EXPR_BIT_BUILTIN: {
            CcExpr* value;
            err = cc_eval_linktime_scalar(p, e, &value);
            if(!err) cc_release_expr(p, value);
            return err;
        }
        case CC_EXPR_NEG: case CC_EXPR_POS: case CC_EXPR_BITNOT: case CC_EXPR_LOGNOT:
        case CC_EXPR_ADD: case CC_EXPR_SUB: case CC_EXPR_MUL: case CC_EXPR_DIV: case CC_EXPR_MOD:
        case CC_EXPR_BITAND: case CC_EXPR_BITOR: case CC_EXPR_BITXOR:
        case CC_EXPR_EQ: case CC_EXPR_NE: case CC_EXPR_LT: case CC_EXPR_GT: case CC_EXPR_LE: case CC_EXPR_GE:
        case CC_EXPR_BSWAP:
            // Relocations can copy a floating representation, but arithmetic
            // on that representation requires its unresolved numeric value.
            if((ccqt_is_basic(e->lhs->type) && ccbt_is_float(e->lhs->type.basic.kind))
                || (ccqt_is_basic(e->type) && ccbt_is_float(e->type.basic.kind))){
                CcExpr* value;
                err = cc_eval_linktime_scalar(p, e, &value);
                if(!err) cc_release_expr(p, value);
                return err;
            }
            err = cc_check_linktime_expr_inner(p, e->lhs,
                ccqt_kind(e->type) == CC_POINTER && ccqt_kind(e->lhs->type) == CC_ARRAY, selected, depth + 1);
            if(err) return err;
            // A selected aggregate may retain earlier writes that are fully
            // overwritten. Its storage reader evaluates only the needed bits.
            if(!selected && ccqt_is_integer(e->type) && ccqt_is_integer(e->lhs->type)){
                for(size_t i = 0, n = cc_expr_nvalues(e); i < n; i++){
                    err = cc_check_linktime_expr(p, e->values[i], 0, depth + 1);
                    if(err) return err;
                }
                CcExpr* value;
                err = cc_eval_linktime_scalar(p, e, &value);
                if(!err) cc_release_expr(p, value);
                if(err == CC_OVERFLOW_ERROR) return CC_NOT_CONSTANT_ERROR;
                // Integer addresses may retain a symbol plus an addend.
                // Other integer operations need a concrete numeric value.
                if(err == CC_NOT_CONSTANT_ERROR && (e->kind == CC_EXPR_ADD || e->kind == CC_EXPR_SUB)){
                    CcEvalAddress relocation;
                    err = cc_eval_integer_address(&(CcEvalCtx){.parser=p, .allow_const=1}, e, 0, &relocation);
                    return err == CC_OVERFLOW_ERROR ? CC_NOT_CONSTANT_ERROR : err;
                }
                return err;
            }
            break;
        case CC_EXPR_LSHIFT: case CC_EXPR_RSHIFT:{
            err = cc_check_linktime_expr(p, e->lhs, 0, depth + 1);
            if(err) return err;
            CcExpr* count;
            err = cc_eval_linktime_scalar(p, e->values[0], &count);
            if(err) return err == CC_OVERFLOW_ERROR ? CC_NOT_CONSTANT_ERROR : err;
            err = cc_eval_check_shift_count(p, e->lhs->type, count);
            cc_release_expr(p, count);
            if(!err && !selected){
                CcExpr* value;
                err = cc_eval_linktime_scalar(p, e, &value);
                if(!err) cc_release_expr(p, value);
            }
            return err == CC_OVERFLOW_ERROR ? CC_NOT_CONSTANT_ERROR : err;
        }
        case CC_EXPR_TYPE_INTROSPECTION:{
            CcExpr* value;
            err = cc_eval_linktime_expr(p, e, &value);
            if(err) return err;
            err = cc_check_linktime_expr_inner(p, value, address, selected, depth + 1);
            cc_release_expr(p, value);
            return err;
        }
        default:
            return CC_NOT_CONSTANT_ERROR;
    }
    for(size_t i = 0, n = cc_expr_nvalues(e); i < n; i++){
        err = cc_check_linktime_expr_inner(p, e->values[i], 0, selected, depth + 1);
        if(err) return err;
    }
    return 0;
}

// Read integer/floating representations and padding. Addresses and compiler
// metadata are opaque, including their null or implicitly zero values.
static int cc_eval_object_bytes_inner(CcEvalCtx*, CcExpr*, uint32_t, uint32_t, unsigned char*, const unsigned char*_Nullable);

static
int
cc_eval_object_bytes(CcEvalCtx* ctx, CcExpr* e, uint32_t offset, uint32_t size, unsigned char* out, const unsigned char*_Nullable mask){
    if(ctx->evaluation_depth >= 256) return CC_NOT_CONSTANT_ERROR;
    if(e->is_lvalue && (e->type.is_volatile || e->type.is_atomic))
        return CC_NOT_CONSTANT_ERROR;
    for(CcEvalVisit* visit = ctx->objects; visit; visit = visit->previous)
        if(visit->expr == e && visit->offset == offset && visit->size == size)
            return CC_NOT_CONSTANT_ERROR;
    CcEvalVisit visit = {ctx->objects, e, offset, size};
    ctx->objects = &visit;
    ctx->evaluation_depth++;
    int err = cc_eval_object_bytes_inner(ctx, e, offset, size, out, mask);
    ctx->evaluation_depth--;
    ctx->objects = visit.previous;
    return err;
}

static
int
cc_eval_object_bytes_inner(CcEvalCtx* ctx, CcExpr* e, uint32_t offset, uint32_t size, unsigned char* out, const unsigned char*_Nullable mask){
    CcParser* p = ctx->parser;
    memset(out, 0, size);
    if(mask){
        _Bool needed = 0;
        for(uint32_t i = 0; i < size; i++) needed |= mask[i] != 0;
        if(!needed) return 0;
    }
    CcExpr* source;
    int source_err = cc_eval_object_source(ctx, e, ctx->evaluation_depth, &source);
    if(source_err) return source_err;
    if(source != e) return cc_eval_object_bytes(ctx, source, offset, size, out, mask);
    if(cc_eval_object_access(e)){
        CcExpr storage = {0};
        CcEvalObjectView view = {.object=e, .storage=&storage, .depth=ctx->evaluation_depth};
        int err = cc_eval_object_view_range(ctx, e, offset, &view);
        if(err) return err;
        if(view.byte_offset > UINT32_MAX) err = CC_NOT_CONSTANT_ERROR;
        else err = cc_eval_object_bytes(ctx, view.object, (uint32_t)view.byte_offset, size, out, mask);
        cc_field_path_free(cc_allocator(p), view.path);
        return err;
    }
    if(e->kind == CC_EXPR_VALUE && ccqt_kind(e->type) == CC_ARRAY && e->text && e->str.length){
        uint32_t elem_size;
        int err = cc_sizeof_as_uint(p, ccqt_as_array(e->type)->element, e->loc, &elem_size);
        if(err) return err;
        uint64_t length = (uint64_t)e->str.length * elem_size;
        if(offset < length){
            uint32_t n = length - offset < size ? (uint32_t)(length - offset) : size;
            memcpy(out, e->text + offset, n);
        }
        return 0;
    }
    if(e->kind == CC_EXPR_CAST && ccqt_bt_eq(e->type, CCBT__Any)){
        if(ccqt_bt_eq(e->lhs->type, CCBT__Any))
            return cc_eval_object_bytes(ctx, e->lhs, offset, size, out, mask);
        uint32_t payload_offset = offsetof(CiRtAny, payload);
        if(offset < payload_offset){
            uint32_t n = size < payload_offset - offset ? size : payload_offset - offset;
            // The tag is opaque compiler metadata, never numeric bytes.
            for(uint32_t i = 0; i < n; i++)
                if(!mask || mask[i]) return CC_NOT_CONSTANT_ERROR;
        }
        uint32_t src_size;
        int err = cc_sizeof_as_uint(p, e->lhs->type, e->loc, &src_size);
        if(err) return err;
        uint32_t lo = offset > payload_offset ? offset : payload_offset;
        uint32_t hi = offset + size < payload_offset + src_size ? offset + size : payload_offset + src_size;
        if(lo < hi)
            return cc_eval_object_bytes(ctx, e->lhs, lo - payload_offset, hi - lo, out + lo - offset, mask ? mask + lo - offset : NULL);
        return 0;
    }
    if(cc_eval_slice_expr(e)){
        uint64_t count;
        CcEvalAddress address;
        int err = cc_eval_slice(ctx, e, 0, &count, &address);
        if(err) return err;
        uint32_t ptr_size = cc_target(p)->sizeof_[CCBT_nullptr_t];
        if(offset > 2 * ptr_size || size > 2 * ptr_size - offset) return CC_NOT_CONSTANT_ERROR;
        if(offset < ptr_size){
            uint32_t n = size < ptr_size - offset ? size : ptr_size - offset;
            memcpy(out, (const unsigned char*)&count + offset, n);
        }
        uint32_t lo = offset > ptr_size ? offset : ptr_size;
        uint32_t hi = offset + size;
        if(lo < hi){
            _Bool needed = !mask;
            if(mask) for(uint32_t i = lo - offset; i < size; i++) needed |= mask[i] != 0;
            if(needed) return CC_NOT_CONSTANT_ERROR;
        }
        return 0;
    }
    if(e->kind == CC_EXPR_INIT_LIST || e->kind == CC_EXPR_COMPOUND_LITERAL){
        CcInitList* il = e->init_list;
        // Queries originate in scalar extraction, so at most 16 bytes are
        // needed. Resolve writes backwards, retaining the last write per bit.
        // An overwritten relocation must never be interpreted as host bytes.
        if(size > 16) return CC_NOT_CONSTANT_ERROR;
        unsigned char written[16] = {0};
        // Bits outside the requested field are already satisfied. They must
        // not force evaluation of an older, overlapping symbolic address.
        if(mask) for(uint32_t i = 0; i < size; i++) written[i] = (unsigned char)~mask[i];
        for(uint32_t i = il->count; i; i--){
            CcInitEntry* ent = &il->entries[i-1];
            if(!ent->value) continue;
            uint32_t sz;
            int err = cc_sizeof_as_uint(p, ent->value->type, e->loc, &sz);
            if(err) return err;
            uint64_t entry_offset;
            err = cc_field_path_resolve(cc_target(p), e->type, ent->path, &entry_offset);
            if(err) return err;
            uint64_t start = entry_offset;
            uint64_t end = start + sz;
            if(start >= (uint64_t)offset + size || end <= offset) continue;
            uint32_t lo = start > offset ? (uint32_t)start : offset;
            uint32_t hi = end < (uint64_t)offset + size ? (uint32_t)end : offset + size;
            if(cc_field_path_bit_width(e->type, ent->path)){
                _Bool needed = 0;
                for(uint32_t byte = lo; byte < hi; byte++){
                    for(unsigned bit = 0; bit < 8; bit++){
                        uint64_t pos = ((uint64_t)byte - start) * 8 + bit;
                        if(pos >= cc_field_path_bit_offset(e->type, ent->path)
                            && pos - cc_field_path_bit_offset(e->type, ent->path) < cc_field_path_bit_width(e->type, ent->path)
                            && !(written[byte-offset] & (1u << bit))) needed = 1;
                    }
                }
                if(!needed) continue;
                CcExpr* value;
                err = cc_eval_expr(ctx, ent->value, &value);
                if(err) return err;
                CiUint128 bits = cc_eval_u128(p, value);
                cc_release_expr(p, value);
                // Merge only this field's bits: adjacent fields can share bytes.
                for(uint32_t byte = lo; byte < hi; byte++){
                    for(unsigned bit = 0; bit < 8; bit++){
                        uint64_t pos = ((uint64_t)byte - start) * 8 + bit;
                        if(pos < cc_field_path_bit_offset(e->type, ent->path)) continue;
                        pos -= cc_field_path_bit_offset(e->type, ent->path);
                        if(pos >= cc_field_path_bit_width(e->type, ent->path)) continue;
                        unsigned char bit_mask = (unsigned char)(1u << bit);
                        if(written[byte-offset] & bit_mask) continue;
                        if(pos >= 128) return CC_NOT_CONSTANT_ERROR;
                        out[byte - offset] = (out[byte - offset] & ~bit_mask)
                            | ((ci_uint128_lo(ci_uint128_shr(bits, pos)) & 1) ? bit_mask : 0);
                        written[byte-offset] |= bit_mask;
                    }
                }
                continue;
            }
            for(uint32_t byte = lo; byte < hi;){
                if(written[byte-offset] == 0xff){ byte++; continue; }
                uint32_t first = byte;
                while(byte < hi && written[byte-offset] != 0xff) byte++;
                unsigned char bytes[16];
                unsigned char needed[16];
                for(uint32_t at = first; at < byte; at++) needed[at-first] = (unsigned char)~written[at-offset];
                err = cc_eval_object_bytes(ctx, ent->value, first - (uint32_t)start, byte - first, bytes, needed);
                if(err) return err;
                for(uint32_t at = first; at < byte; at++){
                    unsigned char prior_mask = written[at-offset];
                    out[at-offset] = (out[at-offset] & prior_mask) | (bytes[at-first] & ~prior_mask);
                    written[at-offset] = 0xff;
                }
            }
        }
        unsigned char pending[16];
        for(uint32_t i = 0; i < size; i++) pending[i] = (unsigned char)~written[i];
        return cc_check_linktime_default_view(ctx, NULL, e->type, offset, size, 0, pending, ctx->evaluation_depth, il, NULL);
    }
    CcExpr* v;
    int err = cc_eval_expr(ctx, e, &v);
    if(err) return err;
    if(v->kind == CC_EXPR_INIT_LIST)
        err = cc_eval_object_bytes(ctx, v, offset, size, out, mask);
    else if(v->kind != CC_EXPR_VALUE)
        err = CC_NOT_CONSTANT_ERROR;
    else {
        CcQualType t = v->type;
        if(ccqt_kind(t) == CC_ENUM) t = ccqt_as_enum(t)->underlying;
        if(!ccqt_is_basic(t) || !(ccbt_is_integer(t.basic.kind) || ccbt_is_float(t.basic.kind))
            || (uint64_t)offset + size > cc_target(p)->sizeof_[t.basic.kind] || (uint64_t)offset + size > sizeof v->data)
            err = CC_NOT_CONSTANT_ERROR;
        else if(ccqt_bt_eq(t, CCBT_float)){
            if(offset + size > sizeof(float)) err = CC_NOT_CONSTANT_ERROR;
            else memcpy(out, (const unsigned char*)&v->float_ + offset, size);
        }
        else if(ccqt_bt_eq(t, CCBT_double) || cc_eval_wide(t))
            memcpy(out, v->data + offset, size);
        else if(ccbt_is_integer(t.basic.kind))
            memcpy(out, (const unsigned char*)&v->uinteger + offset, size);
        else err = CC_NOT_CONSTANT_ERROR;
    }
    cc_release_expr(p, v);
    return err;
}

// Type values hold compiler-owned metadata pointers. Byte reinterpretation
// must never fabricate them, even when the bytes are otherwise constant.
static
int
cc_eval_object_type(CcEvalCtx* ctx, CcExpr* e, uint64_t offset, unsigned depth, CcQualType* out){
    if(depth > 256 || e->type.is_volatile || e->type.is_atomic) return CC_NOT_CONSTANT_ERROR;
    CcParser* p = ctx->parser;
    uint32_t size, object_size;
    int err = cc_sizeof_as_uint(p, ccqt_basic(CCBT__Type), e->loc, &size);
    if(err) return err;
    err = cc_sizeof_as_uint(p, e->type, e->loc, &object_size);
    if(err) return err;
    if(offset > object_size || size > object_size - offset) return CC_NOT_CONSTANT_ERROR;
    CcExpr* source;
    err = cc_eval_object_source(ctx, e, depth, &source);
    if(err) return err;
    if(source != e) return cc_eval_object_type(ctx, source, offset, depth + 1, out);
    if(!offset && ccqt_bt_eq(e->type, CCBT__Type)
        && (e->kind == CC_EXPR_DOT || e->kind == CC_EXPR_SUBSCRIPT)){
        _Bool handled;
        CcExpr* selected = NULL;
        err = cc_eval_object_view_select(ctx, e, 1, &handled, &selected);
        if(err || handled){
            if(!err) err = cc_eval_object_type(ctx, selected, 0, depth + 1, out);
            if(selected) cc_release_expr(p, selected);
            return err;
        }
    }
    if(cc_eval_object_access(e)){
        CcExpr storage = {0};
        CcEvalObjectView view = {.object=e, .storage=&storage, .depth=depth};
        err = cc_eval_object_view_range(ctx, e, offset, &view);
        if(err) return err;
        err = cc_eval_object_type(ctx, view.object, view.byte_offset, depth + 1, out);
        cc_field_path_free(cc_allocator(p), view.path);
        return err;
    }
    if(e->kind == CC_EXPR_CAST && ccqt_bt_eq(e->type, CCBT__Any)){
        if(ccqt_bt_eq(e->lhs->type, CCBT__Any))
            return cc_eval_object_type(ctx, e->lhs, offset, depth + 1, out);
        if(!offset){ *out = e->lhs->type; out->quals = 0; return 0; }
        if(offset < offsetof(CiRtAny, payload)) return CC_NOT_CONSTANT_ERROR;
        return cc_eval_object_type(ctx, e->lhs, offset - offsetof(CiRtAny, payload), depth + 1, out);
    }
    if(e->kind == CC_EXPR_INIT_LIST || e->kind == CC_EXPR_COMPOUND_LITERAL){
        CcInitList* list = e->init_list;
        for(uint32_t i = list->count; i; i--){
            CcInitEntry* entry = &list->entries[i-1];
            uint64_t entry_offset;
            err = cc_field_path_resolve(cc_target(p), e->type, entry->path, &entry_offset);
            if(err) return err;
            if(!entry->value) continue;
            uint32_t entry_size;
            err = cc_sizeof_as_uint(p, entry->value->type, entry->value->loc, &entry_size);
            if(err) return err;
            uint64_t start = entry_offset, end = start + entry_size;
            if(start >= offset + size || end <= offset) continue;
            if(cc_field_path_bit_width(e->type, entry->path) || start > offset || end < offset + size) return CC_NOT_CONSTANT_ERROR;
            return cc_eval_object_type(ctx, entry->value, offset - start, depth + 1, out);
        }
        *out = (CcQualType){0};
        return 0;
    }
    if(!offset && ccqt_bt_eq(e->type, CCBT__Type) && e->kind == CC_EXPR_VALUE){
        *out = e->type_value;
        return 0;
    }
    if(e->kind == CC_EXPR_TYPE_INTROSPECTION){
        CcExpr* value;
        err = cc_eval_expr(ctx, e, &value);
        if(err) return err;
        err = cc_eval_object_type(ctx, value, offset, depth + 1, out);
        cc_release_expr(p, value);
        return err;
    }
    return CC_NOT_CONSTANT_ERROR;
}

static
int
cc_eval_object_scalar(CcEvalCtx* ctx, CcExpr* base, uint32_t offset, CcQualType type, SrcLoc loc, const unsigned char*_Nullable mask, CcExpr*_Nullable*_Nonnull result){
    CcParser* p = ctx->parser;
    CcQualType t = type;
    if(ccqt_kind(t) == CC_ENUM) t = ccqt_as_enum(t)->underlying;
    _Bool pointer = ccqt_kind(t) == CC_POINTER || ccqt_kind(t) == CC_BLOCK_POINTER;
    if(!pointer && (!ccqt_is_basic(t) || !(ccbt_is_integer(t.basic.kind) || ccqt_bt_eq(t, CCBT_float)
        || ccqt_bt_eq(t, CCBT_double) || cc_eval_wide(t) || ccqt_bt_eq(t, CCBT__Type) || ccqt_bt_eq(t, CCBT_nullptr_t))))
        return CC_NOT_CONSTANT_ERROR;
    uint32_t size;
    int err = cc_sizeof_as_uint(p, t, loc, &size);
    if(err) return err;
    if(size > 16) return CC_NOT_CONSTANT_ERROR;
    if(pointer || ccqt_bt_eq(t, CCBT_nullptr_t)){
        CcEvalAddress address;
        err = cc_eval_object_address(ctx, base, offset, size, ctx->evaluation_depth, &address);
        if(!err){
            if(address.kind != CC_EVAL_ABSOLUTE) return CC_NOT_CONSTANT_ERROR;
            CcExpr* node = cc_value_expr(p, loc, type);
            if(!node) return CC_OOM_ERROR;
            node->uinteger = (uint64_t)address.offset;
            *result = node;
            return 0;
        }
        if(err != CC_NOT_CONSTANT_ERROR) return err;
    }
    unsigned char bytes[16] = {0};
    if(ccqt_bt_eq(t, CCBT__Type)){
        CcQualType metadata = {0};
        err = cc_eval_object_type(ctx, base, offset, 0, &metadata);
        if(err == CC_NOT_CONSTANT_ERROR){
            err = cc_eval_object_bytes(ctx, base, offset, size, bytes, mask);
            if(!err) for(uint32_t i = 0; i < size; i++)
                if(bytes[i]){ err = CC_NOT_CONSTANT_ERROR; break; }
        }
        if(err) return err;
        CcExpr* node = cc_value_expr(p, loc, type);
        if(!node) return CC_OOM_ERROR;
        node->type_value = metadata;
        *result = node;
        return 0;
    }
    else err = cc_eval_object_bytes(ctx, base, offset, size, bytes, mask);
    if(err) return err;
    // Numeric representation reads must not fabricate an invalid host _Bool.
    // Bitfield reads still need their containing bits until the caller extracts
    // and normalizes the field, so their intermediate masked value is allowed.
    if(ccqt_bt_eq(t, CCBT_bool) && !mask){
        for(uint32_t i = 0; i < size; i++)
            if(bytes[i] > (i ? 0 : 1)) return CC_NOT_CONSTANT_ERROR;
    }
    // Only actual numeric storage reaches this reconstruction. Pointer and
    // metadata storage never becomes an untyped byte buffer.
    CcExpr* node = cc_value_expr(p, loc, type);
    if(!node) return CC_OOM_ERROR;
    if(cc_eval_wide(t)) memcpy(node->data, bytes, size);
    else if(ccqt_bt_eq(t, CCBT_float)) memcpy(&node->float_, bytes, size);
    else if(ccqt_bt_eq(t, CCBT_double)) memcpy(&node->double_, bytes, size);
    else {
        memcpy(&node->uinteger, bytes, size);
        cc_eval_truncate(p, node);
    }
    *result = node;
    return 0;
}

// Selected aggregate views own their entries, while nested initializer lists
// retain the existing shared-list ownership convention. Keep symbolic leaves
// as expressions rather than trying to turn their addresses into host bytes.
static
int
cc_clone_initializer_expr(CcParser* p, CcExpr* e, unsigned depth, CcExpr*_Nullable*_Nonnull out){
    if(depth >= 256 || e->kind == CC_EXPR_STATEMENT_EXPRESSION) return CC_NOT_CONSTANT_ERROR;
    size_t n = cc_expr_nvalues(e);
    CcExpr* copy = _cc_alloc_expr(p, n);
    if(!copy) return CC_OOM_ERROR;
    memcpy(copy, e, sizeof *copy);
    for(size_t i = 0; i < n; i++) copy->values[i] = NULL;
    _Bool lhs = 0;
    int err = 0;
    switch(e->kind){
        case CC_EXPR_VALUE: case CC_EXPR_VARIABLE: case CC_EXPR_FUNCTION: case CC_EXPR_BUILTIN:
            break;
        case CC_EXPR_INIT_LIST: case CC_EXPR_COMPOUND_LITERAL:
            copy->init_list->rc++;
            break;
        case CC_EXPR_DOT: case CC_EXPR_ARROW:
            copy->field_path = (CcFieldPath){0};
            if(cc_field_path_concat(cc_allocator(p), (CcFieldPath){0}, e->field_path, &copy->field_path))
                err = CC_OOM_ERROR;
            break;
        default:
            lhs = 1;
            copy->lhs = NULL;
            break;
    }
    if(!err && lhs && e->lhs) err = cc_clone_initializer_expr(p, e->lhs, depth+1, &copy->lhs);
    for(size_t i = 0; !err && i < n; i++)
        if(e->values[i]) err = cc_clone_initializer_expr(p, e->values[i], depth+1, &copy->values[i]);
    if(err){
        for(size_t i = 0; i < n; i++)
            if(copy->values[i]) cc_release_expr(p, copy->values[i]);
        if(lhs && copy->lhs) cc_release_expr(p, copy->lhs);
        if(copy->kind == CC_EXPR_DOT || copy->kind == CC_EXPR_ARROW)
            cc_field_path_free(cc_allocator(p), copy->field_path);
        _cc_release_expr(p, copy, n);
        return err;
    }
    *out = copy;
    return 0;
}

// A slice value owns ordinary count/data initializer entries. Keep the data
// expression anchored to the original array or pointer, preserving provenance
// as well as its symbol; a symbol plus byte offset alone loses array bounds.
static
int
cc_eval_slice_value(CcEvalCtx* ctx, CcExpr* e, CcExpr*_Nullable*_Nonnull out){
    CcParser* p = ctx->parser;
    uint64_t count;
    CcEvalAddress address;
    int err = cc_eval_slice(ctx, e, ctx->evaluation_depth, &count, &address);
    if(err) return err;
    CcQualType pointer;
    err = cc_pointer_of(p, ccqt_as_slice(e->type)->pointee, &pointer);
    if(err) return err;
    CcExpr* data = NULL;
    if(address.kind == CC_EVAL_ABSOLUTE){
        data = cc_value_expr(p, e->loc, pointer);
        if(!data) return CC_OOM_ERROR;
        data->uinteger = (uint64_t)address.offset;
    }
    else {
        CcExpr* base = NULL;
        err = cc_clone_initializer_expr(p, e->lhs, ctx->evaluation_depth, &base);
        if(err) return err;
        if(ccqt_kind(base->type) == CC_SLICE){
            data = cc_make_expr(p, CC_EXPR_DOT, e->loc, pointer, 1);
            if(data){
                data->field_path = (CcFieldPath){.n_components=1, .idx0=1};
                data->values[0] = base;
            }
        }
        else data = cc_unary_expr(p, CC_EXPR_CAST, e->loc, pointer, base);
        if(!data){ cc_release_expr(p, base); return CC_OOM_ERROR; }
        if(e->kind == CC_EXPR_SLICE || e->kind == CC_EXPR_SLICE_LO){
            int64_t lo;
            err = cc_eval_integer(ctx, e->values[0], &lo);
            if(err){ cc_release_expr(p, data); return err; }
            if(lo){
                CcExpr* index = cc_int64_expr(p, e->loc, ccqt_basic(CCBT_long_long), lo);
                CcExpr* add = index ? cc_binary_expr(p, CC_EXPR_ADD, e->loc, pointer, data, index) : NULL;
                if(!add){
                    if(index) cc_release_expr(p, index);
                    cc_release_expr(p, data);
                    return CC_OOM_ERROR;
                }
                data = add;
            }
        }
    }
    CcExpr* length = cc_uint64_expr(p, e->loc, ccqt_basic(cc_target(p)->size_type), count);
    CcInitList* list = Allocator_zalloc(cc_allocator(p), sizeof *list + 2 * sizeof(CcInitEntry));
    CcExpr* value = cc_make_expr(p, CC_EXPR_INIT_LIST, e->loc, e->type, 0);
    if(!length || !list || !value){
        cc_release_expr(p, data);
        if(length) cc_release_expr(p, length);
        if(list) Allocator_free(cc_allocator(p), list, sizeof *list + 2 * sizeof(CcInitEntry));
        if(value) _cc_release_expr(p, value, 0);
        return CC_OOM_ERROR;
    }
    list->loc = e->loc;
    list->count = 2;
    list->entries[0] = (CcInitEntry){.path={.n_components=1, .idx0=0}, .value=length};
    list->entries[1] = (CcInitEntry){.path={.n_components=1, .idx0=1}, .value=data};
    value->init_list = list;
    *out = value;
    return 0;
}

static
_Bool
cc_eval_aggregate_type(CcQualType type){
    CcTypeKind kind = ccqt_kind(type);
    return kind == CC_STRUCT || kind == CC_UNION || kind == CC_ARRAY || kind == CC_SLICE
        || ccqt_bt_eq(type, CCBT__Any);
}

// An omitted union initializes its first storage member. Another member must
// read that member's representation rather than invent a zero of its own type.
static _Bool
cc_eval_default_path(CcQualType type, CcFieldPath path){
    for(uint32_t i = 0, n = cc_field_path_count(path); i < n; i++){
        uint32_t index = cc_field_path_component(path, i);
        CcTypeKind kind = ccqt_kind(type);
        if(kind == CC_STRUCT || kind == CC_UNION){
            CcStruct* aggregate = ccqt_as_struct(type);
            if(index >= aggregate->field_count) return 0;
            if(kind == CC_UNION){
                uint32_t first = 0;
                while(first < aggregate->field_count && (aggregate->fields[first].is_method
                    || (aggregate->fields[first].is_bitfield && !aggregate->fields[first].name))) first++;
                if(index != first) return 0;
            }
            type = aggregate->fields[index].type;
        }
        else if(kind == CC_ARRAY) type = ccqt_as_array(type)->element;
        else return 1; // Slice and _Any pseudo-members are checked separately.
    }
    return 1;
}

// An unhandled selection needs representation-level evaluation: for example,
// a read through a different union member. Ordinary subobjects use identity.
static
int
cc_eval_path_select(CcEvalCtx* ctx, CcExpr* base, CcFieldPath path, CcQualType type, unsigned depth,
    _Bool expression, _Bool* handled, CcExpr*_Nullable*_Nonnull out){
    CcParser* p = ctx->parser;
    *handled = 0;
    if(depth >= 256 || (base->is_lvalue && (base->type.is_volatile || base->type.is_atomic)))
        return CC_NOT_CONSTANT_ERROR;
    uint32_t count = cc_field_path_count(path);
    if(!count){
        int err = expression || cc_eval_aggregate_type(type) ? cc_clone_initializer_expr(p, base, depth, out)
            : cc_eval_expr(ctx, base, out);
        if(!err) *handled = 1;
        return err;
    }
    int source_err = cc_eval_object_source(ctx, base, depth, &base);
    if(source_err) return source_err;
    if(base->kind == CC_EXPR_TYPE_INTROSPECTION){
        CcExpr* value;
        int err = cc_eval_expr(ctx, base, &value);
        if(err) return err;
        err = cc_eval_path_select(ctx, value, path, type, depth+1, expression, handled, out);
        cc_release_expr(p, value);
        return err;
    }
    if(base->kind == CC_EXPR_DOT || base->kind == CC_EXPR_ARROW
        || base->kind == CC_EXPR_SUBSCRIPT || base->kind == CC_EXPR_DEREF){
        CcExpr* value;
        int err = cc_eval_expr(ctx, base, &value);
        if(err == CC_NOT_CONSTANT_ERROR) return 0;
        if(err) return err;
        // Reading a representation requires the storage reader below.
        if(value->kind == CC_EXPR_OBJECT_VIEW){
            cc_release_expr(p, value);
            return 0;
        }
        err = cc_eval_path_select(ctx, value, path, type, depth+1, expression, handled, out);
        cc_release_expr(p, value);
        return err;
    }
    if(base->kind == CC_EXPR_CAST && ccqt_bt_eq(base->type, CCBT__Any)){
        uint32_t index = cc_field_path_component(path, 0);
        if(!index && count == 1){
            CcExpr* tag = cc_value_expr(p, base->loc, type);
            if(!tag) return CC_OOM_ERROR;
            tag->type_value = (CcQualType){.unqual=base->lhs->type.unqual};
            *out = tag;
            *handled = 1;
            return 0;
        }
        if(index == 1){
            CcFieldPath tail;
            if(cc_field_path_drop(cc_allocator(p), path, 1, &tail)) return CC_OOM_ERROR;
            int err = cc_eval_path_select(ctx, base->lhs, tail, type, depth+1, expression, handled, out);
            cc_field_path_free(cc_allocator(p), tail);
            return err;
        }
        return 0;
    }
    if(base->kind == CC_EXPR_VALUE && ccqt_kind(base->type) == CC_ARRAY && base->text && count == 1){
        uint32_t index = cc_field_path_component(path, 0), size;
        int err = cc_sizeof_as_uint(p, type, base->loc, &size);
        if(err) return err;
        CcExpr* value = cc_value_expr(p, base->loc, type);
        if(!value) return CC_OOM_ERROR;
        if(index < base->str.length){
            if(size > sizeof value->data){ cc_release_expr(p, value); return CC_NOT_CONSTANT_ERROR; }
            memcpy(value->data, base->text + (size_t)index * size, size);
            cc_eval_truncate(p, value);
        }
        *out = value;
        *handled = 1;
        return 0;
    }
    if(base->kind != CC_EXPR_INIT_LIST && base->kind != CC_EXPR_COMPOUND_LITERAL) return 0;
    CcInitList* list = base->init_list;
    _Bool aggregate = cc_eval_aggregate_type(type);
    uint32_t first = 0;
    CcExpr* replacement = NULL;
    for(uint32_t i = list->count; i; i--){
        CcInitEntry* entry = &list->entries[i-1];
        if(!entry->value || !cc_field_paths_overlap(base->type, path, entry->path)) continue;
        if(cc_field_path_is_prefix(entry->path, path)){
            CcFieldPath tail;
            if(cc_field_path_drop(cc_allocator(p), path, cc_field_path_count(entry->path), &tail)) return CC_OOM_ERROR;
            int err = cc_eval_path_select(ctx, entry->value, tail, type, depth+1, expression, handled, &replacement);
            cc_field_path_free(cc_allocator(p), tail);
            if(err || !*handled) return err;
            if(!aggregate){ *out = replacement; return 0; }
            first = i;
            break;
        }
        if(!aggregate || !cc_field_path_is_prefix(path, entry->path)) return 0;
    }
    if(!replacement && !cc_eval_default_path(base->type, path)) return 0;
    if(!aggregate){
        *out = cc_value_expr(p, base->loc, type);
        if(!*out) return CC_OOM_ERROR;
        *handled = 1;
        return 0;
    }
    uint32_t n = replacement != NULL;
    for(uint32_t i = first; i < list->count; i++)
        if(list->entries[i].value && cc_field_path_is_prefix(path, list->entries[i].path)) n++;
    if(replacement && n == 1){
        *out = replacement;
        *handled = 1;
        return 0;
    }
    size_t size;
    if(mul_overflow((size_t)n, sizeof(CcInitEntry), &size) || add_overflow(sizeof(CcInitList), size, &size)){
        if(replacement) cc_release_expr(p, replacement);
        return CC_OOM_ERROR;
    }
    CcInitList* view = Allocator_zalloc(cc_allocator(p), size);
    CcExpr* value = cc_make_expr(p, CC_EXPR_INIT_LIST, base->loc, type, 0);
    if(!view || !value){
        if(replacement) cc_release_expr(p, replacement);
        if(view) Allocator_free(cc_allocator(p), view, size);
        if(value) _cc_release_expr(p, value, 0);
        return CC_OOM_ERROR;
    }
    view->loc = base->loc;
    view->count = n;
    value->init_list = view;
    uint32_t at = 0;
    if(replacement) view->entries[at++].value = replacement;
    int err = 0;
    for(uint32_t i = first; i < list->count && !err; i++){
        CcInitEntry* entry = &list->entries[i];
        if(!entry->value || !cc_field_path_is_prefix(path, entry->path)) continue;
        CcInitEntry* dest = &view->entries[at++];
        if(cc_field_path_drop(cc_allocator(p), entry->path, count, &dest->path)) err = CC_OOM_ERROR;
        if(!err) err = cc_clone_initializer_expr(p, entry->value, depth+1, &dest->value);
    }
    if(err){ cc_release_expr(p, value); return err; }
    *handled = 1;
    *out = value;
    return 0;
}

// A borrowed object plus an owned path is a view, not a flattened value.
// Only API boundaries that return CcExpr materialize an initializer or clone.

// Recover subobject identity from a symbolic address when the requested type
// matches a real subobject. Casts into a different representation keep only
// the byte offset; they cannot manufacture an initializer path.
static
int
cc_eval_offset_path(CcParser* p, CcQualType owner, uint64_t offset, CcQualType type,
    unsigned depth, CcFieldPath* path, _Bool* found){
    *found = 0;
    if(depth >= 256) return CC_NOT_CONSTANT_ERROR;
    if(!offset && owner.unqual == type.unqual){ *path = (CcFieldPath){0}; *found = 1; return 0; }
    CcTypeKind kind = ccqt_kind(owner);
    uint32_t count = kind == CC_ARRAY ? 1
        : kind == CC_STRUCT || kind == CC_UNION ? ccqt_as_struct(owner)->field_count : 0;
    for(uint32_t i = 0; i < count; i++){
        uint32_t index = i;
        uint64_t start = 0;
        CcQualType child;
        if(kind == CC_ARRAY){
            CcArray* array = ccqt_as_array(owner);
            if(array->is_incomplete) return CC_NOT_CONSTANT_ERROR;
            child = array->element;
            uint32_t size;
            int err = cc_sizeof_as_uint(p, child, (SrcLoc){0}, &size);
            if(err) return err;
            if(!size || offset / size >= array->length || offset / size > UINT32_MAX) return 0;
            index = (uint32_t)(offset / size);
            start = (uint64_t)index * size;
        }
        else {
            CcField* field = &ccqt_as_struct(owner)->fields[i];
            if(field->is_method || field->is_bitfield || field->offset > offset) continue;
            if(ccqt_kind(field->type) == CC_ARRAY && ccqt_as_array(field->type)->is_incomplete) continue;
            start = field->offset;
            child = field->type;
        }
        uint32_t size;
        int err = cc_sizeof_as_uint(p, child, (SrcLoc){0}, &size);
        if(err) return err;
        if(offset - start >= size) continue;
        CcFieldPath tail = {0};
        err = cc_eval_offset_path(p, child, offset - start, type, depth+1, &tail, found);
        if(err) return err;
        if(!*found) continue;
        CcFieldPath step;
        if(cc_field_path_make(cc_allocator(p), &index, 1, &step)){
            cc_field_path_free(cc_allocator(p), tail);
            return CC_OOM_ERROR;
        }
        err = cc_field_path_concat(cc_allocator(p), step, tail, path) ? CC_OOM_ERROR : 0;
        cc_field_path_free(cc_allocator(p), step);
        cc_field_path_free(cc_allocator(p), tail);
        return err;
    }
    return 0;
}

static
int
cc_eval_object_view_address(CcEvalCtx* ctx, CcEvalObjectView* view, CcEvalAddress address, CcQualType type){
    CcParser* p = ctx->parser;
    if(address.offset < 0 || !address.symbol) return CC_NOT_CONSTANT_ERROR;
    if(address.kind == CC_EVAL_VAR){
        CcVariable* var = (CcVariable*)(uintptr_t)address.symbol;
        *view->storage = (CcExpr){.kind=CC_EXPR_VARIABLE, .is_lvalue=1, .type=var->type, .var=var};
        if(!(var->constexpr_ || (ctx->allow_const && cc_linktime_const_variable(view->storage))))
            return CC_NOT_CONSTANT_ERROR;
    }
    else if(address.kind == CC_EVAL_LITERAL){
        *view->storage = (CcExpr){.kind=CC_EXPR_VALUE, .type=address.literal_type, .text=(const char*_Nonnull)address.symbol};
        if(ccqt_as_array(address.literal_type)->length > UINT32_MAX) return CC_NOT_CONSTANT_ERROR;
        view->storage->str.length = (uint32_t)ccqt_as_array(address.literal_type)->length;
    }
    else return CC_NOT_CONSTANT_ERROR;
    if(view->storage->type.is_volatile || view->storage->type.is_atomic) return CC_NOT_CONSTANT_ERROR;
    view->object = view->storage;
    uint32_t object_size, size = 0;
    int err = cc_sizeof_as_uint(p, view->storage->type, (SrcLoc){0}, &object_size);
    if(!err) err = cc_sizeof_as_uint(p, type, (SrcLoc){0}, &size);
    if(err) return err;
    if((uint64_t)address.offset > object_size || size > object_size - (uint64_t)address.offset)
        return CC_NOT_CONSTANT_ERROR;
    if(address.has_range){
        if(address.offset < address.range_start) return CC_NOT_CONSTANT_ERROR;
        uint64_t delta = (uint64_t)address.offset - (uint64_t)address.range_start;
        if(delta > address.range_size || size > address.range_size - delta)
            return CC_NOT_CONSTANT_ERROR;
    }
    CcFieldPath prefix = {0};
    _Bool found;
    err = cc_eval_offset_path(p, view->storage->type, (uint64_t)address.offset, type, view->depth, &prefix, &found);
    if(err) return err;
    if(found){
        CcFieldPath joined;
        err = cc_field_path_concat(cc_allocator(p), prefix, view->path, &joined) ? CC_OOM_ERROR : 0;
        cc_field_path_free(cc_allocator(p), prefix);
        if(err) return err;
        cc_field_path_free(cc_allocator(p), view->path);
        view->path = joined;
    }
    else view->representation = 1;
    return add_overflow(view->byte_offset, (uint64_t)address.offset, &view->byte_offset) ? CC_OVERFLOW_ERROR : 0;
}

// Gather identity before consulting initializer contents. Indirect accesses
// resolve symbol plus offset back to the same object view used by arrays.
static
int
cc_eval_object_view_open(CcEvalCtx* ctx, CcExpr* e, CcEvalObjectView* view){
    CcParser* p = ctx->parser;
    CcExpr* base = e;
    int err = 0;
    while(base->kind == CC_EXPR_DOT || base->kind == CC_EXPR_ARROW
        || base->kind == CC_EXPR_SUBSCRIPT || base->kind == CC_EXPR_DEREF){
        if(view->depth++ >= 256){ err = CC_NOT_CONSTANT_ERROR; goto cleanup; }
        CcFieldPath step = {0};
        uint64_t step_offset = 0;
        if(base->kind == CC_EXPR_DEREF || (base->kind == CC_EXPR_SUBSCRIPT && ccqt_kind(base->lhs->type) != CC_ARRAY)){
            if(base->kind == CC_EXPR_SUBSCRIPT && ccqt_kind(base->lhs->type) == CC_SLICE){
                int64_t index;
                uint64_t count = 0;
                CcEvalAddress data;
                err = cc_eval_integer(ctx, base->values[0], &index);
                if(!err) err = cc_eval_slice(ctx, base->lhs, view->depth, &count, &data);
                if(err) goto cleanup;
                if(index < 0 || (uint64_t)index >= count){ err = CC_NOT_CONSTANT_ERROR; goto cleanup; }
            }
            CcEvalAddress address;
            err = base->kind == CC_EXPR_DEREF ? cc_eval_address(ctx, base->lhs, 0, view->depth, &address)
                : cc_eval_address(ctx, base, 1, view->depth, &address);
            if(!err) err = cc_eval_object_view_address(ctx, view, address, base->type);
            if(err) goto cleanup;
            base = view->object;
            break;
        }
        CcExpr* indirect = NULL;
        CcQualType indirect_type = {0};
        if(base->kind == CC_EXPR_DOT || base->kind == CC_EXPR_ARROW){
            err = cc_eval_check_any_view(ctx, base);
            if(err) goto cleanup;
            step = base->field_path;
            err = cc_field_path_resolve(cc_target(p), cc_expr_field_owner(base), step, &step_offset);
            if(err) goto cleanup;
            if(base->kind == CC_EXPR_ARROW){
                indirect = base->values[0];
                indirect_type = cc_expr_field_owner(base);
            }
            base = base->values[0];
        }
        else {
            int64_t index;
            err = cc_eval_integer(ctx, base->values[0], &index);
            if(err) goto cleanup;
            if(index < 0 || (uint64_t)index >= ccqt_as_array(base->lhs->type)->length || (uint64_t)index > UINT32_MAX){
                err = CC_NOT_CONSTANT_ERROR;
                goto cleanup;
            }
            uint32_t component = (uint32_t)index;
            if(cc_field_path_make(cc_allocator(p), &component, 1, &step)){ err = CC_OOM_ERROR; goto cleanup; }
            err = cc_field_path_resolve(cc_target(p), base->lhs->type, step, &step_offset);
            if(err) goto cleanup;
            base = base->lhs;
        }
        if(add_overflow(view->byte_offset, step_offset, &view->byte_offset)){
            err = CC_OVERFLOW_ERROR;
            goto cleanup;
        }
        CcFieldPath joined;
        if(cc_field_path_concat(cc_allocator(p), step, view->path, &joined)){ err = CC_OOM_ERROR; goto cleanup; }
        cc_field_path_free(cc_allocator(p), view->path);
        view->path = joined;
        if(indirect){
            CcEvalAddress address;
            err = cc_eval_address(ctx, indirect, 0, view->depth, &address);
            if(!err) err = cc_eval_object_view_address(ctx, view, address, indirect_type);
            if(err) goto cleanup;
            base = view->object;
            break;
        }
    }
    view->object = base;
    return 0;
    cleanup:
    cc_field_path_free(cc_allocator(p), view->path);
    view->path = (CcFieldPath){0};
    return err;
}

static
int
cc_eval_linktime_object_view(CcParser* p, CcExpr* e, CcEvalObjectView* view){
    CcEvalCtx ctx = {.parser=p, .allow_const=1};
    return cc_eval_object_view_open(&ctx, e, view);
}

static
int
cc_eval_object_view_range(CcEvalCtx* ctx, CcExpr* e, uint64_t offset, CcEvalObjectView* view){
    int err = cc_eval_object_view_open(ctx, e, view);
    if(err) return err;
    if(add_overflow(view->byte_offset, offset, &view->byte_offset)){
        cc_field_path_free(cc_allocator(ctx->parser), view->path);
        view->path = (CcFieldPath){0};
        return CC_OVERFLOW_ERROR;
    }
    return 0;
}

static
int
cc_eval_object_view_select(CcEvalCtx* ctx, CcExpr* e, _Bool expression, _Bool* handled, CcExpr*_Nullable*_Nonnull out){
    CcParser* p = ctx->parser;
    CcExpr storage = {0};
    CcEvalObjectView view = {.object=e, .storage=&storage};
    *handled = 0;
    int err = cc_eval_object_view_open(ctx, e, &view);
    if(err) return err;
    uint32_t bit_shift = 0;
    CcExpr* selected = NULL;
    if(!view.representation)
        err = cc_eval_path_select(ctx, view.object, view.path, e->type, view.depth, expression, handled, &selected);
    if(!expression && !err && !*handled && !cc_eval_aggregate_type(e->type)){
        unsigned char requested[16] = {0};
        uint32_t width = cc_expr_field_bit_width(e);
        if(view.byte_offset > UINT32_MAX){ err = CC_NOT_CONSTANT_ERROR; goto cleanup; }
        if(width){
            uint32_t size;
            err = cc_sizeof_as_uint(p, e->type, e->loc, &size);
            if(err) goto cleanup;
            bit_shift = cc_expr_field_bit_offset(e);
            if(size > sizeof requested || bit_shift >= 128 || bit_shift > size * 8 || width > size * 8 - bit_shift){
                err = CC_NOT_CONSTANT_ERROR;
                goto cleanup;
            }
            for(uint32_t bit = bit_shift; bit < bit_shift + width; bit++)
                requested[bit / 8] |= 1u << (bit % 8);
        }
        err = cc_eval_object_scalar(ctx, view.object, (uint32_t)view.byte_offset,
            e->type, e->loc, width ? requested : NULL, &selected);
        if(err == CC_NOT_CONSTANT_ERROR) err = 0;
        else if(!err) *handled = 1;
    }
    if(!expression && !err && !*handled && cc_eval_aggregate_type(e->type)){
        // Keep a typed view of the source storage, including padding,
        // relocations and metadata across union views.
        uint32_t size = 0;
        err = cc_check_linktime_expr_inner(p, e, 0, 1, 0);
        if(!err) err = cc_sizeof_as_uint(p, e->type, e->loc, &size);
        if(!err) err = cc_check_linktime_view(ctx, e, e, 0, size, 0, 0);
        if(!err){
            selected = cc_make_expr(p, CC_EXPR_OBJECT_VIEW, e->loc, e->type, 0);
            if(!selected) err = CC_OOM_ERROR;
            else {
                err = cc_clone_initializer_expr(p, e, view.depth, &selected->lhs);
                if(err){ cc_release_expr(p, selected); selected = NULL; }
                else *handled = 1;
            }
        }
    }
    if(err || !*handled) goto cleanup;
    if(!expression && cc_eval_aggregate_type(e->type)){
        err = cc_eval_expr(ctx, selected, out);
        cc_release_expr(p, selected);
    }
    else *out = selected;
    if(!err){
        CcExpr* value = *out;
        if(!value){ err = CC_UNREACHABLE_ERROR; goto cleanup; }
        if(value->type.unqual != e->type.unqual){
            cc_release_expr(p, value);
            *out = NULL;
            *handled = 0;
            goto cleanup;
        }
        value->type = e->type;
        uint32_t width = expression ? 0 : cc_expr_field_bit_width(e);
        if(width){
            CiUint128 bits = cc_eval_u128(p, value);
            if(bit_shift) bits = ci_uint128_shr(bits, bit_shift);
            CiUint128 mask = width == 128 ? ci_uint128_from_int64(-1)
                : ci_uint128_sub(ci_uint128_shl(ci_uint128_from_uint64(1), width), ci_uint128_from_uint64(1));
            bits = ci_uint128_and(bits, mask);
            if(!ccqt_is_unsigned(e->type, !cc_target(p)->char_is_signed)
                && ci_uint128_nonzero(ci_uint128_shr(bits, width-1)))
                bits = ci_uint128_or(bits, ci_uint128_xor(mask, ci_uint128_from_int64(-1)));
            if(cc_eval_wide(e->type)) value->uinteger128 = bits;
            else value->uinteger = ci_uint128_lo(bits);
        }
    }
    cleanup:
    cc_field_path_free(cc_allocator(p), view.path);
    return err;
}

static int cc_eval_expr_inner(CcEvalCtx*, CcExpr*, CcExpr*_Nullable*_Nonnull);

static
int
cc_eval_expr(CcEvalCtx* ctx, CcExpr* e, CcExpr*_Nullable*_Nonnull result){
    if(ctx->evaluation_depth >= 256) return CC_NOT_CONSTANT_ERROR;
    if(e->is_lvalue && (e->type.is_volatile || e->type.is_atomic))
        return CC_NOT_CONSTANT_ERROR;
    for(CcEvalVisit* visit = ctx->expressions; visit; visit = visit->previous)
        if(visit->expr == e) return CC_NOT_CONSTANT_ERROR;
    CcEvalVisit visit = {.previous = ctx->expressions, .expr = e};
    ctx->expressions = &visit;
    ctx->evaluation_depth++;
    int err = cc_eval_expr_inner(ctx, e, result);
    ctx->evaluation_depth--;
    ctx->expressions = visit.previous;
    return err;
}

static
int
cc_eval_expr_inner(CcEvalCtx* ctx, CcExpr* e, CcExpr*_Nullable*_Nonnull result){
    CcParser* p = ctx->parser;
    if(cc_eval_slice_expr(e)) return cc_eval_slice_value(ctx, e, result);
    if(cc_eval_pointer_binary_expr(e)){
        int64_t value;
        int err = cc_eval_pointer_binary(ctx, e, &value);
        if(err) return err;
        *result = cc_int64_expr(p, e->loc, e->type, value);
        return *result ? 0 : CC_OOM_ERROR;
    }
    switch(e->kind){
        DRP_CASES_EXHAUSTED;
        case CC_EXPR_VALUE: {
            CcExpr* node = _cc_alloc_expr(p, 0);
            if(!node) return CC_OOM_ERROR;
            memcpy(node, e, sizeof *e);
            *result = node;
            return 0;
        }
        case CC_EXPR_NEG: {
            CcExpr* operand;
            int err = cc_eval_expr(ctx, e->lhs, &operand);
            if(err) return err;
            if(!ccqt_is_basic(e->type)){
                err = CC_UNREACHABLE_ERROR;
                goto fini_neg;
            }
            if(e->type.unqual != operand->type.unqual){
                err = CC_UNREACHABLE_ERROR;
                goto fini_neg;
            }
            if(cc_eval_wide(operand->type)){
                CcExpr* wide = cc_value_expr(p, e->loc, e->type);
                if(!wide){ err = CC_OOM_ERROR; goto fini_neg; }
                if(ccqt_is_integer(operand->type)){
                    CiUint128 u = operand->uinteger128;
                    if(ccqt_bt_eq(operand->type, CCBT_int128) && ci_uint128_eq(u, ci_uint128_shl(ci_uint128_from_uint64(1),127))) err = CC_OVERFLOW_ERROR;
                    else wide->uinteger128 = ci_uint128_sub(ci_uint128_from_uint64(0), u);
                }
                else cc_eval_store_quad(p, wide, ci_float128_neg(cc_eval_quad(p, operand)));
                if(err) cc_release_expr(p, wide);
                else *result = wide;
                goto fini_neg;
            }
            CcExpr* node;
            switch(e->type.basic.kind){
                DRP_CASES_EXHAUSTED;
                case CCBT__Any:
                    return CC_UNIMPLEMENTED_ERROR;
                case CCBT_INVALID:
                case CCBT_void:
                case CCBT_nullptr_t:
                case CCBT__Type:
                case CCBT_COUNT:
                    err = CC_UNREACHABLE_ERROR;
                    break;
                case CCBT_bool:
                case CCBT_char:
                case CCBT_signed_char:
                case CCBT_unsigned_char:
                case CCBT_short:
                case CCBT_unsigned_short:
                    // Smaller than int types should've been promoted already
                    err = CC_UNREACHABLE_ERROR;
                    break;
                // For our supported targets, the only things that don't
                // correspond to fixed-sized types is long on win64.
                case CCBT_int:
                    neg_int:;{
                    int32_t val;
                    if(sub_overflow((int32_t)0, (int32_t)operand->integer, &val)){
                        err = CC_OVERFLOW_ERROR;
                        break;
                    }
                    node = cc_value_expr(p, e->loc, e->type);
                    if(!node) {err = CC_OOM_ERROR; break;}
                    node->integer = val;
                    *result = node;
                    break;
                }
                case CCBT_long:
                    if(cc_target(p)->sizeof_[CCBT_long] == 8)
                        goto neg_long_long;
                    goto neg_int;
                case CCBT_long_long:
                    neg_long_long:;{
                    int64_t val;
                    if(sub_overflow((int64_t)0, operand->integer, &val)){
                        err = CC_OVERFLOW_ERROR;
                        break;
                    }
                    node = cc_value_expr(p, e->loc, e->type);
                    if(!node) {err = CC_OOM_ERROR; break;}
                    node->integer = val;
                    *result = node;
                    break;
                }
                case CCBT_unsigned:
                case CCBT_unsigned_long:
                case CCBT_unsigned_long_long:{
                    node = cc_value_expr(p, e->loc, e->type);
                    if(!node) {err = CC_OOM_ERROR; break;}
                    node->uinteger = -operand->uinteger;
                    cc_eval_truncate(p, node);
                    *result = node;
                    break;
                }
                case CCBT_float:{
                    node = cc_value_expr(p, e->loc, e->type);
                    if(!node) {err = CC_OOM_ERROR; break;}
                    node->float_ = -operand->float_;
                    *result = node;
                    break;
                }
                case CCBT_double:{
                    node = cc_value_expr(p, e->loc, e->type);
                    if(!node) {err = CC_OOM_ERROR; break;}
                    node->double_ = -operand->double_;
                    *result = node;
                    break;
                }
                case CCBT_int128:
                case CCBT_unsigned_int128:
                case CCBT_float16:
                case CCBT_long_double:
                case CCBT_float128:
                case CCBT_float_complex:
                case CCBT_double_complex:
                case CCBT_long_double_complex:
                    err = CC_UNIMPLEMENTED_ERROR;
                    break;
            }
            fini_neg:;
            cc_release_expr(p, operand);
            return err;
        }
        case CC_EXPR_POS: return cc_eval_expr(ctx, e->lhs, result);
        case CC_EXPR_BITNOT: {
            CcExpr* operand;
            int err = cc_eval_expr(ctx, e->lhs, &operand);
            if(err) return err;
            if(!ccqt_is_basic(e->type)){
                err = CC_UNREACHABLE_ERROR;
                goto fini_bitnot;
            }
            if(e->type.unqual != operand->type.unqual){
                err = CC_UNREACHABLE_ERROR;
                goto fini_bitnot;
            }
            if(cc_eval_wide(operand->type)){
                CcExpr* wide = cc_value_expr(p, e->loc, e->type);
                if(!wide){ err = CC_OOM_ERROR; goto fini_bitnot; }
                wide->uinteger128 = ci_uint128_xor(operand->uinteger128, ci_uint128_sub(ci_uint128_from_uint64(0), ci_uint128_from_uint64(1)));
                if(err) cc_release_expr(p, wide);
                else *result = wide;
                goto fini_bitnot;
            }
            CcExpr* node;
            switch(e->type.basic.kind){
                DRP_CASES_EXHAUSTED;
                case CCBT__Any:
                    return CC_UNIMPLEMENTED_ERROR;
                case CCBT_INVALID:
                case CCBT_void:
                case CCBT_nullptr_t:
                case CCBT__Type:
                case CCBT_COUNT:
                    err = CC_UNREACHABLE_ERROR;
                    goto fini_bitnot;
                case CCBT_bool:
                case CCBT_char:
                case CCBT_signed_char:
                case CCBT_unsigned_char:
                case CCBT_short:
                case CCBT_unsigned_short:
                    // Smaller than int types should've been promoted already
                    err = CC_UNREACHABLE_ERROR;
                    goto fini_bitnot;
                // For our supported targets, the only things that don't
                // correspond to fixed-sized types is long on win64.
                case CCBT_int:
                case CCBT_long:
                case CCBT_long_long:
                case CCBT_unsigned:
                case CCBT_unsigned_long:
                case CCBT_unsigned_long_long:
                    node = cc_value_expr(p, e->loc, e->type);
                    if(!node) {err = CC_OOM_ERROR; goto fini_bitnot;}
                    node->uinteger = ~operand->uinteger;
                    cc_eval_truncate(p, node);
                    *result = node;
                    err = 0;
                    goto fini_bitnot;
                case CCBT_float:
                case CCBT_double:
                    err = CC_UNREACHABLE_ERROR;
                    break;
                case CCBT_int128:
                case CCBT_unsigned_int128:
                case CCBT_float16:
                case CCBT_long_double:
                case CCBT_float128:
                case CCBT_float_complex:
                case CCBT_double_complex:
                case CCBT_long_double_complex:
                    err = CC_UNIMPLEMENTED_ERROR;
                    break;
            }
            fini_bitnot:;
            cc_release_expr(p, operand);
            return err;
        }
        case CC_EXPR_LOGNOT: {
            if(cc_eval_address_type(e->lhs->type)){
                _Bool truth;
                int err = cc_eval_truthy(ctx, e->lhs, &truth);
                if(err) return err;
                *result = cc_int64_expr(p, e->loc, e->type, !truth);
                return *result ? 0 : CC_OOM_ERROR;
            }
            CcExpr* operand;
            int err = cc_eval_expr(ctx, e->lhs, &operand);
            if(err) return err;
            CcQualType ot = operand->type;
            while(ccqt_kind(ot) == CC_ENUM)
                ot = ccqt_as_enum(ot)->underlying;
            if(!ccqt_is_basic(ot)){
                err = CC_UNREACHABLE_ERROR;
                goto fini_lognot;
            }
            if(!ccqt_bt_eq(e->type, CCBT_int)){
                err = CC_UNREACHABLE_ERROR;
                goto fini_lognot;
            }
            if(cc_eval_wide(operand->type)){
                CcExpr* wide = cc_value_expr(p, e->loc, e->type);
                if(!wide){ err = CC_OOM_ERROR; goto fini_lognot; }
                wide->integer = !cc_eval_value_truth(p, operand);
                if(err) cc_release_expr(p, wide);
                else *result = wide;
                goto fini_lognot;
            }
            CcExpr* node;
            switch(ot.basic.kind){
                #define LOGNOT(fixed) do { \
                    node = cc_value_expr(p, e->loc, e->type); \
                    if(!node) {err = CC_OOM_ERROR; goto fini_lognot;}\
                    node->integer = !operand->fixed; \
                    *result = node; \
                    err = 0; \
                    goto fini_lognot; \
                }while(0)
                DRP_CASES_EXHAUSTED;
                case CCBT_INVALID:
                case CCBT_void:
                case CCBT_nullptr_t:
                    node = cc_value_expr(p, e->loc, e->type);
                    if(!node) { err = CC_OOM_ERROR; goto fini_lognot;}
                    node->integer = 1;
                    *result = node;
                    goto fini_lognot;
                case CCBT__Any:
                case CCBT__Type:
                case CCBT_COUNT:
                    err = CC_UNREACHABLE_ERROR;
                    goto fini_lognot;
                case CCBT_bool:
                    LOGNOT(uinteger);
                case CCBT_char:
                    LOGNOT(integer); // signedness doesn't matter for !
                case CCBT_signed_char:
                    LOGNOT(integer);
                case CCBT_unsigned_char:
                    LOGNOT(uinteger);
                case CCBT_short:
                    LOGNOT(integer);
                case CCBT_unsigned_short:
                    LOGNOT(uinteger);
                case CCBT_int:
                    lognot_int:
                    LOGNOT(integer);
                case CCBT_unsigned:
                    lognot_unsigned:;
                    LOGNOT(uinteger);
                case CCBT_long:
                    if(cc_target(p)->sizeof_[CCBT_long] == 8)
                        goto lognot_longlong;
                    else goto lognot_int;
                case CCBT_long_long:
                    lognot_longlong:;
                    LOGNOT(integer);
                case CCBT_unsigned_long:
                    if(cc_target(p)->sizeof_[CCBT_unsigned_long] == 8)
                        goto lognot_unsigned_longlong;
                    goto lognot_unsigned;
                case CCBT_unsigned_long_long:
                    lognot_unsigned_longlong:
                    LOGNOT(uinteger);
                case CCBT_float:
                    LOGNOT(float_);
                case CCBT_double:
                    LOGNOT(double_);
                case CCBT_int128:
                case CCBT_unsigned_int128:
                case CCBT_float16:
                case CCBT_long_double:
                case CCBT_float128:
                case CCBT_float_complex:
                case CCBT_double_complex:
                case CCBT_long_double_complex:
                    err = CC_UNIMPLEMENTED_ERROR;
                    goto fini_lognot;
                #undef LOGNOT
            }
            fini_lognot:;
            cc_release_expr(p, operand);
            return err;
        }
        case CC_EXPR_SUBSCRIPT: {
            // Handle string_literal[constant_index]
            CcExpr* arr = e->lhs;
            if(arr->kind == CC_EXPR_VALUE && arr->str.length && arr->text){
                int64_t i;
                int err = cc_eval_integer(ctx, e->values[0], &i);
                if(err) return err;
                if(i < 0 || (uint64_t)i >= arr->str.length)
                    return CC_VALUE_ERROR;
                CcExpr* node = cc_value_expr(p, e->loc, e->type);
                if(!node) return CC_OOM_ERROR;
                CcArray* arr_type = ccqt_as_array(arr->type);
                uint32_t elem_sz = cc_target(p)->sizeof_[arr_type->element.basic.kind];
                if(elem_sz == 4)
                    node->uinteger = ((const uint32_t*)arr->text)[i];
                else if(elem_sz == 2)
                    node->uinteger = ((const uint16_t*)arr->text)[i];
                else if(cc_target(p)->char_is_signed)
                    node->integer = (signed char)arr->text[i];
                else
                    node->uinteger = (unsigned char)arr->text[i];
                *result = node;
                return 0;
            }
            goto eval_init_list_access;
        }
        case CC_EXPR_COMMA: {
            CcExpr* source;
            int err = cc_eval_comma_source(ctx, e, 0, &source);
            if(err) return err;
            return cc_eval_expr(ctx, source, result);
        }
        case CC_EXPR_TERNARY: {
            _Bool truthy;
            int err = cc_eval_truthy(ctx, e->lhs, &truthy);
            if(err) return err;
            return cc_eval_expr(ctx, e->values[truthy ? 0 : 1], result);
        }
        case CC_EXPR_LOGAND: {
            _Bool truthy;
            int err = cc_eval_truthy(ctx, e->lhs, &truthy);
            if(err) return err;
            if(!truthy){
                *result = cc_int64_expr(p, e->loc, e->type, 0);
                return *result ? 0 : CC_OOM_ERROR;
            }
            err = cc_eval_truthy(ctx, e->values[0], &truthy);
            if(err) return err;
            *result = cc_int64_expr(p, e->loc, e->type, truthy ? 1 : 0);
            return *result ? 0 : CC_OOM_ERROR;
        }
        case CC_EXPR_LOGOR: {
            _Bool truthy;
            int err = cc_eval_truthy(ctx, e->lhs, &truthy);
            if(err) return err;
            if(truthy){
                *result = cc_int64_expr(p, e->loc, e->type, 1);
                return *result ? 0 : CC_OOM_ERROR;
            }
            err = cc_eval_truthy(ctx, e->values[0], &truthy);
            if(err) return err;
            *result = cc_int64_expr(p, e->loc, e->type, truthy ? 1 : 0);
            return *result ? 0 : CC_OOM_ERROR;
        }
        // Binary arithmetic and comparisons
        case CC_EXPR_ADD: case CC_EXPR_SUB: case CC_EXPR_MUL:
        case CC_EXPR_DIV: case CC_EXPR_MOD:
        case CC_EXPR_BITAND: case CC_EXPR_BITOR: case CC_EXPR_BITXOR:
        case CC_EXPR_LSHIFT: case CC_EXPR_RSHIFT:
        case CC_EXPR_EQ: case CC_EXPR_NE:
        case CC_EXPR_LT: case CC_EXPR_GT: case CC_EXPR_LE: case CC_EXPR_GE:
        {
            CcExpr* node = NULL;
            CcExpr *L, *R;
            int err = cc_eval_expr(ctx, e->lhs, &L);
            if(err) return err;
            err = cc_eval_expr(ctx, e->values[0], &R);
            if(err) { cc_release_expr(p, L); return err; }
            // Type equality/inequality
            if((e->kind == CC_EXPR_EQ || e->kind == CC_EXPR_NE)
            && (ccqt_kind(L->type) == CC_POINTER || ccqt_bt_eq(L->type, CCBT_nullptr_t))
            && (ccqt_kind(R->type) == CC_POINTER || ccqt_bt_eq(R->type, CCBT_nullptr_t))
            && !L->uinteger && !R->uinteger){
                node = cc_int64_expr(p, e->loc, e->type, e->kind == CC_EXPR_EQ);
                if(!node) err = CC_OOM_ERROR;
                goto fini_binary;
            }
            if(ccqt_bt_eq(L->type, CCBT__Type) && ccqt_bt_eq(R->type, CCBT__Type)){
                if(e->kind == CC_EXPR_EQ){
                    node = cc_int64_expr(p, e->loc, e->type, L->type_value.bits == R->type_value.bits);
                    if(!node) err = CC_OOM_ERROR;
                    goto fini_binary;
                }
                if(e->kind == CC_EXPR_NE){
                    node = cc_int64_expr(p, e->loc, e->type, L->type_value.bits != R->type_value.bits);
                    if(!node) err = CC_OOM_ERROR;
                    goto fini_binary;
                }
                err = CC_UNREACHABLE_ERROR;
                goto fini_binary;
            }
            // L and R have the same type after usual arithmetic conversions.
            // For arithmetic ops, result type == operand type.
            // For comparisons, result type is int, operand type is L->type.
            CcQualType optype = L->type;
            while(ccqt_kind(optype) == CC_ENUM)
                optype = ccqt_as_enum(optype)->underlying;
            if(!ccqt_is_basic(optype)){
                err = CC_UNREACHABLE_ERROR;
                goto fini_binary;
            }
            if(e->kind == CC_EXPR_LSHIFT || e->kind == CC_EXPR_RSHIFT){
                err = cc_eval_check_shift_count(p, optype, R);
                if(err) goto fini_binary;
            }
            node = cc_value_expr(p, e->loc, e->type);
            if(!node) {err = CC_OOM_ERROR; goto fini_binary;}
            if(cc_eval_wide(optype)){
                err = cc_eval_wide_binary(p, e->kind, L, R, node);
                goto fini_binary;
            }
            #define ARITH(op, lv, rv, field, type) node->field = (type)((type)(lv) op (type)(rv)); break
            #define SIGNED_ARITH(op, chk, lv, rv, field, type) do { \
                type _r; if(chk((type)(lv), (type)(rv), &_r)){err = CC_OVERFLOW_ERROR; goto fini_binary;} \
                node->field = _r; \
            } while(0); break
            #define CMP(op, lv, rv) node->integer = (lv) op (rv); break
            switch(optype.basic.kind){
                DRP_CASES_EXHAUSTED;
                case CCBT_float:
                    switch((uint32_t)e->kind){
                        case CC_EXPR_ADD: ARITH(+, L->float_, R->float_, float_, float);
                        case CC_EXPR_SUB: ARITH(-, L->float_, R->float_, float_, float);
                        case CC_EXPR_MUL: ARITH(*, L->float_, R->float_, float_, float);
                        case CC_EXPR_DIV: ARITH(/, L->float_, R->float_, float_, float);
                        case CC_EXPR_EQ: CMP(==, L->float_, R->float_);
                        case CC_EXPR_NE: CMP(!=, L->float_, R->float_);
                        case CC_EXPR_LT: CMP(<,  L->float_, R->float_);
                        case CC_EXPR_GT: CMP(>,  L->float_, R->float_);
                        case CC_EXPR_LE: CMP(<=, L->float_, R->float_);
                        case CC_EXPR_GE: CMP(>=, L->float_, R->float_);
                        DRP_DEFAULT_UNREACHABLE;
                    }
                    break;
                case CCBT_double:
                    switch((uint32_t)e->kind){
                        case CC_EXPR_ADD: ARITH(+, L->double_, R->double_, double_, double);
                        case CC_EXPR_SUB: ARITH(-, L->double_, R->double_, double_, double);
                        case CC_EXPR_MUL: ARITH(*, L->double_, R->double_, double_, double);
                        case CC_EXPR_DIV: ARITH(/, L->double_, R->double_, double_, double);
                        case CC_EXPR_EQ: CMP(==, L->double_, R->double_);
                        case CC_EXPR_NE: CMP(!=, L->double_, R->double_);
                        case CC_EXPR_LT: CMP(<,  L->double_, R->double_);
                        case CC_EXPR_GT: CMP(>,  L->double_, R->double_);
                        case CC_EXPR_LE: CMP(<=, L->double_, R->double_);
                        case CC_EXPR_GE: CMP(>=, L->double_, R->double_);
                        DRP_DEFAULT_UNREACHABLE;
                    }
                    break;
                case CCBT_char:
                    if(!cc_target(p)->char_is_signed)
                        goto unsigned_;
                    goto int_;
                case CCBT_bool:
                case CCBT_signed_char:
                case CCBT_short:
                case CCBT_int:
                    int_:;{
                    int32_t lv = (int32_t)L->integer, rv = (int32_t)R->integer;
                    switch((uint32_t)e->kind){
                        case CC_EXPR_ADD: SIGNED_ARITH(+, add_overflow, lv, rv, integer, int32_t);
                        case CC_EXPR_SUB: SIGNED_ARITH(-, sub_overflow, lv, rv, integer, int32_t);
                        case CC_EXPR_MUL: SIGNED_ARITH(*, mul_overflow, lv, rv, integer, int32_t);
                        case CC_EXPR_DIV:
                            if(rv == 0 || (lv == INT32_MIN && rv == -1)){err = CC_OVERFLOW_ERROR; goto fini_binary;}
                            ARITH(/, lv, rv, integer, int32_t);
                        case CC_EXPR_MOD:
                            if(rv == 0 || (lv == INT32_MIN && rv == -1)){err = CC_OVERFLOW_ERROR; goto fini_binary;}
                            ARITH(%, lv, rv, integer, int32_t);
                        case CC_EXPR_BITAND: ARITH(&,  lv, rv, integer, int32_t);
                        case CC_EXPR_BITOR:  ARITH(|,  lv, rv, integer, int32_t);
                        case CC_EXPR_BITXOR: ARITH(^,  lv, rv, integer, int32_t);
                        case CC_EXPR_LSHIFT:
                            if(rv < 0 || rv >= 32){err = CC_OVERFLOW_ERROR; goto fini_binary;}
                            node->integer = (int32_t)((uint32_t)lv << rv); break;
                        case CC_EXPR_RSHIFT:
                            if(rv < 0 || rv >= 32){err = CC_OVERFLOW_ERROR; goto fini_binary;}
                            ARITH(>>, lv, rv, integer, int32_t);
                        case CC_EXPR_EQ: CMP(==, lv, rv);
                        case CC_EXPR_NE: CMP(!=, lv, rv);
                        case CC_EXPR_LT: CMP(<,  lv, rv);
                        case CC_EXPR_GT: CMP(>,  lv, rv);
                        case CC_EXPR_LE: CMP(<=, lv, rv);
                        case CC_EXPR_GE: CMP(>=, lv, rv);
                        DRP_DEFAULT_UNREACHABLE;
                    }
                    break;}
                case CCBT_unsigned_char:
                case CCBT_unsigned_short:
                case CCBT_unsigned:
                    unsigned_:;{
                    uint32_t lv = (uint32_t)L->uinteger, rv = (uint32_t)R->uinteger;
                    switch((uint32_t)e->kind){
                        case CC_EXPR_ADD: ARITH(+, lv, rv, uinteger, uint32_t);
                        case CC_EXPR_SUB: ARITH(-, lv, rv, uinteger, uint32_t);
                        case CC_EXPR_MUL: ARITH(*, lv, rv, uinteger, uint32_t);
                        case CC_EXPR_DIV:
                            if(rv == 0){err = CC_OVERFLOW_ERROR; goto fini_binary;}
                            ARITH(/, lv, rv, uinteger, uint32_t);
                        case CC_EXPR_MOD:
                            if(rv == 0){err = CC_OVERFLOW_ERROR; goto fini_binary;}
                            ARITH(%, lv, rv, uinteger, uint32_t);
                        case CC_EXPR_BITAND: ARITH(&,  lv, rv, uinteger, uint32_t);
                        case CC_EXPR_BITOR:  ARITH(|,  lv, rv, uinteger, uint32_t);
                        case CC_EXPR_BITXOR: ARITH(^,  lv, rv, uinteger, uint32_t);
                        case CC_EXPR_LSHIFT:
                            if(rv >= 32){err = CC_OVERFLOW_ERROR; goto fini_binary;}
                            ARITH(<<, lv, rv, uinteger, uint32_t);
                        case CC_EXPR_RSHIFT:
                            if(rv >= 32){err = CC_OVERFLOW_ERROR; goto fini_binary;}
                            ARITH(>>, lv, rv, uinteger, uint32_t);
                        case CC_EXPR_EQ: CMP(==, lv, rv);
                        case CC_EXPR_NE: CMP(!=, lv, rv);
                        case CC_EXPR_LT: CMP(<,  lv, rv);
                        case CC_EXPR_GT: CMP(>,  lv, rv);
                        case CC_EXPR_LE: CMP(<=, lv, rv);
                        case CC_EXPR_GE: CMP(>=, lv, rv);
                        DRP_DEFAULT_UNREACHABLE;
                    }
                    break;}
                case CCBT_long:
                    if(cc_target(p)->sizeof_[CCBT_long] == 8)
                        goto long_long;
                    goto int_;
                case CCBT_unsigned_long:
                    if(cc_target(p)->sizeof_[CCBT_unsigned_long] == 8)
                        goto unsigned_long_long;
                    goto unsigned_;
                case CCBT_long_long:
                    long_long:;{
                    int64_t lv = L->integer, rv = R->integer;
                    switch((uint32_t)e->kind){
                        case CC_EXPR_ADD: SIGNED_ARITH(+, add_overflow, lv, rv, integer, int64_t);
                        case CC_EXPR_SUB: SIGNED_ARITH(-, sub_overflow, lv, rv, integer, int64_t);
                        case CC_EXPR_MUL: SIGNED_ARITH(*, mul_overflow, lv, rv, integer, int64_t);
                        case CC_EXPR_DIV:
                            if(rv == 0 || (lv == INT64_MIN && rv == -1)){err = CC_OVERFLOW_ERROR; goto fini_binary;}
                            ARITH(/, lv, rv, integer, int64_t);
                        case CC_EXPR_MOD:
                            if(rv == 0 || (lv == INT64_MIN && rv == -1)){err = CC_OVERFLOW_ERROR; goto fini_binary;}
                            ARITH(%, lv, rv, integer, int64_t);
                        case CC_EXPR_BITAND: ARITH(&,  lv, rv, integer, int64_t);
                        case CC_EXPR_BITOR:  ARITH(|,  lv, rv, integer, int64_t);
                        case CC_EXPR_BITXOR: ARITH(^,  lv, rv, integer, int64_t);
                        case CC_EXPR_LSHIFT:
                            if(rv < 0 || rv >= 64){err = CC_OVERFLOW_ERROR; goto fini_binary;}
                            node->integer = (int64_t)((uint64_t)lv << rv); break;
                        case CC_EXPR_RSHIFT:
                            if(rv < 0 || rv >= 64){err = CC_OVERFLOW_ERROR; goto fini_binary;}
                            ARITH(>>, lv, rv, integer, int64_t);
                        case CC_EXPR_EQ: CMP(==, lv, rv);
                        case CC_EXPR_NE: CMP(!=, lv, rv);
                        case CC_EXPR_LT: CMP(<,  lv, rv);
                        case CC_EXPR_GT: CMP(>,  lv, rv);
                        case CC_EXPR_LE: CMP(<=, lv, rv);
                        case CC_EXPR_GE: CMP(>=, lv, rv);
                        DRP_DEFAULT_UNREACHABLE;
                    }
                    break;}
                case CCBT_unsigned_long_long:
                    unsigned_long_long:;{
                    uint64_t lv = L->uinteger, rv = R->uinteger;
                    switch((uint32_t)e->kind){
                        case CC_EXPR_ADD: ARITH(+, lv, rv, uinteger, uint64_t);
                        case CC_EXPR_SUB: ARITH(-, lv, rv, uinteger, uint64_t);
                        case CC_EXPR_MUL: ARITH(*, lv, rv, uinteger, uint64_t);
                        case CC_EXPR_DIV:
                            if(rv == 0){err = CC_OVERFLOW_ERROR; goto fini_binary;}
                            ARITH(/, lv, rv, uinteger, uint64_t);
                        case CC_EXPR_MOD:
                            if(rv == 0){err = CC_OVERFLOW_ERROR; goto fini_binary;}
                            ARITH(%, lv, rv, uinteger, uint64_t);
                        case CC_EXPR_BITAND: ARITH(&,  lv, rv, uinteger, uint64_t);
                        case CC_EXPR_BITOR:  ARITH(|,  lv, rv, uinteger, uint64_t);
                        case CC_EXPR_BITXOR: ARITH(^,  lv, rv, uinteger, uint64_t);
                        case CC_EXPR_LSHIFT:
                            if(rv >= 64){err = CC_OVERFLOW_ERROR; goto fini_binary;}
                            ARITH(<<, lv, rv, uinteger, uint64_t);
                        case CC_EXPR_RSHIFT:
                            if(rv >= 64){err = CC_OVERFLOW_ERROR; goto fini_binary;}
                            ARITH(>>, lv, rv, uinteger, uint64_t);
                        case CC_EXPR_EQ: CMP(==, lv, rv);
                        case CC_EXPR_NE: CMP(!=, lv, rv);
                        case CC_EXPR_LT: CMP(<,  lv, rv);
                        case CC_EXPR_GT: CMP(>,  lv, rv);
                        case CC_EXPR_LE: CMP(<=, lv, rv);
                        case CC_EXPR_GE: CMP(>=, lv, rv);
                        DRP_DEFAULT_UNREACHABLE;
                    }
                    break;}
                case CCBT_INVALID:
                case CCBT_void:
                case CCBT_nullptr_t:
                case CCBT__Any:
                case CCBT__Type:
                case CCBT_COUNT:
                    err = CC_UNREACHABLE_ERROR;
                    goto fini_binary;
                case CCBT_int128:
                case CCBT_unsigned_int128:
                case CCBT_float16:
                case CCBT_long_double:
                case CCBT_float128:
                case CCBT_float_complex:
                case CCBT_double_complex:
                case CCBT_long_double_complex:
                    err = CC_UNIMPLEMENTED_ERROR;
                    goto fini_binary;
            }
            #undef ARITH
            #undef SIGNED_ARITH
            #undef CMP
            fini_binary:;
            if(err){
                if(node) cc_release_expr(p, node);
            }
            else
                *result = node;
            cc_release_expr(p, R);
            cc_release_expr(p, L);
            return err;
        }
        case CC_EXPR_CAST: {
            CcQualType target = e->type;
            while(ccqt_kind(target) == CC_ENUM) target = ccqt_as_enum(target)->underlying;
            if(ccqt_bt_eq(target, CCBT_bool)){
                _Bool truth;
                int err = cc_eval_truthy(ctx, e->lhs, &truth);
                if(err) return err;
                *result = cc_int64_expr(p, e->loc, e->type, truth);
                return *result ? 0 : CC_OOM_ERROR;
            }
            CcExpr* operand, *node = NULL;
            int err = cc_eval_expr(ctx, e->lhs, &operand);
            if(err) return err;
            if(operand->type.unqual == e->type.unqual){
                operand->type = e->type;
                *result = operand;
                return 0;
            }
            if(!ccqt_is_basic(target)){
                if(cc_eval_address_type(e->type) && operand->kind == CC_EXPR_VALUE
                    && (ccqt_is_integer(operand->type) || cc_eval_address_type(operand->type))){
                    node = cc_value_expr(p, e->loc, e->type);
                    if(!node){ cc_release_expr(p, operand); return CC_OOM_ERROR; }
                    node->uinteger = ccqt_is_integer(operand->type)
                        ? ci_uint128_lo(cc_eval_u128(p, operand)) : operand->uinteger;
                    uint32_t size = cc_target(p)->sizeof_[CCBT_nullptr_t];
                    if(size < 8) node->uinteger &= UINT64_MAX >> (64 - size * 8);
                    cc_release_expr(p, operand);
                    *result = node;
                    return 0;
                }
                *result = operand;
                return 0;
            }
            CcBasicTypeKind tk = target.basic.kind;
            if(tk == CCBT__Any){
                CcInitList* il = Allocator_zalloc(cc_allocator(p), sizeof(CcInitList) + 2 * sizeof(CcInitEntry));
                if(!il){ cc_release_expr(p, operand); return CC_OOM_ERROR; }
                CcExpr* tag = cc_value_expr(p, e->loc, ccqt_basic(CCBT__Type));
                node = cc_make_expr(p, CC_EXPR_INIT_LIST, e->loc, e->type, 0);
                if(!tag || !node){
                    if(tag) cc_release_expr(p, tag);
                    if(node) _cc_release_expr(p, node, 0);
                    Allocator_free(cc_allocator(p), il, sizeof(CcInitList) + 2 * sizeof(CcInitEntry));
                    cc_release_expr(p, operand);
                    return CC_OOM_ERROR;
                }
                tag->type_value = (CcQualType){.unqual = e->lhs->type.unqual};
                il->loc = e->loc;
                il->count = 2;
                il->entries[0].value = tag;
                il->entries[0].path = (CcFieldPath){.n_components=1, .idx0=0};
                il->entries[1].value = operand;
                il->entries[1].path = (CcFieldPath){.n_components=1, .idx0=1};
                node->init_list = il;
                *result = node;
                return 0;
            }
            if(tk == CCBT_void){
                node = cc_value_expr(p, e->loc, ccqt_basic(CCBT_void));
                if(!node) err = CC_OOM_ERROR;
                goto fini_cast;
            }
            node = cc_value_expr(p, e->loc, e->type);
            if(!node) {err = CC_OOM_ERROR; goto fini_cast;}
            if(cc_eval_wide(operand->type) || cc_eval_wide(e->type)){
                err = cc_eval_wide_cast(p, operand, node);
                if(!err) cc_eval_truncate(p, node);
                goto fini_cast;
            }
            if(ccbt_is_float(tk)){
                if(tk == CCBT_float){
                    err = cc_eval_to_f(p, operand, &node->float_);
                    if(err) goto fini_cast;
                }
                else {
                    err = cc_eval_to_d(p, operand, &node->double_);
                    if(err) goto fini_cast;
                }
            }
            else if(ccbt_is_integer(tk)){
                if(ccbt_is_unsigned(tk, !cc_target(p)->char_is_signed)){
                    err = cc_eval_to_u(p, operand, &node->uinteger);
                    if(err) goto fini_cast;
                }
                else {
                    err = cc_eval_to_i(p, operand, &node->integer);
                    if(err) goto fini_cast;
                }
                cc_eval_truncate(p, node);
            }
            fini_cast:;
            cc_release_expr(p, operand);
            if(!err)
                *result = node;
            else {
                if(node) cc_release_expr(p, node);
            }
            return err;
        }
        case CC_EXPR_TYPE_INTROSPECTION: {
            CcExpr* lhs;
            int err = cc_eval_expr(ctx, e->lhs, &lhs);
            if(err) return err;
            if(!ccqt_bt_eq(lhs->type, CCBT__Type)){
                err = CC_NOT_CONSTANT_ERROR;
                goto fini_introspection;
            }
            CcQualType qt = lhs->type_value;
            CcTypeIntrospectionOp op = e->type_introspection.op;
            #define INTRES(val) do { \
                *result = cc_int64_expr(p, e->loc, e->type, (val)); \
                err = *result ? 0 : CC_OOM_ERROR; \
                goto fini_introspection; \
            } while(0)
            #define UINTRES(val) do { \
                *result = cc_uint64_expr(p, e->loc, e->type, (val)); \
                err = *result ? 0 : CC_OOM_ERROR; \
                goto fini_introspection; \
            } while(0)
            #define TYPERES(t) do { \
                CcExpr* _n = cc_value_expr(p, e->loc, ccqt_basic(CCBT__Type)); \
                if(!_n) { err = CC_OOM_ERROR; goto fini_introspection; } \
                _n->uinteger = (t).bits; \
                *result = _n; err = 0; \
                goto fini_introspection; \
            } while(0)
            switch(op){
                case CC_TYPE_IS_VALID:      INTRES(qt.bits != 0);
                case CC_TYPE_IS_INVALID:    INTRES(qt.bits == 0);
                case CC_TYPE_IS_INTEGER:    INTRES(ccqt_is_basic(qt) && ccbt_is_integer(qt.basic.kind));
                case CC_TYPE_IS_FLOAT:      INTRES(ccqt_is_basic(qt) && ccbt_is_float(qt.basic.kind));
                case CC_TYPE_IS_ARITHMETIC: INTRES((ccqt_is_basic(qt) && ccbt_is_arithmetic(qt.basic.kind)) || ccqt_kind(qt) == CC_ENUM);
                case CC_TYPE_IS_POINTER:    INTRES(ccqt_kind(qt) == CC_POINTER);
                case CC_TYPE_IS_STRUCT:     INTRES(ccqt_kind(qt) == CC_STRUCT);
                case CC_TYPE_IS_UNION:      INTRES(ccqt_kind(qt) == CC_UNION);
                case CC_TYPE_IS_ARRAY:      INTRES(ccqt_kind(qt) == CC_ARRAY && !ccqt_as_array(qt)->is_vector);
                case CC_TYPE_IS_VECTOR:     INTRES(ccqt_kind(qt) == CC_ARRAY && ccqt_as_array(qt)->is_vector);
                case CC_TYPE_IS_SLICE:      INTRES(ccqt_kind(qt) == CC_SLICE);
                case CC_TYPE_IS_FUNCTION:   INTRES(ccqt_kind(qt) == CC_FUNCTION);
                case CC_TYPE_IS_ENUM:       INTRES(ccqt_kind(qt) == CC_ENUM);
                case CC_TYPE_IS_CONST:      INTRES(qt.is_const);
                case CC_TYPE_IS_VOLATILE:   INTRES(qt.is_volatile);
                case CC_TYPE_IS_ATOMIC:     INTRES(qt.is_atomic);
                case CC_TYPE_IS_UNSIGNED:   INTRES(ccqt_is_basic(qt) && ccbt_is_unsigned(qt.basic.kind, !cc_target(p)->char_is_signed));
                case CC_TYPE_IS_SIGNED:     INTRES(ccqt_is_basic(qt) && ccbt_is_integer(qt.basic.kind) && !ccbt_is_unsigned(qt.basic.kind, !cc_target(p)->char_is_signed));
                case CC_TYPE_IS_INCOMPLETE: {
                    CcTypeKind k = ccqt_kind(qt);
                    _Bool is_incomplete;
                    switch(k){
                        DRP_CASES_EXHAUSTED;
                        case CC_STRUCT:   is_incomplete = ccqt_as_struct(qt)->is_incomplete; break;
                        case CC_UNION:    is_incomplete = ccqt_as_union(qt)->is_incomplete; break;
                        case CC_ARRAY:    is_incomplete = ccqt_as_array(qt)->is_incomplete; break;
                        case CC_ENUM:     is_incomplete = ccqt_as_enum(qt)->is_incomplete; break;
                        case CC_FUNCTION: case CC_BASIC: case CC_POINTER: case CC_BLOCK_POINTER: case CC_SLICE: is_incomplete = 0; break;
                    }
                    INTRES(is_incomplete);
                }
                case CC_TYPE_IS_CALLABLE: {
                    CcTypeKind k = ccqt_kind(qt);
                    INTRES(k == CC_FUNCTION || (k == CC_POINTER && ccqt_kind(ccqt_as_ptr(qt)->pointee) == CC_FUNCTION));
                }
                case CC_TYPE_IS_VARIADIC: {
                    CcQualType ft = qt;
                    if(ccqt_kind(ft) == CC_POINTER) ft = ccqt_as_ptr(ft)->pointee;
                    INTRES(ccqt_kind(ft) == CC_FUNCTION && ccqt_as_function(ft)->is_variadic);
                }
                case CC_TYPE_SIZEOF: {
                    uint32_t sz;
                    err = cc_sizeof_as_uint(p, qt, e->loc, &sz);
                    if(err) goto fini_introspection;
                    UINTRES(sz);
                }
                case CC_TYPE_ALIGNOF: {
                    uint32_t al;
                    err = cc_alignof_as_uint(p, qt, e->loc, &al);
                    if(err) goto fini_introspection;
                    UINTRES(al);
                }
                case CC_TYPE_POINTEE:
                    if(ccqt_kind(qt) != CC_POINTER) { err = CC_NOT_CONSTANT_ERROR; goto fini_introspection; }
                    TYPERES(ccqt_as_ptr(qt)->pointee);
                case CC_TYPE_UNQUAL: {
                    CcQualType uq = qt;
                    uq.quals = 0;
                    TYPERES(uq);
                }
                case CC_TYPE_COUNT:
                    if(ccqt_kind(qt) != CC_ARRAY) { err = CC_NOT_CONSTANT_ERROR; goto fini_introspection; }
                    UINTRES(ccqt_as_array(qt)->length);
                case CC_TYPE_LOC:{
                    switch(ccqt_kind(qt)){
                        DRP_CASES_EXHAUSTED;
                        case CC_ENUM: UINTRES(ccqt_as_enum(qt)->loc.bits);
                        case CC_UNION: UINTRES(ccqt_as_union(qt)->loc.bits);
                        case CC_STRUCT: UINTRES(ccqt_as_struct(qt)->loc.bits);
                        case CC_BASIC:
                        case CC_BLOCK_POINTER:
                        case CC_POINTER:
                        case CC_ARRAY:
                        case CC_SLICE:
                        case CC_FUNCTION:
                            err = CC_NOT_CONSTANT_ERROR;
                            goto fini_introspection;
                    }
                }
                case CC_TYPE_IS_CALLABLE_WITH: {
                    CcExpr* arg;
                    err = cc_eval_expr(ctx, e->values[0], &arg);
                    if(err) goto fini_introspection;
                    if(!ccqt_bt_eq(arg->type, CCBT__Type)) {
                        cc_release_expr(p, arg);
                        err = CC_NOT_CONSTANT_ERROR;
                        goto fini_introspection;
                    }
                    CcQualType arg_type = arg->type_value;
                    CcQualType ft = qt;
                    if(ccqt_kind(ft) == CC_POINTER) ft = ccqt_as_ptr(ft)->pointee;
                    _Bool v = 0;
                    if(ccqt_kind(ft) == CC_FUNCTION){
                        CcFunction* f = ccqt_as_function(ft);
                        if(f->param_count == 1)
                            v = cc_implicit_convertible(p, arg_type, f->params[0]);
                    }
                    cc_release_expr(p, arg);
                    INTRES(v);
                }
                case CC_TYPE_IS_CALLABLE_THROUGH:
                case CC_TYPE_CASTABLE_TO: {
                    CcExpr* arg;
                    err = cc_eval_expr(ctx, e->values[0], &arg);
                    if(err) goto fini_introspection;
                    if(!ccqt_bt_eq(arg->type, CCBT__Type)) {
                        cc_release_expr(p, arg);
                        err = CC_NOT_CONSTANT_ERROR;
                        goto fini_introspection;
                    }
                    _Bool castable = e->type_introspection.op == CC_TYPE_IS_CALLABLE_THROUGH ? cc_is_callable_through(p, qt, arg->type_value) : cc_explicit_castable(p, qt, arg->type_value);
                    cc_release_expr(p, arg);
                    INTRES(castable);
                }
                case CC_TYPE_MAKE_ANY:{
                    return CC_NOT_CONSTANT_ERROR; // TODO: we could do this
                }
                case CC_TYPE_FIELDS:
                case CC_TYPE_METHODS: {
                    CcTypeKind k = ccqt_kind(qt);
                    if(k != CC_STRUCT && k != CC_UNION) { err = CC_NOT_CONSTANT_ERROR; goto fini_introspection; }
                    CcStruct* s = ccqt_as_struct(qt);
                    uint32_t count = 0;
                    _Bool methods = e->type_introspection.op == CC_TYPE_METHODS;
                    for(uint32_t i = 0; i < s->field_count; i++)
                        count += s->fields[i].is_method == methods;
                    UINTRES(count);
                }
                case CC_TYPE_RETURN_TYPE: {
                    CcQualType ft = qt;
                    if(ccqt_kind(ft) == CC_POINTER) ft = ccqt_as_ptr(ft)->pointee;
                    if(ccqt_kind(ft) != CC_FUNCTION) { err = CC_NOT_CONSTANT_ERROR; goto fini_introspection; }
                    TYPERES(ccqt_as_function(ft)->return_type);
                }
                case CC_TYPE_PARAM_COUNT: {
                    CcQualType ft = qt;
                    if(ccqt_kind(ft) == CC_POINTER) ft = ccqt_as_ptr(ft)->pointee;
                    if(ccqt_kind(ft) != CC_FUNCTION) { err = CC_NOT_CONSTANT_ERROR; goto fini_introspection; }
                    UINTRES(ccqt_as_function(ft)->param_count);
                }
                case CC_TYPE_PARAM_TYPE: {
                    CcQualType ft = qt;
                    if(ccqt_kind(ft) == CC_POINTER) ft = ccqt_as_ptr(ft)->pointee;
                    if(ccqt_kind(ft) != CC_FUNCTION) { err = CC_NOT_CONSTANT_ERROR; goto fini_introspection; }
                    CcFunction* f = ccqt_as_function(ft);
                    int64_t i;
                    err = cc_eval_integer(ctx, e->values[0], &i);
                    if(err) goto fini_introspection;
                    if(i < 0 || (uint64_t)i >= f->param_count) { err = CC_NOT_CONSTANT_ERROR; goto fini_introspection; }
                    TYPERES(f->params[i]);
                }
                case CC_TYPE_ELEMENT_TYPE:{
                    CcTypeKind k = ccqt_kind(qt);
                    CcQualType val;
                    if(k == CC_ARRAY)
                        val = ccqt_as_array(qt)->element;
                    else if(k == CC_SLICE)
                        val = ccqt_as_slice(qt)->pointee;
                    else {
                        err = CC_NOT_CONSTANT_ERROR;
                        goto fini_introspection;
                    }
                    TYPERES(val);
                }
                case CC_TYPE_UNDERLYING_TYPE:
                    if(ccqt_kind(qt) != CC_ENUM) { err = CC_NOT_CONSTANT_ERROR; goto fini_introspection; }
                    TYPERES(ccqt_as_enum(qt)->underlying);
                case CC_TYPE_ENUMERATORS: {
                    if(ccqt_kind(qt) != CC_ENUM) { err = CC_NOT_CONSTANT_ERROR; goto fini_introspection; }
                    CcEnum* e2 = ccqt_as_enum(qt);
                    UINTRES(e2->enumerator_count);
                }
                case CC_TYPE_FIELD:
                case CC_TYPE_METHOD:
                case CC_TYPE_HAS_FIELD:
                case CC_TYPE_HAS_METHOD: {
                    _Bool has = e->type_introspection.op == CC_TYPE_HAS_FIELD || e->type_introspection.op == CC_TYPE_HAS_METHOD;
                    CcTypeKind k = ccqt_kind(qt);
                    CcStruct* s = k == CC_STRUCT || k == CC_UNION ? ccqt_as_struct(qt) : NULL;
                    if(!s && !has) { err = CC_NOT_CONSTANT_ERROR; goto fini_introspection; }
                    _Bool method = e->type_introspection.op == CC_TYPE_METHOD || e->type_introspection.op == CC_TYPE_HAS_METHOD;
                    CcField* f = NULL;
                    CcField named_field;
                    if(ccqt_kind(e->values[0]->type) == CC_SLICE){
                        uint64_t count;
                        CcEvalAddress data;
                        err = cc_eval_slice(ctx, e->values[0], 0, &count, &data);
                        if(err) goto fini_introspection;
                        if(count && (data.kind != CC_EVAL_LITERAL || data.offset < 0
                            || (uint64_t)data.offset > data.literal_size
                            || count > data.literal_size - (uint64_t)data.offset))
                            err = CC_NOT_CONSTANT_ERROR;
                        if(!err && count > 0 && s){
                            Atom atom = AT_atomize(p->cpp.at, (const char*)data.symbol + data.offset, count);
                            if(!atom) err = CC_OOM_ERROR;
                            else {
                                uint64_t floc;
                                CcQualType type;
                                err = cc_lookup_field_offset(p, qt, atom, &floc, &type, &f);
                                if(err) goto fini_introspection;
                                if(f && f->is_method != method) f = NULL;
                                if(f){
                                    named_field = *f;
                                    named_field.offset = (uint32_t)floc;
                                    f = &named_field;
                                }
                            }
                        }
                        if(err) goto fini_introspection;
                        if(has) INTRES(f != NULL);
                        if(!f) { err = CC_NOT_CONSTANT_ERROR; goto fini_introspection; }
                    }
                    else {
                        int64_t idx;
                        err = cc_eval_integer(ctx, e->values[0], &idx);
                        if(err) goto fini_introspection;
                        if(idx >= 0){
                            for(uint32_t i = 0; i < s->field_count; i++){
                                if(s->fields[i].is_method != method) continue;
                                if(idx-- == 0){ f = &s->fields[i]; break; }
                            }
                        }
                        if(!f) { err = CC_NOT_CONSTANT_ERROR; goto fini_introspection; }
                    }
                    uint32_t nfields = method ? 4 : 6;
                    CcInitList* il = Allocator_zalloc(cc_allocator(p), sizeof(CcInitList) + nfields * sizeof(CcInitEntry));
                    if(!il) { err = CC_OOM_ERROR; goto fini_introspection; }
                    il->loc = e->loc;
                    il->count = nfields;
                    for(uint32_t i = 0; i < nfields; i++)
                        il->entries[i].path = (CcFieldPath){.n_components=1, .idx0=i};
                    CcInitEntry* entries = il->entries;
                    // _Type type;
                    CcExpr* type_val = cc_value_expr(p, e->loc, ccqt_basic(CCBT__Type));
                    if(!type_val) { err = CC_OOM_ERROR; goto fini_introspection; }
                    type_val->uinteger = f->type.bits;
                    entries->value = type_val;
                    entries++;
                    // const char name[:];
                    Atom name = f->is_method ? f->method->name : f->name;
                    CcExpr* name_val = cc_constexpr_string_slice_expr(p, e->loc, name ? name : nil_atom, 0);
                    if(!name_val) { err = CC_OOM_ERROR; goto fini_introspection; }
                    entries->value = name_val;
                    entries++;
                    if(method){
                        CcExpr* offset = cc_uint64_expr(p, e->loc, ccqt_basic(cc_target(p)->size_type), f->offset);
                        if(!offset) { err = CC_OOM_ERROR; goto fini_introspection; }
                        entries->value = offset;
                        entries++;
                        CcExpr* func = cc_make_expr(p, CC_EXPR_FUNCTION, e->loc, f->type, 0);
                        if(!func) { err = CC_OOM_ERROR; goto fini_introspection; }
                        func->func = f->method;
                        CcExpr* address = cc_unary_expr(p, CC_EXPR_CAST, e->loc, ccqt_basic(cc_target(p)->size_type), func);
                        if(!address){ cc_release_expr(p, func); err = CC_OOM_ERROR; goto fini_introspection; }
                        entries->value = address;
                    }
                    else {
                        // unsigned offset;
                        CcExpr* off_val = cc_uint64_expr(p, e->loc, ccqt_basic(CCBT_unsigned), f->offset);
                        if(!off_val) { err = CC_OOM_ERROR; goto fini_introspection; }
                        entries->value = off_val;
                        entries++;
                        // unsigned bitwidth;
                        CcExpr* bw_val = cc_uint64_expr(p, e->loc, ccqt_basic(CCBT_unsigned), f->bitwidth);
                        if(!bw_val) { err = CC_OOM_ERROR; goto fini_introspection; }
                        entries->value = bw_val;
                        entries++;
                        // unsigned bitoffset;
                        CcExpr* bo_val = cc_uint64_expr(p, e->loc, ccqt_basic(CCBT_unsigned), f->bitoffset);
                        if(!bo_val) { err = CC_OOM_ERROR; goto fini_introspection; }
                        entries->value = bo_val;
                        entries++;
                        // Maybe this should be a _Bool?
                        // unsigned is_bitfield;
                        CcExpr* is_bf_val = cc_uint64_expr(p, e->loc, ccqt_basic(CCBT_unsigned), f->is_bitfield);
                        if(!is_bf_val) { err = CC_OOM_ERROR; goto fini_introspection; }
                        entries->value = is_bf_val;
                        entries++;
                    }
                    CcExpr* node = cc_make_expr(p, CC_EXPR_INIT_LIST, e->loc, method ? p->builtin_method : p->builtin_field, 0);
                    if(!node) { err = CC_OOM_ERROR; goto fini_introspection; }
                    node->init_list = il;
                    *result = node;
                    err = 0;
                    goto fini_introspection;
                }
                case CC_TYPE_ENUMERATOR:{
                    if(ccqt_kind(qt) != CC_ENUM) { err = CC_NOT_CONSTANT_ERROR; goto fini_introspection; }
                    CcEnum* enum_ = ccqt_as_enum(qt);
                    int64_t idx;
                    err = cc_eval_integer(ctx, e->values[0], &idx);
                    if(err) goto fini_introspection;
                    if(idx < 0 || (uint64_t)idx >= enum_->enumerator_count) { err = CC_NOT_CONSTANT_ERROR; goto fini_introspection; }
                    CcEnumerator* en = enum_->enumerators[idx];
                    uint32_t nfields = 3; // name, value bits, type
                    CcInitList* il = Allocator_zalloc(cc_allocator(p), sizeof(CcInitList) + nfields * sizeof(CcInitEntry));
                    if(!il) { err = CC_OOM_ERROR; goto fini_introspection; }
                    il->loc = e->loc;
                    il->count = nfields;
                    CcInitEntry* entries = il->entries;
                    // const char name[:];
                    CcExpr* name_val = cc_constexpr_string_slice_expr(p, e->loc, en->name ? en->name : nil_atom, 0);
                    if(!name_val) { err = CC_OOM_ERROR; goto fini_introspection; }
                    entries->path = (CcFieldPath){.n_components=1, .idx0=0};
                    entries->value = name_val;
                    entries++;
                    // unsigned __int128 value;
                    CcExpr* val_node = cc_integer_bits_expr(p, e->loc, ccqt_basic(CCBT_unsigned_int128), en->value);
                    if(!val_node) { err = CC_OOM_ERROR; goto fini_introspection; }
                    entries->path = (CcFieldPath){.n_components=1, .idx0=1};
                    entries->value = val_node;
                    entries++;
                    CcExpr* type_node = cc_value_expr(p, e->loc, ccqt_basic(CCBT__Type));
                    if(!type_node) { err = CC_OOM_ERROR; goto fini_introspection; }
                    type_node->type_value = en->type;
                    entries->path = (CcFieldPath){.n_components=1, .idx0=2};
                    entries->value = type_node;
                    CcExpr* node = cc_make_expr(p, CC_EXPR_INIT_LIST, e->loc, p->builtin_enumerator, 0);
                    if(!node) { err = CC_OOM_ERROR; goto fini_introspection; }
                    node->init_list = il;
                    *result = node;
                    err = 0;
                    goto fini_introspection;
                }
                case CC_TYPE_NAME: {
                    MStringBuilder sb = {.allocator = allocator_from_arena(&p->scratch_arena)};
                    cc_print_type(&sb, qt);
                    Atom a = msb_atomize(&sb, p->cpp.at);
                    if(!a) { err = CC_OOM_ERROR; goto fini_introspection; }
                    msb_destroy(&sb);
                    CcExpr *node = cc_constexpr_string_slice_expr(p, e->loc, a, 0);
                    if(!node) { err = CC_OOM_ERROR; goto fini_introspection; }
                    *result = node;
                    err = 0;
                    goto fini_introspection;
                }
                case CC_TYPE_TAG: {
                    Atom tag = NULL;
                    CcTypeKind k = ccqt_kind(qt);
                    if(k == CC_STRUCT)     tag = ccqt_as_struct(qt)->name;
                    else if(k == CC_UNION) tag = ccqt_as_union(qt)->name;
                    else if(k == CC_ENUM)  tag = ccqt_as_enum(qt)->name;
                    if(!tag) tag = nil_atom;
                    CcExpr* node = cc_constexpr_string_slice_expr(p, e->loc, tag, 0);
                    if(!node) { err = CC_OOM_ERROR; goto fini_introspection; }
                    *result = node;
                    err = 0;
                    goto fini_introspection;
                }
                case CC_TYPE_PUSH_METHOD: // handled at parse time
                case CC_TYPE_NONE:
                    err = CC_NOT_CONSTANT_ERROR;
                    goto fini_introspection;
            }
            err = CC_NOT_CONSTANT_ERROR;
            #undef INTRES
            #undef UINTRES
            #undef TYPERES
            fini_introspection:;
            cc_release_expr(p, lhs);
            return err;
        }
        case CC_EXPR_ATOMIC:
        case CC_EXPR_SIZEOF_VMT:
            return CC_NOT_CONSTANT_ERROR;
        case CC_EXPR_VARIABLE:
            if(e->var->initializer && (e->var->constexpr_
                || (ctx->allow_const && cc_linktime_const_variable(e)))){
                if(ctx->variable_depth >= 256) return CC_NOT_CONSTANT_ERROR;
                ctx->variable_depth++;
                int err = cc_eval_expr(ctx, e->var->initializer, result);
                ctx->variable_depth--;
                return err;
            }
            return CC_NOT_CONSTANT_ERROR;
        case CC_EXPR_COMPOUND_LITERAL:
        case CC_EXPR_INIT_LIST: {
            if((ccqt_is_basic(e->type) && !ccqt_bt_eq(e->type, CCBT__Any))
                || ccqt_kind(e->type) == CC_ENUM || cc_eval_address_type(e->type))
                return cc_eval_object_scalar(ctx, e, 0, e->type, e->loc, NULL, result);
            CcExpr* node = cc_make_expr(p, CC_EXPR_INIT_LIST, e->loc, e->type, 0);
            if(!node) return CC_OOM_ERROR;
            e->init_list->rc++;
            node->init_list = e->init_list;
            *result = node;
            return 0;
        }
        case CC_EXPR_OBJECT_VIEW:
            return cc_clone_initializer_expr(p, e, 0, result);
        case CC_EXPR_FUNCTION:
        case CC_EXPR_ADDR:
        case CC_EXPR_PREINC:
        case CC_EXPR_PREDEC:
        case CC_EXPR_POSTINC:
        case CC_EXPR_POSTDEC:
        case CC_EXPR_ASSIGN:
        case CC_EXPR_ADDASSIGN:
        case CC_EXPR_SUBASSIGN:
        case CC_EXPR_MULASSIGN:
        case CC_EXPR_DIVASSIGN:
        case CC_EXPR_MODASSIGN:
        case CC_EXPR_BITANDASSIGN:
        case CC_EXPR_BITORASSIGN:
        case CC_EXPR_BITXORASSIGN:
        case CC_EXPR_LSHIFTASSIGN:
        case CC_EXPR_RSHIFTASSIGN:
            return CC_NOT_CONSTANT_ERROR;
        case CC_EXPR_SLICE:
        case CC_EXPR_SLICE_HI:
        case CC_EXPR_SLICE_LO:
        case CC_EXPR_SLICE_ALL:
            return cc_eval_slice_value(ctx, e, result);
        case CC_EXPR_CALL:
            return CC_NOT_CONSTANT_ERROR;
        case CC_EXPR_DOT: case CC_EXPR_ARROW: case CC_EXPR_DEREF:
        eval_init_list_access: {
            _Bool handled;
            int err = cc_eval_object_view_select(ctx, e, 0, &handled, result);
            return err ? err : handled ? 0 : CC_NOT_CONSTANT_ERROR;
        }
        case CC_EXPR_STATEMENT_EXPRESSION:
        case CC_EXPR_VA:
        case CC_EXPR_BUILTIN:
        case CC_EXPR_ADD_OVERFLOW:
        case CC_EXPR_MUL_OVERFLOW:
        case CC_EXPR_SUB_OVERFLOW:
            return CC_NOT_CONSTANT_ERROR;
        case CC_EXPR_BIT_BUILTIN: {
            CcExpr* operand;
            int err = cc_eval_expr(ctx, e->lhs, &operand);
            if(err) return err;
            CiUint128 v = cc_eval_u128(p, operand), arg = ci_uint128_from_uint64(0), bits;
            cc_release_expr(p, operand);
            if(e->bit_builtin.nargs && ((e->bit_builtin.op != CC_BIT_CLZ && e->bit_builtin.op != CC_BIT_CTZ)
                || !ci_uint128_nonzero(v))){
                CcExpr* second;
                err = cc_eval_expr(ctx, e->values[0], &second);
                if(err) return err;
                arg = cc_eval_u128(p, second);
                _Bool negative = !ccqt_is_unsigned(second->type, !cc_target(p)->char_is_signed)
                    && (ci_uint128_hi(arg) >> 63);
                cc_release_expr(p, second);
                if(negative && (e->bit_builtin.op == CC_BIT_ROTATE_LEFT || e->bit_builtin.op == CC_BIT_ROTATE_RIGHT))
                    return CC_NOT_CONSTANT_ERROR;
            }
            uint32_t sz;
            err = cc_sizeof_as_uint(p, e->lhs->type, e->loc, &sz);
            if(err) return err;
            if(!cc_bit_builtin(e->bit_builtin.op, v, sz*8, arg, e->bit_builtin.nargs, &bits))
                return CC_NOT_CONSTANT_ERROR;
            *result = cc_integer_bits_expr(p, e->loc, e->type, bits);
            return *result ? 0 : CC_OOM_ERROR;
        }

        case CC_EXPR_BSWAP:{
            CcExpr* operand;
            int err = cc_eval_expr(ctx, e->lhs, &operand);
            if(err) return err;
            uint64_t v;
            err = cc_eval_to_u(p, operand, &v);
            if(err) goto fini_bswap;
            uint32_t sz;
            err = cc_sizeof_as_uint(p, operand->type, e->loc, &sz);
            if(err) goto fini_bswap;
            switch(sz){
                case 2: v = bswap16((uint16_t)v); break;
                case 4: v = bswap32((uint32_t)v); break;
                case 8: v = bswap64((uint64_t)v); break;
                default:
                    err = CC_UNREACHABLE_ERROR;
                    goto fini_bswap;
            }

            CcExpr* node = cc_value_expr(p, e->loc, operand->type);
            if(!node){
                err = CC_OOM_ERROR;
                goto fini_bswap;
            }
            node->uinteger = v;
            *result = node;
            fini_bswap:;
            cc_release_expr(p, operand);
            return err;
        }
        case CC_EXPR_SRCLOC_REFLECT:{
            CcExpr* operand;
            int err = cc_eval_expr(ctx, e->lhs, &operand);
            if(err) return err;
            SrcLoc loc = operand->loc_value;
            if(!loc.bits) return CC_NOT_CONSTANT_ERROR;
            size_t line = loc.line, col = loc.column, file_id = loc.file_id;
            if(loc.is_actually_a_pointer){
                SrcLocExp* exp = (SrcLocExp*)((uintptr_t)loc.pointer.bits << 1);
                while(exp->parent) exp = exp->parent;
                line = exp->line;
                col = exp->column;
                file_id = exp->file_id;
            }
            CcExpr* node;
            switch(e->srcloc.op){
                DRP_CASES_EXHAUSTED;
                case CC_SRCLOC_FILE:{
                    FileCache* fc = p->cpp.fc;
                    if(file_id >= fc->map.count)
                        return CC_NOT_CONSTANT_ERROR;
                    CStringView path = fc->map.data[file_id].path;
                    Atom a = AT_atomize(p->cpp.at, path.text, path.length);
                    if(!a) return CC_OOM_ERROR;
                    node = cc_constexpr_string_slice_expr(p, e->loc, a, 0);
                    if(!node) return CC_OOM_ERROR;
                    break;
                }
                case CC_SRCLOC_LINE:
                case CC_SRCLOC_COL:
                    node = cc_value_expr(p, e->loc, e->type);
                    if(!node) return CC_OOM_ERROR;
                    node->uinteger = e->srcloc.op == CC_SRCLOC_LINE?line:col;
                    break;
            }
            *result = node;
            return 0;
        }
        case CC_EXPR_ALLOCA:
        case CC_EXPR_INTERN:
        case CC_EXPR_HOTSWAP:
        case CC_EXPR_COMPILE:
        case CC_EXPR_MODULE_REFLECT:
        case CC_EXPR_UMUL128:
            return CC_NOT_CONSTANT_ERROR;
    }
}

static
int
cc_eval_integer(CcEvalCtx* ctx, CcExpr* e, int64_t* out){
    CcParser* p = ctx->parser;
    CcExpr* val;
    int err = cc_eval_expr(ctx, e, &val);
    // temporary hack, callers should report different error.
    if(err == CC_OVERFLOW_ERROR) err = CC_NOT_CONSTANT_ERROR;
    if(err) return err;
    if(!ccqt_is_integer(val->type)){
        err = CC_NOT_CONSTANT_ERROR;
        goto finish;
    }
    if(cc_eval_wide(val->type)){
        CiUint128 bits = cc_eval_u128(p, val);
        _Bool is_unsigned = ccqt_is_unsigned(val->type, !cc_target(p)->char_is_signed);
        // Preserve the existing uint64_t bit-pattern convention, but never
        // discard significant bits from a wider evaluated constant. Explicit
        // casts have already narrowed their operand in cc_eval_expr.
        _Bool positive = !ci_uint128_hi(bits) && (is_unsigned || ci_uint128_lo(bits) <= INT64_MAX);
        _Bool negative = !is_unsigned && ci_uint128_hi(bits) == UINT64_MAX && ci_uint128_lo(bits) > INT64_MAX;
        if(!positive && !negative){
            err = CC_NOT_CONSTANT_ERROR;
            goto finish;
        }
    }
    err = cc_eval_to_i(p, val, out);
    finish:
    cc_release_expr(p, val);
    return err;
}

static
int
cc_eval_truthy(CcEvalCtx* ctx, CcExpr* e, _Bool* out){
    CcParser* p = ctx->parser;
    CcExpr* val;
    int err = cc_eval_expr(ctx, e, &val);
    if(err == CC_OVERFLOW_ERROR) err = CC_NOT_CONSTANT_ERROR;
    if(err == CC_NOT_CONSTANT_ERROR && cc_eval_address_type(e->type)){
        CcEvalAddress address;
        err = cc_eval_address(ctx, e, 0, 0, &address);
        if(err) return err;
        if(address.kind != CC_EVAL_ABSOLUTE && !cc_eval_address_nonnull(address))
            return CC_NOT_CONSTANT_ERROR;
        *out = address.kind != CC_EVAL_ABSOLUTE || address.offset != 0;
        return 0;
    }
    if(err) return err;
    *out = cc_eval_value_truth(p, val);
    cc_release_expr(p, val);
    return 0;
}

#ifdef __clang__
#pragma clang assume_nonnull end
#endif
#include "cc_type_cache.c"
#include "cc_scope.c"
#include "cc_printer.c"
#endif
