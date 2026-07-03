//
// Copyright © 2026-2026, David Priver <david@davidpriver.com>
//
#ifndef C_CI_LOWER_C
#define C_CI_LOWER_C
#include "ci_interp.h"
#include "ci_op.h"
#include "cc_errors.h"
#include "srcloc.h"
#include "../Drp/atom.h"
#include "../Drp/merge_sort.h"
#include "../Drp/ckdint.h"

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
};

// A value produced by expression lowering: which slot it lives in, and whether
// it already holds a canonical 0/1 integer (so conditional jumps can test it
// without normalizing through CI_OP_ISTRUE).
typedef struct CiLowerVal CiLowerVal;
struct CiLowerVal {
    uint32_t slot;
    uint32_t size;
    _Bool canonical;
};
#define CI_NO_SLOT UINT32_MAX

static int ci_lower_stmt(CiInterpreter* ci, CiLowerCtx* ctx, CcStmtNode*_Nullable n);
static int ci_lower_stmt_inner(CiInterpreter* ci, CiLowerCtx* ctx, CcStmtNode* n);
static int ci_lower_expr(CiInterpreter* ci, CiLowerCtx* ctx, CcExpr* e, uint32_t dest, CiLowerVal* out);
static int ci_lower_expr_discard(CiInterpreter* ci, CiLowerCtx* ctx, CcExpr* e);
static int ci_lower_cond(CiInterpreter* ci, CiLowerCtx* ctx, CcExpr* cond, CiLowerVal* out);
static int ci_lower_istrue(CiLowerCtx* ctx, const CiLowerVal* v, CcQualType src_type, uint32_t dest, uint32_t dest_size, SrcLoc loc);
static int ci_lower_dest(CiLowerCtx* ctx, uint32_t* dest, uint32_t size);
static int ci_alloc_slot(CiLowerCtx*, uint32_t sz, uint32_t align, uint32_t* slot);
static void ci_backpatch_break_continue(CiLowerCtx*, size_t start, uint32_t break_target, uint32_t continue_target);
static void ci_backpatch_break(CiLowerCtx*, size_t start, uint32_t break_target);
static int ci_lower_resolve_gotos(CiInterpreter*, CiLowerCtx*);
static int ci_cmp_switch_entry(void*_Null_unspecified ctx, const void* a, const void* b);

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
        case CC_STMT_COMPOUND: {
            for(uint32_t i = 0; i < n->count; i++){
                err = ci_lower_stmt(ci, ctx, n->stmts[i]);
                if(err) return err;
            }
            return 0;
        }
        case CC_STMT_IF: {
            CcExpr* cond = n->exprs[0];
            CiLowerVal v;
            err = ci_lower_cond(ci, ctx, cond, &v);
            if(err) return err;
            CiOp* op;
            err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
            if(err) return err;
            *op = (CiOp){
                .kind = CI_OP_JUMP_FALSE,
                .slot = v.slot,
                .slot_size = v.size,
                .loc = n->loc,
            };
            ptrdiff_t jump = (char*)&op->jump - (char*)ctx->out->data;
            err = ci_lower_stmt(ci, ctx, n->stmts[0]);
            if(err) return err;
            if(!n->stmts[1]){
                *(uint32_t*)((char*)ctx->out->data+jump) = (uint32_t)ctx->out->count;
            }
            else {
                err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
                if(err) return err;
                *op = (CiOp){
                    .kind = CI_OP_JUMP,
                    .loc = n->loc,
                };
                *(uint32_t*)((char*)ctx->out->data+jump) = (uint32_t)ctx->out->count;
                jump = (char*)&op->jump - (char*)ctx->out->data;
                err = ci_lower_stmt(ci, ctx, n->stmts[1]);
                if(err) return err;
                *(uint32_t*)((char*)ctx->out->data+jump) = (uint32_t)ctx->out->count;
            }
            return 0;
        }
        case CC_STMT_WHILE: {
            CcExpr* cond = n->exprs[0];
            uint32_t cond_idx = (uint32_t)ctx->out->count;
            CiLowerVal v;
            err = ci_lower_cond(ci, ctx, cond, &v);
            if(err) return err;
            CiOp* op;
            err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
            if(err) return err;
            *op = (CiOp){
                .kind = CI_OP_JUMP_FALSE,
                .slot = v.slot,
                .slot_size = v.size,
                .loc = n->loc,
            };
            ptrdiff_t jump = (char*)&op->jump - (char*)ctx->out->data;
            size_t backpatch_start = ctx->backpatches.count;
            err = ci_lower_stmt(ci, ctx, n->stmts[0]);
            if(err) return err;
            err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
            if(err) return err;
            *op = (CiOp){
                .kind = CI_OP_JUMP,
                .loc = n->loc,
                .jump = cond_idx,
            };
            uint32_t break_idx = (uint32_t)ctx->out->count;
            *(uint32_t*)((char*)ctx->out->data+jump) = break_idx;
            ci_backpatch_break_continue(ctx, backpatch_start, break_idx, cond_idx);
            return 0;
        }
        case CC_STMT_DOWHILE: {
            CcExpr* cond = n->exprs[0];
            size_t backpatch_start = ctx->backpatches.count;
            uint32_t body_start = (uint32_t)ctx->out->count;
            err = ci_lower_stmt(ci, ctx, n->stmts[0]);
            if(err) return err;
            uint32_t cond_idx = (uint32_t)ctx->out->count;
            CiLowerVal v;
            err = ci_lower_cond(ci, ctx, cond, &v);
            if(err) return err;
            CiOp* op;
            err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
            if(err) return err;
            *op = (CiOp){
                .kind = CI_OP_JUMP_TRUE,
                .slot = v.slot,
                .slot_size = v.size,
                .loc = n->loc,
                .jump = body_start,
            };
            ci_backpatch_break_continue(ctx, backpatch_start, (uint32_t)ctx->out->count, cond_idx);
            return 0;
        }
        case CC_STMT_FOR: {
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
            ptrdiff_t jump = -1;
            if(cond){
                CiLowerVal v;
                err = ci_lower_cond(ci, ctx, cond, &v);
                if(err) return err;
                err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
                if(err) return err;
                *op = (CiOp){
                    .kind = CI_OP_JUMP_FALSE,
                    .slot = v.slot,
                    .slot_size = v.size,
                    .loc = n->loc,
                };
                jump = (char*)&op->jump - (char*)ctx->out->data;
            }
            err = ci_lower_stmt(ci, ctx, n->stmts[1]); // body
            if(err) return err;
            uint32_t continue_idx = (uint32_t)ctx->out->count;
            if(inc){
                err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
                if(err) return err;
                *op = (CiOp){
                    .kind = CI_OP_EVAL,
                    .expr = inc,
                    .loc = inc->loc,
                };
            }
            err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
            if(err) return err;
            *op = (CiOp){
                .kind = CI_OP_JUMP,
                .loc = n->loc,
                .jump = top_idx,
            };
            uint32_t break_idx = (uint32_t)ctx->out->count;
            if(jump >= 0)
                *(uint32_t*)((char*)ctx->out->data+jump) = break_idx;
            ci_backpatch_break_continue(ctx, backpatch_start, break_idx, continue_idx);
            return 0;
        }
        case CC_STMT_SWITCH: {
            CcExpr* e = n->exprs[0];
            CiLowerVal v;
            err = ci_lower_expr(ci, ctx, e, CI_NO_SLOT, &v);
            if(err) return err;
            // _Type switches already produce the canonical type bits;
            // treat them as unsigned.
            _Bool is_unsigned = ccqt_bt_eq(e->type, CCBT__Type)
                || ccqt_is_unsigned(e->type, !ci_target(ci)->char_is_signed);
            CiOp* op;
            err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
            if(err) return err;
            *op = (CiOp){
                .kind = CI_OP_SWITCH,
                .slot = v.slot,
                .slot_size = v.size,
                .loc = n->loc,
                .extra = is_unsigned,
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
            // Shrink and steal the table
            if(sw.entries.count){
                err = ma_shrink_to_size(CcSwitchEntry)(&sw.entries, ctx->a);
                if(err){ err = CI_OOM_ERROR; goto switch_cleanup; }
            }
            {
                uint32_t break_target = (uint32_t)ctx->out->count;
                CiOp* swop = &ctx->out->data[sw_idx];
                swop->jump = sw.has_default ? sw.default_target : break_target;
                swop->sw.table = sw.entries.data;
                swop->sw.count = sw.entries.count;
                sw.entries.data = NULL;
                sw.entries.count = 0;
                sw.entries.capacity = 0;
                ci_backpatch_break(ctx, backpatch_start, break_target);
            }
            switch_cleanup:
            ma_cleanup(CcSwitchEntry)(&sw.entries, ctx->a);
            return err;
        }
        case CC_STMT_CASE: {
            if(!ctx->sw)
                return ci_error(ci, n->loc, "ICE: case label outside of switch in lowering at %s:%d", __FILE__, __LINE__);
            CcSwitchEntry entry = {.value = n->case_value, .target = (uint32_t)ctx->out->count};
            err = ma_push(CcSwitchEntry)(&ctx->sw->entries, ctx->a, entry);
            if(err) return CI_OOM_ERROR;
            return ci_lower_stmt(ci, ctx, n->stmts[0]);
        }
        case CC_STMT_DEFAULT: {
            if(!ctx->sw)
                return ci_error(ci, n->loc, "ICE: default label outside of switch in lowering at %s:%d", __FILE__, __LINE__);
            ctx->sw->has_default = 1;
            ctx->sw->default_target = (uint32_t)ctx->out->count;
            return ci_lower_stmt(ci, ctx, n->stmts[0]);
        }
        case CC_STMT_RETURN: {
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
                    .kind = CI_OP_RETURN_SLOT,
                    .src = v.slot,
                    .src_size = v.size,
                    .loc = n->loc,
                };
                return 0;
            }
            err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
            if(err) return err;
            *op = (CiOp){
                .kind = CI_OP_RETURN,
                .loc = n->loc,
            };
            return 0;
        }
        case CC_STMT_BREAK:
        case CC_STMT_CONTINUE: {
            CiOp* op;
            err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
            if(err) return err;
            *op = (CiOp){
                .kind = CI_OP_JUMP,
                .loc = n->loc,
            };
            CiBackpatchTarget t = {
                .kind = n->kind == CC_STMT_BREAK? CI_BP_BREAK : CI_BP_CONTINUE,
                .byteoffset = (size_t)((char*)&op->jump - (char*)ctx->out->data),
                .loc = n->loc,
            };
            err = ma_push(CiBackpatchTarget)(&ctx->backpatches, ctx->a, t);
            if(err) return CI_OOM_ERROR;
            return 0;
        }
        case CC_STMT_GOTO: {
            CiOp* op;
            err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
            if(err) return err;
            *op = (CiOp){
                .kind = CI_OP_JUMP,
                .loc = n->loc,
            };
            CiBackpatchTarget t = {
                .kind = CI_BP_LABEL,
                .label = n->label,
                .byteoffset = (size_t)((char*)&op->jump - (char*)ctx->out->data),
                .loc = n->loc,
            };
            err = ma_push(CiBackpatchTarget)(&ctx->backpatches, ctx->a, t);
            if(err) return CI_OOM_ERROR;
            return 0;
        }
        case CC_STMT_LABEL: {
            void* existing = AM_get(ctx->labels, n->label);
            if(existing)
                return ci_error(ci, n->loc, "ICE: Duplicate label '%.*s' at %s:%d", n->label->length, n->label->data, __FILE__, __LINE__);
            err = AM_put(ctx->labels, ctx->a, n->label, (void*)(uintptr_t)(ctx->out->count + 1));
            if(err) return CI_OOM_ERROR;
            return ci_lower_stmt(ci, ctx, n->stmts[0]);
        }
    }
    return ci_error(ci, n->loc, "ICE: unknown statement kind in lowering at %s:%d", __FILE__, __LINE__);
}

// Flatten an expression into ops leaving its value in a slot.
// Control-flow expressions (&&, ||, ?:, comma) lower to jumps; every other
// kind falls back to CI_OP_EVAL_INTO of the (sub)tree until it grows its own
// ops. dest is the requested slot, or CI_NO_SLOT to allocate one.
static
int
ci_lower_expr(CiInterpreter* ci, CiLowerCtx* ctx, CcExpr* e, uint32_t dest, CiLowerVal* out){
    int err;
    CcParser* p = &ci->parser;
    uint32_t size;
    err = cc_sizeof_as_uint(p, e->type, e->loc, &size);
    if(err) return err;
    out->size = size;
    out->canonical = 0;
    switch((uint32_t)e->kind){
        case CC_EXPR_VALUE: {
            if(ccqt_kind(e->type) == CC_ARRAY || size > 8)
                break; // string literals and oversized values fall back
            err = ci_lower_dest(ctx, &dest, size);
            if(err) return err;
            out->slot = dest;
            CiOp* op;
            err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
            if(err) return err;
            *op = (CiOp){
                .kind = CI_OP_CONST,
                .slot = dest,
                .slot_size = size,
                .loc = e->loc,
            };
            memcpy(&op->imm, &e->uinteger, sizeof op->imm);
            uint64_t val = op->imm;
            if(size < 8)
                val &= ((uint64_t)1 << (size*8)) - 1;
            out->canonical = val <= 1;
            return 0;
        }
        case CC_EXPR_VARIABLE: {
            CcVariable* var = e->var;
            if(!var->automatic || e->type.is_atomic)
                break; // statics resolve storage lazily, atomics need an atomic load
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
                .kind = CI_OP_COPY,
                .slot = dest,
                .slot_size = size,
                .src = (uint32_t)var->frame_offset,
                .src_size = size,
                .loc = e->loc,
            };
            return 0;
        }
        case CC_EXPR_LOGAND:
        case CC_EXPR_LOGOR: {
            // dest = istrue(lhs); if(!dest) goto end; dest = istrue(rhs); end:
            // (|| jumps on true instead)
            err = ci_lower_dest(ctx, &dest, size);
            if(err) return err;
            out->slot = dest;
            CcExpr* lhs = e->lhs;
            CcExpr* rhs = e->values[0];
            uint32_t temp = ctx->temp;
            CiLowerVal v;
            err = ci_lower_expr(ci, ctx, lhs, CI_NO_SLOT, &v);
            if(err) return err;
            err = ci_lower_istrue(ctx, &v, lhs->type, dest, size, lhs->loc);
            if(err) return err;
            ctx->temp = temp;
            CiOp* op;
            err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
            if(err) return err;
            *op = (CiOp){
                .kind = e->kind == CC_EXPR_LOGAND? CI_OP_JUMP_FALSE : CI_OP_JUMP_TRUE,
                .slot = dest,
                .slot_size = size,
                .loc = e->loc,
            };
            ptrdiff_t jump = (char*)&op->jump - (char*)ctx->out->data;
            err = ci_lower_expr(ci, ctx, rhs, CI_NO_SLOT, &v);
            if(err) return err;
            err = ci_lower_istrue(ctx, &v, rhs->type, dest, size, rhs->loc);
            if(err) return err;
            ctx->temp = temp;
            *(uint32_t*)((char*)ctx->out->data+jump) = (uint32_t)ctx->out->count;
            out->canonical = 1;
            return 0;
        }
        case CC_EXPR_TERNARY: {
            // cond; if(!cond) goto else; dest = then; goto end; else: dest = else; end:
            err = ci_lower_dest(ctx, &dest, size);
            if(err) return err;
            out->slot = dest;
            uint32_t temp = ctx->temp;
            CiLowerVal v;
            err = ci_lower_cond(ci, ctx, e->lhs, &v);
            if(err) return err;
            CiOp* op;
            err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
            if(err) return err;
            *op = (CiOp){
                .kind = CI_OP_JUMP_FALSE,
                .slot = v.slot,
                .slot_size = v.size,
                .loc = e->loc,
            };
            ptrdiff_t jump = (char*)&op->jump - (char*)ctx->out->data;
            ctx->temp = temp;
            err = ci_lower_expr(ci, ctx, e->values[0], dest, &v);
            if(err) return err;
            ctx->temp = temp;
            err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
            if(err) return err;
            *op = (CiOp){
                .kind = CI_OP_JUMP,
                .loc = e->loc,
            };
            *(uint32_t*)((char*)ctx->out->data+jump) = (uint32_t)ctx->out->count;
            jump = (char*)&op->jump - (char*)ctx->out->data;
            err = ci_lower_expr(ci, ctx, e->values[1], dest, &v);
            if(err) return err;
            ctx->temp = temp;
            *(uint32_t*)((char*)ctx->out->data+jump) = (uint32_t)ctx->out->count;
            return 0;
        }
        case CC_EXPR_COMMA: {
            CcExpr* lhs = e->lhs;
            uint32_t temp = ctx->temp;
            if(ccqt_bt_eq(lhs->type, CCBT_void)){
                CiOp* op;
                err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
                if(err) return err;
                *op = (CiOp){
                    .kind = CI_OP_EVAL,
                    .expr = lhs,
                    .loc = lhs->loc,
                };
            }
            else {
                CiLowerVal v;
                err = ci_lower_expr(ci, ctx, lhs, CI_NO_SLOT, &v);
                if(err) return err;
            }
            ctx->temp = temp;
            return ci_lower_expr(ci, ctx, e->values[0], dest, out);
        }
        default:
            break;
    }
    // fallback: evaluate the (sub)tree
    err = ci_lower_dest(ctx, &dest, size);
    if(err) return err;
    out->slot = dest;
    CiOp* op;
    err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
    if(err) return err;
    *op = (CiOp){
        .kind = CI_OP_EVAL_INTO,
        .slot = dest,
        .slot_size = size,
        .expr = e,
        .loc = e->loc,
    };
    switch((uint32_t)e->kind){
        case CC_EXPR_LOGNOT:
        case CC_EXPR_EQ:
        case CC_EXPR_NE:
        case CC_EXPR_LT:
        case CC_EXPR_GT:
        case CC_EXPR_LE:
        case CC_EXPR_GE:
            // the evaluator writes these as canonical 0/1
            out->canonical = 1;
            break;
        default:
            break;
    }
    return 0;
}

// Lower an expression for side effects only.
static
int
ci_lower_expr_discard(CiInterpreter* ci, CiLowerCtx* ctx, CcExpr* e){
    int err;
    switch((uint32_t)e->kind){
        case CC_EXPR_VALUE:
        case CC_EXPR_VARIABLE:
        case CC_EXPR_FUNCTION:
            // no side effects (the evaluator also skips discarded reads)
            return 0;
        case CC_EXPR_LOGAND:
        case CC_EXPR_LOGOR: {
            uint32_t temp = ctx->temp;
            CiLowerVal v;
            err = ci_lower_cond(ci, ctx, e->lhs, &v);
            if(err) return err;
            CiOp* op;
            err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
            if(err) return err;
            *op = (CiOp){
                .kind = e->kind == CC_EXPR_LOGAND? CI_OP_JUMP_FALSE : CI_OP_JUMP_TRUE,
                .slot = v.slot,
                .slot_size = v.size,
                .loc = e->loc,
            };
            ptrdiff_t jump = (char*)&op->jump - (char*)ctx->out->data;
            ctx->temp = temp;
            err = ci_lower_expr_discard(ci, ctx, e->values[0]);
            if(err) return err;
            *(uint32_t*)((char*)ctx->out->data+jump) = (uint32_t)ctx->out->count;
            return 0;
        }
        case CC_EXPR_TERNARY: {
            uint32_t temp = ctx->temp;
            CiLowerVal v;
            err = ci_lower_cond(ci, ctx, e->lhs, &v);
            if(err) return err;
            CiOp* op;
            err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
            if(err) return err;
            *op = (CiOp){
                .kind = CI_OP_JUMP_FALSE,
                .slot = v.slot,
                .slot_size = v.size,
                .loc = e->loc,
            };
            ptrdiff_t jump = (char*)&op->jump - (char*)ctx->out->data;
            ctx->temp = temp;
            err = ci_lower_expr_discard(ci, ctx, e->values[0]);
            if(err) return err;
            err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
            if(err) return err;
            *op = (CiOp){
                .kind = CI_OP_JUMP,
                .loc = e->loc,
            };
            *(uint32_t*)((char*)ctx->out->data+jump) = (uint32_t)ctx->out->count;
            jump = (char*)&op->jump - (char*)ctx->out->data;
            err = ci_lower_expr_discard(ci, ctx, e->values[1]);
            if(err) return err;
            *(uint32_t*)((char*)ctx->out->data+jump) = (uint32_t)ctx->out->count;
            return 0;
        }
        case CC_EXPR_COMMA: {
            err = ci_lower_expr_discard(ci, ctx, e->lhs);
            if(err) return err;
            return ci_lower_expr_discard(ci, ctx, e->values[0]);
        }
        default: {
            CiOp* op;
            err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
            if(err) return err;
            *op = (CiOp){
                .kind = CI_OP_EVAL,
                .expr = e,
                .loc = e->loc,
            };
            return 0;
        }
    }
}

static
int
ci_lower_dest(CiLowerCtx* ctx, uint32_t* dest, uint32_t size){
    if(*dest != CI_NO_SLOT) return 0;
    return ci_alloc_slot(ctx, size, size, dest);
}

// Lower a condition expression and normalize it so a conditional jump can
// test its slot without knowing the type.
static
int
ci_lower_cond(CiInterpreter* ci, CiLowerCtx* ctx, CcExpr* cond, CiLowerVal* out){
    int err = ci_lower_expr(ci, ctx, cond, CI_NO_SLOT, out);
    if(err) return err;
    if(out->canonical) return 0;
    uint32_t slot;
    err = ci_alloc_slot(ctx, 1, 1, &slot);
    if(err) return err;
    err = ci_lower_istrue(ctx, out, cond->type, slot, 1, cond->loc);
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
ci_lower_istrue(CiLowerCtx* ctx, const CiLowerVal* v, CcQualType src_type, uint32_t dest, uint32_t dest_size, SrcLoc loc){
    uint32_t float_kind = 0;
    if(ccqt_is_basic(src_type) && ccbt_is_float(src_type.basic.kind))
        float_kind = (uint32_t)src_type.basic.kind;
    CiOp* op;
    int err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
    if(err) return err;
    *op = (CiOp){
        .kind = CI_OP_ISTRUE,
        .slot = dest,
        .slot_size = dest_size,
        .src = v->slot,
        .src_size = v->size,
        .extra = float_kind,
        .loc = loc,
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
            case CI_BP_LABEL: {
                void* v = AM_get(ctx->labels, t->label);
                if(!v)
                    return ci_error(ci, t->loc, "ICE: Use of undeclared label '%.*s' at %s:%d", t->label->length, t->label->data, __FILE__, __LINE__);
                *(uint32_t*)((char*)ctx->out->data+t->byteoffset) = (uint32_t)((uintptr_t)v - 1);
                t->kind = CI_BP_NONE;
                continue;
            }
            case CI_BP_BREAK:
                return ci_error(ci, t->loc, "ICE: unresolved break in lowering at %s:%d", __FILE__, __LINE__);
            case CI_BP_CONTINUE:
                return ci_error(ci, t->loc, "ICE: unresolved continue in lowering at %s:%d", __FILE__, __LINE__);
        }
    }
    ctx->backpatches.count = 0;
    return 0;
}


static
int
ci_lower_func(CiInterpreter* ci, CcFunc* f){
    int err;
    if(f->interp_ops) return 0;
    if(!f->parsed)
        return ci_error(ci, f->loc, "ICE: lowering function '%s' before it was parsed at %s:%d", f->name->data, __FILE__, __LINE__);
    Allocator al = ci_allocator(ci);
    CiFuncOps* ops = Allocator_zalloc(al, sizeof *ops);
    if(!ops) return CI_OOM_ERROR;
    AtomMap(uintptr_t) labels = {0};
    CiLowerCtx ctx = {
        .a = al,
        .out = &ops->code,
        .labels = &labels,
        .temp = f->frame_size, // temps stack above params + locals
        .frame_size = &f->frame_size,
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
            if(o->kind == CI_OP_SWITCH && o->sw.table)
                Allocator_free(al, o->sw.table, o->sw.count * sizeof *o->sw.table);
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
    CiLowerCtx ctx = {
        .a = ci_allocator(ci),
        .out = ops,
        .labels = labels,
        .temp = 0, // toplevel/module slots hold only temps; each batch recycles them
        .frame_size = slot_size,
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

#ifdef __clang__
#pragma clang assume_nonnull end
#endif
#endif
