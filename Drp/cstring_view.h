//
// Copyright © 2021-2026, David Priver <david@davidpriver.com>
//
#ifndef C_STRING_VIEW_H
#define C_STRING_VIEW_H
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

#ifndef CSTRINGVIEW_DEFINED

typedef struct CStringView CStringView;
struct CStringView {
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
typedef struct CStringViewUtf16 CStringViewUtf16;
struct CStringViewUtf16 {
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
CSV_to_SV(CStringView csv){
    return (StringView){.length=csv.length, .text=csv.text};
}


static inline
_Bool
CSV_equals(const CStringView a, const CStringView b){
    if (a.length != b.length) return 0;
    if(!a.length || a.text == b.text) return 1;
    return !memcmp(a.text, b.text, a.length);
}

#ifdef CSV
#error "CSV defined"
#endif

#define CSV(literal) ((CStringView){.length=sizeof("" literal)-1, .text="" literal})
#define SV16(literal) ((StringViewUtf16){.length = sizeof(u"" literal)/2-1, .text=u"" literal})
// MSCV is garbage and doesn't like compound literals for static initializers
// So use this macro instead.
#define CSVI(literal) {sizeof "" literal -1, "" literal}
#define SV16I(literal) {sizeof u"" literal /2-1, u"" literal}

static inline
_Bool
SV_utf16_equals(const StringViewUtf16 a, const StringViewUtf16 b){
    if(a.length != b.length) return 0;
    if(!a.length || a.text == b.text) return 1;
    return !memcmp(a.text, b.text, a.length*sizeof(uint16_t));
}

static inline
_Bool
CSV_SV_equals(const CStringView csv, const StringView sv){
    if(csv.length != sv.length) return 0;
    if(!csv.length || csv.text == sv.text) return 1;
    return !memcmp(csv.text, sv.text, sv.length);
}

static
int
StringView_cmp(const void* a, const void* b){
     const StringView *lhs = (const StringView*)a,
                      *rhs = (const StringView*)b;
    size_t len = lhs->length < rhs->length ? lhs->length : rhs->length;
    int cmp = len ? memcmp(lhs->text, rhs->text, len) : 0;
    if (cmp != 0) return cmp;
    return (lhs->length > rhs->length) - (lhs->length < rhs->length);
}

#ifdef __clang__
#pragma clang assume_nonnull end
#endif

#endif
