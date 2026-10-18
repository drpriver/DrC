#ifndef C_CC_LAYOUT_H
#define C_CC_LAYOUT_H
// Shared layout computations for complete AST types and subobject paths.
#include "cc_target.h"
#include "cc_expr.h"
#include "cc_errors.h"
#include "cc_rt_types.h"
#include "../Drp/ckdint.h"
#ifdef __clang__
#pragma clang assume_nonnull begin
#endif

static inline
int
cc_layout_array_size(uint64_t element_size, size_t length, uint32_t* out){
    uint64_t size;
    if(mul_overflow(element_size, (uint64_t)length, &size) || size > UINT32_MAX)
        return _cc_overflow_error;
    *out = (uint32_t)size;
    return 0;
}

static inline
int
cc_type_sizeof_complete(const CcTargetConfig* tc, CcQualType type, uint32_t* out){
    switch(ccqt_kind(type)){
        case CC_VLA: return _cc_not_constant_error;
        case CC_ALIGNED: return _cc_unreachable_error;
        DRP_CASES_EXHAUSTED;
        case CC_BASIC:    *out = tc->sizeof_[ccqt_basic_kind(type)]; return 0;
        case CC_POINTER: case CC_BLOCK_POINTER: case CC_FUNCTION:
            *out = tc->sizeof_[CCBT_nullptr_t]; return 0;
        case CC_SLICE:    *out = 2*tc->sizeof_[CCBT_nullptr_t]; return 0;
        case CC_ENUM:
            if(ccqt_as_enum(type)->is_incomplete) return _cc_not_constant_error;
            return cc_type_sizeof_complete(tc, ccqt_as_enum(type)->underlying, out);
        case CC_STRUCT: case CC_UNION:
            if(ccqt_as_struct(type)->is_incomplete) return _cc_not_constant_error;
            *out = ccqt_as_struct(type)->size; return 0;
        case CC_VECTOR: *out = ccqt_as_vector(type)->vector_size; return 0;
        case CC_ARRAY:{
            CcArray* a = ccqt_as_array(type);
            if(a->is_incomplete) return _cc_not_constant_error;
            uint32_t element_size;
            int err = cc_type_sizeof_complete(tc, a->element, &element_size);
            if(err) return err;
            return cc_layout_array_size(element_size, a->length, out);
        }
    }
}

static inline
int
cc_field_path_resolve(const CcTargetConfig* target, CcQualType type, CcFieldPath path, uint64_t* out_offset){
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
            int err = cc_type_sizeof_complete(target, ccqt_array_element(type), &size);
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
            if(add_overflow(offset, (uint64_t)field->offset, &offset)) return _cc_overflow_error;
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
                if(add_overflow(offset, (uint64_t)offsetof(CiRtAny, payload), &offset)) return _cc_overflow_error;
                type = ccqt_basic(CCBT_void);
            }
            else type = ccqt_basic(CCBT__Type);
        }
        else return _cc_not_constant_error;
    }
    *out_offset = offset;
    return 0;
}

static inline
uint64_t
cc_expr_field_offset(const CcTargetConfig* target, const CcExpr* e){
    uint64_t offset;
    if(cc_field_path_resolve(target, cc_expr_field_owner(e), e->field_path, &offset)) return UINT64_MAX;
    return offset;
}

#ifdef __clang__
#pragma clang assume_nonnull end
#endif
#endif
