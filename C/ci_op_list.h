#ifndef C_CI_OP_LIST_H
#define C_CI_OP_LIST_H

#define CI_CONST_OPS(X) \
    X(CONST8, 1, 1, 1) \
    X(CONST16, 2, 1, 1) \
    X(CONST32, 4, 1, 1) \
    X(CONST64, 8, 1, 1) \
    X(CONST128, 16, 1, 1) \
    X(CONST_N, op->constant.immsize, 1, 1)

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
    X(LOAD_N, op->load.slot_size, 1, 1) \
    X(LOAD_N_A2, op->load.slot_size, 2, 2) \
    X(LOAD_N_A4, op->load.slot_size, 4, 4) \
    X(LOAD_N_A8, op->load.slot_size, 8, 8) \
    X(LOAD_N_A16, op->load.slot_size, 16, 16)

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
    X(STORE_N, op->store.src_size, 1, 1) \
    X(STORE_N_A2, op->store.src_size, 2, 2) \
    X(STORE_N_A4, op->store.src_size, 4, 4) \
    X(STORE_N_A8, op->store.src_size, 8, 8) \
    X(STORE_N_A16, op->store.src_size, 16, 16)

#define CI_STORE_IMM_OPS(X) \
    X(STORE_IMM8, 1, 1, 1) \
    X(STORE_IMM16_A, 2, 2, 1) \
    X(STORE_IMM16_U, 2, 1, 1) \
    X(STORE_IMM32_A, 4, 4, 1) \
    X(STORE_IMM32_U, 4, 1, 1) \
    X(STORE_IMM64_A, 8, 8, 1) \
    X(STORE_IMM64_U, 8, 1, 1) \
    X(STORE_IMM_N, op->store_imm.size, 1, 1) \
    X(STORE_IMM_N_A2, op->store_imm.size, 2, 1) \
    X(STORE_IMM_N_A4, op->store_imm.size, 4, 1) \
    X(STORE_IMM_N_A8, op->store_imm.size, 8, 1) \
    X(STORE_IMM_N_A16, op->store_imm.size, 16, 1)

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
    X(ZERO_N, op->zero.size, 1, 1) \
    X(ZERO_N_A2, op->zero.size, 2, 1) \
    X(ZERO_N_A4, op->zero.size, 4, 1) \
    X(ZERO_N_A8, op->zero.size, 8, 1) \
    X(ZERO_N_A16, op->zero.size, 16, 1)

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
    X(COPY_N, op->copy.slot_size, 1, 1) \
    X(COPY_N_A2, op->copy.slot_size, 2, 2) \
    X(COPY_N_A4, op->copy.slot_size, 4, 4) \
    X(COPY_N_A8, op->copy.slot_size, 8, 8) \
    X(COPY_N_A16, op->copy.slot_size, 16, 16)

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
    X(MEMCOPY_N, op->memcopy.size, 1, 1) \
    X(MEMCOPY_N_A2, op->memcopy.size, 2, 2) \
    X(MEMCOPY_N_A4, op->memcopy.size, 4, 4) \
    X(MEMCOPY_N_A8, op->memcopy.size, 8, 8) \
    X(MEMCOPY_N_A16, op->memcopy.size, 16, 16) \
    X(MEMCOPY_VAR, ci_read_uint((char*)frame->slots + op->memcopy.size, sizeof(size_t)), 1, 1) \
    X(MEMCOPY_VAR_A2, ci_read_uint((char*)frame->slots + op->memcopy.size, sizeof(size_t)), 2, 2) \
    X(MEMCOPY_VAR_A4, ci_read_uint((char*)frame->slots + op->memcopy.size, sizeof(size_t)), 4, 4) \
    X(MEMCOPY_VAR_A8, ci_read_uint((char*)frame->slots + op->memcopy.size, sizeof(size_t)), 8, 8) \
    X(MEMCOPY_VAR_A16, ci_read_uint((char*)frame->slots + op->memcopy.size, sizeof(size_t)), 16, 16)

#define CI_EXTEND_OPS(X) \
    X(SEXT8_16, 1, 2, ci_uint128_from_int64(ci_read_int(src, 1))) \
    X(ZEXT8_16, 1, 2, ci_uint128_from_uint64(ci_read_uint(src, 1))) \
    X(SEXT8_32, 1, 4, ci_uint128_from_int64(ci_read_int(src, 1))) \
    X(ZEXT8_32, 1, 4, ci_uint128_from_uint64(ci_read_uint(src, 1))) \
    X(SEXT8_64, 1, 8, ci_uint128_from_int64(ci_read_int(src, 1))) \
    X(ZEXT8_64, 1, 8, ci_uint128_from_uint64(ci_read_uint(src, 1))) \
    X(SEXT8_128, 1, 16, ci_uint128_from_int64(ci_read_int(src, 1))) \
    X(ZEXT8_128, 1, 16, ci_uint128_from_uint64(ci_read_uint(src, 1))) \
    X(SEXT16_32, 2, 4, ci_uint128_from_int64(ci_read_int(src, 2))) \
    X(ZEXT16_32, 2, 4, ci_uint128_from_uint64(ci_read_uint(src, 2))) \
    X(SEXT16_64, 2, 8, ci_uint128_from_int64(ci_read_int(src, 2))) \
    X(ZEXT16_64, 2, 8, ci_uint128_from_uint64(ci_read_uint(src, 2))) \
    X(SEXT16_128, 2, 16, ci_uint128_from_int64(ci_read_int(src, 2))) \
    X(ZEXT16_128, 2, 16, ci_uint128_from_uint64(ci_read_uint(src, 2))) \
    X(SEXT32_64, 4, 8, ci_uint128_from_int64(ci_read_int(src, 4))) \
    X(ZEXT32_64, 4, 8, ci_uint128_from_uint64(ci_read_uint(src, 4))) \
    X(SEXT32_128, 4, 16, ci_uint128_from_int64(ci_read_int(src, 4))) \
    X(ZEXT32_128, 4, 16, ci_uint128_from_uint64(ci_read_uint(src, 4))) \
    X(SEXT64_128, 8, 16, ci_uint128_from_int64(ci_read_int(src, 8))) \
    X(ZEXT64_128, 8, 16, ci_uint128_from_uint64(ci_read_uint(src, 8)))

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
    X(ISTRUE_I8, ci_read_uint(src, 1) != 0) \
    X(ISFALSE_I8, !(ci_read_uint(src, 1) != 0)) \
    X(ISTRUE_I16, ci_read_uint(src, 2) != 0) \
    X(ISFALSE_I16, !(ci_read_uint(src, 2) != 0)) \
    X(ISTRUE_I32, ci_read_uint(src, 4) != 0) \
    X(ISFALSE_I32, !(ci_read_uint(src, 4) != 0)) \
    X(ISTRUE_I64, ci_read_uint(src, 8) != 0) \
    X(ISFALSE_I64, !(ci_read_uint(src, 8) != 0)) \
    X(ISTRUE_I128, ci_uint128_nonzero(ci_read_u128(src))) \
    X(ISFALSE_I128, !(ci_uint128_nonzero(ci_read_u128(src)))) \
    X(ISTRUE_F16, ci_narrow_float_nonzero((uint16_t)ci_read_uint(src, 2))) \
    X(ISFALSE_F16, !(ci_narrow_float_nonzero((uint16_t)ci_read_uint(src, 2)))) \
    X(ISTRUE_F32, ci_read_f32(src) != 0.0f) \
    X(ISFALSE_F32, !(ci_read_f32(src) != 0.0f)) \
    X(ISTRUE_F64, ci_read_f64(src) != 0.0) \
    X(ISFALSE_F64, !(ci_read_f64(src) != 0.0)) \
    X(ISTRUE_F80, ci_float80_nonzero(ci_float80_read(src))) \
    X(ISFALSE_F80, !(ci_float80_nonzero(ci_float80_read(src)))) \
    X(ISTRUE_F128, ci_float128_nonzero(ci_float128_read(src))) \
    X(ISFALSE_F128, !(ci_float128_nonzero(ci_float128_read(src))))

#define CI_ITOF_OPS(X) \
    X(S32_TO_F16, uint16_t, ci_float16_from_int32(( int32_t)ci_read_int(src, 4), 0), memcpy(dest, &result, 2)) \
    X(U32_TO_F16, uint16_t, ci_float16_from_uint32(( uint32_t)ci_read_uint(src, 4), 0), memcpy(dest, &result, 2)) \
    X(S64_TO_F16, uint16_t, ci_float16_from_int64(( int64_t)ci_read_int(src, 8), 0), memcpy(dest, &result, 2)) \
    X(U64_TO_F16, uint16_t, ci_float16_from_uint64(( uint64_t)ci_read_uint(src, 8), 0), memcpy(dest, &result, 2)) \
    X(S128_TO_F16, uint16_t, ci_float16_from_uint128(ci_read_u128(src), 0, 0), memcpy(dest, &result, 2)) \
    X(U128_TO_F16, uint16_t, ci_float16_from_uint128(ci_read_u128(src), 1, 0), memcpy(dest, &result, 2)) \
    X(S32_TO_BF16, uint16_t, ci_float16_from_int32(( int32_t)ci_read_int(src, 4), 1), memcpy(dest, &result, 2)) \
    X(U32_TO_BF16, uint16_t, ci_float16_from_uint32(( uint32_t)ci_read_uint(src, 4), 1), memcpy(dest, &result, 2)) \
    X(S64_TO_BF16, uint16_t, ci_float16_from_int64(( int64_t)ci_read_int(src, 8), 1), memcpy(dest, &result, 2)) \
    X(U64_TO_BF16, uint16_t, ci_float16_from_uint64(( uint64_t)ci_read_uint(src, 8), 1), memcpy(dest, &result, 2)) \
    X(S128_TO_BF16, uint16_t, ci_float16_from_uint128(ci_read_u128(src), 0, 1), memcpy(dest, &result, 2)) \
    X(U128_TO_BF16, uint16_t, ci_float16_from_uint128(ci_read_u128(src), 1, 1), memcpy(dest, &result, 2)) \
    X(S32_TO_F32, float, (float)ci_read_int(src, 4), memcpy(dest, &result, 4)) \
    X(U32_TO_F32, float, (float)ci_read_uint(src, 4), memcpy(dest, &result, 4)) \
    X(S64_TO_F32, float, (float)ci_read_int(src, 8), memcpy(dest, &result, 4)) \
    X(U64_TO_F32, float, (float)ci_read_uint(src, 8), memcpy(dest, &result, 4)) \
    X(S128_TO_F32, float, ci_uint128_to_float(ci_read_u128(src), 0), memcpy(dest, &result, 4)) \
    X(U128_TO_F32, float, ci_uint128_to_float(ci_read_u128(src), 1), memcpy(dest, &result, 4)) \
    X(S32_TO_F64, double, (double)ci_read_int(src, 4), memcpy(dest, &result, 8)) \
    X(U32_TO_F64, double, (double)ci_read_uint(src, 4), memcpy(dest, &result, 8)) \
    X(S64_TO_F64, double, (double)ci_read_int(src, 8), memcpy(dest, &result, 8)) \
    X(U64_TO_F64, double, (double)ci_read_uint(src, 8), memcpy(dest, &result, 8)) \
    X(S128_TO_F64, double, ci_uint128_to_double(ci_read_u128(src), 0), memcpy(dest, &result, 8)) \
    X(U128_TO_F64, double, ci_uint128_to_double(ci_read_u128(src), 1), memcpy(dest, &result, 8)) \
    X(S32_TO_F80, CiFloat80, ci_float80_from_uint128(ci_uint128_from_int64(ci_read_int(src, 4)), 0), ci_float80_write(dest, op->convert.slot_size, result)) \
    X(U32_TO_F80, CiFloat80, ci_float80_from_uint128(ci_uint128_from_uint64(ci_read_uint(src, 4)), 1), ci_float80_write(dest, op->convert.slot_size, result)) \
    X(S64_TO_F80, CiFloat80, ci_float80_from_uint128(ci_uint128_from_int64(ci_read_int(src, 8)), 0), ci_float80_write(dest, op->convert.slot_size, result)) \
    X(U64_TO_F80, CiFloat80, ci_float80_from_uint128(ci_uint128_from_uint64(ci_read_uint(src, 8)), 1), ci_float80_write(dest, op->convert.slot_size, result)) \
    X(S128_TO_F80, CiFloat80, ci_float80_from_uint128(ci_read_u128(src), 0), ci_float80_write(dest, op->convert.slot_size, result)) \
    X(U128_TO_F80, CiFloat80, ci_float80_from_uint128(ci_read_u128(src), 1), ci_float80_write(dest, op->convert.slot_size, result)) \
    X(S32_TO_F128, CiFloat128, ci_float128_from_uint128(ci_uint128_from_int64(ci_read_int(src, 4)), 0), ci_float128_write(dest, 16, result)) \
    X(U32_TO_F128, CiFloat128, ci_float128_from_uint128(ci_uint128_from_uint64(ci_read_uint(src, 4)), 1), ci_float128_write(dest, 16, result)) \
    X(S64_TO_F128, CiFloat128, ci_float128_from_uint128(ci_uint128_from_int64(ci_read_int(src, 8)), 0), ci_float128_write(dest, 16, result)) \
    X(U64_TO_F128, CiFloat128, ci_float128_from_uint128(ci_uint128_from_uint64(ci_read_uint(src, 8)), 1), ci_float128_write(dest, 16, result)) \
    X(S128_TO_F128, CiFloat128, ci_float128_from_uint128(ci_read_u128(src), 0), ci_float128_write(dest, 16, result)) \
    X(U128_TO_F128, CiFloat128, ci_float128_from_uint128(ci_read_u128(src), 1), ci_float128_write(dest, 16, result))

#define CI_FTOI_OPS(X) \
    X(F16_TO_S32, 4, ci_uint128_from_uint64((uint64_t)ci_float16_to_int32((uint16_t)ci_read_uint(src, 2), 0))) \
    X(F16_TO_U32, 4, ci_uint128_from_uint64((uint64_t)ci_float16_to_uint32((uint16_t)ci_read_uint(src, 2), 0))) \
    X(F16_TO_S64, 8, ci_uint128_from_uint64((uint64_t)ci_float16_to_int64((uint16_t)ci_read_uint(src, 2), 0))) \
    X(F16_TO_U64, 8, ci_uint128_from_uint64((uint64_t)ci_float16_to_uint64((uint16_t)ci_read_uint(src, 2), 0))) \
    X(F16_TO_I128, 16, ci_float16_to_uint128((uint16_t)ci_read_uint(src, 2), 0)) \
    X(BF16_TO_S32, 4, ci_uint128_from_uint64((uint64_t)ci_float16_to_int32((uint16_t)ci_read_uint(src, 2), 1))) \
    X(BF16_TO_U32, 4, ci_uint128_from_uint64((uint64_t)ci_float16_to_uint32((uint16_t)ci_read_uint(src, 2), 1))) \
    X(BF16_TO_S64, 8, ci_uint128_from_uint64((uint64_t)ci_float16_to_int64((uint16_t)ci_read_uint(src, 2), 1))) \
    X(BF16_TO_U64, 8, ci_uint128_from_uint64((uint64_t)ci_float16_to_uint64((uint16_t)ci_read_uint(src, 2), 1))) \
    X(BF16_TO_I128, 16, ci_float16_to_uint128((uint16_t)ci_read_uint(src, 2), 1)) \
    X(F32_TO_S32, 4, ci_uint128_from_uint64(ci_uint64_from_double((double)ci_read_f32(src), 0))) \
    X(F32_TO_U32, 4, ci_uint128_from_uint64(ci_uint64_from_double((double)ci_read_f32(src), 1))) \
    X(F32_TO_S64, 8, ci_uint128_from_uint64(ci_uint64_from_double((double)ci_read_f32(src), 0))) \
    X(F32_TO_U64, 8, ci_uint128_from_uint64(ci_uint64_from_double((double)ci_read_f32(src), 1))) \
    X(F32_TO_S128, 16, ci_uint128_from_double((double)ci_read_f32(src), 0)) \
    X(F32_TO_U128, 16, ci_uint128_from_double((double)ci_read_f32(src), 1)) \
    X(F64_TO_S32, 4, ci_uint128_from_uint64(ci_uint64_from_double(ci_read_f64(src), 0))) \
    X(F64_TO_U32, 4, ci_uint128_from_uint64(ci_uint64_from_double(ci_read_f64(src), 1))) \
    X(F64_TO_S64, 8, ci_uint128_from_uint64(ci_uint64_from_double(ci_read_f64(src), 0))) \
    X(F64_TO_U64, 8, ci_uint128_from_uint64(ci_uint64_from_double(ci_read_f64(src), 1))) \
    X(F64_TO_S128, 16, ci_uint128_from_double(ci_read_f64(src), 0)) \
    X(F64_TO_U128, 16, ci_uint128_from_double(ci_read_f64(src), 1)) \
    X(F80_TO_I8, 1, ci_float80_to_uint128(ci_float80_read(src))) \
    X(F80_TO_I16, 2, ci_float80_to_uint128(ci_float80_read(src))) \
    X(F80_TO_I32, 4, ci_float80_to_uint128(ci_float80_read(src))) \
    X(F80_TO_I64, 8, ci_float80_to_uint128(ci_float80_read(src))) \
    X(F80_TO_I128, 16, ci_float80_to_uint128(ci_float80_read(src))) \
    X(F128_TO_I8, 1, ci_float128_to_uint128(ci_float128_read(src))) \
    X(F128_TO_I16, 2, ci_float128_to_uint128(ci_float128_read(src))) \
    X(F128_TO_I32, 4, ci_float128_to_uint128(ci_float128_read(src))) \
    X(F128_TO_I64, 8, ci_float128_to_uint128(ci_float128_read(src))) \
    X(F128_TO_I128, 16, ci_float128_to_uint128(ci_float128_read(src)))

#define CI_FTOF_OPS(X) \
    X(F16_TO_BF16, uint16_t, ci_float16_from_float(ci_float16_to_float((uint16_t)ci_read_uint(src, 2), 0), 1), memcpy(dest, &result, 2)) \
    X(F16_TO_F32, float, ci_float16_to_float((uint16_t)ci_read_uint(src, 2), 0), memcpy(dest, &result, 4)) \
    X(F16_TO_F64, double, ci_float16_to_double((uint16_t)ci_read_uint(src, 2), 0), memcpy(dest, &result, 8)) \
    X(F16_TO_F80, CiFloat80, ci_float16_to_float80((uint16_t)ci_read_uint(src, 2), 0), ci_float80_write(dest, op->convert.slot_size, result)) \
    X(F16_TO_F128, CiFloat128, ci_float16_to_quad((uint16_t)ci_read_uint(src, 2), 0), ci_float128_write(dest, 16, result)) \
    X(BF16_TO_F16, uint16_t, ci_float16_from_float(ci_float16_to_float((uint16_t)ci_read_uint(src, 2), 1), 0), memcpy(dest, &result, 2)) \
    X(BF16_TO_F32, float, ci_float16_to_float((uint16_t)ci_read_uint(src, 2), 1), memcpy(dest, &result, 4)) \
    X(BF16_TO_F64, double, ci_float16_to_double((uint16_t)ci_read_uint(src, 2), 1), memcpy(dest, &result, 8)) \
    X(BF16_TO_F80, CiFloat80, ci_float16_to_float80((uint16_t)ci_read_uint(src, 2), 1), ci_float80_write(dest, op->convert.slot_size, result)) \
    X(BF16_TO_F128, CiFloat128, ci_float16_to_quad((uint16_t)ci_read_uint(src, 2), 1), ci_float128_write(dest, 16, result)) \
    X(F32_TO_F16, uint16_t, ci_float16_from_float(ci_read_f32(src), 0), memcpy(dest, &result, 2)) \
    X(F32_TO_BF16, uint16_t, ci_float16_from_float(ci_read_f32(src), 1), memcpy(dest, &result, 2)) \
    X(F32_TO_F64, double, (double)ci_read_f32(src), memcpy(dest, &result, 8)) \
    X(F32_TO_F80, CiFloat80, ci_float80_from_float(ci_read_f32(src)), ci_float80_write(dest, op->convert.slot_size, result)) \
    X(F32_TO_F128, CiFloat128, ci_float128_from_float(ci_read_f32(src)), ci_float128_write(dest, 16, result)) \
    X(F64_TO_F16, uint16_t, ci_float16_from_double(ci_read_f64(src), 0), memcpy(dest, &result, 2)) \
    X(F64_TO_BF16, uint16_t, ci_float16_from_double(ci_read_f64(src), 1), memcpy(dest, &result, 2)) \
    X(F64_TO_F32, float, (float)ci_read_f64(src), memcpy(dest, &result, 4)) \
    X(F64_TO_F80, CiFloat80, ci_float80_from_double(ci_read_f64(src)), ci_float80_write(dest, op->convert.slot_size, result)) \
    X(F64_TO_F128, CiFloat128, ci_float128_from_double(ci_read_f64(src)), ci_float128_write(dest, 16, result)) \
    X(F80_TO_F16, uint16_t, ci_float16_from_float80(ci_float80_read(src), 0), memcpy(dest, &result, 2)) \
    X(F80_TO_BF16, uint16_t, ci_float16_from_float80(ci_float80_read(src), 1), memcpy(dest, &result, 2)) \
    X(F80_TO_F32, float, ci_float80_to_float(ci_float80_read(src)), memcpy(dest, &result, 4)) \
    X(F80_TO_F64, double, ci_float80_to_double(ci_float80_read(src)), memcpy(dest, &result, 8)) \
    X(F80_TO_F128, CiFloat128, ci_float128_from_float80(ci_float80_read(src)), ci_float128_write(dest, 16, result)) \
    X(F128_TO_F16, uint16_t, ci_float16_from_quad(ci_float128_read(src), 0), memcpy(dest, &result, 2)) \
    X(F128_TO_BF16, uint16_t, ci_float16_from_quad(ci_float128_read(src), 1), memcpy(dest, &result, 2)) \
    X(F128_TO_F32, float, ci_float128_to_float(ci_float128_read(src)), memcpy(dest, &result, 4)) \
    X(F128_TO_F64, double, ci_float128_to_double(ci_float128_read(src)), memcpy(dest, &result, 8)) \
    X(F128_TO_F80, CiFloat80, ci_float80_from_float128(ci_float128_read(src)), ci_float80_write(dest, op->convert.slot_size, result))

#define CI_ABI_MOVE_OPS(X) \
    X(ABI_READ_IN8, frame->incoming, 1, 1, memcpy(slot, reg, 1)) \
    X(ABI_READ_IN16, frame->incoming, 2, 1, memcpy(slot, reg, 2)) \
    X(ABI_READ_IN16_A, frame->incoming, 2, 2, memcpy(slot, reg, 2)) \
    X(ABI_READ_IN32, frame->incoming, 4, 1, memcpy(slot, reg, 4)) \
    X(ABI_READ_IN32_A, frame->incoming, 4, 4, memcpy(slot, reg, 4)) \
    X(ABI_READ_IN64, frame->incoming, 8, 1, memcpy(slot, reg, 8)) \
    X(ABI_READ_IN64_A, frame->incoming, 8, 8, memcpy(slot, reg, 8)) \
    X(ABI_READ_IN128, frame->incoming, 16, 1, memcpy(slot, reg, 16)) \
    X(ABI_READ_IN128_A, frame->incoming, 16, 16, memcpy(slot, reg, 16)) \
    X(ABI_READ_IN_N, frame->incoming, op->abi_move.size, 1, memcpy(slot, reg, op->abi_move.size)) \
    X(ABI_READ_IN_N_A2, frame->incoming, op->abi_move.size, 2, memcpy(slot, reg, op->abi_move.size)) \
    X(ABI_READ_IN_N_A4, frame->incoming, op->abi_move.size, 4, memcpy(slot, reg, op->abi_move.size)) \
    X(ABI_READ_IN_N_A8, frame->incoming, op->abi_move.size, 8, memcpy(slot, reg, op->abi_move.size)) \
    X(ABI_READ_IN_N_A16, frame->incoming, op->abi_move.size, 16, memcpy(slot, reg, op->abi_move.size)) \
    X(ABI_READ_OUT8, &frame->stack->registers, 1, 1, memcpy(slot, reg, 1)) \
    X(ABI_READ_OUT16, &frame->stack->registers, 2, 1, memcpy(slot, reg, 2)) \
    X(ABI_READ_OUT16_A, &frame->stack->registers, 2, 2, memcpy(slot, reg, 2)) \
    X(ABI_READ_OUT32, &frame->stack->registers, 4, 1, memcpy(slot, reg, 4)) \
    X(ABI_READ_OUT32_A, &frame->stack->registers, 4, 4, memcpy(slot, reg, 4)) \
    X(ABI_READ_OUT64, &frame->stack->registers, 8, 1, memcpy(slot, reg, 8)) \
    X(ABI_READ_OUT64_A, &frame->stack->registers, 8, 8, memcpy(slot, reg, 8)) \
    X(ABI_READ_OUT128, &frame->stack->registers, 16, 1, memcpy(slot, reg, 16)) \
    X(ABI_READ_OUT128_A, &frame->stack->registers, 16, 16, memcpy(slot, reg, 16)) \
    X(ABI_READ_OUT_N, &frame->stack->registers, op->abi_move.size, 1, memcpy(slot, reg, op->abi_move.size)) \
    X(ABI_READ_OUT_N_A2, &frame->stack->registers, op->abi_move.size, 2, memcpy(slot, reg, op->abi_move.size)) \
    X(ABI_READ_OUT_N_A4, &frame->stack->registers, op->abi_move.size, 4, memcpy(slot, reg, op->abi_move.size)) \
    X(ABI_READ_OUT_N_A8, &frame->stack->registers, op->abi_move.size, 8, memcpy(slot, reg, op->abi_move.size)) \
    X(ABI_READ_OUT_N_A16, &frame->stack->registers, op->abi_move.size, 16, memcpy(slot, reg, op->abi_move.size)) \
    X(ABI_WRITE_OUT8, &frame->stack->registers, 1, 1, memcpy(reg, slot, 1)) \
    X(ABI_WRITE_OUT16, &frame->stack->registers, 2, 1, memcpy(reg, slot, 2)) \
    X(ABI_WRITE_OUT16_A, &frame->stack->registers, 2, 2, memcpy(reg, slot, 2)) \
    X(ABI_WRITE_OUT32, &frame->stack->registers, 4, 1, memcpy(reg, slot, 4)) \
    X(ABI_WRITE_OUT32_A, &frame->stack->registers, 4, 4, memcpy(reg, slot, 4)) \
    X(ABI_WRITE_OUT64, &frame->stack->registers, 8, 1, memcpy(reg, slot, 8)) \
    X(ABI_WRITE_OUT64_A, &frame->stack->registers, 8, 8, memcpy(reg, slot, 8)) \
    X(ABI_WRITE_OUT128, &frame->stack->registers, 16, 1, memcpy(reg, slot, 16)) \
    X(ABI_WRITE_OUT128_A, &frame->stack->registers, 16, 16, memcpy(reg, slot, 16)) \
    X(ABI_WRITE_OUT_N, &frame->stack->registers, op->abi_move.size, 1, memcpy(reg, slot, op->abi_move.size)) \
    X(ABI_WRITE_OUT_N_A2, &frame->stack->registers, op->abi_move.size, 2, memcpy(reg, slot, op->abi_move.size)) \
    X(ABI_WRITE_OUT_N_A4, &frame->stack->registers, op->abi_move.size, 4, memcpy(reg, slot, op->abi_move.size)) \
    X(ABI_WRITE_OUT_N_A8, &frame->stack->registers, op->abi_move.size, 8, memcpy(reg, slot, op->abi_move.size)) \
    X(ABI_WRITE_OUT_N_A16, &frame->stack->registers, op->abi_move.size, 16, memcpy(reg, slot, op->abi_move.size)) \
    X(ABI_WRITE_SEXT8, &frame->stack->registers, 8, 1, uint64_t value = (uint64_t)ci_read_int(slot, 1); memcpy(reg, &value, 8)) \
    X(ABI_WRITE_SEXT16, &frame->stack->registers, 8, 1, uint64_t value = (uint64_t)ci_read_int(slot, 2); memcpy(reg, &value, 8)) \
    X(ABI_WRITE_SEXT16_A, &frame->stack->registers, 8, 2, uint64_t value = (uint64_t)ci_read_int(slot, 2); memcpy(reg, &value, 8)) \
    X(ABI_WRITE_SEXT32, &frame->stack->registers, 8, 1, uint64_t value = (uint64_t)ci_read_int(slot, 4); memcpy(reg, &value, 8)) \
    X(ABI_WRITE_SEXT32_A, &frame->stack->registers, 8, 4, uint64_t value = (uint64_t)ci_read_int(slot, 4); memcpy(reg, &value, 8)) \
    X(ABI_WRITE_ZEXT8, &frame->stack->registers, 8, 1, uint64_t value = (uint64_t)ci_read_uint(slot, 1); memcpy(reg, &value, 8)) \
    X(ABI_WRITE_ZEXT16, &frame->stack->registers, 8, 1, uint64_t value = (uint64_t)ci_read_uint(slot, 2); memcpy(reg, &value, 8)) \
    X(ABI_WRITE_ZEXT16_A, &frame->stack->registers, 8, 2, uint64_t value = (uint64_t)ci_read_uint(slot, 2); memcpy(reg, &value, 8)) \
    X(ABI_WRITE_ZEXT32, &frame->stack->registers, 8, 1, uint64_t value = (uint64_t)ci_read_uint(slot, 4); memcpy(reg, &value, 8)) \
    X(ABI_WRITE_ZEXT32_A, &frame->stack->registers, 8, 4, uint64_t value = (uint64_t)ci_read_uint(slot, 4); memcpy(reg, &value, 8))

#define CI_ABI_TRANSFER_OPS(X) \
    X(ABI_COPY_SLOT_TO_REG_PTR_8, void* source = (char*)frame->slots + op->abi_transfer.src; void* destination; memcpy(&destination, (char*)(&frame->stack->registers) + op->abi_transfer.dst_base.capture_offset, sizeof destination); destination = (char*)destination + (int32_t)op->abi_transfer.dst; memmove(CI_ALIGNED(destination, 1), CI_ALIGNED(source, 1), 1);) \
    X(ABI_COPY_SLOT_TO_REG_PTR_16, void* source = (char*)frame->slots + op->abi_transfer.src; void* destination; memcpy(&destination, (char*)(&frame->stack->registers) + op->abi_transfer.dst_base.capture_offset, sizeof destination); destination = (char*)destination + (int32_t)op->abi_transfer.dst; memmove(CI_ALIGNED(destination, 1), CI_ALIGNED(source, 1), 2);) \
    X(ABI_COPY_SLOT_TO_REG_PTR_16_A, void* source = (char*)frame->slots + op->abi_transfer.src; void* destination; memcpy(&destination, (char*)(&frame->stack->registers) + op->abi_transfer.dst_base.capture_offset, sizeof destination); destination = (char*)destination + (int32_t)op->abi_transfer.dst; memmove(CI_ALIGNED(destination, 2), CI_ALIGNED(source, 2), 2);) \
    X(ABI_COPY_SLOT_TO_REG_PTR_32, void* source = (char*)frame->slots + op->abi_transfer.src; void* destination; memcpy(&destination, (char*)(&frame->stack->registers) + op->abi_transfer.dst_base.capture_offset, sizeof destination); destination = (char*)destination + (int32_t)op->abi_transfer.dst; memmove(CI_ALIGNED(destination, 1), CI_ALIGNED(source, 1), 4);) \
    X(ABI_COPY_SLOT_TO_REG_PTR_32_A, void* source = (char*)frame->slots + op->abi_transfer.src; void* destination; memcpy(&destination, (char*)(&frame->stack->registers) + op->abi_transfer.dst_base.capture_offset, sizeof destination); destination = (char*)destination + (int32_t)op->abi_transfer.dst; memmove(CI_ALIGNED(destination, 4), CI_ALIGNED(source, 4), 4);) \
    X(ABI_COPY_SLOT_TO_REG_PTR_64, void* source = (char*)frame->slots + op->abi_transfer.src; void* destination; memcpy(&destination, (char*)(&frame->stack->registers) + op->abi_transfer.dst_base.capture_offset, sizeof destination); destination = (char*)destination + (int32_t)op->abi_transfer.dst; memmove(CI_ALIGNED(destination, 1), CI_ALIGNED(source, 1), 8);) \
    X(ABI_COPY_SLOT_TO_REG_PTR_64_A, void* source = (char*)frame->slots + op->abi_transfer.src; void* destination; memcpy(&destination, (char*)(&frame->stack->registers) + op->abi_transfer.dst_base.capture_offset, sizeof destination); destination = (char*)destination + (int32_t)op->abi_transfer.dst; memmove(CI_ALIGNED(destination, 8), CI_ALIGNED(source, 8), 8);) \
    X(ABI_COPY_SLOT_TO_REG_PTR_128, void* source = (char*)frame->slots + op->abi_transfer.src; void* destination; memcpy(&destination, (char*)(&frame->stack->registers) + op->abi_transfer.dst_base.capture_offset, sizeof destination); destination = (char*)destination + (int32_t)op->abi_transfer.dst; memmove(CI_ALIGNED(destination, 1), CI_ALIGNED(source, 1), 16);) \
    X(ABI_COPY_SLOT_TO_REG_PTR_128_A, void* source = (char*)frame->slots + op->abi_transfer.src; void* destination; memcpy(&destination, (char*)(&frame->stack->registers) + op->abi_transfer.dst_base.capture_offset, sizeof destination); destination = (char*)destination + (int32_t)op->abi_transfer.dst; memmove(CI_ALIGNED(destination, 16), CI_ALIGNED(source, 16), 16);) \
    X(ABI_COPY_SLOT_TO_REG_PTR_N, void* source = (char*)frame->slots + op->abi_transfer.src; void* destination; memcpy(&destination, (char*)(&frame->stack->registers) + op->abi_transfer.dst_base.capture_offset, sizeof destination); destination = (char*)destination + (int32_t)op->abi_transfer.dst; memmove(CI_ALIGNED(destination, 1), CI_ALIGNED(source, 1), op->abi_transfer.size);) \
    X(ABI_COPY_SLOT_TO_REG_PTR_N_A2, void* source = (char*)frame->slots + op->abi_transfer.src; void* destination; memcpy(&destination, (char*)(&frame->stack->registers) + op->abi_transfer.dst_base.capture_offset, sizeof destination); destination = (char*)destination + (int32_t)op->abi_transfer.dst; memmove(CI_ALIGNED(destination, 2), CI_ALIGNED(source, 2), op->abi_transfer.size);) \
    X(ABI_COPY_SLOT_TO_REG_PTR_N_A4, void* source = (char*)frame->slots + op->abi_transfer.src; void* destination; memcpy(&destination, (char*)(&frame->stack->registers) + op->abi_transfer.dst_base.capture_offset, sizeof destination); destination = (char*)destination + (int32_t)op->abi_transfer.dst; memmove(CI_ALIGNED(destination, 4), CI_ALIGNED(source, 4), op->abi_transfer.size);) \
    X(ABI_COPY_SLOT_TO_REG_PTR_N_A8, void* source = (char*)frame->slots + op->abi_transfer.src; void* destination; memcpy(&destination, (char*)(&frame->stack->registers) + op->abi_transfer.dst_base.capture_offset, sizeof destination); destination = (char*)destination + (int32_t)op->abi_transfer.dst; memmove(CI_ALIGNED(destination, 8), CI_ALIGNED(source, 8), op->abi_transfer.size);) \
    X(ABI_COPY_SLOT_TO_REG_PTR_N_A16, void* source = (char*)frame->slots + op->abi_transfer.src; void* destination; memcpy(&destination, (char*)(&frame->stack->registers) + op->abi_transfer.dst_base.capture_offset, sizeof destination); destination = (char*)destination + (int32_t)op->abi_transfer.dst; memmove(CI_ALIGNED(destination, 16), CI_ALIGNED(source, 16), op->abi_transfer.size);) \
    X(ABI_COPY_INCOMING_PTR_TO_SLOT_8, void* source; memcpy(&source, (char*)(frame->incoming) + op->abi_transfer.src_base.capture_offset, sizeof source); source = (char*)source + (int32_t)op->abi_transfer.src; void* destination = (char*)frame->slots + op->abi_transfer.dst; memmove(CI_ALIGNED(destination, 1), CI_ALIGNED(source, 1), 1);) \
    X(ABI_COPY_INCOMING_PTR_TO_SLOT_16, void* source; memcpy(&source, (char*)(frame->incoming) + op->abi_transfer.src_base.capture_offset, sizeof source); source = (char*)source + (int32_t)op->abi_transfer.src; void* destination = (char*)frame->slots + op->abi_transfer.dst; memmove(CI_ALIGNED(destination, 1), CI_ALIGNED(source, 1), 2);) \
    X(ABI_COPY_INCOMING_PTR_TO_SLOT_16_A, void* source; memcpy(&source, (char*)(frame->incoming) + op->abi_transfer.src_base.capture_offset, sizeof source); source = (char*)source + (int32_t)op->abi_transfer.src; void* destination = (char*)frame->slots + op->abi_transfer.dst; memmove(CI_ALIGNED(destination, 2), CI_ALIGNED(source, 2), 2);) \
    X(ABI_COPY_INCOMING_PTR_TO_SLOT_32, void* source; memcpy(&source, (char*)(frame->incoming) + op->abi_transfer.src_base.capture_offset, sizeof source); source = (char*)source + (int32_t)op->abi_transfer.src; void* destination = (char*)frame->slots + op->abi_transfer.dst; memmove(CI_ALIGNED(destination, 1), CI_ALIGNED(source, 1), 4);) \
    X(ABI_COPY_INCOMING_PTR_TO_SLOT_32_A, void* source; memcpy(&source, (char*)(frame->incoming) + op->abi_transfer.src_base.capture_offset, sizeof source); source = (char*)source + (int32_t)op->abi_transfer.src; void* destination = (char*)frame->slots + op->abi_transfer.dst; memmove(CI_ALIGNED(destination, 4), CI_ALIGNED(source, 4), 4);) \
    X(ABI_COPY_INCOMING_PTR_TO_SLOT_64, void* source; memcpy(&source, (char*)(frame->incoming) + op->abi_transfer.src_base.capture_offset, sizeof source); source = (char*)source + (int32_t)op->abi_transfer.src; void* destination = (char*)frame->slots + op->abi_transfer.dst; memmove(CI_ALIGNED(destination, 1), CI_ALIGNED(source, 1), 8);) \
    X(ABI_COPY_INCOMING_PTR_TO_SLOT_64_A, void* source; memcpy(&source, (char*)(frame->incoming) + op->abi_transfer.src_base.capture_offset, sizeof source); source = (char*)source + (int32_t)op->abi_transfer.src; void* destination = (char*)frame->slots + op->abi_transfer.dst; memmove(CI_ALIGNED(destination, 8), CI_ALIGNED(source, 8), 8);) \
    X(ABI_COPY_INCOMING_PTR_TO_SLOT_128, void* source; memcpy(&source, (char*)(frame->incoming) + op->abi_transfer.src_base.capture_offset, sizeof source); source = (char*)source + (int32_t)op->abi_transfer.src; void* destination = (char*)frame->slots + op->abi_transfer.dst; memmove(CI_ALIGNED(destination, 1), CI_ALIGNED(source, 1), 16);) \
    X(ABI_COPY_INCOMING_PTR_TO_SLOT_128_A, void* source; memcpy(&source, (char*)(frame->incoming) + op->abi_transfer.src_base.capture_offset, sizeof source); source = (char*)source + (int32_t)op->abi_transfer.src; void* destination = (char*)frame->slots + op->abi_transfer.dst; memmove(CI_ALIGNED(destination, 16), CI_ALIGNED(source, 16), 16);) \
    X(ABI_COPY_INCOMING_PTR_TO_SLOT_N, void* source; memcpy(&source, (char*)(frame->incoming) + op->abi_transfer.src_base.capture_offset, sizeof source); source = (char*)source + (int32_t)op->abi_transfer.src; void* destination = (char*)frame->slots + op->abi_transfer.dst; memmove(CI_ALIGNED(destination, 1), CI_ALIGNED(source, 1), op->abi_transfer.size);) \
    X(ABI_COPY_INCOMING_PTR_TO_SLOT_N_A2, void* source; memcpy(&source, (char*)(frame->incoming) + op->abi_transfer.src_base.capture_offset, sizeof source); source = (char*)source + (int32_t)op->abi_transfer.src; void* destination = (char*)frame->slots + op->abi_transfer.dst; memmove(CI_ALIGNED(destination, 2), CI_ALIGNED(source, 2), op->abi_transfer.size);) \
    X(ABI_COPY_INCOMING_PTR_TO_SLOT_N_A4, void* source; memcpy(&source, (char*)(frame->incoming) + op->abi_transfer.src_base.capture_offset, sizeof source); source = (char*)source + (int32_t)op->abi_transfer.src; void* destination = (char*)frame->slots + op->abi_transfer.dst; memmove(CI_ALIGNED(destination, 4), CI_ALIGNED(source, 4), op->abi_transfer.size);) \
    X(ABI_COPY_INCOMING_PTR_TO_SLOT_N_A8, void* source; memcpy(&source, (char*)(frame->incoming) + op->abi_transfer.src_base.capture_offset, sizeof source); source = (char*)source + (int32_t)op->abi_transfer.src; void* destination = (char*)frame->slots + op->abi_transfer.dst; memmove(CI_ALIGNED(destination, 8), CI_ALIGNED(source, 8), op->abi_transfer.size);) \
    X(ABI_COPY_INCOMING_PTR_TO_SLOT_N_A16, void* source; memcpy(&source, (char*)(frame->incoming) + op->abi_transfer.src_base.capture_offset, sizeof source); source = (char*)source + (int32_t)op->abi_transfer.src; void* destination = (char*)frame->slots + op->abi_transfer.dst; memmove(CI_ALIGNED(destination, 16), CI_ALIGNED(source, 16), op->abi_transfer.size);) \
    X(ABI_COPY_INCOMING_PTR_INDIRECT_TO_SLOT_8, void* source; memcpy(&source, (char*)(frame->incoming) + op->abi_transfer.src_base.capture_offset, sizeof source); source = (char*)source + (int32_t)op->abi_transfer.src; memcpy(&source, source, sizeof source); void* destination = (char*)frame->slots + op->abi_transfer.dst; memmove(CI_ALIGNED(destination, 1), CI_ALIGNED(source, 1), 1);) \
    X(ABI_COPY_INCOMING_PTR_INDIRECT_TO_SLOT_16, void* source; memcpy(&source, (char*)(frame->incoming) + op->abi_transfer.src_base.capture_offset, sizeof source); source = (char*)source + (int32_t)op->abi_transfer.src; memcpy(&source, source, sizeof source); void* destination = (char*)frame->slots + op->abi_transfer.dst; memmove(CI_ALIGNED(destination, 1), CI_ALIGNED(source, 1), 2);) \
    X(ABI_COPY_INCOMING_PTR_INDIRECT_TO_SLOT_16_A, void* source; memcpy(&source, (char*)(frame->incoming) + op->abi_transfer.src_base.capture_offset, sizeof source); source = (char*)source + (int32_t)op->abi_transfer.src; memcpy(&source, source, sizeof source); void* destination = (char*)frame->slots + op->abi_transfer.dst; memmove(CI_ALIGNED(destination, 2), CI_ALIGNED(source, 2), 2);) \
    X(ABI_COPY_INCOMING_PTR_INDIRECT_TO_SLOT_32, void* source; memcpy(&source, (char*)(frame->incoming) + op->abi_transfer.src_base.capture_offset, sizeof source); source = (char*)source + (int32_t)op->abi_transfer.src; memcpy(&source, source, sizeof source); void* destination = (char*)frame->slots + op->abi_transfer.dst; memmove(CI_ALIGNED(destination, 1), CI_ALIGNED(source, 1), 4);) \
    X(ABI_COPY_INCOMING_PTR_INDIRECT_TO_SLOT_32_A, void* source; memcpy(&source, (char*)(frame->incoming) + op->abi_transfer.src_base.capture_offset, sizeof source); source = (char*)source + (int32_t)op->abi_transfer.src; memcpy(&source, source, sizeof source); void* destination = (char*)frame->slots + op->abi_transfer.dst; memmove(CI_ALIGNED(destination, 4), CI_ALIGNED(source, 4), 4);) \
    X(ABI_COPY_INCOMING_PTR_INDIRECT_TO_SLOT_64, void* source; memcpy(&source, (char*)(frame->incoming) + op->abi_transfer.src_base.capture_offset, sizeof source); source = (char*)source + (int32_t)op->abi_transfer.src; memcpy(&source, source, sizeof source); void* destination = (char*)frame->slots + op->abi_transfer.dst; memmove(CI_ALIGNED(destination, 1), CI_ALIGNED(source, 1), 8);) \
    X(ABI_COPY_INCOMING_PTR_INDIRECT_TO_SLOT_64_A, void* source; memcpy(&source, (char*)(frame->incoming) + op->abi_transfer.src_base.capture_offset, sizeof source); source = (char*)source + (int32_t)op->abi_transfer.src; memcpy(&source, source, sizeof source); void* destination = (char*)frame->slots + op->abi_transfer.dst; memmove(CI_ALIGNED(destination, 8), CI_ALIGNED(source, 8), 8);) \
    X(ABI_COPY_INCOMING_PTR_INDIRECT_TO_SLOT_128, void* source; memcpy(&source, (char*)(frame->incoming) + op->abi_transfer.src_base.capture_offset, sizeof source); source = (char*)source + (int32_t)op->abi_transfer.src; memcpy(&source, source, sizeof source); void* destination = (char*)frame->slots + op->abi_transfer.dst; memmove(CI_ALIGNED(destination, 1), CI_ALIGNED(source, 1), 16);) \
    X(ABI_COPY_INCOMING_PTR_INDIRECT_TO_SLOT_128_A, void* source; memcpy(&source, (char*)(frame->incoming) + op->abi_transfer.src_base.capture_offset, sizeof source); source = (char*)source + (int32_t)op->abi_transfer.src; memcpy(&source, source, sizeof source); void* destination = (char*)frame->slots + op->abi_transfer.dst; memmove(CI_ALIGNED(destination, 16), CI_ALIGNED(source, 16), 16);) \
    X(ABI_COPY_INCOMING_PTR_INDIRECT_TO_SLOT_N, void* source; memcpy(&source, (char*)(frame->incoming) + op->abi_transfer.src_base.capture_offset, sizeof source); source = (char*)source + (int32_t)op->abi_transfer.src; memcpy(&source, source, sizeof source); void* destination = (char*)frame->slots + op->abi_transfer.dst; memmove(CI_ALIGNED(destination, 1), CI_ALIGNED(source, 1), op->abi_transfer.size);) \
    X(ABI_COPY_INCOMING_PTR_INDIRECT_TO_SLOT_N_A2, void* source; memcpy(&source, (char*)(frame->incoming) + op->abi_transfer.src_base.capture_offset, sizeof source); source = (char*)source + (int32_t)op->abi_transfer.src; memcpy(&source, source, sizeof source); void* destination = (char*)frame->slots + op->abi_transfer.dst; memmove(CI_ALIGNED(destination, 2), CI_ALIGNED(source, 2), op->abi_transfer.size);) \
    X(ABI_COPY_INCOMING_PTR_INDIRECT_TO_SLOT_N_A4, void* source; memcpy(&source, (char*)(frame->incoming) + op->abi_transfer.src_base.capture_offset, sizeof source); source = (char*)source + (int32_t)op->abi_transfer.src; memcpy(&source, source, sizeof source); void* destination = (char*)frame->slots + op->abi_transfer.dst; memmove(CI_ALIGNED(destination, 4), CI_ALIGNED(source, 4), op->abi_transfer.size);) \
    X(ABI_COPY_INCOMING_PTR_INDIRECT_TO_SLOT_N_A8, void* source; memcpy(&source, (char*)(frame->incoming) + op->abi_transfer.src_base.capture_offset, sizeof source); source = (char*)source + (int32_t)op->abi_transfer.src; memcpy(&source, source, sizeof source); void* destination = (char*)frame->slots + op->abi_transfer.dst; memmove(CI_ALIGNED(destination, 8), CI_ALIGNED(source, 8), op->abi_transfer.size);) \
    X(ABI_COPY_INCOMING_PTR_INDIRECT_TO_SLOT_N_A16, void* source; memcpy(&source, (char*)(frame->incoming) + op->abi_transfer.src_base.capture_offset, sizeof source); source = (char*)source + (int32_t)op->abi_transfer.src; memcpy(&source, source, sizeof source); void* destination = (char*)frame->slots + op->abi_transfer.dst; memmove(CI_ALIGNED(destination, 16), CI_ALIGNED(source, 16), op->abi_transfer.size);) \
    X(ABI_COPY_SLOT_TO_SLOT_PTR_8, void* source = (char*)frame->slots + op->abi_transfer.src; void* destination; memcpy(&destination, (char*)frame->slots + op->abi_transfer.dst_pointer, sizeof destination); destination = (char*)destination + (int32_t)op->abi_transfer.dst; memmove(CI_ALIGNED(destination, 1), CI_ALIGNED(source, 1), 1);) \
    X(ABI_COPY_SLOT_TO_SLOT_PTR_16, void* source = (char*)frame->slots + op->abi_transfer.src; void* destination; memcpy(&destination, (char*)frame->slots + op->abi_transfer.dst_pointer, sizeof destination); destination = (char*)destination + (int32_t)op->abi_transfer.dst; memmove(CI_ALIGNED(destination, 1), CI_ALIGNED(source, 1), 2);) \
    X(ABI_COPY_SLOT_TO_SLOT_PTR_16_A, void* source = (char*)frame->slots + op->abi_transfer.src; void* destination; memcpy(&destination, (char*)frame->slots + op->abi_transfer.dst_pointer, sizeof destination); destination = (char*)destination + (int32_t)op->abi_transfer.dst; memmove(CI_ALIGNED(destination, 2), CI_ALIGNED(source, 2), 2);) \
    X(ABI_COPY_SLOT_TO_SLOT_PTR_32, void* source = (char*)frame->slots + op->abi_transfer.src; void* destination; memcpy(&destination, (char*)frame->slots + op->abi_transfer.dst_pointer, sizeof destination); destination = (char*)destination + (int32_t)op->abi_transfer.dst; memmove(CI_ALIGNED(destination, 1), CI_ALIGNED(source, 1), 4);) \
    X(ABI_COPY_SLOT_TO_SLOT_PTR_32_A, void* source = (char*)frame->slots + op->abi_transfer.src; void* destination; memcpy(&destination, (char*)frame->slots + op->abi_transfer.dst_pointer, sizeof destination); destination = (char*)destination + (int32_t)op->abi_transfer.dst; memmove(CI_ALIGNED(destination, 4), CI_ALIGNED(source, 4), 4);) \
    X(ABI_COPY_SLOT_TO_SLOT_PTR_64, void* source = (char*)frame->slots + op->abi_transfer.src; void* destination; memcpy(&destination, (char*)frame->slots + op->abi_transfer.dst_pointer, sizeof destination); destination = (char*)destination + (int32_t)op->abi_transfer.dst; memmove(CI_ALIGNED(destination, 1), CI_ALIGNED(source, 1), 8);) \
    X(ABI_COPY_SLOT_TO_SLOT_PTR_64_A, void* source = (char*)frame->slots + op->abi_transfer.src; void* destination; memcpy(&destination, (char*)frame->slots + op->abi_transfer.dst_pointer, sizeof destination); destination = (char*)destination + (int32_t)op->abi_transfer.dst; memmove(CI_ALIGNED(destination, 8), CI_ALIGNED(source, 8), 8);) \
    X(ABI_COPY_SLOT_TO_SLOT_PTR_128, void* source = (char*)frame->slots + op->abi_transfer.src; void* destination; memcpy(&destination, (char*)frame->slots + op->abi_transfer.dst_pointer, sizeof destination); destination = (char*)destination + (int32_t)op->abi_transfer.dst; memmove(CI_ALIGNED(destination, 1), CI_ALIGNED(source, 1), 16);) \
    X(ABI_COPY_SLOT_TO_SLOT_PTR_128_A, void* source = (char*)frame->slots + op->abi_transfer.src; void* destination; memcpy(&destination, (char*)frame->slots + op->abi_transfer.dst_pointer, sizeof destination); destination = (char*)destination + (int32_t)op->abi_transfer.dst; memmove(CI_ALIGNED(destination, 16), CI_ALIGNED(source, 16), 16);) \
    X(ABI_COPY_SLOT_TO_SLOT_PTR_N, void* source = (char*)frame->slots + op->abi_transfer.src; void* destination; memcpy(&destination, (char*)frame->slots + op->abi_transfer.dst_pointer, sizeof destination); destination = (char*)destination + (int32_t)op->abi_transfer.dst; memmove(CI_ALIGNED(destination, 1), CI_ALIGNED(source, 1), op->abi_transfer.size);) \
    X(ABI_COPY_SLOT_TO_SLOT_PTR_N_A2, void* source = (char*)frame->slots + op->abi_transfer.src; void* destination; memcpy(&destination, (char*)frame->slots + op->abi_transfer.dst_pointer, sizeof destination); destination = (char*)destination + (int32_t)op->abi_transfer.dst; memmove(CI_ALIGNED(destination, 2), CI_ALIGNED(source, 2), op->abi_transfer.size);) \
    X(ABI_COPY_SLOT_TO_SLOT_PTR_N_A4, void* source = (char*)frame->slots + op->abi_transfer.src; void* destination; memcpy(&destination, (char*)frame->slots + op->abi_transfer.dst_pointer, sizeof destination); destination = (char*)destination + (int32_t)op->abi_transfer.dst; memmove(CI_ALIGNED(destination, 4), CI_ALIGNED(source, 4), op->abi_transfer.size);) \
    X(ABI_COPY_SLOT_TO_SLOT_PTR_N_A8, void* source = (char*)frame->slots + op->abi_transfer.src; void* destination; memcpy(&destination, (char*)frame->slots + op->abi_transfer.dst_pointer, sizeof destination); destination = (char*)destination + (int32_t)op->abi_transfer.dst; memmove(CI_ALIGNED(destination, 8), CI_ALIGNED(source, 8), op->abi_transfer.size);) \
    X(ABI_COPY_SLOT_TO_SLOT_PTR_N_A16, void* source = (char*)frame->slots + op->abi_transfer.src; void* destination; memcpy(&destination, (char*)frame->slots + op->abi_transfer.dst_pointer, sizeof destination); destination = (char*)destination + (int32_t)op->abi_transfer.dst; memmove(CI_ALIGNED(destination, 16), CI_ALIGNED(source, 16), op->abi_transfer.size);) \
    X(ABI_COPY_INCOMING_TO_INCOMING_PTR_64_A, void* source = (char*)frame->incoming + op->abi_transfer.src_reg.capture_offset; void* destination; memcpy(&destination, (char*)(frame->incoming) + op->abi_transfer.dst_base.capture_offset, sizeof destination); destination = (char*)destination + (int32_t)op->abi_transfer.dst; memmove(CI_ALIGNED(destination, 8), CI_ALIGNED(source, 8), 8);) \
    X(ABI_ADDRESS_REG_PTR_TO_REG_PTR, void* source; memcpy(&source, (char*)(&frame->stack->registers) + op->abi_transfer.src_base.capture_offset, sizeof source); source = (char*)source + (int32_t)op->abi_transfer.src; void* destination = (char*)(&frame->stack->registers) + op->abi_transfer.dst_reg.capture_offset; memcpy(CI_ALIGNED(destination, 1), &source, sizeof source);) \
    X(ABI_ADDRESS_REG_PTR_TO_REG_PTR_A, void* source; memcpy(&source, (char*)(&frame->stack->registers) + op->abi_transfer.src_base.capture_offset, sizeof source); source = (char*)source + (int32_t)op->abi_transfer.src; void* destination = (char*)(&frame->stack->registers) + op->abi_transfer.dst_reg.capture_offset; memcpy(CI_ALIGNED(destination, 8), &source, sizeof source);) \
    X(ABI_ADDRESS_REG_PTR_TO_REG_PTR_PTR, void* source; memcpy(&source, (char*)(&frame->stack->registers) + op->abi_transfer.src_base.capture_offset, sizeof source); source = (char*)source + (int32_t)op->abi_transfer.src; void* destination; memcpy(&destination, (char*)(&frame->stack->registers) + op->abi_transfer.dst_base.capture_offset, sizeof destination); destination = (char*)destination + (int32_t)op->abi_transfer.dst; memcpy(CI_ALIGNED(destination, 1), &source, sizeof source);) \
    X(ABI_ADDRESS_REG_PTR_TO_REG_PTR_PTR_A, void* source; memcpy(&source, (char*)(&frame->stack->registers) + op->abi_transfer.src_base.capture_offset, sizeof source); source = (char*)source + (int32_t)op->abi_transfer.src; void* destination; memcpy(&destination, (char*)(&frame->stack->registers) + op->abi_transfer.dst_base.capture_offset, sizeof destination); destination = (char*)destination + (int32_t)op->abi_transfer.dst; memcpy(CI_ALIGNED(destination, 8), &source, sizeof source);) \
    X(ABI_ADDRESS_SLOT_PTR_TO_REG_PTR, void* source; memcpy(&source, (char*)frame->slots + op->abi_transfer.src_pointer, sizeof source); source = (char*)source + (int32_t)op->abi_transfer.src; void* destination = (char*)(&frame->stack->registers) + op->abi_transfer.dst_reg.capture_offset; memcpy(CI_ALIGNED(destination, 1), &source, sizeof source);) \
    X(ABI_ADDRESS_SLOT_PTR_TO_REG_PTR_A, void* source; memcpy(&source, (char*)frame->slots + op->abi_transfer.src_pointer, sizeof source); source = (char*)source + (int32_t)op->abi_transfer.src; void* destination = (char*)(&frame->stack->registers) + op->abi_transfer.dst_reg.capture_offset; memcpy(CI_ALIGNED(destination, 8), &source, sizeof source);) \
    X(ABI_ADDRESS_SLOT_TO_REG_PTR, void* source = (char*)frame->slots + op->abi_transfer.src; void* destination = (char*)(&frame->stack->registers) + op->abi_transfer.dst_reg.capture_offset; memcpy(CI_ALIGNED(destination, 1), &source, sizeof source);) \
    X(ABI_ADDRESS_SLOT_TO_REG_PTR_A, void* source = (char*)frame->slots + op->abi_transfer.src; void* destination = (char*)(&frame->stack->registers) + op->abi_transfer.dst_reg.capture_offset; memcpy(CI_ALIGNED(destination, 8), &source, sizeof source);) \
    X(ABI_ADDRESS_SLOT_TO_SLOT_PTR, void* source = (char*)frame->slots + op->abi_transfer.src; void* destination = (char*)frame->slots + op->abi_transfer.dst; memcpy(CI_ALIGNED(destination, 1), &source, sizeof source);) \
    X(ABI_ADDRESS_SLOT_TO_SLOT_PTR_A, void* source = (char*)frame->slots + op->abi_transfer.src; void* destination = (char*)frame->slots + op->abi_transfer.dst; memcpy(CI_ALIGNED(destination, 8), &source, sizeof source);) \
    X(ABI_ADDRESS_INCOMING_PTR_TO_SLOT_PTR_PTR, void* source; memcpy(&source, (char*)(frame->incoming) + op->abi_transfer.src_base.capture_offset, sizeof source); source = (char*)source + (int32_t)op->abi_transfer.src; void* destination; memcpy(&destination, (char*)frame->slots + op->abi_transfer.dst_pointer, sizeof destination); destination = (char*)destination + (int32_t)op->abi_transfer.dst; memcpy(CI_ALIGNED(destination, 1), &source, sizeof source);) \
    X(ABI_ADDRESS_INCOMING_PTR_TO_SLOT_PTR_PTR_A, void* source; memcpy(&source, (char*)(frame->incoming) + op->abi_transfer.src_base.capture_offset, sizeof source); source = (char*)source + (int32_t)op->abi_transfer.src; void* destination; memcpy(&destination, (char*)frame->slots + op->abi_transfer.dst_pointer, sizeof destination); destination = (char*)destination + (int32_t)op->abi_transfer.dst; memcpy(CI_ALIGNED(destination, 8), &source, sizeof source);) \
    X(ABI_ADDRESS_INCOMING_SAVE_TO_SLOT_PTR_PTR, void* source = (char*)frame->incoming + op->abi_transfer.src_base.capture_offset + op->abi_transfer.src; void* destination; memcpy(&destination, (char*)frame->slots + op->abi_transfer.dst_pointer, sizeof destination); destination = (char*)destination + (int32_t)op->abi_transfer.dst; memcpy(CI_ALIGNED(destination, 1), &source, sizeof source);) \
    X(ABI_ADDRESS_INCOMING_SAVE_TO_SLOT_PTR_PTR_A, void* source = (char*)frame->incoming + op->abi_transfer.src_base.capture_offset + op->abi_transfer.src; void* destination; memcpy(&destination, (char*)frame->slots + op->abi_transfer.dst_pointer, sizeof destination); destination = (char*)destination + (int32_t)op->abi_transfer.dst; memcpy(CI_ALIGNED(destination, 8), &source, sizeof source);) \
    X(ABI_ZERO_SLOT_8, void* destination = (char*)frame->slots + op->abi_transfer.dst; memset(CI_ALIGNED(destination, 1), 0, 1);) \
    X(ABI_ZERO_SLOT_16, void* destination = (char*)frame->slots + op->abi_transfer.dst; memset(CI_ALIGNED(destination, 1), 0, 2);) \
    X(ABI_ZERO_SLOT_16_A, void* destination = (char*)frame->slots + op->abi_transfer.dst; memset(CI_ALIGNED(destination, 2), 0, 2);) \
    X(ABI_ZERO_SLOT_32, void* destination = (char*)frame->slots + op->abi_transfer.dst; memset(CI_ALIGNED(destination, 1), 0, 4);) \
    X(ABI_ZERO_SLOT_32_A, void* destination = (char*)frame->slots + op->abi_transfer.dst; memset(CI_ALIGNED(destination, 4), 0, 4);) \
    X(ABI_ZERO_SLOT_64, void* destination = (char*)frame->slots + op->abi_transfer.dst; memset(CI_ALIGNED(destination, 1), 0, 8);) \
    X(ABI_ZERO_SLOT_64_A, void* destination = (char*)frame->slots + op->abi_transfer.dst; memset(CI_ALIGNED(destination, 8), 0, 8);) \
    X(ABI_ZERO_SLOT_128, void* destination = (char*)frame->slots + op->abi_transfer.dst; memset(CI_ALIGNED(destination, 1), 0, 16);) \
    X(ABI_ZERO_SLOT_128_A, void* destination = (char*)frame->slots + op->abi_transfer.dst; memset(CI_ALIGNED(destination, 16), 0, 16);) \
    X(ABI_ZERO_SLOT_N, void* destination = (char*)frame->slots + op->abi_transfer.dst; memset(CI_ALIGNED(destination, 1), 0, op->abi_transfer.size);) \
    X(ABI_ZERO_SLOT_N_A2, void* destination = (char*)frame->slots + op->abi_transfer.dst; memset(CI_ALIGNED(destination, 2), 0, op->abi_transfer.size);) \
    X(ABI_ZERO_SLOT_N_A4, void* destination = (char*)frame->slots + op->abi_transfer.dst; memset(CI_ALIGNED(destination, 4), 0, op->abi_transfer.size);) \
    X(ABI_ZERO_SLOT_N_A8, void* destination = (char*)frame->slots + op->abi_transfer.dst; memset(CI_ALIGNED(destination, 8), 0, op->abi_transfer.size);) \
    X(ABI_ZERO_SLOT_N_A16, void* destination = (char*)frame->slots + op->abi_transfer.dst; memset(CI_ALIGNED(destination, 16), 0, op->abi_transfer.size);) \
    X(ABI_CONSTANT_REG_8, void* destination = (char*)(&frame->stack->registers) + op->abi_transfer.dst_reg.capture_offset; uint32_t value = op->abi_transfer.immediate; memcpy(CI_ALIGNED(destination, 1), &value, 1);) \
    X(ABI_CONSTANT_SLOT_PTR_32, void* destination; memcpy(&destination, (char*)frame->slots + op->abi_transfer.dst_pointer, sizeof destination); destination = (char*)destination + (int32_t)op->abi_transfer.dst; uint32_t value = op->abi_transfer.immediate; memcpy(CI_ALIGNED(destination, 1), &value, 4);) \
    X(ABI_CONSTANT_SLOT_PTR_32_A, void* destination; memcpy(&destination, (char*)frame->slots + op->abi_transfer.dst_pointer, sizeof destination); destination = (char*)destination + (int32_t)op->abi_transfer.dst; uint32_t value = op->abi_transfer.immediate; memcpy(CI_ALIGNED(destination, 4), &value, 4);)

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
    X(BSWAP16, 2, bswap16((uint16_t)ci_read_uint(src, 2))) \
    X(BSWAP32, 4, bswap32((uint32_t)ci_read_uint(src, 4))) \
    X(BSWAP64, 8, bswap64(ci_read_uint(src, 8)))

#define CI_BOUNDS_OPS(X) \
    X(BOUNDS32, 4, >=, op->bounds.index_signed) \
    X(BOUNDS_INCLUSIVE32, 4, >, op->bounds.index_signed) \
    X(BOUNDS64, 8, >=, op->bounds.index_signed) \
    X(BOUNDS_INCLUSIVE64, 8, >, op->bounds.index_signed)

#define CI_INDEX_OPS(X) \
    X(INDEX32_S8, 4, (uint64_t)ci_read_int(src, 1)) \
    X(INDEX32_U8, 4, ci_read_uint(src, 1)) \
    X(INDEX32_S16, 4, (uint64_t)ci_read_int(src, 2)) \
    X(INDEX32_U16, 4, ci_read_uint(src, 2)) \
    X(INDEX32, 4, ci_read_uint(src, 4)) \
    X(INDEX64_S8, 8, (uint64_t)ci_read_int(src, 1)) \
    X(INDEX64_U8, 8, ci_read_uint(src, 1)) \
    X(INDEX64_S16, 8, (uint64_t)ci_read_int(src, 2)) \
    X(INDEX64_U16, 8, ci_read_uint(src, 2)) \
    X(INDEX64_S32, 8, (uint64_t)ci_read_int(src, 4)) \
    X(INDEX64_U32, 8, ci_read_uint(src, 4)) \
    X(INDEX64, 8, ci_read_uint(src, 8))

#define CI_LOAD_BITFIELD_OPS(X) \
    X(LDBF_U8, 1, ci_bitfield_extend(ci_bitfield_read(CI_ALIGNED(ptr + op->load_bf.offset, 1), 1, op->load_bf.bit_offset, op->load_bf.bit_width), op->load_bf.bit_width, 0), 1) \
    X(LDBF_S8, 1, ci_bitfield_extend(ci_bitfield_read(CI_ALIGNED(ptr + op->load_bf.offset, 1), 1, op->load_bf.bit_offset, op->load_bf.bit_width), op->load_bf.bit_width, 1), 1) \
    X(LDBF_U16, 2, ci_bitfield_extend(ci_bitfield_read(CI_ALIGNED(ptr + op->load_bf.offset, 1), 2, op->load_bf.bit_offset, op->load_bf.bit_width), op->load_bf.bit_width, 0), 1) \
    X(LDBF_U16_A, 2, ci_bitfield_extend(ci_bitfield_read(CI_ALIGNED(ptr + op->load_bf.offset, 2), 2, op->load_bf.bit_offset, op->load_bf.bit_width), op->load_bf.bit_width, 0), 2) \
    X(LDBF_S16, 2, ci_bitfield_extend(ci_bitfield_read(CI_ALIGNED(ptr + op->load_bf.offset, 1), 2, op->load_bf.bit_offset, op->load_bf.bit_width), op->load_bf.bit_width, 1), 1) \
    X(LDBF_S16_A, 2, ci_bitfield_extend(ci_bitfield_read(CI_ALIGNED(ptr + op->load_bf.offset, 2), 2, op->load_bf.bit_offset, op->load_bf.bit_width), op->load_bf.bit_width, 1), 2) \
    X(LDBF_U32, 4, ci_bitfield_extend(ci_bitfield_read(CI_ALIGNED(ptr + op->load_bf.offset, 1), 4, op->load_bf.bit_offset, op->load_bf.bit_width), op->load_bf.bit_width, 0), 1) \
    X(LDBF_U32_A, 4, ci_bitfield_extend(ci_bitfield_read(CI_ALIGNED(ptr + op->load_bf.offset, 4), 4, op->load_bf.bit_offset, op->load_bf.bit_width), op->load_bf.bit_width, 0), 4) \
    X(LDBF_S32, 4, ci_bitfield_extend(ci_bitfield_read(CI_ALIGNED(ptr + op->load_bf.offset, 1), 4, op->load_bf.bit_offset, op->load_bf.bit_width), op->load_bf.bit_width, 1), 1) \
    X(LDBF_S32_A, 4, ci_bitfield_extend(ci_bitfield_read(CI_ALIGNED(ptr + op->load_bf.offset, 4), 4, op->load_bf.bit_offset, op->load_bf.bit_width), op->load_bf.bit_width, 1), 4) \
    X(LDBF_U64, 8, ci_bitfield_extend(ci_bitfield_read(CI_ALIGNED(ptr + op->load_bf.offset, 1), 8, op->load_bf.bit_offset, op->load_bf.bit_width), op->load_bf.bit_width, 0), 1) \
    X(LDBF_U64_A, 8, ci_bitfield_extend(ci_bitfield_read(CI_ALIGNED(ptr + op->load_bf.offset, 8), 8, op->load_bf.bit_offset, op->load_bf.bit_width), op->load_bf.bit_width, 0), 8) \
    X(LDBF_S64, 8, ci_bitfield_extend(ci_bitfield_read(CI_ALIGNED(ptr + op->load_bf.offset, 1), 8, op->load_bf.bit_offset, op->load_bf.bit_width), op->load_bf.bit_width, 1), 1) \
    X(LDBF_S64_A, 8, ci_bitfield_extend(ci_bitfield_read(CI_ALIGNED(ptr + op->load_bf.offset, 8), 8, op->load_bf.bit_offset, op->load_bf.bit_width), op->load_bf.bit_width, 1), 8)

#define CI_LOAD_BITFIELD_128_OPS(X) \
    X(LDBF_U128, 1, 1) \
    X(LDBF_U128_A, 1, 16) \
    X(LDBF_S128, 0, 1) \
    X(LDBF_S128_A, 0, 16)

#define CI_LOAD_BITFIELD_BYTES_OPS(X) \
    X(LDBFB_U8, 1, ci_bitfield_extend(ci_bitfield_read_bytes(ptr + op->load_bf.offset, op->load_bf.bit_offset, op->load_bf.bit_width), op->load_bf.bit_width, 0)) \
    X(LDBFB_S8, 1, ci_bitfield_extend(ci_bitfield_read_bytes(ptr + op->load_bf.offset, op->load_bf.bit_offset, op->load_bf.bit_width), op->load_bf.bit_width, 1)) \
    X(LDBFB_U16, 2, ci_bitfield_extend(ci_bitfield_read_bytes(ptr + op->load_bf.offset, op->load_bf.bit_offset, op->load_bf.bit_width), op->load_bf.bit_width, 0)) \
    X(LDBFB_S16, 2, ci_bitfield_extend(ci_bitfield_read_bytes(ptr + op->load_bf.offset, op->load_bf.bit_offset, op->load_bf.bit_width), op->load_bf.bit_width, 1)) \
    X(LDBFB_U32, 4, ci_bitfield_extend(ci_bitfield_read_bytes(ptr + op->load_bf.offset, op->load_bf.bit_offset, op->load_bf.bit_width), op->load_bf.bit_width, 0)) \
    X(LDBFB_S32, 4, ci_bitfield_extend(ci_bitfield_read_bytes(ptr + op->load_bf.offset, op->load_bf.bit_offset, op->load_bf.bit_width), op->load_bf.bit_width, 1)) \
    X(LDBFB_U64, 8, ci_bitfield_extend(ci_bitfield_read_bytes(ptr + op->load_bf.offset, op->load_bf.bit_offset, op->load_bf.bit_width), op->load_bf.bit_width, 0)) \
    X(LDBFB_S64, 8, ci_bitfield_extend(ci_bitfield_read_bytes(ptr + op->load_bf.offset, op->load_bf.bit_offset, op->load_bf.bit_width), op->load_bf.bit_width, 1))

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
    X(CALL_DIRECT, func = d->func; fn = func->native_func;) \
    X(CALL_INDIRECT, memcpy(&fn, (char*)frame->slots + op->call.args_slot - CI_INDIRECT_CALL_PREFIX_SIZE, sizeof fn); func = ci_closure_map_get(ci, (void*)fn);)

#define CI_SWITCH_DENSE_OPS(X) \
    X(SWITCH_DENSE, 0)

#define CI_SWITCH_RANGE_OPS(X) \
    X(SWITCH_SIGNED, UINT64_C(1) << 63) \
    X(SWITCH_UNSIGNED, 0)

#define CI_BUILTIN_OPS(X) \
    X(TRAP, return ci_error(ci, op->loc, "__builtin_trap");) \
    X(UNREACHABLE, return ci_error(ci, op->loc, "__builtin_unreachable reached");) \
    X(ABORT, return ci_error(ci, op->loc, "__builtin_abort called");) \
    X(DEBUGTRAP, ) \
    X(BACKTRACE, ci_backtrace(ci, frame, 0);) \
    X(READCYCLECOUNTER, uint64_t ticks = performance_counter(); memcpy((char*)frame->slots + op->builtin.slot, &ticks, 8);) \
    X(READCYCLECOUNTER_DISCARD, (void)performance_counter();)

#define CI_ALLOCA_OPS(X) \
    X(STACK_ALLOC, ci_stack_alloc) \
    X(STACK_ZALLOC, ci_stack_zalloc)

#define CI_FENCE_OPS(X) \
    X(THREAD_FENCE, CI_THREAD_FENCE();) \
    X(SIGNAL_FENCE, )

#define CI_BIT_COUNT_OPS(X) \
    X(FFS8, 8, UINT64_MAX >> 56, (lo || hi) ? cc_bit_builtin_ctz(lo, hi, 8) + 1 : 0) \
    X(FFS16, 16, UINT64_MAX >> 48, (lo || hi) ? cc_bit_builtin_ctz(lo, hi, 16) + 1 : 0) \
    X(FFS32, 32, UINT64_MAX >> 32, (lo || hi) ? cc_bit_builtin_ctz(lo, hi, 32) + 1 : 0) \
    X(FFS64, 64, UINT64_MAX >> 0, (lo || hi) ? cc_bit_builtin_ctz(lo, hi, 64) + 1 : 0) \
    X(FFS128, 128, UINT64_MAX, (lo || hi) ? cc_bit_builtin_ctz(lo, hi, 128) + 1 : 0) \
    X(CLRSB8, 8, UINT64_MAX >> 56, (lo >> 7) ? cc_bit_builtin_clz(lo ^ (UINT64_MAX >> 56), ~hi, 8) - 1 : cc_bit_builtin_clz(lo, hi, 8) - 1) \
    X(CLRSB16, 16, UINT64_MAX >> 48, (lo >> 15) ? cc_bit_builtin_clz(lo ^ (UINT64_MAX >> 48), ~hi, 16) - 1 : cc_bit_builtin_clz(lo, hi, 16) - 1) \
    X(CLRSB32, 32, UINT64_MAX >> 32, (lo >> 31) ? cc_bit_builtin_clz(lo ^ (UINT64_MAX >> 32), ~hi, 32) - 1 : cc_bit_builtin_clz(lo, hi, 32) - 1) \
    X(CLRSB64, 64, UINT64_MAX >> 0, (lo >> 63) ? cc_bit_builtin_clz(lo ^ (UINT64_MAX >> 0), ~hi, 64) - 1 : cc_bit_builtin_clz(lo, hi, 64) - 1) \
    X(CLRSB128, 128, UINT64_MAX, (hi >> 63) ? cc_bit_builtin_clz(lo ^ UINT64_MAX, ~hi, 128) - 1 : cc_bit_builtin_clz(lo, hi, 128) - 1) \
    X(PARITY8, 8, UINT64_MAX >> 56, (popcount_64(lo) + popcount_64(hi)) & 1) \
    X(PARITY16, 16, UINT64_MAX >> 48, (popcount_64(lo) + popcount_64(hi)) & 1) \
    X(PARITY32, 32, UINT64_MAX >> 32, (popcount_64(lo) + popcount_64(hi)) & 1) \
    X(PARITY64, 64, UINT64_MAX >> 0, (popcount_64(lo) + popcount_64(hi)) & 1) \
    X(PARITY128, 128, UINT64_MAX, (popcount_64(lo) + popcount_64(hi)) & 1) \
    X(CLZ8, 8, UINT64_MAX >> 56, cc_bit_builtin_clz(lo, hi, 8)) \
    X(CLZ16, 16, UINT64_MAX >> 48, cc_bit_builtin_clz(lo, hi, 16)) \
    X(CLZ32, 32, UINT64_MAX >> 32, cc_bit_builtin_clz(lo, hi, 32)) \
    X(CLZ64, 64, UINT64_MAX >> 0, cc_bit_builtin_clz(lo, hi, 64)) \
    X(CLZ128, 128, UINT64_MAX, cc_bit_builtin_clz(lo, hi, 128)) \
    X(CTZ8, 8, UINT64_MAX >> 56, cc_bit_builtin_ctz(lo, hi, 8)) \
    X(CTZ16, 16, UINT64_MAX >> 48, cc_bit_builtin_ctz(lo, hi, 16)) \
    X(CTZ32, 32, UINT64_MAX >> 32, cc_bit_builtin_ctz(lo, hi, 32)) \
    X(CTZ64, 64, UINT64_MAX >> 0, cc_bit_builtin_ctz(lo, hi, 64)) \
    X(CTZ128, 128, UINT64_MAX, cc_bit_builtin_ctz(lo, hi, 128)) \
    X(POPCOUNT8, 8, UINT64_MAX >> 56, popcount_64(lo) + popcount_64(hi)) \
    X(POPCOUNT16, 16, UINT64_MAX >> 48, popcount_64(lo) + popcount_64(hi)) \
    X(POPCOUNT32, 32, UINT64_MAX >> 32, popcount_64(lo) + popcount_64(hi)) \
    X(POPCOUNT64, 64, UINT64_MAX >> 0, popcount_64(lo) + popcount_64(hi)) \
    X(POPCOUNT128, 128, UINT64_MAX, popcount_64(lo) + popcount_64(hi)) \
    X(BIT_WIDTH8, 8, UINT64_MAX >> 56, 8 - cc_bit_builtin_clz(lo, hi, 8)) \
    X(BIT_WIDTH16, 16, UINT64_MAX >> 48, 16 - cc_bit_builtin_clz(lo, hi, 16)) \
    X(BIT_WIDTH32, 32, UINT64_MAX >> 32, 32 - cc_bit_builtin_clz(lo, hi, 32)) \
    X(BIT_WIDTH64, 64, UINT64_MAX >> 0, 64 - cc_bit_builtin_clz(lo, hi, 64)) \
    X(BIT_WIDTH128, 128, UINT64_MAX, 128 - cc_bit_builtin_clz(lo, hi, 128)) \
    X(COUNT_ZEROS8, 8, UINT64_MAX >> 56, 8 - (popcount_64(lo) + popcount_64(hi))) \
    X(COUNT_ZEROS16, 16, UINT64_MAX >> 48, 16 - (popcount_64(lo) + popcount_64(hi))) \
    X(COUNT_ZEROS32, 32, UINT64_MAX >> 32, 32 - (popcount_64(lo) + popcount_64(hi))) \
    X(COUNT_ZEROS64, 64, UINT64_MAX >> 0, 64 - (popcount_64(lo) + popcount_64(hi))) \
    X(COUNT_ZEROS128, 128, UINT64_MAX, 128 - (popcount_64(lo) + popcount_64(hi))) \
    X(FIRST_LEADING_ONE8, 8, UINT64_MAX >> 56, (lo || hi) ? cc_bit_builtin_clz(lo, hi, 8) + 1 : 0) \
    X(FIRST_LEADING_ONE16, 16, UINT64_MAX >> 48, (lo || hi) ? cc_bit_builtin_clz(lo, hi, 16) + 1 : 0) \
    X(FIRST_LEADING_ONE32, 32, UINT64_MAX >> 32, (lo || hi) ? cc_bit_builtin_clz(lo, hi, 32) + 1 : 0) \
    X(FIRST_LEADING_ONE64, 64, UINT64_MAX >> 0, (lo || hi) ? cc_bit_builtin_clz(lo, hi, 64) + 1 : 0) \
    X(FIRST_LEADING_ONE128, 128, UINT64_MAX, (lo || hi) ? cc_bit_builtin_clz(lo, hi, 128) + 1 : 0) \
    X(FIRST_LEADING_ZERO8, 8, UINT64_MAX >> 56, cc_bit_builtin_clz(lo ^ (UINT64_MAX >> 56), ~hi, 8) == 8 ? 0 : cc_bit_builtin_clz(lo ^ (UINT64_MAX >> 56), ~hi, 8) + 1) \
    X(FIRST_LEADING_ZERO16, 16, UINT64_MAX >> 48, cc_bit_builtin_clz(lo ^ (UINT64_MAX >> 48), ~hi, 16) == 16 ? 0 : cc_bit_builtin_clz(lo ^ (UINT64_MAX >> 48), ~hi, 16) + 1) \
    X(FIRST_LEADING_ZERO32, 32, UINT64_MAX >> 32, cc_bit_builtin_clz(lo ^ (UINT64_MAX >> 32), ~hi, 32) == 32 ? 0 : cc_bit_builtin_clz(lo ^ (UINT64_MAX >> 32), ~hi, 32) + 1) \
    X(FIRST_LEADING_ZERO64, 64, UINT64_MAX >> 0, cc_bit_builtin_clz(lo ^ (UINT64_MAX >> 0), ~hi, 64) == 64 ? 0 : cc_bit_builtin_clz(lo ^ (UINT64_MAX >> 0), ~hi, 64) + 1) \
    X(FIRST_LEADING_ZERO128, 128, UINT64_MAX, cc_bit_builtin_clz(lo ^ UINT64_MAX, ~hi, 128) == 128 ? 0 : cc_bit_builtin_clz(lo ^ UINT64_MAX, ~hi, 128) + 1) \
    X(FIRST_TRAILING_ZERO8, 8, UINT64_MAX >> 56, cc_bit_builtin_ctz(lo ^ (UINT64_MAX >> 56), ~hi, 8) == 8 ? 0 : cc_bit_builtin_ctz(lo ^ (UINT64_MAX >> 56), ~hi, 8) + 1) \
    X(FIRST_TRAILING_ZERO16, 16, UINT64_MAX >> 48, cc_bit_builtin_ctz(lo ^ (UINT64_MAX >> 48), ~hi, 16) == 16 ? 0 : cc_bit_builtin_ctz(lo ^ (UINT64_MAX >> 48), ~hi, 16) + 1) \
    X(FIRST_TRAILING_ZERO32, 32, UINT64_MAX >> 32, cc_bit_builtin_ctz(lo ^ (UINT64_MAX >> 32), ~hi, 32) == 32 ? 0 : cc_bit_builtin_ctz(lo ^ (UINT64_MAX >> 32), ~hi, 32) + 1) \
    X(FIRST_TRAILING_ZERO64, 64, UINT64_MAX >> 0, cc_bit_builtin_ctz(lo ^ (UINT64_MAX >> 0), ~hi, 64) == 64 ? 0 : cc_bit_builtin_ctz(lo ^ (UINT64_MAX >> 0), ~hi, 64) + 1) \
    X(FIRST_TRAILING_ZERO128, 128, UINT64_MAX, cc_bit_builtin_ctz(lo ^ UINT64_MAX, ~hi, 128) == 128 ? 0 : cc_bit_builtin_ctz(lo ^ UINT64_MAX, ~hi, 128) + 1) \
    X(LEADING_ONES8, 8, UINT64_MAX >> 56, cc_bit_builtin_clz(lo ^ (UINT64_MAX >> 56), ~hi, 8)) \
    X(LEADING_ONES16, 16, UINT64_MAX >> 48, cc_bit_builtin_clz(lo ^ (UINT64_MAX >> 48), ~hi, 16)) \
    X(LEADING_ONES32, 32, UINT64_MAX >> 32, cc_bit_builtin_clz(lo ^ (UINT64_MAX >> 32), ~hi, 32)) \
    X(LEADING_ONES64, 64, UINT64_MAX >> 0, cc_bit_builtin_clz(lo ^ (UINT64_MAX >> 0), ~hi, 64)) \
    X(LEADING_ONES128, 128, UINT64_MAX, cc_bit_builtin_clz(lo ^ UINT64_MAX, ~hi, 128)) \
    X(TRAILING_ONES8, 8, UINT64_MAX >> 56, cc_bit_builtin_ctz(lo ^ (UINT64_MAX >> 56), ~hi, 8)) \
    X(TRAILING_ONES16, 16, UINT64_MAX >> 48, cc_bit_builtin_ctz(lo ^ (UINT64_MAX >> 48), ~hi, 16)) \
    X(TRAILING_ONES32, 32, UINT64_MAX >> 32, cc_bit_builtin_ctz(lo ^ (UINT64_MAX >> 32), ~hi, 32)) \
    X(TRAILING_ONES64, 64, UINT64_MAX >> 0, cc_bit_builtin_ctz(lo ^ (UINT64_MAX >> 0), ~hi, 64)) \
    X(TRAILING_ONES128, 128, UINT64_MAX, cc_bit_builtin_ctz(lo ^ UINT64_MAX, ~hi, 128))

#define CI_SINGLE_BIT_OPS(X) \
    X(HAS_SINGLE_BIT8, 8) \
    X(HAS_SINGLE_BIT16, 16) \
    X(HAS_SINGLE_BIT32, 32) \
    X(HAS_SINGLE_BIT64, 64) \
    X(HAS_SINGLE_BIT128, 128)

#define CI_BIT_CEIL_OPS(X) \
    X(BIT_CEIL8, 8, UINT64_MAX >> 56, if(!hi && lo <= 1) result = ci_uint128_from_uint64(1); else { uint32_t n = 8 - cc_bit_builtin_clz((lo - 1) & (UINT64_MAX >> 56), hi - (lo == 0), 8); if(n < 8) result = cc_bit_builtin_power(n); }) \
    X(BIT_CEIL16, 16, UINT64_MAX >> 48, if(!hi && lo <= 1) result = ci_uint128_from_uint64(1); else { uint32_t n = 16 - cc_bit_builtin_clz((lo - 1) & (UINT64_MAX >> 48), hi - (lo == 0), 16); if(n < 16) result = cc_bit_builtin_power(n); }) \
    X(BIT_CEIL32, 32, UINT64_MAX >> 32, if(!hi && lo <= 1) result = ci_uint128_from_uint64(1); else { uint32_t n = 32 - cc_bit_builtin_clz((lo - 1) & (UINT64_MAX >> 32), hi - (lo == 0), 32); if(n < 32) result = cc_bit_builtin_power(n); }) \
    X(BIT_CEIL64, 64, UINT64_MAX >> 0, if(!hi && lo <= 1) result = ci_uint128_from_uint64(1); else { uint32_t n = 64 - cc_bit_builtin_clz((lo - 1) & (UINT64_MAX >> 0), hi - (lo == 0), 64); if(n < 64) result = cc_bit_builtin_power(n); }) \
    X(BIT_CEIL128, 128, UINT64_MAX, if(!hi && lo <= 1) result = ci_uint128_from_uint64(1); else { uint32_t n = 128 - cc_bit_builtin_clz((lo - 1) & UINT64_MAX, hi - (lo == 0), 128); if(n < 128) result = cc_bit_builtin_power(n); })

#define CI_BIT_FLOOR_OPS(X) \
    X(BIT_FLOOR8, 8, UINT64_MAX >> 56, if(lo || hi) result = cc_bit_builtin_power(8 - cc_bit_builtin_clz(lo, hi, 8) - 1);) \
    X(BIT_FLOOR16, 16, UINT64_MAX >> 48, if(lo || hi) result = cc_bit_builtin_power(16 - cc_bit_builtin_clz(lo, hi, 16) - 1);) \
    X(BIT_FLOOR32, 32, UINT64_MAX >> 32, if(lo || hi) result = cc_bit_builtin_power(32 - cc_bit_builtin_clz(lo, hi, 32) - 1);) \
    X(BIT_FLOOR64, 64, UINT64_MAX >> 0, if(lo || hi) result = cc_bit_builtin_power(64 - cc_bit_builtin_clz(lo, hi, 64) - 1);) \
    X(BIT_FLOOR128, 128, UINT64_MAX, if(lo || hi) result = cc_bit_builtin_power(128 - cc_bit_builtin_clz(lo, hi, 128) - 1);)

#define CI_ROTATE_OPS(X) \
    X(ROTL8, 8, ci_uint128_shl(value, shift), ci_uint128_shr(value, (8 - shift) % 8)) \
    X(ROTR8, 8, ci_uint128_shr(value, shift), ci_uint128_shl(value, (8 - shift) % 8)) \
    X(ROTL16, 16, ci_uint128_shl(value, shift), ci_uint128_shr(value, (16 - shift) % 16)) \
    X(ROTR16, 16, ci_uint128_shr(value, shift), ci_uint128_shl(value, (16 - shift) % 16)) \
    X(ROTL32, 32, ci_uint128_shl(value, shift), ci_uint128_shr(value, (32 - shift) % 32)) \
    X(ROTR32, 32, ci_uint128_shr(value, shift), ci_uint128_shl(value, (32 - shift) % 32)) \
    X(ROTL64, 64, ci_uint128_shl(value, shift), ci_uint128_shr(value, (64 - shift) % 64)) \
    X(ROTR64, 64, ci_uint128_shr(value, shift), ci_uint128_shl(value, (64 - shift) % 64)) \
    X(ROTL128, 128, ci_uint128_shl(value, shift), ci_uint128_shr(value, (128 - shift) % 128)) \
    X(ROTR128, 128, ci_uint128_shr(value, shift), ci_uint128_shl(value, (128 - shift) % 128))

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
    X(VA_ARG_ARM, ) \
    X(VA_ARG_ARM_ALIGN16, go = (go + 15) & ~15;)

#define CI_VA_ARM_INDIRECT_OPS(X) \
    X(VA_ARG_ARM_INDIRECT, 0)

#define CI_VA_STACK_OPS(X) \
    X(VA_ARG_STACK, (void**)ap, ) \
    X(VA_ARG_STACK_INDIRECT, (void**)ap, memcpy(&source, source, sizeof source);) \
    X(VA_ARG_SYSV_STACK, &((NcSysvVaListTag*)ap)->overflow_arg_area, ) \
    X(VA_ARG_ARM_STACK, &((NcAapcs64VaList*)ap)->__stack, )

#endif
