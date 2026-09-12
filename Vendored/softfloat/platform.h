#ifndef SOFTFLOAT_PLATFORM_H
#define SOFTFLOAT_PLATFORM_H

#define LITTLEENDIAN 1
#define SOFTFLOAT_FAST_INT64 1
#define SOFTFLOAT_ROUND_ODD 1
#define INLINE_LEVEL 5
#define SOFTFLOAT_FAST_DIV32TO16 1
#define SOFTFLOAT_FAST_DIV64TO32 1
#define INLINE static inline
#if defined _MSC_VER && !defined __clang__
#define THREAD_LOCAL __declspec(thread)
#else
#define THREAD_LOCAL _Thread_local
#endif

#if defined __GNUC__ || defined __clang__
#define SOFTFLOAT_BUILTIN_CLZ 1
#ifdef __SIZEOF_INT128__
#define SOFTFLOAT_INTRINSIC_INT128 1
#endif
#include "SoftFloat-3e/source/include/opts-GCC.h"
#elif defined _MSC_VER && defined _M_X64
#include "opts-MSVC.h"
#endif

#endif
