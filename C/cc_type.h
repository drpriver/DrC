#ifndef C_CC_TYPE_H
#define C_CC_TYPE_H
//
// Copyright © 2026-2026, David Priver <david@davidpriver.com>
//
#include <stdint.h>
#include "srcloc.h"
#include "../Drp/atom.h"
#include "../Drp/atom_map.h"
#include "../Drp/typed_enum.h"
#ifdef __clang__
#pragma clang assume_nonnull begin
#endif

enum CcTypeKind TYPED_ENUM(uint32_t){
    CC_BASIC,
    CC_ENUM,
    CC_POINTER,
    CC_STRUCT,
    CC_UNION,
    CC_FUNCTION,
    CC_ARRAY,
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
    CCBT_float_complex,
    CCBT_double_complex,
    CCBT_long_double_complex,
    CCBT_nullptr_t,
    CCBT__Type,
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
                      _ptr: sizeof(uintptr_t)*8-3;
        };
        struct {
            uintptr_t _quals: 3;
            CcBasicTypeKind kind: sizeof(uintptr_t)*8-3;
        } basic;
    };
};

static inline void* _ccqt_to_type_ptr(CcQualType t){ return (void*)(t.bits & ~(uintptr_t)7); }

// Basic types are small values real pointers are large.
static inline _Bool ccqt_is_basic(CcQualType t) { return t.ptr && t.ptr < CCBT_COUNT; }

static
inline
CcTypeKind ccqt_kind(CcQualType t) {
    if (ccqt_is_basic(t)) return CC_BASIC;
    return (CcTypeKind)(*(uint32_t*)_ccqt_to_type_ptr(t) & 0xf);
}

typedef struct CcPointer CcPointer;
struct CcPointer {
    _Alignas(8) union {
        uint32_t _bits;
        struct {
            CcTypeKind kind: 4;
            uint32_t restrict_: 1;
            uint32_t _padding: 27;
        };
    };
    CcQualType pointee;
};

typedef struct CcExpr CcExpr;

typedef struct CcArray CcArray;
struct CcArray {
    _Alignas(8) union {
        uint32_t _bits;
        struct {
            CcTypeKind kind:        4;
            uint32_t is_static:     1;
            uint32_t is_vla:        1;
            uint32_t is_incomplete: 1;
            uint32_t is_vector:     1;
            uint32_t _padding:      24;
        };
    };
    uint32_t vector_size; // in bytes
    CcQualType element;
    union {
        size_t length;
        CcExpr* _Nullable vla_expr;
    };
};

typedef struct CcFunction CcFunction;
struct CcFunction {
    _Alignas(8) union {
        uint32_t _bits;
        struct {
            CcTypeKind kind:       4;
            uint32_t is_variadic:  1;
            uint32_t no_prototype: 1;
            uint32_t _padding:     26;
        };
    };
    CcQualType return_type;
    uint32_t param_count;
    CcQualType params[];
};

typedef struct CcFunc CcFunc;
typedef struct CcField CcField;
struct CcField {
    CcQualType type;
    union {
        Atom name;          // 0 for anonymous fields
        CcFunc * method;
    };
    uint32_t offset;
    uint32_t bitwidth:  7,
             bitoffset: 6, // bit offset within storage unit
             is_method: 1,
             is_bitfield: 1,
             alignment: 17; // from _Alignas, 0 means default
    SrcLoc loc;
};

// Should probably be in target, but target depends on this header.
enum CcSysVEightByte TYPED_ENUM(uint32_t){
    CC_SYSV_SSE,
    CC_SYSV_INTEGER,
};
TYPEDEF_ENUM(CcSysVEightByte, uint32_t);

typedef struct CcStruct CcStruct;
struct CcStruct {
    _Alignas(8) union {
        uint32_t _bits;
        struct {
            CcTypeKind kind:        4;
            uint32_t is_incomplete: 1,
                     packed:        1,
                     has_fam:       1,
                     _padding:     25;
        };
        struct {
            CcTypeKind kind:        4;
            uint32_t is_incomplete: 1,
                     packed:        1,
                     has_fam:       1,
                     _padding:     22,
                   is_memory_class: 1;
            CcSysVEightByte class0: 1,
                            class1: 1;
        } sysv;
        struct {
            CcTypeKind kind:        4;
            uint32_t is_incomplete: 1,
                     packed:        1,
                     has_fam:       1,
                     _padding:     17,
                     hfa_type:      5,
                     hfa_count:     3; // 0-4, 0 means not hfa
            _Static_assert(CCBT_COUNT <= (1<<5)-1, "");
        } arm64;
    };
    uint32_t size;
    Atom name;
    SrcLoc loc;
    uint32_t alignment;
    uint32_t field_count;
    CcField* _Null_unspecified fields;
    void*_Null_unspecified ffi_cache; // opaque, managed by native_call.c
};

typedef struct CcUnion CcUnion;
struct CcUnion {
    _Alignas(8) union {
        uint32_t _bits;
        struct {
            CcTypeKind kind:        4;
            uint32_t is_incomplete: 1,
                     packed:        1,
                     _padding:     26;
        };
        struct {
            CcTypeKind kind:        4;
            uint32_t is_incomplete: 1,
                     packed:        1,
                     _padding:     23,
                   is_memory_class: 1;
            CcSysVEightByte class0: 1,
                            class1: 1;
        } sysv;
        struct {
            CcTypeKind kind:        4;
            uint32_t is_incomplete: 1,
                     packed:        1,
                     _padding:     18,
                     hfa_type:      5,
                     hfa_count:     3; // 0-4, 0 means not hfa
            _Static_assert(CCBT_COUNT <= (1<<5)-1, "");
        } arm64;
    };
    uint32_t size;
    Atom name;
    SrcLoc loc;
    uint32_t alignment;
    uint32_t field_count;
    CcField* _Nullable fields;
    void*_Null_unspecified ffi_cache; // opaque, managed by native_call.c
};
_Static_assert(offsetof(CcUnion, size) == 4, "");
_Static_assert(offsetof(CcStruct, size) == 4, "");
_Static_assert(sizeof(CcUnion) == sizeof(CcStruct), "");

typedef struct CcEnumerator CcEnumerator;
struct CcEnumerator {
    Atom name;
    int64_t value;
    CcQualType type;
    SrcLoc loc;
};

typedef struct CcEnum CcEnum;
struct CcEnum {
    _Alignas(8) union {
        uint32_t _bits;
        struct {
            CcTypeKind kind:        4;
            uint32_t is_incomplete: 1;
            uint32_t _padding:      27;
        };
    };
    Atom name;
    SrcLoc loc;
    CcQualType underlying;
    size_t enumerator_count;
    CcEnumerator*_Nonnull* _Nullable enumerators;
};


static inline
_Bool
ccbt_is_integer(CcBasicTypeKind k){
    return k >= CCBT_bool && k <= CCBT_unsigned_int128;
}

static inline
_Bool
ccbt_is_float(CcBasicTypeKind k){
    return k >= CCBT_float16 && k <= CCBT_float128;
}

static inline
_Bool
ccbt_is_arithmetic(CcBasicTypeKind k){
    // bool through long_double_complex, excludes void, nullptr_t, INVALID
    return k >= CCBT_bool && k <= CCBT_long_double_complex;
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
        default:
            return 0;
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
        default:
            return -1;
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
        default:
            return k;
    }
}

static inline
CcQualType
ccqt_basic(CcBasicTypeKind k){
    return (CcQualType){.basic.kind = k};
}

// Check if a type is pointer-like (pointer or array) for arithmetic purposes
static inline
_Bool
ccqt_is_pointer_like(CcQualType t){
    CcTypeKind k = ccqt_kind(t);
    return k == CC_POINTER || (k == CC_ARRAY && !((CcArray*)_ccqt_to_type_ptr(t))->is_vector);
}

static inline CcEnum*     ccqt_as_enum    (CcQualType t){ return _ccqt_to_type_ptr(t); }
static inline CcPointer*  ccqt_as_ptr     (CcQualType t){ return _ccqt_to_type_ptr(t); }
static inline CcStruct*   ccqt_as_struct  (CcQualType t){ return _ccqt_to_type_ptr(t); }
static inline CcUnion*    ccqt_as_union   (CcQualType t){ return _ccqt_to_type_ptr(t); }
static inline CcFunction* ccqt_as_function(CcQualType t){ return _ccqt_to_type_ptr(t); }
static inline CcArray*    ccqt_as_array   (CcQualType t){ return _ccqt_to_type_ptr(t); }

static inline _Bool ccqt_bt_eq(CcQualType t, CcBasicTypeKind bt){return ccqt_is_basic(t) && t.basic.kind == bt;}
static inline
_Bool
ccqt_is_unsigned(CcQualType t, _Bool unsigned_char){
    while(ccqt_kind(t) == CC_ENUM)
        t = ccqt_as_enum(t)->underlying;
    return ccqt_is_basic(t) && ccbt_is_unsigned(t.basic.kind, unsigned_char);
}

#ifdef __clang__
#pragma clang assume_nonnull end
#endif
#endif
