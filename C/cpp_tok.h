#ifndef C_CPP_TOK_H
#define C_CPP_TOK_H
//
// Copyright © 2026-2026, David Priver <david@davidpriver.com>
//
#include <stdint.h>
#include "../Drp/typed_enum.h"

#ifdef __clang__
#pragma clang assume_nonnull begin
#endif

enum CPPTokenType TYPED_ENUM(uint64_t) {
    CPP_EOF         = 0,
    CPP_HEADER_NAME = 1,
    CPP_IDENTIFIER  = 2,
    CPP_NUMBER      = 3,
    CPP_CHAR        = 4,
    CPP_STRING      = 5,
    CPP_PUNCTUATOR  = 6,
    CPP_WHITESPACE  = 7,
    CPP_NEWLINE     = 8,
    CPP_PLACEMARKER = 9,
};
TYPEDEF_ENUM(CPPTokenType, uint64_t);
typedef struct SrcLoc SrcLoc;
struct SrcLoc {
    union {
        struct {
            uint32_t line;
            uint16_t column;
            uint16_t file;
        };
        uint64_t bits;
    };
};

typedef struct CPPToken CPPToken;
struct CPPToken {
    CPPTokenType type;
    SrcLoc loc;
    SrcLoc expand_loc;
    StringView txt;
};
_Static_assert(sizeof(CPPToken) == 5*sizeof(uint64_t), "");

#ifdef __clang__
#pragma clang assume_nonnull end
#endif
#endif
