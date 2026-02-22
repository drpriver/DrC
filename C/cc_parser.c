#ifndef C_CC_PARSER_C
#define C_CC_PARSER_C
//
// Copyright © 2026-2026, David Priver <david@davidpriver.com>
//
#include <stdarg.h>
#include "cc_parser.h"
#include "cpp_preprocessor.h"
#include "../Drp/bit_util.h"
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

enum {
    CC_NO_ERROR,
    CC_OOM_ERROR,
    CC_SYNTAX_ERROR,
    CC_UNREACHABLE_ERROR,
    CC_UNIMPLEMENTED_ERROR,
    CC_FILE_NOT_FOUND_ERROR,
};

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
    if(ccbt_int_rank(s) > ccbt_int_rank(u)){
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
            CcPointer* ptr = (CcPointer*)(t.bits & ~(uintptr_t)7);
            *out = ptr->pointee;
            return 0;
        }
        if(kind == CC_ARRAY){
            CcArray* arr = (CcArray*)(t.bits & ~(uintptr_t)7);
            *out = arr->element;
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
                    result_type = ccqt_basic(p->lexer.cpp.target.ptrdiff_type);
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
                    node->type.basic.kind = p->lexer.cpp.target.wchar_type;
                    node->uinteger = tok.constant.integer_value;
                    break;
                case CC_CHAR16:
                    node->type.basic.kind = p->lexer.cpp.target.char16_type;
                    node->uinteger = tok.constant.integer_value;
                    break;
                case CC_CHAR32:
                    node->type.basic.kind = p->lexer.cpp.target.char32_type;
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
            return cc_error(p, tok.loc, "Identifier lookup unimplemented");
            CcExpr* node = cc_alloc_expr(p, 0);
            if(!node) return CC_OOM_ERROR;
            node->kind = CC_EXPR_IDENTIFIER;
            node->loc = tok.loc;
            node->text = tok.ident.ident->data;
            node->extra = tok.ident.ident->length;
            *out = node;
            return 0;
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
                return cc_error(p, tok.loc, "sizeof not yet supported");
                CcExpr* operand;
                err = cc_parse_prefix(p, &operand);
                if(err) return err;
            }
            if(tok.kw.kw == CC__Countof){
                return cc_error(p, tok.loc, "_Countof not yet supported");
            }
            if(tok.kw.kw == CC_alignof){
                return cc_error(p, tok.loc, "alignof not yet supported");
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
                // text=member name (in union with value0), values[0]=operand
                CcExpr* mnode = cc_alloc_expr(p, 1);
                if(!mnode) return CC_OOM_ERROR;
                mnode->kind = mkind;
                mnode->loc = tok.loc;
                mnode->extra = member.ident.ident->length;
                mnode->text = member.ident.ident->data;
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
            CcPointer* p = (CcPointer*)(t.bits & ~(uintptr_t)7);
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
            CcArray* a = (CcArray*)(t.bits & ~(uintptr_t)7);
            cc_print_type_pre(sb, a->element);
            return;
        }
        case CC_FUNCTION: {
            CcFunction* f = (CcFunction*)(t.bits & ~(uintptr_t)7);
            cc_print_type_pre(sb, f->return_type);
            return;
        }
        case CC_STRUCT: {
            CcStruct* s = (CcStruct*)(t.bits & ~(uintptr_t)7);
            if(s->name) msb_sprintf(sb, "struct %.*s", s->name->length, s->name->data);
            else msb_write_literal(sb, "struct <anon>");
            return;
        }
        case CC_UNION: {
            CcUnion* u = (CcUnion*)(t.bits & ~(uintptr_t)7);
            if(u->name) msb_sprintf(sb, "union %.*s", u->name->length, u->name->data);
            else msb_write_literal(sb, "union <anon>");
            return;
        }
        case CC_ENUM: {
            CcEnum* e = (CcEnum*)(t.bits & ~(uintptr_t)7);
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
            CcPointer* p = (CcPointer*)(t.bits & ~(uintptr_t)7);
            if(cc_type_needs_parens(p->pointee))
                msb_write_char(sb, ')');
            cc_print_type_post(sb, p->pointee);
            return;
        }
        case CC_ARRAY: {
            CcArray* a = (CcArray*)(t.bits & ~(uintptr_t)7);
            if(a->is_incomplete)
                msb_write_literal(sb, "[]");
            else
                msb_sprintf(sb, "[%zu]", a->length);
            cc_print_type_post(sb, a->element);
            return;
        }
        case CC_FUNCTION: {
            CcFunction* f = (CcFunction*)(t.bits & ~(uintptr_t)7);
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
            // Check if it looks like a string (str.length set and text non-null)
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
            if(e->type.bits){
                msb_write_char(sb, ':');
                cc_print_type(sb, e->type);
            }
            return;
        case CC_EXPR_IDENTIFIER:
            msb_sprintf(sb, "%.*s", e->extra, e->text);
            return;
        case CC_EXPR_VARIABLE:
        case CC_EXPR_FUNCTION:
        case CC_EXPR_SIZEOF_VMT:
        case CC_EXPR_COMPOUND_LITERAL:
        case CC_EXPR_STATEMENT_EXPRESSION:
            msb_sprintf(sb, "<unimpl>");
            return;
        // Unary prefix
        #define UNOP(K, S) case K: msb_write_literal(sb, "(" S); if(e->type.bits){msb_write_char(sb, ':'); cc_print_type(sb, e->type);} msb_write_char(sb, ' '); cc_print_expr(sb, e->value0); msb_write_char(sb, ')'); return;
        UNOP(CC_EXPR_NEG,    "-")
        UNOP(CC_EXPR_POS,    "+")
        UNOP(CC_EXPR_BITNOT, "~")
        UNOP(CC_EXPR_LOGNOT, "!")
        UNOP(CC_EXPR_DEREF,  "*")
        UNOP(CC_EXPR_ADDR,   "&")
        UNOP(CC_EXPR_PREINC, "++pre")
        UNOP(CC_EXPR_PREDEC, "--pre")
        // Unary postfix
        UNOP(CC_EXPR_POSTINC, "post++")
        UNOP(CC_EXPR_POSTDEC, "post--")
        #undef UNOP
        // Binary ops
        #define BINOP(K, S) case K: msb_write_literal(sb, "(" S); if(e->type.bits){msb_write_char(sb, ':'); cc_print_type(sb, e->type);} msb_write_char(sb, ' '); cc_print_expr(sb, e->value0); msb_write_char(sb, ' '); cc_print_expr(sb, e->values[0]); msb_write_char(sb, ')'); return;
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
        BINOP(CC_EXPR_SUBSCRIPT,    "[]")
        #undef BINOP
        case CC_EXPR_TERNARY:
            msb_write_literal(sb, "(?");
            if(e->type.bits){ msb_write_char(sb, ':'); cc_print_type(sb, e->type); }
            msb_write_char(sb, ' ');
            cc_print_expr(sb, e->value0);
            msb_write_char(sb, ' ');
            cc_print_expr(sb, e->values[0]);
            msb_write_char(sb, ' ');
            cc_print_expr(sb, e->values[1]);
            msb_write_char(sb, ')');
            return;
        case CC_EXPR_CAST:
            msb_write_literal(sb, "(cast<");
            cc_print_type(sb, e->type);
            msb_write_literal(sb, "> ");
            cc_print_expr(sb, e->value0);
            msb_write_char(sb, ')');
            return;
        case CC_EXPR_DOT:
            msb_write_literal(sb, "(. ");
            cc_print_expr(sb, e->values[0]);
            msb_sprintf(sb, " %.*s)", e->extra, e->text);
            return;
        case CC_EXPR_ARROW:
            msb_write_literal(sb, "(-> ");
            cc_print_expr(sb, e->values[0]);
            msb_sprintf(sb, " %.*s)", e->extra, e->text);
            return;
        case CC_EXPR_CALL:
            msb_write_literal(sb, "(call ");
            cc_print_expr(sb, e->value0);
            for(uint32_t i = 0; i < e->call.nargs; i++){
                msb_write_char(sb, ' ');
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
cc_print_eval_result(CcEvalResult r){
    switch(r.kind){
        case CC_EVAL_INT:    printf("%lld", (long long)r.i); break;
        case CC_EVAL_UINT:   printf("%llu", (unsigned long long)r.u); break;
        case CC_EVAL_FLOAT:  printf("%g", (double)r.f); break;
        case CC_EVAL_DOUBLE: printf("%g", r.d); break;
        case CC_EVAL_ERROR:  fputs("<cannot evaluate>", stdout); break;
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
        default:
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
                    case CC_enum:
                        return cc_unimplemented(p, tok.loc, "enum parsing in declaration");
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
                    case CC_union:
                        return cc_unimplemented(p, tok.loc, "union parsing in declaration");
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
                    case CC_struct:
                        return cc_unimplemented(p, tok.loc, "struct parsing in declaration");
                    case CC_typeof:
                        goto do_typeof;
                    case CC_alignas:
                        return cc_unimplemented(p, tok.loc, "alignas parsing in declaration");
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
                        _Bool is_typename = peek.type == CC_KEYWORD;
                        if(!is_typename && peek.type == CC_IDENTIFIER){
                            CcSymbol sym;
                            is_typename = cc_scope_lookup_symbol(p->current, peek.ident.ident, CC_SCOPE_WALK_CHAIN, &sym) && sym.kind == CC_SYM_TYPEDEF;
                        }
                        if(is_typename){
                            CcDeclBase typeof_base = {.type.bits = (uintptr_t)-1};
                            err = cc_parse_declaration_specifier(p, &typeof_base.spec, &typeof_base.type);
                            if(err) return err;
                            err = cc_resolve_specifiers(p, &typeof_base);
                            if(err) return err;
                            CcQualType head = {0};
                            CcQualType* tail = &head;
                            err = cc_parse_declarator(p, &head, &tail, NULL, NULL);
                            if(err) return err;
                            *tail = typeof_base.type;
                            *base_type = cc_intern_qualtype(p, head);
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

#ifdef __clang__
#pragma clang assume_nonnull end
#endif
#ifndef MARRAY_CCQUALTYPE
#define MARRAY_CCQUALTYPE
#define MARRAY_T CcQualType
#include "../Drp/Marray.h"
#endif
#ifdef __clang__
#pragma clang assume_nonnull begin
#endif

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
    if(ccqt_is_basic(t)) return t;
    uintptr_t quals = t.bits & 7;
    switch(ccqt_kind(t)){
        case CC_POINTER: {
            CcPointer* old = (CcPointer*)(t.bits & ~(uintptr_t)7);
            CcQualType pointee = cc_intern_qualtype(p, old->pointee);
            CcPointer* ptr = cc_intern_pointer(&p->type_cache, cc_allocator(p), pointee, old->restrict_);
            if(!ptr) return t;
            return (CcQualType){.bits = (uintptr_t)ptr | quals};
        }
        case CC_ARRAY: {
            CcArray* old = (CcArray*)(t.bits & ~(uintptr_t)7);
            CcQualType elem = cc_intern_qualtype(p, old->element);
            CcArray* arr = cc_intern_array(&p->type_cache, cc_allocator(p), elem, old->length, old->is_static, old->is_incomplete);
            if(!arr) return t;
            return (CcQualType){.bits = (uintptr_t)arr | quals};
        }
        case CC_FUNCTION: {
            CcFunction* old = (CcFunction*)(t.bits & ~(uintptr_t)7);
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
            is_fndef = tail != &head
                    && !ccqt_is_basic(head)
                    && ccqt_kind(head) == CC_FUNCTION;
            *tail = declbase->type;
            type = cc_intern_qualtype(p, head);
        }
        // postfix processing
        err = cc_next_token(p, &tok);
        if(err) return err;
        _Bool stop = 0;
        for(;;){
            switch(tok.type){
                case CC_EOF:
                    stop = 1;
                    break;
                case CC_KEYWORD:
                    if(tok.kw.kw == CC_asm)
                        return cc_unimplemented(p, tok.loc, "asm mangling");
                    // Clang maintainers are assholes and don't support the comment version and the macro ifdef soup is like 8 lines
                    goto fallthrough;
                case CC_CONSTANT:
                case CC_STRING_LITERAL:
                case CC_IDENTIFIER:
                    fallthrough:;
                    return cc_error(p, tok.loc, "Expected ',' or ';'");
                case CC_PUNCTUATOR:
                    if(tok.punct.punct == ';'){
                        stop = 1;
                        break;
                    }
                    if(tok.punct.punct == ',')
                        break;
                    if(tok.punct.punct == '=')
                        return cc_unimplemented(p, tok.loc, "initializer");
                    if(first && tok.punct.punct == '{' && is_fndef)
                        return cc_unimplemented(p, tok.loc, "function body");
                    return cc_error(p, tok.loc, "Expected ',' or ';'");
            }
            break;
        }
        if(!name){
            if(stop) break;
            continue;
        }
        if(declbase->spec.sp_typedef){
            err = cc_scope_insert_typedef(cc_allocator(p), p->current, name, type);
            if(err) return err;
        }
        else if(is_fndef){
            CcFunc* func = Allocator_zalloc(cc_allocator(p), sizeof *func);
            if(!func) return CC_OOM_ERROR;
            *func = (CcFunc){
                .type = (CcFunction*)(type.bits & ~(uintptr_t)7),
                .name = name,
                .loc = tok.loc,
                .extern_ = declbase->spec.sp_extern,
                .static_ = declbase->spec.sp_static,
                .inline_ = declbase->spec.sp_inline,
                .params.count = param_names.count,
                .params.data = param_names.data,
            };
            err = cc_scope_insert_func(cc_allocator(p), p->current, name, func);
            if(err) return err;
        }
        else {
            if(declbase->spec.sp_inline)
                return cc_error(p, tok.loc, "'inline' is only valid on functions");
            if(declbase->spec.sp_noreturn)
                return cc_error(p, tok.loc, "'_Noreturn' is only valid on functions");
            CcVariable* var = Allocator_zalloc(cc_allocator(p), sizeof *var);
            if(!var) return CC_OOM_ERROR;
            *var = (CcVariable){
                .name = name,
                .loc = tok.loc,
                .type = type,
                .extern_ = declbase->spec.sp_extern,
                .static_ = declbase->spec.sp_static,
            };
            err = cc_scope_insert_var(cc_allocator(p), p->current, name, var);
            if(err) return err;
        }
        if(stop) break;
    }
    return err;
}

#ifdef __clang__
#pragma clang assume_nonnull end
#endif
#include "cc_type_cache.c"
#include "cc_scope.c"
#endif
