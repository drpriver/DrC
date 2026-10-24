//
// Copyright © 2024-2026, David Priver <david@davidpriver.com>
//
#ifndef MEASURE_TIME_H
#define MEASURE_TIME_H

#include <stdint.h>
static inline uint64_t monotonic_time_us(void);
#include "posixheader.h"
#include "windowsheader.h"

#if defined __linux__ || defined __APPLE__

#include <stdint.h>
// returns microseconds
static inline
uint64_t
monotonic_time_us(void){
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC_RAW, &t);
    return t.tv_sec * 1000000llu + t.tv_nsec/1000;
}

#elif defined _WIN32
static uint64_t monotonic_time_freq;

// returns microseconds
static inline
uint64_t
monotonic_time_us(void){
    uint64_t time;
    if(!monotonic_time_freq){
        // This should never fail.
        // "On systems that run Windows XP or later, the function will always
        // succeed and will thus never return zero."
        BOOL ok = QueryPerformanceFrequency((LARGE_INTEGER*)&monotonic_time_freq);
        (void)ok;
    }

    // This should never fail.
    // "On systems that run Windows XP or later, the function will always
    // succeed and will thus never return zero."
    BOOL ok = QueryPerformanceCounter((LARGE_INTEGER*)&time);
    (void)ok;
    return  (1000000llu * time) / monotonic_time_freq;
}
#elif defined __wasm__

static inline
uint64_t
monotonic_time_us(void){
    return 0;
}
#endif

#ifndef force_inline
#if defined __GNUC__ || defined __clang__ || defined __DRC__
#define force_inline static inline __attribute__((always_inline))
#elif defined _MSC_VER
#define force_inline static inline __forceinline
#else
#define force_inline static inline
#endif
#endif

#ifndef __has_builtin
#define __has_builtin(x) 0
#endif

force_inline
uint64_t
performance_counter(void){
    #if __has_builtin(__builtin_readcyclecounter)
        return __builtin_readcyclecounter();
    #elif defined _MSC_VER && defined _M_ARM64
        return (uint64_t)_ReadStatusReg(ARM64_CNTVCT);
    #elif defined _MSC_VER && (defined _M_IX86 || defined _M_X64)
        return __rdtsc();
    #elif (defined __GNUC__ || defined __clang__) && (defined __i386__ || defined __x86_64__)
        #if __has_builtin(__builtin_ia32_rdtsc)
        return __builtin_ia32_rdtsc();
        #else
        uint32_t lo, hi;
        __asm__ volatile("rdtsc" : "=a"(lo), "=d"(hi));
        return ((uint64_t)hi << 32) | lo;
        #endif
    #elif (defined __GNUC__ || defined __clang__) && defined __aarch64__
        #if __has_builtin(__builtin_arm_rsr64)
        return __builtin_arm_rsr64("cntvct_el0");
        #elif __has_builtin(__builtin_aarch64_rsr64)
        return __builtin_aarch64_rsr64("cntvct_el0");
        #else
        uint64_t ticks;
        __asm__ volatile("mrs %0, cntvct_el0" : "=r"(ticks));
        return ticks;
        #endif
    #else
        return 0;
    #endif
}

#endif
