#ifndef DRP_CRC32C_H
#define DRP_CRC32C_H
#include <stdint.h>
#if !defined(__DRC__) && defined(__ARM_ACLE) && __ARM_FEATURE_CRC32
#include <arm_acle.h>
#elif !defined(__DRC__) && defined(__SSE4_2__)
#include <nmmintrin.h>
#endif

// Raw Castagnoli CRC update: least significant byte first, no complements.
static inline 
uint32_t
drp_crc32c_software(uint32_t crc, uint64_t value, unsigned size){
    for(unsigned i = 0; i < size; i++, value >>= 8){
        crc ^= (uint8_t)value;
        for(unsigned bit = 0; bit < 8; bit++)
            crc = (crc >> 1) ^ (0x82f63b78u & (0u - (crc & 1)));
    }
    return crc;
}

static inline 
uint32_t
drp_crc32c(uint32_t crc, uint64_t value, unsigned size){
    #if defined(__DRC__)
        switch(size){
            case 1: return __builtin_crc32c8(crc, value);
            case 2: return __builtin_crc32c16(crc, value);
            case 4: return __builtin_crc32c32(crc, value);
            case 8: return __builtin_crc32c64(crc, value);
        }
    #elif defined(__ARM_ACLE) && __ARM_FEATURE_CRC32
        switch(size){
            case 1: return __crc32cb(crc, (uint8_t)value);
            case 2: return __crc32ch(crc, (uint16_t)value);
            case 4: return __crc32cw(crc, (uint32_t)value);
            case 8: return __crc32cd(crc, value);
        }
    #elif defined(__SSE4_2__)
        switch(size){
            case 1: return _mm_crc32_u8(crc, (uint8_t)value);
            case 2: return _mm_crc32_u16(crc, (uint16_t)value);
            case 4: return _mm_crc32_u32(crc, (uint32_t)value);
            #if defined(__x86_64__)
                case 8: return (uint32_t)_mm_crc32_u64(crc, value);
            #else
                case 8: return _mm_crc32_u32(_mm_crc32_u32(crc, (uint32_t)value), (uint32_t)(value >> 32));
            #endif
        }
    #endif
    return drp_crc32c_software(crc, value, size);
}
#endif
