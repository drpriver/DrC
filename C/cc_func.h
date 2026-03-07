#ifndef C_CC_FUNC_H
#define C_CC_FUNC_H
//
// Copyright © 2026-2026, David Priver <david@davidpriver.com>
//
#include <stdint.h>
#include "srcloc.h"
#include "../Drp/atom.h"
#include "../Drp/atom_map.h"
#include "cc_stmt.h"
#include "cc_tok.h"
#include "cc_type.h"

#ifndef MARRAY_CCTOKEN
#define MARRAY_CCTOKEN
#define MARRAY_T CcToken
#include "../Drp/Marray.h"
#endif
#ifndef MARRAY_CCSTATMENT
#define MARRAY_CCSTATMENT
#define MARRAY_T CcStatement
#include "../Drp/Marray.h"
#endif

#ifdef __clang__
#pragma clang assume_nonnull begin
#endif

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
             _padding: 27;
    uint32_t frame_size; // size of params + automatic local vars
    Marray(CcToken)*_Nullable tokens; // If set, the unparsed function body. 
                                      // Return to the parser's free list when done.
    Marray(CcStatement) body;
    struct {
        size_t count;
        Atom _Nullable*_Null_unspecified data;
    } params;
    struct CcVariable*_Nullable*_Null_unspecified param_vars; // set during body parsing, parallel to params
    AtomMap(uintptr_t) labels; // label name -> statement index in body: NOTE: we're punning the pointers, it's not pointers to uintptr_t
    void (*native_func)(void); // native function pointer for calling from interpreted/bytecode, use type to figure out calling convention etc.
    void*_Nullable native_closure; // NativeClosure*, managed by native_call.c
};

#ifdef __clang__
#pragma clang assume_nonnull end
#endif
#endif
