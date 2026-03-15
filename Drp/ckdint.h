#ifndef DRP_CKD_INT_H
#define DRP_CKD_INT_H
//
// Copyright © 2026-2026, David Priver <david@davidpriver.com>
//
// Compatibility header for overflow intrinsics
//

#if defined __GNUC__
#define add_overflow __builtin_add_overflow
#define sub_overflow __builtin_sub_overflow
#define mul_overflow __builtin_mul_overflow
#else

#if defined __has_include
  #if __has_include(<stdckdint.h>)
    #include <stdckdint.h>
    #define add_overflow(a, b, r) ckd_add(r, a, b)
    #define sub_overflow(a, b, r) ckd_sub(r, a, b)
    #define mul_overflow(a, b, r) ckd_mul(r, a, b)
  #endif
#endif

#if !defined add_overflow
#include <stdint.h>
#include <limits.h>

#if defined _MSC_VER
#include <intrin.h>
#endif

static inline _Bool add_overflow_u32(uint32_t a, uint32_t b, uint32_t *r) {
    *r = a + b;
    return *r < a;
}
static inline _Bool add_overflow_u64(uint64_t a, uint64_t b, uint64_t *r) {
    *r = a + b;
    return *r < a;
}
static inline _Bool add_overflow_i32(int32_t a, int32_t b, int32_t *r) {
    uint32_t ur = (uint32_t)a + (uint32_t)b;
    *r = (int32_t)ur;
    return (int32_t)(((uint32_t)a ^ ur) & ((uint32_t)b ^ ur)) < 0;
}
static inline _Bool add_overflow_i64(int64_t a, int64_t b, int64_t *r) {
    uint64_t ur = (uint64_t)a + (uint64_t)b;
    *r = (int64_t)ur;
    return (int64_t)(((uint64_t)a ^ ur) & ((uint64_t)b ^ ur)) < 0;
}

static inline _Bool sub_overflow_u32(uint32_t a, uint32_t b, uint32_t *r) {
    *r = a - b;
    return a < b;
}
static inline _Bool sub_overflow_u64(uint64_t a, uint64_t b, uint64_t *r) {
    *r = a - b;
    return a < b;
}
static inline _Bool sub_overflow_i32(int32_t a, int32_t b, int32_t *r) {
    uint32_t ua = (uint32_t)a, ub = (uint32_t)b;
    uint32_t ur = ua - ub;
    *r = (int32_t)ur;
    return (int32_t)((ua ^ ub) & (ua ^ ur)) < 0;
}
static inline _Bool sub_overflow_i64(int64_t a, int64_t b, int64_t *r) {
    uint64_t ua = (uint64_t)a, ub = (uint64_t)b;
    uint64_t ur = ua - ub;
    *r = (int64_t)ur;
    return (int64_t)((ua ^ ub) & (ua ^ ur)) < 0;
}

static inline _Bool mul_overflow_u32(uint32_t a, uint32_t b, uint32_t *r) {
    uint64_t w = (uint64_t)a * b;
    *r = (uint32_t)w;
    return (w >> 32) != 0;
}
static inline _Bool mul_overflow_i32(int32_t a, int32_t b, int32_t *r) {
    int64_t w = (int64_t)a * b;
    *r = (int32_t)w;
    return w != *r;
}

#if defined _MSC_VER
static inline _Bool mul_overflow_u64(uint64_t a, uint64_t b, uint64_t *r) {
    uint64_t high;
    *r = _umul128(a, b, &high);
    return high != 0;
}
#else
static inline _Bool mul_overflow_u64(uint64_t a, uint64_t b, uint64_t *r) {
    *r = a * b;
    return a != 0 && *r / a != b;
}
#endif

static inline _Bool mul_overflow_i64(int64_t a, int64_t b, int64_t *r) {
    *r = (int64_t)((uint64_t)a * (uint64_t)b);
    if (a == 0 || b == 0) return 0;
    if (a == -1) return b == INT64_MIN;
    if (b == -1) return a == INT64_MIN;
    return *r / a != b;
}

#if LONG_MAX > INT32_MAX
#define add_overflow(a, b, r) _Generic((r), \
    int *:                add_overflow_i32, \
    unsigned int *:       add_overflow_u32, \
    long *:               add_overflow_i64, \
    unsigned long *:      add_overflow_u64, \
    long long *:          add_overflow_i64, \
    unsigned long long *: add_overflow_u64)(a, b, (void*)r)
#define sub_overflow(a, b, r) _Generic((r), \
    int *:                sub_overflow_i32, \
    unsigned int *:       sub_overflow_u32, \
    long *:               sub_overflow_i64, \
    unsigned long *:      sub_overflow_u64, \
    long long *:          sub_overflow_i64, \
    unsigned long long *: sub_overflow_u64)(a, b, (void*)r)
#define mul_overflow(a, b, r) _Generic((r), \
    int *:                mul_overflow_i32, \
    unsigned int *:       mul_overflow_u32, \
    long *:               mul_overflow_i64, \
    unsigned long *:      mul_overflow_u64, \
    long long *:          mul_overflow_i64, \
    unsigned long long *: mul_overflow_u64)(a, b, (void*)r)
#else
#define add_overflow(a, b, r) _Generic((r), \
    int *:                add_overflow_i32, \
    unsigned int *:       add_overflow_u32, \
    long *:               add_overflow_i32, \
    unsigned long *:      add_overflow_u32, \
    long long *:          add_overflow_i64, \
    unsigned long long *: add_overflow_u64)(a, b, (void*)r)
#define sub_overflow(a, b, r) _Generic((r), \
    int *:                sub_overflow_i32, \
    unsigned int *:       sub_overflow_u32, \
    long *:               sub_overflow_i32, \
    unsigned long *:      sub_overflow_u32, \
    long long *:          sub_overflow_i64, \
    unsigned long long *: sub_overflow_u64)(a, b, (void*)r)
#define mul_overflow(a, b, r) _Generic((r), \
    int *:                mul_overflow_i32, \
    unsigned int *:       mul_overflow_u32, \
    long *:               mul_overflow_i32, \
    unsigned long *:      mul_overflow_u32, \
    long long *:          mul_overflow_i64, \
    unsigned long long *: mul_overflow_u64)(a, b, (void*)r)
#endif

#endif
#endif
#endif
