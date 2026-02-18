#ifndef C_CC_LEXER_C
#define C_CC_LEXER_C
//
// Copyright © 2026-2026, David Priver <david@davidpriver.com>
//
#include <stdarg.h>
#include "cc_lexer.h"
#include "cpp_tok.h"
#include "../Drp/parse_numbers.h"
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
X(_Countof) \
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
            if(sv_equals(txt, SV("countof"))) return CC__Countof;
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
    const char* s = cpptok->txt.text;
    size_t len = cpptok->txt.length;
    // Detect hex prefix before suffix stripping so we don't eat hex digits
    _Bool maybe_hex = (len > 2 && s[0] == '0' && (s[1] == 'x' || s[1] == 'X'));
    // Strip suffix from end
    _Bool has_u = 0;
    int num_l = 0;
    _Bool has_f = 0;
    while(len){
        char c = s[len-1];
        if(c == 'u' || c == 'U'){
            if(has_u) break;
            has_u = 1;
            len--;
        }
        else if(c == 'l' || c == 'L'){
            if(num_l >= 2) break;
            num_l++;
            len--;
        }
        else if(!maybe_hex && (c == 'f' || c == 'F')){
            if(has_f) break;
            has_f = 1;
            len--;
        }
        else break;
    }
    if(!len)
        return cc_error(lexer, cpptok->loc, "Invalid number literal");
    if(has_f && has_u)
        return cc_error(lexer, cpptok->loc, "Invalid suffix: 'f' and 'u' are mutually exclusive");
    if(has_f && num_l > 1)
        return cc_error(lexer, cpptok->loc, "Invalid suffix: 'f' and 'll' are mutually exclusive");
    // Strip digit separators into a stack buffer
    char buf[256];
    size_t buf_len = 0;
    for(size_t i = 0; i < len; i++){
        if(s[i] == '\'') continue;
        if(buf_len >= sizeof buf - 1)
            return cc_error(lexer, cpptok->loc, "Number literal too long");
        buf[buf_len++] = s[i];
    }
    if(!buf_len)
        return cc_error(lexer, cpptok->loc, "Invalid number literal");
    // Detect float: contains '.', or 'e'/'E' (decimal), or 'p'/'P' (hex)
    _Bool is_float = 0;
    _Bool is_hex = (buf_len > 2 && buf[0] == '0' && (buf[1] == 'x' || buf[1] == 'X'));
    for(size_t i = 0; i < buf_len; i++){
        if(buf[i] == '.'){
            is_float = 1;
            break;
        }
        if(!is_hex && (buf[i] == 'e' || buf[i] == 'E')){
            is_float = 1;
            break;
        }
        if(is_hex && (buf[i] == 'p' || buf[i] == 'P')){
            is_float = 1;
            break;
        }
    }
    if(is_float){
        if(has_u)
            return cc_error(lexer, cpptok->loc, "Invalid suffix: 'u' on floating-point literal");
        if(is_hex)
            return cc_error(lexer, cpptok->loc, "Hex floating-point literals not yet supported");
        CCConstantType ctype;
        if(has_f){
            ctype = CC_FLOAT;
            FloatResult fr = parse_float(buf, buf_len);
            if(fr.errored)
                return cc_error(lexer, cpptok->loc, "Invalid floating-point literal");
            *cctok = (CCToken){
                .constant = {
                    .type = CC_CONSTANT,
                    .ctype = ctype,
                    .float_value = fr.result,
                },
                .loc = cpptok->loc,
            };
        }
        else {
            DoubleResult dr = parse_double(buf, buf_len);
            if(dr.errored)
                return cc_error(lexer, cpptok->loc, "Invalid floating-point literal");
            if(num_l)
                ctype = CC_LONG_DOUBLE;
            else
                ctype = CC_DOUBLE;
            *cctok = (CCToken){
                .constant = {
                    .type = CC_CONSTANT,
                    .ctype = ctype,
                    .double_value = dr.result,
                },
                .loc = cpptok->loc,
            };
        }
        return 0;
    }
    // Integer
    uint64_t v = 0;
    if(is_hex){
        Uint64Result u = parse_hex(buf, buf_len);
        if(u.errored) return cc_error(lexer, cpptok->loc, "Invalid hex digit in number");
        v = u.result;
    }
    else if(buf_len > 2 && buf[0] == '0' && (buf[1] == 'b' || buf[1] == 'B')){
        Uint64Result u = parse_binary(buf, buf_len);
        if(u.errored) return cc_error(lexer, cpptok->loc, "Invalid binary digit in number");
        v = u.result;
    }
    else if(buf_len > 1 && buf[0] == '0'){
        Uint64Result u = parse_octal_inner(buf+1, buf_len-1);
        if(u.errored) return cc_error(lexer, cpptok->loc, "Invalid octal digit in number");
        v = u.result;
    }
    else{
        Uint64Result u = parse_uint64(buf, buf_len);
        if(u.errored) return cc_error(lexer, cpptok->loc, "Invalid digit in number");
        v = u.result;
    }
    CCConstantType ctype;
    if(has_u && num_l >= 2)      ctype = CC_UNSIGNED_LONG_LONG;
    else if(num_l >= 2)          ctype = CC_LONG_LONG;
    else if(has_u && num_l == 1) ctype = CC_UNSIGNED_LONG;
    else if(num_l == 1)          ctype = CC_LONG;
    else if(has_u)               ctype = CC_UNSIGNED;
    else                         ctype = CC_INT;
    *cctok = (CCToken){
        .constant = {
            .type = CC_CONSTANT,
            .ctype = ctype,
            .integer_value = v,
        },
        .loc = cpptok->loc,
    };
    return 0;
}
static
int
cpp_string_to_cc_tok(CcLexer* lexer, CPPToken* cpptok, CCToken* cctok){
    (void)lexer;
    const char* s = cpptok->txt.text;
    size_t len = cpptok->txt.length;
    // Find the opening quote
    const char* q = memchr(s, '"', len);
    if(!q || q == s + len - 1)
        return cc_error(lexer, cpptok->loc, "Invalid string literal");
    size_t prefix_len = (size_t)(q - s);
    CCStringType stype;
    if(prefix_len == 0)      stype = CC_STRING;
    else if(prefix_len == 1 && s[0] == 'L') stype = CC_LSTRING;
    else if(prefix_len == 1 && s[0] == 'u') stype = CC_uSTRING;
    else if(prefix_len == 1 && s[0] == 'U') stype = CC_USTRING;
    else if(prefix_len == 2 && s[0] == 'u' && s[1] == '8') stype = CC_U8STRING;
    else return cc_error(lexer, cpptok->loc, "Invalid string prefix");
    const char* content = q + 1;
    // len - prefix_len - 2: skip prefix, opening quote, closing quote
    uint32_t content_len = (uint32_t)(len - prefix_len - 2);
    *cctok = (CCToken){
        .str = {
            .type = CC_STRING_LITERAL,
            .stype = stype,
            .length = content_len,
            .text = content,
        },
        .loc = cpptok->loc,
    };
    return 0;
}
static
int
cpp_char_to_cc_tok(CcLexer* lexer, CPPToken* cpptok, CCToken* cctok){
    const char* s = cpptok->txt.text;
    size_t len = cpptok->txt.length;
    // Skip prefix (L, u, U, u8) to find opening quote, track type
    const char* p = s;
    const char* end = s + len;
    CCConstantType ctype = CC_INT;
    if(p < end && *p == 'L'){ p++; ctype = CC_WCHAR; }
    else if(p < end && *p == 'U'){ p++; ctype = CC_CHAR32; }
    else if(p + 1 < end && p[0] == 'u' && p[1] == '8'){ p += 2; ctype = CC_UCHAR; }
    else if(p < end && *p == 'u'){ p++; ctype = CC_CHAR16; }
    if(p >= end || *p != '\'')
        return cc_error(lexer, cpptok->loc, "Invalid character constant");
    p++; // skip opening quote
    const char* e = end - 1; // closing quote
    if(e <= p || *e != '\'')
        return cc_error(lexer, cpptok->loc, "Invalid character constant");
    int64_t v = 0;
    int nchars = 0;
    while(p < e){
        nchars++;
        unsigned char c;
        if(*p == '\\'){
            p++;
            if(p == e)
                return cc_error(lexer, cpptok->loc, "Invalid escape in character constant");
            switch(*p){
                case 'n':  c = '\n'; p++; break;
                case 't':  c = '\t'; p++; break;
                case 'r':  c = '\r'; p++; break;
                case '\\': c = '\\'; p++; break;
                case '\'': c = '\''; p++; break;
                case '"':  c = '"';  p++; break;
                case 'a':  c = '\a'; p++; break;
                case 'b':  c = '\b'; p++; break;
                case 'f':  c = '\f'; p++; break;
                case 'v':  c = '\v'; p++; break;
                case '0': case '1': case '2': case '3':
                case '4': case '5': case '6': case '7':
                    c = 0;
                    for(int i = 0; i < 3 && p < e && *p >= '0' && *p <= '7'; i++, p++)
                        c = (unsigned char)((c << 3) | (*p - '0'));
                    break;
                case 'x':
                    p++;
                    c = 0;
                    while(p < e){
                        if(*p >= '0' && *p <= '9')      c = (unsigned char)((c << 4) | (*p - '0'));
                        else if(*p >= 'a' && *p <= 'f') c = (unsigned char)((c << 4) | (*p - 'a' + 10));
                        else if(*p >= 'A' && *p <= 'F') c = (unsigned char)((c << 4) | (*p - 'A' + 10));
                        else break;
                        p++;
                    }
                    break;
                case 'u': {
                    p++;
                    uint32_t uval = 0;
                    for(int i = 0; i < 4 && p < e; i++, p++){
                        if(*p >= '0' && *p <= '9')      uval = (uval << 4) | (uint32_t)(*p - '0');
                        else if(*p >= 'a' && *p <= 'f') uval = (uval << 4) | (uint32_t)(*p - 'a' + 10);
                        else if(*p >= 'A' && *p <= 'F') uval = (uval << 4) | (uint32_t)(*p - 'A' + 10);
                        else return cc_error(lexer, cpptok->loc, "Invalid \\u escape");
                    }
                    v = (v << 16) | uval;
                    continue;
                }
                case 'U': {
                    p++;
                    uint32_t uval = 0;
                    for(int i = 0; i < 8 && p < e; i++, p++){
                        if(*p >= '0' && *p <= '9')      uval = (uval << 4) | (uint32_t)(*p - '0');
                        else if(*p >= 'a' && *p <= 'f') uval = (uval << 4) | (uint32_t)(*p - 'a' + 10);
                        else if(*p >= 'A' && *p <= 'F') uval = (uval << 4) | (uint32_t)(*p - 'A' + 10);
                        else return cc_error(lexer, cpptok->loc, "Invalid \\U escape");
                    }
                    v = (v << 32) | uval;
                    continue;
                }
                default:
                    c = (unsigned char)*p; p++; break;
            }
        }
        else
            c = (unsigned char)*p++;
        v = (v << 8) | c;
    }
    if(ctype != CC_INT && nchars != 1)
        return cc_error(lexer, cpptok->loc, "Multi-character character constant with prefix is not allowed");
    *cctok = (CCToken){
        .constant = {
            .type = CC_CONSTANT,
            .ctype = ctype,
            .integer_value = (uint64_t)v,
        },
        .loc = cpptok->loc,
    };
    return 0;
}
static
int
cpp_punct_to_cc_tok(CcLexer* lexer, CPPToken* cpptok, CCToken* cctok){
    uint32_t p = (uint32_t)cpptok->punct;
    #ifdef __GNUC__
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wmultichar"
    #endif
    if(p == '#' || p == '##')
        return cc_error(lexer, cpptok->loc, "Stray '%s' in program", p == '#' ? "#" : "##");
    #ifdef __GNUC__
    #pragma GCC diagnostic pop
    #endif
    *cctok = (CCToken){
        .punct = {
            .type = CC_PUNCTUATOR,
            .punct = (CCPunct)p,
        },
        .loc = cpptok->loc,
    };
    return 0;
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
