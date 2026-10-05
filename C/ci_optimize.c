#ifndef CI_OPTIMIZE_C
#define CI_OPTIMIZE_C
//
// Copyright © 2026-2026, David Priver <david@davidpriver.com>
//
#include "../Drp/switch_macros.h"
#include "ci_optimize.h"
#include "ci_interp.h"
#ifdef __clang__
#pragma clang assume_nonnull begin
#endif
//
// For now we only do inlining, but one day!
//
enum {CI_INLINE_OP_THRESHOLD=10};

static
void
ci_inline_cleanup(Marray(CiOp)* code, Allocator al){
    for(size_t i = 0; i < code->count; i++){
        CiOp* op = &code->data[i];
        if(op->kind == CI_OP_SWITCH && op->switch_.table)
            Allocator_free(al, op->switch_.table, sizeof *op->switch_.table + op->switch_.table->count*sizeof op->switch_.table->data[0]);
    }
    ma_cleanup(CiOp)(code, al);
}

typedef struct CiInlineSlots CiInlineSlots;
struct CiInlineSlots {
    uint32_t base,
             args_slot,
             args_size;
};

static
uint32_t
ci_inline_slot(const CiInlineSlots* slots, uint32_t slot){
    return slot < slots->args_size ? slots->args_slot+slot : slots->base+slot;
}

static
void
ci_inline_relocate(CiOp* op, const CiInlineSlots* slots, const uint32_t* map){
#define CI_REMAP(slot) ((slot) = ci_inline_slot(slots, (slot)))
    switch(op->kind){
        DRP_CASES_EXHAUSTED;
        case CI_OP_CONST:
            CI_REMAP(op->constant.slot);
            break;
        case CI_OP_COPY:
            CI_REMAP(op->copy.slot);
            CI_REMAP(op->copy.src);
            break;
        case CI_OP_ALU_IMM32:
        case CI_OP_ALU_IMM64:
            CI_REMAP(op->alu_imm.slot);
            CI_REMAP(op->alu_imm.src);
            break;
        case CI_OP_INDEX:
            CI_REMAP(op->index.slot);
            CI_REMAP(op->index.base);
            CI_REMAP(op->index.index);
            break;
        case CI_OP_ALU8:
        case CI_OP_ALU16:
        case CI_OP_ALU32:
        case CI_OP_ALU64:
        case CI_OP_ALU128:
            CI_REMAP(op->alu.slot);
            CI_REMAP(op->alu.src);
            CI_REMAP(op->alu.src2);
            break;
        case CI_OP_CMP32:
        case CI_OP_CMP64:
        case CI_OP_CMP128:
            CI_REMAP(op->cmp.slot);
            CI_REMAP(op->cmp.src);
            CI_REMAP(op->cmp.src2);
            break;
        case CI_OP_CMP_JUMP32:
        case CI_OP_CMP_JUMP64:
            CI_REMAP(op->cmp_jump.src);
            CI_REMAP(op->cmp_jump.src2);
            op->cmp_jump.jump = map[op->cmp_jump.jump];
            break;
        case CI_OP_FALU32:
        case CI_OP_FALU64:
        case CI_OP_FALU80:
        case CI_OP_FALU128:
            CI_REMAP(op->falu32.slot);
            CI_REMAP(op->falu32.src);
            CI_REMAP(op->falu32.src2);
            break;
        case CI_OP_FCMP32:
        case CI_OP_FCMP64:
        case CI_OP_FCMP80:
        case CI_OP_FCMP128:
            CI_REMAP(op->fcmp32.slot);
            CI_REMAP(op->fcmp32.src);
            CI_REMAP(op->fcmp32.src2);
            break;
        case CI_OP_CHECKED:
            CI_REMAP(op->checked.result);
            CI_REMAP(op->checked.overflow);
            CI_REMAP(op->checked.src);
            CI_REMAP(op->checked.src2);
            break;
        case CI_OP_BIT_BUILTIN:
            CI_REMAP(op->bit_builtin.slot);
            CI_REMAP(op->bit_builtin.src);
            CI_REMAP(op->bit_builtin.src2);
            break;
        case CI_OP_CONVERT:
        case CI_OP_ITOF:
        case CI_OP_FTOI:
        case CI_OP_FTOF:
            CI_REMAP(op->convert.slot);
            CI_REMAP(op->convert.src);
            break;
        case CI_OP_SLOT_ADDR:
            CI_REMAP(op->slot_addr.slot);
            CI_REMAP(op->slot_addr.src);
            break;
        case CI_OP_VAR_ADDR:
        case CI_OP_TLS_ADDR:
            CI_REMAP(op->var_addr.slot);
            break;
        case CI_OP_FUNC_ADDR:
            CI_REMAP(op->func_addr.slot);
            break;
        case CI_OP_BOUNDS:
            CI_REMAP(op->bounds.src);
            CI_REMAP(op->bounds.src2);
            break;
        case CI_OP_LOAD:
            CI_REMAP(op->load.slot);
            CI_REMAP(op->load.src);
            break;
        case CI_OP_STORE:
            CI_REMAP(op->store.slot);
            CI_REMAP(op->store.src);
            break;
        case CI_OP_STORE_IMM:
            CI_REMAP(op->store_imm.slot);
            break;
        case CI_OP_MEMCOPY:
            CI_REMAP(op->memcopy.slot);
            CI_REMAP(op->memcopy.src);
            break;
        case CI_OP_ZERO:
            CI_REMAP(op->zero.slot);
            break;
        case CI_OP_LOAD_BITFIELD:
            CI_REMAP(op->load_bf.slot);
            CI_REMAP(op->load_bf.src);
            break;
        case CI_OP_STORE_BITFIELD:
            CI_REMAP(op->store_bf.slot);
            CI_REMAP(op->store_bf.src);
            break;
        case CI_OP_CALL:
            CI_REMAP(op->call.args_slot);
            CI_REMAP(op->call.ret_slot);
            break;
        case CI_OP_ISTRUE:
            CI_REMAP(op->istrue.slot);
            CI_REMAP(op->istrue.src);
            break;
        case CI_OP_JUMP:
            op->jump.jump = map[op->jump.jump];
            break;
        case CI_OP_JUMP_FALSE:
        case CI_OP_JUMP_TRUE:
            CI_REMAP(op->jump_false.slot);
            op->jump_false.jump = map[op->jump_false.jump];
            break;
        case CI_OP_RETURN:
            break;
        case CI_OP_RETURN_SLOT:
            CI_REMAP(op->return_slot.src);
            break;
        case CI_OP_SWITCH:
            CI_REMAP(op->switch_.slot);
            op->switch_.jump = map[op->switch_.jump];
            if(op->switch_.table)
                for(size_t i = 0; i < op->switch_.table->count; i++)
                    op->switch_.table->data[i].target = map[op->switch_.table->data[i].target];
            break;
        case CI_OP_ATOMIC_LOAD:
            CI_REMAP(op->atomic_load.slot);
            CI_REMAP(op->atomic_load.src);
            break;
        case CI_OP_ATOMIC_STORE:
            CI_REMAP(op->atomic_store.slot);
            CI_REMAP(op->atomic_store.src);
            break;
        case CI_OP_ATOMIC_RMW:
            CI_REMAP(op->atomic_rmw.slot);
            CI_REMAP(op->atomic_rmw.src);
            CI_REMAP(op->atomic_rmw.src2);
            break;
        case CI_OP_ATOMIC_CAS:
            CI_REMAP(op->atomic_cas.slot);
            CI_REMAP(op->atomic_cas.src);
            CI_REMAP(op->atomic_cas.expected);
            CI_REMAP(op->atomic_cas.desired);
            break;
        case CI_OP_FENCE:
        case CI_OP_BUILTIN:
            break;
        case CI_OP_ALLOCA:
            CI_REMAP(op->alloca.slot);
            CI_REMAP(op->alloca.src);
            break;
        case CI_OP_BSWAP:
            CI_REMAP(op->bswap.slot);
            CI_REMAP(op->bswap.src);
            break;
        case CI_OP_VA_START:
            CI_REMAP(op->va_start_.slot);
            break;
        case CI_OP_VA_ARG:
            CI_REMAP(op->va_arg_.slot);
            CI_REMAP(op->va_arg_.src);
            break;
        case CI_OP_RT_CALL:
            CI_REMAP(op->rt_call.slot);
            for(unsigned i = 0; i < op->rt_call.nargs; i++)
                CI_REMAP(op->rt_call.args[i]);
            break;
    }
#undef CI_REMAP
}

static
int
ci_inline_append(CiInterpreter* ci, Marray(CiOp)* out, CiOp op, const CiInlineSlots* slots, const uint32_t* map){
    Allocator al = ci_allocator(ci);
    CiSwitchTable* table = NULL;
    if(op.kind == CI_OP_SWITCH && op.switch_.table){
        size_t size = sizeof *op.switch_.table + op.switch_.table->count*sizeof op.switch_.table->data[0];
        table = Allocator_dupe(al, op.switch_.table, size);
        if(!table) return CI_OOM_ERROR;
        op.switch_.table = table;
    }
    ci_inline_relocate(&op, slots, map);
    int err = ma_push(CiOp)(out, al, op);
    if(err && table) Allocator_free(al, table, sizeof *op.switch_.table + table->count*sizeof op.switch_.table->data[0]);
    return err ? CI_OOM_ERROR : 0;
}

static
CcFunc*_Nullable
ci_inline_candidate(const CiOp* op){
    if(op->kind != CI_OP_CALL || op->call.is_indirect)
        return NULL;
    CiCallDescriptor* d = op->call.descrip;
    CcFunc* f = d->func;
    if(!f->inline_ && !f->always_inline) return NULL;
    if(!f->interp_ops) return NULL;
    if(f->interp_ops->inline_state != CI_INLINE_FINISHED) return NULL;
    if(f->interp_ops->inline_blocked) return NULL;
    if(f->type->is_variadic) return NULL;
    if(f->type->no_prototype) return NULL;
    if(d->call_type != f->type) return NULL;
    if(d->nargs != f->params.count) return NULL;
    if(!f->always_inline && f->interp_ops->code.count > CI_INLINE_OP_THRESHOLD) return NULL;
    return f;
}


static
size_t
ci_inline_op_size(const CiOp* op, const CiOp* call, _Bool last){
    if(op->kind == CI_OP_RETURN || op->kind == CI_OP_RETURN_SLOT)
        return (size_t)(op->kind == CI_OP_RETURN_SLOT && call->call.ret_size) + !last;
    return 1;
}

static
int
ci_inline_expand(CiInterpreter* ci, Marray(CiOp)* out, const CiOp* call, CcFunc* f, uint32_t base, uint32_t end){
    Allocator al = ci_allocator(ci);
    Marray(CiOp)* body = &f->interp_ops->code;
    uint32_t* map = Allocator_alloc(al, (body->count+1)*sizeof *map);
    if(!map) return CI_OOM_ERROR;
    int err = 0;
    CiCallDescriptor* d = call->call.descrip;
    CiInlineSlots slots = {
        .base=base-(d->fixed_size & ~15u),
        .args_slot=call->call.args_slot,
        .args_size=d->fixed_size,
    };
    uint32_t pc = (uint32_t)out->count;
    for(size_t i = 0; i < body->count; i++){
        map[i] = pc;
        pc += (uint32_t)ci_inline_op_size(&body->data[i], call, i+1 == body->count);
    }
    map[body->count] = end;
    for(size_t i = 0; i < body->count; i++){
        CiOp op = body->data[i];
        if(op.kind == CI_OP_RETURN || op.kind == CI_OP_RETURN_SLOT){
            if(op.kind == CI_OP_RETURN_SLOT && call->call.ret_size){
                CiOp copy = {.copy = {.kind=CI_OP_COPY, .slot=call->call.ret_slot,
                    .slot_size=op.return_slot.src_size, .src=ci_inline_slot(&slots, op.return_slot.src),
                    .src_size=op.return_slot.src_size, .loc=op.loc}};
                if(ma_push(CiOp)(out, al, copy)){ err = CI_OOM_ERROR; goto cleanup; }
            }
            if(i+1 != body->count){
                CiOp jump = {.jump = {.kind=CI_OP_JUMP, .jump=end, .loc=op.loc}};
                if(ma_push(CiOp)(out, al, jump)){ err = CI_OOM_ERROR; goto cleanup; }
            }
        }
        else {
            err = ci_inline_append(ci, out, op, &slots, map);
            if(err) goto cleanup;
        }
    }
    cleanup:
    Allocator_free(al, map, (body->count+1)*sizeof *map);
    return err;
}

static
int
ci_inline_code(CiInterpreter* ci, Marray(CiOp)* code, uint32_t* frame_size, size_t start, AtomMap(uintptr_t)*_Nullable labels, size_t*_Nullable pc){
    _Bool found = 0;
    for(size_t i = start; i < code->count; i++)
        if(ci_inline_candidate(&code->data[i])){ found = 1; break; }
    if(!found) return 0;
    Allocator al = ci_allocator(ci);
    size_t map_size = (code->count+1)*sizeof(uint32_t);
    uint32_t* map = Allocator_alloc(al, map_size);
    if(!map) return CI_OOM_ERROR;
    Marray(CiOp) out = {0};
    int err = 0;
    uint64_t base = ((uint64_t)*frame_size+15) & ~(uint64_t)15;
    uint64_t size = *frame_size, count = 0;
    for(size_t i = 0; i < code->count; i++){
        map[i] = (uint32_t)count;
        CcFunc* f = i >= start ? ci_inline_candidate(&code->data[i]) : NULL;
        if(f){
            uint32_t prefix = code->data[i].call.descrip->fixed_size & ~15u;
            if(base+f->frame_size-prefix > size) size = base+f->frame_size-prefix;
            for(size_t j = 0; j < f->interp_ops->code.count; j++)
                count += ci_inline_op_size(&f->interp_ops->code.data[j], &code->data[i], j+1 == f->interp_ops->code.count);
        }
        else count++;
        if(size > UINT32_MAX || count > UINT32_MAX){
            err = CI_OOM_ERROR;
            goto cleanup;
        }
    }
    map[code->count] = (uint32_t)count;
    for(size_t i = 0; i < code->count; i++){
        CcFunc* f = i >= start ? ci_inline_candidate(&code->data[i]) : NULL;
        if(f) err = ci_inline_expand(ci, &out, &code->data[i], f, (uint32_t)base, map[i+1]);
        else {
            CiInlineSlots slots = {0};
            err = ci_inline_append(ci, &out, code->data[i], &slots, map);
        }
        if(err) goto cleanup;
    }
    if(labels){
        AtomMapItems items = AM_items((AtomMap(uintptr_t)*_Nonnull)labels);
        for(size_t i = 0; i < items.count; i++)
            if(items.data[i].atom != nil_atom && items.data[i].p)
                items.data[i].p = (void*)(uintptr_t)(map[(uintptr_t)items.data[i].p-1]+1);
    }
    if(pc) *pc = map[*pc];
    ci_inline_cleanup(code, al);
    *code = out;
    out = (Marray(CiOp)){0};
    *frame_size = (uint32_t)size;
    cleanup:
    ci_inline_cleanup(&out, al);
    Allocator_free(al, map, map_size);
    return err;
}

static
int
ci_optimize_code(CiInterpreter* ci, Marray(CiOp)* code, uint32_t* frame_size, size_t start, AtomMap(uintptr_t)*_Nullable labels, size_t*_Nullable pc){
    return ci_inline_code(ci, code, frame_size, start, labels, pc);
}

typedef struct CiInlineVisit CiInlineVisit;
struct CiInlineVisit {
    CcFunc* func;
    const CiInlineVisit*_Nullable parent;
};

static
int
ci_inline_visit(CiInterpreter* ci, CcFunc* f, const CiInlineVisit*_Nullable parent){
    CiFuncOps* ops = f->interp_ops;
    if(!ops || ops->inline_state == CI_INLINE_FINISHED) return 0;
    // prevent inline recursion loops
    if(ops->inline_state == CI_INLINE_VISITING){
        for(const CiInlineVisit* p = parent; p; p = p->parent){
            p->func->interp_ops->inline_blocked = 1;
            if(p->func == f) break;
        }
        return 0;
    }
    ops->inline_state = CI_INLINE_VISITING;
    CiInlineVisit visit = {.func=f, .parent=parent};
    int err = 0;
    for(size_t i = 0; i < ops->code.count; i++){
        const CiOp* op = &ops->code.data[i];
        if(op->kind == CI_OP_ALLOCA || op->kind == CI_OP_VA_START)
            ops->inline_blocked = 1;
        if(op->kind == CI_OP_CALL && !op->call.is_indirect){
            err = ci_inline_visit(ci, op->call.descrip->func, &visit);
            if(err) goto cleanup;
        }
    }
    err = ci_inline_code(ci, &ops->code, &f->frame_size, 0, NULL, NULL);
    cleanup:
    ops->inline_state = err ? CI_INLINE_UNVISITED : CI_INLINE_FINISHED;
    return err;
}

static
int
ci_inline_func(CiInterpreter* ci, CcFunc* f){
    return ci_inline_visit(ci, f, NULL);
}

static
int
ci_optimize_func(CiInterpreter* ci, CcFunc* f){
    return ci_inline_func(ci, f);
}

static
int
ci_inline_toplevel(CiInterpreter* ci){
    int err = ci_inline_code(ci, &ci->toplevel_ops, &ci->toplevel_slot_size, ci->toplevel_inlined, &ci->toplevel_labels, &ci->top_frame.pc);
    if(err) return err;
    if(ci->toplevel_slot_size > ci->toplevel_slots_cap){
        uint32_t cap = ci->toplevel_slot_size;
        void* slots = Allocator_zalloc(ci_allocator(ci), cap);
        if(!slots) return CI_OOM_ERROR;
        if(ci->toplevel_slots){
            memcpy(slots, ci->toplevel_slots, ci->toplevel_slots_cap);
            Allocator_free(ci_allocator(ci), ci->toplevel_slots, ci->toplevel_slots_cap);
        }
        ci->toplevel_slots = slots;
        ci->toplevel_slots_cap = cap;
    }
    ci->toplevel_inlined = ci->toplevel_ops.count;
    ci->top_frame.ops = ci->toplevel_ops.data;
    ci->top_frame.op_count = ci->toplevel_ops.count;
    ci->top_frame.slots = ci->toplevel_slots;
    return 0;
}
static
int
ci_optimize_toplevel(CiInterpreter* ci){
    return ci_inline_toplevel(ci);
}
#ifdef __clang__
#pragma clang assume_nonnull end
#endif
#endif
