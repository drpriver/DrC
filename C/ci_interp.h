#ifndef C_CI_INTERP_H
#define C_CI_INTERP_H
//
// Copyright © 2026-2026, David Priver <david@davidpriver.com>
//
#include <stddef.h>
#include "cc_stmt.h"
#include "cc_expr.h"
#include "ci_op.h"
#include "ci_lower.h"
#include "ci_stack.h"
#include "cc_parser.h"
#include "cc_type.h"
#include "../Drp/stringview.h"
#include "../Drp/atom_map.h"
#include "../Drp/atom.h"
#include "../Drp/pointer_map.h"
#include "../Drp/bidi_pointer_map.h"
#include "../Drp/thread_utils.h"
#include "../Drp/blob_table.h"

#ifdef __clang__
#pragma clang assume_nonnull begin
#endif

enum {
    CI_DEFAULT_INLINE_BUDGET = 12,
    CI_DEFAULT_INLINE_HINT_BUDGET = 40,
};

typedef struct CiInterpFrame CiInterpFrame;
typedef union NcCallState NcCallState;
struct CiInterpFrame {
    _Alignas(16) CiInterpFrame*_Null_unspecified parent;
    Atom name;
    size_t pc;
    size_t op_count;
    const CiOp *_Null_unspecified ops;
    void *_Null_unspecified slots; // params/locals/temps storage; the frame's
                                  // trailing data for call frames, an
                                  // interpreter-owned buffer for the top frame
    void* return_buf;
    size_t return_size;
    size_t data_length;
    NcCallState * _Null_unspecified incoming;
    NcCallPlan *_Null_unspecified abi;
    CiStack *_Null_unspecified stack;
    CiStackMark stack_mark; // rewind point before this frame's allocation
};
_Static_assert(_Alignof(CiInterpFrame) % 16 == 0, "slots must stay 16-aligned");
_Static_assert(sizeof(CiInterpFrame) % 16 == 0, "slots must stay 16-aligned");

typedef struct CiVirtualLib CiVirtualLib;
struct CiVirtualLib {
    AtomMap(void*) symbols;
};

typedef struct CiModule CiModule;
struct CiModule {
    CcModule cc;
    Marray(CiOp) ops;
    AtomMap(uintptr_t) labels; // label -> op index + 1
    size_t lowered; // number of parsed top-level nodes already lowered
    uint32_t slot_size;
    uint32_t frame_alignment;
};
_Static_assert(offsetof(CiModule, cc) == 0, "module handle must identify its parsed module");
typedef struct CiTlsThread CiTlsThread;
typedef struct CiClosureData CiClosureData;

typedef struct CiInterpreter CiInterpreter;
struct CiInterpreter {
    CcParser parser;
    CiModule root;
    CiInterpFrame top_frame;
    uint32_t toplevel_slots_cap;
    uint32_t toplevel_alloc_alignment;
    void*_Null_unspecified toplevel_slots;
    union {
        uint32_t flags;
        struct {
            uint32_t can_dlopen:         1,
                     procedural_macros:  1,
                     poison_frame_slots: 1, // debug fill for uninitialized automatic storage
                     opt_noinline:       1,
                    _padding:            28;
        };
    };
    uint32_t inline_budget, 
             inline_hint_budget;
    int exit_code;
    AtomMap(CiVirtualLib*) virtual_libs;
    AtomMap opened_libs;
    AtomMap lib_paths;
    PointerMap(CiModule*, CiModule*) modules;
    BidiPointerMap(CcFunc*, void(*)(void)) closure_map;
    CiClosureData* _Nullable closures;
    LOCK_T error_lock,
           atom_lock,
           resolve_lock;
    // Shared worklist: a cached lowered function's dependencies stay here,
    // including pending work if preparation fails and is retried.
    CiLowerDeps deps;
    size_t next_module_id;
    BlobTable bt;
    ThreadLocalKey tls_key;
    _Bool tls_key_created;
    size_t tls_slot_count;
    LOCK_T tls_lock;
    CiTlsThread*_Nullable tls_threads;
};

typedef struct CiArg CiArg;
struct CiArg {
    const void* data;
    size_t size;
    CcQualType type;
};

static int ci_interp_step(CiInterpreter*, CiInterpFrame*);
static int ci_interp_run(CiInterpreter*, CiInterpFrame*);
static int ci_prepare_toplevel(CiInterpreter*);
static int ci_append_lib_path(CiInterpreter*, StringView);
static int ci_register_pragmas(CiInterpreter*);
static int ci_preload_system_libs(CiInterpreter*);
static int ci_register_macros(CiInterpreter*);
static int ci_load_library(CiInterpreter*, StringView);
static int ci_load_framework(CiInterpreter*, StringView);
// args should omit ignored empty params on appropriate ABIs
static int ci_call_by_name(CiInterpreter*, StringView name, const CiArg* _Nullable args, uint32_t nargs, void* result, size_t size);
static int ci_call_main(CiInterpreter*, int argc, char*_Null_unspecified*_Null_unspecified argv, char*_Null_unspecified*_Null_unspecified envp, int* out_ret);
static int ci_resolve_refs(CiInterpreter*);
static int ci_resolve_deps(CiInterpreter*, CiLowerDeps*);
static int ci_add_root(CiInterpreter*, StringView name);
static int ci_resolve_root(CiInterpreter*, StringView name);
static int ci_backtrace(CiInterpreter* ci, CiInterpFrame*, int);
static int ci_register_sym(CiInterpreter*, StringView libname, StringView symname, void* sym);
static AtomTable* ci_lock_atoms(CiInterpreter*);
static void ci_unlock_atoms(CiInterpreter*, AtomTable*);
// Call before releasing the interpreter or its parser.
// However, this requires you to know there aren't any outstanding
// threads, so in practice you can't really call this unless you have
// control over all of the interpreted code.
static void ci_tls_cleanup(CiInterpreter*);


#ifdef __clang__
#pragma clang assume_nonnull end
#endif
#endif
