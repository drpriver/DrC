#ifndef SIMP_C_PARSE_H
#define SIMP_C_PARSE_H
#include <stdint.h>
#include "../../compiler_warnings.h"
#include "../../Drp/atom.h"
#include "../../Drp/atom_table.h"
#include "../../Drp/atom_map.h"
#include "simp_c_lex.h"

typedef struct CType CType;
typedef struct Scope Scope;
struct Scope {
    AtomMap(CType*) typedef_table;
    AtomMap(CType*) tag_table;
    AtomMap(CDeclaration*) decls;
};
#define MARRAY_T Scope
#include "../../Drp/Marray.h"
#ifdef __clang__
#pragma clang assume_nonnull begin
#endif

typedef struct CLogger CLogger;
struct CLogger {
    uintptr_t handle;
    void (*write_func)(uintptr_t, const char*, size_t);
};

typedef struct CParseCtx CParseCtx;
struct CParseCtx {
    AtomTable at;
    Allocator allocator;
    Allocator scratch;
    Marray(Scope) scopes;
    size_t current_scope;
    CTokens tokens;
    size_t current_token;
    CLogger logger;
};

SIMP_C_LEX_API
void
#ifdef __GNUC__
__attribute__((format(printf, 2, 3)))
#endif
simp_c_logf(CParseCtx* ctx, const char*, ...);

SIMP_C_LEX_API
Atom
#ifdef __GNUC__
__attribute__((format(printf, 2, 3)))
#endif
simp_c_atomf(CParseCtx* ctx, const char*, ...);

SIMP_C_LEX_API
int
simp_c_parse_tu(CParseCtx* ctx);

SIMP_C_LEX_API
int
simp_c_register_atoms(CParseCtx* ctx);

typedef struct CExpression CExpression;
typedef struct CField CField;
struct CField {
    Atom name;
    const CType* type;
    _Bool is_bitfield: 1;
    uintptr_t bits: 6;
    uintptr_t: 57;
};

typedef struct CDeclaration CDeclaration;
struct CDeclaration {
    Atom name;
    const CType* type;
    union {
        CExpression* _Nullable initializer;
    };
    uintptr_t definition: 1;
    uintptr_t constexpr_: 1;
    uintptr_t extern_: 1;
    uintptr_t register_: 1;
    uintptr_t static_: 1;
    uintptr_t thread_local_: 1;
    uintptr_t inline_: 1;
    uintptr_t noreturn_: 1;
#if UINTPTR_MAX == UINT32_MAX
    uintptr_t: 24;
#else
    uintptr_t: 56;
#endif
};

enum CTypeKind {
    CT_BASIC = 0,
    CT_TYPEDEF = 1,
    CT_POINTER = 2,
    CT_ARRAY = 3,
    CT_STRUCT = 4,
    CT_UNION = 5,
    CT_ENUM = 6,
    CT_FUNCTION = 7,
};
typedef enum CTypeKind CTypeKind;

struct CType {
    Atom name;
    uintptr_t kind:      3;
    uintptr_t const_:    1;
    uintptr_t restrict_: 1;
    uintptr_t volatile_: 1;
    uintptr_t _Atomic_:  1;
#if UINTPTR_MAX == UINT32_MAX
    uintptr_t: 25;
#else
    uintptr_t: 57;
#endif
};
_Static_assert(sizeof(CType) == 2 * sizeof(uintptr_t), "");
typedef struct CTypeBasic CTypeBasic;
typedef struct CTypeTypedef CTypeTypedef;
typedef struct CTypePointer CTypePointer;
typedef struct CTypeArray CTypeArray;
typedef struct CTypeStruct CTypeStruct;
typedef struct CTypeUnion CTypeUnion;
typedef struct CTypeEnum CTypeEnum;
typedef struct CTypeFunction CTypeFunction;

static inline const CTypeBasic    *_Nullable isCTypeBasic   (const CType* t){ return t->kind == CT_BASIC?    (const CTypeBasic*)t    : NULL; }
static inline const CTypeTypedef  *_Nullable isCTypeTypedef (const CType* t){ return t->kind == CT_TYPEDEF?  (const CTypeTypedef*)t  : NULL; }
static inline const CTypePointer  *_Nullable isCTypePointer (const CType* t){ return t->kind == CT_POINTER?  (const CTypePointer*)t  : NULL; }
static inline const CTypeArray    *_Nullable isCTypeArray   (const CType* t){ return t->kind == CT_ARRAY?    (const CTypeArray*)t    : NULL; }
static inline const CTypeStruct   *_Nullable isCTypeStruct  (const CType* t){ return t->kind == CT_STRUCT?   (const CTypeStruct*)t   : NULL; }
static inline const CTypeUnion    *_Nullable isCTypeUnion   (const CType* t){ return t->kind == CT_UNION?    (const CTypeUnion*)t    : NULL; }
static inline const CTypeEnum     *_Nullable isCTypeEnum    (const CType* t){ return t->kind == CT_ENUM?     (const CTypeEnum*)t     : NULL; }
static inline const CTypeFunction *_Nullable isCTypeFunction(const CType* t){ return t->kind == CT_FUNCTION? (const CTypeFunction*)t : NULL; }

struct CTypeBasic {
    Atom name;
    uintptr_t kind:        3;
    uintptr_t const_:      1;
    uintptr_t restrict_:   1;
    uintptr_t volatile_:   1;
    uintptr_t _Atomic_:    1;

    #if UINTPTR_MAX == UINT32_MAX
    uintptr_t : 25;
    #else
    uintptr_t : 57;
    #endif
};
_Static_assert(sizeof(CTypeBasic) == sizeof(uintptr_t)*2, "");
struct CTypeTypedef {
    Atom name;
    uintptr_t kind:      3;
    uintptr_t const_:    1;
    uintptr_t restrict_: 1;
    uintptr_t volatile_: 1;
    uintptr_t _Atomic_:  1;
    #if UINTPTR_MAX == UINT32_MAX
    uintptr_t _pad: 25;
    #else
    uintptr_t _pad: 57;
    #endif
    const CType* orig;
};
_Static_assert(sizeof(CTypeTypedef) == sizeof(uintptr_t)*3, "");
struct CTypePointer {
    Atom name;
    uintptr_t kind:      3;
    uintptr_t const_:    1;
    uintptr_t restrict_: 1;
    uintptr_t volatile_: 1;
    uintptr_t _Atomic_:  1;
    #if UINTPTR_MAX == UINT32_MAX
    uintptr_t _pad: 25;
    #else
    uintptr_t _pad: 57;
    #endif
    const CType* pointee;
};
_Static_assert(sizeof(CTypePointer) == sizeof(uintptr_t)*3, "");
struct CTypeArray {
    Atom name;
    uintptr_t kind:      3;
    uintptr_t const_:    1;
    uintptr_t restrict_: 1;
    uintptr_t volatile_: 1;
    uintptr_t _Atomic_:  1;
    uintptr_t is_vla:    1;
    uintptr_t unsized:   1;
    #if UINTPTR_MAX == UINT32_MAX
    uintptr_t _pad: 23;
    #else
    uintptr_t _pad: 55;
    #endif
    const CType* type;
    union {
        CExpression* elen;
        uintptr_t len;
    };
};
_Static_assert(sizeof(CTypeArray) == sizeof(uintptr_t)*4, "");
struct CTypeStruct {
    Atom name;
    uintptr_t kind:      3;
    uintptr_t const_:    1;
    uintptr_t restrict_: 1;
    uintptr_t volatile_: 1;
    uintptr_t _Atomic_:  1;
    uintptr_t opaque:    1;
    #if UINTPTR_MAX == UINT32_MAX
    uintptr_t mcount: 24;
    #else
    uintptr_t mcount: 56;
    #endif
    CField members[];
};
_Static_assert(sizeof(CTypeStruct) == sizeof(uintptr_t)*2, "");
struct CTypeUnion {
    Atom name;
    uintptr_t kind:      3;
    uintptr_t const_:    1;
    uintptr_t restrict_: 1;
    uintptr_t volatile_: 1;
    uintptr_t _Atomic_:  1;
    uintptr_t opaque:    1;
    #if UINTPTR_MAX == UINT32_MAX
    uintptr_t mcount: 24;
    #else
    uintptr_t mcount: 56;
    #endif
    CField members[];
};
_Static_assert(sizeof(CTypeUnion) == sizeof(uintptr_t)*2, "");
typedef struct CEnumDeclarator CEnumDeclarator;
struct CEnumDeclarator {
    Atom name;
    CExpression*_Nullable value;
};
_Static_assert(sizeof(CTypeUnion) == sizeof(uintptr_t)*2, "");
struct CTypeEnum {
    Atom name;
    uintptr_t kind:      3;
    uintptr_t const_:    1;
    uintptr_t restrict_: 1;
    uintptr_t volatile_: 1;
    uintptr_t _Atomic_:  1;
    uintptr_t opaque:    1;
    #if UINTPTR_MAX == UINT32_MAX
    uintptr_t mcount: 24;
    #else
    uintptr_t mcount: 56;
    #endif
    const CType* underlying;
    CEnumDeclarator members[];
};
_Static_assert(sizeof(CTypeEnum) == sizeof(uintptr_t)*3, "");

typedef struct CParameter CParameter;
struct CParameter {
    const CType* type;
    Atom name;
};
struct CTypeFunction {
    Atom name;
    uintptr_t kind:      3;
    uintptr_t const_:    1;
    uintptr_t restrict_: 1;
    uintptr_t volatile_: 1;
    uintptr_t _Atomic_:  1;
    #if UINTPTR_MAX == UINT32_MAX
    uintptr_t param_count: 25;
    #else
    uintptr_t param_count: 55;
    #endif
    const CType* ret_type;
    CParameter params[];
};
_Static_assert(sizeof(CTypeFunction) == sizeof(uintptr_t)*3, "");


#ifdef __clang__
#pragma clang assume_nonnull end
#endif
#endif
