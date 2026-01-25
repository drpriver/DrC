#ifndef DRP_OFFSETOF_H
#define DRP_OFFSETOF_H
#include <stddef.h>
#include <stdint.h>

#if defined(_MSC_VER) && !defined(__clang__)
// apparently msvc offsetof is buggy??
#undef offsetof
#define offsetof(S, m) (size_t)((const char*)&((S*)0)->m - (const char*)0)
#endif

#if defined(_WIN32) && (defined(__clang__) || defined(__GNUC__))
#undef offsetof
#define offsetof __builtin_offsetof
#endif

#ifndef parentof
#define parentof(p, S, m) ((S*)((uintptr_t)(p) - offsetof(S, m)))
#endif


#endif
