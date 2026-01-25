//
// Copyright © 2024-2025, David Priver <david@davidpriver.com>
//
//
// NOTE: you need to include this before any std header as many
// either define these macros or include windows.h at some point.
#ifndef WINDOWSHEADER_H
#define WINDOWSHEADER_H
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define WIN32_EXTRA_LEAN
#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif
// Idk why, but this conflicts with builtin
// so hacky #define
// #define _mm_prefetch _WINDOWS_MM_PREFETCH

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wnonportable-include-path"
#pragma clang diagnostic ignored "-Wmicrosoft-anon-tag"
#pragma clang diagnostic ignored "-Wpragma-pack"
#pragma clang diagnostic ignored "-Wignored-pragma-intrinsic"
#pragma clang diagnostic ignored "-Wignored-attributes"
#pragma clang diagnostic ignored "-Wgnu-folding-constant"
#pragma clang diagnostic ignored "-Wexpansion-to-defined"
#include <intrin.h>
#endif
#ifdef _MSC_VER
#pragma warning(disable:5105)   // macro producing defined is UB lmao
#endif
#ifdef __MINGW32__
#include <windows.h>
#else
#include <Windows.h>
#endif
#ifdef __clang__
#pragma clang diagnostic pop
#endif
// #undef _mm_prefetch
#undef ERROR
enum {IS_WINDOWS=1};
#else
enum {IS_WINDOWS=0};
#endif

#endif
