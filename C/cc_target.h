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

enum CcBitfieldABI TYPED_ENUM(uint8_t) {
    CC_BITFIELD_SYSV,  // SysV/Itanium: same-sized types share storage units
    CC_BITFIELD_MSVC,  // MSVC: different types never share storage units
};
TYPEDEF_ENUM(CcBitfieldABI, uint8_t);

// Target-specific type configuration.
// Sizes are in bytes. Type fields use CcBasicTypeKind.
typedef struct CcTargetConfig CcTargetConfig;
struct CcTargetConfig {
    CcTarget target;
    uint8_t sizeof_[CCBT_COUNT];
    uint8_t alignof_[CCBT_COUNT];
    CcBitfieldABI bitfield_abi;
    CcBasicTypeKind size_type;
    CcBasicTypeKind ptrdiff_type;
    CcBasicTypeKind wchar_type;
    CcBasicTypeKind char16_type;
    CcBasicTypeKind char32_type;
    CcBasicTypeKind wint_type;
    CcBasicTypeKind intmax_type;
    CcBasicTypeKind intptr_type;
    CcBasicTypeKind int64_type;
    CcBasicTypeKind sig_atomic_type;
    CcBasicTypeKind int_fast8_type;
    CcBasicTypeKind int_fast16_type;
    CcBasicTypeKind int_fast32_type;
    CcBasicTypeKind int_fast64_type;
    uint8_t max_align;
    _Bool is_lp64;
    _Bool user_label_prefix;
    _Bool char_is_signed;
    StringView platform_macros; // nul-character terminated macros. use `=` to include an arg. length should include terminating nul
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
        .sig_atomic_type = CCBT_int,
        .int_fast8_type  = CCBT_signed_char,
        .int_fast16_type = CCBT_long,
        .int_fast32_type = CCBT_long,
        .int_fast64_type = CCBT_long,
        .max_align = 16,
        .is_lp64 = 1,
        .user_label_prefix = 0,
        .char_is_signed = 1,
        .platform_macros = SV(
            "__linux__\0"
            "__linux\0"
            "linux\0"
            "__gnu_linux__\0"
            "__unix__\0"
            "__unix\0"
            "unix\0"
            "__x86_64__\0"
            "__x86_64\0"
            "__amd64__\0"
            "__amd64\0"
        ),
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
        .sig_atomic_type = CCBT_int,
        .int_fast8_type  = CCBT_signed_char,
        .int_fast16_type = CCBT_long,
        .int_fast32_type = CCBT_long,
        .int_fast64_type = CCBT_long,
        .max_align = 16,
        .is_lp64 = 1,
        .user_label_prefix = 0,
        .char_is_signed = 0,
        .platform_macros = SV(
            "__linux__\0"
            "__linux\0"
            "linux\0"
            "__gnu_linux__\0"
            "__unix__\0"
            "__unix\0"
            "unix\0"
            "__aarch64__\0"
        ),
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
        .sig_atomic_type = CCBT_int,
        .int_fast8_type  = CCBT_signed_char,
        .int_fast16_type = CCBT_short,
        .int_fast32_type = CCBT_int,
        .int_fast64_type = CCBT_long_long,
        .max_align = 16,
        .is_lp64 = 1,
        .user_label_prefix = 1,
        .char_is_signed = 1,
        .platform_macros = SV(
            "__APPLE__\0"
            "__MACH__\0"
            "__x86_64__\0"
            "__x86_64\0"
            "__amd64__\0"
            "__amd64\0"
            "TARGET_CPU_X86_64\0"
            "TARGET_OS_MAC\0"
            "TARGET_OS_OSX\0"
            "__ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__=101400\0"
        ),
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
        .sig_atomic_type = CCBT_int,
        .int_fast8_type  = CCBT_signed_char,
        .int_fast16_type = CCBT_short,
        .int_fast32_type = CCBT_int,
        .int_fast64_type = CCBT_long_long,
        .max_align = 16,
        .is_lp64 = 1,
        .user_label_prefix = 1,
        .char_is_signed = 0,
        .platform_macros = SV(
            "__APPLE__\0"
            "__MACH__\0"
            "__aarch64__\0"
            "__arm64__\0"
            "TARGET_CPU_ARM64\0"
            "TARGET_OS_MAC\0"
            "TARGET_OS_OSX\0"
            "__ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__=110000\0"
        ),
    };
}

static inline CcTargetConfig
cc_target_x86_64_windows(void){
    return (CcTargetConfig){
        .target = CC_TARGET_X86_64_WINDOWS,
        .bitfield_abi = CC_BITFIELD_MSVC,
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
        .sig_atomic_type = CCBT_int,
        .int_fast8_type  = CCBT_signed_char,
        .int_fast16_type = CCBT_short,
        .int_fast32_type = CCBT_int,
        .int_fast64_type = CCBT_long_long,
        .max_align = 8,
        .is_lp64 = 0,
        .user_label_prefix = 0,
        .char_is_signed = 1,
        .platform_macros = SV(
            "_WIN32\0"
            "_WIN64\0"
            "__x86_64__\0"
            "__x86_64\0"
            "__amd64__\0"
            "__amd64\0"
            "_M_X64\0"
            "_M_AMD64\0"
        ),
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
        .sig_atomic_type = CCBT_int,
        .int_fast8_type  = CCBT_signed_char,
        .int_fast16_type = CCBT_long,
        .int_fast32_type = CCBT_long,
        .int_fast64_type = CCBT_long,
        .max_align = 16,
        .is_lp64 = 1,
        .user_label_prefix = 0,
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
