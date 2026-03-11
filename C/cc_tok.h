#ifndef C_CC_TOK_H
#define C_CC_TOK_H
//
// Copyright © 2026-2026, David Priver <david@davidpriver.com>
//
#include <stdint.h>
#include "srcloc.h"
#include "../Drp/typed_enum.h"
#include "../Drp/atom.h"

#ifdef __clang__
#pragma clang assume_nonnull begin
#endif

enum CcTokenType TYPED_ENUM(uint32_t) {
    CC_EOF,
    CC_KEYWORD,
    CC_IDENTIFIER,
    CC_CONSTANT,
    CC_STRING_LITERAL,
    CC_PUNCTUATOR,
};
TYPEDEF_ENUM(CcTokenType, uint32_t);

enum CcConstantType TYPED_ENUM(uint32_t){
    CC_INT,
    CC_UNSIGNED,
    CC_LONG,
    CC_UNSIGNED_LONG,
    CC_LONG_LONG,
    CC_UNSIGNED_LONG_LONG,
    CC_FLOAT,
    CC_DOUBLE,
    CC_LONG_DOUBLE,
    CC_WCHAR,
    CC_CHAR16,
    CC_CHAR32,
    CC_UCHAR,
};
TYPEDEF_ENUM(CcConstantType, uint32_t);
enum CcStringType TYPED_ENUM(uint32_t){
    CC_STRING,
    CC_LSTRING,
    CC_uSTRING,
    CC_USTRING,
    CC_U8STRING,
};
TYPEDEF_ENUM(CcStringType, uint32_t);

enum CcKeyword TYPED_ENUM(uint32_t){
    CC_do,
    CC_if,
    CC_for,
    CC_int,
    CC_asm,
    CC_true,
    CC_long,
    CC_char,
    CC_auto,
    CC_bool,
    CC_else,
    CC_enum,
    CC_case,
    CC_goto,
    CC_void,
    CC_false,
    CC_break,
    CC_float,
    CC_const,
    CC__Type,
    CC_short,
    CC_union,
    CC_while,
    CC_double,
    CC_extern,
    CC_inline,
    CC_return,
    CC_signed,
    CC_sizeof,
    CC_static,
    CC_struct,
    CC_switch,
    CC_typeof,
    CC_alignas,
    CC_alignof,
    CC_default,
    CC_typedef,
    CC_nullptr,
    CC__Atomic,
    CC__BitInt,
    CC__Complex,
    CC_continue,
    CC_register,
    CC_restrict,
    CC_unsigned,
    CC_volatile,
    CC__Generic,
    CC___int128,
    CC__Float16,
    CC__Float32,
    CC__Float64,
    CC_constexpr,
    CC__Float128,
    CC__Float32x,
    CC__Float64x,
    CC__Imaginary,
    CC__Noreturn,
    CC__Decimal32,
    CC__Decimal64,
    CC__Decimal128,
    CC___auto_type,
    CC_thread_local,
    CC_static_assert,
    CC_typeof_unqual,
    CC__Countof,
    CC___attribute__,
};
TYPEDEF_ENUM(CcKeyword, uint32_t);

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmultichar"
#endif
enum CcPunct TYPED_ENUM(uint32_t){
    CC_lbracket = '[',
    CC_rbracket = ']',
    CC_lparen   = '(',
    CC_rparen   = ')',
    CC_lbrace   = '{',
    CC_rbrace   = '}',
    CC_dot      = '.',
    CC_amp      = '&',
    CC_star     = '*',
    CC_plus     = '+',
    CC_minus    = '-',
    CC_tilde    = '~',
    CC_bang     = '!',
    CC_slash    = '/',
    CC_percent  = '%',
    CC_lt       = '<',
    CC_gt       = '>',
    CC_xor      = '^',
    CC_pipe     = '|',
    CC_question = '?',
    CC_colon    = ':',
    CC_semi     = ';',
    CC_assign   = '=',
    CC_comma    = ',',
    CC_arrow          = '->',
    CC_plusplus       = '++',
    CC_minusminus     = '--',
    CC_lshift         = '<<',
    CC_rshift         = '>>',
    CC_le             = '<=',
    CC_ge             = '>=',
    CC_eq             = '==',
    CC_ne             = '!=',
    CC_and            = '&&',
    CC_or             = '||',
    CC_double_colon   = '::',
    CC_ellipsis       = '...',
    CC_star_assign    = '*=',
    CC_slash_assign   = '/=',
    CC_percent_assign = '%=',
    CC_plus_assign    = '+=',
    CC_minus_assign   = '-=',
    CC_lshift_assign  = '<<=',
    CC_rshift_assign  = '>>=',
    CC_amp_assign     = '&=',
    CC_xor_assign     = '^=',
    CC_pipe_assign    = '|=',
};
#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif
TYPEDEF_ENUM(CcPunct, uint32_t);


typedef struct CcToken CcToken;
struct CcToken {
    union {
        struct {
            CcTokenType type: 8;
            uint32_t _bitpadding: 24;
            uint32_t _pad;
            uint64_t _pad2;
        };
        struct {
            CcTokenType type: 8;
            CcKeyword kw: 8;
            uint32_t _bitpadding: 16;
            uint32_t _pad;
            uint64_t _pad2;
        } kw;
        struct {
            CcTokenType type: 8;
            uint32_t _bitpadding: 24;
            uint32_t _pad;
            Atom ident;
        } ident;
        struct {
            CcTokenType type: 8;
            CcConstantType ctype: 8;
            uint32_t _bitpadding: 16;
            uint32_t _pad;
            union {
                uint64_t integer_value;
                float float_value;
                double double_value;
            };
        } constant;
        struct {
            CcTokenType type: 8;
            CcStringType stype: 8;
            uint32_t _bitpadding: 16;
            uint32_t length;
            union {
                const char* utf8;
                const unsigned short* utf16;
                const unsigned int* utf32;
            };
        } str;
        struct {
            CcTokenType type: 8;
            CcPunct punct: 24;
            uint32_t _pad;
            uint64_t _pad2;
        } punct;
    };
    SrcLoc loc;
};
#ifdef __clang__
#pragma clang assume_nonnull end
#endif

#endif
