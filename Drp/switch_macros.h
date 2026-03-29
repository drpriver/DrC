#ifndef DRP_SWITCH_MACROS_H
#define DRP_SWITCH_MACROS_H
//
// Copyright © 2026-2026, David Priver <david@davidpriver.com>
//

//
// For exhaustive switches, gcc and msvc think an enum can
// be any value of the underlying type while clang
// will assume it's a valid value.
// So those compilers need a hint that all cases are covered.
//
#ifndef CASES_EXHAUSTED
#if defined __clang__
#define CASES_EXHAUSTED
#elif defined __GNUC__
#define CASES_EXHAUSTED default: __builtin_unreachable()
#elif defined _MSC_VER
#define CASES_EXHAUSTED default: __assume(0)
#else
#define CASES_EXHAUSTED
#endif
#endif

//
// For when the default case is unreachable because only a subset
// of the switch values are possible.
//
#ifndef DEFAULT_UNREACHABLE
#if defined __GNUC__
#define DEFAULT_UNREACHABLE default: __builtin_unreachable()
#elif defined _MSC_VER
#define DEFAULT_UNREACHABLE default: __assume(0)
#else
#define DEFAULT_UNREACHABLE default: abort()
#endif
#endif

#endif
