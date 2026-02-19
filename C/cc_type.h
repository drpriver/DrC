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
    CC_VECTOR,
};
TYPEDEF_ENUM(CcTypeKind, uint32_t);
enum CcBasicTypeKind TYPED_ENUM(uintptr_t){
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
    CCBT_float,
    CCBT_double,
    CCBT_long_double,
    CCBT_float_complex,
    CCBT_double_complex,
    CCBT_long_double_complex,
    CCBT_nullptr_t,
    CCBT_COUNT,
};
TYPEDEF_ENUM(CcBasicTypeKind, uintptr_t);

typedef struct CcQualType CcQualType;
struct CcQualType {
    union {
        uintptr_t bits;
        struct {
            uintptr_t is_const:    1,
                      is_volatile: 1,
                      is_atomic:   1,
                      ptr: sizeof(uintptr_t)*8-3;
        };
        struct {
            uintptr_t _quals: 3;
            CcBasicTypeKind kind: sizeof(uintptr_t)*8-3;
        } basic;
    };
};

// Basic types are small values real pointers are large.
static inline _Bool ccqt_is_basic(CcQualType t) { return t.ptr < CCBT_COUNT; }

static
inline
CcTypeKind ccqt_kind(CcQualType t) {
    if (ccqt_is_basic(t)) return CC_BASIC;
    return (CcTypeKind)(*(uint32_t*)(t.bits & ~(uintptr_t)7) & 0xf);
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
            uint32_t _padding:      25;
        };
    };
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

typedef struct CcField CcField;
struct CcField {
    CcQualType type;
    Atom name;          // 0 for anonymous fields
    uint32_t offset;
    uint32_t bitwidth:  7, // 0 = not a bitfield, max 64
             bitoffset: 6, // bit offset within storage unit
             _padding: 19;
};

typedef struct CcStruct CcStruct;
struct CcStruct {
    _Alignas(8) union {
        uint32_t _bits;
        struct {
            CcTypeKind kind:        4;
            uint32_t is_incomplete: 1;
            uint32_t packed:        1;
            uint32_t _padding:      26;
        };
    };
    Atom name;
    SrcLoc loc;
    uint32_t size;
    uint32_t alignment;
    uint32_t field_count;
    CcField* _Nullable fields;
    void*_Null_unspecified ffi_cache; // opaque, managed by native_call.c
};

typedef struct CcUnion CcUnion;
struct CcUnion {
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
    uint32_t size;
    uint32_t alignment;
    uint32_t field_count;
    CcField* _Nullable fields;
    void*_Null_unspecified ffi_cache; // opaque, managed by native_call.c
};

typedef struct CcEnumerator CcEnumerator;
struct CcEnumerator {
    Atom name;
    int64_t value;
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
    CcBasicTypeKind underlying;
    uint32_t enumerator_count;
    CcEnumerator* _Nullable enumerators;
};

typedef struct CcVector CcVector;
struct CcVector {
    _Alignas(8) union {
        uint32_t _bits;
        struct {
            CcTypeKind kind:  4;
            uint32_t _padding: 28;
        };
    };
    CcQualType element;
    uint32_t vector_size; // total size in bytes (attribute value)
};

#ifdef __clang__
#pragma clang assume_nonnull end
#endif
#endif
