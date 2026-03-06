#ifndef C_CI_INTERP_H
#define C_CI_INTERP_H
//
// Copyright © 2026-2026, David Priver <david@davidpriver.com>
//
#include <stddef.h>
#include "cc_stmt.h"
#include "cc_expr.h"
#include "ci_interp.h"
#include "cc_parser.h"
#include "../Drp/stringview.h"
#include "../Drp/atom_map.h"
#include "../Drp/atom.h"
#include "../Drp/pointer_map.h"

#ifdef __clang__
#pragma clang assume_nonnull begin
#endif

typedef struct CiInterpFrame CiInterpFrame;
struct CiInterpFrame {
    CiInterpFrame*_Nullable parent;
    size_t pc;
    size_t stmt_count;
    CcStatement*_Null_unspecified stmts;
    void* return_buf;
    size_t return_size;
    size_t data_length; // after this is the data, but we can't use a FLA and also embed in CcInterpreter
};

typedef struct CiInterpreter CiInterpreter;
struct CiInterpreter {
    CcParser parser;
    CiInterpFrame top_frame;
    CiInterpFrame *current_frame;
    int exit_code;
    AtomMap(void*) opened_libs;
    AtomMap lib_paths;
    PointerMap(void*) ffi_cache; // CcFunction* -> NativeCallCache*
};

// Execute one statement at current pc. Advances pc.
// Returns 0 on success, >0 on error.
static int ci_interp_step(CiInterpreter*);
// Evaluate an expression, writing sizeof(expr->type) bytes into result.
static int ci_interp_expr(CiInterpreter*, CcExpr* expr, void* result, size_t size);
// Evaluate an expression as an lvalue, returning pointer to its storage.
static int ci_interp_lvalue(CiInterpreter*, CcExpr* expr, void*_Nullable*_Nonnull out, size_t* size);
static int ci_append_lib_path(CiInterpreter*, StringView);
static int ci_register_pragmas(CiInterpreter*);
static int ci_register_macros(CiInterpreter*);
static int ci_load_library(CiInterpreter*, StringView);
static int ci_call_by_name(CiInterpreter*, StringView name, void* result, size_t size);


#ifdef __clang__
#pragma clang assume_nonnull end
#endif
#endif
