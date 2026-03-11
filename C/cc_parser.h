#ifndef C_CC_PARSER_H
#define C_CC_PARSER_H
//
// Copyright © 2026-2026, David Priver <david@davidpriver.com>
//
#include "../Drp/atom_map.h"
#include "../Drp/pointer_map.h"
#include "../Drp/typed_enum.h"
#include "../Drp/free_list.h"
#include "../Drp/Allocators/arena_allocator.h"
#include "cc_tok.h"
#include "cc_type.h"
#include "cc_type_cache.h"
#include "cc_expr.h"
#include "cc_stmt.h"
#include "cc_func.h"
#include "cc_var.h"
#include "cc_scope.h"
#include "cpp_preprocessor.h"
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
typedef struct CcPackRecord CcPackRecord;
struct CcPackRecord {
    StringView ident;
    uint16_t pack;
};
#ifndef MARRAY_CCPACKRECORD
#define MARRAY_CCPACKRECORD
#define MARRAY_T CcPackRecord
#include "../Drp/Marray.h"
#endif
#ifdef __clang__
#pragma clang assume_nonnull begin
#endif

typedef struct CcAttributes CcAttributes;
struct CcAttributes {
    union {
        uint64_t bits;
        struct {
            uint64_t packed:            1,
                     transparent_union: 1,
                     has_aligned:       1,
                     _padding:          13,
                     vector_size:       16,
                     aligned:           16,
                     _padding2:         16;
        };
    };
};

static inline
void
cc_clear_attributes(CcAttributes* attrs){
    attrs->bits = 0;
}

enum CcBuiltinFunc TYPED_ENUM(uintptr_t) {
    CC_BUILTIN_NONE,
    CC__builtin_constant_p,
    CC__builtin_offsetof,
    CC__func__,
    CC__type_equals, // (expr-or-type, type)
    CC__is_pointer, // (expr-or-type)
    CC__is_arithmetic, // (expr-or-type)
    CC__is_castable_to, // (expr-or-type, type)
    CC__is_implicitly_castable_to, // (expr-or-type, type)
    CC__has_quals, // (expr-or-type, qualifier)
    CC__is_const, // (expr-or-type)
    CC__atomic_fetch_add, // (ptr, val, memorder)
    CC__atomic_fetch_sub, // (ptr, val, memorder)
    CC__atomic_load_n,    // (ptr, memorder)
    CC__atomic_load,      // (ptr, ret, memorder)
    CC__atomic_store_n,   // (ptr, val, memorder)
    CC__atomic_exchange_n,// (ptr, val, memorder)
    CC__atomic_compare_exchange_n, // (ptr, expected, desired, weak, success_order, failure_order)
    CC__atomic_compare_exchange,   // (ptr, expected, desired_ptr, weak, success_order, failure_order)
    CC__atomic_store,         // (ptr, val_ptr, memorder)
    CC__atomic_exchange,      // (ptr, val_ptr, ret_ptr, memorder)
    CC__atomic_thread_fence,  // (memorder)
    CC__atomic_signal_fence,  // (memorder)
    CC__builtin_va_start,
    CC__builtin_va_end,
    CC__builtin_va_arg,
    CC__builtin_va_copy,
    CC__builtin_expect,
    CC__builtin_unreachable,
    CC__builtin_trap,
    CC__builtin_debugtrap,
    CC__builtin_abort,
    CC__builtin_mul_overflow,
    CC__builtin_add_overflow,
    CC__builtin_sub_overflow,
    CC__builtin_popcount,
    CC__builtin_popcountl,
    CC__builtin_popcountll,
    CC__builtin_ctz,
    CC__builtin_ctzl,
    CC__builtin_ctzll,
    CC__builtin_clz,
    CC__builtin_clzl,
    CC__builtin_clzll,
    CC__builtin_huge_val,
    CC__builtin_huge_valf,
    CC__builtin_huge_vall,
    CC__builtin_nan,
    CC__builtin_nanf,
    CC__nan,
    CC__builtin_alloca,
    CC__builtin_intern,
    CC__bt,
};
TYPEDEF_ENUM(CcBuiltinFunc, uintptr_t);

typedef struct CcParser CcParser;
struct CcParser {
    union {
        uint32_t flags;
        struct {
            uint32_t repl:1, // repl mode, allow top level statements
                     eager_parsing: 1, // Parse function bodies upon definition instead of upon use.
                    _padding:30;
        };
    };
    Marray(CcStatement) toplevel_statements; // only allowed in repl/script mode.
    AtomMap(uintptr_t) toplevel_labels; // label name -> statement index (1-based, like CcFunc.labels)
    CppPreprocessor cpp;
    CcTypeCache type_cache;
    // for lookahead/pushback, LIFO
    Marray(CcToken) pending;
    CcAttributes attributes;
    Marray(CcPackRecord) pack_stack;
    uint16_t pragma_pack; // 0 = default (no pack), otherwise pack(N) value
    CcScope global;
    CcScope* current;
    CcFunc*_Nullable current_func;
    FreeList(CcScope) scratch_scopes;
    FreeList(Marray(CcToken)) scratch_tokens;
    ArenaAllocator scratch_arena;
    uint32_t loop_depth;
    struct CcSwitchCtx* _Nullable switch_ctx;
    AtomMap(uintptr_t) builtins;
    PointerMap used_funcs; // CcFunc* set (value = key)
    PointerMap used_vars;  // CcVariable* set, non-automatic only (value = key)
    PointerMap used_call_types; // CcFunction* set, function types used in indirect calls (value = key)
    PointerMap used_var_calls;  // CcExpr* set, variadic call expressions (value = key)
};


static int cc_parse_all(CcParser*);
static void cc_parser_discard_input(CcParser*);
static int cc_push_scope(CcParser*);
static void cc_pop_scope(CcParser*);
static int cc_register_pragmas(CcParser*);
static int cc_define_builtin_types(CcParser*);
static int cc_parse_func_body(CcParser*, CcFunc*);
static void cc_print_type(MStringBuilder* sb, CcQualType t);

#ifdef __clang__
#pragma clang assume_nonnull end
#endif
#endif
