#ifndef C_CI_OP_PRINTER_H
#define C_CI_OP_PRINTER_H
//
// Copyright © 2026-2026, David Priver <david@davidpriver.com>
//
#include <stdint.h>
#include <stddef.h>
#include "../Drp/MStringBuilder.h"
#include "../Drp/msb_sprintf.h"
#include "ci_op.h"
#include "ci_interp.h"
#include "cc_errors.h"
#include "cc_expr.h"
#include "cc_func.h"
#include "cc_var.h"
#include "cc_type.h"
#include "cc_memory_order.h"

#ifdef __clang__
#pragma clang assume_nonnull begin
#endif

static void ci_op_print(const CiOp* op, MStringBuilder* out);
static void ci_op_print_range(MStringBuilder* out, uint32_t slot, uint32_t size);
static void ci_op_print_deref(MStringBuilder* out, uint32_t slot, uint32_t offset);
static const char* ci_op_alu_sym(CiAluOp op);
static const char* ci_op_cmp_sym(CiCmpOp op);
static const char* ci_op_falu_sym(CiFaluOp op);
static const char* ci_op_float_suffix(uint32_t float_kind);

static
void
ci_op_print_range(MStringBuilder* out, uint32_t slot, uint32_t size){
    msb_sprintf(out, "[%u:%u]", slot, slot+size);
}

static
void
ci_op_print_deref(MStringBuilder* out, uint32_t slot, uint32_t offset){
    if(offset)
        msb_sprintf(out, "*([%u]+%u)", slot, offset);
    else
        msb_sprintf(out, "*[%u]", slot);
}

static
const char*
ci_op_alu_sym(CiAluOp op){
    switch(op){
        case CI_ALU_ADD: return "+";
        case CI_ALU_SUB: return "-";
        case CI_ALU_MUL: return "*";
        case CI_ALU_DIV: return "/";
        case CI_ALU_MOD: return "%";
        case CI_ALU_AND: return "&";
        case CI_ALU_OR:  return "|";
        case CI_ALU_XOR: return "^";
        case CI_ALU_SHL: return "<<";
        case CI_ALU_SHR: return ">>";
        case CI_ALU_NEG: return "-";
        case CI_ALU_NOT: return "~";
    }
    return "?";
}

static
const char*
ci_op_cmp_sym(CiCmpOp op){
    switch(op){
        case CI_CMP_EQ: return "==";
        case CI_CMP_NE: return "!=";
        case CI_CMP_LT: return "<";
        case CI_CMP_GT: return ">";
        case CI_CMP_LE: return "<=";
        case CI_CMP_GE: return ">=";
    }
    return "?";
}

static
const char*
ci_op_falu_sym(CiFaluOp op){
    switch(op){
        case CI_FALU_ADD: return "+";
        case CI_FALU_SUB: return "-";
        case CI_FALU_MUL: return "*";
        case CI_FALU_DIV: return "/";
        case CI_FALU_NEG: return "-";
    }
    return "?";
}

static
const char*
ci_op_float_suffix(uint32_t float_kind){
    switch((CcBasicTypeKind)float_kind){
        case CCBT_float16:     return ".f16";
        case CCBT_float:       return ".f32";
        case CCBT_double:      return ".f64";
        case CCBT_long_double: return ".f80";
        case CCBT_float128:    return ".f128";
        default:               return ".f?";
    }
}

static
const
char*
ci_op_armw_name(CiAtomicRmwOp op){
    switch(op){
        case CI_ARMW_XCHG: return "xchg";
        case CI_ARMW_ADD:  return "fetch_add";
        case CI_ARMW_SUB:  return "fetch_sub";
        case CI_ARMW_AND:  return "fetch_and";
        case CI_ARMW_OR:   return "fetch_or";
        case CI_ARMW_XOR:  return "fetch_xor";
    }
    return "?";
}

static
const
char*
ci_op_memory_order(CcMemoryOrder m){
    switch((uint32_t)m){
    case CC_MO_RELAXED: return "relaxed";
    case CC_MO_CONSUME: return "consume";
    case CC_MO_ACQUIRE: return "acquire";
    case CC_MO_RELEASE: return "release";
    case CC_MO_ACQ_REL: return "acquire-release";
    case CC_MO_SEQ_CST: return "seq-cst";
    case CC_MO_COUNT: default: return "???";
    }
}

static
void
ci_op_print(const CiOp* op, MStringBuilder* out){
    switch(op->kind){
        case CI_OP_EVAL:
            msb_write_literal(out, "(void)");
            cc_print_expr(out, op->eval.expr);
            msb_write_literal(out, " (tree walker)");
            break;
        case CI_OP_EVAL_INTO:
            ci_op_print_range(out, op->eval_into.slot, op->eval_into.slot_size);
            msb_write_literal(out, " = ");
            cc_print_expr(out, op->eval_into.expr);
            msb_write_literal(out, " (tree walker)");
            break;
        case CI_OP_CONST:
            ci_op_print_range(out, op->constant.slot, op->constant.immsize);
            uint64_t v = op->constant.immediate[0];
            if(op->constant.is_anon_array){
                size_t before = out->cursor;
                switch((CcBasicTypeKind)op->constant.bt_kind){
                    case CCBT_char:
                    case CCBT_unsigned_char:
                    case CCBT_signed_char:
                        msb_write_literal(out, " = \"");
                        before = out->cursor;
                        msb_write_str(out, (const char*)v, op->constant.immediate[1]-1);
                        msb_write_literal(out, "\"");
                        goto fixup;
                    case CCBT_short:
                    case CCBT_unsigned_short:
                        msb_write_literal(out, " = u\"");
                        before = out->cursor;
                        msb_write_utf16(out, (const uint16_t*)v, op->constant.immediate[1]-1);
                        msb_write_literal(out, "\"");
                        goto fixup;
                    case CCBT_int:
                    case CCBT_unsigned:
                        msb_write_literal(out, " = U\"");
                        before = out->cursor;
                        msb_write_utf32(out, ((const uint32_t*)v), op->constant.immediate[1]-1);
                        msb_write_literal(out, "\"");
                        goto fixup;
                    default:
                        break;
                }
                if(0){
                    fixup:
                    for(size_t i = before; i < out->cursor-1; i++){
                        unsigned char c = (unsigned char)out->data[i];
                        char buff[8];
                        char* repl; size_t len;
                        if(c == '\\'){
                            msb_replace_range(out, i, i+1, "\\\\", 2);
                            i += 1;
                        }
                        else if(c == '"'){
                            msb_replace_range(out, i, i+1, "\\\"", 2);
                            i += 1;
                        }
                        else if(c < 32){
                            switch(c){
                                case '\0':
                                    repl = "\\0";
                                    len = 2;
                                    break;
                                case '\r':
                                    repl = "\\r";
                                    len = 2;
                                    break;
                                case '\n':
                                    repl = "\\n";
                                    len = 2;
                                    break;
                                case '\t':
                                    repl = "\\t";
                                    len = 2;
                                    break;
                                case '\f':
                                    repl = "\\f";
                                    len = 2;
                                    break;
                                default:
                                    len = stbsp_snprintf(buff, sizeof buff, "\\x%x", c);
                                    repl = buff;
                                    break;
                            }
                            msb_replace_range(out, i, i+1, repl, len);
                            i += len-1;
                        }
                    }
                    return;
                }
            }
            else
                switch((CcBasicTypeKind)op->constant.bt_kind){
                    case CCBT_bool:
                        msb_sprintf(out, " = %s", v?"true":"false");
                        return;
                    case CCBT_char:
                        if(v >= 32 && v < 127)
                            msb_sprintf(out, " = '%c'", (char)v);
                        else
                            msb_sprintf(out, " = '\\x%x'", (int)v);
                        return;
                    case CCBT_signed_char:
                    case CCBT_short:
                    case CCBT_int:
                    case CCBT_long:
                    case CCBT_long_long:
                        switch(op->constant.immsize){
                            case 1:
                                msb_sprintf(out, " = %lld", (long long)(int8_t)v);
                                return;
                            case 2:
                                msb_sprintf(out, " = %lld", (long long)(int16_t)v);
                                return;
                            case 4:
                                msb_sprintf(out, " = %lld", (long long)(int32_t)v);
                                return;
                            case 8:
                                msb_sprintf(out, " = %lld", (long long)(int64_t)v);
                                return;
                            default:
                                break;
                        }
                        break;
                    case CCBT_unsigned_char:
                    case CCBT_unsigned_short:
                    case CCBT_unsigned:
                    case CCBT_unsigned_long:
                    case CCBT_unsigned_long_long:
                        msb_sprintf(out, " = %llu (0x%llx)", (unsigned long long)v, (unsigned long long)v);
                        return;
                    case CCBT_float:{
                        float f;
                        memcpy(&f, &v, sizeof f);
                        msb_sprintf(out, " = %f (0x%x)", (double)f, (unsigned)v);
                        return;
                    }
                    case CCBT_double:{
                        double f;
                        memcpy(&f, &v, sizeof f);
                        msb_sprintf(out, " = %f (0x%llx)", f, (unsigned long long)v);
                        return;
                    }
                    case CCBT__Type:
                        msb_write_literal(out, " = ");
                        cc_print_type(out, (CcQualType){.bits=v});
                        return;
                    default:
                        break;
                }
            if(op->constant.immsize > 8){
                msb_sprintf(out, " = 0x%016llx%016llx",
                    (unsigned long long)op->constant.immediate[1],
                    (unsigned long long)op->constant.immediate[0]);
            }
            else {
                msb_sprintf(out, " = %llu", (unsigned long long)v);
                if(v > 9) msb_sprintf(out, " (0x%llx)", (unsigned long long)v);
            }
            break;
        case CI_OP_COPY:
            ci_op_print_range(out, op->copy.slot, op->copy.slot_size);
            msb_write_literal(out, " = ");
            ci_op_print_range(out, op->copy.src, op->copy.src_size);
            break;
        case CI_OP_ALU8:{
            uint32_t sz;
            sz = 1;
            goto op_alu;
        case CI_OP_ALU16:
            sz = 2;
            goto op_alu;
        case CI_OP_ALU32:
            sz = 4;
            goto op_alu;
        case CI_OP_ALU64:
            sz=8;
            goto op_alu;
        case CI_OP_ALU128:
            sz=16;
            goto op_alu;
            op_alu:;
            ci_op_print_range(out, op->alu.slot, sz);
            msb_write_literal(out, " = ");
            if(op->alu.op >= CI_ALU_NEG){
                msb_sprintf(out, "%s%s", ci_op_alu_sym(op->alu.op), op->alu.is_unsigned?"u":"");
                ci_op_print_range(out, op->alu.src, sz);
            }
            else {
                ci_op_print_range(out, op->alu.src, sz);
                msb_sprintf(out, " %s%s ", ci_op_alu_sym(op->alu.op), op->alu.is_unsigned?"u":"");
                ci_op_print_range(out, op->alu.src2, sz);
            }
            break;
        }
        case CI_OP_CMP32:{
            uint32_t sz;
            sz = 4;
            goto op_cmp;
        case CI_OP_CMP64:
            sz = 8;
            goto op_cmp;
        case CI_OP_CMP128:
            sz = 16;
            goto op_cmp;
            op_cmp:;
            ci_op_print_range(out, op->cmp.slot, op->cmp.slot_size);
            msb_write_literal(out, " = ");
            ci_op_print_range(out, op->cmp.src, sz);
            msb_sprintf(out, " %s%s ", ci_op_cmp_sym(op->cmp.op), op->cmp.is_unsigned?"u":"");
            ci_op_print_range(out, op->cmp.src2, sz);
            break;
        }
        case CI_OP_FALU32:
        case CI_OP_FALU64:{
            uint32_t width = op->kind == CI_OP_FALU32? 4 : 8;
            const char* suffix = op->kind == CI_OP_FALU32? "f32" : "f64";
            ci_op_print_range(out, op->falu32.slot, op->falu32.slot_size);
            msb_write_literal(out, " = ");
            if(op->falu32.op == CI_FALU_NEG){
                msb_sprintf(out, "%s%s", ci_op_falu_sym(op->falu32.op), suffix);
                ci_op_print_range(out, op->falu32.src, width);
            }
            else {
                ci_op_print_range(out, op->falu32.src, width);
                msb_sprintf(out, " %s%s ", ci_op_falu_sym(op->falu32.op), suffix);
                ci_op_print_range(out, op->falu32.src2, width);
            }
        } break;
        case CI_OP_FCMP32:
        case CI_OP_FCMP64:{
            uint32_t width = op->kind == CI_OP_FCMP32? 4 : 8;
            const char* suffix = op->kind == CI_OP_FCMP32? "f32" : "f64";
            ci_op_print_range(out, op->fcmp32.slot, op->fcmp32.slot_size);
            msb_write_literal(out, " = ");
            ci_op_print_range(out, op->fcmp32.src, width);
            msb_sprintf(out, " %s%s ", ci_op_cmp_sym(op->fcmp32.op), suffix);
            ci_op_print_range(out, op->fcmp32.src2, width);
        } break;
        case CI_OP_CHECKED:{
            const char* sym = op->checked.op == CI_CHK_ADD? "+"
                            : op->checked.op == CI_CHK_SUB? "-" : "*";
            ci_op_print_range(out, op->checked.result, op->checked.res_size);
            msb_write_literal(out, ", ");
            ci_op_print_range(out, op->checked.overflow, 1);
            msb_sprintf(out, " = checked %s ", sym);
            ci_op_print_range(out, op->checked.src, op->checked.src_size);
            msb_write_literal(out, ", ");
            ci_op_print_range(out, op->checked.src2, op->checked.src2_size);
        } break;
        case CI_OP_BITCOUNT:{
            const char* name = op->bitcount.op == CI_BITCNT_POPCOUNT? "popcount"
                             : op->bitcount.op == CI_BITCNT_CLZ? "clz" : "ctz";
            ci_op_print_range(out, op->bitcount.slot, op->bitcount.slot_size);
            msb_sprintf(out, " = %s ", name);
            ci_op_print_range(out, op->bitcount.src, op->bitcount.src_size);
        } break;
        case CI_OP_CONVERT:
            ci_op_print_range(out, op->convert.slot, op->convert.slot_size);
            msb_sprintf(out, " = %s ", op->convert.is_unsigned?"zext":"sext");
            ci_op_print_range(out, op->convert.src, op->convert.src_size);
            break;
        case CI_OP_ITOF:
            ci_op_print_range(out, op->itof.slot, op->itof.slot_size);
            msb_sprintf(out, " = itof%s ", op->itof.is_unsigned?".u":".s");
            ci_op_print_range(out, op->itof.src, op->itof.src_size);
            break;
        case CI_OP_FTOI:
            ci_op_print_range(out, op->ftoi.slot, op->ftoi.slot_size);
            msb_sprintf(out, " = ftoi%s ", op->ftoi.is_unsigned?".u":".s");
            ci_op_print_range(out, op->ftoi.src, op->ftoi.src_size);
            break;
        case CI_OP_FTOF:
            ci_op_print_range(out, op->ftof.slot, op->ftof.slot_size);
            msb_write_literal(out, " = ftof ");
            ci_op_print_range(out, op->ftof.src, op->ftof.src_size);
            break;
        case CI_OP_SLOT_ADDR:
            ci_op_print_range(out, op->slot_addr.slot, op->slot_addr.slot_size);
            msb_sprintf(out, " = &[%u]", op->slot_addr.src);
            break;
        case CI_OP_VAR_ADDR:{
            Atom name = op->var_addr.var->name;
            const char *s = name&&name->length?name->data:"<anon>";
            ci_op_print_range(out, op->var_addr.slot, op->var_addr.slot_size);
            msb_sprintf(out, " = &%s", s);
        } break;
        case CI_OP_FUNC_ADDR:{
            Atom name = op->func_addr.func->name;
            const char *s = name&&name->length?name->data:"<anon>";
            ci_op_print_range(out, op->func_addr.slot, op->func_addr.slot_size);
            msb_sprintf(out, " = &%s", s);
        } break;
        case CI_OP_BOUNDS:
            msb_sprintf(out, "bounds%s ", op->bounds.index_signed?".s":"");
            ci_op_print_range(out, op->bounds.src, op->bounds.src_size);
            msb_sprintf(out, " %s ", op->bounds.inclusive?"<=":"<");
            ci_op_print_range(out, op->bounds.src2, op->bounds.src2_size);
            break;
        case CI_OP_LOAD:
            ci_op_print_range(out, op->load.slot, op->load.slot_size);
            msb_write_literal(out, " = ");
            ci_op_print_deref(out, op->load.src, op->load.offset);
            break;
        case CI_OP_STORE:
            ci_op_print_deref(out, op->store.slot, op->store.offset);
            msb_write_literal(out, " = ");
            ci_op_print_range(out, op->store.src, op->store.src_size);
            break;
        case CI_OP_MEMCOPY:
            ci_op_print_deref(out, op->memcopy.slot, op->memcopy.offset);
            msb_write_literal(out, " = ");
            ci_op_print_deref(out, op->memcopy.src, op->memcopy.src_offset);
            msb_sprintf(out, " (%u bytes)", op->memcopy.size);
            break;
        case CI_OP_ZERO:
            msb_write_literal(out, "zero ");
            ci_op_print_deref(out, op->zero.slot, op->zero.offset);
            msb_sprintf(out, " (%u bytes)", op->zero.size);
            break;
        case CI_OP_LOAD_BITFIELD:
            ci_op_print_range(out, op->load_bf.slot, op->load_bf.slot_size);
            msb_sprintf(out, " = bits%s[%u:%u] of ",
                op->load_bf.is_signed?".s":".u",
                op->load_bf.bit_offset, op->load_bf.bit_offset+op->load_bf.bit_width);
            ci_op_print_deref(out, op->load_bf.src, op->load_bf.offset);
            break;
        case CI_OP_STORE_BITFIELD:
            msb_sprintf(out, "bits%s[%u:%u] of ",
                op->store_bf.is_signed?".s":".u",
                op->store_bf.bit_offset, op->store_bf.bit_offset+op->store_bf.bit_width);
            ci_op_print_deref(out, op->store_bf.slot, op->store_bf.offset);
            msb_write_literal(out, " = ");
            ci_op_print_range(out, op->store_bf.src, op->store_bf.src_size);
            break;
        case CI_OP_CALL:{
            CiCallDescriptor* d = op->call.descrip;
            if(op->call.is_indirect){
                if(op->call.ret_size){
                    ci_op_print_range(out, op->call.ret_slot, op->call.ret_size);
                    msb_write_literal(out, " = ");
                }
                msb_sprintf(out, "call *[%u](", op->call.argv_slot);
                for(uint32_t i = 0; i < d->nargs; i++){
                    if(i) msb_write_literal(out, ", ");
                    msb_sprintf(out, "*[%u]", op->call.argv_slot + 8 + i * 8);
                }
                msb_write_char(out, ')');
            }else {
                Atom name = d->func->name;
                const char *s = name&&name->length?name->data:"<anon>";
                if(op->call.ret_size){
                    ci_op_print_range(out, op->call.ret_slot, op->call.ret_size);
                    msb_write_literal(out, " = ");
                }
                msb_sprintf(out, "call %s(", s);
                for(uint32_t i = 0; i < d->nargs; i++){
                    if(i) msb_write_literal(out, ", ");
                    msb_sprintf(out, "*[%u]", op->call.argv_slot + i * 8);
                }
                msb_write_char(out, ')');
            }
        } break;
        case CI_OP_ISTRUE:
            ci_op_print_range(out, op->istrue.slot, op->istrue.slot_size);
            msb_sprintf(out, " = %s%s ",
                op->istrue.negate?"isfalse":"istrue",
                op->istrue.float_kind?ci_op_float_suffix(op->istrue.float_kind):"");
            ci_op_print_range(out, op->istrue.src, op->istrue.src_size);
            break;
        case CI_OP_JUMP:
            msb_sprintf(out, "jump 0x%x", op->jump.jump);
            break;
        case CI_OP_JUMP_FALSE:
            msb_write_literal(out, "if !");
            ci_op_print_range(out, op->jump_false.slot, op->jump_false.slot_size);
            msb_sprintf(out, " jump 0x%x", op->jump_false.jump);
            break;
        case CI_OP_JUMP_TRUE:
            msb_write_literal(out, "if ");
            ci_op_print_range(out, op->jump_true.slot, op->jump_true.slot_size);
            msb_sprintf(out, " jump 0x%x", op->jump_true.jump);
            break;
        case CI_OP_RETURN:
            msb_write_literal(out, "return");
            break;
        case CI_OP_RETURN_SLOT:
            msb_write_literal(out, "return ");
            ci_op_print_range(out, op->return_slot.src, op->return_slot.src_size);
            break;
        case CI_OP_SWITCH:
            msb_sprintf(out, "switch%s ", op->switch_.is_unsigned?".u":"");
            ci_op_print_range(out, op->switch_.slot, op->switch_.slot_size);
            msb_write_literal(out, " {");
            if(op->switch_.table){
                for(size_t i = 0; i < op->switch_.table->count; i++){
                    CcSwitchEntry e = op->switch_.table->data[i];
                    if(op->switch_.is_unsigned)
                        msb_sprintf(out, "%llu=>0x%x, ", (unsigned long long)e.value, e.target);
                    else
                        msb_sprintf(out, "%lld=>0x%x, ", (long long)e.value, e.target);
                }
            }
            msb_sprintf(out, "default=>0x%x}", op->switch_.jump);
            break;
        case CI_OP_ATOMIC_LOAD:
            ci_op_print_range(out, op->atomic_load.slot, op->atomic_load.slot_size);
            msb_write_literal(out, " = ");
            ci_op_print_deref(out, op->atomic_load.src, op->atomic_load.offset);
            msb_sprintf(out, " (atomic %s)", ci_op_memory_order(op->atomic_load.memorder));
            break;
        case CI_OP_ATOMIC_STORE:
            ci_op_print_deref(out, op->atomic_store.slot, op->atomic_store.offset);
            msb_write_literal(out, " = ");
            ci_op_print_range(out, op->atomic_store.src, op->atomic_store.src_size);
            msb_sprintf(out, " (atomic %s)", ci_op_memory_order(op->atomic_store.memorder));
            break;
        case CI_OP_ATOMIC_RMW:
            ci_op_print_range(out, op->atomic_rmw.slot, op->atomic_rmw.slot_size);
            msb_sprintf(out, " = %s ", ci_op_armw_name(op->atomic_rmw.op));
            ci_op_print_deref(out, op->atomic_rmw.src, op->atomic_rmw.offset);
            msb_write_literal(out, ", ");
            ci_op_print_range(out, op->atomic_rmw.src2, op->atomic_rmw.slot_size);
            msb_sprintf(out, " (atomic %s)%s", ci_op_memory_order(op->atomic_rmw.memorder), op->atomic_rmw.discard?" (discard)":"");
            break;
        case CI_OP_ATOMIC_CAS:
            ci_op_print_range(out, op->atomic_cas.slot, 1);
            msb_sprintf(out, " = cas%s ", op->atomic_cas.weak?".weak":"");
            ci_op_print_deref(out, op->atomic_cas.src, op->atomic_cas.offset);
            msb_write_literal(out, ", expected=");
            ci_op_print_range(out, op->atomic_cas.expected, op->atomic_cas.size);
            msb_write_literal(out, ", desired=");
            ci_op_print_range(out, op->atomic_cas.desired, op->atomic_cas.size);
            msb_sprintf(out, " (atomic %s, fail %s)", ci_op_memory_order(op->atomic_cas.memorder), ci_op_memory_order(op->atomic_cas.fail_memorder));
            break;
        case CI_OP_FENCE:
            msb_sprintf(out, "%s fence %s", op->fence.is_signal?"signal":"atomic", ci_op_memory_order(op->fence.memorder));
            break;
        case CI_OP_ALLOCA:
            msb_sprintf(out, "[%u] = alloca([%u])", op->alloca.slot, op->alloca.src);
            break;
        case CI_OP_BSWAP:
            msb_sprintf(out, "[%u] = bswap%u([%u])", op->bswap.slot, op->bswap.size*8, op->bswap.src);
            break;
        case CI_OP_BUILTIN:
            switch((uint32_t)op->builtin.op){
                case CC_BUILTIN_UNREACHABLE: msb_write_literal(out, "unreachable"); break;
                case CC_BUILTIN_TRAP: msb_write_literal(out, "trap"); break;
                case CC_BUILTIN_DEBUGTRAP: msb_write_literal(out, "debugtrap"); break;
                case CC_BUILTIN_ABORT: msb_write_literal(out, "abort()"); break;
                case CC_BUILTIN_BACKTRACE: msb_write_literal(out, "backtrace()"); break;
                default: msb_write_literal(out, "?invalid builtinop"); break;
            }
            break;
        case CI_OP_VA_START:
            msb_sprintf(out, "va_start(*[%u])", op->va_start_.slot);
            break;
        case CI_OP_VA_ARG:
            ci_op_print_range(out, op->va_arg_.slot, op->va_arg_.slot_size);
            msb_sprintf(out, " = va_arg(*[%u])%s", op->va_arg_.src, op->va_arg_.is_fp?" (fp)":"");
            break;
    }
}

#ifdef __clang__
#pragma clang assume_nonnull end
#endif
#endif
