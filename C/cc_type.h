#ifndef C_CC_TYPE_H
#define C_CC_TYPE_H
//
// Copyright © 2026-2026, David Priver <david@davidpriver.com>
//
#include <stdint.h>
#include <string.h>
#include "ci_softnum.h"
#include "srcloc.h"
#include "../Drp/atom.h"
#include "../Drp/atom_map.h"
#include "../Drp/typed_enum.h"
#include "../Drp/switch_macros.h"
#ifdef __clang__
#pragma clang assume_nonnull begin
#endif

#ifdef _MSC_VER
#define CC_ALIGN(n) __declspec(align(n))
#else
#define CC_ALIGN(n) __attribute((aligned(n)))
#endif


enum CcTypeKind TYPED_ENUM(uint32_t){
    CC_BASIC,
    CC_ENUM,
    CC_POINTER,
    CC_STRUCT,
    CC_UNION,
    CC_FUNCTION,
    CC_ARRAY,
    CC_BLOCK_POINTER,
    CC_SLICE,
    CC_ALIGNED,
    CC_VLA,
    CC_VECTOR,
};
TYPEDEF_ENUM(CcTypeKind, uint32_t);
enum CcBasicTypeKind TYPED_ENUM(uintptr_t){
    CCBT_INVALID,
    CCBT_void,
    CCBT_bool,
    CCBT_char,
    CCBT_signed_char,
    CCBT_unsigned_char,
    CCBT_short,
    CCBT_unsigned_short,
    CCBT_int,
    CCBT_unsigned,
    CCBT_long,
    CCBT_unsigned_long,
    CCBT_long_long,
    CCBT_unsigned_long_long,
    CCBT_int128,
    CCBT_unsigned_int128,
    CCBT_float16,
    CCBT_float,
    CCBT_double,
    CCBT_long_double,
    CCBT_float128,
    CCBT_bfloat16,
    CCBT_fp16,
    CCBT_float16_complex,
    CCBT_float_complex,
    CCBT_double_complex,
    CCBT_long_double_complex,
    CCBT_char_complex,
    CCBT_signed_char_complex,
    CCBT_unsigned_char_complex,
    CCBT_short_complex,
    CCBT_unsigned_short_complex,
    CCBT_int_complex,
    CCBT_unsigned_complex,
    CCBT_long_complex,
    CCBT_unsigned_long_complex,
    CCBT_long_long_complex,
    CCBT_unsigned_long_long_complex,
    CCBT_int128_complex,
    CCBT_unsigned_int128_complex,
    CCBT_nullptr_t,
    CCBT__Type,
    CCBT__Any,
    CCBT_COUNT,
};
TYPEDEF_ENUM(CcBasicTypeKind, uintptr_t);

typedef struct CcQualType CcQualType;
struct CcQualType {
    union {
        uintptr_t bits; // 0 == INVALID/no type
        struct {
            uintptr_t is_const:    1,
                      is_volatile: 1,
                      is_atomic:   1,
                      ptr: sizeof(uintptr_t)*8-3;
        };
        struct {
            uintptr_t quals: 3,
                      unqual: sizeof(uintptr_t)*8-3;
        };
        struct {
            uintptr_t _quals: 3;
            CcBasicTypeKind kind: sizeof(uintptr_t)*8-3;
        } basic;
    };
};
#define CCQT_NONE (CcQualType){0}

static inline CcQualType ccqt_unaligned(CcQualType t);
static inline
CcTypeKind
ccqt_raw_kind(CcQualType t){
    if(t.ptr < CCBT_COUNT) return CC_BASIC;
    return (CcTypeKind)(*(uint32_t*)(t.bits & ~(uintptr_t)7) & 0xf);
}
static inline void* _ccqt_to_type_ptr(CcQualType t){ return (void*)(ccqt_unaligned(t).bits & ~(uintptr_t)7); }

static inline _Bool ccqt_is_basic(CcQualType t) { return ccqt_unaligned(t).ptr < CCBT_COUNT; }

static
inline
CcTypeKind ccqt_kind(CcQualType t) {
    return ccqt_raw_kind(ccqt_unaligned(t));
}

typedef struct CcPointer CcPointer;
struct CC_ALIGN(8) CcPointer {
    union {
        uint32_t _bits;
        struct {
            CcTypeKind kind:    4; // CC_POINTER
            uint32_t restrict_: 1;
            uint32_t _padding:  27;
        };
    };
    CcQualType pointee;
};
_Static_assert(_Alignof(CcPointer)==8, "");

typedef struct CcSlice CcSlice;
struct CC_ALIGN(8) CcSlice {
    union {
        uint32_t _bits;
        struct {
            CcTypeKind kind:    4; // CC_SLICE
            uint32_t restrict_: 1;
            uint32_t _padding:  27;
        };
    };
    CcQualType pointee;
};
_Static_assert(_Alignof(CcSlice)==8, "");

typedef struct CcExpr CcExpr;

typedef struct CcArray CcArray;
struct CC_ALIGN(8) CcArray {
    union {
        uint32_t _bits;
        struct {
            CcTypeKind kind:          4; // CC_ARRAY
            uint32_t is_static:       1,
                     is_incomplete:   1,
                     parameter_quals: 3,
                     _padding:        23; // free real estate
        };
    };
    CcQualType element;
    size_t length;
    SrcLoc declarator_loc; // parsed arrays only; preserved when an outer layer becomes a VLA
};
_Static_assert(_Alignof(CcArray)==8, "");

typedef struct CcVector CcVector;
struct CC_ALIGN(8) CcVector {
    CcTypeKind kind;
    uint32_t vector_size; // in bytes
    CcQualType element;
    size_t length;
};
_Static_assert(_Alignof(CcVector)==8, "");

typedef struct CcVla CcVla;
struct CC_ALIGN(8) CcVla {
    union {
        uint32_t _bits;
        struct {
            CcTypeKind kind:   4; // CC_VLA
            uint32_t unspecified: 1,
                     parameter_quals: 3,
                     _padding: 24; // Free real estate
        };
    };
    uint32_t _pad; // free real estate
    CcQualType element;
    CcExpr *_Nonnull bound; // captured dimension, or a constant for a fixed outer layer
    SrcLoc loc;
};
_Static_assert(_Alignof(CcVla)==8, "");

enum {CC_MAX_PARAMS=0xffff};
typedef struct CcFunction CcFunction;
struct CC_ALIGN(8) CcFunction {
    union {
        uint32_t _bits;
        struct {
            CcTypeKind kind:       4;
            uint32_t is_variadic:  1,
                     no_prototype: 1,
                     _padding:     10,
                    fixed_param_count: 16; // params before the variadic boundary
        };
    };
    CcQualType return_type;
    uint32_t param_count;
    CcQualType params[];
};
_Static_assert(_Alignof(CcFunction)==8, "");

typedef struct CcFunc CcFunc;
typedef struct CcField CcField;
struct CcField {
    CcQualType type;
    union {
        Atom name;          // 0 for anonymous fields
        CcFunc * method;
    };
    uint32_t offset;
    uint32_t bitwidth:  8,
             bitoffset: 7, // bit offset within storage unit
             is_method: 1,
             is_bitfield: 1,
             packed: 1,
             _padding: 14;
    uint32_t alignment; // from _Alignas, 0 means default
    uint32_t layout_alignment; // effective member alignment after record packing
    SrcLoc loc;
};

// Should probably be in target, but target depends on this header.
enum CcSysVEightByte TYPED_ENUM(uint8_t){
    CC_SYSV_NO_CLASS,
    CC_SYSV_SSE,
    CC_SYSV_INTEGER,
    CC_SYSV_SSEUP,
    CC_SYSV_X87,
    CC_SYSV_X87UP,
    CC_SYSV_COMPLEX_X87,
    CC_SYSV_MEMORY,
};
TYPEDEF_ENUM(CcSysVEightByte, uint8_t);

typedef struct CcSysvAggregate CcSysvAggregate;
struct CcSysvAggregate {
    uint32_t classes:24; // eight 3-bit classes; use cc_sysv_class/set_class
    uint32_t eightbyte_count:4;
    uint32_t gp_count:2, fp_count:2; // SSEUP shares the preceding SSE register
    uint32_t x87_count:2; // return registers only
    // Widest SSE/SSEUP group. 32/64-byte groups describe AVX/AVX-512 PCS
    uint32_t vector_size:7;
    uint32_t is_memory_class:1; // stack argument, indirect return
    uint32_t argument_on_stack:1; // also true for X87/X87UP (direct x87 return)
    uint32_t _padding:21;
};
_Static_assert(CC_SYSV_MEMORY < 8, "SysV classes must fit in 3 bits");
_Static_assert(sizeof(CcSysvAggregate) == 8, "packed SysV aggregate metadata");

static inline
CcSysVEightByte
cc_sysv_class(const CcSysvAggregate* abi, uint32_t index){
    return (CcSysVEightByte)((abi->classes >> (3*index)) & 7);
}

static inline
void
cc_sysv_set_class(CcSysvAggregate* abi, uint32_t index, CcSysVEightByte cls){
    uint32_t shift = 3*index;
    abi->classes = (abi->classes & ~(7u << shift)) | ((uint32_t)cls << shift);
}

typedef struct CcArm64Aggregate CcArm64Aggregate;
struct CcArm64Aggregate {
    uint32_t hfa_type:6; // CcBasicTypeKind
    uint32_t hfa_count:3, hva_count:3; // mutually exclusive; 1-4 SIMD registers
    uint32_t member_size:5; // bytes in each homogeneous FP/vector member (0-16)
    uint32_t gp_count:2; // non-homogeneous direct records use 1-2 GP registers
    uint32_t copy_alignment:5; // Stage B.6: 8 or 16 bytes
    uint32_t is_indirect:1; // caller-owned copy argument; result address in x8
    uint32_t is_empty:1; // no value-bearing members; Darwin omits the argument
    uint32_t _padding0:6;
    uint32_t natural_alignment:16; // member alignment before record adjustment
    uint32_t _padding1:16;
    uint32_t member_offsets:24; // four 6-bit byte offsets (0-48); use accessors
    uint32_t _padding2:8;
};
_Static_assert(CCBT_COUNT <= 64, "HFA type must fit in 6 bits");
_Static_assert(sizeof(CcArm64Aggregate) == 12, "packed ARM64 aggregate metadata");

static inline
uint32_t
cc_arm64_member_offset(const CcArm64Aggregate* abi, uint32_t index){
    return (abi->member_offsets >> (6*index)) & 63;
}

static inline
void
cc_arm64_set_member_offset(CcArm64Aggregate* abi, uint32_t index, uint32_t offset){
    uint32_t shift = 6*index;
    abi->member_offsets = (abi->member_offsets & ~(63u << shift)) | (offset << shift);
}

typedef struct CcWindowsAggregate CcWindowsAggregate;
struct CcWindowsAggregate {
    uint8_t is_indirect:1; // C records other than sizes 1, 2, 4, 8
    uint8_t gp_count:1; // direct record: one positional GP argument / rax result
    uint8_t _padding:6;
};
_Static_assert(sizeof(CcWindowsAggregate) == 1, "packed Windows aggregate metadata");

typedef struct CcStruct CcStruct;
struct CC_ALIGN(8) CcStruct {
    union {
        uint32_t _bits;
        struct {
            CcTypeKind kind:          4; // CC_STRUCT
            uint32_t is_incomplete:   1,
                     packed:          1,
                     has_fam:         1,
                     has_wide_vector: 1,
                     is_defining:     1,
                     _padding:        23;
        };
    };
    uint32_t size;
    Atom name;
    SrcLoc loc;
    uint32_t alignment;
    uint32_t field_count;
    CcField* _Null_unspecified fields;
    struct {
        uint32_t count, capacity;
        CcField* _Null_unspecified data;
    } methods;
    union {
        CcSysvAggregate sysv;
        CcArm64Aggregate arm64;
        CcWindowsAggregate win64;
    };
};
_Static_assert(_Alignof(CcStruct)==8, "");

typedef struct CcUnion CcUnion;
struct CC_ALIGN(8) CcUnion {
    union {
        uint32_t _bits;
        struct {
            CcTypeKind kind:            4; // CC_UNION
            uint32_t is_incomplete:     1,
                     packed:            1,
                     _reserved_has_fam: 1,
                     has_wide_vector:   1,
                     is_defining:       1,
                     _padding:          23;
        };
    };
    uint32_t size;
    Atom name;
    SrcLoc loc;
    uint32_t alignment;
    uint32_t field_count;
    CcField* _Nullable fields;
    struct {
        uint32_t count, capacity;
        CcField* _Null_unspecified data;
    } methods;
    union {
        CcSysvAggregate sysv;
        CcArm64Aggregate arm64;
        CcWindowsAggregate win64;
    };
};
_Static_assert(_Alignof(CcUnion)==8, "");
_Static_assert(offsetof(CcUnion, size) == 4, "");
_Static_assert(offsetof(CcStruct, size) == 4, "");
_Static_assert(sizeof(CcUnion) == sizeof(CcStruct), "");
_Static_assert(offsetof(CcUnion, sysv) == offsetof(CcStruct, sysv), "record ABI layout");

typedef struct CcEnumerator CcEnumerator;
struct CcEnumerator {
    Atom name;
    CiUint128 value;
    CcQualType type;
    SrcLoc loc;
};

typedef struct CcEnum CcEnum;
struct CC_ALIGN(8) CcEnum {
    union {
        uint32_t _bits;
        struct {
            CcTypeKind kind:               4; // CC_ENUM
            uint32_t is_incomplete:        1,
                     has_fixed_underlying: 1,
                     has_definition:       1,
                     is_defining:          1,
                     packed:               1,
                     _padding:             23;
        };
    };
    Atom name;
    SrcLoc loc;
    CcQualType underlying;
    size_t enumerator_count;
    CcEnumerator*_Nonnull* _Nullable enumerators;
};
_Static_assert(_Alignof(CcEnum)==8, "");

typedef struct CcAligned CcAligned;
struct CC_ALIGN(8) CcAligned {
    union {
        uint32_t _bits;
        struct {
            CcTypeKind kind: 4; // CC_ALIGNED
            uint32_t _padding: 28;
        };
    };
    uint32_t alignment;
    CcQualType underlying;
};
_Static_assert(_Alignof(CcAligned)==8, "");

static inline
CcQualType
ccqt_unaligned(CcQualType t){
    while(ccqt_raw_kind(t) == CC_ALIGNED){
        CcQualType underlying = ((CcAligned*)(t.bits & ~(uintptr_t)7))->underlying;
        underlying.quals |= t.quals;
        t = underlying;
    }
    return t;
}
static inline CcBasicTypeKind ccqt_basic_kind(CcQualType t){ return ccqt_unaligned(t).basic.kind; }


static inline
_Bool
ccbt_is_integer(CcBasicTypeKind k){
    return k >= CCBT_bool && k <= CCBT_unsigned_int128;
}

static inline
_Bool
ccbt_is_float(CcBasicTypeKind k){
    return k >= CCBT_float16 && k <= CCBT_fp16;
}

static inline
_Bool
ccbt_is_complex(CcBasicTypeKind k){
    return k >= CCBT_float16_complex && k <= CCBT_unsigned_int128_complex;
}

static inline CcBasicTypeKind ccbt_complex_real(CcBasicTypeKind k){
    return k <= CCBT_long_double_complex
        ? CCBT_float16 + (k-CCBT_float16_complex)
        : CCBT_char + (k-CCBT_char_complex);
}

static inline CcBasicTypeKind ccbt_complex_kind(CcBasicTypeKind k){
    if(k >= CCBT_float16 && k <= CCBT_long_double)
        return CCBT_float16_complex + (k-CCBT_float16);
    if(k >= CCBT_char && k <= CCBT_unsigned_int128)
        return CCBT_char_complex + (k-CCBT_char);
    return CCBT_INVALID;
}

static inline
_Bool
ccbt_is_arithmetic(CcBasicTypeKind k){
    return k >= CCBT_bool && k <= CCBT_unsigned_int128_complex;
}

static inline
_Bool
ccbt_is_unsigned(CcBasicTypeKind k, _Bool char_is_unsigned){
    switch(k){
        case CCBT_char:
            return char_is_unsigned;
        case CCBT_bool:
        case CCBT_unsigned_char:
        case CCBT_unsigned_short:
        case CCBT_unsigned:
        case CCBT_unsigned_long:
        case CCBT_unsigned_long_long:
        case CCBT_unsigned_int128:
            return 1;
        case CCBT_COUNT:
        case CCBT_INVALID:
        case CCBT__Any:
        case CCBT__Type:
        case CCBT_double:
        case CCBT_double_complex:
        case CCBT_float:
        case CCBT_float128:
        case CCBT_bfloat16:
        case CCBT_fp16:
        case CCBT_float16:
        case CCBT_float_complex:
        case CCBT_int:
        case CCBT_int128:
        case CCBT_long:
        case CCBT_long_double:
        case CCBT_long_double_complex:
        case CCBT_float16_complex:
        case CCBT_char_complex:
        case CCBT_signed_char_complex:
        case CCBT_unsigned_char_complex:
        case CCBT_short_complex:
        case CCBT_unsigned_short_complex:
        case CCBT_int_complex:
        case CCBT_unsigned_complex:
        case CCBT_long_complex:
        case CCBT_unsigned_long_complex:
        case CCBT_long_long_complex:
        case CCBT_unsigned_long_long_complex:
        case CCBT_int128_complex:
        case CCBT_unsigned_int128_complex:
        case CCBT_long_long:
        case CCBT_nullptr_t:
        case CCBT_short:
        case CCBT_signed_char:
        case CCBT_void:
            return 0;
        DRP_CASES_EXHAUSTED;
    }
}

static inline
int
ccbt_int_rank(CcBasicTypeKind k){
    switch(k){
        case CCBT_bool:
            return 0;
        case CCBT_char:
        case CCBT_signed_char:
        case CCBT_unsigned_char:
            return 1;
        case CCBT_short:
        case CCBT_unsigned_short:
            return 2;
        case CCBT_int:
        case CCBT_unsigned:
            return 3;
        case CCBT_long:
        case CCBT_unsigned_long:
            return 4;
        case CCBT_long_long:
        case CCBT_unsigned_long_long:
            return 5;
        case CCBT_int128:
        case CCBT_unsigned_int128:
            return 6;
        case CCBT_COUNT:
        case CCBT_INVALID:
        case CCBT__Type:
        case CCBT__Any:
        case CCBT_double:
        case CCBT_double_complex:
        case CCBT_float:
        case CCBT_float128:
        case CCBT_bfloat16:
        case CCBT_fp16:
        case CCBT_float16:
        case CCBT_float_complex:
        case CCBT_long_double:
        case CCBT_long_double_complex:
        case CCBT_float16_complex:
        case CCBT_char_complex:
        case CCBT_signed_char_complex:
        case CCBT_unsigned_char_complex:
        case CCBT_short_complex:
        case CCBT_unsigned_short_complex:
        case CCBT_int_complex:
        case CCBT_unsigned_complex:
        case CCBT_long_complex:
        case CCBT_unsigned_long_complex:
        case CCBT_long_long_complex:
        case CCBT_unsigned_long_long_complex:
        case CCBT_int128_complex:
        case CCBT_unsigned_int128_complex:
        case CCBT_nullptr_t:
        case CCBT_void:
            return -1;
        DRP_CASES_EXHAUSTED;
    }
}

static inline
CcBasicTypeKind
ccbt_to_unsigned(CcBasicTypeKind k){
    switch(k){
        case CCBT_char:
        case CCBT_signed_char:
            return CCBT_unsigned_char;
        case CCBT_short:
            return CCBT_unsigned_short;
        case CCBT_int:
            return CCBT_unsigned;
        case CCBT_long:
            return CCBT_unsigned_long;
        case CCBT_long_long:
            return CCBT_unsigned_long_long;
        case CCBT_int128:
            return CCBT_unsigned_int128;
        case CCBT_COUNT:
        case CCBT_INVALID:
        case CCBT__Any:
        case CCBT__Type:
        case CCBT_bool:
        case CCBT_double:
        case CCBT_double_complex:
        case CCBT_float:
        case CCBT_float128:
        case CCBT_bfloat16:
        case CCBT_fp16:
        case CCBT_float16:
        case CCBT_float_complex:
        case CCBT_long_double:
        case CCBT_long_double_complex:
        case CCBT_float16_complex:
        case CCBT_char_complex:
        case CCBT_signed_char_complex:
        case CCBT_unsigned_char_complex:
        case CCBT_short_complex:
        case CCBT_unsigned_short_complex:
        case CCBT_int_complex:
        case CCBT_unsigned_complex:
        case CCBT_long_complex:
        case CCBT_unsigned_long_complex:
        case CCBT_long_long_complex:
        case CCBT_unsigned_long_long_complex:
        case CCBT_int128_complex:
        case CCBT_unsigned_int128_complex:
        case CCBT_nullptr_t:
        case CCBT_unsigned:
        case CCBT_unsigned_char:
        case CCBT_unsigned_int128:
        case CCBT_unsigned_long:
        case CCBT_unsigned_long_long:
        case CCBT_unsigned_short:
        case CCBT_void:
            return k;
        DRP_CASES_EXHAUSTED;
    }
}

static inline
CcBasicTypeKind
ccbt_to_signed(CcBasicTypeKind k){
    switch(k){
        case CCBT_char:
        case CCBT_unsigned_char:
            return CCBT_signed_char;
        case CCBT_unsigned_short:
            return CCBT_short;
        case CCBT_unsigned:
            return CCBT_int;
        case CCBT_unsigned_long:
            return CCBT_long;
        case CCBT_unsigned_long_long:
            return CCBT_long_long;
        case CCBT_unsigned_int128:
            return CCBT_int128;
        case CCBT_COUNT:
        case CCBT_INVALID:
        case CCBT__Any:
        case CCBT__Type:
        case CCBT_bool:
        case CCBT_double:
        case CCBT_double_complex:
        case CCBT_float:
        case CCBT_float128:
        case CCBT_bfloat16:
        case CCBT_fp16:
        case CCBT_float16:
        case CCBT_float_complex:
        case CCBT_int:
        case CCBT_int128:
        case CCBT_long:
        case CCBT_long_double:
        case CCBT_long_double_complex:
        case CCBT_float16_complex:
        case CCBT_char_complex:
        case CCBT_signed_char_complex:
        case CCBT_unsigned_char_complex:
        case CCBT_short_complex:
        case CCBT_unsigned_short_complex:
        case CCBT_int_complex:
        case CCBT_unsigned_complex:
        case CCBT_long_complex:
        case CCBT_unsigned_long_complex:
        case CCBT_long_long_complex:
        case CCBT_unsigned_long_long_complex:
        case CCBT_int128_complex:
        case CCBT_unsigned_int128_complex:
        case CCBT_long_long:
        case CCBT_nullptr_t:
        case CCBT_short:
        case CCBT_signed_char:
        case CCBT_void:
            return k;
        DRP_CASES_EXHAUSTED;
    }
}

static inline
CcQualType
ccqt_basic(CcBasicTypeKind k){
    return (CcQualType){.basic.kind = k};
}


static inline
_Bool
ccqt_is_some_kind_of_pointer(CcQualType t){
    CcTypeKind kind = ccqt_kind(t);
    return kind == CC_POINTER || kind == CC_BLOCK_POINTER;
}

static inline
_Bool
ccqt_is_block_ptr(CcQualType t){
    return ccqt_kind(t) == CC_BLOCK_POINTER;
}

static inline CcEnum*     ccqt_as_enum    (CcQualType t){ return _ccqt_to_type_ptr(t); }
static inline CcPointer*  ccqt_as_ptr     (CcQualType t){ return _ccqt_to_type_ptr(t); }
static inline CcSlice*    ccqt_as_slice   (CcQualType t){ return _ccqt_to_type_ptr(t); }
static inline CcStruct*   ccqt_as_struct  (CcQualType t){ return _ccqt_to_type_ptr(t); }
static inline CcUnion*    ccqt_as_union   (CcQualType t){ return _ccqt_to_type_ptr(t); }
static inline CcFunction* ccqt_as_function(CcQualType t){ return _ccqt_to_type_ptr(t); }
static inline CcArray*    ccqt_as_array   (CcQualType t){ return _ccqt_to_type_ptr(t); }
static inline CcVector* ccqt_as_vector(CcQualType t){ return _ccqt_to_type_ptr(t); }
static inline CcVla*      ccqt_as_vla     (CcQualType t){ return _ccqt_to_type_ptr(t); }
static inline _Bool ccqt_is_array(CcQualType t){
    return ccqt_kind(t) == CC_VLA || ccqt_kind(t) == CC_ARRAY;
}
// Fixed homogeneous storage shared by array and vector initializer paths.
static inline _Bool ccqt_is_fixed_sequence(CcQualType t){
    return ccqt_kind(t) == CC_ARRAY || ccqt_kind(t) == CC_VECTOR;
}
static inline size_t ccqt_sequence_length(CcQualType t){
    return ccqt_kind(t) == CC_VECTOR ? ccqt_as_vector(t)->length : ccqt_as_array(t)->length;
}
static inline _Bool ccqt_array_incomplete(CcQualType t){
    return ccqt_kind(t) == CC_ARRAY && ccqt_as_array(t)->is_incomplete;
}
static inline CcQualType ccqt_array_element(CcQualType t){
    if(ccqt_kind(t) == CC_VECTOR) return ccqt_as_vector(t)->element;
    return ccqt_kind(t) == CC_VLA ? ccqt_as_vla(t)->element : ccqt_as_array(t)->element;
}

static inline
CcQualType
ccqt_qualified_array_element(CcQualType t){
    t = ccqt_unaligned(t);
    CcQualType element = ccqt_array_element(t);
    element.quals |= t.quals;
    return element;
}

static inline
CcQualType
ccqt_array_object_type(CcQualType t){
    t = ccqt_unaligned(t);
    while(ccqt_is_array(t))
        t = ccqt_unaligned(ccqt_qualified_array_element(t));
    return t;
}
static inline
uintptr_t
cc_object_quals(CcQualType type){
    return ccqt_array_object_type(type).quals;
}
// cc_object_quals()
// -----------------
// Gets the qualifiers of the stored object, descending through array layers
// and accumulating array qualification. This is different than `.quals`
// as arrays have a qualifed element type, the array itself is not qualified.
//
// Returns:
// --------
// The object's qualifier bits, in the CcQualType.quals representation.
//
// Arguments:
// ----------
// type: Object or array type.
//
static inline _Bool ccqt_is_vm(CcQualType t){
    for(;;){
        switch(ccqt_kind(t)){
            case CC_VLA: return 1;
            case CC_ARRAY: t = ccqt_as_array(t)->element; break;
            case CC_POINTER: case CC_BLOCK_POINTER: t = ccqt_as_ptr(t)->pointee; break;
            case CC_SLICE: t = ccqt_as_slice(t)->pointee; break;
            default: return 0;
        }
    }
}
static inline CcAligned*  ccqt_as_aligned (CcQualType t){ return (void*)(t.bits & ~(uintptr_t)7); }
static inline uint32_t ccqt_explicit_alignment(CcQualType t){
    return ccqt_raw_kind(t) == CC_ALIGNED ? ccqt_as_aligned(t)->alignment : 0;
}

static inline _Bool ccqt_bt_eq(CcQualType t, CcBasicTypeKind bt){return ccqt_is_basic(t) && ccqt_basic_kind(t) == bt;}
static inline
_Bool
ccqt_has_pointer_representation(CcQualType t){
    return ccqt_kind(t) == CC_POINTER
        || ccqt_kind(t) == CC_BLOCK_POINTER
        || ccqt_bt_eq(t, CCBT_nullptr_t);
}

static inline
_Bool
ccqt_is_narrow_float(CcQualType t){
    return ccqt_bt_eq(t, CCBT_float16) || ccqt_bt_eq(t, CCBT_bfloat16) || ccqt_bt_eq(t, CCBT_fp16);
}

static inline
CcQualType
ccqt_enum_underlying(CcQualType t){
    while(ccqt_kind(t) == CC_ENUM) t = ccqt_as_enum(t)->underlying;
    return t;
}

static inline
_Bool
ccqt_is_complete_object(CcQualType type){
    for(;;){
        switch(ccqt_kind(type)){
            case CC_BASIC:
                return !ccqt_bt_eq(type, CCBT_void) && !ccqt_bt_eq(type, CCBT_INVALID);
            case CC_ARRAY:
                if(ccqt_as_array(type)->is_incomplete) return 0;
                type = ccqt_as_array(type)->element;
                break;
            case CC_VLA:
                type = ccqt_as_vla(type)->element;
                break;
            case CC_STRUCT: case CC_UNION:
                return !ccqt_as_struct(type)->is_incomplete;
            case CC_ENUM:
                return !ccqt_as_enum(type)->is_incomplete;
            case CC_POINTER: case CC_BLOCK_POINTER: case CC_SLICE: case CC_VECTOR:
                return 1;
            case CC_FUNCTION: case CC_ALIGNED:
                return 0;
            DRP_CASES_EXHAUSTED;
        }
    }
}

static inline
_Bool
ccqt_is_bool(CcQualType t){
    t = ccqt_enum_underlying(t);
    return ccqt_bt_eq(t, CCBT_bool);
}


static inline
CcFunction* _Nullable
ccqt_callable_function(CcQualType t){
    if(ccqt_kind(t) == CC_POINTER) t = ccqt_as_ptr(t)->pointee;
    return ccqt_kind(t) == CC_FUNCTION ? ccqt_as_function(t) : NULL;
}
static inline
_Bool
ccqt_is_unsigned(CcQualType t, _Bool unsigned_char){
    t = ccqt_enum_underlying(t);
    return ccqt_is_basic(t) && ccbt_is_unsigned(ccqt_basic_kind(t), unsigned_char);
}
static inline
_Bool
ccqt_is_integer(CcQualType t){
    t = ccqt_enum_underlying(t);
    return ccqt_is_basic(t) && ccbt_is_integer(ccqt_basic_kind(t));
}
#undef CC_ALIGN

#ifdef __clang__
#pragma clang assume_nonnull end
#endif
#endif
