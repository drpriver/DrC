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
#include "cc_target.h"
#include "nc_native_call.h"
#include "ci_op_list.h"
#include "../Drp/typed_enum.h"
#include "../Drp/atom.h"
#include "../Drp/ByteBuilder.h"

#if !defined __clang__ && !defined _Null_unspecified
#define _Null_unspecified
#endif
#if !defined __clang__ && !defined _Nonnull
#define _Nonnull
#endif

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

enum CiOpKind TYPED_ENUM(uint32_t){
    CI_OPCODE_NATIVE_LIST(CI_OPCODE_ENUM)
#define CI_OP_ENUM(name, ...) CI_OP_##name,
    CI_OPCODE_FLAT_LIST(CI_OP_ENUM)
#undef CI_OP_ENUM
    CI_OP_COUNT,
};
TYPEDEF_ENUM(CiOpKind, uint32_t);
#undef CI_OPCODE_ENUM
_Static_assert(CI_OP_COUNT < 65536, "16-bit opcode");


// Printing categories are not opcodes and are never stored in instructions.
enum CiOpFamily {
    CI_F_CONST = CI_OP_COUNT,
    CI_F_LOAD,
    CI_F_STORE,
    CI_F_STORE_IMM,
    CI_F_ZERO,
    CI_F_COPY,
    CI_F_MEMCOPY,
    CI_F_CONVERT,
    CI_F_CHECKED,
    CI_F_CHECKED_ARITH,
    CI_F_ISTRUE,
    CI_F_ITOF,
    CI_F_FTOI,
    CI_F_FTOF,
    CI_F_ABI_MOVE,
    CI_F_ABI_PAIR,
    CI_F_ABI_TRANSFER,
    CI_F_JUMP_FALSE,
    CI_F_JUMP_TRUE,
    CI_F_BSWAP,
    CI_F_BOUNDS,
    CI_F_INDEX,
    CI_F_LOAD_BITFIELD,
    CI_F_LOAD_BITFIELD_BYTES,
    CI_F_LOAD_SLOT_BITFIELD,
    CI_F_LOAD_SLOT_BITFIELD_BYTES,
    CI_F_BITFIELD_JUMP,
    CI_F_STORE_BITFIELD,
    CI_F_STORE_BITFIELD_BYTES,
    CI_F_CALL,
    CI_F_SWITCH,
    CI_F_BUILTIN,
    CI_F_ALLOCA,
    CI_F_FENCE,
    CI_F_BIT_BUILTIN,
    CI_F_ATOMIC_LOAD,
    CI_F_ATOMIC_STORE,
    CI_F_ATOMIC_RMW,
    CI_F_ATOMIC_CAS,
    CI_F_VA_ARG,
};

static inline
uint32_t
ci_op_family(CiOpKind kind){
    switch(kind){
#define CI_FAMILY_CASE(name, ...) case CI_OP_##name: return CI_F_CONST;
        CI_CONST_OPS(CI_FAMILY_CASE)
#undef CI_FAMILY_CASE
#define CI_FAMILY_CASE(name, ...) case CI_OP_##name: return CI_F_LOAD;
        CI_LOAD_OPS(CI_FAMILY_CASE)
#undef CI_FAMILY_CASE
#define CI_FAMILY_CASE(name, ...) case CI_OP_##name: return CI_F_STORE;
        CI_STORE_OPS(CI_FAMILY_CASE)
#undef CI_FAMILY_CASE
#define CI_FAMILY_CASE(name, ...) case CI_OP_##name: return CI_F_STORE_IMM;
        CI_STORE_IMM_OPS(CI_FAMILY_CASE)
#undef CI_FAMILY_CASE
#define CI_FAMILY_CASE(name, ...) case CI_OP_##name: return CI_F_ZERO;
        CI_ZERO_OPS(CI_FAMILY_CASE)
#undef CI_FAMILY_CASE
#define CI_FAMILY_CASE(name, ...) case CI_OP_##name: return CI_F_COPY;
        CI_COPY_OPS(CI_FAMILY_CASE)
#undef CI_FAMILY_CASE
#define CI_FAMILY_CASE(name, ...) case CI_OP_##name: return CI_F_MEMCOPY;
        CI_MEMCOPY_OPS(CI_FAMILY_CASE)
#undef CI_FAMILY_CASE
#define CI_FAMILY_CASE(name, ...) case CI_OP_##name: return CI_F_CONVERT;
        CI_EXTEND_OPS(CI_FAMILY_CASE)
#undef CI_FAMILY_CASE
#define CI_FAMILY_CASE(name, ...) case CI_OP_##name: return CI_F_CHECKED;
        CI_OVERFLOW_OPS(CI_FAMILY_CASE)
#undef CI_FAMILY_CASE
#define CI_FAMILY_CASE(name, ...) case CI_OP_##name: return CI_F_CHECKED_ARITH;
        CI_CHECKED_OPS(CI_FAMILY_CASE)
#undef CI_FAMILY_CASE
#define CI_FAMILY_CASE(name, ...) case CI_OP_##name: return CI_F_ISTRUE;
        CI_TRUTH_OPS(CI_FAMILY_CASE)
#undef CI_FAMILY_CASE
#define CI_FAMILY_CASE(name, ...) case CI_OP_##name: return CI_F_ITOF;
        CI_ITOF_OPS(CI_FAMILY_CASE)
#undef CI_FAMILY_CASE
#define CI_FAMILY_CASE(name, ...) case CI_OP_##name: return CI_F_FTOI;
        CI_FTOI_OPS(CI_FAMILY_CASE)
#undef CI_FAMILY_CASE
#define CI_FAMILY_CASE(name, ...) case CI_OP_##name: return CI_F_FTOF;
        CI_FTOF_OPS(CI_FAMILY_CASE)
#undef CI_FAMILY_CASE
#define CI_FAMILY_CASE(name, ...) case CI_OP_##name: return CI_F_ABI_PAIR;
        CI_ABI_PAIR_OPS(CI_FAMILY_CASE)
#undef CI_FAMILY_CASE
#define CI_FAMILY_CASE(name, ...) case CI_OP_##name: return CI_F_ABI_MOVE;
        CI_ABI_MOVE_OPS(CI_FAMILY_CASE)
        CI_ABI_GP_MOVE_OPS(CI_FAMILY_CASE)
        CI_ABI_REG_MOVE_OPS(CI_FAMILY_CASE)
#undef CI_FAMILY_CASE
#define CI_FAMILY_CASE(name, ...) case CI_OP_##name: return CI_F_ABI_TRANSFER;
        CI_ABI_TRANSFER_OPS(CI_FAMILY_CASE)
#undef CI_FAMILY_CASE
#define CI_FAMILY_CASE(name, ...) case CI_OP_##name: return CI_F_JUMP_FALSE;
        CI_JUMP_FALSE_OPS(CI_FAMILY_CASE)
#undef CI_FAMILY_CASE
#define CI_FAMILY_CASE(name, ...) case CI_OP_##name: return CI_F_JUMP_TRUE;
        CI_JUMP_TRUE_OPS(CI_FAMILY_CASE)
#undef CI_FAMILY_CASE
#define CI_FAMILY_CASE(name, ...) case CI_OP_##name: return CI_F_BSWAP;
        CI_BSWAP_OPS(CI_FAMILY_CASE)
#undef CI_FAMILY_CASE
#define CI_FAMILY_CASE(name, ...) case CI_OP_##name: return CI_F_BOUNDS;
        CI_BOUNDS_OPS(CI_FAMILY_CASE)
#undef CI_FAMILY_CASE
#define CI_FAMILY_CASE(name, ...) case CI_OP_##name: return CI_F_INDEX;
        CI_INDEX_OPS(CI_FAMILY_CASE)
#undef CI_FAMILY_CASE
#define CI_FAMILY_CASE(name, ...) case CI_OP_##name: return CI_F_LOAD_BITFIELD;
        CI_LOAD_BITFIELD_OPS(CI_FAMILY_CASE)
#undef CI_FAMILY_CASE
#define CI_FAMILY_CASE(name, ...) case CI_OP_##name: return CI_F_LOAD_BITFIELD;
        CI_LOAD_BITFIELD_128_OPS(CI_FAMILY_CASE)
#undef CI_FAMILY_CASE
#define CI_FAMILY_CASE(name, ...) case CI_OP_##name: return CI_F_LOAD_BITFIELD_BYTES;
        CI_LOAD_BITFIELD_BYTES_OPS(CI_FAMILY_CASE)
#undef CI_FAMILY_CASE
#define CI_FAMILY_CASE(name, ...) case CI_OP_##name: return CI_F_LOAD_BITFIELD_BYTES;
        CI_LOAD_BITFIELD_BYTES_128_OPS(CI_FAMILY_CASE)
#undef CI_FAMILY_CASE
#define CI_FAMILY_CASE(name, ...) case CI_OP_##name: return CI_F_BITFIELD_JUMP;
        CI_BITFIELD_JUMP_OPS(CI_FAMILY_CASE)
#undef CI_FAMILY_CASE
#define CI_FAMILY_CASE(name, ...) case CI_OP_##name: return CI_F_LOAD_SLOT_BITFIELD;
        CI_LOAD_SLOT_BITFIELD_OPS(CI_FAMILY_CASE)
        CI_LOAD_SLOT_BITFIELD_128_OPS(CI_FAMILY_CASE)
#undef CI_FAMILY_CASE
#define CI_FAMILY_CASE(name, ...) case CI_OP_##name: return CI_F_LOAD_SLOT_BITFIELD_BYTES;
        CI_LOAD_SLOT_BITFIELD_BYTES_OPS(CI_FAMILY_CASE)
        CI_LOAD_SLOT_BITFIELD_BYTES_128_OPS(CI_FAMILY_CASE)
#undef CI_FAMILY_CASE
#define CI_FAMILY_CASE(name, ...) case CI_OP_##name: return CI_F_STORE_BITFIELD;
        CI_STORE_BITFIELD_OPS(CI_FAMILY_CASE)
#undef CI_FAMILY_CASE
#define CI_FAMILY_CASE(name, ...) case CI_OP_##name: return CI_F_STORE_BITFIELD;
        CI_STORE_BITFIELD_128_OPS(CI_FAMILY_CASE)
#undef CI_FAMILY_CASE
#define CI_FAMILY_CASE(name, ...) case CI_OP_##name: return CI_F_STORE_BITFIELD_BYTES;
        CI_STORE_BITFIELD_BYTES_OPS(CI_FAMILY_CASE)
#undef CI_FAMILY_CASE
#define CI_FAMILY_CASE(name, ...) case CI_OP_##name: return CI_F_STORE_BITFIELD_BYTES;
        CI_STORE_BITFIELD_BYTES_128_OPS(CI_FAMILY_CASE)
#undef CI_FAMILY_CASE
#define CI_FAMILY_CASE(name, ...) case CI_OP_##name: return CI_F_CALL;
        CI_CALL_OPS(CI_FAMILY_CASE)
#undef CI_FAMILY_CASE
#define CI_FAMILY_CASE(name, ...) case CI_OP_##name: return CI_F_SWITCH;
        CI_SWITCH_DENSE_OPS(CI_FAMILY_CASE)
#undef CI_FAMILY_CASE
#define CI_FAMILY_CASE(name, ...) case CI_OP_##name: return CI_F_SWITCH;
        CI_SWITCH_RANGE_OPS(CI_FAMILY_CASE)
#undef CI_FAMILY_CASE
#define CI_FAMILY_CASE(name, ...) case CI_OP_##name: return CI_F_BUILTIN;
        CI_BUILTIN_OPS(CI_FAMILY_CASE)
#undef CI_FAMILY_CASE
#define CI_FAMILY_CASE(name, ...) case CI_OP_##name: return CI_F_ALLOCA;
        CI_ALLOCA_OPS(CI_FAMILY_CASE)
#undef CI_FAMILY_CASE
#define CI_FAMILY_CASE(name, ...) case CI_OP_##name: return CI_F_FENCE;
        CI_FENCE_OPS(CI_FAMILY_CASE)
#undef CI_FAMILY_CASE
#define CI_FAMILY_CASE(name, ...) case CI_OP_##name: return CI_F_BIT_BUILTIN;
        CI_BIT_COUNT_OPS(CI_FAMILY_CASE)
#undef CI_FAMILY_CASE
#define CI_FAMILY_CASE(name, ...) case CI_OP_##name: return CI_F_BIT_BUILTIN;
        CI_SINGLE_BIT_OPS(CI_FAMILY_CASE)
#undef CI_FAMILY_CASE
#define CI_FAMILY_CASE(name, ...) case CI_OP_##name: return CI_F_BIT_BUILTIN;
        CI_BIT_CEIL_OPS(CI_FAMILY_CASE)
#undef CI_FAMILY_CASE
#define CI_FAMILY_CASE(name, ...) case CI_OP_##name: return CI_F_BIT_BUILTIN;
        CI_BIT_FLOOR_OPS(CI_FAMILY_CASE)
#undef CI_FAMILY_CASE
#define CI_FAMILY_CASE(name, ...) case CI_OP_##name: return CI_F_BIT_BUILTIN;
        CI_ROTATE_OPS(CI_FAMILY_CASE)
#undef CI_FAMILY_CASE
#define CI_FAMILY_CASE(name, ...) case CI_OP_##name: return CI_F_BIT_BUILTIN;
        CI_CRC_OPS(CI_FAMILY_CASE)
#undef CI_FAMILY_CASE
#define CI_FAMILY_CASE(name, ...) case CI_OP_##name: return CI_F_ATOMIC_LOAD;
        CI_ATOMIC_LOAD_OPS(CI_FAMILY_CASE)
#undef CI_FAMILY_CASE
#define CI_FAMILY_CASE(name, ...) case CI_OP_##name: return CI_F_ATOMIC_STORE;
        CI_ATOMIC_STORE_OPS(CI_FAMILY_CASE)
#undef CI_FAMILY_CASE
#define CI_FAMILY_CASE(name, ...) case CI_OP_##name: return CI_F_ATOMIC_RMW;
        CI_ATOMIC_RMW_OPS(CI_FAMILY_CASE)
#undef CI_FAMILY_CASE
#define CI_FAMILY_CASE(name, ...) case CI_OP_##name: return CI_F_ATOMIC_CAS;
        CI_ATOMIC_CAS_OPS(CI_FAMILY_CASE)
#undef CI_FAMILY_CASE
#define CI_FAMILY_CASE(name, ...) case CI_OP_##name: return CI_F_VA_ARG;
        CI_VA_SYSV_OPS(CI_FAMILY_CASE)
#undef CI_FAMILY_CASE
#define CI_FAMILY_CASE(name, ...) case CI_OP_##name: return CI_F_VA_ARG;
        CI_VA_ARM_OPS(CI_FAMILY_CASE)
#undef CI_FAMILY_CASE
#define CI_FAMILY_CASE(name, ...) case CI_OP_##name: return CI_F_VA_ARG;
        CI_VA_ARM_INDIRECT_OPS(CI_FAMILY_CASE)
#undef CI_FAMILY_CASE
#define CI_FAMILY_CASE(name, ...) case CI_OP_##name: return CI_F_VA_ARG;
        CI_VA_STACK_OPS(CI_FAMILY_CASE)
#undef CI_FAMILY_CASE
        default: return kind;
    }
}

static inline
const char* _Nonnull
ci_op_name(CiOpKind kind){
    switch(kind){
#define CI_NAME_CASE(name, ...) case CI_OP_##name: return #name;
        CI_CONST_OPS(CI_NAME_CASE)
        CI_LOAD_OPS(CI_NAME_CASE)
        CI_STORE_OPS(CI_NAME_CASE)
        CI_STORE_IMM_OPS(CI_NAME_CASE)
        CI_ZERO_OPS(CI_NAME_CASE)
        CI_COPY_OPS(CI_NAME_CASE)
        CI_MEMCOPY_OPS(CI_NAME_CASE)
        CI_EXTEND_OPS(CI_NAME_CASE)
        CI_OVERFLOW_OPS(CI_NAME_CASE)
        CI_CHECKED_OPS(CI_NAME_CASE)
        CI_TRUTH_OPS(CI_NAME_CASE)
        CI_ITOF_OPS(CI_NAME_CASE)
        CI_FTOI_OPS(CI_NAME_CASE)
        CI_FTOF_OPS(CI_NAME_CASE)
        CI_ABI_PAIR_OPS(CI_NAME_CASE)
        CI_ABI_MOVE_OPS(CI_NAME_CASE)
        CI_ABI_GP_MOVE_OPS(CI_NAME_CASE)
        CI_ABI_REG_MOVE_OPS(CI_NAME_CASE)
        CI_ABI_TRANSFER_OPS(CI_NAME_CASE)
        CI_JUMP_FALSE_OPS(CI_NAME_CASE)
        CI_JUMP_TRUE_OPS(CI_NAME_CASE)
        CI_BSWAP_OPS(CI_NAME_CASE)
        CI_BOUNDS_OPS(CI_NAME_CASE)
        CI_INDEX_OPS(CI_NAME_CASE)
        CI_BITFIELD_JUMP_OPS(CI_NAME_CASE)
        CI_LOAD_SLOT_BITFIELD_OPS(CI_NAME_CASE)
        CI_LOAD_SLOT_BITFIELD_128_OPS(CI_NAME_CASE)
        CI_LOAD_SLOT_BITFIELD_BYTES_OPS(CI_NAME_CASE)
        CI_LOAD_SLOT_BITFIELD_BYTES_128_OPS(CI_NAME_CASE)
        CI_LOAD_BITFIELD_OPS(CI_NAME_CASE)
        CI_LOAD_BITFIELD_128_OPS(CI_NAME_CASE)
        CI_LOAD_BITFIELD_BYTES_OPS(CI_NAME_CASE)
        CI_LOAD_BITFIELD_BYTES_128_OPS(CI_NAME_CASE)
        CI_STORE_BITFIELD_OPS(CI_NAME_CASE)
        CI_STORE_BITFIELD_128_OPS(CI_NAME_CASE)
        CI_STORE_BITFIELD_BYTES_OPS(CI_NAME_CASE)
        CI_STORE_BITFIELD_BYTES_128_OPS(CI_NAME_CASE)
        CI_CALL_OPS(CI_NAME_CASE)
        CI_SWITCH_DENSE_OPS(CI_NAME_CASE)
        CI_SWITCH_RANGE_OPS(CI_NAME_CASE)
        CI_BUILTIN_OPS(CI_NAME_CASE)
        CI_ALLOCA_OPS(CI_NAME_CASE)
        CI_FENCE_OPS(CI_NAME_CASE)
        CI_BIT_COUNT_OPS(CI_NAME_CASE)
        CI_SINGLE_BIT_OPS(CI_NAME_CASE)
        CI_BIT_CEIL_OPS(CI_NAME_CASE)
        CI_BIT_FLOOR_OPS(CI_NAME_CASE)
        CI_ROTATE_OPS(CI_NAME_CASE)
        CI_CRC_OPS(CI_NAME_CASE)
        CI_ATOMIC_LOAD_OPS(CI_NAME_CASE)
        CI_ATOMIC_STORE_OPS(CI_NAME_CASE)
        CI_ATOMIC_RMW_OPS(CI_NAME_CASE)
        CI_ATOMIC_CAS_OPS(CI_NAME_CASE)
        CI_VA_SYSV_OPS(CI_NAME_CASE)
        CI_VA_ARM_OPS(CI_NAME_CASE)
        CI_VA_ARM_INDIRECT_OPS(CI_NAME_CASE)
        CI_VA_STACK_OPS(CI_NAME_CASE)
#undef CI_NAME_CASE
        default: return "";
    }
}

// Indirect calls store the native entry and resolved CcFunc before arguments.
enum { CI_INDIRECT_CALL_PREFIX_SIZE = 2 * sizeof(void*) };

typedef struct CiCallDescriptor CiCallDescriptor;
typedef int CiCallInvoker(const NcCallPlan* _Nonnull, void (*_Nonnull)(void), NcCallState* _Nonnull);
struct CiCallDescriptor {
    NcCallPlan* _Nonnull abi;
    CiCallInvoker* _Nonnull invoke;
    const CcFunc* _Nonnull func;
    const CcFunction*_Nonnull call_type; // effective signature, including promoted variadic arguments
    uint32_t nargs;
};

typedef struct CiSwitchTable CiSwitchTable;
struct CiSwitchTable {
    size_t count;
    CcSwitchEntry data[];
};

enum CiAbiBank TYPED_ENUM(uint32_t){
    CI_ABI_GP,
    CI_ABI_FP,
    CI_ABI_RESULT_GP,
    CI_ABI_RESULT_FP,
    CI_ABI_RESULT_X87,
    CI_ABI_STACK_BASE,
    CI_ABI_INDIRECT_RESULT,
    CI_ABI_VECTOR_COUNT,
};
TYPEDEF_ENUM(CiAbiBank, uint32_t);
typedef union CiAbiRegister CiAbiRegister;
union CiAbiRegister {
    struct {
        CiAbiBank bank:          8;
        uint32_t index:          8,
                 capture_offset: 16;
    };
    uint32_t bits;
};

enum CiAbiAddress TYPED_ENUM(uint32_t){
    CI_ABI_SLOT,
    CI_ABI_REG,
    CI_ABI_INCOMING,
    CI_ABI_REG_PTR,
    CI_ABI_INCOMING_PTR,
    CI_ABI_REG_PTR_INDIRECT,
    CI_ABI_INCOMING_PTR_INDIRECT,
    CI_ABI_SLOT_PTR,
    CI_ABI_GP_SAVE,
    CI_ABI_FP_SAVE,
};
TYPEDEF_ENUM(CiAbiAddress, uint32_t);
enum CiAbiTransfer TYPED_ENUM(uint32_t){
    CI_ABI_COPY,
    CI_ABI_ADDRESS,
    CI_ABI_ZERO,
    CI_ABI_CONSTANT,
    CI_ABI_ZERO_EXTEND,
    CI_ABI_SIGN_EXTEND,
};
TYPEDEF_ENUM(CiAbiTransfer, uint32_t);
typedef struct CiOp CiOp;
struct CiOp {
    union {
        struct {
            CiOpKind kind: 16;
            uint32_t _pad: 16;
            uint32_t incoming_slot; // UINT32_MAX for non-variadic functions
            uint64_t _padding[2];
            SrcLoc loc;
        } frame_enter;
        struct {
            CiOpKind kind:            16; // Concrete ABI transfer
            CiAbiTransfer operation:  3;
            CiAbiAddress source:      4,
                         destination: 4;
            uint32_t source_size:     5; // extension input width; otherwise size
            union {
                uint32_t src,
                         immediate;
                CiAbiRegister src_reg;
            };
            union {
                uint32_t dst;
                CiAbiRegister dst_reg;
            };
            uint32_t size; // destination width in bytes
            union {
                uint32_t src_pointer;
                CiAbiRegister src_base;
            };
            union {
                uint32_t dst_pointer;
                CiAbiRegister dst_base;
            };
            SrcLoc loc;
        } abi_transfer;
        struct {
            CiOpKind kind: 16; // CI_OP_ABI_MOVE
            uint32_t write: 1,
                     incoming: 1,
                     size: 5,
                     source_size: 5,
                     _pad: 1;
            CiAbiTransfer operation: 3; // COPY, SIGN_EXTEND, ZERO_EXTEND
            uint32_t slot;
            CiAbiRegister reg;
            uint32_t _padding[3];
            SrcLoc loc;
        } abi_move;
        struct {
            CiOpKind kind: 16;
            uint32_t write: 1, _padding: 15;
            uint32_t slot0, slot1;
            CiAbiRegister reg0, reg1;
            uint32_t _pad;
            SrcLoc loc;
        } abi_pair;
        struct {
            CiOpKind kind: 16;
            uint32_t _bitpad: 16;
            uint32_t _pad;
            uint64_t pad[2];
            SrcLoc loc;
        };
        struct {
            CiOpKind kind: 16; // CI_OP_CAPTURE_BACKTRACE
            uint32_t _pad: 16;
            uint32_t slot, slot_size, buffer, capacity;
            uint32_t _padding;
            SrcLoc loc;
        } capture_backtrace;
        struct {
            CiOpKind kind: 16; // CI_OP_CONST
            uint32_t bt_kind: 7; // for pretty printing
            uint32_t is_anon_array: 1; // for pretty printing
            uint32_t immsize: 8;
            uint32_t slot;
            uint64_t immediate[2];
            SrcLoc loc;
        } constant;
        struct {
            CiOpKind kind: 16; // CI_OP_COPY
            uint32_t is_volatile: 1,
                     _bitpad: 15;
            uint32_t _pad;
            uint32_t slot, slot_size,
                     src, src_size;
            SrcLoc loc;
        } copy;
        struct {
            CiOpKind kind: 16; // Immediate integer arithmetic
            uint32_t _bitpad: 16;
            uint32_t slot, src, _pad;
            uint64_t immediate;
            SrcLoc loc;
        } alu_imm;
        struct {
            CiOpKind kind: 16; // CI_OP_INDEX
            uint32_t ptr_size: 4, index_size: 4, index_unsigned: 1, _bitpad: 7;
            uint32_t slot, base, index, scale, _pad;
            SrcLoc loc;
        } index;
        struct {
            CiOpKind kind: 16; // Integer arithmetic
            uint32_t _bitpad: 16;
            uint32_t slot,
                     src,
                     src2;
            uint32_t pad[2];
            SrcLoc loc;
        } alu;
        struct {
            CiOpKind kind: 16; // Integer comparisons
            uint32_t _bitpad: 16;
            uint32_t slot,
                     slot_size,
                     src,
                     src2;
            uint32_t pad;
            SrcLoc loc;
        } cmp;
        struct {
            CiOpKind kind: 16; // Fused integer comparison and branch
            uint32_t when_true: 1, _bitpad: 15;
            int32_t jump; // instruction displacement from this operation
            uint32_t _pad, src, src2, patch_next;
            SrcLoc loc;
        } cmp_jump;
        struct {
            CiOpKind kind: 16;
            uint32_t bit_offset: 8, bit_width: 8;
            int32_t jump;
            uint32_t src, offset;
            uint32_t when_true: 1, from_slot: 1, is_volatile: 1, _padding: 29;
            uint32_t patch_next;
            SrcLoc loc;
        } bitfield_jump;
        struct {
            CiOpKind kind: 16; // Floating-point arithmetic
            uint32_t is_brian_float: 1, _bitpad: 15;
            uint32_t slot,
                     slot_size,
                     src,
                     src2;
            uint32_t pad;
            SrcLoc loc;
        } falu32, falu64;
        struct {
            CiOpKind kind: 16; // Floating-point comparisons
            uint32_t is_brian_float: 1, _bitpad: 15;
            uint32_t slot,
                     slot_size,
                     src,
                     src2;
            uint32_t pad;
            SrcLoc loc;
        } fcmp32, fcmp64;
        struct {
            CiOpKind kind: 16; // CI_OP_CHECKED
            CiCheckedOp op: 2;
            uint32_t src_size: 4,
                     src2_size: 4,
                     res_size: 4,
                     src_unsigned: 1,
                     src2_unsigned: 1;
            uint32_t result,
                     overflow,
                     src,
                     src2;
            uint32_t res_unsigned: 1, _padding: 31;
            SrcLoc loc;
        } checked;
        struct {
            CiOpKind kind: 16; // CI_OP_CHECK: fail if the one-byte boolean is false.
            uint32_t _bitpad: 16;
            uint32_t src;
            const char* _Nonnull message;
            uint64_t pad;
            SrcLoc loc;
        } check;
        struct {
            CiOpKind kind: 16; // CI_OP_PROPAGATE_ERROR: return a nonzero int error.
            uint32_t _bitpad: 16;
            uint32_t src;
            uint64_t pad[2];
            SrcLoc loc;
        } propagate_error;
        struct {
            CiOpKind kind: 16;
            CcBitBuiltinOp op: 6;
            uint32_t src_size: 5, src2_size: 5;
            uint32_t slot, slot_size, src, src2;
            uint32_t pad;
            SrcLoc loc;
        } bit_builtin;
        struct {
            CiOpKind kind: 16; // CI_OP_CONVERT, CI_OP_ITOF, CI_OP_FTOI, CI_OP_FTOF
            uint32_t is_unsigned: 1,
                     checked_index: 1,
                     checked_bound: 1,
                     src_is_brian_float: 1,
                     dst_is_brian_float: 1,
                     _bitpad: 11;
            uint32_t src_float: 8, dst_float: 8, _padding: 16;
            uint32_t slot, slot_size,
                     src, src_size;
            SrcLoc loc;
        } convert, itof, ftoi, ftof;
        struct {
            CiOpKind kind: 16; // CI_OP_SLOT_ADDR
            uint32_t _bitpad: 16;
            uint32_t slot, slot_size, src;
            uint64_t pad[1];
            SrcLoc loc;
        } slot_addr;
        struct {
            CiOpKind kind: 16; // CI_OP_VAR_ADDR / CI_OP_TLS_ADDR
            uint32_t _bitpad: 16;
            uint32_t pad;
            uint32_t slot, slot_size;
            CcVariable*_Nonnull var;
            SrcLoc loc;
        } var_addr;
        struct {
            CiOpKind kind: 16; // CI_OP_FUNC_ADDR
            uint32_t _bitpad: 16;
            uint32_t pad;
            uint32_t slot, slot_size;
            CcFunc*_Nonnull func;
            SrcLoc loc;
        } func_addr;
        struct {
            CiOpKind kind: 16; // CI_OP_BOUNDS
            uint32_t inclusive: 1,
                     index_signed: 1,
                     _bitpad: 14;
            uint32_t pad;
            uint32_t src, src_size,
                     src2, src2_size;
            SrcLoc loc;
        } bounds;
        struct {
            CiOpKind kind: 16; // CI_OP_STORE
            // Guaranteed alignment of the effective address: 1u << align_log2.
            uint32_t align_log2: 5,
                     is_volatile: 1,
                     _bitpad: 10;
            uint32_t pad;
            uint32_t slot,
                     src,
                     src_size,
                     offset;
            SrcLoc loc;
        } store;
        struct {
            CiOpKind kind: 16; // CI_OP_STORE_IMM
            uint32_t size: 8,
                     align_log2: 5,
                     is_volatile: 1,
                     _bitpad: 2;
            uint32_t slot, offset, _pad;
            uint64_t immediate;
            SrcLoc loc;
        } store_imm;
        struct {
            CiOpKind kind: 16; // CI_OP_LOAD
            uint32_t align_log2: 5,
                     is_volatile: 1,
                     _bitpad: 10;
            uint32_t pad;
            uint32_t slot,
                     slot_size,
                     src,
                     offset;
            SrcLoc loc;
        } load;
        struct {
            CiOpKind kind: 16; // CI_OP_MEMCOPY
            uint32_t dst_align_log2: 5,
                     src_align_log2: 5,
                     is_volatile: 1,
                     dynamic_size: 1,
                     _bitpad: 4;
            uint32_t size; // byte count, or size_t slot when dynamic_size
            uint32_t slot,
                     offset,
                     src,
                     src_offset;
            SrcLoc loc;
        } memcopy;
        struct {
            CiOpKind kind: 16; // CI_OP_ZERO
            uint32_t align_log2: 5,
                     is_volatile: 1,
                     _bitpad: 10;
            uint32_t size;
            uint32_t slot, offset;
            uint64_t pad;
            SrcLoc loc;
        } zero;
        struct {
            CiOpKind kind: 16; // CI_OP_STORE_BITFIELD[_BYTES]
            uint32_t bit_offset: 8,
                     bit_width: 8;
            uint32_t is_signed: 1, align_log2: 5, is_volatile: 1, _padding: 25;
            uint32_t slot,
                     src,
                     src_size,
                     offset;
            SrcLoc loc;
        } store_bf;
        struct {
            CiOpKind kind: 16; // CI_OP_LOAD_BITFIELD[_BYTES]
            uint32_t bit_offset: 8,
                     bit_width: 8;
            uint32_t is_signed: 1, align_log2: 5, is_volatile: 1, _padding: 25;
            uint32_t slot, slot_size,
                     src,
                     offset;
            SrcLoc loc;
        } load_bf;
        struct {
            CiOpKind kind: 16; // CI_OP_ATOMIC_LOAD
            CcMemoryOrder memorder: 4;
            uint32_t is_volatile: 1,
                     _bitpad: 11;
            uint32_t pad;
            uint32_t slot, slot_size,
                     src,
                     offset;
            SrcLoc loc;
        } atomic_load;
        struct {
            CiOpKind kind: 16; // CI_OP_ATOMIC_STORE
            CcMemoryOrder memorder: 4;
            uint32_t is_volatile: 1,
                     _bitpad: 11;
            uint32_t pad;
            uint32_t slot,
                     src,
                     src_size,
                     offset;
            SrcLoc loc;
        } atomic_store;
        struct {
            CiOpKind kind: 16; // CI_OP_ATOMIC_RMW
            CiAtomicRmwOp op: 8;
            CcMemoryOrder memorder: 4;
            uint32_t discard: 1,
                     is_volatile: 1,
                     _bitpad: 2;
            uint32_t offset;
            uint32_t slot, slot_size,
                     src,
                     src2;
            SrcLoc loc;
        } atomic_rmw;
        struct {
            CiOpKind kind: 16; // CI_OP_ATOMIC_CAS
            CcMemoryOrder memorder: 4;
            CcMemoryOrder fail_memorder: 4;
            uint32_t weak: 1,
                     size: 5,
                     is_volatile: 1,
                     _bitpad: 1;
            uint32_t offset;
            uint32_t slot,
                     src,
                     expected,
                     desired;
            SrcLoc loc;
        } atomic_cas;
        struct {
            CiOpKind kind: 16; // CI_OP_FENCE
            CcMemoryOrder memorder: 4;
            uint32_t is_signal: 1,
                     _bitpad: 11;
            uint32_t _pad;
            uint64_t pad[2];
            SrcLoc loc;
        } fence;
        struct {
            CiOpKind kind: 16; // CI_OP_CLEAR_CACHE
            uint32_t _bitpad: 16;
            uint32_t _pad;
            uint32_t begin, end;
            uint64_t pad;
            SrcLoc loc;
        } clear_cache;
        struct {
            CiOpKind kind: 16; // CI_OP_CALL
            uint32_t is_indirect: 1, _pad:15;
            uint32_t ret_slot,
                     ret_size,
                     args_slot;
            CiCallDescriptor *_Nonnull descrip;
            SrcLoc loc;
        } call;
        struct {
            CiOpKind kind: 16;
            uint32_t _pad: 16;
            uint32_t slot;
            uint64_t _padding[2];
            SrcLoc loc;
        } call_resolve;
        struct {
            CiOpKind kind: 16; // CI_OP_ISTRUE
            uint32_t float_width: 8,
                     negate: 1,
                     _bitpad: 7;
            uint32_t pad;
            uint32_t slot, slot_size,
                     src, src_size;
            SrcLoc loc;
        } istrue;
        struct {
            CiOpKind kind: 16; // CI_OP_JUMP
            uint32_t _bitpad: 16;
            int32_t jump; // instruction displacement from this operation
            uint32_t pad[3], patch_next; // lowering-only unresolved branch link
            SrcLoc loc;
        } jump;
        struct {
            CiOpKind kind: 16; // CI_OP_JUMP_FALSE, CI_OP_JUMP_TRUE
            uint32_t _bitpad: 16;
            int32_t jump;
            uint32_t slot, slot_size;
            uint32_t pad, patch_next;
            SrcLoc loc;
        } jump_false, jump_true;
        struct {
            CiOpKind kind: 16; // CI_OP_RETURN_SLOT
            uint32_t _bitpad: 16;
            uint32_t _pad;
            uint32_t src, src_size;
            uint64_t pad[1];
            SrcLoc loc;
        } return_slot;
        struct {
            CiOpKind kind: 16; // CI_OP_SWITCH
            uint32_t is_unsigned: 1,
                     is_dense: 1,
                     _bitpad: 14;
            int32_t jump;
            uint32_t slot, slot_size;
            CiSwitchTable*_Null_unspecified table;
            SrcLoc loc;
        } switch_;
        struct {
            CiOpKind kind: 16; // CI_OP_ALLOCA
            uint32_t zero_initialize: 1,
                     _bitpad: 15;
            uint32_t alignment;
            uint32_t slot, src;
            uint64_t pad;
            SrcLoc loc;
        } alloca;
        struct {
            CiOpKind kind: 16; // CI_OP_BSWAP
            uint32_t _bitpad: 16;
            uint32_t size;
            uint32_t slot, src;
            uint64_t pad;
            SrcLoc loc;
        } bswap;
        struct {
            CiOpKind kind: 16; // CI_OP_BUILTIN
            CcBuiltinOp op: 8;
            uint32_t _bitpad: 8;
            uint32_t slot; // readcyclecounter result or UINT32_MAX
            uint64_t pad[2];
            SrcLoc loc;
        } builtin;
        struct {
            CiOpKind kind: 16; // CI_OP_VA_ARG
            uint32_t count: 8, gp: 4, fp: 4;
            uint32_t pointers; // Output slots for GP/stack and FP source pointers.
            uint32_t size, src, stack_mask, stack_stride;
            SrcLoc loc;
        } va_arg_;
    };
};
_Static_assert(sizeof(CiOp) == 32, "");
_Static_assert(offsetof(CiOp, abi_transfer.loc) == offsetof(CiOp, loc), "ABI transfer source location layout");
_Static_assert(offsetof(CiOp, abi_move.loc) == offsetof(CiOp, loc), "ABI move source location layout");
_Static_assert(offsetof(CiOp, cmp.src) == offsetof(CiOp, cmp_jump.src), "comparison operand layout");
_Static_assert(offsetof(CiOp, cmp.src2) == offsetof(CiOp, cmp_jump.src2), "comparison operand layout");
_Static_assert(offsetof(CiOp, jump_false.jump) == offsetof(CiOp, cmp_jump.jump), "branch target layout");
_Static_assert(offsetof(CiOp, jump.jump) == offsetof(CiOp, bitfield_jump.jump)
    && offsetof(CiOp, jump.patch_next) == offsetof(CiOp, bitfield_jump.patch_next), "bitfield branch layout");
_Static_assert(offsetof(CiOp, jump.patch_next) == offsetof(CiOp, cmp_jump.patch_next)
    && offsetof(CiOp, jump.patch_next) == offsetof(CiOp, jump_false.patch_next), "branch patch link layout");

#ifndef MARRAY_CIOP
#define MARRAY_CIOP
#define MARRAY_T CiOp
#include "../Drp/Marray.h"
#endif

static inline
_Bool
ci_op_has_target(CiOpKind kind){
    switch(kind){
        case CI_OP_JUMP:
#define CI_TARGET_CASE(name, ...) case CI_OP_##name:
        CI_BITFIELD_JUMP_OPS(CI_TARGET_CASE)
        CI_JUMP_FALSE_OPS(CI_TARGET_CASE)
        CI_JUMP_TRUE_OPS(CI_TARGET_CASE)
        CI_SWITCH_DENSE_OPS(CI_TARGET_CASE)
        CI_SWITCH_RANGE_OPS(CI_TARGET_CASE)
#undef CI_TARGET_CASE
#define CI_TARGET_CASE(name, suffix) case CI_OP_##name##suffix:
        CI_CMP_OPS(CI_TARGET_CASE, _JUMP32)
        CI_CMP_OPS(CI_TARGET_CASE, _JUMP64)
#undef CI_TARGET_CASE
            return 1;
        default: return 0;
    }
}

// Shared executable layout for functions, expressions, and top-level code.
typedef struct CiCode CiCode;
struct CiCode {
    Atom _Null_unspecified name;
    size_t frame_size, alignment, op_count;
    CiOp ops[];
};

static inline
size_t
ci_code_count(const ByteBuilder* _Nonnull builder){
    return builder->cursor ? (builder->cursor - offsetof(CiCode, ops)) / sizeof(CiOp) : 0;
}

static inline
CiOp* _Null_unspecified
ci_code_ops(const ByteBuilder* _Nonnull builder){
    return builder->data ? ((CiCode*)builder->data)->ops : NULL;
}

static inline
void
ci_code_truncate(ByteBuilder* _Nonnull builder, size_t count){
    if(builder->cursor) builder->cursor = offsetof(CiCode, ops) + count * sizeof(CiOp);
    builder->errored = 0;
}

static inline
int
ci_code_alloc(ByteBuilder* _Nonnull builder, Allocator al, CiOp* _Null_unspecified* _Nonnull out){
    builder->allocator = al;
    builder->alignment = _Alignof(CiCode);
    size_t prefix = builder->cursor ? 0 : offsetof(CiCode, ops);
    if(bb_ensure_additional(builder, prefix + sizeof(CiOp))) return 1;
    if(prefix){
        memset(builder->data, 0, prefix);
        builder->cursor = prefix;
    }
    *out = (CiOp*)((char*)builder->data + builder->cursor);
    builder->cursor += sizeof(CiOp);
    return 0;
}

static inline
int
ci_code_push(ByteBuilder* _Nonnull builder, Allocator al, CiOp op){
    CiOp* dest;
    int err = ci_code_alloc(builder, al, &dest);
    if(err) return err;
    *dest = op;
    return 0;
}

// Function-specific ABI metadata, hung off CcFunc.interp_ops.
typedef struct CiFuncOps CiFuncOps;
struct CiFuncOps {
    NcCallPlan* _Nonnull abi;
    CiCode* _Nonnull code;
    const CcFunction* _Nonnull native_type; // embedding API signature, excluding ignored arguments
};

#endif
