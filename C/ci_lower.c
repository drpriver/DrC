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
#include "nc_native_call.h"
#include "../Drp/drp_profile.h"
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

#ifndef MARRAY_CCINITENTRY
#define MARRAY_CCINITENTRY
#define MARRAY_T CcInitEntry
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
    union {
        Atom _Null_unspecified label;
        uintptr_t levels; // Remaining loops/switches to exit before patching the jump
                          // we only use 8 bits of this if you need to smuggle some bits in.
    };
    size_t byteoffset;
    SrcLoc loc;
};
#ifndef MARRAY_CIBACKPATCHTARGET
#define MARRAY_CIBACKPATCHTARGET
#define MARRAY_T CiBackpatchTarget
#include "../Drp/Marray.h"
#endif

typedef struct CiFoldValue CiFoldValue;
struct CiFoldValue {
    uint32_t sz;
    CcQualType type;
    uint64_t bits[2];
};
#ifndef MARRAY_CIFOLDVALUE
#define MARRAY_CIFOLDVALUE
#define MARRAY_T CiFoldValue
#include "../Drp/Marray.h"
#endif

#ifdef __clang__
#pragma clang assume_nonnull begin
#endif
static _Bool cc_eval_aggregate_type(CcQualType);
static int cc_lvalue_alignment(CcParser*, const CcExpr*, uint32_t*);
static int cc_type_alignment(CcParser*, CcQualType, SrcLoc, uint32_t*);
static int cc_pointer_type_alignment(CcParser*, CcQualType, SrcLoc, uint32_t, uint32_t*);
static _Bool cc_eval_size_product_valid(CiUint128 length, CiUint128 element_size, unsigned size_bits, uint32_t element_alignment);
static int cc_sizeof_as_expr(CcParser*, CcQualType, SrcLoc, CcExpr*_Nullable*_Nonnull);
static int cc_check_linktime_expr(CcParser*, CcExpr*, _Bool, unsigned);

static
CiAbiRegister
ci_abi_resolve_register(CcTarget target, CiAbiRegister reg){
    static const uint16_t offsets[CC_TARGET_COUNT][CI_ABI_VECTOR_COUNT+1] = {
        [CC_TARGET_X86_64_LINUX] = {
            [CI_ABI_GP]              = offsetof(NcCaptureSysV64, gp),
            [CI_ABI_FP]              = offsetof(NcCaptureSysV64, fp),
            [CI_ABI_RESULT_GP]       = offsetof(NcCaptureSysV64, result_gp),
            [CI_ABI_RESULT_FP]       = offsetof(NcCaptureSysV64, result_fp),
            [CI_ABI_RESULT_X87]      = offsetof(NcCaptureSysV64, result_x87),
            [CI_ABI_STACK_BASE]      = offsetof(NcCaptureSysV64, stack_args),
            [CI_ABI_INDIRECT_RESULT] = offsetof(NcCaptureSysV64, gp),
            [CI_ABI_VECTOR_COUNT]    = offsetof(NcCaptureSysV64, incoming_rax),
        },
        [CC_TARGET_X86_64_MACOS] = {
            [CI_ABI_GP]              = offsetof(NcCaptureSysV64, gp),
            [CI_ABI_FP]              = offsetof(NcCaptureSysV64, fp),
            [CI_ABI_RESULT_GP]       = offsetof(NcCaptureSysV64, result_gp),
            [CI_ABI_RESULT_FP]       = offsetof(NcCaptureSysV64, result_fp),
            [CI_ABI_RESULT_X87]      = offsetof(NcCaptureSysV64, result_x87),
            [CI_ABI_STACK_BASE]      = offsetof(NcCaptureSysV64, stack_args),
            [CI_ABI_INDIRECT_RESULT] = offsetof(NcCaptureSysV64, gp),
            [CI_ABI_VECTOR_COUNT]    = offsetof(NcCaptureSysV64, incoming_rax),
        },
        [CC_TARGET_X86_64_WINDOWS_MSVC] = {
            [CI_ABI_GP]              = offsetof(NcCaptureWin64, gp),
            [CI_ABI_FP]              = offsetof(NcCaptureWin64, fp),
            [CI_ABI_RESULT_GP]       = offsetof(NcCaptureWin64, result_gp),
            [CI_ABI_RESULT_FP]       = offsetof(NcCaptureWin64, result_fp),
            [CI_ABI_STACK_BASE]      = offsetof(NcCaptureWin64, stack_args),
            [CI_ABI_INDIRECT_RESULT] = offsetof(NcCaptureWin64, gp),
        },
        [CC_TARGET_X86_64_WINDOWS_MINGW] = {
            [CI_ABI_GP]              = offsetof(NcCaptureWin64, gp),
            [CI_ABI_FP]              = offsetof(NcCaptureWin64, fp),
            [CI_ABI_RESULT_GP]       = offsetof(NcCaptureWin64, result_gp),
            [CI_ABI_RESULT_FP]       = offsetof(NcCaptureWin64, result_fp),
            [CI_ABI_STACK_BASE]      = offsetof(NcCaptureWin64, stack_args),
            [CI_ABI_INDIRECT_RESULT] = offsetof(NcCaptureWin64, gp),
        },
        [CC_TARGET_AARCH64_LINUX] = {
            [CI_ABI_GP]              = offsetof(NcCaptureArm64, gp),
            [CI_ABI_FP]              = offsetof(NcCaptureArm64, fp),
            [CI_ABI_RESULT_GP]       = offsetof(NcCaptureArm64, result_gp),
            [CI_ABI_RESULT_FP]       = offsetof(NcCaptureArm64, result_fp),
            [CI_ABI_STACK_BASE]      = offsetof(NcCaptureArm64, stack_args),
            [CI_ABI_INDIRECT_RESULT] = offsetof(NcCaptureArm64, indirect_result),
        },
        [CC_TARGET_AARCH64_MACOS] = {
            [CI_ABI_GP]              = offsetof(NcCaptureArm64, gp),
            [CI_ABI_FP]              = offsetof(NcCaptureArm64, fp),
            [CI_ABI_RESULT_GP]       = offsetof(NcCaptureArm64, result_gp),
            [CI_ABI_RESULT_FP]       = offsetof(NcCaptureArm64, result_fp),
            [CI_ABI_STACK_BASE]      = offsetof(NcCaptureArm64, stack_args),
            [CI_ABI_INDIRECT_RESULT] = offsetof(NcCaptureArm64, indirect_result),
        },
        [CC_TARGET_TEST] = {
            [CI_ABI_RESULT_GP]       = offsetof(NcCaptureTest, result_gp),
            [CI_ABI_RESULT_FP]       = offsetof(NcCaptureTest, result_fp),
            [CI_ABI_STACK_BASE]      = offsetof(NcCaptureTest, stack_args),
            [CI_ABI_INDIRECT_RESULT] = offsetof(NcCaptureTest, indirect_result),
        },
    };
    uint32_t stride = reg.bank==CI_ABI_GP || reg.bank==CI_ABI_RESULT_GP ? 8 : 16;
    reg.capture_offset = offsets[target][reg.bank] + reg.index*stride;
    return reg;
}

typedef struct CiAbiAddressSpec CiAbiAddressSpec;
struct CiAbiAddressSpec {
    CiAbiAddress mode;
    union { uint32_t offset, immediate; CiAbiRegister reg; };
    union { uint32_t pointer; CiAbiRegister base; };
};

static
_Bool
ci_abi_has_padding(NcAbiType type){
    uint32_t end = 0;
    for(unsigned i = 0; i < type.count; i++){
        NcAbiPiece piece = type.pieces[i];
        if(piece.value_offset != end) return 1;
        end += piece.size;
    }
    return end != type.size;
}

static
CiAbiAddressSpec
ci_abi_resolve_address(CcTarget target, CiAbiAddressSpec address){
    switch(address.mode){
        case CI_ABI_REG:
        case CI_ABI_INCOMING:
            address.reg = ci_abi_resolve_register(target, address.reg);
            break;
        case CI_ABI_REG_PTR:
        case CI_ABI_INCOMING_PTR:
        case CI_ABI_REG_PTR_INDIRECT:
        case CI_ABI_INCOMING_PTR_INDIRECT:
            address.base = ci_abi_resolve_register(target, address.base);
            break;
        case CI_ABI_GP_SAVE:
        case CI_ABI_FP_SAVE:
            address.base = ci_abi_resolve_register(target, (CiAbiRegister){ .bank = address.mode == CI_ABI_GP_SAVE? CI_ABI_GP : CI_ABI_FP});
            break;
        case CI_ABI_SLOT:
        case CI_ABI_SLOT_PTR:
            break;
        DRP_CASES_EXHAUSTED;
    }
    return address;
}

static
int
ci_abi_emit(Marray(CiOp)* out, Allocator al, SrcLoc loc, CcTarget target, CiAbiTransfer operation, CiAbiAddressSpec dst, CiAbiAddressSpec src, uint32_t size){
    dst = ci_abi_resolve_address(target, dst);
    if(operation != CI_ABI_CONSTANT && operation != CI_ABI_ZERO)
        src = ci_abi_resolve_address(target, src);
    CiOp op = {.abi_transfer={
        .kind=CI_OP_ABI_TRANSFER,
        .operation=operation,
        .source=src.mode,
        .destination=dst.mode,
        .src=src.offset,
        .dst=dst.offset,
        .src_pointer=src.pointer,
        .dst_pointer=dst.pointer,
        .size=size,
        .loc=loc
    }};
    if(operation == CI_ABI_SIGN_EXTEND || operation == CI_ABI_ZERO_EXTEND){
        op.abi_transfer.source_size = size;
        op.abi_transfer.size = 8;
    }
    return ma_push(CiOp)(out, al, op);
}

static
int
ci_lower_abi_argument(Marray(CiOp)* out, Allocator al, const NcCallPlan* p, uint32_t index, uint32_t slot, _Bool incoming, SrcLoc loc){
    NcAbiArgument a = p->args[index];
    NcAbiType t = a.type;
    if(t.mode == NC_ABI_IGNORED) return 0;
    int err;
    _Bool write = !incoming;
    CiAbiAddress regs = incoming ? CI_ABI_INCOMING : CI_ABI_REG;
    CiAbiAddress ptr = incoming ? CI_ABI_INCOMING_PTR : CI_ABI_REG_PTR;
    CiAbiAddressSpec value = {.mode = CI_ABI_SLOT, .offset = slot};
    switch(p->target){
        case CC_TARGET_X86_64_LINUX:
        case CC_TARGET_X86_64_MACOS: {
            if(!a.on_stack && t.mode != NC_ABI_INDIRECT && t.count == 1 && t.size && t.size <= 16){
                NcAbiPiece piece = t.pieces[0];
                _Bool fp = piece.bank == NC_ABI_FP;
                if(!piece.value_offset && piece.size == t.size && (fp || piece.bank == NC_ABI_GP)){
                    CiAbiRegister reg;
                    if(fp)
                        reg = (CiAbiRegister){.bank = CI_ABI_FP, .index = a.fp};
                    else
                        reg = (CiAbiRegister){.bank = CI_ABI_GP, .index = a.gp};
                    reg = ci_abi_resolve_register(p->target, reg);
                    _Bool extend = write && !fp && t.size < 8 && (t.sign_extend || t.zero_extend);
                    CiAbiTransfer operation = CI_ABI_COPY;
                    if(extend && t.sign_extend)
                        operation = CI_ABI_SIGN_EXTEND;
                    else if(extend)
                        operation = CI_ABI_ZERO_EXTEND;
                    return ma_push(CiOp)(out, al, (CiOp){.abi_move = {
                        .kind = CI_OP_ABI_MOVE,
                        .write = write,
                        .incoming = incoming,
                        .operation = operation,
                        .source_size = t.size,
                        .size = extend? 8 : t.size,
                        .slot = slot,
                        .reg = reg,
                        .loc = loc,
                    }});
                }
            }
            if(a.on_stack){
                CiAbiAddressSpec memory = {
                    .mode = ptr,
                    .base = {.bank = CI_ABI_STACK_BASE},
                    .offset = a.stack_offset,
                };
                CiAbiAddressSpec destination, source;
                if(write){
                    destination = memory;
                    source = value;
                }
                else {
                    destination = value;
                    source = memory;
                }
                return ci_abi_emit(out, al, loc, p->target, CI_ABI_COPY, destination, source, t.size);
            }
            if(!write && ci_abi_has_padding(t)){
                err = ci_abi_emit(out, al, loc, p->target, CI_ABI_ZERO, value, (CiAbiAddressSpec){.immediate = 0}, t.size);
                if(err) return err;
            }
            unsigned gp = a.gp, fp = a.fp;
            for(unsigned i = 0; i < t.count; i++){
                NcAbiPiece piece = t.pieces[i];
                CiAbiAddressSpec reg = {.mode = regs};
                if(piece.bank == NC_ABI_FP){
                    reg.reg.bank = CI_ABI_FP;
                    reg.reg.index = fp++;
                }
                else {
                    reg.reg.bank = CI_ABI_GP;
                    reg.reg.index = gp++;
                }
                CiAbiAddressSpec part = (CiAbiAddressSpec){.mode = CI_ABI_SLOT, .offset = slot + piece.value_offset};
                CiAbiTransfer action = CI_ABI_COPY;
                if(write && (t.sign_extend || t.zero_extend) && piece.bank == NC_ABI_GP && piece.size < 8)
                    action = t.sign_extend? CI_ABI_SIGN_EXTEND : CI_ABI_ZERO_EXTEND;
                CiAbiAddressSpec destination, source;
                if(write){
                    destination = reg;
                    source = part;
                }
                else {
                    destination = part;
                    source = reg;
                }
                err = ci_abi_emit(out, al, loc, p->target, action, destination, source, piece.size);
                if(err) return err;
            }
            return 0;
        }
        case CC_TARGET_AARCH64_LINUX: {
            if(!a.on_stack && t.mode != NC_ABI_INDIRECT && t.count == 1 && t.size && t.size <= 16){
                NcAbiPiece piece = t.pieces[0];
                _Bool fp = piece.bank == NC_ABI_FP;
                if(!piece.value_offset && piece.size == t.size && (fp || piece.bank == NC_ABI_GP)){
                    CiAbiRegister reg;
                    if(fp)
                        reg = (CiAbiRegister){.bank = CI_ABI_FP, .index = a.fp};
                    else
                        reg = (CiAbiRegister){.bank = CI_ABI_GP, .index = a.gp};
                    reg = ci_abi_resolve_register(p->target, reg);
                    _Bool extend = write && !fp && t.size < 8 && (t.sign_extend || t.zero_extend);
                    CiAbiTransfer operation = CI_ABI_COPY;
                    if(extend && t.sign_extend)
                        operation = CI_ABI_SIGN_EXTEND;
                    else if(extend)
                        operation = CI_ABI_ZERO_EXTEND;
                    return ma_push(CiOp)(out, al, (CiOp){.abi_move = {
                        .kind = CI_OP_ABI_MOVE,
                        .write = write,
                        .incoming = incoming,
                        .operation = operation,
                        .source_size = t.size, .size = extend? 8 : t.size,
                        .slot = slot,
                        .reg = reg,
                        .loc = loc,
                    }});
                }
            }
            if(t.mode == NC_ABI_INDIRECT){
                if(write){
                    uint32_t alignment = p->stack_alignment > 16 ? (uint32_t)p->stack_alignment : 16;
                    uint32_t offset = (uint32_t)(((p->stack_size + alignment - 1) & ~(size_t)(alignment - 1)) + a.copy_offset);
                    CiAbiAddressSpec copy = {
                        .mode = CI_ABI_REG_PTR,
                        .base = {.bank = CI_ABI_STACK_BASE},
                        .offset = offset,
                    };
                    err = ci_abi_emit(out, al, loc, p->target, CI_ABI_COPY, copy, value, t.size);
                    if(err) return err;
                    CiAbiAddressSpec destination;
                    if(a.on_stack)
                        destination = (CiAbiAddressSpec){.mode = CI_ABI_REG_PTR, .base = {.bank = CI_ABI_STACK_BASE, .index = 0}, .offset = a.stack_offset};
                    else
                        destination = (CiAbiAddressSpec){.mode = CI_ABI_REG, .reg = {.bank = CI_ABI_GP, .index = a.gp}};
                    return ci_abi_emit(out, al, loc, p->target, CI_ABI_ADDRESS, destination, copy, 8);
                }
                CiAbiAddressSpec source;
                if(a.on_stack)
                    source = (CiAbiAddressSpec){
                        .mode = incoming? CI_ABI_INCOMING_PTR_INDIRECT : CI_ABI_REG_PTR_INDIRECT,
                        .base = {.bank = CI_ABI_STACK_BASE},
                        .offset = a.stack_offset,
                    };
                else
                    source = (CiAbiAddressSpec){
                        .mode = ptr,
                        .base = {.bank = CI_ABI_GP, .index = a.gp},
                        .offset = 0
                    };
                return ci_abi_emit(out, al, loc, p->target, CI_ABI_COPY, value, source, t.size);
            }
            if(a.on_stack){
                CiAbiAddressSpec memory = {
                    .mode = ptr,
                    .base = {.bank = CI_ABI_STACK_BASE},
                    .offset = a.stack_offset,
                };
                CiAbiAddressSpec destination, source;
                if(write){
                    destination = memory;
                    source = value;
                }
                else {
                    destination = value;
                    source = memory;
                }
                return ci_abi_emit(out, al, loc, p->target, CI_ABI_COPY, destination, source, t.size);
            }
            if(!write && ci_abi_has_padding(t)){
                err = ci_abi_emit(out, al, loc, p->target, CI_ABI_ZERO, value, (CiAbiAddressSpec){.immediate = 0}, t.size);
                if(err) return err;
            }
            unsigned gp = a.gp, fp = a.fp;
            for(unsigned i = 0; i < t.count; i++){
                NcAbiPiece piece = t.pieces[i];
                CiAbiAddressSpec reg;
                if(piece.bank == NC_ABI_FP)
                    reg = (CiAbiAddressSpec){.mode = regs, .reg = {.bank = CI_ABI_FP, .index = fp++}};
                else
                    reg = (CiAbiAddressSpec){.mode = regs, .reg = {.bank = CI_ABI_GP, .index = gp++}};
                CiAbiAddressSpec part = (CiAbiAddressSpec){.mode = CI_ABI_SLOT, .offset = slot + piece.value_offset};
                CiAbiTransfer action = CI_ABI_COPY;
                if(write && (t.sign_extend || t.zero_extend) && piece.bank == NC_ABI_GP && piece.size < 8)
                    action = t.sign_extend ? CI_ABI_SIGN_EXTEND : CI_ABI_ZERO_EXTEND;
                CiAbiAddressSpec destination, source;
                if(write){
                    destination = reg;
                    source = part;
                }
                else {
                    destination = part;
                    source = reg;
                }
                err = ci_abi_emit(out, al, loc, p->target, action, destination, source, piece.size);
                if(err) return err;
            }
            return 0;
        }
        case CC_TARGET_AARCH64_MACOS: {
            if(!a.on_stack && t.mode != NC_ABI_INDIRECT && t.count == 1 && t.size && t.size <= 16){
                NcAbiPiece piece = t.pieces[0];
                _Bool fp = piece.bank == NC_ABI_FP;
                if(!piece.value_offset && piece.size == t.size && (fp || piece.bank == NC_ABI_GP)){
                    CiAbiRegister reg;
                    if(fp)
                        reg = (CiAbiRegister){.bank = CI_ABI_FP, .index = a.fp};
                    else
                        reg = (CiAbiRegister){.bank = CI_ABI_GP, .index = a.gp};
                    reg = ci_abi_resolve_register(p->target, reg);
                    _Bool extend = write && !fp && t.size < 8 && (t.sign_extend || t.zero_extend);
                    CiAbiTransfer operation = CI_ABI_COPY;
                    if(extend && t.sign_extend)
                        operation = CI_ABI_SIGN_EXTEND;
                    else if(extend)
                        operation = CI_ABI_ZERO_EXTEND;
                    return ma_push(CiOp)(out, al, (CiOp){.abi_move = {
                        .kind = CI_OP_ABI_MOVE, .write = write, .incoming = incoming,
                        .operation = operation,
                        .source_size = t.size, .size = extend ? 8 : t.size, .slot = slot, .reg = reg, .loc = loc,
                    }});
                }
            }
            if(t.mode == NC_ABI_INDIRECT){
                if(write){
                    uint32_t alignment = p->stack_alignment > 16 ? (uint32_t)p->stack_alignment : 16;
                    uint32_t offset = (uint32_t)(((p->stack_size + alignment - 1) & ~(size_t)(alignment - 1)) + a.copy_offset);
                    CiAbiAddressSpec copy = {
                        .mode = CI_ABI_REG_PTR, .base = {.bank = CI_ABI_STACK_BASE}, .offset = offset,
                    };
                    err = ci_abi_emit(out, al, loc, p->target, CI_ABI_COPY, copy, value, t.size);
                    if(err) return err;
                    CiAbiAddressSpec destination;
                    if(a.on_stack)
                        destination = (CiAbiAddressSpec){.mode = CI_ABI_REG_PTR, .base = {.bank = CI_ABI_STACK_BASE, .index = 0}, .offset = a.stack_offset};
                    else
                        destination = (CiAbiAddressSpec){.mode = CI_ABI_REG, .reg = {.bank = CI_ABI_GP, .index = a.gp}};
                    return ci_abi_emit(out, al, loc, p->target, CI_ABI_ADDRESS, destination, copy, 8);
                }
                CiAbiAddressSpec source;
                if(a.on_stack)
                    source = (CiAbiAddressSpec){
                        .mode = incoming ? CI_ABI_INCOMING_PTR_INDIRECT : CI_ABI_REG_PTR_INDIRECT,
                        .base = {.bank = CI_ABI_STACK_BASE}, .offset = a.stack_offset,
                    };
                else
                    source = (CiAbiAddressSpec){.mode = ptr, .base = {.bank = CI_ABI_GP, .index = a.gp}, .offset = 0};
                return ci_abi_emit(out, al, loc, p->target, CI_ABI_COPY, value, source, t.size);
            }
            if(a.on_stack){
                CiAbiAddressSpec memory = {
                    .mode = ptr, .base = {.bank = CI_ABI_STACK_BASE}, .offset = a.stack_offset,
                };
                CiAbiAddressSpec destination, source;
                if(write){
                    destination = memory;
                    source = value;
                }
                else {
                    destination = value;
                    source = memory;
                }
                return ci_abi_emit(out, al, loc, p->target, CI_ABI_COPY, destination, source, t.size);
            }
            if(!write && ci_abi_has_padding(t)){
                err = ci_abi_emit(out, al, loc, p->target, CI_ABI_ZERO, value, (CiAbiAddressSpec){.immediate = 0}, t.size);
                if(err) return err;
            }
            unsigned gp = a.gp, fp = a.fp;
            for(unsigned i = 0; i < t.count; i++){
                NcAbiPiece piece = t.pieces[i];
                CiAbiAddressSpec reg;
                if(piece.bank == NC_ABI_FP)
                    reg = (CiAbiAddressSpec){.mode = regs, .reg = {.bank = CI_ABI_FP, .index = fp++}};
                else
                    reg = (CiAbiAddressSpec){.mode = regs, .reg = {.bank = CI_ABI_GP, .index = gp++}};
                CiAbiAddressSpec part = (CiAbiAddressSpec){.mode = CI_ABI_SLOT, .offset = slot + piece.value_offset};
                CiAbiTransfer action = CI_ABI_COPY;
                if(write && (t.sign_extend || t.zero_extend) && piece.bank == NC_ABI_GP && piece.size < 8)
                    action = t.sign_extend ? CI_ABI_SIGN_EXTEND : CI_ABI_ZERO_EXTEND;
                CiAbiAddressSpec destination, source;
                if(write){
                    destination = reg;
                    source = part;
                }
                else {
                    destination = part;
                    source = reg;
                }
                err = ci_abi_emit(out, al, loc, p->target, action, destination, source, piece.size);
                if(err) return err;
            }
            return 0;
        }
        case CC_TARGET_X86_64_WINDOWS_MSVC:
        case CC_TARGET_X86_64_WINDOWS_MINGW: {
            if(!a.on_stack && t.mode != NC_ABI_INDIRECT && t.count == 1 && t.size && t.size <= 16){
                NcAbiPiece piece = t.pieces[0];
                _Bool fp = piece.bank == NC_ABI_FP;
                if(!piece.value_offset && piece.size == t.size && (fp || piece.bank == NC_ABI_GP) && !(write && fp && p->is_variadic)){
                    CiAbiRegister reg = {.bank = fp ? CI_ABI_FP : CI_ABI_GP, .index = a.gp};
                    reg = ci_abi_resolve_register(p->target, reg);
                    return ma_push(CiOp)(out, al, (CiOp){.abi_move = {
                        .kind = CI_OP_ABI_MOVE,
                        .write = write,
                        .incoming = incoming,
                        .operation = CI_ABI_COPY,
                        .source_size = t.size,
                        .size = t.size,
                        .slot = slot,
                        .reg = reg,
                        .loc = loc,
                    }});
                }
            }
            if(t.mode == NC_ABI_INDIRECT){
                if(write){
                    uint32_t alignment = p->stack_alignment > 16 ? (uint32_t)p->stack_alignment : 16;
                    uint32_t offset = (uint32_t)(((p->stack_size + alignment - 1) & ~(size_t)(alignment - 1)) + a.copy_offset);
                    CiAbiAddressSpec copy = {
                        .mode = CI_ABI_REG_PTR,
                        .base = {.bank = CI_ABI_STACK_BASE},
                        .offset = offset,
                    };
                    err = ci_abi_emit(out, al, loc, p->target, CI_ABI_COPY, copy, value, t.size);
                    if(err) return err;
                    CiAbiAddressSpec destination;
                    if(a.on_stack)
                        destination = (CiAbiAddressSpec){.mode = CI_ABI_REG_PTR, .base = {.bank = CI_ABI_STACK_BASE, .index = 0}, .offset = a.stack_offset};
                    else
                        destination = (CiAbiAddressSpec){.mode = CI_ABI_REG, .reg = {.bank = CI_ABI_GP, .index = a.gp}};
                    return ci_abi_emit(out, al, loc, p->target, CI_ABI_ADDRESS, destination, copy, 8);
                }
                CiAbiAddressSpec source;
                if(a.on_stack)
                    source = (CiAbiAddressSpec){
                        .mode = incoming ? CI_ABI_INCOMING_PTR_INDIRECT : CI_ABI_REG_PTR_INDIRECT,
                        .base = {.bank = CI_ABI_STACK_BASE}, .offset = a.stack_offset,
                    };
                else
                    source = (CiAbiAddressSpec){.mode = ptr, .base = {.bank = CI_ABI_GP, .index = a.gp}, .offset = 0};
                return ci_abi_emit(out, al, loc, p->target, CI_ABI_COPY, value, source, t.size);
            }
            if(a.on_stack){
                CiAbiAddressSpec memory = {
                    .mode = ptr, .base = {.bank = CI_ABI_STACK_BASE}, .offset = a.stack_offset,
                };
                CiAbiAddressSpec destination, source;
                if(write){
                    destination = memory;
                    source = value;
                }
                else {
                    destination = value;
                    source = memory;
                }
                return ci_abi_emit(out, al, loc, p->target, CI_ABI_COPY, destination, source, t.size);
            }

            CiAbiAddressSpec gp = (CiAbiAddressSpec){.mode = regs, .reg = {.bank = CI_ABI_GP, .index = a.gp}};
            CiAbiAddressSpec fp = (CiAbiAddressSpec){.mode = regs, .reg = {.bank = CI_ABI_FP, .index = a.gp}};
            _Bool floating = t.count && t.pieces[0].bank == NC_ABI_FP;
            if(!write)
                return ci_abi_emit(out, al, loc, p->target, CI_ABI_COPY, value, floating ? fp : gp, t.size);
            if(floating){
                err = ci_abi_emit(out, al, loc, p->target, CI_ABI_COPY, fp, value, t.size);
                if(err) return err;
            }
            if(!floating || p->is_variadic)
                return ci_abi_emit(out, al, loc, p->target, CI_ABI_COPY, gp, value, t.size);
            return 0;
        }
        case CC_TARGET_TEST: {
            CiAbiAddressSpec memory = {
                .mode = ptr,
                .base = {.bank = CI_ABI_STACK_BASE},
                .offset = a.stack_offset,
            };
            CiAbiAddressSpec destination, source;
            if(write){
                destination = memory;
                source = value;
            }
            else {
                destination = value;
                source = memory;
            }
            return ci_abi_emit(out, al, loc, p->target, CI_ABI_COPY, destination, source, t.size);
        }
        case CC_TARGET_COUNT:
            return CI_UNREACHABLE_ERROR;
        DRP_CASES_EXHAUSTED;
    }
}

static
int
ci_lower_abi_result(Marray(CiOp)* out, Allocator al, const NcCallPlan* p, uint32_t slot, _Bool write, SrcLoc loc){
    NcAbiType t = p->result;
    if(t.mode == NC_ABI_IGNORED || t.mode == NC_ABI_INDIRECT)
        return 0;
    int err;
    if(!write && ci_abi_has_padding(t)){
        CiAbiAddressSpec dst = {.mode = CI_ABI_SLOT, .offset = slot},
                         src = {.immediate = 0};
        err = ci_abi_emit(out, al, loc, p->target, CI_ABI_ZERO, dst, src, t.size);
        if(err) return err;
    }
    unsigned gp = 0, fp = 0, x87 = 0;
    for(unsigned i = 0; i < t.count; i++){
        NcAbiPiece piece = t.pieces[i];
        CiAbiAddressSpec reg;
        switch(piece.bank){
            case NC_ABI_FP:
                reg = (CiAbiAddressSpec){.mode = CI_ABI_REG, .reg = {.bank = CI_ABI_RESULT_FP, .index = fp++}};
                break;
            case NC_ABI_X87:
                reg = (CiAbiAddressSpec){.mode = CI_ABI_REG, .reg = {.bank = CI_ABI_RESULT_X87, .index = x87++}};
                break;
            case NC_ABI_GP:
                reg = (CiAbiAddressSpec){.mode = CI_ABI_REG, .reg = {.bank = CI_ABI_RESULT_GP, .index = gp++}};
                break;
            DRP_CASES_EXHAUSTED;
        }
        CiAbiAddressSpec value = {.mode = CI_ABI_SLOT, .offset = slot + piece.value_offset};
        CiAbiTransfer action = CI_ABI_COPY;
        if(write && (t.sign_extend || t.zero_extend) && piece.bank == NC_ABI_GP && piece.size < 8)
            action = t.sign_extend ? CI_ABI_SIGN_EXTEND : CI_ABI_ZERO_EXTEND;
        CiAbiAddressSpec destination, source;
        if(write){
            destination = reg;
            source = value;
        }
        else {
            destination = value;
            source = reg;
        }
        err = ci_abi_emit(out, al, loc, p->target, action, destination, source, piece.size);
        if(err) return err;
    }
    return 0;
}

typedef struct CiLowerSwitch CiLowerSwitch;
struct CiLowerSwitch {
    Marray(CcSwitchEntry) entries;
    uint32_t default_target;
    _Bool has_default;
};

typedef struct CiLowerCtx CiLowerCtx;
typedef struct CiFoldCache CiFoldCache;
typedef struct CiInlineState CiInlineState;
typedef struct CiInlineBinding CiInlineBinding;
struct CiInlineBinding {
    CcVariable* var;
    uint32_t slot;
    _Bool constant;
    _Bool parameter;
    CiFoldValue value;
};

struct CiInlineState {
    const CiLowerCtx *parent;
    uint32_t result,
             returns;
    CcStmtNode *_Nullable tail_return;
    _Bool borrow_result;
    uint32_t value_slot;
    _Bool result_seen,
          result_constant;
    CiFoldValue result_value;
};
struct CiLowerCtx {
    Allocator a;
    Marray(CiOp)* out;
    AtomMap(uintptr_t)* labels; // label -> op index + 1
    CiLowerSwitch* _Nullable sw; // innermost switch being lowered
    uint32_t temp; // top of the local + temp slot stack; statements save/restore
                   // this around their children so siblings recycle slots
    uint64_t materialized_addresses; // generation: new temporary objects may escape through pointers
    uint32_t materialized_end; // live temporary objects cannot be recycled as expression scratch
    uint32_t frame_alignment; // largest alignment required by automatic storage
    uint32_t* frame_size; // high-water mark of slot usage: &func->frame_size,
                          // or the toplevel/module slot size
    Marray(CiBackpatchTarget) backpatches;
    uint32_t size_size, ptr_size; // cached common sizes
    _Bool char_is_unsigned;
    _Bool static_initializer;
    _Bool has_alloca, has_vla;
    CcLongDoubleFormat ldbl_fmt;
    CiLowerDeps deps;
    CiFoldCache* _Nullable fold_cache;
    const NcCallPlan *_Nullable function_abi;
    uint32_t result_pointer_slot; // incoming indirect-result pointer, live for the function
    CcFunc *_Nullable function;
    CiInlineState *_Nullable inline_state;
    PointerMap(CcVariable, CiInlineBinding) bindings;
    _Bool statement_fallthrough;
};

static
CiInlineBinding* _Nullable
ci_inline_binding(CiLowerCtx* ctx, CcVariable* var){
    return PM_get(&ctx->bindings, var);
}

static
void
ci_bindings_cleanup(CiLowerCtx* ctx){
    PointerMapItems items = PM_items(&ctx->bindings);
    for(size_t i = 0; i < items.count; i++){
        CiInlineBinding* binding = items.data[i].value;
        Allocator_free(ctx->a, binding, 1, sizeof *binding, _Alignof(CiInlineBinding));
    }
    if(ctx->bindings.data)
        Allocator_free(ctx->a, ctx->bindings.data, PM_alloc_size(ctx->bindings.cap), 1, _Alignof(PointerMapItem));
    ctx->bindings = (PointerMap){0};
}

static
uint32_t
ci_variable_slot(CiLowerCtx* ctx, CcVariable* var){
    if(!ctx->inline_state) return (uint32_t)var->frame_offset;
    CiInlineBinding* b=ci_inline_binding(ctx,var);
    return b?b->slot:(uint32_t)var->frame_offset;
}

static
int
ci_bind_slot(CiLowerCtx* ctx, CcVariable* var, uint32_t slot){
    if(!ctx->inline_state) var->frame_offset=slot;
    if(!ctx->function) return 0;
    CiInlineBinding* b=ci_inline_binding(ctx,var);
    if(!b){
        b = Allocator_zalloc(ctx->a, 1, sizeof *b, _Alignof(CiInlineBinding));
        if(!b) return CI_OOM_ERROR;
        b->var = var;
        if(PM_put(&ctx->bindings, ctx->a, var, b)){
            Allocator_free(ctx->a, b, 1, sizeof *b, _Alignof(CiInlineBinding));
            return CI_OOM_ERROR;
        }
    }
    b->slot=slot;
    return 0;
}

static inline
_Bool
ci_arg_is_ignored(CcTarget target, CcQualType type){
    switch(target){
        case CC_TARGET_AARCH64_MACOS: {
            CcTypeKind kind = ccqt_kind(type);
            return (kind == CC_STRUCT || kind == CC_UNION) && ccqt_as_struct(type)->arm64.is_empty;
        }
        case CC_TARGET_X86_64_LINUX:
        case CC_TARGET_X86_64_MACOS:
            return 0;
        case CC_TARGET_X86_64_WINDOWS_MSVC:
        case CC_TARGET_X86_64_WINDOWS_MINGW:
            return 0;
        case CC_TARGET_AARCH64_LINUX:
            return 0;
        case CC_TARGET_TEST:
        case CC_TARGET_COUNT:
            return 0;
        DRP_CASES_EXHAUSTED;
    }
}

static
_Bool
ci_bitfield_needs_bytes(CcQualType type, CcFieldPath path, uint32_t size){
    for(uint32_t i = 0, count = cc_field_path_count(path); i < count; i++){
        uint32_t index = cc_field_path_component(path, i);
        if(ccqt_is_fixed_sequence(type)){
            type = ccqt_array_element(type);
            continue;
        }
        CcStruct* record = ccqt_as_struct(type);
        CcField* field = &record->fields[index];
        if(i+1 == count){
            return (uint32_t)field->bitoffset + field->bitwidth > size*8u
                || (uint64_t)field->offset + size > record->size;
        }
        type = field->type;
    }
    return 1;
}

static
_Bool
ci_expr_bitfield_needs_bytes(const CcExpr* lv, uint32_t size){
    while(lv->kind == CC_EXPR_COMMA) lv = lv->values[0];
    return ci_bitfield_needs_bytes(cc_expr_field_owner(lv), lv->field_path, size);
}

static
int
ci_init_entry_loc(CiInterpreter* ci, CcQualType type, CcInitEntry* entry, CiFieldLoc* out){
    int err = cc_field_path_offset(ci_target(ci), type, entry->path, &out->byte_offset);
    if(err) return err;
    out->bit_offset = cc_field_path_bit_offset(type, entry->path);
    out->bit_width = cc_field_path_bit_width(type, entry->path);
    return 0;
}

static
int
ci_lower_func_dep(CiLowerCtx* ctx, CcFunc* func, CcFuncDepFlags flags){
    return ci_deps_add_func(&ctx->deps, ctx->a, func, flags);
}

typedef struct CiLowerVal CiLowerVal;
struct CiLowerVal {
    uint32_t slot;
    uint32_t size;
    _Bool canonical; // already 0 or 1
    _Bool borrowed; // storage predates this expression; preserve before later side effects
    _Bool constant;
    CiFoldValue value;
};
#define CI_NO_SLOT UINT32_MAX

static const CcTargetConfig* ci_target(const CiInterpreter*);
static int ci_lower_stmt(CiInterpreter* ci, CiLowerCtx* ctx, CcStmtNode*_Nullable n);
static int ci_try_preinit_global(CiInterpreter*, CiLowerCtx*, CcExpr*, _Bool*);
static int ci_lower_stmt_inner(CiInterpreter* ci, CiLowerCtx* ctx, CcStmtNode* n);
static
int
ci_lower_inline_return(CiLowerCtx* ctx, SrcLoc loc){
    int err = ma_push(CiOp)(
        ctx->out,
        ctx->a,
        (CiOp){.jump={
            .kind=CI_OP_JUMP,
            .jump=ctx->inline_state->returns,
            .loc=loc,
        }}
    );
    if(err) return err;
    ctx->inline_state->returns = (uint32_t)ctx->out->count;
    return 0;
}

static
_Bool
ci_stable_variable(CiLowerCtx* ctx, CcExpr* e){
    return ctx->function
        && e->kind == CC_EXPR_VARIABLE
        && e->var->automatic
        && !e->type.is_volatile
        && !e->type.is_atomic
        && !e->var->mutable_storage;
}
static int ci_lower_expr(CiInterpreter* ci, CiLowerCtx* ctx, CcExpr* e, uint32_t dest, CiLowerVal* out);
static int ci_lower_expr_inner(CiInterpreter* ci, CiLowerCtx* ctx, CcExpr* e, uint32_t dest, CiLowerVal* out);
static int ci_preserve_value(CiInterpreter*, CiLowerCtx*, CcExpr*, CiLowerVal*);
static int ci_expr_discardable(CiLowerCtx*, CcExpr*, _Bool*);
static int ci_lower_cast_operand(CiInterpreter* ci, CiLowerCtx* ctx, CcExpr* e, uint32_t dest, CiLowerVal* out);
static int ci_lower_expr_discard(CiInterpreter* ci, CiLowerCtx* ctx, CcExpr* e);
static int ci_lower_cond(CiInterpreter* ci, CiLowerCtx* ctx, CcExpr* cond, CiLowerVal* out);
static int ci_lower_branch(CiInterpreter* ci, CiLowerCtx* ctx, CcExpr* cond, _Bool when_true, SrcLoc loc, uint32_t* chain);
static void ci_patch_branches(CiLowerCtx* ctx, uint32_t chain, uint32_t target);
static int ci_lower_istrue(CiLowerCtx* ctx, const CiLowerVal* v, CcQualType src_type, uint32_t dest, uint32_t dest_size, _Bool negate, SrcLoc loc);
static int ci_lower_dest(CiLowerCtx* ctx, uint32_t* dest, uint32_t size, uint32_t align);
static int ci_lower_expr_dest(CiInterpreter* ci, CiLowerCtx* ctx, const CcExpr* e, uint32_t* dest, uint32_t size);
typedef struct CiLowerAddr CiLowerAddr;
struct CiLowerAddr {
    uint32_t slot;
    uint32_t disp;
    uint32_t align_log2; // alignment of slot's pointer + disp
    _Bool is_volatile; // destination access qualification for aggregate initialization
};
static uint32_t cc_offset_alignment(uint32_t align_log2, uint64_t offset);
static uint32_t ci_frame_alignment(const CiLowerCtx*, uint32_t);
static int ci_lvalue_alignment(CiInterpreter*, CiLowerCtx*, const CcExpr*, uint32_t*);
static int ci_pointer_alignment(CiInterpreter*, CiLowerCtx*, const CcExpr*, uint32_t*);
static _Bool ci_frame_lvalue(CiInterpreter* ci, CiLowerCtx* ctx, const CcExpr* lv, uint32_t* offset);
static int ci_lower_addr(CiInterpreter* ci, CiLowerCtx* ctx, CcExpr* lv, _Bool one_past_ok, CiLowerAddr* out);
static int ci_lower_addr_inner(CiInterpreter*, CiLowerCtx*, CcExpr*, _Bool, CiLowerAddr*);
static int ci_lower_pointer_addr(CiInterpreter*, CiLowerCtx*, CcExpr*, uint32_t, _Bool, CiLowerAddr*);
static int ci_pointer_offset(CiInterpreter*, CiLowerCtx*, CcExpr*, uint64_t, uint64_t, CcExpr*_Nonnull*_Nonnull, uint64_t*);
static int ci_lower_materialize_addr(CiInterpreter* ci, CiLowerCtx* ctx, CcExpr* e, CiLowerAddr* out);
static int ci_addr_to_value(CiLowerCtx* ctx, CiLowerAddr a, uint32_t dest, uint32_t size, uint32_t align, SrcLoc loc, CiLowerVal* out);
static int ci_lower_bitfield_addr(CiInterpreter* ci, CiLowerCtx* ctx, CcExpr* lv, CiLowerAddr* out);
static int ci_emit_load_bitfield(CiInterpreter* ci, CiLowerCtx* ctx, const CcExpr* lv, CiLowerAddr a, uint32_t dest, uint32_t size);
static int ci_emit_store_bitfield(CiLowerCtx* ctx, const CcExpr* lv, CiLowerAddr a, uint32_t src, uint32_t size);
static int ci_lower_whole_byte_bitfield(CiLowerCtx*, CiOp*);
static
uint32_t
ci_float_width(CcQualType t, CcLongDoubleFormat fmt){
    if(!ccqt_is_basic(t)) return 0;
    switch(ccqt_basic_kind(t)){
        case CCBT_fp16:
        case CCBT_float16: case CCBT_bfloat16: return 16;
        case CCBT_float: return 32;
        case CCBT_double: return 64;
        case CCBT_float128: return 128;
        case CCBT_long_double: return fmt == CC_LONG_DOUBLE_BINARY64 ? 64 : fmt == CC_LONG_DOUBLE_X87 ? 80 : 128;
        default: return 0;
    }
}
static
CiOpKind
ci_falu_kind(CcQualType t, CcLongDoubleFormat fmt){
    switch(ci_float_width(t, fmt)){
        case 16: return CI_OP_FALU16;
        case 32: return CI_OP_FALU32;
        case 64: return CI_OP_FALU64;
        case 80: return CI_OP_FALU80;
        default: return CI_OP_FALU128;
    }
}
static _Bool ci_falu_type(CcQualType t);
static CiAluOp ci_alu_op_for(CcExprKind kind);
static CiCmpOp ci_cmp_op_for(CcExprKind kind);
static CiOpKind ci_int_op_kind(uint32_t size);
static CiOpKind ci_cmp_op_kind(uint32_t size);
static int ci_emit_const(CiInterpreter*, CiLowerCtx*, uint64_t, uint32_t, SrcLoc, uint32_t*);
static int ci_emit_alu(CiLowerCtx*, CiAluOp, uint32_t, uint32_t, uint32_t, uint32_t, SrcLoc);
static _Bool ci_falu_op_for(CcExprKind kind, CiFaluOp* out);
static int ci_lower_assign_direct(CiInterpreter* ci, CiLowerCtx* ctx, CcExpr* e, _Bool* handled);
static int ci_lower_init_list(CiInterpreter* ci, CiLowerCtx* ctx, CcExpr* e, CiLowerAddr dst, _Bool zero);
static int ci_try_lower_init_template(CiInterpreter*, CiLowerCtx*, CcExpr*, CiLowerAddr, uint32_t, _Bool*);
static int ci_lower_incdec(CiInterpreter* ci, CiLowerCtx* ctx, CcExpr* e, uint32_t dest, CiLowerVal*_Nullable out);
static int ci_lower_checked(CiInterpreter* ci, CiLowerCtx* ctx, CcExpr* e, uint32_t dest, CiLowerVal*_Nullable out);
static int ci_lower_umul128(CiInterpreter* ci, CiLowerCtx* ctx, CcExpr* e, uint32_t dest, CiLowerVal*_Nullable out);
static int ci_lower_bit_builtin(CiInterpreter* ci, CiLowerCtx* ctx, CcExpr* e, uint32_t dest, CiLowerVal* out);
static int ci_lower_call(CiInterpreter* ci, CiLowerCtx* ctx, CcExpr* e, uint32_t dest, CiLowerVal*_Nullable out);
static _Bool ci_armw_op_for(CcExprKind kind, CiAtomicRmwOp* out);
static int ci_lower_atomic_load_lv(CiInterpreter* ci, CiLowerCtx* ctx, CcExpr* e, uint32_t dest, CiLowerVal* out, uint32_t size);
static int ci_lower_atomic_operand(CiInterpreter*, CiLowerCtx*, const CcExpr*, CiLowerVal*, uint32_t, SrcLoc);
static int ci_emit_copy(CiLowerCtx*, uint32_t dst, uint32_t src, uint32_t size, SrcLoc);
static int ci_emit_copy_access(CiLowerCtx*, uint32_t dst, uint32_t src, uint32_t size, SrcLoc, _Bool observable);
static int ci_lower_atomic_compound_assign(CiInterpreter* ci, CiLowerCtx* ctx, CcExpr* e, uint32_t dest, CiLowerVal* out, uint32_t size, _Bool is_ptr, _Bool op_unsigned, _Bool is_float, CiFaluOp fop);
static int ci_lower_atomic_builtin(CiInterpreter* ci, CiLowerCtx* ctx, CcExpr* e, uint32_t dest, CiLowerVal*_Nullable out);
static int ci_lower_va(CiInterpreter* ci, CiLowerCtx* ctx, CcExpr* e, uint32_t dest, CiLowerVal*_Nullable out);
static int ci_lower_slice(CiInterpreter* ci, CiLowerCtx* ctx, CcExpr* e, uint32_t dest, CiLowerVal* out);
static int ci_lower_rt_call(CiInterpreter* ci, CiLowerCtx* ctx, CcExpr* e, CiRuntimeOp rt_op, uint32_t dest, CiLowerVal*_Nullable out);
static int ci_lower_clear_cache(CiInterpreter* ci, CiLowerCtx* ctx, CcExpr* e);
static int ci_lower_reflect(CiInterpreter* ci, CiLowerCtx* ctx, CcExpr* e, uint32_t dest, CiLowerVal*_Nullable out);
static int ci_alloc_slot(CiLowerCtx*, uint32_t sz, uint32_t align, uint32_t* slot);
static void ci_backpatch_break_continue(CiLowerCtx*, size_t start, uint32_t break_target, uint32_t continue_target);
static int ci_lower_resolve_gotos(CiInterpreter*, CiLowerCtx*);
static int ci_cmp_switch_entry(void*_Null_unspecified ctx, const void* a, const void* b);

enum CiDiscardState TYPED_ENUM(uintptr_t){
    CI_DISCARD_UNCHECKED,
    CI_DISCARD_REQUIRED,
    CI_DISCARD_SAFE,
};
TYPEDEF_ENUM(CiDiscardState, uintptr_t);

struct CiFoldCache {
    // 1 means not foldable (also used while evaluating); otherwise value index + 2.
    PointerMap(CcExpr, uintptr_t) expressions;
    PointerMap(CcExpr, CiDiscardState) discardable;
    Marray(CiFoldValue) values;
};

static
void
ci_fold_cache_cleanup(CiFoldCache* cache, Allocator al){
    if(cache->expressions.cap)
        Allocator_free(al, cache->expressions.data, PM_alloc_size(cache->expressions.cap), 1, _Alignof(PointerMapItem));
    if(cache->discardable.cap)
        Allocator_free(al, cache->discardable.data, PM_alloc_size(cache->discardable.cap), 1, _Alignof(PointerMapItem));
    ma_cleanup(CiFoldValue)(&cache->values, al);
    *cache = (CiFoldCache){0};
}

enum {FOLD_FAIL=-1};
// return 0 on success, positive error code on real error, FOLD_FAIL on fold fail
static int ci_fold_expr(CiInterpreter* ci, CiLowerCtx* ctx, CcExpr* e, CiFoldValue* folded);
static int ci_fold_expr_uncached(CiInterpreter*, CiLowerCtx*, CcExpr*, CiFoldValue*);
static uint64_t ci_fold_offset(CiLowerCtx* ctx, const CiFoldValue* index, uint32_t elem_sz);
static int ci_fold_condition(CiInterpreter* ci, CiLowerCtx* ctx, CcExpr* e, _Bool* truth);
static int ci_fold_read_inputs(CiInterpreter*, CiLowerCtx*, CcExpr*, unsigned);
static int ci_compare_type_limit(CiInterpreter*, CiLowerCtx*, CcExpr*, CcExpr*_Nonnull*_Nonnull, _Bool*);
enum {CI_ASSUMED_VARARG_ALIGN=8};
static size_t cc_expr_nvalues(CcExpr*);


static
int
ci_layout_decls(CiInterpreter* ci, CiLowerCtx* ctx, CcStmtNode* n){
    for(uint32_t i = 0; i < n->decls.count; i++){
        CcVariable* var = n->decls.data[i];
        CiInlineBinding* binding=ci_inline_binding(ctx,var);
        if(binding && binding->parameter) continue;
        if(var->constexpr_ && !var->mutable_storage) continue;
        uint32_t size, align, slot;
        if(ccqt_kind(var->type) == CC_VLA){
            int err = ci_alloc_slot(ctx, ctx->ptr_size, ctx->ptr_size, &slot);
            if(err) return err;
            err = ci_bind_slot(ctx,var,slot);
            if(err) return err;
            continue;
        }
        int err = cc_sizeof_as_uint(&ci->parser, var->type, var->loc, &size);
        if(err) return err;
        err = cc_alignof_as_uint(&ci->parser, var->type, var->loc, &align);
        if(err) return err;
        if(var->alignment > align) align = var->alignment;
        err = ci_alloc_slot(ctx, size, align, &slot);
        if(err) return err;
        err = ci_bind_slot(ctx,var,slot);
        if(err) return err;
    }
    return 0;
}

static
int
ci_lower_global_storage_addr(CiLowerCtx* ctx, CcVariable* var, uint32_t* slot){
    int err = ci_deps_add_var(&ctx->deps, ctx->a, var, CI_VAR_DEP_USED);
    if(err) return err;
    err = ci_alloc_slot(ctx, ctx->ptr_size, ctx->ptr_size, slot);
    if(err) return err;
    return ma_push(CiOp)(ctx->out, ctx->a, (CiOp){.var_addr={
        .kind=CI_OP_VAR_ADDR,
        .slot=*slot,
        .slot_size=ctx->ptr_size,
        .var=var,
        .loc=var->loc,
    }});
}

static
int
ci_lower_cleanups(CiInterpreter* ci, CiLowerCtx* ctx, CcCleanupRange range){
    for(CcDefer* d = range.first; d != range.stop; d = d->prev){
        if(d->vm_scope){
            if(d->stack_mark){
                uint32_t temp = ctx->temp;
                uint32_t mark = ci_variable_slot(ctx,(CcVariable*_Nonnull)d->stack_mark);
                int err;
                if(!d->stack_mark->automatic){
                    uint32_t addr;
                    err = ci_lower_global_storage_addr(ctx, (CcVariable*)d->stack_mark, &addr);
                    if(err) return err;
                    err = ci_alloc_slot(ctx, sizeof(CiStackMark), _Alignof(CiStackMark), &mark);
                    if(err) return err;
                    err = ma_push(CiOp)(ctx->out, ctx->a, (CiOp){.load={
                        .kind=CI_OP_LOAD,
                        .slot=mark,
                        .slot_size=sizeof(CiStackMark),
                        .src=addr,
                        .loc=d->stack_mark->loc,
                    }});
                    if(err) return err;
                }
                err = ma_push(CiOp)(ctx->out, ctx->a, (CiOp){.stack_restore={
                    .kind=CI_OP_STACK_RESTORE,
                    .slot=mark,
                    .loc=d->stack_mark->loc,
                }});
                ctx->temp = temp;
                if(err) return err;
            }
            continue;
        }
        AtomMap(uintptr_t) labels = {0};
        AtomMap(uintptr_t)* saved_labels = ctx->labels;
        Marray(CiBackpatchTarget) saved_patches = ctx->backpatches;
        CiLowerSwitch* saved_switch = ctx->sw;
        ctx->labels = &labels;
        ctx->backpatches = (Marray(CiBackpatchTarget)){0};
        ctx->sw = NULL;
        int err = ci_lower_stmt(ci, ctx, d->body);
        if(err) goto cleanup;
        err = ci_lower_resolve_gotos(ci, ctx);
        cleanup:
        ma_cleanup(CiBackpatchTarget)(&ctx->backpatches, ctx->a);
        if(labels.data) Allocator_free(ctx->a, labels.data, AM_alloc_size(labels.cap), 1, _Alignof(AtomMapItem));
        ctx->labels = saved_labels;
        ctx->backpatches = saved_patches;
        ctx->sw = saved_switch;
        if(err) return err;
    }
    return 0;
}

static
int
ci_lower_stmt(CiInterpreter* ci, CiLowerCtx* ctx, CcStmtNode*_Nullable n){
    if(!n){
        ctx->statement_fallthrough = 1;
        return 0;
    }
    uint32_t temp = ctx->temp;
    uint32_t materialized_end = ctx->materialized_end;
    int err = ci_layout_decls(ci, ctx, (CcStmtNode*_Nonnull)n);
    if(err) goto cleanup;
    err = ci_lower_stmt_inner(ci, ctx, (CcStmtNode*_Nonnull)n);
    if(err) goto cleanup;
    if(!n->no_fallthrough && (n->kind == CC_STMT_COMPOUND || n->kind == CC_STMT_NULL))
        err = ci_lower_cleanups(ci, ctx, n->cleanup);

    cleanup:
    if(n->kind != CC_STMT_COMPOUND){
        _Bool folded_branch = 0;
        if(n->kind == CC_STMT_IF){
            _Bool truth;
            if(!ci_fold_condition(ci, ctx, n->exprs[0], &truth)){
                CcStmtNode* dead = n->stmts[truth];
                folded_branch = !dead || !dead->has_jump_target;
            }
        }
        if(!folded_branch)
            ctx->statement_fallthrough = !n->no_fallthrough;
    }
    ctx->temp = temp;
    ctx->materialized_end = materialized_end;
    return err;
}

static
int
ci_lower_stmt_inner(CiInterpreter* ci, CiLowerCtx* ctx, CcStmtNode* n){
    int err;
    switch(n->kind){
        case CC_STMT_VLA: {
            if(ctx->has_alloca) return ci_error(ci, n->loc, "cannot mix explicit alloca and VLA allocations in one function");
            ctx->has_vla = 1;
            CcExpr* size;
            err = cc_sizeof_as_expr(&ci->parser, n->vla.object->type, n->loc, &size);
            if(err) return err;
            CiLowerVal value;
            err = ci_lower_expr(ci, ctx, size, CI_NO_SLOT, &value);
            cc_release_expr(&ci->parser, size);
            if(err) return err;
            uint32_t alignment;
            err = cc_alignof_as_uint(&ci->parser, n->vla.object->type, n->loc, &alignment);
            if(err) return err;
            if(n->vla.object->alignment > alignment)
                alignment = n->vla.object->alignment;
            uint32_t slot = ci_variable_slot(ctx, n->vla.object);
            uint32_t mark = ci_variable_slot(ctx, n->vla.mark),
                     addr = 0,
                     mark_addr = 0;
            if(!n->vla.object->automatic){
                err = ci_lower_global_storage_addr(ctx, n->vla.object, &addr);
                if(err) return err;
                err = ci_lower_global_storage_addr(ctx, n->vla.mark, &mark_addr);
                if(err) return err;
                err = ci_alloc_slot(ctx, ctx->ptr_size, ctx->ptr_size, &slot);
                if(err) return err;
                err = ci_alloc_slot(ctx, sizeof(CiStackMark), _Alignof(CiStackMark), &mark);
                if(err) return err;
            }
            err = ma_push(CiOp)(ctx->out, ctx->a, (CiOp){.stack_save={
                .kind=CI_OP_STACK_SAVE,
                .slot=mark,
                .loc=n->loc,
            }});
            if(err) return err;
            err = ma_push(CiOp)(ctx->out, ctx->a, (CiOp){.alloca={
                .kind=CI_OP_ALLOCA,
                .zero_initialize=n->zero_initialize,
                .slot=slot,
                .src=value.slot,
                .alignment=alignment,
                .loc=n->loc,
            }});
            if(err) return err;
            if(!n->vla.object->automatic){
                err = ma_push(CiOp)(ctx->out, ctx->a, (CiOp){.store={
                    .kind=CI_OP_STORE,
                    .slot=addr,
                    .src=slot,
                    .src_size=ctx->ptr_size,
                    .loc=n->loc,
                }});
                if(err) return err;
                err = ma_push(CiOp)(ctx->out, ctx->a, (CiOp){.store={
                    .kind=CI_OP_STORE,
                    .slot=mark_addr,
                    .src=mark,
                    .src_size=sizeof(CiStackMark),
                    .loc=n->loc,
                }});
                if(err) return err;
            }
            return 0;
        }
        case CC_STMT_NULL:
        case CC_STMT_DEFER:
            return 0;
        case CC_STMT_EXPR:
            {
                _Bool handled;
                err = ci_try_preinit_global(ci, ctx, n->exprs[0], &handled);
                if(err || handled) return err;
            }
            if(n->exprs[0]->kind == CC_EXPR_ASSIGN && n->exprs[0]->lhs->kind == CC_EXPR_VARIABLE){
                CcVariable* var = n->exprs[0]->lhs->var;
                if(var->automatic
                && var->constexpr_
                && !var->mutable_storage
                && n->exprs[0]->values[0] == var->initializer)
                    return 0;
            }
            return ci_lower_expr_discard(ci, ctx, n->exprs[0]);
        case CC_STMT_COMPOUND:{
            _Bool reachable = 1;
            for(uint32_t i = 0; i < n->count; i++){
                if(!reachable && !n->stmts[i]->has_jump_target) continue;
                err = ci_lower_stmt(ci, ctx, n->stmts[i]);
                if(err) return err;
                reachable = ctx->statement_fallthrough;
            }
            ctx->statement_fallthrough = reachable;
            return 0;
        }
        case CC_STMT_IF:{
            CcExpr* cond = n->exprs[0];
            _Bool truth;
            int folded = ci_fold_condition(ci, ctx, cond, &truth);
            if(folded > 0) return folded;
            if(!folded){
                CcStmtNode* dead = n->stmts[truth ? 1 : 0];
                if(!dead || !dead->has_jump_target)
                    return ci_lower_stmt(ci, ctx, n->stmts[truth ? 0 : 1]);
            }
            CiOp* op;
            uint32_t chain = 0;
            err = ci_lower_branch(ci, ctx, cond, 0, n->loc, &chain);
            if(err) return err;
            err = ci_lower_stmt(ci, ctx, n->stmts[0]);
            if(err) return err;
            if(!n->stmts[1]){
                ci_patch_branches(ctx, chain, (uint32_t)ctx->out->count);
            }
            else if(n->stmts[0] && n->stmts[0]->no_fallthrough){
                ci_patch_branches(ctx, chain, (uint32_t)ctx->out->count);
                return ci_lower_stmt(ci, ctx, n->stmts[1]);
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
            if(!n->has_jump_target){
                _Bool truth;
                int folded = ci_fold_condition(ci, ctx, cond, &truth);
                if(folded > 0) return folded;
                if(!folded && !truth) return 0;
            }
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
            if(cond && !n->has_jump_target){
                _Bool truth;
                int folded = ci_fold_condition(ci, ctx, cond, &truth);
                if(folded > 0) return folded;
                if(!folded && !truth) return 0; // initializer was still evaluated
            }
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
                void* scratch = Allocator_alloc(ctx->a, sw.entries.count, sizeof *sw.entries.data, _Alignof(CcSwitchEntry));
                if(!scratch){ err = CI_OOM_ERROR; goto switch_cleanup; }
                drp_merge_sort(scratch, sw.entries.data, sw.entries.count, sizeof(CcSwitchEntry), &is_unsigned, ci_cmp_switch_entry);
                Allocator_free(ctx->a, scratch, sw.entries.count, sizeof *sw.entries.data, _Alignof(CcSwitchEntry));
            }
            // Check for overlaps in the controlling type's order.
            uint64_t sign = is_unsigned ? 0 : UINT64_C(1) << 63;
            for(size_t i = 1; i < sw.entries.count; i++){
                if((sw.entries.data[i].low ^ sign) <= (sw.entries.data[i-1].high ^ sign)){
                    err = ci_error(ci, n->loc, "overlapping case values%s", "");
                    goto switch_cleanup;
                }
            }
            size_t count = sw.entries.count;
            _Bool is_dense = count && sw.entries.data[count-1].low - sw.entries.data[0].low == count-1;
            for(size_t i = 0; is_dense && i < count; i++){
                if(sw.entries.data[i].low != sw.entries.data[i].high) is_dense = 0;
            }
            // Copy the sorted entries into the op's out-of-line table
            {
                CiSwitchTable* table = Allocator_alloc_extra(ctx->a, sw.entries.count, sizeof *table->data, sizeof *table, _Alignof(CiSwitchTable));
                if(!table){ err = CI_OOM_ERROR; goto switch_cleanup; }
                table->count = sw.entries.count;
                if(sw.entries.count)
                    memcpy(table->data, sw.entries.data, sw.entries.count * sizeof(CcSwitchEntry));
                uint32_t break_target = (uint32_t)ctx->out->count;
                CiOp* swop = &ctx->out->data[sw_idx];
                swop->switch_.jump = sw.has_default ? sw.default_target : break_target;
                swop->switch_.table = table;
                swop->switch_.is_dense = is_dense;
                ci_backpatch_break_continue(ctx, backpatch_start, break_target, UINT32_MAX);
            }
            switch_cleanup:
            ma_cleanup(CcSwitchEntry)(&sw.entries, ctx->a);
            return err;
        }
        case CC_STMT_CASE:{
            if(!ctx->sw)
                return ci_ice(ci, n->loc, "case label outside of switch in lowering%s", "");
            if(!n->case_empty){
                CcSwitchEntry entry = {.low = n->case_.low, .high = n->case_.high, .target = (uint32_t)ctx->out->count};
                err = ma_push(CcSwitchEntry)(&ctx->sw->entries, ctx->a, entry);
                if(err) return CI_OOM_ERROR;
            }
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
            CiInlineState* inline_state = ctx->inline_state;
            CcCleanupRange cleanup = {n->return_.first, NULL};
            if(e && ccqt_bt_eq(e->type, CCBT_void)){
                err = ci_lower_expr_discard(ci, ctx, e);
                if(err) return err;
            }
            else if(e){
                CiLowerVal v;
                _Bool borrow = inline_state
                    && inline_state->borrow_result
                    && n == inline_state->tail_return
                    && !inline_state->returns
                    && !cleanup.first;
                uint32_t result = inline_state && !cleanup.first && !borrow? inline_state->result : CI_NO_SLOT;
                err = ci_lower_expr(ci, ctx, e, result, &v);
                if(err) return err;
                if(cleanup.first){
                    uint32_t saved;
                    err = ci_alloc_slot(ctx, v.size, 16, &saved);
                    if(err) return err;
                    err = ci_emit_copy(ctx, saved, v.slot, v.size, n->loc);
                    if(err) return err;
                    err = ci_lower_cleanups(ci, ctx, cleanup);
                    if(err) return err;
                    v.slot = saved;
                }
                if(inline_state){
                    borrow = borrow && !inline_state->returns;
                    if(!inline_state->result_seen){
                        inline_state->result_value = v.value;
                        inline_state->result_constant = v.constant;
                    }
                    else if(!v.constant
                    || v.value.sz != inline_state->result_value.sz
                    || memcmp(v.value.bits, inline_state->result_value.bits, v.value.sz))
                        inline_state->result_constant = 0;
                    inline_state->result_seen = 1;
                    if(borrow) inline_state->value_slot = v.slot;
                    else if(inline_state->result != CI_NO_SLOT){
                        err = ci_emit_copy(ctx, inline_state->result, v.slot, v.size, n->loc);
                        if(err) return err;
                    }
                    return n == inline_state->tail_return? 0 : ci_lower_inline_return(ctx, n->loc);
                }
                if(!ctx->function_abi){
                    return ma_push(CiOp)(ctx->out, ctx->a, (CiOp){.return_slot = {
                        .kind = CI_OP_RETURN_SLOT,
                        .src = v.slot,
                        .src_size = v.size,
                        .loc = n->loc,
                    }});
                }
                const NcCallPlan* p = ctx->function_abi;
                if(p->result.mode == NC_ABI_INDIRECT){
                    CiAbiAddressSpec destination = (CiAbiAddressSpec){
                        .mode = CI_ABI_SLOT_PTR,
                        .pointer = ctx->result_pointer_slot,
                        .offset = 0
                    };
                    CiAbiAddressSpec src = {.mode = CI_ABI_REG, .reg = {.bank = CI_ABI_INDIRECT_RESULT, .index = 0}};
                    err = ci_abi_emit(ctx->out, ctx->a, n->loc, p->target, CI_ABI_ADDRESS, src, destination, 8);
                    if(err) return err;
                    src = (CiAbiAddressSpec){.mode = CI_ABI_SLOT, .offset = v.slot};
                    err = ci_abi_emit(ctx->out, ctx->a, n->loc, p->target, CI_ABI_COPY, destination, src, p->result.size);
                    if(err) return err;
                    switch(p->target){
                        case CC_TARGET_X86_64_LINUX:
                        case CC_TARGET_X86_64_MACOS: {
                            src = (CiAbiAddressSpec){.mode = CI_ABI_REG, .reg = {.bank = CI_ABI_RESULT_GP, .index = 0}};
                            err = ci_abi_emit(ctx->out, ctx->a, n->loc, p->target, CI_ABI_ADDRESS, src, destination, 8);
                            if(err) return err;
                            break;
                        }
                        case CC_TARGET_X86_64_WINDOWS_MSVC:
                        case CC_TARGET_X86_64_WINDOWS_MINGW: {
                            src = (CiAbiAddressSpec){.mode = CI_ABI_REG, .reg = {.bank = CI_ABI_RESULT_GP, .index = 0}};
                            err = ci_abi_emit(ctx->out, ctx->a, n->loc, p->target, CI_ABI_ADDRESS, src, destination, 8);
                            if(err) return err;
                            break;
                        }
                        case CC_TARGET_AARCH64_LINUX:
                            break;
                        case CC_TARGET_AARCH64_MACOS:
                            break;
                        case CC_TARGET_TEST:
                            break;
                        case CC_TARGET_COUNT:
                            return ci_error(ci, n->loc, "return: unsupported target");
                        DRP_CASES_EXHAUSTED;
                    }
                }
                else {
                    err = ci_lower_abi_result(ctx->out, ctx->a, p, v.slot, 1, n->loc);
                    if(err) return err;
                }
                return ma_push(CiOp)(ctx->out, ctx->a, (CiOp){.kind = CI_OP_RETURN, .loc = n->loc});
            }
            err = ci_lower_cleanups(ci, ctx, cleanup);
            if(err) return err;
            if(inline_state) return n == inline_state->tail_return? 0 : ci_lower_inline_return(ctx, n->loc);
            return ma_push(CiOp)(ctx->out, ctx->a, (CiOp){.kind = CI_OP_RETURN, .loc = n->loc});
        }
        case CC_STMT_BREAK:
        case CC_STMT_CONTINUE:{
            err = ci_lower_cleanups(ci, ctx, n->cleanup);
            if(err) return err;
            CiOp* op;
            err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
            if(err) return err;
            *op = (CiOp){.jump = {.kind = CI_OP_JUMP, .loc = n->loc}};
            CiBackpatchTarget t = {
                .kind = n->kind == CC_STMT_BREAK? CI_BP_BREAK : CI_BP_CONTINUE,
                .levels = n->jump_levels,
                .byteoffset = (size_t)((char*)&op->jump.jump - (char*)ctx->out->data),
                .loc = n->loc,
            };
            err = ma_push(CiBackpatchTarget)(&ctx->backpatches, ctx->a, t);
            if(err) return CI_OOM_ERROR;
            return 0;
        }
        case CC_STMT_GOTO:{
            if(n->goto_.cleanup){
                err = ci_lower_cleanups(ci, ctx, *n->goto_.cleanup);
                if(err) return err;
            }
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
ci_lower_sizeof(CiInterpreter* ci, CiLowerCtx* ctx, CcQualType type, SrcLoc loc, CiLowerVal* out){
    CcExpr* expr;
    int err = cc_sizeof_as_expr(&ci->parser, type, loc, &expr);
    if(err) return err;
    err = ci_lower_expr(ci, ctx, expr, CI_NO_SLOT, out);
    cc_release_expr(&ci->parser, expr);
    return err;
}

static
int
ci_scale_pointer_index(CiInterpreter* ci, CiLowerCtx* ctx, CcQualType type, SrcLoc loc, uint32_t index, uint32_t* out){
    CiLowerVal stride;
    int err = ci_lower_sizeof(ci, ctx, type, loc, &stride);
    if(err) return err;
    err = ci_alloc_slot(ctx, ctx->size_size, ctx->size_size, out);
    if(err) return err;
    CiOp* op;
    err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
    if(err) return err;
    *op = (CiOp){.alu={
        .kind=ci_int_op_kind(ctx->size_size),
        .slot=*out,
        .src=index,
        .src2=stride.slot,
        .op=CI_ALU_MUL,
        .is_unsigned=1,
        .loc=loc,
    }};
    return 0;
}

static
int
ci_lower_init_list(CiInterpreter* ci, CiLowerCtx* ctx, CcExpr* e, CiLowerAddr dst, _Bool zero){
    int err;
    CcParser* p = &ci->parser;
    uint32_t sz;
    err = cc_sizeof_as_uint(p, e->type, e->loc, &sz);
    if(err) return err;
    dst.is_volatile |= e->type.is_volatile;
    if(zero){
        _Bool handled;
        err = ci_try_lower_init_template(ci, ctx, e, dst, sz, &handled);
        if(err || handled) return err;
    }
    CiOp* op;
    if(zero){
        err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
        if(err) return err;
        *op = (CiOp){
            .zero = {
                .is_volatile = dst.is_volatile,
                .kind = CI_OP_ZERO,
                .slot = dst.slot,
                .offset = dst.disp,
                .align_log2 = dst.align_log2,
                .size = sz,
                .loc = e->loc,
            }
        };
    }
    CcInitList* l = e->init_list;
    uint64_t written_end = 0;
    for(uint32_t i = 0; i < l->count; i++){
        CcInitEntry* entry = &l->entries[i];
        CiFieldLoc entry_loc;
        int entry_path_err = ci_init_entry_loc(ci, e->type, entry, &entry_loc);
        if(entry_path_err) return entry_path_err;
        CcExpr* value = entry->value;
        if(!value) continue;
        uint32_t esz;
        err = cc_expr_sizeof_as_uint(p, value, &esz);
        if(err) return err;
        uint32_t off = dst.disp + (uint32_t)entry_loc.byte_offset;
        uint64_t start = entry_loc.byte_offset;
        uint64_t end = start + (entry_loc.bit_width ? (entry_loc.bit_offset + entry_loc.bit_width + 7u) / 8u : esz);
        if(!entry_loc.bit_width && value->kind == CC_EXPR_INIT_LIST){
            _Bool overwritten = 0;
            // In-order, disjoint initializers need no scan or extra zeroing.
            // A backwards designator may replace an already written subobject.
            if(start < written_end && esz){
                for(uint32_t j = 0; j < i; j++){
                    CcInitEntry* prev = &l->entries[j];
                    if(!prev->value) continue;
                    if(cc_field_paths_overlap(e->type, entry->path, prev->path)){
                        overwritten = 1;
                        break;
                    }
                }
            }
            err = ci_lower_init_list(ci, ctx, value, (CiLowerAddr){.slot = dst.slot, .disp = off, .is_volatile = dst.is_volatile, .align_log2 = cc_offset_alignment(dst.align_log2, entry_loc.byte_offset)}, overwritten);
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
        if(entry_loc.bit_width){
            *op = (CiOp){
                .store_bf = {
                    .is_volatile = dst.is_volatile,
                    .kind = ci_bitfield_needs_bytes(e->type, entry->path, esz) ? CI_OP_STORE_BITFIELD_BYTES : CI_OP_STORE_BITFIELD,
                    .slot = dst.slot,
                    .src = v.slot,
                    .src_size = esz,
                    .offset = off,
                    .align_log2 = cc_offset_alignment(dst.align_log2, entry_loc.byte_offset),
                    .bit_offset = entry_loc.bit_offset,
                    .bit_width = entry_loc.bit_width,
                    .loc = value->loc,
                }
            };
            err = ci_lower_whole_byte_bitfield(ctx, op);
            if(err) return err;
        }
        else {
            *op = (CiOp){
                .store = {
                    .is_volatile = dst.is_volatile,
                    .kind = CI_OP_STORE,
                    .slot = dst.slot,
                    .src = v.slot,
                    .src_size = esz,
                    .offset = off,
                    .align_log2 = cc_offset_alignment(dst.align_log2, entry_loc.byte_offset),
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
ci_convert_scalar(CiLowerCtx* ctx, CiLowerVal v, CcQualType from, CcQualType to, uint32_t dest, uint32_t size, SrcLoc loc){
    from = ccqt_enum_underlying(from);
    to = ccqt_enum_underlying(to);
    if(ccqt_is_bool(to)) return ci_lower_istrue(ctx, &v, from, dest, size, 0, loc);
    _Bool ff = ci_falu_type(from), tf = ci_falu_type(to);
    if(!ff && !tf && v.size == size)
        return ci_emit_copy(ctx, dest, v.slot, size, loc);
    CiOpKind kind = ff ? (tf ? CI_OP_FTOF : CI_OP_FTOI) : (tf ? CI_OP_ITOF : CI_OP_CONVERT);
    CiOp* op;
    int err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
    if(err) return err;
    *op = (CiOp){.convert = {
        .kind = kind,
        .slot = dest,
        .slot_size = size,
        .src = v.slot,
        .src_size = v.size,
        .src_is_brian_float = ccqt_bt_eq(from, CCBT_bfloat16),
        .src_float = ci_float_width(from, ctx->ldbl_fmt),
        .dst_is_brian_float = ccqt_bt_eq(to, CCBT_bfloat16),
        .dst_float = ci_float_width(to, ctx->ldbl_fmt),
        .is_unsigned = ccqt_is_unsigned(ff && !tf ? to : from, ctx->char_is_unsigned),
        .loc = loc
    }};
    return 0;
}

static
int
ci_lower_promoted_compound(CiInterpreter* ci, CiLowerCtx* ctx, CcExpr* e, uint32_t dest, uint32_t size, CiLowerVal*_Nullable out){
    CcExpr* lhs = e->lhs;
    CcQualType arithmetic = e->compound.type;
    uint32_t arithmetic_size;
    int err = cc_sizeof_as_uint(&ci->parser, arithmetic, e->loc, &arithmetic_size);
    if(err) return err;
    uint32_t cur;
    err = ci_alloc_slot(ctx, size, size, &cur);
    if(err) return err;
    uint32_t keep = ctx->temp;
    _Bool bitfield = cc_expr_field_bit_width(lhs) != 0;
    _Bool atomic = lhs->type.is_atomic;
    CiLowerAddr a;
    err = bitfield ? ci_lower_bitfield_addr(ci, ctx, lhs, &a) : ci_lower_addr(ci, ctx, lhs, 0, &a);
    if(err) return err;
    CiLowerVal r;
    err = ci_lower_expr(ci, ctx, e->values[0], CI_NO_SLOT, &r);
    if(err) return err;
    uint32_t old, promoted, result, ok = 0;
    err = ci_alloc_slot(ctx, size, size, &old);
    if(err) return err;
    err = ci_alloc_slot(ctx, arithmetic_size, arithmetic_size, &promoted);
    if(err) return err;
    err = ci_alloc_slot(ctx, arithmetic_size, arithmetic_size, &result);
    if(err) return err;
    if(atomic){
        err = ci_alloc_slot(ctx, 1, 1, &ok);
        if(err) return err;
    }
    CiOp* op;
    if(bitfield){
        err = ci_emit_load_bitfield(ci, ctx, lhs, a, old, size);
        if(err) return err;
    }
    else {
        err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
        if(err) return err;
        if(atomic) *op = (CiOp){.atomic_load = {.is_volatile = lhs->type.is_volatile, .kind = CI_OP_ATOMIC_LOAD,
            .memorder = CC_MO_SEQ_CST, .slot = old, .slot_size = size,
            .src = a.slot, .offset = a.disp, .loc = e->loc}};
        else *op = (CiOp){.load = {
            .is_volatile = lhs->type.is_volatile,
            .kind = CI_OP_LOAD,
            .slot = old,
            .slot_size = size,
            .src = a.slot,
            .offset = a.disp,
            .align_log2 = a.align_log2,
            .loc = e->loc
        }};
    }
    uint32_t loop = (uint32_t)ctx->out->count;
    err = ci_convert_scalar(ctx, (CiLowerVal){.slot = old, .size = size}, lhs->type,
                            arithmetic, promoted, arithmetic_size, e->loc);
    if(err) return err;
    err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
    if(err) return err;
    if(ci_falu_type(arithmetic)){
        CiFaluOp fop;
        if(!ci_falu_op_for(e->kind, &fop)) return ci_ice(ci, e->loc, "invalid floating compound operator%s", "");
        *op = (CiOp){.falu32 = {
            .is_brian_float = ccqt_bt_eq(arithmetic, CCBT_bfloat16),
            .kind = ci_falu_kind(arithmetic, ctx->ldbl_fmt),
            .op = fop, .slot = result, .slot_size = arithmetic_size,
            .src = promoted, .src2 = r.slot, .loc = e->loc
        }};
    }
    else *op = (CiOp){.alu = {.kind = ci_int_op_kind(arithmetic_size),
        .op = ci_alu_op_for(e->kind), .is_unsigned = ccqt_is_unsigned(arithmetic, ctx->char_is_unsigned),
        .slot = result, .src = promoted, .src2 = r.slot, .loc = e->loc}};
    err = ci_convert_scalar(ctx, (CiLowerVal){.slot = result, .size = arithmetic_size},
                            arithmetic, lhs->type, cur, size, e->loc);
    if(err) return err;
    if(bitfield){
        err = ci_emit_store_bitfield(ctx, lhs, a, cur, size);
        if(err) return err;
        if(out){
            err = ci_emit_load_bitfield(ci, ctx, lhs, a, cur, size);
            if(err) return err;
        }
    }
    else {
        err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
        if(err) return err;
        if(atomic){
            *op = (CiOp){.atomic_cas = {.is_volatile = lhs->type.is_volatile, .kind = CI_OP_ATOMIC_CAS,
                .memorder = CC_MO_SEQ_CST, .fail_memorder = CC_MO_SEQ_CST, .weak = 1,
                .size = size, .slot = ok, .src = a.slot, .expected = old,
                .desired = cur, .offset = a.disp, .loc = e->loc}};
            err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
            if(err) return err;
            *op = (CiOp){.jump_false = {.kind = CI_OP_JUMP_FALSE, .slot = ok,
                .slot_size = 1, .jump = loop, .loc = e->loc}};
        }
        else *op = (CiOp){.store = {
            .is_volatile = lhs->type.is_volatile,
            .kind = CI_OP_STORE,
            .slot = a.slot,
            .src = cur,
            .src_size = size,
            .offset = a.disp,
            .align_log2 = a.align_log2,
            .loc = e->loc
        }};
    }
    ctx->temp = keep;
    if(!out) return 0;
    out->slot = cur;
    out->canonical = ccqt_is_bool(lhs->type);
    if(dest != CI_NO_SLOT){
        err = ci_emit_copy(ctx, dest, cur, size, e->loc);
        if(err) return err;
        out->slot = dest;
    }
    return 0;
}

enum CiImmediateResult TYPED_ENUM(uint32_t){
    CI_IMMEDIATE_ALU,
    CI_IMMEDIATE_IDENTITY,
    CI_IMMEDIATE_CONSTANT,
};
TYPEDEF_ENUM(CiImmediateResult, uint32_t);

static
CiImmediateResult
ci_simplify_integer_immediate(CiAluOp* op, uint64_t* immediate, uint32_t size, _Bool uns){
    uint64_t n = *immediate;
    uint64_t ones = size == 4 ? UINT32_MAX : UINT64_MAX;
    if((!n && (*op == CI_ALU_ADD || *op == CI_ALU_SUB || *op == CI_ALU_OR || *op == CI_ALU_XOR || *op == CI_ALU_SHL || *op == CI_ALU_SHR))
    || (n == 1 && (*op == CI_ALU_MUL || *op == CI_ALU_DIV))
    || (n == ones && *op == CI_ALU_AND))
        return CI_IMMEDIATE_IDENTITY;
    if((!n && (*op == CI_ALU_MUL || *op == CI_ALU_AND))
        || (n == ones && *op == CI_ALU_OR) || (n == 1 && *op == CI_ALU_MOD)){
        *immediate = *op == CI_ALU_OR ? ones : 0;
        return CI_IMMEDIATE_CONSTANT;
    }
    if(n && !(n & (n-1))){
        if(*op == CI_ALU_MUL){
            *op = CI_ALU_SHL;
            *immediate = ctz_64(n);
        }
        else if(uns && *op == CI_ALU_DIV){
            *op = CI_ALU_SHR;
            *immediate = ctz_64(n);
        }
        else if(uns && *op == CI_ALU_MOD){
            *op = CI_ALU_AND;
            *immediate = n-1;
        }
    }
    return CI_IMMEDIATE_ALU;
}

static
int
ci_lower_compound(CiInterpreter* ci, CiLowerCtx* ctx, CcExpr* e, uint32_t dest, CiLowerVal*_Nullable out){
    CcParser* p = &ci->parser;
    uint32_t size;
    int err = cc_sizeof_as_uint(p, e->type, e->loc, &size);
    if(err) return err;
    if(out) *out = (CiLowerVal){.size=size, .canonical=ccqt_is_bool(e->type)};
    CcExpr* lhs = e->lhs;
    CcExpr* rhs = e->values[0];
    CcQualType storage_type = e->type;
    storage_type = ccqt_enum_underlying(storage_type);
    storage_type = (CcQualType){.unqual = storage_type.unqual};
    if(e->compound.type.bits && e->compound.type.bits != storage_type.bits)
        return ci_lower_promoted_compound(ci, ctx, e, dest, size, out);
    // Identical arithmetic and storage types can operate in place.
    CiOpKind opkind;
    CiFaluOp fop = 0;
    _Bool is_ptr = 0;
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
        opkind = ci_falu_kind(e->type, ctx->ldbl_fmt);
    }
    else if(ccqt_kind(e->type) == CC_POINTER
         && (e->kind == CC_EXPR_ADDASSIGN || e->kind == CC_EXPR_SUBASSIGN)
         && ccqt_is_integer(rhs->type)){
        uint32_t rsz;
        err = cc_sizeof_as_uint(p, rhs->type, rhs->loc, &rsz);
        if(err) return err;
        if(rsz != ctx->size_size)
            return ci_ice(ci, e->loc, "ptr arith failed to implicit cast to right size: %u", rsz);
        is_ptr = 1;
        opkind = ci_int_op_kind(ctx->ptr_size);
        op_unsigned = ccqt_is_unsigned(rhs->type, ctx->char_is_unsigned);
    }
    else {
        return ci_unimplemented(ci, e->loc, "huh?");
    }
    if(cc_expr_field_bit_width(lhs)){
        if(opkind != CI_OP_ALU128 && opkind != CI_OP_ALU64 && opkind != CI_OP_ALU32 && opkind != CI_OP_ALU16 && opkind != CI_OP_ALU8)
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
        if(out){
            err = ci_emit_load_bitfield(ci, ctx, lhs, a, cur, size);
            if(err) return err;
        }
        ctx->temp = keep; // free rhs and address scratch; cur survives
        if(!out) return 0;
        if(dest == CI_NO_SLOT){
            out->slot = cur;
            return 0;
        }
        out->slot = dest;
        err = ci_emit_copy(ctx, dest, cur, size, e->loc);
        if(err) return err;
        return 0;
    }
    // Resolve the target: a frame slot operated on in place, or a
    // computed address loaded into a temp and stored back.
    uint32_t cur;
    CiLowerAddr a;
    _Bool is_mem;
    uint32_t vslot;
    CiOp* op;
    if(ci_frame_lvalue(ci, ctx, lhs, &vslot)){
        is_mem = 0;
        cur = vslot;
    }
    else if(lhs->type.is_atomic){
        // rmw handles the hardware ops at <= 8 bytes; floats and
        // 128-bit ints go through a compare-exchange loop instead.
        _Bool op_is_float = opkind == CI_OP_FALU16 || opkind == CI_OP_FALU32 || opkind == CI_OP_FALU64 || opkind == CI_OP_FALU80 || opkind == CI_OP_FALU128;
        CiLowerVal ignored;
        return ci_lower_atomic_compound_assign(ci, ctx, e, dest, out ? (CiLowerVal*)out : &ignored, size, is_ptr, op_unsigned, op_is_float, fop);
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
                .is_volatile = lhs->type.is_volatile,
                .kind = CI_OP_LOAD,
                .slot = cur,
                .slot_size = size,
                .src = a.slot,
                .offset = a.disp,
                .align_log2 = a.align_log2,
                .loc = e->loc,
            }
        };
    }
    CiFoldValue fold;
    _Bool use_imm = 0;
    if(!is_ptr && (opkind == CI_OP_ALU32 || opkind == CI_OP_ALU64)){
        int fold_fail = ci_fold_expr(ci, ctx, rhs, &fold);
        if(fold_fail > 0) return fold_fail;
        use_imm = fold_fail == 0 && fold.sz == size;
    }
    CiAluOp alu_op = ci_alu_op_for(e->kind);
    uint64_t immediate = use_imm ? ci_read_uint(fold.bits, size) : 0;
    CiImmediateResult simplified = use_imm ? ci_simplify_integer_immediate(&alu_op, &immediate, size, op_unsigned) : CI_IMMEDIATE_ALU;
    if(simplified == CI_IMMEDIATE_IDENTITY && !is_mem && !lhs->type.is_volatile){
        ctx->temp = keep;
        if(!out) return 0;
        err = ci_lower_expr_dest(ci, ctx, e, &dest, size);
        if(err) return err;
        return ci_lower_expr(ci, ctx, lhs, dest, (CiLowerVal*)out);
    }
    CiLowerVal r = {0};
    if(!use_imm){
        err = ci_lower_expr(ci, ctx, rhs, CI_NO_SLOT, &r);
        if(err) return err;
    }
    if(is_ptr){
        err = ci_scale_pointer_index(ci, ctx, ccqt_as_ptr(e->type)->pointee, e->loc, r.slot, &r.slot);
        if(err) return err;
        r.size = ctx->ptr_size;
        op_unsigned = 1;
    }
    err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
    if(err) return err;
    if(simplified == CI_IMMEDIATE_CONSTANT){
        *op = (CiOp){.constant={.kind=CI_OP_CONST, .slot=cur,
            .immsize=size, .loc=e->loc,
            .bt_kind=ccqt_is_basic(e->type) ? (uint32_t)ccqt_basic_kind(e->type) : CCBT_void}};
        ci_write_uint(op->constant.immediate, size, immediate);
    }
    else if(use_imm){
        *op = (CiOp){
            .alu_imm = {
                .kind = size == 4 ? CI_OP_ALU_IMM32 : CI_OP_ALU_IMM64,
                .op = alu_op,
                .is_unsigned = op_unsigned,
                .slot = cur,
                .src = cur,
                .loc = e->loc,
            }
        };
        op->alu_imm.immediate = immediate;
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
                .is_brian_float = ccqt_bt_eq(e->type, CCBT_bfloat16),
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
                .is_volatile = lhs->type.is_volatile,
                .kind = CI_OP_STORE,
                .slot = a.slot,
                .src = cur,
                .src_size = size,
                .offset = a.disp,
                .align_log2 = a.align_log2,
                .loc = e->loc,
            }
        };
    }
    ctx->temp = keep; // free rhs and address scratch; cur survives
    if(!out) return 0;
    if(dest == CI_NO_SLOT && is_mem){
        out->slot = cur;
        return 0;
    }
    // Frame storage can be changed by a subsequent operand.
    err = ci_lower_expr_dest(ci, ctx, e, &dest, size);
    if(err) return err;
    out->slot = dest;
    err = ci_emit_copy(ctx, dest, cur, size, e->loc);
    if(err) return err;
    return 0;
}

static
int
ci_lower_constant(CiInterpreter* ci, CiLowerCtx* ctx, CcExpr* e, const CiFoldValue* value, uint32_t dest, CiLowerVal* out){
    int err = ci_lower_expr_dest(ci, ctx, e, &dest, value->sz);
    if(err) return err;
    *out = (CiLowerVal){
        .slot=dest,
        .size=value->sz,
        .constant=1,
        .value=*value,
        .canonical=ccqt_is_integer(value->type) && !value->bits[1] && value->bits[0] <= 1
    };
    CiOp* op;
    err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
    if(err) return err;
    *op = (CiOp){.constant={
        .kind=CI_OP_CONST,
        .slot=dest,
        .immsize=value->sz,
        .bt_kind=ccqt_is_basic(value->type) ? (uint32_t)ccqt_basic_kind(value->type) : CCBT_void,
        .loc=e->loc
    }};
    memcpy(op->constant.immediate, value->bits, sizeof value->bits);
    return 0;
}

static
_Bool
ci_expr_is_boolean(CcExpr* e){
    if(ccqt_is_bool(e->type)) return 1;
    switch((uint32_t)e->kind){
        case CC_EXPR_LOGNOT: case CC_EXPR_LOGAND: case CC_EXPR_LOGOR:
        case CC_EXPR_EQ: case CC_EXPR_NE: case CC_EXPR_LT: case CC_EXPR_LE:
        case CC_EXPR_GT: case CC_EXPR_GE: return 1;
        default: return 0;
    }
}

static
int
ci_lower_assign_inner(CiInterpreter* ci, CiLowerCtx* ctx, CcExpr* e, uint32_t dest, CiLowerVal*_Nullable out, uint32_t size){
    int err;
    CcParser* p = &ci->parser;
    CcExpr* lhs = e->lhs;
    CcExpr* rhs = e->values[0];
    uint32_t rsz;
    err = cc_sizeof_as_uint(p, rhs->type, rhs->loc, &rsz);
    if(err) return err;
    if(rsz < size && ccqt_is_fixed_sequence(lhs->type) && ccqt_is_fixed_sequence(rhs->type)){
        CiLowerAddr a;
        err = ci_lower_addr(ci, ctx, lhs, 0, &a);
        if(err) return err;
        CiLowerVal v;
        err = ci_lower_expr(ci, ctx, rhs, CI_NO_SLOT, &v);
        if(err) return err;
        CiOp* op;
        err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
        if(err) return err;
        *op = (CiOp){.store = {
            .is_volatile = lhs->type.is_volatile,
            .kind = CI_OP_STORE,
            .slot = a.slot,
            .offset = a.disp,
            .align_log2 = a.align_log2,
            .src = v.slot,
            .src_size = rsz,
            .loc = e->loc
        }};
        err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
        if(err) return err;
        *op = (CiOp){.zero = {
            .is_volatile = lhs->type.is_volatile,
            .kind = CI_OP_ZERO,
            .slot = a.slot,
            .offset = a.disp + rsz,
            .align_log2 = cc_offset_alignment(a.align_log2, rsz),
            .size = size - rsz,
            .loc = e->loc
        }};
        if(out){
            err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
            if(err) return err;
            *op = (CiOp){.load = {
                .is_volatile = lhs->type.is_volatile,
                .kind = CI_OP_LOAD,
                .slot = dest,
                .slot_size = size,
                .src = a.slot,
                .offset = a.disp,
                .align_log2 = a.align_log2,
                .loc = e->loc
            }};
            out->slot = dest;
        }
        return 0;
    }
    if(rsz != size)
        return ci_unreachable(ci, rhs->loc, "size mismatch");
    if(lhs->type.is_atomic){
        CiLowerAddr a;
        err = ci_lower_addr(ci, ctx, lhs, 0, &a); // access
        if(err) return err;
        CiLowerVal v;
        err = ci_lower_expr(ci, ctx, rhs, dest, &v);
        if(err) return err;
        err = ci_lower_atomic_operand(ci, ctx, rhs, &v, size, e->loc);
        if(err) return err;
        CiOp* op;
        err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
        if(err) return err;
        *op = (CiOp){
            .atomic_store = {
                .is_volatile = lhs->type.is_volatile,
                .kind = CI_OP_ATOMIC_STORE,
                .memorder = CC_MO_SEQ_CST,
                .slot = a.slot,
                .src = v.slot,
                .src_size = size,
                .offset = a.disp,
                .loc = e->loc,
            }
        };
        if(out){
            out->slot = v.slot;
            out->canonical = v.canonical;
        }
        return 0;
    }
    uint32_t vslot;
    if(ci_frame_lvalue(ci, ctx, lhs, &vslot)){
        _Bool stage = !(lhs->kind == CC_EXPR_VARIABLE && lhs->var->initializer == rhs)
            && (ccqt_kind(rhs->type) == CC_STRUCT || ccqt_kind(rhs->type) == CC_UNION || ccqt_is_fixed_sequence(rhs->type) || ccqt_kind(rhs->type) == CC_SLICE)
            && (rhs->kind == CC_EXPR_TERNARY || rhs->kind == CC_EXPR_COMMA || rhs->kind == CC_EXPR_CAST
                || rhs->kind == CC_EXPR_CALL || rhs->kind == CC_EXPR_STATEMENT_EXPRESSION);
        CiLowerVal v;
        err = ci_lower_expr(ci, ctx, rhs, stage ? CI_NO_SLOT : vslot, &v);
        if(err) return err;
        if(stage) err = ci_emit_copy(ctx, vslot, v.slot, size, e->loc);
        if(err) return err;
        if(lhs->kind==CC_EXPR_VARIABLE
        && lhs->var->initializer==rhs
        && !lhs->type.is_volatile
        && !lhs->type.is_atomic
        && !lhs->var->mutable_storage){
            CiInlineBinding* b = ci_inline_binding(ctx, lhs->var);
            if(b){
                _Bool changed= b->constant != v.constant || (v.constant && (b->value.sz != v.value.sz || b->value.type.bits !=v.value.type.bits || memcmp(b->value.bits, v.value.bits, v.value.sz)));
                b->constant = v.constant;
                b->value = v.value;
                if(changed && ctx->fold_cache){
                    PM_clear(&ctx->fold_cache->expressions);
                    ctx->fold_cache->values.count = 0;
                }
            }
        }
        if(!out) return 0;
        out->constant  = v.constant;
        out->value     = v.value;
        out->canonical = v.canonical;
        out->slot      = dest;
        err = ci_emit_copy(ctx, dest, vslot, size, e->loc);
        if(err) return err;
        return 0;
    }
    CiLowerAddr a;
    if(cc_expr_field_bit_width(lhs)){
        err = ci_lower_bitfield_addr(ci, ctx, lhs, &a);
        if(err) return err;
        CiLowerVal v;
        err = ci_lower_expr(ci, ctx, rhs, CI_NO_SLOT, &v);
        if(err) return err;
        err = ci_emit_store_bitfield(ctx, lhs, a, v.slot, size);
        if(err) return err;
        if(!out) return 0;
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
            .is_volatile = lhs->type.is_volatile,
            .kind = CI_OP_STORE,
            .slot = a.slot,
            .src = v.slot,
            .src_size = size,
            .offset = a.disp,
            .align_log2 = a.align_log2,
            .loc = e->loc,
        }
    };
    if(out){
        out->slot = v.slot;
        out->canonical = v.canonical;
    }
    return 0;
}

static
int
ci_lower_assign(CiInterpreter* ci, CiLowerCtx* ctx, CcExpr* e, uint32_t dest, CiLowerVal*_Nullable out, uint32_t size){
    int err;
    if(out){
        err = ci_lower_expr_dest(ci, ctx, e, &dest, size);
        if(err) return err;
    }
    uint32_t temp = ctx->temp;
    uint64_t materialized = ctx->materialized_addresses;
    err = ci_lower_assign_inner(ci, ctx, e, dest, out, size);
    if(err) return err;
    if(out){
        err = ci_emit_copy(ctx, dest, out->slot, size, e->loc);
        if(err) return err;
        out->slot = dest;
    }
    if(ctx->materialized_addresses == materialized) ctx->temp = temp;
    return 0;
}

static
int
ci_lower_expr(CiInterpreter* ci, CiLowerCtx* ctx, CcExpr* e, uint32_t dest, CiLowerVal* out){
    uint32_t temp = ctx->temp;
    *out=(CiLowerVal){0};
    int err = ci_lower_expr_inner(ci, ctx, e, dest, out);
    if(err) return err;
    out->borrowed = out->slot < temp;
    if(out->constant) out->value.type = e->type;
    return 0;
}

static
int
ci_preserve_value(CiInterpreter* ci, CiLowerCtx* ctx, CcExpr* e, CiLowerVal* value){
    if(!value->borrowed) return 0;
    if(ci_stable_variable(ctx, e)) return 0;
    uint32_t dest = CI_NO_SLOT;
    int err = ci_lower_expr_dest(ci, ctx, e, &dest, value->size);
    if(err) return err;
    err = ci_emit_copy(ctx, dest, value->slot, value->size, e->loc);
    if(err) return err;
    value->slot = dest;
    value->borrowed = 0;
    return 0;
}

static
int
ci_emit_check(CiLowerCtx* ctx, uint32_t src, uint32_t size, _Bool negate,
    const char* message, SrcLoc loc){
    uint32_t condition;
    int err = ci_alloc_slot(ctx, 1, 1, &condition);
    if(err) return err;
    err = ma_push(CiOp)(ctx->out, ctx->a, (CiOp){.istrue={
        .kind=CI_OP_ISTRUE,
        .src=src,
        .src_size=size,
        .slot=condition,
        .slot_size=1,
        .negate=negate,
        .loc=loc,
    }});
    if(err) return err;
    return ma_push(CiOp)(ctx->out, ctx->a, (CiOp){.check={
        .kind=CI_OP_CHECK,
        .src=condition,
        .message=message,
        .loc=loc,
    }});
}

static
int
ci_lower_size_product(CiInterpreter* ci, CiLowerCtx* ctx, CcExpr* e, uint32_t dest, CiLowerVal* out){
    int err = ci_lower_expr_dest(ci, ctx, e, &dest, ctx->size_size);
    if(err) return err;
    out->slot = dest;
    uint32_t temp = ctx->temp;
    CiLowerVal l, r;
    err = ci_lower_expr(ci, ctx, e->lhs, CI_NO_SLOT, &l);
    if(err) return err;
    err = ci_lower_expr(ci, ctx, e->values[0], CI_NO_SLOT, &r);
    if(err) return err;
    if(e->binary.element_alignment){
        const char* message = "array element size is not a multiple of its alignment";
        err = ci_emit_check(ctx, r.slot, r.size, 0, message, e->loc);
        if(err) return err;
        uint32_t alignment, remainder;
        err = ci_emit_const(ci, ctx, e->binary.element_alignment, r.size, e->loc, &alignment);
        if(err) return err;
        err = ci_alloc_slot(ctx, r.size, r.size, &remainder);
        if(err) return err;
        err = ci_emit_alu(ctx, CI_ALU_MOD, remainder, r.slot, alignment, r.size, e->loc);
        if(err) return err;
        err = ci_emit_check(ctx, remainder, r.size, 1, message, e->loc);
        if(err) return err;
    }
    uint32_t overflow;
    err = ci_alloc_slot(ctx, 1, 1, &overflow);
    if(err) return err;
    err = ma_push(CiOp)(ctx->out, ctx->a, (CiOp){.checked={
        .kind=CI_OP_CHECKED,
        .op=CI_CHK_MUL,
        .src_size=l.size,
        .src2_size=r.size,
        .res_size=ctx->size_size,
        .src_unsigned=1,
        .src2_unsigned=1,
        .res_unsigned=1,
        .result=dest,
        .overflow=overflow,
        .src=l.slot,
        .src2=r.slot,
        .loc=e->loc,
    }});
    if(err) return err;
    err = ci_emit_check(ctx, overflow, 1, 1, "VLA size exceeds size_t range", e->loc);
    ctx->temp = temp;
    return err;
}

static
int
ci_lower_expr_inner(CiInterpreter* ci, CiLowerCtx* ctx, CcExpr* e, uint32_t dest, CiLowerVal* out){
    int err;
    if(ccqt_is_basic(e->type) && ccbt_is_complex(ccqt_basic_kind(e->type))){
        ci_error(ci, e->loc, "complex expression lowering is not supported");
        return CI_UNIMPLEMENTED_ERROR;
    }
    if(ccqt_kind(e->type) == CC_FUNCTION && e->kind == CC_EXPR_DEREF)
        return ci_lower_expr(ci, ctx, e->lhs, dest, out);
    {
        CiFoldValue fold_val;
        int fold_fail = ci_fold_expr(ci, ctx, e, &fold_val);
        if(fold_fail > 0) return fold_fail;
        if(fold_fail == 0) return ci_lower_constant(ci, ctx, e, &fold_val, dest, out);
    }
    CcParser* p = &ci->parser;
    uint32_t size;
    if(ccqt_kind(e->type) == CC_FUNCTION) size = ctx->ptr_size;
    else {
        err = cc_expr_sizeof_as_uint(p, e, &size);
        if(err) return err;
    }
    out->size = size;
    out->canonical = e->kind != CC_EXPR_BLOB && ccqt_is_bool(e->type);
    switch(e->kind){
        case CC_EXPR_FUNCTION:
            return ci_lower_cast_operand(ci, ctx, e, dest, out);
        case CC_EXPR_BLOB:
        case CC_EXPR_VALUE:{
            if(e->kind == CC_EXPR_BLOB || ccqt_is_fixed_sequence(e->type) || ccqt_kind(e->type) == CC_VLA){
                err = ci_lower_expr_dest(ci, ctx, e, &dest, size);
                if(err) return err;
                out->slot = dest;
                uint32_t aslot;
                err = ci_alloc_slot(ctx, 8, 8, &aslot);
                if(err) return err;
                CiOp* op;
                err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
                if(err) return err;
                *op = (CiOp){
                    .constant = {
                        .kind = CI_OP_CONST,
                        .bt_kind = (uint32_t)(ccqt_is_basic(cc_expr_array_element(e))?ccqt_basic_kind(cc_expr_array_element(e)):CCBT_nullptr_t),
                        .is_anon_array = 1,
                        .immsize = 8,
                        .slot = aslot,
                        .immediate = {
                            (uint64_t)cc_expr_array_data(e),
                            cc_expr_array_count(e),
                        },
                        .loc = e->loc,
                    },
                };
                err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
                if(err) return err;
                *op = (CiOp){
                    .load = {
                        .is_volatile = e->type.is_volatile,
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
            if(var->automatic && var->constexpr_ && !var->mutable_storage){
                if(dest == CI_NO_SLOT){
                    uint32_t align;
                    err = cc_alignof_as_uint(p, var->type, e->loc, &align);
                    if(err) return err;
                    if(var->alignment > align) align = var->alignment;
                    err = ci_alloc_slot(ctx, size, align, &dest);
                    if(err) return err;
                }
                return ci_lower_expr(ci, ctx, var->initializer, dest, out);
            }
            if(e->type.is_atomic){
                return ci_lower_atomic_load_lv(ci, ctx, e, dest, out, size);
            }
            if(!var->automatic){
                uint32_t alignment;
                err = ci_lvalue_alignment(ci, ctx, e, &alignment);
                if(err) return err;
                err = ci_deps_add_var(&ctx->deps, ctx->a, var, CI_VAR_DEP_USED);
                if(err) return CI_OOM_ERROR;
                err = ci_lower_expr_dest(ci, ctx, e, &dest, size);
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
                        .kind = var->thread_local_ ? CI_OP_TLS_ADDR : CI_OP_VAR_ADDR,
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
                        .is_volatile = e->type.is_volatile,
                        .kind = CI_OP_LOAD,
                        .slot = dest,
                        .slot_size = size,
                        .src = aslot,
                        .align_log2 = alignment,
                        .loc = e->loc,
                    }
                };
                ctx->temp = temp;
                return 0;
            }
            if(dest == CI_NO_SLOT && e->type.is_volatile){
                err = ci_lower_expr_dest(ci, ctx, e, &dest, size);
                if(err) return err;
            }
            if(dest == CI_NO_SLOT){
                // the variable's storage is already a slot; refer to it directly
                out->slot = ci_variable_slot(ctx,var);
                return 0;
            }
            out->slot = dest;
            err = ci_emit_copy_access(ctx, dest, ci_variable_slot(ctx,var), size, e->loc, e->type.is_volatile);
            if(err) return err;
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
            if((e->kind == CC_EXPR_DOT || e->kind == CC_EXPR_ARROW) && cc_expr_field_bit_width(e)){
                err = ci_lower_expr_dest(ci, ctx, e, &dest, size);
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
            if(ci_frame_lvalue(ci, ctx, e, &off)){
                // a member of a local is already a slot
                if(dest == CI_NO_SLOT){
                    out->slot = off;
                    return 0;
                }
                out->slot = dest;
                err = ci_emit_copy_access(ctx, dest, off, size, e->loc, e->type.is_volatile);
                if(err) return err;
                return 0;
            }
            err = ci_lower_expr_dest(ci, ctx, e, &dest, size);
            if(err) return err;
            uint32_t temp = ctx->temp;
            CiLowerAddr a;
            if(e->kind == CC_EXPR_DEREF)
                err = ci_lower_pointer_addr(ci, ctx, e->lhs, 0, 0, &a);
            else if(e->kind == CC_EXPR_ARROW){
                uint64_t field_offset;
                err = cc_expr_field_offset(ci_target(ci), e, &field_offset);
                if(err) return err;
                err = ci_lower_pointer_addr(ci, ctx, e->values[0], (uint32_t)field_offset, 0, &a);
            }
            else err = ci_lower_addr(ci, ctx, e, 0, &a); // access
            if(err) return err;
            out->slot = dest;
            CiOp* op;
            err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
            if(err) return err;
            *op = (CiOp){
                .load = {
                    .is_volatile = e->type.is_volatile,
                    .kind = CI_OP_LOAD,
                    .slot = dest,
                    .slot_size = size,
                    .src = a.slot,
                    .offset = a.disp,
                    .align_log2 = a.align_log2,
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
            if(ci_frame_lvalue(ci, ctx, lv, &off)){
                err = ci_lower_expr_dest(ci, ctx, e, &dest, size);
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
            return ci_addr_to_value(ctx, a, dest, size, ci_target(ci)->alignof_[CCBT_nullptr_t], e->loc, out);
        }
        case CC_EXPR_CAST:{
            CcExpr* operand = e->lhs;
            if(ccqt_is_array(operand->type) && ccqt_kind(e->type) == CC_SLICE)
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
            if(cc_conversion_preserves_representation(from, to))
                return ci_lower_expr(ci, ctx, operand, dest, out);
            if(ccqt_bt_eq(to, CCBT__Any)){
                err = ci_lower_expr_dest(ci, ctx, e, &dest, size);
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
                err = ci_emit_copy(ctx, dest + offsetof(CiRtAny, payload), v.slot, src_size, e->loc);
                if(err) return err;
                ctx->temp = temp;
                return 0;
            }
            _Bool from_float = ci_falu_type(from);
            _Bool from_addr = ccqt_kind(from) == CC_POINTER || ccqt_is_fixed_sequence(from)
                            || ccqt_kind(from) == CC_VLA || ccqt_kind(from) == CC_FUNCTION
                            || ccqt_bt_eq(from, CCBT_nullptr_t);
            _Bool from_int = from_addr || ccqt_is_integer(from);
            uint32_t from_sz = ctx->ptr_size;
            if(ccqt_is_integer(from)){
                err = cc_sizeof_as_uint(p, from, e->loc, &from_sz);
                if(err) return err;
            }
            if(ccqt_is_bool(to)){
                err = ci_lower_expr_dest(ci, ctx, e, &dest, size);
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
            if(!from_int && !from_float)
                return ci_unimplemented(ci, e->loc, "cast from unsupported type");
            _Bool to_int = ccqt_has_pointer_representation(to) || ccqt_is_integer(to) || ccqt_bt_eq(to, CCBT__Type);
            _Bool to_float = ci_falu_type(to);
            CiOpKind kind;
            uint32_t is_unsigned = 0;
            if(from_int && to_int){
                while(operand->kind == CC_EXPR_CAST && ccqt_is_integer(from)
                    && !cc_expr_has_runtime_check(e)
                    && !cc_expr_has_runtime_check(operand)
                    && ccqt_is_integer(to) && !ccqt_is_bool(from)
                    && !from.is_volatile && !from.is_atomic
                    && ccqt_is_integer(operand->lhs->type)){
                    uint32_t original_size;
                    err = cc_sizeof_as_uint(p, operand->lhs->type, operand->loc, &original_size);
                    if(err) return err;
                    if(original_size > from_sz || size > original_size)
                        break;
                    operand = operand->lhs;
                    from = operand->type;
                    from_sz = original_size;
                    from_addr = 0;
                }
                if(from_sz == size && !cc_expr_has_runtime_check(e))
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
            err = ci_lower_expr_dest(ci, ctx, e, &dest, size);
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
                    .src_is_brian_float = ccqt_bt_eq(from, CCBT_bfloat16),
                    .src_float = ci_float_width(from, ctx->ldbl_fmt),
                    .dst_is_brian_float = ccqt_bt_eq(to, CCBT_bfloat16),
                    .dst_float = ci_float_width(to, ctx->ldbl_fmt),
                    .is_unsigned = is_unsigned,
                    .checked_index = e->cast.checked_index,
                    .checked_bound = e->cast.checked_bound,
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
            if(ccqt_is_integer(e->type) && operand->kind == e->kind
                && operand->type.unqual == e->type.unqual
                && !operand->type.is_volatile && !operand->type.is_atomic
                && operand->lhs->type.unqual == e->type.unqual)
                return ci_lower_expr(ci, ctx, operand->lhs, dest, out);
            CiOpKind kind;
            _Bool is_not = e->kind == CC_EXPR_BITNOT;
            if(e->kind == CC_EXPR_NEG && ci_falu_type(e->type)){
                kind = ci_falu_kind(e->type, ctx->ldbl_fmt);
            }
            else if(ccqt_is_integer(e->type)){
                kind = ci_int_op_kind(size);
                if(size < 4 || !kind)
                    return ci_ice(ci, e->loc, "unsupported alu operand size %u", size);
            }
            else {
                return ci_unimplemented(ci, e->loc, "Unimplemented op");
            }
            uint32_t osz;
            err = cc_sizeof_as_uint(p, operand->type, operand->loc, &osz);
            if(err) return err;
            if(osz != size) return ci_ice(ci, e->loc, "Missing implicit cast? %u", osz);
            err = ci_lower_expr_dest(ci, ctx, e, &dest, size);
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
                        .is_brian_float = ccqt_bt_eq(e->type, CCBT_bfloat16),
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
            CcExpr* operand = e->lhs;
            _Bool negate = 1;
            while(operand->kind == CC_EXPR_LOGNOT && !operand->type.is_volatile && !operand->type.is_atomic){
                negate = !negate;
                operand = operand->lhs;
            }
            if(!negate && ci_expr_is_boolean(operand) && operand->type.unqual == e->type.unqual)
                return ci_lower_expr(ci, ctx, operand, dest, out);
            err = ci_lower_expr_dest(ci, ctx, e, &dest, size);
            if(err) return err;
            out->slot = dest;
            uint32_t temp = ctx->temp;
            CiLowerVal v;
            err = ci_lower_expr(ci, ctx, operand, CI_NO_SLOT, &v);
            if(err) return err;
            err = ci_lower_istrue(ctx, &v, operand->type, dest, size, negate, e->loc);
            if(err) return err;
            ctx->temp = temp;
            out->canonical = 1;
            return 0;
        }
        case CC_EXPR_ASSIGN:
            return ci_lower_assign(ci, ctx, e, dest, out, size);
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
            return ci_lower_compound(ci, ctx, e, dest, out);
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
                CcQualType pointee = ccqt_as_ptr(pe->type)->pointee;
                if(lhs_ptr && rhs_ptr){
                    // Empty pointees retain the byte difference, as do byte-sized ones.
                    uint32_t constant_size;
                    _Bool byte_difference = !cc_type_sizeof_complete(ci_target(ci), pointee, &constant_size, NULL)
                        && constant_size <= 1;
                    err = ci_lower_expr_dest(ci, ctx, e, &dest, size);
                    if(err) return err;
                    out->slot = dest;
                    uint32_t temp = ctx->temp;
                    CiLowerVal l, r;
                    err = ci_lower_expr(ci, ctx, lhs, CI_NO_SLOT, &l);
                    if(err) return err;
                    err = ci_lower_expr(ci, ctx, rhs, CI_NO_SLOT, &r);
                    if(err) return err;
                    uint32_t diff = dest;
                    if(!byte_difference){
                        err = ci_alloc_slot(ctx, ctx->ptr_size, ctx->ptr_size, &diff);
                        if(err) return err;
                    }
                    CiOp* op;
                    err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
                    if(err) return err;
                    *op = (CiOp){
                        .alu = {
                            .kind = ci_int_op_kind(ctx->ptr_size),
                            .slot = diff,
                            .src = l.slot,
                            .src2 = r.slot,
                            .op = CI_ALU_SUB,
                            .is_unsigned = 1,
                            .loc = e->loc,
                        }
                    };
                    if(!byte_difference){
                        CiLowerVal stride;
                        err = ci_lower_sizeof(ci, ctx, pointee, e->loc, &stride);
                        if(err) return err;
                        uint32_t esz = stride.slot;
                        err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
                        if(err) return err;
                        *op = (CiOp){
                            .alu = {
                                .kind = ci_int_op_kind(ctx->ptr_size),
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
                err = ci_lower_expr_dest(ci, ctx, e, &dest, size);
                if(err) return err;
                out->slot = dest;
                uint32_t temp = ctx->temp;
                if(ctx->ptr_size == 4 || ctx->ptr_size == 8){
                    CcExpr* base;
                    uint64_t offset;
                    err = ci_pointer_offset(ci, ctx, e, 0, UINT64_MAX, &base, &offset);
                    if(err) return err;
                    if(base != e){
                        CiLowerVal ptr;
                        err = ci_lower_expr(ci, ctx, base, CI_NO_SLOT, &ptr);
                        if(err) return err;
                        CiOp* op;
                        err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
                        if(err) return err;
                        *op = (CiOp){
                            .alu_imm = {
                                .kind = ctx->ptr_size == 4 ? CI_OP_ALU_IMM32 : CI_OP_ALU_IMM64,
                                .op = CI_ALU_ADD,
                                .is_unsigned = 1,
                                .slot = dest,
                                .src = ptr.slot,
                                .immediate = offset,
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
                err = ci_scale_pointer_index(ci, ctx, pointee, e->loc, iv->slot, &iv->slot);
                if(err) return err;
                idx_unsigned = 1;
                CiOp* op;
                err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
                if(err) return err;
                *op = (CiOp){
                    .alu = {
                        .kind = ci_int_op_kind(ctx->ptr_size),
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
                if(e->kind == CC_EXPR_MUL && e->binary.checked_size)
                    return ci_lower_size_product(ci, ctx, e, dest, out);
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
                CiFoldValue fold;
                int fold_fail = -1;
                _Bool additive = e->kind == CC_EXPR_ADD || e->kind == CC_EXPR_SUB;
                _Bool associative = e->kind == CC_EXPR_MUL
                    || e->kind == CC_EXPR_BITAND
                    || e->kind == CC_EXPR_BITOR
                    || e->kind == CC_EXPR_BITXOR;
                _Bool commutative = e->kind == CC_EXPR_ADD || associative;
                if((size == 4 || size == 8) && (rsz == size || is_shift)){
                    fold_fail = ci_fold_expr(ci, ctx, rhs, &fold);
                    if(fold_fail > 0) return fold_fail;
                    if(!fold_fail && fold.sz > 8) fold_fail = FOLD_FAIL;
                    if(fold_fail == FOLD_FAIL && commutative){
                        fold_fail = ci_fold_expr(ci, ctx, lhs, &fold);
                        if(fold_fail > 0) return fold_fail;
                        if(!fold_fail){
                            CcExpr* swap = lhs;
                            lhs = rhs;
                            rhs = swap;
                        }
                    }
                }
                if(!fold_fail && (additive || associative) && lhs->type.unqual == e->type.unqual && rhs->type.unqual == e->type.unqual){
                    uint64_t immediate = ci_read_uint(fold.bits, size);
                    while((lhs->kind == e->kind || (additive && (lhs->kind == CC_EXPR_ADD || lhs->kind == CC_EXPR_SUB)))
                        && !cc_expr_has_runtime_check(lhs)
                        && !lhs->type.is_volatile && !lhs->type.is_atomic
                        && lhs->lhs->type.unqual == e->type.unqual
                        && lhs->values[0]->type.unqual == e->type.unqual){

                        CiFoldValue addend;
                        CcExpr* base = lhs->lhs;
                        int status = ci_fold_expr(ci, ctx, lhs->values[0], &addend);
                        if(status > 0) return status;
                        if(status == FOLD_FAIL && lhs->kind != CC_EXPR_SUB){
                            status = ci_fold_expr(ci, ctx, lhs->lhs, &addend);
                            if(status > 0) return status;
                            if(!status) base = lhs->values[0];
                        }
                        if(status == FOLD_FAIL) break;
                        uint64_t value = ci_read_uint(addend.bits, size);
                        switch((uint32_t)e->kind){
                            case CC_EXPR_ADD: case CC_EXPR_SUB:
                                if(lhs->kind == e->kind) immediate += value;
                                else immediate -= value;
                                break;
                            case CC_EXPR_MUL: immediate *= value; break;
                            case CC_EXPR_BITAND: immediate &= value; break;
                            case CC_EXPR_BITOR: immediate |= value; break;
                            case CC_EXPR_BITXOR: immediate ^= value; break;
                        }
                        lhs = base;
                    }
                    ci_write_uint(fold.bits, size, immediate);
                }
                CiAluOp alu_op = ci_alu_op_for(e->kind);
                uint64_t immediate = 0;
                if(!fold_fail){
                    immediate = ci_read_uint(fold.bits, fold.sz);
                    CiImmediateResult simplified = ci_simplify_integer_immediate(&alu_op, &immediate, size,
                        ccqt_is_unsigned(lhs->type, ctx->char_is_unsigned));
                    if(simplified == CI_IMMEDIATE_IDENTITY && lhs->type.unqual == e->type.unqual)
                        return ci_lower_expr(ci, ctx, lhs, dest, out);
                    if(simplified == CI_IMMEDIATE_CONSTANT){
                        err = ci_lower_expr_discard(ci, ctx, lhs);
                        if(err) return err;
                        CiFoldValue value = {.sz=size, .type=e->type, .bits={immediate}};
                        return ci_lower_constant(ci, ctx, e, &value, dest, out);
                    }
                }
                err = ci_lower_expr_dest(ci, ctx, e, &dest, size);
                if(err) return err;
                out->slot = dest;
                uint32_t temp = ctx->temp;
                CiLowerVal l, r;
                err = ci_lower_expr(ci, ctx, lhs, CI_NO_SLOT, &l);
                if(err) return err;
                if(fold_fail == 0){
                    CiOp* op;
                    err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
                    if(err) return err;
                    *op = (CiOp){
                        .alu_imm = {
                            .kind = size == 4 ? CI_OP_ALU_IMM32 : CI_OP_ALU_IMM64,
                            .op = alu_op,
                            .is_unsigned = ccqt_is_unsigned(lhs->type, ctx->char_is_unsigned),
                            .slot = dest,
                            .src = l.slot,
                            .loc = e->loc,
                        }
                    };
                    op->alu_imm.immediate = immediate;
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
            if(ci_falu_type(lhs->type) && ci_falu_type(rhs->type) && ccqt_basic_kind(lhs->type) == ccqt_basic_kind(rhs->type)){
                CiFaluOp fop;
                if(!ci_falu_op_for(e->kind, &fop))
                    return ci_ice(ci, e->loc, "float-only invalid binary operator reached lowering%s", "");
                CiOpKind kind = ci_falu_kind(lhs->type, ctx->ldbl_fmt);
                err = ci_lower_expr_dest(ci, ctx, e, &dest, size);
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
                        .is_brian_float = ccqt_bt_eq(e->type, CCBT_bfloat16),
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
            if(ccqt_is_basic(lhs->type) && ccbt_is_arithmetic(ccqt_basic_kind(lhs->type)) && ccqt_is_basic(rhs->type) && ccbt_is_arithmetic(ccqt_basic_kind(rhs->type))){
                if(ccqt_basic_kind(lhs->type) != ccqt_basic_kind(rhs->type))
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
            CcExpr* evaluated;
            _Bool limit_truth;
            int limit = ci_compare_type_limit(ci, ctx, e, &evaluated, &limit_truth);
            if(limit > 0) return limit;
            if(!limit){
                err = ci_lower_expr_discard(ci, ctx, evaluated);
                if(err) return err;
                CiFoldValue value = {.sz=size, .type=e->type, .bits={limit_truth}};
                return ci_lower_constant(ci, ctx, e, &value, dest, out);
            }
            uint32_t requested_dest = dest;
            _Bool lhs_ptr = ccqt_has_pointer_representation(lhs->type);
            _Bool rhs_ptr = ccqt_has_pointer_representation(rhs->type);
            CiOpKind kind;
            CiCmpOp cmp_op = ci_cmp_op_for(e->kind);
            _Bool is_unsigned = 0;
            uint32_t op_size;
            _Bool is_float = 0;
            if(ci_falu_type(lhs->type) && ci_falu_type(rhs->type) && ccqt_basic_kind(lhs->type) == ccqt_basic_kind(rhs->type)){
                is_float = 1;
                err = cc_sizeof_as_uint(p, lhs->type, lhs->loc, &op_size);
                if(err) return err;
                uint32_t width = ci_float_width(lhs->type, ctx->ldbl_fmt);
                kind = width == 16 ? CI_OP_FCMP16 : width == 32 ? CI_OP_FCMP32 : width == 64 ? CI_OP_FCMP64 : width == 80 ? CI_OP_FCMP80 : CI_OP_FCMP128;
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
            else if(ccqt_is_basic(lhs->type) && ccbt_is_arithmetic(ccqt_basic_kind(lhs->type))
                 && ccqt_is_basic(rhs->type) && ccbt_is_arithmetic(ccqt_basic_kind(rhs->type))){
                if(ccqt_basic_kind(lhs->type) != ccqt_basic_kind(rhs->type))
                    return ci_ice(ci, e->loc, "comparison operands were not converted to a common type%s", "");
                return ci_unimplemented(ci, e->loc, "unsupported arithmetic comparison op");
            }
            else {
                return ci_ice(ci, e->loc, "illegal comparison expression reached lowering%s", "");
            }
            err = ci_lower_expr_dest(ci, ctx, e, &dest, size);
            if(err) return err;
            out->slot = dest;
            out->canonical = 1;
            uint32_t temp = ctx->temp;
            CiLowerVal l, r;
            err = ci_lower_expr(ci, ctx, lhs, CI_NO_SLOT, &l);
            if(err) return err;
            if(l.canonical && ccqt_is_integer(lhs->type) && ccqt_is_integer(rhs->type) && (e->kind == CC_EXPR_EQ || e->kind == CC_EXPR_NE)){
                CiFoldValue constant;
                int status = ci_fold_expr(ci, ctx, rhs, &constant);
                if(status > 0) return status;
                if(!status && !constant.bits[1] && constant.bits[0] <= 1){
                    _Bool negate = (e->kind == CC_EXPR_EQ) != (_Bool)constant.bits[0];
                    if(!negate && l.size == size && requested_dest == CI_NO_SLOT){
                        *out = l;
                        return 0;
                    }
                    err = ci_lower_istrue(ctx, &l, lhs->type, dest, size, negate, e->loc);
                    ctx->temp = temp;
                    return err;
                }
            }
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
                        .is_brian_float = ccqt_bt_eq(lhs->type, CCBT_bfloat16),
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
            _Bool truth;
            CcExpr* operand = NULL;
            int folded = ci_fold_condition(ci, ctx, e->lhs, &truth);
            if(folded > 0) return folded;
            if(!folded){
                if(truth == (e->kind == CC_EXPR_LOGOR)){
                    CiFoldValue value = {.sz=size, .type=e->type, .bits={truth}};
                    return ci_lower_constant(ci, ctx, e, &value, dest, out);
                }
                operand = e->values[0];
            }
            else {
                folded = ci_fold_condition(ci, ctx, e->values[0], &truth);
                if(folded > 0) return folded;
                if(!folded){
                    if(truth == (e->kind == CC_EXPR_LOGOR)){
                        err = ci_lower_expr_discard(ci, ctx, e->lhs);
                        if(err) return err;
                        CiFoldValue value = {.sz=size, .type=e->type, .bits={truth}};
                        return ci_lower_constant(ci, ctx, e, &value, dest, out);
                    }
                    operand = e->lhs;
                }
            }
            if(operand){
                if(ci_expr_is_boolean(operand) && operand->type.unqual == e->type.unqual)
                    return ci_lower_expr(ci, ctx, operand, dest, out);
                err = ci_lower_expr_dest(ci, ctx, e, &dest, size);
                if(err) return err;
                uint32_t temp = ctx->temp;
                CiLowerVal value;
                err = ci_lower_expr(ci, ctx, operand, CI_NO_SLOT, &value);
                if(err) return err;
                err = ci_lower_istrue(ctx, &value, operand->type, dest, size, 0, e->loc);
                ctx->temp = temp;
                *out = (CiLowerVal){.slot=dest, .size=size, .canonical=1};
                return err;
            }
            uint32_t work;
            if(dest == CI_NO_SLOT){
                err = ci_lower_expr_dest(ci, ctx, e, &dest, size);
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
                err = ci_emit_copy(ctx, dest, work, size, e->loc);
                if(err) return err;
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
            if(ccqt_is_integer(e->type)){
                CiFoldValue yes, no;
                int a = ci_fold_expr(ci, ctx, e->values[0], &yes);
                if(a > 0) return a;
                int b = ci_fold_expr(ci, ctx, e->values[1], &no);
                if(b > 0) return b;
                if(!a && !b && yes.sz == no.sz && !memcmp(yes.bits, no.bits, yes.sz)){
                    err = ci_lower_expr_discard(ci, ctx, e->lhs);
                    if(err) return err;
                    return ci_lower_constant(ci, ctx, e, &yes, dest, out);
                }
                if(!a && !b && !yes.bits[1] && !no.bits[1] && yes.bits[0] <= 1 && no.bits[0] <= 1 && yes.bits[0] != no.bits[0]){
                    _Bool negate = !yes.bits[0];
                    if(!negate && ci_expr_is_boolean(e->lhs) && e->lhs->type.unqual == e->type.unqual)
                        return ci_lower_expr(ci, ctx, e->lhs, dest, out);
                    err = ci_lower_expr_dest(ci, ctx, e, &dest, size);
                    if(err) return err;
                    uint32_t temp = ctx->temp;
                    CiLowerVal condition;
                    err = ci_lower_expr(ci, ctx, e->lhs, CI_NO_SLOT, &condition);
                    if(err) return err;
                    err = ci_lower_istrue(ctx, &condition, e->lhs->type, dest, size, negate, e->loc);
                    ctx->temp = temp;
                    *out = (CiLowerVal){.slot=dest, .size=size, .canonical=1};
                    return err;
                }
            }
            // cond; if(!cond) goto else; dest = then; goto end; else: dest = else; end:
            err = ci_lower_expr_dest(ci, ctx, e, &dest, size);
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
            // The result must outlive the statement expression's locals.
            err = ci_lower_expr_dest(ci, ctx, e, &dest, size);
            if(err) return err;
            out->slot = dest;
            uint32_t temp = ctx->temp;
            // A caller-supplied destination can be an enclosing variable.
            // Preserve the expression value privately until cleanup finishes;
            // the enclosing assignment must take effect after that cleanup.
            uint32_t result = dest;
            if(size && body->cleanup.first != body->cleanup.stop){
                err = ci_alloc_slot(ctx, size, 16, &result);
                if(err) return err;
            }
            err = ci_layout_decls(ci, ctx, body);
            if(err) goto statement_expression_end;
            uint32_t count = body->count;
            CcExpr*_Nullable value = NULL;
            if(count && body->stmts[count-1]->kind == CC_STMT_EXPR
                && !ccqt_bt_eq(e->type, CCBT_void)){
                value = body->stmts[count-1]->exprs[0];
                count--;
            }
            for(uint32_t i = 0; i < count; i++){
                err = ci_lower_stmt(ci, ctx, body->stmts[i]);
                if(err) goto statement_expression_end;
            }
            if(value){
                err = ci_lower_expr(ci, ctx, (CcExpr*_Nonnull)value, result, out);
                if(err) goto statement_expression_end;
            }
            err = ci_lower_cleanups(ci, ctx, body->cleanup);
            if(err) goto statement_expression_end;
            if(value && result != dest)
                err = ci_emit_copy(ctx, dest, result, size, e->loc);
            statement_expression_end:
            out->slot = dest;
            ctx->temp = temp;
            return err;
        }
        case CC_EXPR_ATOMIC:
            return ci_lower_atomic_builtin(ci, ctx, e, dest, out);
        case CC_EXPR_OBJECT_VIEW:
            return ci_lower_expr(ci, ctx, e->lhs, dest, out);
        case CC_EXPR_COMPOUND_LITERAL:
        case CC_EXPR_INIT_LIST:{
            // Parser ensures that we are either initializing or going through an anonymous
            // compound literal, so it is safe to build in-place.
            err = ci_lower_expr_dest(ci, ctx, e, &dest, size);
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
            // dest can be a packed member supplied by the caller. Infer its
            // alignment from the frame offset, not the initializer's type.
            err = ci_lower_init_list(ci, ctx, e, (CiLowerAddr){.slot = aslot, .align_log2 = ci_frame_alignment(ctx, dest)}, 1);
            if(err) return err;
            ctx->temp = temp;
            return 0;
        }
        case CC_EXPR_SIZEOF_VMT:
            return ci_unimplemented(ci, e->loc, "sizeof vmt");
        case CC_EXPR_VA:
            return ci_lower_va(ci, ctx, e, dest, out);
        case CC_EXPR_ADD_OVERFLOW:
        case CC_EXPR_MUL_OVERFLOW:
        case CC_EXPR_SUB_OVERFLOW:
            return ci_lower_checked(ci, ctx, e, dest, out);
        case CC_EXPR_BIT_BUILTIN:
            return ci_lower_bit_builtin(ci, ctx, e, dest, out);
        case CC_EXPR_BSWAP:{
            CcExpr* arg = e->lhs;
            err = ci_lower_expr_dest(ci, ctx, e, &dest, size);
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
            if(ctx->has_vla)
                return ci_error(ci, e->loc, "cannot mix explicit alloca and VLA allocations in one function");
            ctx->has_alloca = 1;
            CcExpr *sz = e->lhs;
            err = ci_lower_expr_dest(ci, ctx, e, &dest, size);
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
                    .zero_initialize = 1,
                    .alignment = 16,
                    .src = l.slot,
                    .slot = dest,
                    .loc = e->loc,
                },
            };
            ctx->temp = temp;
            return 0;
        }
        case CC_EXPR_BUILTIN:{
            if(e->builtin.op == CC_BUILTIN_READCYCLECOUNTER){
                err = ci_lower_expr_dest(ci, ctx, e, &dest, size);
                if(err) return err;
                out->slot = dest;
            }
            if(e->builtin.op == CC_BUILTIN_CLEAR_CACHE)
                return ci_lower_clear_cache(ci, ctx, e);
            if(e->builtin.op == CC_BUILTIN_CAPTURE_BACKTRACE || e->builtin.op == CC_BUILTIN_SYMBOLICATE)
                return ci_lower_rt_call(ci, ctx, e, e->builtin.op == CC_BUILTIN_CAPTURE_BACKTRACE ? CI_RT_CAPTURE_BACKTRACE : CI_RT_SYMBOLICATE, dest, out);
            CiOp* op;
            err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
            if(err) return err;
            *op = (CiOp){
                .builtin = {
                    .kind = CI_OP_BUILTIN,
                    .op = e->builtin.op,
                    .slot = dest,
                    .loc = e->loc,
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
        case CC_EXPR_SRCLOC_REFLECT:
            return ci_lower_reflect(ci, ctx, e, dest, out);
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
    if(ccqt_is_fixed_sequence(e->type) || ccqt_kind(e->type) == CC_VLA){
        CiLowerAddr a;
        int err = ci_lower_addr(ci, ctx, e, 1, &a);
        if(err) return err;
        return ci_addr_to_value(ctx, a, dest, ctx->ptr_size, ci_target(ci)->alignof_[CCBT_nullptr_t], e->loc, out);
    }
    if(ccqt_kind(e->type) == CC_FUNCTION){
        if(e->kind == CC_EXPR_DEREF)
            return ci_lower_expr(ci, ctx, e->lhs, dest, out);
        if(e->kind != CC_EXPR_FUNCTION)
            return ci_lower_expr(ci, ctx, e, dest, out);
        int err = ci_lower_func_dep(ctx, e->func, (CcFuncDepFlags)(CC_FUNC_DEP_USED | CC_FUNC_DEP_ADDR_TAKEN));
        if(err) return err;
        err = ci_lower_expr_dest(ci, ctx, e, &dest, ctx->ptr_size);
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
    // Metadata aggregates may need runtime storage (for example when passed
    // by value), but their construction still need not call the runtime.
    if(e->kind == CC_EXPR_TYPE_INTROSPECTION || e->kind == CC_EXPR_SRCLOC_REFLECT){
        int err = ci_fold_read_inputs(ci, ctx, e, 0);
        if(err == CI_OOM_ERROR) return err;
        if(!err){
            CcExpr* value = NULL;
            err = cc_eval_linktime_expr(&ci->parser, e, &value);
            if(!err){
                // The evaluator owns this temporary tree and may reuse its addresses.
                CiFoldCache* cache = ctx->fold_cache;
                ctx->fold_cache = NULL;
                if(out) err = ci_lower_expr(ci, ctx, value, dest, (CiLowerVal*)out);
                ctx->fold_cache = cache;
                cc_release_expr(&ci->parser, value);
                return err;
            }
            if(err == CI_OOM_ERROR) return err;
        }
    }
    _Bool module = e->kind == CC_EXPR_MODULE_REFLECT;
    _Bool srcloc = e->kind == CC_EXPR_SRCLOC_REFLECT;
    uint32_t subop = srcloc ? (uint32_t)e->srcloc.op : module ? (uint32_t)e->module.op : (uint32_t)e->type_introspection.op;
    if(module && !out && subop != CC_MODULE_RUN) return 0;
    int nargs = 1;
    if(!srcloc && (module ?
        (  subop == CC_MODULE_FUNC
        || subop == CC_MODULE_FUNC_INFO
        || subop == CC_MODULE_VAR
        || subop == CC_MODULE_TYPE
        || subop == CC_MODULE_SYMBOL
        || subop == CC_MODULE_PARSE_TYPE)
        : (subop == CC_TYPE_IS_CALLABLE_WITH
        || subop == CC_TYPE_IS_CALLABLE_THROUGH
        || subop == CC_TYPE_CASTABLE_TO
        || subop == CC_TYPE_FIELD
        || subop == CC_TYPE_METHOD
        || subop == CC_TYPE_HAS_FIELD
        || subop == CC_TYPE_HAS_METHOD
        || subop == CC_TYPE_ENUMERATOR
        || subop == CC_TYPE_PARAM_TYPE
        || subop == CC_TYPE_MAKE_ANY)))
        nargs++;
    int err;
    if(out){
        err = ci_lower_expr_dest(ci, ctx, e, &dest, out->size);
        if(err) return err;
        out->slot = dest;
    }
    uint32_t temp = ctx->temp;
    uint32_t result = dest;
    if(out){
        uint32_t align;
        err = cc_alignof_as_uint(&ci->parser, e->type, e->loc, &align);
        if(err) return err;
        // Runtime reflection writes typed objects; a packed caller destination
        // does not meet that interface's alignment requirement.
        if(dest & (align - 1)){
            err = ci_alloc_slot(ctx, out->size, align, &result);
            if(err) return err;
        }
    }
    CiOp call = {.rt_call = {
        .kind = CI_OP_RT_CALL,
        .op = srcloc ? CI_RT_SRCLOC_REFLECT : module ? CI_RT_MODULE_REFLECT : CI_RT_TYPE_REFLECT,
        .nargs = nargs,
        .slot = result,
        .slot_size = out ? out->size : 0,
        .reflect_op = subop,
        .member_by_name = (module ? subop == CC_MODULE_SYMBOL || subop == CC_MODULE_PARSE_TYPE :
            !srcloc && (subop == CC_TYPE_FIELD || subop == CC_TYPE_METHOD || subop == CC_TYPE_HAS_FIELD || subop == CC_TYPE_HAS_METHOD))
            && ccqt_kind(e->values[0]->type) == CC_SLICE,
        .loc = e->loc,
    }};
    for(uint32_t i = 0; i < call.rt_call.nargs; i++){
        _Bool type_list = i && !module && !srcloc && subop == CC_TYPE_IS_CALLABLE_WITH;
        uint32_t arg_size = i && (call.rt_call.member_by_name || type_list) ? sizeof(CiRtSlice) : 8;
        err = ci_alloc_slot(ctx, arg_size, 8, &call.rt_call.args[i]);
        if(err) return err;
        CiLowerVal v;
        CcExpr* argument = i ? e->values[0] : e->lhs;
        err = ci_lower_expr(ci, ctx, argument, call.rt_call.args[i], &v);
        if(err) return err;
        if(!srcloc && i == 0 && (module || subop == CC_TYPE_FIELD || subop == CC_TYPE_METHOD || subop == CC_TYPE_ENUMERATOR || subop == CC_TYPE_PARAM_TYPE)){
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
    if(module && subop == CC_MODULE_SYMBOL && ccqt_kind(e->type) == CC_POINTER){
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
    if(out && result != dest){
        err = ci_emit_copy(ctx, dest, result, out->size, e->loc);
        if(err) return err;
    }
    ctx->temp = temp;
    return 0;
}

static
int
ci_lower_clear_cache(CiInterpreter* ci, CiLowerCtx* ctx, CcExpr* e){
    if(ci_target(ci)->arch == CC_ARCH_x86_64){
        // No cache maintenance on x86, but both operands must still be evaluated.
        int err = ci_lower_expr_discard(ci, ctx, e->values[0]);
        if(err) return err;
        return ci_lower_expr_discard(ci, ctx, e->values[1]);
    }
    uint32_t temp = ctx->temp;
    CiLowerVal begin, end;
    int err = ci_lower_expr(ci, ctx, e->values[0], CI_NO_SLOT, &begin);
    if(err) return err;
    err = ci_lower_expr(ci, ctx, e->values[1], CI_NO_SLOT, &end);
    if(err) return err;
    CiOp* op;
    err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
    if(err) return err;
    *op = (CiOp){
        .clear_cache = {
            .kind = CI_OP_CLEAR_CACHE,
            .begin = begin.slot,
            .end = end.slot,
            .loc = e->loc,
        },
    };
    ctx->temp = temp;
    return 0;
}

static
int
ci_lower_rt_call(CiInterpreter* ci, CiLowerCtx* ctx, CcExpr* e, CiRuntimeOp rt_op, uint32_t dest, CiLowerVal*_Nullable out){
    _Bool trace = rt_op == CI_RT_CAPTURE_BACKTRACE || rt_op == CI_RT_SYMBOLICATE;
    uint32_t nargs = (trace || rt_op == CI_RT_HOTSWAP || rt_op == CI_RT_COMPILE) ? 2 : 1;
    CcExpr* args[2] = {e->lhs, NULL};
    if(nargs == 2)
        args[1] = e->values[0];
    if(trace){
        args[0] = e->values[0];
        args[1] = e->values[1];
    }
    uint32_t result_size = 0;
    if(out){
        int err = ci_lower_expr_dest(ci, ctx, e, &dest, out->size);
        if(err) return err;
        out->slot = dest;
        result_size = out->size;
    }
    uint32_t temp = ctx->temp;
    uint32_t slots[3] = {0};
    for(uint32_t i = 0; i < nargs; i++){
        CiLowerVal v;
        int err = ci_lower_expr(ci, ctx, args[i], CI_NO_SLOT, &v);
        if(err) return err;
        slots[i] = v.slot;
    }
    if(rt_op == CI_RT_COMPILE){
        int err = ci_emit_const(ci, ctx, (uintptr_t)e->compile.module, sizeof(void*), e->loc, &slots[2]);
        if(err) return err;
        nargs = 3;
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
    _Bool is_pointer = ccqt_kind(e->type) == CC_POINTER;
    uint64_t step = 0, step_hi = 0;
    uint32_t step_bt = (uint32_t)ci_target(ci)->size_type;
    uint32_t step_immsize = size;
    _Bool is_float = 0;
    if(ccqt_is_integer(e->type)){
        step = 1;
    }
    else if(ci_falu_type(e->type)){
        is_float = 1;
        step_bt = (uint32_t)ccqt_basic_kind(e->type);
        step_immsize = size;
        if(size == 2){
            step = ccqt_bt_eq(e->type, CCBT_bfloat16) ? 0x3f80 : 0x3c00;
        }
        else if(size == 4){
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
            if(ci_float_width(e->type, ctx->ldbl_fmt) == 80){
                step = UINT64_C(0x8000000000000000); step_hi = 0x3fff;
            }
            else { step = 0; step_hi = UINT64_C(0x3fff000000000000); }
        }
    }
    else if(!is_pointer){
        return ci_unimplemented(ci, e->loc, "incdec on unsupported type");
    }
    _Bool is_pre = e->kind == CC_EXPR_PREINC || e->kind == CC_EXPR_PREDEC;
    _Bool is_inc = e->kind == CC_EXPR_PREINC || e->kind == CC_EXPR_POSTINC;
    _Bool is_bool = ccqt_is_bool(e->type);
    CiOpKind alukind;
    CiFaluOp fop = 0;
    if(is_float){
        alukind = ci_falu_kind(e->type, ctx->ldbl_fmt);
        fop = is_inc? CI_FALU_ADD : CI_FALU_SUB;
    }
    else {
        alukind = ci_int_op_kind(size);
        if(!alukind) return ci_ice(ci, e->loc, "unsupported integer ALU size %u", size);
    }
    CiOp* op;
    uint32_t vslot;
    if(ci_frame_lvalue(ci, ctx, lhs, &vslot)){
        if(out && !is_pre){
            err = ci_lower_expr_dest(ci, ctx, e, &dest, size);
            if(err) return err;
            err = ci_emit_copy(ctx, dest, vslot, size, e->loc);
            if(err) return err;
            out->slot = dest;
        }
        uint32_t temp = ctx->temp;
        if(!is_pointer && !is_float && (size == 4 || size == 8)){
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
            if(is_pointer){
                CiLowerVal stride;
                err = ci_lower_sizeof(ci, ctx, ccqt_as_ptr(e->type)->pointee, e->loc, &stride);
                if(err) return err;
                err = ci_emit_copy(ctx, sslot, stride.slot, size, e->loc);
                if(err) return err;
            }
            else {
            err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
            if(err) return err;
            *op = (CiOp){
                .constant = {
                    .kind = CI_OP_CONST,
                    .bt_kind = step_bt,
                    .slot = sslot,
                    .immsize = step_immsize,
                    .immediate = {step, step_hi},
                    .loc = e->loc,
                }
            };
            }
            err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
            if(err) return err;
            if(is_float){
                *op = (CiOp){
                    .falu32 = {
                        .is_brian_float = ccqt_bt_eq(e->type, CCBT_bfloat16),
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
        if(is_bool){
            CiLowerVal v = {.slot = vslot, .size = size};
            err = ci_lower_istrue(ctx, &v, ccqt_basic(CCBT_unsigned_char), vslot, size, 0, e->loc);
            if(err) return err;
        }
        ctx->temp = temp;
        if(out && is_pre){
            err = ci_lower_expr_dest(ci, ctx, e, &dest, size);
            if(err) return err;
            err = ci_emit_copy(ctx, dest, vslot, size, e->loc);
            if(err) return err;
            out->slot = dest;
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
        err = ci_lower_addr(ci, ctx, lhs, 0, &a);
        if(err) return err;
        if(is_pointer){
            CiLowerVal stride;
            err = ci_lower_sizeof(ci, ctx, ccqt_as_ptr(e->type)->pointee, e->loc, &stride);
            if(err) return err;
            err = ci_emit_copy(ctx, sslot, stride.slot, size, e->loc);
            if(err) return err;
        }
        else {
        err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
        if(err) return err;
        *op = (CiOp){
            .constant = {
                .kind = CI_OP_CONST,
                .bt_kind = step_bt,
                .slot = sslot,
                .immsize = size,
                .immediate = {step, step_hi},
                .loc = e->loc,
            }
        };
        }
        uint32_t result;
        if(is_float || size > 8 || is_bool){
            uint32_t newv, ok;
            err = ci_alloc_slot(ctx, size, size, &newv);
            if(err) return err;
            err = ci_alloc_slot(ctx, 1, 1, &ok);
            if(err) return err;
            err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
            if(err) return err;
            *op = (CiOp){
                .atomic_load = {
                    .is_volatile = lhs->type.is_volatile,
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
                        .is_brian_float = ccqt_bt_eq(e->type, CCBT_bfloat16),
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
            if(is_bool){
                CiLowerVal v = {.slot = newv, .size = size};
                err = ci_lower_istrue(ctx, &v, ccqt_basic(CCBT_unsigned_char), newv, size, 0, e->loc);
                if(err) return err;
            }
            err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
            if(err) return err;
            *op = (CiOp){
                .atomic_cas = {
                    .is_volatile = lhs->type.is_volatile,
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
            err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
            if(err) return err;
            *op = (CiOp){
                .atomic_rmw = {
                    .is_volatile = lhs->type.is_volatile,
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
        err = ci_emit_copy(ctx, dest, result, size, e->loc);
        if(err) return err;
        return 0;
    }
    uint32_t old;
    err = ci_alloc_slot(ctx, size, size, &old);
    if(err) return err;
    CiLowerAddr a;
    _Bool is_bf = cc_expr_field_bit_width(lhs) != 0;
    if(is_bf)
        err = ci_lower_bitfield_addr(ci, ctx, lhs, &a);
    else
        err = ci_lower_addr(ci, ctx, lhs, 0, &a);
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
                .is_volatile = lhs->type.is_volatile,
                .kind = CI_OP_LOAD,
                .slot = old,
                .slot_size = size,
                .src = a.slot,
                .offset = a.disp,
                .align_log2 = a.align_log2,
                .loc = e->loc,
            }
        };
    }
    uint32_t newv;
    err = ci_alloc_slot(ctx, size, size, &newv);
    if(err) return err;
    _Bool immediate=!is_pointer && !is_float && (size==4 || size==8);
    uint32_t sslot = 0;
    if(!immediate){
    err = ci_alloc_slot(ctx, step_immsize, step_immsize, &sslot);
    if(err) return err;
    if(is_pointer){
        CiLowerVal stride;
        err = ci_lower_sizeof(ci, ctx, ccqt_as_ptr(e->type)->pointee, e->loc, &stride);
        if(err) return err;
        err = ci_emit_copy(ctx, sslot, stride.slot, size, e->loc);
        if(err) return err;
    }
    else {
    err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
    if(err) return err;
    *op = (CiOp){
        .constant = {
            .kind = CI_OP_CONST,
            .bt_kind = step_bt,
            .slot = sslot,
            .immsize = step_immsize,
            .immediate = {step, step_hi},
            .loc = e->loc,
        }
    };
    }
    }
    err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
    if(err) return err;
    if(immediate){
        *op=(CiOp){.alu_imm={
            .kind = size == 4 ?CI_OP_ALU_IMM32 : CI_OP_ALU_IMM64,
            .slot = newv,
            .src = old,
            .op = is_inc? CI_ALU_ADD : CI_ALU_SUB,
            .is_unsigned = 1,
            .immediate = step,
            .loc = e->loc
        }};
    }
    else if(is_float){
        *op = (CiOp){
            .falu32 = {
                .is_brian_float = ccqt_bt_eq(e->type, CCBT_bfloat16),
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
    if(is_bool){
        CiLowerVal v = {.slot = newv, .size = size};
        err = ci_lower_istrue(ctx, &v, ccqt_basic(CCBT_unsigned_char), newv, size, 0, e->loc);
        if(err) return err;
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
                .is_volatile = lhs->type.is_volatile,
                .kind = CI_OP_STORE,
                .slot = a.slot,
                .src = newv,
                .src_size = size,
                .offset = a.disp,
                .align_log2 = a.align_log2,
                .loc = e->loc,
            }
        };
    }
    if(out){
        // pre yields the new value, post the old; both live in kept slots
        out->slot = is_pre? newv : old;
        if(dest != CI_NO_SLOT){
            err = ci_emit_copy(ctx, dest, out->slot, size, e->loc);
            if(err) return err;
            out->slot = dest;
        }
    }
    return 0;
}

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
    if(out){
        err = ci_lower_expr_dest(ci, ctx, e, &dest, 1);
        if(err) return err;
        out->slot = dest;
    }
    uint32_t save = ctx->temp;
    uint32_t ovf;
    err = ci_alloc_slot(ctx, 1, 1, &ovf);
    if(err) return err;
    CiLowerVal av, bv, pv;
    err = ci_lower_expr(ci, ctx, ae, CI_NO_SLOT, &av);
    if(err) return err;
    err = ci_lower_expr(ci, ctx, be, CI_NO_SLOT, &bv);
    if(err) return err;
    err = ci_lower_expr(ci, ctx, rese, CI_NO_SLOT, &pv); // the result pointer
    if(err) return err;
    uint32_t alignment;
    err = ci_pointer_alignment(ci, ctx, rese, &alignment);
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
            .is_volatile = dtype.is_volatile,
            .src = rtmp,
            .align_log2 = alignment,
            .src_size = dsz,
            .offset = 0,
            .loc = e->loc,
        }
    };
    if(out){
        err = ci_emit_copy(ctx, dest, ovf, 1, e->loc);
        if(err) return err;
    }
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
        err = ci_lower_expr_dest(ci, ctx, e, &dest, 8);
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
    uint32_t alignment;
    err = ci_pointer_alignment(ci, ctx, he, &alignment);
    if(err) return err;
    err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
    if(err) return err;
    *op = (CiOp){
        .store = {
            .kind = CI_OP_STORE,
            .slot = hv.slot,
            .is_volatile = ccqt_as_ptr(he->type)->pointee.is_volatile,
            .src = prod + 8,
            .align_log2 = alignment,
            .src_size = 8,
            .offset = 0,
            .loc = e->loc,
        }
    };
    if(out){
        // value = product[0:8], copied to a stable slot before the scratch is freed
        err = ci_emit_copy(ctx, dest, prod, 8, e->loc);
        if(err) return err;
    }
    ctx->temp = save;
    return 0;
}

static int
ci_lower_bit_builtin(CiInterpreter* ci, CiLowerCtx* ctx, CcExpr* e, uint32_t dest, CiLowerVal* out){
    uint32_t sz, rsz, ssz = 0;
    int err = cc_sizeof_as_uint(&ci->parser, e->lhs->type, e->loc, &sz);
    if(err) return err;
    err = cc_sizeof_as_uint(&ci->parser, e->type, e->loc, &rsz);
    if(err) return err;
    err = ci_lower_expr_dest(ci, ctx, e, &dest, rsz);
    if(err) return err;
    out->slot = dest;
    uint32_t save = ctx->temp;
    CiLowerVal a, b = {0};
    err = ci_lower_expr(ci, ctx, e->lhs, CI_NO_SLOT, &a);
    if(err) return err;
    uint32_t skip_fallback = 0, skip_count = 0;
    _Bool lazy = e->bit_builtin.nargs && (e->bit_builtin.op == CC_BIT_CLZ || e->bit_builtin.op == CC_BIT_CTZ);
    if(lazy){
        uint32_t truth;
        err = ci_alloc_slot(ctx, 1, 1, &truth);
        if(err) return err;
        err = ci_lower_istrue(ctx, &a, e->lhs->type, truth, 1, 0, e->loc);
        if(err) return err;
        CiOp* jump;
        err = ma_alloc(CiOp)(ctx->out, ctx->a, &jump);
        if(err) return err;
        skip_fallback = (uint32_t)ctx->out->count-1;
        *jump = (CiOp){.jump_true = {.kind = CI_OP_JUMP_TRUE, .slot = truth, .slot_size = 1, .loc = e->loc}};
        err = ci_lower_expr(ci, ctx, e->values[0], dest, &b);
        if(err) return err;
        err = ma_alloc(CiOp)(ctx->out, ctx->a, &jump);
        if(err) return err;
        skip_count = (uint32_t)ctx->out->count-1;
        *jump = (CiOp){.jump = {.kind = CI_OP_JUMP, .loc = e->loc}};
        ctx->out->data[skip_fallback].jump_true.jump = (uint32_t)ctx->out->count;
    }
    else if(e->bit_builtin.nargs){
        err = cc_sizeof_as_uint(&ci->parser, e->values[0]->type, e->loc, &ssz);
        if(err) return err;
        err = ci_lower_expr(ci, ctx, e->values[0], CI_NO_SLOT, &b);
        if(err) return err;
    }
    CiOp* op;
    err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
    if(err) return err;
    *op = (CiOp){.bit_builtin = {
        .kind = CI_OP_BIT_BUILTIN, .op = e->bit_builtin.op,
        .src_size = sz, .src2_size = ssz,
        .slot = dest, .slot_size = rsz, .src = a.slot, .src2 = b.slot,
        .loc = e->loc,
    }};
    if(lazy) ctx->out->data[skip_count].jump.jump = (uint32_t)ctx->out->count;
    ctx->temp = save;
    return 0;
}

static
_Bool
ci_memcpy_frame_addr(CiInterpreter* ci, CiLowerCtx* ctx, CcExpr* e, uint32_t* slot){
    // Pointer casts and array decay preserve the address of local storage.
    while(e->kind == CC_EXPR_CAST && ccqt_kind(e->type) == CC_POINTER && (ccqt_kind(e->lhs->type) == CC_POINTER || (ccqt_is_fixed_sequence(e->lhs->type) || ccqt_kind(e->lhs->type) == CC_VLA)))
        e = e->lhs;
    if(e->kind == CC_EXPR_ADDR)
        return ci_frame_lvalue(ci, ctx, e->lhs, slot);
    if((ccqt_is_fixed_sequence(e->type) || ccqt_kind(e->type) == CC_VLA))
        return ci_frame_lvalue(ci, ctx, e, slot);
    return 0;
}

static
int
ci_lower_memcpy(CiInterpreter* ci, CiLowerCtx* ctx, CcExpr* e, uint32_t dest, CiLowerVal*_Nullable out, uint32_t size){
    int err;
    if(out){
        err = ci_lower_expr_dest(ci, ctx, e, &dest, out->size);
        if(err) return err;
        out->slot = dest;
    }
    uint32_t temp = ctx->temp;
    uint32_t dst, src;
    _Bool dst_frame = ci_memcpy_frame_addr(ci, ctx, e->values[0], &dst);
    _Bool src_frame = ci_memcpy_frame_addr(ci, ctx, e->values[1], &src);
    uint32_t dst_alignment, src_alignment;
    err = ci_pointer_alignment(ci, ctx, e->values[0], &dst_alignment);
    if(err) return err;
    err = ci_pointer_alignment(ci, ctx, e->values[1], &src_alignment);
    if(err) return err;
    // Capture argument values in independent slots, as a normal call does:
    // evaluating the source may change the destination pointer variable.
    CiLowerVal v;
    if(!dst_frame){
        err = ci_alloc_slot(ctx, ctx->ptr_size, ctx->ptr_size, &dst);
        if(err) return err;
        err = ci_lower_expr(ci, ctx, e->values[0], dst, &v);
        if(err) return err;
    }
    if(!src_frame){
        err = ci_alloc_slot(ctx, ctx->ptr_size, ctx->ptr_size, &src);
        if(err) return err;
        err = ci_lower_expr(ci, ctx, e->values[1], src, &v);
        if(err) return err;
    }
    CiOp* op;
    if(size && dst_frame && src_frame){
        err = ci_emit_copy(ctx, dst, src, size, e->loc);
        if(err) return err;
    }
    else if(size){
        err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
        if(err) return err;
        if(dst_frame) *op = (CiOp){.load = {
            .kind = CI_OP_LOAD, .slot = dst, .slot_size = size,
            .align_log2 = src_alignment,
            .src = src, .loc = e->loc,
        }};
        else if(src_frame) *op = (CiOp){.store = {
            .kind = CI_OP_STORE, .slot = dst, .src = src,
            .align_log2 = dst_alignment,
            .src_size = size, .loc = e->loc,
        }};
        else *op = (CiOp){.memcopy = {
            .kind = CI_OP_MEMCOPY,
            .dst_align_log2 = dst_alignment, .src_align_log2 = src_alignment,
            .slot = dst,
            .src = src,
            .size = size,
            .loc = e->loc,
        }};
    }
    if(out && !dst_frame){
        err = ci_emit_copy(ctx, dest, dst, ctx->ptr_size, e->loc);
        if(err) return err;
    }
    else if(out){
        err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
        if(err) return err;
        *op = (CiOp){.slot_addr = {
            .kind = CI_OP_SLOT_ADDR, .slot = dest, .slot_size = ctx->ptr_size,
            .src = dst, .loc = e->loc,
        }};
    }
    ctx->temp = temp;
    return 0;
}

static
int
ci_lower_native_signature(CiInterpreter* ci, Allocator al, CcFunction* type, CcFunction*_Nonnull*_Nonnull out){
    *out = type;
    CcTarget target = ci_target(ci)->target;
    uint32_t total = 0, fixed = 0;
    for(uint32_t i = 0; i < type->param_count; i++){
        if(ci_arg_is_ignored(target, type->params[i])) continue;
        total++;
        if(i < type->fixed_param_count) fixed++;
    }
    if(total == type->param_count) return 0;
    CcQualType* types = NULL;
    if(total){
        types = Allocator_alloc(al, total, sizeof *types, _Alignof(CcQualType));
        if(!types) return CI_OOM_ERROR;
    }
    uint32_t n = 0;
    for(uint32_t i = 0; i < type->param_count; i++)
        if(!ci_arg_is_ignored(target, type->params[i])) types[n++] = type->params[i];
    CcFunction* native_type = cc_intern_function(&ci->parser.type_cache, al, type->return_type, types, total, fixed, type->is_variadic, type->no_prototype);
    if(types) Allocator_free(al, types, total, sizeof *types, _Alignof(CcQualType));
    if(!native_type) return CI_OOM_ERROR;
    *out = native_type;
    return 0;
}

static
int
ci_lower_call_arguments(CiInterpreter* ci, CiLowerCtx* ctx, CcExpr* e, CiCallDescriptor* d, uint32_t* storage_slot, uint32_t result, _Bool variadic, _Bool direct){
    NcCallPlan* plan = d->abi;
    uint32_t storage = *storage_slot;
    uint32_t* slots = Allocator_alloc(ctx->a, (plan->count ? plan->count : 1), sizeof *slots, _Alignof(uint32_t));
    if(!slots) return CI_OOM_ERROR;
    int err = 0;
    uint32_t last_effect = 0;
    for(uint32_t i = plan->count; i > 1;){
        _Bool safe;
        err = ci_expr_discardable(ctx, e->values[--i], &safe);
        if(err) goto cleanup;
        if(!safe){
            last_effect = i;
            break;
        }
    }
    for(uint32_t i = 0; i < plan->count; i++){
        CiLowerVal v;
        err = ci_lower_expr(ci, ctx, e->values[i], CI_NO_SLOT, &v);
        if(err) goto cleanup;
        if(i < last_effect){
            err = ci_preserve_value(ci, ctx, e->values[i], &v);
            if(err) goto cleanup;
        }
        slots[i] = v.slot;
    }
    if(direct){
        size_t size = nc_stack_storage_size(plan);
        err = ci_alloc_slot(ctx, (uint32_t)size, size? (uint32_t)plan->stack_alignment : 1, &storage);
        if(err) goto cleanup;
        *storage_slot = storage;
    }
    if(nc_stack_storage_size(plan) || variadic){
        uint32_t stack = storage + (uint32_t)nc_stack_storage_prefix(plan);
        CiAbiAddressSpec dst = {.mode = CI_ABI_REG, .reg = {.bank = CI_ABI_STACK_BASE, .index = 0}},
                         src = {.mode = CI_ABI_SLOT, .offset = stack};
        err = ci_abi_emit(ctx->out, ctx->a, e->loc, plan->target, CI_ABI_ADDRESS, dst, src, 8);
        if(err) goto cleanup;
    }
    switch(plan->target){
        case CC_TARGET_X86_64_LINUX:
        case CC_TARGET_X86_64_MACOS:
            if(variadic || d->call_type->no_prototype){
                CiAbiAddressSpec dst = {.mode = CI_ABI_REG, .reg = {.bank = CI_ABI_VECTOR_COUNT, .index = 0}},
                                 src = {.immediate = plan->fp};
                err = ci_abi_emit(ctx->out, ctx->a, e->loc, plan->target, CI_ABI_CONSTANT, dst, src, 1);
                if(err) goto cleanup;
            }
            break;
        case CC_TARGET_X86_64_WINDOWS_MSVC:
        case CC_TARGET_X86_64_WINDOWS_MINGW:
            break;
        case CC_TARGET_AARCH64_LINUX:
            break;
        case CC_TARGET_AARCH64_MACOS:
            break;
        case CC_TARGET_TEST:
            break;
        case CC_TARGET_COUNT:
            err = ci_error(ci, e->loc, "call: unsupported target");
            goto cleanup;
        DRP_CASES_EXHAUSTED;
    }
    if(plan->result.mode == NC_ABI_INDIRECT){
        CiAbiAddressSpec dst = {.mode = CI_ABI_REG, .reg = {.bank = CI_ABI_INDIRECT_RESULT, .index = 0}},
                         src = {.mode = CI_ABI_SLOT, .offset = result};
        err = ci_abi_emit(ctx->out, ctx->a, e->loc, plan->target, CI_ABI_ADDRESS, dst, src, 8);
        if(err) goto cleanup;
    }
    for(uint32_t i = 0; i < plan->count; i++){
        err = ci_lower_abi_argument(ctx->out, ctx->a, plan, i, slots[i], 0, e->loc);
        if(err) goto cleanup;
    }
    cleanup:
    Allocator_free(ctx->a, slots, (plan->count ? plan->count : 1), sizeof *slots, _Alignof(uint32_t));
    return err;
}

static
int
ci_lower_inline_call(CiInterpreter* ci, CiLowerCtx* ctx, CcExpr* e, CcFunc* f, uint32_t dest, CiLowerVal*_Nullable out, _Bool borrow_result, _Bool* handled){
    *handled = 0;
    if(ci->opt_noinline
    || f->noinline
    || !f->defined
    || f->hotswappable
    || f->interp_lowering
    || f->type->is_variadic
    || f->type->no_prototype
    || e->call.nargs != f->params.count)
        return 0;
    unsigned depth = 0;
    for(const CiLowerCtx* p = ctx; p; p = p->inline_state ? p->inline_state->parent : NULL){
        if(p->function == f || ++depth > 32)
            return 0;
    }
    int err;
    if(!f->parsed){
        err = cc_parse_func_body(&ci->parser, f);
        if(err) return err;
    }
    if(f->inline_blocked) return 0;
    err = ci_lower_func(ci, f, &ci->deps);
    if(err) return err;
    size_t budget = f->inline_ ? ci->inline_hint_budget : ci->inline_budget;
    if(!f->always_inline && f->interp_ops->code.count > budget) return 0;
    AtomMap(uintptr_t) labels = {0};
    CiFoldCache cache = {0};
    CiInlineState inline_state = {
        .parent = ctx,
        .result = dest,
        .borrow_result = borrow_result,
        .value_slot = dest,
    };
    CiLowerCtx body = {
        .a = ctx->a,
        .out = ctx->out,
        .labels = &labels,
        .materialized_addresses = ctx->materialized_addresses,
        .frame_alignment = ctx->frame_alignment,
        .frame_size = ctx->frame_size,
        .size_size = ctx->size_size,
        .ptr_size = ctx->ptr_size,
        .char_is_unsigned = ctx->char_is_unsigned,
        .static_initializer = ctx->static_initializer,
        .has_alloca = ctx->has_alloca,
        .has_vla = ctx->has_vla,
        .ldbl_fmt = ctx->ldbl_fmt,
        .fold_cache = &cache,
        .function = f,
        .inline_state = &inline_state,
    };
    CcStmtNode* tail = f->body_tree;
    while(tail && tail->kind == CC_STMT_COMPOUND && tail->count)
        tail = tail->stmts[tail->count - 1];
    inline_state.tail_return = tail && tail->kind == CC_STMT_RETURN ? tail : NULL;
    *handled = 1;
    for(size_t i = 0; i < f->params.count; i++){
        CcVariable* var = f->params.data[i].decl;
        uint32_t size, align, slot;
        err = cc_sizeof_as_uint(&ci->parser, var->type, var->loc, &size);
        if(err) goto cleanup;
        err = cc_alignof_as_uint(&ci->parser, var->type, var->loc, &align);
        if(err) goto cleanup;
        if(var->alignment > align) align = var->alignment;
        _Bool stable = !var->mutable_storage && !var->type.is_volatile && !var->type.is_atomic;
        CiLowerVal v = {0};
        int folded = FOLD_FAIL;
        if(stable)
            folded = ci_fold_expr(ci, ctx, e->values[i], &v.value);
        if(folded > 0){
            err = folded;
            goto cleanup;
        }
        if(!folded){
            slot = CI_NO_SLOT;
            v.constant = 1;
        }
        else if(stable && ci_stable_variable(ctx, e->values[i])){
            err = ci_lower_expr(ci, ctx, e->values[i], CI_NO_SLOT, &v);
            if(err) goto cleanup;
            slot = v.slot;
        }
        else {
            err = ci_alloc_slot(ctx, size, align, &slot);
            if(err) goto cleanup;
            err = ci_lower_expr(ci, ctx, e->values[i], slot, &v);
            if(err) goto cleanup;
        }
        err = ci_bind_slot(&body, var, slot);
        if(err) goto cleanup;
        CiInlineBinding* b = ci_inline_binding(&body, var);
        b->parameter = 1;
        if(stable){
            b->value = v.value;
            b->constant = v.constant;
        }
    }
    body.temp = ctx->temp;
    body.materialized_end = ctx->materialized_end;
    err = ci_lower_stmt(ci, &body, f->body_tree);
    if(err) goto cleanup;
    err = ci_lower_resolve_gotos(ci, &body);
    if(err) goto cleanup;
    ci_patch_branches(&body, inline_state.returns, (uint32_t)body.out->count);
    err = ci_lower_deps_merge(&ctx->deps, &body.deps, ctx->a);
    if(err) goto cleanup;
    if(out && inline_state.result_seen && !body.statement_fallthrough){
        out->constant = inline_state.result_constant;
        out->value = inline_state.result_value;
        out->slot = inline_state.value_slot;
    }
    cleanup:
    if(body.frame_alignment > ctx->frame_alignment)
        ctx->frame_alignment = body.frame_alignment;
    ci_bindings_cleanup(&body);
    if(labels.data)
        Allocator_free(ctx->a, labels.data, AM_alloc_size(labels.cap), 1, _Alignof(AtomMapItem));
    ma_cleanup(CiBackpatchTarget)(&body.backpatches, ctx->a);
    ci_fold_cache_cleanup(&cache, ctx->a);
    ci_lower_deps_cleanup(&body.deps, ctx->a);
    return err;
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
        if(ccqt_kind(ct) == CC_FUNCTION) ftype = ccqt_as_function(ct);
        else {
            if(ccqt_kind(ct) != CC_POINTER)
                return ci_unreachable(ci, e->loc, "calling a non-function-pointer?");
            CcQualType pointee = ccqt_as_ptr(ct)->pointee;
            if(ccqt_kind(pointee) != CC_FUNCTION)
                return ci_unreachable(ci, e->loc, "calling a non-function pointer?");
            ftype = ccqt_as_function(pointee);
        }
    }
    uint32_t nargs = e->call.nargs;
    if(!ftype->is_variadic && nargs != ftype->param_count)
        return ci_unimplemented(ci, e->loc, "K&R calls");
    if(func && func->libc_builtin && !func->defined && !func->mangle && nargs == 3 && sv_equals2(SV("memcpy"), func->name->data, func->name->length)){
        CiFoldValue size;
        err = ci_fold_expr(ci, ctx, e->values[2], &size);
        if(err > 0) return err;
        if(!err && size.sz <= 8 && size.bits[0] <= UINT32_MAX)
            return ci_lower_memcpy(ci, ctx, e, dest, out, (uint32_t)size.bits[0]);
    }
    if(func){
        err = ci_lower_func_dep(ctx, func, CC_FUNC_DEP_USED);
        if(err) return err;
    }
    uint32_t ret_size = 0;
    _Bool borrow_result = dest == CI_NO_SLOT;
    if(out){
        err = ci_lower_expr_dest(ci, ctx, e, &dest, out->size);
        if(err) return err;
        out->slot = dest;
        ret_size = out->size;
    }
    uint32_t temp = ctx->temp;
    if(func){
        _Bool handled;
        err = ci_lower_inline_call(ci, ctx, e, func, out? dest : CI_NO_SLOT, out, borrow_result, &handled);
        if(err || handled){
            ctx->temp = temp;
            if(err) return err;
            if(out && out->slot+out->size>ctx->temp)
                ctx->temp = out->slot+out->size;
            return err;
        }
    }
    CiCallDescriptor* d = Allocator_zalloc(ctx->a, 1, sizeof *d, _Alignof(CiCallDescriptor));
    if(!d) return CI_OOM_ERROR;
    if(func) d->func = func;
    d->nargs = nargs;
    for(uint32_t i = 0; i < nargs; i++){
        uint32_t size;
        err = cc_sizeof_as_uint(p, e->values[i]->type, e->values[i]->loc, &size);
        if(err) return err;
    }
    d->call_type = ftype;
    if(ftype->is_variadic && nargs > ftype->param_count){
        CcQualType* types = Allocator_alloc(ctx->a, nargs, sizeof *types, _Alignof(CcQualType));
        if(!types) return CI_OOM_ERROR;
        for(uint32_t i = 0; i < nargs; i++)
            types[i] = i < ftype->param_count ? ftype->params[i] : e->values[i]->type;
        CcFunction* call_type = cc_intern_function(&p->type_cache, ctx->a, ftype->return_type, types, nargs, ftype->param_count, 1, 0);
        Allocator_free(ctx->a, types, nargs, sizeof *types, _Alignof(CcQualType));
        if(!call_type) return CI_OOM_ERROR;
        d->call_type = call_type;
    }
    err = nc_plan_create(ctx->a, ci_target(ci), d->call_type, &d->abi);
    if(err == _cc_overflow_error)
        return ci_error(ci, e->loc, "call argument storage exceeds 32-bit layout limit");
    if(err == _cc_unimplemented_error){
        ci_error(ci, e->loc, "call argument failed to lower");
        return _cc_unimplemented_error;
    }
    if(err) return err;
    size_t state_size = nc_stack_storage_size(d->abi);
    if(!out && d->abi->result.mode == NC_ABI_INDIRECT){
        ret_size = d->abi->result.size;
        err = ci_lower_expr_dest(ci, ctx, e, &dest, ret_size);
        if(err) return err;
    }
    if(state_size > UINT32_MAX)
        return ci_error(ci, e->loc, "call argument storage exceeds 32-bit layout limit");
    uint32_t args_slot=0;
    if(!func){
        err = ci_alloc_slot(ctx, 8, 8, &args_slot);
        if(err) return err;
        err = ci_alloc_slot(ctx, (uint32_t)state_size, state_size ? (uint32_t)d->abi->stack_alignment : 1, &args_slot);
        if(err == _cc_overflow_error)
            return ci_error(ci, e->loc, "call argument storage exceeds 32-bit layout limit");
        if(err) return err;
        CiLowerVal v;
        err = ci_lower_expr(ci, ctx, callee, args_slot - 8, &v);
        if(err == _cc_overflow_error)
            return ci_error(ci, e->loc, "call argument storage exceeds 32-bit layout limit");
        if(err) return err;
    }
    err = ci_lower_call_arguments(ci, ctx, e, d, &args_slot, ret_size ? dest : CI_NO_SLOT, d->call_type->is_variadic, func!=NULL);
    if(err == _cc_overflow_error)
        return ci_error(ci, e->loc, "call argument storage exceeds 32-bit layout limit");
    if(err) return err;
    CiOp* op;
    err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
    if(err) return err;
    *op = (CiOp){
        .call = {
            .kind = CI_OP_CALL,
            .is_indirect = func?0:1,
            .ret_slot = ret_size ? dest : 0,
            .ret_size = ret_size,
            .args_slot = args_slot,
            .descrip = d,
            .loc = e->loc,
        }
    };
    if(out && ret_size){
        err = ci_lower_abi_result(ctx->out, ctx->a, d->abi, dest, 0, e->loc);
        if(err) return err;
    }
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
        && !cc_expr_field_bit_width(lhs)
        && !ci_frame_lvalue(ci, ctx, lhs, &frame_slot)){
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
                    .is_volatile = lhs->type.is_volatile,
                    .kind = CI_OP_STORE_IMM,
                    .size = size,
                    .slot = a.slot,
                    .offset = a.disp, .align_log2 = a.align_log2,
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
        if(ci_frame_lvalue(ci, ctx, lhs, &frame_off))
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
        case CC_VECTOR:
        case CC_ARRAY:
        case CC_SLICE:
            break;
        default:
            return 0; // scalars stage through a slot cheaply
    }
    uint32_t off;
    if(ci_frame_lvalue(ci, ctx, lhs, &off))
        return 0; // the rhs loads directly into the slot
    switch((uint32_t)rhs->kind){
        case CC_EXPR_BLOB:
        case CC_EXPR_VALUE:
            if(rhs->kind == CC_EXPR_BLOB || ccqt_is_fixed_sequence(rhs->type))
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
    if(ci_frame_lvalue(ci, ctx, rhs, &off))
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
            .is_volatile = lhs->type.is_volatile || rhs->type.is_volatile,
            .kind = CI_OP_MEMCOPY,
            .slot = dst.slot,
            .offset = dst.disp, .dst_align_log2 = dst.align_log2,
            .src = src.slot,
            .src_offset = src.disp, .src_align_log2 = src.align_log2,
            .size = rsz,
            .loc = e->loc,
        }
    };
    if(rsz < size){
        err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
        if(err) return err;
        *op = (CiOp){
            .zero = {
                .is_volatile = lhs->type.is_volatile,
                .kind = CI_OP_ZERO,
                .slot = dst.slot,
                .offset = dst.disp+rsz,
                .align_log2 = cc_offset_alignment(dst.align_log2, rsz),
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

static
int
ci_lower_atomic_slot(CiLowerCtx* ctx, uint32_t size, uint32_t* slot){
    if(size > ctx->frame_alignment) ctx->frame_alignment = size;
    if((*slot & (size - 1)) == 0) return 0;
    return ci_alloc_slot(ctx, size, size, slot);
}

static
int
ci_lower_atomic_load_lv(CiInterpreter* ci, CiLowerCtx* ctx, CcExpr* e, uint32_t dest, CiLowerVal* out, uint32_t size){
    int err;
    err = ci_lower_expr_dest(ci, ctx, e, &dest, size);
    if(err) return err;
    uint32_t temp = ctx->temp;
    CiLowerAddr a;
    err = ci_lower_addr(ci, ctx, e, 0, &a); // access
    if(err) return err;
    uint32_t slot = dest;
    err = ci_lower_atomic_slot(ctx, size, &slot);
    if(err) return err;
    CiOp* op;
    err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
    if(err) return err;
    *op = (CiOp){
        .atomic_load = {
            .is_volatile = e->type.is_volatile,
            .kind = CI_OP_ATOMIC_LOAD,
            .memorder = CC_MO_SEQ_CST,
            .slot = slot,
            .slot_size = size,
            .src = a.slot,
            .offset = a.disp,
            .loc = e->loc,
        }
    };
    if(slot != dest){
        err = ci_emit_copy(ctx, dest, slot, size, e->loc);
        if(err) return err;
    }
    ctx->temp = temp;
    out->slot = dest;
    return 0;
}

static
int
ci_lower_atomic_operand(CiInterpreter* ci, CiLowerCtx* ctx, const CcExpr* ve, CiLowerVal* v, uint32_t sz, SrcLoc loc){
    (void)ci;
    if(v->size == sz){
        uint32_t src = v->slot;
        int err = ci_lower_atomic_slot(ctx, sz, &v->slot);
        if(err) return err;
        if(v->slot == src) return 0;
        return ci_emit_copy(ctx, v->slot, src, sz, loc);
    }
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
ci_lower_atomic_compound_assign(CiInterpreter* ci, CiLowerCtx* ctx, CcExpr* e, uint32_t dest, CiLowerVal* out, uint32_t size, _Bool is_ptr, _Bool op_unsigned, _Bool is_float, CiFaluOp fop){
    int err;
    CiOpKind falukind = ci_falu_kind(e->type, ctx->ldbl_fmt);
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
    if(is_ptr){
        err = ci_scale_pointer_index(ci, ctx, ccqt_as_ptr(e->type)->pointee, e->loc, r.slot, &r.slot);
        if(err) return err;
        r.size = ctx->ptr_size;
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
                .is_volatile = lhs->type.is_volatile,
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
                .is_volatile = lhs->type.is_volatile,
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
                    .is_brian_float = ccqt_bt_eq(e->type, CCBT_bfloat16),
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
                .is_volatile = lhs->type.is_volatile,
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
    err = ci_emit_copy(ctx, dest, cur, size, e->loc);
    if(err) return err;
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
            err = ci_lower_expr_dest(ci, ctx, e, &dest, out->size);
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
    uint32_t alignment;
    err = ci_pointer_alignment(ci, ctx, e->lhs, &alignment);
    if(err) return err;
    if(out){
        // the result slot survives; everything else is scratch
        err = ci_lower_expr_dest(ci, ctx, e, &dest, out->size);
        if(err) return err;
        out->slot = dest;
    }
    uint32_t temp = ctx->temp;
    CiLowerVal pv;
    err = ci_lower_expr(ci, ctx, e->lhs, CI_NO_SLOT, &pv);
    if(err) return err;
    if(out && out->size == sz){
        err = ci_lower_atomic_slot(ctx, sz, &dest);
        if(err) return err;
    }
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
                    .is_volatile = ccqt_as_ptr(e->lhs->type)->pointee.is_volatile,
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
                    .is_volatile = ccqt_as_ptr(e->lhs->type)->pointee.is_volatile,
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
                    .is_volatile = ccqt_as_ptr(e->lhs->type)->pointee.is_volatile,
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
                    .is_volatile = ccqt_as_ptr(e->lhs->type)->pointee.is_volatile,
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
            // The exchange itself can replace storage holding this argument.
            err = ci_preserve_value(ci, ctx, e->values[1], &rp);
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
                    .is_volatile = ccqt_as_ptr(e->lhs->type)->pointee.is_volatile,
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
            // TODO: respect weak flag.
            // Just always do strong for now.
            err = ci_lower_expr_discard(ci, ctx, e->values[2]);
            if(err) return err;
            CiLowerVal ep;
            err = ci_lower_expr(ci, ctx, e->values[0], CI_NO_SLOT, &ep);
            if(err) return err;
            uint32_t eslot;
            err = ci_alloc_slot(ctx, sz, sz, &eslot);
            if(err) return err;
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
            // Read the expected object only after all argument expressions.
            err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
            if(err) return err;
            *op = (CiOp){.load = {.kind = CI_OP_LOAD, .slot = eslot,
                .slot_size = sz, .src = ep.slot, .loc = e->loc}};
            uint32_t ok;
            err = ci_alloc_slot(ctx, 1, 1, &ok);
            if(err) return err;
            err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
            if(err) return err;
            *op = (CiOp){
                .atomic_cas = {
                    .is_volatile = ccqt_as_ptr(e->lhs->type)->pointee.is_volatile,
                    .kind = CI_OP_ATOMIC_CAS,
                    .memorder = mo,
                    .fail_memorder = e->atomic.fail_memorder,
                    .weak = 0,
                    .size = sz,
                    .slot = ok,
                    .src = pv.slot,
                    .expected = eslot,
                    .desired = dv.slot,
                    .loc = e->loc,
                }
            };
            // Only failure writes expected. It may alias the atomic object,
            // which already contains desired after a successful exchange.
            err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
            if(err) return err;
            uint32_t skip_writeback = (uint32_t)ctx->out->count - 1;
            *op = (CiOp){.jump_true = {.kind = CI_OP_JUMP_TRUE,
                .slot = ok, .slot_size = 1, .loc = e->loc}};
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
            ctx->out->data[skip_writeback].jump_true.jump = (uint32_t)ctx->out->count;
            if(out){
                err = ci_emit_copy(ctx, dest, ok, 1, e->loc);
                if(err) return err;
                out->canonical = 1;
            }
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
                err = ci_scale_pointer_index(ci, ctx, ccqt_as_ptr(obj)->pointee, e->loc, v.slot, &v.slot);
                if(err) return err;
                v.size = ctx->ptr_size;
            }
            err = ci_lower_atomic_operand(ci, ctx, e->values[0], &v, sz, e->loc);
            if(err) return err;
            _Bool want_new = aop == CC_ATOMIC_ADD_FETCH || aop == CC_ATOMIC_SUB_FETCH;
            if(out && want_new){
                // The RMW may modify storage borrowed by the delta.
                err = ci_preserve_value(ci, ctx, e->values[0], &v);
                if(err) return err;
            }
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
                    .is_volatile = ccqt_as_ptr(e->lhs->type)->pointee.is_volatile,
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
                    .is_volatile = ccqt_as_ptr(e->lhs->type)->pointee.is_volatile,
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
            err = ci_emit_copy(ctx, eslot, c.slot, sz, e->loc);
            if(err) return err;
            err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
            if(err) return err;
            *op = (CiOp){
                .atomic_cas = {
                    .is_volatile = ccqt_as_ptr(e->lhs->type)->pointee.is_volatile,
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
                err = ci_emit_copy(ctx, dest, eslot, sz, e->loc);
                if(err) return err;
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
            err = ci_emit_copy(ctx, des, lo.slot, 8, e->loc);
            if(err) return err;
            err = ci_emit_copy(ctx, des + 8, hi.slot, 8, e->loc);
            if(err) return err;
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
            uint32_t ok;
            err = ci_alloc_slot(ctx, 1, 1, &ok);
            if(err) return err;
            err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
            if(err) return err;
            *op = (CiOp){
                .atomic_cas = {
                    .is_volatile = ccqt_as_ptr(e->lhs->type)->pointee.is_volatile,
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
            if(out){
                err = ci_emit_copy(ctx, dest, ok, 1, e->loc);
                if(err) return err;
                out->canonical = 1;
            }
            break;
        }
        case CC_ATOMIC_THREAD_FENCE:
        case CC_ATOMIC_SIGNAL_FENCE:
            break; // handled above
    }
    if(out && dest != out->slot){
        err = ci_emit_copy(ctx, out->slot, dest, sz, e->loc);
        if(err) return err;
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
ci_emit_copy_access(CiLowerCtx* ctx, uint32_t dst, uint32_t src, uint32_t size, SrcLoc loc, _Bool observable){
    if(!observable && (dst == src || !size)) return 0;
    int err;
    CiOp* op;
    err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
    if(err) return err;
    *op = (CiOp){
        .copy = {
            .is_volatile = observable,
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
ci_emit_copy(CiLowerCtx* ctx, uint32_t dst, uint32_t src, uint32_t size, SrcLoc loc){
    return ci_emit_copy_access(ctx, dst, src, size, loc, 0);
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
            .kind = ci_int_op_kind(size),
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

    CcQualType element = ccqt_as_slice(e->type)->pointee;
    uint32_t size;
    err = cc_sizeof_as_uint(p, e->type, e->loc, &size);
    if(err) return err;

    err = ci_lower_expr_dest(ci, ctx, e, &dest, size);
    if(err) return err;
    *out = (CiLowerVal){
        .slot = dest,
        .size = size,
    };
    uint32_t temp = ctx->temp;

    uint32_t base_ptr;
    uint32_t len_slot = 0;
    _Bool have_len = 0;
    if(bk == CC_VLA){
        CiLowerAddr a;
        err = ci_lower_addr(ci, ctx, base, 0, &a);
        if(err) return err;
        CiLowerVal pointer, length;
        err = ci_addr_to_value(ctx, a, CI_NO_SLOT, ptr_size, t->alignof_[CCBT_nullptr_t], e->loc, &pointer);
        if(err) return err;
        base_ptr = pointer.slot;
        err = ci_lower_expr(ci, ctx, ccqt_as_vla(base->type)->bound, CI_NO_SLOT, &length);
        if(err) return err;
        len_slot = length.slot;
        have_len = 1;
    }
    else if(bk == CC_ARRAY || bk == CC_VECTOR){
        CiLowerAddr a;
        err = ci_lower_addr(ci, ctx, base, 0, &a);
        if(err) return err;
        CiLowerVal bv;
        err = ci_addr_to_value(ctx, a, CI_NO_SLOT, ptr_size, t->alignof_[CCBT_nullptr_t], e->loc, &bv);
        if(err) return err;
        base_ptr = bv.slot;
        if(!ccqt_array_incomplete(base->type)){
            err = ci_emit_const(ci, ctx, ccqt_sequence_length(base->type), size_size, e->loc, &len_slot);
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

    uint32_t result;
    err = ci_alloc_slot(ctx, size, ptr_size, &result);
    if(err) return err;
    // count = hi - lo
    if(lo_is_zero)
        err = ci_emit_copy(ctx, result, hi_slot, size_size, e->loc);
    else
        err = ci_emit_alu(ctx, CI_ALU_SUB, result, hi_slot, lo_slot, size_size, e->loc);
    if(err) return err;

    // data = base_ptr + lo*elem_sz  →  dest[8:16]
    if(lo_is_zero){
        err = ci_emit_copy(ctx, result + ptr_size, base_ptr, size_size, e->loc);
        if(err) return err;
    }
    else {
        uint32_t scaled;
        err = ci_scale_pointer_index(ci, ctx, element, e->loc, lo_slot, &scaled);
        if(err) return err;
        err = ci_emit_alu(ctx, CI_ALU_ADD, result + size_size, base_ptr, scaled, size_size, e->loc);
        if(err) return err;
    }
    err = ci_emit_copy(ctx, dest, result, size, e->loc);
    if(err) return err;
    ctx->temp = temp;
    return 0;
}

static
int
ci_lower_va(CiInterpreter* ci, CiLowerCtx* ctx, CcExpr* e, uint32_t dest, CiLowerVal*_Nullable out){
    int err;
    switch(e->va.op){
    case CC_VA_START:{
        if(!ctx->function_abi)
            return ci_ice(ci, e->loc, "va_start requires a function ABI%s", "");
        uint32_t temp = ctx->temp;
        CiLowerVal ap;
        err = ci_lower_expr(ci, ctx, e->lhs, CI_NO_SLOT, &ap);
        if(err) return err;
        ctx->temp = temp;
        const NcCallPlan* p = ctx->function_abi;
        uint32_t slot = ap.slot;
        CiAbiAddressSpec overflow = {
            .mode = CI_ABI_INCOMING_PTR,
            .base = {.bank = CI_ABI_STACK_BASE},
            .offset = (uint32_t)p->named_stack_size,
        };
        switch(p->target){
            case CC_TARGET_X86_64_LINUX:
            case CC_TARGET_X86_64_MACOS: {
                CiAbiAddressSpec dst, src;
                dst = (CiAbiAddressSpec){.mode = CI_ABI_SLOT_PTR, .pointer = slot, .offset = offsetof(NcSysvVaListTag, gp_offset)};
                src = (CiAbiAddressSpec){.immediate = p->named_gp * 8};
                err = ci_abi_emit(ctx->out, ctx->a, e->loc, p->target, CI_ABI_CONSTANT, dst, src, 4);
                if(err) return err;
                dst = (CiAbiAddressSpec){.mode = CI_ABI_SLOT_PTR, .pointer = slot, .offset = offsetof(NcSysvVaListTag, fp_offset)};
                src = (CiAbiAddressSpec){.immediate = 48 + p->named_fp * 16};
                err = ci_abi_emit(ctx->out, ctx->a, e->loc, p->target, CI_ABI_CONSTANT, dst, src, 4);
                if(err) return err;
                dst = (CiAbiAddressSpec){.mode = CI_ABI_SLOT_PTR, .pointer = slot, .offset = offsetof(NcSysvVaListTag, overflow_arg_area)};
                err = ci_abi_emit(ctx->out, ctx->a, e->loc, p->target, CI_ABI_ADDRESS, dst, overflow, 8);
                if(err) return err;
                dst = (CiAbiAddressSpec){.mode = CI_ABI_SLOT_PTR, .pointer = slot, .offset = offsetof(NcSysvVaListTag, reg_save_area)};
                src = (CiAbiAddressSpec){.mode = CI_ABI_GP_SAVE};
                return ci_abi_emit(ctx->out, ctx->a, e->loc, p->target, CI_ABI_ADDRESS, dst, src, 8);
            }
            case CC_TARGET_AARCH64_LINUX: {
                CiAbiAddressSpec dst, src;
                dst = (CiAbiAddressSpec){.mode = CI_ABI_SLOT_PTR, .pointer = slot, .offset = offsetof(NcAapcs64VaList, __stack)};
                err = ci_abi_emit(ctx->out, ctx->a, e->loc, p->target, CI_ABI_ADDRESS, dst, overflow, 8);
                if(err) return err;
                dst = (CiAbiAddressSpec){.mode = CI_ABI_SLOT_PTR, .pointer = slot, .offset = offsetof(NcAapcs64VaList, __gr_top)};
                src = (CiAbiAddressSpec){.mode = CI_ABI_GP_SAVE, .offset = 64};
                err = ci_abi_emit(ctx->out, ctx->a, e->loc, p->target, CI_ABI_ADDRESS, dst, src, 8);
                if(err) return err;
                dst = (CiAbiAddressSpec){.mode = CI_ABI_SLOT_PTR, .pointer = slot, .offset = offsetof(NcAapcs64VaList, __vr_top)};
                src = (CiAbiAddressSpec){.mode = CI_ABI_FP_SAVE, .offset = 128};
                err = ci_abi_emit(ctx->out, ctx->a, e->loc, p->target, CI_ABI_ADDRESS, dst, src, 8);
                if(err) return err;
                dst = (CiAbiAddressSpec){.mode = CI_ABI_SLOT_PTR, .pointer = slot, .offset = offsetof(NcAapcs64VaList, __gr_offs)};
                src = (CiAbiAddressSpec){.immediate = (uint32_t)(((int)p->named_gp - 8) * 8)};
                err = ci_abi_emit(ctx->out, ctx->a, e->loc, p->target, CI_ABI_CONSTANT, dst, src, 4);
                if(err) return err;
                dst = (CiAbiAddressSpec){.mode = CI_ABI_SLOT_PTR, .pointer = slot, .offset = offsetof(NcAapcs64VaList, __vr_offs)};
                src = (CiAbiAddressSpec){.immediate = (uint32_t)(((int)p->named_fp - 8) * 16)};
                return ci_abi_emit(ctx->out, ctx->a, e->loc, p->target, CI_ABI_CONSTANT, dst, src, 4);
            }
            case CC_TARGET_AARCH64_MACOS: {
                overflow.offset = (uint32_t)((p->named_stack_size + 7) & ~(size_t)7);
                CiAbiAddressSpec dst = {.mode = CI_ABI_SLOT_PTR, .pointer = slot, .offset = 0};
                return ci_abi_emit(ctx->out, ctx->a, e->loc, p->target, CI_ABI_ADDRESS, dst, overflow, 8);
            }
            case CC_TARGET_X86_64_WINDOWS_MSVC:
            case CC_TARGET_X86_64_WINDOWS_MINGW: {
                CiAbiAddressSpec dst, src;
                overflow.offset = (uint32_t)((p->named_stack_size + 7) & ~(size_t)7);
                if(p->named_gp < 4){
                    for(unsigned i = p->named_gp; i < 4; i++){
                        dst = (CiAbiAddressSpec){.mode = CI_ABI_INCOMING_PTR, .base = {.bank = CI_ABI_STACK_BASE, .index = 0}, .offset = i * 8 - 32};
                        src = (CiAbiAddressSpec){.mode = CI_ABI_INCOMING, .reg = {.bank = CI_ABI_GP, .index = i}};
                        err = ci_abi_emit(ctx->out, ctx->a, e->loc, p->target, CI_ABI_COPY, dst, src, 8);
                        if(err) return err;
                    }
                    overflow.offset = p->named_gp * 8 - 32;
                }
                dst = (CiAbiAddressSpec){.mode = CI_ABI_SLOT_PTR, .pointer = slot, .offset = 0};
                return ci_abi_emit(ctx->out, ctx->a, e->loc, p->target, CI_ABI_ADDRESS, dst, overflow, 8);
            }
            case CC_TARGET_TEST: {
                overflow.offset = (uint32_t)((p->named_stack_size + 7) & ~(size_t)7);
                CiAbiAddressSpec dst = {.mode = CI_ABI_SLOT_PTR, .pointer = slot, .offset = 0};
                return ci_abi_emit(ctx->out, ctx->a, e->loc, p->target, CI_ABI_ADDRESS, dst, overflow, 8);
            }
            case CC_TARGET_COUNT:
                return ci_error(ci, e->loc, "va_start: unsupported target");
            DRP_CASES_EXHAUSTED;
        }
    }
    case CC_VA_END:
        return ci_lower_expr_discard(ci, ctx, e->lhs);
    case CC_VA_ARG:{
        CcParser* p = &ci->parser;
        uint32_t size, align;
        err = cc_alignof_as_uint(p, ccqt_unaligned(e->type), e->loc, &align);
        if(err) return err;
        err = cc_sizeof_as_uint(p, e->type, e->loc, &size);
        if(err) return err;
        err = ci_lower_dest(ctx, &dest, size, align);
        if(err) return err;
        if(out) out->slot = dest;
        uint32_t temp = ctx->temp;
        CiLowerVal ap;
        err = ci_lower_expr(ci, ctx, e->lhs, CI_NO_SLOT, &ap);
        if(err) return err;
        NcAbiType type;
        err = nc_abi_type(ci_target(ci), e->type, NC_ABI_VARIADIC_ARGUMENT, &type);
        if(err) return err;
        CiOp* op;
        err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
        if(err) return err;
        *op = (CiOp){.va_arg_={
            .kind = CI_OP_VA_ARG,
            .align_log2 = type.align_log2,
            .count = type.count,
            .mode = type.mode,
            .pieces = {type.pieces[0], type.pieces[1], type.pieces[2], type.pieces[3]},
            .slot = dest,
            .size = type.size,
            .src = ap.slot,
            .loc = e->loc,
            .target = ci_target(ci)->target,
        }};
        ctx->temp = temp;
        return 0;
    }
    case CC_VA_COPY:{
        uint32_t n;
        switch(ci_target(ci)->target){
        case CC_TARGET_AARCH64_MACOS:
        case CC_TARGET_X86_64_WINDOWS_MSVC:
        case CC_TARGET_X86_64_WINDOWS_MINGW:
        case CC_TARGET_TEST:
            n = sizeof(void*);
            break;
        case CC_TARGET_X86_64_LINUX:
        case CC_TARGET_X86_64_MACOS:
            n = sizeof(NcSysvVaListTag);
            break;
        case CC_TARGET_AARCH64_LINUX:
            n = sizeof(NcAapcs64VaList);
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
                .is_volatile = ccqt_as_ptr(e->lhs->type)->pointee.is_volatile || ccqt_as_ptr(e->values[0]->type)->pointee.is_volatile,
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

static
int
ci_expr_discardable(CiLowerCtx* ctx, CcExpr* e, _Bool* safe){
    CiFoldCache* cache = ctx->fold_cache;
    CiDiscardState entry = cache ? (CiDiscardState)(uintptr_t)PM_get(&cache->discardable, e) : CI_DISCARD_UNCHECKED;
    if(entry != CI_DISCARD_UNCHECKED){
        *safe = entry == CI_DISCARD_SAFE;
        return 0;
    }
    *safe = 0;
    if(cc_expr_has_runtime_check(e))
        return 0;
    if(e->is_lvalue && (e->type.is_volatile || e->type.is_atomic))
        return 0;
    // Keep unsupported-type diagnostics in the normal discard walker.
    if(ccqt_is_basic(e->type) && ccbt_is_complex(ccqt_basic_kind(e->type)))
        return 0;
    CcExpr* children[3];
    unsigned count = 0;
    switch((uint32_t)e->kind){
        case CC_EXPR_BLOB:
        case CC_EXPR_VALUE:
        case CC_EXPR_VARIABLE:
        case CC_EXPR_FUNCTION:
            break;
        case CC_EXPR_NEG: case CC_EXPR_POS: case CC_EXPR_BITNOT:
        case CC_EXPR_LOGNOT: case CC_EXPR_DEREF: case CC_EXPR_ADDR:
        case CC_EXPR_CAST: case CC_EXPR_BSWAP: case CC_EXPR_OBJECT_VIEW:
            children[count++] = e->lhs;
            break;
        case CC_EXPR_ADD: case CC_EXPR_SUB: case CC_EXPR_MUL:
        case CC_EXPR_DIV: case CC_EXPR_MOD: case CC_EXPR_BITAND:
        case CC_EXPR_BITOR: case CC_EXPR_BITXOR: case CC_EXPR_LSHIFT:
        case CC_EXPR_RSHIFT: case CC_EXPR_EQ: case CC_EXPR_NE:
        case CC_EXPR_LT: case CC_EXPR_LE: case CC_EXPR_GT: case CC_EXPR_GE:
        case CC_EXPR_LOGAND: case CC_EXPR_LOGOR: case CC_EXPR_COMMA:
        case CC_EXPR_SUBSCRIPT:
            children[count++] = e->lhs;
            children[count++] = e->values[0];
            break;
        case CC_EXPR_TERNARY:
            children[count++] = e->lhs;
            children[count++] = e->values[0];
            children[count++] = e->values[1];
            break;
        case CC_EXPR_DOT: case CC_EXPR_ARROW:
            children[count++] = e->values[0];
            break;
        default: goto done;
    }
    for(unsigned i = 0; i < count; i++){
        _Bool child;
        int err = ci_expr_discardable(ctx, children[i], &child);
        if(err) return err;
        if(!child) goto done;
    }
    *safe = 1;
    done:
    if(cache && PM_put(&cache->discardable, ctx->a, e, (void*)(uintptr_t)(*safe ? CI_DISCARD_SAFE : CI_DISCARD_REQUIRED)))
        return CI_OOM_ERROR;
    return 0;
}

static
int
ci_lower_vla_assign(CiInterpreter* ci, CiLowerCtx* ctx, CcExpr* e, CiLowerAddr*_Nullable out){
    CiLowerAddr dst, src;
    int err = ci_lower_addr(ci, ctx, e->lhs, 0, &dst);
    if(err) return err;
    err = ci_lower_addr(ci, ctx, e->values[0], 0, &src);
    if(err) return err;
    CiLowerVal size;
    err = ci_lower_sizeof(ci, ctx, e->type, e->loc, &size);
    if(err) return err;
    _Bool observable = ccqt_array_object_type(e->lhs->type).is_volatile
        || ccqt_array_object_type(e->values[0]->type).is_volatile;
    if(out){
        uint32_t alignment, slot;
        err = cc_alignof_as_uint(&ci->parser, e->type, e->loc, &alignment);
        if(err) return err;
        err = ci_alloc_slot(ctx, ctx->ptr_size, ctx->ptr_size, &slot);
        if(err) return err;
        err = ma_push(CiOp)(ctx->out, ctx->a, (CiOp){.alloca={
            .kind=CI_OP_ALLOCA, .slot=slot, .src=size.slot,
            .alignment=alignment, .loc=e->loc,
        }});
        if(err) return err;
        *out = (CiLowerAddr){.slot=slot};
        err = ma_push(CiOp)(ctx->out, ctx->a, (CiOp){.memcopy={
            .kind=CI_OP_MEMCOPY, .slot=slot, .src=src.slot,
            .src_offset=src.disp, .src_align_log2=src.align_log2,
            .size=size.slot, .dynamic_size=1, .is_volatile=observable, .loc=e->loc,
        }});
        if(err) return err;
        src = *out;
    }
    return ma_push(CiOp)(ctx->out, ctx->a, (CiOp){.memcopy={
        .kind=CI_OP_MEMCOPY, .slot=dst.slot, .offset=dst.disp,
        .dst_align_log2=dst.align_log2, .src=src.slot, .src_offset=src.disp,
        .src_align_log2=src.align_log2, .size=size.slot, .dynamic_size=1,
        .is_volatile=observable, .loc=e->loc,
    }});
}

static
int
ci_lower_address_discard(CiInterpreter* ci, CiLowerCtx* ctx, CcExpr* e){
    int err;
    switch(e->kind){
        case CC_EXPR_VARIABLE: case CC_EXPR_FUNCTION:
        case CC_EXPR_VALUE: case CC_EXPR_BLOB:
            return 0;
        case CC_EXPR_DEREF:
            return ci_lower_expr_discard(ci, ctx, e->lhs);
        case CC_EXPR_ARROW:
            return ci_lower_expr_discard(ci, ctx, e->values[0]);
        case CC_EXPR_DOT:
            return ci_lower_address_discard(ci, ctx, e->values[0]);
        case CC_EXPR_OBJECT_VIEW:
            return ci_lower_address_discard(ci, ctx, e->lhs);
        case CC_EXPR_SUBSCRIPT:
            err = ccqt_is_fixed_sequence(e->lhs->type)
                ? ci_lower_address_discard(ci, ctx, e->lhs)
                : ci_lower_expr_discard(ci, ctx, e->lhs);
            if(err) return err;
            return ci_lower_expr_discard(ci, ctx, e->values[0]);
        case CC_EXPR_COMMA:
            err = ci_lower_expr_discard(ci, ctx, e->lhs);
            if(err) return err;
            return ci_lower_address_discard(ci, ctx, e->values[0]);
        case CC_EXPR_STATEMENT_EXPRESSION:
            return ci_lower_stmt(ci, ctx, e->stmt_body);
        case CC_EXPR_COMPOUND_LITERAL: case CC_EXPR_INIT_LIST:
            for(size_t i = 0; i < e->init_list->count; i++){
                err = ci_lower_expr_discard(ci, ctx, e->init_list->entries[i].value);
                if(err) return err;
            }
            return 0;
        default: {
            uint32_t temp = ctx->temp;
            CiLowerAddr address;
            err = ci_lower_addr(ci, ctx, e, 1, &address);
            ctx->temp = temp;
            return err;
        }
    }
}

static
int
ci_lower_expr_discard(CiInterpreter* ci, CiLowerCtx* ctx, CcExpr* e){
    int err;
    if(cc_expr_has_runtime_check(e)){
        uint32_t temp = ctx->temp;
        CiLowerVal value;
        err = ci_lower_expr(ci, ctx, e, CI_NO_SLOT, &value);
        ctx->temp = temp;
        return err;
    }
    if(e->kind == CC_EXPR_ASSIGN && ccqt_kind(e->type) == CC_VLA)
        return ci_lower_vla_assign(ci, ctx, e, NULL);
    if(ccqt_is_fixed_sequence(e->type) || ccqt_kind(e->type) == CC_VLA || ccqt_kind(e->type) == CC_FUNCTION)
        return ci_lower_address_discard(ci, ctx, e);
    if(e->is_lvalue && (e->type.is_volatile || e->type.is_atomic) && !ccqt_is_fixed_sequence(e->type) && ccqt_kind(e->type) != CC_FUNCTION){
        uint32_t temp = ctx->temp, size, align, slot;
        err = cc_sizeof_as_uint(&ci->parser, e->type, e->loc, &size);
        if(err) return err;
        err = cc_alignof_as_uint(&ci->parser, e->type, e->loc, &align);
        if(err) return err;
        err = ci_alloc_slot(ctx, size, align, &slot);
        if(err) return err;
        CiLowerVal value;
        err = ci_lower_expr(ci, ctx, e, slot, &value);
        ctx->temp = temp;
        return err;
    }
    if(ccqt_is_basic(e->type) && ccbt_is_complex(ccqt_basic_kind(e->type))){
        ci_error(ci, e->loc, "complex expression lowering is not supported");
        return CI_UNIMPLEMENTED_ERROR;
    }
    switch(e->kind){
        case CC_EXPR_OBJECT_VIEW:
            return ci_lower_expr_discard(ci, ctx, e->lhs);
        case CC_EXPR_BLOB:
        case CC_EXPR_VALUE:
            return 0;
        case CC_EXPR_VARIABLE:
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
        case CC_EXPR_BIT_BUILTIN:
            if(e->bit_builtin.nargs && (e->bit_builtin.op == CC_BIT_CLZ || e->bit_builtin.op == CC_BIT_CTZ)){
                uint32_t chain = 0;
                err = ci_lower_branch(ci, ctx, e->lhs, 1, e->loc, &chain);
                if(err) return err;
                err = ci_lower_expr_discard(ci, ctx, e->values[0]);
                if(err) return err;
                ci_patch_branches(ctx, chain, (uint32_t)ctx->out->count);
                return 0;
            }
            err = ci_lower_expr_discard(ci, ctx, e->lhs);
            if(err) return err;
            if(e->bit_builtin.nargs)
                return ci_lower_expr_discard(ci, ctx, e->values[0]);
            return 0;
        case CC_EXPR_NEG:
        case CC_EXPR_POS:
        case CC_EXPR_BITNOT:
        case CC_EXPR_LOGNOT:
        case CC_EXPR_DEREF:
        case CC_EXPR_CAST:
        case CC_EXPR_SLICE_ALL:
        case CC_EXPR_BSWAP:
            err = ci_lower_expr_discard(ci, ctx, e->lhs);
            return err;
        case CC_EXPR_ALLOCA:
            if(ctx->has_vla) return ci_error(ci, e->loc, "cannot mix explicit alloca and VLA allocations in one function");
            ctx->has_alloca = 1;
            return ci_lower_expr_discard(ci, ctx, e->lhs);
        case CC_EXPR_ADDR:
            return ci_lower_address_discard(ci, ctx, e->lhs);
        case CC_EXPR_INTERN:
        case CC_EXPR_SRCLOC_REFLECT:
            // idk maybe these should be treated as having side effects?
            err = ci_lower_expr_discard(ci, ctx, e->lhs);
            return err;
        case CC_EXPR_COMPILE:
            err = ci_lower_expr_discard(ci, ctx, e->lhs);
            if(err) return err;
            err = ci_lower_expr_discard(ci, ctx, e->values[0]);
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
            _Bool safe;
            err = ci_expr_discardable(ctx, e->values[0], &safe);
            if(err) return err;
            if(safe) return ci_lower_expr_discard(ci, ctx, e->lhs);
            _Bool truth;
            int folded = ci_fold_condition(ci, ctx, e->lhs, &truth);
            if(folded > 0) return folded;
            if(!folded){
                if(truth == (e->kind == CC_EXPR_LOGOR)) return 0;
                return ci_lower_expr_discard(ci, ctx, e->values[0]);
            }
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
            uint32_t size;
            err = cc_sizeof_as_uint(&ci->parser, e->type, e->loc, &size);
            if(err) return err;
            return ci_lower_assign(ci, ctx, e, CI_NO_SLOT, NULL, size);
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
            return ci_lower_compound(ci, ctx, e, CI_NO_SLOT, NULL);
        }
        case CC_EXPR_TERNARY:{
            _Bool yes, no;
            err = ci_expr_discardable(ctx, e->values[0], &yes);
            if(err) return err;
            err = ci_expr_discardable(ctx, e->values[1], &no);
            if(err) return err;
            if(yes && no) return ci_lower_expr_discard(ci, ctx, e->lhs);
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
            if(e->builtin.op == CC_BUILTIN_CLEAR_CACHE)
                return ci_lower_clear_cache(ci, ctx, e);
            if(e->builtin.op == CC_BUILTIN_CAPTURE_BACKTRACE || e->builtin.op == CC_BUILTIN_SYMBOLICATE)
                return ci_lower_rt_call(ci, ctx, e, e->builtin.op == CC_BUILTIN_CAPTURE_BACKTRACE ? CI_RT_CAPTURE_BACKTRACE : CI_RT_SYMBOLICATE, CI_NO_SLOT, NULL);
            CiOp* op;
            err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
            if(err) return err;
            *op = (CiOp){
                .builtin = {
                    .kind = CI_OP_BUILTIN,
                    .op = e->builtin.op,
                    .loc = e->loc,
                    .slot = CI_NO_SLOT,
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
ci_lower_dest(CiLowerCtx* ctx, uint32_t* dest, uint32_t size, uint32_t align){
    if(*dest != CI_NO_SLOT) return 0;
    return ci_alloc_slot(ctx, size, align, dest);
}

static
int
ci_lower_expr_dest(CiInterpreter* ci, CiLowerCtx* ctx, const CcExpr* e, uint32_t* dest, uint32_t size){
    if(*dest != CI_NO_SLOT) return 0;
    // FIXME: why are we getting function types here?
    CcQualType type = ccqt_kind(e->type) == CC_FUNCTION ? ccqt_basic(CCBT_nullptr_t) : e->type;
    uint32_t align;
    int err = cc_alignof_as_uint(&ci->parser, type, e->loc, &align);
    if(err) return err;
    return ci_lower_dest(ctx, dest, size, align);
}

static
int
ci_addr_to_value(CiLowerCtx* ctx, CiLowerAddr a, uint32_t dest, uint32_t size, uint32_t align, SrcLoc loc, CiLowerVal* out){
    int err;
    out->size = size;
    out->canonical = 0;
    if(a.disp == 0 && dest == CI_NO_SLOT){
        // the base pointer slot already holds the value
        out->slot = a.slot;
        return 0;
    }
    err = ci_lower_dest(ctx, &dest, size, align);
    if(err) return err;
    out->slot = dest;
    CiOp* op;
    if(a.disp == 0){
        err = ci_emit_copy(ctx, dest, a.slot, size, loc);
        if(err) return err;
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
uint32_t
ci_frame_alignment(const CiLowerCtx* ctx, uint32_t slot){
    uint32_t alignment = ctx->frame_alignment > 16 ? ctx->frame_alignment : 16;
    uint32_t exponent = (uint32_t)(31 - clz_32(alignment));
    return cc_offset_alignment(exponent, slot);
}

static
int
ci_index_alignment(CiInterpreter* ci, CiLowerCtx* ctx, CcExpr* index, uint32_t stride, uint32_t* out){
    CiFoldValue value;
    int status = ci_fold_expr(ci, ctx, index, &value);
    if(status > 0) return status;
    uint64_t offset = status == 0 ? ci_fold_offset(ctx, &value, stride) : stride;
    *out = cc_offset_alignment(*out, offset);
    return 0;
}

static
int
ci_pointer_alignment(CiInterpreter* ci, CiLowerCtx* ctx, const CcExpr* e, uint32_t* out){
    if(ccqt_kind(e->type) == CC_VLA)
        return cc_type_alignment(&ci->parser, e->type, e->loc, out);
    uint32_t source_alignment = 0;
    if((ccqt_is_fixed_sequence(e->type) || ccqt_kind(e->type) == CC_VLA))
        return ci_lvalue_alignment(ci, ctx, e, out);
    switch((uint32_t)e->kind){
        case CC_EXPR_ADDR:
            return ci_lvalue_alignment(ci, ctx, e->lhs, out);
        case CC_EXPR_COMMA:
            return ci_pointer_alignment(ci, ctx, e->values[0], out);
        case CC_EXPR_ASSIGN:
            return ci_pointer_alignment(ci, ctx, e->values[0], out);
        case CC_EXPR_OBJECT_VIEW:
            return ci_pointer_alignment(ci, ctx, e->lhs, out);
        case CC_EXPR_BLOB:
        case CC_EXPR_VALUE:
            // Literal pointer bits need not satisfy the pointee's alignment.
            *out = 0;
            return 0;
        case CC_EXPR_CAST:{
            // The programmer promises the destination type's alignment.
            // Preserve any stronger guarantee, including aligned array decay.
            if(ccqt_kind(e->lhs->type) == CC_POINTER || (ccqt_is_fixed_sequence(e->lhs->type) || ccqt_kind(e->lhs->type) == CC_VLA)){
                int err = ci_pointer_alignment(ci, ctx, e->lhs, &source_alignment);
                if(err) return err;
            }
            break;
        }
        case CC_EXPR_TERNARY:{
            uint32_t other;
            int err = ci_pointer_alignment(ci, ctx, e->values[0], out);
            if(err) return err;
            err = ci_pointer_alignment(ci, ctx, e->values[1], &other);
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
                return cc_type_alignment(&ci->parser, ccqt_as_ptr(base->type)->pointee, e->loc, out);
            int err = ci_pointer_alignment(ci, ctx, base, out);
            if(err) return err;
            uint32_t stride;
            err = cc_sizeof_as_uint(&ci->parser, ccqt_as_ptr(base->type)->pointee, e->loc, &stride);
            if(err) return err;
            return ci_index_alignment(ci, ctx, index, stride, out);
        }
    }
    return cc_pointer_type_alignment(&ci->parser, e->type, e->loc, source_alignment, out);
}

static
int
ci_lvalue_alignment(CiInterpreter* ci, CiLowerCtx* ctx, const CcExpr* e, uint32_t* out){
    if(ccqt_kind(e->type) == CC_VLA)
        return cc_type_alignment(&ci->parser, e->type, e->loc, out);
    int err;
    switch((uint32_t)e->kind){
        case CC_EXPR_COMMA:
            return ci_lvalue_alignment(ci, ctx, e->values[0], out);
        case CC_EXPR_DEREF:
            return ci_pointer_alignment(ci, ctx, e->lhs, out);
        case CC_EXPR_DOT:
        case CC_EXPR_ARROW:
            err = e->kind == CC_EXPR_DOT
                ? ci_lvalue_alignment(ci, ctx, e->values[0], out)
                : ci_pointer_alignment(ci, ctx, e->values[0], out);
            if(err) return err;
            uint64_t field_offset;
            err = cc_expr_field_offset(ci_target(ci), e, &field_offset);
            if(err) return err;
            *out = cc_offset_alignment(*out, field_offset);
            return 0;
        case CC_EXPR_SUBSCRIPT:{
            const CcExpr* base = e->lhs;
            if((ccqt_is_fixed_sequence(base->type) || ccqt_kind(base->type) == CC_VLA))
                err = ci_lvalue_alignment(ci, ctx, base, out);
            else if(ccqt_kind(base->type) == CC_POINTER)
                err = ci_pointer_alignment(ci, ctx, base, out);
            else
                err = cc_type_alignment(&ci->parser, e->type, e->loc, out);
            if(err) return err;
            uint32_t stride;
            err = cc_sizeof_as_uint(&ci->parser, e->type, e->loc, &stride);
            if(err) return err;
            return ci_index_alignment(ci, ctx, e->values[0], stride, out);
        }
    }
    return cc_lvalue_alignment(&ci->parser, e, out);
}

static
int
ci_lower_addr(CiInterpreter* ci, CiLowerCtx* ctx, CcExpr* lv, _Bool one_past_ok, CiLowerAddr* out){
    uint32_t alignment;
    int err = ci_lvalue_alignment(ci, ctx, lv, &alignment);
    if(err) return err;
    err = ci_lower_addr_inner(ci, ctx, lv, one_past_ok, out);
    if(err) return err;
    out->align_log2 = alignment;
    out->is_volatile = lv->type.is_volatile;
    return 0;
}

static
int
ci_lower_bitfield_addr(CiInterpreter* ci, CiLowerCtx* ctx, CcExpr* lv, CiLowerAddr* out){
    int err;
    while(lv->kind == CC_EXPR_COMMA){
        err = ci_lower_expr_discard(ci, ctx, lv->lhs);
        if(err) return err;
        lv = lv->values[0];
    }
    uint64_t field_offset;
    err = cc_expr_field_offset(ci_target(ci), lv, &field_offset);
    if(err) return err;
    if(lv->kind == CC_EXPR_ARROW){
        CiLowerVal v;
        err = ci_lower_expr(ci, ctx, lv->values[0], CI_NO_SLOT, &v);
        if(err) return err;
        err = ci_preserve_value(ci, ctx, lv->values[0], &v);
        if(err) return err;
        out->slot = v.slot;
        out->disp = (uint32_t)field_offset;
        return ci_lvalue_alignment(ci, ctx, lv, &out->align_log2);
    }
    // DOT: the storage unit is at the base's address plus the member's offset
    err = ci_lower_addr(ci, ctx, lv->values[0], 0, out);
    if(err) return err;
    out->disp += (uint32_t)field_offset;
    return ci_lvalue_alignment(ci, ctx, lv, &out->align_log2);
}

static
int
ci_lower_whole_byte_bitfield(CiLowerCtx* ctx, CiOp* op){
    _Bool load = op->kind == CI_OP_LOAD_BITFIELD || op->kind == CI_OP_LOAD_BITFIELD_BYTES;
    CiOp field = *op;
    uint32_t width = load ? field.load_bf.bit_width : field.store_bf.bit_width;
    uint32_t bit_offset = load ? field.load_bf.bit_offset : field.store_bf.bit_offset;
    if(bit_offset % 8 || (width != 8 && width != 16 && width != 32 && width != 64))
        return 0;
    uint32_t bytes = width / 8;
    uint32_t size = load ? field.load_bf.slot_size : field.store_bf.src_size;
    uint32_t offset = load ? field.load_bf.offset : field.store_bf.offset;
    uint32_t alignment = load ? field.load_bf.align_log2 : field.store_bf.align_log2;
    uint32_t byte_offset = bit_offset / 8;
    if(bytes > size || byte_offset > UINT32_MAX - offset) return 0;
    alignment = cc_offset_alignment(alignment, byte_offset);
    if(load){
        *op = (CiOp){.load = {
            .is_volatile = field.load_bf.is_volatile,
            .kind = CI_OP_LOAD,
            .slot = field.load_bf.slot,
            .slot_size = bytes,
            .src = field.load_bf.src,
            .offset = offset + byte_offset,
            .align_log2 = alignment,
            .loc = field.load_bf.loc,
        }};
        if(bytes < size){
            int err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
            if(err) return err;
            *op = (CiOp){.convert = {
                .kind = CI_OP_CONVERT,
                .slot = field.load_bf.slot,
                .slot_size = size,
                .src = field.load_bf.slot,
                .src_size = bytes,
                .is_unsigned = !field.load_bf.is_signed,
                .loc = field.load_bf.loc,
            }};
        }
    }
    else {
        *op = (CiOp){.store = {
            .is_volatile = field.store_bf.is_volatile,
            .kind = CI_OP_STORE,
            .slot = field.store_bf.slot,
            .src = field.store_bf.src,
            .src_size = bytes,
            .offset = offset + byte_offset,
            .align_log2 = alignment,
            .loc = field.store_bf.loc,
        }};
    }
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
            .is_volatile = lv->type.is_volatile,
            .kind = ci_expr_bitfield_needs_bytes(lv, size) ? CI_OP_LOAD_BITFIELD_BYTES : CI_OP_LOAD_BITFIELD,
            .slot = dest,
            .slot_size = size,
            .src = a.slot,
            .offset = a.disp, .align_log2 = a.align_log2,
            .bit_offset = cc_expr_field_bit_offset(lv),
            .bit_width = cc_expr_field_bit_width(lv),
            .is_signed = !ccqt_is_unsigned(lv->type, ctx->char_is_unsigned),
            .loc = lv->loc,
        }
    };
    return ci_lower_whole_byte_bitfield(ctx, op);
}

static
int
ci_emit_store_bitfield(CiLowerCtx* ctx, const CcExpr* lv, CiLowerAddr a, uint32_t src, uint32_t size){
    CiOp* op;
    int err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
    if(err) return err;
    *op = (CiOp){
        .store_bf = {
            .is_volatile = lv->type.is_volatile,
            .kind = ci_expr_bitfield_needs_bytes(lv, size) ? CI_OP_STORE_BITFIELD_BYTES : CI_OP_STORE_BITFIELD,
            .slot = a.slot,
            .src = src,
            .src_size = size,
            .offset = a.disp, .align_log2 = a.align_log2,
            .bit_offset = cc_expr_field_bit_offset(lv),
            .bit_width = cc_expr_field_bit_width(lv),
            .loc = lv->loc,
        }
    };
    return ci_lower_whole_byte_bitfield(ctx, op);
}

static
_Bool
ci_frame_lvalue(CiInterpreter* ci, CiLowerCtx* ctx, const CcExpr* lv, uint32_t* offset){
    if(ccqt_kind(lv->type) == CC_VLA) return 0;
    if(lv->type.is_volatile) return 0;
    switch((uint32_t)lv->kind){
        case CC_EXPR_VARIABLE:
            if(!lv->var->automatic) return 0;
            if(lv->var->constexpr_ && !lv->var->mutable_storage) return 0;
            if(lv->type.is_atomic) return 0;
            *offset = ci_variable_slot(ctx,lv->var);
            return 1;
        case CC_EXPR_DOT:{
            if(cc_expr_field_bit_width(lv)) return 0;
            if(lv->type.is_atomic) return 0;
            uint32_t base;
            if(!ci_frame_lvalue(ci, ctx, lv->values[0], &base)) return 0;
            uint64_t field_offset;
            if(cc_expr_field_offset(ci_target(ci), lv, &field_offset)) return 0;
            *offset = base + (uint32_t)field_offset;
            return 1;
        }
        default:
            return 0;
    }
}

static
int
ci_pointer_offset(CiInterpreter* ci, CiLowerCtx* ctx, CcExpr* e, uint64_t initial, uint64_t max_offset, CcExpr*_Nonnull*_Nonnull base, uint64_t* displacement){
    *base = e;
    *displacement = initial;
    uint64_t offset = initial;
    CcQualType type = e->type;
    while((e->kind == CC_EXPR_ADD || e->kind == CC_EXPR_SUB)
        && ccqt_kind(e->type) == CC_POINTER && e->type.unqual == type.unqual
        && !e->type.is_volatile && !e->type.is_atomic
        && (ctx->ptr_size == 4 || ctx->ptr_size == 8)){
        CcExpr* pointer = e->lhs;
        CcExpr* index = e->values[0];
        if(e->kind == CC_EXPR_ADD && ccqt_kind(index->type) == CC_POINTER){
            pointer = e->values[0];
            index = e->lhs;
        }
        if(pointer->type.unqual != type.unqual || !ccqt_is_integer(index->type))
            break;
        if(ccqt_kind(ccqt_as_ptr(type)->pointee) == CC_VLA) break;
        CiFoldValue value;
        int err = ci_fold_expr(ci, ctx, index, &value);
        if(err > 0) return err;
        if(err == FOLD_FAIL) break;
        uint32_t elem_size;
        err = cc_sizeof_as_uint(&ci->parser, ccqt_as_ptr(type)->pointee, e->loc, &elem_size);
        if(err) return err;
        uint64_t bytes = ci_fold_offset(ctx, &value, elem_size);
        offset = e->kind == CC_EXPR_ADD ? offset + bytes : offset - bytes;
        if(ctx->ptr_size == 4) offset = (uint32_t)offset;
        e = pointer;
        if(offset <= max_offset){
            *base = e;
            *displacement = offset;
        }
    }
    return 0;
}

static
int
ci_lower_pointer_addr(CiInterpreter* ci, CiLowerCtx* ctx, CcExpr* e, uint32_t field_offset, _Bool snapshot, CiLowerAddr* out){
    uint32_t alignment;
    int err = ci_pointer_alignment(ci, ctx, e, &alignment);
    if(err) return err;
    alignment = cc_offset_alignment(alignment, field_offset);
    CcExpr* base;
    uint64_t displacement;
    err = ci_pointer_offset(ci, ctx, e, field_offset, UINT32_MAX, &base, &displacement);
    if(err) return err;
    CiLowerVal value;
    err = ci_lower_expr(ci, ctx, base, CI_NO_SLOT, &value);
    if(err) return err;
    if(snapshot){
        err = ci_preserve_value(ci, ctx, base, &value);
        if(err) return err;
    }
    *out = (CiLowerAddr){.slot=value.slot, .disp=(uint32_t)displacement, .align_log2=alignment};
    return 0;
}

static
int
ci_lower_addr_inner(CiInterpreter* ci, CiLowerCtx* ctx, CcExpr* lv, _Bool one_past_ok, CiLowerAddr* out){
    int err;
    if(lv->kind == CC_EXPR_CAST
    && ccqt_kind(lv->type) == CC_VLA
    && cc_conversion_preserves_representation(lv->lhs->type, lv->type))
        return ci_lower_addr(ci, ctx, lv->lhs, one_past_ok, out);
    if(lv->kind == CC_EXPR_ASSIGN && ccqt_kind(lv->type) == CC_VLA)
        return ci_lower_vla_assign(ci, ctx, lv, out);
    if(lv->kind == CC_EXPR_VARIABLE && ccqt_kind(lv->type) == CC_VLA){
        if(!lv->var->automatic){
            CcExpr ref = *lv;
            CcPointer pointer = {.kind=CC_POINTER, .pointee=ccqt_as_vla(lv->type)->element};
            ref.type = (CcQualType){.bits=(uintptr_t)&pointer};
            CiLowerVal value;
            err = ci_lower_expr(ci, ctx, &ref, CI_NO_SLOT, &value);
            if(err) return err;
            *out = (CiLowerAddr){.slot=value.slot};
            return 0;
        }
        *out = (CiLowerAddr){.slot=ci_variable_slot(ctx,lv->var)};
        return 0;
    }
    CcParser* p = &ci->parser;
    uint32_t frame_off;
    if(ci_frame_lvalue(ci, ctx, lv, &frame_off)){
        // a local (or a member chain of one) lives in a slot
        uint32_t aslot;
        err = ci_alloc_slot(ctx, ctx->ptr_size, ctx->ptr_size, &aslot);
        if(err) return err;
        CiOp* op;
        err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
        if(err) return err;
        *op = (CiOp){
            .slot_addr = {
                .kind = CI_OP_SLOT_ADDR,
                .slot = aslot,
                .slot_size = ctx->ptr_size,
                .src = frame_off,
                .loc = lv->loc,
            }
        };
        out->slot = aslot;
        out->disp = 0;
        return 0;
    }
    switch((uint32_t)lv->kind){
        case CC_EXPR_BLOB:
        case CC_EXPR_VALUE:{
            if(lv->kind != CC_EXPR_BLOB && !ccqt_is_fixed_sequence(lv->type))
                break; // not an lvalue; materialized below
            uint32_t aslot;
            err = ci_alloc_slot(ctx, ctx->ptr_size, ctx->ptr_size, &aslot);
            if(err) return err;
            CiOp* op;
            err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
            if(err) return err;
            *op = (CiOp){
                .constant = {
                    .kind = CI_OP_CONST,
                    .bt_kind = (uint32_t)(ccqt_is_basic(cc_expr_array_element(lv))?ccqt_basic_kind(cc_expr_array_element(lv)):CCBT_nullptr_t),
                    .is_anon_array = 1,
                    .immsize = ctx->ptr_size,
                    .slot = aslot,
                    .immediate = {
                        (uint64_t)cc_expr_array_data(lv),
                        cc_expr_array_count(lv),
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
            if(var->automatic && var->constexpr_ && !var->mutable_storage)
                return ci_lower_materialize_addr(ci, ctx, lv, out);
            if(var->automatic){
                // atomic locals miss the frame fast path above, but their
                // storage is a slot all the same
                uint32_t aslot;
                err = ci_alloc_slot(ctx, ctx->ptr_size, ctx->ptr_size, &aslot);
                if(err) return err;
                CiOp* op;
                err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
                if(err) return err;
                *op = (CiOp){
                    .slot_addr = {
                        .kind = CI_OP_SLOT_ADDR,
                        .slot = aslot,
                        .slot_size = ctx->ptr_size,
                        .src = ci_variable_slot(ctx,var),
                        .loc = lv->loc,
                    }
                };
                out->slot = aslot;
                out->disp = 0;
                return 0;
            }
            err = ci_deps_add_var(&ctx->deps, ctx->a, var, CI_VAR_DEP_USED);
            if(err) return CI_OOM_ERROR;
            uint32_t aslot;
            err = ci_alloc_slot(ctx, ctx->ptr_size, ctx->ptr_size, &aslot);
            if(err) return err;
            CiOp* op;
            err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
            if(err) return err;
            *op = (CiOp){
                .var_addr = {
                    .kind = var->thread_local_ ? CI_OP_TLS_ADDR : CI_OP_VAR_ADDR,
                    .slot = aslot,
                    .slot_size = ctx->ptr_size,
                    .var = var,
                    .loc = lv->loc,
                }
            };
            out->slot = aslot;
            out->disp = 0;
            return 0;
        }
        case CC_EXPR_DEREF:{
            return ci_lower_pointer_addr(ci, ctx, lv->lhs, 0, 1, out);
        }
        case CC_EXPR_ARROW:{
            if(cc_expr_field_bit_width(lv))
                return ci_unreachable(ci, lv->loc, "addr of bitfield");
            uint64_t field_offset;
            err = cc_expr_field_offset(ci_target(ci), lv, &field_offset);
            if(err) return err;
            return ci_lower_pointer_addr(ci, ctx, lv->values[0], (uint32_t)field_offset, 1, out);
        }
        case CC_EXPR_DOT:{
            if(cc_expr_field_bit_width(lv))
                return ci_unreachable(ci, lv->loc, "addr of bitfield");
            CcExpr* base = lv->values[0];
            if(base->is_lvalue)
                err = ci_lower_addr(ci, ctx, base, 0, out); // base must be a valid object
            else
                err = ci_lower_materialize_addr(ci, ctx, base, out);
            if(err) return err;
            uint64_t field_offset;
            err = cc_expr_field_offset(ci_target(ci), lv, &field_offset);
            if(err) return err;
            if(field_offset > UINT32_MAX-out->disp){
                CiLowerVal address;
                err = ci_addr_to_value(ctx, *out, CI_NO_SLOT, ctx->ptr_size, ci_target(ci)->alignof_[CCBT_nullptr_t], lv->loc, &address);
                if(err) return err;
                *out = (CiLowerAddr){.slot=address.slot};
            }
            out->disp += field_offset;
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
            uint32_t elem_sz = 1;
            _Bool constant_stride = !cc_type_sizeof_complete(ci_target(ci), lv->type, &elem_sz, NULL);
            if(!constant_stride) elem_sz = 1;
            CiOp* op;
            CiFoldValue index;
            int fold_fail = ci_fold_expr(ci, ctx, idx, &index);
            if(fold_fail > 0) return fold_fail;
            if(!constant_stride) fold_fail = FOLD_FAIL;
            uint32_t base_temp = ctx->temp;
            uint32_t base_ptr;     // slot holding an 8-byte base pointer
            uint32_t base_disp = 0;// offset folded into the result displacement
            _Bool do_check = 0;
            uint32_t len_slot = 0;
            if(bk == CC_POINTER){
                CiLowerVal b;
                err = ci_lower_expr(ci, ctx, base, CI_NO_SLOT, &b);
                if(err) return err;
                if(fold_fail == 0){
                    // Preserve the evaluated address if a later RHS changes
                    // a local pointer whose storage ci_lower_expr could reuse.
                    err = ci_preserve_value(ci, ctx, base, &b);
                    if(err) return err;
                }
                base_ptr = b.slot;
            }
            else if(bk == CC_VLA){
                CiLowerAddr ba;
                err = ci_lower_addr(ci, ctx, base, 0, &ba);
                if(err) return err;
                base_ptr = ba.slot;
                base_disp = ba.disp;
                CiLowerVal length;
                err = ci_lower_expr(ci, ctx, ccqt_as_vla(base->type)->bound, CI_NO_SLOT, &length);
                if(err) return err;
                len_slot = length.slot;
                do_check = 1;
            }
            else if(bk == CC_ARRAY || bk == CC_VECTOR){
                CiLowerAddr ba;
                err = ci_lower_addr(ci, ctx, base, 0, &ba);
                if(err) return err;
                base_ptr = ba.slot;
                base_disp = ba.disp;
                // Elide the check only for genuine flexible-array-member idioms:
                // a C99 FLA (incomplete), a zero-length member, or a length-1
                // member at the end of a struct (the struct hack).
                _Bool skip = ccqt_array_incomplete(base->type);
                if(!skip && ccqt_sequence_length(base->type) <= 1
                    && (base->kind == CC_EXPR_DOT || base->kind == CC_EXPR_ARROW)){
                    if(ccqt_sequence_length(base->type) == 0){
                        skip = 1;
                    }
                    else {
                        // length 1: skip only if it is the struct's last field
                        CcQualType st = base->values[0]->type;
                        if(base->kind == CC_EXPR_ARROW && ccqt_kind(st) == CC_POINTER)
                            st = ccqt_as_ptr(st)->pointee;
                        if(ccqt_kind(st) == CC_STRUCT){
                            CcStruct* s = ccqt_as_struct(st);
                            uint64_t field_offset;
                            err = cc_expr_field_offset(ci_target(ci), base, &field_offset);
                            if(err) return err;
                            if(s->field_count && s->fields
                                && s->fields[s->field_count-1].offset == field_offset)
                                skip = 1;
                        }
                    }
                }
                if(!skip && fold_fail == 0){
                    _Bool negative = !ccqt_is_unsigned(idx->type, ctx->char_is_unsigned) && ci_read_int(index.bits, index.sz) < 0;
                    uint64_t i = ci_read_uint(index.bits, index.sz);
                    // Invalid constant accesses must still fail at runtime,
                    // including in code reached by a goto or a later call.
                    skip = !negative && (i < ccqt_sequence_length(base->type) || (one_past_ok && i == ccqt_sequence_length(base->type)));
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
                            .immediate = {ccqt_sequence_length(base->type)},
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
                    err = ci_emit_copy(ctx, snapshot, base_ptr, ctx->ptr_size, lv->loc);
                    if(err) return err;
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
            if(!constant_stride){
                err = ci_scale_pointer_index(ci, ctx, lv->type, lv->loc, widx, &widx);
                if(err) return err;
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
    if(!v.borrowed){
        ctx->materialized_addresses++;
        uint32_t end = v.slot + v.size; // the allocation already checked overflow
        if(end > ctx->materialized_end) ctx->materialized_end = end;
    }
    uint32_t aslot;
    err = ci_alloc_slot(ctx, ctx->ptr_size, ctx->ptr_size, &aslot);
    if(err) return err;
    CiOp* op;
    err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
    if(err) return err;
    *op = (CiOp){
        .slot_addr = {
            .kind = CI_OP_SLOT_ADDR,
            .slot = aslot,
            .slot_size = ctx->ptr_size,
            .src = v.slot,
            .loc = e->loc,
        }
    };
    out->slot = aslot;
    out->disp = 0;
    out->align_log2 = ci_frame_alignment(ctx, v.slot);
    return 0;
}

static
_Bool
ci_falu_type(CcQualType t){
    if(!ccqt_is_basic(t)) return 0;
    CcBasicTypeKind k = ccqt_basic_kind(t);
    return k == CCBT_fp16 || k == CCBT_float16 || k == CCBT_bfloat16 || k == CCBT_float || k == CCBT_double || k == CCBT_long_double || k == CCBT_float128;
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
ci_integer_range(CiInterpreter* ci, CiLowerCtx* ctx, CcExpr* e, CiUint128* min, CiUint128* max){
    uint32_t size;
    int err = cc_sizeof_as_uint(&ci->parser, e->type, e->loc, &size);
    if(err) return err;
    if(!size || size > 16) return FOLD_FAIL;
    unsigned width = ccqt_is_bool(e->type) ? 1 : size*8;
    _Bool uns = ccqt_is_bool(e->type) || ccqt_is_unsigned(e->type, ctx->char_is_unsigned);
    for(;;){
        unsigned field_width = cc_expr_field_bit_width(e);
        if(field_width && field_width < width) width = field_width;
        if(e->kind != CC_EXPR_CAST || !ccqt_is_integer(e->lhs->type)) break;
        CcExpr* child = e->lhs;
        uint32_t child_size;
        err = cc_sizeof_as_uint(&ci->parser, child->type, child->loc, &child_size);
        if(err) return err;
        if(!child_size || child_size > 16) break;
        unsigned child_width = ccqt_is_bool(child->type) ? 1 : child_size*8;
        field_width = cc_expr_field_bit_width(child);
        if(field_width && field_width < child_width) child_width = field_width;
        _Bool child_uns = ccqt_is_bool(child->type) || ccqt_is_unsigned(child->type, ctx->char_is_unsigned);
        if(child_width > width || (uns && !child_uns) || (!uns && child_uns && child_width == width))
            break;
        width = child_width;
        uns = child_uns;
        e = child;
    }
    CiUint128 ones = ci_uint128_from_int64(-1);
    if(uns){
        *min = ci_uint128_from_uint64(0);
        *max = ci_uint128_shr(ones, 128-width);
    }
    else {
        *max = width == 1 ? ci_uint128_from_uint64(0) : ci_uint128_shr(ones, 129-width);
        *min = ci_uint128_xor(*max, ones);
    }
    return 0;
}

static
int
ci_compare_type_limit(CiInterpreter* ci, CiLowerCtx* ctx, CcExpr* e, CcExpr*_Nonnull*_Nonnull evaluated, _Bool* truth){
    CcExprKind kind = e->kind;
    if(kind != CC_EXPR_LT && kind != CC_EXPR_LE && kind != CC_EXPR_GT && kind != CC_EXPR_GE && kind != CC_EXPR_EQ && kind != CC_EXPR_NE)
        return FOLD_FAIL;
    CcExpr* operand = e->lhs;
    CcExpr* constant = e->values[0];
    if(!ccqt_is_basic(operand->type) || !ccqt_is_integer(operand->type) || operand->type.unqual != constant->type.unqual)
        return FOLD_FAIL;
    CiFoldValue value;
    int err = ci_fold_expr(ci, ctx, constant, &value);
    if(err > 0) return err;
    if(err == FOLD_FAIL){
        err = ci_fold_expr(ci, ctx, operand, &value);
        if(err) return err;
        operand = constant;
        switch((uint32_t)kind){
            case CC_EXPR_LT: kind = CC_EXPR_GT; break;
            case CC_EXPR_LE: kind = CC_EXPR_GE; break;
            case CC_EXPR_GT: kind = CC_EXPR_LT; break;
            case CC_EXPR_GE: kind = CC_EXPR_LE; break;
        }
    }
    if(!value.sz || value.sz > 16) return FOLD_FAIL;
    CiUint128 min, max;
    err = ci_integer_range(ci, ctx, operand, &min, &max);
    if(err) return err;
    CiUint128 bits;
    ci_uint128_read(&bits, value.bits, value.sz);
    _Bool uns = ccqt_is_unsigned(operand->type, ctx->char_is_unsigned) || ccqt_is_bool(operand->type);
    if(!uns && value.sz < 16){
        unsigned shift = 128-value.sz*8;
        bits = ci_uint128_from_int128(ci_int128_shr(ci_int128_from_uint128(ci_uint128_shl(bits, shift)), shift));
    }
    _Bool below = uns ? ci_uint128_lt(bits, min) : ci_int128_lt(ci_int128_from_uint128(bits), ci_int128_from_uint128(min));
    _Bool above = uns ? ci_uint128_gt(bits, max) : ci_int128_gt(ci_int128_from_uint128(bits), ci_int128_from_uint128(max));
    _Bool at_min = ci_uint128_eq(bits, min), at_max = ci_uint128_eq(bits, max);
    switch((uint32_t)kind){
        case CC_EXPR_LT: case CC_EXPR_GE:
            if(below || at_min) *truth = kind == CC_EXPR_GE;
            else if(above) *truth = kind == CC_EXPR_LT;
            else return FOLD_FAIL;
            break;
        case CC_EXPR_GT: case CC_EXPR_LE:
            if(above || at_max) *truth = kind == CC_EXPR_LE;
            else if(below) *truth = kind == CC_EXPR_GT;
            else return FOLD_FAIL;
            break;
        case CC_EXPR_EQ: case CC_EXPR_NE:
            if(!below && !above) return FOLD_FAIL;
            *truth = kind == CC_EXPR_NE;
            break;
    }
    *evaluated = operand;
    return 0;
}

static
int
ci_lower_branch(CiInterpreter* ci, CiLowerCtx* ctx, CcExpr* cond, _Bool when_true, SrcLoc loc, uint32_t* chain){
    int err;
    _Bool truth;
    int folded = ci_fold_condition(ci, ctx, cond, &truth);
    if(folded > 0) return folded;
    if(folded == FOLD_FAIL){
        CcExpr* evaluated;
        folded = ci_compare_type_limit(ci, ctx, cond, &evaluated, &truth);
        if(folded > 0) return folded;
        if(!folded){
            err = ci_lower_expr_discard(ci, ctx, evaluated);
            if(err) return err;
        }
    }
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
    if(v->canonical && !negate && v->size == dest_size){
        return ci_emit_copy(ctx, dest, v->slot, dest_size, loc);
    }
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
            .float_width = ccqt_is_narrow_float(src_type) ? 16 : ci_float_width(src_type, ctx->ldbl_fmt),
            .negate = negate,
            .loc = loc,
        }
    };
    return 0;
}

static
int
ci_alloc_slot(CiLowerCtx* ctx, uint32_t sz, uint32_t align, uint32_t* slot){
    if(align > ctx->frame_alignment) ctx->frame_alignment = align;
    uint32_t frm = ctx->temp;
    if(frm < ctx->materialized_end) frm = ctx->materialized_end;
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
    // Switches pass UINT32_MAX because they cannot be continue targets.
    for(size_t i = start; i < ctx->backpatches.count; i++){
        CiBackpatchTarget* t = &ctx->backpatches.data[i];
        if((t->kind == CI_BP_BREAK || t->kind == CI_BP_CONTINUE) && --t->levels)
            continue;
        if(t->kind == CI_BP_BREAK){
            *(uint32_t*)((char*)ctx->out->data+t->byteoffset) = break_target;
            t->kind = CI_BP_NONE;
        }
        else if(t->kind == CI_BP_CONTINUE && continue_target != UINT32_MAX){
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
int
ci_cmp_switch_entry(void*_Null_unspecified ctx, const void* a, const void* b){
    uint64_t sign = *(const _Bool*)ctx ? 0 : UINT64_C(1) << 63;
    const CcSwitchEntry* ea = a;
    const CcSwitchEntry* eb = b;
    if((ea->low ^ sign) < (eb->low ^ sign)) return -1;
    if((ea->low ^ sign) > (eb->low ^ sign)) return 1;
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

static
int
ci_lower_func(CiInterpreter* ci, CcFunc* f, CiLowerDeps* deps){
    DRP_PROFILE_BEGIN(profile, "lower.function");
    int err;
    if(f->interp_ops) return 0;
    if(!f->parsed)
        return ci_ice(ci, f->loc, "lowering function '%s' before it was parsed", f->name->data);
    Allocator al = ci_allocator(ci);
    CiFuncOps* ops = Allocator_zalloc(al, 1, sizeof *ops, _Alignof(CiFuncOps));
    if(!ops) return CI_OOM_ERROR;
    f->interp_lowering = 1;
    f->frame_size = 0;
    AtomMap(uintptr_t) labels = {0};
    const CcTargetConfig* t = ci_target(ci);
    CiFoldCache fold_cache = {0};
    CiLowerCtx ctx = {
        .a = al,
        .out = &ops->code,
        .labels = &labels,
        .frame_size = &f->frame_size,
        .size_size = t->sizeof_[t->size_type],
        .ptr_size = t->sizeof_[CCBT_nullptr_t],
        .char_is_unsigned = !t->char_is_signed,
        .ldbl_fmt = t->long_double_format,
        .fold_cache = &fold_cache,
        .function = f,
    };
    err = nc_plan_create(al, t, f->type, &ops->abi);
    if(err) goto finally;
    ctx.function_abi = ops->abi;
    if(ops->abi->result.mode == NC_ABI_INDIRECT){
        err = ci_alloc_slot(&ctx, ctx.ptr_size, ctx.ptr_size, &ctx.result_pointer_slot);
        if(err) goto finally;
        CiOp save = {.abi_move = {
            .kind = CI_OP_ABI_MOVE,
            .incoming = 1,
            .operation = CI_ABI_COPY,
            .size = ctx.ptr_size,
            .source_size = ctx.ptr_size,
            .slot = ctx.result_pointer_slot,
            .reg = ci_abi_resolve_register(t->target, (CiAbiRegister){.bank = CI_ABI_INDIRECT_RESULT}),
            .loc = f->loc,
        }};
        err = ma_push(CiOp)(&ops->code, al, save);
        if(err) goto finally;
    }
    uint32_t native_index = 0;
    for(size_t i = 0; i < f->params.count; i++){
        CcVariable* var = f->params.data[i].decl;
        uint32_t size, align, slot;
        err = cc_sizeof_as_uint(&ci->parser, var->type, var->loc, &size);
        if(err) goto finally;
        err = cc_alignof_as_uint(&ci->parser, var->type, var->loc, &align);
        if(err) goto finally;
        if(var->alignment > align) align = var->alignment;
        err = ci_alloc_slot(&ctx, size, align, &slot);
        if(err) goto finally;
        err = ci_bind_slot(&ctx, var, slot);
        if(err) goto finally;
        ci_inline_binding(&ctx, var)->parameter = 1;
        f->params.data[i].offset = slot;
        f->params.data[i].sz = size;
        f->params.data[i].native_index = ci_arg_is_ignored(t->target, var->type) ? UINT32_MAX : native_index++;
        err = ci_lower_abi_argument(&ops->code, al, ops->abi, (uint32_t)i, slot, 1, f->loc);
        if(err) goto finally;
    }
    DRP_PROFILE_BEGIN(body_profile, "lower.function_body");
    err = ci_lower_stmt(ci, &ctx, f->body_tree);
    DRP_PROFILE_END(body_profile);
    if(err) goto finally;
    DRP_PROFILE_BEGIN(finish_profile, "lower.function_finalize");
    err = ci_lower_resolve_gotos(ci, &ctx);
    if(err) goto finally;
    err = ci_lower_native_signature(ci, al, f->type, &ops->native_type);
    if(err) goto finally;
    err = ci_lower_deps_merge(deps, &ctx.deps, al);
    DRP_PROFILE_END(finish_profile);
    finally:
    f->interp_lowering = 0;
    ci_bindings_cleanup(&ctx);
    ci_fold_cache_cleanup(&fold_cache, al);
    ci_lower_deps_cleanup(&ctx.deps, al);
    ma_cleanup(CiBackpatchTarget)(&ctx.backpatches, al);
    if(labels.data)
        Allocator_free(al, labels.data, AM_alloc_size(labels.cap), 1, _Alignof(AtomMapItem));
    if(err){
        for(size_t i = 0; i < ops->code.count; i++){
            CiOp* o = &ops->code.data[i];
            if(o->kind == CI_OP_SWITCH && o->switch_.table)
                Allocator_free_extra(al, o->switch_.table, o->switch_.table->count, sizeof *o->switch_.table->data, sizeof *o->switch_.table, _Alignof(CiSwitchTable));
        }
        ma_cleanup(CiOp)(&ops->code, al);
        Allocator_free(al, ops, 1, sizeof *ops, _Alignof(CiFuncOps));
        return err;
    }
    ops->frame_alignment = ctx.frame_alignment > 16 ? ctx.frame_alignment : 16;
    f->interp_ops = ops;
    return 0;
    DRP_PROFILE_END(profile);
}

static
int
ci_lower_nodes(CiInterpreter* ci, Parray(CcStmtNode)* nodes, size_t* lowered, Marray(CiOp)* ops, AtomMap(uintptr_t)* labels, uint32_t* slot_size, uint32_t* frame_alignment, CiLowerDeps* deps){
    int err = 0;
    size_t old_count = ops->count;
    uint32_t old_size = *slot_size;
    AtomMap(uintptr_t) old_labels = *labels;
    Allocator al = ci_allocator(ci);
    if(labels->cap){
        void* data = Allocator_alloc(al, AM_alloc_size(labels->cap), 1, _Alignof(AtomMapItem));
        if(!data) return CI_OOM_ERROR;
        old_labels.data = data;
        memcpy(old_labels.data, labels->data, AM_alloc_size(labels->cap));
    }
    const CcTargetConfig* t = ci_target(ci);
    CiFoldCache fold_cache = {0};
    CiLowerCtx ctx = {
        .a = ci_allocator(ci),
        .out = ops,
        .labels = labels,
        .frame_size = slot_size,
        .frame_alignment = *frame_alignment,
        .size_size = t->sizeof_[t->size_type],
        .ptr_size = t->sizeof_[CCBT_nullptr_t],
        .char_is_unsigned = !t->char_is_signed,
        .ldbl_fmt = t->long_double_format,
        .fold_cache = &fold_cache,
    };
    for(size_t i = *lowered; i < nodes->count; i++){
        err = ci_lower_stmt(ci, &ctx, nodes->data[i]);
        if(err) goto cleanup;
    }
    err = ci_lower_resolve_gotos(ci, &ctx);
    if(err) goto cleanup;
    err = ci_lower_deps_merge(deps, &ctx.deps, ctx.a);
    cleanup:
    ci_fold_cache_cleanup(&fold_cache, al);
    ci_lower_deps_cleanup(&ctx.deps, ctx.a);
    ma_cleanup(CiBackpatchTarget)(&ctx.backpatches, ctx.a);
    if(err){
        for(size_t i = old_count; i < ops->count; i++){
            CiOp* op = &ops->data[i];
            if(op->kind == CI_OP_SWITCH && op->switch_.table)
                Allocator_free_extra(al, op->switch_.table, op->switch_.table->count, sizeof *op->switch_.table->data, sizeof *op->switch_.table, _Alignof(CiSwitchTable));
        }
        ops->count = old_count;
        *slot_size = old_size;
        if(labels->cap)
            Allocator_free(al, labels->data, AM_alloc_size(labels->cap), 1, _Alignof(AtomMapItem));
        *labels = old_labels;
    }
    else {
        *frame_alignment = ctx.frame_alignment > 16? ctx.frame_alignment : 16;
        *lowered = nodes->count;
        if(old_labels.cap)
            Allocator_free(al, old_labels.data, AM_alloc_size(old_labels.cap), 1, _Alignof(AtomMapItem));
    }
    return err;
}

static
int
ci_resize_toplevel_slots(CiInterpreter* ci){
    uint32_t alignment = 16;
    if(ci->root.frame_alignment > 16)
        alignment = ci->root.frame_alignment;
    if(ci->root.slot_size > ci->toplevel_slots_cap || (ci->toplevel_slots && alignment > ci->toplevel_alloc_alignment)){
        uint32_t new_cap = ci->root.slot_size > ci->toplevel_slots_cap? ci->root.slot_size : ci->toplevel_slots_cap;
        void* slots = Allocator_zalloc(ci_allocator(ci), new_cap, 1, alignment);
        if(!slots) return CI_OOM_ERROR;
        if(ci->toplevel_slots){
            memcpy(slots, ci->toplevel_slots, ci->toplevel_slots_cap);
            Allocator_free(ci_allocator(ci), ci->toplevel_slots, ci->toplevel_slots_cap, 1, ci->toplevel_alloc_alignment);
        }
        ci->toplevel_slots = slots;
        ci->toplevel_slots_cap = new_cap;
        ci->toplevel_alloc_alignment = alignment;
    }
    return 0;
}

static
int
ci_lower_toplevel(CiInterpreter* ci, CiLowerDeps* deps){
    DRP_PROFILE_BEGIN(profile, "lower.toplevel");
    int err = ci_lower_module(ci, &ci->root, deps);
    if(err) return err;
    err = ci_resize_toplevel_slots(ci);
    if(err) return err;
    ci->top_frame.ops = ci->root.ops.data;
    ci->top_frame.op_count = ci->root.ops.count;
    ci->top_frame.slots = ci->toplevel_slots;
    return 0;
    DRP_PROFILE_END(profile);
}

static
int
ci_lower_module(CiInterpreter* ci, CiModule* module, CiLowerDeps* deps){
    return ci_lower_nodes(
        ci,
        &module->cc.nodes,
        &module->lowered,
        &module->ops,
        &module->labels,
        &module->slot_size,
        &module->frame_alignment,
        deps
    );
}

static
void
ci_lowered_expr_cleanup(CiLoweredExpr* code, Allocator al){
    for(size_t i = 0; i < code->ops.count; i++){
        CiOp* op = &code->ops.data[i];
        if(op->kind == CI_OP_SWITCH && op->switch_.table)
            Allocator_free_extra(al, op->switch_.table, op->switch_.table->count, sizeof *op->switch_.table->data, sizeof *op->switch_.table, _Alignof(CiSwitchTable));
    }
    ma_cleanup(CiOp)(&code->ops, al);
    *code = (CiLoweredExpr){0};
}

static
int
ci_lower_standalone_expr(CiInterpreter* ci, CcExpr* expr, CiLoweredExpr* code, CiLowerDeps* deps){
    Allocator al = ci_allocator(ci);
    AtomMap(uintptr_t) labels = {0};
    const CcTargetConfig* t = ci_target(ci);
    CiFoldCache fold_cache = {0};
    CiLowerCtx ctx = {
        .a = al,
        .out = &code->ops,
        .labels = &labels,
        .frame_size = &code->frame_size,
        .size_size = t->sizeof_[t->size_type],
        .ptr_size = t->sizeof_[CCBT_nullptr_t],
        .char_is_unsigned = !t->char_is_signed,
        .ldbl_fmt = t->long_double_format,
        .fold_cache = &fold_cache,
    };
    CiLowerVal value = {0};
    _Bool is_void = ccqt_bt_eq(expr->type, CCBT_void);
    int err = is_void ? ci_lower_expr_discard(ci, &ctx, expr) : ci_lower_expr(ci, &ctx, expr, CI_NO_SLOT, &value);
    if(err) goto cleanup;
    err = ci_lower_resolve_gotos(ci, &ctx);
    if(err) goto cleanup;
    err = ci_lower_deps_merge(deps, &ctx.deps, al);
    if(err) goto cleanup;
    code->frame_alignment = ctx.frame_alignment > 16 ? ctx.frame_alignment : 16;
    code->value_slot = value.slot;
    code->value_size = value.size;
    cleanup:
    ci_fold_cache_cleanup(&fold_cache, al);
    ci_lower_deps_cleanup(&ctx.deps, al);
    ma_cleanup(CiBackpatchTarget)(&ctx.backpatches, al);
    if(labels.data) Allocator_free(al, labels.data, AM_alloc_size(labels.cap), 1, _Alignof(AtomMapItem));
    if(err) ci_lowered_expr_cleanup(code, al);
    return err;
}

static int ci_static_address(CiInterpreter*, CcExpr*, _Bool, CiStaticReloc*, CiLowerCtx* _Nullable);

typedef struct CiStaticBuffer CiStaticBuffer;
struct CiStaticBuffer {
    unsigned char* bytes;
    uint32_t size;
    Marray(CiStaticReloc) relocs;
    // Runtime templates may only fold expressions whose effects can be elided.
    // NULL selects the more permissive rules for static initialization.
    CiLowerCtx* _Nullable runtime_ctx;
};

static int ci_static_write(CiInterpreter*, CiStaticBuffer*, CcExpr*, uint32_t, uint32_t);
static int ci_static_slice_value(CiInterpreter*, CcExpr*, uint64_t*, CiStaticReloc*, CiLowerCtx*_Nullable);
static int ci_fold_truth(CiLowerCtx*, const CiFoldValue*, _Bool*);
static CiUint128 ci_fold_integer(CiLowerCtx*, const CiFoldValue*);
static int ci_static_fold(CiInterpreter*, CcExpr*, CiFoldValue*);
static int ci_static_truth(CiInterpreter*, CcExpr*, _Bool*, CiLowerCtx* _Nullable);

static
_Bool
ci_foldable_variable(CcExpr* e){
    CcQualType type = e->type;
    while(ccqt_is_fixed_sequence(type)) type = ccqt_qualified_array_element(type);
    type = ccqt_unaligned(type);
    return e->var->initializer && (e->var->constexpr_ || type.is_const)
        && !type.is_volatile && !type.is_atomic;
}

static
CiLowerCtx
ci_static_fold_ctx(CiInterpreter* ci){
    const CcTargetConfig* target = ci_target(ci);
    return (CiLowerCtx){
        .ptr_size = target->sizeof_[CCBT_nullptr_t],
        .char_is_unsigned = !target->char_is_signed,
        .ldbl_fmt = target->long_double_format,
        .static_initializer = 1,
    };
}

static
int
ci_static_numeric_cast(CiInterpreter* ci, CcExpr* e, CiFoldValue* out){
    if(e->kind != CC_EXPR_CAST
    || !(ci_falu_type(e->type)
    || (ccqt_is_integer(e->type) && ci_falu_type(e->lhs->type))))
        return _cc_not_constant_error;
    CiFoldValue from;
    int err = ci_static_fold(ci, e->lhs, &from);
    if(err) return err;
    CiLowerCtx ctx = ci_static_fold_ctx(ci);
    CcExpr source = {.kind=CC_EXPR_VALUE, .type=from.type, .loc=e->loc};
    CcExpr converted = {.kind=CC_EXPR_VALUE, .type=e->type, .loc=e->loc};
    if(ccqt_is_integer(from.type)){
        CiUint128 n = ci_fold_integer(&ctx, &from);
        if(from.sz > 8) cc_expr_set_uinteger128(&source, n);
        else source.uinteger = ci_uint128_lo(n);
    }
    else {
        if(from.sz > sizeof source.data) return _cc_not_constant_error;
        memcpy(source.data, from.bits, from.sz);
    }
    uint32_t size;
    err = cc_sizeof_as_uint(&ci->parser, e->type, e->loc, &size);
    if(err) return err;
    if(!size || size > sizeof out->bits || size > sizeof converted.data) return _cc_not_constant_error;
    err = cc_eval_numeric_cast(&ci->parser, &source, &converted, 1);
    if(err) return err == _cc_overflow_error ? _cc_not_constant_error : err;
    *out = (CiFoldValue){.sz = size, .type = e->type};
    memcpy(out->bits, converted.data, size);
    return 0;
}

static
int
ci_static_float_binary(CiInterpreter* ci, CcExpr* e, CiFoldValue* out){
    if(!e->lhs || !ci_falu_type(e->lhs->type)
        || e->lhs->type.unqual != e->values[0]->type.unqual)
        return _cc_not_constant_error;
    CiLowerCtx ctx = ci_static_fold_ctx(ci);
    CiFoldValue lhs, rhs;
    int err = ci_static_fold(ci, e->lhs, &lhs);
    if(err) return err;
    err = ci_static_fold(ci, e->values[0], &rhs);
    if(err) return err;
    uint32_t size;
    err = cc_sizeof_as_uint(&ci->parser, e->type, e->loc, &size);
    if(err) return err;
    *out = (CiFoldValue){.sz = size, .type = e->type};
    _Bool comparison = 0, result = 0;
    if(ccqt_is_narrow_float(lhs.type)){
        CcExpr a = {.kind = CC_EXPR_VALUE, .type = lhs.type};
        CcExpr b = {.kind = CC_EXPR_VALUE, .type = rhs.type};
        CcExpr value = {.kind = CC_EXPR_VALUE, .type = e->type};
        memcpy(a.data, lhs.bits, 2);
        memcpy(b.data, rhs.bits, 2);
        err = cc_eval_wide_binary(ci_target(ci), e->kind, &a, &b, &value);
        if(err) return err;
        memcpy(out->bits, value.data, size);
        return 0;
    }
    if(ccqt_bt_eq(lhs.type, CCBT_float)){
        float a, b, value = 0;
        memcpy(&a, lhs.bits, sizeof a);
        memcpy(&b, rhs.bits, sizeof b);
        switch((uint32_t)e->kind){
            case CC_EXPR_ADD: value = a + b; break;
            case CC_EXPR_SUB: value = a - b; break;
            case CC_EXPR_MUL: value = a * b; break;
            case CC_EXPR_DIV: value = a / b; break;
            case CC_EXPR_EQ: comparison = 1; result = a == b; break;
            case CC_EXPR_NE: comparison = 1; result = a != b; break;
            case CC_EXPR_LT: comparison = 1; result = a < b; break;
            case CC_EXPR_GT: comparison = 1; result = a > b; break;
            case CC_EXPR_LE: comparison = 1; result = a <= b; break;
            case CC_EXPR_GE: comparison = 1; result = a >= b; break;
            default: return _cc_not_constant_error;
        }
        if(!comparison) memcpy(out->bits, &value, sizeof value);
    }
    else if(ccqt_bt_eq(lhs.type, CCBT_double)
        || (ccqt_bt_eq(lhs.type, CCBT_long_double) && ctx.ldbl_fmt == CC_LONG_DOUBLE_BINARY64)){
        double a, b, value = 0;
        memcpy(&a, lhs.bits, sizeof a);
        memcpy(&b, rhs.bits, sizeof b);
        switch((uint32_t)e->kind){
            case CC_EXPR_ADD: value = a + b; break;
            case CC_EXPR_SUB: value = a - b; break;
            case CC_EXPR_MUL: value = a * b; break;
            case CC_EXPR_DIV: value = a / b; break;
            case CC_EXPR_EQ: comparison = 1; result = a == b; break;
            case CC_EXPR_NE: comparison = 1; result = a != b; break;
            case CC_EXPR_LT: comparison = 1; result = a < b; break;
            case CC_EXPR_GT: comparison = 1; result = a > b; break;
            case CC_EXPR_LE: comparison = 1; result = a <= b; break;
            case CC_EXPR_GE: comparison = 1; result = a >= b; break;
            default: return _cc_not_constant_error;
        }
        if(!comparison) memcpy(out->bits, &value, sizeof value);
    }
    else if(ci_float_width(lhs.type, ctx.ldbl_fmt) == 80){
        CiFloat80 a = ci_float80_read(lhs.bits), b = ci_float80_read(rhs.bits), value = {0};
        switch((uint32_t)e->kind){
            case CC_EXPR_ADD: value = ci_float80_add(a, b); break;
            case CC_EXPR_SUB: value = ci_float80_sub(a, b); break;
            case CC_EXPR_MUL: value = ci_float80_mul(a, b); break;
            case CC_EXPR_DIV: value = ci_float80_div(a, b); break;
            case CC_EXPR_EQ: comparison = 1; result = ci_float80_eq(a, b); break;
            case CC_EXPR_NE: comparison = 1; result = !ci_float80_eq(a, b); break;
            case CC_EXPR_LT: comparison = 1; result = ci_float80_lt(a, b); break;
            case CC_EXPR_GT: comparison = 1; result = ci_float80_lt(b, a); break;
            case CC_EXPR_LE: comparison = 1; result = ci_float80_le(a, b); break;
            case CC_EXPR_GE: comparison = 1; result = ci_float80_le(b, a); break;
            default: return _cc_not_constant_error;
        }
        if(!comparison) ci_float80_write(out->bits, size, value);
    }
    else {
        CiFloat128 a = ci_float128_read(lhs.bits), b = ci_float128_read(rhs.bits), value = {0};
        switch((uint32_t)e->kind){
            case CC_EXPR_ADD: value = ci_float128_add(a, b); break;
            case CC_EXPR_SUB: value = ci_float128_sub(a, b); break;
            case CC_EXPR_MUL: value = ci_float128_mul(a, b); break;
            case CC_EXPR_DIV: value = ci_float128_div(a, b); break;
            case CC_EXPR_EQ: comparison = 1; result = ci_float128_eq(a, b); break;
            case CC_EXPR_NE: comparison = 1; result = !ci_float128_eq(a, b); break;
            case CC_EXPR_LT: comparison = 1; result = ci_float128_lt(a, b); break;
            case CC_EXPR_GT: comparison = 1; result = ci_float128_lt(b, a); break;
            case CC_EXPR_LE: comparison = 1; result = ci_float128_le(a, b); break;
            case CC_EXPR_GE: comparison = 1; result = ci_float128_le(b, a); break;
            default: return _cc_not_constant_error;
        }
        if(!comparison) ci_float128_write(out->bits, size, value);
    }
    if(comparison) ci_write_uint(out->bits, size, result);
    return 0;
}

static
int
ci_static_fold(CiInterpreter* ci, CcExpr* e, CiFoldValue* out){
    CiLowerCtx ctx = ci_static_fold_ctx(ci);
    if(e->kind == CC_EXPR_DOT || e->kind == CC_EXPR_DEREF
        || e->kind == CC_EXPR_ARROW || e->kind == CC_EXPR_SUBSCRIPT
        || ((e->kind == CC_EXPR_INIT_LIST || e->kind == CC_EXPR_COMPOUND_LITERAL)
            && !cc_eval_aggregate_type(e->type))){
        uint32_t size;
        int err = cc_sizeof_as_uint(&ci->parser, e->type, e->loc, &size);
        if(err) return err;
        if(size > sizeof out->bits) return _cc_not_constant_error;
        *out = (CiFoldValue){.sz=size, .type=e->type};
        CiStaticBuffer buffer = {.size=size, .bytes=(unsigned char*)out->bits};
        err = ci_static_write(ci, &buffer, e, 0, size);
        if(!err && buffer.relocs.count) err = _cc_not_constant_error;
        ma_cleanup(CiStaticReloc)(&buffer.relocs, ci_allocator(ci));
        return err;
    }
    if(e->kind == CC_EXPR_TYPE_INTROSPECTION || e->kind == CC_EXPR_SRCLOC_REFLECT){
        CcExpr* value;
        int err = cc_eval_linktime_scalar(&ci->parser, e, &value);
        if(err) return err;
        uint32_t size;
        err = cc_sizeof_as_uint(&ci->parser, e->type, e->loc, &size);
        if(!err && size > sizeof out->bits) err = _cc_not_constant_error;
        if(!err){
            *out = (CiFoldValue){.sz = size, .type = e->type};
            memcpy(out->bits, value->data, size);
        }
        cc_release_expr(&ci->parser, value);
        return err;
    }
    if(cc_eval_pointer_binary_expr(e)){
        int64_t value;
        int err = cc_eval_symbolic_binary(&ci->parser, e, 1, &value);
        if(err) return err;
        uint32_t size;
        err = cc_sizeof_as_uint(&ci->parser, e->type, e->loc, &size);
        if(err) return err;
        *out = (CiFoldValue){.sz = size, .type = e->type};
        ci_write_uint(out->bits, size, (uint64_t)value);
        return 0;
    }
    if(e->kind == CC_EXPR_LOGNOT || e->kind == CC_EXPR_LOGAND || e->kind == CC_EXPR_LOGOR){
        _Bool truth;
        int err = ci_static_truth(ci, e->lhs, &truth, NULL);
        if(err) return err;
        if(e->kind == CC_EXPR_LOGNOT) truth = !truth;
        else if(truth == (e->kind == CC_EXPR_LOGAND)){
            err = ci_static_truth(ci, e->values[0], &truth, NULL);
            if(err) return err;
        }
        uint32_t size;
        err = cc_sizeof_as_uint(&ci->parser, e->type, e->loc, &size);
        if(err) return err;
        *out = (CiFoldValue){.sz = size, .type = e->type};
        ci_write_uint(out->bits, size, truth);
        return 0;
    }
    if(e->kind == CC_EXPR_CAST && ccqt_is_bool(e->type)){
        _Bool truth;
        int err = ci_static_truth(ci, e->lhs, &truth, NULL);
        if(err) return err;
        *out = (CiFoldValue){.sz = 1, .type = e->type};
        ci_write_uint(out->bits, 1, truth);
        return 0;
    }
    if(e->kind == CC_EXPR_CAST && ci_falu_type(e->type)
    && ccqt_is_integer(e->lhs->type))
        return ci_static_numeric_cast(ci, e, out);
    int err = ci_fold_expr(ci, &ctx, e, out);
    if(err == FOLD_FAIL && e->kind == CC_EXPR_CAST && ci_falu_type(e->type))
        return ci_static_numeric_cast(ci, e, out);
    if(err == FOLD_FAIL && e->kind == CC_EXPR_CAST && ccqt_is_integer(e->type) && ci_falu_type(e->lhs->type))
        return ci_static_numeric_cast(ci, e, out);
    if(err == FOLD_FAIL){
        switch((uint32_t)e->kind){
            case CC_EXPR_ADD: case CC_EXPR_SUB: case CC_EXPR_MUL: case CC_EXPR_DIV:
            case CC_EXPR_EQ: case CC_EXPR_NE: case CC_EXPR_LT: case CC_EXPR_GT:
            case CC_EXPR_LE: case CC_EXPR_GE:
                return ci_static_float_binary(ci, e, out);
            default: break;
        }
    }
    return err == FOLD_FAIL ? _cc_not_constant_error : err;
}

static
int
ci_initializer_fold(CiInterpreter* ci, CcExpr* e, CiFoldValue* out, CiLowerCtx* _Nullable runtime_ctx){
    DRP_PROFILE_BEGIN(profile, "initializer.fold");
    if(!runtime_ctx) return ci_static_fold(ci, e, out);
    CiLowerCtx ctx = *runtime_ctx;
    // Static helpers can pass synthesized expressions with shorter lifetimes.
    ctx.fold_cache = NULL;
    int err = ci_fold_expr(ci, &ctx, e, out);
    return err == FOLD_FAIL ? _cc_not_constant_error : err;
    DRP_PROFILE_END(profile);
}

static
int
ci_static_integer(CiInterpreter* ci, CcExpr* e, int64_t* out, CiLowerCtx* _Nullable runtime_ctx){
    CiFoldValue value;
    int err = ci_initializer_fold(ci, e, &value, runtime_ctx);
    if(err) return err;
    if(!ccqt_is_integer(value.type) || value.sz > sizeof value.bits) return _cc_not_constant_error;
    CiLowerCtx ctx = ci_static_fold_ctx(ci);
    CiUint128 n = ci_fold_integer(&ctx, &value);
    _Bool positive = ci_uint128_hi(n) == 0 && ci_uint128_lo(n) <= INT64_MAX;
    _Bool negative = !ccqt_is_unsigned(value.type, ctx.char_is_unsigned)
        && ci_uint128_hi(n) == UINT64_MAX && ci_uint128_lo(n) > INT64_MAX;
    if(!positive && !negative)
        return _cc_not_constant_error;
    *out = (int64_t)ci_uint128_lo(n);
    return 0;
}

static
int
ci_static_truth(CiInterpreter* ci, CcExpr* e, _Bool* out, CiLowerCtx* _Nullable runtime_ctx){
    CiFoldValue value;
    int err = ci_initializer_fold(ci, e, &value, runtime_ctx);
    if(err == _cc_not_constant_error
        && (ccqt_has_pointer_representation(e->type) || ccqt_is_fixed_sequence(e->type) || ccqt_kind(e->type) == CC_FUNCTION)){
        CiStaticReloc address = {.kind = CI_STATIC_RELOC_ABSOLUTE};
        err = ci_static_address(ci, e, 0, &address, runtime_ctx);
        if(!err) *out = address.kind != CI_STATIC_RELOC_ABSOLUTE || address.addend != 0;
        return err;
    }
    if(err) return err;
    CiLowerCtx ctx = runtime_ctx ? *runtime_ctx : ci_static_fold_ctx(ci);
    err = ci_fold_truth(&ctx, &value, out);
    if(!runtime_ctx && err == FOLD_FAIL && ccqt_bt_eq(value.type, CCBT_long_double)){
        if(ctx.ldbl_fmt == CC_LONG_DOUBLE_X87)
            *out = ci_float80_nonzero(ci_float80_read(value.bits));
        else if(ctx.ldbl_fmt == CC_LONG_DOUBLE_BINARY128)
            *out = ci_float128_nonzero(ci_float128_read(value.bits));
        else return _cc_not_constant_error;
        return 0;
    }
    return err == FOLD_FAIL ? _cc_not_constant_error : err;
}

static
int
ci_static_discard(CiInterpreter* ci, CcExpr* e, CiLowerCtx* _Nullable runtime_ctx){
    uint32_t size;
    int err = cc_sizeof_as_uint(&ci->parser, e->type, e->loc, &size);
    if(err) return err;
    Allocator al = ci_allocator(ci);
    unsigned char small[32];
    unsigned char* bytes = size <= sizeof small ? small : Allocator_alloc(al, size, 1, _Alignof(unsigned char));
    if(!bytes) return CI_OOM_ERROR;
    CiStaticBuffer buffer = {.bytes = bytes, .size = size, .runtime_ctx = runtime_ctx};
    err = ci_static_write(ci, &buffer, e, 0, size);
    ma_cleanup(CiStaticReloc)(&buffer.relocs, al);
    if(bytes != small) Allocator_free(al, bytes, size, 1, _Alignof(unsigned char));
    return err;
}

static
int
ci_static_read_address(CiInterpreter* ci, CcExpr* e, CiStaticReloc* out, CiLowerCtx* _Nullable runtime_ctx){
    unsigned char bytes[8] = {0};
    uint32_t size = ci_target(ci)->sizeof_[CCBT_nullptr_t];
    uint32_t object_size;
    int err = cc_expr_sizeof_as_uint(&ci->parser, e, &object_size);
    if(err) return err;
    if(object_size != size) return _cc_not_constant_error;
    CiStaticBuffer buffer = {.bytes = bytes, .size = size, .runtime_ctx = runtime_ctx};
    err = ci_static_write(ci, &buffer, e, 0, size);
    if(!err){
        if(buffer.relocs.count == 1 && buffer.relocs.data[0].offset == 0 && buffer.relocs.data[0].size == size){
            CiStaticReloc r = buffer.relocs.data[0];
            r.offset = out->offset;
            r.size = out->size;
            *out = r;
        }
        else if(!buffer.relocs.count)
            memcpy(&out->addend, bytes, size);
        else err = _cc_not_constant_error;
    }
    ma_cleanup(CiStaticReloc)(&buffer.relocs, ci_allocator(ci));
    return err;
}

static
int
ci_static_address(CiInterpreter* ci, CcExpr* e, _Bool lvalue, CiStaticReloc* out, CiLowerCtx* _Nullable runtime_ctx){
    if(ccqt_kind(e->type) == CC_VLA) return _cc_not_constant_error;
    CcParser* p = &ci->parser;
    int err;
    switch((uint32_t)e->kind){
        case CC_EXPR_VARIABLE:
            if(!lvalue && !ccqt_is_fixed_sequence(e->type)){
                if(ci_foldable_variable(e))
                    return ci_static_address(ci, e->var->initializer, 0, out, runtime_ctx);
                return _cc_not_constant_error;
            }
            if(e->var->automatic || e->var->thread_local_) return _cc_not_constant_error;
            out->kind = CI_STATIC_RELOC_VAR;
            out->var = e->var;
            return 0;
        case CC_EXPR_FUNCTION:
            out->kind = CI_STATIC_RELOC_FUNC;
            out->func = e->func;
            return 0;
        case CC_EXPR_BLOB:
        case CC_EXPR_VALUE:
            if(e->kind == CC_EXPR_BLOB || ccqt_is_fixed_sequence(e->type) || ccqt_kind(e->type) == CC_VLA){
                out->kind = CI_STATIC_RELOC_LITERAL;
                out->literal = cc_expr_array_data(e);
                return 0;
            }
            if(!lvalue && ccqt_has_pointer_representation(e->type)){
                out->addend = e->uinteger;
                return 0;
            }
            break;
        case CC_EXPR_INIT_LIST: case CC_EXPR_COMPOUND_LITERAL:
            if(!lvalue && (ccqt_has_pointer_representation(e->type) || ccqt_is_integer(e->type)))
                return ci_static_read_address(ci, e, out, runtime_ctx);
            return _cc_not_constant_error;
        case CC_EXPR_ADDR:
            return ci_static_address(ci, e->lhs, 1, out, runtime_ctx);
        case CC_EXPR_COMMA:{
            // The parser materializes compound literals as (anon = init, anon).
            // File-scope anonymous storage is itself a static data dependency.
            CcExpr* assign = e->lhs;
            CcExpr* ref = e->values[0];
            if(assign->kind == CC_EXPR_ASSIGN && assign->lhs->kind == CC_EXPR_VARIABLE
                && ref->kind == CC_EXPR_VARIABLE && assign->lhs->var == ref->var
                && ref->var->name == nil_atom && !ref->var->automatic){
                err = ci_static_discard(ci, assign->values[0], runtime_ctx);
                if(err) return err;
                out->initialize = 1;
                return ci_static_address(ci, ref, lvalue, out, runtime_ctx);
            }
            err = ci_static_discard(ci, assign, runtime_ctx);
            if(err) return err;
            return ci_static_address(ci, ref, lvalue, out, runtime_ctx);
        }
        case CC_EXPR_DEREF:
            if(!lvalue && ccqt_has_pointer_representation(e->type))
                return ci_static_read_address(ci, e, out, runtime_ctx);
            if(lvalue || ccqt_is_fixed_sequence(e->type) || ccqt_kind(e->type) == CC_FUNCTION)
                return ci_static_address(ci, e->lhs, 0, out, runtime_ctx);
            return _cc_not_constant_error;
        case CC_EXPR_DOT:
        case CC_EXPR_ARROW:
            if(!lvalue && !ccqt_is_fixed_sequence(e->type) && !cc_expr_field_bit_width(e))
                return ci_static_read_address(ci, e, out, runtime_ctx);
            if((!lvalue && !ccqt_is_fixed_sequence(e->type)) || cc_expr_field_bit_width(e))
                return _cc_not_constant_error;
            err = ci_static_address(ci, e->values[0], e->kind == CC_EXPR_DOT, out, runtime_ctx);
            if(err) return err;
            uint64_t field_offset;
            err = cc_expr_field_offset(ci_target(ci), e, &field_offset);
            if(err) return err;
            out->addend += field_offset;
            return 0;
        case CC_EXPR_SUBSCRIPT:{
            if(!lvalue && !ccqt_is_fixed_sequence(e->type)
                && (ccqt_is_fixed_sequence(e->lhs->type) || ccqt_has_pointer_representation(e->type)))
                return ci_static_read_address(ci, e, out, runtime_ctx);
            if(!lvalue && !ccqt_is_fixed_sequence(e->type)) return _cc_not_constant_error;
            int64_t index;
            err = ci_static_integer(ci, e->values[0], &index, runtime_ctx);
            if(err) return err;
            if(ccqt_kind(e->lhs->type) == CC_VLA) return _cc_not_constant_error;
            if(ccqt_is_fixed_sequence(e->lhs->type)){
                if(!ccqt_array_incomplete(e->lhs->type) && (index < 0 || (uint64_t)index > ccqt_sequence_length(e->lhs->type)))
                    return _cc_not_constant_error;
            }
            uint32_t scale;
            err = cc_sizeof_as_uint(p, e->type, e->loc, &scale);
            if(err) return err;
            if(ccqt_kind(e->lhs->type) == CC_SLICE){
                uint64_t count;
                err = ci_static_slice_value(ci, e->lhs, &count, out, runtime_ctx);
                if(!err && (index < 0 || (uint64_t)index > count)) err = _cc_not_constant_error;
            }
            else err = ci_static_address(ci, e->lhs, ccqt_is_fixed_sequence(e->lhs->type), out, runtime_ctx);
            if(err) return err;
            out->addend += (uint64_t)index * scale;
            return 0;
        }
        case CC_EXPR_CAST:{
            if(!ccqt_has_pointer_representation(e->type) && !ccqt_is_integer(e->type))
                return _cc_not_constant_error;
            if(ccqt_is_integer(e->lhs->type)){
                // Integer-to-pointer casts preserve the low pointer bits;
                // the source need not fit in a signed address offset.
                CiFoldValue integer;
                err = ci_initializer_fold(ci, e->lhs, &integer, runtime_ctx);
                if(!err){
                    CiLowerCtx ctx = runtime_ctx ? *runtime_ctx : ci_static_fold_ctx(ci);
                    out->addend = ci_uint128_lo(ci_fold_integer(&ctx, &integer));
                    return 0;
                }
                if(err != _cc_not_constant_error) return err;
            }
            uint32_t size;
            err = cc_sizeof_as_uint(p, e->type, e->loc, &size);
            if(err) return err;
            if(size < ci_target(ci)->sizeof_[CCBT_nullptr_t]) return _cc_not_constant_error;
            return ci_static_address(ci, e->lhs, ccqt_is_fixed_sequence(e->lhs->type), out, runtime_ctx);
        }
        case CC_EXPR_ADD:
        case CC_EXPR_SUB:{
            CcExpr* base = e->lhs;
            CcExpr* index = e->values[0];
            if(e->kind == CC_EXPR_ADD && ccqt_kind(index->type) == CC_POINTER){
                base = e->values[0];
                index = e->lhs;
            }
            int64_t n;
            err = ci_static_integer(ci, index, &n, runtime_ctx);
            if(err == _cc_not_constant_error && e->kind == CC_EXPR_ADD){
                base = e->values[0];
                index = e->lhs;
                err = ci_static_integer(ci, index, &n, runtime_ctx);
            }
            if(err) return err;
            uint32_t scale = 1;
            if(ccqt_kind(base->type) == CC_POINTER){
                CcQualType pointee = ccqt_as_ptr(base->type)->pointee;
                if(!ccqt_bt_eq(pointee, CCBT_void)){
                    err = cc_sizeof_as_uint(p, pointee, e->loc, &scale);
                    if(err) return err;
                }
            }
            err = ci_static_address(ci, base, 0, out, runtime_ctx);
            if(err) return err;
            uint64_t delta = (uint64_t)n * scale;
            out->addend += e->kind == CC_EXPR_SUB ? -delta : delta;
            return 0;
        }
        case CC_EXPR_TERNARY:{
            _Bool truth;
            err = ci_static_truth(ci, e->lhs, &truth, runtime_ctx);
            if(err) return err;
            return ci_static_address(ci, e->values[truth ? 0 : 1], lvalue, out, runtime_ctx);
        }
        default:
            break;
    }
    if(lvalue) return _cc_not_constant_error;
    int64_t integer;
    err = ci_static_integer(ci, e, &integer, runtime_ctx);
    if(err) return err;
    out->addend = (uint64_t)integer;
    return 0;
}

static
int
ci_static_add_reloc(CiStaticBuffer* data, Allocator al, CiStaticReloc reloc){
    size_t count = data->relocs.count;
    size_t at = count;
    if(count && reloc.offset < data->relocs.data[count - 1].offset){
        size_t lo = 0, hi = count;
        while(lo < hi){
            size_t mid = lo + (hi - lo) / 2;
            if(data->relocs.data[mid].offset < reloc.offset) lo = mid + 1;
            else hi = mid;
        }
        at = lo;
    }
    int err = ma_push(CiStaticReloc)(&data->relocs, al, reloc);
    if(err) return err;
    if(at != count){
        CiStaticReloc* items = data->relocs.data;
        memmove(items + at + 1, items + at, (count - at) * sizeof *items);
        items[at] = reloc;
    }
    return 0;
}

static
void
ci_static_forget_relocs(CiStaticBuffer* data, uint32_t offset, uint32_t size){
    DRP_PROFILE_BEGIN(profile, "initializer.forget_relocs");
    size_t count = data->relocs.count;
    if(!count) return;
    CiStaticReloc* items = data->relocs.data;
    uint64_t end = (uint64_t)offset + size;
    if(offset >= (uint64_t)items[count - 1].offset + items[count - 1].size
    || end <= items[0].offset)
        return;
    size_t lo = 0, hi = count;
    while(lo < hi){
        size_t mid = lo + (hi - lo) / 2;
        if((uint64_t)items[mid].offset + items[mid].size <= offset) lo = mid + 1;
        else hi = mid;
    }
    size_t first = lo;
    hi = count;
    while(lo < hi){
        size_t mid = lo + (hi - lo) / 2;
        if(items[mid].offset < end) lo = mid + 1;
        else hi = mid;
    }
    if(lo == first) return; // The write lies in a gap between relocations.
    memmove(items + first, items + lo, (count - lo) * sizeof *items);
    data->relocs.count = count - (lo - first);
    DRP_PROFILE_END(profile);
}

static
int
ci_static_copy_range(CiInterpreter* ci, CiStaticBuffer* data, const CiStaticBuffer* source,
    uint32_t start, uint32_t offset, uint32_t size, const unsigned char* mask){
    for(size_t i = 0; i < source->relocs.count; i++){
        CiStaticReloc r = source->relocs.data[i];
        uint64_t end = (uint64_t)r.offset + r.size;
        if(end <= start || r.offset >= (uint64_t)start + size) continue;
        uint32_t lo = r.offset > start ? r.offset : start;
        uint32_t hi = end < (uint64_t)start + size ? (uint32_t)end : start + size;
        _Bool needed = 0;
        for(uint32_t at = lo; at < hi; at++) needed |= mask[at-start] != 0;
        if(!needed) continue;
        if(r.offset < start || end > (uint64_t)start + size) return _cc_not_constant_error;
        for(uint32_t at = lo; at < hi; at++)
            if(mask[at-start] != 0xff) return _cc_not_constant_error;
        r.offset = offset + r.offset - start;
        if(ci_static_add_reloc(data, ci_allocator(ci), r))
            return CI_OOM_ERROR;
    }
    for(uint32_t i = 0; i < size; i++){
        unsigned char* byte = data->bytes + offset + i;
        *byte = (*byte & ~mask[i]) | (source->bytes[start+i] & mask[i]);
    }
    return 0;
}

static
int
ci_static_read_range(CiInterpreter* ci, CiStaticBuffer* data, CcExpr* e,
    uint32_t start, uint32_t offset, uint32_t size, const unsigned char* mask, unsigned depth){
    CcParser* p = &ci->parser;
    if(depth >= 256 || (e->is_lvalue && (e->type.is_volatile || e->type.is_atomic)))
        return _cc_not_constant_error;
    uint32_t object_size;
    int err = cc_expr_sizeof_as_uint(p, e, &object_size);
    if(err) return err;
    if(start > object_size || size > object_size-start) return _cc_not_constant_error;
    _Bool needed = 0;
    for(uint32_t i = 0; i < size; i++) needed |= mask[i] != 0;
    if(!needed) return 0;
    if(e->kind == CC_EXPR_VARIABLE && ci_foldable_variable(e))
        return ci_static_read_range(ci, data, e->var->initializer, start, offset, size, mask, depth+1);
    if(e->kind == CC_EXPR_OBJECT_VIEW
    || (e->kind == CC_EXPR_CAST && (cc_conversion_preserves_representation(e->lhs->type, e->type)
    || (ccqt_has_pointer_representation(e->type) && ccqt_has_pointer_representation(e->lhs->type)))))
        return ci_static_read_range(ci, data, e->lhs, start, offset, size, mask, depth+1);
    if(e->kind == CC_EXPR_TYPE_INTROSPECTION || e->kind == CC_EXPR_SRCLOC_REFLECT){
        CcExpr* value;
        err = cc_eval_linktime_expr(p, e, &value);
        if(err) return err;
        err = ci_static_read_range(ci, data, value, start, offset, size, mask, depth+1);
        cc_release_expr(p, value);
        return err;
    }
    if(e->kind == CC_EXPR_TERNARY){
        _Bool truth;
        err = ci_static_truth(ci, e->lhs, &truth, data->runtime_ctx);
        if(err) return err;
        return ci_static_read_range(ci, data, e->values[truth ? 0 : 1], start, offset, size, mask, depth+1);
    }
    if(e->kind == CC_EXPR_COMMA){
        CcExpr* assign = e->lhs;
        CcExpr* ref = e->values[0];
        if(assign->kind == CC_EXPR_ASSIGN && assign->lhs->kind == CC_EXPR_VARIABLE
            && ref->kind == CC_EXPR_VARIABLE && assign->lhs->var == ref->var
            && ref->var->name == nil_atom && !ref->var->automatic)
            return ci_static_read_range(ci, data, assign->values[0], start, offset, size, mask, depth+1);
        err = ci_static_discard(ci, assign, data->runtime_ctx);
        if(err) return err;
        return ci_static_read_range(ci, data, ref, start, offset, size, mask, depth+1);
    }
    if(!cc_expr_field_bit_width(e) && (e->kind == CC_EXPR_DOT || e->kind == CC_EXPR_ARROW
        || e->kind == CC_EXPR_DEREF || e->kind == CC_EXPR_SUBSCRIPT)){
        CcExpr storage = {0};
        CcEvalObjectView view = {.object=e, .storage=&storage};
        err = cc_eval_linktime_object_view(p, e, &view);
        if(err) return err;
        uint64_t at;
        if(add_overflow(view.byte_offset, (uint64_t)start, &at) || at > UINT32_MAX)
            err = _cc_not_constant_error;
        else err = ci_static_read_range(ci, data, view.object, (uint32_t)at, offset, size, mask, depth+1);
        cc_field_path_free(cc_allocator(p), view.path);
        return err;
    }
    // Every aggregate write also supplies zeroes for omitted fields and padding.
    for(uint32_t i = 0; i < size; i++) data->bytes[offset+i] &= ~mask[i];
    if(e->kind == CC_EXPR_CAST && ccqt_bt_eq(e->type, CCBT__Any)){
        CcQualType tag = {.unqual=e->lhs->type.unqual};
        uint32_t payload = offsetof(CiRtAny, payload);
        for(uint32_t i = 0; i < size; i++)
            if(start+i < sizeof tag)
                data->bytes[offset+i] |= ((const unsigned char*)&tag)[start+i] & mask[i];
        uint32_t src_size;
        err = cc_sizeof_as_uint(p, e->lhs->type, e->loc, &src_size);
        if(err) return err;
        uint32_t lo = start > payload ? start : payload;
        uint32_t hi = start+size < payload+src_size ? start+size : payload+src_size;
        if(lo < hi) return ci_static_read_range(ci, data, e->lhs, lo-payload,
            offset+lo-start, hi-lo, mask+lo-start, depth+1);
        return 0;
    }
    if(e->kind == CC_EXPR_INIT_LIST || e->kind == CC_EXPR_COMPOUND_LITERAL){
        Allocator al = ci_allocator(ci);
        unsigned char* pending = Allocator_alloc(al, size, 1, _Alignof(unsigned char));
        if(!pending) return CI_OOM_ERROR;
        memcpy(pending, mask, size);
        CcInitList* list = e->init_list;
        for(uint32_t i = list->count; i; i--){
            CcInitEntry* entry = &list->entries[i-1];
            if(!entry->value) continue;
            CiFieldLoc loc;
            err = ci_init_entry_loc(ci, e->type, entry, &loc);
            if(err) break;
            uint32_t sz;
            err = cc_expr_sizeof_as_uint(p, entry->value, &sz);
            if(err) break;
            uint64_t end = loc.byte_offset + (loc.bit_width ? (loc.bit_offset + loc.bit_width + 7u) / 8u : sz);
            if(loc.byte_offset >= (uint64_t)start+size || end <= start) continue;
            uint32_t lo = loc.byte_offset > start ? (uint32_t)loc.byte_offset : start;
            uint32_t hi = end < (uint64_t)start+size ? (uint32_t)end : start+size;
            needed = 0;
            for(uint32_t at = lo; at < hi; at++){
                unsigned char bits = pending[at-start];
                if(loc.bit_width){
                    bits = 0;
                    for(unsigned bit = 0; bit < 8; bit++){
                        uint64_t pos = ((uint64_t)at-loc.byte_offset)*8+bit;
                        if(pos >= loc.bit_offset && pos-loc.bit_offset < loc.bit_width)
                            bits |= pending[at-start] & (1u << bit);
                    }
                }
                needed |= bits != 0;
            }
            if(!needed) continue;
            if(loc.bit_width){
                CiFoldValue folded;
                err = ci_initializer_fold(ci, entry->value, &folded, data->runtime_ctx);
                if(err) break;
                if(folded.sz != sz || sz > sizeof folded.bits){ err = _cc_not_constant_error; break; }
                const unsigned char* bits = (const unsigned char*)folded.bits;
                for(uint32_t at = lo; at < hi; at++){
                    for(unsigned bit = 0; bit < 8; bit++){
                        uint64_t pos = ((uint64_t)at-loc.byte_offset)*8+bit;
                        if(pos < loc.bit_offset || pos-loc.bit_offset >= loc.bit_width) continue;
                        pos -= loc.bit_offset;
                        unsigned char bm = (unsigned char)(1u << bit);
                        if(!(pending[at-start] & bm)) continue;
                        if(pos/8 >= sz){ err = _cc_not_constant_error; break; }
                        if(bits[pos/8] & (1u << (pos%8))) data->bytes[offset+at-start] |= bm;
                        pending[at-start] &= ~bm;
                    }
                    if(err) break;
                }
                if(err) break;
            }
            else {
                err = ci_static_read_range(ci, data, entry->value, lo-(uint32_t)loc.byte_offset,
                    offset+lo-start, hi-lo, pending+lo-start, depth+1);
                if(err) break;
                memset(pending+lo-start, 0, hi-lo);
            }
        }
        Allocator_free(al, pending, size, 1, _Alignof(unsigned char));
        return err;
    }
    if((e->kind == CC_EXPR_BLOB || (e->kind == CC_EXPR_VALUE && ccqt_is_fixed_sequence(e->type))) && cc_expr_array_data(e)){
        uint32_t elem_size;
        err = cc_sizeof_as_uint(p, cc_expr_array_element(e), e->loc, &elem_size);
        if(err) return err;
        uint64_t length = (uint64_t)cc_expr_array_count(e)*elem_size;
        for(uint32_t i = 0; i < size; i++)
            if((uint64_t)start+i < length) data->bytes[offset+i] |= cc_expr_array_data(e)[start+i] & mask[i];
        return 0;
    }
    unsigned char bytes[16] = {0};
    if(object_size > sizeof bytes) return _cc_not_constant_error;
    CiStaticBuffer buffer = {.bytes=bytes, .size=object_size, .runtime_ctx=data->runtime_ctx};
    err = ci_static_write(ci, &buffer, e, 0, object_size);
    if(!err) err = ci_static_copy_range(ci, data, &buffer, start, offset, size, mask);
    ma_cleanup(CiStaticReloc)(&buffer.relocs, ci_allocator(ci));
    return err;
}

static
int
ci_static_subobject(CiInterpreter* ci, CiStaticBuffer* data, CcExpr* base, uint32_t start, uint32_t offset, uint32_t size){
    if(!size) return 0;
    Allocator al = ci_allocator(ci);
    unsigned char* mask = Allocator_alloc(al, size, 1, _Alignof(unsigned char));
    if(!mask) return CI_OOM_ERROR;
    memset(mask, 0xff, size);
    int err = ci_static_read_range(ci, data, base, start, offset, size, mask, 0);
    Allocator_free(al, mask, size, 1, _Alignof(unsigned char));
    return err;
}
static
int
ci_static_slice_value(CiInterpreter* ci, CcExpr* e, uint64_t* length, CiStaticReloc* address, CiLowerCtx*_Nullable runtime_ctx){
    uint32_t ptr_size = ci_target(ci)->sizeof_[CCBT_nullptr_t];
    unsigned char bytes[16] = {0};
    CiStaticBuffer buffer = {.bytes = bytes, .size = 2 * ptr_size, .runtime_ctx = runtime_ctx};
    int err = ci_static_write(ci, &buffer, e, 0, buffer.size);
    if(!err){
        *length = 0;
        memcpy(length, bytes, ptr_size);
        if(!buffer.relocs.count){
            address->kind = CI_STATIC_RELOC_ABSOLUTE;
            address->addend = 0;
            memcpy(&address->addend, bytes + ptr_size, ptr_size);
        }
        else if(buffer.relocs.count == 1 && buffer.relocs.data[0].offset == ptr_size && buffer.relocs.data[0].size == ptr_size){
            uint32_t offset = address->offset;
            *address = buffer.relocs.data[0];
            address->offset = offset;
        }
        else err = _cc_not_constant_error;
    }
    ma_cleanup(CiStaticReloc)(&buffer.relocs, ci_allocator(ci));
    return err;
}

static
int
ci_static_slice(CiInterpreter* ci, CiStaticBuffer* data, CcExpr* e, uint32_t offset){
    // Runtime templates retain the optimizer's stricter rules for eliding
    // effects and inexact arithmetic, before asking for an aggregate value.
    if(data->runtime_ctx){
        unsigned bounds = e->kind == CC_EXPR_SLICE ? 2
            : e->kind == CC_EXPR_SLICE_LO || e->kind == CC_EXPR_SLICE_HI ? 1 : 0;
        for(unsigned i = 0; i < bounds; i++){
            int64_t bound;
            int err = ci_static_integer(ci, e->values[i], &bound, data->runtime_ctx);
            if(err) return err;
        }
        CiStaticReloc address = {.kind=CI_STATIC_RELOC_ABSOLUTE};
        CcTypeKind kind = ccqt_kind(e->lhs->type);
        int err;
        if(kind == CC_SLICE){
            uint64_t count;
            err = ci_static_slice_value(ci, e->lhs, &count, &address, data->runtime_ctx);
        }
        else err = ci_static_address(ci, e->lhs, (kind == CC_ARRAY || kind == CC_VECTOR), &address, data->runtime_ctx);
        if(err) return err;
    }
    CcExpr* value;
    int err = cc_eval_linktime_expr(&ci->parser, e, &value);
    if(err) return err;
    uint32_t size;
    err = cc_sizeof_as_uint(&ci->parser, e->type, e->loc, &size);
    if(!err) err = ci_static_write(ci, data, value, offset, size);
    cc_release_expr(&ci->parser, value);
    return err;
}

static
int
ci_static_write(CiInterpreter* ci, CiStaticBuffer* data, CcExpr* e, uint32_t offset, uint32_t size){
    DRP_PROFILE_BEGIN(profile, "initializer.write");
    CcParser* p = &ci->parser;
    int err;
    if(e->is_lvalue && (e->type.is_volatile || e->type.is_atomic))
        return _cc_not_constant_error;
    if(offset > data->size || size > data->size - offset)
        return ci_error(ci, e->loc, "static initializer exceeds object size");
    memset(data->bytes + offset, 0, size);
    ci_static_forget_relocs(data, offset, size);
    if(e->kind == CC_EXPR_TYPE_INTROSPECTION || e->kind == CC_EXPR_SRCLOC_REFLECT){
        CcExpr* value;
        err = cc_eval_linktime_expr(p, e, &value);
        if(err) return err;
        err = ci_static_write(ci, data, value, offset, size);
        cc_release_expr(p, value);
        return err;
    }
    if(e->kind == CC_EXPR_VARIABLE && ci_foldable_variable(e))
        return ci_static_write(ci, data, e->var->initializer, offset, size);
    if(e->kind == CC_EXPR_OBJECT_VIEW
    || (e->kind == CC_EXPR_CAST && (cc_conversion_preserves_representation(e->lhs->type, e->type)
    || (ccqt_has_pointer_representation(e->type) && ccqt_has_pointer_representation(e->lhs->type)))))
        return ci_static_write(ci, data, e->lhs, offset, size);
    if(e->kind == CC_EXPR_SLICE || e->kind == CC_EXPR_SLICE_LO || e->kind == CC_EXPR_SLICE_HI
        || e->kind == CC_EXPR_SLICE_ALL || (e->kind == CC_EXPR_CAST && ccqt_kind(e->type) == CC_SLICE))
        return ci_static_slice(ci, data, e, offset);
    if(e->kind == CC_EXPR_DOT || e->kind == CC_EXPR_ARROW
        || e->kind == CC_EXPR_DEREF || e->kind == CC_EXPR_SUBSCRIPT){
        if(cc_expr_field_bit_width(e)){
            CcExpr* value;
            err = cc_eval_linktime_scalar(p, e, &value);
            if(err) return err;
            if(size > sizeof value->data) err = _cc_not_constant_error;
            else memcpy(data->bytes + offset, value->data, size);
            cc_release_expr(p, value);
            return err;
        }
        CcExpr storage = {0};
        CcEvalObjectView view = {.object=e, .storage=&storage};
        err = cc_eval_linktime_object_view(p, e, &view);
        if(err) return err;
        if(view.byte_offset > UINT32_MAX) err = _cc_not_constant_error;
        else err = ci_static_subobject(ci, data, view.object, (uint32_t)view.byte_offset,
            offset, size);
        cc_field_path_free(cc_allocator(p), view.path);
        return err;
    }
    if(e->kind == CC_EXPR_TERNARY){
        _Bool truth;
        err = ci_static_truth(ci, e->lhs, &truth, data->runtime_ctx);
        if(err) return err;
        return ci_static_write(ci, data, e->values[truth ? 0 : 1], offset, size);
    }
    if(e->kind == CC_EXPR_COMMA){
        CcExpr* assign = e->lhs;
        CcExpr* ref = e->values[0];
        if(assign->kind == CC_EXPR_ASSIGN && assign->lhs->kind == CC_EXPR_VARIABLE
            && ref->kind == CC_EXPR_VARIABLE && assign->lhs->var == ref->var
            && ref->var->name == nil_atom && !ref->var->automatic)
            return ci_static_write(ci, data, assign->values[0], offset, size);
        err = ci_static_discard(ci, e->lhs, data->runtime_ctx);
        if(err) return err;
        return ci_static_write(ci, data, e->values[0], offset, size);
    }
    if(e->kind == CC_EXPR_CAST && ccqt_bt_eq(e->type, CCBT__Any)){
        CcQualType tag = {.unqual = e->lhs->type.unqual};
        memcpy(data->bytes + offset, &tag, sizeof tag);
        uint32_t sz;
        err = cc_sizeof_as_uint(p, e->lhs->type, e->loc, &sz);
        if(err) return err;
        return ci_static_write(ci, data, e->lhs, offset + offsetof(CiRtAny, payload), sz);
    }
    if(e->kind == CC_EXPR_INIT_LIST || e->kind == CC_EXPR_COMPOUND_LITERAL){
        CcInitList* list = e->init_list;
        for(uint32_t i = 0; i < list->count; i++){
            CcInitEntry* entry = &list->entries[i];
            CiFieldLoc entry_loc;
            int entry_path_err = ci_init_entry_loc(ci, e->type, entry, &entry_loc);
            if(entry_path_err) return entry_path_err;
            if(!entry->value) continue;
            uint32_t sz;
            err = cc_expr_sizeof_as_uint(p, entry->value, &sz);
            if(err) return err;
            uint64_t at = (uint64_t)offset + entry_loc.byte_offset;
            if(at > data->size) return _cc_not_constant_error;
            if(entry_loc.bit_width){
                unsigned char bits[16] = {0};
                if(sz > sizeof bits) return _cc_not_constant_error;
                CiFoldValue folded;
                err = ci_initializer_fold(ci, entry->value, &folded, data->runtime_ctx);
                if(err) return err;
                if(folded.sz != sz) return _cc_not_constant_error;
                memcpy(bits, folded.bits, sz);
                uint32_t first = entry_loc.bit_offset / 8;
                uint32_t end = (entry_loc.bit_offset + entry_loc.bit_width + 7u) / 8;
                if(at + end > data->size) return _cc_not_constant_error;
                ci_static_forget_relocs(data, (uint32_t)at + first, end - first);
                for(uint32_t bit = 0; bit < entry_loc.bit_width; bit++){
                    uint32_t pos = entry_loc.bit_offset + bit;
                    if(at + pos / 8 >= data->size || bit / 8 >= sz) return _cc_not_constant_error;
                    unsigned char mask = (unsigned char)(1u << (pos % 8));
                    unsigned char* byte = data->bytes + at + pos / 8;
                    *byte = (*byte & ~mask) | ((bits[bit / 8] & (1u << (bit % 8))) ? mask : 0);
                }
            }
            else {
                err = ci_static_write(ci, data, entry->value, (uint32_t)at, sz);
                if(err) return err;
            }
        }
        return 0;
    }
    if((e->kind == CC_EXPR_BLOB || (e->kind == CC_EXPR_VALUE && ccqt_is_fixed_sequence(e->type))) && cc_expr_array_data(e)){
        uint32_t elem_size;
        err = cc_sizeof_as_uint(p, cc_expr_array_element(e), e->loc, &elem_size);
        if(err) return err;
        uint64_t bytes = (uint64_t)cc_expr_array_count(e) * elem_size;
        memcpy(data->bytes + offset, cc_expr_array_data(e), bytes < size ? (size_t)bytes : size);
        return 0;
    }
    CiFoldValue folded;
    err = ccqt_has_pointer_representation(e->type) ? _cc_not_constant_error : ci_initializer_fold(ci, e, &folded, data->runtime_ctx);
    if(!err){
        if(folded.sz != size) return _cc_not_constant_error;
        memcpy(data->bytes + offset, folded.bits, size);
    }
    if(err != _cc_not_constant_error) return err;
    // Constant bytes cannot encode a symbol's address until resolution finishes.
    if(!ccqt_has_pointer_representation(e->type) && !ccqt_is_integer(e->type)) return err;
    CiStaticReloc reloc = {.kind = CI_STATIC_RELOC_ABSOLUTE, .offset = offset, .size = size};
    err = ci_static_address(ci, e, 0, &reloc, data->runtime_ctx);
    if(err) return err;
    if(size > sizeof(uint64_t) || !size) return _cc_not_constant_error;
    if(reloc.kind == CI_STATIC_RELOC_ABSOLUTE){
        memcpy(data->bytes + offset, &reloc.addend, size);
        return 0;
    }
    memset(data->bytes + offset, 0, size);
    err = ci_static_add_reloc(data, ci_allocator(ci), reloc);
    return err ? CI_OOM_ERROR : 0;
    DRP_PROFILE_END(profile);
}

static
uint32_t
ci_init_store_count(CcExpr* e){
    if(e->kind != CC_EXPR_INIT_LIST && e->kind != CC_EXPR_COMPOUND_LITERAL)
        return 1;
    uint32_t count = 0;
    CcInitList* list = e->init_list;
    for(uint32_t i = 0; i < list->count && count < 4; i++)
        if(list->entries[i].value)
            count += ci_init_store_count(list->entries[i].value);
    return count;
}

static
int
ci_collect_template_entries(CiInterpreter* ci, CcExpr* e, CcFieldPath prefix, Marray(CcInitEntry)* entries){
    CcInitList* list = e->init_list;
    uint64_t written_end = 0;
    // Partial templates move constant writes ahead of dynamic expressions.
    // Keep overlapping subobjects on the ordered initialization path.
    for(uint32_t i = 0; i < list->count; i++){
        CcInitEntry* entry = &list->entries[i];
        CiFieldLoc entry_loc;
        int entry_path_err = ci_init_entry_loc(ci, e->type, entry, &entry_loc);
        if(entry_path_err) return entry_path_err;
        if(!entry->value) continue;
        uint32_t size;
        int err = cc_expr_sizeof_as_uint(&ci->parser, entry->value, &size);
        if(err) return err;
        uint64_t start = entry_loc.byte_offset;
        uint64_t end = start + size;
        if(entry_loc.bit_width){
            start += entry_loc.bit_offset / 8;
            end = entry_loc.byte_offset + (entry_loc.bit_offset + entry_loc.bit_width + 7u) / 8;
        }
        for(uint32_t j = 0; start < written_end && j < i; j++){
            CcInitEntry* prev = &list->entries[j];
            CiFieldLoc prev_loc;
            int prev_path_err = ci_init_entry_loc(ci, e->type, prev, &prev_loc);
            if(prev_path_err) return prev_path_err;
            if(!prev->value) continue;
            uint32_t prev_size;
            err = cc_expr_sizeof_as_uint(&ci->parser, prev->value, &prev_size);
            if(err) return err;
            uint64_t prev_start = prev_loc.byte_offset;
            uint64_t prev_end = prev_start + prev_size;
            if(prev_loc.bit_width){
                prev_start += prev_loc.bit_offset / 8;
                prev_end = prev_loc.byte_offset + (prev_loc.bit_offset + prev_loc.bit_width + 7u) / 8;
            }
            // Typed subobjects share the evaluator's replacement rules.
            // Bitfield probes still copy storage bytes, so distinct bitfields
            // sharing a byte cannot be reordered by a partial template.
            if(cc_field_paths_overlap(e->type, entry->path, prev->path)
                || ((entry_loc.bit_width || prev_loc.bit_width) && start < prev_end && prev_start < end))
                return _cc_not_constant_error;
        }
        if(end > written_end) written_end = end;
        CcFieldPath path;
        if(cc_field_path_concat(ci_allocator(ci), prefix, entry->path, &path)) return CI_OOM_ERROR;
        if(entry->value->kind == CC_EXPR_INIT_LIST && !entry_loc.bit_width){
            err = ci_collect_template_entries(ci, entry->value, path, entries);
            cc_field_path_free(ci_allocator(ci), path);
            if(err) return err;
        }
        else {
            CcInitEntry leaf = *entry;
            leaf.path = path;
            err = ma_push(CcInitEntry)(entries, ci_allocator(ci), leaf);
            if(err){ cc_field_path_free(ci_allocator(ci), path); return CI_OOM_ERROR; }
        }
    }
    return 0;
}

static
int
ci_partial_init_template(CiInterpreter* ci, CiStaticBuffer* buffer, CcExpr* e, CcInitList* _Nullable* _Nonnull residual){
    Allocator al = ci_allocator(ci);
    Marray(CcInitEntry) entries = {0};
    CiStaticBuffer probe = {.size = buffer->size, .runtime_ctx = buffer->runtime_ctx};
    CcInitList* single = NULL;
    CcInitList* runtime = NULL;
    unsigned char* probe_bytes = NULL;
    int err = ci_collect_template_entries(ci, e, (CcFieldPath){0}, &entries);
    if(err) goto cleanup;
    runtime = Allocator_zalloc_extra(al, entries.count, sizeof *runtime->entries, sizeof *runtime, _Alignof(CcInitList));
    single = Allocator_zalloc_extra(al, 1, sizeof *single->entries, sizeof *single, _Alignof(CcInitList));
    probe_bytes = Allocator_zalloc(al, probe.size, 1, _Alignof(unsigned char));
    if(!runtime || !single || !probe_bytes){ err = CI_OOM_ERROR; goto cleanup; }
    probe.bytes = probe_bytes;
    single->count = 1;
    runtime->loc = e->loc;
    CcExpr candidate = *e;
    candidate.init_list = single;
    uint32_t constants = 0;
    for(size_t i = 0; i < entries.count; i++){
        CcInitEntry entry = entries.data[i];
        CiFieldLoc entry_loc;
        err = ci_init_entry_loc(ci, e->type, &entry, &entry_loc);
        if(err) goto cleanup;
        single->entries[0] = entry;
        memset(probe.bytes, 0, probe.size);
        probe.relocs.count = 0;
        err = ci_static_write(ci, &probe, &candidate, 0, probe.size);
        if(!err){
            for(size_t j = 0; j < probe.relocs.count; j++)
                if(probe.relocs.data[j].initialize){ err = _cc_not_constant_error; break; }
        }
        if(err == _cc_not_constant_error){
            if(cc_field_path_concat(al, (CcFieldPath){0}, entry.path, &entry.path)){ err = CI_OOM_ERROR; goto cleanup; }
            runtime->entries[runtime->count++] = entry;
            err = 0;
            continue;
        }
        if(err) goto cleanup;
        uint32_t size;
        err = cc_expr_sizeof_as_uint(&ci->parser, entry.value, &size);
        if(err) goto cleanup;
        uint64_t start = entry_loc.byte_offset;
        if(entry_loc.bit_width){
            uint32_t first = entry_loc.bit_offset / 8;
            size = (entry_loc.bit_offset + entry_loc.bit_width + 7u) / 8 - first;
            start += first;
        }
        if(start > buffer->size || size > buffer->size - start){ err = _cc_not_constant_error; goto cleanup; }
        memcpy(buffer->bytes + start, probe.bytes + start, size);
        for(size_t j = 0; j < probe.relocs.count; j++){
            err = ci_static_add_reloc(buffer, al, probe.relocs.data[j]);
            if(err){ err = CI_OOM_ERROR; goto cleanup; }
        }
        constants += ci_init_store_count(entry.value);
    }
    if(constants < 4){ err = _cc_not_constant_error; goto cleanup; }
    // Shrink so the caller can free the residual using its count.
    CcInitList* result = Allocator_alloc_extra(al, runtime->count, sizeof *result->entries, sizeof *result, _Alignof(CcInitList));
    if(!result){
        err = CI_OOM_ERROR;
        goto cleanup;
    }
    memcpy(result, runtime, sizeof *result + runtime->count * sizeof *result->entries);
    runtime->count = 0; // Transfer ownership of the residual paths.
    *residual = result;
    cleanup:
    if(runtime){
        for(uint32_t i = 0; i < runtime->count; i++) cc_field_path_free(al, runtime->entries[i].path);
        Allocator_free_extra(al, runtime, entries.count, sizeof *runtime->entries, sizeof *runtime, _Alignof(CcInitList));
    }
    if(single) Allocator_free_extra(al, single, 1, sizeof *single->entries, sizeof *single, _Alignof(CcInitList));
    if(probe_bytes) Allocator_free(al, probe_bytes, probe.size, 1, _Alignof(unsigned char));
    ma_cleanup(CiStaticReloc)(&probe.relocs, al);
    for(size_t i = 0; i < entries.count; i++) cc_field_path_free(al, entries.data[i].path);
    ma_cleanup(CcInitEntry)(&entries, al);
    return err;
}

static
int
ci_try_lower_init_template(CiInterpreter* ci, CiLowerCtx* ctx, CcExpr* e, CiLowerAddr dst, uint32_t size, _Bool* handled){
    *handled = 0;
    // One constant-load and one copy replace the zero-fill and at least four stores.
    if(!size || ci_init_store_count(e) < 4) return 0;
    Allocator al = ci_allocator(ci);
    unsigned char* bytes = Allocator_zalloc(al, size, 1, _Alignof(unsigned char));
    if(!bytes) return CI_OOM_ERROR;
    CiStaticBuffer buffer = {.bytes = bytes, .size = size, .runtime_ctx = ctx};
    CcInitList* residual = NULL;
    int err = ci_static_write(ci, &buffer, e, 0, size);
    if(!err){
        for(size_t i = 0; i < buffer.relocs.count; i++)
            if(buffer.relocs.data[i].initialize){ err = _cc_not_constant_error; break; }
    }
    if(err == _cc_not_constant_error){
        memset(bytes, 0, size);
        buffer.relocs.count = 0;
        err = ci_partial_init_template(ci, &buffer, e, &residual);
        if(err == _cc_not_constant_error){ err = 0; goto cleanup; }
    }
    if(err) goto cleanup;
    const void* template;
    if(buffer.relocs.count){
        // Keep a private copy: linking fills in addresses before this code runs.
        for(size_t i = 0; i < buffer.relocs.count; i++){
            CiStaticReloc* r = &buffer.relocs.data[i];
            switch(r->kind){
                case CI_STATIC_RELOC_VAR:
                    err = ci_deps_add_var(&ctx->deps, ctx->a, r->var, CI_VAR_DEP_USED);
                    if(err) err = CI_OOM_ERROR;
                    break;
                case CI_STATIC_RELOC_FUNC:
                    err = ci_lower_func_dep(ctx, r->func, (CcFuncDepFlags)(CC_FUNC_DEP_USED | CC_FUNC_DEP_ADDR_TAKEN));
                    break;
                case CI_STATIC_RELOC_ABSOLUTE:
                case CI_STATIC_RELOC_LITERAL:
                    break;
            }
            if(err) goto cleanup;
        }
        size_t nrelocs = buffer.relocs.count;
        CiInitTemplate* source;
        size_t alloc_size;
        if(fma_overflow(nrelocs, sizeof *source->relocs, sizeof *source, &alloc_size)
        || fma_overflow(size, sizeof *source->bytes, alloc_size, &alloc_size)){
            err = CI_OOM_ERROR;
            goto cleanup;
        }
        Allocator template_al = allocator_from_arena(&ci->bt.arena);
        source = Allocator_alloc(template_al, alloc_size, 1, _Alignof(CiInitTemplate));
        if(!source){ err = CI_OOM_ERROR; goto cleanup; }
        source->size = size;
        source->reloc_count = nrelocs;
        source->bytes = (unsigned char*)(source->relocs + nrelocs);
        source->linked = 0;
        memcpy(source->relocs, buffer.relocs.data, nrelocs * sizeof *source->relocs);
        memcpy(source->bytes, bytes, size);
        err = PM_put(&ctx->deps.templates, ctx->a, source, (void*)1);
        if(err){ err = CI_OOM_ERROR; goto cleanup; }
        template = source->bytes;
    }
    else {
        BlobAtom blob = BT_atomize(&ci->bt, bytes, size);
        if(!blob){ err = CI_OOM_ERROR; goto cleanup; }
        template = blob->data;
    }
    uint32_t temp = ctx->temp;
    uint32_t src;
    err = ci_alloc_slot(ctx, sizeof(void*), sizeof(void*), &src);
    if(err) goto cleanup;
    CiOp* op;
    err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
    if(err) goto cleanup;
    *op = (CiOp){
        .constant = {
            .kind = CI_OP_CONST,
            .bt_kind = CCBT__Any,
            .slot = src,
            .immsize = sizeof(void*),
            .loc = e->loc,
        },
    };
    memcpy(op->constant.immediate, &template, sizeof template);
    err = ma_alloc(CiOp)(ctx->out, ctx->a, &op);
    if(err) goto cleanup;
    *op = (CiOp){
        .memcopy = {
            .is_volatile = dst.is_volatile,
            .kind = CI_OP_MEMCOPY,
            .slot = dst.slot,
            .offset = dst.disp, .dst_align_log2 = dst.align_log2,
            .src = src,
            .size = size,
            .loc = e->loc,
        },
    };
    ctx->temp = temp;
    if(residual && residual->count){
        CcExpr runtime = *e;
        runtime.init_list = residual;
        err = ci_lower_init_list(ci, ctx, &runtime, dst, 0);
        if(err) goto cleanup;
    }
    *handled = 1;
    cleanup:
    if(residual){
        for(uint32_t i = 0; i < residual->count; i++) cc_field_path_free(al, residual->entries[i].path);
        Allocator_free_extra(al, residual, residual->count, sizeof *residual->entries, sizeof *residual, _Alignof(CcInitList));
    }
    ma_cleanup(CiStaticReloc)(&buffer.relocs, al);
    Allocator_free(al, bytes, size, 1, _Alignof(unsigned char));
    return err;
}

static
int
ci_build_static_data(CiInterpreter* ci, CiStaticData* data, CiLowerDeps* deps){
    DRP_PROFILE_BEGIN(profile, "resolve.static_initializer");
    Allocator al = ci_allocator(ci);
    CcVariable* var = data->var;
    int err = cc_sizeof_as_uint(&ci->parser, var->type, var->loc, &data->size);
    if(err) return err;
    unsigned char* bytes = Allocator_zalloc(al, data->size, 1, _Alignof(unsigned char));
    if(!bytes) return CI_OOM_ERROR;
    CiStaticBuffer buffer = {.size = data->size, .bytes = bytes};
    CiLowerDeps local = {0};
    err = ci_static_write(ci, &buffer, var->initializer, 0, buffer.size);
    if(err == _cc_not_constant_error)
        err = ci_error(ci, var->initializer->loc, "ICE: unsupported parser-accepted static initializer");
    if(err) goto cleanup;
    for(size_t i = 0; i < buffer.relocs.count; i++){
        CiStaticReloc* r = &buffer.relocs.data[i];
        switch(r->kind){
            case CI_STATIC_RELOC_VAR:
                err = ci_deps_add_var(&local, al, r->var, CI_VAR_DEP_USED | (r->initialize ? CI_VAR_DEP_INITIALIZE : 0));
                if(err) err = CI_OOM_ERROR;
                break;
            case CI_STATIC_RELOC_FUNC:
                err = ci_deps_add_func(&local, al, r->func, CC_FUNC_DEP_USED | CC_FUNC_DEP_ADDR_TAKEN);
                break;
            case CI_STATIC_RELOC_ABSOLUTE:
            case CI_STATIC_RELOC_LITERAL:
                break;
        }
        if(err) goto cleanup;
    }
    BlobAtom blob = BT_atomize(&ci->bt, buffer.bytes, buffer.size);
    if(!blob){ err = CI_OOM_ERROR; goto cleanup; }
    err = ci_lower_deps_merge(deps, &local, al);
    if(err) goto cleanup;
    data->blob = blob;
    data->relocs = buffer.relocs;
    buffer.relocs = (Marray(CiStaticReloc)){0};
    cleanup:
    ci_lower_deps_cleanup(&local, al);
    ma_cleanup(CiStaticReloc)(&buffer.relocs, al);
    Allocator_free(al, buffer.bytes, buffer.size, 1, _Alignof(unsigned char));
    return err;
    DRP_PROFILE_END(profile);
}

static
int
ci_try_preinit_global(CiInterpreter* ci, CiLowerCtx* ctx, CcExpr* e, _Bool* handled){
    *handled = 0;
    if(e->kind != CC_EXPR_ASSIGN || e->lhs->kind != CC_EXPR_VARIABLE)
        return 0;
    CcVariable* var = e->lhs->var;
    if(!var->interp_try_preinit || e->values[0] != var->initializer)
        return 0;
    Allocator al = ci_allocator(ci);
    CcParser* p = &ci->parser;
    Logger quiet = {.level = LOG_OFF, .buff = {.allocator = al}};
    Logger* logger = p->cpp.logger;
    p->cpp.logger = &quiet;
    CiStaticBuffer buffer = {0};
    int err = cc_check_linktime_expr(p, var->initializer, 0, 0);
    if(err) goto cleanup;
    err = cc_sizeof_as_uint(p, var->type, var->loc, &buffer.size);
    if(err) goto cleanup;
    unsigned char* bytes = Allocator_zalloc(al, buffer.size ? buffer.size : 1, 1, _Alignof(unsigned char));
    if(!bytes){
        err = CI_OOM_ERROR;
        goto cleanup;
    }
    buffer.bytes = bytes;
    err = ci_static_write(ci, &buffer, var->initializer, 0, buffer.size);
    cleanup:
    p->cpp.logger = logger;
    msb_destroy(&quiet.buff);
    if(buffer.bytes) Allocator_free(al, buffer.bytes, buffer.size ? buffer.size : 1, 1, _Alignof(unsigned char));
    ma_cleanup(CiStaticReloc)(&buffer.relocs, al);
    if(err) return err == CI_OOM_ERROR ? err : 0;
    err = ci_deps_add_var(&ctx->deps, al, var, CI_VAR_DEP_USED);
    if(err) return err;
    var->interp_preinit = 1;
    *handled = 1;
    return 0;
}

static
void
ci_static_data_cleanup(CiStaticData* data, Allocator al){
    ma_cleanup(CiStaticReloc)(&data->relocs, al);
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

typedef struct CiFoldFloatFormat CiFoldFloatFormat;
struct CiFoldFloatFormat {
    uint32_t precision, bias, max_exp;
};

static
int
ci_fold_float_format(CiLowerCtx* ctx, CcQualType type, CiFoldFloatFormat* out){
    switch(ci_float_width(type, ctx->ldbl_fmt)){
        case 16: *out = ccqt_bt_eq(type, CCBT_bfloat16) ? (CiFoldFloatFormat){7, 127, 255} : (CiFoldFloatFormat){10, 15, 31}; return 0;
        case 32: *out = (CiFoldFloatFormat){23, 127, 255}; return 0;
        case 64: *out = (CiFoldFloatFormat){52, 1023, 2047}; return 0;
        default: return FOLD_FAIL;
    }
}

static
int
ci_fold_float_read(CiLowerCtx* ctx, const CiFoldValue* v, CiFoldFloat* out){
    CiFoldFloatFormat format;
    int err = ci_fold_float_format(ctx, v->type, &format);
    if(err) return err;
    uint64_t fraction;
    uint32_t exponent;
    if(v->sz == 2){
        uint16_t bits;
        memcpy(&bits, v->bits, 2);
        fraction = bits & ((1u << format.precision) - 1);
        exponent = (bits >> format.precision) & format.max_exp;
        out->negative = bits >> 15;
    }
    else if(format.precision == 23){
        CiIEE754Float32 f;
        memcpy(&f.u, v->bits, sizeof f.u);
        fraction = f.fraction;
        exponent = f.exponent;
        out->negative = f.sign;
    }
    else {
        CiIEE754Float64 f;
        memcpy(&f.u, v->bits, sizeof f.u);
        fraction = f.fraction;
        exponent = (uint32_t)f.exponent;
        out->negative = f.sign;
    }
    if((exponent == format.max_exp || exponent == 0) && fraction) return FOLD_FAIL;
    out->infinity = exponent == format.max_exp;
    out->significand = fraction | (exponent && !out->infinity ? (uint64_t)1 << format.precision : 0);
    out->exponent = (int)exponent - (int)format.bias - (int)format.precision;
    return 0;
}

static
int
ci_fold_float_write(CiLowerCtx* ctx, CiFoldValue* out, CiUint128 magnitude, int exponent, _Bool negative, _Bool infinity){
    CiFoldFloatFormat format;
    int err = ci_fold_float_format(ctx, out->type, &format);
    if(err) return err;
    uint32_t precision = format.precision, bias = format.bias, max_exp = format.max_exp;
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
    if(out->sz == 2){
        uint16_t bits = (uint16_t)(((uint32_t)negative << 15) | (encoded_exp << precision) | (uint32_t)fraction);
        memcpy(out->bits, &bits, 2);
    }
    else if(precision == 23){
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
ci_fold_float_binary(CiLowerCtx* ctx, CcExprKind kind, const CiFoldValue* lhs, const CiFoldValue* rhs, CiFoldValue* out){
    if(lhs->type.unqual != rhs->type.unqual) return FOLD_FAIL;
    CiFoldFloat a, b;
    int err = ci_fold_float_read(ctx, lhs, &a);
    if(err) return err;
    err = ci_fold_float_read(ctx, rhs, &b);
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
            return ci_fold_float_write(ctx, out, ci_uint128_mul(u, v), a.exponent + b.exponent, a.negative != b.negative, 0);
        case CC_EXPR_DIV:{
            if(!b.significand) return FOLD_FAIL;
            // A binary-exact quotient requires the denominator's odd part to
            // divide the numerator. Powers of two just adjust the exponent.
            int twos = ctz_64(b.significand);
            uint64_t odd = b.significand >> twos;
            if(a.significand % odd) return FOLD_FAIL;
            u = ci_uint128_from_uint64(a.significand / odd);
            return ci_fold_float_write(ctx, out, u, a.exponent - b.exponent - twos, a.negative != b.negative, 0);
        }
        case CC_EXPR_ADD:
        case CC_EXPR_SUB:{
            if(kind == CC_EXPR_SUB) b.negative = !b.negative;
            if(!a.significand && !b.significand){
                if(a.negative != b.negative) return FOLD_FAIL; // zero sign depends on rounding
                return ci_fold_float_write(ctx, out, u, 0, a.negative, 0);
            }
            if(!a.significand)
                return ci_fold_float_write(ctx, out, v, b.exponent, b.negative, 0);
            if(!b.significand)
                return ci_fold_float_write(ctx, out, u, a.exponent, a.negative, 0);
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
            return ci_fold_float_write(ctx, out, u, exponent, a.negative, 0);
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
        return ci_fold_float_write(ctx, to, u, 0, negative, 0);
    }
    if(!ci_falu_type(from->type)) return FOLD_FAIL;
    if(from->type.unqual == to->type.unqual){
        memcpy(to->bits, from->bits, from->sz);
        return 0;
    }
    CiFoldFloat f;
    int err = ci_fold_float_read(ctx, from, &f);
    if(err) return err;
    CiUint128 u = ci_uint128_from_uint64(f.significand);
    if(ci_falu_type(to->type))
        return ci_fold_float_write(ctx, to, u, f.exponent, f.negative, f.infinity);
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
    if(!ci_uint128_magnitude_fits_integer(u, f.negative, to->sz * 8, uns))
        return FOLD_FAIL;
    if(f.negative) u = ci_uint128_sub(zero, u);
    ci_uint128_write(to->bits, to->sz, u);
    return 0;
}

static
int
ci_fold_truth(CiLowerCtx* ctx, const CiFoldValue* v, _Bool* truth){
    if(ccqt_is_narrow_float(v->type)){
        uint16_t bits;
        memcpy(&bits, v->bits, sizeof bits);
        *truth = ci_narrow_float_nonzero(bits);
    }
    else if(ci_falu_type(v->type)){
        if(ctx->static_initializer){
            switch(ci_float_width(v->type, ctx->ldbl_fmt)){
                case 80: *truth = ci_float80_nonzero(ci_float80_read(v->bits)); return 0;
                case 128: *truth = ci_float128_nonzero(ci_float128_read(v->bits)); return 0;
                case 32:{
                    float value;
                    memcpy(&value, v->bits, sizeof value);
                    *truth = value != 0;
                    return 0;
                }
                case 64:{
                    double value;
                    memcpy(&value, v->bits, sizeof value);
                    *truth = value != 0;
                    return 0;
                }
            }
        }
        CiFoldFloat f;
        int err = ci_fold_float_read(ctx, v, &f);
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
    return ci_fold_truth(ctx, &value, truth);
}

static
int
ci_fold_operand(CiInterpreter* ci, CiLowerCtx* ctx, CcExpr* e, CiFoldValue* folded){
    return ctx->static_initializer ? ci_static_fold(ci, e, folded) : ci_fold_expr(ci, ctx, e, folded);
}

static int
ci_fold_read_inputs(CiInterpreter* ci, CiLowerCtx* ctx, CcExpr* e, unsigned depth){
    if(depth >= 256 || e->type.is_volatile || e->type.is_atomic) return FOLD_FAIL;
    int err;
    switch((uint32_t)e->kind){
        case CC_EXPR_BLOB:
        case CC_EXPR_VALUE:
        case CC_EXPR_FUNCTION:
            return 0;
        case CC_EXPR_VARIABLE:
            if(!ci_foldable_variable(e)) return FOLD_FAIL;
            return ci_fold_read_inputs(ci, ctx, e->var->initializer, depth+1);
        case CC_EXPR_DOT: case CC_EXPR_ARROW:
            return ci_fold_read_inputs(ci, ctx, e->values[0], depth+1);
        case CC_EXPR_ADDR:
            if(e->lhs->kind == CC_EXPR_VARIABLE) return 0;
            return ci_fold_read_inputs(ci, ctx, e->lhs, depth+1);
        case CC_EXPR_DEREF: case CC_EXPR_OBJECT_VIEW:
            return ci_fold_read_inputs(ci, ctx, e->lhs, depth+1);
        case CC_EXPR_TERNARY:{
            _Bool truth;
            err = ci_fold_condition(ci, ctx, e->lhs, &truth);
            if(err) return err;
            return ci_fold_read_inputs(ci, ctx, e->values[truth ? 0 : 1], depth+1);
        }
        case CC_EXPR_INIT_LIST: case CC_EXPR_COMPOUND_LITERAL:
            for(size_t i = 0; i < e->init_list->count; i++){
                CcExpr* value = e->init_list->entries[i].value;
                if(!value) continue;
                err = ci_fold_read_inputs(ci, ctx, value, depth+1);
                if(err) return err;
            }
            return 0;
        case CC_EXPR_CAST:
            if(e->type.unqual == e->lhs->type.unqual
                || ccqt_kind(e->type) == CC_POINTER || ccqt_kind(e->type) == CC_SLICE)
                return ci_fold_read_inputs(ci, ctx, e->lhs, depth+1);
            break;
        case CC_EXPR_TYPE_INTROSPECTION: case CC_EXPR_SRCLOC_REFLECT: case CC_EXPR_SUBSCRIPT:
        case CC_EXPR_SLICE: case CC_EXPR_SLICE_LO: case CC_EXPR_SLICE_HI:
        case CC_EXPR_SLICE_ALL: case CC_EXPR_COMMA:
            err = ci_fold_read_inputs(ci, ctx, e->lhs, depth+1);
            if(err) return err;
            for(size_t i = 0; i < cc_expr_nvalues(e); i++){
                err = ci_fold_read_inputs(ci, ctx, e->values[i], depth+1);
                if(err) return err;
            }
            return 0;
        default: break;
    }
    CiFoldValue ignored;
    return ci_fold_expr(ci, ctx, e, &ignored);
}

static int
ci_fold_read(CiInterpreter* ci, CiLowerCtx* ctx, CcExpr* e, CiFoldValue* result){
    int err = ctx->static_initializer ? 0 : ci_fold_read_inputs(ci, ctx, e, 0);
    if(err) return err;
    CiStaticBuffer buffer = {.bytes=(unsigned char*)result->bits, .size=result->sz,
        .runtime_ctx=ctx->static_initializer ? NULL : ctx};
    err = ci_static_write(ci, &buffer, e, 0, result->sz);
    for(size_t i = 0; !err && i < buffer.relocs.count; i++){
        CiStaticReloc* r = &buffer.relocs.data[i];
        // Literal storage lives with the program. Other symbols must retain
        // their relocations until dependency resolution, not become raw bits.
        if(r->kind != CI_STATIC_RELOC_LITERAL || r->initialize) err = FOLD_FAIL;
        else ci_write_uint(buffer.bytes+r->offset, r->size, (uintptr_t)r->literal+r->addend);
    }
    ma_cleanup(CiStaticReloc)(&buffer.relocs, ci_allocator(ci));
    return err == _cc_not_constant_error || err == _cc_overflow_error ? FOLD_FAIL : err;
}

static
int
ci_fold_expr(CiInterpreter* ci, CiLowerCtx* ctx, CcExpr* e, CiFoldValue* folded){
    if(e->kind == CC_EXPR_BLOB) return FOLD_FAIL;
    if(e->kind==CC_EXPR_VARIABLE && !e->type.is_volatile && !e->type.is_atomic){
        CiInlineBinding* b = ci_inline_binding(ctx,e->var);
        if(b && b->constant){
            *folded = b->value;
            return 0;
        }
    }
    CiFoldCache* cache = ctx->fold_cache;
    if(!cache || ctx->static_initializer)
        return ci_fold_expr_uncached(ci, ctx, e, folded);
    switch((uint32_t)e->kind){
        case CC_EXPR_VALUE:
            return ci_fold_expr_uncached(ci, ctx, e, folded);
        case CC_EXPR_VARIABLE:
            if(!ci_foldable_variable(e)) return FOLD_FAIL;
            break;
        case CC_EXPR_CALL:
        case CC_EXPR_ASSIGN:
        case CC_EXPR_ADDASSIGN: case CC_EXPR_SUBASSIGN:
        case CC_EXPR_MULASSIGN: case CC_EXPR_DIVASSIGN: case CC_EXPR_MODASSIGN:
        case CC_EXPR_BITANDASSIGN: case CC_EXPR_BITORASSIGN: case CC_EXPR_BITXORASSIGN:
        case CC_EXPR_LSHIFTASSIGN: case CC_EXPR_RSHIFTASSIGN:
        case CC_EXPR_PREINC: case CC_EXPR_PREDEC:
        case CC_EXPR_POSTINC: case CC_EXPR_POSTDEC:
            return FOLD_FAIL;
    }
    uintptr_t entry = (uintptr_t)PM_get(&cache->expressions, e);
    if(entry){
        if(entry == 1) return FOLD_FAIL;
        *folded = cache->values.data[entry-2];
        return 0;
    }
    int err;
    err = PM_put(&cache->expressions, ctx->a, e, (void*)(uintptr_t)1);
    if(err) return CI_OOM_ERROR;
    err = ci_fold_expr_uncached(ci, ctx, e, folded);
    if(err == FOLD_FAIL) return FOLD_FAIL;
    if(err) goto error;
    err = ma_push(CiFoldValue)(&cache->values, ctx->a, *folded);
    if(err) goto oom;
    entry = cache->values.count + 1;
    err = PM_put(&cache->expressions, ctx->a, e, (void*)entry);
    if(err) goto oom;
    return 0;
    oom:
    err = CI_OOM_ERROR;
    error:
    // clear the cached FOLD_FAIL
    PM_del(&cache->expressions, e);
    return err;
}

static
int
ci_fold_expr_uncached(CiInterpreter* ci, CiLowerCtx* ctx, CcExpr* e, CiFoldValue* folded){
    if((ccqt_is_fixed_sequence(e->type) || ccqt_kind(e->type) == CC_VLA)) return FOLD_FAIL;
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
    if(cc_eval_pointer_binary_expr(e)){
        int64_t value;
        err = cc_eval_symbolic_binary(&ci->parser, e, 0, &value);
        if(err) return err == CI_OOM_ERROR ? err : FOLD_FAIL;
        ci_write_uint(result.bits, sz, (uint64_t)value);
        *folded = result;
        return 0;
    }
    CiFoldValue l, r;
    CiUint128 zero = ci_uint128_from_uint64(0), u = zero;
    _Bool truth;
    switch((uint32_t)e->kind){
        case CC_EXPR_BLOB:
            return FOLD_FAIL;
        case CC_EXPR_VALUE:
            memcpy(result.bits, e->data, sz);
            *folded = result;
            return 0;
        case CC_EXPR_VARIABLE:
            // I think we can also detect write-once variables.
            if(!ci_foldable_variable(e)) return FOLD_FAIL;
            if(e->var->constexpr_ && ccqt_kind(e->type) == CC_POINTER){
                // Symbolic literal pointers can be constexpr even when the scalar
                // evaluator cannot represent their relocation as numeric bytes.
                CiStaticReloc address = {.kind=CI_STATIC_RELOC_ABSOLUTE};
                err = ci_static_address(ci, e, 0, &address, ctx);
                if(!err && (address.kind == CI_STATIC_RELOC_ABSOLUTE || address.kind == CI_STATIC_RELOC_LITERAL)){
                    uint64_t bits = address.addend;
                    if(address.kind == CI_STATIC_RELOC_LITERAL) bits += (uintptr_t)address.literal;
                    ci_write_uint(result.bits, sz, bits);
                    *folded = result;
                    return 0;
                }
                if(err == CI_OOM_ERROR) return err;
            }
            err = ci_fold_operand(ci, ctx, e->var->initializer, &l);
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
            err = ci_fold_operand(ci, ctx, e->lhs, &l);
            if(err) return err;
            if(e->kind == CC_EXPR_COMMA){
                err = ci_fold_operand(ci, ctx, e->values[0], &r);
            }
            else {
                err = ci_fold_truth(ctx, &l, &truth);
                if(err) return err;
                if(e->kind == CC_EXPR_TERNARY)
                    err = ci_fold_operand(ci, ctx, e->values[truth ? 0 : 1], &r);
                else {
                    if(e->kind == CC_EXPR_LOGNOT) truth = !truth;
                    else if(truth == (e->kind == CC_EXPR_LOGAND)){
                        err = ci_fold_operand(ci, ctx, e->values[0], &r);
                        if(err) return err;
                        err = ci_fold_truth(ctx, &r, &truth);
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
        case CC_EXPR_BIT_BUILTIN: {
            err = ci_fold_operand(ci, ctx, e->lhs, &l);
            if(err) return err;
            CiUint128 arg = zero;
            if(e->bit_builtin.nargs && ((e->bit_builtin.op != CC_BIT_CLZ && e->bit_builtin.op != CC_BIT_CTZ)
                || !ci_uint128_nonzero(ci_fold_integer(ctx, &l)))){
                err = ci_fold_operand(ci, ctx, e->values[0], &r);
                if(err) return err;
                arg = ci_fold_integer(ctx, &r);
                if(!ccqt_is_unsigned(r.type, ctx->char_is_unsigned)
                    && (ci_uint128_hi(arg) >> 63)
                    && (e->bit_builtin.op == CC_BIT_ROTATE_LEFT || e->bit_builtin.op == CC_BIT_ROTATE_RIGHT))
                    return FOLD_FAIL;
            }
            if(!cc_bit_builtin(e->bit_builtin.op, ci_fold_integer(ctx, &l), l.sz*8,
                               arg, e->bit_builtin.nargs, &u)) return FOLD_FAIL;
            break;
        }
        case CC_EXPR_CAST:
        case CC_EXPR_POS:
        case CC_EXPR_NEG:
        case CC_EXPR_BITNOT:
        case CC_EXPR_BSWAP:
            err = ci_fold_operand(ci, ctx, e->lhs, &l);
            if(err) return err;
            if(e->kind == CC_EXPR_CAST
            && (e->cast.checked_index || e->cast.checked_bound)
            && !ci_uint128_fits_integer(ci_fold_integer(ctx, &l), sz*8, ccqt_is_unsigned(e->type, ctx->char_is_unsigned)))
                return FOLD_FAIL;
            if(e->kind == CC_EXPR_CAST && ccqt_is_bool(e->type)){
                err = ci_fold_truth(ctx, &l, &truth);
                if(err) return err;
                u = ci_uint128_from_uint64(truth);
                break;
            }
            if(e->kind == CC_EXPR_CAST && (ci_falu_type(l.type) || ci_falu_type(e->type))){
                if(ctx->static_initializer)
                    return ci_static_numeric_cast(ci, e, folded);
                err = ci_fold_float_cast(ctx, &l, &result);
                if(err) return err;
                *folded = result;
                return 0;
            }
            if((e->kind == CC_EXPR_POS || e->kind == CC_EXPR_NEG) && ci_falu_type(l.type) && l.type.unqual == e->type.unqual){
                // Unary sign operations do not round or quiet signaling NaNs.
                memcpy(result.bits, l.bits, l.sz);
                if(e->kind == CC_EXPR_NEG){
                    if(l.sz == 4){
                        uint32_t bits;
                        memcpy(&bits, result.bits, sizeof bits);
                        bits ^= (uint32_t)1 << 31;
                        memcpy(result.bits, &bits, sizeof bits);
                    }
                    else if(ci_float_width(l.type, ctx->ldbl_fmt) == 16) result.bits[0] ^= (uint64_t)1 << 15;
                    else if(ci_float_width(l.type, ctx->ldbl_fmt) == 80) result.bits[1] ^= (uint64_t)1 << 15;
                    else if(ci_float_width(l.type, ctx->ldbl_fmt) == 128) result.bits[1] ^= (uint64_t)1 << 63;
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
                    switch(l.sz){
                        case 1: n = v; break;
                        case 2: n = bswap16((uint16_t)v); break;
                        case 4: n = bswap32((uint32_t)v); break;
                        case 8: n = bswap64(v); break;
                        default: return FOLD_FAIL;
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
                if(ctx->static_initializer) return ci_static_float_binary(ci, e, folded);
                err = ci_fold_operand(ci, ctx, e->lhs, &l);
                if(err) return err;
                err = ci_fold_operand(ci, ctx, e->values[0], &r);
                if(err) return err;
                err = ci_fold_float_binary(ctx, e->kind, &l, &r, &result);
                if(err) return err;
                *folded = result;
                return 0;
            }
            if((e->kind == CC_EXPR_EQ || e->kind == CC_EXPR_NE)
                && (ccqt_kind(e->lhs->type) == CC_POINTER || ccqt_bt_eq(e->lhs->type, CCBT_nullptr_t))
                && (ccqt_kind(e->values[0]->type) == CC_POINTER || ccqt_bt_eq(e->values[0]->type, CCBT_nullptr_t))){
                err = ci_fold_operand(ci, ctx, e->lhs, &l);
                if(err) return err;
                err = ci_fold_operand(ci, ctx, e->values[0], &r);
                if(err) return err;
                CiUint128 a, b;
                ci_uint128_read(&a, l.bits, l.sz);
                ci_uint128_read(&b, r.bits, r.sz);
                _Bool equal = ci_uint128_eq(a, b);
                u = ci_uint128_from_uint64(e->kind == CC_EXPR_EQ ? equal : !equal);
                break;
            }
            if(!ccqt_is_integer(e->lhs->type) || !ccqt_is_integer(e->values[0]->type)) return FOLD_FAIL;
            err = ci_fold_operand(ci, ctx, e->lhs, &l);
            if(err) return err;
            err = ci_fold_operand(ci, ctx, e->values[0], &r);
            if(err) return err;
            CiUint128 a = ci_fold_integer(ctx, &l), b = ci_fold_integer(ctx, &r);
            if(e->kind == CC_EXPR_MUL && e->binary.checked_size
            && !cc_eval_size_product_valid(a, b, sz*8, e->binary.element_alignment))
                return FOLD_FAIL;
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
            return FOLD_FAIL;
        case CC_EXPR_DOT:
        case CC_EXPR_SUBSCRIPT:
        case CC_EXPR_ARROW:
        case CC_EXPR_DEREF:
        case CC_EXPR_TYPE_INTROSPECTION:
        case CC_EXPR_SRCLOC_REFLECT:
        case CC_EXPR_OBJECT_VIEW:
        case CC_EXPR_COMPOUND_LITERAL:
        case CC_EXPR_INIT_LIST:
            err = ci_fold_read(ci, ctx, e, &result);
            if(err) return err;
            *folded = result;
            return 0;
        case CC_EXPR_ADDR:
        // could elide bounds checks etc.
        case CC_EXPR_SLICE_ALL:
        case CC_EXPR_SLICE:
        case CC_EXPR_SLICE_LO:
        case CC_EXPR_SLICE_HI:
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
