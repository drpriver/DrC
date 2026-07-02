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
    uint32_t* frame_size; // temp slots allocate from here: &func->frame_size, or the toplevel/module slot size
    Marray(CiBackpatchTarget) backpatches;
};

static int ci_lower_stmt(CiInterpreter* ci, CiLowerCtx* ctx, CcStmtNode*_Nullable n);
static int ci_alloc_slot(CiLowerCtx*, uint32_t sz, uint32_t align, uint32_t* slot);
static void ci_backpatch_break_continue(CiLowerCtx*, size_t start, uint32_t break_target, uint32_t continue_target);
static void ci_backpatch_break(CiLowerCtx*, size_t start, uint32_t break_target);
static int ci_lower_resolve_gotos(CiInterpreter*, CiLowerCtx*);
static int ci_cmp_switch_entry(void*_Null_unspecified ctx, const void* a, const void* b);

static
int
ci_lower_stmt(CiInterpreter* ci, CiLowerCtx* ctx, CcStmtNode*_Nullable n){
    int err;
    CcParser* p = &ci->parser;
    if(!n) return 0;
    switch(n->kind){
        case CC_STMT_NULL:
            return 0;
        case CC_STMT_EXPR: {
            CiOp* op;
            err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
            if(err) return err;
            *op = (CiOp){
                .kind  = CI_OP_EVAL,
                .expr = n->exprs[0],
                .loc = n->loc,
            };
            return 0;
        }
        case CC_STMT_COMPOUND: {
            for(uint32_t i = 0; i < n->count; i++){
                err = ci_lower_stmt(ci, ctx, n->stmts[i]);
                if(err) return err;
            }
            return 0;
        }
        case CC_STMT_IF: {
            uint32_t cond_slot, cond_size;
            CcExpr* cond = n->exprs[0];
            err = cc_sizeof_as_uint(p, cond->type, n->loc, &cond_size);
            if(err) return err;
            err = ci_alloc_slot(ctx, cond_size, cond_size, &cond_slot);
            if(err) return err;
            CiOp* op;
            err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
            if(err) return err;
            *op = (CiOp){
                .kind = CI_OP_EVAL_INTO,
                .slot = cond_slot,
                .slot_size = cond_size,
                .loc = cond->loc,
                .expr = cond,
            };
            err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
            if(err) return err;
            *op = (CiOp){
                .kind = CI_OP_JUMP_FALSE,
                .slot = cond_slot,
                .slot_size = cond_size,
                .loc = n->loc,
                .expr = cond,
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
            uint32_t cond_slot, cond_size;
            CcExpr* cond = n->exprs[0];
            err = cc_sizeof_as_uint(p, cond->type, n->loc, &cond_size);
            if(err) return err;
            err = ci_alloc_slot(ctx, cond_size, cond_size, &cond_slot);
            if(err) return err;
            uint32_t cond_idx = (uint32_t)ctx->out->count;
            CiOp* op;
            err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
            if(err) return err;
            *op = (CiOp){
                .kind = CI_OP_EVAL_INTO,
                .slot = cond_slot,
                .slot_size = cond_size,
                .loc = cond->loc,
                .expr = cond,
            };
            err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
            if(err) return err;
            *op = (CiOp){
                .kind = CI_OP_JUMP_FALSE,
                .slot = cond_slot,
                .slot_size = cond_size,
                .loc = n->loc,
                .expr = cond,
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
            uint32_t cond_slot, cond_size;
            err = cc_sizeof_as_uint(p, cond->type, n->loc, &cond_size);
            if(err) return err;
            err = ci_alloc_slot(ctx, cond_size, cond_size, &cond_slot);
            if(err) return err;
            CiOp* op;
            err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
            if(err) return err;
            *op = (CiOp){
                .kind = CI_OP_EVAL_INTO,
                .slot = cond_slot,
                .slot_size = cond_size,
                .loc = cond->loc,
                .expr = cond,
            };
            err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
            if(err) return err;
            *op = (CiOp){
                .kind = CI_OP_JUMP_TRUE,
                .slot = cond_slot,
                .slot_size = cond_size,
                .loc = n->loc,
                .jump = body_start,
                .expr = cond,
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
                uint32_t cond_slot, cond_size;
                err = cc_sizeof_as_uint(p, cond->type, n->loc, &cond_size);
                if(err) return err;
                err = ci_alloc_slot(ctx, cond_size, cond_size, &cond_slot);
                if(err) return err;
                err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
                if(err) return err;
                *op = (CiOp){
                    .kind = CI_OP_EVAL_INTO,
                    .slot = cond_slot,
                    .slot_size = cond_size,
                    .loc = cond->loc,
                    .expr = cond,
                };
                err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
                if(err) return err;
                *op = (CiOp){
                    .kind = CI_OP_JUMP_FALSE,
                    .slot = cond_slot,
                    .slot_size = cond_size,
                    .loc = n->loc,
                    .expr = cond,
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
            uint32_t val_slot, val_size;
            err = cc_sizeof_as_uint(p, e->type, n->loc, &val_size);
            if(err) return err;
            err = ci_alloc_slot(ctx, val_size, val_size, &val_slot);
            if(err) return err;
            CiOp* op;
            err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
            if(err) return err;
            *op = (CiOp){
                .kind = CI_OP_EVAL_INTO,
                .slot = val_slot,
                .slot_size = val_size,
                .loc = e->loc,
                .expr = e,
            };
            err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
            if(err) return err;
            *op = (CiOp){
                .kind = CI_OP_SWITCH,
                .slot = val_slot,
                .slot_size = val_size,
                .loc = n->loc,
                .expr = e,
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
            CiOp* op;
            err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
            if(err) return err;
            *op = (CiOp){
                .kind = CI_OP_RETURN,
                .expr = n->exprs[0],
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

static
int
ci_alloc_slot(CiLowerCtx* ctx, uint32_t sz, uint32_t align, uint32_t* slot){
    uint32_t frm = *ctx->frame_size;
    if(add_overflow(frm, align - 1, &frm))
        return _cc_overflow_error;
    frm &= ~(align - 1);
    uint32_t new_frm;
    if(add_overflow(frm, sz, &new_frm))
        return _cc_overflow_error;
    *slot = frm;
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
