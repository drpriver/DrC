//
// Copyright © 2026-2026, David Priver <david@davidpriver.com>
//
#ifndef C_CI_LOWER_C
#define C_CI_LOWER_C
#include <string.h>
#include "cc_target.h"
#include "ci_interp.h"
#include "ci_op.h"
#include "cc_errors.h"
#include "srcloc.h"
#include "../Drp/atom.h"
#include "../Drp/merge_sort.h"
#include "../Drp/ckdint.h"

#ifndef ci_ice
#define ci_ice(ci, loc, fmt, ...) ci_error(ci, loc, "ICE: " fmt " at %s:%d", __VA_ARGS__, __FILE__, __LINE__)
#endif

#ifndef MARRAY_CCSWITCHENTRY
#define MARRAY_CCSWITCHENTRY
#define MARRAY_T CcSwitchEntry
#include "../Drp/Marray.h"
#endif

typedef struct CiBackpatchTarget CiBackpatchTarget;
struct CiBackpatchTarget {
    enum {
        CI_BP_NONE,
        CI_BP_LABEL,
        CI_BP_BREAK,
        CI_BP_CONTINUE,
    } kind;
    Atom _Null_unspecified label;
    size_t byteoffset;
    SrcLoc loc;
};
#ifndef MARRAY_CIBACKPATCHTARGET
#define MARRAY_CIBACKPATCHTARGET
#define MARRAY_T CiBackpatchTarget
#include "../Drp/Marray.h"
#endif

#ifdef __clang__
#pragma clang assume_nonnull begin
#endif

typedef struct CiLowerSwitch CiLowerSwitch;
struct CiLowerSwitch {
    Marray(CcSwitchEntry) entries;
    uint32_t default_target;
    _Bool has_default;
};


typedef struct CiLowerCtx CiLowerCtx;
struct CiLowerCtx {
    Allocator a;
    Marray(CiOp)* out;
    AtomMap(uintptr_t)* labels; // label -> op index + 1
    CiLowerSwitch* _Nullable sw; // innermost switch being lowered
    uint32_t temp; // top of the temp slot stack; statements save/restore
                   // this around their children so siblings recycle slots
    uint32_t* frame_size; // high-water mark of temp usage: &func->frame_size,
                          // or the toplevel/module slot size
    Marray(CiBackpatchTarget) backpatches;
    uint32_t size_size, ptr_size; // cached common sizes
    _Bool char_is_unsigned;
};

typedef struct CiLowerVal CiLowerVal;
struct CiLowerVal {
    uint32_t slot;
    uint32_t size;
    _Bool canonical; // already 0 or 1
};
#define CI_NO_SLOT UINT32_MAX

static const CcTargetConfig* ci_target(const CiInterpreter*);
static int ci_lower_stmt(CiInterpreter* ci, CiLowerCtx* ctx, CcStmtNode*_Nullable n);
static int ci_lower_stmt_inner(CiInterpreter* ci, CiLowerCtx* ctx, CcStmtNode* n);
static int ci_lower_expr(CiInterpreter* ci, CiLowerCtx* ctx, CcExpr* e, uint32_t dest, CiLowerVal* out);
static int ci_lower_cast_operand(CiInterpreter* ci, CiLowerCtx* ctx, CcExpr* e, uint32_t dest, CiLowerVal* out);
static int ci_lower_expr_discard(CiInterpreter* ci, CiLowerCtx* ctx, CcExpr* e);
static int ci_lower_cond(CiInterpreter* ci, CiLowerCtx* ctx, CcExpr* cond, CiLowerVal* out);
static int ci_lower_branch(CiInterpreter* ci, CiLowerCtx* ctx, CcExpr* cond, _Bool when_true, SrcLoc loc, uint32_t* chain);
static void ci_patch_branches(CiLowerCtx* ctx, uint32_t chain, uint32_t target);
static int ci_lower_istrue(CiLowerCtx* ctx, const CiLowerVal* v, CcQualType src_type, uint32_t dest, uint32_t dest_size, _Bool negate, SrcLoc loc);
static int ci_lower_dest(CiLowerCtx* ctx, uint32_t* dest, uint32_t size);
typedef struct CiLowerAddr CiLowerAddr;
struct CiLowerAddr {
    uint32_t slot;
    uint32_t disp;
};
static _Bool ci_frame_lvalue(const CcExpr* lv, uint32_t* offset);
static int ci_lower_addr(CiInterpreter* ci, CiLowerCtx* ctx, CcExpr* lv, _Bool one_past_ok, CiLowerAddr* out);
static int ci_lower_materialize_addr(CiInterpreter* ci, CiLowerCtx* ctx, CcExpr* e, CiLowerAddr* out);
static int ci_addr_to_value(CiLowerCtx* ctx, CiLowerAddr a, uint32_t dest, uint32_t size, SrcLoc loc, CiLowerVal* out);
static int ci_lower_bitfield_addr(CiInterpreter* ci, CiLowerCtx* ctx, CcExpr* lv, CiLowerAddr* out);
static int ci_emit_load_bitfield(CiInterpreter* ci, CiLowerCtx* ctx, const CcExpr* lv, CiLowerAddr a, uint32_t dest, uint32_t size);
static int ci_emit_store_bitfield(CiLowerCtx* ctx, const CcExpr* lv, CiLowerAddr a, uint32_t src, uint32_t size);
static _Bool ci_falu_type(CcQualType t);
static CiAluOp ci_alu_op_for(CcExprKind kind);
static CiCmpOp ci_cmp_op_for(CcExprKind kind);
static CiOpKind ci_int_op_kind(uint32_t size);
static CiOpKind ci_cmp_op_kind(uint32_t size);
static _Bool ci_falu_op_for(CcExprKind kind, CiFaluOp* out);
static int ci_lower_assign_direct(CiInterpreter* ci, CiLowerCtx* ctx, CcExpr* e, _Bool* handled);
static int ci_lower_init_list(CiInterpreter* ci, CiLowerCtx* ctx, CcExpr* e, CiLowerAddr dst, _Bool zero);
static int ci_lower_incdec(CiInterpreter* ci, CiLowerCtx* ctx, CcExpr* e, uint32_t dest, CiLowerVal*_Nullable out);
static int ci_lower_checked(CiInterpreter* ci, CiLowerCtx* ctx, CcExpr* e, uint32_t dest, CiLowerVal*_Nullable out);
static int ci_lower_umul128(CiInterpreter* ci, CiLowerCtx* ctx, CcExpr* e, uint32_t dest, CiLowerVal*_Nullable out);
static int ci_lower_bitcount(CiInterpreter* ci, CiLowerCtx* ctx, CcExpr* e, uint32_t dest, CiLowerVal* out);
static int ci_lower_call(CiInterpreter* ci, CiLowerCtx* ctx, CcExpr* e, uint32_t dest, CiLowerVal*_Nullable out);
static _Bool ci_armw_op_for(CcExprKind kind, CiAtomicRmwOp* out);
static int ci_lower_atomic_load_lv(CiInterpreter* ci, CiLowerCtx* ctx, CcExpr* e, uint32_t dest, CiLowerVal* out, uint32_t size);
static int ci_lower_atomic_compound_assign(CiInterpreter* ci, CiLowerCtx* ctx, CcExpr* e, uint32_t dest, CiLowerVal* out, uint32_t size, _Bool is_ptr, uint32_t elem_sz, _Bool op_unsigned, _Bool is_float, CiFaluOp fop);
static int ci_lower_atomic_builtin(CiInterpreter* ci, CiLowerCtx* ctx, CcExpr* e, uint32_t dest, CiLowerVal*_Nullable out);
static int ci_lower_va(CiInterpreter* ci, CiLowerCtx* ctx, CcExpr* e, uint32_t dest, CiLowerVal*_Nullable out);
static int ci_lower_slice(CiInterpreter* ci, CiLowerCtx* ctx, CcExpr* e, uint32_t dest, CiLowerVal* out);
static int ci_lower_rt_call(CiInterpreter* ci, CiLowerCtx* ctx, CcExpr* e, CiRuntimeOp rt_op, uint32_t dest, CiLowerVal*_Nullable out);
static int ci_lower_reflect(CiInterpreter* ci, CiLowerCtx* ctx, CcExpr* e, uint32_t dest, CiLowerVal*_Nullable out);
static int ci_alloc_slot(CiLowerCtx*, uint32_t sz, uint32_t align, uint32_t* slot);
static void ci_backpatch_break_continue(CiLowerCtx*, size_t start, uint32_t break_target, uint32_t continue_target);
static void ci_backpatch_break(CiLowerCtx*, size_t start, uint32_t break_target);
static int ci_lower_resolve_gotos(CiInterpreter*, CiLowerCtx*);
static int ci_cmp_switch_entry(void*_Null_unspecified ctx, const void* a, const void* b);

typedef struct CiFoldValue CiFoldValue;
struct CiFoldValue {
    uint32_t sz;
    CcQualType type;
    uint64_t bits[2];
};
enum {FOLD_FAIL=-1};
// return 0 on success, positive error code on real error, FOLD_FAIL on fold fail
static int ci_fold_expr(CiInterpreter* ci, CiLowerCtx* ctx, CcExpr* e, CiFoldValue* folded);
static uint64_t ci_fold_offset(CiLowerCtx* ctx, const CiFoldValue* index, uint32_t elem_sz);
static int ci_fold_condition(CiInterpreter* ci, CiLowerCtx* ctx, CcExpr* e, _Bool* truth);

static
int
ci_lower_stmt(CiInterpreter* ci, CiLowerCtx* ctx, CcStmtNode*_Nullable n){
    if(!n) return 0;
    uint32_t temp = ctx->temp;
    int err = ci_lower_stmt_inner(ci, ctx, (CcStmtNode*_Nonnull)n);
    ctx->temp = temp;
    return err;
}

static
int
ci_lower_stmt_inner(CiInterpreter* ci, CiLowerCtx* ctx, CcStmtNode* n){
    int err;
    switch(n->kind){
        case CC_STMT_NULL:
            return 0;
        case CC_STMT_EXPR:
            return ci_lower_expr_discard(ci, ctx, n->exprs[0]);
        case CC_STMT_COMPOUND:{
            for(uint32_t i = 0; i < n->count; i++){
                err = ci_lower_stmt(ci, ctx, n->stmts[i]);
                if(err) return err;
            }
            return 0;
        }
        case CC_STMT_IF:{
            CcExpr* cond = n->exprs[0];
            CiOp* op;
            uint32_t chain = 0;
            err = ci_lower_branch(ci, ctx, cond, 0, n->loc, &chain);
            if(err) return err;
            err = ci_lower_stmt(ci, ctx, n->stmts[0]);
            if(err) return err;
            if(!n->stmts[1]){
                ci_patch_branches(ctx, chain, (uint32_t)ctx->out->count);
            }
            else {
                err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
                if(err) return err;
                *op = (CiOp){
                    .jump = {
                        .kind = CI_OP_JUMP,
                        .loc = n->loc,
                    }
                };
                ci_patch_branches(ctx, chain, (uint32_t)ctx->out->count);
                ptrdiff_t jump = (char*)&op->jump.jump - (char*)ctx->out->data;
                err = ci_lower_stmt(ci, ctx, n->stmts[1]);
                if(err) return err;
                *(uint32_t*)((char*)ctx->out->data+jump) = (uint32_t)ctx->out->count;
            }
            return 0;
        }
        case CC_STMT_WHILE:{
            CcExpr* cond = n->exprs[0];
            uint32_t cond_idx = (uint32_t)ctx->out->count;
            CiOp* op;
            uint32_t chain = 0;
            err = ci_lower_branch(ci, ctx, cond, 0, n->loc, &chain);
            if(err) return err;
            size_t backpatch_start = ctx->backpatches.count;
            err = ci_lower_stmt(ci, ctx, n->stmts[0]);
            if(err) return err;
            err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
            if(err) return err;
            *op = (CiOp){
                .jump = {
                    .kind = CI_OP_JUMP,
                    .loc = n->loc,
                    .jump = cond_idx,
                }
            };
            uint32_t break_idx = (uint32_t)ctx->out->count;
            ci_patch_branches(ctx, chain, break_idx);
            ci_backpatch_break_continue(ctx, backpatch_start, break_idx, cond_idx);
            return 0;
        }
        case CC_STMT_DOWHILE:{
            CcExpr* cond = n->exprs[0];
            size_t backpatch_start = ctx->backpatches.count;
            uint32_t body_start = (uint32_t)ctx->out->count;
            err = ci_lower_stmt(ci, ctx, n->stmts[0]);
            if(err) return err;
            uint32_t cond_idx = (uint32_t)ctx->out->count;
            uint32_t chain = 0;
            err = ci_lower_branch(ci, ctx, cond, 1, n->loc, &chain);
            if(err) return err;
            ci_patch_branches(ctx, chain, body_start);
            ci_backpatch_break_continue(ctx, backpatch_start, (uint32_t)ctx->out->count, cond_idx);
            return 0;
        }
        case CC_STMT_FOR:{
            // init;
            // top: cond;               (if cond)
            // if false goto break;     (if cond)
            // body;
            // continue: inc;           (if inc)
            // goto top;
            // break:
            CiOp* op;
            if(n->stmts[0]){ // initializer
                err = ci_lower_stmt(ci, ctx, n->stmts[0]);
                if(err) return err;
            }
            size_t backpatch_start = ctx->backpatches.count;
            CcExpr* cond = n->exprs[0];
            CcExpr* inc = n->exprs[1];
            uint32_t top_idx = (uint32_t)ctx->out->count;
            uint32_t chain = 0;
            if(cond){
                err = ci_lower_branch(ci, ctx, cond, 0, n->loc, &chain);
                if(err) return err;
            }
            err = ci_lower_stmt(ci, ctx, n->stmts[1]); // body
            if(err) return err;
            uint32_t continue_idx = (uint32_t)ctx->out->count;
            if(inc){
                err = ci_lower_expr_discard(ci, ctx, inc);
                if(err) return err;
            }
            err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
            if(err) return err;
            *op = (CiOp){
                .jump = {
                    .kind = CI_OP_JUMP,
                    .loc = n->loc,
                    .jump = top_idx,
                }
            };
            uint32_t break_idx = (uint32_t)ctx->out->count;
            ci_patch_branches(ctx, chain, break_idx);
            ci_backpatch_break_continue(ctx, backpatch_start, break_idx, continue_idx);
            return 0;
        }
        case CC_STMT_SWITCH:{
            CcExpr* e = n->exprs[0];
            CiLowerVal v;
            err = ci_lower_expr(ci, ctx, e, CI_NO_SLOT, &v);
            if(err) return err;
            // _Type switches already produce the canonical type bits;
            // treat them as unsigned.
            _Bool is_unsigned = ccqt_bt_eq(e->type, CCBT__Type)
                || ccqt_is_unsigned(e->type, ctx->char_is_unsigned);
            CiOp* op;
            err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
            if(err) return err;
            *op = (CiOp){
                .switch_ = {
                    .kind = CI_OP_SWITCH,
                    .slot = v.slot,
                    .slot_size = v.size,
                    .loc = n->loc,
                    .is_unsigned = is_unsigned,
                }
            };
            uint32_t sw_idx = (uint32_t)(op - ctx->out->data);
            size_t backpatch_start = ctx->backpatches.count;
            CiLowerSwitch sw = {0};
            {
                CiLowerSwitch* prev = ctx->sw;
                ctx->sw = &sw;
                err = ci_lower_stmt(ci, ctx, n->stmts[0]);
                ctx->sw = prev;
            }
            if(err) goto switch_cleanup;
            // Sort entries by value
            if(sw.entries.count){
                size_t scratch_sz = sw.entries.count * sizeof(CcSwitchEntry);
                void* scratch = Allocator_alloc(ctx->a, scratch_sz);
                if(!scratch){ err = CI_OOM_ERROR; goto switch_cleanup; }
                drp_merge_sort(scratch, sw.entries.data, sw.entries.count, sizeof(CcSwitchEntry), NULL, ci_cmp_switch_entry);
                Allocator_free(ctx->a, scratch, scratch_sz);
            }
            // Check for duplicate case values
            for(size_t i = 1; i < sw.entries.count; i++){
                if(sw.entries.data[i].value == sw.entries.data[i-1].value){
                    err = ci_error(ci, n->loc, "duplicate case value '%lld'", (long long)sw.entries.data[i].value);
                    goto switch_cleanup;
                }
            }
            // Copy the sorted entries into the op's out-of-line table
            {
                size_t table_size = sizeof(CiSwitchTable) + sw.entries.count * sizeof(CcSwitchEntry);
                CiSwitchTable* table = Allocator_alloc(ctx->a, table_size);
                if(!table){ err = CI_OOM_ERROR; goto switch_cleanup; }
                table->count = sw.entries.count;
                if(sw.entries.count)
                    memcpy(table->data, sw.entries.data, sw.entries.count * sizeof(CcSwitchEntry));
                uint32_t break_target = (uint32_t)ctx->out->count;
                CiOp* swop = &ctx->out->data[sw_idx];
                swop->switch_.jump = sw.has_default ? sw.default_target : break_target;
                swop->switch_.table = table;
                ci_backpatch_break(ctx, backpatch_start, break_target);
            }
            switch_cleanup:
            ma_cleanup(CcSwitchEntry)(&sw.entries, ctx->a);
            return err;
        }
        case CC_STMT_CASE:{
            if(!ctx->sw)
                return ci_ice(ci, n->loc, "case label outside of switch in lowering%s", "");
            CcSwitchEntry entry = {.value = n->case_value, .target = (uint32_t)ctx->out->count};
            err = ma_push(CcSwitchEntry)(&ctx->sw->entries, ctx->a, entry);
            if(err) return CI_OOM_ERROR;
            return ci_lower_stmt(ci, ctx, n->stmts[0]);
        }
        case CC_STMT_DEFAULT:{
            if(!ctx->sw)
                return ci_ice(ci, n->loc, "default label outside of switch in lowering%s", "");
            ctx->sw->has_default = 1;
            ctx->sw->default_target = (uint32_t)ctx->out->count;
            return ci_lower_stmt(ci, ctx, n->stmts[0]);
        }
        case CC_STMT_RETURN:{
            CcExpr* e = n->exprs[0];
            CiOp* op;
            if(e && ccqt_bt_eq(e->type, CCBT_void)){
                err = ci_lower_expr_discard(ci, ctx, e);
                if(err) return err;
            }
            else if(e){
                CiLowerVal v;
                err = ci_lower_expr(ci, ctx, e, CI_NO_SLOT, &v);
                if(err) return err;
                err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
                if(err) return err;
                *op = (CiOp){
                    .return_slot = {
                        .kind = CI_OP_RETURN_SLOT,
                        .src = v.slot,
                        .src_size = v.size,
                        .loc = n->loc,
                    }
                };
                return 0;
            }
            err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
            if(err) return err;
            *op = (CiOp){
                .return_ = {
                    .kind = CI_OP_RETURN,
                    .loc = n->loc,
                }
            };
            return 0;
        }
        case CC_STMT_BREAK:
        case CC_STMT_CONTINUE:{
            CiOp* op;
            err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
            if(err) return err;
            *op = (CiOp){
                .jump = {
                    .kind = CI_OP_JUMP,
                    .loc = n->loc,
                }
            };
            CiBackpatchTarget t = {
                .kind = n->kind == CC_STMT_BREAK? CI_BP_BREAK : CI_BP_CONTINUE,
                .byteoffset = (size_t)((char*)&op->jump.jump - (char*)ctx->out->data),
                .loc = n->loc,
            };
            err = ma_push(CiBackpatchTarget)(&ctx->backpatches, ctx->a, t);
            if(err) return CI_OOM_ERROR;
            return 0;
        }
        case CC_STMT_GOTO:{
            CiOp* op;
            err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
            if(err) return err;
            *op = (CiOp){
                .jump = {
                    .kind = CI_OP_JUMP,
                    .loc = n->loc,
                }
            };
            CiBackpatchTarget t = {
                .kind = CI_BP_LABEL,
                .label = n->label,
                .byteoffset = (size_t)((char*)&op->jump.jump - (char*)ctx->out->data),
                .loc = n->loc,
            };
            err = ma_push(CiBackpatchTarget)(&ctx->backpatches, ctx->a, t);
            if(err) return CI_OOM_ERROR;
            return 0;
        }
        case CC_STMT_LABEL:{
            void* existing = AM_get(ctx->labels, n->label);
            if(existing)
                return ci_ice(ci, n->loc, "Duplicate label '%.*s'", n->label->length, n->label->data);
            err = AM_put(ctx->labels, ctx->a, n->label, (void*)(uintptr_t)(ctx->out->count + 1));
            if(err) return CI_OOM_ERROR;
            return ci_lower_stmt(ci, ctx, n->stmts[0]);
        }
    }
    return ci_ice(ci, n->loc, "unknown statement kind in lowering at%s", "");
}

static
int
ci_lower_init_list(CiInterpreter* ci, CiLowerCtx* ctx, CcExpr* e, CiLowerAddr dst, _Bool zero){
    int err;
    CcParser* p = &ci->parser;
    uint32_t sz;
    err = cc_sizeof_as_uint(p, e->type, e->loc, &sz);
    if(err) return err;
    CiOp* op;
    if(zero){
        err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
        if(err) return err;
        *op = (CiOp){
            .zero = {
                .kind = CI_OP_ZERO,
                .slot = dst.slot,
                .offset = dst.disp,
                .size = sz,
                .loc = e->loc,
            }
        };
    }
    CcInitList* l = e->init_list;
    uint64_t written_end = 0;
    for(uint32_t i = 0; i < l->count; i++){
        CcInitEntry* entry = &l->entries[i];
        CcExpr* value = entry->value;
        if(!value) continue;
        uint32_t esz;
        err = cc_sizeof_as_uint(p, value->type, value->loc, &esz);
        if(err) return err;
        uint32_t off = dst.disp + (uint32_t)entry->field_loc.byte_offset;
        uint64_t start = entry->field_loc.byte_offset;
        uint64_t end = start + (entry->field_loc.bit_width ? (entry->field_loc.bit_offset + entry->field_loc.bit_width + 7u) / 8u : esz);
        if(!entry->field_loc.bit_width && value->kind == CC_EXPR_INIT_LIST){
            _Bool overwritten = 0;
            // In-order, disjoint initializers need no scan or extra zeroing.
            // A backwards designator may replace an already written subobject.
            if(start < written_end && esz){
                for(uint32_t j = 0; j < i; j++){
                    CcInitEntry* prev = &l->entries[j];
                    if(!prev->value) continue;
                    uint64_t prev_start = prev->field_loc.byte_offset;
                    if(prev_start >= end) continue;
                    uint32_t prev_size;
                    err = cc_sizeof_as_uint(p, prev->value->type, prev->value->loc, &prev_size);
                    if(err) return err;
                    uint64_t prev_end = prev_start + prev_size;
                    if(prev->field_loc.bit_width){
                        prev_start += prev->field_loc.bit_offset / 8;
                        prev_end = prev->field_loc.byte_offset
                            + (prev->field_loc.bit_offset + prev->field_loc.bit_width + 7) / 8;
                    }
                    if(prev_start < end && start < prev_end){
                        overwritten = 1;
                        break;
                    }
                }
            }
            err = ci_lower_init_list(ci, ctx, value, (CiLowerAddr){.slot = dst.slot, .disp = off}, overwritten);
            if(err) return err;
            if(end > written_end) written_end = end;
            continue;
        }
        if(end > written_end) written_end = end;
        uint32_t temp = ctx->temp;
        CiLowerVal v;
        err = ci_lower_expr(ci, ctx, value, CI_NO_SLOT, &v);
        if(err) return err;
        err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
        if(err) return err;
        if(entry->field_loc.bit_width){
            *op = (CiOp){
                .store_bf = {
                    .kind = CI_OP_STORE_BITFIELD,
                    .slot = dst.slot,
                    .src = v.slot,
                    .src_size = esz,
                    .offset = off,
                    .bit_offset = entry->field_loc.bit_offset,
                    .bit_width = entry->field_loc.bit_width,
                    .loc = value->loc,
                }
            };
        }
        else {
            *op = (CiOp){
                .store = {
                    .kind = CI_OP_STORE,
                    .slot = dst.slot,
                    .src = v.slot,
                    .src_size = esz,
                    .offset = off,
                    .loc = value->loc,
                }
            };
        }
        ctx->temp = temp;
    }
    return 0;
}

static
int
ci_lower_expr(CiInterpreter* ci, CiLowerCtx* ctx, CcExpr* e, uint32_t dest, CiLowerVal* out){
    int err;
    {
        CiFoldValue fold_val;
        int fold_fail = ci_fold_expr(ci, ctx, e, &fold_val);
        if(fold_fail > 0) return fold_fail;
        if(fold_fail == 0){
            err = ci_lower_dest(ctx, &dest, fold_val.sz);
            if(err) return err;
            out->slot = dest;
            out->size = fold_val.sz;
            out->canonical = ccqt_is_integer(fold_val.type) && !fold_val.bits[1] && fold_val.bits[0] <= 1;
            CiOp* op;
            err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
            if(err) return err;
            *op = (CiOp){
                .constant = {
                    .kind = CI_OP_CONST,
                    .bt_kind = (uint32_t)(ccqt_is_basic(fold_val.type)? fold_val.type.basic.kind : CCBT_void),
                    .slot = dest,
                    .immsize = fold_val.sz,
                    .loc = e->loc,
                },
            };
            memcpy(op->constant.immediate, fold_val.bits, sizeof fold_val.bits);
            return 0;
        }
    }
    CcParser* p = &ci->parser;
    uint32_t size;
    err = cc_sizeof_as_uint(p, e->type, e->loc, &size);
    if(err) return err;
    out->size = size;
    out->canonical = 0;
    switch(e->kind){
        case CC_EXPR_VALUE:{
            if(ccqt_kind(e->type) == CC_ARRAY){
                err = ci_lower_dest(ctx, &dest, size);
                if(err) return err;
                out->slot = dest;
                uint32_t aslot;
                err = ci_alloc_slot(ctx, 8, 8, &aslot);
                if(err) return err;
                CiOp* op;
                err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
                if(err) return err;
                CcArray* arr = ccqt_as_array(e->type);
                *op = (CiOp){
                    .constant = {
                        .kind = CI_OP_CONST,
                        .bt_kind = (uint32_t)(ccqt_is_basic(arr->element)?arr->element.basic.kind:CCBT_nullptr_t),
                        .is_anon_array = 1,
                        .immsize = 8,
                        .slot = aslot,
                        .immediate = {
                            (uint64_t)e->text,
                            e->str.length,
                        },
                        .loc = e->loc,
                    },
                };
                err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
                if(err) return err;
                *op = (CiOp){
                    .load = {
                        .kind = CI_OP_LOAD,
                        .slot = dest,
                        .slot_size = size,
                        .src = aslot,
                        .offset = 0,
                        .loc = e->loc,
                    },
                };
                return 0;
            }
            return ci_unimplemented(ci, e->loc, "oversized or unsupported values");
        }
        case CC_EXPR_VARIABLE:{
            CcVariable* var = e->var;
            if(e->type.is_atomic){
                return ci_lower_atomic_load_lv(ci, ctx, e, dest, out, size);
            }
            if(!var->automatic){
                // static/global: address through the GOT-style op, then load.
                // Array rvalues (array assignment extension) load like any
                // other whole object.
                err = ci_lower_dest(ctx, &dest, size);
                if(err) return err;
                out->slot = dest;
                uint32_t temp = ctx->temp;
                uint32_t aslot;
                err = ci_alloc_slot(ctx, 8, 8, &aslot);
                if(err) return err;
                CiOp* op;
                err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
                if(err) return err;
                *op = (CiOp){
                    .var_addr = {
                        .kind = CI_OP_VAR_ADDR,
                        .slot = aslot,
                        .slot_size = 8,
                        .var = var,
                        .loc = e->loc,
                    }
                };
                err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
                if(err) return err;
                *op = (CiOp){
                    .load = {
                        .kind = CI_OP_LOAD,
                        .slot = dest,
                        .slot_size = size,
                        .src = aslot,
                        .loc = e->loc,
                    }
                };
                ctx->temp = temp;
                return 0;
            }
            if(dest == CI_NO_SLOT){
                // the variable's storage is already a slot; refer to it directly
                out->slot = (uint32_t)var->frame_offset;
                return 0;
            }
            out->slot = dest;
            CiOp* op;
            err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
            if(err) return err;
            *op = (CiOp){
                .copy = {
                    .kind = CI_OP_COPY,
                    .slot = dest,
                    .slot_size = size,
                    .src = (uint32_t)var->frame_offset,
                    .src_size = size,
                    .loc = e->loc,
                }
            };
            return 0;
        }
        case CC_EXPR_DEREF:
        case CC_EXPR_ARROW:
        case CC_EXPR_DOT:
        case CC_EXPR_SUBSCRIPT:{
            if(e->type.is_atomic){
                return ci_lower_atomic_load_lv(ci, ctx, e, dest, out, size);
            }
            // Array rvalues (array assignment extension) load like any other
            // whole object.
            if((e->kind == CC_EXPR_DOT || e->kind == CC_EXPR_ARROW) && e->field_loc.bit_width){
                err = ci_lower_dest(ctx, &dest, size);
                if(err) return err;
                uint32_t temp = ctx->temp;
                CiLowerAddr a;
                err = ci_lower_bitfield_addr(ci, ctx, e, &a);
                if(err) return err;
                err = ci_emit_load_bitfield(ci, ctx, e, a, dest, size);
                if(err) return err;
                ctx->temp = temp;
                out->slot = dest;
                return 0;
            }
            uint32_t off;
            if(ci_frame_lvalue(e, &off)){
                // a member of a local is already a slot
                if(dest == CI_NO_SLOT){
                    out->slot = off;
                    return 0;
                }
                out->slot = dest;
                CiOp* op;
                err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
                if(err) return err;
                *op = (CiOp){
                    .copy = {
                        .kind = CI_OP_COPY,
                        .slot = dest,
                        .slot_size = size,
                        .src = off,
                        .src_size = size,
                        .loc = e->loc,
                    }
                };
                return 0;
            }
            err = ci_lower_dest(ctx, &dest, size);
            if(err) return err;
            uint32_t temp = ctx->temp;
            CiLowerAddr a;
            err = ci_lower_addr(ci, ctx, e, 0, &a); // access
            if(err) return err;
            out->slot = dest;
            CiOp* op;
            err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
            if(err) return err;
            *op = (CiOp){
                .load = {
                    .kind = CI_OP_LOAD,
                    .slot = dest,
                    .slot_size = size,
                    .src = a.slot,
                    .offset = a.disp,
                    .loc = e->loc,
                }
            };
            ctx->temp = temp;
            return 0;
        }
        case CC_EXPR_ADDR:{
            CcExpr* lv = e->lhs;
            uint32_t off;
            CiOp* op;
            if(ci_frame_lvalue(lv, &off)){
                err = ci_lower_dest(ctx, &dest, size);
                if(err) return err;
                out->slot = dest;
                err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
                if(err) return err;
                *op = (CiOp){
                    .slot_addr = {
                        .kind = CI_OP_SLOT_ADDR,
                        .slot = dest,
                        .slot_size = size,
                        .src = off,
                        .loc = e->loc,
                    }
                };
                return 0;
            }
            CiLowerAddr a;
            err = ci_lower_addr(ci, ctx, lv, 1, &a); // address-of: one-past ok
            if(err) return err;
            return ci_addr_to_value(ctx, a, dest, size, e->loc, out);
        }
        case CC_EXPR_CAST:{
            CcExpr* operand = e->lhs;
            if(ccqt_kind(operand->type) == CC_ARRAY && ccqt_kind(e->type) == CC_SLICE)
                return ci_lower_slice(ci, ctx, e, dest, out);
            while(operand->kind == CC_EXPR_COMMA){
                err = ci_lower_expr_discard(ci, ctx, operand->lhs);
                if(err) return err;
                operand = operand->values[0];
            }
            CcQualType from = operand->type;
            CcQualType to = e->type;
            if(ccqt_bt_eq(to, CCBT_void)){
                out->slot = CI_NO_SLOT;
                out->size = 0;
                return ci_lower_expr_discard(ci, ctx, operand);
            }
            if(from.unqual == to.unqual)
                return ci_lower_expr(ci, ctx, operand, dest, out);
            if(ccqt_kind(from) == CC_SLICE && ccqt_kind(to) == CC_SLICE)
                return ci_lower_expr(ci, ctx, operand, dest, out);
            if(ccqt_bt_eq(to, CCBT__Any)){
                err = ci_lower_dest(ctx, &dest, size);
                if(err) return err;
                out->slot = dest;
                uint32_t temp = ctx->temp;
                uint32_t src_size;
                err = cc_sizeof_as_uint(p, from, e->loc, &src_size);
                if(err) return err;
                // Stage before writing the tag or zeroing the payload: the
                // source may refer to the destination's own payload.
                uint32_t src;
                err = ci_alloc_slot(ctx, src_size, 8, &src);
                if(err) return err;
                CiLowerVal v;
                err = ci_lower_expr(ci, ctx, operand, src, &v);
                if(err) return err;
                CiOp* ops;
                err = ma_alloc(CiOp)(ctx->out, ctx->a, &ops);
                if(err) return err;
                ops[0] = (CiOp){.constant = {
                    .kind = CI_OP_CONST, .bt_kind = CCBT__Any,
                    .slot = dest, .immsize = sizeof(CiRtAny),
                    .immediate = {((CcQualType){.unqual = from.unqual}).bits, 0},
                    .loc = e->loc,
                }};
                err = ma_alloc(CiOp)(ctx->out, ctx->a, &ops);
                if(err) return err;
                ops[0] = (CiOp){.copy = {
                    .kind = CI_OP_COPY, .slot = dest + offsetof(CiRtAny, payload),
                    .slot_size = src_size, .src = v.slot, .src_size = src_size,
                    .loc = e->loc,
                }};
                ctx->temp = temp;
                return 0;
            }
            _Bool from_float = ci_falu_type(from);
            _Bool from_addr = ccqt_kind(from) == CC_POINTER || ccqt_kind(from) == CC_ARRAY
                           || ccqt_kind(from) == CC_FUNCTION || ccqt_bt_eq(from, CCBT_nullptr_t);
            _Bool from_int = from_addr || ccqt_is_integer(from);
            uint32_t from_sz = ctx->ptr_size;
            if(ccqt_is_integer(from)){
                err = cc_sizeof_as_uint(p, from, e->loc, &from_sz);
                if(err) return err;
            }
            if(!from_int && !from_float)
                return ci_unimplemented(ci, e->loc, "cast from unsupported type");
            if(ccqt_bt_eq(to, CCBT_bool)){
                err = ci_lower_dest(ctx, &dest, size);
                if(err) return err;
                out->slot = dest;
                uint32_t temp = ctx->temp;
                CiLowerVal v;
                err = ci_lower_cast_operand(ci, ctx, operand, CI_NO_SLOT, &v);
                if(err) return err;
                err = ci_lower_istrue(ctx, &v, from, dest, size, 0, e->loc);
                if(err) return err;
                ctx->temp = temp;
                out->canonical = 1;
                return 0;
            }
            _Bool to_int = ccqt_kind(to) == CC_POINTER || ccqt_bt_eq(to, CCBT_nullptr_t) || ccqt_is_integer(to);
            _Bool to_float = ci_falu_type(to);
            CiOpKind kind;
            uint32_t is_unsigned = 0;
            if(from_int && to_int){
                if(from_sz == size)
                    return ci_lower_cast_operand(ci, ctx, operand, dest, out);
                kind = CI_OP_CONVERT;
                is_unsigned = from_addr || ccqt_is_unsigned(from, ctx->char_is_unsigned);
            }
            else if(from_int && to_float){
                kind = CI_OP_ITOF;
                is_unsigned = ccqt_is_unsigned(from, ctx->char_is_unsigned);
            }
            else if(from_float && to_int){
                kind = CI_OP_FTOI;
                is_unsigned = ccqt_is_unsigned(to, ctx->char_is_unsigned);
            }
            else if(from_float && to_float){
                kind = CI_OP_FTOF;
            }
            else {
                return ci_unimplemented(ci, e->loc, "cast to unsupported type");
            }
            err = ci_lower_dest(ctx, &dest, size);
            if(err) return err;
            out->slot = dest;
            uint32_t temp = ctx->temp;
            CiLowerVal v;
            err = ci_lower_cast_operand(ci, ctx, operand, CI_NO_SLOT, &v);
            if(err) return err;
            CiOp* op;
            err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
            if(err) return err;
            *op = (CiOp){
                .convert = {
                    .kind = kind,
                    .slot = dest,
                    .slot_size = size,
                    .src = v.slot,
                    .src_size = v.size,
                    .is_unsigned = is_unsigned,
                    .loc = e->loc,
                }
            };
            ctx->temp = temp;
            out->canonical = kind == CI_OP_CONVERT && v.canonical;
            return 0;
        }
        case CC_EXPR_POS:
            return ci_lower_expr(ci, ctx, e->lhs, dest, out);
        case CC_EXPR_NEG:
        case CC_EXPR_BITNOT:{
            CcExpr* operand = e->lhs;
            CiOpKind kind;
            _Bool is_not = e->kind == CC_EXPR_BITNOT;
            if(e->kind == CC_EXPR_NEG && ci_falu_type(e->type)){
                kind = e->type.basic.kind == CCBT_float? CI_OP_FALU32 : CI_OP_FALU64;
            }
            else if(ccqt_is_integer(e->type)){
                switch(size){
                    case 4: kind = CI_OP_ALU32; break;
                    case 8: kind = CI_OP_ALU64; break;
                    case 16: kind = CI_OP_ALU128; break;
                    default: return ci_ice(ci, e->loc, "unsupported alu operand size %u", size);
                }
            }
            else {
                return ci_unimplemented(ci, e->loc, "Unimplemented op");
            }
            uint32_t osz;
            err = cc_sizeof_as_uint(p, operand->type, operand->loc, &osz);
            if(err) return err;
            if(osz != size) return ci_ice(ci, e->loc, "Missing implicit cast? %u", osz);
            err = ci_lower_dest(ctx, &dest, size);
            if(err) return err;
            out->slot = dest;
            uint32_t temp = ctx->temp;
            CiLowerVal v;
            err = ci_lower_expr(ci, ctx, operand, CI_NO_SLOT, &v);
            if(err) return err;
            CiOp* op;
            err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
            if(err) return err;
            if(kind == CI_OP_ALU32 || kind == CI_OP_ALU64 || kind == CI_OP_ALU128){
                *op = (CiOp){
                    .alu = {
                        .kind = kind,
                        .op = is_not? CI_ALU_NOT : CI_ALU_NEG,
                        .is_unsigned = is_not,
                        .slot = dest,
                        .src = v.slot,
                        .src2 = v.slot,
                        .loc = e->loc,
                    }
                };
            }
            else {
                *op = (CiOp){
                    .falu32 = {
                        .kind = kind,
                        .op = CI_FALU_NEG,
                        .slot = dest,
                        .slot_size = size,
                        .src = v.slot,
                        .src2 = v.slot,
                        .loc = e->loc,
                    }
                };
            }
            ctx->temp = temp;
            return 0;
        }
        case CC_EXPR_LOGNOT:{
            err = ci_lower_dest(ctx, &dest, size);
            if(err) return err;
            out->slot = dest;
            uint32_t temp = ctx->temp;
            CiLowerVal v;
            err = ci_lower_expr(ci, ctx, e->lhs, CI_NO_SLOT, &v);
            if(err) return err;
            err = ci_lower_istrue(ctx, &v, e->lhs->type, dest, size, 1, e->loc);
            if(err) return err;
            ctx->temp = temp;
            out->canonical = 1;
            return 0;
        }
        case CC_EXPR_ASSIGN:{
            CcExpr* lhs = e->lhs;
            CcExpr* rhs = e->values[0];
            uint32_t rsz;
            err = cc_sizeof_as_uint(p, rhs->type, rhs->loc, &rsz);
            if(err) return err;
            if(rsz != size)
                return ci_unreachable(ci, rhs->loc, "size mismatch");
            if(lhs->type.is_atomic){
                CiLowerAddr a;
                err = ci_lower_addr(ci, ctx, lhs, 0, &a); // access
                if(err) return err;
                CiLowerVal v;
                err = ci_lower_expr(ci, ctx, rhs, dest, &v);
                if(err) return err;
                CiOp* op;
                err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
                if(err) return err;
                *op = (CiOp){
                    .atomic_store = {
                        .kind = CI_OP_ATOMIC_STORE,
                        .memorder = CC_MO_SEQ_CST,
                        .slot = a.slot,
                        .src = v.slot,
                        .src_size = size,
                        .offset = a.disp,
                        .loc = e->loc,
                    }
                };
                out->slot = v.slot;
                out->canonical = v.canonical;
                return 0;
            }
            uint32_t vslot;
            if(ci_frame_lvalue(lhs, &vslot)){
                CiLowerVal v;
                err = ci_lower_expr(ci, ctx, rhs, vslot, &v);
                if(err) return err;
                out->canonical = v.canonical;
                if(dest == CI_NO_SLOT){
                    out->slot = vslot;
                    return 0;
                }
                out->slot = dest;
                CiOp* op;
                err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
                if(err) return err;
                *op = (CiOp){
                    .copy = {
                        .kind = CI_OP_COPY,
                        .slot = dest,
                        .slot_size = size,
                        .src = vslot,
                        .src_size = size,
                        .loc = e->loc,
                    }
                };
                return 0;
            }
            CiLowerAddr a;
            if((lhs->kind == CC_EXPR_DOT || lhs->kind == CC_EXPR_ARROW) && lhs->field_loc.bit_width){
                // read-modify-write on the storage unit; the assignment's
                // value is the stored bits, re-read truncated and extended
                err = ci_lower_bitfield_addr(ci, ctx, lhs, &a);
                if(err) return err;
                CiLowerVal v;
                err = ci_lower_expr(ci, ctx, rhs, CI_NO_SLOT, &v);
                if(err) return err;
                err = ci_emit_store_bitfield(ctx, lhs, a, v.slot, size);
                if(err) return err;
                err = ci_lower_dest(ctx, &dest, size);
                if(err) return err;
                err = ci_emit_load_bitfield(ci, ctx, lhs, a, dest, size);
                if(err) return err;
                out->slot = dest;
                return 0;
            }
            err = ci_lower_addr(ci, ctx, lhs, 0, &a); // access
            if(err) return err;
            CiLowerVal v;
            err = ci_lower_expr(ci, ctx, rhs, dest, &v);
            if(err) return err;
            CiOp* op;
            err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
            if(err) return err;
            *op = (CiOp){
                .store = {
                    .kind = CI_OP_STORE,
                    .slot = a.slot,
                    .src = v.slot,
                    .src_size = size,
                    .offset = a.disp,
                    .loc = e->loc,
                }
            };
            out->slot = v.slot;
            out->canonical = v.canonical;
            return 0;
        }
        case CC_EXPR_ADDASSIGN:
        case CC_EXPR_SUBASSIGN:
        case CC_EXPR_MULASSIGN:
        case CC_EXPR_DIVASSIGN:
        case CC_EXPR_MODASSIGN:
        case CC_EXPR_BITANDASSIGN:
        case CC_EXPR_BITORASSIGN:
        case CC_EXPR_BITXORASSIGN:
        case CC_EXPR_LSHIFTASSIGN:
        case CC_EXPR_RSHIFTASSIGN:{
            CcExpr* lhs = e->lhs;
            CcExpr* rhs = e->values[0];
            // The parser casts the rhs to the lhs type (cc_implicit_cast), so
            // both operands share e->type here. Decide the arithmetic before
            // emitting anything with side effects (the lvalue is evaluated
            // once).
            CiOpKind opkind;
            CiFaluOp fop = 0;
            _Bool is_ptr = 0;
            uint32_t elem_sz = 1;
            _Bool op_unsigned = 0;
            if(ccqt_is_integer(e->type) && ccqt_is_integer(rhs->type)){
                uint32_t rsz;
                err = cc_sizeof_as_uint(p, rhs->type, rhs->loc, &rsz);
                if(err) return err;
                if(size != rsz) return ci_ice(ci, e->loc, "lhs and rhs sizes differ %u %u", size, rsz);
                opkind = ci_int_op_kind(size);
                if(!opkind) return ci_ice(ci, e->loc, "unsupported integer ALU size %u", size);
                op_unsigned = ccqt_is_unsigned(e->type, ctx->char_is_unsigned);
            }
            else if(ci_falu_type(e->type) && ci_falu_op_for(e->kind, &fop)){
                opkind = size == 4? CI_OP_FALU32 : CI_OP_FALU64;
            }
            else if(ccqt_kind(e->type) == CC_POINTER
                 && (e->kind == CC_EXPR_ADDASSIGN || e->kind == CC_EXPR_SUBASSIGN)
                 && ccqt_is_integer(rhs->type)){
                uint32_t rsz;
                err = cc_sizeof_as_uint(p, rhs->type, rhs->loc, &rsz);
                if(err) return err;
                if(rsz != ctx->size_size)
                    return ci_ice(ci, e->loc, "ptr arith failed to implicit cast to right size: %u", rsz);
                err = cc_sizeof_as_uint(p, ccqt_as_ptr(e->type)->pointee, e->loc, &elem_sz);
                if(err) return err;
                is_ptr = 1;
                opkind = ctx->ptr_size == 16?CI_OP_ALU128:ctx->ptr_size == 4? CI_OP_ALU32:CI_OP_ALU64;
                op_unsigned = ccqt_is_unsigned(rhs->type, ctx->char_is_unsigned);
            }
            else {
                return ci_unimplemented(ci, e->loc, "huh?");
            }
            if((lhs->kind == CC_EXPR_DOT || lhs->kind == CC_EXPR_ARROW) && lhs->field_loc.bit_width){
                if(opkind == CI_OP_ALU128)
                    return ci_unimplemented(ci, e->loc, "128bit bitfields");
                if(opkind != CI_OP_ALU64 && opkind != CI_OP_ALU32 && opkind != CI_OP_ALU16 && opkind != CI_OP_ALU8)
                    return ci_ice(ci, e->loc, "Invalid bitfield op%s", "");
                uint32_t cur;
                err = ci_alloc_slot(ctx, size, size, &cur);
                if(err) return err;
                uint32_t keep = ctx->temp; // cur is established; the rest is scratch
                CiLowerAddr a;
                err = ci_lower_bitfield_addr(ci, ctx, lhs, &a);
                if(err) return err;
                err = ci_emit_load_bitfield(ci, ctx, lhs, a, cur, size);
                if(err) return err;
                CiLowerVal r;
                err = ci_lower_expr(ci, ctx, rhs, CI_NO_SLOT, &r);
                if(err) return err;
                CiOp* op;
                err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
                if(err) return err;
                *op = (CiOp){
                    .alu = {
                        .kind = opkind,
                        .slot = cur,
                        .src = cur,
                        .src2 = r.slot,
                        .op = ci_alu_op_for(e->kind),
                        .is_unsigned = ccqt_is_unsigned(e->type, ctx->char_is_unsigned),
                        .loc = e->loc,
                    }
                };
                err = ci_emit_store_bitfield(ctx, lhs, a, cur, size);
                if(err) return err;
                // the expression's value is the stored bits, re-read
                // truncated and extended
                err = ci_emit_load_bitfield(ci, ctx, lhs, a, cur, size);
                if(err) return err;
                ctx->temp = keep; // free rhs and address scratch; cur survives
                if(dest == CI_NO_SLOT){
                    out->slot = cur;
                    return 0;
                }
                out->slot = dest;
                err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
                if(err) return err;
                *op = (CiOp){
                    .copy = {
                        .kind = CI_OP_COPY,
                        .slot = dest,
                        .slot_size = size,
                        .src = cur,
                        .src_size = size,
                        .loc = e->loc,
                    }
                };
                return 0;
            }
            // Resolve the target: a frame slot operated on in place, or a
            // computed address loaded into a temp and stored back.
            uint32_t cur;
            CiLowerAddr a;
            _Bool is_mem;
            uint32_t vslot;
            CiOp* op;
            if(ci_frame_lvalue(lhs, &vslot)){
                is_mem = 0;
                cur = vslot;
            }
            else if(lhs->type.is_atomic){
                // rmw handles the hardware ops at <= 8 bytes; floats and
                // 128-bit ints go through a compare-exchange loop instead.
                _Bool op_is_float = opkind == CI_OP_FALU32 || opkind == CI_OP_FALU64;
                return ci_lower_atomic_compound_assign(ci, ctx, e, dest, out, size, is_ptr, elem_sz, op_unsigned, op_is_float, fop);
            }
            else {
                is_mem = 1;
                err = ci_alloc_slot(ctx, size, size, &cur);
                if(err) return err;
            }
            uint32_t keep = ctx->temp; // cur is established; the rest is scratch
            if(is_mem){
                err = ci_lower_addr(ci, ctx, lhs, 0, &a); // access
                if(err) return err;
                err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
                if(err) return err;
                *op = (CiOp){
                    .load = {
                        .kind = CI_OP_LOAD,
                        .slot = cur,
                        .slot_size = size,
                        .src = a.slot,
                        .offset = a.disp,
                        .loc = e->loc,
                    }
                };
            }
            CiFoldValue fold;
            _Bool use_imm = 0;
            if(opkind == CI_OP_ALU32 || opkind == CI_OP_ALU64){
                int fold_fail = ci_fold_expr(ci, ctx, rhs, &fold);
                if(fold_fail > 0) return fold_fail;
                use_imm = fold_fail == 0 && fold.sz == size;
                if(use_imm && is_ptr){
                    fold.bits[0] = ci_fold_offset(ctx, &fold, elem_sz);
                    op_unsigned = 1;
                }
            }
            CiLowerVal r = {0};
            if(!use_imm){
                err = ci_lower_expr(ci, ctx, rhs, CI_NO_SLOT, &r);
                if(err) return err;
            }
            if(is_ptr && !use_imm && elem_sz != 1){
                uint32_t esz, scaled;
                err = ci_alloc_slot(ctx, ctx->size_size, ctx->size_size, &esz);
                if(err) return err;
                err = ci_alloc_slot(ctx, ctx->size_size, ctx->size_size, &scaled);
                if(err) return err;
                err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
                if(err) return err;
                *op = (CiOp){
                    .constant = {
                        .kind = CI_OP_CONST,
                        .bt_kind = (uint32_t)ci_target(ci)->intptr_type,
                        .slot = esz,
                        .immsize = ctx->ptr_size,
                        .immediate = {elem_sz},
                        .loc = e->loc,
                    }
                };
                err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
                if(err) return err;
                *op = (CiOp){
                    .alu = {
                        .kind = ctx->ptr_size==8?CI_OP_ALU64:ctx->ptr_size==4?CI_OP_ALU32:CI_OP_ALU128,
                        .slot = scaled,
                        .src = r.slot,
                        .src2 = esz,
                        .op = CI_ALU_MUL,
                        .is_unsigned = op_unsigned,
                        .loc = e->loc,
                    }
                };
                r.slot = scaled;
                r.size = ctx->ptr_size;
                op_unsigned = 1;
            }
            err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
            if(err) return err;
            if(use_imm){
                *op = (CiOp){
                    .alu_imm = {
                        .kind = size == 4 ? CI_OP_ALU_IMM32 : CI_OP_ALU_IMM64,
                        .op = ci_alu_op_for(e->kind),
                        .is_unsigned = op_unsigned,
                        .slot = cur,
                        .src = cur,
                        .loc = e->loc,
                    }
                };
                memcpy(&op->alu_imm.immediate, fold.bits, sizeof op->alu_imm.immediate);
            }
            else if(opkind == CI_OP_ALU8 || opkind == CI_OP_ALU16 || opkind == CI_OP_ALU32 || opkind == CI_OP_ALU64 || opkind == CI_OP_ALU128){
                *op = (CiOp){
                    .alu = {
                        .kind = opkind,
                        .op = ci_alu_op_for(e->kind),
                        .is_unsigned = op_unsigned,
                        .slot = cur,
                        .src = cur,
                        .src2 = r.slot,
                        .loc = e->loc,
                    }
                };
            }
            else {
                *op = (CiOp){
                    .falu32 = {
                        .kind = opkind,
                        .op = fop,
                        .slot = cur,
                        .slot_size = size,
                        .src = cur,
                        .src2 = r.slot,
                        .loc = e->loc,
                    }
                };
            }
            if(is_mem){
                err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
                if(err) return err;
                *op = (CiOp){
                    .store = {
                        .kind = CI_OP_STORE,
                        .slot = a.slot,
                        .src = cur,
                        .src_size = size,
                        .offset = a.disp,
                        .loc = e->loc,
                    }
                };
            }
            ctx->temp = keep; // free rhs and address scratch; cur survives
            if(dest == CI_NO_SLOT){
                out->slot = cur;
                return 0;
            }
            out->slot = dest;
            err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
            if(err) return err;
            *op = (CiOp){
                .copy = {
                    .kind = CI_OP_COPY,
                    .slot = dest,
                    .slot_size = size,
                    .src = cur,
                    .src_size = size,
                    .loc = e->loc,
                }
            };
            return 0;
        }
        case CC_EXPR_PREINC:
        case CC_EXPR_PREDEC:
        case CC_EXPR_POSTINC:
        case CC_EXPR_POSTDEC:
            return ci_lower_incdec(ci, ctx, e, dest, out);
        case CC_EXPR_CALL:
            return ci_lower_call(ci, ctx, e, dest, out);
        case CC_EXPR_ADD:
        case CC_EXPR_SUB:
        case CC_EXPR_MUL:
        case CC_EXPR_DIV:
        case CC_EXPR_MOD:
        case CC_EXPR_BITAND:
        case CC_EXPR_BITOR:
        case CC_EXPR_BITXOR:
        case CC_EXPR_LSHIFT:
        case CC_EXPR_RSHIFT:{
            CcExpr* lhs = e->lhs;
            CcExpr* rhs = e->values[0];
            _Bool lhs_ptr = ccqt_kind(lhs->type) == CC_POINTER;
            _Bool rhs_ptr = ccqt_kind(rhs->type) == CC_POINTER;
            if((lhs_ptr || rhs_ptr) && (e->kind == CC_EXPR_ADD || e->kind == CC_EXPR_SUB)){
                CcExpr* pe = lhs_ptr? lhs : rhs;
                uint32_t elem_sz;
                err = cc_sizeof_as_uint(p, ccqt_as_ptr(pe->type)->pointee, e->loc, &elem_sz);
                if(err) return err;
                if(lhs_ptr && rhs_ptr){
                    if(!elem_sz)
                        elem_sz = 1; // zero-sized pointees keep the byte difference
                    err = ci_lower_dest(ctx, &dest, size);
                    if(err) return err;
                    out->slot = dest;
                    uint32_t temp = ctx->temp;
                    CiLowerVal l, r;
                    err = ci_lower_expr(ci, ctx, lhs, CI_NO_SLOT, &l);
                    if(err) return err;
                    err = ci_lower_expr(ci, ctx, rhs, CI_NO_SLOT, &r);
                    if(err) return err;
                    uint32_t diff = dest;
                    if(elem_sz != 1){
                        err = ci_alloc_slot(ctx, ctx->ptr_size, ctx->ptr_size, &diff);
                        if(err) return err;
                    }
                    CiOp* op;
                    err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
                    if(err) return err;
                    *op = (CiOp){
                        .alu = {
                            .kind = ctx->ptr_size==8?CI_OP_ALU64:ctx->ptr_size==4?CI_OP_ALU32:CI_OP_ALU128,
                            .slot = diff,
                            .src = l.slot,
                            .src2 = r.slot,
                            .op = CI_ALU_SUB,
                            .is_unsigned = 1,
                            .loc = e->loc,
                        }
                    };
                    if(elem_sz != 1){
                        uint32_t esz;
                        err = ci_alloc_slot(ctx, ctx->ptr_size, ctx->ptr_size, &esz);
                        if(err) return err;
                        err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
                        if(err) return err;
                        *op = (CiOp){
                            .constant = {
                                .kind = CI_OP_CONST,
                                .bt_kind = (uint32_t)ci_target(ci)->size_type,
                                .slot = esz,
                                .immsize = ctx->ptr_size,
                                .immediate = {elem_sz},
                                .loc = e->loc,
                            }
                        };
                        err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
                        if(err) return err;
                        *op = (CiOp){
                            .alu = {
                                .kind = ctx->ptr_size==8?CI_OP_ALU64:ctx->ptr_size==4?CI_OP_ALU32:CI_OP_ALU128,
                                .slot = dest,
                                .src = diff,
                                .src2 = esz,
                                .op = CI_ALU_DIV,
                                .is_unsigned = 0,
                                .loc = e->loc,
                            }
                        };
                    }
                    ctx->temp = temp;
                    return 0;
                }
                // ptr ± int: the index extends per its own signedness
                CcExpr* ie = lhs_ptr? rhs : lhs;
                uint32_t isz;
                err = cc_sizeof_as_uint(p, ie->type, ie->loc, &isz);
                if(err) return err;
                if(!ccqt_is_integer(ie->type)) return ci_ice(ci, ie->loc, "Non integer index%s", "");
                if(isz != ctx->size_size) return ci_ice(ci, ie->loc, "Non index-sized index: %u", isz);
                _Bool idx_unsigned = ccqt_is_unsigned(ie->type, ctx->char_is_unsigned);
                err = ci_lower_dest(ctx, &dest, size);
                if(err) return err;
                out->slot = dest;
                uint32_t temp = ctx->temp;
                if(ctx->ptr_size == 4 || ctx->ptr_size == 8){
                    CiFoldValue index;
                    int fold_fail = ci_fold_expr(ci, ctx, ie, &index);
                    if(fold_fail > 0) return fold_fail;
                    if(fold_fail == 0){
                        CiLowerVal ptr;
                        err = ci_lower_expr(ci, ctx, pe, CI_NO_SLOT, &ptr);
                        if(err) return err;
                        CiOp* op;
                        err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
                        if(err) return err;
                        *op = (CiOp){
                            .alu_imm = {
                                .kind = ctx->ptr_size == 4 ? CI_OP_ALU_IMM32 : CI_OP_ALU_IMM64,
                                .op = e->kind == CC_EXPR_ADD ? CI_ALU_ADD : CI_ALU_SUB,
                                .is_unsigned = 1,
                                .slot = dest,
                                .src = ptr.slot,
                                .immediate = ci_fold_offset(ctx, &index, elem_sz),
                                .loc = e->loc,
                            }
                        };
                        ctx->temp = temp;
                        return 0;
                    }
                }
                CiLowerVal l, r;
                err = ci_lower_expr(ci, ctx, lhs, CI_NO_SLOT, &l);
                if(err) return err;
                err = ci_lower_expr(ci, ctx, rhs, CI_NO_SLOT, &r);
                if(err) return err;
                CiLowerVal* pv = lhs_ptr? &l : &r;
                CiLowerVal* iv = lhs_ptr? &r : &l;
                CiOp* op;
                if(elem_sz != 1){
                    uint32_t esz, scaled;
                    err = ci_alloc_slot(ctx, ctx->ptr_size, ctx->ptr_size, &esz);
                    if(err) return err;
                    err = ci_alloc_slot(ctx, ctx->ptr_size, ctx->ptr_size, &scaled);
                    if(err) return err;
                    err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
                    if(err) return err;
                    *op = (CiOp){
                        .constant = {
                            .kind = CI_OP_CONST,
                            .bt_kind = (uint32_t)ci_target(ci)->intptr_type,
                            .slot = esz,
                            .immsize = ctx->ptr_size,
                            .immediate = {elem_sz},
                            .loc = e->loc,
                        }
                    };
                    err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
                    if(err) return err;
                    *op = (CiOp){
                        .alu = {
                            .kind = ctx->ptr_size==8?CI_OP_ALU64:ctx->ptr_size==4?CI_OP_ALU32:CI_OP_ALU128,
                            .slot = scaled,
                            .src = iv->slot,
                            .src2 = esz,
                            .op = CI_ALU_MUL,
                            .is_unsigned = idx_unsigned,
                            .loc = e->loc,
                        }
                    };
                    iv->slot = scaled;
                    iv->size = ctx->ptr_size;
                    idx_unsigned = 1;
                }
                err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
                if(err) return err;
                *op = (CiOp){
                    .alu = {
                        .kind = ctx->ptr_size==8?CI_OP_ALU64:ctx->ptr_size==4?CI_OP_ALU32:CI_OP_ALU128,
                        .slot = dest,
                        .src = pv->slot,
                        .src2 = iv->slot,
                        .op = e->kind == CC_EXPR_ADD? CI_ALU_ADD : CI_ALU_SUB,
                        .is_unsigned = idx_unsigned,
                        .loc = e->loc,
                    }
                };
                ctx->temp = temp;
                return 0;
            }
            if(ccqt_is_integer(lhs->type) && ccqt_is_integer(rhs->type)){
                uint32_t lsz, rsz;
                err = cc_sizeof_as_uint(p, lhs->type, lhs->loc, &lsz);
                if(err) return err;
                err = cc_sizeof_as_uint(p, rhs->type, rhs->loc, &rsz);
                if(err) return err;
                _Bool is_shift = e->kind == CC_EXPR_LSHIFT || e->kind == CC_EXPR_RSHIFT;
                if(lsz != size || rsz != size){
                    if(!is_shift)
                        return ci_ice(ci, e->loc, "binary operands were not converted to result size%s", "");
                    if(lsz != size)
                        return ci_ice(ci, e->loc, "shift lhs was not converted to result size%s", "");
                }
                CiOpKind kind = ci_int_op_kind(size);
                if(!kind)
                    return ci_ice(ci, e->loc, "integer binary op has unsupported result size %u", size);
                err = ci_lower_dest(ctx, &dest, size);
                if(err) return err;
                out->slot = dest;
                uint32_t temp = ctx->temp;
                CiLowerVal l, r;
                err = ci_lower_expr(ci, ctx, lhs, CI_NO_SLOT, &l);
                if(err) return err;
                CiFoldValue fold;
                int fold_fail = -1;
                if((size == 4 || size == 8) && rsz == size){
                    fold_fail = ci_fold_expr(ci, ctx, rhs, &fold);
                    if(fold_fail > 0) return fold_fail;
                }
                if(fold_fail == 0){
                    CiOp* op;
                    err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
                    if(err) return err;
                    *op = (CiOp){
                        .alu_imm = {
                            .kind = size == 4 ? CI_OP_ALU_IMM32 : CI_OP_ALU_IMM64,
                            .op = ci_alu_op_for(e->kind),
                            .is_unsigned = ccqt_is_unsigned(lhs->type, ctx->char_is_unsigned),
                            .slot = dest,
                            .src = l.slot,
                            .loc = e->loc,
                        }
                    };
                    memcpy(&op->alu_imm.immediate, fold.bits, sizeof op->alu_imm.immediate);
                    ctx->temp = temp;
                    return 0;
                }
                err = ci_lower_expr(ci, ctx, rhs, CI_NO_SLOT, &r);
                if(err) return err;
                CiOp* op;
                if(is_shift && r.size != size){
                    uint32_t conv;
                    err = ci_alloc_slot(ctx, size, size, &conv);
                    if(err) return err;
                    err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
                    if(err) return err;
                    *op = (CiOp){
                        .convert = {
                            .kind = CI_OP_CONVERT,
                            .slot = conv,
                            .slot_size = size,
                            .src = r.slot,
                            .src_size = r.size,
                            .is_unsigned = ccqt_is_unsigned(rhs->type, ctx->char_is_unsigned),
                            .loc = rhs->loc,
                        }
                    };
                    r.slot = conv;
                    r.size = size;
                }
                err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
                if(err) return err;
                *op = (CiOp){
                    .alu = {
                        .kind = kind,
                        .op = ci_alu_op_for(e->kind),
                        .is_unsigned = ccqt_is_unsigned(lhs->type, ctx->char_is_unsigned),
                        .slot = dest,
                        .src = l.slot,
                        .src2 = r.slot,
                        .loc = e->loc,
                    }
                };
                ctx->temp = temp;
                return 0;
            }
            if(ci_falu_type(lhs->type) && ci_falu_type(rhs->type) && lhs->type.basic.kind == rhs->type.basic.kind){
                CiFaluOp fop;
                if(!ci_falu_op_for(e->kind, &fop))
                    return ci_ice(ci, e->loc, "float-only invalid binary operator reached lowering%s", "");
                CiOpKind kind = lhs->type.basic.kind == CCBT_float? CI_OP_FALU32 : CI_OP_FALU64;
                err = ci_lower_dest(ctx, &dest, size);
                if(err) return err;
                out->slot = dest;
                uint32_t temp = ctx->temp;
                CiLowerVal l, r;
                err = ci_lower_expr(ci, ctx, lhs, CI_NO_SLOT, &l);
                if(err) return err;
                err = ci_lower_expr(ci, ctx, rhs, CI_NO_SLOT, &r);
                if(err) return err;
                CiOp* op;
                err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
                if(err) return err;
                *op = (CiOp){
                    .falu32 = {
                        .kind = kind,
                        .op = fop,
                        .slot = dest,
                        .slot_size = size,
                        .src = l.slot,
                        .src2 = r.slot,
                        .loc = e->loc,
                    }
                };
                ctx->temp = temp;
                return 0;
            }
            if(ccqt_is_basic(lhs->type) && ccbt_is_arithmetic(lhs->type.basic.kind) && ccqt_is_basic(rhs->type) && ccbt_is_arithmetic(rhs->type.basic.kind)){
                if(lhs->type.basic.kind != rhs->type.basic.kind)
                    return ci_ice(ci, e->loc, "arithmetic operands were not converted to a common type%s", "");
                return ci_unimplemented(ci, e->loc, "unsupported arithmetic binary op");
            }
            return ci_ice(ci, e->loc, "illegal binary expression reached lowering%s", "");
        }
        case CC_EXPR_EQ:
        case CC_EXPR_NE:
        case CC_EXPR_LT:
        case CC_EXPR_GT:
        case CC_EXPR_LE:
        case CC_EXPR_GE:{
            CcExpr* lhs = e->lhs;
            CcExpr* rhs = e->values[0];
            _Bool lhs_ptr = ccqt_kind(lhs->type) == CC_POINTER || ccqt_kind(lhs->type) == CC_BLOCK_POINTER || ccqt_bt_eq(lhs->type, CCBT_nullptr_t);
            _Bool rhs_ptr = ccqt_kind(rhs->type) == CC_POINTER || ccqt_kind(rhs->type) == CC_BLOCK_POINTER || ccqt_bt_eq(rhs->type, CCBT_nullptr_t);
            CiOpKind kind;
            CiCmpOp cmp_op = ci_cmp_op_for(e->kind);
            _Bool is_unsigned = 0;
            uint32_t op_size;
            _Bool is_float = 0;
            if(ci_falu_type(lhs->type) && ci_falu_type(rhs->type) && lhs->type.basic.kind == rhs->type.basic.kind){
                is_float = 1;
                op_size = lhs->type.basic.kind == CCBT_float? 4 : 8;
                kind = op_size == 4? CI_OP_FCMP32 : CI_OP_FCMP64;
            }
            else if(ccqt_is_integer(lhs->type) && ccqt_is_integer(rhs->type)){
                uint32_t lsz, rsz;
                err = cc_sizeof_as_uint(p, lhs->type, lhs->loc, &lsz);
                if(err) return err;
                err = cc_sizeof_as_uint(p, rhs->type, rhs->loc, &rsz);
                if(err) return err;
                op_size = lsz > rsz? lsz : rsz;
                kind = ci_cmp_op_kind(op_size);
                if(!kind)
                    return ci_ice(ci, e->loc, "integer comparison operand size was not promoted%s", "");
                is_unsigned = ccqt_is_unsigned(lhs->type, ctx->char_is_unsigned);
            }
            else if(ccqt_bt_eq(lhs->type, CCBT__Type) && ccqt_bt_eq(rhs->type, CCBT__Type)){
                if(e->kind != CC_EXPR_EQ && e->kind != CC_EXPR_NE)
                    return ci_ice(ci, e->loc, "ordered comparison of _Type reached lowering%s", "");
                uint32_t lsz, rsz;
                err = cc_sizeof_as_uint(p, lhs->type, lhs->loc, &lsz);
                if(err) return err;
                err = cc_sizeof_as_uint(p, rhs->type, rhs->loc, &rsz);
                if(err) return err;
                if(lsz != rsz)
                    return ci_ice(ci, e->loc, "_Type operands have different sizes%s", "");
                op_size = lsz;
                kind = ci_cmp_op_kind(op_size);
                if(!kind)
                    return ci_ice(ci, e->loc, "_Type comparison operand size is unsupported %u", op_size);
                is_unsigned = 1;
            }
            else if((lhs_ptr || rhs_ptr) && (lhs_ptr || ccqt_is_integer(lhs->type)) && (rhs_ptr || ccqt_is_integer(rhs->type))){
                uint32_t lsz, rsz;
                err = cc_sizeof_as_uint(p, lhs->type, lhs->loc, &lsz);
                if(err) return err;
                err = cc_sizeof_as_uint(p, rhs->type, rhs->loc, &rsz);
                if(err) return err;
                op_size = lsz > rsz? lsz : rsz;
                kind = ci_cmp_op_kind(op_size);
                if(!kind)
                    return ci_ice(ci, e->loc, "pointer comparison operand size was not promoted%s", "");
                is_unsigned = 1;
            }
            else if(ccqt_is_basic(lhs->type) && ccbt_is_arithmetic(lhs->type.basic.kind)
                 && ccqt_is_basic(rhs->type) && ccbt_is_arithmetic(rhs->type.basic.kind)){
                if(lhs->type.basic.kind != rhs->type.basic.kind)
                    return ci_ice(ci, e->loc, "comparison operands were not converted to a common type%s", "");
                return ci_unimplemented(ci, e->loc, "unsupported arithmetic comparison op");
            }
            else {
                return ci_ice(ci, e->loc, "illegal comparison expression reached lowering%s", "");
            }
            err = ci_lower_dest(ctx, &dest, size);
            if(err) return err;
            out->slot = dest;
            out->canonical = 1;
            uint32_t temp = ctx->temp;
            CiLowerVal l, r;
            err = ci_lower_expr(ci, ctx, lhs, CI_NO_SLOT, &l);
            if(err) return err;
            err = ci_lower_expr(ci, ctx, rhs, CI_NO_SLOT, &r);
            if(err) return err;
            if(l.size != op_size)
                return ci_ice(ci, e->loc, "lhs not lowered to common size: lhs: %u, op_size: %u", l.size, op_size);
            if(r.size != op_size)
                return ci_ice(ci, e->loc, "rhs not lowered to common size: rhs: %u, op_size: %u", l.size, op_size);
            CiOp* op;
            if(is_float){
                err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
                if(err) return err;
                *op = (CiOp){
                    .fcmp32 = {
                        .kind = kind,
                        .op = cmp_op,
                        .slot = dest,
                        .slot_size = size,
                        .src = l.slot,
                        .src2 = r.slot,
                        .loc = e->loc,
                    }
                };
                ctx->temp = temp;
                return 0;
            }
            err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
            if(err) return err;
            *op = (CiOp){
                .cmp = {
                    .kind = kind,
                    .op = cmp_op,
                    .is_unsigned = is_unsigned,
                    .slot = dest,
                    .slot_size = size,
                    .src = l.slot,
                    .src2 = r.slot,
                    .loc = e->loc,
                }
            };
            ctx->temp = temp;
            return 0;
        }
        case CC_EXPR_LOGAND:
        case CC_EXPR_LOGOR:{
            uint32_t work;
            if(dest == CI_NO_SLOT){
                err = ci_lower_dest(ctx, &dest, size);
                if(err) return err;
                work = dest;
            }
            else {
                err = ci_alloc_slot(ctx, size, size, &work);
                if(err) return err;
            }
            out->slot = dest;
            CcExpr* lhs = e->lhs;
            CcExpr* rhs = e->values[0];
            uint32_t temp = ctx->temp;
            CiLowerVal v;
            err = ci_lower_expr(ci, ctx, lhs, CI_NO_SLOT, &v);
            if(err) return err;
            err = ci_lower_istrue(ctx, &v, lhs->type, work, size, 0, lhs->loc);
            if(err) return err;
            ctx->temp = temp;
            CiOp* op;
            err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
            if(err) return err;
            *op = (CiOp){
                .jump_false = {
                    .kind = e->kind == CC_EXPR_LOGAND? CI_OP_JUMP_FALSE : CI_OP_JUMP_TRUE,
                    .slot = work,
                    .slot_size = size,
                    .loc = e->loc,
                }
            };
            ptrdiff_t jump = (char*)&op->jump_false.jump - (char*)ctx->out->data;
            err = ci_lower_expr(ci, ctx, rhs, CI_NO_SLOT, &v);
            if(err) return err;
            err = ci_lower_istrue(ctx, &v, rhs->type, work, size, 0, rhs->loc);
            if(err) return err;
            ctx->temp = temp;
            *(uint32_t*)((char*)ctx->out->data+jump) = (uint32_t)ctx->out->count;
            if(work != dest){
                err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
                if(err) return err;
                *op = (CiOp){
                    .copy = {
                        .kind = CI_OP_COPY,
                        .slot = dest,
                        .slot_size = size,
                        .src = work,
                        .src_size = size,
                        .loc = e->loc,
                    }
                };
            }
            out->canonical = 1;
            return 0;
        }
        case CC_EXPR_TERNARY:{
            _Bool truth;
            int folded = ci_fold_condition(ci, ctx, e->lhs, &truth);
            if(folded > 0) return folded;
            if(folded == 0)
                return ci_lower_expr(ci, ctx, e->values[truth ? 0 : 1], dest, out);
            // cond; if(!cond) goto else; dest = then; goto end; else: dest = else; end:
            err = ci_lower_dest(ctx, &dest, size);
            if(err) return err;
            out->slot = dest;
            uint32_t temp = ctx->temp;
            CiLowerVal v;
            CiOp* op;
            uint32_t chain = 0;
            err = ci_lower_branch(ci, ctx, e->lhs, 0, e->loc, &chain);
            if(err) return err;
            ctx->temp = temp;
            err = ci_lower_expr(ci, ctx, e->values[0], dest, &v);
            if(err) return err;
            ctx->temp = temp;
            err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
            if(err) return err;
            *op = (CiOp){
                .jump = {
                    .kind = CI_OP_JUMP,
                    .loc = e->loc,
                }
            };
            ci_patch_branches(ctx, chain, (uint32_t)ctx->out->count);
            ptrdiff_t jump = (char*)&op->jump.jump - (char*)ctx->out->data;
            err = ci_lower_expr(ci, ctx, e->values[1], dest, &v);
            if(err) return err;
            ctx->temp = temp;
            *(uint32_t*)((char*)ctx->out->data+jump) = (uint32_t)ctx->out->count;
            return 0;
        }
        case CC_EXPR_COMMA:{
            err = ci_lower_expr_discard(ci, ctx, e->lhs);
            if(err) return err;
            return ci_lower_expr(ci, ctx, e->values[0], dest, out);
        }
        case CC_EXPR_STATEMENT_EXPRESSION:{
            CcStmtNode* body = e->stmt_body;
            uint32_t count = body->count;
            CcExpr*_Nullable value = NULL;
            if(count && body->stmts[count-1]->kind == CC_STMT_EXPR
                && !ccqt_bt_eq(e->type, CCBT_void)){
                value = body->stmts[count-1]->exprs[0];
                count--;
            }
            for(uint32_t i = 0; i < count; i++){
                err = ci_lower_stmt(ci, ctx, body->stmts[i]);
                if(err) return err;
            }
            if(value)
                return ci_lower_expr(ci, ctx, (CcExpr*_Nonnull)value, dest, out);
            err = ci_lower_dest(ctx, &dest, size);
            if(err) return err;
            out->slot = dest;
            return 0;
        }
        case CC_EXPR_ATOMIC:
            return ci_lower_atomic_builtin(ci, ctx, e, dest, out);
        case CC_EXPR_COMPOUND_LITERAL:
        case CC_EXPR_INIT_LIST:{
            // Parser ensures that we are either initializing or going through an anonymous
            // compound literal, so it is safe to build in-place.
            err = ci_lower_dest(ctx, &dest, size);
            if(err) return err;
            out->slot = dest;
            uint32_t temp = ctx->temp;
            uint32_t aslot;
            err = ci_alloc_slot(ctx, 8, 8, &aslot);
            if(err) return err;
            CiOp* op;
            err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
            if(err) return err;
            *op = (CiOp){
                .slot_addr = {
                    .kind = CI_OP_SLOT_ADDR,
                    .slot = aslot,
                    .slot_size = 8,
                    .src = dest,
                    .loc = e->loc,
                }
            };
            err = ci_lower_init_list(ci, ctx, e, (CiLowerAddr){.slot = aslot, .disp = 0}, 1);
            if(err) return err;
            ctx->temp = temp;
            return 0;
        }
        case CC_EXPR_SIZEOF_VMT:
            return ci_unimplemented(ci, e->loc, "sizeof vmt");
        case CC_EXPR_FUNCTION:
            return ci_unreachable(ci, e->loc, "function as value?");
        case CC_EXPR_VA:
            return ci_lower_va(ci, ctx, e, dest, out);
        case CC_EXPR_ADD_OVERFLOW:
        case CC_EXPR_MUL_OVERFLOW:
        case CC_EXPR_SUB_OVERFLOW:
            return ci_lower_checked(ci, ctx, e, dest, out);
        case CC_EXPR_POPCOUNT:
        case CC_EXPR_CLZ:
        case CC_EXPR_CTZ:
            return ci_lower_bitcount(ci, ctx, e, dest, out);
        case CC_EXPR_BSWAP:{
            CcExpr* arg = e->lhs;
            err = ci_lower_dest(ctx, &dest, size);
            if(err) return err;
            out->slot = dest;
            uint32_t temp = ctx->temp;
            CiLowerVal l;
            err = ci_lower_expr(ci, ctx, arg, CI_NO_SLOT, &l);
            if(err) return err;
            CiOp* op;
            err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
            if(err) return err;
            *op = (CiOp){
                .bswap = {
                    .kind = CI_OP_BSWAP,
                    .size = size,
                    .src = l.slot,
                    .slot = dest,
                    .loc = e->loc,
                },
            };
            ctx->temp = temp;
            return 0;
        }
        case CC_EXPR_ALLOCA:{
            CcExpr *sz = e->lhs;
            err = ci_lower_dest(ctx, &dest, size);
            if(err) return err;
            out->slot = dest;
            uint32_t temp = ctx->temp;
            CiLowerVal l;
            err = ci_lower_expr(ci, ctx, sz, CI_NO_SLOT, &l);
            if(err) return err;
            CiOp* op;
            err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
            if(err) return err;
            *op = (CiOp){
                .alloca = {
                    .kind = CI_OP_ALLOCA,
                    .src = l.slot,
                    .slot = dest,
                },
            };
            ctx->temp = temp;
            return 0;
        }
        case CC_EXPR_BUILTIN:{
            CiOp* op;
            err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
            if(err) return err;
            *op = (CiOp){
                .builtin = {
                    .kind = CI_OP_BUILTIN,
                    .op = e->builtin.op,
                },
            };
            return 0;
        }
        case CC_EXPR_SLICE_ALL:
        case CC_EXPR_SLICE:
        case CC_EXPR_SLICE_LO:
        case CC_EXPR_SLICE_HI:
            return ci_lower_slice(ci, ctx, e, dest, out);
        case CC_EXPR_INTERN:
            return ci_lower_rt_call(ci, ctx, e, CI_RT_INTERN, dest, out);
        case CC_EXPR_HOTSWAP:
            return ci_lower_rt_call(ci, ctx, e, CI_RT_HOTSWAP, dest, out);
        case CC_EXPR_COMPILE:
            return ci_lower_rt_call(ci, ctx, e, CI_RT_COMPILE, dest, out);
        case CC_EXPR_MODULE_REFLECT:
        case CC_EXPR_TYPE_INTROSPECTION:
            return ci_lower_reflect(ci, ctx, e, dest, out);
        case CC_EXPR_UMUL128:
            return ci_lower_umul128(ci, ctx, e, dest, out);
        DRP_CASES_EXHAUSTED;
    }
    return ci_unreachable(ci, e->loc, "unhandled expression kind");
}

static
int
ci_lower_cast_operand(CiInterpreter* ci, CiLowerCtx* ctx, CcExpr* e, uint32_t dest, CiLowerVal* out){
    if(ccqt_kind(e->type) == CC_ARRAY){
        CiLowerAddr a;
        int err = ci_lower_addr(ci, ctx, e, 1, &a);
        if(err) return err;
        return ci_addr_to_value(ctx, a, dest, ctx->ptr_size, e->loc, out);
    }
    if(ccqt_kind(e->type) == CC_FUNCTION){
        if(e->kind == CC_EXPR_DEREF)
            return ci_lower_expr(ci, ctx, e->lhs, dest, out);
        if(e->kind != CC_EXPR_FUNCTION)
            return ci_ice(ci, e->loc, "unexpected function cast operand%s", "");
        int err = ci_lower_dest(ctx, &dest, ctx->ptr_size);
        if(err) return err;
        *out = (CiLowerVal){.slot = dest, .size = ctx->ptr_size};
        CiOp* op;
        err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
        if(err) return err;
        *op = (CiOp){
            .func_addr = {
                .kind = CI_OP_FUNC_ADDR,
                .slot = dest,
                .slot_size = ctx->ptr_size,
                .func = e->func,
                .loc = e->loc,
            }
        };
        return 0;
    }
    return ci_lower_expr(ci, ctx, e, dest, out);
}

static
int
ci_lower_reflect(CiInterpreter* ci, CiLowerCtx* ctx, CcExpr* e, uint32_t dest, CiLowerVal*_Nullable out){
    _Bool module = e->kind == CC_EXPR_MODULE_REFLECT;
    uint32_t subop = module ? (uint32_t)e->module.op : (uint32_t)e->type_introspection.op;
    if(module && !out && subop != CC_MODULE_RUN) return 0;
    int nargs = 1;
    if(module ?
        (  subop == CC_MODULE_FUNC
        || subop == CC_MODULE_VAR
        || subop == CC_MODULE_TYPE
        || subop == CC_MODULE_SYMBOL
        || subop == CC_MODULE_PARSE_TYPE)
        : (subop == CC_TYPE_IS_CALLABLE_WITH
        || subop == CC_TYPE_CASTABLE_TO
        || subop == CC_TYPE_FIELD
        || subop == CC_TYPE_ENUMERATOR
        || subop == CC_TYPE_PARAM_TYPE
        || subop == CC_TYPE_MAKE_ANY))
        nargs++;
    int err;
    if(out){
        err = ci_lower_dest(ctx, &dest, out->size);
        if(err) return err;
        out->slot = dest;
    }
    uint32_t temp = ctx->temp;
    CiOp call = {.rt_call = {
        .kind = CI_OP_RT_CALL,
        .op = module ? CI_RT_MODULE_REFLECT : CI_RT_TYPE_REFLECT,
        .nargs = nargs,
        .slot = dest,
        .slot_size = out ? out->size : 0,
        .reflect_op = subop,
        .loc = e->loc,
    }};
    for(uint32_t i = 0; i < call.rt_call.nargs; i++){
        err = ci_alloc_slot(ctx, 8, 8, &call.rt_call.args[i]);
        if(err) return err;
        CiLowerVal v;
        err = ci_lower_expr(ci, ctx, i ? e->values[0] : e->lhs, call.rt_call.args[i], &v);
        if(err) return err;
        if(i == 0 && (module || subop == CC_TYPE_FIELD
            || subop == CC_TYPE_ENUMERATOR || subop == CC_TYPE_PARAM_TYPE)){
            // Receiver errors must precede evaluation of the optional operand.
            CiOp* check;
            err = ma_alloc(CiOp)(ctx->out, ctx->a, &check);
            if(err) return err;
            *check = call;
            check->rt_call.op = module ? CI_RT_MODULE_VALIDATE : CI_RT_TYPE_VALIDATE;
            check->rt_call.nargs = 1;
            check->rt_call.slot = CI_NO_SLOT;
            check->rt_call.slot_size = 0;
        }
    }
    if(module && subop == CC_MODULE_SYMBOL){
        err = ci_alloc_slot(ctx, 8, 8, &call.rt_call.args[2]);
        if(err) return err;
        CiOp* expected;
        err = ma_alloc(CiOp)(ctx->out, ctx->a, &expected);
        if(err) return err;
        *expected = (CiOp){.constant = {
            .kind = CI_OP_CONST,
            .slot = call.rt_call.args[2],
            .immsize = 8,
            .immediate = {ccqt_as_ptr(e->type)->pointee.bits},
            .loc = e->loc,
        }};
        call.rt_call.nargs = 3;
    }
    CiOp* op;
    err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
    if(err) return err;
    *op = call;
    ctx->temp = temp;
    return 0;
}

static
int
ci_lower_rt_call(CiInterpreter* ci, CiLowerCtx* ctx, CcExpr* e, CiRuntimeOp rt_op, uint32_t dest, CiLowerVal*_Nullable out){
    uint32_t nargs = rt_op == CI_RT_HOTSWAP ? 2 : 1;
    CcExpr* args[2] = {e->lhs, NULL};
    if(nargs == 2)
        args[1] = e->values[0];
    uint32_t temp = ctx->temp;
    uint32_t slots[3] = {0};
    for(uint32_t i = 0; i < nargs; i++){
        CiLowerVal v;
        int err = ci_lower_expr(ci, ctx, args[i], CI_NO_SLOT, &v);
        if(err) return err;
        slots[i] = v.slot;
    }
    uint32_t result_size = 0;
    if(out){
        int err = ci_lower_dest(ctx, &dest, out->size);
        if(err) return err;
        out->slot = dest;
        result_size = out->size;
    }
    CiOp* op;
    int err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
    if(err) return err;
    *op = (CiOp){
        .rt_call = {
            .kind = CI_OP_RT_CALL,
            .op = rt_op,
            .nargs = nargs,
            .slot = dest,
            .slot_size = result_size,
            .args = {slots[0], slots[1], slots[2]},
            .loc = e->loc,
        },
    };
    ctx->temp = temp;
    return 0;
}

static
int
ci_lower_incdec(CiInterpreter* ci, CiLowerCtx* ctx, CcExpr* e, uint32_t dest, CiLowerVal*_Nullable out){
    int err;
    CcParser* p = &ci->parser;
    CcExpr* lhs = e->lhs;
    uint32_t size;
    err = cc_sizeof_as_uint(p, e->type, e->loc, &size);
    if(err) return err;
    uint64_t step;
    uint32_t step_bt = (uint32_t)ci_target(ci)->size_type;
    uint32_t step_immsize = size;
    _Bool is_float = 0;
    if(ccqt_kind(e->type) == CC_POINTER){
        uint32_t pointee_sz;
        err = cc_sizeof_as_uint(p, ccqt_as_ptr(e->type)->pointee, e->loc, &pointee_sz);
        if(err) return err;
        step = pointee_sz;
    }
    else if(ccqt_is_integer(e->type)){
        step = 1;
    }
    else if(ci_falu_type(e->type)){
        is_float = 1;
        step_bt = (uint32_t)e->type.basic.kind;
        step_immsize = size;
        if(size == 4){
            float one = 1.0f;
            uint32_t bits;
            memcpy(&bits, &one, 4);
            step = bits;
        }
        else if(size == 8){
            double one = 1.0;
            memcpy(&step, &one, 8);
        }
        else {
            return ci_unimplemented(ci, e->loc, "incdec on unsupported falu type");
        }
    }
    else {
        return ci_unimplemented(ci, e->loc, "incdec on unsupported type");
    }
    _Bool is_pre = e->kind == CC_EXPR_PREINC || e->kind == CC_EXPR_PREDEC;
    _Bool is_inc = e->kind == CC_EXPR_PREINC || e->kind == CC_EXPR_POSTINC;
    CiOpKind alukind;
    CiFaluOp fop = 0;
    if(is_float){
        alukind = size == 4? CI_OP_FALU32 : CI_OP_FALU64;
        fop = is_inc? CI_FALU_ADD : CI_FALU_SUB;
    }
    else {
        alukind = ci_int_op_kind(size);
        if(!alukind) return ci_ice(ci, e->loc, "unsupported integer ALU size %u", size);
    }
    CiOp* op;
    uint32_t vslot;
    if(ci_frame_lvalue(lhs, &vslot)){
        if(out && !is_pre){
            // the post forms yield the old value, captured before modifying
            err = ci_lower_dest(ctx, &dest, size);
            if(err) return err;
            err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
            if(err) return err;
            *op = (CiOp){
                .copy = {
                    .kind = CI_OP_COPY,
                    .slot = dest,
                    .slot_size = size,
                    .src = vslot,
                    .src_size = size,
                    .loc = e->loc,
                }
            };
            out->slot = dest;
        }
        uint32_t temp = ctx->temp;
        if(!is_float && (size == 4 || size == 8)){
            err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
            if(err) return err;
            *op = (CiOp){
                .alu_imm = {
                    .kind = size == 4 ? CI_OP_ALU_IMM32 : CI_OP_ALU_IMM64,
                    .slot = vslot,
                    .src = vslot,
                    .op = is_inc ? CI_ALU_ADD : CI_ALU_SUB,
                    .is_unsigned = 1,
                    .immediate = step,
                    .loc = e->loc,
                }
            };
        }
        else {
            uint32_t sslot;
            err = ci_alloc_slot(ctx, step_immsize, step_immsize, &sslot);
            if(err) return err;
            err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
            if(err) return err;
            *op = (CiOp){
                .constant = {
                    .kind = CI_OP_CONST,
                    .bt_kind = step_bt,
                    .slot = sslot,
                    .immsize = step_immsize,
                    .immediate = {step},
                    .loc = e->loc,
                }
            };
            err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
            if(err) return err;
            if(is_float){
                *op = (CiOp){
                    .falu32 = {
                        .kind = alukind,
                        .op = fop,
                        .slot = vslot,
                        .slot_size = size,
                        .src = vslot,
                        .src2 = sslot,
                        .loc = e->loc,
                    }
                };
            }
            else {
                *op = (CiOp){
                    .alu = {
                        .kind = alukind,
                        .slot = vslot,
                        .src = vslot,
                        .src2 = sslot,
                        .op = is_inc?CI_ALU_ADD:CI_ALU_SUB,
                        .is_unsigned = 1,
                        .loc = e->loc,
                    }
                };
            }
        }
        ctx->temp = temp;
        if(out && is_pre){
            if(dest == CI_NO_SLOT){
                out->slot = vslot;
            }
            else {
                err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
                if(err) return err;
                *op = (CiOp){
                    .copy = {
                        .kind = CI_OP_COPY,
                        .slot = dest,
                        .slot_size = size,
                        .src = vslot,
                        .src_size = size,
                        .loc = e->loc,
                    }
                };
                out->slot = dest;
            }
        }
        return 0;
    }
    if(lhs->type.is_atomic){
        uint32_t save = ctx->temp;
        uint32_t old, sslot;
        err = ci_alloc_slot(ctx, size, size, &old);
        if(err) return err;
        err = ci_alloc_slot(ctx, size, size, &sslot);
        if(err) return err;
        CiLowerAddr a;
        err = ci_lower_addr(ci, ctx, lhs, 0, &a); // access
        if(err) return err;
        err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
        if(err) return err;
        *op = (CiOp){
            .constant = {
                .kind = CI_OP_CONST,
                .bt_kind = step_bt,
                .slot = sslot,
                .immsize = size,
                .immediate = {step},
                .loc = e->loc,
            }
        };
        // result holds the expression's value: the new value for pre forms,
        // the loaded old value for post forms.
        uint32_t result;
        if(is_float || size > 8){
            // No hardware atomic FADD, and rmw tops out at 8 bytes, so
            // compare-exchange loop: load, recompute the new value (as a float
            // or 128-bit int), then cas; a failed cas reloads old and retries.
            uint32_t newv, ok;
            err = ci_alloc_slot(ctx, size, size, &newv);
            if(err) return err;
            err = ci_alloc_slot(ctx, 1, 1, &ok);
            if(err) return err;
            err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
            if(err) return err;
            *op = (CiOp){
                .atomic_load = {
                    .kind = CI_OP_ATOMIC_LOAD,
                    .memorder = CC_MO_SEQ_CST,
                    .slot = old,
                    .slot_size = size,
                    .src = a.slot,
                    .offset = a.disp,
                    .loc = e->loc,
                }
            };
            uint32_t loop = (uint32_t)ctx->out->count;
            err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
            if(err) return err;
            if(is_float){
                *op = (CiOp){
                    .falu32 = {
                        .kind = alukind,
                        .op = fop,
                        .slot = newv,
                        .slot_size = size,
                        .src = old,
                        .src2 = sslot,
                        .loc = e->loc,
                    }
                };
            }
            else {
                *op = (CiOp){
                    .alu = {
                        .kind = alukind, // CI_OP_ALU128
                        .op = is_inc? CI_ALU_ADD : CI_ALU_SUB,
                        .is_unsigned = 1,
                        .slot = newv,
                        .src = old,
                        .src2 = sslot,
                        .loc = e->loc,
                    }
                };
            }
            err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
            if(err) return err;
            *op = (CiOp){
                .atomic_cas = {
                    .kind = CI_OP_ATOMIC_CAS,
                    .memorder = CC_MO_SEQ_CST,
                    .fail_memorder = CC_MO_SEQ_CST,
                    .weak = 1,
                    .size = size,
                    .slot = ok,
                    .src = a.slot,
                    .expected = old,
                    .desired = newv,
                    .offset = a.disp,
                    .loc = e->loc,
                }
            };
            err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
            if(err) return err;
            *op = (CiOp){
                .jump_false = {
                    .kind = CI_OP_JUMP_FALSE,
                    .slot = ok,
                    .slot_size = 1,
                    .jump = loop,
                    .loc = e->loc,
                }
            };
            result = is_pre? newv : old;
        }
        else {
            // one atomic fetch-add/sub; pre forms recompute the new value from
            // the returned old one
            err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
            if(err) return err;
            *op = (CiOp){
                .atomic_rmw = {
                    .kind = CI_OP_ATOMIC_RMW,
                    .op = is_inc? CI_ARMW_ADD : CI_ARMW_SUB,
                    .memorder = CC_MO_SEQ_CST,
                    .discard = !out,
                    .slot = old,
                    .slot_size = size,
                    .src = a.slot,
                    .src2 = sslot,
                    .offset = a.disp,
                    .loc = e->loc,
                }
            };
            if(out && is_pre){
                err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
                if(err) return err;
                *op = (CiOp){
                    .alu = {
                        .kind = alukind,
                        .slot = old,
                        .src = old,
                        .src2 = sslot,
                        .op = is_inc? CI_ALU_ADD : CI_ALU_SUB,
                        .is_unsigned = 1,
                        .loc = e->loc,
                    }
                };
            }
            result = old;
        }
        if(!out){
            ctx->temp = save;
            return 0;
        }
        if(dest == CI_NO_SLOT){
            out->slot = result;
            return 0;
        }
        out->slot = dest;
        err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
        if(err) return err;
        *op = (CiOp){
            .copy = {
                .kind = CI_OP_COPY,
                .slot = dest,
                .slot_size = size,
                .src = result,
                .src_size = size,
                .loc = e->loc,
            }
        };
        return 0;
    }
    // Memory path: load-modify-store through a computed address, evaluated
    // once. old holds the loaded value, new the incremented value.
    uint32_t save = ctx->temp;
    uint32_t old;
    err = ci_alloc_slot(ctx, size, size, &old);
    if(err) return err;
    CiLowerAddr a;
    _Bool is_bf = (lhs->kind == CC_EXPR_DOT || lhs->kind == CC_EXPR_ARROW)
        && lhs->field_loc.bit_width;
    if(is_bf && size > 8){
        ctx->temp = save;
        return ci_unimplemented(ci, e->loc, "incdec on a 128-bit bitfield");
    }
    if(is_bf)
        err = ci_lower_bitfield_addr(ci, ctx, lhs, &a);
    else
        err = ci_lower_addr(ci, ctx, lhs, 0, &a); // access
    if(err) return err;
    if(is_bf){
        err = ci_emit_load_bitfield(ci, ctx, lhs, a, old, size);
        if(err) return err;
    }
    else {
        err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
        if(err) return err;
        *op = (CiOp){
            .load = {
                .kind = CI_OP_LOAD,
                .slot = old,
                .slot_size = size,
                .src = a.slot,
                .offset = a.disp,
                .loc = e->loc,
            }
        };
    }
    uint32_t newv;
    err = ci_alloc_slot(ctx, size, size, &newv);
    if(err) return err;
    uint32_t sslot;
    err = ci_alloc_slot(ctx, step_immsize, step_immsize, &sslot);
    if(err) return err;
    err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
    if(err) return err;
    *op = (CiOp){
        .constant = {
            .kind = CI_OP_CONST,
            .bt_kind = step_bt,
            .slot = sslot,
            .immsize = step_immsize,
            .immediate = {step},
            .loc = e->loc,
        }
    };
    err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
    if(err) return err;
    if(is_float){
        *op = (CiOp){
            .falu32 = {
                .kind = alukind,
                .op = fop,
                .slot = newv,
                .slot_size = size,
                .src = old,
                .src2 = sslot,
                .loc = e->loc,
            }
        };
    }
    else {
        *op = (CiOp){
            .alu = {
                .kind = alukind,
                .slot = newv,
                .src = old,
                .src2 = sslot,
                .op = is_inc?CI_ALU_ADD:CI_ALU_SUB,
                .is_unsigned = 1,
                .loc = e->loc,
            }
        };
    }
    if(is_bf){
        err = ci_emit_store_bitfield(ctx, lhs, a, newv, size);
        if(err) return err;
        if(out && is_pre){
            // pre yields the stored bits, re-read truncated and extended
            err = ci_emit_load_bitfield(ci, ctx, lhs, a, newv, size);
            if(err) return err;
        }
    }
    else {
        err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
        if(err) return err;
        *op = (CiOp){
            .store = {
                .kind = CI_OP_STORE,
                .slot = a.slot,
                .src = newv,
                .src_size = size,
                .offset = a.disp,
                .loc = e->loc,
            }
        };
    }
    if(out){
        // pre yields the new value, post the old; both live in kept slots
        out->slot = is_pre? newv : old;
        if(dest != CI_NO_SLOT){
            err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
            if(err) return err;
            *op = (CiOp){
                .copy = {
                    .kind = CI_OP_COPY,
                    .slot = dest,
                    .slot_size = size,
                    .src = out->slot,
                    .src_size = size,
                    .loc = e->loc,
                }
            };
            out->slot = dest;
        }
    }
    return 0;
}

// Lower __builtin_{add,sub,mul}_overflow(a, b, &res) into a CI_OP_CHECKED plus
// a store of the truncated result through the result pointer. The expression's
// value is the 1-byte overflow bool; out is null in statement context, where
// the flag is discarded but the store still happens.
static
int
ci_lower_checked(CiInterpreter* ci, CiLowerCtx* ctx, CcExpr* e, uint32_t dest, CiLowerVal*_Nullable out){
    int err;
    CcParser* p = &ci->parser;
    CcExpr* ae = e->lhs;
    CcExpr* be = e->values[0];
    CcExpr* rese = e->values[1];
    uint32_t asz, bsz, dsz;
    err = cc_sizeof_as_uint(p, ae->type, ae->loc, &asz);
    if(err) return err;
    err = cc_sizeof_as_uint(p, be->type, be->loc, &bsz);
    if(err) return err;
    CcQualType dtype = ccqt_as_ptr(rese->type)->pointee;
    err = cc_sizeof_as_uint(p, dtype, e->loc, &dsz);
    if(err) return err;
    CiCheckedOp cmp_op = e->kind == CC_EXPR_ADD_OVERFLOW? CI_CHK_ADD
                    : e->kind == CC_EXPR_SUB_OVERFLOW? CI_CHK_SUB
                    : CI_CHK_MUL;
    // the expression value is the 1-byte overflow bool
    uint32_t ovf;
    uint32_t save;
    if(out){
        err = ci_lower_dest(ctx, &dest, 1);
        if(err) return err;
        out->slot = dest;
        ovf = dest;
        save = ctx->temp;
    }
    else {
        // statement context: the flag is discarded, but the store must happen
        save = ctx->temp;
        err = ci_alloc_slot(ctx, 1, 1, &ovf);
        if(err) return err;
    }
    CiLowerVal av, bv, pv;
    err = ci_lower_expr(ci, ctx, ae, CI_NO_SLOT, &av);
    if(err) return err;
    err = ci_lower_expr(ci, ctx, be, CI_NO_SLOT, &bv);
    if(err) return err;
    err = ci_lower_expr(ci, ctx, rese, CI_NO_SLOT, &pv); // the result pointer
    if(err) return err;
    uint32_t rtmp;
    err = ci_alloc_slot(ctx, dsz, dsz, &rtmp);
    if(err) return err;
    CiOp* op;
    err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
    if(err) return err;
    *op = (CiOp){
        .checked = {
            .kind = CI_OP_CHECKED,
            .op = cmp_op,
            .src_size = asz,
            .src2_size = bsz,
            .res_size = dsz,
            .src_unsigned = ccqt_is_unsigned(ae->type, ctx->char_is_unsigned),
            .src2_unsigned = ccqt_is_unsigned(be->type, ctx->char_is_unsigned),
            .res_unsigned = ccqt_is_unsigned(dtype, ctx->char_is_unsigned),
            .result = rtmp,
            .overflow = ovf,
            .src = av.slot,
            .src2 = bv.slot,
            .loc = e->loc,
        }
    };
    err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
    if(err) return err;
    *op = (CiOp){
        .store = {
            .kind = CI_OP_STORE,
            .slot = pv.slot,
            .src = rtmp,
            .src_size = dsz,
            .offset = 0,
            .loc = e->loc,
        }
    };
    ctx->temp = save;
    return 0;
}

static
int
ci_lower_umul128(CiInterpreter* ci, CiLowerCtx* ctx, CcExpr* e, uint32_t dest, CiLowerVal*_Nullable out){
    int err;
    CcExpr* ae = e->lhs;
    CcExpr* be = e->values[0];
    CcExpr* he = e->values[1]; // pointer to the high half
    if(out){
        err = ci_lower_dest(ctx, &dest, 8);
        if(err) return err;
        out->slot = dest;
    }
    uint32_t save = ctx->temp;
    CiLowerVal av, bv, hv;
    err = ci_lower_expr(ci, ctx, ae, CI_NO_SLOT, &av);
    if(err) return err;
    err = ci_lower_expr(ci, ctx, be, CI_NO_SLOT, &bv);
    if(err) return err;
    err = ci_lower_expr(ci, ctx, he, CI_NO_SLOT, &hv);
    if(err) return err;
    uint32_t aw, bw;
    err = ci_alloc_slot(ctx, 16, 16, &aw);
    if(err) return err;
    err = ci_alloc_slot(ctx, 16, 16, &bw);
    if(err) return err;
    CiOp* op;
    err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
    if(err) return err;
    *op = (CiOp){
        .convert = {
            .kind = CI_OP_CONVERT,
            .slot = aw,
            .slot_size = 16,
            .src = av.slot,
            .src_size = av.size,
            .is_unsigned = 1,
            .loc = ae->loc,
        }
    };
    err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
    if(err) return err;
    *op = (CiOp){
        .convert = {
            .kind = CI_OP_CONVERT,
            .slot = bw,
            .slot_size = 16,
            .src = bv.slot,
            .src_size = bv.size,
            .is_unsigned = 1,
            .loc = be->loc,
        }
    };
    uint32_t prod;
    err = ci_alloc_slot(ctx, 16, 16, &prod);
    if(err) return err;
    err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
    if(err) return err;
    *op = (CiOp){
        .alu = {
            .kind = CI_OP_ALU128,
            .op = CI_ALU_MUL,
            .is_unsigned = 1,
            .slot = prod,
            .src = aw,
            .src2 = bw,
            .loc = e->loc,
        }
    };
    // *high = product[8:16]
    err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
    if(err) return err;
    *op = (CiOp){
        .store = {
            .kind = CI_OP_STORE,
            .slot = hv.slot,
            .src = prod + 8,
            .src_size = 8,
            .offset = 0,
            .loc = e->loc,
        }
    };
    if(out){
        // value = product[0:8], copied to a stable slot before the scratch is freed
        err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
        if(err) return err;
        *op = (CiOp){
            .copy = {
                .kind = CI_OP_COPY,
                .slot = dest,
                .slot_size = 8,
                .src = prod,
                .src_size = 8,
                .loc = e->loc,
            }
        };
    }
    ctx->temp = save;
    return 0;
}

// Lower __builtin_popcount/clz/ctz into a CI_OP_BITCOUNT. The operand keeps its
// own width (the parser rejects __int128); the result is int.
static
int
ci_lower_bitcount(CiInterpreter* ci, CiLowerCtx* ctx, CcExpr* e, uint32_t dest, CiLowerVal* out){
    int err;
    CcParser* p = &ci->parser;
    uint32_t sz;
    err = cc_sizeof_as_uint(p, e->lhs->type, e->lhs->loc, &sz);
    if(err) return err;
    uint32_t rsz;
    err = cc_sizeof_as_uint(p, e->type, e->loc, &rsz);
    if(err) return err;
    CiBitCountOp bop = e->kind == CC_EXPR_POPCOUNT? CI_BITCNT_POPCOUNT
                     : e->kind == CC_EXPR_CTZ? CI_BITCNT_CTZ
                     : CI_BITCNT_CLZ;
    err = ci_lower_dest(ctx, &dest, rsz);
    if(err) return err;
    out->slot = dest;
    uint32_t save = ctx->temp;
    CiLowerVal v;
    err = ci_lower_expr(ci, ctx, e->lhs, CI_NO_SLOT, &v);
    if(err) return err;
    CiOp* op;
    err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
    if(err) return err;
    *op = (CiOp){
        .bitcount = {
            .kind = CI_OP_BITCOUNT,
            .op = bop,
            .src_size = sz,
            .slot = dest,
            .slot_size = rsz,
            .src = v.slot,
            .loc = e->loc,
        }
    };
    ctx->temp = save;
    return 0;
}

static
int
ci_lower_call(CiInterpreter* ci, CiLowerCtx* ctx, CcExpr* e, uint32_t dest, CiLowerVal*_Nullable out){
    int err;
    CcParser* p = &ci->parser;
    CcExpr* callee = e->lhs;
    CcFunc* func = NULL;
    CcFunction* ftype;
    if(callee->kind == CC_EXPR_FUNCTION){
        func = callee->func;
        ftype = func->type;
    }
    else {
        CcQualType ct = callee->type;
        while(ccqt_kind(ct) == CC_FUNCTION && callee->kind == CC_EXPR_DEREF){
            // (*fp)(...): the function pointer is the deref's operand
            callee = callee->lhs;
            ct = callee->type;
        }
        if(ccqt_kind(ct) != CC_POINTER)
            return ci_unreachable(ci, e->loc, "calling a non-function-pointer?");
        CcQualType pointee = ccqt_as_ptr(ct)->pointee;
        if(ccqt_kind(pointee) != CC_FUNCTION)
            return ci_unreachable(ci, e->loc, "calling a non-function pointer?");
        ftype = ccqt_as_function(pointee);
    }
    uint32_t nargs = e->call.nargs;
    if(!ftype->is_variadic && nargs != ftype->param_count)
        return ci_unimplemented(ci, e->loc, "K&R calls");
    uint32_t ret_size = 0;
    if(out){
        err = ci_lower_dest(ctx, &dest, out->size);
        if(err) return err;
        out->slot = dest;
        ret_size = out->size;
    }
    uint32_t temp = ctx->temp;
    // The argv region; an indirect call's function pointer is its first cell.
    uint32_t argv_cells = nargs + (func? 0 : 1);
    uint32_t argv_slot = 0;
    if(argv_cells){
        err = ci_alloc_slot(ctx, argv_cells * 8, 8, &argv_slot);
        if(err) return err;
    }
    uint32_t argv_cell = argv_slot;
    if(!func){
        CiLowerVal v;
        err = ci_lower_expr(ci, ctx, callee, argv_cell, &v);
        if(err) return err;
        argv_cell += 8;
    }
    CiCallDescriptor* d = Allocator_zalloc(ctx->a, sizeof *d + nargs * sizeof d->arg_sizes[0]);
    if(!d) return CI_OOM_ERROR;
    if(func) d->func = func;
    else d->func_type = ftype;
    d->nargs = nargs;
    d->expr = e;
    for(uint32_t i = 0; i < nargs; i++){
        uint32_t asz;
        err = cc_sizeof_as_uint(p, e->values[i]->type, e->values[i]->loc, &asz);
        if(err) return err;
        d->arg_sizes[i] = asz;
        uint32_t slot_sz = asz < 8 ? 8 : asz;
        uint32_t slot;
        err = ci_alloc_slot(ctx, slot_sz, slot_sz > 8 ? 16 : 8, &slot);
        if(err) return err;
        CiLowerVal v;
        err = ci_lower_expr(ci, ctx, e->values[i], slot, &v);
        if(err) return err;
        CiOp* op;
        err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
        if(err) return err;
        *op = (CiOp){
            .slot_addr = {
                .kind = CI_OP_SLOT_ADDR,
                .slot = argv_cell + i * 8,
                .slot_size = 8,
                .src = slot,
                .loc = e->values[i]->loc,
            }
        };
    }
    CiOp* op;
    err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
    if(err) return err;
    *op = (CiOp){
        .call = {
            .kind = CI_OP_CALL,
            .is_indirect = func?0:1,
            .is_variadic = ftype->is_variadic && nargs != ftype->param_count,
            .ret_slot = out? dest : 0,
            .ret_size = ret_size,
            .argv_slot = argv_slot,
            .descrip = d,
            .loc = e->loc,
        }
    };
    ctx->temp = temp;
    return 0;
}

static
int
ci_lower_assign_direct(CiInterpreter* ci, CiLowerCtx* ctx, CcExpr* e, _Bool* handled){
    int err;
    CcParser* p = &ci->parser;
    *handled = 0;
    CcExpr* lhs = e->lhs;
    CcExpr* rhs = e->values[0];
    if(lhs->type.is_atomic || rhs->type.is_atomic)
        return 0; // atomics fall back
    uint32_t frame_slot;
    if((ccqt_is_integer(rhs->type) || ci_falu_type(rhs->type)
            || ccqt_kind(rhs->type) == CC_POINTER || ccqt_bt_eq(rhs->type, CCBT_nullptr_t))
        && !((lhs->kind == CC_EXPR_DOT || lhs->kind == CC_EXPR_ARROW) && lhs->field_loc.bit_width)
        && !ci_frame_lvalue(lhs, &frame_slot)){
        uint32_t size, rhs_size;
        err = cc_sizeof_as_uint(p, lhs->type, lhs->loc, &size);
        if(err) return err;
        err = cc_sizeof_as_uint(p, rhs->type, rhs->loc, &rhs_size);
        if(err) return err;
        CiFoldValue fold;
        int fold_fail = ci_fold_expr(ci, ctx, rhs, &fold);
        if(fold_fail > 0) return fold_fail;
        if(fold_fail == 0 && size == rhs_size && (size == 1 || size == 2 || size == 4 || size == 8)){
            uint32_t temp = ctx->temp;
            CiLowerAddr a;
            err = ci_lower_addr(ci, ctx, lhs, 0, &a);
            if(err) return err;
            CiOp* op;
            err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
            if(err) return err;
            *op = (CiOp){
                .store_imm = {
                    .kind = CI_OP_STORE_IMM,
                    .size = size,
                    .slot = a.slot,
                    .offset = a.disp,
                    .loc = e->loc,
                }
            };
            memcpy(&op->store_imm.immediate, fold.bits, sizeof op->store_imm.immediate);
            ctx->temp = temp;
            *handled = 1;
            return 0;
        }
    }
    if(rhs->kind == CC_EXPR_INIT_LIST || rhs->kind == CC_EXPR_COMPOUND_LITERAL){
        uint32_t frame_off;
        if(ci_frame_lvalue(lhs, &frame_off))
            return 0;
        uint32_t temp = ctx->temp;
        CiLowerAddr a;
        err = ci_lower_addr(ci, ctx, lhs, 0, &a); // access
        if(err) return err;
        err = ci_lower_init_list(ci, ctx, rhs, a, 1);
        if(err) return err;
        ctx->temp = temp;
        *handled = 1;
        return 0;
    }
    switch(ccqt_kind(lhs->type)){
        case CC_STRUCT:
        case CC_UNION:
        case CC_ARRAY:
        case CC_SLICE:
            break;
        default:
            return 0; // scalars stage through a slot cheaply
    }
    uint32_t off;
    if(ci_frame_lvalue(lhs, &off))
        return 0; // the rhs loads directly into the slot
    switch((uint32_t)rhs->kind){
        case CC_EXPR_VALUE:
            if(ccqt_kind(rhs->type) == CC_ARRAY)
                break;
            return 0;
        case CC_EXPR_VARIABLE:
        case CC_EXPR_DEREF:
        case CC_EXPR_ARROW:
        case CC_EXPR_DOT:
        case CC_EXPR_SUBSCRIPT:
            break;
        default:
            return 0; // not an lvalue; it must be materialized anyway
    }
    if(ci_frame_lvalue(rhs, &off))
        return 0; // the store reads directly from the slot
    uint32_t size, rsz;
    err = cc_sizeof_as_uint(p, lhs->type, lhs->loc, &size);
    if(err) return err;
    err = cc_sizeof_as_uint(p, rhs->type, rhs->loc, &rsz);
    if(err) return err;
    if(rsz > size)
        return 0;
    uint32_t temp = ctx->temp;
    CiLowerAddr dst;
    err = ci_lower_addr(ci, ctx, lhs, 0, &dst); // access
    if(err) return err;
    CiLowerAddr src;
    err = ci_lower_addr(ci, ctx, rhs, 0, &src); // access
    if(err) return err;
    CiOp* op;
    err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
    if(err) return err;
    *op = (CiOp){
        .memcopy = {
            .kind = CI_OP_MEMCOPY,
            .slot = dst.slot,
            .offset = dst.disp,
            .src = src.slot,
            .src_offset = src.disp,
            .size = size,
            .loc = e->loc,
        }
    };
    if(rsz < size){
        err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
        if(err) return err;
        *op = (CiOp){
            .zero = {
                .kind = CI_OP_ZERO,
                .slot = dst.slot,
                .offset = dst.disp+rsz,
                .size = size-rsz,
                .loc = e->loc,
            }
        };
    }
    ctx->temp = temp;
    *handled = 1;
    return 0;
}

static
_Bool
ci_armw_op_for(CcExprKind kind, CiAtomicRmwOp* out){
    switch((uint32_t)kind){
        case CC_EXPR_ADDASSIGN:    *out = CI_ARMW_ADD; return 1;
        case CC_EXPR_SUBASSIGN:    *out = CI_ARMW_SUB; return 1;
        case CC_EXPR_BITANDASSIGN: *out = CI_ARMW_AND; return 1;
        case CC_EXPR_BITORASSIGN:  *out = CI_ARMW_OR;  return 1;
        case CC_EXPR_BITXORASSIGN: *out = CI_ARMW_XOR; return 1;
        default: return 0;
    }
}

// Load of an _Atomic lvalue in value context; plain accesses are seq_cst.
static
int
ci_lower_atomic_load_lv(CiInterpreter* ci, CiLowerCtx* ctx, CcExpr* e, uint32_t dest, CiLowerVal* out, uint32_t size){
    int err;
    err = ci_lower_dest(ctx, &dest, size);
    if(err) return err;
    uint32_t temp = ctx->temp;
    CiLowerAddr a;
    err = ci_lower_addr(ci, ctx, e, 0, &a); // access
    if(err) return err;
    CiOp* op;
    err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
    if(err) return err;
    *op = (CiOp){
        .atomic_load = {
            .kind = CI_OP_ATOMIC_LOAD,
            .memorder = CC_MO_SEQ_CST,
            .slot = dest,
            .slot_size = size,
            .src = a.slot,
            .offset = a.disp,
            .loc = e->loc,
        }
    };
    ctx->temp = temp;
    out->slot = dest;
    return 0;
}

static
int
ci_lower_atomic_operand(CiInterpreter* ci, CiLowerCtx* ctx, const CcExpr* ve, CiLowerVal* v, uint32_t sz, SrcLoc loc){
    (void)ci;
    if(v->size == sz) return 0;
    int err;
    uint32_t conv;
    err = ci_alloc_slot(ctx, sz, sz, &conv);
    if(err) return err;
    CiOp* op;
    err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
    if(err) return err;
    *op = (CiOp){
        .convert = {
            .kind = CI_OP_CONVERT,
            .is_unsigned = ccqt_is_unsigned(ve->type, ctx->char_is_unsigned),
            .slot = conv,
            .slot_size = sz,
            .src = v->slot,
            .src_size = v->size,
            .loc = loc,
        }
    };
    v->slot = conv;
    v->size = sz;
    return 0;
}

static
int
ci_lower_atomic_compound_assign(CiInterpreter* ci, CiLowerCtx* ctx, CcExpr* e, uint32_t dest, CiLowerVal* out, uint32_t size, _Bool is_ptr, uint32_t elem_sz, _Bool op_unsigned, _Bool is_float, CiFaluOp fop){
    int err;
    CiOpKind falukind = size == 4? CI_OP_FALU32 : CI_OP_FALU64;
    CiOpKind intalukind = ci_int_op_kind(size);
    if(!intalukind) return ci_ice(ci, e->loc, "unsupported integer ALU size %u", size);
    CcExpr* lhs = e->lhs;
    CcExpr* rhs = e->values[0];
    CiOp* op;
    // cur holds the new value and survives; the rest is scratch
    uint32_t cur;
    err = ci_alloc_slot(ctx, size, size, &cur);
    if(err) return err;
    uint32_t keep = ctx->temp;
    CiLowerAddr a;
    err = ci_lower_addr(ci, ctx, lhs, 0, &a); // access
    if(err) return err;
    CiLowerVal r;
    err = ci_lower_expr(ci, ctx, rhs, CI_NO_SLOT, &r);
    if(err) return err;
    if(is_ptr && elem_sz != 1){
        // pointer += and -= scale the integer by the pointee size
        uint32_t esz, scaled;
        err = ci_alloc_slot(ctx, 8, 8, &esz);
        if(err) return err;
        err = ci_alloc_slot(ctx, 8, 8, &scaled);
        if(err) return err;
        err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
        if(err) return err;
        *op = (CiOp){
            .constant = {
                .kind = CI_OP_CONST,
                .bt_kind = (uint32_t)ci_target(ci)->size_type,
                .slot = esz,
                .immsize = 8,
                .immediate = {elem_sz},
                .loc = e->loc,
            }
        };
        err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
        if(err) return err;
        *op = (CiOp){
            .alu = {
                .kind = ctx->size_size == 8? CI_OP_ALU64 : CI_OP_ALU32,
                .slot = scaled,
                .src = r.slot,
                .src2 = esz,
                .op = CI_ALU_MUL,
                .is_unsigned = op_unsigned,
                .loc = e->loc,
            }
        };
        r.slot = scaled;
        r.size = 8;
        op_unsigned = 1;
    }
    // rmw and cas operands are the object's size
    err = ci_lower_atomic_operand(ci, ctx, rhs, &r, size, e->loc);
    if(err) return err;
    CiAtomicRmwOp armw;
    if(!is_float && size <= 8 && ci_armw_op_for(e->kind, &armw)){
        uint32_t old;
        err = ci_alloc_slot(ctx, size, size, &old);
        if(err) return err;
        err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
        if(err) return err;
        *op = (CiOp){
            .atomic_rmw = {
                .kind = CI_OP_ATOMIC_RMW,
                .op = armw,
                .memorder = CC_MO_SEQ_CST,
                .slot = old,
                .slot_size = size,
                .src = a.slot,
                .src2 = r.slot,
                .offset = a.disp,
                .loc = e->loc,
            }
        };
        err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
        if(err) return err;
        *op = (CiOp){
            .alu = {
                .kind = intalukind,
                .slot = cur,
                .src = old,
                .src2 = r.slot,
                .op = ci_alu_op_for(e->kind),
                .is_unsigned = op_unsigned,
                .loc = e->loc,
            }
        };
    }
    else {
        // compare-exchange loop: the failed cas reloads the old value, so
        // recompute and retry until the store wins
        uint32_t old, ok;
        err = ci_alloc_slot(ctx, size, size, &old);
        if(err) return err;
        err = ci_alloc_slot(ctx, 1, 1, &ok);
        if(err) return err;
        err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
        if(err) return err;
        *op = (CiOp){
            .atomic_load = {
                .kind = CI_OP_ATOMIC_LOAD,
                .memorder = CC_MO_SEQ_CST,
                .slot = old,
                .slot_size = size,
                .src = a.slot,
                .offset = a.disp,
                .loc = e->loc,
            }
        };
        uint32_t loop = (uint32_t)ctx->out->count;
        err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
        if(err) return err;
        if(is_float){
            *op = (CiOp){
                .falu32 = {
                    .kind = falukind,
                    .op = fop,
                    .slot = cur,
                    .slot_size = size,
                    .src = old,
                    .src2 = r.slot,
                    .loc = e->loc,
                }
            };
        }
        else {
            *op = (CiOp){
                .alu = {
                    .kind = intalukind,
                    .slot = cur,
                    .src = old,
                    .src2 = r.slot,
                    .op = ci_alu_op_for(e->kind),
                    .is_unsigned = op_unsigned,
                    .loc = e->loc,
                }
            };
        }
        err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
        if(err) return err;
        *op = (CiOp){
            .atomic_cas = {
                .kind = CI_OP_ATOMIC_CAS,
                .memorder = CC_MO_SEQ_CST,
                .fail_memorder = CC_MO_SEQ_CST,
                .weak = 1,
                .size = size,
                .slot = ok,
                .src = a.slot,
                .expected = old,
                .desired = cur,
                .offset = a.disp,
                .loc = e->loc,
            }
        };
        err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
        if(err) return err;
        *op = (CiOp){
            .jump_false = {
                .kind = CI_OP_JUMP_FALSE,
                .slot = ok,
                .slot_size = 1,
                .jump = loop,
                .loc = e->loc,
            }
        };
    }
    ctx->temp = keep;
    if(dest == CI_NO_SLOT){
        out->slot = cur;
        return 0;
    }
    out->slot = dest;
    err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
    if(err) return err;
    *op = (CiOp){
        .copy = {
            .kind = CI_OP_COPY,
            .slot = dest,
            .slot_size = size,
            .src = cur,
            .src_size = size,
            .loc = e->loc,
        }
    };
    return 0;
}

static
int
ci_lower_atomic_builtin(CiInterpreter* ci, CiLowerCtx* ctx, CcExpr* e, uint32_t dest, CiLowerVal*_Nullable out){
    int err;
    CcParser* p = &ci->parser;
    CcAtomicOp aop = e->atomic.op;
    CcMemoryOrder mo = e->atomic.memorder;
    CiOp* op;
    if(aop == CC_ATOMIC_THREAD_FENCE || aop == CC_ATOMIC_SIGNAL_FENCE){
        err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
        if(err) return err;
        *op = (CiOp){
            .fence = {
                .kind = CI_OP_FENCE,
                .memorder = mo,
                .is_signal = aop == CC_ATOMIC_SIGNAL_FENCE,
                .loc = e->loc,
            }
        };
        if(out){
            // void result; the slot exists but is never read
            err = ci_lower_dest(ctx, &dest, out->size);
            if(err) return err;
            out->slot = dest;
        }
        return 0;
    }
    uint32_t sz;
    err = cc_sizeof_as_uint(p, ccqt_as_ptr(e->lhs->type)->pointee, e->loc, &sz);
    if(err) return err;
    if(aop == CC_ATOMIC_INTERLOCKED_COMPARE_EXCHANGE128)
        sz = 16; // the pointer is to the low __int64 of the pair
    if(out){
        // the result slot survives; everything else is scratch
        err = ci_lower_dest(ctx, &dest, out->size);
        if(err) return err;
        out->slot = dest;
    }
    uint32_t temp = ctx->temp;
    CiLowerVal pv;
    err = ci_lower_expr(ci, ctx, e->lhs, CI_NO_SLOT, &pv);
    if(err) return err;
    switch(aop){
        case CC_ATOMIC_LOAD_N:{
            uint32_t slot = dest;
            if(!out){
                err = ci_alloc_slot(ctx, sz, sz, &slot);
                if(err) return err;
            }
            err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
            if(err) return err;
            *op = (CiOp){
                .atomic_load = {
                    .kind = CI_OP_ATOMIC_LOAD,
                    .memorder = mo,
                    .slot = slot,
                    .slot_size = sz,
                    .src = pv.slot,
                    .loc = e->loc,
                }
            };
            break;
        }
        case CC_ATOMIC_LOAD:{
            // the generic form returns the value through a pointer
            CiLowerVal dp;
            err = ci_lower_expr(ci, ctx, e->values[0], CI_NO_SLOT, &dp);
            if(err) return err;
            uint32_t tmp;
            err = ci_alloc_slot(ctx, sz, sz, &tmp);
            if(err) return err;
            err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
            if(err) return err;
            *op = (CiOp){
                .atomic_load = {
                    .kind = CI_OP_ATOMIC_LOAD,
                    .memorder = mo,
                    .slot = tmp,
                    .slot_size = sz,
                    .src = pv.slot,
                    .loc = e->loc,
                }
            };
            err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
            if(err) return err;
            *op = (CiOp){
                .store = {
                    .kind = CI_OP_STORE,
                    .slot = dp.slot,
                    .src = tmp,
                    .src_size = sz,
                    .loc = e->loc,
                }
            };
            break;
        }
        case CC_ATOMIC_STORE_N:
        case CC_ATOMIC_STORE:{
            CiLowerVal v;
            err = ci_lower_expr(ci, ctx, e->values[0], CI_NO_SLOT, &v);
            if(err) return err;
            if(aop == CC_ATOMIC_STORE){
                // the generic form passes the value by pointer
                uint32_t tmp;
                err = ci_alloc_slot(ctx, sz, sz, &tmp);
                if(err) return err;
                err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
                if(err) return err;
                *op = (CiOp){
                    .load = {
                        .kind = CI_OP_LOAD,
                        .slot = tmp,
                        .slot_size = sz,
                        .src = v.slot,
                        .loc = e->loc,
                    }
                };
                v.slot = tmp;
                v.size = sz;
            }
            else {
                err = ci_lower_atomic_operand(ci, ctx, e->values[0], &v, sz, e->loc);
                if(err) return err;
            }
            err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
            if(err) return err;
            *op = (CiOp){
                .atomic_store = {
                    .kind = CI_OP_ATOMIC_STORE,
                    .memorder = mo,
                    .slot = pv.slot,
                    .src = v.slot,
                    .src_size = sz,
                    .loc = e->loc,
                }
            };
            break;
        }
        case CC_ATOMIC_EXCHANGE_N:{
            CiLowerVal v;
            err = ci_lower_expr(ci, ctx, e->values[0], CI_NO_SLOT, &v);
            if(err) return err;
            err = ci_lower_atomic_operand(ci, ctx, e->values[0], &v, sz, e->loc);
            if(err) return err;
            uint32_t slot = dest;
            if(!out){
                err = ci_alloc_slot(ctx, sz, sz, &slot);
                if(err) return err;
            }
            err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
            if(err) return err;
            *op = (CiOp){
                .atomic_rmw = {
                    .kind = CI_OP_ATOMIC_RMW,
                    .op = CI_ARMW_XCHG,
                    .memorder = mo,
                    .discard = !out,
                    .slot = slot,
                    .slot_size = sz,
                    .src = pv.slot,
                    .src2 = v.slot,
                    .loc = e->loc,
                }
            };
            break;
        }
        case CC_ATOMIC_EXCHANGE:{
            // the generic form: value and old-value result both by pointer
            CiLowerVal vp, rp;
            err = ci_lower_expr(ci, ctx, e->values[0], CI_NO_SLOT, &vp);
            if(err) return err;
            err = ci_lower_expr(ci, ctx, e->values[1], CI_NO_SLOT, &rp);
            if(err) return err;
            uint32_t tmpv, tmpold;
            err = ci_alloc_slot(ctx, sz, sz, &tmpv);
            if(err) return err;
            err = ci_alloc_slot(ctx, sz, sz, &tmpold);
            if(err) return err;
            err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
            if(err) return err;
            *op = (CiOp){
                .load = {
                    .kind = CI_OP_LOAD,
                    .slot = tmpv,
                    .slot_size = sz,
                    .src = vp.slot,
                    .loc = e->loc,
                }
            };
            err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
            if(err) return err;
            *op = (CiOp){
                .atomic_rmw = {
                    .kind = CI_OP_ATOMIC_RMW,
                    .op = CI_ARMW_XCHG,
                    .memorder = mo,
                    .slot = tmpold,
                    .slot_size = sz,
                    .src = pv.slot,
                    .src2 = tmpv,
                    .loc = e->loc,
                }
            };
            err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
            if(err) return err;
            *op = (CiOp){
                .store = {
                    .kind = CI_OP_STORE,
                    .slot = rp.slot,
                    .src = tmpold,
                    .src_size = sz,
                    .loc = e->loc,
                }
            };
            break;
        }
        case CC_ATOMIC_COMPARE_EXCHANGE_N:
        case CC_ATOMIC_COMPARE_EXCHANGE:{
            CiLowerVal ep;
            err = ci_lower_expr(ci, ctx, e->values[0], CI_NO_SLOT, &ep);
            if(err) return err;
            uint32_t eslot;
            err = ci_alloc_slot(ctx, sz, sz, &eslot);
            if(err) return err;
            err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
            if(err) return err;
            *op = (CiOp){
                .load = {
                    .kind = CI_OP_LOAD,
                    .slot = eslot,
                    .slot_size = sz,
                    .src = ep.slot,
                    .loc = e->loc,
                }
            };
            CiLowerVal dv;
            err = ci_lower_expr(ci, ctx, e->values[1], CI_NO_SLOT, &dv);
            if(err) return err;
            if(aop == CC_ATOMIC_COMPARE_EXCHANGE){
                // the generic form passes the desired value by pointer
                uint32_t tmp;
                err = ci_alloc_slot(ctx, sz, sz, &tmp);
                if(err) return err;
                err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
                if(err) return err;
                *op = (CiOp){
                    .load = {
                        .kind = CI_OP_LOAD,
                        .slot = tmp,
                        .slot_size = sz,
                        .src = dv.slot,
                        .loc = e->loc,
                    }
                };
                dv.slot = tmp;
                dv.size = sz;
            }
            else {
                err = ci_lower_atomic_operand(ci, ctx, e->values[1], &dv, sz, e->loc);
                if(err) return err;
            }
            uint32_t ok = dest;
            if(!out){
                err = ci_alloc_slot(ctx, 1, 1, &ok);
                if(err) return err;
            }
            err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
            if(err) return err;
            *op = (CiOp){
                .atomic_cas = {
                    .kind = CI_OP_ATOMIC_CAS,
                    .memorder = mo,
                    .fail_memorder = e->atomic.fail_memorder,
                    .weak = e->atomic.weak,
                    .size = sz,
                    .slot = ok,
                    .src = pv.slot,
                    .expected = eslot,
                    .desired = dv.slot,
                    .loc = e->loc,
                }
            };
            // the old value writes back through the expected pointer; on
            // success it equals what was already there
            err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
            if(err) return err;
            *op = (CiOp){
                .store = {
                    .kind = CI_OP_STORE,
                    .slot = ep.slot,
                    .src = eslot,
                    .src_size = sz,
                    .loc = e->loc,
                }
            };
            if(out) out->canonical = 1;
            break;
        }
        case CC_ATOMIC_FETCH_ADD:
        case CC_ATOMIC_FETCH_SUB:
        case CC_ATOMIC_ADD_FETCH:
        case CC_ATOMIC_SUB_FETCH:
        case CC_ATOMIC_FETCH_AND:
        case CC_ATOMIC_FETCH_OR:
        case CC_ATOMIC_FETCH_XOR:{
            CiLowerVal v;
            err = ci_lower_expr(ci, ctx, e->values[0], CI_NO_SLOT, &v);
            if(err) return err;
            _Bool is_add = aop == CC_ATOMIC_FETCH_ADD || aop == CC_ATOMIC_ADD_FETCH;
            _Bool is_sub = aop == CC_ATOMIC_FETCH_SUB || aop == CC_ATOMIC_SUB_FETCH;
            CcQualType obj = ccqt_as_ptr(e->lhs->type)->pointee;
            obj.is_atomic = 0;
            if((is_add || is_sub) && ccqt_kind(obj) == CC_POINTER){
                // scale the delta by the pointee size
                uint32_t elem_sz;
                err = cc_sizeof_as_uint(p, ccqt_as_ptr(obj)->pointee, e->loc, &elem_sz);
                if(err) return err;
                if(elem_sz != 1){
                    uint32_t esz, scaled;
                    err = ci_alloc_slot(ctx, 8, 8, &esz);
                    if(err) return err;
                    err = ci_alloc_slot(ctx, 8, 8, &scaled);
                    if(err) return err;
                    err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
                    if(err) return err;
                    *op = (CiOp){
                        .constant = {
                            .kind = CI_OP_CONST,
                            .bt_kind = (uint32_t)ci_target(ci)->size_type,
                            .slot = esz,
                            .immsize = 8,
                            .immediate = {elem_sz},
                            .loc = e->loc,
                        }
                    };
                    err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
                    if(err) return err;
                    *op = (CiOp){
                        .alu = {
                            .kind = ctx->size_size == 8? CI_OP_ALU64 : CI_OP_ALU32,
                            .slot = scaled,
                            .src = v.slot,
                            .src2 = esz,
                            .op = CI_ALU_MUL,
                            .is_unsigned = ccqt_is_unsigned(e->values[0]->type, ctx->char_is_unsigned),
                            .loc = e->loc,
                        }
                    };
                    v.slot = scaled;
                    v.size = 8;
                }
            }
            err = ci_lower_atomic_operand(ci, ctx, e->values[0], &v, sz, e->loc);
            if(err) return err;
            _Bool want_new = aop == CC_ATOMIC_ADD_FETCH || aop == CC_ATOMIC_SUB_FETCH;
            uint32_t old = dest;
            if(!out || want_new){
                err = ci_alloc_slot(ctx, sz, sz, &old);
                if(err) return err;
            }
            CiAtomicRmwOp armw =
                is_add? CI_ARMW_ADD :
                is_sub? CI_ARMW_SUB :
                aop == CC_ATOMIC_FETCH_AND? CI_ARMW_AND :
                aop == CC_ATOMIC_FETCH_OR? CI_ARMW_OR : CI_ARMW_XOR;
            err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
            if(err) return err;
            *op = (CiOp){
                .atomic_rmw = {
                    .kind = CI_OP_ATOMIC_RMW,
                    .op = armw,
                    .memorder = mo,
                    .discard = !out,
                    .slot = old,
                    .slot_size = sz,
                    .src = pv.slot,
                    .src2 = v.slot,
                    .loc = e->loc,
                }
            };
            if(out && want_new){
                err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
                if(err) return err;
                *op = (CiOp){
                    .alu = {
                        .kind = ci_int_op_kind(sz),
                        .slot = dest,
                        .src = old,
                        .src2 = v.slot,
                        .op = is_add? CI_ALU_ADD : CI_ALU_SUB,
                        .is_unsigned = 1,
                        .loc = e->loc,
                    }
                };
            }
            break;
        }
        case CC_ATOMIC_INTERLOCKED_INCREMENT:
        case CC_ATOMIC_INTERLOCKED_DECREMENT:{
            // these yield the new value
            _Bool inc = aop == CC_ATOMIC_INTERLOCKED_INCREMENT;
            uint32_t one, old;
            err = ci_alloc_slot(ctx, sz, sz, &one);
            if(err) return err;
            err = ci_alloc_slot(ctx, sz, sz, &old);
            if(err) return err;
            err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
            if(err) return err;
            *op = (CiOp){
                .constant = {
                    .kind = CI_OP_CONST,
                    .bt_kind = (uint32_t)ci_target(ci)->size_type,
                    .slot = one,
                    .immsize = sz,
                    .immediate = {1},
                    .loc = e->loc,
                }
            };
            err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
            if(err) return err;
            *op = (CiOp){
                .atomic_rmw = {
                    .kind = CI_OP_ATOMIC_RMW,
                    .op = inc? CI_ARMW_ADD : CI_ARMW_SUB,
                    .memorder = mo,
                    .discard = !out,
                    .slot = old,
                    .slot_size = sz,
                    .src = pv.slot,
                    .src2 = one,
                    .loc = e->loc,
                }
            };
            if(out){
                err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
                if(err) return err;
                *op = (CiOp){
                    .alu = {
                        .kind = ci_int_op_kind(sz),
                        .slot = dest,
                        .src = old,
                        .src2 = one,
                        .op = inc? CI_ALU_ADD : CI_ALU_SUB,
                        .is_unsigned = 1,
                        .loc = e->loc,
                    }
                };
            }
            break;
        }
        case CC_ATOMIC_INTERLOCKED_COMPARE_EXCHANGE:{
            // values[0] = exchange, values[1] = comparand; yields the old value
            CiLowerVal x;
            err = ci_lower_expr(ci, ctx, e->values[0], CI_NO_SLOT, &x);
            if(err) return err;
            err = ci_lower_atomic_operand(ci, ctx, e->values[0], &x, sz, e->loc);
            if(err) return err;
            CiLowerVal c;
            err = ci_lower_expr(ci, ctx, e->values[1], CI_NO_SLOT, &c);
            if(err) return err;
            err = ci_lower_atomic_operand(ci, ctx, e->values[1], &c, sz, e->loc);
            if(err) return err;
            // cas writes the old value into expected, so it must be a
            // scratch slot, never the comparand's own storage
            uint32_t eslot, ok;
            err = ci_alloc_slot(ctx, sz, sz, &eslot);
            if(err) return err;
            err = ci_alloc_slot(ctx, 1, 1, &ok);
            if(err) return err;
            err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
            if(err) return err;
            *op = (CiOp){
                .copy = {
                    .kind = CI_OP_COPY,
                    .slot = eslot,
                    .slot_size = sz,
                    .src = c.slot,
                    .src_size = sz,
                    .loc = e->loc,
                }
            };
            err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
            if(err) return err;
            *op = (CiOp){
                .atomic_cas = {
                    .kind = CI_OP_ATOMIC_CAS,
                    .memorder = mo,
                    .fail_memorder = mo,
                    .size = sz,
                    .slot = ok,
                    .src = pv.slot,
                    .expected = eslot,
                    .desired = x.slot,
                    .loc = e->loc,
                }
            };
            if(out){
                err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
                if(err) return err;
                *op = (CiOp){
                    .copy = {
                        .kind = CI_OP_COPY,
                        .slot = dest,
                        .slot_size = sz,
                        .src = eslot,
                        .src_size = sz,
                        .loc = e->loc,
                    }
                };
            }
            break;
        }
        case CC_ATOMIC_INTERLOCKED_COMPARE_EXCHANGE128:{
            // values[0] = exchange_high, values[1] = exchange_low,
            // values[2] = comparand pointer (in/out); yields a bool
            CiLowerVal hi, lo;
            err = ci_lower_expr(ci, ctx, e->values[0], CI_NO_SLOT, &hi);
            if(err) return err;
            err = ci_lower_expr(ci, ctx, e->values[1], CI_NO_SLOT, &lo);
            if(err) return err;
            uint32_t des;
            err = ci_alloc_slot(ctx, 16, 16, &des);
            if(err) return err;
            err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
            if(err) return err;
            *op = (CiOp){
                .copy = {
                    .kind = CI_OP_COPY,
                    .slot = des,
                    .slot_size = 8,
                    .src = lo.slot,
                    .src_size = 8,
                    .loc = e->loc,
                }
            };
            err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
            if(err) return err;
            *op = (CiOp){
                .copy = {
                    .kind = CI_OP_COPY,
                    .slot = des + 8,
                    .slot_size = 8,
                    .src = hi.slot,
                    .src_size = 8,
                    .loc = e->loc,
                }
            };
            CiLowerVal cp;
            err = ci_lower_expr(ci, ctx, e->values[2], CI_NO_SLOT, &cp);
            if(err) return err;
            uint32_t eslot;
            err = ci_alloc_slot(ctx, 16, 16, &eslot);
            if(err) return err;
            err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
            if(err) return err;
            *op = (CiOp){
                .load = {
                    .kind = CI_OP_LOAD,
                    .slot = eslot,
                    .slot_size = 16,
                    .src = cp.slot,
                    .loc = e->loc,
                }
            };
            uint32_t ok = dest;
            if(!out){
                err = ci_alloc_slot(ctx, 1, 1, &ok);
                if(err) return err;
            }
            err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
            if(err) return err;
            *op = (CiOp){
                .atomic_cas = {
                    .kind = CI_OP_ATOMIC_CAS,
                    .memorder = mo,
                    .fail_memorder = mo,
                    .size = 16,
                    .slot = ok,
                    .src = pv.slot,
                    .expected = eslot,
                    .desired = des,
                    .loc = e->loc,
                }
            };
            err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
            if(err) return err;
            *op = (CiOp){
                .store = {
                    .kind = CI_OP_STORE,
                    .slot = cp.slot,
                    .src = eslot,
                    .src_size = 16,
                    .loc = e->loc,
                }
            };
            if(out) out->canonical = 1;
            break;
        }
        case CC_ATOMIC_THREAD_FENCE:
        case CC_ATOMIC_SIGNAL_FENCE:
            break; // handled above
    }
    ctx->temp = temp;
    return 0;
}

static
int
ci_emit_const(CiInterpreter* ci, CiLowerCtx* ctx, uint64_t v, uint32_t size, SrcLoc loc, uint32_t* out_slot){
    int err;
    uint32_t s;
    err = ci_alloc_slot(ctx, size, size, &s);
    if(err) return err;
    CiOp* op;
    err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
    if(err) return err;
    *op = (CiOp){
        .constant = {
            .kind = CI_OP_CONST,
            .bt_kind = (uint32_t)ci_target(ci)->size_type,
            .slot = s,
            .immsize = size,
            .immediate = {v},
            .loc = loc,
        }
    };
    *out_slot = s;
    return 0;
}

static
int
ci_emit_bound(CiLowerCtx* ctx, uint32_t idx, uint32_t bound, uint32_t size, _Bool idx_signed, SrcLoc loc){
    int err;
    CiOp* op;
    err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
    if(err) return err;
    *op = (CiOp){
        .bounds = {
            .kind = CI_OP_BOUNDS,
            .src = idx,
            .src_size = size,
            .src2 = bound,
            .src2_size = size,
            .inclusive = 1,
            .index_signed = idx_signed,
            .loc = loc,
        }
    };
    return 0;
}

static
int
ci_emit_copy(CiLowerCtx* ctx, uint32_t dst, uint32_t src, uint32_t size, SrcLoc loc){
    int err;
    CiOp* op;
    err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
    if(err) return err;
    *op = (CiOp){
        .copy = {
            .kind = CI_OP_COPY,
            .slot = dst,
            .slot_size = size,
            .src = src,
            .src_size = size,
            .loc = loc,
        }
    };
    return 0;
}

static
int
ci_emit_alu(CiLowerCtx* ctx, CiAluOp aop, uint32_t dst, uint32_t a, uint32_t b, uint32_t size, SrcLoc loc){
    int err;
    CiOp* op;
    err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
    if(err) return err;
    *op = (CiOp){
        .alu = {
            .kind = size==8?CI_OP_ALU64:size==4?CI_OP_ALU32:CI_OP_ALU128,
            .op = aop,
            .slot = dst,
            .src = a,
            .src2 = b,
            .is_unsigned = 1,
            .loc = loc,
        }
    };
    return 0;
}

static
int
ci_lower_slice(CiInterpreter* ci, CiLowerCtx* ctx, CcExpr* e, uint32_t dest, CiLowerVal* out){
    int err;
    CcParser* p = &ci->parser;
    CcExpr* base = e->lhs;
    CcTypeKind bk = ccqt_kind(base->type);
    const CcTargetConfig* t = ci_target(ci);
    uint32_t size_size = t->sizeof_[t->size_type];
    uint32_t ptr_size = t->sizeof_[t->intptr_type];

    uint32_t elem_sz;
    err = cc_sizeof_as_uint(p, ccqt_as_slice(e->type)->pointee, e->loc, &elem_sz);
    if(err) return err;
    uint32_t size;
    err = cc_sizeof_as_uint(p, e->type, e->loc, &size);
    if(err) return err;

    err = ci_lower_dest(ctx, &dest, size);
    if(err) return err;
    *out = (CiLowerVal){
        .slot = dest,
        .size = size,
    };
    uint32_t temp = ctx->temp;

    uint32_t base_ptr;
    uint32_t len_slot = 0;
    _Bool have_len = 0;
    if(bk == CC_ARRAY){
        CcArray* arr = ccqt_as_array(base->type);
        CiLowerAddr a;
        err = ci_lower_addr(ci, ctx, base, 0, &a);
        if(err) return err;
        CiLowerVal bv;
        err = ci_addr_to_value(ctx, a, CI_NO_SLOT, ptr_size, e->loc, &bv);
        if(err) return err;
        base_ptr = bv.slot;
        if(!arr->is_incomplete){
            err = ci_emit_const(ci, ctx, arr->length, size_size, e->loc, &len_slot);
            if(err) return err;
            have_len = 1;
        }
    }
    else if(bk == CC_SLICE){
        CiLowerVal sv;
        err = ci_lower_expr(ci, ctx, base, CI_NO_SLOT, &sv); // {count@0, data@ptr_size}
        if(err) return err;
        len_slot = sv.slot;
        base_ptr = sv.slot + ptr_size;
        have_len = 1;
    }
    else if(bk == CC_POINTER){
        CiLowerVal bv;
        err = ci_lower_expr(ci, ctx, base, CI_NO_SLOT, &bv);
        if(err) return err;
        base_ptr = bv.slot;
    }
    else return ci_ice(ci, e->loc, "unhandled type in slice op%s", "");

    // Resolve the low and high indices. lo_is_zero and hi_is_len let us drop
    // both the arithmetic and the redundant checks for the omitted bounds.
    uint32_t lo_slot = 0, hi_slot = 0;
    _Bool lo_is_zero = 0, hi_is_len = 0;
    _Bool lo_signed = 0, hi_signed = 0;
    switch(e->kind){
    case CC_EXPR_SLICE:{
        CiLowerVal iv;
        err = ci_lower_expr(ci, ctx, e->values[0], CI_NO_SLOT, &iv);
        if(err) return err;
        lo_slot = iv.slot;
        lo_signed = !ccqt_is_unsigned(e->values[0]->type, ctx->char_is_unsigned);
        err = ci_lower_expr(ci, ctx, e->values[1], CI_NO_SLOT, &iv);
        if(err) return err;
        hi_slot = iv.slot;
        hi_signed = !ccqt_is_unsigned(e->values[1]->type, ctx->char_is_unsigned);
        break;
    }
    case CC_EXPR_SLICE_LO:{
        CiLowerVal iv;
        err = ci_lower_expr(ci, ctx, e->values[0], CI_NO_SLOT, &iv);
        if(err) return err;
        lo_slot = iv.slot;
        lo_signed = !ccqt_is_unsigned(e->values[0]->type, ctx->char_is_unsigned);
        if(!have_len)
            return ci_ice(ci, e->loc, "slice-from of a pointer without an upper bound%s", "");
        hi_slot = len_slot;
        hi_is_len = 1;
        break;
    }
    case CC_EXPR_SLICE_HI:{
        CiLowerVal iv;
        err = ci_lower_expr(ci, ctx, e->values[0], CI_NO_SLOT, &iv);
        if(err) return err;
        hi_slot = iv.slot;
        hi_signed = !ccqt_is_unsigned(e->values[0]->type, ctx->char_is_unsigned);
        lo_is_zero = 1;
        break;
    }
    case CC_EXPR_CAST:
    case CC_EXPR_SLICE_ALL:
        if(!have_len)
            return ci_ice(ci, e->loc, "slice-all of a pointer without an upper bound%s", "");
        lo_is_zero = 1;
        hi_slot = len_slot;
        hi_is_len = 1;
        break;
    default:
        return ci_ice(ci, e->loc, "not a slice expression%s", "");
    }

    // Per-index upper bound: the real length, or INT64_MAX when there is none
    // (so the only thing rejected is a negative signed index).
    uint32_t max_slot = 0;
    if(!have_len){
        err = ci_emit_const(ci, ctx, size_size==8?(uint64_t)INT64_MAX:(uint64_t)INT32_MAX, size_size, e->loc, &max_slot);
        if(err) return err;
    }
    if(!lo_is_zero){
        if(have_len){
            err = ci_emit_bound(ctx, lo_slot, len_slot, size_size, lo_signed, e->loc);
            if(err) return err;
        }
        else if(lo_signed){
            err = ci_emit_bound(ctx, lo_slot, max_slot, size_size, lo_signed, e->loc);
            if(err) return err;
        }
    }
    if(!hi_is_len){
        if(have_len){
            err = ci_emit_bound(ctx, hi_slot, len_slot, size_size, hi_signed, e->loc);
            if(err) return err;
        }
        else if(hi_signed){
            err = ci_emit_bound(ctx, hi_slot, max_slot, size_size, hi_signed, e->loc);
            if(err) return err;
        }
    }
    if(!lo_is_zero && !hi_is_len){
        // hi >= lo
        err = ci_emit_bound(ctx, lo_slot, hi_slot, size_size, lo_signed, e->loc);
        if(err) return err;
    }

    // count = hi - lo  →  dest[0:8]
    if(lo_is_zero)
        err = ci_emit_copy(ctx, dest, hi_slot, size_size, e->loc);
    else
        err = ci_emit_alu(ctx, CI_ALU_SUB, dest, hi_slot, lo_slot, size_size, e->loc);
    if(err) return err;

    // data = base_ptr + lo*elem_sz  →  dest[8:16]
    if(lo_is_zero){
        err = ci_emit_copy(ctx, dest + ptr_size, base_ptr, size_size, e->loc);
        if(err) return err;
    }
    else {
        uint32_t scaled = lo_slot;
        if(elem_sz != 1){
            uint32_t es;
            err = ci_emit_const(ci, ctx, elem_sz, size_size, e->loc, &es);
            if(err) return err;
            err = ci_alloc_slot(ctx, size_size, size_size, &scaled);
            if(err) return err;
            err = ci_emit_alu(ctx, CI_ALU_MUL, scaled, lo_slot, es, size_size, e->loc);
            if(err) return err;
        }
        err = ci_emit_alu(ctx, CI_ALU_ADD, dest + size_size, base_ptr, scaled, size_size, e->loc);
        if(err) return err;
    }

    ctx->temp = temp;
    return 0;
}

static
int
ci_lower_va(CiInterpreter* ci, CiLowerCtx* ctx, CcExpr* e, uint32_t dest, CiLowerVal*_Nullable out){
    int err;
    switch(e->va.op){
    case CC_VA_START:{
        uint32_t temp = ctx->temp;
        CiLowerVal ap;
        err = ci_lower_expr(ci, ctx, e->lhs, CI_NO_SLOT, &ap);
        if(err) return err;
        CiOp* op;
        err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
        if(err) return err;
        *op = (CiOp){
            .va_start_ = {
                .kind = CI_OP_VA_START,
                .slot = ap.slot,
                .loc = e->loc,
                .target = ci_target(ci)->target,
            },
        };
        ctx->temp = temp;
        return 0;
    }
    case CC_VA_END:
        return 0;
    case CC_VA_ARG:{
        CcParser* p = &ci->parser;
        uint32_t size;
        err = cc_sizeof_as_uint(p, e->type, e->loc, &size);
        if(err) return err;
        err = ci_lower_dest(ctx, &dest, size);
        if(err) return err;
        if(out) out->slot = dest;
        uint32_t temp = ctx->temp;
        CiLowerVal ap;
        err = ci_lower_expr(ci, ctx, e->lhs, CI_NO_SLOT, &ap);
        if(err) return err;
        _Bool is_fp = ccqt_is_basic(e->type) && ccbt_is_float(e->type.basic.kind);
        CiOp* op;
        err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
        if(err) return err;
        *op = (CiOp){
            .va_arg_ = {
                .kind = CI_OP_VA_ARG,
                .is_fp = is_fp,
                .slot = dest,
                .slot_size = size,
                .src = ap.slot,
                .loc = e->loc,
                .target = ci_target(ci)->target,
            },
        };
        ctx->temp = temp;
        return 0;
    }
    case CC_VA_COPY:{
        uint32_t n;
        switch(ci_target(ci)->target){
        case CC_TARGET_AARCH64_MACOS:
        case CC_TARGET_X86_64_WINDOWS:
        case CC_TARGET_TEST:
            n = sizeof(void*);
            break;
        case CC_TARGET_X86_64_LINUX:
        case CC_TARGET_X86_64_MACOS:
            n = sizeof(CiSysvVaListTag);
            break;
        case CC_TARGET_AARCH64_LINUX:
            n = sizeof(CiAapcs64VaList);
            break;
        case CC_TARGET_COUNT:
            return ci_error(ci, e->loc, "va_copy: unsupported target");
        DRP_CASES_EXHAUSTED;
        }
        uint32_t temp = ctx->temp;
        CiLowerVal dst_ap, src_ap;
        err = ci_lower_expr(ci, ctx, e->lhs, CI_NO_SLOT, &dst_ap);
        if(err) return err;
        err = ci_lower_expr(ci, ctx, e->values[0], CI_NO_SLOT, &src_ap);
        if(err) return err;
        CiOp* op;
        err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
        if(err) return err;
        *op = (CiOp){
            .memcopy = {
                .kind = CI_OP_MEMCOPY,
                .size = n,
                .slot = dst_ap.slot,
                .offset = 0,
                .src = src_ap.slot,
                .src_offset = 0,
                .loc = e->loc,
            },
        };
        ctx->temp = temp;
        return 0;
    }
    }
    return ci_error(ci, e->loc, "unsupported va operation");
}

// Lower an expression for side effects only.
static
int
ci_lower_expr_discard(CiInterpreter* ci, CiLowerCtx* ctx, CcExpr* e){
    int err;
    switch(e->kind){
        case CC_EXPR_VALUE:
            return 0;
        case CC_EXPR_VARIABLE:
            // FIXME: volatile reads
            return 0;
        case CC_EXPR_SIZEOF_VMT:
            return ci_unimplemented(ci, e->loc, "sizeof(vmt)");
        case CC_EXPR_FUNCTION:
            return 0;
        case CC_EXPR_COMPOUND_LITERAL:
        case CC_EXPR_INIT_LIST:{
            CcInitList *l = e->init_list;
            for(size_t i = 0; i < l->count; i++){
                err = ci_lower_expr_discard(ci, ctx, l->entries[i].value);
                if(err) return err;
            }
            return 0;
        }
        case CC_EXPR_NEG:
        case CC_EXPR_POS:
        case CC_EXPR_BITNOT:
        case CC_EXPR_LOGNOT:
        case CC_EXPR_DEREF:
        case CC_EXPR_ADDR:
        case CC_EXPR_CAST:
        case CC_EXPR_POPCOUNT:
        case CC_EXPR_CLZ:
        case CC_EXPR_CTZ:
        case CC_EXPR_ALLOCA:
        case CC_EXPR_SLICE_ALL:
        case CC_EXPR_BSWAP:
            err = ci_lower_expr_discard(ci, ctx, e->lhs);
            return err;
        case CC_EXPR_INTERN:
        case CC_EXPR_COMPILE:
            // idk maybe these should be treated as having side effects?
            err = ci_lower_expr_discard(ci, ctx, e->lhs);
            return err;
        case CC_EXPR_PREINC:
        case CC_EXPR_PREDEC:
        case CC_EXPR_POSTINC:
        case CC_EXPR_POSTDEC:
            return ci_lower_incdec(ci, ctx, e, CI_NO_SLOT, NULL);
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
        case CC_EXPR_EQ:
        case CC_EXPR_NE:
        case CC_EXPR_LT:
        case CC_EXPR_GT:
        case CC_EXPR_LE:
        case CC_EXPR_GE:
        case CC_EXPR_SUBSCRIPT:
        case CC_EXPR_SLICE_LO:
        case CC_EXPR_SLICE_HI:
            err = ci_lower_expr_discard(ci, ctx, e->lhs);
            if(err) return err;
            err = ci_lower_expr_discard(ci, ctx, e->values[0]);
            return err;
        case CC_EXPR_LOGAND:
        case CC_EXPR_LOGOR:{
            uint32_t temp = ctx->temp;
            uint32_t chain = 0;
            err = ci_lower_branch(ci, ctx, e->lhs, e->kind == CC_EXPR_LOGOR, e->loc, &chain);
            if(err) return err;
            ctx->temp = temp;
            err = ci_lower_expr_discard(ci, ctx, e->values[0]);
            if(err) return err;
            ci_patch_branches(ctx, chain, (uint32_t)ctx->out->count);
            return 0;
        }

        case CC_EXPR_ASSIGN:{
            _Bool handled;
            err = ci_lower_assign_direct(ci, ctx, e, &handled);
            if(err) return err;
            if(handled) return 0;
            CiLowerVal v;
            return ci_lower_expr(ci, ctx, e, CI_NO_SLOT, &v);
        }
        case CC_EXPR_ADDASSIGN:
        case CC_EXPR_SUBASSIGN:
        case CC_EXPR_MULASSIGN:
        case CC_EXPR_DIVASSIGN:
        case CC_EXPR_MODASSIGN:
        case CC_EXPR_BITANDASSIGN:
        case CC_EXPR_BITORASSIGN:
        case CC_EXPR_BITXORASSIGN:
        case CC_EXPR_LSHIFTASSIGN:
        case CC_EXPR_RSHIFTASSIGN:{
            CiLowerVal v;
            return ci_lower_expr(ci, ctx, e, CI_NO_SLOT, &v);
        }
        case CC_EXPR_TERNARY:{
            _Bool truth;
            int folded = ci_fold_condition(ci, ctx, e->lhs, &truth);
            if(folded > 0) return folded;
            if(folded == 0)
                return ci_lower_expr_discard(ci, ctx, e->values[truth ? 0 : 1]);
            uint32_t temp = ctx->temp;
            CiOp* op;
            uint32_t chain = 0;
            err = ci_lower_branch(ci, ctx, e->lhs, 0, e->loc, &chain);
            if(err) return err;
            ctx->temp = temp;
            err = ci_lower_expr_discard(ci, ctx, e->values[0]);
            if(err) return err;
            err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
            if(err) return err;
            *op = (CiOp){
                .jump = {
                    .kind = CI_OP_JUMP,
                    .loc = e->loc,
                }
            };
            ci_patch_branches(ctx, chain, (uint32_t)ctx->out->count);
            ptrdiff_t jump = (char*)&op->jump.jump - (char*)ctx->out->data;
            err = ci_lower_expr_discard(ci, ctx, e->values[1]);
            if(err) return err;
            *(uint32_t*)((char*)ctx->out->data+jump) = (uint32_t)ctx->out->count;
            return 0;
        }
        case CC_EXPR_CALL:
            return ci_lower_call(ci, ctx, e, CI_NO_SLOT, NULL);
        case CC_EXPR_DOT:
        case CC_EXPR_ARROW:
            err = ci_lower_expr_discard(ci, ctx, e->values[0]);
            return err;
        case CC_EXPR_COMMA:{
            err = ci_lower_expr_discard(ci, ctx, e->lhs);
            if(err) return err;
            return ci_lower_expr_discard(ci, ctx, e->values[0]);
        }
        case CC_EXPR_STATEMENT_EXPRESSION:
            err = ci_lower_stmt(ci, ctx, e->stmt_body);
            return err;
        case CC_EXPR_ATOMIC:
            return ci_lower_atomic_builtin(ci, ctx, e, CI_NO_SLOT, NULL);
        case CC_EXPR_VA:
            return ci_lower_va(ci, ctx, e, CI_NO_SLOT, NULL);
        case CC_EXPR_BUILTIN:{
            CiOp* op;
            err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
            if(err) return err;
            *op = (CiOp){
                .builtin = {
                    .kind = CI_OP_BUILTIN,
                    .op = e->builtin.op,
                    .loc = e->loc,
                },
            };
            return 0;
        }
        case CC_EXPR_ADD_OVERFLOW:
        case CC_EXPR_MUL_OVERFLOW:
        case CC_EXPR_SUB_OVERFLOW:
            return ci_lower_checked(ci, ctx, e, CI_NO_SLOT, NULL);
        case CC_EXPR_UMUL128:
            return ci_lower_umul128(ci, ctx, e, CI_NO_SLOT, NULL);
        case CC_EXPR_SLICE:
            err = ci_lower_expr_discard(ci, ctx, e->lhs);
            if(err) return err;
            err = ci_lower_expr_discard(ci, ctx, e->values[0]);
            if(err) return err;
            err = ci_lower_expr_discard(ci, ctx, e->values[1]);
            return err;
        case CC_EXPR_HOTSWAP:
            return ci_lower_rt_call(ci, ctx, e, CI_RT_HOTSWAP, CI_NO_SLOT, NULL);
        case CC_EXPR_MODULE_REFLECT:
        case CC_EXPR_TYPE_INTROSPECTION:
            return ci_lower_reflect(ci, ctx, e, CI_NO_SLOT, NULL);
        DRP_CASES_EXHAUSTED;
    }
    return ci_unreachable(ci, e->loc, "unhandled discarded expression kind");
}

static
int
ci_lower_dest(CiLowerCtx* ctx, uint32_t* dest, uint32_t size){
    if(*dest != CI_NO_SLOT) return 0;
    return ci_alloc_slot(ctx, size, size, dest);
}

static
int
ci_addr_to_value(CiLowerCtx* ctx, CiLowerAddr a, uint32_t dest, uint32_t size, SrcLoc loc, CiLowerVal* out){
    int err;
    out->size = size;
    out->canonical = 0;
    if(a.disp == 0 && dest == CI_NO_SLOT){
        // the base pointer slot already holds the value
        out->slot = a.slot;
        return 0;
    }
    err = ci_lower_dest(ctx, &dest, size);
    if(err) return err;
    out->slot = dest;
    CiOp* op;
    if(a.disp == 0){
        err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
        if(err) return err;
        *op = (CiOp){
            .copy = {
                .kind = CI_OP_COPY,
                .slot = dest,
                .slot_size = size,
                .src = a.slot,
                .src_size = size,
                .loc = loc,
            }
        };
        return 0;
    }
    if(size == 4 || size == 8){
        err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
        if(err) return err;
        *op = (CiOp){.alu_imm = {
            .kind = size == 4 ? CI_OP_ALU_IMM32 : CI_OP_ALU_IMM64,
            .slot = dest, .src = a.slot, .immediate = a.disp,
            .op = CI_ALU_ADD, .is_unsigned = 1, .loc = loc,
        }};
        return 0;
    }
    // dest = a.slot + a.disp; the displacement temp recycles at statement end
    uint32_t cslot;
    err = ci_alloc_slot(ctx, 8, 8, &cslot);
    if(err) return err;
    err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
    if(err) return err;
    *op = (CiOp){
        .constant = {
            .kind = CI_OP_CONST,
            .bt_kind = CCBT_unsigned_long_long, // XXX
            .slot = cslot,
            .immsize = 8,
            .immediate = {a.disp},
            .loc = loc,
        }
    };
    err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
    if(err) return err;
    *op = (CiOp){
        .alu = {
            .kind = CI_OP_ALU64,
            .slot = dest,
            .src = a.slot,
            .src2 = cslot,
            .op = CI_ALU_ADD,
            .is_unsigned = 1,
            .loc = loc,
        }
    };
    return 0;
}

static
int
ci_lower_bitfield_addr(CiInterpreter* ci, CiLowerCtx* ctx, CcExpr* lv, CiLowerAddr* out){
    int err;
    if(lv->kind == CC_EXPR_ARROW){
        CiLowerVal v;
        err = ci_lower_expr(ci, ctx, lv->values[0], CI_NO_SLOT, &v);
        if(err) return err;
        out->slot = v.slot;
        out->disp = (uint32_t)lv->field_loc.byte_offset;
        return 0;
    }
    // DOT: the storage unit is at the base's address plus the member's offset
    err = ci_lower_addr(ci, ctx, lv->values[0], 0, out);
    if(err) return err;
    out->disp += (uint32_t)lv->field_loc.byte_offset;
    return 0;
}

static
int
ci_emit_load_bitfield(CiInterpreter* ci, CiLowerCtx* ctx, const CcExpr* lv, CiLowerAddr a, uint32_t dest, uint32_t size){
    (void)ci;
    CiOp* op;
    int err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
    if(err) return err;
    *op = (CiOp){
        .load_bf = {
            .kind = CI_OP_LOAD_BITFIELD,
            .slot = dest,
            .slot_size = size,
            .src = a.slot,
            .offset = a.disp,
            .bit_offset = lv->field_loc.bit_offset,
            .bit_width = lv->field_loc.bit_width,
            .is_signed = !ccqt_is_unsigned(lv->type, ctx->char_is_unsigned),
            .loc = lv->loc,
        }
    };
    return 0;
}

static
int
ci_emit_store_bitfield(CiLowerCtx* ctx, const CcExpr* lv, CiLowerAddr a, uint32_t src, uint32_t size){
    CiOp* op;
    int err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
    if(err) return err;
    *op = (CiOp){
        .store_bf = {
            .kind = CI_OP_STORE_BITFIELD,
            .slot = a.slot,
            .src = src,
            .src_size = size,
            .offset = a.disp,
            .bit_offset = lv->field_loc.bit_offset,
            .bit_width = lv->field_loc.bit_width,
            .loc = lv->loc,
        }
    };
    return 0;
}

static
_Bool
ci_frame_lvalue(const CcExpr* lv, uint32_t* offset){
    switch((uint32_t)lv->kind){
        case CC_EXPR_VARIABLE:
            if(!lv->var->automatic) return 0;
            if(lv->type.is_atomic) return 0;
            *offset = (uint32_t)lv->var->frame_offset;
            return 1;
        case CC_EXPR_DOT:{
            if(lv->field_loc.bit_width) return 0;
            if(lv->type.is_atomic) return 0;
            uint32_t base;
            if(!ci_frame_lvalue(lv->values[0], &base)) return 0;
            *offset = base + (uint32_t)lv->field_loc.byte_offset;
            return 1;
        }
        default:
            return 0;
    }
}

static
int
ci_lower_addr(CiInterpreter* ci, CiLowerCtx* ctx, CcExpr* lv, _Bool one_past_ok, CiLowerAddr* out){
    int err;
    CcParser* p = &ci->parser;
    uint32_t frame_off;
    if(ci_frame_lvalue(lv, &frame_off)){
        // a local (or a member chain of one) lives in a slot
        uint32_t aslot;
        err = ci_alloc_slot(ctx, 8, 8, &aslot);
        if(err) return err;
        CiOp* op;
        err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
        if(err) return err;
        *op = (CiOp){
            .slot_addr = {
                .kind = CI_OP_SLOT_ADDR,
                .slot = aslot,
                .slot_size = 8,
                .src = frame_off,
                .loc = lv->loc,
            }
        };
        out->slot = aslot;
        out->disp = 0;
        return 0;
    }
    switch((uint32_t)lv->kind){
        case CC_EXPR_VALUE:{
            if(ccqt_kind(lv->type) != CC_ARRAY)
                break; // not an lvalue; materialized below
            uint32_t aslot;
            err = ci_alloc_slot(ctx, 8, 8, &aslot);
            if(err) return err;
            CiOp* op;
            err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
            if(err) return err;
            CcArray* arr = ccqt_as_array(lv->type);
            *op = (CiOp){
                .constant = {
                    .kind = CI_OP_CONST,
                    .bt_kind = (uint32_t)(ccqt_is_basic(arr->element)?arr->element.basic.kind:CCBT_nullptr_t),
                    .is_anon_array = 1,
                    .immsize = 8,
                    .slot = aslot,
                    .immediate = {
                        (uint64_t)lv->text,
                        lv->str.length,
                    },
                    .loc = lv->loc,
                },
            };
            out->slot = aslot;
            out->disp = 0;
            return 0;
        }
        case CC_EXPR_VARIABLE:{
            CcVariable* var = lv->var;
            if(var->automatic){
                // atomic locals miss the frame fast path above, but their
                // storage is a slot all the same
                uint32_t aslot;
                err = ci_alloc_slot(ctx, 8, 8, &aslot);
                if(err) return err;
                CiOp* op;
                err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
                if(err) return err;
                *op = (CiOp){
                    .slot_addr = {
                        .kind = CI_OP_SLOT_ADDR,
                        .slot = aslot,
                        .slot_size = 8,
                        .src = (uint32_t)var->frame_offset,
                        .loc = lv->loc,
                    }
                };
                out->slot = aslot;
                out->disp = 0;
                return 0;
            }
            uint32_t aslot;
            err = ci_alloc_slot(ctx, 8, 8, &aslot);
            if(err) return err;
            CiOp* op;
            err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
            if(err) return err;
            *op = (CiOp){
                .var_addr = {
                    .kind = CI_OP_VAR_ADDR,
                    .slot = aslot,
                    .slot_size = 8,
                    .var = var,
                    .loc = lv->loc,
                }
            };
            out->slot = aslot;
            out->disp = 0;
            return 0;
        }
        case CC_EXPR_DEREF:{
            CiLowerVal v;
            err = ci_lower_expr(ci, ctx, lv->lhs, CI_NO_SLOT, &v);
            if(err) return err;
            out->slot = v.slot;
            out->disp = 0;
            return 0;
        }
        case CC_EXPR_ARROW:{
            if(lv->field_loc.bit_width)
                return ci_unreachable(ci, lv->loc, "addr of bitfield");
            CiLowerVal v;
            err = ci_lower_expr(ci, ctx, lv->values[0], CI_NO_SLOT, &v);
            if(err) return err;
            out->slot = v.slot;
            out->disp = (uint32_t)lv->field_loc.byte_offset;
            return 0;
        }
        case CC_EXPR_DOT:{
            if(lv->field_loc.bit_width)
                return ci_unreachable(ci, lv->loc, "addr of bitfield");
            CcExpr* base = lv->values[0];
            if(base->is_lvalue)
                err = ci_lower_addr(ci, ctx, base, 0, out); // base must be a valid object
            else
                err = ci_lower_materialize_addr(ci, ctx, base, out);
            if(err) return err;
            out->disp += (uint32_t)lv->field_loc.byte_offset;
            return 0;
        }
        case CC_EXPR_COMMA:{
            err = ci_lower_expr_discard(ci, ctx, lv->lhs);
            if(err) return err;
            return ci_lower_addr(ci, ctx, lv->values[0], one_past_ok, out);
        }
        case CC_EXPR_SUBSCRIPT:{
            CcExpr* base = lv->lhs;
            CcExpr* idx = lv->values[0];
            CcTypeKind bk = ccqt_kind(base->type);
            uint32_t idx_sz;
            err = cc_sizeof_as_uint(p, idx->type, idx->loc, &idx_sz);
            if(err) return err;
            if(idx_sz > 8 || !ccqt_is_integer(idx->type))
                return ci_ice(ci, idx->loc, "invalid index type%s", "");
            uint32_t elem_sz;
            err = cc_sizeof_as_uint(p, lv->type, lv->loc, &elem_sz);
            if(err) return err;
            CiOp* op;
            CiFoldValue index;
            int fold_fail = ci_fold_expr(ci, ctx, idx, &index);
            if(fold_fail > 0) return fold_fail;
            uint32_t base_temp = ctx->temp;
            uint32_t base_ptr;     // slot holding an 8-byte base pointer
            uint32_t base_disp = 0;// offset folded into the result displacement
            _Bool do_check = 0;
            uint32_t len_slot = 0;
            if(bk == CC_POINTER){
                CiLowerVal b;
                uint32_t base_dest = CI_NO_SLOT;
                if(fold_fail == 0){
                    // Preserve the evaluated address if a later RHS changes
                    // a local pointer whose storage ci_lower_expr could reuse.
                    err = ci_alloc_slot(ctx, ctx->ptr_size, ctx->ptr_size, &base_dest);
                    if(err) return err;
                }
                err = ci_lower_expr(ci, ctx, base, base_dest, &b);
                if(err) return err;
                base_ptr = b.slot;
            }
            else if(bk == CC_ARRAY){
                CcArray* arr = ccqt_as_array(base->type);
                CiLowerAddr ba;
                err = ci_lower_addr(ci, ctx, base, 0, &ba);
                if(err) return err;
                base_ptr = ba.slot;
                base_disp = ba.disp;
                // Elide the check only for genuine flexible-array-member idioms:
                // a C99 FLA (incomplete), a zero-length member, or a length-1
                // member at the end of a struct (the struct hack).
                _Bool skip = arr->is_incomplete;
                if(!skip && arr->length <= 1
                    && (base->kind == CC_EXPR_DOT || base->kind == CC_EXPR_ARROW)){
                    if(arr->length == 0){
                        skip = 1;
                    }
                    else {
                        // length 1: skip only if it is the struct's last field
                        CcQualType st = base->values[0]->type;
                        if(base->kind == CC_EXPR_ARROW && ccqt_kind(st) == CC_POINTER)
                            st = ccqt_as_ptr(st)->pointee;
                        if(ccqt_kind(st) == CC_STRUCT){
                            CcStruct* s = ccqt_as_struct(st);
                            if(s->field_count && s->fields
                                && s->fields[s->field_count-1].offset == (uint32_t)base->field_loc.byte_offset)
                                skip = 1;
                        }
                    }
                }
                if(!skip && fold_fail == 0){
                    _Bool negative = !ccqt_is_unsigned(idx->type, ctx->char_is_unsigned) && ci_read_int(index.bits, index.sz) < 0;
                    uint64_t i = ci_read_uint(index.bits, index.sz);
                    // Invalid constant accesses must still fail at runtime,
                    // including in code reached by a goto or a later call.
                    skip = !negative && (i < arr->length || (one_past_ok && i == arr->length));
                }
                if(!skip){
                    err = ci_alloc_slot(ctx, ctx->size_size, ctx->size_size, &len_slot);
                    if(err) return err;
                    err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
                    if(err) return err;
                    *op = (CiOp){
                        .constant = {
                            .kind = CI_OP_CONST,
                            .bt_kind = (uint32_t)ci_target(ci)->size_type,
                            .slot = len_slot,
                            .immsize = ctx->size_size,
                            .immediate = {arr->length},
                            .loc = lv->loc,
                        }
                    };
                    do_check = 1;
                }
            }
            else if(bk == CC_SLICE){
                CiLowerVal sv;
                err = ci_lower_expr(ci, ctx, base, CI_NO_SLOT, &sv); // {count@0, data@8}
                if(err) return err;
                base_ptr = sv.slot + ctx->ptr_size; // .data
                len_slot = sv.slot;     // .count
                do_check = 1;
            }
            else {
                return ci_unimplemented(ci, base->loc, "exotic base");
            }
            if(fold_fail == 0 && !do_check && (ctx->ptr_size == 4 || ctx->ptr_size == 8)){
                if(base_ptr < base_temp){
                    // An array member can use a local pointer as its base too.
                    uint32_t snapshot;
                    err = ci_alloc_slot(ctx, ctx->ptr_size, ctx->ptr_size, &snapshot);
                    if(err) return err;
                    err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
                    if(err) return err;
                    *op = (CiOp){.copy = {
                        .kind = CI_OP_COPY, .slot = snapshot, .src = base_ptr,
                        .slot_size = ctx->ptr_size, .src_size = ctx->ptr_size, .loc = lv->loc,
                    }};
                    base_ptr = snapshot;
                }
                uint64_t offset = ci_fold_offset(ctx, &index, elem_sz);
                if(offset <= UINT32_MAX - base_disp){
                    out->slot = base_ptr;
                    out->disp = base_disp + (uint32_t)offset;
                    return 0;
                }
                uint32_t addr;
                err = ci_alloc_slot(ctx, ctx->ptr_size, ctx->ptr_size, &addr);
                if(err) return err;
                err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
                if(err) return err;
                *op = (CiOp){.alu_imm = {
                    .kind = ctx->ptr_size == 4 ? CI_OP_ALU_IMM32 : CI_OP_ALU_IMM64,
                    .slot = addr, .src = base_ptr, .immediate = offset,
                    .op = CI_ALU_ADD, .is_unsigned = 1, .loc = lv->loc,
                }};
                out->slot = addr;
                out->disp = base_disp;
                return 0;
            }
            CcExpr* index_expr = idx;
            if(!do_check && idx->kind == CC_EXPR_CAST && ccqt_is_integer(idx->lhs->type)){
                uint32_t src_size;
                err = cc_sizeof_as_uint(p, idx->lhs->type, idx->loc, &src_size);
                if(err) return err;
                if(src_size < idx_sz)
                    index_expr = idx->lhs;
            }
            CiLowerVal iv;
            err = ci_lower_expr(ci, ctx, index_expr, CI_NO_SLOT, &iv);
            if(err) return err;
            _Bool idx_unsigned = ccqt_is_unsigned(index_expr->type, ctx->char_is_unsigned);
            uint32_t widx = iv.slot;
            if(do_check && iv.size != ctx->size_size)
                return ci_ice(ci, idx->loc, "invalid index size %u", iv.size);
            if(do_check){
                err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
                if(err) return err;
                *op = (CiOp){
                    .bounds = {
                        .kind = CI_OP_BOUNDS,
                        .src = widx,
                        .src_size = ctx->size_size,
                        .src2 = len_slot,
                        .src2_size = ctx->size_size,
                        .inclusive = one_past_ok,
                        .index_signed = !idx_unsigned,
                        .loc = lv->loc,
                    }
                };
            }
            uint32_t addr;
            err = ci_alloc_slot(ctx, ctx->ptr_size, ctx->ptr_size, &addr);
            if(err) return err;
            err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
            if(err) return err;
            *op = (CiOp){
                .index = {
                    .kind = CI_OP_INDEX,
                    .ptr_size = ctx->ptr_size,
                    .index_size = iv.size,
                    .index_unsigned = idx_unsigned,
                    .slot = addr,
                    .base = base_ptr,
                    .index = widx,
                    .scale = elem_sz,
                    .loc = lv->loc,
                }
            };
            out->slot = addr;
            out->disp = base_disp;
            return 0;
        }
        default:
            break;
    }
    if(!lv->is_lvalue)
        // no storage anywhere: the materialized value is the object
        return ci_lower_materialize_addr(ci, ctx, lv, out);
    return ci_ice(ci, lv->loc, "unhandled lvalue shape in lowering%s", "");
}

static
int
ci_lower_materialize_addr(CiInterpreter* ci, CiLowerCtx* ctx, CcExpr* e, CiLowerAddr* out){
    CiLowerVal v;
    int err = ci_lower_expr(ci, ctx, e, CI_NO_SLOT, &v);
    if(err) return err;
    uint32_t aslot;
    err = ci_alloc_slot(ctx, 8, 8, &aslot);
    if(err) return err;
    CiOp* op;
    err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
    if(err) return err;
    *op = (CiOp){
        .slot_addr = {
            .kind = CI_OP_SLOT_ADDR,
            .slot = aslot,
            .slot_size = 8,
            .src = v.slot,
            .loc = e->loc,
        }
    };
    out->slot = aslot;
    out->disp = 0;
    return 0;
}

static
_Bool
ci_falu_type(CcQualType t){
    if(!ccqt_is_basic(t)) return 0;
    CcBasicTypeKind k = t.basic.kind;
    return k == CCBT_float || k == CCBT_double;
}

static
_Bool
ci_falu_op_for(CcExprKind kind, CiFaluOp* out){
    switch((uint32_t)kind){
        case CC_EXPR_ADD: case CC_EXPR_ADDASSIGN: *out = CI_FALU_ADD; return 1;
        case CC_EXPR_SUB: case CC_EXPR_SUBASSIGN: *out = CI_FALU_SUB; return 1;
        case CC_EXPR_MUL: case CC_EXPR_MULASSIGN: *out = CI_FALU_MUL; return 1;
        case CC_EXPR_DIV: case CC_EXPR_DIVASSIGN: *out = CI_FALU_DIV; return 1;
        default: return 0;
    }
}

static
CiAluOp
ci_alu_op_for(CcExprKind kind){
    switch((uint32_t)kind){
        case CC_EXPR_ADD:    return CI_ALU_ADD;
        case CC_EXPR_SUB:    return CI_ALU_SUB;
        case CC_EXPR_MUL:    return CI_ALU_MUL;
        case CC_EXPR_DIV:    return CI_ALU_DIV;
        case CC_EXPR_MOD:    return CI_ALU_MOD;
        case CC_EXPR_BITAND: return CI_ALU_AND;
        case CC_EXPR_BITOR:  return CI_ALU_OR;
        case CC_EXPR_BITXOR: return CI_ALU_XOR;
        case CC_EXPR_LSHIFT: return CI_ALU_SHL;
        case CC_EXPR_RSHIFT: return CI_ALU_SHR;
        case CC_EXPR_ADDASSIGN:    return CI_ALU_ADD;
        case CC_EXPR_SUBASSIGN:    return CI_ALU_SUB;
        case CC_EXPR_MULASSIGN:    return CI_ALU_MUL;
        case CC_EXPR_DIVASSIGN:    return CI_ALU_DIV;
        case CC_EXPR_MODASSIGN:    return CI_ALU_MOD;
        case CC_EXPR_BITANDASSIGN: return CI_ALU_AND;
        case CC_EXPR_BITORASSIGN:  return CI_ALU_OR;
        case CC_EXPR_BITXORASSIGN: return CI_ALU_XOR;
        case CC_EXPR_LSHIFTASSIGN: return CI_ALU_SHL;
        case CC_EXPR_RSHIFTASSIGN: return CI_ALU_SHR;
    }
    return CI_ALU_ADD; // unreachable: callers only pass the kinds above
}

static
CiCmpOp
ci_cmp_op_for(CcExprKind kind){
    switch((uint32_t)kind){
        case CC_EXPR_EQ: return CI_CMP_EQ;
        case CC_EXPR_NE: return CI_CMP_NE;
        case CC_EXPR_LT: return CI_CMP_LT;
        case CC_EXPR_GT: return CI_CMP_GT;
        case CC_EXPR_LE: return CI_CMP_LE;
        case CC_EXPR_GE: return CI_CMP_GE;
    }
    return CI_CMP_EQ; // unreachable: callers only pass the kinds above
}

static
CiOpKind
ci_int_op_kind(uint32_t size){
    switch(size){
        case 1: return CI_OP_ALU8;
        case 2: return CI_OP_ALU16;
        case 4: return CI_OP_ALU32;
        case 8: return CI_OP_ALU64;
        case 16: return CI_OP_ALU128;
    }
    return 0;
}

static
CiOpKind
ci_cmp_op_kind(uint32_t size){
    switch(size){
        case 4: return CI_OP_CMP32;
        case 8: return CI_OP_CMP64;
        case 16: return CI_OP_CMP128;
    }
    return 0;
}

static
int
ci_lower_branch_leaf(CiInterpreter* ci, CiLowerCtx* ctx, CcExpr* cond, _Bool when_true, SrcLoc loc, CiOp*_Nonnull*_Nonnull out){
    CiLowerVal v;
    int err = ci_lower_cond(ci, ctx, cond, &v);
    if(err) return err;
    switch((uint32_t)cond->kind){
        case CC_EXPR_EQ: case CC_EXPR_NE:
        case CC_EXPR_LT: case CC_EXPR_GT:
        case CC_EXPR_LE: case CC_EXPR_GE:{
            CiOp* tail = &ma_tail(*ctx->out);
            if((tail->kind == CI_OP_CMP32 || tail->kind == CI_OP_CMP64)
                && tail->cmp.slot == v.slot){
                CiOp cmp = *tail;
                *tail = (CiOp){
                    .cmp_jump = {
                        .kind = cmp.kind == CI_OP_CMP32 ? CI_OP_CMP_JUMP32 : CI_OP_CMP_JUMP64,
                        .op = cmp.cmp.op,
                        .is_unsigned = cmp.cmp.is_unsigned,
                        .when_true = when_true,
                        .src = cmp.cmp.src,
                        .src2 = cmp.cmp.src2,
                        .loc = loc,
                    }
                };
                *out = tail;
                return 0;
            }
            break;
        }
    }
    CiOp* op;
    err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
    if(err) return err;
    *op = (CiOp){
        .jump_false = {
            .kind = when_true ? CI_OP_JUMP_TRUE : CI_OP_JUMP_FALSE,
            .slot = v.slot,
            .slot_size = v.size,
            .loc = loc,
        }
    };
    *out = op;
    return 0;
}

// Until patched, branch targets form a list of instruction indices plus one.
// Using indices keeps the list valid when the opcode array grows.
static
void
ci_patch_branches(CiLowerCtx* ctx, uint32_t chain, uint32_t target){
    while(chain){
        CiOp* op = &ctx->out->data[chain - 1];
        uint32_t next = op->jump_false.jump;
        op->jump_false.jump = target;
        chain = next;
    }
}

static
int
ci_lower_branch(CiInterpreter* ci, CiLowerCtx* ctx, CcExpr* cond, _Bool when_true, SrcLoc loc, uint32_t* chain){
    int err;
    _Bool truth;
    int folded = ci_fold_condition(ci, ctx, cond, &truth);
    if(folded > 0) return folded;
    if(folded == 0){
        if(truth != when_true) return 0;
        CiOp* op;
        err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
        if(err) return err;
        *op = (CiOp){.jump = {.kind = CI_OP_JUMP, .jump = *chain, .loc = loc}};
        *chain = (uint32_t)(op - ctx->out->data) + 1;
        return 0;
    }
    if(cond->kind == CC_EXPR_LOGNOT)
        return ci_lower_branch(ci, ctx, cond->lhs, !when_true, loc, chain);
    if(cond->kind == CC_EXPR_COMMA){
        err = ci_lower_expr_discard(ci, ctx, cond->lhs);
        if(err) return err;
        return ci_lower_branch(ci, ctx, cond->values[0], when_true, loc, chain);
    }
    if(cond->kind == CC_EXPR_LOGAND || cond->kind == CC_EXPR_LOGOR){
        _Bool short_circuit = cond->kind == CC_EXPR_LOGOR;
        if(when_true == short_circuit){
            err = ci_lower_branch(ci, ctx, cond->lhs, when_true, loc, chain);
            if(err) return err;
            return ci_lower_branch(ci, ctx, cond->values[0], when_true, loc, chain);
        }
        uint32_t skip = 0;
        err = ci_lower_branch(ci, ctx, cond->lhs, short_circuit, loc, &skip);
        if(err) return err;
        err = ci_lower_branch(ci, ctx, cond->values[0], when_true, loc, chain);
        if(err) return err;
        ci_patch_branches(ctx, skip, (uint32_t)ctx->out->count);
        return 0;
    }
    uint32_t temp = ctx->temp;
    CiOp* op;
    err = ci_lower_branch_leaf(ci, ctx, cond, when_true, loc, &op);
    if(err) return err;
    op->jump_false.jump = *chain;
    *chain = (uint32_t)(op - ctx->out->data) + 1;
    ctx->temp = temp;
    return 0;
}

static
int
ci_lower_cond(CiInterpreter* ci, CiLowerCtx* ctx, CcExpr* cond, CiLowerVal* out){
    int err = ci_lower_expr(ci, ctx, cond, CI_NO_SLOT, out);
    if(err) return err;
    if(out->canonical) return 0;
    if((ccqt_is_integer(cond->type) || ccqt_kind(cond->type) == CC_POINTER)
        && (out->size == 1 || out->size == 2 || out->size == 4 || out->size == 8))
        return 0;
    uint32_t slot;
    err = ci_alloc_slot(ctx, 1, 1, &slot);
    if(err) return err;
    err = ci_lower_istrue(ctx, out, cond->type, slot, 1, 0, cond->loc);
    if(err) return err;
    *out = (CiLowerVal){
        .slot = slot,
        .size = 1,
        .canonical = 1,
    };
    return 0;
}

static
int
ci_lower_istrue(CiLowerCtx* ctx, const CiLowerVal* v, CcQualType src_type, uint32_t dest, uint32_t dest_size, _Bool negate, SrcLoc loc){
    uint32_t float_kind = 0;
    if(ccqt_is_basic(src_type) && ccbt_is_float(src_type.basic.kind))
        float_kind = (uint32_t)src_type.basic.kind;
    CiOp* op;
    int err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
    if(err) return err;
    *op = (CiOp){
        .istrue = {
            .kind = CI_OP_ISTRUE,
            .slot = dest,
            .slot_size = dest_size,
            .src = v->slot,
            .src_size = v->size,
            .float_kind = float_kind,
            .negate = negate,
            .loc = loc,
        }
    };
    return 0;
}

static
int
ci_alloc_slot(CiLowerCtx* ctx, uint32_t sz, uint32_t align, uint32_t* slot){
    uint32_t frm = ctx->temp;
    if(add_overflow(frm, align - 1, &frm))
        return _cc_overflow_error;
    frm &= ~(align - 1);
    uint32_t new_frm;
    if(add_overflow(frm, sz, &new_frm))
        return _cc_overflow_error;
    *slot = frm;
    ctx->temp = new_frm;
    if(new_frm > *ctx->frame_size)
        *ctx->frame_size = new_frm;
    return 0;
}

static
void
ci_backpatch_break_continue(CiLowerCtx* ctx, size_t start, uint32_t break_target, uint32_t continue_target){
    for(size_t i = start; i < ctx->backpatches.count; i++){
        CiBackpatchTarget* t = &ctx->backpatches.data[i];
        if(t->kind == CI_BP_BREAK){
            *(uint32_t*)((char*)ctx->out->data+t->byteoffset) = break_target;
            t->kind = CI_BP_NONE;
        }
        else if(t->kind == CI_BP_CONTINUE){
            *(uint32_t*)((char*)ctx->out->data+t->byteoffset) = continue_target;
            t->kind = CI_BP_NONE;
        }
    }
    while(ctx->backpatches.count > start){
        if(ctx->backpatches.data[ctx->backpatches.count-1].kind == CI_BP_NONE)
            ctx->backpatches.count--;
        else
            break;
    }
}

static
void
ci_backpatch_break(CiLowerCtx* ctx, size_t start, uint32_t break_target){
    for(size_t i = start; i < ctx->backpatches.count; i++){
        CiBackpatchTarget* t = &ctx->backpatches.data[i];
        if(t->kind == CI_BP_BREAK){
            *(uint32_t*)((char*)ctx->out->data+t->byteoffset) = break_target;
            t->kind = CI_BP_NONE;
        }
    }
    while(ctx->backpatches.count > start){
        if(ctx->backpatches.data[ctx->backpatches.count-1].kind == CI_BP_NONE)
            ctx->backpatches.count--;
        else
            break;
    }
}

static
int
ci_cmp_switch_entry(void*_Null_unspecified ctx, const void* a, const void* b){
    (void)ctx;
    const CcSwitchEntry* ea = a;
    const CcSwitchEntry* eb = b;
    if(ea->value < eb->value) return -1;
    if(ea->value > eb->value) return 1;
    return 0;
}

static
int
ci_lower_resolve_gotos(CiInterpreter* ci, CiLowerCtx* ctx){
    for(size_t i = 0; i < ctx->backpatches.count; i++){
        CiBackpatchTarget* t = &ctx->backpatches.data[i];
        switch(t->kind){
            case CI_BP_NONE:
                continue;
            case CI_BP_LABEL:{
                void* v = AM_get(ctx->labels, t->label);
                if(!v)
                    return ci_ice(ci, t->loc, "Use of undeclared label '%.*s'", t->label->length, t->label->data);
                *(uint32_t*)((char*)ctx->out->data+t->byteoffset) = (uint32_t)((uintptr_t)v - 1);
                t->kind = CI_BP_NONE;
                continue;
            }
            case CI_BP_BREAK:
                return ci_ice(ci, t->loc, "unresolved break in lowering%s", "");
            case CI_BP_CONTINUE:
                return ci_ice(ci, t->loc, "unresolved continue in lowering%s", "");
        }
    }
    ctx->backpatches.count = 0;
    return 0;
}


// Execute a standalone expression without changing the caller's opcode stream.
static
int
ci_eval_lowered_expr(CiInterpreter* ci, CiInterpFrame*_Nullable parent, CcExpr* expr, void* result, size_t size){
    Allocator al = ci_allocator(ci);
    Marray(CiOp) ops = {0};
    AtomMap(uintptr_t) labels = {0};
    uint32_t frame_size = 0;
    const CcTargetConfig* t = ci_target(ci);
    CiLowerCtx ctx = {
        .a = al,
        .out = &ops,
        .labels = &labels,
        .frame_size = &frame_size,
        .size_size = t->sizeof_[t->size_type],
        .ptr_size = t->sizeof_[CCBT_nullptr_t],
        .char_is_unsigned = !t->char_is_signed,
    };
    CiInterpFrame frame = {.parent = parent, .return_buf = result, .return_size = size};
    CiLowerVal value = {0};
    _Bool is_void = ccqt_bt_eq(expr->type, CCBT_void);
    int err = is_void ? ci_lower_expr_discard(ci, &ctx, expr) : ci_lower_expr(ci, &ctx, expr, CI_NO_SLOT, &value);
    if(err) goto cleanup;
    err = ci_lower_resolve_gotos(ci, &ctx);
    if(err) goto cleanup;
    if(!is_void && value.size > size){
        err = CI_RESULT_TOO_SMALL(ci, expr->loc, value.size, size);
        goto cleanup;
    }
    if(frame_size){
        frame.slots = Allocator_zalloc(al, frame_size);
        if(!frame.slots){ err = CI_OOM_ERROR; goto cleanup; }
    }
    frame.ops = ops.data;
    frame.op_count = ops.count;
    err = ci_interp_run(ci, &frame);
    if(!err && !is_void && value.size)
        memcpy(result, (char*)frame.slots + value.slot, value.size);
    cleanup:
    ci_free_alloca_list(al, frame.alloca_list);
    if(frame.slots) Allocator_free(al, frame.slots, frame_size);
    ma_cleanup(CiBackpatchTarget)(&ctx.backpatches, al);
    if(labels.data) Allocator_free(al, labels.data, AM_alloc_size(labels.cap));
    for(size_t i = 0; i < ops.count; i++){
        CiOp* op = &ops.data[i];
        if(op->kind == CI_OP_SWITCH && op->switch_.table)
            Allocator_free(al, op->switch_.table, sizeof(CiSwitchTable) + op->switch_.table->count * sizeof(CcSwitchEntry));
    }
    ma_cleanup(CiOp)(&ops, al);
    return err;
}

static
int
ci_lower_func(CiInterpreter* ci, CcFunc* f){
    int err;
    if(f->interp_ops) return 0;
    if(!f->parsed)
        return ci_ice(ci, f->loc, "lowering function '%s' before it was parsed", f->name->data);
    Allocator al = ci_allocator(ci);
    CiFuncOps* ops = Allocator_zalloc(al, sizeof *ops);
    if(!ops) return CI_OOM_ERROR;
    AtomMap(uintptr_t) labels = {0};
    const CcTargetConfig* t = ci_target(ci);
    CiLowerCtx ctx = {
        .a = al,
        .out = &ops->code,
        .labels = &labels,
        .temp = f->frame_size, // temps stack above params + locals
        .frame_size = &f->frame_size,
        .size_size = t->sizeof_[t->size_type],
        .ptr_size = t->sizeof_[CCBT_nullptr_t],
        .char_is_unsigned = !t->char_is_signed,
    };
    err = ci_lower_stmt(ci, &ctx, f->body_tree);
    if(!err)
        err = ci_lower_resolve_gotos(ci, &ctx);
    ma_cleanup(CiBackpatchTarget)(&ctx.backpatches, al);
    if(labels.data)
        Allocator_free(al, labels.data, AM_alloc_size(labels.cap));
    if(err){
        for(size_t i = 0; i < ops->code.count; i++){
            CiOp* o = &ops->code.data[i];
            if(o->kind == CI_OP_SWITCH && o->switch_.table)
                Allocator_free(al, o->switch_.table, sizeof(CiSwitchTable) + o->switch_.table->count * sizeof(CcSwitchEntry));
        }
        ma_cleanup(CiOp)(&ops->code, al);
        Allocator_free(al, ops, sizeof *ops);
        return err;
    }
    f->interp_ops = ops;
    return 0;
}

static
int
ci_lower_nodes(CiInterpreter* ci, Parray(CcStmtNode)* nodes, size_t* lowered, Marray(CiOp)* ops, AtomMap(uintptr_t)* labels, uint32_t* slot_size){
    int err = 0;
    const CcTargetConfig* t = ci_target(ci);
    CiLowerCtx ctx = {
        .a = ci_allocator(ci),
        .out = ops,
        .labels = labels,
        .temp = 0, // toplevel/module slots hold only temps; each batch recycles them
        .frame_size = slot_size,
        .size_size = t->sizeof_[t->size_type],
        .ptr_size = t->sizeof_[CCBT_nullptr_t],
        .char_is_unsigned = !t->char_is_signed,
    };
    for(size_t i = *lowered; i < nodes->count; i++){
        err = ci_lower_stmt(ci, &ctx, nodes->data[i]);
        if(err) break;
    }
    if(!err)
        err = ci_lower_resolve_gotos(ci, &ctx);
    ma_cleanup(CiBackpatchTarget)(&ctx.backpatches, ctx.a);
    *lowered = nodes->count;
    return err;
}

static
int
ci_lower_toplevel(CiInterpreter* ci){
    CcParser* p = &ci->parser;
    int err = ci_lower_nodes(ci, &p->toplevel_nodes, &ci->toplevel_lowered, &ci->toplevel_ops, &ci->toplevel_labels, &ci->toplevel_slot_size);
    if(err) return err;
    if(ci->toplevel_slot_size > ci->toplevel_slots_cap){
        uint32_t new_cap = ci->toplevel_slot_size * 2;
        void* slots = Allocator_zalloc(ci_allocator(ci), new_cap);
        if(!slots) return CI_OOM_ERROR;
        if(ci->toplevel_slots)
            Allocator_free(ci_allocator(ci), ci->toplevel_slots, ci->toplevel_slots_cap);
        ci->toplevel_slots = slots;
        ci->toplevel_slots_cap = new_cap;
    }
    ci->top_frame.ops = ci->toplevel_ops.data;
    ci->top_frame.op_count = ci->toplevel_ops.count;
    ci->top_frame.slots = ci->toplevel_slots;
    return 0;
}

static
int
ci_lower_module(CiInterpreter* ci, CiModule* module){
    return ci_lower_nodes(ci, &module->nodes, &module->lowered,
        &module->ops, &module->labels, &module->slot_size);
}

static
CiUint128
ci_fold_integer(CiLowerCtx* ctx, const CiFoldValue* v){
    CiUint128 result;
    if(v->sz < 16 && !ccqt_is_unsigned(v->type, ctx->char_is_unsigned))
        return ci_uint128_from_int64(ci_read_int(v->bits, v->sz));
    ci_uint128_read(&result, v->bits, v->sz);
    return result;
}

static
uint64_t
ci_fold_offset(CiLowerCtx* ctx, const CiFoldValue* index, uint32_t elem_sz){
    uint64_t offset = ci_uint128_lo(ci_fold_integer(ctx, index)) * (uint64_t)elem_sz;
    return ctx->ptr_size == 4 ? (uint32_t)offset : offset;
}

typedef struct CiFoldFloat CiFoldFloat;
struct CiFoldFloat {
    uint64_t significand;
    int exponent;
    _Bool negative, infinity;
};

static
int
ci_fold_float_read(const CiFoldValue* v, CiFoldFloat* out){
    uint64_t fraction;
    uint32_t exponent, max_exp, precision, bias;
    if(ccqt_bt_eq(v->type, CCBT_float)){
        CiIEE754Float32 f;
        memcpy(&f.u, v->bits, sizeof f.u);
        fraction = f.fraction;
        exponent = f.exponent;
        out->negative = f.sign;
        max_exp = 255; precision = 23; bias = 127;
    }
    else if(ccqt_bt_eq(v->type, CCBT_double)){
        CiIEE754Float64 f;
        memcpy(&f.u, v->bits, sizeof f.u);
        fraction = f.fraction;
        exponent = (uint32_t)f.exponent;
        out->negative = f.sign;
        max_exp = 2047; precision = 52; bias = 1023;
    }
    else return FOLD_FAIL;
    if((exponent == max_exp || exponent == 0) && fraction) return FOLD_FAIL;
    out->infinity = exponent == max_exp;
    out->significand = fraction | (exponent && !out->infinity ? (uint64_t)1 << precision : 0);
    out->exponent = (int)exponent - (int)bias - (int)precision;
    return 0;
}

static
int
ci_fold_float_write(CiFoldValue* out, CiUint128 magnitude, int exponent, _Bool negative, _Bool infinity){
    uint32_t precision, bias, max_exp;
    if(ccqt_bt_eq(out->type, CCBT_float)){
        precision = 23; bias = 127; max_exp = 255;
    }
    else if(ccqt_bt_eq(out->type, CCBT_double)){
        precision = 52; bias = 1023; max_exp = 2047;
    }
    else return FOLD_FAIL;
    uint64_t fraction = 0;
    uint32_t encoded_exp = 0;
    if(infinity) encoded_exp = max_exp;
    else if(ci_uint128_nonzero(magnitude)){
        uint64_t hi = ci_uint128_hi(magnitude), lo = ci_uint128_lo(magnitude);
        int top = hi ? 127 - clz_64(hi) : 63 - clz_64(lo);
        int exp = exponent + top + (int)bias;
        if(exp <= 0 || exp >= (int)max_exp) return FOLD_FAIL;
        int shift = top - (int)precision;
        if(shift > 0){
            CiUint128 reduced = ci_uint128_shr(magnitude, (uint64_t)shift);
            if(!ci_uint128_eq(ci_uint128_shl(reduced, (uint64_t)shift), magnitude)) return FOLD_FAIL;
            fraction = ci_uint128_lo(reduced);
        }
        else fraction = lo << (uint32_t)-shift;
        fraction &= ((uint64_t)1 << precision) - 1;
        encoded_exp = (uint32_t)exp;
    }
    if(precision == 23){
        CiIEE754Float32 f = {.u = 0};
        f.sign = negative;
        f.exponent = encoded_exp;
        f.fraction = (uint32_t)fraction;
        memcpy(out->bits, &f.u, sizeof f.u);
    }
    else {
        CiIEE754Float64 f = {.u = 0};
        f.sign = negative;
        f.exponent = encoded_exp;
        f.fraction = fraction;
        memcpy(out->bits, &f.u, sizeof f.u);
    }
    return 0;
}

static
int
ci_fold_float_binary(CcExprKind kind, const CiFoldValue* lhs, const CiFoldValue* rhs, CiFoldValue* out){
    if(lhs->type.unqual != rhs->type.unqual) return FOLD_FAIL;
    CiFoldFloat a, b;
    int err = ci_fold_float_read(lhs, &a);
    if(err) return err;
    err = ci_fold_float_read(rhs, &b);
    if(err) return err;
    switch((uint32_t)kind){
        case CC_EXPR_EQ: case CC_EXPR_NE:
        case CC_EXPR_LT: case CC_EXPR_GT: case CC_EXPR_LE: case CC_EXPR_GE:{
            uint64_t mask = ((uint64_t)1 << (lhs->sz * 8 - 1)) - 1;
            uint64_t am = ci_read_uint(lhs->bits, lhs->sz) & mask;
            uint64_t bm = ci_read_uint(rhs->bits, rhs->sz) & mask;
            int cmp;
            if(!am && !bm) cmp = 0; // +0 == -0
            else if(a.negative != b.negative) cmp = a.negative ? -1 : 1;
            else {
                cmp = am < bm ? -1 : am > bm ? 1 : 0;
                if(a.negative) cmp = -cmp;
            }
            _Bool result = 0;
            switch((uint32_t)kind){
                case CC_EXPR_EQ: result = cmp == 0; break;
                case CC_EXPR_NE: result = cmp != 0; break;
                case CC_EXPR_LT: result = cmp < 0; break;
                case CC_EXPR_GT: result = cmp > 0; break;
                case CC_EXPR_LE: result = cmp <= 0; break;
                case CC_EXPR_GE: result = cmp >= 0; break;
            }
            ci_write_uint(out->bits, out->sz, result);
            return 0;
        }
    }
    if(a.infinity || b.infinity) return FOLD_FAIL;
    CiUint128 u = ci_uint128_from_uint64(a.significand);
    CiUint128 v = ci_uint128_from_uint64(b.significand);
    switch((uint32_t)kind){
        case CC_EXPR_MUL:
            return ci_fold_float_write(out, ci_uint128_mul(u, v), a.exponent + b.exponent, a.negative != b.negative, 0);
        case CC_EXPR_DIV:{
            if(!b.significand) return FOLD_FAIL;
            // A binary-exact quotient requires the denominator's odd part to
            // divide the numerator. Powers of two just adjust the exponent.
            int twos = ctz_64(b.significand);
            uint64_t odd = b.significand >> twos;
            if(a.significand % odd) return FOLD_FAIL;
            u = ci_uint128_from_uint64(a.significand / odd);
            return ci_fold_float_write(out, u, a.exponent - b.exponent - twos, a.negative != b.negative, 0);
        }
        case CC_EXPR_ADD:
        case CC_EXPR_SUB:{
            if(kind == CC_EXPR_SUB) b.negative = !b.negative;
            if(!a.significand && !b.significand){
                if(a.negative != b.negative) return FOLD_FAIL; // zero sign depends on rounding
                return ci_fold_float_write(out, u, 0, a.negative, 0);
            }
            if(!a.significand)
                return ci_fold_float_write(out, v, b.exponent, b.negative, 0);
            if(!b.significand)
                return ci_fold_float_write(out, u, a.exponent, a.negative, 0);
            int exponent = a.exponent < b.exponent ? a.exponent : b.exponent;
            int ashift = a.exponent - exponent;
            int bshift = b.exponent - exponent;
            // Reserve a carry bit. Larger exponent gaps cannot be handled by
            // this bounded exact accumulator; leave them to the VM.
            if(ashift + 63 - clz_64(a.significand) >= 127 || bshift + 63 - clz_64(b.significand) >= 127) return FOLD_FAIL;
            u = ci_uint128_shl(u, (uint64_t)ashift);
            v = ci_uint128_shl(v, (uint64_t)bshift);
            if(a.negative == b.negative) u = ci_uint128_add(u, v);
            else {
                if(ci_uint128_eq(u, v)) return FOLD_FAIL; // exact cancellation has rounding-dependent zero sign
                if(ci_uint128_gt(u, v)) u = ci_uint128_sub(u, v);
                else {
                    u = ci_uint128_sub(v, u);
                    a.negative = b.negative;
                }
            }
            return ci_fold_float_write(out, u, exponent, a.negative, 0);
        }
        default: return FOLD_FAIL;
    }
}

static
int
ci_fold_float_cast(CiLowerCtx* ctx, const CiFoldValue* from, CiFoldValue* to){
    CiUint128 zero = ci_uint128_from_uint64(0);
    if(ccqt_is_integer(from->type)){
        CiUint128 u = ci_fold_integer(ctx, from);
        _Bool negative = !ccqt_is_unsigned(from->type, ctx->char_is_unsigned) && (ci_uint128_hi(u) >> 63);
        if(negative) u = ci_uint128_sub(zero, u);
        return ci_fold_float_write(to, u, 0, negative, 0);
    }
    if(!ci_falu_type(from->type)) return FOLD_FAIL;
    if(from->type.unqual == to->type.unqual){
        memcpy(to->bits, from->bits, from->sz);
        return 0;
    }
    CiFoldFloat f;
    int err = ci_fold_float_read(from, &f);
    if(err) return err;
    CiUint128 u = ci_uint128_from_uint64(f.significand);
    if(ci_falu_type(to->type))
        return ci_fold_float_write(to, u, f.exponent, f.negative, f.infinity);
    if(!ccqt_is_integer(to->type) || f.infinity) return FOLD_FAIL;
    if(f.significand){
        if(f.exponent < 0){
            int shift = -f.exponent;
            if(shift >= 64 || ((f.significand >> shift) << shift) != f.significand)
                return FOLD_FAIL; // Fractional truncation may raise FE_INEXACT.
            u = ci_uint128_from_uint64(f.significand >> shift);
        }
        else {
            int top = 63 - clz_64(f.significand);
            if(f.exponent + top >= 128) return FOLD_FAIL;
            u = ci_uint128_shl(u, (uint64_t)f.exponent);
        }
    }
    _Bool uns = ccqt_is_unsigned(to->type, ctx->char_is_unsigned);
    uint32_t width = to->sz * 8;
    if(uns){
        if(f.negative && ci_uint128_nonzero(u)) return FOLD_FAIL;
        if(width < 128 && ci_uint128_nonzero(ci_uint128_shr(u, width))) return FOLD_FAIL;
    }
    else {
        CiUint128 limit = ci_uint128_shl(ci_uint128_from_uint64(1), width - 1);
        if(ci_uint128_gt(u, limit) || (!f.negative && ci_uint128_eq(u, limit))) return FOLD_FAIL;
    }
    if(f.negative) u = ci_uint128_sub(zero, u);
    ci_uint128_write(to->bits, to->sz, u);
    return 0;
}

static
int
ci_fold_truth(const CiFoldValue* v, _Bool* truth){
    if(ci_falu_type(v->type)){
        CiFoldFloat f;
        int err = ci_fold_float_read(v, &f);
        if(err) return err;
        *truth = f.infinity || f.significand != 0;
    }
    else if(ccqt_is_integer(v->type) || ccqt_kind(v->type) == CC_POINTER || ccqt_bt_eq(v->type, CCBT_nullptr_t)){
        CiUint128 u;
        ci_uint128_read(&u, v->bits, v->sz);
        *truth = ci_uint128_nonzero(u);
    }
    else return FOLD_FAIL;
    return 0;
}

static
int
ci_fold_condition(CiInterpreter* ci, CiLowerCtx* ctx, CcExpr* e, _Bool* truth){
    CiFoldValue value;
    int err = ci_fold_expr(ci, ctx, e, &value);
    if(err) return err;
    return ci_fold_truth(&value, truth);
}

static
int
ci_fold_expr(CiInterpreter* ci, CiLowerCtx* ctx, CcExpr* e, CiFoldValue* folded){
    if(ccqt_kind(e->type) == CC_ARRAY) return FOLD_FAIL;
    if(e->kind != CC_EXPR_VALUE){
        if(e->type.is_atomic || e->type.is_volatile) return FOLD_FAIL;
        if(!ccqt_is_basic(e->type) && !ccqt_is_integer(e->type)
            && ccqt_kind(e->type) != CC_POINTER) return FOLD_FAIL;
    }
    uint32_t sz;
    int err = cc_sizeof_as_uint(&ci->parser, e->type, e->loc, &sz);
    if(err) return err;
    if(sz > 16) return FOLD_FAIL;
    if(e->kind != CC_EXPR_VALUE && sz != 1 && sz != 2 && sz != 4 && sz != 8 && sz != 16) return FOLD_FAIL;
    CiFoldValue result = {.sz = sz, .type = e->type};
    CiFoldValue l, r;
    CiUint128 zero = ci_uint128_from_uint64(0), u = zero;
    _Bool truth;
    switch((uint32_t)e->kind){
        case CC_EXPR_VALUE:
            // The literal payload is eight bytes, including for wide types.
            memcpy(result.bits, &e->uinteger, sz < sizeof e->uinteger ? sz : sizeof e->uinteger);
            *folded = result;
            return 0;
        case CC_EXPR_VARIABLE:
            // I think we can also detect write-once variables.
            if(!(e->var->constexpr_ || e->type.is_const) || !e->var->initializer) return FOLD_FAIL;
            err = ci_fold_expr(ci, ctx, e->var->initializer, &l);
            if(err) return err;
            if(l.sz != sz) return FOLD_FAIL;
            memcpy(result.bits, l.bits, sz);
            *folded = result;
            return 0;
        case CC_EXPR_COMMA:
        case CC_EXPR_TERNARY:
        case CC_EXPR_LOGAND:
        case CC_EXPR_LOGOR:
        case CC_EXPR_LOGNOT:
            err = ci_fold_expr(ci, ctx, e->lhs, &l);
            if(err) return err;
            if(e->kind == CC_EXPR_COMMA){
                err = ci_fold_expr(ci, ctx, e->values[0], &r);
            }
            else {
                err = ci_fold_truth(&l, &truth);
                if(err) return err;
                if(e->kind == CC_EXPR_TERNARY)
                    err = ci_fold_expr(ci, ctx, e->values[truth ? 0 : 1], &r);
                else {
                    if(e->kind == CC_EXPR_LOGNOT) truth = !truth;
                    else if(truth == (e->kind == CC_EXPR_LOGAND)){
                        err = ci_fold_expr(ci, ctx, e->values[0], &r);
                        if(err) return err;
                        err = ci_fold_truth(&r, &truth);
                        if(err) return err;
                    }
                    u = ci_uint128_from_uint64(truth);
                    break;
                }
            }
            if(err) return err;
            if(r.sz != sz) return FOLD_FAIL;
            memcpy(result.bits, r.bits, sz);
            *folded = result;
            return 0;
        case CC_EXPR_CAST:
        case CC_EXPR_POS:
        case CC_EXPR_NEG:
        case CC_EXPR_BITNOT:
        case CC_EXPR_POPCOUNT:
        case CC_EXPR_CLZ:
        case CC_EXPR_CTZ:
        case CC_EXPR_BSWAP:
            err = ci_fold_expr(ci, ctx, e->lhs, &l);
            if(err) return err;
            if(e->kind == CC_EXPR_CAST && ccqt_bt_eq(e->type, CCBT_bool)){
                err = ci_fold_truth(&l, &truth);
                if(err) return err;
                u = ci_uint128_from_uint64(truth);
                break;
            }
            if(e->kind == CC_EXPR_CAST && (ci_falu_type(l.type) || ci_falu_type(e->type))){
                err = ci_fold_float_cast(ctx, &l, &result);
                if(err) return err;
                *folded = result;
                return 0;
            }
            if((e->kind == CC_EXPR_POS || e->kind == CC_EXPR_NEG)
                && ci_falu_type(l.type) && l.type.unqual == e->type.unqual){
                // Unary sign operations do not round or quiet signaling NaNs.
                memcpy(result.bits, l.bits, l.sz);
                if(e->kind == CC_EXPR_NEG){
                    if(l.sz == 4){
                        uint32_t bits;
                        memcpy(&bits, result.bits, sizeof bits);
                        bits ^= (uint32_t)1 << 31;
                        memcpy(result.bits, &bits, sizeof bits);
                    }
                    else result.bits[0] ^= (uint64_t)1 << 63;
                }
                *folded = result;
                return 0;
            }
            if(e->kind == CC_EXPR_CAST){
                _Bool from_ptr = ccqt_kind(l.type) == CC_POINTER || ccqt_bt_eq(l.type, CCBT_nullptr_t);
                _Bool to_ptr = ccqt_kind(e->type) == CC_POINTER || ccqt_bt_eq(e->type, CCBT_nullptr_t);
                if((from_ptr || ccqt_is_integer(l.type)) && (to_ptr || ccqt_is_integer(e->type))){
                    if(from_ptr) ci_uint128_read(&u, l.bits, l.sz);
                    else u = ci_fold_integer(ctx, &l);
                    break;
                }
            }
            if(!ccqt_is_integer(l.type) || !ccqt_is_integer(e->type)) return FOLD_FAIL;
            u = ci_fold_integer(ctx, &l);
            switch((uint32_t)e->kind){
                case CC_EXPR_CAST: case CC_EXPR_POS: break;
                case CC_EXPR_NEG: u = ci_uint128_sub(zero, u); break;
                case CC_EXPR_BITNOT: u = ci_uint128_xor(u, ci_uint128_from_int64(-1)); break;
                default:{
                    if(l.sz > 8) return FOLD_FAIL;
                    uint64_t v = ci_read_uint(l.bits, l.sz), n;
                    if(e->kind == CC_EXPR_POPCOUNT) n = (uint64_t)popcount_64(v);
                    else if(e->kind == CC_EXPR_CLZ) n = v ? (uint64_t)clz_64(v) - (64 - l.sz * 8) : l.sz * 8;
                    else if(e->kind == CC_EXPR_CTZ) n = v ? (uint64_t)ctz_64(v) : l.sz * 8;
                    else {
                        n = 0;
                        for(uint32_t i = 0; i < l.sz; i++, v >>= 8) n = (n << 8) | (v & 255);
                    }
                    u = ci_uint128_from_uint64(n);
                    break;
                }
            }
            break;
        case CC_EXPR_ADD: case CC_EXPR_SUB: case CC_EXPR_MUL:
        case CC_EXPR_DIV: case CC_EXPR_MOD:
        case CC_EXPR_BITAND: case CC_EXPR_BITOR: case CC_EXPR_BITXOR:
        case CC_EXPR_LSHIFT: case CC_EXPR_RSHIFT:
        case CC_EXPR_EQ: case CC_EXPR_NE:
        case CC_EXPR_LT: case CC_EXPR_GT: case CC_EXPR_LE: case CC_EXPR_GE:{
            if(ci_falu_type(e->lhs->type) && ci_falu_type(e->values[0]->type)){
                err = ci_fold_expr(ci, ctx, e->lhs, &l);
                if(err) return err;
                err = ci_fold_expr(ci, ctx, e->values[0], &r);
                if(err) return err;
                err = ci_fold_float_binary(e->kind, &l, &r, &result);
                if(err) return err;
                *folded = result;
                return 0;
            }
            if(!ccqt_is_integer(e->lhs->type) || !ccqt_is_integer(e->values[0]->type)) return FOLD_FAIL;
            err = ci_fold_expr(ci, ctx, e->lhs, &l);
            if(err) return err;
            err = ci_fold_expr(ci, ctx, e->values[0], &r);
            if(err) return err;
            CiUint128 a = ci_fold_integer(ctx, &l), b = ci_fold_integer(ctx, &r);
            _Bool uns = ccqt_is_unsigned(l.type, ctx->char_is_unsigned);
            CiInt128 sa = ci_int128_from_uint128(a), sb = ci_int128_from_uint128(b);
            switch((uint32_t)e->kind){
                case CC_EXPR_ADD: u = ci_uint128_add(a, b); break;
                case CC_EXPR_SUB: u = ci_uint128_sub(a, b); break;
                case CC_EXPR_MUL: u = ci_uint128_mul(a, b); break;
                case CC_EXPR_DIV: case CC_EXPR_MOD:{
                    if(!ci_uint128_nonzero(b)) return FOLD_FAIL;
                    CiUint128 min = ci_uint128_shl(ci_uint128_from_uint64(1), l.sz * 8 - 1);
                    if(!uns && ci_uint128_eq(b, ci_uint128_from_int64(-1))
                        && ci_uint128_eq(a, ci_uint128_sub(zero, min))) return FOLD_FAIL;
                    if(e->kind == CC_EXPR_DIV)
                        u = uns ? ci_uint128_div(a, b) : ci_uint128_from_int128(ci_int128_div(sa, sb));
                    else
                        u = uns ? ci_uint128_mod(a, b) : ci_uint128_from_int128(ci_int128_mod(sa, sb));
                    break;
                }
                case CC_EXPR_BITAND: u = ci_uint128_and(a, b); break;
                case CC_EXPR_BITOR: u = ci_uint128_or(a, b); break;
                case CC_EXPR_BITXOR: u = ci_uint128_xor(a, b); break;
                case CC_EXPR_LSHIFT: case CC_EXPR_RSHIFT:
                    if(ci_uint128_hi(b) || ci_uint128_lo(b) >= l.sz * 8) return FOLD_FAIL;
                    if(e->kind == CC_EXPR_LSHIFT) u = ci_uint128_shl(a, ci_uint128_lo(b));
                    else u = uns ? ci_uint128_shr(a, ci_uint128_lo(b))
                        : ci_uint128_from_int128(ci_int128_shr(sa, ci_uint128_lo(b)));
                    break;
                case CC_EXPR_EQ: u = ci_uint128_from_uint64(ci_uint128_eq(a, b)); break;
                case CC_EXPR_NE: u = ci_uint128_from_uint64(ci_uint128_ne(a, b)); break;
                case CC_EXPR_LT: u = ci_uint128_from_uint64(uns ? ci_uint128_lt(a, b) : ci_int128_lt(sa, sb)); break;
                case CC_EXPR_GT: u = ci_uint128_from_uint64(uns ? ci_uint128_gt(a, b) : ci_int128_gt(sa, sb)); break;
                case CC_EXPR_LE: u = ci_uint128_from_uint64(uns ? ci_uint128_le(a, b) : ci_int128_le(sa, sb)); break;
                case CC_EXPR_GE: u = ci_uint128_from_uint64(uns ? ci_uint128_ge(a, b) : ci_int128_ge(sa, sb)); break;
            }
            break;
        }
        case CC_EXPR_CALL: // I think gcc treats some libc/libm funcs as intrins, we could do the same
        // TODO: folded lvalues? can't produce them in the bytecode,
        // but we could have them as intermediaries.
        // But especially with const/constexpr vars we should be
        // able to fold out reads from them.
        case CC_EXPR_DOT:
        case CC_EXPR_ARROW:
        case CC_EXPR_SUBSCRIPT:
        case CC_EXPR_DEREF:
        case CC_EXPR_ADDR:


        // This could be done, just requires work.
        case CC_EXPR_TYPE_INTROSPECTION:

        // could elide bounds checks etc.
        case CC_EXPR_SLICE_ALL:
        case CC_EXPR_SLICE:
        case CC_EXPR_SLICE_LO:
        case CC_EXPR_SLICE_HI:

        // If it fits, why not?
        // Also might be needed for type-punning like
        // `(union {int i; float f;}){.f=1.f}.i`
        case CC_EXPR_COMPOUND_LITERAL:
        case CC_EXPR_INIT_LIST:
        // rest are runtime
        default: return FOLD_FAIL;
    }
    ci_uint128_write(result.bits, sz, u);
    *folded = result;
    return 0;
}

#ifdef __clang__
#pragma clang assume_nonnull end
#endif
#endif
