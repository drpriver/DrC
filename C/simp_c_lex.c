#ifndef SIMPLE_C_LEX_C
#define SIMPLE_C_LEX_C
// #include <stdio.h>
#include "simp_c_lex.h"
#ifdef __clang__
#pragma clang assume_nonnull begin
#endif

#ifndef CASE_0_9
#define CASE_0_9 '0': case '1': case '2': case '3': case '4': case '5': \
    case '6': case '7': case '8': case '9'
#endif

#ifndef CASE_a_z
#define CASE_a_z 'a': case 'b': case 'c': case 'd': case 'e': case 'f': \
    case 'g': case 'h': case 'i': case 'j': case 'k': case 'l': case 'm': \
    case 'n': case 'o': case 'p': case 'q': case 'r': case 's': case 't': \
    case 'u': case 'v': case 'w': case 'x': case 'y': case 'z'
#endif

#ifndef CASE_A_Z
#define CASE_A_Z 'A': case 'B': case 'C': case 'D': case 'E': case 'F': \
    case 'G': case 'H': case 'I': case 'J': case 'K': case 'L': case 'M': \
    case 'N': case 'O': case 'P': case 'Q': case 'R': case 'S': case 'T': \
    case 'U': case 'V': case 'W': case 'X': case 'Y': case 'Z'
#endif

#ifndef CASE_A_F
#define CASE_A_F 'A': case 'B': case 'C': case 'D': case 'E': case 'F'
#endif

#ifndef CASE_a_f
#define CASE_a_f 'a': case 'b': case 'c': case 'd': case 'e': case 'f'
#endif

#ifndef CASE_WS
#define CASE_WS ' ': case '\t': case '\r': case '\n'
#endif

#ifndef FALLTHROUGH
#if defined(__GNUC__) || defined(__clang__)
#define FALLTHROUGH __attribute__((fallthrough))
#else
#define FALLTHROUGH
#endif
#endif

SIMP_C_LEX_API
warn_unused
int
simp_c_lex(size_t txtlen, const char* txt_, Allocator a, Marray(CToken)* outtokens){
    const unsigned char* txt = (const unsigned char*)txt_;
    #define ERROR(msg, ...) 1
    // #define ERROR(msg, ...) fprintf(stderr, msg "\n", ##__VA_ARGS__), 1
    enum State {
        NIL = 0,
        WS = 1,
        STRING = 2,
        CHAR = 3,
        BEGIN_NUMBER = 4,
        NUMBER_SUFFIX = 5,
        POST_DOT = 6,
        POST_EXP = 7,
        HEX = 8,
        BIN = 9,
        OCTAL = 10,
        IDENT = 11,
        PREPROC = 12,
        CPP_COMMENT = 13,
        C_COMMENT = 14,
    };
    enum State s = NIL;
    const unsigned char* tok_begin = txt;
    const unsigned char* current = txt;
    const unsigned char* const end = txt + txtlen;
    int advance = 0;
    unsigned punct;
    #define EMIT(t, st) do { \
        int err = ma_push(CToken)(outtokens, a, (CToken){t, st, {current-tok_begin, (const char*)tok_begin}}); \
        if(err) return err; \
    } while(0)
    for(;;){
        Continue:;
        current += advance;
        int c = current < end? current[0] : -1;
        int peek = current + 1 < end? current[1] : -1;
        int peek2 = current + 2 < end? current[2] : -1;
        switch(s){
            case NIL:
                nil_dispatch:;
                tok_begin = current;
                advance = 1;
                switch(c){
                    default:
                        current++;
                        EMIT(CTOK_INVALID, 0);
                        s = NIL;
                        advance = 0;
                        goto Continue;
                        return ERROR("Unhandled character: %d", c);
                    case -1:
                        goto done;
                    case '.':
                        switch(peek){
                            case CASE_0_9:
                                s = POST_DOT;
                                goto Continue;
                            case '.':
                                if(peek2 == '.'){
                                    current += 3;
                                    EMIT(CTOK_PUNCTUATOR, CP_ellipsis);
                                    s = NIL;
                                    advance = 0;
                                    goto Continue;
                                }
                                FALLTHROUGH;
                            default:
                                current++;
                                EMIT(CTOK_PUNCTUATOR, CP_dot);
                                s = NIL;
                                advance = 0;
                                goto Continue;
                        }
                    case CASE_0_9:
                        if(c == '0'){
                            if((peek | 0x20) == 'x'){
                                advance = 2;
                                s = HEX;
                                goto Continue;
                            }
                            if((peek | 0x20) == 'b'){
                                advance = 2;
                                s = BIN;
                                goto Continue;
                            }
                            if(peek == '.'){
                                advance = 2;
                                s = POST_DOT;
                                goto Continue;
                            }
                            s = OCTAL;
                            goto Continue;
                        }
                        s = BEGIN_NUMBER;
                        goto Continue;
                    case CASE_a_z:
                    case CASE_A_Z:
                    case '_':
                        if(c == 'u'){
                            if(peek == '8' && (peek2 == '"' || peek2 == '\'')){
                                s = peek2 == '"'?STRING:CHAR;
                                advance = 3;
                                goto Continue;
                            }
                            if(peek == '"' || peek == '\''){
                                s = peek == '"'?STRING:CHAR;
                                advance = 2;
                                goto Continue;
                            }
                        }
                        if((c == 'L' || c == 'U') && (peek == '"' || peek == '\'')){
                            s = peek == '"'?STRING:CHAR;
                            advance = 2;
                            goto Continue;
                        }
                        s = IDENT;
                        goto Continue;
                    case CASE_WS:
                        s = WS;
                        goto Continue;
                    case '"':
                        s = STRING;
                        goto Continue;
                    case '\'':
                        s = CHAR;
                        goto Continue;
                    case '#':
                        s = PREPROC;
                        goto Continue;
                    case '\\':
                        if(peek == '\n'){
                            s = WS;
                            advance = 2;
                            goto Continue;
                        }
                        if(peek == '\r' && peek2 == '\n'){
                            s = WS;
                            advance = 3;
                            goto Continue;
                        }
                        EMIT(CTOK_INVALID, 0);
                        s = NIL;
                        goto Continue;
                    case '/':
                        if(peek == '/'){
                            advance = 2;
                            s = CPP_COMMENT;
                            goto Continue;
                        }
                        if(peek == '*'){
                            advance = 2;
                            s = C_COMMENT;
                            goto Continue;
                        }
                        punct = '/';
                        if(peek == '='){
                            current++;
                            punct = simp_mb_char2('/','=');
                        }
                        current++;
                        EMIT(CTOK_PUNCTUATOR, punct);
                        s = NIL;
                        advance = 0;
                        goto Continue;
                    case '-': case '+':
                    case '<': case '>':
                    case '&': case '|':
                        if(peek == c){
                            current+=2;
                            EMIT(CTOK_PUNCTUATOR, simp_mb_char2(c, peek));
                            s = NIL;
                            advance = 0;
                            goto Continue;
                        }
                        FALLTHROUGH;
                    case '*': case '~':
                    case '!': case '%': case '^':
                    case '=':
                        if(peek == '='){
                            current+=2;
                            EMIT(CTOK_PUNCTUATOR, simp_mb_char2(c, peek));
                            s = NIL;
                            advance = 0;
                            goto Continue;
                        }
                        if(c == '-' && peek == '>'){
                            current+=2;
                            EMIT(CTOK_PUNCTUATOR, simp_mb_char2(c, peek));
                            s = NIL;
                            advance = 0;
                            goto Continue;
                        }
                        current++;
                        EMIT(CTOK_PUNCTUATOR, c);
                        s = NIL;
                        advance = 0;
                        goto Continue;
                    case ':':
                        if(peek == ':'){
                            current+=2;
                            EMIT(CTOK_PUNCTUATOR, simp_mb_char2(':', ':'));
                            s = NIL;
                            advance = 0;
                            goto Continue;
                        }
                        FALLTHROUGH;
                    case '(': case ')':
                    case '[': case ']':
                    case '{': case '}':
                    case '?':
                    case ';': case ',':
                        current++;
                        EMIT(CTOK_PUNCTUATOR, c);
                        s = NIL;
                        advance = 0;
                        goto Continue;
                }
            case WS:
                switch(c){
                    case CASE_WS:
                        advance = 1;
                        goto Continue;
                    case '\\':
                        if(peek == '\n'){
                            advance = 2;
                            goto Continue;
                        }
                        if(peek == '\r' && peek2 == '\n'){
                            advance = 3;
                            goto Continue;
                        }
                        FALLTHROUGH;
                    default:
                        EMIT(CTOK_WHITESPACE, 0);
                        goto nil_dispatch;
                }
            case STRING:
                if(c == '\\' && peek != '\n' && !(peek == '\r' && peek2 == '\n')){
                    advance = 2;
                    goto Continue;
                }
                if(c == '"'){
                    current++;
                    EMIT(CTOK_STRING_LITERAL, 0);
                    advance = 0;
                    s = NIL;
                    goto Continue;
                }
                if(c == '\n' || (c == '\r' && peek == '\n')){
                    EMIT(CTOK_INVALID, 0);
                    advance = 0;
                    s = NIL;
                    goto Continue;
                }
                if(c == -1){
                    EMIT(CTOK_INVALID, 0);
                    advance = 0;
                    s = NIL;
                    goto Continue;
                }
                advance = 1;
                goto Continue;
            case CHAR:
                if(c == '\\' && peek != '\n' && !(peek == '\r' && peek2 == '\n')){
                    advance = 2;
                    goto Continue;
                }
                if(c == '\''){
                    current++;
                    EMIT(CTOK_CONSTANT, CC_CHARACTER);
                    advance = 0;
                    s = NIL;
                    goto Continue;
                }
                if(c == '\n' || (c == '\r' && peek == '\n')){
                    EMIT(CTOK_INVALID, 0);
                    advance = 0;
                    s = NIL;
                    goto Continue;
                }
                if(c == -1){
                    EMIT(CTOK_INVALID, 0);
                    advance = 0;
                    s = NIL;
                    goto Continue;
                }
                advance = 1;
                goto Continue;
            case BEGIN_NUMBER:
                switch(c){
                    case CASE_0_9:
                    case '\'': // weird c23 digit separator
                        advance = 1;
                        goto Continue;
                    case '.':
                        advance = 1;
                        s = POST_DOT;
                        goto Continue;
                    case 'e': case 'E':
                        if(peek == '-' || peek == '+')
                            advance = 2;
                        else
                            advance = 1;
                        s = POST_EXP;
                        goto Continue;
                    case 'l': case 'u': case 'L': case 'U':
                        advance = 1;
                        s = NUMBER_SUFFIX;
                        goto Continue;
                    // 0x and 0b are handled already
                    default:
                        // EMIT(CTOK_NUMBER);
                        EMIT(CTOK_CONSTANT, CC_INTEGER);
                        advance = 0;
                        s = NIL;
                        goto Continue;
                }
            case NUMBER_SUFFIX:
                switch(c){
                    case 'l': case 'L': case 'u': case 'U':
                        advance = 1;
                        goto Continue;
                    default:
                        // EMIT(CTOK_NUMBER);
                        EMIT(CTOK_CONSTANT, CC_INTEGER);
                        advance = 0;
                        s = NIL;
                        goto Continue;
                }
            case POST_DOT:
                switch(c){
                    case CASE_0_9:
                        advance = 1;
                        goto Continue;
                    case 'e': case 'E':
                        if(peek == '-' || peek == '+')
                            advance = 2;
                        else
                            advance = 1;
                        s = POST_EXP;
                        goto Continue;
                    case 'l': case 'L':
                    case 'f': case 'F':
                        current++;
                        FALLTHROUGH;
                    default:
                        EMIT(CTOK_CONSTANT, CC_FLOATING);
                        // EMIT(CTOK_FLOATING_NUMBER);
                        advance = 0;
                        s = NIL;
                        goto Continue;
                }
            case POST_EXP:
                switch(c){
                    case CASE_0_9:
                        advance = 1;
                        goto Continue;
                    case 'l': case 'L':
                    case 'f': case 'F':
                        current++;
                        FALLTHROUGH;
                    default:
                        // EMIT(CTOK_FLOATING_NUMBER);
                        EMIT(CTOK_CONSTANT, CC_FLOATING);
                        advance = 0;
                        s = NIL;
                        goto Continue;
                }
            case HEX:
                switch(c){
                    case '\'': // weird c23 digit separator
                        if(current == tok_begin + 2){
                            current++;
                            EMIT(CTOK_INVALID, 0);
                            advance = 0;
                            s = NIL;
                            goto Continue;
                        }
                        switch(peek){
                            case CASE_0_9:
                            case CASE_A_F:
                            case CASE_a_f:
                                advance = 2;
                                goto Continue;
                            default:
                                current++;
                                EMIT(CTOK_INVALID, 0);
                                advance = 0;
                                s = NIL;
                                goto Continue;
                        }
                    case CASE_0_9:
                    case CASE_A_F:
                    case CASE_a_f:
                        advance = 1;
                        goto Continue;
                    case 'l': case 'u': case 'L': case 'U':
                        advance = 1;
                        s = NUMBER_SUFFIX;
                        goto Continue;
                    default:
                        // EMIT(CTOK_NUMBER);
                        EMIT(CTOK_CONSTANT, CC_INTEGER);
                        advance = 0;
                        s = NIL;
                        goto Continue;
                }
            case BIN:
                switch(c){
                    case '\'': // weird c23 digit separator
                        if(current == tok_begin + 2){
                            current++;
                            EMIT(CTOK_INVALID, 0);
                            advance = 0;
                            s = NIL;
                            goto Continue;
                        }
                        switch(peek){
                            case '0': case '1':
                                advance = 2;
                                goto Continue;
                            default:
                                current++;
                                EMIT(CTOK_INVALID, 0);
                                advance = 0;
                                s = NIL;
                                goto Continue;
                        }
                    case '0': case '1':
                        advance = 1;
                        goto Continue;
                    case 'l': case 'u': case 'L': case 'U':
                        advance = 1;
                        s = NUMBER_SUFFIX;
                        goto Continue;
                    default:
                        // EMIT(CTOK_NUMBER);
                        EMIT(CTOK_CONSTANT, CC_INTEGER);
                        advance = 0;
                        s = NIL;
                        goto Continue;
                }
            case OCTAL:
                switch(c){
                    case '\'':
                        switch(peek){
                            case '0': case '1': case '2': case '3':
                            case '4': case '5': case '6': case '7':
                                advance = 2;
                                goto Continue;
                            default:
                                current++;
                                EMIT(CTOK_INVALID, 0);
                                advance = 0;
                                s = NIL;
                                goto Continue;
                        }
                    case '0': case '1': case '2': case '3':
                    case '4': case '5': case '6': case '7':
                        advance = 1;
                        goto Continue;
                    case 'l': case 'u': case 'L': case 'U':
                        advance = 1;
                        s = NUMBER_SUFFIX;
                        goto Continue;
                    default:
                        // EMIT(CTOK_NUMBER);
                        EMIT(CTOK_CONSTANT, CC_INTEGER);
                        advance = 0;
                        s = NIL;
                        goto Continue;
                }
            case IDENT:
                switch(c){
                    case CASE_a_z:
                    case CASE_A_Z:
                    case CASE_0_9:
                    case '_':
                        advance = 1;
                        goto Continue;
                    default:
                        {
                            CKeyword kw = simp_c_is_keyword((StringView){current-tok_begin, (const char*)tok_begin});
                            if(kw)
                                EMIT(CTOK_KEYWORD, kw);
                            else
                                EMIT(CTOK_IDENTIFIER, 0);
                        }
                        advance = 0;
                        s = NIL;
                        goto Continue;
                }
            case PREPROC:
                if(c == '\\' && peek == '\n'){
                    advance = 2;
                    goto Continue;
                }
                if(c == '\\' && peek == '\r' && peek2 == '\n'){
                    advance = 3;
                    goto Continue;
                }
                if(c == '\n' || c == -1 || (c == '\r' && peek == '\n')){
                    EMIT(CTOK_PREPROC, 0);
                    advance = 0;
                    s = NIL;
                    goto Continue;
                }
                advance = 1;
                goto Continue;
            case CPP_COMMENT:
                if(c == '\n' || c == -1 || (c == '\r' && peek == '\n')){
                    current++;
                    advance = 0;
                    s = NIL;
                    EMIT(CTOK_COMMENT, 0);
                    goto Continue;
                }
                advance = 1;
                goto Continue;
            case C_COMMENT:
                if(c == '*' && peek == '/'){
                    current += 2;
                    advance = 0;
                    s = NIL;
                    EMIT(CTOK_COMMENT, 0);
                    goto Continue;
                }
                if(c == -1){
                    current++;
                    EMIT(CTOK_INVALID, 0);
                    advance = 0;
                    s = NIL;
                    goto Continue;
                }
                advance = 1;
                goto Continue;
        }
    }

    done:;
    return 0;
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

#define CKWS(X) CKWS2(X) CKWS3(X) CKWS4(X) CKWS5(X) CKWS6(X) CKWS7(X) CKWS8(X) CKWS9(X) CKWS10(X) CKWS11(X) CKWS12(X) CKWS13(X)

SIMP_C_LEX_API
CKeyword
simp_c_is_keyword(StringView txt){
#define X(kw) if(sv_equals(txt, SV(#kw))) return CKW_##kw;
    switch(txt.length){
        case 2:
            CKWS2(X);
            return 0;
        case 3:
            CKWS3(X);
            return 0;
        case 4:
            CKWS4(X);
            return 0;
        case 5:
            CKWS5(X);
            if(sv_equals(txt, SV("_Bool"))) return CKW_bool;
            return 0;
        case 6:
            CKWS6(X);
            return 0;
        case 7:
            CKWS7(X);
            return 0;
        case 8:
            CKWS8(X);
            if(sv_equals(txt, SV("_Alignas"))) return CKW_alignas;
            if(sv_equals(txt, SV("_Alignof"))) return CKW_alignof;
            return 0;
        case 9:
            CKWS9(X);
            return 0;
        case 10:
            CKWS10(X);
            return 0;
        case 11:
            CKWS11(X);
            return 0;
        case 12:
            CKWS12(X);
            return 0;
        case 13:
            CKWS13(X)
            if(sv_equals(txt, SV("_Thread_local"))) return CKW_thread_local;
            return 0;
        case 14:
            if(sv_equals(txt, SV("_Static_assert"))) return CKW_static_assert;
            return 0;
        default:
            return 0;
    }
}
SIMP_C_LEX_API
CBasicType
simp_c_is_basic_type(StringView txt){
    switch(txt.length){
        case 2:
            return 0;
        case 3:
            if(sv_equals(txt, SV("int"))) return CBT_INT;
            return 0;
        case 4:
            if(sv_equals(txt, SV("char"))) return CBT_CHAR;
            if(sv_equals(txt, SV("long"))) return CBT_LONG;
            if(sv_equals(txt, SV("auto"))) return CBT_AUTO;
            if(sv_equals(txt, SV("void"))) return CBT_VOID;
            if(sv_equals(txt, SV("bool"))) return CBT_BOOL;
            return 0;
        case 5:
            if(sv_equals(txt, SV("_Bool"))) return CBT_BOOL;
            if(sv_equals(txt, SV("float"))) return CBT_FLOAT;
            if(sv_equals(txt, SV("short"))) return CBT_SHORT;
            return 0;
        case 6:
            if(sv_equals(txt, SV("signed"))) return CBT_SIGNED;
            if(sv_equals(txt, SV("double"))) return CBT_DOUBLE;
            return 0;
        case 7:
            return 0;
        case 8:
            if(sv_equals(txt, SV("unsigned"))) return CBT_UNSIGNED;
            // if(sv_equals(txt, SV("_Complex"))) return CBT_UNSIGNED;
            if(sv_equals(txt, SV("_Float16"))) return CBT_FLOAT16;
            if(sv_equals(txt, SV("_Float32"))) return CBT_FLOAT32;
            if(sv_equals(txt, SV("_Float64"))) return CBT_FLOAT64;
            return 0;
        case 9:
            if(sv_equals(txt, SV("_Float128"))) return CBT_FLOAT128;
            return 0;
        case 10:
            if(sv_equals(txt, SV("_Decimal32"))) return CBT_DECIMAL32;
            if(sv_equals(txt, SV("_Decimal64"))) return CBT_DECIMAL64;
            return 0;
        case 11:
            if(sv_equals(txt, SV("_Decimal128"))) return CBT_DECIMAL128;
            return 0;
        default:
            return 0;
    }
}

#ifdef __clang__
#pragma clang assume_nonnull end
#endif
#endif
