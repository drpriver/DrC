//
// Copyright © 2026-2026, David Priver <david@davidpriver.com>
//
#ifndef C_CC_PARSER_C
#define C_CC_PARSER_C
#include <stdarg.h>
#include <float.h>
#include <math.h>
#include "../Drp/msb_atomize.h"
#include "../Drp/bit_util.h"
#include "../Drp/parray.h"
#include "../Drp/merge_sort.h"
#include "../Drp/ckdint.h"
#include "../Drp/switch_macros.h"
#include "../Drp/countof.h"
#include "../Drp/drp_profile.h"
#include "cc_expr.h"
#include "cc_parser.h"
#include "cc_type_check.h"
#include "cpp_preprocessor.h"
#include "cc_errors.h"
#include "cc_printer.h"

#ifndef MARRAY_T_CCFUNCPARAM
#define MARRAY_T_CCFUNCPARAM
#define MARRAY_T CcFuncParam
#include "../Drp/Marray.h"
#endif

#ifdef __clang__
#pragma clang assume_nonnull begin
#endif

enum {CC_CONTROL_LOOP_MAX=255};

typedef struct CcParsedParams CcParsedParams;
struct CcParsedParams {
    Marray(CcFuncParam) names;
    CcScope* _Nullable scope;
    CcStmtNode* _Nullable bounds;
    _Bool unspecified_vla;
    SrcLoc typed_pack_loc;
};
static int cc_merge_default_args(CcParser*, CcFunc*, CcParsedParams*, _Bool, SrcLoc);
static int cc_clone_expr(CcParser*, CcExpr*, unsigned, SrcLoc, _Bool, CcExpr*_Nullable*_Nonnull);
static int cc_clone_default_arg(CcParser*, CcExpr*, SrcLoc, CcParseFlags, CcExpr*_Nullable*_Nonnull);
static int cc_prepare_default_arg(CcParser*, CcExpr*, unsigned, Parray*, _Bool);
static size_t cc_expr_nvalues(CcExpr*);
static int cc_source_location_expr(CcParser*, CcBuiltinOp, SrcLoc, CcExpr*_Nullable*_Nonnull);
static int cc_parse_expr(CcParser* p, CcParseFlags, CcExpr* _Nullable* _Nonnull out, CcQualType expected);
static int cc_parse_assignment_expr(CcParser* p, CcParseFlags, CcExpr* _Nullable* _Nonnull out, CcQualType);
static int cc_parse_ternary_expr(CcParser* p, CcParseFlags, CcExpr* _Nullable* _Nonnull out, CcQualType expected);
static int cc_parse_comma_tail(CcParser*, CcParseFlags, CcExpr*, CcExpr* _Nullable* _Nonnull, CcQualType expected);
static int cc_parse_assignment_tail(CcParser*, CcParseFlags, CcExpr*, CcExpr* _Nullable* _Nonnull);
static int cc_parse_ternary_tail(CcParser*, CcParseFlags, CcExpr*, CcExpr* _Nullable* _Nonnull, CcQualType expected);
static int cc_parse_infix(CcParser* p, CcParseFlags, CcExpr* left, int min_prec, CcExpr* _Nullable* _Nonnull out);
static int cc_parse_prefix(CcParser* p, CcParseFlags, CcExpr* _Nullable* _Nonnull out, CcQualType expected);
static int cc_parse_primary(CcParser* p, CcParseFlags, CcExpr* _Nullable* _Nonnull out, CcQualType expected);
static int cc_parse_postfix(CcParser* p, CcParseFlags, CcExpr* operand, CcExpr* _Nullable* _Nonnull out);
static int cc_parse_lambda(CcParser* p, CcParseFlags, SrcLoc loc, CcExpr* _Nullable* _Nonnull out);
static int cc_parse_lambda_body(CcParser* p, CcParseFlags, SrcLoc loc, CcQualType type, CcParsedParams* param_names, CcExpr* _Nullable* _Nonnull out);
static int cc_skip_to_next_comma_or_paren(CcParser* p, const char* context);
static int cc_parse_Generic(CcParser* p, CcParseFlags, CcExpr* _Nullable* _Nonnull out, CcQualType);
static int cc_next_token(CcParser* p, CcToken* tok);
static int cc_unget(CcParser* p, CcToken* tok);
static int cc_peek(CcParser* p, CcToken* tok);
static int cc_expect_punct(CcParser* p, CcPunct punct);
static int cc_expect_punct_supp(CcParser* p, CcPunct punct, const char*);
static void _cc_release_expr(CcParser* p, CcExpr*, size_t nvalues);
static void cc_release_expr(CcParser* p, CcExpr*);
static CcExpr*_Nullable _cc_alloc_expr(CcParser* p, size_t nvalues);
static CcExpr*_Nullable cc_make_expr(CcParser* p, CcExprKind kind, SrcLoc loc, CcQualType type, size_t nvalues);
static void cc_mark_mutable_storage(CcExpr*);
warn_unused static int cc_pointer_of(CcParser* p, CcQualType pointee, CcQualType* out);
warn_unused static int cc_slice_of(CcParser* p, CcQualType pointee, SrcLoc loc, CcQualType* out);
LOG_PRINTF(3, 4) static int cc_error(CcParser*, SrcLoc, const char*, ...);
LOG_PRINTF(3, 4) static MStringBuilder* cc_start_error(CcParser*, SrcLoc, const char*, ...);
static MStringBuilder* cc_start_errorv(CcParser*, SrcLoc, const char*, va_list);
static int cc_finish_error(CcParser*, SrcLoc);
LOG_PRINTF(3, 4) static void cc_warn(CcParser*, SrcLoc, const char*, ...);
LOG_PRINTF(3, 4) static void cc_info(CcParser*, SrcLoc, const char*, ...);
#define cc_unimplemented(p, loc, msg) (cc_error(p, loc, "UNIMPLEMENTED: " msg " at %s:%d", __FILE__, __LINE__), CC_UNIMPLEMENTED_ERROR)
#define cc_unreachable(p, loc, msg) (cc_error(p, loc, "UNREACHABLE code reached: " msg " at %s:%d", __FILE__, __LINE__), CC_UNREACHABLE_ERROR)
#define cc_ice(cc, loc, fmt, ...) (cc_error(p, loc, "ICE: " fmt " at %s:%d", __VA_ARGS__, __FILE__, __LINE__), CC_UNREACHABLE_ERROR)
// Only use this when it is absolutely impossible, prefer cc_ice() above
// if it could be a bug.
#ifndef cc_unsafe_unreachable
#ifdef __GNUC__
#define cc_unsafe_unreachable() __builtin_unreachable()
#elif defined _MSC_VER
#define cc_unsafe_unreachable() __assume(0)
#else
#define cc_unsafe_unreachable() abort()
#endif
#endif
static _Bool cc_binop_lookup(CcPunct punct, CcExprKind* kind, int* prec);
static int cc_va_list_to_ptr(CcParser* p, SrcLoc loc, CcExpr*_Nonnull*_Nonnull e, _Bool writable);
typedef struct CcEvalCtx CcEvalCtx;
typedef struct CcEvalVisit CcEvalVisit;
struct CcEvalVisit {
    CcEvalVisit* previous;
    CcExpr* expr;
    uint32_t offset, size;
};
struct CcEvalCtx {
    CcParser* parser;
    _Bool allow_const;
    _Bool allow_inexact_float_casts;
    unsigned variable_depth;
    unsigned evaluation_depth;
    CcEvalVisit* expressions;
    CcEvalVisit* objects;
};

typedef struct CcStaticControl CcStaticControl;
struct CcStaticControl {
    CcStaticControl* _Nullable previous;
    _Bool is_loop;
    unsigned runtime_control_depth;
    enum {CC_STATIC_NEXT, CC_STATIC_BREAK, CC_STATIC_CONTINUE} control;
};

typedef struct CcStaticFor CcStaticFor;
struct CcStaticFor {
    CcStaticFor* _Nullable previous;
    CcScope* bindings;
    CcStaticControl flow;
};

static
_Bool
cc_static_control_pending(CcParser* p){
    for(CcStaticControl* c = p->static_control; c; c = c->previous)
        if(c->control) return 1;
    return 0;
}

static
_Bool
cc_static_rebind_target(CcParser* p, CcExpr* e){
    return p->static_for && e->kind == CC_EXPR_VARIABLE && cc_scope_lookup_var(p->static_for->bindings, e->var->name, CC_SCOPE_NO_WALK) == e->var;
}

static int cc_eval_expr(CcEvalCtx*, CcExpr*, CcExpr*_Nullable*_Nonnull);
static int cc_eval_integer(CcEvalCtx*, CcExpr*, int64_t*);
static int cc_lvalue_alignment(CcParser*, const CcExpr*, uint32_t*);
static int cc_pointer_alignment(CcParser*, const CcExpr*, uint32_t*);
static CiUint128 cc_eval_u128(const CcTargetConfig*, CcExpr*);
static int cc_eval_case_range(CcParser*, SrcLoc, CcQualType, CcExpr*, CcExpr*, CiUint128*, CiUint128*, _Bool*);
static int cc_eval_truthy(CcEvalCtx*, CcExpr*, _Bool*);
static int cc_check_linktime_expr(CcParser*, CcExpr*, _Bool, unsigned);
static _Bool cc_assign_lookup(CcPunct punct, CcExprKind* kind);
static Marray(CcToken)*_Nullable cc_get_scratch(CcParser* p);
static void cc_release_scratch(CcParser* p, Marray(CcToken)*);
static Allocator cc_allocator(CcParser*p);
static Allocator cc_scratch_allocator(CcParser*p);
static int cc_parse_declarator(CcParser* p, CcParseFlags flags, CcQualType* out_head, CcQualType*_Nonnull*_Nonnull out_tail, Atom _Nullable * _Nullable out_name, SrcLoc* _Nullable out_name_loc, CcParsedParams *_Nullable out_param_names);
static CcQualType cc_intern_qualtype(CcParser* p, CcQualType t);
static int cc_adjust_parameter_type(CcParser*, CcQualType, CcQualType*);
static _Bool cc_is_type_start(CcParser* p, CcToken* tok);
static int cc_parse_func_body_inner(CcParser* p, CcFunc* f, _Bool terminate_on_rbrace);
static int cc_parse_local_methods(CcParser* p);
static CcLabelCtx* cc_label_ctx(CcParser* p);
static int cc_check_gotos(CcParser* p, CcLabelCtx* ctx);
static int cc_check_transfer(CcParser*, SrcLoc, CcDeferContext, CcDeferContext);
static int cc_parse_type_name(CcParser* p, CcParseFlags flags, CcQualType* out, CcParsedParams* _Nullable param_names);
static int cc_sizeof_as_expr(CcParser* p, CcQualType t, SrcLoc loc, CcExpr* _Nullable* _Nonnull out);
static int cc_alignof_as_expr(CcParser* p, CcQualType t, SrcLoc loc, CcExpr* _Nullable* _Nonnull out);
static int cc_sizeof_as_uint(CcParser* p, CcQualType t, SrcLoc loc, uint32_t* out);
static int cc_alignof_as_uint(CcParser* p, CcQualType t, SrcLoc loc, uint32_t* out);
static int cc_check_cast(CcParser* p, CcQualType from, CcQualType to, SrcLoc loc);
static CcExpr* _Nullable cc_value_expr(CcParser* p, SrcLoc loc, CcQualType type);
static CcExpr* _Nullable cc_int64_expr(CcParser* p, SrcLoc loc, CcQualType type, int64_t);
static CcExpr* _Nullable cc_integer_bits_expr(CcParser*, SrcLoc, CcQualType, CiUint128);
static CcExpr* _Nullable cc_uint64_expr(CcParser* p, SrcLoc loc, CcQualType type, uint64_t);
static CcExpr* _Nullable cc_unary_expr(CcParser* p, CcExprKind kind, SrcLoc loc, CcQualType type, CcExpr* operand);
static CcExpr* _Nullable cc_binary_expr(CcParser* p, CcExprKind kind, SrcLoc loc, CcQualType type, CcExpr* left, CcExpr* right);
typedef struct CcDeclBase CcDeclBase;
static int cc_check_func_compat(CcParser* p, CcFunc* existing, const CcDeclBase* declbase, CcQualType new_type, _Bool definition, SrcLoc loc);
static int cc_merge_compatible_decl_types(CcParser* p, CcQualType old, CcQualType new_, CcQualType* out);
static int cc_qualify_object_type(CcParser* p, CcQualType type, uintptr_t quals, CcQualType* out);
static int cc_parse_attributes(CcParser* p, CcParseFlags flags, CcAttributes* attrs);
static _Bool cc_is_c23_attribute_start(CcParser* p);
static int cc_parse_c23_attributes(CcParser* p, CcParseFlags flags, CcAttributes* attrs);
static int cc_parse_declspec(CcParser* p, CcParseFlags flags, CcAttributes* attrs);
static int cc_parse_struct_or_union(CcParser* p, CcParseFlags flags, SrcLoc loc, _Bool is_union, _Bool member, CcQualType* base_type);
static int cc_check_anon_member_duplicates(CcParser* p, CcField* existing, uint32_t existing_count, CcQualType anon_type, SrcLoc loc);
static _Bool cc_has_field(CcParser*, CcField*_Nullable, uint32_t, Atom);
static int cc_lookup_field(CcParser*, CcField*_Nullable, uint32_t, Atom, CcFieldPath*_Nullable, CcQualType*, CcQualType*_Nullable, CcField*_Nullable*_Nonnull);
static int cc_lookup_member(CcParser*, CcStruct*, Atom, CcFieldPath*_Nullable, CcQualType*, CcQualType*_Nullable, CcField*_Nullable*_Nonnull);
static int cc_append_method(CcParser*, CcStruct*, CcFunc*, SrcLoc);
static int cc_lookup_field_offset(CcParser*, CcQualType, Atom, uint64_t*, CcQualType*, CcField*_Nullable*_Nonnull);
static int cc_compute_struct_layout(CcParser* p, CcStruct* s, uint16_t pack_value);
static int cc_compute_union_layout(CcParser* p, CcUnion* u, uint16_t pack_value);
static int cc_parse_init_list(CcParser* p, CcParseFlags flags, CcExpr* _Nullable* _Nonnull out, CcQualType target_type);
static int cc_desugar_compound_literal(CcParser* p, CcExpr* compound_lit, CcExpr*_Nullable*_Nonnull out);
static const CcTargetConfig* cc_target(const CcParser*);
static int cc_handle_static_assert(CcParser*, CcParseFlags);
static CcStmtNode*_Nullable cc_stmt_node(CcParser*, CcStmtKind, SrcLoc, uint32_t count);
static int cc_stmt_scope_vars(CcParser*, CcStmtNode*);
static void cc_free_stmt_tree(CcParser*, CcStmtNode*_Nullable);
static CcStmtSink*_Nullable cc_push_stmt_sink(CcParser*);
static void cc_pop_stmt_sink(CcParser*, CcStmtSink*);
static int cc_sink_push(CcParser*, CcStmtNode*);
static int cc_finalize_stmt_list(CcParser*, SrcLoc, Parray(CcStmtNode)*, _Bool always_compound, CcStmtNode*_Nullable*_Nonnull out);
static int cc_has_builtin(void* _Null_unspecified ctx, CppPreprocessor* cpp, SrcLoc, CppTokens* outtoks, const CppTokens* args, const Marray(size_t)* arg_seps);
static int cc_check_printf_format(CcParser*, const CcPrintfFormat*, CcExpr*_Nonnull*_Nonnull, uint32_t, SrcLoc);
static int cc_merge_printf_formats(CcParser*, const CcPrintfFormat*_Nullable, const CcPrintfFormat*_Nullable*_Nonnull);
static int cc_apply_printf_formats(CcParser*, CcFunc*, const CcPrintfFormat*_Nullable);

enum {
    CC_NO_ERROR             = _cc_no_error,
    CC_OOM_ERROR            = _cc_oom_error,
    CC_SYNTAX_ERROR         = _cc_syntax_error,
    CC_UNREACHABLE_ERROR    = _cc_unreachable_error,
    CC_UNIMPLEMENTED_ERROR  = _cc_unimplemented_error,
    CC_FILE_NOT_FOUND_ERROR = _cc_file_not_found_error,
    CC_VALUE_ERROR          = _cc_invalid_value_error,
    CC_OVERFLOW_ERROR       = _cc_overflow_error,
    CC_NOT_CONSTANT_ERROR   = _cc_not_constant_error,
};

struct CcStmtSink {
    CcStmtSink*_Null_unspecified prev;
    Parray(CcStmtNode) stmts;
};

#ifdef __clang__
#pragma clang assume_nonnull end
#endif
#ifndef MARRAY_CCQUALTYPE
#define MARRAY_CCQUALTYPE
#define MARRAY_T CcQualType
#include "../Drp/Marray.h"
#endif
#ifndef MARRAY_CCFIELD
#define MARRAY_CCFIELD
#define MARRAY_T CcField
#include "../Drp/Marray.h"
#endif
#ifndef MARRAY_CCINITENTRY
#define MARRAY_CCINITENTRY
#define MARRAY_T CcInitEntry
#include "../Drp/Marray.h"
#endif
#ifndef MARRAY_CCSWITCHENTRY
#define MARRAY_CCSWITCHENTRY
#define MARRAY_T CcSwitchEntry
#include "../Drp/Marray.h"
#endif

typedef struct CcStaticCase CcStaticCase;
struct CcStaticCase { CiUint128 value, high; SrcLoc loc; _Bool empty; };
#define MARRAY_T CcStaticCase
#include "../Drp/Marray.h"
typedef struct CcInitWorkEntry CcInitWorkEntry;
struct CcInitWorkEntry {
    CcInitEntry init;
    uint32_t nested, next_blob;
};
#define MARRAY_T CcInitWorkEntry
#include "../Drp/Marray.h"

typedef struct CcInitWorkList CcInitWorkList;
struct CcInitWorkList {
    Marray(CcInitWorkEntry) entries;
    SrcLoc loc;
    CcQualType type;
    uint32_t root;
};
#define MARRAY_T CcInitWorkList
#include "../Drp/Marray.h"

typedef struct CcInitPathNode CcInitPathNode;
struct CcInitPathNode {
    PointerMap children;
    uint32_t index, child, next, slot, blobs;
    _Bool active;
};
#define MARRAY_T CcInitPathNode
#include "../Drp/Marray.h"

typedef struct CcInitIndex CcInitIndex;
struct CcInitIndex {
    CcParser*_Nonnull parser;
    Marray(CcInitWorkList) lists;
    Marray(CcInitPathNode) nodes;
};

#ifdef __clang__
#pragma clang assume_nonnull begin
#endif

typedef struct CcInitBuilder CcInitBuilder;
struct CcInitBuilder {
    Marray(CcInitEntry) entries;
    _Bool may_override;
};

static int cc_parse_init(CcParser* p, CcParseFlags flags, CcQualType target, _Bool braced, SrcLoc loc, CcInitBuilder* buf, uint32_t*_Nullable out_max_index, CcExpr*_Nullable first_value, Marray(uint32_t)* path);

typedef struct CcSpecifier CcSpecifier;
struct CcSpecifier {
    union {
        uint32_t bits;
        struct {
            uint32_t sp_typebits: 9,
                     sp_storagebits: 6,
                     _sp_typedef: 1,
                     sp_funcbits: 2,
                     sp_qualbits: 4,

                     _sp_infer: 1,
                     sp_complex: 1,
                     _padding1: 32-24;
        };
        struct {
            uint32_t
                     sp___auto_type:  1,
                     sp_unsigned:     1,
                     sp_signed:       1,
                     sp_long:         2,
                     sp_short:        1,
                     sp_int:          1,
                     sp_char:         1,
                     sp_int128:       1,

                     sp_auto:         1,
                     sp_constexpr:    1,
                     sp_extern:       1,
                     sp_register:     1,
                     sp_static:       1,
                     sp_thread_local: 1,
                     sp_typedef:      1,

                     sp_inline:       1,
                     sp_noreturn:     1,

                     sp_const:        1,
                     sp_volatile:     1,
                     sp_atomic:       1,
                     sp_restrict:     1,

                     sp_infer_type:    1,
                     _sp_complex:     1,
                     _padding2:       32-24;
        };
    };
};
_Static_assert(sizeof(CcSpecifier) == sizeof(uint32_t), "");
static int cc_parse_declaration_specifier(CcParser* p, CcParseFlags flags, CcDeclBase* base);
static int cc_parse_enum(CcParser* p, CcParseFlags flags, SrcLoc loc, _Bool member, CcQualType* base_type);

typedef struct CcDeclBase CcDeclBase;
struct CcDeclBase {
    CcQualType type;
    CcSpecifier spec;
    SrcLoc loc;
    uint16_t alignment; // from _Alignas or prefix __attribute__((aligned))
    uint16_t alignas_alignment;
    _Bool has_alignas;
    _Bool member;
};
static int cc_check_decl_alignment(CcParser*, const CcDeclBase*, CcQualType, _Bool);

typedef struct CcParsedDecls CcParsedDecls;
struct CcParsedDecls {
    CcVariable* _Nullable last_object;
    CcFunc* _Nullable function_definition;
};
static int cc_parse_decls(CcParser* p, CcParseFlags flags, const CcDeclBase* declbase, CcParsedDecls* out);
static int cc_resolve_specifiers(CcParser* p, CcDeclBase* declbase);
static int cc_check_decl_type_qualifiers(CcParser*, CcQualType, CcSpecifier, SrcLoc);
typedef struct CcControlLabel CcControlLabel;
struct CcControlLabel {
    CcControlLabel* _Nullable prev;
    Atom name;
};
static int cc_parse_label(CcParser*, CcParseFlags, CcStmtNode*_Nullable*_Nonnull);
static int cc_parse_statement(CcParser* p, CcParseFlags flags, CcControlLabel*_Nullable labels, CcStmtNode*_Nullable*_Nonnull out);
static int cc_parse_one(CcParser* p, CcParseFlags flags);
static int cc_parse_one_inner(CcParser* p, CcParseFlags flags, CcControlLabel*_Nullable labels);
static int cc_static_goto_missing(CcParser* p);
static int cc_parse_selection_header(CcParser*, CcParseFlags, const char*, CcExpr*_Nullable*_Nonnull);
static int cc_parse_for_condition(CcParser*, CcParseFlags, SrcLoc, CcExpr*_Nullable*_Nonnull);

static
CcScope*_Nullable
cc_alloc_scope(CcParser* p){
    CcScope* s = fl_pop(&p->scratch_scopes);
    if(s) cc_scope_clear(s);
    else {
        s = Allocator_zalloc(cc_allocator(p), 1, sizeof *s, _Alignof(CcScope));
        if(!s) return NULL;
    }
    s->parent = p->current;
    return s;
}

static
int
cc_push_scope(CcParser* p){
    CcScope* s = cc_alloc_scope(p);
    if(!s) return CC_OOM_ERROR;
    p->current = s;
    return 0;
}

static
void
cc_pop_scope(CcParser* p){
    CcScope* s = p->current;
    p->current = s->parent;
    if(!s->retained) fl_push(&p->scratch_scopes, s);
}

static
int
cc_type_error(CcParser* p, SrcLoc loc, CcQualType type, const char* fmt, ...){
    va_list va;
    va_start(va, fmt);
    MStringBuilder* sb = cc_start_errorv(p, loc, fmt, va);
    va_end(va);
    msb_write_literal(sb, ". The type is '");
    cc_print_type(sb, type);
    msb_write_char(sb, '\'');
    return cc_finish_error(p, loc);
}

static
int
cc_types_error(CcParser* p, SrcLoc loc, CcQualType a, CcQualType b, const char* fmt, ...){
    va_list va;
    va_start(va, fmt);
    MStringBuilder* sb = cc_start_errorv(p, loc, fmt, va);
    va_end(va);
    msb_write_literal(sb, ". The types are '");
    cc_print_type(sb, a);
    msb_write_literal(sb, "' and '");
    cc_print_type(sb, b);
    msb_write_char(sb, '\'');
    return cc_finish_error(p, loc);
}

static
void
cc_arithmetic_operand_type(const CcTargetConfig* target, CcExpr* e, CcQualType* out){
    cc_promote_bitfield_operand(target, e->type, cc_expr_field_bit_width(e), out);
}

static
int
cc_integer_promote(CcParser* p, CcQualType t, CcQualType* out, SrcLoc loc){
    const char* failure;
    if(cc_integer_promotion(t, out, &failure)){
        return cc_type_error(p, loc, t, "%s", failure);
    }
    return 0;
}

static
int
cc_integer_promote_expr(CcParser* p, CcExpr* e, CcQualType* out, SrcLoc loc){
    CcQualType type;
    cc_arithmetic_operand_type(cc_target(p), e, &type);
    return cc_integer_promote(p, type, out, loc);
}

static
int
cc_usual_arithmetic(CcParser* p, CcExpr* lhs, CcExpr* rhs, CcQualType* out, SrcLoc loc){
    CcQualType a, b;
    cc_arithmetic_operand_type(cc_target(p), lhs, &a);
    cc_arithmetic_operand_type(cc_target(p), rhs, &b);
    const char* failure;
    if(cc_usual_arithmetic_conversions(cc_target(p), a, b, out, &failure)){
        return cc_types_error(p, loc, lhs->type, rhs->type, "%s", failure);
    }
    return 0;
}

static
int
cc_check_atomic_size(CcParser* p, CcQualType type, uint32_t sz, SrcLoc loc){
    if(!sz || (sz & (sz-1)))
        return cc_type_error(p, loc, type, "atomic operand size %u is not a power of 2", sz);
    if(sz > cc_target(p)->atomic_lock_free_max)
        return cc_type_error(p, loc, type, "atomic operand size %u exceeds target's maximum lock-free size %u", sz, cc_target(p)->atomic_lock_free_max);
    switch(sz){
        case 1: case 2: case 4: case 8: case 16: return 0;
        default: return cc_type_error(p, loc, type, "unsupported atomic operand size %u", sz);
    }
}

static
int
cc_check_atomic_object_type(CcParser* p, CcQualType type, SrcLoc loc){
    if(!type.is_atomic) return 0;
    uint32_t sz;
    int err = cc_sizeof_as_uint(p, type, loc, &sz);
    if(err) return err;
    return cc_check_atomic_size(p, type, sz, loc);
}

static
int
cc_check_atomic_alignment(CcParser* p, const CcExpr* e, _Bool pointer, uint32_t size, SrcLoc loc){
    uint32_t alignment;
    int err = pointer ? cc_pointer_alignment(p, e, &alignment)
        : cc_lvalue_alignment(p, e, &alignment);
    if(err) return err;
    if((UINT32_C(1) << alignment) < size)
        return cc_error(p, loc, "underaligned atomic access requires %u-byte alignment", size);
    return 0;
}

static
int
cc_check_atomic_object_access(CcParser* p, const CcExpr* e, SrcLoc loc){
    int err = cc_check_atomic_object_type(p, e->type, loc);
    if(err || !e->type.is_atomic || !e->is_lvalue) return err;
    uint32_t size;
    err = cc_sizeof_as_uint(p, e->type, loc, &size);
    if(err) return err;
    return cc_check_atomic_alignment(p, e, 0, size, e->loc);
}

static
int
cc_check_atomic_rmw(CcParser* p, const CcExpr* e, SrcLoc loc){
    if(!cc_type_supports_atomic_rmw(e->type))
        return cc_type_error(p, loc, e->type, "atomic read-modify-write requires scalar atomic type");
    return cc_check_atomic_object_access(p, e, loc);
}

static
int
cc_check_atomic_type_specifier_type(CcParser* p, CcQualType type, SrcLoc loc){
    const char* failure;
    if(cc_check_atomic_specifier(type, &failure)){
        return cc_type_error(p, loc, type, "%s", failure);
    }
    return 0;
}

static
int
cc_check_atomic_memory_order(CcParser* p, CcAtomicOp op, unsigned order, SrcLoc loc, _Bool failure_order){
    if(order >= CC_MO_COUNT)
        return cc_error(p, loc, "invalid memory order value %u", order);
    if(failure_order){
        if(order == CC_MO_RELEASE || order == CC_MO_ACQ_REL)
            return cc_error(p, loc, "atomic compare-exchange failure order cannot be release or acq_rel");
        return 0;
    }
    if(op == CC_ATOMIC_LOAD || op == CC_ATOMIC_LOAD_N){
        if(order == CC_MO_RELEASE || order == CC_MO_ACQ_REL)
            return cc_error(p, loc, "atomic load memory order cannot be release or acq_rel");
    }
    if(op == CC_ATOMIC_STORE || op == CC_ATOMIC_STORE_N){
        if(order == CC_MO_CONSUME || order == CC_MO_ACQUIRE || order == CC_MO_ACQ_REL)
            return cc_error(p, loc, "atomic store memory order cannot be consume, acquire, or acq_rel");
    }
    return 0;
}


static
int
cc_deref_type_or_error(CcParser* p, CcQualType t, CcQualType* out, SrcLoc loc, _Bool subscript){
    const char* failure;
    if(cc_deref_type(t, out, subscript, &failure)){
        return cc_type_error(p, loc, t, "%s", failure);
    }
    return 0;
}

static
int
cc_check_type_has_size_or_error(CcParser* p, CcQualType type, SrcLoc loc){
    if(!cc_type_has_size(type)) return cc_type_error(p, loc, type, "type has no size");
    return 0;
}

static
int
cc_check_incdec(CcParser* p, const CcExpr* e, SrcLoc loc){
    const char* failure;
    if(cc_check_incdec_operand(e->type, &failure)) return cc_type_error(p, loc, e->type, "%s", failure);
    return cc_check_atomic_rmw(p, e, loc);
}





static
int
cc_member_path(CcParser* p, CcStruct* s, Atom name, _Bool owner, CcFieldPath* out){
    CcQualType type;
    CcField* field;
    CcFieldPath path;
    int err = cc_lookup_member(p, s, name, &path, &type, NULL, &field);
    if(err) return err;
    if(!field) return CC_NOT_CONSTANT_ERROR;
    if(owner && field->is_method){
        err = cc_field_path_take(cc_allocator(p), path, cc_field_path_count(path)-1, out);
        cc_field_path_free(cc_allocator(p), path);
        return err ? CC_OOM_ERROR : 0;
    }
    *out = path;
    return 0;
}

static
int
cc_base_path(CcParser* p, CcQualType from, CcQualType to, CcFieldPath* out){
    from = ccqt_unaligned(from);
    to = ccqt_unaligned(to);
    if(cc_qualification_convertible(from, to)){
        *out = (CcFieldPath){0};
        return 0;
    }
    CcStruct* s = ccqt_as_struct(from);
    for(uint32_t i = 0; i < s->field_count; i++){
        CcField* field = &s->fields[i];
        if(field->name || field->is_bitfield) continue;
        CcQualType sub = field->type;
        sub.quals |= from.quals;
        if(!cc_plan9_base(sub, to)) continue;
        CcFieldPath suffix;
        int err = cc_base_path(p, sub, to, &suffix);
        if(err) return err;
        CcFieldPath prefix = {0};
        err = cc_field_path_make(cc_allocator(p), &i, 1, &prefix);
        if(!err) err = cc_field_path_concat(cc_allocator(p), prefix, suffix, out);
        cc_field_path_free(cc_allocator(p), prefix);
        cc_field_path_free(cc_allocator(p), suffix);
        return err ? CC_OOM_ERROR : 0;
    }
    return CC_NOT_CONSTANT_ERROR;
}

static
int
cc_set_member_path(CcParser* p, CcExpr* e, CcQualType owner_type, Atom name, _Bool owner, uint32_t builtin_index){
    if(ccqt_kind(owner_type) == CC_STRUCT || ccqt_kind(owner_type) == CC_UNION){
        CcStruct* s = ccqt_as_struct(owner_type);
        return cc_member_path(p, s, name, owner, &e->field_path);
    }
    e->field_path = (CcFieldPath){.n_components=1, .idx0=builtin_index};
    return 0;
}

static
int
cc_is_null_pointer_constant(CcParser* p, CcExpr* e, _Bool* out){
    if(ccqt_bt_eq(e->type, CCBT_nullptr_t)){
        *out = 1;
        return 0;
    }
    if(e->kind == CC_EXPR_CAST
    && ccqt_kind(e->type) == CC_POINTER
    && ccqt_is_integer(e->lhs->type)){
        CcQualType pointee = ccqt_unaligned(ccqt_as_ptr(e->type)->pointee);
        if(ccqt_bt_eq(pointee, CCBT_void) && !pointee.quals)
            return cc_is_null_pointer_constant(p, e->lhs, out);
    }
    if(!ccqt_is_integer(e->type)){
        *out = 0;
        return 0;
    }
    int64_t value;
    int err = cc_eval_integer(&(CcEvalCtx){.parser=p}, e, &value);
    if(err == _cc_not_constant_error){
        *out = 0;
        return 0;
    }
    if(err) return err;
    *out = value == 0;
    return 0;
}

static
int
cc_implicit_cast(CcParser* p, CcExpr* e, CcQualType target, CcExpr* _Nullable* _Nonnull out){
    int err = cc_check_atomic_object_access(p, e, e->loc);
    if(err) return err;
    if(ccqt_bt_eq(target, CCBT_void)){
        *out = e;
        return 0;
    }
    if(e->type.bits == target.bits && cc_is_value_operand_type(e->type)){
        *out = e;
        return 0;
    }
    _Bool is_null_pointer_constant = 0;
    if(ccqt_kind(target) == CC_POINTER
           || ccqt_bt_eq(target, CCBT_nullptr_t)
           || ccqt_kind(target) == CC_BLOCK_POINTER
           || ccqt_bt_eq(target, CCBT__Type)){
        err = cc_is_null_pointer_constant(p, e, &is_null_pointer_constant);
        if(err) return err;
    }
    if(!is_null_pointer_constant && !cc_implicit_convertible(cc_target(p), e->type, target)){
        MStringBuilder* sb = cc_start_error(p, e->loc, "cannot implicitly convert from '");
        cc_print_type(sb, e->type);
        msb_write_literal(sb, "' to '");
        cc_print_type(sb, target);
        msb_write_char(sb, '\'');
        return cc_finish_error(p, e->loc);
    }
    if(ccqt_kind(target) == CC_SLICE && ccqt_kind(e->type) == CC_ARRAY && (e->kind == CC_EXPR_VALUE || e->kind == CC_EXPR_BLOB) && cc_expr_array_data(e) && cc_expr_array_count(e)){
        CcQualType pointer;
        err = cc_pointer_of(p, ccqt_as_slice(target)->pointee, &pointer);
        if(err) return err;
        CcInitList* il = Allocator_zalloc_extra(cc_allocator(p), 2, sizeof *il->entries, sizeof *il, _Alignof(CcInitList));
        if(!il) return CC_OOM_ERROR;
        CcExpr* count = cc_uint64_expr(p, e->loc, ccqt_basic(cc_target(p)->size_type), cc_expr_array_count(e) - 1);
        CcExpr* data = cc_make_expr(p, CC_EXPR_CAST, e->loc, pointer, 0);
        CcExpr* slice = cc_make_expr(p, CC_EXPR_INIT_LIST, e->loc, target, 0);
        if(!count || !data || !slice){
            if(count) cc_release_expr(p, count);
            if(data) _cc_release_expr(p, data, 0);
            if(slice) _cc_release_expr(p, slice, 0);
            Allocator_free_extra(cc_allocator(p), il, 2, sizeof *il->entries, sizeof *il, _Alignof(CcInitList));
            return CC_OOM_ERROR;
        }
        data->lhs = e;
        il->loc = e->loc;
        il->count = 2;
        il->entries[0].path = (CcFieldPath){.n_components=1, .idx0=0};
        il->entries[0].value = count;
        il->entries[1].path = (CcFieldPath){.n_components=1, .idx0=1};
        il->entries[1].value = data;
        slice->init_list = il;
        *out = slice;
        return 0;
    }
    if(e->kind == CC_EXPR_COMPOUND_LITERAL){
        err = cc_desugar_compound_literal(p, e, &e);
        if(err) return err;
    }
    if(ccqt_is_array(e->type) && (ccqt_kind(target) == CC_POINTER || ccqt_kind(target) == CC_SLICE))
        cc_mark_mutable_storage(e);
    if(ccqt_bt_eq(target, CCBT__Any) && !ccqt_bt_eq(e->type, CCBT__Any)){
        CcTypeKind k = ccqt_kind(e->type);
        if((ccqt_is_array(e->type)) || k == CC_FUNCTION){
            CcQualType ptr;
            err = cc_pointer_of(p, e->type, &ptr);
            if(err) return err;
            CcExpr* addr = cc_unary_expr(p, k == CC_FUNCTION ? CC_EXPR_CAST : CC_EXPR_ADDR, e->loc, ptr, e);
            if(!addr) return CC_OOM_ERROR;
            e = addr;
        }
    }
    if(ccqt_is_array(e->type) && ccqt_kind(target) == CC_POINTER){
        CcQualType element = ccqt_qualified_array_element(e->type);
        CcQualType to = ccqt_as_ptr(target)->pointee;
        if(!cc_types_compatible_ignoring_cv(element, to) && cc_plan9_base(element, to)){
            CcQualType decay;
            err = cc_pointer_of(p, element, &decay);
            if(err) return err;
            CcExpr* cast = cc_unary_expr(p, CC_EXPR_CAST, e->loc, decay, e);
            if(!cast) return CC_OOM_ERROR;
            e = cast;
        }
    }
    if(ccqt_kind(e->type) == CC_POINTER && ccqt_kind(target) == CC_POINTER){
        CcQualType from = ccqt_as_ptr(e->type)->pointee;
        CcQualType to = ccqt_as_ptr(target)->pointee;
        if(!cc_types_compatible_ignoring_cv(from, to) && cc_plan9_base(from, to)){
            CcExpr* member = cc_make_expr(p, CC_EXPR_ARROW, e->loc, to, 1);
            if(!member) return CC_OOM_ERROR;
            member->is_lvalue = 1;
            err = cc_base_path(p, from, to, &member->field_path);
            if(err){
                _cc_release_expr(p, member, 1);
                return err;
            }
            member->values[0] = e;
            CcExpr* addr = cc_unary_expr(p, CC_EXPR_ADDR, e->loc, target, member);
            if(!addr){
                cc_field_path_free(cc_allocator(p), member->field_path);
                _cc_release_expr(p, member, 1);
                return CC_OOM_ERROR;
            }
            *out = addr;
            return 0;
        }
    }
    CcExpr* cast = cc_make_expr(p, CC_EXPR_CAST, e->loc, target, 0);
    if(!cast) return CC_OOM_ERROR;
    cast->lhs = e;
    *out = cast;
    return 0;
}

static
int
cc_decay_array(CcParser* p, CcExpr*_Nullable*_Nonnull expr){
    CcExpr* e = *expr;
    CcQualType type = e->type;
    if(!ccqt_is_array(type)) return 0;
    CcQualType pointer;
    int err = cc_pointer_of(p, ccqt_qualified_array_element(type), &pointer);
    if(err) return err;
    return cc_implicit_cast(p, e, pointer, expr);
}

static
int
cc_decay(CcParser* p, CcExpr*_Nullable*_Nonnull expr){
    CcExpr* e = *expr;
    if(ccqt_is_array(e->type))
        return cc_decay_array(p, expr);
    if(ccqt_kind(e->type) == CC_FUNCTION){
        CcQualType pointer;
        int err = cc_pointer_of(p, e->type, &pointer);
        if(err) return err;
        return cc_implicit_cast(p, e, pointer, expr);
    }
    return 0;
}

static
int
cc_require_scalar(CcParser* p, CcExpr*_Nullable*_Nonnull expr, SrcLoc loc, const char* context){
    int err = cc_decay(p, expr);
    if(err) return err;
    CcExpr* value = *expr;
    if(cc_is_scalar_operand(value->type))
        return cc_check_atomic_object_access(p, value, value->loc);
    return cc_type_error(p, loc, (*expr)->type, "%s requires scalar type", context);
}

static
int
cc_implicit_cast_to_index(CcParser* p, CcExpr* e, CcExpr* _Nullable* _Nonnull out){
    CcQualType src;
    cc_arithmetic_operand_type(cc_target(p), e, &src);
    if(!ccqt_is_integer(src))
        return cc_type_error(p, e->loc, e->type, "index requires integer type");
    const CcTargetConfig* t = cc_target(p);
    CcQualType target = ccqt_basic(ccqt_is_unsigned(src, !t->char_is_signed)? t->size_type : ccbt_to_signed(t->size_type));
    uint32_t size;
    int err = cc_sizeof_as_uint(p, src, e->loc, &size);
    if(err) return err;
    err = cc_implicit_cast(p, e, target, out);
    if(!err && size > t->sizeof_[t->size_type])
        (*out)->cast.checked_index = 1;
    return err;
}

static
_Bool
cc_is_type_start(CcParser* p, CcToken* tok){
    if(tok->type == CC_KEYWORD){
        switch(tok->kw.kw){
            case CC_void: case CC_char: case CC_short: case CC_int:
            case CC_long: case CC_float: case CC_double:
            case CC_signed: case CC_unsigned: case CC_bool:
            case CC___int128:
            case CC_struct: case CC_union: case CC_enum:
            case CC_typeof: case CC_typeof_unqual:
            case CC___auto_type:
            case CC__Complex: case CC__Imaginary:
            case CC___fp16:
            case CC___bf16:
            case CC__Float16: case CC__Float32: case CC__Float64: case CC__Float128: case CC__Float32x: case CC__Float64x:
            case CC__Decimal32: case CC__Decimal64: case CC__Decimal128:
            case CC__BitInt:
            case CC__Atomic:
            case CC_const: case CC_volatile: case CC_restrict:
            case CC__Any:
            case CC__Type:
            case CC__Self:
                return 1;
            case CC_do:
            case CC_if:
            case CC_for:
            case CC_asm:
            case CC_true:
            case CC_auto:
            case CC_else:
            case CC_case:
            case CC_goto:
            case CC_false:
            case CC_break:
            case CC_while:
            case CC_extern:
            case CC_inline:
            case CC_return:
            case CC__Defer:
            case CC_sizeof:
            case CC_static:
            case CC_switch:
            case CC_alignas:
            case CC_alignof:
            case CC_default:
            case CC_typedef:
            case CC_nullptr:
            case CC_continue:
            case CC_register:
            case CC__Generic:
            case CC_constexpr:
            case CC__Noreturn:
            case CC_thread_local:
            case CC___forceinline:
            case CC_static_assert:
            case CC__Countof:
            case CC___attribute__:
            case CC___declspec:
                return 0;
        }
    }
    if(tok->type == CC_IDENTIFIER){
        CcSymbol sym;
        return cc_scope_lookup_symbol(p->current, tok->ident.ident, CC_SCOPE_WALK_CHAIN, &sym) && sym.kind == CC_SYM_TYPEDEF;
    }
    return 0;
}

static
int
cc_check_derived_type(CcParser* p, CcQualType type, SrcLoc loc){
    const char* failure;
    if(cc_check_derived_type_rules(cc_target(p), type, &failure)) return cc_type_error(p, loc, type, "%s", failure);
    return 0;
}

static int cc_const_object_type(CcParser*, CcQualType, CcQualType*);

static
_Bool
cc_is_compound_type_start(CcParser* p, CcToken* tok){
    if(cc_is_type_start(p, tok)) return 1;
    if(tok->type != CC_KEYWORD) return 0;
    switch(tok->kw.kw){
        case CC_static: case CC_thread_local: case CC_constexpr: case CC_register:
        case CC_auto: case CC_extern: case CC_typedef: case CC_alignas:
            return 1;
        default: return 0;
    }
}

static
int
cc_apply_vector_type(CcParser* p, CcQualType* type, uint32_t size, SrcLoc loc){
    if(!size) return 0;
    int err;
    union {
        CcPointer pointer;
        CcSlice slice;
        CcArray array;
        CcVla vla;
        CcAligned aligned;
    } copy;
    CcQualType* child = NULL;
    switch(ccqt_is_basic(*type) ? CC_BASIC : ccqt_raw_kind(*type)){
        case CC_POINTER: case CC_BLOCK_POINTER:
            copy.pointer = *ccqt_as_ptr(*type);
            child = &copy.pointer.pointee;
            break;
        case CC_SLICE:
            copy.slice = *ccqt_as_slice(*type);
            child = &copy.slice.pointee;
            break;
        case CC_ARRAY:
            copy.array = *ccqt_as_array(*type);
            child = &copy.array.element;
            break;
        case CC_VLA:
            copy.vla = *ccqt_as_vla(*type);
            child = &copy.vla.element;
            break;
        case CC_FUNCTION: {
            CcFunction* old = ccqt_as_function(*type);
            CcQualType result = old->return_type;
            err = cc_apply_vector_type(p, &result, size, loc);
            if(err) return err;
            CcFunction* function = cc_intern_function(&p->type_cache, cc_allocator(p),
                result, old->params, old->param_count, old->fixed_param_count,
                old->is_variadic, old->no_prototype);
            if(!function) return CC_OOM_ERROR;
            *type = (CcQualType){.bits=(uintptr_t)function | type->quals};
            return cc_check_derived_type(p, *type, loc);
        }
        case CC_ALIGNED:
            copy.aligned = *ccqt_as_aligned(*type);
            child = &copy.aligned.underlying;
            break;
        default: break;
    }
    if(child){
        err = cc_apply_vector_type(p, child, size, loc);
        if(err) return err;
        *type = cc_intern_qualtype(p, (CcQualType){.bits=(uintptr_t)&copy | type->quals});
        if(!type->bits) return CC_OOM_ERROR;
        return cc_check_derived_type(p, *type, loc);
    }
    CcQualType base = ccqt_unaligned(*type);
    uint32_t length;
    const char* failure;
    if(cc_vector_element_count(cc_target(p), base, size, &length, &failure))
        return cc_type_error(p, loc, base, "%s", failure);
    CcVector* vector = cc_intern_vector(&p->type_cache, cc_allocator(p), base, length, size);
    if(!vector) return CC_OOM_ERROR;
    *type = (CcQualType){.bits=(uintptr_t)vector | base.quals};
    return cc_check_derived_type(p, *type, loc);
}

static
int
cc_parse_type_syntax_inner(CcParser* p, CcParseFlags flags, CcQualType* out, CcParsedParams* _Nullable param_names, CcDeclBase* _Nullable storage, _Bool type_expression){
    CcDeclBase base = {0};
    int err = cc_parse_declaration_specifier(p, flags, &base);
    if(err) return err;
    if(storage){
        if(base.spec.sp_auto || base.spec.sp_extern || base.spec.sp_typedef || base.spec.sp_funcbits)
            return cc_error(p, base.loc, "invalid storage class in compound literal");
        if(base.spec.sp_register && !p->current_func)
            return cc_error(p, base.loc, "register compound literal requires block scope");
        if(base.spec.sp_thread_local && p->current_func && !base.spec.sp_static)
            return cc_error(p, base.loc, "block-scope thread_local compound literal requires static");
    }
    else if(base.has_alignas)
        return cc_error(p, base.loc, "_Alignas requires a compound literal");
    else if(base.spec.sp_storagebits || base.spec.sp_typedef || base.spec.sp_funcbits)
        return cc_error(p, base.loc, "storage class or function specifier is not allowed in a type name");
    if(!base.spec.bits && !base.type.bits){
        CcToken peek;
        err = cc_peek(p, &peek);
        if(err) return err;
        return cc_error(p, peek.loc, "Expected type name");
    }
    err = cc_resolve_specifiers(p, &base);
    if(err) return err;
    if(base.spec.sp_infer_type)
        return cc_error(p, base.loc, type_expression && param_names
            ? "Expected type in expression, got only qualifiers/storage class"
            : "Expected type name, got only qualifiers/storage class");
    CcQualType head = {0};
    CcQualType* tail = &head;
    Atom name = NULL;
    err = cc_parse_declarator(p, flags, &head, &tail, type_expression ? &name : NULL, NULL, param_names);
    if(err) return err;
    if(name)
        return cc_error(p, base.loc, "unexpected declarator name '%.*s' in type expression", name->length, name->data);
    *tail = base.type;
    *out = cc_intern_qualtype(p, head);
    if(!out->bits) return CC_OOM_ERROR;
    err = cc_parse_attributes(p, flags, &p->attributes);
    if(err) return err;
    err = cc_apply_vector_type(p, out, p->attributes.vector_size, base.loc);
    if(err) return err;
    if(storage && base.spec.sp_constexpr){
        if(cc_constexpr_forbidden_qualifiers(*out))
            return cc_type_error(p, base.loc, *out, "constexpr object cannot have volatile or atomic subobjects");
        err = cc_const_object_type(p, *out, out);
        if(err) return err;
    }
    if(storage){
        *storage = base;
        if(p->attributes.has_aligned && p->attributes.aligned > storage->alignment)
            storage->alignment = p->attributes.aligned;
    }
    return cc_check_derived_type(p, *out, base.loc);
}

static
int
cc_parse_type_syntax(CcParser* p, CcParseFlags flags, CcQualType* out, CcParsedParams* _Nullable param_names, CcDeclBase* _Nullable storage, _Bool type_expression){
    CcAttributes saved = p->attributes;
    cc_clear_attributes(&p->attributes);
    int err = cc_parse_type_syntax_inner(p, flags, out, param_names, storage, type_expression);
    p->attributes = saved;
    return err;
}

static
int
cc_parse_compound_type(CcParser* p, CcParseFlags flags, CcQualType* out, CcParsedParams* _Nullable param_names, CcDeclBase* _Nullable storage){
    return cc_parse_type_syntax(p, flags, out, param_names, storage, 0);
}

static
int
cc_parse_type_name(CcParser* p, CcParseFlags flags, CcQualType* out, CcParsedParams* _Nullable param_names){
    return cc_parse_type_syntax(p, flags, out, param_names, NULL, 0);
}

static
int
cc_parse_abstract_type_name(CcParser* p, CcParseFlags flags, CcQualType* out){
    return cc_parse_type_syntax(p, flags, out, NULL, NULL, 1);
}

static
int
cc_parse_type_string(CcParser* p, CcScope* scope, SrcLoc loc, StringView source, CcQualType* out){
    *out = (CcQualType){0};
    CcScope* old_current = p->current;
    int err = 0;
    p->current = scope;
    cc_parser_discard_input(p);
    CppFrame frame = {
        .txt = source,
        .file_id = loc.is_actually_a_pointer ? ((SrcLocExp*)((uintptr_t)loc.pointer.bits << 1))->file_id : loc.file_id,
        .line = loc.is_actually_a_pointer ? ((SrcLocExp*)((uintptr_t)loc.pointer.bits << 1))->line : loc.line,
        .column = loc.is_actually_a_pointer ? ((SrcLocExp*)((uintptr_t)loc.pointer.bits << 1))->column : loc.column,
    };
    err = ma_push(CppFrame)(&p->cpp.frames, p->cpp.allocator, frame);
    if(err){
        err = CC_OOM_ERROR;
        goto done;
    }
    err = cc_parse_abstract_type_name(p, (CcParseFlags){0}, out);
    if(err) goto done;
    CcToken tok;
    err = cc_next_token(p, &tok);
    if(err) goto done;
    if(tok.type != CC_EOF)
        err = cc_error(p, tok.loc, "unexpected token after type name");
    done:
    p->current = old_current;
    cc_parser_discard_input(p);
    return err;
}

static
int
cc_check_function_definition_type(CcParser* p, CcFunction* type, SrcLoc loc){
    const char* failure;
    if(cc_check_function_definition_types(cc_target(p), type, &failure)) return cc_type_error(p, loc, (CcQualType){.bits=(uintptr_t)type}, "%s", failure);
    return 0;
}

static
int
cc_parse_lambda_body(CcParser* p, CcParseFlags flags, SrcLoc loc, CcQualType type, CcParsedParams* param_names, CcExpr* _Nullable* _Nonnull out){
    if(param_names->unspecified_vla)
        return cc_error(p, loc, "unspecified VLA bound is only allowed in a function prototype");
    int err;
    CcToken tok;
    err = cc_next_token(p, &tok); // consume '{'
    if(err){
        ma_cleanup(CcFuncParam)(&param_names->names, cc_allocator(p));
        return err;
    }
    CcFunction* ftype = ccqt_as_function(type);
    err = cc_check_function_definition_type(p, ftype, loc);
    if(err) return err;
    CcFunc* func = Allocator_zalloc(cc_allocator(p), 1, sizeof *func, _Alignof(CcFunc));
    if(!func){
        ma_cleanup(CcFuncParam)(&param_names->names, cc_allocator(p));
        return CC_OOM_ERROR;
    }
    func->name = NULL;
    func->type = ftype;
    func->loc = loc;
    func->defined = 1;
    err = cc_merge_default_args(p, func, param_names, 1, loc);
    if(err) return err;
    func->param_scope = param_names->scope;
    func->body_tree = param_names->bounds;
    func->definition_scope = cc_scope_retain(p->current);
    func->module = p->module;
    func->enclosing = p->current_func;
    err = cc_parse_func_body_inner(p, func, 1);
    if(err) return err;
    err = cc_expect_punct(p, CC_rbrace);
    if(err) return err;
    CcExpr* node = cc_make_expr(p, CC_EXPR_FUNCTION, loc, (CcQualType){.bits=(uintptr_t)func->type}, 0);
    if(!node) return CC_OOM_ERROR;
    node->func = func;
    return cc_parse_postfix(p, flags, node, out);
}

static
int
cc_parse_lambda(CcParser* p, CcParseFlags flags, SrcLoc loc, CcExpr* _Nullable* _Nonnull out){
    CcParsedParams param_names = {0};
    CcQualType type;
    int err = cc_parse_type_syntax(p, flags, &type, &param_names, NULL, 1);
    if(err){
        cleanup_names:
        ma_cleanup(CcFuncParam)(&param_names.names, cc_allocator(p));
        return err;
    }
    CcToken peek;
    err = cc_peek(p, &peek);
    if(err) goto cleanup_names;
    if(ccqt_kind(type) != CC_FUNCTION){
        ma_cleanup(CcFuncParam)(&param_names.names, cc_allocator(p));
        if(peek.type == CC_PUNCTUATOR && peek.punct.punct == CC_lbrace)
            return cc_type_error(p, loc, type, "Lambda requires a function type, got non-function type");
    }
    if(peek.type != CC_PUNCTUATOR || peek.punct.punct != CC_lbrace){
        for(size_t i = 0; i < param_names.names.count; i++){
            CcExpr* value = param_names.names.data[i].default_value;
            if(value){
                ma_cleanup(CcFuncParam)(&param_names.names, cc_allocator(p));
                return cc_error(p, value->loc, "default arguments require a function declaration or definition");
            }
        }
        ma_cleanup(CcFuncParam)(&param_names.names, cc_allocator(p));
        CcExpr* type_val = cc_value_expr(p, loc, ccqt_basic(CCBT__Type));
        if(!type_val) return CC_OOM_ERROR;
        type_val->uinteger = type.bits;
        return cc_parse_postfix(p, flags, type_val, out);
    }
    return cc_parse_lambda_body(p, flags, loc, type, &param_names, out);
}

static
int
cc_desugar_compound_literal(CcParser* p, CcExpr* cl, CcExpr*_Nullable*_Nonnull out){
    if(cl->defer_compound_storage){
        cl->is_lvalue = 1;
        *out = cl;
        return 0;
    }
    int err;
    CcQualType type = cl->type;
    SrcLoc loc = cl->loc;
    CcVariable* anon = Allocator_zalloc(cc_allocator(p), 1, sizeof *anon, _Alignof(CcVariable));
    if(!anon) return CC_OOM_ERROR;
    *anon = (CcVariable){
        .name = nil_atom,
        .loc = loc,
        .type = type,
        .automatic = p->current_func != NULL && !cl->compound_static && !cl->compound_thread_local,
        .static_ = cl->compound_static,
        .thread_local_ = cl->compound_thread_local,
        .constexpr_ = cl->compound_constexpr,
        .alignment = cl->compound_alignment,
        .initializer = cl,
    };
    err = cc_scope_insert_var(cc_allocator(p), cc_scope_declarations(p->current), nil_atom, anon);
    if(err) return err;
    cl->kind = CC_EXPR_INIT_LIST; // demote to plain init list for the assignment RHS
    CcExpr* var_ref = cc_make_expr(p, CC_EXPR_VARIABLE, loc, type, 0);
    if(!var_ref) return CC_OOM_ERROR;
    var_ref->is_lvalue = 1;
    var_ref->var = anon;
    if(anon->static_ || anon->thread_local_ || (anon->constexpr_ && !anon->automatic)){
        anon->interp_preinit = 1;
        *out = var_ref;
        return 0;
    }
    CcExpr* assign = cc_binary_expr(p, CC_EXPR_ASSIGN, loc, type, var_ref, cl);
    if(!assign) return CC_OOM_ERROR;
    CcExpr* var_ref2 = cc_make_expr(p, CC_EXPR_VARIABLE, loc, type, 0);
    if(!var_ref2) return CC_OOM_ERROR;
    var_ref2->is_lvalue = 1;
    var_ref2->var = anon;
    var_ref2->compound_register = cl->compound_register;
    CcExpr* comma = cc_binary_expr(p, CC_EXPR_COMMA, loc, type, assign, var_ref2);
    if(!comma) return CC_OOM_ERROR;
    comma->is_lvalue = var_ref2->is_lvalue;
    *out = comma;
    return 0;
}

static
int
cc_parse_compound_literal(CcParser* p, CcParseFlags flags, CcQualType type, CcDeclBase declaration, CcExpr*_Nullable*_Nonnull out){
    int err = cc_check_decl_alignment(p, &declaration, type, 0);
    if(err) return err;
    CcSpecifier storage = declaration.spec;
    CcParseFlags init_flags = flags;
    if(storage.sp_constexpr) init_flags.vc = CC_CONSTEXPR_VALUE;
    else if(storage.sp_static || storage.sp_thread_local) init_flags.vc = CC_LINKTIME_VALUE;
    CcExpr* result;
    err = cc_parse_init_list(p, init_flags, &result, type);
    if(err) return err;
    result->kind = CC_EXPR_COMPOUND_LITERAL;
    result->compound_static = storage.sp_static;
    result->compound_thread_local = storage.sp_thread_local;
    result->compound_constexpr = storage.sp_constexpr;
    result->compound_register = storage.sp_register;
    result->compound_alignment = declaration.alignment;
    result->defer_compound_storage = flags.parsing_default_arg && !storage.sp_static && !storage.sp_thread_local;
    if(storage.sp_static || storage.sp_thread_local || storage.sp_constexpr){
        err = cc_check_linktime_expr(p, result, 0, 0);
        if(err == CC_NOT_CONSTANT_ERROR)
            return cc_error(p, result->loc, "compound literal initializer requires a link-time constant");
        if(err) return err;
    }
    // Persistent objects must have storage even when used only as values.
    if(storage.sp_static || storage.sp_thread_local){
        err = cc_desugar_compound_literal(p, result, &result);
        if(err) return err;
    }
    *out = result;
    return 0;
}

static
int
cc_wrap_to_desugared_compound_literal(CcParser* p, CcParseFlags flags, CcExpr* operand, CcExpr*_Nullable*_Nonnull out){
    CcQualType type = operand->type;
    SrcLoc loc = operand->loc;
    int err;
    if(flags.parsing_default_arg){
        CcInitList* list = Allocator_zalloc_extra(cc_allocator(p), 1, sizeof *list->entries, sizeof *list, _Alignof(CcInitList));
        if(!list) return CC_OOM_ERROR;
        CcExpr* literal = cc_make_expr(p, CC_EXPR_COMPOUND_LITERAL, loc, type, 0);
        if(!literal){
            Allocator_free_extra(cc_allocator(p), list, 1, sizeof *list->entries, sizeof *list, _Alignof(CcInitList));
            return CC_OOM_ERROR;
        }
        list->loc = loc;
        list->count = 1;
        list->entries[0].value = operand;
        literal->init_list = list;
        literal->defer_compound_storage = 1;
        literal->is_lvalue = 1;
        *out = literal;
        return 0;
    }
    CcVariable* anon = Allocator_zalloc(cc_allocator(p), 1, sizeof *anon, _Alignof(CcVariable));
    if(!anon) return CC_OOM_ERROR;
    *anon = (CcVariable){
        .name = nil_atom,
        .loc = loc,
        .type = type,
        .automatic = p->current_func != NULL,
        .initializer = operand,
    };
    err = cc_scope_insert_var(cc_allocator(p), cc_scope_declarations(p->current), nil_atom, anon);
    if(err) return err;
    CcExpr* var_ref = cc_make_expr(p, CC_EXPR_VARIABLE, loc, type, 0);
    if(!var_ref) return CC_OOM_ERROR;
    var_ref->is_lvalue = 1;
    var_ref->var = anon;
    CcExpr* assign = cc_binary_expr(p, CC_EXPR_ASSIGN, loc, type, var_ref, operand);
    if(!assign) return CC_OOM_ERROR;
    CcExpr* var_ref2 = cc_make_expr(p, CC_EXPR_VARIABLE, loc, type, 0);
    if(!var_ref2) return CC_OOM_ERROR;
    var_ref2->is_lvalue = 1;
    var_ref2->var = anon;
    CcExpr* comma = cc_binary_expr(p, CC_EXPR_COMMA, loc, type, assign, var_ref2);
    if(!comma) return CC_OOM_ERROR;
    comma->is_lvalue = var_ref2->is_lvalue;
    *out = comma;
    return 0;
}

static
int
cc_check_cast(CcParser* p, CcQualType from, CcQualType to, SrcLoc loc){
    const char* failure;
    if(cc_check_cast_types(cc_target(p), from, to, &failure))
        return cc_types_error(p, loc, from, to, "%s", failure);
    return 0;
}

static
_Bool
cc_symbol_has_linkage(const CcSymbol *sym){
    switch(sym->kind){
        DRP_CASES_EXHAUSTED;
        case CC_SYM_VAR:  return sym->var->has_linkage;
        case CC_SYM_FUNC: return sym->func->has_linkage;
        case CC_SYM_TYPEDEF:
        case CC_SYM_ENUMERATOR:
            return 0;
    }
}

static
_Bool
cc_lookup_visible_linked_symbol(CcScope *scope, CcScope* _Nullable last, Atom name, CcSymbol *out){
    for(; scope; scope = scope->parent){
        CcSymbol sym;
        if(cc_scope_lookup_symbol(scope, name, CC_SCOPE_NO_WALK, &sym)){
            if(!cc_symbol_has_linkage(&sym)) return 0;
            *out = sym;
            return 1;
        }
        if(scope == last) break;
    }
    return 0;
}

static
_Bool
cc_lookup_external_entity(CcParser *p, Atom name, _Bool explicit_extern, CcSymbol *out){
    CcSymbol file_scoped;
    if(cc_scope_lookup_symbol(&p->module->scope, name, CC_SCOPE_NO_WALK, &file_scoped)
    && cc_symbol_has_linkage(&file_scoped)
    && !(file_scoped.kind == CC_SYM_VAR ? file_scoped.var->static_ : file_scoped.func->static_)){
        *out = file_scoped;
        return 1;
    }
    CcSymbol *entry = AM_get(&p->module->hidden_externs, name);
    if(entry && !cc_symbol_has_linkage(entry))
        entry = NULL;
    if(explicit_extern && !entry)
        entry = AM_get(&p->extern_symbols, name);
    if(!entry || !cc_symbol_has_linkage(entry))
        return 0;
    *out = *entry;
    return 1;
}

static
int
cc_insert_extern_symbol(CcParser *p, AtomMap(CcSymbol) *symbols, Atom name, CcSymbol sym){
    CcSymbol *existing = AM_get(symbols, name);
    if(existing && cc_symbol_has_linkage(existing)){
        if(existing->kind == sym.kind && existing->ptr == sym.ptr)
            return 0;
        SrcLoc loc = sym.kind == CC_SYM_VAR ? sym.var->loc : sym.func->loc;
        return cc_error(p, loc, "conflicting extern binding for '%s'", name->data);
    }
    CcSymbol *entry = Allocator_zalloc(cc_allocator(p), 1, sizeof *entry, _Alignof(CcSymbol));
    if(!entry) return CC_OOM_ERROR;
    *entry = sym;
    int err = AM_put(symbols, cc_allocator(p), name, entry);
    if(err){
        Allocator_free(cc_allocator(p), entry, 1, sizeof *entry, _Alignof(CcSymbol));
        return CC_OOM_ERROR;
    }
    return 0;
}

static
int
cc_record_extern(CcParser *p, Atom name, CcSymbol sym, _Bool explicit_extern){
    CcSymbol file_scoped;
    if(cc_scope_lookup_symbol(&p->module->scope, name, CC_SCOPE_NO_WALK, &file_scoped)
    && file_scoped.kind == sym.kind
    && file_scoped.ptr == sym.ptr)
        AM_del(&p->module->hidden_externs, name);
    else {
        int err = cc_insert_extern_symbol(p, &p->module->hidden_externs, name, sym);
        if(err) return err;
    }
    if(!explicit_extern) return 0;
    return cc_insert_extern_symbol(p, &p->extern_symbols, name, sym);
}

static
int
cc_check_func_compat(CcParser* p, CcFunc* existing, const CcDeclBase* declbase, CcQualType new_ftype, _Bool definition, SrcLoc loc){
    CcFunction* new_type = ccqt_as_function(new_ftype);
    CcFunction* old_type = existing->type;
    if(!existing->static_ && declbase->spec.sp_static)
        return cc_error(p, loc, "static declaration of '%.*s' follows non-static declaration", existing->name->length, existing->name->data);
    if(!cc_types_compatible(old_type->return_type, new_type->return_type))
        return cc_types_error(p, loc, old_type->return_type, new_type->return_type, "conflicting return type for '%.*s'", existing->name->length, existing->name->data);
    if(old_type->no_prototype || new_type->no_prototype){
        CcFunction* prototype = new_type->no_prototype ? old_type : new_type;
        if(prototype->is_variadic)
            return cc_types_error(p, loc, (CcQualType){.bits=(uintptr_t)old_type}, new_ftype, "conflicting variadic specifier for '%.*s'", existing->name->length, existing->name->data);
        // An empty parameter list in a definition specifies zero parameters.
        if(((existing->defined && old_type->no_prototype) || (definition && new_type->no_prototype))
            && prototype->param_count)
            return cc_types_error(p, loc, (CcQualType){.bits=(uintptr_t)old_type}, new_ftype, "conflicting number of parameters for '%.*s'", existing->name->length, existing->name->data);
        for(uint32_t i = 0; i < prototype->param_count; i++){
            if(cc_requires_default_promotion(prototype->params[i]))
                return cc_types_error(p, loc, (CcQualType){.bits=(uintptr_t)old_type}, new_ftype, "conflicting type for parameter %u of '%.*s'", i+1, existing->name->length, existing->name->data);
        }
        return 0;
    }
    if(old_type->is_variadic != new_type->is_variadic)
        return cc_types_error(p, loc, (CcQualType){.bits=(uintptr_t)old_type}, new_ftype, "conflicting variadic specifier for '%.*s'", existing->name->length, existing->name->data);
    if(old_type->param_count != new_type->param_count)
        return cc_types_error(p, loc, (CcQualType){.bits=(uintptr_t)old_type}, new_ftype, "conflicting number of parameters for '%.*s'", existing->name->length, existing->name->data);
    for(uint32_t i = 0; i < old_type->param_count; i++){
        CcQualType old_param = cc_parameter_value_type(old_type->params[i]);
        CcQualType new_param = cc_parameter_value_type(new_type->params[i]);
        if(!cc_types_compatible(old_param, new_param))
            return cc_types_error(p, loc, old_param, new_param, "conflicting type for parameter %u of '%.*s'", i + 1, existing->name->length, existing->name->data);
    }
    return 0;
}

typedef struct CcCompositePair CcCompositePair;
struct CcCompositePair {
    CcQualType a, b, composite;
    const CcCompositePair* _Nullable previous;
};


static
int
cc_merge_decl_types(CcParser* p, CcQualType old, CcQualType new_, CcQualType* out, const CcCompositePair* _Nullable pairs){
    *out = new_;
    if(old.bits == new_.bits) return 0;
    if(ccqt_raw_kind(old) == CC_ALIGNED || ccqt_raw_kind(new_) == CC_ALIGNED){
        CcQualType merged;
        int err = cc_merge_decl_types(p, ccqt_unaligned(old), ccqt_unaligned(new_), &merged, pairs);
        if(err) return err;
        uint32_t old_alignment = ccqt_explicit_alignment(old);
        uint32_t new_alignment = ccqt_explicit_alignment(new_);
        uint32_t alignment = old_alignment > new_alignment ? old_alignment : new_alignment;
        CcAligned* aligned = cc_intern_aligned(&p->type_cache, cc_allocator(p), merged, alignment);
        if(!aligned) return CC_OOM_ERROR;
        *out = (CcQualType){.bits=(uintptr_t)aligned | merged.quals};
        return 0;
    }
    CcTypeKind kind = ccqt_kind(old);
    if((kind == CC_ENUM && ccqt_kind(new_) == CC_BASIC)
        || (kind == CC_BASIC && ccqt_kind(new_) == CC_ENUM)){
        if(!cc_types_compatible(old, new_)) return CC_SYNTAX_ERROR;
        *out = kind == CC_ENUM ? old : new_;
        return 0;
    }
    if(ccqt_is_array(old) && ccqt_is_array(new_)){
        if(kind == CC_ARRAY && ccqt_kind(new_) == CC_ARRAY){
            CcArray* a = ccqt_as_array(old);
            CcArray* b = ccqt_as_array(new_);
            if(!a->is_incomplete && !b->is_incomplete && a->length != b->length)
                return CC_SYNTAX_ERROR;
        }
        CcQualType element;
        int err = cc_merge_decl_types(p, ccqt_qualified_array_element(old), ccqt_qualified_array_element(new_), &element, pairs);
        if(err) return err;
        CcQualType bound = cc_array_bound_information(old) > cc_array_bound_information(new_) ? old : new_;
        if(ccqt_kind(bound) == CC_ARRAY){
            CcArray* source = ccqt_as_array(bound);
            CcArray* array = cc_intern_array(&p->type_cache, cc_allocator(p), element, source->length, source->is_incomplete);
            if(!array) return CC_OOM_ERROR;
            *out = (CcQualType){.bits=(uintptr_t)array};
        }
        else {
            CcVla* vla = cc_intern_vla(&p->type_cache, cc_allocator(p), ccqt_as_vla(bound), element);
            if(!vla) return CC_OOM_ERROR;
            *out = (CcQualType){.bits=(uintptr_t)vla};
        }
        return 0;
    }
    if(kind != ccqt_kind(new_) || old.quals != new_.quals)
        return CC_SYNTAX_ERROR;
    switch(kind){
        case CC_VLA: case CC_ARRAY: cc_unsafe_unreachable(); // Handled above.
        case CC_VECTOR: return cc_types_compatible(old, new_) ? 0 : CC_SYNTAX_ERROR;
        case CC_ALIGNED: cc_unsafe_unreachable();
        case CC_POINTER:
        case CC_BLOCK_POINTER: {
            CcPointer* a = ccqt_as_ptr(old);
            CcPointer* b = ccqt_as_ptr(new_);
            if(a->restrict_ != b->restrict_) return CC_SYNTAX_ERROR;
            CcQualType pointee;
            int err = cc_merge_decl_types(p, a->pointee, b->pointee, &pointee, pairs);
            if(err) return err;
            CcPointer* ptr = cc_intern_pointer(&p->type_cache, cc_allocator(p), pointee, b->restrict_, kind == CC_BLOCK_POINTER);
            if(!ptr) return CC_OOM_ERROR;
            *out = (CcQualType){.bits = (uintptr_t)ptr | new_.quals};
            return 0;
        }
        case CC_SLICE: {
            CcSlice* a = ccqt_as_slice(old);
            CcSlice* b = ccqt_as_slice(new_);
            if(a->restrict_ != b->restrict_) return CC_SYNTAX_ERROR;
            CcQualType pointee;
            int err = cc_merge_decl_types(p, a->pointee, b->pointee, &pointee, pairs);
            if(err) return err;
            CcSlice* slice = cc_intern_slice(&p->type_cache, cc_allocator(p), pointee, b->restrict_);
            if(!slice) return CC_OOM_ERROR;
            *out = (CcQualType){.bits = (uintptr_t)slice | new_.quals};
            return 0;
        }
        case CC_FUNCTION: {
            CcFunction* a = ccqt_as_function(old);
            CcFunction* b = ccqt_as_function(new_);
            CcQualType ret;
            int err = cc_merge_decl_types(p, a->return_type, b->return_type, &ret, pairs);
            if(err) return err;
            CcFunction* proto = b->no_prototype ? a : b;
            if(a->no_prototype || b->no_prototype){
                if(proto->is_variadic) return CC_SYNTAX_ERROR;
                for(uint32_t i = 0; i < proto->param_count; i++){
                    if(cc_requires_default_promotion(proto->params[i]))
                        return CC_SYNTAX_ERROR;
                }
            }
            else if(a->param_count != b->param_count || a->is_variadic != b->is_variadic)
                return CC_SYNTAX_ERROR;
            Marray(CcQualType) params = {0};
            for(uint32_t i = 0; i < proto->param_count; i++){
                CcQualType pt = proto->params[i];
                pt.quals = 0;
                if(!a->no_prototype && !b->no_prototype){
                    CcQualType ap = a->params[i], bp = b->params[i];
                    ap.quals = bp.quals = 0;
                    err = cc_merge_decl_types(p, ap, bp, &pt, pairs);
                    if(err) break;
                }
                err = ma_push(CcQualType)(&params, cc_scratch_allocator(p), pt);
                if(err){
                    err = CC_OOM_ERROR;
                    break;
                }
            }
            if(!err){
                CcFunction* f = cc_intern_function(&p->type_cache, cc_allocator(p), ret, params.data, proto->param_count, proto->param_count, proto->is_variadic, proto->no_prototype);
                if(!f) err = CC_OOM_ERROR;
                else *out = (CcQualType){.bits = (uintptr_t)f | new_.quals};
            }
            ma_cleanup(CcQualType)(&params, cc_scratch_allocator(p));
            return err;
        }
        case CC_STRUCT:
        case CC_UNION: {
            for(const CcCompositePair* prev = pairs; prev; prev = prev->previous){
                if((prev->a.unqual == old.unqual && prev->b.unqual == new_.unqual)
                    || (prev->a.unqual == new_.unqual && prev->b.unqual == old.unqual)){
                    *out = prev->composite;
                    out->quals = new_.quals;
                    return 0;
                }
            }
            int err;
            CcStruct* a = ccqt_as_struct(old);
            CcStruct* b = ccqt_as_struct(new_);
            CcStruct* record = Allocator_alloc(cc_allocator(p), 1, sizeof *record, _Alignof(CcStruct));
            if(!record) return CC_OOM_ERROR;
            *record = *b;
            record->methods.capacity = record->methods.count;
            record->methods.data = NULL;
            if(b->methods.count){
                record->methods.data = Allocator_dupe(cc_allocator(p), b->methods.data, b->methods.count, sizeof *b->methods.data, _Alignof(CcField));
                if(!record->methods.data) return CC_OOM_ERROR;
            }
            record->fields = Allocator_alloc(cc_allocator(p), b->field_count, sizeof *record->fields, _Alignof(CcField));
            if(b->field_count && !record->fields) return CC_OOM_ERROR;
            *out = (CcQualType){.bits = (uintptr_t)record | new_.quals};
            CcCompositePair pair = {.a=old, .b=new_, .composite=*out, .previous=pairs};
            for(uint32_t i = 0; i < b->field_count; i++){
                record->fields[i] = b->fields[i];
                err = cc_merge_decl_types(p, a->fields[i].type, b->fields[i].type, &record->fields[i].type, &pair);
                if(err) return err;
            }
            return 0;
        }
        case CC_BASIC:
            return CC_SYNTAX_ERROR;
        case CC_ENUM:
            if(old.unqual < new_.unqual) *out = old;
            return 0;
        DRP_CASES_EXHAUSTED;
    }
}

static
int
cc_merge_compatible_decl_types(CcParser* p, CcQualType old, CcQualType new_, CcQualType* out){
    if(!cc_types_compatible(old, new_)) return CC_SYNTAX_ERROR;
    if(old.bits == new_.bits){ *out = old; return 0; }
    return cc_merge_decl_types(p, old, new_, out, NULL);
}

static
int
cc_merge_qualified_types(CcParser* p, CcQualType a, CcQualType b, CcQualType* out){
    uintptr_t quals = cc_object_quals(a) | cc_object_quals(b);
    int err = cc_qualify_object_type(p, a, quals, &a);
    if(err) return err;
    err = cc_qualify_object_type(p, b, quals, &b);
    if(err) return err;
    return cc_merge_compatible_decl_types(p, a, b, out);
}

static
int
cc_merge_func_decl_type(CcParser* p, CcFunction* old, CcQualType* type){
    CcQualType composite;
    int err = cc_merge_compatible_decl_types(p, (CcQualType){.bits=(uintptr_t)old}, *type, &composite);
    if(err) return err;
    *type = composite;
    return 0;
}

static
int
cc_check_var_type(CcParser* p, CcVariable* var, CcQualType* type, SrcLoc loc){
    CcQualType composite;
    int err = cc_merge_compatible_decl_types(p, var->type, *type, &composite);
    if(!err){
        *type = composite;
        return 0;
    }
    if(err != CC_SYNTAX_ERROR) return err;
    MStringBuilder* sb;
    if(ccqt_kind(var->type) == CC_ARRAY && ccqt_kind(*type) == CC_ARRAY
        && !ccqt_as_array(var->type)->is_incomplete && !ccqt_as_array(*type)->is_incomplete
        && ccqt_as_array(var->type)->length != ccqt_as_array(*type)->length){
        sb = cc_start_error(p, loc, "conflicting array length for '%s': %zu; previous declaration has length %zu ('",
            var->name->data, ccqt_as_array(*type)->length, ccqt_as_array(var->type)->length);
        cc_print_type(sb, *type);
        msb_write_literal(sb, "' vs '");
        cc_print_type(sb, var->type);
        msb_write_literal(sb, "')");
    }
    else {
        sb = cc_start_error(p, loc, "conflicting type for '%s': '", var->name->data);
        cc_print_type(sb, *type);
        msb_write_literal(sb, "'; previous declaration has type '");
        cc_print_type(sb, var->type);
        msb_write_char(sb, '\'');
    }
    return cc_finish_error(p, loc);
}

static
int
cc_sizeof_as_expr(CcParser* p, CcQualType t, SrcLoc loc, CcExpr* _Nullable* _Nonnull out){
    const CcTargetConfig* tgt = cc_target(p);
    CcQualType size_type = ccqt_basic(tgt->size_type);
    switch(ccqt_kind(t)){
        case CC_VLA: {
            CcVla* vla = ccqt_as_vla(t);
            CcExpr *bound, *element;
            int err = cc_clone_expr(p, vla->bound, 0, loc, 0, &bound);
            if(err) return err;
            err = cc_sizeof_as_expr(p, vla->element, loc, &element);
            if(err) return err;
            CcExpr* size = cc_binary_expr(p, CC_EXPR_MUL, loc, size_type, bound, element);
            if(!size) return CC_OOM_ERROR;
            size->binary.checked_size = 1;
            size->binary.element_alignment = ccqt_explicit_alignment(vla->element);
            *out = size;
            return 0;
        }
        case CC_ALIGNED: cc_unsafe_unreachable();
        case CC_BASIC:{
            if(ccqt_basic_kind(t) >= CCBT_COUNT)
                return cc_error(p, loc, "sizeof applied to invalid kind");
            CcExpr* node = cc_value_expr(p, loc, size_type);
            if(!node) return CC_OOM_ERROR;
            node->uinteger = tgt->sizeof_[ccqt_basic_kind(t)];
            *out = node;
            return 0;
        }
        case CC_BLOCK_POINTER:
        case CC_POINTER:{
            CcExpr* node = cc_value_expr(p, loc, size_type);
            if(!node) return CC_OOM_ERROR;
            node->uinteger = tgt->sizeof_[CCBT_nullptr_t];
            *out = node;
            return 0;
        }
        case CC_SLICE:{
            CcExpr* node = cc_value_expr(p, loc, size_type);
            if(!node) return CC_OOM_ERROR;
            node->uinteger = 2*tgt->sizeof_[CCBT_nullptr_t]; // struct {uintptr len; T* data;}
            *out = node;
            return 0;
        }
        case CC_VECTOR: {
            CcVector* arr = ccqt_as_vector(t);
            CcExpr* node = cc_value_expr(p, loc, size_type);
            if(!node) return CC_OOM_ERROR;
            node->uinteger = arr->vector_size;
            *out = node;
            return 0;
        }
        case CC_ARRAY: {
            CcArray* arr = ccqt_as_array(t);
            if(arr->is_incomplete)
                return cc_type_error(p, loc, t, "sizeof applied to incomplete array type");
            CcExpr* elem_size;
            int err = cc_sizeof_as_expr(p, arr->element, loc, &elem_size);
            if(err) return err;
            if(elem_size->kind == CC_EXPR_VALUE){
                uint32_t size;
                err = cc_sizeof_as_uint(p, t, loc, &size);
                if(err){
                    cc_release_expr(p, elem_size);
                    return err;
                }
                elem_size->uinteger = size;
                *out = elem_size;
                return 0;
            }
            CcExpr* len = cc_value_expr(p, loc, size_type);
            if(!len) return CC_OOM_ERROR;
            len->uinteger = (uint64_t)arr->length;
            CcExpr* mul = cc_binary_expr(p, CC_EXPR_MUL, loc, size_type, len, elem_size);
            if(!mul) return CC_OOM_ERROR;
            mul->binary.checked_size = 1;
            mul->binary.element_alignment = ccqt_explicit_alignment(arr->element);
            *out = mul;
            return 0;
        }
        case CC_STRUCT:{
            CcStruct* s = ccqt_as_struct(t);
            if(s->is_incomplete)
                return cc_type_error(p, loc, t, "sizeof applied to incomplete struct type");
            CcExpr* node = cc_value_expr(p, loc, size_type);
            if(!node) return CC_OOM_ERROR;
            node->uinteger = s->size;
            *out = node;
            return 0;
        }
        case CC_UNION:{
            CcUnion* u = ccqt_as_union(t);
            if(u->is_incomplete)
                return cc_type_error(p, loc, t, "sizeof applied to incomplete union type");
            CcExpr* node = cc_value_expr(p, loc, size_type);
            if(!node) return CC_OOM_ERROR;
            node->uinteger = u->size;
            *out = node;
            return 0;
        }
        case CC_ENUM:{
            CcEnum* e = ccqt_as_enum(t);
            if(e->is_incomplete) return cc_type_error(p, loc, t, "sizeof applied to incomplete enum type");
            return cc_sizeof_as_expr(p, e->underlying, loc, out);
        }
        case CC_FUNCTION:
            return cc_type_error(p, loc, t, "sizeof applied to function type");
    }
    cc_unsafe_unreachable();
}

static
int
cc_expr_sizeof_as_uint(CcParser* p, const CcExpr* e, uint32_t* out){
    if(e->kind == CC_EXPR_BLOB){
        *out = (uint32_t)e->blob.n_bytes;
        return 0;
    }
    return cc_sizeof_as_uint(p, e->type, e->loc, out);
}

static
int
cc_alignof_as_expr(CcParser* p, CcQualType t, SrcLoc loc, CcExpr* _Nullable* _Nonnull out){
    uint32_t alignment;
    int err = cc_alignof_as_uint(p, t, loc, &alignment);
    if(err) return err;
    CcExpr* node = cc_value_expr(p, loc, ccqt_basic(cc_target(p)->size_type));
    if(!node) return CC_OOM_ERROR;
    node->uinteger = alignment;
    *out = node;
    return 0;
}

static
int
cc_sizeof_as_uint(CcParser* p, CcQualType t, SrcLoc loc, uint32_t* out){
    CcQualType failure_type;
    int err = cc_type_sizeof_complete(cc_target(p), t, out, &failure_type);
    if(!err) return 0;
    if(err == CC_OVERFLOW_ERROR)
        return cc_error(p, loc, "object size exceeds 32-bit layout limit");
    switch(ccqt_kind(failure_type)){
        case CC_VLA:
            return cc_type_error(p, loc, failure_type, "variable length type requires a runtime size");
        case CC_ARRAY:
        case CC_STRUCT:
        case CC_UNION:
            return cc_type_error(p, loc, failure_type, "Taking sizeof of an incomplete type");
        case CC_ENUM:
            return cc_type_error(p, loc, failure_type, "sizeof applied to incomplete enum type");
        case CC_FUNCTION:
            return cc_type_error(p, loc, failure_type, "Taking sizeof of a function type (not function pointer type)");
        case CC_BASIC:
            return ((void)cc_error(p, loc, "basic kind out of bounds"), CC_UNREACHABLE_ERROR);
        default:
            cc_unsafe_unreachable();
    }
}
int
cc_alignof_as_uint(CcParser* p, CcQualType t, SrcLoc loc, uint32_t* out){
    if(t.is_atomic){
        t.is_atomic = 0;
        int err = cc_alignof_as_uint(p, t, loc, out);
        if(err) return err;
        uint32_t size;
        err = cc_sizeof_as_uint(p, t, loc, &size);
        if(err) return err;
        // The supported native atomic widths require size alignment, even
        // when an aggregate's non-atomic version has weaker alignment.
        if(size && size <= 16 && !(size & (size - 1)) && size > *out)
            *out = size;
        return 0;
    }
    if(ccqt_raw_kind(t) == CC_ALIGNED){
        uint32_t natural;
        int err = cc_alignof_as_uint(p, ccqt_unaligned(t), loc, &natural);
        if(err) return err;
        *out = ccqt_as_aligned(t)->alignment;
        return 0;
    }
    const CcTargetConfig* tgt = cc_target(p);
    switch(ccqt_kind(t)){
        case CC_VLA: return cc_alignof_as_uint(p, ccqt_as_vla(t)->element, loc, out);
        case CC_ALIGNED: cc_unsafe_unreachable();
        case CC_BASIC:
            if(ccqt_basic_kind(t) >= CCBT_COUNT)
                return ((void)cc_error(p, loc, "basic kind out of bounds"), CC_UNREACHABLE_ERROR);
            *out = tgt->alignof_[ccqt_basic_kind(t)];
            return 0;
        case CC_BLOCK_POINTER:
        case CC_POINTER:
        case CC_SLICE:
            *out = tgt->alignof_[CCBT_nullptr_t];
            return 0;
        case CC_VECTOR: {
            CcVector* arr = ccqt_as_vector(t);
            *out = arr->vector_size > tgt->max_align ? tgt->max_align:arr->vector_size;
            return 0;
        }
        case CC_ARRAY: {
            CcArray* arr = ccqt_as_array(t);
            return cc_alignof_as_uint(p, arr->element, loc, out);
        }
        case CC_STRUCT: {
            CcStruct* s = ccqt_as_struct(t);
            if(s->is_incomplete)
                return cc_type_error(p, loc, t, "alignof applied to incomplete struct type");
            *out = s->alignment;
            return 0;
        }
        case CC_UNION: {
            CcUnion* u = ccqt_as_union(t);
            if(u->is_incomplete)
                return cc_type_error(p, loc, t, "alignof applied to incomplete union type");
            *out = u->alignment;
            return 0;
        }
        case CC_ENUM: {
            CcEnum* e = ccqt_as_enum(t);
            if(e->is_incomplete) return cc_type_error(p, loc, t, "alignof applied to incomplete enum type");
            return cc_alignof_as_uint(p, e->underlying, loc, out);
        }
        case CC_FUNCTION:
            return cc_type_error(p, loc, t, "alignof applied to function type");
    }
    cc_unsafe_unreachable();
}

static
int
cc_type_alignment(CcParser* p, CcQualType type, SrcLoc loc, uint32_t* out){
    uint32_t alignment;
    int err = cc_alignof_as_uint(p, type, loc, &alignment);
    if(err) return err;
    *out = alignment ? (uint32_t)(31 - clz_32(alignment)) : 0;
    return 0;
}

static
uint32_t
cc_offset_alignment(uint32_t align_log2, uint64_t offset){
    if(!offset) return align_log2;
    uint32_t offset_log2 = (uint32_t)ctz_64(offset);
    return offset_log2 < align_log2 ? offset_log2 : align_log2;
}

static
int
cc_index_alignment(CcParser* p, CcExpr* index, uint32_t stride, uint32_t* out){
    int64_t value;
    int err = cc_eval_integer(&(CcEvalCtx){.parser=p}, index, &value);
    if(err && err != CC_NOT_CONSTANT_ERROR) return err;
    uint64_t offset = err ? stride : (uint64_t)value * stride;
    *out = cc_offset_alignment(*out, offset);
    return 0;
}

static
int
cc_pointer_type_alignment(CcParser* p, CcQualType type, SrcLoc loc, uint32_t source_alignment, uint32_t* out){
    if(ccqt_kind(type) == CC_POINTER){
        CcQualType pointee = ccqt_as_ptr(type)->pointee;
        if((ccqt_kind(pointee) == CC_STRUCT && ccqt_as_struct(pointee)->is_incomplete)
            || (ccqt_kind(pointee) == CC_UNION && ccqt_as_union(pointee)->is_incomplete)
            || (ccqt_kind(pointee) == CC_ENUM && ccqt_as_enum(pointee)->is_incomplete)){
            *out = source_alignment;
            return 0;
        }
        if(ccqt_kind(pointee) != CC_FUNCTION && !ccqt_bt_eq(pointee, CCBT_void)){
            int err = cc_type_alignment(p, pointee, loc, out);
            if(err) return err;
            if(source_alignment > *out) *out = source_alignment;
            return 0;
        }
    }
    *out = source_alignment;
    return 0;
}

static
int
cc_pointer_alignment(CcParser* p, const CcExpr* e, uint32_t* out){
    if(ccqt_kind(e->type) == CC_VLA)
        return cc_type_alignment(p, e->type, e->loc, out);
    uint32_t source_alignment = 0;
    if((ccqt_is_fixed_sequence(e->type) || ccqt_kind(e->type) == CC_VLA))
        return cc_lvalue_alignment(p, e, out);
    switch((uint32_t)e->kind){
        case CC_EXPR_ADDR:
            return cc_lvalue_alignment(p, e->lhs, out);
        case CC_EXPR_COMMA:
            return cc_pointer_alignment(p, e->values[0], out);
        case CC_EXPR_ASSIGN:
            return cc_pointer_alignment(p, e->values[0], out);
        case CC_EXPR_OBJECT_VIEW:
            return cc_pointer_alignment(p, e->lhs, out);
        case CC_EXPR_BLOB:
        case CC_EXPR_VALUE:
            *out = 0;
            return 0;
        case CC_EXPR_CAST:{
            // Trust the programmer
            if(ccqt_kind(e->lhs->type) == CC_POINTER || (ccqt_is_fixed_sequence(e->lhs->type) || ccqt_kind(e->lhs->type) == CC_VLA)){
                int err = cc_pointer_alignment(p, e->lhs, &source_alignment);
                if(err) return err;
            }
            break;
        }
        case CC_EXPR_TERNARY:{
            uint32_t other;
            int err = cc_pointer_alignment(p, e->values[0], out);
            if(err) return err;
            err = cc_pointer_alignment(p, e->values[1], &other);
            if(err) return err;
            if(other < *out) *out = other;
            return 0;
        }
        case CC_EXPR_ADD:
        case CC_EXPR_SUB:{
            const CcExpr* base = e->lhs;
            CcExpr* index = e->values[0];
            if(ccqt_kind(base->type) != CC_POINTER){ base = e->values[0]; index = e->lhs; }
            if(ccqt_kind(ccqt_as_ptr(base->type)->pointee) == CC_VLA)
                return cc_type_alignment(p, ccqt_as_ptr(base->type)->pointee, e->loc, out);
            int err = cc_pointer_alignment(p, base, out);
            if(err) return err;
            uint32_t stride;
            err = cc_sizeof_as_uint(p, ccqt_as_ptr(base->type)->pointee, e->loc, &stride);
            if(err) return err;
            return cc_index_alignment(p, index, stride, out);
        }
    }
    return cc_pointer_type_alignment(p, e->type, e->loc, source_alignment, out);
}

static
int
cc_lvalue_alignment(CcParser* p, const CcExpr* e, uint32_t* out){
    if(ccqt_kind(e->type) == CC_VLA)
        return cc_type_alignment(p, e->type, e->loc, out);
    int err;
    switch((uint32_t)e->kind){
        case CC_EXPR_COMMA:
            return cc_lvalue_alignment(p, e->values[0], out);
        case CC_EXPR_DEREF:
            return cc_pointer_alignment(p, e->lhs, out);
        case CC_EXPR_DOT:
        case CC_EXPR_ARROW:
            err = e->kind == CC_EXPR_DOT
                ? cc_lvalue_alignment(p, e->values[0], out)
                : cc_pointer_alignment(p, e->values[0], out);
            if(err) return err;
            uint64_t field_offset;
            err = cc_expr_field_offset(cc_target(p), e, &field_offset);
            if(err) return err;
            *out = cc_offset_alignment(*out, field_offset);
            return 0;
        case CC_EXPR_SUBSCRIPT:{
            const CcExpr* base = e->lhs;
            if((ccqt_is_fixed_sequence(base->type) || ccqt_kind(base->type) == CC_VLA))
                err = cc_lvalue_alignment(p, base, out);
            else if(ccqt_kind(base->type) == CC_POINTER)
                err = cc_pointer_alignment(p, base, out);
            else
                err = cc_type_alignment(p, e->type, e->loc, out);
            if(err) return err;
            uint32_t stride;
            err = cc_sizeof_as_uint(p, e->type, e->loc, &stride);
            if(err) return err;
            return cc_index_alignment(p, e->values[0], stride, out);
        }
    }
    err = cc_type_alignment(p, e->type, e->loc, out);
    if(err) return err;
    if(e->kind == CC_EXPR_VARIABLE){
        uint32_t alignment = e->var->alignment;
        uint32_t exponent = alignment ? (uint32_t)(31 - clz_32(alignment)) : 0;
        if(exponent > *out) *out = exponent;
    }
    return 0;
}

static
int
cc_expr_alignment(CcParser* p, const CcExpr* e, uint32_t* out){
    while(e->kind == CC_EXPR_COMMA) e = e->values[0];
    int err = cc_alignof_as_uint(p, e->type, e->loc, out);
    if(err) return err;
    if(e->kind == CC_EXPR_VARIABLE){
        if(e->var->alignment > *out) *out = e->var->alignment;
    }
    else if(e->kind == CC_EXPR_COMPOUND_LITERAL){
        if(e->compound_alignment > *out) *out = e->compound_alignment;
    }
    else if(e->kind == CC_EXPR_DOT || e->kind == CC_EXPR_ARROW){
        CcQualType owner = cc_expr_field_owner(e);
        uint32_t available;
        if(e->kind == CC_EXPR_DOT)
            err = cc_expr_alignment(p, e->values[0], &available);
        else
            err = cc_alignof_as_uint(p, owner, e->loc, &available);
        if(err) return err;
        for(uint32_t i = 0; i < cc_field_path_count(e->field_path); i++){
            if(ccqt_kind(owner) != CC_STRUCT && ccqt_kind(owner) != CC_UNION)
                break; // Builtin pseudo-members use their type alignment.
            CcField* field = &ccqt_as_struct(owner)->fields[cc_field_path_component(e->field_path, i)];
            if(field->layout_alignment < available) available = field->layout_alignment;
            *out = available;
            owner = field->type;
        }
    }
    return 0;
}

static
int
cc_check_decl_alignment(CcParser* p, const CcDeclBase* base, CcQualType type, _Bool bitfield){
    if(!base->has_alignas) return 0;
    if(base->spec.sp_typedef)
        return cc_error(p, base->loc, "_Alignas is not allowed on typedef declarations");
    if(base->spec.sp_register)
        return cc_error(p, base->loc, "_Alignas is not allowed on register objects");
    if(ccqt_kind(type) == CC_FUNCTION)
        return cc_type_error(p, base->loc, type, "_Alignas is not allowed on functions");
    if(bitfield)
        return cc_error(p, base->loc, "_Alignas is not allowed on bitfields");
    if(!base->alignas_alignment) return 0;
    uint32_t natural;
    int err = cc_alignof_as_uint(p, type, base->loc, &natural);
    if(err) return err;
    if(base->alignas_alignment < natural)
        return cc_type_error(p, base->loc, type, "_Alignas cannot weaken natural alignment");
    return 0;
}

static
int
cc_apply_field_attributes(CcParser* p, const CcDeclBase* base, const CcAttributes* attrs, CcField* field){
    if(base->spec.sp_funcbits)
        return cc_error(p, base->loc, "function specifier is not allowed on data members");
    int err = cc_apply_vector_type(p, &field->type, attrs->vector_size, base->loc);
    if(err) return err;
    err = cc_check_derived_type(p, field->type, base->loc);
    if(err) return err;
    const char* failure;
    if(cc_check_member_type(field->type, field->is_bitfield, &failure))
        return cc_type_error(p, field->loc, field->type, "%s", failure);
    err = cc_check_decl_alignment(p, base, field->type, field->is_bitfield);
    if(err) return err;
    field->alignment = base->alignment;
    if(attrs->has_aligned && attrs->aligned > field->alignment)
        field->alignment = attrs->aligned;
    field->packed = attrs->packed;
    return 0;
}

static
_Bool
cc_binop_lookup(CcPunct punct, CcExprKind* kind, int* prec){
    static const struct {
        CcPunct punct;
        CcExprKind kind;
        int prec;
    } binop_table[] = {
        {CC_or,      CC_EXPR_LOGOR,  4},
        {CC_and,     CC_EXPR_LOGAND, 5},
        {CC_pipe,    CC_EXPR_BITOR,  6},
        {CC_xor,     CC_EXPR_BITXOR, 7},
        {CC_amp,     CC_EXPR_BITAND, 8},
        {CC_eq,      CC_EXPR_EQ,     9},
        {CC_ne,      CC_EXPR_NE,     9},
        {CC_lt,      CC_EXPR_LT,     10},
        {CC_gt,      CC_EXPR_GT,     10},
        {CC_le,      CC_EXPR_LE,     10},
        {CC_ge,      CC_EXPR_GE,     10},
        {CC_lshift,  CC_EXPR_LSHIFT, 11},
        {CC_rshift,  CC_EXPR_RSHIFT, 11},
        {CC_plus,    CC_EXPR_ADD,    12},
        {CC_minus,   CC_EXPR_SUB,    12},
        {CC_star,    CC_EXPR_MUL,    13},
        {CC_slash,   CC_EXPR_DIV,    13},
        {CC_percent, CC_EXPR_MOD,    13},
    };
    for(size_t i = 0; i < sizeof binop_table / sizeof binop_table[0]; i++){
        if(binop_table[i].punct == punct){
            *kind = binop_table[i].kind;
            *prec = binop_table[i].prec;
            return 1;
        }
    }
    return 0;
}

static
_Bool
cc_assign_lookup(CcPunct punct, CcExprKind* kind){
    switch((uint32_t)punct){
        case CC_assign:         *kind = CC_EXPR_ASSIGN;       return 1;
        case CC_plus_assign:    *kind = CC_EXPR_ADDASSIGN;    return 1;
        case CC_minus_assign:   *kind = CC_EXPR_SUBASSIGN;    return 1;
        case CC_star_assign:    *kind = CC_EXPR_MULASSIGN;    return 1;
        case CC_slash_assign:   *kind = CC_EXPR_DIVASSIGN;    return 1;
        case CC_percent_assign: *kind = CC_EXPR_MODASSIGN;    return 1;
        case CC_amp_assign:     *kind = CC_EXPR_BITANDASSIGN; return 1;
        case CC_pipe_assign:    *kind = CC_EXPR_BITORASSIGN;  return 1;
        case CC_xor_assign:     *kind = CC_EXPR_BITXORASSIGN; return 1;
        case CC_lshift_assign:  *kind = CC_EXPR_LSHIFTASSIGN; return 1;
        case CC_rshift_assign:  *kind = CC_EXPR_RSHIFTASSIGN; return 1;
        default: return 0;
    }
}

static
int
cc_parse_expr(CcParser* p, CcParseFlags flags, CcExpr* _Nullable* _Nonnull out, CcQualType expected){
    // Expected types guide braced initializers, not conversions of ordinary
    // expressions. Carry the hint even before discovering a comma or ternary;
    // their operands still undergo the usual type checks.
    CcExpr* left;
    int err = cc_parse_assignment_expr(p, flags, &left, expected);
    if(err) return err;
    return cc_parse_comma_tail(p, flags, left, out, expected);
}

static
int
cc_parse_comma_tail(CcParser* p, CcParseFlags flags, CcExpr* left, CcExpr* _Nullable* _Nonnull out, CcQualType expected){
    int err;
    for(;;){
        CcToken tok;
        err = cc_next_token(p, &tok);
        if(err) return err;
        if(tok.type == CC_PUNCTUATOR && tok.punct.punct == CC_comma){
            CcExpr* right;
            err = cc_parse_assignment_expr(p, flags, &right, expected);
            if(err) return err;
            err = cc_decay(p, &left);
            if(err) return err;
            err = cc_implicit_cast(p, left, ccqt_basic(CCBT_void), &left);
            if(err) return err;
            err = cc_decay(p, &right);
            if(err) return err;
            CcExpr* node = cc_make_expr(p, CC_EXPR_COMMA, tok.loc, right->type, 1);
            if(!node) return CC_OOM_ERROR;
            node->is_lvalue = right->is_lvalue;
            node->lhs = left;
            node->values[0] = right;
            left = node;
        }
        else {
            cc_unget(p, &tok);
            break;
        }
    }
    *out = left;
    return 0;
}

static
int
cc_parse_assignment_expr(CcParser* p, CcParseFlags flags, CcExpr* _Nullable* _Nonnull out, CcQualType expected){
    CcExpr* left;
    int err = cc_parse_ternary_expr(p, flags, &left, expected);
    if(err) return err;
    return cc_parse_assignment_tail(p, flags, left, out);
}

static
int
cc_parse_assignment_tail(CcParser* p, CcParseFlags flags, CcExpr* left, CcExpr* _Nullable* _Nonnull out){
    CcToken tok;
    int err = cc_next_token(p, &tok);
    if(err) return err;
    if(tok.type == CC_PUNCTUATOR){
        CcExprKind kind;
        if(cc_assign_lookup(tok.punct.punct, &kind)){
            if(flags.static_rebind && !cc_static_rebind_target(p, left))
                return cc_error(p, left->loc, "static for update must rebind a clause-1 variable");
            if(flags.vc > CC_RUNTIME_VALUE && !flags.static_rebind)
                return cc_error(p, tok.loc, "assignment in constant expression");
            if(left->kind == CC_EXPR_COMPOUND_LITERAL){
                err = cc_desugar_compound_literal(p, left, &left);
                if(err) return err;
            }
            if(!left->is_lvalue)
                return cc_error(p, tok.loc, "expression is not assignable");
            if(!ccqt_is_complete_object(left->type))
                return cc_type_error(p, tok.loc, left->type, "assignment requires a complete object type");
            if(ccqt_array_object_type(left->type).is_const && !flags.static_rebind)
                return cc_type_error(p, tok.loc, left->type, "cannot assign to variable with const-qualified type");
            CcExpr* right;
            CcToken peek_assign;
            err = cc_peek(p, &peek_assign);
            if(err) return err;
            err = cc_parse_assignment_expr(p, flags, &right, kind == CC_EXPR_ASSIGN ? left->type : CCQT_NONE);
            if(err) return err;
            if(kind != CC_EXPR_ASSIGN && kind != CC_EXPR_ADDASSIGN && kind != CC_EXPR_SUBASSIGN){
                CcQualType lt = left->type;
                lt.is_atomic = 0;
                lt = ccqt_enum_underlying(lt);
                if(!ccqt_is_basic(lt) || !ccbt_is_arithmetic(ccqt_basic_kind(lt))){
                    return cc_types_error(p, tok.loc, left->type, right->type, "compound assignment requires arithmetic operands");
                }
            }
            if(kind == CC_EXPR_ASSIGN){
                err = cc_check_atomic_object_access(p, left, tok.loc);
                if(err) return err;
            }
            else {
                err = cc_check_atomic_rmw(p, left, tok.loc);
                if(err) return err;
            }
            if(kind != CC_EXPR_ASSIGN && ccqt_bt_eq(left->type, CCBT__Any))
                return cc_types_error(p, tok.loc, left->type, right->type, "compound assignment requires arithmetic or pointer type");
            if(kind == CC_EXPR_MODASSIGN || kind == CC_EXPR_BITANDASSIGN
            || kind == CC_EXPR_BITORASSIGN || kind == CC_EXPR_BITXORASSIGN
            || kind == CC_EXPR_LSHIFTASSIGN || kind == CC_EXPR_RSHIFTASSIGN){
                if(!ccqt_is_integer(left->type)){
                    return cc_types_error(p, tok.loc, left->type, right->type, "operator '%s' requires integer operands", cc_expr_kind_to_operator(kind));
                }
            }
            CcQualType operation_type = CCQT_NONE;
            if((kind == CC_EXPR_ADDASSIGN || kind == CC_EXPR_SUBASSIGN) && ccqt_kind(left->type) == CC_POINTER){
                err = cc_check_type_has_size_or_error(p, ccqt_as_ptr(left->type)->pointee, tok.loc);
                if(err) return err;
                if(!ccqt_is_integer(right->type))
                    return cc_types_error(p, tok.loc, left->type, right->type, "pointer arithmetic requires integer operand");
                err = cc_implicit_cast_to_index(p, right, &right);
                if(err) return err;
            }
            else if(kind == CC_EXPR_ASSIGN){
                err = cc_implicit_cast(p, right, (CcQualType){.unqual=left->type.unqual}, &right);
                if(err) return err;
            }
            else {
                if(kind == CC_EXPR_MODASSIGN || kind == CC_EXPR_BITANDASSIGN
                || kind == CC_EXPR_BITORASSIGN || kind == CC_EXPR_BITXORASSIGN
                || kind == CC_EXPR_LSHIFTASSIGN || kind == CC_EXPR_RSHIFTASSIGN){
                    if(!ccqt_is_integer(right->type))
                        return cc_types_error(p, tok.loc, left->type, right->type, "operator '%s' requires integer operands", cc_expr_kind_to_operator(kind));
                }
                if(kind == CC_EXPR_LSHIFTASSIGN || kind == CC_EXPR_RSHIFTASSIGN){
                    err = cc_integer_promote_expr(p, left, &operation_type, tok.loc);
                }
                else err = cc_usual_arithmetic(p, left, right, &operation_type, tok.loc);
                if(err) return err;
                err = cc_implicit_cast(p, right, operation_type, &right);
                if(err) return err;
            }
            if(!flags.static_rebind && kind == CC_EXPR_ASSIGN && (right->kind == CC_EXPR_COMPOUND_LITERAL || right->kind == CC_EXPR_INIT_LIST)){
                err = cc_desugar_compound_literal(p, right, &right);
                if(err) return err;
            }
            CcExpr* node = cc_make_expr(p, kind, tok.loc, (CcQualType){.unqual=left->type.unqual}, 1);
            if(!node) return CC_OOM_ERROR;
            node->lhs = left;
            cc_mark_mutable_storage(left);
            if(kind != CC_EXPR_ASSIGN) node->compound.type = operation_type;
            node->values[0] = right;
            *out = node;
            return 0;
        }
    }
    cc_unget(p, &tok);
    *out = left;
    return 0;
}

static
int
cc_parse_ternary_expr(CcParser* p, CcParseFlags flags, CcExpr* _Nullable* _Nonnull out, CcQualType expected){
    CcExpr* cond;
    int err = cc_parse_prefix(p, flags, &cond, expected);
    if(err) return err;
    return cc_parse_ternary_tail(p, flags, cond, out, expected);
}

static
int
cc_parse_ternary_tail(CcParser* p, CcParseFlags flags, CcExpr* cond, CcExpr* _Nullable* _Nonnull out, CcQualType expected){
    int err = cc_parse_infix(p, flags, cond, 4, &cond);
    if(err) return err;
    CcToken tok;
    err = cc_next_token(p, &tok);
    if(err) return err;
    if(tok.type == CC_PUNCTUATOR && tok.punct.punct == CC_question){
        err = cc_require_scalar(p, &cond, tok.loc, "'?:'");
        if(err) return err;
        CcExpr* then_expr;
        err = cc_parse_expr(p, flags, &then_expr, expected);
        if(err) return err;
        err = cc_expect_punct(p, CC_colon);
        if(err) return err;
        CcExpr* else_expr;
        err = cc_parse_ternary_expr(p, flags, &else_expr, expected);
        if(err) return err;
        CcQualType common;
        CcTypeKind tk = ccqt_kind(then_expr->type);
        CcTypeKind ek = ccqt_kind(else_expr->type);
        _Bool tptr = tk == CC_POINTER || ccqt_is_array(then_expr->type) || tk == CC_FUNCTION;
        _Bool eptr = ek == CC_POINTER || ccqt_is_array(else_expr->type) || ek == CC_FUNCTION;
        if(tptr && eptr){
            err = cc_decay(p, &then_expr);
            if(err) return err;
            err = cc_decay(p, &else_expr);
            if(err) return err;
            CcQualType ttype = then_expr->type;
            CcQualType etype = else_expr->type;
            _Bool tnull, enull;
            err = cc_is_null_pointer_constant(p, then_expr, &tnull);
            if(err) return err;
            err = cc_is_null_pointer_constant(p, else_expr, &enull);
            if(err) return err;
            if(tnull) common = etype;
            else if(enull) common = ttype;
            else {
                CcPointer* tp = ccqt_as_ptr(ttype);
                CcPointer* ep = ccqt_as_ptr(etype);
                CcQualType tpointee = tp->pointee;
                CcQualType epointee = ep->pointee;
                _Bool tvoid = ccqt_is_basic(tpointee) && ccqt_basic_kind(tpointee) == CCBT_void;
                _Bool evoid = ccqt_is_basic(epointee) && ccqt_basic_kind(epointee) == CCBT_void;
                CcQualType pointee;
                if(tvoid || evoid){
                    pointee = ccqt_basic(CCBT_void);
                    pointee.quals = cc_object_quals(tpointee) | cc_object_quals(epointee);
                    pointee.is_atomic = 0;
                }
                else if(cc_types_compatible_ignoring_cv(tpointee, epointee)){
                    err = cc_merge_qualified_types(p, tpointee, epointee, &pointee);
                    if(err) return err;
                }
                else {
                    CcQualType target = epointee;
                    target.quals |= tpointee.quals;
                    if(cc_pointer_pointee_convertible(tpointee, target))
                        pointee = target;
                    else {
                        pointee = tpointee;
                        pointee.quals |= epointee.quals;
                    }
                }
                err = cc_pointer_of(p, pointee, &common);
                if(err) return err;
            }
        }
        else if(tptr && (ccqt_is_basic(else_expr->type) || ek == CC_ENUM)){
            _Bool is_npc;
            err = cc_is_null_pointer_constant(p, else_expr, &is_npc);
            if(err) return err;
            if(!is_npc)
                return cc_types_error(p, tok.loc, then_expr->type, else_expr->type, "incompatible operand types for ternary");
            err = cc_decay(p, &then_expr);
            if(err) return err;
            common = then_expr->type;
        }
        else if(eptr && (ccqt_is_basic(then_expr->type) || tk == CC_ENUM)){
            _Bool is_npc;
            err = cc_is_null_pointer_constant(p, then_expr, &is_npc);
            if(err) return err;
            if(!is_npc)
                return cc_types_error(p, tok.loc, then_expr->type, else_expr->type, "incompatible operand types for ternary");
            err = cc_decay(p, &else_expr);
            if(err) return err;
            common = else_expr->type;
        }
        else if(ccqt_bt_eq(then_expr->type, CCBT_nullptr_t) && ccqt_bt_eq(else_expr->type, CCBT_nullptr_t)){
            common = ccqt_basic(CCBT_nullptr_t);
        }
        else if(ccqt_is_basic(then_expr->type) && ccqt_basic_kind(then_expr->type) == CCBT_void
             && ccqt_is_basic(else_expr->type) && ccqt_basic_kind(else_expr->type) == CCBT_void){
            common = then_expr->type;
        }
        else if((ccqt_bt_eq(then_expr->type, CCBT__Any)
            || ccqt_bt_eq(then_expr->type, CCBT__Type) || tk == CC_VECTOR)
            && cc_types_compatible_unqualified(then_expr->type, else_expr->type)){
            common = then_expr->type;
        }
        else if((tk == CC_STRUCT || tk == CC_UNION) && tk == ek
            && cc_types_compatible((CcQualType){.unqual=then_expr->type.unqual}, (CcQualType){.unqual=else_expr->type.unqual})){
            err = cc_merge_compatible_decl_types(p, (CcQualType){.unqual=then_expr->type.unqual},
                (CcQualType){.unqual=else_expr->type.unqual}, &common);
            if(err) return err;
        }
        else if(tk == CC_SLICE && ek == CC_SLICE){
            CcQualType tpointee = ccqt_as_slice(then_expr->type)->pointee;
            CcQualType epointee = ccqt_as_slice(else_expr->type)->pointee;
            if(!cc_types_compatible_ignoring_cv(tpointee, epointee))
                return cc_types_error(p, tok.loc, then_expr->type, else_expr->type, "incompatible operand types for ternary");
            CcQualType pointee;
            err = cc_merge_qualified_types(p, tpointee, epointee, &pointee);
            if(err) return err;
            err = cc_slice_of(p, pointee, tok.loc, &common);
            if(err) return err;
        }
        else {
            err = cc_usual_arithmetic(p, then_expr, else_expr, &common, tok.loc);
            if(err) return err;
        }
        common = (CcQualType){.unqual=common.unqual};
        err = cc_implicit_cast(p, then_expr, common, &then_expr);
        if(err) return err;
        err = cc_implicit_cast(p, else_expr, common, &else_expr);
        if(err) return err;
        CcExpr* node = cc_make_expr(p, CC_EXPR_TERNARY, tok.loc, common, 2);
        if(!node) return CC_OOM_ERROR;
        node->lhs = cond;
        node->values[0] = then_expr;
        node->values[1] = else_expr;
        *out = node;
        return 0;
    }
    cc_unget(p, &tok);
    *out = cond;
    return 0;
}

static
int
cc_parse_infix(CcParser* p, CcParseFlags flags, CcExpr* left, int min_prec, CcExpr* _Nullable* _Nonnull out){
    for(;;){
        CcToken tok;
        int err = cc_next_token(p, &tok);
        if(err) return err;
        if(tok.type != CC_PUNCTUATOR){
            cc_unget(p, &tok);
            break;
        }
        CcExprKind kind;
        int prec;
        if(!cc_binop_lookup(tok.punct.punct, &kind, &prec)){
            cc_unget(p, &tok);
            break;
        }
        if(prec < min_prec){
            cc_unget(p, &tok);
            break;
        }
        CcExpr* right;
        err = cc_parse_prefix(p, flags, &right, CCQT_NONE);
        if(err) return err;
        err = cc_parse_infix(p, flags, right, prec + 1, &right);
        if(err) return err;
        if((kind == CC_EXPR_ADD || kind == CC_EXPR_SUB)
            && (ccqt_bt_eq(left->type, CCBT_nullptr_t) || ccqt_bt_eq(right->type, CCBT_nullptr_t)))
            return cc_types_error(p, tok.loc, left->type, right->type, "pointer arithmetic does not accept nullptr_t");
        CcQualType result_type = {0};
        switch(kind){
            case CC_EXPR_LOGAND: case CC_EXPR_LOGOR:
                err = cc_require_scalar(p, &left, tok.loc, kind == CC_EXPR_LOGAND ? "'&&'" : "'||'");
                if(err) return err;
                err = cc_require_scalar(p, &right, tok.loc, kind == CC_EXPR_LOGAND ? "'&&'" : "'||'");
                if(err) return err;
                result_type = ccqt_basic(CCBT_int);
                break;
            case CC_EXPR_EQ: case CC_EXPR_NE:
            case CC_EXPR_LT: case CC_EXPR_GT:
            case CC_EXPR_LE: case CC_EXPR_GE: {
                if(ccqt_is_basic(left->type) && ccqt_basic_kind(left->type) == CCBT__Type && ccqt_is_basic(right->type) && ccqt_basic_kind(right->type) == CCBT__Type){
                    if(kind != CC_EXPR_EQ && kind != CC_EXPR_NE)
                        return cc_error(p, tok.loc, "ordered comparison of _Type values");
                    result_type = ccqt_basic(CCBT_int);
                    break;
                }
                _Bool lp = ccqt_kind(left->type) == CC_POINTER || ccqt_is_array(left->type)
                    || ccqt_kind(left->type) == CC_FUNCTION || ccqt_bt_eq(left->type, CCBT_nullptr_t);
                _Bool rp = ccqt_kind(right->type) == CC_POINTER || ccqt_is_array(right->type)
                    || ccqt_kind(right->type) == CC_FUNCTION || ccqt_bt_eq(right->type, CCBT_nullptr_t);
                if((lp || rp) && kind != CC_EXPR_EQ && kind != CC_EXPR_NE){
                    if(!lp || !rp || ccqt_bt_eq(left->type, CCBT_nullptr_t)
                        || ccqt_bt_eq(right->type, CCBT_nullptr_t)
                        || ccqt_callable_function(left->type) || ccqt_callable_function(right->type))
                        return cc_types_error(p, tok.loc, left->type, right->type, "ordered pointer comparison requires two object pointers");
                }
                if(!lp && !rp){
                    CcQualType common;
                    err = cc_usual_arithmetic(p, left, right, &common, tok.loc);
                    if(err) return err;
                    if(kind != CC_EXPR_EQ && kind != CC_EXPR_NE && ccbt_is_complex(ccqt_basic_kind(common)))
                        return cc_types_error(p, tok.loc, left->type, right->type, "ordered comparison requires real operands");
                    err = cc_implicit_cast(p, left, common, &left);
                    if(err) return err;
                    err = cc_implicit_cast(p, right, common, &right);
                    if(err) return err;
                }
                else if(lp && rp){
                    err = cc_decay(p, &left);
                    if(err) return err;
                    err = cc_decay(p, &right);
                    if(err) return err;
                    if(!ccqt_bt_eq(left->type, CCBT_nullptr_t) && !ccqt_bt_eq(right->type, CCBT_nullptr_t)){
                        CcQualType lpointee, rpointee;
                        err = cc_deref_type_or_error(p, left->type, &lpointee, tok.loc, 0);
                        if(err) return cc_unreachable(p, tok.loc, "Error dereferencing lhs pointer");
                        err = cc_deref_type_or_error(p, right->type, &rpointee, tok.loc, 0);
                        if(err) return cc_unreachable(p, tok.loc, "Error dereferencing rhs pointer");
                        if(!cc_types_compatible_ignoring_cv(lpointee, rpointee)
                        && !(ccqt_is_basic(lpointee) && ccqt_basic_kind(lpointee) == CCBT_void)
                        && !(ccqt_is_basic(rpointee) && ccqt_basic_kind(rpointee) == CCBT_void))
                            return cc_types_error(p, tok.loc, left->type, right->type, "comparison of incompatible pointer types");
                    }
                }
                else if(lp){
                    err = cc_decay(p, &left);
                    if(err) return err;
                    err = cc_implicit_cast(p, right, left->type, &right);
                    if(err) return err;
                }
                else {
                    err = cc_decay(p, &right);
                    if(err) return err;
                    err = cc_implicit_cast(p, left, right->type, &left);
                    if(err) return err;
                }
                result_type = ccqt_basic(CCBT_int);
                break;
            }
            case CC_EXPR_LSHIFT: case CC_EXPR_RSHIFT: {
                CcQualType lp, rp;
                err = cc_integer_promote_expr(p, left, &lp, tok.loc);
                if(err) return err;
                err = cc_integer_promote_expr(p, right, &rp, tok.loc);
                if(err) return err;
                if(!ccqt_is_integer(lp) || !ccqt_is_integer(rp))
                    return cc_types_error(p, tok.loc, left->type, right->type, "shift operands require integer type");
                err = cc_implicit_cast(p, left, lp, &left);
                if(err) return err;
                err = cc_implicit_cast(p, right, rp, &right);
                if(err) return err;
                result_type = lp;
                break;
            }
            case CC_EXPR_ADD: {
                _Bool lptr = ccqt_kind(left->type) == CC_POINTER || ccqt_is_array(left->type);
                _Bool rptr = ccqt_kind(right->type) == CC_POINTER || ccqt_is_array(right->type);
                if(lptr && rptr)
                    return cc_types_error(p, tok.loc, left->type, right->type, "addition of two pointers");
                if(lptr || rptr) {
                    CcExpr** ptr_operand = lptr ? &left : &right;
                    CcExpr* int_operand = lptr ? right : left;
                    CcQualType element;
                    err = cc_deref_type_or_error(p, (*ptr_operand)->type, &element, tok.loc, 0);
                    if(err) return err;
                    err = cc_check_type_has_size_or_error(p, element, tok.loc);
                    if(err) return err;
                    if(!ccqt_is_integer(int_operand->type))
                        return cc_types_error(p, tok.loc, left->type, right->type, "pointer arithmetic requires integer operand");
                    if(rptr){
                        err = cc_implicit_cast_to_index(p, left, &left);
                        if(err) return err;
                    }
                    else {
                        err = cc_implicit_cast_to_index(p, right, &right);
                        if(err) return err;
                    }
                    if(ccqt_is_array((*ptr_operand)->type)){
                        err = cc_pointer_of(p, ccqt_qualified_array_element((*ptr_operand)->type), &result_type);
                        if(err) return err;
                        err = cc_implicit_cast(p, *ptr_operand, result_type, ptr_operand);
                        if(err) return err;
                    }
                    else
                        result_type = (CcQualType){.unqual=(*ptr_operand)->type.unqual};
                }
                else {
                    err = cc_usual_arithmetic(p, left, right, &result_type, tok.loc);
                    if(err) return err;
                    err = cc_implicit_cast(p, left, result_type, &left);
                    if(err) return err;
                    err = cc_implicit_cast(p, right, result_type, &right);
                    if(err) return err;
                }
                break;
            }
            case CC_EXPR_SUB: {
                _Bool lptr = ccqt_kind(left->type) == CC_POINTER || ccqt_is_array(left->type);
                _Bool rptr = ccqt_kind(right->type) == CC_POINTER || ccqt_is_array(right->type);
                if(lptr && rptr){
                    CcQualType lp, rp;
                    err = cc_deref_type_or_error(p, left->type, &lp, tok.loc, 0);
                    if(err) return err;
                    err = cc_deref_type_or_error(p, right->type, &rp, tok.loc, 0);
                    if(err) return err;
                    err = cc_check_type_has_size_or_error(p, lp, tok.loc);
                    if(err) return err;
                    err = cc_check_type_has_size_or_error(p, rp, tok.loc);
                    if(err) return err;
                    if(!cc_types_compatible_ignoring_cv(lp, rp)
                    && !(ccqt_is_basic(lp) && ccqt_basic_kind(lp) == CCBT_void)
                    && !(ccqt_is_basic(rp) && ccqt_basic_kind(rp) == CCBT_void))
                        return cc_types_error(p, tok.loc, left->type, right->type, "pointer subtraction with incompatible types");
                    if(ccqt_is_array(left->type)){
                        CcQualType ptr_type;
                        err = cc_pointer_of(p, ccqt_qualified_array_element(left->type), &ptr_type);
                        if(err) return err;
                        err = cc_implicit_cast(p, left, ptr_type, &left);
                        if(err) return err;
                    }
                    if(ccqt_is_array(right->type)){
                        CcQualType ptr_type;
                        err = cc_pointer_of(p, ccqt_qualified_array_element(right->type), &ptr_type);
                        if(err) return err;
                        err = cc_implicit_cast(p, right, ptr_type, &right);
                        if(err) return err;
                    }
                    if(ccqt_bt_eq(lp, CCBT_void) != ccqt_bt_eq(rp, CCBT_void)){
                        CcQualType pointee = ccqt_bt_eq(lp, CCBT_void) ? rp : lp;
                        pointee.quals |= lp.quals | rp.quals;
                        CcQualType common;
                        err = cc_pointer_of(p, pointee, &common);
                        if(err) return err;
                        err = cc_implicit_cast(p, left, common, &left);
                        if(err) return err;
                        err = cc_implicit_cast(p, right, common, &right);
                        if(err) return err;
                    }
                    result_type = ccqt_basic(cc_target(p)->ptrdiff_type);
                }
                else if(lptr){
                    CcQualType element;
                    err = cc_deref_type_or_error(p, left->type, &element, tok.loc, 0);
                    if(err) return err;
                    err = cc_check_type_has_size_or_error(p, element, tok.loc);
                    if(err) return err;
                    if(!ccqt_is_integer(right->type))
                        return cc_types_error(p, tok.loc, left->type, right->type, "pointer arithmetic requires integer operand");
                    err = cc_implicit_cast_to_index(p, right, &right);
                    if(err) return err;
                    if(ccqt_is_array(left->type)){
                        err = cc_pointer_of(p, ccqt_qualified_array_element(left->type), &result_type);
                        if(err) return err;
                        err = cc_implicit_cast(p, left, result_type, &left);
                        if(err) return err;
                    }
                    else
                        result_type = (CcQualType){.unqual=left->type.unqual};
                }
                else {
                    err = cc_usual_arithmetic(p, left, right, &result_type, tok.loc);
                    if(err) return err;
                    err = cc_implicit_cast(p, left, result_type, &left);
                    if(err) return err;
                    err = cc_implicit_cast(p, right, result_type, &right);
                    if(err) return err;
                }
                break;
            }
            case CC_EXPR_MUL:
            case CC_EXPR_DIV:
            case CC_EXPR_MOD:
            case CC_EXPR_BITAND:
            case CC_EXPR_BITOR:
            case CC_EXPR_BITXOR: {
                err = cc_usual_arithmetic(p, left, right, &result_type, tok.loc);
                if(err) return err;
                if(kind == CC_EXPR_MOD || kind == CC_EXPR_BITAND
                || kind == CC_EXPR_BITOR || kind == CC_EXPR_BITXOR){
                    if(!ccqt_is_integer(result_type))
                        return cc_types_error(p, tok.loc, left->type, right->type, "operator '%s' requires integer operands", cc_expr_kind_to_operator(kind));
                }
                err = cc_implicit_cast(p, left, result_type, &left);
                if(err) return err;
                err = cc_implicit_cast(p, right, result_type, &right);
                if(err) return err;
                break;
            }
            case CC_EXPR_BLOB:
            case CC_EXPR_VALUE:
            case CC_EXPR_SIZEOF_VMT:
            case CC_EXPR_VARIABLE:
            case CC_EXPR_FUNCTION:
            case CC_EXPR_COMPOUND_LITERAL:
            case CC_EXPR_INIT_LIST:
            case CC_EXPR_OBJECT_VIEW:
            case CC_EXPR_NEG:
            case CC_EXPR_POS:
            case CC_EXPR_BITNOT:
            case CC_EXPR_LOGNOT:
            case CC_EXPR_DEREF:
            case CC_EXPR_ADDR:
            case CC_EXPR_PREINC:
            case CC_EXPR_PREDEC:
            case CC_EXPR_POSTINC:
            case CC_EXPR_POSTDEC:
            case CC_EXPR_ASSIGN:
            case CC_EXPR_ADDASSIGN:
            case CC_EXPR_SUBASSIGN:
            case CC_EXPR_MULASSIGN:
            case CC_EXPR_DIVASSIGN:
            case CC_EXPR_MODASSIGN:
            case CC_EXPR_BITANDASSIGN:
            case CC_EXPR_BITORASSIGN:
            case CC_EXPR_BITXORASSIGN:
            case CC_EXPR_LSHIFTASSIGN:
            case CC_EXPR_RSHIFTASSIGN:
            case CC_EXPR_TERNARY:
            case CC_EXPR_CAST:
            case CC_EXPR_CALL:
            case CC_EXPR_SUBSCRIPT:
            case CC_EXPR_DOT:
            case CC_EXPR_ARROW:
            case CC_EXPR_COMMA:
            case CC_EXPR_STATEMENT_EXPRESSION:
            case CC_EXPR_ATOMIC:
            case CC_EXPR_VA:
            case CC_EXPR_BUILTIN:
            case CC_EXPR_ADD_OVERFLOW:
            case CC_EXPR_MUL_OVERFLOW:
            case CC_EXPR_SUB_OVERFLOW:
            case CC_EXPR_BIT_BUILTIN:
            case CC_EXPR_ALLOCA:
            case CC_EXPR_INTERN:
            case CC_EXPR_HOTSWAP:
            case CC_EXPR_SRCLOC_REFLECT:
            case CC_EXPR_COMPILE:
            case CC_EXPR_MODULE_REFLECT:
            case CC_EXPR_TYPE_INTROSPECTION:
            case CC_EXPR_UMUL128:
            case CC_EXPR_SLICE:
            case CC_EXPR_SLICE_LO:
            case CC_EXPR_SLICE_HI:
            case CC_EXPR_SLICE_ALL:
            case CC_EXPR_BSWAP:
                return CC_UNREACHABLE_ERROR;
        }
        CcExpr* node = cc_make_expr(p, kind, tok.loc, result_type, 1);
        if(!node) return CC_OOM_ERROR;
        node->lhs = left;
        node->values[0] = right;
        left = node;
    }
    *out = left;
    return 0;
}

static
int
cc_parse_type_value_tail(CcParser* p, CcParseFlags flags, CcQualType type,
    SrcLoc loc, _Bool comma, CcExpr* _Nullable* _Nonnull out){
    CcExpr* value = cc_value_expr(p, loc, ccqt_basic(CCBT__Type));
    if(!value) return CC_OOM_ERROR;
    value->type_value = type;
    int err = cc_parse_postfix(p, flags, value, &value);
    if(err) goto fail;
    err = cc_parse_ternary_tail(p, flags, value, &value, CCQT_NONE);
    if(err) goto fail;
    err = cc_parse_assignment_tail(p, flags, value, &value);
    if(err) goto fail;
    if(comma) err = cc_parse_comma_tail(p, flags, value, &value, CCQT_NONE);
    if(err) goto fail;
    *out = value;
    fail:
    return err;
}

static
int
cc_parse_type_query_operand(CcParser* p, CcParseFlags flags, CcPunct terminator,
    _Bool comma, CcQualType* type, CcExpr* _Nullable* _Nonnull expression){
    *expression = NULL;
    CcToken tok;
    int err = cc_peek(p, &tok);
    if(err) return err;
    CcParseFlags expression_flags = flags;
    expression_flags.vc = CC_RUNTIME_VALUE;
    if(cc_is_type_start(p, &tok)){
        SrcLoc loc = tok.loc;
        err = cc_parse_type_name(p, flags, type, NULL);
        if(err) return err;
        err = cc_peek(p, &tok);
        if(err) return err;
        if(tok.type == CC_PUNCTUATOR && tok.punct.punct == terminator) return 0;
        err = cc_parse_type_value_tail(p, expression_flags, *type, loc, comma, expression);
    }
    else if(comma)
        err = cc_parse_expr(p, expression_flags, expression, CCQT_NONE);
    else
        err = cc_parse_assignment_expr(p, expression_flags, expression, CCQT_NONE);
    if(err) return err;
    *type = (*expression)->type;
    return 0;
}

static
int
cc_parse_type_operand_tail(CcParser* p, CcParseFlags flags, CcQualType type,
    CcDeclBase storage, SrcLoc loc, CcExpr* _Nullable* _Nonnull out){
    *out = NULL;
    CcToken tok;
    int err = cc_peek(p, &tok);
    if(err) return err;
    if(tok.type != CC_PUNCTUATOR || tok.punct.punct != CC_rparen){
        if(storage.spec.sp_storagebits)
            return cc_error(p, loc, "storage class requires a compound literal");
        if(storage.has_alignas)
            return cc_error(p, loc, "_Alignas requires a compound literal");
        err = cc_parse_type_value_tail(p, flags, type, loc, 1, out);
        if(err) return err;
    }
    err = cc_expect_punct(p, CC_rparen);
    if(err || *out) return err;
    err = cc_peek(p, &tok);
    if(err) return err;
    if(tok.type == CC_PUNCTUATOR && tok.punct.punct == CC_lbrace){
        CcExpr* literal = NULL; // GCC 13 CFA is buggy, so we init here
        err = cc_parse_compound_literal(p, flags, type, storage, &literal);
        if(err) return err;
        return cc_parse_postfix(p, flags, literal, out);
    }
    if(storage.spec.sp_storagebits)
        return cc_error(p, loc, "storage class requires a compound literal");
    if(storage.has_alignas)
        return cc_error(p, loc, "_Alignas requires a compound literal");
    return 0;
}

static
int
cc_parse_type_or_unary_operand(CcParser* p, CcParseFlags flags, SrcLoc loc,
    CcQualType* type, CcExpr* _Nullable* _Nonnull operand){
    *operand = NULL;
    CcToken tok;
    int err = cc_peek(p, &tok);
    if(err) return err;
    if(tok.type == CC_PUNCTUATOR && tok.punct.punct == CC_lparen){
        err = cc_next_token(p, &tok);
        if(err) return err;
        CcToken peek;
        err = cc_peek(p, &peek);
        if(err) return err;
        if(cc_is_compound_type_start(p, &peek)){
            CcDeclBase storage = {0};
            err = cc_parse_compound_type(p, flags, type, NULL, &storage);
            if(err) return err;
            err = cc_parse_type_operand_tail(p, flags, *type, storage, loc, operand);
            if(!err && *operand) *type = (*operand)->type;
            return err;
        }
        err = cc_unget(p, &tok);
        if(err) return err;
    }
    err = cc_parse_prefix(p, flags, operand, CCQT_NONE);
    if(!err) *type = (*operand)->type;
    return err;
}

static
int
cc_parse_prefix_inner(CcParser* p, CcParseFlags flags, CcExpr* _Nullable* _Nonnull out, CcQualType expected);

static
_Bool
cc_token_starts_cast_operand(const CcToken* tok){
    if(tok->type == CC_EOF) return 0;
    if(tok->type != CC_PUNCTUATOR) return 1;
    switch(tok->punct.punct){
        case CC_lparen: case CC_lbrace:
        case CC_plus: case CC_minus: case CC_tilde: case CC_bang:
        case CC_star: case CC_amp: case CC_plusplus: case CC_minusminus:
            return 1;
        default: return 0;
    }
}

static
int
cc_check_addressable(CcParser* p, CcExpr* operand, SrcLoc loc){
    CcExpr* object = operand;
    while(object->kind == CC_EXPR_CAST) object = object->lhs;
    for(;;){
        if(object->compound_register)
            return cc_error(p, loc, "cannot take address of register compound literal");
        if(object->kind == CC_EXPR_VARIABLE && object->var->register_)
            return cc_error(p, loc, "cannot take address of register variable");
        if(object->kind == CC_EXPR_DOT || object->kind == CC_EXPR_COMMA)
            object = object->values[0];
        else if(object->kind == CC_EXPR_SUBSCRIPT
            && (ccqt_is_array(object->lhs->type) || ccqt_kind(object->lhs->type) == CC_VECTOR))
            object = object->lhs;
        else break;
    }
    if(cc_expr_field_bit_width(operand))
        return cc_error(p, loc, "cannot take address of bitfield");
    return 0;
}

static
int
cc_adapt_receiver(CcParser* p, CcFunction* function, CcExpr* operand,
    SrcLoc loc, CcExpr* _Nullable* _Nonnull out){
    *out = operand;
    if(!function->param_count) return 0;
    _Bool wants_pointer = ccqt_kind(function->params[0]) == CC_POINTER;
    _Bool is_pointer = ccqt_kind(operand->type) == CC_POINTER;
    CcExprKind kind;
    CcQualType type;
    int err;
    if(wants_pointer && !is_pointer && operand->is_lvalue){
        err = cc_check_addressable(p, operand, loc);
        if(err) return err;
        err = cc_pointer_of(p, operand->type, &type);
        kind = CC_EXPR_ADDR;
    }
    else if(!wants_pointer && is_pointer){
        err = cc_deref_type_or_error(p, operand->type, &type, loc, 0);
        kind = CC_EXPR_DEREF;
    }
    else return 0;
    if(err) return err;
    CcExpr* result = cc_unary_expr(p, kind, loc, type, operand);
    if(!result) return CC_OOM_ERROR;
    result->is_lvalue = kind == CC_EXPR_DEREF;
    *out = result;
    return 0;
}

static
int
cc_parse_prefix(CcParser* p, CcParseFlags flags, CcExpr* _Nullable* _Nonnull out, CcQualType expected){
    CcStmtSink* sink = cc_push_stmt_sink(p);
    if(!sink) return CC_OOM_ERROR;
    int err = cc_parse_prefix_inner(p, flags, out, expected);
    if(err) goto done;
    while(sink->stmts.count && *out){
        // Prepend in reverse so captured initializers retain evaluation order.
        CcStmtNode* stmt = sink->stmts.data[sink->stmts.count-1];
        if(stmt->kind != CC_STMT_EXPR){
            err = cc_ice(p, stmt->loc, "%s", "non-expression initializer in prefix expression");
            goto done;
        }
        CcExpr* value = *out;
        CcExpr* comma = cc_binary_expr(p, CC_EXPR_COMMA, value->loc, value->type, stmt->expr, value);
        if(!comma){
            err = CC_OOM_ERROR;
            goto done;
        }
        comma->is_lvalue = value->is_lvalue;
        *out = comma;
        stmt->expr = NULL; // ownership transferred to the comma expression
        sink->stmts.count--;
        cc_free_stmt_tree(p, stmt);
    }
done:
    cc_pop_stmt_sink(p, sink);
    return err;
}

static
int
cc_parse_prefix_inner(CcParser* p, CcParseFlags flags, CcExpr* _Nullable* _Nonnull out, CcQualType expected){
    CcToken tok;
    int err = cc_next_token(p, &tok);
    if(err) return err;
    if(tok.type == CC_PUNCTUATOR){
        if(tok.punct.punct == CC_lparen){
            CcToken peek;
            err = cc_peek(p, &peek);
            if(err) return err;
            if(cc_is_compound_type_start(p, &peek)){
                CcQualType cast_type;
                CcDeclBase storage = {0};
                CcParsedParams param_names = {0};
                err = cc_parse_compound_type(p, flags, &cast_type, &param_names, &storage);
                if(err){
                    ma_cleanup(CcFuncParam)(&param_names.names, cc_allocator(p));
                    return err;
                }
                CcToken peek_after;
                err = cc_peek(p, &peek_after);
                if(err){
                    ma_cleanup(CcFuncParam)(&param_names.names, cc_allocator(p));
                    return err;
                }
                if(peek_after.type == CC_PUNCTUATOR && peek_after.punct.punct == CC_lbrace
                   && ccqt_kind(cast_type) == CC_FUNCTION){
                    if(storage.spec.sp_storagebits)
                        return cc_error(p, tok.loc, "storage class requires a compound literal");
                    err = cc_check_decl_alignment(p, &storage, cast_type, 0);
                    if(err) return err;
                    CcExpr* lambda;
                    err = cc_parse_lambda_body(p, flags, tok.loc, cast_type, &param_names, &lambda);
                    if(err) return err;
                    err = cc_expect_punct(p, CC_rparen);
                    if(err) return err;
                    return cc_parse_postfix(p, flags, lambda, out);
                }
                ma_cleanup(CcFuncParam)(&param_names.names, cc_allocator(p));
                CcExpr* value;
                err = cc_parse_type_operand_tail(p, flags, cast_type, storage, tok.loc, &value);
                if(err) return err;
                if(value)
                    return cc_parse_postfix(p, flags, value, out);
                CcToken peek2;
                err = cc_peek(p, &peek2);
                if(err) return err;
                if(!cc_token_starts_cast_operand(&peek2)){
                    CcExpr* type_val = cc_value_expr(p, tok.loc, ccqt_basic(CCBT__Type));
                    if(!type_val) return CC_OOM_ERROR;
                    type_val->uinteger = cast_type.bits;
                    return cc_parse_postfix(p, flags, type_val, out);
                }
                CcExpr* operand;
                err = cc_parse_prefix(p, flags, &operand, cast_type);
                if(err) return err;
                err = cc_check_cast(p, operand->type, cast_type, tok.loc);
                if(err) return err;
                err = cc_check_atomic_object_access(p, operand, operand->loc);
                if(err) return err;
                cast_type.quals = 0;
                if(ccqt_bt_eq(cast_type, CCBT__Any))
                    return cc_implicit_cast(p, operand, cast_type, out);
                CcExpr* cast = cc_unary_expr(p, CC_EXPR_CAST, tok.loc, cast_type, operand);
                if(!cast) return CC_OOM_ERROR;
                *out = cast;
                return 0;
            }
        }
        CcExprKind kind = CC_EXPR_VALUE;
        _Bool is_prefix = 1;
        switch(tok.punct.punct){
            case CC_minus:     kind = CC_EXPR_NEG;    break;
            case CC_plus:      kind = CC_EXPR_POS;    break;
            case CC_tilde:     kind = CC_EXPR_BITNOT; break;
            case CC_bang:      kind = CC_EXPR_LOGNOT; break;
            case CC_star:      kind = CC_EXPR_DEREF;  break;
            case CC_amp:       kind = CC_EXPR_ADDR;   break;
            case CC_plusplus:   kind = CC_EXPR_PREINC; break;
            case CC_minusminus:kind = CC_EXPR_PREDEC; break;
            case CC_lbracket:
            case CC_rbracket:
            case CC_lparen:
            case CC_rparen:
            case CC_lbrace:
            case CC_rbrace:
            case CC_dot:
            case CC_slash:
            case CC_percent:
            case CC_lt:
            case CC_gt:
            case CC_xor:
            case CC_pipe:
            case CC_question:
            case CC_colon:
            case CC_semi:
            case CC_assign:
            case CC_comma:
            case CC_arrow:
            case CC_lshift:
            case CC_rshift:
            case CC_le:
            case CC_ge:
            case CC_eq:
            case CC_ne:
            case CC_and:
            case CC_or:
            case CC_double_colon:
            case CC_ellipsis:
            case CC_star_assign:
            case CC_slash_assign:
            case CC_percent_assign:
            case CC_plus_assign:
            case CC_minus_assign:
            case CC_lshift_assign:
            case CC_rshift_assign:
            case CC_amp_assign:
            case CC_xor_assign:
            case CC_pipe_assign:
                is_prefix = 0; break;
        }
        if(is_prefix){
            if((kind == CC_EXPR_PREINC || kind == CC_EXPR_PREDEC) && flags.vc > CC_RUNTIME_VALUE && !flags.static_rebind)
                return cc_error(p, tok.loc, "increment/decrement in constant expression");
            CcExpr* operand;
            CcParseFlags operand_flags = flags;
            if(kind == CC_EXPR_ADDR) operand_flags.vc = CC_RUNTIME_VALUE;
            CcQualType operand_expected = CCQT_NONE;
            if(kind == CC_EXPR_ADDR && expected.bits && ccqt_kind(expected) == CC_POINTER)
                operand_expected = ccqt_as_ptr(expected)->pointee;
            err = cc_parse_prefix(p, operand_flags, &operand, operand_expected);
            if(err) return err;
            CcQualType result_type;
            switch(kind){
                case CC_EXPR_NEG: case CC_EXPR_POS: {
                    err = cc_integer_promote_expr(p, operand, &result_type, tok.loc);
                    if(err) return err;
                    err = cc_implicit_cast(p, operand, result_type, &operand);
                    if(err) return err;
                    break;
                }
                case CC_EXPR_BITNOT: {
                    if(!ccqt_is_integer(operand->type))
                        return cc_type_error(p, tok.loc, operand->type, "'~' requires integer type");
                    err = cc_integer_promote_expr(p, operand, &result_type, tok.loc);
                    if(err) return err;
                    err = cc_implicit_cast(p, operand, result_type, &operand);
                    if(err) return err;
                    break;
                }
                case CC_EXPR_LOGNOT:
                    err = cc_require_scalar(p, &operand, tok.loc, "'!'");
                    if(err) return err;
                    result_type = ccqt_basic(CCBT_int);
                    break;
                case CC_EXPR_DEREF:
                    if(ccqt_kind(operand->type) == CC_FUNCTION){
                        // decays back to same function
                        *out = operand;
                        return 0;
                    }
                    if(ccqt_is_array(operand->type)){
                        CcQualType ptr_type;
                        err = cc_pointer_of(p, ccqt_qualified_array_element(operand->type), &ptr_type);
                        if(err) return err;
                        err = cc_implicit_cast(p, operand, ptr_type, &operand);
                        if(err) return err;
                    }
                    if(ccqt_kind(operand->type) == CC_SLICE)
                        return cc_type_error(p, tok.loc, operand->type, "dereferencing non-pointer type");
                    err = cc_deref_type_or_error(p, operand->type, &result_type, tok.loc, 0);
                    if(err) return err;
                    break;
                case CC_EXPR_ADDR: {
                    err = cc_check_addressable(p, operand, tok.loc);
                    if(err) return err;
                    CcExpr* val = operand;
                    while(val->kind == CC_EXPR_CAST){
                        val = val->lhs;
                    }
                    if(val->kind == CC_EXPR_VALUE && !ccqt_is_fixed_sequence(val->type)){
                        // eg &3. Desugar to &(int){3}.
                        err = cc_wrap_to_desugared_compound_literal(p, flags, operand, &operand);
                        if(err) return err;
                    }
                    if(operand->kind == CC_EXPR_INIT_LIST){
                        operand->kind = CC_EXPR_COMPOUND_LITERAL;
                        operand->defer_compound_storage = flags.parsing_default_arg;
                    }
                    if(operand->kind == CC_EXPR_COMPOUND_LITERAL){
                        err = cc_desugar_compound_literal(p, operand, &operand);
                        if(err) return err;
                    }
                    if(ccqt_kind(operand->type) == CC_FUNCTION){
                        err = cc_pointer_of(p, operand->type, &result_type);
                        if(err) return err;
                        return cc_implicit_cast(p, operand, result_type, out);
                    }
                    if(!operand->is_lvalue)
                        return cc_error(p, tok.loc, "cannot take address of rvalue");
                    if(flags.vc >= CC_LINKTIME_VALUE && operand->kind == CC_EXPR_VARIABLE && operand->var->automatic)
                        return cc_error(p, tok.loc, "address of automatic variable in constant expression");
                    err = cc_pointer_of(p, operand->type, &result_type);
                    if(err) return err;
                    break;
                }
                case CC_EXPR_PREINC:
                case CC_EXPR_PREDEC:
                    if(flags.static_rebind && !cc_static_rebind_target(p, operand))
                        return cc_error(p, operand->loc, "static for update must rebind a clause-1 variable");
                    if(operand->kind == CC_EXPR_COMPOUND_LITERAL){
                        err = cc_desugar_compound_literal(p, operand, &operand);
                        if(err) return err;
                    }
                    if(!operand->is_lvalue)
                        return cc_error(p, tok.loc, "expression is not an lvalue");
                    if(operand->type.is_const && !flags.static_rebind)
                        return cc_type_error(p, tok.loc, operand->type, "cannot modify const-qualified variable");
                    err = cc_check_incdec(p, operand, tok.loc);
                    if(err) return err;
                    result_type = (CcQualType){.unqual=operand->type.unqual};
                    break;
                case CC_EXPR_BLOB:
                case CC_EXPR_VALUE:
                case CC_EXPR_SIZEOF_VMT:
                case CC_EXPR_VARIABLE:
                case CC_EXPR_FUNCTION:
                case CC_EXPR_COMPOUND_LITERAL:
                case CC_EXPR_INIT_LIST:
                case CC_EXPR_OBJECT_VIEW:
                case CC_EXPR_POSTINC:
                case CC_EXPR_POSTDEC:
                case CC_EXPR_ADD:
                case CC_EXPR_SUB:
                case CC_EXPR_MUL:
                case CC_EXPR_DIV:
                case CC_EXPR_MOD:
                case CC_EXPR_BITAND:
                case CC_EXPR_BITOR:
                case CC_EXPR_BITXOR:
                case CC_EXPR_LSHIFT:
                case CC_EXPR_RSHIFT:
                case CC_EXPR_LOGAND:
                case CC_EXPR_LOGOR:
                case CC_EXPR_EQ:
                case CC_EXPR_NE:
                case CC_EXPR_LT:
                case CC_EXPR_GT:
                case CC_EXPR_LE:
                case CC_EXPR_GE:
                case CC_EXPR_ASSIGN:
                case CC_EXPR_ADDASSIGN:
                case CC_EXPR_SUBASSIGN:
                case CC_EXPR_MULASSIGN:
                case CC_EXPR_DIVASSIGN:
                case CC_EXPR_MODASSIGN:
                case CC_EXPR_BITANDASSIGN:
                case CC_EXPR_BITORASSIGN:
                case CC_EXPR_BITXORASSIGN:
                case CC_EXPR_LSHIFTASSIGN:
                case CC_EXPR_RSHIFTASSIGN:
                case CC_EXPR_TERNARY:
                case CC_EXPR_CAST:
                case CC_EXPR_CALL:
                case CC_EXPR_SUBSCRIPT:
                case CC_EXPR_DOT:
                case CC_EXPR_ARROW:
                case CC_EXPR_COMMA:
                case CC_EXPR_STATEMENT_EXPRESSION:
                case CC_EXPR_ATOMIC:
                case CC_EXPR_VA:
                case CC_EXPR_BUILTIN:
                case CC_EXPR_ADD_OVERFLOW:
                case CC_EXPR_MUL_OVERFLOW:
                case CC_EXPR_SUB_OVERFLOW:
                case CC_EXPR_BIT_BUILTIN:
                case CC_EXPR_ALLOCA:
                case CC_EXPR_INTERN:
                case CC_EXPR_HOTSWAP:
                case CC_EXPR_SRCLOC_REFLECT:
                case CC_EXPR_COMPILE:
                case CC_EXPR_MODULE_REFLECT:
                case CC_EXPR_TYPE_INTROSPECTION:
                case CC_EXPR_UMUL128:
                case CC_EXPR_SLICE:
                case CC_EXPR_SLICE_ALL:
                case CC_EXPR_SLICE_LO:
                case CC_EXPR_SLICE_HI:
                case CC_EXPR_BSWAP:
                    return CC_UNREACHABLE_ERROR;
            }
            CcExpr* node = cc_make_expr(p, kind, tok.loc, result_type, 0);
            if(!node) return CC_OOM_ERROR;
            node->is_lvalue = kind == CC_EXPR_DEREF;
            node->lhs = operand;
            if(kind == CC_EXPR_ADDR || kind == CC_EXPR_PREINC || kind == CC_EXPR_PREDEC)
                cc_mark_mutable_storage(operand);
            *out = node;
            return 0;
        }
    }
    cc_unget(p, &tok);
    CcExpr* primary;
    err = cc_parse_primary(p, flags, &primary, expected);
    if(err) return err;
    return cc_parse_postfix(p, flags, primary, out);
}

static
int
cc_check_atomic_buffer(CcParser* p, CcExpr*_Nullable*_Nonnull expr, CcQualType object, _Bool writable, SrcLoc loc){
    int err = cc_decay_array(p, expr);
    if(err) return err;
    CcQualType type = (*expr)->type;
    if(ccqt_kind(type) != CC_POINTER)
        return cc_types_error(p, loc, type, object, "atomic buffer argument must be a pointer to compatible non-atomic storage");
    CcQualType element = ccqt_as_ptr(type)->pointee;
    CcQualType storage = ccqt_array_object_type(element);
    if(storage.is_atomic || !cc_types_compatible_unqualified(element, object))
        return cc_types_error(p, loc, type, object, "atomic buffer argument must be a pointer to compatible non-atomic storage");
    if(writable && storage.is_const)
        return cc_type_error(p, loc, type, "atomic output buffer must be writable");
    return 0;
}

static
int
cc_parse_atomic_order(CcParser* p, CcParseFlags flags, SrcLoc loc, CcAtomicOp op, _Bool failure, CcMemoryOrder* out){
    flags.unevaluated = 1;
    CcExpr* expr = NULL;
    int err = cc_parse_assignment_expr(p, flags, &expr, CCQT_NONE);
    if(err) return err;
    int64_t value;
    err = cc_eval_integer(&(CcEvalCtx){p}, expr, &value);
    SrcLoc eloc = expr->loc;
    cc_release_expr(p, expr);
    if(err && err != CC_NOT_CONSTANT_ERROR) return err;
    if(err) return cc_error(p, eloc, "memory order must be a constant expression");
    if(value < 0 || value >= CC_MO_COUNT)
        return cc_error(p, eloc, "invalid memory order value %lld", (long long)value);
    unsigned order = (unsigned)value;
    err = cc_check_atomic_memory_order(p, op, order, loc, failure);
    if(err) return err;
    *out = (CcMemoryOrder)order;
    return 0;
}

enum CcAtomicObjectRule {
    CC_AOBJ_READ,
    CC_AOBJ_WRITE,
    CC_AOBJ_ADD_SUB,
    CC_AOBJ_BITWISE,
};
TYPEDEF_ENUM(CcAtomicObjectRule, uint8_t);

enum CcAtomicArgumentKind {
    CC_AARG_END,
    CC_AARG_VALUE,
    CC_AARG_INPUT_BUFFER,
    CC_AARG_OUTPUT_BUFFER,
    CC_AARG_DELTA,
    CC_AARG_WEAK,
};
TYPEDEF_ENUM(CcAtomicArgumentKind, uint8_t);

enum CcAtomicResultKind {
    CC_ARESULT_VALUE,
    CC_ARESULT_VOID,
    CC_ARESULT_BOOL,
};
TYPEDEF_ENUM(CcAtomicResultKind, uint8_t);

typedef struct CcAtomicBuiltinSignature CcAtomicBuiltinSignature;
struct CcAtomicBuiltinSignature {
    CcAtomicObjectRule object;
    CcAtomicResultKind result;
    CcAtomicArgumentKind args[3]; // Arguments after the object, before memory orders.
    uint8_t orders;
};

static const CcAtomicBuiltinSignature cc_atomic_builtin_signatures[] = {
    [CC_ATOMIC_LOAD]               = {CC_AOBJ_READ,    CC_ARESULT_VOID,  {CC_AARG_OUTPUT_BUFFER}, 1},
    [CC_ATOMIC_LOAD_N]             = {CC_AOBJ_READ,    CC_ARESULT_VALUE, {CC_AARG_END}, 1},
    [CC_ATOMIC_STORE]              = {CC_AOBJ_WRITE,   CC_ARESULT_VOID,  {CC_AARG_INPUT_BUFFER}, 1},
    [CC_ATOMIC_STORE_N]            = {CC_AOBJ_WRITE,   CC_ARESULT_VOID,  {CC_AARG_VALUE}, 1},
    [CC_ATOMIC_EXCHANGE]           = {CC_AOBJ_WRITE,   CC_ARESULT_VOID,  {CC_AARG_INPUT_BUFFER, CC_AARG_OUTPUT_BUFFER}, 1},
    [CC_ATOMIC_EXCHANGE_N]         = {CC_AOBJ_WRITE,   CC_ARESULT_VALUE, {CC_AARG_VALUE}, 1},
    [CC_ATOMIC_COMPARE_EXCHANGE]   = {CC_AOBJ_WRITE,   CC_ARESULT_BOOL,  {CC_AARG_OUTPUT_BUFFER, CC_AARG_INPUT_BUFFER, CC_AARG_WEAK}, 2},
    [CC_ATOMIC_COMPARE_EXCHANGE_N] = {CC_AOBJ_WRITE,   CC_ARESULT_BOOL,  {CC_AARG_OUTPUT_BUFFER, CC_AARG_VALUE, CC_AARG_WEAK}, 2},
    [CC_ATOMIC_FETCH_ADD]          = {CC_AOBJ_ADD_SUB, CC_ARESULT_VALUE, {CC_AARG_DELTA}, 1},
    [CC_ATOMIC_FETCH_SUB]          = {CC_AOBJ_ADD_SUB, CC_ARESULT_VALUE, {CC_AARG_DELTA}, 1},
    [CC_ATOMIC_ADD_FETCH]          = {CC_AOBJ_ADD_SUB, CC_ARESULT_VALUE, {CC_AARG_DELTA}, 1},
    [CC_ATOMIC_SUB_FETCH]          = {CC_AOBJ_ADD_SUB, CC_ARESULT_VALUE, {CC_AARG_DELTA}, 1},
    [CC_ATOMIC_FETCH_AND]          = {CC_AOBJ_BITWISE, CC_ARESULT_VALUE, {CC_AARG_VALUE}, 1},
    [CC_ATOMIC_FETCH_OR]           = {CC_AOBJ_BITWISE, CC_ARESULT_VALUE, {CC_AARG_VALUE}, 1},
    [CC_ATOMIC_FETCH_XOR]          = {CC_AOBJ_BITWISE, CC_ARESULT_VALUE, {CC_AARG_VALUE}, 1},
};

static
int
cc_check_atomic_builtin_object(CcParser* p, SrcLoc loc, const CcExpr* pointer, CcAtomicObjectRule rule){
    CcQualType object = ccqt_as_ptr(pointer->type)->pointee;
    if(!ccqt_is_complete_object(object))
        return cc_type_error(p, loc, object, "atomic operation requires a complete object type");
    if(rule != CC_AOBJ_READ && ccqt_array_object_type(object).is_const)
        return cc_type_error(p, loc, object, "atomic operation requires writable object");
    uint32_t size;
    int err = cc_sizeof_as_uint(p, object, loc, &size);
    if(err) return err;
    err = cc_check_atomic_size(p, object, size, loc);
    if(err) return err;
    err = cc_check_atomic_alignment(p, pointer, 1, size, loc);
    if(err) return err;
    if(rule == CC_AOBJ_ADD_SUB || rule == CC_AOBJ_BITWISE){
        if(ccqt_is_bool(object))
            return cc_type_error(p, loc, object, "atomic arithmetic requires non-boolean storage");
        if(size > 8)
            return cc_type_error(p, loc, object, "atomic arithmetic not supported for operand size %u", size);
    }
    switch(rule){
        case CC_AOBJ_READ:
        case CC_AOBJ_WRITE:
            return 0;
        case CC_AOBJ_ADD_SUB:
            if(ccqt_kind(object) != CC_POINTER && !ccqt_is_integer(object))
                return cc_type_error(p, loc, object, "atomic fetch add/sub requires integer or pointer type");
            if(ccqt_kind(object) == CC_POINTER)
                return cc_check_type_has_size_or_error(p, ccqt_as_ptr(object)->pointee, loc);
            return 0;
        case CC_AOBJ_BITWISE:
            if(!ccqt_is_integer(object))
                return cc_type_error(p, loc, object, "atomic fetch bitwise operation requires integer type");
            return 0;
        DRP_CASES_EXHAUSTED;
    }
}

static
int
cc_check_interlocked_pointer(CcParser* p, CcExpr*_Nullable*_Nonnull ptr, CcBasicTypeKind expected, SrcLoc loc){
    int err = cc_decay_array(p, ptr);
    if(err) return err;
    if(ccqt_kind((*ptr)->type) != CC_POINTER)
        return cc_type_error(p, loc, (*ptr)->type, "first argument to interlocked builtin must be a pointer");
    CcQualType object = ccqt_as_ptr((*ptr)->type)->pointee;
    CcExpr* pointer = *ptr;
    err = cc_check_atomic_builtin_object(p, loc, pointer, CC_AOBJ_WRITE);
    if(err) return err;
    if(!cc_types_compatible_unqualified(object, ccqt_basic(expected)))
        return cc_types_error(p, loc, object, ccqt_basic(expected), "interlocked destination has incorrect pointee type");
    return 0;
}

static
int
cc_parse_atomic_argument(CcParser* p, CcParseFlags flags, SrcLoc loc, CcQualType value_type, CcAtomicArgumentKind kind, CcExpr*_Nullable*_Nonnull out){
    int err = cc_expect_punct(p, ',');
    if(err) return err;
    err = cc_parse_assignment_expr(p, flags, out, CCQT_NONE);
    if(err) return err;
    CcQualType target = value_type;
    switch(kind){
        case CC_AARG_INPUT_BUFFER:
        case CC_AARG_OUTPUT_BUFFER:
            return cc_check_atomic_buffer(p, out, value_type, kind == CC_AARG_OUTPUT_BUFFER, loc);
        case CC_AARG_WEAK:
            target = ccqt_basic(CCBT_bool);
            break;
        case CC_AARG_DELTA:
            if(ccqt_kind(value_type) == CC_POINTER)
                target = ccqt_basic(cc_target(p)->ptrdiff_type);
            break;
        case CC_AARG_VALUE:
            break;
        case CC_AARG_END:
            return CC_UNREACHABLE_ERROR;
        DRP_CASES_EXHAUSTED;
    }
    CcExpr* expr = *out;
    return cc_implicit_cast(p, expr, target, out);
}

static
int
cc_parse_atomic_builtin(CcParser* p, CcParseFlags flags, SrcLoc loc, CcAtomicOp op, CcExpr*_Nullable*_Nonnull out){
    if(op >= countof(cc_atomic_builtin_signatures) || !cc_atomic_builtin_signatures[op].orders)
        return CC_UNREACHABLE_ERROR;
    const CcAtomicBuiltinSignature* signature = &cc_atomic_builtin_signatures[op];
    CcExpr* ptr = NULL;
    CcExpr* args[3] = {0};
    int err = cc_expect_punct(p, '(');
    if(err) goto cleanup;
    err = cc_parse_assignment_expr(p, flags, &ptr, CCQT_NONE);
    if(err) goto cleanup;
    err = cc_decay_array(p, &ptr);
    if(err) goto cleanup;
    if(ccqt_kind(ptr->type) != CC_POINTER){
        err = cc_type_error(p, loc, ptr->type, "first argument to atomic builtin must be a pointer");
        goto cleanup;
    }
    CcQualType value_type = ccqt_as_ptr(ptr->type)->pointee;
    err = cc_check_atomic_builtin_object(p, loc, ptr, signature->object);
    if(err) goto cleanup;
    value_type.quals = 0;
    unsigned nargs = 0;
    for(;nargs < countof(args) && signature->args[nargs] != CC_AARG_END; nargs++){
        err = cc_parse_atomic_argument(p, flags, loc, value_type, signature->args[nargs], &args[nargs]);
        if(err) goto cleanup;
    }
    CcMemoryOrder orders[2] = {CC_MO_RELAXED, CC_MO_RELAXED};
    for(unsigned i = 0; i < signature->orders; i++){
        err = cc_expect_punct(p, ',');
        if(err) goto cleanup;
        err = cc_parse_atomic_order(p, flags, loc, op, i == 1, &orders[i]);
        if(err) goto cleanup;
    }
    if(signature->orders == 2){
        CcMemoryOrder maximum = orders[0] == CC_MO_RELEASE ? CC_MO_RELAXED
            : orders[0] == CC_MO_ACQ_REL ? CC_MO_ACQUIRE : orders[0];
        if(orders[1] > maximum){
            err = cc_error(p, loc, "atomic compare-exchange failure order is stronger than success order");
            goto cleanup;
        }
    }
    err = cc_expect_punct(p, ')');
    if(err) goto cleanup;
    CcQualType result = value_type;
    switch(signature->result){
        case CC_ARESULT_VALUE: break;
        case CC_ARESULT_VOID: result = ccqt_basic(CCBT_void); break;
        case CC_ARESULT_BOOL: result = ccqt_basic(CCBT_bool); break;
        DRP_CASES_EXHAUSTED;
    }
    CcExpr* node = cc_make_expr(p, CC_EXPR_ATOMIC, loc, result, nargs);
    if(!node){
        err = CC_OOM_ERROR;
        goto cleanup;
    }
    node->atomic.op = op;
    node->atomic.memorder = orders[0];
    node->atomic.fail_memorder = orders[1];
    node->lhs = ptr;
    for(unsigned i = 0; i < nargs; i++)
        node->values[i] = args[i];
    *out = node;
    return 0;
cleanup:
    if(ptr) cc_release_expr(p, ptr);
    for(unsigned i = 0; i < countof(args); i++)
        if(args[i]) cc_release_expr(p, args[i]);
    return err;
}

static
int
cc_parse_overflow_builtin(CcParser* p, CcParseFlags flags, SrcLoc loc, CcExprKind kind, CcExpr*_Nullable*_Nonnull out){
    CcExpr* args[3] = {0};
    int err = cc_expect_punct(p, '(');
    if(err) goto cleanup;
    for(unsigned i = 0; i < 3; i++){
        if(i){
            err = cc_expect_punct(p, ',');
            if(err) goto cleanup;
        }
        err = cc_parse_assignment_expr(p, flags, &args[i], CCQT_NONE);
        if(err) goto cleanup;
        CcQualType type = args[i]->type;
        if(i == 2){
            err = cc_decay_array(p, &args[i]);
            if(err) goto cleanup;
            if(ccqt_kind(args[i]->type) != CC_POINTER){
                err = cc_type_error(p, args[i]->loc, args[i]->type, "third argument to overflow builtin must be a pointer to integer type");
                goto cleanup;
            }
            type = ccqt_as_ptr(args[i]->type)->pointee;
            if(type.is_const || type.is_atomic){
                err = cc_type_error(p, loc, type, "overflow builtin result must be writable non-atomic storage");
                goto cleanup;
            }
        }
        if(!ccqt_is_integer(type)){
            err = cc_type_error(p, args[i]->loc, args[i]->type, "%s argument to overflow builtin must be %s",
                i == 0 ? "first" : i == 1 ? "second" : "third",
                i == 2 ? "a pointer to integer type" : "an integer type");
            goto cleanup;
        }
        if(i == 2 && ccqt_is_bool(type)){
            err = cc_type_error(p, loc, type, "overflow builtin requires a non-boolean integer result");
            goto cleanup;
        }
        uint32_t size;
        err = cc_sizeof_as_uint(p, type, args[i]->loc, &size);
        if(err) goto cleanup;
        if(size > 8){
            err = cc_error(p, args[i]->loc, "__int128 is not supported for overflow builtins");
            goto cleanup;
        }
    }
    err = cc_expect_punct(p, ')');
    if(err) goto cleanup;
    CcExpr* node = cc_make_expr(p, kind, loc, ccqt_basic(CCBT_bool), 2);
    if(!node){
        err = CC_OOM_ERROR;
        goto cleanup;
    }
    node->lhs = args[0];
    node->values[0] = args[1];
    node->values[1] = args[2];
    *out = node;
    return 0;
cleanup:
    for(unsigned i = 0; i < 3; i++) if(args[i]) cc_release_expr(p, args[i]);
    return err;
}

static
int
cc_check_va_start_context(CcParser* p, SrcLoc loc){
    if(!p->current_func || !p->current_func->type->is_variadic)
        return cc_error(p, loc, "va_start requires a variadic function");
    return 0;
}

static
int
cc_parse_primary(CcParser* p, CcParseFlags flags, CcExpr* _Nullable* _Nonnull out, CcQualType expected){
    CcParseFlags runtime_flags = flags;
    runtime_flags.vc = CC_RUNTIME_VALUE;
    CcParseFlags constexpr_flags = flags;
    constexpr_flags.vc = CC_CONSTEXPR_VALUE;
    CcToken tok;
    int err = cc_next_token(p, &tok);
    if(err) return err;
    switch(tok.type){
        case CC_CONSTANT: {
            CcExpr* node = cc_make_expr(p, CC_EXPR_VALUE, tok.loc, ccqt_basic(tok.constant.ctype), 0);
            if(!node) return CC_OOM_ERROR;
            memcpy(node->data, &tok.constant.integer128_value, sizeof node->data);
            if(tok.constant.is_imaginary){
                CcBasicTypeKind scalar = ccqt_basic_kind(node->type);
                if(ccbt_complex_kind(scalar) == CCBT_INVALID){
                    cc_release_expr(p, node);
                    return cc_unimplemented(p, tok.loc, "extended precision imaginary literal");
                }
                CcExpr* zero = cc_value_expr(p, tok.loc, node->type);
                if(!zero){
                    cc_release_expr(p, node);
                    return CC_OOM_ERROR;
                }
                CcExpr* complex = cc_make_expr(p, CC_EXPR_BUILTIN, tok.loc,
                    ccqt_basic(ccbt_complex_kind(scalar)), 2);
                if(!complex){
                    cc_release_expr(p, node);
                    cc_release_expr(p, zero);
                    return CC_OOM_ERROR;
                }
                complex->builtin.op = CC_BUILTIN_COMPLEX;
                complex->values[0] = zero;
                complex->values[1] = node;
                node = complex;
            }
            *out = node;
            return 0;
        }
        case CC_STRING_LITERAL: {
            CcExpr* node = cc_make_expr(p, CC_EXPR_VALUE, tok.loc, (CcQualType){0}, 0);
            if(!node) return CC_OOM_ERROR;
            node->str.length = tok.str.length;
            node->text = tok.str.utf8;
            CcBasicTypeKind elem_type;
            switch(tok.str.stype){
                case CC_STRING:   elem_type = CCBT_char; break;
                case CC_LSTRING:  elem_type = cc_target(p)->wchar_type; break;
                case CC_uSTRING:  elem_type = cc_target(p)->char16_type; break;
                case CC_USTRING:  elem_type = cc_target(p)->char32_type; break;
                case CC_U8STRING: elem_type = CCBT_unsigned_char; break;
                DRP_CASES_EXHAUSTED;
            }
            CcArray* sa = cc_intern_array(&p->type_cache, cc_allocator(p), ccqt_basic(elem_type), tok.str.length, 0);
            if(!sa) return CC_OOM_ERROR;
            node->type = (CcQualType){.bits = (uintptr_t)sa};
            node->is_lvalue = 1;
            *out = node;
            return 0;
        }
        case CC_IDENTIFIER: {
            CcBuiltinFunc builtin = (CcBuiltinFunc)AM_get(&p->builtins, tok.ident.ident);
            switch(builtin){
                case CC_BUILTIN_NONE:
                    break;
                case CC__builtin_complex: {
                    err = cc_expect_punct(p, '(');
                    if(err) return err;
                    CcExpr* real = NULL;
                    CcExpr* imag = NULL;
                    err = cc_parse_assignment_expr(p, flags, &real, CCQT_NONE);
                    if(err) goto complex_error;
                    err = cc_expect_punct(p, ',');
                    if(err) goto complex_error;
                    err = cc_parse_assignment_expr(p, flags, &imag, CCQT_NONE);
                    if(err) goto complex_error;
                    err = cc_expect_punct(p, ')');
                    if(err) goto complex_error;
                    if(!real || !imag || !ccqt_is_basic(real->type)
                        || !ccqt_is_basic(imag->type)
                        || ccqt_basic_kind(real->type) != ccqt_basic_kind(imag->type)
                        || ccqt_basic_kind(real->type) < CCBT_float16
                        || ccqt_basic_kind(real->type) > CCBT_long_double){
                        err = real && imag
                            ? cc_types_error(p, tok.loc, real->type, imag->type, "__builtin_complex requires two arguments of the same real floating type")
                            : cc_error(p, tok.loc, "__builtin_complex requires two arguments of the same real floating type");
                        goto complex_error;
                    }
                    CcExpr* node = cc_make_expr(p, CC_EXPR_BUILTIN, tok.loc,
                        ccqt_basic(ccbt_complex_kind(ccqt_basic_kind(real->type))), 2);
                    if(!node){
                        err = CC_OOM_ERROR;
                        goto complex_error;
                    }
                    node->builtin.op = CC_BUILTIN_COMPLEX;
                    node->values[0] = real;
                    node->values[1] = imag;
                    *out = node;
                    return 0;
                    complex_error:
                    if(real) cc_release_expr(p, real);
                    if(imag) cc_release_expr(p, imag);
                    return err;
                }
                {
                    CcBuiltinOp op;
                    CcQualType type;
                case CC__builtin_LINE:     op = CC_BUILTIN_LINE;     type = ccqt_basic(CCBT_unsigned); goto builtin_src;
                case CC__builtin_COLUMN:   op = CC_BUILTIN_COLUMN;   type = ccqt_basic(CCBT_unsigned); goto builtin_src;
                case CC__builtin_FILE:     op = CC_BUILTIN_FILE;     type = p->const_char_star;        goto builtin_src;
                case CC__builtin_FUNCTION: op = CC_BUILTIN_FUNCTION; type = p->const_char_star;        goto builtin_src;
                case CC__builtin_SRCLOC:   op = CC_BUILTIN_SRCLOC;   type = p->builtin_src_loc;        goto builtin_src;
                    builtin_src:;
                    err = cc_expect_punct(p, '(');
                    if(err) return err;
                    err = cc_expect_punct(p, ')');
                    if(err) return err;
                    if(!flags.parsing_default_arg) return cc_source_location_expr(p, op, tok.loc, out);
                    CcExpr* node = cc_make_expr(p, CC_EXPR_BUILTIN, tok.loc, type, 0);
                    if(!node) return CC_OOM_ERROR;
                    node->builtin.op = op;
                    *out = node;
                    return 0;
                }
                case CC__builtin_constant_p: {
                    CcParseFlags operand_flags = runtime_flags;
                    operand_flags.unevaluated = 1;
                    err = cc_expect_punct(p, CC_lparen);
                    if(err) return err;
                    CcExpr* arg;
                    err = cc_parse_assignment_expr(p, operand_flags, &arg, CCQT_NONE);
                    if(err) return err;
                    err = cc_expect_punct(p, CC_rparen);
                    if(err) return err;
                    CcExpr* ev;
                    err = cc_eval_expr(&(CcEvalCtx){.parser = p}, arg, &ev);
                    if(!err) cc_release_expr(p, ev);
                    if(err == CC_OVERFLOW_ERROR) err = CC_NOT_CONSTANT_ERROR;
                    if(err && err != CC_NOT_CONSTANT_ERROR) return err;
                    cc_release_expr(p, arg);
                    CcExpr* node = cc_int64_expr(p, tok.loc, ccqt_basic(CCBT_int), err?0:1);
                    if(!node) return CC_OOM_ERROR;
                    *out = node;
                    return 0;
                }
                case CC__builtin_offsetof:{
                    CcParseFlags operand_flags = flags;
                    operand_flags.unevaluated = 1;
                    err = cc_expect_punct(p, CC_lparen);
                    if(err) return err;
                    CcQualType type;
                    err = cc_parse_type_name(p, operand_flags, &type, NULL);
                    if(err) return err;
                    err = cc_expect_punct(p, CC_comma);
                    if(err) return err;
                    uint64_t offset = 0;
                    CcQualType cur = type;
                    for(;;){
                        CcToken member;
                        err = cc_next_token(p, &member);
                        if(err) return err;
                        if(member.type != CC_IDENTIFIER)
                            return cc_error(p, member.loc, "expected member name in __builtin_offsetof");
                        CcTypeKind tk = ccqt_kind(cur);
                        uint64_t floc = 0;
                        CcQualType member_type = {0};
                        CcField* found = NULL;
                        if(tk == CC_STRUCT || tk == CC_UNION){
                            err = cc_lookup_field_offset(p, cur, member.ident.ident, &floc, &member_type, &found);
                            if(err) return err;
                        }
                        if(!found)
                            return cc_type_error(p, member.loc, cur, "no member named '%s' in type", member.ident.ident->data);
                        if(found->is_bitfield || found->is_method)
                            return cc_error(p, member.loc, "__builtin_offsetof requires a non-bitfield data member");
                        offset += floc;
                        cur = member_type;
                        for(;;){
                            CcToken next;
                            err = cc_peek(p, &next);
                            if(err) return err;
                            if(next.type != CC_PUNCTUATOR || next.punct.punct != '[')
                                break;
                            cc_next_token(p, &next);
                            if(!ccqt_is_fixed_sequence(cur))
                                return cc_type_error(p, next.loc, cur, "subscript in __builtin_offsetof requires array type");
                            CcExpr* idx_expr = NULL;
                            err = cc_parse_assignment_expr(p, operand_flags, &idx_expr, CCQT_NONE);
                            if(err) return err;
                            int64_t idx;
                            err = cc_eval_integer(&(CcEvalCtx){p}, idx_expr, &idx);
                            cc_release_expr(p, idx_expr);
                            if(err && err != CC_NOT_CONSTANT_ERROR) return err;
                            if(err)
                                return cc_error(p, next.loc, "array index in __builtin_offsetof must be a constant integer");
                            uint32_t elem_size;
                            err = cc_sizeof_as_uint(p, ccqt_array_element(cur), next.loc, &elem_size);
                            if(err) return err;
                            offset += (uint64_t)idx * elem_size;
                            cur = ccqt_array_element(cur);
                            err = cc_expect_punct(p, CC_rbracket);
                            if(err) return err;
                        }
                        CcToken next;
                        err = cc_peek(p, &next);
                        if(err) return err;
                        if(next.type == CC_PUNCTUATOR && next.punct.punct == '.'){
                            cc_next_token(p, &next);
                            continue;
                        }
                        break;
                    }
                    err = cc_expect_punct(p, CC_rparen);
                    if(err) return err;
                    CcQualType size_type = ccqt_basic(cc_target(p)->size_type);
                    CcExpr* node = cc_value_expr(p, tok.loc, size_type);
                    if(!node) return CC_OOM_ERROR;
                    node->uinteger = offset;
                    *out = node;
                    return 0;
                }
                case CC__builtin_types_compatible_p:{
                    CcParseFlags type_flags = flags;
                    type_flags.unevaluated = 1;
                    err = cc_expect_punct(p, CC_lparen);
                    if(err) return err;
                    CcQualType type1, type2;
                    err = cc_parse_type_name(p, type_flags, &type1, NULL);
                    if(err) return err;
                    err = cc_expect_punct(p, CC_comma);
                    if(err) return err;
                    err = cc_parse_type_name(p, type_flags, &type2, NULL);
                    if(err) return err;
                    err = cc_expect_punct(p, CC_rparen);
                    if(err) return err;
                    _Bool is_same = cc_types_compatible_unqualified(type1, type2);

                    CcExpr* node = cc_value_expr(p, tok.loc, ccqt_basic(CCBT_int));
                    if(!node) return CC_OOM_ERROR;
                    node->integer = is_same;
                    *out = node;
                    return 0;
                }
                case CC__builtin_choose_expr:{
                    CcParseFlags condition_flags = constexpr_flags;
                    condition_flags.unevaluated = 1;
                    err = cc_expect_punct(p, CC_lparen);
                    if(err) return err;
                    CcExpr* cond;
                    err = cc_parse_assignment_expr(p, condition_flags, &cond, CCQT_NONE);
                    if(err) return err;
                    err = cc_decay(p, &cond);
                    if(err) return err;
                    _Bool b;
                    err = cc_eval_truthy(&(CcEvalCtx){p}, cond, &b);
                    if(err) return err;
                    err = cc_expect_punct(p, CC_comma);
                    if(err) return err;
                    CcExpr* result;
                    if(b){
                        err = cc_parse_assignment_expr(p, flags, &result, expected);
                        if(err) return err;
                        err = cc_expect_punct(p, CC_comma);
                        if(err) return err;
                        err = cc_skip_to_next_comma_or_paren(p, "__builtin_choose_expr");
                        if(err) return err;
                    }
                    else {
                        err = cc_skip_to_next_comma_or_paren(p, "__builtin_choose_expr");
                        if(err) return err;
                        err = cc_expect_punct(p, CC_comma);
                        if(err) return err;
                        err = cc_parse_assignment_expr(p, flags, &result, expected);
                        if(err) return err;
                    }
                    err = cc_expect_punct(p, CC_rparen);
                    if(err) return err;
                    *out = result;
                    return 0;
                }
                case CC__func__:{
                    Atom name = p->current_func ? p->current_func->name : NULL;
                    const char* s = name ? name->data : "";
                    uint32_t len = name ? name->length : 0;
                    CcArray* sa = cc_intern_array(&p->type_cache, cc_allocator(p), ccqt_basic(CCBT_char), len + 1, 0);
                    if(!sa) return CC_OOM_ERROR;
                    CcQualType type = {.bits = (uintptr_t)sa};
                    CcExpr* node = cc_value_expr(p, tok.loc, type);
                    if(!node) return CC_OOM_ERROR;
                    node->str.length = len + 1;
                    node->text = s;
                    *out = node;
                    return 0;
                }
                case CC__atomic_load:
                    return cc_parse_atomic_builtin(p, flags, tok.loc, CC_ATOMIC_LOAD, out);
                case CC__atomic_store:
                    return cc_parse_atomic_builtin(p, flags, tok.loc, CC_ATOMIC_STORE, out);
                case CC__atomic_exchange:
                    return cc_parse_atomic_builtin(p, flags, tok.loc, CC_ATOMIC_EXCHANGE, out);
                case CC__atomic_compare_exchange:
                    return cc_parse_atomic_builtin(p, flags, tok.loc, CC_ATOMIC_COMPARE_EXCHANGE, out);
                case CC__atomic_fetch_add:
                    return cc_parse_atomic_builtin(p, flags, tok.loc, CC_ATOMIC_FETCH_ADD, out);
                case CC__atomic_fetch_sub:
                    return cc_parse_atomic_builtin(p, flags, tok.loc, CC_ATOMIC_FETCH_SUB, out);
                case CC__atomic_add_fetch:
                    return cc_parse_atomic_builtin(p, flags, tok.loc, CC_ATOMIC_ADD_FETCH, out);
                case CC__atomic_sub_fetch:
                    return cc_parse_atomic_builtin(p, flags, tok.loc, CC_ATOMIC_SUB_FETCH, out);
                case CC__atomic_fetch_and:
                    return cc_parse_atomic_builtin(p, flags, tok.loc, CC_ATOMIC_FETCH_AND, out);
                case CC__atomic_fetch_or:
                    return cc_parse_atomic_builtin(p, flags, tok.loc, CC_ATOMIC_FETCH_OR, out);
                case CC__atomic_fetch_xor:
                    return cc_parse_atomic_builtin(p, flags, tok.loc, CC_ATOMIC_FETCH_XOR, out);
                case CC__atomic_load_n:
                    return cc_parse_atomic_builtin(p, flags, tok.loc, CC_ATOMIC_LOAD_N, out);
                case CC__atomic_store_n:
                    return cc_parse_atomic_builtin(p, flags, tok.loc, CC_ATOMIC_STORE_N, out);
                case CC__atomic_exchange_n:
                    return cc_parse_atomic_builtin(p, flags, tok.loc, CC_ATOMIC_EXCHANGE_N, out);
                case CC__atomic_compare_exchange_n:
                    return cc_parse_atomic_builtin(p, flags, tok.loc, CC_ATOMIC_COMPARE_EXCHANGE_N, out);
                case CC__atomic_thread_fence:
                case CC__atomic_signal_fence: {
                    enum CcAtomicOp op = (builtin == CC__atomic_thread_fence) ? CC_ATOMIC_THREAD_FENCE : CC_ATOMIC_SIGNAL_FENCE;
                    err = cc_expect_punct(p, '(');
                    if(err) return err;
                    CcMemoryOrder order;
                    err = cc_parse_atomic_order(p, flags, tok.loc, op, 0, &order);
                    if(err) return err;
                    err = cc_expect_punct(p, ')');
                    if(err) return err;
                    CcExpr* node = cc_make_expr(p, CC_EXPR_ATOMIC, tok.loc, ccqt_basic(CCBT_void), 0);
                    if(!node) return CC_OOM_ERROR;
                    node->atomic.op = op;
                    node->atomic.memorder = order;
                    *out = node;
                    return 0;
                }
                case CC_InterlockedExchange:
                case CC_InterlockedExchange8:
                case CC_InterlockedExchange16:
                case CC_InterlockedExchange64:
                case CC_InterlockedCompareExchange:
                case CC_InterlockedCompareExchange8:
                case CC_InterlockedCompareExchange16:
                case CC_InterlockedCompareExchange64:
                case CC_InterlockedExchangeAdd:
                case CC_InterlockedExchangeAdd8:
                case CC_InterlockedExchangeAdd16:
                case CC_InterlockedExchangeAdd64:
                case CC_InterlockedAnd:
                case CC_InterlockedAnd8:
                case CC_InterlockedAnd16:
                case CC_InterlockedAnd64:
                case CC_InterlockedOr:
                case CC_InterlockedOr8:
                case CC_InterlockedOr16:
                case CC_InterlockedOr64:
                case CC_InterlockedXor:
                case CC_InterlockedXor8:
                case CC_InterlockedXor16:
                case CC_InterlockedXor64: {
                    enum CcAtomicOp op;
                    int nargs;
                    CcBasicTypeKind val_kind;
                    switch(builtin){
                        case CC_InterlockedExchange:      op = CC_ATOMIC_EXCHANGE_N; nargs = 1; val_kind = CCBT_long; break;
                        case CC_InterlockedExchange8:     op = CC_ATOMIC_EXCHANGE_N; nargs = 1; val_kind = CCBT_char; break;
                        case CC_InterlockedExchange16:    op = CC_ATOMIC_EXCHANGE_N; nargs = 1; val_kind = CCBT_short; break;
                        case CC_InterlockedExchange64:    op = CC_ATOMIC_EXCHANGE_N; nargs = 1; val_kind = CCBT_long_long; break;
                        case CC_InterlockedCompareExchange:   op = CC_ATOMIC_INTERLOCKED_COMPARE_EXCHANGE; nargs = 2; val_kind = CCBT_long; break;
                        case CC_InterlockedCompareExchange8:  op = CC_ATOMIC_INTERLOCKED_COMPARE_EXCHANGE; nargs = 2; val_kind = CCBT_char; break;
                        case CC_InterlockedCompareExchange16: op = CC_ATOMIC_INTERLOCKED_COMPARE_EXCHANGE; nargs = 2; val_kind = CCBT_short; break;
                        case CC_InterlockedCompareExchange64: op = CC_ATOMIC_INTERLOCKED_COMPARE_EXCHANGE; nargs = 2; val_kind = CCBT_long_long; break;
                        case CC_InterlockedExchangeAdd:   op = CC_ATOMIC_FETCH_ADD; nargs = 1; val_kind = CCBT_long; break;
                        case CC_InterlockedExchangeAdd8:  op = CC_ATOMIC_FETCH_ADD; nargs = 1; val_kind = CCBT_char; break;
                        case CC_InterlockedExchangeAdd16: op = CC_ATOMIC_FETCH_ADD; nargs = 1; val_kind = CCBT_short; break;
                        case CC_InterlockedExchangeAdd64: op = CC_ATOMIC_FETCH_ADD; nargs = 1; val_kind = CCBT_long_long; break;
                        case CC_InterlockedAnd:   op = CC_ATOMIC_FETCH_AND; nargs = 1; val_kind = CCBT_long; break;
                        case CC_InterlockedAnd8:  op = CC_ATOMIC_FETCH_AND; nargs = 1; val_kind = CCBT_char; break;
                        case CC_InterlockedAnd16: op = CC_ATOMIC_FETCH_AND; nargs = 1; val_kind = CCBT_short; break;
                        case CC_InterlockedAnd64: op = CC_ATOMIC_FETCH_AND; nargs = 1; val_kind = CCBT_long_long; break;
                        case CC_InterlockedOr:    op = CC_ATOMIC_FETCH_OR;  nargs = 1; val_kind = CCBT_long; break;
                        case CC_InterlockedOr8:   op = CC_ATOMIC_FETCH_OR;  nargs = 1; val_kind = CCBT_char; break;
                        case CC_InterlockedOr16:  op = CC_ATOMIC_FETCH_OR;  nargs = 1; val_kind = CCBT_short; break;
                        case CC_InterlockedOr64:  op = CC_ATOMIC_FETCH_OR;  nargs = 1; val_kind = CCBT_long_long; break;
                        case CC_InterlockedXor:   op = CC_ATOMIC_FETCH_XOR; nargs = 1; val_kind = CCBT_long; break;
                        case CC_InterlockedXor8:  op = CC_ATOMIC_FETCH_XOR; nargs = 1; val_kind = CCBT_char; break;
                        case CC_InterlockedXor16: op = CC_ATOMIC_FETCH_XOR; nargs = 1; val_kind = CCBT_short; break;
                        case CC_InterlockedXor64: op = CC_ATOMIC_FETCH_XOR; nargs = 1; val_kind = CCBT_long_long; break;
                        default: return CC_UNREACHABLE_ERROR;
                    }
                    CcQualType val_type = ccqt_basic(val_kind);
                    err = cc_expect_punct(p, '(');
                    if(err) return err;
                    CcExpr* ptr_expr;
                    err = cc_parse_assignment_expr(p, flags, &ptr_expr, CCQT_NONE);
                    if(err) return err;
                    err = cc_check_interlocked_pointer(p, &ptr_expr, val_kind, tok.loc);
                    if(err) return err;
                    CcExpr* node = cc_make_expr(p, CC_EXPR_ATOMIC, tok.loc, val_type, nargs);
                    if(!node) return CC_OOM_ERROR;
                    node->atomic.op = op;
                    node->atomic.memorder = CC_MO_SEQ_CST;
                    node->lhs = ptr_expr;
                    for(int i = 0; i < nargs; i++){
                        err = cc_expect_punct(p, ',');
                        if(err) return err;
                        err = cc_parse_assignment_expr(p, flags, &node->values[i], CCQT_NONE);
                        if(err) return err;
                        err = cc_implicit_cast(p, node->values[i], val_type, &node->values[i]);
                        if(err) return err;
                    }
                    err = cc_expect_punct(p, ')');
                    if(err) return err;
                    *out = node;
                    return 0;
                }
                case CC_InterlockedIncrement:
                case CC_InterlockedIncrement16:
                case CC_InterlockedIncrement64:
                case CC_InterlockedDecrement:
                case CC_InterlockedDecrement16:
                case CC_InterlockedDecrement64: {
                    enum CcAtomicOp op;
                    CcBasicTypeKind val_kind;
                    switch(builtin){
                        case CC_InterlockedIncrement:   op = CC_ATOMIC_INTERLOCKED_INCREMENT; val_kind = CCBT_long; break;
                        case CC_InterlockedIncrement16: op = CC_ATOMIC_INTERLOCKED_INCREMENT; val_kind = CCBT_short; break;
                        case CC_InterlockedIncrement64: op = CC_ATOMIC_INTERLOCKED_INCREMENT; val_kind = CCBT_long_long; break;
                        case CC_InterlockedDecrement:   op = CC_ATOMIC_INTERLOCKED_DECREMENT; val_kind = CCBT_long; break;
                        case CC_InterlockedDecrement16: op = CC_ATOMIC_INTERLOCKED_DECREMENT; val_kind = CCBT_short; break;
                        case CC_InterlockedDecrement64: op = CC_ATOMIC_INTERLOCKED_DECREMENT; val_kind = CCBT_long_long; break;
                        default: return CC_UNREACHABLE_ERROR;
                    }
                    err = cc_expect_punct(p, '(');
                    if(err) return err;
                    CcExpr* ptr_expr;
                    err = cc_parse_assignment_expr(p, flags, &ptr_expr, CCQT_NONE);
                    if(err) return err;
                    err = cc_check_interlocked_pointer(p, &ptr_expr, val_kind, tok.loc);
                    if(err) return err;
                    err = cc_expect_punct(p, ')');
                    if(err) return err;
                    CcExpr* node = cc_make_expr(p, CC_EXPR_ATOMIC, tok.loc, ccqt_basic(val_kind), 0);
                    if(!node) return CC_OOM_ERROR;
                    node->atomic.op = op;
                    node->atomic.memorder = CC_MO_SEQ_CST;
                    node->lhs = ptr_expr;
                    *out = node;
                    return 0;
                }
                case CC_InterlockedCompareExchange128: {
                    err = cc_expect_punct(p, '(');
                    if(err) return err;
                    CcExpr* ptr_expr;
                    err = cc_parse_assignment_expr(p, flags, &ptr_expr, CCQT_NONE);
                    if(err) return err;
                    err = cc_check_interlocked_pointer(p, &ptr_expr, CCBT_long_long, tok.loc);
                    if(err) return err;
                    err = cc_check_atomic_size(p, ptr_expr->type, 16, tok.loc);
                    if(err) return err;
                    CcQualType ll_type = ccqt_basic(CCBT_long_long);
                    CcQualType ll_ptr_type;
                    err = cc_pointer_of(p, ll_type, &ll_ptr_type);
                    if(err) return err;
                    CcExpr* node = cc_make_expr(p, CC_EXPR_ATOMIC, tok.loc, ccqt_basic(CCBT_unsigned_char), 3);
                    if(!node) return CC_OOM_ERROR;
                    node->atomic.op = CC_ATOMIC_INTERLOCKED_COMPARE_EXCHANGE128;
                    node->atomic.memorder = CC_MO_SEQ_CST;
                    node->lhs = ptr_expr;
                    err = cc_expect_punct(p, ',');
                    if(err) return err;
                    err = cc_parse_assignment_expr(p, flags, &node->values[0], CCQT_NONE);
                    if(err) return err;
                    err = cc_implicit_cast(p, node->values[0], ll_type, &node->values[0]);
                    if(err) return err;
                    err = cc_expect_punct(p, ',');
                    if(err) return err;
                    err = cc_parse_assignment_expr(p, flags, &node->values[1], CCQT_NONE);
                    if(err) return err;
                    err = cc_implicit_cast(p, node->values[1], ll_type, &node->values[1]);
                    if(err) return err;
                    err = cc_expect_punct(p, ',');
                    if(err) return err;
                    err = cc_parse_assignment_expr(p, flags, &node->values[2], CCQT_NONE);
                    if(err) return err;
                    err = cc_implicit_cast(p, node->values[2], ll_ptr_type, &node->values[2]);
                    if(err) return err;
                    err = cc_expect_punct(p, ')');
                    if(err) return err;
                    *out = node;
                    return 0;
                }
                case CC__umul128: {
                    CcQualType ull_type = ccqt_basic(CCBT_unsigned_long_long);
                    CcQualType ull_ptr_type;
                    err = cc_pointer_of(p, ull_type, &ull_ptr_type);
                    if(err) return err;
                    err = cc_expect_punct(p, '(');
                    if(err) return err;
                    CcExpr* a;
                    err = cc_parse_assignment_expr(p, flags, &a, CCQT_NONE);
                    if(err) return err;
                    err = cc_implicit_cast(p, a, ull_type, &a);
                    if(err) return err;
                    err = cc_expect_punct(p, ',');
                    if(err) return err;
                    CcExpr* b;
                    err = cc_parse_assignment_expr(p, flags, &b, CCQT_NONE);
                    if(err) return err;
                    err = cc_implicit_cast(p, b, ull_type, &b);
                    if(err) return err;
                    err = cc_expect_punct(p, ',');
                    if(err) return err;
                    CcExpr* high_ptr;
                    err = cc_parse_assignment_expr(p, flags, &high_ptr, CCQT_NONE);
                    if(err) return err;
                    err = cc_implicit_cast(p, high_ptr, ull_ptr_type, &high_ptr);
                    if(err) return err;
                    err = cc_expect_punct(p, ')');
                    if(err) return err;
                    CcExpr* node = cc_make_expr(p, CC_EXPR_UMUL128, tok.loc, ull_type, 2);
                    if(!node) return CC_OOM_ERROR;
                    node->lhs = a;
                    node->values[0] = b;
                    node->values[1] = high_ptr;
                    *out = node;
                    return 0;
                }
                case CC__builtin_va_start: {
                    err = cc_check_va_start_context(p, tok.loc);
                    if(err) return err;
                    err = cc_expect_punct(p, '(');
                    if(err) return err;
                    CcExpr* ap_expr;
                    err = cc_parse_assignment_expr(p, flags, &ap_expr, CCQT_NONE);
                    if(err) return err;
                    err = cc_va_list_to_ptr(p, tok.loc, &ap_expr, 1);
                    if(err) return err;
                    CcToken peek;
                    err = cc_peek(p, &peek);
                    if(err) return err;
                    if(peek.type == CC_PUNCTUATOR && peek.punct.punct == ','){
                        err = cc_next_token(p, &peek); // consume comma
                        if(err) return err;
                        CcExpr* ignored;
                        CcParseFlags ignored_flags = flags;
                        ignored_flags.unevaluated = 1;
                        err = cc_parse_assignment_expr(p, ignored_flags, &ignored, CCQT_NONE);
                        if(err) return err;
                        cc_release_expr(p, ignored);
                    }
                    err = cc_expect_punct(p, ')');
                    if(err) return err;
                    CcExpr* node = cc_make_expr(p, CC_EXPR_VA, tok.loc, ccqt_basic(CCBT_void), 0);
                    if(!node) return CC_OOM_ERROR;
                    node->va.op = CC_VA_START;
                    node->lhs = ap_expr;
                    *out = node;
                    return 0;
                }
                case CC__builtin_va_end: {
                    err = cc_expect_punct(p, '(');
                    if(err) return err;
                    CcExpr* ap_expr;
                    err = cc_parse_assignment_expr(p, flags, &ap_expr, CCQT_NONE);
                    if(err) return err;
                    err = cc_expect_punct(p, ')');
                    if(err) return err;
                    err = cc_va_list_to_ptr(p, tok.loc, &ap_expr, 1);
                    if(err) return err;
                    CcExpr* node = cc_make_expr(p, CC_EXPR_VA, tok.loc, ccqt_basic(CCBT_void), 0);
                    if(!node) return CC_OOM_ERROR;
                    node->va.op = CC_VA_END;
                    node->lhs = ap_expr;
                    *out = node;
                    return 0;
                }
                case CC__builtin_va_arg: {
                    err = cc_expect_punct(p, '(');
                    if(err) return err;
                    CcExpr* ap_expr;
                    err = cc_parse_assignment_expr(p, flags, &ap_expr, CCQT_NONE);
                    if(err) return err;
                    err = cc_va_list_to_ptr(p, tok.loc, &ap_expr, 1);
                    if(err) return err;
                    err = cc_expect_punct(p, ',');
                    if(err) return err;
                    CcQualType arg_type;
                    err = cc_parse_type_name(p, flags, &arg_type, NULL);
                    if(err) return err;
                    if(!ccqt_is_complete_object(arg_type))
                        return cc_type_error(p, tok.loc, arg_type, "va_arg requires a complete object type");
                    err = cc_expect_punct(p, ')');
                    if(err) return err;
                    CcExpr* node = cc_make_expr(p, CC_EXPR_VA, tok.loc, arg_type, 0);
                    if(!node) return CC_OOM_ERROR;
                    node->va.op = CC_VA_ARG;
                    node->lhs = ap_expr;
                    *out = node;
                    return 0;
                }
                case CC__builtin_va_copy: {
                    err = cc_expect_punct(p, '(');
                    if(err) return err;
                    CcExpr* dest_expr;
                    err = cc_parse_assignment_expr(p, flags, &dest_expr, CCQT_NONE);
                    if(err) return err;
                    err = cc_va_list_to_ptr(p, tok.loc, &dest_expr, 1);
                    if(err) return err;
                    err = cc_expect_punct(p, ',');
                    if(err) return err;
                    CcExpr* src_expr;
                    err = cc_parse_assignment_expr(p, flags, &src_expr, CCQT_NONE);
                    if(err) return err;
                    err = cc_expect_punct(p, ')');
                    if(err) return err;
                    err = cc_va_list_to_ptr(p, tok.loc, &src_expr, 0);
                    if(err) return err;
                    CcExpr* node = cc_make_expr(p, CC_EXPR_VA, tok.loc, ccqt_basic(CCBT_void), 1);
                    if(!node) return CC_OOM_ERROR;
                    node->va.op = CC_VA_COPY;
                    node->lhs = dest_expr;
                    node->values[0] = src_expr;
                    *out = node;
                    return 0;
                }
                case CC__builtin_expect:{
                    err = cc_expect_punct(p, '(');
                    if(err) return err;
                    CcExpr* value;
                    err = cc_parse_assignment_expr(p, flags, &value, CCQT_NONE);
                    if(err) return err;
                    err = cc_decay(p, &value);
                    if(err) return err;
                    err = cc_expect_punct(p, ',');
                    if(err) return err;
                    CcExpr* unused;
                    err = cc_parse_assignment_expr(p, flags, &unused, CCQT_NONE);
                    if(err) return err;
                    err = cc_decay(p, &unused);
                    if(err) return err;
                    err = cc_implicit_cast(p, unused, ccqt_basic(CCBT_void), &unused);
                    if(err) return err;
                    err = cc_expect_punct(p, ')');
                    if(err) return err;
                    CcExpr* result = cc_binary_expr(p, CC_EXPR_COMMA, tok.loc, value->type, unused, value);
                    if(!result) return CC_OOM_ERROR;
                    result->is_lvalue = value->is_lvalue;
                    *out = result;
                    return 0;
                }
                case CC__builtin_capture_backtrace:{
                    CcBuiltinOp op;
                    CcQualType first, second, ret;
                    op = CC_BUILTIN_CAPTURE_BACKTRACE;
                    err = cc_pointer_of(p, p->void_star, &first);
                    if(err) return err;
                    second = ccqt_basic(CCBT_int);
                    ret = ccqt_basic(CCBT_int);
                    goto parse_trace;
                case CC__builtin_symbolicate:
                    op = CC_BUILTIN_SYMBOLICATE;
                    first = p->void_star;
                    err = cc_pointer_of(p, p->builtin_src_loc, &second);
                    if(err) return err;
                    ret = ccqt_basic(CCBT_void);
                    goto parse_trace;
                case CC__builtin___clear_cache:
                    op = CC_BUILTIN_CLEAR_CACHE;
                    first = second = p->void_star;
                    ret = ccqt_basic(CCBT_void);

                    parse_trace:;
                    CcExpr *first_arg = NULL, *second_arg = NULL;
                    err = cc_expect_punct(p, '(');
                    if(err) return err;
                    err = cc_parse_assignment_expr(p, flags, &first_arg, CCQT_NONE);
                    if(err) goto trace_error;
                    err = cc_implicit_cast(p, first_arg, first, &first_arg);
                    if(err) goto trace_error;
                    err = cc_expect_punct(p, ',');
                    if(err) goto trace_error;
                    err = cc_parse_assignment_expr(p, flags, &second_arg, CCQT_NONE);
                    if(err) goto trace_error;
                    err = cc_implicit_cast(p, second_arg, second, &second_arg);
                    if(err) goto trace_error;
                    err = cc_expect_punct(p, ')');
                    if(err) goto trace_error;
                    CcExpr* node = cc_make_expr(p, CC_EXPR_BUILTIN, tok.loc, ret, 2);
                    if(!node){
                        err = CC_OOM_ERROR;
                        goto trace_error;
                    }
                    node->builtin.op = op;
                    node->values[0] = first_arg;
                    node->values[1] = second_arg;
                    *out = node;
                    return 0;
                    trace_error:
                    if(first_arg) cc_release_expr(p, first_arg);
                    if(second_arg) cc_release_expr(p, second_arg);
                    return err;
                }
                case CC__builtin_unreachable:
                case CC__builtin_readcyclecounter:
                case CC__builtin_trap:
                case CC__builtin_debugtrap:
                case CC__builtin_abort:
                case CC__bt:{
                    err = cc_expect_punct(p, '(');
                    if(err) return err;
                    err = cc_expect_punct(p, ')');
                    if(err) return err;
                    CcBuiltinOp op = CC_BUILTIN_UNREACHABLE;
                    switch(builtin){
                        case CC__builtin_unreachable: op = CC_BUILTIN_UNREACHABLE; break;
                        case CC__builtin_trap:        op = CC_BUILTIN_TRAP; break;
                        case CC__builtin_readcyclecounter: op = CC_BUILTIN_READCYCLECOUNTER; break;
                        case CC__builtin_debugtrap:   op = CC_BUILTIN_DEBUGTRAP; break;
                        case CC__builtin_abort:       op = CC_BUILTIN_ABORT; break;
                        case CC__bt:                  op = CC_BUILTIN_BACKTRACE; break;
                        default:
                        return CC_UNREACHABLE_ERROR;
                    }
                    CcBasicTypeKind ret = op == CC_BUILTIN_READCYCLECOUNTER ? CCBT_unsigned_long_long : CCBT_void;
                    CcExpr* node = cc_make_expr(p, CC_EXPR_BUILTIN, tok.loc, ccqt_basic(ret), 0);
                    if(!node) return CC_OOM_ERROR;
                    node->builtin.op = op;
                    *out = node;
                    return 0;
                }
                case CC__builtin_sub_overflow:{
                    CcExprKind kind;
                    kind = CC_EXPR_SUB_OVERFLOW;
                    goto builtin_overflow;
                case CC__builtin_mul_overflow:
                    kind = CC_EXPR_MUL_OVERFLOW;
                    goto builtin_overflow;
                case CC__builtin_add_overflow:
                    kind = CC_EXPR_ADD_OVERFLOW;
                    builtin_overflow:;
                    return cc_parse_overflow_builtin(p, flags, tok.loc, kind, out);
                }
                {
                    CcBitBuiltinOp op;
                case CC__builtin_popcount: op = CC_BIT_POPCOUNT; goto bit_builtin;
                case CC__builtin_popcountl: op = CC_BIT_POPCOUNT; goto bit_builtin;
                case CC__builtin_popcountll: op = CC_BIT_POPCOUNT; goto bit_builtin;
                case CC__builtin_clz: op = CC_BIT_CLZ; goto bit_builtin;
                case CC__builtin_clzl: op = CC_BIT_CLZ; goto bit_builtin;
                case CC__builtin_clzll: op = CC_BIT_CLZ; goto bit_builtin;
                case CC__builtin_ctz: op = CC_BIT_CTZ; goto bit_builtin;
                case CC__builtin_ctzl: op = CC_BIT_CTZ; goto bit_builtin;
                case CC__builtin_ctzll: op = CC_BIT_CTZ; goto bit_builtin;
                case CC__builtin_ffs: op = CC_BIT_FFS; goto bit_builtin;
                case CC__builtin_ffsl: op = CC_BIT_FFS; goto bit_builtin;
                case CC__builtin_ffsll: op = CC_BIT_FFS; goto bit_builtin;
                case CC__builtin_clrsb: op = CC_BIT_CLRSB; goto bit_builtin;
                case CC__builtin_clrsbl: op = CC_BIT_CLRSB; goto bit_builtin;
                case CC__builtin_clrsbll: op = CC_BIT_CLRSB; goto bit_builtin;
                case CC__builtin_parity: op = CC_BIT_PARITY; goto bit_builtin;
                case CC__builtin_parityl: op = CC_BIT_PARITY; goto bit_builtin;
                case CC__builtin_parityll: op = CC_BIT_PARITY; goto bit_builtin;
                case CC__builtin_ffsg: op = CC_BIT_FFS; goto bit_builtin;
                case CC__builtin_clzg: op = CC_BIT_CLZ; goto bit_builtin;
                case CC__builtin_ctzg: op = CC_BIT_CTZ; goto bit_builtin;
                case CC__builtin_clrsbg: op = CC_BIT_CLRSB; goto bit_builtin;
                case CC__builtin_popcountg: op = CC_BIT_POPCOUNT; goto bit_builtin;
                case CC__builtin_parityg: op = CC_BIT_PARITY; goto bit_builtin;
                case CC__builtin_stdc_bit_ceil: op = CC_BIT_BIT_CEIL; goto bit_builtin;
                case CC__builtin_stdc_bit_floor: op = CC_BIT_BIT_FLOOR; goto bit_builtin;
                case CC__builtin_stdc_bit_width: op = CC_BIT_BIT_WIDTH; goto bit_builtin;
                case CC__builtin_stdc_count_ones: op = CC_BIT_COUNT_ONES; goto bit_builtin;
                case CC__builtin_stdc_count_zeros: op = CC_BIT_COUNT_ZEROS; goto bit_builtin;
                case CC__builtin_stdc_first_leading_one: op = CC_BIT_FIRST_LEADING_ONE; goto bit_builtin;
                case CC__builtin_stdc_first_leading_zero: op = CC_BIT_FIRST_LEADING_ZERO; goto bit_builtin;
                case CC__builtin_stdc_first_trailing_one: op = CC_BIT_FIRST_TRAILING_ONE; goto bit_builtin;
                case CC__builtin_stdc_first_trailing_zero: op = CC_BIT_FIRST_TRAILING_ZERO; goto bit_builtin;
                case CC__builtin_stdc_has_single_bit: op = CC_BIT_HAS_SINGLE_BIT; goto bit_builtin;
                case CC__builtin_stdc_leading_ones: op = CC_BIT_LEADING_ONES; goto bit_builtin;
                case CC__builtin_stdc_leading_zeros: op = CC_BIT_LEADING_ZEROS; goto bit_builtin;
                case CC__builtin_stdc_trailing_ones: op = CC_BIT_TRAILING_ONES; goto bit_builtin;
                case CC__builtin_stdc_trailing_zeros: op = CC_BIT_TRAILING_ZEROS; goto bit_builtin;
                case CC__builtin_stdc_rotate_left: op = CC_BIT_ROTATE_LEFT; goto bit_builtin;
                case CC__builtin_stdc_rotate_right: op = CC_BIT_ROTATE_RIGHT; goto bit_builtin;
                    bit_builtin:;
                    _Bool fixed = (builtin >= CC__builtin_popcount && builtin <= CC__builtin_clzll) || (builtin >= CC__builtin_ffs && builtin <= CC__builtin_parityll);
                    _Bool stdc = builtin >= CC__builtin_stdc_bit_ceil && builtin <= CC__builtin_stdc_rotate_right;
                    _Bool rotate = op == CC_BIT_ROTATE_LEFT || op == CC_BIT_ROTATE_RIGHT;
                    err = cc_expect_punct(p, '(');
                    if(err) return err;
                    CcExpr* arg;
                    err = cc_parse_assignment_expr(p, flags, &arg, CCQT_NONE);
                    if(err) return err;
                    if(fixed){
                        CcBasicTypeKind param;
                        switch(builtin){
                            case CC__builtin_popcount: param = CCBT_unsigned; break;
                            case CC__builtin_popcountl: param = CCBT_unsigned_long; break;
                            case CC__builtin_popcountll: param = CCBT_unsigned_long_long; break;
                            case CC__builtin_clz: param = CCBT_unsigned; break;
                            case CC__builtin_clzl: param = CCBT_unsigned_long; break;
                            case CC__builtin_clzll: param = CCBT_unsigned_long_long; break;
                            case CC__builtin_ctz: param = CCBT_unsigned; break;
                            case CC__builtin_ctzl: param = CCBT_unsigned_long; break;
                            case CC__builtin_ctzll: param = CCBT_unsigned_long_long; break;
                            case CC__builtin_ffsl: case CC__builtin_clrsbl: param = CCBT_long; break;
                            case CC__builtin_ffsll: case CC__builtin_clrsbll: param = CCBT_long_long; break;
                            case CC__builtin_parity: param = CCBT_unsigned; break;
                            case CC__builtin_parityl: param = CCBT_unsigned_long; break;
                            case CC__builtin_parityll: param = CCBT_unsigned_long_long; break;
                            default: param = CCBT_int; break;
                        }
                        err = cc_implicit_cast(p, arg, ccqt_basic(param), &arg);
                        if(err) return err;
                    }
                    else {
                        _Bool signed_arg = op == CC_BIT_FFS || op == CC_BIT_CLRSB;
                        if(!ccqt_is_integer(arg->type) || ccqt_is_bool(arg->type) || (ccqt_is_unsigned(arg->type, !cc_target(p)->char_is_signed) == signed_arg))
                            return cc_type_error(p, arg->loc, arg->type, "bit builtin requires %s integer argument", signed_arg ? "a signed" : "an unsigned");
                        // Load lvalues without applying integral promotions.
                        err = cc_implicit_cast(p, arg, arg->type, &arg);
                        if(err) return err;
                    }
                    CcExpr* second = NULL;
                    CcToken next;
                    err = cc_peek(p, &next);
                    if(err) return err;
                    if(rotate || (!fixed && (op == CC_BIT_CLZ || op == CC_BIT_CTZ) && next.type == CC_PUNCTUATOR && next.punct.punct == ',')){
                        err = cc_expect_punct(p, ',');
                        if(err) return err;
                        err = cc_parse_assignment_expr(p, flags, &second, CCQT_NONE);
                        if(err) return err;
                        if(rotate){
                            if(!ccqt_is_integer(second->type))
                                return cc_type_error(p, second->loc, second->type, "rotation count must have integer type");
                            err = cc_implicit_cast(p, second, second->type, &second);
                        }
                        else {
                            if(!ccqt_bt_eq(second->type, CCBT_int))
                                return cc_type_error(p, second->loc, second->type, "zero fallback must have int type");
                            err = cc_implicit_cast(p, second, ccqt_basic(CCBT_int), &second);
                        }
                        if(err) return err;
                    }
                    err = cc_expect_punct(p, ')');
                    if(err) return err;
                    CcQualType result_type = stdc ? ccqt_basic(CCBT_unsigned) : ccqt_basic(CCBT_int);
                    if(op == CC_BIT_BIT_CEIL || op == CC_BIT_BIT_FLOOR || rotate)
                        result_type = (CcQualType){.unqual = arg->type.unqual};
                    CcExpr* node = cc_make_expr(p, CC_EXPR_BIT_BUILTIN, tok.loc, result_type, second != NULL);
                    if(!node) return CC_OOM_ERROR;
                    node->bit_builtin.op = op;
                    node->bit_builtin.nargs = second != NULL;
                    node->lhs = arg;
                    if(second) node->values[0] = second;
                    *out = node;
                    return 0;
                }

                case CC__builtin_crc32c8:
                case CC__builtin_crc32c16:
                case CC__builtin_crc32c32:
                case CC__builtin_crc32c64:{
                    unsigned index = (unsigned)(builtin - CC__builtin_crc32c8);
                    CcBasicTypeKind types[] = {CCBT_unsigned_char, CCBT_unsigned_short, CCBT_unsigned,
                        ccbt_to_unsigned(cc_target(p)->int64_type)};
                    err = cc_expect_punct(p, '(');
                    if(err) return err;
                    CcExpr *seed, *value;
                    err = cc_parse_assignment_expr(p, flags, &seed, CCQT_NONE);
                    if(err) return err;
                    err = cc_implicit_cast(p, seed, ccqt_basic(CCBT_unsigned), &seed);
                    if(err) return err;
                    err = cc_expect_punct(p, ',');
                    if(err) return err;
                    err = cc_parse_assignment_expr(p, flags, &value, CCQT_NONE);
                    if(err) return err;
                    err = cc_implicit_cast(p, value, ccqt_basic(types[index]), &value);
                    if(err) return err;
                    err = cc_expect_punct(p, ')');
                    if(err) return err;
                    CcExpr* node = cc_make_expr(p, CC_EXPR_BIT_BUILTIN, tok.loc, ccqt_basic(CCBT_unsigned), 1);
                    if(!node) return CC_OOM_ERROR;
                    node->bit_builtin.op = CC_BIT_CRC32C8 + index;
                    node->bit_builtin.nargs = 1;
                    node->lhs = seed;
                    node->values[0] = value;
                    *out = node;
                    return 0;
                }
                case CC__builtin_huge_val:
                case CC__builtin_huge_valf:
                case CC__builtin_huge_vall:{
                    err = cc_expect_punct(p, '(');
                    if(err) return err;
                    err = cc_expect_punct(p, ')');
                    if(err) return err;
                    CcExpr* node;
                    if(builtin == CC__builtin_huge_valf){
                        node = cc_make_expr(p, CC_EXPR_VALUE, tok.loc, ccqt_basic(CCBT_float), 0);
                        if(!node) return CC_OOM_ERROR;
                        node->float_ = INFINITY;
                    }
                    else if(builtin == CC__builtin_huge_val){
                        node = cc_make_expr(p, CC_EXPR_VALUE, tok.loc, ccqt_basic(CCBT_double), 0);
                        if(!node) return CC_OOM_ERROR;
                        node->double_ = (double)INFINITY;
                    }
                    else {
                        node = cc_make_expr(p, CC_EXPR_VALUE, tok.loc, ccqt_basic(CCBT_long_double), 0);
                        if(!node) return CC_OOM_ERROR;
                        node->double_ = (double)INFINITY;
                    }
                    *out = node;
                    return 0;
                }
                case CC__builtin_alloca:{
                    err = cc_expect_punct(p, '(');
                    if(err) return err;
                    CcExpr* sz;
                    err = cc_parse_assignment_expr(p, flags, &sz, CCQT_NONE);
                    if(err) return err;
                    err = cc_implicit_cast(p, sz, ccqt_basic(cc_target(p)->size_type), &sz);
                    if(err) return err;
                    err = cc_expect_punct(p, ')');
                    if(err) return err;
                    CcExpr* node = cc_make_expr(p, CC_EXPR_ALLOCA, tok.loc, p->void_star, 0);
                    if(!node) return CC_OOM_ERROR;
                    node->lhs = sz;
                    *out = node;
                    return 0;
                }
                case CC__builtin_intern:{
                    err = cc_expect_punct(p, '(');
                    if(err) return err;
                    CcExpr* arg;
                    err = cc_parse_assignment_expr(p, flags, &arg, CCQT_NONE);
                    if(err) return err;
                    if(!cc_implicit_convertible(cc_target(p), arg->type, p->const_char_slice))
                        return cc_type_error(p, arg->loc, arg->type, "__builtin_intern argument must be convertible to const char[:]");
                    err = cc_implicit_cast(p, arg, p->const_char_slice, &arg);
                    if(err) return err;
                    err = cc_expect_punct(p, ')');
                    if(err) return err;
                    CcExpr* node = cc_make_expr(p, CC_EXPR_INTERN, tok.loc, p->const_char_slice, 0);
                    if(!node) return CC_OOM_ERROR;
                    node->lhs = arg;
                    *out = node;
                    return 0;
                }
                case CC__compile:{
                    err = cc_expect_punct(p, '(');
                    if(err) return err;
                    CcExpr* arg = NULL, *arg2 = NULL;
                    if(0){
                        __compile_fail:
                        if(arg) cc_release_expr(p, arg);
                        if(arg2) cc_release_expr(p, arg2);
                        return err;
                    }
                    err = cc_parse_assignment_expr(p, flags, &arg, CCQT_NONE);
                    if(err) return err;
                    if(!cc_implicit_convertible(cc_target(p), arg->type, p->const_char_slice)){
                        err = cc_type_error(p, arg->loc, arg->type, "__compile argument must be convertible to const char[:]");
                        goto __compile_fail;
                    }
                    err = cc_implicit_cast(p, arg, p->const_char_slice, &arg);
                    if(err) goto __compile_fail;

                    err = cc_expect_punct(p, ',');
                    if(err) goto __compile_fail;
                    err = cc_parse_assignment_expr(p, flags, &arg2, CCQT_NONE);
                    if(err) goto __compile_fail;
                    if(!cc_implicit_convertible(cc_target(p), arg2->type, p->const_char_slice)){
                        err = cc_type_error(p, arg2->loc, arg2->type, "__compile argument must be convertible to const char[:]");
                        goto __compile_fail;
                    }
                    err = cc_implicit_cast(p, arg2, p->const_char_slice, &arg2);
                    if(err) goto __compile_fail;

                    err = cc_expect_punct(p, ')');
                    if(err)  goto __compile_fail;
                    CcExpr* node = cc_make_expr(p, CC_EXPR_COMPILE, tok.loc, p->builtin_module, 1);
                    if(!node) { err = CC_OOM_ERROR; goto __compile_fail;}
                    node->lhs = arg;
                    node->values[0] = arg2;
                    node->compile.module = p->module;
                    *out = node;
                    return 0;
                }
                case CC__root_module:{
                    err = cc_expect_punct(p, '(');
                    if(err) return err;
                    err = cc_expect_punct(p, ')');
                    if(err) return err;
                    CcExpr* node = cc_value_expr(p, tok.loc, p->builtin_module);
                    if(!node) return CC_OOM_ERROR;
                    node->uinteger = (uintptr_t)p->root;
                    *out = node;
                    return 0;
                }
                case CC__hotswap:{
                    err = cc_expect_punct(p, '(');
                    if(err) return err;
                    CcExpr* old_func;
                    err = cc_parse_assignment_expr(p, flags, &old_func, CCQT_NONE);
                    if(err) return err;
                    if(ccqt_kind(old_func->type) == CC_FUNCTION){
                        CcQualType ptr_type;
                        err = cc_pointer_of(p, old_func->type, &ptr_type);
                        if(err) return err;
                        err = cc_implicit_cast(p, old_func, ptr_type, &old_func);
                        if(err) return err;
                    }
                    if(ccqt_kind(old_func->type) != CC_POINTER
                    || ccqt_kind(ccqt_as_ptr(old_func->type)->pointee) != CC_FUNCTION)
                        return cc_type_error(p, old_func->loc, old_func->type, "__hotswap first argument must be a function pointer");
                    err = cc_expect_punct(p, ',');
                    if(err) return err;
                    CcExpr* new_func;
                    err = cc_parse_assignment_expr(p, flags, &new_func, CCQT_NONE);
                    if(err) return err;
                    if(ccqt_kind(new_func->type) == CC_FUNCTION){
                        CcQualType ptr_type;
                        err = cc_pointer_of(p, new_func->type, &ptr_type);
                        if(err) return err;
                        err = cc_implicit_cast(p, new_func, ptr_type, &new_func);
                        if(err) return err;
                    }
                    if(!cc_types_same((CcQualType){.unqual=new_func->type.unqual},
                                      (CcQualType){.unqual=old_func->type.unqual}))
                        return cc_types_error(p, new_func->loc, old_func->type, new_func->type, "__hotswap function pointer types must match");
                    err = cc_expect_punct(p, ')');
                    if(err) return err;
                    CcExpr* node = cc_make_expr(p, CC_EXPR_HOTSWAP, tok.loc, ccqt_basic(CCBT_int), 1);
                    if(!node) return CC_OOM_ERROR;
                    node->lhs = old_func;
                    node->values[0] = new_func;
                    *out = node;
                    return 0;
                }
                case CC__builtin_nanf:
                case CC__builtin_nan:
                case CC__nan:{
                    err = cc_expect_punct(p, '(');
                    if(err) return err;
                    CcExpr* arg;
                    err = cc_parse_assignment_expr(p, flags, &arg, CCQT_NONE);
                    if(err) return err;
                    err = cc_expect_punct(p, ')');
                    if(err) return err;
                    CcExpr* node;
                    if(builtin == CC__builtin_nanf){
                        node = cc_make_expr(p, CC_EXPR_VALUE, tok.loc, ccqt_basic(CCBT_float), 0);
                        if(!node) return CC_OOM_ERROR;
                        node->float_ = NAN;
                    }
                    else {
                        node = cc_make_expr(p, CC_EXPR_VALUE, tok.loc, ccqt_basic(CCBT_double), 0);
                        if(!node) return CC_OOM_ERROR;
                        node->double_ = (double)NAN;
                    }
                    CcExpr* result = cc_binary_expr(p, CC_EXPR_COMMA, tok.loc, node->type, arg, node);
                    if(!result) return CC_OOM_ERROR;
                    *out = result;
                    return 0;
                }
                case CC__builtin_bswap16:{
                    CcQualType t;
                    t = ccqt_basic(CCBT_unsigned_short);
                    goto bswap;
                case CC__builtin_bswap32:
                    t = ccqt_basic(CCBT_unsigned);
                    goto bswap;
                case CC__builtin_bswap64:
                    t = ccqt_basic(ccbt_to_unsigned(cc_target(p)->int64_type));
                    bswap:
                    err = cc_expect_punct(p, '(');
                    if(err) return err;
                    CcExpr* arg;
                    err = cc_parse_assignment_expr(p, flags, &arg, CCQT_NONE);
                    if(err) return err;
                    err = cc_implicit_cast(p, arg, t, &arg);
                    if(err) return err;
                    err = cc_expect_punct(p, ')');
                    if(err) return err;
                    CcExpr* node = cc_make_expr(p, CC_EXPR_BSWAP, tok.loc, t, 0);
                    if(!node) return CC_OOM_ERROR;
                    node->lhs = arg;
                    *out = node;
                    return 0;
                }
            }
            CcSymbol sym;
            if(!cc_scope_lookup_symbol(p->current, tok.ident.ident, CC_SCOPE_WALK_CHAIN, &sym)){
                return cc_error(p, tok.loc, "undeclared identifier '%.*s'", tok.ident.ident->length, tok.ident.ident->data);
            }
            if(flags.static_rebind && p->static_for){
                CcVariable* binding = cc_scope_lookup_var(p->static_for->bindings, tok.ident.ident, CC_SCOPE_NO_WALK);
                if(binding) sym = (CcSymbol){.kind=CC_SYM_VAR, .var=binding};
            }
            switch(sym.kind){
                case CC_SYM_VAR:{
                    if(flags.vc == CC_CONSTEXPR_VALUE && !flags.static_rebind && !sym.var->constexpr_ && !ccqt_is_fixed_sequence(sym.var->type))
                        return cc_error(p, tok.loc, "expression '%s' is not a constant expression", tok.ident.ident->data);
                    if(flags.vc == CC_LINKTIME_VALUE && sym.var->automatic && !sym.var->constexpr_)
                        return cc_error(p, tok.loc, "expression '%s' is not a constant expression", tok.ident.ident->data);
                    CcExpr* node = cc_make_expr(p, CC_EXPR_VARIABLE, tok.loc, sym.var->type, 0);
                    if(!node) return CC_OOM_ERROR;
                    node->is_lvalue = 1;
                    node->var = sym.var;
                    *out = node;
                    return 0;
                }
                case CC_SYM_FUNC:{
                    CcExpr* node = cc_make_expr(p, CC_EXPR_FUNCTION, tok.loc, (CcQualType){.bits = (uintptr_t)sym.func->type}, 0);
                    if(!node) return CC_OOM_ERROR;
                    node->func = sym.func;
                    *out = node;
                    return 0;
                }
                case CC_SYM_ENUMERATOR:{
                    CcExpr* node = cc_integer_bits_expr(p, tok.loc, sym.enumerator->type, sym.enumerator->value);
                    if(!node) return CC_OOM_ERROR;
                    *out = node;
                    return 0;
                }
                case CC_SYM_TYPEDEF:
                    err = cc_unget(p, &tok);
                    if(err) return err;
                    return cc_parse_lambda(p, flags, tok.loc, out);
            }
            return cc_error(p, tok.loc, "unexpected symbol kind");
        }
        case CC_PUNCTUATOR:
            if(tok.punct.punct == CC_lparen){
                CcToken peek;
                err = cc_peek(p, &peek);
                if(err) return err;
                if(peek.type == CC_PUNCTUATOR && peek.punct.punct == '{'){
                    if(flags.vc == CC_CONSTEXPR_VALUE)
                        return cc_error(p, tok.loc, "statement expression in constant expression");
                    err = cc_next_token(p, &tok);
                    if(err) return err;
                    // statement expression
                    CcDeferContext saved_defer_context = p->defer_context;
                    err = cc_push_scope(p);
                    if(err) return err;
                    CcStmtSink* sink = cc_push_stmt_sink(p);
                    if(!sink){
                        cc_pop_scope(p);
                        return CC_OOM_ERROR;
                    }
                    CcStmtNode* node = NULL;
                    for(;;){
                        err = cc_peek(p, &peek);
                        if(err) goto end_block;
                        if(peek.type == CC_EOF){
                            err = cc_error(p, tok.loc, "Unterminated statement expression");
                            goto end_block;
                        }
                        if(peek.type == CC_PUNCTUATOR && peek.punct.punct == '}'){
                            if(p->static_goto_target){
                                err = cc_static_goto_missing(p);
                                goto end_block;
                            }
                            cc_next_token(p, &peek); // consume '}'
                            break;
                        }
                        err = cc_parse_one(p, flags);
                        if(err) goto end_block;
                    }
                    err = cc_finalize_stmt_list(p, tok.loc, &sink->stmts, 1, &node);
                    if(err) goto end_block;
                    err = cc_stmt_scope_vars(p, node);
                    if(err) goto end_block;
                    node->cleanup = (CcCleanupRange){p->defer_context.active, saved_defer_context.active};
                    end_block:
                    p->defer_context = saved_defer_context;
                    cc_pop_scope(p);
                    cc_pop_stmt_sink(p, sink);
                    if(err){
                        cc_free_stmt_tree(p, node);
                        return err;
                    }
                    err = cc_expect_punct(p, CC_rparen);
                    if(err){
                        cc_free_stmt_tree(p, node);
                        return err;
                    }
                    CcQualType t = ccqt_basic(CCBT_void);
                    if(node->count){
                        CcStmtNode* last = node->stmts[node->count-1];
                        if(last->kind == CC_STMT_EXPR){
                            err = cc_decay(p, &last->exprs[0]);
                            if(err){
                                cc_free_stmt_tree(p, node);
                                return err;
                            }
                            t = last->exprs[0]->type;
                        }
                    }
                    CcExpr* val = cc_make_expr(p, CC_EXPR_STATEMENT_EXPRESSION, tok.loc, t, 0);
                    if(!val){
                        cc_free_stmt_tree(p, node);
                        return CC_OOM_ERROR;
                    }
                    val->stmt_body = node;
                    *out = val;
                    return 0;
                }
                CcExpr* inner;
                err = cc_parse_expr(p, flags, &inner, expected);
                if(err) return err;
                err = cc_expect_punct(p, CC_rparen);
                if(err) return err;
                *out = inner;
                return 0;
            }
            if(tok.punct.punct == CC_lbrace){
                if(!expected.bits)
                    return cc_error(p, tok.loc, "braced initializer requires an expected type");
                err = cc_unget(p, &tok);
                if(err) return err;
                return cc_parse_init_list(p, flags, out, expected);
            }
            return cc_error(p, tok.loc, "Unexpected punctuator in expression");
        case CC_KEYWORD:
            switch(tok.kw.kw){
            case CC_static_assert: {
                err = cc_unget(p, &tok);
                if(err) return err;
                err = cc_handle_static_assert(p, flags);
                if(err) return err;
                CcExpr* v = cc_value_expr(p, tok.loc, ccqt_basic(CCBT_void));
                if(!v) return CC_OOM_ERROR;
                *out = v;
                return 0;
            }
            case CC_sizeof:{
                CcExpr* operand = NULL;
                CcParseFlags sizeof_flags = runtime_flags;
                sizeof_flags.unevaluated |= flags.vc != CC_RUNTIME_VALUE;
                CcQualType type;
                err = cc_parse_type_or_unary_operand(p, sizeof_flags, tok.loc, &type, &operand);
                if(err) return err;
                if(operand && cc_expr_field_bit_width(operand))
                    return cc_error(p, tok.loc, "sizeof applied to bitfield");
                if(!type.ptr)
                    return cc_error(p, tok.loc, "cannot take sizeof incomplete type");
                CcExpr* sz;
                err = cc_sizeof_as_expr(p, type, tok.loc, &sz);
                if(err) return err;
                if(operand && ccqt_kind(type) == CC_VLA){
                    CcExpr* comma = cc_binary_expr(p, CC_EXPR_COMMA, tok.loc, sz->type, operand, sz);
                    if(!comma) return CC_OOM_ERROR;
                    sz = comma;
                }
                else if(operand) cc_release_expr(p, operand);
                *out = sz;
                return 0;
            }
            case CC_alignof:{
                CcParseFlags alignof_flags = runtime_flags;
                alignof_flags.unevaluated = 1;
                CcExpr* operand = NULL;
                CcQualType type;
                err = cc_parse_type_or_unary_operand(p, alignof_flags, tok.loc, &type, &operand);
                if(err) return err;
                if(!operand) return cc_alignof_as_expr(p, type, tok.loc, out);
                uint32_t alignment;
                err = cc_expr_alignment(p, operand, &alignment);
                if(err) return err;
                CcExpr* al = cc_value_expr(p, tok.loc, ccqt_basic(cc_target(p)->size_type));
                if(!al) return CC_OOM_ERROR;
                al->uinteger = alignment;
                cc_release_expr(p, operand);
                *out = al;
                return 0;
            }
            case CC__Countof:{
                CcParseFlags countof_flags = runtime_flags;
                countof_flags.unevaluated |= flags.vc != CC_RUNTIME_VALUE;
                CcExpr* operand = NULL;
                CcQualType arr_type;
                err = cc_parse_type_or_unary_operand(p, countof_flags, tok.loc, &arr_type, &operand);
                if(err) return err;
                if(operand && ccqt_kind(arr_type) == CC_SLICE){
                    if(flags.vc != CC_RUNTIME_VALUE)
                        return cc_error(p, operand->loc, "Can only _Countof a slice at runtime");
                    CcExpr* node = cc_make_expr(p, CC_EXPR_DOT, operand->loc, ccqt_basic(cc_target(p)->size_type), 1);
                    if(!node) return CC_OOM_ERROR;
                    node->field_path = (CcFieldPath){.n_components=1, .idx0=0};
                    node->values[0] = operand;
                    *out = node;
                    return 0;
                }
                if(ccqt_kind(arr_type) == CC_VLA){
                    CcExpr* count;
                    err = cc_clone_expr(p, ccqt_as_vla(arr_type)->bound, 0, tok.loc, 0, &count);
                    if(err) return err;
                    if(operand){
                        CcExpr* comma = cc_binary_expr(p, CC_EXPR_COMMA, tok.loc, count->type, operand, count);
                        if(!comma) return CC_OOM_ERROR;
                        count = comma;
                    }
                    *out = count;
                    return 0;
                }
                if(operand) cc_release_expr(p, operand);
                if(!ccqt_is_fixed_sequence(arr_type))
                    return cc_type_error(p, tok.loc, arr_type, "_Countof requires an array type");
                if(ccqt_array_incomplete(arr_type))
                    return cc_type_error(p, tok.loc, arr_type, "_Countof applied to incomplete array type");
                CcQualType size_type = ccqt_basic(cc_target(p)->size_type);
                CcExpr* node = cc_value_expr(p, tok.loc, size_type);
                if(!node) return CC_OOM_ERROR;
                node->uinteger = (uint64_t)ccqt_sequence_length(arr_type);
                *out = node;
                return 0;
            }
            case CC_true:
            case CC_false:{
                CcExpr* node = cc_value_expr(p, tok.loc, ccqt_basic(CCBT_bool));
                if(!node) return CC_OOM_ERROR;
                node->uinteger = tok.kw.kw == CC_true ? 1 : 0;
                *out = node;
                return 0;
            }
            case CC_nullptr:{
                CcExpr* node = cc_value_expr(p, tok.loc, ccqt_basic(CCBT_nullptr_t));
                if(!node) return CC_OOM_ERROR;
                node->uinteger = 0;
                *out = node;
                return 0;
            }
            case CC__Generic:
                return cc_parse_Generic(p, flags, out, expected);
            case CC__Atomic:
            case CC__BitInt:
            case CC__Complex:
            case CC__Decimal128:
            case CC__Decimal32:
            case CC__Decimal64:
            case CC__Float128:
            case CC___fp16:
            case CC___bf16:
            case CC__Float16:
            case CC__Float32:
            case CC__Float32x:
            case CC__Float64:
            case CC__Float64x:
            case CC__Imaginary:
            case CC__Any:
            case CC__Type:
            case CC__Self:
            case CC___auto_type:
            case CC___int128:
            case CC_bool:
            case CC_char:
            case CC_const:
            case CC_double:
            case CC_enum:
            case CC_float:
            case CC_int:
            case CC_long:
            case CC_restrict:
            case CC_short:
            case CC_signed:
            case CC_struct:
            case CC_typeof:
            case CC_typeof_unqual:
            case CC_union:
            case CC_unsigned:
            case CC_void:
            case CC_volatile:
                err = cc_unget(p, &tok);
                if(err) return err;
                return cc_parse_lambda(p, flags, tok.loc, out);
            case CC___attribute__:
            case CC___declspec:
            case CC__Noreturn:
            case CC_alignas:
            case CC_auto:
            case CC_asm:
            case CC_break:
            case CC_case:
            case CC_constexpr:
            case CC_continue:
            case CC_default:
            case CC_do:
            case CC_else:
            case CC_extern:
            case CC_for:
            case CC_goto:
            case CC_if:
            case CC_inline:
            case CC_register:
            case CC_return:
            case CC__Defer:
            case CC_static:
            case CC_switch:
            case CC_thread_local:
            case CC___forceinline:
            case CC_typedef:
            case CC_while:
                return cc_error(p, tok.loc, "Unexpected keyword in expression");
                DRP_CASES_EXHAUSTED;
            }
        case CC_EMBED:
            return cc_unreachable(p, tok.loc, "embed should be gone by now");
        case CC_EOF:
            return cc_error(p, tok.loc, "Unexpected end of input in expression");
    }
    return cc_error(p, tok.loc, "Unexpected token in expression");
}

static
int
cc_skip_to_next_comma_or_paren(CcParser* p, const char* context){
    int err;
    int depth = 0;
    CcToken tok;
    for(;;){
        err = cc_next_token(p, &tok);
        if(err) return err;
        if(tok.type == CC_EOF)
            return cc_error(p, tok.loc, "unterminated %s", context);
        if(tok.type != CC_PUNCTUATOR) continue;
        switch(tok.punct.punct){
            case '(': case '[': case '{': depth++; break;
            case ')': case ']': case '}':
                if(depth == 0){
                    err = cc_unget(p, &tok);
                    if(err) return err;
                    goto done_skip;
                }
                depth--;
                break;
            case ',':
                if(depth == 0){
                    err = cc_unget(p, &tok);
                    if(err) return err;
                    goto done_skip;
                }
                break;
            case CC_amp:
            case CC_amp_assign:
            case CC_and:
            case CC_arrow:
            case CC_assign:
            case CC_bang:
            case CC_colon:
            case CC_dot:
            case CC_double_colon:
            case CC_ellipsis:
            case CC_eq:
            case CC_ge:
            case CC_gt:
            case CC_le:
            case CC_lshift:
            case CC_lshift_assign:
            case CC_lt:
            case CC_minus:
            case CC_minus_assign:
            case CC_minusminus:
            case CC_ne:
            case CC_or:
            case CC_percent:
            case CC_percent_assign:
            case CC_pipe:
            case CC_pipe_assign:
            case CC_plus:
            case CC_plus_assign:
            case CC_plusplus:
            case CC_question:
            case CC_rshift:
            case CC_rshift_assign:
            case CC_semi:
            case CC_slash:
            case CC_slash_assign:
            case CC_star:
            case CC_star_assign:
            case CC_tilde:
            case CC_xor:
            case CC_xor_assign:
            break;
        }
    }
    done_skip:;
    return 0;
}
static
int
cc_parse_Generic(CcParser* p, CcParseFlags flags, CcExpr*_Nullable*_Nonnull out, CcQualType expected){
    CcParseFlags type_flags = flags;
    type_flags.vc = CC_RUNTIME_VALUE;
    type_flags.unevaluated = 1;
    int err;
    err = cc_expect_punct(p, '(');
    if(err) return err;
    CcQualType tswitch;
    CcToken tok;
    err = cc_peek(p, &tok);
    if(err) return err;
    SrcLoc loc = tok.loc;
    CcExpr* condition;
    err = cc_parse_type_query_operand(p, type_flags, CC_comma, 0, &tswitch, &condition);
    if(err) return err;
    if(condition){
        cc_release_expr(p, condition);
        CcTypeKind tk = ccqt_kind(tswitch);
        switch(tk){
        case CC_VLA: err = cc_pointer_of(p, ccqt_qualified_array_element(tswitch), &tswitch); if(err) return err; break;
        case CC_VECTOR: break;
        case CC_ALIGNED: cc_unsafe_unreachable();
        case CC_ARRAY:{
            err = cc_pointer_of(p, ccqt_qualified_array_element(tswitch), &tswitch);
            if(err) return err;
            break;
        }
        case CC_FUNCTION:{
            err = cc_pointer_of(p, tswitch, &tswitch);
            if(err) return err;
            break;
        }
        case CC_BASIC:
        case CC_ENUM:
        case CC_POINTER:
        case CC_BLOCK_POINTER:
        case CC_STRUCT:
        case CC_UNION:
        case CC_SLICE:
            break;
        }
        tswitch.quals = 0;
    }
    err = cc_expect_punct(p, ',');
    if(err) return err;
    CcExpr* result = NULL;
    Marray(CcToken) default_tokens = {0};
    _Bool has_default = 0;
    for(;;){
        err = cc_peek(p, &tok);
        if(err) goto done;
        _Bool is_default = 0;
        _Bool is_match = 0;
        if(tok.type == CC_KEYWORD && tok.kw.kw == CC_default){
            if(has_default){
                err = cc_error(p, tok.loc, "more than one default in _Generic");
                goto done;
            }
            err = cc_next_token(p, &tok);
            if(err) goto done;
            has_default = 1;
            is_default = 1;
        }
        else {
            CcQualType assoc_type;
            CcParseFlags assoc_flags = type_flags;
            assoc_flags.generic_association = 1;
            err = cc_parse_type_name(p, assoc_flags, &assoc_type, NULL);
            if(err) goto done;
            is_match = cc_types_compatible(assoc_type, tswitch);
        }
        err = cc_expect_punct(p, ':');
        if(err) goto done;
        if(is_match && !result){
            err = cc_parse_assignment_expr(p, flags, &result, expected);
            if(err) goto done;
        }
        else if(is_default && !result){
            int depth = 0;
            for(;;){
                err = cc_next_token(p, &tok);
                if(err) goto done;
                if(tok.type == CC_EOF){
                    err = cc_error(p, tok.loc, "unterminated _Generic");
                    goto done;
                }
                if(tok.type == CC_PUNCTUATOR){
                    if(depth == 0 && (tok.punct.punct == ',' || tok.punct.punct == ')')){
                        err = cc_unget(p, &tok);
                        if(err) goto done;
                        break;
                    }
                    if(tok.punct.punct == '(' || tok.punct.punct == '[' || tok.punct.punct == '{') depth++;
                    else if(tok.punct.punct == ')' || tok.punct.punct == ']' || tok.punct.punct == '}') depth--;
                }
                err = ma_push(CcToken)(&default_tokens, cc_allocator(p), tok);
                if(err){
                    err = CC_OOM_ERROR;
                    goto done;
                }
            }
        }
        else {
            err = cc_skip_to_next_comma_or_paren(p, "_Generic");
            if(err) goto done;
        }
        err = cc_next_token(p, &tok);
        if(err) goto done;
        if(tok.type == CC_PUNCTUATOR && tok.punct.punct == ')')
            break;
        if(tok.type != CC_PUNCTUATOR || tok.punct.punct != ','){
            err = cc_error(p, tok.loc, "expected ',' or ')' in _Generic");
            goto done;
        }
    }
    if(!result && has_default){
        err = ma_push(CcToken)(&default_tokens, cc_allocator(p), (CcToken){.type=CC_EOF});
        if(err){
            err = CC_OOM_ERROR;
            goto done;
        }
        for(size_t i = 0, j = default_tokens.count; i < j; i++){
            j--;
            CcToken tmp = default_tokens.data[i];
            default_tokens.data[i] = default_tokens.data[j];
            default_tokens.data[j] = tmp;
        }
        Marray(CcToken) saved_pending = p->pending;
        p->pending = default_tokens;
        err = cc_parse_assignment_expr(p, flags, &result, expected);
        if(!err){
            err = cc_next_token(p, &tok);
            if(!err && tok.type != CC_EOF)
                err = cc_error(p, tok.loc, "unexpected token after _Generic default expression");
        }
        default_tokens = p->pending;
        p->pending = saved_pending;
        if(err) goto done;
    }
    if(!result){
        err = cc_type_error(p, loc, tswitch, "no matching type in _Generic and no default");
        goto done;
    }
    *out = result;
    done:
    if(err && result) cc_release_expr(p, result);
    ma_cleanup(CcToken)(&default_tokens, cc_allocator(p));
    return err;
}

static
int
cc_pack_call_args(CcParser* p, CcParseFlags flags, SrcLoc loc, CcQualType slice, Parray(CcExpression)* args, uint32_t start){
    uint32_t count = (uint32_t)args->count - start;
    CcQualType element = ccqt_as_slice(slice)->pointee;
    CcArray* array = cc_intern_array(&p->type_cache, cc_allocator(p), element, count, 0);
    if(!array) return CC_OOM_ERROR;
    for(uint32_t i = start; i < args->count; i++){
        CcExpr** arg = (CcExpr**)&args->data[i];
        int err = cc_implicit_cast(p, *arg, element, arg);
        if(err) return err;
    }
    CcInitList* list = Allocator_zalloc_extra(cc_allocator(p), count, sizeof *list->entries, sizeof *list, _Alignof(CcInitList));
    if(!list) return CC_OOM_ERROR;
    CcExpr* literal = cc_make_expr(p, CC_EXPR_COMPOUND_LITERAL, loc, (CcQualType){.bits=(uintptr_t)array}, 0);
    if(!literal){
        Allocator_free_extra(cc_allocator(p), list, count, sizeof *list->entries, sizeof *list, _Alignof(CcInitList));
        return CC_OOM_ERROR;
    }
    list->loc = loc;
    list->count = count;
    for(uint32_t i = 0; i < count; i++)
        list->entries[i] = (CcInitEntry){.path={.n_components=1, .idx0=i}, .value=(CcExpr*)args->data[start+i]};
    literal->init_list = list;
    literal->defer_compound_storage = flags.parsing_default_arg || flags.vc > CC_RUNTIME_VALUE;
    CcExpr* packed;
    int err = cc_implicit_cast(p, literal, slice, &packed);
    if(err) return err;
    args->count = start;
    if(pa_push(args, cc_scratch_allocator(p), packed)) return CC_OOM_ERROR;
    return 0;
}

static
int
cc_parse_typed_pack(CcParser* p, CcParseFlags flags, SrcLoc loc, CcQualType slice, CcExpr* _Nonnull* _Nonnull out){
    Parray(CcExpression) args = {0};
    int err;
    _Bool explicit_pack = 0;
    for(;;){
        CcToken tok;
        err = cc_peek(p, &tok);
        if(err) goto cleanup;
        if(tok.type == CC_PUNCTUATOR && tok.punct.punct == CC_rparen) break;
        if(explicit_pack){
            err = cc_error(p, tok.loc, "cannot combine an explicit typed pack with trailing arguments");
            goto cleanup;
        }
        if(tok.type == CC_PUNCTUATOR && (tok.punct.punct == CC_dot || tok.punct.punct == CC_lbracket)){
            cc_next_token(p, &tok);
            if(tok.punct.punct == CC_dot){
                CcToken name;
                err = cc_next_token(p, &name);
                if(err) goto cleanup;
                if(name.type != CC_IDENTIFIER){
                    err = cc_error(p, name.loc, "expected parameter name after '.'");
                    goto cleanup;
                }
                if(name.ident.ident->length != 4 || memcmp(name.ident.ident->data, "args", 4)){
                    err = cc_error(p, name.loc, "no parameter named '%.*s'", name.ident.ident->length, name.ident.ident->data);
                    goto cleanup;
                }
            }
            else {
                CcExpr* index;
                CcParseFlags index_flags = flags;
                index_flags.unevaluated = 1;
                err = cc_parse_assignment_expr(p, index_flags, &index, CCQT_NONE);
                if(err) goto cleanup;
                int64_t value;
                err = cc_eval_integer(&(CcEvalCtx){p}, index, &value);
                cc_release_expr(p, index);
                if(err){
                    if(err == CC_NOT_CONSTANT_ERROR)
                        err = cc_error(p, tok.loc, "positional designator must be a constant integer expression");
                    goto cleanup;
                }
                if(value != 0){
                    err = cc_error(p, tok.loc, "positional designator value out of range");
                    goto cleanup;
                }
                err = cc_expect_punct(p, CC_rbracket);
                if(err) goto cleanup;
            }
            err = cc_expect_punct(p, CC_assign);
            if(err) goto cleanup;
            if(args.count){
                err = cc_error(p, tok.loc, "cannot combine an explicit typed pack with trailing arguments");
                goto cleanup;
            }
            explicit_pack = 1;
        }
        CcExpr* arg;
        CcQualType type = explicit_pack ? slice : ccqt_as_slice(slice)->pointee;
        err = cc_parse_assignment_expr(p, flags, &arg, type);
        if(err) goto cleanup;
        if(explicit_pack){
            err = cc_implicit_cast(p, arg, slice, &arg);
            if(err) goto cleanup;
        }
        err = pa_push(&args, cc_scratch_allocator(p), arg);
        if(err) goto cleanup;
        err = cc_peek(p, &tok);
        if(err) goto cleanup;
        if(tok.type != CC_PUNCTUATOR || tok.punct.punct != CC_comma) break;
        cc_next_token(p, &tok);
    }
    if(!explicit_pack){
        err = cc_pack_call_args(p, flags, loc, slice, &args, 0);
        if(err) goto cleanup;
    }
    *out = (CcExpr*)args.data[0];
    err = 0;
cleanup:
    if(err){
        for(size_t i = 0; i < args.count; i++)
            cc_release_expr(p, (CcExpr*)args.data[i]);
    }
    pa_cleanup(&args, cc_scratch_allocator(p));
    return err;
}

static
int
cc_parse_postfix(CcParser* p, CcParseFlags flags, CcExpr* operand, CcExpr* _Nullable* _Nonnull out){
    CcParseFlags constexpr_flags = flags;
    constexpr_flags.vc = CC_CONSTEXPR_VALUE;
    CcExpr* _Nullable receiver = NULL;
    for(;;){
        CcToken tok;
        int err = cc_next_token(p, &tok);
        if(err) return err;
        if(tok.type != CC_PUNCTUATOR){
            cc_unget(p, &tok);
            break;
        }
        if(tok.punct.punct != CC_lparen && tok.punct.punct != CC_dot && tok.punct.punct != CC_arrow)
            receiver = NULL;
        if(operand->kind == CC_EXPR_COMPOUND_LITERAL){
            switch((uint32_t)tok.punct.punct){
                case CC_plusplus: case CC_minusminus:
                case CC_lbracket: case CC_dot: case CC_arrow:
                    err = cc_desugar_compound_literal(p, operand, &operand);
                    if(err) return err;
                    break;
                default: break;
            }
        }
        switch((uint32_t)tok.punct.punct){
            case CC_plusplus:
            case CC_minusminus: {
                if(flags.static_rebind && !cc_static_rebind_target(p, operand))
                    return cc_error(p, operand->loc, "static for update must rebind a clause-1 variable");
                if(flags.vc > CC_RUNTIME_VALUE && !flags.static_rebind)
                    return cc_error(p, tok.loc, "increment/decrement in constant expression");
                if(!operand->is_lvalue)
                    return cc_error(p, tok.loc, "expression is not an lvalue");
                if(operand->type.is_const && !flags.static_rebind)
                    return cc_type_error(p, tok.loc, operand->type, "cannot modify const-qualified variable");
                err = cc_check_incdec(p, operand, tok.loc);
                if(err) return err;
                CcExprKind kind = tok.punct.punct == CC_plusplus ? CC_EXPR_POSTINC : CC_EXPR_POSTDEC;
                CcExpr* node = cc_make_expr(p, kind, tok.loc, (CcQualType){.unqual=operand->type.unqual}, 0);
                if(!node) return CC_OOM_ERROR;
                node->lhs = operand;
                cc_mark_mutable_storage(operand);
                operand = node;
                continue;
            }
            case CC_lbracket:{
                CcToken peek;
                err = cc_peek(p, &peek);
                if(err) return err;
                CcExpr* index = NULL;
                if(peek.type != CC_PUNCTUATOR || peek.punct.punct != ':'){
                    err = cc_parse_expr(p, flags, &index, CCQT_NONE);
                    if(err) return err;
                    err = cc_peek(p, &peek);
                    if(err) return err;
                }
                if(peek.type == CC_PUNCTUATOR && peek.punct.punct == ':'){
                    CcQualType elem_type;
                    err = cc_deref_type_or_error(p, operand->type, &elem_type, tok.loc, 0);
                    if(err) return err;
                    err = cc_check_type_has_size_or_error(p, elem_type, tok.loc);
                    if(err) return err;
                    CcQualType slice_type;
                    err = cc_slice_of(p, elem_type, tok.loc, &slice_type);
                    if(err) return err;
                    if(index){
                        err = cc_implicit_cast_to_index(p, index, &index);
                        if(err) return err;
                    }
                    err = cc_next_token(p, &peek);
                    if(err) return err;
                    err = cc_peek(p, &peek);
                    if(err) return err;
                    CcExpr* hi = NULL;
                    if(peek.type == CC_PUNCTUATOR && peek.punct.punct == ']'){
                        if(ccqt_kind(operand->type) == CC_POINTER)
                            return cc_error(p, tok.loc, "slice of pointer requires upper bound");
                        if(ccqt_array_incomplete(operand->type))
                            return cc_error(p, tok.loc, "slice of incomplete array requires upper bound");
                    }
                    else{
                        err = cc_parse_expr(p, flags, &hi, CCQT_NONE);
                        if(err) return err;
                        err = cc_implicit_cast_to_index(p, hi, &hi);
                        if(err) return err;
                    }
                    err = cc_expect_punct(p, CC_rbracket);
                    if(err) return err;
                    CcExprKind kind = index ? (hi ? CC_EXPR_SLICE : CC_EXPR_SLICE_LO)
                        : (hi ? CC_EXPR_SLICE_HI : CC_EXPR_SLICE_ALL);
                    CcExpr* node = cc_make_expr(p, kind, tok.loc, slice_type, !!index + !!hi);
                    if(!node) return CC_OOM_ERROR;
                    node->lhs = operand;
                    if(ccqt_is_array(operand->type)) cc_mark_mutable_storage(operand);
                    if(index || hi) node->values[0] = index ? index : hi;
                    if(index && hi) node->values[1] = hi;
                    operand = node;
                    continue;
                }
                err = cc_expect_punct(p, CC_rbracket);
                if(err) return err;
                {
                    CcQualType idx_type = index->type;
                    _Bool idx_int = ccqt_is_integer(idx_type);
                    _Bool idx_ptr = ccqt_kind(idx_type) == CC_POINTER || ccqt_is_array(idx_type);
                    if(!idx_int && !idx_ptr)
                        return cc_type_error(p, tok.loc, idx_type, "array subscript requires integer or pointer type");
                }
                if(ccqt_is_integer(operand->type)
                    && (ccqt_kind(index->type) == CC_POINTER || ccqt_is_array(index->type))){
                    CcExpr* tmp = operand;
                    operand = index;
                    index = tmp;
                }
                {
                    err = cc_implicit_cast_to_index(p, index, &index);
                    if(err) return err;
                }
                CcQualType elem_type;
                err = cc_deref_type_or_error(p, operand->type, &elem_type, tok.loc, 1);
                if(err) return err;
                err = cc_check_type_has_size_or_error(p, elem_type, tok.loc);
                if(err) return err;
                CcExpr* node = cc_make_expr(p, CC_EXPR_SUBSCRIPT, tok.loc, elem_type, 1);
                if(!node) return CC_OOM_ERROR;
                node->is_lvalue = ccqt_kind(operand->type) != CC_VECTOR || operand->is_lvalue;
                node->lhs = operand;
                node->values[0] = index;
                operand = node;
                continue;
            }
            case CC_dot:
            case CC_arrow: {
                CcExprKind mkind = tok.punct.punct == CC_dot ? CC_EXPR_DOT : CC_EXPR_ARROW;
                if(mkind == CC_EXPR_ARROW){
                    err = cc_decay_array(p, &operand);
                    if(err) return err;
                }
                CcToken member;
                err = cc_next_token(p, &member);
                if(err) return err;
                if(member.type == CC_PUNCTUATOR && member.punct.punct == '['){
                    CcExpr* idx;
                    CcParseFlags index_flags = constexpr_flags;
                    index_flags.unevaluated = 1;
                    err = cc_parse_assignment_expr(p, index_flags, &idx, CCQT_NONE);
                    if(err) return err;
                    int64_t i;
                    err = cc_eval_integer(&(CcEvalCtx){p}, idx, &i);
                    SrcLoc loc = idx->loc;
                    cc_release_expr(p, idx);
                    if(err == CC_NOT_CONSTANT_ERROR)
                        return cc_error(p, loc, "field index must be a constant integer expression");
                    if(err) return err;
                    if(i < 0) return cc_error(p, loc, "field index must be a non-negative integer");
                    err = cc_expect_punct(p, ']');
                    if(err) return err;
                    CcQualType agg_type = operand->type;
                    if(ccqt_kind(agg_type) == CC_POINTER){
                        CcPointer* ptr = ccqt_as_ptr(agg_type);
                        agg_type = ptr->pointee;
                        mkind = CC_EXPR_ARROW;
                    }
                    else {
                        mkind = CC_EXPR_DOT;
                    }
                    agg_type = ccqt_unaligned(agg_type);
                    CcTypeKind tk = ccqt_kind(agg_type);
                    CcField* f;
                    if(tk == CC_STRUCT){
                        CcStruct* s = ccqt_as_struct(agg_type);
                        if(s->is_incomplete)
                            return cc_error(p, loc, "can't access the fields of an incomplete struct");
                        if(i >= s->field_count)
                            return cc_error(p, loc, "field index out of bounds");
                        f = &s->fields[i];
                    }
                    else if(tk == CC_UNION){
                        CcUnion* u = ccqt_as_union(agg_type);
                        if(u->is_incomplete)
                            return cc_error(p, loc, "can't access the fields of an incomplete union");
                        if(i >= u->field_count)
                            return cc_error(p, loc, "field index out of bounds");
                        f = &u->fields[i];
                    }
                    else {
                        MStringBuilder* sb = cc_start_error(p, loc, "field index syntax unsupported on this type: ");
                        cc_print_type(sb, agg_type);
                        return cc_finish_error(p, loc);
                    }
                    if(agg_type.is_atomic)
                        return cc_error(p, member.loc, "member access on atomic struct or union is undefined behavior");
                    CcQualType member_type = f->type;
                    member_type.quals |= agg_type.quals;
                    CcExpr* node = cc_make_expr(p, mkind, tok.loc, member_type, 1);
                    if(!node) return CC_OOM_ERROR;
                    node->is_lvalue = mkind == CC_EXPR_ARROW || operand->is_lvalue;
                    node->field_path = (CcFieldPath){.n_components=1, .idx0=(uint32_t)i};
                    node->values[0] = operand;
                    operand = node;
                    continue;
                }
                if(member.type != CC_IDENTIFIER)
                    return cc_error(p, member.loc, "Expected identifier after '%s'", mkind == CC_EXPR_DOT ? "." : "->");
                Atom member_name = member.ident.ident;
                CcQualType agg_type = operand->type;
                if(ccqt_kind(agg_type) == CC_POINTER){
                    CcPointer* ptr = ccqt_as_ptr(agg_type);
                    agg_type = ptr->pointee;
                    mkind = CC_EXPR_ARROW;
                }
                else {
                    mkind = CC_EXPR_DOT;
                }
                agg_type = ccqt_unaligned(agg_type);
                uint64_t floc = 0;
                CcQualType member_type = {0};
                CcQualType member_owner = {0};
                CcFunc* _Null_unspecified method = NULL;
                CcTypeKind tk = ccqt_kind(agg_type);
                if(tk == CC_STRUCT && p->builtin_src_loc.bits && agg_type.ptr == ccqt_as_ptr(p->builtin_src_loc)->pointee.ptr){
                    StringView name = {member_name->length, member_name->data};
                    CcSrcLocOp op;
                    CcQualType type = ccqt_basic(cc_target(p)->size_type);
                    if(sv_equals(name, SV("file"))){
                        op = CC_SRCLOC_FILE;
                        type = p->const_char_slice;
                    }
                    else if(sv_equals(name, SV("line")))
                        op = CC_SRCLOC_LINE;
                    else if(sv_equals(name, SV("col")))
                        op = CC_SRCLOC_COL;
                    else
                        goto fucs;
                    CcExpr* node = cc_unary_expr(p, CC_EXPR_SRCLOC_REFLECT, tok.loc, type, operand);
                    if(!node) return CC_OOM_ERROR;
                    node->srcloc.op = op;
                    operand = node;
                    continue;
                }
                if(tk == CC_STRUCT && p->builtin_module.bits && agg_type.ptr == ccqt_as_ptr(p->builtin_module)->pointee.ptr){
                    StringView mname = {member_name->length, member_name->data};
                    if(sv_equals(mname, SV("symbol"))){
                        err = cc_expect_punct(p, '(');
                        if(err) return err;
                        CcExpr* name;
                        err = cc_parse_assignment_expr(p, flags, &name, CCQT_NONE);
                        if(err) return err;
                        CcToken next;
                        err = cc_peek(p, &next);
                        if(err) return err;
                        CcQualType result_type = ccqt_basic(CCBT__Any);
                        if(next.type == CC_PUNCTUATOR && next.punct.punct == ','){
                            err = cc_expect_punct(p, ',');
                            if(err) return err;
                            CcQualType symbol_type;
                            err = cc_parse_type_name(p, flags, &symbol_type, NULL);
                            if(err) return err;
                            err = cc_pointer_of(p, symbol_type, &result_type);
                            if(err) return err;
                        }
                        if(!cc_implicit_convertible(cc_target(p), name->type, p->const_char_slice))
                            return cc_types_error(p, name->loc, name->type, p->const_char_slice, "_Module.symbol first argument must be convertible to const char[:]");
                        err = cc_implicit_cast(p, name, p->const_char_slice, &name);
                        if(err) return err;
                        err = cc_expect_punct(p, ')');
                        if(err) return err;
                        CcExpr* node = cc_make_expr(p, CC_EXPR_MODULE_REFLECT, tok.loc, result_type, 1);
                        if(!node) return CC_OOM_ERROR;
                        node->lhs = operand;
                        node->module.op = CC_MODULE_SYMBOL;
                        node->values[0] = name;
                        operand = node;
                        continue;
                    }
                    if(sv_equals(mname, SV("run"))){
                        err = cc_expect_punct(p, '(');
                        if(err) return err;
                        err = cc_expect_punct(p, ')');
                        if(err) return err;
                        CcExpr* node = cc_make_expr(p, CC_EXPR_MODULE_REFLECT, tok.loc, ccqt_basic(CCBT_int), 0);
                        if(!node) return CC_OOM_ERROR;
                        node->module.op = CC_MODULE_RUN;
                        node->lhs = operand;
                        operand = node;
                        continue;
                    }
                    if(sv_equals(mname, SV("parse_type"))){
                        err = cc_expect_punct(p, '(');
                        if(err) return err;
                        CcExpr* name;
                        err = cc_parse_assignment_expr(p, flags, &name, CCQT_NONE);
                        if(err) return err;
                        if(!cc_implicit_convertible(cc_target(p), name->type, p->const_char_slice))
                            return cc_types_error(p, name->loc, name->type, p->const_char_slice, "_Module.parse_type first argument must be convertible to const char[:]");
                        err = cc_implicit_cast(p, name, p->const_char_slice, &name);
                        if(err) return err;
                        err = cc_expect_punct(p, ')');
                        if(err) return err;
                        CcExpr* node = cc_make_expr(p, CC_EXPR_MODULE_REFLECT, tok.loc, ccqt_basic(CCBT__Type), 1);
                        if(!node) return CC_OOM_ERROR;
                        node->module.op = CC_MODULE_PARSE_TYPE;
                        node->lhs = operand;
                        node->values[0] = name;
                        operand = node;
                        continue;
                    }
                    CcModuleOp module_op = CC_MODULE_NONE;
                    CcQualType module_result_type = ccqt_basic(cc_target(p)->size_type);
                    _Bool module_method = 0;
                    if(sv_equals(mname, SV("func_count")))
                        module_op = CC_MODULE_FUNC_COUNT;
                    else if(sv_equals(mname, SV("func"))){
                        module_op = CC_MODULE_FUNC;
                        module_result_type = p->builtin_module_member;
                        module_method = 1;
                    }
                    else if(sv_equals(mname, SV("func_decl"))){
                        module_op = CC_MODULE_FUNC_INFO;
                        module_result_type = p->builtin_module_member;
                        module_method = 1;
                    }
                    else if(sv_equals(mname, SV("var_count")))
                        module_op = CC_MODULE_VAR_COUNT;
                    else if(sv_equals(mname, SV("var"))){
                        module_op = CC_MODULE_VAR;
                        module_result_type = p->builtin_module_member;
                        module_method = 1;
                    }
                    else if(sv_equals(mname, SV("type_count"))){
                        module_op = CC_MODULE_TYPE_COUNT;
                    }
                    else if(sv_equals(mname, SV("type"))){
                        module_op = CC_MODULE_TYPE;
                        module_result_type = p->builtin_module_member;
                        module_method = 1;
                    }
                    if(module_op != CC_MODULE_NONE){
                        CcExpr* node = cc_make_expr(p, CC_EXPR_MODULE_REFLECT, tok.loc, module_result_type, module_method ? 1 : 0);
                        if(!node) return CC_OOM_ERROR;
                        node->lhs = operand;
                        node->module.op = module_op;
                        if(module_method){
                            err = cc_expect_punct(p, '(');
                            if(err) return err;
                            CcExpr* idx;
                            err = cc_parse_assignment_expr(p, flags, &idx, CCQT_NONE);
                            if(err) return err;
                            if(!cc_implicit_convertible(cc_target(p), idx->type, ccqt_basic(cc_target(p)->size_type)))
                                return cc_types_error(p, idx->loc, idx->type, ccqt_basic(cc_target(p)->size_type), "_Module reflection index must be convertible to size_t");
                            err = cc_implicit_cast(p, idx, ccqt_basic(cc_target(p)->size_type), &idx);
                            if(err) return err;
                            err = cc_expect_punct(p, ')');
                            if(err) return err;
                            node->values[0] = idx;
                        }
                        operand = node;
                        continue;
                    }
                    goto fucs;
                }
                if(tk == CC_STRUCT){
                    CcStruct* s = ccqt_as_struct(agg_type);
                    CcField* field;
                    err = cc_lookup_member(p, s, member_name, NULL, &member_type, &member_owner, &field);
                    if(err) return err;
                    if(field && field->is_method) method = field->method;
                    if(member_type.bits) member_type.quals |= agg_type.quals;
                }
                else if(tk == CC_UNION){
                    CcUnion* u = ccqt_as_union(agg_type);
                    CcField* field;
                    err = cc_lookup_member(p, (CcStruct*)u, member_name, NULL, &member_type, &member_owner, &field);
                    if(err) return err;
                    if(field && field->is_method) method = field->method;
                    if(member_type.bits) member_type.quals |= agg_type.quals;
                }
                else if(tk == CC_BASIC && ccqt_basic_kind(agg_type) == CCBT__Type){
                    CcQualType result_type;
                    _Bool is_method = 0;
                    CcTypeIntrospectionOp ti_op = (CcTypeIntrospectionOp)(uintptr_t)AM_get(&p->type_intro, member_name);
                    switch(ti_op){
                    case CC_TYPE_NONE:
                        goto fucs;
                    case CC_TYPE_PUSH_METHOD: {
                        if(cc_scope_declarations(p->current) != &p->module->scope)
                            return cc_error(p, member.loc, "push method only allowed at global scope");
                        CcExpr* tv;
                        err = cc_eval_expr(&(CcEvalCtx){.parser = p}, operand, &tv);
                        if(err == CC_OVERFLOW_ERROR) err = CC_NOT_CONSTANT_ERROR;
                        if(err && err != CC_NOT_CONSTANT_ERROR) return err;
                        if(err || !ccqt_bt_eq(tv->type, CCBT__Type))
                            return cc_error(p, member.loc, "push_method requires a constant type");
                        CcQualType qt = tv->type_value;
                        CcTypeKind k = ccqt_kind(qt);
                        if(k != CC_STRUCT && k != CC_UNION)
                            return cc_type_error(p, member.loc, qt, "push_method requires a struct or union type");
                        err = cc_expect_punct(p, '(');
                        if(err) return err;
                        CcToken mname_tok;
                        err = cc_next_token(p, &mname_tok);
                        if(err) return err;
                        if(mname_tok.type != CC_IDENTIFIER)
                            return cc_error(p, mname_tok.loc, "push_method: expected method name");
                        Atom methods_name = mname_tok.ident.ident;
                        err = cc_expect_punct(p, CC_comma);
                        if(err) return err;
                        CcExpr* func_expr;
                        err = cc_parse_assignment_expr(p, flags, &func_expr, CCQT_NONE);
                        if(err) return err;
                        CcFunc* func;
                        if(func_expr->kind == CC_EXPR_FUNCTION)
                            func = func_expr->func;
                        else
                            return cc_error(p, func_expr->loc, "push_method: expected function");
                        func->name = methods_name;
                        cc_release_expr(p, func_expr);
                        err = cc_expect_punct(p, CC_rparen);
                        if(err) return err;
                        CcStruct* s = ccqt_as_struct(qt);
                        err = cc_append_method(p, s, func, operand->loc);
                        if(err) return err;
                        cc_release_expr(p, operand);
                        CcExpr* dummy = cc_value_expr(p, tok.loc, ccqt_basic(CCBT_void));
                        if(!dummy) return CC_OOM_ERROR;
                        operand = dummy;
                        continue;
                    }
                    case CC_TYPE_FIELD:
                        result_type = p->builtin_field;
                        is_method = 1;
                        break;
                    case CC_TYPE_METHOD:
                        result_type = p->builtin_method;
                        is_method = 1;
                        break;
                    case CC_TYPE_HAS_FIELD:
                    case CC_TYPE_HAS_METHOD:
                        result_type = ccqt_basic(CCBT_bool);
                        is_method = 1;
                        break;
                    case CC_TYPE_ENUMERATOR:
                        result_type = p->builtin_enumerator;
                        is_method = 1;
                        break;
                    case CC_TYPE_ENUMERATORS:
                    case CC_TYPE_FIELDS:
                    case CC_TYPE_METHODS:
                        result_type = ccqt_basic(cc_target(p)->size_type);
                        break;
                    case CC_TYPE_NAME:
                    case CC_TYPE_TAG:
                        result_type = p->const_char_slice;
                        break;
                    case CC_TYPE_IS_VALID:
                    case CC_TYPE_IS_INVALID:
                    case CC_TYPE_IS_INTEGER:
                    case CC_TYPE_IS_FLOAT:
                    case CC_TYPE_IS_ARITHMETIC:
                    case CC_TYPE_IS_POINTER:
                    case CC_TYPE_IS_STRUCT:
                    case CC_TYPE_IS_UNION:
                    case CC_TYPE_IS_ARRAY:
                    case CC_TYPE_IS_VECTOR:
                    case CC_TYPE_IS_SLICE:
                    case CC_TYPE_IS_FUNCTION:
                    case CC_TYPE_IS_ENUM:
                    case CC_TYPE_IS_CONST:
                    case CC_TYPE_IS_VOLATILE:
                    case CC_TYPE_IS_ATOMIC:
                    case CC_TYPE_IS_UNSIGNED:
                    case CC_TYPE_IS_SIGNED:
                    case CC_TYPE_IS_CALLABLE:
                    case CC_TYPE_IS_VARIADIC:
                    case CC_TYPE_IS_INCOMPLETE:
                        result_type = ccqt_basic(CCBT_bool);
                        break;
                    case CC_TYPE_SIZEOF:
                    case CC_TYPE_ALIGNOF:
                    case CC_TYPE_COUNT:
                    case CC_TYPE_PARAM_COUNT:
                        result_type = ccqt_basic(cc_target(p)->size_type);
                        break;
                    case CC_TYPE_LOC:
                        result_type = p->builtin_src_loc;
                        break;
                    case CC_TYPE_POINTEE:
                    case CC_TYPE_UNQUAL:
                    case CC_TYPE_RETURN_TYPE:
                    case CC_TYPE_ELEMENT_TYPE:
                    case CC_TYPE_UNDERLYING_TYPE:
                        result_type = ccqt_basic(CCBT__Type);
                        break;
                    case CC_TYPE_PARAM_TYPE:
                        result_type = ccqt_basic(CCBT__Type);
                        is_method = 1;
                        break;
                    case CC_TYPE_IS_CALLABLE_WITH:
                    case CC_TYPE_IS_CALLABLE_THROUGH:
                    case CC_TYPE_CASTABLE_TO:
                        result_type = ccqt_basic(CCBT_bool);
                        is_method = 1;
                        break;
                    case CC_TYPE_MAKE_ANY:
                        result_type = ccqt_basic(CCBT__Any);
                        is_method= 1;
                        break;
                    }
                    if(is_method){
                        err = cc_expect_punct(p, '(');
                        if(err) return err;
                        CcExpr* arg_val;
                        if(ti_op == CC_TYPE_FIELD || ti_op == CC_TYPE_METHOD
                            || ti_op == CC_TYPE_HAS_FIELD || ti_op == CC_TYPE_HAS_METHOD
                            || ti_op == CC_TYPE_PARAM_TYPE || ti_op == CC_TYPE_ENUMERATOR){
                            CcExpr* arg_expr;
                            err = cc_parse_assignment_expr(p, flags, &arg_expr, CCQT_NONE);
                            if(err) return err;
                            CcQualType size_type = ccqt_basic(cc_target(p)->size_type);
                            CcQualType arg_type = ti_op == CC_TYPE_HAS_FIELD || ti_op == CC_TYPE_HAS_METHOD
                                || ((ti_op == CC_TYPE_FIELD || ti_op == CC_TYPE_METHOD)
                                    && cc_implicit_convertible(cc_target(p), arg_expr->type, p->const_char_slice))
                                ? p->const_char_slice : size_type;
                            err = cc_implicit_cast(p, arg_expr, arg_type, &arg_expr);
                            if(err) return err;
                            arg_val = arg_expr;
                        }
                        else if(ti_op == CC_TYPE_MAKE_ANY){
                            CcExpr* arg_expr;
                            err = cc_parse_assignment_expr(p, flags, &arg_expr, CCQT_NONE);
                            if(err) return err;
                            err = cc_implicit_cast(p, arg_expr, p->const_void_star, &arg_expr);
                            if(err) return err;
                            arg_val = arg_expr;
                        }
                        else if(ti_op == CC_TYPE_IS_CALLABLE_WITH){
                            CcQualType element = ccqt_basic(CCBT__Type), slice;
                            element.is_const = 1;
                            err = cc_slice_of(p, element, tok.loc, &slice);
                            if(err) return err;
                            err = cc_parse_typed_pack(p, flags, tok.loc, slice, &arg_val);
                            if(err) return err;
                        }
                        else if(ti_op == CC_TYPE_IS_CALLABLE_THROUGH){
                            err = cc_parse_assignment_expr(p, flags, &arg_val, CCQT_NONE);
                            if(err) return err;
                            if(!ccqt_bt_eq(arg_val->type, CCBT__Type)){
                                err = cc_type_error(p, tok.loc, arg_val->type, "is_callable_through requires a function type");
                                cc_release_expr(p, arg_val);
                                return err;
                            }
                        }
                        else {
                            CcQualType arg_type;
                            CcParseFlags argument_flags = flags;
                            argument_flags.unevaluated = 1;
                            CcExpr* arg_expr;
                            err = cc_parse_type_query_operand(p, argument_flags, CC_rparen, 0, &arg_type, &arg_expr);
                            if(err) return err;
                            if(arg_expr) cc_release_expr(p, arg_expr);
                            arg_val = cc_make_expr(p, CC_EXPR_VALUE, tok.loc, ccqt_basic(CCBT__Type), 0);
                            if(!arg_val) return CC_OOM_ERROR;
                            arg_val->uinteger = arg_type.bits;
                        }
                        err = cc_expect_punct(p, CC_rparen);
                        if(err) return err;
                        CcExpr* node = cc_make_expr(p, CC_EXPR_TYPE_INTROSPECTION, tok.loc, result_type, 1);
                        if(!node) return CC_OOM_ERROR;
                        node->type_introspection.op = ti_op;
                        node->lhs = operand;
                        node->values[0] = arg_val;
                        operand = node;
                    }
                    else {
                        if(ti_op == CC_TYPE_SIZEOF || ti_op == CC_TYPE_COUNT){
                            CcExpr* known;
                            int known_err = cc_eval_expr(&(CcEvalCtx){p}, operand, &known);
                            if(!known_err){
                                CcQualType reflected = known->type_value;
                                cc_release_expr(p, known);
                                if(ccqt_kind(reflected) == CC_VLA){
                                    CcExpr* value;
                                    err = ti_op == CC_TYPE_SIZEOF ?
                                        cc_sizeof_as_expr(p, reflected, tok.loc, &value)
                                        : cc_clone_expr(p, ccqt_as_vla(reflected)->bound, 0, tok.loc, 0, &value);
                                    if(err) return err;
                                    cc_release_expr(p, operand);
                                    operand = value;
                                    continue;
                                }
                            }
                            else if(known_err != CC_NOT_CONSTANT_ERROR) return known_err;
                        }
                        CcExpr* node = cc_make_expr(p, CC_EXPR_TYPE_INTROSPECTION, tok.loc, result_type, 0);
                        if(!node) return CC_OOM_ERROR;
                        node->type_introspection.op = ti_op;
                        node->lhs = operand;
                        operand = node;
                    }
                    continue;
                }
                else if(ccqt_bt_eq(agg_type, CCBT__Any)){
                    StringView mname = {member_name->length, member_name->data};
                    _Bool payload = sv_equals(mname, SV("payload"));
                    if(payload){
                        member_type = ccqt_basic(CCBT_void);
                        floc = offsetof(CiRtAny, payload);
                    }
                    else if(sv_equals(mname, SV("type")))
                        member_type = ccqt_basic(CCBT__Type);
                    else if(sv_equals(mname, SV("as"))){
                        err = cc_expect_punct(p, CC_lparen);
                        if(err) return err;
                        CcExpr* te;
                        err = cc_parse_assignment_expr(p, constexpr_flags, &te, CCQT_NONE);
                        if(err) return err;
                        CcExpr* tv = NULL;
                        err = cc_eval_expr(&(CcEvalCtx){.parser = p}, te, &tv);
                        cc_release_expr(p, te);
                        if(err) return err == CC_NOT_CONSTANT_ERROR ? cc_error(p, member.loc, "_Any.as requires a constant type") : err;
                        if(!ccqt_bt_eq(tv->type, CCBT__Type)){
                            err = cc_type_error(p, member.loc, tv->type, "_Any.as requires a complete, non-array object type fitting its payload");
                            cc_release_expr(p, tv);
                            return err;
                        }
                        member_type = tv->type_value;
                        cc_release_expr(p, tv);
                        if(ccqt_kind(member_type) == CC_FUNCTION){ // auto-decay to pointer for convenience.
                            err = cc_pointer_of(p, member_type, &member_type);
                            if(err) return err;
                        }
                        if(!cc_any_payload_type(cc_target(p), member_type))
                            return cc_type_error(p, member.loc, member_type, "_Any.as requires a complete, non-array object type fitting its payload");
                        err = cc_expect_punct(p, CC_rparen);
                        if(err) return err;
                        floc = offsetof(CiRtAny, payload);
                    }
                    if(member_type.bits){
                        member_type.quals |= agg_type.quals;
                        CcExpr* field = cc_make_expr(p, mkind, tok.loc, member_type, 1);
                        if(!field) return CC_OOM_ERROR;
                        field->is_lvalue = mkind == CC_EXPR_ARROW || operand->is_lvalue;
                        err = cc_set_member_path(p, field, agg_type, member_name, 0, floc != 0);
                        if(err){
                            _cc_release_expr(p, field, 1);
                            return err;
                        }
                        field->values[0] = operand;
                        operand = field;
                        if(payload){
                            err = cc_check_addressable(p, field, tok.loc);
                            if(err) return err;
                            CcQualType ptr;
                            err = cc_pointer_of(p, member_type, &ptr);
                            if(err) return err;
                            CcExpr* addr = cc_unary_expr(p, CC_EXPR_ADDR, tok.loc, ptr, field);
                            if(!addr) return CC_OOM_ERROR;
                            operand = addr;
                        }
                        continue;
                    }
                }
                else if(tk == CC_SLICE){
                    StringView mname = {member_name->length, member_name->data};
                    if(sv_equals(mname, SV("count")) || sv_equals(mname, SV("length"))){
                        member_type = ccqt_basic(cc_target(p)->size_type);
                        floc = offsetof(CiRtSlice, count);
                    }
                    else if(sv_equals(mname, SV("data"))){
                        err = cc_pointer_of(p, ccqt_as_slice(agg_type)->pointee, &member_type);
                        if(err) return err;
                        // XXX: should this use target?
                        // All of our supported platforms match the host though, so idk.
                        floc = offsetof(CiRtSlice, data);
                    }
                }
                else {
                }
                fucs:;
                if(!member_type.bits){
                    // FUCS: x.foo(args) -> foo(x, args)
                    CcFunc* fucs_func = cc_scope_lookup_func(p->current, member_name, CC_SCOPE_WALK_CHAIN);
                    if(!fucs_func){
                        MStringBuilder* sb = cc_start_error(p, member.loc, "No member named '%s' for '", member_name->data);
                        cc_print_type(sb, agg_type);
                        if(tk == CC_STRUCT || tk == CC_UNION)
                            msb_sprintf(sb, "'");
                        else msb_sprintf(sb, "' (not a struct or union)");
                        return cc_finish_error(p, member.loc);
                    }
                    CcExpr* fnode = cc_make_expr(p, CC_EXPR_FUNCTION, tok.loc, (CcQualType){.bits = (uintptr_t)fucs_func->type}, 0);
                    if(!fnode) return CC_OOM_ERROR;
                    fnode->func = fucs_func;
                    // Prefer normal argument conversion (including decay and boxing)
                    // before adapting a receiver by address or dereference.
                    if(fucs_func->type->param_count > 0 && cc_implicit_convertible(cc_target(p), operand->type, fucs_func->type->params[0])){
                        receiver = operand;
                    }
                    else {
                        err = cc_adapt_receiver(p, fucs_func->type, operand, tok.loc, &receiver);
                        if(err) return err;
                    }
                    operand = fnode;
                    continue;
                }
                if(agg_type.is_atomic && (tk == CC_STRUCT || tk == CC_UNION))
                    return cc_error(p, member.loc, "member access on atomic struct or union is undefined behavior");
                if(method){
                    if(member_owner.bits){
                        member_owner.quals |= agg_type.quals;
                        CcExpr* subobject = cc_make_expr(p, mkind, tok.loc, member_owner, 1);
                        if(!subobject) return CC_OOM_ERROR;
                        subobject->is_lvalue = mkind == CC_EXPR_ARROW || operand->is_lvalue;
                        err = cc_set_member_path(p, subobject, agg_type, member_name, 1, 0);
                        if(err){
                            _cc_release_expr(p, subobject, 1);
                            return err;
                        }
                        subobject->values[0] = operand;
                        operand = subobject;
                    }
                    CcExpr* mnode = cc_make_expr(p, CC_EXPR_FUNCTION, tok.loc, member_type, 0);
                    if(!mnode) return CC_OOM_ERROR;
                    mnode->func = method;
                    err = cc_adapt_receiver(p, method->type, operand, tok.loc, &receiver);
                    if(err) return err;
                    operand = mnode;
                    continue;
                }
                member_type.quals |= agg_type.quals;
                CcExpr* mnode = cc_make_expr(p, mkind, tok.loc, member_type, 1);
                if(!mnode) return CC_OOM_ERROR;
                mnode->is_lvalue = mkind == CC_EXPR_ARROW || operand->is_lvalue;
                err = cc_set_member_path(p, mnode, agg_type, member_name, 0, floc != 0);
                if(err){
                    _cc_release_expr(p, mnode, 1);
                    return err;
                }
                mnode->values[0] = operand;
                operand = mnode;
                continue;
            }
            case CC_lparen: {
                if(flags.vc > CC_RUNTIME_VALUE)
                    return cc_error(p, tok.loc, "function call in constant expression");
                CcFunction* ftype = ccqt_callable_function(operand->type);
                if(!ftype)
                    return cc_type_error(p, tok.loc, operand->type, "Called object is not a function or function pointer");
                if(!ccqt_bt_eq(ftype->return_type, CCBT_void) && !ccqt_is_complete_object(ftype->return_type))
                    return cc_type_error(p, tok.loc, ftype->return_type, "function call requires a complete return type");
                CcToken peek;
                err = cc_next_token(p, &peek);
                if(err) return err;
                uint32_t pack_index = UINT32_MAX;
                if(operand->kind == CC_EXPR_FUNCTION){
                    for(size_t i = 0; i < operand->func->params.count; i++)
                        if(operand->func->params.data[i].typed_pack) pack_index = (uint32_t)i;
                }
                _Bool typed_pack = pack_index != UINT32_MAX;
                if(peek.type == CC_PUNCTUATOR && peek.punct.punct == CC_rparen && !receiver && !typed_pack
                    && !(operand->kind == CC_EXPR_FUNCTION && operand->func->params.count)){
                    if(!cc_function_accepts_arity(ftype, 0)){
                        if(ftype->is_variadic)
                            return cc_type_error(p, tok.loc, operand->type, "Too few arguments: expected at least %u, got 0", (unsigned)ftype->param_count);
                        return cc_type_error(p, tok.loc, operand->type, "Expected %u arguments, got 0", (unsigned)ftype->param_count);
                    }
                    CcExpr* node = cc_make_expr(p, CC_EXPR_CALL, tok.loc, ftype->return_type, 0);
                    if(!node) return CC_OOM_ERROR;
                    node->lhs = operand;
                    operand = node;
                    continue;
                }
                _Bool rparen_consumed = 0;
                if(peek.type == CC_PUNCTUATOR && peek.punct.punct == CC_rparen)
                    rparen_consumed = 1;
                else
                    cc_unget(p, &peek);
                CcFuncParam*_Null_unspecified param_names = NULL;
                size_t param_names_count = 0;
                if(operand->kind == CC_EXPR_FUNCTION && operand->func->params.count){
                    param_names = operand->func->params.data;
                    param_names_count = operand->func->params.count;
                }
                Allocator call_al = cc_scratch_allocator(p);
                Parray(CcExpression) args = {0};
                Parray(CcExpression) pack_args = {0};
                _Bool has_receiver = receiver != NULL;
                if(receiver){
                    err = pa_push(&args, call_al, receiver);
                    if(err) return CC_OOM_ERROR;
                    receiver = NULL;
                }
                _Bool has_named = 0;
                _Bool explicit_pack = 0;
                uint32_t positional_index = has_receiver ? 1 : 0;
                if(rparen_consumed) goto call_args_done;
                for(;;){
                    CcToken dot;
                    err = cc_peek(p, &dot);
                    if(err) goto call_cleanup;
                    if(dot.type == CC_PUNCTUATOR && dot.punct.punct == CC_dot){
                        cc_next_token(p, &dot);
                        CcToken name_tok;
                        err = cc_next_token(p, &name_tok);
                        if(err) goto call_cleanup;
                        if(name_tok.type != CC_IDENTIFIER){
                            err = cc_error(p, name_tok.loc, "expected parameter name after '.'");
                            goto call_cleanup;
                        }
                        CcToken eq;
                        err = cc_next_token(p, &eq);
                        if(err) goto call_cleanup;
                        if(eq.type != CC_PUNCTUATOR || eq.punct.punct != CC_assign){
                            err = cc_error(p, eq.loc, "expected '=' after parameter name");
                            goto call_cleanup;
                        }
                        if(!param_names){
                            err = cc_error(p, dot.loc, "named arguments require a function with known parameter names");
                            goto call_cleanup;
                        }
                        Atom name = name_tok.ident.ident;
                        uint32_t idx = UINT32_MAX;
                        for(size_t j = 0; j < param_names_count; j++){
                            if(param_names[j].name == name){
                                idx = (uint32_t)j;
                                break;
                            }
                        }
                        if(idx == UINT32_MAX){
                            err = cc_error(p, name_tok.loc, "no parameter named '%.*s'", name->length, name->data);
                            goto call_cleanup;
                        }
                        if(idx == pack_index && pack_args.count){
                            err = cc_error(p, name_tok.loc, "cannot combine an explicit typed pack with trailing arguments");
                            goto call_cleanup;
                        }
                        // Parse the value
                        CcExpr* arg;
                        err = cc_parse_assignment_expr(p, flags, &arg, ftype->params[idx]);
                        if(err) goto call_cleanup;
                        // Ensure args array is big enough
                        if(args.count <= idx){
                            err = pa_zextend(&args, call_al, idx-args.count+1);
                            if(err) goto call_cleanup;
                        }
                        if(args.data[idx] != NULL){
                            err = cc_error(p, name_tok.loc, "duplicate argument for parameter '%.*s'", name->length, name->data);
                            goto call_cleanup;
                        }
                        args.data[idx] = arg;
                        if(idx == pack_index) explicit_pack = 1;
                        has_named = 1;
                    }
                    else if(dot.type == CC_PUNCTUATOR && dot.punct.punct == CC_lbracket){
                        cc_next_token(p, &dot);
                        CcExpr* idx_expr;
                        CcParseFlags index_flags = flags;
                        index_flags.unevaluated = 1;
                        err = cc_parse_assignment_expr(p, index_flags, &idx_expr, CCQT_NONE);
                        if(err) goto call_cleanup;
                        int64_t idx_signed;
                        err = cc_eval_integer(&(CcEvalCtx){p}, idx_expr, &idx_signed);
                        cc_release_expr(p, idx_expr);
                        if(err){
                            if(err == CC_NOT_CONSTANT_ERROR)
                                err = cc_error(p, dot.loc, "positional designator must be a constant integer expression");
                            goto call_cleanup;
                        }
                        if(idx_signed < 0 || (uint64_t)idx_signed >= ftype->param_count){
                            err = cc_error(p, dot.loc, "positional designator value out of range");
                            goto call_cleanup;
                        }
                        uint32_t idx = (uint32_t)idx_signed;
                        if(idx == pack_index && pack_args.count){
                            err = cc_error(p, dot.loc, "cannot combine an explicit typed pack with trailing arguments");
                            goto call_cleanup;
                        }
                        err = cc_expect_punct(p, CC_rbracket);
                        if(err) goto call_cleanup;
                        CcToken eq;
                        err = cc_next_token(p, &eq);
                        if(err) goto call_cleanup;
                        if(eq.type != CC_PUNCTUATOR || eq.punct.punct != CC_assign){
                            err = cc_error(p, eq.loc, "expected '=' after positional designator");
                            goto call_cleanup;
                        }
                        CcExpr* arg;
                        err = cc_parse_assignment_expr(p, flags, &arg, ftype->params[idx]);
                        if(err) goto call_cleanup;
                        if(args.count <= idx){
                            err = pa_zextend(&args, call_al, idx-args.count+1);
                            if(err) goto call_cleanup;
                        }
                        if(args.data[idx] != NULL){
                            err = cc_error(p, dot.loc, "duplicate argument for position %u", (unsigned)idx);
                            goto call_cleanup;
                        }
                        args.data[idx] = arg;
                        if(idx == pack_index) explicit_pack = 1;
                        has_named = 1;
                    }
                    else {
                        if(has_named){
                            while(positional_index < pack_index && positional_index < args.count && args.data[positional_index] != NULL)
                                positional_index++;
                        }
                        CcExpr* arg;
                        if(explicit_pack && positional_index >= pack_index){
                            err = cc_error(p, dot.loc, "cannot combine an explicit typed pack with trailing arguments");
                            goto call_cleanup;
                        }
                        CcQualType hint = positional_index < ftype->param_count ? ftype->params[positional_index] : CCQT_NONE;
                        if(typed_pack && positional_index >= pack_index)
                            hint = ccqt_as_slice(ftype->params[pack_index])->pointee;
                        err = cc_parse_assignment_expr(p, flags, &arg, hint);
                        if(err) goto call_cleanup;
                        if(typed_pack && positional_index >= pack_index){
                            err = pa_push(&pack_args, call_al, arg);
                            if(err){
                                cc_release_expr(p, arg);
                                err = CC_OOM_ERROR;
                                goto call_cleanup;
                            }
                        }
                        else if(has_named){
                            if(args.count <= positional_index){
                                err = pa_zextend(&args, call_al, positional_index-args.count+1);
                                if(err) goto call_cleanup;
                            }
                            if(args.data[positional_index] != NULL){
                                err = cc_error(p, arg->loc, "argument position %u already filled by named argument", (unsigned)positional_index);
                                goto call_cleanup;
                            }
                            args.data[positional_index] = arg;
                        }
                        else {
                            err = pa_push(&args, call_al, arg);
                            if(err){
                                err = CC_OOM_ERROR;
                                goto call_cleanup;
                            }
                        }
                        positional_index++;
                    }
                    CcToken sep;
                    err = cc_next_token(p, &sep);
                    if(err) goto call_cleanup;
                    if(sep.type == CC_PUNCTUATOR && sep.punct.punct == CC_rparen)
                        break;
                    if(sep.type != CC_PUNCTUATOR || sep.punct.punct != CC_comma){
                        err = cc_error(p, sep.loc, "Expected ',' or ')' in function call");
                        goto call_cleanup;
                    }
                    err = cc_peek(p, &sep);
                    if(err) goto call_cleanup;
                    if(sep.type == CC_PUNCTUATOR && sep.punct.punct == CC_rparen){
                        cc_next_token(p, &sep);
                        break;
                    }
                }
                call_args_done:;
                SrcLoc call_loc = operand->loc;
                uint32_t fixed_count = ftype->param_count;
                if(param_names){
                    for(uint32_t i = 0; i < fixed_count && i < param_names_count; i++){
                        if(i == pack_index) continue;
                        if(i < args.count && args.data[i]) continue;
                        CcExpr* default_value = param_names[i].default_value;
                        if(!default_value) continue;
                        if(args.count <= i){
                            err = pa_zextend(&args, call_al, i-args.count+1);
                            if(err){
                                err = CC_OOM_ERROR;
                                goto call_cleanup;
                            }
                        }
                        err = cc_clone_default_arg(p, default_value, call_loc, flags, (CcExpr**)&args.data[i]);
                        if(err) goto call_cleanup;
                    }
                }
                if(typed_pack && !explicit_pack){
                    if(args.count < pack_index){
                        err = cc_type_error(p, tok.loc, operand->type, "Too few arguments: expected at least %u, got %u", pack_index, (unsigned)args.count);
                        goto call_cleanup;
                    }
                    if(args.count < ftype->param_count){
                        err = pa_zextend(&args, call_al, ftype->param_count-args.count);
                        if(err){
                            err = CC_OOM_ERROR;
                            goto call_cleanup;
                        }
                    }
                    err = cc_pack_call_args(p, flags, tok.loc, ftype->params[pack_index], &pack_args, 0);
                    if(err) goto call_cleanup;
                    args.data[pack_index] = pack_args.data[0];
                    pack_args.count = 0;
                }
                uint32_t nargs = (uint32_t)args.count;
                for(uint32_t i = 0; i < nargs && i < ftype->param_count; i++){
                    if(!args.data[i]){
                        Atom pn = (param_names && i < param_names_count) ? param_names[i].name : NULL;
                        err = cc_error(p, tok.loc, "missing argument for parameter '%.*s'",
                            pn ? (int)pn->length : 1, pn ? pn->data : "?");
                        goto call_cleanup;
                    }
                }
                if(!cc_function_accepts_arity(ftype, nargs)){
                    if(ftype->is_variadic){
                        err = cc_type_error(p, tok.loc, operand->type, "Too few arguments: expected at least %u, got %u", (unsigned)ftype->param_count, (unsigned)nargs);
                        goto call_cleanup;
                    }
                    err = cc_type_error(p, tok.loc, operand->type, "Expected %u arguments, got %u", (unsigned)ftype->param_count, (unsigned)nargs);
                    goto call_cleanup;
                }
                for(uint32_t i = 0; i < nargs; i++){
                    CcExpr** argp = (CcExpr**)&args.data[i];
                    if(!cc_is_value_operand_type((*argp)->type)){
                        err = cc_type_error(p, (*argp)->loc, (*argp)->type, "argument must have a complete object type");
                        goto call_cleanup;
                    }
                    if(!ftype->no_prototype && i < ftype->param_count){
                        err = cc_implicit_cast(p, *argp, ftype->params[i], argp);
                        if(err) goto call_cleanup;
                    }
                    else {
                        CcQualType at;
                        cc_arithmetic_operand_type(cc_target(p), *argp, &at);
                        if(ccqt_is_array(at) || ccqt_kind(at) == CC_FUNCTION){
                            err = cc_decay(p, argp);
                            if(err) goto call_cleanup;
                        }
                        else {
                            err = cc_implicit_cast(p, *argp, cc_default_promotion(at), argp);
                            if(err) goto call_cleanup;
                        }
                    }
                }
                if(operand->kind == CC_EXPR_FUNCTION){
                    for(const CcPrintfFormat* format = operand->func->printf_formats; format; format = format->next){
                        uint32_t fi = format->string_index - 1;
                        SrcLoc fmt_loc = fi < nargs ? ((CcExpr*)args.data[fi])->loc : tok.loc;
                        err = cc_check_printf_format(p, format, (CcExpr*_Nonnull*_Nonnull)args.data, nargs, fmt_loc);
                        if(err) goto call_cleanup;
                    }
                }
                CcExpr* node = cc_make_expr(p, CC_EXPR_CALL, tok.loc, ftype->return_type, nargs);
                if(!node){
                    err = CC_OOM_ERROR;
                    goto call_cleanup;
                }
                node->call.nargs = nargs;
                node->lhs = operand;
                memcpy(node->values, args.data, nargs * sizeof(CcExpr*));
                operand = node;
                err = 0;
                call_cleanup:
                if(err){
                    for(size_t i = 0; i < args.count; i++)
                        if(args.data[i]) cc_release_expr(p, args.data[i]);
                    for(size_t i = 0; i < pack_args.count; i++)
                        if(pack_args.data[i]) cc_release_expr(p, pack_args.data[i]);
                }
                pa_cleanup(&args, call_al);
                pa_cleanup(&pack_args, call_al);
                if(err) return err;
                continue;
            }
            default:
                cc_unget(p, &tok);
                goto done;
        }
    }
done:
    *out = operand;
    return 0;
}

// Collect one statement without resolving names or evaluating expressions.
// This is shared by skipped static branches and replayed static-loop bodies.
static int cc_static_statement_tokens(CcParser*, Marray(CcToken)*, SrcLoc);

static
int
cc_static_collect_token(CcParser* p, Marray(CcToken)* tokens, CcToken* tok){
    int err = cc_next_token(p, tok);
    if(err) return err;
    if(tok->type == CC_EOF) return cc_error(p, tok->loc, "unterminated static statement");
    return ma_push(CcToken)(tokens, cc_allocator(p), *tok) ? CC_OOM_ERROR : 0;
}

static
int
cc_static_collect_group(CcParser* p, Marray(CcToken)* tokens, CcPunct open, CcPunct close, SrcLoc loc){
    CcToken tok;
    int err = cc_static_collect_token(p, tokens, &tok);
    if(err) return err;
    if(tok.type != CC_PUNCTUATOR || tok.punct.punct != open)
        return cc_error(p, loc, "expected delimiter in static statement");
    unsigned depth = 1;
    while(depth){
        err = cc_static_collect_token(p, tokens, &tok);
        if(err) return err;
        if(tok.type != CC_PUNCTUATOR) continue;
        if(tok.punct.punct == open) depth++;
        else if(tok.punct.punct == close) depth--;
    }
    return 0;
}

static
int
cc_static_statement_tokens(CcParser* p, Marray(CcToken)* tokens, SrcLoc loc){
    CcToken tok;
    while(cc_is_c23_attribute_start(p)){
        int err = cc_static_collect_group(p, tokens, '[', ']', loc);
        if(err) return err;
    }
    int err = cc_peek(p, &tok);
    if(err) return err;
    if(tok.type == CC_PUNCTUATOR && tok.punct.punct == '{')
        return cc_static_collect_group(p, tokens, '{', '}', loc);
    err = cc_static_collect_token(p, tokens, &tok);
    if(err) return err;
    if(tok.type == CC_KEYWORD && tok.kw.kw == CC_static){
        err = cc_static_collect_token(p, tokens, &tok);
        if(err) return err;
    }
    if(tok.type == CC_KEYWORD && tok.kw.kw == CC__Defer)
        return cc_static_statement_tokens(p, tokens, loc);
    if(tok.type == CC_KEYWORD && (tok.kw.kw == CC_if || tok.kw.kw == CC_for
       || tok.kw.kw == CC_while || tok.kw.kw == CC_switch)){
        _Bool is_if = tok.kw.kw == CC_if;
        err = cc_static_collect_group(p, tokens, '(', ')', loc);
        if(err) return err;
        err = cc_static_statement_tokens(p, tokens, loc);
        if(err || !is_if) return err;
        err = cc_peek(p, &tok);
        if(err) return err;
        if(tok.type != CC_KEYWORD || tok.kw.kw != CC_else) return 0;
        err = cc_static_collect_token(p, tokens, &tok);
        if(err) return err;
        return cc_static_statement_tokens(p, tokens, loc);
    }
    if(tok.type == CC_KEYWORD && tok.kw.kw == CC_do){
        err = cc_static_statement_tokens(p, tokens, loc);
        if(err) return err;
        err = cc_static_collect_token(p, tokens, &tok);
        if(err) return err;
        if(tok.type != CC_KEYWORD || tok.kw.kw != CC_while)
            return cc_error(p, tok.loc, "Expected 'while' after do body");
        err = cc_static_collect_group(p, tokens, '(', ')', loc);
        if(err) return err;
        err = cc_static_collect_token(p, tokens, &tok);
        if(err) return err;
        if(tok.type != CC_PUNCTUATOR || tok.punct.punct != ';')
            return cc_error(p, tok.loc, "expected ';' after do statement");
        return 0;
    }
    if(tok.type == CC_IDENTIFIER){
        CcToken peek;
        err = cc_peek(p, &peek);
        if(err) return err;
        if(peek.type == CC_PUNCTUATOR && peek.punct.punct == ':'){
            err = cc_static_collect_token(p, tokens, &peek);
            if(err) return err;
            return cc_static_statement_tokens(p, tokens, loc);
        }
    }
    if(tok.type == CC_KEYWORD && (tok.kw.kw == CC_case || tok.kw.kw == CC_default)){
        unsigned depth = 0, conditional = 0;
        for(;;){
            err = cc_static_collect_token(p, tokens, &tok);
            if(err) return err;
            if(tok.type != CC_PUNCTUATOR) continue;
            switch((uint32_t)tok.punct.punct){
                case '(': case '[': case '{': depth++; break;
                case ')': case ']': case '}':
                    if(!depth) return cc_error(p, tok.loc, "expected ':' after case label");
                    depth--;
                    break;
                case '?': if(!depth) conditional++; break;
                case ':':
                    if(!depth){
                        if(conditional) conditional--;
                        else return cc_static_statement_tokens(p, tokens, loc);
                    }
                    break;
                default: break;
            }
        }
    }
    unsigned parens = 0, brackets = 0, braces = 0;
    _Bool declaration = tok.type == CC_IDENTIFIER || cc_is_type_start(p, &tok);
    _Bool assignment = 0, function_body = 0;
    uint32_t last_punct = 0;
    for(;;){
        if(tok.type == CC_PUNCTUATOR){
            switch((uint32_t)tok.punct.punct){
                case '(': parens++; break;
                case ')': if(!parens) return cc_error(p, tok.loc, "unbalanced static statement"); parens--; break;
                case '[': brackets++; break;
                case ']': if(!brackets) return cc_error(p, tok.loc, "unbalanced static statement"); brackets--; break;
                case '=': if(!parens && !brackets && !braces) assignment = 1; break;
                case '{':
                    if(!parens && !brackets && !braces && declaration && !assignment && last_punct == ')')
                        function_body = 1;
                    braces++;
                    break;
                case '}':
                    if(!braces) return cc_error(p, tok.loc, "expected ';' in static statement");
                    braces--;
                    if(function_body && !braces && !parens && !brackets) return 0;
                    break;
                case ';': if(!parens && !brackets && !braces) return 0; break;
                default: break;
            }
        }
        last_punct = tok.type == CC_PUNCTUATOR ? tok.punct.punct : 0;
        err = cc_static_collect_token(p, tokens, &tok);
        if(err) return err;
    }
}

static
int
cc_skip_static_statement(CcParser* p, SrcLoc loc){
    Marray(CcToken) tokens = {0};
    int err = cc_static_statement_tokens(p, &tokens, loc);
    ma_cleanup(CcToken)(&tokens, cc_allocator(p));
    return err;
}

static
int
cc_skip_static_else_chain(CcParser* p){
    int err;
    CcToken tok;
    for(;;){
        err = cc_peek(p, &tok);
        if(err) return err;
        if(tok.type != CC_KEYWORD || tok.kw.kw != CC_else)
            return 0;
        cc_next_token(p, &tok);
        err = cc_peek(p, &tok);
        if(err) return err;
        if(tok.type == CC_KEYWORD && tok.kw.kw == CC_if){
            cc_next_token(p, &tok);
            err = cc_expect_punct(p, '(');
            if(err) return err;
            int depth = 1;
            while(depth > 0){
                err = cc_next_token(p, &tok);
                if(err) return err;
                if(tok.type == CC_EOF) return cc_error(p, tok.loc, "unterminated static if condition");
                if(tok.type == CC_PUNCTUATOR){
                    if(tok.punct.punct == '(') depth++;
                    else if(tok.punct.punct == ')') depth--;
                }
            }
            err = cc_skip_static_statement(p, tok.loc);
            if(err) return err;
            continue;
        }
        return cc_skip_static_statement(p, tok.loc);
    }
}

static
int
cc_parse_static_if_body(CcParser* p, CcParseFlags flags, SrcLoc loc){
    CcToken first;
    int err = cc_peek(p, &first);
    if(err) return err;
    if(first.type != CC_PUNCTUATOR || first.punct.punct != '{')
        return cc_parse_one(p, flags);
    err = cc_expect_punct(p, '{');
    if(err) return err;
    for(;;){
        CcToken peek;
        err = cc_peek(p, &peek);
        if(err) return err;
        if(peek.type == CC_EOF) return cc_error(p, loc, "unterminated static if block");
        if(peek.type == CC_PUNCTUATOR && peek.punct.punct == '}'){
            cc_next_token(p, &peek);
            break;
        }
        err = cc_parse_one(p, flags);
        if(err) return err;
    }
    return 0;
}

static
int
cc_parse_static_if_inner(CcParser* p, CcParseFlags flags, SrcLoc loc);

static
int
cc_parse_static_if(CcParser* p, CcParseFlags flags, SrcLoc loc){
    int err = cc_push_scope(p);
    if(err) return err;
    CcScope* scope = p->current;
    err = cc_parse_static_if_inner(p, flags, loc);
    p->current = scope;
    cc_pop_scope(p);
    return err;
}

static
int
cc_parse_static_if_inner(CcParser* p, CcParseFlags flags, SrcLoc loc){
    int err;
    CcToken tok;
    _Bool predicate;
    {
        CcExpr* cond = NULL;
        CcParseFlags header_flags = flags;
        header_flags.static_header = 1;
        err = cc_parse_selection_header(p, header_flags, "static if", &cond);
        if(err) return err;
        err = cc_decay(p, &cond);
        if(err) return err;
        SrcLoc cond_loc = cond->loc;
        err = cc_eval_truthy(&(CcEvalCtx){p}, cond, &predicate);
        cc_release_expr(p, cond);
        if(err && err != CC_NOT_CONSTANT_ERROR) return err;
        if(err)
            return cc_error(p, cond_loc, "static if condition must be a constant expression");
    }
    p->current->static_for_bindings = 1;
    if(predicate){
        err = cc_parse_static_if_body(p, flags, loc);
        if(err) return err;
        return cc_skip_static_else_chain(p);
    }
    err = cc_skip_static_statement(p, loc);
    if(err) return err;
    err = cc_peek(p, &tok);
    if(err) return err;
    if(tok.type != CC_KEYWORD || tok.kw.kw != CC_else)
        return 0;
    cc_next_token(p, &tok);
    err = cc_peek(p, &tok);
    if(err) return err;
    if(tok.type == CC_KEYWORD && tok.kw.kw == CC_if){
        cc_next_token(p, &tok);
        return cc_parse_static_if(p, flags, loc);
    }
    return cc_parse_static_if_body(p, flags, loc);
}

static
int
cc_static_goto_missing(CcParser* p){
    Atom target = p->static_goto_target;
    p->static_goto_target = NULL;
    return cc_error(p, p->static_goto_loc, "Forward label '%.*s' not found in static goto scope", (int)target->length, target->data);
}

static
int
cc_parse_static_goto(CcParser* p, SrcLoc loc){
    int err;
    CcToken tok;
    err = cc_next_token(p, &tok);
    if(err) return err;
    if(tok.type != CC_IDENTIFIER)
        return cc_error(p, tok.loc, "Expected identifier for `static goto`");
    Atom label = tok.ident.ident;
    err = cc_expect_punct(p, ';');
    if(err) return err;
    p->static_goto_target = label;
    p->static_goto_loc = loc;
    return 0;
}

static
int
cc_handle_static_goto(CcParser* p){
    int err;
    CcToken tok;
    Atom target = p->static_goto_target;
    int braces = 0, parens = 0, brackets = 0;
    _Bool statement_start = 1;
    for(;;){
        err = cc_next_token(p, &tok);
        if(err) return err;
        if(tok.type == CC_EOF) return cc_static_goto_missing(p);
        if(!braces && !parens && !brackets && statement_start
           && tok.type == CC_KEYWORD && tok.kw.kw == CC_static){
            CcToken name, colon;
            err = cc_peek(p, &name);
            if(err) return err;
            if(name.type == CC_IDENTIFIER){
                cc_next_token(p, &name);
                err = cc_peek(p, &colon);
                if(err) return err;
                if(colon.type == CC_PUNCTUATOR && colon.punct.punct == ':'){
                    cc_next_token(p, &colon);
                    if(name.ident.ident == target){
                        p->static_goto_target = NULL;
                        err = cc_unget(p, &colon);
                        if(err) return err;
                        err = cc_unget(p, &name);
                        if(err) return err;
                        return cc_unget(p, &tok);
                    }
                    continue;
                }
                err = cc_unget(p, &name);
                if(err) return err;
            }
        }
        if(!braces && !parens && !brackets && statement_start && tok.type == CC_IDENTIFIER){
            CcToken peek;
            err = cc_peek(p, &peek);
            if(err) return err;
            if(peek.type == CC_PUNCTUATOR && peek.punct.punct == ':'){
                if(tok.ident.ident == target){
                    p->static_goto_target = NULL;
                    err = cc_unget(p, &tok); // Preserve the label for ordinary gotos.
                    if(err) return err;
                    break;
                }
                err = cc_next_token(p, &peek);
                if(err) return err;
                continue;
            }
        }
        if(tok.type == CC_PUNCTUATOR){
            switch(tok.punct.punct){
                case '{': braces++; break;
                case '}':
                    if(!braces)
                        return cc_unget(p, &tok);
                    braces--;
                    break;
                case '(': parens++; break;
                case ')': if(parens) parens--; break;
                case '[': brackets++; break;
                case ']': if(brackets) brackets--; break;
                default: break;
            }
            if(!braces && !parens && !brackets && (tok.punct.punct == ';' || tok.punct.punct == '}')){
                statement_start = 1;
                continue;
            }
        }
        statement_start = 0;
    }
    return 0;
}

static
int
cc_static_tokens(CcParser* p, CcPunct terminator, Marray(CcToken)* tokens, SrcLoc loc){
    int depth = 0;
    for(;;){
        CcToken tok;
        int err = cc_next_token(p, &tok);
        if(err) return err;
        if(tok.type == CC_EOF)
            return cc_error(p, loc, "unterminated static for");
        if(tok.type == CC_PUNCTUATOR){
            if(!depth && tok.punct.punct == terminator)
                return 0;
            switch((uint32_t)tok.punct.punct){
                case '(': case '[': case '{': depth++; break;
                case ')': case ']': case '}':
                    if(!depth)
                        return cc_error(p, tok.loc, "unbalanced static for tokens");
                    depth--;
                    break;
                default: break;
            }
        }
        if(ma_push(CcToken)(tokens, cc_allocator(p), tok))
            return CC_OOM_ERROR;
    }
}

static
int
cc_static_replay(CcParser* p, const Marray(CcToken)* tokens){
    if(ma_push(CcToken)(&p->pending, cc_allocator(p), (CcToken){.type=CC_EOF}))
        return CC_OOM_ERROR;
    for(size_t i = tokens->count; i; i--){
        if(ma_push(CcToken)(&p->pending, cc_allocator(p), tokens->data[i-1]))
            return CC_OOM_ERROR;
    }
    return 0;
}

static
int
cc_static_expression(CcParser* p, CcParseFlags flags, const Marray(CcToken)* tokens, CcQualType expected, CcExpr*_Nullable*_Nonnull out){
    flags.unevaluated = 1;
    Marray(CcToken) saved = p->pending;
    p->pending = (Marray(CcToken)){0};
    CcExpr* expr = NULL;
    int err = ma_push(CcToken)(&p->pending, cc_allocator(p), (CcToken){.type=CC_EOF});
    if(err) goto cleanup;
    err = ma_push(CcToken)(&p->pending, cc_allocator(p), (CcToken){.punct={.type=CC_PUNCTUATOR, .punct=';'}});
    if(err) goto cleanup;
    for(size_t i = tokens->count; i; i--){
        err = ma_push(CcToken)(&p->pending, cc_allocator(p), tokens->data[i-1]);
        if(err) goto cleanup;
    }
    flags.vc = CC_CONSTEXPR_VALUE;
    flags.static_header = 1;
    err = cc_parse_for_condition(p, flags, tokens->data[0].loc, &expr);
    if(err) goto cleanup;
    CcToken tok;
    err = cc_next_token(p, &tok);
    if(err) goto cleanup;
    if(tok.type != CC_EOF){
        err = cc_error(p, tok.loc, "unexpected token in static for expression");
        goto cleanup;
    }
    if(expected.bits){
        err = cc_implicit_cast(p, expr, expected, &expr);
        if(err) goto cleanup;
    }
    if(ccqt_kind(expr->type) == CC_POINTER){
        err = cc_check_linktime_expr(p, expr, 0, 0);
        if(err) goto cleanup;
        err = cc_clone_expr(p, expr, 0, (SrcLoc){0}, 0, out);
    }
    else
        err = cc_eval_expr(&(CcEvalCtx){.parser=p}, expr, out);

    cleanup:
    if(err == CC_NOT_CONSTANT_ERROR && expr)
        err = cc_error(p, expr->loc, "static for expression must be constexpr evaluable");
    if(expr) cc_release_expr(p, expr);
    ma_cleanup(CcToken)(&p->pending, cc_allocator(p));
    p->pending = saved;
    return err;
}

static
int
cc_static_update_op(CcExprKind kind, CcExprKind* binary){
    switch((uint32_t)kind){
        case CC_EXPR_ASSIGN: *binary = CC_EXPR_VALUE; break;
        case CC_EXPR_ADDASSIGN: case CC_EXPR_PREINC: case CC_EXPR_POSTINC: *binary = CC_EXPR_ADD; break;
        case CC_EXPR_SUBASSIGN: case CC_EXPR_PREDEC: case CC_EXPR_POSTDEC: *binary = CC_EXPR_SUB; break;
        case CC_EXPR_MULASSIGN: *binary = CC_EXPR_MUL; break;
        case CC_EXPR_DIVASSIGN: *binary = CC_EXPR_DIV; break;
        case CC_EXPR_MODASSIGN: *binary = CC_EXPR_MOD; break;
        case CC_EXPR_LSHIFTASSIGN: *binary = CC_EXPR_LSHIFT; break;
        case CC_EXPR_RSHIFTASSIGN: *binary = CC_EXPR_RSHIFT; break;
        case CC_EXPR_BITANDASSIGN: *binary = CC_EXPR_BITAND; break;
        case CC_EXPR_BITORASSIGN: *binary = CC_EXPR_BITOR; break;
        case CC_EXPR_BITXORASSIGN: *binary = CC_EXPR_BITXOR; break;
        default: return 0;
    }
    return 1;
}

static
int
cc_static_update_values(CcParser* p, CcExpr* e, _Bool refresh){
    CcExprKind binary;
    if(cc_static_update_op(e->kind, &binary))
        return cc_error(p, e->loc, "static for rebinding value must be constexpr evaluable");
    if(e->kind == CC_EXPR_VARIABLE){
        if(refresh){
            CcVariable* var = cc_scope_lookup_var(p->static_for->bindings, e->var->name, CC_SCOPE_NO_WALK);
            if(var) e->var = var;
        }
        return 0;
    }
    for(size_t i = 0; i < cc_expr_nvalues(e); i++){
        int err = cc_static_update_values(p, e->values[i], refresh);
        if(err) return err;
    }
    switch((uint32_t)e->kind){
        case CC_EXPR_BLOB:
        case CC_EXPR_VALUE:
        case CC_EXPR_FUNCTION:
        case CC_EXPR_BUILTIN:
        case CC_EXPR_DOT: case CC_EXPR_ARROW:
            return 0;
        case CC_EXPR_INIT_LIST: case CC_EXPR_COMPOUND_LITERAL:
            for(size_t i = 0; i < e->init_list->count; i++){
                CcExpr* value = e->init_list->entries[i].value;
                if(!value) continue;
                int err = cc_static_update_values(p, value, refresh);
                if(err) return err;
            }
            return 0;
        case CC_EXPR_STATEMENT_EXPRESSION:
            return cc_error(p, e->loc, "static for rebinding value must be constexpr evaluable");
        default:
            return e->lhs ? cc_static_update_values(p, e->lhs, refresh) : 0;
    }
}

static
int
cc_static_update_validate(CcParser* p, CcExpr* e){
    if(e->kind == CC_EXPR_COMMA){
        int err = cc_static_update_validate(p, e->lhs);
        if(err) return err;
        return cc_static_update_validate(p, e->values[0]);
    }
    CcExprKind binary;
    if(!cc_static_update_op(e->kind, &binary))
        return cc_error(p, e->loc, "static for requires a rebinding update");
    if(!cc_static_rebind_target(p, e->lhs))
        return cc_error(p, e->lhs->loc, "static for update must rebind a clause-1 variable");
    if(cc_expr_nvalues(e)) return cc_static_update_values(p, e->values[0], 0);
    return 0;
}

static
int
cc_parse_static_update(CcParser* p, CcParseFlags flags, const Marray(CcToken)* tokens, SrcLoc loc, CcExpr*_Nullable*_Nonnull out){
    flags.unevaluated = 1;
    if(!tokens->count) return cc_error(p, loc, "static for requires a rebinding update");
    Marray(CcToken) saved = p->pending;
    p->pending = (Marray(CcToken)){0};
    CcExpr* expr = NULL;
    int err = cc_static_replay(p, tokens);
    if(err) goto cleanup;
    flags.vc = CC_CONSTEXPR_VALUE;
    flags.static_rebind = 1;
    err = cc_parse_expr(p, flags, &expr, CCQT_NONE);
    if(err) goto cleanup;
    CcToken tok;
    err = cc_next_token(p, &tok);
    if(err) goto cleanup;
    if(tok.type != CC_EOF){
        err = cc_error(p, tok.loc, "unexpected token in static for update");
        goto cleanup;
    }
    err = cc_static_update_validate(p, expr);
    if(err) goto cleanup;
    *out = expr;
    expr = NULL;
    cleanup:
    if(expr) cc_release_expr(p, expr);
    ma_cleanup(CcToken)(&p->pending, cc_allocator(p));
    p->pending = saved;
    return err;
}

static
int
cc_copy_static_scope(CcParser* p, CcScope* scope, CcScope*_Nonnull*_Nonnull out){
    CcScope* copy = cc_alloc_scope(p);
    if(!copy) return CC_OOM_ERROR;
    int err;
    copy->parent = scope->parent;
    copy->static_for_bindings = scope->static_for_bindings;
    const AtomMap* sources[] = {
        &scope->functions,
        &scope->structs,
        &scope->unions,
        &scope->enums,
        &scope->enumerators,
    };
    AtomMap* targets[] = {
        &copy->functions,
        &copy->structs,
        &copy->unions,
        &copy->enums,
        &copy->enumerators,
    };
    for(size_t m = 0; m < countof(sources); m++){
        err = AM_clone_into(targets[m], cc_allocator(p), sources[m]);
        if(err) goto cleanup;
    }
    err = AM16_clone_into(&copy->typedefs, cc_allocator(p), &scope->typedefs);
    if(err) goto cleanup;
    err = CcAnonAM_clone_into(&copy->variables, cc_allocator(p), &scope->variables);
    if(err) goto cleanup;
    *out = copy;
    return 0;
    cleanup:
    fl_push(&p->scratch_scopes, copy);
    return err;
}

static
int
cc_static_reparent_scope(CcParser* p, CcScope* scope, CcScope* previous, CcScope* bindings, CcScope*_Nonnull*_Nonnull out){
    if(scope == previous){
        *out = bindings;
        return 0;
    }
    CcScope* parent;
    int err = cc_static_reparent_scope(p, scope->parent, previous, bindings, &parent);
    if(err) return err;
    if(scope->retained){
        err = cc_copy_static_scope(p, scope, &scope);
        if(err) return err;
    }
    scope->parent = parent;
    *out = scope;
    return 0;
}

static
int
cc_eval_static_update(CcParser* p, CcExpr* e){
    if(e->kind == CC_EXPR_COMMA){
        int err = cc_eval_static_update(p, e->lhs);
        if(err) return err;
        return cc_eval_static_update(p, e->values[0]);
    }
    CcVariable* old = cc_scope_lookup_var(p->static_for->bindings, e->lhs->var->name, CC_SCOPE_NO_WALK);
    CcExprKind binary;
    if(!cc_static_update_op(e->kind, &binary)) return CC_UNREACHABLE_ERROR;
    CcExpr* expr = NULL;
    CcExpr* left = NULL;
    CcExpr* value = NULL;
    int err;
    if(cc_expr_nvalues(e)){
        err = cc_static_update_values(p, e->values[0], 1);
        if(err) goto cleanup;
        err = cc_clone_expr(p, e->values[0], 0, e->loc, 0, &expr);
        if(err) goto cleanup;
    }
    else {
        expr = cc_int64_expr(p, e->loc, ccqt_basic(CCBT_int), 1);
        if(!expr){
            err = CC_OOM_ERROR;
            goto cleanup;
        }
    }
    if(binary != CC_EXPR_VALUE){
        left = cc_make_expr(p, CC_EXPR_VARIABLE, e->loc, old->type, 0);
        if(!left){
            err = CC_OOM_ERROR;
            goto cleanup;
        }
        left->var = old;
        CcQualType operation = cc_expr_nvalues(e) ? e->compound.type : CCQT_NONE;
        if(ccqt_kind(old->type) == CC_POINTER) operation = (CcQualType){.unqual=old->type.unqual};
        else if(!cc_expr_nvalues(e)){
            err = cc_usual_arithmetic(p, left, expr, &operation, e->loc);
            if(err) goto cleanup;
        }
        err = cc_implicit_cast(p, left, operation, &left);
        if(err) goto cleanup;
        if(ccqt_kind(operation) != CC_POINTER){
            err = cc_implicit_cast(p, expr, operation, &expr);
            if(err) goto cleanup;
        }
        CcExpr* calculation = cc_binary_expr(p, binary, e->loc, operation, left, expr);
        if(!calculation){
            err = CC_OOM_ERROR;
            goto cleanup;
        }
        expr = calculation;
        left = NULL;
    }
    err = cc_implicit_cast(p, expr, old->type, &expr);
    if(err) goto cleanup;
    if(ccqt_kind(old->type) == CC_POINTER){
        err = cc_check_linktime_expr(p, expr, 0, 0);
        if(err) goto cleanup;
        err = cc_clone_expr(p, expr, 0, e->loc, 0, &value);
    }
    else err = cc_eval_expr(&(CcEvalCtx){.parser=p}, expr, &value);
    if(err) goto cleanup;
    CcVariable* next = Allocator_zalloc(cc_allocator(p), 1, sizeof *next, _Alignof(CcVariable));
    if(!next){
        err = CC_OOM_ERROR;
        goto cleanup;
    }
    *next = *old;
    next->initializer = value;
    next->interp_initialized = 0;
    next->interp_val = NULL;
    value = NULL;
    // Emitted expressions retain variables, not maps. Replace the map entry
    // in place unless a function retains this lexical scope as a snapshot.
    CcScope* previous = p->static_for->bindings;
    if(!previous->retained){
        err = cc_scope_insert_var(cc_allocator(p), previous, next->name, next);
        if(err) goto cleanup;
        if(ccqt_bt_eq(next->type, CCBT__Type)){
            err = cc_scope_insert_typedef(cc_allocator(p), previous, next->name, next->initializer->type_value, next->loc);
            if(err) goto cleanup;
        }
        goto cleanup;
    }
    CcScope* bindings;
    err = cc_copy_static_scope(p, previous, &bindings);
    if(err) goto cleanup;
    err = cc_scope_insert_var(cc_allocator(p), bindings, next->name, next);
    if(err) goto cleanup;
    if(ccqt_bt_eq(next->type, CCBT__Type)){
        err = cc_scope_insert_typedef(cc_allocator(p), bindings, next->name, next->initializer->type_value, next->loc);
        if(err) goto cleanup;
    }
    err = cc_static_reparent_scope(p, p->current, previous, bindings, &p->current);
    if(err) goto cleanup;
    p->static_for->bindings = bindings;
    cleanup:
    if(err == CC_NOT_CONSTANT_ERROR) err = cc_error(p, e->loc, "static for expression must be constexpr evaluable");
    if(left) cc_release_expr(p, left);
    if(expr) cc_release_expr(p, expr);
    if(value) cc_release_expr(p, value);
    return err;
}

static
int
cc_replay_static_update(CcParser* p, CcParseFlags flags, const Marray(CcToken)* tokens, SrcLoc loc){
    // Split only the sequenced rebinding list, preserving commas inside a
    // rebinding's RHS. Peel grouping parentheses so nested lists work too.
    Marray(CcToken) part = *tokens;
    for(;;){
        size_t depth = 0;
        size_t conditional = 0;
        _Bool grouped = part.count > 1 && part.data[0].type == CC_PUNCTUATOR
            && part.data[0].punct.punct == '('
            && part.data[part.count-1].type == CC_PUNCTUATOR
            && part.data[part.count-1].punct.punct == ')';
        for(size_t i = 0; i < part.count; i++){
            CcToken* tok = &part.data[i];
            if(tok->type != CC_PUNCTUATOR) continue;
            switch((uint32_t)tok->punct.punct){
                case '(': case '[': case '{': depth++; break;
                case ')': case ']': case '}':
                    if(!depth) return cc_error(p, tok->loc, "unbalanced static for tokens");
                    depth--;
                    if(!depth && i != part.count-1) grouped = 0;
                    break;
                case '?': if(!depth) conditional++; break;
                case ':': if(!depth && conditional) conditional--; break;
                case ',':
                    if(!depth && !conditional){
                        Marray(CcToken) left = {.data=part.data, .count=i};
                        Marray(CcToken) right = {.data=part.data+i+1, .count=part.count-i-1};
                        int err = cc_replay_static_update(p, flags, &left, loc);
                        if(err) return err;
                        return cc_replay_static_update(p, flags, &right, loc);
                    }
                    break;
                default: break;
            }
        }
        if(!grouped) break;
        part.data++;
        part.count -= 2;
    }
    // Parsing can evaluate sizeof, type expressions, and reflection. Reparse
    // after each preceding rebinding, not just once per iteration.
    CcExpr* expr = NULL;
    int err = cc_parse_static_update(p, flags, &part, loc, &expr);
    if(err) goto cleanup;
    err = cc_eval_static_update(p, expr);
    cleanup:
    if(expr) cc_release_expr(p, expr);
    return err;
}

static
int
cc_parse_static_for(CcParser* p, CcParseFlags flags, SrcLoc loc){
    int err = cc_expect_punct(p, '(');
    if(err) return err;
    err = cc_push_scope(p);
    if(err) return err;
    CcStaticFor loop = {.previous=p->static_for, .bindings=p->current,
        .flow={.previous=p->static_control, .is_loop=1, .runtime_control_depth=p->runtime_control_depth}};
    p->static_for = &loop;
    p->static_control = &loop.flow;
    Marray(CcToken) condition = {0}, update = {0}, body = {0};
    CcStmtSink* init_sink = cc_push_stmt_sink(p);
    if(!init_sink){
        err = CC_OOM_ERROR;
        goto done;
    }
    CcDeclBase base = {0};
    err = cc_parse_declaration_specifier(p, flags, &base);
    if(err) goto done;
    if(!(base.spec.bits || base.type.bits)){
        err = cc_error(p, loc, "static for clause-1 must be a constexpr declaration");
        goto done;
    }
    if(base.spec.sp_typedef || base.spec.sp_extern || base.spec.sp_static || base.spec.sp_thread_local){
        err = cc_error(p, loc, "invalid storage class in static for clause-1");
        goto done;
    }
    base.spec.sp_constexpr = 1;
    err = cc_resolve_specifiers(p, &base);
    if(err) goto done;
    CcParsedDecls decls;
    err = cc_parse_decls(p, flags, &base, &decls);
    if(err) goto done;
    if(decls.function_definition){
        err = cc_error(p, loc, "static for clause-1 must declare variables");
        goto done;
    }
    err = cc_expect_punct(p, CC_semi);
    if(err) goto done;
    AtomMapItems items = CcAnonAM_items(&loop.bindings->variables);
    if(!items.count){
        err = cc_error(p, loc, "static for clause-1 must declare variables");
        goto done;
    }
    for(size_t i = 0; i < items.count; i++){
        CcVariable* var = items.data[i].p;
        if(!var->initializer){
            err = cc_error(p, var->loc, "static for variable requires an initializer");
            goto done;
        }
        CcExpr* value = NULL;
        if(ccqt_kind(var->type) == CC_POINTER){
            err = cc_check_linktime_expr(p, var->initializer, 0, 0);
            if(err) goto initializer_done;
            err = cc_clone_expr(p, var->initializer, 0, (SrcLoc){0}, 0, &value);
        }
        else err = cc_eval_expr(&(CcEvalCtx){.parser=p}, var->initializer, &value);
        initializer_done:
        if(err){
            if(err == CC_NOT_CONSTANT_ERROR)
                err = cc_error(p, var->loc, "static for initializer must be constexpr evaluable");
            goto done;
        }
        var->initializer = value;
        var->automatic = 0;
        var->static_ = 1;
        var->interp_preinit = 1;
    }
    cc_pop_stmt_sink(p, init_sink);
    init_sink = NULL;
    loop.bindings->static_for_bindings = 1;
    err = cc_static_tokens(p, ';', &condition, loc);
    if(err) goto done;
    err = cc_static_tokens(p, ')', &update, loc);
    if(err) goto done;
    // Only require an update here. Parsing it can evaluate dependent types
    // and sizeof operands, so validate each rebinding when it is reached.
    if(!update.count){
        err = cc_error(p, loc, "static for requires a rebinding update");
        goto done;
    }
    err = cc_static_statement_tokens(p, &body, loc);
    if(err) goto done;
    // The outer braces group a static body without introducing a real scope.
    if(body.count && body.data[0].type == CC_PUNCTUATOR && body.data[0].punct.punct == '{'){
        memmove(body.data, body.data+1, (body.count-2)*sizeof *body.data);
        body.count -= 2;
    }
    CcScope* condition_scope = NULL;
    for(size_t iteration = 0; ; iteration++){
        if(condition_scope){
            p->current = condition_scope;
            cc_pop_scope(p);
            condition_scope = NULL;
        }
        p->current = loop.bindings;
        err = cc_push_scope(p);
        if(err) break;
        condition_scope = p->current;
        if(condition.count){
            CcExpr* value;
            err = cc_static_expression(p, flags, &condition, CCQT_NONE, &value);
            if(err) break;
            _Bool predicate = 0;
            err = cc_require_scalar(p, &value, loc, "static for condition");
            if(err){
                cc_release_expr(p, value);
                break;
            }
            err = cc_eval_truthy(&(CcEvalCtx){.parser=p}, value, &predicate);
            cc_release_expr(p, value);
            if(err || !predicate) break;
        }
        condition_scope->static_for_bindings = 1;
        if(iteration == 1000000){
            err = cc_error(p, loc, "static for iteration limit exceeded");
            break;
        }
        Marray(CcToken) saved = p->pending;
        p->pending = (Marray(CcToken)){0};
        err = ma_push(CcToken)(&p->pending, cc_allocator(p), (CcToken){.type=CC_EOF});
        if(err) goto iteration_oom;
        err = ma_push(CcToken)(&p->pending, cc_allocator(p), (CcToken){.punct={.type=CC_PUNCTUATOR, .loc=loc, .punct='}'}});
        if(err) goto iteration_oom;
        for(size_t i = body.count; i; i--){
            err = ma_push(CcToken)(&p->pending, cc_allocator(p), body.data[i-1]);
            if(err) goto iteration_oom;
        }
        err = ma_push(CcToken)(&p->pending, cc_allocator(p), (CcToken){.punct={.type=CC_PUNCTUATOR, .loc=loc, .punct='{'}});
        if(err) goto iteration_oom;
        err = cc_parse_static_if_body(p, flags, loc);
        if(err) goto iteration_end;
        if(p->static_goto_target) err = cc_static_goto_missing(p);
        goto iteration_end;
        iteration_oom:
        err = CC_OOM_ERROR;
        iteration_end:
        ma_cleanup(CcToken)(&p->pending, cc_allocator(p));
        p->pending = saved;
        if(err || loop.flow.control == CC_STATIC_BREAK) break;
        loop.flow.control = CC_STATIC_NEXT;
        err = cc_replay_static_update(p, flags, &update, loc);
        condition_scope = p->current;
        if(err) break;
    }
    if(condition_scope){
        p->current = condition_scope;
        cc_pop_scope(p);
    }
    done:
    if(init_sink) cc_pop_stmt_sink(p, init_sink);
    ma_cleanup(CcToken)(&condition, cc_allocator(p));
    ma_cleanup(CcToken)(&update, cc_allocator(p));
    ma_cleanup(CcToken)(&body, cc_allocator(p));
    p->static_for = loop.previous;
    p->static_control = loop.flow.previous;
    p->current = loop.bindings;
    cc_pop_scope(p);
    return err;
}

static
int
cc_static_switch_key(CcParser* p, CcParseFlags flags, CcQualType* type, CiUint128* key, CcStaticCase*_Nullable range){
    flags.unevaluated = 1;
    CcExpr* expr = NULL;
    CcExpr* value = NULL;
    flags.vc = CC_CONSTEXPR_VALUE;
    flags.static_header = !range;
    int err = range ? cc_parse_expr(p, flags, &expr, CCQT_NONE)
        : cc_parse_selection_header(p, flags, "static switch", &expr);
    if(err) return err;
    if(range){
        CcToken next;
        err = cc_peek(p, &next);
        if(err) goto done;
        if(next.type == CC_PUNCTUATOR && next.punct.punct == CC_ellipsis){
            CcExpr* high = NULL;
            err = cc_next_token(p, &next);
            if(err) goto range_cleanup;
            err = cc_parse_expr(p, flags, &high, CCQT_NONE);
            if(err) goto range_cleanup;
            err = cc_eval_case_range(p, range->loc, *type, expr, high, key, &range->high, &range->empty);
            range_cleanup:
            if(high) cc_release_expr(p, high);
            goto done;
        }
    }
    _Bool is_type = ccqt_bt_eq(expr->type, CCBT__Type);
    if(!is_type && !ccqt_is_integer(expr->type)){
        err = cc_type_error(p, expr->loc, expr->type, "static switch requires integer or _Type expressions");
        goto done;
    }
    if(!type->bits){
        if(is_type) *type = ccqt_basic(CCBT__Type);
        else err = cc_integer_promote_expr(p, expr, type, expr->loc);
        if(err) goto done;
    }
    if(is_type != ccqt_bt_eq(*type, CCBT__Type)){
        err = cc_types_error(p, expr->loc, expr->type, *type, "static case type does not match static switch expression");
        goto done;
    }
    err = cc_implicit_cast(p, expr, *type, &expr);
    if(err) goto evaluation_done;
    err = cc_eval_expr(&(CcEvalCtx){.parser=p}, expr, &value);
    if(err) goto evaluation_done;
    *key = is_type ? ci_uint128_from_uint64(value->type_value.bits) : cc_eval_u128(cc_target(p), value);
    if(range) range->high = *key;
    evaluation_done:
    if(err == CC_NOT_CONSTANT_ERROR || err == CC_OVERFLOW_ERROR)
        err = cc_error(p, expr->loc, "static switch expression must be constexpr evaluable");
    done:
    if(value) cc_release_expr(p, value);
    cc_release_expr(p, expr);
    return err;
}

static
int
cc_parse_static_switch_inner(CcParser* p, CcParseFlags flags, SrcLoc loc);

static
int
cc_parse_static_switch(CcParser* p, CcParseFlags flags, SrcLoc loc){
    int err = cc_push_scope(p);
    if(err) return err;
    CcScope* scope = p->current;
    err = cc_parse_static_switch_inner(p, flags, loc);
    p->current = scope;
    cc_pop_scope(p);
    return err;
}

static
int
cc_parse_static_switch_inner(CcParser* p, CcParseFlags flags, SrcLoc loc){
    int err;
    CcQualType type = CCQT_NONE;
    CiUint128 key;
    err = cc_static_switch_key(p, flags, &type, &key, NULL);
    if(err) return err;
    p->current->static_for_bindings = 1;
    err = cc_expect_punct(p, '{');
    if(err) return err;
    Marray(CcToken) body = {0}, emitted = {0};
    Marray(CcStaticCase) cases = {0};
    err = cc_static_tokens(p, '}', &body, loc);
    if(err) goto cleanup;
    Marray(CcToken) saved = p->pending;
    p->pending = (Marray(CcToken)){0};
    err = cc_static_replay(p, &body);
    size_t selected = SIZE_MAX, fallback = SIZE_MAX;
    // Sign-extended keys become monotonically ordered after flipping the sign bit.
    _Bool uns = ccqt_bt_eq(type, CCBT__Type) || ccqt_is_unsigned(type, !cc_target(p)->char_is_signed);
    CiUint128 sign = uns ? ci_uint128_from_uint64(0) : ci_uint128_shl(ci_uint128_from_uint64(1), 127);
    CiUint128 ordered_key = ci_uint128_xor(key, sign);
    while(!err){
        CcToken tok;
        err = cc_peek(p, &tok);
        if(err || tok.type == CC_EOF) break;
        if(tok.type == CC_KEYWORD && tok.kw.kw == CC_static){
            CcToken label;
            cc_next_token(p, &tok);
            err = cc_peek(p, &label);
            if(err) break;
            if(label.type == CC_KEYWORD && (label.kw.kw == CC_case || label.kw.kw == CC_default)){
                cc_next_token(p, &label);
                if(label.kw.kw == CC_default){
                    if(fallback != SIZE_MAX){
                        err = cc_error(p, label.loc, "duplicate static default label");
                        break;
                    }
                    fallback = emitted.count;
                }
                else {
                    CcStaticCase entry = {.loc=label.loc};
                    err = cc_static_switch_key(p, flags, &type, &entry.value, &entry);
                    if(err) break;
                    if(!entry.empty){
                        CiUint128 low = ci_uint128_xor(entry.value, sign);
                        CiUint128 high = ci_uint128_xor(entry.high, sign);
                        for(size_t i = 0; i < cases.count; i++){
                            CcStaticCase prev = cases.data[i];
                            if(ci_uint128_le(low, ci_uint128_xor(prev.high, sign)) && ci_uint128_le(ci_uint128_xor(prev.value, sign), high)){
                                err = cc_error(p, label.loc, "%s",
                                    ci_uint128_eq(entry.value, entry.high) && ci_uint128_eq(prev.value, prev.high) ?
                                    "duplicate static case value" : "overlapping static case values"
                                );
                                break;
                            }
                        }
                        if(err) break;
                        if(ma_push(CcStaticCase)(&cases, cc_allocator(p), entry)){
                            err = CC_OOM_ERROR;
                            break;
                        }
                        if(ci_uint128_le(low, ordered_key) && ci_uint128_le(ordered_key, high))
                            selected = emitted.count;
                    }
                }
                err = cc_expect_punct(p, ':');
                continue;
            }
            // A static named label is a marker, including at the closing brace.
            if(label.type == CC_IDENTIFIER){
                CcToken colon;
                cc_next_token(p, &label);
                err = cc_peek(p, &colon);
                if(err) break;
                if(colon.type == CC_PUNCTUATOR && colon.punct.punct == ':'){
                    cc_next_token(p, &colon);
                    if(ma_push(CcToken)(&emitted, cc_allocator(p), tok)
                       || ma_push(CcToken)(&emitted, cc_allocator(p), label)
                       || ma_push(CcToken)(&emitted, cc_allocator(p), colon)) err = CC_OOM_ERROR;
                    continue;
                }
                err = cc_unget(p, &label);
                if(err) break;
            }
            err = cc_unget(p, &tok);
            if(err) break;
        }
        err = cc_static_statement_tokens(p, &emitted, loc);
    }
    ma_cleanup(CcToken)(&p->pending, cc_allocator(p));
    p->pending = (Marray(CcToken)){0};
    if(err) goto restore_pending;
    if(selected == SIZE_MAX) selected = fallback;
    if(selected != SIZE_MAX){
        CcStaticControl flow = {.previous=p->static_control, .runtime_control_depth=p->runtime_control_depth};
        p->static_control = &flow;
        err = ma_push(CcToken)(&p->pending, cc_allocator(p), (CcToken){.type=CC_EOF});
        if(err) goto switch_oom;
        err = ma_push(CcToken)(&p->pending, cc_allocator(p), (CcToken){.punct={.type=CC_PUNCTUATOR, .loc=loc, .punct='}'}});
        if(err) goto switch_oom;
        for(size_t i = emitted.count; i > selected; i--){
            err = ma_push(CcToken)(&p->pending, cc_allocator(p), emitted.data[i-1]);
            if(err) goto switch_oom;
        }
        err = ma_push(CcToken)(&p->pending, cc_allocator(p), (CcToken){.punct={.type=CC_PUNCTUATOR, .loc=loc, .punct='{'}});
        if(err) goto switch_oom;
        err = cc_parse_static_if_body(p, flags, loc);
        if(err) goto switch_end;
        if(p->static_goto_target) err = cc_static_goto_missing(p);
        goto switch_end;
        switch_oom:
        err = CC_OOM_ERROR;
        switch_end:
        p->static_control = flow.previous;
    }
    restore_pending:
    ma_cleanup(CcToken)(&p->pending, cc_allocator(p));
    p->pending = saved;
    cleanup:
    ma_cleanup(CcToken)(&body, cc_allocator(p));
    ma_cleanup(CcToken)(&emitted, cc_allocator(p));
    ma_cleanup(CcStaticCase)(&cases, cc_allocator(p));
    return err;
}

static
int
cc_skip_static_control(CcParser* p){
    int depth = 0;
    for(;;){
        CcToken tok;
        int err = cc_next_token(p, &tok);
        if(err) return err;
        if(tok.type == CC_EOF) return cc_error(p, tok.loc, "unterminated static for body");
        if(tok.type != CC_PUNCTUATOR) continue;
        if(tok.punct.punct == '{') depth++;
        else if(tok.punct.punct == '}'){
            if(!depth) return cc_unget(p, &tok);
            depth--;
        }
    }
}

static
int
cc_parse_one(CcParser* p, CcParseFlags flags){
    flags = (CcParseFlags){.unevaluated=flags.unevaluated};
    int err = cc_parse_one_inner(p, flags, NULL);
    if(err) return err;
    if(p->static_goto_target) return 0;
    if(cc_static_control_pending(p)) return 0;
    return cc_parse_local_methods(p);
}

static
int
cc_parse_one_inner(CcParser* p, CcParseFlags flags, CcControlLabel*_Nullable labels){
    int err;
    CcToken tok;
    if(cc_static_control_pending(p)) return cc_skip_static_control(p);
    if(p->static_goto_target){
        err = cc_handle_static_goto(p);
        if(err) return err;
        if(p->static_goto_target) return 0;
    }
    err = cc_peek(p, &tok);
    if(err) return err;
    if(tok.type == CC_KEYWORD && tok.kw.kw == CC_asm){
        cc_next_token(p, &tok);
        err = cc_peek(p, &tok);
        if(err) return err;
        if(tok.type == CC_PUNCTUATOR && tok.punct.punct == '('){
            cc_next_token(p, &tok);
            err = cc_peek(p, &tok);
            if(err) return err;
            if(tok.type == CC_STRING_LITERAL && tok.str.length <= 1){
                cc_next_token(p, &tok);
                err = cc_peek(p, &tok);
                if(err) return err;
            }
            if(tok.type == CC_PUNCTUATOR && tok.punct.punct == ')'){
                cc_next_token(p, &tok);
            }
            else {
                return cc_error(p, tok.loc, "Unable to handle this asm statement: expected empty string and then closing paren");
            }
            err = cc_expect_punct(p, ';');
            if(err) return err;
            return 0;
        }
        else {
            return cc_error(p, tok.loc, "Unable to handle this asm statement");
        }
    }
    if(tok.type == CC_KEYWORD && tok.kw.kw == CC_static){
        CcToken control_tok;
        cc_next_token(p, &tok);
        err = cc_peek(p, &control_tok);
        if(err) return err;
        if(control_tok.type == CC_KEYWORD){
            switch(control_tok.kw.kw){
                case CC_if:
                    cc_next_token(p, &control_tok);
                    return cc_parse_static_if(p, flags, tok.loc);
                case CC_goto:
                    cc_next_token(p, &control_tok);
                    return cc_parse_static_goto(p, tok.loc);
                case CC_switch:
                    cc_next_token(p, &control_tok);
                    return cc_parse_static_switch(p, flags, tok.loc);
                case CC_case:
                case CC_default:
                    return cc_error(p, tok.loc, "static case/default must appear directly in a static switch body");
                case CC_for:
                    cc_next_token(p, &control_tok);
                    return cc_parse_static_for(p, flags, tok.loc);
                case CC_break:
                case CC_continue: {
                    cc_next_token(p, &control_tok);
                    CcStaticControl* target = p->static_control;
                    if(control_tok.kw.kw == CC_continue)
                        while(target && !target->is_loop) target = target->previous;
                    if(!target) return cc_error(p, tok.loc, control_tok.kw.kw == CC_break
                        ? "static break outside static for or static switch" : "static continue outside static for");
                    if(p->runtime_control_depth != target->runtime_control_depth)
                        return cc_error(p, tok.loc, "static break/continue cannot cross runtime control flow");
                    err = cc_expect_punct(p, ';');
                    if(err) return err;
                    target->control = control_tok.kw.kw == CC_break ? CC_STATIC_BREAK : CC_STATIC_CONTINUE;
                    return 0;
                }
                default:
                    break;
            }
        }
        else if(control_tok.type == CC_IDENTIFIER){
            CcToken colon;
            cc_next_token(p, &control_tok);
            err = cc_peek(p, &colon);
            if(err) return err;
            if(colon.type == CC_PUNCTUATOR && colon.punct.punct == ':'){
                cc_next_token(p, &colon);
                return 0; // Compile-time marker; do not emit a runtime label.
            }
            err = cc_unget(p, &control_tok);
            if(err) return err;
        }
        cc_unget(p, &tok);
    }
    if(cc_is_c23_attribute_start(p)){
        err = cc_parse_c23_attributes(p, flags, &p->attributes);
        if(err) return err;
        err = cc_peek(p, &tok);
        if(err) return err;
        if(tok.type == CC_PUNCTUATOR && tok.punct.punct == ';'){
            err = cc_next_token(p, &tok); // consume ';'
            if(err) return err;
            cc_clear_attributes(&p->attributes);
            return 0;
        }
    }
    CcStmtNode* label;
    err = cc_parse_label(p, flags, &label);
    if(err) return err;
    if(label){
        err = cc_sink_push(p, label);
        if(err) return err;
        err = cc_peek(p, &tok);
        if(err) return err;
        if(tok.type == CC_EOF || (tok.type == CC_PUNCTUATOR && tok.punct.punct == '}'))
            return 0;
        if(label->kind == CC_STMT_LABEL){
            CcControlLabel binding = {.prev=labels, .name=label->label};
            return cc_parse_one_inner(p, flags, &binding);
        }
        return cc_parse_one_inner(p, flags, labels);
    }
    CcDeclBase b = {0};
    err = cc_parse_declaration_specifier(p, flags, &b);
    if(err) return err;
    if(b.spec.bits || b.type.bits){
        if(b.spec.sp_typedef && !b.spec.sp_typebits && !b.type.bits){
            CcToken peek;
            err = cc_peek(p, &peek);
            if(err) return err;
            return cc_error(p, peek.loc, "typedef requires a type");
        }
        err = cc_resolve_specifiers(p, &b);
        if(err) return err;
        CcParsedDecls decls;
        err = cc_parse_decls(p, flags, &b, &decls);
        if(err) return err;
        return decls.function_definition ? 0 : cc_expect_punct(p, CC_semi);
    }

    err = cc_peek(p, &tok);
    if(err) return err;
    switch(tok.type){
        case CC_EMBED:
            return cc_unreachable(p, tok.loc, "embed should be gone by now");
        case CC_CONSTANT:
        case CC_EOF:
        case CC_IDENTIFIER:
        case CC_PUNCTUATOR:
        case CC_STRING_LITERAL:
        case CC_KEYWORD:
            break;
    }
    {
        CcStmtNode*_Null_unspecified node = NULL;
        err = cc_parse_statement(p, flags, labels, &node);
        if(err) return err;
        return cc_sink_push(p, node);
    }
}

static
int
cc_parse_all(CcParser* p){
    DRP_PROFILE_BEGIN(profile, "parser.translation_unit");
    int err = 0;
    CcDeferContext saved_defer_context = p->defer_context;
    CcToken tok;
    CcLabelCtx* lctx = cc_label_ctx(p);
    size_t goto_mark = lctx->gotos.count;
    CcStmtSink* sink = cc_push_stmt_sink(p);
    if(!sink) return CC_OOM_ERROR;
    for(;;){
        err = cc_peek(p, &tok);
        if(err) break;
        if(p->static_goto_target && (tok.type == CC_EOF || (tok.type == CC_PUNCTUATOR && tok.punct.punct == '}'))){
            err = cc_static_goto_missing(p);
            break;
        }
        if(tok.type == CC_EOF)
            break;
        err = cc_parse_one(p, (CcParseFlags){0});
        if(err) break;
    }
    if(err) goto end_batch;
    err = cc_check_gotos(p, lctx);
    if(err) goto end_batch;
    if(p->defer_context.active != saved_defer_context.active){
        CcStmtNode* exit = cc_stmt_node(p, CC_STMT_NULL, tok.loc, 0);
        if(!exit){
            err = CC_OOM_ERROR;
            goto end_batch;
        }
        exit->cleanup = (CcCleanupRange){p->defer_context.active, saved_defer_context.active};
        err = cc_sink_push(p, exit);
    }
    end_batch:
    p->defer_context = saved_defer_context;
    if(err){
        // Drop references from the failed batch.
        lctx->gotos.count = goto_mark;
        goto cleanup;
    }
    err = pa_extend(&p->module->nodes, cc_allocator(p), sink->stmts.count, sink->stmts.data);
    if(err){
        err = CC_OOM_ERROR;
        goto cleanup;
    }
    sink->stmts.count = 0;
    cleanup:
    cc_pop_stmt_sink(p, sink);
    return err;
    DRP_PROFILE_END(profile);
}

static
void
cc_parser_discard_input(CcParser* p){
    p->static_goto_target = NULL;
    p->pending.count = 0;
    cpp_discard_all_input(&p->cpp);
}

static
CcLabelCtx*
cc_label_ctx(CcParser* p){
    if(p->current_func)
        return &((CcFunc*_Nonnull)p->current_func)->label_ctx;
    return &p->module->labels;
}

static
int
cc_check_gotos(CcParser* p, CcLabelCtx* ctx){
    for(size_t i = 0; i < ctx->gotos.count; i++){
        CcLabelRef* g = &ctx->gotos.data[i];
        CcDeferContext target;
        if(!cc_lctx_get_defer(ctx, g->label, &target))
            return cc_error(p, g->loc, "Use of undeclared label '%.*s'", g->label->length, g->label->data);
        int err = cc_check_transfer(p, g->loc, g->defer_context, target);
        if(err) return err;
        if(g->cleanup)
            *g->cleanup = (CcCleanupRange){g->defer_context.active, target.active};
    }
    ctx->gotos.count = 0;
    return 0;
}

static
int
cc_check_transfer(CcParser* p, SrcLoc loc, CcDeferContext from, CcDeferContext to){
    if(from.deferred_body != to.deferred_body)
        return cc_error(p, loc, "jump crosses a deferred block boundary");
    CcDefer* d = from.active;
    while(d != to.active){
        if(!d) return cc_error(p, loc, to.active && to.active->vm_scope ?
            "jump enters the scope of a variably modified declaration"
            : "jump enters the scope of a defer statement");
        d = d->prev;
    }
    return 0;
}

static
MStringBuilder*
cc_start_errorv(CcParser* p, SrcLoc loc, const char* fmt, va_list va){
    cpp_include_backtrace(&p->cpp, LOG_PRINT_ERROR);
    cpp_msg_preamble(&p->cpp, loc, "error");
    MStringBuilder* sb = &p->cpp.logger->buff;
    msb_vsprintf(sb, fmt, va);
    return sb;
}

static
MStringBuilder*
cc_start_error(CcParser* p, SrcLoc loc, const char* fmt, ...){
    va_list va;
    va_start(va, fmt);
    MStringBuilder *sb = cc_start_errorv(p, loc, fmt, va);
    va_end(va);
    return sb;
}

static
int
cc_finish_error(CcParser* p, SrcLoc loc){
    log_flush(p->cpp.logger, LOG_PRINT_ERROR);
    cpp_msg_postamble(&p->cpp, loc, LOG_PRINT_ERROR);
    return CC_SYNTAX_ERROR;
}

static
int
cc_error(CcParser* p, SrcLoc loc, const char* fmt, ...){
    va_list va;
    va_start(va, fmt);
    cpp_msg(&p->cpp, loc, LOG_PRINT_ERROR, "error", fmt, va);
    va_end(va);
    return CC_SYNTAX_ERROR;
}

static
void
cc_warn(CcParser* p, SrcLoc loc, const char* fmt, ...){
    va_list va;
    va_start(va, fmt);
    cpp_msg(&p->cpp, loc, LOG_PRINT_ERROR, "warning", fmt, va);
    va_end(va);
}

static
void
cc_info(CcParser* p, SrcLoc loc, const char* fmt, ...){
    va_list va;
    va_start(va, fmt);
    cpp_msg(&p->cpp, loc, LOG_PRINT_ERROR, "info", fmt, va);
    va_end(va);
}

static
int
cc_get_string_literal(CcExpr* e, const char*_Nullable*_Nonnull text, uint32_t* length){
    for(;;){
        if(e->kind == CC_EXPR_CAST){
            e = e->lhs;
            continue;
        }
        if(e->kind == CC_EXPR_VARIABLE && e->var->constexpr_ && e->var->initializer){
            e = e->var->initializer;
            continue;
        }
        break;
    }
    if(e->kind != CC_EXPR_VALUE || ccqt_kind(e->type) != CC_ARRAY){
        *text = NULL;
        return 0;
    }
    CcArray* a = ccqt_as_array(e->type);
    if(!ccqt_is_basic(a->element) || ccqt_basic_kind(a->element) != CCBT_char){
        *text = NULL;
        return 0;
    }
    *length = e->str.length;
    *text = e->text;
    return 0;
}

static
int
cc_printf_format_error(CcParser* p, SrcLoc loc, const char* spec, int spec_len, uint32_t arg_num, CcQualType expected, CcQualType actual){
    cpp_msg_preamble(&p->cpp, loc, "error");
    MStringBuilder* buff = &p->cpp.logger->buff;
    msb_sprintf(buff, "format specifier '%%");
    msb_write_str(buff, spec, spec_len);
    msb_sprintf(buff, "' (argument %u) expects '", (unsigned)arg_num);
    cc_print_type(buff, expected);
    msb_write_literal(buff, "', but argument has type '");
    cc_print_type(buff, actual);
    msb_write_char(buff, '\'');
    log_flush(p->cpp.logger, LOG_PRINT_ERROR);
    cpp_msg_postamble(&p->cpp, loc, LOG_PRINT_ERROR);
    return CC_SYNTAX_ERROR;
}

static
int
cc_merge_printf_formats(CcParser* p, const CcPrintfFormat* _Nullable source,
    const CcPrintfFormat* _Nullable* _Nonnull destination){
    for(; source; source = source->next){
        _Bool duplicate = 0;
        for(const CcPrintfFormat* old = *destination; old; old = old->next){
            if(old->string_index == source->string_index && old->first_to_check == source->first_to_check){
                duplicate = 1;
                break;
            }
        }
        if(duplicate) continue;
        CcPrintfFormat* copy = Allocator_alloc(cc_allocator(p), 1, sizeof *copy, _Alignof(CcPrintfFormat));
        if(!copy) return CC_OOM_ERROR;
        *copy = *source;
        copy->next = *destination;
        *destination = copy;
    }
    return 0;
}

static
int
cc_apply_printf_formats(CcParser* p, CcFunc* func, const CcPrintfFormat* _Nullable formats){
    for(const CcPrintfFormat* format = formats; format; format = format->next){
        if(!format->string_index || format->string_index > func->type->param_count)
            return cc_error(p, format->loc, "format string index is outside the parameter list");
        CcQualType type = func->type->params[format->string_index - 1];
        if(ccqt_kind(type) != CC_POINTER || !ccqt_bt_eq(ccqt_as_ptr(type)->pointee, CCBT_char))
            return cc_type_error(p, format->loc, type, "format string parameter must be a char pointer");
        if(format->first_to_check
            && (!func->type->is_variadic || format->first_to_check - 1 != func->type->param_count))
            return cc_error(p, format->loc, "format first argument must name the first variadic argument or be zero");
    }
    return cc_merge_printf_formats(p, formats, &func->printf_formats);
}

static
int
cc_check_printf_format(CcParser* p, const CcPrintfFormat* format, CcExpr*_Nonnull*_Nonnull args, uint32_t nargs, SrcLoc loc){
    uint32_t fmt_idx = format->string_index - 1;
    uint32_t first_vararg = format->first_to_check ? format->first_to_check - 1 : nargs;
    if(fmt_idx >= nargs) return 0;
    const char* fmt = NULL;
    uint32_t slen = 0;
    int err = cc_get_string_literal(args[fmt_idx], &fmt, &slen);
    if(err) return err;
    if(!fmt) return 0;
    if(slen == 0) return 0;
    slen--;
    const CcTargetConfig* tc = cc_target(p);
    uint32_t arg_idx = first_vararg;
    uint32_t expected_args = 0;
    for(uint32_t i = 0; i < slen; i++){
        if(fmt[i] != '%') continue;
        i++;
        if(i >= slen)
            return cc_error(p, loc, "incomplete format specifier at end of string");
        if(fmt[i] == '%') continue;
        while(i < slen && (fmt[i] == '-' || fmt[i] == '+' || fmt[i] == ' ' || fmt[i] == '#' || fmt[i] == '0' || fmt[i] == '\'' || fmt[i] == '$' || fmt[i] == '_'))
            i++;
        if(i >= slen)
            return cc_error(p, loc, "incomplete format specifier at end of string");
        if(fmt[i] == '*'){
            expected_args++;
            if(arg_idx < nargs){
                CcQualType actual = args[arg_idx]->type;
                if(!ccqt_is_basic(actual) || (ccqt_basic_kind(actual) != CCBT_int && ccqt_basic_kind(actual) != CCBT_unsigned))
                    return cc_printf_format_error(p, loc, "*", 1, arg_idx - first_vararg + 1, ccqt_basic(CCBT_int), actual);
            }
            arg_idx++;
            i++;
        }
        else {
            while(i < slen && fmt[i] >= '0' && fmt[i] <= '9') i++;
        }
        if(i >= slen)
            return cc_error(p, loc, "incomplete format specifier at end of string");
        if(fmt[i] == '.'){
            i++;
            if(i >= slen)
                return cc_error(p, loc, "incomplete format specifier at end of string");
            if(fmt[i] == '*'){
                expected_args++;
                if(arg_idx < nargs){
                    CcQualType actual = args[arg_idx]->type;
                    if(!ccqt_is_basic(actual) || (ccqt_basic_kind(actual) != CCBT_int && ccqt_basic_kind(actual) != CCBT_unsigned))
                        return cc_printf_format_error(p, loc, ".*", 2, arg_idx - first_vararg + 1, ccqt_basic(CCBT_int), actual);
                }
                arg_idx++;
                i++;
            }
            else {
                while(i < slen && fmt[i] >= '0' && fmt[i] <= '9') i++;
            }
        }
        if(i >= slen)
            return cc_error(p, loc, "incomplete format specifier at end of string");
        const char* spec_start = fmt + i;
        enum {
            LEN_NONE,
            LEN_h,    // h
            LEN_hh,   // hh
            LEN_l,    // l
            LEN_ll,   // ll
            LEN_L,    // L (long double)
            LEN_z,    // z
            LEN_j,    // j
            LEN_t,    // t
            LEN_I,    // I   (microslop, pointer-sized)
            LEN_I32,  // I32 (microslop)
            LEN_I64,  // I64 (microslop)
        } len_mod = LEN_NONE;
        if(fmt[i] == 'h'){
            i++;
            if(i < slen && fmt[i] == 'h'){
                len_mod = LEN_hh;
                i++;
            }
            else len_mod = LEN_h;
        }
        else if(fmt[i] == 'l'){
            i++;
            if(i < slen && fmt[i] == 'l'){
                len_mod = LEN_ll;
                i++;
            }
            else len_mod = LEN_l;
        }
        else if(fmt[i] == 'L'){
            len_mod = LEN_L;
            i++;
        }
        else if(fmt[i] == 'z'){
            len_mod = LEN_z;
            i++;
        }
        else if(fmt[i] == 'j'){
            len_mod = LEN_j;
            i++;
        }
        else if(fmt[i] == 't'){
            len_mod = LEN_t;
            i++;
        }
        else if(fmt[i] == 'I'){
            i++;
            if(i + 1 < slen && fmt[i] == '6' && fmt[i+1] == '4'){
                len_mod = LEN_I64;
                i += 2;
            }
            else if(i + 1 < slen && fmt[i] == '3' && fmt[i+1] == '2'){
                len_mod = LEN_I32;
                i += 2;
            }
            else len_mod = LEN_I;
        }
        if(i >= slen)
            return cc_error(p, loc, "incomplete format specifier at end of string");
        char conv = fmt[i];
        int spec_len = (int)(fmt + i + 1 - spec_start);
        CcQualType expected = {0};
        if(len_mod != LEN_NONE){
            _Bool valid = 1;
            switch(len_mod){
                case LEN_NONE: break;
                case LEN_h: case LEN_hh:
                case LEN_ll: case LEN_j: case LEN_z: case LEN_t:
                    switch(conv){
                        case 'b': case 'B': case 'd': case 'i':
                        case 'o': case 'u': case 'x': case 'X':
                        case 'n': break;
                        default: valid = 0;
                    }
                    break;
                case LEN_l:
                    switch(conv){
                        case 'b': case 'B': case 'd': case 'i':
                        case 'o': case 'u': case 'x': case 'X':
                        case 'n': case 'c': case 's':
                        case 'a': case 'A': case 'e': case 'E':
                        case 'f': case 'F': case 'g': case 'G':
                            break;
                        default: valid = 0;
                    }
                    break;
                case LEN_L:
                    switch(conv){
                        case 'a': case 'A': case 'e': case 'E':
                        case 'f': case 'F': case 'g': case 'G':
                            break;
                        default: valid = 0;
                    }
                    break;
                case LEN_I: case LEN_I32: case LEN_I64:
                    switch(conv){
                        case 'b': case 'B': case 'd': case 'i':
                        case 'o': case 'u': case 'x': case 'X':
                            break;
                        default: valid = 0;
                    }
                    break;
            }
            if(!valid)
                return cc_error(p, loc, "invalid length modifier for '%%%c' format specifier", conv);
        }
        switch(conv){
            case 'd': case 'i': case 'n':
                switch(len_mod){
                    DRP_CASES_EXHAUSTED;
                    case LEN_NONE: expected = ccqt_basic(CCBT_int); break;
                    case LEN_h:    expected = ccqt_basic(conv == 'n' ? CCBT_short : CCBT_int); break;
                    case LEN_hh:   expected = ccqt_basic(conv == 'n' ? CCBT_signed_char : CCBT_int); break;
                    case LEN_l:    expected = ccqt_basic(CCBT_long); break;
                    case LEN_ll:   expected = ccqt_basic(CCBT_long_long); break;
                    case LEN_z:    expected = ccqt_basic(ccbt_to_signed(tc->size_type)); break;
                    case LEN_j:    expected = ccqt_basic(tc->intmax_type); break;
                    case LEN_t:    expected = ccqt_basic(tc->ptrdiff_type); break;
                    case LEN_I:    expected = ccqt_basic(ccbt_to_signed(tc->size_type)); break;
                    case LEN_I32:  expected = ccqt_basic(CCBT_int); break;
                    case LEN_I64:  expected = ccqt_basic(CCBT_long_long); break;
                    case LEN_L: break;
                }
                if(conv == 'n'){
                    err = cc_pointer_of(p, expected, &expected);
                    if(err) return err;
                }
                break;
            case 'u': case 'x': case 'X': case 'o':
            case 'b': case 'B':
                switch(len_mod){
                    DRP_CASES_EXHAUSTED;
                    case LEN_NONE: expected = ccqt_basic(CCBT_unsigned); break;
                    case LEN_h:    expected = ccqt_basic(CCBT_unsigned); break;
                    case LEN_hh:   expected = ccqt_basic(CCBT_unsigned); break;
                    case LEN_l:    expected = ccqt_basic(CCBT_unsigned_long); break;
                    case LEN_ll:   expected = ccqt_basic(CCBT_unsigned_long_long); break;
                    case LEN_z:    expected = ccqt_basic(tc->size_type); break;
                    case LEN_j:    expected = ccqt_basic(ccbt_to_unsigned(tc->intmax_type)); break;
                    case LEN_t:    expected = ccqt_basic(ccbt_to_unsigned(tc->ptrdiff_type)); break;
                    case LEN_I:    expected = ccqt_basic(tc->size_type); break;
                    case LEN_I32:  expected = ccqt_basic(CCBT_unsigned); break;
                    case LEN_I64:  expected = ccqt_basic(CCBT_unsigned_long_long); break;
                    case LEN_L: break;
                }
                break;
            case 'f': case 'F': case 'e': case 'E':
            case 'g': case 'G': case 'a': case 'A':
                if(len_mod == LEN_L)
                    expected = ccqt_basic(CCBT_long_double);
                else
                    expected = ccqt_basic(CCBT_double);
                break;
            case 'c':
                if(len_mod == LEN_l)
                    expected = ccqt_basic(tc->wint_type);
                else
                    expected = ccqt_basic(CCBT_int);
                break;
            case 's':
                if(len_mod == LEN_l){
                    CcQualType wchar_qt = ccqt_basic(tc->wchar_type);
                    err = cc_pointer_of(p, wchar_qt, &expected);
                    if(err) return err;
                }
                else
                    expected = p->char_star;
                break;
            case 'p':
                expected = p->void_star;
                break;
            default:
                return cc_error(p, loc, "invalid format specifier '%%%c'", conv);
        }
        expected_args++;
        if(expected.bits && arg_idx < nargs){
            expected = cc_default_promotion(expected);
            CcQualType actual = args[arg_idx]->type;
            uint32_t arg_num = arg_idx - first_vararg + 1;
            if(conv == 's'){
                CcBasicTypeKind expect_elem = ccqt_basic_kind(ccqt_as_ptr(expected)->pointee);
                _Bool ok = ccqt_kind(actual) == CC_POINTER
                    && ccqt_is_basic(ccqt_as_ptr(actual)->pointee)
                    && ccqt_basic_kind(ccqt_as_ptr(actual)->pointee) == expect_elem;
                if(!ok)
                    return cc_printf_format_error(p, loc, spec_start, spec_len, arg_num, expected, actual);
            }
            else if(conv == 'n'){
                if(ccqt_kind(actual) != CC_POINTER
                    || !cc_types_compatible(ccqt_as_ptr(actual)->pointee, ccqt_as_ptr(expected)->pointee))
                    return cc_printf_format_error(p, loc, spec_start, spec_len, arg_num, expected, actual);
            }
            else if(conv == 'p'){
                if(ccqt_kind(actual) != CC_POINTER && !ccqt_bt_eq(actual, CCBT_nullptr_t))
                    return cc_printf_format_error(p, loc, spec_start, spec_len, arg_num, expected, actual);
            }
            else {
                _Bool ok = ccqt_is_basic(actual) && ccqt_basic_kind(actual) == ccqt_basic_kind(expected);
                if(!ok && ccqt_is_basic(actual)){
                    CcBasicTypeKind ek = ccqt_basic_kind(expected);
                    CcBasicTypeKind ak = ccqt_basic_kind(actual);
                    ok = ccbt_is_integer(ek) && ccbt_is_integer(ak)
                        && tc->sizeof_[ek] == tc->sizeof_[ak];
                }
                if(!ok)
                    return cc_printf_format_error(p, loc, spec_start, spec_len, arg_num, expected, actual);
            }
        }
        arg_idx++;
    }
    if(!format->first_to_check) return 0;
    uint32_t provided = nargs - first_vararg;
    if(expected_args > provided)
        return cc_error(p, loc, "format specifies %u argument%s, but only %u provided",
            expected_args, expected_args == 1 ? "" : "s", provided);
    if(expected_args < provided)
        return cc_error(p, loc, "format specifies %u argument%s, but %u provided",
            expected_args, expected_args == 1 ? "" : "s", provided);
    return 0;
}

static
int
cc_next_token_packed(CcParser* p, CcToken* tok){
    if(p->pending.count){
        *tok = ma_pop(CcToken)(&p->pending);
        return 0;
    }
    return cpp_next_c_token(&p->cpp, tok);
}

static
int
cc_next_token(CcParser* p, CcToken* tok){
    int err = cc_next_token_packed(p, tok);
    if(err || tok->type != CC_EMBED) return err;
    CcToken tail = *tok;
    *tok = (CcToken){.constant={
        .type=CC_CONSTANT,
        .ctype=CCBT_int,
        .loc=tail.loc,
        .integer_value=((const unsigned char*)tail.embed.bytes.buff)[0]
    }};
    if(--tail.embed.bytes.n_bytes){
        tail.embed.bytes.buff = (const unsigned char*)tail.embed.bytes.buff+1;
        err = cc_unget(p, &tail);
        if(err) return err;
        CcToken comma = {.punct={
            .type=CC_PUNCTUATOR,
            .punct=',',
            .loc=tail.loc
        }};
        return cc_unget(p, &comma);
    }
    return 0;
}

static
int
cc_unget(CcParser* p, CcToken* tok){
    return ma_push(CcToken)(&p->pending, cc_allocator(p), *tok);
}

static
int
cc_peek(CcParser* p, CcToken* tok){
    int err = cc_next_token(p, tok);
    if(err) return err;
    return cc_unget(p, tok);
}

static int cc_expect_punct(CcParser* p, CcPunct punct){ return cc_expect_punct_supp(p, punct, "");}
static
int
cc_expect_punct_supp(CcParser* p, CcPunct punct, const char* supp){
    CcToken tok;
    int err = cc_next_token(p, &tok);
    if(err) return err;
    if(tok.type != CC_PUNCTUATOR || tok.punct.punct != punct){
        if(punct == ';' && tok.type == CC_EOF)
            return 0;
        char buf[4];
        int len = 0;
        uint32_t v = (uint32_t)punct;
        if(v > 0xFFFF){
            buf[len++] = (char)(v >> 16);
            buf[len++] = (char)(v >> 8);
            buf[len++] = (char)v;
        }
        else if(v > 0xFF){
            buf[len++] = (char)(v >> 8);
            buf[len++] = (char)v;
        }
        else {
            buf[len++] = (char)v;
        }
        buf[len] = 0;
        return cc_error(p, tok.loc, "Expected '%s'%s", buf, supp);
    }
    return 0;
}

static
CcExpr* _Nullable
_cc_alloc_expr(CcParser* p, size_t nvalues){
    #if CC_RECYCLE_EXPRS
    if(nvalues < countof(p->exprs)){
        CcExpr* n = fl_pop(&p->exprs[nvalues]);
        if(n){
            memset(n, 0, sizeof *n + nvalues * sizeof *n->values);
            return n;
        }
    }
    #endif
    CcExpr* e = Allocator_zalloc_extra(cc_allocator(p), nvalues, sizeof *e->values, sizeof *e, _Alignof(CcExpr));
    return e;
}
static
void
_cc_release_expr(CcParser* p, CcExpr* e, size_t nvalues){
    #if CC_RECYCLE_EXPRS
    if(nvalues < sizeof p->exprs / sizeof p->exprs[0]){
        fl_push(&p->exprs[nvalues], e);
        return;
    }
    #endif
    Allocator_free_extra(cc_allocator(p), e, nvalues, sizeof *e->values, sizeof *e, _Alignof(CcExpr));
}
static
size_t
cc_expr_nvalues(CcExpr* e){
    switch(e->kind){
        case CC_EXPR_BIT_BUILTIN:
            return e->bit_builtin.nargs;
        case CC_EXPR_OBJECT_VIEW:
        case CC_EXPR_NEG:
        case CC_EXPR_POS:
        case CC_EXPR_BITNOT:
        case CC_EXPR_LOGNOT:
        case CC_EXPR_DEREF:
        case CC_EXPR_ADDR:
        case CC_EXPR_PREINC:
        case CC_EXPR_PREDEC:
        case CC_EXPR_POSTINC:
        case CC_EXPR_POSTDEC:
        case CC_EXPR_CAST:
        case CC_EXPR_SIZEOF_VMT:
        case CC_EXPR_BSWAP:
        case CC_EXPR_ALLOCA:
        case CC_EXPR_INTERN:
        case CC_EXPR_SRCLOC_REFLECT:
        case CC_EXPR_STATEMENT_EXPRESSION:
            return 0;
        case CC_EXPR_COMPILE:
            return 1;
        case CC_EXPR_MODULE_REFLECT:
            switch(e->module.op){
                case CC_MODULE_NONE:
                case CC_MODULE_FUNC_COUNT:
                case CC_MODULE_FUNC:
                case CC_MODULE_FUNC_INFO:
                case CC_MODULE_VAR_COUNT:
                case CC_MODULE_VAR:
                case CC_MODULE_TYPE_COUNT:
                case CC_MODULE_TYPE:
                case CC_MODULE_RUN:
                     return 0;
                case CC_MODULE_PARSE_TYPE:
                case CC_MODULE_SYMBOL:
                    return 1;
                DRP_CASES_EXHAUSTED;
            }
        case CC_EXPR_HOTSWAP:
            return 1;
        case CC_EXPR_BLOB:
        case CC_EXPR_VALUE:
        case CC_EXPR_VARIABLE:
        case CC_EXPR_FUNCTION:
            return 0;
        case CC_EXPR_BUILTIN:
            return e->builtin.op == CC_BUILTIN_COMPLEX
                || e->builtin.op == CC_BUILTIN_CAPTURE_BACKTRACE
                || e->builtin.op == CC_BUILTIN_CLEAR_CACHE
                || e->builtin.op == CC_BUILTIN_SYMBOLICATE ? 2 : 0;
        case CC_EXPR_ADD:
        case CC_EXPR_SUB:
        case CC_EXPR_MUL:
        case CC_EXPR_DIV:
        case CC_EXPR_MOD:
        case CC_EXPR_BITAND:
        case CC_EXPR_BITOR:
        case CC_EXPR_BITXOR:
        case CC_EXPR_LSHIFT:
        case CC_EXPR_RSHIFT:
        case CC_EXPR_LOGAND:
        case CC_EXPR_LOGOR:
        case CC_EXPR_EQ:
        case CC_EXPR_NE:
        case CC_EXPR_LT:
        case CC_EXPR_GT:
        case CC_EXPR_LE:
        case CC_EXPR_GE:
        case CC_EXPR_ASSIGN:
        case CC_EXPR_ADDASSIGN:
        case CC_EXPR_SUBASSIGN:
        case CC_EXPR_MULASSIGN:
        case CC_EXPR_DIVASSIGN:
        case CC_EXPR_MODASSIGN:
        case CC_EXPR_BITANDASSIGN:
        case CC_EXPR_BITORASSIGN:
        case CC_EXPR_BITXORASSIGN:
        case CC_EXPR_LSHIFTASSIGN:
        case CC_EXPR_RSHIFTASSIGN:
        case CC_EXPR_COMMA:
        case CC_EXPR_SUBSCRIPT:
        case CC_EXPR_DOT:
        case CC_EXPR_ARROW:
            return 1;
        case CC_EXPR_TERNARY:
        case CC_EXPR_ADD_OVERFLOW:
        case CC_EXPR_MUL_OVERFLOW:
        case CC_EXPR_SUB_OVERFLOW:
        case CC_EXPR_UMUL128:
            return 2;
        case CC_EXPR_CALL:
            return e->call.nargs;
        case CC_EXPR_VA:
            return (e->va.op == CC_VA_COPY) ? 1 : 0;
        case CC_EXPR_ATOMIC:
            switch(e->atomic.op){
                case CC_ATOMIC_LOAD_N:
                case CC_ATOMIC_THREAD_FENCE:
                case CC_ATOMIC_SIGNAL_FENCE:
                case CC_ATOMIC_INTERLOCKED_INCREMENT:
                case CC_ATOMIC_INTERLOCKED_DECREMENT:
                    return 0;
                case CC_ATOMIC_STORE_N:
                case CC_ATOMIC_FETCH_ADD:
                case CC_ATOMIC_FETCH_SUB:
                case CC_ATOMIC_ADD_FETCH:
                case CC_ATOMIC_SUB_FETCH:
                case CC_ATOMIC_FETCH_AND:
                case CC_ATOMIC_FETCH_OR:
                case CC_ATOMIC_FETCH_XOR:
                case CC_ATOMIC_EXCHANGE_N:
                case CC_ATOMIC_LOAD:
                case CC_ATOMIC_STORE:
                    return 1;
                case CC_ATOMIC_EXCHANGE:
                case CC_ATOMIC_INTERLOCKED_COMPARE_EXCHANGE:
                    return 2;
                case CC_ATOMIC_COMPARE_EXCHANGE_N:
                case CC_ATOMIC_COMPARE_EXCHANGE:
                case CC_ATOMIC_INTERLOCKED_COMPARE_EXCHANGE128:
                    return 3;
            }
            return 0;
        case CC_EXPR_TYPE_INTROSPECTION:
            switch(e->type_introspection.op){
                case CC_TYPE_IS_CALLABLE_WITH:
                case CC_TYPE_IS_CALLABLE_THROUGH:
                case CC_TYPE_CASTABLE_TO:
                case CC_TYPE_MAKE_ANY:
                case CC_TYPE_FIELD:
                case CC_TYPE_METHOD:
                case CC_TYPE_HAS_FIELD:
                case CC_TYPE_HAS_METHOD:
                case CC_TYPE_PARAM_TYPE:
                case CC_TYPE_ENUMERATOR:
                    return 1;
                case CC_TYPE_ALIGNOF:
                case CC_TYPE_COUNT:
                case CC_TYPE_LOC:
                case CC_TYPE_ELEMENT_TYPE:
                case CC_TYPE_ENUMERATORS:
                case CC_TYPE_FIELDS:
                case CC_TYPE_METHODS:
                case CC_TYPE_IS_ARITHMETIC:
                case CC_TYPE_IS_ARRAY:
                case CC_TYPE_IS_VECTOR:
                case CC_TYPE_IS_SLICE:
                case CC_TYPE_IS_ATOMIC:
                case CC_TYPE_IS_CALLABLE:
                case CC_TYPE_IS_CONST:
                case CC_TYPE_IS_ENUM:
                case CC_TYPE_IS_FLOAT:
                case CC_TYPE_IS_FUNCTION:
                case CC_TYPE_IS_INCOMPLETE:
                case CC_TYPE_IS_VALID:
                case CC_TYPE_IS_INVALID:
                case CC_TYPE_IS_INTEGER:
                case CC_TYPE_IS_POINTER:
                case CC_TYPE_IS_SIGNED:
                case CC_TYPE_IS_STRUCT:
                case CC_TYPE_IS_UNION:
                case CC_TYPE_IS_UNSIGNED:
                case CC_TYPE_IS_VARIADIC:
                case CC_TYPE_IS_VOLATILE:
                case CC_TYPE_NAME:
                case CC_TYPE_NONE:
                case CC_TYPE_PARAM_COUNT:
                case CC_TYPE_POINTEE:
                case CC_TYPE_PUSH_METHOD:
                case CC_TYPE_RETURN_TYPE:
                case CC_TYPE_SIZEOF:
                case CC_TYPE_TAG:
                case CC_TYPE_UNDERLYING_TYPE:
                case CC_TYPE_UNQUAL:
                    return 0;
                DRP_CASES_EXHAUSTED;
            }
        case CC_EXPR_COMPOUND_LITERAL:
        case CC_EXPR_INIT_LIST:
            return 0;
        case CC_EXPR_SLICE:
            return 2;
        case CC_EXPR_SLICE_LO:
            return 1;
        case CC_EXPR_SLICE_HI:
            return 1;
        case CC_EXPR_SLICE_ALL:
            return 0;
    }
    return 0;
}

static
void
cc_release_expr(CcParser* p, CcExpr* e){
    size_t nvalues = cc_expr_nvalues(e);
    for(size_t i = 0; i < nvalues; i++)
        cc_release_expr(p, e->values[i]);
    switch(e->kind){
        case CC_EXPR_BLOB:
        case CC_EXPR_VALUE:
        case CC_EXPR_VARIABLE:
        case CC_EXPR_FUNCTION:
        case CC_EXPR_BUILTIN:
            break;
        case CC_EXPR_ARROW:
        case CC_EXPR_DOT:
            cc_field_path_free(cc_allocator(p), e->field_path);
            break;
        case CC_EXPR_COMPOUND_LITERAL:
        case CC_EXPR_INIT_LIST:
            if(!e->init_list->rc--){
                for(uint32_t i = 0; i < e->init_list->count; i++){
                    cc_field_path_free(cc_allocator(p), e->init_list->entries[i].path);
                    if(e->init_list->entries[i].value)
                        cc_release_expr(p, e->init_list->entries[i].value);
                }
                Allocator_free_extra(cc_allocator(p), e->init_list, e->init_list->count, sizeof *e->init_list->entries, sizeof *e->init_list, _Alignof(CcInitList));
            }
            break;
        case CC_EXPR_STATEMENT_EXPRESSION:
            cc_free_stmt_tree(p, e->stmt_body);
            break;
        case CC_EXPR_OBJECT_VIEW:
        case CC_EXPR_ADD:
        case CC_EXPR_ADDASSIGN:
        case CC_EXPR_ADDR:
        case CC_EXPR_ADD_OVERFLOW:
        case CC_EXPR_ALLOCA:
        case CC_EXPR_ASSIGN:
        case CC_EXPR_ATOMIC:
        case CC_EXPR_BITAND:
        case CC_EXPR_BITANDASSIGN:
        case CC_EXPR_BITNOT:
        case CC_EXPR_BITOR:
        case CC_EXPR_BITORASSIGN:
        case CC_EXPR_BITXOR:
        case CC_EXPR_BITXORASSIGN:
        case CC_EXPR_CALL:
        case CC_EXPR_CAST:
        case CC_EXPR_COMMA:
        case CC_EXPR_BSWAP:
        case CC_EXPR_DEREF:
        case CC_EXPR_DIV:
        case CC_EXPR_DIVASSIGN:
        case CC_EXPR_EQ:
        case CC_EXPR_GE:
        case CC_EXPR_GT:
        case CC_EXPR_INTERN:
        case CC_EXPR_LE:
        case CC_EXPR_LOGAND:
        case CC_EXPR_LOGNOT:
        case CC_EXPR_LOGOR:
        case CC_EXPR_LSHIFT:
        case CC_EXPR_LSHIFTASSIGN:
        case CC_EXPR_LT:
        case CC_EXPR_MOD:
        case CC_EXPR_MODASSIGN:
        case CC_EXPR_MUL:
        case CC_EXPR_MULASSIGN:
        case CC_EXPR_MUL_OVERFLOW:
        case CC_EXPR_NE:
        case CC_EXPR_NEG:
        case CC_EXPR_BIT_BUILTIN:
        case CC_EXPR_POS:
        case CC_EXPR_POSTDEC:
        case CC_EXPR_POSTINC:
        case CC_EXPR_PREDEC:
        case CC_EXPR_PREINC:
        case CC_EXPR_RSHIFT:
        case CC_EXPR_RSHIFTASSIGN:
        case CC_EXPR_SIZEOF_VMT:
        case CC_EXPR_SUB:
        case CC_EXPR_SUBASSIGN:
        case CC_EXPR_SUBSCRIPT:
        case CC_EXPR_SUB_OVERFLOW:
        case CC_EXPR_HOTSWAP:
        case CC_EXPR_SRCLOC_REFLECT:
        case CC_EXPR_COMPILE:
        case CC_EXPR_MODULE_REFLECT:
        case CC_EXPR_TERNARY:
        case CC_EXPR_TYPE_INTROSPECTION:
        case CC_EXPR_UMUL128:
        case CC_EXPR_VA:
        case CC_EXPR_SLICE:
        case CC_EXPR_SLICE_LO:
        case CC_EXPR_SLICE_HI:
        case CC_EXPR_SLICE_ALL:
            if(e->lhs)
                cc_release_expr(p, e->lhs);
            break;
    }
    _cc_release_expr(p, e, nvalues);
}

static
CcExpr*_Nullable
cc_make_expr(CcParser* p, CcExprKind kind, SrcLoc loc, CcQualType type, size_t nvalues){
    if(p->current_func && (kind == CC_EXPR_ALLOCA || kind == CC_EXPR_VA))
        p->current_func->inline_blocked = 1;
    CcExpr* node = _cc_alloc_expr(p, nvalues);
    if(node){
        node->kind = kind;
        node->loc = loc;
        node->type = type;
    }
    return node;
}

static
CcExpr* _Nullable
cc_value_expr(CcParser* p, SrcLoc loc, CcQualType type){
    CcExpr* node = _cc_alloc_expr(p, 0);
    if(!node) return NULL;
    node->kind = CC_EXPR_VALUE;
    node->loc = loc;
    node->type = type;
    return node;
}

static
CcExpr*_Nullable
cc_integer_bits_expr(CcParser* p, SrcLoc loc, CcQualType type, CiUint128 bits){
    CcExpr* node = cc_value_expr(p, loc, type);
    if(node){
        CcQualType t = type;
        t = ccqt_enum_underlying(t);
        if(ccqt_bt_eq(t, CCBT_int128) || ccqt_bt_eq(t, CCBT_unsigned_int128))
            node->uinteger128 = bits;
        else node->uinteger = ci_uint128_lo(bits);
    }
    return node;
}

static
CcExpr*_Nullable
cc_int64_expr(CcParser* p, SrcLoc loc, CcQualType type, int64_t v){
    return cc_integer_bits_expr(p, loc, type, ci_uint128_from_int64(v));
}

static
CcExpr*_Nullable
cc_uint64_expr(CcParser* p, SrcLoc loc, CcQualType type, uint64_t v){
    return cc_integer_bits_expr(p, loc, type, ci_uint128_from_uint64(v));
}

static
CcExpr*_Nullable
cc_constexpr_string_slice_expr(CcParser* p, SrcLoc loc, Atom name, _Bool include_nul){
    CcInitList* il = Allocator_zalloc_extra(cc_allocator(p), 2, sizeof *il->entries, sizeof *il, _Alignof(CcInitList));
    if(!il) return NULL;
    CcExpr *count = NULL, *data = NULL, *literal = NULL, *node = NULL;
    if(0){
        oom:
        if(node) cc_release_expr(p, node);
        if(data) cc_release_expr(p, data);
        if(literal) cc_release_expr(p, literal);
        if(count) cc_release_expr(p, count);
        Allocator_free_extra(cc_allocator(p), il, 2, sizeof *il->entries, sizeof *il, _Alignof(CcInitList));
        return NULL;
    }
    il->loc = loc;
    il->count = 2;
    count = cc_uint64_expr(p, loc, ccqt_basic(cc_target(p)->size_type), name->length+include_nul);
    if(!count) goto oom;
    CcArray* array = cc_intern_array(&p->type_cache, cc_allocator(p), ccqt_as_ptr(p->const_char_star)->pointee, name->length + 1, 0);
    if(!array) goto oom;
    literal = cc_value_expr(p, loc, (CcQualType){.bits=(uintptr_t)array});
    if(!literal) goto oom;
    literal->text = name->data;
    literal->str.length = name->length + 1;
    data = cc_make_expr(p, CC_EXPR_CAST, loc, p->const_char_star, 0);
    if(!data) goto oom;
    data->lhs = literal;
    literal = NULL;
    node = cc_make_expr(p, CC_EXPR_INIT_LIST, loc, p->const_char_slice, 0);
    if(!node) goto oom;
    il->entries[0].path = (CcFieldPath){.n_components=1, .idx0=0};
    il->entries[0].value = count;
    il->entries[1].path = (CcFieldPath){.n_components=1, .idx0=1};
    il->entries[1].value = data;
    node->init_list = il;
    return node;
}

static
void
cc_mark_mutable_storage(CcExpr* e){
    for(;;){
        switch((uint32_t)e->kind){
            case CC_EXPR_VARIABLE:
                e->var->mutable_storage = 1;
                return;
            case CC_EXPR_DOT: case CC_EXPR_COMMA:
                e = e->values[0];
                break;
            case CC_EXPR_CAST:
                e = e->lhs;
                break;
            case CC_EXPR_SUBSCRIPT:
                if(!ccqt_is_fixed_sequence(e->lhs->type) && ccqt_kind(e->lhs->type) != CC_VLA)
                    return;
                e = e->lhs;
                break;
            default:
                return;
        }
    }
}

static
CcExpr* _Nullable
cc_unary_expr(CcParser* p, CcExprKind kind, SrcLoc loc, CcQualType type, CcExpr* operand){
    CcExpr* node = _cc_alloc_expr(p, 0);
    if(!node) return NULL;
    node->kind = kind;
    node->loc = loc;
    node->type = type;
    node->lhs = operand;
    if(kind == CC_EXPR_ADDR
    || kind == CC_EXPR_PREINC
    || kind == CC_EXPR_PREDEC
    || kind == CC_EXPR_POSTINC
    || kind == CC_EXPR_POSTDEC
    || (kind == CC_EXPR_CAST && ccqt_is_array(operand->type) && (ccqt_kind(type) == CC_POINTER || ccqt_kind(type) == CC_SLICE)))
        cc_mark_mutable_storage(operand);
    return node;
}

static
CcExpr* _Nullable
cc_binary_expr(CcParser* p, CcExprKind kind, SrcLoc loc, CcQualType type, CcExpr* left, CcExpr* right){
    CcExpr* node = _cc_alloc_expr(p, 1);
    if(!node) return NULL;
    node->kind = kind;
    node->loc = loc;
    node->type = type;
    node->lhs = left;
    node->values[0] = right;
    return node;
}
static
int
cc_pointer_of(CcParser* p, CcQualType pointee, CcQualType* out){
    pointee = cc_intern_qualtype(p, pointee);
    if(!pointee.bits) return CC_OOM_ERROR;
    CcPointer* ptr = cc_intern_pointer(&p->type_cache, cc_allocator(p), pointee, 0, 0);
    if(!ptr) return CC_OOM_ERROR;
    *out = (CcQualType){.bits = (uintptr_t)ptr};
    return 0;
}

static
int
cc_slice_of(CcParser* p, CcQualType pointee, SrcLoc loc, CcQualType* out){
    pointee = cc_intern_qualtype(p, pointee);
    if(!pointee.bits) return CC_OOM_ERROR;
    CcSlice* slice = cc_intern_slice(&p->type_cache, cc_allocator(p), pointee, 0);
    if(!slice) return CC_OOM_ERROR;
    *out = (CcQualType){.bits = (uintptr_t)slice};
    return cc_check_derived_type(p, *out, loc);
}

static
int
cc_va_list_to_ptr(CcParser* p, SrcLoc loc, CcExpr*_Nonnull*_Nonnull e, _Bool writable){
    CcExpr* expr = *e;
    if(ccqt_kind(expr->type) == CC_POINTER){
        CcQualType object = ccqt_as_ptr(expr->type)->pointee;
        CcQualType expected = ccqt_as_ptr(p->builtin_va_list_ptr)->pointee;
        if(writable ? cc_types_compatible(object, expected)
            : cc_types_compatible_ignoring_cv(object, expected))
            return 0;
    }
    if(!(writable ? cc_types_compatible(expr->type, p->builtin_va_list)
        : cc_types_compatible_ignoring_cv(expr->type, p->builtin_va_list)))
        return cc_type_error(p, loc, (*e)->type, "expression does not have type va_list");
    CcQualType pointer;
    int err = cc_pointer_of(p, ccqt_is_array(expr->type)
        ? ccqt_qualified_array_element(expr->type) : expr->type, &pointer);
    if(err) return err;
    if(ccqt_is_array(expr->type)){
        return cc_implicit_cast(p, expr, pointer, e);
    }
    if(!expr->is_lvalue)
        return cc_error(p, loc, "va_list argument must be an lvalue");
    CcExpr* addr = cc_make_expr(p, CC_EXPR_ADDR, loc, pointer, 0);
    if(!addr) return CC_OOM_ERROR;
    addr->lhs = expr;
    cc_mark_mutable_storage(expr);
    *e = addr;
    return 0;
}

static
Marray(CcToken)*_Nullable
cc_get_scratch(CcParser* p){
    Marray(CcToken)* toks = fl_pop(&p->scratch_tokens);
    if(!toks) toks = Allocator_zalloc(cc_allocator(p), 1, sizeof *toks, _Alignof(Marray(CcToken)));
    if(!toks) return toks;
    toks->count = 0;
    return toks;
}

static
void
cc_release_scratch(CcParser* p, Marray(CcToken)* toks){
    fl_push(&p->scratch_tokens, toks);
}

static
Allocator
cc_allocator(CcParser*p){
    return p->cpp.allocator;
}
static
Allocator
cc_scratch_allocator(CcParser*p){
    return allocator_from_arena(&p->scratch_arena);
}

static
void
cc_add_alignment_attribute(CcAttributes* attrs, uint16_t alignment){
    if(alignment > attrs->aligned) attrs->aligned = alignment;
    attrs->has_aligned = 1;
}

static
int
cc_parse_alignment_argument(CcParser* p, CcParseFlags flags, SrcLoc loc, CcAttributes* attrs, const char* name){
    int err = cc_expect_punct(p, CC_lparen);
    if(err) return err;
    flags.vc = CC_CONSTEXPR_VALUE;
    flags.unevaluated = 1;
    CcExpr* expr = NULL;
    err = cc_parse_assignment_expr(p, flags, &expr, CCQT_NONE);
    if(err) return err;
    if(!expr)
        return cc_error(p, loc, "expected constant expression for %s", name);
    int64_t align_i;
    err = cc_eval_integer(&(CcEvalCtx){p}, expr, &align_i);
    cc_release_expr(p, expr);
    if(err && err != CC_NOT_CONSTANT_ERROR) return err;
    if(err)
        return cc_error(p, loc, "%s requires a constant integral expression", name);
    uint64_t align = (uint64_t)align_i;
    if(align == 0 || (align & (align - 1)) != 0)
        return cc_error(p, loc, "alignment must be a positive power of 2");
    if(align > UINT16_MAX)
        return cc_error(p, loc, "alignment too large");
    cc_add_alignment_attribute(attrs, (uint16_t)align);
    return cc_expect_punct(p, CC_rparen);
}

// cc_match_gnu_attribute
// ----------------------
// Tries to match a gnu-style attribute.
// On match, parses any arguments and updates *attrs.
// For unrecognized names, skips any argument list.
//
// Arguments:
// ----------
// p:
//     The parser.
//
// loc:
//     Source location for error reporting.
//
// attr_name:
//     Canonicalized attribute name (without any __).
//
// attrs:
//     Attribute flags to update.
//
// Returns:
// --------
// 0 on success. Error code on failure.
static
int
cc_match_gnu_attribute(CcParser* p, CcParseFlags flags, SrcLoc loc, StringView attr_name, CcAttributes* attrs){
    CcParseFlags constexpr_flags = flags;
    constexpr_flags.vc = CC_CONSTEXPR_VALUE;
    int err = 0;
    CcToken tok;
    if(sv_equals(attr_name, SV("packed"))){
        attrs->packed = 1;
    }
    else if(sv_equals(attr_name, SV("always_inline"))){
        attrs->always_inline = 1;
    }
    else if(sv_equals(attr_name, SV("noinline"))){
        attrs->noinline = 1;
    }
    else if(sv_equals(attr_name, SV("transparent_union"))){
        attrs->transparent_union = 1;
    }
    else if(sv_equals(attr_name, SV("aligned"))){
        err = cc_peek(p, &tok);
        if(err) return err;
        if(tok.type == CC_PUNCTUATOR && tok.punct.punct == CC_lparen){
            err = cc_parse_alignment_argument(p, flags, loc, attrs, "aligned attribute");
            if(err) return err;
        }
        else {
            cc_add_alignment_attribute(attrs, cc_target(p)->max_align);
        }
    }
    else if(sv_equals(attr_name, SV("vector_size"))){
        constexpr_flags.unevaluated = 1;
        err = cc_expect_punct(p, CC_lparen);
        if(err) return err;
        CcExpr* expr = NULL;
        err = cc_parse_assignment_expr(p, constexpr_flags, &expr, CCQT_NONE);
        if(err) return err;
        if(!expr)
            return cc_error(p, loc, "expected constant expression for vector_size attribute");
        int64_t vs_i;
        err = cc_eval_integer(&(CcEvalCtx){p}, expr, &vs_i);
        cc_release_expr(p, expr);
        if(err && err != CC_NOT_CONSTANT_ERROR) return err;
        if(err)
            return cc_error(p, loc, "vector_size attribute requires a constant integral expression");
        uint64_t vs = (uint64_t)vs_i;
        if(vs == 0 || (vs & (vs - 1)) != 0)
            return cc_error(p, loc, "vector_size must be a power of 2");
        if(vs > UINT16_MAX)
            return cc_error(p, loc, "vector_size too large");
        attrs->vector_size = (uint16_t)vs;
        err = cc_expect_punct(p, CC_rparen);
        if(err) return err;
    }
    else if(sv_equals(attr_name, SV("format"))){
        constexpr_flags.unevaluated = 1;
        err = cc_expect_punct(p, CC_lparen);
        if(err) return err;
        CcToken arch_tok;
        err = cc_next_token(p, &arch_tok);
        if(err) return err;
        if(arch_tok.type != CC_IDENTIFIER)
            return cc_error(p, arch_tok.loc, "expected format archetype (e.g. 'printf')");
        StringView arch = {.text = arch_tok.ident.ident->data, .length = arch_tok.ident.ident->length};
        if(sv_startswith(arch, SV("__")))
            arch = sv_slice(arch, 2, arch.length);
        if(sv_endswith(arch, SV("__")))
            arch = sv_slice(arch, 0, arch.length-2);
        _Bool printf_ = sv_equals(arch, SV("printf"));
        uint32_t indices[2] = {0};
        for(unsigned i = 0; i < 2; i++){
            err = cc_expect_punct(p, CC_comma);
            if(err) return err;
            CcExpr* expr = NULL;
            err = cc_parse_assignment_expr(p, constexpr_flags, &expr, CCQT_NONE);
            if(err) return err;
            if(!expr) return cc_error(p, loc, "expected format argument index");
            if(printf_){
                int64_t index = 0;
                SrcLoc index_loc = expr->loc;
                err = ccqt_is_integer(expr->type)
                    ? cc_eval_integer(&(CcEvalCtx){.parser=p}, expr, &index) : CC_NOT_CONSTANT_ERROR;
                cc_release_expr(p, expr);
                if(err && err != CC_NOT_CONSTANT_ERROR && err != CC_OVERFLOW_ERROR) return err;
                if(err || index < (i ? 0 : 1) || (uint64_t)index > UINT32_MAX)
                    return cc_error(p, index_loc, "format argument index must be an integer constant in range");
                indices[i] = (uint32_t)index;
            }
            else cc_release_expr(p, expr);
        }
        err = cc_expect_punct(p, CC_rparen);
        if(err) return err;
        if(printf_){
            CcPrintfFormat format = {.string_index=indices[0], .first_to_check=indices[1], .loc=loc};
            err = cc_merge_printf_formats(p, &format, &attrs->printf_formats);
            if(err) return err;
        }
    }
    else {
        err = cc_peek(p, &tok);
        if(err) return err;
        if(tok.type == CC_PUNCTUATOR && tok.punct.punct == CC_lparen){
            err = cc_next_token(p, &tok);
            if(err) return err;
            int depth = 1;
            while(depth > 0){
                err = cc_next_token(p, &tok);
                if(err) return err;
                if(tok.type == CC_EOF)
                    return cc_error(p, loc, "unterminated attribute argument list");
                if(tok.type == CC_PUNCTUATOR){
                    if(tok.punct.punct == CC_lparen) depth++;
                    else if(tok.punct.punct == CC_rparen) depth--;
                }
            }
        }
    }
    return 0;
}

static
int
cc_parse_attributes(CcParser* p, CcParseFlags flags, CcAttributes* attrs){
    int err = 0;
    CcToken tok;
    for(;;){
        err = cc_parse_c23_attributes(p, flags, attrs);
        if(err) return err;
        err = cc_peek(p, &tok);
        if(err) return err;
        if(tok.type != CC_KEYWORD || tok.kw.kw != CC___attribute__)
            return 0;
        err = cc_next_token(p, &tok);
        if(err) return err;
        err = cc_expect_punct(p, CC_lparen);
        if(err) return err;
        err = cc_expect_punct(p, CC_lparen);
        if(err) return err;
        err = cc_peek(p, &tok);
        if(err) return err;
        if(tok.type == CC_PUNCTUATOR && tok.punct.punct == CC_rparen)
            goto close_parens;
        for(;;){
            err = cc_next_token(p, &tok);
            if(err) return err;
            if(tok.type != CC_IDENTIFIER && tok.type != CC_KEYWORD)
                return cc_error(p, tok.loc, "expected attribute name");
            StringView attr_name;
            if(tok.type == CC_IDENTIFIER)
                attr_name = (StringView){.text = tok.ident.ident->data, .length = tok.ident.ident->length};
            else
                attr_name = SV("");
            if(sv_startswith(attr_name, SV("__")))
                attr_name = sv_slice(attr_name, 2, attr_name.length);
            if(sv_endswith(attr_name, SV("__")))
                attr_name = sv_slice(attr_name, 0, attr_name.length-2);
            err = cc_match_gnu_attribute(p, flags, tok.loc, attr_name, attrs);
            if(err) return err;
            err = cc_peek(p, &tok);
            if(err) return err;
            if(tok.type == CC_PUNCTUATOR && tok.punct.punct == CC_comma){
                err = cc_next_token(p, &tok);
                if(err) return err;
                continue;
            }
            break;
        }
        close_parens:
        err = cc_expect_punct(p, CC_rparen);
        if(err) return err;
        err = cc_expect_punct(p, CC_rparen);
        if(err) return err;
    }
}

static
_Bool
cc_is_c23_attribute_start(CcParser* p){
    CcToken t1, t2;
    int err = cc_peek(p, &t1);
    if(err) return 0;
    if(t1.type != CC_PUNCTUATOR || t1.punct.punct != CC_lbracket)
        return 0;
    err = cc_next_token(p, &t1);
    if(err) return 0;
    err = cc_peek(p, &t2);
    cc_unget(p, &t1);
    if(err) return 0;
    return t2.type == CC_PUNCTUATOR && t2.punct.punct == CC_lbracket;
}

static
int
cc_parse_c23_attributes(CcParser* p, CcParseFlags flags, CcAttributes* attrs){
    int err = 0;
    CcToken tok;
    for(;;){
        if(!cc_is_c23_attribute_start(p))
            return 0;
        err = cc_next_token(p, &tok);
        if(err) return err;
        SrcLoc attr_loc = tok.loc;
        err = cc_next_token(p, &tok);
        if(err) return err;
        err = cc_peek(p, &tok);
        if(err) return err;
        if(tok.type == CC_PUNCTUATOR && tok.punct.punct == CC_rbracket)
            goto close_brackets;
        for(;;){
            err = cc_peek(p, &tok);
            if(err) return err;
            if(tok.type == CC_PUNCTUATOR && tok.punct.punct == CC_rbracket)
                break;
            err = cc_next_token(p, &tok);
            if(err) return err;
            if(tok.type != CC_IDENTIFIER && tok.type != CC_KEYWORD)
                return cc_error(p, tok.loc, "expected attribute name");
            if(tok.type == CC_KEYWORD){
                if(tok.kw.kw == CC__Noreturn)
                    attrs->is_noreturn = 1;
                goto c23_skip_args;
            }
            StringView attr_name = {.text = tok.ident.ident->data, .length = tok.ident.ident->length};
            _Bool is_gnu = 0,
                  is_drc = 0;
            err = cc_peek(p, &tok);
            if(err) return err;
            if(tok.type == CC_PUNCTUATOR && tok.punct.punct == CC_double_colon){
                err = cc_next_token(p, &tok);
                if(err) return err;
                StringView prefix = attr_name;
                if(sv_startswith(prefix, SV("__")))
                    prefix = sv_slice(prefix, 2, prefix.length);
                if(sv_endswith(prefix, SV("__")))
                    prefix = sv_slice(prefix, 0, prefix.length-2);
                is_gnu = sv_equals(prefix, SV("gnu"));
                is_drc = sv_equals(prefix, SV("drc"));
                err = cc_next_token(p, &tok);
                if(err) return err;
                if(tok.type != CC_IDENTIFIER && tok.type != CC_KEYWORD)
                    return cc_error(p, tok.loc, "expected attribute name after '::'");
                if(tok.type == CC_IDENTIFIER)
                    attr_name = (StringView){.text = tok.ident.ident->data, .length = tok.ident.ident->length};
                else {
                    goto c23_skip_args;
                }
                if(!is_gnu && !is_drc)
                    goto c23_skip_args;
            }
            if(sv_startswith(attr_name, SV("__")))
                attr_name = sv_slice(attr_name, 2, attr_name.length);
            if(sv_endswith(attr_name, SV("__")))
                attr_name = sv_slice(attr_name, 0, attr_name.length-2);
            if(is_drc){
                if(sv_equals(attr_name, SV("hotswappable"))){
                    attrs->hotswappable = 1;
                    err = cc_peek(p, &tok);
                    if(err) return err;
                    if(tok.type == CC_PUNCTUATOR && tok.punct.punct == CC_lparen)
                        return cc_error(p, tok.loc, "drc::hotswappable takes no arguments");
                    goto c23_skip_comma;
                }
                goto c23_skip_args;
            }
            if(sv_equals(attr_name, SV("noreturn")))
                attrs->is_noreturn = 1;
            else if(is_gnu){
                err = cc_match_gnu_attribute(p, flags, tok.loc, attr_name, attrs);
                if(err) return err;
                goto c23_skip_comma;
            }
            c23_skip_args:
            err = cc_peek(p, &tok);
            if(err) return err;
            if(tok.type == CC_PUNCTUATOR && tok.punct.punct == CC_lparen){
                err = cc_next_token(p, &tok);
                if(err) return err;
                int depth = 1;
                while(depth > 0){
                    err = cc_next_token(p, &tok);
                    if(err) return err;
                    if(tok.type == CC_EOF)
                        return cc_error(p, attr_loc, "unterminated attribute argument list");
                    if(tok.type == CC_PUNCTUATOR){
                        if(tok.punct.punct == CC_lparen) depth++;
                        else if(tok.punct.punct == CC_rparen) depth--;
                    }
                }
            }
            c23_skip_comma:
            err = cc_peek(p, &tok);
            if(err) return err;
            if(tok.type == CC_PUNCTUATOR && tok.punct.punct == ','){
                err = cc_next_token(p, &tok);
                if(err) return err;
                continue;
            }
            break;
        }
        close_brackets:
        err = cc_expect_punct(p, CC_rbracket);
        if(err) return err;
        err = cc_expect_punct(p, CC_rbracket);
        if(err) return err;
    }
}

static
int
cc_parse_declspec(CcParser* p, CcParseFlags flags, CcAttributes* attrs){
    int err = 0;
    CcToken tok;
    for(;;){
        err = cc_peek(p, &tok);
        if(err) return err;
        if(tok.type != CC_KEYWORD || tok.kw.kw != CC___declspec)
            return 0;
        err = cc_next_token(p, &tok);
        if(err) return err;
        SrcLoc declspec_loc = tok.loc;
        err = cc_expect_punct(p, CC_lparen);
        if(err) return err;
        for(;;){
            err = cc_peek(p, &tok);
            if(err) return err;
            if(tok.type == CC_PUNCTUATOR && tok.punct.punct == CC_rparen)
                break;
            err = cc_next_token(p, &tok);
            if(err) return err;
            if(tok.type == CC_KEYWORD){
                if(tok.kw.kw == CC__Noreturn)
                    attrs->is_noreturn = 1;
                else {
                    err = cc_peek(p, &tok);
                    if(err) return err;
                    if(tok.type == CC_PUNCTUATOR && tok.punct.punct == CC_lparen){
                        err = cc_next_token(p, &tok);
                        if(err) return err;
                        int depth = 1;
                        while(depth > 0){
                            err = cc_next_token(p, &tok);
                            if(err) return err;
                            if(tok.type == CC_EOF)
                                return cc_error(p, declspec_loc, "unterminated __declspec argument list");
                            if(tok.type == CC_PUNCTUATOR){
                                if(tok.punct.punct == CC_lparen) depth++;
                                else if(tok.punct.punct == CC_rparen) depth--;
                            }
                        }
                    }
                }
                continue;
            }
            if(tok.type != CC_IDENTIFIER)
                return cc_error(p, tok.loc, "expected __declspec specifier name");
            StringView name = {.text = tok.ident.ident->data, .length = tok.ident.ident->length};
            if(sv_equals(name, SV("align"))){
                err = cc_parse_alignment_argument(p, flags, tok.loc, attrs, "__declspec(align)");
                if(err) return err;
            }
            else if(sv_equals(name, SV("thread"))){
                attrs->is_thread_local = 1;
            }
            else {
                err = cc_peek(p, &tok);
                if(err) return err;
                if(tok.type == CC_PUNCTUATOR && tok.punct.punct == CC_lparen){
                    err = cc_next_token(p, &tok);
                    if(err) return err;
                    int depth = 1;
                    while(depth > 0){
                        err = cc_next_token(p, &tok);
                        if(err) return err;
                        if(tok.type == CC_EOF)
                            return cc_error(p, declspec_loc, "unterminated __declspec argument list");
                        if(tok.type == CC_PUNCTUATOR){
                            if(tok.punct.punct == CC_lparen) depth++;
                            else if(tok.punct.punct == CC_rparen) depth--;
                        }
                    }
                }
            }
        }
        err = cc_expect_punct(p, CC_rparen);
        if(err) return err;
    }
}

static inline int
cc_align_to(CcParser* p, SrcLoc loc, uint32_t offset, uint32_t alignment, uint32_t* out){
    uint32_t padding = (0u - offset) & (alignment - 1);
    if(add_overflow(offset, padding, out))
        return cc_error(p, loc, "object size exceeds 32-bit layout limit");
    return 0;
}

static
CcSysVEightByte
cc_sysv_merge(CcSysVEightByte a, CcSysVEightByte b){
    if(a == b || b == CC_SYSV_NO_CLASS) return a;
    if(a == CC_SYSV_NO_CLASS) return b;
    if(a == CC_SYSV_MEMORY || b == CC_SYSV_MEMORY) return CC_SYSV_MEMORY;
    if(a == CC_SYSV_INTEGER || b == CC_SYSV_INTEGER) return CC_SYSV_INTEGER;
    if(a == CC_SYSV_X87 || a == CC_SYSV_X87UP || a == CC_SYSV_COMPLEX_X87)
        return CC_SYSV_MEMORY;
    if(b == CC_SYSV_X87 || b == CC_SYSV_X87UP || b == CC_SYSV_COMPLEX_X87)
        return CC_SYSV_MEMORY;
    return CC_SYSV_SSE;
}

static
_Bool
cc_sysv_mark(CcSysVEightByte cls[_Nonnull 8], uint32_t off, uint32_t size, CcSysVEightByte c){
    if(off > 64 || size > 64-off) return 1;
    if(!size) return 0;
    for(uint32_t i = off/8; i <= (off+size-1)/8; i++)
        cls[i] = cc_sysv_merge(cls[i], c);
    return 0;
}

static _Bool cc_sysv_classify_type(const CcTargetConfig*, CcQualType, uint32_t, CcSysVEightByte[_Nonnull 8]);

static
_Bool
cc_sysv_classify_fields(const CcTargetConfig* tc, const CcField* _Null_unspecified fields, uint32_t count, uint32_t base, CcSysVEightByte cls[_Nonnull 8]){
    for(uint32_t i = 0; i < count; i++){
        const CcField* f = &fields[i];
        if(f->is_bitfield){
            // Classify occupied bits, not the declared storage unit.
            // Packed fields can cross eightbytes.
            // Zero-width fields contribute nothing.
            if(!f->bitwidth) continue;
            if(tc->os == CC_OS_MACOS && !f->name) continue;
            uint64_t first = ((uint64_t)base + f->offset)*8 + f->bitoffset;
            uint64_t end = first + f->bitwidth;
            if(end > 64*8) return 1;
            if(cc_sysv_mark(cls, (uint32_t)(first/8), (uint32_t)((end+7)/8-first/8), CC_SYSV_INTEGER))
                return 1;
            continue;
        }
        if(cc_sysv_classify_type(tc, f->type, base + f->offset, cls))
            return 1;
    }
    return 0;
}

static
_Bool
cc_sysv_classify_type(const CcTargetConfig* tc, CcQualType type, uint32_t off, CcSysVEightByte cls[_Nonnull 8]){
    switch(ccqt_kind(type)){
        case CC_VLA: return 1;
        case CC_ALIGNED: cc_unsafe_unreachable();
        case CC_BASIC:{
            CcBasicTypeKind bk = ccqt_basic_kind(type);
            uint32_t sz = tc->sizeof_[bk];
            if(off % tc->alignof_[bk]) return 1;
            if(bk == CCBT_long_double && tc->long_double_format == CC_LONG_DOUBLE_X87)
                return cc_sysv_mark(cls, off, 8, CC_SYSV_X87) || cc_sysv_mark(cls, off+8, 8, CC_SYSV_X87UP);
            // COMPLEX_X87 is a scalar class. A record containing one is larger
            // than two eightbytes and fails aggregate post-merge cleanup.
            if(bk == CCBT_long_double_complex && tc->long_double_format == CC_LONG_DOUBLE_X87)
                return 1;
            if(bk == CCBT_float128 || (bk == CCBT_long_double && tc->long_double_format == CC_LONG_DOUBLE_BINARY128))
                return cc_sysv_mark(cls, off, 8, CC_SYSV_SSE) || cc_sysv_mark(cls, off+8, 8, CC_SYSV_SSEUP);
            if(bk == CCBT_long_double_complex && tc->long_double_format == CC_LONG_DOUBLE_BINARY128){
                for(uint32_t i = 0; i < 2; i++)
                    if(cc_sysv_mark(cls, off+16*i, 8, CC_SYSV_SSE) || cc_sysv_mark(cls, off+16*i+8, 8, CC_SYSV_SSEUP))
                        return 1;
                return 0;
            }
            CcSysVEightByte c = ccbt_is_float(bk)
                || bk == CCBT_float16_complex
                || bk == CCBT_float_complex
                || bk == CCBT_double_complex
                || bk == CCBT_long_double_complex ?
                CC_SYSV_SSE : CC_SYSV_INTEGER;
            return cc_sysv_mark(cls, off, sz, c);
        }
        case CC_BLOCK_POINTER:
        case CC_POINTER:
        case CC_FUNCTION:
        case CC_SLICE:
            if(off % tc->alignof_[CCBT_nullptr_t]) return 1;
            return cc_sysv_mark(cls, off, tc->sizeof_[CCBT_nullptr_t] * (ccqt_kind(type) == CC_SLICE ? 2 : 1), CC_SYSV_INTEGER);
        case CC_ENUM:
            return cc_sysv_classify_type(tc, ccqt_as_enum(type)->underlying, off, cls);
        case CC_STRUCT:
            return cc_sysv_classify_fields(tc, ccqt_as_struct(type)->fields, ccqt_as_struct(type)->field_count, off, cls);
        case CC_UNION:
            return cc_sysv_classify_fields(tc, ccqt_as_union(type)->fields, ccqt_as_union(type)->field_count, off, cls);
        case CC_VECTOR: {
            CcVector* arr = ccqt_as_vector(type);
            uint32_t sz = arr->vector_size;
            if((sz != 1 && sz != 2 && sz != 4 && sz != 8 && sz != 16 && sz != 32 && sz != 64) || off % sz) return 1;
            CcBasicTypeKind elem = ccqt_basic_kind(arr->element);
            if(sz < 8){
                // Clang coerces small vectors to integers. GCC uses SSE
                // for two half-precision elements, but passes other small
                // floating vectors in memory.
                if(tc->os != CC_OS_MACOS && ccbt_is_float(elem)){
                    if(sz == 4 && (elem == CCBT_float16 || elem == CCBT_bfloat16 || elem == CCBT_fp16))
                        return cc_sysv_mark(cls, off, sz, CC_SYSV_SSE);
                    return 1;
                }
                return cc_sysv_mark(cls, off, sz, CC_SYSV_INTEGER);
            }
            if(sz == 8){
                if(elem == CCBT_double) return 1; // single-double vector
                if(tc->os == CC_OS_MACOS && tc->sizeof_[elem] == 8 && ccbt_is_integer(elem))
                    return cc_sysv_mark(cls, off, sz, CC_SYSV_INTEGER);
                return cc_sysv_mark(cls, off, sz, CC_SYSV_SSE);
            }
            // GCC passes wide vectors of 128-bit integers in memory even with AVX enabled.
            if(sz > 16 && tc->os != CC_OS_MACOS && ccbt_is_integer(elem) && tc->sizeof_[elem] == 16)
                return 1;
            if(cc_sysv_mark(cls, off, 8, CC_SYSV_SSE)) return 1;
            for(uint32_t i = 8; i < sz; i += 8)
                if(cc_sysv_mark(cls, off+i, 8, CC_SYSV_SSEUP)) return 1;
            return 0;
        }
        case CC_ARRAY: {
            CcArray* arr = ccqt_as_array(type);
            // Clang and GCC disagree on what FAMs do,
            // so follow GCC on Linux (ignore FAMs) and Clang on AppleOS (MEMORY).
            if(arr->is_incomplete) return tc->os == CC_OS_MACOS;
            uint32_t sz, elem_sz;
            if(cc_type_sizeof_complete(tc, type, &sz, NULL) || off > 64 || sz > 64-off || cc_type_sizeof_complete(tc, arr->element, &elem_sz, NULL))
                return 1;
            if(!elem_sz) return 0;
            for(size_t i = 0; i < arr->length; i++)
                if(cc_sysv_classify_type(tc, arr->element, off+(uint32_t)i*elem_sz, cls))
                    return 1;
            return 0;
        }
    }
    return 1;
}

static
void
cc_sysv_classify_record(const CcTargetConfig* tc, const CcField* _Null_unspecified fields, uint32_t count, uint32_t size, CcSysvAggregate* out){
    *out = (CcSysvAggregate){0};
    CcSysVEightByte classes[8] = {0};
    if(size > 64) goto memory;
    out->eightbyte_count = (uint8_t)((size+7)/8);
    if(cc_sysv_classify_fields(tc, fields, count, 0, classes))
        goto memory;
    for(uint32_t i = 0; i < out->eightbyte_count; i++){
        CcSysVEightByte c = classes[i];
        if(c == CC_SYSV_MEMORY)
            goto memory;
        if(c == CC_SYSV_X87UP && (!i || classes[i-1] != CC_SYSV_X87))
            goto memory;
    }
    if(size > 16){
        if(classes[0] != CC_SYSV_SSE)
            goto memory;
        for(uint32_t i = 1; i < out->eightbyte_count; i++)
            if(classes[i] != CC_SYSV_SSEUP)
                goto memory;
    }
    for(uint32_t i = 0; i < out->eightbyte_count; i++){
        CcSysVEightByte c = classes[i];
        if(c == CC_SYSV_SSEUP && (!i || (classes[i-1] != CC_SYSV_SSE && classes[i-1] != CC_SYSV_SSEUP)))
            c = classes[i] = CC_SYSV_SSE;
        if(c == CC_SYSV_INTEGER)
            out->gp_count++;
        else if(c == CC_SYSV_SSE){
            out->fp_count++;
            uint32_t width = 8;
            for(uint32_t j = i+1; j < out->eightbyte_count && classes[j] == CC_SYSV_SSEUP; j++)
                width += 8;
            if(width > out->vector_size)
                out->vector_size = (uint8_t)width;
        }
        else if(c == CC_SYSV_X87)
            out->x87_count++;
    }
    for(uint32_t i = 0; i < out->eightbyte_count; i++)
        cc_sysv_set_class(out, i, classes[i]);
    out->argument_on_stack = out->x87_count != 0;
    return;
    memory:
    *out = (CcSysvAggregate){.is_memory_class=1, .argument_on_stack=1};
    cc_sysv_set_class(out, 0, CC_SYSV_MEMORY);
}

static _Bool cc_arm64_homogeneous(const CcTargetConfig*, CcQualType, CcArm64Aggregate*);

static
_Bool
cc_arm64_homogeneous_append(CcArm64Aggregate* dst, const CcArm64Aggregate* src, uint32_t off){
    uint32_t dn = dst->hfa_count + dst->hva_count;
    uint32_t sn = src->hfa_count + src->hva_count;
    if(!sn) return 1;
    if(dn && (dst->hfa_type != src->hfa_type || dst->member_size != src->member_size || !!dst->hva_count != !!src->hva_count))
        return 0;
    if(dn+sn > 4)
        return 0;
    dst->hfa_type = src->hfa_type;
    dst->member_size = src->member_size;
    for(uint32_t i = 0; i < sn; i++){
        uint32_t offset = off + cc_arm64_member_offset(src, i);
        if(offset > 48)
            return 0;
        cc_arm64_set_member_offset(dst, dn+i, offset);
    }
    if(src->hva_count) dst->hva_count = (uint8_t)(dn+sn);
    else dst->hfa_count = (uint8_t)(dn+sn);
    return 1;
}

static
_Bool
cc_arm64_homogeneous(const CcTargetConfig* tc, CcQualType type, CcArm64Aggregate* out){
    *out = (CcArm64Aggregate){0};
    switch(ccqt_kind(type)){
        case CC_VLA: return 0;
        case CC_ALIGNED: cc_unsafe_unreachable();
        case CC_BASIC:{
            CcBasicTypeKind bk = ccqt_basic_kind(type);
            uint32_t n = 1;
            if(bk == CCBT_float16_complex){
                bk = CCBT_float16;
                n = 2;
            }
            else if(bk == CCBT_float_complex){
                bk = CCBT_float;
                n = 2;
            }
            else if(bk == CCBT_double_complex){
                bk = CCBT_double;
                n = 2;
            }
            else if(bk == CCBT_long_double_complex){
                bk = CCBT_long_double;
                n = 2;
            }
            else if(!ccbt_is_float(bk)) return 0;
            // These are the same ABI fundamental type despite distinct C types.
            if(bk == CCBT_bfloat16 || bk == CCBT_fp16)
                bk = CCBT_float16;
            if(bk == CCBT_long_double && tc->long_double_format == CC_LONG_DOUBLE_BINARY64)
                bk = CCBT_double;
            if(bk == CCBT_float128 && tc->long_double_format == CC_LONG_DOUBLE_BINARY128)
                bk = CCBT_long_double;
            out->hfa_type = (uint32_t)bk;
            out->hfa_count = (uint8_t)n;
            out->member_size = tc->sizeof_[bk];
            if(n == 2) cc_arm64_set_member_offset(out, 1, out->member_size);
            return 1;
        }
        case CC_VECTOR: {
            CcVector* arr = ccqt_as_vector(type);
            if(arr->vector_size != 8 && arr->vector_size != 16) return 0;
            out->hva_count = 1;
            out->hfa_type = CCBT_INVALID;
            out->member_size = (uint8_t)arr->vector_size;
            return 1;
        }
        case CC_ARRAY: {
            CcArray* arr = ccqt_as_array(type);
            if(arr->is_incomplete)
                return 0;
            // Zero-length array members disqualify Clang HFAs.
            // GCC also rejects arrays whose element type is not homogeneous.
            if(!arr->length && tc->os == CC_OS_MACOS)
                return 0;
            CcArm64Aggregate elem;
            uint32_t stride;
            if(!cc_arm64_homogeneous(tc, arr->element, &elem) || cc_type_sizeof_complete(tc, arr->element, &stride, NULL))
                return 0;
            if(!arr->length) return 1;
            if(!stride)
                return 1;
            if(arr->length > 4)
                return 0;
            for(size_t i = 0; i < arr->length; i++)
                if(!cc_arm64_homogeneous_append(out, &elem, (uint32_t)i*stride))
                    return 0;
            break;
        }
        case CC_STRUCT:
        case CC_UNION:{
            CcStruct* s = ccqt_as_struct(type);
            if(s->is_incomplete)
                return 0;
            for(uint32_t i = 0; i < s->field_count; i++){
                CcField* f = &s->fields[i];
                if(f->is_bitfield && !f->bitwidth)
                    continue;
                if(f->is_bitfield)
                    return 0;
                CcArm64Aggregate member;
                if(!cc_arm64_homogeneous(tc, f->type, &member))
                    return 0;
                if(ccqt_kind(type) == CC_STRUCT){
                    if(!cc_arm64_homogeneous_append(out, &member, f->offset))
                        return 0;
                }
                else {
                    uint32_t n = member.hfa_count + member.hva_count;
                    uint32_t prev = out->hfa_count + out->hva_count;
                    if(!n) continue;
                    if(prev && (out->hfa_type != member.hfa_type || out->member_size != member.member_size || !!out->hva_count != !!member.hva_count))
                        return 0;
                    if(n > prev) *out = member;
                }
            }
            break;
        }
        case CC_BLOCK_POINTER:
        case CC_POINTER:
        case CC_FUNCTION:
        case CC_ENUM:
        case CC_SLICE:
            return 0;
    }
    uint32_t size;
    if(cc_type_sizeof_complete(tc, type, &size, NULL))
        return 0;
    uint32_t n = out->hfa_count + out->hva_count;
    if(size != n*out->member_size) return 0;
    for(uint32_t i = 0; i < n; i++)
        if(cc_arm64_member_offset(out, i) != i*out->member_size)
            return 0;
    return 1;
}

static
_Bool
cc_arm64_empty_type(CcQualType type){
    switch(ccqt_kind(type)){
        case CC_STRUCT: case CC_UNION:
            return ccqt_as_struct(type)->arm64.is_empty;
        case CC_ARRAY:{
            const CcArray* arr = ccqt_as_array(type);
            return !arr->length || cc_arm64_empty_type(arr->element);
        }
        default: return 0;
    }
}

static
_Bool
cc_arm64_empty_record(const CcStruct* s){
    // Unnamed bitfields occupy storage but do not carry a member value.
    for(uint32_t i = 0; i < s->field_count; i++){
        const CcField* f = &s->fields[i];
        if(f->is_bitfield && !f->name) continue;
        if(!cc_arm64_empty_type(f->type)) return 0;
    }
    return 1;
}

static
_Bool
cc_type_has_wide_vector(CcQualType type){
    switch(ccqt_kind(type)){
        case CC_VECTOR: return ccqt_as_vector(type)->vector_size > 16;
        case CC_ARRAY:{
            CcArray* arr = ccqt_as_array(type);
            return cc_type_has_wide_vector(arr->element);
        }
        case CC_STRUCT: case CC_UNION:
            return ccqt_as_struct(type)->has_wide_vector;
        default: return 0; // pointees and function signatures are not subobjects
    }
}

static
void
cc_classify_record(CcParser* p, CcStruct* s, uint32_t natural_alignment){
    const CcTargetConfig* tc = cc_target(p);
    s->has_wide_vector = 0;
    for(uint32_t i = 0; i < s->field_count; i++){
        if(cc_type_has_wide_vector(s->fields[i].type)){
            s->has_wide_vector = 1;
            break;
        }
    }
    switch(tc->target){
        case CC_TARGET_X86_64_LINUX:
        case CC_TARGET_X86_64_MACOS:
            cc_sysv_classify_record(tc, s->fields, s->field_count, s->size, &s->sysv);
            break;
        case CC_TARGET_AARCH64_LINUX:
        case CC_TARGET_AARCH64_MACOS:{
            s->arm64 = (CcArm64Aggregate){0};
            CcQualType type = {.bits=(uintptr_t)s};
            CcArm64Aggregate ha;
            if(cc_arm64_homogeneous(tc, type, &ha)) s->arm64 = ha;
            s->arm64.is_empty = cc_arm64_empty_record(s);
            s->arm64.natural_alignment = natural_alignment;
            s->arm64.copy_alignment = natural_alignment <= 8 ? 8 : 16;
            // Darwin omits empty arguments even when they have nonzero size.
            if(!s->arm64.hfa_count && !s->arm64.hva_count && !(tc->target == CC_TARGET_AARCH64_MACOS && s->arm64.is_empty)){
                s->arm64.is_indirect = s->size > 16;
                s->arm64.gp_count = s->arm64.is_indirect ? 0 : (uint8_t)((s->size+7)/8);
            }
            break;
        }
        case CC_TARGET_X86_64_WINDOWS_MSVC:
        case CC_TARGET_X86_64_WINDOWS_MINGW:
            s->win64 = (CcWindowsAggregate){0};
            s->win64.is_indirect = s->size != 0 && s->size != 1 && s->size != 2 && s->size != 4 && s->size != 8;
            s->win64.gp_count = s->size && !s->win64.is_indirect ? 1 : 0;
            break;
        case CC_TARGET_TEST:
        case CC_TARGET_COUNT:
            break;
    }
}

static
int
cc_field_layout_alignment(CcParser* p, CcField* field, _Bool packed, uint16_t pack_value, uint32_t* out){
    int err = cc_alignof_as_uint(p, field->type, field->loc, out);
    if(err) return err;
    if(packed || field->packed) *out = 1;
    else if(pack_value && *out > pack_value) *out = pack_value;
    if(field->alignment > *out) *out = field->alignment;
    field->layout_alignment = *out;
    return 0;
}

static
int
cc_compute_struct_layout(CcParser* p, CcStruct* s, uint16_t pack_value){
    DRP_PROFILE_BEGIN(profile, "parser.struct_layout");
    int err = 0;
    uint32_t offset = 0;
    uint32_t max_align = 1;
    uint32_t bitfield_offset = 0;        // bit offset within current storage unit
    uint32_t bitfield_storage_end = 0;   // byte offset of end of current storage unit
    uint32_t bitfield_storage_start = 0; // byte offset of start of current storage unit
    uint32_t bitfield_storage_size = 0; // allocation unit size for the MS ABI
    CcBitfieldABI bf_abi = cc_target(p)->bitfield_abi;
    for(uint32_t i = 0; i < s->field_count; i++){
        CcField* f = &s->fields[i];
        if(ccqt_is_vm(f->type))
            return cc_type_error(p, f->loc, f->type, "member cannot have variably modified type");
        if(ccqt_kind(f->type) == CC_ARRAY){
            CcArray* arr = ccqt_as_array(f->type);
            if(arr->is_incomplete){
                if(i + 1 < s->field_count)
                    return cc_error(p, f->loc, "flexible array member must be last field");
                if(bitfield_offset > 0){
                    offset = bitfield_storage_end;
                    bitfield_offset = 0;
                }
                uint32_t field_align;
                err = cc_field_layout_alignment(p, f, s->packed, pack_value, &field_align);
                if(err) return err;
                err = cc_align_to(p, f->loc, offset, field_align, &offset);
                if(err) return err;
                f->offset = offset;
                if(field_align > max_align) max_align = field_align;
                s->has_fam = 1;
                continue;
            }
        }
        if(ccqt_kind(f->type) == CC_STRUCT){
            CcStruct* inner = ccqt_as_struct(f->type);
            if(inner->has_fam){
                if(!f->name && i + 1 == s->field_count)
                    s->has_fam = 1;
                else
                    return cc_type_error(p, f->loc, f->type, "struct with flexible array member cannot be embedded");
            }
        }
        uint32_t field_size, field_align;
        err = cc_sizeof_as_uint(p, f->type, f->loc, &field_size);
        if(err) return err;
        err = cc_field_layout_alignment(p, f, s->packed, pack_value, &field_align);
        if(err) return err;
        enum {char_bit=8};
        if(f->is_bitfield){
            uint32_t bw = f->bitwidth;
            uint32_t storage_bits = field_size * char_bit;
            if(bw == 0){
                if(bitfield_offset > 0){
                    offset = bitfield_storage_end;
                }
                if(bf_abi == CC_BITFIELD_SYSV){
                    uint32_t zero_align;
                    err = cc_alignof_as_uint(p, f->type, f->loc, &zero_align);
                    if(err) return err;
                    err = cc_align_to(p, f->loc, offset, zero_align, &offset);
                    if(err) return err;
                    if(cc_target(p)->arch == CC_ARCH_ARM64 && zero_align > max_align)
                        max_align = zero_align;
                }
                else if(bitfield_offset > 0){
                    err = cc_align_to(p, f->loc, offset, field_align, &offset);
                    if(err) return err;
                    if(field_align > max_align) max_align = field_align;
                }
                bitfield_offset = 0;
                bitfield_storage_end = 0;
                bitfield_storage_size = 0;
                f->offset = offset;
                f->bitoffset = 0;
                continue;
            }
            if(bf_abi == CC_BITFIELD_SYSV){
                uint64_t pos = bitfield_offset ?  (uint64_t)bitfield_storage_start*8 + bitfield_offset : (uint64_t)offset*8;
                _Bool packed = s->packed || f->packed || pack_value > 0;
                uint64_t align_bits = (uint64_t)field_align*8;
                if(f->alignment) pos = (pos+align_bits-1) & ~(align_bits-1);
                uint64_t start = packed ? pos/8*8 : pos/align_bits*align_bits;
                if(!packed && pos-start+bw > storage_bits){
                    pos = (pos+align_bits-1) & ~(align_bits-1);
                    start = pos;
                }
                uint64_t end = (pos+bw+7)/8;
                if(end > UINT32_MAX)
                    return cc_error(p, f->loc, "object size exceeds 32-bit layout limit");
                f->offset = (uint32_t)(start/8);
                f->bitoffset = (uint32_t)(pos-start);
                bitfield_storage_start = (uint32_t)(pos/8);
                bitfield_offset = (uint32_t)(pos%8+bw);
                bitfield_storage_end = (uint32_t)end;
                // On AAPCS64 even unnamed fields contribute member alignment.
                if((f->name || cc_target(p)->arch == CC_ARCH_ARM64) && field_align > max_align)
                    max_align = field_align;
                continue;
            }
            _Bool fits;
            uint32_t f_offset = 0, f_bitoffset = 0;
            fits = bitfield_storage_size == field_size && bitfield_offset + bw <= storage_bits;
            f_offset = bitfield_storage_start;
            f_bitoffset = bitfield_offset;
            if(fits){
                f->offset = f_offset;
                f->bitoffset = f_bitoffset;
                bitfield_offset += bw;
                uint32_t end;
                if(add_overflow(f_offset, field_size, &end))
                    return cc_error(p, f->loc, "object size exceeds 32-bit layout limit");
                if(end > bitfield_storage_end) bitfield_storage_end = end;
            }
            else {
                if(bitfield_offset > 0)
                    offset = bitfield_storage_end;
                err = cc_align_to(p, f->loc, offset, field_align, &offset);
                if(err) return err;
                f->offset = offset;
                f->bitoffset = 0;
                bitfield_offset = bw;
                bitfield_storage_start = offset;
                if(add_overflow(offset, field_size, &bitfield_storage_end))
                    return cc_error(p, f->loc, "object size exceeds 32-bit layout limit");

                bitfield_storage_size = field_size;
            }
            if(field_align > max_align)
                max_align = field_align;
            continue;
        }
        if(bitfield_offset > 0){
            offset = bitfield_storage_end;
            bitfield_offset = 0;
            bitfield_storage_end = 0;

            bitfield_storage_size = 0;
        }
        err = cc_align_to(p, f->loc, offset, field_align, &offset);
        if(err) return err;
        f->offset = offset;
        if(add_overflow(offset, field_size, &offset))
            return cc_error(p, f->loc, "object size exceeds 32-bit layout limit");
        if(field_align > max_align)
            max_align = field_align;
    }
    if(bitfield_offset > 0)
        offset = bitfield_storage_end;
    uint32_t natural_alignment = max_align;
    if(s->alignment > max_align)
        max_align = s->alignment;
    s->alignment = max_align;
    err = cc_align_to(p, s->loc, offset, max_align, &s->size);
    if(err) return err;
    s->is_incomplete = 0;
    cc_classify_record(p, s, natural_alignment);
    return 0;
    DRP_PROFILE_END(profile);
}

static
int
cc_compute_union_layout(CcParser* p, CcUnion* u, uint16_t pack_value){
    uint32_t max_size = 0;
    uint32_t max_align = 1;
    int err = 0;
    for(uint32_t i = 0; i < u->field_count; i++){
        CcField* f = &u->fields[i];
        if(ccqt_is_vm(f->type))
            return cc_type_error(p, f->loc, f->type, "member cannot have variably modified type");
        if(ccqt_array_incomplete(f->type)){
            uint32_t field_align;
            err = cc_field_layout_alignment(p, f, u->packed, pack_value, &field_align);
            if(err) return err;
            f->offset = 0;
            if(field_align > max_align) max_align = field_align;
            continue;
        }
        uint32_t field_size;
        err = cc_sizeof_as_uint(p, f->type, f->loc, &field_size);
        if(err) return err;
        uint32_t field_align;
        err = cc_field_layout_alignment(p, f, u->packed, pack_value, &field_align);
        if(err) return err;
        f->offset = 0;
        if(f->is_bitfield){
            f->bitoffset = 0;
            if((u->packed || f->packed || pack_value) && cc_target(p)->bitfield_abi == CC_BITFIELD_SYSV)
                field_size = (f->bitwidth + 7)/8;
        }
        if(field_size > max_size) max_size = field_size;
        if(field_align > max_align) max_align = field_align;
    }
    uint32_t natural_alignment = max_align;
    if(u->alignment > max_align)
        max_align = u->alignment;
    u->alignment = max_align;
    err = cc_align_to(p, u->loc, max_size, max_align, &u->size);
    if(err) return err;
    u->is_incomplete = 0;
    cc_classify_record(p, (CcStruct*)u, natural_alignment);
    return 0;
}

static
int
cc_pragma_pack(void* _Null_unspecified ctx, CppPreprocessor* cpp, SrcLoc loc, const CppToken*_Null_unspecified toks, size_t ntoks){
    CcParser* p = (CcParser*)ctx;
    if(!ntoks || toks[0].type != CPP_PUNCTUATOR || toks[0].punct != '(')
        return ((void)cc_warn(p, loc, "#pragma pack expects '('"), 0);
    if(ntoks < 2)
        return ((void)cc_warn(p, loc, "#pragma pack expects at least ()"), 0);
    const CppToken* end = toks+ntoks;
    for(const CppToken* t = end; --t != toks;){
        if(t->type == CPP_PUNCTUATOR && t->punct == ')'){
            end = t+1;
            break;
        }
    }
    int err = 0;
    CppTokens* expanded = cpp_get_scratch(cpp);
    if(end - toks > 2){
        err = cpp_expand_argument(cpp, toks+1, end-toks-2, expanded);
        if(err) goto finally;
        toks = expanded->data;
        end = toks + expanded->count;
    }
    else {
        toks = toks+1;
        end = end-1;
    }
    while(toks < end && toks->type == CPP_WHITESPACE) toks++;
    while(toks < end && end[-1].type == CPP_WHITESPACE) end--;
    if(toks == end){ // pack()
        p->pragma_pack = 0; // default alignment
        goto finally;
    }
    const CppToken* number = NULL;
    if(toks->type == CPP_NUMBER){
        number = toks++;
        while(toks < end && toks->type == CPP_WHITESPACE) toks++;
    }
    else if(toks->type == CPP_IDENTIFIER){
        StringView word = toks->txt;
        toks++;
        while(toks < end && toks->type == CPP_WHITESPACE) toks++;
        if(sv_equals(word, SV("show"))){
            if(p->pragma_pack)
                cc_info(p, loc, "#pragma pack(show): %d", (int)p->pragma_pack);
            else
                cc_info(p, loc, "#pragma pack(show): default");
            if(toks != end) cc_warn(p, toks->loc, "Extra tokens after show");
            goto finally;
        }
        else if(sv_equals(word, SV("push"))){
            // #pragma pack( push [ , identifier ] [ , n ] )
            const CppToken* ident = NULL;
            if(toks != end && toks->type == CPP_PUNCTUATOR && toks->punct == ','){
                toks++;
                while(toks < end && toks->type == CPP_WHITESPACE) toks++;
                if(toks->type == CPP_IDENTIFIER){
                    ident = toks++;
                    while(toks < end && toks->type == CPP_WHITESPACE) toks++;
                    if(toks != end && toks->type == CPP_PUNCTUATOR && toks->punct == ','){
                        toks++;
                        while(toks < end && toks->type == CPP_WHITESPACE) toks++;
                    }
                }
                // technically this allows [,identifer] [n] instead of [, identifer] [, n] but whatever
                if(toks->type == CPP_NUMBER){
                    number = toks++;
                    while(toks < end && toks->type == CPP_WHITESPACE) toks++;
                }
            }
            CcPackRecord r = {
                .ident = ident?ident->txt:(StringView){0},
                .pack = p->pragma_pack,
            };
            err = ma_push(CcPackRecord)(&p->pack_stack, cc_allocator(p), r);
            if(err) goto finally;
        }
        else if(sv_equals(word, SV("pop"))){
            // #pragma pack( pop [ , { identifier | n } ] )
            const CppToken* ident = NULL;
            if(toks != end && toks->type == CPP_PUNCTUATOR && toks->punct == ','){
                toks++;
                while(toks < end && toks->type == CPP_WHITESPACE) toks++;
                if(toks->type == CPP_IDENTIFIER){
                    ident = toks++;
                    while(toks < end && toks->type == CPP_WHITESPACE) toks++;
                }
                else if(toks->type == CPP_NUMBER){
                    number = toks++;
                    while(toks < end && toks->type == CPP_WHITESPACE) toks++;
                }
            }
            if(!p->pack_stack.count){
                cc_warn(p, loc, "pack stack empty");
            }
            else {
                if(ident){
                    for(size_t i = p->pack_stack.count; i--; ){
                        if(sv_equals(p->pack_stack.data[i].ident, ident->txt)){
                            p->pragma_pack = p->pack_stack.data[i].pack;
                            p->pack_stack.count = i;
                            break;
                        }
                        if(i == 0){
                            cc_warn(p, ident->loc, "'%.*s' not found in pack stack", sv_p(ident->txt));
                        }
                    }
                }
                else {
                    p->pragma_pack = ma_tail(p->pack_stack).pack;
                    p->pack_stack.count--;
                }
            }
        }
        else {
            cc_warn(p, loc, "Unrecognized pragma pack() command");
            goto finally;
        }
    }
    if(number){
        int64_t pack = 8;
        err = cpp_eval_parse_number(cpp, *number, &pack);
        if(err) goto finally;
        if(pack < 0 || pack > UINT16_MAX){
            cc_warn(p, number->loc, "pack value too big, treating as 8");
            pack = 8;
        }
        if(pack != 1 && pack != 2 && pack != 4 && pack != 8 && pack != 16){
            cc_warn(p, number->loc, "value %lld invalid, treating as 8", (long long)pack);
            pack = 8;
        }
        p->pragma_pack = (uint16_t)pack;
    }
    if(toks != end){
        cc_warn(p, toks->loc, "Extra tokens in pack()");
    }
    finally:
    if(expanded)
        cpp_release_scratch(cpp, expanded);
    return err;
}

static
int
cc_pragma_typedef(void* _Null_unspecified ctx, CppPreprocessor* cpp, SrcLoc loc, const CppToken*_Null_unspecified toks, size_t ntoks){
    CcParser* p = (CcParser*)ctx;
    (void)cpp;
    const CppToken* end = toks+ntoks;
    while(toks < end && toks->type == CPP_WHITESPACE) toks++;
    while(toks < end && end[-1].type == CPP_WHITESPACE) end--;
    if(toks == end || toks->type != CPP_IDENTIFIER){
        return cc_error(p, loc, "#pragma typedef requires 'on' or 'off'");
    }
    if(sv_equals(toks->txt, SV("on"))){
        p->auto_typedef = 1;
    }
    else if(sv_equals(toks->txt, SV("off"))){
        p->auto_typedef = 0;
    }
    else {
        return cc_error(p, loc, "#pragma typedef requires 'on' or 'off'");
    }
    return 0;
}

static
int
cc_register_pragmas(CcParser* p){
    int err = cpp_register_pragma(&p->cpp, SV("pack"), cc_pragma_pack, p);
    if(err) return err;
    err = cpp_register_pragma(&p->cpp, SV("typedef"), cc_pragma_typedef, p);
    if(err) return err;
    return 0;
}

static
int
cc_lookup_field(CcParser* p, CcField*_Nullable fields, uint32_t count, Atom name,
    CcFieldPath*_Nullable out_path, CcQualType* out_type, CcQualType*_Nullable out_owner, CcField*_Nullable*_Nonnull out_field){
    *out_field = NULL;
    *out_type = CCQT_NONE;
    if(out_path) *out_path = (CcFieldPath){0};
    if(out_owner) *out_owner = CCQT_NONE;
    for(uint32_t i = 0; i < count; i++){
        CcField* field = &fields[i];
        if(field->name == name){
            if(out_path){
                CcFieldPath path;
                if(cc_field_path_make(cc_allocator(p), &i, 1, &path)) return CC_OOM_ERROR;
                *out_path = path;
            }
            *out_field = field;
            *out_type = field->type;
            return 0;
        }
        if(field->name || (ccqt_kind(field->type) != CC_STRUCT && ccqt_kind(field->type) != CC_UNION))
            continue;
        CcStruct* sub = ccqt_as_struct(field->type);
        CcFieldPath suffix = {0};
        int err = cc_lookup_member(p, sub, name, out_path ? &suffix : NULL,
            out_type, out_owner, out_field);
        if(err) return err;
        if(!*out_field) continue;
        if(!(*out_field)->is_method) out_type->quals |= field->type.quals;
        if(out_path){
            CcFieldPath prefix = {0};
            CcFieldPath path = {0};
            err = cc_field_path_make(cc_allocator(p), &i, 1, &prefix);
            if(!err) err = cc_field_path_concat(cc_allocator(p), prefix, suffix, &path);
            cc_field_path_free(cc_allocator(p), prefix);
            cc_field_path_free(cc_allocator(p), suffix);
            if(err) return CC_OOM_ERROR;
            *out_path = path;
        }
        if(out_owner){
            if(!out_owner->bits) *out_owner = field->type;
            else out_owner->quals |= field->type.quals;
        }
        return 0;
    }
    return 0;
}

static
int
cc_lookup_member(CcParser* p, CcStruct* s, Atom name, CcFieldPath* _Nullable path, CcQualType* type, CcQualType* _Nullable owner, CcField* _Nullable* _Nonnull field){
    for(uint32_t i = 0; i < s->methods.count; i++){
        CcField* m = &s->methods.data[i];
        if(m->method->name != name) continue;
        *field = m;
        *type = m->type;
        if(owner) *owner = CCQT_NONE;
        // A synthetic final component identifies the method. Consumers strip
        // it before resolving the data path to the receiver subobject.
        uint32_t index = s->field_count + i;
        if(path){
            CcFieldPath result;
            int err = cc_field_path_make(cc_allocator(p), &index, 1, &result);
            if(err) return err;
            *path = result;
        }
        return 0;
    }
    return cc_lookup_field(p, s->fields, s->field_count, name, path, type, owner, field);
}

static
int
cc_append_method(CcParser* p, CcStruct* s, CcFunc* func, SrcLoc loc){
    if(!func->type->param_count)
        return cc_types_error(p, loc, (CcQualType){.bits=(uintptr_t)func->type}, (CcQualType){.bits=(uintptr_t)s}, "method requires a receiver of its enclosing type or a pointer to it");
    CcQualType receiver = func->type->params[0];
    if(ccqt_kind(receiver) == CC_POINTER) receiver = ccqt_as_ptr(receiver)->pointee;
    if(_ccqt_to_type_ptr(receiver) != s)
        return cc_types_error(p, loc, func->type->params[0], (CcQualType){.bits=(uintptr_t)s}, "method requires a receiver of its enclosing type or a pointer to it");
    CcQualType type;
    CcField* found;
    int err = cc_lookup_member(p, s, func->name, NULL, &type, NULL, &found);
    if(err) return err;
    if(found) return cc_error(p, loc, "duplicate member '%s'", func->name->data);
    if(s->methods.count == s->methods.capacity){
        uint32_t capacity = 4;
        if(s->methods.capacity && mul_overflow(s->methods.capacity, 2u, &capacity))
            return CC_OOM_ERROR;
        CcField* methods = Allocator_realloc(cc_allocator(p), s->methods.data, s->methods.capacity, capacity, sizeof *s->methods.data, _Alignof(CcField));
        if(!methods) return CC_OOM_ERROR;
        s->methods.data = methods;
        s->methods.capacity = capacity;
    }
    s->methods.data[s->methods.count++] = (CcField){.type={
        .bits=(uintptr_t)func->type},
        .method=func,
        .is_method=1,
        .loc=loc
    };
    return 0;
}

static
_Bool
cc_has_field(CcParser* p, CcField*_Nullable fields, uint32_t count, Atom name){
    CcQualType type;
    CcField* field;
    // A name probe doesn't construct or allocate a path.
    (void)cc_lookup_field(p, fields, count, name, NULL, &type, NULL, &field);
    return field != NULL;
}

static
uint32_t
cc_find_field_index(CcParser* p, CcField*_Nullable fields, uint32_t count, Atom name, CcQualType* out_type, Marray(uint32_t)* path){
    CcFieldPath member;
    CcField* field;
    int err = cc_lookup_field(p, fields, count, name, &member, out_type, NULL, &field);
    if(err) return UINT32_MAX;
    if(!field || field->is_method){
        cc_field_path_free(cc_allocator(p), member);
        return count;
    }
    uint32_t first = cc_field_path_component(member, 0);
    for(uint32_t i = 0, n = cc_field_path_count(member); i < n; i++){
        if(ma_push(uint32_t)(path, cc_scratch_allocator(p), cc_field_path_component(member, i))){
            err = CC_OOM_ERROR;
            break;
        }
    }
    cc_field_path_free(cc_allocator(p), member);
    return err ? UINT32_MAX : first;
}

static
int
cc_get_fields(CcQualType t, CcField*_Nullable*_Nonnull out_fields, uint32_t* out_count){
    CcTypeKind tk = ccqt_kind(t);
    if(tk == CC_STRUCT){
        CcStruct* s = ccqt_as_struct(t);
        *out_fields = s->fields;
        *out_count = s->field_count;
        return 0;
    }
    if(tk == CC_UNION){
        CcUnion* u = ccqt_as_union(t);
        *out_fields = u->fields;
        *out_count = u->field_count;
        return 0;
    }
    return 1;
}

static
int
cc_lookup_field_offset(CcParser* p, CcQualType type, Atom name, uint64_t* out_offset, CcQualType* out_type, CcField*_Nullable*_Nonnull out_field){
    CcField* fields;
    uint32_t count;
    if(cc_get_fields(type, &fields, &count)){
        *out_field = NULL;
        return 0;
    }
    CcFieldPath path;
    int err = cc_lookup_member(p, ccqt_as_struct(type), name, &path, out_type, NULL, out_field);
    if(err || !*out_field) return err;
    if((*out_field)->is_method){
        CcFieldPath owner;
        err = cc_field_path_take(cc_allocator(p), path, cc_field_path_count(path)-1, &owner);
        cc_field_path_free(cc_allocator(p), path);
        if(err) return CC_OOM_ERROR;
        path = owner;
    }
    err = cc_field_path_offset(cc_target(p), type, path, out_offset);
    cc_field_path_free(cc_allocator(p), path);
    return err;
}

static
int
cc_push_scalar(CcParser* p, CcExpr* value, CcQualType target, CcInitBuilder* buf, Marray(uint32_t)* path){
    if(ccqt_array_incomplete(target)){
        if((value->kind == CC_EXPR_INIT_LIST || value->kind == CC_EXPR_COMPOUND_LITERAL) && !value->init_list->count){
            cc_release_expr(p, value);
            return 0;
        }
        return cc_error(p, value->loc, "initialization of flexible array members is not supported");
    }
    CcQualType t = {.unqual=target.unqual};
    CcExpr* casted;
    int err = cc_implicit_cast(p, value, t, &casted);
    if(err) return err;
    CcInitEntry entry = {.value = casted};
    if(path->count > UINT32_MAX || cc_field_path_make(cc_allocator(p), path->data, (uint32_t)path->count, &entry.path)){
        cc_release_expr(p, casted);
        return CC_OOM_ERROR;
    }
    err = ma_push(CcInitEntry)(&buf->entries, cc_scratch_allocator(p), entry);
    if(err){
        cc_field_path_free(cc_allocator(p), entry.path);
        cc_release_expr(p, casted);
        return CC_OOM_ERROR;
    }
    return 0;
}

static
int
cc_init_list_comma(CcParser* p){
    CcToken peek;
    int err = cc_peek(p, &peek);
    if(err) return err;
    if(peek.type == CC_PUNCTUATOR && peek.punct.punct == ','){
        cc_next_token(p, &peek);
        return 0;
    }
    if(peek.type == CC_PUNCTUATOR && peek.punct.punct == CC_rbrace)
        return 0;
    return cc_error(p, peek.loc, "expected ',' or '}' in initializer list");
}

static
int
cc_parse_scalar_value(CcParser* p, CcParseFlags flags, CcQualType target, CcExpr*_Nullable*_Nonnull out){
    CcToken peek;
    int err = cc_peek(p, &peek);
    if(err) return err;
    if(peek.type == CC_PUNCTUATOR && peek.punct.punct == CC_lbrace){
        uint32_t depth = 0;
        while(peek.type == CC_PUNCTUATOR && peek.punct.punct == CC_lbrace){
            cc_next_token(p, &peek);
            depth++;
            err = cc_peek(p, &peek);
            if(err) return err;
        }
        if(peek.type == CC_PUNCTUATOR && peek.punct.punct == CC_rbrace){
            CcInitList* list = Allocator_zalloc(cc_allocator(p), 1, sizeof *list, _Alignof(CcInitList));
            if(!list) return CC_OOM_ERROR;
            list->loc = peek.loc;
            CcExpr* node = cc_make_expr(p, CC_EXPR_INIT_LIST, peek.loc, (CcQualType){.unqual = target.unqual}, 0);
            if(!node){
                Allocator_free(cc_allocator(p), list, 1, sizeof *list, _Alignof(CcInitList));
                return CC_OOM_ERROR;
            }
            node->init_list = list;
            *out = node;
        }
        else {
            err = cc_parse_assignment_expr(p, flags, out, target);
            if(err) return err;
        }
        for(uint32_t i = 0; i < depth; i++){
            err = cc_peek(p, &peek);
            if(err) return err;
            if(peek.type == CC_PUNCTUATOR && peek.punct.punct == ','){
                cc_next_token(p, &peek);
                err = cc_peek(p, &peek);
                if(err) return err;
                if(!(peek.type == CC_PUNCTUATOR && peek.punct.punct == CC_rbrace))
                    return cc_error(p, peek.loc, "excess elements in scalar initializer");
            }
            err = cc_expect_punct(p, CC_rbrace);
            if(err) return err;
        }
        return 0;
    }
    return cc_parse_assignment_expr(p, flags, out, target);
}

static
int
cc_parse_desig_tail(CcParser* p, CcParseFlags flags, CcQualType* sub, Marray(uint32_t)* path){
    CcParseFlags constexpr_flags = flags;
    constexpr_flags.vc = CC_CONSTEXPR_VALUE;
    constexpr_flags.unevaluated = 1;
    int err;
    CcToken peek;
    for(;;){
        err = cc_peek(p, &peek);
        if(err) return err;
        if(peek.type != CC_PUNCTUATOR) break;
        if(peek.punct.punct == '.'){
            cc_next_token(p, &peek);
            CcToken field_tok;
            err = cc_next_token(p, &field_tok);
            if(err) return err;
            if(field_tok.type != CC_IDENTIFIER)
                return cc_error(p, field_tok.loc, "expected field name after '.'");
            CcField*_Nullable sub_fields;
            uint32_t sub_count;
            if(cc_get_fields(*sub, &sub_fields, &sub_count))
                return cc_type_error(p, peek.loc, *sub, "member designator into non-struct/union type");
            CcQualType inner_type;
            uint32_t index = cc_find_field_index(p, sub_fields, sub_count, field_tok.ident.ident, &inner_type, path);
            if(index == UINT32_MAX) return CC_OOM_ERROR;
            if(index >= sub_count)
                return cc_error(p, peek.loc, "no member named '%.*s'", field_tok.ident.ident->length, field_tok.ident.ident->data);
            *sub = inner_type;
        }
        else if(peek.punct.punct == CC_lbracket){
            cc_next_token(p, &peek);
            if(ccqt_kind(*sub) != CC_ARRAY)
                return cc_type_error(p, peek.loc, *sub, "index designator into non-array type");
            CcArray* a = ccqt_as_array(*sub);
            CcExpr* idx_expr;
            err = cc_parse_assignment_expr(p, constexpr_flags, &idx_expr, CCQT_NONE);
            if(err) return err;
            int64_t idx_signed;
            err = cc_eval_integer(&(CcEvalCtx){p}, idx_expr, &idx_signed);
            cc_release_expr(p, idx_expr);
            if(err && err != CC_NOT_CONSTANT_ERROR) return err;
            if(err)
                return cc_error(p, peek.loc, "array designator must be a constant integer expression");
            if(idx_signed < 0 || idx_signed > UINT32_MAX)
                return cc_error(p, peek.loc, "array designator value out of range");
            uint32_t idx = (uint32_t)idx_signed;
            if(ma_push(uint32_t)(path, cc_scratch_allocator(p), idx)) return CC_OOM_ERROR;
            err = cc_expect_punct(p, CC_rbracket);
            if(err) return err;
            if(!a->is_incomplete && idx >= a->length)
                return cc_error(p, peek.loc, "array index %u out of bounds (size %zu)", idx, a->length);
            uint32_t esz = 0;
            err = cc_sizeof_as_uint(p, a->element, peek.loc, &esz);
            if(err) return err;
            *sub = a->element;
        }
        else break;
    }
    return cc_expect_punct(p, CC_assign);
}

static
int
cc_init_apply_value(CcParser* p, CcParseFlags flags, CcQualType field_type, CcExpr* value, SrcLoc loc, CcInitBuilder* buf, Marray(uint32_t)* path){
    if(value->kind == CC_EXPR_COMPOUND_LITERAL)
        value->kind = CC_EXPR_INIT_LIST;
    CcQualType unqual = {.unqual=field_type.unqual};
    CcTypeKind ftk = ccqt_kind(unqual);
    if(ftk == CC_STRUCT || ftk == CC_UNION || ftk == CC_ARRAY || ftk == CC_VECTOR){
        if(cc_implicit_convertible(cc_target(p), value->type, unqual))
            return cc_push_scalar(p, value, unqual, buf, path);
        return cc_parse_init(p, flags, field_type, 0, loc, buf, NULL, value, path);
    }
    return cc_push_scalar(p, value, field_type, buf, path);
}


static
int
cc_check_string_array(CcParser* p, CcQualType target_type, CcExpr* value){
    if(value->kind != CC_EXPR_VALUE || ccqt_kind(value->type) != CC_ARRAY || !value->text)
        return cc_types_error(p, value->loc, value->type, target_type, "array initializer requires a string literal");
    if(!cc_string_array_elements_compatible(ccqt_array_element(target_type), ccqt_array_element(value->type)))
        return cc_types_error(p, value->loc, value->type, target_type, "string literal has incompatible array element type");
    return 0;
}

static
int
cc_parse_init_value(CcParser* p, CcParseFlags flags, CcQualType field_type, _Bool positional, SrcLoc loc, CcInitBuilder* buf, Marray(uint32_t)* path){
    if(!positional && buf->entries.count) buf->may_override = 1;
    int err;
    CcTypeKind ftk = ccqt_kind(field_type);
    if(ftk == CC_STRUCT || ftk == CC_UNION || ftk == CC_ARRAY || ftk == CC_VECTOR){
        CcToken peek;
        err = cc_peek(p, &peek);
        if(err) return err;
        if(ftk == CC_ARRAY && peek.type == CC_STRING_LITERAL){
            CcArray* arr = ccqt_as_array(field_type);
            if(cc_string_array_element(cc_target(p), arr->element)){
                CcExpr* v;
                err = cc_parse_assignment_expr(p, flags, &v, CCQT_NONE);
                if(err) return err;
                if(v->kind != CC_EXPR_VALUE || ccqt_kind(v->type) != CC_ARRAY || !v->text)
                    return cc_init_apply_value(p, flags, field_type, v, loc, buf, path);
                err = cc_check_string_array(p, field_type, v);
                if(err) return err;
                if(!arr->is_incomplete && arr->length < v->str.length - 1)
                    return cc_error(p, v->loc, "initializer string too long for array");
                // Truncate string literal if needed.
                if(!arr->is_incomplete && arr->length < ccqt_as_array(v->type)->length)
                    v->type = field_type;
                if(!arr->is_incomplete){
                    CcInitList* list = Allocator_zalloc_extra(cc_allocator(p), 1, sizeof *list->entries, sizeof *list, _Alignof(CcInitList));
                    if(!list) return CC_OOM_ERROR;
                    CcExpr* node = cc_make_expr(p, CC_EXPR_INIT_LIST, v->loc, field_type, 0);
                    if(!node){
                        Allocator_free_extra(cc_allocator(p), list, 1, sizeof *list->entries, sizeof *list, _Alignof(CcInitList));
                        return CC_OOM_ERROR;
                    }
                    list->loc = v->loc;
                    list->count = 1;
                    list->entries[0].value = v;
                    node->init_list = list;
                    v = node;
                }
                return cc_push_scalar(p, v, field_type, buf, path);
            }
        }
        {
            CcExpr* v;
            err = cc_parse_assignment_expr(p, flags, &v, field_type);
            if(err) return err;
            if(positional)
                return cc_init_apply_value(p, flags, field_type, v, loc, buf, path);
            return cc_push_scalar(p, v, field_type, buf, path);
        }
    }
    // Scalar
    CcExpr* v;
    err = cc_parse_scalar_value(p, flags, field_type, &v);
    if(err) return err;
    return cc_push_scalar(p, v, field_type, buf, path);
}

static
int
cc_is_parent_token(CcParser *p, _Bool *out){
    CcToken peek;
    int err = cc_peek(p, &peek);
    if(err) return err;
    if(peek.type == CC_PUNCTUATOR){
        CcPunct pp = peek.punct.punct;
        if(pp == CC_rbrace || pp == '.' || pp == CC_lbracket){
            *out = 1;
            return 0;
        }
    }
    if(peek.type == CC_EOF){
        *out = 1;
        return 0;
    }
    *out = 0;
    return 0;
}

static
int
cc_init_unbraced_comma(CcParser *p, _Bool *stop){
    CcToken comma;
    int err = cc_peek(p, &comma);
    if(err) return err;
    *stop = comma.type != CC_PUNCTUATOR || comma.punct.punct != ',';
    if(*stop) return 0;
    err = cc_next_token(p, &comma);
    if(err) return err;
    err = cc_is_parent_token(p, stop);
    if(err) return err;
    // Leave the separator for the enclosing list when brace elision ends.
    return *stop ? cc_unget(p, &comma) : 0;
}

static
int
cc_parse_designated_init(CcParser *p, CcParseFlags flags, CcQualType target, size_t depth, SrcLoc loc, CcInitBuilder* buf, Marray(uint32_t) *path){
    int err;
    if(depth == path->count){
        err = cc_parse_init_value(p, flags, target, 0, loc, buf, path);
        if(err) return err;
        return cc_init_list_comma(p);
    }
    CcTypeKind kind = ccqt_kind(target);
    CcField *fields = NULL;
    uint32_t count = 0;
    CcQualType element = CCQT_NONE;
    if(kind == CC_ARRAY){
        CcArray *array = ccqt_as_array(target);
        count = (uint32_t)array->length;
        element = array->element;
    }
    else {
        err = cc_get_fields(target, &fields, &count);
        if(err) return err;
    }
    uint32_t index = path->data[depth];
    CcQualType sub = fields ? fields[index].type : element;
    err = cc_parse_designated_init(p, flags, sub, depth + 1, loc, buf, path);
    if(err) return err;
    if(kind == CC_UNION) return 0;
    for(; ++index < count;){
        if(fields && !fields[index].name && fields[index].is_bitfield) 
            continue;
        _Bool stop;
        err = cc_is_parent_token(p, &stop);
        if(err) return err;
        if(stop) break;
        path->count = depth;
        if(ma_push(uint32_t)(path, cc_scratch_allocator(p), index)) return CC_OOM_ERROR;
        sub = fields ? fields[index].type : element;
        err = cc_parse_init_value(p, flags, sub, 1, loc, buf, path);
        if(err) return err;
        err = cc_init_list_comma(p);
        if(err) return err;
    }
    return 0;
}

static
int
cc_parse_init(CcParser* p, CcParseFlags flags, CcQualType target, _Bool braced, SrcLoc loc, CcInitBuilder* buf, uint32_t*_Nullable out_max_index, CcExpr*_Nullable first_value, Marray(uint32_t)* path){
    CcParseFlags constexpr_flags = flags;
    constexpr_flags.vc = CC_CONSTEXPR_VALUE;
    constexpr_flags.unevaluated = 1;
    int err;
    size_t path_depth = path->count;
    CcQualType unqual = {.unqual=target.unqual};
    CcTypeKind tk = ccqt_kind(unqual);
    switch(tk){
    case CC_VLA: {
        if(!braced || first_value)
            return cc_error(p, loc, "VLA initializer must be {} or {0}");
        CcToken tok;
        err = cc_next_token(p, &tok);
        if(err) return err;
        // DrC also accepts {0}, normalized to empty initialization.
        if(tok.type == CC_CONSTANT && tok.constant.ctype == CCBT_int && !tok.constant.integer_value){
            err = cc_next_token(p, &tok);
            if(err) return err;
        }
        if(tok.type != CC_PUNCTUATOR || tok.punct.punct != CC_rbrace)
            return cc_error(p, loc, "VLA initializer must be {} or {0}");
        return 0;
    }
    case CC_ALIGNED: cc_unsafe_unreachable();
    case CC_STRUCT: {
        CcStruct* s = ccqt_as_struct(unqual);
        if(s->is_incomplete)
            return cc_type_error(p, loc, target, "initializer for incomplete struct type");
        uint32_t fi = 0;
        for(;;){
            path->count = path_depth;
            if(first_value){
                CcExpr* fv = first_value;
                while(fi < s->field_count && !s->fields[fi].name && s->fields[fi].is_bitfield)
                    fi++;
                if(fi >= s->field_count) break;
                CcField* fld = &s->fields[fi];
                if(ma_push(uint32_t)(path, cc_scratch_allocator(p), fi)) return CC_OOM_ERROR;
                err = cc_init_apply_value(p, flags, fld->type, fv, loc, buf, path);
                if(err) return err;
                first_value = NULL;
                fi++;
                while(fi < s->field_count && !s->fields[fi].name && s->fields[fi].is_bitfield)
                    fi++;
                if(fi >= s->field_count) break;
                _Bool stop;
                err = cc_init_unbraced_comma(p, &stop);
                if(err) return err;
                if(stop) break;
                continue;
            }
            CcToken peek;
            err = cc_peek(p, &peek);
            if(err) return err;
            if(braced){
                if(peek.type == CC_PUNCTUATOR && peek.punct.punct == CC_rbrace){
                    cc_next_token(p, &peek);
                    break;
                }
                if(peek.type == CC_EOF)
                    return cc_error(p, loc, "unterminated initializer list");
                if(peek.type == CC_PUNCTUATOR && peek.punct.punct == CC_lbracket)
                    return cc_error(p, peek.loc, "array designator in struct initializer");
                if(peek.type == CC_PUNCTUATOR && peek.punct.punct == '.'){
                    cc_next_token(p, &peek);
                    SrcLoc desig_loc = peek.loc;
                    CcToken field_tok;
                    err = cc_next_token(p, &field_tok);
                    if(err) return err;
                    if(field_tok.type != CC_IDENTIFIER)
                        return cc_error(p, field_tok.loc, "expected field name after '.'");
                    CcQualType sub;
                    uint32_t idx = cc_find_field_index(p, s->fields, s->field_count, field_tok.ident.ident, &sub, path);
                    if(idx == UINT32_MAX) return CC_OOM_ERROR;
                    if(idx >= s->field_count)
                        return cc_error(p, desig_loc, "no member named '%.*s'", field_tok.ident.ident->length, field_tok.ident.ident->data);
                    fi = idx + 1;
                    err = cc_parse_desig_tail(p, flags, &sub, path);
                    if(err) return err;
                    err = cc_parse_designated_init(p, flags, s->fields[idx].type, path_depth + 1, desig_loc, buf, path);
                    if(err) return err;
                    continue;
                }
            }
            else {
                _Bool stop;
                err = cc_is_parent_token(p, &stop);
                if(err) return err;
                if(stop) break;
            }
            while(fi < s->field_count && !s->fields[fi].name && s->fields[fi].is_bitfield)
                fi++;
            if(fi >= s->field_count){
                if(braced){
                    if(peek.type == CC_PUNCTUATOR && peek.punct.punct == CC_rbrace){
                        cc_next_token(p, &peek);
                        break;
                    }
                    return cc_error(p, peek.loc, "excess elements in struct initializer");
                }
                break;
            }
            CcField* fld = &s->fields[fi];
            if(ma_push(uint32_t)(path, cc_scratch_allocator(p), fi)) return CC_OOM_ERROR;
            err = cc_parse_init_value(p, flags, fld->type, 1, loc, buf, path);
            if(err) return err;
            fi++;
            while(fi < s->field_count && !s->fields[fi].name && s->fields[fi].is_bitfield)
                fi++;
            if(braced){
                err = cc_init_list_comma(p);
                if(err) return err;
            }
            else {
                if(fi >= s->field_count) break;
                _Bool stop;
                err = cc_init_unbraced_comma(p, &stop);
                if(err) return err;
                if(stop) break;
            }
        }
    } break;
    case CC_UNION: {
        CcUnion* u = ccqt_as_union(unqual);
        if(u->is_incomplete)
            return cc_type_error(p, loc, target, "initializer for incomplete union type");
        CcToken peek;
        err = cc_peek(p, &peek);
        if(err) return err;
        if(braced && peek.type == CC_PUNCTUATOR && peek.punct.punct == CC_rbrace){
            cc_next_token(p, &peek);
            return 0;
        }
        if(braced){
            if(peek.type == CC_EOF)
                return cc_error(p, loc, "unterminated initializer list");
            if(peek.type == CC_PUNCTUATOR && peek.punct.punct == CC_lbracket)
                return cc_error(p, peek.loc, "array designator in union initializer");
            if(peek.type == CC_PUNCTUATOR && peek.punct.punct == '.'){
                cc_next_token(p, &peek);
                SrcLoc desig_loc = peek.loc;
                CcToken field_tok;
                err = cc_next_token(p, &field_tok);
                if(err) return err;
                if(field_tok.type != CC_IDENTIFIER)
                    return cc_error(p, field_tok.loc, "expected field name after '.'");
                CcQualType sub;
                uint32_t ufi = cc_find_field_index(p, u->fields, u->field_count, field_tok.ident.ident, &sub, path);
                if(ufi == UINT32_MAX) return CC_OOM_ERROR;
                if(ufi >= u->field_count)
                    return cc_error(p, desig_loc, "no member named '%.*s'", field_tok.ident.ident->length, field_tok.ident.ident->data);
                err = cc_parse_desig_tail(p, flags, &sub, path);
                if(err) return err;
                err = cc_parse_designated_init(p, flags, u->fields[ufi].type, path_depth + 1, desig_loc, buf, path);
                if(err) return err;
                for(;;){
                    path->count = path_depth;
                    err = cc_peek(p, &peek);
                    if(err) return err;
                    if(peek.type == CC_PUNCTUATOR && peek.punct.punct == CC_rbrace)
                        break;
                    if(peek.type != CC_PUNCTUATOR || peek.punct.punct != '.')
                        return cc_error(p, peek.loc, "expected '.' or '}' in union initializer");
                    cc_next_token(p, &peek);
                    desig_loc = peek.loc;
                    err = cc_next_token(p, &field_tok);
                    if(err) return err;
                    if(field_tok.type != CC_IDENTIFIER)
                        return cc_error(p, field_tok.loc, "expected field name after '.'");
                    ufi = cc_find_field_index(p, u->fields, u->field_count, field_tok.ident.ident, &sub, path);
                    if(ufi == UINT32_MAX) return CC_OOM_ERROR;
                    if(ufi >= u->field_count)
                        return cc_error(p, desig_loc, "no member named '%.*s'", field_tok.ident.ident->length, field_tok.ident.ident->data);
                    err = cc_parse_desig_tail(p, flags, &sub, path);
                    if(err) return err;
                    err = cc_parse_designated_init(p, flags, u->fields[ufi].type, path_depth + 1, desig_loc, buf, path);
                    if(err) return err;
                }
                return cc_expect_punct(p, CC_rbrace);
            }
        }
        CcField* field = NULL;
        for(uint32_t fi = 0; fi < u->field_count; fi++){
            if(u->fields[fi].name || !u->fields[fi].is_bitfield){
                field = &u->fields[fi];
                break;
            }
        }
        if(!field)
            return cc_error(p, loc, "initializer for empty union");
        if(ma_push(uint32_t)(path, cc_scratch_allocator(p), (uint32_t)(field - u->fields))) return CC_OOM_ERROR;
        if(first_value){
            CcExpr* fv = first_value;
            first_value = NULL;
            err = cc_init_apply_value(p, flags, field->type, fv, loc, buf, path);
        }
        else
            err = cc_parse_init_value(p, flags, field->type, 1, loc, buf, path);
        if(err) return err;
        if(braced){
            err = cc_init_list_comma(p);
            if(err) return err;
            err = cc_peek(p, &peek);
            if(err) return err;
            // A positional initializer selects the first member, but later
            // designators may select another member of this same union.
            if(peek.type == CC_PUNCTUATOR && peek.punct.punct == '.'){
                path->count = path_depth;
                return cc_parse_init(p, flags, target, 1, loc, buf, NULL, NULL, path);
            }
            err = cc_expect_punct(p, CC_rbrace);
            if(err) return err;
        }
    } break;
    case CC_VECTOR: {
        CcQualType elem = ccqt_as_vector(unqual)->element;
        uint32_t length = (uint32_t)ccqt_as_vector(unqual)->length;
        uint32_t ai = 0;
        for(;;){
            path->count = path_depth;
            CcToken peek;
            err = cc_peek(p, &peek);
            if(err) return err;
            if(braced){
                if(peek.type == CC_PUNCTUATOR && peek.punct.punct == CC_rbrace){
                    cc_next_token(p, &peek);
                    break;
                }
                if(peek.type == CC_EOF)
                    return cc_error(p, loc, "unterminated initializer list");
                if(peek.type == CC_PUNCTUATOR && (peek.punct.punct == '.' || peek.punct.punct == CC_lbracket))
                    return cc_error(p, peek.loc, "designators not allowed in vector initializer");
            }
            else {
                _Bool stop;
                err = cc_is_parent_token(p, &stop);
                if(err) return err;
                if(stop) break;
            }
            if(ai >= length){
                if(braced)
                    return cc_error(p, peek.loc, "excess elements in vector initializer");
                break;
            }
            CcExpr* v;
            err = cc_parse_scalar_value(p, flags, elem, &v);
            if(err) return err;
            if(ma_push(uint32_t)(path, cc_scratch_allocator(p), ai)) return CC_OOM_ERROR;
            err = cc_push_scalar(p, v, elem, buf, path);
            if(err) return err;
            ai++;
            if(braced){
                err = cc_init_list_comma(p);
                if(err) return err;
            }
            else {
                if(ai >= length) break;
                _Bool stop;
                err = cc_init_unbraced_comma(p, &stop);
                if(err) return err;
                if(stop) break;
            }
        }
        break;
    }
    case CC_ARRAY: {
        CcArray* arr = ccqt_as_array(unqual);
        CcQualType elem = arr->element;
        uint32_t elem_size = 0;
        err = cc_sizeof_as_uint(p, elem, loc, &elem_size);
        if(err) return err;
        if(!arr->is_incomplete){
            uint32_t size;
            err = cc_sizeof_as_uint(p, unqual, loc, &size);
            if(err) return err;
        }
        uint32_t ai = 0, max_ai = 0, literal_skip = 0;
        _Bool literal_runs = ccqt_kind(elem) == CC_BASIC && ccqt_is_integer(elem) && elem_size <= 8 && !elem.is_atomic && !elem.is_volatile;
        for(;;){
            path->count = path_depth;
            if(!first_value && (arr->is_incomplete || ai < arr->length)){
                CcToken packed;
                err = cc_next_token_packed(p, &packed);
                if(err) return err;
                _Bool byte = ccqt_bt_eq(elem, CCBT_unsigned_char) || ccqt_bt_eq(elem, CCBT_signed_char) || ccqt_bt_eq(elem, CCBT_char);
                if(packed.type == CC_EMBED && byte && !elem.is_atomic && !elem.is_volatile){
                    uint32_t n = (uint32_t)packed.embed.bytes.n_bytes;
                    if(!arr->is_incomplete && n > arr->length-ai) n = (uint32_t)(arr->length-ai);
                    uint32_t next, size;
                    if(add_overflow(ai, n, &next) || mul_overflow(elem_size, next, &size))
                        return cc_error(p, packed.loc, "object size exceeds 32-bit layout limit");
                    CcExpr* blob = cc_make_expr(p, CC_EXPR_BLOB, packed.loc, elem, 0);
                    if(!blob) return CC_OOM_ERROR;
                    blob->blob = (ByteView){.buff=packed.embed.bytes.buff, .n_bytes=n};
                    blob->blob_info.count = n;
                    CcInitEntry entry = {.value=blob};
                    err = ma_push(uint32_t)(path, cc_scratch_allocator(p), ai);
                    if(err) goto blob_cleanup;
                    err = cc_field_path_make(cc_allocator(p), path->data, (uint32_t)path->count, &entry.path);
                    if(err) goto blob_cleanup;
                    err = ma_push(CcInitEntry)(&buf->entries, cc_scratch_allocator(p), entry);
                    if(err) goto blob_cleanup;
                    if(n < packed.embed.bytes.n_bytes){
                        packed.embed.bytes.buff = (const unsigned char*)packed.embed.bytes.buff + n; packed.embed.bytes.n_bytes -= n;
                        err = cc_unget(p, &packed);
                        if(err) return err;
                        CcToken comma = {.punct={.type=CC_PUNCTUATOR,.punct=',',.loc=packed.loc}};
                        err = cc_unget(p, &comma);
                        if(err) return err;
                    }
                    ai = next;
                    if(ai > max_ai) max_ai = ai;
                    if(braced){ err = cc_init_list_comma(p); if(err) return err; }
                    else {
                        if(ai >= arr->length) break;
                        CcToken comma;
                        err = cc_peek(p, &comma);
                        if(err) return err;
                        if(comma.type != CC_PUNCTUATOR || comma.punct.punct != ',') break;
                        cc_next_token(p, &comma);
                    }
                    continue;
                    blob_cleanup:
                    cc_field_path_free(cc_allocator(p), entry.path);
                    cc_release_expr(p, blob);
                    return CC_OOM_ERROR;
                }
                uint32_t limit = !literal_runs ? 0 : arr->is_incomplete ? UINT32_MAX / elem_size - ai : (uint32_t)(arr->length - ai);
                enum { CC_INIT_LITERAL_MIN = 4 };
                if(literal_runs
                && !literal_skip
                && limit >= CC_INIT_LITERAL_MIN
                && packed.type == CC_CONSTANT
                && !packed.constant.is_imaginary
                && ccbt_is_integer(packed.constant.ctype)
                && packed.constant.ctype != CCBT_int128
                && packed.constant.ctype != CCBT_unsigned_int128){
                    uint32_t count = 0;
                    DRP_PROFILE_BEGIN(integer_run, "parser.initializer_integer_run");
                    CcToken *probe = Allocator_alloc(cc_scratch_allocator(p), 2*CC_INIT_LITERAL_MIN, sizeof *probe, _Alignof(CcToken));
                    if(!probe) return CC_OOM_ERROR;
                    ByteBuilder bb = {.allocator=allocator_from_arena(&p->cpp.synth_arena)};
                    CcToken tok = packed;
                    CcToken last_sep;
                    _Bool closed = 0;
                    _Bool is_bool = ccqt_is_bool(elem);
                    err = 0;
                    for(;;){
                        CcToken sep;
                        err = cc_next_token_packed(p, &sep);
                        if(err) goto run_cleanup;
                        if(sep.type != CC_PUNCTUATOR || (sep.punct.punct != ',' && sep.punct.punct != CC_rbrace)){
                            err = cc_unget(p, &sep);
                            if(err) goto run_cleanup;
                            err = cc_unget(p, &tok);
                            if(err) goto run_cleanup;
                            break;
                        }
                        probe[2*count] = tok;
                        probe[2*count + 1] = sep;
                        count++;
                        last_sep = sep;
                        closed = sep.punct.punct == CC_rbrace;
                        if(count == CC_INIT_LITERAL_MIN || closed)
                            break;
                        err = cc_next_token_packed(p, &tok);
                        if(err) goto run_cleanup;
                        if(tok.type != CC_CONSTANT
                        || tok.constant.is_imaginary
                        || !ccbt_is_integer(tok.constant.ctype)
                        || tok.constant.ctype == CCBT_int128
                        || tok.constant.ctype == CCBT_unsigned_int128){
                            err = cc_unget(p, &tok);
                            if(err) goto run_cleanup;
                            break;
                        }
                    }
                    if(count < CC_INIT_LITERAL_MIN){
                        for(uint32_t i = 2*count; i--;){
                            err = cc_unget(p, &probe[i]);
                            if(err) goto run_cleanup;
                        }
                        literal_skip += count? count - 1 : 0;
                        count = 0;
                        goto run_cleanup;
                    }
                    err = bb_ensure_additional(&bb, 512);
                    _Static_assert(8 * CC_INIT_LITERAL_MIN <= 512, "unchecked put bounds check");
                    if(err){
                        err = CC_OOM_ERROR;
                        goto run_cleanup;
                    }
                    if(is_bool){
                        for(uint32_t i = 0; i < 2*count; i += 2)
                            bb_put_uint8_assume(&bb, probe[i].constant.uint64_value != 0);
                    }
                    else {
                        switch(elem_size){
                            case 1:
                            for(uint32_t i = 0; i < 2*count; i += 2)
                                bb_put_uint8_assume(&bb, probe[i].constant.uint8_value);
                            break;
                            case 2:
                            for(uint32_t i = 0; i < 2*count; i += 2)
                                bb_put_uint16_assume(&bb, probe[i].constant.uint16_value);
                            break;
                            case 4:
                            for(uint32_t i = 0; i < 2*count; i += 2)
                                bb_put_uint32_assume(&bb, probe[i].constant.uint32_value);
                            break;
                            case 8:
                            for(uint32_t i = 0; i < 2*count; i += 2)
                                bb_put_uint64_assume(&bb, probe[i].constant.uint64_value);
                            break;
                            DRP_CASES_EXHAUSTED;
                        }
                    }
                    for(; !closed && count < limit; count++){
                        err = cc_next_token_packed(p, &tok);
                        if(err) goto run_cleanup;
                        if(tok.type != CC_CONSTANT
                        || tok.constant.is_imaginary
                        || !ccbt_is_integer(tok.constant.ctype)
                        || tok.constant.ctype == CCBT_int128
                        || tok.constant.ctype == CCBT_unsigned_int128){
                            err = cc_unget(p, &tok);
                            if(err) goto run_cleanup;
                            break;
                        }
                        CcToken sep;
                        err = cc_next_token_packed(p, &sep);
                        if(err) goto run_cleanup;
                        if(sep.type != CC_PUNCTUATOR || (sep.punct.punct != ',' && sep.punct.punct != CC_rbrace)){
                            err = cc_unget(p, &sep);
                            if(err) goto run_cleanup;
                            err = cc_unget(p, &tok);
                            if(err) goto run_cleanup;
                            break;
                        }
                        last_sep = sep;
                        closed = sep.punct.punct == CC_rbrace;
                        if(is_bool)
                            bb_put_uint8(&bb, tok.constant.uint64_value != 0);
                        else
                            switch(elem_size){
                                case 1: bb_put_uint8(&bb, tok.constant.uint8_value); break;
                                case 2: bb_put_uint16(&bb, tok.constant.uint16_value); break;
                                case 4: bb_put_uint32(&bb, tok.constant.uint32_value); break;
                                case 8: bb_put_uint64(&bb, tok.constant.uint64_value); break;
                                DRP_CASES_EXHAUSTED;
                            }
                    }
                    if(bb.errored){
                        err = CC_OOM_ERROR;
                        goto run_cleanup;
                    }
                    // Leave the final delimiter for normal list handling.
                    err = cc_unget(p, &last_sep);
                    if(err) goto run_cleanup;
                    CcExpr* blob = cc_make_expr(p, CC_EXPR_BLOB, packed.loc, elem, 0);
                    if(!blob){
                        err = CC_OOM_ERROR;
                        goto run_cleanup;
                    }
                    ByteBuffer blob_data = bb_detach(&bb);
                    blob->blob = (ByteView){.buff=blob_data.buff, .n_bytes=blob_data.n_bytes};
                    blob->blob_info.count = count;
                    CcInitEntry entry = {.value=blob};
                    if(ma_push(uint32_t)(path, cc_scratch_allocator(p), ai)
                    || cc_field_path_make(cc_allocator(p), path->data, (uint32_t)path->count, &entry.path)
                    || ma_push(CcInitEntry)(&buf->entries, cc_scratch_allocator(p), entry)){
                        cc_field_path_free(cc_allocator(p), entry.path);
                        cc_release_expr(p, blob);
                        err = CC_OOM_ERROR;
                        goto run_cleanup;
                    }
                    run_cleanup:
                    Allocator_free(cc_scratch_allocator(p), probe, 2*CC_INIT_LITERAL_MIN, sizeof *probe, _Alignof(CcToken));
                    bb_destroy(&bb);
                    if(err) return err;
                    DRP_PROFILE_END(integer_run);
                    if(count){
                        ai += count;
                        if(ai > max_ai) max_ai = ai;
                        if(braced){
                            err = cc_init_list_comma(p);
                            if(err) return err;
                        }
                        else {
                            if(!arr->is_incomplete && ai >= arr->length) break;
                            _Bool stop;
                            err = cc_init_unbraced_comma(p, &stop);
                            if(err) return err;
                            if(stop) break;
                        }
                        continue;
                    }
                }
                else {
                    if(literal_skip) literal_skip--;
                    err = cc_unget(p, &packed);
                    if(err) return err;
                }
            }
            if(first_value){
                CcExpr* fv = first_value;
                if(!arr->is_incomplete && ai >= arr->length) break;
                if(ma_push(uint32_t)(path, cc_scratch_allocator(p), ai)) return CC_OOM_ERROR;
                err = cc_init_apply_value(p, flags, elem, fv, loc, buf, path);
                if(err) return err;
                first_value = NULL;
                ai++;
                if(ai > max_ai) max_ai = ai;
                if(braced){
                    err = cc_init_list_comma(p);
                    if(err) return err;
                    continue;
                }
                if(!arr->is_incomplete && ai >= arr->length) break;
                _Bool stop;
                err = cc_init_unbraced_comma(p, &stop);
                if(err) return err;
                if(stop) break;
                continue;
            }
            CcToken peek;
            err = cc_peek(p, &peek);
            if(err) return err;
            if(braced){
                if(peek.type == CC_PUNCTUATOR && peek.punct.punct == CC_rbrace){
                    cc_next_token(p, &peek);
                    break;
                }
                if(peek.type == CC_EOF)
                    return cc_error(p, loc, "unterminated initializer list");
                if(peek.type == CC_PUNCTUATOR && peek.punct.punct == '.')
                    return cc_error(p, peek.loc, "field designator in array initializer");
                if(peek.type == CC_PUNCTUATOR && peek.punct.punct == CC_lbracket){
                    cc_next_token(p, &peek);
                    SrcLoc desig_loc = peek.loc;
                    CcExpr* idx_expr;
                    err = cc_parse_assignment_expr(p, constexpr_flags, &idx_expr, CCQT_NONE);
                    if(err) return err;
                    int64_t idx_signed;
                    err = cc_eval_integer(&(CcEvalCtx){p}, idx_expr, &idx_signed);
                    cc_release_expr(p, idx_expr);
                    if(err && err != CC_NOT_CONSTANT_ERROR) return err;
                    if(err)
                        return cc_error(p, desig_loc, "array designator must be a constant integer expression");
                    if(idx_signed < 0 || idx_signed > UINT32_MAX)
                        return cc_error(p, desig_loc, "array designator value out of range");
                    uint32_t idx = (uint32_t)idx_signed;
                    err = cc_expect_punct(p, CC_rbracket);
                    if(err) return err;
                    if(!arr->is_incomplete && idx >= arr->length)
                        return cc_error(p, desig_loc, "array index %u out of bounds (size %zu)", idx, arr->length);
                    uint32_t size;
                    if(add_overflow(idx, 1u, &ai) || mul_overflow(elem_size, ai, &size))
                        return cc_error(p, desig_loc, "object size exceeds 32-bit layout limit");
                    if(ai > max_ai) max_ai = ai;
                    CcQualType sub = elem;
                    if(ma_push(uint32_t)(path, cc_scratch_allocator(p), idx)) return CC_OOM_ERROR;
                    err = cc_parse_desig_tail(p, flags, &sub, path);
                    if(err) return err;
                    err = cc_parse_designated_init(p, flags, elem, path_depth + 1, desig_loc, buf, path);
                    if(err) return err;
                    continue;
                }
            }
            else {
                _Bool stop;
                err = cc_is_parent_token(p, &stop);
                if(err) return err;
                if(stop) break;
            }
            if(!arr->is_incomplete && ai >= arr->length){
                if(braced){
                    err = cc_peek(p, &peek);
                    if(err) return err;
                    if(peek.type == CC_PUNCTUATOR && peek.punct.punct == CC_rbrace){
                        cc_next_token(p, &peek);
                        break;
                    }
                    return cc_error(p, peek.loc, "excess elements in array initializer");
                }
                break;
            }
            uint32_t size, next;
            if(add_overflow(ai, 1u, &next) || mul_overflow(elem_size, next, &size))
                return cc_error(p, peek.loc, "object size exceeds 32-bit layout limit");
            if(ma_push(uint32_t)(path, cc_scratch_allocator(p), ai)) return CC_OOM_ERROR;
            err = cc_parse_init_value(p, flags, elem, 1, loc, buf, path);
            if(err) return err;
            ai = next;
            if(ai > max_ai) max_ai = ai;
            if(braced){
                err = cc_init_list_comma(p);
                if(err) return err;
            }
            else {
                if(!arr->is_incomplete && ai >= arr->length) break;
                _Bool stop;
                err = cc_init_unbraced_comma(p, &stop);
                if(err) return err;
                if(stop) break;
            }
        }
        if(out_max_index) *out_max_index = max_ai;
    } break;
    case CC_BASIC:
    case CC_ENUM:
    case CC_POINTER:
    case CC_BLOCK_POINTER:
    case CC_FUNCTION:
    case CC_SLICE:
        return cc_type_error(p, loc, target, "cannot initialize type with initializer list");
    }
    return 0;
}

static
int
cc_complete_array_type(CcParser* p, CcQualType type, size_t length, CcQualType* out){
    if(ccqt_raw_kind(type) == CC_ALIGNED){
        CcQualType underlying;
        int err = cc_complete_array_type(p, ccqt_unaligned(type), length, &underlying);
        if(err) return err;
        CcAligned* aligned = cc_intern_aligned(&p->type_cache, cc_allocator(p), underlying, ccqt_as_aligned(type)->alignment);
        if(!aligned) return CC_OOM_ERROR;
        *out = (CcQualType){.bits=(uintptr_t)aligned | underlying.quals};
        return 0;
    }
    CcArray completed = *ccqt_as_array(type);
    completed.length = length;
    completed.is_incomplete = 0;
    *out = cc_intern_qualtype(p, (CcQualType){.bits=(uintptr_t)&completed | type.quals});
    return out->bits ? 0 : CC_OOM_ERROR;
}

static void cc_init_index_clear(CcInitIndex*, uint32_t, uint32_t);
static int cc_init_index_apply(CcInitIndex*, uint32_t, uint32_t);

static
void
cc_init_index_drop(CcInitIndex* ctx, uint32_t group, uint32_t slot){
    if(slot == UINT32_MAX) return;
    CcInitWorkEntry* entry = &ctx->lists.data[group].entries.data[slot];
    if(entry->nested != UINT32_MAX)
        cc_init_index_clear(ctx, entry->nested, ctx->lists.data[entry->nested].root);
    if(entry->init.value) cc_release_expr(ctx->parser, entry->init.value);
    entry->init.value = NULL;
    entry->nested = UINT32_MAX;
}

static
void
cc_init_index_clear_children(CcInitIndex* ctx, uint32_t group, uint32_t node){
    uint32_t child = ctx->nodes.data[node].child;
    ctx->nodes.data[node].child = UINT32_MAX;
    while(child != UINT32_MAX){
        uint32_t next = ctx->nodes.data[child].next;
        cc_init_index_clear(ctx, group, child);
        child = next;
    }
}

static
void
cc_init_index_clear(CcInitIndex* ctx, uint32_t group, uint32_t node){
    cc_init_index_clear_children(ctx, group, node);
    CcInitPathNode* n = &ctx->nodes.data[node];
    cc_init_index_drop(ctx, group, n->slot);
    uint32_t blob = n->blobs;
    while(blob != UINT32_MAX){
        uint32_t next = ctx->lists.data[group].entries.data[blob].next_blob;
        cc_init_index_drop(ctx, group, blob);
        blob = next;
    }
    n->slot = n->blobs = UINT32_MAX;
    n->active = 0;
}

static
int
cc_init_index_node(CcInitIndex* ctx, uint32_t index, uint32_t* out){
    if(ctx->nodes.count >= UINT32_MAX) return CC_OOM_ERROR;
    *out = (uint32_t)ctx->nodes.count;
    CcInitPathNode node = {
        .index = index,
        .child = UINT32_MAX,
        .next  = UINT32_MAX,
        .slot  = UINT32_MAX,
        .blobs = UINT32_MAX,
    };
    return ma_push(CcInitPathNode)(&ctx->nodes, cc_scratch_allocator(ctx->parser), node) ? CC_OOM_ERROR : 0;
}

static
int
cc_init_index_open(CcInitIndex* ctx, CcQualType type, SrcLoc loc, CcInitEntry* entries, uint32_t count, uint32_t* out){
    if(ctx->lists.count >= UINT32_MAX) return CC_OOM_ERROR;
    CcInitWorkList list = {.type=type, .loc=loc};
    int err = cc_init_index_node(ctx, 0, &list.root);
    if(err) return err;
    uint32_t group = (uint32_t)ctx->lists.count;
    if(ma_push(CcInitWorkList)(&ctx->lists, cc_scratch_allocator(ctx->parser), list))
        return CC_OOM_ERROR;
    *out = group;
    // Transfer ownership incrementally so the caller can clean up any entries
    // not yet moved if allocation fails.
    for(uint32_t i = 0; i < count; i++){
        CcInitWorkEntry entry = {.init=entries[i], .nested=UINT32_MAX, .next_blob=UINT32_MAX};
        if(ma_push(CcInitWorkEntry)(&ctx->lists.data[group].entries, cc_scratch_allocator(ctx->parser), entry))
            return CC_OOM_ERROR;
        entries[i] = (CcInitEntry){0};
    }
    for(uint32_t i = 0; i < count; i++){
        err = cc_init_index_apply(ctx, group, i);
        if(err) return err;
    }
    return 0;
}

static
int
cc_init_index_apply(CcInitIndex* ctx, uint32_t group, uint32_t slot){
    CcInitWorkEntry* entry = &ctx->lists.data[group].entries.data[slot];
    if(!entry->init.value) return 0;
    CcFieldPath path = entry->init.path;
    uint32_t count = cc_field_path_count(path);
    uint32_t node = ctx->lists.data[group].root;
    CcQualType type = ctx->lists.data[group].type;
    Allocator al = cc_allocator(ctx->parser);
    for(uint32_t i = 0; i < count; i++){
        uint32_t prev = ctx->nodes.data[node].slot;
        if(prev != UINT32_MAX){
            CcInitWorkEntry* owner = &ctx->lists.data[group].entries.data[prev];
            if(owner->init.value->kind == CC_EXPR_INIT_LIST){
                uint32_t nested = owner->nested;
                if(nested == UINT32_MAX){
                    CcInitList* original = owner->init.value->init_list;
                    int err = cc_init_index_open(ctx, type, original->loc, original->entries, original->count, &nested);
                    if(err) return err;
                    owner->nested = nested;
                }
                CcFieldPath relative;
                if(cc_field_path_drop(al, path, i, &relative)) return CC_OOM_ERROR;
                if(ctx->lists.data[nested].entries.count >= UINT32_MAX){
                    cc_field_path_free(al, relative);
                    return CC_OOM_ERROR;
                }
                uint32_t next = (uint32_t)ctx->lists.data[nested].entries.count;
                CcInitWorkEntry added = {.init={.path=relative, .value=entry->init.value},
                    .nested=UINT32_MAX, .next_blob=UINT32_MAX};
                if(ma_push(CcInitWorkEntry)(&ctx->lists.data[nested].entries, cc_scratch_allocator(ctx->parser), added)){
                    cc_field_path_free(al, relative);
                    return CC_OOM_ERROR;
                }
                entry->init.value = NULL;
                return cc_init_index_apply(ctx, nested, next);
            }
        }
        uint32_t index = cc_field_path_component(path, i);
        CcTypeKind kind = ccqt_kind(type);
        uint32_t child = ctx->nodes.data[node].child;
        if(kind == CC_UNION && child != UINT32_MAX && ctx->nodes.data[child].index != index)
            cc_init_index_clear_children(ctx, group, node);
        // PointerMap only hashes these integer keys; it never dereferences them.
        const void* key = (const void*)(uintptr_t)index;
        uintptr_t found = (uintptr_t)PM_get(&ctx->nodes.data[node].children, key);
        if(found) child = (uint32_t)(found-1);
        else {
            int err = cc_init_index_node(ctx, index, &child);
            if(err) return err;
            if(PM_put(&ctx->nodes.data[node].children, cc_scratch_allocator(ctx->parser), key, (const void*)(uintptr_t)(child+1))) return CC_OOM_ERROR;
        }
        if(!ctx->nodes.data[child].active){
            ctx->nodes.data[child].active = 1;
            ctx->nodes.data[child].next = ctx->nodes.data[node].child;
            ctx->nodes.data[node].child = child;
        }
        node = child;
        if(kind == CC_ARRAY || kind == CC_VECTOR) type = ccqt_array_element(type);
        else type = ccqt_as_struct(type)->fields[index].type;
    }
    cc_init_index_clear_children(ctx, group, node);
    cc_init_index_drop(ctx, group, ctx->nodes.data[node].slot);
    ctx->nodes.data[node].slot = UINT32_MAX;
    // A blob spans consecutive array elements. Replacing its first element
    // does not replace its tail. Keep these constant writes in source order;
    // replacing an ancestor or switching union members still discards them.
    if(entry->init.value->kind == CC_EXPR_BLOB){
        entry->next_blob = ctx->nodes.data[node].blobs;
        ctx->nodes.data[node].blobs = slot;
    }
    else ctx->nodes.data[node].slot = slot;
    return 0;
}

static
int
cc_init_index_finish(CcInitIndex* ctx, uint32_t group, CcInitList*_Nullable* _Nonnull out){
    DRP_PROFILE_BEGIN(profile, "parser.initializer_compact");
    CcInitWorkList* list = &ctx->lists.data[group];
    uint32_t count = 0;
    for(size_t i = 0; i < list->entries.count; i++){
        CcInitWorkEntry* entry = &list->entries.data[i];
        if(!entry->init.value) continue;
        if(entry->nested != UINT32_MAX){
            int err = cc_init_index_finish(ctx, entry->nested, &entry->init.value->init_list);
            if(err) return err;
        }
        count++;
    }
    Allocator al = cc_allocator(ctx->parser);
    CcInitList* result = Allocator_zalloc_extra(al, count, sizeof *result->entries, sizeof *result, _Alignof(CcInitList));
    if(!result) return CC_OOM_ERROR;
    result->count = count;
    result->loc = list->loc;
    uint32_t at = 0;
    for(size_t i = 0; i < list->entries.count; i++){
        CcInitWorkEntry* entry = &list->entries.data[i];
        if(!entry->init.value) continue;
        result->entries[at++] = entry->init;
        entry->init = (CcInitEntry){0};
    }
    if(*out)
        Allocator_free_extra(al, *out, (*out)->count, sizeof *result->entries, sizeof *result, _Alignof(CcInitList));
    *out = result;
    return 0;
    DRP_PROFILE_END(profile);
}

static
int
cc_init_may_overlap(CcParser* p, CcQualType type, SrcLoc loc, Marray(CcInitEntry)* entries, _Bool* overlap){
    uint64_t written_end = 0;
    *overlap = 0;
    for(size_t i = 0; i < entries->count; i++){
        CcInitEntry* entry = &entries->data[i];
        CcQualType sub = type;
        uint64_t offset = 0, start = 0;
        _Bool in_union = 0;
        for(uint32_t j = 0, n = cc_field_path_count(entry->path); j < n; j++){
            uint32_t index = cc_field_path_component(entry->path, j);
            CcTypeKind kind = ccqt_kind(sub);
            if(kind == CC_UNION && !in_union){ start = offset; in_union = 1; }
            if(kind == CC_ARRAY || kind == CC_VECTOR){
                sub = ccqt_array_element(sub);
                uint32_t size;
                int err = cc_sizeof_as_uint(p, sub, loc, &size);
                if(err) return err;
                offset += (uint64_t)index * size;
            }
            else {
                CcField* field = &ccqt_as_struct(sub)->fields[index];
                offset += field->offset;
                sub = field->type;
            }
        }
        if(!in_union) start = offset;
        if(start < written_end){ *overlap = 1; return 0; }
        uint32_t size;
        int err = cc_expr_sizeof_as_uint(p, entry->value, &size);
        if(err) return err;
        if(offset + size > written_end) written_end = offset + size;
    }
    return 0;
}

static
int
cc_init_builder_finish(CcParser* p, CcQualType type, SrcLoc loc, CcInitBuilder* builder, CcInitList*_Nullable* _Nonnull out){
    DRP_PROFILE_BEGIN(profile, "parser.initializer_overrides");
    Marray(CcInitEntry)* entries = &builder->entries;
    _Bool overlap = 0;
    int err = 0;
    if(builder->may_override && entries->count > 1){
        err = cc_init_may_overlap(p, type, loc, entries, &overlap);
        if(err) return err;
    }
    if(!overlap){
        CcInitList* result = Allocator_zalloc_extra(cc_allocator(p), entries->count, sizeof *result->entries, sizeof *result, _Alignof(CcInitList));
        if(!result) return CC_OOM_ERROR;
        result->loc = loc;
        result->count = (uint32_t)entries->count;
        for(size_t i = 0; i < entries->count; i++) result->entries[i] = entries->data[i];
        entries->count = 0;
        *out = result;
        return 0;
    }
    CcInitIndex ctx = {.parser=p};
    uint32_t root;
    err = cc_init_index_open(&ctx, type, loc, entries->data, (uint32_t)entries->count, &root);
    if(!err) err = cc_init_index_finish(&ctx, root, out);
    Allocator al = cc_allocator(p);
    for(size_t i = 0; i < ctx.lists.count; i++){
        Marray(CcInitWorkEntry)* work = &ctx.lists.data[i].entries;
        for(size_t j = 0; j < work->count; j++){
            cc_field_path_free(al, work->data[j].init.path);
            if(work->data[j].init.value) cc_release_expr(p, work->data[j].init.value);
        }
        ma_cleanup(CcInitWorkEntry)(work, cc_scratch_allocator(p));
    }
    for(size_t i = 0; i < ctx.nodes.count; i++){
        PointerMap* map = &ctx.nodes.data[i].children;
        if(map->data) Allocator_free(cc_scratch_allocator(p), map->data, PM_alloc_size(map->cap), 1, _Alignof(PointerMapItem));
    }
    ma_cleanup(CcInitWorkList)(&ctx.lists, cc_scratch_allocator(p));
    ma_cleanup(CcInitPathNode)(&ctx.nodes, cc_scratch_allocator(p));
    return err;
    DRP_PROFILE_END(profile);
}


static
int
cc_init_validate_entries(CcParser* p, CcQualType type, SrcLoc loc, CcInitEntry* entries, uint32_t count){
    for(uint32_t i = 0; i < count; i++){
        CcInitEntry* entry = &entries[i];
        uint64_t offset;
        int err = cc_field_path_offset(cc_target(p), type, entry->path, &offset);
        if(err) return err;
        CcQualType sub = type;
        for(uint32_t j = 0, n = cc_field_path_count(entry->path);; j++){
            if(ccqt_array_incomplete(sub)){
                if(j < n || (entry->value && !(entry->value->kind == CC_EXPR_INIT_LIST && !entry->value->init_list->count)))
                    return cc_error(p, entry->value ? entry->value->loc : loc, "initialization of flexible array members is not supported");
            }
            if(j == n) break;
            uint32_t index = cc_field_path_component(entry->path, j);
            CcTypeKind kind = ccqt_kind(sub);
            if(kind == CC_ARRAY || kind == CC_VECTOR)
                sub = ccqt_array_element(sub);
            else if(kind == CC_STRUCT || kind == CC_UNION)
                sub = ccqt_as_struct(sub)->fields[index].type;
            else
                break;
        }
    }
    return 0;
}

static
int
cc_parse_init_list(CcParser* p, CcParseFlags flags, CcExpr* _Nullable* _Nonnull out, CcQualType target_type){
    DRP_PROFILE_BEGIN(profile, "parser.initializer_list");
    CcToken brace;
    int err = cc_next_token(p, &brace);
    if(err) return err;
    SrcLoc loc = brace.loc;
    if(!cc_type_is_initializable(target_type))
        return cc_type_error(p, loc, target_type, "initializer requires a complete object type or an array of unknown bound");
    CcInitList* list = NULL;
    CcQualType resolved_type = target_type;
    CcTypeKind tk = ccqt_kind(target_type);
    CcExpr* first_value = NULL;
    if(tk == CC_ARRAY && cc_string_array_element(cc_target(p), ccqt_array_element(target_type))){
        CcToken peek;
        err = cc_peek(p, &peek);
        if(err) return err;
        if(peek.type == CC_STRING_LITERAL){
            CcExpr* value;
            err = cc_parse_assignment_expr(p, flags, &value, CCQT_NONE);
            if(err) return err;
            if(value->kind != CC_EXPR_VALUE || ccqt_kind(value->type) != CC_ARRAY || !value->text){
                first_value = value;
                goto parse_aggregate;
            }
            err = cc_check_string_array(p, target_type, value);
            if(err) return err;
            CcArray* target = ccqt_as_array(target_type);
            CcArray* source = ccqt_as_array(value->type);
            if(target->is_incomplete){
                err = cc_complete_array_type(p, target_type, source->length, &resolved_type);
                if(err) return err;
            }
            else if(target->length < source->length - 1)
                return cc_error(p, value->loc, "initializer string too long for array");
            else if(target->length < source->length)
                value->type = target_type;
            err = cc_peek(p, &peek);
            if(err) return err;
            if(peek.type == CC_PUNCTUATOR && peek.punct.punct == ',')
                cc_next_token(p, &peek);
            err = cc_expect_punct(p, CC_rbrace);
            if(err) return err;
            list = Allocator_zalloc_extra(cc_allocator(p), 1, sizeof *list->entries, sizeof *list, _Alignof(CcInitList));
            if(!list) return CC_OOM_ERROR;
            list->loc = loc;
            list->count = 1;
            list->entries[0] = (CcInitEntry){.value = value};
            goto make_node;
        }
    }
    if(tk == CC_BASIC || tk == CC_POINTER || tk == CC_ENUM || tk == CC_BLOCK_POINTER || tk == CC_SLICE){
        CcToken peek;
        err = cc_peek(p, &peek);
        if(err) return err;
        if(peek.type == CC_PUNCTUATOR && peek.punct.punct == CC_rbrace){
            cc_next_token(p, &peek);
            list = Allocator_zalloc(cc_allocator(p), 1, sizeof *list, _Alignof(CcInitList));
            if(!list) return CC_OOM_ERROR;
            list->loc = loc;
            list->count = 0;
        }
        else {
            if(peek.type == CC_PUNCTUATOR && (peek.punct.punct == '.' || peek.punct.punct == CC_lbracket))
                return cc_error(p, peek.loc, "designators not allowed in scalar initializer");
            CcExpr* v;
            err = cc_parse_scalar_value(p, flags, target_type, &v);
            if(err) return err;
            CcQualType t = {.unqual=target_type.unqual};
            err = cc_implicit_cast(p, v, t, &v);
            if(err) return err;
            err = cc_peek(p, &peek);
            if(err) return err;
            if(peek.type == CC_PUNCTUATOR && peek.punct.punct == ','){
                cc_next_token(p, &peek);
                err = cc_peek(p, &peek);
                if(err) return err;
                if(!(peek.type == CC_PUNCTUATOR && peek.punct.punct == CC_rbrace))
                    return cc_error(p, peek.loc, "excess elements in scalar initializer");
            }
            err = cc_expect_punct(p, CC_rbrace);
            if(err) return err;
            list = Allocator_zalloc_extra(cc_allocator(p), 1, sizeof *list->entries, sizeof *list, _Alignof(CcInitList));
            if(!list) return CC_OOM_ERROR;
            list->loc = loc;
            list->count = 1;
            list->entries[0] = (CcInitEntry){.value = v};
        }
    }
    else {
        parse_aggregate:;
        CcInitBuilder builder = {0};
        Marray(uint32_t) path = {0};
        uint32_t max_index = 0;
        err = cc_parse_init(p, flags, target_type, 1, loc, &builder, &max_index, first_value, &path);
        ma_cleanup(uint32_t)(&path, cc_scratch_allocator(p));
        if(!err && builder.entries.count > UINT32_MAX) err = CC_OOM_ERROR;
        if(!err && tk == CC_ARRAY){
            CcArray* arr = ccqt_as_array(target_type);
            if(arr->is_incomplete)
                err = cc_complete_array_type(p, target_type, max_index, &resolved_type);
        }
        if(!err)
            err = cc_init_validate_entries(p, resolved_type, loc, builder.entries.data, (uint32_t)builder.entries.count);
        if(!err)
            err = cc_init_builder_finish(p, resolved_type, loc, &builder, &list);
        for(size_t i = 0; i < builder.entries.count; i++){
            cc_field_path_free(cc_allocator(p), builder.entries.data[i].path);
            if(builder.entries.data[i].value)
                cc_release_expr(p, builder.entries.data[i].value);
        }
        ma_cleanup(CcInitEntry)(&builder.entries, cc_scratch_allocator(p));
        if(err) return err;
        goto create_node;
    }
    make_node:;
    err = cc_init_validate_entries(p, resolved_type, loc, list->entries, list->count);
    if(err) goto release_list;
    create_node:;
    CcExpr* node = cc_make_expr(p, CC_EXPR_INIT_LIST, loc, resolved_type, 0);
    if(!node){
        err = CC_OOM_ERROR;
        goto release_list;
    }
    node->init_list = list;
    *out = node;
    return 0;
    release_list:
    for(uint32_t i = 0; i < list->count; i++){
        cc_field_path_free(cc_allocator(p), list->entries[i].path);
        if(list->entries[i].value) cc_release_expr(p, list->entries[i].value);
    }
    Allocator_free_extra(cc_allocator(p), list, (size_t)list->count, sizeof *list->entries, sizeof *list, _Alignof(CcInitList));
    return err;
    DRP_PROFILE_END(profile);
}

static
int
cc_check_anon_member_duplicates(CcParser* p, CcField* existing, uint32_t existing_count, CcQualType anon_type, SrcLoc loc){
    CcField* inner_fields;
    uint32_t inner_count;
    CcTypeKind tk = ccqt_kind(anon_type);
    if(tk == CC_STRUCT){
        CcStruct* s = ccqt_as_struct(anon_type);
        inner_fields = s->fields;
        inner_count = s->field_count;
    }
    else if(tk == CC_UNION){
        CcUnion* u = ccqt_as_union(anon_type);
        inner_fields = u->fields;
        inner_count = u->field_count;
    }
    else {
        return ((void)cc_error(p, loc, "ICE: bad assumption about anonymous field"), CC_UNREACHABLE_ERROR);
    }
    for(uint32_t i = 0; i < inner_count; i++){
        CcField* f = &inner_fields[i];
        if(f->name){
            if(cc_has_field(p, existing, existing_count, f->name))
                return cc_error(p, loc, "duplicate member '%s'", f->name->data);
        }
        else {
            CcTypeKind ftk = ccqt_kind(f->type);
            if(ftk == CC_STRUCT || ftk == CC_UNION){
                int err = cc_check_anon_member_duplicates(p, existing, existing_count, f->type, loc);
                if(err) return err;
            }
        }
    }
    CcStruct* record = ccqt_as_struct(anon_type);
    for(uint32_t i = 0; i < record->methods.count; i++){
        Atom name = record->methods.data[i].method->name;
        if(cc_has_field(p, existing, existing_count, name))
            return cc_error(p, loc, "duplicate member '%s'", name->data);
    }
    return 0;
}

static
int
cc_parse_struct_or_union(CcParser* p, CcParseFlags flags, SrcLoc loc, _Bool is_union, _Bool member, CcQualType* base_type){
    DRP_PROFILE_BEGIN(profile, "parser.struct_or_union");
    CcParseFlags constexpr_flags = flags;
    constexpr_flags.vc = CC_CONSTEXPR_VALUE;
    constexpr_flags.unevaluated = 1;
    int err = 0;
    CcToken tok;
    CcAttributes declaration_attrs = p->attributes;
    CcAttributes attrs = declaration_attrs;
    cc_clear_attributes(&p->attributes);
    err = cc_parse_attributes(p, flags, &attrs);
    if(err) return err;
    err = cc_parse_declspec(p, flags, &attrs);
    if(err) return err;
    Atom name = NULL;
    err = cc_peek(p, &tok);
    if(err) return err;
    if(tok.type == CC_IDENTIFIER){
        err = cc_next_token(p, &tok);
        if(err) return err;
        name = tok.ident.ident;
    }
    err = cc_peek(p, &tok);
    if(err) return err;
    if(tok.type == CC_PUNCTUATOR && tok.punct.punct == '{'){
        err = cc_next_token(p, &tok);
        if(err) return err;
        void* existing = NULL;
        _Bool redefining = 0;
        if(name){
            if(is_union){
                CcUnion* u = existing = cc_scope_lookup_union_tag(cc_scope_declarations(p->current), name, CC_SCOPE_NO_WALK);
                if(u && u->is_defining) return cc_error(p, loc, "Redefinition of %s '%s'", is_union ? "union" : "struct", name->data);
                redefining = u && !u->is_incomplete;
                if(!u){
                    // Register incomplete tag before parsing body so self-references resolve.
                    u = Allocator_zalloc(cc_allocator(p), 1, sizeof *u, _Alignof(CcUnion));
                    if(!u) return CC_OOM_ERROR;
                    *u = (CcUnion){.kind = CC_UNION, .name = name, .loc = loc, .is_incomplete = 1};
                    err = cc_scope_insert_union_tag(cc_allocator(p), cc_scope_declarations(p->current), name, u);
                    if(err) return CC_OOM_ERROR;
                    existing = u;
                    if(p->auto_typedef){
                        err = cc_scope_insert_typedef(cc_allocator(p), cc_scope_declarations(p->current), name, (CcQualType){.bits = (uintptr_t)u}, (SrcLoc){0});
                        if(err) return err;
                    }
                }
            }
            else {
                CcStruct* s = existing = cc_scope_lookup_struct_tag(cc_scope_declarations(p->current), name, CC_SCOPE_NO_WALK);
                if(s && s->is_defining) return cc_error(p, loc, "Redefinition of %s '%s'", is_union ? "union" : "struct", name->data);
                redefining = s && !s->is_incomplete;
                if(!s){
                    // Register incomplete tag before parsing body so self-references resolve.
                    s = Allocator_zalloc(cc_allocator(p), 1, sizeof *s, _Alignof(CcStruct));
                    if(!s) return CC_OOM_ERROR;
                    *s = (CcStruct){.kind = CC_STRUCT, .name = name, .loc = loc, .is_incomplete = 1};
                    err = cc_scope_insert_struct_tag(cc_allocator(p), cc_scope_declarations(p->current), name, s);
                    if(err) return CC_OOM_ERROR;
                    existing = s;
                    if(p->auto_typedef){
                        err = cc_scope_insert_typedef(cc_allocator(p), cc_scope_declarations(p->current), name, (CcQualType){.bits = (uintptr_t)s}, (SrcLoc){0});
                        if(err) return err;
                    }
                }
            }
        }
        else {
            // Anonymous struct/union: allocate incomplete placeholder so _Self
            // inside the body can refer to it.
            if(is_union){
                CcUnion* u = Allocator_zalloc(cc_allocator(p), 1, sizeof *u, _Alignof(CcUnion));
                if(!u) return CC_OOM_ERROR;
                *u = (CcUnion){.kind = CC_UNION, .loc = loc, .is_incomplete = 1};
                existing = u;
            }
            else {
                CcStruct* s = Allocator_zalloc(cc_allocator(p), 1, sizeof *s, _Alignof(CcStruct));
                if(!s) return CC_OOM_ERROR;
                *s = (CcStruct){.kind = CC_STRUCT, .loc = loc, .is_incomplete = 1};
                existing = s;
            }
        }
        CcQualType saved_tag_type = p->current_tag_type;
        ((CcStruct*)existing)->is_defining = 1;
        p->current_tag_type = (CcQualType){.bits = (uintptr_t)existing};
        Marray(CcField) fields_arr = {0};
        _Bool saw_method = 0;
        for(;;){
            err = cc_peek(p, &tok);
            if(err) goto struct_err;
            if(tok.type == CC_PUNCTUATOR && tok.punct.punct == '}')
                break;
            if(tok.type == CC_KEYWORD && tok.kw.kw == CC_static_assert){
                err = cc_handle_static_assert(p, flags);
                if(err) goto struct_err;
                err = cc_expect_punct(p, CC_semi);
                if(err) goto struct_err;
                continue;
            }
            CcAttributes member_attrs = {0};
            cc_clear_attributes(&p->attributes);
            CcDeclBase member_base = {.member=1};
            err = cc_parse_declaration_specifier(p, flags, &member_base);
            if(err) goto struct_err;
            member_attrs = p->attributes;
            cc_clear_attributes(&p->attributes);
            if(member_base.spec.sp_storagebits || member_base.spec.sp_typedef){
                err = cc_error(p, loc, "Storage class specifiers not allowed in struct/union members");
                goto struct_err;
            }
            if(!member_base.spec.bits && !member_base.type.bits){
                err = cc_error(p, tok.loc, "Expected type specifier in struct/union member");
                goto struct_err;
            }
            err = cc_resolve_specifiers(p, &member_base);
            if(err) goto struct_err;
            if(member_base.spec.sp_infer_type){
                err = cc_error(p, tok.loc, "Expected type specifier in struct/union member");
                goto struct_err;
            }
            err = cc_peek(p, &tok);
            if(err) goto struct_err;
            if(tok.type == CC_PUNCTUATOR && tok.punct.punct == ';'){
                err = cc_next_token(p, &tok); // consume ';'
                if(err) goto struct_err;
                // This is either an anonymous struct/union or a Plan9 extension
                // (or just a forward decl).
                CcTypeKind member_tk = ccqt_kind(member_base.type);
                if(member_tk == CC_STRUCT || member_tk == CC_UNION){
                    if(member_tk == CC_STRUCT && ccqt_as_struct(member_base.type)->is_incomplete)
                        continue;
                    if(member_tk == CC_UNION && ccqt_as_union(member_base.type)->is_incomplete)
                        continue;
                    err = cc_check_anon_member_duplicates(p, fields_arr.data, (uint32_t)fields_arr.count, member_base.type, tok.loc);
                    if(err) goto struct_err;
                    CcField field = {
                        .type = member_base.type,
                        .name = NULL, // anonymous
                        .loc = tok.loc,
                    };
                    err = cc_apply_field_attributes(p, &member_base, &member_attrs, &field);
                    if(err) goto struct_err;
                    err = ma_push(CcField)(&fields_arr, cc_allocator(p), field);
                    if(err){
                        err = CC_OOM_ERROR;
                        goto struct_err;
                    }
                    continue;
                }
                if(member_tk == CC_ENUM) // enum decl, it's fine
                    continue;
                cc_warn(p, tok.loc, "Declaration does not declare anything");
                continue;
            }
            // Parse member declarators: name [: bitwidth] [, name [: bitwidth]]* ;
            CcAttributes member_base_attrs = member_attrs;
            for(;;){
                member_attrs = member_base_attrs;
                p->attributes = member_attrs;
                Atom member_name = NULL;
                CcQualType member_type;
                uint64_t bitwidth = 0;
                _Bool is_bitfield = 0;
                // Check for anonymous bitfield: `: bitwidth`
                err = cc_peek(p, &tok);
                if(err) goto struct_err;
                if(tok.type == CC_PUNCTUATOR && tok.punct.punct == ':'){
                    // Anonymous bitfields omit only the declarator.
                    member_type = member_base.type;
                }
                else {
                    // Parse declarator
                    CcQualType head = {0};
                    CcQualType* tail = &head;
                    CcParsedParams param_names = {0};
                    SrcLoc name_loc = {0};
                    err = cc_parse_declarator(p, flags, &head, &tail, &member_name, &name_loc, &param_names);
                    if(err){
                        ma_cleanup(CcFuncParam)(&param_names.names, cc_allocator(p));
                        goto struct_err;
                    }
                    member_attrs = p->attributes;
                    cc_clear_attributes(&p->attributes);
                    *tail = member_base.type;
                    member_type = cc_intern_qualtype(p, head);
                    err = member_type.bits ? cc_check_derived_type(p, member_type, member_base.loc) : CC_OOM_ERROR;
                    if(err){
                        ma_cleanup(CcFuncParam)(&param_names.names, cc_allocator(p));
                        goto struct_err;
                    }
                    // Method: member type is a function type (not pointer to function)
                    if(ccqt_kind(member_type) == CC_FUNCTION){
                        err = cc_check_decl_alignment(p, &member_base, member_type, 0);
                        if(err){
                            ma_cleanup(CcFuncParam)(&param_names.names, cc_allocator(p));
                            goto struct_err;
                        }
                        err = cc_parse_attributes(p, flags, &member_attrs);
                        if(!err) err = cc_apply_vector_type(p, &member_type, member_attrs.vector_size, member_base.loc);
                        if(err){
                            ma_cleanup(CcFuncParam)(&param_names.names, cc_allocator(p));
                            goto struct_err;
                        }
                        if(!member_name){
                            ma_cleanup(CcFuncParam)(&param_names.names, cc_allocator(p));
                            err = cc_error(p, tok.loc, "expected method name");
                            goto struct_err;
                        }
                        saw_method = 1;
                        CcStruct* record = existing;
                        CcField* old_method = NULL;
                        for(uint32_t i = 0; i < record->methods.count; i++)
                            if(record->methods.data[i].method->name == member_name)
                                old_method = &record->methods.data[i];
                        CcFunc* func = old_method ? old_method->method : NULL;
                        Atom symbol = NULL;
                        CcScope* scope = cc_scope_declarations(p->current);
                        if(name && scope == &p->module->scope){
                            MStringBuilder mangled = {.allocator=cc_scratch_allocator(p)};
                            msb_sprintf(&mangled, "__methods.%c_%u_%s_%s", is_union ? 'u' : 's', (unsigned)name->length, name->data, member_name->data);
                            if(mangled.errored){
                                msb_destroy(&mangled);
                                err = CC_OOM_ERROR;
                                goto struct_err;
                            }
                            symbol = msb_atomize(&mangled, p->cpp.at);
                            msb_destroy(&mangled);
                            if(!symbol){
                                err = CC_OOM_ERROR;
                                goto struct_err;
                            }
                            if(!func) func = AM_get(&p->external_methods, symbol);
                        }
                        _Bool new_method = !func;
                        // Check for method body
                        // If tail == &head, the function type came from the
                        // base type (e.g. a typedef), not the declarator.
                        // In that case, a body is not allowed.
                        err = cc_peek(p, &tok);
                        if(err) goto struct_err;
                        _Bool has_body = tok.type == CC_PUNCTUATOR && tok.punct.punct == '{';
                        if(tail == &head && has_body){
                            err = cc_error(p, tok.loc, "cannot define method with typedef function type");
                            goto struct_err;
                        }
                        if(func){
                            if(has_body && func->defined){
                                err = cc_error(p, name_loc, "redefinition of method '%s'", member_name->data);
                                goto struct_err;
                            }
                            CcQualType previous_type = {.bits=(uintptr_t)func->type};
                            // Matching file-scope tags across TUs can denote the
                            // same opaque receiver even when only one TU has its layout.
                            _Bool owner_matches = func->_Self_type.bits
                                && (ccqt_as_struct(func->_Self_type)->is_incomplete
                                    || record->is_incomplete
                                    || cc_types_compatible(func->_Self_type, p->current_tag_type));
                            if(!owner_matches || !cc_method_types_compatible(previous_type, member_type, func->_Self_type, p->current_tag_type)){
                                err = cc_error(p, name_loc, "conflicting declaration of method '%s'", member_name->data);
                                goto struct_err;
                            }
                        }
                        else {
                            func = Allocator_zalloc(cc_allocator(p), 1, sizeof *func, _Alignof(CcFunc));
                            if(!func){ err = CC_OOM_ERROR; goto struct_err; }
                            func->name = member_name;
                            func->loc = name_loc;
                        }
                        if(has_body || !func->type){
                            func->type = ccqt_as_function(member_type);
                            func->_Self_type = p->current_tag_type;
                        }
                        err = cc_merge_default_args(p, func, &param_names, has_body || new_method, name_loc);
                        if(err) goto struct_err;
                        if(has_body || new_method) func->param_scope = param_names.scope;
                        if(has_body){
                            if(param_names.unspecified_vla){
                                err = cc_error(p, name_loc, "[*] is not allowed in a function definition");
                                goto struct_err;
                            }
                            func->body_tree = param_names.bounds;
                        }
                        else cc_free_stmt_tree(p, param_names.bounds);
                        if(old_method) old_method->type = member_type;
                        else {
                            err = cc_append_method(p, record, func, name_loc);
                            if(err) goto struct_err;
                            record->methods.data[record->methods.count-1].type = member_type;
                        }
                        if(symbol){
                            func->mangle = symbol;
                            if(AM_put(&p->external_methods, cc_allocator(p), symbol, func)){
                                err = CC_OOM_ERROR; goto struct_err;
                            }
                        }
                        if(tok.type == CC_PUNCTUATOR && tok.punct.punct == '{'){
                            err = cc_next_token(p, &tok); // consume '{'
                            if(err) goto struct_err;
                            Marray(CcToken)* body_tokens = cc_get_scratch(p);
                            if(!body_tokens){
                                err = CC_OOM_ERROR;
                                goto struct_err;
                            }
                            int depth = 1;
                            while(depth > 0){
                                CcToken t;
                                err = cc_next_token(p, &t);
                                if(err) goto struct_err;
                                if(t.type == CC_EOF){
                                    err = cc_error(p, tok.loc, "Unexpected EOF in method body");
                                    goto struct_err;
                                }
                                if(t.type == CC_PUNCTUATOR){
                                    if(t.punct.punct == '{') depth++;
                                    else if(t.punct.punct == '}') depth--;
                                }
                                if(depth > 0){
                                    err = ma_push(CcToken)(body_tokens, cc_allocator(p), t);
                                    if(err) goto struct_err;
                                }
                            }
                            func->_Self_type = p->current_tag_type;
                            func->tokens = body_tokens;
                            func->defined = 1;
                            func->definition_scope = cc_scope_retain(p->current);
                            func->module = p->module;
                            if(p->current_func){
                                func->enclosing = p->current_func;
                                err = pa_push(&cc_scope_declarations(p->current)->deferred_methods, cc_allocator(p), func);
                                if(err){
                                    err = CC_OOM_ERROR;
                                    goto struct_err;
                                }
                            }
                        }
                        // Parse optional attributes after method
                        err = cc_parse_attributes(p, flags, &member_attrs);
                        if(err) goto struct_err;
                        err = cc_apply_printf_formats(p, func, member_attrs.printf_formats);
                        if(err) goto struct_err;
                        cc_clear_attributes(&p->attributes);
                        if(cc_has_field(p, fields_arr.data, (uint32_t)fields_arr.count, func->name)){
                            err = cc_error(p, tok.loc, "duplicate member '%s'", func->name->data);
                            goto struct_err;
                        }
                        // Method definitions with body don't need ';'
                        if(has_body){
                            err = cc_peek(p, &tok);
                            if(err) goto struct_err;
                            // Allow optional ';' after method body
                            if(tok.type == CC_PUNCTUATOR && tok.punct.punct == ';'){
                                err = cc_next_token(p, &tok);
                                if(err) goto struct_err;
                            }
                            goto next_member;
                        }
                        break; // fall through to ';' expect
                    }
                    ma_cleanup(CcFuncParam)(&param_names.names, cc_allocator(p));
                }
                // Named and unnamed bitfields use the same width/type rules.
                err = cc_peek(p, &tok);
                if(err) goto struct_err;
                if(tok.type == CC_PUNCTUATOR && tok.punct.punct == ':'){
                    err = cc_next_token(p, &tok); // consume ':'
                    if(err) goto struct_err;
                    CcExpr* bw_expr = NULL;
                    err = cc_parse_assignment_expr(p, constexpr_flags, &bw_expr, CCQT_NONE);
                    if(err) goto struct_err;
                    if(!bw_expr){
                        err = cc_error(p, tok.loc, "expected constant expression for bitfield width");
                        goto struct_err;
                    }
                    int64_t bw_i;
                    err = cc_eval_integer(&(CcEvalCtx){p}, bw_expr, &bw_i);
                    cc_release_expr(p, bw_expr);
                    if(err){
                        if(err == CC_NOT_CONSTANT_ERROR)
                            err = cc_error(p, tok.loc, "bitfield width must be a constant integral expression");
                        goto struct_err;
                    }
                    bitwidth = (uint64_t)bw_i;
                    is_bitfield = 1;
                    if(member_type.is_atomic){
                        err = cc_type_error(p, tok.loc, member_type, "atomic bitfields are not supported");
                        goto struct_err;
                    }
                    if(!ccqt_is_integer(member_type)){
                        err = cc_type_error(p, tok.loc, member_type, "bitfield must have integer or enum type");
                        goto struct_err;
                    }
                    uint32_t type_size;
                    err = cc_sizeof_as_uint(p, member_type, tok.loc, &type_size);
                    if(err) goto struct_err;
                    if(member_name && bitwidth == 0){
                        err = cc_error(p, tok.loc, "named bitfield '%s' cannot have zero width", member_name->data);
                        goto struct_err;
                    }
                    uint32_t max_width = ccqt_is_bool(member_type) ? 1 : type_size * 8;
                    if(bitwidth > max_width){
                        err = cc_type_error(p, tok.loc, member_type, "bitfield width (%llu) exceeds size of type (%u bits)", (unsigned long long)bitwidth, max_width);
                        goto struct_err;
                    }
                }
                // Parse optional attributes after the declarator
                err = cc_parse_attributes(p, flags, &member_attrs);
                if(err) goto struct_err;
                cc_clear_attributes(&p->attributes);
                if(member_name && cc_has_field(p, fields_arr.data, (uint32_t)fields_arr.count, member_name)){
                    err = cc_error(p, tok.loc, "duplicate member '%s'", member_name->data);
                    goto struct_err;
                }
                CcField field = {
                    .type = member_type,
                    .name = member_name,
                    .bitwidth = (uint32_t)bitwidth,
                    .is_bitfield = is_bitfield,
                    .loc = tok.loc,
                };
                err = cc_apply_field_attributes(p, &member_base, &member_attrs, &field);
                if(err) goto struct_err;
                err = ma_push(CcField)(&fields_arr, cc_allocator(p), field);
                if(err){
                    err = CC_OOM_ERROR;
                    goto struct_err;
                }
                // Check for comma or semicolon
                err = cc_peek(p, &tok);
                if(err) goto struct_err;
                if(tok.type == CC_PUNCTUATOR && tok.punct.punct == ','){
                    err = cc_next_token(p, &tok); // consume ','
                    if(err) goto struct_err;
                    continue;
                }
                break;
            }
            err = cc_expect_punct(p, CC_semi);
            if(err) goto struct_err;
            next_member:;
        }
        err = cc_expect_punct(p, CC_rbrace);
        if(err) goto struct_err;
        p->current_tag_type = saved_tag_type;
        // Parse optional trailing attributes
        err = cc_parse_attributes(p, flags, &attrs);
        if(err) goto struct_err;
        CcStruct* record = existing;
        for(uint32_t i = 0; i < record->methods.count; i++){
            Atom methods_name = record->methods.data[i].method->name;
            if(cc_has_field(p, fields_arr.data, (uint32_t)fields_arr.count, methods_name)){
                err = cc_error(p, loc, "duplicate member '%s'", methods_name->data);
                goto struct_err;
            }
        }
        // Methods describe an interface, not a data layout. An explicit empty
        // block completes an empty record; a methods-only block never does.
        if(saw_method && !fields_arr.count) goto struct_err;
        if(redefining){
            // Keep the original type (and all pointers to it) canonical. Only the
            // candidate member list and layout are temporary during validation.
            CcStruct candidate = {.kind = is_union ? CC_UNION : CC_STRUCT,
                .name = name, .loc = loc, .fields = fields_arr.data,
                .field_count = (uint32_t)fields_arr.count, .packed = attrs.packed,
                .alignment = attrs.has_aligned ? attrs.aligned : 0};
            err = is_union ? cc_compute_union_layout(p, (CcUnion*)&candidate, p->pragma_pack)
                : cc_compute_struct_layout(p, &candidate, p->pragma_pack);
            if(!err && !cc_record_members_match(existing, &candidate, 1))
                err = cc_error(p, loc, "Redefinition of %s '%s'", is_union ? "union" : "struct", name->data);
            goto struct_err;
        }
        // Finalize
        err = ma_shrink_to_size(CcField)(&fields_arr, cc_allocator(p));
        if(err) return err;
        uint32_t field_count = (uint32_t)fields_arr.count;
        CcField* flat_fields = fields_arr.data;
        if(!is_union){
            CcStruct* s = (CcStruct*)existing;
            if(!s){
                s = Allocator_zalloc(cc_allocator(p), 1, sizeof *s, _Alignof(CcStruct));
                if(!s) return CC_OOM_ERROR;
            }
            *s = (CcStruct){
                .kind = CC_STRUCT,
                .is_incomplete = 1,
                .name = name,
                .loc = loc,
                .field_count = field_count,
                .fields = flat_fields,
                .packed = attrs.packed,
                .methods.data = s->methods.data,
                .methods.count = s->methods.count,
                .methods.capacity = s->methods.capacity,
            };
            if(attrs.has_aligned)
                s->alignment = attrs.aligned;
            err = cc_compute_struct_layout(p, s, p->pragma_pack);
            if(err) return err;
            if(name && !existing){
                err = cc_scope_insert_struct_tag(cc_allocator(p), cc_scope_declarations(p->current), name, s);
                if(err) return CC_OOM_ERROR;
            }
            *base_type = (CcQualType){.bits = (uintptr_t)s};
        }
        else {
            CcUnion* u = (CcUnion*)existing;
            if(!u){
                u = Allocator_zalloc(cc_allocator(p), 1, sizeof *u, _Alignof(CcUnion));
                if(!u) return CC_OOM_ERROR;
            }
            *u = (CcUnion){
                .kind = CC_UNION,
                .is_incomplete = 1,
                .name = name,
                .loc = loc,
                .field_count = field_count,
                .fields = flat_fields,
                .packed = attrs.packed,
                .methods.data = u->methods.data,
                .methods.count = u->methods.count,
                .methods.capacity = u->methods.capacity,
            };
            if(attrs.has_aligned)
                u->alignment = attrs.aligned;
            err = cc_compute_union_layout(p, u, p->pragma_pack);
            if(err) return err;
            if(name && !existing){
                err = cc_scope_insert_union_tag(cc_allocator(p), cc_scope_declarations(p->current), name, u);
                if(err) return CC_OOM_ERROR;
            }
            *base_type = (CcQualType){.bits = (uintptr_t)u};
        }
        return 0;

        struct_err:
        ((CcStruct*)existing)->is_defining = 0;
        p->current_tag_type = saved_tag_type;
        ma_cleanup(CcField)(&fields_arr, cc_allocator(p));
        if(!err) *base_type = (CcQualType){.bits = (uintptr_t)existing};
        return err;
    }

    p->attributes = declaration_attrs;
    if(!name)
        return cc_error(p, loc, "expected %s name or '{'", !is_union ? "struct" : "union");

    _Bool standalone = !member && tok.type == CC_PUNCTUATOR && tok.punct.punct == CC_semi;
    CcScope* tag_scope = standalone ? cc_scope_declarations(p->current) : p->current;
    if(!is_union){
        CcStruct* s = cc_scope_lookup_struct_tag(tag_scope, name, standalone ? CC_SCOPE_NO_WALK : CC_SCOPE_WALK_CHAIN);
        if(!s){
            // Forward declaration — create incomplete struct
            s = Allocator_zalloc(cc_allocator(p), 1, sizeof *s, _Alignof(CcStruct));
            if(!s) return CC_OOM_ERROR;
            *s = (CcStruct){
                .kind = CC_STRUCT,
                .name = name,
                .loc = loc,
                .is_incomplete = 1,
            };
            err = cc_scope_insert_struct_tag(cc_allocator(p), cc_scope_declarations(p->current), name, s);
            if(err) return CC_OOM_ERROR;
        }
        *base_type = (CcQualType){.bits = (uintptr_t)s};
    }
    else {
        CcUnion* u = cc_scope_lookup_union_tag(tag_scope, name, standalone ? CC_SCOPE_NO_WALK : CC_SCOPE_WALK_CHAIN);
        if(!u){
            u = Allocator_zalloc(cc_allocator(p), 1, sizeof *u, _Alignof(CcUnion));
            if(!u) return CC_OOM_ERROR;
            *u = (CcUnion){
                .kind = CC_UNION,
                .name = name,
                .loc = loc,
                .is_incomplete = 1,
            };
            err = cc_scope_insert_union_tag(cc_allocator(p), cc_scope_declarations(p->current), name, u);
            if(err) return CC_OOM_ERROR;
        }
        *base_type = (CcQualType){.bits = (uintptr_t)u};
    }
    return 0;
    DRP_PROFILE_END(profile);
}



static
_Bool
cc_enumerator_value_is_negative(const CcTargetConfig* target, CcQualType type, CiUint128 bits){
    return !ccqt_is_unsigned(type, !target->char_is_signed)
        && (ci_uint128_hi(bits) >> 63);
}

static
_Bool
cc_enumerator_value_fits(const CcTargetConfig* target, CcQualType type, CiUint128 bits, _Bool negative){
    type = ccqt_enum_underlying(type);
    if(ccqt_bt_eq(type, CCBT_bool))
        return !negative && ci_uint128_le(bits, ci_uint128_from_uint64(1));
    uint32_t width = target->sizeof_[ccqt_basic_kind(type)] * 8;
    _Bool unsigned_ = ccqt_is_unsigned(type, !target->char_is_signed);
    CiUint128 magnitude = negative ? ci_uint128_sub(ci_uint128_from_uint64(0), bits) : bits;
    return ci_uint128_magnitude_fits_integer(magnitude, negative, width, unsigned_);
}

static const CcBasicTypeKind cc_enum_signed_types[] = {
    CCBT_signed_char, CCBT_short, CCBT_int, CCBT_long, CCBT_long_long, CCBT_int128,
};
static const CcBasicTypeKind cc_enum_unsigned_types[] = {
    CCBT_unsigned_char, CCBT_unsigned_short, CCBT_unsigned, CCBT_unsigned_long,
    CCBT_unsigned_long_long, CCBT_unsigned_int128,
};

static
int
cc_finalize_enum(CcParser* p, CcEnum* e, _Bool fixed, _Bool packed){
    if(fixed) return 0;
    _Bool all_int = 1, negative = 0;
    for(size_t i = 0; i < e->enumerator_count; i++){
        CcEnumerator* en = e->enumerators[i];
        _Bool neg = cc_enumerator_value_is_negative(cc_target(p), en->type, en->value);
        negative |= neg;
        all_int &= cc_enumerator_value_fits(cc_target(p), ccqt_basic(CCBT_int), en->value, neg);
    }
    CcQualType underlying = CCQT_NONE;
    if(all_int && !packed) underlying = ccqt_basic(CCBT_int);
    else {
        const CcBasicTypeKind* types = negative ? cc_enum_signed_types : cc_enum_unsigned_types;
        uint32_t min_size = packed ? 1 : cc_target(p)->sizeof_[CCBT_int];
        for(size_t i = 0; i < sizeof cc_enum_signed_types / sizeof *cc_enum_signed_types; i++){
            CcQualType candidate = ccqt_basic(types[i]);
            if(cc_target(p)->sizeof_[types[i]] < min_size) continue;
            _Bool fits = 1;
            for(size_t j = 0; j < e->enumerator_count; j++){
                CcEnumerator* en = e->enumerators[j];
                if(!cc_enumerator_value_fits(cc_target(p), candidate, en->value, cc_enumerator_value_is_negative(cc_target(p), en->type, en->value))){
                    fits = 0;
                    break;
                }
            }
            if(fits){
                underlying = candidate;
                break;
            }
        }
    }
    if(!underlying.bits)
        return cc_error(p, e->loc, "no integer type can represent all enumerator values");
    e->underlying = underlying;
    CcQualType member_type = all_int ? ccqt_basic(CCBT_int) : (CcQualType){.bits=(uintptr_t)e};
    for(size_t i = 0; i < e->enumerator_count; i++) e->enumerators[i]->type = member_type;
    return 0;
}

static
int
cc_parse_enum(CcParser* p, CcParseFlags flags, SrcLoc loc, _Bool member, CcQualType* base_type){
    DRP_PROFILE_BEGIN(profile, "parser.enum");
    CcParseFlags constexpr_flags = flags;
    constexpr_flags.vc = CC_CONSTEXPR_VALUE;
    constexpr_flags.unevaluated = 1;
    int err = 0;
    CcToken tok;
    Atom name = NULL;
    CcQualType underlying = ccqt_basic(CCBT_int);
    _Bool has_fixed_underlying = 0;
    // Optional attributes after 'enum'
    CcAttributes enum_attrs = {0};
    err = cc_parse_attributes(p, flags, &enum_attrs);
    if(err) return err;
    // Optional tag name
    err = cc_peek(p, &tok);
    if(err) return err;
    if(tok.type == CC_IDENTIFIER){
        err = cc_next_token(p, &tok);
        if(err) return err;
        name = tok.ident.ident;
    }
    err = cc_parse_attributes(p, flags, &enum_attrs);
    if(err) return err;
    // Optional fixed underlying type: enum name : int { ... }
    err = cc_peek(p, &tok);
    if(err) return err;
    _Bool underlying_colon = tok.type == CC_PUNCTUATOR && tok.punct.punct == ':';
    if(underlying_colon && (member || flags.generic_association)){
        CcToken next;
        err = cc_next_token(p, &tok);
        if(err) return err;
        err = cc_peek(p, &next);
        if(err) return err;
        underlying_colon = cc_is_type_start(p, &next);
        err = cc_unget(p, &tok);
        if(err) return err;
    }
    if(underlying_colon){
        err = cc_next_token(p, &tok); // consume ':'
        if(err) return err;
        has_fixed_underlying = 1;
        CcDeclBase ub = {0};
        err = cc_parse_declaration_specifier(p, flags, &ub);
        if(err) return err;
        CcSpecifier type_spec = ub.spec;
        type_spec.sp_const = type_spec.sp_volatile = type_spec.sp_atomic = 0;
        if(ub.has_alignas || type_spec.sp_typebits != type_spec.bits)
            return cc_error(p, loc, "Underlying type does not allow non-type specifiers");
        if(ub.spec.sp_infer_type)
            return cc_error(p, loc, "__auto_type not allowed as underlying type of enum");
        if(!ub.spec.bits && !ub.type.bits)
            return cc_error(p, loc, "Expected type specifier for enum underlying type");
        err = cc_resolve_specifiers(p, &ub);
        if(err) return err;
        if(!ccqt_is_basic(ub.type) || !ccbt_is_integer(ccqt_basic_kind(ub.type)))
            return cc_type_error(p, loc, ub.type, "enum underlying type must be an integer type");
        // An aligned typedef selects the integer representation, but its
        // alignment override does not become part of the enum's type.
        underlying = (CcQualType){.unqual=ccqt_unaligned(ub.type).unqual};
    }
    // Check for enum body
    err = cc_peek(p, &tok);
    if(err) return err;
    if(tok.type == CC_PUNCTUATOR && tok.punct.punct == '{'){
        err = cc_next_token(p, &tok); // consume '{'
        if(err) return err;
        CcEnum* e = NULL;
        _Bool redefining = 0;
        if(name){
            CcEnum* existing = cc_scope_lookup_enum_tag(cc_scope_declarations(p->current), name, CC_SCOPE_NO_WALK);
            if(existing){
                if(existing->is_defining)
                    return cc_error(p, loc, "Redefinition of enum '%s'", name->data);
                redefining = existing->has_definition;
                if(redefining && !existing->has_fixed_underlying && has_fixed_underlying)
                    return cc_error(p, loc, "Redefinition of enum '%s' with fixed underlying type", name->data);
                if(existing->has_fixed_underlying && has_fixed_underlying && existing->underlying.bits != underlying.bits)
                    return cc_types_error(p, loc, existing->underlying, underlying, "Redefinition of enum '%s' with differing underlying types", name->data);
                if(existing->has_fixed_underlying){
                    underlying = existing->underlying;
                    has_fixed_underlying = 1;
                }
                e = existing;
                if(!redefining){
                    e->loc = loc;
                    e->underlying = underlying;
                }
            }
        }
        if(!e){
            e = Allocator_zalloc(cc_allocator(p), 1, sizeof *e, _Alignof(CcEnum));
            if(!e) return CC_OOM_ERROR;
            *e = (CcEnum){
                .kind = CC_ENUM,
                .name = name,
                .loc = loc,
                .underlying = underlying,
            };
            if(name){
                err = cc_scope_insert_enum_tag(cc_allocator(p), cc_scope_declarations(p->current), name, e);
                if(err)  return CC_OOM_ERROR;
            }
        }
        e->has_fixed_underlying = has_fixed_underlying;
        e->has_definition = 1;
        e->is_incomplete = !redefining && !has_fixed_underlying;
        e->is_defining = 1;
        CcQualType enum_type = {.bits = (uintptr_t)e};
        // Parse enumerator list
        Parray(CcEnumerator) enumerators = {0};
        CiUint128 next_value = ci_uint128_from_uint64(0);
        CcQualType next_type = ccqt_basic(CCBT_int);
        _Bool next_overflow = 0;
        for(;;){
            err = cc_peek(p, &tok);
            if(err) goto enum_err;
            if(tok.type == CC_PUNCTUATOR && tok.punct.punct == '}')
                break;
            err = cc_next_token(p, &tok);
            if(err) goto enum_err;
            if(tok.type != CC_IDENTIFIER){
                err = cc_error(p, tok.loc, "expected enumerator name");
                goto enum_err;
            }
            Atom ename = tok.ident.ident;
            CcSymbol sym;
            _Bool found = cc_scope_lookup_symbol(cc_scope_declarations(p->current), ename, CC_SCOPE_NO_WALK, &sym);
            CcEnumerator* previous = NULL;
            if(redefining){
                for(size_t i = 0; i < e->enumerator_count; i++)
                    if(e->enumerators[i]->name == ename) previous = e->enumerators[i];
                for(size_t i = 0; i < enumerators.count; i++){
                    if(enumerators.data[i] == previous){
                        err = cc_error(p, tok.loc, "Redefinition of enumerator '%s'", ename->data);
                        goto enum_err;
                    }
                }
                if(!previous){
                    err = cc_error(p, loc, "Redefinition of enum '%s'", name->data);
                    goto enum_err;
                }
            }
            if(found && !previous){
                if(sym.kind == CC_SYM_ENUMERATOR)
                    err = cc_error(p, tok.loc, "Redefinition of enumerator '%s'", ename->data);
                else
                    err = cc_error(p, tok.loc, "Redefinition of '%s' as a different kind of symbol", ename->data);
                goto enum_err;
            }
            SrcLoc eloc = tok.loc;
            // Optional attributes after enumerator name
            {
                CcAttributes enumerator_attrs = {0};
                err = cc_parse_attributes(p, flags, &enumerator_attrs);
                if(err) goto enum_err;
            }
            // Optional = constant-expression
            err = cc_peek(p, &tok);
            if(err) goto enum_err;
            if(tok.type == CC_PUNCTUATOR && tok.punct.punct == '='){
                err = cc_next_token(p, &tok); // consume '='
                if(err) goto enum_err;
                CcExpr* expr = NULL;
                err = cc_parse_assignment_expr(p, constexpr_flags, &expr, CCQT_NONE);
                if(err) goto enum_err;
                if(!expr){
                    err = cc_error(p, tok.loc, "expected constant expression");
                    goto enum_err;
                }
                CcExpr* value;
                err = cc_eval_expr(&(CcEvalCtx){p}, expr, &value);
                cc_release_expr(p, expr);
                if(!err){
                    if(!ccqt_is_integer(value->type)) err = CC_NOT_CONSTANT_ERROR;
                    else {
                        next_value = cc_eval_u128(cc_target(p), value);
                        next_type = value->type;
                        if(!has_fixed_underlying && cc_enumerator_value_fits(cc_target(p), ccqt_basic(CCBT_int), next_value,
                            cc_enumerator_value_is_negative(cc_target(p), next_type, next_value))) next_type = ccqt_basic(CCBT_int);
                    }
                    cc_release_expr(p, value);
                }
                if(err == CC_OVERFLOW_ERROR) err = CC_NOT_CONSTANT_ERROR;
                if(err){
                    if(err == CC_NOT_CONSTANT_ERROR)
                        err = cc_error(p, tok.loc, "enumerator value must be a constant integer expression");
                    goto enum_err;
                }
                next_overflow = 0;
            }
            else if(next_overflow){
                err = cc_error(p, eloc, has_fixed_underlying
                    ? "implicit enumerator value exceeds underlying type range"
                    : "implicit enumerator value exceeds supported integer range");
                goto enum_err;
            }
            _Bool negative = cc_enumerator_value_is_negative(cc_target(p), next_type, next_value);
            if(has_fixed_underlying && !cc_enumerator_value_fits(cc_target(p), underlying, next_value, negative)){
                err = cc_error(p, eloc, ccqt_bt_eq(underlying, CCBT_bool)
                    ? "enumerator value is out of range for bool underlying type"
                    : "enumerator value is out of range for underlying type");
                goto enum_err;
            }
            if(has_fixed_underlying) next_type = enum_type;
            CcEnumerator* enumerator = previous;
            if(previous){
                if(!ci_uint128_eq(previous->value, next_value)){
                    err = cc_error(p, loc, "Redefinition of enum '%s'", name->data);
                    goto enum_err;
                }
                next_type = previous->type;
            }
            else {
                enumerator = Allocator_zalloc(cc_allocator(p), 1, sizeof *enumerator, _Alignof(CcEnumerator));
                if(!enumerator){
                    err = CC_OOM_ERROR;
                    goto enum_err;
                }
                *enumerator = (CcEnumerator){
                    .name = ename,
                    .value = next_value,
                    .type = next_type,
                    .loc = eloc,
                };
                err = cc_scope_insert_enumerator(cc_allocator(p), cc_scope_declarations(p->current), ename, enumerator);
                if(err){
                    err = CC_OOM_ERROR;
                    goto enum_err;
                }
            }
            err = pa_push(&enumerators, cc_allocator(p), enumerator);
            if(err){
                err = CC_OOM_ERROR;
                goto enum_err;
            }
            next_overflow = !negative && ci_uint128_eq(next_value, ci_uint128_not(ci_uint128_from_uint64(0)));
            next_value = ci_uint128_add(next_value, ci_uint128_from_uint64(1));
            _Bool next_negative = negative && ci_uint128_nonzero(next_value);
            if(!next_overflow && !cc_enumerator_value_fits(cc_target(p), next_type, next_value, next_negative)){
                next_overflow = 1;
                if(!has_fixed_underlying){
                    _Bool unsigned_ = ccqt_is_unsigned(next_type, !cc_target(p)->char_is_signed);
                    const CcBasicTypeKind* types = unsigned_ ? cc_enum_unsigned_types : cc_enum_signed_types;
                    for(size_t i = 0; i < sizeof cc_enum_signed_types / sizeof *cc_enum_signed_types; i++){
                        CcQualType candidate = ccqt_basic(types[i]);
                        if(cc_enumerator_value_fits(cc_target(p), candidate, next_value, next_negative)){
                            next_type = candidate;
                            next_overflow = 0;
                            break;
                        }
                    }
                }
            }
            err = cc_peek(p, &tok);
            if(err) goto enum_err;
            if(tok.type == CC_PUNCTUATOR && tok.punct.punct == ','){
                err = cc_next_token(p, &tok); // consume ','
                if(err) goto enum_err;
                continue;
            }
            break;
        }
        err = cc_expect_punct(p, CC_rbrace);
        if(err) goto enum_err;
        err = cc_parse_attributes(p, flags, &enum_attrs);
        if(err) goto enum_err;
        if(redefining){
            if(enumerators.count != e->enumerator_count || e->packed != enum_attrs.packed)
                err = cc_error(p, loc, "Redefinition of enum '%s'", name->data);
            if(!err) *base_type = enum_type;
            goto enum_err;
        }
        // Finalize enumerators
        err = pa_shrink_to_size(&enumerators, cc_allocator(p));
        if(err) goto enum_err;
        e->enumerators = (CcEnumerator**)enumerators.data;
        e->enumerator_count = enumerators.count;
        e->packed = enum_attrs.packed;
        err = cc_finalize_enum(p, e, has_fixed_underlying, enum_attrs.packed);
        if(err) goto enum_err;
        e->is_incomplete = 0;
        e->is_defining = 0;
        *base_type = enum_type;
        return 0;

        enum_err:
        e->is_defining = 0;
        pa_cleanup(&enumerators, cc_allocator(p));
        return err;
    }

    // No body — just a reference: enum name
    if(!name)
        return cc_error(p, loc, "expected enum name or '{'");
    _Bool local_tag = has_fixed_underlying || (tok.type == CC_PUNCTUATOR && tok.punct.punct == CC_semi);
    CcEnum* e = cc_scope_lookup_enum_tag(local_tag ? cc_scope_declarations(p->current) : p->current, name,
        local_tag ? CC_SCOPE_NO_WALK : CC_SCOPE_WALK_CHAIN);
    if(e && has_fixed_underlying){
        if(e->has_fixed_underlying && e->underlying.bits != underlying.bits)
            return cc_types_error(p, loc, e->underlying, underlying, "Redefinition of enum '%s' with differing underlying types", name->data);
        if(e->has_definition && !e->has_fixed_underlying)
            return cc_error(p, loc, "Redefinition of enum '%s' with fixed underlying type", name->data);
        e->underlying = underlying;
        e->has_fixed_underlying = 1;
        e->is_incomplete = 0;
    }
    if(!e){
        // A fixed underlying type completes the layout even without a body.
        e = Allocator_zalloc(cc_allocator(p), 1, sizeof *e, _Alignof(CcEnum));
        if(!e) return CC_OOM_ERROR;
        *e = (CcEnum){
            .kind = CC_ENUM,
            .name = name,
            .loc = loc,
            .underlying = underlying,
            .is_incomplete = !has_fixed_underlying,
            .has_fixed_underlying = has_fixed_underlying,
        };
        err = cc_scope_insert_enum_tag(cc_allocator(p), cc_scope_declarations(p->current), name, e);
        if(err) return CC_OOM_ERROR;
    }
    *base_type = (CcQualType){.bits = (uintptr_t)e};
    return 0;
    DRP_PROFILE_END(profile);
}

static
int
cc_parse_declaration_specifier(CcParser* p, CcParseFlags flags, CcDeclBase* base){
    DRP_PROFILE_BEGIN(profile, "parser.declaration_specifiers");
    CcParseFlags constexpr_flags = flags;
    constexpr_flags.vc = CC_CONSTEXPR_VALUE;
    constexpr_flags.unevaluated = 1;
    CcSpecifier* spec = &base->spec;
    CcQualType* base_type = &base->type;
    if(base_type->bits) return cc_unreachable(p, base->loc, "parsing decl specifier with base type set");
    if(spec->bits) return cc_unreachable(p, base->loc, "parsing decl specifier with spec set");
    int err = 0;
    CcToken tok;
    for(int i = 0; ; i++){
        err = cc_next_token(p, &tok);
        if(err) return err;
        if(i == 0) base->loc = tok.loc;
        switch(tok.type){
            case CC_KEYWORD:
                switch(tok.kw.kw){
                    case CC_else:
                    case CC_asm:
                    case CC_true:
                    case CC_do:
                    case CC_if:
                    case CC_case:
                    case CC_goto:
                    case CC_for:
                    case CC_false:
                    case CC_break:
                    case CC_while:
                    case CC_return:
                    case CC__Defer:
                    case CC_sizeof:
                    case CC_switch:
                    case CC_alignof:
                    case CC_default:
                    case CC_nullptr:
                    case CC_continue:
                    case CC__Generic:
                    case CC__Countof:
                    case CC_static_assert:
                        if(i == 0) return cc_unget(p, &tok);
                        return cc_error(p, tok.loc, "Unexpected keyword when parsing declaration");
                    case CC_int:
                        if(base_type->bits)
                            return cc_error(p, tok.loc, "Second type in declaration");
                        if(spec->sp_int)
                            return cc_error(p, tok.loc, "Duplicate int in declaration");
                        if(spec->sp_char)
                            return cc_error(p, tok.loc, "int after char");
                        if(spec->sp___auto_type)
                            return cc_error(p, tok.loc, "int after __auto_type");
                        if(spec->sp_int128)
                            return cc_error(p, tok.loc, "int after __int128");
                        spec->sp_int = 1;
                        continue;
                    case CC_long:
                        if(base_type->bits && !ccqt_bt_eq(*base_type, CCBT_double))
                            return cc_error(p, tok.loc, "Second type in declaration");
                        if(ccqt_bt_eq(*base_type, CCBT_double) && spec->sp_long)
                            return cc_error(p, tok.loc, "long long double in declaration");
                        if(spec->sp_long > 2)
                            return cc_error(p, tok.loc, "Duplicate long after long long long in declaration");
                        if(spec->sp_char)
                            return cc_error(p, tok.loc, "long after char");
                        if(spec->sp_short)
                            return cc_error(p, tok.loc, "long after short");
                        if(spec->sp___auto_type)
                            return cc_error(p, tok.loc, "long after __auto_type");
                        if(spec->sp_int128)
                            return cc_error(p, tok.loc, "long after __int128");
                        spec->sp_long++;
                        continue;
                    case CC_char:
                        if(base_type->bits)
                            return cc_error(p, tok.loc, "Second type in declaration");
                        if(spec->sp_char)
                            return cc_error(p, tok.loc, "Duplicate char in declaration");
                        if(spec->sp_long)
                            return cc_error(p, tok.loc, "char after long");
                        if(spec->sp_short)
                            return cc_error(p, tok.loc, "char after short");
                        if(spec->sp___auto_type)
                            return cc_error(p, tok.loc, "char after __auto_type");
                        if(spec->sp_int)
                            return cc_error(p, tok.loc, "char after int");
                        if(spec->sp_int128)
                            return cc_error(p, tok.loc, "char after __int128");
                        spec->sp_char = 1;
                        continue;
                    case CC___auto_type:
                        if(base_type->bits)
                            return cc_error(p, tok.loc, "Second type in declaration");
                        if(spec->sp_typebits)
                            return cc_error(p, tok.loc, "Second type in declaration");
                        spec->sp___auto_type = 1;
                        continue;
                    case CC___int128:
                        if(base_type->bits)
                            return cc_error(p, tok.loc, "Second type in declaration");
                        if(spec->sp_int128)
                            return cc_error(p, tok.loc, "Duplicate __int128 in declaration");
                        if(spec->sp_char)
                            return cc_error(p, tok.loc, "__int128 after char");
                        if(spec->sp_short)
                            return cc_error(p, tok.loc, "__int128 after short");
                        if(spec->sp_long)
                            return cc_error(p, tok.loc, "__int128 after long");
                        if(spec->sp_int)
                            return cc_error(p, tok.loc, "__int128 after int");
                        if(spec->sp___auto_type)
                            return cc_error(p, tok.loc, "__int128 after __auto_type");
                        spec->sp_int128 = 1;
                        continue;
                    case CC_auto:
                        if(spec->sp_auto)
                            return cc_error(p, tok.loc, "duplicate storage class specifier: auto");
                        if(spec->sp_typedef)
                            return cc_error(p, tok.loc, "auto after typedef");
                        spec->sp_auto = 1;
                        continue;
                    case CC__Any:
                        if(base_type->bits || spec->sp_typebits){
                            Atom a = AT_atomize(p->cpp.at, "_Any", 4);
                            if(!a) return CC_OOM_ERROR;
                            tok = (CcToken){.ident = {.type = CC_IDENTIFIER, .ident = a, .loc = tok.loc}};
                            return cc_unget(p, &tok);
                        }
                        *base_type = ccqt_basic(CCBT__Any);
                        continue;
                    case CC__Type:
                        if(base_type->bits || spec->sp_typebits){
                            Atom a = AT_atomize(p->cpp.at, "_Type", 5);
                            if(!a) return CC_OOM_ERROR;
                            tok = (CcToken){.ident = {.type = CC_IDENTIFIER, .ident = a, .loc = tok.loc}};
                            return cc_unget(p, &tok);
                        }
                        *base_type = ccqt_basic(CCBT__Type);
                        continue;
                    case CC__Self:
                        if(base_type->bits || spec->sp_typebits || !p->current_tag_type.bits){
                            Atom a = AT_atomize(p->cpp.at, "_Self", 5);
                            if(!a) return CC_OOM_ERROR;
                            tok = (CcToken){.ident = {.type = CC_IDENTIFIER, .ident = a, .loc = tok.loc}};
                            return cc_unget(p, &tok);
                        }
                        *base_type = p->current_tag_type;
                        continue;
                    case CC_bool:
                        if(base_type->bits)
                            return cc_error(p, tok.loc, "Second type in declaration");
                        if(spec->sp_typebits)
                            return cc_error(p, tok.loc, "Second type in declaration");
                        *base_type = ccqt_basic(CCBT_bool);
                        continue;
                    case CC_enum: {
                        if(base_type->bits)
                            return cc_error(p, tok.loc, "Second type in declaration");
                        if(spec->sp_typebits)
                            return cc_error(p, tok.loc, "enum with other type specifiers");
                        err = cc_parse_enum(p, flags, tok.loc, base->member, base_type);
                        if(err) return err;
                        continue;
                    }
                    case CC_void:
                        if(base_type->bits)
                            return cc_error(p, tok.loc, "Second type in declaration");
                        if(spec->sp_typebits)
                            return cc_error(p, tok.loc, "Second type in declaration");
                        *base_type = ccqt_basic(CCBT_void);
                        continue;
                    case CC_float:
                        if(base_type->bits)
                            return cc_error(p, tok.loc, "Second type in declaration");
                        if(spec->sp_typebits)
                            return cc_error(p, tok.loc, "Second type in declaration");
                        *base_type = ccqt_basic(CCBT_float);
                        continue;
                    case CC_const:
                        spec->sp_const = 1;
                        continue;
                    case CC_short:
                        if(base_type->bits)
                            return cc_error(p, tok.loc, "Second type in declaration");
                        if(spec->sp_short)
                            return cc_error(p, tok.loc, "Duplicate short in declaration");
                        if(spec->sp_long)
                            return cc_error(p, tok.loc, "short after long");
                        if(spec->sp_char)
                            return cc_error(p, tok.loc, "short after char");
                        if(spec->sp___auto_type)
                            return cc_error(p, tok.loc, "short after __auto_type");
                        if(spec->sp_int128)
                            return cc_error(p, tok.loc, "short after __int128");
                        spec->sp_short = 1;
                        continue;
                    case CC_union: {
                        if(base_type->bits)
                            return cc_error(p, tok.loc, "Second type in declaration");
                        if(spec->sp_typebits)
                            return cc_error(p, tok.loc, "union with other type specifiers");
                        err = cc_parse_struct_or_union(p, flags, tok.loc, 1, base->member, base_type);
                        if(err) return err;
                        continue;
                    }
                    case CC_double:
                        if(base_type->bits)
                            return cc_error(p, tok.loc, "Second type in declaration");
                        {
                            uint32_t count = popcount_32(spec->sp_typebits);
                            if(count > 1 || (count == 1 && spec->sp_long != 1))
                                return cc_error(p, tok.loc, "double with other types");
                        }
                        *base_type = ccqt_basic(CCBT_double);
                        continue;
                    case CC_extern:
                        if(spec->sp_extern)
                            return cc_error(p, tok.loc, "duplicate storage class specifier: extern");
                        if(spec->sp_static)
                            return cc_error(p, tok.loc, "extern after static");
                        if(spec->sp_typedef)
                            return cc_error(p, tok.loc, "extern after typedef");
                        if(spec->sp_register)
                            return cc_error(p, tok.loc, "extern after register");
                        if(spec->sp_constexpr)
                            return cc_error(p, tok.loc, "extern after constexpr");
                        spec->sp_extern = 1;
                        continue;
                    case CC___forceinline:
                        p->attributes.always_inline = 1;
                        goto inline_;
                    case CC_inline:
                        inline_:;
                        if(spec->sp_typedef)
                            return cc_error(p, tok.loc, "inline after typedef");
                        spec->sp_inline = 1;
                        continue;
                    case CC_signed:
                    case CC_unsigned: {
                        _Bool uns = tok.kw.kw == CC_unsigned;
                        const char* spelling = uns ? "unsigned" : "signed";
                        if(base_type->bits)
                            return cc_error(p, tok.loc, "Second type in declaration");
                        if(uns ? spec->sp_unsigned : spec->sp_signed)
                            return cc_error(p, tok.loc, "Duplicate %s in declaration", spelling);
                        if(uns ? spec->sp_signed : spec->sp_unsigned)
                            return cc_error(p, tok.loc, "%s after %s", spelling, uns ? "signed" : "unsigned");
                        if(spec->sp___auto_type)
                            return cc_error(p, tok.loc, "%s after __auto_type", spelling);
                        if(uns) spec->sp_unsigned = 1;
                        else spec->sp_signed = 1;
                        continue;
                    }
                    case CC_static:
                        if(spec->sp_static)
                            return cc_error(p, tok.loc, "duplicate storage class specifier: static");
                        if(spec->sp_extern)
                            return cc_error(p, tok.loc, "static after extern");
                        if(spec->sp_typedef)
                            return cc_error(p, tok.loc, "static after typedef");
                        if(spec->sp_register)
                            return cc_error(p, tok.loc, "static after register");
                        spec->sp_static = 1;
                        continue;
                    case CC_struct: {
                        if(base_type->bits)
                            return cc_error(p, tok.loc, "Second type in declaration");
                        if(spec->sp_typebits)
                            return cc_error(p, tok.loc, "struct with other type specifiers");
                        err = cc_parse_struct_or_union(p, flags, tok.loc, 0, base->member, base_type);
                        if(err) return err;
                        continue;
                    }
                    case CC_typeof:
                        goto do_typeof;
                    case CC_alignas: {
                        base->has_alignas = 1;
                        err = cc_expect_punct(p, CC_lparen);
                        if(err) return err;
                        CcToken peek;
                        err = cc_peek(p, &peek);
                        if(err) return err;
                        uint32_t align_val;
                        if(cc_is_type_start(p, &peek)){
                            CcQualType align_type;
                            CcParseFlags align_flags = flags;
                            align_flags.unevaluated = 1;
                            err = cc_parse_type_name(p, align_flags, &align_type, NULL);
                            if(err) return err;
                            err = cc_alignof_as_uint(p, align_type, tok.loc, &align_val);
                            if(err) return err;
                        }
                        else {
                            CcExpr* expr = NULL;
                            err = cc_parse_assignment_expr(p, constexpr_flags, &expr, CCQT_NONE);
                            if(err) return err;
                            if(!expr)
                                return cc_error(p, tok.loc, "expected expression in _Alignas");
                            int64_t av;
                            err = cc_eval_integer(&(CcEvalCtx){p}, expr, &av);
                            cc_release_expr(p, expr);
                            if(err && err != CC_NOT_CONSTANT_ERROR) return err;
                            if(err)
                                return cc_error(p, tok.loc, "_Alignas requires a constant integer expression");
                            if(av < 0)
                                return cc_error(p, tok.loc, "_Alignas value must be non-negative");
                            if(av != 0 && (av & (av - 1)) != 0)
                                return cc_error(p, tok.loc, "_Alignas value must be zero or a power of 2");
                            if(av > UINT16_MAX)
                                return cc_error(p, tok.loc, "alignment too large");
                            align_val = (uint32_t)av;
                        }
                        err = cc_expect_punct(p, CC_rparen);
                        if(err) return err;
                        if(align_val > base->alignas_alignment)
                            base->alignas_alignment = (uint16_t)align_val;
                        if(align_val > 0){
                            if(align_val > base->alignment)
                                base->alignment = (uint16_t)align_val;
                        }
                        continue;
                    }
                    case CC_typedef:
                        if(spec->sp_typedef)
                            return cc_error(p, tok.loc, "duplicate storage class specifier: typedef");
                        if(spec->sp_storagebits)
                            return cc_error(p, tok.loc, "typedef after storage class");
                        if(spec->sp_funcbits)
                            return cc_error(p, tok.loc, "typedef after function specifier");
                        spec->sp_typedef = 1;
                        continue;
                    case CC__Atomic: {
                        CcToken peek;
                        err = cc_peek(p, &peek);
                        if(err) return err;
                        if(peek.type == CC_PUNCTUATOR && peek.punct.punct == CC_lparen){
                            if(base_type->bits)
                                return cc_error(p, tok.loc, "Second type in declaration");
                            if(spec->sp_typebits)
                                return cc_error(p, tok.loc, "Second type in declaration");
                            err = cc_next_token(p, &peek); // consume '('
                            if(err) return err;
                            err = cc_parse_type_name(p, flags, base_type, NULL);
                            if(err) return err;
                            err = cc_check_atomic_type_specifier_type(p, *base_type, tok.loc);
                            if(err) return err;
                            err = cc_expect_punct(p, CC_rparen);
                            if(err) return err;
                            base_type->is_atomic = 1;
                        }
                        spec->sp_atomic = 1;
                        continue;
                    }
                    case CC__BitInt:
                        return cc_unimplemented(p, tok.loc, "_BitInt parsing in declaration");
                    case CC__Complex:
                        if(spec->sp_complex)
                            return cc_error(p, tok.loc, "Duplicate _Complex in declaration");
                        spec->sp_complex = 1;
                        continue;
                    case CC_register:
                        if(spec->sp_register)
                            return cc_error(p, tok.loc, "duplicate storage class specifier: register");
                        if(spec->sp_typedef)
                            return cc_error(p, tok.loc, "register after typedef");
                        if(spec->sp_static)
                            return cc_error(p, tok.loc, "register after static");
                        if(spec->sp_extern)
                            return cc_error(p, tok.loc, "register after extern");
                        if(spec->sp_thread_local)
                            return cc_error(p, tok.loc, "register after thread_local");
                        spec->sp_register = 1;
                        continue;
                    case CC_restrict:
                        spec->sp_restrict = 1;
                        continue;
                    case CC_volatile:
                        spec->sp_volatile = 1;
                        continue;
                    case CC_constexpr:
                        if(spec->sp_constexpr)
                            return cc_error(p, tok.loc, "duplicate storage class specifier: constexpr");
                        if(spec->sp_extern)
                            return cc_error(p, tok.loc, "constexpr after extern");
                        if(spec->sp_typedef)
                            return cc_error(p, tok.loc, "constexpr after typedef");
                        if(spec->sp_thread_local)
                            return cc_error(p, tok.loc, "constexpr after thread_local");
                        if(spec->sp_atomic)
                            return cc_error(p, tok.loc, "constexpr after _Atomic");
                        spec->sp_constexpr = 1;
                        continue;
                    {
                        CcQualType t;
                    case CC___fp16:
                        t = ccqt_basic(CCBT_fp16);
                        goto float_decl;
                    case CC___bf16:
                        t = ccqt_basic(CCBT_bfloat16);
                        goto float_decl;
                    case CC__Float16:
                        t = ccqt_basic(CCBT_float16);
                        goto float_decl;
                    case CC__Float32:
                        t = ccqt_basic(CCBT_float);
                        goto float_decl;
                    case CC__Float64:
                        t = ccqt_basic(CCBT_double);
                        goto float_decl;
                    case CC__Float128:
                        t = ccqt_basic(CCBT_float128);
                        goto float_decl;
                    case CC__Float32x:
                        t = ccqt_basic(CCBT_double);
                        goto float_decl;
                    case CC__Float64x:
                        t = ccqt_basic(CCBT_long_double);
                        goto float_decl;

                        float_decl:
                        if(base_type->bits)
                            return cc_error(p, tok.loc, "Second type in declaration");
                        if(spec->sp_typebits)
                            return cc_error(p, tok.loc, "Second type in declaration");
                        *base_type = t;
                    }continue;
                    case CC__Imaginary:
                        return cc_unimplemented(p, tok.loc, "_Imaginary parsing in declaration");
                    case CC__Noreturn:
                        if(spec->sp_typedef)
                            return cc_error(p, tok.loc, "noreturn after typedef");
                        spec->sp_noreturn = 1;
                        continue;
                    case CC__Decimal32:
                    case CC__Decimal64:
                    case CC__Decimal128:
                        return cc_unimplemented(p, tok.loc, "_DecimalNN parsing in declaration");
                    case CC_thread_local:
                        if(spec->sp_thread_local)
                            return cc_error(p, tok.loc, "duplicate storage class specifier: thread_local");
                        if(spec->sp_typedef)
                            return cc_error(p, tok.loc, "thread_local after typedef");
                        if(spec->sp_register)
                            return cc_error(p, tok.loc, "thread_local after register");
                        if(spec->sp_constexpr) // standard says no, but why not?
                            return cc_error(p, tok.loc, "thread_local after constexpr");
                        spec->sp_thread_local = 1;
                        continue;
                    case CC___attribute__:
                        err = cc_unget(p, &tok);
                        if(err) return err;
                        err = cc_parse_attributes(p, flags, &p->attributes);
                        if(err) return err;
                        goto transfer_attrs;
                    case CC___declspec:
                        err = cc_unget(p, &tok);
                        if(err) return err;
                        err = cc_parse_declspec(p, flags, &p->attributes);
                        if(err) return err;
                        goto transfer_attrs;
                    case CC_typeof_unqual:
                    do_typeof: {
                        if(base_type->bits)
                            return cc_error(p, tok.loc, "typeof after type");
                        if(spec->sp_typebits)
                            return cc_error(p, tok.loc, "typeof after type specifiers");
                        _Bool unqual = tok.kw.kw == CC_typeof_unqual;
                        err = cc_expect_punct(p, CC_lparen);
                        if(err) return err;
                        CcExpr* expr = NULL;
                        err = cc_parse_type_query_operand(p, flags, CC_rparen, 1, base_type, &expr);
                        if(err) return err;
                        if(expr){
                            if(ccqt_is_vm(expr->type) && !flags.unevaluated){
                                CcStmtNode* effect = cc_stmt_node(p, CC_STMT_EXPR, expr->loc, 0);
                                if(!effect) return CC_OOM_ERROR;
                                effect->expr = expr;
                                err = cc_sink_push(p, effect);
                                if(err) return err;
                            }
                            else cc_release_expr(p, expr);
                        }
                        if(unqual){
                            err = cc_unqual_object_type(p, *base_type, base_type);
                            if(err) return err;
                        }
                        err = cc_expect_punct(p, CC_rparen);
                        if(err) return err;
                        continue;
                    }
                }
                break;
            case CC_IDENTIFIER: {
                if(spec->sp_typebits || base_type->bits){
                    // Already have a type — this identifier is not a type name.
                    return cc_unget(p, &tok);
                }
                CcSymbol sym;
                if(!cc_scope_lookup_symbol(p->current, tok.ident.ident, CC_SCOPE_WALK_CHAIN, &sym) || sym.kind != CC_SYM_TYPEDEF){
                    // Not a typedef (or shadowed by var/func) — end of specifiers.
                    return cc_unget(p, &tok);
                }
                *base_type = sym.type;
                continue;
            }
            case CC_EMBED:
                return cc_unreachable(p, tok.loc, "embed should be gone by now");
            case CC_EOF:
            case CC_CONSTANT:
            case CC_STRING_LITERAL:
                return cc_unget(p, &tok);
            case CC_PUNCTUATOR:
                if(tok.punct.punct == CC_lbracket){
                    CcToken peek;
                    err = cc_peek(p, &peek);
                    if(err) return err;
                    if(peek.type == CC_PUNCTUATOR && peek.punct.punct == CC_lbracket){
                        err = cc_unget(p, &tok);
                        if(err) return err;
                        err = cc_parse_c23_attributes(p, flags, &p->attributes);
                        if(err) return err;
                        goto transfer_attrs;
                    }
                }
                return cc_unget(p, &tok);
            transfer_attrs:
                if(p->attributes.has_aligned){
                    if(p->attributes.aligned > base->alignment)
                        base->alignment = p->attributes.aligned;
                }
                if(p->attributes.is_noreturn){
                    spec->sp_noreturn = 1;
                    p->attributes.is_noreturn = 0;
                }
                if(p->attributes.is_thread_local){
                    spec->sp_thread_local = 1;
                    p->attributes.is_thread_local = 0;
                }
                continue;
        }
    }
    return 0;
    DRP_PROFILE_END(profile);
}

static
CcStmtNode*_Nullable
cc_stmt_node(CcParser* p, CcStmtKind k, SrcLoc loc, uint32_t count){
    if(p->current_func && k == CC_STMT_VLA)
        p->current_func->inline_blocked = 1;
    CcStmtNode* n = Allocator_zalloc_extra(cc_allocator(p), count, sizeof *n->stmts, sizeof *n, _Alignof(CcStmtNode));
    if(!n) return NULL;
    n->kind = k;
    n->count = count;
    n->loc = loc;
    if(k == CC_STMT_LABEL || k == CC_STMT_CASE || k == CC_STMT_DEFAULT){
        n->has_jump_target = 1;
        p->stmt_jump_targets++;
    }
    return n;
}

static
void
cc_free_stmt_tree(CcParser* p, CcStmtNode*_Nullable n){
    if(!n) return;
    switch(n->kind){
        case CC_STMT_EXPR:
        case CC_STMT_IF:
        case CC_STMT_WHILE:
        case CC_STMT_DOWHILE:
        case CC_STMT_SWITCH:
        case CC_STMT_RETURN:
            if(n->exprs[0]) cc_release_expr(p, n->exprs[0]);
            break;
        case CC_STMT_FOR:
            if(n->exprs[0]) cc_release_expr(p, n->exprs[0]);
            if(n->exprs[1]) cc_release_expr(p, n->exprs[1]);
            break;
        case CC_STMT_NULL:
        case CC_STMT_VLA:
        case CC_STMT_DEFER:
        case CC_STMT_COMPOUND:
        case CC_STMT_CASE:
        case CC_STMT_DEFAULT:
        case CC_STMT_BREAK:
        case CC_STMT_CONTINUE:
        case CC_STMT_GOTO:
        case CC_STMT_LABEL:
            break;
    }
    for(uint32_t i = 0; i < n->count; i++)
        cc_free_stmt_tree(p, n->stmts[i]);
    pa_cleanup(&n->decls, cc_allocator(p));
    Allocator_free_extra(cc_allocator(p), n, n->count, sizeof *n->stmts, sizeof *n, _Alignof(CcStmtNode));
}

static
CcStmtSink*_Nullable
cc_push_stmt_sink(CcParser* p){
    CcStmtSink* sink = fl_pop(&p->scratch_stmt_sinks);
    if(!sink) sink = Allocator_zalloc(cc_allocator(p), 1, sizeof *sink, _Alignof(CcStmtSink));
    if(!sink) return NULL;
    sink->stmts.count = 0;
    sink->prev = p->stmt_sink;
    p->stmt_sink = sink;
    return sink;
}

static
void
cc_pop_stmt_sink(CcParser* p, CcStmtSink* sink){
    p->stmt_sink = sink->prev;
    for(size_t i = 0; i < sink->stmts.count; i++)
        cc_free_stmt_tree(p, sink->stmts.data[i]);
    fl_push(&p->scratch_stmt_sinks, sink);
}

static
int
cc_stmt_scope_vars(CcParser* p, CcStmtNode* stmt){
    AtomMapItems items = CcAnonAM_items(&p->current->variables);
    for(size_t i = 0; i < items.count; i++){
        CcVariable* var = items.data[i].p;
        if(!var || !var->automatic) continue;
        int err = pa_push(&stmt->decls, cc_allocator(p), var);
        if(err) return CC_OOM_ERROR;
    }
    return 0;
}

static inline
void
cc_stmt_summarize_flow(CcStmtNode* n){
    switch(n->kind){
        case CC_STMT_RETURN: case CC_STMT_BREAK: case CC_STMT_CONTINUE: case CC_STMT_GOTO:
            n->no_fallthrough = 1;
            break;
        case CC_STMT_COMPOUND:
            n->no_fallthrough = 0;
            for(uint32_t i = 0; i < n->count; i++){
                CcStmtNode* child = n->stmts[i];
                if(!n->no_fallthrough || child->has_jump_target)
                    n->no_fallthrough = child->no_fallthrough;
            }
            break;
        case CC_STMT_IF:
            n->no_fallthrough = n->stmts[0] && n->stmts[1]
                && n->stmts[0]->no_fallthrough && n->stmts[1]->no_fallthrough;
            break;
        case CC_STMT_LABEL: case CC_STMT_CASE: case CC_STMT_DEFAULT:
            n->no_fallthrough = n->stmts[0] && n->stmts[0]->no_fallthrough;
            break;
        default: n->no_fallthrough = 0; break;
    }
}


static
int
cc_sink_push(CcParser* p, CcStmtNode* n){
    cc_stmt_summarize_flow(n);
    int err;
    if(!p->stmt_sink){
        SrcLoc loc = n->loc;
        cc_free_stmt_tree(p, n);
        return cc_unreachable(p, loc, "statement with no active statement sink");
    }
    err = pa_push(&p->stmt_sink->stmts, cc_allocator(p), n);
    if(err){
        cc_free_stmt_tree(p, n);
        return CC_OOM_ERROR;
    }
    return 0;
}

static
int
cc_finalize_stmt_list(CcParser* p, SrcLoc loc, Parray(CcStmtNode)* list, _Bool always_compound, CcStmtNode*_Nullable*_Nonnull out){
    *out = NULL;
    if(!always_compound && list->count == 1){
        *out = list->data[0];
        list->count = 0;
        return 0;
    }
    CcStmtNode* n = cc_stmt_node(p, CC_STMT_COMPOUND, loc, (uint32_t)list->count);
    if(!n) return CC_OOM_ERROR;
    for(size_t i = 0; i < list->count; i++){
        n->stmts[i] = list->data[i];
        n->has_jump_target |= n->stmts[i]->has_jump_target;
    }
    cc_stmt_summarize_flow(n);
    list->count = 0;
    *out = n;
    return 0;
}

typedef struct CcSwitchCtx CcSwitchCtx;
struct CcSwitchCtx {
    CcQualType type;
    CcDeferContext defer_context;
    _Bool has_default;
    Marray(CcSwitchEntry) cases; // for duplicate detection
};

typedef struct CcControl CcControl;
struct CcControl {
    CcControl* _Nullable prev;
    CcControlLabel* _Nullable labels;
    CcDeferContext context;
    _Bool loop;
};

static
int
cc_eval_case_range(CcParser* p, SrcLoc loc, CcQualType type, CcExpr* low, CcExpr* high, CiUint128* lo, CiUint128* hi, _Bool* empty){
    if(ccqt_bt_eq(type, CCBT__Type))
        return cc_error(p, loc, "case ranges require an integer switch expression");
    CcExpr* exprs[2] = {low, high};
    CcExpr* values[2] = {NULL, NULL};
    CiUint128 bits[2];
    _Bool negative[2];
    int err = 0;
    for(unsigned i = 0; i < 2; i++){
        if(!ccqt_is_integer(exprs[i]->type))
            err = CC_NOT_CONSTANT_ERROR;
        else
            err = cc_eval_expr(&(CcEvalCtx){.parser=p}, exprs[i], &values[i]);
        if(err == CC_NOT_CONSTANT_ERROR || err == CC_OVERFLOW_ERROR)
            err = cc_error(p, loc, "case range endpoints must be constant integer expressions");
        if(err) goto done;
        bits[i] = cc_eval_u128(cc_target(p), values[i]);
        negative[i] = !ccqt_is_unsigned(values[i]->type, !cc_target(p)->char_is_signed) && (ci_uint128_hi(bits[i]) >> 63);
    }
    // Compare mathematical values without applying the usual arithmetic conversions.
    *empty = negative[0] != negative[1] ? negative[1] : ci_uint128_gt(bits[0], bits[1]);
    if(*empty){
        cc_warn(p, loc, "empty case range");
        *lo = ci_uint128_from_uint64(1);
        *hi = ci_uint128_from_uint64(0);
        goto done;
    }
    for(unsigned i = 0; i < 2; i++){
        CcExpr* converted = NULL;
        err = cc_implicit_cast(p, values[i], type, &values[i]);
        if(err) goto done;
        err = cc_eval_expr(&(CcEvalCtx){.parser=p}, values[i], &converted);
        if(err) goto done;
        CiUint128 after = cc_eval_u128(cc_target(p), converted);
        _Bool after_negative = !ccqt_is_unsigned(type, !cc_target(p)->char_is_signed) && (ci_uint128_hi(after) >> 63);
        cc_release_expr(p, converted);
        if(!ci_uint128_eq(bits[i], after) || negative[i] != after_negative){
            err = cc_error(p, loc, "case range values must be representable in the switch type");
            goto done;
        }
    }
    *lo = bits[0];
    *hi = bits[1];
    done:
    for(unsigned i = 0; i < 2; i++)
        if(values[i]) cc_release_expr(p, values[i]);
    return err;
}

static
int
cc_parse_label(CcParser* p, CcParseFlags flags, CcStmtNode*_Nullable*_Nonnull out){
    flags.vc = CC_CONSTEXPR_VALUE;
    flags.unevaluated = 1;
    CcToken tok;
    int err = cc_peek(p, &tok);
    if(err) return err;
    if(tok.type != CC_IDENTIFIER && (tok.type != CC_KEYWORD || (tok.kw.kw != CC_case && tok.kw.kw != CC_default))){
        *out = NULL;
        return 0;
    }
    err = cc_next_token(p, &tok);
    if(err) return err;
    if(tok.type == CC_IDENTIFIER){
        CcToken peek;
        err = cc_peek(p, &peek);
        if(err) return err;
        if(peek.type != CC_PUNCTUATOR || peek.punct.punct != ':'){
            *out = NULL;
            return cc_unget(p, &tok);
        }
        cc_next_token(p, &peek); // consume ':'
        Atom label_name = tok.ident.ident;
        CcLabelCtx* lctx = cc_label_ctx(p);
        if(cc_lctx_has_defer(lctx, label_name))
            return cc_error(p, tok.loc, "Duplicate label '%.*s'", label_name->length, label_name->data);
        CcStmtNode* node = cc_stmt_node(p, CC_STMT_LABEL, tok.loc, 1);
        if(!node) return CC_OOM_ERROR;
        node->label = label_name;
        err = cc_lctx_put_defer(lctx, cc_allocator(p), label_name, &p->defer_context);
        if(err){
            cc_free_stmt_tree(p, node);
            return CC_OOM_ERROR;
        }
        *out = node;
        return 0;
    }
    if(!p->switch_ctx)
        return cc_error(p, tok.loc, "'%s' label not within a switch statement", tok.kw.kw == CC_case ? "case" : "default");
    err = cc_check_transfer(p, tok.loc, p->switch_ctx->defer_context, p->defer_context);
    if(err) return err;
    if(tok.kw.kw == CC_default){
        err = cc_expect_punct(p, ':');
        if(err) return err;
        if(p->switch_ctx->has_default)
            return cc_error(p, tok.loc, "Multiple default labels in switch");
        p->switch_ctx->has_default = 1;
        CcStmtNode* node = cc_stmt_node(p, CC_STMT_DEFAULT, tok.loc, 1);
        if(!node) return CC_OOM_ERROR;
        *out = node;
        return 0;
    }
    CcExpr* case_expr;
    err = cc_parse_expr(p, flags, &case_expr, CCQT_NONE);
    if(err) return err;
    CcToken next;
    CcExpr* high_expr = NULL;
    err = cc_peek(p, &next);
    if(!err && next.type == CC_PUNCTUATOR && next.punct.punct == CC_ellipsis){
        err = cc_next_token(p, &next);
        if(!err) err = cc_parse_expr(p, flags, &high_expr, CCQT_NONE);
    }
    if(err){
        cc_release_expr(p, case_expr);
        return err;
    }
    err = cc_expect_punct(p, ':');
    if(err){
        cc_release_expr(p, case_expr);
        if(high_expr) cc_release_expr(p, high_expr);
        return err;
    }
    uint64_t case_val, case_high = 0;
    _Bool empty = 0, range = high_expr != NULL;
    if(high_expr){
        CiUint128 low, high;
        err = cc_eval_case_range(p, tok.loc, p->switch_ctx->type, case_expr, high_expr, &low, &high, &empty);
        cc_release_expr(p, case_expr);
        cc_release_expr(p, high_expr);
        if(err) return err;
        case_val = ci_uint128_lo(low);
        case_high = ci_uint128_lo(high);
    }
    else if(ccqt_bt_eq(p->switch_ctx->type, CCBT__Type)){
        CcExpr* case_val_expr;
        err = cc_eval_expr(&(CcEvalCtx){.parser = p}, case_expr, &case_val_expr);
        if(err == CC_OVERFLOW_ERROR) err = CC_NOT_CONSTANT_ERROR;
        cc_release_expr(p, case_expr);
        if(err && err != CC_NOT_CONSTANT_ERROR) return err;
        if(err)
            return cc_error(p, tok.loc, "case label must be a constant _Type expression");
        if(!ccqt_bt_eq(case_val_expr->type, CCBT__Type)){
            err = cc_types_error(p, tok.loc, case_val_expr->type, p->switch_ctx->type, "case label type does not match _Type switch expression");
            cc_release_expr(p, case_val_expr);
            return err;
        }
        case_val = case_val_expr->type_value.bits;
        cc_release_expr(p, case_val_expr);
    }
    else {
        int64_t case_i = 0;
        // Validate before casting, which could otherwise make floating cases integral.
        if(!ccqt_is_integer(case_expr->type)) err = CC_NOT_CONSTANT_ERROR;
        else {
            err = cc_implicit_cast(p, case_expr, p->switch_ctx->type, &case_expr);
            if(!err) err = cc_eval_integer(&(CcEvalCtx){p}, case_expr, &case_i);
        }
        cc_release_expr(p, case_expr);
        if(err && err != CC_NOT_CONSTANT_ERROR) return err;
        if(err)
            return cc_error(p, tok.loc, "case label must be a constant integer expression");
        case_val = (uint64_t)case_i;
    }
    if(!range) case_high = case_val;
    _Bool uns = ccqt_bt_eq(p->switch_ctx->type, CCBT__Type) || ccqt_is_unsigned(p->switch_ctx->type, !cc_target(p)->char_is_signed);
    uint64_t sign = uns ? 0 : UINT64_C(1) << 63;
    if(!empty){
        for(size_t i = 0; i < p->switch_ctx->cases.count; i++){
            CcSwitchEntry prev = p->switch_ctx->cases.data[i];
            if((case_val ^ sign) <= (prev.high ^ sign) && (prev.low ^ sign) <= (case_high ^ sign)){
                if(!range && prev.low == prev.high)
                    return cc_error(p, tok.loc, "duplicate case value '%lld'", (long long)case_val);
                return cc_error(p, tok.loc, "overlapping case values");
            }
        }
        CcSwitchEntry entry = {.low = case_val, .high = case_high};
        err = ma_push(CcSwitchEntry)(&p->switch_ctx->cases, cc_allocator(p), entry);
        if(err) return CC_OOM_ERROR;
    }
    CcStmtNode* node = cc_stmt_node(p, CC_STMT_CASE, tok.loc, 1);
    if(!node) return CC_OOM_ERROR;
    node->case_.low = case_val;
    node->case_.high = case_high;
    node->case_empty = empty;
    *out = node;
    return 0;
}

static int cc_parse_statement_inner(CcParser*, CcParseFlags, CcControlLabel*_Nullable, CcStmtNode*_Nullable*_Nonnull);

static
int
cc_parse_scoped_statement(CcParser* p, CcParseFlags flags, _Bool unlabeled, CcStmtNode*_Nullable*_Nonnull out){
    CcDeferContext saved = p->defer_context;
    CcStmtNode* stmt = NULL;
    int err = cc_parse_statement(p, flags, NULL, &stmt);
    if(err) goto cleanup;
    if(unlabeled && (stmt->kind == CC_STMT_LABEL || stmt->kind == CC_STMT_CASE || stmt->kind == CC_STMT_DEFAULT)){
        err = cc_error(p, stmt->loc, "expected unlabeled statement after _Defer");
        goto cleanup;
    }
    if(p->defer_context.active == saved.active)
        goto success;
    CcStmtNode* wrapper = cc_stmt_node(p, CC_STMT_COMPOUND, stmt->loc, 1);
    if(!wrapper){
        err = CC_OOM_ERROR;
        goto cleanup;
    }
    wrapper->stmts[0] = stmt;
    wrapper->has_jump_target = stmt->has_jump_target;
    wrapper->cleanup = (CcCleanupRange){p->defer_context.active, saved.active};
    cc_stmt_summarize_flow(wrapper);
    stmt = wrapper;

    success:
    *out = stmt;
    stmt = NULL;

    cleanup:
    cc_free_stmt_tree(p, stmt);
    p->defer_context = saved;
    return err;
}

static
int
cc_parse_control_body(CcParser* p, CcParseFlags flags, _Bool loop, CcControlLabel*_Nullable labels, CcStmtNode*_Nullable*_Nonnull out){
    CcControl control = {.prev=p->control, .labels=labels, .context=p->defer_context, .loop=loop};
    p->control = &control;
    int err = cc_parse_scoped_statement(p, flags, 0, out);
    p->control = control.prev;
    return err;
}

static
int
cc_static_header_declaration(CcParser* p, _Bool* declaration){
    // Static headers also accept type expressions (int, int*, T == int,
    // etc.). An initialized declaration has an unparenthesized '=';
    // explicit storage classes also select declaration parsing.
    *declaration = 0;
    CcToken first;
    int err = cc_peek(p, &first);
    if(err) return err;
    if(!cc_is_compound_type_start(p, &first) && !(first.type == CC_PUNCTUATOR && first.punct.punct == '['))
        return 0;
    Marray(CcToken) tokens = {0};
    int depth = 0;
    for(;;){
        CcToken tok;
        err = cc_next_token(p, &tok);
        if(err) goto cleanup;
        err = ma_push(CcToken)(&tokens, cc_allocator(p), tok);
        if(err){
            err = CC_OOM_ERROR;
            goto cleanup;
        }
        if(tok.type == CC_EOF) break;
        if(!depth && tok.type == CC_KEYWORD){
            switch(tok.kw.kw){
                case CC_constexpr:
                case CC_typedef:
                case CC_extern:
                case CC_static:
                case CC_thread_local:
                case CC_register:
                case CC_auto:
                    *declaration = 1; break;
                default: break;
            }
        }
        if(tok.type != CC_PUNCTUATOR)
            continue;
        CcPunct punct = tok.punct.punct;
        if(!depth && (punct == ';' || punct == ')'))
            break;
        if(!depth && punct == '=') *declaration = 1;
        if(punct == '(' || punct == '[' || punct == '{')
            depth++;
        else if(punct == ')' || punct == ']' || punct == '}')
            depth--;
    }
    for(size_t i = tokens.count; i; i--){
        err = cc_unget(p, &tokens.data[i-1]);
        if(err) goto cleanup;
    }
    cleanup:
    ma_cleanup(CcToken)(&tokens, cc_allocator(p));
    return err;
}

static
int
cc_parse_header_decls(CcParser* p, CcParseFlags flags, CcParsedDecls* decls, _Bool* declaration){
    int err;
    *declaration = 1;
    if(flags.static_header){
        err = cc_static_header_declaration(p, declaration);
        if(err || !*declaration) return err;
    }
    CcDeclBase base = {0};
    err = cc_parse_declaration_specifier(p, flags, &base);
    if(err) return err;
    *declaration = base.spec.bits || base.type.bits;
    if(!*declaration) return 0;
    if(flags.static_header && !base.spec.sp_typedef){
        if(base.spec.sp_extern || base.spec.sp_static || base.spec.sp_thread_local || base.spec.sp_register)
            return cc_error(p, base.loc, "invalid storage class in static control header");
        base.spec.sp_constexpr = 1;
        // These bindings have no runtime initialization, even inside functions.
        // Establish storage before parsing so their initializers are not owned
        // by discarded automatic-initialization statements.
        base.spec.sp_static = 1;
    }
    err = cc_resolve_specifiers(p, &base);
    if(err) return err;
    err = cc_parse_decls(p, flags, &base, decls);
    if(err) return err;
    if(flags.static_header){
        AtomMapItems items = CcAnonAM_items(&p->current->variables);
        for(size_t i = 0; i < items.count; i++){
            CcVariable* var = items.data[i].p;
            if(!var->initializer)
                return cc_error(p, var->loc, "static header variable requires an initializer");
            var->automatic = 0;
            var->static_ = 1;
            var->interp_preinit = 1;
        }
    }
    return 0;
}

static
int
cc_parse_for_condition(CcParser* p, CcParseFlags flags, SrcLoc loc, CcExpr*_Nullable*_Nonnull out){
    *out = NULL;
    CcStmtSink* sink = cc_push_stmt_sink(p);
    if(!sink) return CC_OOM_ERROR;
    CcExpr* expr = NULL;
    CcStmtNode* body = NULL;
    CcParsedDecls decls = {0};
    _Bool declaration;
    int err = cc_parse_header_decls(p, flags, &decls, &declaration);
    if(err) goto cleanup;
    if(declaration){
        if(decls.function_definition){
            err = cc_error(p, decls.function_definition->loc, "function definition is not allowed in a for condition");
            goto cleanup;
        }
        err = cc_expect_punct(p, CC_semi);
        if(err) goto cleanup;
        CcVariable* last = decls.last_object;
        if(!last || !last->initializer){
            err = cc_error(p, loc, "implicit condition requires an initialized object");
            goto cleanup;
        }
        expr = cc_make_expr(p, CC_EXPR_VARIABLE, last->loc, last->type, 0);
        if(!expr){
            err = CC_OOM_ERROR;
            goto cleanup;
        }
        expr->is_lvalue = 1;
        expr->var = last;
    }
    else {
        err = cc_parse_expr(p, flags, &expr, CCQT_NONE);
        if(err) goto cleanup;
        err = cc_expect_punct(p, ';');
        if(err) goto cleanup;
    }
    err = cc_require_scalar(p, &expr, loc, "'for' condition");
    if(err) goto cleanup;
    if(!flags.static_header && sink->stmts.count){
        CcQualType type = expr->type;
        CcStmtNode* value = cc_stmt_node(p, CC_STMT_EXPR, loc, 0);
        if(!value){
            err = CC_OOM_ERROR;
            goto cleanup;
        }
        value->exprs[0] = expr;
        expr = NULL;
        err = cc_sink_push(p, value);
        if(err) goto cleanup;
        err = cc_finalize_stmt_list(p, loc, &sink->stmts, 1, &body);
        if(err) goto cleanup;
        // Keep initialization in the condition
        expr = cc_make_expr(p, CC_EXPR_STATEMENT_EXPRESSION, loc, type, 0);
        if(!expr){
            err = CC_OOM_ERROR;
            goto cleanup;
        }
        expr->stmt_body = body;
        body = NULL;
    }
    *out = expr;
    expr = NULL;
    cleanup:
    if(expr) cc_release_expr(p, expr);
    cc_free_stmt_tree(p, body);
    cc_pop_stmt_sink(p, sink);
    return err;
}

static
int
cc_parse_selection_header_inner(CcParser* p, CcParseFlags flags, const char* statement, CcExpr*_Nullable*_Nonnull out){
    *out = NULL;
    int err = cc_expect_punct(p, '(');
    if(err) return err;
    for(;;){
        CcToken first;
        err = cc_peek(p, &first);
        if(err) return err;
        CcParsedDecls decls = {0};
        CcExpr* expr = NULL;
        _Bool declaration;
        err = cc_parse_header_decls(p, flags, &decls, &declaration);
        if(err) return err;
        if(declaration){
            if(decls.function_definition)
                return cc_error(p, decls.function_definition->loc, "function definition is not allowed inside '%s' parentheses", statement);
        }
        else {
            err = cc_parse_expr(p, flags, &expr, CCQT_NONE);
            if(err) return err;
            if(!expr) return cc_error(p, first.loc, "expected condition inside '%s' parentheses", statement);
        }
        CcToken tok;
        err = cc_peek(p, &tok);
        if(err) goto expr_error;
        if(tok.type == CC_PUNCTUATOR && tok.punct.punct == ';'){
            err = cc_next_token(p, &tok);
            if(err) goto expr_error;
            err = cc_peek(p, &tok);
            if(err) goto expr_error;
        }
        else if(!(tok.type == CC_PUNCTUATOR && tok.punct.punct == ')')){
            err = cc_error(p, tok.loc, "expected ';' or ')' inside '%s' parentheses", statement);
            goto expr_error;
        }
        if(tok.type == CC_PUNCTUATOR && tok.punct.punct == ')'){
            if(declaration){
                CcVariable* last = decls.last_object;
                if(!last || !last->initializer)
                    return cc_error(p, tok.loc, "implicit condition requires an initialized object");
                expr = cc_make_expr(p, CC_EXPR_VARIABLE, last->loc, last->type, 0);
                if(!expr) return CC_OOM_ERROR;
                expr->is_lvalue = 1;
                expr->var = last;
            }
            err = cc_next_token(p, &tok);
            if(err) goto expr_error;
            *out = expr;
            return 0;
        }
        if(expr){
            err = cc_implicit_cast(p, expr, ccqt_basic(CCBT_void), &expr);
            if(err) goto expr_error;
            if(flags.static_header){
                CcExpr* value = NULL;
                err = cc_eval_expr(&(CcEvalCtx){.parser=p}, expr, &value);
                if(value) cc_release_expr(p, value);
                if(err == CC_NOT_CONSTANT_ERROR)
                    err = cc_error(p, expr->loc, "static header expression must be constexpr evaluable");
                cc_release_expr(p, expr);
                if(err) return err;
                continue;
            }
            CcStmtNode* stmt = cc_stmt_node(p, CC_STMT_EXPR, expr->loc, 0);
            if(!stmt){
                err = CC_OOM_ERROR;
                goto expr_error;
            }
            stmt->exprs[0] = expr;
            err = cc_sink_push(p, stmt);
            if(err) return err;
        }
        continue;
        expr_error:
        if(expr) cc_release_expr(p, expr);
        return err;
    }
}

static
int
cc_parse_selection_header(CcParser* p, CcParseFlags flags, const char* statement, CcExpr*_Nullable*_Nonnull out){
    if(!flags.static_header) return cc_parse_selection_header_inner(p, flags, statement, out);
    flags.vc = CC_CONSTEXPR_VALUE;
    flags.unevaluated = 1;
    CcStmtSink* sink = cc_push_stmt_sink(p);
    if(!sink) return CC_OOM_ERROR;
    int err = cc_parse_selection_header_inner(p, flags, statement, out);
    cc_pop_stmt_sink(p, sink);
    return err;
}

static
int
cc_parse_selection(CcParser* p, CcParseFlags flags, CcToken tok, CcControlLabel*_Nullable labels, CcStmtNode*_Nullable*_Nonnull out){
    int err = cc_push_scope(p);
    if(err) return err;
    CcStmtSink* sink = cc_push_stmt_sink(p);
    if(!sink){
        cc_pop_scope(p);
        return CC_OOM_ERROR;
    }
    CcExpr* cond = NULL;
    CcStmtNode* node = NULL;
    CcStmtNode* wrapper = NULL;
    _Bool is_switch = tok.kw.kw == CC_switch;
    err = cc_parse_selection_header(p, flags, is_switch ? "switch" : "if", &cond);
    if(err) goto cleanup;
    if(!is_switch){
        err = cc_require_scalar(p, &cond, tok.loc, "'if' condition");
        if(err) goto cleanup;
    }
    else {
        CcQualType st = cond->type;
        st = ccqt_enum_underlying(st);
        if(!ccqt_is_basic(st) || (!ccbt_is_integer(ccqt_basic_kind(st)) && ccqt_basic_kind(st) != CCBT__Type)){
            err = cc_type_error(p, tok.loc, cond->type, "switch requires integer or _Type expression");
            goto cleanup;
        }
        if(ccqt_basic_kind(st) == CCBT_int128 || ccqt_basic_kind(st) == CCBT_unsigned_int128){
            err = cc_error(p, tok.loc, "__int128 is not supported in switch");
            goto cleanup;
        }
        if(ccqt_basic_kind(st) != CCBT__Type){
            CcQualType promoted;
            err = cc_integer_promote_expr(p, cond, &promoted, tok.loc);
            if(err) goto cleanup;
            err = cc_implicit_cast(p, cond, promoted, &cond);
            if(err) goto cleanup;
        }
    }
    node = cc_stmt_node(p, is_switch ? CC_STMT_SWITCH : CC_STMT_IF, tok.loc, is_switch ? 1 : 2);
    if(!node){
        err = CC_OOM_ERROR;
        goto cleanup;
    }
    node->exprs[0] = cond;
    cond = NULL;
    if(is_switch){
        CcSwitchCtx ctx = {.type = node->exprs[0]->type, .defer_context = p->defer_context};
        CcSwitchCtx* prev_ctx = p->switch_ctx;
        p->switch_ctx = &ctx;
        p->loop_depth++;
        err = cc_parse_control_body(p, flags, 0, labels, &node->stmts[0]);
        p->loop_depth--;
        p->switch_ctx = prev_ctx;
        ma_cleanup(CcSwitchEntry)(&ctx.cases, cc_allocator(p));
        if(err) goto cleanup;
    }
    else {
        err = cc_parse_scoped_statement(p, flags, 0, &node->stmts[0]);
        if(err) goto cleanup;
        CcToken peek;
        err = cc_peek(p, &peek);
        if(err) goto cleanup;
        if(peek.type == CC_KEYWORD && peek.kw.kw == CC_else){
            err = cc_next_token(p, &peek);
            if(err) goto cleanup;
            err = cc_parse_scoped_statement(p, flags, 0, &node->stmts[1]);
            if(err) goto cleanup;
        }
    }
    err = cc_sink_push(p, node);
    node = NULL; // The sink owns it, including on failure.
    if(err) goto cleanup;
    err = cc_finalize_stmt_list(p, tok.loc, &sink->stmts, 1, &wrapper);
    if(err) goto cleanup;
    err = cc_stmt_scope_vars(p, wrapper);
    if(err) goto cleanup;
    *out = wrapper;
    wrapper = NULL;
    cleanup:
    if(cond) cc_release_expr(p, cond);
    cc_free_stmt_tree(p, node);
    cc_free_stmt_tree(p, wrapper);
    cc_pop_stmt_sink(p, sink);
    cc_pop_scope(p);
    return err;
}

static
int
cc_parse_statement(CcParser* p, CcParseFlags flags, CcControlLabel*_Nullable labels, CcStmtNode*_Nullable*_Nonnull out){
    flags = (CcParseFlags){.unevaluated=flags.unevaluated};
    uint64_t jump_targets = p->stmt_jump_targets;
    int err;
    CcToken tok;
    if(cc_is_c23_attribute_start(p)){
        CcAttributes stmt_attrs = {0};
        err = cc_parse_c23_attributes(p, flags, &stmt_attrs);
        if(err) return err;
        err = cc_peek(p, &tok);
        if(err) return err;
        if(tok.type == CC_PUNCTUATOR && tok.punct.punct == ';'){
            // bare [[attr]];
            err = cc_next_token(p, &tok); // consume ';'
            if(err) return err;
            CcStmtNode* node = cc_stmt_node(p, CC_STMT_NULL, tok.loc, 0);
            if(!node) return CC_OOM_ERROR;
            *out = node;
            return 0;
        }
        // Merge for the next declaration/statement
        p->attributes.bits |= stmt_attrs.bits;
        err = cc_merge_printf_formats(p, stmt_attrs.printf_formats, &p->attributes.printf_formats);
        if(err) return err;
        // Fall through
    }
    err = cc_peek(p, &tok);
    if(err) return err;
    if(p->loop_depth >= CC_CONTROL_LOOP_MAX && tok.type == CC_KEYWORD && (tok.kw.kw == CC_for || tok.kw.kw == CC_while || tok.kw.kw == CC_do || tok.kw.kw == CC_switch))
        return cc_error(p, tok.loc, "control nesting exceeds %d levels", CC_CONTROL_LOOP_MAX);
    _Bool runtime_boundary = tok.type == CC_KEYWORD
        &&(tok.kw.kw == CC_if
        || tok.kw.kw == CC_switch
        || tok.kw.kw == CC_for
        || tok.kw.kw == CC_while
        || tok.kw.kw == CC_do
        || tok.kw.kw == CC__Defer);
    // Include the whole statement, including conditions and both if branches.
    // A nested `static for` records this depth and can use its own controls.
    p->runtime_control_depth += runtime_boundary;
    CcDeferContext saved_defer_context = p->defer_context;
    _Bool compound = tok.type == CC_PUNCTUATOR && tok.punct.punct == '{';
    CcStmtNode* stmt = NULL;
    err = cc_parse_statement_inner(p, flags, labels, &stmt);
    CcDefer* active = p->defer_context.active;
    if(compound)
        p->defer_context = saved_defer_context;
    p->runtime_control_depth -= runtime_boundary;
    if(err) return err;
    if(stmt){
        if(compound)
            stmt->cleanup = (CcCleanupRange){active, saved_defer_context.active};
        stmt->has_jump_target |= p->stmt_jump_targets != jump_targets;
        cc_stmt_summarize_flow(stmt);
    }
    *out = stmt;
    return 0;
}

static
int
cc_parse_statement_inner(CcParser* p, CcParseFlags flags, CcControlLabel*_Nullable labels, CcStmtNode*_Nullable*_Nonnull out){
    CcStmtNode* label;
    int err = cc_parse_label(p, flags, &label);
    if(err) return err;
    if(label){
        if(label->kind == CC_STMT_LABEL){
            CcControlLabel binding = {.prev=labels, .name=label->label};
            err = cc_parse_statement(p, flags, &binding, &label->stmts[0]);
        }
        else
            err = cc_parse_statement(p, flags, labels, &label->stmts[0]);
        if(err){
            cc_free_stmt_tree(p, label);
            return err;
        }
        *out = label;
        return 0;
    }
    CcToken tok;
    err = cc_next_token(p, &tok);
    if(err) return err;
    switch(tok.type){
        case CC_EMBED:
            return cc_unreachable(p, tok.loc, "embed should be gone by now");
        case CC_EOF: {
            if(p->defer_context.deferred_body)
                return cc_error(p, tok.loc, "expected statement in deferred block");
            CcStmtNode* node = cc_stmt_node(p, CC_STMT_NULL, tok.loc, 0);
            if(!node) return CC_OOM_ERROR;
            *out = node;
            return 0;
        }
        case CC_KEYWORD:
            switch(tok.kw.kw){
                case CC__Defer: {
                    CcDefer* d = Allocator_zalloc(cc_allocator(p), 1, sizeof *d, _Alignof(CcDefer));
                    if(!d) return CC_OOM_ERROR;
                    CcStmtNode* node = cc_stmt_node(p, CC_STMT_DEFER, tok.loc, 1);
                    if(!node) return CC_OOM_ERROR;
                    CcDeferContext saved = p->defer_context;
                    uint64_t targets = p->stmt_jump_targets;
                    CcToken first, next;
                    err = cc_peek(p, &first);
                    if(err) goto defer_end;
                    if(first.type == CC_EOF){
                        err = cc_error(p, tok.loc, "expected unlabeled statement after _Defer");
                        goto defer_end;
                    }
                    if(first.type == CC_IDENTIFIER){
                        err = cc_next_token(p, &first);
                        if(err) goto defer_end;
                        err = cc_peek(p, &next);
                        if(err) goto defer_end;
                        if(next.type == CC_PUNCTUATOR && next.punct.punct == ':'){
                            err = cc_error(p, first.loc, "expected unlabeled statement after _Defer");
                            goto defer_end;
                        }
                        err = cc_unget(p, &first);
                        if(err) goto defer_end;
                    }
                    if(first.type == CC_KEYWORD && (first.kw.kw == CC_case || first.kw.kw == CC_default)){
                        err = cc_error(p, first.loc, "expected unlabeled statement after _Defer");
                        goto defer_end;
                    }
                    p->defer_context = (CcDeferContext){.deferred_body=d};
                    err = cc_push_scope(p);
                    if(err) goto defer_end;
                    err = cc_parse_scoped_statement(p, flags, 1, &node->stmts[0]);
                    if(err) goto defer_scope_end;
                    if(p->static_goto_target){
                        err = cc_static_goto_missing(p);
                        if(err) goto defer_scope_end;
                    }
                    err = cc_stmt_scope_vars(p, node->stmts[0]);

                    defer_scope_end:
                    cc_pop_scope(p);
                    defer_end:
                    // These labels cannot be entered from the enclosing code.
                    p->stmt_jump_targets = targets;
                    p->defer_context = saved;
                    if(err){
                        cc_free_stmt_tree(p, node);
                        return err;
                    }
                    d->prev = saved.active;
                    d->body = node->stmts[0];
                    p->defer_context.active = d;
                    *out = node;
                    return 0;
                }
                case CC_static: {
                    err = cc_unget(p, &tok);
                    if(err) return err;
                    CcStmtSink* sink = cc_push_stmt_sink(p);
                    if(!sink) return CC_OOM_ERROR;
                    err = cc_parse_one(p, flags);
                    if(err) goto static_end;
                    err = cc_finalize_stmt_list(p, tok.loc, &sink->stmts, 0, out);
                    static_end:
                    cc_pop_stmt_sink(p, sink);
                    return err;
                }
                case CC_for: {
                    // for(init; cond; inc) body
                    err = cc_expect_punct(p, '(');
                    if(err) return err;
                    // for introduces a new scope for init declarations
                    err = cc_push_scope(p);
                    if(err) return err;
                    CcStmtNode*_Nullable init = NULL;
                    CcStmtNode*_Nullable body = NULL;
                    CcExpr* _Null_unspecified cond_expr = NULL;
                    CcExpr* _Null_unspecified inc_expr = NULL;
                    // Parse init
                    {
                        CcToken peek;
                        err = cc_peek(p, &peek);
                        if(err) goto for_end;
                        if(peek.type == CC_PUNCTUATOR && peek.punct.punct == ';'){
                            // empty init
                            cc_next_token(p, &peek); // consume ';'
                        }
                        else {
                            CcStmtSink* sink = cc_push_stmt_sink(p);
                            if(!sink){
                                err = CC_OOM_ERROR;
                                goto for_end;
                            }
                            CcDeclBase b = {0};
                            err = cc_parse_declaration_specifier(p, flags, &b);
                            if(err) goto init_done;
                            if(b.spec.bits || b.type.bits){
                                // Declaration init, including multiple declarators.
                                err = cc_resolve_specifiers(p, &b);
                                if(err) goto init_done;
                                CcParsedDecls decls;
                                err = cc_parse_decls(p, flags, &b, &decls);
                                if(err) goto init_done;
                                if(decls.function_definition){
                                    err = cc_error(p, decls.function_definition->loc, "function definition is not allowed in a for initializer");
                                    goto init_done;
                                }
                                err = cc_expect_punct(p, CC_semi);
                                if(err) goto init_done;
                            }
                            else {
                                // Expression init
                                CcExpr* init_expr;
                                err = cc_parse_expr(p, flags, &init_expr, CCQT_NONE);
                                if(err) goto init_done;
                                err = cc_implicit_cast(p, init_expr, ccqt_basic(CCBT_void), &init_expr);
                                if(err){
                                    cc_release_expr(p, init_expr);
                                    goto init_done;
                                }
                                err = cc_expect_punct(p, ';');
                                if(err){
                                    cc_release_expr(p, init_expr);
                                    goto init_done;
                                }
                                CcStmtNode* e = cc_stmt_node(p, CC_STMT_EXPR, tok.loc, 0);
                                if(!e){
                                    cc_release_expr(p, init_expr);
                                    err = CC_OOM_ERROR;
                                    goto init_done;
                                }
                                e->exprs[0] = init_expr;
                                err = cc_sink_push(p, e);
                                if(err) goto init_done;
                            }
                            err = cc_finalize_stmt_list(p, tok.loc, &sink->stmts, 0, &init);
                            init_done:
                            cc_pop_stmt_sink(p, sink);
                            if(err) goto for_end;
                        }
                    }
                    // Parse cond
                    {
                        CcToken peek;
                        err = cc_peek(p, &peek);
                        if(err) goto for_end;
                        if(!(peek.type == CC_PUNCTUATOR && peek.punct.punct == ';')){
                            err = cc_parse_for_condition(p, flags, tok.loc, &cond_expr);
                        }
                        else {
                            err = cc_next_token(p, &peek); // consume ';'
                        }
                        if(err) goto for_end;
                    }
                    // Parse inc
                    {
                        CcToken peek;
                        err = cc_peek(p, &peek);
                        if(err) goto for_end;
                        if(!(peek.type == CC_PUNCTUATOR && peek.punct.punct == ')')){
                            err = cc_parse_expr(p, flags, &inc_expr, CCQT_NONE);
                            if(err) goto for_end;
                            err = cc_implicit_cast(p, inc_expr, ccqt_basic(CCBT_void), &inc_expr);
                            if(err) goto for_end;
                        }
                        err = cc_expect_punct(p, ')');
                        if(err) goto for_end;
                    }
                    // Parse body
                    err = cc_parse_local_methods(p);
                    if(err) goto for_end;
                    p->loop_depth++;
                    err = cc_parse_control_body(p, flags, 1, labels, &body);
                    p->loop_depth--;
                    if(err) goto for_end;
                    {
                        CcStmtNode* node = cc_stmt_node(p, CC_STMT_FOR, tok.loc, 2);
                        if(!node){
                            err = CC_OOM_ERROR;
                            goto for_end;
                        }
                        err = cc_stmt_scope_vars(p, node);
                        if(err){
                            cc_free_stmt_tree(p, node);
                            goto for_end;
                        }
                        node->exprs[0] = cond_expr;
                        node->exprs[1] = inc_expr;
                        node->stmts[0] = init;
                        node->stmts[1] = body;
                        *out = node;
                    }
                    for_end:
                    cc_pop_scope(p);
                    if(err){
                        cc_free_stmt_tree(p, init);
                        cc_free_stmt_tree(p, body);
                        if(cond_expr) cc_release_expr(p, cond_expr);
                        if(inc_expr) cc_release_expr(p, inc_expr);
                    }
                    return err;
                }
                case CC_while: {
                    // while(cond) body
                    err = cc_expect_punct(p, '(');
                    if(err) return err;
                    CcExpr* cond;
                    err = cc_parse_expr(p, flags, &cond, CCQT_NONE);
                    if(err) return err;
                    err = cc_require_scalar(p, &cond, tok.loc, "'while' condition");
                    if(err) goto while_condition_error;
                    err = cc_expect_punct(p, ')');
                    if(err) goto while_condition_error;
                    CcStmtNode* node = cc_stmt_node(p, CC_STMT_WHILE, tok.loc, 1);
                    if(!node){
                        err = CC_OOM_ERROR;
                        goto while_condition_error;
                    }
                    node->exprs[0] = cond;
                    CcStmtNode*_Nullable body = NULL;
                    p->loop_depth++;
                    err = cc_parse_control_body(p, flags, 1, labels, &body);
                    p->loop_depth--;
                    if(err){
                        cc_free_stmt_tree(p, node);
                        return err;
                    }
                    node->stmts[0] = body;
                    *out = node;
                    return 0;
                    while_condition_error:
                    cc_release_expr(p, cond);
                    return err;
                }
                case CC_do: {
                    // do body while(cond);
                    CcStmtNode*_Nullable body = NULL;
                    p->loop_depth++;
                    err = cc_parse_control_body(p, flags, 1, labels, &body);
                    p->loop_depth--;
                    if(err) return err;
                    {
                        CcToken wtok;
                        err = cc_next_token(p, &wtok);
                        if(err){
                            cc_free_stmt_tree(p, body);
                            return err;
                        }
                        if(wtok.type != CC_KEYWORD || wtok.kw.kw != CC_while){
                            cc_free_stmt_tree(p, body);
                            return cc_error(p, wtok.loc, "Expected 'while' after do body");
                        }
                    }
                    err = cc_expect_punct(p, '(');
                    if(err){
                        cc_free_stmt_tree(p, body);
                        return err;
                    }
                    CcExpr* cond;
                    err = cc_parse_expr(p, flags, &cond, CCQT_NONE);
                    if(err){
                        cc_free_stmt_tree(p, body);
                        return err;
                    }
                    err = cc_require_scalar(p, &cond, tok.loc, "'do-while' condition");
                    if(err) goto do_condition_error;
                    err = cc_expect_punct(p, ')');
                    if(err) goto do_condition_error;
                    err = cc_expect_punct(p, ';');
                    if(err) goto do_condition_error;
                    CcStmtNode* node = cc_stmt_node(p, CC_STMT_DOWHILE, tok.loc, 1);
                    if(!node){
                        err = CC_OOM_ERROR;
                        goto do_condition_error;
                    }
                    node->exprs[0] = cond;
                    node->stmts[0] = body;
                    *out = node;
                    return 0;
                    do_condition_error:
                    cc_release_expr(p, cond);
                    cc_free_stmt_tree(p, body);
                    return err;
                }
                case CC_if:
                case CC_switch:
                    return cc_parse_selection(p, flags, tok, labels, out);
                case CC_break:
                case CC_continue: {
                    _Bool is_continue = tok.kw.kw == CC_continue;
                    CcToken operand;
                    err = cc_peek(p, &operand);
                    if(err) return err;
                    Atom name = NULL;
                    uint32_t requested = 0;
                    if(operand.type == CC_IDENTIFIER){
                        // Only an identifier followed by ';' names a label.
                        err = cc_next_token(p, &operand);
                        if(err) return err;
                        CcToken next;
                        err = cc_peek(p, &next);
                        if(err) return err;
                        if(next.type == CC_PUNCTUATOR && next.punct.punct == ';')
                            name = operand.ident.ident;
                        else {
                            err = cc_unget(p, &operand);
                            if(err) return err;
                        }
                    }
                    if(!name && !(operand.type == CC_PUNCTUATOR && operand.punct.punct == ';')){
                        CcExpr* depth;
                        err = cc_parse_expr(p, (CcParseFlags){.vc=CC_CONSTEXPR_VALUE, .unevaluated=1}, &depth, CCQT_NONE);
                        if(err) return err;
                        int64_t value = 0;
                        err = ccqt_is_integer(depth->type)
                            ? cc_eval_integer(&(CcEvalCtx){.parser=p}, depth, &value)
                            : CC_NOT_CONSTANT_ERROR;
                        cc_release_expr(p, depth);
                        if(err && err != CC_NOT_CONSTANT_ERROR && err != CC_OVERFLOW_ERROR)
                            return err;
                        if(err || value < 1 || value > CC_CONTROL_LOOP_MAX)
                            return cc_error(p, tok.loc, "jump level must be a constant integer expression from 1 to %d", CC_CONTROL_LOOP_MAX);
                        requested = (uint32_t)value;
                    }
                    CcControl* target = p->control;
                    uint32_t levels = 1;
                    for(; target; target = target->prev, ++levels){
                        if(name){
                            CcControlLabel* binding = target->labels;
                            for(; binding; binding = binding->prev)
                                if(binding->name == name) break;
                            if(binding) break;
                        }
                        else if(requested ? levels == requested : (!is_continue || target->loop))
                            break;
                    }
                    if(!target || (is_continue && !target->loop)){
                        if(name)
                            return cc_error(p, tok.loc, "'%s' target '%.*s' is not an enclosing %s",
                                is_continue ? "continue" : "break", name->length, name->data,
                                is_continue ? "loop" : "loop or switch");
                        if(requested){
                            if(!target)
                                return cc_error(p, tok.loc, "jump level exceeds enclosing control depth");
                            return cc_error(p, tok.loc, "'continue' target is a switch");
                        }
                        return cc_error(p, tok.loc, "%s", is_continue ?
                                "'continue' statement not in loop statement"
                                : "'break' statement not in loop or switch statement");
                    }
                    err = cc_check_transfer(p, tok.loc, p->defer_context, target->context);
                    if(err) return err;
                    err = cc_expect_punct(p, ';');
                    if(err) return err;
                    CcStmtNode* node = cc_stmt_node(p, is_continue ? CC_STMT_CONTINUE : CC_STMT_BREAK, tok.loc, 0);
                    if(!node) return CC_OOM_ERROR;
                    node->jump_levels = levels;
                    node->cleanup = (CcCleanupRange){p->defer_context.active, target->context.active};
                    *out = node;
                    return 0;
                }
                case CC_case:
                case CC_default:
                    return cc_unreachable(p, tok.loc, "label was not consumed");
                case CC_return: {
                    if(p->defer_context.deferred_body)
                        return cc_error(p, tok.loc, "return exits a deferred block");
                    CcExpr* _Null_unspecified ret_expr = NULL;
                    CcToken peek;
                    err = cc_peek(p, &peek);
                    if(err) return err;
                    CcQualType ret_type = ccqt_basic(CCBT_int);
                    if(p->current_func)
                        ret_type = p->current_func->type->return_type;
                    if(!(peek.type == CC_PUNCTUATOR && peek.punct.punct == ';')){
                        err = cc_parse_expr(p, flags, &ret_expr, ret_type);
                        if(err) return err;
                        err = cc_implicit_cast(p, ret_expr, ret_type, &ret_expr); // technically this casts to void if returning from void func, but that's an ok extension
                        if(err) return err;
                    }
                    else if(!(ccqt_is_basic(ret_type) && ccqt_basic_kind(ret_type) == CCBT_void)){
                        return cc_error(p, peek.loc, "Returning void from non-void function");
                    }
                    err = cc_expect_punct(p, ';');
                    if(err){
                        if(ret_expr) cc_release_expr(p, ret_expr);
                        return err;
                    }
                    CcStmtNode* node = cc_stmt_node(p, CC_STMT_RETURN, tok.loc, 0);
                    if(!node){
                        if(ret_expr) cc_release_expr(p, ret_expr);
                        return CC_OOM_ERROR;
                    }
                    node->exprs[0] = ret_expr;
                    node->return_.first = p->defer_context.active;
                    *out = node;
                    return 0;
                }
                case CC_goto: {
                    CcToken label_tok;
                    err = cc_next_token(p, &label_tok);
                    if(err) return err;
                    if(label_tok.type != CC_IDENTIFIER)
                        return cc_error(p, label_tok.loc, "Expected identifier after 'goto'");
                    err = cc_expect_punct(p, ';');
                    if(err) return err;
                    CcStmtNode* node = cc_stmt_node(p, CC_STMT_GOTO, tok.loc, 0);
                    if(!node) return CC_OOM_ERROR;
                    node->label = label_tok.ident.ident;
                    if(p->defer_context.active){
                        node->goto_.cleanup = Allocator_zalloc(cc_allocator(p), 1, sizeof *node->goto_.cleanup, _Alignof(CcCleanupRange));
                        if(!node->goto_.cleanup){
                            cc_free_stmt_tree(p, node);
                            return CC_OOM_ERROR;
                        }
                    }
                    err = ma_push(CcLabelRef)(&cc_label_ctx(p)->gotos, cc_allocator(p), (CcLabelRef){
                        .label=node->label,
                        .loc=node->loc,
                        .defer_context=p->defer_context,
                        .cleanup=node->goto_.cleanup
                    });
                    if(err){
                        cc_free_stmt_tree(p, node);
                        return CC_OOM_ERROR;
                    }
                    *out = node;
                    return 0;
                }
                case CC_sizeof:
                case CC_true:
                case CC_alignof:
                case CC_nullptr:
                case CC__Generic:
                case CC__Countof:
                case CC_false:
                case CC_static_assert:
                    goto expression_statement;
                case CC_int:
                case CC_asm:
                case CC_long:
                case CC_char:
                case CC_auto:
                case CC_bool:
                case CC_else:
                case CC_enum:
                case CC_void:
                case CC_float:
                case CC_const:
                case CC_short:
                case CC_union:
                case CC_double:
                case CC_extern:
                case CC_inline:
                case CC_signed:
                case CC_struct:
                case CC_typeof:
                case CC_alignas:
                case CC_typedef:
                case CC__Atomic:
                case CC__BitInt:
                case CC__Complex:
                case CC_register:
                case CC_restrict:
                case CC_unsigned:
                case CC_volatile:
                case CC___fp16:
                case CC___bf16:
                case CC__Float16:
                case CC__Float32:
                case CC__Float64:
                case CC_constexpr:
                case CC__Float128:
                case CC__Float32x:
                case CC__Float64x:
                case CC__Imaginary:
                case CC__Noreturn:
                case CC__Decimal32:
                case CC__Decimal64:
                case CC__Decimal128:
                case CC___auto_type:
                case CC___int128:
                case CC_thread_local:
                case CC___forceinline:
                case CC_typeof_unqual:
                case CC__Any:
                case CC__Type:
                case CC__Self:
                    return cc_error(p, tok.loc, "Unexpected keyword in this position");
                case CC___attribute__: {
                    // __attribute__((fallthrough)); etc.
                    err = cc_unget(p, &tok);
                    if(err) return err;
                    CcAttributes stmt_attrs = {0};
                    err = cc_parse_attributes(p, flags, &stmt_attrs);
                    if(err) return err;
                    err = cc_expect_punct(p, ';');
                    if(err) return err;
                    CcStmtNode* node = cc_stmt_node(p, CC_STMT_NULL, tok.loc, 0);
                    if(!node) return CC_OOM_ERROR;
                    *out = node;
                    return 0;
                }
                case CC___declspec:
                    return cc_error(p, tok.loc, "Unexpected keyword in this position");
            }
            break;
        case CC_PUNCTUATOR:
            if(tok.punct.punct == '{'){
                CcStmtSink* sink = cc_push_stmt_sink(p);
                if(!sink) return CC_OOM_ERROR;
                err = cc_push_scope(p);
                if(err){
                    cc_pop_stmt_sink(p, sink);
                    return err;
                }
                CcStmtNode* node = NULL;
                for(;;){
                    CcToken peek;
                    err = cc_peek(p, &peek);
                    if(err) goto end_block;
                    if(peek.type == CC_EOF){
                        err = cc_error(p, tok.loc, "Unterminated block");
                        goto end_block;
                    }
                    if(peek.type == CC_PUNCTUATOR && peek.punct.punct == '}'){
                        if(p->static_goto_target){
                            err = cc_static_goto_missing(p);
                            goto end_block;
                        }
                        cc_next_token(p, &peek); // consume '}'
                        break;
                    }
                    err = cc_parse_one(p, flags);
                    if(err) goto end_block;
                }
                err = cc_finalize_stmt_list(p, tok.loc, &sink->stmts, 1, &node);
                if(err) goto end_block;
                err = cc_stmt_scope_vars(p, node);
                if(err) goto end_block;
                *out = node;
                node = NULL;
                end_block:
                cc_free_stmt_tree(p, node);
                cc_pop_scope(p);
                cc_pop_stmt_sink(p, sink);
                return err;
            }
            if(tok.punct.punct == ';'){
                CcStmtNode* node = cc_stmt_node(p, CC_STMT_NULL, tok.loc, 0);
                if(!node) return CC_OOM_ERROR;
                *out = node;
                return 0;
            }
            goto expression_statement;
        case CC_IDENTIFIER:
        case CC_CONSTANT:
        case CC_STRING_LITERAL:{
            expression_statement:;
            err = cc_unget(p, &tok);
            if(err) return err;
            CcExpr* expr;
            err = cc_parse_expr(p, flags, &expr, CCQT_NONE);
            if(err) return err;
            err = cc_implicit_cast(p, expr, ccqt_basic(CCBT_void), &expr);
            if(err){
                cc_release_expr(p, expr);
                return err;
            }
            err = cc_expect_punct(p, ';');
            if(err){
                cc_release_expr(p, expr);
                return err;
            }
            CcStmtNode* node = cc_stmt_node(p, CC_STMT_EXPR, tok.loc, 0);
            if(!node){
                cc_release_expr(p, expr);
                return CC_OOM_ERROR;
            }
            node->exprs[0] = expr;
            *out = node;
            return 0;
        }
    }
    return CC_UNREACHABLE_ERROR;
}

static
int
cc_check_decl_type_qualifiers(CcParser* p, CcQualType type, CcSpecifier spec, SrcLoc loc){
    const char* failure;
    if(spec.sp_restrict && !cc_restrict_qualifiable(type))
        return cc_type_error(p, loc, type, "restrict requires a pointer type");
    if(type.is_atomic && cc_check_atomic_qualifier(type, &failure))
        return cc_type_error(p, loc, type, "%s", failure);
    return 0;
}

static
int
cc_resolve_specifiers(CcParser* p, CcDeclBase* declbase){
    CcDeclBase b = *declbase;
    if(!b.spec.bits && !b.type.bits) return cc_unreachable(p, declbase->loc, "Resolving specifier with no spec and no type");
    if(!b.spec.sp_typebits && !b.type.bits && b.spec.sp_typedef)
        return cc_unreachable(p, declbase->loc, "typedef with no type in resolve_specifiers");
    if(!b.spec.sp_typebits && !b.type.bits && !b.spec.sp_complex)
        b.spec.sp_infer_type = 1;
    if(b.spec.sp___auto_type)
        b.spec.sp_infer_type = 1;
    if(b.spec.sp_complex && !b.type.bits && !b.spec.sp_typebits)
        b.type = ccqt_basic(CCBT_double);
    if(!b.type.bits && !b.spec.sp_infer_type){
        // construct type from keywords
        if(b.spec.sp_char){
            b.type = ccqt_basic(b.spec.sp_signed? CCBT_signed_char: b.spec.sp_unsigned? CCBT_unsigned_char : CCBT_char);
        }
        else if(b.spec.sp_int128){
            b.type = ccqt_basic(b.spec.sp_unsigned? CCBT_unsigned_int128 : CCBT_int128);
        }
        else {
            CcBasicTypeKind k = CCBT_int;
            if(b.spec.sp_short)
                k -= 2;
            if(b.spec.sp_unsigned)
                k++;
            k += b.spec.sp_long * 2;
            b.type = ccqt_basic(k);
        }
    }
    if(ccqt_is_basic(b.type) && ccqt_basic_kind(b.type) == CCBT_double && b.spec.sp_long)
        b.type.basic.kind = CCBT_long_double;
    if(b.spec.sp_complex){
        if(!ccqt_is_basic(b.type) || b.spec.sp_infer_type
            || ccbt_complex_kind(ccqt_basic_kind(b.type)) == CCBT_INVALID)
            return cc_type_error(p, b.loc, b.type, "invalid scalar type for _Complex");
        b.type.basic.kind = ccbt_complex_kind(ccqt_basic_kind(b.type));
    }
    if(b.spec.sp_const) b.type.is_const = 1;
    if(b.spec.sp_volatile) b.type.is_volatile = 1;
    if(b.spec.sp_atomic) b.type.is_atomic = 1;
    if(!b.spec.sp_infer_type){
        int err = cc_check_decl_type_qualifiers(p, b.type, b.spec, b.loc);
        if(err) return err;
    }
    *declbase = b;
    return 0;
}

static
int
cc_new_vla_bound(CcParser* p, SrcLoc loc, CcQualType type, CcVariable*_Nonnull*_Nonnull out){
    CcVariable* var = Allocator_zalloc(cc_allocator(p), 1, sizeof *var, _Alignof(CcVariable));
    if(!var) return CC_OOM_ERROR;
    *var = (CcVariable){
        .name=nil_atom,
        .loc=loc,
        .type=type,
        .vla_bound=1,
        .automatic=p->current_func || p->current != &p->module->scope
    };
    CcScope* scope = p->function_scope ? p->function_scope : cc_scope_declarations(p->current);
    int err = cc_scope_insert_var(cc_allocator(p), scope, nil_atom, var);
    if(err) return err;
    *out = var;
    return 0;
}

static
int
cc_capture_vla_bound(CcParser* p, CcParseFlags flags, CcExpr* dim, CcExpr*_Nonnull*_Nonnull out){
    if(!ccqt_is_integer(dim->type))
        return cc_type_error(p, dim->loc, dim->type, "array dimension must have integer type");
    CcQualType type = ccqt_basic(cc_target(p)->size_type);
    CcExpr* init;
    int err = cc_implicit_cast(p, dim, type, &init);
    if(err) return err;
    uint32_t width;
    err = cc_sizeof_as_uint(p, dim->type, dim->loc, &width);
    if(err) return err;
    if(!ccqt_is_unsigned(dim->type, !cc_target(p)->char_is_signed)
        || width > cc_target(p)->sizeof_[cc_target(p)->size_type])
        init->cast.checked_bound = 1;
    if(flags.unevaluated){
        *out = init;
        return 0;
    }
    CcVariable* var;
    err = cc_new_vla_bound(p, dim->loc, type, &var);
    if(err) return err;
    CcExpr* ref = cc_make_expr(p, CC_EXPR_VARIABLE, dim->loc, type, 0);
    CcExpr* value = cc_make_expr(p, CC_EXPR_VARIABLE, dim->loc, type, 0);
    if(!ref || !value) return CC_OOM_ERROR;
    ref->var = value->var = var;
    ref->is_lvalue = value->is_lvalue = 1;
    CcExpr* assign = cc_binary_expr(p, CC_EXPR_ASSIGN, dim->loc, type, ref, init);
    if(!assign) return CC_OOM_ERROR;
    CcStmtNode* stmt = cc_stmt_node(p, CC_STMT_EXPR, dim->loc, 0);
    if(!stmt) return CC_OOM_ERROR;
    stmt->expr = assign;
    err = cc_sink_push(p, stmt);
    if(err) return err;
    *out = value;
    return 0;
}

// Recursive declarator parser using double-pointer technique.
// out_head/out_tail thread the type chain: after return, set
// *out_tail = base_type to complete the type.
// This is kind of crazy but based on the technique in the last section of
// https://mkukri.xyz/2022/05/01/declarators.html
static
int
cc_parse_declarator_inner(CcParser* p, CcParseFlags flags, CcQualType* out_head, CcQualType*_Nonnull*_Nonnull out_tail, Atom _Nullable * _Nullable out_name, SrcLoc* _Nullable out_name_loc, CcParsedParams *_Nullable out_param_names){
    int err = 0;
    CcToken tok;
    err = cc_next_token(p, &tok);
    if(err) return err;
    if(tok.type == CC_PUNCTUATOR && (tok.punct.punct == '*' || tok.punct.punct == '^')){
        // Pointer or block-pointer (^) declarator: parse qualifiers, recurse, then splice.
        CcTypeKind ptr_kind = tok.punct.punct == '^' ? CC_BLOCK_POINTER : CC_POINTER;
        _Bool const_ = 0, volatile_ = 0, atomic_ = 0;
        for(;;){
            err = cc_parse_attributes(p, flags, &p->attributes);
            if(err) return err;
            err = cc_next_token(p, &tok);
            if(err) return err;
            if(tok.type == CC_KEYWORD){
                switch(tok.kw.kw){
                    case CC_restrict: continue;
                    case CC_const:    const_    = 1; continue;
                    case CC_volatile: volatile_ = 1; continue;
                    case CC__Atomic:  atomic_   = 1; continue;
                    default: break;
                }
            }
            err = cc_unget(p, &tok);
            if(err) return err;
            break;
        }
        err = cc_parse_declarator_inner(p, flags, out_head, out_tail, out_name, out_name_loc, out_param_names);
        if(err) return err;
        CcPointer* ptr = Allocator_zalloc(cc_scratch_allocator(p), 1, sizeof *ptr, _Alignof(CcPointer));
        if(!ptr) return CC_OOM_ERROR;
        ptr->kind = ptr_kind;
        ptr->pointee = **out_tail;
        CcQualType qt = {.bits = (uintptr_t)ptr};
        qt.is_const = const_;
        qt.is_volatile = volatile_;
        qt.is_atomic = atomic_;
        **out_tail = qt;
        *out_tail = &ptr->pointee;
        return 0;
    }
    if(tok.type == CC_PUNCTUATOR && tok.punct.punct == '('){
        // Disambiguate: grouped declarator vs function parameter list.
        // '(' followed by '*', '^', '(' or non-typedef identifier -> grouped declarator.
        // '(' followed by type keyword, ')', '...', or typedef name -> function params.
        CcToken peek;
        err = cc_peek(p, &peek);
        if(err) return err;
        _Bool grouped = peek.type == CC_PUNCTUATOR && (peek.punct.punct == '*' || peek.punct.punct == '^' || peek.punct.punct == '(');
        if(!grouped && peek.type == CC_IDENTIFIER){
            // Identifier after '(' — grouped declarator unless it's a typedef
            // (not shadowed by a var/func in a closer scope).
            CcSymbol sym;
            grouped = !cc_scope_lookup_symbol(p->current, peek.ident.ident, CC_SCOPE_WALK_CHAIN, &sym) || sym.kind != CC_SYM_TYPEDEF;
        }
        if(grouped){
            err = cc_parse_declarator_inner(p, flags, out_head, out_tail, out_name, out_name_loc, out_param_names);
            if(err) return err;
            err = cc_expect_punct(p, CC_rparen);
            if(err) return err;
        }
        else {
            // Put back '(' so the postfix loop handles it as function params.
            err = cc_unget(p, &tok);
            if(err) return err;
        }
    }
    else if(tok.type == CC_IDENTIFIER){
        if(!out_name)
            return cc_error(p, tok.loc, "identifier is not allowed in a type name");
        if(out_name)
            *out_name = tok.ident.ident;
        if(out_name_loc)
            *out_name_loc = tok.loc;
    }
    else {
        // Abstract declarator or end of declarator
        err = cc_unget(p, &tok);
        if(err) return err;
    }
    // Postfix: arrays and function params
    for(;;){
        err = cc_next_token(p, &tok);
        if(err) return err;
        if(tok.type == CC_PUNCTUATOR && tok.punct.punct == '['){
            // Disambiguate: '[' '[' is C23 attribute, not array
            CcToken peek;
            err = cc_peek(p, &peek);
            if(err) return err;
            if(peek.type == CC_PUNCTUATOR && peek.punct.punct == '['){
                err = cc_unget(p, &tok);
                if(err) return err;
                err = cc_parse_c23_attributes(p, flags, &p->attributes);
                if(err) return err;
                continue;
            }
            if(peek.type == CC_PUNCTUATOR && peek.punct.punct == ':'){
                err = cc_next_token(p, &peek);
                if(err) return err;
                err = cc_expect_punct(p, ']');
                if(err) return err;
                CcSlice* s = Allocator_zalloc(cc_scratch_allocator(p), 1, sizeof *s, _Alignof(CcSlice));
                if(!s) return CC_OOM_ERROR;
                s->kind = CC_SLICE;
                s->pointee = **out_tail;
                **out_tail = (CcQualType){.bits = (uintptr_t)s};
                *out_tail = &s->pointee;
                continue;
            }
            CcArray* arr = Allocator_zalloc(cc_scratch_allocator(p), 1, sizeof *arr, _Alignof(CcArray));
            if(!arr) return CC_OOM_ERROR;
            arr->kind = CC_ARRAY;
            arr->declarator_loc = tok.loc;
            // Parse optional qualifiers and 'static' inside [] (C99 §6.7.6.2)
            // e.g. int a[static restrict 10], int a[const], int a[restrict]
            for(;;){
                err = cc_peek(p, &peek);
                if(err) return err;
                if(peek.type != CC_KEYWORD) break;
                if(peek.kw.kw == CC_static){
                    if(!flags.parameter_type || *out_tail != out_head)
                        return cc_error(p, peek.loc, "static array bound requires an outermost parameter array");
                    if(arr->is_static)
                        return cc_error(p, peek.loc, "duplicate static in array declarator");
                    cc_next_token(p, &peek);
                    arr->is_static = 1;
                }
                else if(peek.kw.kw == CC_const
                     || peek.kw.kw == CC_volatile
                     || peek.kw.kw == CC__Atomic
                     || peek.kw.kw == CC_restrict){
                    if(!flags.parameter_type || *out_tail != out_head)
                        return cc_error(p, peek.loc, "array bracket qualifiers require an outermost parameter array");
                    cc_next_token(p, &peek);
                    CcQualType quals = {0};
                    quals.is_const = peek.kw.kw == CC_const;
                    quals.is_volatile = peek.kw.kw == CC_volatile;
                    quals.is_atomic = peek.kw.kw == CC__Atomic;
                    arr->parameter_quals |= quals.quals;
                }
                else break;
            }
            err = cc_peek(p, &peek);
            if(err) return err;
            if(peek.type == CC_PUNCTUATOR && peek.punct.punct == ']'){
                // []
                if(arr->is_static)
                    return cc_error(p, peek.loc, "static array parameter requires a bound");
                arr->is_incomplete = 1;
            }
            else if(peek.type == CC_PUNCTUATOR && peek.punct.punct == '*'){
                err = cc_next_token(p, &peek);
                if(err) return err;
                CcToken after_star;
                err = cc_peek(p, &after_star);
                if(err) return err;
                if(after_star.type != CC_PUNCTUATOR || after_star.punct.punct != ']'){
                    err = cc_unget(p, &peek);
                    if(err) return err;
                    goto array_dimension;
                }
                if(arr->is_static)
                    return cc_error(p, peek.loc, "static array parameter requires a bound");
                if(!flags.parameter_type && !flags.generic_association)
                    return cc_error(p, peek.loc, "[*] requires function prototype scope");
                CcVla* vla = Allocator_zalloc(cc_allocator(p), 1, sizeof *vla, _Alignof(CcVla));
                if(!vla) return CC_OOM_ERROR;
                vla->kind = CC_VLA;
                vla->unspecified = 1;
                vla->parameter_quals = arr->parameter_quals;
                vla->loc = tok.loc;
                CcExpr* bound = cc_uint64_expr(p, peek.loc, ccqt_basic(cc_target(p)->size_type), 1);
                if(!bound) return CC_OOM_ERROR;
                vla->bound = bound;
                err = cc_expect_punct(p, ']');
                if(err) return err;
                vla->element = **out_tail;
                **out_tail = (CcQualType){.bits=(uintptr_t)vla};
                *out_tail = &vla->element;
                continue;
            }
            else {
                array_dimension:;
                CcExpr* dim = NULL;
                CcParseFlags dim_flags = flags;
                dim_flags.vc = CC_RUNTIME_VALUE;
                dim_flags.static_rebind = 0;
                dim_flags.parameter_type = 0;
                dim_flags.generic_association = 0;
                err = cc_parse_assignment_expr(p, dim_flags, &dim, CCQT_NONE);
                if(err) return err;
                if(!dim) return cc_error(p, tok.loc, "Expected array dimension");
                if(!ccqt_is_integer(dim->type))
                    return cc_type_error(p, tok.loc, dim->type, "array dimension must have integer type");
                int64_t length;
                err = cc_eval_integer(&(CcEvalCtx){p}, dim, &length);
                if(err && err != CC_NOT_CONSTANT_ERROR) return err;
                if(err){
                    CcExpr* constant;
                    int constant_err = cc_eval_expr(&(CcEvalCtx){p}, dim, &constant);
                    if(!constant_err){
                        cc_release_expr(p, constant);
                        return cc_error(p, tok.loc, "array dimension exceeds supported range");
                    }
                    if(constant_err != CC_NOT_CONSTANT_ERROR) return constant_err;
                    CcVla* vla = Allocator_zalloc(cc_allocator(p), 1, sizeof *vla, _Alignof(CcVla));
                    if(!vla) return CC_OOM_ERROR;
                    vla->kind = CC_VLA;
                    vla->parameter_quals = arr->parameter_quals;
                    vla->loc = tok.loc;
                    err = cc_capture_vla_bound(p, flags, dim, &vla->bound);
                    if(err) return err;
                    err = cc_expect_punct(p, CC_rbracket);
                    if(err) return err;
                    vla->element = **out_tail;
                    **out_tail = (CcQualType){.bits=(uintptr_t)vla};
                    *out_tail = &vla->element;
                    continue;
                }
                cc_release_expr(p, dim);
                if(length < 0) return cc_error(p, tok.loc, "Negative array length");
                arr->length = (size_t)length;
            }
            err = cc_expect_punct(p, CC_rbracket);
            if(err) return err;
            arr->element = **out_tail;
            **out_tail = (CcQualType){.bits = (uintptr_t)arr};
            *out_tail = &arr->element;
            continue;
        }

        if(tok.type == CC_PUNCTUATOR && tok.punct.punct == '('){
            // Function parameters
            if(out_param_names && out_param_names->scope) out_param_names = NULL;
            _Bool prototype_only = !out_param_names || out_head->bits;
            err = cc_push_scope(p);
            if(err) return CC_OOM_ERROR;
            if(out_param_names) out_param_names->scope = p->current;
            CcScope* saved_function_scope = p->function_scope;
            p->function_scope = p->current;
            // Isolate prototype effects without allocating for ordinary parameters.
            // The statement array grows only when a bound emits an initializer.
            CcStmtSink bounds_sink = {.prev = p->stmt_sink};
            p->stmt_sink = &bounds_sink;
            CcAttributes saved_param_attributes = p->attributes;
            Marray(CcQualType) param_types = {0};
            _Bool variadic = 0;
            _Bool no_prototype = 0;
            CcToken peek;
            err = cc_peek(p, &peek);
            if(err) goto param_err;
            if(peek.type == CC_PUNCTUATOR && peek.punct.punct == ')'){
                // () — no prototype
                no_prototype = 1;
                goto param_done;
            }
            for(;;){
                err = cc_peek(p, &peek);
                if(err) goto param_err;
                // ...
                if(peek.type == CC_PUNCTUATOR && peek.punct.punct == CC_ellipsis){
                    err = cc_next_token(p, &peek);
                    if(err) goto param_err;
                    variadic = 1;
                    break;
                }
                // Parse parameter: declaration-specifiers [declarator]
                cc_clear_attributes(&p->attributes);
                CcDeclBase param_base = {0};
                CcParseFlags param_flags = flags;
                param_flags.parameter_type = 1;
                param_flags.unevaluated |= prototype_only;
                err = cc_parse_declaration_specifier(p, param_flags, &param_base);
                if(err) goto param_err;
                if(!param_base.spec.bits && !param_base.type.bits){
                    err = cc_error(p, peek.loc, "Expected type specifier in function parameter");
                    goto param_err;
                }
                if(param_base.spec.sp_typedef){
                    err = cc_error(p, peek.loc, "typedef not allowed in function parameter");
                    goto param_err;
                }
                if(param_base.spec.sp_funcbits){
                    err = cc_error(p, peek.loc, "function specifier is not allowed on parameters");
                    goto param_err;
                }
                if(param_base.spec.sp_thread_local){
                    err = cc_error(p, peek.loc, "thread_local is not valid on parameters");
                    goto param_err;
                }
                if(param_base.spec.sp_static
                || param_base.spec.sp_extern
                || param_base.spec.sp_auto
                || param_base.spec.sp_constexpr){
                    err = cc_error(p, peek.loc, "only register storage class is allowed on parameters");
                    goto param_err;
                }
                if(param_base.has_alignas){
                    err = cc_error(p, peek.loc, "_Alignas is not allowed on parameters");
                    goto param_err;
                }
                err = cc_resolve_specifiers(p, &param_base);
                if(err) goto param_err;
                if(param_base.spec.sp_infer_type){
                    err = cc_error(p, peek.loc, "Expected type in function parameter");
                    goto param_err;
                }

                CcQualType param_head = {0};
                CcQualType* param_tail = &param_head;
                Atom param_name = NULL;
                err = cc_parse_declarator(p, param_flags, &param_head, &param_tail, &param_name, NULL, NULL);
                if(err) goto param_err;
                *param_tail = param_base.type;
                if(out_param_names){
                    CcQualType check = param_head;
                    for(;;){
                        if(ccqt_kind(check) == CC_VLA){
                            out_param_names->unspecified_vla |= ccqt_as_vla(check)->unspecified;
                            check = ccqt_as_vla(check)->element;
                        }
                        else if(ccqt_kind(check) == CC_ARRAY)
                            check = ccqt_as_array(check)->element;
                        else if(ccqt_kind(check) == CC_POINTER)
                            check = ccqt_as_ptr(check)->pointee;
                        else break;
                    }
                }
                if(ccqt_is_basic(param_head) && ccqt_basic_kind(param_head) == CCBT_void){
                    CcToken end;
                    err = cc_peek(p, &end);
                    if(err) goto param_err;
                    if(!param_name && !param_types.count && !param_head.quals
                        && !param_base.spec.sp_storagebits
                        && end.type == CC_PUNCTUATOR && end.punct.punct == CC_rparen)
                        goto param_done;
                    err = cc_error(p, peek.loc, "parameter cannot have void type");
                    goto param_err;
                }
                // consume trailing __attribute__
                {
                    err = cc_parse_attributes(p, flags, &p->attributes);
                    if(err) goto param_err;
                    if(p->attributes.vector_size){
                        err = cc_check_derived_type(p, param_head, param_base.loc);
                        if(err) goto param_err;
                        err = cc_adjust_parameter_type(p, param_head, &param_head);
                        if(err) goto param_err;
                    }
                    err = cc_apply_vector_type(p, &param_head, p->attributes.vector_size, param_base.loc);
                    if(err) goto param_err;
                    if(p->attributes.has_aligned && p->attributes.aligned > param_base.alignment)
                        param_base.alignment = p->attributes.aligned;
                    p->attributes = saved_param_attributes;
                }

                SrcLoc param_loc = peek.loc;
                _Bool typed_pack = 0;
                err = cc_peek(p, &peek);
                if(err) goto param_err;
                if(peek.type == CC_PUNCTUATOR && peek.punct.punct == CC_ellipsis){
                    if(!out_param_names){
                        err = cc_error(p, peek.loc, "typed varargs require a direct function declaration");
                        goto param_err;
                    }
                    err = cc_next_token(p, &peek);
                    if(err) goto param_err;
                    for(size_t i = 0; i < out_param_names->names.count; i++){
                        if(out_param_names->names.data[i].typed_pack){
                            err = cc_error(p, peek.loc, "only one typed varargs pack is allowed");
                            goto param_err;
                        }
                    }
                    out_param_names->typed_pack_loc = peek.loc;
                    CcQualType element = cc_intern_qualtype(p, param_head);
                    if(!element.bits){ err = CC_OOM_ERROR; goto param_err; }
                    err = cc_check_derived_type(p, element, param_loc);
                    if(err) goto param_err;
                    uint32_t size;
                    if(ccqt_bt_eq(element, CCBT_void)
                    || ccqt_kind(element) == CC_FUNCTION
                    || cc_type_sizeof_complete(cc_target(p), element, &size, NULL)
                    ){
                        err = cc_type_error(p, peek.loc, element, "typed varargs require a complete object element type");
                        goto param_err;
                    }
                    err = cc_slice_of(p, element, peek.loc, &param_head);
                    if(err) goto param_err;
                    typed_pack = 1;
                }

                err = ma_push(CcQualType)(&param_types, cc_scratch_allocator(p), param_head);
                if(err){
                    err = CC_OOM_ERROR;
                    goto param_err;
                }

                if(param_name){
                    if(cc_scope_lookup_var(cc_scope_declarations(p->current), param_name, CC_SCOPE_NO_WALK)){
                        err = cc_error(p, param_loc, "duplicate parameter name '%.*s'", param_name->length, param_name->data);
                        goto param_err;
                    }
                }
                CcQualType param_type = cc_intern_qualtype(p, param_head);
                if(!param_type.bits){ err = CC_OOM_ERROR; goto param_err; }
                err = cc_check_derived_type(p, param_type, param_loc);
                if(err) goto param_err;
                err = cc_adjust_parameter_type(p, param_head, &param_type);
                if(err) goto param_err;
                CcVariable* var = Allocator_zalloc(cc_allocator(p), 1, sizeof *var, _Alignof(CcVariable));
                if(!var){
                    err = CC_OOM_ERROR;
                    goto param_err;
                }
                if(!param_name) param_name = nil_atom;
                *var = (CcVariable){
                    .name = param_name,
                    .loc = param_loc,
                    .type = param_type,
                    .automatic = 1,
                    .register_ = param_base.spec.sp_register,
                    .alignment = param_base.alignment,
                };
                err = cc_scope_insert_var(cc_allocator(p), cc_scope_declarations(p->current), param_name, var);
                if(err){
                    err = CC_OOM_ERROR;
                    goto param_err;
                }
                if(out_param_names){
                    Marray(CcFuncParam)* pn = &out_param_names->names;
                    err = ma_push(CcFuncParam)(pn, cc_allocator(p), (CcFuncParam){.name = param_name, .decl = var, .typed_pack = typed_pack});
                    if(err){
                        err = CC_OOM_ERROR;
                        goto param_err;
                    }
                }

                err = cc_peek(p, &peek);
                if(err) goto param_err;
                if(peek.type == CC_PUNCTUATOR && peek.punct.punct == CC_assign){
                    if(typed_pack){
                        err = cc_error(p, peek.loc, "typed varargs packs cannot have default arguments");
                        goto param_err;
                    }
                    if(!out_param_names){
                        err = cc_error(p, peek.loc, "default arguments require a direct function declaration");
                        goto param_err;
                    }
                    err = cc_next_token(p, &peek);
                    if(err) goto param_err;
                    CcParseFlags default_flags = flags;
                    default_flags.vc = CC_RUNTIME_VALUE;
                    default_flags.parsing_default_arg = 1;
                    CcExpr* value = NULL;
                    err = cc_parse_assignment_expr(p, default_flags, &value, param_type);
                    if(err) goto param_err;
                    out_param_names->names.data[out_param_names->names.count-1].default_value = value;
                    err = cc_implicit_cast(p, value, (CcQualType){.unqual=param_type.unqual}, &value);
                    if(err) goto param_err;
                    out_param_names->names.data[out_param_names->names.count-1].default_value = value;
                    Parray captures = {0};
                    err = cc_prepare_default_arg(p, value, 0, &captures, 0);
                    pa_cleanup(&captures, cc_scratch_allocator(p));
                    if(err) goto param_err;
                    err = cc_peek(p, &peek);
                    if(err) goto param_err;
                }
                if(peek.type == CC_PUNCTUATOR && peek.punct.punct == ','){
                    err = cc_next_token(p, &peek);
                    if(err) goto param_err;
                    continue;
                }
                break;
            }
            param_done:;
            {
                CcFunction* func = Allocator_zalloc_extra(cc_scratch_allocator(p), param_types.count, sizeof *func->params, sizeof *func, _Alignof(CcFunction));
                if(!func){
                    err = CC_OOM_ERROR;
                    goto param_err;
                }
                func->kind = CC_FUNCTION;
                func->is_variadic = variadic;
                func->no_prototype = no_prototype;
                func->param_count = (uint32_t)param_types.count;
                for(uint32_t i = 0; i < param_types.count; i++)
                    func->params[i] = param_types.data[i];
                ma_cleanup(CcQualType)(&param_types, cc_scratch_allocator(p));
                err = cc_expect_punct(p, CC_rparen);
                if(err) goto param_err;
                func->return_type = **out_tail;
                **out_tail = (CcQualType){.bits = (uintptr_t)func};
                *out_tail = &func->return_type;
                if(out_param_names && bounds_sink.stmts.count){
                    err = cc_finalize_stmt_list(p, tok.loc, &bounds_sink.stmts, 1, &out_param_names->bounds);
                    if(err) goto param_err;
                }
                p->stmt_sink = bounds_sink.prev;
                p->function_scope = saved_function_scope;
                p->attributes = saved_param_attributes;
                for(size_t i = 0; i < bounds_sink.stmts.count; i++)
                    cc_free_stmt_tree(p, bounds_sink.stmts.data[i]);
                pa_cleanup(&bounds_sink.stmts, cc_allocator(p));
                // Only collect param names for the first (outermost) function.
                out_param_names = NULL;
                continue;
            }
            param_err:
            p->stmt_sink = bounds_sink.prev;
            p->function_scope = saved_function_scope;
            p->attributes = saved_param_attributes;
            for(size_t i = 0; i < bounds_sink.stmts.count; i++)
                cc_free_stmt_tree(p, bounds_sink.stmts.data[i]);
            pa_cleanup(&bounds_sink.stmts, cc_allocator(p));
            ma_cleanup(CcQualType)(&param_types, cc_scratch_allocator(p));
            return err;
        }
        err = cc_unget(p, &tok);
        if(err) return err;
        break;
    }
    return 0;
}

static
int
cc_parse_declarator(CcParser* p, CcParseFlags flags, CcQualType* out_head, CcQualType*_Nonnull*_Nonnull out_tail, Atom _Nullable * _Nullable out_name, SrcLoc* _Nullable out_name_loc, CcParsedParams *_Nullable out_param_names){
    CcScope* saved_scope = p->current;
    int err = cc_parse_declarator_inner(p, flags, out_head, out_tail, out_name, out_name_loc, out_param_names);
    if(!err && out_param_names && ccqt_kind(*out_head) != CC_FUNCTION){
        for(size_t i = 0; i < out_param_names->names.count; i++){
            CcExpr* value = out_param_names->names.data[i].default_value;
            if(value){
                err = cc_error(p, value->loc, "default arguments require a direct function declaration");
                break;
            }
        }
    }
    if(!err && out_param_names && out_param_names->typed_pack_loc.bits
        && (!out_name || ccqt_kind(*out_head) != CC_FUNCTION))
        err = cc_error(p, out_param_names->typed_pack_loc, "typed varargs require a direct function declaration");
    while(p->current != saved_scope){
        CcScope* scope = p->current;
        if(out_param_names && scope == out_param_names->scope){
            if(!err){
                p->current = scope->parent;
                continue;
            }
            out_param_names->scope = NULL;
        }
        cc_pop_scope(p);
    }
    return err;
}

static
int
cc_merge_default_args(CcParser* p, CcFunc* func, CcParsedParams* params, _Bool replace, SrcLoc loc){
    size_t count = replace ? params->names.count : func->params.count;
    _Bool seen_default = 0;
    _Bool named_only = 0;
    for(size_t i = 0; i < count; i++){
        CcFuncParam* incoming = i < params->names.count ? &params->names.data[i] : NULL;
        CcFuncParam* previous = i < func->params.count ? &func->params.data[i] : NULL;
        if(incoming && previous && incoming->default_value && previous->default_value)
            return cc_error(p, incoming->default_value->loc, "redefinition of default argument for parameter %u", (unsigned)i+1);
        CcFuncParam* param = replace ? incoming : previous;
        if(param->typed_pack){
            named_only = 1;
            continue;
        }
        _Bool has_default = (incoming && incoming->default_value) || (previous && previous->default_value);
        if(!named_only && seen_default && !has_default)
            return cc_error(p, loc, "missing default argument for parameter %u", (unsigned)i+1);
        seen_default |= has_default;
    }
    for(size_t i = 0; i < count; i++){
        CcFuncParam* incoming = i < params->names.count ? &params->names.data[i] : NULL;
        CcFuncParam* previous = i < func->params.count ? &func->params.data[i] : NULL;
        if(replace){
            if(!incoming->default_value && previous)
                incoming->default_value = previous->default_value;
        }
        else if(incoming && incoming->default_value)
            previous->default_value = incoming->default_value;
    }
    if(replace){
        func->params.count = params->names.count;
        func->params.data = params->names.data;
    }
    return 0;
}

static
int
cc_qualify_object_type(CcParser* p, CcQualType type, uintptr_t quals, CcQualType* out){
    int err;
    if(ccqt_raw_kind(type) == CC_ALIGNED){
        CcQualType underlying;
        err = cc_qualify_object_type(p, ccqt_unaligned(type), quals, &underlying);
        if(err) return err;
        CcAligned* aligned = cc_intern_aligned(&p->type_cache, cc_allocator(p), underlying, ccqt_as_aligned(type)->alignment);
        if(!aligned) return CC_OOM_ERROR;
        *out = (CcQualType){.bits=(uintptr_t)aligned | underlying.quals};
        return 0;
    }
    if(!ccqt_is_array(type)){
        type.quals = quals;
        *out = type;
        return 0;
    }
    CcQualType element;
    err = cc_qualify_object_type(p, ccqt_array_element(type), quals, &element);
    if(err) return err;
    if(ccqt_kind(type) == CC_VLA){
        CcVla* vla = cc_intern_vla(&p->type_cache, cc_allocator(p), ccqt_as_vla(type), element);
        if(!vla) return CC_OOM_ERROR;
        *out = (CcQualType){.bits=(uintptr_t)vla};
    }
    else {
        CcArray* old = ccqt_as_array(type);
        CcArray* array = cc_intern_array(&p->type_cache, cc_allocator(p), element, old->length, old->is_incomplete);
        if(!array) return CC_OOM_ERROR;
        *out = (CcQualType){.bits=(uintptr_t)array};
    }
    return 0;
}

static
int
cc_const_object_type(CcParser* p, CcQualType type, CcQualType* out){
    CcQualType object = ccqt_array_object_type(type);
    object.is_const = 1;
    return cc_qualify_object_type(p, type, object.quals, out);
}

static
int
cc_unqual_object_type(CcParser* p, CcQualType type, CcQualType* out){
    return cc_qualify_object_type(p, type, 0, out);
}

static
int
cc_adjust_parameter_type(CcParser* p, CcQualType type, CcQualType* out){
    // Bracket qualifiers describe the adjusted pointer, not the array or its
    // elements. Read this declarator metadata before canonicalizing the type.
    uintptr_t quals = ccqt_kind(type) == CC_ARRAY ? ccqt_as_array(type)->parameter_quals
        : ccqt_kind(type) == CC_VLA ? ccqt_as_vla(type)->parameter_quals : 0;
    type = cc_intern_qualtype(p, type);
    if(!type.bits) return CC_OOM_ERROR;
    if(ccqt_is_array(type) || ccqt_kind(type) == CC_FUNCTION){
        CcQualType pointee = ccqt_is_array(type) ? ccqt_qualified_array_element(type) : type;
        CcPointer* ptr = cc_intern_pointer(&p->type_cache, cc_allocator(p), pointee, 0, 0);
        if(!ptr) return CC_OOM_ERROR;
        type = (CcQualType){.bits=(uintptr_t)ptr | quals};
    }
    *out = type;
    return 0;
}

static
CcQualType
cc_intern_qualtype(CcParser* p, CcQualType t){
    // Return no type on allocation failure. A parsed node may live in scratch
    // storage and must never escape as a substitute for a canonical type.
    uintptr_t quals = t.quals;
    if(ccqt_raw_kind(t) == CC_ALIGNED){
        CcQualType underlying = cc_intern_qualtype(p, ccqt_unaligned(t));
        if(!underlying.bits) return CCQT_NONE;
        CcAligned* aligned = cc_intern_aligned(&p->type_cache, cc_allocator(p), underlying, ccqt_as_aligned(t)->alignment);
        if(!aligned) return CCQT_NONE;
        return (CcQualType){.bits=(uintptr_t)aligned | underlying.quals};
    }
    switch(ccqt_kind(t)){
        case CC_VLA: {
            CcVla* old = ccqt_as_vla(t);
            CcQualType element = cc_intern_qualtype(p, ccqt_qualified_array_element(t));
            if(!element.bits) return CCQT_NONE;
            CcVla* vla = cc_intern_vla(&p->type_cache, cc_allocator(p), old, element);
            if(!vla) return CCQT_NONE;
            return (CcQualType){.bits=(uintptr_t)vla};
        }
        case CC_ALIGNED: cc_unsafe_unreachable();
        case CC_BASIC:
            return t;
        case CC_POINTER:
        case CC_BLOCK_POINTER: {
            CcPointer* old = ccqt_as_ptr(t);
            CcQualType pointee = cc_intern_qualtype(p, old->pointee);
            if(!pointee.bits) return CCQT_NONE;
            CcPointer* ptr = cc_intern_pointer(&p->type_cache, cc_allocator(p), pointee, old->restrict_, ccqt_kind(t)==CC_BLOCK_POINTER);
            if(!ptr) return CCQT_NONE;
            return (CcQualType){.bits=(uintptr_t)ptr | quals};
        }
        case CC_SLICE: {
            CcSlice* old = ccqt_as_slice(t);
            CcQualType pointee = cc_intern_qualtype(p, old->pointee);
            if(!pointee.bits) return CCQT_NONE;
            CcSlice* slice = cc_intern_slice(&p->type_cache, cc_allocator(p), pointee, old->restrict_);
            if(!slice) return CCQT_NONE;
            return (CcQualType){.bits=(uintptr_t)slice | quals};
        }
        case CC_VECTOR: {
            CcVector* old = ccqt_as_vector(t);
            CcQualType element = cc_intern_qualtype(p, old->element);
            if(!element.bits) return CCQT_NONE;
            CcVector* vector = cc_intern_vector(&p->type_cache, cc_allocator(p), element, old->length, old->vector_size);
            if(!vector) return CCQT_NONE;
            return (CcQualType){.bits=(uintptr_t)vector | quals};
        }
        case CC_ARRAY: {
            CcArray* old = ccqt_as_array(t);
            CcQualType elem = cc_intern_qualtype(p, ccqt_qualified_array_element(t));
            if(!elem.bits) return CCQT_NONE;
            if(ccqt_kind(elem) == CC_VLA && !old->is_incomplete){
                CcVla* vla = Allocator_zalloc(cc_allocator(p), 1, sizeof *vla, _Alignof(CcVla));
                if(!vla) return CCQT_NONE;
                vla->kind = CC_VLA;
                vla->element = elem;
                vla->loc = old->declarator_loc;
                CcExpr* bound = cc_uint64_expr(p, vla->loc, ccqt_basic(cc_target(p)->size_type), old->length);
                if(!bound) return CCQT_NONE;
                vla->bound = bound;
                CcVla* canonical = cc_intern_vla(&p->type_cache, cc_allocator(p), vla, elem);
                if(!canonical) return CCQT_NONE;
                return (CcQualType){.bits=(uintptr_t)canonical};
            }
            CcArray* arr = cc_intern_array(&p->type_cache, cc_allocator(p), elem, old->length, old->is_incomplete);
            if(!arr) return CCQT_NONE;
            return (CcQualType){.bits = (uintptr_t)arr};
        }
        case CC_FUNCTION: {
            CcFunction* old = ccqt_as_function(t);
            CcQualType* params = old->param_count ? Allocator_alloc(cc_scratch_allocator(p), old->param_count, sizeof *params, _Alignof(CcQualType)) : NULL;
            if(old->param_count && !params) return CCQT_NONE;
            for(uint32_t i = 0; i < old->param_count; i++){
                if(cc_adjust_parameter_type(p, old->params[i], &params[i])){
                    if(params) Allocator_free(cc_scratch_allocator(p), params, old->param_count, sizeof *params, _Alignof(CcQualType));
                    return CCQT_NONE;
                }
            }
            CcQualType ret = cc_intern_qualtype(p, old->return_type);
            CcFunction* func = ret.bits ? cc_intern_function(&p->type_cache, cc_allocator(p), ret, params, old->param_count, old->param_count, old->is_variadic, old->no_prototype) : NULL;
            if(params) Allocator_free(cc_scratch_allocator(p), params, old->param_count, sizeof *params, _Alignof(CcQualType));
            if(!func) return CCQT_NONE;
            return (CcQualType){.bits = (uintptr_t)func | quals};
        }
        case CC_ENUM:
        case CC_STRUCT:
        case CC_UNION:
            return t;
        DRP_CASES_EXHAUSTED;
    }
}

static
int
cc_check_pack_decl(CcParser* p, CcFunc* func, CcParsedParams* params, CcQualType type, SrcLoc loc){
    if(func->type->no_prototype || ccqt_as_function(type)->no_prototype) return 0;
    uint32_t old_pack = UINT32_MAX, new_pack = UINT32_MAX;
    for(size_t i = 0; i < func->params.count; i++)
        if(func->params.data[i].typed_pack) old_pack = (uint32_t)i;
    for(size_t i = 0; i < params->names.count; i++)
        if(params->names.data[i].typed_pack) new_pack = (uint32_t)i;
    if(old_pack != new_pack)
        return cc_error(p, loc, "conflicting typed varargs specifier for '%.*s'", func->name->length, func->name->data);
    return 0;
}

static
int
cc_parse_decls(CcParser* p, CcParseFlags flags, const CcDeclBase* declbase, CcParsedDecls* out){
    DRP_PROFILE_BEGIN(profile, "parser.declarations");
    *out = (CcParsedDecls){0};
    if(declbase->spec.sp_register
    && cc_scope_declarations(p->current) == &p->module->scope)
        return cc_error(p, declbase->loc, "register requires block scope");
    int err = 0;
    CcToken tok;
    CcAttributes decl_attributes = p->attributes;
    _Bool decl_always_inline = p->attributes.always_inline;
    p->attributes.always_inline = 0;
    _Bool decl_noinline = p->attributes.noinline;
    p->attributes.noinline = 0;
    _Bool decl_hotswappable = p->attributes.hotswappable;
    p->attributes.hotswappable = 0;
    if(p->auto_typedef && !declbase->spec.sp_typedef && !declbase->spec.sp_infer_type){
        Atom tag_name = NULL;
        CcQualType base = declbase->type;
        if(!ccqt_is_basic(base)){
            CcTypeKind tk = ccqt_kind(base);
            if(tk == CC_STRUCT) tag_name = ccqt_as_struct(base)->name;
            else if(tk == CC_UNION) tag_name = ccqt_as_union(base)->name;
            else if(tk == CC_ENUM) tag_name = ccqt_as_enum(base)->name;
        }
        if(tag_name){
            CcQualType existing_td = cc_scope_lookup_typedef(cc_scope_declarations(p->current), tag_name, CC_SCOPE_NO_WALK);
            if(!existing_td.bits){
                err = cc_scope_insert_typedef(cc_allocator(p), cc_scope_declarations(p->current), tag_name, base, (SrcLoc){0});
                if(err) return err;
            }
        }
    }
    for(_Bool first = 1;;first=0){
        if(!first) p->attributes = decl_attributes;
        Atom name = NULL;
        uint16_t decl_alignment = declbase->alignment;
        SrcLoc name_loc = {0};
        CcQualType type;
        _Bool is_fndef = 0;
        CcExpr* initializer = NULL;
        CcParsedParams param_names = {0};
        if(declbase->spec.sp_infer_type){
            err = cc_next_token(p, &tok);
            if(err) return err;
            if(tok.type != CC_IDENTIFIER)
                return cc_error(p, tok.loc, "Expected identifier for type-inferred declaration");
            name = tok.ident.ident;
            name_loc = tok.loc;
            type = declbase->type;
        }
        else {
            CcQualType head = {0};
            CcQualType* tail = &head;
            err = cc_parse_declarator(p, flags, &head, &tail, &name, &name_loc, &param_names);
            if(err){
                ma_cleanup(CcFuncParam)(&param_names.names, cc_allocator(p));
                return err;
            }
            // tail != &head means the declarator itself built derived types.
            // Only allow function bodies when the declarator introduced the
            // function type, not when it came from a typedef.
            is_fndef = name && tail != &head && ccqt_kind(head) == CC_FUNCTION;
            *tail = declbase->type;
            type = cc_intern_qualtype(p, head);
            err = type.bits ? cc_check_derived_type(p, type, declbase->loc) : CC_OOM_ERROR;
            if(err){
                ma_cleanup(CcFuncParam)(&param_names.names, cc_allocator(p));
                return err;
            }
            if(declbase->spec.sp_constexpr){
                err = cc_const_object_type(p, type, &type);
                if(err) return err;
            }
        }
        // trailing __attribute__
        err = cc_parse_attributes(p, flags, &p->attributes);
        if(err) return err;
        Atom asm_label = NULL;
        {
            CcToken peek_asm;
            err = cc_peek(p, &peek_asm);
            if(err) return err;
            if(peek_asm.type == CC_KEYWORD && peek_asm.kw.kw == CC_asm){
                cc_next_token(p, &tok);
                err = cc_next_token(p, &tok);
                if(err) return err;
                if(tok.type != CC_PUNCTUATOR || tok.punct.punct != '(')
                    return cc_error(p, tok.loc, "Expected '(' after asm");
                err = cc_next_token(p, &tok);
                if(err) return err;
                if(tok.type != CC_STRING_LITERAL)
                    return cc_error(p, tok.loc, "Expected string literal in asm label");
                asm_label = AT_atomize(p->cpp.at, tok.str.utf8, tok.str.length - 1);
                if(!asm_label) return CC_OOM_ERROR;
                err = cc_next_token(p, &tok);
                if(err) return err;
                if(tok.type != CC_PUNCTUATOR || tok.punct.punct != ')')
                    return cc_error(p, tok.loc, "Expected ')' after asm label");
            }
        }
        err = cc_parse_attributes(p, flags, &p->attributes);
        if(err) return err;
        err = cc_apply_vector_type(p, &type, p->attributes.vector_size, declbase->loc);
        if(err) return err;
        if(ccqt_kind(type) == CC_FUNCTION){
            err = cc_check_decl_alignment(p, declbase, type, 0);
            if(err) return err;
        }
        if(p->attributes.has_aligned || p->attributes.packed || p->attributes.transparent_union){
            CcTypeKind tk = ccqt_kind(type);
            if(p->attributes.has_aligned){
                if(declbase->spec.sp_typedef){
                    if(tk == CC_FUNCTION || (ccqt_is_basic(type) && ccqt_basic_kind(type) == CCBT_void))
                        return cc_type_error(p, declbase->loc, type, "aligned typedef requires an object type");
                    CcAligned* aligned = cc_intern_aligned(&p->type_cache, cc_allocator(p), type, p->attributes.aligned);
                    if(!aligned) return CC_OOM_ERROR;
                    type = (CcQualType){.bits=(uintptr_t)aligned | type.quals};
                }
                else if(p->attributes.aligned > decl_alignment)
                    decl_alignment = p->attributes.aligned;
                p->attributes.has_aligned = 0;
                p->attributes.aligned = 0;
            }
            if(p->attributes.packed && tk != CC_STRUCT)
                return cc_type_error(p, declbase->loc, type, "packed attribute on non-struct type is not supported");
            if(p->attributes.transparent_union && tk != CC_UNION)
                return cc_type_error(p, declbase->loc, type, "transparent_union attribute on non-union type is not supported");
        }
        const CcPrintfFormat* printf_formats = p->attributes.printf_formats;
        _Bool is_always_inline = decl_always_inline || p->attributes.always_inline;
        _Bool is_noinline = decl_noinline || p->attributes.noinline;
        _Bool is_hotswappable = decl_hotswappable || p->attributes.hotswappable;
        _Bool is_noreturn = declbase->spec.sp_noreturn || p->attributes.is_noreturn;
        if(is_hotswappable && (ccqt_kind(type) != CC_FUNCTION || declbase->spec.sp_typedef))
            return cc_error(p, name_loc, "drc::hotswappable requires a function declaration");
        cc_clear_attributes(&p->attributes);
        if(type.bits){
            err = cc_check_derived_type(p, type, declbase->loc);
            if(err) return err;
        }
        // postfix processing
        _Bool stop = 0;
        err = cc_next_token(p, &tok);
        if(err) return err;
        if(first && tok.type == CC_PUNCTUATOR && tok.punct.punct == '{'){
            if(param_names.unspecified_vla)
                return cc_error(p, tok.loc, "[*] is not allowed in a function definition");
            if(!is_fndef)
                return cc_error(p, tok.loc, "Expected ',' or ';'");
            if(declbase->spec.sp_typedef
            || declbase->spec.sp_auto
            || declbase->spec.sp_register
            || declbase->spec.sp_constexpr)
                return cc_error(p, name_loc, "invalid storage class in function definition");
            err = cc_check_function_definition_type(p, ccqt_as_function(type), name_loc);
            if(err) return err;
            if(declbase->spec.sp_thread_local)
                return cc_error(p, name_loc, "thread_local is only valid on variables");
            _Bool eager = p->eager_parsing || p->current_func;
            CcSymbol sym;
            CcFunc* func = NULL;
            _Bool found = cc_scope_lookup_symbol(cc_scope_declarations(p->current), name, CC_SCOPE_NO_WALK, &sym);
            _Bool root_definition = cc_scope_declarations(p->current) == &p->module->scope;
            if(!found && root_definition && !declbase->spec.sp_static){
                CcScope *last = declbase->spec.sp_extern ? NULL : &p->module->scope;
                found = cc_lookup_visible_linked_symbol(p->current, last, name, &sym);
                if(!found)
                    found = cc_lookup_external_entity(p, name, declbase->spec.sp_extern, &sym);
            }
            if(found){
                switch(sym.kind){
                    case CC_SYM_FUNC:
                        func = sym.func;
                        break;
                    case CC_SYM_VAR:
                    case CC_SYM_TYPEDEF:
                    case CC_SYM_ENUMERATOR:
                        return cc_error(p, tok.loc, "Redefinition of '%.*s' as a different kind of symbol", name->length, name->data);
                }
            }
            if(func){
                if(func->defined || func->native_func)
                    return cc_error(p, tok.loc, "Redefinition of function '%.*s'", name->length, name->data);
                err = cc_check_func_compat(p, func, declbase, type, 1, tok.loc);
                if(err) return err;
                err = cc_check_pack_decl(p, func, &param_names, type, tok.loc);
                if(err) return err;
                err = cc_merge_func_decl_type(p, func->type, &type);
                if(err) return err;
            }
            else {
                func = Allocator_zalloc(cc_allocator(p), 1, sizeof *func, _Alignof(CcFunc));
                if(!func) return CC_OOM_ERROR;
                func->name = name;
            }
            err = cc_scope_insert_func(cc_allocator(p), cc_scope_declarations(p->current), name, func);
            if(err) return err;
            func->type = ccqt_as_function(type);
            func->loc = name_loc;
            if(asm_label) func->mangle = asm_label;
            func->defined = 1;
            func->has_linkage = root_definition;
            if(declbase->spec.sp_extern || declbase->spec.sp_static){
                func->extern_ = declbase->spec.sp_extern;
                func->static_ |= declbase->spec.sp_static;
            }
            if(root_definition && !func->static_){
                err = cc_record_extern(p, name, (CcSymbol){.kind=CC_SYM_FUNC, .func=func}, declbase->spec.sp_extern);
                if(err) return err;
            }
            func->inline_ = func->inline_ || declbase->spec.sp_inline;
            func->always_inline = func->always_inline || is_always_inline;
            func->noinline = func->noinline || is_noinline;
            func->noreturn_ = func->noreturn_ || is_noreturn;
            func->hotswappable = func->hotswappable || is_hotswappable;
            err = cc_apply_printf_formats(p, func, printf_formats);
            if(err) return err;
            err = cc_merge_default_args(p, func, &param_names, 1, name_loc);
            if(err) return err;
            func->param_scope = param_names.scope;
            func->body_tree = param_names.bounds;
            func->definition_scope = cc_scope_retain(p->current);
            func->module = p->module;
            if(eager){
                func->_Self_type = p->current_tag_type;
                func->enclosing = p->current_func;
                err = cc_parse_func_body_inner(p, func, 1);
                if(err) return err;
                err = cc_expect_punct(p, CC_rbrace);
                if(err) return err;
            }
            else {
                Marray(CcToken)* body_tokens = cc_get_scratch(p);
                if(!body_tokens) return CC_OOM_ERROR;
                DRP_PROFILE_BEGIN(tokens_profile, "parser.function_tokens");
                int depth = 1;
                while(depth > 0){
                    CcToken t;
                    err = cc_next_token(p, &t);
                    if(err) return err;
                    if(t.type == CC_EOF)
                        return cc_error(p, tok.loc, "Unexpected EOF in function body");
                    if(t.type == CC_PUNCTUATOR){
                        if(t.punct.punct == '{') depth++;
                        else if(t.punct.punct == '}') depth--;
                    }
                    if(depth > 0){
                        err = ma_push(CcToken)(body_tokens, cc_allocator(p), t);
                        if(err) return err;
                    }
                }
                func->_Self_type = p->current_tag_type;
                func->tokens = body_tokens;
                DRP_PROFILE_END(tokens_profile);
            }
            out->function_definition = func;
            return 0;
        }
        // There is no function body to inherit this prototype scope.
        cc_free_stmt_tree(p, param_names.bounds);
        CcScope* prototype_scope = param_names.scope;
        if(prototype_scope){
            CcScope* saved_scope = p->current;
            p->current = prototype_scope;
            err = cc_parse_local_methods(p);
            p->current = saved_scope;
            if(err) return err;
            if(!prototype_scope->retained) fl_push(&p->scratch_scopes, prototype_scope);
            param_names.scope = NULL;
        }
        // For non-typedef, non-function variable declarations, insert
        // the variable into scope before parsing the initializer so that
        // self-referential expressions like sizeof(*var) work.
        CcVariable* _Null_unspecified var = NULL;
        _Bool redecl = 0;
        _Bool is_func_decl = is_fndef || (type.ptr && ccqt_kind(type) == CC_FUNCTION);
        if(is_func_decl
        && p->current_func
        && !declbase->spec.sp_typedef
        && (declbase->spec.sp_register || declbase->spec.sp_auto))
            return cc_error(p, name_loc, "invalid storage class in block-scope function declaration");
        if(ccqt_is_vm(type)){
            if(declbase->spec.sp_extern)
                return cc_type_error(p, name_loc, type, "variably modified declaration cannot have external linkage");
            if(ccqt_kind(type) == CC_VLA
            && !declbase->spec.sp_typedef
            && (declbase->spec.sp_static || declbase->spec.sp_thread_local || declbase->spec.sp_constexpr))
                return cc_type_error(p, name_loc, type, "VLA object requires automatic storage duration");
            CcDefer* marker = Allocator_zalloc(cc_allocator(p), 1, sizeof *marker, _Alignof(CcDefer));
            if(!marker) return CC_OOM_ERROR;
            marker->prev = p->defer_context.active;
            marker->vm_scope = 1;
            p->defer_context.active = marker;
        }
        if(declbase->spec.sp_thread_local && (is_func_decl || declbase->spec.sp_typedef))
            return cc_error(p, name_loc, "thread_local is only valid on variables");
        if(declbase->spec.sp_thread_local && p->current_func && !declbase->spec.sp_static && !declbase->spec.sp_extern)
            return cc_error(p, name_loc, "block-scope thread_local requires static or extern");
        if(name && !declbase->spec.sp_typedef && !is_func_decl){
            if(declbase->spec.sp_inline)
                return cc_error(p, tok.loc, "'inline' is only valid on functions");
            if(declbase->spec.sp_noreturn)
                return cc_error(p, tok.loc, "'_Noreturn' is only valid on functions");
            CcSymbol sym;
            _Bool found = cc_scope_lookup_symbol(cc_scope_declarations(p->current), name, CC_SCOPE_NO_WALK, &sym);
            _Bool root_declaration = cc_scope_declarations(p->current) == &p->module->scope;
            if(!found && !declbase->spec.sp_static && (root_declaration || declbase->spec.sp_extern)){
                if(declbase->spec.sp_extern)
                    found = cc_lookup_visible_linked_symbol(p->current, NULL, name, &sym);
                if(!found)
                    found = cc_lookup_external_entity(p, name, declbase->spec.sp_extern, &sym);
            }
            if(found){
                switch(sym.kind){
                    case CC_SYM_VAR:
                        if(!root_declaration && !(declbase->spec.sp_extern && sym.var->has_linkage))
                            return cc_error(p, tok.loc, "redefinition of '%.*s'", name->length, name->data);
                        // Validate declarations before constructing their composite type.
                        var = sym.var;
                        redecl = 1;
                        if(var->thread_local_ != declbase->spec.sp_thread_local)
                            return cc_error(p, name_loc, "conflicting thread-local storage for '%s'", name->data);
                        if((declbase->spec.sp_static && !var->static_) || (var->static_ && !declbase->spec.sp_static && !declbase->spec.sp_extern)){
                            return cc_error(p, tok.loc, "conflicting storage class for '%s': %s; previous declaration has %s",
                                name->data, declbase->spec.sp_static ? "'static'" : "no storage class",
                                var->static_ ? "'static'" : var->extern_ ? "'extern'" : "no storage class"
                            );
                        }
                        if(!declbase->spec.sp_infer_type){
                            err = cc_check_var_type(p, var, &type, tok.loc);
                            if(err) return err;
                            var->type = type;
                        }
                        err = cc_scope_insert_var(cc_allocator(p), cc_scope_declarations(p->current), name, var);
                        if(err) return err;
                        if(asm_label) var->mangle = asm_label;
                        if(decl_alignment > var->alignment) var->alignment = decl_alignment;
                        goto skip_var_alloc;
                    case CC_SYM_FUNC:
                    case CC_SYM_TYPEDEF:
                    case CC_SYM_ENUMERATOR:
                        return cc_error(p, tok.loc, "redefinition of '%.*s' as a different kind of symbol", name->length, name->data);
                }
            }
            var = Allocator_zalloc(cc_allocator(p), 1, sizeof *var, _Alignof(CcVariable));
            if(!var) return CC_OOM_ERROR;
            *var = (CcVariable){
                .name = name,
                .mangle = asm_label,
                .loc = name_loc,
                .type = type,
                .alignment = decl_alignment,
                .extern_ = declbase->spec.sp_extern,
                .static_ = declbase->spec.sp_static,
                .constexpr_ = declbase->spec.sp_constexpr,
                .thread_local_ = declbase->spec.sp_thread_local,
                .register_ = declbase->spec.sp_register,
                .automatic = p->current_func != NULL && !declbase->spec.sp_static && !declbase->spec.sp_extern,
                .has_linkage = root_declaration || declbase->spec.sp_extern,
            };
            err = cc_scope_insert_var(cc_allocator(p), cc_scope_declarations(p->current), name, var);
            if(err) return err;
            skip_var_alloc:;
            if(var->has_linkage && !var->static_){
                err = cc_record_extern(p, name, (CcSymbol){.kind=CC_SYM_VAR, .var=var}, declbase->spec.sp_extern);
                if(err) return err;
            }
        }
        if(tok.type == CC_PUNCTUATOR && tok.punct.punct == '='){
            if(declbase->spec.sp_typedef)
                return cc_error(p, tok.loc, "typedef cannot have an initializer");
            if(declbase->spec.sp_extern && p->current_func)
                return cc_error(p, tok.loc, "block-scope extern declaration cannot have an initializer");
            CcParseFlags init_flags = flags;
            init_flags.vc = CC_RUNTIME_VALUE;
            if(declbase->spec.sp_constexpr)
                init_flags.vc = CC_CONSTEXPR_VALUE;
            else if(var && !var->automatic && (var->static_ || var->thread_local_))
                init_flags.vc = CC_LINKTIME_VALUE;
            err = cc_parse_assignment_expr(p, init_flags, &initializer, type);
            if(err) return err;
            if(!initializer) return cc_error(p, tok.loc, "Expected expression after '='");
            err = cc_next_token(p, &tok);
            if(err) return err;
        }
        if(initializer){
            if(initializer->kind == CC_EXPR_INIT_LIST){
                // Init list type-checking is done during parsing.
                // For incomplete arrays, update the variable type from the resolved init list type.
                if(ccqt_array_incomplete(type)){
                    uintptr_t quals = type.quals;
                    type = initializer->type;
                    type.quals |= quals;
                }
            }
            else if(declbase->spec.sp_infer_type){
                // Infer type from initializer
                type = initializer->type;
                if(!ccqt_is_array(type)) type.quals = 0;
                // Decay array to pointer, function to function pointer
                if(ccqt_is_array(type)){
                    err = cc_pointer_of(p, ccqt_qualified_array_element(type), &type);
                    if(err) return err;
                    err = cc_implicit_cast(p, initializer, type, &initializer);
                    if(err) return err;
                }
                else if(ccqt_kind(type) == CC_FUNCTION){
                    err = cc_pointer_of(p, type, &type);
                    if(err) return err;
                    err = cc_implicit_cast(p, initializer, type, &initializer);
                    if(err) return err;
                }
                // Apply qualifiers from specifier
                if(declbase->spec.sp_const || declbase->spec.sp_constexpr) type.is_const = 1;
                if(declbase->spec.sp_volatile) type.is_volatile = 1;
                if(declbase->spec.sp_atomic) type.is_atomic = 1;
            }
            else if(ccqt_kind(type) == CC_ARRAY
                    && ccqt_kind(initializer->type) == CC_ARRAY
                    && initializer->kind == CC_EXPR_VALUE && initializer->text){
                // String literal initializing a char array.
                CcArray* target_arr = ccqt_as_array(type);
                CcArray* init_arr = ccqt_as_array(initializer->type);
                err = cc_check_string_array(p, type, initializer);
                if(err) return err;
                if(target_arr->is_incomplete){
                    // char s[] = "abc" -> size from string literal.
                    err = cc_complete_array_type(p, type, init_arr->length, &type);
                    if(err) return err;
                }
                else if(target_arr->length < init_arr->length - 1)
                    return cc_error(p, tok.loc, "initializer string too long for array");
                else if(target_arr->length < init_arr->length)
                    // Truncate string literal if needed.
                    initializer->type = type;
            }
            else {
                // Check compatibility and insert implicit cast
                err = cc_check_atomic_object_type(p, type, tok.loc);
                if(err) return err;
                CcQualType target = {.unqual=type.unqual};
                err = cc_implicit_cast(p, initializer, target, &initializer);
                if(err) return err;
            }
        }
        else if(declbase->spec.sp_infer_type){
            return cc_error(p, tok.loc, "type-inferred declaration requires initializer");
        }
        if(declbase->spec.sp_infer_type){
            err = cc_check_decl_type_qualifiers(p, type, declbase->spec, declbase->loc);
            if(err) return err;
        }
        if(declbase->spec.sp_constexpr){
            if(!initializer)
                return cc_error(p, name_loc, "constexpr object requires an initializer");
            if(cc_constexpr_forbidden_qualifiers(type))
                return cc_type_error(p, name_loc, type, "constexpr object cannot have volatile or atomic subobjects");
        }
        err = cc_check_decl_alignment(p, declbase, type, 0);
        if(err) return err;
        if(tok.type == CC_EOF || (tok.type == CC_PUNCTUATOR && (tok.punct.punct == ';' || tok.punct.punct == ')'))){
            err = cc_unget(p, &tok);
            if(err) return err;
            stop = 1;
        }
        else if(tok.type == CC_PUNCTUATOR && tok.punct.punct == ','){
        }
        else
            return cc_error(p, tok.loc, "Expected ',' or ';'");
        out->last_object = var;
        if(!name){
            if(stop) break;
            continue;
        }
        if(declbase->spec.sp_typedef){
            CcSymbol sym;
            for(size_t i = 0; i < param_names.names.count; i++){
                CcExpr* value = param_names.names.data[i].default_value;
                if(value) return cc_error(p, value->loc, "default arguments are not allowed in typedefs");
            }
            if(param_names.typed_pack_loc.bits)
                return cc_error(p, param_names.typed_pack_loc, "typed varargs require a direct function declaration");
            _Bool found = cc_scope_lookup_symbol(cc_scope_declarations(p->current), name, CC_SCOPE_NO_WALK, &sym);
            if(found){
                switch(sym.kind){
                    case CC_SYM_TYPEDEF:
                        if(ccqt_is_vm(sym.type) || ccqt_is_vm(type))
                            return cc_error(p, tok.loc, "redefinition of variably modified typedef '%.*s'", name->length, name->data);
                        if(!cc_types_same(sym.type, type)){
                            cpp_include_backtrace(&p->cpp, LOG_PRINT_ERROR);
                            cpp_msg_preamble(&p->cpp, tok.loc, "error");
                            MStringBuilder* buff = &p->cpp.logger->buff;
                            msb_sprintf(buff, "redefinition of typedef '%.*s' as '", name->length, name->data);
                            cc_print_type(buff, type);
                            msb_write_literal(buff, "'; previously defined as '");
                            cc_print_type(buff, sym.type);
                            msb_write_char(buff, '\'');
                            log_flush(p->cpp.logger, LOG_PRINT_ERROR);
                            cpp_msg_postamble(&p->cpp, tok.loc, LOG_PRINT_ERROR);
                            return CC_SYNTAX_ERROR;
                        }
                        break;
                    case CC_SYM_VAR:
                    case CC_SYM_FUNC:
                    case CC_SYM_ENUMERATOR:
                        return cc_error(p, tok.loc, "redefinition of '%.*s' as a different kind of symbol", name->length, name->data);
                }
            }
            if(!found){
                err = cc_scope_insert_typedef(cc_allocator(p), cc_scope_declarations(p->current), name, type, name_loc);
                if(err) return err;
            }
        }
        else if(is_func_decl){
            CcSymbol sym;
            CcFunc* func = NULL;
            _Bool found = cc_scope_lookup_symbol(cc_scope_declarations(p->current), name, CC_SCOPE_NO_WALK, &sym);
            _Bool root_declaration = cc_scope_declarations(p->current) == &p->module->scope;
            if(!found && !declbase->spec.sp_static){
                CcScope *last = declbase->spec.sp_extern ? NULL : &p->module->scope;
                found = cc_lookup_visible_linked_symbol(p->current, last, name, &sym);
                if(!found)
                    found = cc_lookup_external_entity(p, name, declbase->spec.sp_extern, &sym);
            }
            if(found){
                switch(sym.kind){
                    case CC_SYM_FUNC:
                        func = sym.func;
                        break;
                    case CC_SYM_VAR:
                    case CC_SYM_TYPEDEF:
                    case CC_SYM_ENUMERATOR:
                        return cc_error(p, tok.loc, "redefinition of '%.*s' as a different kind of symbol", name->length, name->data);
                }
            }
            _Bool keep_prototype = func && !func->type->no_prototype && ccqt_as_function(type)->no_prototype;
            if(func){
                err = cc_check_func_compat(p, func, declbase, type, 0, tok.loc);
                if(err) return err;
                err = cc_check_pack_decl(p, func, &param_names, type, tok.loc);
                if(err) return err;
                err = cc_merge_func_decl_type(p, func->type, &type);
                if(err) return err;
            }
            else {
                func = Allocator_zalloc(cc_allocator(p), 1, sizeof *func, _Alignof(CcFunc));
                if(!func) return CC_OOM_ERROR;
                func->name = name;
                func->loc = name_loc;
                func->has_linkage = root_declaration || !declbase->spec.sp_static;
            }
            err = cc_scope_insert_func(cc_allocator(p), cc_scope_declarations(p->current), name, func);
            if(err) return err;
            func->type = ccqt_as_function(type);
            if(asm_label) func->mangle = asm_label;
            if(declbase->spec.sp_extern || declbase->spec.sp_static){
                func->extern_ = declbase->spec.sp_extern;
                func->static_ |= declbase->spec.sp_static;
            }
            if(func->has_linkage && !func->static_){
                err = cc_record_extern(p, name, (CcSymbol){.kind=CC_SYM_FUNC, .func=func}, declbase->spec.sp_extern);
                if(err) return err;
            }
            func->inline_ = func->inline_ || declbase->spec.sp_inline;
            func->always_inline = func->always_inline || is_always_inline;
            func->noinline = func->noinline || is_noinline;
            func->noreturn_ = func->noreturn_ || is_noreturn;
            func->hotswappable = func->hotswappable || is_hotswappable;
            err = cc_apply_printf_formats(p, func, printf_formats);
            if(err) return err;
            err = cc_merge_default_args(p, func, &param_names, !func->defined && !keep_prototype, name_loc);
            if(err) return err;
        }
        else {
            if(!initializer && !declbase->spec.sp_extern){
                CcTypeKind tk = ccqt_kind(type);
                if(tk == CC_BASIC && ccqt_basic_kind(type) == CCBT_void)
                    return cc_error(p, tok.loc, "variable has incomplete type 'void'");
                if(tk == CC_ARRAY && ccqt_as_array(type)->is_incomplete && p->current_func)
                    return cc_type_error(p, tok.loc, type, "variable '%.*s' has incomplete array type", name->length, name->data);
                if(tk == CC_STRUCT && ccqt_as_struct(type)->is_incomplete)
                    return cc_error(p, tok.loc, "variable has incomplete type 'struct %s'", ccqt_as_struct(type)->name ? ccqt_as_struct(type)->name->data : "(anonymous)");
                if(tk == CC_UNION && ccqt_as_union(type)->is_incomplete)
                    return cc_error(p, tok.loc, "variable has incomplete type 'union %s'", ccqt_as_union(type)->name ? ccqt_as_union(type)->name->data : "(anonymous)");
                if(tk == CC_ENUM && ccqt_as_enum(type)->is_incomplete)
                    return cc_type_error(p, tok.loc, type, "variable has incomplete enum type");
            }
            if(var){
                if(initializer && (var->initializer || (var->extern_ && var->interp_val)))
                    return cc_error(p, tok.loc, "redefinition of '%.*s'", name->length, name->data);
                if(redecl && declbase->spec.sp_infer_type){
                    err = cc_check_var_type(p, var, &type, tok.loc);
                    if(err) return err;
                }
                // An initialized definition supersedes tentative definitions.
                // Otherwise keep the first declaration that provides storage.
                if(initializer || (var->extern_ && !var->initializer && !declbase->spec.sp_extern))
                    var->loc = name_loc;
                if(!declbase->spec.sp_extern)
                    var->extern_ = 0;
                var->type = type;
                if(initializer)
                    var->initializer = initializer;
                if(initializer && !var->constexpr_ && ccqt_kind(type) != CC_VLA){
                    CcQualType object_type = ccqt_array_object_type(type);
                    if(object_type.is_const && !object_type.is_volatile && !object_type.is_atomic){
                        int constant_err = cc_check_linktime_expr(p, initializer, 0, 0);
                        if(constant_err && constant_err != CC_NOT_CONSTANT_ERROR) return constant_err;
                        if(!constant_err) var->constexpr_ = 1;
                    }
                }
                if(initializer && (var->static_ || var->constexpr_ || var->thread_local_)){
                    err = cc_check_linktime_expr(p, initializer, 0, 0);
                    if(err == CC_NOT_CONSTANT_ERROR)
                        return cc_error(p, initializer->loc, "%s initializer requires a link-time constant", var->constexpr_ ? "constexpr" : var->thread_local_ ? "thread_local" : "static");
                    if(err) return err;
                }
                if(var->constexpr_ && ccqt_bt_eq(type, CCBT__Type)){
                    CcExpr* value;
                    err = cc_eval_expr(&(CcEvalCtx){.parser = p}, initializer, &value);
                    if(err) return err == CC_NOT_CONSTANT_ERROR ? cc_error(p, initializer->loc, "constexpr _Type initializer is not a constant expression") : err;
                    CcQualType defined_type = value->type_value;
                    cc_release_expr(p, value);
                    if(defined_type.bits){
                        err = cc_scope_insert_typedef(cc_allocator(p), cc_scope_declarations(p->current), var->name, defined_type, var->loc);
                        if(err) return err;
                    }
                }
            }
            if(var && ccqt_kind(type) == CC_VLA){
                if(initializer && (initializer->kind != CC_EXPR_INIT_LIST || initializer->init_list->count))
                    return cc_error(p, name_loc, "VLA initializer must be {} or {0}");
                CcVariable* mark = Allocator_zalloc(cc_allocator(p), 1, sizeof *mark, _Alignof(CcVariable));
                if(!mark) return CC_OOM_ERROR;
                *mark = (CcVariable){
                    .name=nil_atom,
                    .loc=name_loc,
                    .type=p->stack_mark_type,
                    .automatic=var->automatic,
                };
                err = cc_scope_insert_var(cc_allocator(p), cc_scope_declarations(p->current), nil_atom, mark);
                if(err) return err;
                CcStmtNode* allocation = cc_stmt_node(p, CC_STMT_VLA, name_loc, 0);
                if(!allocation) return CC_OOM_ERROR;
                allocation->vla.object = var;
                allocation->vla.mark = mark;
                allocation->zero_initialize = initializer != NULL;
                initializer = NULL;
                p->defer_context.active->stack_mark = mark;
                err = cc_sink_push(p, allocation);
                if(err) return err;
            }
            if(initializer){
                if(var && (var->static_ || var->thread_local_ || (var->constexpr_ && !var->automatic))){
                    var->interp_preinit = 1;
                    if(initializer->kind == CC_EXPR_COMPOUND_LITERAL)
                        initializer->kind = CC_EXPR_INIT_LIST;
                }
                else {
                    if(var && !var->automatic && p->current == &p->module->scope)
                        var->interp_try_preinit = 1;
                    CcExpr* var_ref = cc_make_expr(p, CC_EXPR_VARIABLE, tok.loc, type, 0);
                    if(!var_ref) return CC_OOM_ERROR;
                    var_ref->is_lvalue = 1;
                    var_ref->var = var;
                    if(initializer->kind == CC_EXPR_COMPOUND_LITERAL)
                        initializer->kind = CC_EXPR_INIT_LIST;
                    CcExpr* assign = cc_binary_expr(p, CC_EXPR_ASSIGN, tok.loc, type, var_ref, initializer);
                    if(!assign) return CC_OOM_ERROR;
                    CcStmtNode* node = cc_stmt_node(p, CC_STMT_EXPR, tok.loc, 0);
                    if(!node){
                        cc_release_expr(p, assign);
                        return CC_OOM_ERROR;
                    }
                    node->exprs[0] = assign;
                    err = cc_sink_push(p, node);
                    if(err) return err;
                }
            }
        }
        if(stop) break;
    }
    return cc_parse_local_methods(p);
    DRP_PROFILE_END(profile);
}
static
const CcTargetConfig*
cc_target(const CcParser* p){
    return p->cpp.target;
}

static
int
cc_handle_static_assert(CcParser* p, CcParseFlags flags){
    flags.vc = CC_CONSTEXPR_VALUE;
    flags.unevaluated = 1;
    CcToken tok;
    int err;
    err = cc_next_token(p, &tok);
    if(err) return err;
    if(tok.type != CC_KEYWORD || tok.kw.kw != CC_static_assert)
        return ((void)cc_error(p, tok.loc, "ICE, handling static assert, but not on a static assert token"), CC_UNREACHABLE_ERROR);
    SrcLoc assert_loc = tok.loc;
    err = cc_expect_punct(p, CC_lparen);
    if(err) return err;
    CcExpr* expr = NULL;
    err = cc_parse_assignment_expr(p, flags, &expr, CCQT_NONE);
    if(err) return err;
    if(!expr)
        return cc_error(p, assert_loc, "expected expression in static_assert");
    err = cc_decay(p, &expr);
    if(err) return err;
    _Bool sa_truthy;
    err = cc_eval_truthy(&(CcEvalCtx){p}, expr, &sa_truthy);
    if(err){
        cc_release_expr(p, expr);
        if(err != CC_NOT_CONSTANT_ERROR) return err;
        return cc_error(p, assert_loc, "static_assert expression is not a constant expression");
    }
    // Check for optional message.
    const char* msg = NULL;
    size_t msg_len = 0;
    err = cc_peek(p, &tok);
    if(err) return err;
    if(tok.type == CC_PUNCTUATOR && tok.punct.punct == CC_comma){
        cc_next_token(p, &tok); // consume comma
        err = cc_next_token(p, &tok);
        if(err) return err;
        if(tok.type != CC_STRING_LITERAL)
            return cc_error(p, tok.loc, "expected string literal in static_assert");
        msg = tok.str.utf8;
        msg_len = tok.str.length;
    }
    err = cc_expect_punct(p, CC_rparen);
    if(err) return err;
    if(!sa_truthy){
        MStringBuilder tmp = {.allocator = allocator_from_arena(&p->scratch_arena)};
        msb_write_literal(&tmp, "static assertion failed: ");
        cc_print_expr(&tmp, expr);
        cc_release_expr(p, expr);
        if(msg){
            msb_write_literal(&tmp, ": \"");
            msb_write_str(&tmp, msg, msg_len - 1);
            msb_write_literal(&tmp, "\"");
        }
        StringView sv = msb_borrow_sv(&tmp);
        cc_error(p, assert_loc, "%.*s", sv_p(sv));
        return CC_SYNTAX_ERROR;
    }
    cc_release_expr(p, expr);
    return 0;
}

static
int
cc_register_extern_var(CcParser* p, StringView name, CcQualType type){
    Allocator al = cc_allocator(p);
    Atom a = AT_atomize(p->cpp.at, name.text, name.length);
    if(!a) return CC_OOM_ERROR;
    CcVariable* var = Allocator_zalloc(al, 1, sizeof *var, _Alignof(CcVariable));
    if(!var) return CC_OOM_ERROR;
    var->name = a;
    var->type = type;
    var->extern_ = 1;
    int err = cc_scope_insert_var(al, &p->root->scope, a, var);
    if(err) Allocator_free(al, var, 1, sizeof *var, _Alignof(CcVariable));
    return err;
}

static
int
cc_has_builtin(void* _Null_unspecified ctx, CppPreprocessor* cpp, SrcLoc loc, CppTokens* outtoks, const CppTokens* args, const Marray(size_t)* arg_seps){
    (void)arg_seps;
    CcParser* p = ctx;
    // Find the identifier token in the args.
    StringView name = {0};
    for(size_t i = 0; i < args->count; i++){
        CppToken tok = args->data[i];
        if(tok.type == CPP_WHITESPACE || tok.type == CPP_NEWLINE) continue;
        if(tok.type == CPP_IDENTIFIER){
            name = tok.txt;
            break;
        }
        return cpp_error(cpp, tok.loc, "Expected identifier in __has_builtin");
    }
    if(!name.text)
        return cpp_error(cpp, loc, "Expected identifier in __has_builtin");
    _Bool found = 0;
    Atom a = AT_get_atom(cpp->at, name.text, name.length);
    if(a)
        found = AM_get(&p->builtins, a) != 0;
    CppToken result = {
        .type = CPP_NUMBER,
        .txt = found ? SV("1") : SV("0"),
        .loc = loc,
    };
    return cpp_push_tok(cpp, outtoks, result);
}

static
int
cc_feature_query(CppPreprocessor* cpp, SrcLoc loc, CppTokens* outtoks, const CppTokens* args, _Bool extension){
    StringView name = {0};
    for(size_t i = 0; i < args->count; i++){
        CppToken tok = args->data[i];
        if(tok.type == CPP_WHITESPACE || tok.type == CPP_NEWLINE) continue;
        if(tok.type != CPP_IDENTIFIER || name.length)
            return cpp_error(cpp, loc, "%s() requires a single identifier", extension?"__has_extension":"__has_feature");
        name = tok.txt;
    }
    if(!name.length)
        return cpp_error(cpp, loc, "%s() requires a single identifier", extension?"__has_extension":"__has_feature");
    if(name.length > 4 && sv_startswith(name, SV("__")) && sv_endswith(name, SV("__")))
        name = sv_slice(name, 2, name.length-2);
    static const struct {
        StringView name;
        _Bool extension_only;
    } supported[] = {
        {SVI("c_alignas"), 0},
        {SVI("c_alignof"), 0},
        {SVI("c_atomic"), 0},
        {SVI("c_generic_selections"), 0},
        {SVI("c_static_assert"), 0},
        {SVI("c_thread_local"), 0},
        {SVI("c_fixed_enum"), 0},
        {SVI("c_countof"), 0},
        {SVI("tls"), 0},
        {SVI("enumerator_attributes"), 0},
        {SVI("c_attributes"), 1},
        {SVI("c_generic_selection_with_controlling_type"), 1},
        {SVI("cxx_fixed_enum"), 1},
        {SVI("cxx_binary_literals"), 1},
    };
    _Bool found = 0;
    for(size_t i = 0; i < countof(supported); i++){
        if(sv_equals(name, supported[i].name)){
            found = extension || !supported[i].extension_only;
            break;
        }
    }
    CppToken result = {
        .type = CPP_NUMBER,
        .txt = found ? SV("1") : SV("0"),
        .loc = loc,
    };
    return cpp_push_tok(cpp, outtoks, result);
}

static
int
cc_has_feature(void* _Null_unspecified ctx, CppPreprocessor* cpp, SrcLoc loc, CppTokens* outtoks, const CppTokens* args, const Marray(size_t)* arg_seps){
    (void)ctx;
    (void)arg_seps;
    return cc_feature_query(cpp, loc, outtoks, args, 0);
}

static
int
cc_has_extension(void* _Null_unspecified ctx, CppPreprocessor* cpp, SrcLoc loc, CppTokens* outtoks, const CppTokens* args, const Marray(size_t)* arg_seps){
    (void)ctx;
    (void)arg_seps;
    return cc_feature_query(cpp, loc, outtoks, args, 1);
}

static
int
cc_define_builtin_types(CcParser* p){
    int err;
    Allocator al = cc_allocator(p);
    const CcTargetConfig *t = p->cpp.target;

    {
        err = cc_pointer_of(p, ccqt_basic(CCBT_void), &p->void_star);
        if(err) return err;
        CcQualType const_void = ccqt_basic(CCBT_void);
        const_void.is_const = 1;
        err = cc_pointer_of(p, const_void, &p->const_void_star);
        if(err) return err;
        err = cc_pointer_of(p, ccqt_basic(CCBT_char), &p->char_star);
        if(err) return err;
        CcQualType const_char = ccqt_basic(CCBT_char);
        const_char.is_const = 1;
        err = cc_pointer_of(p, const_char, &p->const_char_star);
        if(err) return err;
        err = cc_slice_of(p, ccqt_basic(CCBT_char), (SrcLoc){0}, &p->char_slice);
        if(err) return err;
        err = cc_slice_of(p, const_char, (SrcLoc){0}, &p->const_char_slice);
        if(err) return err;
        CcArray* mark_type = cc_intern_array(&p->type_cache, al, ccqt_basic(t->size_type), 2, 0);
        if(!mark_type) return CC_OOM_ERROR;
        p->stack_mark_type = (CcQualType){.bits=(uintptr_t)mark_type};
    }
    Atom va_list_name = AT_atomize(p->cpp.at, "__builtin_va_list", sizeof "__builtin_va_list"-1);
    if(!va_list_name) return CC_OOM_ERROR;
    Atom gnu_va_list = AT_atomize(p->cpp.at, "__gnuc_va_list", sizeof "__gnuc_va_list" - 1);
    if(!gnu_va_list) return CC_OOM_ERROR;
    CcQualType va_list_type;

    switch(t->target){
        case CC_TARGET_X86_64_LINUX:
        case CC_TARGET_X86_64_MACOS: {
            // struct __va_list_tag { unsigned gp_offset; unsigned fp_offset;
            //                       void *overflow_arg_area; void *reg_save_area; };
            Atom tag_name = AT_atomize(p->cpp.at, "__va_list_tag", sizeof "__va_list_tag" -1);
            if(!tag_name) return CC_OOM_ERROR;

            Atom gp_name = AT_atomize(p->cpp.at, "gp_offset", sizeof "gp_offset" - 1);
            Atom fp_name = AT_atomize(p->cpp.at, "fp_offset", sizeof "fp_offset" - 1);
            Atom oa_name = AT_atomize(p->cpp.at, "overflow_arg_area", sizeof "overflow_arg_area" -1);
            Atom rs_name = AT_atomize(p->cpp.at, "reg_save_area", sizeof "reg_save_area"-1);
            if(!gp_name || !fp_name || !oa_name || !rs_name) return CC_OOM_ERROR;

            CcField* fields = Allocator_alloc(al, 4, sizeof *fields, _Alignof(CcField));
            if(!fields) return CC_OOM_ERROR;
            fields[0] = (CcField){.type = ccqt_basic(CCBT_unsigned), .name = gp_name};
            fields[1] = (CcField){.type = ccqt_basic(CCBT_unsigned), .name = fp_name};
            fields[2] = (CcField){.type = p->void_star, .name = oa_name};
            fields[3] = (CcField){.type = p->void_star, .name = rs_name};

            CcStruct* s = Allocator_zalloc(al, 1, sizeof *s, _Alignof(CcStruct));
            if(!s) return CC_OOM_ERROR;
            *s = (CcStruct){
                .kind = CC_STRUCT,
                .name = tag_name,
                .field_count = 4,
                .fields = fields,
            };
            err = cc_compute_struct_layout(p, s, 0);
            if(err) return err;
            err = cc_scope_insert_struct_tag(al, &p->root->scope, tag_name, s);
            if(err) return CC_OOM_ERROR;

            // typedef __va_list_tag __builtin_va_list[1];
            CcQualType struct_type = {.bits = (uintptr_t)s};
            CcArray* arr = cc_intern_array(&p->type_cache, al, struct_type, 1, 0);
            if(!arr) return CC_OOM_ERROR;
            va_list_type = (CcQualType){.bits = (uintptr_t)arr};
            break;
        }
        case CC_TARGET_AARCH64_LINUX: {
            // struct __va_list { void *__stack; void *__gr_top; void *__vr_top;
            //                    int __gr_offs; int __vr_offs; };
            Atom tag_name = AT_atomize(p->cpp.at, "__va_list", sizeof "__va_list" - 1);
            if(!tag_name) return CC_OOM_ERROR;

            Atom stack_name = AT_atomize(p->cpp.at, "__stack", sizeof "__stack" - 1);
            Atom gr_top_name = AT_atomize(p->cpp.at, "__gr_top", sizeof "__gr_top" - 1);
            Atom vr_top_name = AT_atomize(p->cpp.at, "__vr_top", sizeof "__vr_top" - 1);
            Atom gr_offs_name = AT_atomize(p->cpp.at, "__gr_offs", sizeof "__gr_offs" - 1);
            Atom vr_offs_name = AT_atomize(p->cpp.at, "__vr_offs", sizeof "__vr_offs" - 1);
            if(!stack_name || !gr_top_name || !vr_top_name || !gr_offs_name || !vr_offs_name)
                return CC_OOM_ERROR;

            CcField* fields = Allocator_alloc(al, 5, sizeof *fields, _Alignof(CcField));
            if(!fields) return CC_OOM_ERROR;
            fields[0] = (CcField){.type = p->void_star, .name = stack_name};
            fields[1] = (CcField){.type = p->void_star, .name = gr_top_name};
            fields[2] = (CcField){.type = p->void_star, .name = vr_top_name};
            fields[3] = (CcField){.type = ccqt_basic(CCBT_int), .name = gr_offs_name};
            fields[4] = (CcField){.type = ccqt_basic(CCBT_int), .name = vr_offs_name};

            CcStruct* s = Allocator_zalloc(al, 1, sizeof *s, _Alignof(CcStruct));
            if(!s) return CC_OOM_ERROR;
            *s = (CcStruct){
                .kind = CC_STRUCT,
                .name = tag_name,
                .field_count = 5,
                .fields = fields,
            };
            err = cc_compute_struct_layout(p, s, 0);
            if(err) return err;
            err = cc_scope_insert_struct_tag(al, &p->root->scope, tag_name, s);
            if(err) return CC_OOM_ERROR;

            // typedef struct __va_list __builtin_va_list;
            va_list_type = (CcQualType){.bits = (uintptr_t)s};
            break;
        }
        case CC_TARGET_AARCH64_MACOS:
        case CC_TARGET_TEST: {
            // typedef void *__builtin_va_list;
            va_list_type = p->void_star;
            break;
        }
        case CC_TARGET_X86_64_WINDOWS_MSVC:
        case CC_TARGET_X86_64_WINDOWS_MINGW: {
            // typedef char *__builtin_va_list
            va_list_type = p->char_star;
            break;
        }
        case CC_TARGET_COUNT:
            return CC_UNREACHABLE_ERROR;
        DRP_CASES_EXHAUSTED;
    }
    err = cc_scope_insert_typedef(al, &p->root->scope, va_list_name, va_list_type, (SrcLoc){0});
    if(err) return CC_OOM_ERROR;
    err = cc_scope_insert_typedef(al, &p->root->scope, gnu_va_list, va_list_type, (SrcLoc){0});
    if(err) return CC_OOM_ERROR;
    p->builtin_va_list = va_list_type;
    if(ccqt_kind(va_list_type) == CC_ARRAY){
        // Array decays to pointer-to-element.
        err = cc_pointer_of(p, ccqt_array_element(va_list_type), &p->builtin_va_list_ptr);
        if(err) return err;
    }
    else {
        // Non-array: pointer to the va_list type.
        err = cc_pointer_of(p, va_list_type, &p->builtin_va_list_ptr);
        if(err) return err;
    }

    {
        struct f {StringView name; CcQualType type; size_t offset;} fieldinfos[] = {
            {SVI("type"), {.basic.kind=CCBT__Type}, offsetof(CiRtField, type)},
            {SVI("name"), p->const_char_slice, offsetof(CiRtField, name)},
            {SVI("offset"), {.basic.kind=CCBT_unsigned}, offsetof(CiRtField, offset)},
            {SVI("bitwidth"), {.basic.kind=CCBT_unsigned}, offsetof(CiRtField, bitwidth)},
            {SVI("bitoffset"), {.basic.kind=CCBT_unsigned}, offsetof(CiRtField, bitoffset)},
            {SVI("is_bitfield"), {.basic.kind=CCBT_unsigned}, offsetof(CiRtField, is_bitfield)},
        };
        CcField* fields = Allocator_zalloc(al, (sizeof fieldinfos / sizeof fieldinfos[0]), sizeof *fields, _Alignof(CcField));
        if(!fields) return CC_OOM_ERROR;
        for(size_t i = 0; i < sizeof fieldinfos / sizeof fieldinfos[0]; i++){
            struct f* f = &fieldinfos[i];
            Atom a = AT_atomize(p->cpp.at, f->name.text, f->name.length);
            if(!a) return CC_OOM_ERROR;
            CcField* field = &fields[i];
            field->type = f->type;
            field->name = a;
            field->offset = (unsigned)f->offset;
        }
        Atom name = AT_ATOMIZE(p->cpp.at, "__builtin_Field");
        if(!name) return CC_OOM_ERROR;
        CcStruct* s = Allocator_zalloc(al, 1, sizeof *s, _Alignof(CcStruct));
        if(!s) return CC_OOM_ERROR;
        *s = (CcStruct){
            .kind = CC_STRUCT,
            .name = name,
            .field_count = sizeof fieldinfos / sizeof fieldinfos[0],
            .fields = fields,
        };
        err = cc_compute_struct_layout(p, s, 0);
        if(err) return err;
        err = cc_scope_insert_struct_tag(al, &p->root->scope, name, s);
        if(err) return CC_OOM_ERROR;
        p->builtin_field = (CcQualType){.bits = (uintptr_t)s};
        err = cc_scope_insert_typedef(al, &p->root->scope, name, p->builtin_field, (SrcLoc){0});
        if(err) return CC_OOM_ERROR;
    }

    {
        struct f {StringView name; CcQualType type; size_t offset;} methodinfos[] = {
            {SVI("type"), {.basic.kind=CCBT__Type}, offsetof(CiRtMethod, type)},
            {SVI("name"), p->const_char_slice, offsetof(CiRtMethod, name)},
            {SVI("offset"), ccqt_basic(cc_target(p)->size_type), offsetof(CiRtMethod, offset)},
            {SVI("address"), ccqt_basic(cc_target(p)->size_type), offsetof(CiRtMethod, address)},
        };
        CcField* fields = Allocator_zalloc(al, (sizeof methodinfos / sizeof methodinfos[0]), sizeof *fields, _Alignof(CcField));
        if(!fields) return CC_OOM_ERROR;
        for(size_t i = 0; i < sizeof methodinfos / sizeof methodinfos[0]; i++){
            struct f* f = &methodinfos[i];
            Atom a = AT_atomize(p->cpp.at, f->name.text, f->name.length);
            if(!a) return CC_OOM_ERROR;
            fields[i] = (CcField){.type = f->type, .name = a, .offset = (unsigned)f->offset};
        }
        Atom name = AT_ATOMIZE(p->cpp.at, "__builtin_Method");
        if(!name) return CC_OOM_ERROR;
        CcStruct* s = Allocator_zalloc(al, 1, sizeof *s, _Alignof(CcStruct));
        if(!s) return CC_OOM_ERROR;
        *s = (CcStruct){
            .kind = CC_STRUCT,
            .name = name,
            .field_count = sizeof methodinfos / sizeof methodinfos[0],
            .fields = fields,
        };
        err = cc_compute_struct_layout(p, s, 0);
        if(err) return err;
        err = cc_scope_insert_struct_tag(al, &p->root->scope, name, s);
        if(err) return CC_OOM_ERROR;
        p->builtin_method = (CcQualType){.bits = (uintptr_t)s};
        err = cc_scope_insert_typedef(al, &p->root->scope, name, p->builtin_method, (SrcLoc){0});
        if(err) return CC_OOM_ERROR;
    }

    {
        struct f {StringView name; CcQualType type; size_t offset;} enuminfos[] = {
            {SVI("name"), p->const_char_slice, offsetof(CiRtEnumerator, name)},
            {SVI("value"), ccqt_basic(CCBT_unsigned_int128), offsetof(CiRtEnumerator, value)},
            {SVI("type"), ccqt_basic(CCBT__Type), offsetof(CiRtEnumerator, type)},
        };
        CcField* fields = Allocator_zalloc(al, countof(enuminfos), sizeof *fields, _Alignof(CcField));
        if(!fields) return CC_OOM_ERROR;
        for(size_t i = 0; i < sizeof enuminfos / sizeof enuminfos[0]; i++){
            struct f* f = &enuminfos[i];
            Atom a = AT_atomize(p->cpp.at, f->name.text, f->name.length);
            if(!a) return CC_OOM_ERROR;
            CcField* field = &fields[i];
            field->type = f->type;
            field->name = a;
            field->offset = (unsigned)f->offset;
        }
        Atom name = AT_ATOMIZE(p->cpp.at, "__builtin_Enumerator");
        if(!name) return CC_OOM_ERROR;
        CcStruct* s = Allocator_zalloc(al, 1, sizeof *s, _Alignof(CcStruct));
        if(!s) return CC_OOM_ERROR;
        *s = (CcStruct){
            .kind = CC_STRUCT,
            .name = name,
            .field_count = sizeof enuminfos / sizeof enuminfos[0],
            .fields = fields,
        };
        err = cc_compute_struct_layout(p, s, 0);
        if(err) return err;
        err = cc_scope_insert_struct_tag(al, &p->root->scope, name, s);
        if(err) return CC_OOM_ERROR;
        p->builtin_enumerator = (CcQualType){.bits = (uintptr_t)s};
        err = cc_scope_insert_typedef(al, &p->root->scope, name, p->builtin_enumerator, (SrcLoc){0});
        if(err) return CC_OOM_ERROR;
    }
    {
        Atom name = AT_ATOMIZE(p->cpp.at, "__builtin_SrcLoc");
        if(!name) return CC_OOM_ERROR;
        CcStruct* s = Allocator_zalloc(al, 1, sizeof *s, _Alignof(CcStruct));
        if(!s) return CC_OOM_ERROR;
        *s = (CcStruct){
            .kind = CC_STRUCT,
            .is_incomplete = 1,
            .name = name,
        };
        err = cc_scope_insert_struct_tag(al, &p->root->scope, name, s);
        if(err) return CC_OOM_ERROR;
        CcQualType struct_ = {.bits = (uintptr_t)s};
        err = cc_pointer_of(p, struct_, &p->builtin_src_loc);
        if(err) return err;
        Atom typedef_ = AT_ATOMIZE(p->cpp.at, "_SrcLoc");
        if(!typedef_) return CC_OOM_ERROR;
        err = cc_scope_insert_typedef(al, &p->root->scope, typedef_, p->builtin_src_loc, (SrcLoc){0});
        if(err) return CC_OOM_ERROR;
    }
    {
        struct f {StringView name; CcQualType type; size_t offset;} memberinfos[] = {
            {SVI("type"), ccqt_basic(CCBT__Type), offsetof(CiRtModuleMember, type)},
            {SVI("name"), p->const_char_slice, offsetof(CiRtModuleMember, name)},
            {SVI("address"), p->void_star, offsetof(CiRtModuleMember, address)},
            {SVI("srcloc"), p->builtin_src_loc, offsetof(CiRtModuleMember, loc)},
        };
        CcField* fields = Allocator_zalloc(al, countof(memberinfos), sizeof *fields, _Alignof(CcField));
        if(!fields) return CC_OOM_ERROR;
        for(size_t i = 0; i < sizeof memberinfos / sizeof memberinfos[0]; i++){
            struct f* f = &memberinfos[i];
            Atom a = AT_atomize(p->cpp.at, f->name.text, f->name.length);
            if(!a) return CC_OOM_ERROR;
            CcField* field = &fields[i];
            field->type = f->type;
            field->name = a;
            field->offset = (unsigned)f->offset;
        }
        Atom name = AT_ATOMIZE(p->cpp.at, "__builtin_ModuleMember");
        if(!name) return CC_OOM_ERROR;
        CcStruct* s = Allocator_zalloc(al, 1, sizeof *s, _Alignof(CcStruct));
        if(!s) return CC_OOM_ERROR;
        *s = (CcStruct){
            .kind = CC_STRUCT,
            .name = name,
            .field_count = sizeof memberinfos / sizeof memberinfos[0],
            .fields = fields,
        };
        err = cc_compute_struct_layout(p, s, 0);
        if(err) return err;
        err = cc_scope_insert_struct_tag(al, &p->root->scope, name, s);
        if(err) return CC_OOM_ERROR;
        p->builtin_module_member = (CcQualType){.bits = (uintptr_t)s};
        err = cc_scope_insert_typedef(al, &p->root->scope, name, p->builtin_module_member, (SrcLoc){0});
        if(err) return CC_OOM_ERROR;
        Atom public_name = AT_ATOMIZE(p->cpp.at, "_ModuleMember");
        if(!public_name) return CC_OOM_ERROR;
        err = cc_scope_insert_typedef(al, &p->root->scope, public_name, p->builtin_module_member, (SrcLoc){0});
        if(err) return CC_OOM_ERROR;
    }

    {
        Atom name = AT_ATOMIZE(p->cpp.at, "__builtin_Module");
        if(!name) return CC_OOM_ERROR;
        CcStruct* s = Allocator_zalloc(al, 1, sizeof *s, _Alignof(CcStruct));
        if(!s) return CC_OOM_ERROR;
        *s = (CcStruct){
            .kind = CC_STRUCT,
            .is_incomplete = 1,
            .name = name,
        };
        err = cc_scope_insert_struct_tag(al, &p->root->scope, name, s);
        if(err) return CC_OOM_ERROR;
        CcQualType module_struct = {.bits = (uintptr_t)s};
        err = cc_pointer_of(p, module_struct, &p->builtin_module);
        if(err) return err;
        Atom module_typedef = AT_ATOMIZE(p->cpp.at, "_Module");
        if(!module_typedef) return CC_OOM_ERROR;
        err = cc_scope_insert_typedef(al, &p->root->scope, module_typedef, p->builtin_module, (SrcLoc){0});
        if(err) return CC_OOM_ERROR;
    }
    {
        // integer typedefs
        const struct {
            StringView name;
            CcBasicTypeKind kind;
        } to_register[] = {
            { SVI("__int128_t"), CCBT_int128, },
            { SVI("__uint128_t"), CCBT_unsigned_int128, },
            { SVI("int128_t"), CCBT_int128},
            { SVI("uint128_t"), CCBT_unsigned_int128},
            { SVI("int64_t"), t->int64_type},
            { SVI("uint64_t"), ccbt_to_unsigned(t->int64_type)},
            { SVI("intptr_t"), t->intptr_type},
            { SVI("uintptr_t"), ccbt_to_unsigned(t->intptr_type)},
            { SVI("size_t"), t->size_type},
            { SVI("uint32_t"), CCBT_unsigned},
            { SVI("int32_t"), CCBT_int},
            { SVI("uint16_t"), CCBT_unsigned_short},
            { SVI("int16_t"), CCBT_short},
            { SVI("uint8"), CCBT_unsigned_char},
            { SVI("int8_t"), CCBT_signed_char},
        };
        for(size_t i = 0; i < sizeof to_register / sizeof to_register[0]; i++){
            Atom name = AT_atomize(p->cpp.at, to_register[i].name.text, to_register[i].name.length);
            if(!name) return CC_OOM_ERROR;
            err = cc_scope_insert_typedef(al, &p->root->scope, name, ccqt_basic(to_register[i].kind), (SrcLoc){0});
            if(err) return CC_OOM_ERROR;
        }
    }

    // Register builtin functions
    {
        static const struct { StringView name; CcBuiltinFunc id; } builtins[] = {
            {SVI("__builtin_LINE"), CC__builtin_LINE},
            {SVI("__builtin_COLUMN"), CC__builtin_COLUMN},
            {SVI("__builtin_FILE"), CC__builtin_FILE},
            {SVI("__builtin_FUNCTION"), CC__builtin_FUNCTION},
            {SVI("__builtin_SRCLOC"), CC__builtin_SRCLOC},
            {SVI("__builtin_complex"), CC__builtin_complex},
            {SVI("__builtin_constant_p"), CC__builtin_constant_p},
            {SVI("__builtin_offsetof"), CC__builtin_offsetof},
            {SVI("__builtin_types_compatible_p"), CC__builtin_types_compatible_p},
            {SVI("__builtin_choose_expr"), CC__builtin_choose_expr},
            {SVI("__func__"), CC__func__},
            {SVI("__FUNCTION__"), CC__func__},
            {SVI("__atomic_fetch_add"), CC__atomic_fetch_add},
            {SVI("__atomic_fetch_sub"), CC__atomic_fetch_sub},
            {SVI("__atomic_add_fetch"), CC__atomic_add_fetch},
            {SVI("__atomic_sub_fetch"), CC__atomic_sub_fetch},
            {SVI("__atomic_fetch_and"), CC__atomic_fetch_and},
            {SVI("__atomic_fetch_or"),  CC__atomic_fetch_or},
            {SVI("__atomic_fetch_xor"), CC__atomic_fetch_xor},
            {SVI("__atomic_load_n"), CC__atomic_load_n},
            {SVI("__atomic_load"), CC__atomic_load},
            {SVI("__atomic_store_n"), CC__atomic_store_n},
            {SVI("__atomic_exchange_n"), CC__atomic_exchange_n},
            {SVI("__atomic_compare_exchange_n"), CC__atomic_compare_exchange_n},
            {SVI("__atomic_compare_exchange"), CC__atomic_compare_exchange},
            {SVI("__atomic_store"), CC__atomic_store},
            {SVI("__atomic_exchange"), CC__atomic_exchange},
            {SVI("__atomic_thread_fence"), CC__atomic_thread_fence},
            {SVI("__atomic_signal_fence"), CC__atomic_signal_fence},
            {SVI("__builtin_va_start"), CC__builtin_va_start},
            {SVI("__va_start"), CC__builtin_va_start},
            {SVI("__builtin_va_end"),   CC__builtin_va_end},
            {SVI("__va_end"),   CC__builtin_va_end},
            {SVI("__builtin_va_arg"),   CC__builtin_va_arg},
            {SVI("__builtin_va_copy"),  CC__builtin_va_copy},
            {SVI("__builtin_expect"),  CC__builtin_expect},
            {SVI("__builtin___clear_cache"), CC__builtin___clear_cache},
            {SVI("__builtin_unreachable"), CC__builtin_unreachable},
            {SVI("__builtin_trap"), CC__builtin_trap},
            {SVI("__builtin_readcyclecounter"), CC__builtin_readcyclecounter},
            {SVI("__builtin_debugtrap"), CC__builtin_debugtrap},
            {SVI("__builtin_abort"), CC__builtin_abort},
            {SVI("__builtin_mul_overflow"), CC__builtin_mul_overflow},
            {SVI("__builtin_add_overflow"), CC__builtin_add_overflow},
            {SVI("__builtin_sub_overflow"), CC__builtin_sub_overflow},
            {SVI("__builtin_popcount"), CC__builtin_popcount},
            {SVI("__builtin_ffs"), CC__builtin_ffs},
            {SVI("__builtin_ffsl"), CC__builtin_ffsl},
            {SVI("__builtin_ffsll"), CC__builtin_ffsll},
            {SVI("__builtin_clrsb"), CC__builtin_clrsb},
            {SVI("__builtin_clrsbl"), CC__builtin_clrsbl},
            {SVI("__builtin_clrsbll"), CC__builtin_clrsbll},
            {SVI("__builtin_parity"), CC__builtin_parity},
            {SVI("__builtin_parityl"), CC__builtin_parityl},
            {SVI("__builtin_parityll"), CC__builtin_parityll},
            {SVI("__builtin_ffsg"), CC__builtin_ffsg},
            {SVI("__builtin_clzg"), CC__builtin_clzg},
            {SVI("__builtin_ctzg"), CC__builtin_ctzg},
            {SVI("__builtin_clrsbg"), CC__builtin_clrsbg},
            {SVI("__builtin_popcountg"), CC__builtin_popcountg},
            {SVI("__builtin_parityg"), CC__builtin_parityg},
            {SVI("__builtin_stdc_bit_ceil"), CC__builtin_stdc_bit_ceil},
            {SVI("__builtin_stdc_bit_floor"), CC__builtin_stdc_bit_floor},
            {SVI("__builtin_stdc_bit_width"), CC__builtin_stdc_bit_width},
            {SVI("__builtin_stdc_count_ones"), CC__builtin_stdc_count_ones},
            {SVI("__builtin_stdc_count_zeros"), CC__builtin_stdc_count_zeros},
            {SVI("__builtin_stdc_first_leading_one"), CC__builtin_stdc_first_leading_one},
            {SVI("__builtin_stdc_first_leading_zero"), CC__builtin_stdc_first_leading_zero},
            {SVI("__builtin_stdc_first_trailing_one"), CC__builtin_stdc_first_trailing_one},
            {SVI("__builtin_stdc_first_trailing_zero"), CC__builtin_stdc_first_trailing_zero},
            {SVI("__builtin_stdc_has_single_bit"), CC__builtin_stdc_has_single_bit},
            {SVI("__builtin_stdc_leading_ones"), CC__builtin_stdc_leading_ones},
            {SVI("__builtin_stdc_leading_zeros"), CC__builtin_stdc_leading_zeros},
            {SVI("__builtin_stdc_trailing_ones"), CC__builtin_stdc_trailing_ones},
            {SVI("__builtin_stdc_trailing_zeros"), CC__builtin_stdc_trailing_zeros},
            {SVI("__builtin_stdc_rotate_left"), CC__builtin_stdc_rotate_left},
            {SVI("__builtin_stdc_rotate_right"), CC__builtin_stdc_rotate_right},
            {SVI("__builtin_popcountl"), CC__builtin_popcountl},
            {SVI("__builtin_popcountll"), CC__builtin_popcountll},
            {SVI("__builtin_ctz"), CC__builtin_ctz},
            {SVI("__builtin_ctzl"), CC__builtin_ctzl},
            {SVI("__builtin_ctzll"), CC__builtin_ctzll},
            {SVI("__builtin_clz"), CC__builtin_clz},
            {SVI("__builtin_clzl"), CC__builtin_clzl},
            {SVI("__builtin_clzll"), CC__builtin_clzll},
            {SVI("__builtin_huge_val"), CC__builtin_huge_val},
            {SVI("__builtin_huge_valf"), CC__builtin_huge_valf},
            {SVI("__builtin_huge_vall"), CC__builtin_huge_vall},
            {SVI("__builtin_inf"), CC__builtin_huge_val},
            {SVI("__builtin_inff"), CC__builtin_huge_valf},
            {SVI("__builtin_infl"), CC__builtin_huge_vall},
            {SVI("__builtin_nan"), CC__builtin_nan},
            {SVI("__builtin_nanf"), CC__builtin_nanf},
            {SVI("__nan"), CC__nan},
            {SVI("__nan"), CC__nan},
            {SVI("__builtin_bswap16"), CC__builtin_bswap16},
            {SVI("__builtin_bswap32"), CC__builtin_bswap32},
            {SVI("__builtin_bswap64"), CC__builtin_bswap64},
            {SVI("__builtin_crc32c8"), CC__builtin_crc32c8},
            {SVI("__builtin_crc32c16"), CC__builtin_crc32c16},
            {SVI("__builtin_crc32c32"), CC__builtin_crc32c32},
            {SVI("__builtin_crc32c64"), CC__builtin_crc32c64},
            {SVI("__builtin_alloca"), CC__builtin_alloca},
            {SVI("_alloca"), CC__builtin_alloca},
            {SVI("alloca"), CC__builtin_alloca},
            {SVI("__builtin_intern"), CC__builtin_intern},
            {SVI("__bt"), CC__bt},
            {SVI("__builtin_capture_backtrace"), CC__builtin_capture_backtrace},
            {SVI("__builtin_symbolicate"), CC__builtin_symbolicate},
            {SVI("_InterlockedExchange"), CC_InterlockedExchange},
            {SVI("_InterlockedExchange8"), CC_InterlockedExchange8},
            {SVI("_InterlockedExchange16"), CC_InterlockedExchange16},
            {SVI("_InterlockedExchange64"), CC_InterlockedExchange64},
            {SVI("_InterlockedCompareExchange"), CC_InterlockedCompareExchange},
            {SVI("_InterlockedCompareExchange8"), CC_InterlockedCompareExchange8},
            {SVI("_InterlockedCompareExchange16"), CC_InterlockedCompareExchange16},
            {SVI("_InterlockedCompareExchange64"), CC_InterlockedCompareExchange64},
            {SVI("_InterlockedCompareExchange128"), CC_InterlockedCompareExchange128},
            {SVI("_InterlockedIncrement"), CC_InterlockedIncrement},
            {SVI("_InterlockedIncrement16"), CC_InterlockedIncrement16},
            {SVI("_InterlockedIncrement64"), CC_InterlockedIncrement64},
            {SVI("_InterlockedDecrement"), CC_InterlockedDecrement},
            {SVI("_InterlockedDecrement16"), CC_InterlockedDecrement16},
            {SVI("_InterlockedDecrement64"), CC_InterlockedDecrement64},
            {SVI("_InterlockedExchangeAdd"), CC_InterlockedExchangeAdd},
            {SVI("_InterlockedExchangeAdd8"), CC_InterlockedExchangeAdd8},
            {SVI("_InterlockedExchangeAdd16"), CC_InterlockedExchangeAdd16},
            {SVI("_InterlockedExchangeAdd64"), CC_InterlockedExchangeAdd64},
            {SVI("_InterlockedAnd"), CC_InterlockedAnd},
            {SVI("_InterlockedAnd8"), CC_InterlockedAnd8},
            {SVI("_InterlockedAnd16"), CC_InterlockedAnd16},
            {SVI("_InterlockedAnd64"), CC_InterlockedAnd64},
            {SVI("_InterlockedOr"), CC_InterlockedOr},
            {SVI("_InterlockedOr8"), CC_InterlockedOr8},
            {SVI("_InterlockedOr16"), CC_InterlockedOr16},
            {SVI("_InterlockedOr64"), CC_InterlockedOr64},
            {SVI("_InterlockedXor"), CC_InterlockedXor},
            {SVI("_InterlockedXor8"), CC_InterlockedXor8},
            {SVI("_InterlockedXor16"), CC_InterlockedXor16},
            {SVI("_InterlockedXor64"), CC_InterlockedXor64},
            {SVI("_byteswap_ushort"), CC__builtin_bswap16},
            {SVI("_byteswap_ulong"), CC__builtin_bswap32},
            {SVI("_byteswap_uint64"), CC__builtin_bswap64},
            {SVI("_umul128"), CC__umul128},
            {SVI("__root_module"), CC__root_module},
            {SVI("__hotswap"), CC__hotswap},
            {SVI("__compile"), CC__compile},
        };
        if(AM_reserve(&p->builtins, al, sizeof builtins / sizeof builtins[0])) return CC_OOM_ERROR;
        for(size_t i = 0; i < sizeof builtins / sizeof builtins[0]; i++){
            Atom a = AT_atomize(p->cpp.at, builtins[i].name.text, builtins[i].name.length);
            if(!a) return CC_OOM_ERROR;
            err = AM_put(&p->builtins, al, a, (void*)builtins[i].id);
            if(err) return CC_OOM_ERROR;
        }
    }
    err = cpp_define_builtin_func_macro(&p->cpp, SV("__has_builtin"), cc_has_builtin, p, 1, 0, 1);
    if(err) return err;
    err = cpp_define_builtin_func_macro(&p->cpp, SV("__has_feature"), cc_has_feature, p, 1, 0, 1);
    if(err) return err;
    err = cpp_define_builtin_func_macro(&p->cpp, SV("__has_extension"), cc_has_extension, p, 1, 0, 1);
    if(err) return err;

    // Register type methods/fields
    {
        static const struct { StringView name; CcTypeIntrospectionOp op; } typeintro[] = {
            {SVI("name"), CC_TYPE_NAME},
            {SVI("tag"), CC_TYPE_TAG},
            {SVI("is_valid"), CC_TYPE_IS_VALID},
            {SVI("is_invalid"), CC_TYPE_IS_INVALID},
            {SVI("is_integer"), CC_TYPE_IS_INTEGER},
            {SVI("is_float"), CC_TYPE_IS_FLOAT},
            {SVI("is_arithmetic"), CC_TYPE_IS_ARITHMETIC},
            {SVI("is_pointer"), CC_TYPE_IS_POINTER},
            {SVI("is_struct"), CC_TYPE_IS_STRUCT},
            {SVI("is_union"), CC_TYPE_IS_UNION},
            {SVI("is_array"), CC_TYPE_IS_ARRAY},
            {SVI("is_vector"), CC_TYPE_IS_VECTOR},
            {SVI("is_slice"), CC_TYPE_IS_SLICE},
            {SVI("is_function"), CC_TYPE_IS_FUNCTION},
            {SVI("is_enum"), CC_TYPE_IS_ENUM},
            {SVI("is_const"), CC_TYPE_IS_CONST},
            {SVI("is_volatile"), CC_TYPE_IS_VOLATILE},
            {SVI("is_atomic"), CC_TYPE_IS_ATOMIC},
            {SVI("is_unsigned"), CC_TYPE_IS_UNSIGNED},
            {SVI("is_signed"), CC_TYPE_IS_SIGNED},
            {SVI("is_callable"), CC_TYPE_IS_CALLABLE},
            {SVI("is_variadic"), CC_TYPE_IS_VARIADIC},
            {SVI("is_incomplete"), CC_TYPE_IS_INCOMPLETE},
            {SVI("sizeof_"), CC_TYPE_SIZEOF},
            {SVI("alignof_"), CC_TYPE_ALIGNOF},
            {SVI("pointee"), CC_TYPE_POINTEE},
            {SVI("unqual"), CC_TYPE_UNQUAL},
            {SVI("count"), CC_TYPE_COUNT},
            {SVI("loc"), CC_TYPE_LOC},
            {SVI("is_callable_with"), CC_TYPE_IS_CALLABLE_WITH},
            {SVI("is_callable_through"), CC_TYPE_IS_CALLABLE_THROUGH},
            {SVI("is_castable_to"), CC_TYPE_CASTABLE_TO},
            {SVI("make_any"), CC_TYPE_MAKE_ANY},
            {SVI("field"), CC_TYPE_FIELD}, // field name or index;
            {SVI("fields"), CC_TYPE_FIELDS},
            {SVI("method"), CC_TYPE_METHOD},
            {SVI("methods"), CC_TYPE_METHODS},
            {SVI("has_field"), CC_TYPE_HAS_FIELD},
            {SVI("has_method"), CC_TYPE_HAS_METHOD},
            {SVI("push_method"), CC_TYPE_PUSH_METHOD},
            {SVI("enumerators"), CC_TYPE_ENUMERATORS},
            {SVI("enumerator"), CC_TYPE_ENUMERATOR},
            {SVI("return_type"), CC_TYPE_RETURN_TYPE},
            {SVI("param_count"), CC_TYPE_PARAM_COUNT},
            {SVI("param_type"), CC_TYPE_PARAM_TYPE},
            {SVI("element_type"), CC_TYPE_ELEMENT_TYPE},
            {SVI("underlying_type"), CC_TYPE_UNDERLYING_TYPE},
        };
        if(AM_reserve(&p->type_intro, al, sizeof typeintro / sizeof typeintro[0])) return CC_OOM_ERROR;
        for(size_t i = 0; i < sizeof typeintro / sizeof typeintro[0]; i++){
            Atom a = AT_atomize(p->cpp.at, typeintro[i].name.text, typeintro[i].name.length);
            if(!a) return CC_OOM_ERROR;
            err = AM_put(&p->type_intro, al, a, (void*)(uintptr_t)typeintro[i].op);
            if(err) return CC_OOM_ERROR;
        }
    }
    // Register __builtin_ libc functions
    {
        struct b {StringView name; CcQualType ret; int nargs; CcQualType params[3]; _Bool variadic; _Bool printf_like;} builtins[] = {
            {SVI("__builtin_memcpy"), p->void_star, 3, {p->void_star, p->const_void_star, {.basic.kind=t->size_type}}, .variadic=0},
            {SVI("__builtin_memcmp"), {.basic.kind=CCBT_int}, 3, {p->const_void_star, p->const_void_star, {.basic.kind=t->size_type}}, .variadic=0},
            {SVI("__builtin_strcmp"), {.basic.kind=CCBT_int}, 2, {p->const_char_star, p->const_char_star}, .variadic=0},
            {SVI("__builtin_memmove"), p->void_star, 3, {p->void_star, p->const_void_star, {.basic.kind=t->size_type}}, .variadic=0},
            {SVI("__builtin_memset"), p->void_star, 3, {p->void_star, {.basic.kind=CCBT_int}, {.basic.kind=t->size_type}}, .variadic=0},
            {SVI("__builtin_malloc"), p->void_star, 1, {{.basic.kind=t->size_type}}, .variadic=0},
            {SVI("__builtin_realloc"), p->void_star, 2, {p->void_star, {.basic.kind=t->size_type}}, .variadic=0},
            {SVI("__builtin_calloc"), p->void_star, 2, {{.basic.kind=t->size_type},{.basic.kind=t->size_type}}, .variadic=0},
            {SVI("__builtin_free"), {.basic.kind=CCBT_void}, 1, {p->void_star}, .variadic=0},
            {SVI("__builtin_bzero"), {.basic.kind=CCBT_void}, 2, {p->void_star, {.basic.kind=t->size_type}}, .variadic=0},
            {SVI("__builtin_snprintf"), {.basic.kind=CCBT_int}, 3, {p->char_star, {.basic.kind=t->size_type}, p->const_char_star}, .variadic=1, .printf_like=1},
            {SVI("__builtin_printf"), {.basic.kind=CCBT_int}, 1, {p->const_char_star}, .variadic=1, .printf_like=1},
            {SVI("__builtin_fabsf"), {.basic.kind=CCBT_float}, 1, {{.basic.kind=CCBT_float}}},
            {SVI("__builtin_fabs"), {.basic.kind=CCBT_double}, 1, {{.basic.kind=CCBT_double}}},
            {SVI("__builtin_fabsl"), {.basic.kind=CCBT_long_double}, 1, {{.basic.kind=CCBT_long_double}}},
        };
        for(size_t i = 0; i < sizeof builtins / sizeof builtins[0]; i++){
            struct b* b = &builtins[i];
            CcFunction* ftype = cc_intern_function(&p->type_cache, al, b->ret, b->params, b->nargs, b->nargs, b->variadic, 0);
            if(!ftype) return CC_OOM_ERROR;
            CcFunc* func = Allocator_zalloc(al, 1, sizeof *func, _Alignof(CcFunc));
            if(!func) return CC_OOM_ERROR;
            Atom key = AT_atomize(p->cpp.at, b->name.text, b->name.length);
            if(!key) return CC_OOM_ERROR;
            enum {PREFIX_LENGTH = sizeof "__builtin_" - 1};
            Atom name = AT_atomize(p->cpp.at, b->name.text + PREFIX_LENGTH, b->name.length - PREFIX_LENGTH);
            if(!name) return CC_OOM_ERROR;
            func->name = name;
            func->type = ftype;
            func->extern_ = 1;
            func->libc_builtin = 1;
            func->has_linkage = 1;
            if(b->printf_like){
                CcPrintfFormat format = {.string_index=(uint32_t)b->nargs, .first_to_check=(uint32_t)b->nargs+1};
                err = cc_apply_printf_formats(p, func, &format);
                if(err) return err;
            }
            err = cc_scope_insert_func(al, &p->root->scope, key, func);
            if(err) return CC_OOM_ERROR;
            err = cc_scope_insert_func(al, &p->root->scope, name, func);
            if(err) return CC_OOM_ERROR;
        }
    }
    return 0;
}

static
int
cc_parse_local_methods(CcParser* p){
    if(!p->current_func) return 0;
    CcScope* scope = cc_scope_declarations(p->current);
    while(scope->deferred_methods.count){
        CcFunc* f = scope->deferred_methods.data[--scope->deferred_methods.count];
        int err = cc_parse_func_body(p, f);
        if(err) return err;
    }
    return 0;
}

static
int
cc_append_function_exit(CcParser* p, CcFunc* f, CcStmtSink* sink){
    CcStmtNode* last = sink->stmts.count ? sink->stmts.data[sink->stmts.count-1] : NULL;
    if(last && last->kind == CC_STMT_RETURN)
        return 0;
    CcQualType ret = f->type->return_type;
    StringView name = SV("");
    if(f->name)
        name = (StringView){f->name->length, f->name->data};
    _Bool main_zero = !f->enclosing && ccqt_bt_eq(ret, CCBT_int) && sv_equals(name, SV("main"));
    _Bool returns = !f->noreturn_ && (main_zero || ccqt_bt_eq(ret, CCBT_void));
    CcStmtNode* node = cc_stmt_node(p, returns ? CC_STMT_RETURN : CC_STMT_EXPR, f->loc, 0);
    if(!node) return CC_OOM_ERROR;
    if(returns){
        node->return_.first = p->defer_context.active;
        if(main_zero){
            node->return_.expr = cc_int64_expr(p, f->loc, ret, 0);
            if(!node->return_.expr){
                cc_free_stmt_tree(p, node);
                return CC_OOM_ERROR;
            }
        }
    }
    else {
        CcExpr* expr = cc_make_expr(p, CC_EXPR_BUILTIN, f->loc, ccqt_basic(CCBT_void), 0);
        if(!expr){
            cc_free_stmt_tree(p, node);
            return CC_OOM_ERROR;
        }
        expr->builtin.op = CC_BUILTIN_TRAP;
        node->expr = expr;
    }
    return cc_sink_push(p, node);
}

static
int
cc_parse_func_body_inner(CcParser* p, CcFunc* f, _Bool terminate_on_rbrace){
    int err = cc_check_function_definition_type(p, f->type, f->loc);
    if(err) return err;
    CcScope* saved_scope = p->current;
    CcModule* saved_module = p->module;
    CcModule* module = f->module;
    if(module) p->module = module;
    if(f->definition_scope) p->current = (CcScope*)f->definition_scope;
    CcStaticFor* saved_static_for = p->static_for;
    p->static_for = NULL;
    CcStaticControl* saved_static_control = p->static_control;
    p->static_control = NULL;
    CcFunc* prev = p->current_func;
    CcScope* saved_function_scope = p->function_scope;
    CcQualType prev__Self = p->current_tag_type;
    uint32_t prev_loop_depth = p->loop_depth;
    CcControl* saved_control = p->control;
    p->control = NULL;
    CcSwitchCtx* prev_switch_ctx = p->switch_ctx;
    CcAttributes prev_attributes = p->attributes;
    CcDeferContext saved_defer_context = p->defer_context;
    p->defer_context = (CcDeferContext){0};
    p->loop_depth = 0;
    p->switch_ctx = NULL;
    cc_clear_attributes(&p->attributes);
    p->current_func = f;
    p->current_tag_type = f->_Self_type;
    CcScope* param_scope = f->param_scope;
    if(param_scope){
        param_scope->parent = p->current;
        p->current = param_scope;
        f->param_scope = NULL;
        err = 0;
    }
    else err = cc_push_scope(p);
    if(err) goto restore_context;
    p->function_scope = p->current;
    err = cc_parse_local_methods(p);
    if(err) goto end_scope;
    {
        CcStmtSink* sink = cc_push_stmt_sink(p);
        if(!sink){
            err = CC_OOM_ERROR;
            goto end_scope;
        }
        if(f->body_tree){
            err = cc_sink_push(p, (CcStmtNode*)f->body_tree);
            f->body_tree = NULL;
            if(err){
                cc_pop_stmt_sink(p, sink);
                goto end_scope;
            }
        }
        for(;;){
            CcToken peek;
            err = cc_peek(p, &peek);
            if(err) break;
            if(p->static_goto_target && (peek.type == CC_EOF ||
               (peek.type == CC_PUNCTUATOR && peek.punct.punct == '}'))){
                err = cc_static_goto_missing(p);
                break;
            }
            if(terminate_on_rbrace){
                if(peek.type == CC_PUNCTUATOR && peek.punct.punct == CC_rbrace) break;
                if(peek.type == CC_EOF){
                    err = cc_error(p, f->loc, "Unexpected EOF in function body");
                    break;
                }
            }
            else {
                if(peek.type == CC_EOF) break;
            }
            err = cc_parse_one(p, (CcParseFlags){0});
            if(err) break;
        }
        if(err) goto end_sink;
        err = cc_append_function_exit(p, f, sink);
        if(err) goto end_sink;
        err = cc_finalize_stmt_list(p, f->loc, &sink->stmts, 1, &f->body_tree);
        if(err) goto end_sink;
        err = cc_stmt_scope_vars(p, (CcStmtNode*)f->body_tree);
        if(err) goto end_sink;
        f->body_tree->cleanup = (CcCleanupRange){p->defer_context.active, NULL};
        end_sink:
        cc_pop_stmt_sink(p, sink);
        if(err) goto end_scope;
    }
    err = cc_check_gotos(p, &f->label_ctx);
    if(err) goto end_scope;
    f->parsed = 1;
    end_scope:
    ma_cleanup(CcLabelRef)(&f->label_ctx.gotos, cc_allocator(p));
    cc_pop_scope(p);
    restore_context:
    p->current = saved_scope;
    p->module = saved_module;
    p->static_for = saved_static_for;
    p->static_control = saved_static_control;
    p->current_func = prev;
    p->function_scope = saved_function_scope;
    p->current_tag_type = prev__Self;
    p->loop_depth = prev_loop_depth;
    p->control = saved_control;
    p->switch_ctx = prev_switch_ctx;
    p->attributes = prev_attributes;
    p->defer_context = saved_defer_context;
    return err;
}

static
int
cc_parse_func_body(CcParser* p, CcFunc* f){
    DRP_PROFILE_BEGIN(profile, "parser.function_body");
    if(!f->defined) return CC_UNREACHABLE_ERROR;
    if(f->parsed) return 0;
    if(f->parse_failed) return CC_SYNTAX_ERROR;
    Marray(CcToken)* tokens = f->tokens;
    if(!tokens){
        f->parse_failed = 1;
        return CC_SYNTAX_ERROR;
    }
    // Append EOF sentinel so parsing doesn't fall through to the main stream.
    CcToken eof_tok = {.type = CC_EOF};
    int eof_err = ma_push(CcToken)(tokens, cc_allocator(p), eof_tok);
    if(eof_err) return CC_OOM_ERROR;
    // Reverse the token array so it works as LIFO pending
    for(size_t i = 0, j = tokens->count; i < j; ){
        j--;
        CcToken tmp = tokens->data[i];
        tokens->data[i] = tokens->data[j];
        tokens->data[j] = tmp;
        i++;
    }
    // Swap into pending
    Marray(CcToken) saved_pending = p->pending;
    p->pending = *tokens;
    int err = cc_parse_func_body_inner(p, f, 0);
    // Restore pending (tokens array was consumed into p->pending)
    *tokens = p->pending;
    p->pending = saved_pending;
    cc_release_scratch(p, tokens);
    f->tokens = NULL;
    if(err) f->parse_failed = 1;
    return err;
    DRP_PROFILE_END(profile);
}

static
_Bool
cc_eval_size_product_valid(CiUint128 length, CiUint128 element_size,
    unsigned size_bits, uint32_t element_alignment){
    uint64_t stride = ci_uint128_lo(element_size);
    return ci_uint128_fits_integer(ci_uint128_mul(length, element_size), size_bits, 1)
        && (!element_alignment || (stride != 0 && stride % element_alignment == 0));
}

// Wide constants retain the target representation in the expression payload.
static
_Bool
cc_eval_wide(CcQualType t){
    t = ccqt_enum_underlying(t);
    if(!ccqt_is_basic(t)) return 0;
    switch(ccqt_basic_kind(t)){
        case CCBT_long_double:
        case CCBT_float128:
        case CCBT_int128:
        case CCBT_unsigned_int128:
            return 1;
        default:
            return 0;
    }
}

static
CiUint128
cc_eval_u128(const CcTargetConfig* target, CcExpr* v){
    if(cc_eval_wide(v->type) && ccqt_is_integer(v->type))
        return v->uinteger128;
    if(ccqt_has_pointer_representation(v->type))
        return ci_uint128_from_uint64(v->uinteger);
    if(ccqt_is_unsigned(v->type, !target->char_is_signed))
        return ci_uint128_from_uint64(v->uinteger);
    return ci_uint128_from_int64(v->integer);
}

static
CiFloat128
cc_eval_quad(const CcTargetConfig* target, CcExpr* v){
    if(ccqt_is_basic(v->type)){
        switch(ccqt_basic_kind(v->type)){
            case CCBT_fp16:
            case CCBT_float16:
                return ci_float16_to_quad((uint16_t)v->uinteger, 0);
            case CCBT_bfloat16:
                return ci_float16_to_quad((uint16_t)v->uinteger, 1);
            case CCBT_float:
                return ci_float128_from_float(v->float_);
            case CCBT_double:
                return ci_float128_from_double(v->double_);
            case CCBT_float128:
                return v->quad;
            case CCBT_long_double:
                switch(target->long_double_format){
                    case CC_LONG_DOUBLE_X87:
                        return ci_float128_from_float80(v->x87);
                    case CC_LONG_DOUBLE_BINARY128:
                        return v->quad;
                    case CC_LONG_DOUBLE_BINARY64:
                        return ci_float128_from_double(v->double_);
                }
                break;
            default:
                break;
        }
    }
    return ci_float128_from_uint128(cc_eval_u128(target, v), ccqt_is_unsigned(v->type, !target->char_is_signed));
}

static
void
cc_eval_store_quad(const CcTargetConfig* target, CcExpr* v, CiFloat128 q){
    if(ccqt_is_basic(v->type)){
        switch(ccqt_basic_kind(v->type)){
            case CCBT_fp16:
            case CCBT_float16:
                v->uinteger = ci_float16_from_quad(q, 0);
                return;
            case CCBT_bfloat16:
                v->uinteger = ci_float16_from_quad(q, 1);
                return;
            case CCBT_long_double:
                switch(target->long_double_format){
                    case CC_LONG_DOUBLE_X87:
                        v->x87 = ci_float80_from_float128(q);
                        return;
                    case CC_LONG_DOUBLE_BINARY64:
                        v->double_ = ci_float128_to_double(q);
                        return;
                    case CC_LONG_DOUBLE_BINARY128:
                        break;
                }
                break;
            default:
                break;
        }
    }
    v->quad = q;
}

static
_Bool
cc_eval_value_truth(const CcTargetConfig* target, CcExpr* v){
    CcQualType type = ccqt_enum_underlying(v->type);
    if(!ccqt_is_basic(type)) return v->uinteger != 0;
    switch(ccqt_basic_kind(type)){
        case CCBT_fp16:
        case CCBT_float16:
        case CCBT_bfloat16:
            return ci_narrow_float_nonzero((uint16_t)v->uinteger);
        case CCBT_long_double:
            switch(target->long_double_format){
                case CC_LONG_DOUBLE_BINARY64:
                    return v->double_ != 0;
                case CC_LONG_DOUBLE_X87:
                    return ci_float80_nonzero(v->x87);
                case CC_LONG_DOUBLE_BINARY128:
                    return ci_float128_nonzero(v->quad);
            }
            cc_unsafe_unreachable();
        case CCBT_float128:
            return ci_float128_nonzero(v->quad);
        case CCBT_float:
            return v->float_ != 0;
        case CCBT_double:
            return v->double_ != 0;
        case CCBT_int128:
        case CCBT_unsigned_int128:
            return ci_uint128_nonzero(v->uinteger128);
        default:
            return v->uinteger != 0;
    }
}

static
int
cc_eval_narrow_cast(CcParser* p, CcExpr* v, CcExpr* out, _Bool allow_inexact){
    const CcTargetConfig* target = cc_target(p);
    CcQualType source_type = ccqt_enum_underlying(v->type);
    CcQualType dest_type = ccqt_enum_underlying(out->type);
    if(!ccqt_is_basic(source_type) || !ccqt_is_basic(dest_type))
        return CC_NOT_CONSTANT_ERROR;
    CcBasicTypeKind source_kind = ccqt_basic_kind(source_type);
    CcBasicTypeKind dest_kind = ccqt_basic_kind(dest_type);
    if(ccqt_is_narrow_float(dest_type)){
        _Bool is_brian_float = dest_kind == CCBT_bfloat16;
        uint_fast8_t saved_flags = softfloat_exceptionFlags;
        softfloat_exceptionFlags = 0;
        uint16_t bits = 0;
        int err = 0;
        switch(source_kind){
            case CCBT_bool:
            case CCBT_char:
            case CCBT_signed_char:
            case CCBT_unsigned_char:
            case CCBT_short:
            case CCBT_unsigned_short:
            case CCBT_int:
            case CCBT_unsigned:
            case CCBT_long:
            case CCBT_unsigned_long:
            case CCBT_long_long:
            case CCBT_unsigned_long_long:
            case CCBT_int128:
            case CCBT_unsigned_int128:{
                uint32_t size;
                err = cc_sizeof_as_uint(p, source_type, v->loc, &size);
                if(err) goto restore_flags;
                _Bool uns = ccqt_is_unsigned(source_type, !target->char_is_signed);
                if(size > 8){
                    bits = ci_float16_from_uint128(v->uinteger128, uns, is_brian_float);
                }
                else if(uns){
                    if(size <= 4)
                        bits = ci_float16_from_uint32((uint32_t)v->uinteger, is_brian_float);
                    else
                        bits = ci_float16_from_uint64(v->uinteger, is_brian_float);
                }
                else {
                    if(size <= 4)
                        bits = ci_float16_from_int32((int32_t)v->integer, is_brian_float);
                    else
                        bits = ci_float16_from_int64(v->integer, is_brian_float);
                }
                break;
            }
            case CCBT_fp16:
            case CCBT_float16:
            case CCBT_bfloat16: {
                _Bool source_is_brian_float = source_kind == CCBT_bfloat16;
                float value = ci_float16_to_float((uint16_t)v->uinteger, source_is_brian_float);
                bits = ci_float16_from_float(value, is_brian_float);
                break;
            }
            case CCBT_float:
                bits = ci_float16_from_float(v->float_, is_brian_float);
                break;
            case CCBT_double:
                bits = ci_float16_from_double(v->double_, is_brian_float);
                break;
            case CCBT_float128:
                bits = ci_float16_from_quad(v->quad, is_brian_float);
                break;
            case CCBT_long_double:
                switch(target->long_double_format){
                    case CC_LONG_DOUBLE_BINARY64:
                        bits = ci_float16_from_double(v->double_, is_brian_float);
                        break;
                    case CC_LONG_DOUBLE_X87:
                        bits = ci_float16_from_float80(v->x87, is_brian_float);
                        break;
                    case CC_LONG_DOUBLE_BINARY128:
                        bits = ci_float16_from_quad(v->quad, is_brian_float);
                        break;
                }
                break;
            default:
                err = CC_NOT_CONSTANT_ERROR;
                goto restore_flags;
        }
        if(!allow_inexact){
            uint16_t infinity = is_brian_float ? 0x7f80 : 0x7c00;
            _Bool is_nan = (bits & 0x7fff) > infinity;
            _Bool conversion_failed = softfloat_exceptionFlags & (softfloat_flag_inexact | softfloat_flag_invalid);
            if(is_nan || conversion_failed)
                err = CC_NOT_CONSTANT_ERROR;
        }
        restore_flags:
        softfloat_exceptionFlags = saved_flags;
        if(err) return err;
        out->uinteger = bits;
        return 0;
    }
    uint16_t bits = (uint16_t)v->uinteger;
    _Bool is_brian_float = source_kind == CCBT_bfloat16;
    if(ccqt_is_integer(dest_type)){
        uint32_t size;
        int err = cc_sizeof_as_uint(p, dest_type, out->loc, &size);
        if(err) return err;
        _Bool uns = ccqt_is_unsigned(dest_type, !target->char_is_signed);
        // Every narrow finite value is exactly representable in binary64.
        double value = ci_float16_to_double(bits, is_brian_float);
        double bound;
        uint64_t encoded_bound = (uint64_t)(1023 + size * 8 - !uns) << 52;
        memcpy(&bound, &encoded_bound, sizeof bound);
        double low = uns? 0 : -bound;
        if(!(value >= low || value > low - 1) || !(value < bound))
            return CC_OVERFLOW_ERROR;
        if(size > 8)
            out->uinteger128 = ci_float16_to_uint128(bits, is_brian_float);
        else if(uns){
            if(size <= 4)
                out->uinteger = ci_float16_to_uint32(bits, is_brian_float);
            else
                out->uinteger = ci_float16_to_uint64(bits, is_brian_float);
        }
        else {
            if(size <= 4)
                out->integer = ci_float16_to_int32(bits, is_brian_float);
            else
                out->integer = ci_float16_to_int64(bits, is_brian_float);
        }
        return 0;
    }
    uint16_t infinity = is_brian_float? 0x7f80 : 0x7c00;
    if(!allow_inexact && (bits & 0x7fff) > infinity)
        return CC_NOT_CONSTANT_ERROR;
    switch(dest_kind){
        case CCBT_float:
            out->float_ = ci_float16_to_float(bits, is_brian_float);
            break;
        case CCBT_double:
            out->double_ = ci_float16_to_double(bits, is_brian_float);
            break;
        case CCBT_float128:
            out->quad = ci_float16_to_quad(bits, is_brian_float);
            break;
        case CCBT_long_double:
            switch(target->long_double_format){
                case CC_LONG_DOUBLE_BINARY64:
                    out->double_ = ci_float16_to_double(bits, is_brian_float);
                    break;
                case CC_LONG_DOUBLE_X87:
                    out->x87 = ci_float16_to_float80(bits, is_brian_float);
                    break;
                case CC_LONG_DOUBLE_BINARY128:
                    out->quad = ci_float16_to_quad(bits, is_brian_float);
                    break;
            }
            break;
        default:
            return CC_NOT_CONSTANT_ERROR;
    }
    return 0;
}

static
int
cc_eval_numeric_cast(CcParser* p, CcExpr* v, CcExpr* out, _Bool allow_inexact){
    CcQualType t = out->type;
    if(ccqt_is_bool(t)){
        out->integer = cc_eval_value_truth(cc_target(p), v);
        return 0;
    }
    if(ccqt_is_narrow_float(t) || ccqt_is_narrow_float(v->type))
        return cc_eval_narrow_cast(p, v, out, allow_inexact);
    _Bool from_float = ccqt_is_basic(v->type) && ccbt_is_float(ccqt_basic_kind(v->type));
    if(ccqt_is_basic(t) && ccbt_is_float(ccqt_basic_kind(t))){
        CiFloat128 q = cc_eval_quad(cc_target(p), v);
        if(from_float){
            switch(ccqt_basic_kind(t)){
                case CCBT_float:
                    out->float_ = ci_float128_to_float(q);
                    break;
                case CCBT_double:
                    out->double_ = ci_float128_to_double(q);
                    break;
                case CCBT_long_double:
                case CCBT_float128:
                    cc_eval_store_quad(cc_target(p), out, q);
                    break;
                default:
                    return CC_NOT_CONSTANT_ERROR;
            }
        }
        else {
            // Convert integers directly to the destination precision. Rounding
            // through quad first can double-round a 128-bit integer.
            CiUint128 n = cc_eval_u128(cc_target(p), v);
            _Bool uns = ccqt_is_unsigned(v->type, !cc_target(p)->char_is_signed);
            switch(ccqt_basic_kind(t)){
                case CCBT_float:
                    out->float_ = ci_uint128_to_float(n, uns);
                    break;
                case CCBT_double:
                    out->double_ = ci_uint128_to_double(n, uns);
                    break;
                case CCBT_float128:
                    out->quad = ci_float128_from_uint128(n, uns);
                    break;
                case CCBT_long_double:
                    switch(cc_target(p)->long_double_format){
                        case CC_LONG_DOUBLE_BINARY64:
                            out->double_ = ci_uint128_to_double(n, uns);
                            break;
                        case CC_LONG_DOUBLE_X87:
                            out->x87 = ci_float80_from_uint128(n, uns);
                            break;
                        case CC_LONG_DOUBLE_BINARY128:
                            out->quad = ci_float128_from_uint128(n, uns);
                            break;
                    }
                    break;
                default:
                    return CC_NOT_CONSTANT_ERROR;
            }
        }
        if(allow_inexact) return 0;
        CiFloat128 back = cc_eval_quad(cc_target(p), out);
        if(!ci_float128_eq(back, q)) return CC_NOT_CONSTANT_ERROR;
        if(!from_float && !ci_uint128_eq(ci_float128_to_uint128(back), cc_eval_u128(cc_target(p), v)))
            return CC_NOT_CONSTANT_ERROR;
        return 0;
    }
    if(!ccqt_is_integer(t)) return CC_NOT_CONSTANT_ERROR;
    CiUint128 u;
    if(from_float){
        CiFloat128 q = cc_eval_quad(cc_target(p), v);
        uint32_t size;
        int err = cc_sizeof_as_uint(p, t, out->loc, &size);
        if(err) return err;
        _Bool uns = ccqt_is_unsigned(t, !cc_target(p)->char_is_signed);
        CiFloat128 bound = ci_float128_from_uint128(ci_uint128_shl(ci_uint128_from_uint64(1), size*8-1), 1);
        if(uns) bound = ci_float128_add(bound, bound);
        CiFloat128 low = uns ? ci_float128_from_int64(0) : ci_float128_neg(bound);
        CiFloat128 below = ci_float128_sub(low, ci_float128_from_int64(1));
        if(!(ci_float128_le(low, q) || ci_float128_lt(below, q)) || !ci_float128_lt(q, bound))
            return CC_OVERFLOW_ERROR;
        u = ci_float128_to_uint128(q);
    }
    else u = cc_eval_u128(cc_target(p), v);
    if(cc_eval_wide(t)) out->uinteger128 = u;
    else out->uinteger = ci_uint128_lo(u);
    return 0;
}

static
int
cc_eval_wide_binary(const CcTargetConfig* target, CcExprKind op, CcExpr* l, CcExpr* r, CcExpr* out){
    if(ccqt_is_narrow_float(l->type)){
        uint16_t a = (uint16_t)l->uinteger,
                 b = (uint16_t)r->uinteger;
        _Bool is_brian_float = ccqt_bt_eq(l->type, CCBT_bfloat16);
        switch(op){
            case CC_EXPR_ADD: out->uinteger =  ci_float16_add(a, b, is_brian_float); return 0;
            case CC_EXPR_SUB: out->uinteger =  ci_float16_sub(a, b, is_brian_float); return 0;
            case CC_EXPR_MUL: out->uinteger =  ci_float16_mul(a, b, is_brian_float); return 0;
            case CC_EXPR_DIV: out->uinteger =  ci_float16_div(a, b, is_brian_float); return 0;
            case CC_EXPR_EQ:  out->integer  =  ci_float16_eq (a, b, is_brian_float); return 0;
            case CC_EXPR_NE:  out->integer  = !ci_float16_eq (a, b, is_brian_float); return 0;
            case CC_EXPR_LT:  out->integer  =  ci_float16_lt (a, b, is_brian_float); return 0;
            case CC_EXPR_GT:  out->integer  =  ci_float16_lt (b, a, is_brian_float); return 0;
            case CC_EXPR_LE:  out->integer  =  ci_float16_le (a, b, is_brian_float); return 0;
            case CC_EXPR_GE:  out->integer  =  ci_float16_le (b, a, is_brian_float); return 0;
            default: return CC_NOT_CONSTANT_ERROR;
        }
    }
    if(ccqt_bt_eq(l->type, CCBT_long_double) || ccqt_bt_eq(l->type, CCBT_float128)){
        // Perform arithmetic in the target format, avoiding double rounding.
        #define FLOAT_OPS(prefix, a, b, field) \
            switch((unsigned)op){ \
                case CC_EXPR_ADD: out->field = prefix##_add(a,b); return 0; \
                case CC_EXPR_SUB: out->field = prefix##_sub(a,b); return 0; \
                case CC_EXPR_MUL: out->field = prefix##_mul(a,b); return 0; \
                case CC_EXPR_DIV: out->field = prefix##_div(a,b); return 0; \
                case CC_EXPR_EQ: out->integer = prefix##_eq(a,b); return 0; \
                case CC_EXPR_NE: out->integer = !prefix##_eq(a,b); return 0; \
                case CC_EXPR_LT: out->integer = prefix##_lt(a,b); return 0; \
                case CC_EXPR_GT: out->integer = prefix##_lt(b,a); return 0; \
                case CC_EXPR_LE: out->integer = prefix##_le(a,b); return 0; \
                case CC_EXPR_GE: out->integer = prefix##_le(b,a); return 0; \
                default: return CC_NOT_CONSTANT_ERROR; \
            }
        if(ccqt_bt_eq(l->type, CCBT_long_double) && target->long_double_format == CC_LONG_DOUBLE_X87){
            FLOAT_OPS(ci_float80, l->x87, r->x87, x87);
        }
        if(ccqt_bt_eq(l->type, CCBT_long_double) && target->long_double_format == CC_LONG_DOUBLE_BINARY64){
            // Binary64 operands/results are rounded by the host double operations.
            switch((unsigned)op){
                case CC_EXPR_ADD: out->double_ = l->double_ + r->double_; return 0;
                case CC_EXPR_SUB: out->double_ = l->double_ - r->double_; return 0;
                case CC_EXPR_MUL: out->double_ = l->double_ * r->double_; return 0;
                case CC_EXPR_DIV: out->double_ = l->double_ / r->double_; return 0;
                default: break;
            }
        }
        CiFloat128 a = cc_eval_quad(target,l), b = cc_eval_quad(target,r);
        FLOAT_OPS(ci_float128, a, b, quad);
        #undef FLOAT_OPS
    }
    CiUint128 a = cc_eval_u128(target,l), b = cc_eval_u128(target,r), z = ci_uint128_from_uint64(0), u = z;
    CiInt128 sa = ci_int128_from_uint128(a), sb = ci_int128_from_uint128(b);
    _Bool uns = ccqt_is_unsigned(l->type, !target->char_is_signed);
    _Bool an = ci_uint128_hi(a)>>63, bn = ci_uint128_hi(b)>>63;
    switch((unsigned)op){
        case CC_EXPR_ADD:
            u = ci_uint128_add(a,b);
            if(!uns && an == bn && (ci_uint128_hi(u)>>63) != an) return CC_OVERFLOW_ERROR;
            break;
        case CC_EXPR_SUB:
            u = ci_uint128_sub(a,b);
            if(!uns && an != bn && (ci_uint128_hi(u)>>63) != an) return CC_OVERFLOW_ERROR;
            break;
        case CC_EXPR_MUL:{
            if(!uns){
                CiUint128 aa = an ? ci_uint128_sub(z,a) : a, bb = bn ? ci_uint128_sub(z,b) : b;
                CiUint128 limit = ci_uint128_shl(ci_uint128_from_uint64(1),127);
                if(an == bn) limit = ci_uint128_sub(limit,ci_uint128_from_uint64(1));
                if(ci_uint128_nonzero(bb) && ci_uint128_gt(aa,ci_uint128_div(limit,bb))) return CC_OVERFLOW_ERROR;
            }
            u = ci_uint128_mul(a,b); break;
        }
        case CC_EXPR_DIV: case CC_EXPR_MOD:
            if(!ci_uint128_nonzero(b)) return CC_OVERFLOW_ERROR;
            if(!uns && ci_uint128_eq(a,ci_uint128_shl(ci_uint128_from_uint64(1),127))
                && ci_uint128_eq(b,ci_uint128_sub(z,ci_uint128_from_uint64(1)))) return CC_OVERFLOW_ERROR;
            if(uns) u = op == CC_EXPR_DIV ? ci_uint128_div(a,b) : ci_uint128_mod(a,b);
            else u = ci_uint128_from_int128(op == CC_EXPR_DIV ? ci_int128_div(sa,sb) : ci_int128_mod(sa,sb));
            break;
        case CC_EXPR_BITAND: u = ci_uint128_and(a,b); break;
        case CC_EXPR_BITOR: u = ci_uint128_or(a,b); break;
        case CC_EXPR_BITXOR: u = ci_uint128_xor(a,b); break;
        case CC_EXPR_LSHIFT: case CC_EXPR_RSHIFT:
            if(ci_uint128_hi(b) || ci_uint128_lo(b)>=128) return CC_OVERFLOW_ERROR;
            if(op == CC_EXPR_LSHIFT) u = ci_uint128_shl(a,ci_uint128_lo(b));
            else if(uns) u = ci_uint128_shr(a,ci_uint128_lo(b));
            else u = ci_uint128_from_int128(ci_int128_shr(sa,ci_uint128_lo(b)));
            break;
        case CC_EXPR_EQ: out->integer = ci_uint128_eq(a,b); return 0;
        case CC_EXPR_NE: out->integer = ci_uint128_ne(a,b); return 0;
        case CC_EXPR_LT: out->integer = uns ? ci_uint128_lt(a,b) : ci_int128_lt(sa,sb); return 0;
        case CC_EXPR_GT: out->integer = uns ? ci_uint128_gt(a,b) : ci_int128_gt(sa,sb); return 0;
        case CC_EXPR_LE: out->integer = uns ? ci_uint128_le(a,b) : ci_int128_le(sa,sb); return 0;
        case CC_EXPR_GE: out->integer = uns ? ci_uint128_ge(a,b) : ci_int128_ge(sa,sb); return 0;
        default: return CC_NOT_CONSTANT_ERROR;
    }
    out->uinteger128 = u;
    return 0;
}

static
int
cc_eval_to_i(CcParser* p, CcExpr* v, int64_t* out){
    if(ccqt_has_pointer_representation(v->type)){
        *out = (int64_t)v->uinteger;
        return 0;
    }
    if(cc_eval_wide(v->type)){
        CcExpr converted = {.type = ccqt_basic(CCBT_long_long)};
        int err = cc_eval_numeric_cast(p, v, &converted, 0);
        if(!err) *out = converted.integer;
        return err;
    }
    (void)p;
    CcQualType t = v->type;
    t = ccqt_enum_underlying(t);
    if(!ccqt_is_basic(t)) return CC_NOT_CONSTANT_ERROR;
    CcBasicTypeKind k = ccqt_basic_kind(t);
    switch(k){
        DRP_CASES_EXHAUSTED;
        case CCBT_double:{
            double d = v->double_;
            if(d != d || d >= 9223372036854775808.0 || d < -9223372036854775808.0) return CC_NOT_CONSTANT_ERROR;
            *out = (int64_t)d;
            return 0;
        }
        case CCBT_float:{
            float f = v->float_;
            if(f != f || f >= 9223372036854775808.0f || f < -9223372036854775808.0f) return CC_NOT_CONSTANT_ERROR;
            *out = (int64_t)f;
            return 0;
        }
        case CCBT__Any:{
            return CC_UNIMPLEMENTED_ERROR;
        }
        case CCBT_char:
        case CCBT_bool:
        case CCBT_signed_char:
        case CCBT_short:
        case CCBT_int:
        case CCBT_long:
        case CCBT_long_long:
        case CCBT_unsigned_char:
        case CCBT_unsigned_short:
        case CCBT_unsigned:
        case CCBT_unsigned_long:
        case CCBT_unsigned_long_long:
            *out = v->integer;
            return 0;
        case CCBT_bfloat16:
        case CCBT_fp16:
        case CCBT_float16:
        case CCBT_long_double:
        case CCBT_float128:
        case CCBT_int128:
        case CCBT_unsigned_int128:
        case CCBT_float_complex:
        case CCBT_double_complex:
        case CCBT_long_double_complex:
        case CCBT_float16_complex:
        case CCBT_char_complex:
        case CCBT_signed_char_complex:
        case CCBT_unsigned_char_complex:
        case CCBT_short_complex:
        case CCBT_unsigned_short_complex:
        case CCBT_int_complex:
        case CCBT_unsigned_complex:
        case CCBT_long_complex:
        case CCBT_unsigned_long_complex:
        case CCBT_long_long_complex:
        case CCBT_unsigned_long_long_complex:
        case CCBT_int128_complex:
        case CCBT_unsigned_int128_complex:
            return CC_UNIMPLEMENTED_ERROR;
        case CCBT_INVALID:
        case CCBT_void:
        case CCBT_nullptr_t:
        case CCBT__Type:
        case CCBT_COUNT:
            return CC_UNREACHABLE_ERROR;
    }
}

static
int
cc_eval_to_u(CcParser* p, CcExpr* v, uint64_t* out){
    if(ccqt_has_pointer_representation(v->type)){
        *out = v->uinteger;
        return 0;
    }
    if(cc_eval_wide(v->type)){
        CcExpr converted = {.type = ccqt_basic(CCBT_unsigned_long_long)};
        int err = cc_eval_numeric_cast(p, v, &converted, 0);
        if(!err) *out = converted.uinteger;
        return err;
    }
    (void)p;
    CcQualType t = v->type;
    t = ccqt_enum_underlying(t);
    if(!ccqt_is_basic(t)) return CC_NOT_CONSTANT_ERROR;
    CcBasicTypeKind k = ccqt_basic_kind(t);
    switch(k){
        DRP_CASES_EXHAUSTED;
        case CCBT_double:{
            double d = v->double_;
            if(d != d || d <= -1.0 || d >= 18446744073709551616.0) return CC_OVERFLOW_ERROR;
            *out = (uint64_t)d;
            return 0;
        }
        case CCBT_float:{
            float f = v->float_;
            if(f != f || f <= -1.0f || f >= 18446744073709551616.0f) return CC_OVERFLOW_ERROR;
            *out = (uint64_t)f;
            return 0;
        }
        case CCBT__Any:
            return CC_UNIMPLEMENTED_ERROR;
        case CCBT_char:
        case CCBT_bool:
        case CCBT_signed_char:
        case CCBT_short:
        case CCBT_int:
        case CCBT_long:
        case CCBT_long_long:
        case CCBT_unsigned_char:
        case CCBT_unsigned_short:
        case CCBT_unsigned:
        case CCBT_unsigned_long:
        case CCBT_unsigned_long_long:
            *out = v->uinteger;
            return 0;
        case CCBT_bfloat16:
        case CCBT_fp16:
        case CCBT_float16:
        case CCBT_long_double:
        case CCBT_float128:
        case CCBT_int128:
        case CCBT_unsigned_int128:
        case CCBT_float_complex:
        case CCBT_double_complex:
        case CCBT_long_double_complex:
        case CCBT_float16_complex:
        case CCBT_char_complex:
        case CCBT_signed_char_complex:
        case CCBT_unsigned_char_complex:
        case CCBT_short_complex:
        case CCBT_unsigned_short_complex:
        case CCBT_int_complex:
        case CCBT_unsigned_complex:
        case CCBT_long_complex:
        case CCBT_unsigned_long_complex:
        case CCBT_long_long_complex:
        case CCBT_unsigned_long_long_complex:
        case CCBT_int128_complex:
        case CCBT_unsigned_int128_complex:
            return CC_UNIMPLEMENTED_ERROR;
        case CCBT_INVALID:
        case CCBT_void:
        case CCBT_nullptr_t:
        case CCBT__Type:
        case CCBT_COUNT:
            return CC_UNREACHABLE_ERROR;
    }
}

static
int
cc_eval_to_f(CcParser* p, CcExpr* v, float* out){
    if(cc_eval_wide(v->type)){
        CcExpr converted = {.type = ccqt_basic(CCBT_float)};
        int err = cc_eval_numeric_cast(p, v, &converted, 0);
        if(!err) *out = converted.float_;
        return err;
    }
    CcQualType t = v->type;
    t = ccqt_enum_underlying(t);
    if(!ccqt_is_basic(t)) return CC_NOT_CONSTANT_ERROR;
    CcBasicTypeKind k = ccqt_basic_kind(t);
    switch(k){
        DRP_CASES_EXHAUSTED;
        case CCBT_double:{
            double d = v->double_;
            if(d != d) return CC_NOT_CONSTANT_ERROR;
            if(d > (double)FLT_MAX || d < -(double)FLT_MAX) return CC_NOT_CONSTANT_ERROR;
            float f = (float)d;
            if((double)f != d) return CC_NOT_CONSTANT_ERROR;
            *out = f;
            return 0;
        }
        case CCBT_float:
            *out = v->float_;
            return 0;
        case CCBT_char:
            if(cc_target(p)->char_is_signed)
                goto signed_;
            goto unsigned_;
        case CCBT_bool:
        case CCBT_signed_char:
        case CCBT_short:
        case CCBT_int:
        case CCBT_long:
        case CCBT_long_long:
            signed_:;{
            float f = (float)v->integer;
            if(f >= 9223372036854775808.0f || f < -9223372036854775808.0f) return CC_NOT_CONSTANT_ERROR;
            if((int64_t)f != v->integer) return CC_NOT_CONSTANT_ERROR;
            *out = f;
            return 0;
        }
        case CCBT_unsigned_char:
        case CCBT_unsigned_short:
        case CCBT_unsigned:
        case CCBT_unsigned_long:
        case CCBT_unsigned_long_long:
            unsigned_:;{
            float f = (float)v->uinteger;
            if(f < 0.0f || f >= 18446744073709551616.0f) return CC_NOT_CONSTANT_ERROR;
            if((uint64_t)f != v->uinteger) return CC_NOT_CONSTANT_ERROR;
            *out = f;
            return 0;
        }
        case CCBT__Any:
            return CC_UNIMPLEMENTED_ERROR;
        case CCBT_int128:
        case CCBT_unsigned_int128:
        case CCBT_bfloat16:
        case CCBT_fp16:
        case CCBT_float16:
        case CCBT_long_double:
        case CCBT_float128:
        case CCBT_float_complex:
        case CCBT_double_complex:
        case CCBT_long_double_complex:
        case CCBT_float16_complex:
        case CCBT_char_complex:
        case CCBT_signed_char_complex:
        case CCBT_unsigned_char_complex:
        case CCBT_short_complex:
        case CCBT_unsigned_short_complex:
        case CCBT_int_complex:
        case CCBT_unsigned_complex:
        case CCBT_long_complex:
        case CCBT_unsigned_long_complex:
        case CCBT_long_long_complex:
        case CCBT_unsigned_long_long_complex:
        case CCBT_int128_complex:
        case CCBT_unsigned_int128_complex:
            return CC_UNIMPLEMENTED_ERROR;
        case CCBT_nullptr_t:
        case CCBT_INVALID:
        case CCBT_void:
        case CCBT__Type:
        case CCBT_COUNT:
            return CC_UNREACHABLE_ERROR;
    }
}

static
int
cc_eval_to_d(CcParser* p, CcExpr* v, double* out){
    if(cc_eval_wide(v->type)){
        CcExpr converted = {.type = ccqt_basic(CCBT_double)};
        int err = cc_eval_numeric_cast(p, v, &converted, 0);
        if(!err) *out = converted.double_;
        return err;
    }
    CcQualType t = v->type;
    t = ccqt_enum_underlying(t);
    if(!ccqt_is_basic(t)) return CC_NOT_CONSTANT_ERROR;
    CcBasicTypeKind k = ccqt_basic_kind(t);
    switch(k){
        DRP_CASES_EXHAUSTED;
        case CCBT_double:
            *out = v->double_;
            return 0;
        case CCBT_float:
            *out = (double)v->float_;
            return 0;
        case CCBT_char:
            if(cc_target(p)->char_is_signed)
                goto signed_;
            goto unsigned_;
        case CCBT_bool:
        case CCBT_signed_char:
        case CCBT_short:
        case CCBT_int:
        case CCBT_long:
        case CCBT_long_long:
            signed_:;{
            double d = (double)v->integer;
            if(d >= 9223372036854775808.0 || d < -9223372036854775808.0) return CC_NOT_CONSTANT_ERROR;
            if((int64_t)d != v->integer) return CC_NOT_CONSTANT_ERROR;
            *out = d;
            return 0;
        }
        case CCBT_unsigned_char:
        case CCBT_unsigned_short:
        case CCBT_unsigned:
        case CCBT_unsigned_long:
        case CCBT_unsigned_long_long:
            unsigned_:;{
            double d = (double)v->uinteger;
            if(d < 0.0 || d >= 18446744073709551616.0) return CC_NOT_CONSTANT_ERROR;
            if((uint64_t)d != v->uinteger) return CC_NOT_CONSTANT_ERROR;
            *out = d;
            return 0;
        }
        case CCBT_int128:
        case CCBT_unsigned_int128:
            return CC_UNIMPLEMENTED_ERROR;
        case CCBT__Any:
            return CC_UNIMPLEMENTED_ERROR;
        case CCBT_bfloat16:
        case CCBT_fp16:
        case CCBT_float16:
        case CCBT_long_double:
        case CCBT_float128:
        case CCBT_float_complex:
        case CCBT_double_complex:
        case CCBT_long_double_complex:
        case CCBT_float16_complex:
        case CCBT_char_complex:
        case CCBT_signed_char_complex:
        case CCBT_unsigned_char_complex:
        case CCBT_short_complex:
        case CCBT_unsigned_short_complex:
        case CCBT_int_complex:
        case CCBT_unsigned_complex:
        case CCBT_long_complex:
        case CCBT_unsigned_long_complex:
        case CCBT_long_long_complex:
        case CCBT_unsigned_long_long_complex:
        case CCBT_int128_complex:
        case CCBT_unsigned_int128_complex:
            return CC_UNIMPLEMENTED_ERROR;
        case CCBT_nullptr_t:
        case CCBT_INVALID:
        case CCBT_void:
        case CCBT__Type:
        case CCBT_COUNT:
            return CC_UNREACHABLE_ERROR;
    }
}

static
void
cc_eval_truncate(CcParser* p, CcExpr* node){
    CcQualType t = node->type;
    t = ccqt_enum_underlying(t);
    if(!ccqt_is_basic(t)) return;
    CcBasicTypeKind k = ccqt_basic_kind(t);
    if(!ccbt_is_integer(k)) return;
    uint32_t sz;
    if(cc_sizeof_as_uint(p, t, node->loc, &sz)) return;
    if(sz >= 8) return;
    uint32_t bits = sz * 8;
    uint64_t mask = ((uint64_t)1 << bits) - 1;
    if(ccbt_is_unsigned(k, !cc_target(p)->char_is_signed)){
        node->uinteger &= mask;
    }
    else {
        int64_t val = node->integer & (int64_t)mask;
        if(val & ((int64_t)1 << (bits - 1)))
            val |= ~(int64_t)mask;
        node->integer = val;
    }
}

static int cc_eval_object_scalar(CcEvalCtx*, CcExpr*, uint32_t, CcQualType, SrcLoc, const unsigned char*_Nullable, CcExpr*_Nullable*_Nonnull);

// Shift operands are promoted independently; the count may be wider than
// the value being shifted. Check it before any width-specific narrowing.
static
int
cc_eval_check_shift_count(CcParser* p, CcQualType left, CcExpr* right){
    uint32_t size;
    int err = cc_sizeof_as_uint(p, left, right->loc, &size);
    if(err) return err;
    CiUint128 count = cc_eval_u128(cc_target(p), right);
    return ci_uint128_hi(count) || ci_uint128_lo(count) >= (uint64_t)size * 8 ? CC_OVERFLOW_ERROR : 0;
}

static
int
cc_eval_check_any_view(CcEvalCtx* ctx, CcExpr* e){
    CcParser* p = ctx->parser;
    if(!ccqt_bt_eq(e->values[0]->type, CCBT__Any)
    || cc_field_path_count(e->field_path) != 1
    || cc_field_path_component(e->field_path, 0) != 1) return 0;
    CcExpr* tag;
    int err = cc_eval_object_scalar(ctx, e->values[0], 0, ccqt_basic(CCBT__Type), e->loc, NULL, &tag);
    if(err) return err;
    _Bool matches = tag->type_value.bits && tag->type_value.unqual == e->type.unqual;
    cc_release_expr(p, tag);
    return matches ? 0 : CC_NOT_CONSTANT_ERROR;
}

static
_Bool
cc_linktime_const_variable(CcExpr* e){
    CcQualType type = e->type;
    while(ccqt_is_fixed_sequence(type))
        type = ccqt_qualified_array_element(type);
    type = ccqt_unaligned(type);
    return e->var->initializer && (e->var->constexpr_ || type.is_const) && !type.is_volatile && !type.is_atomic;
}

// Addresses remain symbolic until an operation cancels the storage identity.
// In particular, offsets of different subobjects of one variable may be
// subtracted: this is intentionally more permissive than C's array-only rule.
typedef struct CcEvalAddress CcEvalAddress;
struct CcEvalAddress {
    enum { CC_EVAL_ABSOLUTE, CC_EVAL_VAR, CC_EVAL_FUNC, CC_EVAL_LITERAL } kind;
    const void* _Nullable symbol;
    int64_t offset;
    uint64_t literal_size;
    CcQualType literal_type;
    int64_t range_start;
    uint64_t range_size;
    _Bool has_range;
};

static
_Bool
cc_eval_address_nonnull(CcEvalAddress address){
    if(address.kind == CC_EVAL_ABSOLUTE) return 0;
    if(!address.offset) return 1;
    // An address within its known object, including one-past, cannot be null.
    return address.has_range && address.range_start >= 0
        && address.offset >= address.range_start
        && (uint64_t)address.offset - (uint64_t)address.range_start <= address.range_size;
}

static
_Bool
cc_eval_pointer_binary_expr(CcExpr* e){
    switch((unsigned)e->kind){
        case CC_EXPR_EQ: case CC_EXPR_NE:
            return ccqt_has_pointer_representation(e->lhs->type) && ccqt_has_pointer_representation(e->values[0]->type);
        case CC_EXPR_SUB:
        case CC_EXPR_LT: case CC_EXPR_LE: case CC_EXPR_GT: case CC_EXPR_GE:
            return ccqt_kind(e->lhs->type) == CC_POINTER
                && ccqt_kind(e->values[0]->type) == CC_POINTER;
        default: return 0;
    }
}

static int cc_eval_address(CcEvalCtx*, CcExpr*, _Bool, unsigned, CcEvalAddress*);
static int cc_eval_slice(CcEvalCtx*, CcExpr*, unsigned, uint64_t*, CcEvalAddress*);
static int cc_eval_object_view_select(CcEvalCtx*, CcExpr*, _Bool, _Bool*, CcExpr*_Nullable*_Nonnull);
static int cc_eval_object_view_range(CcEvalCtx*, CcExpr*, uint64_t, CcEvalObjectView*);
static int cc_check_linktime_expr(CcParser*, CcExpr*, _Bool, unsigned);

static
_Bool
cc_eval_object_access(CcExpr* e){
    return !cc_expr_field_bit_width(e) && (e->kind == CC_EXPR_DOT || e->kind == CC_EXPR_ARROW
        || e->kind == CC_EXPR_SUBSCRIPT || e->kind == CC_EXPR_DEREF);
}

static
int
cc_eval_comma_source(CcEvalCtx* ctx, CcExpr* e, _Bool storage, CcExpr*_Nonnull*_Nonnull out){
    CcExpr* assign = e->lhs;
    CcExpr* ref = e->values[0];
    if(assign->kind == CC_EXPR_ASSIGN && assign->lhs->kind == CC_EXPR_VARIABLE
        && ref->kind == CC_EXPR_VARIABLE && assign->lhs->var == ref->var
        && ref->var->name == nil_atom && (!ref->var->automatic || ref->var->constexpr_)){
        *out = storage ? ref : assign->values[0];
        return 0;
    }
    CcExpr* discard;
    int err = cc_eval_expr(ctx, assign, &discard);
    if(err == CC_NOT_CONSTANT_ERROR){
        // A discarded relocation needs no numeric address. Still validate
        // the expression so comma cannot hide effects or invalid arithmetic.
        err = cc_check_linktime_expr(ctx->parser, assign, 0, ctx->evaluation_depth);
        if(err) return err;
    }
    else {
        if(err) return err;
        cc_release_expr(ctx->parser, discard);
    }
    *out = ref;
    return 0;
}

// Resolve storage-preserving wrappers once for all kinds of object reads.
// The result borrows the original expression, preserving cycle identity.
static
int
cc_eval_object_source(CcEvalCtx* ctx, CcExpr* e, unsigned depth, CcExpr*_Nonnull*_Nonnull out){
    for(;;){
        if(depth++ >= 256 || (e->is_lvalue && (e->type.is_volatile || e->type.is_atomic)))
            return CC_NOT_CONSTANT_ERROR;
        if(e->kind == CC_EXPR_VARIABLE){
            if(!e->var->initializer || !(e->var->constexpr_ || (ctx->allow_const && cc_linktime_const_variable(e))))
                return CC_NOT_CONSTANT_ERROR;
            e = e->var->initializer;
        }
        else if(e->kind == CC_EXPR_OBJECT_VIEW
            || (e->kind == CC_EXPR_CAST && cc_conversion_preserves_representation(e->lhs->type, e->type)))
            e = e->lhs;
        else if(e->kind == CC_EXPR_TERNARY){
            _Bool truth;
            int err = cc_eval_truthy(ctx, e->lhs, &truth);
            if(err) return err;
            e = e->values[truth ? 0 : 1];
        }
        else if(e->kind == CC_EXPR_COMMA){
            int err = cc_eval_comma_source(ctx, e, 0, &e);
            if(err) return err;
        }
        else { *out = e; return 0; }
    }
}

static
_Bool
cc_eval_slice_expr(CcExpr* e){
    return ccqt_kind(e->type) == CC_SLICE
        && (e->kind == CC_EXPR_SLICE || e->kind == CC_EXPR_SLICE_LO || e->kind == CC_EXPR_SLICE_HI
            || e->kind == CC_EXPR_SLICE_ALL || (e->kind == CC_EXPR_CAST && e->type.unqual != e->lhs->type.unqual));
}

// Select a pointer-sized subobject from a constant initializer without turning
// a symbolic pointer into host bytes. The last overlapping initializer wins;
// partial overwrites cannot retain the original pointer's symbolic identity.
static
int
cc_eval_object_address(CcEvalCtx* ctx, CcExpr* e, uint64_t offset, uint32_t size, unsigned depth, CcEvalAddress* out){
    if(depth > 256 || e->type.is_volatile || e->type.is_atomic) return CC_NOT_CONSTANT_ERROR;
    CcParser* p = ctx->parser;
    uint32_t object_size;
    int err = cc_expr_sizeof_as_uint(p, e, &object_size);
    if(err) return err;
    if(offset > object_size || size > object_size - offset) return CC_NOT_CONSTANT_ERROR;
    CcExpr* source;
    err = cc_eval_object_source(ctx, e, depth, &source);
    if(err) return err;
    if(source != e) return cc_eval_object_address(ctx, source, offset, size, depth + 1, out);
    if(e->kind == CC_EXPR_TYPE_INTROSPECTION){
        CcExpr* value;
        err = cc_eval_expr(ctx, e, &value);
        if(err) return err;
        err = cc_eval_object_address(ctx, value, offset, size, depth + 1, out);
        cc_release_expr(p, value);
        return err;
    }
    if(cc_eval_object_access(e)){
        CcExpr storage = {0};
        CcEvalObjectView view = {.object=e, .storage=&storage, .depth=depth};
        err = cc_eval_object_view_range(ctx, e, offset, &view);
        if(err) return err;
        err = cc_eval_object_address(ctx, view.object, view.byte_offset, size, depth + 1, out);
        cc_field_path_free(cc_allocator(p), view.path);
        return err;
    }
    if(e->kind == CC_EXPR_CAST && ccqt_bt_eq(e->type, CCBT__Any)){
        if(ccqt_bt_eq(e->lhs->type, CCBT__Any))
            return cc_eval_object_address(ctx, e->lhs, offset, size, depth + 1, out);
        if(offset < offsetof(CiRtAny, payload)) return CC_NOT_CONSTANT_ERROR;
        return cc_eval_object_address(ctx, e->lhs, offset - offsetof(CiRtAny, payload), size, depth + 1, out);
    }
    if(cc_eval_slice_expr(e)){
        uint32_t ptr_size = cc_target(p)->sizeof_[CCBT_nullptr_t];
        if(offset != ptr_size || size != ptr_size) return CC_NOT_CONSTANT_ERROR;
        uint64_t count;
        return cc_eval_slice(ctx, e, depth + 1, &count, out);
    }
    if(e->kind == CC_EXPR_INIT_LIST || e->kind == CC_EXPR_COMPOUND_LITERAL){
        CcInitList* list = e->init_list;
        for(uint32_t i = list->count; i; i--){
            CcInitEntry* entry = &list->entries[i-1];
            uint64_t entry_offset;
            err = cc_field_path_offset(cc_target(p), e->type, entry->path, &entry_offset);
            if(err) return err;
            if(!entry->value) continue;
            uint32_t entry_size;
            err = cc_expr_sizeof_as_uint(p, entry->value, &entry_size);
            if(err) return err;
            uint64_t start = entry_offset;
            uint64_t end = start + entry_size;
            if(start >= offset + size || end <= offset) continue;
            if(cc_field_path_bit_width(e->type, entry->path) || start > offset || end < offset + size)
                return CC_NOT_CONSTANT_ERROR;
            return cc_eval_object_address(ctx, entry->value, offset - start, size, depth + 1, out);
        }
        *out = (CcEvalAddress){0};
        return 0;
    }
    if(!offset && size == object_size
        && ccqt_has_pointer_representation(e->type))
        return cc_eval_address(ctx, e, 0, depth + 1, out);
    return CC_NOT_CONSTANT_ERROR;
}

static
int
cc_eval_slice(CcEvalCtx* ctx, CcExpr* e, unsigned depth, uint64_t* count, CcEvalAddress* address){
    if(depth > 256 || e->type.is_volatile || e->type.is_atomic) return CC_NOT_CONSTANT_ERROR;
    CcParser* p = ctx->parser;
    uint32_t ptr_size = cc_target(p)->sizeof_[CCBT_nullptr_t];
    if(!cc_eval_slice_expr(e)){
        CcExpr* length;
        int err = cc_eval_object_scalar(ctx, e, 0, ccqt_basic(cc_target(p)->size_type), e->loc, NULL, &length);
        if(err) return err;
        *count = length->uinteger;
        cc_release_expr(p, length);
        return cc_eval_object_address(ctx, e, ptr_size, ptr_size, depth + 1, address);
    }
    CcExpr* base = e->lhs;
    CcTypeKind kind = ccqt_kind(base->type);
    uint64_t length = 0;
    _Bool have_length = (kind == CC_ARRAY || kind == CC_VECTOR) || kind == CC_SLICE;
    int err;
    if(kind == CC_SLICE){
        err = cc_eval_slice(ctx, base, depth + 1, &length, address);
        if(err) return err;
    }
    else {
        err = cc_eval_address(ctx, base, (kind == CC_ARRAY || kind == CC_VECTOR), depth + 1, address);
        if(err) return err;
        if(kind == CC_ARRAY || kind == CC_VECTOR){
            if(ccqt_array_incomplete(base->type)) return CC_NOT_CONSTANT_ERROR;
            length = ccqt_sequence_length(base->type);
        }
    }
    if(length > INT64_MAX) return CC_NOT_CONSTANT_ERROR;
    int64_t lo = 0, hi = (int64_t)length;
    if(e->kind == CC_EXPR_SLICE || e->kind == CC_EXPR_SLICE_LO){
        err = cc_eval_integer(ctx, e->values[0], &lo);
        if(err) return err;
    }
    if(e->kind == CC_EXPR_SLICE || e->kind == CC_EXPR_SLICE_HI){
        err = cc_eval_integer(ctx, e->values[e->kind == CC_EXPR_SLICE ? 1 : 0], &hi);
        if(err) return err;
    }
    else if(!have_length) return CC_NOT_CONSTANT_ERROR;
    if(lo < 0 || hi < lo || (have_length && (uint64_t)hi > length)) return CC_NOT_CONSTANT_ERROR;
    uint32_t element_size;
    err = cc_sizeof_as_uint(p, ccqt_as_slice(e->type)->pointee, e->loc, &element_size);
    if(err) return err;
    int64_t delta;
    if(mul_overflow(lo, (int64_t)element_size, &delta) || add_overflow(address->offset, delta, &address->offset))
        return CC_OVERFLOW_ERROR;
    *count = (uint64_t)(hi - lo);
    return 0;
}

static
int
cc_eval_address_value(CcEvalCtx* ctx, CcExpr* e, CcEvalAddress* out){
    if(ctx->evaluation_depth >= 256 || e->type.is_volatile || e->type.is_atomic)
        return CC_NOT_CONSTANT_ERROR;
    ctx->evaluation_depth++;
    _Bool handled;
    CcExpr* selected = NULL;
    int err = cc_eval_object_view_select(ctx, e, 1, &handled, &selected);
    if(err || handled){
        if(!err) err = cc_eval_address(ctx, selected, 0, 0, out);
        if(selected) cc_release_expr(ctx->parser, selected);
        ctx->evaluation_depth--;
        return err;
    }
    CcExpr* value;
    err = cc_eval_expr(ctx, e, &value);
    if(!err){
        if(value->kind == CC_EXPR_VALUE
            && (ccqt_kind(value->type) == CC_POINTER || ccqt_bt_eq(value->type, CCBT_nullptr_t)))
            *out = (CcEvalAddress){.offset = (int64_t)value->uinteger};
        else err = CC_NOT_CONSTANT_ERROR;
        cc_release_expr(ctx->parser, value);
    }
    if(err == CC_NOT_CONSTANT_ERROR){
        uint32_t size;
        err = cc_sizeof_as_uint(ctx->parser, e->type, e->loc, &size);
        if(!err) err = cc_eval_object_address(ctx, e, 0, size, 0, out);
    }
    ctx->evaluation_depth--;
    return err;
}

static
int
cc_eval_address(CcEvalCtx* ctx, CcExpr* e, _Bool lvalue, unsigned depth, CcEvalAddress* out){
    if(ccqt_kind(e->type) == CC_VLA) return CC_NOT_CONSTANT_ERROR;
    if(depth > 256) return CC_NOT_CONSTANT_ERROR;
    CcParser* p = ctx->parser;
    int err;
    switch((unsigned)e->kind){
        case CC_EXPR_VARIABLE:
            if(lvalue || ccqt_is_fixed_sequence(e->type)){
                if(e->var->automatic || e->var->thread_local_) return CC_NOT_CONSTANT_ERROR;
                *out = (CcEvalAddress){.kind = CC_EVAL_VAR, .symbol = e->var};
                uint32_t size;
                if(!cc_type_sizeof_complete(cc_target(p), e->type, &size, NULL)){
                    out->has_range = 1;
                    out->range_size = size;
                }
                return 0;
            }
            if(e->var->initializer && !e->type.is_volatile && !e->type.is_atomic
                && (e->var->constexpr_ || (ctx->allow_const && cc_linktime_const_variable(e))))
                return cc_eval_address(ctx, e->var->initializer, 0, depth + 1, out);
            return CC_NOT_CONSTANT_ERROR;
        case CC_EXPR_FUNCTION:
            *out = (CcEvalAddress){.kind = CC_EVAL_FUNC, .symbol = e->func};
            return 0;
        case CC_EXPR_BLOB:
        case CC_EXPR_VALUE:
            if(ccqt_is_fixed_sequence(e->type) && cc_expr_array_data(e)){
                uint32_t size;
                err = cc_sizeof_as_uint(p, e->type, e->loc, &size);
                if(err) return err;
                *out = (CcEvalAddress){.kind = CC_EVAL_LITERAL, .symbol = cc_expr_array_data(e),
                    .literal_size = size, .literal_type=e->type,
                    .has_range=1, .range_size=size};
                return 0;
            }
            if(!lvalue && ccqt_has_pointer_representation(e->type)){
                *out = (CcEvalAddress){.offset = (int64_t)e->uinteger};
                return 0;
            }
            return CC_NOT_CONSTANT_ERROR;
        case CC_EXPR_INIT_LIST: case CC_EXPR_COMPOUND_LITERAL:
            if(!lvalue && ccqt_has_pointer_representation(e->type)){
                uint32_t size;
                err = cc_sizeof_as_uint(p, e->type, e->loc, &size);
                if(err) return err;
                return cc_eval_object_address(ctx, e, 0, size, depth+1, out);
            }
            return CC_NOT_CONSTANT_ERROR;
        case CC_EXPR_ADDR:
            return cc_eval_address(ctx, e->lhs, 1, depth + 1, out);
        case CC_EXPR_DEREF:
            if(!lvalue && ccqt_has_pointer_representation(e->type))
                return cc_eval_address_value(ctx, e, out);
            return lvalue || ccqt_is_fixed_sequence(e->type)
                ? cc_eval_address(ctx, e->lhs, 0, depth + 1, out) : CC_NOT_CONSTANT_ERROR;
        case CC_EXPR_DOT: case CC_EXPR_ARROW:
            if(!lvalue && ccqt_has_pointer_representation(e->type))
                return cc_eval_address_value(ctx, e, out);
            if((!lvalue && !ccqt_is_fixed_sequence(e->type)) || cc_expr_field_bit_width(e))
                return CC_NOT_CONSTANT_ERROR;
            err = cc_eval_address(ctx, e->values[0], e->kind == CC_EXPR_DOT, depth + 1, out);
            if(err) return err;
            uint64_t field_offset;
            err = cc_expr_field_offset(cc_target(p), e, &field_offset);
            if(err) return err;
            if(field_offset > INT64_MAX
                || add_overflow(out->offset, (int64_t)field_offset, &out->offset))
                return CC_OVERFLOW_ERROR;
            if(!(ccqt_is_fixed_sequence(e->type) && ccqt_array_incomplete(e->type))){
                out->has_range = 1;
                out->range_start = out->offset;
                uint32_t size;
                err = cc_sizeof_as_uint(p, e->type, e->loc, &size);
                if(err) return err;
                out->range_size = size;
            }
            return 0;
        case CC_EXPR_SUBSCRIPT:{
            if(!lvalue && (ccqt_kind(e->type) == CC_POINTER || ccqt_bt_eq(e->type, CCBT_nullptr_t)))
                return cc_eval_address_value(ctx, e, out);
            if(!lvalue && !ccqt_is_fixed_sequence(e->type)) return CC_NOT_CONSTANT_ERROR;
            int64_t index, offset;
            err = cc_eval_integer(ctx, e->values[0], &index);
            if(err) return err;
            uint32_t size;
            if(cc_type_sizeof_complete(cc_target(p), e->type, &size, NULL))
                return CC_NOT_CONSTANT_ERROR;
            if(ccqt_kind(e->lhs->type) == CC_SLICE){
                uint64_t count;
                err = cc_eval_slice(ctx, e->lhs, depth + 1, &count, out);
                if(!err && (index < 0 || (uint64_t)index > count)) err = CC_NOT_CONSTANT_ERROR;
            }
            else
                err = cc_eval_address(ctx, e->lhs, ccqt_is_fixed_sequence(e->lhs->type), depth + 1, out);
            if(err) return err;
            if(ccqt_is_fixed_sequence(e->lhs->type) && !ccqt_array_incomplete(e->lhs->type)){
                out->has_range = 1;
                out->range_start = out->offset;
                uint32_t array_size;
                err = cc_sizeof_as_uint(p, e->lhs->type, e->loc, &array_size);
                if(err) return err;
                out->range_size = array_size;
            }
            if(mul_overflow(index, (int64_t)size, &offset) || add_overflow(out->offset, offset, &out->offset))
                return CC_OVERFLOW_ERROR;
            return 0;
        }
        case CC_EXPR_CAST:
            if(!ccqt_has_pointer_representation(e->type)) return CC_NOT_CONSTANT_ERROR;
            if(ccqt_has_pointer_representation(e->lhs->type) || ccqt_is_fixed_sequence(e->lhs->type)
                || ccqt_kind(e->lhs->type) == CC_FUNCTION)
                return cc_eval_address(ctx, e->lhs, ccqt_is_fixed_sequence(e->lhs->type), depth + 1, out);
            if(ccqt_is_integer(e->lhs->type)){
                CcExpr* value;
                err = cc_eval_expr(ctx, e, &value);
                if(err) return err;
                if(value->kind == CC_EXPR_VALUE && ccqt_has_pointer_representation(value->type))
                    *out = (CcEvalAddress){.offset = (int64_t)value->uinteger};
                else err = CC_NOT_CONSTANT_ERROR;
                cc_release_expr(p, value);
                return err;
            }
            return CC_NOT_CONSTANT_ERROR;
        case CC_EXPR_ADD: case CC_EXPR_SUB:{
            if(ccqt_kind(e->type) != CC_POINTER) return CC_NOT_CONSTANT_ERROR;
            CcExpr* base = e->lhs;
            CcExpr* index = e->values[0];
            if(e->kind == CC_EXPR_ADD && ccqt_kind(base->type) != CC_POINTER){
                base = e->values[0];
                index = e->lhs;
            }
            int64_t n, offset;
            err = cc_eval_integer(ctx, index, &n);
            if(err) return err;
            uint32_t size = 1;
            CcQualType pointee = ccqt_as_ptr(base->type)->pointee;
            if(!ccqt_bt_eq(pointee, CCBT_void)){
                if(cc_type_sizeof_complete(cc_target(p), pointee, &size, NULL))
                    return CC_NOT_CONSTANT_ERROR;
            }
            err = cc_eval_address(ctx, base, 0, depth + 1, out);
            if(err) return err;
            if(mul_overflow(n, (int64_t)size, &offset)) return CC_OVERFLOW_ERROR;
            if(e->kind == CC_EXPR_ADD)
                return add_overflow(out->offset, offset, &out->offset) ? CC_OVERFLOW_ERROR : 0;
            return sub_overflow(out->offset, offset, &out->offset) ? CC_OVERFLOW_ERROR : 0;
        }
        case CC_EXPR_TERNARY:{
            _Bool truth;
            err = cc_eval_truthy(ctx, e->lhs, &truth);
            if(err) return err;
            return cc_eval_address(ctx, e->values[truth ? 0 : 1], lvalue, depth + 1, out);
        }
        case CC_EXPR_COMMA:{
            CcExpr* source;
            err = cc_eval_comma_source(ctx, e, lvalue || ccqt_is_fixed_sequence(e->type), &source);
            if(err) return err;
            return cc_eval_address(ctx, source, lvalue, depth + 1, out);
        }
        default: return CC_NOT_CONSTANT_ERROR;
    }
}

static
int
cc_eval_pointer_binary(CcEvalCtx* ctx, CcExpr* e, int64_t* result){
    CcEvalAddress l, r;
    int err = cc_eval_address(ctx, e->lhs, 0, 0, &l);
    if(err) return err;
    err = cc_eval_address(ctx, e->values[0], 0, 0, &r);
    if(err) return err;
    if(l.kind != r.kind || l.symbol != r.symbol){
        // A symbol's address within its known object cannot be null. Other comparisons
        // between unrelated bases may depend on placement or literal merging.
        if(e->kind == CC_EXPR_EQ || e->kind == CC_EXPR_NE){
            _Bool left_null = l.kind == CC_EVAL_ABSOLUTE && !l.offset;
            _Bool right_null = r.kind == CC_EVAL_ABSOLUTE && !r.offset;
            if((left_null && cc_eval_address_nonnull(r))
                || (right_null && cc_eval_address_nonnull(l))){
                *result = e->kind == CC_EXPR_NE;
                return 0;
            }
        }
        return CC_NOT_CONSTANT_ERROR;
    }
    switch((unsigned)e->kind){
        case CC_EXPR_EQ: *result = l.offset == r.offset; return 0;
        case CC_EXPR_NE: *result = l.offset != r.offset; return 0;
        case CC_EXPR_LT: *result = l.kind == CC_EVAL_ABSOLUTE ? (uint64_t)l.offset < (uint64_t)r.offset : l.offset < r.offset; return 0;
        case CC_EXPR_LE: *result = l.kind == CC_EVAL_ABSOLUTE ? (uint64_t)l.offset <= (uint64_t)r.offset : l.offset <= r.offset; return 0;
        case CC_EXPR_GT: *result = l.kind == CC_EVAL_ABSOLUTE ? (uint64_t)l.offset > (uint64_t)r.offset : l.offset > r.offset; return 0;
        case CC_EXPR_GE: *result = l.kind == CC_EVAL_ABSOLUTE ? (uint64_t)l.offset >= (uint64_t)r.offset : l.offset >= r.offset; return 0;
        case CC_EXPR_SUB:{
            if(l.kind != CC_EVAL_VAR && l.kind != CC_EVAL_LITERAL) return CC_NOT_CONSTANT_ERROR;
            uint32_t size = 1;
            CcQualType pointee = ccqt_as_ptr(e->lhs->type)->pointee;
            if(!ccqt_bt_eq(pointee, CCBT_void)){
                if(cc_type_sizeof_complete(cc_target(ctx->parser), pointee, &size, NULL))
                    return CC_NOT_CONSTANT_ERROR;
            }
            int64_t delta;
            if(sub_overflow(l.offset, r.offset, &delta)) return CC_OVERFLOW_ERROR;
            if(!size || delta % size) return CC_NOT_CONSTANT_ERROR;
            delta /= size;
            uint32_t result_size;
            err = cc_sizeof_as_uint(ctx->parser, e->type, e->loc, &result_size);
            if(err) return err;
            if(result_size < 8){
                int64_t max = ((int64_t)1 << (result_size * 8 - 1)) - 1;
                if(delta < -max - 1 || delta > max) return CC_OVERFLOW_ERROR;
            }
            *result = delta;
            return 0;
        }
        default: return CC_NOT_CONSTANT_ERROR;
    }
}

static
int
cc_eval_symbolic_binary(CcParser* p, CcExpr* e, _Bool allow_const, int64_t* result){
    return cc_eval_pointer_binary(&(CcEvalCtx){.parser = p, .allow_const = allow_const}, e, result);
}

static
CcEvalCtx
cc_linktime_eval_ctx(CcParser* p){
    return (CcEvalCtx){.parser=p, .allow_const=1, .allow_inexact_float_casts=1};
}

static
int
cc_eval_linktime_expr(CcParser* p, CcExpr* e, CcExpr*_Nullable*_Nonnull result){
    CcEvalCtx ctx = cc_linktime_eval_ctx(p);
    return cc_eval_expr(&ctx, e, result);
}

static
int
cc_eval_linktime_scalar(CcParser* p, CcExpr* e, CcExpr*_Nullable*_Nonnull result){
    CcExpr* value;
    int err = cc_eval_linktime_expr(p, e, &value);
    if(err) return err;
    if(value->kind != CC_EXPR_VALUE){
        cc_release_expr(p, value);
        return CC_NOT_CONSTANT_ERROR;
    }
    *result = value;
    return 0;
}

// Integer relocations use the same symbolic base and checked byte offset as
// pointer constants. A numeric operand may adjust a symbol, but cannot replace
// its coefficient with -1 or combine it with a second symbol.
static
int
cc_eval_integer_address(CcEvalCtx* ctx, CcExpr* e, unsigned depth, CcEvalAddress* out){
    if(depth >= 256 || !ccqt_is_integer(e->type)) return CC_NOT_CONSTANT_ERROR;
    CcParser* p = ctx->parser;
    uint32_t size;
    int err = cc_sizeof_as_uint(p, e->type, e->loc, &size);
    if(err) return err;
    if(size < cc_target(p)->sizeof_[CCBT_nullptr_t] || size > sizeof(uint64_t)) return CC_NOT_CONSTANT_ERROR;
    CcExpr* source;
    err = cc_eval_object_source(ctx, e, depth, &source);
    if(err) return err;
    if(source != e) return cc_eval_integer_address(ctx, source, depth+1, out);
    if(e->kind == CC_EXPR_CAST){
        if(ccqt_has_pointer_representation(e->lhs->type)
        || ccqt_is_fixed_sequence(e->lhs->type)
        || ccqt_kind(e->lhs->type) == CC_FUNCTION)
            return cc_eval_address(ctx, e->lhs, ccqt_is_fixed_sequence(e->lhs->type), depth+1, out);
        return cc_eval_integer_address(ctx, e->lhs, depth+1, out);
    }
    if(e->kind == CC_EXPR_INIT_LIST || e->kind == CC_EXPR_COMPOUND_LITERAL){
        CcInitList* list = e->init_list;
        if(list->count == 1 && !cc_field_path_count(list->entries[0].path) && list->entries[0].value)
            return cc_eval_integer_address(ctx, list->entries[0].value, depth+1, out);
        return CC_NOT_CONSTANT_ERROR;
    }
    if(e->kind == CC_EXPR_DOT || e->kind == CC_EXPR_ARROW || e->kind == CC_EXPR_SUBSCRIPT || e->kind == CC_EXPR_DEREF){
        _Bool handled;
        CcExpr* value = NULL;
        err = cc_eval_object_view_select(ctx, e, 1, &handled, &value);
        if(!err) err = handled ? cc_eval_integer_address(ctx, (CcExpr*_Nonnull)value, depth+1, out) : CC_NOT_CONSTANT_ERROR;
        if(value) cc_release_expr(p, value);
        return err;
    }
    if(e->kind != CC_EXPR_ADD && e->kind != CC_EXPR_SUB) return CC_NOT_CONSTANT_ERROR;
    CcExpr* base = e->lhs;
    CcExpr* index = e->values[0];
    CcExpr* value;
    err = cc_eval_expr(ctx, index, &value);
    if(err == CC_NOT_CONSTANT_ERROR && e->kind == CC_EXPR_ADD){
        base = e->values[0];
        index = e->lhs;
        err = cc_eval_expr(ctx, index, &value);
    }
    if(err) return err;
    if(value->kind != CC_EXPR_VALUE || !ccqt_is_integer(value->type)){
        cc_release_expr(p, value);
        return CC_NOT_CONSTANT_ERROR;
    }
    CiUint128 bits = cc_eval_u128(cc_target(p), value);
    _Bool positive = !ci_uint128_hi(bits) && ci_uint128_lo(bits) <= INT64_MAX;
    _Bool negative = !ccqt_is_unsigned(value->type, !cc_target(p)->char_is_signed)
        && ci_uint128_hi(bits) == UINT64_MAX && ci_uint128_lo(bits) > INT64_MAX;
    cc_release_expr(p, value);
    if(!positive && !negative) return CC_NOT_CONSTANT_ERROR;
    int64_t offset = (int64_t)ci_uint128_lo(bits);
    err = cc_eval_integer_address(ctx, base, depth+1, out);
    if(err) return err;
    return (e->kind == CC_EXPR_ADD ? add_overflow(out->offset, offset, &out->offset)
        : sub_overflow(out->offset, offset, &out->offset)) ? CC_OVERFLOW_ERROR : 0;
}

static int cc_eval_object_bytes(CcEvalCtx*, CcExpr*, uint32_t, uint32_t, unsigned char*, const unsigned char*_Nullable);

// Opaque values may cross an aggregate view only into a matching typed slot.
// _Any has a metadata tag and can carry either opaque class in its payload.
static
_Bool
cc_eval_opaque_region(CcParser* p, CcQualType type, uint64_t offset, uint32_t size, _Bool metadata, unsigned depth){
    if(depth >= 256 || size != cc_target(p)->sizeof_[metadata ? CCBT__Type : CCBT_nullptr_t]) return 0;
    if(metadata && ccqt_bt_eq(type, CCBT__Type)) return !offset;
    if(!metadata && ccqt_has_pointer_representation(type)) return !offset;
    if(!metadata && ccqt_kind(type) == CC_SLICE)
        return offset == cc_target(p)->sizeof_[CCBT_nullptr_t];
    if(ccqt_bt_eq(type, CCBT__Any))
        return (metadata && !offset) || offset == offsetof(CiRtAny, payload);
    CcTypeKind kind = ccqt_kind(type);
    if(kind == CC_ARRAY || kind == CC_VECTOR){
        if(ccqt_array_incomplete(type)) return 0;
        uint32_t stride;
        if(cc_type_sizeof_complete(cc_target(p), ccqt_array_element(type), &stride, NULL))
            return 0;
        if(!stride || offset/stride >= ccqt_sequence_length(type))
            return 0;
        return cc_eval_opaque_region(p, ccqt_array_element(type), offset%stride, size, metadata, depth+1);
    }
    if(kind == CC_STRUCT || kind == CC_UNION){
        CcStruct* aggregate = ccqt_as_struct(type);
        for(uint32_t i = 0; i < aggregate->field_count; i++){
            CcField* field = &aggregate->fields[i];
            if(field->is_bitfield || offset < field->offset) continue;
            if(cc_eval_opaque_region(p, field->type, offset-field->offset, size, metadata, depth+1)) return 1;
        }
    }
    return 0;
}

static
int
cc_check_linktime_opaque_view(CcEvalCtx* ctx, CcExpr* root, uint32_t offset, uint32_t size){
    if(cc_eval_opaque_region(ctx->parser, root->type, offset, size, 1, 0)){
        CcExpr* value;
        int err = cc_eval_object_scalar(ctx, root, offset, ccqt_basic(CCBT__Type), root->loc, NULL, &value);
        if(!err) cc_release_expr(ctx->parser, value);
        if(err != CC_NOT_CONSTANT_ERROR) return err;
    }
    if(cc_eval_opaque_region(ctx->parser, root->type, offset, size, 0, 0)){
        CcEvalAddress address;
        int err = cc_eval_object_address(ctx, root, offset, size, 0, &address);
        if(err != CC_NOT_CONSTANT_ERROR) return err;
    }
    unsigned char bytes[16];
    if(size > sizeof bytes) return CC_NOT_CONSTANT_ERROR;
    return cc_eval_object_bytes(ctx, root, offset, size, bytes, NULL);
}

// Omitted members still have a typed zero value: an omitted pointer is not
// integer storage merely because its representation happens to be zero.
// A NULL root selects numeric-only checking of the unwritten bits in mask.
typedef struct CcEvalDefaultPath CcEvalDefaultPath;
struct CcEvalDefaultPath {
    const CcEvalDefaultPath*_Nullable parent;
    uint32_t index, count;
};

static
uint32_t
cc_eval_default_union_member(CcInitList* list, const CcEvalDefaultPath*_Nullable path){
    uint32_t count = path ? path->count : 0;
    for(uint32_t i = 0; i < list->count; i++){
        CcInitEntry* entry = &list->entries[i];
        if(!entry->value || cc_field_path_count(entry->path) <= count) continue;
        const CcEvalDefaultPath*_Nullable component = path;
        while(component && cc_field_path_component(entry->path, component->count-1) == component->index)
            component = component->parent;
        if(!component) return cc_field_path_component(entry->path, count);
    }
    return UINT32_MAX;
}

static
int
cc_check_linktime_default_view(CcEvalCtx* ctx, CcExpr*_Nullable root, CcQualType type,
    uint64_t offset, uint32_t size, uint32_t root_offset, const unsigned char*_Nullable mask, unsigned depth,
    CcInitList*_Nullable list, const CcEvalDefaultPath*_Nullable path){
    if(!size) return 0;
    if(mask){
        _Bool needed = 0;
        for(uint32_t i = 0; i < size; i++) needed |= mask[i] != 0;
        if(!needed) return 0;
    }
    if(depth >= 256) return CC_NOT_CONSTANT_ERROR;
    CcParser* p = ctx->parser;
    if(ccqt_bt_eq(type, CCBT__Type) || ccqt_has_pointer_representation(type)){
        if(!root) return CC_NOT_CONSTANT_ERROR;
        return cc_check_linktime_opaque_view(ctx, (CcExpr*_Nonnull)root, root_offset, size);
    }
    if(ccqt_bt_eq(type, CCBT__Any) || ccqt_kind(type) == CC_SLICE){
        _Bool any = ccqt_bt_eq(type, CCBT__Any);
        uint32_t start = any ? 0 : cc_target(p)->sizeof_[CCBT_nullptr_t];
        uint32_t end = start + cc_target(p)->sizeof_[any ? CCBT__Type : CCBT_nullptr_t];
        uint64_t lo = offset > start ? offset : start;
        uint64_t hi = offset+size < end ? offset+size : end;
        if(lo < hi){
            _Bool needed = !mask;
            if(mask) for(uint32_t i = (uint32_t)(lo-offset); i < hi-offset; i++) needed |= mask[i] != 0;
            if(needed){
                if(!root) return CC_NOT_CONSTANT_ERROR;
                return cc_check_linktime_opaque_view(ctx, (CcExpr*_Nonnull)root, root_offset+(uint32_t)(lo-offset), (uint32_t)(hi-lo));
            }
        }
        return 0;
    }
    CcTypeKind kind = ccqt_kind(type);
    if(kind == CC_ARRAY || kind == CC_VECTOR){
        uint32_t stride;
        int err = cc_type_sizeof_complete(cc_target(p), ccqt_array_element(type), &stride, NULL);
        if(err) return err;
        if(!stride) return CC_NOT_CONSTANT_ERROR;
        uint64_t first = offset/stride, last = (offset+size-1)/stride;
        for(uint64_t i = first; i <= last; i++){
            uint64_t start = i*stride, end = start+stride;
            uint64_t lo = offset > start ? offset : start;
            uint64_t hi = offset+size < end ? offset+size : end;
            CcEvalDefaultPath child = {.parent=path, .index=(uint32_t)i, .count=path ? path->count+1 : 1};
            err = cc_check_linktime_default_view(
                ctx, root,
                ccqt_array_element(type), lo-start,
                (uint32_t)(hi-lo), root_offset+(uint32_t)(lo-offset),
                mask ? mask+lo-offset : NULL, depth+1, list, &child
            );
            if(err) return err;
        }
    }
    else if(kind == CC_STRUCT || kind == CC_UNION){
        CcStruct* aggregate = ccqt_as_struct(type);
        uint32_t selected = kind == CC_UNION && list ? cc_eval_default_union_member((CcInitList*_Nonnull)list, path) : UINT32_MAX;
        for(uint32_t i = 0; i < aggregate->field_count; i++){
            if(selected != UINT32_MAX && i != selected) continue;
            CcField* field = &aggregate->fields[i];
            if(field->is_bitfield){
                if(kind == CC_UNION && field->name) return 0;
                continue;
            }
            // Flexible members contribute no storage, including tail padding.
            if(ccqt_is_fixed_sequence(field->type) && ccqt_array_incomplete(field->type))
                continue;
            uint64_t start = field->offset;
            uint32_t field_size;
            int err = cc_type_sizeof_complete(cc_target(p), field->type, &field_size, NULL);
            if(err) return err;
            uint64_t end = start + field_size;
            uint64_t lo = offset > start ? offset : start;
            uint64_t hi = offset+size < end ? offset+size : end;
            if(lo < hi){
                CcEvalDefaultPath child = {.parent=path, .index=i, .count=path ? path->count+1 : 1};
                err = cc_check_linktime_default_view(ctx, root, field->type, lo-start,
                    (uint32_t)(hi-lo), root_offset+(uint32_t)(lo-offset), mask ? mask+lo-offset : NULL, depth+1, list, &child);
                if(err) return err;
            }
            if(kind == CC_UNION) break;
        }
    }
    return 0;
}

// Follow the source representation of an aggregate view. A relocation may be
// copied whole, but a boundary through it requires concrete final bytes. Read
// those bytes from the original view so later designators can overwrite an
// earlier relocation, including writes in a containing initializer.
static
int
cc_check_linktime_view(CcEvalCtx* ctx, CcExpr* root, CcExpr* e, uint64_t offset, uint32_t size, uint32_t root_offset, unsigned depth){
    if(depth > 256) return CC_NOT_CONSTANT_ERROR;
    CcParser* p = ctx->parser;
    uint32_t object_size;
    int err = cc_expr_sizeof_as_uint(p, e, &object_size);
    if(err) return err;
    if(offset > object_size || size > object_size - offset) return CC_NOT_CONSTANT_ERROR;
    if(!size) return 0;
    CcExpr* source;
    err = cc_eval_object_source(ctx, e, depth, &source);
    if(err) return err;
    if(source != e) return cc_check_linktime_view(ctx, root, source, offset, size, root_offset, depth + 1);
    if(cc_eval_object_access(e)){
        CcExpr storage = {0};
        CcEvalObjectView view = {.object=e, .storage=&storage, .depth=depth};
        err = cc_eval_object_view_range(ctx, e, offset, &view);
        if(err) return err;
        err = cc_check_linktime_view(ctx, root, view.object, view.byte_offset, size, root_offset, depth + 1);
        cc_field_path_free(cc_allocator(p), view.path);
        return err;
    }
    if(e->kind == CC_EXPR_CAST && ccqt_bt_eq(e->type, CCBT__Any)){
        if(ccqt_bt_eq(e->lhs->type, CCBT__Any))
            return cc_check_linktime_view(ctx, root, e->lhs, offset, size, root_offset, depth + 1);
        uint32_t tag_size = cc_target(p)->sizeof_[CCBT__Type];
        if(offset < tag_size){
            uint32_t n = size < tag_size-offset ? size : tag_size-(uint32_t)offset;
            err = cc_check_linktime_opaque_view(ctx, root, root_offset, n);
            if(err) return err;
        }
        uint32_t payload_size;
        err = cc_sizeof_as_uint(p, e->lhs->type, e->loc, &payload_size);
        if(err) return err;
        uint64_t start = offsetof(CiRtAny, payload), end = start + payload_size;
        uint64_t lo = offset > start ? offset : start;
        uint64_t hi = offset + size < end ? offset + size : end;
        if(lo >= hi) return 0;
        return cc_check_linktime_view(ctx, root, e->lhs, lo - start, (uint32_t)(hi - lo), root_offset + (uint32_t)(lo - offset), depth + 1);
    }
    if(e->kind == CC_EXPR_INIT_LIST || e->kind == CC_EXPR_COMPOUND_LITERAL){
        CcInitList* list = e->init_list;
        for(uint32_t i = 0; i < list->count; i++){
            CcInitEntry* entry = &list->entries[i];
            uint64_t entry_offset;
            err = cc_field_path_offset(cc_target(p), e->type, entry->path, &entry_offset);
            if(err) return err;
            if(!entry->value || cc_field_path_bit_width(e->type, entry->path)) continue;
            uint32_t entry_size;
            err = cc_expr_sizeof_as_uint(p, entry->value, &entry_size);
            if(err) return err;
            uint64_t start = entry_offset, end = start + entry_size;
            uint64_t lo = offset > start ? offset : start;
            uint64_t hi = offset + size < end ? offset + size : end;
            if(lo >= hi) continue;
            err = cc_check_linktime_view(ctx, root, entry->value, lo - start, (uint32_t)(hi - lo), root_offset + (uint32_t)(lo - offset), depth + 1);
            if(err) return err;
        }
        return cc_check_linktime_default_view(ctx, root, e->type, offset, size, root_offset, NULL, depth+1, list, NULL);
    }
    if(e->kind == CC_EXPR_TYPE_INTROSPECTION){
        CcExpr* value;
        err = cc_eval_expr(ctx, e, &value);
        if(err) return err;
        err = cc_check_linktime_view(ctx, root, value, offset, size, root_offset, depth + 1);
        cc_release_expr(p, value);
        return err;
    }
    if(ccqt_bt_eq(e->type, CCBT__Type))
        return cc_check_linktime_opaque_view(ctx, root, root_offset, size);
    if(ccqt_has_pointer_representation(e->type))
        return cc_check_linktime_opaque_view(ctx, root, root_offset, size);
    if(ccqt_kind(e->type) == CC_SLICE
        && (e->kind == CC_EXPR_SLICE || e->kind == CC_EXPR_SLICE_LO || e->kind == CC_EXPR_SLICE_HI
            || e->kind == CC_EXPR_SLICE_ALL || e->kind == CC_EXPR_CAST)){
        uint32_t ptr_size = cc_target(p)->sizeof_[CCBT_nullptr_t];
        uint64_t lo = offset > ptr_size ? offset : ptr_size;
        uint64_t hi = offset + size < 2 * ptr_size ? offset + size : 2 * ptr_size;
        if(lo >= hi) return 0;
        return cc_check_linktime_opaque_view(ctx, root, root_offset+(uint32_t)(lo-offset), (uint32_t)(hi-lo));
    }
    CcExpr* address_expr = e;
    if(e->kind == CC_EXPR_CAST && ccqt_is_integer(e->type) && ccqt_kind(e->lhs->type) == CC_POINTER)
        address_expr = e->lhs;
    if(ccqt_kind(address_expr->type) == CC_POINTER && (offset || size != object_size)){
        CcEvalAddress address;
        err = cc_eval_address(ctx, address_expr, 0, 0, &address);
        if(err) return err;
        if(address.kind != CC_EVAL_ABSOLUTE){
            unsigned char bytes[16];
            if(size > sizeof bytes) return CC_NOT_CONSTANT_ERROR;
            return cc_eval_object_bytes(ctx, root, root_offset, size, bytes, NULL);
        }
    }
    return 0;
}

static int cc_check_linktime_expr_inner(CcParser*, CcExpr*, _Bool, _Bool, unsigned);
static _Bool cc_eval_aggregate_type(CcQualType);

static
int
cc_check_linktime_subobject(CcParser* p, CcExpr* e, unsigned depth){
    if(depth >= 256) return CC_NOT_CONSTANT_ERROR;
    CcEvalCtx ctx = cc_linktime_eval_ctx(p);
    _Bool handled;
    CcExpr* selected = NULL;
    int selection_err = cc_eval_object_view_select(&ctx, e, 1, &handled, &selected);
    if(selection_err || handled){
        if(!selection_err) selection_err = cc_check_linktime_expr_inner(p, (CcExpr*_Nonnull)selected, 0,
            cc_eval_aggregate_type(selected->type), depth + 1);
        if(selected) cc_release_expr(p, selected);
        return selection_err;
    }
    _Bool metadata = ccqt_bt_eq(e->type, CCBT__Type);
    if(ccqt_has_pointer_representation(e->type)){
        CcEvalAddress address;
        return cc_eval_address(&ctx, e, 0, 0, &address);
    }
    CcTypeKind kind = ccqt_kind(e->type);
    if(kind == CC_STRUCT || kind == CC_UNION || kind == CC_SLICE || ccqt_bt_eq(e->type, CCBT__Any)){
        uint32_t size;
        int err = cc_sizeof_as_uint(p, e->type, e->loc, &size);
        if(err) return err;
        return cc_check_linktime_view(&ctx, e, e, 0, size, 0, 0);
    }
    _Bool floating = ccqt_is_basic(e->type) && ccbt_is_float(ccqt_basic_kind(e->type));
    if(!metadata && !floating && !ccqt_is_integer(e->type)) return 0;
    uint32_t size;
    int err = cc_sizeof_as_uint(p, e->type, e->loc, &size);
    if(err) return err;
    // Numeric subobjects require actual numeric bytes. Opaque pointer and
    // metadata copies are handled separately above.
    CcExpr* value;
    err = cc_eval_linktime_scalar(p, e, &value);
    if(!err) cc_release_expr(p, value);
    return err;
}

static
int
cc_check_linktime_expr(CcParser* p, CcExpr* e, _Bool address, unsigned depth){
    return cc_check_linktime_expr_inner(p, e, address, 0, depth);
}

static
int
cc_check_linktime_expr_inner(CcParser* p, CcExpr* e, _Bool address, _Bool selected, unsigned depth){
    CcEvalCtx ctx = cc_linktime_eval_ctx(p);
    if(depth > 256) return CC_NOT_CONSTANT_ERROR;
    if(!address && e->is_lvalue && (e->type.is_volatile || e->type.is_atomic))
        return CC_NOT_CONSTANT_ERROR;
    if(cc_eval_pointer_binary_expr(e)){
        int64_t value;
        return cc_eval_pointer_binary(&ctx, e, &value);
    }
    int err;
    switch(e->kind){
        case CC_EXPR_OBJECT_VIEW:
            return cc_check_linktime_expr_inner(p, e->lhs, address, selected, depth + 1);
        case CC_EXPR_BLOB:
        case CC_EXPR_VALUE:
        case CC_EXPR_FUNCTION:
            return 0;
        case CC_EXPR_VARIABLE: {
            if(address) return (e->var->automatic || e->var->thread_local_) ? CC_NOT_CONSTANT_ERROR : 0;
            if(!cc_linktime_const_variable(e)) return CC_NOT_CONSTANT_ERROR;
            return cc_check_linktime_expr_inner(p, e->var->initializer, 0, selected, depth + 1);
        }
        case CC_EXPR_INIT_LIST: case CC_EXPR_COMPOUND_LITERAL:
            for(uint32_t i = 0; i < e->init_list->count; i++){
                if(!e->init_list->entries[i].value) continue;
                err = cc_check_linktime_expr_inner(p, e->init_list->entries[i].value, 0, selected, depth + 1);
                if(err) return err;
            }
            return 0;
        case CC_EXPR_ADDR:
            return cc_check_linktime_expr(p, e->lhs, 1, depth + 1);
        case CC_EXPR_DEREF:
            if(!address){
                err = cc_check_linktime_subobject(p, e, depth);
                if(err) return err;
            }
            return cc_check_linktime_expr(p, e->lhs, 0, depth + 1);
        case CC_EXPR_DOT:
            if(!address){
                err = cc_eval_check_any_view(&ctx, e);
                if(err == CC_NOT_CONSTANT_ERROR)
                    return cc_error(p, e->loc, "constant _Any.as requires the stored type, ignoring top-level qualifiers");
                if(err) return err;
                if(!selected){
                    err = cc_check_linktime_subobject(p, e, depth);
                    if(err) return err;
                }
            }
            // Selecting a field does not read its entire containing aggregate.
            return cc_check_linktime_expr_inner(p, e->values[0], address, !address, depth + 1);
        case CC_EXPR_ARROW:
            if(!address && !selected){
                err = cc_check_linktime_subobject(p, e, depth);
                if(err) return err;
            }
            return cc_check_linktime_expr(p, e->values[0], 0, depth + 1);
        case CC_EXPR_SUBSCRIPT:
            if(address && ccqt_kind(e->lhs->type) == CC_SLICE){
                CcEvalAddress value;
                return cc_eval_address(&ctx, e, 1, depth + 1, &value);
            }
            if(!address && !selected){
                err = cc_check_linktime_subobject(p, e, depth);
                if(err) return err;
            }
            err = cc_check_linktime_expr_inner(p, e->lhs, address, !address, depth + 1);
            if(err) return err;
            return cc_check_linktime_expr(p, e->values[0], 0, depth + 1);
        case CC_EXPR_CAST:
            err = cc_check_linktime_expr_inner(
                p, e->lhs,
                (ccqt_has_pointer_representation(e->type) || ccqt_kind(e->type) == CC_SLICE) && ccqt_is_fixed_sequence(e->lhs->type),
                selected && cc_conversion_preserves_representation(e->lhs->type, e->type),
                depth + 1
            );
            if(err) return err;
            if(!selected
            && ccqt_is_integer(e->type)
            && !ccqt_is_bool(e->type)
            && (ccqt_is_integer(e->lhs->type)
                || ccqt_has_pointer_representation(e->lhs->type)
                || ccqt_is_fixed_sequence(e->lhs->type)
                || ccqt_kind(e->lhs->type) == CC_FUNCTION)
            ){
                CcExpr* value;
                err = cc_eval_linktime_scalar(p, e, &value);
                if(!err) cc_release_expr(p, value);
                if(err == CC_NOT_CONSTANT_ERROR){
                    CcEvalAddress relocation;
                    err = cc_eval_integer_address(&ctx, e, 0, &relocation);
                }
                return err == CC_OVERFLOW_ERROR ? CC_NOT_CONSTANT_ERROR : err;
            }
            if(e->type.unqual != e->lhs->type.unqual
                && ((ccqt_is_basic(e->type) && ccbt_is_float(ccqt_basic_kind(e->type)))
                    || (ccqt_is_basic(e->lhs->type) && ccbt_is_float(ccqt_basic_kind(e->lhs->type))))
                && (ccqt_is_integer(e->type) || (ccqt_is_basic(e->type) && ccbt_is_float(ccqt_basic_kind(e->type))))){
                CcExpr* value;
                err = cc_eval_linktime_expr(p, e->lhs, &value);
                if(!err) cc_release_expr(p, value);
                if(err) return err;
            }
            if(ccqt_is_integer(e->type) && !ccqt_is_bool(e->type)
                && ccqt_is_basic(e->lhs->type) && ccbt_is_float(ccqt_basic_kind(e->lhs->type))){
                CcExpr* value;
                err = cc_eval_linktime_expr(p, e->lhs, &value);
                if(err && err != CC_NOT_CONSTANT_ERROR) return err;
                if(!err){
                    CcExpr converted = {.type=e->type, .loc=e->loc};
                    err = cc_eval_numeric_cast(p, value, &converted, 1);
                    cc_release_expr(p, value);
                    if(err == CC_OVERFLOW_ERROR)
                        return cc_error(p, e->loc, "static initializer conversion is out of range");
                    if(err) return err;
                }
            }
            return 0;
        case CC_EXPR_COMMA:
            // The parser represents a compound literal's anonymous storage as
            // an assignment followed by a reference to that storage.
            if(e->lhs->kind == CC_EXPR_ASSIGN && e->lhs->lhs->kind == CC_EXPR_VARIABLE
                && e->values[0]->kind == CC_EXPR_VARIABLE
                && e->lhs->lhs->var == e->values[0]->var
                && e->values[0]->var->name == nil_atom
                && (!e->values[0]->var->automatic || (!address && e->values[0]->var->constexpr_)))
                return cc_check_linktime_expr(p, e->lhs->values[0], 0, depth + 1);
            err = cc_check_linktime_expr(p, e->lhs, 0, depth + 1);
            if(err) return err;
            return cc_check_linktime_expr_inner(p, e->values[0], address, selected, depth + 1);
        case CC_EXPR_SLICE: case CC_EXPR_SLICE_ALL: case CC_EXPR_SLICE_LO: case CC_EXPR_SLICE_HI: {
            err = cc_check_linktime_expr(p, e->lhs, ccqt_is_fixed_sequence(e->lhs->type), depth + 1);
            if(err) return err;
            for(size_t i = 0, n = cc_expr_nvalues(e); i < n; i++){
                err = cc_check_linktime_expr_inner(p, e->values[i], 0, selected, depth + 1);
                if(err) return err;
            }
            uint64_t count;
            CcEvalAddress data;
            return cc_eval_slice(&ctx, e, depth, &count, &data);
        }
        case CC_EXPR_LOGAND: case CC_EXPR_LOGOR: case CC_EXPR_TERNARY: {
            err = cc_check_linktime_expr(p, e->lhs, 0, depth + 1);
            if(err) return err;
            _Bool truth;
            err = cc_eval_truthy(&ctx, e->lhs, &truth);
            if(err) return err;
            if(e->kind == CC_EXPR_TERNARY)
                return cc_check_linktime_expr_inner(p, e->values[truth ? 0 : 1], address, selected, depth + 1);
            if((e->kind == CC_EXPR_LOGAND && !truth) || (e->kind == CC_EXPR_LOGOR && truth))
                return 0;
            return cc_check_linktime_expr(p, e->values[0], 0, depth + 1);
        }
        case CC_EXPR_BIT_BUILTIN: {
            CcExpr* value;
            err = cc_eval_linktime_scalar(p, e, &value);
            if(!err) cc_release_expr(p, value);
            return err;
        }
        case CC_EXPR_NEG: case CC_EXPR_POS: case CC_EXPR_BITNOT: case CC_EXPR_LOGNOT:
        case CC_EXPR_ADD: case CC_EXPR_SUB: case CC_EXPR_MUL: case CC_EXPR_DIV: case CC_EXPR_MOD:
        case CC_EXPR_BITAND: case CC_EXPR_BITOR: case CC_EXPR_BITXOR:
        case CC_EXPR_EQ: case CC_EXPR_NE: case CC_EXPR_LT: case CC_EXPR_GT: case CC_EXPR_LE: case CC_EXPR_GE:
        case CC_EXPR_BSWAP:
            // Relocations can copy a floating representation, but arithmetic
            // on that representation requires its unresolved numeric value.
            if((ccqt_is_basic(e->lhs->type) && ccbt_is_float(ccqt_basic_kind(e->lhs->type)))
                || (ccqt_is_basic(e->type) && ccbt_is_float(ccqt_basic_kind(e->type)))){
                CcExpr* value;
                err = cc_eval_linktime_scalar(p, e, &value);
                if(!err) cc_release_expr(p, value);
                return err;
            }
            err = cc_check_linktime_expr_inner(
                p, e->lhs,
                ccqt_kind(e->type) == CC_POINTER && ccqt_is_fixed_sequence(e->lhs->type),
                selected, depth + 1
            );
            if(err) return err;
            // A selected aggregate may retain earlier writes that are fully
            // overwritten. Its storage reader evaluates only the needed bits.
            if(!selected && ccqt_is_integer(e->type) && ccqt_is_integer(e->lhs->type)){
                for(size_t i = 0, n = cc_expr_nvalues(e); i < n; i++){
                    err = cc_check_linktime_expr(p, e->values[i], 0, depth + 1);
                    if(err) return err;
                }
                CcExpr* value;
                err = cc_eval_linktime_scalar(p, e, &value);
                if(!err) cc_release_expr(p, value);
                if(err == CC_OVERFLOW_ERROR) return CC_NOT_CONSTANT_ERROR;
                // Integer addresses may retain a symbol plus an addend.
                // Other integer operations need a concrete numeric value.
                if(err == CC_NOT_CONSTANT_ERROR && (e->kind == CC_EXPR_ADD || e->kind == CC_EXPR_SUB)){
                    CcEvalAddress relocation;
                    err = cc_eval_integer_address(&ctx, e, 0, &relocation);
                    return err == CC_OVERFLOW_ERROR ? CC_NOT_CONSTANT_ERROR : err;
                }
                return err;
            }
            break;
        case CC_EXPR_LSHIFT: case CC_EXPR_RSHIFT:{
            err = cc_check_linktime_expr(p, e->lhs, 0, depth + 1);
            if(err) return err;
            CcExpr* count;
            err = cc_eval_linktime_scalar(p, e->values[0], &count);
            if(err) return err == CC_OVERFLOW_ERROR ? CC_NOT_CONSTANT_ERROR : err;
            err = cc_eval_check_shift_count(p, e->lhs->type, count);
            cc_release_expr(p, count);
            if(!err && !selected){
                CcExpr* value;
                err = cc_eval_linktime_scalar(p, e, &value);
                if(!err) cc_release_expr(p, value);
            }
            return err == CC_OVERFLOW_ERROR ? CC_NOT_CONSTANT_ERROR : err;
        }
        case CC_EXPR_TYPE_INTROSPECTION:{
            CcExpr* value;
            err = cc_eval_linktime_expr(p, e, &value);
            if(err) return err;
            err = cc_check_linktime_expr_inner(p, value, address, selected, depth + 1);
            cc_release_expr(p, value);
            return err;
        }
        default:
            return CC_NOT_CONSTANT_ERROR;
    }
    for(size_t i = 0, n = cc_expr_nvalues(e); i < n; i++){
        err = cc_check_linktime_expr_inner(p, e->values[i], 0, selected, depth + 1);
        if(err) return err;
    }
    return 0;
}

// Read integer/floating representations and padding. Addresses and compiler
// metadata are opaque, including their null or implicitly zero values.
static int cc_eval_object_bytes_inner(CcEvalCtx*, CcExpr*, uint32_t, uint32_t, unsigned char*, const unsigned char*_Nullable);

static
int
cc_eval_object_bytes(CcEvalCtx* ctx, CcExpr* e, uint32_t offset, uint32_t size, unsigned char* out, const unsigned char*_Nullable mask){
    if(ctx->evaluation_depth >= 256) return CC_NOT_CONSTANT_ERROR;
    if(e->is_lvalue && (e->type.is_volatile || e->type.is_atomic))
        return CC_NOT_CONSTANT_ERROR;
    for(CcEvalVisit* visit = ctx->objects; visit; visit = visit->previous)
        if(visit->expr == e && visit->offset == offset && visit->size == size)
            return CC_NOT_CONSTANT_ERROR;
    CcEvalVisit visit = {ctx->objects, e, offset, size};
    ctx->objects = &visit;
    ctx->evaluation_depth++;
    int err = cc_eval_object_bytes_inner(ctx, e, offset, size, out, mask);
    ctx->evaluation_depth--;
    ctx->objects = visit.previous;
    return err;
}

static
int
cc_eval_object_bytes_inner(CcEvalCtx* ctx, CcExpr* e, uint32_t offset, uint32_t size, unsigned char* out, const unsigned char*_Nullable mask){
    CcParser* p = ctx->parser;
    memset(out, 0, size);
    if(mask){
        _Bool needed = 0;
        for(uint32_t i = 0; i < size; i++) needed |= mask[i] != 0;
        if(!needed) return 0;
    }
    CcExpr* source;
    int source_err = cc_eval_object_source(ctx, e, ctx->evaluation_depth, &source);
    if(source_err) return source_err;
    if(source != e) return cc_eval_object_bytes(ctx, source, offset, size, out, mask);
    if(cc_eval_object_access(e)){
        CcExpr storage = {0};
        CcEvalObjectView view = {.object=e, .storage=&storage, .depth=ctx->evaluation_depth};
        int err = cc_eval_object_view_range(ctx, e, offset, &view);
        if(err) return err;
        if(view.byte_offset > UINT32_MAX) err = CC_NOT_CONSTANT_ERROR;
        else err = cc_eval_object_bytes(ctx, view.object, (uint32_t)view.byte_offset, size, out, mask);
        cc_field_path_free(cc_allocator(p), view.path);
        return err;
    }
    if((e->kind == CC_EXPR_BLOB || (e->kind == CC_EXPR_VALUE && ccqt_is_fixed_sequence(e->type))) && cc_expr_array_data(e) && cc_expr_array_count(e)){
        uint32_t elem_size;
        int err = cc_sizeof_as_uint(p, cc_expr_array_element(e), e->loc, &elem_size);
        if(err) return err;
        uint64_t length = (uint64_t)cc_expr_array_count(e) * elem_size;
        if(offset < length){
            uint32_t n = length - offset < size ? (uint32_t)(length - offset) : size;
            memcpy(out, cc_expr_array_data(e) + offset, n);
        }
        return 0;
    }
    if(e->kind == CC_EXPR_CAST && ccqt_bt_eq(e->type, CCBT__Any)){
        if(ccqt_bt_eq(e->lhs->type, CCBT__Any))
            return cc_eval_object_bytes(ctx, e->lhs, offset, size, out, mask);
        uint32_t payload_offset = offsetof(CiRtAny, payload);
        if(offset < payload_offset){
            uint32_t n = size < payload_offset - offset ? size : payload_offset - offset;
            // The tag is opaque compiler metadata, never numeric bytes.
            for(uint32_t i = 0; i < n; i++)
                if(!mask || mask[i]) return CC_NOT_CONSTANT_ERROR;
        }
        uint32_t src_size;
        int err = cc_sizeof_as_uint(p, e->lhs->type, e->loc, &src_size);
        if(err) return err;
        uint32_t lo = offset > payload_offset ? offset : payload_offset;
        uint32_t hi = offset + size < payload_offset + src_size ? offset + size : payload_offset + src_size;
        if(lo < hi)
            return cc_eval_object_bytes(ctx, e->lhs, lo - payload_offset, hi - lo, out + lo - offset, mask ? mask + lo - offset : NULL);
        return 0;
    }
    if(cc_eval_slice_expr(e)){
        uint64_t count;
        CcEvalAddress address;
        int err = cc_eval_slice(ctx, e, 0, &count, &address);
        if(err) return err;
        uint32_t ptr_size = cc_target(p)->sizeof_[CCBT_nullptr_t];
        if(offset > 2 * ptr_size || size > 2 * ptr_size - offset) return CC_NOT_CONSTANT_ERROR;
        if(offset < ptr_size){
            uint32_t n = size < ptr_size - offset ? size : ptr_size - offset;
            memcpy(out, (const unsigned char*)&count + offset, n);
        }
        uint32_t lo = offset > ptr_size ? offset : ptr_size;
        uint32_t hi = offset + size;
        if(lo < hi){
            _Bool needed = !mask;
            if(mask) for(uint32_t i = lo - offset; i < size; i++) needed |= mask[i] != 0;
            if(needed) return CC_NOT_CONSTANT_ERROR;
        }
        return 0;
    }
    if(e->kind == CC_EXPR_INIT_LIST || e->kind == CC_EXPR_COMPOUND_LITERAL){
        CcInitList* il = e->init_list;
        // Queries originate in scalar extraction, so at most 16 bytes are
        // needed. Resolve writes backwards, retaining the last write per bit.
        // An overwritten relocation must never be interpreted as host bytes.
        if(size > 16) return CC_NOT_CONSTANT_ERROR;
        unsigned char written[16] = {0};
        // Bits outside the requested field are already satisfied. They must
        // not force evaluation of an older, overlapping symbolic address.
        if(mask) for(uint32_t i = 0; i < size; i++) written[i] = (unsigned char)~mask[i];
        for(uint32_t i = il->count; i; i--){
            CcInitEntry* ent = &il->entries[i-1];
            if(!ent->value) continue;
            uint32_t sz;
            int err = cc_expr_sizeof_as_uint(p, ent->value, &sz);
            if(err) return err;
            uint64_t entry_offset;
            err = cc_field_path_offset(cc_target(p), e->type, ent->path, &entry_offset);
            if(err) return err;
            uint64_t start = entry_offset;
            uint64_t end = start + sz;
            if(start >= (uint64_t)offset + size || end <= offset) continue;
            uint32_t lo = start > offset ? (uint32_t)start : offset;
            uint32_t hi = end < (uint64_t)offset + size ? (uint32_t)end : offset + size;
            if(cc_field_path_bit_width(e->type, ent->path)){
                _Bool needed = 0;
                for(uint32_t byte = lo; byte < hi; byte++){
                    for(unsigned bit = 0; bit < 8; bit++){
                        uint64_t pos = ((uint64_t)byte - start) * 8 + bit;
                        if(pos >= cc_field_path_bit_offset(e->type, ent->path)
                            && pos - cc_field_path_bit_offset(e->type, ent->path) < cc_field_path_bit_width(e->type, ent->path)
                            && !(written[byte-offset] & (1u << bit))) needed = 1;
                    }
                }
                if(!needed) continue;
                CcExpr* value;
                err = cc_eval_expr(ctx, ent->value, &value);
                if(err) return err;
                CiUint128 bits = cc_eval_u128(cc_target(p), value);
                cc_release_expr(p, value);
                // Merge only this field's bits: adjacent fields can share bytes.
                for(uint32_t byte = lo; byte < hi; byte++){
                    for(unsigned bit = 0; bit < 8; bit++){
                        uint64_t pos = ((uint64_t)byte - start) * 8 + bit;
                        if(pos < cc_field_path_bit_offset(e->type, ent->path)) continue;
                        pos -= cc_field_path_bit_offset(e->type, ent->path);
                        if(pos >= cc_field_path_bit_width(e->type, ent->path)) continue;
                        unsigned char bit_mask = (unsigned char)(1u << bit);
                        if(written[byte-offset] & bit_mask) continue;
                        if(pos >= 128) return CC_NOT_CONSTANT_ERROR;
                        out[byte - offset] = (out[byte - offset] & ~bit_mask)
                            | ((ci_uint128_lo(ci_uint128_shr(bits, pos)) & 1) ? bit_mask : 0);
                        written[byte-offset] |= bit_mask;
                    }
                }
                continue;
            }
            for(uint32_t byte = lo; byte < hi;){
                if(written[byte-offset] == 0xff){
                    byte++;
                    continue;
                }
                uint32_t first = byte;
                while(byte < hi && written[byte-offset] != 0xff) byte++;
                unsigned char bytes[16];
                unsigned char needed[16];
                for(uint32_t at = first; at < byte; at++) needed[at-first] = (unsigned char)~written[at-offset];
                err = cc_eval_object_bytes(ctx, ent->value, first - (uint32_t)start, byte - first, bytes, needed);
                if(err) return err;
                for(uint32_t at = first; at < byte; at++){
                    unsigned char prior_mask = written[at-offset];
                    out[at-offset] = (out[at-offset] & prior_mask) | (bytes[at-first] & ~prior_mask);
                    written[at-offset] = 0xff;
                }
            }
        }
        unsigned char pending[16];
        for(uint32_t i = 0; i < size; i++) pending[i] = (unsigned char)~written[i];
        return cc_check_linktime_default_view(ctx, NULL, e->type, offset, size, 0, pending, ctx->evaluation_depth, il, NULL);
    }
    CcExpr* v;
    int err = cc_eval_expr(ctx, e, &v);
    if(err) return err;
    if(v->kind == CC_EXPR_INIT_LIST)
        err = cc_eval_object_bytes(ctx, v, offset, size, out, mask);
    else if(v->kind != CC_EXPR_VALUE)
        err = CC_NOT_CONSTANT_ERROR;
    else {
        CcQualType t = v->type;
        t = ccqt_enum_underlying(t);
        if(!ccqt_is_basic(t) || !(ccbt_is_integer(ccqt_basic_kind(t)) || ccbt_is_float(ccqt_basic_kind(t)))
            || (uint64_t)offset + size > cc_target(p)->sizeof_[ccqt_basic_kind(t)] || (uint64_t)offset + size > sizeof v->data)
            err = CC_NOT_CONSTANT_ERROR;
        else if(ccqt_bt_eq(t, CCBT_float)){
            if(offset + size > sizeof(float)) err = CC_NOT_CONSTANT_ERROR;
            else memcpy(out, (const unsigned char*)&v->float_ + offset, size);
        }
        else if(ccqt_bt_eq(t, CCBT_double) || cc_eval_wide(t) || ccqt_is_narrow_float(t))
            memcpy(out, v->data + offset, size);
        else if(ccbt_is_integer(ccqt_basic_kind(t)))
            memcpy(out, (const unsigned char*)&v->uinteger + offset, size);
        else err = CC_NOT_CONSTANT_ERROR;
    }
    cc_release_expr(p, v);
    return err;
}

// Type values hold compiler-owned metadata pointers. Byte reinterpretation
// must never fabricate them, even when the bytes are otherwise constant.
static
int
cc_eval_object_type(CcEvalCtx* ctx, CcExpr* e, uint64_t offset, unsigned depth, CcQualType* out){
    if(depth > 256 || e->type.is_volatile || e->type.is_atomic) return CC_NOT_CONSTANT_ERROR;
    CcParser* p = ctx->parser;
    uint32_t size, object_size;
    int err = cc_sizeof_as_uint(p, ccqt_basic(CCBT__Type), e->loc, &size);
    if(err) return err;
    err = cc_expr_sizeof_as_uint(p, e, &object_size);
    if(err) return err;
    if(offset > object_size || size > object_size - offset) return CC_NOT_CONSTANT_ERROR;
    CcExpr* source;
    err = cc_eval_object_source(ctx, e, depth, &source);
    if(err) return err;
    if(source != e) return cc_eval_object_type(ctx, source, offset, depth + 1, out);
    if(!offset && ccqt_bt_eq(e->type, CCBT__Type)
        && (e->kind == CC_EXPR_DOT || e->kind == CC_EXPR_SUBSCRIPT)){
        _Bool handled;
        CcExpr* selected = NULL;
        err = cc_eval_object_view_select(ctx, e, 1, &handled, &selected);
        if(err || handled){
            if(!err) err = cc_eval_object_type(ctx, selected, 0, depth + 1, out);
            if(selected) cc_release_expr(p, selected);
            return err;
        }
    }
    if(cc_eval_object_access(e)){
        CcExpr storage = {0};
        CcEvalObjectView view = {.object=e, .storage=&storage, .depth=depth};
        err = cc_eval_object_view_range(ctx, e, offset, &view);
        if(err) return err;
        err = cc_eval_object_type(ctx, view.object, view.byte_offset, depth + 1, out);
        cc_field_path_free(cc_allocator(p), view.path);
        return err;
    }
    if(e->kind == CC_EXPR_CAST && ccqt_bt_eq(e->type, CCBT__Any)){
        if(ccqt_bt_eq(e->lhs->type, CCBT__Any))
            return cc_eval_object_type(ctx, e->lhs, offset, depth + 1, out);
        if(!offset){
            *out = e->lhs->type;
            out->quals = 0;
            return 0;
        }
        if(offset < offsetof(CiRtAny, payload)) return CC_NOT_CONSTANT_ERROR;
        return cc_eval_object_type(ctx, e->lhs, offset - offsetof(CiRtAny, payload), depth + 1, out);
    }
    if(e->kind == CC_EXPR_INIT_LIST || e->kind == CC_EXPR_COMPOUND_LITERAL){
        CcInitList* list = e->init_list;
        for(uint32_t i = list->count; i; i--){
            CcInitEntry* entry = &list->entries[i-1];
            uint64_t entry_offset;
            err = cc_field_path_offset(cc_target(p), e->type, entry->path, &entry_offset);
            if(err) return err;
            if(!entry->value) continue;
            uint32_t entry_size;
            err = cc_expr_sizeof_as_uint(p, entry->value, &entry_size);
            if(err) return err;
            uint64_t start = entry_offset, end = start + entry_size;
            if(start >= offset + size || end <= offset) continue;
            if(cc_field_path_bit_width(e->type, entry->path) || start > offset || end < offset + size) return CC_NOT_CONSTANT_ERROR;
            return cc_eval_object_type(ctx, entry->value, offset - start, depth + 1, out);
        }
        *out = (CcQualType){0};
        return 0;
    }
    if(!offset && ccqt_bt_eq(e->type, CCBT__Type) && e->kind == CC_EXPR_VALUE){
        *out = e->type_value;
        return 0;
    }
    if(e->kind == CC_EXPR_TYPE_INTROSPECTION){
        CcExpr* value;
        err = cc_eval_expr(ctx, e, &value);
        if(err) return err;
        err = cc_eval_object_type(ctx, value, offset, depth + 1, out);
        cc_release_expr(p, value);
        return err;
    }
    return CC_NOT_CONSTANT_ERROR;
}

static
int
cc_eval_object_scalar(CcEvalCtx* ctx, CcExpr* base, uint32_t offset, CcQualType type, SrcLoc loc, const unsigned char*_Nullable mask, CcExpr*_Nullable*_Nonnull result){
    CcParser* p = ctx->parser;
    CcQualType t = type;
    t = ccqt_enum_underlying(t);
    _Bool pointer = ccqt_kind(t) == CC_POINTER || ccqt_kind(t) == CC_BLOCK_POINTER;
    if(!pointer){
        if(!ccqt_is_basic(t)) return CC_NOT_CONSTANT_ERROR;
        switch(ccqt_basic_kind(t)){
            case CCBT_bool:
            case CCBT_char:
            case CCBT_signed_char:
            case CCBT_unsigned_char:
            case CCBT_short:
            case CCBT_unsigned_short:
            case CCBT_int:
            case CCBT_unsigned:
            case CCBT_long:
            case CCBT_unsigned_long:
            case CCBT_long_long:
            case CCBT_unsigned_long_long:
            case CCBT_int128:
            case CCBT_unsigned_int128:
            case CCBT_fp16:
            case CCBT_float16:
            case CCBT_bfloat16:
            case CCBT_float:
            case CCBT_double:
            case CCBT_long_double:
            case CCBT_float128:
            case CCBT__Type:
            case CCBT_nullptr_t:
                break;
            default:
                return CC_NOT_CONSTANT_ERROR;
        }
    }
    uint32_t size;
    int err = cc_sizeof_as_uint(p, t, loc, &size);
    if(err) return err;
    if(size > 16) return CC_NOT_CONSTANT_ERROR;
    if(pointer || ccqt_bt_eq(t, CCBT_nullptr_t)){
        CcEvalAddress address;
        err = cc_eval_object_address(ctx, base, offset, size, ctx->evaluation_depth, &address);
        if(!err){
            if(address.kind != CC_EVAL_ABSOLUTE) return CC_NOT_CONSTANT_ERROR;
            CcExpr* node = cc_value_expr(p, loc, type);
            if(!node) return CC_OOM_ERROR;
            node->uinteger = (uint64_t)address.offset;
            *result = node;
            return 0;
        }
        if(err != CC_NOT_CONSTANT_ERROR) return err;
    }
    unsigned char bytes[16] = {0};
    if(ccqt_bt_eq(t, CCBT__Type)){
        CcQualType metadata = {0};
        err = cc_eval_object_type(ctx, base, offset, 0, &metadata);
        if(err == CC_NOT_CONSTANT_ERROR){
            err = cc_eval_object_bytes(ctx, base, offset, size, bytes, mask);
            if(!err) for(uint32_t i = 0; i < size; i++)
                if(bytes[i]){
                    err = CC_NOT_CONSTANT_ERROR;
                    break;
                }
        }
        if(err) return err;
        CcExpr* node = cc_value_expr(p, loc, type);
        if(!node) return CC_OOM_ERROR;
        node->type_value = metadata;
        *result = node;
        return 0;
    }
    else err = cc_eval_object_bytes(ctx, base, offset, size, bytes, mask);
    if(err) return err;
    // Numeric representation reads must not fabricate an invalid host _Bool.
    // Bitfield reads still need their containing bits until the caller extracts
    // and normalizes the field, so their intermediate masked value is allowed.
    if(ccqt_bt_eq(t, CCBT_bool) && !mask){
        for(uint32_t i = 0; i < size; i++)
            if(bytes[i] > (i ? 0 : 1)) return CC_NOT_CONSTANT_ERROR;
    }
    // Only actual numeric storage reaches this reconstruction. Pointer and
    // metadata storage never becomes an untyped byte buffer.
    CcExpr* node = cc_value_expr(p, loc, type);
    if(!node) return CC_OOM_ERROR;
    if(cc_eval_wide(t)) memcpy(node->data, bytes, size);
    else if(ccqt_bt_eq(t, CCBT_float)) memcpy(&node->float_, bytes, size);
    else if(ccqt_bt_eq(t, CCBT_double)) memcpy(&node->double_, bytes, size);
    else {
        memcpy(&node->uinteger, bytes, size);
        cc_eval_truncate(p, node);
    }
    *result = node;
    return 0;
}

static
int
cc_source_location_expr(CcParser* p, CcBuiltinOp op, SrcLoc loc, CcExpr*_Nullable*_Nonnull out){
    if(op == CC_BUILTIN_SRCLOC){
        CcExpr* value = cc_value_expr(p, loc, p->builtin_src_loc);
        if(!value) return CC_OOM_ERROR;
        value->loc_value = loc;
        *out = value;
        return 0;
    }
    size_t line = loc.line,
           column = loc.column,
           file_id = loc.file_id;
    if(loc.is_actually_a_pointer){
        SrcLocExp* exp = (SrcLocExp*)((uintptr_t)loc.pointer.bits << 1);
        while(exp->parent) exp = exp->parent;
        line = exp->line;
        column = exp->column;
        file_id = exp->file_id;
    }
    if(op == CC_BUILTIN_LINE || op == CC_BUILTIN_COLUMN){
        CcExpr* value = cc_int64_expr(p, loc, ccqt_basic(CCBT_unsigned), op == CC_BUILTIN_LINE ? line : column);
        if(!value) return CC_OOM_ERROR;
        *out = value;
        return 0;
    }
    StringView text = SV("");
    if(op == CC_BUILTIN_FILE){
        if(file_id >= p->cpp.fc->map.count) return CC_UNREACHABLE_ERROR;
        CStringView path = p->cpp.fc->map.data[file_id].path;
        text = (StringView){path.length, path.text};
    }
    else if(op == CC_BUILTIN_FUNCTION){
        if(p->current_func && p->current_func->name)
            text = (StringView){p->current_func->name->length, p->current_func->name->data};
    }
    else return CC_UNREACHABLE_ERROR;
    Atom atom = AT_atomize(p->cpp.at, text.text, text.length);
    if(!atom) return CC_OOM_ERROR;
    CcArray* array = cc_intern_array(&p->type_cache, cc_allocator(p), ccqt_basic(CCBT_char), text.length+1, 0);
    if(!array) return CC_OOM_ERROR;
    CcExpr* value = cc_value_expr(p, loc, (CcQualType){.bits=(uintptr_t)array});
    if(!value) return CC_OOM_ERROR;
    value->text = atom->data;
    value->str.length = (uint32_t)text.length+1;
    int err = cc_implicit_cast(p, value, p->const_char_star, out);
    if(err) cc_release_expr(p, value);
    return err;
}

static int cc_clone_expr_inner(CcParser*, CcExpr*, unsigned, SrcLoc, _Bool, Parray*_Nullable, CcExpr*_Nullable*_Nonnull);

static
int
cc_clone_default_type(CcParser* p, CcQualType type, unsigned depth, SrcLoc call_loc, _Bool defer_storage, Parray* vars, CcQualType* out){
    *out = type;
    if(!ccqt_is_vm(type)) return 0;
    if(depth >= 256) return CC_NOT_CONSTANT_ERROR;
    // Rebuild only temporary nodes here, then intern the transformed graph.
    // Captures are remapped by variable identity; unrelated typedefs retain
    // their canonical nodes and repeated references share the remapped type.
    union {
        CcAligned aligned;
        CcPointer pointer;
        CcSlice slice;
        CcArray array;
        CcVla vla;
    } storage;
    CcQualType* element;
    switch(ccqt_raw_kind(type)){
        case CC_ALIGNED: {
            CcAligned* copy = &storage.aligned;
            *copy = *ccqt_as_aligned(type);
            out->bits = (uintptr_t)copy;
            element = &copy->underlying;
            break;
        }
        case CC_POINTER: case CC_BLOCK_POINTER: {
            CcPointer* copy = &storage.pointer;
            *copy = *ccqt_as_ptr(type);
            out->bits = (uintptr_t)copy;
            element = &copy->pointee;
            break;
        }
        case CC_SLICE: {
            CcSlice* copy = &storage.slice;
            *copy = *ccqt_as_slice(type);
            out->bits = (uintptr_t)copy;
            element = &copy->pointee;
            break;
        }
        case CC_ARRAY: {
            CcArray* copy = &storage.array;
            *copy = *ccqt_as_array(type);
            out->bits = (uintptr_t)copy;
            element = &copy->element;
            break;
        }
        case CC_VLA: {
            CcVla* copy = &storage.vla;
            *copy = *ccqt_as_vla(type);
            int err = cc_clone_expr_inner(p, copy->bound, depth+1, call_loc, defer_storage, vars, (CcExpr*_Nullable*)&copy->bound);
            if(err) return err;
            out->bits = (uintptr_t)copy;
            element = &copy->element;
            break;
        }
        default: return CC_UNREACHABLE_ERROR;
    }
    out->quals = type.quals;
    int err = cc_clone_default_type(p, *element, depth+1, call_loc, defer_storage, vars, element);
    if(err){
        *out = CCQT_NONE;
        return err;
    }
    *out = cc_intern_qualtype(p, *out);
    return out->bits ? 0 : CC_OOM_ERROR;
}

static
int
cc_map_default_bound(CcParser* p, Parray* vars, CcVariable* source, _Bool clone){
    for(size_t i=0; i<vars->count; i+=2){
        if(vars->data[i] == source) return 0;
    }
    CcVariable* var = source;
    if(clone){
        int err = cc_new_vla_bound(p, source->loc, source->type, &var);
        if(err) return err;
    }
    if(pa_push(vars, cc_scratch_allocator(p), source) || pa_push(vars, cc_scratch_allocator(p), var))
        return CC_OOM_ERROR;
    return 0;
}

static
int
cc_clone_expr(CcParser* p, CcExpr* e, unsigned depth, SrcLoc call_loc, _Bool defer_storage, CcExpr*_Nullable*_Nonnull out){
    return cc_clone_expr_inner(p, e, depth, call_loc, defer_storage, NULL, out);
}

static
int
cc_prepare_default_arg(CcParser* p, CcExpr* e, unsigned depth, Parray* vars, _Bool clone){
    if(depth >= 256) return CC_NOT_CONSTANT_ERROR;
    if(e->kind == CC_EXPR_STATEMENT_EXPRESSION)
        return cc_error(p, e->loc, "default argument may not use a statement expression");
    int err;
    switch(e->kind){
        case CC_EXPR_VA:
            if(e->va.op == CC_VA_START){
                err = cc_check_va_start_context(p, e->loc);
                if(err) return err;
            }
            goto visit_lhs;
        case CC_EXPR_ASSIGN:
            if(e->lhs->kind == CC_EXPR_VARIABLE && e->lhs->var->vla_bound){
                err = cc_map_default_bound(p, vars, e->lhs->var, clone);
                if(err) return err;
            }
            goto visit_lhs;
        case CC_EXPR_INIT_LIST:
        case CC_EXPR_COMPOUND_LITERAL:
            for(uint32_t i = 0; i < e->init_list->count; i++){
                CcExpr* value = e->init_list->entries[i].value;
                if(!value) continue;
                err = cc_prepare_default_arg(p, value, depth+1, vars, clone);
                if(err) return err;
            }
            return 0;
        case CC_EXPR_VARIABLE: {
            CcVariable* var = e->var;
            if(!var->automatic || var->constexpr_) break;
            _Bool owned = 0;
            for(size_t i=0; i<vars->count; i+=2)
                if(vars->data[i] == var){ owned = 1; break; }
            if(!owned)
                return cc_error(p, e->loc, "default argument may not use automatic variable '%.*s'",
                    var->name->length, var->name->data);
            break;
        }
        case CC_EXPR_BLOB:
        case CC_EXPR_VALUE:
        case CC_EXPR_FUNCTION:
        case CC_EXPR_BUILTIN:
        case CC_EXPR_DOT:
        case CC_EXPR_ARROW:
            break;
        default:
        visit_lhs:
            if(e->lhs){
                err = cc_prepare_default_arg(p, e->lhs, depth+1, vars, clone);
                if(err) return err;
            }
            break;
    }
    for(size_t i = 0, n = cc_expr_nvalues(e); i < n; i++){
        if(!e->values[i]) continue;
        err = cc_prepare_default_arg(p, e->values[i], depth+1, vars, clone);
        if(err) return err;
    }
    return 0;
}

static
int
cc_clone_default_arg(CcParser* p, CcExpr* e, SrcLoc call_loc, CcParseFlags flags, CcExpr*_Nullable*_Nonnull out){
    // Unevaluated calls need the argument's type, but not caller-owned captures.
    if(flags.unevaluated)
        return cc_clone_expr(p, e, 0, call_loc, flags.parsing_default_arg, out);
    Parray vars = {0};
    int err = cc_prepare_default_arg(p, e, 0, &vars, 1);
    if(err) goto cleanup;
    err = cc_clone_expr_inner(p, e, 0, call_loc, flags.parsing_default_arg, vars.count ? &vars : NULL, out);
    cleanup:
    pa_cleanup(&vars, cc_scratch_allocator(p));
    return err;
}

static
int
cc_clone_expr_inner(CcParser* p, CcExpr* e, unsigned depth, SrcLoc call_loc, _Bool defer_storage, Parray*_Nullable vars, CcExpr*_Nullable*_Nonnull out){
    if(depth >= 256 || e->kind == CC_EXPR_STATEMENT_EXPRESSION) return CC_NOT_CONSTANT_ERROR;
    if(call_loc.bits && e->kind == CC_EXPR_BUILTIN && e->builtin.op >= CC_BUILTIN_LINE && e->builtin.op <= CC_BUILTIN_SRCLOC)
        return cc_source_location_expr(p, e->builtin.op, call_loc, out);
    size_t n = cc_expr_nvalues(e);
    CcExpr* copy = _cc_alloc_expr(p, n);
    if(!copy) return CC_OOM_ERROR;
    memcpy(copy, e, sizeof *copy);
    if(p->current_func && (e->kind == CC_EXPR_ALLOCA || e->kind == CC_EXPR_VA))
        p->current_func->inline_blocked = 1;
    _Bool lhs = 0;
    int err = 0;
    switch(e->kind){
        case CC_EXPR_BLOB:
        case CC_EXPR_VALUE:
        case CC_EXPR_FUNCTION:
        case CC_EXPR_BUILTIN:
            break;
        case CC_EXPR_VARIABLE:
            if(vars && e->var->vla_bound){
                for(size_t i = 0; i < vars->count; i += 2){
                    if(vars->data[i] == e->var){
                        copy->var = vars->data[i+1];
                        break;
                    }
                }
            }
            break;
        case CC_EXPR_INIT_LIST:
        case CC_EXPR_COMPOUND_LITERAL:
            if(!call_loc.bits){
                copy->init_list->rc++;
                break;
            }
            CcInitList* source = e->init_list;
            CcInitList* list = Allocator_zalloc_extra(cc_allocator(p), source->count, sizeof *list->entries, sizeof *list, _Alignof(CcInitList));
            if(!list){
                err = CC_OOM_ERROR;
                goto release_copy;
            }
            copy->init_list = list;
            copy->init_list->loc = source->loc;
            copy->init_list->count = source->count;
            for(uint32_t i = 0; i < source->count; i++){
                CcInitEntry* entry = &copy->init_list->entries[i];
                if(cc_field_path_concat(cc_allocator(p), (CcFieldPath){0}, source->entries[i].path, &entry->path)){
                    err = CC_OOM_ERROR;
                    goto cleanup;
                }
                if(source->entries[i].value){
                    err = cc_clone_expr_inner(p, source->entries[i].value, depth+1, call_loc, defer_storage, vars, &entry->value);
                    if(err) goto cleanup;
                }
            }
            break;
        case CC_EXPR_DOT: case CC_EXPR_ARROW:
            copy->field_path = (CcFieldPath){0};
            if(cc_field_path_concat(cc_allocator(p), (CcFieldPath){0}, e->field_path, &copy->field_path)){
                err = CC_OOM_ERROR;
                goto cleanup;
            }
            break;
        default:
            lhs = 1;
            copy->lhs = NULL;
            break;
    }
    if(lhs && e->lhs){
        err = cc_clone_expr_inner(p, e->lhs, depth+1, call_loc, defer_storage, vars, &copy->lhs);
        if(err) goto cleanup;
    }
    for(size_t i = 0; i < n; i++){
        if(!e->values[i]) continue;
        err = cc_clone_expr_inner(p, e->values[i], depth+1, call_loc, defer_storage, vars, &copy->values[i]);
        if(err) goto cleanup;
    }
    if(vars){
        Parray *vars_ = vars; // _Nonnull
        err = cc_clone_default_type(p, e->type, depth, call_loc, defer_storage, vars_, &copy->type);
        if(err) goto cleanup;
        if(e->kind == CC_EXPR_VALUE && ccqt_bt_eq(e->type, CCBT__Type)){
            err = cc_clone_default_type(p, e->type_value, depth, call_loc, defer_storage, vars_, &copy->type_value);
            if(err) goto cleanup;
        }
    }
    if(call_loc.bits && !defer_storage && copy->kind == CC_EXPR_COMPOUND_LITERAL && copy->defer_compound_storage){
        copy->defer_compound_storage = 0;
        err = cc_desugar_compound_literal(p, copy, out);
        if(err) goto cleanup;
        return 0;
    }
    *out = copy;
    return 0;
cleanup:
    if(copy->kind == CC_EXPR_INIT_LIST || copy->kind == CC_EXPR_COMPOUND_LITERAL){
        cc_release_expr(p, copy);
        return err;
    }
    for(size_t i = 0; i < n; i++)
        if(copy->values[i]) cc_release_expr(p, copy->values[i]);
    if(lhs && copy->lhs) cc_release_expr(p, copy->lhs);
    if(copy->kind == CC_EXPR_DOT || copy->kind == CC_EXPR_ARROW)
        cc_field_path_free(cc_allocator(p), copy->field_path);
release_copy:
    _cc_release_expr(p, copy, n);
    return err;
}

static
int
cc_eval_slice_value(CcEvalCtx* ctx, CcExpr* e, CcExpr*_Nullable*_Nonnull out){
    CcParser* p = ctx->parser;
    uint64_t count;
    CcEvalAddress address;
    int err = cc_eval_slice(ctx, e, ctx->evaluation_depth, &count, &address);
    if(err) return err;
    CcQualType pointer;
    err = cc_pointer_of(p, ccqt_as_slice(e->type)->pointee, &pointer);
    if(err) return err;
    CcExpr* data = NULL;
    if(address.kind == CC_EVAL_ABSOLUTE){
        data = cc_value_expr(p, e->loc, pointer);
        if(!data) return CC_OOM_ERROR;
        data->uinteger = (uint64_t)address.offset;
    }
    else {
        CcExpr* base = NULL;
        err = cc_clone_expr(p, e->lhs, ctx->evaluation_depth, (SrcLoc){0}, 0, &base);
        if(err) return err;
        if(ccqt_kind(base->type) == CC_SLICE){
            data = cc_make_expr(p, CC_EXPR_DOT, e->loc, pointer, 1);
            if(data){
                data->field_path = (CcFieldPath){.n_components=1, .idx0=1};
                data->values[0] = base;
            }
        }
        else data = cc_unary_expr(p, CC_EXPR_CAST, e->loc, pointer, base);
        if(!data){
            cc_release_expr(p, base);
            return CC_OOM_ERROR;
        }
        if(e->kind == CC_EXPR_SLICE || e->kind == CC_EXPR_SLICE_LO){
            int64_t lo;
            err = cc_eval_integer(ctx, e->values[0], &lo);
            if(err){
                cc_release_expr(p, data);
                return err;
            }
            if(lo){
                CcExpr* index = cc_int64_expr(p, e->loc, ccqt_basic(CCBT_long_long), lo);
                CcExpr* add = index ? cc_binary_expr(p, CC_EXPR_ADD, e->loc, pointer, data, index) : NULL;
                if(!add){
                    if(index) cc_release_expr(p, index);
                    cc_release_expr(p, data);
                    return CC_OOM_ERROR;
                }
                data = add;
            }
        }
    }
    CcExpr* length = cc_uint64_expr(p, e->loc, ccqt_basic(cc_target(p)->size_type), count);
    CcInitList* list = Allocator_zalloc_extra(cc_allocator(p), 2, sizeof *list->entries, sizeof *list, _Alignof(CcInitList));
    CcExpr* value = cc_make_expr(p, CC_EXPR_INIT_LIST, e->loc, e->type, 0);
    if(!length || !list || !value){
        cc_release_expr(p, data);
        if(length) cc_release_expr(p, length);
        if(list) Allocator_free_extra(cc_allocator(p), list, 2, sizeof *list->entries, sizeof *list, _Alignof(CcInitList));
        if(value) _cc_release_expr(p, value, 0);
        return CC_OOM_ERROR;
    }
    list->loc = e->loc;
    list->count = 2;
    list->entries[0] = (CcInitEntry){.path={.n_components=1, .idx0=0}, .value=length};
    list->entries[1] = (CcInitEntry){.path={.n_components=1, .idx0=1}, .value=data};
    value->init_list = list;
    *out = value;
    return 0;
}

static
_Bool
cc_eval_aggregate_type(CcQualType type){
    CcTypeKind kind = ccqt_kind(type);
    return kind == CC_STRUCT
        || kind == CC_UNION
        || kind == CC_ARRAY
        || kind == CC_VECTOR
        || kind == CC_SLICE
        || ccqt_bt_eq(type, CCBT__Any);
}

static
_Bool
cc_eval_default_path(CcQualType type, CcFieldPath path){
    for(uint32_t i = 0, n = cc_field_path_count(path); i < n; i++){
        uint32_t index = cc_field_path_component(path, i);
        CcTypeKind kind = ccqt_kind(type);
        if(kind == CC_STRUCT || kind == CC_UNION){
            CcStruct* aggregate = ccqt_as_struct(type);
            if(index >= aggregate->field_count) return 0;
            if(kind == CC_UNION){
                uint32_t first = 0;
                while(first < aggregate->field_count && aggregate->fields[first].is_bitfield
                    && !aggregate->fields[first].name) first++;
                if(index != first) return 0;
            }
            type = aggregate->fields[index].type;
        }
        else if(kind == CC_ARRAY || kind == CC_VECTOR)
            type = ccqt_array_element(type);
        else return 1; // Slice and _Any pseudo-members are checked separately.
    }
    return 1;
}

static
int
cc_eval_path_select(CcEvalCtx* ctx, CcExpr* base, CcFieldPath path, CcQualType type, unsigned depth,
    _Bool expression, _Bool* handled, CcExpr*_Nullable*_Nonnull out){
    CcParser* p = ctx->parser;
    *handled = 0;
    if(depth >= 256 || (base->is_lvalue && (base->type.is_volatile || base->type.is_atomic)))
        return CC_NOT_CONSTANT_ERROR;
    uint32_t count = cc_field_path_count(path);
    if(!count){
        int err = expression || cc_eval_aggregate_type(type) ? cc_clone_expr(p, base, depth, (SrcLoc){0}, 0, out)
            : cc_eval_expr(ctx, base, out);
        if(!err) *handled = 1;
        return err;
    }
    int source_err = cc_eval_object_source(ctx, base, depth, &base);
    if(source_err) return source_err;
    if(base->kind == CC_EXPR_TYPE_INTROSPECTION){
        CcExpr* value;
        int err = cc_eval_expr(ctx, base, &value);
        if(err) return err;
        err = cc_eval_path_select(ctx, value, path, type, depth+1, expression, handled, out);
        cc_release_expr(p, value);
        return err;
    }
    if(base->kind == CC_EXPR_DOT || base->kind == CC_EXPR_ARROW
        || base->kind == CC_EXPR_SUBSCRIPT || base->kind == CC_EXPR_DEREF){
        CcExpr* value;
        int err = cc_eval_expr(ctx, base, &value);
        if(err == CC_NOT_CONSTANT_ERROR) return 0;
        if(err) return err;
        if(value->kind == CC_EXPR_OBJECT_VIEW){
            cc_release_expr(p, value);
            return 0;
        }
        err = cc_eval_path_select(ctx, value, path, type, depth+1, expression, handled, out);
        cc_release_expr(p, value);
        return err;
    }
    if(base->kind == CC_EXPR_CAST && ccqt_bt_eq(base->type, CCBT__Any)){
        uint32_t index = cc_field_path_component(path, 0);
        if(!index && count == 1){
            CcExpr* tag = cc_value_expr(p, base->loc, type);
            if(!tag) return CC_OOM_ERROR;
            tag->type_value = (CcQualType){.unqual=base->lhs->type.unqual};
            *out = tag;
            *handled = 1;
            return 0;
        }
        if(index == 1){
            CcFieldPath tail;
            if(cc_field_path_drop(cc_allocator(p), path, 1, &tail)) return CC_OOM_ERROR;
            int err = cc_eval_path_select(ctx, base->lhs, tail, type, depth+1, expression, handled, out);
            cc_field_path_free(cc_allocator(p), tail);
            return err;
        }
        return 0;
    }
    if((base->kind == CC_EXPR_BLOB || (base->kind == CC_EXPR_VALUE && ccqt_is_fixed_sequence(base->type))) && cc_expr_array_data(base) && count == 1){
        uint32_t index = cc_field_path_component(path, 0), size;
        int err = cc_sizeof_as_uint(p, type, base->loc, &size);
        if(err) return err;
        CcExpr* value = cc_value_expr(p, base->loc, type);
        if(!value) return CC_OOM_ERROR;
        if(index < cc_expr_array_count(base)){
            if(size > sizeof value->data){
                cc_release_expr(p, value);
                return CC_NOT_CONSTANT_ERROR;
            }
            memcpy(value->data, cc_expr_array_data(base) + (size_t)index * size, size);
            cc_eval_truncate(p, value);
        }
        *out = value;
        *handled = 1;
        return 0;
    }
    if(base->kind != CC_EXPR_INIT_LIST && base->kind != CC_EXPR_COMPOUND_LITERAL) return 0;
    CcInitList* list = base->init_list;
    for(uint32_t i = 0; i < list->count; i++)
        if(list->entries[i].value && list->entries[i].value->kind == CC_EXPR_BLOB)
            return 0;
    _Bool aggregate = cc_eval_aggregate_type(type);
    uint32_t first = 0;
    CcExpr* replacement = NULL;
    for(uint32_t i = list->count; i; i--){
        CcInitEntry* entry = &list->entries[i-1];
        if(!entry->value || !cc_field_paths_overlap(base->type, path, entry->path)) continue;
        if(cc_field_path_is_prefix(entry->path, path)){
            CcFieldPath tail;
            if(cc_field_path_drop(cc_allocator(p), path, cc_field_path_count(entry->path), &tail)) return CC_OOM_ERROR;
            int err = cc_eval_path_select(ctx, entry->value, tail, type, depth+1, expression, handled, &replacement);
            cc_field_path_free(cc_allocator(p), tail);
            if(err || !*handled) return err;
            if(!aggregate){
                *out = replacement;
                return 0;
            }
            first = i;
            break;
        }
        if(!aggregate || !cc_field_path_is_prefix(path, entry->path)) return 0;
    }
    if(!replacement && !cc_eval_default_path(base->type, path)) return 0;
    if(!aggregate){
        *out = cc_value_expr(p, base->loc, type);
        if(!*out) return CC_OOM_ERROR;
        *handled = 1;
        return 0;
    }
    uint32_t n = replacement != NULL;
    for(uint32_t i = first; i < list->count; i++)
        if(list->entries[i].value && cc_field_path_is_prefix(path, list->entries[i].path)) n++;
    if(replacement && n == 1){
        *out = replacement;
        *handled = 1;
        return 0;
    }
    CcInitList* view = Allocator_zalloc_extra(cc_allocator(p), n, sizeof *view->entries, sizeof *view, _Alignof(CcInitList));
    CcExpr* value = cc_make_expr(p, CC_EXPR_INIT_LIST, base->loc, type, 0);
    if(!view || !value){
        if(replacement) cc_release_expr(p, replacement);
        if(view) Allocator_free_extra(cc_allocator(p), view, n, sizeof *view->entries, sizeof *view, _Alignof(CcInitList));
        if(value) _cc_release_expr(p, value, 0);
        return CC_OOM_ERROR;
    }
    view->loc = base->loc;
    view->count = n;
    value->init_list = view;
    uint32_t at = 0;
    if(replacement) view->entries[at++].value = replacement;
    int err = 0;
    for(uint32_t i = first; i < list->count && !err; i++){
        CcInitEntry* entry = &list->entries[i];
        if(!entry->value || !cc_field_path_is_prefix(path, entry->path)) continue;
        CcInitEntry* dest = &view->entries[at++];
        if(cc_field_path_drop(cc_allocator(p), entry->path, count, &dest->path)) err = CC_OOM_ERROR;
        if(!err) err = cc_clone_expr(p, entry->value, depth+1, (SrcLoc){0}, 0, &dest->value);
    }
    if(err){
        cc_release_expr(p, value);
        return err;
    }
    *handled = 1;
    *out = value;
    return 0;
}

static
int
cc_eval_offset_path(CcParser* p, CcQualType owner, uint64_t offset, CcQualType type,
    unsigned depth, CcFieldPath* path, _Bool* found){
    *found = 0;
    if(depth >= 256) return CC_NOT_CONSTANT_ERROR;
    if(!offset && owner.unqual == type.unqual){
        *path = (CcFieldPath){0};
        *found = 1;
        return 0;
    }
    CcTypeKind kind = ccqt_kind(owner);
    // FIXME: this is a mess of an expression
    uint32_t count = (kind == CC_ARRAY || kind == CC_VECTOR) ? 1
        : kind == CC_STRUCT || kind == CC_UNION ? ccqt_as_struct(owner)->field_count : 0;
    for(uint32_t i = 0; i < count; i++){
        uint32_t index = i;
        uint64_t start = 0;
        CcQualType child;
        if(kind == CC_ARRAY || kind == CC_VECTOR){
            if(ccqt_array_incomplete(owner))
                return CC_NOT_CONSTANT_ERROR;
            child = ccqt_array_element(owner);
            uint32_t size;
            int err = cc_sizeof_as_uint(p, child, (SrcLoc){0}, &size);
            if(err) return err;
            // FIXME: overflow intrins instead?
            if(!size || offset / size >= ccqt_sequence_length(owner) || offset / size > UINT32_MAX)
                return 0;
            index = (uint32_t)(offset / size);
            start = (uint64_t)index * size;
        }
        else {
            CcField* field = &ccqt_as_struct(owner)->fields[i];
            if(field->is_bitfield || field->offset > offset) continue;
            if(ccqt_is_fixed_sequence(field->type) && ccqt_array_incomplete(field->type))
                continue;
            start = field->offset;
            child = field->type;
        }
        uint32_t size;
        int err = cc_sizeof_as_uint(p, child, (SrcLoc){0}, &size);
        if(err) return err;
        if(offset - start >= size) continue;
        CcFieldPath tail = {0};
        err = cc_eval_offset_path(p, child, offset - start, type, depth+1, &tail, found);
        if(err) return err;
        if(!*found) continue;
        CcFieldPath step;
        if(cc_field_path_make(cc_allocator(p), &index, 1, &step)){
            cc_field_path_free(cc_allocator(p), tail);
            return CC_OOM_ERROR;
        }
        err = cc_field_path_concat(cc_allocator(p), step, tail, path) ? CC_OOM_ERROR : 0;
        cc_field_path_free(cc_allocator(p), step);
        cc_field_path_free(cc_allocator(p), tail);
        return err;
    }
    return 0;
}

static
int
cc_eval_object_view_address(CcEvalCtx* ctx, CcEvalObjectView* view, CcEvalAddress address, CcQualType type){
    CcParser* p = ctx->parser;
    if(address.offset < 0 || !address.symbol) return CC_NOT_CONSTANT_ERROR;
    if(address.kind == CC_EVAL_VAR){
        CcVariable* var = (CcVariable*)(uintptr_t)address.symbol;
        *view->storage = (CcExpr){.kind=CC_EXPR_VARIABLE, .is_lvalue=1, .type=var->type, .var=var};
        if(!(var->constexpr_ || (ctx->allow_const && cc_linktime_const_variable(view->storage))))
            return CC_NOT_CONSTANT_ERROR;
    }
    else if(address.kind == CC_EVAL_LITERAL){
        *view->storage = (CcExpr){.kind=CC_EXPR_VALUE, .type=address.literal_type, .text=(const char*_Nonnull)address.symbol};
        if(ccqt_sequence_length(address.literal_type) > UINT32_MAX)
            return CC_NOT_CONSTANT_ERROR;
        view->storage->str.length = (uint32_t)ccqt_sequence_length(address.literal_type);
    }
    else return CC_NOT_CONSTANT_ERROR;
    if(view->storage->type.is_volatile || view->storage->type.is_atomic) return CC_NOT_CONSTANT_ERROR;
    view->object = view->storage;
    uint32_t object_size, size = 0;
    int err = cc_sizeof_as_uint(p, view->storage->type, (SrcLoc){0}, &object_size);
    if(!err) err = cc_sizeof_as_uint(p, type, (SrcLoc){0}, &size);
    if(err) return err;
    if((uint64_t)address.offset > object_size || size > object_size - (uint64_t)address.offset)
        return CC_NOT_CONSTANT_ERROR;
    if(address.has_range){
        if(address.offset < address.range_start) return CC_NOT_CONSTANT_ERROR;
        uint64_t delta = (uint64_t)address.offset - (uint64_t)address.range_start;
        if(delta > address.range_size || size > address.range_size - delta)
            return CC_NOT_CONSTANT_ERROR;
    }
    CcFieldPath prefix = {0};
    _Bool found;
    err = cc_eval_offset_path(p, view->storage->type, (uint64_t)address.offset, type, view->depth, &prefix, &found);
    if(err) return err;
    if(found){
        CcFieldPath joined;
        err = cc_field_path_concat(cc_allocator(p), prefix, view->path, &joined) ? CC_OOM_ERROR : 0;
        cc_field_path_free(cc_allocator(p), prefix);
        if(err) return err;
        cc_field_path_free(cc_allocator(p), view->path);
        view->path = joined;
    }
    else view->representation = 1;
    return add_overflow(view->byte_offset, (uint64_t)address.offset, &view->byte_offset) ? CC_OVERFLOW_ERROR : 0;
}

static
int
cc_eval_object_view_open(CcEvalCtx* ctx, CcExpr* e, CcEvalObjectView* view){
    CcParser* p = ctx->parser;
    CcExpr* base = e;
    int err = 0;
    while(base->kind == CC_EXPR_DOT || base->kind == CC_EXPR_ARROW
        || base->kind == CC_EXPR_SUBSCRIPT || base->kind == CC_EXPR_DEREF){
        if(view->depth++ >= 256){
            err = CC_NOT_CONSTANT_ERROR;
            goto cleanup;
        }
        CcFieldPath step = {0};
        uint64_t step_offset = 0;
        if(base->kind == CC_EXPR_DEREF || (base->kind == CC_EXPR_SUBSCRIPT && !ccqt_is_fixed_sequence(base->lhs->type))){
            if(base->kind == CC_EXPR_SUBSCRIPT && ccqt_kind(base->lhs->type) == CC_SLICE){
                int64_t index;
                uint64_t count = 0;
                CcEvalAddress data;
                err = cc_eval_integer(ctx, base->values[0], &index);
                if(!err) err = cc_eval_slice(ctx, base->lhs, view->depth, &count, &data);
                if(err) goto cleanup;
                if(index < 0 || (uint64_t)index >= count){
                    err = CC_NOT_CONSTANT_ERROR;
                    goto cleanup;
                }
            }
            CcEvalAddress address;
            err = base->kind == CC_EXPR_DEREF ? cc_eval_address(ctx, base->lhs, 0, view->depth, &address)
                : cc_eval_address(ctx, base, 1, view->depth, &address);
            if(!err) err = cc_eval_object_view_address(ctx, view, address, base->type);
            if(err) goto cleanup;
            base = view->object;
            break;
        }
        CcExpr* indirect = NULL;
        CcQualType indirect_type = {0};
        if(base->kind == CC_EXPR_DOT || base->kind == CC_EXPR_ARROW){
            err = cc_eval_check_any_view(ctx, base);
            if(err) goto cleanup;
            step = base->field_path;
            err = cc_field_path_offset(cc_target(p), cc_expr_field_owner(base), step, &step_offset);
            if(err) goto cleanup;
            if(base->kind == CC_EXPR_ARROW){
                indirect = base->values[0];
                indirect_type = cc_expr_field_owner(base);
            }
            base = base->values[0];
        }
        else {
            int64_t index;
            err = cc_eval_integer(ctx, base->values[0], &index);
            if(err) goto cleanup;
            if(index < 0 || (uint64_t)index >= ccqt_sequence_length(base->lhs->type) || (uint64_t)index > UINT32_MAX){
                err = CC_NOT_CONSTANT_ERROR;
                goto cleanup;
            }
            uint32_t component = (uint32_t)index;
            if(cc_field_path_make(cc_allocator(p), &component, 1, &step)){
                err = CC_OOM_ERROR;
                goto cleanup;
            }
            err = cc_field_path_offset(cc_target(p), base->lhs->type, step, &step_offset);
            if(err) goto cleanup;
            base = base->lhs;
        }
        if(add_overflow(view->byte_offset, step_offset, &view->byte_offset)){
            err = CC_OVERFLOW_ERROR;
            goto cleanup;
        }
        CcFieldPath joined;
        if(cc_field_path_concat(cc_allocator(p), step, view->path, &joined)){
            err = CC_OOM_ERROR;
            goto cleanup;
        }
        cc_field_path_free(cc_allocator(p), view->path);
        view->path = joined;
        if(indirect){
            CcEvalAddress address;
            err = cc_eval_address(ctx, indirect, 0, view->depth, &address);
            if(!err) err = cc_eval_object_view_address(ctx, view, address, indirect_type);
            if(err) goto cleanup;
            base = view->object;
            break;
        }
    }
    view->object = base;
    return 0;
    cleanup:
    cc_field_path_free(cc_allocator(p), view->path);
    view->path = (CcFieldPath){0};
    return err;
}

static
int
cc_eval_linktime_object_view(CcParser* p, CcExpr* e, CcEvalObjectView* view){
    CcEvalCtx ctx = cc_linktime_eval_ctx(p);
    return cc_eval_object_view_open(&ctx, e, view);
}

static
int
cc_eval_object_view_range(CcEvalCtx* ctx, CcExpr* e, uint64_t offset, CcEvalObjectView* view){
    int err = cc_eval_object_view_open(ctx, e, view);
    if(err) return err;
    if(add_overflow(view->byte_offset, offset, &view->byte_offset)){
        cc_field_path_free(cc_allocator(ctx->parser), view->path);
        view->path = (CcFieldPath){0};
        return CC_OVERFLOW_ERROR;
    }
    return 0;
}

static
int
cc_eval_object_view_select(CcEvalCtx* ctx, CcExpr* e, _Bool expression, _Bool* handled, CcExpr*_Nullable*_Nonnull out){
    CcParser* p = ctx->parser;
    CcExpr storage = {0};
    CcEvalObjectView view = {.object=e, .storage=&storage};
    *handled = 0;
    int err = cc_eval_object_view_open(ctx, e, &view);
    if(err) return err;
    uint32_t bit_shift = 0;
    CcExpr* selected = NULL;
    if(!view.representation){
        err = cc_eval_path_select(ctx, view.object, view.path, e->type, view.depth, expression, handled, &selected);
        if(err) goto cleanup;
    }
    if(!expression && !*handled && !cc_eval_aggregate_type(e->type)){
        unsigned char requested[16] = {0};
        uint32_t width = cc_expr_field_bit_width(e);
        if(view.byte_offset > UINT32_MAX){
            err = CC_NOT_CONSTANT_ERROR;
            goto cleanup;
        }
        if(width){
            uint32_t size;
            err = cc_sizeof_as_uint(p, e->type, e->loc, &size);
            if(err) goto cleanup;
            bit_shift = cc_expr_field_bit_offset(e);
            if(size > sizeof requested || bit_shift >= 128 || bit_shift > size * 8 || width > size * 8 - bit_shift){
                err = CC_NOT_CONSTANT_ERROR;
                goto cleanup;
            }
            for(uint32_t bit = bit_shift; bit < bit_shift + width; bit++)
                requested[bit / 8] |= 1u << (bit % 8);
        }
        err = cc_eval_object_scalar(ctx, view.object, (uint32_t)view.byte_offset,
            e->type, e->loc, width ? requested : NULL, &selected);
        if(err == CC_NOT_CONSTANT_ERROR) err = 0;
        else {
            if(err) goto cleanup;
            *handled = 1;
        }
    }
    if(!expression && !*handled && cc_eval_aggregate_type(e->type)){
        uint32_t size = 0;
        err = cc_check_linktime_expr_inner(p, e, 0, 1, 0);
        if(err) goto cleanup;
        err = cc_sizeof_as_uint(p, e->type, e->loc, &size);
        if(err) goto cleanup;
        err = cc_check_linktime_view(ctx, e, e, 0, size, 0, 0);
        if(err) goto cleanup;
        selected = cc_make_expr(p, CC_EXPR_OBJECT_VIEW, e->loc, e->type, 0);
        if(!selected){
            err = CC_OOM_ERROR;
            goto cleanup;
        }
        err = cc_clone_expr(p, e, view.depth, (SrcLoc){0}, 0, &selected->lhs);
        if(err){
            cc_release_expr(p, selected);
            goto cleanup;
        }
        *handled = 1;
    }
    if(!*handled) goto cleanup;
    if(!expression && cc_eval_aggregate_type(e->type)){
        err = cc_eval_expr(ctx, selected, out);
        cc_release_expr(p, selected);
    }
    else *out = selected;
    if(err) goto cleanup;
    CcExpr* value = *out;
    if(!value){
        err = CC_UNREACHABLE_ERROR;
        goto cleanup;
    }
    if(!cc_types_compatible_unqualified(value->type, e->type)){
        cc_release_expr(p, value);
        *out = NULL;
        *handled = 0;
        goto cleanup;
    }
    value->type = e->type;
    uint32_t width = expression ? 0 : cc_expr_field_bit_width(e);
    if(width){
        CiUint128 bits = cc_eval_u128(cc_target(p), value);
        if(bit_shift) bits = ci_uint128_shr(bits, bit_shift);
        CiUint128 mask = width == 128 ? ci_uint128_from_int64(-1)
            : ci_uint128_sub(ci_uint128_shl(ci_uint128_from_uint64(1), width), ci_uint128_from_uint64(1));
        bits = ci_uint128_and(bits, mask);
        if(!ccqt_is_unsigned(e->type, !cc_target(p)->char_is_signed)
            && ci_uint128_nonzero(ci_uint128_shr(bits, width-1)))
            bits = ci_uint128_or(bits, ci_uint128_xor(mask, ci_uint128_from_int64(-1)));
        if(cc_eval_wide(e->type)) value->uinteger128 = bits;
        else value->uinteger = ci_uint128_lo(bits);
    }
    cleanup:
    cc_field_path_free(cc_allocator(p), view.path);
    return err;
}

static int cc_eval_expr_inner(CcEvalCtx*, CcExpr*, CcExpr*_Nullable*_Nonnull);

static
int
cc_eval_expr(CcEvalCtx* ctx, CcExpr* e, CcExpr*_Nullable*_Nonnull result){
    if(ctx->evaluation_depth >= 256) return CC_NOT_CONSTANT_ERROR;
    if(e->is_lvalue && (e->type.is_volatile || e->type.is_atomic))
        return CC_NOT_CONSTANT_ERROR;
    for(CcEvalVisit* visit = ctx->expressions; visit; visit = visit->previous)
        if(visit->expr == e) return CC_NOT_CONSTANT_ERROR;
    CcEvalVisit visit = {.previous = ctx->expressions, .expr = e};
    ctx->expressions = &visit;
    ctx->evaluation_depth++;
    int err = cc_eval_expr_inner(ctx, e, result);
    ctx->evaluation_depth--;
    ctx->expressions = visit.previous;
    return err;
}

static
int
cc_eval_initializer_value(CcEvalCtx* ctx, CcExpr* e, unsigned depth, CcExpr*_Nullable*_Nonnull out){
    CcParser* p = ctx->parser;
    if(depth >= 256) return cc_clone_expr(p, e, 0, (SrcLoc){0}, 0, out);
    int err = cc_eval_expr(ctx, e, out);
    if(err == CC_OOM_ERROR) return err;
    if(err) goto symbolic;
    CcExpr* folded = *out;
    if(!folded || cc_types_compatible_unqualified(e->type, folded->type))
        return 0;
    cc_release_expr(p, folded);
    *out = NULL;
    symbolic:
    if(!ccqt_has_pointer_representation(e->type))
        goto preserve;
    CcExpr* selected = NULL;
    if(e->kind == CC_EXPR_CAST && ccqt_has_pointer_representation(e->lhs->type)){
        err = cc_eval_initializer_value(ctx, e->lhs, depth+1, &selected);
        if(err) return err;
        if(selected->type.unqual == e->type.unqual){
            selected->type = e->type;
            *out = selected;
            return 0;
        }
        *out = cc_unary_expr(p, CC_EXPR_CAST, e->loc, e->type, selected);
        if(!*out){
            cc_release_expr(p, selected);
            return CC_OOM_ERROR;
        }
        return 0;
    }
    if(e->kind == CC_EXPR_DOT
    || e->kind == CC_EXPR_ARROW
    || e->kind == CC_EXPR_SUBSCRIPT
    || e->kind == CC_EXPR_DEREF){
        _Bool handled = 0;
        err = cc_eval_object_view_select(ctx, e, 1, &handled, &selected);
        if(err == CC_OOM_ERROR) return err;
        if(err) goto preserve;
        if(!handled) goto preserve;
        err = cc_eval_initializer_value(ctx, selected, depth+1, out);
        cc_release_expr(p, selected);
        return err;
    }
    preserve:
    return cc_clone_expr(p, e, 0, (SrcLoc){0}, 0, out);
}

static
int
cc_eval_expr_inner(CcEvalCtx* ctx, CcExpr* e, CcExpr*_Nullable*_Nonnull result){
    CcParser* p = ctx->parser;
    if(cc_eval_slice_expr(e)) return cc_eval_slice_value(ctx, e, result);
    if(cc_eval_pointer_binary_expr(e)){
        int64_t value;
        int err = cc_eval_pointer_binary(ctx, e, &value);
        if(err) return err;
        *result = cc_int64_expr(p, e->loc, e->type, value);
        return *result ? 0 : CC_OOM_ERROR;
    }
    switch(e->kind){
        DRP_CASES_EXHAUSTED;
        case CC_EXPR_BLOB:
        case CC_EXPR_VALUE: {
            CcExpr* node = _cc_alloc_expr(p, 0);
            if(!node) return CC_OOM_ERROR;
            memcpy(node, e, sizeof *e);
            *result = node;
            return 0;
        }
        case CC_EXPR_NEG: {
            CcExpr* operand;
            int err = cc_eval_expr(ctx, e->lhs, &operand);
            if(err) return err;
            if(!ccqt_is_basic(e->type)){
                err = CC_UNREACHABLE_ERROR;
                goto fini_neg;
            }
            if(e->type.unqual != operand->type.unqual){
                err = CC_UNREACHABLE_ERROR;
                goto fini_neg;
            }
            if(ccqt_is_narrow_float(operand->type)){
                CcExpr* narrow = cc_value_expr(p, e->loc, e->type);
                if(!narrow){
                    err = CC_OOM_ERROR;
                    goto fini_neg;
                }
                narrow->uinteger = operand->uinteger ^ 0x8000;
                *result = narrow;
                goto fini_neg;
            }
            if(cc_eval_wide(operand->type)){
                CcExpr* wide = cc_value_expr(p, e->loc, e->type);
                if(!wide){
                    err = CC_OOM_ERROR;
                    goto fini_neg;
                }
                if(ccqt_is_integer(operand->type)){
                    CiUint128 u = operand->uinteger128;
                    if(ccqt_bt_eq(operand->type, CCBT_int128) && ci_uint128_eq(u, ci_uint128_shl(ci_uint128_from_uint64(1),127))) err = CC_OVERFLOW_ERROR;
                    else wide->uinteger128 = ci_uint128_sub(ci_uint128_from_uint64(0), u);
                }
                else cc_eval_store_quad(cc_target(p), wide, ci_float128_neg(cc_eval_quad(cc_target(p), operand)));
                if(err) cc_release_expr(p, wide);
                else *result = wide;
                goto fini_neg;
            }
            CcExpr* node;
            switch(ccqt_basic_kind(e->type)){
                DRP_CASES_EXHAUSTED;
                case CCBT__Any:
                    return CC_UNIMPLEMENTED_ERROR;
                case CCBT_INVALID:
                case CCBT_void:
                case CCBT_nullptr_t:
                case CCBT__Type:
                case CCBT_COUNT:
                    err = CC_UNREACHABLE_ERROR;
                    break;
                case CCBT_bool:
                case CCBT_char:
                case CCBT_signed_char:
                case CCBT_unsigned_char:
                case CCBT_short:
                case CCBT_unsigned_short:
                    // Smaller than int types should've been promoted already
                    err = CC_UNREACHABLE_ERROR;
                    break;
                // For our supported targets, the only things that don't
                // correspond to fixed-sized types is long on win64.
                case CCBT_int:
                    neg_int:;{
                    int32_t val;
                    if(sub_overflow((int32_t)0, (int32_t)operand->integer, &val)){
                        err = CC_OVERFLOW_ERROR;
                        break;
                    }
                    node = cc_value_expr(p, e->loc, e->type);
                    if(!node) {err = CC_OOM_ERROR; break;}
                    node->integer = val;
                    *result = node;
                    break;
                }
                case CCBT_long:
                    if(cc_target(p)->sizeof_[CCBT_long] == 8)
                        goto neg_long_long;
                    goto neg_int;
                case CCBT_long_long:
                    neg_long_long:;{
                    int64_t val;
                    if(sub_overflow((int64_t)0, operand->integer, &val)){
                        err = CC_OVERFLOW_ERROR;
                        break;
                    }
                    node = cc_value_expr(p, e->loc, e->type);
                    if(!node) {err = CC_OOM_ERROR; break;}
                    node->integer = val;
                    *result = node;
                    break;
                }
                case CCBT_unsigned:
                case CCBT_unsigned_long:
                case CCBT_unsigned_long_long:{
                    node = cc_value_expr(p, e->loc, e->type);
                    if(!node) {err = CC_OOM_ERROR; break;}
                    node->uinteger = -operand->uinteger;
                    cc_eval_truncate(p, node);
                    *result = node;
                    break;
                }
                case CCBT_float:{
                    node = cc_value_expr(p, e->loc, e->type);
                    if(!node) {err = CC_OOM_ERROR; break;}
                    node->float_ = -operand->float_;
                    *result = node;
                    break;
                }
                case CCBT_double:{
                    node = cc_value_expr(p, e->loc, e->type);
                    if(!node) {err = CC_OOM_ERROR; break;}
                    node->double_ = -operand->double_;
                    *result = node;
                    break;
                }
                case CCBT_int128:
                case CCBT_unsigned_int128:
                case CCBT_bfloat16:
                case CCBT_fp16:
                case CCBT_float16:
                case CCBT_long_double:
                case CCBT_float128:
                case CCBT_float_complex:
                case CCBT_double_complex:
                case CCBT_long_double_complex:
                case CCBT_float16_complex:
                case CCBT_char_complex:
                case CCBT_signed_char_complex:
                case CCBT_unsigned_char_complex:
                case CCBT_short_complex:
                case CCBT_unsigned_short_complex:
                case CCBT_int_complex:
                case CCBT_unsigned_complex:
                case CCBT_long_complex:
                case CCBT_unsigned_long_complex:
                case CCBT_long_long_complex:
                case CCBT_unsigned_long_long_complex:
                case CCBT_int128_complex:
                case CCBT_unsigned_int128_complex:
                    err = CC_UNIMPLEMENTED_ERROR;
                    break;
            }
            fini_neg:;
            cc_release_expr(p, operand);
            return err;
        }
        case CC_EXPR_POS: return cc_eval_expr(ctx, e->lhs, result);
        case CC_EXPR_BITNOT: {
            CcExpr* operand;
            int err = cc_eval_expr(ctx, e->lhs, &operand);
            if(err) return err;
            if(!ccqt_is_basic(e->type)){
                err = CC_UNREACHABLE_ERROR;
                goto fini_bitnot;
            }
            if(e->type.unqual != operand->type.unqual){
                err = CC_UNREACHABLE_ERROR;
                goto fini_bitnot;
            }
            if(cc_eval_wide(operand->type)){
                CcExpr* wide = cc_value_expr(p, e->loc, e->type);
                if(!wide){
                    err = CC_OOM_ERROR;
                    goto fini_bitnot;
                }
                wide->uinteger128 = ci_uint128_xor(operand->uinteger128, ci_uint128_sub(ci_uint128_from_uint64(0), ci_uint128_from_uint64(1)));
                if(err) cc_release_expr(p, wide);
                else *result = wide;
                goto fini_bitnot;
            }
            CcExpr* node;
            switch(ccqt_basic_kind(e->type)){
                DRP_CASES_EXHAUSTED;
                case CCBT__Any:
                    return CC_UNIMPLEMENTED_ERROR;
                case CCBT_INVALID:
                case CCBT_void:
                case CCBT_nullptr_t:
                case CCBT__Type:
                case CCBT_COUNT:
                    err = CC_UNREACHABLE_ERROR;
                    goto fini_bitnot;
                case CCBT_bool:
                case CCBT_char:
                case CCBT_signed_char:
                case CCBT_unsigned_char:
                case CCBT_short:
                case CCBT_unsigned_short:
                    // Smaller than int types should've been promoted already
                    err = CC_UNREACHABLE_ERROR;
                    goto fini_bitnot;
                // For our supported targets, the only things that don't
                // correspond to fixed-sized types is long on win64.
                case CCBT_int:
                case CCBT_long:
                case CCBT_long_long:
                case CCBT_unsigned:
                case CCBT_unsigned_long:
                case CCBT_unsigned_long_long:
                    node = cc_value_expr(p, e->loc, e->type);
                    if(!node) {err = CC_OOM_ERROR; goto fini_bitnot;}
                    node->uinteger = ~operand->uinteger;
                    cc_eval_truncate(p, node);
                    *result = node;
                    err = 0;
                    goto fini_bitnot;
                case CCBT_float:
                case CCBT_double:
                    err = CC_UNREACHABLE_ERROR;
                    break;
                case CCBT_int128:
                case CCBT_unsigned_int128:
                case CCBT_bfloat16:
                case CCBT_fp16:
                case CCBT_float16:
                case CCBT_long_double:
                case CCBT_float128:
                case CCBT_float_complex:
                case CCBT_double_complex:
                case CCBT_long_double_complex:
                case CCBT_float16_complex:
                case CCBT_char_complex:
                case CCBT_signed_char_complex:
                case CCBT_unsigned_char_complex:
                case CCBT_short_complex:
                case CCBT_unsigned_short_complex:
                case CCBT_int_complex:
                case CCBT_unsigned_complex:
                case CCBT_long_complex:
                case CCBT_unsigned_long_complex:
                case CCBT_long_long_complex:
                case CCBT_unsigned_long_long_complex:
                case CCBT_int128_complex:
                case CCBT_unsigned_int128_complex:
                    err = CC_UNIMPLEMENTED_ERROR;
                    break;
            }
            fini_bitnot:;
            cc_release_expr(p, operand);
            return err;
        }
        case CC_EXPR_LOGNOT: {
            _Bool truth;
            int err = cc_eval_truthy(ctx, e->lhs, &truth);
            if(err) return err;
            *result = cc_int64_expr(p, e->loc, e->type, !truth);
            return *result ? 0 : CC_OOM_ERROR;
        }
        case CC_EXPR_COMMA: {
            CcExpr* source;
            int err = cc_eval_comma_source(ctx, e, 0, &source);
            if(err) return err;
            return cc_eval_expr(ctx, source, result);
        }
        case CC_EXPR_TERNARY: {
            _Bool truthy;
            int err = cc_eval_truthy(ctx, e->lhs, &truthy);
            if(err) return err;
            return cc_eval_expr(ctx, e->values[truthy ? 0 : 1], result);
        }
        case CC_EXPR_LOGAND: {
            _Bool truthy;
            int err = cc_eval_truthy(ctx, e->lhs, &truthy);
            if(err) return err;
            if(!truthy){
                *result = cc_int64_expr(p, e->loc, e->type, 0);
                return *result ? 0 : CC_OOM_ERROR;
            }
            err = cc_eval_truthy(ctx, e->values[0], &truthy);
            if(err) return err;
            *result = cc_int64_expr(p, e->loc, e->type, truthy ? 1 : 0);
            return *result ? 0 : CC_OOM_ERROR;
        }
        case CC_EXPR_LOGOR: {
            _Bool truthy;
            int err = cc_eval_truthy(ctx, e->lhs, &truthy);
            if(err) return err;
            if(truthy){
                *result = cc_int64_expr(p, e->loc, e->type, 1);
                return *result ? 0 : CC_OOM_ERROR;
            }
            err = cc_eval_truthy(ctx, e->values[0], &truthy);
            if(err) return err;
            *result = cc_int64_expr(p, e->loc, e->type, truthy ? 1 : 0);
            return *result ? 0 : CC_OOM_ERROR;
        }
        // Binary arithmetic and comparisons
        case CC_EXPR_ADD: case CC_EXPR_SUB: case CC_EXPR_MUL:
        case CC_EXPR_DIV: case CC_EXPR_MOD:
        case CC_EXPR_BITAND: case CC_EXPR_BITOR: case CC_EXPR_BITXOR:
        case CC_EXPR_LSHIFT: case CC_EXPR_RSHIFT:
        case CC_EXPR_EQ: case CC_EXPR_NE:
        case CC_EXPR_LT: case CC_EXPR_GT: case CC_EXPR_LE: case CC_EXPR_GE:
        {
            CcExpr* node = NULL;
            CcExpr *L, *R;
            int err = cc_eval_expr(ctx, e->lhs, &L);
            if(err) return err;
            err = cc_eval_expr(ctx, e->values[0], &R);
            if(err) { cc_release_expr(p, L); return err; }
            if(e->kind == CC_EXPR_MUL && e->binary.checked_size
                && !cc_eval_size_product_valid(cc_eval_u128(cc_target(p), L), cc_eval_u128(cc_target(p), R),
                    cc_target(p)->sizeof_[cc_target(p)->size_type]*8, e->binary.element_alignment)){
                cc_release_expr(p, L);
                cc_release_expr(p, R);
                return CC_NOT_CONSTANT_ERROR;
            }
            // Type equality/inequality
            if((e->kind == CC_EXPR_EQ || e->kind == CC_EXPR_NE)
            && (ccqt_kind(L->type) == CC_POINTER || ccqt_bt_eq(L->type, CCBT_nullptr_t))
            && (ccqt_kind(R->type) == CC_POINTER || ccqt_bt_eq(R->type, CCBT_nullptr_t))
            && !L->uinteger && !R->uinteger){
                node = cc_int64_expr(p, e->loc, e->type, e->kind == CC_EXPR_EQ);
                if(!node) err = CC_OOM_ERROR;
                goto fini_binary;
            }
            if(ccqt_bt_eq(L->type, CCBT__Type) && ccqt_bt_eq(R->type, CCBT__Type)){
                if(e->kind == CC_EXPR_EQ){
                    node = cc_int64_expr(p, e->loc, e->type, L->type_value.bits == R->type_value.bits);
                    if(!node) err = CC_OOM_ERROR;
                    goto fini_binary;
                }
                if(e->kind == CC_EXPR_NE){
                    node = cc_int64_expr(p, e->loc, e->type, L->type_value.bits != R->type_value.bits);
                    if(!node) err = CC_OOM_ERROR;
                    goto fini_binary;
                }
                err = CC_UNREACHABLE_ERROR;
                goto fini_binary;
            }
            // L and R have the same type after usual arithmetic conversions.
            // For arithmetic ops, result type == operand type.
            // For comparisons, result type is int, operand type is L->type.
            CcQualType optype = L->type;
            optype = ccqt_enum_underlying(optype);
            if(!ccqt_is_basic(optype)){
                err = CC_UNREACHABLE_ERROR;
                goto fini_binary;
            }
            if(e->kind == CC_EXPR_LSHIFT || e->kind == CC_EXPR_RSHIFT){
                err = cc_eval_check_shift_count(p, optype, R);
                if(err) goto fini_binary;
            }
            node = cc_value_expr(p, e->loc, e->type);
            if(!node) {err = CC_OOM_ERROR; goto fini_binary;}
            if(cc_eval_wide(optype) || ccqt_is_narrow_float(optype)){
                err = cc_eval_wide_binary(cc_target(p), e->kind, L, R, node);
                goto fini_binary;
            }
            #define ARITH(op, lv, rv, field, type) node->field = (type)((type)(lv) op (type)(rv)); break
            #define SIGNED_ARITH(op, chk, lv, rv, field, type) do { \
                type _r; if(chk((type)(lv), (type)(rv), &_r)){err = CC_OVERFLOW_ERROR; goto fini_binary;} \
                node->field = _r; \
            } while(0); break
            #define CMP(op, lv, rv) node->integer = (lv) op (rv); break
            switch(ccqt_basic_kind(optype)){
                DRP_CASES_EXHAUSTED;
                case CCBT_float:
                    switch((uint32_t)e->kind){
                        case CC_EXPR_ADD: ARITH(+, L->float_, R->float_, float_, float);
                        case CC_EXPR_SUB: ARITH(-, L->float_, R->float_, float_, float);
                        case CC_EXPR_MUL: ARITH(*, L->float_, R->float_, float_, float);
                        case CC_EXPR_DIV: ARITH(/, L->float_, R->float_, float_, float);
                        case CC_EXPR_EQ: CMP(==, L->float_, R->float_);
                        case CC_EXPR_NE: CMP(!=, L->float_, R->float_);
                        case CC_EXPR_LT: CMP(<,  L->float_, R->float_);
                        case CC_EXPR_GT: CMP(>,  L->float_, R->float_);
                        case CC_EXPR_LE: CMP(<=, L->float_, R->float_);
                        case CC_EXPR_GE: CMP(>=, L->float_, R->float_);
                        DRP_DEFAULT_UNREACHABLE;
                    }
                    break;
                case CCBT_double:
                    switch((uint32_t)e->kind){
                        case CC_EXPR_ADD: ARITH(+, L->double_, R->double_, double_, double);
                        case CC_EXPR_SUB: ARITH(-, L->double_, R->double_, double_, double);
                        case CC_EXPR_MUL: ARITH(*, L->double_, R->double_, double_, double);
                        case CC_EXPR_DIV: ARITH(/, L->double_, R->double_, double_, double);
                        case CC_EXPR_EQ: CMP(==, L->double_, R->double_);
                        case CC_EXPR_NE: CMP(!=, L->double_, R->double_);
                        case CC_EXPR_LT: CMP(<,  L->double_, R->double_);
                        case CC_EXPR_GT: CMP(>,  L->double_, R->double_);
                        case CC_EXPR_LE: CMP(<=, L->double_, R->double_);
                        case CC_EXPR_GE: CMP(>=, L->double_, R->double_);
                        DRP_DEFAULT_UNREACHABLE;
                    }
                    break;
                case CCBT_char:
                    if(!cc_target(p)->char_is_signed)
                        goto unsigned_;
                    goto int_;
                case CCBT_bool:
                case CCBT_signed_char:
                case CCBT_short:
                case CCBT_int:
                    int_:;{
                    int32_t lv = (int32_t)L->integer, rv = (int32_t)R->integer;
                    switch((uint32_t)e->kind){
                        case CC_EXPR_ADD: SIGNED_ARITH(+, add_overflow, lv, rv, integer, int32_t);
                        case CC_EXPR_SUB: SIGNED_ARITH(-, sub_overflow, lv, rv, integer, int32_t);
                        case CC_EXPR_MUL: SIGNED_ARITH(*, mul_overflow, lv, rv, integer, int32_t);
                        case CC_EXPR_DIV:
                            if(rv == 0 || (lv == INT32_MIN && rv == -1)){err = CC_OVERFLOW_ERROR; goto fini_binary;}
                            ARITH(/, lv, rv, integer, int32_t);
                        case CC_EXPR_MOD:
                            if(rv == 0 || (lv == INT32_MIN && rv == -1)){err = CC_OVERFLOW_ERROR; goto fini_binary;}
                            ARITH(%, lv, rv, integer, int32_t);
                        case CC_EXPR_BITAND: ARITH(&,  lv, rv, integer, int32_t);
                        case CC_EXPR_BITOR:  ARITH(|,  lv, rv, integer, int32_t);
                        case CC_EXPR_BITXOR: ARITH(^,  lv, rv, integer, int32_t);
                        case CC_EXPR_LSHIFT:
                            if(rv < 0 || rv >= 32){err = CC_OVERFLOW_ERROR; goto fini_binary;}
                            node->integer = (int32_t)((uint32_t)lv << rv); break;
                        case CC_EXPR_RSHIFT:
                            if(rv < 0 || rv >= 32){err = CC_OVERFLOW_ERROR; goto fini_binary;}
                            ARITH(>>, lv, rv, integer, int32_t);
                        case CC_EXPR_EQ: CMP(==, lv, rv);
                        case CC_EXPR_NE: CMP(!=, lv, rv);
                        case CC_EXPR_LT: CMP(<,  lv, rv);
                        case CC_EXPR_GT: CMP(>,  lv, rv);
                        case CC_EXPR_LE: CMP(<=, lv, rv);
                        case CC_EXPR_GE: CMP(>=, lv, rv);
                        DRP_DEFAULT_UNREACHABLE;
                    }
                    break;}
                case CCBT_unsigned_char:
                case CCBT_unsigned_short:
                case CCBT_unsigned:
                    unsigned_:;{
                    uint32_t lv = (uint32_t)L->uinteger, rv = (uint32_t)R->uinteger;
                    switch((uint32_t)e->kind){
                        case CC_EXPR_ADD: ARITH(+, lv, rv, uinteger, uint32_t);
                        case CC_EXPR_SUB: ARITH(-, lv, rv, uinteger, uint32_t);
                        case CC_EXPR_MUL: ARITH(*, lv, rv, uinteger, uint32_t);
                        case CC_EXPR_DIV:
                            if(rv == 0){err = CC_OVERFLOW_ERROR; goto fini_binary;}
                            ARITH(/, lv, rv, uinteger, uint32_t);
                        case CC_EXPR_MOD:
                            if(rv == 0){err = CC_OVERFLOW_ERROR; goto fini_binary;}
                            ARITH(%, lv, rv, uinteger, uint32_t);
                        case CC_EXPR_BITAND: ARITH(&,  lv, rv, uinteger, uint32_t);
                        case CC_EXPR_BITOR:  ARITH(|,  lv, rv, uinteger, uint32_t);
                        case CC_EXPR_BITXOR: ARITH(^,  lv, rv, uinteger, uint32_t);
                        case CC_EXPR_LSHIFT:
                            if(rv >= 32){err = CC_OVERFLOW_ERROR; goto fini_binary;}
                            ARITH(<<, lv, rv, uinteger, uint32_t);
                        case CC_EXPR_RSHIFT:
                            if(rv >= 32){err = CC_OVERFLOW_ERROR; goto fini_binary;}
                            ARITH(>>, lv, rv, uinteger, uint32_t);
                        case CC_EXPR_EQ: CMP(==, lv, rv);
                        case CC_EXPR_NE: CMP(!=, lv, rv);
                        case CC_EXPR_LT: CMP(<,  lv, rv);
                        case CC_EXPR_GT: CMP(>,  lv, rv);
                        case CC_EXPR_LE: CMP(<=, lv, rv);
                        case CC_EXPR_GE: CMP(>=, lv, rv);
                        DRP_DEFAULT_UNREACHABLE;
                    }
                    break;}
                case CCBT_long:
                    if(cc_target(p)->sizeof_[CCBT_long] == 8)
                        goto long_long;
                    goto int_;
                case CCBT_unsigned_long:
                    if(cc_target(p)->sizeof_[CCBT_unsigned_long] == 8)
                        goto unsigned_long_long;
                    goto unsigned_;
                case CCBT_long_long:
                    long_long:;{
                    int64_t lv = L->integer, rv = R->integer;
                    switch((uint32_t)e->kind){
                        case CC_EXPR_ADD: SIGNED_ARITH(+, add_overflow, lv, rv, integer, int64_t);
                        case CC_EXPR_SUB: SIGNED_ARITH(-, sub_overflow, lv, rv, integer, int64_t);
                        case CC_EXPR_MUL: SIGNED_ARITH(*, mul_overflow, lv, rv, integer, int64_t);
                        case CC_EXPR_DIV:
                            if(rv == 0 || (lv == INT64_MIN && rv == -1)){err = CC_OVERFLOW_ERROR; goto fini_binary;}
                            ARITH(/, lv, rv, integer, int64_t);
                        case CC_EXPR_MOD:
                            if(rv == 0 || (lv == INT64_MIN && rv == -1)){err = CC_OVERFLOW_ERROR; goto fini_binary;}
                            ARITH(%, lv, rv, integer, int64_t);
                        case CC_EXPR_BITAND: ARITH(&,  lv, rv, integer, int64_t);
                        case CC_EXPR_BITOR:  ARITH(|,  lv, rv, integer, int64_t);
                        case CC_EXPR_BITXOR: ARITH(^,  lv, rv, integer, int64_t);
                        case CC_EXPR_LSHIFT:
                            if(rv < 0 || rv >= 64){err = CC_OVERFLOW_ERROR; goto fini_binary;}
                            node->integer = (int64_t)((uint64_t)lv << rv); break;
                        case CC_EXPR_RSHIFT:
                            if(rv < 0 || rv >= 64){err = CC_OVERFLOW_ERROR; goto fini_binary;}
                            ARITH(>>, lv, rv, integer, int64_t);
                        case CC_EXPR_EQ: CMP(==, lv, rv);
                        case CC_EXPR_NE: CMP(!=, lv, rv);
                        case CC_EXPR_LT: CMP(<,  lv, rv);
                        case CC_EXPR_GT: CMP(>,  lv, rv);
                        case CC_EXPR_LE: CMP(<=, lv, rv);
                        case CC_EXPR_GE: CMP(>=, lv, rv);
                        DRP_DEFAULT_UNREACHABLE;
                    }
                    break;}
                case CCBT_unsigned_long_long:
                    unsigned_long_long:;{
                    uint64_t lv = L->uinteger, rv = R->uinteger;
                    switch((uint32_t)e->kind){
                        case CC_EXPR_ADD: ARITH(+, lv, rv, uinteger, uint64_t);
                        case CC_EXPR_SUB: ARITH(-, lv, rv, uinteger, uint64_t);
                        case CC_EXPR_MUL: ARITH(*, lv, rv, uinteger, uint64_t);
                        case CC_EXPR_DIV:
                            if(rv == 0){err = CC_OVERFLOW_ERROR; goto fini_binary;}
                            ARITH(/, lv, rv, uinteger, uint64_t);
                        case CC_EXPR_MOD:
                            if(rv == 0){err = CC_OVERFLOW_ERROR; goto fini_binary;}
                            ARITH(%, lv, rv, uinteger, uint64_t);
                        case CC_EXPR_BITAND: ARITH(&,  lv, rv, uinteger, uint64_t);
                        case CC_EXPR_BITOR:  ARITH(|,  lv, rv, uinteger, uint64_t);
                        case CC_EXPR_BITXOR: ARITH(^,  lv, rv, uinteger, uint64_t);
                        case CC_EXPR_LSHIFT:
                            if(rv >= 64){err = CC_OVERFLOW_ERROR; goto fini_binary;}
                            ARITH(<<, lv, rv, uinteger, uint64_t);
                        case CC_EXPR_RSHIFT:
                            if(rv >= 64){err = CC_OVERFLOW_ERROR; goto fini_binary;}
                            ARITH(>>, lv, rv, uinteger, uint64_t);
                        case CC_EXPR_EQ: CMP(==, lv, rv);
                        case CC_EXPR_NE: CMP(!=, lv, rv);
                        case CC_EXPR_LT: CMP(<,  lv, rv);
                        case CC_EXPR_GT: CMP(>,  lv, rv);
                        case CC_EXPR_LE: CMP(<=, lv, rv);
                        case CC_EXPR_GE: CMP(>=, lv, rv);
                        DRP_DEFAULT_UNREACHABLE;
                    }
                    break;}
                case CCBT_INVALID:
                case CCBT_void:
                case CCBT_nullptr_t:
                case CCBT__Any:
                case CCBT__Type:
                case CCBT_COUNT:
                    err = CC_UNREACHABLE_ERROR;
                    goto fini_binary;
                case CCBT_int128:
                case CCBT_unsigned_int128:
                case CCBT_bfloat16:
                case CCBT_fp16:
                case CCBT_float16:
                case CCBT_long_double:
                case CCBT_float128:
                case CCBT_float_complex:
                case CCBT_double_complex:
                case CCBT_long_double_complex:
                case CCBT_float16_complex:
                case CCBT_char_complex:
                case CCBT_signed_char_complex:
                case CCBT_unsigned_char_complex:
                case CCBT_short_complex:
                case CCBT_unsigned_short_complex:
                case CCBT_int_complex:
                case CCBT_unsigned_complex:
                case CCBT_long_complex:
                case CCBT_unsigned_long_complex:
                case CCBT_long_long_complex:
                case CCBT_unsigned_long_long_complex:
                case CCBT_int128_complex:
                case CCBT_unsigned_int128_complex:
                    err = CC_UNIMPLEMENTED_ERROR;
                    goto fini_binary;
            }
            #undef ARITH
            #undef SIGNED_ARITH
            #undef CMP
            fini_binary:;
            if(err){
                if(node) cc_release_expr(p, node);
            }
            else
                *result = node;
            cc_release_expr(p, R);
            cc_release_expr(p, L);
            return err;
        }
        case CC_EXPR_CAST: {
            CcQualType target = e->type;
            target = ccqt_enum_underlying(target);
            if(ccqt_bt_eq(target, CCBT_bool)){
                _Bool truth;
                int err = cc_eval_truthy(ctx, e->lhs, &truth);
                if(err) return err;
                *result = cc_int64_expr(p, e->loc, e->type, truth);
                return *result ? 0 : CC_OOM_ERROR;
            }
            CcExpr* operand, *node = NULL;
            int err = cc_eval_expr(ctx, e->lhs, &operand);
            if(err) return err;
            if((e->cast.checked_index || e->cast.checked_bound) && !ci_uint128_fits_integer(cc_eval_u128(cc_target(p), operand),
                cc_target(p)->sizeof_[cc_target(p)->size_type]*8,
                ccqt_is_unsigned(e->type, !cc_target(p)->char_is_signed))){
                cc_release_expr(p, operand);
                return CC_NOT_CONSTANT_ERROR;
            }
            if(ccqt_unaligned(operand->type).unqual == ccqt_unaligned(e->type).unqual){
                operand->type = e->type;
                *result = operand;
                return 0;
            }
            if(!ccqt_is_basic(target) || ccqt_has_pointer_representation(target)){
                if(ccqt_has_pointer_representation(e->type) && operand->kind == CC_EXPR_VALUE
                    && (ccqt_is_integer(operand->type) || ccqt_has_pointer_representation(operand->type))){
                    node = cc_value_expr(p, e->loc, e->type);
                    if(!node){
                        cc_release_expr(p, operand);
                        return CC_OOM_ERROR;
                    }
                    node->uinteger = ccqt_is_integer(operand->type)
                        ? ci_uint128_lo(cc_eval_u128(cc_target(p), operand)) : operand->uinteger;
                    uint32_t size = cc_target(p)->sizeof_[CCBT_nullptr_t];
                    if(size < 8) node->uinteger &= UINT64_MAX >> (64 - size * 8);
                    cc_release_expr(p, operand);
                    *result = node;
                    return 0;
                }
                *result = operand;
                return 0;
            }
            CcBasicTypeKind tk = ccqt_basic_kind(target);
            if(tk == CCBT__Any){
                CcInitList* il = Allocator_zalloc_extra(cc_allocator(p), 2, sizeof *il->entries, sizeof *il, _Alignof(CcInitList));
                if(!il){
                    cc_release_expr(p, operand);
                    return CC_OOM_ERROR;
                }
                CcExpr* tag = cc_value_expr(p, e->loc, ccqt_basic(CCBT__Type));
                node = cc_make_expr(p, CC_EXPR_INIT_LIST, e->loc, e->type, 0);
                if(!tag || !node){
                    if(tag) cc_release_expr(p, tag);
                    if(node) _cc_release_expr(p, node, 0);
                    Allocator_free_extra(cc_allocator(p), il, 2, sizeof *il->entries, sizeof *il, _Alignof(CcInitList));
                    cc_release_expr(p, operand);
                    return CC_OOM_ERROR;
                }
                tag->type_value = (CcQualType){.unqual = e->lhs->type.unqual};
                il->loc = e->loc;
                il->count = 2;
                il->entries[0].value = tag;
                il->entries[0].path = (CcFieldPath){.n_components=1, .idx0=0};
                il->entries[1].value = operand;
                il->entries[1].path = (CcFieldPath){.n_components=1, .idx0=1};
                node->init_list = il;
                *result = node;
                return 0;
            }
            if(tk == CCBT_void){
                node = cc_value_expr(p, e->loc, ccqt_basic(CCBT_void));
                if(!node) err = CC_OOM_ERROR;
                goto fini_cast;
            }
            if(ccbt_is_complex(tk)){
                err = CC_NOT_CONSTANT_ERROR;
                goto fini_cast;
            }
            node = cc_value_expr(p, e->loc, e->type);
            if(!node) {err = CC_OOM_ERROR; goto fini_cast;}
            if(cc_eval_wide(operand->type)
            || cc_eval_wide(e->type)
            || ccqt_is_narrow_float(operand->type)
            || ccqt_is_narrow_float(e->type)
            || (ctx->allow_inexact_float_casts && ccbt_is_float(tk))
            || (ccqt_is_integer(target) && ccqt_is_basic(operand->type) && ccbt_is_float(ccqt_basic_kind(operand->type)))){
                err = cc_eval_numeric_cast(p, operand, node, ctx->allow_inexact_float_casts);
                if(!err) cc_eval_truncate(p, node);
                goto fini_cast;
            }
            if(ccbt_is_float(tk)){
                if(tk == CCBT_float){
                    err = cc_eval_to_f(p, operand, &node->float_);
                    if(err) goto fini_cast;
                }
                else {
                    err = cc_eval_to_d(p, operand, &node->double_);
                    if(err) goto fini_cast;
                }
            }
            else if(ccbt_is_integer(tk)){
                if(ccbt_is_unsigned(tk, !cc_target(p)->char_is_signed)){
                    err = cc_eval_to_u(p, operand, &node->uinteger);
                    if(err) goto fini_cast;
                }
                else {
                    err = cc_eval_to_i(p, operand, &node->integer);
                    if(err) goto fini_cast;
                }
                cc_eval_truncate(p, node);
            }
            fini_cast:;
            cc_release_expr(p, operand);
            if(!err)
                *result = node;
            else {
                if(node) cc_release_expr(p, node);
            }
            return err;
        }
        case CC_EXPR_TYPE_INTROSPECTION: {
            CcExpr* lhs;
            int err = cc_eval_expr(ctx, e->lhs, &lhs);
            if(err) return err;
            if(!ccqt_bt_eq(lhs->type, CCBT__Type)){
                err = CC_NOT_CONSTANT_ERROR;
                goto fini_introspection;
            }
            CcQualType qt = lhs->type_value;
            CcTypeIntrospectionOp op = e->type_introspection.op;
            #define INTRES(val) do { \
                *result = cc_int64_expr(p, e->loc, e->type, (val)); \
                err = *result ? 0 : CC_OOM_ERROR; \
                goto fini_introspection; \
            } while(0)
            #define UINTRES(val) do { \
                *result = cc_uint64_expr(p, e->loc, e->type, (val)); \
                err = *result ? 0 : CC_OOM_ERROR; \
                goto fini_introspection; \
            } while(0)
            #define TYPERES(t) do { \
                CcExpr* _n = cc_value_expr(p, e->loc, ccqt_basic(CCBT__Type)); \
                if(!_n) { err = CC_OOM_ERROR; goto fini_introspection; } \
                _n->uinteger = (t).bits; \
                *result = _n; err = 0; \
                goto fini_introspection; \
            } while(0)
            switch(op){
                case CC_TYPE_IS_VALID: case CC_TYPE_IS_INVALID:
                case CC_TYPE_IS_INTEGER: case CC_TYPE_IS_FLOAT: case CC_TYPE_IS_ARITHMETIC:
                case CC_TYPE_IS_POINTER: case CC_TYPE_IS_STRUCT: case CC_TYPE_IS_UNION:
                case CC_TYPE_IS_ARRAY: case CC_TYPE_IS_VECTOR: case CC_TYPE_IS_SLICE:
                case CC_TYPE_IS_FUNCTION: case CC_TYPE_IS_ENUM:
                case CC_TYPE_IS_CONST: case CC_TYPE_IS_VOLATILE: case CC_TYPE_IS_ATOMIC:
                case CC_TYPE_IS_UNSIGNED: case CC_TYPE_IS_SIGNED:
                case CC_TYPE_IS_CALLABLE: case CC_TYPE_IS_INCOMPLETE: case CC_TYPE_IS_VARIADIC:
                    INTRES(cc_type_predicate(cc_target(p), op, qt));
                case CC_TYPE_SIZEOF: {
                    uint32_t sz;
                    err = cc_sizeof_as_uint(p, qt, e->loc, &sz);
                    if(err) goto fini_introspection;
                    UINTRES(sz);
                }
                case CC_TYPE_ALIGNOF: {
                    uint32_t al;
                    err = cc_alignof_as_uint(p, qt, e->loc, &al);
                    if(err) goto fini_introspection;
                    UINTRES(al);
                }
                case CC_TYPE_POINTEE:
                    if(ccqt_kind(qt) != CC_POINTER) { err = CC_NOT_CONSTANT_ERROR; goto fini_introspection; }
                    TYPERES(ccqt_as_ptr(qt)->pointee);
                case CC_TYPE_UNQUAL: {
                    CcQualType uq;
                    err = cc_unqual_object_type(p, qt, &uq);
                    if(err) goto fini_introspection;
                    TYPERES(uq);
                }
                case CC_TYPE_COUNT:
                    if(!ccqt_is_fixed_sequence(qt)) {
                        err = CC_NOT_CONSTANT_ERROR;
                        goto fini_introspection;
                    }
                    UINTRES(ccqt_sequence_length(qt));
                case CC_TYPE_LOC:{
                    switch(ccqt_kind(qt)){
                        case CC_VLA: UINTRES(ccqt_as_vla(qt)->loc.bits);
                        case CC_ALIGNED: cc_unsafe_unreachable();
                        DRP_CASES_EXHAUSTED;
                        case CC_ENUM: UINTRES(ccqt_as_enum(qt)->loc.bits);
                        case CC_UNION: UINTRES(ccqt_as_union(qt)->loc.bits);
                        case CC_STRUCT: UINTRES(ccqt_as_struct(qt)->loc.bits);
                        case CC_VECTOR:
                        case CC_BASIC:
                        case CC_BLOCK_POINTER:
                        case CC_POINTER:
                        case CC_ARRAY:
                        case CC_SLICE:
                        case CC_FUNCTION:
                            err = CC_NOT_CONSTANT_ERROR;
                            goto fini_introspection;
                    }
                }
                case CC_TYPE_IS_CALLABLE_WITH: {
                    CcExpr* list;
                    err = cc_eval_object_source(ctx, e->values[0], 0, &list);
                    if(err) goto fini_introspection;
                    if(list->kind == CC_EXPR_CAST && ccqt_is_array(list->lhs->type)){
                        err = cc_eval_object_source(ctx, list->lhs, 0, &list);
                        if(err) goto fini_introspection;
                    }
                    uint64_t count;
                    _Bool array = ccqt_kind(list->type) == CC_ARRAY;
                    if(array) count = ccqt_as_array(list->type)->length;
                    else {
                        CcEvalAddress address;
                        err = cc_eval_slice(ctx, list, 0, &count, &address);
                        if(err) goto fini_introspection;
                    }
                    if(count > UINT32_MAX) INTRES(0);
                    CcQualType *args = NULL;
                    if(count){
                        args = Allocator_alloc(cc_scratch_allocator(p), (size_t)count, sizeof *args, _Alignof(CcQualType));
                        if(!args){
                            err = CC_OOM_ERROR;
                            goto fini_introspection;
                        }
                    }
                    CcExpr index = {.kind=CC_EXPR_VALUE, .type=ccqt_basic(cc_target(p)->size_type)};
                    CcExpr* access = NULL;
                    if(!array && count){
                        access = cc_make_expr(p, CC_EXPR_SUBSCRIPT, e->loc, ccqt_basic(CCBT__Type), 1);
                        if(!access) err = CC_OOM_ERROR;
                        else { access->lhs = list; access->values[0] = &index; }
                    }
                    for(uint32_t i = 0; !err && i < count; i++){
                        index.uinteger = i;
                        err = cc_eval_object_type(ctx, array ? list : access,
                            array ? (uint64_t)i * cc_target(p)->sizeof_[CCBT__Type] : 0, 0, &args[i]);
                    }
                    // The access node borrows both operands.
                    if(access) _cc_release_expr(p, access, 1);
                    _Bool v = !err && cc_is_callable_with(cc_target(p), qt, args, (uint32_t)count);
                    if(args) Allocator_free(cc_scratch_allocator(p), args, (size_t)count, sizeof *args, _Alignof(CcQualType));
                    if(err) goto fini_introspection;
                    INTRES(v);
                }
                case CC_TYPE_IS_CALLABLE_THROUGH:
                case CC_TYPE_CASTABLE_TO: {
                    CcExpr* arg;
                    err = cc_eval_expr(ctx, e->values[0], &arg);
                    if(err) goto fini_introspection;
                    if(!ccqt_bt_eq(arg->type, CCBT__Type)) {
                        cc_release_expr(p, arg);
                        err = CC_NOT_CONSTANT_ERROR;
                        goto fini_introspection;
                    }
                    _Bool castable = e->type_introspection.op == CC_TYPE_IS_CALLABLE_THROUGH ? cc_is_callable_through(cc_target(p), qt, arg->type_value) : cc_explicit_castable(cc_target(p), qt, arg->type_value);
                    cc_release_expr(p, arg);
                    INTRES(castable);
                }
                case CC_TYPE_MAKE_ANY:{
                    return CC_NOT_CONSTANT_ERROR; // TODO: we could do this
                }
                case CC_TYPE_FIELDS:
                case CC_TYPE_METHODS: {
                    CcTypeKind k = ccqt_kind(qt);
                    if(k != CC_STRUCT && k != CC_UNION) { err = CC_NOT_CONSTANT_ERROR; goto fini_introspection; }
                    CcStruct* s = ccqt_as_struct(qt);
                    _Bool methods = e->type_introspection.op == CC_TYPE_METHODS;
                    UINTRES(methods ? s->methods.count : s->field_count);
                }
                case CC_TYPE_RETURN_TYPE: {
                    CcFunction* f = ccqt_callable_function(qt);
                    if(!f) { err = CC_NOT_CONSTANT_ERROR; goto fini_introspection; }
                    TYPERES(f->return_type);
                }
                case CC_TYPE_PARAM_COUNT: {
                    CcFunction* f = ccqt_callable_function(qt);
                    if(!f) { err = CC_NOT_CONSTANT_ERROR; goto fini_introspection; }
                    UINTRES(f->param_count);
                }
                case CC_TYPE_PARAM_TYPE: {
                    CcFunction* f = ccqt_callable_function(qt);
                    if(!f) { err = CC_NOT_CONSTANT_ERROR; goto fini_introspection; }
                    int64_t i;
                    err = cc_eval_integer(ctx, e->values[0], &i);
                    if(err) goto fini_introspection;
                    if(i < 0 || (uint64_t)i >= f->param_count) { err = CC_NOT_CONSTANT_ERROR; goto fini_introspection; }
                    TYPERES(f->params[i]);
                }
                case CC_TYPE_ELEMENT_TYPE:{
                    CcTypeKind k = ccqt_kind(qt);
                    CcQualType val;
                    if(k == CC_ARRAY || k == CC_VECTOR || k == CC_VLA)
                        val = ccqt_qualified_array_element(qt);
                    else if(k == CC_SLICE)
                        val = ccqt_as_slice(qt)->pointee;
                    else {
                        err = CC_NOT_CONSTANT_ERROR;
                        goto fini_introspection;
                    }
                    TYPERES(val);
                }
                case CC_TYPE_UNDERLYING_TYPE:
                    if(ccqt_kind(qt) != CC_ENUM) { err = CC_NOT_CONSTANT_ERROR; goto fini_introspection; }
                    TYPERES(ccqt_as_enum(qt)->underlying);
                case CC_TYPE_ENUMERATORS: {
                    if(ccqt_kind(qt) != CC_ENUM) { err = CC_NOT_CONSTANT_ERROR; goto fini_introspection; }
                    CcEnum* e2 = ccqt_as_enum(qt);
                    UINTRES(e2->enumerator_count);
                }
                case CC_TYPE_FIELD:
                case CC_TYPE_METHOD:
                case CC_TYPE_HAS_FIELD:
                case CC_TYPE_HAS_METHOD: {
                    _Bool has = e->type_introspection.op == CC_TYPE_HAS_FIELD || e->type_introspection.op == CC_TYPE_HAS_METHOD;
                    CcTypeKind k = ccqt_kind(qt);
                    CcStruct* s = k == CC_STRUCT || k == CC_UNION ? ccqt_as_struct(qt) : NULL;
                    if(!s && !has) { err = CC_NOT_CONSTANT_ERROR; goto fini_introspection; }
                    _Bool method = e->type_introspection.op == CC_TYPE_METHOD || e->type_introspection.op == CC_TYPE_HAS_METHOD;
                    CcField* f = NULL;
                    CcField named_field;
                    if(ccqt_kind(e->values[0]->type) == CC_SLICE){
                        uint64_t count;
                        CcEvalAddress data;
                        err = cc_eval_slice(ctx, e->values[0], 0, &count, &data);
                        if(err) goto fini_introspection;
                        if(count && (data.kind != CC_EVAL_LITERAL || data.offset < 0
                            || (uint64_t)data.offset > data.literal_size
                            || count > data.literal_size - (uint64_t)data.offset))
                            err = CC_NOT_CONSTANT_ERROR;
                        if(!err && count > 0 && s){
                            Atom atom = AT_atomize(p->cpp.at, (const char*)data.symbol + data.offset, count);
                            if(!atom) err = CC_OOM_ERROR;
                            else {
                                uint64_t floc;
                                CcQualType type;
                                err = cc_lookup_field_offset(p, qt, atom, &floc, &type, &f);
                                if(err) goto fini_introspection;
                                if(f && f->is_method != method) f = NULL;
                                if(f){
                                    named_field = *f;
                                    named_field.type = type;
                                    named_field.offset = (uint32_t)floc;
                                    f = &named_field;
                                }
                            }
                        }
                        if(err) goto fini_introspection;
                        if(has) INTRES(f != NULL);
                        if(!f) { err = CC_NOT_CONSTANT_ERROR; goto fini_introspection; }
                    }
                    else {
                        int64_t idx;
                        err = cc_eval_integer(ctx, e->values[0], &idx);
                        if(err) goto fini_introspection;
                        if(idx >= 0){
                            uint32_t count = method ? s->methods.count : s->field_count;
                            if((uint64_t)idx < count) f = &(method ? s->methods.data : s->fields)[idx];
                        }
                        if(!f) { err = CC_NOT_CONSTANT_ERROR; goto fini_introspection; }
                    }
                    uint32_t nfields = method ? 4 : 6;
                    CcInitList* il = Allocator_zalloc_extra(cc_allocator(p), nfields, sizeof *il->entries, sizeof *il, _Alignof(CcInitList));
                    if(!il) { err = CC_OOM_ERROR; goto fini_introspection; }
                    il->loc = e->loc;
                    il->count = nfields;
                    for(uint32_t i = 0; i < nfields; i++)
                        il->entries[i].path = (CcFieldPath){.n_components=1, .idx0=i};
                    CcInitEntry* entries = il->entries;
                    // _Type type;
                    CcExpr* type_val = cc_value_expr(p, e->loc, ccqt_basic(CCBT__Type));
                    if(!type_val) { err = CC_OOM_ERROR; goto fini_introspection; }
                    type_val->uinteger = f->type.bits;
                    entries->value = type_val;
                    entries++;
                    // const char name[:];
                    Atom name = f->is_method ? f->method->name : f->name;
                    CcExpr* name_val = cc_constexpr_string_slice_expr(p, e->loc, name ? name : nil_atom, 0);
                    if(!name_val) { err = CC_OOM_ERROR; goto fini_introspection; }
                    entries->value = name_val;
                    entries++;
                    if(method){
                        CcExpr* offset = cc_uint64_expr(p, e->loc, ccqt_basic(cc_target(p)->size_type), f->offset);
                        if(!offset) { err = CC_OOM_ERROR; goto fini_introspection; }
                        entries->value = offset;
                        entries++;
                        CcExpr* func = cc_make_expr(p, CC_EXPR_FUNCTION, e->loc, f->type, 0);
                        if(!func) { err = CC_OOM_ERROR; goto fini_introspection; }
                        func->func = f->method;
                        CcExpr* address = cc_unary_expr(p, CC_EXPR_CAST, e->loc, ccqt_basic(cc_target(p)->size_type), func);
                        if(!address){
                            cc_release_expr(p, func);
                            err = CC_OOM_ERROR;
                            goto fini_introspection;
                        }
                        entries->value = address;
                    }
                    else {
                        // unsigned offset;
                        CcExpr* off_val = cc_uint64_expr(p, e->loc, ccqt_basic(CCBT_unsigned), f->offset);
                        if(!off_val) { err = CC_OOM_ERROR; goto fini_introspection; }
                        entries->value = off_val;
                        entries++;
                        // unsigned bitwidth;
                        CcExpr* bw_val = cc_uint64_expr(p, e->loc, ccqt_basic(CCBT_unsigned), f->bitwidth);
                        if(!bw_val) { err = CC_OOM_ERROR; goto fini_introspection; }
                        entries->value = bw_val;
                        entries++;
                        // unsigned bitoffset;
                        CcExpr* bo_val = cc_uint64_expr(p, e->loc, ccqt_basic(CCBT_unsigned), f->bitoffset);
                        if(!bo_val) { err = CC_OOM_ERROR; goto fini_introspection; }
                        entries->value = bo_val;
                        entries++;
                        // Maybe this should be a _Bool?
                        // unsigned is_bitfield;
                        CcExpr* is_bf_val = cc_uint64_expr(p, e->loc, ccqt_basic(CCBT_unsigned), f->is_bitfield);
                        if(!is_bf_val) { err = CC_OOM_ERROR; goto fini_introspection; }
                        entries->value = is_bf_val;
                        entries++;
                    }
                    CcExpr* node = cc_make_expr(p, CC_EXPR_INIT_LIST, e->loc, method ? p->builtin_method : p->builtin_field, 0);
                    if(!node) { err = CC_OOM_ERROR; goto fini_introspection; }
                    node->init_list = il;
                    *result = node;
                    err = 0;
                    goto fini_introspection;
                }
                case CC_TYPE_ENUMERATOR:{
                    if(ccqt_kind(qt) != CC_ENUM) { err = CC_NOT_CONSTANT_ERROR; goto fini_introspection; }
                    CcEnum* enum_ = ccqt_as_enum(qt);
                    int64_t idx;
                    err = cc_eval_integer(ctx, e->values[0], &idx);
                    if(err) goto fini_introspection;
                    if(idx < 0 || (uint64_t)idx >= enum_->enumerator_count) { err = CC_NOT_CONSTANT_ERROR; goto fini_introspection; }
                    CcEnumerator* en = enum_->enumerators[idx];
                    uint32_t nfields = 3; // name, value bits, type
                    CcInitList* il = Allocator_zalloc_extra(cc_allocator(p), nfields, sizeof *il->entries, sizeof *il, _Alignof(CcInitList));
                    if(!il) { err = CC_OOM_ERROR; goto fini_introspection; }
                    il->loc = e->loc;
                    il->count = nfields;
                    CcInitEntry* entries = il->entries;
                    // const char name[:];
                    CcExpr* name_val = cc_constexpr_string_slice_expr(p, e->loc, en->name ? en->name : nil_atom, 0);
                    if(!name_val) { err = CC_OOM_ERROR; goto fini_introspection; }
                    entries->path = (CcFieldPath){.n_components=1, .idx0=0};
                    entries->value = name_val;
                    entries++;
                    // unsigned __int128 value;
                    CcExpr* val_node = cc_integer_bits_expr(p, e->loc, ccqt_basic(CCBT_unsigned_int128), en->value);
                    if(!val_node) { err = CC_OOM_ERROR; goto fini_introspection; }
                    entries->path = (CcFieldPath){.n_components=1, .idx0=1};
                    entries->value = val_node;
                    entries++;
                    CcExpr* type_node = cc_value_expr(p, e->loc, ccqt_basic(CCBT__Type));
                    if(!type_node) { err = CC_OOM_ERROR; goto fini_introspection; }
                    type_node->type_value = en->type;
                    entries->path = (CcFieldPath){.n_components=1, .idx0=2};
                    entries->value = type_node;
                    CcExpr* node = cc_make_expr(p, CC_EXPR_INIT_LIST, e->loc, p->builtin_enumerator, 0);
                    if(!node) { err = CC_OOM_ERROR; goto fini_introspection; }
                    node->init_list = il;
                    *result = node;
                    err = 0;
                    goto fini_introspection;
                }
                case CC_TYPE_NAME: {
                    MStringBuilder sb = {.allocator = allocator_from_arena(&p->scratch_arena)};
                    cc_print_type(&sb, qt);
                    Atom a = msb_atomize(&sb, p->cpp.at);
                    if(!a) { err = CC_OOM_ERROR; goto fini_introspection; }
                    msb_destroy(&sb);
                    CcExpr *node = cc_constexpr_string_slice_expr(p, e->loc, a, 0);
                    if(!node) { err = CC_OOM_ERROR; goto fini_introspection; }
                    *result = node;
                    err = 0;
                    goto fini_introspection;
                }
                case CC_TYPE_TAG: {
                    Atom tag = NULL;
                    CcTypeKind k = ccqt_kind(qt);
                    if(k == CC_STRUCT)     tag = ccqt_as_struct(qt)->name;
                    else if(k == CC_UNION) tag = ccqt_as_union(qt)->name;
                    else if(k == CC_ENUM)  tag = ccqt_as_enum(qt)->name;
                    if(!tag) tag = nil_atom;
                    CcExpr* node = cc_constexpr_string_slice_expr(p, e->loc, tag, 0);
                    if(!node) { err = CC_OOM_ERROR; goto fini_introspection; }
                    *result = node;
                    err = 0;
                    goto fini_introspection;
                }
                case CC_TYPE_PUSH_METHOD: // handled at parse time
                case CC_TYPE_NONE:
                    err = CC_NOT_CONSTANT_ERROR;
                    goto fini_introspection;
            }
            err = CC_NOT_CONSTANT_ERROR;
            #undef INTRES
            #undef UINTRES
            #undef TYPERES
            fini_introspection:;
            cc_release_expr(p, lhs);
            return err;
        }
        case CC_EXPR_ATOMIC:
        case CC_EXPR_SIZEOF_VMT:
            return CC_NOT_CONSTANT_ERROR;
        case CC_EXPR_VARIABLE:
            if(e->var->initializer && (e->var->constexpr_
                || (ctx->allow_const && cc_linktime_const_variable(e)))){
                if(ctx->variable_depth >= 256) return CC_NOT_CONSTANT_ERROR;
                ctx->variable_depth++;
                int err = cc_eval_expr(ctx, e->var->initializer, result);
                ctx->variable_depth--;
                return err;
            }
            return CC_NOT_CONSTANT_ERROR;
        case CC_EXPR_COMPOUND_LITERAL:
        case CC_EXPR_INIT_LIST: {
            if((ccqt_is_basic(e->type) && !ccqt_bt_eq(e->type, CCBT__Any))
                || ccqt_kind(e->type) == CC_ENUM || ccqt_has_pointer_representation(e->type))
                return cc_eval_object_scalar(ctx, e, 0, e->type, e->loc, NULL, result);
            CcExpr* node = cc_make_expr(p, CC_EXPR_INIT_LIST, e->loc, e->type, 0);
            if(!node) return CC_OOM_ERROR;
            CcInitList* source = e->init_list;
            CcInitList* list = Allocator_zalloc_extra(cc_allocator(p), source->count, sizeof *list->entries, sizeof *list, _Alignof(CcInitList));
            if(!list){
                _cc_release_expr(p, node, 0);
                return CC_OOM_ERROR;
            }
            node->init_list = list;
            list->loc = source->loc;
            list->count = source->count;
            for(uint32_t i = 0; i < source->count; i++){
                CcInitEntry* entry = &list->entries[i];
                CcExpr* value = source->entries[i].value;
                int err = cc_field_path_concat(cc_allocator(p), (CcFieldPath){0}, source->entries[i].path, &entry->path);
                if(err){
                    cc_release_expr(p, node);
                    return CC_OOM_ERROR;
                }
                if(!value) continue;
                err = cc_eval_initializer_value(ctx, value, 0, &entry->value);
                if(err){
                    cc_release_expr(p, node);
                    return err;
                }
            }
            *result = node;
            return 0;
        }
        case CC_EXPR_OBJECT_VIEW:
            return cc_clone_expr(p, e, 0, (SrcLoc){0}, 0, result);
        case CC_EXPR_FUNCTION:
        case CC_EXPR_ADDR:
        case CC_EXPR_PREINC:
        case CC_EXPR_PREDEC:
        case CC_EXPR_POSTINC:
        case CC_EXPR_POSTDEC:
        case CC_EXPR_ASSIGN:
        case CC_EXPR_ADDASSIGN:
        case CC_EXPR_SUBASSIGN:
        case CC_EXPR_MULASSIGN:
        case CC_EXPR_DIVASSIGN:
        case CC_EXPR_MODASSIGN:
        case CC_EXPR_BITANDASSIGN:
        case CC_EXPR_BITORASSIGN:
        case CC_EXPR_BITXORASSIGN:
        case CC_EXPR_LSHIFTASSIGN:
        case CC_EXPR_RSHIFTASSIGN:
            return CC_NOT_CONSTANT_ERROR;
        case CC_EXPR_SLICE:
        case CC_EXPR_SLICE_HI:
        case CC_EXPR_SLICE_LO:
        case CC_EXPR_SLICE_ALL:
            return cc_eval_slice_value(ctx, e, result);
        case CC_EXPR_CALL:
            return CC_NOT_CONSTANT_ERROR;
        case CC_EXPR_DOT: case CC_EXPR_ARROW: case CC_EXPR_DEREF:
        case CC_EXPR_SUBSCRIPT: {
            _Bool handled;
            int err = cc_eval_object_view_select(ctx, e, 0, &handled, result);
            return err ? err : handled ? 0 : CC_NOT_CONSTANT_ERROR;
        }
        case CC_EXPR_STATEMENT_EXPRESSION:
        case CC_EXPR_VA:
        case CC_EXPR_BUILTIN:
        case CC_EXPR_ADD_OVERFLOW:
        case CC_EXPR_MUL_OVERFLOW:
        case CC_EXPR_SUB_OVERFLOW:
            return CC_NOT_CONSTANT_ERROR;
        case CC_EXPR_BIT_BUILTIN: {
            CcExpr* operand;
            int err = cc_eval_expr(ctx, e->lhs, &operand);
            if(err) return err;
            CiUint128 v = cc_eval_u128(cc_target(p), operand), arg = ci_uint128_from_uint64(0), bits;
            cc_release_expr(p, operand);
            if(e->bit_builtin.nargs && ((e->bit_builtin.op != CC_BIT_CLZ && e->bit_builtin.op != CC_BIT_CTZ)
                || !ci_uint128_nonzero(v))){
                CcExpr* second;
                err = cc_eval_expr(ctx, e->values[0], &second);
                if(err) return err;
                arg = cc_eval_u128(cc_target(p), second);
                _Bool negative = !ccqt_is_unsigned(second->type, !cc_target(p)->char_is_signed)
                    && (ci_uint128_hi(arg) >> 63);
                cc_release_expr(p, second);
                if(negative && (e->bit_builtin.op == CC_BIT_ROTATE_LEFT || e->bit_builtin.op == CC_BIT_ROTATE_RIGHT))
                    return CC_NOT_CONSTANT_ERROR;
            }
            uint32_t sz;
            err = cc_sizeof_as_uint(p, e->lhs->type, e->loc, &sz);
            if(err) return err;
            if(!cc_bit_builtin(e->bit_builtin.op, v, sz*8, arg, e->bit_builtin.nargs, &bits))
                return CC_NOT_CONSTANT_ERROR;
            *result = cc_integer_bits_expr(p, e->loc, e->type, bits);
            return *result ? 0 : CC_OOM_ERROR;
        }

        case CC_EXPR_BSWAP:{
            CcExpr* operand;
            int err = cc_eval_expr(ctx, e->lhs, &operand);
            if(err) return err;
            uint64_t v;
            err = cc_eval_to_u(p, operand, &v);
            if(err) goto fini_bswap;
            uint32_t sz;
            err = cc_sizeof_as_uint(p, operand->type, e->loc, &sz);
            if(err) goto fini_bswap;
            switch(sz){
                case 2: v = bswap16((uint16_t)v); break;
                case 4: v = bswap32((uint32_t)v); break;
                case 8: v = bswap64((uint64_t)v); break;
                default:
                    err = CC_UNREACHABLE_ERROR;
                    goto fini_bswap;
            }

            CcExpr* node = cc_value_expr(p, e->loc, operand->type);
            if(!node){
                err = CC_OOM_ERROR;
                goto fini_bswap;
            }
            node->uinteger = v;
            *result = node;
            fini_bswap:;
            cc_release_expr(p, operand);
            return err;
        }
        case CC_EXPR_SRCLOC_REFLECT:{
            CcExpr* operand;
            int err = cc_eval_expr(ctx, e->lhs, &operand);
            if(err) return err;
            SrcLoc loc = operand->loc_value;
            if(!loc.bits) return CC_NOT_CONSTANT_ERROR;
            size_t line = loc.line, col = loc.column, file_id = loc.file_id;
            if(loc.is_actually_a_pointer){
                SrcLocExp* exp = (SrcLocExp*)((uintptr_t)loc.pointer.bits << 1);
                while(exp->parent) exp = exp->parent;
                line = exp->line;
                col = exp->column;
                file_id = exp->file_id;
            }
            CcExpr* node;
            switch(e->srcloc.op){
                DRP_CASES_EXHAUSTED;
                case CC_SRCLOC_FILE:{
                    FileCache* fc = p->cpp.fc;
                    if(file_id >= fc->map.count)
                        return CC_NOT_CONSTANT_ERROR;
                    CStringView path = fc->map.data[file_id].path;
                    Atom a = AT_atomize(p->cpp.at, path.text, path.length);
                    if(!a) return CC_OOM_ERROR;
                    node = cc_constexpr_string_slice_expr(p, e->loc, a, 0);
                    if(!node) return CC_OOM_ERROR;
                    break;
                }
                case CC_SRCLOC_LINE:
                case CC_SRCLOC_COL:
                    node = cc_value_expr(p, e->loc, e->type);
                    if(!node) return CC_OOM_ERROR;
                    node->uinteger = e->srcloc.op == CC_SRCLOC_LINE?line:col;
                    break;
            }
            *result = node;
            return 0;
        }
        case CC_EXPR_ALLOCA:
        case CC_EXPR_INTERN:
        case CC_EXPR_HOTSWAP:
        case CC_EXPR_COMPILE:
        case CC_EXPR_MODULE_REFLECT:
        case CC_EXPR_UMUL128:
            return CC_NOT_CONSTANT_ERROR;
    }
}

static
int
cc_eval_integer(CcEvalCtx* ctx, CcExpr* e, int64_t* out){
    CcParser* p = ctx->parser;
    CcExpr* val;
    int err = cc_eval_expr(ctx, e, &val);
    // temporary hack, callers should report different error.
    if(err == CC_OVERFLOW_ERROR) err = CC_NOT_CONSTANT_ERROR;
    if(err) return err;
    if(!ccqt_is_integer(val->type)){
        err = CC_NOT_CONSTANT_ERROR;
        goto finish;
    }
    if(cc_eval_wide(val->type)){
        CiUint128 bits = cc_eval_u128(cc_target(p), val);
        _Bool is_unsigned = ccqt_is_unsigned(val->type, !cc_target(p)->char_is_signed);
        _Bool positive = !ci_uint128_hi(bits) && (is_unsigned || ci_uint128_lo(bits) <= INT64_MAX);
        _Bool negative = !is_unsigned && ci_uint128_hi(bits) == UINT64_MAX && ci_uint128_lo(bits) > INT64_MAX;
        if(!positive && !negative){
            err = CC_NOT_CONSTANT_ERROR;
            goto finish;
        }
    }
    err = cc_eval_to_i(p, val, out);
    finish:
    cc_release_expr(p, val);
    return err;
}

static
int
cc_eval_truthy(CcEvalCtx* ctx, CcExpr* e, _Bool* out){
    CcParser* p = ctx->parser;
    CcExpr* val;
    int err = cc_eval_expr(ctx, e, &val);
    if(err == CC_OVERFLOW_ERROR) err = CC_NOT_CONSTANT_ERROR;
    if(err == CC_NOT_CONSTANT_ERROR && ccqt_has_pointer_representation(e->type)){
        CcEvalAddress address;
        err = cc_eval_address(ctx, e, 0, 0, &address);
        if(err) return err;
        if(address.kind != CC_EVAL_ABSOLUTE && !cc_eval_address_nonnull(address))
            return CC_NOT_CONSTANT_ERROR;
        *out = address.kind != CC_EVAL_ABSOLUTE || address.offset != 0;
        return 0;
    }
    if(err) return err;
    *out = cc_eval_value_truth(cc_target(p), val);
    cc_release_expr(p, val);
    return 0;
}

#ifdef __clang__
#pragma clang assume_nonnull end
#endif
#include "nc_abi.c"
#include "cc_type_cache.c"
#include "cc_scope.c"
#include "cc_printer.c"
#include "cc_type_check.c"
#include "cc_expr.c"
#endif
