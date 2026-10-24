#ifndef MSB_SPRINTF_H
#define MSB_SPRINTF_H
#include "../Vendored/stb/stb_sprintf.h"
#include <stdarg.h>
#include "MStringBuilder.h"

static inline
void
msb_vsprintf(MStringBuilder*, const char*, va_list);
static inline
void
#ifdef __GNUC__
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
    if(sb->errored) return;
    va_list copy;
    va_copy(copy, vap);
    ptrdiff_t n = stbsp_vsnprintf(NULL, 0, fmt, copy);
    va_end(copy);
    if(n >= 0){
        int err = msb_ensure_additional(sb, (size_t)n + 1);
        if(err) return;
        char* data = sb->data + sb->cursor;
        ptrdiff_t n2 = stbsp_vsnprintf(data, sb->capacity-sb->cursor, fmt, vap);
        if(n2 < 0 || (size_t)n2 >= sb->capacity-sb->cursor){
            sb->errored = 1;
            return;
        }
        sb->cursor += (size_t)n2;
    }
    else{
        sb->errored = 1;
    }
}
#endif
