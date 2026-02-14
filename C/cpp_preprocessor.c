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
LOG_PRINTF(3, 4)
static int cpp_error(CPreprocessor*, SrcLoc, const char*, ...);
LOG_PRINTF(3, 4)
static void cpp_warn(CPreprocessor*, SrcLoc, const char*, ...);
LOG_PRINTF(3, 4)
static void cpp_info(CPreprocessor*, SrcLoc, const char*, ...);

static Marray(CPPToken)*_Nullable cpp_get_scratch(CPreprocessor*);
static Marray(size_t)*_Nullable cpp_get_scratch_idxes(CPreprocessor*);
static void cpp_release_scratch(CPreprocessor*, Marray(CPPToken)*);
static void cpp_release_scratch_idxes(CPreprocessor*, Marray(size_t)*);
enum {
    CPP_NO_ERROR = 0,
    CPP_OOM_ERROR,
    CPP_MACRO_ALREADY_EXISTS_ERROR,
    CPP_SYNTAX_ERROR,
    CPP_UNREACHABLE_ERROR,
};

static
int
cpp_define_macro(CPreprocessor* cpp, StringView name, size_t ntoks, size_t nparams, CMacro*_Nullable*_Nonnull outmacro){
    Atom key = AT_atomize(cpp->at, name.text, name.length);
    if(!key) return CPP_OOM_ERROR;
    CMacro* macro = AM_get(&cpp->macros, key);
    if(macro) return CPP_MACRO_ALREADY_EXISTS_ERROR;
    size_t size = sizeof *macro + sizeof(CPPToken)*ntoks + sizeof(Atom)*nparams;
    macro = Allocator_zalloc(cpp->allocator, size);
    if(!macro) return CPP_OOM_ERROR;
    macro->nreplace = ntoks;
    macro->nparams = nparams;
    int err = AM_put(&cpp->macros, cpp->allocator, key, macro);
    if(err) Allocator_free(cpp->allocator, macro, size);
    else *outmacro = macro;
    return err?CPP_OOM_ERROR:0;
}

static
int
cpp_undef_macro(CPreprocessor* cpp, StringView name){
    Atom key = AT_get_atom(cpp->at, name.text, name.length);
    if(!key) return 0;
    CMacro* macro = AM_get(&cpp->macros, key);
    if(!macro) return 0;
    size_t size = sizeof *macro + sizeof(CPPToken)*macro->nreplace + sizeof(Atom)*macro->nparams;
    Allocator_free(cpp->allocator, macro, size);
    int err = AM_put(&cpp->macros, cpp->allocator, key, NULL);
    if(err) return CPP_OOM_ERROR;
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
cpp_tokenize_from_frame(CPreprocessor* cpp, CPPFrame* f, CPPToken* tok){
    retry:;
    SrcLoc loc = {.file_id = f->file_id, .line = f->line, .column = f->column};
    cpp_handle_continuation(f);
    size_t start = f->cursor;
    int c = cpp_next_char(f);
    switch(c){
        default:
            default_:
            *tok = (CPPToken){.type = CPP_OTHER, .txt = {1, &f->txt.text[start]}, .loc = loc};
            return 0;
        case -1:
            *tok = (CPPToken){.type = CPP_EOF, .loc = loc};
            return 0;
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
                goto retry;
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
    // Start of file is start of line
    if(f->cursor == 0)
        cpp->at_line_start = 1;
    cpp_handle_continuation(f);
    if(f->cursor == f->txt.length){
        cpp->frames.count--;
        goto again;
    }
    return cpp_tokenize_from_frame(cpp, f, tok);
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
    return CPP_SYNTAX_ERROR;
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
            if(err == CPP_MACRO_ALREADY_EXISTS_ERROR){
                Atom a = AT_get_atom(cpp->at, name.text, name.length);
                if(!a) return CPP_UNREACHABLE_ERROR;
                CMacro* m = AM_get(&cpp->macros, a);
                if(!m) return CPP_UNREACHABLE_ERROR;
                if(m->nparams || m->is_function_like || m->nreplace){
                    return cpp_error(cpp, tok.loc, "Duplicate object-like macro (%.*s) with different definitions", sv_p(name));
                }
                err = 0;
            }
            if(err) return err;
            // push it back so dispatch loop sees newline
            return ma_push(CPPToken)(&cpp->pending, cpp->allocator, tok);
        }
        else if(tok.type == CPP_PUNCTUATOR && tok.punct == '('){
            Marray(CPPToken) *names = cpp_get_scratch(cpp);
            if(!names) return CPP_OOM_ERROR;
            Marray(CPPToken) *repl = cpp_get_scratch(cpp);
            if(!repl) return CPP_OOM_ERROR;
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
                if(tok.type == CPP_WHITESPACE && repl->count && ma_tail(*repl).type == CPP_WHITESPACE){
                    // coalesce whitespace in #defines
                }
                else {
                    err = ma_push(CPPToken)(repl, cpp->allocator, tok);
                    if(err) return err;
                }
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
            if(err == CPP_MACRO_ALREADY_EXISTS_ERROR){
                Atom a = AT_get_atom(cpp->at, name.text, name.length);
                if(!a) return CPP_UNREACHABLE_ERROR;
                m = AM_get(&cpp->macros, a);
                if(!m) return CPP_UNREACHABLE_ERROR;
                if(!m->is_function_like){
                    return cpp_error(cpp, tok.loc, "redefinition of an object-like macro (%.*s) as a function-like macro", sv_p(name));
                }
                if(!!variadic != !!m->is_variadic)
                    return cpp_error(cpp, tok.loc, "Duplicate function-like macro (%.*s) with different definitions", sv_p(name));
                if(names->count != m->nparams)
                    return cpp_error(cpp, tok.loc, "Duplicate function-like macro (%.*s) with different definitions", sv_p(name));
                for(size_t i = 0; i < names->count; i++){
                    CPPToken tname = names->data[i];
                    Atom pname = cpp_cmacro_params(m)[i];
                    if(AT_get_atom(cpp->at, tname.txt.text, tname.txt.length) != pname)
                        return cpp_error(cpp, tok.loc, "Duplicate function-like macro (%.*s) with different definitions", sv_p(name));
                }
                if(repl->count != m->nreplace){
                    return cpp_error(cpp, tok.loc, "%d: Duplicate function-like macro (%.*s) with different definitions %zu %zu", __LINE__, sv_p(name), repl->count, (size_t)m->nreplace);
                }
                for(size_t i = 0; i < repl->count; i++){
                    CPPToken r = repl->data[i];
                    CPPToken pre = cpp_cmacro_replacement(m)[i];
                    if(r.type == CPP_WHITESPACE && pre.type == CPP_WHITESPACE)
                        continue;
                    if(r.type != pre.type)
                        return cpp_error(cpp, tok.loc, "%d: Duplicate function-like macro (%.*s) with different definitions", __LINE__, sv_p(name));
                    if(!sv_equals(r.txt, pre.txt))
                        return cpp_error(cpp, tok.loc, "%d: Duplicate function-like macro (%.*s) with different definitions", __LINE__, sv_p(name));
                }
                err = 0;
                goto finish_func_macro;
            }
            if(err) return err;
            m->is_variadic = variadic;
            m->is_function_like = 1;
            Atom* params = cpp_cmacro_params(m);
            for(size_t i = 0; i < names->count; i++){
                Atom a = AT_atomize(cpp->at, names->data[i].txt.text, names->data[i].txt.length);
                if(!a) return CPP_OOM_ERROR;
                for(size_t j = 0; j < i; j++){
                    if(params[j] == a)
                        return cpp_error(cpp, names->data[i].loc, "Duplicate macro param name");
                }
                params[i] = a;
            }
            MARRAY_FOR_EACH(CPPToken, t, *repl){
                if(t->type != CPP_IDENTIFIER)
                    continue;
                // Tag __VA_ARGS__ in variadic macros
                if(variadic && sv_equals(t->txt, SV("__VA_ARGS__"))){
                    t->param_idx = m->nparams + 1;
                    continue;
                }
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
            finish_func_macro:;
            cpp_release_scratch(cpp, repl);
            cpp_release_scratch(cpp, names);
            return 0;
        }
        else if(tok.type == CPP_WHITESPACE){
            while(tok.type == CPP_WHITESPACE){
                err = cpp_next_raw_token(cpp, &tok);
                if(err) return err;
            }
            if(tok.type != CPP_WHITESPACE){
                err = ma_push(CPPToken)(&cpp->pending, cpp->allocator, tok);
                if(err) return err;
            }
            Marray(CPPToken) *repl = cpp_get_scratch(cpp);
            if(!repl) return CPP_OOM_ERROR;
            for(;;){
                err = cpp_next_raw_token(cpp, &tok);
                if(err) return err;
                if(tok.type == CPP_EOF || tok.type == CPP_NEWLINE){
                    // push it back so dispatch loop sees newline
                    err = ma_push(CPPToken)(&cpp->pending, cpp->allocator, tok);
                    if(err) return err;
                    break;
                }
                if(tok.type == CPP_WHITESPACE && repl->count && ma_tail(*repl).type == CPP_WHITESPACE){
                    // coalesce whitespace in #defines
                }
                else {
                    err = ma_push(CPPToken)(repl, cpp->allocator, tok);
                    if(err) return err;
                }
            }
            while(repl->count && ma_tail(*repl).type == CPP_WHITESPACE)
                repl->count--;
            err = cpp_define_obj_macro(cpp, name, repl->data, repl->count);
            if(err == CPP_MACRO_ALREADY_EXISTS_ERROR){
                Atom a = AT_get_atom(cpp->at, name.text, name.length);
                if(!a) return CPP_UNREACHABLE_ERROR;
                CMacro* m = AM_get(&cpp->macros, a);
                if(!m) return CPP_UNREACHABLE_ERROR;
                if(m->is_function_like)
                    return cpp_error(cpp, tok.loc, "Redefinition of function-like macro (%.*s) as an object-like macro", sv_p(name));
                if(m->nreplace != repl->count)
                    return cpp_error(cpp, tok.loc, "Duplicate object-like macro (%.*s) with different definitions, %zu != %zu", sv_p(name), repl->count, (size_t)m->nreplace);
                for(size_t i = 0; i < repl->count; i++){
                    CPPToken r = repl->data[i];
                    CPPToken pre = cpp_cmacro_replacement(m)[i];
                    if(r.type == CPP_WHITESPACE && pre.type == CPP_WHITESPACE)
                        continue;
                    if(r.type != pre.type)
                        return cpp_error(cpp, tok.loc, "Duplicate object-like macro (%.*s) with different definitions (%zu different type)", sv_p(name), i);
                    if(!sv_equals(r.txt, pre.txt))
                        return cpp_error(cpp, tok.loc, "Duplicate object-like macro (%.*s) with different definitions (%zu different content)", sv_p(name), i);
                }
                // Duplicate macro definition, ok
                err = 0;
            }
            if(err) return err;
            cpp_release_scratch(cpp, repl);
            return 0;
        }
        else {
            return cpp_error(cpp, tok.loc, "Unexpected token type in #define");
        }
    }
    else if(sv_equals(tok.txt, SV("undef"))){
        err = cpp_next_raw_token(cpp, &tok);
        if(err) return err;
        if(tok.type != CPP_WHITESPACE) return cpp_error(cpp, tok.loc, "macro name missing");
        err = cpp_next_raw_token(cpp, &tok);
        if(err) return err;
        if(tok.type != CPP_IDENTIFIER) return cpp_error(cpp, tok.loc, "macro name missing");
        StringView name = tok.txt;
        for(;;){
            err = cpp_next_raw_token(cpp, &tok);
            if(err) return err;
            if(tok.type == CPP_WHITESPACE) continue;
            if(tok.type == CPP_NEWLINE || tok.type == CPP_EOF){
                // push it back so dispatch loop sees newline
                err = ma_push(CPPToken)(&cpp->pending, cpp->allocator, tok);
                if(err) return err;
                break;
            }
            cpp_warn(cpp, tok.loc, "Trailing tokens after #undef");
        }
        err = cpp_undef_macro(cpp, name);
        if(err){
            cpp_info(cpp, tok.loc, "error undefing macro: %d", err);
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

// Helper: Get argument N from args array using arg_seps indices
// arg_seps[i] points to the comma token in args; arg0 = args[0..arg_seps[0]),
// arg1 = args[arg_seps[0]+1..arg_seps[1]), etc. (skip the comma)
static
void
cpp_get_argument(const Marray(CPPToken) *args, const Marray(size_t) *arg_seps,
                 size_t arg_idx, CPPToken*_Nullable*_Nonnull out_start, size_t *out_count){
    size_t start, end;
    if(arg_idx == 0){
        start = 0;
        end = arg_seps->count > 0 ? arg_seps->data[0] : args->count;
    }
    else if(arg_idx <= arg_seps->count){
        // Start after the comma token
        start = arg_seps->data[arg_idx - 1] + 1;
        end = (arg_idx < arg_seps->count) ? arg_seps->data[arg_idx] : args->count;
    }
    else {
        // Beyond available arguments (for variadic)
        start = args->count;
        end = args->count;
    }
    // Skip leading/trailing whitespace
    while(start < end && args->data[start].type == CPP_WHITESPACE)
        start++;
    while(end > start && args->data[end-1].type == CPP_WHITESPACE)
        end--;
    *out_start = (start < args->count) ? &args->data[start] : NULL;
    *out_count = end - start;
}

// Helper: Get variadic arguments (all args from nparams onward, comma-separated)
static
void
cpp_get_va_args(const Marray(CPPToken) *args, const Marray(size_t) *arg_seps,
                size_t nparams, CPPToken*_Nullable*_Nonnull out_start, size_t *out_count){
    size_t start;
    if(nparams == 0){
        start = 0;
    }
    else if(nparams <= arg_seps->count){
        // Start after the comma token
        start = arg_seps->data[nparams - 1] + 1;
    }
    else {
        start = args->count;
    }
    // Skip leading whitespace
    while(start < args->count && args->data[start].type == CPP_WHITESPACE)
        start++;
    size_t end = args->count;
    // Skip trailing whitespace
    while(end > start && args->data[end-1].type == CPP_WHITESPACE)
        end--;
    *out_start = (start < args->count) ? &args->data[start] : NULL;
    *out_count = end - start;
}

// Forward declaration
static int cpp_expand_argument(CPreprocessor *cpp, CPPToken*_Nullable toks, size_t count, Marray(CPPToken) *out);

// Helper: Check if VA_ARGS is non-empty after expansion (C23 6.10.4.1)
static
_Bool
cpp_va_args_nonempty(CPreprocessor *cpp, const Marray(CPPToken) *args, const Marray(size_t) *arg_seps, size_t nparams){
    CPPToken* start;
    size_t count;
    cpp_get_va_args(args, arg_seps, nparams, &start, &count);
    if(!count) return 0;

    // Expand the VA_ARGS and check if result is non-empty
    Marray(CPPToken) *expanded = cpp_get_scratch(cpp);
    if(!expanded) return 0; // conservative: treat as empty on error
    int err = cpp_expand_argument(cpp, start, count, expanded);
    if(err){
        cpp_release_scratch(cpp, expanded);
        return 0;
    }
    _Bool nonempty = 0;
    for(size_t i = 0; i < expanded->count; i++){
        if(expanded->data[i].type != CPP_WHITESPACE &&
           expanded->data[i].type != CPP_NEWLINE &&
           expanded->data[i].type != CPP_PLACEMARKER){
            nonempty = 1;
            break;
        }
    }
    cpp_release_scratch(cpp, expanded);
    return nonempty;
}

// Helper: Stringify argument tokens (C23 6.10.4.2)
static
CPPToken
cpp_stringify_argument(CPreprocessor *cpp, CPPToken*_Nullable toks, size_t count, SrcLoc loc){
    MStringBuilder sb = {.allocator = allocator_from_arena(&cpp->synth_arena)};
    msb_write_char(&sb, '"');
    for(size_t i = 0; i < count; i++){
        CPPToken t = toks[i];
        if(t.type == CPP_WHITESPACE || t.type == CPP_NEWLINE){
            if(msb_peek(&sb) != ' ')
                msb_write_char(&sb, ' ');
            continue;
        }
        // For string/char literals, escape " and backslash
        if(t.type == CPP_STRING || t.type == CPP_CHAR){
            for(size_t j = 0; j < t.txt.length; j++){
                char c = t.txt.text[j];
                if(c == '"' || c == '\\')
                    msb_write_char(&sb, '\\');
                msb_write_char(&sb, c);
            }
        }
        else
            msb_write_str(&sb, t.txt.text, t.txt.length);
    }
    // Remove trailing space if any
    if(msb_peek(&sb) == ' ')
        sb.cursor--;
    msb_write_char(&sb, '"');
    return (CPPToken){
        .type = CPP_STRING,
        .txt = msb_detach_sv(&sb),
        .loc = loc
    };
}

// Forward declaration for tokenizing from a frame directly
static int cpp_tokenize_from_frame(CPreprocessor *cpp, CPPFrame *f, CPPToken *tok);

// Helper: Paste two tokens (C23 6.10.4.3)
static
int
cpp_paste_tokens(CPreprocessor *cpp, CPPToken left, CPPToken right, CPPToken *result, SrcLoc loc){
    // Handle placemarker tokens
    if(left.type == CPP_PLACEMARKER){
        *result = right;
        return 0;
    }
    if(right.type == CPP_PLACEMARKER){
        *result = left;
        return 0;
    }
    // Concatenate texts
    MStringBuilder sb = {.allocator = allocator_from_arena(&cpp->synth_arena)};
    msb_write_str(&sb, left.txt.text, left.txt.length);
    msb_write_str(&sb, right.txt.text, right.txt.length);
    StringView pasted = msb_detach_sv(&sb);

    // Tokenize directly from a local frame (don't touch cpp->frames or pending)
    CPPFrame temp_frame = {
        .txt = pasted,
        .cursor = 0,
        .line = loc.line,
        .column = loc.column,
        .file_id = loc.file_id
    };

    CPPToken tok;
    int err = cpp_tokenize_from_frame(cpp, &temp_frame, &tok);
    if(err) return err;

    // Check if we consumed the entire pasted string and got exactly one token
    if(temp_frame.cursor != pasted.length || tok.type == CPP_WHITESPACE || tok.type == CPP_EOF){
        return cpp_error(cpp, loc, "pasting \"%.*s\" and \"%.*s\" does not give a valid preprocessing token",
                        sv_p(left.txt), sv_p(right.txt));
    }

    tok.loc = loc;
    *result = tok;
    return 0;
}

// Helper: Expand argument through prescan
static
int
cpp_expand_argument(CPreprocessor *cpp, CPPToken*_Nullable toks, size_t count, Marray(CPPToken) *out){
    if(!count) return 0;

    // Push EOF marker first (will be at bottom of stack)
    CPPToken end_marker = {.type = CPP_EOF};
    int err = ma_push(CPPToken)(&cpp->pending, cpp->allocator, end_marker);
    if(err) return err;

    // Push tokens in reverse order (so they come out in forward order)
    for(size_t i = count; i-- > 0;){
        err = ma_push(CPPToken)(&cpp->pending, cpp->allocator, toks[i]);
        if(err) return err;
    }

    // Pump through cpp_next_pp_token until we hit the EOF marker
    for(;;){
        CPPToken tok;
        err = cpp_next_pp_token(cpp, &tok);
        if(err) return err;
        if(tok.type == CPP_EOF) break;
        err = ma_push(CPPToken)(out, cpp->allocator, tok);
        if(err) return err;
    }
    return 0;
}

static
int
cpp_expand_func_macro(CPreprocessor *cpp, CMacro *macro, const Marray(CPPToken) *args, const Marray(size_t) *arg_seps, Marray(CPPToken) *dst){
    int err;
    CPPToken* repl = cpp_cmacro_replacement(macro);
    size_t nreplace = macro->nreplace;

    // First pass: identify which params are operands of # or ##
    // We need a bitset for params that need raw (unexpanded) args
    uint64_t needs_raw = 0; // Bitmask: bit i set if param i needs raw arg
    for(size_t i = 0; i < nreplace; i++){
        CPPToken t = repl[i];
        if(t.type == CPP_PUNCTUATOR && t.punct == '#'){
            // # stringification - next non-whitespace must be param
            for(size_t j = i + 1; j < nreplace; j++){
                if(repl[j].type == CPP_WHITESPACE) continue;
                if(repl[j].param_idx > 0)
                    needs_raw |= (1ULL << (repl[j].param_idx - 1));
                break;
            }
        }
        else if(t.type == CPP_PUNCTUATOR && t.punct == '##'){
            // ## pasting - params on either side need raw
            // Look left (skip whitespace)
            for(size_t j = i; j-- > 0;){
                if(repl[j].type == CPP_WHITESPACE) continue;
                if(repl[j].param_idx > 0)
                    needs_raw |= (1ULL << (repl[j].param_idx - 1));
                break;
            }
            // Look right (skip whitespace)
            for(size_t j = i + 1; j < nreplace; j++){
                if(repl[j].type == CPP_WHITESPACE) continue;
                if(repl[j].param_idx > 0)
                    needs_raw |= (1ULL << (repl[j].param_idx - 1));
                break;
            }
        }
    }

    // Pre-expand arguments that are NOT adjacent to # or ##
    Marray(CPPToken) **expanded_args = NULL;
    size_t total_params = macro->nparams + (macro->is_variadic ? 1 : 0);
    if(total_params){
        expanded_args = Allocator_zalloc(cpp->allocator, sizeof(Marray(CPPToken)*) * total_params);
        if(!expanded_args) return 1;
    }

    // Substitution pass: build result list
    Marray(CPPToken) *result = cpp_get_scratch(cpp);
    if(!result) return CPP_OOM_ERROR;

    for(size_t i = 0; i < nreplace; i++){
        CPPToken t = repl[i];

        // Handle # stringification
        if(t.type == CPP_PUNCTUATOR && t.punct == '#'){
            // Skip whitespace to find param or __VA_OPT__
            size_t j = i + 1;
            while(j < nreplace && repl[j].type == CPP_WHITESPACE) j++;

            // Check for # __VA_OPT__(content)
            if(j < nreplace && repl[j].type == CPP_IDENTIFIER && sv_equals(repl[j].txt, SV("__VA_OPT__"))){
                // Parse __VA_OPT__(content)
                size_t k = j + 1;
                while(k < nreplace && repl[k].type == CPP_WHITESPACE) k++;
                if(k >= nreplace || repl[k].type != CPP_PUNCTUATOR || repl[k].punct != '('){
                    cpp_release_scratch(cpp, result);
                    if(expanded_args) Allocator_free(cpp->allocator, expanded_args, sizeof(Marray(CPPToken)*) * total_params);
                    return cpp_error(cpp, repl[j].loc, "__VA_OPT__ must be followed by (content)");
                }
                k++; // skip '('
                size_t content_start = k;
                int paren = 1;
                while(k < nreplace && paren > 0){
                    if(repl[k].type == CPP_PUNCTUATOR){
                        if(repl[k].punct == '(') paren++;
                        else if(repl[k].punct == ')') paren--;
                    }
                    if(paren > 0) k++;
                }
                if(paren != 0){
                    cpp_release_scratch(cpp, result);
                    if(expanded_args) Allocator_free(cpp->allocator, expanded_args, sizeof(Marray(CPPToken)*) * total_params);
                    return cpp_error(cpp, repl[j].loc, "unterminated __VA_OPT__");
                }
                size_t content_end = k; // points to ')'

                CPPToken stringified;
                if(cpp_va_args_nonempty(cpp, args, arg_seps, macro->nparams)){
                    // Substitute parameters in content (raw, not expanded)
                    Marray(CPPToken) *content_tokens = cpp_get_scratch(cpp);
                    if(!content_tokens) return CPP_OOM_ERROR;
                    for(size_t m = content_start; m < content_end; m++){
                        CPPToken ct = repl[m];
                        if(ct.param_idx > 0){
                            size_t pidx = ct.param_idx - 1;
                            CPPToken* arg_start;
                            size_t arg_count;
                            if(pidx == macro->nparams && macro->is_variadic){
                                cpp_get_va_args(args, arg_seps, macro->nparams, &arg_start, &arg_count);
                            }
                            else {
                                cpp_get_argument(args, arg_seps, pidx, &arg_start, &arg_count);
                            }
                            if(arg_count == 0){
                                // Empty arg -> placemarker
                                CPPToken pm = {.type = CPP_PLACEMARKER, .loc = ct.loc};
                                err = ma_push(CPPToken)(content_tokens, cpp->allocator, pm);
                                if(err) return err;
                            }
                            else {
                                for(size_t n = 0; n < arg_count; n++){
                                    err = ma_push(CPPToken)(content_tokens, cpp->allocator, arg_start[n]);
                                    if(err) return err;
                                }
                            }
                        }
                        else {
                            err = ma_push(CPPToken)(content_tokens, cpp->allocator, ct);
                            if(err) return err;
                        }
                    }
                    // Process ## operators in the content
                    Marray(CPPToken) *pasted_content = cpp_get_scratch(cpp);
                    if(!pasted_content){
                        cpp_release_scratch(cpp, content_tokens);
                        return CPP_OOM_ERROR;
                    }
                    for(size_t m = 0; m < content_tokens->count; m++){
                        CPPToken ct = content_tokens->data[m];
                        if(ct.type == CPP_WHITESPACE){
                            // Skip whitespace around ##
                            size_t n = m + 1;
                            while(n < content_tokens->count && content_tokens->data[n].type == CPP_WHITESPACE) n++;
                            if(n < content_tokens->count && content_tokens->data[n].type == CPP_PUNCTUATOR && content_tokens->data[n].punct == '##'){
                                continue; // skip whitespace before ##
                            }
                            err = ma_push(CPPToken)(pasted_content, cpp->allocator, ct);
                            if(err) return err;
                            continue;
                        }
                        if(ct.type == CPP_PUNCTUATOR && ct.punct == '##'){
                            // Skip whitespace after ##
                            size_t n = m + 1;
                            while(n < content_tokens->count && content_tokens->data[n].type == CPP_WHITESPACE) n++;
                            if(pasted_content->count == 0 || n >= content_tokens->count){
                                // ## at start or end - error
                                cpp_release_scratch(cpp, pasted_content);
                                cpp_release_scratch(cpp, content_tokens);
                                cpp_release_scratch(cpp, result);
                                if(expanded_args) Allocator_free(cpp->allocator, expanded_args, sizeof(Marray(CPPToken)*) * total_params);
                                return cpp_error(cpp, ct.loc, "'##' at invalid position in __VA_OPT__ content");
                            }
                            CPPToken left_tok = ma_tail(*pasted_content);
                            pasted_content->count--;
                            CPPToken right_tok = content_tokens->data[n];
                            m = n; // skip to right operand
                            CPPToken paste_result;
                            err = cpp_paste_tokens(cpp, left_tok, right_tok, &paste_result, ct.loc);
                            if(err){
                                cpp_release_scratch(cpp, pasted_content);
                                cpp_release_scratch(cpp, content_tokens);
                                cpp_release_scratch(cpp, result);
                                if(expanded_args) Allocator_free(cpp->allocator, expanded_args, sizeof(Marray(CPPToken)*) * total_params);
                                return err;
                            }
                            err = ma_push(CPPToken)(pasted_content, cpp->allocator, paste_result);
                            if(err) return err;
                            continue;
                        }
                        err = ma_push(CPPToken)(pasted_content, cpp->allocator, ct);
                        if(err) return err;
                    }
                    cpp_release_scratch(cpp, content_tokens);
                    // Remove placemarkers and stringify
                    Marray(CPPToken) *final_content = cpp_get_scratch(cpp);
                    if(!final_content){
                        cpp_release_scratch(cpp, pasted_content);
                        return CPP_OOM_ERROR;
                    }
                    for(size_t m = 0; m < pasted_content->count; m++){
                        if(pasted_content->data[m].type != CPP_PLACEMARKER){
                            err = ma_push(CPPToken)(final_content, cpp->allocator, pasted_content->data[m]);
                            if(err) return err;
                        }
                    }
                    cpp_release_scratch(cpp, pasted_content);
                    stringified = cpp_stringify_argument(cpp, final_content->data, final_content->count, t.loc);
                    cpp_release_scratch(cpp, final_content);
                }
                else {
                    // Empty VA_ARGS -> empty string
                    stringified = (CPPToken){.type = CPP_STRING, .txt = SV("\"\""), .loc = t.loc};
                }
                err = ma_push(CPPToken)(result, cpp->allocator, stringified);
                if(err) return err;
                i = k; // skip past ')'
                continue;
            }

            if(j >= nreplace || repl[j].param_idx == 0){
                cpp_release_scratch(cpp, result);
                if(expanded_args) Allocator_free(cpp->allocator, expanded_args, sizeof(Marray(CPPToken)*) * total_params);
                return cpp_error(cpp, t.loc, "'#' is not followed by a macro parameter");
            }
            // Get raw argument
            size_t param_idx = repl[j].param_idx - 1;
            CPPToken* arg_start;
            size_t arg_count;
            if(param_idx == macro->nparams && macro->is_variadic){
                cpp_get_va_args(args, arg_seps, macro->nparams, &arg_start, &arg_count);
            }
            else {
                cpp_get_argument(args, arg_seps, param_idx, &arg_start, &arg_count);
            }
            CPPToken stringified = cpp_stringify_argument(cpp, arg_start, arg_count, t.loc);
            err = ma_push(CPPToken)(result, cpp->allocator, stringified);
            if(err) return err;
            i = j; // skip to after the param
            continue;
        }

        // Handle __VA_OPT__
        if(t.type == CPP_IDENTIFIER && sv_equals(t.txt, SV("__VA_OPT__"))){
            // Expect ( content )
            size_t j = i + 1;
            while(j < nreplace && repl[j].type == CPP_WHITESPACE) j++;
            if(j >= nreplace || repl[j].type != CPP_PUNCTUATOR || repl[j].punct != '('){
                cpp_release_scratch(cpp, result);
                if(expanded_args) Allocator_free(cpp->allocator, expanded_args, sizeof(Marray(CPPToken)*) * total_params);
                return cpp_error(cpp, t.loc, "__VA_OPT__ must be followed by (content)");
            }
            j++; // skip '('
            size_t content_start = j;
            int paren = 1;
            while(j < nreplace && paren > 0){
                if(repl[j].type == CPP_PUNCTUATOR){
                    if(repl[j].punct == '(') paren++;
                    else if(repl[j].punct == ')') paren--;
                }
                if(paren > 0) j++;
            }
            if(paren != 0){
                cpp_release_scratch(cpp, result);
                if(expanded_args) Allocator_free(cpp->allocator, expanded_args, sizeof(Marray(CPPToken)*) * total_params);
                return cpp_error(cpp, t.loc, "unterminated __VA_OPT__");
            }
            size_t content_end = j; // points to ')'

            // Check if VA_ARGS is non-empty
            if(cpp_va_args_nonempty(cpp, args, arg_seps, macro->nparams)){
                // Include content tokens (they'll be processed in subsequent iterations)
                // We need to recursively process the content
                for(size_t k = content_start; k < content_end; k++){
                    CPPToken ct = repl[k];
                    // Process param substitution within __VA_OPT__ content
                    if(ct.param_idx > 0){
                        size_t pidx = ct.param_idx - 1;
                        CPPToken* arg_start;
                        size_t arg_count;
                        if(pidx == macro->nparams && macro->is_variadic){
                            cpp_get_va_args(args, arg_seps, macro->nparams, &arg_start, &arg_count);
                        }
                        else {
                            cpp_get_argument(args, arg_seps, pidx, &arg_start, &arg_count);
                        }
                        // Check if adjacent to ##
                        _Bool adj_paste = 0;
                        // Look left in content
                        for(size_t m = k; m-- > content_start;){
                            if(repl[m].type == CPP_WHITESPACE) continue;
                            if(repl[m].type == CPP_PUNCTUATOR && repl[m].punct == '##') adj_paste = 1;
                            break;
                        }
                        // Look right in content
                        for(size_t m = k + 1; m < content_end; m++){
                            if(repl[m].type == CPP_WHITESPACE) continue;
                            if(repl[m].type == CPP_PUNCTUATOR && repl[m].punct == '##') adj_paste = 1;
                            break;
                        }

                        if(arg_count == 0){
                            CPPToken pm = {.type = CPP_PLACEMARKER, .loc = ct.loc};
                            err = ma_push(CPPToken)(result, cpp->allocator, pm);
                            if(err) return err;
                        }
                        else if(adj_paste){
                            for(size_t m = 0; m < arg_count; m++){
                                err = ma_push(CPPToken)(result, cpp->allocator, arg_start[m]);
                                if(err) return err;
                            }
                        }
                        else {
                            // Expand the argument
                            if(!expanded_args[pidx]){
                                expanded_args[pidx] = cpp_get_scratch(cpp);
                                if(!expanded_args[pidx]) return CPP_OOM_ERROR;
                                err = cpp_expand_argument(cpp, arg_start, arg_count, expanded_args[pidx]);
                                if(err) return err;
                            }
                            for(size_t m = 0; m < expanded_args[pidx]->count; m++){
                                err = ma_push(CPPToken)(result, cpp->allocator, expanded_args[pidx]->data[m]);
                                if(err) return err;
                            }
                        }
                    }
                    else {
                        err = ma_push(CPPToken)(result, cpp->allocator, ct);
                        if(err) return err;
                    }
                }
            }
            // else: VA_ARGS is empty, skip content entirely
            i = content_end; // skip past ')'
            continue;
        }

        // Handle parameter substitution
        if(t.param_idx > 0){
            size_t param_idx = t.param_idx - 1;
            CPPToken* arg_start;
            size_t arg_count;
            if(param_idx == macro->nparams && macro->is_variadic){
                cpp_get_va_args(args, arg_seps, macro->nparams, &arg_start, &arg_count);
            }
            else {
                cpp_get_argument(args, arg_seps, param_idx, &arg_start, &arg_count);
            }

            // Check if adjacent to ##
            _Bool adj_paste = (needs_raw & (1ULL << param_idx)) != 0;

            if(arg_count == 0){
                // Empty argument -> placemarker
                CPPToken pm = {.type = CPP_PLACEMARKER, .loc = t.loc};
                err = ma_push(CPPToken)(result, cpp->allocator, pm);
                if(err) return err;
            }
            else if(adj_paste){
                // Use raw (unexpanded) argument
                for(size_t j = 0; j < arg_count; j++){
                    err = ma_push(CPPToken)(result, cpp->allocator, arg_start[j]);
                    if(err) return err;
                }
            }
            else {
                // Expand the argument
                if(!expanded_args[param_idx]){
                    expanded_args[param_idx] = cpp_get_scratch(cpp);
                    if(!expanded_args[param_idx]) return CPP_OOM_ERROR;
                    err = cpp_expand_argument(cpp, arg_start, arg_count, expanded_args[param_idx]);
                    if(err) return err;
                }
                if(expanded_args[param_idx]->count == 0){
                    CPPToken pm = {.type = CPP_PLACEMARKER, .loc = t.loc};
                    err = ma_push(CPPToken)(result, cpp->allocator, pm);
                    if(err) return err;
                }
                else {
                    for(size_t j = 0; j < expanded_args[param_idx]->count; j++){
                        err = ma_push(CPPToken)(result, cpp->allocator, expanded_args[param_idx]->data[j]);
                        if(err) return err;
                    }
                }
            }
            continue;
        }

        // Regular token (including ## which we preserve for pasting pass)
        err = ma_push(CPPToken)(result, cpp->allocator, t);
        if(err) return err;
    }

    // Free expanded args
    for(size_t i = 0; i < total_params; i++){
        if(expanded_args && expanded_args[i])
            cpp_release_scratch(cpp, expanded_args[i]);
    }
    if(expanded_args)
        Allocator_free(cpp->allocator, expanded_args, sizeof(Marray(CPPToken)*) * total_params);

    // Pasting pass: process ## operators left-to-right
    Marray(CPPToken) *pasted = cpp_get_scratch(cpp);
    if(!pasted){
        cpp_release_scratch(cpp, result);
        return CPP_OOM_ERROR;
    }

    for(size_t i = 0; i < result->count; i++){
        CPPToken t = result->data[i];
        if(t.type == CPP_WHITESPACE){
            // Skip whitespace around ##
            // Look ahead to see if next non-ws is ##
            size_t j = i + 1;
            while(j < result->count && result->data[j].type == CPP_WHITESPACE) j++;
            if(j < result->count && result->data[j].type == CPP_PUNCTUATOR && result->data[j].punct == '##'){
                // Skip this whitespace
                continue;
            }
            err = ma_push(CPPToken)(pasted, cpp->allocator, t);
            if(err) return err;
            continue;
        }
        if(t.type == CPP_PUNCTUATOR && t.punct == '##'){
            // Token paste operator
            if(pasted->count == 0){
                cpp_release_scratch(cpp, result);
                cpp_release_scratch(cpp, pasted);
                return cpp_error(cpp, t.loc, "'##' cannot appear at start of macro expansion");
            }
            // Skip whitespace after ##
            size_t j = i + 1;
            while(j < result->count && result->data[j].type == CPP_WHITESPACE) j++;
            if(j >= result->count){
                cpp_release_scratch(cpp, result);
                cpp_release_scratch(cpp, pasted);
                return cpp_error(cpp, t.loc, "'##' cannot appear at end of macro expansion");
            }
            // Pop left operand
            CPPToken left = ma_tail(*pasted);
            pasted->count--;
            CPPToken right = result->data[j];
            i = j; // skip to right operand

            CPPToken paste_result;
            err = cpp_paste_tokens(cpp, left, right, &paste_result, t.loc);
            if(err){
                cpp_release_scratch(cpp, result);
                cpp_release_scratch(cpp, pasted);
                return err;
            }
            err = ma_push(CPPToken)(pasted, cpp->allocator, paste_result);
            if(err) return err;
            continue;
        }
        err = ma_push(CPPToken)(pasted, cpp->allocator, t);
        if(err) return err;
    }
    cpp_release_scratch(cpp, result);

    // Remove placemarkers and push result in reverse order
    macro->is_disabled = 1;
    CPPToken reenable = {.type = CPP_REENABLE, .data1 = macro};
    err = ma_push(CPPToken)(dst, cpp->allocator, reenable);
    if(err) return err;

    for(size_t i = pasted->count; i-- > 0;){
        CPPToken t = pasted->data[i];
        // Skip placemarkers
        if(t.type == CPP_PLACEMARKER)
            continue;
        // Paint blue any tokens matching disabled macros
        if(t.type == CPP_IDENTIFIER && !t.disabled){
            Atom a = AT_get_atom(cpp->at, t.txt.text, t.txt.length);
            if(a){
                CMacro* m = AM_get(&cpp->macros, a);
                if(m && m->is_disabled)
                    t.disabled = 1;
            }
        }
        err = ma_push(CPPToken)(dst, cpp->allocator, t);
        if(err) return err;
    }

    cpp_release_scratch(cpp, pasted);
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
