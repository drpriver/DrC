#ifndef C_CPP_PREPROCESSOR_C
#define C_CPP_PREPROCESSOR_C
//
// Copyright © 2026-2026, David Priver <david@davidpriver.com>
//
#include <stdarg.h>
#include "cpp_preprocessor.h"
#include "cpp_tok.h"
#ifdef __clang__
#pragma clang assume_nonnull begin
#endif

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmultichar"
#endif


// Internal APIs
static int cpp_next_raw_token(CPreprocessor*, CPPToken*);
static int cpp_next_pp_token(CPreprocessor*, CPPToken*);
static int cpp_error(CPreprocessor*, SrcLoc, const char*, ...);
static void cpp_warn(CPreprocessor*, SrcLoc, const char*, ...);
static void cpp_info(CPreprocessor*, SrcLoc, const char*, ...);

static Marray(CPPToken)*_Nullable cpp_get_scratch(CPreprocessor*);
static Marray(size_t)*_Nullable cpp_get_scratch_idxes(CPreprocessor*);
static void cpp_release_scratch(CPreprocessor*, Marray(CPPToken)*);
static void cpp_release_scratch_idxes(CPreprocessor*, Marray(size_t)*);

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
cpp_define_obj_macro(CPreprocessor* cpp, StringView name, CPPToken*_Null_unspecified toks, size_t ntoks){
    CMacro* macro;
    int err = cpp_define_macro(cpp, name, ntoks, 0, &macro);
    if(err) return err;
    if(ntoks) memcpy(cpp_cmacro_replacement(macro), toks, ntoks * sizeof *toks);
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
    // phase 5
    for(;;){
        int err = cpp_next_pp_token(cpp, tok);
        if(err) return err;
        if(tok->type == CPP_WHITESPACE || tok->type == CPP_NEWLINE)
            continue;
        return 0;
    }
}
static int cpp_handle_directive(CPreprocessor *cpp);
static int cpp_expand_obj_macro(CPreprocessor *cpp, CMacro *macro, Marray(CPPToken) *dst);
static int cpp_expand_func_macro(CPreprocessor *cpp, CMacro *macro, const Marray(CPPToken) *args, const Marray(size_t) *arg_seps, Marray(CPPToken) *dst);

static
int
cpp_next_pp_token(CPreprocessor* cpp, CPPToken* ptok){
    // phase 4
    for(;;){
        CPPToken tok;
        int err = cpp_next_raw_token(cpp, &tok);
        if(err) return err;
        switch(tok.type){
            case CPP_NEWLINE:
                cpp->at_line_start = 1;
                *ptok = tok;
                return 0;
                continue;
            case CPP_WHITESPACE:
                *ptok = tok;
                return 0;
                continue;
            case CPP_IDENTIFIER:{
                if(tok.disabled) goto noexp;
                Atom a = AT_get_atom(cpp->at, tok.txt.text, tok.txt.length);
                if(!a) goto noexp;
                CMacro* macro = AM_get(&cpp->macros, a);
                if(!macro) goto noexp;
                if(macro->is_disabled) goto noexp;
                if(macro->is_function_like){
                    // Need to check for '(' - if not present, not an invocation
                    CPPToken next;
                    do {
                        err = cpp_next_raw_token(cpp, &next);
                        if(err) return err;
                    } while(next.type == CPP_WHITESPACE || next.type == CPP_NEWLINE);
                    if(next.type != CPP_PUNCTUATOR || next.punct != '('){
                        // not actually an invocation
                        err = ma_push(CPPToken)(&cpp->pending, cpp->allocator, next);
                        if(err) return err;
                        goto noexp;
                    }
                    Marray(CPPToken) *args = cpp_get_scratch(cpp);
                    Marray(size_t) *arg_seps = cpp_get_scratch_idxes(cpp);
                    if(!args || !arg_seps) return 1;
                    for(int paren = 1;;){
                        err = cpp_next_raw_token(cpp, &next);
                        if(err) return err;
                        if(next.type == CPP_EOF)
                            return cpp_error(cpp, next.loc, "EOF in function-like macro invocation %s()", a->data);
                        if(next.type == CPP_PUNCTUATOR){
                            if(next.punct == ')'){
                                paren--;
                                if(!paren) break;
                            }
                            else if(next.punct == '(')
                                paren++;
                            else if(next.punct == ',' && paren == 1){
                                if(macro->is_variadic || (macro->nparams > 1 && arg_seps->count < (size_t)macro->nparams-1)){
                                    err = ma_push(size_t)(arg_seps, cpp->allocator, args->count);
                                    if(err) return err;
                                }
                                else
                                    return cpp_error(cpp, next.loc, "Too many arguments to function-like macro");
                            }
                        }
                        err = ma_push(CPPToken)(args, cpp->allocator, next);
                        if(err) return err;
                    }
                    if(args->count && !macro->nparams && !macro->is_variadic)
                        return cpp_error(cpp, args->data[0].loc, "Too many arguments to function-like macro");
                    if(arg_seps->count+1 < macro->nparams)
                        return cpp_error(cpp, args->data[0].loc, "Too few arguments to function-like macro");
                    err = cpp_expand_func_macro(cpp, macro, args, arg_seps, &cpp->pending);
                    if(err) return err;
                    cpp_release_scratch_idxes(cpp, arg_seps);
                    cpp_release_scratch(cpp, args);
                    continue;
                }
                err = cpp_expand_obj_macro(cpp, macro, &cpp->pending);
                if(err) return err;
                continue;

                noexp:
                *ptok = tok;
                cpp->at_line_start = 0;
            }return 0;
            case CPP_PUNCTUATOR:
                if(cpp->at_line_start && tok.punct == '#'){
                    cpp->at_line_start = 0;
                    err = cpp_handle_directive(cpp);
                    if(err) return err;
                    continue;
                }
                *ptok = tok;
                cpp->at_line_start = 0;
                return 0;
            case CPP_EOF:
            case CPP_STRING:
            case CPP_CHAR:
            case CPP_NUMBER:
            case CPP_OTHER:
                *ptok = tok;
                cpp->at_line_start = 0;
                return 0;
            default:
                return cpp_error(cpp, tok.loc, "%.*s token escaped", sv_p(CPPTokenTypeSV[tok.type]));
        }
    }
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
cpp_handle_continuation(CPPFrame* f){
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
            f->line++;
            f->column = 1;
            continue;
        }
        if(txt.text[c+1] == '\r'){
            if(c + 2 == txt.length){
                c += 2;
                break;
            }
            if(txt.text[c+2] == '\n'){
                c += 3;
                f->line++;
                f->column = 1;
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
    cpp_handle_continuation(f);
    StringView txt = f->txt;
    int result;
    if(f->cursor == txt.length)
        result = -1;
    else {
        result = (int)(unsigned char)txt.text[f->cursor++];
        if(result == '\n'){
            f->line++;
            f->column = 1;
        }
        else {
            f->column++;
        }
    }
    return result;
}

static
inline
int
cpp_peek_char(CPPFrame* f){
    cpp_handle_continuation(f);
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
        f->column++;
        return 1;
    }
    return 0;
}

static
inline
_Bool
cpp_match_2char(CPPFrame* f, int ch1, int ch2){
    struct CPPFrameLoc loc = f->loc;
    if(cpp_match_char(f, ch1) && cpp_match_char(f, ch2)){
        return 1;
    }
    f->loc = loc;
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
            f->column++;
            return 1;
        }
    return 0;
}

static
StringView
cpp_clean_token(CPreprocessor* cpp, size_t len, const char* txt){
    MStringBuilder sb = {.allocator = allocator_from_arena(&cpp->synth_arena)};

    const char* prev = txt;
    const char* p = txt;
    const char* end = p + len;
    for(;end!=p;){
        const char* back = memchr(p, '\\', end-p);
        if(!back) break;
        if(back + 1 < end && back[1] == '\n'){
            if(back != prev) msb_write_str(&sb, prev, back-prev);
            p = back+2;
            prev = p;
            continue;
        }
        if(back + 2 < end && back[1] == '\r' && back[2] == '\n'){
            if(back != prev) msb_write_str(&sb, prev, back-prev);
            p = back+3;
            prev = p;
            continue;
        }
        p = back+1;
    }
    if(prev==txt) return (StringView){len, txt};
    if(end != prev) msb_write_str(&sb, prev, end-prev);
    return msb_detach_sv(&sb);
}

static
int
cpp_next_raw_token(CPreprocessor* cpp, CPPToken* tok){
    while(cpp->pending.count){
        *tok = ma_tail(cpp->pending);
        cpp->pending.count--;
        if(tok->type == CPP_REENABLE){
            ((CMacro*)tok->data1)->is_disabled = 0;
            continue;
        }
        return 0;
    }
    // phase 1-3
    again:;
    if(!cpp->frames.count){
        *tok = (CPPToken){.type = CPP_EOF};
        return 0;
    }
    CPPFrame* f = &ma_tail(cpp->frames);
    SrcLoc loc = {.file_id = f->file_id, .line = f->line, .column = f->column};
    cpp_handle_continuation(f);
    size_t start = f->cursor;
    int c = cpp_next_char(f);
    switch(c){
        default:
            default_:
            *tok = (CPPToken){.type = CPP_OTHER, .txt = {1, &f->txt.text[start]}, .loc = loc};
            return 0;
        case -1:{
            cpp->frames.count--;
            goto again;
        }
        case '\n':
            *tok = (CPPToken){.type = CPP_NEWLINE, .txt = SV("\n"), .loc = loc};
            return 0;
        // Whitespace
        case ' ': case '\t': case '\r': case '\f': case '\v':
            while(cpp_match_oneof(f, " \t\r\f\v"))
                ;                                            // don't clean whitespace
            *tok = (CPPToken){.type = CPP_WHITESPACE, .txt = {f->cursor-start, &f->txt.text[start]}, .loc = loc};
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
                    f->column++;
                    goto string_or_char;
                }
            }
            else if(c == 'u'){
                c = cpp_peek_char(f);
                if(c == '"' || c == '\''){
                    f->cursor++;
                    f->column++;
                    goto string_or_char;
                }
                if(cpp_match_char(f, '8')){
                    c = cpp_peek_char(f);
                    if(c == '"' || c == '\''){
                        f->cursor++;
                        f->column++;
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
                        f->column++;
                        continue;
                    default:
                        break;
                }
                break;
            }
            *tok = (CPPToken){.type = CPP_IDENTIFIER, .txt = cpp_clean_token(cpp, f->cursor-start, &f->txt.text[start]), .loc = loc};
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
                        f->column++;
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
                                    f->column++;
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
            *tok = (CPPToken){.type = CPP_NUMBER, .txt = cpp_clean_token(cpp, f->cursor-start, &f->txt.text[start]), .loc = loc};
            return 0;
        case '.':{
            c = cpp_peek_char(f);
            if(c >= '0' && c <= '9'){
                f->cursor++;
                f->column++;
                goto pp_number;
            }
            if(cpp_match_2char(f, '.', '.')){
                *tok = (CPPToken){.type = CPP_PUNCTUATOR, .txt = SV("..."), .punct='...', .loc = loc};
                return 0;
            }
            *tok = (CPPToken){.type = CPP_PUNCTUATOR, .txt = SV("."), .punct='.', .loc = loc};
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
                if(c == '\n' || c == -1)
                    return cpp_error(cpp, loc, "Unterminated %s literal",  (terminator == '"')?"string":"character");
                if(c == '\\')
                    backslash = !backslash;
                else if(c == terminator && !backslash)
                    break;
                else
                    backslash = 0;
            }
            *tok = (CPPToken){.type = type, .txt = cpp_clean_token(cpp, f->cursor-start, &f->txt.text[start]), .loc = loc};
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
                    f->column++;
                }
                                                                 // don't bother cleaning continuations, it's a comment
                *tok = (CPPToken){.type = CPP_WHITESPACE, .txt = {f->cursor-start, &f->txt.text[start]}, .loc = loc};
                return 0;
            }
            else if(cpp_match_char(f, '*')){ // C comment
                for(;;){
                    c = cpp_next_char(f);
                    if(c == -1)
                        return cpp_error(cpp, loc, "Unterminated comment");
                    if(c == '*' && cpp_match_char(f, '/')){       // don't bother cleaning continuations, it's a comment
                        *tok = (CPPToken){.type = CPP_WHITESPACE, .txt = {f->cursor-start, &f->txt.text[start]}, .loc = loc};
                        return 0;
                    }
                }
            }
            if(cpp_match_char(f, '='))
                *tok = (CPPToken){.type = CPP_PUNCTUATOR, .txt = SV("/="), .punct='/=', .loc = loc};
            else
                *tok = (CPPToken){.type = CPP_PUNCTUATOR, .txt = SV("/"), .punct='/', .loc = loc};
            return 0;
        }
        case '#':{
            if(cpp_match_char(f, '#'))
                *tok = (CPPToken){.type = CPP_PUNCTUATOR, .txt = SV("##"), .punct='##', .loc = loc};
            else
                *tok = (CPPToken){.type = CPP_PUNCTUATOR, .txt = SV("#"), .punct='#', .loc = loc};
            return 0;
        }
        case '*':
            if(cpp_match_char(f, '='))
                *tok = (CPPToken){.type = CPP_PUNCTUATOR, .txt=SV("*="), .punct='*=', .loc = loc};
            else
                *tok = (CPPToken){.type = CPP_PUNCTUATOR, .txt=SV("*"), .punct='*', .loc = loc};
            return 0;
        case '~':
            *tok = (CPPToken){.type = CPP_PUNCTUATOR, .txt = SV("~"), .punct='~', .loc = loc};
            return 0;
        case '!':
            if(cpp_match_char(f, '='))
                *tok = (CPPToken){.type = CPP_PUNCTUATOR, .txt = SV("!="), .punct='!=', .loc = loc};
            else
                *tok = (CPPToken){.type = CPP_PUNCTUATOR, .txt = SV("!"), .punct='!', .loc = loc};
            return 0;
        case '%':
            if(cpp_match_char(f, ':')){
                // %:%:
                if(cpp_match_2char(f, '%', ':')){
                    *tok = (CPPToken){.type = CPP_PUNCTUATOR, .txt = SV("##"), .punct='##', .loc = loc};
                    return 0;
                }
                *tok = (CPPToken){.type = CPP_PUNCTUATOR, .txt = SV("#"), .punct='#', .loc = loc};
                return 0;
            }
            if(cpp_match_char(f, '>')){
                *tok = (CPPToken){.type = CPP_PUNCTUATOR, .txt = SV("}"), .punct='}', .loc = loc};
                return 0;
            }
            if(cpp_match_char(f, '='))
                *tok = (CPPToken){.type = CPP_PUNCTUATOR, .txt = SV("%="), .punct='%=', .loc = loc};
            else
                *tok = (CPPToken){.type = CPP_PUNCTUATOR, .txt = SV("%"), .punct='%', .loc = loc};
            return 0;
        case '^':
            if(cpp_match_char(f, '='))
                *tok = (CPPToken){.type = CPP_PUNCTUATOR, .txt = SV("^="), .punct='^=', .loc = loc};
            else
                *tok = (CPPToken){.type = CPP_PUNCTUATOR, .txt = SV("^"), .punct='^', .loc = loc};
            return 0;
        case '=':
            if(cpp_match_char(f, '='))
                *tok = (CPPToken){.type = CPP_PUNCTUATOR, .txt = SV("=="), .punct='==', .loc = loc};
            else
                *tok = (CPPToken){.type = CPP_PUNCTUATOR, .txt = SV("="), .punct='=', .loc = loc};
            return 0;
        case '-':
            if(cpp_match_char(f, '>')){
                *tok = (CPPToken){.type = CPP_PUNCTUATOR, .txt = SV("->"), .punct='->', .loc = loc};
                return 0;
            }
            if(cpp_match_char(f, '-')){
                *tok = (CPPToken){.type = CPP_PUNCTUATOR, .txt = SV("--"), .punct='--', .loc = loc};
                return 0;
            }
            if(cpp_match_char(f, '='))
                *tok = (CPPToken){.type = CPP_PUNCTUATOR, .txt = SV("-="), .punct='-=', .loc = loc};
            else
                *tok = (CPPToken){.type = CPP_PUNCTUATOR, .txt = SV("-"), .punct='-', .loc = loc};
            return 0;
        case '+':
            if(cpp_match_char(f, '+')){
                *tok = (CPPToken){.type = CPP_PUNCTUATOR, .txt = SV("++"), .punct='++', .loc = loc};
                return 0;
            }
            if(cpp_match_char(f, '='))
                *tok = (CPPToken){.type = CPP_PUNCTUATOR, .txt = SV("+="), .punct='+=', .loc = loc};
            else
                *tok = (CPPToken){.type = CPP_PUNCTUATOR, .txt = SV("+"), .punct='+', .loc = loc};
            return 0;
        case '<':
            if(cpp_match_char(f, ':')){
                *tok = (CPPToken){.type = CPP_PUNCTUATOR, .txt = SV("["), .punct='[', .loc = loc};
                return 0;
            }
            if(cpp_match_char(f, '%')){
                *tok = (CPPToken){.type = CPP_PUNCTUATOR, .txt = SV("{"), .punct='{', .loc = loc};
                return 0;
            }
            if(cpp_match_char(f, '<')){
                if(cpp_match_char(f, '='))
                    *tok = (CPPToken){.type = CPP_PUNCTUATOR, .txt = SV("<<="), .punct='<<=', .loc = loc};
                else
                    *tok = (CPPToken){.type = CPP_PUNCTUATOR, .txt = SV("<<"), .punct='<<', .loc = loc};
                return 0;
            }
            if(cpp_match_char(f, '='))
                *tok = (CPPToken){.type = CPP_PUNCTUATOR, .txt = SV("<="), .punct='<=', .loc = loc};
            else
                *tok = (CPPToken){.type = CPP_PUNCTUATOR, .txt = SV("<"), .punct='<', .loc = loc};
            return 0;
        case '>':
            if(cpp_match_char(f, '>')){
                if(cpp_match_char(f, '='))
                    *tok = (CPPToken){.type = CPP_PUNCTUATOR, .txt = SV(">>="), .punct='>>=', .loc = loc};
                else
                    *tok = (CPPToken){.type = CPP_PUNCTUATOR, .txt = SV(">>"), .punct='>>', .loc = loc};
                return 0;
            }
            if(cpp_match_char(f, '='))
                *tok = (CPPToken){.type = CPP_PUNCTUATOR, .txt = SV(">="), .punct='>=', .loc = loc};
            else
                *tok = (CPPToken){.type = CPP_PUNCTUATOR, .txt = SV(">"), .punct='>', .loc = loc};
            return 0;
        case '&':
            if(cpp_match_char(f, '&')){
                *tok = (CPPToken){.type = CPP_PUNCTUATOR, .txt = SV("&&"), .punct='&&', .loc = loc};
                return 0;
            }
            if(cpp_match_char(f, '='))
                *tok = (CPPToken){.type = CPP_PUNCTUATOR, .txt = SV("&="), .punct='&=', .loc = loc};
            else
                *tok = (CPPToken){.type = CPP_PUNCTUATOR, .txt = SV("&"), .punct='&', .loc = loc};
            return 0;
        case '|':
            if(cpp_match_char(f, '|')){
                *tok = (CPPToken){.type = CPP_PUNCTUATOR, .txt = SV("||"), .punct='|', .loc = loc};
                return 0;
            }
            if(cpp_match_char(f, '='))
                *tok = (CPPToken){.type = CPP_PUNCTUATOR, .txt = SV("|="), .punct='|=', .loc = loc};
            else
                *tok = (CPPToken){.type = CPP_PUNCTUATOR, .txt = SV("|"), .punct='|', .loc = loc};
            return 0;
        case ':':
            if(cpp_match_char(f, '>')){
                *tok = (CPPToken){.type = CPP_PUNCTUATOR, .txt = SV("]"), .punct=']', .loc = loc};
                return 0;
            }
            if(cpp_match_char(f, ':'))
                *tok = (CPPToken){.type = CPP_PUNCTUATOR, .txt = SV("::"), .punct='::', .loc = loc};
            else
                *tok = (CPPToken){.type = CPP_PUNCTUATOR, .txt = SV(":"), .punct=':', .loc = loc};
            return 0;
        case '(':
            *tok = (CPPToken){.type = CPP_PUNCTUATOR, .txt = SV("("), .punct='(', .loc = loc};
            return 0;
        case ')':
            *tok = (CPPToken){.type = CPP_PUNCTUATOR, .txt = SV(")"), .punct=')', .loc = loc};
            return 0;
        case '[':
            *tok = (CPPToken){.type = CPP_PUNCTUATOR, .txt = SV("["), .punct='[', .loc = loc};
            return 0;
        case ']':
            *tok = (CPPToken){.type = CPP_PUNCTUATOR, .txt = SV("]"), .punct=']', .loc = loc};
            return 0;
        case '{':
            *tok = (CPPToken){.type = CPP_PUNCTUATOR, .txt = SV("{"), .punct='{', .loc = loc};
            return 0;
        case '}':
            *tok = (CPPToken){.type = CPP_PUNCTUATOR, .txt = SV("}"), .punct='}', .loc = loc};
            return 0;
        case '?':
            *tok = (CPPToken){.type = CPP_PUNCTUATOR, .txt = SV("?"), .punct='?', .loc = loc};
            return 0;
        case ';':
            *tok = (CPPToken){.type = CPP_PUNCTUATOR, .txt = SV(";"), .punct=';', .loc = loc};
            return 0;
        case ',':
            *tok = (CPPToken){.type = CPP_PUNCTUATOR, .txt = SV(","), .punct=',', .loc = loc};
            return 0;
    }
}

static
int
cpp_error(CPreprocessor* cpp, SrcLoc loc, const char* fmt, ...){
    // TODO: location chaining
    LongString path = loc.file_id < cpp->fc->map.count?cpp->fc->map.data[loc.file_id].path:LS("???");
    log_sprintf(cpp->logger, "%s:%d:%d: error: ", path.text, (int)loc.line, (int)loc.column);
    va_list va;
    va_start(va, fmt);
    log_logv(cpp->logger, LOG_PRINT_ERROR, fmt, va);
    va_end(va);
    return 1;
}

static
void
cpp_warn(CPreprocessor* cpp, SrcLoc loc, const char* fmt, ...){
    // TODO: location chaining
    LongString path = loc.file_id < cpp->fc->map.count?cpp->fc->map.data[loc.file_id].path:LS("???");
    log_sprintf(cpp->logger, "%s:%d:%d: warning: ", path.text, (int)loc.line, (int)loc.column);
    va_list va;
    va_start(va, fmt);
    log_logv(cpp->logger, LOG_PRINT_ERROR, fmt, va);
    va_end(va);
}

static
void
cpp_info(CPreprocessor* cpp, SrcLoc loc, const char* fmt, ...){
    // TODO: location chaining
    LongString path = loc.file_id < cpp->fc->map.count?cpp->fc->map.data[loc.file_id].path:LS("???");
    log_sprintf(cpp->logger, "%s:%d:%d: info: ", path.text, (int)loc.line, (int)loc.column);
    va_list va;
    va_start(va, fmt);
    log_logv(cpp->logger, LOG_PRINT_ERROR, fmt, va);
    va_end(va);
}

static
int
cpp_handle_directive(CPreprocessor* cpp){
    int err;
    CPPToken tok;
    do {
        err = cpp_next_raw_token(cpp, &tok);
        if(err) return err;
    }while(tok.type == CPP_WHITESPACE);
    if(tok.type != CPP_IDENTIFIER){
        while(tok.type != CPP_EOF && tok.type != CPP_NEWLINE){
            err = cpp_next_raw_token(cpp, &tok);
            if(err) return err;
        }
        // push it back so dispatch loop sees newline
        return ma_push(CPPToken)(&cpp->pending, cpp->allocator, tok);
    }
    if(sv_equals(tok.txt, SV("define"))){
        err = cpp_next_raw_token(cpp, &tok);
        if(err) return err;
        if(tok.type != CPP_WHITESPACE) return cpp_error(cpp, tok.loc, "macro name missing");
        err = cpp_next_raw_token(cpp, &tok);
        if(err) return err;
        if(tok.type != CPP_IDENTIFIER) return cpp_error(cpp, tok.loc, "macro name missing");
        StringView name = tok.txt;
        err = cpp_next_raw_token(cpp, &tok);
        if(tok.type == CPP_NEWLINE || tok.type == CPP_EOF){
            // #define foo
            err = cpp_define_obj_macro(cpp, name, NULL, 0);
            if(err) return err;
            // push it back so dispatch loop sees newline
            return ma_push(CPPToken)(&cpp->pending, cpp->allocator, tok);
        }
        else if(tok.type == CPP_PUNCTUATOR && tok.punct == '('){
            Marray(CPPToken) *names = cpp_get_scratch(cpp);
            if(!names) return 1;
            Marray(CPPToken) *repl = cpp_get_scratch(cpp);
            if(!repl) return 1;
            _Bool variadic = 0;
            for(;;){
                do {
                    err = cpp_next_raw_token(cpp, &tok);
                    if(err) return err;
                }while(tok.type == CPP_WHITESPACE);
                if(tok.type == CPP_PUNCTUATOR && tok.punct == ')')
                    break;
                if(names->count){
                    if(tok.type != CPP_PUNCTUATOR || tok.punct != ',')
                        return cpp_error(cpp, tok.loc, "Expecting ',' between param names");
                    do {
                        err = cpp_next_raw_token(cpp, &tok);
                        if(err) return err;
                    }while(tok.type == CPP_WHITESPACE);
                }
                if(tok.type == CPP_PUNCTUATOR && tok.punct == '...'){
                    variadic = 1;
                    do {
                        err = cpp_next_raw_token(cpp, &tok);
                        if(err) return err;
                    }while(tok.type == CPP_WHITESPACE);
                    if(tok.type != CPP_PUNCTUATOR || tok.punct != ')')
                        return cpp_error(cpp, tok.loc, "... must be last macro param");
                    break;
                }
                if(tok.type != CPP_IDENTIFIER)
                    return cpp_error(cpp, tok.loc, "expected macro param name");
                err = ma_push(CPPToken)(names, cpp->allocator, tok);
                if(err) return err;
            }
            err = cpp_next_raw_token(cpp, &tok);
            if(err) return err;
            if(tok.type != CPP_WHITESPACE && tok.type != CPP_NEWLINE && tok.type != CPP_EOF)
                return cpp_error(cpp, tok.loc, "Illegal token immediately after ')'");
            if(tok.type == CPP_WHITESPACE){
                err = cpp_next_raw_token(cpp, &tok);
                if(err) return err;
            }
            while(tok.type != CPP_NEWLINE && tok.type != CPP_EOF){
                err = ma_push(CPPToken)(repl, cpp->allocator, tok);
                if(err) return err;
                err = cpp_next_raw_token(cpp, &tok);
                if(err) return err;
            }
            // push it back so dispatch loop sees newline
            err = ma_push(CPPToken)(&cpp->pending, cpp->allocator, tok);
            if(err) return err;
            while(repl->count && ma_tail(*repl).type == CPP_WHITESPACE)
                repl->count--;
            CMacro* m;
            err = cpp_define_macro(cpp, name, repl->count, names->count, &m);
            if(err) return err;
            m->is_variadic = variadic;
            m->is_function_like = 1;
            Atom* params = cpp_cmacro_params(m);
            for(size_t i = 0; i < names->count; i++){
                Atom a = AT_atomize(cpp->at, names->data[i].txt.text, names->data[i].txt.length);
                if(!a) return 1;
                for(size_t j = 0; j < i; j++){
                    if(params[j] == a)
                        return cpp_error(cpp, names->data[i].loc, "Duplicate macro param name");
                }
                params[i] = a;
            }
            MARRAY_FOR_EACH(CPPToken, t, *repl){
                if(t->type != CPP_IDENTIFIER)
                    continue;
                Atom a = AT_get_atom(cpp->at, t->txt.text, t->txt.length);
                if(!a) continue; // Not already in atom table, thus not in the params list either
                for(size_t i = 0; i < m->nparams; i++){
                    if(a == params[i]){
                        t->param_idx = i+1;
                        break;
                    }
                }
            }
            if(repl->count)
                memcpy(cpp_cmacro_replacement(m), repl->data, repl->count*sizeof repl->data[0]);
            cpp_release_scratch(cpp, repl);
            cpp_release_scratch(cpp, names);
            return 0;
        }
        else if(tok.type == CPP_WHITESPACE){
            Marray(CPPToken) *repl = cpp_get_scratch(cpp);
            if(!repl) return 1;
            for(;;){
                err = cpp_next_raw_token(cpp, &tok);
                if(err) return err;
                if(tok.type == CPP_EOF || tok.type == CPP_NEWLINE){
                    // push it back so dispatch loop sees newline
                    err = ma_push(CPPToken)(&cpp->pending, cpp->allocator, tok);
                    if(err) return err;
                    break;
                }
                err = ma_push(CPPToken)(repl, cpp->allocator, tok);
                if(err) return err;
            }
            while(repl->count && ma_tail(*repl).type == CPP_WHITESPACE)
                repl->count--;
            err = cpp_define_obj_macro(cpp, name, repl->data, repl->count);
            if(err) return err;
            cpp_release_scratch(cpp, repl);
            return 0;
        }
        else {
            return cpp_error(cpp, tok.loc, "Unexpected token type in #define");
        }
    }
    return 0;
}
static
int
cpp_expand_obj_macro(CPreprocessor *cpp, CMacro *macro, Marray(CPPToken) *dst){
    macro->is_disabled = 1;
    // Push reenable token (so it's consumed last)
    CPPToken reenable = {.type = CPP_REENABLE, .data1 = macro};
    int err = ma_push(CPPToken)(dst, cpp->allocator, reenable);
    if(err) return err;

    // Push replacement tokens in reverse order
    CPPToken* repl = cpp_cmacro_replacement(macro);
    for(size_t i = macro->nreplace; i-- > 0;){
        CPPToken tok = repl[i];
        // If this token matches a disabled macro, paint it blue
        if(tok.type == CPP_IDENTIFIER && !tok.disabled){
            Atom a = AT_get_atom(cpp->at, tok.txt.text, tok.txt.length);
            if(a){
                CMacro* m = AM_get(&cpp->macros, a);
                if(m && m->is_disabled)
                    tok.disabled = 1;
            }
        }
        err = ma_push(CPPToken)(dst, cpp->allocator, tok);
        if(err) return err;
    }
    return 0;
}

static
int
cpp_expand_func_macro(CPreprocessor *cpp, CMacro *macro, const Marray(CPPToken) *args, const Marray(size_t) *arg_seps, Marray(CPPToken) *dst){
    size_t nargs = arg_seps->count+1;
    if(!macro->nparams && !macro->is_variadic)
        nargs = 0;
    (void)nargs;
    (void)args;
    int err;
    macro->is_disabled = 1;
    CPPToken reenable = {.type = CPP_REENABLE, .data1 = macro};
    err = ma_push(CPPToken)(dst, cpp->allocator, reenable);
    if(err) return err;
    return 0;
}

static 
Marray(CPPToken)*_Nullable
cpp_get_scratch(CPreprocessor *cpp){
    Marray(CPPToken) *scratch = fl_pop(&cpp->scratch_list);
    if(!scratch) scratch = Allocator_zalloc(cpp->allocator, sizeof *scratch);
    if(!scratch) return NULL;
    scratch->count = 0;
    return scratch;
}
static 
void 
cpp_release_scratch(CPreprocessor *cpp, Marray(CPPToken) *scratch){
    fl_push(&cpp->scratch_list, scratch);
}

static 
Marray(size_t)*_Nullable
cpp_get_scratch_idxes(CPreprocessor *cpp){
    Marray(size_t) *scratch = fl_pop(&cpp->scratch_idxes);
    if(!scratch) scratch = Allocator_zalloc(cpp->allocator, sizeof *scratch);
    if(!scratch) return NULL;
    scratch->count = 0;
    return scratch;
}
static 
void 
cpp_release_scratch_idxes(CPreprocessor *cpp, Marray(size_t) *scratch){
    fl_push(&cpp->scratch_idxes, scratch);
}

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#ifdef __clang__
#pragma clang assume_nonnull end
#endif
#endif
