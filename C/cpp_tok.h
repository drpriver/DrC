#ifndef C_CPP_TOK_H
#define C_CPP_TOK_H
//
// Copyright © 2026-2026, David Priver <david@davidpriver.com>
//
#include <stdint.h>
#include "srcloc.h"
#include "../Drp/long_string.h"
#include "../Drp/typed_enum.h"

#ifdef __clang__
#pragma clang assume_nonnull begin
#endif

// Some of these tokens are internal
enum CppTokenType TYPED_ENUM(uint64_t) {
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
    CPP_REENABLE    = 11,
};
TYPEDEF_ENUM(CppTokenType, uint64_t);
static const StringView CppTokenTypeSV[] = {
    [CPP_EOF        ] = SVI("EOF"),
    [CPP_HEADER_NAME] = SVI("HEADER_NAME"),
    [CPP_IDENTIFIER ] = SVI("IDENTIFIER"),
    [CPP_NUMBER     ] = SVI("NUMBER"),
    [CPP_CHAR       ] = SVI("CHAR"),
    [CPP_STRING     ] = SVI("STRING"),
    [CPP_PUNCTUATOR ] = SVI("PUNCTUATOR"),
    [CPP_WHITESPACE ] = SVI("WHITESPACE"),
    [CPP_NEWLINE    ] = SVI("NEWLINE"),
    [CPP_OTHER      ] = SVI("OTHER"),
    [CPP_PLACEMARKER] = SVI("PLACEMARKER"),
    [CPP_REENABLE   ] = SVI("REENABLE"),
};


typedef struct CppToken CppToken;
struct CppToken {
    union {
        uint64_t _bits;
        struct {
            CppTokenType type: 4;
            uint64_t _pad: 4;
            uint64_t disabled: 1;
            uint64_t _pad2: 7;
            uint64_t param_idx: 16; // 0 means not a param, otherwise 1-based idx
            uint64_t punct: 32;
        };
    };
    union {
        StringView txt;
        struct {
            void *data1, *data2;
        };
    };
    SrcLoc loc;
};

#ifdef __clang__
#pragma clang assume_nonnull end
#endif
#endif
