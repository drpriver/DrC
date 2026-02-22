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
    CC_TARGET_TEST, // stable target for tests, not a real platform
    CC_TARGET_COUNT,
};
TYPEDEF_ENUM(CcTarget, int);

static const StringView cc_target_names[CC_TARGET_COUNT] = {
    [CC_TARGET_X86_64_LINUX]   = SV("x86_64-linux"),
    [CC_TARGET_AARCH64_LINUX]  = SV("aarch64-linux"),
    [CC_TARGET_X86_64_MACOS]   = SV("x86_64-macos"),
    [CC_TARGET_AARCH64_MACOS]  = SV("aarch64-macos"),
    [CC_TARGET_X86_64_WINDOWS] = SV("x86_64-windows"),
    [CC_TARGET_TEST]           = SV("test"),
};

// Target-specific type configuration.
// Sizes are in bytes. Type fields use CcBasicTypeKind.
typedef struct CcTargetConfig CcTargetConfig;
struct CcTargetConfig {
    CcTarget target;
    uint8_t sizeof_[CCBT_COUNT];
    uint8_t alignof_[CCBT_COUNT];
    CcBasicTypeKind size_type;
    CcBasicTypeKind ptrdiff_type;
    CcBasicTypeKind wchar_type;
    CcBasicTypeKind char16_type;
    CcBasicTypeKind char32_type;
    CcBasicTypeKind wint_type;
    CcBasicTypeKind intmax_type;
    CcBasicTypeKind intptr_type;
    CcBasicTypeKind int64_type;
    _Bool char_is_signed;
};

// sizeof(void) = 1 is a GNU extension.
// alignof complex types = alignof component type.
// nullptr_t is pointer-sized; sizeof_[CCBT_nullptr_t] doubles as pointer size.

static inline CcTargetConfig
cc_target_x86_64_linux(void){
    return (CcTargetConfig){
        .target = CC_TARGET_X86_64_LINUX,
        .sizeof_ = {
            [CCBT_void]                = 1,
            [CCBT_bool]                = 1,
            [CCBT_char]                = 1,
            [CCBT_signed_char]         = 1,
            [CCBT_unsigned_char]       = 1,
            [CCBT_short]               = 2,
            [CCBT_unsigned_short]      = 2,
            [CCBT_int]                 = 4,
            [CCBT_unsigned]            = 4,
            [CCBT_long]                = 8,
            [CCBT_unsigned_long]       = 8,
            [CCBT_long_long]           = 8,
            [CCBT_unsigned_long_long]  = 8,
            [CCBT_float]               = 4,
            [CCBT_double]              = 8,
            [CCBT_long_double]         = 16,
            [CCBT_float_complex]       = 8,
            [CCBT_double_complex]      = 16,
            [CCBT_long_double_complex] = 32,
            [CCBT_nullptr_t]           = 8,
        },
        .alignof_ = {
            [CCBT_void]                = 1,
            [CCBT_bool]                = 1,
            [CCBT_char]                = 1,
            [CCBT_signed_char]         = 1,
            [CCBT_unsigned_char]       = 1,
            [CCBT_short]               = 2,
            [CCBT_unsigned_short]      = 2,
            [CCBT_int]                 = 4,
            [CCBT_unsigned]            = 4,
            [CCBT_long]                = 8,
            [CCBT_unsigned_long]       = 8,
            [CCBT_long_long]           = 8,
            [CCBT_unsigned_long_long]  = 8,
            [CCBT_float]               = 4,
            [CCBT_double]              = 8,
            [CCBT_long_double]         = 16,
            [CCBT_float_complex]       = 4,
            [CCBT_double_complex]      = 8,
            [CCBT_long_double_complex] = 16,
            [CCBT_nullptr_t]           = 8,
        },
        .size_type    = CCBT_unsigned_long,
        .ptrdiff_type = CCBT_long,
        .wchar_type   = CCBT_int,
        .char16_type  = CCBT_unsigned_short,
        .char32_type  = CCBT_unsigned,
        .wint_type    = CCBT_unsigned,
        .intmax_type  = CCBT_long,
        .intptr_type  = CCBT_long,
        .int64_type   = CCBT_long,
        .char_is_signed = 1,
    };
}

static inline CcTargetConfig
cc_target_aarch64_linux(void){
    return (CcTargetConfig){
        .target = CC_TARGET_AARCH64_LINUX,
        .sizeof_ = {
            [CCBT_void]                = 1,
            [CCBT_bool]                = 1,
            [CCBT_char]                = 1,
            [CCBT_signed_char]         = 1,
            [CCBT_unsigned_char]       = 1,
            [CCBT_short]               = 2,
            [CCBT_unsigned_short]      = 2,
            [CCBT_int]                 = 4,
            [CCBT_unsigned]            = 4,
            [CCBT_long]                = 8,
            [CCBT_unsigned_long]       = 8,
            [CCBT_long_long]           = 8,
            [CCBT_unsigned_long_long]  = 8,
            [CCBT_float]               = 4,
            [CCBT_double]              = 8,
            [CCBT_long_double]         = 16,
            [CCBT_float_complex]       = 8,
            [CCBT_double_complex]      = 16,
            [CCBT_long_double_complex] = 32,
            [CCBT_nullptr_t]           = 8,
        },
        .alignof_ = {
            [CCBT_void]                = 1,
            [CCBT_bool]                = 1,
            [CCBT_char]                = 1,
            [CCBT_signed_char]         = 1,
            [CCBT_unsigned_char]       = 1,
            [CCBT_short]               = 2,
            [CCBT_unsigned_short]      = 2,
            [CCBT_int]                 = 4,
            [CCBT_unsigned]            = 4,
            [CCBT_long]                = 8,
            [CCBT_unsigned_long]       = 8,
            [CCBT_long_long]           = 8,
            [CCBT_unsigned_long_long]  = 8,
            [CCBT_float]               = 4,
            [CCBT_double]              = 8,
            [CCBT_long_double]         = 16,
            [CCBT_float_complex]       = 4,
            [CCBT_double_complex]      = 8,
            [CCBT_long_double_complex] = 16,
            [CCBT_nullptr_t]           = 8,
        },
        .size_type    = CCBT_unsigned_long,
        .ptrdiff_type = CCBT_long,
        .wchar_type   = CCBT_unsigned,
        .char16_type  = CCBT_unsigned_short,
        .char32_type  = CCBT_unsigned,
        .wint_type    = CCBT_unsigned,
        .intmax_type  = CCBT_long,
        .intptr_type  = CCBT_long,
        .int64_type   = CCBT_long,
        .char_is_signed = 0,
    };
}

static inline CcTargetConfig
cc_target_x86_64_macos(void){
    return (CcTargetConfig){
        .target = CC_TARGET_X86_64_MACOS,
        .sizeof_ = {
            [CCBT_void]                = 1,
            [CCBT_bool]                = 1,
            [CCBT_char]                = 1,
            [CCBT_signed_char]         = 1,
            [CCBT_unsigned_char]       = 1,
            [CCBT_short]               = 2,
            [CCBT_unsigned_short]      = 2,
            [CCBT_int]                 = 4,
            [CCBT_unsigned]            = 4,
            [CCBT_long]                = 8,
            [CCBT_unsigned_long]       = 8,
            [CCBT_long_long]           = 8,
            [CCBT_unsigned_long_long]  = 8,
            [CCBT_float]               = 4,
            [CCBT_double]              = 8,
            [CCBT_long_double]         = 16,
            [CCBT_float_complex]       = 8,
            [CCBT_double_complex]      = 16,
            [CCBT_long_double_complex] = 32,
            [CCBT_nullptr_t]           = 8,
        },
        .alignof_ = {
            [CCBT_void]                = 1,
            [CCBT_bool]                = 1,
            [CCBT_char]                = 1,
            [CCBT_signed_char]         = 1,
            [CCBT_unsigned_char]       = 1,
            [CCBT_short]               = 2,
            [CCBT_unsigned_short]      = 2,
            [CCBT_int]                 = 4,
            [CCBT_unsigned]            = 4,
            [CCBT_long]                = 8,
            [CCBT_unsigned_long]       = 8,
            [CCBT_long_long]           = 8,
            [CCBT_unsigned_long_long]  = 8,
            [CCBT_float]               = 4,
            [CCBT_double]              = 8,
            [CCBT_long_double]         = 16,
            [CCBT_float_complex]       = 4,
            [CCBT_double_complex]      = 8,
            [CCBT_long_double_complex] = 16,
            [CCBT_nullptr_t]           = 8,
        },
        .size_type    = CCBT_unsigned_long,
        .ptrdiff_type = CCBT_long,
        .wchar_type   = CCBT_int,
        .char16_type  = CCBT_unsigned_short,
        .char32_type  = CCBT_unsigned,
        .wint_type    = CCBT_int,
        .intmax_type  = CCBT_long,
        .intptr_type  = CCBT_long,
        .int64_type   = CCBT_long_long,
        .char_is_signed = 1,
    };
}

static inline CcTargetConfig
cc_target_aarch64_macos(void){
    return (CcTargetConfig){
        .target = CC_TARGET_AARCH64_MACOS,
        .sizeof_ = {
            [CCBT_void]                = 1,
            [CCBT_bool]                = 1,
            [CCBT_char]                = 1,
            [CCBT_signed_char]         = 1,
            [CCBT_unsigned_char]       = 1,
            [CCBT_short]               = 2,
            [CCBT_unsigned_short]      = 2,
            [CCBT_int]                 = 4,
            [CCBT_unsigned]            = 4,
            [CCBT_long]                = 8,
            [CCBT_unsigned_long]       = 8,
            [CCBT_long_long]           = 8,
            [CCBT_unsigned_long_long]  = 8,
            [CCBT_float]               = 4,
            [CCBT_double]              = 8,
            [CCBT_long_double]         = 8,
            [CCBT_float_complex]       = 8,
            [CCBT_double_complex]      = 16,
            [CCBT_long_double_complex] = 16,
            [CCBT_nullptr_t]           = 8,
        },
        .alignof_ = {
            [CCBT_void]                = 1,
            [CCBT_bool]                = 1,
            [CCBT_char]                = 1,
            [CCBT_signed_char]         = 1,
            [CCBT_unsigned_char]       = 1,
            [CCBT_short]               = 2,
            [CCBT_unsigned_short]      = 2,
            [CCBT_int]                 = 4,
            [CCBT_unsigned]            = 4,
            [CCBT_long]                = 8,
            [CCBT_unsigned_long]       = 8,
            [CCBT_long_long]           = 8,
            [CCBT_unsigned_long_long]  = 8,
            [CCBT_float]               = 4,
            [CCBT_double]              = 8,
            [CCBT_long_double]         = 8,
            [CCBT_float_complex]       = 4,
            [CCBT_double_complex]      = 8,
            [CCBT_long_double_complex] = 8,
            [CCBT_nullptr_t]           = 8,
        },
        .size_type    = CCBT_unsigned_long,
        .ptrdiff_type = CCBT_long,
        .wchar_type   = CCBT_int,
        .char16_type  = CCBT_unsigned_short,
        .char32_type  = CCBT_unsigned,
        .wint_type    = CCBT_int,
        .intmax_type  = CCBT_long,
        .intptr_type  = CCBT_long,
        .int64_type   = CCBT_long_long,
        .char_is_signed = 0,
    };
}

static inline CcTargetConfig
cc_target_x86_64_windows(void){
    return (CcTargetConfig){
        .target = CC_TARGET_X86_64_WINDOWS,
        .sizeof_ = {
            [CCBT_void]                = 1,
            [CCBT_bool]                = 1,
            [CCBT_char]                = 1,
            [CCBT_signed_char]         = 1,
            [CCBT_unsigned_char]       = 1,
            [CCBT_short]               = 2,
            [CCBT_unsigned_short]      = 2,
            [CCBT_int]                 = 4,
            [CCBT_unsigned]            = 4,
            [CCBT_long]                = 4,
            [CCBT_unsigned_long]       = 4,
            [CCBT_long_long]           = 8,
            [CCBT_unsigned_long_long]  = 8,
            [CCBT_float]               = 4,
            [CCBT_double]              = 8,
            [CCBT_long_double]         = 8,
            [CCBT_float_complex]       = 8,
            [CCBT_double_complex]      = 16,
            [CCBT_long_double_complex] = 16,
            [CCBT_nullptr_t]           = 8,
        },
        .alignof_ = {
            [CCBT_void]                = 1,
            [CCBT_bool]                = 1,
            [CCBT_char]                = 1,
            [CCBT_signed_char]         = 1,
            [CCBT_unsigned_char]       = 1,
            [CCBT_short]               = 2,
            [CCBT_unsigned_short]      = 2,
            [CCBT_int]                 = 4,
            [CCBT_unsigned]            = 4,
            [CCBT_long]                = 4,
            [CCBT_unsigned_long]       = 4,
            [CCBT_long_long]           = 8,
            [CCBT_unsigned_long_long]  = 8,
            [CCBT_float]               = 4,
            [CCBT_double]              = 8,
            [CCBT_long_double]         = 8,
            [CCBT_float_complex]       = 4,
            [CCBT_double_complex]      = 8,
            [CCBT_long_double_complex] = 8,
            [CCBT_nullptr_t]           = 8,
        },
        .size_type    = CCBT_unsigned_long_long,
        .ptrdiff_type = CCBT_long_long,
        .wchar_type   = CCBT_unsigned_short,
        .char16_type  = CCBT_unsigned_short,
        .char32_type  = CCBT_unsigned,
        .wint_type    = CCBT_unsigned_short,
        .intmax_type  = CCBT_long_long,
        .intptr_type  = CCBT_long_long,
        .int64_type   = CCBT_long_long,
        .char_is_signed = 1,
    };
}

static inline CcTargetConfig
cc_target_test(void){
    return (CcTargetConfig){
        .target = CC_TARGET_TEST,
        .sizeof_ = {
            [CCBT_void]                = 1,
            [CCBT_bool]                = 1,
            [CCBT_char]                = 1,
            [CCBT_signed_char]         = 1,
            [CCBT_unsigned_char]       = 1,
            [CCBT_short]               = 2,
            [CCBT_unsigned_short]      = 2,
            [CCBT_int]                 = 4,
            [CCBT_unsigned]            = 4,
            [CCBT_long]                = 8,
            [CCBT_unsigned_long]       = 8,
            [CCBT_long_long]           = 8,
            [CCBT_unsigned_long_long]  = 8,
            [CCBT_float]               = 4,
            [CCBT_double]              = 8,
            [CCBT_long_double]         = 16,
            [CCBT_float_complex]       = 8,
            [CCBT_double_complex]      = 16,
            [CCBT_long_double_complex] = 32,
            [CCBT_nullptr_t]           = 8,
        },
        .alignof_ = {
            [CCBT_void]                = 1,
            [CCBT_bool]                = 1,
            [CCBT_char]                = 1,
            [CCBT_signed_char]         = 1,
            [CCBT_unsigned_char]       = 1,
            [CCBT_short]               = 2,
            [CCBT_unsigned_short]      = 2,
            [CCBT_int]                 = 4,
            [CCBT_unsigned]            = 4,
            [CCBT_long]                = 8,
            [CCBT_unsigned_long]       = 8,
            [CCBT_long_long]           = 8,
            [CCBT_unsigned_long_long]  = 8,
            [CCBT_float]               = 4,
            [CCBT_double]              = 8,
            [CCBT_long_double]         = 16,
            [CCBT_float_complex]       = 4,
            [CCBT_double_complex]      = 8,
            [CCBT_long_double_complex] = 16,
            [CCBT_nullptr_t]           = 8,
        },
        .size_type    = CCBT_unsigned_long,
        .ptrdiff_type = CCBT_long,
        .wchar_type   = CCBT_int,
        .char16_type  = CCBT_unsigned_short,
        .char32_type  = CCBT_unsigned,
        .wint_type    = CCBT_int,
        .intmax_type  = CCBT_long,
        .intptr_type  = CCBT_long,
        .int64_type   = CCBT_long,
        .char_is_signed = 1,
    };
}

typedef CcTargetConfig (* _Nonnull CcTargetFunc)(void);
static const CcTargetFunc cc_target_funcs[CC_TARGET_COUNT] = {
    [CC_TARGET_X86_64_LINUX]   = cc_target_x86_64_linux,
    [CC_TARGET_AARCH64_LINUX]  = cc_target_aarch64_linux,
    [CC_TARGET_X86_64_MACOS]   = cc_target_x86_64_macos,
    [CC_TARGET_AARCH64_MACOS]  = cc_target_aarch64_macos,
    [CC_TARGET_X86_64_WINDOWS] = cc_target_x86_64_windows,
    [CC_TARGET_TEST]           = cc_target_test,
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
