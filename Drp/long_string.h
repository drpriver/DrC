//
// Copyright © 2021-2025, David Priver <david@davidpriver.com>
//
#ifndef LONG_STRING_H
#define LONG_STRING_H
// size_t
#include <stddef.h>
// strlen, memcmp
#include <string.h>
// uint16_t
#include <stdint.h>

#include "stringview.h"

#ifdef __clang__
#pragma clang assume_nonnull begin
#else
#ifndef _Null_unspecified
#define _Null_unspecified
#endif
#endif

#ifndef force_inline
#if defined(__GNUC__) || defined(__clang__)
#define force_inline static inline __attribute__((always_inline))
#elif defined(_MSC_VER)
#define force_inline static inline __forceinline
#else
#define force_inline static inline
#endif
#endif

// It is very likely you want to put the LongString and/or StringView
// into your public API, but this header contains lots of convenience functions
// that depend on the macros header and particular coding style that would
// be inappropriate for a public header.
//
// Setting this macro means there is already a typedef for LongString,
// StringView and StringViewUtf16, which allows you to expose the structs
// without the rest of this file.

#ifndef LONGSTRING_DEFINED

typedef struct LongString LongString;
struct LongString {
    size_t length; // excludes the terminating NUL
    const char*_Null_unspecified text; // utf-8 encoded text
};

_Static_assert(sizeof(unsigned short) == 2, "unsigned short is not uint16_t");
typedef struct StringViewUtf16 StringViewUtf16;
struct StringViewUtf16 {
    size_t length; // in code units
    // utf-16 encoded code points, native endianness
    const unsigned short*_Null_unspecified text;
};
typedef struct LongStringUtf16 LongStringUtf16;
struct LongStringUtf16 {
    size_t length; // in code units
    // utf-16 encoded code points, native endianness
    const unsigned short*_Null_unspecified text;
};

#endif

typedef struct StringView2 StringView2;
struct StringView2 {
    StringView key;
    StringView value;
};

force_inline
StringView
LS_to_SV(LongString ls){
    return (StringView){.length=ls.length, .text=ls.text};
}


static inline
_Bool
LS_equals(const LongString a, const LongString b){
    if (a.length != b.length)
        return 0;
    if(a.text == b.text)
        return 1;
    // assert(a.text);
    // assert(b.text);
    return a.text && b.text && !memcmp(a.text, b.text, a.length);
}

#ifdef LS
#error "LS defined"
#endif

#define LS(literal) ((LongString){.length=sizeof("" literal)-1, .text="" literal})
#define SV16(literal) ((StringViewUtf16){.length = sizeof(u"" literal)/2-1, .text=u"" literal})
// MSCV is garbage and doesn't like compound literals for static initializers
// So use this macro instead.
#define LSI(literal) {sizeof "" literal -1, "" literal}
#define SV16I(literal) {sizeof u"" literal /2-1, u"" literal}

static inline
_Bool
SV_utf16_equals(const StringViewUtf16 a, const StringViewUtf16 b){
    if(a.length != b.length)
        return 0;
    if(a.text == b.text)
        return 1;
    // assert(a.text);
    // assert(b.text);
    return a.text && b.text && memcmp(a.text, b.text, a.length*sizeof(uint16_t)) == 0;
}

static inline
_Bool
LS_SV_equals(const LongString ls, const StringView sv){
    if(ls.length != sv.length)
        return 0;
    if(ls.text == sv.text)
        return 1;
    // assert(ls.text);
    // assert(sv.text);
    return ls.text && sv.text && memcmp(ls.text, sv.text, sv.length)==0;
}

// Maybe it's UB (idk) but this works for LongStrings as well.
// Although maybe I should just use strcmp for those.
force_inline
int
StringView_cmp(const void* a, const void* b){
    // TODO: There's probably a cleaner way to implement this.
    const StringView* lhs = (const StringView*)a;
    const StringView* rhs = (const StringView*)b;
    size_t l1 = lhs->length;
    size_t l2 = rhs->length;
    if(l1 == l2){
        if(!l1)
            return 0;
        if(lhs->text == rhs->text)
            return 0;
        return memcmp(lhs->text, rhs->text, l1);
    }
    if(!lhs->length)
        return -(int)(unsigned char)rhs->text[0];
    if(!rhs->length)
        return (int)(unsigned char)lhs->text[0];
    int prefix_cmp = memcmp(lhs->text, rhs->text, lhs->length > rhs->length?rhs->length:lhs->length);
    if(prefix_cmp)
        return prefix_cmp;
    if(lhs->length > rhs->length){
        return (int)(unsigned char)lhs->text[rhs->length];
    }
    return -(int)(unsigned char)rhs->text[lhs->length];
}

#ifdef __clang__
#pragma clang assume_nonnull end
#endif

#endif
