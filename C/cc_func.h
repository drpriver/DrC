#ifndef C_CC_FUNC_H
#define C_CC_FUNC_H
//
// Copyright © 2026-2026, David Priver <david@davidpriver.com>
//
#include <stdint.h>
#include "srcloc.h"
#include "../Drp/atom.h"
#include "../Drp/atom_map16.h"
#include "../Drp/parray.h"
#include "cc_stmt.h"
#include "cc_tok.h"
#include "cc_type.h"

#ifndef MARRAY_CCTOKEN
#define MARRAY_CCTOKEN
#define MARRAY_T CcToken
#include "../Drp/Marray.h"
#endif

typedef struct CcPrintfFormat CcPrintfFormat;
struct CcPrintfFormat {
    uint32_t string_index; // One-based
    uint32_t first_to_check; // One-based, 0 is syntax only
    SrcLoc loc;
    const CcPrintfFormat* _Nullable next;
};

// Label validation outlives statement trees discarded by sizeof, typeof, etc.
typedef struct CcLabelRef CcLabelRef;
struct CcLabelRef {
    Atom _Nonnull label;
    SrcLoc loc;
    CcDeferContext defer_context;
    CcCleanupRange* _Nullable cleanup;
};
#ifndef MARRAY_CCLABELREF
#define MARRAY_CCLABELREF
#define MARRAY_T CcLabelRef
#include "../Drp/Marray.h"
#endif

#ifdef __clang__
#pragma clang assume_nonnull begin
#else
#ifndef _Nullable
#define _Nullable
#endif
#endif

typedef struct CiFuncOps CiFuncOps;
typedef struct CcVariable CcVariable;
typedef struct CcScope CcScope;

typedef struct CcFuncParam CcFuncParam;
struct CcFuncParam {
    Atom name;
    CcVariable*_Nullable decl;
    _Bool typed_pack;
    uint32_t sz, offset; // set by lowering
    uint32_t native_index; // set by lowering; UINT32_MAX for an ignored ABI argument
    CcExpr*_Nullable default_value;
};

typedef struct CcLabelCtx CcLabelCtx;
struct CcLabelCtx {
    AtomMap16(CcDeferContext) labels;
    Marray(CcLabelRef) gotos;
};

static inline
int
cc_lctx_put_defer(CcLabelCtx* ctx, Allocator allocator, Atom label, const CcDeferContext* defer_context){
    // AM16 reserves an all-zero payload for deletion. Bias the first pointer
    // so labels outside any defer remain present.
    uint64_t payload[2] = {
        (uintptr_t)defer_context->active + 1,
        (uintptr_t)defer_context->deferred_body,
    };
    return AM16_put(&ctx->labels, allocator, label, payload);
}

static inline
_Bool
cc_lctx_has_defer(const CcLabelCtx* ctx, Atom label){
    return AM16_get(&ctx->labels, label) != NULL;
}

// Returns false for an unknown label, leaving out untouched.
static inline
_Bool
cc_lctx_get_defer(const CcLabelCtx* ctx, Atom label, CcDeferContext* out){
    const uint64_t* payload = AM16_get(&ctx->labels, label);
    if(!payload) return 0;
    *out = (CcDeferContext){
        .active = (CcDefer*)(uintptr_t)(payload[0] - 1),
        .deferred_body = (CcDefer*)(uintptr_t)payload[1],
    };
    return 1;
}

typedef struct CcFunc CcFunc;
struct CcFunc {
    CcFunc*_Nullable enclosing; // For nested functions
    CcQualType _Self_type;
    CcFunction* type;
    // TODO: we can have a slim attribute map here instead
    const CcPrintfFormat* _Nullable printf_formats;
    Atom name;
    Atom _Nullable mangle;
    SrcLoc loc; // declaration, updated to definition
    uint32_t extern_: 1,
             static_: 1,
             inline_: 1,
             always_inline: 1,
             noinline: 1,
             hotswappable: 1,
             defined: 1,
             parsed:  1, // If 0, then just an array of tokens instead of array of stmts and needs
                         // to be parsed on first use or codegen.
             parse_failed: 1, // Body failed to parse; tokens have been released. Don't retry.
             libc_builtin: 1,
             inline_blocked: 1,
             interp_lowering: 1,
             noreturn_: 1,
             _padding: 19; // free real estate
    uint32_t frame_size; // size of params + automatic local vars + temporaries;
    Marray(CcToken)*_Nullable tokens; // If set, the unparsed function body.
                                      // Return to the parser's free list when done.
    CcStmtNode*_Nullable body_tree; // lazily allocated, can be allocated with bound initializers due to VLA side-effects.
                                    // owned by this func
    CiFuncOps*_Nullable interp_ops; // executable form; created and owned by ci_lower
    struct {
        size_t count;
        CcFuncParam* _Null_unspecified data;
    } params;
    CcScope*_Nullable param_scope; // declarations from the definition's parameter list
    CcScope*_Nullable definition_scope; // lexical parent, including iteration bindings
    CcLabelCtx label_ctx;
    CcFunc*_Nullable hotswap;
    void (*native_func)(void); // native function pointer for calling from interpreted/bytecode, use type to figure out calling convention etc.
    void*_Nullable native_closure; // NativeClosure*, managed by native_call.c
};

#ifdef __clang__
#pragma clang assume_nonnull end
#endif
#endif
