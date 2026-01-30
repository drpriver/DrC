#ifndef C_CPP_TOK_H
#define C_CPP_TOK_H
//
// Copyright © 2026-2026, David Priver <david@davidpriver.com>
//
#include <stdint.h>
#include "../Drp/long_string.h"
#include "../Drp/typed_enum.h"

#ifdef __clang__
#pragma clang assume_nonnull begin
#endif

// Some of these tokens are internal
enum CPPTokenType TYPED_ENUM(uint32_t) {
    CPP_EOF         = 0,
    CPP_HEADER_NAME = 1,
    CPP_IDENTIFIER  = 2,
    CPP_NUMBER      = 3,
    CPP_CHAR        = 4,
    CPP_STRING      = 5,
    CPP_PUNCTUATOR  = 6,
    CPP_WHITESPACE  = 7,
    CPP_NEWLINE     = 8,
    CPP_OTHER       = 9,
    CPP_PLACEMARKER = 10,
};
TYPEDEF_ENUM(CPPTokenType, uint32_t);
static const StringView CPPTokenTypeSV[] = {
    [CPP_EOF        ] = SV("CPP_EOF"),
    [CPP_HEADER_NAME] = SV("CPP_HEADER_NAME"),
    [CPP_IDENTIFIER ] = SV("CPP_IDENTIFIER"),
    [CPP_NUMBER     ] = SV("CPP_NUMBER"),
    [CPP_CHAR       ] = SV("CPP_CHAR"),
    [CPP_STRING     ] = SV("CPP_STRING"),
    [CPP_PUNCTUATOR ] = SV("CPP_PUNCTUATOR"),
    [CPP_WHITESPACE ] = SV("CPP_WHITESPACE"),
    [CPP_NEWLINE    ] = SV("CPP_NEWLINE"),
    [CPP_OTHER      ] = SV("CPP_OTHER"),
    [CPP_PLACEMARKER] = SV("CPP_PLACEMARKER"),
};

typedef struct SrcLoc SrcLoc;
struct SrcLoc {
    union {
        uint64_t bits; // 0 is invalid
        struct {
            uint64_t file_id: 16;
            uint64_t column: 16;
            uint64_t line: 31;
            uint64_t is_actually_a_pointer: 1;
        };
        struct {
            uint64_t bits: 63; // (SrcLocExp*)(pointer.bits<<1)
            uint64_t is_actually_a_pointer: 1;
        } pointer;
    };
};
// Should be allocated in an arena
typedef struct SrcLocExp SrcLocExp;
struct SrcLocExp {
    uint64_t file_id: 16;
    uint64_t column: 16;
    uint64_t line: 32;
    SrcLocExp*_Nullable parent;
};

typedef struct CPPToken CPPToken;
struct CPPToken {
    CPPTokenType type;
    StringView txt;
    SrcLoc loc;
};

#ifdef __clang__
#pragma clang assume_nonnull end
#endif
#endif
