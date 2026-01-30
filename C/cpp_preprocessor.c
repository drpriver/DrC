#ifndef C_CPP_PREPROCESSOR_C
#define C_CPP_PREPROCESSOR_C
//
// Copyright © 2026-2026, David Priver <david@davidpriver.com>
//
#include "cpp_preprocessor.h"
#ifdef __clang__
#pragma clang assume_nonnull begin
#endif


// Internal APIs
static int cpp_next_raw_token(CPreprocessor*, CPPToken*);
static int cpp_next_pp_token(CPreprocessor*, CPPToken*);

static
int
cpp_define_macro(CPreprocessor* cpp, StringView name, size_t ntoks, size_t nparams, CMacro*_Nullable*_Nonnull outmacro){
    Atom key = AT_atomize(cpp->at, name.text, name.length);
    if(!key) return 1;
    CMacro* macro = AM_get(&cpp->macros, key);
    if(macro) return 1;
    size_t size = sizeof *macro + sizeof(CPPToken)*ntoks + sizeof(Atom)*nparams;
    macro = Allocator_zalloc(cpp->allocator, size);
    if(!macro) return 1;
    macro->nreplace = ntoks;
    macro->nparams = nparams;
    int err = AM_put(&cpp->macros, cpp->allocator, key, macro);
    if(err) Allocator_free(cpp->allocator, macro, size);
    else *outmacro = macro;
    return err;
}

static
int
cpp_undef_macro(CPreprocessor* cpp, StringView name){
    Atom key = AT_atomize(cpp->at, name.text, name.length);
    if(!key) return 1;
    CMacro* macro = AM_get(&cpp->macros, key);
    if(!macro) return 0;
    size_t size = sizeof *macro + sizeof(CPPToken)*macro->nreplace + sizeof(Atom)*macro->nparams;
    Allocator_free(cpp->allocator, macro, size);
    int err = AM_put(&cpp->macros, cpp->allocator, key, NULL);
    if(err) return err;
    return 0;
}

static
int
cpp_define_obj_macro(CPreprocessor* cpp, StringView name, CPPToken* toks, size_t ntoks){
    CMacro* macro;
    int err = cpp_define_macro(cpp, name, ntoks, 0, &macro);
    if(err) return err;
    if(ntoks) memcpy(pp_cmacro_replacement(macro), toks, ntoks * sizeof *toks);
    return 0;
}

static
_Bool
cpp_has_include(CPreprocessor* cpp, _Bool quote, StringView header_name){
    MStringBuilder* sb = fc_path_builder(cpp->fc);
    for(size_t i = quote?0:1; i < arrlen(cpp->include_paths); i++){
        Marray(StringView)* dirs = &cpp->include_paths[i];
        MARRAY_FOR_EACH_VALUE(StringView, d, *dirs){
            msb_reset(sb);
            if(!d.length) continue;
            msb_write_str(sb, d.text, d.length);
            if(msb_peek(sb) != '/')
                msb_write_char(sb, '/');
            msb_write_str(sb, header_name.text, header_name.length);
            if(fc_is_file(cpp->fc)){
                return 1;
            }
        }
    }
    return 0;
}

static
int
cpp_next_token(CPreprocessor* cpp, CPPToken* tok){
    return cpp_next_pp_token(cpp, tok);
    return 0;
}

static
int
cpp_next_pp_token(CPreprocessor* cpp, CPPToken* tok){
    return cpp_next_raw_token(cpp, tok);
}


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

#ifndef CASE_0_9
#define CASE_0_9 '0': case '1': case '2': case '3': case '4': case '5': \
    case '6': case '7': case '8': case '9'
#endif

static inline
void
cpp_skip_(CPPFrame* f){
    StringView txt = f->txt;
    size_t c = f->cursor;
    // skip line continuations
    for(;c < txt.length && txt.text[c] == '\\';){
        if(c + 1 == txt.length){
            c += 1;
            break;
        }
        if(txt.text[c+1] == '\n'){
            c += 2;
            continue;
        }
        if(txt.text[c+1] == '\r'){
            if(c + 2 == txt.length){
                c += 2;
                break;
            }
            if(txt.text[c+2] == '\n'){
                c += 3;
                continue;
            }
            c += 2;
            continue;
        }
        break;
    }
    f->cursor = c;
}

static
inline
int
cpp_next_char(CPPFrame* f){
    cpp_skip_(f);
    StringView txt = f->txt;
    int result;
    if(f->cursor == txt.length)
        result = -1;
    else
        result = (int)(unsigned char)txt.text[f->cursor++];
    return result;
}

static
inline
int
cpp_peek_char(CPPFrame* f){
    cpp_skip_(f);
    StringView txt = f->txt;
    int result;
    if(f->cursor == txt.length)
        result = -1;
    else
        result = (int)(unsigned char)txt.text[f->cursor]; // don't advance
    return result;
}

static
inline
_Bool
cpp_match_char(CPPFrame* f, int ch){
    if(cpp_peek_char(f) == ch){
        f->cursor++;
        return 1;
    }
    return 0;
}

static
inline
_Bool
cpp_match_2char(CPPFrame* f, int ch1, int ch2){
    size_t backup = f->cursor;
    if(cpp_match_char(f, ch1) && cpp_match_char(f, ch2)){
        return 1;
    }
    f->cursor = backup;
    return 0;
}

static
inline
_Bool
cpp_match_oneof(CPPFrame* f, const char* set){
    int ch = cpp_peek_char(f);
    for(;*set;set++)
        if(ch == *set){
            f->cursor++;
            return 1;
        }
    return 0;
}

static
int
cpp_next_raw_token(CPreprocessor* cpp, CPPToken* tok){
    if(cpp->token_buff.count){
        *tok = cpp->token_buff.data[0];
        ma_remove_at(CPPToken)(&cpp->token_buff, 0);
        return 0;
    }
    again:;
    if(!cpp->frames.count){
        *tok = (CPPToken){.type = CPP_EOF};
        return 0;
    }
    CPPFrame* f = &ma_tail(cpp->frames);
    size_t start = f->cursor;
    int c = cpp_next_char(f);
    switch(c){
        default:
            default_:
            *tok = (CPPToken){.type = CPP_OTHER, .txt = {1, &f->txt.text[start]}};
            return 0;
        case -1:{
            cpp->frames.count--;
            goto again;
        }
        case '\n':
            *tok = (CPPToken){.type = CPP_NEWLINE, .txt = {1, &f->txt.text[start]}};
            return 0;
        // Whitespace
        case ' ': case '\t': case '\r': case '\f': case '\v':
            while(cpp_match_oneof(f, " \t\r\f\v"))
                ;
            *tok = (CPPToken){.type = CPP_WHITESPACE, .txt = {f->cursor-start, &f->txt.text[start]}};
            return 0;
        case 0xEF: { // Check for utf-8 bom
            if(cpp_match_2char(f, 0xBB, 0xBF))
                goto again;
            goto default_;
        }
        // identifier
        case CASE_a_z:
        case CASE_A_Z:
        case '_':
            if(c == 'L' || c == 'U'){
                c = cpp_peek_char(f);
                if(c == '"' || c == '\''){
                    f->cursor++;
                    goto string_or_char;
                }
            }
            else if(c == 'u'){
                c = cpp_next_char(f);
                if(c == '"' || c == '\''){
                    f->cursor++;
                    goto string_or_char;
                }
                if(c == '8'){
                    c = cpp_peek_char(f);
                    if(c == '"' || c == '\''){
                        f->cursor++;
                        goto string_or_char;
                    }
                }
            }
            for(;;){
                c = cpp_peek_char(f);
                switch(c){
                    case CASE_a_z:
                    case CASE_A_Z:
                    case CASE_0_9:
                    case '_':
                        f->cursor++;
                        continue;
                    default:
                        break;
                }
                break;
            }
            *tok = (CPPToken){.type = CPP_IDENTIFIER, .txt = {f->cursor-start, &f->txt.text[start]}};
            return 0;
        // Number (pp-number)
        case CASE_0_9:
            pp_number:
            // pp-number: digit | . digit | pp-number (digit|.|e±|E±|p±|P±|identifier-char)
            for(;;){
                c = cpp_peek_char(f);
                switch(c){
                    case CASE_0_9:
                    case CASE_a_z:
                    case CASE_A_Z:
                    case '_':
                    case '\'':
                    case '.':
                        f->cursor++;
                        if(c == 'e' || c == 'E' || c == 'p' || c == 'P'){
                            cpp_match_oneof(f, "+-");
                            continue;
                        }
                        if(c == '\''){
                            c = cpp_peek_char(f);
                            switch(c){
                                case CASE_0_9:
                                case CASE_a_z:
                                case CASE_A_Z:
                                case '_':
                                    f->cursor++;
                                    continue;
                                default:
                                    goto break_;
                            }
                        }
                        continue;
                    default:
                        break;
                }
                break_:
                break;
            }
            *tok = (CPPToken){.type = CPP_NUMBER, .txt = {f->cursor-start, &f->txt.text[start]}};
            return 0;
        case '.':{
            c = cpp_peek_char(f);
            if(c >= '0' && c <= '9'){
                f->cursor++;
                goto pp_number;
            }
            if(cpp_match_2char(f, '.', '.')){
                *tok = (CPPToken){.type = CPP_PUNCTUATOR, .txt = SV("...")};
                return 0;
            }
            *tok = (CPPToken){.type = CPP_PUNCTUATOR, .txt = SV(".")};
            return 0;
        }
        // String / character literal
        case '"':
        case '\'':
        string_or_char:{
            int terminator = (c == '"') ? '"' : '\'';
            CPPTokenType type = (terminator == '"') ? CPP_STRING : CPP_CHAR;
            _Bool backslash = 0;
            for(;;){
                c = cpp_next_char(f);
                if(c == '\n' || c == -1){
                    // FIXME: report location
                    log_error(cpp->logger, "Unterminated %s literal", (terminator == '"')?"string":"character");
                    return 1;
                }
                if(c == '\\')
                    backslash = !backslash;
                else if(c == terminator && !backslash)
                    break;
                else
                    backslash = 0;
            }
            *tok = (CPPToken){.type = type, .txt = {f->cursor-start, &f->txt.text[start]}};
            return 0;
        }
        // Comments
        case '/':{
            if(cpp_match_char(f, '/')){ // C++ comment
                for(;;){
                    c = cpp_peek_char(f);
                    if(c == -1 || c == '\n')
                        break;
                    f->cursor++;
                }
                *tok = (CPPToken){.type = CPP_WHITESPACE, .txt = {f->cursor-start, &f->txt.text[start]}};
                return 0;
            }
            else if(cpp_match_char(f, '*')){ // C comment
                for(;;){
                    c = cpp_next_char(f);
                    if(c == -1){
                        log_error(cpp->logger, "Unterminated comment");
                        return 1;
                    }
                    if(c == '*' && cpp_match_char(f, '/')){
                        *tok = (CPPToken){.type = CPP_WHITESPACE, .txt = {f->cursor-start, &f->txt.text[start]}};
                        return 0;
                    }
                }
            }
            *tok = (CPPToken){.type = CPP_PUNCTUATOR, .txt = cpp_match_char(f, '=')?SV("/="):SV("/")};
            return 0;
        }
        case '#':{
            *tok = (CPPToken){.type = CPP_PUNCTUATOR, .txt = cpp_match_char(f, '#')?SV("##"):SV("#")};
            return 0;
        }
        case '*':
            *tok = (CPPToken){.type = CPP_PUNCTUATOR, .txt = cpp_match_char(f, '=')?SV("*="):SV("*")};
            return 0;
        case '~':
            *tok = (CPPToken){.type = CPP_PUNCTUATOR, .txt = SV("~")};
            return 0;
        case '!':
            *tok = (CPPToken){.type = CPP_PUNCTUATOR, .txt = cpp_match_char(f, '=')?SV("!="):SV("!")};
            return 0;
        case '%':
            if(cpp_match_char(f, ':')){
                // %:%:
                if(cpp_match_2char(f, '%', ':')){
                    *tok = (CPPToken){.type = CPP_PUNCTUATOR, .txt = SV("##")};
                    return 0;
                }
                *tok = (CPPToken){.type = CPP_PUNCTUATOR, .txt = SV("#")};
                return 0;
            }
            if(cpp_match_char(f, '>')){
                *tok = (CPPToken){.type = CPP_PUNCTUATOR, .txt = SV("}")};
                return 0;
            }
            *tok = (CPPToken){.type = CPP_PUNCTUATOR, .txt = cpp_match_char(f, '=')?SV("%="):SV("%")};
            return 0;
        case '^':
            *tok = (CPPToken){.type = CPP_PUNCTUATOR, .txt = cpp_match_char(f, '=')?SV("^="):SV("^")};
            return 0;
        case '=':
            *tok = (CPPToken){.type = CPP_PUNCTUATOR, .txt = cpp_match_char(f, '=')?SV("=="):SV("=")};
            return 0;
        case '-':
            if(cpp_match_char(f, '>')){
                *tok = (CPPToken){.type = CPP_PUNCTUATOR, .txt = SV("->")};
                return 0;
            }
            if(cpp_match_char(f, '-')){
                *tok = (CPPToken){.type = CPP_PUNCTUATOR, .txt = SV("--")};
                return 0;
            }
            *tok = (CPPToken){.type = CPP_PUNCTUATOR, .txt = cpp_match_char(f, '=')?SV("-="):SV("-")};
            return 0;
        case '+':
            if(cpp_match_char(f, '+')){
                *tok = (CPPToken){.type = CPP_PUNCTUATOR, .txt = SV("++")};
                return 0;
            }
            *tok = (CPPToken){.type = CPP_PUNCTUATOR, .txt = cpp_match_char(f, '=')?SV("+="):SV("+")};
            return 0;
        case '<':
            if(cpp_match_char(f, ':')){
                *tok = (CPPToken){.type = CPP_PUNCTUATOR, .txt = SV("[")};
                return 0;
            }
            if(cpp_match_char(f, '%')){
                *tok = (CPPToken){.type = CPP_PUNCTUATOR, .txt = SV("{")};
                return 0;
            }
            if(cpp_match_char(f, '<')){
                *tok = (CPPToken){.type = CPP_PUNCTUATOR, .txt = cpp_match_char(f, '=')?SV("<<="):SV("<<")};
                return 0;
            }
            *tok = (CPPToken){.type = CPP_PUNCTUATOR, .txt = cpp_match_char(f, '=')?SV("<="):SV("<")};
            return 0;
        case '>':
            if(cpp_match_char(f, '>')){
                *tok = (CPPToken){.type = CPP_PUNCTUATOR, .txt = cpp_match_char(f, '=')?SV(">>="):SV(">>")};
                return 0;
            }
            *tok = (CPPToken){.type = CPP_PUNCTUATOR, .txt = cpp_match_char(f, '=')?SV(">="):SV(">")};
            return 0;
        case '&':
            if(cpp_match_char(f, '&')){
                *tok = (CPPToken){.type = CPP_PUNCTUATOR, .txt = SV("&&")};
                return 0;
            }
            *tok = (CPPToken){.type = CPP_PUNCTUATOR, .txt = cpp_match_char(f, '=')?SV("&="):SV("&")};
            return 0;
        case '|':
            if(cpp_match_char(f, '|')){
                *tok = (CPPToken){.type = CPP_PUNCTUATOR, .txt = SV("||")};
                return 0;
            }
            *tok = (CPPToken){.type = CPP_PUNCTUATOR, .txt = cpp_match_char(f, '=')?SV("|="):SV("|")};
            return 0;
        case ':':
            if(cpp_match_char(f, '>')){
                *tok = (CPPToken){.type = CPP_PUNCTUATOR, .txt = SV("]")};
                return 0;
            }
            *tok = (CPPToken){.type = CPP_PUNCTUATOR, .txt = cpp_match_char(f, ':')?SV("::"):SV(":")};
            return 0;
        case '(':
            *tok = (CPPToken){.type = CPP_PUNCTUATOR, .txt = SV("(")};
            return 0;
        case ')':
            *tok = (CPPToken){.type = CPP_PUNCTUATOR, .txt = SV(")")};
            return 0;
        case '[':
            *tok = (CPPToken){.type = CPP_PUNCTUATOR, .txt = SV("[")};
            return 0;
        case ']':
            *tok = (CPPToken){.type = CPP_PUNCTUATOR, .txt = SV("]")};
            return 0;
        case '{':
            *tok = (CPPToken){.type = CPP_PUNCTUATOR, .txt = SV("{")};
            return 0;
        case '}':
            *tok = (CPPToken){.type = CPP_PUNCTUATOR, .txt = SV("}")};
            return 0;
        case '?':
            *tok = (CPPToken){.type = CPP_PUNCTUATOR, .txt = SV("?")};
            return 0;
        case ';':
            *tok = (CPPToken){.type = CPP_PUNCTUATOR, .txt = SV(";")};
            return 0;
        case ',':
            *tok = (CPPToken){.type = CPP_PUNCTUATOR, .txt = SV(",")};
            return 0;
    }
}

#ifdef __clang__
#pragma clang assume_nonnull end
#endif
#endif
