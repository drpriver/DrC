#ifndef C_PP_TOK_H
#define C_PP_TOK_H
//
// Copyright © 2026-2026, David Priver <david@davidpriver.com>
//
#include <stdint.h>
#include "../Drp/typed_enum.h"

#ifdef __clang__
#pragma clang assume_nonnull begin
#endif

enum PPTokenType TYPED_ENUM(uint64_t) {
    PP_EOF         = 0,
    PP_HEADER_NAME = 1,
    PP_IDENTIFIER  = 2,
    PP_NUMBER      = 3,
    PP_CHAR        = 4,
    PP_STRING      = 5,
    PP_PUNCTUATOR  = 6,
    PP_WHITESPACE  = 7,
    PP_NEWLINE     = 8,
    PP_PLACEMARKER = 9,
};
TYPEDEF_ENUM(PPTokenType, uint64_t);

typedef struct PPToken PPToken;
struct PPToken {
    PPTokenType type: 4;
    uint64_t length:  12;
    uint64_t line:    24;
    uint64_t column:  12; // Saturates
    uint64_t file:    12;

    uint64_t exp_line:   24;
    uint64_t exp_column: 12; // Saturates
    uint64_t exp_file:   12;
    uint64_t _padding:   16;
    const char* txt;
};
_Static_assert(sizeof(PPToken) == 3*sizeof(uint64_t), "");

#ifdef __clang__
#pragma clang assume_nonnull end
#endif
#endif
