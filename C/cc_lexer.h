#ifndef C_CC_LEXER_H
#define C_CC_LEXER_H
//
// Copyright © 2026-2026, David Priver <david@davidpriver.com>
//
#include "c_tok.h"
#include "cpp_preprocessor.h"
#ifdef __clang__
#pragma clang assume_nonnull begin
#endif

typedef struct CcLexer CcLexer;
struct CcLexer {
    CPreprocessor cpp;
};

static int cc_lex_next_token(CcLexer* lexer, CCToken* tok);

#ifdef __clang__
#pragma clang assume_nonnull end
#endif
#endif
