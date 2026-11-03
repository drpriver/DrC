#ifndef C_CI_OP_LIST_H
#define C_CI_OP_LIST_H

#define CI_ALU_BINARY_OPS(X, suffix) \
    X(ADD, suffix) \
    X(SUB, suffix) \
    X(MUL, suffix) \
    X(SDIV, suffix) \
    X(UDIV, suffix) \
    X(SMOD, suffix) \
    X(UMOD, suffix) \
    X(AND, suffix) \
    X(OR, suffix) \
    X(XOR, suffix) \
    X(SHL, suffix) \
    X(ASR, suffix) \
    X(LSR, suffix)

#define CI_ALU_UNARY_OPS(X, suffix) \
    X(NEG, suffix) \
    X(NOT, suffix)

#define CI_CMP_OPS(X, suffix) \
    X(EQ, suffix) \
    X(NE, suffix) \
    X(SLT, suffix) \
    X(ULT, suffix) \
    X(SGT, suffix) \
    X(UGT, suffix) \
    X(SLE, suffix) \
    X(ULE, suffix) \
    X(SGE, suffix) \
    X(UGE, suffix)

#define CI_FALU_OPS(X, suffix) \
    X(FADD, suffix) \
    X(FSUB, suffix) \
    X(FMUL, suffix) \
    X(FDIV, suffix) \
    X(FNEG, suffix)

#define CI_FCMP_OPS(X, suffix) \
    X(FEQ, suffix) \
    X(FNE, suffix) \
    X(FLT, suffix) \
    X(FGT, suffix) \
    X(FLE, suffix) \
    X(FGE, suffix)

#define CI_OPCODE_ENUM(name, suffix) CI_OP_##name##suffix,
#define CI_OPCODE_CASE(name, suffix) case CI_OP_##name##suffix:

#define CI_OPCODE_NATIVE_LIST(X) \
    X(UMUL32_64, ) \
    X(UMUL64_128, ) \
    X(SHL_IMM128, ) \
    X(ASR_IMM128, ) \
    X(LSR_IMM128, ) \
    CI_ALU_BINARY_OPS(X, _IMM32) \
    CI_ALU_BINARY_OPS(X, _IMM64) \
    CI_ALU_BINARY_OPS(X, 8) \
    CI_ALU_UNARY_OPS(X, 8) \
    CI_ALU_BINARY_OPS(X, 16) \
    CI_ALU_UNARY_OPS(X, 16) \
    CI_ALU_BINARY_OPS(X, 32) \
    CI_ALU_UNARY_OPS(X, 32) \
    CI_ALU_BINARY_OPS(X, 64) \
    CI_ALU_UNARY_OPS(X, 64) \
    CI_ALU_BINARY_OPS(X, 128) \
    CI_ALU_UNARY_OPS(X, 128) \
    CI_CMP_OPS(X, 32) \
    CI_CMP_OPS(X, 64) \
    CI_CMP_OPS(X, 128) \
    CI_CMP_OPS(X, _JUMP32) \
    CI_CMP_OPS(X, _JUMP64) \
    CI_FALU_OPS(X, _BF16) \
    CI_FALU_OPS(X, 16) \
    CI_FALU_OPS(X, 32) \
    CI_FALU_OPS(X, 64) \
    CI_FALU_OPS(X, 80) \
    CI_FALU_OPS(X, 128) \
    CI_FCMP_OPS(X, _BF16) \
    CI_FCMP_OPS(X, 16) \
    CI_FCMP_OPS(X, 32) \
    CI_FCMP_OPS(X, 64) \
    CI_FCMP_OPS(X, 80) \
    CI_FCMP_OPS(X, 128) \
    X(CHECK, ) \
    X(PROPAGATE_ERROR, ) \
    X(SLOT_ADDR, ) \
    X(VAR_ADDR, ) \
    X(TLS_ADDR, ) \
    X(FUNC_ADDR, ) \
    X(JUMP, ) \
    X(RETURN, ) \
    X(RETURN_SLOT, ) \
    X(CLEAR_CACHE, ) \
    X(STACK_SAVE, ) \
    X(STACK_RESTORE, ) \
    X(CAPTURE_BACKTRACE, ) \
    X(FRAME_ENTER, ) \
    X(FRAME_READY, ) \
    X(FRAME_SAVE, ) \
    X(FRAME_RESTORE, ) \
    X(CALL_RESOLVE, )

#define CI_OPCODE_FLAT_LIST(X) \
    CI_CONST_OPS(X) \
    CI_LOAD_OPS(X) \
    CI_STORE_OPS(X) \
    CI_STORE_IMM_OPS(X) \
    CI_ZERO_OPS(X) \
    CI_COPY_OPS(X) \
    CI_MEMCOPY_OPS(X) \
    CI_EXTEND_OPS(X) \
    CI_OVERFLOW_OPS(X) \
    CI_CHECKED_OPS(X) \
    CI_TRUTH_OPS(X) \
    CI_ITOF_OPS(X) \
    CI_FTOI_OPS(X) \
    CI_FTOF_OPS(X) \
    CI_ABI_MOVE_OPS(X) \
    CI_ABI_TRANSFER_OPS(X) \
    CI_JUMP_FALSE_OPS(X) \
    CI_JUMP_TRUE_OPS(X) \
    CI_BSWAP_OPS(X) \
    CI_BOUNDS_OPS(X) \
    CI_INDEX_OPS(X) \
    CI_LOAD_BITFIELD_OPS(X) \
    CI_LOAD_BITFIELD_128_OPS(X) \
    CI_LOAD_BITFIELD_BYTES_OPS(X) \
    CI_LOAD_BITFIELD_BYTES_128_OPS(X) \
    CI_STORE_BITFIELD_OPS(X) \
    CI_STORE_BITFIELD_128_OPS(X) \
    CI_STORE_BITFIELD_BYTES_OPS(X) \
    CI_STORE_BITFIELD_BYTES_128_OPS(X) \
    CI_CALL_OPS(X) \
    CI_SWITCH_DENSE_OPS(X) \
    CI_SWITCH_RANGE_OPS(X) \
    CI_BUILTIN_OPS(X) \
    CI_ALLOCA_OPS(X) \
    CI_FENCE_OPS(X) \
    CI_BIT_COUNT_OPS(X) \
    CI_SINGLE_BIT_OPS(X) \
    CI_BIT_CEIL_OPS(X) \
    CI_BIT_FLOOR_OPS(X) \
    CI_ROTATE_OPS(X) \
    CI_CRC_OPS(X) \
    CI_ATOMIC_LOAD_OPS(X) \
    CI_ATOMIC_STORE_OPS(X) \
    CI_ATOMIC_RMW_OPS(X) \
    CI_ATOMIC_CAS_OPS(X) \
    CI_VA_SYSV_OPS(X) \
    CI_VA_ARM_OPS(X) \
    CI_VA_ARM_INDIRECT_OPS(X) \
    CI_VA_STACK_OPS(X) \
    CI_ABI_GP_MOVE_OPS(X) \
    CI_ABI_REG_MOVE_OPS(X) \
    CI_LOAD_SLOT_BITFIELD_OPS(X) \
    CI_LOAD_SLOT_BITFIELD_128_OPS(X) \
    CI_LOAD_SLOT_BITFIELD_BYTES_OPS(X) \
    CI_LOAD_SLOT_BITFIELD_BYTES_128_OPS(X) \
    CI_BITFIELD_JUMP_OPS(X) \
    CI_ABI_PAIR_OPS(X)

// Memory families: name, byte width, destination alignment, source alignment.
// Width 0 comes from the instruction; MEMCOPY width -1 comes from a size slot.
#define CI_CONST_OPS(X) \
    X(CONST8, 1, 1, 1) \
    X(CONST16, 2, 1, 1) \
    X(CONST32, 4, 1, 1) \
    X(CONST64, 8, 1, 1) \
    X(CONST128, 16, 1, 1) \
    X(CONST_N, 0, 1, 1)

#define CI_LOAD_OPS(X) \
    X(LOAD8, 1, 1, 1) \
    X(LOAD16_A, 2, 2, 2) \
    X(LOAD16_U, 2, 1, 1) \
    X(LOAD32_A, 4, 4, 4) \
    X(LOAD32_U, 4, 1, 1) \
    X(LOAD64_A, 8, 8, 8) \
    X(LOAD64_U, 8, 1, 1) \
    X(LOAD128_A, 16, 16, 16) \
    X(LOAD128_U, 16, 1, 1) \
    X(LOAD_N, 0, 1, 1) \
    X(LOAD_N_A2, 0, 2, 2) \
    X(LOAD_N_A4, 0, 4, 4) \
    X(LOAD_N_A8, 0, 8, 8) \
    X(LOAD_N_A16, 0, 16, 16)

#define CI_STORE_OPS(X) \
    X(STORE8, 1, 1, 1) \
    X(STORE16_A, 2, 2, 2) \
    X(STORE16_U, 2, 1, 1) \
    X(STORE32_A, 4, 4, 4) \
    X(STORE32_U, 4, 1, 1) \
    X(STORE64_A, 8, 8, 8) \
    X(STORE64_U, 8, 1, 1) \
    X(STORE128_A, 16, 16, 16) \
    X(STORE128_U, 16, 1, 1) \
    X(STORE_N, 0, 1, 1) \
    X(STORE_N_A2, 0, 2, 2) \
    X(STORE_N_A4, 0, 4, 4) \
    X(STORE_N_A8, 0, 8, 8) \
    X(STORE_N_A16, 0, 16, 16)

#define CI_STORE_IMM_OPS(X) \
    X(STORE_IMM8, 1, 1, 1) \
    X(STORE_IMM16_A, 2, 2, 1) \
    X(STORE_IMM16_U, 2, 1, 1) \
    X(STORE_IMM32_A, 4, 4, 1) \
    X(STORE_IMM32_U, 4, 1, 1) \
    X(STORE_IMM64_A, 8, 8, 1) \
    X(STORE_IMM64_U, 8, 1, 1) \
    X(STORE_IMM_N, 0, 1, 1) \
    X(STORE_IMM_N_A2, 0, 2, 1) \
    X(STORE_IMM_N_A4, 0, 4, 1) \
    X(STORE_IMM_N_A8, 0, 8, 1) \
    X(STORE_IMM_N_A16, 0, 16, 1)

#define CI_ZERO_OPS(X) \
    X(ZERO8, 1, 1, 1) \
    X(ZERO16_A, 2, 2, 1) \
    X(ZERO16_U, 2, 1, 1) \
    X(ZERO32_A, 4, 4, 1) \
    X(ZERO32_U, 4, 1, 1) \
    X(ZERO64_A, 8, 8, 1) \
    X(ZERO64_U, 8, 1, 1) \
    X(ZERO128_A, 16, 16, 1) \
    X(ZERO128_U, 16, 1, 1) \
    X(ZERO_N, 0, 1, 1) \
    X(ZERO_N_A2, 0, 2, 1) \
    X(ZERO_N_A4, 0, 4, 1) \
    X(ZERO_N_A8, 0, 8, 1) \
    X(ZERO_N_A16, 0, 16, 1)

#define CI_COPY_OPS(X) \
    X(COPY8, 1, 1, 1) \
    X(COPY16_A, 2, 2, 2) \
    X(COPY16_U, 2, 1, 1) \
    X(COPY32_A, 4, 4, 4) \
    X(COPY32_U, 4, 1, 1) \
    X(COPY64_A, 8, 8, 8) \
    X(COPY64_U, 8, 1, 1) \
    X(COPY128_A, 16, 16, 16) \
    X(COPY128_U, 16, 1, 1) \
    X(COPY_N, 0, 1, 1) \
    X(COPY_N_A2, 0, 2, 2) \
    X(COPY_N_A4, 0, 4, 4) \
    X(COPY_N_A8, 0, 8, 8) \
    X(COPY_N_A16, 0, 16, 16)

#define CI_MEMCOPY_OPS(X) \
    X(MEMCOPY8, 1, 1, 1) \
    X(MEMCOPY16_A, 2, 2, 2) \
    X(MEMCOPY16_U, 2, 1, 1) \
    X(MEMCOPY32_A, 4, 4, 4) \
    X(MEMCOPY32_U, 4, 1, 1) \
    X(MEMCOPY64_A, 8, 8, 8) \
    X(MEMCOPY64_U, 8, 1, 1) \
    X(MEMCOPY128_A, 16, 16, 16) \
    X(MEMCOPY128_U, 16, 1, 1) \
    X(MEMCOPY_N, 0, 1, 1) \
    X(MEMCOPY_N_A2, 0, 2, 2) \
    X(MEMCOPY_N_A4, 0, 4, 4) \
    X(MEMCOPY_N_A8, 0, 8, 8) \
    X(MEMCOPY_N_A16, 0, 16, 16) \
    X(MEMCOPY_VAR, -1, 1, 1) \
    X(MEMCOPY_VAR_A2, -1, 2, 2) \
    X(MEMCOPY_VAR_A4, -1, 4, 4) \
    X(MEMCOPY_VAR_A8, -1, 8, 8) \
    X(MEMCOPY_VAR_A16, -1, 16, 16)

#define CI_EXTEND_OPS(X) \
    X(SEXT8_16, 1, 2, 1) \
    X(ZEXT8_16, 1, 2, 0) \
    X(SEXT8_32, 1, 4, 1) \
    X(ZEXT8_32, 1, 4, 0) \
    X(SEXT8_64, 1, 8, 1) \
    X(ZEXT8_64, 1, 8, 0) \
    X(SEXT8_128, 1, 16, 1) \
    X(ZEXT8_128, 1, 16, 0) \
    X(SEXT16_32, 2, 4, 1) \
    X(ZEXT16_32, 2, 4, 0) \
    X(SEXT16_64, 2, 8, 1) \
    X(ZEXT16_64, 2, 8, 0) \
    X(SEXT16_128, 2, 16, 1) \
    X(ZEXT16_128, 2, 16, 0) \
    X(SEXT32_64, 4, 8, 1) \
    X(ZEXT32_64, 4, 8, 0) \
    X(SEXT32_128, 4, 16, 1) \
    X(ZEXT32_128, 4, 16, 0) \
    X(SEXT64_128, 8, 16, 1) \
    X(ZEXT64_128, 8, 16, 0)

#define CI_CHECKED_OPS(X) \
    X(CHECKED_ADD_S8, int8_t, int32_t, add_overflow) \
    X(CHECKED_ADD_U8, uint8_t, uint32_t, add_overflow) \
    X(CHECKED_ADD_S16, int16_t, int32_t, add_overflow) \
    X(CHECKED_ADD_U16, uint16_t, uint32_t, add_overflow) \
    X(CHECKED_ADD_S32, int32_t, int32_t, add_overflow) \
    X(CHECKED_ADD_U32, uint32_t, uint32_t, add_overflow) \
    X(CHECKED_ADD_S64, int64_t, int64_t, add_overflow) \
    X(CHECKED_ADD_U64, uint64_t, uint64_t, add_overflow) \
    X(CHECKED_SUB_S8, int8_t, int32_t, sub_overflow) \
    X(CHECKED_SUB_U8, uint8_t, uint32_t, sub_overflow) \
    X(CHECKED_SUB_S16, int16_t, int32_t, sub_overflow) \
    X(CHECKED_SUB_U16, uint16_t, uint32_t, sub_overflow) \
    X(CHECKED_SUB_S32, int32_t, int32_t, sub_overflow) \
    X(CHECKED_SUB_U32, uint32_t, uint32_t, sub_overflow) \
    X(CHECKED_SUB_S64, int64_t, int64_t, sub_overflow) \
    X(CHECKED_SUB_U64, uint64_t, uint64_t, sub_overflow) \
    X(CHECKED_MUL_S8, int8_t, int32_t, mul_overflow) \
    X(CHECKED_MUL_U8, uint8_t, uint32_t, mul_overflow) \
    X(CHECKED_MUL_S16, int16_t, int32_t, mul_overflow) \
    X(CHECKED_MUL_U16, uint16_t, uint32_t, mul_overflow) \
    X(CHECKED_MUL_S32, int32_t, int32_t, mul_overflow) \
    X(CHECKED_MUL_U32, uint32_t, uint32_t, mul_overflow) \
    X(CHECKED_MUL_S64, int64_t, int64_t, mul_overflow) \
    X(CHECKED_MUL_U64, uint64_t, uint64_t, mul_overflow)

#define CI_OVERFLOW_OPS(X) \
    X(OVERFLOW_S8, 8, 0) \
    X(OVERFLOW_U8, 8, 1) \
    X(OVERFLOW_S16, 16, 0) \
    X(OVERFLOW_U16, 16, 1) \
    X(OVERFLOW_S32, 32, 0) \
    X(OVERFLOW_U32, 32, 1) \
    X(OVERFLOW_S64, 64, 0) \
    X(OVERFLOW_U64, 64, 1)

#define CI_TRUTH_OPS(X) \
    X(ISTRUE_I8, I, 8, 0) \
    X(ISFALSE_I8, I, 8, 1) \
    X(ISTRUE_I16, I, 16, 0) \
    X(ISFALSE_I16, I, 16, 1) \
    X(ISTRUE_I32, I, 32, 0) \
    X(ISFALSE_I32, I, 32, 1) \
    X(ISTRUE_I64, I, 64, 0) \
    X(ISFALSE_I64, I, 64, 1) \
    X(ISTRUE_I128, I, 128, 0) \
    X(ISFALSE_I128, I, 128, 1) \
    X(ISTRUE_F16, F, 16, 0) \
    X(ISFALSE_F16, F, 16, 1) \
    X(ISTRUE_F32, F, 32, 0) \
    X(ISFALSE_F32, F, 32, 1) \
    X(ISTRUE_F64, F, 64, 0) \
    X(ISFALSE_F64, F, 64, 1) \
    X(ISTRUE_F80, F, 80, 0) \
    X(ISFALSE_F80, F, 80, 1) \
    X(ISTRUE_F128, F, 128, 0) \
    X(ISFALSE_F128, F, 128, 1)

// Conversions: name, source format, destination format (and integer result bytes for FTOI).
#define CI_ITOF_OPS(X) \
    X(S32_TO_F16, S32, F16) \
    X(U32_TO_F16, U32, F16) \
    X(S64_TO_F16, S64, F16) \
    X(U64_TO_F16, U64, F16) \
    X(S128_TO_F16, S128, F16) \
    X(U128_TO_F16, U128, F16) \
    X(S32_TO_BF16, S32, BF16) \
    X(U32_TO_BF16, U32, BF16) \
    X(S64_TO_BF16, S64, BF16) \
    X(U64_TO_BF16, U64, BF16) \
    X(S128_TO_BF16, S128, BF16) \
    X(U128_TO_BF16, U128, BF16) \
    X(S32_TO_F32, S32, F32) \
    X(U32_TO_F32, U32, F32) \
    X(S64_TO_F32, S64, F32) \
    X(U64_TO_F32, U64, F32) \
    X(S128_TO_F32, S128, F32) \
    X(U128_TO_F32, U128, F32) \
    X(S32_TO_F64, S32, F64) \
    X(U32_TO_F64, U32, F64) \
    X(S64_TO_F64, S64, F64) \
    X(U64_TO_F64, U64, F64) \
    X(S128_TO_F64, S128, F64) \
    X(U128_TO_F64, U128, F64) \
    X(S32_TO_F80, S32, F80) \
    X(U32_TO_F80, U32, F80) \
    X(S64_TO_F80, S64, F80) \
    X(U64_TO_F80, U64, F80) \
    X(S128_TO_F80, S128, F80) \
    X(U128_TO_F80, U128, F80) \
    X(S32_TO_F128, S32, F128) \
    X(U32_TO_F128, U32, F128) \
    X(S64_TO_F128, S64, F128) \
    X(U64_TO_F128, U64, F128) \
    X(S128_TO_F128, S128, F128) \
    X(U128_TO_F128, U128, F128)

#define CI_FTOI_OPS(X) \
    X(F16_TO_S32, F16, S32, 4) \
    X(F16_TO_U32, F16, U32, 4) \
    X(F16_TO_S64, F16, S64, 8) \
    X(F16_TO_U64, F16, U64, 8) \
    X(F16_TO_I128, F16, I128, 16) \
    X(BF16_TO_S32, BF16, S32, 4) \
    X(BF16_TO_U32, BF16, U32, 4) \
    X(BF16_TO_S64, BF16, S64, 8) \
    X(BF16_TO_U64, BF16, U64, 8) \
    X(BF16_TO_I128, BF16, I128, 16) \
    X(F32_TO_S32, F32, S32, 4) \
    X(F32_TO_U32, F32, U32, 4) \
    X(F32_TO_S64, F32, S64, 8) \
    X(F32_TO_U64, F32, U64, 8) \
    X(F32_TO_S128, F32, S128, 16) \
    X(F32_TO_U128, F32, U128, 16) \
    X(F64_TO_S32, F64, S32, 4) \
    X(F64_TO_U32, F64, U32, 4) \
    X(F64_TO_S64, F64, S64, 8) \
    X(F64_TO_U64, F64, U64, 8) \
    X(F64_TO_S128, F64, S128, 16) \
    X(F64_TO_U128, F64, U128, 16) \
    X(F80_TO_I8, F80, I8, 1) \
    X(F80_TO_I16, F80, I16, 2) \
    X(F80_TO_I32, F80, I32, 4) \
    X(F80_TO_I64, F80, I64, 8) \
    X(F80_TO_I128, F80, I128, 16) \
    X(F128_TO_I8, F128, I8, 1) \
    X(F128_TO_I16, F128, I16, 2) \
    X(F128_TO_I32, F128, I32, 4) \
    X(F128_TO_I64, F128, I64, 8) \
    X(F128_TO_I128, F128, I128, 16)

#define CI_FTOF_OPS(X) \
    X(F16_TO_BF16, F16, BF16) \
    X(F16_TO_F32, F16, F32) \
    X(F16_TO_F64, F16, F64) \
    X(F16_TO_F80, F16, F80) \
    X(F16_TO_F128, F16, F128) \
    X(BF16_TO_F16, BF16, F16) \
    X(BF16_TO_F32, BF16, F32) \
    X(BF16_TO_F64, BF16, F64) \
    X(BF16_TO_F80, BF16, F80) \
    X(BF16_TO_F128, BF16, F128) \
    X(F32_TO_F16, F32, F16) \
    X(F32_TO_BF16, F32, BF16) \
    X(F32_TO_F64, F32, F64) \
    X(F32_TO_F80, F32, F80) \
    X(F32_TO_F128, F32, F128) \
    X(F64_TO_F16, F64, F16) \
    X(F64_TO_BF16, F64, BF16) \
    X(F64_TO_F32, F64, F32) \
    X(F64_TO_F80, F64, F80) \
    X(F64_TO_F128, F64, F128) \
    X(F80_TO_F16, F80, F16) \
    X(F80_TO_BF16, F80, BF16) \
    X(F80_TO_F32, F80, F32) \
    X(F80_TO_F64, F80, F64) \
    X(F80_TO_F128, F80, F128) \
    X(F128_TO_F16, F128, F16) \
    X(F128_TO_BF16, F128, BF16) \
    X(F128_TO_F32, F128, F32) \
    X(F128_TO_F64, F128, F64) \
    X(F128_TO_F80, F128, F80)

// name, incoming, write, operation, byte width, alignment, extension source bytes
#define CI_ABI_MOVE_OPS(X) \
    X(ABI_READ_IN8, 1, 0, COPY, 1, 1, 0) \
    X(ABI_READ_IN16, 1, 0, COPY, 2, 1, 0) \
    X(ABI_READ_IN16_A, 1, 0, COPY, 2, 2, 0) \
    X(ABI_READ_IN32, 1, 0, COPY, 4, 1, 0) \
    X(ABI_READ_IN32_A, 1, 0, COPY, 4, 4, 0) \
    X(ABI_READ_IN64, 1, 0, COPY, 8, 1, 0) \
    X(ABI_READ_IN64_A, 1, 0, COPY, 8, 8, 0) \
    X(ABI_READ_IN128, 1, 0, COPY, 16, 1, 0) \
    X(ABI_READ_IN128_A, 1, 0, COPY, 16, 16, 0) \
    X(ABI_READ_IN_N, 1, 0, COPY, 0, 1, 0) \
    X(ABI_READ_IN_N_A2, 1, 0, COPY, 0, 2, 0) \
    X(ABI_READ_IN_N_A4, 1, 0, COPY, 0, 4, 0) \
    X(ABI_READ_IN_N_A8, 1, 0, COPY, 0, 8, 0) \
    X(ABI_READ_IN_N_A16, 1, 0, COPY, 0, 16, 0) \
    X(ABI_READ_OUT8, 0, 0, COPY, 1, 1, 0) \
    X(ABI_READ_OUT16, 0, 0, COPY, 2, 1, 0) \
    X(ABI_READ_OUT16_A, 0, 0, COPY, 2, 2, 0) \
    X(ABI_READ_OUT32, 0, 0, COPY, 4, 1, 0) \
    X(ABI_READ_OUT32_A, 0, 0, COPY, 4, 4, 0) \
    X(ABI_READ_OUT64, 0, 0, COPY, 8, 1, 0) \
    X(ABI_READ_OUT64_A, 0, 0, COPY, 8, 8, 0) \
    X(ABI_READ_OUT128, 0, 0, COPY, 16, 1, 0) \
    X(ABI_READ_OUT128_A, 0, 0, COPY, 16, 16, 0) \
    X(ABI_READ_OUT_N, 0, 0, COPY, 0, 1, 0) \
    X(ABI_READ_OUT_N_A2, 0, 0, COPY, 0, 2, 0) \
    X(ABI_READ_OUT_N_A4, 0, 0, COPY, 0, 4, 0) \
    X(ABI_READ_OUT_N_A8, 0, 0, COPY, 0, 8, 0) \
    X(ABI_READ_OUT_N_A16, 0, 0, COPY, 0, 16, 0) \
    X(ABI_WRITE_OUT8, 0, 1, COPY, 1, 1, 0) \
    X(ABI_WRITE_OUT16, 0, 1, COPY, 2, 1, 0) \
    X(ABI_WRITE_OUT16_A, 0, 1, COPY, 2, 2, 0) \
    X(ABI_WRITE_OUT32, 0, 1, COPY, 4, 1, 0) \
    X(ABI_WRITE_OUT32_A, 0, 1, COPY, 4, 4, 0) \
    X(ABI_WRITE_OUT64, 0, 1, COPY, 8, 1, 0) \
    X(ABI_WRITE_OUT64_A, 0, 1, COPY, 8, 8, 0) \
    X(ABI_WRITE_OUT128, 0, 1, COPY, 16, 1, 0) \
    X(ABI_WRITE_OUT128_A, 0, 1, COPY, 16, 16, 0) \
    X(ABI_WRITE_OUT_N, 0, 1, COPY, 0, 1, 0) \
    X(ABI_WRITE_OUT_N_A2, 0, 1, COPY, 0, 2, 0) \
    X(ABI_WRITE_OUT_N_A4, 0, 1, COPY, 0, 4, 0) \
    X(ABI_WRITE_OUT_N_A8, 0, 1, COPY, 0, 8, 0) \
    X(ABI_WRITE_OUT_N_A16, 0, 1, COPY, 0, 16, 0) \
    X(ABI_WRITE_SEXT8, 0, 1, SIGN_EXTEND, 8, 1, 1) \
    X(ABI_WRITE_SEXT16, 0, 1, SIGN_EXTEND, 8, 1, 2) \
    X(ABI_WRITE_SEXT16_A, 0, 1, SIGN_EXTEND, 8, 2, 2) \
    X(ABI_WRITE_SEXT32, 0, 1, SIGN_EXTEND, 8, 1, 4) \
    X(ABI_WRITE_SEXT32_A, 0, 1, SIGN_EXTEND, 8, 4, 4) \
    X(ABI_WRITE_ZEXT8, 0, 1, ZERO_EXTEND, 8, 1, 1) \
    X(ABI_WRITE_ZEXT16, 0, 1, ZERO_EXTEND, 8, 1, 2) \
    X(ABI_WRITE_ZEXT16_A, 0, 1, ZERO_EXTEND, 8, 2, 2) \
    X(ABI_WRITE_ZEXT32, 0, 1, ZERO_EXTEND, 8, 1, 4) \
    X(ABI_WRITE_ZEXT32_A, 0, 1, ZERO_EXTEND, 8, 4, 4)

// name, operation, source address mode, destination address mode, byte width, alignment
#define CI_ABI_TRANSFER_OPS(X) \
    X(ABI_COPY_SLOT_TO_REG_PTR_8, COPY, SLOT, REG_PTR, 1, 1) \
    X(ABI_COPY_SLOT_TO_REG_PTR_16, COPY, SLOT, REG_PTR, 2, 1) \
    X(ABI_COPY_SLOT_TO_REG_PTR_16_A, COPY, SLOT, REG_PTR, 2, 2) \
    X(ABI_COPY_SLOT_TO_REG_PTR_32, COPY, SLOT, REG_PTR, 4, 1) \
    X(ABI_COPY_SLOT_TO_REG_PTR_32_A, COPY, SLOT, REG_PTR, 4, 4) \
    X(ABI_COPY_SLOT_TO_REG_PTR_64, COPY, SLOT, REG_PTR, 8, 1) \
    X(ABI_COPY_SLOT_TO_REG_PTR_64_A, COPY, SLOT, REG_PTR, 8, 8) \
    X(ABI_COPY_SLOT_TO_REG_PTR_128, COPY, SLOT, REG_PTR, 16, 1) \
    X(ABI_COPY_SLOT_TO_REG_PTR_128_A, COPY, SLOT, REG_PTR, 16, 16) \
    X(ABI_COPY_SLOT_TO_REG_PTR_N, COPY, SLOT, REG_PTR, 0, 1) \
    X(ABI_COPY_SLOT_TO_REG_PTR_N_A2, COPY, SLOT, REG_PTR, 0, 2) \
    X(ABI_COPY_SLOT_TO_REG_PTR_N_A4, COPY, SLOT, REG_PTR, 0, 4) \
    X(ABI_COPY_SLOT_TO_REG_PTR_N_A8, COPY, SLOT, REG_PTR, 0, 8) \
    X(ABI_COPY_SLOT_TO_REG_PTR_N_A16, COPY, SLOT, REG_PTR, 0, 16) \
    X(ABI_COPY_REG_PTR_TO_SLOT_8, COPY, REG_PTR, SLOT, 1, 1) \
    X(ABI_COPY_REG_PTR_TO_SLOT_16, COPY, REG_PTR, SLOT, 2, 1) \
    X(ABI_COPY_REG_PTR_TO_SLOT_16_A, COPY, REG_PTR, SLOT, 2, 2) \
    X(ABI_COPY_REG_PTR_TO_SLOT_32, COPY, REG_PTR, SLOT, 4, 1) \
    X(ABI_COPY_REG_PTR_TO_SLOT_32_A, COPY, REG_PTR, SLOT, 4, 4) \
    X(ABI_COPY_REG_PTR_TO_SLOT_64, COPY, REG_PTR, SLOT, 8, 1) \
    X(ABI_COPY_REG_PTR_TO_SLOT_64_A, COPY, REG_PTR, SLOT, 8, 8) \
    X(ABI_COPY_REG_PTR_TO_SLOT_128, COPY, REG_PTR, SLOT, 16, 1) \
    X(ABI_COPY_REG_PTR_TO_SLOT_128_A, COPY, REG_PTR, SLOT, 16, 16) \
    X(ABI_COPY_REG_PTR_TO_SLOT_N, COPY, REG_PTR, SLOT, 0, 1) \
    X(ABI_COPY_REG_PTR_TO_SLOT_N_A2, COPY, REG_PTR, SLOT, 0, 2) \
    X(ABI_COPY_REG_PTR_TO_SLOT_N_A4, COPY, REG_PTR, SLOT, 0, 4) \
    X(ABI_COPY_REG_PTR_TO_SLOT_N_A8, COPY, REG_PTR, SLOT, 0, 8) \
    X(ABI_COPY_REG_PTR_TO_SLOT_N_A16, COPY, REG_PTR, SLOT, 0, 16) \
    X(ABI_COPY_REG_PTR_INDIRECT_TO_SLOT_8, COPY, REG_PTR_INDIRECT, SLOT, 1, 1) \
    X(ABI_COPY_REG_PTR_INDIRECT_TO_SLOT_16, COPY, REG_PTR_INDIRECT, SLOT, 2, 1) \
    X(ABI_COPY_REG_PTR_INDIRECT_TO_SLOT_16_A, COPY, REG_PTR_INDIRECT, SLOT, 2, 2) \
    X(ABI_COPY_REG_PTR_INDIRECT_TO_SLOT_32, COPY, REG_PTR_INDIRECT, SLOT, 4, 1) \
    X(ABI_COPY_REG_PTR_INDIRECT_TO_SLOT_32_A, COPY, REG_PTR_INDIRECT, SLOT, 4, 4) \
    X(ABI_COPY_REG_PTR_INDIRECT_TO_SLOT_64, COPY, REG_PTR_INDIRECT, SLOT, 8, 1) \
    X(ABI_COPY_REG_PTR_INDIRECT_TO_SLOT_64_A, COPY, REG_PTR_INDIRECT, SLOT, 8, 8) \
    X(ABI_COPY_REG_PTR_INDIRECT_TO_SLOT_128, COPY, REG_PTR_INDIRECT, SLOT, 16, 1) \
    X(ABI_COPY_REG_PTR_INDIRECT_TO_SLOT_128_A, COPY, REG_PTR_INDIRECT, SLOT, 16, 16) \
    X(ABI_COPY_REG_PTR_INDIRECT_TO_SLOT_N, COPY, REG_PTR_INDIRECT, SLOT, 0, 1) \
    X(ABI_COPY_REG_PTR_INDIRECT_TO_SLOT_N_A2, COPY, REG_PTR_INDIRECT, SLOT, 0, 2) \
    X(ABI_COPY_REG_PTR_INDIRECT_TO_SLOT_N_A4, COPY, REG_PTR_INDIRECT, SLOT, 0, 4) \
    X(ABI_COPY_REG_PTR_INDIRECT_TO_SLOT_N_A8, COPY, REG_PTR_INDIRECT, SLOT, 0, 8) \
    X(ABI_COPY_REG_PTR_INDIRECT_TO_SLOT_N_A16, COPY, REG_PTR_INDIRECT, SLOT, 0, 16) \
    X(ABI_COPY_SLOT_TO_SLOT_PTR_8, COPY, SLOT, SLOT_PTR, 1, 1) \
    X(ABI_COPY_SLOT_TO_SLOT_PTR_16, COPY, SLOT, SLOT_PTR, 2, 1) \
    X(ABI_COPY_SLOT_TO_SLOT_PTR_16_A, COPY, SLOT, SLOT_PTR, 2, 2) \
    X(ABI_COPY_SLOT_TO_SLOT_PTR_32, COPY, SLOT, SLOT_PTR, 4, 1) \
    X(ABI_COPY_SLOT_TO_SLOT_PTR_32_A, COPY, SLOT, SLOT_PTR, 4, 4) \
    X(ABI_COPY_SLOT_TO_SLOT_PTR_64, COPY, SLOT, SLOT_PTR, 8, 1) \
    X(ABI_COPY_SLOT_TO_SLOT_PTR_64_A, COPY, SLOT, SLOT_PTR, 8, 8) \
    X(ABI_COPY_SLOT_TO_SLOT_PTR_128, COPY, SLOT, SLOT_PTR, 16, 1) \
    X(ABI_COPY_SLOT_TO_SLOT_PTR_128_A, COPY, SLOT, SLOT_PTR, 16, 16) \
    X(ABI_COPY_SLOT_TO_SLOT_PTR_N, COPY, SLOT, SLOT_PTR, 0, 1) \
    X(ABI_COPY_SLOT_TO_SLOT_PTR_N_A2, COPY, SLOT, SLOT_PTR, 0, 2) \
    X(ABI_COPY_SLOT_TO_SLOT_PTR_N_A4, COPY, SLOT, SLOT_PTR, 0, 4) \
    X(ABI_COPY_SLOT_TO_SLOT_PTR_N_A8, COPY, SLOT, SLOT_PTR, 0, 8) \
    X(ABI_COPY_SLOT_TO_SLOT_PTR_N_A16, COPY, SLOT, SLOT_PTR, 0, 16) \
    X(ABI_COPY_INCOMING_TO_INCOMING_PTR_64_A, COPY, INCOMING, INCOMING_PTR, 8, 8) \
    X(ABI_ADDRESS_REG_PTR_TO_REG_PTR, ADDRESS, REG_PTR, REG, 0, 1) \
    X(ABI_ADDRESS_REG_PTR_TO_REG_PTR_A, ADDRESS, REG_PTR, REG, 0, 8) \
    X(ABI_ADDRESS_REG_PTR_TO_REG_PTR_PTR, ADDRESS, REG_PTR, REG_PTR, 0, 1) \
    X(ABI_ADDRESS_REG_PTR_TO_REG_PTR_PTR_A, ADDRESS, REG_PTR, REG_PTR, 0, 8) \
    X(ABI_ADDRESS_SLOT_PTR_TO_REG_PTR, ADDRESS, SLOT_PTR, REG, 0, 1) \
    X(ABI_ADDRESS_SLOT_PTR_TO_REG_PTR_A, ADDRESS, SLOT_PTR, REG, 0, 8) \
    X(ABI_ADDRESS_SLOT_TO_REG_PTR, ADDRESS, SLOT, REG, 0, 1) \
    X(ABI_ADDRESS_SLOT_TO_REG_PTR_A, ADDRESS, SLOT, REG, 0, 8) \
    X(ABI_ADDRESS_SLOT_TO_SLOT_PTR, ADDRESS, SLOT, SLOT, 0, 1) \
    X(ABI_ADDRESS_SLOT_TO_SLOT_PTR_A, ADDRESS, SLOT, SLOT, 0, 8) \
    X(ABI_ADDRESS_INCOMING_PTR_TO_SLOT_PTR_PTR, ADDRESS, INCOMING_PTR, SLOT_PTR, 0, 1) \
    X(ABI_ADDRESS_INCOMING_PTR_TO_SLOT_PTR_PTR_A, ADDRESS, INCOMING_PTR, SLOT_PTR, 0, 8) \
    X(ABI_ADDRESS_INCOMING_SAVE_TO_SLOT_PTR_PTR, ADDRESS, INCOMING_SAVE, SLOT_PTR, 0, 1) \
    X(ABI_ADDRESS_INCOMING_SAVE_TO_SLOT_PTR_PTR_A, ADDRESS, INCOMING_SAVE, SLOT_PTR, 0, 8) \
    X(ABI_ZERO_SLOT_8, ZERO, SLOT, SLOT, 1, 1) \
    X(ABI_ZERO_SLOT_16, ZERO, SLOT, SLOT, 2, 1) \
    X(ABI_ZERO_SLOT_16_A, ZERO, SLOT, SLOT, 2, 2) \
    X(ABI_ZERO_SLOT_32, ZERO, SLOT, SLOT, 4, 1) \
    X(ABI_ZERO_SLOT_32_A, ZERO, SLOT, SLOT, 4, 4) \
    X(ABI_ZERO_SLOT_64, ZERO, SLOT, SLOT, 8, 1) \
    X(ABI_ZERO_SLOT_64_A, ZERO, SLOT, SLOT, 8, 8) \
    X(ABI_ZERO_SLOT_128, ZERO, SLOT, SLOT, 16, 1) \
    X(ABI_ZERO_SLOT_128_A, ZERO, SLOT, SLOT, 16, 16) \
    X(ABI_ZERO_SLOT_N, ZERO, SLOT, SLOT, 0, 1) \
    X(ABI_ZERO_SLOT_N_A2, ZERO, SLOT, SLOT, 0, 2) \
    X(ABI_ZERO_SLOT_N_A4, ZERO, SLOT, SLOT, 0, 4) \
    X(ABI_ZERO_SLOT_N_A8, ZERO, SLOT, SLOT, 0, 8) \
    X(ABI_ZERO_SLOT_N_A16, ZERO, SLOT, SLOT, 0, 16) \
    X(ABI_CONSTANT_REG_8, CONSTANT, SLOT, REG, 1, 1) \
    X(ABI_CONSTANT_SLOT_PTR_32, CONSTANT, SLOT, SLOT_PTR, 4, 1) \
    X(ABI_CONSTANT_SLOT_PTR_32_A, CONSTANT, SLOT, SLOT_PTR, 4, 4)

#define CI_JUMP_FALSE_OPS(X) \
    X(JZ8, 1) \
    X(JZ16, 2) \
    X(JZ32, 4) \
    X(JZ64, 8) \
    X(JZ128, 16)

#define CI_JUMP_TRUE_OPS(X) \
    X(JNZ8, 1) \
    X(JNZ16, 2) \
    X(JNZ32, 4) \
    X(JNZ64, 8) \
    X(JNZ128, 16)

#define CI_BSWAP_OPS(X) \
    X(BSWAP16, 2, uint16_t, bswap16) \
    X(BSWAP32, 4, uint32_t, bswap32) \
    X(BSWAP64, 8, uint64_t, bswap64)

#define CI_BOUNDS_OPS(X) \
    X(BOUNDS32, 4, >=) \
    X(BOUNDS_INCLUSIVE32, 4, >) \
    X(BOUNDS64, 8, >=) \
    X(BOUNDS_INCLUSIVE64, 8, >)

#define CI_INDEX_OPS(X) \
    X(INDEX32_S8, 4, 1, 1) \
    X(INDEX32_U8, 4, 1, 0) \
    X(INDEX32_S16, 4, 2, 1) \
    X(INDEX32_U16, 4, 2, 0) \
    X(INDEX32, 4, 4, 0) \
    X(INDEX64_S8, 8, 1, 1) \
    X(INDEX64_U8, 8, 1, 0) \
    X(INDEX64_S16, 8, 2, 1) \
    X(INDEX64_U16, 8, 2, 0) \
    X(INDEX64_S32, 8, 4, 1) \
    X(INDEX64_U32, 8, 4, 0) \
    X(INDEX64, 8, 8, 0)

#define CI_LOAD_BITFIELD_OPS(X) \
    X(LDBF_U8, 1, 0, 1) \
    X(LDBF_S8, 1, 1, 1) \
    X(LDBF_U16, 2, 0, 1) \
    X(LDBF_U16_A, 2, 0, 2) \
    X(LDBF_S16, 2, 1, 1) \
    X(LDBF_S16_A, 2, 1, 2) \
    X(LDBF_U32, 4, 0, 1) \
    X(LDBF_U32_A, 4, 0, 4) \
    X(LDBF_S32, 4, 1, 1) \
    X(LDBF_S32_A, 4, 1, 4) \
    X(LDBF_U64, 8, 0, 1) \
    X(LDBF_U64_A, 8, 0, 8) \
    X(LDBF_S64, 8, 1, 1) \
    X(LDBF_S64_A, 8, 1, 8)

#define CI_LOAD_BITFIELD_128_OPS(X) \
    X(LDBF_U128, 1, 1) \
    X(LDBF_U128_A, 1, 16) \
    X(LDBF_S128, 0, 1) \
    X(LDBF_S128_A, 0, 16)

#define CI_LOAD_BITFIELD_BYTES_OPS(X) \
    X(LDBFB_U8, 1, 0) \
    X(LDBFB_S8, 1, 1) \
    X(LDBFB_U16, 2, 0) \
    X(LDBFB_S16, 2, 1) \
    X(LDBFB_U32, 4, 0) \
    X(LDBFB_S32, 4, 1) \
    X(LDBFB_U64, 8, 0) \
    X(LDBFB_S64, 8, 1)

#define CI_LOAD_BITFIELD_BYTES_128_OPS(X) \
    X(LDBFB_U128, 1) \
    X(LDBFB_S128, 0)

#define CI_STORE_BITFIELD_OPS(X) \
    X(STBF8, 1, 1) \
    X(STBF16, 2, 1) \
    X(STBF16_A, 2, 2) \
    X(STBF32, 4, 1) \
    X(STBF32_A, 4, 4) \
    X(STBF64, 8, 1) \
    X(STBF64_A, 8, 8)

#define CI_STORE_BITFIELD_128_OPS(X) \
    X(STBF128, 1) \
    X(STBF128_A, 16)

#define CI_STORE_BITFIELD_BYTES_OPS(X) \
    X(STBFB8, 1) \
    X(STBFB16, 2) \
    X(STBFB32, 4) \
    X(STBFB64, 8)

#define CI_STORE_BITFIELD_BYTES_128_OPS(X) \
    X(STBFB128, 0)

#define CI_CALL_OPS(X) \
    X(CALL_DIRECT, 0) \
    X(CALL_INDIRECT, 1)

#define CI_SWITCH_DENSE_OPS(X) \
    X(SWITCH_DENSE, 0)

#define CI_SWITCH_RANGE_OPS(X) \
    X(SWITCH_SIGNED, UINT64_C(1) << 63) \
    X(SWITCH_UNSIGNED, 0)

#define CI_BUILTIN_OPS(X) \
    X(TRAP, ERROR, "__builtin_trap") \
    X(UNREACHABLE, ERROR, "__builtin_unreachable reached") \
    X(ABORT, ERROR, "__builtin_abort called") \
    X(DEBUGTRAP, NOOP, 0) \
    X(BACKTRACE, BACKTRACE, 0) \
    X(READCYCLECOUNTER, COUNTER, 1) \
    X(READCYCLECOUNTER_DISCARD, COUNTER, 0)

#define CI_ALLOCA_OPS(X) \
    X(STACK_ALLOC, ci_stack_alloc) \
    X(STACK_ZALLOC, ci_stack_zalloc)

#define CI_FENCE_OPS(X) \
    X(THREAD_FENCE, 1) \
    X(SIGNAL_FENCE, 0)

#define CI_BIT_COUNT_OPS(X) \
    X(FFS8, 8, FFS) \
    X(FFS16, 16, FFS) \
    X(FFS32, 32, FFS) \
    X(FFS64, 64, FFS) \
    X(FFS128, 128, FFS) \
    X(CLRSB8, 8, CLRSB) \
    X(CLRSB16, 16, CLRSB) \
    X(CLRSB32, 32, CLRSB) \
    X(CLRSB64, 64, CLRSB) \
    X(CLRSB128, 128, CLRSB) \
    X(PARITY8, 8, PARITY) \
    X(PARITY16, 16, PARITY) \
    X(PARITY32, 32, PARITY) \
    X(PARITY64, 64, PARITY) \
    X(PARITY128, 128, PARITY) \
    X(CLZ8, 8, CLZ) \
    X(CLZ16, 16, CLZ) \
    X(CLZ32, 32, CLZ) \
    X(CLZ64, 64, CLZ) \
    X(CLZ128, 128, CLZ) \
    X(CTZ8, 8, CTZ) \
    X(CTZ16, 16, CTZ) \
    X(CTZ32, 32, CTZ) \
    X(CTZ64, 64, CTZ) \
    X(CTZ128, 128, CTZ) \
    X(POPCOUNT8, 8, POPCOUNT) \
    X(POPCOUNT16, 16, POPCOUNT) \
    X(POPCOUNT32, 32, POPCOUNT) \
    X(POPCOUNT64, 64, POPCOUNT) \
    X(POPCOUNT128, 128, POPCOUNT) \
    X(BIT_WIDTH8, 8, BIT_WIDTH) \
    X(BIT_WIDTH16, 16, BIT_WIDTH) \
    X(BIT_WIDTH32, 32, BIT_WIDTH) \
    X(BIT_WIDTH64, 64, BIT_WIDTH) \
    X(BIT_WIDTH128, 128, BIT_WIDTH) \
    X(COUNT_ZEROS8, 8, COUNT_ZEROS) \
    X(COUNT_ZEROS16, 16, COUNT_ZEROS) \
    X(COUNT_ZEROS32, 32, COUNT_ZEROS) \
    X(COUNT_ZEROS64, 64, COUNT_ZEROS) \
    X(COUNT_ZEROS128, 128, COUNT_ZEROS) \
    X(FIRST_LEADING_ONE8, 8, FIRST_LEADING_ONE) \
    X(FIRST_LEADING_ONE16, 16, FIRST_LEADING_ONE) \
    X(FIRST_LEADING_ONE32, 32, FIRST_LEADING_ONE) \
    X(FIRST_LEADING_ONE64, 64, FIRST_LEADING_ONE) \
    X(FIRST_LEADING_ONE128, 128, FIRST_LEADING_ONE) \
    X(FIRST_LEADING_ZERO8, 8, FIRST_LEADING_ZERO) \
    X(FIRST_LEADING_ZERO16, 16, FIRST_LEADING_ZERO) \
    X(FIRST_LEADING_ZERO32, 32, FIRST_LEADING_ZERO) \
    X(FIRST_LEADING_ZERO64, 64, FIRST_LEADING_ZERO) \
    X(FIRST_LEADING_ZERO128, 128, FIRST_LEADING_ZERO) \
    X(FIRST_TRAILING_ZERO8, 8, FIRST_TRAILING_ZERO) \
    X(FIRST_TRAILING_ZERO16, 16, FIRST_TRAILING_ZERO) \
    X(FIRST_TRAILING_ZERO32, 32, FIRST_TRAILING_ZERO) \
    X(FIRST_TRAILING_ZERO64, 64, FIRST_TRAILING_ZERO) \
    X(FIRST_TRAILING_ZERO128, 128, FIRST_TRAILING_ZERO) \
    X(LEADING_ONES8, 8, LEADING_ONES) \
    X(LEADING_ONES16, 16, LEADING_ONES) \
    X(LEADING_ONES32, 32, LEADING_ONES) \
    X(LEADING_ONES64, 64, LEADING_ONES) \
    X(LEADING_ONES128, 128, LEADING_ONES) \
    X(TRAILING_ONES8, 8, TRAILING_ONES) \
    X(TRAILING_ONES16, 16, TRAILING_ONES) \
    X(TRAILING_ONES32, 32, TRAILING_ONES) \
    X(TRAILING_ONES64, 64, TRAILING_ONES) \
    X(TRAILING_ONES128, 128, TRAILING_ONES)

#define CI_SINGLE_BIT_OPS(X) \
    X(HAS_SINGLE_BIT8, 8) \
    X(HAS_SINGLE_BIT16, 16) \
    X(HAS_SINGLE_BIT32, 32) \
    X(HAS_SINGLE_BIT64, 64) \
    X(HAS_SINGLE_BIT128, 128)

#define CI_BIT_CEIL_OPS(X) \
    X(BIT_CEIL8, 8) \
    X(BIT_CEIL16, 16) \
    X(BIT_CEIL32, 32) \
    X(BIT_CEIL64, 64) \
    X(BIT_CEIL128, 128)

#define CI_BIT_FLOOR_OPS(X) \
    X(BIT_FLOOR8, 8) \
    X(BIT_FLOOR16, 16) \
    X(BIT_FLOOR32, 32) \
    X(BIT_FLOOR64, 64) \
    X(BIT_FLOOR128, 128)

#define CI_ROTATE_OPS(X) \
    X(ROTL8, 8, ci_uint128_shl, ci_uint128_shr) \
    X(ROTR8, 8, ci_uint128_shr, ci_uint128_shl) \
    X(ROTL16, 16, ci_uint128_shl, ci_uint128_shr) \
    X(ROTR16, 16, ci_uint128_shr, ci_uint128_shl) \
    X(ROTL32, 32, ci_uint128_shl, ci_uint128_shr) \
    X(ROTR32, 32, ci_uint128_shr, ci_uint128_shl) \
    X(ROTL64, 64, ci_uint128_shl, ci_uint128_shr) \
    X(ROTR64, 64, ci_uint128_shr, ci_uint128_shl) \
    X(ROTL128, 128, ci_uint128_shl, ci_uint128_shr) \
    X(ROTR128, 128, ci_uint128_shr, ci_uint128_shl)

#define CI_CRC_OPS(X) \
    X(CRC32C8, 1) \
    X(CRC32C16, 2) \
    X(CRC32C32, 4) \
    X(CRC32C64, 8)

#define CI_ATOMIC_LOAD_OPS(X) \
    X(ATOMIC_LOAD8, 8) \
    X(ATOMIC_LOAD16, 16) \
    X(ATOMIC_LOAD32, 32) \
    X(ATOMIC_LOAD64, 64) \
    X(ATOMIC_LOAD128, 128)

#define CI_ATOMIC_STORE_OPS(X) \
    X(ATOMIC_STORE8, 8) \
    X(ATOMIC_STORE16, 16) \
    X(ATOMIC_STORE32, 32) \
    X(ATOMIC_STORE64, 64) \
    X(ATOMIC_STORE128, 128)

#define CI_ATOMIC_RMW_OPS(X) \
    X(ATOMIC_XCHG8, CI_ATOMIC_XCHG8) \
    X(ATOMIC_XCHG16, CI_ATOMIC_XCHG16) \
    X(ATOMIC_XCHG32, CI_ATOMIC_XCHG32) \
    X(ATOMIC_XCHG64, CI_ATOMIC_XCHG64) \
    X(ATOMIC_XCHG128, CI_ATOMIC_XCHG128) \
    X(ATOMIC_ADD8, CI_ATOMIC_ADD8) \
    X(ATOMIC_ADD16, CI_ATOMIC_ADD16) \
    X(ATOMIC_ADD32, CI_ATOMIC_ADD32) \
    X(ATOMIC_ADD64, CI_ATOMIC_ADD64) \
    X(ATOMIC_SUB8, CI_ATOMIC_SUB8) \
    X(ATOMIC_SUB16, CI_ATOMIC_SUB16) \
    X(ATOMIC_SUB32, CI_ATOMIC_SUB32) \
    X(ATOMIC_SUB64, CI_ATOMIC_SUB64) \
    X(ATOMIC_AND8, CI_ATOMIC_AND8) \
    X(ATOMIC_AND16, CI_ATOMIC_AND16) \
    X(ATOMIC_AND32, CI_ATOMIC_AND32) \
    X(ATOMIC_AND64, CI_ATOMIC_AND64) \
    X(ATOMIC_OR8, CI_ATOMIC_OR8) \
    X(ATOMIC_OR16, CI_ATOMIC_OR16) \
    X(ATOMIC_OR32, CI_ATOMIC_OR32) \
    X(ATOMIC_OR64, CI_ATOMIC_OR64) \
    X(ATOMIC_XOR8, CI_ATOMIC_XOR8) \
    X(ATOMIC_XOR16, CI_ATOMIC_XOR16) \
    X(ATOMIC_XOR32, CI_ATOMIC_XOR32) \
    X(ATOMIC_XOR64, CI_ATOMIC_XOR64)

#define CI_ATOMIC_CAS_OPS(X) \
    X(ATOMIC_CAS8, 8, 0) \
    X(ATOMIC_CAS_WEAK8, 8, 1) \
    X(ATOMIC_CAS16, 16, 0) \
    X(ATOMIC_CAS_WEAK16, 16, 1) \
    X(ATOMIC_CAS32, 32, 0) \
    X(ATOMIC_CAS_WEAK32, 32, 1) \
    X(ATOMIC_CAS64, 64, 0) \
    X(ATOMIC_CAS_WEAK64, 64, 1) \
    X(ATOMIC_CAS128, 128, 0) \
    X(ATOMIC_CAS_WEAK128, 128, 1)

#define CI_VA_SYSV_OPS(X) \
    X(VA_ARG_SYSV, 0)

#define CI_VA_ARM_OPS(X) \
    X(VA_ARG_ARM, 1) \
    X(VA_ARG_ARM_ALIGN16, 16)

#define CI_VA_ARM_INDIRECT_OPS(X) \
    X(VA_ARG_ARM_INDIRECT, 0)

#define CI_VA_STACK_OPS(X) \
    X(VA_ARG_STACK, STACK, 0) \
    X(VA_ARG_STACK_INDIRECT, STACK, 1) \
    X(VA_ARG_SYSV_STACK, SYSV, 0) \
    X(VA_ARG_ARM_STACK, ARM, 0)

// name, write, GP register index, slot alignment
#define CI_ABI_GP_MOVE_OPS(X) \
    X(ABI_READ_GP0_64, 0, 0, 1) \
    X(ABI_READ_GP0_64_A, 0, 0, 8) \
    X(ABI_WRITE_GP0_64, 1, 0, 1) \
    X(ABI_WRITE_GP0_64_A, 1, 0, 8) \
    X(ABI_READ_GP1_64, 0, 1, 1) \
    X(ABI_READ_GP1_64_A, 0, 1, 8) \
    X(ABI_WRITE_GP1_64, 1, 1, 1) \
    X(ABI_WRITE_GP1_64_A, 1, 1, 8) \
    X(ABI_READ_GP2_64, 0, 2, 1) \
    X(ABI_READ_GP2_64_A, 0, 2, 8) \
    X(ABI_WRITE_GP2_64, 1, 2, 1) \
    X(ABI_WRITE_GP2_64_A, 1, 2, 8) \
    X(ABI_READ_GP3_64, 0, 3, 1) \
    X(ABI_READ_GP3_64_A, 0, 3, 8) \
    X(ABI_WRITE_GP3_64, 1, 3, 1) \
    X(ABI_WRITE_GP3_64_A, 1, 3, 8) \
    X(ABI_READ_GP4_64, 0, 4, 1) \
    X(ABI_READ_GP4_64_A, 0, 4, 8) \
    X(ABI_WRITE_GP4_64, 1, 4, 1) \
    X(ABI_WRITE_GP4_64_A, 1, 4, 8) \
    X(ABI_READ_GP5_64, 0, 5, 1) \
    X(ABI_READ_GP5_64_A, 0, 5, 8) \
    X(ABI_WRITE_GP5_64, 1, 5, 1) \
    X(ABI_WRITE_GP5_64_A, 1, 5, 8) \
    X(ABI_READ_GP6_64, 0, 6, 1) \
    X(ABI_READ_GP6_64_A, 0, 6, 8) \
    X(ABI_WRITE_GP6_64, 1, 6, 1) \
    X(ABI_WRITE_GP6_64_A, 1, 6, 8) \
    X(ABI_READ_GP7_64, 0, 7, 1) \
    X(ABI_READ_GP7_64_A, 0, 7, 8) \
    X(ABI_WRITE_GP7_64, 1, 7, 1) \
    X(ABI_WRITE_GP7_64_A, 1, 7, 8)

#define CI_ABI_REG_MOVE_OPS(X) \
    X(ABI_READ_GP0_8, 0, 0, 1, 1) \
    X(ABI_WRITE_GP0_8, 1, 0, 1, 1) \
    X(ABI_READ_GP0_32, 0, 0, 4, 1) \
    X(ABI_READ_GP0_32_A, 0, 0, 4, 4) \
    X(ABI_WRITE_GP0_32, 1, 0, 4, 1) \
    X(ABI_WRITE_GP0_32_A, 1, 0, 4, 4) \
    X(ABI_SEXT_GP0_8, 1, 0, 1, 1) \
    X(ABI_ZEXT_GP0_8, 1, 0, 1, 1) \
    X(ABI_SEXT_GP0_32, 1, 0, 4, 1) \
    X(ABI_SEXT_GP0_32_A, 1, 0, 4, 4) \
    X(ABI_ZEXT_GP0_32, 1, 0, 4, 1) \
    X(ABI_ZEXT_GP0_32_A, 1, 0, 4, 4) \
    X(ABI_READ_GP1_8, 0, 1, 1, 1) \
    X(ABI_WRITE_GP1_8, 1, 1, 1, 1) \
    X(ABI_READ_GP1_32, 0, 1, 4, 1) \
    X(ABI_READ_GP1_32_A, 0, 1, 4, 4) \
    X(ABI_WRITE_GP1_32, 1, 1, 4, 1) \
    X(ABI_WRITE_GP1_32_A, 1, 1, 4, 4) \
    X(ABI_SEXT_GP1_8, 1, 1, 1, 1) \
    X(ABI_ZEXT_GP1_8, 1, 1, 1, 1) \
    X(ABI_SEXT_GP1_32, 1, 1, 4, 1) \
    X(ABI_SEXT_GP1_32_A, 1, 1, 4, 4) \
    X(ABI_ZEXT_GP1_32, 1, 1, 4, 1) \
    X(ABI_ZEXT_GP1_32_A, 1, 1, 4, 4) \
    X(ABI_READ_GP2_8, 0, 2, 1, 1) \
    X(ABI_WRITE_GP2_8, 1, 2, 1, 1) \
    X(ABI_READ_GP2_32, 0, 2, 4, 1) \
    X(ABI_READ_GP2_32_A, 0, 2, 4, 4) \
    X(ABI_WRITE_GP2_32, 1, 2, 4, 1) \
    X(ABI_WRITE_GP2_32_A, 1, 2, 4, 4) \
    X(ABI_SEXT_GP2_8, 1, 2, 1, 1) \
    X(ABI_ZEXT_GP2_8, 1, 2, 1, 1) \
    X(ABI_SEXT_GP2_32, 1, 2, 4, 1) \
    X(ABI_SEXT_GP2_32_A, 1, 2, 4, 4) \
    X(ABI_ZEXT_GP2_32, 1, 2, 4, 1) \
    X(ABI_ZEXT_GP2_32_A, 1, 2, 4, 4) \
    X(ABI_READ_GP3_8, 0, 3, 1, 1) \
    X(ABI_WRITE_GP3_8, 1, 3, 1, 1) \
    X(ABI_READ_GP3_32, 0, 3, 4, 1) \
    X(ABI_READ_GP3_32_A, 0, 3, 4, 4) \
    X(ABI_WRITE_GP3_32, 1, 3, 4, 1) \
    X(ABI_WRITE_GP3_32_A, 1, 3, 4, 4) \
    X(ABI_SEXT_GP3_8, 1, 3, 1, 1) \
    X(ABI_ZEXT_GP3_8, 1, 3, 1, 1) \
    X(ABI_SEXT_GP3_32, 1, 3, 4, 1) \
    X(ABI_SEXT_GP3_32_A, 1, 3, 4, 4) \
    X(ABI_ZEXT_GP3_32, 1, 3, 4, 1) \
    X(ABI_ZEXT_GP3_32_A, 1, 3, 4, 4) \
    X(ABI_READ_GP4_8, 0, 4, 1, 1) \
    X(ABI_WRITE_GP4_8, 1, 4, 1, 1) \
    X(ABI_READ_GP4_32, 0, 4, 4, 1) \
    X(ABI_READ_GP4_32_A, 0, 4, 4, 4) \
    X(ABI_WRITE_GP4_32, 1, 4, 4, 1) \
    X(ABI_WRITE_GP4_32_A, 1, 4, 4, 4) \
    X(ABI_SEXT_GP4_8, 1, 4, 1, 1) \
    X(ABI_ZEXT_GP4_8, 1, 4, 1, 1) \
    X(ABI_SEXT_GP4_32, 1, 4, 4, 1) \
    X(ABI_SEXT_GP4_32_A, 1, 4, 4, 4) \
    X(ABI_ZEXT_GP4_32, 1, 4, 4, 1) \
    X(ABI_ZEXT_GP4_32_A, 1, 4, 4, 4) \
    X(ABI_READ_GP5_8, 0, 5, 1, 1) \
    X(ABI_WRITE_GP5_8, 1, 5, 1, 1) \
    X(ABI_READ_GP5_32, 0, 5, 4, 1) \
    X(ABI_READ_GP5_32_A, 0, 5, 4, 4) \
    X(ABI_WRITE_GP5_32, 1, 5, 4, 1) \
    X(ABI_WRITE_GP5_32_A, 1, 5, 4, 4) \
    X(ABI_SEXT_GP5_8, 1, 5, 1, 1) \
    X(ABI_ZEXT_GP5_8, 1, 5, 1, 1) \
    X(ABI_SEXT_GP5_32, 1, 5, 4, 1) \
    X(ABI_SEXT_GP5_32_A, 1, 5, 4, 4) \
    X(ABI_ZEXT_GP5_32, 1, 5, 4, 1) \
    X(ABI_ZEXT_GP5_32_A, 1, 5, 4, 4) \
    X(ABI_READ_GP6_8, 0, 6, 1, 1) \
    X(ABI_WRITE_GP6_8, 1, 6, 1, 1) \
    X(ABI_READ_GP6_32, 0, 6, 4, 1) \
    X(ABI_READ_GP6_32_A, 0, 6, 4, 4) \
    X(ABI_WRITE_GP6_32, 1, 6, 4, 1) \
    X(ABI_WRITE_GP6_32_A, 1, 6, 4, 4) \
    X(ABI_SEXT_GP6_8, 1, 6, 1, 1) \
    X(ABI_ZEXT_GP6_8, 1, 6, 1, 1) \
    X(ABI_SEXT_GP6_32, 1, 6, 4, 1) \
    X(ABI_SEXT_GP6_32_A, 1, 6, 4, 4) \
    X(ABI_ZEXT_GP6_32, 1, 6, 4, 1) \
    X(ABI_ZEXT_GP6_32_A, 1, 6, 4, 4) \
    X(ABI_READ_GP7_8, 0, 7, 1, 1) \
    X(ABI_WRITE_GP7_8, 1, 7, 1, 1) \
    X(ABI_READ_GP7_32, 0, 7, 4, 1) \
    X(ABI_READ_GP7_32_A, 0, 7, 4, 4) \
    X(ABI_WRITE_GP7_32, 1, 7, 4, 1) \
    X(ABI_WRITE_GP7_32_A, 1, 7, 4, 4) \
    X(ABI_SEXT_GP7_8, 1, 7, 1, 1) \
    X(ABI_ZEXT_GP7_8, 1, 7, 1, 1) \
    X(ABI_SEXT_GP7_32, 1, 7, 4, 1) \
    X(ABI_SEXT_GP7_32_A, 1, 7, 4, 4) \
    X(ABI_ZEXT_GP7_32, 1, 7, 4, 1) \
    X(ABI_ZEXT_GP7_32_A, 1, 7, 4, 4) \
    X(ABI_READ_FP0_32, 0, 0, 4, 1) \
    X(ABI_READ_FP0_32_A, 0, 0, 4, 4) \
    X(ABI_WRITE_FP0_32, 1, 0, 4, 1) \
    X(ABI_WRITE_FP0_32_A, 1, 0, 4, 4) \
    X(ABI_READ_FP0_64, 0, 0, 8, 1) \
    X(ABI_READ_FP0_64_A, 0, 0, 8, 8) \
    X(ABI_WRITE_FP0_64, 1, 0, 8, 1) \
    X(ABI_WRITE_FP0_64_A, 1, 0, 8, 8) \
    X(ABI_READ_FP0_128, 0, 0, 16, 1) \
    X(ABI_READ_FP0_128_A, 0, 0, 16, 16) \
    X(ABI_WRITE_FP0_128, 1, 0, 16, 1) \
    X(ABI_WRITE_FP0_128_A, 1, 0, 16, 16) \
    X(ABI_READ_FP1_32, 0, 1, 4, 1) \
    X(ABI_READ_FP1_32_A, 0, 1, 4, 4) \
    X(ABI_WRITE_FP1_32, 1, 1, 4, 1) \
    X(ABI_WRITE_FP1_32_A, 1, 1, 4, 4) \
    X(ABI_READ_FP1_64, 0, 1, 8, 1) \
    X(ABI_READ_FP1_64_A, 0, 1, 8, 8) \
    X(ABI_WRITE_FP1_64, 1, 1, 8, 1) \
    X(ABI_WRITE_FP1_64_A, 1, 1, 8, 8) \
    X(ABI_READ_FP1_128, 0, 1, 16, 1) \
    X(ABI_READ_FP1_128_A, 0, 1, 16, 16) \
    X(ABI_WRITE_FP1_128, 1, 1, 16, 1) \
    X(ABI_WRITE_FP1_128_A, 1, 1, 16, 16) \
    X(ABI_READ_FP2_32, 0, 2, 4, 1) \
    X(ABI_READ_FP2_32_A, 0, 2, 4, 4) \
    X(ABI_WRITE_FP2_32, 1, 2, 4, 1) \
    X(ABI_WRITE_FP2_32_A, 1, 2, 4, 4) \
    X(ABI_READ_FP2_64, 0, 2, 8, 1) \
    X(ABI_READ_FP2_64_A, 0, 2, 8, 8) \
    X(ABI_WRITE_FP2_64, 1, 2, 8, 1) \
    X(ABI_WRITE_FP2_64_A, 1, 2, 8, 8) \
    X(ABI_READ_FP2_128, 0, 2, 16, 1) \
    X(ABI_READ_FP2_128_A, 0, 2, 16, 16) \
    X(ABI_WRITE_FP2_128, 1, 2, 16, 1) \
    X(ABI_WRITE_FP2_128_A, 1, 2, 16, 16) \
    X(ABI_READ_FP3_32, 0, 3, 4, 1) \
    X(ABI_READ_FP3_32_A, 0, 3, 4, 4) \
    X(ABI_WRITE_FP3_32, 1, 3, 4, 1) \
    X(ABI_WRITE_FP3_32_A, 1, 3, 4, 4) \
    X(ABI_READ_FP3_64, 0, 3, 8, 1) \
    X(ABI_READ_FP3_64_A, 0, 3, 8, 8) \
    X(ABI_WRITE_FP3_64, 1, 3, 8, 1) \
    X(ABI_WRITE_FP3_64_A, 1, 3, 8, 8) \
    X(ABI_READ_FP3_128, 0, 3, 16, 1) \
    X(ABI_READ_FP3_128_A, 0, 3, 16, 16) \
    X(ABI_WRITE_FP3_128, 1, 3, 16, 1) \
    X(ABI_WRITE_FP3_128_A, 1, 3, 16, 16) \
    X(ABI_READ_FP4_32, 0, 4, 4, 1) \
    X(ABI_READ_FP4_32_A, 0, 4, 4, 4) \
    X(ABI_WRITE_FP4_32, 1, 4, 4, 1) \
    X(ABI_WRITE_FP4_32_A, 1, 4, 4, 4) \
    X(ABI_READ_FP4_64, 0, 4, 8, 1) \
    X(ABI_READ_FP4_64_A, 0, 4, 8, 8) \
    X(ABI_WRITE_FP4_64, 1, 4, 8, 1) \
    X(ABI_WRITE_FP4_64_A, 1, 4, 8, 8) \
    X(ABI_READ_FP4_128, 0, 4, 16, 1) \
    X(ABI_READ_FP4_128_A, 0, 4, 16, 16) \
    X(ABI_WRITE_FP4_128, 1, 4, 16, 1) \
    X(ABI_WRITE_FP4_128_A, 1, 4, 16, 16) \
    X(ABI_READ_FP5_32, 0, 5, 4, 1) \
    X(ABI_READ_FP5_32_A, 0, 5, 4, 4) \
    X(ABI_WRITE_FP5_32, 1, 5, 4, 1) \
    X(ABI_WRITE_FP5_32_A, 1, 5, 4, 4) \
    X(ABI_READ_FP5_64, 0, 5, 8, 1) \
    X(ABI_READ_FP5_64_A, 0, 5, 8, 8) \
    X(ABI_WRITE_FP5_64, 1, 5, 8, 1) \
    X(ABI_WRITE_FP5_64_A, 1, 5, 8, 8) \
    X(ABI_READ_FP5_128, 0, 5, 16, 1) \
    X(ABI_READ_FP5_128_A, 0, 5, 16, 16) \
    X(ABI_WRITE_FP5_128, 1, 5, 16, 1) \
    X(ABI_WRITE_FP5_128_A, 1, 5, 16, 16) \
    X(ABI_READ_FP6_32, 0, 6, 4, 1) \
    X(ABI_READ_FP6_32_A, 0, 6, 4, 4) \
    X(ABI_WRITE_FP6_32, 1, 6, 4, 1) \
    X(ABI_WRITE_FP6_32_A, 1, 6, 4, 4) \
    X(ABI_READ_FP6_64, 0, 6, 8, 1) \
    X(ABI_READ_FP6_64_A, 0, 6, 8, 8) \
    X(ABI_WRITE_FP6_64, 1, 6, 8, 1) \
    X(ABI_WRITE_FP6_64_A, 1, 6, 8, 8) \
    X(ABI_READ_FP6_128, 0, 6, 16, 1) \
    X(ABI_READ_FP6_128_A, 0, 6, 16, 16) \
    X(ABI_WRITE_FP6_128, 1, 6, 16, 1) \
    X(ABI_WRITE_FP6_128_A, 1, 6, 16, 16) \
    X(ABI_READ_FP7_32, 0, 7, 4, 1) \
    X(ABI_READ_FP7_32_A, 0, 7, 4, 4) \
    X(ABI_WRITE_FP7_32, 1, 7, 4, 1) \
    X(ABI_WRITE_FP7_32_A, 1, 7, 4, 4) \
    X(ABI_READ_FP7_64, 0, 7, 8, 1) \
    X(ABI_READ_FP7_64_A, 0, 7, 8, 8) \
    X(ABI_WRITE_FP7_64, 1, 7, 8, 1) \
    X(ABI_WRITE_FP7_64_A, 1, 7, 8, 8) \
    X(ABI_READ_FP7_128, 0, 7, 16, 1) \
    X(ABI_READ_FP7_128_A, 0, 7, 16, 16) \
    X(ABI_WRITE_FP7_128, 1, 7, 16, 1) \
    X(ABI_WRITE_FP7_128_A, 1, 7, 16, 16)

#define CI_LOAD_SLOT_BITFIELD_OPS(X) \
    X(LDBFS_U8, 1, 0, 1) \
    X(LDBFS_S8, 1, 1, 1) \
    X(LDBFS_U16, 2, 0, 1) \
    X(LDBFS_U16_A, 2, 0, 2) \
    X(LDBFS_S16, 2, 1, 1) \
    X(LDBFS_S16_A, 2, 1, 2) \
    X(LDBFS_U32, 4, 0, 1) \
    X(LDBFS_U32_A, 4, 0, 4) \
    X(LDBFS_S32, 4, 1, 1) \
    X(LDBFS_S32_A, 4, 1, 4) \
    X(LDBFS_U64, 8, 0, 1) \
    X(LDBFS_U64_A, 8, 0, 8) \
    X(LDBFS_S64, 8, 1, 1) \
    X(LDBFS_S64_A, 8, 1, 8)

#define CI_LOAD_SLOT_BITFIELD_128_OPS(X) \
    X(LDBFS_U128, 1, 1) \
    X(LDBFS_U128_A, 1, 16) \
    X(LDBFS_S128, 0, 1) \
    X(LDBFS_S128_A, 0, 16)

#define CI_LOAD_SLOT_BITFIELD_BYTES_OPS(X) \
    X(LDBFBS_U8, 1, 0) \
    X(LDBFBS_S8, 1, 1) \
    X(LDBFBS_U16, 2, 0) \
    X(LDBFBS_S16, 2, 1) \
    X(LDBFBS_U32, 4, 0) \
    X(LDBFBS_S32, 4, 1) \
    X(LDBFBS_U64, 8, 0) \
    X(LDBFBS_S64, 8, 1)

#define CI_LOAD_SLOT_BITFIELD_BYTES_128_OPS(X) \
    X(LDBFBS_U128, 1) \
    X(LDBFBS_S128, 0)

#define CI_BITFIELD_JUMP_OPS(X) \
    X(BF_JZ8, 1, 0, 0) \
    X(BF_JNZ8, 1, 0, 1) \
    X(BF_JZ16, 2, 0, 0) \
    X(BF_JNZ16, 2, 0, 1) \
    X(BF_JZ32, 4, 0, 0) \
    X(BF_JNZ32, 4, 0, 1) \
    X(BF_JZ64, 8, 0, 0) \
    X(BF_JNZ64, 8, 0, 1) \
    X(BF_JZ_BYTES, 0, 0, 0) \
    X(BF_JNZ_BYTES, 0, 0, 1) \
    X(BFS_JZ8, 1, 1, 0) \
    X(BFS_JNZ8, 1, 1, 1) \
    X(BFS_JZ16, 2, 1, 0) \
    X(BFS_JNZ16, 2, 1, 1) \
    X(BFS_JZ32, 4, 1, 0) \
    X(BFS_JNZ32, 4, 1, 1) \
    X(BFS_JZ64, 8, 1, 0) \
    X(BFS_JNZ64, 8, 1, 1) \
    X(BFS_JZ_BYTES, 0, 1, 0) \
    X(BFS_JNZ_BYTES, 0, 1, 1)

#define CI_ABI_PAIR_OPS(X) \
    X(ABI_READ_GP01_64, 0, 0, 1) \
    X(ABI_READ_GP01_64_A, 0, 0, 8) \
    X(ABI_READ_GP01_64_CONTIG, 0, 1, 1) \
    X(ABI_READ_GP01_64_CONTIG_A, 0, 1, 8) \
    X(ABI_WRITE_GP01_64, 1, 0, 1) \
    X(ABI_WRITE_GP01_64_A, 1, 0, 8) \
    X(ABI_WRITE_GP01_64_CONTIG, 1, 1, 1) \
    X(ABI_WRITE_GP01_64_CONTIG_A, 1, 1, 8)

#endif
