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

enum CiAtomicRmwOp TYPED_ENUM(uint32_t){
    CI_ARMW_XCHG,
    CI_ARMW_ADD,
    CI_ARMW_SUB,
    CI_ARMW_AND,
    CI_ARMW_OR,
    CI_ARMW_XOR,
};
TYPEDEF_ENUM(CiAtomicRmwOp, uint32_t);

enum CiCheckedOp TYPED_ENUM(uint32_t){
    CI_CHK_ADD,
    CI_CHK_SUB,
    CI_CHK_MUL,
};
TYPEDEF_ENUM(CiCheckedOp, uint32_t);

enum CiBitCountOp TYPED_ENUM(uint32_t){
    CI_BITCNT_POPCOUNT,
    CI_BITCNT_CLZ,
    CI_BITCNT_CTZ,
};
TYPEDEF_ENUM(CiBitCountOp, uint32_t);

enum CiRuntimeOp TYPED_ENUM(uint32_t){
    CI_RT_INTERN,
    CI_RT_HOTSWAP,
    CI_RT_SRCLOC_REFLECT,
    CI_RT_COMPILE,
    CI_RT_TYPE_REFLECT,
    CI_RT_MODULE_REFLECT,
    CI_RT_TYPE_VALIDATE,
    CI_RT_MODULE_VALIDATE,
};
TYPEDEF_ENUM(CiRuntimeOp, uint32_t);

enum CiOpKind TYPED_ENUM(uint32_t){
    CI_OP_CONST,
    CI_OP_COPY,
    CI_OP_ALU_IMM32,
    CI_OP_ALU_IMM64,
    CI_OP_INDEX,
    CI_OP_ALU8,
    CI_OP_ALU16,
    CI_OP_ALU32,
    CI_OP_ALU64,
    CI_OP_ALU128,
    CI_OP_CMP32,
    CI_OP_CMP64,
    CI_OP_CMP128,
    CI_OP_CMP_JUMP32,
    CI_OP_CMP_JUMP64,
    CI_OP_FALU32,
    CI_OP_FALU64,
    CI_OP_FALU80,
    CI_OP_FALU128,
    CI_OP_FCMP32,
    CI_OP_FCMP64,
    CI_OP_FCMP80,
    CI_OP_FCMP128,
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
    CI_OP_STORE_IMM,
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
    CI_OP_RT_CALL,
};
TYPEDEF_ENUM(CiOpKind, uint32_t);

typedef struct CiCallDescriptor CiCallDescriptor;
struct CiCallDescriptor {
    CcFunc* _Nonnull func;
    CcFunction*_Nonnull call_type; // effective signature, including promoted variadic arguments
    uint32_t nargs;
    uint32_t fixed_size, varargs_offset, args_size;
    uint32_t* _Nonnull arg_offsets;
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
            CiOpKind kind: 8; // CI_OP_RT_CALL
            CiRuntimeOp op: 8;
            uint32_t nargs: 7;
            uint32_t member_by_name: 1;
            uint32_t reflect_op: 8; // CcTypeIntrospectionOp, CcModuleOp, or CcSrcLocOp
            uint32_t slot, slot_size;
            uint32_t args[3]; // reflection: receiver, optional index/name, symbol expected type
            SrcLoc loc;
        } rt_call;
        struct {
            CiOpKind kind: 8; // CI_OP_CONST
            uint32_t bt_kind: 7; // for pretty printing
            uint32_t is_anon_array: 1; // for pretty printing
            uint32_t immsize: 16;
            uint32_t slot;
            uint64_t immediate[2];
            SrcLoc loc;
        } constant;
        struct {
            CiOpKind kind: 8; // CI_OP_COPY
            uint32_t _bitpad: 24;
            uint32_t _pad;
            uint32_t slot, slot_size,
                     src, src_size;
            SrcLoc loc;
        } copy;
        struct {
            CiOpKind kind: 8; // CI_OP_ALU_IMM32, CI_OP_ALU_IMM64
            CiAluOp op: 8;
            uint32_t is_unsigned: 1, _bitpad: 15;
            uint32_t slot, src, _pad;
            uint64_t immediate;
            SrcLoc loc;
        } alu_imm;
        struct {
            CiOpKind kind: 8; // CI_OP_INDEX
            uint32_t ptr_size: 4, index_size: 4, index_unsigned: 1, _bitpad: 15;
            uint32_t slot, base, index, scale, _pad;
            SrcLoc loc;
        } index;
        struct {
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
            CiOpKind kind: 8; // CI_OP_CMP32, CI_OP_CMP64, CI_OP_CMP128
            CiCmpOp op: 8;
            uint32_t is_unsigned: 1,
                     _bitpad: 15;
            uint32_t slot,
                     slot_size,
                     src,
                     src2;
            uint32_t pad;
            SrcLoc loc;
        } cmp;
        struct {
            CiOpKind kind: 8; // CI_OP_CMP_JUMP32, CI_OP_CMP_JUMP64
            CiCmpOp op: 8;
            uint32_t is_unsigned: 1, when_true: 1, _bitpad: 14;
            uint32_t jump, _pad, src, src2, _pad2;
            SrcLoc loc;
        } cmp_jump;
        struct {
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
                     src_float: 8,
                     dst_float: 8,
                     _bitpad: 7;
            uint32_t pad;
            uint32_t slot, slot_size,
                     src, src_size;
            SrcLoc loc;
        } convert, itof, ftoi, ftof;
        struct {
            CiOpKind kind: 8; // CI_OP_SLOT_ADDR
            uint32_t _bitpad: 24;
            uint32_t slot, slot_size, src;
            uint64_t pad[1];
            SrcLoc loc;
        } slot_addr;
        struct {
            CiOpKind kind: 8; // CI_OP_VAR_ADDR
            uint32_t _bitpad: 24;
            uint32_t pad;
            uint32_t slot, slot_size;
            CcVariable*_Nonnull var;
            SrcLoc loc;
        } var_addr;
        struct {
            CiOpKind kind: 8; // CI_OP_FUNC_ADDR
            uint32_t _bitpad: 24;
            uint32_t pad;
            uint32_t slot, slot_size;
            CcFunc*_Nonnull func;
            SrcLoc loc;
        } func_addr;
        struct {
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
            CiOpKind kind: 8; // CI_OP_STORE_IMM
            uint32_t size: 8, _bitpad: 16;
            uint32_t slot, offset, _pad;
            uint64_t immediate;
            SrcLoc loc;
        } store_imm;
        struct {
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
            uint32_t is_indirect: 1, _pad:23;
            uint32_t ret_slot,
                     ret_size,
                     args_slot;
            CiCallDescriptor *_Nonnull descrip;
            SrcLoc loc;
        } call;
        struct {
            CiOpKind kind: 8; // CI_OP_ISTRUE
            uint32_t float_width: 16,
                     negate: 1,
                     _bitpad: 7;
            uint32_t pad;
            uint32_t slot, slot_size,
                     src, src_size;
            SrcLoc loc;
        } istrue;
        struct {
            CiOpKind kind: 8; // CI_OP_JUMP
            uint32_t _bitpad: 24;
            uint32_t jump;
            uint64_t pad[2];
            SrcLoc loc;
        } jump;
        struct {
            CiOpKind kind: 8; // CI_OP_JUMP_FALSE, CI_OP_JUMP_TRUE
            uint32_t _bitpad: 24;
            uint32_t jump;
            uint32_t slot, slot_size;
            uint64_t pad[1];
            SrcLoc loc;
        } jump_false, jump_true;
        struct {
            CiOpKind kind: 8; // CI_OP_RETURN
            uint32_t _bitpad: 24;
            uint32_t _pad;
            uint64_t pad[2];
            SrcLoc loc;
        } return_;
        struct {
            CiOpKind kind: 8; // CI_OP_RETURN_SLOT
            uint32_t _bitpad: 24;
            uint32_t _pad;
            uint32_t src, src_size;
            uint64_t pad[1];
            SrcLoc loc;
        } return_slot;
        struct {
            CiOpKind kind: 8; // CI_OP_SWITCH
            uint32_t is_unsigned: 1,
                     _bitpad: 23;
            uint32_t jump;
            uint32_t slot, slot_size;
            CiSwitchTable*_Null_unspecified table;
            SrcLoc loc;
        } switch_;
        struct {
            CiOpKind kind: 8; // CI_OP_ALLOCA
            uint32_t _bitpad: 24;
            uint32_t _pad;
            uint32_t slot, src;
            uint64_t pad;
            SrcLoc loc;
        } alloca;
        struct {
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
_Static_assert(offsetof(CiOp, cmp.src) == offsetof(CiOp, cmp_jump.src), "comparison operand layout");
_Static_assert(offsetof(CiOp, cmp.src2) == offsetof(CiOp, cmp_jump.src2), "comparison operand layout");
_Static_assert(offsetof(CiOp, jump_false.jump) == offsetof(CiOp, cmp_jump.jump), "branch target layout");

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
