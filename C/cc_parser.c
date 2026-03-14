#ifndef C_CC_PARSER_C
#define C_CC_PARSER_C
//
// Copyright © 2026-2026, David Priver <david@davidpriver.com>
//
#include <stdarg.h>
#include "../Drp/bit_util.h"
#include "../Drp/parray.h"
#include "../Drp/merge_sort.h"
#include "cc_expr.h"
#include "cc_parser.h"
#include "cpp_preprocessor.h"
#include "cc_errors.h"
#ifdef __clang__
#pragma clang assume_nonnull begin
#endif


static int cc_parse_expr(CcParser* p, CcValueClass, CcExpr* _Nullable* _Nonnull out);
static int cc_parse_assignment_expr(CcParser* p, CcValueClass, CcExpr* _Nullable* _Nonnull out);
static int cc_parse_ternary_expr(CcParser* p, CcValueClass, CcExpr* _Nullable* _Nonnull out);
static int cc_parse_infix(CcParser* p, CcValueClass, CcExpr* left, int min_prec, CcExpr* _Nullable* _Nonnull out);
static int cc_parse_prefix(CcParser* p, CcValueClass, CcExpr* _Nullable* _Nonnull out);
static int cc_parse_primary(CcParser* p, CcValueClass, CcExpr* _Nullable* _Nonnull out);
static int cc_parse_postfix(CcParser* p, CcValueClass, CcExpr* operand, CcExpr* _Nullable* _Nonnull out);
static int cc_parse_lambda(CcParser* p, CcValueClass, SrcLoc loc, CcExpr* _Nullable* _Nonnull out);
static int cc_parse_Generic(CcParser* p, CcValueClass, CcExpr* _Nullable* _Nonnull out);
static int cc_next_token(CcParser* p, CcToken* tok);
static int cc_unget(CcParser* p, CcToken* tok);
static int cc_peek(CcParser* p, CcToken* tok);
static int cc_expect_punct(CcParser* p, CcPunct punct);
static void _cc_release_expr(CcParser* p, CcExpr*, size_t nvalues);
static void cc_release_expr(CcParser* p, CcExpr*);
static CcExpr*_Nullable _cc_alloc_expr(CcParser* p, size_t nvalues);
static CcExpr*_Nullable cc_make_expr(CcParser* p, CcExprKind kind, SrcLoc loc, CcQualType type, size_t nvalues);
warn_unused static int cc_pointer_of(CcParser* p, CcQualType pointee, CcQualType* out);
LOG_PRINTF(3, 4) static int cc_error(CcParser*, SrcLoc, const char*, ...);
LOG_PRINTF(3, 4) static void cc_warn(CcParser*, SrcLoc, const char*, ...);
LOG_PRINTF(3, 4) static void cc_info(CcParser*, SrcLoc, const char*, ...);
LOG_PRINTF(3, 4) static void cc_debug(CcParser*, SrcLoc, const char*, ...);
#define cc_unimplemented(p, loc, msg) (cc_error(p, loc, "UNIMPLEMENTED: " msg " at %s:%d", __FILE__, __LINE__), CC_UNIMPLEMENTED_ERROR)
#define cc_unreachable(p, loc, msg) (cc_error(p, loc, "UNREACHABLE code reached: " msg " at %s:%d", __FILE__, __LINE__), CC_UNREACHABLE_ERROR)
static _Bool cc_binop_lookup(CcPunct punct, CcExprKind* kind, int* prec);
static int cc_va_list_to_ptr(CcParser* p, SrcLoc loc, CcExpr*_Nonnull*_Nonnull e);
typedef struct CcEvalResult CcEvalResult;
struct CcEvalResult {
    enum { CC_EVAL_INT, CC_EVAL_UINT, CC_EVAL_FLOAT, CC_EVAL_DOUBLE, CC_EVAL_TYPE, CC_EVAL_VOID, CC_EVAL_INIT_LIST, CC_EVAL_STRING} kind;
    union { int64_t i; uint64_t u; float f; double d; CcQualType type; CcInitList* init_list; struct { const char* text; size_t length; } str; };
};
static int cc_eval_expr(CcParser* p, CcExpr* e, CcEvalResult* result);
static _Bool cc_assign_lookup(CcPunct punct, CcExprKind* kind);
static Marray(CcToken)*_Nullable cc_get_scratch(CcParser* p);
static void cc_release_scratch(CcParser* p, Marray(CcToken)*);
static Allocator cc_allocator(CcParser*p);
static Allocator cc_scratch_allocator(CcParser*p);
static int cc_parse_declarator(CcParser* p, CcQualType* out_head, CcQualType*_Nonnull*_Nonnull out_tail, Atom _Nullable * _Nullable out_name, Marray(Atom) *_Nullable out_param_names);
static CcQualType cc_intern_qualtype(CcParser* p, CcQualType t);
static _Bool cc_is_type_start(CcParser* p, CcToken* tok);
static int cc_parse_func_body_inner(CcParser* p, CcFunc* f, _Bool terminate_on_rbrace);
static int cc_parse_type_name(CcParser* p, CcQualType* out);
static int cc_sizeof_as_expr(CcParser* p, CcQualType t, SrcLoc loc, CcExpr* _Nullable* _Nonnull out);
static int cc_alignof_as_expr(CcParser* p, CcQualType t, SrcLoc loc, CcExpr* _Nullable* _Nonnull out);
static int cc_sizeof_as_uint(CcParser* p, CcQualType t, SrcLoc loc, uint32_t* out);
static int cc_alignof_as_uint(CcParser* p, CcQualType t, SrcLoc loc, uint32_t* out);
static int cc_check_cast(CcParser* p, CcQualType from, CcQualType to, SrcLoc loc);
static void cc_print_type(MStringBuilder*, CcQualType t);
static CcExpr* _Nullable cc_value_expr(CcParser* p, SrcLoc loc, CcQualType type);
static CcExpr* _Nullable cc_unary_expr(CcParser* p, CcExprKind kind, SrcLoc loc, CcQualType type, CcExpr* operand);
static CcExpr* _Nullable cc_binary_expr(CcParser* p, CcExprKind kind, SrcLoc loc, CcQualType type, CcExpr* left, CcExpr* right);
typedef struct CcDeclBase CcDeclBase;
static int cc_check_func_compat(CcParser* p, CcFunc* existing, const CcDeclBase* declbase, CcQualType new_type, SrcLoc loc);
static int cc_parse_attributes(CcParser* p, CcAttributes* attrs);
static int cc_parse_struct_or_union(CcParser* p, SrcLoc loc, _Bool is_union, CcQualType* base_type);
static int cc_check_anon_member_duplicates(CcParser* p, CcField* existing, uint32_t existing_count, CcQualType anon_type, SrcLoc loc);
static _Bool cc_lookup_field(CcField* _Nullable fields, uint32_t field_count, Atom name, CcFieldLoc* out_loc, CcQualType* out_type, CcFunc*_Nullable*_Nullable out_method);
static int cc_compute_struct_layout(CcParser* p, CcStruct* s, uint16_t pack_value);
static int cc_compute_union_layout(CcParser* p, CcUnion* u, uint16_t pack_value);
static int cc_parse_init_list(CcParser* p, CcValueClass vc, CcExpr* _Nullable* _Nonnull out, CcQualType target_type);
static int cc_desugar_compound_literal(CcParser* p, CcExpr* compound_lit, CcExpr*_Nullable*_Nonnull out);
static const CcTargetConfig* cc_target(const CcParser*);
static int cc_handle_static_asssert(CcParser*);
static int cc_stmt(CcParser*, CcStmtKind, SrcLoc, size_t*);
static CcStatement*_Nullable cc_get_stmt(CcParser*, size_t);
static uint32_t cc_type_sizeof_assume_complete(const CcTargetConfig* tc, CcQualType type);

enum {
    CC_NO_ERROR                 = _cc_no_error,
    CC_OOM_ERROR                = _cc_oom_error,
    CC_SYNTAX_ERROR             = _cc_syntax_error,
    CC_UNREACHABLE_ERROR        = _cc_unreachable_error,
    CC_UNIMPLEMENTED_ERROR      = _cc_unimplemented_error,
    CC_FILE_NOT_FOUND_ERROR     = _cc_file_not_found_error,
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

#ifndef DEFAULT_UNREACHABLE
#if defined __GNUC__ && !defined __clang__
#define DEFAULT_UNREACHABLE default: __builtin_unreachable()
#elif defined _MSC_VER
#define DEFAULT_UNREACHABLE default: __assume(0)
#else
#define DEFAULT_UNREACHABLE
#endif
#endif

static int cc_parse_init(CcParser* p, CcValueClass vc, CcQualType target, uint64_t base_offset, _Bool braced, SrcLoc loc, Marray(CcInitEntry)* buf, uint32_t*_Nullable out_max_index, CcExpr*_Nullable first_value);

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
static int cc_parse_statement(CcParser* p);
static int cc_parse_one(CcParser* p);
static int cc_resolve_gotos(CcParser* p, CcStatement* stmts, size_t count, const AtomMap(uintptr_t)* labels);
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

// Integer promotion: types with rank < int promote to int
// Returns 0 on success, non-zero if t is not an arithmetic type.
static
int
cc_integer_promote(CcParser* p, CcQualType t, CcQualType* out, SrcLoc loc){
    if(!ccqt_is_basic(t) && ccqt_kind(t) == CC_ENUM)
        t = ccqt_as_enum(t)->underlying;
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

// C 6.3.1.8 usual arithmetic conversions
// Returns 0 on success, non-zero if operands are not arithmetic types.
static
int
cc_usual_arithmetic(CcParser* p, CcQualType a, CcQualType b, CcQualType* out, SrcLoc loc){
    // Promote enums to their underlying integer type.
    if(!ccqt_is_basic(a) && ccqt_kind(a) == CC_ENUM)
        a = ccqt_as_enum(a)->underlying;
    if(!ccqt_is_basic(b) && ccqt_kind(b) == CC_ENUM)
        b = ccqt_as_enum(b)->underlying;
    if(!ccqt_is_basic(a) || !ccqt_is_basic(b))
        return cc_error(p, loc, "usual arithmetic conversions require arithmetic types");
    CcBasicTypeKind ak = a.basic.kind, bk = b.basic.kind;
    if(ak == CCBT_void || bk == CCBT_void)
        return cc_error(p, loc, "usual arithmetic conversions on void");
    // _Float128
    if(ak == CCBT_float128 || bk == CCBT_float128){
        *out = ccqt_basic(CCBT_float128); return 0;
    }
    // long double
    if(ak == CCBT_long_double || bk == CCBT_long_double){
        *out = ccqt_basic(CCBT_long_double); return 0;
    }
    // double
    if(ak == CCBT_double || bk == CCBT_double){
        *out = ccqt_basic(CCBT_double); return 0;
    }
    // float
    if(ak == CCBT_float || bk == CCBT_float){
        *out = ccqt_basic(CCBT_float); return 0;
    }
    // Integer promotions
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
    // Different signedness
    CcBasicTypeKind u = a_unsigned ? ak : bk;
    CcBasicTypeKind s = a_unsigned ? bk : ak;
    if(ccbt_int_rank(u) >= ccbt_int_rank(s)){
        *out = ccqt_basic(u); return 0;
    }
    // Signed has higher rank. Use it only if it can represent all
    // values of the unsigned type (i.e. is strictly wider).
    if(cc_target(p)->sizeof_[s] > cc_target(p)->sizeof_[u]){
        *out = ccqt_basic(s); return 0;
    }
    *out = ccqt_basic(ccbt_to_unsigned(s));
    return 0;
}

static
int
cc_require_scalar(CcParser* p, CcExpr* e, SrcLoc loc, const char* context){
    CcTypeKind k = ccqt_kind(e->type);
    if(k == CC_POINTER || k == CC_ENUM || k == CC_ARRAY)
        return 0;
    if(ccqt_is_basic(e->type))
        return 0;
    return cc_error(p, loc, "%s requires scalar type", context);
}

// Extract pointee type from pointer or element type from array
// Returns 0 on success, non-zero if t is not a pointer or array.
static
int
cc_deref_type(CcParser* p, CcQualType t, CcQualType* out, SrcLoc loc){
    if(!ccqt_is_basic(t)){
        CcTypeKind kind = ccqt_kind(t);
        if(kind == CC_POINTER){
            *out = ccqt_as_ptr(t)->pointee;
            return 0;
        }
        if(kind == CC_ARRAY && !ccqt_as_array(t)->is_vector){
            *out = ccqt_as_array(t)->element;
            return 0;
        }
    }
    return cc_error(p, loc, "dereferencing non-pointer type");
}

// Check if `from` can be implicitly converted to `to`.
// Based on C2y 6.5.17.2 simple assignment constraints.
static
_Bool
cc_implicit_convertible(CcQualType from, CcQualType to){
    if(from.bits == to.bits) return 1;
    CcTypeKind fk = ccqt_kind(from), tk = ccqt_kind(to);
    _Bool f_arith = (fk == CC_BASIC && ccbt_is_arithmetic(from.basic.kind)) || fk == CC_ENUM;
    _Bool t_arith = (tk == CC_BASIC && ccbt_is_arithmetic(to.basic.kind)) || tk == CC_ENUM;
    // arithmetic <- arithmetic (includes bool, enum)
    if(f_arith && t_arith) return 1;
    // struct/union <- compatible struct/union (same type, ignoring qualifiers)
    if(fk == CC_STRUCT && tk == CC_STRUCT) return from.ptr == to.ptr;
    if(fk == CC_UNION && tk == CC_UNION) return from.ptr == to.ptr;
    // pointer <- pointer
    // The left pointee must have all qualifiers of the right pointee.
    // One of the pointees may be void.
    if(fk == CC_POINTER && tk == CC_POINTER){
        CcQualType fp = ccqt_as_ptr(from)->pointee;
        CcQualType tp = ccqt_as_ptr(to)->pointee;
        // void* <-> any* is ok
        _Bool fvoid = ccqt_is_basic(fp) && fp.basic.kind == CCBT_void;
        _Bool tvoid = ccqt_is_basic(tp) && tp.basic.kind == CCBT_void;
        if(fvoid || tvoid || fp.ptr == tp.ptr){
            // target pointee must have all qualifiers of source pointee
            if((fp.is_const    && !tp.is_const)
            || (fp.is_volatile && !tp.is_volatile)
            || (fp.is_atomic   && !tp.is_atomic))
                return 0;
            return 1;
        }
        // Plan9 extension: ptr-to-struct -> ptr-to-anonymously-embedded-struct
        if(ccqt_kind(fp) == CC_STRUCT && ccqt_kind(tp) == CC_STRUCT){
            CcStruct* fs = ccqt_as_struct(fp);
            for(uint32_t i = 0; i < fs->field_count; i++){
                CcField* f = &fs->fields[i];
                if(f->name || f->is_method || f->is_bitfield) continue;
                if(f->type.ptr == tp.ptr) return 1;
            }
        }
        return 0;
    }
    // array decays to pointer (but not vectors)
    if(fk == CC_ARRAY && tk == CC_POINTER && !ccqt_as_array(from)->is_vector) return 1;
    // function decays to function pointer
    if(fk == CC_FUNCTION && tk == CC_POINTER) return 1;
    // pointer <- nullptr_t
    if(fk == CC_BASIC && from.basic.kind == CCBT_nullptr_t && tk == CC_POINTER) return 1;
    // nullptr_t <- nullptr_t (different qualifiers)
    if(fk == CC_BASIC && from.basic.kind == CCBT_nullptr_t
    && tk == CC_BASIC && to.basic.kind == CCBT_nullptr_t) return 1;
    // bool <- pointer or nullptr_t
    if(tk == CC_BASIC && to.basic.kind == CCBT_bool){
        if(fk == CC_POINTER) return 1;
        if(fk == CC_BASIC && from.basic.kind == CCBT_nullptr_t) return 1;
    }
    // vector <- compatible vector (same type, ignoring qualifiers)
    if(fk == CC_ARRAY && tk == CC_ARRAY && ccqt_as_array(from)->is_vector && ccqt_as_array(to)->is_vector)
        return from.ptr == to.ptr;
    return 0;
}

static
_Bool
cc_explicit_castable(CcQualType from, CcQualType to){
    if(from.bits == to.bits) return 1;
    if(ccqt_is_basic(to) && to.basic.kind == CCBT_void) return 1;
    CcTypeKind fk = ccqt_kind(from), tk = ccqt_kind(to);
    if(fk == CC_ENUM){
        from = ccqt_as_enum(from)->underlying;
        fk = ccqt_kind(from);
    }
    if(tk == CC_ENUM){
        to = ccqt_as_enum(to)->underlying;
        tk = ccqt_kind(to);
    }
    if(tk == CC_ARRAY || tk == CC_FUNCTION) return 0;
    if(tk == CC_STRUCT || tk == CC_UNION)
        return from.ptr == to.ptr;
    if(fk == CC_STRUCT || fk == CC_UNION) return 0;
    if(fk == CC_BASIC && from.basic.kind == CCBT_void) return 0;
    _Bool f_arith = (fk == CC_BASIC && ccbt_is_arithmetic(from.basic.kind));
    _Bool t_arith = (tk == CC_BASIC && ccbt_is_arithmetic(to.basic.kind));
    if(f_arith && t_arith) return 1;
    _Bool f_ptr = fk == CC_POINTER || (fk == CC_ARRAY && !ccqt_as_array(from)->is_vector) || fk == CC_FUNCTION || (fk == CC_BASIC && from.basic.kind == CCBT_nullptr_t);
    _Bool t_ptr = tk == CC_POINTER || (tk == CC_BASIC && to.basic.kind == CCBT_nullptr_t);
    if(f_ptr && t_ptr) return 1;
    if(f_ptr && (tk == CC_BASIC && ccbt_is_integer(to.basic.kind))) return 1;
    if(fk == CC_BASIC && ccbt_is_integer(from.basic.kind) && t_ptr) return 1;
    return 0;
}

// Wrap an expression in an implicit cast if types differ.
// Returns 0 on success, error on incompatible types or OOM.
static
int
cc_implicit_cast(CcParser* p, CcExpr* e, CcQualType target, CcExpr* _Nullable* _Nonnull out){
    if(target.basic.kind == CCBT_void && ccqt_is_basic(target)){
        *out = e; return 0;
    }
    if(e->type.bits == target.bits){
        *out = e; return 0;
    }
    // null pointer constant: integer literal 0 -> pointer
    _Bool is_npc = ccqt_kind(target) == CC_POINTER
        && e->kind == CC_EXPR_VALUE
        && ccqt_is_basic(e->type)
        && ccbt_is_integer(e->type.basic.kind)
        && e->uinteger == 0;
    if(!is_npc && !cc_implicit_convertible(e->type, target)){
        cpp_msg_preamble(&p->cpp, e->loc, "error");
        MStringBuilder* buff = &p->cpp.logger->buff;
        msb_write_literal(buff, "cannot implicitly convert from '");
        cc_print_type(buff, e->type);
        msb_write_literal(buff, "' to '");
        cc_print_type(buff, target);
        msb_write_char(buff, '\'');
        log_flush(p->cpp.logger, LOG_PRINT_ERROR);
        cpp_msg_postamble(&p->cpp, e->loc, LOG_PRINT_ERROR);
        return CC_SYNTAX_ERROR;
    }
    if(e->kind == CC_EXPR_COMPOUND_LITERAL){
        int err = cc_desugar_compound_literal(p, e, &e);
        if(err) return err;
    }
    CcExpr* cast = cc_make_expr(p, CC_EXPR_CAST, e->loc, target, 0);
    if(!cast) return CC_OOM_ERROR;
    cast->lhs = e;
    *out = cast;
    return 0;
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
            case CC__Type:
                return 1;
            default:
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
cc_parse_type_name(CcParser* p, CcQualType* out){
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
    err = cc_parse_declarator(p, &head, &tail, NULL, NULL);
    if(err) return err;
    *tail = base.type;
    *out = cc_intern_qualtype(p, head);
    return 0;
}

static
int
cc_parse_lambda(CcParser* p, CcValueClass vc, SrcLoc loc, CcExpr* _Nullable* _Nonnull out){
    // Parse the function type using existing type parsing machinery.
    // e.g. int(int x, int y) — this is already a valid function type.
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
    Marray(Atom) param_names = {0};
    err = cc_parse_declarator(p, &head, &tail, NULL, &param_names);
    if(err){ ma_cleanup(Atom)(&param_names, cc_allocator(p)); return err; }
    *tail = base.type;
    CcQualType type = cc_intern_qualtype(p, head);
    CcToken peek;
    err = cc_peek(p, &peek);
    if(err){
        ma_cleanup(Atom)(&param_names, cc_allocator(p));
        return err;
    }
    if(ccqt_kind(type) != CC_FUNCTION){
        ma_cleanup(Atom)(&param_names, cc_allocator(p));
        if(peek.type == CC_PUNCTUATOR && peek.punct.punct == CC_lbrace)
            return cc_error(p, loc, "Lambda requires a function type, got non-function type");
    }
    if(peek.type != CC_PUNCTUATOR || peek.punct.punct != CC_lbrace){
        ma_cleanup(Atom)(&param_names, cc_allocator(p));
        CcExpr* type_val = cc_value_expr(p, loc, ccqt_basic(CCBT__Type));
        if(!type_val) return CC_OOM_ERROR;
        type_val->uinteger = type.bits;
        return cc_parse_postfix(p, vc, type_val, out);
    }
    err = cc_next_token(p, &peek); // consume '{'
    if(err){ ma_cleanup(Atom)(&param_names, cc_allocator(p)); return err; }
    // Create anonymous CcFunc
    CcFunction* ftype = ccqt_as_function(type);
    CcFunc* func = Allocator_zalloc(cc_allocator(p), sizeof *func);
    if(!func){ ma_cleanup(Atom)(&param_names, cc_allocator(p)); return CC_OOM_ERROR; }
    func->name = NULL;
    func->type = ftype;
    func->loc = loc;
    func->defined = 1;
    func->params.count = param_names.count;
    func->params.data = param_names.data;
    func->enclosing = p->current_func;
    err = cc_parse_func_body_inner(p, func, 1);
    if(err) return err;
    // consume '}'
    err = cc_expect_punct(p, CC_rbrace);
    if(err) return err;
    // Build CC_EXPR_FUNCTION node
    CcExpr* node = cc_make_expr(p, CC_EXPR_FUNCTION, loc, (CcQualType){.bits=(uintptr_t)func->type}, 0);
    if(!node) return CC_OOM_ERROR;
    node->func = func;
    err = PM_put(&p->used_funcs, cc_allocator(p), func, func);
    if(err) return CC_OOM_ERROR;
    return cc_parse_postfix(p, vc, node, out);
}


// Desugar a CC_EXPR_COMPOUND_LITERAL into an anonymous variable:
//   (type){init} -> (__anon = init_list, __anon)
// The anonymous variable has block scope lifetime.
static
int
cc_desugar_compound_literal(CcParser* p, CcExpr* cl, CcExpr*_Nullable*_Nonnull out){
    CcQualType type = cl->type;
    SrcLoc loc = cl->loc;
    CcVariable* anon = Allocator_zalloc(cc_allocator(p), sizeof *anon);
    if(!anon) return CC_OOM_ERROR;
    *anon = (CcVariable){
        .name = nil_atom,
        .loc = loc,
        .type = type,
        .automatic = p->current_func != NULL,
    };
    if(anon->automatic){
        uint32_t sz, align;
        int err = cc_sizeof_as_uint(p, type, loc, &sz);
        if(err) return err;
        err = cc_alignof_as_uint(p, type, loc, &align);
        if(err) return err;
        p->current_func->frame_size = (p->current_func->frame_size + align - 1) & ~(align - 1);
        anon->frame_offset = p->current_func->frame_size;
        p->current_func->frame_size += sz;
    }
    else {
        int err = PM_put(&p->used_vars, cc_allocator(p), anon, anon);
        if(err) return CC_OOM_ERROR;
    }
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

// Validate that a cast from `from` to `to` is legal.
static
int
cc_check_cast(CcParser* p, CcQualType from, CcQualType to, SrcLoc loc){
    // Cast to void is always valid.
    if(ccqt_is_basic(to) && to.basic.kind == CCBT_void) return 0;
    CcTypeKind to_kind = ccqt_kind(to);
    CcTypeKind from_kind = ccqt_kind(from);
    if(to_kind == CC_ENUM){
        to = ccqt_as_enum(to)->underlying;
        to_kind = ccqt_kind(to);
    }
    if(from_kind == CC_ENUM){
        from = ccqt_as_enum(from)->underlying;
        from_kind = ccqt_kind(from);
    }
    // Cannot cast to array, function, struct, or union.
    if(to_kind == CC_ARRAY)
        return cc_error(p, loc, "cannot cast to array type");
    if(to_kind == CC_FUNCTION)
        return cc_error(p, loc, "cannot cast to function type");
    if(to_kind == CC_STRUCT || to_kind == CC_UNION){
        // Same-type struct/union assignment is valid.
        if(from.ptr == to.ptr)
            return 0;
        return cc_error(p, loc, "cannot cast to struct or union type");
    }
    // Cannot cast from struct, union, or void.
    if(from_kind == CC_STRUCT || from_kind == CC_UNION)
        return cc_error(p, loc, "cannot cast from struct or union type");
    if(from_kind == CC_BASIC && from.basic.kind == CCBT_void)
        return cc_error(p, loc, "cannot cast from void");
    // Arithmetic <-> arithmetic is always valid.
    // (integer, float, complex, bool, enum underlying types)
    _Bool from_arith = from_kind == CC_BASIC && ccbt_is_arithmetic(from.basic.kind);
    _Bool to_arith = to_kind == CC_BASIC && ccbt_is_arithmetic(to.basic.kind);
    if(from_arith && to_arith)
        return 0;
    // Pointer/array/function sources are pointer-like for cast purposes.
    _Bool from_ptr = from_kind == CC_POINTER || (from_kind == CC_ARRAY && !ccqt_as_array(from)->is_vector) || from_kind == CC_FUNCTION || (from_kind == CC_BASIC && from.basic.kind == CCBT_nullptr_t);
    _Bool to_ptr = to_kind == CC_POINTER || (to_kind == CC_BASIC && to.basic.kind == CCBT_nullptr_t);
    // Pointer <-> pointer.
    if(from_ptr && to_ptr)
        return 0;
    // Pointer <-> integer (includes bool and enum).
    if(from_ptr && to_kind == CC_BASIC && ccbt_is_integer(to.basic.kind))
        return 0;
    if(to_ptr && from_kind == CC_BASIC && ccbt_is_integer(from.basic.kind))
        return 0;
    // Pointer <-> float is not allowed.
    return cc_error(p, loc, "invalid cast");
}

static
int
cc_check_func_compat(CcParser* p, CcFunc* existing, const CcDeclBase* declbase, CcQualType new_ftype, SrcLoc loc){
    CcFunction* new_type = ccqt_as_function(new_ftype);
    CcFunction* old_type = existing->type;
    // Check linkage: static must be consistent.
    if(existing->static_ && !declbase->spec.sp_static)
        return cc_error(p, loc, "non-static declaration of '%.*s' follows static declaration", existing->name->length, existing->name->data);
    if(!existing->static_ && declbase->spec.sp_static)
        return cc_error(p, loc, "static declaration of '%.*s' follows non-static declaration", existing->name->length, existing->name->data);
    // If either side has no prototype (K&R), skip type checking.
    if(old_type->no_prototype || new_type->no_prototype)
        return 0;
    // Check return type.
    if(old_type->return_type.bits != new_type->return_type.bits)
        return cc_error(p, loc, "conflicting return type for '%.*s'", existing->name->length, existing->name->data);
    // Check variadic.
    if(old_type->is_variadic != new_type->is_variadic)
        return cc_error(p, loc, "conflicting variadic specifier for '%.*s'", existing->name->length, existing->name->data);
    // Check parameter count.
    if(old_type->param_count != new_type->param_count)
        return cc_error(p, loc, "conflicting number of parameters for '%.*s'", existing->name->length, existing->name->data);
    // Check parameter types.
    for(uint32_t i = 0; i < old_type->param_count; i++){
        // Compare ignoring top-level qualifiers on params (C permits that).
        uintptr_t old_bits = old_type->params[i].bits & ~(uintptr_t)7;
        uintptr_t new_bits = new_type->params[i].bits & ~(uintptr_t)7;
        if(old_bits != new_bits)
            return cc_error(p, loc, "conflicting type for parameter %u of '%.*s'", i + 1, existing->name->length, existing->name->data);
    }
    return 0;
}

// Build an expression for sizeof(type).
// For fixed-size types, produces a CC_EXPR_VALUE constant.
// For VLA types, produces an arithmetic expression tree.
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
        case CC_POINTER: {
            CcExpr* node = cc_value_expr(p, loc, size_type);
            if(!node) return CC_OOM_ERROR;
            node->uinteger = tgt->sizeof_[CCBT_nullptr_t];
            *out = node;
            return 0;
        }
        case CC_ARRAY: {
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
                // Runtime: vla_expr * sizeof(element)
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
            // Fixed size: constant fold if element size is constant
            if(elem_size->kind == CC_EXPR_VALUE){
                elem_size->uinteger *= arr->length;
                *out = elem_size;
                return 0;
            }
            // Element is VLA-derived, multiply at runtime
            CcExpr* len = cc_value_expr(p, loc, size_type);
            if(!len) return CC_OOM_ERROR;
            len->uinteger = (uint64_t)arr->length;
            CcExpr* mul = cc_binary_expr(p, CC_EXPR_MUL, loc, size_type, len, elem_size);
            if(!mul) return CC_OOM_ERROR;
            *out = mul;
            return 0;
        }
        case CC_STRUCT: {
            CcStruct* s = ccqt_as_struct(t);
            if(s->is_incomplete)
                return cc_error(p, loc, "sizeof applied to incomplete struct type");
            CcExpr* node = cc_value_expr(p, loc, size_type);
            if(!node) return CC_OOM_ERROR;
            node->uinteger = s->size;
            *out = node;
            return 0;
        }
        case CC_UNION: {
            CcUnion* u = ccqt_as_union(t);
            if(u->is_incomplete)
                return cc_error(p, loc, "sizeof applied to incomplete union type");
            CcExpr* node = cc_value_expr(p, loc, size_type);
            if(!node) return CC_OOM_ERROR;
            node->uinteger = u->size;
            *out = node;
            return 0;
        }
        case CC_ENUM: {
            CcEnum* e = ccqt_as_enum(t);
            return cc_sizeof_as_expr(p, e->underlying, loc, out);
        }
        case CC_FUNCTION:
            return cc_error(p, loc, "sizeof applied to function type");
    }
    #ifdef __GNUC__
    __builtin_unreachable();
    #endif
}

// Build an expression for alignof(type).
// Alignment is always a compile-time constant.
static
int
cc_alignof_as_expr(CcParser* p, CcQualType t, SrcLoc loc, CcExpr* _Nullable* _Nonnull out){
    const CcTargetConfig* cfg = cc_target(p);
    CcQualType size_type = ccqt_basic(cfg->size_type);
    uint64_t align;
    switch(ccqt_kind(t)){
        DEFAULT_UNREACHABLE;
        case CC_BASIC:{
            if(t.basic.kind >= CCBT_COUNT)
                return cc_error(p, loc, "alignof applied to invalid kind");
            CcExpr* node = cc_value_expr(p, loc, size_type);
            if(!node) return CC_OOM_ERROR;
            node->uinteger = cfg->alignof_[t.basic.kind];
            *out = node;
            return 0;
        }
        case CC_POINTER:
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

// Get the sizeof a type without creating an expression node.
// Returns 0 for incomplete types.
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
        case CC_POINTER: {
            *out = tgt->sizeof_[CCBT_nullptr_t];
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
            *out = (uint32_t)arr->length * elem_size;
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
            return cc_sizeof_as_uint(p, e->underlying, loc, out);
        }
        case CC_FUNCTION:
            return ((void)cc_error(p, loc, "Taking sizeof of a function type (not function pointer type)"), CC_UNREACHABLE_ERROR);
    }
    #ifdef __GNUC__
    __builtin_unreachable();
    #endif
}

// Get the alignof a type without creating an expression node.
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
        case CC_POINTER:
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
            return cc_alignof_as_uint(p, e->underlying, loc, out);
        }
        case CC_FUNCTION:
            return ((void)cc_error(p, loc, "Taking alignof of a function type (not function pointer type)"), CC_UNREACHABLE_ERROR);
    }
    #ifdef __GNUC__
    __builtin_unreachable();
    #endif
}

static
_Bool
cc_binop_lookup(CcPunct punct, CcExprKind* kind, int* prec){
    // Precedences: higher number = tighter binding
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

// comma expression (lowest precedence)
static
int
cc_parse_expr(CcParser* p, CcValueClass vc, CcExpr* _Nullable* _Nonnull out){
    CcExpr* left;
    int err = cc_parse_assignment_expr(p, vc, &left);
    if(err) return err;
    for(;;){
        CcToken tok;
        err = cc_next_token(p, &tok);
        if(err) return err;
        if(tok.type == CC_PUNCTUATOR && tok.punct.punct == CC_comma){
            CcExpr* right;
            err = cc_parse_assignment_expr(p, vc, &right);
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

// assignment expression (right-associative) + ternary
static
int
cc_parse_assignment_expr(CcParser* p, CcValueClass vc, CcExpr* _Nullable* _Nonnull out){
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
            if(!left->is_lvalue)
                return cc_error(p, tok.loc, "expression is not assignable");
            if(left->type.is_const)
                return cc_error(p, tok.loc, "cannot assign to variable with const-qualified type");
            CcExpr* right;
            CcToken peek_assign;
            err = cc_peek(p, &peek_assign);
            if(err) return err;
            if(kind == CC_EXPR_ASSIGN && peek_assign.type == CC_PUNCTUATOR && peek_assign.punct.punct == CC_lbrace){
                err = cc_parse_init_list(p, vc, &right, left->type);
            }
            else {
                // right-associative: recurse into assignment_expr
                err = cc_parse_assignment_expr(p, vc, &right);
            }
            if(err) return err;
            // Compound assignments (other than += and -=) require arithmetic LHS.
            if(kind != CC_EXPR_ASSIGN && kind != CC_EXPR_ADDASSIGN && kind != CC_EXPR_SUBASSIGN){
                CcQualType lt = left->type;
                if(!ccqt_is_basic(lt) && ccqt_kind(lt) == CC_ENUM) lt = ccqt_as_enum(lt)->underlying;
                if(!ccqt_is_basic(lt) || !ccbt_is_arithmetic(lt.basic.kind)){
                    return cc_error(p, tok.loc, "compound assignment requires arithmetic operands");
                }
            }
            // Integer-only compound assignments
            if(kind == CC_EXPR_MODASSIGN || kind == CC_EXPR_BITANDASSIGN
            || kind == CC_EXPR_BITORASSIGN || kind == CC_EXPR_BITXORASSIGN
            || kind == CC_EXPR_LSHIFTASSIGN || kind == CC_EXPR_RSHIFTASSIGN){
                CcQualType lt = left->type;
                if(!ccqt_is_basic(lt) && ccqt_kind(lt) == CC_ENUM) lt = ccqt_as_enum(lt)->underlying;
                if(!ccqt_is_basic(lt) || !ccbt_is_integer(lt.basic.kind))
                    return cc_error(p, tok.loc, "operator requires integer operands");
            }
            // For += and -=, pointer +/- integer is valid pointer arithmetic.
            if((kind == CC_EXPR_ADDASSIGN || kind == CC_EXPR_SUBASSIGN)
                && ccqt_kind(left->type) == CC_POINTER){
                CcQualType rt = right->type;
                if(!ccqt_is_basic(rt) && ccqt_kind(rt) == CC_ENUM) rt = ccqt_as_enum(rt)->underlying;
                if(!ccqt_is_basic(rt) || !ccbt_is_integer(rt.basic.kind))
                    return cc_error(p, tok.loc, "pointer arithmetic requires integer operand");
                // no cast needed
            }
            else {
                err = cc_implicit_cast(p, right, left->type, &right);
                if(err) return err;
            }
            if(kind == CC_EXPR_ASSIGN && right->kind == CC_EXPR_COMPOUND_LITERAL){
                err = cc_desugar_compound_literal(p, right, &right);
                if(err) return err;
            }
            CcExpr* node = cc_make_expr(p, kind, tok.loc, left->type, 1);
            if(!node) return CC_OOM_ERROR;
            node->lhs = left;
            node->values[0] = right;
            *out = node;
            return 0;
        }
    }
    cc_unget(p, &tok);
    *out = left;
    return 0;
}

// ternary expression
static
int
cc_parse_ternary_expr(CcParser* p, CcValueClass vc, CcExpr* _Nullable* _Nonnull out){
    CcExpr* cond;
    // Parse the condition using infix with minimum precedence
    int err = cc_parse_prefix(p, vc, &cond);
    if(err) return err;
    err = cc_parse_infix(p, vc, cond, 4, &cond);
    if(err) return err;
    CcToken tok;
    err = cc_next_token(p, &tok);
    if(err) return err;
    if(tok.type == CC_PUNCTUATOR && tok.punct.punct == CC_question){
        err = cc_require_scalar(p, cond, tok.loc, "'?:'");
        if(err) return err;
        CcExpr* then_expr;
        err = cc_parse_expr(p, vc, &then_expr);
        if(err) return err;
        err = cc_expect_punct(p, CC_colon);
        if(err) return err;
        CcExpr* else_expr;
        // right-associative: recurse into ternary
        err = cc_parse_ternary_expr(p, vc, &else_expr);
        if(err) return err;
        CcQualType common;
        CcTypeKind tk = ccqt_kind(then_expr->type);
        CcTypeKind ek = ccqt_kind(else_expr->type);
        _Bool tptr = ccqt_is_pointer_like(then_expr->type) || tk == CC_FUNCTION;
        _Bool eptr = ccqt_is_pointer_like(else_expr->type) || ek == CC_FUNCTION;
        if(tptr && eptr){
            // Decay arrays/functions to pointers.
            CcQualType ttype = then_expr->type;
            CcQualType etype = else_expr->type;
            if(tk == CC_ARRAY && !ccqt_as_array(ttype)->is_vector){
                err = cc_pointer_of(p, ccqt_as_array(ttype)->element, &ttype);
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
                err = cc_pointer_of(p, ccqt_as_array(etype)->element, &etype);
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
            // Both are now pointers. Merge qualifiers, prefer void* if either side is void*.
            CcPointer* tp = ccqt_as_ptr(ttype);
            CcPointer* ep = ccqt_as_ptr(etype);
            CcQualType tpointee = tp->pointee;
            CcQualType epointee = ep->pointee;
            _Bool tvoid = ccqt_is_basic(tpointee) && tpointee.basic.kind == CCBT_void;
            _Bool evoid = ccqt_is_basic(epointee) && epointee.basic.kind == CCBT_void;
            CcQualType pointee;
            if(tvoid || evoid){
                pointee = ccqt_basic(CCBT_void);
                pointee.is_const    = tpointee.is_const    | epointee.is_const;
                pointee.is_volatile = tpointee.is_volatile | epointee.is_volatile;
                pointee.is_atomic   = tpointee.is_atomic   | epointee.is_atomic;
            }
            else {
                pointee = tpointee;
                pointee.is_const    |= epointee.is_const;
                pointee.is_volatile |= epointee.is_volatile;
                pointee.is_atomic   |= epointee.is_atomic;
            }
            err = cc_pointer_of(p, pointee, &common);
            if(err) return err;
            common.is_const    = ttype.is_const    | etype.is_const;
            common.is_volatile = ttype.is_volatile | etype.is_volatile;
            common.is_atomic   = ttype.is_atomic   | etype.is_atomic;
        }
        else if(tptr && ccqt_is_basic(else_expr->type)){
            // pointer and null pointer constant or nullptr_t
            _Bool is_npc = else_expr->kind == CC_EXPR_VALUE
                && ccbt_is_integer(else_expr->type.basic.kind)
                && else_expr->uinteger == 0;
            _Bool is_nullptr = else_expr->type.basic.kind == CCBT_nullptr_t;
            if(!is_npc && !is_nullptr)
                return cc_error(p, tok.loc, "incompatible operand types for ternary");
            common = then_expr->type;
        }
        else if(eptr && ccqt_is_basic(then_expr->type)){
            _Bool is_npc = then_expr->kind == CC_EXPR_VALUE
                && ccbt_is_integer(then_expr->type.basic.kind)
                && then_expr->uinteger == 0;
            _Bool is_nullptr = then_expr->type.basic.kind == CCBT_nullptr_t;
            if(!is_npc && !is_nullptr)
                return cc_error(p, tok.loc, "incompatible operand types for ternary");
            common = else_expr->type;
        }
        else if(ccqt_is_basic(then_expr->type) && then_expr->type.basic.kind == CCBT_void
             && ccqt_is_basic(else_expr->type) && else_expr->type.basic.kind == CCBT_void){
            common = then_expr->type;
        }
        else if((tk == CC_STRUCT || tk == CC_UNION) && then_expr->type.ptr == else_expr->type.ptr){
            common = then_expr->type;
        }
        else {
            err = cc_usual_arithmetic(p, then_expr->type, else_expr->type, &common, tok.loc);
            if(err) return err;
        }
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

// Pratt-style infix: left-to-right binary operators
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
        // Look ahead: if next op has higher precedence, recurse
        err = cc_parse_infix(p, vc, right, prec + 1, &right);
        if(err) return err;
        // Compute result type, insert implicit casts
        CcQualType result_type = {0};
        switch(kind){
            case CC_EXPR_LOGAND: case CC_EXPR_LOGOR:
                err = cc_require_scalar(p, left, tok.loc, kind == CC_EXPR_LOGAND ? "'&&'" : "'||'");
                if(err) return err;
                err = cc_require_scalar(p, right, tok.loc, kind == CC_EXPR_LOGAND ? "'&&'" : "'||'");
                if(err) return err;
                result_type = ccqt_basic(CCBT_int);
                break;
            case CC_EXPR_EQ: case CC_EXPR_NE:
            case CC_EXPR_LT: case CC_EXPR_GT:
            case CC_EXPR_LE: case CC_EXPR_GE: {
                // _Type == _Type, _Type != _Type
                if(ccqt_is_basic(left->type) && left->type.basic.kind == CCBT__Type
                && ccqt_is_basic(right->type) && right->type.basic.kind == CCBT__Type){
                    if(kind != CC_EXPR_EQ && kind != CC_EXPR_NE)
                        return cc_error(p, tok.loc, "ordered comparison of _Type values");
                    result_type = ccqt_basic(CCBT_int);
                    break;
                }
                _Bool lp = ccqt_is_pointer_like(left->type);
                _Bool rp = ccqt_is_pointer_like(right->type);
                if(!lp && !rp){
                    CcQualType common;
                    err = cc_usual_arithmetic(p, left->type, right->type, &common, tok.loc);
                    if(err) return err;
                    err = cc_implicit_cast(p, left, common, &left);
                    if(err) return err;
                    err = cc_implicit_cast(p, right, common, &right);
                    if(err) return err;
                }
                else if(lp && rp){
                    CcQualType lpointee, rpointee;
                    cc_deref_type(p, left->type, &lpointee, tok.loc);
                    cc_deref_type(p, right->type, &rpointee, tok.loc);
                    if(_ccqt_to_type_ptr(lpointee) != _ccqt_to_type_ptr(rpointee)
                    && !(ccqt_is_basic(lpointee) && lpointee.basic.kind == CCBT_void)
                    && !(ccqt_is_basic(rpointee) && rpointee.basic.kind == CCBT_void))
                        return cc_error(p, tok.loc, "comparison of incompatible pointer types");
                }
                else {
                    // One pointer, one non-pointer: only == and != with null pointer constant
                    CcExpr* non_ptr = lp ? right : left;
                    _Bool is_npc = non_ptr->kind == CC_EXPR_VALUE
                        && ccqt_is_basic(non_ptr->type)
                        && ccbt_is_integer(non_ptr->type.basic.kind)
                        && non_ptr->uinteger == 0;
                    _Bool is_nullptr = ccqt_is_basic(non_ptr->type) && non_ptr->type.basic.kind == CCBT_nullptr_t;
                    if(!(is_npc || is_nullptr) || (kind != CC_EXPR_EQ && kind != CC_EXPR_NE))
                        return cc_error(p, tok.loc, "comparison of pointer with non-pointer");
                }
                result_type = ccqt_basic(CCBT_int);
                break;
            }
            case CC_EXPR_LSHIFT: case CC_EXPR_RSHIFT: {
                CcQualType lp, rp;
                err = cc_integer_promote(p, left->type, &lp, tok.loc);
                if(err) return err;
                err = cc_integer_promote(p, right->type, &rp, tok.loc);
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
                    {
                        CcQualType it = int_operand->type;
                        if(!ccqt_is_basic(it) && ccqt_kind(it) == CC_ENUM) it = ccqt_as_enum(it)->underlying;
                        if(!ccqt_is_basic(it) || !ccbt_is_integer(it.basic.kind))
                            return cc_error(p, tok.loc, "pointer arithmetic requires integer operand");
                    }
                    if(ccqt_kind((*ptr_operand)->type) == CC_ARRAY){
                        err = cc_pointer_of(p, ccqt_as_array((*ptr_operand)->type)->element, &result_type);
                        if(err) return err;
                        err = cc_implicit_cast(p, *ptr_operand, result_type, ptr_operand);
                        if(err) return err;
                    }
                    else
                        result_type = (*ptr_operand)->type;
                }
                else {
                    err = cc_usual_arithmetic(p, left->type, right->type, &result_type, tok.loc);
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
                    // ptr - ptr = ptrdiff_t
                    CcQualType lp, rp;
                    cc_deref_type(p, left->type, &lp, tok.loc);
                    cc_deref_type(p, right->type, &rp, tok.loc);
                    if(_ccqt_to_type_ptr(lp) != _ccqt_to_type_ptr(rp)
                    && !(ccqt_is_basic(lp) && lp.basic.kind == CCBT_void)
                    && !(ccqt_is_basic(rp) && rp.basic.kind == CCBT_void))
                        return cc_error(p, tok.loc, "pointer subtraction with incompatible types");
                    result_type = ccqt_basic(cc_target(p)->ptrdiff_type);
                }
                else if(lptr){
                    {
                        CcQualType rt = right->type;
                        if(!ccqt_is_basic(rt) && ccqt_kind(rt) == CC_ENUM) rt = ccqt_as_enum(rt)->underlying;
                        if(!ccqt_is_basic(rt) || !ccbt_is_integer(rt.basic.kind))
                            return cc_error(p, tok.loc, "pointer arithmetic requires integer operand");
                    }
                    if(ccqt_kind(left->type) == CC_ARRAY){
                        err = cc_pointer_of(p, ccqt_as_array(left->type)->element, &result_type);
                        if(err) return err;
                        err = cc_implicit_cast(p, left, result_type, &left);
                        if(err) return err;
                    }
                    else
                        result_type = left->type;
                }
                else {
                    err = cc_usual_arithmetic(p, left->type, right->type, &result_type, tok.loc);
                    if(err) return err;
                    err = cc_implicit_cast(p, left, result_type, &left);
                    if(err) return err;
                    err = cc_implicit_cast(p, right, result_type, &right);
                    if(err) return err;
                }
                break;
            }
            default: {
                // arithmetic/bitwise: *,/,%,&,|,^
                err = cc_usual_arithmetic(p, left->type, right->type, &result_type, tok.loc);
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

// Prefix unary operators
static
int
cc_parse_prefix(CcParser* p, CcValueClass vc, CcExpr* _Nullable* _Nonnull out){
    CcToken tok;
    int err = cc_next_token(p, &tok);
    if(err) return err;
    if(tok.type == CC_PUNCTUATOR){
        // Check for (type) cast / compound literal
        if(tok.punct.punct == CC_lparen){
            CcToken peek;
            err = cc_peek(p, &peek);
            if(err) return err;
            if(cc_is_type_start(p, &peek)){
                CcQualType cast_type;
                err = cc_parse_type_name(p, &cast_type);
                if(err) return err;
                err = cc_expect_punct(p, CC_rparen);
                if(err) return err;
                // Check for compound literal: (type){...}
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
                // (type).member → _Type value
                // (type) as _Type value when followed by operator, ), ;, or ,
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
                // Cast expression: (type) operand
                CcExpr* operand;
                err = cc_parse_prefix(p, vc, &operand);
                if(err) return err;
                err = cc_check_cast(p, operand->type, cast_type, tok.loc);
                if(err) return err;
                CcExpr* cast = cc_unary_expr(p, CC_EXPR_CAST, tok.loc, cast_type, operand);
                if(!cast) return CC_OOM_ERROR;
                *out = cast;
                return 0;
            }
            // Not a type — fall through to prefix/primary handling
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
            default: is_prefix = 0; break;
        }
        if(is_prefix){
            if((kind == CC_EXPR_PREINC || kind == CC_EXPR_PREDEC) && vc > CC_RUNTIME_VALUE)
                return cc_error(p, tok.loc, "increment/decrement in constant expression");
            CcExpr* operand;
            // & doesn't evaluate its operand's value, so parse with RUNTIME
            CcValueClass operand_vc = (kind == CC_EXPR_ADDR) ? CC_RUNTIME_VALUE : vc;
            err = cc_parse_prefix(p, operand_vc, &operand);
            if(err) return err;
            // Insert implicit casts and compute type
            CcQualType result_type;
            switch(kind){
                case CC_EXPR_NEG: case CC_EXPR_POS: {
                    err = cc_integer_promote(p, operand->type, &result_type, tok.loc);
                    if(err) return err;
                    err = cc_implicit_cast(p, operand, result_type, &operand);
                    if(err) return err;
                    break;
                }
                case CC_EXPR_BITNOT: {
                    if(!ccqt_is_basic(operand->type) && ccqt_kind(operand->type) == CC_ENUM)
                        operand->type = ccqt_as_enum(operand->type)->underlying;
                    if(!ccqt_is_basic(operand->type))
                        return cc_error(p, tok.loc, "'~' requires integer type");
                    if(ccbt_is_float(operand->type.basic.kind))
                        return cc_error(p, tok.loc, "'~' requires integer type");
                    err = cc_integer_promote(p, operand->type, &result_type, tok.loc);
                    if(err) return err;
                    err = cc_implicit_cast(p, operand, result_type, &operand);
                    if(err) return err;
                    break;
                }
                case CC_EXPR_LOGNOT:
                    err = cc_require_scalar(p, operand, tok.loc, "'!'");
                    if(err) return err;
                    result_type = ccqt_basic(CCBT_int);
                    break;
                case CC_EXPR_DEREF:
                    err = cc_deref_type(p, operand->type, &result_type, tok.loc);
                    if(err) return err;
                    break;
                case CC_EXPR_ADDR: {
                    if(vc == CC_CONSTEXPR_VALUE)
                        return cc_error(p, tok.loc, "address-of in constant expression");
                    if(operand->kind == CC_EXPR_COMPOUND_LITERAL){
                        err = cc_desugar_compound_literal(p, operand, &operand);
                        if(err) return err;
                    }
                    if((operand->kind == CC_EXPR_DOT || operand->kind == CC_EXPR_ARROW) && operand->field_loc.bit_width)
                        return cc_error(p, tok.loc, "cannot take address of bitfield");
                    // &func is the same as function-to-pointer decay: just cast.
                    if(ccqt_kind(operand->type) == CC_FUNCTION){
                        err = cc_pointer_of(p, operand->type, &result_type);
                        if(err) return err;
                        return cc_implicit_cast(p, operand, result_type, out);
                    }
                    if(!operand->is_lvalue)
                        return cc_error(p, tok.loc, "cannot take address of rvalue");
                    if(vc == CC_LINKTIME_VALUE && operand->kind == CC_EXPR_VARIABLE && operand->var->automatic)
                        return cc_error(p, tok.loc, "address of automatic variable in constant expression");
                    err = cc_pointer_of(p, operand->type, &result_type);
                    if(err) return err;
                    break;
                }
                default: // PREINC, PREDEC
                    if(operand->kind == CC_EXPR_COMPOUND_LITERAL){
                        err = cc_desugar_compound_literal(p, operand, &operand);
                        if(err) return err;
                    }
                    if(!operand->is_lvalue)
                        return cc_error(p, tok.loc, "expression is not an lvalue");
                    if(operand->type.is_const)
                        return cc_error(p, tok.loc, "cannot modify const-qualified variable");
                    {
                        CcTypeKind tk = ccqt_kind(operand->type);
                        if(tk != CC_POINTER && tk != CC_BASIC && tk != CC_ENUM)
                            return cc_error(p, tok.loc, "increment/decrement requires arithmetic or pointer type");
                    }
                    result_type = operand->type;
                    break;
            }
            CcExpr* node = cc_make_expr(p, kind, tok.loc, result_type, 0);
            if(!node) return CC_OOM_ERROR;
            node->is_lvalue = kind == CC_EXPR_DEREF;
            node->lhs = operand;
            *out = node;
            return 0;
        }
    }
    // Not a prefix op: put it back and parse primary + postfix
    cc_unget(p, &tok);
    CcExpr* primary;
    err = cc_parse_primary(p, vc, &primary);
    if(err) return err;
    return cc_parse_postfix(p, vc, primary, out);
}

// Primary expressions: literals, identifiers, parenthesized
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
                    node->double_ = tok.constant.double_value;
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
            // Type: char[N] (array of char, length includes null terminator)
            CcArray* sa = cc_intern_array(&p->type_cache, cc_allocator(p), ccqt_basic(CCBT_char), tok.str.length, 0, 0, 0, 0);
            if(!sa) return CC_OOM_ERROR;
            node->type = (CcQualType){.bits = (uintptr_t)sa};
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
                    err = cc_parse_assignment_expr(p, vc, &arg);
                    if(err) return err;
                    err = cc_expect_punct(p, CC_rparen);
                    if(err) return err;
                    CcEvalResult ev;
                    err = cc_eval_expr(p,arg,&ev);
                    cc_release_expr(p, arg);
                    CcExpr* node = cc_value_expr(p, tok.loc, ccqt_basic(CCBT_int));
                    if(!node) return CC_OOM_ERROR;
                    node->integer = err ? 0 : 1;
                    *out = node;
                    return 0;
                }
                case CC__builtin_offsetof:{
                    // __builtin_offsetof(type, member-designator)
                    // member-designator: ident ( '.' ident | '[' expr ']' )*
                    err = cc_expect_punct(p, CC_lparen);
                    if(err) return err;
                    CcQualType type;
                    err = cc_parse_type_name(p, &type);
                    if(err) return err;
                    err = cc_expect_punct(p, CC_comma);
                    if(err) return err;
                    uint64_t offset = 0;
                    CcQualType cur = type;
                    // First element must be an identifier
                    for(;;){
                        CcToken member;
                        err = cc_next_token(p, &member);
                        if(err) return err;
                        if(member.type != CC_IDENTIFIER)
                            return cc_error(p, member.loc, "expected member name in __builtin_offsetof");
                        CcTypeKind tk = ccqt_kind(cur);
                        CcFieldLoc floc = {0};
                        CcQualType member_type = {0};
                        _Bool found = 0;
                        if(tk == CC_STRUCT){
                            CcStruct* s = ccqt_as_struct(cur);
                            found = cc_lookup_field(s->fields, s->field_count, member.ident.ident, &floc, &member_type, NULL);
                        }
                        else if(tk == CC_UNION){
                            CcUnion* u = ccqt_as_union(cur);
                            found = cc_lookup_field(u->fields, u->field_count, member.ident.ident, &floc, &member_type, NULL);
                        }
                        if(!found)
                            return cc_error(p, member.loc, "no member named '%s' in type", member.ident.ident->data);
                        offset += floc.byte_offset;
                        cur = member_type;
                        // Handle subscripts: [expr][expr]...
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
                            err = cc_parse_assignment_expr(p, vc, &idx_expr);
                            if(err) return err;
                            CcEvalResult idx_val;
                            err = cc_eval_expr(p,idx_expr,&idx_val);
                            cc_release_expr(p, idx_expr);
                            if(err)
                                return cc_error(p, next.loc, "array index in __builtin_offsetof must be a constant integer");
                            int64_t idx;
                            switch(idx_val.kind){
                                DEFAULT_UNREACHABLE;
                                case CC_EVAL_INT:    idx = idx_val.i; break;
                                case CC_EVAL_UINT:   idx = (int64_t)idx_val.u; break;
                                case CC_EVAL_TYPE: case CC_EVAL_VOID: case CC_EVAL_INIT_LIST: case CC_EVAL_STRING:
                                case CC_EVAL_FLOAT:
                                case CC_EVAL_DOUBLE:
                                return cc_error(p, next.loc, "array index in __builtin_offsetof must be a constant integer");
                            }
                            uint32_t elem_size;
                            err = cc_sizeof_as_uint(p, arr->element, next.loc, &elem_size);
                            if(err) return err;
                            offset += (uint64_t)idx * elem_size;
                            cur = arr->element;
                            err = cc_expect_punct(p, CC_rbracket);
                            if(err) return err;
                        }
                        // Check for '.' continuation
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
                    // __atomic_load(ptr, ret, memorder)
                    err = cc_expect_punct(p, '(');
                    if(err) return err;
                    CcExpr* ptr_expr;
                    err = cc_parse_assignment_expr(p, vc, &ptr_expr);
                    if(err) return err;
                    CcQualType ptr_type = ptr_expr->type;
                    if(ccqt_kind(ptr_type) != CC_POINTER)
                        return cc_error(p, tok.loc, "first argument to __atomic_load must be a pointer");
                    CcQualType pointee = ccqt_as_ptr(ptr_type)->pointee;
                    err = cc_expect_punct(p, ',');
                    if(err) return err;
                    CcExpr* ret_expr;
                    err = cc_parse_assignment_expr(p, vc, &ret_expr);
                    if(err) return err;
                    err = cc_expect_punct(p, ',');
                    if(err) return err;
                    // memorder (discard)
                    CcExpr* mo;
                    err = cc_parse_assignment_expr(p, vc, &mo);
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
                    // __atomic_store(ptr, val_ptr, memorder)
                    err = cc_expect_punct(p, '(');
                    if(err) return err;
                    CcExpr* ptr_expr;
                    err = cc_parse_assignment_expr(p, vc, &ptr_expr);
                    if(err) return err;
                    if(ccqt_kind(ptr_expr->type) != CC_POINTER)
                        return cc_error(p, tok.loc, "first argument to __atomic_store must be a pointer");
                    err = cc_expect_punct(p, ',');
                    if(err) return err;
                    CcExpr* val_expr;
                    err = cc_parse_assignment_expr(p, vc, &val_expr);
                    if(err) return err;
                    err = cc_expect_punct(p, ',');
                    if(err) return err;
                    CcExpr* mo;
                    err = cc_parse_assignment_expr(p, vc, &mo);
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
                    // __atomic_exchange(ptr, val_ptr, ret_ptr, memorder)
                    err = cc_expect_punct(p, '(');
                    if(err) return err;
                    CcExpr* ptr_expr;
                    err = cc_parse_assignment_expr(p, vc, &ptr_expr);
                    if(err) return err;
                    if(ccqt_kind(ptr_expr->type) != CC_POINTER)
                        return cc_error(p, tok.loc, "first argument to __atomic_exchange must be a pointer");
                    err = cc_expect_punct(p, ',');
                    if(err) return err;
                    CcExpr* val_expr;
                    err = cc_parse_assignment_expr(p, vc, &val_expr);
                    if(err) return err;
                    err = cc_expect_punct(p, ',');
                    if(err) return err;
                    CcExpr* ret_expr;
                    err = cc_parse_assignment_expr(p, vc, &ret_expr);
                    if(err) return err;
                    err = cc_expect_punct(p, ',');
                    if(err) return err;
                    CcExpr* mo;
                    err = cc_parse_assignment_expr(p, vc, &mo);
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
                case CC__atomic_compare_exchange:
                case CC__atomic_fetch_add:
                case CC__atomic_fetch_sub:
                case CC__atomic_load_n:
                case CC__atomic_store_n:
                case CC__atomic_exchange_n:
                case CC__atomic_compare_exchange_n: {
                    enum CcAtomicOp op;
                    switch(builtin){
                        case CC__atomic_fetch_add: op = CC_ATOMIC_FETCH_ADD; break;
                        case CC__atomic_fetch_sub: op = CC_ATOMIC_FETCH_SUB; break;
                        case CC__atomic_load_n:    op = CC_ATOMIC_LOAD_N;    break;
                        case CC__atomic_store_n:   op = CC_ATOMIC_STORE_N;   break;
                        case CC__atomic_exchange_n:op = CC_ATOMIC_EXCHANGE_N; break;
                        case CC__atomic_compare_exchange_n: op = CC_ATOMIC_COMPARE_EXCHANGE_N; break;
                        case CC__atomic_compare_exchange:   op = CC_ATOMIC_COMPARE_EXCHANGE;   break;
                        default: return cc_unreachable(p, tok.loc, "compiler broken??");
                    }
                    err = cc_expect_punct(p, '(');
                    if(err) return err;
                    // First arg: pointer
                    CcExpr* ptr_expr;
                    err = cc_parse_assignment_expr(p, vc, &ptr_expr);
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
                    if((op == CC_ATOMIC_FETCH_ADD || op == CC_ATOMIC_FETCH_SUB) && pointee_sz > 8)
                        return cc_error(p, tok.loc, "atomic arithmetic not supported for operand size %u", pointee_sz);
                    // Determine result type and number of runtime value args.
                    CcQualType result_type;
                    CcQualType arg_types[2]; // at most 2 runtime args (expected, desired)
                    int nargs; // number of runtime value args (excludes memorder/weak)
                    int nconst; // number of trailing constant args (memorder, weak, etc.)
                    switch(op){
                        case CC_ATOMIC_LOAD_N:
                            // __atomic_load_n(ptr, memorder)
                            result_type = pointee;
                            nargs = 0;
                            nconst = 1;
                            break;
                        case CC_ATOMIC_STORE_N:
                            // __atomic_store_n(ptr, val, memorder)
                            result_type = ccqt_basic(CCBT_void);
                            arg_types[0] = pointee;
                            nargs = 1;
                            nconst = 1;
                            break;
                        case CC_ATOMIC_FETCH_ADD:
                        case CC_ATOMIC_FETCH_SUB:
                        case CC_ATOMIC_EXCHANGE_N:
                            // (ptr, val, memorder)
                            result_type = pointee;
                            arg_types[0] = pointee;
                            nargs = 1;
                            nconst = 1;
                            break;
                        case CC_ATOMIC_COMPARE_EXCHANGE_N:
                            // (ptr, expected, desired, weak, success_order, fail_order)
                            result_type = ccqt_basic(CCBT_bool);
                            arg_types[0] = ptr_type; // expected is same pointer type
                            arg_types[1] = pointee;  // desired is value
                            nargs = 2;
                            nconst = 3;
                            break;
                        case CC_ATOMIC_COMPARE_EXCHANGE:
                            // (ptr, expected, desired_ptr, weak, success_order, fail_order)
                            result_type = ccqt_basic(CCBT_bool);
                            arg_types[0] = ptr_type; // expected is same pointer type
                            arg_types[1] = ptr_type; // desired is pointer
                            nargs = 2;
                            nconst = 3;
                            break;
                        case CC_ATOMIC_LOAD:
                        case CC_ATOMIC_STORE:
                        case CC_ATOMIC_EXCHANGE:
                        case CC_ATOMIC_THREAD_FENCE:
                        case CC_ATOMIC_SIGNAL_FENCE:
                            return CC_UNREACHABLE_ERROR;
                    }
                    CcExpr* node = cc_make_expr(p, CC_EXPR_ATOMIC, tok.loc, result_type, nargs);
                    if(!node) return CC_OOM_ERROR;
                    node->atomic.op = op;
                    node->lhs = ptr_expr;
                    // Parse runtime value args.
                    for(int i = 0; i < nargs; i++){
                        err = cc_expect_punct(p, ',');
                        if(err) return err;
                        err = cc_parse_assignment_expr(p, vc, &node->values[i]);
                        if(err) return err;
                        err = cc_implicit_cast(p, node->values[i], arg_types[i], &node->values[i]);
                        if(err) return err;
                    }
                    // Parse constant args (weak, memorders).
                    unsigned const_vals[3];
                    for(int i = 0; i < nconst; i++){
                        err = cc_expect_punct(p, ',');
                        if(err) return err;
                        CcExpr* const_expr;
                        err = cc_parse_assignment_expr(p, vc, &const_expr);
                        if(err) return err;
                        CcEvalResult ev;
                        err = cc_eval_expr(p,const_expr,&ev);
                        if(err)
                            return cc_error(p, const_expr->loc, "memory order must be a constant expression");
                        const_vals[i] = (unsigned)ev.i;
                        if(const_vals[i] >= CC_MO_COUNT)
                            return cc_error(p, const_expr->loc, "invalid memory order value %u", const_vals[i]);
                        cc_release_expr(p, const_expr);
                    }
                    if(op == CC_ATOMIC_COMPARE_EXCHANGE){
                        node->atomic.weak = const_vals[0];
                        node->atomic.memorder = const_vals[1];
                        node->atomic.fail_memorder = const_vals[2];
                    }
                    else {
                        node->atomic.memorder = const_vals[0];
                    }
                    err = cc_expect_punct(p, ')');
                    if(err) return err;
                    *out = node;
                    return 0;
                }
                case CC__atomic_thread_fence:
                case CC__atomic_signal_fence: {
                    enum CcAtomicOp op = (builtin == CC__atomic_thread_fence) ? CC_ATOMIC_THREAD_FENCE : CC_ATOMIC_SIGNAL_FENCE;
                    err = cc_expect_punct(p, '(');
                    if(err) return err;
                    CcExpr* const_expr;
                    err = cc_parse_assignment_expr(p, vc, &const_expr);
                    if(err) return err;
                    CcEvalResult ev;
                    err = cc_eval_expr(p,const_expr,&ev);
                    if(err)
                        return cc_error(p, const_expr->loc, "memory order must be a constant expression");
                    unsigned order = (unsigned)ev.i;
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
                case CC__builtin_va_start: {
                    // (ap, param) or (ap)
                    err = cc_expect_punct(p, '(');
                    if(err) return err;
                    CcExpr* ap_expr;
                    err = cc_parse_assignment_expr(p, vc, &ap_expr);
                    if(err) return err;
                    err = cc_va_list_to_ptr(p, tok.loc, &ap_expr);
                    if(err) return err;
                    // Optional second arg (last fixed param) — parse and ignore.
                    CcToken peek;
                    err = cc_peek(p, &peek);
                    if(err) return err;
                    if(peek.type == CC_PUNCTUATOR && peek.punct.punct == ','){
                        err = cc_next_token(p, &peek); // consume comma
                        if(err) return err;
                        CcExpr* ignored;
                        err = cc_parse_assignment_expr(p, vc, &ignored);
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
                    // (ap)
                    err = cc_expect_punct(p, '(');
                    if(err) return err;
                    CcExpr* ap_expr;
                    err = cc_parse_assignment_expr(p, vc, &ap_expr);
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
                    // (ap, type)
                    err = cc_expect_punct(p, '(');
                    if(err) return err;
                    CcExpr* ap_expr;
                    err = cc_parse_assignment_expr(p, vc, &ap_expr);
                    if(err) return err;
                    err = cc_va_list_to_ptr(p, tok.loc, &ap_expr);
                    if(err) return err;
                    err = cc_expect_punct(p, ',');
                    if(err) return err;
                    CcQualType arg_type;
                    err = cc_parse_type_name(p, &arg_type);
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
                    // (dest, src)
                    err = cc_expect_punct(p, '(');
                    if(err) return err;
                    CcExpr* dest_expr;
                    err = cc_parse_assignment_expr(p, vc, &dest_expr);
                    if(err) return err;
                    err = cc_va_list_to_ptr(p, tok.loc, &dest_expr);
                    if(err) return err;
                    err = cc_expect_punct(p, ',');
                    if(err) return err;
                    CcExpr* src_expr;
                    err = cc_parse_assignment_expr(p, vc, &src_expr);
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
                    err = cc_parse_assignment_expr(p, vc, out);
                    if(err) return err;
                    err = cc_expect_punct(p, ',');
                    if(err) return err;
                    CcExpr* unused;
                    err = cc_parse_assignment_expr(p, vc, &unused);
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
                        default: return CC_UNREACHABLE_ERROR;
                    }
                    CcExpr* node = cc_make_expr(p, CC_EXPR_BUILTIN, tok.loc, ccqt_basic(CCBT_void), 0);
                    if(!node) return CC_OOM_ERROR;
                    node->builtin.op = op;
                    *out = node;
                    return 0;
                }
                case CC__builtin_sub_overflow:
                case CC__builtin_mul_overflow:
                case CC__builtin_add_overflow:{
                    // (a, b, &result) -> bool
                    err = cc_expect_punct(p, '(');
                    if(err) return err;
                    CcExpr* a;
                    err = cc_parse_assignment_expr(p, vc, &a);
                    if(err) return err;
                    if(!ccqt_is_basic(a->type) || !ccbt_is_integer(a->type.basic.kind))
                        return cc_error(p, a->loc, "first argument to overflow builtin must be an integer type");
                    err = cc_expect_punct(p, ',');
                    if(err) return err;
                    CcExpr* b;
                    err = cc_parse_assignment_expr(p, vc, &b);
                    if(err) return err;
                    if(!ccqt_is_basic(b->type) || !ccbt_is_integer(b->type.basic.kind))
                        return cc_error(p, b->loc, "second argument to overflow builtin must be an integer type");
                    err = cc_expect_punct(p, ',');
                    if(err) return err;
                    CcExpr* res;
                    err = cc_parse_assignment_expr(p, vc, &res);
                    if(err) return err;
                    if(ccqt_kind(res->type) != CC_POINTER
                    || !ccqt_is_basic(ccqt_as_ptr(res->type)->pointee)
                    || !ccbt_is_integer(ccqt_as_ptr(res->type)->pointee.basic.kind))
                        return cc_error(p, res->loc, "third argument to overflow builtin must be a pointer to integer type");
                    err = cc_expect_punct(p, ')');
                    if(err) return err;
                    CcExprKind kind;
                    switch(builtin){
                        case CC__builtin_add_overflow: kind = CC_EXPR_ADD_OVERFLOW; break;
                        case CC__builtin_sub_overflow: kind = CC_EXPR_SUB_OVERFLOW; break;
                        case CC__builtin_mul_overflow: kind = CC_EXPR_MUL_OVERFLOW; break;
                        default: return CC_UNREACHABLE_ERROR;
                    }
                    CcExpr* node = cc_make_expr(p, kind, tok.loc, ccqt_basic(CCBT_bool), 2);
                    if(!node) return CC_OOM_ERROR;
                    node->lhs = a;
                    node->values[0] = b;
                    node->values[1] = res;
                    *out = node;
                    return 0;
                }
                case CC__builtin_popcount:
                case CC__builtin_popcountl:
                case CC__builtin_popcountll:{
                    err = cc_expect_punct(p, '(');
                    if(err) return err;
                    CcExpr* arg;
                    err = cc_parse_assignment_expr(p, vc, &arg);
                    if(err) return err;
                    if(!ccqt_is_basic(arg->type) || !ccbt_is_integer(arg->type.basic.kind))
                        return cc_error(p, arg->loc, "argument to popcount must be an integer type");
                    err = cc_expect_punct(p, ')');
                    if(err) return err;
                    CcExpr* node = cc_make_expr(p, CC_EXPR_POPCOUNT, tok.loc, ccqt_basic(CCBT_int), 0);
                    if(!node) return CC_OOM_ERROR;
                    node->lhs = arg;
                    *out = node;
                    return 0;
                }
                case CC__builtin_ctz:
                case CC__builtin_ctzl:
                case CC__builtin_ctzll:
                case CC__builtin_clz:
                case CC__builtin_clzl:
                case CC__builtin_clzll:{
                    err = cc_expect_punct(p, '(');
                    if(err) return err;
                    CcExpr* arg;
                    err = cc_parse_assignment_expr(p, vc, &arg);
                    if(err) return err;
                    if(!ccqt_is_basic(arg->type) || !ccbt_is_integer(arg->type.basic.kind))
                        return cc_error(p, arg->loc, "argument to ctz/clz must be an integer type");
                    err = cc_expect_punct(p, ')');
                    if(err) return err;
                    CcExprKind kind = (builtin == CC__builtin_ctz
                                    || builtin == CC__builtin_ctzl
                                    || builtin == CC__builtin_ctzll)
                                    ? CC_EXPR_CTZ : CC_EXPR_CLZ;
                    CcExpr* node = cc_make_expr(p, kind, tok.loc, ccqt_basic(CCBT_int), 0);
                    if(!node) return CC_OOM_ERROR;
                    node->lhs = arg;
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
                        node->float_ = (float)(1.0/0.0);
                    }
                    else if(builtin == CC__builtin_huge_val){
                        node = cc_make_expr(p, CC_EXPR_VALUE, tok.loc, ccqt_basic(CCBT_double), 0);
                        if(!node) return CC_OOM_ERROR;
                        node->double_ = 1.0/0.0;
                    }
                    else {
                        node = cc_make_expr(p, CC_EXPR_VALUE, tok.loc, ccqt_basic(CCBT_long_double), 0);
                        if(!node) return CC_OOM_ERROR;
                        node->double_ = 1.0/0.0;
                    }
                    *out = node;
                    return 0;
                }
                case CC__builtin_alloca: {
                    err = cc_expect_punct(p, '(');
                    if(err) return err;
                    CcExpr* sz;
                    err = cc_parse_assignment_expr(p, vc, &sz);
                    if(err) return err;
                    err = cc_expect_punct(p, ')');
                    if(err) return err;
                    CcExpr* node = cc_make_expr(p, CC_EXPR_ALLOCA, tok.loc, p->void_star, 0);
                    if(!node) return CC_OOM_ERROR;
                    node->lhs = sz;
                    *out = node;
                    return 0;
                }
                case CC__builtin_intern: {
                    err = cc_expect_punct(p, '(');
                    if(err) return err;
                    CcExpr* arg;
                    err = cc_parse_assignment_expr(p, vc, &arg);
                    if(err) return err;
                    if(!cc_implicit_convertible(arg->type, p->const_char_star))
                        return cc_error(p, arg->loc, "__builtin_intern argument must be a char pointer");
                    err = cc_expect_punct(p, ')');
                    if(err) return err;
                    CcExpr* node = cc_make_expr(p, CC_EXPR_INTERN, tok.loc, p->const_char_star, 0);
                    if(!node) return CC_OOM_ERROR;
                    node->lhs = arg;
                    *out = node;
                    return 0;
                }
                case CC__builtin_nanf:
                case CC__builtin_nan:
                case CC__nan:{
                    // nanf("") / nan("") — ignore the string arg
                    err = cc_expect_punct(p, '(');
                    if(err) return err;
                    CcExpr* arg;
                    err = cc_parse_assignment_expr(p, vc, &arg);
                    if(err) return err;
                    cc_release_expr(p, arg);
                    err = cc_expect_punct(p, ')');
                    if(err) return err;
                    CcExpr* node;
                    if(builtin == CC__builtin_nanf){
                        node = cc_make_expr(p, CC_EXPR_VALUE, tok.loc, ccqt_basic(CCBT_float), 0);
                        if(!node) return CC_OOM_ERROR;
                        node->float_ = (float)(0.0/0.0);
                    }
                    else {
                        node = cc_make_expr(p, CC_EXPR_VALUE, tok.loc, ccqt_basic(CCBT_double), 0);
                        if(!node) return CC_OOM_ERROR;
                        node->double_ = 0.0/0.0;
                    }
                    *out = node;
                    return 0;
                }
            }
            CcSymbol sym;
            if(!cc_scope_lookup_symbol(p->current, tok.ident.ident, CC_SCOPE_WALK_CHAIN, &sym)){
                return cc_error(p, tok.loc, "undeclared identifier '%.*s'",
                    tok.ident.ident->length, tok.ident.ident->data);
            }
            switch(sym.kind){
                case CC_SYM_VAR: {
                    if(vc == CC_CONSTEXPR_VALUE && !sym.var->constexpr_)
                        return cc_error(p, tok.loc, "expression is not a constant expression");
                    if(vc == CC_LINKTIME_VALUE && sym.var->automatic)
                        return cc_error(p, tok.loc, "expression is not a constant expression");
                    CcExpr* node = cc_make_expr(p, CC_EXPR_VARIABLE, tok.loc, sym.var->type, 0);
                    if(!node) return CC_OOM_ERROR;
                    node->is_lvalue = 1;
                    node->var = sym.var;
                    if(!sym.var->automatic){
                        err = PM_put(&p->used_vars, cc_allocator(p), sym.var, sym.var);
                        if(err) return CC_OOM_ERROR;
                    }
                    *out = node;
                    return 0;
                }
                case CC_SYM_FUNC: {
                    CcExpr* node = cc_make_expr(p, CC_EXPR_FUNCTION, tok.loc, (CcQualType){.bits = (uintptr_t)sym.func->type}, 0);
                    if(!node) return CC_OOM_ERROR;
                    node->func = sym.func;
                    {
                        err = PM_put(&p->used_funcs, cc_allocator(p), sym.func, sym.func);
                        if(err) return CC_OOM_ERROR;
                    }
                    *out = node;
                    return 0;
                }
                case CC_SYM_ENUMERATOR: {
                    CcExpr* node = cc_make_expr(p, CC_EXPR_VALUE, tok.loc, sym.enumerator->type, 0);
                    if(!node) return CC_OOM_ERROR;
                    node->integer = sym.enumerator->value;
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
                CcExpr* inner;
                err = cc_parse_expr(p, vc, &inner);
                if(err) return err;
                err = cc_expect_punct(p, CC_rparen);
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
                    // Could be sizeof(type) or sizeof(expr)
                    err = cc_next_token(p, &peek); // consume '('
                    if(err) return err;
                    CcToken peek2;
                    err = cc_peek(p, &peek2);
                    if(err) return err;
                    if(cc_is_type_start(p, &peek2)){
                        CcQualType type;
                        err = cc_parse_type_name(p, &type);
                        if(err) return err;
                        err = cc_expect_punct(p, CC_rparen);
                        if(err) return err;
                        // Check for compound literal: sizeof(type){...}
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
                    // sizeof(expr) — put '(' back, parse as unary
                    err = cc_unget(p, &peek);
                    if(err) return err;
                }
                // sizeof unary-expression
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
                    // Could be alignof(type) or alignof(expr)
                    err = cc_next_token(p, &peek); // consume '('
                    if(err) return err;
                    CcToken peek2;
                    err = cc_peek(p, &peek2);
                    if(err) return err;
                    if(cc_is_type_start(p, &peek2)){
                        CcQualType type;
                        err = cc_parse_type_name(p, &type);
                        if(err) return err;
                        err = cc_expect_punct(p, CC_rparen);
                        if(err) return err;
                        CcExpr* al;
                        err = cc_alignof_as_expr(p, type, tok.loc, &al);
                        if(err) return err;
                        *out = al;
                        return 0;
                    }
                    // alignof(expr) — put '(' back, parse as unary
                    err = cc_unget(p, &peek);
                    if(err) return err;
                }
                // alignof unary-expression (extension)
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
                err = cc_expect_punct(p, CC_lparen);
                if(err) return err;
                CcToken peek;
                err = cc_peek(p, &peek);
                if(err) return err;
                CcQualType arr_type;
                if(cc_is_type_start(p, &peek)){
                    err = cc_parse_type_name(p, &arr_type);
                    if(err) return err;
                }
                else {
                    CcExpr* expr = NULL;
                    err = cc_parse_expr(p, CC_RUNTIME_VALUE, &expr);
                    if(err) return err;
                    arr_type = expr->type;
                    cc_release_expr(p, expr);
                }
                err = cc_expect_punct(p, CC_rparen);
                if(err) return err;
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
            default:
                if(cc_is_type_start(p, &tok)){
                    err = cc_unget(p, &tok);
                    if(err) return err;
                    return cc_parse_lambda(p, vc, tok.loc, out);
                }
                return cc_error(p, tok.loc, "Unexpected keyword in expression");
            }
        case CC_EOF:
            return cc_error(p, tok.loc, "Unexpected end of input in expression");
    }
    return cc_error(p, tok.loc, "Unexpected token in expression");
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
        err = cc_parse_type_name(p, &tswitch);
        if(err) return err;
    }
    else {
        CcExpr* condition;
        err = cc_parse_assignment_expr(p, vc, &condition);
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
        case CC_STRUCT:
        case CC_UNION:
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
            err = cc_parse_type_name(p, &assoc_type);
            if(err) return err;
            is_match = (assoc_type.bits == tswitch.bits);
        }
        err = cc_expect_punct(p, ':');
        if(err) return err;
        if(is_match && !result){
            err = cc_parse_assignment_expr(p, vc, &result);
            if(err) return err;
        }
        else if(is_default){
            err = cc_parse_assignment_expr(p, vc, &default_result);
            if(err) return err;
        }
        else {
            // Skip non-selected association expression (bag of tokens).
            int depth = 0;
            for(;;){
                err = cc_next_token(p, &tok);
                if(err) return err;
                if(tok.type == CC_EOF)
                    return cc_error(p, tok.loc, "unterminated _Generic");
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
                    default: break;
                }
            }
            done_skip:;
        }
        // Expect ',' or ')'
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

// Postfix operators
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
        // Compound literal used as lvalue in postfix context: desugar to anonymous variable.
        // Only desugar for actual postfix operators that need an lvalue, not for
        // tokens like ',' or '}' that just terminate the expression.
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
                {
                    CcTypeKind tk = ccqt_kind(operand->type);
                    if(tk != CC_POINTER && tk != CC_BASIC && tk != CC_ENUM)
                        return cc_error(p, tok.loc, "increment/decrement requires arithmetic or pointer type");
                }
                CcExpr* node = cc_make_expr(p, CC_EXPR_POSTINC, tok.loc, operand->type, 0);
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
                {
                    CcTypeKind tk = ccqt_kind(operand->type);
                    if(tk != CC_POINTER && tk != CC_BASIC && tk != CC_ENUM)
                        return cc_error(p, tok.loc, "increment/decrement requires arithmetic or pointer type");
                }
                CcExpr* node = cc_make_expr(p, CC_EXPR_POSTDEC, tok.loc, operand->type, 0);
                if(!node) return CC_OOM_ERROR;
                node->lhs = operand;
                operand = node;
                continue;
            }
            case CC_lbracket: {
                // subscript: operand[expr]
                CcExpr* index;
                err = cc_parse_expr(p, vc, &index);
                if(err) return err;
                err = cc_expect_punct(p, CC_rbracket);
                if(err) return err;
                {
                    CcQualType idx_type = index->type;
                    if(!ccqt_is_basic(idx_type) && ccqt_kind(idx_type) == CC_ENUM)
                        idx_type = ccqt_as_enum(idx_type)->underlying;
                    _Bool idx_int = ccqt_is_basic(idx_type) && ccbt_is_integer(idx_type.basic.kind);
                    _Bool idx_ptr = ccqt_is_pointer_like(idx_type);
                    if(!idx_int && !idx_ptr)
                        return cc_error(p, tok.loc, "array subscript requires integer or pointer type");
                }
                CcQualType elem_type;
                err = cc_deref_type(p, operand->type, &elem_type, tok.loc);
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
                // Resolve the aggregate type, allowing . and -> interchangeably.
                CcQualType agg_type = operand->type;
                if(ccqt_kind(agg_type) == CC_POINTER){
                    CcPointer* ptr = ccqt_as_ptr(agg_type);
                    agg_type = ptr->pointee;
                    mkind = CC_EXPR_ARROW;
                }
                else {
                    mkind = CC_EXPR_DOT;
                }
                CcFieldLoc floc = {0};
                CcQualType member_type = {0};
                CcFunc* _Null_unspecified method = NULL;
                CcTypeKind tk = ccqt_kind(agg_type);
                if(tk == CC_STRUCT){
                    CcStruct* s = ccqt_as_struct(agg_type);
                    cc_lookup_field(s->fields, s->field_count, member_name, &floc, &member_type, &method);
                }
                else if(tk == CC_UNION){
                    CcUnion* u = ccqt_as_union(agg_type);
                    cc_lookup_field(u->fields, u->field_count, member_name, &floc, &member_type, &method);
                }
                else if(tk == CC_BASIC && agg_type.basic.kind == CCBT__Type){
                    CcQualType result_type;
                    _Bool is_method = 0; // methods take a (type-or-expr) argument
                    CcTypeIntrospectionOp ti_op = (CcTypeIntrospectionOp)(uintptr_t)AM_get(&p->type_intro, member_name);
                    switch(ti_op){
                    case CC_TYPE_NONE:
                        goto fucs;
                    case CC_TYPE_PUSH_METHOD: {
                        if(p->current != &p->global)
                            return cc_error(p, member.loc, "push method only allowed at global scope");
                        // (type).push_method(func_name) — compile-time mutation
                        CcEvalResult tv;
                        err = cc_eval_expr(p, operand, &tv);
                        if(err || tv.kind != CC_EVAL_TYPE)
                            return cc_error(p, member.loc, "push_method requires a constant type");
                        CcQualType qt = tv.type;
                        CcTypeKind k = ccqt_kind(qt);
                        if(k != CC_STRUCT && k != CC_UNION)
                            return cc_error(p, member.loc, "push_method requires a struct or union type");
                        err = cc_expect_punct(p, '(');
                        if(err) return err;
                        // First arg: method name
                        CcToken mname_tok;
                        err = cc_next_token(p, &mname_tok);
                        if(err) return err;
                        if(mname_tok.type != CC_IDENTIFIER)
                            return cc_error(p, mname_tok.loc, "push_method: expected method name");
                        Atom method_name = mname_tok.ident.ident;
                        err = cc_expect_punct(p, CC_comma);
                        if(err) return err;
                        // Second arg: function (name or lambda)
                        CcExpr* func_expr;
                        err = cc_parse_assignment_expr(p, vc, &func_expr);
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
                    case CC_TYPE_ENUMERATOR:
                        result_type = p->builtin_enumerator;
                        is_method = 1;
                        break;
                    case CC_TYPE_ENUMERATORS:
                    case CC_TYPE_FIELDS:
                        result_type = ccqt_basic(cc_target(p)->size_type);
                        break;
                    case CC_TYPE_NAME:
                    case CC_TYPE_TAG:
                        result_type = p->const_char_star;
                        break;
                    case CC_TYPE_IS_INTEGER:
                    case CC_TYPE_IS_FLOAT:
                    case CC_TYPE_IS_ARITHMETIC:
                    case CC_TYPE_IS_POINTER:
                    case CC_TYPE_IS_STRUCT:
                    case CC_TYPE_IS_UNION:
                    case CC_TYPE_IS_ARRAY:
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
                    case CC_TYPE_CASTABLE_TO:
                        result_type = ccqt_basic(CCBT_bool);
                        is_method = 1;
                        break;
                    }
                    if(is_method){
                        err = cc_expect_punct(p, '(');
                        if(err) return err;
                        CcExpr* arg_val;
                        if(ti_op == CC_TYPE_FIELD || ti_op == CC_TYPE_PARAM_TYPE || ti_op == CC_TYPE_ENUMERATOR){
                            // Parse expression argument (index).
                            CcExpr* arg_expr;
                            err = cc_parse_assignment_expr(p, vc, &arg_expr);
                            if(err) return err;
                            CcQualType size_type = ccqt_basic(cc_target(p)->size_type);
                            err = cc_implicit_cast(p, arg_expr, size_type, &arg_expr);
                            if(err) return err;
                            arg_val = arg_expr;
                        }
                        else {
                            // Parse type-or-expression argument.
                            CcQualType arg_type;
                            CcToken peek2;
                            err = cc_peek(p, &peek2);
                            if(err) return err;
                            if(cc_is_type_start(p, &peek2)){
                                err = cc_parse_type_name(p, &arg_type);
                                if(err) return err;
                            }
                            else {
                                CcExpr* arg_expr;
                                err = cc_parse_assignment_expr(p, vc, &arg_expr);
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
                else {
                    // Not a struct/union — fall through to FUCS lookup.
                }
                fucs:;
                if(!member_type.bits){
                    // FUCS: x.foo(args) -> foo(x, args)
                    // Look up member_name as a free function.
                    CcFunc* ufcs_func = cc_scope_lookup_func(p->current, member_name, CC_SCOPE_WALK_CHAIN);
                    if(!ufcs_func){
                        if(tk == CC_STRUCT || tk == CC_UNION)
                            return cc_error(p, member.loc, "no member named '%s'", member_name->data);
                        return cc_error(p, member.loc, "not a struct or union");
                    }
                    CcExpr* fnode = cc_make_expr(p, CC_EXPR_FUNCTION, tok.loc, (CcQualType){.bits = (uintptr_t)ufcs_func->type}, 0);
                    if(!fnode) return CC_OOM_ERROR;
                    fnode->func = ufcs_func;
                    err = PM_put(&p->used_funcs, cc_allocator(p), ufcs_func, ufcs_func);
                    if(err) return CC_OOM_ERROR;
                    // Auto-& if the first param is a pointer and operand is a non-pointer lvalue.
                    if(ufcs_func->type->param_count > 0 && ccqt_kind(ufcs_func->type->params[0]) == CC_POINTER && ccqt_kind(operand->type) != CC_POINTER && operand->is_lvalue){
                        CcQualType addr_type;
                        err = cc_pointer_of(p, operand->type, &addr_type);
                        if(err) return err;
                        CcExpr* addr = cc_make_expr(p, CC_EXPR_ADDR, tok.loc, addr_type, 0);
                        if(!addr) return CC_OOM_ERROR;
                        addr->lhs = operand;
                        receiver = addr;
                    }
                    else if(ufcs_func->type->param_count > 0 && ccqt_kind(operand->type) == CC_POINTER && ccqt_kind(ufcs_func->type->params[0]) != CC_POINTER){
                        // Auto-deref if operand is a pointer but first param wants a value.
                        CcQualType deref_type;
                        err = cc_deref_type(p, operand->type, &deref_type, tok.loc);
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
                if(method){
                    CcExpr* mnode = cc_make_expr(p, CC_EXPR_FUNCTION, tok.loc, member_type, 0);
                    if(!mnode) return CC_OOM_ERROR;
                    mnode->func = method;
                    err = PM_put(&p->used_funcs, cc_allocator(p), method, method);
                    if(err) return CC_OOM_ERROR;
                    if(mkind == CC_EXPR_ARROW){
                        // Already a pointer
                        receiver = operand;
                    }
                    else {
                        // Take address of the object
                        CcQualType addr_type;
                        err = cc_pointer_of(p, operand->type, &addr_type);
                        if(err) return err;
                        CcExpr* addr = cc_make_expr(p, CC_EXPR_ADDR, tok.loc, addr_type, 0);
                        if(!addr) return CC_OOM_ERROR;
                        addr->lhs = operand;
                        receiver = addr;
                    }
                    operand = mnode;
                    continue;
                }
                CcExpr* mnode = cc_make_expr(p, mkind, tok.loc, member_type, 1);
                if(!mnode) return CC_OOM_ERROR;
                mnode->is_lvalue = mkind == CC_EXPR_ARROW || operand->is_lvalue;
                mnode->field_loc = floc;
                mnode->values[0] = operand;
                operand = mnode;
                continue;
            }
            case CC_lparen: {
                // Function call: operand(args...)
                if(vc > CC_RUNTIME_VALUE)
                    return cc_error(p, tok.loc, "function call in constant expression");
                // Count args by parsing into a temp buffer
                // First check for empty arg list
                CcToken peek;
                err = cc_next_token(p, &peek);
                if(err) return err;
                if(peek.type == CC_PUNCTUATOR && peek.punct.punct == CC_rparen && !receiver){
                    // No args
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
                    if(operand->kind != CC_EXPR_FUNCTION){
                        err = PM_put(&p->used_call_types, cc_allocator(p), ftype, ftype);
                        if(err) return CC_OOM_ERROR;
                    }
                    CcExpr* node = cc_make_expr(p, CC_EXPR_CALL, tok.loc, ftype->return_type, 0);
                    if(!node) return CC_OOM_ERROR;
                    node->lhs = operand;
                    operand = node;
                    continue;
                }
                _Bool rparen_consumed = 0;
                if(receiver && peek.type == CC_PUNCTUATOR && peek.punct.punct == CC_rparen)
                    rparen_consumed = 1;
                else
                    cc_unget(p, &peek);
                // Resolve function type early so we can look up param names
                CcQualType ct = operand->type;
                if(ccqt_kind(ct) == CC_POINTER)
                    ct = ccqt_as_ptr(ct)->pointee;
                if(ccqt_kind(ct) != CC_FUNCTION)
                    return cc_error(p, tok.loc, "Called object is not a function or function pointer");
                CcFunction* ftype = ccqt_as_function(ct);
                // Get param names if available (from CcFunc declaration)
                Atom*_Null_unspecified param_names = NULL;
                size_t param_names_count = 0;
                if(operand->kind == CC_EXPR_FUNCTION && operand->func->params.count){
                    param_names = operand->func->params.data;
                    param_names_count = operand->func->params.count;
                }
                Allocator call_al = cc_scratch_allocator(p);
                Parray args = {0};
                _Bool has_receiver = receiver != NULL;
                if(receiver){
                    err = pa_push(&args, call_al, receiver);
                    if(err){ return CC_OOM_ERROR; }
                    receiver = NULL;
                }
                _Bool has_named = 0;
                uint32_t positional_index = has_receiver ? 1 : 0;
                if(rparen_consumed) goto call_args_done;
                for(;;){
                    // Check for designated argument: .name = or [N] =
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
                        // Find param index by name
                        Atom name = name_tok.ident.ident;
                        uint32_t idx = UINT32_MAX;
                        for(size_t j = 0; j < param_names_count; j++){
                            if(param_names[j] == name){ idx = (uint32_t)j; break; }
                        }
                        if(idx == UINT32_MAX){
                            err = cc_error(p, name_tok.loc, "no parameter named '%.*s'", name->length, name->data);
                            goto call_cleanup;
                        }
                        // Parse the value
                        CcExpr* arg;
                        err = cc_parse_assignment_expr(p, vc, &arg);
                        if(err) goto call_cleanup;
                        // Ensure args array is big enough
                        while(args.count <= idx){
                            err = pa_push(&args, call_al, NULL);
                            if(err){ err = CC_OOM_ERROR; goto call_cleanup; }
                        }
                        if(args.data[idx] != NULL){
                            err = cc_error(p, name_tok.loc, "duplicate argument for parameter '%.*s'", name->length, name->data);
                            goto call_cleanup;
                        }
                        args.data[idx] = arg;
                        has_named = 1;
                    }
                    else if(dot.type == CC_PUNCTUATOR && dot.punct.punct == CC_lbracket){
                        // Designated positional argument: [N] = value
                        cc_next_token(p, &dot);
                        CcExpr* idx_expr;
                        err = cc_parse_assignment_expr(p, vc, &idx_expr);
                        if(err) goto call_cleanup;
                        CcEvalResult ev;
                        err = cc_eval_expr(p,idx_expr,&ev);
                        cc_release_expr(p, idx_expr);
                        if(err){
                            err = cc_error(p, dot.loc, "positional designator must be a constant expression"); goto call_cleanup;
                        }
                        int64_t idx_signed;
                        switch(ev.kind){
                            DEFAULT_UNREACHABLE;
                            case CC_EVAL_INT:    idx_signed = ev.i; break;
                            case CC_EVAL_UINT:   idx_signed = (int64_t)ev.u; break;
                            case CC_EVAL_FLOAT:
                            case CC_EVAL_DOUBLE: err = cc_error(p, dot.loc, "positional designator must be an integer"); goto call_cleanup;
                            case CC_EVAL_TYPE: case CC_EVAL_VOID: case CC_EVAL_INIT_LIST: case CC_EVAL_STRING:   err = cc_error(p, dot.loc, "positional designator must be a constant expression"); goto call_cleanup;
                        }
                        if(idx_signed < 0 || idx_signed > UINT32_MAX){
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
                        err = cc_parse_assignment_expr(p, vc, &arg);
                        if(err) goto call_cleanup;
                        while(args.count <= idx){
                            err = pa_push(&args, call_al, NULL);
                            if(err){ err = CC_OOM_ERROR; goto call_cleanup; }
                        }
                        if(args.data[idx] != NULL){
                            err = cc_error(p, dot.loc, "duplicate argument for position %u", (unsigned)idx);
                            goto call_cleanup;
                        }
                        args.data[idx] = arg;
                        has_named = 1;
                    }
                    else {
                        // Positional argument
                        if(has_named){
                            // Skip already-filled named slots
                            while(positional_index < args.count && args.data[positional_index] != NULL)
                                positional_index++;
                        }
                        CcExpr* arg;
                        err = cc_parse_assignment_expr(p, vc, &arg);
                        if(err) goto call_cleanup;
                        if(has_named){
                            while(args.count <= positional_index){
                                err = pa_push(&args, call_al, NULL);
                                if(err){ err = CC_OOM_ERROR; goto call_cleanup; }
                            }
                            if(args.data[positional_index] != NULL){
                                err = cc_error(p, arg->loc, "argument position %u already filled by named argument", (unsigned)positional_index);
                                goto call_cleanup;
                            }
                            args.data[positional_index] = arg;
                            positional_index++;
                        }
                        else {
                            err = pa_push(&args, call_al, arg);
                            if(err){ err = CC_OOM_ERROR; goto call_cleanup; }
                        }
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
                    // Allow trailing comma as extension
                    err = cc_peek(p, &sep);
                    if(err) goto call_cleanup;
                    if(sep.type == CC_PUNCTUATOR && sep.punct.punct == CC_rparen){
                        cc_next_token(p, &sep);
                        break;
                    }
                }
                call_args_done:;
                uint32_t nargs = (uint32_t)args.count;
                // Check for unfilled slots when named args were used
                if(has_named){
                    for(uint32_t i = 0; i < nargs && i < ftype->param_count; i++){
                        if(!args.data[i]){
                            Atom pn = (param_names && i < param_names_count) ? param_names[i] : NULL;
                            err = cc_error(p, tok.loc, "missing argument for parameter '%.*s'",
                                pn ? (int)pn->length : 1, pn ? pn->data : "?");
                            goto call_cleanup;
                        }
                    }
                }
                // Check arg count
                if(!ftype->is_variadic && !ftype->no_prototype && nargs != ftype->param_count){
                    err = cc_error(p, tok.loc, "Expected %u arguments, got %u", (unsigned)ftype->param_count, (unsigned)nargs);
                    goto call_cleanup;
                }
                if(ftype->is_variadic && nargs < ftype->param_count){
                    err = cc_error(p, tok.loc, "Too few arguments: expected at least %u, got %u", (unsigned)ftype->param_count, (unsigned)nargs);
                    goto call_cleanup;
                }
                // Type check / implicit cast args
                for(uint32_t i = 0; i < nargs; i++){
                    CcExpr** argp = (CcExpr**)&args.data[i];
                    if(!ftype->no_prototype && i < ftype->param_count){
                        err = cc_implicit_cast(p, *argp, ftype->params[i], argp);
                        if(err) goto call_cleanup;
                    }
                    else {
                        CcQualType at = (*argp)->type;
                        if(ccqt_kind(at) == CC_ARRAY && !ccqt_as_array(at)->is_vector){
                            // Array decays to pointer to element
                            CcQualType ptr_type;
                            err = cc_pointer_of(p, ccqt_as_array(at)->element, &ptr_type);
                            if(err) goto call_cleanup;
                            err = cc_implicit_cast(p, *argp, ptr_type, argp);
                            if(err) goto call_cleanup;
                        }
                        else if(ccqt_is_basic(at)){
                            CcBasicTypeKind k = at.basic.kind;
                            if(k == CCBT_float){
                                err = cc_implicit_cast(p, *argp, ccqt_basic(CCBT_double), argp);
                                if(err) goto call_cleanup;
                            }
                            else if(ccbt_is_integer(k) && ccbt_int_rank(k) < ccbt_int_rank(CCBT_int)){
                                err = cc_implicit_cast(p, *argp, ccqt_basic(CCBT_int), argp);
                                if(err) goto call_cleanup;
                            }
                        }
                    }
                }
                if(operand->kind != CC_EXPR_FUNCTION){
                    err = PM_put(&p->used_call_types, cc_allocator(p), ftype, ftype);
                    if(err){ err = CC_OOM_ERROR; goto call_cleanup; }
                }
                CcExpr* node = cc_make_expr(p, CC_EXPR_CALL, tok.loc, ftype->return_type, nargs);
                if(!node){ err = CC_OOM_ERROR; goto call_cleanup; }
                node->call.nargs = nargs;
                node->lhs = operand;
                memcpy(node->values, args.data, nargs * sizeof(CcExpr*));
                if(ftype->is_variadic && nargs > ftype->param_count){
                    err = PM_put(&p->used_var_calls, cc_allocator(p), node, node);
                    if(err){ err = CC_OOM_ERROR; goto call_cleanup; }
                }
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
    if(operand->kind == CC_EXPR_FUNCTION)
        operand->func->addr_taken = 1;
    *out = operand;
    return 0;
}

static const char* _Null_unspecified cc_basic_names[] = {
    [CCBT_INVALID]            = "<invalid>",
    [CCBT_void]               = "void",
    [CCBT_bool]               = "_Bool",
    [CCBT_char]               = "char",
    [CCBT_signed_char]        = "signed char",
    [CCBT_unsigned_char]      = "unsigned char",
    [CCBT_short]              = "short",
    [CCBT_unsigned_short]     = "unsigned short",
    [CCBT_int]                = "int",
    [CCBT_unsigned]           = "unsigned int",
    [CCBT_long]               = "long",
    [CCBT_unsigned_long]      = "unsigned long",
    [CCBT_long_long]          = "long long",
    [CCBT_unsigned_long_long] = "unsigned long long",
    [CCBT_int128]             = "__int128",
    [CCBT_unsigned_int128]    = "unsigned __int128",
    [CCBT_float16]            = "_Float16",
    [CCBT_float]              = "float",
    [CCBT_double]             = "double",
    [CCBT_long_double]        = "long double",
    [CCBT_float128]           = "_Float128",
    [CCBT_float_complex]      = "float _Complex",
    [CCBT_double_complex]     = "double _Complex",
    [CCBT_long_double_complex]= "long double _Complex",
    [CCBT_nullptr_t]          = "nullptr_t",
    [CCBT__Type]              = "_Type",
};

static void cc_print_type_pre(MStringBuilder*, CcQualType t);
static void cc_print_type_post(MStringBuilder*, CcQualType t);

static
_Bool
cc_type_needs_parens(CcQualType t){
    CcTypeKind k = ccqt_kind(t);
    return (k == CC_ARRAY && !ccqt_as_array(t)->is_vector) || k == CC_FUNCTION;
}

static
void
cc_print_type_pre(MStringBuilder* sb, CcQualType t){
    CcTypeKind kind = ccqt_kind(t);
    switch(kind){
        case CC_BASIC:
            if(t.is_const) msb_write_literal(sb, "const ");
            if(t.is_volatile) msb_write_literal(sb, "volatile ");
            if(t.is_atomic) msb_write_literal(sb, "_Atomic ");
            CcBasicTypeKind k = t.basic.kind;
            msb_sprintf(sb, "%s", k < CCBT_COUNT ? cc_basic_names[k] : "<bad-basic>");
            return;
        case CC_POINTER: {
            CcPointer* p = ccqt_as_ptr(t);
            cc_print_type_pre(sb, p->pointee);
            if(cc_type_needs_parens(p->pointee))
                msb_write_literal(sb, " (*");
            else
                msb_write_literal(sb, " *");
            if(t.is_const) msb_write_literal(sb, "const ");
            if(t.is_volatile) msb_write_literal(sb, "volatile ");
            if(t.is_atomic) msb_write_literal(sb, "_Atomic ");
            if(p->restrict_) msb_write_literal(sb, "restrict ");
            return;
        }
        case CC_ARRAY: {
            if(t.is_const) msb_write_literal(sb, "const ");
            if(t.is_volatile) msb_write_literal(sb, "volatile ");
            if(t.is_atomic) msb_write_literal(sb, "_Atomic ");
            CcArray* a = ccqt_as_array(t);
            cc_print_type_pre(sb, a->element);
            return;
        }
        case CC_FUNCTION: {
            CcFunction* f = ccqt_as_function(t);
            cc_print_type_pre(sb, f->return_type);
            return;
        }
        case CC_STRUCT: {
            if(t.is_const) msb_write_literal(sb, "const ");
            if(t.is_volatile) msb_write_literal(sb, "volatile ");
            if(t.is_atomic) msb_write_literal(sb, "_Atomic ");
            CcStruct* s = ccqt_as_struct(t);
            if(s->name) msb_sprintf(sb, "struct %.*s", s->name->length, s->name->data);
            else msb_write_literal(sb, "struct <anon>");
            return;
        }
        case CC_UNION: {
            if(t.is_const) msb_write_literal(sb, "const ");
            if(t.is_volatile) msb_write_literal(sb, "volatile ");
            if(t.is_atomic) msb_write_literal(sb, "_Atomic ");
            CcUnion* u = ccqt_as_union(t);
            if(u->name) msb_sprintf(sb, "union %.*s", u->name->length, u->name->data);
            else msb_write_literal(sb, "union <anon>");
            return;
        }
        case CC_ENUM: {
            if(t.is_const) msb_write_literal(sb, "const ");
            if(t.is_volatile) msb_write_literal(sb, "volatile ");
            if(t.is_atomic) msb_write_literal(sb, "_Atomic ");
            CcEnum* e = ccqt_as_enum(t);
            if(e->name) msb_sprintf(sb, "enum %.*s", e->name->length, e->name->data);
            else msb_write_literal(sb, "enum <anon>");
            return;
        }
    }
    msb_sprintf(sb, "<type:%d>", (int)kind);
}

static
void
cc_print_type_post(MStringBuilder* sb, CcQualType t){
    switch(ccqt_kind(t)){
        case CC_POINTER: {
            CcPointer* p = ccqt_as_ptr(t);
            if(cc_type_needs_parens(p->pointee))
                msb_write_char(sb, ')');
            cc_print_type_post(sb, p->pointee);
            return;
        }
        case CC_ARRAY: {
            CcArray* a = ccqt_as_array(t);
            if(a->is_vector)
                msb_sprintf(sb, " __attribute__((vector_size(%u)))", a->vector_size);
            else if(a->is_incomplete)
                msb_write_literal(sb, "[]");
            else
                msb_sprintf(sb, "[%zu]", a->length);
            cc_print_type_post(sb, a->element);
            return;
        }
        case CC_FUNCTION: {
            CcFunction* f = ccqt_as_function(t);
            msb_write_char(sb, '(');
            for(uint32_t i = 0; i < f->param_count; i++){
                if(i) msb_write_literal(sb, ", ");
                cc_print_type_pre(sb, f->params[i]);
                cc_print_type_post(sb, f->params[i]);
            }
            if(f->is_variadic){
                if(f->param_count) msb_write_literal(sb, ", ");
                msb_write_literal(sb, "...");
            }
            if(!f->param_count && !f->is_variadic){
                if(f->no_prototype) {} // empty parens
                else msb_write_literal(sb, "void");
            }
            msb_write_char(sb, ')');
            cc_print_type_post(sb, f->return_type);
            return;
        }
        case CC_STRUCT:
        case CC_UNION:
        case CC_ENUM:
        case CC_BASIC:
            return;
    }
}

static
void
cc_print_type(MStringBuilder* sb, CcQualType t){
    cc_print_type_pre(sb, t);
    cc_print_type_post(sb, t);
}

static
void
cc_print_runtime_value(CcParser* p, CcQualType type, const void* data, MStringBuilder* sb, int indent){
    const CcTargetConfig* tgt = cc_target(p);
    SrcLoc loc = {0};
    switch(ccqt_kind(type)){
        case CC_BASIC: {
            CcBasicTypeKind k = type.basic.kind;
            switch(k){
                case CCBT_void:
                    msb_write_literal(sb, "void");
                    return;
                case CCBT_bool:
                    if(*(const uint8_t*)data) msb_write_literal(sb, "true");
                    else msb_write_literal(sb, "false");
                    return;
                case CCBT_char: case CCBT_signed_char: case CCBT_unsigned_char: {
                    unsigned char c = *(const unsigned char*)data;
                    if(c >= 0x20 && c < 0x7f)
                        msb_sprintf(sb, "'%c' (%u)", c, c);
                    else
                        msb_sprintf(sb, "%u", c);
                    return;
                }
                case CCBT_float:
                    msb_sprintf(sb, "%g", (double)*(const float*)data);
                    return;
                case CCBT_double: case CCBT_long_double:
                    msb_sprintf(sb, "%g", *(const double*)data);
                    return;
                case CCBT__Type:
                    msb_write_char(sb, '(');
                    cc_print_type(sb, (CcQualType){.bits = *(const uintptr_t*)data});
                    msb_write_char(sb, ')');
                    return;
                case CCBT_nullptr_t:
                    msb_write_literal(sb, "nullptr");
                    return;
                case CCBT_short: case CCBT_int: case CCBT_long: case CCBT_long_long: {
                    int64_t v = 0;
                    memcpy(&v, data, tgt->sizeof_[k]);
                    unsigned sz = tgt->sizeof_[k];
                    if(sz < 8 && (v & ((int64_t)1 << (sz*8-1))))
                        v |= ~(((int64_t)1 << (sz*8)) - 1);
                    msb_sprintf(sb, "%lld", (long long)v);
                    return;
                }
                case CCBT_unsigned_short: case CCBT_unsigned: case CCBT_unsigned_long:
                case CCBT_unsigned_long_long: {
                    uint64_t v = 0;
                    memcpy(&v, data, tgt->sizeof_[k]);
                    msb_sprintf(sb, "%llu", (unsigned long long)v);
                    return;
                }
                default:
                    msb_write_literal(sb, "<unknown basic>");
                    return;
            }
        }
        case CC_POINTER: {
            void* ptr;
            memcpy(&ptr, data, tgt->sizeof_[CCBT_nullptr_t]);
            msb_sprintf(sb, "%p", ptr);
            return;
        }
        case CC_ENUM: {
            CcEnum* e = ccqt_as_enum(type);
            int64_t v = 0;
            uint32_t sz = tgt->sizeof_[e->underlying.basic.kind];
            memcpy(&v, data, sz);
            if(sz < 8 && (v & ((int64_t)1 << (sz*8-1))))
                v |= ~(((int64_t)1 << (sz*8)) - 1);
            for(size_t i = 0; i < e->enumerator_count; i++){
                if(e->enumerators[i]->value == v){
                    msb_sprintf(sb, "%.*s (%lld)", (int)e->enumerators[i]->name->length, e->enumerators[i]->name->data, (long long)v);
                    return;
                }
            }
            msb_sprintf(sb, "%lld", (long long)v);
            return;
        }
        case CC_ARRAY: {
            CcArray* arr = ccqt_as_array(type);
            if(arr->is_incomplete || arr->is_vla){ msb_write_literal(sb, "[...]"); return; }
            uint32_t elem_sz;
            if(cc_sizeof_as_uint(p, arr->element, loc, &elem_sz)){ msb_write_literal(sb, "[...]"); return; }
            msb_write_char(sb, '{');
            for(size_t i = 0; i < arr->length; i++){
                if(i) msb_write_literal(sb, ", ");
                if(i >= 16){ msb_write_literal(sb, "..."); break; }
                cc_print_runtime_value(p, arr->element, (const char*)data + i * elem_sz, sb, indent);
            }
            msb_write_char(sb, '}');
            return;
        }
        case CC_STRUCT: {
            CcStruct* s = ccqt_as_struct(type);
            if(s->is_incomplete){ msb_write_literal(sb, "{<incomplete>}"); return; }
            msb_write_literal(sb, "{\n");
            for(uint32_t i = 0; i < s->field_count; i++){
                CcField* f = &s->fields[i];
                if(f->is_method) continue;
                for(int j = 0; j < indent + 1; j++) msb_write_literal(sb, "  ");
                if(f->name)
                    msb_sprintf(sb, ".%.*s = ", (int)f->name->length, f->name->data);
                else
                    msb_write_literal(sb, "<anon> = ");
                if(f->is_bitfield){
                    uint64_t storage = 0;
                    memcpy(&storage, (const char*)data + f->offset, sizeof(uint32_t));
                    uint64_t val = (storage >> f->bitoffset) & (((uint64_t)1 << f->bitwidth) - 1);
                    msb_sprintf(sb, "%llu", (unsigned long long)val);
                }
                else {
                    cc_print_runtime_value(p, f->type, (const char*)data + f->offset, sb, indent + 1);
                }
                msb_write_literal(sb, ",\n");
            }
            for(int j = 0; j < indent; j++) msb_write_literal(sb, "  ");
            msb_write_char(sb, '}');
            return;
        }
        case CC_UNION: {
            CcUnion* u = ccqt_as_union(type);
            if(u->is_incomplete){ msb_write_literal(sb, "{<incomplete>}"); return; }
            msb_write_literal(sb, "{ /* union, ");
            msb_sprintf(sb, "%u bytes: ", u->size);
            for(uint32_t i = 0; i < u->size && i < 32; i++){
                msb_sprintf(sb, "%02x", ((const unsigned char*)data)[i]);
                if(i + 1 < u->size && i + 1 < 32) msb_write_char(sb, ' ');
            }
            if(u->size > 32) msb_write_literal(sb, " ...");
            msb_write_literal(sb, " */ }");
            return;
        }
        case CC_FUNCTION:
            msb_write_literal(sb, "<function>");
            return;
    }
}

static
void
cc_print_expr(MStringBuilder*sb, CcExpr* e){
    switch(e->kind){
        case CC_EXPR_VALUE:
            if(e->str.length && e->text){
                msb_sprintf(sb, "\"%.*s\"", e->str.length - 1, e->text);
            }
            else if(ccqt_is_basic(e->type) && e->type.basic.kind == CCBT__Type){
                msb_write_char(sb, '(');
                cc_print_type(sb, (CcQualType){.bits = e->uinteger});
                msb_write_char(sb, ')');
            }
            else if(ccqt_is_basic(e->type) && ccbt_is_float(e->type.basic.kind)){
                if(e->type.basic.kind == CCBT_float)
                    msb_sprintf(sb, "%gf", (double)e->float_);
                else
                    msb_sprintf(sb, "%g", e->double_);
            }
            else {
                msb_sprintf(sb, "%llu", (unsigned long long)e->uinteger);
            }
            return;
        case CC_EXPR_VARIABLE:
            msb_sprintf(sb, "%.*s", e->var->name->length, e->var->name->data);
            return;
        case CC_EXPR_FUNCTION:
            if(e->func->name)
                msb_sprintf(sb, "%.*s", e->func->name->length, e->func->name->data);
            else
                msb_write_literal(sb, "<lambda>");
            return;
        case CC_EXPR_SIZEOF_VMT:
        case CC_EXPR_STATEMENT_EXPRESSION:
        case CC_EXPR_ATOMIC:
        case CC_EXPR_VA:
        case CC_EXPR_BUILTIN:
        case CC_EXPR_MUL_OVERFLOW:
        case CC_EXPR_ADD_OVERFLOW:
        case CC_EXPR_SUB_OVERFLOW:
        case CC_EXPR_POPCOUNT:
        case CC_EXPR_CTZ:
        case CC_EXPR_CLZ:
        case CC_EXPR_ALLOCA:
        case CC_EXPR_INTERN:
            msb_write_literal(sb, "<unimpl>");
            return;
        case CC_EXPR_COMPOUND_LITERAL:
            msb_write_char(sb, '(');
            cc_print_type(sb, e->type);
            msb_write_char(sb, ')');
            goto print_init_list;
        case CC_EXPR_INIT_LIST:
        print_init_list: {
            CcInitList* il = e->init_list;
            msb_write_char(sb, '{');
            for(uint32_t i = 0; i < il->count; i++){
                if(i) msb_write_literal(sb, ", ");
                CcInitEntry* ent = &il->entries[i];
                _Bool show_offset = ent->field_loc.byte_offset || ent->field_loc.bit_width || il->count > 1;
                if(show_offset){
                    msb_sprintf(sb, "@%llu", (unsigned long long)ent->field_loc.byte_offset);
                    if(ent->field_loc.bit_width)
                        msb_sprintf(sb, ":%llu:%llu", (unsigned long long)ent->field_loc.bit_offset, (unsigned long long)ent->field_loc.bit_width);
                    msb_write_literal(sb, " = ");
                }
                if(ent->value)
                    cc_print_expr(sb, ent->value);
            }
            msb_write_char(sb, '}');
            return;
        }
        // Unary prefix
        #define UNOP(K, S) case K: msb_write_literal(sb, S); cc_print_expr(sb, e->lhs); return;
        UNOP(CC_EXPR_NEG,    "-")
        UNOP(CC_EXPR_POS,    "+")
        UNOP(CC_EXPR_BITNOT, "~")
        UNOP(CC_EXPR_LOGNOT, "!")
        UNOP(CC_EXPR_DEREF,  "*")
        UNOP(CC_EXPR_ADDR,   "&")
        UNOP(CC_EXPR_PREINC, "++")
        UNOP(CC_EXPR_PREDEC, "--")
        #undef UNOP
        // Unary postfix
        case CC_EXPR_POSTINC: cc_print_expr(sb, e->lhs); msb_write_literal(sb, "++"); return;
        case CC_EXPR_POSTDEC: cc_print_expr(sb, e->lhs); msb_write_literal(sb, "--"); return;
        // Binary ops
        #define BINOP(K, S) case K: msb_write_char(sb, '('); cc_print_expr(sb, e->lhs); msb_write_literal(sb, " " S " "); cc_print_expr(sb, e->values[0]); msb_write_char(sb, ')'); return;
        BINOP(CC_EXPR_ADD,    "+")
        BINOP(CC_EXPR_SUB,    "-")
        BINOP(CC_EXPR_MUL,    "*")
        BINOP(CC_EXPR_DIV,    "/")
        BINOP(CC_EXPR_MOD,    "%")
        BINOP(CC_EXPR_BITAND, "&")
        BINOP(CC_EXPR_BITOR,  "|")
        BINOP(CC_EXPR_BITXOR, "^")
        BINOP(CC_EXPR_LSHIFT, "<<")
        BINOP(CC_EXPR_RSHIFT, ">>")
        BINOP(CC_EXPR_LOGAND, "&&")
        BINOP(CC_EXPR_LOGOR,  "||")
        BINOP(CC_EXPR_EQ,     "==")
        BINOP(CC_EXPR_NE,     "!=")
        BINOP(CC_EXPR_LT,     "<")
        BINOP(CC_EXPR_GT,     ">")
        BINOP(CC_EXPR_LE,     "<=")
        BINOP(CC_EXPR_GE,     ">=")
        BINOP(CC_EXPR_ASSIGN,       "=")
        BINOP(CC_EXPR_ADDASSIGN,    "+=")
        BINOP(CC_EXPR_SUBASSIGN,    "-=")
        BINOP(CC_EXPR_MULASSIGN,    "*=")
        BINOP(CC_EXPR_DIVASSIGN,    "/=")
        BINOP(CC_EXPR_MODASSIGN,    "%=")
        BINOP(CC_EXPR_BITANDASSIGN, "&=")
        BINOP(CC_EXPR_BITORASSIGN,  "|=")
        BINOP(CC_EXPR_BITXORASSIGN, "^=")
        BINOP(CC_EXPR_LSHIFTASSIGN, "<<=")
        BINOP(CC_EXPR_RSHIFTASSIGN, ">>=")
        BINOP(CC_EXPR_COMMA,        ",")
        #undef BINOP
        case CC_EXPR_SUBSCRIPT:
            cc_print_expr(sb, e->lhs);
            msb_write_char(sb, '[');
            cc_print_expr(sb, e->values[0]);
            msb_write_char(sb, ']');
            return;
        case CC_EXPR_TERNARY:
            msb_write_char(sb, '(');
            cc_print_expr(sb, e->lhs);
            msb_write_literal(sb, " ? ");
            cc_print_expr(sb, e->values[0]);
            msb_write_literal(sb, " : ");
            cc_print_expr(sb, e->values[1]);
            msb_write_char(sb, ')');
            return;
        case CC_EXPR_CAST:
            msb_write_char(sb, '(');
            cc_print_type(sb, e->type);
            msb_write_char(sb, ')');
            cc_print_expr(sb, e->lhs);
            return;
        case CC_EXPR_DOT:
            cc_print_expr(sb, e->values[0]);
            msb_sprintf(sb, ".@%llu", (unsigned long long)e->field_loc.byte_offset);
            return;
        case CC_EXPR_ARROW:
            cc_print_expr(sb, e->values[0]);
            msb_sprintf(sb, "->@%llu", (unsigned long long)e->field_loc.byte_offset);
            return;
        case CC_EXPR_CALL:
            cc_print_expr(sb, e->lhs);
            msb_write_char(sb, '(');
            for(uint32_t i = 0; i < e->call.nargs; i++){
                if(i) msb_write_literal(sb, ", ");
                cc_print_expr(sb, e->values[i]);
            }
            msb_write_char(sb, ')');
            return;
        case CC_EXPR_TYPE_INTROSPECTION:
            cc_print_expr(sb, e->lhs);
            msb_write_literal(sb, ".<type_introspection>()");
            return;
    }
    msb_write_literal(sb, "<unknown>");
}

// Promote to common type for binary ops
static
void
cc_eval_promote(CcEvalResult* a, CcEvalResult* b){
    // float/double promotions
    if(a->kind == CC_EVAL_DOUBLE || b->kind == CC_EVAL_DOUBLE){
        if(a->kind == CC_EVAL_INT)       { a->d = (double)a->i; a->kind = CC_EVAL_DOUBLE; }
        else if(a->kind == CC_EVAL_UINT) { a->d = (double)a->u; a->kind = CC_EVAL_DOUBLE; }
        else if(a->kind == CC_EVAL_FLOAT){ a->d = (double)a->f; a->kind = CC_EVAL_DOUBLE; }
        if(b->kind == CC_EVAL_INT)       { b->d = (double)b->i; b->kind = CC_EVAL_DOUBLE; }
        else if(b->kind == CC_EVAL_UINT) { b->d = (double)b->u; b->kind = CC_EVAL_DOUBLE; }
        else if(b->kind == CC_EVAL_FLOAT){ b->d = (double)b->f; b->kind = CC_EVAL_DOUBLE; }
        return;
    }
    if(a->kind == CC_EVAL_FLOAT || b->kind == CC_EVAL_FLOAT){
        if(a->kind == CC_EVAL_INT)       { a->f = (float)a->i; a->kind = CC_EVAL_FLOAT; }
        else if(a->kind == CC_EVAL_UINT) { a->f = (float)a->u; a->kind = CC_EVAL_FLOAT; }
        if(b->kind == CC_EVAL_INT)       { b->f = (float)b->i; b->kind = CC_EVAL_FLOAT; }
        else if(b->kind == CC_EVAL_UINT) { b->f = (float)b->u; b->kind = CC_EVAL_FLOAT; }
        return;
    }
    // unsigned promotion
    if(a->kind == CC_EVAL_UINT || b->kind == CC_EVAL_UINT){
        if(a->kind == CC_EVAL_INT) { a->u = (uint64_t)a->i; a->kind = CC_EVAL_UINT; }
        if(b->kind == CC_EVAL_INT) { b->u = (uint64_t)b->i; b->kind = CC_EVAL_UINT; }
    }
}


static
int
cc_eval_expr(CcParser* p, CcExpr* e, CcEvalResult* result){
    switch(e->kind){
        DEFAULT_UNREACHABLE;
        case CC_EXPR_VALUE:
            if(e->str.length && e->text){
                *result = (CcEvalResult){.kind = CC_EVAL_STRING, .str = {e->text, e->str.length}};
                return 0;
            }
            switch(ccqt_kind(e->type)){
                case CC_BASIC:
                switch(e->type.basic.kind){
                    case CCBT_float:
                        *result = (CcEvalResult){.kind = CC_EVAL_FLOAT, .f = e->float_};
                        return 0;
                    case CCBT_double: case CCBT_long_double:
                        *result = (CcEvalResult){.kind = CC_EVAL_DOUBLE, .d = e->double_};
                        return 0;
                    case CCBT_unsigned: case CCBT_unsigned_long:
                    case CCBT_unsigned_long_long: case CCBT_unsigned_char:
                    case CCBT_unsigned_short: case CCBT_bool:
                        *result = (CcEvalResult){.kind = CC_EVAL_UINT, .u = e->uinteger};
                        return 0;
                    case CCBT_char:
                        if(!cc_target(p)->char_is_signed){
                            *result = (CcEvalResult){.kind = CC_EVAL_UINT, .u = e->uinteger};
                            return 0;
                        }
                        *result = (CcEvalResult){.kind = CC_EVAL_INT, .i = e->integer};
                        return 0;
                    case CCBT_signed_char:
                    case CCBT_short: case CCBT_int:
                    case CCBT_long: case CCBT_long_long:
                        *result = (CcEvalResult){.kind = CC_EVAL_INT, .i = e->integer};
                        return 0;
                    case CCBT__Type:
                        *result = (CcEvalResult){.kind = CC_EVAL_TYPE, .type = {.bits = e->uinteger}};
                        return 0;
                    case CCBT_int128: case CCBT_unsigned_int128:
                    case CCBT_float16: case CCBT_float128:
                    case CCBT_float_complex: case CCBT_double_complex:
                    case CCBT_long_double_complex:
                    case CCBT_void: case CCBT_nullptr_t:
                    case CCBT_INVALID: case CCBT_COUNT:
                        return 1;
                } return 1;
                case CC_ENUM:
                    *result = (CcEvalResult){.kind = CC_EVAL_UINT, .u = e->uinteger};
                    return 0;
                default:
                    return 1;

            }
        case CC_EXPR_NEG: {
            int err = cc_eval_expr(p, e->lhs, result);
            if(err) return err;
            switch(result->kind){
                DEFAULT_UNREACHABLE;
                case CC_EVAL_INT:    result->i = -result->i; return 0;
                case CC_EVAL_UINT:   *result = (CcEvalResult){.kind = CC_EVAL_INT, .i = -(int64_t)result->u}; return 0;
                case CC_EVAL_FLOAT:  result->f = -result->f; return 0;
                case CC_EVAL_DOUBLE: result->d = -result->d; return 0;
                case CC_EVAL_TYPE: case CC_EVAL_VOID: case CC_EVAL_INIT_LIST: case CC_EVAL_STRING:
                    return 1;
            }
        }
        case CC_EXPR_POS: return cc_eval_expr(p,e->lhs,result);
        case CC_EXPR_BITNOT: {
            int err = cc_eval_expr(p,e->lhs,result);
            if(err) return err;
            switch(result->kind){
                DEFAULT_UNREACHABLE;
                case CC_EVAL_INT:  result->i = ~result->i; return 0;
                case CC_EVAL_UINT: result->u = ~result->u; return 0;
                case CC_EVAL_FLOAT:
                case CC_EVAL_DOUBLE:
                case CC_EVAL_TYPE: case CC_EVAL_VOID: case CC_EVAL_INIT_LIST: case CC_EVAL_STRING:
                    return 1;
            }
        }
        case CC_EXPR_LOGNOT: {
            int err = cc_eval_expr(p,e->lhs,result);
            if(err) return err;
            switch(result->kind){
                case CC_EVAL_INT:    result->i = !result->i; return 0;
                case CC_EVAL_UINT:   *result = (CcEvalResult){.kind = CC_EVAL_INT, .i = !result->u}; return 0;
                case CC_EVAL_FLOAT:  *result = (CcEvalResult){.kind = CC_EVAL_INT, .i = !result->f}; return 0;
                case CC_EVAL_DOUBLE: *result = (CcEvalResult){.kind = CC_EVAL_INT, .i = !result->d}; return 0;
                case CC_EVAL_TYPE: case CC_EVAL_VOID: case CC_EVAL_INIT_LIST: case CC_EVAL_STRING:   return 1;
            }
            return 1;
        }
        case CC_EXPR_SUBSCRIPT: {
            // Handle string_literal[constant_index]
            CcExpr* arr = e->lhs;
            if(arr->kind == CC_EXPR_VALUE && arr->str.length && arr->text){
                CcEvalResult idx;
                int err = cc_eval_expr(p, e->values[0], &idx);
                if(err) return err;
                uint64_t i;
                switch(idx.kind){
                    case CC_EVAL_INT:
                        if(idx.i < 0 || (uint64_t)idx.i >= arr->str.length)
                            return 1;
                        i = (uint64_t)idx.i;
                        break;
                    case CC_EVAL_UINT:
                        if(idx.u >= arr->str.length)
                            return 1;
                        i = idx.u;
                        break;
                    case CC_EVAL_FLOAT: case CC_EVAL_DOUBLE: case CC_EVAL_TYPE: case CC_EVAL_VOID: case CC_EVAL_INIT_LIST: case CC_EVAL_STRING: return 1;
                    DEFAULT_UNREACHABLE;
                }
                unsigned char c = (unsigned char)arr->text[i];
                if(cc_target(p)->char_is_signed){
                    *result = (CcEvalResult){.kind = CC_EVAL_INT, .i = (signed char)c};
                    return 0;
                }
                *result = (CcEvalResult){.kind = CC_EVAL_UINT, .u = c};
                return 0;
            }
            goto eval_init_list_access;
        }
        case CC_EXPR_COMMA: {
            CcEvalResult discard;
            int err = cc_eval_expr(p,e->lhs,&discard);
            if(err) return err;
            return cc_eval_expr(p,e->values[0],result);
        }
        case CC_EXPR_TERNARY: {
            CcEvalResult cond;
            int err = cc_eval_expr(p,e->lhs,&cond);
            if(err) return err;
            _Bool truthy;
            switch(cond.kind){
                case CC_EVAL_INT:    truthy = cond.i != 0; break;
                case CC_EVAL_UINT:   truthy = cond.u != 0; break;
                case CC_EVAL_FLOAT:  truthy = cond.f != 0; break;
                case CC_EVAL_DOUBLE: truthy = cond.d != 0; break;
                case CC_EVAL_TYPE: case CC_EVAL_VOID: case CC_EVAL_INIT_LIST: case CC_EVAL_STRING:   return 1;
                DEFAULT_UNREACHABLE;
            }
            return cc_eval_expr(p,e->values[truthy ? 0 : 1],result);
        }
        // Binary arithmetic
        case CC_EXPR_ADD: case CC_EXPR_SUB: case CC_EXPR_MUL:
        case CC_EXPR_DIV: case CC_EXPR_MOD:
        case CC_EXPR_BITAND: case CC_EXPR_BITOR: case CC_EXPR_BITXOR:
        case CC_EXPR_LSHIFT: case CC_EXPR_RSHIFT:
        case CC_EXPR_LOGAND: case CC_EXPR_LOGOR:
        case CC_EXPR_EQ: case CC_EXPR_NE:
        case CC_EXPR_LT: case CC_EXPR_GT: case CC_EXPR_LE: case CC_EXPR_GE:
        {
            CcEvalResult L, R;
            int err = cc_eval_expr(p,e->lhs,&L);
            if(err) return err;
            err = cc_eval_expr(p,e->values[0],&R);
            if(err) return err;
            if(L.kind == CC_EVAL_TYPE && R.kind == CC_EVAL_TYPE){
                if(e->kind == CC_EXPR_EQ){
                    *result = (CcEvalResult){.kind=CC_EVAL_INT, .i=L.type.bits == R.type.bits};
                    return 0;
                }
                if(e->kind == CC_EXPR_NE){
                    *result = (CcEvalResult){.kind=CC_EVAL_INT, .i=L.type.bits != R.type.bits};
                    return 0;
                }
                return 1;
            }
            cc_eval_promote(&L, &R);
            #define IBINOP(op) \
                switch(L.kind){ \
                    case CC_EVAL_INT:    *result = (CcEvalResult){.kind=CC_EVAL_INT, .i=L.i op R.i}; return 0; \
                    case CC_EVAL_UINT:   *result = (CcEvalResult){.kind=CC_EVAL_UINT, .u=L.u op R.u}; return 0; \
                    case CC_EVAL_FLOAT:  *result = (CcEvalResult){.kind=CC_EVAL_FLOAT, .f=L.f op R.f}; return 0; \
                    case CC_EVAL_DOUBLE: *result = (CcEvalResult){.kind=CC_EVAL_DOUBLE, .d=L.d op R.d}; return 0; \
                    case CC_EVAL_TYPE: case CC_EVAL_VOID: case CC_EVAL_INIT_LIST: case CC_EVAL_STRING: return 1; \
                    DEFAULT_UNREACHABLE; \
                }
            #define IINTOP(op) \
                switch(L.kind){ \
                    case CC_EVAL_INT:  *result = (CcEvalResult){.kind=CC_EVAL_INT, .i=L.i op R.i}; return 0; \
                    case CC_EVAL_UINT: *result = (CcEvalResult){.kind=CC_EVAL_UINT, .u=L.u op R.u}; return 0; \
                    case CC_EVAL_FLOAT: case CC_EVAL_DOUBLE: case CC_EVAL_TYPE: case CC_EVAL_VOID: case CC_EVAL_INIT_LIST: case CC_EVAL_STRING: return 1; \
                    DEFAULT_UNREACHABLE; \
                }
            #define ICMPOP(op) \
                switch(L.kind){ \
                    case CC_EVAL_INT:    *result = (CcEvalResult){.kind=CC_EVAL_INT, .i=L.i op R.i}; return 0; \
                    case CC_EVAL_UINT:   *result = (CcEvalResult){.kind=CC_EVAL_INT, .i=L.u op R.u}; return 0; \
                    case CC_EVAL_FLOAT:  *result = (CcEvalResult){.kind=CC_EVAL_INT, .i=L.f op R.f}; return 0; \
                    case CC_EVAL_DOUBLE: *result = (CcEvalResult){.kind=CC_EVAL_INT, .i=L.d op R.d}; return 0; \
                    case CC_EVAL_TYPE: case CC_EVAL_VOID: case CC_EVAL_INIT_LIST: case CC_EVAL_STRING: return 1; \
                    DEFAULT_UNREACHABLE; \
                }
            switch(e->kind){
                case CC_EXPR_ADD: IBINOP(+)
                case CC_EXPR_SUB: IBINOP(-)
                case CC_EXPR_MUL: IBINOP(*)
                case CC_EXPR_DIV:
                    // Check for division by zero
                    if(L.kind == CC_EVAL_INT && R.i == 0) return 1;
                    if(L.kind == CC_EVAL_UINT && R.u == 0) return 1;
                    IBINOP(/)
                case CC_EXPR_MOD:
                    if(L.kind == CC_EVAL_INT && R.i == 0) return 1;
                    if(L.kind == CC_EVAL_UINT && R.u == 0) return 1;
                    IINTOP(%)
                case CC_EXPR_BITAND: IINTOP(&)
                case CC_EXPR_BITOR:  IINTOP(|)
                case CC_EXPR_BITXOR: IINTOP(^)
                case CC_EXPR_LSHIFT: IINTOP(<<)
                case CC_EXPR_RSHIFT: IINTOP(>>)
                case CC_EXPR_LOGAND:
                    switch(L.kind){
                        case CC_EVAL_INT:    *result = (CcEvalResult){.kind=CC_EVAL_INT, .i=L.i && R.i}; return 0;
                        case CC_EVAL_UINT:   *result = (CcEvalResult){.kind=CC_EVAL_INT, .i=L.u && R.u}; return 0;
                        case CC_EVAL_FLOAT:  *result = (CcEvalResult){.kind=CC_EVAL_INT, .i=L.f && R.f}; return 0;
                        case CC_EVAL_DOUBLE: *result = (CcEvalResult){.kind=CC_EVAL_INT, .i=L.d && R.d}; return 0;
                        case CC_EVAL_TYPE: case CC_EVAL_VOID: case CC_EVAL_INIT_LIST: case CC_EVAL_STRING: return 1;
                        DEFAULT_UNREACHABLE;
                    }
                case CC_EXPR_LOGOR:
                    switch(L.kind){
                        case CC_EVAL_INT:    *result = (CcEvalResult){.kind=CC_EVAL_INT, .i=L.i || R.i}; return 0;
                        case CC_EVAL_UINT:   *result = (CcEvalResult){.kind=CC_EVAL_INT, .i=L.u || R.u}; return 0;
                        case CC_EVAL_FLOAT:  *result = (CcEvalResult){.kind=CC_EVAL_INT, .i=L.f || R.f}; return 0;
                        case CC_EVAL_DOUBLE: *result = (CcEvalResult){.kind=CC_EVAL_INT, .i=L.d || R.d}; return 0;
                        case CC_EVAL_TYPE: case CC_EVAL_VOID: case CC_EVAL_INIT_LIST: case CC_EVAL_STRING: return 1;
                        DEFAULT_UNREACHABLE;
                    }
                case CC_EXPR_EQ: ICMPOP(==)
                case CC_EXPR_NE: ICMPOP(!=)
                case CC_EXPR_LT: ICMPOP(<)
                case CC_EXPR_GT: ICMPOP(>)
                case CC_EXPR_LE: ICMPOP(<=)
                case CC_EXPR_GE: ICMPOP(>=)
                default: return 1;
            }
            #undef IBINOP
            #undef IINTOP
            #undef ICMPOP
        }
        case CC_EXPR_CAST: {
            int err = cc_eval_expr(p,e->lhs,result);
            if(err) return err;
            if(!ccqt_is_basic(e->type)) return 0;
            CcBasicTypeKind tk = e->type.basic.kind;
            if(tk == CCBT_void){
                *result = (CcEvalResult){.kind = CC_EVAL_VOID};
                return 0;
            }
            if(ccbt_is_float(tk)){
                double d;
                switch(result->kind){
                    DEFAULT_UNREACHABLE;
                    case CC_EVAL_INT:    d = (double)result->i; break;
                    case CC_EVAL_UINT:   d = (double)result->u; break;
                    case CC_EVAL_FLOAT:  d = (double)result->f; break;
                    case CC_EVAL_DOUBLE: d = result->d; break;
                    case CC_EVAL_TYPE: case CC_EVAL_VOID: case CC_EVAL_INIT_LIST: case CC_EVAL_STRING:
                        return 1;
                }
                if(tk == CCBT_float){
                    *result = (CcEvalResult){.kind = CC_EVAL_FLOAT, .f = (float)d};
                    return 0;
                }
                *result = (CcEvalResult){.kind = CC_EVAL_DOUBLE, .d = d};
                return 0;
            }
            if(ccbt_is_integer(tk)){
                if(ccbt_is_unsigned(tk, !cc_target(p)->char_is_signed)){
                    switch(result->kind){
                        DEFAULT_UNREACHABLE;
                        case CC_EVAL_INT:    *result = (CcEvalResult){.kind=CC_EVAL_UINT, .u=(uint64_t)result->i}; return 0;
                        case CC_EVAL_UINT:   return 0;
                        case CC_EVAL_FLOAT:  *result = (CcEvalResult){.kind=CC_EVAL_UINT, .u=(uint64_t)result->f}; return 0;
                        case CC_EVAL_DOUBLE: *result = (CcEvalResult){.kind=CC_EVAL_UINT, .u=(uint64_t)result->d}; return 0;
                        case CC_EVAL_TYPE: case CC_EVAL_VOID: case CC_EVAL_INIT_LIST: case CC_EVAL_STRING:
                            return 1;
                    }
                }
                else {
                    switch(result->kind){
                        DEFAULT_UNREACHABLE;
                        case CC_EVAL_INT:    return 0;
                        case CC_EVAL_UINT:   *result = (CcEvalResult){.kind=CC_EVAL_INT, .i=(int64_t)result->u}; return 0;
                        case CC_EVAL_FLOAT:  *result = (CcEvalResult){.kind=CC_EVAL_INT, .i=(int64_t)result->f}; return 0;
                        case CC_EVAL_DOUBLE: *result = (CcEvalResult){.kind=CC_EVAL_INT, .i=(int64_t)result->d}; return 0;
                        case CC_EVAL_TYPE: case CC_EVAL_VOID: case CC_EVAL_INIT_LIST: case CC_EVAL_STRING:
                            return 1;
                    }
                }
            }
            return 0;
        }
        case CC_EXPR_TYPE_INTROSPECTION: {
            CcEvalResult lhs;
            int err = cc_eval_expr(p, e->lhs, &lhs);
            if(err) return err;
            if(lhs.kind != CC_EVAL_TYPE) return 1;
            CcQualType qt = lhs.type;
            CcTypeIntrospectionOp op = e->type_introspection.op;
            switch(op){
                case CC_TYPE_IS_INTEGER:    *result = (CcEvalResult){.kind=CC_EVAL_INT, .i = ccqt_is_basic(qt) && ccbt_is_integer(qt.basic.kind)}; return 0;
                case CC_TYPE_IS_FLOAT:      *result = (CcEvalResult){.kind=CC_EVAL_INT, .i = ccqt_is_basic(qt) && ccbt_is_float(qt.basic.kind)}; return 0;
                case CC_TYPE_IS_ARITHMETIC: *result = (CcEvalResult){.kind=CC_EVAL_INT, .i = (ccqt_is_basic(qt) && ccbt_is_arithmetic(qt.basic.kind)) || ccqt_kind(qt) == CC_ENUM}; return 0;
                case CC_TYPE_IS_POINTER:    *result = (CcEvalResult){.kind=CC_EVAL_INT, .i = ccqt_kind(qt) == CC_POINTER}; return 0;
                case CC_TYPE_IS_STRUCT:     *result = (CcEvalResult){.kind=CC_EVAL_INT, .i = ccqt_kind(qt) == CC_STRUCT}; return 0;
                case CC_TYPE_IS_UNION:      *result = (CcEvalResult){.kind=CC_EVAL_INT, .i = ccqt_kind(qt) == CC_UNION}; return 0;
                case CC_TYPE_IS_ARRAY:      *result = (CcEvalResult){.kind=CC_EVAL_INT, .i = ccqt_kind(qt) == CC_ARRAY}; return 0;
                case CC_TYPE_IS_FUNCTION:   *result = (CcEvalResult){.kind=CC_EVAL_INT, .i = ccqt_kind(qt) == CC_FUNCTION}; return 0;
                case CC_TYPE_IS_ENUM:       *result = (CcEvalResult){.kind=CC_EVAL_INT, .i = ccqt_kind(qt) == CC_ENUM}; return 0;
                case CC_TYPE_IS_CONST:      *result = (CcEvalResult){.kind=CC_EVAL_INT, .i = qt.is_const}; return 0;
                case CC_TYPE_IS_VOLATILE:   *result = (CcEvalResult){.kind=CC_EVAL_INT, .i = qt.is_volatile}; return 0;
                case CC_TYPE_IS_ATOMIC:     *result = (CcEvalResult){.kind=CC_EVAL_INT, .i = qt.is_atomic}; return 0;
                case CC_TYPE_IS_UNSIGNED:   *result = (CcEvalResult){.kind=CC_EVAL_INT, .i = ccqt_is_basic(qt) && ccbt_is_unsigned(qt.basic.kind, !cc_target(p)->char_is_signed)}; return 0;
                case CC_TYPE_IS_SIGNED:    *result = (CcEvalResult){.kind=CC_EVAL_INT, .i = ccqt_is_basic(qt) && ccbt_is_integer(qt.basic.kind) && !ccbt_is_unsigned(qt.basic.kind, !cc_target(p)->char_is_signed)}; return 0;
                case CC_TYPE_IS_INCOMPLETE: {
                    CcTypeKind k = ccqt_kind(qt);
                    _Bool is_incomplete;
                    switch(k){
                        DEFAULT_UNREACHABLE;
                        case CC_STRUCT:
                            is_incomplete = ccqt_as_struct(qt)->is_incomplete;
                            break;
                        case CC_UNION:
                            is_incomplete = ccqt_as_union(qt)->is_incomplete;
                            break;
                        case CC_ARRAY:
                            is_incomplete = ccqt_as_array(qt)->is_incomplete;
                            break;
                        case CC_ENUM:
                            is_incomplete = ccqt_as_enum(qt)->is_incomplete;
                            break;
                        case CC_FUNCTION:
                        case CC_BASIC:
                        case CC_POINTER:
                            is_incomplete = 0;
                    }
                    *result = (CcEvalResult){.kind=CC_EVAL_INT, .i=is_incomplete};
                    return 0;
                }
                case CC_TYPE_IS_CALLABLE: {
                    CcTypeKind k = ccqt_kind(qt);
                    *result = (CcEvalResult){.kind=CC_EVAL_INT, .i = k == CC_FUNCTION || (k == CC_POINTER && ccqt_kind(ccqt_as_ptr(qt)->pointee) == CC_FUNCTION)};
                    return 0;
                }
                case CC_TYPE_IS_VARIADIC: {
                    CcQualType ft = qt;
                    if(ccqt_kind(ft) == CC_POINTER) ft = ccqt_as_ptr(ft)->pointee;
                    *result = (CcEvalResult){.kind=CC_EVAL_INT, .i = ccqt_kind(ft) == CC_FUNCTION && ccqt_as_function(ft)->is_variadic};
                    return 0;
                }
                case CC_TYPE_SIZEOF: {
                    uint32_t sz;
                    err = cc_sizeof_as_uint(p, qt, e->loc, &sz);
                    if(err) return 1;
                    *result = (CcEvalResult){.kind=CC_EVAL_UINT, .u = sz};
                    return 0;
                }
                case CC_TYPE_ALIGNOF: {
                    uint32_t al;
                    err = cc_alignof_as_uint(p, qt, e->loc, &al);
                    if(err) return 1;
                    *result = (CcEvalResult){.kind=CC_EVAL_UINT, .u = al};
                    return 0;
                }
                case CC_TYPE_POINTEE:
                    if(ccqt_kind(qt) != CC_POINTER) return 1;
                    *result = (CcEvalResult){.kind=CC_EVAL_TYPE, .type = ccqt_as_ptr(qt)->pointee};
                    return 0;
                case CC_TYPE_UNQUAL: {
                    CcQualType uq = qt;
                    uq.quals = 0;
                    *result = (CcEvalResult){.kind=CC_EVAL_TYPE, .type = uq};
                    return 0;
                }
                case CC_TYPE_COUNT:
                    if(ccqt_kind(qt) != CC_ARRAY) return 1;
                    *result = (CcEvalResult){.kind=CC_EVAL_UINT, .u = ccqt_as_array(qt)->length};
                    return 0;
                case CC_TYPE_IS_CALLABLE_WITH: {
                    CcEvalResult arg;
                    err = cc_eval_expr(p, e->values[0], &arg);
                    if(err) return err;
                    if(arg.kind != CC_EVAL_TYPE) return 1;
                    CcQualType ft = qt;
                    if(ccqt_kind(ft) == CC_POINTER) ft = ccqt_as_ptr(ft)->pointee;
                    _Bool v = 0;
                    if(ccqt_kind(ft) == CC_FUNCTION){
                        CcFunction* f = ccqt_as_function(ft);
                        if(f->param_count == 1)
                            v = cc_implicit_convertible(arg.type, f->params[0]);
                    }
                    *result = (CcEvalResult){.kind=CC_EVAL_INT, .i = v};
                    return 0;
                }
                case CC_TYPE_CASTABLE_TO: {
                    CcEvalResult arg;
                    err = cc_eval_expr(p, e->values[0], &arg);
                    if(err) return err;
                    if(arg.kind != CC_EVAL_TYPE) return 1;
                    *result = (CcEvalResult){.kind=CC_EVAL_INT, .i = cc_explicit_castable(qt, arg.type)};
                    return 0;
                }
                case CC_TYPE_FIELDS: {
                    CcTypeKind k = ccqt_kind(qt);
                    if(k != CC_STRUCT && k != CC_UNION) return 1;
                    CcStruct* s = ccqt_as_struct(qt);
                    *result = (CcEvalResult){.kind=CC_EVAL_INT, .i = s->field_count};
                    return 0;
                }
                case CC_TYPE_RETURN_TYPE: {
                    CcQualType ft = qt;
                    if(ccqt_kind(ft) == CC_POINTER) ft = ccqt_as_ptr(ft)->pointee;
                    if(ccqt_kind(ft) != CC_FUNCTION) return 1;
                    *result = (CcEvalResult){.kind=CC_EVAL_TYPE, .type = ccqt_as_function(ft)->return_type};
                    return 0;
                }
                case CC_TYPE_PARAM_COUNT: {
                    CcQualType ft = qt;
                    if(ccqt_kind(ft) == CC_POINTER) ft = ccqt_as_ptr(ft)->pointee;
                    if(ccqt_kind(ft) != CC_FUNCTION) return 1;
                    *result = (CcEvalResult){.kind=CC_EVAL_UINT, .u = ccqt_as_function(ft)->param_count};
                    return 0;
                }
                case CC_TYPE_PARAM_TYPE: {
                    CcQualType ft = qt;
                    if(ccqt_kind(ft) == CC_POINTER) ft = ccqt_as_ptr(ft)->pointee;
                    if(ccqt_kind(ft) != CC_FUNCTION) return 1;
                    CcFunction* f = ccqt_as_function(ft);
                    CcEvalResult idx;
                    err = cc_eval_expr(p, e->values[0], &idx);
                    if(err) return err;
                    if(idx.kind != CC_EVAL_INT && idx.kind != CC_EVAL_UINT) return 1;
                    uint64_t i = idx.kind == CC_EVAL_UINT ? idx.u : (uint64_t)idx.i;
                    if(i >= f->param_count) return 1;
                    *result = (CcEvalResult){.kind=CC_EVAL_TYPE, .type = f->params[i]};
                    return 0;
                }
                case CC_TYPE_ELEMENT_TYPE:
                    if(ccqt_kind(qt) != CC_ARRAY) return 1;
                    *result = (CcEvalResult){.kind=CC_EVAL_TYPE, .type = ccqt_as_array(qt)->element};
                    return 0;
                case CC_TYPE_UNDERLYING_TYPE:
                    if(ccqt_kind(qt) != CC_ENUM) return 1;
                    *result = (CcEvalResult){.kind=CC_EVAL_TYPE, .type = ccqt_as_enum(qt)->underlying};
                    return 0;
                case CC_TYPE_ENUMERATORS: {
                    if(ccqt_kind(qt) != CC_ENUM) return 1;
                    CcEnum* e2 = ccqt_as_enum(qt);
                    *result = (CcEvalResult){.kind=CC_EVAL_UINT, .u = e2->enumerator_count};
                    return 0;
                }
                case CC_TYPE_PUSH_METHOD: // handled at parse time, shouldn't reach here
                case CC_TYPE_ENUMERATOR: // can't constant-fold (returns struct)
                case CC_TYPE_FIELD:
                case CC_TYPE_NAME:
                case CC_TYPE_TAG:
                case CC_TYPE_NONE:
                    return 1; // can't constant-fold
            }
            return 1;
        }
        case CC_EXPR_ATOMIC:
        case CC_EXPR_SIZEOF_VMT:
        case CC_EXPR_VARIABLE:
            if(e->var->constexpr_ && e->var->initializer)
                return cc_eval_expr(p, e->var->initializer, result);
            return 1;
        case CC_EXPR_INIT_LIST:
            *result = (CcEvalResult){.kind = CC_EVAL_INIT_LIST, .init_list = e->init_list};
            return 0;
        case CC_EXPR_FUNCTION:
        case CC_EXPR_COMPOUND_LITERAL:
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
        case CC_EXPR_CALL:
        case CC_EXPR_DOT:
        eval_init_list_access: {
            // Accumulate byte offset through chained DOTs and SUBSCRIPTs
            // to resolve against the root init list.
            uint64_t offset = 0;
            CcExpr* cur = e;
            for(;;){
                if(cur->kind == CC_EXPR_DOT){
                    offset += cur->field_loc.byte_offset;
                    cur = cur->values[0];
                }
                else if(cur->kind == CC_EXPR_SUBSCRIPT){
                    CcEvalResult idx;
                    int err = cc_eval_expr(p, cur->values[0], &idx);
                    if(err) return err;
                    uint64_t i;
                    switch(idx.kind){
                        case CC_EVAL_INT:  i = (uint64_t)idx.i; break;
                        case CC_EVAL_UINT: i = idx.u; break;
                        default: return 1;
                    }
                    uint32_t elem_size;
                    err = cc_sizeof_as_uint(p, cur->type, cur->loc, &elem_size);
                    if(err) return 1;
                    offset += i * elem_size;
                    cur = cur->lhs;
                }
                else break;
            }
            CcEvalResult base;
            int err = cc_eval_expr(p, cur, &base);
            if(err) return err;
            if(base.kind == CC_EVAL_STRING){
                if(offset < base.str.length){
                    unsigned char c = (unsigned char)base.str.text[offset];
                    if(cc_target(p)->char_is_signed)
                        *result = (CcEvalResult){.kind = CC_EVAL_INT, .i = (signed char)c};
                    else
                        *result = (CcEvalResult){.kind = CC_EVAL_UINT, .u = c};
                    return 0;
                }
                return 1;
            }
            if(base.kind != CC_EVAL_INIT_LIST) return 1;
            CcInitList* il = base.init_list;
            for(uint32_t i = 0; i < il->count; i++){
                uint64_t entry_off = il->entries[i].field_loc.byte_offset;
                CcExpr* v = il->entries[i].value;
                // String literal spanning a range of bytes
                if(v->kind == CC_EXPR_VALUE && v->str.length && v->text
                && offset >= entry_off && offset < entry_off + v->str.length){
                    uint64_t idx = offset - entry_off;
                    unsigned char c = (unsigned char)v->text[idx];
                    if(cc_target(p)->char_is_signed)
                        *result = (CcEvalResult){.kind = CC_EVAL_INT, .i = (signed char)c};
                    else
                        *result = (CcEvalResult){.kind = CC_EVAL_UINT, .u = c};
                    return 0;
                }
                if(entry_off == offset && il->entries[i].field_loc.bit_offset == e->field_loc.bit_offset)
                    return cc_eval_expr(p, v, result);
            }
            return 1;
        }
        case CC_EXPR_ARROW:
        case CC_EXPR_STATEMENT_EXPRESSION:
        case CC_EXPR_VA:
        case CC_EXPR_BUILTIN:
        case CC_EXPR_ADD_OVERFLOW:
        case CC_EXPR_MUL_OVERFLOW:
        case CC_EXPR_SUB_OVERFLOW:
            return 1;
        case CC_EXPR_POPCOUNT:
        case CC_EXPR_CLZ:
        case CC_EXPR_CTZ: {
            int err = cc_eval_expr(p, e->lhs, result);
            if(err) return err;
            uint64_t v;
            switch(result->kind){
                case CC_EVAL_INT:  v = (uint64_t)result->i; break;
                case CC_EVAL_UINT: v = result->u; break;
                case CC_EVAL_FLOAT: case CC_EVAL_DOUBLE:
                case CC_EVAL_TYPE: case CC_EVAL_VOID: case CC_EVAL_INIT_LIST: case CC_EVAL_STRING:
                    return 1;
                DEFAULT_UNREACHABLE;
            }
            int64_t r;
            if(e->kind == CC_EXPR_POPCOUNT){
                r = 0;
                while(v){ r += v & 1; v >>= 1; }
            }
            else if(e->kind == CC_EXPR_CLZ){
                if(v == 0) return 1;
                r = __builtin_clzll(v);
            }
            else {
                if(v == 0) return 1;
                r = __builtin_ctzll(v);
            }
            *result = (CcEvalResult){.kind = CC_EVAL_INT, .i = r};
            return 0;
        }
        case CC_EXPR_ALLOCA:
        case CC_EXPR_INTERN:
            return 1;
    }
}

static
void
cc_print_eval_result(MStringBuilder* sb, CcEvalResult r){
    switch(r.kind){
        case CC_EVAL_INT:    msb_sprintf(sb, "%lld", (long long)r.i); break;
        case CC_EVAL_UINT:   msb_sprintf(sb, "%llu", (unsigned long long)r.u); break;
        case CC_EVAL_FLOAT:  msb_sprintf(sb, "%g", (double)r.f); break;
        case CC_EVAL_DOUBLE: msb_sprintf(sb, "%g", r.d); break;
        case CC_EVAL_TYPE: case CC_EVAL_VOID: case CC_EVAL_INIT_LIST: case CC_EVAL_STRING:  msb_write_literal(sb, "<cannot evaluate>"); break;
    }
}

// Skip all remaining else/else-if branches (condition was already taken).
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
        cc_next_token(p, &tok); // consume `else`
        err = cc_peek(p, &tok);
        if(err) return err;
        if(tok.type == CC_KEYWORD && tok.kw.kw == CC_if){
            cc_next_token(p, &tok); // consume `if`
            // Skip the condition: ( ... )
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
            continue; // check for more else/else-if
        }
        // plain else { ... }
        return cc_skip_braced_block(p);
    }
}

// Parse a braced block body, executing its contents.
// No scope push/pop — declarations are injected into the current scope.
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

// Handle `static if(cond) { ... } [else if(cond) { ... }]* [else { ... }]`
// Assumes `static` and `if` have already been consumed.
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
        CcEvalResult ev;
        err = cc_eval_expr(p,cond,&ev);
        SrcLoc cond_loc = cond->loc;
        cc_release_expr(p, cond);
        if(err)
            return cc_error(p, cond_loc, "static if condition must be a constant expression");
        switch(ev.kind){
            case CC_EVAL_INT:    predicate = ev.i != 0; break;
            case CC_EVAL_UINT:   predicate = ev.u != 0; break;
            case CC_EVAL_FLOAT:  predicate = ev.f != 0; break;
            case CC_EVAL_DOUBLE: predicate = ev.d != 0; break;
            case CC_EVAL_TYPE: case CC_EVAL_VOID: case CC_EVAL_INIT_LIST: case CC_EVAL_STRING:
                return cc_error(p, cond_loc, "static if condition must be a constant expression");
            DEFAULT_UNREACHABLE;
        }
    }
    if(predicate){
        err = cc_parse_static_if_body(p, loc);
        if(err) return err;
        return cc_skip_static_else_chain(p);
    }
    err = cc_skip_braced_block(p);
    if(err) return err;
    // Check for else / else if
    err = cc_peek(p, &tok);
    if(err) return err;
    if(tok.type != CC_KEYWORD || tok.kw.kw != CC_else)
        return 0;
    cc_next_token(p, &tok); // consume `else`
    err = cc_peek(p, &tok);
    if(err) return err;
    if(tok.type == CC_KEYWORD && tok.kw.kw == CC_if){
        cc_next_token(p, &tok); // consume `if`
        return cc_parse_static_if(p, loc); // recurse for else-if
    }
    // plain else { ... }
    return cc_parse_static_if_body(p, loc);
}

static
int
cc_parse_one(CcParser* p){
    int err;
    CcToken tok;
    // Check for `static if`
    err = cc_peek(p, &tok);
    if(err) return err;
    if(tok.type == CC_KEYWORD && tok.kw.kw == CC_static){
        CcToken if_tok;
        cc_next_token(p, &tok); // consume `static`
        err = cc_peek(p, &if_tok);
        if(err) return err;
        if(if_tok.type == CC_KEYWORD && if_tok.kw.kw == CC_if){
            cc_next_token(p, &if_tok); // consume `if`
            return cc_parse_static_if(p, tok.loc);
        }
        cc_unget(p, &tok); // push `static` back
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
                return cc_handle_static_asssert(p);
            }
            goto Ldefault;
        default:
            Ldefault:;
            break;
    }
    return cc_parse_statement(p);
}

static
int
cc_parse_all(CcParser* p){
    int err;
    CcToken tok;
    for(;;){
        err = cc_peek(p, &tok);
        if(err) return err;
        if(tok.type == CC_EOF)
            break;
        err = cc_parse_one(p);
        if(err) return err;
    }
    return cc_resolve_gotos(p,
        p->toplevel_statements.data,
        p->toplevel_statements.count,
        &p->toplevel_labels);
}

static
void
cc_parser_discard_input(CcParser* p){
    p->pending.count = 0;
    cpp_discard_all_input(&p->cpp);
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

static
int
cc_expect_punct(CcParser* p, CcPunct punct){
    CcToken tok;
    int err = cc_next_token(p, &tok);
    if(err) return err;
    if(tok.type != CC_PUNCTUATOR || tok.punct.punct != punct){
        // Build a readable name for the expected punctuator
        char buf[4];
        int len = 0;
        uint32_t v = (uint32_t)punct;
        // Multi-char puncts are stored as multi-char constants
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
        return cc_error(p, tok.loc, "Expected '%s'", buf);
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
            // cc_info(p, (SrcLoc){0}, "free list hit");
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
        // nvalues = 0, has lhs
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
        case CC_EXPR_POPCOUNT:
        case CC_EXPR_CLZ:
        case CC_EXPR_CTZ:
        case CC_EXPR_ALLOCA:
        case CC_EXPR_INTERN:
        case CC_EXPR_STATEMENT_EXPRESSION:
            return 0;
        // nvalues = 0, no children
        case CC_EXPR_VALUE:
        case CC_EXPR_VARIABLE:
        case CC_EXPR_FUNCTION:
        case CC_EXPR_BUILTIN:
            return 0;
        // nvalues = 1
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
        // nvalues = 2
        case CC_EXPR_TERNARY:
        case CC_EXPR_ADD_OVERFLOW:
        case CC_EXPR_MUL_OVERFLOW:
        case CC_EXPR_SUB_OVERFLOW:
            return 2;
        // variable
        case CC_EXPR_CALL:
            return e->call.nargs;
        // these need case-by-case
        case CC_EXPR_VA:
            return (e->va.op == CC_VA_COPY) ? 1 : 0;
        case CC_EXPR_ATOMIC:
            switch(e->atomic.op){
                case CC_ATOMIC_LOAD_N:
                case CC_ATOMIC_THREAD_FENCE:
                case CC_ATOMIC_SIGNAL_FENCE:
                    return 0;
                case CC_ATOMIC_STORE_N:
                case CC_ATOMIC_FETCH_ADD:
                case CC_ATOMIC_FETCH_SUB:
                case CC_ATOMIC_EXCHANGE_N:
                case CC_ATOMIC_LOAD:
                case CC_ATOMIC_STORE:
                    return 1;
                case CC_ATOMIC_EXCHANGE:
                case CC_ATOMIC_COMPARE_EXCHANGE_N:
                case CC_ATOMIC_COMPARE_EXCHANGE:
                    return 2;
            }
            return 0;
        case CC_EXPR_TYPE_INTROSPECTION:
            return (e->type_introspection.op >= CC_TYPE_IS_CALLABLE_WITH) ? 1 : 0;
        case CC_EXPR_COMPOUND_LITERAL:
        case CC_EXPR_INIT_LIST:
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
    // Release lhs child if this kind uses lhs as a CcExpr*
    switch(e->kind){
        case CC_EXPR_VALUE:
        case CC_EXPR_VARIABLE:
        case CC_EXPR_FUNCTION:
        case CC_EXPR_BUILTIN:
        case CC_EXPR_ARROW:
        case CC_EXPR_DOT:
            break;
        case CC_EXPR_COMPOUND_LITERAL:
        case CC_EXPR_INIT_LIST:
            if(e->init_list){
                for(uint32_t i = 0; i < e->init_list->count; i++){
                    if(e->init_list->entries[i].value)
                        cc_release_expr(p, e->init_list->entries[i].value);
                }
                Allocator_free(cc_allocator(p), e->init_list, sizeof(CcInitList) + e->init_list->count * sizeof(CcInitEntry));
            }
            break;
        default:
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
    CcPointer* ptr = cc_intern_pointer(&p->type_cache, cc_allocator(p), pointee, 0);
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

static
int
cc_parse_attributes(CcParser* p, CcAttributes* attrs){
    int err = 0;
    CcToken tok;
    for(;;){
        err = cc_peek(p, &tok);
        if(err) return err;
        if(tok.type != CC_KEYWORD || tok.kw.kw != CC___attribute__)
            return 0;
        err = cc_next_token(p, &tok); // consume __attribute__
        if(err) return err;
        SrcLoc attr_loc = tok.loc;
        // Expect ((
        err = cc_expect_punct(p, CC_lparen);
        if(err) return err;
        err = cc_expect_punct(p, CC_lparen);
        if(err) return err;
        // Parse attribute list: attr [, attr]*
        // Empty attribute list is allowed: __attribute__(())
        err = cc_peek(p, &tok);
        if(err) return err;
        if(tok.type == CC_PUNCTUATOR && tok.punct.punct == CC_rparen)
            goto close_parens;
        for(;;){
            err = cc_next_token(p, &tok);
            if(err) return err;
            if(tok.type != CC_IDENTIFIER && tok.type != CC_KEYWORD)
                return cc_error(p, tok.loc, "expected attribute name");
            // Get the attribute name as a string
            StringView attr_name;
            if(tok.type == CC_IDENTIFIER)
                attr_name = (StringView){.text = tok.ident.ident->data, .length = tok.ident.ident->length};
            else {
                // Attribute name that is also a keyword.
                // Just ignore it for now, in the future we can parse const I guess.
                attr_name = SV("");
            }
            // Strip leading/trailing underscores for canonical matching
            if(sv_startswith(attr_name, SV("__")))
                attr_name = sv_slice(attr_name, 2, attr_name.length);
            if(sv_endswith(attr_name, SV("__")))
                attr_name = sv_slice(attr_name, 0, attr_name.length-2);
            if(sv_equals(attr_name, SV("packed"))){
                attrs->packed = 1;
            }
            else if(sv_equals(attr_name, SV("transparent_union"))){
                attrs->transparent_union = 1;
            }
            else if(sv_equals(attr_name, SV("aligned"))){
                // Check for optional (N) argument
                err = cc_peek(p, &tok);
                if(err) return err;
                if(tok.type == CC_PUNCTUATOR && tok.punct.punct == CC_lparen){
                    err = cc_next_token(p, &tok); // consume '('
                    if(err) return err;
                    CcExpr* expr = NULL;
                    err = cc_parse_assignment_expr(p, CC_CONSTEXPR_VALUE, &expr);
                    if(err) return err;
                    if(!expr)
                        return cc_error(p, tok.loc, "expected constant expression for aligned attribute");
                    CcEvalResult val;
                    err = cc_eval_expr(p,expr,&val);
                    cc_release_expr(p, expr);
                    if(err)
                        return cc_error(p, tok.loc, "aligned attribute requires a constant expression");
                    if(val.kind != CC_EVAL_INT && val.kind != CC_EVAL_UINT)
                        return cc_error(p, tok.loc, "aligned attribute requires a constant integral expression");
                    uint64_t align = val.u;
                    if(align == 0 || (align & (align - 1)) != 0)
                        return cc_error(p, tok.loc, "alignment must be a positive power of 2");
                    attrs->aligned = (uint16_t)align;
                    attrs->has_aligned = 1;
                    err = cc_expect_punct(p, CC_rparen);
                    if(err) return err;
                }
                else {
                    // aligned without argument means maximum alignment for the target
                    attrs->aligned = cc_target(p)->max_align;
                    attrs->has_aligned = 1;
                }
            }
            else if(sv_equals(attr_name, SV("vector_size"))){
                err = cc_expect_punct(p, CC_lparen);
                if(err) return err;
                CcExpr* expr = NULL;
                err = cc_parse_assignment_expr(p, CC_CONSTEXPR_VALUE, &expr);
                if(err) return err;
                if(!expr)
                    return cc_error(p, tok.loc, "expected constant expression for vector_size attribute");
                CcEvalResult val;
                err = cc_eval_expr(p,expr,&val);
                cc_release_expr(p, expr);
                if(err)
                    return cc_error(p, tok.loc, "vector_size attribute requires a constant expression");
                if(val.kind != CC_EVAL_INT && val.kind != CC_EVAL_UINT)
                    return cc_error(p, tok.loc, "vector_size attribute requires a constant integral expression");
                uint64_t vs = val.u;
                if(vs == 0 || (vs & (vs - 1)) != 0)
                    return cc_error(p, tok.loc, "vector_size must be a power of 2");
                if(vs > UINT16_MAX)
                    return cc_error(p, tok.loc, "vector_size too large");
                attrs->vector_size = (uint16_t)vs;
                err = cc_expect_punct(p, CC_rparen);
                if(err) return err;
            }
            else {
                // Unknown attribute — skip any argument list
                if(0) cc_warn(p, tok.loc, "ignoring unknown attribute '%.*s'", (int)attr_name.length, attr_name.text);
                err = cc_peek(p, &tok);
                if(err) return err;
                if(tok.type == CC_PUNCTUATOR && tok.punct.punct == CC_lparen){
                    err = cc_next_token(p, &tok); // consume '('
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
            }
            // Check for comma or end
            err = cc_peek(p, &tok);
            if(err) return err;
            if(tok.type == CC_PUNCTUATOR && tok.punct.punct == CC_comma){
                err = cc_next_token(p, &tok); // consume ','
                if(err) return err;
                continue;
            }
            break;
        }
        close_parens:
        // Expect ))
        err = cc_expect_punct(p, CC_rparen);
        if(err) return err;
        err = cc_expect_punct(p, CC_rparen);
        if(err) return err;
    }
}


static inline
uint32_t
cc_align_to(uint32_t offset, uint32_t alignment){
    return (offset + alignment - 1) & ~(alignment - 1);
}

static _Bool cc_sysv_classify_type(const CcTargetConfig* tc, CcQualType type, uint32_t off, CcSysVEightByte cls[_Nonnull 2]);
static CcBasicTypeKind cc_arm64_hfa_check(const CcTargetConfig* tc, CcQualType type, CcBasicTypeKind base, uint32_t* count);

static
uint32_t
cc_type_sizeof_assume_complete(const CcTargetConfig* tc, CcQualType type){
    switch(ccqt_kind(type)){
        DEFAULT_UNREACHABLE;
        case CC_BASIC:    return tc->sizeof_[type.basic.kind];
        case CC_POINTER:  return tc->sizeof_[CCBT_nullptr_t];
        case CC_FUNCTION: return tc->sizeof_[CCBT_nullptr_t];
        case CC_ENUM:     return cc_type_sizeof_assume_complete(tc, ccqt_as_enum(type)->underlying);
        case CC_STRUCT:   return ccqt_as_struct(type)->size;
        case CC_UNION:    return ccqt_as_union(type)->size;
        case CC_ARRAY:{
            CcArray* a = ccqt_as_array(type);
            if(a->is_vector) return a->vector_size;
            return cc_type_sizeof_assume_complete(tc, a->element) * (uint32_t)a->length;
        }
    }
}

// SysV x86_64 eightbyte classification.
// cls[0]/cls[1] track the class per eightbyte (INTEGER dominates SSE).
// Returns 1 if the aggregate must be passed in MEMORY.

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
            // long double / float128 / long double complex force MEMORY
            // in aggregates on x86_64.
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
        case CC_POINTER:
        case CC_FUNCTION:
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
            uint32_t elem_sz = cc_type_sizeof_assume_complete(tc, arr->element);
            for(uint32_t i = 0; i < (uint32_t)arr->length; i++){
                if(cc_sysv_classify_type(tc, arr->element, off + i * elem_sz, cls))
                    return 1;
            }
            return 0;
        }
    }
    return 0;
}

// ARM64 HFA detection.
// Recursively checks that all leaf data types are the same float type.
// `base` is CCBT_INVALID on first call, set to the float type once found.
// `count` accumulates the number of float elements (for structs/arrays)
// or is computed from size for unions.
// Returns the base type, or CCBT_INVALID if not HFA-compatible.

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
        case CC_POINTER:
        case CC_FUNCTION:
        case CC_ENUM:
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
            // All members must be same-base-type HFA-compatible.
            // Count for the union = size / sizeof(base), since members overlap.
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
    uint32_t bitfield_offset = 0; // bit offset within current storage unit
    uint32_t bitfield_storage_end = 0; // byte offset of end of current storage unit
    uint32_t bitfield_storage_start = 0; // byte offset of start of current storage unit
    CcQualType bitfield_type = {0}; // type of current bitfield run (for MSVC ABI)
    CcBitfieldABI bf_abi = cc_target(p)->bitfield_abi;
    for(uint32_t i = 0; i < s->field_count; i++){
        CcField* f = &s->fields[i];
        if(f->is_method) continue;
        // Flexible array member: incomplete array as the last field.
        if(ccqt_kind(f->type) == CC_ARRAY){
            CcArray* arr = ccqt_as_array(f->type);
            if(arr->is_incomplete){
                if(i + 1 < s->field_count){
                    // Check that no non-method fields follow.
                    _Bool has_later = 0;
                    for(uint32_t j = i + 1; j < s->field_count; j++){
                        if(!s->fields[j].is_method){ has_later = 1; break; }
                    }
                    if(has_later)
                        return cc_error(p, f->loc, "flexible array member must be last field");
                }
                // End any bitfield run.
                if(bitfield_offset > 0){
                    offset = bitfield_storage_end;
                    bitfield_offset = 0;
                }
                uint32_t field_align;
                err = cc_alignof_as_uint(p, f->type, f->loc, &field_align);
                if(err) return err;
                if(s->packed) field_align = 1;
                else if(pack_value > 0 && field_align > pack_value) field_align = pack_value;
                offset = cc_align_to(offset, field_align);
                f->offset = offset;
                if(field_align > max_align) max_align = field_align;
                s->has_fam = 1;
                continue;
            }
        }
        // Reject embedded struct with FAM, unless it's an anonymous
        // member at the end (GCC extension: FAM in anonymous struct).
        if(ccqt_kind(f->type) == CC_STRUCT){
            CcStruct* inner = ccqt_as_struct(f->type);
            if(inner->has_fam){
                _Bool is_last = 1;
                for(uint32_t j = i + 1; j < s->field_count; j++){
                    if(!s->fields[j].is_method){ is_last = 0; break; }
                }
                if(!f->name && is_last){
                    // Anonymous struct with FAM at end: propagate FAM
                    s->has_fam = 1;
                }
                else {
                    return cc_error(p, f->loc, "struct with flexible array member cannot be embedded");
                }
            }
        }
        uint32_t field_size, field_align;
        err = cc_sizeof_as_uint(p, f->type, f->loc, &field_size);
        if(err) return err;
        err = cc_alignof_as_uint(p, f->type, f->loc, &field_align);
        if(err) return err;
        if(f->alignment > field_align)
            field_align = f->alignment;
        if(s->packed)
            field_align = 1;
        else if(pack_value > 0 && field_align > pack_value)
            field_align = pack_value;
        if(f->is_bitfield){
            // Bitfield layout
            uint32_t bw = f->bitwidth;
            uint32_t storage_bits = field_size * 8;
            if(bw == 0){
                // Zero-width bitfield: force alignment to next storage unit boundary
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
            // Can we pack into the current storage unit?
            _Bool fits;
            uint32_t f_offset = 0, f_bitoffset = 0;
            if(bf_abi == CC_BITFIELD_MSVC){
                fits = bitfield_type.bits == f->type.bits && bitfield_offset + bw <= storage_bits;
                f_offset = bitfield_storage_start;
                f_bitoffset = bitfield_offset;
            }
            else if(!bitfield_storage_end){
                fits = 0;
            }
            else {
                // SysV/Itanium: pack across different underlying types.
                // Check if the bits fit within this field's own
                // naturally-aligned storage unit at the current position.
                uint32_t abs_bit = bitfield_storage_start * 8 + bitfield_offset;
                uint32_t su_start = ((abs_bit / 8) / field_align) * field_align;
                uint32_t bit_in_su = abs_bit - su_start * 8;
                fits = bit_in_su + bw <= field_size * 8;
                f_offset = su_start;
                f_bitoffset = bit_in_su;
            }
            if(fits){
                f->offset = f_offset;
                f->bitoffset = f_bitoffset;
                bitfield_offset += bw;
                if(f_offset + field_size > bitfield_storage_end)
                    bitfield_storage_end = f_offset + field_size;
            }
            else {
                // Start new storage unit
                if(bitfield_offset > 0)
                    offset = bitfield_storage_end;
                offset = cc_align_to(offset, field_align);
                f->offset = offset;
                f->bitoffset = 0;
                bitfield_offset = bw;
                bitfield_storage_start = offset;
                bitfield_storage_end = offset + field_size;

                bitfield_type = f->type;
            }
            if(field_align > max_align)
                max_align = field_align;
            continue;
        }
        // Regular field: end any bitfield run
        if(bitfield_offset > 0){
            offset = bitfield_storage_end;
            bitfield_offset = 0;
            bitfield_storage_end = 0;

            bitfield_type = (CcQualType){0};
        }
        offset = cc_align_to(offset, field_align);
        f->offset = offset;
        offset += field_size;
        if(field_align > max_align)
            max_align = field_align;
    }
    // End any trailing bitfield run
    if(bitfield_offset > 0)
        offset = bitfield_storage_end;
    // Apply explicit struct-level alignment from __attribute__((aligned(N)))
    // This is already stored in s->alignment if set before calling this function.
    if(s->alignment > max_align)
        max_align = s->alignment;
    s->alignment = max_align;
    s->size = cc_align_to(offset, max_align);
    // Compute calling convention classification.
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
            if(s->size > 16 || s->alignment > 16) break;
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
        // FAM in union: zero-size, just contributes alignment
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
        if(f->alignment > field_align)
            field_align = f->alignment;
        if(pack_value > 0 && field_align > pack_value)
            field_align = pack_value;
        f->offset = 0; // all union fields start at offset 0
        if(f->is_bitfield){
            f->bitoffset = 0;
            // For bitfields in unions, size is the storage unit size
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
    u->size = cc_align_to(max_size, max_align);
    // Compute calling convention classification.
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
            if(u->size > 16 || u->alignment > 16) break;
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

    // Find the closing paren.
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
        p->pragma_pack = 8; // Apparently /Zp: can change this?
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
            cc_info(p, loc, "#pragma pack(show): %d", (int)p->pragma_pack);
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
_Bool
cc_lookup_field(CcField* _Nullable fields, uint32_t field_count, Atom name, CcFieldLoc* out_loc, CcQualType* out_type, CcFunc*_Nullable*_Nullable out_method){
    for(uint32_t i = 0; i < field_count; i++){
        CcField* f = &fields[i];
        if(f->is_method){
            if(f->method->name == name){
                *out_loc = (CcFieldLoc){.byte_offset = f->offset};
                *out_type = f->type;
                if(out_method) *out_method = f->method;
                return 1;
            }
            continue;
        }
        if(f->name == name){
            *out_loc = (CcFieldLoc){
                .byte_offset = f->offset,
                .bit_offset = f->is_bitfield ? f->bitoffset : 0,
                .bit_width = f->is_bitfield ? f->bitwidth : 0,
            };
            *out_type = f->type;
            if(out_method) *out_method = NULL;
            return 1;
        }
        if(!f->name){
            // Anonymous member — search recursively
            CcTypeKind tk = ccqt_kind(f->type);
            CcField* _Nullable sub_fields = NULL;
            uint32_t sub_count = 0;
            if(tk == CC_STRUCT){
                CcStruct* inner = ccqt_as_struct(f->type);
                sub_fields = inner->fields;
                sub_count = inner->field_count;
            }
            else if(tk == CC_UNION){
                CcUnion* inner = ccqt_as_union(f->type);
                sub_fields = inner->fields;
                sub_count = inner->field_count;
            }
            if(sub_fields && cc_lookup_field(sub_fields, sub_count, name, out_loc, out_type, out_method)){
                out_loc->byte_offset += f->offset;
                return 1;
            }
        }
    }
    return 0;
}

static
_Bool
cc_has_field(CcField* _Nullable fields, uint32_t field_count, Atom name){
    CcFieldLoc loc;
    CcQualType type;
    return cc_lookup_field(fields, field_count, name, &loc, &type, NULL);
}

static
uint32_t
cc_find_field_index(CcField*_Nullable fields, uint32_t field_count, Atom name, CcFieldLoc* out_loc, CcQualType* out_type){
    for(uint32_t k = 0; k < field_count; k++){
        if(fields[k].is_method) continue;
        if(fields[k].name == name){
            *out_loc = (CcFieldLoc){
                .byte_offset = fields[k].offset,
                .bit_offset = fields[k].is_bitfield ? fields[k].bitoffset : 0,
                .bit_width = fields[k].is_bitfield ? fields[k].bitwidth : 0,
            };
            *out_type = fields[k].type;
            return k;
        }
        if(!fields[k].name){
            CcFieldLoc inner_loc;
            CcQualType inner_type;
            CcField* _Nullable sub_fields;
            uint32_t sub_count;
            CcTypeKind tk = ccqt_kind(fields[k].type);
            if(tk == CC_STRUCT){
                sub_fields = ccqt_as_struct(fields[k].type)->fields;
                sub_count = ccqt_as_struct(fields[k].type)->field_count;
            }
            else if(tk == CC_UNION){
                sub_fields = ccqt_as_union(fields[k].type)->fields;
                sub_count = ccqt_as_union(fields[k].type)->field_count;
            }
            else continue;
            if(cc_lookup_field(sub_fields, sub_count, name, &inner_loc, &inner_type, NULL)){
                inner_loc.byte_offset += fields[k].offset;
                *out_loc = inner_loc;
                *out_type = inner_type;
                return k;
            }
        }
    }
    *out_loc = (CcFieldLoc){0};
    *out_type = (CcQualType){0};
    return field_count;
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
cc_push_scalar(CcParser* p, CcExpr* value, CcQualType target, CcFieldLoc field_loc, Marray(CcInitEntry)* buf){
    CcQualType t = target;
    t.is_const = 0; t.is_volatile = 0; t.is_atomic = 0;
    CcExpr* casted;
    int err = cc_implicit_cast(p, value, t, &casted);
    if(err) return err;
    err = ma_push(CcInitEntry)(buf, cc_allocator(p),
        ((CcInitEntry){.field_loc = field_loc, .value = casted}));
    if(err) return CC_OOM_ERROR;
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
cc_parse_scalar_value(CcParser* p, CcValueClass vc, CcExpr*_Nullable*_Nonnull out){
    CcToken peek;
    int err = cc_peek(p, &peek);
    if(err) return err;
    if(peek.type == CC_PUNCTUATOR && peek.punct.punct == CC_lbrace){
        // Count and consume opening braces
        uint32_t depth = 0;
        while(peek.type == CC_PUNCTUATOR && peek.punct.punct == CC_lbrace){
            cc_next_token(p, &peek);
            depth++;
            err = cc_peek(p, &peek);
            if(err) return err;
        }
        err = cc_parse_assignment_expr(p, vc, out);
        if(err) return err;
        // Consume matching closing braces
        for(uint32_t i = 0; i < depth; i++){
            err = cc_peek(p, &peek);
            if(err) return err;
            // Allow trailing comma before '}'
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
    return cc_parse_assignment_expr(p, vc, out);
}

static
int
cc_parse_desig_tail(CcParser* p, CcQualType* sub, CcFieldLoc* fl){
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
            CcFieldLoc inner_loc;
            CcQualType inner_type;
            if(!cc_lookup_field(sub_fields, sub_count, field_tok.ident.ident, &inner_loc, &inner_type, NULL))
                return cc_error(p, peek.loc, "no member named '%.*s'",
                    field_tok.ident.ident->length, field_tok.ident.ident->data);
            fl->byte_offset += inner_loc.byte_offset;
            fl->bit_offset = inner_loc.bit_offset;
            fl->bit_width = inner_loc.bit_width;
            *sub = inner_type;
        }
        else if(peek.punct.punct == CC_lbracket){
            cc_next_token(p, &peek);
            if(ccqt_kind(*sub) != CC_ARRAY)
                return cc_error(p, peek.loc, "index designator into non-array type");
            CcArray* a = ccqt_as_array(*sub);
            CcExpr* idx_expr;
            err = cc_parse_assignment_expr(p, CC_CONSTEXPR_VALUE, &idx_expr);
            if(err) return err;
            CcEvalResult ev;
            err = cc_eval_expr(p,idx_expr,&ev);
            cc_release_expr(p, idx_expr);
            if(err)
                return cc_error(p, peek.loc, "array designator must be a constant expression");
            int64_t idx_signed;
            switch(ev.kind){
                DEFAULT_UNREACHABLE;
                case CC_EVAL_INT:    idx_signed = ev.i; break;
                case CC_EVAL_UINT:   idx_signed = (int64_t)ev.u; break;
                case CC_EVAL_FLOAT:
                case CC_EVAL_DOUBLE: return cc_error(p, peek.loc, "array designator must be an integer");
                case CC_EVAL_TYPE: case CC_EVAL_VOID: case CC_EVAL_INIT_LIST: case CC_EVAL_STRING:   return cc_error(p, peek.loc, "array designator must be a constant expression");
            }
            if(idx_signed < 0 || idx_signed > UINT32_MAX)
                return cc_error(p, peek.loc, "array designator value out of range");
            uint32_t idx = (uint32_t)idx_signed;
            err = cc_expect_punct(p, CC_rbracket);
            if(err) return err;
            if(!a->is_incomplete && idx >= a->length)
                return cc_error(p, peek.loc, "array index %u out of bounds (size %zu)", idx, a->length);
            uint32_t esz = 0;
            err = cc_sizeof_as_uint(p, a->element, peek.loc, &esz);
            if(err) return err;
            fl->byte_offset += idx * esz;
            *sub = a->element;
            fl->bit_offset = 0; fl->bit_width = 0;
        }
        else break;
    }
    return cc_expect_punct(p, CC_assign);
}

static
int
cc_init_apply_value(CcParser* p, CcValueClass vc, CcQualType field_type, CcFieldLoc field_loc, CcExpr* value, SrcLoc loc, Marray(CcInitEntry)* buf){
    if(value->kind == CC_EXPR_COMPOUND_LITERAL)
        value->kind = CC_EXPR_INIT_LIST;
    CcQualType unqual = field_type;
    unqual.is_const = 0; unqual.is_volatile = 0; unqual.is_atomic = 0;
    CcTypeKind ftk = ccqt_kind(unqual);
    if(ftk == CC_STRUCT || ftk == CC_UNION || ftk == CC_ARRAY){
        if(cc_implicit_convertible(value->type, unqual))
            return cc_push_scalar(p, value, unqual, field_loc, buf);
        return cc_parse_init(p, vc,field_type, field_loc.byte_offset, 0, loc, buf, NULL, value);
    }
    return cc_push_scalar(p, value, field_type, field_loc, buf);
}

static
int
cc_parse_init_value(CcParser* p, CcValueClass vc, CcQualType field_type, CcFieldLoc field_loc, _Bool positional, SrcLoc loc, Marray(CcInitEntry)* buf){
    int err;
    CcTypeKind ftk = ccqt_kind(field_type);
    if(ftk == CC_STRUCT || ftk == CC_UNION || ftk == CC_ARRAY){
        CcToken peek;
        err = cc_peek(p, &peek);
        if(err) return err;
        if(peek.type == CC_PUNCTUATOR && peek.punct.punct == CC_lbrace){
            cc_next_token(p, &peek); // consume '{'
            return cc_parse_init(p, vc,field_type, field_loc.byte_offset, 1, peek.loc, buf, NULL, NULL);
        }
        // String literal initializing a char array (brace elision)
        if(ftk == CC_ARRAY && peek.type == CC_STRING_LITERAL){
            CcArray* arr = ccqt_as_array(field_type);
            CcBasicTypeKind ek = ccqt_is_basic(arr->element) ? arr->element.basic.kind : CCBT_COUNT;
            if(ek == CCBT_char || ek == CCBT_signed_char || ek == CCBT_unsigned_char){
                CcExpr* v;
                err = cc_parse_assignment_expr(p, vc, &v);
                if(err) return err;
                if(!arr->is_incomplete && arr->length < v->str.length - 1)
                    return cc_error(p, v->loc, "initializer string too long for array");
                err = ma_push(CcInitEntry)(buf, cc_allocator(p),
                    ((CcInitEntry){.field_loc = field_loc, .value = v}));
                if(err) return CC_OOM_ERROR;
                return 0;
            }
        }
        {
            CcExpr* v;
            err = cc_parse_assignment_expr(p, vc, &v);
            if(err) return err;
            if(positional)
                return cc_init_apply_value(p, vc,field_type, field_loc, v, loc, buf);
            return cc_push_scalar(p, v, field_type, field_loc, buf);
        }
    }
    // Scalar
    CcExpr* v;
    err = cc_parse_scalar_value(p, vc, &v);
    if(err) return err;
    return cc_push_scalar(p, v, field_type, field_loc, buf);
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
cc_parse_init(CcParser* p, CcValueClass vc, CcQualType target, uint64_t base_offset, _Bool braced, SrcLoc loc, Marray(CcInitEntry)* buf, uint32_t*_Nullable out_max_index, CcExpr*_Nullable first_value){
    int err;
    CcQualType unqual = target;
    unqual.is_const = 0;
    unqual.is_volatile = 0;
    unqual.is_atomic = 0;
    CcTypeKind tk = ccqt_kind(unqual);
    switch(tk){
    case CC_STRUCT: {
        CcStruct* s = ccqt_as_struct(unqual);
        if(s->is_incomplete)
            return cc_error(p, loc, "initializer for incomplete struct type");
        uint32_t fi = 0;
        for(;;){
            if(first_value){
                CcExpr* fv = first_value;
                while(fi < s->field_count && (s->fields[fi].is_method || (!s->fields[fi].name && s->fields[fi].is_bitfield)))
                    fi++;
                if(fi >= s->field_count) break;
                CcField* fld = &s->fields[fi];
                CcFieldLoc fl = {
                    .byte_offset = base_offset + fld->offset,
                    .bit_offset = fld->is_bitfield ? fld->bitoffset : 0,
                    .bit_width = fld->is_bitfield ? fld->bitwidth : 0,
                };
                err = cc_init_apply_value(p, vc,fld->type, fl, fv, loc, buf);
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
                // Check for designator
                if(peek.type == CC_PUNCTUATOR && peek.punct.punct == CC_lbracket)
                    return cc_error(p, peek.loc, "array designator in struct initializer");
                if(peek.type == CC_PUNCTUATOR && peek.punct.punct == '.'){
                    cc_next_token(p, &peek); // consume '.'
                    SrcLoc desig_loc = peek.loc;
                    CcToken field_tok;
                    err = cc_next_token(p, &field_tok);
                    if(err) return err;
                    if(field_tok.type != CC_IDENTIFIER)
                        return cc_error(p, field_tok.loc, "expected field name after '.'");
                    CcFieldLoc fl;
                    CcQualType sub;
                    uint32_t idx = cc_find_field_index(s->fields, s->field_count, field_tok.ident.ident, &fl, &sub);
                    if(idx >= s->field_count)
                        return cc_error(p, desig_loc, "no member named '%.*s'",
                            field_tok.ident.ident->length, field_tok.ident.ident->data);
                    fi = idx + 1;
                    fl.byte_offset += base_offset;
                    err = cc_parse_desig_tail(p, &sub, &fl);
                    if(err) return err;
                    err = cc_parse_init_value(p, vc,sub, fl, 0, desig_loc, buf);
                    if(err) return err;
                    err = cc_init_list_comma(p);
                    if(err) return err;
                    continue;
                }
            }
            else {
                // Brace elision: check if we should stop
                _Bool stop;
                err = cc_is_parent_token(p, &stop);
                if(err) return err;
                if(stop) break;
            }
            // Positional: skip methods and anonymous bitfield padding
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
            CcFieldLoc fl = {
                .byte_offset = base_offset + fld->offset,
                .bit_offset = fld->is_bitfield ? fld->bitoffset : 0,
                .bit_width = fld->is_bitfield ? fld->bitwidth : 0,
            };
            err = cc_parse_init_value(p, vc,fld->type, fl, 1, loc, buf);
            if(err) return err;
            fi++;
            // Advance past padding fields so fi reflects next real field
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
                cc_next_token(p, &peek); // consume '.'
                SrcLoc desig_loc = peek.loc;
                CcToken field_tok;
                err = cc_next_token(p, &field_tok);
                if(err) return err;
                if(field_tok.type != CC_IDENTIFIER)
                    return cc_error(p, field_tok.loc, "expected field name after '.'");
                CcFieldLoc fl;
                CcQualType sub;
                if(!cc_lookup_field(u->fields, u->field_count, field_tok.ident.ident, &fl, &sub, NULL))
                    return cc_error(p, desig_loc, "no member named '%.*s'", field_tok.ident.ident->length, field_tok.ident.ident->data);
                fl.byte_offset += base_offset;
                err = cc_parse_desig_tail(p, &sub, &fl);
                if(err) return err;
                err = cc_parse_init_value(p, vc,sub, fl, 0, desig_loc, buf);
                if(err) return err;
                err = cc_init_list_comma(p);
                if(err) return err;
                return cc_expect_punct(p, CC_rbrace);
            }
        }
        // Positional: first named or non-bitfield non-method field
        CcField* field = NULL;
        for(uint32_t fi = 0; fi < u->field_count; fi++){
            if(u->fields[fi].name || (!u->fields[fi].is_bitfield && !u->fields[fi].is_method)){
                field = &u->fields[fi];
                break;
            }
        }
        if(!field)
            return cc_error(p, loc, "initializer for empty union");
        CcFieldLoc fl = {
            .byte_offset = base_offset + field->offset,
            .bit_offset = field->is_bitfield ? field->bitoffset : 0,
            .bit_width = field->is_bitfield ? field->bitwidth : 0,
        };
        if(first_value){
            CcExpr* fv = first_value;
            first_value = NULL;
            err = cc_init_apply_value(p, vc,field->type, fl, fv, loc, buf);
        }
        else
            err = cc_parse_init_value(p, vc,field->type, fl, 1, loc, buf);
        if(err) return err;
        if(braced){
            err = cc_init_list_comma(p);
            if(err) return err;
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
        if(arr->is_vector){
            uint32_t length = (uint32_t)arr->length;
            uint32_t ai = 0;
            for(;;){
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
                uint64_t elem_offset = base_offset + ai * elem_size;
                CcExpr* v;
                err = cc_parse_scalar_value(p, vc, &v);
                if(err) return err;
                err = cc_push_scalar(p, v, elem, (CcFieldLoc){.byte_offset = elem_offset}, buf);
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
            if(first_value){
                CcExpr* fv = first_value;
                if(!arr->is_incomplete && ai >= arr->length) break;
                CcFieldLoc fl = {.byte_offset = base_offset + ai * elem_size};
                err = cc_init_apply_value(p, vc,elem, fl, fv, loc, buf);
                if(err) return err;
                first_value = NULL;
                ai++;
                if(ai > max_ai) max_ai = ai;
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
                    cc_next_token(p, &peek); // consume '['
                    SrcLoc desig_loc = peek.loc;
                    CcExpr* idx_expr;
                    err = cc_parse_assignment_expr(p, CC_CONSTEXPR_VALUE, &idx_expr);
                    if(err) return err;
                    CcEvalResult ev;
                    err = cc_eval_expr(p,idx_expr,&ev);
                    cc_release_expr(p, idx_expr);
                    if(err)
                        return cc_error(p, desig_loc, "array designator must be a constant expression");
                    int64_t idx_signed;
                    switch(ev.kind){
                        DEFAULT_UNREACHABLE;
                        case CC_EVAL_INT:    idx_signed = ev.i; break;
                        case CC_EVAL_UINT:   idx_signed = (int64_t)ev.u; break;
                        case CC_EVAL_FLOAT:
                        case CC_EVAL_DOUBLE: return cc_error(p, desig_loc, "array designator must be an integer");
                        case CC_EVAL_TYPE: case CC_EVAL_VOID: case CC_EVAL_INIT_LIST: case CC_EVAL_STRING:   return cc_error(p, desig_loc, "array designator must be a constant expression");
                    }
                    if(idx_signed < 0 || idx_signed > UINT32_MAX)
                        return cc_error(p, desig_loc, "array designator value out of range");
                    uint32_t idx = (uint32_t)idx_signed;
                    err = cc_expect_punct(p, CC_rbracket);
                    if(err) return err;
                    if(!arr->is_incomplete && idx >= arr->length)
                        return cc_error(p, desig_loc, "array index %u out of bounds (size %zu)", idx, arr->length);
                    ai = idx + 1;
                    if(ai > max_ai) max_ai = ai;
                    CcQualType sub = elem;
                    CcFieldLoc fl = {.byte_offset = base_offset + idx * elem_size};
                    err = cc_parse_desig_tail(p, &sub, &fl);
                    if(err) return err;
                    err = cc_parse_init_value(p, vc,sub, fl, 0, desig_loc, buf);
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
            CcFieldLoc fl = {.byte_offset = base_offset + ai * elem_size};
            err = cc_parse_init_value(p, vc,elem, fl, 1, loc, buf);
            if(err) return err;
            ai++;
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
    case CC_FUNCTION:
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
    // Handle scalar types: {value} or {{{value}}}
    if(tk == CC_BASIC || tk == CC_POINTER || tk == CC_ENUM){
        CcToken peek;
        err = cc_peek(p, &peek);
        if(err) return err;
        if(peek.type == CC_PUNCTUATOR && peek.punct.punct == CC_rbrace){
            // Empty scalar init: {}, count=0
            cc_next_token(p, &peek);
            list = Allocator_alloc(cc_allocator(p), sizeof(CcInitList));
            if(!list) return CC_OOM_ERROR;
            list->loc = loc;
            list->count = 0;
        }
        else {
            // Check for designator — not allowed in scalar init
            if(peek.type == CC_PUNCTUATOR && (peek.punct.punct == '.' || peek.punct.punct == CC_lbracket))
                return cc_error(p, peek.loc, "designators not allowed in scalar initializer");
            CcExpr* v;
            err = cc_parse_scalar_value(p, vc, &v);
            if(err) return err;
            CcQualType t = target_type;
            t.is_const = 0; t.is_volatile = 0; t.is_atomic = 0;
            err = cc_implicit_cast(p, v, t, &v);
            if(err) return err;
            // Consume optional trailing comma
            err = cc_peek(p, &peek);
            if(err) return err;
            if(peek.type == CC_PUNCTUATOR && peek.punct.punct == ','){
                cc_next_token(p, &peek);
                // Check for excess elements
                err = cc_peek(p, &peek);
                if(err) return err;
                if(!(peek.type == CC_PUNCTUATOR && peek.punct.punct == CC_rbrace))
                    return cc_error(p, peek.loc, "excess elements in scalar initializer");
            }
            err = cc_expect_punct(p, CC_rbrace);
            if(err) return err;
            list = Allocator_alloc(cc_allocator(p), sizeof(CcInitList) + sizeof(CcInitEntry));
            if(!list) return CC_OOM_ERROR;
            list->loc = loc;
            list->count = 1;
            list->entries[0] = (CcInitEntry){.field_loc = {0}, .value = v};
        }
    }
    else {
        Marray(CcInitEntry) entries = {0};
        uint32_t max_index = 0;
        err = cc_parse_init(p, vc,target_type, 0, 1, loc, &entries, &max_index, NULL);
        if(err) return err;
        list = Allocator_alloc(cc_allocator(p), sizeof(CcInitList) + entries.count * sizeof(CcInitEntry));
        if(!list) return CC_OOM_ERROR;
        list->loc = loc;
        list->count = (uint32_t)entries.count;
        for(size_t i = 0; i < entries.count; i++) list->entries[i] = entries.data[i];
        if(tk == CC_ARRAY){
            CcArray* arr = ccqt_as_array(target_type);
            if(arr->is_incomplete){
                CcArray* new_arr = cc_intern_array(&p->type_cache, cc_allocator(p), arr->element, max_index, arr->is_static, 0, 0, 0);
                if(!new_arr) return CC_OOM_ERROR;
                resolved_type = (CcQualType){.bits = (uintptr_t)new_arr | (target_type.quals)};
            }
        }
    }
    CcExpr* node = cc_make_expr(p, CC_EXPR_INIT_LIST, loc, resolved_type, 0);
    if(!node) return CC_OOM_ERROR;
    node->init_list = list;
    *out = node;
    return 0;
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
            if(cc_has_field(existing, existing_count, f->method->name))
                return cc_error(p, loc, "duplicate member '%s'", f->method->name->data);
        }
        else if(f->name){
            if(cc_has_field(existing, existing_count, f->name))
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
        err = cc_next_token(p, &tok); // consume '{'
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
                }
            }
        }
        Marray(CcField) fields_arr = {0};
        for(;;){
            err = cc_peek(p, &tok);
            if(err) goto struct_err;
            if(tok.type == CC_PUNCTUATOR && tok.punct.punct == '}')
                break;
            if(tok.type == CC_KEYWORD && tok.kw.kw == CC_static_assert){
                err = cc_handle_static_asssert(p);
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
                    err = cc_parse_assignment_expr(p, CC_CONSTEXPR_VALUE, &bw_expr);
                    if(err) goto struct_err;
                    if(!bw_expr){
                        err = cc_error(p, tok.loc, "expected constant expression for bitfield width");
                        goto struct_err;
                    }
                    CcEvalResult bw_val;
                    err = cc_eval_expr(p,bw_expr,&bw_val);
                    cc_release_expr(p, bw_expr);
                    if(err){
                        err = cc_error(p, tok.loc, "bitfield width must be a constant expression");
                        goto struct_err;
                    }
                    if(bw_val.kind != CC_EVAL_INT && bw_val.kind != CC_EVAL_UINT){
                        err = cc_error(p, tok.loc, "bitfield width must be an integral constant expression");
                        goto struct_err;
                    }
                    bitwidth = bw_val.u;
                    member_type = member_base.type;
                    is_bitfield = 1;
                    if(!(ccqt_is_basic(member_type) && ccbt_is_integer(member_type.basic.kind))
                       && ccqt_kind(member_type) != CC_ENUM){
                        err = cc_error(p, tok.loc, "bitfield must have integer or enum type");
                        goto struct_err;
                    }
                    uint32_t type_size;
                    err = cc_sizeof_as_uint(p, member_type, tok.loc, &type_size);
                    if(err) goto struct_err;
                    if(bitwidth > type_size * 8){
                        err = cc_error(p, tok.loc, "bitfield width (%llu) exceeds size of type (%u bits)", (unsigned long long)bitwidth, type_size * 8);
                        goto struct_err;
                    }
                }
                else {
                    // Parse declarator
                    CcQualType head = {0};
                    CcQualType* tail = &head;
                    Marray(Atom) param_names = {0};
                    err = cc_parse_declarator(p, &head, &tail, &member_name, &param_names);
                    if(err){
                        ma_cleanup(Atom)(&param_names, cc_allocator(p));
                        goto struct_err;
                    }
                    *tail = member_base.type;
                    member_type = cc_intern_qualtype(p, head);
                    // Method: member type is a function type (not pointer to function)
                    if(ccqt_kind(member_type) == CC_FUNCTION){
                        CcFunc* func = Allocator_zalloc(cc_allocator(p), sizeof *func);
                        if(!func){ ma_cleanup(Atom)(&param_names, cc_allocator(p)); err = CC_OOM_ERROR; goto struct_err; }
                        func->name = member_name;
                        func->type = ccqt_as_function(member_type);
                        func->loc = tok.loc;
                        func->params.count = param_names.count;
                        func->params.data = param_names.data;
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
                            func->tokens = body_tokens;
                            func->defined = 1;
                        }
                        // Parse optional attributes after method
                        err = cc_parse_attributes(p, &member_attrs);
                        if(err) goto struct_err;
                        cc_clear_attributes(&p->attributes);
                        if(cc_has_field(fields_arr.data, (uint32_t)fields_arr.count, func->name)){
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
                    ma_cleanup(Atom)(&param_names, cc_allocator(p));

                    // Check for bitfield
                    err = cc_peek(p, &tok);
                    if(err) goto struct_err;
                    if(tok.type == CC_PUNCTUATOR && tok.punct.punct == ':'){
                        err = cc_next_token(p, &tok); // consume ':'
                        if(err) goto struct_err;
                        CcExpr* bw_expr = NULL;
                        err = cc_parse_assignment_expr(p, CC_CONSTEXPR_VALUE, &bw_expr);
                        if(err) goto struct_err;
                        if(!bw_expr){
                            err = cc_error(p, tok.loc, "expected constant expression for bitfield width");
                            goto struct_err;
                        }
                        CcEvalResult bw_val;
                        err = cc_eval_expr(p,bw_expr,&bw_val);
                        cc_release_expr(p, bw_expr);
                        if(err){
                            err = cc_error(p, tok.loc, "bitfield width must be a constant expression");
                            goto struct_err;
                        }
                        if(bw_val.kind != CC_EVAL_INT && bw_val.kind != CC_EVAL_UINT){
                            err = cc_error(p, tok.loc, "bitfield width must be an integral constant expression");
                            goto struct_err;
                        }
                        bitwidth = bw_val.u;
                        is_bitfield = 1;
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
                        if(bitwidth > type_size * 8){
                            err = cc_error(p, tok.loc, "bitfield width (%llu) exceeds size of type (%u bits)", (unsigned long long)bitwidth, type_size * 8);
                            goto struct_err;
                        }
                    }
                }
                // Parse optional attributes after the declarator
                err = cc_parse_attributes(p, &member_attrs);
                if(err) goto struct_err;
                cc_clear_attributes(&p->attributes);
                // Create field
                if(member_name && cc_has_field(fields_arr.data, (uint32_t)fields_arr.count, member_name)){
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
int
cc_parse_enum(CcParser* p, SrcLoc loc, CcQualType* base_type){
    int err = 0;
    CcToken tok;
    Atom name = NULL;
    CcQualType underlying = ccqt_basic(CCBT_int);
    _Bool has_fixed_underlying = 0;
    // Optional tag name
    err = cc_peek(p, &tok);
    if(err) return err;
    if(tok.type == CC_IDENTIFIER){
        err = cc_next_token(p, &tok);
        if(err) return err;
        name = tok.ident.ident;
    }
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
        if(ub.spec.sp_typebits != ub.spec.bits)
            return cc_error(p, loc, "Underlying type does not allow non-type specifiers");
        if(ub.spec.sp_infer_type)
            return cc_error(p, loc, "__auto_type not allowed as underlying type of enum");
        if(!ub.spec.bits && !ub.type.bits)
            return cc_error(p, loc, "Expected type specifier for enum underlying type");
        err = cc_resolve_specifiers(p, &ub);
        if(err) return err;
        if(!ccqt_is_basic(ub.type) || !ccbt_is_integer(ub.type.basic.kind))
            return cc_error(p, loc, "enum underlying type must be an integer type");
        underlying = ub.type;
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
                if(!existing->is_incomplete)
                    return cc_error(p, loc, "Redefinition of enum '%s'", name->data);
                if(existing->underlying.bits && existing->underlying.bits != underlying.bits)
                    return cc_error(p, loc, "Redefinition of enum '%s' with differing underlying types", name->data);
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
        CcQualType enum_type = {.bits = (uintptr_t)e};
        // Parse enumerator list
        Parray enumerators = {0};
        int64_t next_value = 0;
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
            SrcLoc eloc = tok.loc;
            if(cc_scope_lookup_enumerator(p->current, ename, CC_SCOPE_NO_WALK)){
                err = cc_error(p, eloc, "Redefinition of enumerator '%s'", ename->data);
                goto enum_err;
            }
            // Optional = constant-expression
            err = cc_peek(p, &tok);
            if(err) goto enum_err;
            if(tok.type == CC_PUNCTUATOR && tok.punct.punct == '='){
                err = cc_next_token(p, &tok); // consume '='
                if(err) goto enum_err;
                CcExpr* expr = NULL;
                err = cc_parse_assignment_expr(p, CC_CONSTEXPR_VALUE, &expr);
                if(err) goto enum_err;
                if(!expr){
                    err = cc_error(p, tok.loc, "expected constant expression");
                    goto enum_err;
                }
                CcEvalResult val;
                err = cc_eval_expr(p,expr,&val);
                cc_release_expr(p, expr);
                if(err){
                    err = cc_error(p, tok.loc, "enumerator value is not a constant expression");
                    goto enum_err;
                }
                switch(val.kind){
                    case CC_EVAL_INT:  next_value = val.i; break;
                    case CC_EVAL_UINT: next_value = (int64_t)val.u; break;
                    case CC_EVAL_FLOAT:
                    case CC_EVAL_DOUBLE:
                        err = cc_error(p, tok.loc, "Enumerator value is a floating point value");
                        goto enum_err;
                    case CC_EVAL_TYPE: case CC_EVAL_VOID: case CC_EVAL_INIT_LIST: case CC_EVAL_STRING:
                        err = cc_error(p, tok.loc, "enumerator value is not a constant expression");
                        goto enum_err;
                }
            }
            CcEnumerator* enumerator = Allocator_zalloc(cc_allocator(p), sizeof *enumerator);
            *enumerator = (CcEnumerator){
                .name = ename,
                .value = next_value,
                .type = has_fixed_underlying? enum_type : underlying,
                .loc = eloc,
            };
            err = cc_scope_insert_enumerator(cc_allocator(p), p->current, ename, enumerator);
            if(err){ err = CC_OOM_ERROR; goto enum_err; }
            err = pa_push(&enumerators, cc_allocator(p), enumerator);
            if(err){ err = CC_OOM_ERROR; goto enum_err; }
            next_value++;
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
        // Finalize enumerators
        err = pa_shrink_to_size(&enumerators, cc_allocator(p));
        if(err) goto enum_err;
        e->enumerators = (CcEnumerator**)enumerators.data;
        e->enumerator_count = enumerators.count;
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
    CcEnum* e = cc_scope_lookup_enum_tag(p->current, name, CC_SCOPE_WALK_CHAIN);
    if(!e){
        // Forward declaration — create incomplete enum
        e = Allocator_zalloc(cc_allocator(p), sizeof *e);
        if(!e) return CC_OOM_ERROR;
        *e = (CcEnum){
            .kind = CC_ENUM,
            .name = name,
            .loc = loc,
            .underlying = underlying,
            .is_incomplete = 1,
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
                        if(spec->sp_long > 1)
                            return cc_error(p, tok.loc, "Duplicate long after long long in declaration");
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
                    case CC__Type:
                        if(base_type->bits)
                            return cc_error(p, tok.loc, "Second type in declaration");
                        if(spec->sp_typebits)
                            return cc_error(p, tok.loc, "Second type in declaration");
                        *base_type = ccqt_basic(CCBT__Type);
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
                            err = cc_parse_type_name(p, &align_type);
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
                                case CC_POINTER:
                                    align_val = cc_target(p)->alignof_[CCBT_nullptr_t];
                                    break;
                                default:
                                    return cc_error(p, tok.loc, "_Alignas with this type not yet supported");
                            }
                        }
                        else {
                            CcExpr* expr = NULL;
                            err = cc_parse_assignment_expr(p, CC_CONSTEXPR_VALUE, &expr);
                            if(err) return err;
                            if(!expr)
                                return cc_error(p, tok.loc, "expected expression in _Alignas");
                            CcEvalResult val;
                            err = cc_eval_expr(p,expr,&val);
                            cc_release_expr(p, expr);
                            if(err)
                                return cc_error(p, tok.loc, "_Alignas requires a constant expression");
                            int64_t av;
                            switch(val.kind){
                                DEFAULT_UNREACHABLE;
                                case CC_EVAL_INT:    av = val.i; break;
                                case CC_EVAL_UINT:   av = (int64_t)val.u; break;
                                case CC_EVAL_FLOAT:
                                case CC_EVAL_DOUBLE: return cc_error(p, tok.loc, "_Alignas requires an integer expression");
                                case CC_EVAL_TYPE: case CC_EVAL_VOID: case CC_EVAL_INIT_LIST: case CC_EVAL_STRING:   return cc_error(p, tok.loc, "_Alignas requires a constant expression");
                            }
                            if(av < 0)
                                return cc_error(p, tok.loc, "_Alignas value must be non-negative");
                            if(av != 0 && (av & (av - 1)) != 0)
                                return cc_error(p, tok.loc, "_Alignas value must be zero or a power of 2");
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
                    case CC__Atomic:
                        return cc_unimplemented(p, tok.loc, "_Atomic parsing in declaration");
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
                    case CC___attribute__: {
                        err = cc_unget(p, &tok);
                        if(err) return err;
                        err = cc_parse_attributes(p, &p->attributes);
                        if(err) return err;
                        if(p->attributes.has_aligned){
                            if(p->attributes.aligned > base->alignment)
                                base->alignment = p->attributes.aligned;
                            p->attributes.has_aligned = 0;
                            p->attributes.aligned = 0;
                        }
                        continue;
                    }
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
                            err = cc_parse_type_name(p, base_type);
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
                        if(unqual){
                            base_type->is_const = 0;
                            base_type->is_volatile = 0;
                            base_type->is_atomic = 0;
                        }
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
            case CC_PUNCTUATOR:
                return cc_unget(p, &tok);
        }
    }
    return 0;
}

// Resolve goto labels to statement indices.
// Must be called after the full body is parsed.
static
int
cc_resolve_gotos(CcParser* p, CcStatement* stmts, size_t count, const AtomMap(uintptr_t)* labels){
    for(size_t i = 0; i < count; i++){
        CcStatement* s = &stmts[i];
        if(s->kind != CC_STMT_GOTO) continue;
        if(!s->goto_label) continue;
        Atom label = s->goto_label;
        void* v = AM_get(labels, label);
        if(!v)
            return cc_error(p, s->loc, "Use of undeclared label '%.*s'", label->length, label->data);
        s->targets[0] = (uint32_t)((uintptr_t)v - 1);
        s->goto_label = NULL; // clear the temp
    }
    return 0;
}

static
int
cc_stmt(CcParser* p, CcStmtKind k, SrcLoc loc, size_t* idx){
    int err;
    CcStatement *s;
    Marray(CcStatement) *stmts = p->current_func?&p->current_func->body : &p->toplevel_statements;
    err = ma_zalloc(CcStatement)(stmts, cc_allocator(p), &s);
    if(err) return CC_OOM_ERROR;
    *idx = (size_t)(s - stmts->data);
    s->kind = k;
    s->loc = loc;
    return 0;
}

static
CcStatement*_Nullable
cc_get_stmt(CcParser* p , size_t idx){
    Marray(CcStatement) *stmts = p->current_func?&p->current_func->body : &p->toplevel_statements;
    if(idx >= stmts->count) return NULL;
    return &stmts->data[idx];
}

static
void
cc_backpatch_break_continue(CcParser* p, size_t body_start, uint32_t break_target, uint32_t continue_target){
    Marray(CcStatement)* stmts = p->current_func
        ? &p->current_func->body
        : &p->toplevel_statements;
    for(size_t i = body_start; i < stmts->count; i++){
        CcStatement* s = &stmts->data[i];
        if(s->kind == CC_STMT_BREAK){
            s->kind = CC_STMT_GOTO;
            s->targets[0] = break_target;
        }
        else if(s->kind == CC_STMT_CONTINUE){
            s->kind = CC_STMT_GOTO;
            s->targets[0] = continue_target;
        }
    }
}

static
void
cc_backpatch_break(CcParser* p, size_t body_start, uint32_t break_target){
    Marray(CcStatement)* stmts = p->current_func
        ? &p->current_func->body
        : &p->toplevel_statements;
    for(size_t i = body_start; i < stmts->count; i++){
        CcStatement* s = &stmts->data[i];
        if(s->kind == CC_STMT_BREAK){
            s->kind = CC_STMT_GOTO;
            s->targets[0] = break_target;
        }
    }
}

typedef struct CcSwitchCtx CcSwitchCtx;
struct CcSwitchCtx {
    Marray(CcSwitchEntry) entries;
    uint32_t default_target;
    _Bool has_default;
};

static
int
cc_cmp_switch_entry(void*_Null_unspecified ctx, const void* a, const void* b){
    (void)ctx;
    const CcSwitchEntry* ea = a;
    const CcSwitchEntry* eb = b;
    if(ea->value < eb->value) return -1;
    if(ea->value > eb->value) return 1;
    return 0;
}

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
cc_parse_statement(CcParser* p){
    int err;
    CcToken tok;
    size_t stmt_idx;
    err = cc_next_token(p, &tok);
    if(err) return err;
    switch(tok.type){
        case CC_EOF:
            return 0;
        case CC_KEYWORD:
            switch(tok.kw.kw){
                case CC_for: {
                    // for(init; cond; inc) body
                    //
                    // Lowered to:
                    //   [init decl or expr stmt]  -- handled inline
                    //   N: CC_STMT_FOR            -- cond check, targets[0]=EXIT
                    //   N+1..M: body stmts
                    //   M+1: CC_STMT_EXPR(inc)    -- if inc exists
                    //   M+2: CC_STMT_GOTO(N)      -- back-edge
                    //   EXIT: next stmt
                    err = cc_expect_punct(p, '(');
                    if(err) return err;
                    // for introduces a new scope for init declarations
                    err = cc_push_scope(p);
                    if(err) return err;
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
                            CcDeclBase b = {0};
                            err = cc_parse_declaration_specifier(p, &b);
                            if(err) goto for_end;
                            if(b.spec.bits || b.type.bits){
                                // Declaration init: cc_parse_decls handles
                                // multiple declarators and consumes the ';'
                                err = cc_resolve_specifiers(p, &b);
                                if(!err) err = cc_parse_decls(p, &b);
                                if(err) goto for_end;
                            }
                            else {
                                // Expression init
                                CcExpr* init_expr;
                                err = cc_parse_expr(p, CC_RUNTIME_VALUE, &init_expr);
                                if(err) goto for_end;
                                err = cc_expect_punct(p, ';');
                                if(err) goto for_end;
                                size_t init_idx;
                                err = cc_stmt(p, CC_STMT_EXPR, tok.loc, &init_idx);
                                if(err) goto for_end;
                                CcStatement* init_s = cc_get_stmt(p, init_idx);
                                if(!init_s){ err = CC_UNREACHABLE_ERROR; goto for_end; }
                                init_s->exprs[0] = init_expr;
                            }
                        }
                    }
                    // Parse cond
                    CcExpr* _Nullable cond_expr = NULL;
                    {
                        CcToken peek;
                        err = cc_peek(p, &peek);
                        if(err) goto for_end;
                        if(!(peek.type == CC_PUNCTUATOR && peek.punct.punct == ';')){
                            err = cc_parse_expr(p, CC_RUNTIME_VALUE, &cond_expr);
                            if(err) goto for_end;
                            err = cc_require_scalar(p, (CcExpr*)cond_expr, tok.loc, "'for' condition");
                            if(err) goto for_end;
                        }
                        err = cc_expect_punct(p, ';');
                        if(err) goto for_end;
                    }
                    // Parse inc
                    CcExpr* _Nullable inc_expr = NULL;
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
                    // Emit CC_STMT_FOR (cond check)
                    size_t for_idx;
                    err = cc_stmt(p, CC_STMT_FOR, tok.loc, &for_idx);
                    if(err) goto for_end;
                    {
                        CcStatement* for_s = cc_get_stmt(p, for_idx);
                        if(!for_s){ err = CC_UNREACHABLE_ERROR; goto for_end; }
                        for_s->exprs[1] = cond_expr;
                    }
                    // Parse body
                    p->loop_depth++;
                    err = cc_parse_statement(p);
                    p->loop_depth--;
                    if(err) goto for_end;
                    // continue target = inc (or back-edge goto if no inc)
                    {
                        Marray(CcStatement)* stmts = p->current_func
                            ? &p->current_func->body
                            : &p->toplevel_statements;
                        uint32_t continue_target = (uint32_t)stmts->count;
                        // Emit inc as expr stmt (if present)
                        if(inc_expr){
                            size_t inc_idx;
                            err = cc_stmt(p, CC_STMT_EXPR, tok.loc, &inc_idx);
                            if(err) goto for_end;
                            CcStatement* inc_s = cc_get_stmt(p, inc_idx);
                            if(!inc_s){ err = CC_UNREACHABLE_ERROR; goto for_end; }
                            inc_s->exprs[0] = inc_expr;
                        }
                        // Emit goto back to for cond check
                        {
                            size_t goto_idx;
                            err = cc_stmt(p, CC_STMT_GOTO, tok.loc, &goto_idx);
                            if(err) goto for_end;
                            CcStatement* goto_s = cc_get_stmt(p, goto_idx);
                            if(!goto_s){ err = CC_UNREACHABLE_ERROR; goto for_end; }
                            goto_s->targets[0] = (uint32_t)for_idx;
                        }
                        // Backpatch for's targets[0] = exit (current position)
                        uint32_t break_target = (uint32_t)stmts->count;
                        CcStatement* for_s = cc_get_stmt(p, for_idx);
                        if(!for_s){ err = CC_UNREACHABLE_ERROR; goto for_end; }
                        for_s->targets[0] = break_target;
                        // Backpatch break/continue in body
                        cc_backpatch_break_continue(p, for_idx + 1, break_target, continue_target);
                    }
                    for_end:
                    cc_pop_scope(p);
                    return err;
                }
                case CC_while: {
                    // while(cond) body
                    //
                    //   N: CC_STMT_WHILE       -- cond check, targets[0]=EXIT
                    //   N+1..M: body stmts
                    //   M+1: CC_STMT_GOTO(N)   -- back-edge
                    //   EXIT: next stmt
                    err = cc_expect_punct(p, '(');
                    if(err) return err;
                    CcExpr* cond;
                    err = cc_parse_expr(p, CC_RUNTIME_VALUE, &cond);
                    if(err) return err;
                    err = cc_require_scalar(p, cond, tok.loc, "'while' condition");
                    if(err) return err;
                    err = cc_expect_punct(p, ')');
                    if(err) return err;
                    size_t while_idx;
                    err = cc_stmt(p, CC_STMT_WHILE, tok.loc, &while_idx);
                    if(err) return err;
                    {
                        CcStatement* ws = cc_get_stmt(p, while_idx);
                        if(!ws) return CC_UNREACHABLE_ERROR;
                        ws->exprs[0] = cond;
                    }
                    p->loop_depth++;
                    err = cc_parse_statement(p);
                    p->loop_depth--;
                    if(err) return err;
                    {
                        size_t goto_idx;
                        err = cc_stmt(p, CC_STMT_GOTO, tok.loc, &goto_idx);
                        if(err) return err;
                        CcStatement* gs = cc_get_stmt(p, goto_idx);
                        if(!gs) return CC_UNREACHABLE_ERROR;
                        gs->targets[0] = (uint32_t)while_idx;
                    }
                    {
                        Marray(CcStatement)* stmts = p->current_func
                            ? &p->current_func->body
                            : &p->toplevel_statements;
                        uint32_t break_target = (uint32_t)stmts->count;
                        CcStatement* ws = cc_get_stmt(p, while_idx);
                        if(!ws) return CC_UNREACHABLE_ERROR;
                        ws->targets[0] = break_target;
                        cc_backpatch_break_continue(p, while_idx + 1, break_target, (uint32_t)while_idx);
                    }
                    return 0;
                }
                case CC_do: {
                    // do body while(cond);
                    //
                    //   N..M: body stmts
                    //   M+1: CC_STMT_DOWHILE   -- cond check, targets[0]=N
                    //   M+2: next stmt
                    Marray(CcStatement)* stmts = p->current_func
                        ? &p->current_func->body
                        : &p->toplevel_statements;
                    size_t body_start = stmts->count;
                    p->loop_depth++;
                    err = cc_parse_statement(p);
                    p->loop_depth--;
                    if(err) return err;
                    {
                        CcToken wtok;
                        err = cc_next_token(p, &wtok);
                        if(err) return err;
                        if(wtok.type != CC_KEYWORD || wtok.kw.kw != CC_while)
                            return cc_error(p, wtok.loc, "Expected 'while' after do body");
                    }
                    err = cc_expect_punct(p, '(');
                    if(err) return err;
                    CcExpr* cond;
                    err = cc_parse_expr(p, CC_RUNTIME_VALUE, &cond);
                    if(err) return err;
                    err = cc_require_scalar(p, cond, tok.loc, "'do-while' condition");
                    if(err) return err;
                    err = cc_expect_punct(p, ')');
                    if(err) return err;
                    err = cc_expect_punct(p, ';');
                    if(err) return err;
                    size_t dw_idx;
                    err = cc_stmt(p, CC_STMT_DOWHILE, tok.loc, &dw_idx);
                    if(err) return err;
                    CcStatement* dws = cc_get_stmt(p, dw_idx);
                    if(!dws) return CC_UNREACHABLE_ERROR;
                    dws->exprs[0] = cond;
                    dws->targets[0] = (uint32_t)body_start;
                    cc_backpatch_break_continue(p, body_start, (uint32_t)(dw_idx + 1), (uint32_t)dw_idx);
                    return 0;
                }
                case CC_if: {
                    // if(cond) then-body [else else-body]
                    //
                    // Without else:
                    //   N: CC_STMT_IF          -- cond check, targets[0]=EXIT
                    //   N+1..M: then-body
                    //   EXIT: next stmt
                    //
                    // With else:
                    //   N: CC_STMT_IF          -- cond check, targets[0]=ELSE
                    //   N+1..M: then-body
                    //   M+1: CC_STMT_GOTO(EXIT) -- skip else
                    //   ELSE..X: else-body
                    //   EXIT: next stmt
                    err = cc_expect_punct(p, '(');
                    if(err) return err;
                    CcExpr* cond;
                    err = cc_parse_expr(p, CC_RUNTIME_VALUE, &cond);
                    if(err) return err;
                    err = cc_require_scalar(p, cond, tok.loc, "'if' condition");
                    if(err) return err;
                    err = cc_expect_punct(p, ')');
                    if(err) return err;
                    size_t if_idx;
                    err = cc_stmt(p, CC_STMT_IF, tok.loc, &if_idx);
                    if(err) return err;
                    {
                        CcStatement* ifs = cc_get_stmt(p, if_idx);
                        if(!ifs) return CC_UNREACHABLE_ERROR;
                        ifs->exprs[0] = cond;
                    }
                    err = cc_parse_statement(p);
                    if(err) return err;
                    // Check for else
                    CcToken peek;
                    err = cc_peek(p, &peek);
                    if(err) return err;
                    if(peek.type == CC_KEYWORD && peek.kw.kw == CC_else){
                        cc_next_token(p, &peek); // consume 'else'
                        // Emit goto to skip else body
                        size_t goto_idx;
                        err = cc_stmt(p, CC_STMT_GOTO, tok.loc, &goto_idx);
                        if(err) return err;
                        // Backpatch if's targets[0] = else start (current position)
                        {
                            Marray(CcStatement)* stmts = p->current_func
                                ? &p->current_func->body
                                : &p->toplevel_statements;
                            CcStatement* ifs = cc_get_stmt(p, if_idx);
                            if(!ifs) return CC_UNREACHABLE_ERROR;
                            ifs->targets[0] = (uint32_t)stmts->count;
                        }
                        err = cc_parse_statement(p);
                        if(err) return err;
                        // Backpatch goto's targets[0] = exit (current position)
                        {
                            Marray(CcStatement)* stmts = p->current_func
                                ? &p->current_func->body
                                : &p->toplevel_statements;
                            CcStatement* gs = cc_get_stmt(p, goto_idx);
                            if(!gs) return CC_UNREACHABLE_ERROR;
                            gs->targets[0] = (uint32_t)stmts->count;
                        }
                    }
                    else {
                        // No else: backpatch if's targets[0] = exit
                        Marray(CcStatement)* stmts = p->current_func
                            ? &p->current_func->body
                            : &p->toplevel_statements;
                        CcStatement* ifs = cc_get_stmt(p, if_idx);
                        if(!ifs) return CC_UNREACHABLE_ERROR;
                        ifs->targets[0] = (uint32_t)stmts->count;
                    }
                    return 0;
                }
                case CC_break: {
                    if(!p->loop_depth)
                        return cc_error(p, tok.loc, "'break' statement not in loop or switch statement");
                    err = cc_expect_punct(p, ';');
                    if(err) return err;
                    err = cc_stmt(p, CC_STMT_BREAK, tok.loc, &stmt_idx);
                    if(err) return err;
                    return 0;
                }
                case CC_continue: {
                    if(p->loop_depth <= p->switch_depth)
                        return cc_error(p, tok.loc, "'continue' statement not in loop statement");
                    err = cc_expect_punct(p, ';');
                    if(err) return err;
                    err = cc_stmt(p, CC_STMT_CONTINUE, tok.loc, &stmt_idx);
                    if(err) return err;
                    return 0;
                }
                case CC_switch: {
                    // switch(expr) { case V: ... default: ... }
                    //
                    // Lowered to:
                    //   N: CC_STMT_SWITCH  -- exprs[0]=expr, targets[0]=EXIT,
                    //                         targets[1]=default (or EXIT),
                    //                         targets[2]=table_count,
                    //                         switch_table=sorted entries
                    //   N+1..M: body stmts (case bodies, fall through)
                    //   EXIT: next stmt
                    err = cc_expect_punct(p, '(');
                    if(err) return err;
                    CcExpr* switch_expr;
                    err = cc_parse_expr(p, CC_RUNTIME_VALUE, &switch_expr);
                    if(err) return err;
                    {
                        CcQualType st = switch_expr->type;
                        if(!ccqt_is_basic(st) && ccqt_kind(st) == CC_ENUM)
                            st = ccqt_as_enum(st)->underlying;
                        if(!ccqt_is_basic(st) || !ccbt_is_integer(st.basic.kind))
                            return cc_error(p, tok.loc, "switch requires integer expression");
                    }
                    err = cc_expect_punct(p, ')');
                    if(err) return err;
                    size_t switch_idx;
                    err = cc_stmt(p, CC_STMT_SWITCH, tok.loc, &switch_idx);
                    if(err) return err;
                    {
                        CcStatement* sw = cc_get_stmt(p, switch_idx);
                        if(!sw) return CC_UNREACHABLE_ERROR;
                        sw->switch_expr = switch_expr;
                    }
                    // Parse switch body
                    {
                        CcSwitchCtx ctx = {0};
                        CcSwitchCtx* prev_ctx = p->switch_ctx;
                        p->switch_ctx = &ctx;
                        p->loop_depth++; // for break
                        p->switch_depth++;
                        err = cc_parse_statement(p);
                        p->switch_depth--;
                        p->loop_depth--;
                        p->switch_ctx = prev_ctx;
                        if(err) goto switch_cleanup;
                        // Sort entries by value
                        {
                            void* scratch = Allocator_alloc(cc_scratch_allocator(p), ctx.entries.count * sizeof(CcSwitchEntry));
                            if(!scratch && ctx.entries.count){ err = CC_OOM_ERROR; goto switch_cleanup; }
                            drp_merge_sort(scratch, ctx.entries.data, ctx.entries.count, sizeof(CcSwitchEntry), NULL, cc_cmp_switch_entry);
                            Allocator_free(cc_scratch_allocator(p), scratch, ctx.entries.count * sizeof(CcSwitchEntry));
                        }
                        // Check for duplicate case values
                        for(uint32_t i = 1; i < ctx.entries.count; i++){
                            if(ctx.entries.data[i].value == ctx.entries.data[i-1].value){
                                err = cc_error(p, tok.loc, "duplicate case value '%lld'", (long long)ctx.entries.data[i].value);
                                goto switch_cleanup;
                            }
                        }
                        // Shrink and steal the table
                        if(ctx.entries.count){
                            int serr = ma_shrink_to_size(CcSwitchEntry)(&ctx.entries, cc_allocator(p));
                            if(serr){ err = CC_OOM_ERROR; goto switch_cleanup; }
                        }
                        {
                            Marray(CcStatement)* stmts = p->current_func
                                ? &p->current_func->body
                                : &p->toplevel_statements;
                            uint32_t break_target = (uint32_t)stmts->count;
                            CcStatement* sw = cc_get_stmt(p, switch_idx);
                            if(!sw){ err = CC_UNREACHABLE_ERROR; goto switch_cleanup; }
                            sw->targets[0] = break_target;
                            sw->targets[1] = ctx.has_default ? ctx.default_target : break_target;
                            sw->targets[2] = (uint32_t)ctx.entries.count;
                            sw->switch_table = ctx.entries.data;
                            ctx.entries.data = NULL;
                            ctx.entries.count = 0;
                            ctx.entries.capacity = 0;
                            cc_backpatch_break(p, switch_idx + 1, break_target);
                        }
                        switch_cleanup:
                        ma_cleanup(CcSwitchEntry)(&ctx.entries, cc_allocator(p));
                    }
                    return err;
                }
                case CC_case: {
                    if(!p->switch_ctx)
                        return cc_error(p, tok.loc, "'case' label not within a switch statement");
                    CcExpr* case_expr;
                    err = cc_parse_expr(p, CC_CONSTEXPR_VALUE, &case_expr);
                    if(err) return err;
                    err = cc_expect_punct(p, ':');
                    if(err) return err;
                    CcEvalResult ev;
                    err = cc_eval_expr(p,case_expr,&ev);
                    cc_release_expr(p, case_expr);
                    if(err)
                        return cc_error(p, tok.loc, "case label must be a constant expression");
                    uint64_t case_val;
                    switch(ev.kind){
                        DEFAULT_UNREACHABLE;
                        case CC_EVAL_INT:  case_val = (uint64_t)ev.i; break;
                        case CC_EVAL_UINT: case_val = ev.u; break;
                        case CC_EVAL_FLOAT:
                        case CC_EVAL_DOUBLE:
                        case CC_EVAL_TYPE: case CC_EVAL_VOID: case CC_EVAL_INIT_LIST: case CC_EVAL_STRING:
                        return cc_error(p, tok.loc, "case label must be an integer constant expression");
                    }
                    Marray(CcStatement)* stmts = p->current_func
                        ? &p->current_func->body
                        : &p->toplevel_statements;
                    CcSwitchEntry entry = {.value = case_val, .target = (uint32_t)stmts->count};
                    err = ma_push(CcSwitchEntry)(&p->switch_ctx->entries, cc_allocator(p), entry);
                    if(err) return CC_OOM_ERROR;
                    return cc_parse_statement(p);
                }
                case CC_default: {
                    if(!p->switch_ctx)
                        return cc_error(p, tok.loc, "'default' label not within a switch statement");
                    err = cc_expect_punct(p, ':');
                    if(err) return err;
                    if(p->switch_ctx->has_default)
                        return cc_error(p, tok.loc, "Multiple default labels in switch");
                    p->switch_ctx->has_default = 1;
                    Marray(CcStatement)* stmts = p->current_func
                        ? &p->current_func->body
                        : &p->toplevel_statements;
                    p->switch_ctx->default_target = (uint32_t)stmts->count;
                    return cc_parse_statement(p);
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
                    if(err) return err;
                    err = cc_stmt(p, CC_STMT_RETURN, tok.loc, &stmt_idx);
                    if(err) return err;
                    CcStatement* s = cc_get_stmt(p, stmt_idx);
                    if(!s) return CC_UNREACHABLE_ERROR;
                    s->exprs[0] = ret_expr;
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
                    err = cc_stmt(p, CC_STMT_GOTO, tok.loc, &stmt_idx);
                    if(err) return err;
                    CcStatement* s = cc_get_stmt(p, stmt_idx);
                    if(!s) return CC_UNREACHABLE_ERROR;
                    s->goto_label = label_tok.ident.ident;
                    return 0;
                }
                case CC_sizeof:
                case CC_true:
                case CC_alignof:
                case CC_nullptr:
                case CC__Generic:
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
                case CC__Countof:
                case CC__Type:
                    return cc_error(p, tok.loc, "Unexpected keyword in this position");
                case CC___attribute__:
                    return cc_unimplemented(p, tok.loc, "TODO: __attribute__ as statement"); // __attribute__((fallthrough))
            }
            break;
        case CC_PUNCTUATOR:
            if(tok.punct.punct == '{'){
                err = cc_push_scope(p);
                if(err) return err;
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
                cc_pop_scope(p);
                return err;
            }
            if(tok.punct.punct == ';'){
                err = cc_stmt(p, CC_STMT_NULL, tok.loc, &stmt_idx);
                if(err) return err;
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
                err = cc_stmt(p, CC_STMT_LABEL, tok.loc, &stmt_idx);
                if(err) return err;
                AtomMap(uintptr_t)* labels = p->current_func
                    ? &p->current_func->labels
                    : &p->toplevel_labels;
                void* existing = AM_get(labels, label_name);
                if(existing)
                    return cc_error(p, tok.loc, "Duplicate label '%.*s'", label_name->length, label_name->data);
                err = AM_put(labels, cc_allocator(p), label_name, (void*)(uintptr_t)(stmt_idx + 1));
                if(err) return CC_OOM_ERROR;
                return cc_parse_statement(p);
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
            if(err) return err;
            err = cc_stmt(p, CC_STMT_EXPR, tok.loc, &stmt_idx);
            if(err) return err;
            CcStatement* s = cc_get_stmt(p, stmt_idx);
            if(!s) return CC_UNREACHABLE_ERROR;
            s->exprs[0] = expr;
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
    if(b.spec.sp_constexpr)
        b.spec.sp_const = 1;
    if(b.spec.sp_const) b.type.is_const = 1;
    if(b.spec.sp_volatile) b.type.is_volatile = 1;
    if(b.spec.sp_atomic) b.type.is_atomic = 1;
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
cc_parse_declarator(CcParser* p, CcQualType* out_head, CcQualType*_Nonnull*_Nonnull out_tail, Atom _Nullable * _Nullable out_name, Marray(Atom) *_Nullable out_param_names){
    int err = 0;
    CcToken tok;
    err = cc_next_token(p, &tok);
    if(err) return err;
    if(tok.type == CC_PUNCTUATOR && tok.punct.punct == '*'){
        // Pointer declarator: parse qualifiers, recurse, then splice.
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
            break;
        }
        err = cc_parse_declarator(p, out_head, out_tail, out_name, out_param_names);
        if(err) return err;
        CcPointer* ptr = Allocator_zalloc(cc_scratch_allocator(p), sizeof *ptr);
        if(!ptr) return CC_OOM_ERROR;
        ptr->kind = CC_POINTER;
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
        // '(' followed by '*', '(' or non-typedef identifier -> grouped declarator.
        // '(' followed by type keyword, ')', '...', or typedef name -> function params.
        CcToken peek;
        err = cc_peek(p, &peek);
        if(err) return err;
        _Bool grouped = peek.type == CC_PUNCTUATOR && (peek.punct.punct == '*' || peek.punct.punct == '(');
        if(!grouped && peek.type == CC_IDENTIFIER){
            // Identifier after '(' — grouped declarator unless it's a typedef
            // (not shadowed by a var/func in a closer scope).
            CcSymbol sym;
            grouped = !cc_scope_lookup_symbol(p->current, peek.ident.ident, CC_SCOPE_WALK_CHAIN, &sym) || sym.kind != CC_SYM_TYPEDEF;
        }
        if(grouped){
            err = cc_parse_declarator(p, out_head, out_tail, out_name, out_param_names);
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
            CcArray* arr = Allocator_zalloc(cc_scratch_allocator(p), sizeof *arr);
            if(!arr) return CC_OOM_ERROR;
            arr->kind = CC_ARRAY;
            // Parse optional qualifiers and 'static' inside [] (C99 §6.7.6.2)
            // e.g. int a[static restrict 10], int a[const], int a[restrict]
            CcToken peek;
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
                err = cc_parse_assignment_expr(p, CC_CONSTEXPR_VALUE, &dim);
                if(err) return err;
                if(!dim) return cc_error(p, tok.loc, "Expected array dimension");
                CcEvalResult val;
                err = cc_eval_expr(p,dim,&val);
                cc_release_expr(p, dim);
                if(err) return cc_unimplemented(p, tok.loc, "VLA array dimensions");
                int64_t length;
                switch(val.kind){
                    DEFAULT_UNREACHABLE;
                    case CC_EVAL_INT:    length = val.i; break;
                    case CC_EVAL_UINT:   length = (int64_t)val.u; break;
                    case CC_EVAL_FLOAT:
                    case CC_EVAL_DOUBLE: return cc_error(p, tok.loc, "array length must be an integer");
                    case CC_EVAL_TYPE: case CC_EVAL_VOID: case CC_EVAL_INIT_LIST: case CC_EVAL_STRING:   return cc_unimplemented(p, tok.loc, "VLA array dimensions");
                }
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
                err = cc_resolve_specifiers(p, &param_base);
                if(err) goto param_err;
                if(param_base.spec.sp_infer_type){
                    err = cc_error(p, peek.loc, "Expected type in function parameter");
                    goto param_err;
                }

                CcQualType param_head = {0};
                CcQualType* param_tail = &param_head;
                Atom param_name = NULL;
                err = cc_parse_declarator(p, &param_head, &param_tail, &param_name, NULL);
                if(err) goto param_err;
                *param_tail = param_base.type;
                if(ccqt_is_basic(param_head) && param_head.basic.kind == CCBT_void){
                    err = cc_error(p, peek.loc, "parameter cannot have void type");
                    goto param_err;
                }
                // consume trailing __attribute__ on parameter (e.g. __attribute__((unused)))
                {
                    CcAttributes param_attrs = {0};
                    err = cc_parse_attributes(p, &param_attrs);
                    if(err) goto param_err;
                }

                err = ma_push(CcQualType)(&param_types, cc_scratch_allocator(p), param_head);
                if(err){ err = CC_OOM_ERROR; goto param_err; }

                if(out_param_names){
                    if(param_name){
                        Marray(Atom)* pn = out_param_names;
                        for(size_t j = 0; j < pn->count; j++){
                            if(pn->data[j] == param_name){
                                err = cc_error(p, peek.loc, "duplicate parameter name '%.*s'", param_name->length, param_name->data);
                                goto param_err;
                            }
                        }
                    }
                    Marray(Atom)* pn = out_param_names;
                    err = ma_push(Atom)(pn, cc_allocator(p), param_name);
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
            CcFunction* func = cc_intern_function(&p->type_cache, cc_allocator(p), ret, old->params, old->param_count, old->is_variadic, old->no_prototype);
            if(!func) return t;
            return (CcQualType){.bits = (uintptr_t)func | quals};
        }
        default:
            return t;
    }
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
                err = cc_scope_insert_typedef(cc_allocator(p), p->current, tag_name, base);
                if(err) return err;
            }
        }
    }
    for(_Bool first = 1;;first=0){
        Atom name = NULL;
        CcQualType type;
        _Bool is_fndef = 0;
        CcExpr* initializer = NULL;
        Marray(Atom) param_names = {0};
        if(declbase->spec.sp_infer_type){
            err = cc_next_token(p, &tok);
            if(err) return err;
            if(tok.type != CC_IDENTIFIER)
                return cc_error(p, tok.loc, "Expected identifier for type-inferred declaration");
            name = tok.ident.ident;
            type = declbase->type;
        }
        else {
            CcQualType head = {0};
            CcQualType* tail = &head;
            err = cc_parse_declarator(p, &head, &tail, &name, &param_names);
            if(err){
                ma_cleanup(Atom)(&param_names, cc_allocator(p));
                return err;
            }
            // tail != &head means the declarator itself built derived types.
            // Only allow function bodies when the declarator introduced the
            // function type, not when it came from a typedef.
            is_fndef = tail != &head && ccqt_kind(head) == CC_FUNCTION;
            *tail = declbase->type;
            type = cc_intern_qualtype(p, head);
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
                if(tk == CC_FUNCTION){
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
            if(p->attributes.has_aligned && tk != CC_STRUCT && tk != CC_UNION)
                return cc_error(p, declbase->loc, "aligned attribute on non-struct/union type is not supported");
            if(p->attributes.packed && tk != CC_STRUCT)
                return cc_error(p, declbase->loc, "packed attribute on non-struct type is not supported");
            if(p->attributes.transparent_union && tk != CC_UNION)
                return cc_error(p, declbase->loc, "transparent_union attribute on non-union type is not supported");
        }
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
        // postfix processing
        _Bool stop = 0;
        err = cc_next_token(p, &tok);
        if(err) return err;
        if(first && tok.type == CC_PUNCTUATOR && tok.punct.punct == '{'){
            if(!is_fndef)
                return cc_error(p, tok.loc, "Expected ',' or ';'");
            _Bool eager = p->eager_parsing || p->current_func;
            if(!eager){
                // Collect tokens for lazy parsing
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
                // Lookup existing forward declaration
                CcFunc* func = cc_scope_lookup_func(p->current, name, CC_SCOPE_NO_WALK);
                if(func){
                    if(func->defined)
                        return cc_error(p, tok.loc, "Redefinition of function '%.*s'", name->length, name->data);
                    err = cc_check_func_compat(p, func, declbase, type, tok.loc);
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
                func->loc = tok.loc;
                func->mangle = asm_label;
                func->defined = 1;
                func->extern_ = declbase->spec.sp_extern;
                func->static_ = declbase->spec.sp_static;
                func->inline_ = declbase->spec.sp_inline;
                func->tokens = body_tokens;
                func->params.count = param_names.count;
                func->params.data = param_names.data;
                return 0;
            }
            // Eager parsing: parse body directly
            CcFunc* func = cc_scope_lookup_func(p->current, name, CC_SCOPE_NO_WALK);
            if(func){
                if(func->defined)
                    return cc_error(p, tok.loc, "Redefinition of function '%.*s'", name->length, name->data);
                err = cc_check_func_compat(p, func, declbase, type, tok.loc);
                if(err) return err;
            }
            else {
                func = Allocator_zalloc(cc_allocator(p), sizeof *func);
                if(!func) return CC_OOM_ERROR;
                func->name = name;
                err = cc_scope_insert_func(cc_allocator(p), p->current, name, func);
                if(err) return err;
            }
            CcFunction* ftype = ccqt_as_function(type);
            func->type = ftype;
            func->loc = tok.loc;
            func->mangle = asm_label;
            func->defined = 1;
            func->extern_ = declbase->spec.sp_extern;
            func->static_ = declbase->spec.sp_static;
            func->inline_ = declbase->spec.sp_inline;
            func->params.count = param_names.count;
            func->params.data = param_names.data;
            func->enclosing = p->current_func;
            err = cc_parse_func_body_inner(p, func, 1);
            if(err) return err;
            err = cc_expect_punct(p, CC_rbrace);
            if(err) return err;
            return 0;
        }
        // For non-typedef, non-function variable declarations, insert
        // the variable into scope before parsing the initializer so that
        // self-referential expressions like sizeof(*var) work.
        CcVariable* _Null_unspecified var = NULL;
        _Bool is_func_decl = is_fndef || (type.ptr && ccqt_kind(type) == CC_FUNCTION);
        if(name && !declbase->spec.sp_typedef && !is_func_decl){
            if(declbase->spec.sp_inline)
                return cc_error(p, tok.loc, "'inline' is only valid on functions");
            if(declbase->spec.sp_noreturn)
                return cc_error(p, tok.loc, "'_Noreturn' is only valid on functions");
            {
                CcVariable* existing = cc_scope_lookup_var(p->current, name, CC_SCOPE_NO_WALK);
                if(existing){
                    if(p->current != &p->global)
                        return cc_error(p, tok.loc, "redefinition of '%.*s'", name->length, name->data);
                    // merge tentative definitions
                    var = existing;
                    goto skip_var_alloc;
                }
            }
            var = Allocator_zalloc(cc_allocator(p), sizeof *var);
            if(!var) return CC_OOM_ERROR;
            *var = (CcVariable){
                .name = name,
                .mangle = asm_label,
                .loc = tok.loc,
                .type = type,
                .alignment = declbase->alignment,
                .extern_ = declbase->spec.sp_extern,
                .static_ = declbase->spec.sp_static,
                .constexpr_ = declbase->spec.sp_constexpr,
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
            else if(var && !var->automatic && var->static_)
                init_vc = CC_LINKTIME_VALUE;
            CcToken peek_init;
            err = cc_peek(p, &peek_init);
            if(err) return err;
            if(peek_init.type == CC_PUNCTUATOR && peek_init.punct.punct == CC_lbrace){
                err = cc_parse_init_list(p, init_vc, &initializer, type);
                if(err) return err;
            }
            else {
                err = cc_parse_assignment_expr(p, init_vc, &initializer);
                if(err) return err;
            }
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
                // Apply qualifiers from specifier
                if(declbase->spec.sp_const) type.is_const = 1;
                if(declbase->spec.sp_volatile) type.is_volatile = 1;
                if(declbase->spec.sp_atomic) type.is_atomic = 1;
            }
            else if(ccqt_kind(type) == CC_ARRAY
                    && ccqt_kind(initializer->type) == CC_ARRAY
                    && initializer->kind == CC_EXPR_VALUE && initializer->text){
                // String literal initializing a char array.
                CcArray* target_arr = ccqt_as_array(type);
                CcArray* init_arr = ccqt_as_array(initializer->type);
                if(target_arr->is_incomplete){
                    // char s[] = "abc" → size from string literal.
                    // Intern a new complete array with the original element type.
                    CcArray* arr = cc_intern_array(&p->type_cache, cc_allocator(p),
                        target_arr->element, init_arr->length,
                        target_arr->is_static, 0, 0, 0);
                    if(!arr) return CC_OOM_ERROR;
                    type = (CcQualType){.bits = (uintptr_t)arr | type.quals};
                }
                else if(target_arr->length < init_arr->length - 1){
                    return cc_error(p, tok.loc, "initializer string too long for array");
                }
                // else: char t[3] = "abc" is valid (drops null terminator)
            }
            else {
                // Check compatibility and insert implicit cast
                CcQualType target = type;
                target.is_const = 0;
                target.is_volatile = 0;
                target.is_atomic = 0;
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
            CcQualType existing_td = cc_scope_lookup_typedef(p->current, name, CC_SCOPE_NO_WALK);
            if(existing_td.bits && existing_td.bits != type.bits)
                return cc_error(p, tok.loc, "redefinition of typedef '%.*s' with different type", name->length, name->data);
            err = cc_scope_insert_typedef(cc_allocator(p), p->current, name, type);
            if(err) return err;
        }
        else if(is_func_decl){
            CcFunc* func = cc_scope_lookup_func(p->current, name, CC_SCOPE_NO_WALK);
            if(func){
                err = cc_check_func_compat(p, func, declbase, type, tok.loc);
                if(err) return err;
            }
            else {
                func = Allocator_zalloc(cc_allocator(p), sizeof *func);
                if(!func) return CC_OOM_ERROR;
                func->name = name;
                func->loc = tok.loc;
                err = cc_scope_insert_func(cc_allocator(p), p->current, name, func);
                if(err) return err;
            }
            func->type = ccqt_as_function(type);
            func->mangle = asm_label;
            func->extern_ = declbase->spec.sp_extern;
            func->static_ = declbase->spec.sp_static;
            func->inline_ = declbase->spec.sp_inline;
            if(!func->defined){
                func->params.count = param_names.count;
                func->params.data = param_names.data;
            }
        }
        else {
            // var was already created and inserted into scope above.
            // Reject incomplete types without initializer.
            if(!initializer && !declbase->spec.sp_extern){
                CcTypeKind tk = ccqt_kind(type);
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
                var->type = type;
                if(initializer)
                    var->initializer = initializer;
                if(var->automatic && p->current_func){
                    uint32_t sz, align;
                    err = cc_sizeof_as_uint(p, type, tok.loc, &sz);
                    if(err) return err;
                    err = cc_alignof_as_uint(p, type, tok.loc, &align);
                    if(err) return err;
                    if(var->alignment && var->alignment > align)
                        align = var->alignment;
                    p->current_func->frame_size = (p->current_func->frame_size + align - 1) & ~(align - 1);
                    var->frame_offset = p->current_func->frame_size;
                    p->current_func->frame_size += sz;
                }
            }
            if(initializer){
                if(var && !var->automatic){
                    err = PM_put(&p->used_vars, cc_allocator(p), var, var);
                    if(err) return CC_OOM_ERROR;
                }
                // Function-scope statics: ci_resolve_refs evaluates
                // the initializer once before execution (no re-init,
                // no thread races). Everything else: runtime assign.
                if(var && var->static_ && p->current_func){
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
                    size_t si;
                    err = cc_stmt(p, CC_STMT_EXPR, tok.loc, &si);
                    if(err) return err;
                    CcStatement* s = cc_get_stmt(p, si);
                    if(!s) return CC_UNREACHABLE_ERROR;
                    s->exprs[0] = assign;
                }
            }
        }
        if(stop) break;
    }
    return err;
}
static
const CcTargetConfig*
cc_target(const CcParser* p){
    return &p->cpp.target;
}

static
int
cc_handle_static_asssert(CcParser* p){
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
    err = cc_parse_assignment_expr(p, CC_CONSTEXPR_VALUE, &expr);
    if(err) return err;
    if(!expr)
        return cc_error(p, assert_loc, "expected expression in static_assert");
    CcEvalResult val;
    err = cc_eval_expr(p,expr,&val);
    if(err){
        cc_release_expr(p, expr);
        return cc_error(p, assert_loc, "static_assert expression is not a constant expression");
    }
    _Bool sa_truthy;
    switch(val.kind){
        case CC_EVAL_INT:    sa_truthy = val.i != 0; break;
        case CC_EVAL_UINT:   sa_truthy = val.u != 0; break;
        case CC_EVAL_FLOAT:  sa_truthy = val.f != 0; break;
        case CC_EVAL_DOUBLE: sa_truthy = val.d != 0; break;
        case CC_EVAL_TYPE: case CC_EVAL_VOID: case CC_EVAL_INIT_LIST: case CC_EVAL_STRING:
            cc_release_expr(p, expr);
            return cc_error(p, assert_loc, "static_assert expression is not a constant expression");
        DEFAULT_UNREACHABLE;
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

            CcField* fields = Allocator_alloc(al, 5 * sizeof(CcField));
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
        default: {
            // typedef void *__builtin_va_list;
            err = cc_pointer_of(p, ccqt_basic(CCBT_void), &va_list_type);
            if(err) return err;
            break;
        }
    }
    err = cc_scope_insert_typedef(al, &p->global, va_list_name, va_list_type);
    if(err) return CC_OOM_ERROR;
    err = cc_scope_insert_typedef(al, &p->global, gnu_va_list, va_list_type);
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
            {SV("type"), {.basic.kind=CCBT__Type}, offsetof(CiRtField, type)},
            {SV("name"), p->const_char_star, offsetof(CiRtField, name)},
            {SV("name_length"), {.basic.kind=CCBT_unsigned}, offsetof(CiRtField, name_length)},
            {SV("offset"), {.basic.kind=CCBT_unsigned}, offsetof(CiRtField, offset)},
            {SV("bitwidth"), {.basic.kind=CCBT_unsigned}, offsetof(CiRtField, bitwidth)},
            {SV("bitoffset"), {.basic.kind=CCBT_unsigned}, offsetof(CiRtField, bitoffset)},
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
        err = cc_scope_insert_typedef(al, &p->global, name, p->builtin_field);
        if(err) return CC_OOM_ERROR;
    }

    {
        struct f {StringView name; CcQualType type; size_t offset;} enuminfos[] = {
            {SV("name"), p->const_char_star, offsetof(CiRtEnumerator, name)},
            {SV("name_length"),{.basic.kind=CCBT_unsigned}, offsetof(CiRtEnumerator, name)},
            {SV("value"), {.basic.kind=CCBT_long_long}, offsetof(CiRtEnumerator, value)},
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
        err = cc_scope_insert_typedef(al, &p->global, name, p->builtin_enumerator);
        if(err) return CC_OOM_ERROR;
    }

    // typedef __int128 __int128_t; typedef unsigned __int128 __uint128_t;
    Atom int128_name = AT_atomize(p->cpp.at, "__int128_t", 10);
    Atom uint128_name = AT_atomize(p->cpp.at, "__uint128_t", 11);
    if(!int128_name || !uint128_name) return CC_OOM_ERROR;
    err = cc_scope_insert_typedef(al, &p->global, int128_name, ccqt_basic(CCBT_int128));
    if(err) return CC_OOM_ERROR;
    err = cc_scope_insert_typedef(al, &p->global, uint128_name, ccqt_basic(CCBT_unsigned_int128));
    if(err) return CC_OOM_ERROR;

    // Register builtin functions
    {
        static const struct { StringView name; CcBuiltinFunc id; } builtins[] = {
            {SV("__builtin_constant_p"), CC__builtin_constant_p},
            {SV("__builtin_offsetof"), CC__builtin_offsetof},
            {SV("__func__"), CC__func__},
            {SV("__FUNCTION__"), CC__func__},
            {SV("__atomic_fetch_add"), CC__atomic_fetch_add},
            {SV("__atomic_fetch_sub"), CC__atomic_fetch_sub},
            {SV("__atomic_load_n"), CC__atomic_load_n},
            {SV("__atomic_load"), CC__atomic_load},
            {SV("__atomic_store_n"), CC__atomic_store_n},
            {SV("__atomic_exchange_n"), CC__atomic_exchange_n},
            {SV("__atomic_compare_exchange_n"), CC__atomic_compare_exchange_n},
            {SV("__atomic_compare_exchange"), CC__atomic_compare_exchange},
            {SV("__atomic_store"), CC__atomic_store},
            {SV("__atomic_exchange"), CC__atomic_exchange},
            {SV("__atomic_thread_fence"), CC__atomic_thread_fence},
            {SV("__atomic_signal_fence"), CC__atomic_signal_fence},
            {SV("__builtin_va_start"), CC__builtin_va_start},
            {SV("__builtin_va_end"),   CC__builtin_va_end},
            {SV("__builtin_va_arg"),   CC__builtin_va_arg},
            {SV("__builtin_va_copy"),  CC__builtin_va_copy},
            {SV("__builtin_expect"),  CC__builtin_expect},
            {SV("__builtin_unreachable"), CC__builtin_unreachable},
            {SV("__builtin_trap"), CC__builtin_trap},
            {SV("__builtin_debugtrap"), CC__builtin_debugtrap},
            {SV("__builtin_abort"), CC__builtin_abort},
            {SV("__builtin_mul_overflow"), CC__builtin_mul_overflow},
            {SV("__builtin_add_overflow"), CC__builtin_add_overflow},
            {SV("__builtin_sub_overflow"), CC__builtin_sub_overflow},
            {SV("__builtin_popcount"), CC__builtin_popcount},
            {SV("__builtin_popcountl"), CC__builtin_popcountl},
            {SV("__builtin_popcountll"), CC__builtin_popcountll},
            {SV("__builtin_ctz"), CC__builtin_ctz},
            {SV("__builtin_ctzl"), CC__builtin_ctzl},
            {SV("__builtin_ctzll"), CC__builtin_ctzll},
            {SV("__builtin_clz"), CC__builtin_clz},
            {SV("__builtin_clzl"), CC__builtin_clzl},
            {SV("__builtin_clzll"), CC__builtin_clzll},
            {SV("__builtin_huge_val"), CC__builtin_huge_val},
            {SV("__builtin_huge_valf"), CC__builtin_huge_valf},
            {SV("__builtin_huge_vall"), CC__builtin_huge_vall},
            {SV("__builtin_nan"), CC__builtin_nan},
            {SV("__builtin_nanf"), CC__builtin_nanf},
            {SV("__nan"), CC__nan},
            {SV("__builtin_alloca"), CC__builtin_alloca},
            {SV("_alloca"), CC__builtin_alloca},
            {SV("alloca"), CC__builtin_alloca},
            {SV("__builtin_intern"), CC__builtin_intern},
            {SV("__bt"), CC__bt},
        };
        for(size_t i = 0; i < sizeof builtins / sizeof builtins[0]; i++){
            Atom a = AT_atomize(p->cpp.at, builtins[i].name.text, builtins[i].name.length);
            if(!a) return CC_OOM_ERROR;
            err = AM_put(&p->builtins, al, a, (void*)builtins[i].id);
            if(err) return CC_OOM_ERROR;
        }
    }
    // Register type methods/fields
    {
        static const struct { StringView name; CcTypeIntrospectionOp op; } typeintro[] = {
            {SV("name"), CC_TYPE_NAME},
            {SV("tag"), CC_TYPE_TAG},
            {SV("is_integer"), CC_TYPE_IS_INTEGER},
            {SV("is_float"), CC_TYPE_IS_FLOAT},
            {SV("is_arithmetic"), CC_TYPE_IS_ARITHMETIC},
            {SV("is_pointer"), CC_TYPE_IS_POINTER},
            {SV("is_struct"), CC_TYPE_IS_STRUCT},
            {SV("is_union"), CC_TYPE_IS_UNION},
            {SV("is_array"), CC_TYPE_IS_ARRAY},
            {SV("is_function"), CC_TYPE_IS_FUNCTION},
            {SV("is_enum"), CC_TYPE_IS_ENUM},
            {SV("is_const"), CC_TYPE_IS_CONST},
            {SV("is_volatile"), CC_TYPE_IS_VOLATILE},
            {SV("is_atomic"), CC_TYPE_IS_ATOMIC},
            {SV("is_unsigned"), CC_TYPE_IS_UNSIGNED},
            {SV("is_signed"), CC_TYPE_IS_SIGNED},
            {SV("is_callable"), CC_TYPE_IS_CALLABLE},
            {SV("is_variadic"), CC_TYPE_IS_VARIADIC},
            {SV("is_incomplete"), CC_TYPE_IS_INCOMPLETE},
            {SV("sizeof_"), CC_TYPE_SIZEOF},
            {SV("alignof_"), CC_TYPE_ALIGNOF},
            {SV("pointee"), CC_TYPE_POINTEE},
            {SV("unqual"), CC_TYPE_UNQUAL},
            {SV("count"), CC_TYPE_COUNT},
            {SV("is_callable_with"), CC_TYPE_IS_CALLABLE_WITH},
            {SV("is_castable_to"), CC_TYPE_CASTABLE_TO},
            {SV("field"), CC_TYPE_FIELD}, // field name or index;
            {SV("fields"), CC_TYPE_FIELDS},
            {SV("push_method"), CC_TYPE_PUSH_METHOD},
            {SV("enumerators"), CC_TYPE_ENUMERATORS},
            {SV("enumerator"), CC_TYPE_ENUMERATOR},
            {SV("return_type"), CC_TYPE_RETURN_TYPE},
            {SV("param_count"), CC_TYPE_PARAM_COUNT},
            {SV("param_type"), CC_TYPE_PARAM_TYPE},
            {SV("element_type"), CC_TYPE_ELEMENT_TYPE},
            {SV("underlying_type"), CC_TYPE_UNDERLYING_TYPE},
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
        struct b {StringView name; CcQualType ret; int nargs; CcQualType params[3]; _Bool variadic;} builtins[] = {
            {SV("memcpy"), p->void_star, 3, {p->void_star, p->const_void_star, {.basic.kind=t.size_type}}, .variadic=0},
            {SV("memmove"), p->void_star, 3, {p->void_star, p->const_void_star, {.basic.kind=t.size_type}}, .variadic=0},
            {SV("memset"), p->void_star, 3, {p->void_star, {.basic.kind=CCBT_int}, {.basic.kind=t.size_type}}, .variadic=0},
            {SV("malloc"), p->void_star, 1, {{.basic.kind=t.size_type}}, .variadic=0},
            {SV("realloc"), p->void_star, 2, {p->void_star, {.basic.kind=t.size_type}}, .variadic=0},
            {SV("calloc"), p->void_star, 2, {{.basic.kind=t.size_type},{.basic.kind=t.size_type}}, .variadic=0},
            {SV("free"), {.basic.kind=CCBT_void}, 1, {p->void_star}, .variadic=0},
            {SV("snprintf"), {.basic.kind=CCBT_int}, 3, {p->char_star, {.basic.kind=t.size_type}, p->const_char_star}, .variadic=1},
            {SV("printf"), {.basic.kind=CCBT_int}, 1, {p->const_char_star}, .variadic=1},
        };
        for(size_t i = 0; i < sizeof builtins / sizeof builtins[0]; i++){
            struct b* b = &builtins[i];
            CcFunction* ftype = cc_intern_function(&p->type_cache, al, b->ret, b->params, b->nargs, b->variadic, 0);
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
            err = cc_scope_insert_func(al, &p->global, key, func);
            if(err) return CC_OOM_ERROR;
            err = cc_scope_insert_func(al, &p->global, name, func);
            if(err) return CC_OOM_ERROR;
        }
    }
    return 0;
}
// Core function body parser. Sets up scope, registers params, parses
// declarations/statements. When terminate_on_rbrace is true, stops at '}'
// (but does not consume it). When false, stops at EOF (for lazy parsing
// from saved tokens).
static
int
cc_parse_func_body_inner(CcParser* p, CcFunc* f, _Bool terminate_on_rbrace){
    CcFunction* ftype = f->type;
    int err = 0;
    CcFunc* prev = p->current_func;
    p->current_func = f;
    err = cc_push_scope(p);
    if(err){ p->current_func = prev; return err; }
    // Register parameters as variables
    if(ftype->param_count){
        f->param_vars = Allocator_zalloc(cc_allocator(p), ftype->param_count * sizeof *f->param_vars);
        if(!f->param_vars){ err = CC_OOM_ERROR; goto end_scope; }
    }
    f->frame_size = 0;
    for(uint32_t i = 0; i < ftype->param_count; i++){
        Atom name = (i < f->params.count) ? f->params.data[i] : NULL;
        if(!name) continue;
        uint32_t param_sz, param_align;
        err = cc_sizeof_as_uint(p, ftype->params[i], f->loc, &param_sz);
        if(err) goto end_scope;
        err = cc_alignof_as_uint(p, ftype->params[i], f->loc, &param_align);
        if(err) goto end_scope;
        f->frame_size = (f->frame_size + param_align - 1) & ~(param_align - 1);
        CcVariable* var = Allocator_zalloc(cc_allocator(p), sizeof *var);
        if(!var){ err = CC_OOM_ERROR; goto end_scope; }
        *var = (CcVariable){
            .name = name,
            .loc = f->loc,
            .type = ftype->params[i],
            .automatic = 1,
            .frame_offset = f->frame_size,
        };
        f->frame_size += param_sz;
        f->param_vars[i] = var;
        err = cc_scope_insert_var(cc_allocator(p), p->current, name, var);
        if(err) goto end_scope;
    }
    for(;;){
        CcToken peek;
        err = cc_peek(p, &peek);
        if(err) goto end_scope;
        if(terminate_on_rbrace){
            if(peek.type == CC_PUNCTUATOR && peek.punct.punct == CC_rbrace) break;
            if(peek.type == CC_EOF){
                err = cc_error(p, f->loc, "Unexpected EOF in function body");
                goto end_scope;
            }
        }
        else {
            if(peek.type == CC_EOF) break;
        }
        err = cc_parse_one(p);
        if(err) goto end_scope;
    }
    f->parsed = 1;
    err = cc_resolve_gotos(p, f->body.data, f->body.count, &f->labels);
    end_scope:
    cc_pop_scope(p);
    p->current_func = prev;
    return err;
}

static
int
cc_parse_func_body(CcParser* p, CcFunc* f){
    if(!f->defined) return CC_UNREACHABLE_ERROR;
    if(f->parsed) return 0;
    Marray(CcToken)* tokens = f->tokens;
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
    return err;
}

#ifdef __clang__
#pragma clang assume_nonnull end
#endif
#include "cc_type_cache.c"
#include "cc_scope.c"
#endif
