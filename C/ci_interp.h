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
#include "cc_type.h"
#include "../Drp/stringview.h"
#include "../Drp/atom_map.h"
#include "../Drp/atom.h"
#include "../Drp/pointer_map.h"
#include "../Drp/bidi_pointer_map.h"
#include "../Drp/thread_utils.h"

#ifdef __clang__
#pragma clang assume_nonnull begin
#endif

typedef struct CiAllocaBlock CiAllocaBlock;
struct CiAllocaBlock {
    CiAllocaBlock*_Null_unspecified next;
    size_t size;
    // data follows
};

typedef struct CiInterpFrame CiInterpFrame;
struct CiInterpFrame {
    CiInterpFrame*_Null_unspecified parent;
    size_t pc;
    size_t stmt_count;
    CcStatement*_Null_unspecified stmts;
    void* return_buf;
    size_t return_size;
    size_t data_length; // after this is the data, but we can't use a FLA and also embed in CcInterpreter
    void*_Null_unspecified varargs_buf; // points into trailing data, past frame_size
    CiAllocaBlock*_Null_unspecified alloca_list;
};

typedef struct CiVirtualLib CiVirtualLib;
struct CiVirtualLib {
    AtomMap(void*) symbols;
};

typedef struct CiInterpreter CiInterpreter;
struct CiInterpreter {
    CcParser parser;
    CiInterpFrame top_frame;
    union {
        uint32_t flags;
        struct {
            uint32_t can_dlopen: 1,
                     procedural_macros: 1,
                    _padding:30;
        };
    };
    int exit_code;
    AtomMap(CiVirtualLib*) virtual_libs;
    AtomMap opened_libs;
    AtomMap lib_paths;
    PointerMap(CcFunction*, NativeCallCache*) ffi_cache;
    BidiPointerMap(CcFunc*, void(*)(void)) closure_map;
    LOCK_T error_lock;
    LOCK_T atom_lock;
    size_t resolved_variadic,
           resolved_libc,
           resolved_funcs,
           resolved_vars;
};

typedef struct CiArg CiArg;
struct CiArg {
    const void* data;
    size_t size;
    CcQualType type;
};

static int ci_interp_step(CiInterpreter*, CiInterpFrame*);
static int ci_interp_expr(CiInterpreter*, CiInterpFrame*, CcExpr* expr, void* result, size_t size);
static int ci_append_lib_path(CiInterpreter*, StringView);
static int ci_register_pragmas(CiInterpreter*);
static int ci_register_macros(CiInterpreter*);
static int ci_load_library(CiInterpreter*, StringView);
static int ci_load_framework(CiInterpreter*, StringView);
static int ci_call_by_name(CiInterpreter*, StringView name, const CiArg* _Nullable args, uint32_t nargs, void* result, size_t size);
static int ci_call_main(CiInterpreter*, int argc, char*_Null_unspecified*_Null_unspecified argv, char*_Null_unspecified*_Null_unspecified envp, int* out_ret);
static int ci_resolve_refs(CiInterpreter*, _Bool libc_only);
static int ci_backtrace(CiInterpreter* ci, CiInterpFrame*, int);
static int ci_register_sym(CiInterpreter*, StringView libname, StringView symname, void* sym);
static AtomTable* ci_lock_atoms(CiInterpreter*);
static void ci_unlock_atoms(CiInterpreter*, AtomTable*);


#ifdef __clang__
#pragma clang assume_nonnull end
#endif
#endif
