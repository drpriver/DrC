//
// Copyright © 2024-2025, David Priver <david@davidpriver.com>
//
#ifndef ATOMF_H
#define ATOMF_H
#include <stdarg.h>
#include "atom_table.h"
#include "../Vendored/stb/stb_sprintf.h"

#ifdef __clang__
#pragma clang assume_nonnull begin
#endif
static inline
Atom _Nullable
#ifdef __GNUC__
__attribute__((format(printf, 2, 3 )))
#endif
AT_atomize_f(AtomTable* at, const char* fmt, ...);

static inline
Atom _Nullable
AT_atomize_fv(AtomTable* at, const char* fmt, va_list vap);

static inline
Atom _Nullable
#ifdef __GNUC__
__attribute__((format(printf, 2, 3 )))
#endif
AT_atomize_f(AtomTable* at, const char* fmt, ...){
    va_list vap;
    va_start(vap, fmt);
    Atom a = AT_atomize_fv(at, fmt, vap);
    va_end(vap);
    return a;
}

static inline
Atom _Nullable
AT_atomize_fv(AtomTable* at, const char* fmt, va_list vap){
    char buff[2048];
    int n = stbsp_vsnprintf(buff, sizeof buff, fmt, vap);
    if(n > (int)sizeof buff)
        n = -1+(int)sizeof buff;
    Atom a = AT_atomize(at, buff, n);
    return a;
}
#ifdef __clang__
#pragma clang assume_nonnull end
#endif

#endif
