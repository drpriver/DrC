#ifndef C_CC_TYPE_CHECK_C
#define C_CC_TYPE_CHECK_C
//
// Copyright © 2026-2026, David Priver <david@davidpriver.com>
//
#include "../Drp/ckdint.h"
#include "cc_type_check.h"
#include "cc_expr.h"
#include "cc_var.h"
#include "cc_rt_types.h"
#include "cc_errors.h"
#include "nc_native_call.h"
#include <stddef.h>
#ifdef __clang__
#pragma clang assume_nonnull begin
#endif

// Only use this when it is absolutely impossible
#ifndef cc_unsafe_unreachable
#ifdef __GNUC__
#define cc_unsafe_unreachable() __builtin_unreachable()
#elif defined _MSC_VER
#define cc_unsafe_unreachable() __assume(0)
#else
#define cc_unsafe_unreachable() abort()
#endif
#endif

#define cc_type_check_fail(failure, why) ((void)(*failure = why), _cc_syntax_error)

static
_Bool
cc_type_predicate(const CcTargetConfig* target, CcTypeIntrospectionOp op, CcQualType qt){
    switch(op){
        case CC_TYPE_IS_VALID: return qt.bits != 0;
        case CC_TYPE_IS_INVALID: return qt.bits == 0;
        case CC_TYPE_IS_INTEGER: return ccqt_is_integer(qt);
        case CC_TYPE_IS_FLOAT: return ccqt_is_basic(qt) && ccbt_is_float(ccqt_basic_kind(qt));
        case CC_TYPE_IS_ARITHMETIC: return (ccqt_is_basic(qt) && ccbt_is_arithmetic(ccqt_basic_kind(qt))) || ccqt_kind(qt) == CC_ENUM;
        case CC_TYPE_IS_POINTER: return ccqt_kind(qt) == CC_POINTER;
        case CC_TYPE_IS_STRUCT: return ccqt_kind(qt) == CC_STRUCT;
        case CC_TYPE_IS_UNION: return ccqt_kind(qt) == CC_UNION;
        case CC_TYPE_IS_ARRAY: return ccqt_is_array(qt);
        case CC_TYPE_IS_VECTOR: return ccqt_kind(qt) == CC_VECTOR;
        case CC_TYPE_IS_SLICE: return ccqt_kind(qt) == CC_SLICE;
        case CC_TYPE_IS_FUNCTION: return ccqt_kind(qt) == CC_FUNCTION;
        case CC_TYPE_IS_ENUM: return ccqt_kind(qt) == CC_ENUM;
        case CC_TYPE_IS_CONST: return ccqt_array_object_type(qt).is_const;
        case CC_TYPE_IS_VOLATILE: return ccqt_array_object_type(qt).is_volatile;
        case CC_TYPE_IS_ATOMIC: return qt.is_atomic;
        case CC_TYPE_IS_UNSIGNED: return ccqt_is_unsigned(qt, !target->char_is_signed);
        case CC_TYPE_IS_SIGNED: return ccqt_is_integer(qt) && !ccqt_is_unsigned(qt, !target->char_is_signed);
        case CC_TYPE_IS_INCOMPLETE:
            return qt.bits && ccqt_kind(qt) != CC_FUNCTION && !ccqt_is_complete_object(qt);
        case CC_TYPE_IS_CALLABLE:
            return ccqt_callable_function(qt) != NULL;
        case CC_TYPE_IS_VARIADIC: {
            CcFunction* f = ccqt_callable_function(qt);
            return f && f->is_variadic;
        }
        default: return 0;
    }
}

static
CcQualType
cc_parameter_value_type(CcQualType t){
    t = ccqt_unaligned(t);
    t.quals = 0;
    return t;
}

static
CcQualType
cc_promote_integer_rank(CcQualType t){
    if(ccqt_is_basic(t)
    && ccbt_is_integer(ccqt_basic_kind(t))
    && ccbt_int_rank(ccqt_basic_kind(t)) < ccbt_int_rank(CCBT_int))
        return ccqt_basic(CCBT_int);
    return t;
}

static
CcQualType
cc_default_promotion(CcQualType t){
    t = ccqt_enum_underlying(t);
    t = ccqt_unaligned(t);
    t.quals = 0;
    if(ccqt_bt_eq(t, CCBT_float)) return ccqt_basic(CCBT_double);
    return cc_promote_integer_rank(t);
}

static
_Bool
cc_requires_default_promotion(CcQualType t){
    t = ccqt_enum_underlying(t);
    return cc_default_promotion(t).unqual != ccqt_unaligned(t).unqual;
}

static
_Bool
cc_is_value_operand_type(CcQualType t){
    // Value operands require complete objects, except arrays and functions
    // which can yield pointer values through decay.
    return ccqt_is_array(t) || ccqt_kind(t) == CC_FUNCTION || ccqt_is_complete_object(t);
}

typedef struct CcTypesMatchFlags CcTypesMatchFlags;
struct CcTypesMatchFlags {
    union {
        uint32_t _bits;
        struct {
            uint32_t same: 1, // identity instead of compatibility
                     anon: 1, // allow sructural matching of unnamed records
                     unqual: 1, // ignore outer qualifiers
                     ignore_cv: 1, // ignore only cv
                     _padding: 28; // free real estate
        };
    };
};

typedef struct CcTypePair CcTypePair;
struct CcTypePair {
    CcQualType a, b;
    const CcTypePair* _Nullable previous;
};


static
_Bool
cc_layout_array_stride_valid(uint64_t element_size, uint32_t alignment){
    return !alignment || (element_size >= alignment && element_size % alignment == 0);
}

static
int
cc_layout_array_size(uint64_t element_size, size_t length, uint32_t* out){
    uint64_t size;
    if(mul_overflow(element_size, (uint64_t)length, &size) || size > UINT32_MAX)
        return _cc_overflow_error;
    *out = (uint32_t)size;
    return 0;
}

static
int
cc_type_sizeof_complete(const CcTargetConfig* tc, CcQualType type, uint32_t* out,
    CcQualType* _Nullable failure_type){
    int err = _cc_not_constant_error;
    switch(ccqt_kind(type)){
        DRP_CASES_EXHAUSTED;
        case CC_VLA: break;
        case CC_ALIGNED: err = _cc_unreachable_error; break;
        case CC_BASIC:
            if(ccqt_basic_kind(type) >= CCBT_COUNT){
                err = _cc_unreachable_error;
                break;
            }
            *out = tc->sizeof_[ccqt_basic_kind(type)]; return 0;
        case CC_POINTER:
        case CC_BLOCK_POINTER:
            *out = tc->sizeof_[CCBT_nullptr_t];
            return 0;
        case CC_FUNCTION: err = _cc_unreachable_error; break;
        case CC_SLICE:    *out = 2*tc->sizeof_[CCBT_nullptr_t]; return 0;
        case CC_ENUM:
            if(ccqt_as_enum(type)->is_incomplete) break;
            return cc_type_sizeof_complete(tc, ccqt_as_enum(type)->underlying, out, failure_type);
        case CC_STRUCT:
        case CC_UNION:
            if(ccqt_as_struct(type)->is_incomplete) break;
            *out = ccqt_as_struct(type)->size;
            return 0;
        case CC_VECTOR: *out = ccqt_as_vector(type)->vector_size; return 0;
        case CC_ARRAY:{
            CcArray* a = ccqt_as_array(type);
            if(a->is_incomplete) break;
            uint32_t element_size;
            err = cc_type_sizeof_complete(tc, a->element, &element_size, failure_type);
            if(err) return err;
            err = cc_layout_array_size(element_size, a->length, out);
            if(!err) return 0;
            break;
        }
    }
    if(failure_type) *failure_type = type;
    return err;
}

static _Bool cc_types_match_inner(CcQualType a, CcQualType b, unsigned depth, const CcTypePair* _Nullable pairs, CcTypesMatchFlags flags);

static
_Bool
cc_vla_bounds_same(const CcVla* a, const CcVla* b){
    if(a->unspecified != b->unspecified) return 0;
    if(a->bound == b->bound) return 1;
    if(a->bound->kind == CC_EXPR_VARIABLE
    && b->bound->kind == CC_EXPR_VARIABLE
    && a->bound->var->vla_bound
    && b->bound->var->vla_bound)
        return a->bound->var == b->bound->var;
    return a->bound->kind == CC_EXPR_VALUE
        && b->bound->kind == CC_EXPR_VALUE
        && a->bound->type.bits == b->bound->type.bits
        && a->bound->uinteger == b->bound->uinteger;
}


static
_Bool
cc_record_members_match_inner(CcStruct *a, CcStruct *b, unsigned depth, const CcTypePair *_Nullable pairs, _Bool same){
    if(a->field_count != b->field_count) return 0;
    if(a->packed != b->packed) return 0;
    if(a->size != b->size) return 0;
    if(a->alignment != b->alignment) return 0;
    for(uint32_t i = 0; i < a->field_count; i++){
        CcField* x = &a->fields[i];
        CcField* y = &b->fields[i];
        if(x->name != y->name) return 0;
        if(x->alignment != y->alignment) return 0;
        if(x->layout_alignment != y->layout_alignment) return 0;
        if(x->packed != y->packed) return 0;
        if(x->offset != y->offset) return 0;
        if(x->is_bitfield != y->is_bitfield) return 0;
        if(x->bitwidth != y->bitwidth) return 0;
        if(x->is_bitfield && x->bitoffset != y->bitoffset) return 0;
        if(ccqt_kind(x->type) == CC_ARRAY
        && ccqt_kind(y->type) == CC_ARRAY
        && ccqt_as_array(x->type)->is_incomplete != ccqt_as_array(y->type)->is_incomplete)
            return 0;
        CcTypesMatchFlags flags = {.same = same, .anon = !x->name && !x->is_bitfield};
        if(!cc_types_match_inner(x->type, y->type, depth+1, pairs, flags))
            return 0;
    }
    return 1;
}

static
_Bool
cc_record_members_match(CcStruct *a, CcStruct *b, _Bool same){
    return cc_record_members_match_inner(a, b, 0, NULL, same);
}

static
_Bool
cc_types_match_inner(CcQualType a, CcQualType b, unsigned depth, const CcTypePair *_Nullable pairs, CcTypesMatchFlags flags){
    if(depth >= 256) return 0;
    if(flags.same){
        if(cc_types_same(a, b)) return 1;
        if(!flags.anon
        || ccqt_kind(a) != ccqt_kind(b)
        || (ccqt_kind(a) != CC_STRUCT && ccqt_kind(a) != CC_UNION))
            return 0;
        if(ccqt_raw_kind(a) == CC_ALIGNED || ccqt_raw_kind(b) == CC_ALIGNED){
            if(ccqt_raw_kind(a) != ccqt_raw_kind(b)
            || ccqt_as_aligned(a)->alignment != ccqt_as_aligned(b)->alignment)
                return 0;
        }
    }
    a = ccqt_unaligned(a);
    b = ccqt_unaligned(b);
    if(flags.unqual)
        a.quals = b.quals = 0;
    else if(flags.ignore_cv){
        a.is_const = b.is_const = 0;
        a.is_volatile = b.is_volatile = 0;
    }
    if(a.bits == b.bits)
        return 1;
    if(ccqt_is_array(a) && ccqt_is_array(b)){
        if(ccqt_kind(a) == CC_ARRAY && ccqt_kind(b) == CC_ARRAY){
            CcArray* x = ccqt_as_array(a);
            CcArray* y = ccqt_as_array(b);
            if(!x->is_incomplete && !y->is_incomplete && x->length != y->length)
                return 0;
        }
        CcTypesMatchFlags f = {.unqual = flags.unqual, .ignore_cv = flags.ignore_cv};
        return cc_types_match_inner(ccqt_qualified_array_element(a), ccqt_qualified_array_element(b), depth+1, pairs, f);
    }
    if(a.quals != b.quals)
        return 0;
    if(ccqt_kind(a) != ccqt_kind(b)){
        CcQualType enumeration = ccqt_kind(a) == CC_ENUM ? a : b;
        CcQualType integer = ccqt_kind(a) == CC_ENUM ? b : a;
        if(ccqt_kind(enumeration) != CC_ENUM
        || !ccqt_is_integer(integer)
        || ccqt_kind(integer) != CC_BASIC)
            return 0;
        CcEnum* e = ccqt_as_enum(enumeration);
        if(e->is_incomplete) return 0;
        CcQualType underlying = e->underlying;
        underlying.quals |= enumeration.quals;
        return cc_types_match_inner(underlying, integer, depth+1, pairs, flags);
    }
    CcTypePair pair = {a, b, pairs};
    switch(ccqt_kind(a)){
        case CC_VLA: return 0;
        case CC_ALIGNED: cc_unsafe_unreachable();
        case CC_POINTER:
        case CC_BLOCK_POINTER:
            return ccqt_as_ptr(a)->restrict_ == ccqt_as_ptr(b)->restrict_
                && cc_types_match_inner(ccqt_as_ptr(a)->pointee, ccqt_as_ptr(b)->pointee, depth+1, pairs, (CcTypesMatchFlags){0});
        case CC_SLICE:
            return ccqt_as_slice(a)->restrict_ == ccqt_as_slice(b)->restrict_
                && cc_types_match_inner(ccqt_as_slice(a)->pointee, ccqt_as_slice(b)->pointee, depth+1, pairs, (CcTypesMatchFlags){0});
        case CC_VECTOR: {
            CcVector* x = ccqt_as_vector(a);
            CcVector* y = ccqt_as_vector(b);
            return x->vector_size == y->vector_size
                && x->length == y->length
                && cc_types_match_inner(x->element, y->element, depth+1, pairs, (CcTypesMatchFlags){.unqual=flags.unqual});
        }
        case CC_ARRAY: cc_unsafe_unreachable();
        case CC_FUNCTION: {
            CcFunction* x = ccqt_as_function(a);
            CcFunction* y = ccqt_as_function(b);
            if(!cc_types_match_inner(x->return_type, y->return_type, depth+1, pairs, (CcTypesMatchFlags){0}))
                return 0;
            if(x->no_prototype || y->no_prototype){
                CcFunction* proto = x->no_prototype ? y : x;
                if(proto->is_variadic) return 0;
                for(uint32_t i = 0; i < proto->param_count; i++){
                    if(cc_requires_default_promotion(proto->params[i]))
                        return 0;
                }
                return 1;
            }
            if(x->param_count != y->param_count || x->is_variadic != y->is_variadic)
                return 0;
            for(uint32_t i = 0; i < x->param_count; i++){
                CcQualType at = cc_parameter_value_type(x->params[i]);
                CcQualType bt = cc_parameter_value_type(y->params[i]);
                if(!cc_types_match_inner(at, bt, depth+1, pairs, (CcTypesMatchFlags){0}))
                    return 0;
            }
            return 1;
        }
        case CC_STRUCT:
        case CC_UNION: {
            CcStruct* x = ccqt_as_struct(a);
            CcStruct* y = ccqt_as_struct(b);
            for(const CcTypePair* prev = pairs; prev; prev = prev->previous)
                if(prev->a.unqual == a.unqual && prev->b.unqual == b.unqual)
                    return 1;
            if(x->is_incomplete || y->is_incomplete || x->name != y->name || (!x->name && !flags.anon))
                return 0;
            return cc_record_members_match_inner(x, y, depth, &pair, flags.same);
        }
        case CC_ENUM: {
            CcEnum* x = ccqt_as_enum(a);
            CcEnum* y = ccqt_as_enum(b);
            if(x->is_incomplete || y->is_incomplete) return 0;
            if(!x->name || x->name != y->name) return 0;
            if(x->has_fixed_underlying != y->has_fixed_underlying) return 0;
            if(x->underlying.bits != y->underlying.bits) return 0;
            if(x->enumerator_count != y->enumerator_count) return 0;
            for(size_t i = 0; i < x->enumerator_count; i++){
                CcEnumerator* en = x->enumerators[i];
                size_t j;
                for(j = 0; j < y->enumerator_count; j++)
                    if(en->name == y->enumerators[j]->name)
                        break;
                if(j == y->enumerator_count || !ci_uint128_eq(en->value, y->enumerators[j]->value))
                    return 0;
            }
            return 1;
        }
        case CC_BASIC: return 0;
        DRP_CASES_EXHAUSTED;
    }
}

static
_Bool
cc_types_match(CcQualType a, CcQualType b, CcTypesMatchFlags flags){
    return cc_types_match_inner(a, b, 0, NULL, flags);
}

static
_Bool
cc_method_types_compatible(CcQualType a, CcQualType b, CcQualType owner_a, CcQualType owner_b){
    CcTypePair owners = {owner_a, owner_b, NULL};
    return cc_types_match_inner(a, b, 0, &owners, (CcTypesMatchFlags){0});
}

static
_Bool
cc_types_same(CcQualType a, CcQualType b){
    // This holds for now, but might change, depending on whether
    // "same" is the regular english word or a term of art (smh).
    return a.bits == b.bits;
}

static
_Bool
cc_types_compatible(CcQualType a, CcQualType b){
    return cc_types_match(a, b, (CcTypesMatchFlags){0});
}

static
_Bool
cc_types_compatible_unqualified(CcQualType a, CcQualType b){
    return cc_types_match(a, b, (CcTypesMatchFlags){.unqual=1});
}

static
_Bool
cc_types_compatible_ignoring_cv(CcQualType a, CcQualType b){
    return cc_types_match(a, b, (CcTypesMatchFlags){.ignore_cv=1});
}

static
_Bool
cc_arrays_copy_compatible(CcQualType a, CcQualType b){
    if(!ccqt_is_array(a) || !ccqt_is_array(b))
        return 0;
    for(unsigned depth = 0; depth < 256; depth++){
        if(!ccqt_is_array(a) || !ccqt_is_array(b))
            return !ccqt_is_array(a)
                && !ccqt_is_array(b)
                && cc_types_compatible_ignoring_cv(a, b);
        if(ccqt_kind(a) != ccqt_kind(b))
            return 0;
        if(ccqt_kind(a) == CC_ARRAY){
            CcArray* x = ccqt_as_array(a);
            CcArray* y = ccqt_as_array(b);
            if(x->is_incomplete || y->is_incomplete || x->length != y->length)
                return 0;
        }
        else if(ccqt_as_vla(a)->unspecified || !cc_vla_bounds_same(ccqt_as_vla(a), ccqt_as_vla(b)))
            return 0;
        a = ccqt_qualified_array_element(a);
        b = ccqt_qualified_array_element(b);
    }
    return 0;
}



static
_Bool
cc_any_payload_type(const CcTargetConfig* target, CcQualType t){
    switch(ccqt_kind(t)){
        case CC_VLA: return 0;
        case CC_ALIGNED: cc_unsafe_unreachable();
        case CC_BASIC:
            if(ccqt_basic_kind(t) == CCBT_INVALID || ccqt_basic_kind(t) == CCBT_void)
                return 0;
            // This is kind of janky, we should probably have the payload
            // size be part of the target config?
            return target->sizeof_[ccqt_basic_kind(t)] <= sizeof(((CiRtAny*)0)->payload);
        case CC_POINTER:
        case CC_BLOCK_POINTER:
            return 1;
        case CC_STRUCT:
            return !ccqt_as_struct(t)->is_incomplete
                && ccqt_as_struct(t)->size <= sizeof(((CiRtAny*)0)->payload);
        case CC_UNION:
            return !ccqt_as_union(t)->is_incomplete
                && ccqt_as_union(t)->size <= sizeof(((CiRtAny*)0)->payload);
        case CC_ENUM:
            return !ccqt_as_enum(t)->is_incomplete
                && cc_any_payload_type(target, ccqt_as_enum(t)->underlying);
        case CC_ARRAY: return 0;
        case CC_VECTOR: {
            CcVector* a = ccqt_as_vector(t);
            return a->vector_size <= sizeof ((CiRtAny*)0)->payload;
        }
        case CC_FUNCTION: case CC_SLICE:
            return 0;
    }
    return 0;
}

static inline
_Bool
cc_any_convertible(const CcTargetConfig* target, CcQualType from){
    CcTypeKind k = ccqt_kind(from);
    if(k == CC_FUNCTION || k == CC_VLA) return 1;
    if(k == CC_ARRAY) return 1;
    return cc_any_payload_type(target, from);
}

static
_Bool
cc_qualification_convertible(CcQualType from, CcQualType to){
    return cc_types_compatible_ignoring_cv(from, to) && (cc_object_quals(from) & ~cc_object_quals(to)) == 0;
}

static
_Bool
cc_plan9_base(CcQualType from, CcQualType to){
    from = ccqt_unaligned(from);
    to = ccqt_unaligned(to);
    CcTypeKind fk = ccqt_kind(from),
               tk = ccqt_kind(to);
    if((fk != CC_STRUCT && fk != CC_UNION) || (tk != CC_STRUCT && tk != CC_UNION))
        return 0;
    if(cc_qualification_convertible(from, to))
        return 1;
    CcStruct* s = ccqt_as_struct(from);
    for(uint32_t i = 0; i < s->field_count; i++){
        CcField* f = &s->fields[i];
        if(f->name || f->is_bitfield) continue;
        CcQualType sub = f->type;
        sub.quals |= from.quals;
        if(cc_plan9_base(sub, to)){
            return 1;
        }
    }
    return 0;
}

static
_Bool
cc_pointer_pointee_convertible(CcQualType from, CcQualType to){
    if(ccqt_bt_eq(from, CCBT_void) || ccqt_bt_eq(to, CCBT_void)){
        CcQualType source = ccqt_array_object_type(from),
                   target = ccqt_array_object_type(to);
        source.is_atomic = target.is_atomic = 0;
        return (source.quals & ~target.quals) == 0;
    }
    if(cc_qualification_convertible(from, to))
        return 1;
    return from.is_atomic == to.is_atomic && cc_plan9_base(from, to);
}

static
_Bool
cc_implicit_convertible(const CcTargetConfig* target, CcQualType from, CcQualType to){
    from = ccqt_unaligned(from);
    to = ccqt_unaligned(to);
    if(!cc_is_value_operand_type(from) || !cc_is_value_operand_type(to))
        return 0;
    if(from.bits == to.bits)
        return 1;
    if(ccqt_bt_eq(to, CCBT__Any))
        return ccqt_bt_eq(from, CCBT__Any)
            || cc_any_convertible(target, from);
    CcTypeKind fk = ccqt_kind(from), tk = ccqt_kind(to);
    if(fk == CC_BASIC && tk == CC_BASIC && ccqt_basic_kind(from) == ccqt_basic_kind(to))
        return 1;
    _Bool f_arith = (fk == CC_BASIC && ccbt_is_arithmetic(ccqt_basic_kind(from))) || fk == CC_ENUM;
    _Bool t_arith = (tk == CC_BASIC && ccbt_is_arithmetic(ccqt_basic_kind(to))) || tk == CC_ENUM;
    if(f_arith && t_arith)
        return 1;
    if((fk == CC_STRUCT || fk == CC_UNION) && fk == tk)
        return cc_types_compatible((CcQualType){.unqual=from.unqual}, (CcQualType){.unqual=to.unqual});
    if(ccqt_is_array(from) && ccqt_is_array(to))
        return cc_arrays_copy_compatible(from, to);
    if(fk == CC_POINTER && tk == CC_POINTER){
        CcQualType fp = ccqt_as_ptr(from)->pointee;
        CcQualType tp = ccqt_as_ptr(to)->pointee;
        return cc_pointer_pointee_convertible(fp, tp);
    }
    if(fk == CC_SLICE && tk == CC_SLICE){
        CcQualType fp = ccqt_as_slice(from)->pointee;
        CcQualType tp = ccqt_as_slice(to)->pointee;
        return cc_qualification_convertible(fp, tp);
    }
    if(ccqt_is_array(from) && tk == CC_SLICE){
        if(fk == CC_ARRAY && ccqt_as_array(from)->is_incomplete) return 0;
        CcQualType ep = ccqt_qualified_array_element(from);
        CcQualType tp = ccqt_as_slice(to)->pointee;
        return cc_qualification_convertible(ep, tp);
    }
    if(ccqt_is_array(from) && tk == CC_POINTER){
        CcQualType ep = ccqt_qualified_array_element(from);
        CcQualType tp = ccqt_as_ptr(to)->pointee;
        return cc_pointer_pointee_convertible(ep, tp);
    }
    if(fk == CC_FUNCTION && tk == CC_POINTER)
        return cc_pointer_pointee_convertible(from, ccqt_as_ptr(to)->pointee);
    if(fk == CC_BASIC && ccqt_basic_kind(from) == CCBT_nullptr_t && tk == CC_POINTER)
        return 1;
    if(fk == CC_BASIC && ccqt_basic_kind(from) == CCBT_nullptr_t
    && tk == CC_BASIC && ccqt_basic_kind(to) == CCBT_nullptr_t)
        return 1;
    if(tk == CC_BASIC && ccqt_basic_kind(to) == CCBT_bool){
        if(fk == CC_POINTER) return 1;
        if(ccqt_is_array(from)) return 1;
        if(fk == CC_FUNCTION) return 1;
        if(fk == CC_BASIC && ccqt_basic_kind(from) == CCBT_nullptr_t) return 1;
    }
    if(fk == CC_VECTOR && tk == CC_VECTOR)
        return from.unqual == to.unqual;
    return 0;
}

static
_Bool
cc_function_accepts_arity(const CcFunction* f, uint32_t count){
    return f->no_prototype
        || (f->is_variadic ? count >= f->param_count : count == f->param_count);
}

static inline
_Bool
cc_function_has_value_types(const CcFunction* f){
    if(!ccqt_bt_eq(f->return_type, CCBT_void)
    && !ccqt_is_complete_object(f->return_type))
        return 0;
    for(uint32_t i = 0; i < f->param_count; i++)
        if(!ccqt_is_complete_object(f->params[i]))
            return 0;
    return 1;
}

static
_Bool
cc_is_callable_with(const CcTargetConfig* target, CcQualType type, const CcQualType *_Null_unspecified args, uint32_t nargs){
    CcFunction* f = ccqt_callable_function(type);
    if(!f) return 0;
    if(!cc_function_has_value_types(f)) return 0;
    if(!cc_function_accepts_arity(f, nargs)) return 0;
    for(uint32_t i = 0; i < nargs; i++){
        if(!cc_is_value_operand_type(args[i])) return 0;
        if(!f->no_prototype
        && i < f->param_count
        && !cc_implicit_convertible(target, args[i], f->params[i]))
            return 0;
    }
    return 1;
}

static
_Bool
cc_call_abi_type_equal(const CcTargetConfig* target, CcQualType a, CcQualType b, NcAbiRole role){
    a = ccqt_enum_underlying(ccqt_unaligned(a));
    b = ccqt_enum_underlying(ccqt_unaligned(b));
    if(ccqt_bt_eq(a, CCBT_bool) != ccqt_bt_eq(b, CCBT_bool)) return 0;
    if(ccqt_bt_eq(a, CCBT__Any) != ccqt_bt_eq(b, CCBT__Any)) return 0;
    if(a.unqual != b.unqual
    && ((!ccqt_is_basic(a) && !ccqt_is_some_kind_of_pointer(a))
        || (!ccqt_is_basic(b) && !ccqt_is_some_kind_of_pointer(b))))
        return 0;
    NcAbiType x, y;
    if(nc_abi_type(target, a, role, &x) || nc_abi_type(target, b, role, &y))
        return 0;
    if(memcmp(&x, &y, offsetof(NcAbiType, pieces)))
        return 0;
    if(memcmp(x.pieces, y.pieces, x.count * sizeof x.pieces[0]))
        return 0;
    return 1;
}

static
_Bool
cc_is_callable_through(const CcTargetConfig* target, CcQualType from, CcQualType through){
    CcFunction* f = ccqt_callable_function(from);
    if(!f || ccqt_kind(through) != CC_FUNCTION) return 0;
    CcFunction* t = ccqt_as_function(through);
    if(!cc_function_has_value_types(f)
    || !cc_function_has_value_types(t))
        return 0;
    if(((f->no_prototype || t->no_prototype) && f != t)
    || f->is_variadic != t->is_variadic
    || f->param_count != t->param_count
    || f->fixed_param_count != t->fixed_param_count)
        return 0;
    if(!cc_call_abi_type_equal(target, f->return_type, t->return_type, NC_ABI_RETURN))
        return 0;
    for(uint32_t i = 0; i < f->param_count; i++){
        NcAbiRole role = i < f->fixed_param_count ? NC_ABI_NAMED_ARGUMENT : NC_ABI_VARIADIC_ARGUMENT;
        if(!cc_call_abi_type_equal(target, f->params[i], t->params[i], role))
            return 0;
    }
    return 1;
}

static
_Bool
cc_string_array_element(const CcTargetConfig* target, CcQualType element){
    if(!ccqt_is_basic(element)) return 0;
    CcBasicTypeKind kind = ccqt_basic_kind(element);
    return kind == CCBT_char
        || kind == CCBT_signed_char
        || kind == CCBT_unsigned_char
        || kind == target->wchar_type
        || kind == target->char16_type
        || kind == target->char32_type;
}

static inline
_Bool
cc_type_contains_fam(CcQualType type){
    if(ccqt_kind(type) == CC_STRUCT)
        return ccqt_as_struct(type)->has_fam;
    if(ccqt_kind(type) == CC_UNION){
        CcUnion* u = ccqt_as_union(type);
        for(uint32_t i = 0; i < u->field_count; i++)
            if(cc_type_contains_fam(u->fields[i].type))
                return 1;
    }
    return 0;
}

static
_Bool
cc_constexpr_forbidden_qualifiers(CcQualType type){
    if(type.is_volatile || type.is_atomic)
        return 1;
    if(ccqt_is_array(type))
        return cc_constexpr_forbidden_qualifiers(ccqt_array_element(type));
    if(ccqt_kind(type) == CC_STRUCT || ccqt_kind(type) == CC_UNION){
        CcStruct* record = ccqt_as_struct(type);
        for(uint32_t i = 0; i < record->field_count; i++)
            if(cc_constexpr_forbidden_qualifiers(record->fields[i].type))
                return 1;
    }
    return 0;
}

static
int
cc_check_cast_types(const CcTargetConfig* target, CcQualType from, CcQualType to, const char* _Null_unspecified* _Nonnull failure){
    from = ccqt_unaligned(from);
    to = ccqt_unaligned(to);
    CcTypeKind fk = ccqt_kind(from), tk = ccqt_kind(to);
    if(tk == CC_ARRAY || tk == CC_VLA)
        return cc_type_check_fail(failure, "cannot cast to array type");
    if(tk == CC_FUNCTION)
        return cc_type_check_fail(failure, "cannot cast to function type");
    if(!ccqt_bt_eq(to, CCBT_void) && !ccqt_is_complete_object(to))
        return cc_type_check_fail(failure, "cannot cast to incomplete type");
    if(from.unqual == to.unqual) return 0;
    if(ccqt_bt_eq(to, CCBT__Any) && (ccqt_bt_eq(from, CCBT__Any) || cc_any_convertible(target, from)))
        return 0;
    if(ccqt_is_basic(to) && ccqt_basic_kind(to) == CCBT_void) return 0;
    if(!ccqt_bt_eq(from, CCBT_void) && !cc_is_value_operand_type(from))
        return cc_type_check_fail(failure, "cannot cast from incomplete type");
    if(fk == CC_ENUM){
        from = ccqt_as_enum(from)->underlying;
        fk = ccqt_kind(from);
    }
    if(tk == CC_ENUM){
        to = ccqt_as_enum(to)->underlying;
        tk = ccqt_kind(to);
    }
    if(tk == CC_STRUCT || tk == CC_UNION){
        if(from.ptr == to.ptr) return 0;
        return cc_type_check_fail(failure, "cannot cast to struct or union type");
    }
    if(fk == CC_STRUCT || fk == CC_UNION)
        return cc_type_check_fail(failure, "cannot cast from struct or union type");
    if(fk == CC_BASIC && ccqt_basic_kind(from) == CCBT_void)
        return cc_type_check_fail(failure, "cannot cast from void");
    if(fk == CC_SLICE && tk == CC_SLICE){
        if(cc_types_compatible_ignoring_cv(ccqt_as_slice(from)->pointee, ccqt_as_slice(to)->pointee))
            return 0;
        return cc_type_check_fail(failure, "cannot cast to slice of different type");
    }
    if(ccqt_is_array(from) && tk == CC_SLICE){
        if(fk == CC_ARRAY && ccqt_as_array(from)->is_incomplete)
            return cc_type_check_fail(failure, "cannot cast to slice from incomplete array or vector");
        if(cc_types_compatible_ignoring_cv(ccqt_qualified_array_element(from), ccqt_as_slice(to)->pointee))
            return 0;
        return cc_type_check_fail(failure, "cannot cast to slice of different type");
    }
    _Bool f_arith = fk == CC_BASIC && ccbt_is_arithmetic(ccqt_basic_kind(from));
    _Bool t_arith = tk == CC_BASIC && ccbt_is_arithmetic(ccqt_basic_kind(to));
    if(f_arith && t_arith) return 0;
    _Bool f_ptr = fk == CC_POINTER
        || (ccqt_is_array(from))
        || fk == CC_FUNCTION
        || (fk == CC_BASIC && ccqt_basic_kind(from) == CCBT_nullptr_t);
    _Bool t_ptr = tk == CC_POINTER
        || (tk == CC_BASIC && ccqt_basic_kind(to) == CCBT_nullptr_t);
    if(f_ptr && t_ptr) return 0;
    if(f_ptr && tk == CC_BASIC && ccbt_is_integer(ccqt_basic_kind(to))) return 0;
    if(t_ptr && fk == CC_BASIC && ccbt_is_integer(ccqt_basic_kind(from))) return 0;
    return cc_type_check_fail(failure, "invalid cast");
}

static
_Bool
cc_explicit_castable(const CcTargetConfig *target, CcQualType from, CcQualType to){
    const char* failure;
    return !cc_check_cast_types(target, from, to, &failure);
}

static
void
cc_promote_bitfield_operand(const CcTargetConfig *target, CcQualType original, uint32_t width, CcQualType *out){
    CcQualType type = original;
    type = ccqt_enum_underlying(type);
    if(!ccqt_is_basic(type) || !ccbt_is_arithmetic(ccqt_basic_kind(type))){
        *out = original;
        return;
    }
    if(width
    && ccqt_is_integer(type)
    && ccbt_int_rank(ccqt_basic_kind(type)) <= ccbt_int_rank(CCBT_int)
    && (!ccqt_is_unsigned(type, !target->char_is_signed) || width < target->sizeof_[CCBT_int]*8))
        *out = ccqt_basic(CCBT_int);
    else
        *out = (CcQualType){.unqual=original.unqual};
}

static
int
cc_integer_promotion(CcQualType t, CcQualType *out, const char *_Null_unspecified *_Nonnull failure){
    t = ccqt_unaligned(t);
    t = ccqt_enum_underlying(t);
    t = (CcQualType){.unqual=t.unqual};
    if(!ccqt_is_basic(t))
        return cc_type_check_fail(failure, "integer promotion requires arithmetic type");
    CcBasicTypeKind k = ccqt_basic_kind(t);
    if(k == CCBT_void)
        return cc_type_check_fail(failure, "integer promotion of void");
    if(ccbt_is_float(k) || ccbt_is_complex(k)){
        *out = t;
        return 0;
    }
    if(!ccbt_is_integer(k))
        return cc_type_check_fail(failure, "integer promotion requires integer type");
    *out = cc_promote_integer_rank(t);
    return 0;
}

static
int
cc_usual_arithmetic_conversions(const CcTargetConfig* target, CcQualType a, CcQualType b, CcQualType* out, const char* _Null_unspecified* _Nonnull failure){
    int err;
    if(ccqt_kind(a) == CC_ENUM)
        a = ccqt_as_enum(a)->underlying;
    if(ccqt_kind(b) == CC_ENUM)
        b = ccqt_as_enum(b)->underlying;
    if(!ccqt_is_basic(a) || !ccqt_is_basic(b))
        return cc_type_check_fail(failure, "usual arithmetic conversions require arithmetic types");
    CcBasicTypeKind ak = ccqt_basic_kind(a), bk = ccqt_basic_kind(b);
    if(ccbt_is_complex(ak) || ccbt_is_complex(bk)){
        CcQualType real_a = ccqt_basic(ccbt_is_complex(ak) ? ccbt_complex_real(ak) : ak);
        CcQualType real_b = ccqt_basic(ccbt_is_complex(bk) ? ccbt_complex_real(bk) : bk);
        CcQualType real;
        err = cc_usual_arithmetic_conversions(target, real_a, real_b, &real, failure);
        if(err) return err;
        CcBasicTypeKind comp = ccbt_complex_kind(ccqt_basic_kind(real));
        if(comp == CCBT_INVALID)
            return cc_type_check_fail(failure, "UNIMPLEMENTED: extended precision complex arithmetic");
        *out = ccqt_basic(comp);
        return 0;
    }
    if(ak == CCBT_void || bk == CCBT_void)
        return cc_type_check_fail(failure, "usual arithmetic conversions on void");
    if(ak == CCBT_float128 || bk == CCBT_float128){
        *out = ccqt_basic(CCBT_float128);
        return 0;
    }
    if(ak == CCBT_long_double || bk == CCBT_long_double){
        *out = ccqt_basic(CCBT_long_double);
        return 0;
    }
    if(ak == CCBT_double || bk == CCBT_double){
        *out = ccqt_basic(CCBT_double);
        return 0;
    }
    if(ak == CCBT_float || bk == CCBT_float){
        *out = ccqt_basic(CCBT_float);
        return 0;
    }
    if(ak == CCBT_float16 || bk == CCBT_float16){
        *out = ccqt_basic(CCBT_float16);
        return 0;
    }
    if(ak == CCBT_bfloat16 || bk == CCBT_bfloat16){
        *out = ccqt_basic(CCBT_bfloat16);
        return 0;
    }
    CcQualType ap, bp;
    err = cc_integer_promotion(a, &ap, failure);
    if(err) return err;
    err = cc_integer_promotion(b, &bp, failure);
    if(err) return err;
    ak = ccqt_basic_kind(ap);
    bk = ccqt_basic_kind(bp);
    if(ak == bk){
        *out = ap;
        return 0;
    }
    _Bool char_is_unsigned = !target->char_is_signed;
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
    if(target->sizeof_[s] > target->sizeof_[u]){
        *out = ccqt_basic(s);
        return 0;
    }
    *out = ccqt_basic(ccbt_to_unsigned(s));
    return 0;
}

static
int
cc_deref_type(CcQualType t, CcQualType* out, _Bool subscript, const char* _Null_unspecified* _Nonnull failure){
    if(!ccqt_is_basic(t)){
        CcTypeKind kind = ccqt_kind(t);
        if(kind == CC_POINTER){
            *out = ccqt_as_ptr(t)->pointee;
            return 0;
        }
        if(kind == CC_VLA || kind == CC_ARRAY || (subscript && kind == CC_VECTOR)){
            *out = ccqt_qualified_array_element(t);
            return 0;
        }
        if(kind == CC_SLICE){
            *out = ccqt_as_slice(t)->pointee;
            return 0;
        }
    }
    return cc_type_check_fail(failure, "dereferencing non-pointer type");
}

static
_Bool
cc_type_has_size(CcQualType type){
    return ccqt_is_complete_object(type) || ccqt_bt_eq(type, CCBT_void);
}

static
int
cc_check_atomic_specifier(CcQualType type, const char *_Null_unspecified *_Nonnull failure){
    CcTypeKind kind = ccqt_kind(type);
    if(kind == CC_ARRAY || kind == CC_VECTOR || kind == CC_VLA)
        return cc_type_check_fail(failure, "_Atomic type name shall not refer to an array type");
    if(kind == CC_FUNCTION)
        return cc_type_check_fail(failure, "_Atomic type name shall not refer to a function type");
    if(type.is_atomic)
        return cc_type_check_fail(failure, "_Atomic type name shall not refer to an atomic type");
    if(type.is_const || type.is_volatile)
        return cc_type_check_fail(failure, "_Atomic type name shall not refer to a qualified type");
    return 0;
}

static
_Bool
cc_restrict_qualifiable(CcQualType type){
    return ccqt_is_some_kind_of_pointer(type);
}

static
int
cc_check_atomic_qualifier(CcQualType type, const char *_Null_unspecified *_Nonnull failure){
    CcTypeKind kind = ccqt_kind(type);
    if(kind == CC_ARRAY || kind == CC_VECTOR || kind == CC_VLA)
        return cc_type_check_fail(failure, "_Atomic qualifier shall not modify an array type");
    if(kind == CC_FUNCTION)
        return cc_type_check_fail(failure, "_Atomic qualifier shall not modify a function type");
    return 0;
}

static
int
cc_check_derived_type_rules(const CcTargetConfig *target, CcQualType type, const char *_Null_unspecified *_Nonnull failure){
    int err;
    for(;;){
        if(type.is_atomic){
            err = cc_check_atomic_qualifier(type, failure);
            if(err) return err;
        }
        switch(ccqt_kind(type)){
            case CC_POINTER:
            case CC_BLOCK_POINTER:
                type = ccqt_as_ptr(type)->pointee;
                break;
            case CC_SLICE:
                type = ccqt_as_slice(type)->pointee;
                if(ccqt_bt_eq(type, CCBT_void))
                    return cc_type_check_fail(failure, "slice of void is not allowed");
                if(ccqt_kind(type) == CC_FUNCTION)
                    return cc_type_check_fail(failure, "slice of functions is not allowed");
                break;
            case CC_FUNCTION: {
                CcFunction* function = ccqt_as_function(type);
                type = function->return_type;
                if(ccqt_is_vm(type))
                    return cc_type_check_fail(failure, "function return type cannot be variably modified");
                if(ccqt_is_array(type))
                    return cc_type_check_fail(failure, "function cannot return array type");
                if(ccqt_kind(type) == CC_FUNCTION)
                    return cc_type_check_fail(failure, "function cannot return function type");
                for(uint32_t i = 0; i < function->param_count; i++){
                    err = cc_check_derived_type_rules(target, function->params[i], failure);
                    if(err) return err;
                }
                break;
            }
            case CC_VLA:
            case CC_ARRAY: {
                CcQualType element = ccqt_array_element(type);
                if(ccqt_kind(type) == CC_ARRAY && ccqt_bt_eq(element, CCBT_void))
                    return cc_type_check_fail(failure, "array of void is not allowed");
                if(ccqt_kind(type) == CC_ARRAY && ccqt_kind(element) == CC_FUNCTION)
                    return cc_type_check_fail(failure, "array of functions is not allowed");
                if(cc_type_contains_fam(element))
                    return cc_type_check_fail(failure, "array element contains a flexible array member");
                if(!ccqt_is_complete_object(element))
                    return cc_type_check_fail(failure, "array requires a complete object element type");
                if(ccqt_raw_kind(element) == CC_ALIGNED && ccqt_kind(element) != CC_VLA){
                    uint32_t size;
                    if(!cc_type_sizeof_complete(target, element, &size, NULL)){
                        uint32_t alignment = ccqt_as_aligned(element)->alignment;
                        if(alignment > size)
                            return cc_type_check_fail(failure, "alignment of array elements is greater than element size");
                        if(!cc_layout_array_stride_valid(size, alignment))
                            return cc_type_check_fail(failure, "size of array element is not a multiple of its alignment");
                    }
                }
                type = element;
                break;
            }
            default:
                return 0;
        }
    }
}

static
int
cc_check_function_definition_types(const CcTargetConfig *target, const CcFunction *type, const char *_Null_unspecified *_Nonnull failure){
    uint32_t size;
    if(!ccqt_bt_eq(type->return_type, CCBT_void)
    && cc_type_sizeof_complete(target, type->return_type, &size, NULL))
        return cc_type_check_fail(failure, "function definition requires a complete return type");
    for(uint32_t i = 0; i < type->param_count; i++)
        if(cc_type_sizeof_complete(target, type->params[i], &size, NULL))
            return cc_type_check_fail(failure, "function definition requires complete parameter types");
    return 0;
}

static
int
cc_check_incdec_operand(CcQualType type, const char *_Null_unspecified *_Nonnull failure){
    if(!ccqt_is_complete_object(type))
        return cc_type_check_fail(failure, "increment/decrement requires a complete object type");
    CcQualType value = type;
    value = ccqt_enum_underlying(value);
    if(ccqt_kind(value) == CC_POINTER){
        if(!cc_type_has_size(ccqt_as_ptr(value)->pointee))
            return cc_type_check_fail(failure, "type has no size");
    }
    else if(!ccqt_is_basic(value) || !ccbt_is_arithmetic(ccqt_basic_kind(value)))
        return cc_type_check_fail(failure, "increment/decrement requires arithmetic or pointer type");
    return 0;
}

static
_Bool
cc_type_supports_atomic_rmw(CcQualType type){
    if(!type.is_atomic) return 1;
    CcQualType base = type;
    base.is_atomic = 0;
    if(!ccqt_is_basic(base) && ccqt_kind(base) == CC_ENUM)
        base = ccqt_as_enum(base)->underlying;
    return ccqt_kind(base) == CC_POINTER || (ccqt_is_basic(base)
        && (ccbt_is_integer(ccqt_basic_kind(base)) || ccbt_is_float(ccqt_basic_kind(base))));
}


static
_Bool
cc_string_array_elements_compatible(CcQualType target, CcQualType source){
    if(cc_types_compatible((CcQualType){.unqual=target.unqual}, (CcQualType){.unqual=source.unqual}))
        return 1;
    _Bool source_char = ccqt_bt_eq(source, CCBT_char)
        || ccqt_bt_eq(source, CCBT_signed_char)
        || ccqt_bt_eq(source, CCBT_unsigned_char);
    _Bool target_char = ccqt_bt_eq(target, CCBT_char)
        || ccqt_bt_eq(target, CCBT_signed_char)
        || ccqt_bt_eq(target, CCBT_unsigned_char);
    return source_char && target_char;
}

static
_Bool
cc_is_scalar_operand(CcQualType type){
    CcTypeKind kind = ccqt_kind(type);
    return kind == CC_POINTER
        || (kind == CC_ENUM && ccqt_is_complete_object(type))
        || (ccqt_is_basic(type)
                && (ccbt_is_arithmetic(ccqt_basic_kind(type))
                    || ccqt_bt_eq(type, CCBT_nullptr_t)));
}

static
int
cc_vector_element_count(const CcTargetConfig* target, CcQualType base, uint32_t size,
    uint32_t* length, const char* _Null_unspecified* _Nonnull failure){
    if(!ccqt_is_basic(base)) return cc_type_check_fail(failure, "vector_size attribute requires a scalar type");
    CcBasicTypeKind kind = ccqt_basic_kind(base);
    if(!ccbt_is_integer(kind) && !ccbt_is_float(kind))
        return cc_type_check_fail(failure, "vector_size attribute requires a real arithmetic element type");
    uint32_t element_size = target->sizeof_[kind];
    if(size < element_size) return cc_type_check_fail(failure, "vector_size is smaller than the element type");
    if(size % element_size) return cc_type_check_fail(failure, "vector_size must be a multiple of the element size");
    *length = size / element_size;
    return 0;
}

static
int
cc_check_member_type(CcQualType type, _Bool bitfield, const char* _Null_unspecified* _Nonnull failure){
    if(!ccqt_is_complete_object(type) && !ccqt_array_incomplete(type))
        return cc_type_check_fail(failure, "member requires a complete object type");
    if(bitfield && !ccqt_is_integer(type))
        return cc_type_check_fail(failure, "bitfield must have integer or enum type");
    return 0;
}

static
_Bool
cc_type_is_initializable(CcQualType type){
    return ccqt_is_complete_object(type) || ccqt_array_incomplete(type);
}

static
unsigned
cc_array_bound_information(CcQualType type){
    if(ccqt_kind(type) == CC_ARRAY)
        return ccqt_as_array(type)->is_incomplete ? 0 : 3;
    return ccqt_as_vla(type)->unspecified ? 1 : 2;
}


// FIXME: depth to prevent unbounded recursion is kind of dumb, but not all
// calls are tail recursive, idk.

static
_Bool
cc_conversion_preserves_representation(CcQualType from, CcQualType to){
    if(ccqt_unaligned(from).unqual == ccqt_unaligned(to).unqual) return 1;
    CcTypeKind kind = ccqt_kind(from);
    return kind == ccqt_kind(to)
        && (kind == CC_STRUCT || kind == CC_UNION || kind == CC_ARRAY || kind == CC_VLA || kind == CC_SLICE);
}

#undef cc_type_check_fail

#ifdef __clang__
#pragma clang assume_nonnull end
#endif
#endif
