#ifndef C_CI_SOFTNUM_H
#define C_CI_SOFTNUM_H
//
// Copyright © 2026-2026, David Priver <david@davidpriver.com>
//
// Software implementations of potentially missing hardware
// types for ctfe and interpreter, etc.
//

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
#endif


#endif
