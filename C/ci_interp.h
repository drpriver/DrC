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
    CcFunc* _Nullable callable;
    size_t callable_node_count;
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
                     nofun:              1, // disable fun
                    _padding:            27;
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
           resolve_lock,
           closure_map_lock;
    CiLowerDeps deps;
    struct {
        CiCallDescriptor *intern,
                         *symbolicate,
                         *hotswap,
                         *compile,
                         *module_entry,
                         *module_run,
                         *type_name,
                         *type_tag,
                         *type_is_valid,
                         *type_is_invalid,
                         *type_is_integer,
                         *type_is_float,
                         *type_is_arithmetic,
                         *type_is_pointer,
                         *type_is_struct,
                         *type_is_union,
                         *type_is_array,
                         *type_is_vector,
                         *type_is_slice,
                         *type_is_function,
                         *type_is_enum,
                         *type_is_const,
                         *type_is_volatile,
                         *type_is_atomic,
                         *type_is_unsigned,
                         *type_is_signed,
                         *type_is_callable,
                         *type_is_incomplete,
                         *type_is_variadic,
                         *type_sizeof,
                         *type_alignof,
                         *type_pointee,
                         *type_unqual,
                         *type_count,
                         *type_loc,
                         *type_is_callable_with,
                         *type_is_callable_through,
                         *type_castable_to,
                         *type_make_any,
                         *type_field,
                         *type_field_named,
                         *type_method,
                         *type_method_named,
                         *type_has_field_named,
                         *type_has_method_named,
                         *type_enumerators,
                         *type_enumerator,
                         *type_return_type,
                         *type_param_count,
                         *type_param_type,
                         *type_element_type,
                         *type_underlying_type,
                         *type_fields,
                         *type_methods,
                         *type_validate_field,
                         *type_validate_method,
                         *type_validate_enumerator,
                         *type_validate_param_type,
                         *module_func_count,
                         *module_var_count,
                         *module_type_count,
                         *module_func,
                         *module_func_info,
                         *module_var,
                         *module_type,
                         *module_symbol,
                         *module_symbol_typed,
                         *module_parse_type,
                         *srcloc_file,
                         *srcloc_line,
                         *srcloc_col;
    } runtime_calls;
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


#ifdef __clang__
#pragma clang assume_nonnull end
#endif
#endif
