#ifndef DRP_TYPEINFO_H
#define DRP_TYPEINFO_H
#include <stdint.h>
#include <stddef.h>
#include "atom.h"
#include "atom_table.h"
#include "stringview.h"
#include "atom_set.h"
#include "typed_enum.h"
#ifdef __clang__
#pragma clang assume_nonnull begin
#else
#ifndef _Nonnull
#define _Nonnull
#endif
#ifndef _Null_unspecified
#define _Null_unspecified
#endif
#endif
enum TypeInfoKind TYPED_ENUM(uint32_t) {
    TIK_UNSET = 0,
    TIK_INT8,
    TIK_INT16,
    TIK_INT32,
    TIK_INT64,
    #if INTPTR_MAX == INT64_MAX
        TIK_INTPTR = TIK_INT64,
        TIK_PTRDIFF_T = TIK_INT64,
    #else
        TIK_INTPTR = TIK_INT32,
        TIK_PTRDIFF_T = TIK_INT32,
    #endif
    TIK_UINT8 = TIK_INT64+1,
    TIK_UINT16,
    TIK_UINT32,
    TIK_UINT64,
    #if UINTPTR_MAX == UINT64_MAX
        TIK_UINTPTR = TIK_UINT64,
        TIK_SIZE_T = TIK_UINT64,
    #else
        TIK_UINTPTR = TIK_UINT32,
        TIK_SIZE_T = TIK_UINT32,
    #endif
    TIK_FLOAT32 = TIK_UINT64 + 1,
    TIK_FLOAT64,
    TIK_BOOL,
    TIK_ATOM,
    TIK_SV,
    TIK_ATOM_SET,
    TIK_STRUCT,
    TIK_TUPLE,
    TIK_ENUM,
    TIK_ATOM_ENUM,
    TIK_MARRAY,
    TIK_FARRAY,
    TIK_ATOM_MAP,
    TIK_DRJSON_VALUE,
    TIK_POINTER,
    TIK_CLASS,
};


TYPEDEF_ENUM(TypeInfoKind, uint32_t);
typedef struct TypeInfo TypeInfo;
struct TypeInfo {
#define TYPEINFO \
    Atom name; \
    size_t size: 18; \
    size_t align:10; \
    TypeInfoKind kind: 6 \

    TYPEINFO;
    size_t _pad: 30;
};

enum MemberKind {
    MK_NORMAL = 0,
    MK_BITFIELD = 1,
    MK_ARRAY = 2,
};
typedef enum MemberKind MemberKind;

typedef struct MemberInfo MemberInfo;
struct MemberInfo {
    Atom name;
    const TypeInfo* type;
    union {
        struct {
            size_t offset: 15;
            size_t kind: 2;
            size_t noser: 1;
            size_t nodeser: 1;
            size_t noprint: 1;
            #if SIZE_MAX == UINT64_MAX
            size_t _pad: 44;
            #else
            size_t _pad: 12;
            #endif
        };
        struct {
            size_t offset: 15;
            size_t kind: 2;
            size_t noser: 1;
            size_t nodeser: 1;
            size_t noprint: 1;
            #if SIZE_MAX == UINT64_MAX
            size_t length: 44;
            #else
            size_t length: 12;
            #endif
        } array;
        struct {
            size_t offset: 15;
            size_t kind: 2;
            size_t noser: 1;
            size_t nodeser: 1;
            size_t noprint: 1;
            size_t bitsize: 6;
            size_t bitoffset: 6;
            #if SIZE_MAX == UINT64_MAX
            size_t _pad: 32;
            #else
            #endif
        } bitfield;
    };
};
_Static_assert(sizeof(MemberInfo) == sizeof(size_t)*3, "");

typedef struct TypeInfoStruct TypeInfoStruct;
struct TypeInfoStruct {
#define STRUCTINFO \
    TYPEINFO; \
    size_t length: 29; \
    /* Whether the struct doesn't need to handle new or deleted fields */ \
    /* when deserializing. */ \
    size_t is_closed: 1 \

    union { TypeInfo type_info; struct { STRUCTINFO; }; };
    // There is no portable way to static initialize flexible array members, so
    // we'll have to engage in type punning.
    // Probably UB, whatever.
    MemberInfo members[];
};
_Static_assert(offsetof(TypeInfoStruct, members) == sizeof(TypeInfo), "");

typedef struct TypeInfoPointer TypeInfoPointer;
struct TypeInfoPointer {
#define POINTERINFO \
    TYPEINFO; \
    size_t _pad: 30

    union { TypeInfo type_info; struct { POINTERINFO; }; };
    const TypeInfo* type;
};
_Static_assert(offsetof(TypeInfoPointer, type) == sizeof(TypeInfo), "");

typedef struct TypeInfoEnum TypeInfoEnum;
struct TypeInfoEnum {
#define ENUMINFO \
    TYPEINFO; \
    size_t named: 1; \
    size_t length: 29

    union { TypeInfo type_info; struct { ENUMINFO; }; };
    Atom _Nonnull names[];
};
_Static_assert(offsetof(TypeInfoEnum, names) == sizeof(TypeInfo), "");

#define ENUM_TYPE_INFO_DECL(enum_name, enum_max) static const struct enum_name##Info { union { TypeInfo type_info; struct { ENUMINFO; }; }; Atom _Nonnull names[enum_max]; } TI_##enum_name

typedef struct TypeInfoAtomEnum TypeInfoAtomEnum;
struct TypeInfoAtomEnum {
#define ATOMENUMINFO \
    TYPEINFO; \
    size_t length: 30

    union { TypeInfo type_info; struct { ATOMENUMINFO; }; };
    Atom _Nonnull names[];
};
_Static_assert(offsetof(TypeInfoAtomEnum, names) == sizeof(TypeInfo), "");

typedef struct TypeInfoMarray TypeInfoMarray;
struct TypeInfoMarray {
#define MARRAYINFO \
    TYPEINFO; \
    size_t _pad: 30 \

    union { TypeInfo type_info; struct { MARRAYINFO; }; };
    const TypeInfo* type;
};
_Static_assert(offsetof(TypeInfoMarray, type) == sizeof(TypeInfo), "");

typedef struct TypeInfoFixedArray TypeInfoFixedArray;
struct TypeInfoFixedArray {
#define FIXEDARRAYINFO \
    TYPEINFO; \
    size_t length: 22; \
    size_t data_offset: 8 \

    union { TypeInfo type_info; struct { FIXEDARRAYINFO; }; };
    const TypeInfo* type;
};
_Static_assert(offsetof(TypeInfoFixedArray, type) == sizeof(TypeInfo), "");

typedef struct TypeInfoAtomMap TypeInfoAtomMap;
struct TypeInfoAtomMap {
#define ATOMMAPINFO \
    TYPEINFO; \
    size_t _pad: 30 \

    union { TypeInfo type_info; struct { ATOMMAPINFO; }; };
    const TypeInfo* type;
};
_Static_assert(offsetof(TypeInfoAtomMap, type) == sizeof(TypeInfo), "");

typedef struct TypeInfoClass TypeInfoClass;
struct TypeInfoClass {
#define CLASSINFO \
    TYPEINFO; \
    size_t atom_tag: 1; \
    size_t vtable_func: 1; \
    size_t _pad: 28 \

    union {TypeInfo type_info; struct {CLASSINFO;}; };
    // Either a pointer to `AtomMap(TypeInfo*)` or an array of `TypeInfo`s
    union {
        const TypeInfo* _Nullable (* _Nonnull func)(Atom);
        const void* _vtable;
    };
};
_Static_assert(offsetof(TypeInfoClass, _vtable) == sizeof(TypeInfo), "");


#ifndef TYPEINFO_ATOMS_HANDLED
#define TYPEINFO_ATOMS_HANDLED
#ifndef __wasm__
    _Alignas(Atom_) static const char ATOM_AT_uint8_[] = "\x05\x00\x00\x00\x3e\xfa\x25\xa8""uint8";
    #define ATOM_AT_uint8 ((Atom)ATOM_AT_uint8_)
    _Alignas(Atom_) static const char ATOM_AT_int8_[] = "\x04\x00\x00\x00\x08\x95\xaa\xd3""int8";
    #define ATOM_AT_int8 ((Atom)ATOM_AT_int8_)
    _Alignas(Atom_) static const char ATOM_AT_uint16_[] = "\x06\x00\x00\x00\xfa\x26\xc7\x04""uint16";
    #define ATOM_AT_uint16 ((Atom)ATOM_AT_uint16_)
    _Alignas(Atom_) static const char ATOM_AT_int16_[] = "\x05\x00\x00\x00\xcc\x07\xfe\x12""int16";
    #define ATOM_AT_int16 ((Atom)ATOM_AT_int16_)
    _Alignas(Atom_) static const char ATOM_AT_uint32_[] = "\x06\x00\x00\x00\x0b\x81\x18\xe4""uint32";
    #define ATOM_AT_uint32 ((Atom)ATOM_AT_uint32_)
    _Alignas(Atom_) static const char ATOM_AT_int32_[] = "\x05\x00\x00\x00\x3d\xa0\x21\xf2""int32";
    #define ATOM_AT_int32 ((Atom)ATOM_AT_int32_)
    _Alignas(Atom_) static const char ATOM_AT_uint64_[] = "\x06\x00\x00\x00\x48\x9f\x91\x9f""uint64";
    #define ATOM_AT_uint64 ((Atom)ATOM_AT_uint64_)
    _Alignas(Atom_) static const char ATOM_AT_int64_[] = "\x05\x00\x00\x00\x7e\xbe\xa8\x89""int64";
    #define ATOM_AT_int64 ((Atom)ATOM_AT_int64_)
    _Alignas(Atom_) static const char ATOM_AT_float32_[] = "\x07\x00\x00\x00\x71\x60\x0b\xe6""float32";
    #define ATOM_AT_float32 ((Atom)ATOM_AT_float32_)
    _Alignas(Atom_) static const char ATOM_AT_float64_[] = "\x07\x00\x00\x00\x32\x7e\x82\x9d""float64";
    #define ATOM_AT_float64 ((Atom)ATOM_AT_float64_)
    _Alignas(Atom_) static const char ATOM_AT_bool_[] = "\x04\x00\x00\x00\xc5\x3a\x04\xe2""bool";
    #define ATOM_AT_bool ((Atom)ATOM_AT_bool_)
    _Alignas(Atom_) static const char ATOM_AT_atom_[] = "\x04\x00\x00\x00\xc3\xb3\x90\xd0""atom";
    #define ATOM_AT_atom ((Atom)ATOM_AT_atom_)
    _Alignas(Atom_) static const char ATOM_AT_StringView_[] = "\x0a\x00\x00\x00\xa7\x0a\x21\x5b""StringView";
    #define ATOM_AT_StringView ((Atom)ATOM_AT_StringView_)
    _Alignas(Atom_) static const char ATOM_AT_AtomSet_[] = "\x07\x00\x00\x00\xef\x2a\xaa\xb0""AtomSet";
    #define ATOM_AT_AtomSet ((Atom)ATOM_AT_AtomSet_)
#else
     _Alignas(Atom_) static const char ATOM_AT_uint8_[] = "\x05\x00\x00\x00\x9e\xe2\x71\xe6""uint8";
     #define ATOM_AT_uint8 ((Atom)ATOM_AT_uint8_)
     _Alignas(Atom_) static const char ATOM_AT_int8_[] = "\x04\x00\x00\x00\x63\x7f\x3c\x73""int8";
     #define ATOM_AT_int8 ((Atom)ATOM_AT_int8_)
     _Alignas(Atom_) static const char ATOM_AT_uint16_[] = "\x06\x00\x00\x00\xc5\x8d\xeb\xd6""uint16";
     #define ATOM_AT_uint16 ((Atom)ATOM_AT_uint16_)
     _Alignas(Atom_) static const char ATOM_AT_int16_[] = "\x05\x00\x00\x00\x28\xef\x03\xa6""int16";
     #define ATOM_AT_int16 ((Atom)ATOM_AT_int16_)
     _Alignas(Atom_) static const char ATOM_AT_uint32_[] = "\x06\x00\x00\x00\xdd\x86\x4c\xc4""uint32";
     #define ATOM_AT_uint32 ((Atom)ATOM_AT_uint32_)
     _Alignas(Atom_) static const char ATOM_AT_int32_[] = "\x05\x00\x00\x00\x2f\xe9\x63\x6e""int32";
     #define ATOM_AT_int32 ((Atom)ATOM_AT_int32_)
     _Alignas(Atom_) static const char ATOM_AT_uint64_[] = "\x06\x00\x00\x00\x88\x67\x99\xec""uint64";
     #define ATOM_AT_uint64 ((Atom)ATOM_AT_uint64_)
     _Alignas(Atom_) static const char ATOM_AT_int64_[] = "\x05\x00\x00\x00\x1b\xca\xfb\xe6""int64";
     #define ATOM_AT_int64 ((Atom)ATOM_AT_int64_)
     _Alignas(Atom_) static const char ATOM_AT_float32_[] = "\x07\x00\x00\x00\x5b\x30\x55\xb9""float32";
     #define ATOM_AT_float32 ((Atom)ATOM_AT_float32_)
     _Alignas(Atom_) static const char ATOM_AT_float64_[] = "\x07\x00\x00\x00\x63\x52\xa3\x17""float64";
     #define ATOM_AT_float64 ((Atom)ATOM_AT_float64_)
     _Alignas(Atom_) static const char ATOM_AT_bool_[] = "\x04\x00\x00\x00\xbb\xf4\x38\xdb""bool";
     #define ATOM_AT_bool ((Atom)ATOM_AT_bool_)
     _Alignas(Atom_) static const char ATOM_AT_atom_[] = "\x04\x00\x00\x00\xef\x7c\xdd\x4e""atom";
     #define ATOM_AT_atom ((Atom)ATOM_AT_atom_)
    _Alignas(Atom_) static const char ATOM_AT_StringView_[] = "\x0a\x00\x00\x00\x15\xc7\xf7\x02""StringView";
    #define ATOM_AT_StringView ((Atom)ATOM_AT_StringView_)
    _Alignas(Atom_) static const char ATOM_AT_AtomSet_[] = "\x07\x00\x00\x00\xa9\x46\xa2\xa7""AtomSet";
    #define ATOM_AT_AtomSet ((Atom)ATOM_AT_AtomSet_)
#endif

static const Atom _Nonnull TYPE_ATOMS[] = {
    ATOM_AT_uint8,
    ATOM_AT_int8,
    ATOM_AT_uint16,
    ATOM_AT_int16,
    ATOM_AT_uint32,
    ATOM_AT_int32,
    ATOM_AT_uint64,
    ATOM_AT_int64,
    ATOM_AT_float32,
    ATOM_AT_float64,
    ATOM_AT_bool,
    ATOM_AT_atom,
    ATOM_AT_StringView,
    ATOM_AT_AtomSet,
};

static
int
register_type_atoms(AtomTable* at){
    for(size_t i = 0; i < sizeof TYPE_ATOMS / sizeof TYPE_ATOMS[0];i++){
        int e = AT_store_atom(at, TYPE_ATOMS[i]);
        if(e != 0) __builtin_debugtrap();
    }
    return 0;
}
#endif

#ifndef TYPEINFO_BASIC_TYPE_INFOS_HANDLED
#define TYPEINFO_BASIC_TYPE_INFOS_HANDLED
static const struct {TypeInfo type_info;} TI_uint8_t   = {{.name=ATOM_AT_uint8,      .align=_Alignof(uint8_t),    .size=sizeof(uint8_t),    .kind=TIK_UINT8,  }};
static const struct {TypeInfo type_info;} TI_int8_t    = {{.name=ATOM_AT_int8,       .align=_Alignof(int8_t),     .size=sizeof(int8_t),     .kind=TIK_INT8,   }};
static const struct {TypeInfo type_info;} TI_uint16_t  = {{.name=ATOM_AT_uint16,     .align=_Alignof(uint16_t),   .size=sizeof(uint16_t),   .kind=TIK_UINT16, }};
static const struct {TypeInfo type_info;} TI_int16_t   = {{.name=ATOM_AT_int16,      .align=_Alignof(int16_t),    .size=sizeof(int16_t),    .kind=TIK_INT16,  }};
static const struct {TypeInfo type_info;} TI_uint32_t  = {{.name=ATOM_AT_uint32,     .align=_Alignof(uint32_t),   .size=sizeof(uint32_t),   .kind=TIK_UINT32, }};
static const struct {TypeInfo type_info;} TI_int32_t   = {{.name=ATOM_AT_int32,      .align=_Alignof(int32_t),    .size=sizeof(int32_t),    .kind=TIK_INT32,  }};
static const struct {TypeInfo type_info;} TI_uint64_t  = {{.name=ATOM_AT_uint64,     .align=_Alignof(uint64_t),   .size=sizeof(uint64_t),   .kind=TIK_UINT64, }};
static const struct {TypeInfo type_info;} TI_int64_t   = {{.name=ATOM_AT_int64,      .align=_Alignof(int64_t),    .size=sizeof(int64_t),    .kind=TIK_INT64,  }};
static const struct {TypeInfo type_info;} TI_float = {{.name=ATOM_AT_float32,    .align=_Alignof(float),      .size=sizeof(float),      .kind=TIK_FLOAT32,}};
static const struct {TypeInfo type_info;} TI_double = {{.name=ATOM_AT_float64,    .align=_Alignof(double),     .size=sizeof(double),     .kind=TIK_FLOAT64,}};
static const struct {TypeInfo type_info;} TI__Bool    = {{.name=ATOM_AT_bool,       .align=_Alignof(_Bool),      .size=sizeof(_Bool),      .kind=TIK_BOOL,   }};
static const struct {TypeInfo type_info;} TI_Atom    = {{.name=ATOM_AT_atom,       .align=_Alignof(Atom),       .size=sizeof(Atom),       .kind=TIK_ATOM,   }};
static const struct {TypeInfo type_info;} TI_SV      = {{.name=ATOM_AT_StringView, .align=_Alignof(StringView), .size=sizeof(StringView), .kind=TIK_SV,     }};
static const struct {TypeInfo type_info;} TI_AtomSet = {{.name=ATOM_AT_AtomSet, .align=_Alignof(AtomSet), .size=sizeof(AtomSet), .kind=TIK_ATOM_SET,}};
static const TypeInfoStruct TI_DrJsonValue = { .kind = TIK_DRJSON_VALUE, };
#define TI_int TI_int32_t
#if SIZE_MAX == UINT64_MAX
#define TI_size_t TI_uint64_t
#else
#define TI_size_t TI_uint32_t
#endif
static const TypeInfo* _Null_unspecified const basic_type_infos[] = {
    [TIK_UNSET]   = 0,
    [TIK_UINT8]   = &TI_uint8_t.type_info,
    [TIK_INT8]    = &TI_int8_t.type_info,
    [TIK_UINT16]  = &TI_uint16_t.type_info,
    [TIK_INT16]   = &TI_int16_t.type_info,
    [TIK_UINT32]  = &TI_uint32_t.type_info,
    [TIK_INT32]   = &TI_int32_t.type_info,
    [TIK_UINT64]  = &TI_uint64_t.type_info,
    [TIK_INT64]   = &TI_int64_t.type_info,
    [TIK_FLOAT32] = &TI_float.type_info,
    [TIK_FLOAT64] = &TI_double.type_info,
    [TIK_BOOL]    = &TI__Bool.type_info,
    [TIK_ATOM]    = &TI_Atom.type_info,
    [TIK_SV]      = &TI_SV.type_info,
};
#define BasicType_to_TypeInfoKind(T) _Generic((T*)0, \
    int8_t *:   TIK_INT8, \
    int16_t *:  TIK_INT16, \
    int32_t *:  TIK_INT32, \
    int64_t *:  TIK_INT64, \
    uint8_t *:  TIK_UINT8, \
    uint16_t *: TIK_UINT16, \
    uint32_t *: TIK_UINT32, \
    uint64_t *: TIK_UINT64, \
    Atom*: TIK_ATOM, \
    _Bool*: TIK_BOOL, \
    StringView*: TIK_SV, \
    LongString*: TIK_SV, \
    float* : TIK_FLOAT32, \
    double*: TIK_FLOAT64)
#define BasicType_to_TypeInfo(T) basic_type_infos[BasicType_to_TypeInfoKind(T)]

#define BasicVal_to_TypeInfoKind(x) _Generic(&x, \
    int8_t *:   TIK_INT8, \
    int16_t *:  TIK_INT16, \
    int32_t *:  TIK_INT32, \
    int64_t *:  TIK_INT64, \
    uint8_t *:  TIK_UINT8, \
    uint16_t *: TIK_UINT16, \
    uint32_t *: TIK_UINT32, \
    uint64_t *: TIK_UINT64, \
    Atom*: TIK_ATOM, \
    _Bool*: TIK_BOOL, \
    StringView*: TIK_SV, \
    LongString*: TIK_SV, \
    float* : TIK_FLOAT32, \
    double*: TIK_FLOAT64)
#define BasicVal_to_TypeInfo(x) basic_type_infos[BasicVal_to_TypeInfoKind(x)]
#define BTypeInfo(T) BasicType_to_TypeInfo(T)
#define BTypeInfoOf(x) BasicVal_to_TypeInfo(T)
#endif

#ifdef __clang__
#pragma clang assume_nonnull end
#endif

#endif
