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

#ifdef __clang__
#pragma clang assume_nonnull begin
#endif

static void ci_op_print(const CiOp* op, MStringBuilder* out);
static void ci_op_print_range(MStringBuilder* out, uint32_t slot, uint32_t size);
static void ci_op_print_deref(MStringBuilder* out, uint32_t slot, uint32_t offset);
static const char* ci_op_alu_sym(CiAluOp op);
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
        case CI_ALU_EQ:  return "==";
        case CI_ALU_NE:  return "!=";
        case CI_ALU_LT:  return "<";
        case CI_ALU_GT:  return ">";
        case CI_ALU_LE:  return "<=";
        case CI_ALU_GE:  return ">=";
        case CI_ALU_NEG: return "-";
        case CI_ALU_NOT: return "~";
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
        case CI_FALU_EQ:  return "==";
        case CI_FALU_NE:  return "!=";
        case CI_FALU_LT:  return "<";
        case CI_FALU_GT:  return ">";
        case CI_FALU_LE:  return "<=";
        case CI_FALU_GE:  return ">=";
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
void
ci_op_print(const CiOp* op, MStringBuilder* out){
    switch(op->kind){
        case CI_OP_EVAL:
            msb_write_literal(out, "(void)");
            cc_print_expr(out, op->eval.expr);
            break;
        case CI_OP_EVAL_INTO:
            ci_op_print_range(out, op->eval_into.slot, op->eval_into.slot_size);
            msb_write_literal(out, " = ");
            cc_print_expr(out, op->eval_into.expr);
            break;
        case CI_OP_CONST:
            ci_op_print_range(out, op->constant.slot, op->constant.immsize);
            if(op->constant.immsize > 8){
                msb_sprintf(out, " = 0x%016llx%016llx",
                    (unsigned long long)op->constant.immediate[1],
                    (unsigned long long)op->constant.immediate[0]);
            }
            else {
                unsigned long long v = op->constant.immediate[0];
                msb_sprintf(out, " = %llu", v);
                if(v > 9) msb_sprintf(out, " (0x%llx)", v);
            }
            break;
        case CI_OP_COPY:
            ci_op_print_range(out, op->copy.slot, op->copy.slot_size);
            msb_write_literal(out, " = ");
            ci_op_print_range(out, op->copy.src, op->copy.src_size);
            break;
        case CI_OP_ALU:
            ci_op_print_range(out, op->alu.slot, op->alu.slot_size);
            msb_write_literal(out, " = ");
            if(op->alu.op >= CI_ALU_NEG){
                msb_sprintf(out, "%s%s", ci_op_alu_sym(op->alu.op), op->alu.is_unsigned?"u":"");
                ci_op_print_range(out, op->alu.src, op->alu.src_size);
            }
            else {
                ci_op_print_range(out, op->alu.src, op->alu.src_size);
                msb_sprintf(out, " %s%s ", ci_op_alu_sym(op->alu.op), op->alu.is_unsigned?"u":"");
                ci_op_print_range(out, op->alu.src2, op->alu.src2_size);
            }
            break;
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
            ci_op_print_range(out, op->var_addr.slot, op->var_addr.slot_size);
            msb_sprintf(out, " = &%.*s", (int)name->length, name->data);
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
            Atom name = op->call.func->name;
            if(op->call.ret_size){
                ci_op_print_range(out, op->call.ret_slot, op->call.ret_size);
                msb_write_literal(out, " = ");
            }
            msb_sprintf(out, "call %.*s(", (int)name->length, name->data);
            ci_op_print_range(out, op->call.src, op->call.src_size);
            msb_write_char(out, ')');
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
    }
}

#ifdef __clang__
#pragma clang assume_nonnull end
#endif
#endif
