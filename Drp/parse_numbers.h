//
// Copyright © 2022-2025, David Priver <david@davidpriver.com>
//
#ifndef PARSE_NUMBERS_H
#define PARSE_NUMBERS_H
// size_t
#include <stddef.h>
// integer types, INT64_MAX, etc.
#include <stdint.h>
#include "typed_enum.h"

// Allow suppression of float parsing.
#ifndef PARSE_NUMBER_PARSE_FLOATS
#define PARSE_NUMBER_PARSE_FLOATS 1
#endif

#if PARSE_NUMBER_PARSE_FLOATS
#include "fast_float.h"
#endif

#include "ckdint.h"

//
// Functions for parsing strings into integers.
//
// Features:
//    * Uses string + length instead of nul-terminated strings.
//    * Ignores locale.
//    * Doesn't use errno.
//    * Handles binary notation, hex notation, #hex notation, decimal.
//

#ifdef __clang__
#pragma clang assume_nonnull begin
#endif

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning( disable : 4146 )
#endif

#ifndef warn_unused

#if defined(__GNUC__) || defined(__clang__)
#define warn_unused __attribute__((warn_unused_result))
#elif defined(_MSC_VER)
#define warn_unused
#else
#define warn_unused
#endif

#endif

// NOTE: The first error that is happened to be detected will be reported.
//       For strings with multiple errors, this is arbitrary.
//       For example, parsing '12dddddddddddddddddddddd12' into an int
//       may report OVERFLOWED_VALUE instead of INVALID_CHARACTER if the length
//       check is done first. There is no guarantee over which particular error
//       is reported, just that an error will be reported for invalid input.
enum ParseNumberError TYPED_ENUM(int) {
    // No error.
    PARSENUMBER_NO_ERROR = 0,
    // Input ended when more input was expected.
    // For example, parsing '0x' as an unsigned, more data is expected after the 'x'.
    PARSENUMBER_UNEXPECTED_END = 1,
    // The result does not fit in the data type.
    PARSENUMBER_OVERFLOWED_VALUE = 2,
    // An invalid character was encountered, like the 'a' in '33a2' when
    // parsing an int.
    PARSENUMBER_INVALID_CHARACTER = 3,
};
TYPEDEF_ENUM(ParseNumberError, int);

//
// Forward declarations of the public API.

//
// All of these functions take pointer + length for strings. They will not read
// beyond the given length and the pointer does not need to be nul-terminated.
// They do not accept trailing or trailing whitespace.
// Decimal formats accept a single leading '+' or '-'.
// These functions can parse the full range of values for the given data type.

//
// Parses a decimal uint64.
static inline
warn_unused
ParseNumberError
parse_uint64(const char* str, size_t length, uint64_t* result);

//
// Parses a decimal int64.
static inline
warn_unused
ParseNumberError
parse_int64(const char* str, size_t length, int64_t* result);

//
// Parses a decimal uint32.
static inline
warn_unused
ParseNumberError
parse_uint32(const char*str, size_t length, uint32_t* result);


//
// Parses a decimal int32.
static inline
warn_unused
ParseNumberError
parse_int32(const char*str, size_t length, int32_t* result);

//
// Parses a decimal int.
static inline
warn_unused
ParseNumberError
parse_int(const char* str, size_t length, int* result);

//
// Parses hex format, but with a leading '#' instead of '0x'.
static inline
warn_unused
ParseNumberError
parse_pound_hex(const char* str, size_t length, uint64_t* result);

//
// Parses traditional hex format, such as '0xf00dface'
static inline
warn_unused
ParseNumberError
parse_hex(const char* str, size_t length, uint64_t* result);

//
// Parses binary notation, such as '0b1101'.
static inline
warn_unused
ParseNumberError
parse_binary(const char* str, size_t length, uint64_t* result);

//
// Parses octal notation, such as '0o755' or '0O755'.
static inline
warn_unused
ParseNumberError
parse_octal(const char* str, size_t length, uint64_t* result);

//
// Parses an unsigned integer, in whatever format is comfortable for a human.
// Accepts 0x hexes, 0b binary, plain decimals, and also # hexes.
static inline
warn_unused
ParseNumberError
parse_unsigned_human(const char* str, size_t length, uint64_t* result);

#if PARSE_NUMBER_PARSE_FLOATS
//
// Parses a float, in regular or scientific form.
static inline
warn_unused
ParseNumberError
parse_float(const char* str, size_t length, float* result);

//
// Parses a double, in regular or scientific form.
static inline
warn_unused
ParseNumberError
parse_double(const char* str, size_t length, double* result);
#endif


// Implementations after this point.

static inline
warn_unused
ParseNumberError
parse_uint64(const char* str, size_t length, uint64_t* result){
    *result = 0;
    if(!length)
        return PARSENUMBER_UNEXPECTED_END;
    if(*str == '+'){
        str++;
        length--;
    }
    if(!length)
        return PARSENUMBER_UNEXPECTED_END;
    // UINT64_MAX is 18,446,744,073,709,551,615 (20 characters)
    if(length > 20)
        return PARSENUMBER_OVERFLOWED_VALUE;
    int bad = 0;
    uint64_t value = 0;
    for(size_t i=0;i < length-1; i++){
        unsigned cval = (unsigned char)str[i];
        cval -= '0';
        if(cval > 9u)
            bad = 1;
        value *= 10;
        value += cval;
    }
    if(bad)
        return PARSENUMBER_INVALID_CHARACTER;
    // Handle the last char differently as it's the only
    // one that can overflow.
    {
        unsigned cval = (unsigned char)str[length-1];
        cval -= '0';
        if(cval > 9u)
            return PARSENUMBER_INVALID_CHARACTER;
        if(mul_overflow(value, 10, &value))
            return PARSENUMBER_OVERFLOWED_VALUE;
        if(add_overflow(value, cval, &value))
            return PARSENUMBER_OVERFLOWED_VALUE;

    }
    *result = value;
    return PARSENUMBER_NO_ERROR;
}

static inline
warn_unused
ParseNumberError
parse_int64(const char* str, size_t length, int64_t* result){
    *result = 0;
    if(!length)
        return PARSENUMBER_UNEXPECTED_END;
    _Bool negative = (*str == '-');
    if(negative){
        str++;
        length--;
    }
    else if(*str == '+'){
        str++;
        length--;
    }
    if(!length)
        return PARSENUMBER_UNEXPECTED_END;
    // INT64_MAX is 9223372036854775807 (19 characters)
    if(length > 19)
        return PARSENUMBER_OVERFLOWED_VALUE;
    int bad = 0;
    uint64_t value = 0;
    for(size_t i=0;i < length-1; i++){
        unsigned cval = (unsigned char)str[i];
        cval -= '0';
        if(cval > 9u)
            bad = 1;
        value *= 10;
        value += cval;
    }
    if(bad)
        return PARSENUMBER_INVALID_CHARACTER;
    // Handle the last char differently as it's the only
    // one that can overflow.
    {
        unsigned cval = (unsigned char)str[length-1];
        cval -= '0';
        if(cval > 9u)
            return PARSENUMBER_INVALID_CHARACTER;
        if(mul_overflow(value, 10, &value))
            return PARSENUMBER_OVERFLOWED_VALUE;
        if(add_overflow(value, cval, &value))
            return PARSENUMBER_OVERFLOWED_VALUE;
    }
    if(negative){
        if(value > (uint64_t)INT64_MAX+1)
            return PARSENUMBER_OVERFLOWED_VALUE;
        *result = (int64_t)-value;
    }
    else{
        if(value > (uint64_t)INT64_MAX)
            return PARSENUMBER_OVERFLOWED_VALUE;
        *result = (int64_t)value;
    }
    return PARSENUMBER_NO_ERROR;
}

static inline
warn_unused
ParseNumberError
parse_uint32(const char*str, size_t length, uint32_t* result){
    *result = 0;
    if(!length)
        return PARSENUMBER_UNEXPECTED_END;
    if(*str == '+'){
        str++;
        length--;
    }
    if(!length)
        return PARSENUMBER_UNEXPECTED_END;
    // UINT32_MAX is 10 characters
    if(length > 10)
        return PARSENUMBER_OVERFLOWED_VALUE;
    int bad = 0;
    uint32_t value = 0;
    for(size_t i=0;i < length-1; i++){
        unsigned cval = (unsigned char)str[i];
        cval -= '0';
        if(cval > 9u)
            bad = 1;
        value *= 10;
        value += cval;
    }
    if(bad)
        return PARSENUMBER_INVALID_CHARACTER;
    // Handle the last char differently as it's the only
    // one that can overflow.
    {
        unsigned cval = (unsigned char)str[length-1];
        cval -= '0';
        if(cval > 9u)
            return PARSENUMBER_INVALID_CHARACTER;
        if(mul_overflow(value, 10, &value))
            return PARSENUMBER_OVERFLOWED_VALUE;
        if(add_overflow(value, cval, &value))
            return PARSENUMBER_OVERFLOWED_VALUE;
    }
    *result = value;
    return PARSENUMBER_NO_ERROR;
}

static inline
warn_unused
ParseNumberError
parse_int32(const char*str, size_t length, int32_t* result){
    *result = 0;
    if(!length)
        return PARSENUMBER_UNEXPECTED_END;
    _Bool negative = (*str == '-');
    if(negative){
        str++;
        length--;
    }
    else if (*str == '+'){
        str++;
        length--;
    }
    if(!length)
        return PARSENUMBER_UNEXPECTED_END;
    // INT32_max is 10 chars
    if(length > 10)
        return PARSENUMBER_OVERFLOWED_VALUE;
    int bad = 0;
    uint32_t value = 0;
    for(size_t i=0;i < length-1; i++){
        unsigned cval = (unsigned char)str[i];
        cval -= '0';
        if(cval > 9u)
            bad = 1;
        value *= 10;
        value += cval;
    }
    if(bad)
        return PARSENUMBER_INVALID_CHARACTER;
    // Handle the last char differently as it's the only
    // one that can overflow.
    {
        unsigned cval = (unsigned char)str[length-1];
        cval -= '0';
        if(cval > 9u)
            return PARSENUMBER_INVALID_CHARACTER;
        if(mul_overflow(value, 10, &value))
            return PARSENUMBER_OVERFLOWED_VALUE;
        if(add_overflow(value, cval, &value))
            return PARSENUMBER_OVERFLOWED_VALUE;
    }
    if(negative){
        if(value > (uint32_t)INT32_MAX+1)
            return PARSENUMBER_OVERFLOWED_VALUE;
        *result = (int32_t)-value;
    }
    else{
        if(value > (uint32_t)INT32_MAX)
            return PARSENUMBER_OVERFLOWED_VALUE;
        *result = (int32_t)value;
    }
    return PARSENUMBER_NO_ERROR;
}

static inline
warn_unused
ParseNumberError
parse_int(const char* str, size_t length, int* result){
    int32_t r;
    ParseNumberError err = parse_int32(str, length, &r);
    *result = r;
    return err;
}

static inline
warn_unused
ParseNumberError
parse_hex_inner(const char* str, size_t length, uint64_t* result){
    *result = 0;
    if(length > sizeof(*result)*2)
        return PARSENUMBER_OVERFLOWED_VALUE;
    uint64_t value = 0;
    for(size_t i = 0; i < length; i++){
        char c = str[i];
        uint64_t char_value;
        switch(c){
            case '0': case '1': case '2': case '3': case '4':
            case '5': case '6': case '7': case '8': case '9':
                char_value = (uint64_t)(c - '0');
                break;
            case 'a': case 'b': case 'c': case 'd':
            case 'e': case 'f':
                char_value = (uint64_t)(c - 'a' + 10);
                break;
            case 'A': case 'B': case 'C': case 'D':
            case 'E': case 'F':
                char_value = (uint64_t)(c - 'A' + 10);
                break;
            default:
                return PARSENUMBER_INVALID_CHARACTER;
        }
        value <<= 4;
        value |= char_value;
    }
    *result = value;
    return PARSENUMBER_NO_ERROR;
}

static inline
warn_unused
ParseNumberError
parse_pound_hex(const char* str, size_t length, uint64_t* result){
    *result = 0;
    if(length < 2)
        return PARSENUMBER_UNEXPECTED_END;
    if(str[0] != '#')
        return PARSENUMBER_INVALID_CHARACTER;
    return parse_hex_inner(str+1, length-1, result);
}

static inline
warn_unused
ParseNumberError
parse_hex(const char* str, size_t length, uint64_t* result){
    *result = 0;
    if(length<3)
        return PARSENUMBER_UNEXPECTED_END;
    if(str[0] != '0')
        return PARSENUMBER_INVALID_CHARACTER;
    if(str[1] != 'x' && str[1] != 'X')
        return PARSENUMBER_INVALID_CHARACTER;
    return parse_hex_inner(str+2, length-2, result);
}

static inline warn_unused ParseNumberError parse_binary_inner(const char*, size_t, uint64_t*);
static inline warn_unused ParseNumberError parse_octal_inner(const char*, size_t, uint64_t*);

static inline
warn_unused
ParseNumberError
parse_binary(const char* str, size_t length, uint64_t* result){
    *result = 0;
    if(length<3)
        return PARSENUMBER_UNEXPECTED_END;
    if(str[0] != '0')
        return PARSENUMBER_INVALID_CHARACTER;
    if(str[1] != 'b' && str[1] != 'B')
        return PARSENUMBER_INVALID_CHARACTER;
    return parse_binary_inner(str+2, length-2, result);
}

static inline
warn_unused
ParseNumberError
parse_binary_inner(const char* str, size_t length, uint64_t* result){
    *result = 0;
    if(!length)
        return PARSENUMBER_UNEXPECTED_END;
    if(length > 64)
        return PARSENUMBER_OVERFLOWED_VALUE;
    unsigned long long mask = 1llu << 63;
    mask >>= (64 - length);
    // @speed
    // 2**4 is only 16, so we could definitely
    // read 4 bytes at a time and then do a fixup.
    // You'd have to see what code is generated though
    // (does the compiler turn it into a binary decision tree?)
    // 2**8 is only 256, that's probably not worth it.
    uint64_t value = 0;
    for(size_t i = 0; i < length; i++, mask>>=1){
        switch(str[i]){
            case '1':
                value |= mask;
                continue;
            case '0':
                continue;
            default:
                return PARSENUMBER_INVALID_CHARACTER;
        }
    }
    *result = value;
    return PARSENUMBER_NO_ERROR;
}


static inline
warn_unused
ParseNumberError
parse_octal(const char* str, size_t length, uint64_t* result){
    *result = 0;
    if(length < 3)
        return PARSENUMBER_UNEXPECTED_END;
    if(str[0] != '0')
        return PARSENUMBER_INVALID_CHARACTER;
    if(str[1] != 'o' && str[1] != 'O')
        return PARSENUMBER_INVALID_CHARACTER;
    return parse_octal_inner(str+2, length-2, result);
}

static inline
warn_unused
ParseNumberError
parse_octal_inner(const char* str, size_t length, uint64_t* result){
    *result = 0;
    if(!length)
        return PARSENUMBER_UNEXPECTED_END;
    // UINT64_MAX is 1777777777777777777777 in octal (22 digits)
    if(length > 22)
        return PARSENUMBER_OVERFLOWED_VALUE;
    int bad = 0;
    uint64_t value = 0;
    for(size_t i = 0; i < length-1; i++){
        unsigned cval = (unsigned char)str[i];
        cval -= '0';
        if(cval > 7u)
            bad = 1;
        value <<= 3;
        value |= cval & 7u;
    }
    if(bad)
        return PARSENUMBER_INVALID_CHARACTER;
    // Handle the last digit with overflow checking.
    {
        unsigned cval = (unsigned char)str[length-1];
        cval -= '0';
        if(cval > 7u)
            return PARSENUMBER_INVALID_CHARACTER;
        if(mul_overflow(value, (uint64_t)8, &value))
            return PARSENUMBER_OVERFLOWED_VALUE;
        if(add_overflow(value, (uint64_t)cval, &value))
            return PARSENUMBER_OVERFLOWED_VALUE;
    }
    *result = value;
    return PARSENUMBER_NO_ERROR;
}

static inline
warn_unused
ParseNumberError
parse_unsigned_human(const char* str, size_t length, uint64_t* result){
    *result = 0;
    if(!length)
        return PARSENUMBER_UNEXPECTED_END;
    if(str[0] == '#')
        return parse_pound_hex(str, length, result);
    if(str[0] == '0' && length > 1){
        if(str[1] == 'x' || str[1] == 'X')
            return parse_hex(str, length, result);
        if(str[1] == 'b' || str[1] == 'B')
            return parse_binary(str, length, result);
    }
    return parse_uint64(str, length, result);
}

#if PARSE_NUMBER_PARSE_FLOATS
static inline
warn_unused
ParseNumberError
parse_float(const char* str, size_t length, float* result){
    *result = 0;
    const char* end = str + length;
    // fast_float doesn't accept leading '+', but we want to.
    while(str != end && *str == '+')
        str++;
    if(str == end)
        return PARSENUMBER_UNEXPECTED_END;
    fast_float_from_chars_result fr = fast_float_from_chars_float(str, end, result, FASTFLOAT_FORMAT_GENERAL);
    if(fr.ptr != end)
        return PARSENUMBER_INVALID_CHARACTER;
    switch(fr.error){
        case FASTFLOAT_NO_ERROR:
            return PARSENUMBER_NO_ERROR;
        case FASTFLOAT_INVALID_VALUE:
            return PARSENUMBER_INVALID_CHARACTER;
        default:
        case FASTFLOAT_BAD_FORMAT:
            // This should never happen, but meh.
            return PARSENUMBER_INVALID_CHARACTER;
    }
}

//
// Parses a double, in regular or scientific form.
static inline
warn_unused
ParseNumberError
parse_double(const char* str, size_t length, double* result){
    *result = 0;
    const char* end = str + length;
    // fast_float doesn't accept leading '+', but we want to.
    while(str != end && *str == '+')
        str++;
    if(str == end)
        return PARSENUMBER_UNEXPECTED_END;
    fast_float_from_chars_result fr = fast_float_from_chars_double(str, end, result, FASTFLOAT_FORMAT_GENERAL);
    if(fr.ptr != end)
        return PARSENUMBER_INVALID_CHARACTER;
    switch(fr.error){
        case FASTFLOAT_NO_ERROR:
            return PARSENUMBER_NO_ERROR;
        case FASTFLOAT_INVALID_VALUE:
            return PARSENUMBER_INVALID_CHARACTER;
        default:
        case FASTFLOAT_BAD_FORMAT:
            // This should never happen, but meh.
            return PARSENUMBER_INVALID_CHARACTER;
    }
}
#endif
#ifdef __clang__
#pragma clang assume_nonnull end
#endif

#ifdef _MSC_VER
#pragma warning(pop)
#endif

#endif
