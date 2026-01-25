#ifndef DRP_BITFIELDS_H
#define DRP_BITFIELDS_H
#include <stdint.h>
#include <stddef.h>

static inline
uint64_t
read_bitfield(const void* src, size_t sz, size_t bitsize, size_t bitoffset, _Bool is_signed){
    uint64_t result;
    // Switch on size to avoid memcpy overhead - only 1,2,4,8 are legal
    switch(sz){
        case 1: result = *(const uint8_t*)src; break;
        case 2: result = *(const uint16_t*)src; break;
        case 4: result = *(const uint32_t*)src; break;
        case 8: result = *(const uint64_t*)src; break;
        default:
            #ifdef _MSC_VER
            __assume(0);
            #else
            __builtin_unreachable();
            #endif
    }
    result >>= bitoffset;
    if(bitsize != 64)
        result &= ((uint64_t)1 << bitsize)-1;
    if(is_signed){
        uint64_t signext = (uint64_t)-1 << (bitsize-1);
        if(result & signext)
            result |= signext;
    }
    return result;
}

static inline
void
write_bitfield(void* dst, uint64_t val, size_t sz, size_t bitsize, size_t bitoffset){
    uint64_t mask = -1;
    if(bitsize < 64)
        mask = ((uint64_t)1 << bitsize) - 1;
    val = (val & mask) << bitoffset;
    uint64_t clearmask = ~(mask << bitoffset);

    // Switch on size to avoid memcpy overhead - only 1,2,4,8 are legal
    switch(sz){
        case 1:{
            uint8_t v = *(uint8_t*)dst;
            v = (uint8_t)((v & clearmask) | val);
            *(uint8_t*)dst = v;
            break;
        }
        case 2:{
            uint16_t v = *(uint16_t*)dst;
            v = (uint16_t)((v & clearmask) | val);
            *(uint16_t*)dst = v;
            break;
        }
        case 4:{
            uint32_t v = *(uint32_t*)dst;
            v = (uint32_t)((v & clearmask) | val);
            *(uint32_t*)dst = v;
            break;
        }
        case 8:{
            uint64_t v = *(uint64_t*)dst;
            v = (v & clearmask) | val;
            *(uint64_t*)dst = v;
            break;
        }
        default:
            #ifdef _MSC_VER
            __assume(0);
            #else
            __builtin_unreachable();
            #endif
    }
}

#endif
