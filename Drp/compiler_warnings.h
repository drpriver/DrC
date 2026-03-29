//
// Copyright © 2024-2025, David Priver <david@davidpriver.com>
//
#ifndef COMPILER_WARNINGS_H
#define COMPILER_WARNINGS_H
#ifdef _WIN32
#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif
#endif

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic error "-Wall"
#pragma GCC diagnostic error "-Wextra"
#pragma GCC diagnostic error "-Wvla"
#pragma GCC diagnostic error "-Wmissing-noreturn"
#pragma GCC diagnostic error "-Wcast-qual"
#pragma GCC diagnostic error "-Wdeprecated"
#pragma GCC diagnostic error "-Wdouble-promotion"
#pragma GCC diagnostic error "-Wint-conversion"
#pragma GCC diagnostic error "-Wimplicit-int"
#pragma GCC diagnostic error "-Wimplicit-function-declaration"
#pragma GCC diagnostic error "-Wincompatible-pointer-types"
#pragma GCC diagnostic error "-Wunused-result"
#pragma GCC diagnostic error "-Wswitch"
#pragma GCC diagnostic error "-Wsizeof-pointer-memaccess"
#ifndef  __MINGW32__
#pragma GCC diagnostic error "-Wformat"
#else
#pragma GCC diagnostic ignored "-Wformat"
#pragma GCC diagnostic ignored "-Wformat-extra-args"
#endif
#pragma GCC diagnostic error "-Wreturn-type"
#pragma GCC diagnostic ignored "-Woverlength-strings"
#pragma GCC diagnostic ignored "-Wunused-function"
#endif

#if defined(__clang__)
#pragma GCC diagnostic error "-Wpedantic"
#pragma clang diagnostic error "-Wgnu-label-as-value"
#pragma clang diagnostic ignored "-Wnullability-extension"
#if __has_warning("-Wfixed-enum-extension")
#pragma clang diagnostic ignored "-Wfixed-enum-extension"
#endif
#if __has_warning("-Wc23-extensions")
#pragma clang diagnostic ignored "-Wc23-extensions"
#endif
#pragma clang diagnostic ignored "-Wgnu-zero-variadic-macro-arguments"
#pragma clang diagnostic ignored "-Wlanguage-extension-token"
#pragma clang diagnostic ignored "-Wgnu-auto-type"
#pragma clang diagnostic ignored "-Wextra-semi"
#pragma clang diagnostic error "-Wassign-enum"
#pragma clang diagnostic error "-Wshadow"
#pragma clang diagnostic error "-Warray-bounds-pointer-arithmetic"
#pragma clang diagnostic error "-Wcovered-switch-default"
#pragma clang diagnostic error "-Wfor-loop-analysis"
#pragma clang diagnostic error "-Winfinite-recursion"
#pragma clang diagnostic error "-Wduplicate-enum"
#pragma clang diagnostic ignored "-Wmissing-field-initializers"
#pragma clang diagnostic error "-Wpointer-type-mismatch"
#pragma clang diagnostic error "-Wextra-tokens"
#pragma clang diagnostic error "-Wmacro-redefined"
#pragma clang diagnostic error "-Winitializer-overrides"
#pragma clang diagnostic error "-Wsometimes-uninitialized"
#pragma clang diagnostic error "-Wunused-comparison"
#pragma clang diagnostic error "-Wundefined-internal"
#pragma clang diagnostic error "-Wnon-literal-null-conversion"
#pragma clang diagnostic error "-Wnullable-to-nonnull-conversion"
#pragma clang diagnostic error "-Wnullability-completeness"
#pragma clang diagnostic error "-Wnullability"
#pragma clang diagnostic error "-Wuninitialized"
#pragma clang diagnostic error "-Wconditional-uninitialized"
#pragma clang diagnostic error "-Wcomma"
#pragma clang diagnostic ignored "-Wmicrosoft-fixed-enum"
#pragma clang diagnostic error "-Wimplicit-int-conversion"
#pragma clang diagnostic error "-Wimplicit-fallthrough"
#if __clang__major__ >= 11
#pragma clang diagnostic error "-Wexcess-initializers"
#endif
#pragma clang diagnostic error "-Wbitfield-constant-conversion"
#pragma clang diagnostic ignored "-Wmultichar"
#endif

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic ignored "-Wmissing-braces"
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#pragma GCC diagnostic ignored "-Wsuggest-attribute=noreturn"
#pragma GCC diagnostic error "-Wswitch-enum"
#endif

#if defined(_MSC_VER) && !defined(__clang__)
#pragma warning( error : 4013 ) // implicit function declaration
#pragma warning( error : 4020 ) // too many actual parameters
#pragma warning( error : 4024 ) // different types for formal and actual parameter
#pragma warning( error : 4028 ) // formal parameter different from declaration
#pragma warning( error : 4029 ) // declared formal parameter list different from definition
#pragma warning( error : 4047 ) // differs in levels of indirection
#pragma warning( error : 4133 ) // incompatible types
#pragma warning( error : 4098 ) // void function returning a value
#pragma warning( error : 4715 ) // not all control paths return a value
#pragma warning( error : 4700 ) // use before assign
#pragma warning( error : 4113 ) // wrong function pointer type
#pragma warning( error : 4033 ) // missing first param
#pragma warning( disable : 4244 )
#pragma warning( disable : 4146 )
#pragma warning( disable : 4267 )
#endif

#ifdef __wasm__
#pragma clang diagnostic ignored "-Wshorten-64-to-32"
#endif


#endif
