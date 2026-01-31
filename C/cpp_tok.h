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
    CPP_OTHER       = 9,
    CPP_PLACEMARKER = 10,
    CPP_REENABLE    = 11,
};
TYPEDEF_ENUM(CPPTokenType, uint64_t);
static const StringView CPPTokenTypeSV[] = {
    [CPP_EOF        ] = SV("EOF"),
    [CPP_HEADER_NAME] = SV("HEADER_NAME"),
    [CPP_IDENTIFIER ] = SV("IDENTIFIER"),
    [CPP_NUMBER     ] = SV("NUMBER"),
    [CPP_CHAR       ] = SV("CHAR"),
    [CPP_STRING     ] = SV("STRING"),
    [CPP_PUNCTUATOR ] = SV("PUNCTUATOR"),
    [CPP_WHITESPACE ] = SV("WHITESPACE"),
    [CPP_NEWLINE    ] = SV("NEWLINE"),
    [CPP_OTHER      ] = SV("OTHER"),
    [CPP_PLACEMARKER] = SV("PLACEMARKER"),
    [CPP_REENABLE   ] = SV("REENABLE"),
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
    union {
        uint64_t _bits;
        struct {
            CPPTokenType type: 4;
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
