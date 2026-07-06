#ifndef C_CC_FUNC_H
#define C_CC_FUNC_H
//
// Copyright © 2026-2026, David Priver <david@davidpriver.com>
//
#include <stdint.h>
#include "srcloc.h"
#include "../Drp/atom.h"
#include "../Drp/atom_map.h"
#include "../Drp/parray.h"
#include "cc_stmt.h"
#include "cc_tok.h"
#include "cc_type.h"

#ifndef MARRAY_CCTOKEN
#define MARRAY_CCTOKEN
#define MARRAY_T CcToken
#include "../Drp/Marray.h"
#endif

#ifdef __clang__
#pragma clang assume_nonnull begin
#endif

typedef struct CiFuncOps CiFuncOps;
typedef struct CcVariable CcVariable;

typedef struct CcLabelCtx CcLabelCtx;
struct CcLabelCtx {
    AtomMap(CcStmtNode) labels;
    Parray(CcStmtNode) gotos;
};

typedef struct CcFunc CcFunc;
struct CcFunc {
    CcFunc*_Nullable enclosing; // For nested functions
    CcFunction* type;
    Atom name;
    Atom _Nullable mangle;
    SrcLoc loc; // declaration, updated to definition
    uint32_t extern_: 1,
             static_: 1,
             inline_: 1,
             defined: 1,
             parsed:  1, // If 0, then just an array of tokens instead of array of stmts and needs
                         // to be parsed on first use or codegen.
             parse_failed: 1, // Body failed to parse; tokens have been released. Don't retry.
             addr_taken: 1, // address taken (used as value, not just called directly)
             libc_builtin: 1,
             printf_like: 1,
             _padding: 23;
    uint32_t frame_size; // size of params + automatic local vars + temporaries;
    Marray(CcToken)*_Nullable tokens; // If set, the unparsed function body.
                                      // Return to the parser's free list when done.
    CcStmtNode*_Nullable body_tree; // set when parsed; owned by this func
    CiFuncOps*_Nullable interp_ops; // executable form; created and owned by ci_lower
    struct {
        size_t count;
        Atom _Nullable*_Null_unspecified data;
    } params;
    CcVariable*_Nullable*_Null_unspecified param_vars; // set during body parsing, parallel to params
    CcLabelCtx label_ctx;
    CcFunc*_Nullable hotswap;
    void (*native_func)(void); // native function pointer for calling from interpreted/bytecode, use type to figure out calling convention etc.
    void*_Nullable native_closure; // NativeClosure*, managed by native_call.c
};

#ifdef __clang__
#pragma clang assume_nonnull end
#endif
#endif
