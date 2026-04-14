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

enum CcValueClass TYPED_ENUM(int) {
    CC_RUNTIME_VALUE,
    CC_LINKTIME_VALUE,
    CC_CONSTEXPR_VALUE,
};
TYPEDEF_ENUM(CcValueClass, int);

typedef struct CcAttributes CcAttributes;
struct CcAttributes {
    union {
        uint64_t bits;
        struct {
            uint64_t packed:            1,
                     transparent_union: 1,
                     has_aligned:       1,
                     printf_like:       1,
                     is_noreturn:       1,
                     is_thread_local:   1,
                     _padding:          10,
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
    CC__atomic_fetch_add, // (ptr, val, memorder)
    CC__atomic_fetch_sub, // (ptr, val, memorder)
    CC__atomic_fetch_and, // (ptr, val, memorder)
    CC__atomic_fetch_or,  // (ptr, val, memorder)
    CC__atomic_fetch_xor, // (ptr, val, memorder)
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
    CC_InterlockedExchange,
    CC_InterlockedExchange8,
    CC_InterlockedExchange16,
    CC_InterlockedExchange64,
    CC_InterlockedCompareExchange,
    CC_InterlockedCompareExchange8,
    CC_InterlockedCompareExchange16,
    CC_InterlockedCompareExchange64,
    CC_InterlockedCompareExchange128,
    CC_InterlockedIncrement,
    CC_InterlockedIncrement16,
    CC_InterlockedIncrement64,
    CC_InterlockedDecrement,
    CC_InterlockedDecrement16,
    CC_InterlockedDecrement64,
    CC_InterlockedExchangeAdd,
    CC_InterlockedExchangeAdd8,
    CC_InterlockedExchangeAdd16,
    CC_InterlockedExchangeAdd64,
    CC_InterlockedAnd,
    CC_InterlockedAnd8,
    CC_InterlockedAnd16,
    CC_InterlockedAnd64,
    CC_InterlockedOr,
    CC_InterlockedOr8,
    CC_InterlockedOr16,
    CC_InterlockedOr64,
    CC_InterlockedXor,
    CC_InterlockedXor8,
    CC_InterlockedXor16,
    CC_InterlockedXor64,
    CC__umul128,
};
TYPEDEF_ENUM(CcBuiltinFunc, uintptr_t);

typedef struct CcParser CcParser;
struct CcParser {
    union {
        uint32_t flags;
        struct {
            uint32_t repl:1, // repl mode, allow top level statements
                     eager_parsing: 1, // Parse function bodies upon definition instead of upon use.
                     auto_typedef: 1, // automatically insert tagged types untagged into scope as typedef.
                    _padding:29;
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
    CcQualType current_tag_type;
    FreeList(CcScope) scratch_scopes;
    FreeList(Marray(CcToken)) scratch_tokens;
    #define CC_RECYCLE_EXPRS 1
    #if CC_RECYCLE_EXPRS
    FreeList(CcExpr) exprs[3];
    #endif
    ArenaAllocator scratch_arena;
    uint32_t loop_depth;
    uint32_t switch_depth;
    struct CcSwitchCtx* _Nullable switch_ctx;
    AtomMap(uintptr_t) builtins;
    AtomMap(uintptr_t) type_intro;
    PointerMap used_funcs; // CcFunc* set (value = key)
    PointerMap used_vars;  // CcVariable* set, non-automatic only (value = key)
    PointerMap used_call_types; // CcFunction* set, function types used in indirect calls (value = key)
    PointerMap used_var_calls;  // CcExpr* set, variadic call expressions (value = key)

    // common types
    CcQualType char_star,
               const_char_star,
               void_star,
               const_void_star,
               builtin_field,
               builtin_enumerator,
               builtin_va_list,
               builtin_va_list_ptr;
};


static int cc_parse_all(CcParser*);
static void cc_parser_discard_input(CcParser*);
static int cc_push_scope(CcParser*);
static void cc_pop_scope(CcParser*);
static int cc_register_pragmas(CcParser*);
static int cc_register_extern_var(CcParser*, StringView name, CcQualType type);
static int cc_define_builtin_types(CcParser*);
static int cc_parse_func_body(CcParser*, CcFunc*);
static void cc_print_type(MStringBuilder* sb, CcQualType t);
static void cc_print_runtime_value(CcParser*, CcQualType, const void*, MStringBuilder*, int indent);

// NOTE: these structs are designed so they match the layout on 
// any of our targets.
typedef struct CiRtField CiRtField; // return by _Type.fields
struct CiRtField {
    CcQualType type;
    const char* name;
    unsigned name_length,
             offset,
             bitwidth,
             bitoffset;
};

typedef struct CiRtEnumerator CiRtEnumerator;
struct CiRtEnumerator {
    const char* name;
    unsigned name_length;
    long long value;
};

#ifdef __clang__
#pragma clang assume_nonnull end
#endif
#endif
