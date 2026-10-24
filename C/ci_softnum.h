#ifndef C_CI_SOFTNUM_H
#define C_CI_SOFTNUM_H
//
// Copyright © 2026-2026, David Priver <david@davidpriver.com>
//
// Software implementations of potentially missing hardware
// types for ctfe and interpreter, etc.
//

#include "../Drp/bit_util.h"

static inline _Bool ci_narrow_float_nonzero(uint16_t bits){
    return (bits & UINT16_C(0x7fff)) != 0;
}

static inline
uint64_t
ci_int64_divrem(int64_t a, int64_t b, _Bool remainder){
    uint64_t au = (uint64_t)a, bu = (uint64_t)b;
    if(a < 0) au = (uint64_t)0 - au;
    if(b < 0) bu = (uint64_t)0 - bu;
    if(!bu) return 0;
    uint64_t result = remainder? au % bu : au / bu;
    return ((a < 0) != (!remainder && b < 0))? (uint64_t)0 - result : result;
}

static inline
uint64_t
ci_lsl64(uint64_t v, uint64_t amount){
    return amount < 64? v << amount : 0;
}
static inline
uint64_t
ci_lsr64(uint64_t v, uint64_t amount){
    return amount < 64? v >> amount : 0;
}
static inline
int64_t
ci_asr64(int64_t v, uint64_t amount){
    if(v >= 0) return (int64_t)ci_lsr64((uint64_t)v, amount);
    return (int64_t)~ci_lsr64(~(uint64_t)v, amount);
}

// 128-bit unsigned integer
#ifdef __SIZEOF_INT128__
typedef unsigned __int128 CiUint128;
static inline CiUint128 ci_uint128_from_uint64(uint64_t v){ return (CiUint128)v; }
static inline CiUint128 ci_uint128_from_int64(int64_t v){ return (CiUint128)v; }
static inline uint64_t ci_uint128_lo(CiUint128 a){ return (uint64_t)a; }
static inline uint64_t ci_uint128_hi(CiUint128 a){ return (uint64_t)(a >> 64); }
static inline CiUint128 ci_uint128_add(CiUint128 a, CiUint128 b){ return a + b; }
static inline CiUint128 ci_uint128_sub(CiUint128 a, CiUint128 b){ return a - b; }
static inline CiUint128 ci_uint128_mul(CiUint128 a, CiUint128 b){ return a * b; }
static inline CiUint128 ci_uint128_div(CiUint128 a, CiUint128 b){ return b? a / b : 0; }
static inline CiUint128 ci_uint128_mod(CiUint128 a, CiUint128 b){ return b? a % b : 0; }
static inline CiUint128 ci_uint128_and(CiUint128 a, CiUint128 b){ return a & b; }
static inline CiUint128 ci_uint128_or(CiUint128 a, CiUint128 b){ return a | b; }
static inline CiUint128 ci_uint128_xor(CiUint128 a, CiUint128 b){ return a ^ b; }
static inline CiUint128 ci_uint128_shl(CiUint128 a, uint64_t b){ return b < 128? a << b : 0; }
static inline CiUint128 ci_uint128_shr(CiUint128 a, uint64_t b){ return b < 128? a >> b : 0; }
static inline CiUint128 ci_uint128_not(CiUint128 a){ return ~a; }
static inline _Bool ci_uint128_eq(CiUint128 a, CiUint128 b){ return a == b; }
static inline _Bool ci_uint128_ne(CiUint128 a, CiUint128 b){ return a != b; }
static inline _Bool ci_uint128_lt(CiUint128 a, CiUint128 b){ return a < b; }
static inline _Bool ci_uint128_gt(CiUint128 a, CiUint128 b){ return a > b; }
static inline _Bool ci_uint128_le(CiUint128 a, CiUint128 b){ return a <= b; }
static inline _Bool ci_uint128_ge(CiUint128 a, CiUint128 b){ return a >= b; }
static inline _Bool ci_uint128_nonzero(CiUint128 a){ return a != 0; }
static inline void ci_uint128_read(CiUint128* out, const void* buf, uint32_t sz){
    *out = 0;
    memcpy(out, buf, sz <= 16? sz : 16);
}
static inline void ci_uint128_write(void* buf, uint32_t sz, CiUint128 v){
    memcpy(buf, &v, sz <= 16? sz : 16);
}
#else
typedef struct { _Alignas(16) uint64_t lo; uint64_t hi; } CiUint128;
_Static_assert(sizeof(CiUint128) == 16, "CiUint128 must match the VM integer representation");
static inline CiUint128 ci_uint128_from_uint64(uint64_t v){
    return (CiUint128){.lo = v, .hi = 0};
}
static inline CiUint128 ci_uint128_from_int64(int64_t v){
    return (CiUint128){.lo = (uint64_t)v, .hi = v < 0? ~(uint64_t)0 : 0};
}
static inline uint64_t ci_uint128_lo(CiUint128 a){ return a.lo; }
static inline uint64_t ci_uint128_hi(CiUint128 a){ return a.hi; }
static inline CiUint128 ci_uint128_add(CiUint128 a, CiUint128 b){
    uint64_t lo = a.lo + b.lo;
    return (CiUint128){lo, a.hi + b.hi + (lo < a.lo)};
}
static inline CiUint128 ci_uint128_sub(CiUint128 a, CiUint128 b){
    uint64_t lo = a.lo - b.lo;
    return (CiUint128){lo, a.hi - b.hi - (a.lo < b.lo)};
}
static inline
CiUint128
ci_uint128_mul(CiUint128 a, CiUint128 b){
#if defined _MSC_VER && defined _M_X64 && !defined _M_ARM64EC
    uint64_t high;
    uint64_t low = _umul128(a.lo, b.lo, &high);
    return (CiUint128){low, high + a.lo * b.hi + a.hi * b.lo};
#elif defined _MSC_VER && (defined _M_ARM64 || defined _M_ARM64EC)
    return (CiUint128){a.lo * b.lo, __umulh(a.lo, b.lo) + a.lo * b.hi + a.hi * b.lo};
#else
    uint64_t al = a.lo & 0xFFFFFFFF, ah = a.lo >> 32;
    uint64_t bl = b.lo & 0xFFFFFFFF, bh = b.lo >> 32;
    uint64_t ll = al * bl;
    uint64_t lh = al * bh;
    uint64_t hl = ah * bl;
    uint64_t hh = ah * bh;
    uint64_t mid = (ll >> 32) + (lh & 0xFFFFFFFF) + (hl & 0xFFFFFFFF);
    return (CiUint128){
        .lo = (ll & 0xFFFFFFFF) | (mid << 32),
        .hi = hh + (lh >> 32) + (hl >> 32) + (mid >> 32) + a.lo * b.hi + a.hi * b.lo,
    };
#endif
}
static inline CiUint128 ci_uint128_and(CiUint128 a, CiUint128 b){
    return (CiUint128){a.lo & b.lo, a.hi & b.hi};
}
static inline CiUint128 ci_uint128_or(CiUint128 a, CiUint128 b){
    return (CiUint128){a.lo | b.lo, a.hi | b.hi};
}
static inline CiUint128 ci_uint128_xor(CiUint128 a, CiUint128 b){
    return (CiUint128){a.lo ^ b.lo, a.hi ^ b.hi};
}
static inline CiUint128 ci_uint128_shl(CiUint128 a, uint64_t b){
    if(b >= 128) return (CiUint128){0, 0};
    if(b >= 64) return (CiUint128){0, a.lo << (b - 64)};
    if(b == 0) return a;
    return (CiUint128){a.lo << b, (a.hi << b) | (a.lo >> (64 - b))};
}
static inline CiUint128 ci_uint128_shr(CiUint128 a, uint64_t b){
    if(b >= 128) return (CiUint128){0, 0};
    if(b >= 64) return (CiUint128){a.hi >> (b - 64), 0};
    if(b == 0) return a;
    return (CiUint128){(a.lo >> b) | (a.hi << (64 - b)), a.hi >> b};
}
static inline CiUint128 ci_uint128_not(CiUint128 a){ return (CiUint128){~a.lo, ~a.hi}; }
static inline _Bool ci_uint128_eq(CiUint128 a, CiUint128 b){ return a.lo == b.lo && a.hi == b.hi; }
static inline _Bool ci_uint128_ne(CiUint128 a, CiUint128 b){ return a.lo != b.lo || a.hi != b.hi; }
static inline _Bool ci_uint128_lt(CiUint128 a, CiUint128 b){ return a.hi < b.hi || (a.hi == b.hi && a.lo < b.lo); }
static inline _Bool ci_uint128_gt(CiUint128 a, CiUint128 b){ return ci_uint128_lt(b, a); }
static inline _Bool ci_uint128_le(CiUint128 a, CiUint128 b){ return !ci_uint128_gt(a, b); }
static inline _Bool ci_uint128_ge(CiUint128 a, CiUint128 b){ return !ci_uint128_lt(a, b); }
static inline _Bool ci_uint128_nonzero(CiUint128 a){ return a.lo || a.hi; }
static inline CiUint128 ci_uint128_div(CiUint128 a, CiUint128 b){
    if(!ci_uint128_nonzero(b)) return (CiUint128){0, 0};
    if(!b.hi && !a.hi) return (CiUint128){a.lo / b.lo, 0};
    // Long division
    CiUint128 q = {0, 0}, r = {0, 0};
    for(int i = 127; i >= 0; i--){
        r = ci_uint128_shl(r, 1);
        r.lo |= (i >= 64? (a.hi >> (i - 64)) : (a.lo >> i)) & 1;
        if(ci_uint128_ge(r, b)){
            r = ci_uint128_sub(r, b);
            if(i >= 64) q.hi |= (uint64_t)1 << (i - 64);
            else q.lo |= (uint64_t)1 << i;
        }
    }
    return q;
}
static inline CiUint128 ci_uint128_mod(CiUint128 a, CiUint128 b){
    if(!ci_uint128_nonzero(b)) return ci_uint128_from_uint64(0);
    CiUint128 q = ci_uint128_div(a, b);
    return ci_uint128_sub(a, ci_uint128_mul(q, b));
}
static inline void ci_uint128_read(CiUint128* out, const void* buf, uint32_t sz){
    *out = (CiUint128){0, 0};
    memcpy(out, buf, sz <= 16? sz : 16);
}
static inline void ci_uint128_write(void* buf, uint32_t sz, CiUint128 v){
    memcpy(buf, &v, sz <= 16? sz : 16);
}
#endif

static inline
_Bool
ci_uint128_fits_integer(CiUint128 v, unsigned width, _Bool is_unsigned){
    if(width >= 128) return 1;
    CiUint128 mask = ci_uint128_sub(ci_uint128_shl(ci_uint128_from_uint64(1), width), ci_uint128_from_uint64(1));
    CiUint128 narrowed = ci_uint128_and(v, mask);
    if(!is_unsigned && ci_uint128_nonzero(ci_uint128_shr(narrowed, width-1)))
        narrowed = ci_uint128_or(narrowed, ci_uint128_not(mask));
    return ci_uint128_eq(v, narrowed);
}

static inline
_Bool
ci_uint128_magnitude_fits_integer(CiUint128 magnitude, _Bool negative, unsigned width, _Bool is_unsigned){
    if(is_unsigned){
        if(negative && ci_uint128_nonzero(magnitude)) return 0;
        return width >= 128 || !ci_uint128_nonzero(ci_uint128_shr(magnitude, width));
    }
    CiUint128 limit = ci_uint128_shl(ci_uint128_from_uint64(1), width-1);
    return negative? ci_uint128_le(magnitude, limit) : ci_uint128_lt(magnitude, limit);
}

#ifndef __SIZEOF_INT128__
static inline
int64_t
ci_uint128_float_significand(CiUint128 v, _Bool is_unsigned, uint32_t* shift){
    _Bool negative = !is_unsigned && (ci_uint128_hi(v) >> 63);
    if(negative) v = ci_uint128_sub(ci_uint128_from_uint64(0), v);
    uint64_t hi = ci_uint128_hi(v), lo = ci_uint128_lo(v);
    *shift = hi? (uint32_t)(65 - clz_64(hi)) : (uint32_t)(lo >> 63);
    CiUint128 reduced = ci_uint128_shr(v, *shift);
    uint64_t n = ci_uint128_lo(reduced);
    if(!ci_uint128_eq(v, ci_uint128_shl(reduced, *shift))) n |= 1;
    return negative? -(int64_t)n : (int64_t)n;
}
#endif

static inline
float
ci_uint128_to_float(CiUint128 v, _Bool is_unsigned){
#ifdef __SIZEOF_INT128__
    return is_unsigned? (float)v : (float)(__int128)v;
#else
    uint32_t shift;
    float f = (float)ci_uint128_float_significand(v, is_unsigned, &shift);
    uint32_t scale_bits = (127u + shift) << 23;
    float scale;
    memcpy(&scale, &scale_bits, sizeof scale);
    return f * scale;
#endif
}

static inline double ci_uint128_to_double(CiUint128 v, _Bool is_unsigned){
#ifdef __SIZEOF_INT128__
    return is_unsigned? (double)v : (double)(__int128)v;
#else
    uint32_t shift;
    double d = (double)ci_uint128_float_significand(v, is_unsigned, &shift);
    uint64_t scale_bits = (uint64_t)(1023u + shift) << 52;
    double scale;
    memcpy(&scale, &scale_bits, sizeof scale);
    return d * scale;
#endif
}

static inline
_Bool
ci_double_integer_fits(double d, double signed_limit, _Bool is_unsigned){
    if(is_unsigned)
        return d > -1.0 && d < signed_limit * 2.0;
    else
        return (d >= -signed_limit || d > -signed_limit - 1.0) && d < signed_limit;
}
static inline uint64_t ci_uint64_from_double(double d, _Bool is_unsigned){
    if(!ci_double_integer_fits(d, 0x1p63, is_unsigned)) return 0;
    return is_unsigned? (uint64_t)d : (uint64_t)(int64_t)d;
}
static inline CiUint128 ci_uint128_from_double(double d, _Bool is_unsigned){
    if(!ci_double_integer_fits(d, 0x1p127, is_unsigned)) return ci_uint128_from_uint64(0);
#ifdef __SIZEOF_INT128__
    return is_unsigned? (CiUint128)d : (CiUint128)(__int128)d;
#else
    _Bool negative = !is_unsigned && d < 0;
    if(negative) d = -d;
    const double two64 = 18446744073709551616.0;
    uint64_t hi = (uint64_t)(d / two64);
    uint64_t lo = (uint64_t)(d - (double)hi * two64);
    CiUint128 v = ci_uint128_or(ci_uint128_shl(ci_uint128_from_uint64(hi), 64), ci_uint128_from_uint64(lo));
    return negative? ci_uint128_sub(ci_uint128_from_uint64(0), v) : v;
#endif
}

// 128-bit signed integer
#ifdef __SIZEOF_INT128__
typedef __int128 CiInt128;
static inline CiInt128 ci_int128_from_int64(int64_t v){ return (CiInt128)v; }
static inline CiInt128 ci_int128_from_uint64(uint64_t v){ return (CiInt128)v; }
static inline _Bool ci_int128_eq(CiInt128 a, CiInt128 b){ return a == b; }
static inline uint64_t ci_int128_lo(CiInt128 a){ return (uint64_t)a; }
static inline _Bool ci_int128_lt(CiInt128 a, CiInt128 b){ return a < b; }
static inline _Bool ci_int128_gt(CiInt128 a, CiInt128 b){ return a > b; }
static inline _Bool ci_int128_le(CiInt128 a, CiInt128 b){ return a <= b; }
static inline _Bool ci_int128_ge(CiInt128 a, CiInt128 b){ return a >= b; }
static inline CiInt128 ci_int128_shr(CiInt128 a, uint64_t b){
    if(a >= 0) return (CiInt128)ci_uint128_shr((CiUint128)a, b);
    return (CiInt128)~ci_uint128_shr(~(CiUint128)a, b);
}
static inline CiInt128 ci_int128_from_uint128(CiUint128 a){ return (CiInt128)a; }
static inline CiUint128 ci_uint128_from_int128(CiInt128 a){ return (CiUint128)a; }
#else
typedef struct { _Alignas(16) uint64_t lo; int64_t hi; } CiInt128;
static inline CiInt128 ci_int128_from_int64(int64_t v){
    return (CiInt128){.lo = (uint64_t)v, .hi = v < 0? (int64_t)-1 : (int64_t)0};
}
static inline CiInt128 ci_int128_from_uint64(uint64_t v){
    return (CiInt128){.lo = v, .hi = 0};
}
static inline _Bool ci_int128_eq(CiInt128 a, CiInt128 b){
    return a.lo == b.lo && a.hi == b.hi;
}
static inline uint64_t ci_int128_lo(CiInt128 a){ return a.lo; }
static inline CiInt128 ci_int128_from_uint128(CiUint128 a){
    return (CiInt128){.lo = a.lo, .hi = (int64_t)a.hi};
}
static inline CiUint128 ci_uint128_from_int128(CiInt128 a){
    return (CiUint128){.lo = a.lo, .hi = (uint64_t)a.hi};
}
static inline _Bool ci_int128_lt(CiInt128 a, CiInt128 b){
    return a.hi < b.hi || (a.hi == b.hi && a.lo < b.lo);
}
static inline _Bool ci_int128_gt(CiInt128 a, CiInt128 b){ return ci_int128_lt(b, a); }
static inline _Bool ci_int128_le(CiInt128 a, CiInt128 b){ return !ci_int128_gt(a, b); }
static inline _Bool ci_int128_ge(CiInt128 a, CiInt128 b){ return !ci_int128_lt(a, b); }
static inline CiInt128 ci_int128_shr(CiInt128 a, uint64_t b){
    int64_t sign = a.hi < 0? (int64_t)-1 : (int64_t)0;
    if(b >= 128) return (CiInt128){(uint64_t)sign, sign};
    if(b >= 64) return (CiInt128){(uint64_t)ci_asr64(a.hi, b - 64), sign};
    if(b == 0) return a;
    return (CiInt128){(a.lo >> b) | ((uint64_t)a.hi << (64 - b)), ci_asr64(a.hi, b)};
}
#endif

static inline CiInt128 ci_int128_add(CiInt128 a, CiInt128 b){
    return ci_int128_from_uint128(ci_uint128_add(ci_uint128_from_int128(a), ci_uint128_from_int128(b)));
}
static inline CiInt128 ci_int128_sub(CiInt128 a, CiInt128 b){
    return ci_int128_from_uint128(ci_uint128_sub(ci_uint128_from_int128(a), ci_uint128_from_int128(b)));
}
static inline CiInt128 ci_int128_neg(CiInt128 a){
    return ci_int128_from_uint128(ci_uint128_sub(ci_uint128_from_uint64(0), ci_uint128_from_int128(a)));
}

static inline CiInt128 ci_int128_divrem(CiInt128 a, CiInt128 b, _Bool remainder){
    CiInt128 zero = ci_int128_from_int64(0);
    if(ci_int128_eq(b, zero)) return zero;
    _Bool a_neg = ci_int128_lt(a, zero);
    _Bool b_neg = ci_int128_lt(b, zero);
    CiUint128 au = a_neg? ci_uint128_from_int128(ci_int128_neg(a)) : ci_uint128_from_int128(a);
    CiUint128 bu = b_neg? ci_uint128_from_int128(ci_int128_neg(b)) : ci_uint128_from_int128(b);
    CiInt128 result = ci_int128_from_uint128(remainder? ci_uint128_mod(au, bu) : ci_uint128_div(au, bu));
    return (a_neg != (!remainder && b_neg))? ci_int128_neg(result) : result;
}
static inline CiInt128 ci_int128_div(CiInt128 a, CiInt128 b){
    return ci_int128_divrem(a, b, 0);
}
static inline CiInt128 ci_int128_mod(CiInt128 a, CiInt128 b){
    return ci_int128_divrem(a, b, 1);
}

typedef union CiIEE754Float16 CiIEE754Float16;
union CiIEE754Float16 {
    #if defined __FLT16_MANT_DIG__
    _Float16 f16;
    #endif
    uint16_t u;
    struct {
        #if defined __BYTE_ORDER__ && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
        uint32_t sign: 1,
                 exponent: 5,
                 fraction: 10;
        #else // Little-Endian
        uint32_t fraction: 10,
                 exponent: 5,
                 sign: 1;
        #endif
    };
    struct {
        #if defined __BYTE_ORDER__ && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
        uint32_t _sign: 1,
                 magnitude: 15;
        #else // Little-Endian
        uint32_t magnitude: 15,
                 _sign: 1;
        #endif
    };
};

typedef union CiIEE754Float32 CiIEE754Float32;
union CiIEE754Float32 {
    _Static_assert(sizeof(float) == 4, "");
    float f;
    #if 0 // TODO: feature check
    _Float32 f32;
    #endif
    uint32_t u;
    struct {
        #if defined __BYTE_ORDER__ && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
        uint32_t sign: 1,
                 exponent: 8,
                 fraction: 23;
        #else // Little-Endian
        uint32_t fraction: 23,
                 exponent: 8,
                 sign: 1;
        #endif
    };
    struct {
        #if defined __BYTE_ORDER__ && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
        uint32_t _sign: 1,
                 magnitude: 31;
        #else // Little-Endian
        uint32_t magnitude: 31,
                 _sign: 1;
        #endif
    };
};
typedef union CiIEE754Float64 CiIEE754Float64;
union CiIEE754Float64 {
    _Static_assert(sizeof(double) == 8, "");
    double d;
    #if 0 // TODO: feature check
    _Float64 f64;
    #endif
    uint64_t u;
    struct {
        #if defined __BYTE_ORDER__ && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
        uint64_t sign: 1,
                 exponent: 11,
                 fraction: 52;
        #else // Little-Endian
        uint64_t fraction: 52,
                 exponent: 11,
                 sign: 1;
        #endif
    };
    struct {
        #if defined __BYTE_ORDER__ && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
        uint64_t _sign: 1,
                 magnitude: 63;
        #else // Little-Endian
        uint64_t magnitude: 63,
                 _sign: 1;
        #endif
    };
};

typedef union CiIEE754Float128 CiIEE754Float128;
union CiIEE754Float128 {
    #if defined __FLT128_MANT_DIG__
    _Float128 f128;
    #endif
    CiUint128 u;
    #if defined __SIZEOF_INT128__ && !defined __DRC__ // 128-bit bitfields unsupported
    struct {
        #if defined __BYTE_ORDER__ && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
        CiUint128 sign: 1,
                 exponent: 15,
                 fraction: 112;
        #else // Little-Endian
        CiUint128 fraction: 112,
                 exponent: 15,
                 sign: 1;
        #endif
    };
    struct {
        #if defined __BYTE_ORDER__ && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
        CiUint128 _sign: 1,
                 magnitude: 127;
        #else // Little-Endian
        CiUint128 magnitude: 127,
                 _sign: 1;
        #endif
    };
    #endif
};

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wimplicit-int-conversion"
#endif
#include "../Vendored/softfloat/softfloat.h"
#ifdef __clang__
#pragma clang diagnostic pop
#endif

#if !defined(_WIN32) && defined __LDBL_MANT_DIG__ && __LDBL_MANT_DIG__ == 64 && __LDBL_MAX_EXP__ == 16384
#define CI_NATIVE_FLOAT80 1
typedef long double CiFloat80;
#else
typedef extFloat80_t CiFloat80;
#endif
static inline CiFloat80 ci_float80_read(const void* buf){ CiFloat80 r = {0}; memcpy(&r, buf, 10); return r; }
static inline void ci_float80_write(void* buf, uint32_t sz, CiFloat80 v){ memset(buf, 0, sz); memcpy(buf, &v, 10); }
static inline CiFloat80 ci_float80_add(CiFloat80 a, CiFloat80 b){
#ifdef CI_NATIVE_FLOAT80
    return a + b;
#else
    CiFloat80 r; extF80M_add(&a, &b, &r); return r;
#endif
}
static inline CiFloat80 ci_float80_sub(CiFloat80 a, CiFloat80 b){
#ifdef CI_NATIVE_FLOAT80
    return a - b;
#else
    CiFloat80 r; extF80M_sub(&a, &b, &r); return r;
#endif
}
static inline CiFloat80 ci_float80_mul(CiFloat80 a, CiFloat80 b){
#ifdef CI_NATIVE_FLOAT80
    return a * b;
#else
    CiFloat80 r; extF80M_mul(&a, &b, &r); return r;
#endif
}
static inline CiFloat80 ci_float80_div(CiFloat80 a, CiFloat80 b){
#ifdef CI_NATIVE_FLOAT80
    return a / b;
#else
    CiFloat80 r; extF80M_div(&a, &b, &r); return r;
#endif
}
static inline _Bool ci_float80_eq(CiFloat80 a, CiFloat80 b){
#ifdef CI_NATIVE_FLOAT80
    return a == b;
#else
    return extF80M_eq(&a, &b);
#endif
}
static inline _Bool ci_float80_lt(CiFloat80 a, CiFloat80 b){
#ifdef CI_NATIVE_FLOAT80
    return a < b;
#else
    return extF80M_lt(&a, &b);
#endif
}
static inline _Bool ci_float80_le(CiFloat80 a, CiFloat80 b){
#ifdef CI_NATIVE_FLOAT80
    return a <= b;
#else
    return extF80M_le(&a, &b);
#endif
}
static inline CiFloat80 ci_float80_neg(CiFloat80 a){
#ifdef CI_NATIVE_FLOAT80
    return -a;
#else
    a.signExp ^= 0x8000;
    return a;
#endif
}
static inline CiFloat80 ci_float80_from_uint64(uint64_t v){
#ifdef CI_NATIVE_FLOAT80
    return (CiFloat80)v;
#else
    CiFloat80 r; ui64_to_extF80M(v, &r); return r;
#endif
}
static inline uint64_t ci_float80_to_uint64(CiFloat80 v){
#ifdef CI_NATIVE_FLOAT80
    return (uint64_t)v;
#else
    return extF80M_to_ui64_r_minMag(&v, false);
#endif
}
static inline CiFloat80 ci_float80_from_int64(int64_t v){
#ifdef CI_NATIVE_FLOAT80
    return (CiFloat80)v;
#else
    CiFloat80 r; i64_to_extF80M(v, &r); return r;
#endif
}
static inline int64_t ci_float80_to_int64(CiFloat80 v){
#ifdef CI_NATIVE_FLOAT80
    return (int64_t)v;
#else
    return extF80M_to_i64_r_minMag(&v, false);
#endif
}
static inline CiFloat80 ci_float80_from_float(float v){
#ifdef CI_NATIVE_FLOAT80
    return (CiFloat80)v;
#else
    float32_t a; memcpy(&a, &v, sizeof a); CiFloat80 r; f32_to_extF80M(a, &r); return r;
#endif
}
static inline float ci_float80_to_float(CiFloat80 v){
#ifdef CI_NATIVE_FLOAT80
    return (float)v;
#else
    float32_t a = extF80M_to_f32(&v); float r; memcpy(&r, &a, sizeof r); return r;
#endif
}
static inline CiFloat80 ci_float80_from_double(double v){
#ifdef CI_NATIVE_FLOAT80
    return (CiFloat80)v;
#else
    float64_t a; memcpy(&a, &v, sizeof a); CiFloat80 r; f64_to_extF80M(a, &r); return r;
#endif
}
static inline double ci_float80_to_double(CiFloat80 v){
#ifdef CI_NATIVE_FLOAT80
    return (double)v;
#else
    float64_t a = extF80M_to_f64(&v); double r; memcpy(&r, &a, sizeof r); return r;
#endif
}
static inline _Bool ci_float80_nonzero(CiFloat80 a){ return !ci_float80_eq(a, ci_float80_from_int64(0)); }

#if defined __LDBL_MANT_DIG__ && __LDBL_MANT_DIG__ == 113 && __LDBL_MAX_EXP__ == 16384
#define CI_NATIVE_FLOAT128 1
typedef long double CiFloat128;
#elif defined __SIZEOF_FLOAT128__ && (defined __GNUC__ || defined __clang__ || defined __DRC__)
#define CI_NATIVE_FLOAT128 1
typedef __float128 CiFloat128;
#else
typedef float128_t CiFloat128;
#endif
static inline CiFloat128 ci_float128_read(const void* buf){ CiFloat128 r = {0}; memcpy(&r, buf, 16); return r; }
static inline void ci_float128_write(void* buf, uint32_t sz, CiFloat128 v){ memset(buf, 0, sz); memcpy(buf, &v, 16); }
static inline CiFloat128 ci_float128_add(CiFloat128 a, CiFloat128 b){
#ifdef CI_NATIVE_FLOAT128
    return a + b;
#else
    CiFloat128 r; f128M_add(&a, &b, &r); return r;
#endif
}
static inline CiFloat128 ci_float128_sub(CiFloat128 a, CiFloat128 b){
#ifdef CI_NATIVE_FLOAT128
    return a - b;
#else
    CiFloat128 r; f128M_sub(&a, &b, &r); return r;
#endif
}
static inline CiFloat128 ci_float128_mul(CiFloat128 a, CiFloat128 b){
#ifdef CI_NATIVE_FLOAT128
    return a * b;
#else
    CiFloat128 r; f128M_mul(&a, &b, &r); return r;
#endif
}
static inline CiFloat128 ci_float128_div(CiFloat128 a, CiFloat128 b){
#ifdef CI_NATIVE_FLOAT128
    return a / b;
#else
    CiFloat128 r; f128M_div(&a, &b, &r); return r;
#endif
}
static inline _Bool ci_float128_eq(CiFloat128 a, CiFloat128 b){
#ifdef CI_NATIVE_FLOAT128
    return a == b;
#else
    return f128M_eq(&a, &b);
#endif
}
static inline _Bool ci_float128_lt(CiFloat128 a, CiFloat128 b){
#ifdef CI_NATIVE_FLOAT128
    return a < b;
#else
    return f128M_lt(&a, &b);
#endif
}
static inline _Bool ci_float128_le(CiFloat128 a, CiFloat128 b){
#ifdef CI_NATIVE_FLOAT128
    return a <= b;
#else
    return f128M_le(&a, &b);
#endif
}
static inline CiFloat128 ci_float128_neg(CiFloat128 a){
#ifdef CI_NATIVE_FLOAT128
    return -a;
#else
    a.v[1] ^= UINT64_C(0x8000000000000000);
    return a;
#endif
}
static inline CiFloat128 ci_float128_from_uint64(uint64_t v){
#ifdef CI_NATIVE_FLOAT128
    return (CiFloat128)v;
#else
    CiFloat128 r; ui64_to_f128M(v, &r); return r;
#endif
}
static inline uint64_t ci_float128_to_uint64(CiFloat128 v){
#ifdef CI_NATIVE_FLOAT128
    return (uint64_t)v;
#else
    return f128M_to_ui64_r_minMag(&v, false);
#endif
}
static inline CiFloat128 ci_float128_from_int64(int64_t v){
#ifdef CI_NATIVE_FLOAT128
    return (CiFloat128)v;
#else
    CiFloat128 r; i64_to_f128M(v, &r); return r;
#endif
}
static inline int64_t ci_float128_to_int64(CiFloat128 v){
#ifdef CI_NATIVE_FLOAT128
    return (int64_t)v;
#else
    return f128M_to_i64_r_minMag(&v, false);
#endif
}
static inline CiFloat128 ci_float128_from_float(float v){
#ifdef CI_NATIVE_FLOAT128
    return (CiFloat128)v;
#else
    float32_t a; memcpy(&a, &v, sizeof a); CiFloat128 r; f32_to_f128M(a, &r); return r;
#endif
}
static inline float ci_float128_to_float(CiFloat128 v){
#ifdef CI_NATIVE_FLOAT128
    return (float)v;
#else
    float32_t a = f128M_to_f32(&v); float r; memcpy(&r, &a, sizeof r); return r;
#endif
}
static inline CiFloat128 ci_float128_from_double(double v){
#ifdef CI_NATIVE_FLOAT128
    return (CiFloat128)v;
#else
    float64_t a; memcpy(&a, &v, sizeof a); CiFloat128 r; f64_to_f128M(a, &r); return r;
#endif
}
static inline double ci_float128_to_double(CiFloat128 v){
#ifdef CI_NATIVE_FLOAT128
    return (double)v;
#else
    float64_t a = f128M_to_f64(&v); double r; memcpy(&r, &a, sizeof r); return r;
#endif
}
static inline _Bool ci_float128_nonzero(CiFloat128 a){ return !ci_float128_eq(a, ci_float128_from_int64(0)); }

static inline CiFloat128 ci_float128_from_float80(CiFloat80 v){
    extFloat80_t a = {0}; float128_t b;
    memcpy(&a, &v, 10);
    extF80M_to_f128M(&a, &b);
    CiFloat128 r; memcpy(&r, &b, sizeof r); return r;
}
static inline CiFloat80 ci_float80_from_float128(CiFloat128 v){
    float128_t a; extFloat80_t b;
    memcpy(&a, &v, sizeof a);
    f128M_to_extF80M(&a, &b);
    return ci_float80_read(&b);
}

static inline
CiUint128
ci_uint128_round_float(CiUint128 v, _Bool negative, unsigned precision, int* top){
    uint64_t hi = ci_uint128_hi(v), lo = ci_uint128_lo(v);
    *top = hi? 127 - clz_64(hi) : lo? 63 - clz_64(lo) : 0;
    int shift = *top - ((int)precision - 1);
    if(shift <= 0) return ci_uint128_shl(v, -shift);
    CiUint128 sig = ci_uint128_shr(v, shift);
    CiUint128 rem = ci_uint128_sub(v, ci_uint128_shl(sig, shift));
    if(!ci_uint128_nonzero(rem)) return sig;
    softfloat_exceptionFlags |= softfloat_flag_inexact;
    CiUint128 half = ci_uint128_shl(ci_uint128_from_uint64(1), shift - 1);
    _Bool increment = 0;
    switch(softfloat_roundingMode){
        case softfloat_round_near_even:
            increment = ci_uint128_gt(rem, half) || (ci_uint128_eq(rem, half) && (ci_uint128_lo(sig) & 1));
            break;
        case softfloat_round_near_maxMag: increment = ci_uint128_ge(rem, half); break;
        case softfloat_round_min: increment = negative; break;
        case softfloat_round_max: increment = !negative; break;
        case softfloat_round_odd: return ci_uint128_or(sig, ci_uint128_from_uint64(1));
        default: break;
    }
    if(increment){
        sig = ci_uint128_add(sig, ci_uint128_from_uint64(1));
        if(ci_uint128_nonzero(ci_uint128_shr(sig, precision))){
            sig = ci_uint128_shr(sig, 1);
            (*top)++;
        }
    }
    return sig;
}

static inline CiFloat80 ci_float80_from_uint128(CiUint128 v, _Bool uns){
#if defined CI_NATIVE_FLOAT80 && defined __SIZEOF_INT128__
    return uns? (CiFloat80)v : (CiFloat80)(__int128)v;
#else
    _Bool neg = !uns && (ci_uint128_hi(v) >> 63);
    if(neg) v = ci_uint128_sub(ci_uint128_from_uint64(0), v);
    uint64_t raw[2] = {0, 0};
    if(ci_uint128_nonzero(v)){
        int top;
        CiUint128 sig = ci_uint128_round_float(v, neg, 64, &top);
        raw[0] = ci_uint128_lo(sig);
        raw[1] = (uint64_t)(top + 16383);
    }
    raw[1] |= (uint64_t)neg << 15;
    return ci_float80_read(raw);
#endif
}
static inline CiUint128 ci_float80_to_uint128(CiFloat80 v){
    uint64_t raw[2]; ci_float80_write(raw, 16, v);
    int exp = (int)(raw[1] & 0x7fff);
    _Bool neg = (raw[1] & 0x8000) != 0;
    CiUint128 sig = ci_uint128_from_uint64(raw[0]);
    int shift = exp - 16383 - 63;
    if(exp == 0 || exp == 0x7fff || shift <= -128 || shift >= 128) return ci_uint128_from_uint64(0);
    CiUint128 r = shift < 0? ci_uint128_shr(sig, -shift) : ci_uint128_shl(sig, shift);
    return neg? ci_uint128_sub(ci_uint128_from_uint64(0), r) : r;
}

static inline CiFloat128 ci_float128_from_uint128(CiUint128 v, _Bool uns){
#if defined CI_NATIVE_FLOAT128 && defined __SIZEOF_INT128__
    return uns? (CiFloat128)v : (CiFloat128)(__int128)v;
#else
    _Bool neg = !uns && (ci_uint128_hi(v) >> 63);
    if(neg) v = ci_uint128_sub(ci_uint128_from_uint64(0), v);
    uint64_t raw[2] = {0, 0};
    if(ci_uint128_nonzero(v)){
        int top;
        CiUint128 sig = ci_uint128_round_float(v, neg, 113, &top);
        raw[0] = ci_uint128_lo(sig);
        raw[1] = (ci_uint128_hi(sig) & UINT64_C(0x0000ffffffffffff)) | ((uint64_t)(top + 16383) << 48);
    }
    raw[1] |= (uint64_t)neg << 63;
    return ci_float128_read(raw);
#endif
}
static inline CiUint128 ci_float128_to_uint128(CiFloat128 v){
    uint64_t raw[2]; ci_float128_write(raw, 16, v);
    int exp = (int)((raw[1] >> 48) & 0x7fff);
    _Bool neg = (raw[1] >> 63) != 0;
    raw[1] = (raw[1] & UINT64_C(0x0000ffffffffffff)) | (exp? UINT64_C(0x0001000000000000) : 0);
    CiUint128 sig; ci_uint128_read(&sig, raw, 16);
    int shift = exp - 16383 - 112;
    if(exp == 0 || exp == 0x7fff || shift <= -128 || shift >= 128) return ci_uint128_from_uint64(0);
    CiUint128 r = shift < 0? ci_uint128_shr(sig, -shift) : ci_uint128_shl(sig, shift);
    return neg? ci_uint128_sub(ci_uint128_from_uint64(0), r) : r;
}

static inline
uint16_t
ci_bfloat16_pack(_Bool negative, uint64_t significand, int exponent){
    uint16_t sign = negative? 0x8000 : 0;
    if(!significand) return sign;
    int top = 63 - clz_64(significand);
    int e = exponent + top;
    int shift = e < -126? -133 - exponent : top - 7;
    uint64_t retained,
             remainder = 0;
    if(shift <= 0)
        retained = significand << -shift;
    else if(shift < 64){
        retained = significand >> shift;
        remainder = significand & ((UINT64_C(1) << shift) - 1);
    }
    else {
        retained = 0;
        remainder = significand;
    }
    uint_fast8_t mode = softfloat_roundingMode;
    if(remainder){
        uint64_t half = shift <= 64? UINT64_C(1) << (shift - 1) : 0;
        _Bool above = shift <= 64 && remainder > half;
        _Bool tie = shift <= 64 && remainder == half;
        if((mode == softfloat_round_near_even && (above || (tie && (retained & 1))))
        || (mode == softfloat_round_near_maxMag && (above || tie))
        || (mode == softfloat_round_min && negative)
        || (mode == softfloat_round_max && !negative))
        retained++;
        else if(mode == softfloat_round_odd)
            retained |= 1;
    }
    uint32_t result = (uint32_t)(e < -126? 0 : e + 126) * 128 + (uint32_t)retained;
    if(e > 127 || result >= 0x7f80){
        softfloat_raiseFlags(softfloat_flag_overflow | softfloat_flag_inexact);
        _Bool infinity = mode == softfloat_round_near_even
            || mode == softfloat_round_near_maxMag
            || (mode == softfloat_round_min && negative)
            || (mode == softfloat_round_max && !negative);
        return sign | (infinity? 0x7f80 : 0x7f7f);
    }
    if(remainder){
        softfloat_raiseFlags(softfloat_flag_inexact);
        if(result < 128 || (softfloat_detectTininess == softfloat_tininess_beforeRounding && e < -126))
            softfloat_raiseFlags(softfloat_flag_underflow);
    }
    return sign | (uint16_t)result;
}

static inline
float32_t
ci_bfloat16_to_f32(uint16_t bits){
    if((bits & 0x7fff) > 0x7f80){
        if(!(bits & 0x40)) softfloat_raiseFlags(softfloat_flag_invalid);
        bits |= 0x40;
    }
    return (float32_t){(uint32_t)bits << 16};
}

static inline
uint16_t
ci_float16_from_float(float value, _Bool is_brian_float){
    float32_t f;
    memcpy(&f, &value, sizeof f);
    if(!is_brian_float) return f32_to_f16(f).v;
    unsigned exp = (f.v >> 23) & 255;
    uint32_t fraction = f.v & 0x7fffff;
    uint16_t sign = (uint16_t)((f.v >> 16) & 0x8000);
    if(exp == 255){
        if(!fraction) return sign | 0x7f80;
        if(!(fraction & 0x400000)) softfloat_raiseFlags(softfloat_flag_invalid);
        return sign | 0x7fc0 | (uint16_t)(fraction >> 16);
    }
    return ci_bfloat16_pack(sign != 0, fraction | (exp? 0x800000 : 0), (int)(exp? exp : 1) - 127 - 23);
}

static inline
float
ci_float16_to_float(uint16_t bits, _Bool is_brian_float){
    float32_t f = is_brian_float? ci_bfloat16_to_f32(bits) : f16_to_f32((float16_t){bits});
    float result;
    memcpy(&result, &f, sizeof result);
    return result;
}

static inline
uint16_t
ci_float16_from_double(double value, _Bool is_brian_float){
    float64_t f;
    memcpy(&f, &value, sizeof f);
    if(!is_brian_float) return f64_to_f16(f).v;
    unsigned exp = (unsigned)((f.v >> 52) & 2047);
    uint64_t fraction = f.v & UINT64_C(0x000fffffffffffff);
    uint16_t sign = (uint16_t)((f.v >> 48) & 0x8000);
    if(exp == 2047){
        if(!fraction) return sign | 0x7f80;
        if(!(fraction & UINT64_C(0x0008000000000000))) softfloat_raiseFlags(softfloat_flag_invalid);
        return sign | 0x7fc0 | (uint16_t)(fraction >> 45);
    }
    return ci_bfloat16_pack(sign != 0, fraction | (exp? UINT64_C(0x0010000000000000) : 0), (int)(exp? exp : 1) - 1023 - 52);
}

static inline
double
ci_float16_to_double(uint16_t bits, _Bool is_brian_float){
    float64_t f = is_brian_float? f32_to_f64(ci_bfloat16_to_f32(bits)) : f16_to_f64((float16_t){bits});
    double result;
    memcpy(&result, &f, sizeof result);
    return result;
}

static inline
uint16_t
ci_float16_from_float80(CiFloat80 value, _Bool is_brian_float){
    extFloat80_t f;
    memcpy(&f, &value, sizeof f);
    if(!is_brian_float) return extF80M_to_f16(&f).v;
    unsigned exp = f.signExp & 0x7fff;
    uint16_t sign = f.signExp & 0x8000;
    if(exp == 0x7fff){
        if(!(f.signif & UINT64_C(0x7fffffffffffffff))) return sign | 0x7f80;
        if(!(f.signif & UINT64_C(0x4000000000000000))) softfloat_raiseFlags(softfloat_flag_invalid);
        return sign | 0x7fc0 | (uint16_t)((f.signif >> 56) & 0x7f);
    }
    return ci_bfloat16_pack(sign != 0, f.signif, (int)(exp? exp : 1) - 16383 - 63);
}

static inline
CiFloat80
ci_float16_to_float80(uint16_t bits, _Bool is_brian_float){
    extFloat80_t f = {0};
    if(is_brian_float) f32_to_extF80M(ci_bfloat16_to_f32(bits), &f);
    else f16_to_extF80M((float16_t){bits}, &f);
    return ci_float80_read(&f);
}

static inline
uint16_t
ci_float16_from_int32(int32_t value, _Bool is_brian_float){
    if(!is_brian_float) return i32_to_f16(value).v;
    _Bool negative = value < 0;
    uint64_t magnitude = negative? UINT64_C(0) - (uint64_t)value : (uint64_t)value;
    return ci_bfloat16_pack(negative, magnitude, 0);
}

static inline
int32_t
ci_float16_to_int32(uint16_t bits, _Bool is_brian_float){
    if(is_brian_float)
        return (int32_t)f32_to_i32_r_minMag(ci_bfloat16_to_f32(bits), 0);
    else
        return (int32_t)f16_to_i32_r_minMag((float16_t){bits}, 0);
}

static inline
uint16_t
ci_float16_from_uint32(uint32_t value, _Bool is_brian_float){
    if(!is_brian_float) return ui32_to_f16(value).v;
    return ci_bfloat16_pack(0, value, 0);
}

static inline
uint32_t
ci_float16_to_uint32(uint16_t bits, _Bool is_brian_float){
    if(is_brian_float)
        return (uint32_t)f32_to_ui32_r_minMag(ci_bfloat16_to_f32(bits), 0);
    else
        return (uint32_t)f16_to_ui32_r_minMag((float16_t){bits}, 0);
}

static inline
uint16_t
ci_float16_from_int64(int64_t value, _Bool is_brian_float){
    if(!is_brian_float) return i64_to_f16(value).v;
    _Bool negative = value < 0;
    uint64_t magnitude = negative? UINT64_C(0) - (uint64_t)value : (uint64_t)value;
    return ci_bfloat16_pack(negative, magnitude, 0);
}

static inline
int64_t
ci_float16_to_int64(uint16_t bits, _Bool is_brian_float){
    if(is_brian_float)
        return (int64_t)f32_to_i64_r_minMag(ci_bfloat16_to_f32(bits), 0);
    else
        return (int64_t)f16_to_i64_r_minMag((float16_t){bits}, 0);
}

static inline
uint16_t
ci_float16_from_uint64(uint64_t value, _Bool is_brian_float){
    if(!is_brian_float) return ui64_to_f16(value).v;
    return ci_bfloat16_pack(0, value, 0);
}

static inline
uint64_t
ci_float16_to_uint64(uint16_t bits, _Bool is_brian_float){
    if(is_brian_float)
        return (uint64_t)f32_to_ui64_r_minMag(ci_bfloat16_to_f32(bits), 0);
    else
        return (uint64_t)f16_to_ui64_r_minMag((float16_t){bits}, 0);
}

// Both narrow formats retain their encoded bits in VM slots.
static inline
CiFloat128
ci_float16_to_quad(uint16_t bits, _Bool is_brian_float){
    float128_t q;
    if(is_brian_float) f32_to_f128M((float32_t){(uint32_t)bits << 16}, &q);
    else f16_to_f128M((float16_t){bits}, &q);
    return ci_float128_read(&q);
}

static inline
uint16_t
ci_float16_from_quad(CiFloat128 value, _Bool is_brian_float){
    float128_t q;
    memcpy(&q, &value, sizeof q);
    if(!is_brian_float) return f128M_to_f16(&q).v;
    uint64_t raw[2];
    memcpy(raw, &q, sizeof raw);
    unsigned exp = (unsigned)((raw[1] >> 48) & 0x7fff);
    uint16_t sign = (uint16_t)((raw[1] >> 48) & 0x8000);
    raw[1] &= UINT64_C(0x0000ffffffffffff);
    CiUint128 sig;
    ci_uint128_read(&sig, raw, 16);
    if(exp == 0x7fff){
        if(!ci_uint128_nonzero(sig)) return sign | 0x7f80;
        if(!(raw[1] & UINT64_C(0x0000800000000000)))
            softfloat_raiseFlags(softfloat_flag_invalid);
        return sign | 0x7fc0 | (uint16_t)(raw[1] >> 41);
    }
    if(!exp && !ci_uint128_nonzero(sig)) return sign;
    if(exp) sig = ci_uint128_or(sig, ci_uint128_shl(ci_uint128_from_uint64(1), 112));
    int e = (int)(exp? exp : 1) - 16383;
    int shift = 105 + (e < -126? -126 - e : 0);
    CiUint128 retained = ci_uint128_shr(sig, (uint64_t)shift);
    CiUint128 rem = ci_uint128_sub(sig, ci_uint128_shl(retained, (uint64_t)shift));
    uint64_t n = ci_uint128_lo(retained);
    _Bool inexact = ci_uint128_nonzero(rem);
    uint_fast8_t mode = softfloat_roundingMode;
    if(inexact){
        CiUint128 half = ci_uint128_shl(ci_uint128_from_uint64(1), (uint64_t)shift - 1);
        _Bool above = shift <= 128 && ci_uint128_gt(rem, half);
        _Bool tie = shift <= 128 && ci_uint128_eq(rem, half);
        if((mode == softfloat_round_near_even && (above || (tie && (n & 1))))
            || (mode == softfloat_round_near_maxMag && (above || tie))
            || (mode == softfloat_round_min && sign)
            || (mode == softfloat_round_max && !sign)) n++;
        else if(mode == softfloat_round_odd) n |= 1;
    }
    int encoded = e < -126? 0 : e + 126;
    uint32_t result = (uint32_t)(encoded > 0? encoded : 0) * 128 + (uint32_t)n;
    if(e > 127 || result >= 0x7f80){
        softfloat_raiseFlags(softfloat_flag_overflow | softfloat_flag_inexact);
        _Bool infinity = mode == softfloat_round_near_even || mode == softfloat_round_near_maxMag
            || (mode == softfloat_round_min && sign) || (mode == softfloat_round_max && !sign);
        return sign | (infinity? 0x7f80 : 0x7f7f);
    }
    if(inexact){
        softfloat_raiseFlags(softfloat_flag_inexact);
        if(result < 128 || (softfloat_detectTininess == softfloat_tininess_beforeRounding && e < -126))
            softfloat_raiseFlags(softfloat_flag_underflow);
    }
    return sign | (uint16_t)result;
}

static inline
uint16_t
ci_float16_from_uint128(CiUint128 v, _Bool uns, _Bool is_brian_float){
    _Bool negative = !uns && (ci_uint128_hi(v) >> 63);
    if(negative) v = ci_uint128_sub(ci_uint128_from_uint64(0), v);
    if(!ci_uint128_nonzero(v))
        return 0;
    int top;
    unsigned precision = is_brian_float? 8 : 11;
    CiUint128 sig = ci_uint128_round_float(v, negative, precision, &top);
    if(is_brian_float)
        return ci_bfloat16_pack(negative, ci_uint128_lo(sig), top - 7);
    uint16_t sign = negative? 0x8000 : 0;
    if(top > 15){
        softfloat_raiseFlags(softfloat_flag_overflow | softfloat_flag_inexact);
        uint_fast8_t mode = softfloat_roundingMode;
        _Bool infinity = mode == softfloat_round_near_even
            || mode == softfloat_round_near_maxMag
            || (mode == softfloat_round_min && negative)
            || (mode == softfloat_round_max && !negative);
        return sign | (infinity? 0x7c00 : 0x7bff);
    }
    return sign | (uint16_t)((top + 15) << 10) | (uint16_t)(ci_uint128_lo(sig) & 0x3ff);
}

static inline
CiUint128
ci_float16_to_uint128(uint16_t bits, _Bool is_brian_float){
    float32_t f = is_brian_float? ci_bfloat16_to_f32(bits) : f16_to_f32((float16_t){bits});
    unsigned exp = (f.v >> 23) & 255;
    int shift = (int)exp - 127 - 23;
    if(!exp || exp == 255 || shift <= -128 || shift >= 128)
        return ci_uint128_from_uint64(0);
    CiUint128 sig = ci_uint128_from_uint64((f.v & 0x7fffff) | 0x800000);
    CiUint128 result = shift < 0? ci_uint128_shr(sig, (uint64_t)-shift) : ci_uint128_shl(sig, (uint64_t)shift);
    return f.v >> 31? ci_uint128_sub(ci_uint128_from_uint64(0), result) : result;
}

static inline
uint16_t
ci_bfloat16_round(float64_t value){
    double d;
    memcpy(&d, &value, sizeof d);
    return ci_float16_from_double(d, 1);
}

static inline
uint16_t
ci_float16_add(uint16_t a, uint16_t b, _Bool is_brian_float){
    if(!is_brian_float) return f16_add((float16_t){a}, (float16_t){b}).v;
    float64_t x = f32_to_f64((float32_t){(uint32_t)a << 16});
    float64_t y = f32_to_f64((float32_t){(uint32_t)b << 16});
    uint_fast8_t mode = softfloat_roundingMode;
    softfloat_roundingMode = softfloat_round_odd;
    float64_t r = f64_add(x, y);
    softfloat_roundingMode = mode;
    if(!(r.v & UINT64_C(0x7fffffffffffffff)) && ((a ^ b) & 0x8000))
        r.v = mode == softfloat_round_min? UINT64_C(0x8000000000000000) : 0;
    return ci_bfloat16_round(r);
}

static inline
uint16_t
ci_float16_sub(uint16_t a, uint16_t b, _Bool is_brian_float){
    if(!is_brian_float) return f16_sub((float16_t){a}, (float16_t){b}).v;
    float64_t x = f32_to_f64((float32_t){(uint32_t)a << 16});
    float64_t y = f32_to_f64((float32_t){(uint32_t)b << 16});
    uint_fast8_t mode = softfloat_roundingMode;
    softfloat_roundingMode = softfloat_round_odd;
    float64_t r = f64_sub(x, y);
    softfloat_roundingMode = mode;
    if(!(r.v & UINT64_C(0x7fffffffffffffff)) && ((a ^ b ^ 0x8000) & 0x8000))
        r.v = mode == softfloat_round_min? UINT64_C(0x8000000000000000) : 0;
    return ci_bfloat16_round(r);
}

static inline
uint16_t
ci_float16_mul(uint16_t a, uint16_t b, _Bool is_brian_float){
    if(!is_brian_float) return f16_mul((float16_t){a}, (float16_t){b}).v;
    float64_t x = f32_to_f64((float32_t){(uint32_t)a << 16});
    float64_t y = f32_to_f64((float32_t){(uint32_t)b << 16});
    uint_fast8_t mode = softfloat_roundingMode;
    softfloat_roundingMode = softfloat_round_odd;
    float64_t r = f64_mul(x, y);
    softfloat_roundingMode = mode;
    return ci_bfloat16_round(r);
}

static inline
uint16_t
ci_float16_div(uint16_t a, uint16_t b, _Bool is_brian_float){
    if(!is_brian_float) return f16_div((float16_t){a}, (float16_t){b}).v;
    float64_t x = f32_to_f64((float32_t){(uint32_t)a << 16});
    float64_t y = f32_to_f64((float32_t){(uint32_t)b << 16});
    uint_fast8_t mode = softfloat_roundingMode;
    softfloat_roundingMode = softfloat_round_odd;
    float64_t r = f64_div(x, y);
    softfloat_roundingMode = mode;
    return ci_bfloat16_round(r);
}

static inline
uint16_t
ci_float16_neg(uint16_t a){
    return a ^ 0x8000;
}

static inline
_Bool
ci_float16_eq(uint16_t a, uint16_t b, _Bool is_brian_float){
    float32_t x = is_brian_float? (float32_t){(uint32_t)a << 16} : f16_to_f32((float16_t){a});
    float32_t y = is_brian_float? (float32_t){(uint32_t)b << 16} : f16_to_f32((float16_t){b});
    return f32_eq(x, y);
}

static inline
_Bool
ci_float16_lt(uint16_t a, uint16_t b, _Bool is_brian_float){
    float32_t x = is_brian_float? (float32_t){(uint32_t)a << 16} : f16_to_f32((float16_t){a});
    float32_t y = is_brian_float? (float32_t){(uint32_t)b << 16} : f16_to_f32((float16_t){b});
    return f32_lt(x, y);
}

static inline
_Bool
ci_float16_le(uint16_t a, uint16_t b, _Bool is_brian_float){
    float32_t x = is_brian_float? (float32_t){(uint32_t)a << 16} : f16_to_f32((float16_t){a});
    float32_t y = is_brian_float? (float32_t){(uint32_t)b << 16} : f16_to_f32((float16_t){b});
    return f32_le(x, y);
}

#endif
