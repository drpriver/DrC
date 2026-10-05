#ifndef C_CC_BIT_BUILTIN_H
#define C_CC_BIT_BUILTIN_H
//
// Copyright © 2026-2026, David Priver <david@davidpriver.com>
//
#include "ci_softnum.h"
#include "../Drp/typed_enum.h"
#include "../Drp/bit_util.h"
#include "../Drp/switch_macros.h"
#include "../Drp/crc32c.h"

enum CcBitBuiltinOp TYPED_ENUM(uint32_t) {
    CC_BIT_FFS,
    CC_BIT_CLRSB,
    CC_BIT_PARITY,
    CC_BIT_CLZ,
    CC_BIT_CTZ,
    CC_BIT_POPCOUNT,
    CC_BIT_BIT_CEIL,
    CC_BIT_BIT_FLOOR,
    CC_BIT_BIT_WIDTH,
    CC_BIT_COUNT_ONES,
    CC_BIT_COUNT_ZEROS,
    CC_BIT_FIRST_LEADING_ONE,
    CC_BIT_FIRST_LEADING_ZERO,
    CC_BIT_FIRST_TRAILING_ONE,
    CC_BIT_FIRST_TRAILING_ZERO,
    CC_BIT_HAS_SINGLE_BIT,
    CC_BIT_LEADING_ONES,
    CC_BIT_LEADING_ZEROS,
    CC_BIT_TRAILING_ONES,
    CC_BIT_TRAILING_ZEROS,
    CC_BIT_ROTATE_LEFT,
    CC_BIT_ROTATE_RIGHT,
    CC_BIT_CRC32C8,
    CC_BIT_CRC32C16,
    CC_BIT_CRC32C32,
    CC_BIT_CRC32C64,
};
TYPEDEF_ENUM(CcBitBuiltinOp, uint32_t);

static inline
const char*
cc_bit_builtin_name(CcBitBuiltinOp op){
    switch(op){
        DRP_CASES_EXHAUSTED;
        case CC_BIT_FFS: return "__builtin_ffsg";
        case CC_BIT_CLRSB: return "__builtin_clrsbg";
        case CC_BIT_PARITY: return "__builtin_parityg";
        case CC_BIT_CLZ: return "__builtin_clzg";
        case CC_BIT_CTZ: return "__builtin_ctzg";
        case CC_BIT_POPCOUNT: return "__builtin_popcountg";
        case CC_BIT_BIT_CEIL: return "__builtin_stdc_bit_ceil";
        case CC_BIT_BIT_FLOOR: return "__builtin_stdc_bit_floor";
        case CC_BIT_BIT_WIDTH: return "__builtin_stdc_bit_width";
        case CC_BIT_COUNT_ONES: return "__builtin_stdc_count_ones";
        case CC_BIT_COUNT_ZEROS: return "__builtin_stdc_count_zeros";
        case CC_BIT_FIRST_LEADING_ONE: return "__builtin_stdc_first_leading_one";
        case CC_BIT_FIRST_LEADING_ZERO: return "__builtin_stdc_first_leading_zero";
        case CC_BIT_FIRST_TRAILING_ONE: return "__builtin_stdc_first_trailing_one";
        case CC_BIT_FIRST_TRAILING_ZERO: return "__builtin_stdc_first_trailing_zero";
        case CC_BIT_HAS_SINGLE_BIT: return "__builtin_stdc_has_single_bit";
        case CC_BIT_LEADING_ONES: return "__builtin_stdc_leading_ones";
        case CC_BIT_LEADING_ZEROS: return "__builtin_stdc_leading_zeros";
        case CC_BIT_TRAILING_ONES: return "__builtin_stdc_trailing_ones";
        case CC_BIT_TRAILING_ZEROS: return "__builtin_stdc_trailing_zeros";
        case CC_BIT_ROTATE_LEFT: return "__builtin_stdc_rotate_left";
        case CC_BIT_ROTATE_RIGHT: return "__builtin_stdc_rotate_right";
        case CC_BIT_CRC32C8: return "__builtin_crc32c8";
        case CC_BIT_CRC32C16: return "__builtin_crc32c16";
        case CC_BIT_CRC32C32: return "__builtin_crc32c32";
        case CC_BIT_CRC32C64: return "__builtin_crc32c64";
    }
    return "<invalid bit builtin>";
}

static inline
uint32_t
cc_bit_builtin_clz(uint64_t lo, uint64_t hi, uint32_t width){
    if(width <= 64) return lo ? (uint32_t)clz_64(lo)-(64-width) : width;
    return hi ? (uint32_t)clz_64(hi) : lo ? 64+(uint32_t)clz_64(lo) : 128;
}

static inline
uint32_t
cc_bit_builtin_ctz(uint64_t lo, uint64_t hi, uint32_t width){
    if(width <= 64) return lo ? (uint32_t)ctz_64(lo) : width;
    return lo ? (uint32_t)ctz_64(lo) : hi ? 64+(uint32_t)ctz_64(hi) : 128;
}

static inline
CiUint128
cc_bit_builtin_words(uint64_t lo, uint64_t hi){
    return ci_uint128_or(ci_uint128_from_uint64(lo), ci_uint128_shl(ci_uint128_from_uint64(hi), 64));
}

static inline
CiUint128
cc_bit_builtin_power(uint32_t bit){
    return bit < 64 ? ci_uint128_from_uint64((uint64_t)1 << bit) : cc_bit_builtin_words(0, (uint64_t)1 << (bit-64));
}

static inline
_Bool
cc_bit_builtin(CcBitBuiltinOp op, CiUint128 v, uint32_t width, CiUint128 arg, _Bool has_arg, CiUint128* result){
    uint64_t mask = width < 64 ? ((uint64_t)1 << width)-1 : ~(uint64_t)0;
    uint64_t lo = ci_uint128_lo(v) & mask,
             hi = width > 64 ? ci_uint128_hi(v) : 0;
    _Bool nonzero = (lo | hi) != 0;
    uint32_t n = 0;
    *result = ci_uint128_from_uint64(0);
    switch(op){
        case CC_BIT_CRC32C8: case CC_BIT_CRC32C16: case CC_BIT_CRC32C32: case CC_BIT_CRC32C64:
            n = drp_crc32c((uint32_t)lo, ci_uint128_lo(arg), 1u << (op - CC_BIT_CRC32C8));
            break;
        case CC_BIT_FFS:
            n = nonzero ? cc_bit_builtin_ctz(lo, hi, width)+1 : 0;
            break;
        case CC_BIT_CLRSB: {
            _Bool sign = width <= 64 ? (lo >> (width-1)) & 1 : hi >> 63;
            n = sign ? cc_bit_builtin_clz(lo ^ mask, ~hi, width)-1 : cc_bit_builtin_clz(lo, hi, width)-1;
            break;
        }
        case CC_BIT_PARITY: case CC_BIT_POPCOUNT: case CC_BIT_COUNT_ONES: case CC_BIT_COUNT_ZEROS: case CC_BIT_HAS_SINGLE_BIT:
            n = (uint32_t)popcount_64(lo);
            if(width > 64) n += (uint32_t)popcount_64(hi);
            if(op == CC_BIT_PARITY) n &= 1;
            else if(op == CC_BIT_COUNT_ZEROS) n = width-n;
            else if(op == CC_BIT_HAS_SINGLE_BIT) n = n == 1;
            break;
        case CC_BIT_CLZ: case CC_BIT_CTZ:
            if(!nonzero){
                if(!has_arg){
                    *result = ci_uint128_from_uint64(width);
                    return 0;
                }
                *result = arg;
                return 1;
            }
            n = op == CC_BIT_CLZ ? cc_bit_builtin_clz(lo, hi, width) : cc_bit_builtin_ctz(lo, hi, width);
            break;
        case CC_BIT_BIT_WIDTH:
            n = width-cc_bit_builtin_clz(lo, hi, width);
            break;
        case CC_BIT_BIT_CEIL:
            if(!hi && lo <= 1){ *result = ci_uint128_from_uint64(1); return 1; }
            n = width-cc_bit_builtin_clz((lo-1) & mask, hi-(lo == 0), width);
            if(n >= width) return 0;
            *result = cc_bit_builtin_power(n);
            return 1;
        case CC_BIT_BIT_FLOOR:
            if(nonzero) *result = cc_bit_builtin_power(width-cc_bit_builtin_clz(lo, hi, width)-1);
            return 1;
        case CC_BIT_FIRST_LEADING_ONE:
            n = nonzero ? cc_bit_builtin_clz(lo, hi, width)+1 : 0;
            break;
        case CC_BIT_FIRST_LEADING_ZERO:
            n = cc_bit_builtin_clz(lo ^ mask, ~hi, width);
            n = n == width ? 0 : n+1;
            break;
        case CC_BIT_FIRST_TRAILING_ONE:
            n = nonzero ? cc_bit_builtin_ctz(lo, hi, width)+1 : 0;
            break;
        case CC_BIT_FIRST_TRAILING_ZERO:
            n = cc_bit_builtin_ctz(lo ^ mask, ~hi, width);
            n = n == width ? 0 : n+1;
            break;
        case CC_BIT_LEADING_ONES:
            n = cc_bit_builtin_clz(lo ^ mask, ~hi, width);
            break;
        case CC_BIT_LEADING_ZEROS:
            n = cc_bit_builtin_clz(lo, hi, width);
            break;
        case CC_BIT_TRAILING_ONES:
            n = cc_bit_builtin_ctz(lo ^ mask, ~hi, width);
            break;
        case CC_BIT_TRAILING_ZEROS:
            n = cc_bit_builtin_ctz(lo, hi, width);
            break;
        case CC_BIT_ROTATE_LEFT: case CC_BIT_ROTATE_RIGHT:
            n = (uint32_t)(ci_uint128_lo(arg) % width);
            if(!n){
                *result = width <= 64 ? ci_uint128_from_uint64(lo) : cc_bit_builtin_words(lo, hi);
                return 1;
            }
            if(op == CC_BIT_ROTATE_RIGHT) n = width-n;
            if(width <= 64){
                *result = ci_uint128_from_uint64(((lo << n) | (lo >> (width-n))) & mask);
                return 1;
            }
            if(n >= 64){
                uint64_t swap = lo; lo = hi; hi = swap;
                n -= 64;
            }
            if(!n) *result = cc_bit_builtin_words(lo, hi);
            else *result = cc_bit_builtin_words((lo << n) | (hi >> (64-n)), (hi << n) | (lo >> (64-n)));
            return 1;
    }
    *result = ci_uint128_from_uint64(n);
    return 1;
}
#endif
