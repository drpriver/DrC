//
// Copyright © 2024-2025, David Priver <david@davidpriver.com>
//
#ifndef RNG_H
#define RNG_H
// size_t
#include <stddef.h>
// uint64_t, uint32_t
#include <stdint.h>

#ifndef RNG_API
#define RNG_API static inline
#endif

#ifndef SER
#define SER(...)
#endif

typedef struct RngState {
    SER() uint64_t state;
    SER() uint64_t inc;
} RngState;

#ifdef __clang__
#pragma clang assume_nonnull begin
#endif


//
// Produces a uniform random u32.
//
RNG_API uint32_t rng_random32(RngState* rng);


//
// Seeds the rng with the given values.
//
RNG_API void seed_rng_fixed(RngState* rng, uint64_t initstate, uint64_t initseq);


//
// Seeds the rng using os APIs when available.
// If not available, uses rdseed on x64.
//
RNG_API void seed_rng_auto(RngState* rng);

//
// Returns a random u32 in the range of [0, bound).
//
RNG_API uint32_t bounded_random(RngState* rng, uint32_t bound);

// Returns 0 or 1 (false or true).
RNG_API _Bool random_bool(RngState* rng);
// Returns 0 or 1 with probability of (num / denom)
RNG_API _Bool random_chance(RngState* rng, uint32_t num, uint32_t denom);

#define SHUFFLE_FUNC(type, mangle) \
static inline \
void \
mangle(RngState* rng, type* data, size_t count_){\
    if(count_ < 2) return;\
    uint32_t count = (uint32_t)count_;\
    for(uint32_t i = 0; i < count-1; i++){\
        uint32_t j = bounded_random(rng, count-i)+i;\
        type tmp = data[i];\
        data[i] = data[j];\
        data[j] = tmp;\
    }\
}

#ifdef __clang__
#pragma clang assume_nonnull end
#endif

#endif
