#ifndef C_CC_LEXER_C
#define C_CC_LEXER_C
//
// Copyright © 2026-2026, David Priver <david@davidpriver.com>
//
#include <stdarg.h>
#include "cc_lexer.h"
#include "cpp_tok.h"
#ifdef __clang__
#pragma clang assume_nonnull begin
#endif
enum {
    CC_LEX_NO_ERROR,
    CC_LEX_OOM_ERROR,
    CC_LEX_SYNTAX_ERROR,
    CC_LEX_UNREACHABLE_ERROR,
    CC_LEX_UNIMPLEMENTED_ERROR,
    CC_LEX_FILE_NOT_FOUND_ERROR,
};

static int cpp_ident_to_cc_tok(CcLexer*, CPPToken*, CCToken*);
static int cpp_number_to_cc_tok(CcLexer*, CPPToken*, CCToken*);
static int cpp_string_to_cc_tok(CcLexer*, CPPToken*, CCToken*);
static int cpp_char_to_cc_tok(CcLexer*, CPPToken*, CCToken*);
static int cpp_punct_to_cc_tok(CcLexer*, CPPToken*, CCToken*);
LOG_PRINTF(3, 4) static int  cc_error(CcLexer*, SrcLoc, const char*, ...);
LOG_PRINTF(3, 4) static void cc_warn(CcLexer*, SrcLoc, const char*, ...);
LOG_PRINTF(3, 4) static void cc_info(CcLexer*, SrcLoc, const char*, ...);
static
int
cc_lex_next_token(CcLexer* lexer, CCToken* tok){
    CPPToken cpp_tok;
    int err;
    for(;;){
        err = cpp_next_token(&lexer->cpp, &cpp_tok);
        if(err) return err;
        switch(cpp_tok.type){
            case CPP_EOF:
                *tok = (CCToken){.type=CC_EOF};
                return 0;
            case CPP_IDENTIFIER:
                return cpp_ident_to_cc_tok(lexer, &cpp_tok, tok);
            case CPP_NUMBER:
                return cpp_number_to_cc_tok(lexer, &cpp_tok, tok);
            case CPP_CHAR:
                return cpp_char_to_cc_tok(lexer, &cpp_tok, tok);
            case CPP_STRING:
                return cpp_string_to_cc_tok(lexer, &cpp_tok, tok);
            case CPP_PUNCTUATOR:
                return cpp_punct_to_cc_tok(lexer, &cpp_tok, tok);
            case CPP_WHITESPACE:
            case CPP_NEWLINE:
                continue;
            case CPP_OTHER:
                return cc_error(lexer, cpp_tok.loc, "Invalid preprocessor token escaped to lexer: '%.*s'", sv_p(cpp_tok.txt));
            case CPP_HEADER_NAME:
            case CPP_PLACEMARKER:
            case CPP_REENABLE:
                return CC_LEX_UNREACHABLE_ERROR;
        }
    }
}

#define CKWS2(X) \
X(do) \
X(if) \

#define CKWS3(X) \
X(for) \
X(int) \

#define CKWS4(X) \
X(true) \
X(long) \
X(char) \
X(auto) \
X(bool) \
X(else) \
X(enum) \
X(case) \
X(goto) \
X(void) \

#define CKWS5(X) \
X(break) \
X(false) \
X(float) \
X(const) \
X(short) \
X(union) \
X(while) \

#define CKWS6(X) \
X(double) \
X(extern) \
X(inline) \
X(return) \
X(signed) \
X(sizeof) \
X(static) \
X(struct) \
X(switch) \
X(typeof) \

#define CKWS7(X) \
X(alignas) \
X(alignof) \
X(default) \
X(typedef) \
X(nullptr) \
X(_Atomic) \
X(_BitInt) \

#define CKWS8(X) \
X(_Complex) \
X(continue) \
X(register) \
X(restrict) \
X(unsigned) \
X(volatile) \
X(_Generic) \
X(_Float16) \
X(_Float32) \
X(_Float64) \

#define CKWS9(X) \
X(constexpr) \
X(_Noreturn) \
X(_Float128) \

#define CKWS10(X) \
X(_Imaginary) \
X(_Decimal32) \
X(_Decimal64) \

#define CKWS11(X) \
X(_Decimal128) \

#define CKWS12(X) \
X(thread_local) \

#define CKWS13(X) \
X(static_assert) \
X(typeof_unqual) \

static
uint32_t
cc_lex_str_to_keyword(StringView txt){
#define X(kw) if(sv_equals(txt, SV(#kw))) return CC_##kw;
    switch(txt.length){
        case 2:
            CKWS2(X);
            return (uint32_t)-1;
        case 3:
            CKWS3(X);
            return (uint32_t)-1;
        case 4:
            CKWS4(X);
            return (uint32_t)-1;
        case 5:
            CKWS5(X);
            if(sv_equals(txt, SV("_Bool"))) return CC_bool;
            return (uint32_t)-1;
        case 6:
            CKWS6(X);
            return (uint32_t)-1;
        case 7:
            CKWS7(X);
            return (uint32_t)-1;
        case 8:
            CKWS8(X);
            if(sv_equals(txt, SV("_Alignas"))) return CC_alignas;
            if(sv_equals(txt, SV("_Alignof"))) return CC_alignof;
            return (uint32_t)-1;
        case 9:
            CKWS9(X);
            return (uint32_t)-1;
        case 10:
            CKWS10(X);
            return (uint32_t)-1;
        case 11:
            CKWS11(X);
            return (uint32_t)-1;
        case 12:
            CKWS12(X);
            return (uint32_t)-1;
        case 13:
            CKWS13(X)
            if(sv_equals(txt, SV("_Thread_local"))) return CC_thread_local;
            return (uint32_t)-1;
        case 14:
            if(sv_equals(txt, SV("_Static_assert"))) return CC_static_assert;
            return (uint32_t)-1;
        default:
            return (uint32_t)-1;
    }
}
static
int
cpp_ident_to_cc_tok(CcLexer* lexer, CPPToken* cpptok, CCToken* cctok){
    uint32_t kw = cc_lex_str_to_keyword(cpptok->txt);
    if(kw != (uint32_t)-1){
        *cctok = (CCToken){
            .kw = {
                .type = CC_KEYWORD,
                .kw = (CCKeyword)kw,
            },
            .loc = cpptok->loc,
        };
        return 0;
    }
    Atom a = AT_atomize(lexer->cpp.at, cpptok->txt.text, cpptok->txt.length);
    if(!a) return CC_LEX_OOM_ERROR;
    *cctok = (CCToken){
        .ident = {
            .type = CC_IDENTIFIER,
            .ident = a,
        },
        .loc = cpptok->loc,
    };
    return 0;
}

static
int
cpp_number_to_cc_tok(CcLexer* lexer, CPPToken* cpptok, CCToken* cctok){
    (void)lexer;
    (void)cpptok;
    (void)cctok;
    return CC_LEX_UNIMPLEMENTED_ERROR;
}
static
int
cpp_string_to_cc_tok(CcLexer* lexer, CPPToken* cpptok, CCToken* cctok){
    (void)lexer;
    (void)cpptok;
    (void)cctok;
    return CC_LEX_UNIMPLEMENTED_ERROR;
}
static
int
cpp_char_to_cc_tok(CcLexer* lexer, CPPToken* cpptok, CCToken* cctok){
    (void)lexer;
    (void)cpptok;
    (void)cctok;
    return CC_LEX_UNIMPLEMENTED_ERROR;
}
static
int
cpp_punct_to_cc_tok(CcLexer* lexer, CPPToken* cpptok, CCToken* cctok){
    (void)lexer;
    (void)cpptok;
    (void)cctok;
    return CC_LEX_UNIMPLEMENTED_ERROR;
}

// just reuse CPP's machinery for now
static void cpp_msg(CPreprocessor* cpp, SrcLoc loc, LogLevel level, const char* prefix, const char* fmt, va_list va);
static
int
cc_error(CcLexer* lexer, SrcLoc loc, const char* fmt, ...){
    va_list va;
    va_start(va, fmt);
    cpp_msg(&lexer->cpp, loc, LOG_PRINT_ERROR, "error", fmt, va);
    va_end(va);
    return CC_LEX_SYNTAX_ERROR;
}

static
void
cc_warn(CcLexer* lexer, SrcLoc loc, const char* fmt, ...){
    va_list va;
    va_start(va, fmt);
    cpp_msg(&lexer->cpp, loc, LOG_PRINT_ERROR, "warning", fmt, va);
    va_end(va);
}

static
void
cc_info(CcLexer* lexer, SrcLoc loc, const char* fmt, ...){
    va_list va;
    va_start(va, fmt);
    cpp_msg(&lexer->cpp, loc, LOG_PRINT_ERROR, "info", fmt, va);
    va_end(va);
}
#ifdef __clang__
#pragma clang assume_nonnull end
#endif
#endif
