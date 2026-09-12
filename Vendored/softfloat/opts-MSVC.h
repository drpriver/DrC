#ifndef DVM_SOFTFLOAT_OPTS_MSVC_H
#define DVM_SOFTFLOAT_OPTS_MSVC_H

#include <stdint.h>
#if defined(_MSC_VER)
#include <intrin.h>
#endif
#include "SoftFloat-3e/source/include/primitiveTypes.h"

INLINE uint_fast8_t softfloat_countLeadingZeros16(uint16_t a){
    unsigned long bit;
    return _BitScanReverse(&bit, a) ? (uint_fast8_t)(15 - bit) : 16;
}
#define softfloat_countLeadingZeros16 softfloat_countLeadingZeros16

INLINE uint_fast8_t softfloat_countLeadingZeros32(uint32_t a){
    unsigned long bit;
    return _BitScanReverse(&bit, a) ? (uint_fast8_t)(31 - bit) : 32;
}
#define softfloat_countLeadingZeros32 softfloat_countLeadingZeros32

INLINE uint_fast8_t softfloat_countLeadingZeros64(uint64_t a){
    unsigned long bit;
    return _BitScanReverse64(&bit, a) ? (uint_fast8_t)(63 - bit) : 64;
}
#define softfloat_countLeadingZeros64 softfloat_countLeadingZeros64

// x64 _umul128 returns the low word and writes the high word.
INLINE struct uint128 softfloat_mul64To128(uint64_t a, uint64_t b){
    struct uint128 z;
    z.v0 = _umul128(a, b, &z.v64);
    return z;
}
#define softfloat_mul64To128 softfloat_mul64To128

INLINE struct uint128 softfloat_mul64ByShifted32To128(uint64_t a, uint32_t b){
    return softfloat_mul64To128(a, (uint64_t)b << 32);
}
#define softfloat_mul64ByShifted32To128 softfloat_mul64ByShifted32To128

INLINE struct uint128 softfloat_mul128By32(uint64_t a64, uint64_t a0, uint32_t b){
    struct uint128 z = softfloat_mul64To128(a0, b);
    z.v64 += a64 * b;
    return z;
}
#define softfloat_mul128By32 softfloat_mul128By32

#endif
