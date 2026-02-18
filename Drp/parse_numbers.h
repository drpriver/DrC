//
// Copyright © 2022-2025, David Priver <david@davidpriver.com>
//
#ifndef PARSE_NUMBERS_H
#define PARSE_NUMBERS_H
// size_t
#include <stddef.h>
// integer types, INT64_MAX, etc.
#include <stdint.h>

// Allow suppression of float parsing.
#ifndef PARSE_NUMBER_PARSE_FLOATS
#define PARSE_NUMBER_PARSE_FLOATS 1
#endif

#if PARSE_NUMBER_PARSE_FLOATS
#include "fast_float.h"
#endif

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

#if defined(__wasm__)
#define __builtin_mul_overflow(a, b, c) ((void)((*c) = a * b), 0)
#define __builtin_add_overflow(a, b, c) ((void)((*c) = a + b), 0)
#endif

#if defined(__IMPORTC__)
__import core.checkedint;

static inline
int
__builtin_mul_overflow_32(uint32_t a, uint32_t b, uint32_t* dst){
    _Bool overflowed = 0;
    *dst = mulu(a, b, overflowed);
    return overflowed;
}
static inline
int
__builtin_mul_overflow_64(uint64_t a, uint64_t b, uint64_t* dst){
    _Bool overflowed = 0;
    *dst = mulu(a, b, overflowed);
    return overflowed;
}
#define __builtin_mul_overflow(a, b, dst) _Generic(a, \
    uint32_t: __builtin_mul_overflow_32, \
    uint64_t: __builtin_mul_overflow_64)(a, b, dst)

static inline
int
__builtin_add_overflow_32(uint32_t a, uint32_t b, uint32_t* dst){
    _Bool overflowed = 0;
    *dst = addu(a, b, overflowed);
    return overflowed;
}
static inline
int
__builtin_add_overflow_64(uint64_t a, uint64_t b, uint64_t* dst){
    _Bool overflowed = 0;
    *dst = addu(a, b, overflowed);
    return overflowed;
}
#define __builtin_add_overflow(a, b, dst) _Generic(a, \
    uint32_t: __builtin_add_overflow_32, \
    uint64_t: __builtin_add_overflow_64)(a, b, dst)



#elif defined(_MSC_VER) && !defined(__clang__)
// Shim the overflow intrinsics for MSVC
// These are slow as they use division.
static inline
int
__builtin_mul_overflow_32(uint32_t a, uint32_t b, uint32_t* dst){
    uint64_t res = (uint64_t)a * (uint64_t)b;
    if(res > (uint64_t)UINT32_MAX) return 1;
    *dst = res;
    return 0;
}
static inline
int
__builtin_mul_overflow_64(uint64_t a, uint64_t b, uint64_t* dst){
    if(a && b > UINT64_MAX / a) return 1;
    *dst = a * b;
    return 0;
}
#define __builtin_mul_overflow(a, b, dst) _Generic(a, \
    uint32_t: __builtin_mul_overflow_32, \
    uint64_t: __builtin_mul_overflow_64)(a, b, dst)

static inline
int
__builtin_add_overflow_32(uint32_t a, uint32_t b, uint32_t* dst){
    return _addcarry_u32(0, a, b, dst);
}
static inline
int
__builtin_add_overflow_64(uint64_t a, uint64_t b, uint64_t* dst){
    return _addcarry_u64(0, a, b, dst);
}
#define __builtin_add_overflow(a, b, dst) _Generic(a, \
    uint32_t: __builtin_add_overflow_32, \
    uint64_t: __builtin_add_overflow_64)(a, b, dst)

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
enum ParseNumberError {
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

//
// Result structs for the various parsing functions.
//
// Check the `errored` field first. If it is non-zero, `result` is
// indeterminate.
// If it is zero, then the result field is valid and is the parsed number.
//

typedef struct Uint64Result {
    uint64_t result;
    enum ParseNumberError errored;
} Uint64Result;

typedef struct Int64Result {
    int64_t result;
    enum ParseNumberError errored;
} Int64Result;

typedef struct Uint32Result {
    uint32_t result;
    enum ParseNumberError errored;
} Uint32Result;

typedef struct Int32Result {
    int32_t result;
    enum ParseNumberError errored;
} Int32Result;

typedef struct IntResult {
    int result;
    enum ParseNumberError errored;
} IntResult;

#if PARSE_NUMBER_PARSE_FLOATS
typedef struct FloatResult {
    float result;
    enum ParseNumberError errored;
} FloatResult;

typedef struct DoubleResult {
    double result;
    enum ParseNumberError errored;
} DoubleResult;
#endif

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
struct Uint64Result
parse_uint64(const char* str, size_t length);

//
// Parses a decimal int64.
static inline
warn_unused
struct Int64Result
parse_int64(const char* str, size_t length);

//
// Parses a decimal uint32.
static inline
warn_unused
struct Uint32Result
parse_uint32(const char*str, size_t length);


//
// Parses a decimal int32.
static inline
warn_unused
struct Int32Result
parse_int32(const char*str, size_t length);

//
// Parses a decimal int.
static inline
warn_unused
struct IntResult
parse_int(const char* str, size_t length);

//
// Parses hex format, but with a leading '#' instead of '0x'.
static inline
warn_unused
struct Uint64Result
parse_pound_hex(const char* str, size_t length);

//
// Parses traditional hex format, such as '0xf00dface'
static inline
warn_unused
struct Uint64Result
parse_hex(const char* str, size_t length);

//
// Parses binary notation, such as '0b1101'.
static inline
warn_unused
struct Uint64Result
parse_binary(const char* str, size_t length);

//
// Parses octal notation, such as '0o755' or '0O755'.
static inline
warn_unused
struct Uint64Result
parse_octal(const char* str, size_t length);

//
// Parses an unsigned integer, in whatever format is comfortable for a human.
// Accepts 0x hexes, 0b binary, plain decimals, and also # hexes.
static inline
warn_unused
#ifdef __wasm__
struct Uint32Result
#else
struct Uint64Result
#endif
parse_unsigned_human(const char* str, size_t length);

//
// Parses a float, in regular or scientific form.
static inline
warn_unused
struct FloatResult
parse_float(const char* str, size_t length);

//
// Parses a double, in regular or scientific form.
static inline
warn_unused
struct DoubleResult
parse_double(const char* str, size_t length);


// Implementations after this point.

static inline
warn_unused
struct Uint64Result
parse_uint64(const char* str, size_t length){
    struct Uint64Result result = {0};
    if(!length){
        result.errored = PARSENUMBER_UNEXPECTED_END;
        return result;
    }
    if(*str == '+'){
        str++;
        length--;
    }
    if(!length){
        result.errored = PARSENUMBER_UNEXPECTED_END;
        return result;
    }
    // UINT64_MAX is 18,446,744,073,709,551,615 (20 characters)
    if(length > 20){
        result.errored = PARSENUMBER_OVERFLOWED_VALUE;
        return result;
    }
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
    if(bad){
        result.errored = PARSENUMBER_INVALID_CHARACTER;
        return result;
    }
    // Handle the last char differently as it's the only
    // one that can overflow.
    {
        unsigned cval = (unsigned char)str[length-1];
        cval -= '0';
        if(cval > 9u){
            result.errored = PARSENUMBER_INVALID_CHARACTER;
            return result;
        }
        if(__builtin_mul_overflow(value, 10, &value)){
            result.errored = PARSENUMBER_OVERFLOWED_VALUE;
            return result;
        }
        if(__builtin_add_overflow(value, cval, &value)){
            result.errored = PARSENUMBER_OVERFLOWED_VALUE;
            return result;
        }

    }
    result.result = value;
    return result;
}

static inline
warn_unused
struct Int64Result
parse_int64(const char* str, size_t length){
    struct Int64Result result = {0};
    if(!length){
        result.errored = PARSENUMBER_UNEXPECTED_END;
        return result;
    }
    _Bool negative = (*str == '-');
    if(negative){
        str++;
        length--;
    }
    else if(*str == '+'){
        str++;
        length--;
    }
    if(!length){
        result.errored = PARSENUMBER_UNEXPECTED_END;
        return result;
    }
    // INT64_MAX is 9223372036854775807 (19 characters)
    if(length > 19){
        result.errored = PARSENUMBER_OVERFLOWED_VALUE;
        return result;
    }
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
    if(bad){
        result.errored = PARSENUMBER_INVALID_CHARACTER;
        return result;
    }
    // Handle the last char differently as it's the only
    // one that can overflow.
    {
        unsigned cval = (unsigned char)str[length-1];
        cval -= '0';
        if(cval > 9u){
            result.errored = PARSENUMBER_INVALID_CHARACTER;
            return result;
        }
        if(__builtin_mul_overflow(value, 10, &value)){
            result.errored = PARSENUMBER_OVERFLOWED_VALUE;
            return result;
        }
        if(__builtin_add_overflow(value, cval, &value)){
            result.errored = PARSENUMBER_OVERFLOWED_VALUE;
            return result;
        }
    }
    if(negative){
        if(value > (uint64_t)INT64_MAX+1){
            result.errored = PARSENUMBER_OVERFLOWED_VALUE;
            return result;
        }
        result.result = (int64_t)-value;
    }
    else{
        if(value > (uint64_t)INT64_MAX){
            result.errored = PARSENUMBER_OVERFLOWED_VALUE;
            return result;
        }
        result.result = (int64_t)value;
    }
    return result;
}

static inline
warn_unused
struct Uint32Result
parse_uint32(const char*str, size_t length){
    struct Uint32Result result = {0};
    if(!length){
        result.errored = PARSENUMBER_UNEXPECTED_END;
        return result;
    }
    if(*str == '+'){
        str++;
        length--;
    }
    if(!length){
        result.errored = PARSENUMBER_UNEXPECTED_END;
        return result;
    }
    // UINT32_MAX is 10 characters
    if(length > 10){
        result.errored = PARSENUMBER_OVERFLOWED_VALUE;
        return result;
    }
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
    if(bad){
        result.errored = PARSENUMBER_INVALID_CHARACTER;
        return result;
    }
    // Handle the last char differently as it's the only
    // one that can overflow.
    {
        unsigned cval = (unsigned char)str[length-1];
        cval -= '0';
        if(cval > 9u){
            result.errored = PARSENUMBER_INVALID_CHARACTER;
            return result;
        }
        if(__builtin_mul_overflow(value, 10, &value)){
            result.errored = PARSENUMBER_OVERFLOWED_VALUE;
            return result;
        }
        if(__builtin_add_overflow(value, cval, &value)){
            result.errored = PARSENUMBER_OVERFLOWED_VALUE;
            return result;
        }
    }
    result.result = value;
    return result;
}

static inline
warn_unused
struct Int32Result
parse_int32(const char*str, size_t length){
    struct Int32Result result = {0};
    if(!length){
        result.errored = PARSENUMBER_UNEXPECTED_END;
        return result;
    }
    _Bool negative = (*str == '-');
    if(negative){
        str++;
        length--;
    }
    else if (*str == '+'){
        str++;
        length--;
    }
    if(!length){
        result.errored = PARSENUMBER_UNEXPECTED_END;
        return result;
    }
    // INT32_max is 10 chars
    if(length > 10){
        result.errored = PARSENUMBER_OVERFLOWED_VALUE;
        return result;
    }
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
    if(bad){
        result.errored = PARSENUMBER_INVALID_CHARACTER;
        return result;
    }
    // Handle the last char differently as it's the only
    // one that can overflow.
    {
        unsigned cval = (unsigned char)str[length-1];
        cval -= '0';
        if(cval > 9u){
            result.errored = PARSENUMBER_INVALID_CHARACTER;
            return result;
        }
        if(__builtin_mul_overflow(value, 10, &value)){
            result.errored = PARSENUMBER_OVERFLOWED_VALUE;
            return result;
        }
        if(__builtin_add_overflow(value, cval, &value)){
            result.errored = PARSENUMBER_OVERFLOWED_VALUE;
            return result;
        }
    }
    if(negative){
        if(value > (uint32_t)INT32_MAX+1){
            result.errored = PARSENUMBER_OVERFLOWED_VALUE;
            return result;
        }
        result.result = (int32_t)-value;
    }
    else{
        if(value > (uint32_t)INT32_MAX){
            result.errored = PARSENUMBER_OVERFLOWED_VALUE;
            return result;
        }
        result.result = (int32_t)value;
    }
    return result;
}

static inline
warn_unused
struct IntResult
parse_int(const char* str, size_t length){
    struct IntResult result;
    struct Int32Result e = parse_int32(str, length);
    result.errored = e.errored;
    result.result = e.result;
    return result;
}

static inline
warn_unused
struct Uint64Result
parse_hex_inner(const char* str, size_t length){
    struct Uint64Result result = {0};
    if(length > sizeof(result.result)*2){
        result.errored = PARSENUMBER_OVERFLOWED_VALUE;
        return result;
    }
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
                result.errored = PARSENUMBER_INVALID_CHARACTER;
                return result;
        }
        value <<= 4;
        value |= char_value;
    }
    result.result = value;
    return result;
}

static inline
warn_unused
struct Uint64Result
parse_pound_hex(const char* str, size_t length){
    struct Uint64Result result = {0};
    if(length < 2){
        result.errored = PARSENUMBER_UNEXPECTED_END;
        return result;
    }
    if(str[0] != '#'){
        result.errored = PARSENUMBER_INVALID_CHARACTER;
        return result;
    }
    return parse_hex_inner(str+1, length-1);
}

static inline
warn_unused
struct Uint64Result
parse_hex(const char* str, size_t length){
    struct Uint64Result result = {0};
    if(length<3){
        result.errored = PARSENUMBER_UNEXPECTED_END;
        return result;
    }
    if(str[0] != '0'){
        result.errored = PARSENUMBER_INVALID_CHARACTER;
        return result;
    }
    if(str[1] != 'x' && str[1] != 'X'){
        result.errored = PARSENUMBER_INVALID_CHARACTER;
        return result;
    }
    return parse_hex_inner(str+2, length-2);
}

static inline warn_unused struct Uint64Result parse_binary_inner(const char*, size_t);
static inline warn_unused struct Uint64Result parse_octal_inner(const char*, size_t);

static inline
warn_unused
struct Uint64Result
parse_binary(const char* str, size_t length){
    struct Uint64Result result = {0};
    if(length<3){
        result.errored = PARSENUMBER_UNEXPECTED_END;
        return result;
    }
    if(str[0] != '0'){
        result.errored = PARSENUMBER_INVALID_CHARACTER;
        return result;
    }
    if(str[1] != 'b' && str[1] != 'B'){
        result.errored = PARSENUMBER_INVALID_CHARACTER;
        return result;
    }
    return parse_binary_inner(str+2, length-2);
}

static inline
warn_unused
struct Uint64Result
parse_binary_inner(const char* str, size_t length){
    struct Uint64Result result = {0};
    unsigned long long mask = 1llu << 63;
    mask >>= (64 - length);
    // @speed
    // 2**4 is only 16, so we could definitely
    // read 4 bytes at a time and then do a fixup.
    // You'd have to see what code is generated though
    // (does the compiler turn it into a binary decision tree?)
    // 2**8 is only 256, that's probably not worth it.
    for(size_t i = 0; i < length; i++, mask>>=1){
        switch(str[i]){
            case '1':
                result.result |= mask;
                continue;
            case '0':
                continue;
            default:
                result.errored = PARSENUMBER_INVALID_CHARACTER;
                return result;
        }
    }
    return result;
}


static inline
warn_unused
struct Uint64Result
parse_octal(const char* str, size_t length){
    struct Uint64Result result = {0};
    if(length < 3){
        result.errored = PARSENUMBER_UNEXPECTED_END;
        return result;
    }
    if(str[0] != '0'){
        result.errored = PARSENUMBER_INVALID_CHARACTER;
        return result;
    }
    if(str[1] != 'o' && str[1] != 'O'){
        result.errored = PARSENUMBER_INVALID_CHARACTER;
        return result;
    }
    return parse_octal_inner(str+2, length-2);
}

static inline
warn_unused
struct Uint64Result
parse_octal_inner(const char* str, size_t length){
    struct Uint64Result result = {0};
    if(!length){
        result.errored = PARSENUMBER_UNEXPECTED_END;
        return result;
    }
    // UINT64_MAX is 1777777777777777777777 in octal (22 digits)
    if(length > 22){
        result.errored = PARSENUMBER_OVERFLOWED_VALUE;
        return result;
    }
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
    if(bad){
        result.errored = PARSENUMBER_INVALID_CHARACTER;
        return result;
    }
    // Handle the last digit with overflow checking.
    {
        unsigned cval = (unsigned char)str[length-1];
        cval -= '0';
        if(cval > 7u){
            result.errored = PARSENUMBER_INVALID_CHARACTER;
            return result;
        }
        if(__builtin_mul_overflow(value, (uint64_t)8, &value)){
            result.errored = PARSENUMBER_OVERFLOWED_VALUE;
            return result;
        }
        if(__builtin_add_overflow(value, (uint64_t)cval, &value)){
            result.errored = PARSENUMBER_OVERFLOWED_VALUE;
            return result;
        }
    }
    result.result = value;
    return result;
}

static inline
warn_unused
#ifdef __wasm__
// XXX bad codegen with the uint64 version
struct Uint32Result
#else
struct Uint64Result
#endif
parse_unsigned_human(const char* str, size_t length){
    #ifndef __wasm__
    struct Uint64Result result = {0};
    if(!length){
        result.errored = PARSENUMBER_UNEXPECTED_END;
        return result;
    }
    if(str[0] == '#')
        return parse_pound_hex(str, length);
    if(str[0] == '0' && length > 1){
        if(str[1] == 'x' || str[1] == 'X')
            return parse_hex(str, length);
        if(str[1] == 'b' || str[1] == 'B')
            return parse_binary(str, length);
    }
    return parse_uint64(str, length);
    #else
    return parse_uint32(str, length);
    #endif
}

#if PARSE_NUMBER_PARSE_FLOATS
static inline
warn_unused
struct FloatResult
parse_float(const char* str, size_t length){
    struct FloatResult result = {0};
    const char* end = str + length;
    // fast_float doesn't accept leading '+', but we want to.
    while(str != end && *str == '+')
        str++;
    if(str == end){
        result.errored = PARSENUMBER_UNEXPECTED_END;
        return result;
    }
    fast_float_from_chars_result fr = fast_float_from_chars_float(str, end, &result.result, FASTFLOAT_FORMAT_GENERAL);
    if(fr.ptr != end){
        result.errored = PARSENUMBER_INVALID_CHARACTER;
        return result;
    }
    switch(fr.error){
        case FASTFLOAT_NO_ERROR:
            return result;
        case FASTFLOAT_INVALID_VALUE:
            result.errored = PARSENUMBER_INVALID_CHARACTER;
            return result;
        default:
        case FASTFLOAT_BAD_FORMAT:
            // This should never happen, but meh.
            result.errored = PARSENUMBER_INVALID_CHARACTER;
            return result;
    }
}

//
// Parses a double, in regular or scientific form.
static inline
warn_unused
struct DoubleResult
parse_double(const char* str, size_t length){
    struct DoubleResult result = {0};
    const char* end = str + length;
    // fast_float doesn't accept leading '+', but we want to.
    while(str != end && *str == '+')
        str++;
    if(str == end){
        result.errored = PARSENUMBER_UNEXPECTED_END;
        return result;
    }
    fast_float_from_chars_result fr = fast_float_from_chars_double(str, end, &result.result, FASTFLOAT_FORMAT_GENERAL);
    if(fr.ptr != end){
        result.errored = PARSENUMBER_INVALID_CHARACTER;
        return result;
    }
    switch(fr.error){
        case FASTFLOAT_NO_ERROR:
            return result;
        case FASTFLOAT_INVALID_VALUE:
            result.errored = PARSENUMBER_INVALID_CHARACTER;
            return result;
        default:
        case FASTFLOAT_BAD_FORMAT:
            // This should never happen, but meh.
            result.errored = PARSENUMBER_INVALID_CHARACTER;
            return result;
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
