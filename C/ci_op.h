#ifndef C_CI_OP_H
#define C_CI_OP_H
//
// Copyright © 2026-2026, David Priver <david@davidpriver.com>
//
#include <stdint.h>
#include "srcloc.h"
#include "cc_stmt.h"
#include "cc_expr.h"
#include "../Drp/typed_enum.h"
#include "../Drp/atom.h"

#if !defined __clang__ && !defined _Null_unspecified
#define _Null_unspecified
#endif
#if !defined __clang__ && !defined _Nonnull
#define _Nonnull
#endif

// Integer operations for CI_OP_ALU; mirrors the tree evaluator's semantics:
// both operands widen to 64 bits with the lhs type's signedness, the
// operation runs in 64 bits, the result truncates to the destination size.
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
    CI_ALU_EQ,
    CI_ALU_NE,
    CI_ALU_LT,
    CI_ALU_GT,
    CI_ALU_LE,
    CI_ALU_GE,
    // unary; src2 is ignored (set equal to src)
    CI_ALU_NEG,
    CI_ALU_NOT,
};
TYPEDEF_ENUM(CiAluOp, uint32_t);

// Float operations for CI_OP_FALU32/CI_OP_FALU64; arithmetic runs at the
// op's native width (FLT_EVAL_METHOD 0), comparisons write a canonical
// integer of the destination size.
enum CiFaluOp TYPED_ENUM(uint32_t){
    CI_FALU_ADD,
    CI_FALU_SUB,
    CI_FALU_MUL,
    CI_FALU_DIV,
    CI_FALU_EQ,
    CI_FALU_NE,
    CI_FALU_LT,
    CI_FALU_GT,
    CI_FALU_LE,
    CI_FALU_GE,
    // unary; src2 is ignored (set equal to src)
    CI_FALU_NEG,
};
TYPEDEF_ENUM(CiFaluOp, uint32_t);

enum CiOpKind TYPED_ENUM(uint32_t){
    CI_OP_EVAL,             // evaluate expr, discard result
    CI_OP_EVAL_INTO,        // evaluate expr into slots[slot..slot+slot_size)
    CI_OP_CONST,            // slots[slot..slot+slot_size) = low slot_size bytes of imm
    CI_OP_COPY,             // slots[slot..slot+slot_size) = slots[src..src+slot_size)
    CI_OP_ALU,              // slots[slot] = slots[src] op slots[src2] as integers;
                            // extra = CiAluOp | (is_unsigned << 16)
    CI_OP_FALU32,           // slots[slot] = slots[src] op slots[src2] as floats;
    CI_OP_FALU64,           // ... as doubles; extra = CiFaluOp
    CI_OP_CONVERT,          // slots[slot..slot+slot_size) = slots[src..src+src_size)
                            // widened to 64 bits then truncated; extra = source is unsigned
    CI_OP_ITOF,             // integer slots[src] to float/double slots[slot] (by slot_size);
                            // extra = source is unsigned
    CI_OP_FTOI,             // float/double slots[src] (by src_size) to integer slots[slot];
                            // extra = destination is unsigned
    CI_OP_FTOF,             // float/double slots[src] to float/double slots[slot] (by sizes)
    CI_OP_SLOT_ADDR,        // slots[slot] = &slots[src]
    CI_OP_VAR_ADDR,         // slots[slot] = the CcVariable* imm's resolved storage
                            // address; a GOT load, read at execution because
                            // lowering can run before the variable resolves
    CI_OP_BOUNDS,           // trap unless the 8-byte unsigned index in slots[src] is in
                            // range of the 8-byte length in slots[src2]; bounds.inclusive
                            // permits index == length (address-of one-past-the-end)
    CI_OP_LOAD,             // slots[slot..slot+slot_size) = ptr[extra..], ptr read from slots[src]
    CI_OP_STORE,            // ptr[extra..] = slots[src..src+src_size), ptr read from slots[slot]
    CI_OP_LOAD_BITFIELD,    // like CI_OP_LOAD, but the slot_size bytes at ptr[extra..] are a
                            // bitfield storage unit: extract bf.bit_width bits at
                            // bf.bit_offset, extend per bf.is_signed
    CI_OP_STORE_BITFIELD,   // like CI_OP_STORE, but a read-modify-write: insert the low
                            // bf.bit_width bits of slots[src..src+src_size) at bf.bit_offset
                            // of the src_size-byte storage unit at ptr[extra..]
    CI_OP_CALL,             // call the CcFunc* in imm; slots[src..src+src_size) holds the
                            // staged arguments, laid out like the callee's parameter area;
                            // the return value lands in slots[slot..slot+slot_size)
                            // (slot_size 0 discards it)
    CI_OP_ISTRUE,           // slots[slot] = slot_size-byte 0/1 of truthy(slots[src..src+src_size));
                            // extra = CcBasicTypeKind when the source is a float, else 0,
                            // | (negate << 16) to compute !truthy instead
    CI_OP_JUMP,             // pc = jump
    CI_OP_JUMP_FALSE,       // if !slots[slot] pc = jump; slot holds a canonical 0/1
    CI_OP_JUMP_TRUE,        // if slots[slot] pc = jump; slot holds a canonical 0/1
    CI_OP_RETURN,           // return with no value; pc = end
    CI_OP_RETURN_SLOT,      // copy slots[src..src+src_size) into return_buf; pc = end
    CI_OP_SWITCH,           // multi-way conditional jump on slots[slot]; binary
                            // search sw.table, no match: pc = jump (default/exit);
                            // extra = 1 if the value is unsigned
};
TYPEDEF_ENUM(CiOpKind, uint32_t);

typedef struct CiOp CiOp;
struct CiOp {
    CiOpKind kind;
    uint32_t jump;
    uint32_t slot, slot_size; // destination (or tested) slot
    uint32_t src, src_size;   // source slot
    uint32_t src2, src2_size; // second source slot
    union {
        uint32_t extra_;           // op-specific immediate
        struct {
            CiAluOp op: 16;
            uint32_t is_unsigned:1,
                     _padding: 15;
        } alu;
        struct {
            CiFaluOp op: 32;
        } falu;
        struct {
            uint32_t is_unsigned: 32;
        } conv;
        struct {
            uint32_t offset;
        } load;
        struct {
            uint32_t offset;
        } store;
        struct {
            uint32_t float_kind: 16,
                     negate: 1,
                     _padding: 15;
        } is_true;
        struct {
            uint32_t inclusive: 1,     // permit index == length
                     index_signed: 1,  // format a failing index as signed
                     _padding: 30;
        } bounds;
    };
    // CI_OP_LOAD_BITFIELD/CI_OP_STORE_BITFIELD; outside the union so the
    // byte offset in load/store stays usable alongside it
    struct {
        uint32_t bit_offset: 8, bit_width: 8, is_signed: 1, _padding: 15;
    } bf;
    SrcLoc loc;
    union {
        CcExpr* _Null_unspecified expr;
        struct {
            CcSwitchEntry* _Null_unspecified table;
            size_t count;
        } sw;               // CI_OP_SWITCH
        uint64_t immediate; // CI_OP_CONST value, CI_OP_CALL CcFunc*, CI_OP_VAR_ADDR CcVariable*
        CcFunc*_Nonnull func;
        CcVariable*_Nonnull var;
    };
};
_Static_assert(sizeof(CiOp) == 64, ""); // FIXME: optimize this

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
