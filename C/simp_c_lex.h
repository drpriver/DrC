#ifndef SIMPLE_C_LEX_H
#define SIMPLE_C_LEX_H
#include "../../Drp/stringview.h"
#include "../../Drp/typed_enum.h"
#ifdef __clang__
#pragma clang assume_nonnull begin
#endif

#ifndef warn_unused
#if defined(__GNUC__) || defined(__clang__)
#define warn_unused __attribute__((warn_unused_result))
#elif defined(_MSC_VER)
#define warn_unused
#else
#define warn_unused
#endif
#endif

#ifndef SIMP_C_force_inline
#if defined(__GNUC__) || defined(__clang__)
#define SIMP_C_force_inline static inline __attribute__((always_inline))
#elif defined(_MSC_VER)
#define SIMP_C_force_inline static inline __forceinline
#else
#define SIMP_C_force_inline static inline
#endif
#endif

#ifndef SIMP_C_LEX_API
#define SIMP_C_LEX_API static
#endif

enum CTokenType TYPED_ENUM(uint32_t) {
    CTOK_INVALID,
    // as per spec
    CTOK_KEYWORD,
    CTOK_IDENTIFIER,
    CTOK_CONSTANT,
    CTOK_STRING_LITERAL,
    CTOK_PUNCTUATOR,

    // extra tokens
    CTOK_PREPROC,
    CTOK_COMMENT,
    CTOK_EOF,
    CTOK_WHITESPACE,
};
TYPEDEF_ENUM(CTokenType, uint32_t);

typedef struct CToken CToken;
struct CToken {
    CTokenType type:8;
    unsigned subtype:24;
    StringView content;
};

enum CKeyword {
    CKW_NONE     = 0,
    CKW_do,
    CKW_if,
    CKW_for,
    CKW_int,
    CKW_true,
    CKW_long,
    CKW_char,
    CKW_auto,
    CKW_bool,
    CKW_else,
    CKW_enum,
    CKW_case,
    CKW_goto,
    CKW_void,
    CKW_false,
    CKW_break,
    CKW_float,
    CKW_const,
    CKW_short,
    CKW_union,
    CKW_while,
    CKW_double,
    CKW_extern,
    CKW_inline,
    CKW_return,
    CKW_signed,
    CKW_sizeof,
    CKW_static,
    CKW_struct,
    CKW_switch,
    CKW_typeof,
    CKW_alignas,
    CKW_alignof,
    CKW_default,
    CKW_typedef,
    CKW_nullptr,
    CKW__Atomic,
    CKW__BitInt,
    CKW__Complex,
    CKW_continue,
    CKW_register,
    CKW_restrict,
    CKW_unsigned,
    CKW_volatile,
    CKW__Generic,
    CKW__Float16,
    CKW__Float32,
    CKW__Float64,
    CKW_constexpr,
    CKW__Float128,
    CKW__Imaginary,
    CKW__Noreturn,
    CKW__Decimal32,
    CKW__Decimal64,
    CKW__Decimal128,
    CKW_thread_local,
    CKW_static_assert,
    CKW_typeof_unqual,
};
typedef enum CKeyword CKeyword;

SIMP_C_LEX_API
CKeyword
simp_c_is_keyword(StringView txt);

#ifdef _WIN32
#undef CC_NONE
#undef CP_NONE
#endif
enum CConstant {
    CC_NONE = 0,
    CC_INTEGER = 1,
    CC_FLOATING = 2,
    CC_ENUMERATION = 3,
    CC_CHARACTER = 4,
    CC_PREDEFINED = 5,
};
typedef enum CConstant CConstant;
#if 0
[ ] ( ) { } . ->
++  --  &  *  +  -  ~  !
/  %  <<  >>  <  >  <=  >=  ==  !=  ^  |  &&  ||
?   :   ::   ;  ...
=  *=  /=  %=  +=  -=  <<=  >>=  &=  ^=  |=
,  #  ##
<:   :>  <%  %>  %:   %:%:
#endif
#define simp_mb_char2(a, b) ((b<<8)|(a))
#define simp_mb_char3(a, b, c) ((c<<16)|(b<<8)|(a))
enum CPunct {
    CP_NONE = 0,
    CP_lbracket = '[',
    CP_rbracket = ']',
    CP_lparen = '(',
    CP_rparen = ')',
    CP_lbrace = '{',
    CP_rbrace = '}',
    CP_dot = '.',
    CP_amp = '&',
    CP_star = '*',
    CP_plus = '+',
    CP_minus = '-',
    CP_tilde = '~',
    CP_bang = '!',
    CP_slash = '/',
    CP_percent = '%',
    CP_lt = '<',
    CP_gt = '>',
    CP_xor = '^',
    CP_pipe = '|',
    CP_question = '?',
    CP_colon = ':',
    CP_semi = ';',
    CP_assign = '=',
    CP_comma = ',',
    CP_pound = '#',

    CP_arrow          = simp_mb_char2('-','>'),
    CP_plusplus       = simp_mb_char2('+','+'),
    CP_minusminus     = simp_mb_char2('-','-'),
    CP_lshift         = simp_mb_char2('<','<'),
    CP_rshift         = simp_mb_char2('>','>'),
    CP_le             = simp_mb_char2('<','='),
    CP_ge             = simp_mb_char2('>','='),
    CP_eq             = simp_mb_char2('=','='),
    CP_ne             = simp_mb_char2('!','='),
    CP_and            = simp_mb_char2('&','&'),
    CP_or             = simp_mb_char2('|','|'),
    CP_double_colon   = simp_mb_char2(':',':'),
    CP_ellipsis       = simp_mb_char3('.','.','.'),
    CP_star_assign    = simp_mb_char2('*','='),
    CP_slash_assign   = simp_mb_char2('/','='),
    CP_percent_assign = simp_mb_char2('%','='),
    CP_plus_assign    = simp_mb_char2('+','='),
    CP_minus_assign   = simp_mb_char2('-','='),
    CP_lshift_assign  = simp_mb_char3('<','<','='),
    CP_rshift_assign  = simp_mb_char3('>','>','='),
    CP_amp_assign     = simp_mb_char2('&','='),
    CP_xor_assign     = simp_mb_char2('^','='),
    CP_pipe_assign    = simp_mb_char2('|','='),
    CP_pound_pound    = simp_mb_char2('#','#'),
};
typedef enum CPunct CPunct;


// TODO: define values
enum CBasicType {
    CBT_NONE     = 0,
    CBT_INT,
    CBT_CHAR,
    CBT_LONG,
    CBT_AUTO,
    CBT_VOID,
    CBT_BOOL,
    CBT_FLOAT,
    CBT_SHORT,
    CBT_SIGNED,
    CBT_DOUBLE,
    CBT_UNSIGNED,
    CBT_DECIMAL32,
    CBT_DECIMAL64,
    CBT_DECIMAL128,
    CBT_FLOAT16,
    CBT_FLOAT32,
    CBT_FLOAT64,
    CBT_FLOAT128,
};

typedef enum CBasicType CBasicType;

SIMP_C_LEX_API
CBasicType
simp_c_is_basic_type(StringView txt);

typedef struct CLoc CLoc;
struct CLoc {
    StringView file;
    int row, column;
};

#ifdef __clang__
#pragma clang assume_nonnull end
#endif

#define MARRAY_T CToken
#include "../../Drp/Marray.h"


#ifdef __clang__
#pragma clang assume_nonnull begin
#endif

typedef struct CTokens CTokens;
struct CTokens {
    CToken* data;
    size_t count;
};

SIMP_C_force_inline
CTokens
ctokens(const Marray(CToken)* a){
    return (CTokens){a->data, a->count};
}

SIMP_C_LEX_API
warn_unused
int
simp_c_lex(size_t txtlen, const char* txt, Allocator a, Marray(CToken)* outtokens);

#ifdef __clang__
#pragma clang assume_nonnull end
#endif
#endif
