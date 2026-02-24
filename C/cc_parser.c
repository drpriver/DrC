#ifndef C_CC_PARSER_C
#define C_CC_PARSER_C
//
// Copyright © 2026-2026, David Priver <david@davidpriver.com>
//
#include <stdarg.h>
#include "../Drp/bit_util.h"
#include "../Drp/parray.h"
#include "cc_parser.h"
#include "cpp_preprocessor.h"
#include "cc_errors.h"
#ifdef __clang__
#pragma clang assume_nonnull begin
#endif

static int cc_parse_expr(CcParser* p, CcExpr* _Nullable* _Nonnull out);
static int cc_parse_assignment_expr(CcParser* p, CcExpr* _Nullable* _Nonnull out);
static int cc_parse_ternary_expr(CcParser* p, CcExpr* _Nullable* _Nonnull out);
static int cc_parse_infix(CcParser* p, CcExpr* left, int min_prec, CcExpr* _Nullable* _Nonnull out);
static int cc_parse_prefix(CcParser* p, CcExpr* _Nullable* _Nonnull out);
static int cc_parse_primary(CcParser* p, CcExpr* _Nullable* _Nonnull out);
static int cc_parse_postfix(CcParser* p, CcExpr* operand, CcExpr* _Nullable* _Nonnull out);
static int cc_next_token(CcParser* p, CcToken* tok);
static int cc_unget(CcParser* p, CcToken* tok);
static int cc_peek(CcParser* p, CcToken* tok);
static int cc_expect_punct(CcParser* p, CcPunct punct);
static CcExpr* _Nullable cc_alloc_expr(CcParser* p, size_t nvalues);
LOG_PRINTF(3, 4) static int cc_error(CcParser*, SrcLoc, const char*, ...);
LOG_PRINTF(3, 4) static void cc_warn(CcParser*, SrcLoc, const char*, ...);
LOG_PRINTF(3, 4) static void cc_info(CcParser*, SrcLoc, const char*, ...);
LOG_PRINTF(3, 4) static void cc_debug(CcParser*, SrcLoc, const char*, ...);
#define cc_unimplemented(p, loc, msg) (cc_error(p, loc, "UNIMPLEMENTED: " msg " at %s:%d", __FILE__, __LINE__), CC_UNIMPLEMENTED_ERROR)
#define cc_unimp(p, msg) cc_unimplemented(p, (SrcLoc){0}, msg)
#define cc_unreachable(p, msg) (cc_error(p, (SrcLoc){0}, "UNREACHABLE code reached: " msg " at %s:%d", __FILE__, __LINE__), CC_UNREACHABLE_ERROR)
static _Bool cc_binop_lookup(CcPunct punct, CcExprKind* kind, int* prec);
static _Bool cc_assign_lookup(CcPunct punct, CcExprKind* kind);
static Marray(CcToken)*_Nullable cc_get_scratch(CcParser* p);
static void cc_release_scratch(CcParser* p, Marray(CcToken)*);
static Allocator cc_allocator(CcParser*p);
static Allocator cc_scratch_allocator(CcParser*p);
static int cc_parse_declarator(CcParser* p, CcQualType* out_head, CcQualType*_Nonnull*_Nonnull out_tail, Atom _Nullable * _Nullable out_name, Marray(Atom) *_Nullable out_param_names);
static CcQualType cc_intern_qualtype(CcParser* p, CcQualType t);
static _Bool cc_is_type_start(CcParser* p, CcToken* tok);
static int cc_parse_type_name(CcParser* p, CcQualType* out);
static int cc_sizeof_as_expr(CcParser* p, CcQualType t, SrcLoc loc, CcExpr* _Nullable* _Nonnull out);
static int cc_alignof_as_expr(CcParser* p, CcQualType t, SrcLoc loc, CcExpr* _Nullable* _Nonnull out);
static int cc_sizeof_as_uint(CcParser* p, CcQualType t, SrcLoc loc, uint32_t* out);
static int cc_alignof_as_uint(CcParser* p, CcQualType t, SrcLoc loc, uint32_t* out);
static int cc_check_cast(CcParser* p, CcQualType from, CcQualType to, SrcLoc loc);
static CcExpr* _Nullable cc_value_expr(CcParser* p, SrcLoc loc, CcQualType type);
static CcExpr* _Nullable cc_unary_expr(CcParser* p, CcExprKind kind, SrcLoc loc, CcQualType type, CcExpr* operand);
static CcExpr* _Nullable cc_binary_expr(CcParser* p, CcExprKind kind, SrcLoc loc, CcQualType type, CcExpr* left, CcExpr* right);
typedef struct CcDeclBase CcDeclBase;
static int cc_check_func_compat(CcParser* p, CcFunc* existing, const CcDeclBase* declbase, CcQualType new_type, SrcLoc loc);
static int cc_parse_attributes(CcParser* p, CcAttributes* attrs);
static int cc_parse_struct_or_union(CcParser* p, SrcLoc loc, _Bool is_union, CcQualType* base_type);
static int cc_check_anon_member_duplicates(CcParser* p, CcField* existing, uint32_t existing_count, CcQualType anon_type, SrcLoc loc);
static CcField* _Nullable cc_lookup_field(CcField* _Nullable fields, uint32_t field_count, Atom name);
static int cc_compute_struct_layout(CcParser* p, CcStruct* s, uint16_t pack_value);
static int cc_compute_union_layout(CcParser* p, CcUnion* u, uint16_t pack_value);
static const CcTargetConfig* cc_target(const CcParser*);
static int cc_handle_static_asssert(CcParser*);

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
#ifdef __clang__
#pragma clang assume_nonnull begin
#endif

typedef struct CcSpecifier CcSpecifier;
struct CcSpecifier {
    union {
        uint32_t bits;
        struct {
            uint32_t sp_typebits: 8,
                     sp_storagebits: 6,
                     _sp_typedef: 1,
                     sp_funcbits: 2,
                     sp_qualbits: 4,

                     _sp_infer: 1,
                     _padding1: 32-22;
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

                     _padding2:       32-22;
        };
    };
};
_Static_assert(sizeof(CcSpecifier) == sizeof(uint32_t), "");
static int cc_parse_declaration_specifier(CcParser* p, CcSpecifier* spec, CcQualType* base_type);
static int cc_parse_enum(CcParser* p, SrcLoc loc, CcQualType* base_type);

typedef struct CcDeclBase CcDeclBase;
struct CcDeclBase {
    CcQualType type;
    CcSpecifier spec;
};

static int cc_resolve_specifiers(CcParser* p, CcDeclBase* declbase);
static int cc_parse_decls(CcParser* p, const CcDeclBase* declbase);
static int cc_parse_statement(CcParser* p);

static
int
cc_push_scope(CcParser* p){
    CcScope* s = fl_pop(&p->scratch_scopes);
    if(s){
        cc_scope_clear(s);
    }
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
    if(!ccqt_is_basic(a) || !ccqt_is_basic(b))
        return cc_error(p, loc, "usual arithmetic conversions require arithmetic types");
    CcBasicTypeKind ak = a.basic.kind, bk = b.basic.kind;
    if(ak == CCBT_void || bk == CCBT_void)
        return cc_error(p, loc, "usual arithmetic conversions on void");
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
    _Bool a_unsigned = ccbt_is_unsigned(ak);
    _Bool b_unsigned = ccbt_is_unsigned(bk);
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
        if(kind == CC_ARRAY){
            *out = ccqt_as_array(t)->element;
            return 0;
        }
    }
    return cc_error(p, loc, "dereferencing non-pointer type");
}

// Wrap an expression in an implicit cast if types differ.
// Returns the original expression if types match or target is void.
static
CcExpr* _Nullable
cc_implicit_cast(CcParser* p, CcExpr* e, CcQualType target){
    if(target.basic.kind == CCBT_void && ccqt_is_basic(target))
        return e;
    if(e->type.bits == target.bits)
        return e;
    if(e->type.basic.kind == CCBT_void && ccqt_is_basic(e->type))
        return e;
    CcExpr* cast = cc_alloc_expr(p, 0);
    if(!cast) return NULL;
    cast->kind = CC_EXPR_CAST;
    cast->loc = e->loc;
    cast->type = target;
    cast->value0 = e;
    return cast;
}

static
_Bool
cc_is_type_start(CcParser* p, CcToken* tok){
    if(tok->type == CC_KEYWORD){
        switch(tok->kw.kw){
            case CC_void: case CC_char: case CC_short: case CC_int:
            case CC_long: case CC_float: case CC_double:
            case CC_signed: case CC_unsigned: case CC_bool:
            case CC_struct: case CC_union: case CC_enum:
            case CC_typeof: case CC_typeof_unqual:
            case CC___auto_type:
            case CC__Complex: case CC__Imaginary:
            case CC__Float16: case CC__Float32: case CC__Float64: case CC__Float128:
            case CC__Decimal32: case CC__Decimal64: case CC__Decimal128:
            case CC__BitInt:
            case CC__Atomic:
            case CC_const: case CC_volatile: case CC_restrict:
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
    CcDeclBase base = {.type.bits = (uintptr_t)-1};
    int err = cc_parse_declaration_specifier(p, &base.spec, &base.type);
    if(err) return err;
    err = cc_resolve_specifiers(p, &base);
    if(err) return err;
    CcQualType head = {0};
    CcQualType* tail = &head;
    err = cc_parse_declarator(p, &head, &tail, NULL, NULL);
    if(err) return err;
    *tail = base.type;
    *out = cc_intern_qualtype(p, head);
    return 0;
}

static
CcExpr* _Nullable
cc_value_expr(CcParser* p, SrcLoc loc, CcQualType type){
    CcExpr* node = cc_alloc_expr(p, 0);
    if(!node) return NULL;
    node->kind = CC_EXPR_VALUE;
    node->loc = loc;
    node->type = type;
    return node;
}

static
CcExpr* _Nullable
cc_unary_expr(CcParser* p, CcExprKind kind, SrcLoc loc, CcQualType type, CcExpr* operand){
    CcExpr* node = cc_alloc_expr(p, 0);
    if(!node) return NULL;
    node->kind = kind;
    node->loc = loc;
    node->type = type;
    node->value0 = operand;
    return node;
}

static
CcExpr* _Nullable
cc_binary_expr(CcParser* p, CcExprKind kind, SrcLoc loc, CcQualType type, CcExpr* left, CcExpr* right){
    CcExpr* node = cc_alloc_expr(p, 1);
    if(!node) return NULL;
    node->kind = kind;
    node->loc = loc;
    node->type = type;
    node->value0 = left;
    node->values[0] = right;
    return node;
}

// Validate that a cast from `from` to `to` is legal.
static
int
cc_check_cast(CcParser* p, CcQualType from, CcQualType to, SrcLoc loc){
    // Cast to void is always valid.
    if(ccqt_is_basic(to) && to.basic.kind == CCBT_void) return 0;
    CcTypeKind to_kind = ccqt_kind(to);
    CcTypeKind from_kind = ccqt_kind(from);
    // Cannot cast to array, function, struct, or union.
    if(to_kind == CC_ARRAY)
        return cc_error(p, loc, "cannot cast to array type");
    if(to_kind == CC_FUNCTION)
        return cc_error(p, loc, "cannot cast to function type");
    if(to_kind == CC_STRUCT || to_kind == CC_UNION)
        return cc_error(p, loc, "cannot cast to struct or union type");
    // Cannot cast from struct, union, or void.
    if(from_kind == CC_STRUCT || from_kind == CC_UNION)
        return cc_error(p, loc, "cannot cast from struct or union type");
    if(from_kind == CC_BASIC && from.basic.kind == CCBT_void)
        return cc_error(p, loc, "cannot cast from void");
    // Arithmetic <-> arithmetic is always valid.
    // (integer, float, complex, bool, enum underlying types)
    _Bool from_arith = (from_kind == CC_BASIC && from.basic.kind != CCBT_nullptr_t) || from_kind == CC_ENUM;
    _Bool to_arith = (to_kind == CC_BASIC && to.basic.kind != CCBT_nullptr_t) || to_kind == CC_ENUM;
    if(from_arith && to_arith)
        return 0;
    // Pointer/array/function sources are pointer-like for cast purposes.
    _Bool from_ptr = from_kind == CC_POINTER || from_kind == CC_ARRAY || from_kind == CC_FUNCTION || (from_kind == CC_BASIC && from.basic.kind == CCBT_nullptr_t);
    _Bool to_ptr = to_kind == CC_POINTER || (to_kind == CC_BASIC && to.basic.kind == CCBT_nullptr_t);
    // Pointer <-> pointer.
    if(from_ptr && to_ptr)
        return 0;
    // Pointer <-> integer (includes bool).
    if(from_ptr && to_arith && ccbt_is_integer(to.basic.kind))
        return 0;
    if(from_arith && ccbt_is_integer(from.basic.kind) && to_ptr)
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
            if(arr->is_incomplete)
                return cc_error(p, loc, "sizeof applied to incomplete array type");
            CcExpr* elem_size;
            int err = cc_sizeof_as_expr(p, arr->element, loc, &elem_size);
            if(err) return err;
            if(arr->is_vla){
                // Runtime: vla_expr * sizeof(element)
                CcExpr* dim = arr->vla_expr;
                if(!dim) return cc_error(p, loc, "sizeof applied to VLA with no dimension");
                CcExpr* cast_dim = cc_implicit_cast(p, dim, size_type);
                if(!cast_dim) return CC_OOM_ERROR;
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
        case CC_VECTOR:{
            CcVector* v = ccqt_as_vector(t);
            CcExpr* node = cc_value_expr(p, loc, size_type);
            if(!node) return CC_OOM_ERROR;
            node->uinteger = v->vector_size;
            *out = node;
            return 0;
        }
    }
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
        case CC_VECTOR:{
            CcVector* v = ccqt_as_vector(t);
            align = v->vector_size > cfg->max_align ? cfg->max_align:v->vector_size;
            break;
        }
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
        case CC_VECTOR:{
            CcVector* v = ccqt_as_vector(t);
            *out = v->vector_size;
            return 0;
        }
    }
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
        case CC_VECTOR:{
            CcVector* v = ccqt_as_vector(t);
            *out = v->vector_size > tgt->max_align ? tgt->max_align:v->vector_size;
            return 0;
        }
    }
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
cc_parse_expr(CcParser* p, CcExpr* _Nullable* _Nonnull out){
    CcExpr* left;
    int err = cc_parse_assignment_expr(p, &left);
    if(err) return err;
    for(;;){
        CcToken tok;
        err = cc_next_token(p, &tok);
        if(err) return err;
        if(tok.type == CC_PUNCTUATOR && tok.punct.punct == CC_comma){
            CcExpr* right;
            err = cc_parse_assignment_expr(p, &right);
            if(err) return err;
            CcExpr* node = cc_alloc_expr(p, 1);
            if(!node) return CC_OOM_ERROR;
            node->kind = CC_EXPR_COMMA;
            node->loc = tok.loc;
            node->value0 = left;
            node->values[0] = right;
            node->type = right->type;
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
cc_parse_assignment_expr(CcParser* p, CcExpr* _Nullable* _Nonnull out){
    CcExpr* left;
    int err = cc_parse_ternary_expr(p, &left);
    if(err) return err;
    CcToken tok;
    err = cc_next_token(p, &tok);
    if(err) return err;
    if(tok.type == CC_PUNCTUATOR){
        CcExprKind kind;
        if(cc_assign_lookup(tok.punct.punct, &kind)){
            CcExpr* right;
            // right-associative: recurse into assignment_expr
            err = cc_parse_assignment_expr(p, &right);
            if(err) return err;
            CcExpr* cright = cc_implicit_cast(p, right, left->type);
            if(!cright) return CC_OOM_ERROR;
            right = cright;
            CcExpr* node = cc_alloc_expr(p, 1);
            if(!node) return CC_OOM_ERROR;
            node->kind = kind;
            node->loc = tok.loc;
            node->type = left->type;
            node->value0 = left;
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
cc_parse_ternary_expr(CcParser* p, CcExpr* _Nullable* _Nonnull out){
    CcExpr* cond;
    // Parse the condition using infix with minimum precedence
    int err = cc_parse_prefix(p, &cond);
    if(err) return err;
    err = cc_parse_infix(p, cond, 4, &cond);
    if(err) return err;
    CcToken tok;
    err = cc_next_token(p, &tok);
    if(err) return err;
    if(tok.type == CC_PUNCTUATOR && tok.punct.punct == CC_question){
        CcExpr* then_expr;
        err = cc_parse_expr(p, &then_expr);
        if(err) return err;
        err = cc_expect_punct(p, CC_colon);
        if(err) return err;
        CcExpr* else_expr;
        // right-associative: recurse into ternary
        err = cc_parse_ternary_expr(p, &else_expr);
        if(err) return err;
        CcQualType common;
        err = cc_usual_arithmetic(p, then_expr->type, else_expr->type, &common, tok.loc);
        if(err) return err;
        CcExpr* ct = cc_implicit_cast(p, then_expr, common);
        if(!ct) return CC_OOM_ERROR;
        then_expr = ct;
        CcExpr* ce = cc_implicit_cast(p, else_expr, common);
        if(!ce) return CC_OOM_ERROR;
        else_expr = ce;
        CcExpr* node = cc_alloc_expr(p, 2);
        if(!node) return CC_OOM_ERROR;
        node->kind = CC_EXPR_TERNARY;
        node->loc = tok.loc;
        node->type = common;
        node->value0 = cond;
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
cc_parse_infix(CcParser* p, CcExpr* left, int min_prec, CcExpr* _Nullable* _Nonnull out){
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
        err = cc_parse_prefix(p, &right);
        if(err) return err;
        // Look ahead: if next op has higher precedence, recurse
        err = cc_parse_infix(p, right, prec + 1, &right);
        if(err) return err;
        // Compute result type, insert implicit casts
        CcQualType result_type = {0};
        switch(kind){
            case CC_EXPR_LOGAND: case CC_EXPR_LOGOR:
                result_type = ccqt_basic(CCBT_int);
                break;
            case CC_EXPR_EQ: case CC_EXPR_NE:
            case CC_EXPR_LT: case CC_EXPR_GT:
            case CC_EXPR_LE: case CC_EXPR_GE: {
                if(!ccqt_is_pointer_like(left->type) && !ccqt_is_pointer_like(right->type)){
                    CcQualType common;
                    err = cc_usual_arithmetic(p, left->type, right->type, &common, tok.loc);
                    if(err) return err;
                    CcExpr* cl = cc_implicit_cast(p, left, common);
                    if(!cl) return CC_OOM_ERROR;
                    left = cl;
                    CcExpr* cr = cc_implicit_cast(p, right, common);
                    if(!cr) return CC_OOM_ERROR;
                    right = cr;
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
                CcExpr* cl = cc_implicit_cast(p, left, lp);
                if(!cl) return CC_OOM_ERROR;
                left = cl;
                CcExpr* cr = cc_implicit_cast(p, right, rp);
                if(!cr) return CC_OOM_ERROR;
                right = cr;
                result_type = lp;
                break;
            }
            case CC_EXPR_ADD: {
                _Bool lptr = ccqt_is_pointer_like(left->type);
                _Bool rptr = ccqt_is_pointer_like(right->type);
                if(lptr)       result_type = left->type;
                else if(rptr)  result_type = right->type;
                else {
                    err = cc_usual_arithmetic(p, left->type, right->type, &result_type, tok.loc);
                    if(err) return err;
                    CcExpr* cl = cc_implicit_cast(p, left, result_type);
                    if(!cl) return CC_OOM_ERROR;
                    left = cl;
                    CcExpr* cr = cc_implicit_cast(p, right, result_type);
                    if(!cr) return CC_OOM_ERROR;
                    right = cr;
                }
                break;
            }
            case CC_EXPR_SUB: {
                _Bool lptr = ccqt_is_pointer_like(left->type);
                _Bool rptr = ccqt_is_pointer_like(right->type);
                if(lptr && rptr){
                    // ptr - ptr = ptrdiff_t
                    result_type = ccqt_basic(cc_target(p)->ptrdiff_type);
                }
                else if(lptr){
                    result_type = left->type;

                }
                else {
                    err = cc_usual_arithmetic(p, left->type, right->type, &result_type, tok.loc);
                    if(err) return err;
                    CcExpr* cl = cc_implicit_cast(p, left, result_type);
                    if(!cl) return CC_OOM_ERROR;
                    left = cl;
                    CcExpr* cr = cc_implicit_cast(p, right, result_type);
                    if(!cr) return CC_OOM_ERROR;
                    right = cr;
                }
                break;
            }
            default: {
                // arithmetic/bitwise: *,/,%,&,|,^
                err = cc_usual_arithmetic(p, left->type, right->type, &result_type, tok.loc);
                if(err) return err;
                CcExpr* cl = cc_implicit_cast(p, left, result_type);
                if(!cl) return CC_OOM_ERROR;
                left = cl;
                CcExpr* cr = cc_implicit_cast(p, right, result_type);
                if(!cr) return CC_OOM_ERROR;
                right = cr;
                break;
            }
        }
        CcExpr* node = cc_alloc_expr(p, 1);
        if(!node) return CC_OOM_ERROR;
        node->kind = kind;
        node->loc = tok.loc;
        node->type = result_type;
        node->value0 = left;
        node->values[0] = right;
        left = node;
    }
    *out = left;
    return 0;
}

// Prefix unary operators
static
int
cc_parse_prefix(CcParser* p, CcExpr* _Nullable* _Nonnull out){
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
                    return cc_unimplemented(p, tok.loc, "compound literal");
                }
                // Cast expression: (type) operand
                CcExpr* operand;
                err = cc_parse_prefix(p, &operand);
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
            CcExpr* operand;
            err = cc_parse_prefix(p, &operand);
            if(err) return err;
            // Insert implicit casts and compute type
            CcQualType result_type;
            switch(kind){
                case CC_EXPR_NEG: case CC_EXPR_POS: case CC_EXPR_BITNOT: {
                    err = cc_integer_promote(p, operand->type, &result_type, tok.loc);
                    if(err) return err;
                    CcExpr* co = cc_implicit_cast(p, operand, result_type);
                    if(!co) return CC_OOM_ERROR;
                    operand = co;
                    break;
                }
                case CC_EXPR_LOGNOT:
                    result_type = ccqt_basic(CCBT_int);
                    break;
                case CC_EXPR_DEREF:
                    err = cc_deref_type(p, operand->type, &result_type, tok.loc);
                    if(err) return err;
                    break;
                case CC_EXPR_ADDR: {
                    CcPointer* ptr = cc_intern_pointer(&p->type_cache, cc_allocator(p), operand->type, 0);
                    if(!ptr) return CC_OOM_ERROR;
                    result_type = (CcQualType){.bits = (uintptr_t)ptr};
                    break;
                }
                default: // PREINC, PREDEC
                    result_type = operand->type;
                    break;
            }
            CcExpr* node = cc_alloc_expr(p, 0);
            if(!node) return CC_OOM_ERROR;
            node->kind = kind;
            node->loc = tok.loc;
            node->type = result_type;
            node->value0 = operand;
            *out = node;
            return 0;
        }
    }
    // Not a prefix op: put it back and parse primary + postfix
    cc_unget(p, &tok);
    CcExpr* primary;
    err = cc_parse_primary(p, &primary);
    if(err) return err;
    return cc_parse_postfix(p, primary, out);
}

// Primary expressions: literals, identifiers, parenthesized
static
int
cc_parse_primary(CcParser* p, CcExpr* _Nullable* _Nonnull out){
    CcToken tok;
    int err = cc_next_token(p, &tok);
    if(err) return err;
    switch(tok.type){
        case CC_CONSTANT: {
            CcExpr* node = cc_alloc_expr(p, 0);
            if(!node) return CC_OOM_ERROR;
            node->kind = CC_EXPR_VALUE;
            node->loc = tok.loc;
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
            CcExpr* node = cc_alloc_expr(p, 0);
            if(!node) return CC_OOM_ERROR;
            node->kind = CC_EXPR_VALUE;
            node->loc = tok.loc;
            node->str.length = tok.str.length;
            node->text = tok.str.text;
            // Type: char* (pointer to char)
            CcPointer* sp = cc_intern_pointer(&p->type_cache, cc_allocator(p), ccqt_basic(CCBT_char), 0);
            if(!sp) return CC_OOM_ERROR;
            node->type = (CcQualType){.bits = (uintptr_t)sp};
            *out = node;
            return 0;
        }
        case CC_IDENTIFIER: {
            CcSymbol sym;
            if(!cc_scope_lookup_symbol(p->current, tok.ident.ident, CC_SCOPE_WALK_CHAIN, &sym)){
                return cc_error(p, tok.loc, "undeclared identifier '%.*s'",
                    tok.ident.ident->length, tok.ident.ident->data);
            }
            switch(sym.kind){
                case CC_SYM_VAR: {
                    CcExpr* node = cc_alloc_expr(p, 0);
                    if(!node) return CC_OOM_ERROR;
                    node->kind = CC_EXPR_VARIABLE;
                    node->loc = tok.loc;
                    node->type = sym.var->type;
                    node->var = sym.var;
                    *out = node;
                    return 0;
                }
                case CC_SYM_FUNC: {
                    CcExpr* node = cc_alloc_expr(p, 0);
                    if(!node) return CC_OOM_ERROR;
                    node->kind = CC_EXPR_FUNCTION;
                    node->loc = tok.loc;
                    node->type = (CcQualType){.bits = (uintptr_t)sym.func->type};
                    node->func = sym.func;
                    *out = node;
                    return 0;
                }
                case CC_SYM_ENUMERATOR: {
                    CcExpr* node = cc_alloc_expr(p, 0);
                    if(!node) return CC_OOM_ERROR;
                    node->kind = CC_EXPR_VALUE;
                    node->loc = tok.loc;
                    node->type = sym.enumerator->type;
                    node->integer = sym.enumerator->value;
                    *out = node;
                    return 0;
                }
                case CC_SYM_TYPEDEF:
                    return cc_error(p, tok.loc, "unexpected type name '%.*s' in expression",
                        tok.ident.ident->length, tok.ident.ident->data);
            }
            return cc_error(p, tok.loc, "unexpected symbol kind");
        }
        case CC_PUNCTUATOR:
            if(tok.punct.punct == CC_lparen){
                CcExpr* inner;
                err = cc_parse_expr(p, &inner);
                if(err) return err;
                err = cc_expect_punct(p, CC_rparen);
                if(err) return err;
                *out = inner;
                return 0;
            }
            return cc_error(p, tok.loc, "Unexpected punctuator in expression");
        case CC_KEYWORD:
            if(tok.kw.kw == CC_sizeof){
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
                err = cc_parse_prefix(p, &operand);
                if(err) return err;
                CcExpr* sz;
                err = cc_sizeof_as_expr(p, operand->type, tok.loc, &sz);
                if(err) return err;
                *out = sz;
                return 0;
            }
            if(tok.kw.kw == CC_alignof){
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
                err = cc_parse_prefix(p, &operand);
                if(err) return err;
                CcExpr* al;
                err = cc_alignof_as_expr(p, operand->type, tok.loc, &al);
                if(err) return err;
                *out = al;
                return 0;
            }
            if(tok.kw.kw == CC__Countof){
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
                    err = cc_parse_expr(p, &expr);
                    if(err) return err;
                    arr_type = expr->type;
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
                    CcExpr* cast = cc_implicit_cast(p, dim, size_type);
                    if(!cast) return CC_OOM_ERROR;
                    *out = cast;
                    return 0;
                }
                CcExpr* node = cc_value_expr(p, tok.loc, size_type);
                if(!node) return CC_OOM_ERROR;
                node->uinteger = (uint64_t)arr->length;
                *out = node;
                return 0;
            }
            if(tok.kw.kw == CC_true || tok.kw.kw == CC_false){
                CcExpr* node = cc_value_expr(p, tok.loc, ccqt_basic(CCBT_bool));
                if(!node) return CC_OOM_ERROR;
                node->uinteger = tok.kw.kw == CC_true ? 1 : 0;
                *out = node;
                return 0;
            }
            if(tok.kw.kw == CC_nullptr){
                CcExpr* node = cc_value_expr(p, tok.loc, ccqt_basic(CCBT_nullptr_t));
                if(!node) return CC_OOM_ERROR;
                node->uinteger = 0;
                *out = node;
                return 0;
            }
            return cc_error(p, tok.loc, "Unexpected keyword in expression");
        case CC_EOF:
            return cc_error(p, tok.loc, "Unexpected end of input in expression");
    }
    return cc_error(p, tok.loc, "Unexpected token in expression");
}

// Postfix operators
static
int
cc_parse_postfix(CcParser* p, CcExpr* operand, CcExpr* _Nullable* _Nonnull out){
    for(;;){
        CcToken tok;
        int err = cc_next_token(p, &tok);
        if(err) return err;
        if(tok.type != CC_PUNCTUATOR){
            cc_unget(p, &tok);
            break;
        }
        switch((uint32_t)tok.punct.punct){
            case CC_plusplus: {
                CcExpr* node = cc_alloc_expr(p, 0);
                if(!node) return CC_OOM_ERROR;
                node->kind = CC_EXPR_POSTINC;
                node->loc = tok.loc;
                node->type = operand->type;
                node->value0 = operand;
                operand = node;
                continue;
            }
            case CC_minusminus: {
                CcExpr* node = cc_alloc_expr(p, 0);
                if(!node) return CC_OOM_ERROR;
                node->kind = CC_EXPR_POSTDEC;
                node->loc = tok.loc;
                node->type = operand->type;
                node->value0 = operand;
                operand = node;
                continue;
            }
            case CC_lbracket: {
                // subscript: operand[expr]
                CcExpr* index;
                err = cc_parse_expr(p, &index);
                if(err) return err;
                err = cc_expect_punct(p, CC_rbracket);
                if(err) return err;
                CcExpr* node = cc_alloc_expr(p, 1);
                if(!node) return CC_OOM_ERROR;
                node->kind = CC_EXPR_SUBSCRIPT;
                node->loc = tok.loc;
                CcQualType elem_type;
                err = cc_deref_type(p, operand->type, &elem_type, tok.loc);
                if(err) return err;
                node->type = elem_type;
                node->value0 = operand;
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
                // Resolve the aggregate type
                CcQualType agg_type = operand->type;
                if(mkind == CC_EXPR_ARROW){
                    // -> requires pointer to struct/union
                    if(ccqt_is_basic(agg_type) || ccqt_kind(agg_type) != CC_POINTER)
                        return cc_error(p, tok.loc, "member reference with '->' requires a pointer type");
                    CcPointer* ptr = ccqt_as_ptr(agg_type);
                    agg_type = ptr->pointee;
                }
                CcQualType member_type = {.bits = (uintptr_t)-1};
                if(!ccqt_is_basic(agg_type)){
                    CcTypeKind tk = ccqt_kind(agg_type);
                    if(tk == CC_STRUCT){
                        CcStruct* s = ccqt_as_struct(agg_type);
                        CcField* f = cc_lookup_field(s->fields, s->field_count, member_name);
                        if(f) member_type = f->type;
                    }
                    else if(tk == CC_UNION){
                        CcUnion* u = ccqt_as_union(agg_type);
                        CcField* f = cc_lookup_field(u->fields, u->field_count, member_name);
                        if(f) member_type = f->type;
                    }
                }
                if(member_type.bits == (uintptr_t)-1)
                    return cc_error(p, member.loc, "no member named '%s'", member_name->data);
                CcExpr* mnode = cc_alloc_expr(p, 1);
                if(!mnode) return CC_OOM_ERROR;
                mnode->kind = mkind;
                mnode->loc = tok.loc;
                mnode->type = member_type;
                mnode->extra = member_name->length;
                mnode->text = member_name->data;
                mnode->values[0] = operand;
                operand = mnode;
                continue;
            }
            case CC_lparen: {
                // Function call: operand(args...)
                // Count args by parsing into a temp buffer
                // First check for empty arg list
                CcToken peek;
                err = cc_next_token(p, &peek);
                if(err) return err;
                if(peek.type == CC_PUNCTUATOR && peek.punct.punct == CC_rparen){
                    // No args
                    CcExpr* node = cc_alloc_expr(p, 0);
                    if(!node) return CC_OOM_ERROR;
                    node->kind = CC_EXPR_CALL;
                    node->loc = tok.loc;
                    node->call.nargs = 0;
                    node->value0 = operand;
                    operand = node;
                    continue;
                }
                cc_unget(p, &peek);
                // Parse args into a small stack buffer, then allocate
                CcExpr* arg_buf[64];
                uint32_t nargs = 0;
                for(;;){
                    if(nargs >= 64)
                        return cc_error(p, tok.loc, "Too many function arguments (max 64)");
                    CcExpr* arg;
                    err = cc_parse_assignment_expr(p, &arg);
                    if(err) return err;
                    arg_buf[nargs++] = arg;
                    CcToken sep;
                    err = cc_next_token(p, &sep);
                    if(err) return err;
                    if(sep.type == CC_PUNCTUATOR && sep.punct.punct == CC_rparen)
                        break;
                    if(sep.type != CC_PUNCTUATOR || sep.punct.punct != CC_comma)
                        return cc_error(p, sep.loc, "Expected ',' or ')' in function call");
                }
                // nargs values + the function expr in value0
                CcExpr* node = cc_alloc_expr(p, nargs);
                if(!node) return CC_OOM_ERROR;
                node->kind = CC_EXPR_CALL;
                node->loc = tok.loc;
                node->call.nargs = nargs;
                node->value0 = operand;
                for(uint32_t i = 0; i < nargs; i++)
                    node->values[i] = arg_buf[i];
                operand = node;
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

// ---------------------------------------------------------------------------
// Type printer
// ---------------------------------------------------------------------------

static const char* _Null_unspecified cc_basic_names[] = {
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
    [CCBT_float]              = "float",
    [CCBT_double]             = "double",
    [CCBT_long_double]        = "long double",
    [CCBT_float_complex]      = "float _Complex",
    [CCBT_double_complex]     = "double _Complex",
    [CCBT_long_double_complex]= "long double _Complex",
    [CCBT_nullptr_t]          = "nullptr_t",
};

static void cc_print_type_pre(MStringBuilder*, CcQualType t);
static void cc_print_type_post(MStringBuilder*, CcQualType t);

// Returns true if this type kind binds tighter than pointer
// (i.e. pointer-to-this needs grouping parens).
static
_Bool
cc_type_needs_parens(CcQualType t){
    if(ccqt_is_basic(t)) return 0;
    CcTypeKind k = ccqt_kind(t);
    return k == CC_ARRAY || k == CC_FUNCTION;
}

static
void
cc_print_type_pre(MStringBuilder* sb, CcQualType t){
    if(ccqt_is_basic(t)){
        if(t.is_const) msb_write_literal(sb, "const ");
        if(t.is_volatile) msb_write_literal(sb, "volatile ");
        if(t.is_atomic) msb_write_literal(sb, "_Atomic ");
        CcBasicTypeKind k = t.basic.kind;
        msb_sprintf(sb, "%s", k < CCBT_COUNT ? cc_basic_names[k] : "<bad-basic>");
        return;
    }
    CcTypeKind kind = ccqt_kind(t);
    switch(kind){
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
            CcStruct* s = ccqt_as_struct(t);
            if(s->name) msb_sprintf(sb, "struct %.*s", s->name->length, s->name->data);
            else msb_write_literal(sb, "struct <anon>");
            return;
        }
        case CC_UNION: {
            CcUnion* u = ccqt_as_union(t);
            if(u->name) msb_sprintf(sb, "union %.*s", u->name->length, u->name->data);
            else msb_write_literal(sb, "union <anon>");
            return;
        }
        case CC_ENUM: {
            CcEnum* e = ccqt_as_enum(t);
            if(e->name) msb_sprintf(sb, "enum %.*s", e->name->length, e->name->data);
            else msb_write_literal(sb, "enum <anon>");
            return;
        }
        default:
            msb_sprintf(sb, "<type:%d>", (int)kind);
            return;
    }
}

static
void
cc_print_type_post(MStringBuilder* sb, CcQualType t){
    if(ccqt_is_basic(t)) return;
    CcTypeKind kind = ccqt_kind(t);
    switch(kind){
        case CC_POINTER: {
            CcPointer* p = ccqt_as_ptr(t);
            if(cc_type_needs_parens(p->pointee))
                msb_write_char(sb, ')');
            cc_print_type_post(sb, p->pointee);
            return;
        }
        case CC_ARRAY: {
            CcArray* a = ccqt_as_array(t);
            if(a->is_incomplete)
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
        default:
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
cc_print_expr(MStringBuilder*sb, CcExpr* e){
    switch(e->kind){
        case CC_EXPR_VALUE:
            if(e->str.length && e->text){
                msb_sprintf(sb, "\"%.*s\"", e->str.length, e->text);
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
        case CC_EXPR_IDENTIFIER:
            msb_sprintf(sb, "%.*s", e->extra, e->text);
            return;
        case CC_EXPR_VARIABLE:
            msb_sprintf(sb, "%.*s", e->var->name->length, e->var->name->data);
            return;
        case CC_EXPR_FUNCTION:
            msb_sprintf(sb, "%.*s", e->func->name->length, e->func->name->data);
            return;
        case CC_EXPR_SIZEOF_VMT:
        case CC_EXPR_COMPOUND_LITERAL:
        case CC_EXPR_STATEMENT_EXPRESSION:
            msb_write_literal(sb, "<unimpl>");
            return;
        // Unary prefix
        #define UNOP(K, S) case K: msb_write_literal(sb, S); cc_print_expr(sb, e->value0); return;
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
        case CC_EXPR_POSTINC: cc_print_expr(sb, e->value0); msb_write_literal(sb, "++"); return;
        case CC_EXPR_POSTDEC: cc_print_expr(sb, e->value0); msb_write_literal(sb, "--"); return;
        // Binary ops
        #define BINOP(K, S) case K: msb_write_char(sb, '('); cc_print_expr(sb, e->value0); msb_write_literal(sb, " " S " "); cc_print_expr(sb, e->values[0]); msb_write_char(sb, ')'); return;
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
            cc_print_expr(sb, e->value0);
            msb_write_char(sb, '[');
            cc_print_expr(sb, e->values[0]);
            msb_write_char(sb, ']');
            return;
        case CC_EXPR_TERNARY:
            msb_write_char(sb, '(');
            cc_print_expr(sb, e->value0);
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
            cc_print_expr(sb, e->value0);
            return;
        case CC_EXPR_DOT:
            cc_print_expr(sb, e->values[0]);
            msb_sprintf(sb, ".%.*s", e->extra, e->text);
            return;
        case CC_EXPR_ARROW:
            cc_print_expr(sb, e->values[0]);
            msb_sprintf(sb, "->%.*s", e->extra, e->text);
            return;
        case CC_EXPR_CALL:
            cc_print_expr(sb, e->value0);
            msb_write_char(sb, '(');
            for(uint32_t i = 0; i < e->call.nargs; i++){
                if(i) msb_write_literal(sb, ", ");
                cc_print_expr(sb, e->values[i]);
            }
            msb_write_char(sb, ')');
            return;
    }
    msb_write_literal(sb, "<unknown>");
}

typedef struct CcEvalResult CcEvalResult;
struct CcEvalResult {
    enum { CC_EVAL_INT, CC_EVAL_UINT, CC_EVAL_FLOAT, CC_EVAL_DOUBLE, CC_EVAL_ERROR } kind;
    union {
        int64_t i;
        uint64_t u;
        float f;
        double d;
    };
};

static CcEvalResult cc_eval_error(void){ return (CcEvalResult){.kind = CC_EVAL_ERROR}; }

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
int64_t
cc_eval_to_int(CcEvalResult r){
    switch(r.kind){
        case CC_EVAL_INT:    return r.i;
        case CC_EVAL_UINT:   return (int64_t)r.u;
        case CC_EVAL_FLOAT:  return (int64_t)r.f;
        case CC_EVAL_DOUBLE: return (int64_t)r.d;
        default: return 0;
    }
}

static
CcEvalResult
cc_eval_expr(CcExpr* e){
    switch(e->kind){
        case CC_EXPR_VALUE:
            if(e->str.length && e->text)
                return cc_eval_error(); // can't eval strings to a number
            if(ccqt_is_basic(e->type)){
                switch(e->type.basic.kind){
                    case CCBT_float:
                        return (CcEvalResult){.kind = CC_EVAL_FLOAT, .f = e->float_};
                    case CCBT_double: case CCBT_long_double:
                        return (CcEvalResult){.kind = CC_EVAL_DOUBLE, .d = e->double_};
                    case CCBT_unsigned: case CCBT_unsigned_long:
                    case CCBT_unsigned_long_long: case CCBT_unsigned_char:
                    case CCBT_unsigned_short: case CCBT_bool:
                        return (CcEvalResult){.kind = CC_EVAL_UINT, .u = e->uinteger};
                    default:
                        return (CcEvalResult){.kind = CC_EVAL_INT, .i = e->integer};
                }
            }
            return (CcEvalResult){.kind = CC_EVAL_UINT, .u = e->uinteger};
        case CC_EXPR_IDENTIFIER:
            return cc_eval_error(); // unresolved identifier
        case CC_EXPR_NEG: {
            CcEvalResult v = cc_eval_expr(e->value0);
            if(v.kind == CC_EVAL_ERROR) return v;
            switch(v.kind){
                case CC_EVAL_INT:    v.i = -v.i; return v;
                case CC_EVAL_UINT:   return (CcEvalResult){.kind = CC_EVAL_INT, .i = -(int64_t)v.u};
                case CC_EVAL_FLOAT:  v.f = -v.f; return v;
                case CC_EVAL_DOUBLE: v.d = -v.d; return v;
                default: return cc_eval_error();
            }
        }
        case CC_EXPR_POS: return cc_eval_expr(e->value0);
        case CC_EXPR_BITNOT: {
            CcEvalResult v = cc_eval_expr(e->value0);
            if(v.kind == CC_EVAL_ERROR) return v;
            switch(v.kind){
                case CC_EVAL_INT:  v.i = ~v.i; return v;
                case CC_EVAL_UINT: v.u = ~v.u; return v;
                default: return cc_eval_error();
            }
        }
        case CC_EXPR_LOGNOT: {
            CcEvalResult v = cc_eval_expr(e->value0);
            if(v.kind == CC_EVAL_ERROR) return v;
            int64_t iv = cc_eval_to_int(v);
            return (CcEvalResult){.kind = CC_EVAL_INT, .i = !iv};
        }
        case CC_EXPR_COMMA: {
            cc_eval_expr(e->value0); // discard
            return cc_eval_expr(e->values[0]);
        }
        case CC_EXPR_TERNARY: {
            CcEvalResult cond = cc_eval_expr(e->value0);
            if(cond.kind == CC_EVAL_ERROR) return cond;
            if(cc_eval_to_int(cond))
                return cc_eval_expr(e->values[0]);
            else
                return cc_eval_expr(e->values[1]);
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
            CcEvalResult L = cc_eval_expr(e->value0);
            CcEvalResult R = cc_eval_expr(e->values[0]);
            if(L.kind == CC_EVAL_ERROR || R.kind == CC_EVAL_ERROR)
                return cc_eval_error();
            cc_eval_promote(&L, &R);
            #define IBINOP(op) \
                switch(L.kind){ \
                    case CC_EVAL_INT:    return (CcEvalResult){.kind=CC_EVAL_INT, .i=L.i op R.i}; \
                    case CC_EVAL_UINT:   return (CcEvalResult){.kind=CC_EVAL_UINT, .u=L.u op R.u}; \
                    case CC_EVAL_FLOAT:  return (CcEvalResult){.kind=CC_EVAL_FLOAT, .f=L.f op R.f}; \
                    case CC_EVAL_DOUBLE: return (CcEvalResult){.kind=CC_EVAL_DOUBLE, .d=L.d op R.d}; \
                    default: return cc_eval_error(); \
                }
            #define IINTOP(op) \
                switch(L.kind){ \
                    case CC_EVAL_INT:  return (CcEvalResult){.kind=CC_EVAL_INT, .i=L.i op R.i}; \
                    case CC_EVAL_UINT: return (CcEvalResult){.kind=CC_EVAL_UINT, .u=L.u op R.u}; \
                    default: return cc_eval_error(); \
                }
            #define ICMPOP(op) \
                switch(L.kind){ \
                    case CC_EVAL_INT:    return (CcEvalResult){.kind=CC_EVAL_INT, .i=L.i op R.i}; \
                    case CC_EVAL_UINT:   return (CcEvalResult){.kind=CC_EVAL_INT, .i=L.u op R.u}; \
                    case CC_EVAL_FLOAT:  return (CcEvalResult){.kind=CC_EVAL_INT, .i=L.f op R.f}; \
                    case CC_EVAL_DOUBLE: return (CcEvalResult){.kind=CC_EVAL_INT, .i=L.d op R.d}; \
                    default: return cc_eval_error(); \
                }
            switch(e->kind){
                case CC_EXPR_ADD: IBINOP(+)
                case CC_EXPR_SUB: IBINOP(-)
                case CC_EXPR_MUL: IBINOP(*)
                case CC_EXPR_DIV:
                    // Check for division by zero
                    if(L.kind == CC_EVAL_INT && R.i == 0) return cc_eval_error();
                    if(L.kind == CC_EVAL_UINT && R.u == 0) return cc_eval_error();
                    IBINOP(/)
                case CC_EXPR_MOD:
                    if(L.kind == CC_EVAL_INT && R.i == 0) return cc_eval_error();
                    if(L.kind == CC_EVAL_UINT && R.u == 0) return cc_eval_error();
                    IINTOP(%)
                case CC_EXPR_BITAND: IINTOP(&)
                case CC_EXPR_BITOR:  IINTOP(|)
                case CC_EXPR_BITXOR: IINTOP(^)
                case CC_EXPR_LSHIFT: IINTOP(<<)
                case CC_EXPR_RSHIFT: IINTOP(>>)
                case CC_EXPR_LOGAND:
                    return (CcEvalResult){.kind=CC_EVAL_INT, .i=cc_eval_to_int(L) && cc_eval_to_int(R)};
                case CC_EXPR_LOGOR:
                    return (CcEvalResult){.kind=CC_EVAL_INT, .i=cc_eval_to_int(L) || cc_eval_to_int(R)};
                case CC_EXPR_EQ: ICMPOP(==)
                case CC_EXPR_NE: ICMPOP(!=)
                case CC_EXPR_LT: ICMPOP(<)
                case CC_EXPR_GT: ICMPOP(>)
                case CC_EXPR_LE: ICMPOP(<=)
                case CC_EXPR_GE: ICMPOP(>=)
                default: return cc_eval_error();
            }
            #undef IBINOP
            #undef IINTOP
            #undef ICMPOP
        }
        case CC_EXPR_CAST: {
            CcEvalResult v = cc_eval_expr(e->value0);
            if(v.kind == CC_EVAL_ERROR) return v;
            if(!ccqt_is_basic(e->type)) return v;
            CcBasicTypeKind tk = e->type.basic.kind;
            if(ccbt_is_float(tk)){
                double d;
                switch(v.kind){
                    case CC_EVAL_INT:    d = (double)v.i; break;
                    case CC_EVAL_UINT:   d = (double)v.u; break;
                    case CC_EVAL_FLOAT:  d = (double)v.f; break;
                    case CC_EVAL_DOUBLE: d = v.d; break;
                    default: return cc_eval_error();
                }
                if(tk == CCBT_float)
                    return (CcEvalResult){.kind = CC_EVAL_FLOAT, .f = (float)d};
                return (CcEvalResult){.kind = CC_EVAL_DOUBLE, .d = d};
            }
            if(ccbt_is_integer(tk)){
                if(ccbt_is_unsigned(tk)){
                    switch(v.kind){
                        case CC_EVAL_INT:    return (CcEvalResult){.kind=CC_EVAL_UINT, .u=(uint64_t)v.i};
                        case CC_EVAL_UINT:   return (CcEvalResult){.kind=CC_EVAL_UINT, .u=v.u};
                        case CC_EVAL_FLOAT:  return (CcEvalResult){.kind=CC_EVAL_UINT, .u=(uint64_t)v.f};
                        case CC_EVAL_DOUBLE: return (CcEvalResult){.kind=CC_EVAL_UINT, .u=(uint64_t)v.d};
                        default: return cc_eval_error();
                    }
                }
                else {
                    switch(v.kind){
                        case CC_EVAL_INT:    return (CcEvalResult){.kind=CC_EVAL_INT, .i=v.i};
                        case CC_EVAL_UINT:   return (CcEvalResult){.kind=CC_EVAL_INT, .i=(int64_t)v.u};
                        case CC_EVAL_FLOAT:  return (CcEvalResult){.kind=CC_EVAL_INT, .i=(int64_t)v.f};
                        case CC_EVAL_DOUBLE: return (CcEvalResult){.kind=CC_EVAL_INT, .i=(int64_t)v.d};
                        default: return cc_eval_error();
                    }
                }
            }
            return v;
        }
        default:
            return cc_eval_error();
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
        case CC_EVAL_ERROR:  msb_write_literal(sb, "<cannot evaluate>"); break;
    }
}

static
int
cc_parse_top_level(CcParser* p, _Bool* finished){
    int err;
    CcToken tok;
    CcDeclBase b = {.type.bits=-1};
    err = cc_parse_declaration_specifier(p, &b.spec, &b.type);
    if(err) return err;
    if(b.spec.bits || b.type.bits != (uintptr_t)-1){
        err = cc_resolve_specifiers(p,  &b);
        if(err) return err;
        err = cc_parse_decls(p, &b);
        if(err) return err;
        return 0;
    }

    err = cc_next_token(p, &tok);
    if(err) return err;
    switch(tok.type){
        case CC_EOF:
            *finished = 1;
            return 0;
        case CC_KEYWORD:
            if(tok.kw.kw == CC_static_assert){
                err = cc_unget(p, &tok);
                if(err) return err;
                return cc_handle_static_asssert(p);
            }
            goto Ldefault;
        default:
            Ldefault:;
            err = cc_unget(p, &tok);
            if(err) return err;
            break;
    }
    return cc_parse_statement(p);
}

static
void
cc_parser_discard_input(CcParser* p){
    p->pending.count = 0;
    cpp_discard_all_input(&p->lexer.cpp);
}


static void cpp_msg(CPreprocessor* cpp, SrcLoc loc, LogLevel level, const char* prefix, const char* fmt, va_list va);

static
int
cc_error(CcParser* p, SrcLoc loc, const char* fmt, ...){
    va_list va;
    va_start(va, fmt);
    cpp_msg(&p->lexer.cpp, loc, LOG_PRINT_ERROR, "error", fmt, va);
    va_end(va);
    return CC_SYNTAX_ERROR;
}

static
void
cc_warn(CcParser* p, SrcLoc loc, const char* fmt, ...){
    va_list va;
    va_start(va, fmt);
    cpp_msg(&p->lexer.cpp, loc, LOG_PRINT_ERROR, "warning", fmt, va);
    va_end(va);
}

static
void
cc_info(CcParser* p, SrcLoc loc, const char* fmt, ...){
    va_list va;
    va_start(va, fmt);
    cpp_msg(&p->lexer.cpp, loc, LOG_PRINT_ERROR, "info", fmt, va);
    va_end(va);
}

static
void
cc_debug(CcParser* p, SrcLoc loc, const char* fmt, ...){
    va_list va;
    va_start(va, fmt);
    cpp_msg(&p->lexer.cpp, loc, LOG_PRINT_ERROR, "debug", fmt, va);
    va_end(va);
}

static
int
cc_next_token(CcParser* p, CcToken* tok){
    if(p->pending.count){
        *tok = ma_pop(CcToken)(&p->pending);
        return 0;
    }
    return cc_lex_next_token(&p->lexer, tok);
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
cc_alloc_expr(CcParser* p, size_t nvalues){
    size_t size = sizeof(CcExpr) + nvalues * sizeof(CcExpr*);
    return Allocator_zalloc(cc_allocator(p), size);
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
    return p->lexer.cpp.allocator;
}
static
Allocator
cc_scratch_allocator(CcParser*p){
    return allocator_from_arena(&p->scratch_arena);
}

// Parse __attribute__((attr-list))
// Can be called multiple times; attributes accumulate into *attrs.
// If the next token is not __attribute__, this is a no-op.
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
                    err = cc_parse_assignment_expr(p, &expr);
                    if(err) return err;
                    if(!expr)
                        return cc_error(p, tok.loc, "expected constant expression for aligned attribute");
                    CcEvalResult val = cc_eval_expr(expr);
                    if(val.kind == CC_EVAL_ERROR)
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
                err = cc_parse_assignment_expr(p, &expr);
                if(err) return err;
                if(!expr)
                    return cc_error(p, tok.loc, "expected constant expression for vector_size attribute");
                CcEvalResult val = cc_eval_expr(expr);
                if(val.kind == CC_EVAL_ERROR)
                    return cc_error(p, tok.loc, "vector_size attribute requires a constant expression");
                if(val.kind != CC_EVAL_INT && val.kind != CC_EVAL_UINT)
                    return cc_error(p, tok.loc, "vector_size attribute requires a constant integral expression");
                attrs->vector_size = (uint16_t)val.u; // TODO: value validation
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

static
int
cc_compute_struct_layout(CcParser* p, CcStruct* s, uint16_t pack_value){
    int err = 0;
    uint32_t offset = 0;
    uint32_t max_align = 1;
    uint32_t bitfield_offset = 0; // bit offset within current storage unit
    uint32_t bitfield_storage_end = 0; // byte offset of end of current storage unit
    uint32_t bitfield_storage_start = 0; // byte offset of start of current storage unit
    uint32_t bitfield_storage_size = 0; // size of current storage unit in bytes
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
                    bitfield_storage_size = 0;
                    bitfield_type = (CcQualType){0};
                }
                f->offset = offset;
                f->bitoffset = 0;
                continue;
            }
            // Can we pack into the current storage unit?
            _Bool fits;
            if(bf_abi == CC_BITFIELD_MSVC)
                fits = bitfield_type.bits == f->type.bits && bitfield_offset + bw <= storage_bits;
            else
                fits = bitfield_storage_size == field_size && bitfield_offset + bw <= storage_bits;
            if(fits){
                // Fits in current storage unit
                f->offset = bitfield_storage_start;
                f->bitoffset = bitfield_offset;
                bitfield_offset += bw;
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
                bitfield_storage_size = field_size;
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
            bitfield_storage_size = 0;
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
    return 0;
}

// Compute union layout: size = max field size, alignment = max field alignment.
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
    return 0;
}

// #pragma pack handler.
// Supports: pack(N), pack(), pack(push, N), pack(pop)
static
int
cc_pragma_pack(void* _Null_unspecified ctx, CPreprocessor* cpp, SrcLoc loc, const CppToken*_Null_unspecified toks, size_t ntoks){
    CcParser* p = (CcParser*)ctx;
    if(!ntoks || toks[0].type != CPP_PUNCTUATOR || toks[0].punct != '(')
        return (cc_warn(p, loc, "#pragma pack expects '('"), 0);
    if(ntoks < 2)
        return (cc_warn(p, loc, "#pragma pack expects at least ()"), 0);

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
        // __builtin_debugtrap();
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

// Register parser-level pragmas. Call after cpp_define_builtin_macros.
static
int
cc_register_pragmas(CcParser* p){
    return cpp_register_pragma(&p->lexer.cpp, SV("pack"), cc_pragma_pack, p);
}

// Look up a field by name in a struct or union.
// For anonymous members, recursively searches inner struct/union fields.
// Returns the field pointer or NULL if not found.
static
CcField* _Nullable
cc_lookup_field(CcField* _Nullable fields, uint32_t field_count, Atom name){
    for(uint32_t i = 0; i < field_count; i++){
        CcField* f = &fields[i];
        if(f->is_method){
            if(f->method->name == name) return f;
            continue;
        }
        if(f->name == name) return f;
        if(!f->name){
            // Anonymous member — search recursively
            CcTypeKind tk = ccqt_kind(f->type);
            if(tk == CC_STRUCT){
                CcStruct* inner = ccqt_as_struct(f->type);
                CcField* found = cc_lookup_field(inner->fields, inner->field_count, name);
                if(found) return found;
            }
            else if(tk == CC_UNION){
                CcUnion* inner = ccqt_as_union(f->type);
                CcField* found = cc_lookup_field(inner->fields, inner->field_count, name);
                if(found) return found;
            }
        }
    }
    return NULL;
}

// Check that fields introduced by an anonymous struct/union member don't
// collide with existing fields.
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
            if(cc_lookup_field(existing, existing_count, f->method->name))
                return cc_error(p, loc, "duplicate member '%s'", f->method->name->data);
        }
        else if(f->name){
            if(cc_lookup_field(existing, existing_count, f->name))
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

// Parse a struct or union specifier.
// kind is CC_STRUCT or CC_UNION.
// On success, *base_type is set to the struct/union type.
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
            }
            else {
                CcStruct* s = existing = cc_scope_lookup_struct_tag(p->current, name, CC_SCOPE_NO_WALK);
                if(s && !s->is_incomplete) return cc_error(p, loc, "Redefinition of %s '%s'", is_union ? "union" : "struct", name->data);
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
            CcDeclBase member_base = {.type.bits = (uintptr_t)-1};
            err = cc_parse_declaration_specifier(p, &member_base.spec, &member_base.type);
            if(err) goto struct_err;
            member_attrs = p->attributes;
            cc_clear_attributes(&p->attributes);
            if(member_base.spec.sp_storagebits){
                err = cc_error(p, loc, "Storage class specifiers not allowed in struct/union members");
                goto struct_err;
            }
            err = cc_resolve_specifiers(p, &member_base);
            if(err) goto struct_err;
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
                    err = cc_parse_assignment_expr(p, &bw_expr);
                    if(err) goto struct_err;
                    if(!bw_expr){
                        err = cc_error(p, tok.loc, "expected constant expression for bitfield width");
                        goto struct_err;
                    }
                    CcEvalResult bw_val = cc_eval_expr(bw_expr);
                    if(bw_val.kind == CC_EVAL_ERROR){
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
                        if(cc_lookup_field(fields_arr.data, (uint32_t)fields_arr.count, func->name)){
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
                        err = cc_parse_assignment_expr(p, &bw_expr);
                        if(err) goto struct_err;
                        if(!bw_expr){
                            err = cc_error(p, tok.loc, "expected constant expression for bitfield width");
                            goto struct_err;
                        }
                        CcEvalResult bw_val = cc_eval_expr(bw_expr);
                        if(bw_val.kind == CC_EVAL_ERROR){
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
                if(member_name && cc_lookup_field(fields_arr.data, (uint32_t)fields_arr.count, member_name)){
                    err = cc_error(p, tok.loc, "duplicate member '%s'", member_name->data);
                    goto struct_err;
                }
                err = ma_push(CcField)(&fields_arr, cc_allocator(p), ((CcField){
                    .type = member_type,
                    .name = member_name,
                    .bitwidth = (uint32_t)bitwidth,
                    .is_bitfield = is_bitfield,
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
        CcDeclBase ub = {.type.bits = (uintptr_t)-1};
        err = cc_parse_declaration_specifier(p, &ub.spec, &ub.type);
        if(err) return err;
        if(ub.spec.sp_typebits != ub.spec.bits)
            return cc_error(p, loc, "Underlying type does not allow non-type specifiers");
        if(ub.spec.sp_infer_type)
            return cc_error(p, loc, "__auto_type not allowed as underlying type of enum");
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
                if(existing->underlying.bits != (uintptr_t)-1 && existing->underlying.bits != underlying.bits)
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
                err = cc_parse_assignment_expr(p, &expr);
                if(err) goto enum_err;
                if(!expr){
                    err = cc_error(p, tok.loc, "expected constant expression");
                    goto enum_err;
                }
                CcEvalResult val = cc_eval_expr(expr);
                if(val.kind == CC_EVAL_ERROR){
                    err = cc_error(p, tok.loc, "enumerator value is not a constant expression");
                    goto enum_err;
                }
                if(val.kind == CC_EVAL_FLOAT || val.kind == CC_EVAL_DOUBLE){
                    err = cc_error(p, tok.loc, "Enumerator value is a floating point value");
                    goto enum_err;
                }
                next_value = cc_eval_to_int(val);
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
cc_parse_declaration_specifier(CcParser* p, CcSpecifier* spec, CcQualType* base_type){
    if(base_type->bits != (uintptr_t)-1) return cc_unreachable(p, "parsing decl specifier with base type set");
    if(spec->bits != 0) return cc_unreachable(p, "parsing decl specifier with spec set");
    int err = 0;
    CcToken tok;
    for(int i = 0; ; i++){
        err = cc_next_token(p, &tok);
        if(err) return err;
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
                        if(base_type->bits != (uintptr_t)-1)
                            return cc_error(p, tok.loc, "Second type in declaration");
                        if(spec->sp_int)
                            return cc_error(p, tok.loc, "Duplicate int in declaration");
                        if(spec->sp_char)
                            return cc_error(p, tok.loc, "int after char");
                        if(spec->sp___auto_type)
                            return cc_error(p, tok.loc, "int after __auto_type");
                        spec->sp_int = 1;
                        continue;
                    case CC_long:
                        if(base_type->bits != (uintptr_t)-1)
                            return cc_error(p, tok.loc, "Second type in declaration");
                        if(spec->sp_long > 1)
                            return cc_error(p, tok.loc, "Duplicate long after long long in declaration");
                        if(spec->sp_char)
                            return cc_error(p, tok.loc, "long after char");
                        if(spec->sp_short)
                            return cc_error(p, tok.loc, "long after short");
                        if(spec->sp___auto_type)
                            return cc_error(p, tok.loc, "long after __auto_type");
                        spec->sp_long++;
                        continue;
                    case CC_char:
                        if(base_type->bits != (uintptr_t)-1)
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
                        spec->sp_char = 1;
                        continue;
                    case CC___auto_type:
                        if(base_type->bits != (uintptr_t)-1)
                            return cc_error(p, tok.loc, "Second type in declaration");
                        if(spec->sp_typebits)
                            return cc_error(p, tok.loc, "Second type in declaration");
                        spec->sp___auto_type = 1;
                        continue;
                    case CC_auto:
                        if(spec->sp_typedef)
                            return cc_error(p, tok.loc, "auto after typedef");
                        spec->sp_auto = 1;
                        continue;
                    case CC_bool:
                        if(base_type->bits != (uintptr_t)-1)
                            return cc_error(p, tok.loc, "Second type in declaration");
                        if(spec->sp_typebits)
                            return cc_error(p, tok.loc, "Second type in declaration");
                        *base_type = ccqt_basic(CCBT_bool);
                        continue;
                    case CC_enum: {
                        if(base_type->bits != (uintptr_t)-1)
                            return cc_error(p, tok.loc, "Second type in declaration");
                        if(spec->sp_typebits)
                            return cc_error(p, tok.loc, "enum with other type specifiers");
                        err = cc_parse_enum(p, tok.loc, base_type);
                        if(err) return err;
                        continue;
                    }
                    case CC_void:
                        if(base_type->bits != (uintptr_t)-1)
                            return cc_error(p, tok.loc, "Second type in declaration");
                        if(spec->sp_typebits)
                            return cc_error(p, tok.loc, "Second type in declaration");
                        *base_type = ccqt_basic(CCBT_void);
                        continue;
                    case CC_float:
                        if(base_type->bits != (uintptr_t)-1)
                            return cc_error(p, tok.loc, "Second type in declaration");
                        if(spec->sp_typebits)
                            return cc_error(p, tok.loc, "Second type in declaration");
                        *base_type = ccqt_basic(CCBT_float);
                        continue;
                    case CC_const:
                        spec->sp_const = 1;
                        continue;
                    case CC_short:
                        if(base_type->bits != (uintptr_t)-1)
                            return cc_error(p, tok.loc, "Second type in declaration");
                        if(spec->sp_short)
                            return cc_error(p, tok.loc, "Duplicate short in declaration");
                        if(spec->sp_long)
                            return cc_error(p, tok.loc, "short after long");
                        if(spec->sp_char)
                            return cc_error(p, tok.loc, "short after char");
                        if(spec->sp___auto_type)
                            return cc_error(p, tok.loc, "short after __auto_type");
                        spec->sp_short = 1;
                        continue;
                    case CC_union: {
                        if(base_type->bits != (uintptr_t)-1)
                            return cc_error(p, tok.loc, "Second type in declaration");
                        if(spec->sp_typebits)
                            return cc_error(p, tok.loc, "union with other type specifiers");
                        err = cc_parse_struct_or_union(p, tok.loc, 1, base_type);
                        if(err) return err;
                        continue;
                    }
                    case CC_double:
                        if(base_type->bits != (uintptr_t)-1)
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
                        if(base_type->bits != (uintptr_t)-1)
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
                        if(base_type->bits != (uintptr_t)-1)
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
                                    CcQualType elem = arr->element;
                                    if(ccqt_is_basic(elem))
                                        align_val = cc_target(p)->alignof_[elem.basic.kind];
                                    else
                                        return cc_error(p, tok.loc, "_Alignas with complex array element type not yet supported");
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
                            err = cc_parse_assignment_expr(p, &expr);
                            if(err) return err;
                            if(!expr)
                                return cc_error(p, tok.loc, "expected expression in _Alignas");
                            CcEvalResult val = cc_eval_expr(expr);
                            if(val.kind == CC_EVAL_ERROR)
                                return cc_error(p, tok.loc, "_Alignas requires a constant expression");
                            int64_t av = cc_eval_to_int(val);
                            if(av < 0)
                                return cc_error(p, tok.loc, "_Alignas value must be non-negative");
                            if(av != 0 && (av & (av - 1)) != 0)
                                return cc_error(p, tok.loc, "_Alignas value must be zero or a power of 2");
                            align_val = (uint32_t)av;
                        }
                        err = cc_expect_punct(p, CC_rparen);
                        if(err) return err;
                        if(align_val > 0){
                            if(!p->attributes.has_aligned || align_val > p->attributes.aligned){
                                p->attributes.aligned = (uint16_t)align_val;
                                p->attributes.has_aligned = 1;
                            }
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
                        if(base_type->bits != (uintptr_t)-1)
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
                    case CC__Float32:
                    case CC__Float64:
                    case CC__Float128:
                        return cc_unimplemented(p, tok.loc, "_FloatNN parsing in declaration");
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
                        continue;
                    }
                    case CC_typeof_unqual:
                    do_typeof: {
                        if(base_type->bits != (uintptr_t)-1)
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
                            err = cc_parse_expr(p, &expr);
                            if(err) return err;
                            if(!expr)
                                return cc_error(p, tok.loc, "Expected expression in typeof");
                            *base_type = expr->type;
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
                if(spec->sp_typebits || base_type->bits != (uintptr_t)-1){
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

static
int
cc_parse_statement(CcParser* p){
    (void)p;
    return cc_unimp(p, "parse statement");
}
static
int
cc_resolve_specifiers(CcParser* p, CcDeclBase* declbase){
    CcDeclBase b = *declbase;
    if(!b.spec.bits && b.type.bits == (uintptr_t)-1) return cc_unreachable(p, "Resolving specifier with no spec and no type");
    if(!b.spec.sp_typebits && b.type.bits == (uintptr_t)-1)
        b.spec.sp_infer_type = 1;
    if(b.spec.sp___auto_type)
        b.spec.sp_infer_type = 1;
    if(b.type.bits == (uintptr_t)-1 && !b.spec.sp_infer_type){
        // construct type from keywords
        if(b.spec.sp_char){
            b.type = ccqt_basic(b.spec.sp_signed? CCBT_signed_char: b.spec.sp_unsigned? CCBT_unsigned_char : CCBT_char);
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
        _Bool restrict_ = 0, const_ = 0, volatile_ = 0, atomic_ = 0;
        for(;;){
            err = cc_next_token(p, &tok);
            if(err) return err;
            if(tok.type == CC_KEYWORD){
                switch(tok.kw.kw){
                    case CC_restrict: restrict_ = 1; continue;
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
        ptr->restrict_ = restrict_;
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
            // Parse array dimension
            CcToken peek;
            err = cc_peek(p, &peek);
            if(err) return err;
            if(peek.type == CC_PUNCTUATOR && peek.punct.punct == ']'){
                // []
                arr->is_incomplete = 1;
            }
            else {
                CcExpr* dim = NULL;
                err = cc_parse_assignment_expr(p, &dim);
                if(err) return err;
                if(!dim) return cc_error(p, tok.loc, "Expected array dimension");
                CcEvalResult val = cc_eval_expr(dim);
                if(val.kind == CC_EVAL_ERROR)
                    return cc_unimplemented(p, tok.loc, "VLA array dimensions");
                int64_t length = cc_eval_to_int(val);
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
                CcDeclBase param_base = {.type.bits = (uintptr_t)-1};
                err = cc_parse_declaration_specifier(p, &param_base.spec, &param_base.type);
                if(err) goto param_err;
                err = cc_resolve_specifiers(p, &param_base);
                if(err) goto param_err;

                CcQualType param_head = {0};
                CcQualType* param_tail = &param_head;
                Atom param_name = NULL;
                err = cc_parse_declarator(p, &param_head, &param_tail, &param_name, NULL);
                if(err) goto param_err;
                *param_tail = param_base.type;

                err = ma_push(CcQualType)(&param_types, cc_scratch_allocator(p), param_head);
                if(err){ err = CC_OOM_ERROR; goto param_err; }

                if(out_param_names){
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

// Walk a type built by cc_parse_declarator and re-intern all derived
// type nodes so pointer equality works for type comparison.
static CcQualType
cc_intern_qualtype(CcParser* p, CcQualType t){
    uintptr_t quals = t.bits & 7;
    switch(ccqt_kind(t)){
        case CC_BASIC:
            return t;
        case CC_POINTER: {
            CcPointer* old = ccqt_as_ptr(t);
            CcQualType pointee = cc_intern_qualtype(p, old->pointee);
            CcPointer* ptr = cc_intern_pointer(&p->type_cache, cc_allocator(p), pointee, old->restrict_);
            if(!ptr) return t;
            return (CcQualType){.bits = (uintptr_t)ptr | quals};
        }
        case CC_ARRAY: {
            CcArray* old = ccqt_as_array(t);
            CcQualType elem = cc_intern_qualtype(p, old->element);
            CcArray* arr = cc_intern_array(&p->type_cache, cc_allocator(p), elem, old->length, old->is_static, old->is_incomplete);
            if(!arr) return t;
            return (CcQualType){.bits = (uintptr_t)arr | quals};
        }
        case CC_FUNCTION: {
            CcFunction* old = ccqt_as_function(t);
            // Intern param types in-place — the old node is throwaway.
            for(uint32_t i = 0; i < old->param_count; i++)
                old->params[i] = cc_intern_qualtype(p, old->params[i]);
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
        }
        // postfix processing
        _Bool stop = 0;
        err = cc_next_token(p, &tok);
        if(err) return err;
        if(first && tok.type == CC_PUNCTUATOR && tok.punct.punct == '{'){
            if(!is_fndef)
                return cc_error(p, tok.loc, "Expected ',' or ';'");
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
            func->defined = 1;
            func->extern_ = declbase->spec.sp_extern;
            func->static_ = declbase->spec.sp_static;
            func->inline_ = declbase->spec.sp_inline;
            func->tokens = body_tokens;
            func->params.count = param_names.count;
            func->params.data = param_names.data;
            if(p->eager_parsing){
                return cc_unimplemented(p, tok.loc, "eager function body parsing");
            }
            return 0;
        }
        else if(tok.type == CC_PUNCTUATOR && tok.punct.punct == '='){
            err = cc_parse_assignment_expr(p, &initializer);
            if(err) return err;
            if(!initializer) return cc_error(p, tok.loc, "Expected expression after '='");
            err = cc_next_token(p, &tok);
            if(err) return err;
        }
        if(initializer){
            if(declbase->spec.sp_infer_type){
                // Infer type from initializer
                type = initializer->type;
                // Apply qualifiers from specifier
                if(declbase->spec.sp_const) type.is_const = 1;
                if(declbase->spec.sp_volatile) type.is_volatile = 1;
                if(declbase->spec.sp_atomic) type.is_atomic = 1;
            }
            else {
                // Check compatibility and insert implicit cast
                CcQualType target = type;
                target.is_const = 0;
                target.is_volatile = 0;
                target.is_atomic = 0;
                err = cc_check_cast(p, initializer->type, target, tok.loc);
                if(err) return err;
                initializer = cc_implicit_cast(p, initializer, target);
                if(!initializer) return CC_OOM_ERROR;
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
            err = cc_scope_insert_typedef(cc_allocator(p), p->current, name, type);
            if(err) return err;
        }
        else if(is_fndef){
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
            func->extern_ = declbase->spec.sp_extern;
            func->static_ = declbase->spec.sp_static;
            func->inline_ = declbase->spec.sp_inline;
            func->params.count = param_names.count;
            func->params.data = param_names.data;
        }
        else {
            if(declbase->spec.sp_inline)
                return cc_error(p, tok.loc, "'inline' is only valid on functions");
            if(declbase->spec.sp_noreturn)
                return cc_error(p, tok.loc, "'_Noreturn' is only valid on functions");
            // Reject incomplete array types without initializer in local scope.
            if(!initializer && !declbase->spec.sp_extern && p->current_func && ccqt_kind(type) == CC_ARRAY){
                CcArray* arr = ccqt_as_array(type);
                if(arr->is_incomplete)
                    return cc_error(p, tok.loc, "variable '%.*s' has incomplete array type", name->length, name->data);
            }
            CcVariable* var = Allocator_zalloc(cc_allocator(p), sizeof *var);
            if(!var) return CC_OOM_ERROR;
            *var = (CcVariable){
                .name = name,
                .loc = tok.loc,
                .type = type,
                .extern_ = declbase->spec.sp_extern,
                .static_ = declbase->spec.sp_static,
                .initializer = initializer,
            };
            err = cc_scope_insert_var(cc_allocator(p), p->current, name, var);
            if(err) return err;
        }
        if(stop) break;
    }
    return err;
}
static
const CcTargetConfig*
cc_target(const CcParser* p){
    return &p->lexer.cpp.target;
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
    err = cc_parse_assignment_expr(p, &expr);
    if(err) return err;
    if(!expr)
        return cc_error(p, assert_loc, "expected expression in static_assert");
    CcEvalResult val = cc_eval_expr(expr);
    if(val.kind == CC_EVAL_ERROR)
        return cc_error(p, assert_loc, "static_assert expression is not a constant expression");
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
        msg = tok.str.text;
        msg_len = tok.str.length;
    }
    err = cc_expect_punct(p, CC_rparen);
    if(err) return err;
    err = cc_expect_punct(p, CC_semi);
    if(err) return err;
    int64_t ival = cc_eval_to_int(val);
    if(!ival){
        MStringBuilder tmp = {.allocator = allocator_from_arena(&p->scratch_arena)};
        msb_write_literal(&tmp, "static assertion failed: ");
        cc_print_expr(&tmp, expr);
        if(msg){
            msb_write_literal(&tmp, ": \"");
            msb_write_str(&tmp, msg, msg_len);
            msb_write_literal(&tmp, "\"");
        }
        StringView sv = msb_borrow_sv(&tmp);
        cc_error(p, assert_loc, "%.*s", sv_p(sv));
        return CC_SYNTAX_ERROR;
    }
    return 0;
}

#ifdef __clang__
#pragma clang assume_nonnull end
#endif
#include "cc_type_cache.c"
#include "cc_scope.c"
#endif
