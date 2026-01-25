#ifndef MSB_SPRINTF_H
#define MSB_SPRINTF_H
#include "../Vendored/stb/stb_sprintf.h"
#include <stdarg.h>
#include "MStringBuilder.h"

#ifndef __builtin_debugtrap
#if defined(__GNUC__) && ! defined(__clang__)
#define __builtin_debugtrap() __builtin_trap()
#elif defined(_MSC_VER)
#define __builtin_debugtrap() __debugbreak()
#endif
#endif

static inline
void
msb_vsprintf(MStringBuilder*, const char*, va_list);
static inline
void
#if defined(__GNUC__)
__attribute__((format(printf, 2, 3)))
#endif
msb_sprintf(MStringBuilder* sb, const char* fmt, ...){
    va_list vap;
    va_start(vap, fmt);
    msb_vsprintf(sb, fmt, vap);
    va_end(vap);
}

static inline
void
msb_vsprintf(MStringBuilder* sb, const char* fmt, va_list vap){
    va_list copy;
    va_copy(copy, vap);
    int n = stbsp_vsnprintf(NULL, 0, fmt, copy);
    va_end(copy);
    if(n >= 0){
        int err = msb_ensure_additional(sb, n+1);
        if(err) return;
        char* data = sb->data + sb->cursor;
        int n2 = stbsp_vsnprintf(data, (int)(sb->capacity-sb->cursor), fmt, vap);
        sb->cursor += n2;
    }
    else{
        sb->errored = 1;
    }
}
#endif
