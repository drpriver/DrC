//
// Copyright © 2026-2026, David Priver <david@davidpriver.com>
//
#ifndef CI_LOWER_H
#define CI_LOWER_H
#include "ci_op.h"
#include "cc_errors.h"
#include "../Drp/pointer_map.h"
#include "../Drp/blob_atom.h"

#ifdef __clang__
#pragma clang assume_nonnull begin
#endif

// A transient storage location resolved from an AST subobject path.
typedef struct CiFieldLoc CiFieldLoc;
struct CiFieldLoc {
    uint64_t byte_offset;
    uint32_t bit_offset, bit_width;
};

enum CcFuncDepFlags TYPED_ENUM(uintptr_t){
    CC_FUNC_DEP_NONE = 0,
    CC_FUNC_DEP_USED = 0x1,
    CC_FUNC_DEP_ADDR_TAKEN = 0x2,
};
TYPEDEF_ENUM(CcFuncDepFlags, uintptr_t);

enum {
    CI_VAR_DEP_USED = 1,
    CI_VAR_DEP_INITIALIZE = 2,
};

typedef struct CiLowerDeps CiLowerDeps;
typedef struct CiInitTemplate CiInitTemplate;
struct CiLowerDeps {
    // Values are integers cast to pointers, not pointers to integers.
    PointerMap(CcVariable, uintptr_t) vars;
    PointerMap(CcFunc, CcFuncDepFlags) funcs;
    // Templates with symbolic addresses are patched after dependencies resolve.
    PointerMap(CiInitTemplate, uintptr_t) templates;
};

static inline
void
ci_lower_deps_cleanup(CiLowerDeps* deps, Allocator al){
    if(deps->vars.cap)
        Allocator_free(al, deps->vars.data, PM_alloc_size(deps->vars.cap));
    if(deps->funcs.cap)
        Allocator_free(al, deps->funcs.data, PM_alloc_size(deps->funcs.cap));
    if(deps->templates.cap)
        Allocator_free(al, deps->templates.data, PM_alloc_size(deps->templates.cap));
    *deps = (CiLowerDeps){0};
}

static inline
int
ci_deps_add_func(CiLowerDeps* deps, Allocator al, CcFunc* func, uintptr_t flags){
    uintptr_t old = (uintptr_t)PM_get(&deps->funcs, func);
    int err = PM_put(&deps->funcs, al, func, (void*)(old | flags));
    return err ? _cc_oom_error : 0;
}

static inline
int
ci_deps_add_var(CiLowerDeps* deps, Allocator al, CcVariable* var, uintptr_t flags){
    uintptr_t old = (uintptr_t)PM_get(&deps->vars, var);
    int err = PM_put(&deps->vars, al, var, (void*)(old | flags));
    return err ? _cc_oom_error : 0;
}

static inline
int
ci_lower_deps_merge(CiLowerDeps* dst, const CiLowerDeps* src, Allocator al){
    PointerMapItems vars = PM_items(&src->vars);
    for(size_t i = 0; i < vars.count; i++){
        int err = ci_deps_add_var(dst, al, (CcVariable*)(uintptr_t)vars.data[i].key, (uintptr_t)vars.data[i].value);
        if(err) return _cc_oom_error;
    }
    PointerMapItems funcs = PM_items(&src->funcs);
    for(size_t i = 0; i < funcs.count; i++){
        CcFunc* func = (CcFunc*)(uintptr_t)funcs.data[i].key;
        int err = ci_deps_add_func(dst, al, func, (uintptr_t)funcs.data[i].value);
        if(err) return err;
    }
    PointerMapItems templates = PM_items(&src->templates);
    for(size_t i = 0; i < templates.count; i++){
        int err = PM_put(&dst->templates, al, (CiInitTemplate*)(uintptr_t)templates.data[i].key, (void*)1);
        if(err) return _cc_oom_error;
    }
    return 0;
}

typedef struct CiLoweredExpr CiLoweredExpr;
struct CiLoweredExpr {
    Marray(CiOp) ops;
    uint32_t frame_size, value_slot, value_size;
};

typedef struct CiInterpreter CiInterpreter;
static int ci_lower_func(CiInterpreter*, CcFunc*, CiLowerDeps*);
static int ci_lower_toplevel(CiInterpreter*, CiLowerDeps*);
static int ci_lower_standalone_expr(CiInterpreter*, CcExpr*, CiLoweredExpr*, CiLowerDeps*);
static void ci_lowered_expr_cleanup(CiLoweredExpr*, Allocator);

enum CiStaticRelocKind TYPED_ENUM(uint32_t){
    CI_STATIC_RELOC_ABSOLUTE,
    CI_STATIC_RELOC_VAR,
    CI_STATIC_RELOC_FUNC,
    CI_STATIC_RELOC_LITERAL,
};
TYPEDEF_ENUM(CiStaticRelocKind, uint32_t);

typedef struct CiStaticReloc CiStaticReloc;
struct CiStaticReloc {
    CiStaticRelocKind kind;
    union {
        CcVariable* var;
        CcFunc* func;
        const void* literal;
    }; 
    // for ABSOLUTE addend is the address
    uint64_t addend;
    uint32_t offset, size;
    _Bool initialize; // The referenced anonymous object needs constant initialization.
};
struct CiInitTemplate {
    uint32_t size;
    size_t reloc_count;
    unsigned char* bytes;
    _Bool linked;
    CiStaticReloc relocs[];
};
#define MARRAY_T CiStaticReloc
#ifdef __clang__
#pragma clang assume_nonnull end
#endif
#include "../Drp/Marray.h"
#ifdef __clang__
#pragma clang assume_nonnull begin
#endif

typedef struct CiStaticData CiStaticData;
struct CiStaticData {
    CcVariable* var;
    BlobAtom blob;
    uint32_t size; // Object size, excluding the blob's padding.
    Marray(CiStaticReloc) relocs;
};
#define MARRAY_T CiStaticData
#ifdef __clang__
#pragma clang assume_nonnull end
#endif
#include "../Drp/Marray.h"
#ifdef __clang__
#pragma clang assume_nonnull begin
#endif
static int ci_build_static_data(CiInterpreter*, CiStaticData*, CiLowerDeps*);
static void ci_static_data_cleanup(CiStaticData*, Allocator);

#ifdef __clang__
#pragma clang assume_nonnull end
#endif
#endif
