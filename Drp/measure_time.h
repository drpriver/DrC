//
// Copyright © 2024-2025, David Priver <david@davidpriver.com>
//
#ifndef MEASURE_TIME_H
#define MEASURE_TIME_H

//
// Gets current monotonically increasing time, measured in microseconds.
// Always succeeds.
// Used for ad-hoc profiling of different parts of the program.
//
static inline uint64_t get_t(void);
#include "posixheader.h"
#include "windowsheader.h"

#if defined(__linux__) || defined(__APPLE__)

#include <stdint.h>
// returns microseconds
static inline
uint64_t
get_t(void){
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC_RAW, &t);
    return t.tv_sec * 1000000llu + t.tv_nsec/1000;
}

#elif defined(_WIN32)
static uint64_t freq;

// returns microseconds
static inline
uint64_t
get_t(void){
    uint64_t time;
    if(!freq){
        // This should never fail.
        // "On systems that run Windows XP or later, the function will always
        // succeed and will thus never return zero."
        BOOL ok = QueryPerformanceFrequency((LARGE_INTEGER*)&freq);
        (void)ok;
    }

    // This should never fail.
    // "On systems that run Windows XP or later, the function will always
    // succeed and will thus never return zero."
    BOOL ok = QueryPerformanceCounter((LARGE_INTEGER*)&time);
    (void)ok;
    return  (1000000llu * time) / freq;
}
#elif defined(__wasm__)

static inline
uint64_t
get_t(void){
    return 0;
}
#endif

#endif
