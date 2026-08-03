#ifndef C_CI_OP_H
#define C_CI_OP_H
//
// Copyright © 2026-2026, David Priver <david@davidpriver.com>
//
#include <stdint.h>
#include <stddef.h>
#include "srcloc.h"
#include "cc_stmt.h"
#include "cc_expr.h"
#include "cc_memory_order.h"
#include "../Drp/typed_enum.h"
#include "../Drp/atom.h"

#if !defined __clang__ && !defined _Null_unspecified
#define _Null_unspecified
#endif
#if !defined __clang__ && !defined _Nonnull
#define _Nonnull
#endif

enum CiAluOp TYPED_ENUM(uint32_t){
    CI_ALU_ADD,
    CI_ALU_SUB,
    CI_ALU_MUL,
    CI_ALU_DIV,
    CI_ALU_MOD,
    CI_ALU_AND,
    CI_ALU_OR,
    CI_ALU_XOR,
    CI_ALU_SHL,
    CI_ALU_SHR,
    // unary
    CI_ALU_NEG,
    CI_ALU_NOT,
};
TYPEDEF_ENUM(CiAluOp, uint32_t);

enum CiCmpOp TYPED_ENUM(uint32_t){
    CI_CMP_EQ,
    CI_CMP_NE,
    CI_CMP_LT,
    CI_CMP_GT,
    CI_CMP_LE,
    CI_CMP_GE,
};
TYPEDEF_ENUM(CiCmpOp, uint32_t);

enum CiFaluOp TYPED_ENUM(uint32_t){
    CI_FALU_ADD,
    CI_FALU_SUB,
    CI_FALU_MUL,
    CI_FALU_DIV,
    // unary
    CI_FALU_NEG,
};
TYPEDEF_ENUM(CiFaluOp, uint32_t);

// The hardware read-modify-write set; everything else (mod, shifts, floats,
// 128-bit arithmetic) is a CAS loop in the lowered code.
enum CiAtomicRmwOp TYPED_ENUM(uint32_t){
    CI_ARMW_XCHG,
    CI_ARMW_ADD,
    CI_ARMW_SUB,
    CI_ARMW_AND,
    CI_ARMW_OR,
    CI_ARMW_XOR,
};
TYPEDEF_ENUM(CiAtomicRmwOp, uint32_t);

// The checked-arithmetic ops for __builtin_{add,sub,mul}_overflow.
enum CiCheckedOp TYPED_ENUM(uint32_t){
    CI_CHK_ADD,
    CI_CHK_SUB,
    CI_CHK_MUL,
};
TYPEDEF_ENUM(CiCheckedOp, uint32_t);

// The bit-counting ops for __builtin_popcount/clz/ctz (and l/ll variants).
enum CiBitCountOp TYPED_ENUM(uint32_t){
    CI_BITCNT_POPCOUNT,
    CI_BITCNT_CLZ,
    CI_BITCNT_CTZ,
};
TYPEDEF_ENUM(CiBitCountOp, uint32_t);

enum CiOpKind TYPED_ENUM(uint32_t){
    CI_OP_EVAL,
    CI_OP_EVAL_INTO,
    CI_OP_CONST,
    CI_OP_COPY,
    CI_OP_ALU8,
    CI_OP_ALU16,
    CI_OP_ALU32,
    CI_OP_ALU64,
    CI_OP_ALU128,
    CI_OP_CMP32,
    CI_OP_CMP64,
    CI_OP_CMP128,
    CI_OP_FALU32,
    CI_OP_FALU64,
    CI_OP_FCMP32,
    CI_OP_FCMP64,
    CI_OP_CHECKED,
    CI_OP_BITCOUNT,
    CI_OP_CONVERT,
    CI_OP_ITOF,
    CI_OP_FTOI,
    CI_OP_FTOF,
    CI_OP_SLOT_ADDR,
    CI_OP_VAR_ADDR,
    CI_OP_FUNC_ADDR,
    CI_OP_BOUNDS,
    CI_OP_LOAD,
    CI_OP_STORE,
    CI_OP_MEMCOPY,
    CI_OP_ZERO,
    CI_OP_LOAD_BITFIELD,
    CI_OP_STORE_BITFIELD,
    CI_OP_CALL,
    CI_OP_ISTRUE,
    CI_OP_JUMP,
    CI_OP_JUMP_FALSE,
    CI_OP_JUMP_TRUE,
    CI_OP_RETURN,
    CI_OP_RETURN_SLOT,
    CI_OP_SWITCH,
    CI_OP_ATOMIC_LOAD,
    CI_OP_ATOMIC_STORE,
    CI_OP_ATOMIC_RMW,
    CI_OP_ATOMIC_CAS,
    CI_OP_FENCE,
    CI_OP_ALLOCA,
    CI_OP_BSWAP,
    CI_OP_BUILTIN,
    CI_OP_VA_START,
    CI_OP_VA_ARG,
    // CI_OP_RT_CALL, // call into runtime support function
};
TYPEDEF_ENUM(CiOpKind, uint32_t);

typedef struct CiCallDescriptor CiCallDescriptor;
struct CiCallDescriptor {
    union {
        CcFunction*_Nonnull func_type;
        CcFunc* _Nonnull func;
    };
    CcExpr*_Nonnull expr;
    uint32_t nargs;
    uint32_t arg_sizes[];
};

typedef struct CiSwitchTable CiSwitchTable;
struct CiSwitchTable {
    size_t count;
    CcSwitchEntry data[];
};

typedef struct CiOp CiOp;
struct CiOp {
    union {
        struct {
            CiOpKind kind: 8;
            uint32_t _bitpad: 24;
            uint32_t _pad;
            uint64_t pad[2];
            SrcLoc loc;
        };
        struct {
            // evaluate expr, discard result
            CiOpKind kind: 8; // CI_OP_EVAL
            uint32_t _bitpad: 24;
            uint32_t _pad;
            CcExpr*_Nonnull expr;
            uint64_t pad;
            SrcLoc loc;
        } eval;
        struct {
            // evaluate expr into slots[slot:slot+slot_size]
            CiOpKind kind: 8; // CI_OP_EVAL_INTO
            uint32_t _bitpad: 24;
            uint32_t _pad;
            CcExpr*_Nonnull expr;
            uint32_t slot, slot_size;
            SrcLoc loc;
        } eval_into;
        struct {
            // slots[slot:slot+immsize] = immediate
            CiOpKind kind: 8; // CI_OP_CONST
            uint32_t bt_kind: 7; // for pretty printing, we can remove if we need the bits
            uint32_t is_anon_array: 1; // for pretty printing, we can remove if we need the bits
            uint32_t immsize: 16;
            uint32_t slot;
            uint64_t immediate[2];
            SrcLoc loc;
        } constant;
        struct {
            // slots[slot:slot+slot_size] = slots[src:src+slot_size]
            CiOpKind kind: 8; // CI_OP_COPY
            uint32_t _bitpad: 24;
            uint32_t _pad;
            uint32_t slot, slot_size,
                     src, src_size;
            SrcLoc loc;
        } copy;
        struct {
            // slots[slot:slot+(implict size)] = slots[src] op slots[src2] as integers
            CiOpKind kind: 8; // CI_OP_ALU8, CI_OP_ALU16, CI_OP_ALU32, CI_OP_ALU64, CI_OP_ALU128
            CiAluOp op: 8;
            uint32_t is_unsigned: 1,
                     _bitpad: 15;
            uint32_t slot,
                     src,
                     src2;
            uint32_t pad[2];
            SrcLoc loc;
        } alu;
        struct {
            // slots[slot:slot+slot_size] = (int)(slots[src] cmp slots[src2])
            CiOpKind kind: 8; // CI_OP_CMP32, CI_OP_CMP64, CI_OP_CMP128
            CiCmpOp op: 8;
            uint32_t is_unsigned: 1,
                     _bitpad: 15;
            uint32_t slot,
                     slot_size, // always sizeof(int), but that is target specific and can't be implied by the kind.
                     src,
                     src2;
            uint32_t pad;
            SrcLoc loc;
        } cmp;
        struct {
            // slots[slot] = slots[src] op slots[src2]
            CiOpKind kind: 8; // CI_OP_FALU32, CI_OP_FALU64
            CiFaluOp op: 8;
            uint32_t _bitpad: 16;
            uint32_t slot,
                     slot_size,
                     src,
                     src2;
            uint32_t pad;
            SrcLoc loc;
        } falu32, falu64;
        struct {
            // slots[slot:slot+slot_size] = (int)(slots[src] cmp slots[src2])
            CiOpKind kind: 8; // CI_OP_FCMP32, CI_OP_FCMP64
            CiCmpOp op: 8;
            uint32_t _bitpad: 16;
            uint32_t slot,
                     slot_size,
                     src,
                     src2;
            uint32_t pad;
            SrcLoc loc;
        } fcmp32, fcmp64;
        struct {
            CiOpKind kind: 8; // CI_OP_CHECKED
            CiCheckedOp op: 8;
            uint32_t src_size: 4,
                     src2_size: 4,
                     res_size: 4,
                     src_unsigned: 1,
                     src2_unsigned: 1,
                     res_unsigned: 1,
                     _bitpad: 1;
            uint32_t result,
                     overflow,
                     src,
                     src2;
            uint32_t pad;
            SrcLoc loc;
        } checked;
        struct {
            CiOpKind kind: 8; // CI_OP_BITCOUNT
            CiBitCountOp op: 8;
            uint32_t src_size: 8,
                     _bitpad: 8;
            uint32_t slot, slot_size,
                     src;
            uint64_t pad;
            SrcLoc loc;
        } bitcount;
        struct {
            CiOpKind kind: 8; // CI_OP_CONVERT, CI_OP_ITOF, CI_OP_FTOI, CI_OP_FTOF
            uint32_t is_unsigned: 1,
                     _bitpad: 23;
            uint32_t pad;
            uint32_t slot, slot_size,
                     src, src_size;
            SrcLoc loc;
        } convert, itof, ftoi, ftof;
        struct {
            // slots[slot] = &slots[src]
            CiOpKind kind: 8; // CI_OP_SLOT_ADDR
            uint32_t _bitpad: 24;
            uint32_t slot, slot_size, src;
            uint64_t pad[1];
            SrcLoc loc;
        } slot_addr;
        struct {
            // slots[slot] = var's resolved storage address; a GOT load, read
            // at execution because lowering can run before the variable
            // resolves
            CiOpKind kind: 8; // CI_OP_VAR_ADDR
            uint32_t _bitpad: 24;
            uint32_t pad;
            uint32_t slot, slot_size;
            CcVariable*_Nonnull var;
            SrcLoc loc;
        } var_addr;
        struct {
            // slots[slot] = func's resolved function pointer; a GOT load, read
            // at execution because lowering can run before the closure or
            // native symbol resolves
            CiOpKind kind: 8; // CI_OP_FUNC_ADDR
            uint32_t _bitpad: 24;
            uint32_t pad;
            uint32_t slot, slot_size;
            CcFunc*_Nonnull func;
            SrcLoc loc;
        } func_addr;
        struct {
            // trap unless the 8-byte unsigned index in slots[src] is in range
            // of the 8-byte length in slots[src2]; inclusive permits
            // index == length (address-of one-past-the-end)
            CiOpKind kind: 8; // CI_OP_BOUNDS
            uint32_t inclusive: 1,
                     index_signed: 1,
                     _bitpad: 22;
            uint32_t pad;
            uint32_t src, src_size,
                     src2, src2_size;
            SrcLoc loc;
        } bounds;
        struct {
            // ptr[offset:] = slots[src:src+src_size], ptr read from slots[slot]
            CiOpKind kind: 8; // CI_OP_STORE
            uint32_t _bitpad: 24;
            uint32_t pad;
            uint32_t slot,
                     src,
                     src_size,
                     offset;
            SrcLoc loc;
        } store;
        struct {
            // slots[slot:slot+slot_size] = ptr[offset:], ptr read from slots[src]
            CiOpKind kind: 8; // CI_OP_LOAD
            uint32_t _bitpad: 24;
            uint32_t pad;
            uint32_t slot,
                     slot_size,
                     src,
                     offset;
            SrcLoc loc;
        } load;
        struct {
            // dst[offset : offset+size] = src[src_offset : src_offset+size],
            // dst read from slots[slot], src read from slots[src]; the
            // regions may overlap exactly (self assignment)
            CiOpKind kind: 8; // CI_OP_MEMCOPY
            uint32_t _bitpad: 24;
            uint32_t size;
            uint32_t slot,
                     offset,
                     src,
                     src_offset;
            SrcLoc loc;
        } memcopy;
        struct {
            CiOpKind kind: 8; // CI_OP_ZERO
            uint32_t _bitpad: 24;
            uint32_t size;
            uint32_t slot, offset;
            uint64_t pad;
            SrcLoc loc;
        } zero;
        struct {
            // like store, but a read-modify-write: insert the low bit_width
            // bits of slots[src:src+src_size] at bit_offset of the
            // src_size-byte storage unit at ptr[offset:]
            CiOpKind kind: 8; // CI_OP_STORE_BITFIELD
            uint32_t bit_offset: 8,
                     bit_width: 8,
                     is_signed: 1,
                     _bitpad: 7;
            uint32_t pad;
            uint32_t slot,
                     src,
                     src_size,
                     offset;
            SrcLoc loc;
        } store_bf;
        struct {
            // like load, but the slot_size bytes at ptr[offset:] are a
            // bitfield storage unit: extract bit_width bits at bit_offset,
            // extend per is_signed
            CiOpKind kind: 8; // CI_OP_LOAD_BITFIELD
            uint32_t bit_offset: 8,
                     bit_width: 8,
                     is_signed: 1,
                     _bitpad: 7;
            uint32_t pad;
            uint32_t slot, slot_size,
                     src,
                     offset;
            SrcLoc loc;
        } load_bf;
        struct {
            // slots[slot:slot+slot_size] = atomic load of ptr[offset:], ptr
            // read from slots[src]; slot_size is a power of two <= 16
            CiOpKind kind: 8; // CI_OP_ATOMIC_LOAD
            CcMemoryOrder memorder: 4;
            uint32_t _bitpad: 20;
            uint32_t pad;
            uint32_t slot, slot_size,
                     src,
                     offset;
            SrcLoc loc;
        } atomic_load;
        struct {
            // atomic store of slots[src:src+src_size] to ptr[offset:], ptr
            // read from slots[slot]; src_size is a power of two <= 16
            CiOpKind kind: 8; // CI_OP_ATOMIC_STORE
            CcMemoryOrder memorder: 4;
            uint32_t _bitpad: 20;
            uint32_t pad;
            uint32_t slot,
                     src,
                     src_size,
                     offset;
            SrcLoc loc;
        } atomic_store;
        struct {
            // atomically: old = ptr[offset:]; ptr[offset:] = old <op>
            // slots[src2]; slots[slot:slot+slot_size] = old. ptr read from
            // slots[src]. slot_size is also the operand size: a power of two
            // <= 8, or 16 for xchg only. slot is always a valid slot;
            // discard means the old value is unused
            CiOpKind kind: 8; // CI_OP_ATOMIC_RMW
            CiAtomicRmwOp op: 8;
            CcMemoryOrder memorder: 4;
            uint32_t discard: 1,
                     _bitpad: 11;
            uint32_t offset;
            uint32_t slot, slot_size,
                     src,
                     src2;
            SrcLoc loc;
        } atomic_rmw;
        struct {
            // atomically: old = ptr[offset:]; if old == slots[expected]:
            // ptr[offset:] = slots[desired]. slots[expected:expected+size] =
            // old (a no-op when the exchange happened); slots[slot] = 1-byte
            // canonical bool of success. ptr read from slots[src]; weak
            // permits spurious failure; size is a power of two <= 16
            CiOpKind kind: 8; // CI_OP_ATOMIC_CAS
            CcMemoryOrder memorder: 4;
            CcMemoryOrder fail_memorder: 4;
            uint32_t weak: 1,
                     size: 5,
                     _bitpad: 10;
            uint32_t offset;
            uint32_t slot,
                     src,
                     expected,
                     desired;
            SrcLoc loc;
        } atomic_cas;
        struct {
            // memory fence; is_signal: compiler barrier only, no instruction
            CiOpKind kind: 8; // CI_OP_FENCE
            CcMemoryOrder memorder: 4;
            uint32_t is_signal: 1,
                     _bitpad: 19;
            uint32_t _pad;
            uint64_t pad[2];
            SrcLoc loc;
        } fence;
        struct {
            CiOpKind kind: 8; // CI_OP_CALL
            uint32_t is_indirect: 1, is_variadic:1, _pad:22;
            uint32_t ret_slot,
                     ret_size,
                     argv_slot;
            CiCallDescriptor *_Nonnull descrip;
            SrcLoc loc;
        } call;
        struct {
            // slots[slot] = slot_size-byte 0/1 of truthy(slots[src:src+src_size]);
            // float_kind = CcBasicTypeKind when the source is a float, else 0;
            // negate computes !truthy instead
            CiOpKind kind: 8; // CI_OP_ISTRUE
            uint32_t float_kind: 16,
                     negate: 1,
                     _bitpad: 7;
            uint32_t pad;
            uint32_t slot, slot_size,
                     src, src_size;
            SrcLoc loc;
        } istrue;
        struct {
            // pc = jump
            CiOpKind kind: 8; // CI_OP_JUMP
            uint32_t _bitpad: 24;
            uint32_t jump;
            uint64_t pad[2];
            SrcLoc loc;
        } jump;
        struct {
            // if !slots[slot] (jump_false) or slots[slot] (jump_true):
            // pc = jump; slot holds a canonical 0/1
            CiOpKind kind: 8; // CI_OP_JUMP_FALSE, CI_OP_JUMP_TRUE
            uint32_t _bitpad: 24;
            uint32_t jump;
            uint32_t slot, slot_size;
            uint64_t pad[1];
            SrcLoc loc;
        } jump_false, jump_true;
        struct {
            // return with no value; pc = end
            CiOpKind kind: 8; // CI_OP_RETURN
            uint32_t _bitpad: 24;
            uint32_t _pad;
            uint64_t pad[2];
            SrcLoc loc;
        } return_;
        struct {
            // copy slots[src:src+src_size] into return_buf; pc = end
            CiOpKind kind: 8; // CI_OP_RETURN_SLOT
            uint32_t _bitpad: 24;
            uint32_t _pad;
            uint32_t src, src_size;
            uint64_t pad[1];
            SrcLoc loc;
        } return_slot;
        struct {
            // multi-way conditional jump on slots[slot]; binary search table,
            // no match: pc = jump (default/exit); is_unsigned: the value is
            // unsigned
            CiOpKind kind: 8; // CI_OP_SWITCH
            uint32_t is_unsigned: 1,
                     _bitpad: 23;
            uint32_t jump;
            uint32_t slot, slot_size;
            CiSwitchTable*_Null_unspecified table;
            SrcLoc loc;
        } switch_;
        struct {
            // evaluate expr into slots[slot:slot+slot_size]
            CiOpKind kind: 8; // CI_OP_ALLOCA
            uint32_t _bitpad: 24;
            uint32_t _pad;
            uint32_t slot, src;
            uint64_t pad;
            SrcLoc loc;
        } alloca;
        struct {
            // evaluate bswap into slots[slot:slot+slot_size]
            CiOpKind kind: 8; // CI_OP_BSWAP
            uint32_t _bitpad: 24;
            uint32_t size;
            uint32_t slot, src;
            uint64_t pad;
            SrcLoc loc;
        } bswap;
        struct {
            CiOpKind kind: 8; // CI_OP_BUILTIN
            CcBuiltinOp op: 8;
            uint32_t _bitpad: 16;
            uint32_t _pad;
            uint64_t pad[2];
            SrcLoc loc;
        } builtin;
        struct {
            CiOpKind kind: 8; // CI_OP_VA_START
            uint32_t _bitpad: 24;
            uint32_t _pad;
            CcTarget target;
            uint32_t slot;
            uint64_t pad[1];
            SrcLoc loc;
        } va_start_;
        struct {
            CiOpKind kind: 8; // CI_OP_VA_ARG
            uint32_t is_fp: 1,
                     _bitpad: 23;
            uint32_t slot, slot_size,
                     src;
            CcTarget target;
            SrcLoc loc;
        } va_arg_;
    };
};
_Static_assert(sizeof(CiOp) == 32, "");

#ifndef MARRAY_CIOP
#define MARRAY_CIOP
#define MARRAY_T CiOp
#include "../Drp/Marray.h"
#endif

// The lowered code of one function, hung off CcFunc.interp_ops.
typedef struct CiFuncOps CiFuncOps;
struct CiFuncOps {
    Marray(CiOp) code;
};

#endif
