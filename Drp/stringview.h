//
// Copyright © 2024-2025, David Priver <david@davidpriver.com>
//
#ifndef STRINGVIEW_H
#define STRINGVIEW_H
#include <stddef.h>
#include <string.h>
#include <stdint.h>
#ifdef __clang__
#pragma clang assume_nonnull begin
#else
#ifndef _Nullable
#define _Nullable
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

typedef struct StringView StringView;
struct StringView {
    size_t length;
    const char* text;
};

#define SV(x) ((StringView){sizeof(x)-1, x})
#define SVI(x) {sizeof(x)-1, x}
#define sv_p(x) (int)(x).length, (x).text

force_inline
_Bool
sv_equals(StringView a, StringView b){
    if(a.length != b.length) return 0;
    if(!a.length || a.text==b.text) return 1;
    return memcmp(a.text, b.text, a.length) == 0;
}

force_inline
_Bool
sv_equals2(StringView a, const char* txt, size_t len){
    if(a.length != len) return 0;
    if(!a.length || a.text == txt) return 1;
    return memcmp(a.text, txt, len) == 0;
}

static inline
uint64_t
_sv_ascii_fold8(uint64_t x) {
    #define HI UINT64_C(0x8080808080808080)
    #define ONES UINT64_C(0x0101010101010101)
    uint64_t ge_A  = ((x | HI) - ONES * 'A')       & HI;
    uint64_t ge_Z1 = ((x | HI) - ONES * ('Z' + 1)) & HI;
    uint64_t upper = ge_A & ~ge_Z1 & ~x & HI;
    #undef HI
    #undef ONES
    return x | (upper >> 2);
}

static inline
_Bool
sv_iequals(StringView a, StringView b){
    if(a.length != b.length) return 0;
    if(!a.length || a.text == b.text) return 1;
    size_t length = a.length;
    const uint8_t *ap = (const uint8_t *)a.text;
    const uint8_t *bp = (const uint8_t *)b.text;
    _Static_assert(sizeof(uint64_t)==8, "");
    while(length >= 8){
        uint64_t aw, bw;
        memcpy(&aw, ap, 8);
        memcpy(&bw, bp, 8);
        if(_sv_ascii_fold8(aw) != _sv_ascii_fold8(bw))
            return 0;
        ap += 8;
        bp += 8;
        length -= 8;
    }
    while(length--){
        uint8_t l = *ap++;
        uint8_t r = *bp++;
        if(l >= 'A' && l <= 'Z')
            l |= 0x20;
        if(r >= 'A' && r <= 'Z')
            r |= 0x20;
        if(l != r)
            return 0;
    }
    return 1;
}

static inline
_Bool
sv_iequals2(StringView a, const char* txt, size_t len){
    return sv_iequals(a, (StringView){len, txt});
}

static inline
_Bool
sv_endswith(StringView s, StringView tail){
    if(s.length < tail.length) return 0;
    return sv_equals2(tail, s.text+s.length-tail.length, tail.length);
}
static inline
_Bool
sv_iendswith(StringView s, StringView tail){
    if(s.length < tail.length) return 0;
    return sv_iequals2(tail, s.text+s.length-tail.length, tail.length);
}
static inline
_Bool
sv_startswith(StringView s, StringView head){
    if(s.length < head.length) return 0;
    return sv_equals2(head, s.text, head.length);
}
static inline
_Bool
sv_istartswith(StringView s, StringView head){
    if(s.length < head.length) return 0;
    return sv_iequals2(head, s.text, head.length);
}

static inline
const char*_Nullable
sv_sv(StringView haystack, StringView needle){
    if(!needle.length) return NULL;
    if(needle.length > haystack.length) return NULL;
    #if !defined(_WIN32) && !defined(__wasm__)
        extern void* memmem(const void*, size_t, const void*, size_t);
        return memmem(haystack.text, haystack.length, needle.text, needle.length);
    #endif
    for(;;){
        const char* p = memchr(haystack.text, needle.text[0], haystack.length);
        if(!p) return NULL;
        size_t plen = haystack.text + haystack.length - p;
        if(plen < needle.length) return NULL;
        if(memcmp(p, needle.text, needle.length) == 0) return p;
        haystack = (StringView){plen-1, p+1};
        if(haystack.length < needle.length) return NULL;
    }
}

static inline
_Bool
sv_contains(StringView haystack, StringView needle){
    return sv_sv(haystack, needle) != NULL;
}

static inline
StringView
sv_slice(StringView s, size_t lo, size_t hi){
    // if(hi > s.length) __builtin_debugtrap();
    // if(lo > s.length) __builtin_debugtrap();
    return (StringView){hi-lo, s.text+lo};
}

force_inline
void
lstrip(StringView* sv){
    while(sv->length){
        switch(sv->text[0]){
            case ' ':
            case '\n':
            case '\t':
                sv->length--;
                sv->text++;
                continue;
            default:
                return;
        }
    }
}

force_inline
void
rstrip(StringView* sv){
    while(sv->length){
        switch(sv->text[sv->length-1]){
            case ' ':
            case '\n':
            case '\t':
                sv->length--;
                continue;
            default:
                return;
        }
    }
}

force_inline
void
lstripc(StringView* sv){
    while(sv->length){
        switch(sv->text[0]){
            case ' ':
            case '\n':
            case '\t':
            case ',':
                sv->length--;
                sv->text++;
                continue;
            default:
                return;
        }
    }
}

force_inline
StringView
stripped(StringView sv){
    lstrip(&sv);
    rstrip(&sv);
    return sv;
}

force_inline
StringView
stripped2(const char* txt, size_t len){
    StringView sv = {len, txt};
    lstrip(&sv);
    rstrip(&sv);
    return sv;
}

static inline
_Bool
sv_split1(StringView sv, char c, StringView* head, StringView* tail){
    const char* p = memchr(sv.text, c, sv.length);
    if(!p){
        *head = sv;
        *tail = (StringView){0};
        return 0;
    }
    *head = (StringView){
        p - sv.text,
        sv.text,
    };
    *tail = (StringView){
        sv.text + sv.length - p -1,
        p+1,
    };
    return 1;
}

static inline
_Bool
sv_split(StringView sv, StringView splitter, StringView* head, StringView* tail){
    const char* p = sv_sv(sv, splitter);
    if(!p){
        *head = sv;
        *tail = (StringView){0};
        return 0;
    }
    *head = (StringView){
        p - sv.text,
        sv.text,
    };
    *tail = (StringView){
        sv.text + sv.length - p - splitter.length,
        p+splitter.length,
    };
    return 1;
}


#ifdef __clang__
#pragma clang assume_nonnull end
#endif
#endif
