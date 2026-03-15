#ifndef C_CI_SOFTNUM_H
#define C_CI_SOFTNUM_H
//
// Copyright © 2026-2026, David Priver <david@davidpriver.com>
//
// Software implementations of potentially missing hardware
// types for ctfe and interpreter, etc.
//

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
static inline CiUint128 ci_uint128_div(CiUint128 a, CiUint128 b){ return b ? a / b : 0; }
static inline CiUint128 ci_uint128_mod(CiUint128 a, CiUint128 b){ return b ? a % b : 0; }
static inline CiUint128 ci_uint128_and(CiUint128 a, CiUint128 b){ return a & b; }
static inline CiUint128 ci_uint128_or(CiUint128 a, CiUint128 b){ return a | b; }
static inline CiUint128 ci_uint128_xor(CiUint128 a, CiUint128 b){ return a ^ b; }
static inline CiUint128 ci_uint128_shl(CiUint128 a, uint64_t b){ return a << b; }
static inline CiUint128 ci_uint128_shr(CiUint128 a, uint64_t b){ return a >> b; }
static inline _Bool ci_uint128_eq(CiUint128 a, CiUint128 b){ return a == b; }
static inline _Bool ci_uint128_ne(CiUint128 a, CiUint128 b){ return a != b; }
static inline _Bool ci_uint128_lt(CiUint128 a, CiUint128 b){ return a < b; }
static inline _Bool ci_uint128_gt(CiUint128 a, CiUint128 b){ return a > b; }
static inline _Bool ci_uint128_le(CiUint128 a, CiUint128 b){ return a <= b; }
static inline _Bool ci_uint128_ge(CiUint128 a, CiUint128 b){ return a >= b; }
static inline _Bool ci_uint128_nonzero(CiUint128 a){ return a != 0; }
static inline void ci_uint128_read(CiUint128* out, const void* buf, uint32_t sz){
    *out = 0;
    memcpy(out, buf, sz <= 16 ? sz : 16);
}
static inline void ci_uint128_write(void* buf, uint32_t sz, CiUint128 v){
    memcpy(buf, &v, sz <= 16 ? sz : 16);
}
#else
typedef struct { uint64_t lo, hi; } CiUint128;
static inline CiUint128 ci_uint128_from_uint64(uint64_t v){
    return (CiUint128){.lo = v, .hi = 0};
}
static inline CiUint128 ci_uint128_from_int64(int64_t v){
    return (CiUint128){.lo = (uint64_t)v, .hi = v < 0 ? ~(uint64_t)0 : 0};
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
static inline CiUint128 ci_uint128_mul(CiUint128 a, CiUint128 b){
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
        r.lo |= (i >= 64 ? (a.hi >> (i - 64)) : (a.lo >> i)) & 1;
        if(ci_uint128_ge(r, b)){
            r = ci_uint128_sub(r, b);
            if(i >= 64) q.hi |= (uint64_t)1 << (i - 64);
            else q.lo |= (uint64_t)1 << i;
        }
    }
    return q;
}
static inline CiUint128 ci_uint128_mod(CiUint128 a, CiUint128 b){
    CiUint128 q = ci_uint128_div(a, b);
    return ci_uint128_sub(a, ci_uint128_mul(q, b));
}
static inline void ci_uint128_read(CiUint128* out, const void* buf, uint32_t sz){
    *out = (CiUint128){0, 0};
    memcpy(out, buf, sz <= 16 ? sz : 16);
}
static inline void ci_uint128_write(void* buf, uint32_t sz, CiUint128 v){
    memcpy(buf, &v, sz <= 16 ? sz : 16);
}
#endif

// 128-bit signed integer
#ifdef __SIZEOF_INT128__
typedef __int128 CiInt128;
static inline CiInt128 ci_int128_from_int64(int64_t v){ return (CiInt128)v; }
static inline CiInt128 ci_int128_from_uint64(uint64_t v){ return (CiInt128)v; }
static inline _Bool ci_int128_eq(CiInt128 a, CiInt128 b){ return a == b; }
static inline CiInt128 ci_int128_add(CiInt128 a, CiInt128 b){ return a + b; }
static inline CiInt128 ci_int128_sub(CiInt128 a, CiInt128 b){ return a - b; }
static inline CiInt128 ci_int128_mul(CiInt128 a, CiInt128 b){ return a * b; }
static inline uint64_t ci_int128_lo(CiInt128 a){ return (uint64_t)a; }
static inline CiInt128 ci_int128_neg(CiInt128 a){ return -a; }
static inline CiInt128 ci_int128_div(CiInt128 a, CiInt128 b){ return b ? a / b : 0; }
static inline CiInt128 ci_int128_mod(CiInt128 a, CiInt128 b){ return b ? a % b : 0; }
static inline _Bool ci_int128_lt(CiInt128 a, CiInt128 b){ return a < b; }
static inline _Bool ci_int128_gt(CiInt128 a, CiInt128 b){ return a > b; }
static inline _Bool ci_int128_le(CiInt128 a, CiInt128 b){ return a <= b; }
static inline _Bool ci_int128_ge(CiInt128 a, CiInt128 b){ return a >= b; }
static inline CiInt128 ci_int128_shr(CiInt128 a, uint64_t b){ return a >> b; }
static inline CiInt128 ci_int128_from_uint128(CiUint128 a){ return (CiInt128)a; }
static inline CiUint128 ci_uint128_from_int128(CiInt128 a){ return (CiUint128)a; }
#else
typedef struct { uint64_t lo; int64_t hi; } CiInt128;
static inline CiInt128 ci_int128_from_int64(int64_t v){
    return (CiInt128){.lo = (uint64_t)v, .hi = v >> 63};
}
static inline CiInt128 ci_int128_from_uint64(uint64_t v){
    return (CiInt128){.lo = v, .hi = 0};
}
static inline _Bool ci_int128_eq(CiInt128 a, CiInt128 b){
    return a.lo == b.lo && a.hi == b.hi;
}
static inline CiInt128 ci_int128_add(CiInt128 a, CiInt128 b){
    uint64_t lo = a.lo + b.lo;
    int64_t hi = a.hi + b.hi + (lo < a.lo);
    return (CiInt128){lo, hi};
}
static inline CiInt128 ci_int128_sub(CiInt128 a, CiInt128 b){
    uint64_t lo = a.lo - b.lo;
    int64_t hi = a.hi - b.hi - (a.lo < b.lo);
    return (CiInt128){lo, hi};
}
static CiInt128 ci_int128_neg(CiInt128 a){
    uint64_t lo = ~a.lo + 1;
    int64_t hi = ~a.hi + (lo == 0);
    return (CiInt128){lo, hi};
}
static CiInt128 ci_int128_mul(CiInt128 a, CiInt128 b){
    _Bool a_neg = a.hi < 0;
    _Bool b_neg = b.hi < 0;
    uint64_t au = a_neg ? (~a.lo + 1) : a.lo;
    uint64_t bu = b_neg ? (~b.lo + 1) : b.lo;
    uint64_t al = au & 0xFFFFFFFF, ah = au >> 32;
    uint64_t bl = bu & 0xFFFFFFFF, bh = bu >> 32;
    uint64_t ll = al * bl;
    uint64_t lh = al * bh;
    uint64_t hl = ah * bl;
    uint64_t hh = ah * bh;
    uint64_t mid = (ll >> 32) + (lh & 0xFFFFFFFF) + (hl & 0xFFFFFFFF);
    CiInt128 r = {
        .lo = (ll & 0xFFFFFFFF) | (mid << 32),
        .hi = (int64_t)(hh + (lh >> 32) + (hl >> 32) + (mid >> 32)),
    };
    if(a_neg != b_neg) r = ci_int128_neg(r);
    return r;
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
    if(b >= 128) return (CiInt128){(uint64_t)(a.hi >> 63), a.hi >> 63};
    if(b >= 64) return (CiInt128){(uint64_t)(a.hi >> (b - 64)), a.hi >> 63};
    if(b == 0) return a;
    return (CiInt128){(a.lo >> b) | ((uint64_t)a.hi << (64 - b)), a.hi >> b};
}
static CiInt128 ci_int128_div(CiInt128 a, CiInt128 b){
    if(!b.lo && !b.hi) return (CiInt128){0, 0};
    _Bool a_neg = a.hi < 0;
    _Bool b_neg = b.hi < 0;
    CiUint128 au = a_neg ? ci_uint128_from_int128(ci_int128_neg(a)) : ci_uint128_from_int128(a);
    CiUint128 bu = b_neg ? ci_uint128_from_int128(ci_int128_neg(b)) : ci_uint128_from_int128(b);
    CiInt128 q = ci_int128_from_uint128(ci_uint128_div(au, bu));
    if(a_neg != b_neg) q = ci_int128_neg(q);
    return q;
}
static CiInt128 ci_int128_mod(CiInt128 a, CiInt128 b){
    if(!b.lo && !b.hi) return (CiInt128){0, 0};
    _Bool a_neg = a.hi < 0;
    CiUint128 au = a_neg ? ci_uint128_from_int128(ci_int128_neg(a)) : ci_uint128_from_int128(a);
    CiUint128 bu = b.hi < 0 ? ci_uint128_from_int128(ci_int128_neg(b)) : ci_uint128_from_int128(b);
    CiInt128 r = ci_int128_from_uint128(ci_uint128_mod(au, bu));
    if(a_neg) r = ci_int128_neg(r);
    return r;
}
#endif


#endif
