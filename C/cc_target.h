#ifndef C_CC_TARGET_H
#define C_CC_TARGET_H
//
// Copyright © 2026-2026, David Priver <david@davidpriver.com>
//
#include <stdint.h>
#include "cc_type.h"
#include "../Drp/typed_enum.h"
#include "../Drp/stringview.h"
#ifdef __clang__
#pragma clang assume_nonnull begin
#endif

enum CcTarget TYPED_ENUM(int) {
    CC_TARGET_X86_64_LINUX,
    CC_TARGET_AARCH64_LINUX,
    CC_TARGET_X86_64_MACOS,
    CC_TARGET_AARCH64_MACOS,
    CC_TARGET_X86_64_WINDOWS,
    CC_TARGET_COUNT,
};
TYPEDEF_ENUM(CcTarget, int);

static const StringView cc_target_names[CC_TARGET_COUNT] = {
    [CC_TARGET_X86_64_LINUX]   = SV("x86_64-linux"),
    [CC_TARGET_AARCH64_LINUX]  = SV("aarch64-linux"),
    [CC_TARGET_X86_64_MACOS]   = SV("x86_64-macos"),
    [CC_TARGET_AARCH64_MACOS]  = SV("aarch64-macos"),
    [CC_TARGET_X86_64_WINDOWS] = SV("x86_64-windows"),
};

// Target-specific type configuration.
// Sizes are in bytes. Type fields use CcBasicTypeKind.
typedef struct CcTargetConfig CcTargetConfig;
struct CcTargetConfig {
    CcTarget target;
    uint8_t sizeof_short;
    uint8_t sizeof_int;
    uint8_t sizeof_long;
    uint8_t sizeof_long_long;
    uint8_t sizeof_float;
    uint8_t sizeof_double;
    uint8_t sizeof_long_double;
    uint8_t sizeof_pointer;
    CcBasicTypeKind size_type;     // size_t underlying type
    CcBasicTypeKind ptrdiff_type;  // ptrdiff_t underlying type
    CcBasicTypeKind wchar_type;    // wchar_t underlying type
    CcBasicTypeKind char16_type;   // char16_t underlying type
    CcBasicTypeKind char32_type;   // char32_t underlying type
    CcBasicTypeKind wint_type;     // wint_t underlying type
    CcBasicTypeKind intmax_type;   // intmax_t underlying type
    CcBasicTypeKind intptr_type;   // intptr_t underlying type
    _Bool char_is_signed;          // plain 'char' is signed
};

static inline CcTargetConfig
cc_target_x86_64_linux(void){
    return (CcTargetConfig){
        .target            = CC_TARGET_X86_64_LINUX,
        .sizeof_short      = 2,
        .sizeof_int        = 4,
        .sizeof_long       = 8,
        .sizeof_long_long  = 8,
        .sizeof_float      = 4,
        .sizeof_double     = 8,
        .sizeof_long_double= 16,
        .sizeof_pointer    = 8,
        .size_type         = CCBT_unsigned_long,
        .ptrdiff_type      = CCBT_long,
        .wchar_type        = CCBT_int,
        .char16_type       = CCBT_unsigned_short,
        .char32_type       = CCBT_unsigned,
        .wint_type         = CCBT_unsigned,
        .intmax_type       = CCBT_long,
        .intptr_type       = CCBT_long,
        .char_is_signed    = 1,
    };
}

static inline CcTargetConfig
cc_target_aarch64_linux(void){
    return (CcTargetConfig){
        .target            = CC_TARGET_AARCH64_LINUX,
        .sizeof_short      = 2,
        .sizeof_int        = 4,
        .sizeof_long       = 8,
        .sizeof_long_long  = 8,
        .sizeof_float      = 4,
        .sizeof_double     = 8,
        .sizeof_long_double= 16,
        .sizeof_pointer    = 8,
        .size_type         = CCBT_unsigned_long,
        .ptrdiff_type      = CCBT_long,
        .wchar_type        = CCBT_unsigned,
        .char16_type       = CCBT_unsigned_short,
        .char32_type       = CCBT_unsigned,
        .wint_type         = CCBT_unsigned,
        .intmax_type       = CCBT_long,
        .intptr_type       = CCBT_long,
        .char_is_signed    = 0,
    };
}

static inline CcTargetConfig
cc_target_x86_64_macos(void){
    return (CcTargetConfig){
        .target            = CC_TARGET_X86_64_MACOS,
        .sizeof_short      = 2,
        .sizeof_int        = 4,
        .sizeof_long       = 8,
        .sizeof_long_long  = 8,
        .sizeof_float      = 4,
        .sizeof_double     = 8,
        .sizeof_long_double= 16,
        .sizeof_pointer    = 8,
        .size_type         = CCBT_unsigned_long,
        .ptrdiff_type      = CCBT_long,
        .wchar_type        = CCBT_int,
        .char16_type       = CCBT_unsigned_short,
        .char32_type       = CCBT_unsigned,
        .wint_type         = CCBT_int,
        .intmax_type       = CCBT_long,
        .intptr_type       = CCBT_long,
        .char_is_signed    = 1,
    };
}

static inline CcTargetConfig
cc_target_aarch64_macos(void){
    return (CcTargetConfig){
        .target            = CC_TARGET_AARCH64_MACOS,
        .sizeof_short      = 2,
        .sizeof_int        = 4,
        .sizeof_long       = 8,
        .sizeof_long_long  = 8,
        .sizeof_float      = 4,
        .sizeof_double     = 8,
        .sizeof_long_double= 16,
        .sizeof_pointer    = 8,
        .size_type         = CCBT_unsigned_long,
        .ptrdiff_type      = CCBT_long,
        .wchar_type        = CCBT_int,
        .char16_type       = CCBT_unsigned_short,
        .char32_type       = CCBT_unsigned,
        .wint_type         = CCBT_int,
        .intmax_type       = CCBT_long,
        .intptr_type       = CCBT_long,
        .char_is_signed    = 0,
    };
}

static inline CcTargetConfig
cc_target_x86_64_windows(void){
    return (CcTargetConfig){
        .target            = CC_TARGET_X86_64_WINDOWS,
        .sizeof_short      = 2,
        .sizeof_int        = 4,
        .sizeof_long       = 4,
        .sizeof_long_long  = 8,
        .sizeof_float      = 4,
        .sizeof_double     = 8,
        .sizeof_long_double= 8,
        .sizeof_pointer    = 8,
        .size_type         = CCBT_unsigned_long_long,
        .ptrdiff_type      = CCBT_long_long,
        .wchar_type        = CCBT_unsigned_short,
        .char16_type       = CCBT_unsigned_short,
        .char32_type       = CCBT_unsigned,
        .wint_type         = CCBT_unsigned_short,
        .intmax_type       = CCBT_long_long,
        .intptr_type       = CCBT_long_long,
        .char_is_signed    = 1,
    };
}

typedef CcTargetConfig (* _Nonnull CcTargetFunc)(void);
static const CcTargetFunc cc_target_funcs[CC_TARGET_COUNT] = {
    [CC_TARGET_X86_64_LINUX]   = cc_target_x86_64_linux,
    [CC_TARGET_AARCH64_LINUX]  = cc_target_aarch64_linux,
    [CC_TARGET_X86_64_MACOS]   = cc_target_x86_64_macos,
    [CC_TARGET_AARCH64_MACOS]  = cc_target_aarch64_macos,
    [CC_TARGET_X86_64_WINDOWS] = cc_target_x86_64_windows,
};

#if defined(__APPLE__) && defined(__aarch64__)
    enum {CC_TARGET_NATIVE = CC_TARGET_AARCH64_MACOS};
#elif defined(__APPLE__) && defined(__x86_64__)
    enum {CC_TARGET_NATIVE = CC_TARGET_X86_64_MACOS};
#elif defined(__linux__) && defined(__aarch64__)
    enum {CC_TARGET_NATIVE = CC_TARGET_AARCH64_LINUX};
#elif defined(__linux__) && defined(__x86_64__)
    enum {CC_TARGET_NATIVE = CC_TARGET_X86_64_LINUX};
#elif defined(_WIN32) && defined(_M_X64)
    enum {CC_TARGET_NATIVE = CC_TARGET_X86_64_WINDOWS};
#else
    #error "Unknown native target"
#endif

#ifdef __clang__
#pragma clang assume_nonnull end
#endif
#endif
