#ifndef DRP_HASHES_CRC32_H
#define DRP_HASHES_CRC32_H
#include <stdint.h>
#include <string.h>
#if !defined(__DRC__) && defined(__ARM_ACLE) && __ARM_FEATURE_CRC32
#include <arm_acle.h>
#elif !defined(__DRC__) && defined(__SSE4_2__)
#include <nmmintrin.h>
#endif

#if !defined __clang__ && !defined _Null_unspecified
#define _Null_unspecified
#endif

#ifndef unreachable
#if defined(__GNUC__) || defined(__clang__)
#define unreachable() __builtin_unreachable()
#else
#define unreachable() __assume(0)
#endif
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


force_inline
uint32_t
crc32c_software(uint32_t crc, uint64_t value, unsigned size){
    for(unsigned i = 0; i < size; i++, value >>= 8){
        crc ^= (uint8_t)value;
        for(unsigned bit = 0; bit < 8; bit++)
            crc = (crc >> 1) ^ (0x82f63b78u & (0u - (crc & 1)));
    }
    return crc;
}

// Use hardware intrinsics if available.
#ifdef __DRC__
    force_inline uint32_t crc32_u64(uint32_t h, uint64_t v){ return __builtin_crc32c64(h, v); }
    force_inline uint32_t crc32_u32(uint32_t h, uint32_t v){ return __builtin_crc32c32(h, v); }
    force_inline uint32_t crc32_u16(uint32_t h, uint16_t v){ return __builtin_crc32c16(h, v); }
    force_inline uint32_t crc32_u8 (uint32_t h, uint8_t  v){ return __builtin_crc32c8(h, v); }
#elif defined __ARM_ACLE && __ARM_FEATURE_CRC32
    force_inline uint32_t crc32_u64(uint32_t h, uint64_t v){ return __crc32cd(h, v); }
    force_inline uint32_t crc32_u32(uint32_t h, uint32_t v){ return __crc32cw(h, v); }
    force_inline uint32_t crc32_u16(uint32_t h, uint16_t v){ return __crc32ch(h, v); }
    force_inline uint32_t crc32_u8 (uint32_t h, uint8_t  v){ return __crc32cb(h, v); }
#elif ((defined __x86_64__ && defined __SSE4_2__) || (defined _M_AMD64 && defined _MSC_VER && !defined __clang__))
    #ifndef __clang__
        force_inline uint32_t crc32_u64(uint32_t h, uint64_t v){ return _mm_crc32_u64(h, v); }
        force_inline uint32_t crc32_u32(uint32_t h, uint32_t v){ return _mm_crc32_u32(h, v); }
        force_inline uint32_t crc32_u16(uint32_t h, uint16_t v){ return _mm_crc32_u16(h, v); }
        force_inline uint32_t crc32_u8 (uint32_t h, uint8_t  v){ return _mm_crc32_u8 (h, v); }
    #else
        force_inline uint32_t crc32_u64(uint32_t h, uint64_t v){ return (uint32_t)__builtin_ia32_crc32di(h, v); }
        force_inline uint32_t crc32_u32(uint32_t h, uint32_t v){ return __builtin_ia32_crc32si(h, v); }
        force_inline uint32_t crc32_u16(uint32_t h, uint16_t v){ return __builtin_ia32_crc32hi(h, v); }
        force_inline uint32_t crc32_u8 (uint32_t h, uint8_t  v){ return __builtin_ia32_crc32qi(h, v); }
    #endif
#else
    force_inline uint32_t crc32_u64(uint32_t h, uint64_t v){ return crc32c_software(h, v, 8); }
    force_inline uint32_t crc32_u32(uint32_t h, uint32_t v){ return crc32c_software(h, v, 4); }
    force_inline uint32_t crc32_u16(uint32_t h, uint16_t v){ return crc32c_software(h, v, 2); }
    force_inline uint32_t crc32_u8 (uint32_t h, uint8_t  v){ return crc32c_software(h, v, 1); }
#endif

force_inline
uint32_t
drp_crc32c(uint32_t crc, uint64_t value, unsigned size){
    switch(size){
        case 8: return crc32_u64(crc, value);
        case 4: return crc32_u32(crc, (uint32_t)value);
        case 2: return crc32_u16(crc, (uint16_t)value);
        case 1: return crc32_u8 (crc, (uint8_t)value);
        default: unreachable();
    }
}

force_inline
uint32_t
crc32_hash(const void*_Null_unspecified key, size_t len){
    const unsigned char* k = key;
    uint32_t h = 0;
    for(;len >= 8; k+=8, len-=8){
        uint64_t v; drp_memcpy_inline(&v, k, 8);
        h = crc32_u64(h, v);
    }
    for(;len >= 4; k+=4, len-=4){
        uint32_t v; drp_memcpy_inline(&v, k, 4);
        h = crc32_u32(h, v);
    }
    for(;len >= 2; k+=2, len-=2){
        uint16_t v; drp_memcpy_inline(&v, k, 2);
        h = crc32_u16(h, v);
    }
    if(len)
        h = crc32_u8(h, *k);
    return h;
}

force_inline
uint32_t
crc32_hash_ascii_insensitive(const void*_Null_unspecified key, size_t len){
    const unsigned char* k = key;
    uint32_t h = 0;
    for(;len >= 8; k+=8, len-=8){
        uint64_t v; drp_memcpy_inline(&v, k, 8);
        h = crc32_u64(h, 0x2020202020202020u|v);
    }
    for(;len >= 4; k+=4, len-=4){
        uint32_t v; drp_memcpy_inline(&v, k, 4);
        h = crc32_u32(h, 0x20202020u|v);
    }
    for(;len >= 2; k+=2, len-=2){
        uint16_t v; drp_memcpy_inline(&v, k, 2);
        h = crc32_u16(h, 0x2020u|v);
    }
    if(len)
        h = crc32_u8(h, 0x20u|*k);
    return h;
}


#endif
