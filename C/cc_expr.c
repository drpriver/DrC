#ifndef C_CC_EXPR_C
#define C_CC_EXPR_C
//
// Copyright © 2026-2026, David Priver <david@davidpriver.com>
//
#include "cc_expr.h"
#include "cc_type_check.h"
#include "cc_rt_types.h"
#ifdef __clang__
#pragma clang assume_nonnull begin
#endif

static
int
cc_field_path_offset(const CcTargetConfig* target, CcQualType type, CcFieldPath path, uint64_t* out_offset){
    uint64_t offset = 0;
    _Bool bitfield = 0;
    for(uint32_t i = 0, count = cc_field_path_count(path); i < count; i++){
        uint32_t index = cc_field_path_component(path, i);
        CcTypeKind kind = ccqt_kind(type);
        if(bitfield) return _cc_not_constant_error;
        if(kind == CC_ARRAY || kind == CC_VECTOR){
            if(!ccqt_array_incomplete(type) && index >= ccqt_sequence_length(type))
                return _cc_not_constant_error;
            uint32_t size;
            int err = cc_type_sizeof_complete(target, ccqt_array_element(type), &size, NULL);
            if(err) return err;
            uint64_t delta;
            if(mul_overflow((uint64_t)index, (uint64_t)size, &delta)
                || add_overflow(offset, delta, &offset)) return _cc_overflow_error;
            type = ccqt_array_element(type);
        }
        else if(kind == CC_STRUCT || kind == CC_UNION){
            CcStruct* aggregate = ccqt_as_struct(type);
            CcField* fields = aggregate->fields;
            uint32_t field_count = aggregate->field_count;
            if(index >= field_count)
                return _cc_not_constant_error;
            CcField* field = &fields[index];
            if(add_overflow(offset, (uint64_t)field->offset, &offset))
                return _cc_overflow_error;
            bitfield = field->is_bitfield;
            type = field->type;
        }
        else if(kind == CC_SLICE && index < 2){
            if(index){
                if(add_overflow(offset, (uint64_t)target->sizeof_[CCBT_nullptr_t], &offset))
                    return _cc_overflow_error;
                type = CCQT_NONE; // A data pointer is a terminal pseudo-member.
            }
            else type = ccqt_basic(target->size_type);
        }
        else if(ccqt_bt_eq(type, CCBT__Any) && index < 2){
            if(index){
                if(add_overflow(offset, (uint64_t)offsetof(CiRtAny, payload), &offset)) 
                    return _cc_overflow_error;
                type = ccqt_basic(CCBT_void);
            }
            else type = ccqt_basic(CCBT__Type);
        }
        else return _cc_not_constant_error;
    }
    *out_offset = offset;
    return 0;
}

static
int
cc_expr_field_offset(const CcTargetConfig* target, const CcExpr* e, uint64_t* out_offset){
    return cc_field_path_offset(target, cc_expr_field_owner(e), e->field_path, out_offset);
}

static
_Bool
cc_field_paths_overlap(CcQualType type, CcFieldPath a, CcFieldPath b){
    uint32_t ac = cc_field_path_count(a), bc = cc_field_path_count(b);
    for(uint32_t i = 0; i < ac && i < bc; i++){
        uint32_t ai = cc_field_path_component(a, i), bi = cc_field_path_component(b, i);
        CcTypeKind kind = ccqt_kind(type);
        if(ai != bi) return kind == CC_UNION;
        if(kind == CC_ARRAY || kind == CC_VECTOR) type = ccqt_array_element(type);
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
    for(uint32_t i = 0; i < count && !extended; i++)
        extended = components[i] > 1023;
    if(extended){
        CcFieldPathEx* ex = Allocator_alloc_extra(al, count, sizeof *ex->components, sizeof *ex, _Alignof(CcFieldPathEx));
        if(!ex)
            return _cc_oom_error;
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

static
void
cc_field_path_free(Allocator al, CcFieldPath path){
    if(path.is_extended){
        CcFieldPathEx* ex = cc_field_path_extended(path);
        Allocator_free_extra(al, ex, (size_t)ex->n_components, sizeof *ex->components, sizeof *ex, _Alignof(CcFieldPathEx));
    }
}

static
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
    CcFieldPathEx* ex = Allocator_alloc_extra(al, count, sizeof *ex->components, sizeof *ex, _Alignof(CcFieldPathEx));
    if(!ex) return 1;
    ex->n_components = count;
    for(uint32_t i = 0; i < a; i++) ex->components[i] = cc_field_path_component(prefix, i);
    for(uint32_t i = 0; i < b; i++) ex->components[a+i] = cc_field_path_component(suffix, i);
    *out = (CcFieldPath){.is_extended=1, .ptr=(uintptr_t)ex >> 1};
    return 0;
}

static
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

static
int
cc_field_path_take(Allocator al, CcFieldPath path, uint32_t count, CcFieldPath* out){
    if(count > cc_field_path_count(path)) return 1;
    if(path.is_extended) return cc_field_path_make(al, cc_field_path_extended(path)->components, count, out);
    uint32_t small[6];
    for(uint32_t i = 0; i < count; i++) small[i] = cc_field_path_component(path, i);
    return cc_field_path_make(al, small, count, out);
}

static
CcField*_Nullable
cc_field_path_field(CcQualType type, CcFieldPath path){
    CcField* field = NULL;
    for(uint32_t i = 0, count = cc_field_path_count(path); i < count; i++){
        uint32_t index = cc_field_path_component(path, i);
        switch(ccqt_kind(type)){
            case CC_STRUCT: field = &ccqt_as_struct(type)->fields[index]; break;
            case CC_UNION: field = &ccqt_as_union(type)->fields[index]; break;
            case CC_VECTOR:
            case CC_ARRAY:
                field = NULL;
                type = ccqt_array_element(type);
                continue;
            default: return NULL; // Slice and _Any pseudo-members aren't bitfields.
        }
        type = field->type;
    }
    return field;
}

static inline
const char*
cc_expr_kind_to_operator(CcExprKind kind){
    switch(kind){
        DRP_CASES_EXHAUSTED;
    case CC_EXPR_NEG:          return "-";
    case CC_EXPR_POS:          return "+";
    case CC_EXPR_BITNOT:       return "~";
    case CC_EXPR_LOGNOT:       return "!";
    case CC_EXPR_DEREF:        return "*";
    case CC_EXPR_ADDR:         return "&";
    case CC_EXPR_PREINC:       return "++";
    case CC_EXPR_PREDEC:       return "--";
    case CC_EXPR_POSTINC:      return "++";
    case CC_EXPR_POSTDEC:      return "--";
    case CC_EXPR_ADD:          return "+";
    case CC_EXPR_SUB:          return "-";
    case CC_EXPR_MUL:          return "*";
    case CC_EXPR_DIV:          return "/";
    case CC_EXPR_MOD:          return "%";
    case CC_EXPR_BITAND:       return "&";
    case CC_EXPR_BITOR:        return "|";
    case CC_EXPR_BITXOR:       return "^";
    case CC_EXPR_LSHIFT:       return "<<";
    case CC_EXPR_RSHIFT:       return ">>";
    case CC_EXPR_LOGAND:       return "&&";
    case CC_EXPR_LOGOR:        return "||";
    case CC_EXPR_EQ:           return "==";
    case CC_EXPR_NE:           return "!=";
    case CC_EXPR_LT:           return "<";
    case CC_EXPR_GT:           return ">";
    case CC_EXPR_LE:           return "<=";
    case CC_EXPR_GE:           return ">=";
    case CC_EXPR_ASSIGN:       return "=";
    case CC_EXPR_ADDASSIGN:    return "+=";
    case CC_EXPR_SUBASSIGN:    return "-=";
    case CC_EXPR_MULASSIGN:    return "*=";
    case CC_EXPR_DIVASSIGN:    return "/=";
    case CC_EXPR_MODASSIGN:    return "%=";
    case CC_EXPR_BITANDASSIGN: return "&=";
    case CC_EXPR_BITORASSIGN:  return "|=";
    case CC_EXPR_BITXORASSIGN: return "^=";
    case CC_EXPR_LSHIFTASSIGN: return "<<=";
    case CC_EXPR_RSHIFTASSIGN: return ">>=";
    case CC_EXPR_TERNARY:      return "?:";
    case CC_EXPR_CALL:         return "()";
    case CC_EXPR_SUBSCRIPT:    return "[]";
    case CC_EXPR_DOT:          return ".";
    case CC_EXPR_ARROW:        return "=>";
    case CC_EXPR_COMMA:        return ",";

    case CC_EXPR_VALUE:
    case CC_EXPR_BLOB:
    case CC_EXPR_SIZEOF_VMT:
    case CC_EXPR_VARIABLE:
    case CC_EXPR_FUNCTION:
    case CC_EXPR_COMPOUND_LITERAL:
    case CC_EXPR_INIT_LIST:
    case CC_EXPR_OBJECT_VIEW:
    case CC_EXPR_CAST:
    case CC_EXPR_STATEMENT_EXPRESSION:
    case CC_EXPR_ATOMIC:
    case CC_EXPR_VA:
    case CC_EXPR_BUILTIN:
    case CC_EXPR_ADD_OVERFLOW:
    case CC_EXPR_MUL_OVERFLOW:
    case CC_EXPR_SUB_OVERFLOW:
    case CC_EXPR_BIT_BUILTIN:
    case CC_EXPR_BSWAP:
    case CC_EXPR_ALLOCA:
    case CC_EXPR_INTERN:
    case CC_EXPR_HOTSWAP:
    case CC_EXPR_SRCLOC_REFLECT:
    case CC_EXPR_COMPILE:
    case CC_EXPR_MODULE_REFLECT:
    case CC_EXPR_TYPE_INTROSPECTION:
    case CC_EXPR_UMUL128:
    case CC_EXPR_SLICE_ALL:
    case CC_EXPR_SLICE:
    case CC_EXPR_SLICE_LO:
    case CC_EXPR_SLICE_HI:
        return "?";
    }
}

#ifdef __clang__
#pragma clang assume_nonnull end
#endif
#endif
