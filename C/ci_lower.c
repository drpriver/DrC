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
static int ci_lower_istrue(CiLowerCtx* ctx, const CiLowerVal* v, CcQualType src_type, uint32_t dest, uint32_t dest_size, _Bool negate, SrcLoc loc);
static int ci_lower_dest(CiLowerCtx* ctx, uint32_t* dest, uint32_t size);
// The base pointer slot and constant byte displacement of a computed address.
typedef struct CiLowerAddr CiLowerAddr;
struct CiLowerAddr {
    uint32_t slot;
    uint32_t disp;
};

static _Bool ci_frame_lvalue(const CcExpr* lv, uint32_t* offset);
// Lower an lvalue to a base pointer slot + displacement. one_past_ok relaxes an
// outermost subscript's bounds check to permit forming (not accessing) a
// one-past-the-end address; it is always false when recursing into a base.
static int ci_lower_addr(CiInterpreter* ci, CiLowerCtx* ctx, CcExpr* lv, _Bool one_past_ok, CiLowerAddr* out, _Bool* handled);
static int ci_lower_lvalue_addr(CiInterpreter* ci, CiLowerCtx* ctx, CcExpr* lv, _Bool one_past_ok, CiLowerAddr* out, _Bool* handled);
static _Bool ci_alu_int_type(CcQualType t);
static _Bool ci_falu_type(CcQualType t);
static CiAluOp ci_alu_op_for(CcExprKind kind);
static _Bool ci_falu_op_for(CcExprKind kind, CiFaluOp* out);
static int ci_lower_incdec(CiInterpreter* ci, CiLowerCtx* ctx, CcExpr* e, uint32_t dest, CiLowerVal*_Nullable out, _Bool* handled);
static int ci_lower_call(CiInterpreter* ci, CiLowerCtx* ctx, CcExpr* e, uint32_t dest, CiLowerVal*_Nullable out, _Bool* handled);
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
                .conv.is_unsigned = is_unsigned,
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
// Control-flow expressions (&&, ||, ?:, comma) lower to jumps; 
// dest is the requested slot, or CI_NO_SLOT to allocate one.
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
            memcpy(&op->immediate, &e->uinteger, sizeof op->immediate);
            uint64_t val = op->immediate;
            if(size < 8)
                val &= ((uint64_t)1 << (size*8)) - 1;
            out->canonical = val <= 1;
            return 0;
        }
        case CC_EXPR_VARIABLE: {
            CcVariable* var = e->var;
            if(e->type.is_atomic)
                break; // atomics need an atomic load
            if(!var->automatic){
                // static/global: address through the GOT-style op, then load
                if(ccqt_kind(e->type) == CC_ARRAY)
                    break; // array rvalues only decay
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
                    .kind = CI_OP_VAR_ADDR,
                    .slot = aslot,
                    .slot_size = 8,
                    .var = var,
                    .loc = e->loc,
                };
                err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
                if(err) return err;
                *op = (CiOp){
                    .kind = CI_OP_LOAD,
                    .slot = dest,
                    .slot_size = size,
                    .src = aslot,
                    .loc = e->loc,
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
                .kind = CI_OP_COPY,
                .slot = dest,
                .slot_size = size,
                .src = (uint32_t)var->frame_offset,
                .src_size = size,
                .loc = e->loc,
            };
            return 0;
        }
        case CC_EXPR_DEREF:
        case CC_EXPR_ARROW:
        case CC_EXPR_DOT:
        case CC_EXPR_SUBSCRIPT: {
            if(e->type.is_atomic)
                break; // atomic loads fall back
            if(ccqt_kind(e->type) == CC_ARRAY)
                break; // array rvalues only decay; no direct loads
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
                    .kind = CI_OP_COPY,
                    .slot = dest,
                    .slot_size = size,
                    .src = off,
                    .src_size = size,
                    .loc = e->loc,
                };
                return 0;
            }
            err = ci_lower_dest(ctx, &dest, size);
            if(err) return err;
            uint32_t temp = ctx->temp;
            CiLowerAddr a;
            _Bool handled;
            err = ci_lower_addr(ci, ctx, e, 0, &a, &handled); // access
            if(err) return err;
            if(!handled){
                ctx->temp = temp;
                break; // bitfields, array/slice subscripts, etc fall back
            }
            out->slot = dest;
            CiOp* op;
            err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
            if(err) return err;
            *op = (CiOp){
                .kind = CI_OP_LOAD,
                .slot = dest,
                .slot_size = size,
                .src = a.slot,
                .load.offset = a.disp,
                .loc = e->loc,
            };
            ctx->temp = temp;
            return 0;
        }
        case CC_EXPR_ADDR: {
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
                    .kind = CI_OP_SLOT_ADDR,
                    .slot = dest,
                    .slot_size = size,
                    .src = off,
                    .loc = e->loc,
                };
                return 0;
            }
            CiLowerAddr a;
            _Bool handled;
            err = ci_lower_addr(ci, ctx, lv, 1, &a, &handled); // address-of: one-past ok
            if(err) return err;
            if(!handled)
                break; // nothing was emitted
            if(a.disp == 0 && dest == CI_NO_SLOT){
                out->slot = a.slot;
                return 0;
            }
            err = ci_lower_dest(ctx, &dest, size);
            if(err) return err;
            out->slot = dest;
            if(a.disp == 0){
                err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
                if(err) return err;
                *op = (CiOp){
                    .kind = CI_OP_COPY,
                    .slot = dest,
                    .slot_size = size,
                    .src = a.slot,
                    .src_size = size,
                    .loc = e->loc,
                };
                return 0;
            }
            // addr and displacement temps recycle at statement end
            uint32_t cslot;
            err = ci_alloc_slot(ctx, 8, 8, &cslot);
            if(err) return err;
            err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
            if(err) return err;
            *op = (CiOp){
                .kind = CI_OP_CONST,
                .slot = cslot,
                .slot_size = 8,
                .immediate = a.disp,
                .loc = e->loc,
            };
            err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
            if(err) return err;
            *op = (CiOp){
                .kind = CI_OP_ALU,
                .slot = dest,
                .slot_size = size,
                .src = a.slot,
                .src_size = 8,
                .src2 = cslot,
                .src2_size = 8,
                .alu = {
                    .op = CI_ALU_ADD,
                    .is_unsigned = 1,
                },
                .loc = e->loc,
            };
            return 0;
        }
        case CC_EXPR_CAST: {
            // CC_EXPR_CAST covers several distinct operations; dispatch like
            // the evaluator does, and only take sizeof once the cast is known
            // to be a scalar conversion (array decay has an incomplete
            // source type, for example).
            CcExpr* operand = e->lhs;
            CcQualType from = operand->type;
            CcQualType to = e->type;
            if(ccqt_kind(from) == CC_FUNCTION)
                break; // function-to-pointer falls back
            // Qualifier-only cast: pass through directly.
            if((from.bits & ~(uintptr_t)7) == (to.bits & ~(uintptr_t)7))
                return ci_lower_expr(ci, ctx, operand, dest, out);
            if(ccqt_kind(from) == CC_ARRAY)
                break; // array decay and array-to-slice fall back
            // Scalar conversions. A pointer is an 8-byte value here; the
            // evaluator reads it with ccqt_is_unsigned (false), which is
            // bit-identical to unsigned at full width, so CI_OP_CONVERT covers
            // int<->pointer and pointer<->pointer casts too.
            _Bool from_float = ci_falu_type(from);
            _Bool from_int = ccqt_kind(from) == CC_POINTER;
            if(!from_int && ci_alu_int_type(from)){
                uint32_t from_sz;
                err = cc_sizeof_as_uint(p, from, e->loc, &from_sz);
                if(err) return err;
                from_int = from_sz <= 8;
            }
            if(!from_int && !from_float)
                break; // 128-bit, long double, non-scalar fall back
            _Bool to_int = ccqt_kind(to) == CC_POINTER || (ci_alu_int_type(to) && size <= 8);
            _Bool to_float = ci_falu_type(to);
            CiOpKind kind;
            uint32_t is_unsigned = 0;
            if(from_int && to_int){
                kind = CI_OP_CONVERT;
                is_unsigned = ccqt_is_unsigned(from, !ci_target(ci)->char_is_signed);
            }
            else if(from_int && to_float){
                kind = CI_OP_ITOF;
                is_unsigned = ccqt_is_unsigned(from, !ci_target(ci)->char_is_signed);
            }
            else if(from_float && to_int){
                kind = CI_OP_FTOI;
                is_unsigned = ccqt_is_unsigned(to, !ci_target(ci)->char_is_signed);
            }
            else if(from_float && to_float){
                kind = CI_OP_FTOF;
            }
            else {
                break; // pointers, 128-bit, long double, etc fall back
            }
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
            *op = (CiOp){
                .kind = kind,
                .slot = dest,
                .slot_size = size,
                .src = v.slot,
                .src_size = v.size,
                .conv.is_unsigned = is_unsigned,
                .loc = e->loc,
            };
            ctx->temp = temp;
            // a canonical 0/1 survives integer widening and truncation
            out->canonical = kind == CI_OP_CONVERT && v.canonical;
            return 0;
        }
        case CC_EXPR_POS:
            // unary plus is a no-op; the evaluator passes through
            return ci_lower_expr(ci, ctx, e->lhs, dest, out);
        case CC_EXPR_NEG:
        case CC_EXPR_BITNOT: {
            CcExpr* operand = e->lhs;
            CiOpKind kind;
            uint32_t extra;
            if(e->kind == CC_EXPR_NEG && ci_falu_type(e->type)){
                kind = e->type.basic.kind == CCBT_float? CI_OP_FALU32 : CI_OP_FALU64;
                extra = (uint32_t)CI_FALU_NEG;
            }
            else if(ci_alu_int_type(e->type) && size <= 8){
                // NEG reads signed and negates; NOT reads unsigned and
                // complements, mirroring the evaluator
                _Bool is_not = e->kind == CC_EXPR_BITNOT;
                kind = CI_OP_ALU;
                extra = (uint32_t)(is_not? CI_ALU_NOT : CI_ALU_NEG) | ((uint32_t)is_not << 16);
            }
            else {
                break; // long double and 128-bit integers fall back
            }
            uint32_t osz;
            err = cc_sizeof_as_uint(p, operand->type, operand->loc, &osz);
            if(err) return err;
            if(osz != size)
                break;
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
            *op = (CiOp){
                .kind = kind,
                .slot = dest,
                .slot_size = size,
                .src = v.slot,
                .src_size = v.size,
                .src2 = v.slot,
                .src2_size = v.size,
                .extra_ = extra,
                .loc = e->loc,
            };
            ctx->temp = temp;
            return 0;
        }
        case CC_EXPR_LOGNOT: {
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
        case CC_EXPR_ASSIGN: {
            CcExpr* lhs = e->lhs;
            CcExpr* rhs = e->values[0];
            if(lhs->type.is_atomic)
                break;
            uint32_t rsz;
            err = cc_sizeof_as_uint(p, rhs->type, rhs->loc, &rsz);
            if(err) return err;
            if(rsz != size)
                break;
            uint32_t vslot;
            if(ci_frame_lvalue(lhs, &vslot)){
                // mirror the tree evaluator: the rhs evaluates directly into
                // the lvalue's storage
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
                    .kind = CI_OP_COPY,
                    .slot = dest,
                    .slot_size = size,
                    .src = vslot,
                    .src_size = size,
                    .loc = e->loc,
                };
                return 0;
            }
            // store through a computed address; the lvalue's side effects
            // come first, like the evaluator. addr and value slots recycle
            // at statement end.
            CiLowerAddr a;
            _Bool handled;
            err = ci_lower_addr(ci, ctx, lhs, 0, &a, &handled); // access
            if(err) return err;
            if(!handled)
                break; // bitfields, array/slice subscripts fall back
            CiLowerVal v;
            err = ci_lower_expr(ci, ctx, rhs, dest, &v);
            if(err) return err;
            CiOp* op;
            err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
            if(err) return err;
            *op = (CiOp){
                .kind = CI_OP_STORE,
                .slot = a.slot,
                .src = v.slot,
                .src_size = size,
                .store.offset = a.disp,
                .loc = e->loc,
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
        case CC_EXPR_RSHIFTASSIGN: {
            CcExpr* lhs = e->lhs;
            CcExpr* rhs = e->values[0];
            // The parser casts the rhs to the lhs type (cc_implicit_cast), so
            // both operands share e->type here. Decide the arithmetic before
            // emitting anything with side effects (the lvalue is evaluated
            // once).
            CiOpKind opkind;
            CiFaluOp fop = 0;
            if(ci_alu_int_type(e->type) && ci_alu_int_type(rhs->type)){
                uint32_t rsz;
                err = cc_sizeof_as_uint(p, rhs->type, rhs->loc, &rsz);
                if(err) return err;
                if(size > 8 || rsz > 8)
                    break;
                opkind = CI_OP_ALU;
            }
            else if(ci_falu_type(e->type) && ci_falu_op_for(e->kind, &fop)){
                opkind = size == 4? CI_OP_FALU32 : CI_OP_FALU64;
            }
            else {
                break; // pointer compound assignment falls back
            }
            // Resolve the target: a frame slot operated on in place, or a
            // computed address loaded into a temp and stored back.
            uint32_t save = ctx->temp;
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
                break;
            }
            else {
                is_mem = 1;
                err = ci_alloc_slot(ctx, size, size, &cur);
                if(err) return err;
            }
            uint32_t keep = ctx->temp; // cur is established; the rest is scratch
            if(is_mem){
                _Bool handled;
                err = ci_lower_addr(ci, ctx, lhs, 0, &a, &handled); // access
                if(err) return err;
                if(!handled){
                    // ci_lower_addr emits nothing when it declines, so the
                    // fallback re-evaluates the lvalue exactly once
                    ctx->temp = save;
                    break;
                }
                err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
                if(err) return err;
                *op = (CiOp){
                    .kind = CI_OP_LOAD,
                    .slot = cur,
                    .slot_size = size,
                    .src = a.slot,
                    .load.offset = a.disp,
                    .loc = e->loc,
                };
            }
            CiLowerVal r;
            err = ci_lower_expr(ci, ctx, rhs, CI_NO_SLOT, &r);
            if(err) return err;
            err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
            if(err) return err;
            *op = (CiOp){
                .kind = opkind,
                .slot = cur,
                .slot_size = size,
                .src = cur,
                .src_size = size,
                .src2 = r.slot,
                .src2_size = r.size,
                .loc = e->loc,
            };
            if(opkind == CI_OP_ALU){
                op->alu.op = ci_alu_op_for(e->kind);
                op->alu.is_unsigned = ccqt_is_unsigned(e->type, !ci_target(ci)->char_is_signed);
            }
            else {
                op->falu.op = fop;
            }
            if(is_mem){
                err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
                if(err) return err;
                *op = (CiOp){
                    .kind = CI_OP_STORE,
                    .slot = a.slot,
                    .src = cur,
                    .src_size = size,
                    .store.offset = a.disp,
                    .loc = e->loc,
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
                .kind = CI_OP_COPY,
                .slot = dest,
                .slot_size = size,
                .src = cur,
                .src_size = size,
                .loc = e->loc,
            };
            return 0;
        }
        case CC_EXPR_PREINC:
        case CC_EXPR_PREDEC:
        case CC_EXPR_POSTINC:
        case CC_EXPR_POSTDEC: {
            _Bool handled;
            err = ci_lower_incdec(ci, ctx, e, dest, out, &handled);
            if(err) return err;
            if(handled) return 0;
            break;
        }
        case CC_EXPR_CALL: {
            _Bool handled;
            err = ci_lower_call(ci, ctx, e, dest, out, &handled);
            if(err) return err;
            if(handled) return 0;
            break;
        }
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
        case CC_EXPR_GE: {
            CcExpr* lhs = e->lhs;
            CcExpr* rhs = e->values[0];
            _Bool canonical = 0;
            switch((uint32_t)e->kind){
                case CC_EXPR_EQ:
                case CC_EXPR_NE:
                case CC_EXPR_LT:
                case CC_EXPR_GT:
                case CC_EXPR_LE:
                case CC_EXPR_GE:
                    canonical = 1;
                    break;
                default:
                    break;
            }
            CiOpKind kind;
            uint32_t extra;
            if(ci_alu_int_type(lhs->type) && ci_alu_int_type(rhs->type)){
                uint32_t lsz, rsz;
                err = cc_sizeof_as_uint(p, lhs->type, lhs->loc, &lsz);
                if(err) return err;
                err = cc_sizeof_as_uint(p, rhs->type, rhs->loc, &rsz);
                if(err) return err;
                if(lsz > 8 || rsz > 8 || size > 8)
                    break; // 128-bit integers fall back
                _Bool is_unsigned = ccqt_is_unsigned(lhs->type, !ci_target(ci)->char_is_signed);
                kind = CI_OP_ALU;
                extra = (uint32_t)ci_alu_op_for(e->kind) | ((uint32_t)is_unsigned << 16);
            }
            else if(ci_falu_type(lhs->type) && ci_falu_type(rhs->type)
                 && lhs->type.basic.kind == rhs->type.basic.kind){
                CiFaluOp fop;
                if(!ci_falu_op_for(e->kind, &fop))
                    break; // no float %, &, |, ^, shifts
                kind = lhs->type.basic.kind == CCBT_float? CI_OP_FALU32 : CI_OP_FALU64;
                extra = (uint32_t)fop;
            }
            else {
                break; // pointers, mixed float widths, etc fall back
            }
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
                .kind = kind,
                .slot = dest,
                .slot_size = size,
                .src = l.slot,
                .src_size = l.size,
                .src2 = r.slot,
                .src2_size = r.size,
                .extra_ = extra,
                .loc = e->loc,
            };
            ctx->temp = temp;
            out->canonical = canonical;
            return 0;
        }
        case CC_EXPR_LOGAND:
        case CC_EXPR_LOGOR: {
            // work = istrue(lhs); if(!work) goto end; work = istrue(rhs); end:
            // (|| jumps on true instead)
            // A caller-supplied dest may alias state the rhs reads
            // (x = x && f(x)) and the lhs's truth value lands in the work
            // slot before the rhs evaluates, so work in a fresh slot then.
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
                .kind = e->kind == CC_EXPR_LOGAND? CI_OP_JUMP_FALSE : CI_OP_JUMP_TRUE,
                .slot = work,
                .slot_size = size,
                .loc = e->loc,
            };
            ptrdiff_t jump = (char*)&op->jump - (char*)ctx->out->data;
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
                    .kind = CI_OP_COPY,
                    .slot = dest,
                    .slot_size = size,
                    .src = work,
                    .src_size = size,
                    .loc = e->loc,
                };
            }
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

// Lower eligible ++/-- (automatic variable of integer or pointer type) into
// ops. out is null when the value is unused (statement context); *handled is
// 0 when the expression must fall back instead.
static
int
ci_lower_incdec(CiInterpreter* ci, CiLowerCtx* ctx, CcExpr* e, uint32_t dest, CiLowerVal*_Nullable out, _Bool* handled){
    int err;
    CcParser* p = &ci->parser;
    *handled = 0;
    CcExpr* lhs = e->lhs;
    uint32_t size;
    err = cc_sizeof_as_uint(p, e->type, e->loc, &size);
    if(err) return err;
    uint64_t step = 1;
    if(ccqt_kind(e->type) == CC_POINTER){
        uint32_t pointee_sz;
        err = cc_sizeof_as_uint(p, ccqt_as_ptr(e->type)->pointee, e->loc, &pointee_sz);
        if(err) return err;
        step = pointee_sz;
    }
    else if(!ci_alu_int_type(e->type) || size > 8)
        return 0; // floats and 128-bit integers fall back
    _Bool is_pre = e->kind == CC_EXPR_PREINC || e->kind == CC_EXPR_PREDEC;
    _Bool is_inc = e->kind == CC_EXPR_PREINC || e->kind == CC_EXPR_POSTINC;
    CiOp* op;
    uint32_t vslot;
    if(ci_frame_lvalue(lhs, &vslot)){
        *handled = 1;
        if(out && !is_pre){
            // the post forms yield the old value, captured before modifying
            err = ci_lower_dest(ctx, &dest, size);
            if(err) return err;
            err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
            if(err) return err;
            *op = (CiOp){
                .kind = CI_OP_COPY,
                .slot = dest,
                .slot_size = size,
                .src = vslot,
                .src_size = size,
                .loc = e->loc,
            };
            out->slot = dest;
        }
        uint32_t temp = ctx->temp;
        uint32_t sslot;
        err = ci_alloc_slot(ctx, 8, 8, &sslot);
        if(err) return err;
        err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
        if(err) return err;
        *op = (CiOp){
            .kind = CI_OP_CONST,
            .slot = sslot,
            .slot_size = 8,
            .immediate = step,
            .loc = e->loc,
        };
        err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
        if(err) return err;
        *op = (CiOp){
            .kind = CI_OP_ALU,
            .slot = vslot,
            .slot_size = size,
            .src = vslot,
            .src_size = size,
            .src2 = sslot,
            .src2_size = 8,
            .alu = {
                .op = is_inc?CI_ALU_ADD:CI_ALU_SUB,
                .is_unsigned = 1,
            },
            .loc = e->loc,
        };
        ctx->temp = temp;
        if(out && is_pre){
            if(dest == CI_NO_SLOT){
                out->slot = vslot;
            }
            else {
                err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
                if(err) return err;
                *op = (CiOp){
                    .kind = CI_OP_COPY,
                    .slot = dest,
                    .slot_size = size,
                    .src = vslot,
                    .src_size = size,
                    .loc = e->loc,
                };
                out->slot = dest;
            }
        }
        return 0;
    }
    if(lhs->type.is_atomic) return 0;
    // Memory path: load-modify-store through a computed address, evaluated
    // once. old holds the loaded value, new the incremented value.
    uint32_t save = ctx->temp;
    uint32_t old;
    err = ci_alloc_slot(ctx, size, size, &old);
    if(err) return err;
    CiLowerAddr a;
    _Bool ok;
    err = ci_lower_addr(ci, ctx, lhs, 0, &a, &ok); // access
    if(err) return err;
    if(!ok){
        // nothing was emitted; the caller falls back and re-evaluates once
        ctx->temp = save;
        return 0;
    }
    *handled = 1;
    err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
    if(err) return err;
    *op = (CiOp){
        .kind = CI_OP_LOAD,
        .slot = old,
        .slot_size = size,
        .src = a.slot,
        .load.offset = a.disp,
        .loc = e->loc,
    };
    uint32_t newv;
    err = ci_alloc_slot(ctx, size, size, &newv);
    if(err) return err;
    uint32_t sslot;
    err = ci_alloc_slot(ctx, 8, 8, &sslot);
    if(err) return err;
    err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
    if(err) return err;
    *op = (CiOp){
        .kind = CI_OP_CONST,
        .slot = sslot,
        .slot_size = 8,
        .immediate = step,
        .loc = e->loc,
    };
    err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
    if(err) return err;
    *op = (CiOp){
        .kind = CI_OP_ALU,
        .slot = newv,
        .slot_size = size,
        .src = old,
        .src_size = size,
        .src2 = sslot,
        .src2_size = 8,
        .alu = {
            .op = is_inc?CI_ALU_ADD:CI_ALU_SUB,
            .is_unsigned = 1,
        },
        .loc = e->loc,
    };
    err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
    if(err) return err;
    *op = (CiOp){
        .kind = CI_OP_STORE,
        .slot = a.slot,
        .src = newv,
        .src_size = size,
        .store.offset = a.disp,
        .loc = e->loc,
    };
    if(out){
        // pre yields the new value, post the old; both live in kept slots
        out->slot = is_pre? newv : old;
        if(dest != CI_NO_SLOT){
            err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
            if(err) return err;
            *op = (CiOp){
                .kind = CI_OP_COPY,
                .slot = dest,
                .slot_size = size,
                .src = out->slot,
                .src_size = size,
                .loc = e->loc,
            };
            out->slot = dest;
        }
    }
    return 0;
}

// Lower an eligible call (direct, defined, non-variadic) into a CI_OP_CALL:
// arguments stage into a caller-frame block laid out like the callee's
// parameter area. out is null when the return value is unused; *handled is 0
// (with nothing emitted) when the call must fall back to the tree evaluator.
static
int
ci_lower_call(CiInterpreter* ci, CiLowerCtx* ctx, CcExpr* e, uint32_t dest, CiLowerVal*_Nullable out, _Bool* handled){
    int err;
    CcParser* p = &ci->parser;
    *handled = 0;
    CcExpr* callee = e->lhs;
    if(callee->kind != CC_EXPR_FUNCTION)
        return 0; // indirect calls fall back
    CcFunc* func = callee->func;
    if(!func->defined || !func->parsed)
        return 0; // native/FFI calls; unparsed bodies have no param layout yet
    CcFunction* ftype = func->type;
    uint32_t nargs = e->call.nargs;
    if(ftype->is_variadic || nargs != ftype->param_count)
        return 0;
    // The staged block mirrors the callee's parameter area.
    uint32_t extent = 0;
    for(uint32_t i = 0; i < nargs; i++){
        CcVariable* var = func->param_vars[i];
        if(!var) continue; // unnamed parameter: the evaluator skips its arg
        uint32_t psz, asz;
        err = cc_sizeof_as_uint(p, ftype->params[i], e->loc, &psz);
        if(err) return err;
        err = cc_sizeof_as_uint(p, e->values[i]->type, e->values[i]->loc, &asz);
        if(err) return err;
        if(asz != psz)
            return 0;
        uint32_t end = (uint32_t)var->frame_offset + psz;
        if(end > extent) extent = end;
    }
    *handled = 1;
    uint32_t ret_size = 0;
    if(out){
        err = ci_lower_dest(ctx, &dest, out->size);
        if(err) return err;
        out->slot = dest;
        ret_size = out->size;
    }
    uint32_t temp = ctx->temp;
    uint32_t stage = 0;
    if(extent){
        err = ci_alloc_slot(ctx, extent, 16, &stage);
        if(err) return err;
    }
    for(uint32_t i = 0; i < nargs; i++){
        CcVariable* var = func->param_vars[i];
        if(!var) continue;
        CiLowerVal v;
        err = ci_lower_expr(ci, ctx, e->values[i], stage + (uint32_t)var->frame_offset, &v);
        if(err) return err;
    }
    CiOp* op;
    err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
    if(err) return err;
    *op = (CiOp){
        .kind = CI_OP_CALL,
        .slot = out? dest : 0,
        .slot_size = ret_size,
        .src = stage,
        .src_size = extent,
        .func = func,
        .loc = e->loc,
    };
    ctx->temp = temp;
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
            return 0;
        case CC_EXPR_ASSIGN: {
            uint32_t off;
            uint32_t k = e->lhs->kind;
            // frame lvalues and computed addresses (statics included) lower
            if(!ci_frame_lvalue(e->lhs, &off)
                && k != CC_EXPR_VARIABLE && k != CC_EXPR_DEREF
                && k != CC_EXPR_ARROW && k != CC_EXPR_DOT
                && k != CC_EXPR_SUBSCRIPT)
                break;
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
        case CC_EXPR_RSHIFTASSIGN: {
            uint32_t off;
            uint32_t k = e->lhs->kind;
            // frame lvalues and computed addresses lower; anything else EVALs
            if(!ci_frame_lvalue(e->lhs, &off)
                && k != CC_EXPR_VARIABLE && k != CC_EXPR_DEREF
                && k != CC_EXPR_ARROW && k != CC_EXPR_DOT
                && k != CC_EXPR_SUBSCRIPT)
                break;
            CiLowerVal v;
            return ci_lower_expr(ci, ctx, e, CI_NO_SLOT, &v);
        }
        case CC_EXPR_PREINC:
        case CC_EXPR_PREDEC:
        case CC_EXPR_POSTINC:
        case CC_EXPR_POSTDEC: {
            _Bool handled;
            err = ci_lower_incdec(ci, ctx, e, CI_NO_SLOT, NULL, &handled);
            if(err) return err;
            if(handled) return 0;
            break;
        }
        case CC_EXPR_CALL: {
            _Bool handled;
            err = ci_lower_call(ci, ctx, e, CI_NO_SLOT, NULL, &handled);
            if(err) return err;
            if(handled) return 0;
            break;
        }
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
        default:
            break;
    }
    // fallback: evaluate the (sub)tree for its side effects
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

static
int
ci_lower_dest(CiLowerCtx* ctx, uint32_t* dest, uint32_t size){
    if(*dest != CI_NO_SLOT) return 0;
    return ci_alloc_slot(ctx, size, size, dest);
}

// Lvalues that resolve to a static frame slot offset at lowering time:
// automatic variables and (nested) non-bitfield members of them. Statics,
// derefs, bitfields, and atomics do not.
static
_Bool
ci_frame_lvalue(const CcExpr* lv, uint32_t* offset){
    switch((uint32_t)lv->kind){
        case CC_EXPR_VARIABLE:
            if(!lv->var->automatic) return 0;
            if(lv->type.is_atomic) return 0;
            *offset = (uint32_t)lv->var->frame_offset;
            return 1;
        case CC_EXPR_DOT: {
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

// Lower an lvalue expression to a base pointer slot + constant displacement.
// Emits nothing when it returns *handled = 0. The address slots stay
// allocated (no ctx->temp restore) so callers can consume them.
static
int
ci_lower_addr(CiInterpreter* ci, CiLowerCtx* ctx, CcExpr* lv, _Bool one_past_ok, CiLowerAddr* out, _Bool* handled){
    int err;
    CcParser* p = &ci->parser;
    *handled = 0;
    switch((uint32_t)lv->kind){
        case CC_EXPR_VARIABLE: {
            CcVariable* var = lv->var;
            if(var->automatic) return 0; // frame lvalues are slots, not addresses
            uint32_t aslot;
            err = ci_alloc_slot(ctx, 8, 8, &aslot);
            if(err) return err;
            CiOp* op;
            err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
            if(err) return err;
            *op = (CiOp){
                .kind = CI_OP_VAR_ADDR,
                .slot = aslot,
                .slot_size = 8,
                .var = var,
                .loc = lv->loc,
            };
            out->slot = aslot;
            out->disp = 0;
            *handled = 1;
            return 0;
        }
        case CC_EXPR_DEREF: {
            CiLowerVal v;
            err = ci_lower_expr(ci, ctx, lv->lhs, CI_NO_SLOT, &v);
            if(err) return err;
            out->slot = v.slot;
            out->disp = 0;
            *handled = 1;
            return 0;
        }
        case CC_EXPR_ARROW: {
            if(lv->field_loc.bit_width) return 0;
            CiLowerVal v;
            err = ci_lower_expr(ci, ctx, lv->values[0], CI_NO_SLOT, &v);
            if(err) return err;
            out->slot = v.slot;
            out->disp = (uint32_t)lv->field_loc.byte_offset;
            *handled = 1;
            return 0;
        }
        case CC_EXPR_DOT: {
            if(lv->field_loc.bit_width) return 0;
            err = ci_lower_addr(ci, ctx, lv->values[0], 0, out, handled); // base must be a valid object
            if(err) return err;
            if(!*handled) return 0;
            out->disp += (uint32_t)lv->field_loc.byte_offset;
            return 0;
        }
        case CC_EXPR_SUBSCRIPT: {
            CcExpr* base = lv->lhs;
            CcExpr* idx = lv->values[0];
            CcTypeKind bk = ccqt_kind(base->type);
            uint32_t idx_sz;
            err = cc_sizeof_as_uint(p, idx->type, idx->loc, &idx_sz);
            if(err) return err;
            if(idx_sz > 8 || !ci_alu_int_type(idx->type)) return 0;
            uint32_t elem_sz;
            err = cc_sizeof_as_uint(p, lv->type, lv->loc, &elem_sz);
            if(err) return err;
            CiOp* op;
            // Resolve the base pointer, and a length slot when the element
            // count is known (arrays: constant; slices: the runtime .count).
            uint32_t base_ptr;     // slot holding an 8-byte base pointer
            uint32_t base_disp = 0;// offset folded into the result displacement
            _Bool do_check = 0;
            uint32_t len_slot = 0;
            if(bk == CC_POINTER){
                CiLowerVal b;
                err = ci_lower_expr(ci, ctx, base, CI_NO_SLOT, &b);
                if(err) return err;
                base_ptr = b.slot; // pointers carry no bounds
            }
            else if(bk == CC_ARRAY){
                CcArray* arr = ccqt_as_array(base->type);
                CiLowerAddr ba;
                _Bool ok;
                err = ci_lower_lvalue_addr(ci, ctx, base, 0, &ba, &ok);
                if(err) return err;
                if(!ok) return 0;
                base_ptr = ba.slot;
                base_disp = ba.disp;
                // Elide the check only for genuine flexible-array-member idioms:
                // a C99 FLA (incomplete), a zero-length member, or a length-1
                // member at the end of a struct (the struct hack). A real member
                // array is still checked.
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
                if(!skip){
                    err = ci_alloc_slot(ctx, 8, 8, &len_slot);
                    if(err) return err;
                    err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
                    if(err) return err;
                    *op = (CiOp){
                        .kind = CI_OP_CONST,
                        .slot = len_slot,
                        .slot_size = 8,
                        .immediate = arr->length,
                        .loc = lv->loc,
                    };
                    do_check = 1;
                }
            }
            else if(bk == CC_SLICE){
                CiLowerVal sv;
                err = ci_lower_expr(ci, ctx, base, CI_NO_SLOT, &sv); // {count@0, data@8}
                if(err) return err;
                base_ptr = sv.slot + 8; // .data
                len_slot = sv.slot;     // .count
                do_check = 1;
            }
            else {
                return 0;
            }
            CiLowerVal iv;
            err = ci_lower_expr(ci, ctx, idx, CI_NO_SLOT, &iv);
            if(err) return err;
            // widen the index to 8 bytes with its own signedness; the unsigned
            // bounds compare then also rejects negative indices
            _Bool idx_unsigned = ccqt_is_unsigned(idx->type, !ci_target(ci)->char_is_signed);
            uint32_t widx = iv.slot;
            if(iv.size != 8){
                err = ci_alloc_slot(ctx, 8, 8, &widx);
                if(err) return err;
                err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
                if(err) return err;
                *op = (CiOp){
                    .kind = CI_OP_CONVERT,
                    .slot = widx,
                    .slot_size = 8,
                    .src = iv.slot,
                    .src_size = iv.size,
                    .conv.is_unsigned = idx_unsigned,
                    .loc = idx->loc,
                };
            }
            if(do_check){
                err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
                if(err) return err;
                *op = (CiOp){
                    .kind = CI_OP_BOUNDS,
                    .src = widx,
                    .src_size = 8,
                    .src2 = len_slot,
                    .src2_size = 8,
                    .bounds = {
                        .inclusive = one_past_ok,
                        .index_signed = !idx_unsigned,
                    },
                    .loc = lv->loc,
                };
            }
            // scale by the element size
            uint32_t scaled = widx;
            if(elem_sz != 1){
                uint32_t cslot;
                err = ci_alloc_slot(ctx, 8, 8, &cslot);
                if(err) return err;
                err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
                if(err) return err;
                *op = (CiOp){
                    .kind = CI_OP_CONST,
                    .slot = cslot,
                    .slot_size = 8,
                    .immediate = elem_sz,
                    .loc = lv->loc,
                };
                err = ci_alloc_slot(ctx, 8, 8, &scaled);
                if(err) return err;
                err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
                if(err) return err;
                *op = (CiOp){
                    .kind = CI_OP_ALU,
                    .slot = scaled,
                    .slot_size = 8,
                    .src = widx,
                    .src_size = 8,
                    .src2 = cslot,
                    .src2_size = 8,
                    .alu = {
                        .op = CI_ALU_MUL,
                        .is_unsigned = 1,
                    },
                    .loc = lv->loc,
                };
            }
            uint32_t addr;
            err = ci_alloc_slot(ctx, 8, 8, &addr);
            if(err) return err;
            err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
            if(err) return err;
            *op = (CiOp){
                .kind = CI_OP_ALU,
                .slot = addr,
                .slot_size = 8,
                .src = base_ptr,
                .src_size = 8,
                .src2 = scaled,
                .src2_size = 8,
                .alu = {
                    .op = CI_ALU_ADD,
                    .is_unsigned = 1,
                },
                .loc = lv->loc,
            };
            out->slot = addr;
            out->disp = base_disp;
            *handled = 1;
            return 0;
        }
        default:
            return 0;
    }
}

// Like ci_lower_addr, but also takes the address of a frame-slot lvalue (a
// local variable or member of one) via CI_OP_SLOT_ADDR. Used to reach the base
// of an array subscript, whose storage may live in a slot rather than behind a
// pointer.
static
int
ci_lower_lvalue_addr(CiInterpreter* ci, CiLowerCtx* ctx, CcExpr* lv, _Bool one_past_ok, CiLowerAddr* out, _Bool* handled){
    uint32_t off;
    if(ci_frame_lvalue(lv, &off)){
        uint32_t aslot;
        int err = ci_alloc_slot(ctx, 8, 8, &aslot);
        if(err) return err;
        CiOp* op;
        err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
        if(err) return err;
        *op = (CiOp){
            .kind = CI_OP_SLOT_ADDR,
            .slot = aslot,
            .slot_size = 8,
            .src = off,
            .loc = lv->loc,
        };
        out->slot = aslot;
        out->disp = 0;
        *handled = 1;
        return 0;
    }
    return ci_lower_addr(ci, ctx, lv, one_past_ok, out, handled);
}

static
_Bool
ci_alu_int_type(CcQualType t){
    if(ccqt_kind(t) == CC_ENUM)
        return 1;
    return ccqt_is_basic(t) && ccbt_is_integer(t.basic.kind);
}

// float16, long double, etc fall back to the tree evaluator
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
        case CC_EXPR_EQ:  *out = CI_FALU_EQ;  return 1;
        case CC_EXPR_NE:  *out = CI_FALU_NE;  return 1;
        case CC_EXPR_LT:  *out = CI_FALU_LT;  return 1;
        case CC_EXPR_GT:  *out = CI_FALU_GT;  return 1;
        case CC_EXPR_LE:  *out = CI_FALU_LE;  return 1;
        case CC_EXPR_GE:  *out = CI_FALU_GE;  return 1;
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
        case CC_EXPR_EQ:     return CI_ALU_EQ;
        case CC_EXPR_NE:     return CI_ALU_NE;
        case CC_EXPR_LT:     return CI_ALU_LT;
        case CC_EXPR_GT:     return CI_ALU_GT;
        case CC_EXPR_LE:     return CI_ALU_LE;
        case CC_EXPR_GE:     return CI_ALU_GE;
    }
    return CI_ALU_ADD; // unreachable: callers only pass the kinds above
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
        .kind = CI_OP_ISTRUE,
        .slot = dest,
        .slot_size = dest_size,
        .src = v->slot,
        .src_size = v->size,
        .is_true = {
            .float_kind = float_kind,
            .negate = negate,
        },
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
