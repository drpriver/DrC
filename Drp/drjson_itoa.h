//
// Copyright © 2022-2024, David Priver <david@davidpriver.com>
//
#ifndef DRJSON_ITOA_H
#define DRJSON_ITOA_H
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#ifndef drj_memcpy
#ifndef __GNUC__
#define drj_memset memset
#define drj_memcpy memcpy
#define drj_memmove memmove
#else
#define drj_memset __builtin_memset
#define drj_memcpy __builtin_memcpy
#define drj_memmove __builtin_memmove
#endif
#endif
// The first 100 characters of 00 - 99.
// Assumes a little endian cpu. (0x3733 translates to the string '37').
// 0x30 is '0'.
static const uint16_t ZERO_TO_NINETY_NINE[] = {
    0x3030, 0x3130, 0x3230, 0x3330, 0x3430, 0x3530, 0x3630, 0x3730, 0x3830, 0x3930,
    0x3031, 0x3131, 0x3231, 0x3331, 0x3431, 0x3531, 0x3631, 0x3731, 0x3831, 0x3931,
    0x3032, 0x3132, 0x3232, 0x3332, 0x3432, 0x3532, 0x3632, 0x3732, 0x3832, 0x3932,
    0x3033, 0x3133, 0x3233, 0x3333, 0x3433, 0x3533, 0x3633, 0x3733, 0x3833, 0x3933,
    0x3034, 0x3134, 0x3234, 0x3334, 0x3434, 0x3534, 0x3634, 0x3734, 0x3834, 0x3934,
    0x3035, 0x3135, 0x3235, 0x3335, 0x3435, 0x3535, 0x3635, 0x3735, 0x3835, 0x3935,
    0x3036, 0x3136, 0x3236, 0x3336, 0x3436, 0x3536, 0x3636, 0x3736, 0x3836, 0x3936,
    0x3037, 0x3137, 0x3237, 0x3337, 0x3437, 0x3537, 0x3637, 0x3737, 0x3837, 0x3937,
    0x3038, 0x3138, 0x3238, 0x3338, 0x3438, 0x3538, 0x3638, 0x3738, 0x3838, 0x3938,
    0x3039, 0x3139, 0x3239, 0x3339, 0x3439, 0x3539, 0x3639, 0x3739, 0x3839, 0x3939,};
_Static_assert(sizeof(ZERO_TO_NINETY_NINE)==200, "");
_Static_assert(sizeof(uint16_t)==2, "");

static inline
size_t
drjson_uint64_to_ascii(char* buff, uint64_t value){
    char tmp[20];
    char* p = tmp+sizeof(tmp);
    while(value >= 100){
        uint64_t old = value;
        p -= 2;
        value /= 100;
        uint64_t last_two_digits = old - 100*value;
        drj_memcpy(p, &ZERO_TO_NINETY_NINE[last_two_digits], 2);
    }
    p -= 2;
    drj_memcpy(p, &ZERO_TO_NINETY_NINE[value], 2);
    p += value < 10;
    size_t length = (tmp + sizeof(tmp)) - p;
    drj_memcpy(buff, p, length);
    return length;
}

static inline
size_t
drjson_uint64_to_hex(char* buff, uint64_t value){
    uint8_t* bytes = (uint8_t*)&value;
#define HEX "0123456789abcdef"
    char* p = buff;
    *p++ = '0';
    *p++ = 'x';
    _Bool non_zero = 0;
    for(size_t i = 7; i--;){
        uint8_t byte = bytes[i];
        if(!byte && !non_zero) continue;
        uint8_t bottom = byte & 0xf;
        uint8_t top = (byte & 0xf0)>>4;
        *p++ = HEX[top];
        *p++ = HEX[bottom];
        non_zero = 1;
    }
    if(!non_zero)
        *p++ = '0';
    return p - buff;
}

static inline
size_t
drjson_int64_to_ascii(char* buff, int64_t value){
    if(value == INT64_MIN){
        drj_memcpy(buff, "-9223372036854775808", sizeof("-9223372036854775808")-1);
        return sizeof("-9223372036854775808")-1;
    }
    int neg = value < 0;
    if(neg){
        value = -value;
        *buff = '-';
        buff++;
    }
    return neg + drjson_uint64_to_ascii(buff, (uint64_t)value);
}
#endif
