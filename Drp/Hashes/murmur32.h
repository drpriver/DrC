#ifndef DRP_HASHES_MURMUR_H
#define DRP_HASHES_MURMUR_H
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#ifndef force_inline
#if defined __GNUC__ || defined __clang__ || defined __DRC__
#define force_inline static inline __attribute__((always_inline))
#elif defined(_MSC_VER)
#define force_inline static inline __forceinline
#else
#define force_inline static inline
#endif
#endif

#if !defined __clang__ && !defined _Null_unspecified
#define _Null_unspecified
#endif

#ifndef drp_memcpy_inline
    #if defined __has_builtin
        #if __has_builtin(__builtin_memcpy_inline)
            #define drp_memcpy_inline __builtin_memcpy_inline
        #elif __has_builtin(__builtin_memcpy)
            #define drp_memcpy_inline __builtin_memcpy
        #endif
    #endif
#endif
#if !defined drp_memcpy_inline && (defined __GNUC__ || defined __clang__)
    #define drp_memcpy_inline __builtin_memcpy
#endif
#ifndef drp_memcpy_inline
    #define drp_memcpy_inline memcpy
#endif

// cut'n'paste from the wikipedia page on murmur hash
force_inline
uint32_t
murmur_32_scramble(uint32_t k) {
    k *= 0xcc9e2d51;
    k = (k << 15) | (k >> 17);
    k *= 0x1b873593;
    return k;
}

static inline
uint32_t
murmur_hash(const void*_Null_unspecified key_, size_t len){
    const uint8_t* key = key_;
    uint32_t seed = 4253307714;
	uint32_t h = seed;
    uint32_t k;
    /* Read in groups of 4. */
    for (size_t i = len >> 2; i; i--) {
        drp_memcpy_inline(&k, key, 4);
        key += sizeof(uint32_t);
        h ^= murmur_32_scramble(k);
        h = (h << 13) | (h >> 19);
        h = h * 5 + 0xe6546b64;
    }
    /* Read the rest. */
    k = 0;
    for (size_t i = len & 3; i; i--) {
        k <<= 8;
        k |= key[i - 1];
    }
    h ^= murmur_32_scramble(k);
    /* Finalize. */
	h ^= len;
	h ^= h >> 16;
	h *= 0x85ebca6b;
	h ^= h >> 13;
	h *= 0xc2b2ae35;
	h ^= h >> 16;
	return h;
}

static inline
uint32_t
murmur_hash_ascii_insensitive(const void*_Null_unspecified key_, size_t len){
    const uint8_t* key = key_;
    uint32_t seed = 4253307714;
	uint32_t h = seed;
    uint32_t k;
    /* Read in groups of 4. */
    for (size_t i = len >> 2; i; i--) {
        drp_memcpy_inline(&k, key, 4);
        k |= 0x20202020u;
        key += sizeof(uint32_t);
        h ^= murmur_32_scramble(k);
        h = (h << 13) | (h >> 19);
        h = h * 5 + 0xe6546b64;
    }
    /* Read the rest. */
    k = 0;
    for (size_t i = len & 3; i; i--) {
        k <<= 8;
        k |= 0x20u|key[i - 1];
    }
    h ^= murmur_32_scramble(k);
    /* Finalize. */
	h ^= len;
	h ^= h >> 16;
	h *= 0x85ebca6b;
	h ^= h >> 13;
	h *= 0xc2b2ae35;
	h ^= h >> 16;
	return h;
}
#endif
