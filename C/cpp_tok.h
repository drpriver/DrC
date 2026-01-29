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
    uint32_t idx;
};

typedef struct CPPToken CPPToken;
struct CPPToken {
    CPPTokenType type;
    SrcLoc loc;
    StringView txt;
};
_Static_assert(sizeof(CPPToken) == 2*sizeof(uint32_t)+2*sizeof(size_t), "");

#ifdef __clang__
#pragma clang assume_nonnull end
#endif
#endif
