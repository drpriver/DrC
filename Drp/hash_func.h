//
// Copyright © 2025-2026, David Priver <david@davidpriver.com>
//
#ifndef HASH_FUNC_H
#define HASH_FUNC_H
#include <stdint.h>
#include <stddef.h>
#include <string.h>

#if !defined USE_RAPIDHASH && !defined USE_MURMUR && !defined USE_CRC32
#define USE_RAPIDHASH
#endif

#ifndef force_inline
#if defined __GNUC__ || defined __clang__ || defined __DRC__
#define force_inline static inline __attribute__((always_inline))
#elif defined(_MSC_VER)
#define force_inline static inline __forceinline
#else
#define force_inline static inline
#endif
#endif


#if defined USE_RAPIDHASH
#include "Hashes/rapidhash.h"
#elif defined USE_MURMUR
#include "Hashes/murmur32.h"
#elif defined USE_CRC32
#include "Hashes/crc32.h"
#else
_Static_assert(0, "unreachable");
#endif


#ifndef DRP_FLATTEN
#if defined __has_attribute
#if __has_attribute(flatten)
#define DRP_FLATTEN __attribute__((flatten))
#endif
#endif
#endif

#ifndef DRP_FLATTEN
#define DRP_FLATTEN
#endif


DRP_FLATTEN
static inline
uint32_t
drp_hash(const void *_Null_unspecified key, size_t len){
    #if defined USE_RAPIDHASH
        return (uint32_t)rapidhash_hash(key, len);
    #elif defined USE_MURMUR
        return murmur_hash(key, len);
    #elif defined USE_CRC32
        return crc32_hash(key, len);
    #else
        _Static_assert(0, "unreachable");
    #endif
}

DRP_FLATTEN
static inline
uint32_t
drp_hash_ascii_insensitive(const void *_Null_unspecified key, size_t len){
    #if defined USE_RAPIDHASH
        return (uint32_t)rapidhash_hash_ascii_insensitive(key, len);
    #elif defined USE_MURMUR
        return murmur_hash_ascii_insensitive(key, len);
    #elif defined USE_CRC32
        return crc32_hash_ascii_insensitive(key, len);
    #else
        _Static_assert(0, "unreachable");
    #endif
}

force_inline
uint32_t
fast_reduce32(uint32_t x, uint32_t y){
    return ((uint64_t)x * (uint64_t)y) >> 32;
}

#endif
