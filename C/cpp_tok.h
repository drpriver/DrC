#ifndef C_CPP_TOK_H
#define C_CPP_TOK_H
//
// Copyright © 2026-2026, David Priver <david@davidpriver.com>
//
#include <stdint.h>
#include "../Drp/ByteBuffer.h"
#include "srcloc.h"
#include "../Drp/cstring_view.h"
#include "../Drp/typed_enum.h"

#ifdef __clang__
#pragma clang assume_nonnull begin
#endif

// Some of these tokens are internal
enum CppTokenType TYPED_ENUM(uint64_t) {
    CPP_END_OF_INPUT       = 0,
    CPP_HEADER_NAME        = 1,
    CPP_IDENTIFIER         = 2,
    CPP_NUMBER             = 3,
    CPP_CHAR               = 4,
    CPP_STRING             = 5,
    CPP_PUNCTUATOR         = 6,
    CPP_WHITESPACE         = 7,
    CPP_NEWLINE            = 8,
    CPP_OTHER              = 9,
    CPP_PLACEMARKER        = 10,
    CPP_REENABLE           = 11,
    CPP_EMBED              = 12,
    CPP_END_OF_SOURCE_FILE = 13,
    CPP_DIRECTIVE          = 14,
};
TYPEDEF_ENUM(CppTokenType, uint64_t);
static const StringView CppTokenTypeSV[] = {
    [CPP_END_OF_INPUT]       = SVI("END_OF_INPUT"),
    [CPP_END_OF_SOURCE_FILE] = SVI("END_OF_SOURCE_FILE"),
    [CPP_DIRECTIVE]          = SVI("DIRECTIVE"),
    [CPP_HEADER_NAME]        = SVI("HEADER_NAME"),
    [CPP_IDENTIFIER ]        = SVI("IDENTIFIER"),
    [CPP_NUMBER     ]        = SVI("NUMBER"),
    [CPP_CHAR       ]        = SVI("CHAR"),
    [CPP_STRING     ]        = SVI("STRING"),
    [CPP_PUNCTUATOR ]        = SVI("PUNCTUATOR"),
    [CPP_WHITESPACE ]        = SVI("WHITESPACE"),
    [CPP_NEWLINE    ]        = SVI("NEWLINE"),
    [CPP_OTHER      ]        = SVI("OTHER"),
    [CPP_PLACEMARKER]        = SVI("PLACEMARKER"),
    [CPP_REENABLE   ]        = SVI("REENABLE"),
    [CPP_EMBED      ]        = SVI("EMBED"),
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
        ByteView bytes;
        struct {
            void *data1, *data2;
        };
    };
    SrcLoc loc;
};

static inline 
_Bool
cpp_token_is_end(CppToken tok){
    return tok.type == CPP_END_OF_SOURCE_FILE || tok.type == CPP_END_OF_INPUT;
}

#ifdef __clang__
#pragma clang assume_nonnull end
#endif
#endif
