#ifndef C_CC_PARSER_H
#define C_CC_PARSER_H
//
// Copyright © 2026-2026, David Priver <david@davidpriver.com>
//
#include "cc_lexer.h"
#include "c_tok.h"
#ifndef MARRAY_CCTOKEN
#define MARRAY_CCTOKEN
#define MARRAY_T CCToken
#include "../Drp/Marray.h"
#endif
#ifdef __clang__
#pragma clang assume_nonnull begin
#endif

typedef struct CcParser CcParser;
struct CcParser {
    CcLexer lexer;
    // for lookahead/pushback, LIFO
    Marray(CCToken) pending;
};

#ifdef __clang__
#pragma clang assume_nonnull end
#endif
#endif
