//
// Copyright © 2024-2025, David Priver <david@davidpriver.com>
//
#ifndef RNG_C
#define RNG_C
#include "rng.h"

#ifndef force_inline
#if defined(__GNUC__) || defined(__clang__)
#define force_inline static inline __attribute__((always_inline))
#elif defined(_MSC_VER)
#define force_inline static inline __forceinline
#else
#define force_inline static inline
#endif
#endif

#ifdef __linux__
// for getrandom
#include <sys/random.h>
#endif

#ifdef __APPLE__
// for arc4random_buf
#include <stdlib.h>
#endif

#ifdef __clang__
#pragma clang assume_nonnull begin
#endif

//
// Produces a uniform random u32.
//
RNG_API
uint32_t
rng_random32(RngState* rng){
    uint64_t oldstate = rng->state;
    rng->state = oldstate * 6364136223846793005ULL + rng->inc;
    uint32_t xorshifted = (uint32_t) ( ((oldstate >> 18u) ^ oldstate) >> 27u);
    uint32_t rot = oldstate >> 59u;
    return (xorshifted >> rot) | (xorshifted << ((-rot) & 31));
}

//
// Seeds the rng with the given values.
//
RNG_API
void
seed_rng_fixed(RngState* rng, uint64_t initstate, uint64_t initseq){
    rng->state = 0U;
    rng->inc = (initseq << 1u) | 1u;
    rng_random32(rng);
    rng->state += initstate;
    rng_random32(rng);
}

//
// Seeds the rng using os APIs when available.
// If not available, uses rdseed on x64.
//
RNG_API
void
seed_rng_auto(RngState* rng){
    _Static_assert(sizeof(unsigned long long) == sizeof(uint64_t), "");
    // spurious warnings on some platforms about unsigned long longs and unsigned longs,
    // so, use the unsigned long long.
    unsigned long long initstate;
    unsigned long long initseq;
#ifdef __APPLE__
    arc4random_buf(&initstate, sizeof(initstate));
    arc4random_buf(&initseq, sizeof(initseq));
#elif defined(__linux__)
    // Too lazy to check for error.  Man page says we can't get
    // interrupted by signals for such a small buffer. and we're
    // not requesting nonblocking.
    ssize_t read;
    read = getrandom(&initstate, sizeof(initstate), 0);
    read = getrandom(&initseq, sizeof(initseq), 0);
    (void)read;
#elif defined(__wasm__)
    __attribute__((import_name("w_get_seed")))
    extern void w_get_seed(uint64_t*);
    w_get_seed(&initstate);
    w_get_seed(&initseq);
#elif defined(__GNUC__) || defined(__clang__)
    // Just use the intel instruction, easier that way.
    // Could load RtlGenRandom (SystemFunction036 in Advapi32.dll),
    // but that seems unnecessary.
    // But if we need a windows arm build, we could do that instead.
    if(0){
        #if 0
        __builtin_ia32_rdseed64_step(&initstate);
        __builtin_ia32_rdseed64_step(&initseq);
        #endif
    }
    else {
        static HMODULE lib;
        if(!lib) lib = LoadLibraryW(L"Advapi32.dll");
        assert(lib);
        typedef BOOLEAN(RtlGenRandomT)(PVOID, ULONG);
        static RtlGenRandomT* gr;
        if(!gr) gr = (RtlGenRandomT*)(void*)GetProcAddress(lib, "SystemFunction036"); // RtlGenRandom
        BOOLEAN r = gr(&initstate, sizeof initstate);
        r = gr(&initseq, sizeof initseq);
        (void)r;
        // FreeLibrary(lib);
    }
#elif defined(_MSC_VER)
    _rdseed64_step(&initstate);
    _rdseed64_step(&initseq);
#else
#error "Need to specify a way to seed an rng with this compiler or platform"
#endif
    seed_rng_fixed(rng, initstate, initseq);
}

// from
// https://lemire.me/blog/2016/06/27/a-fast-alternative-to-the-modulo-reduction/
force_inline
uint32_t
rng_fast_reduce(uint32_t x, uint32_t N){
    return ((uint64_t)x * (uint64_t)N) >> 32;
}

//
// Returns a random u32 in the range of [0, bound).
//
RNG_API
uint32_t
bounded_random(RngState* rng, uint32_t bound){
    uint32_t threshold = -bound % bound;
    // bounded loop to catch unitialized rng errors
    for(size_t i = 0 ; i < 10000; i++){
        uint32_t r = rng_random32(rng);
        if(r >= threshold){
            uint32_t temp = rng_fast_reduce(r, bound);
            // uint32_t temp = r % bound;
            return temp;
        }
    }
    __builtin_debugtrap();
#if defined(__GNUC__) || defined(__clang__)
    __builtin_unreachable();
#else
    return 0;
#endif
}

// Returns 0 or 1 (false or true).
RNG_API
_Bool
random_bool(RngState* rng){
    uint32_t r = rng_random32(rng);
    return r & 1? 1: 0;
}
// Returns 0 or 1 with probability of (num / denom)
RNG_API
_Bool
random_chance(RngState* rng, uint32_t num, uint32_t denom){
    uint32_t r = bounded_random(rng, denom);
    return r < num;
}

#ifdef __clang__
#pragma clang assume_nonnull end
#endif

#endif
