#ifndef C_CPP_PREPROCESSOR_C
#define C_CPP_PREPROCESSOR_C
//
// Copyright © 2026-2026, David Priver <david@davidpriver.com>
//
#include <stdarg.h>
#include "cpp_preprocessor.h"
#include "cpp_tok.h"
#include "../Drp/msb_sprintf.h"
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
LOG_PRINTF(3, 4) static int cpp_error(CPreprocessor*, SrcLoc, const char*, ...);
LOG_PRINTF(3, 4) static void cpp_warn(CPreprocessor*, SrcLoc, const char*, ...);
LOG_PRINTF(3, 4) static void cpp_info(CPreprocessor*, SrcLoc, const char*, ...);

static CPPTokens*_Nullable cpp_get_scratch(CPreprocessor*);
static Marray(size_t)*_Nullable cpp_get_scratch_idxes(CPreprocessor*);
static void cpp_release_scratch(CPreprocessor*, CPPTokens*);
static void cpp_release_scratch_idxes(CPreprocessor*, Marray(size_t)*);
static int cpp_substitute_and_paste(CPreprocessor*, const CPPToken*, size_t, const CMacro*, const CPPTokens*, const Marray(size_t)*, CPPTokens*_Nullable*_Null_unspecified, CPPTokens*, _Bool);
LOG_PRINTF(2, 3) static Atom _Nullable cpp_atomizef(CPreprocessor*, const char* fmt, ...);
enum {
    CPP_NO_ERROR = 0,
    CPP_OOM_ERROR,
    CPP_MACRO_ALREADY_EXISTS_ERROR,
    CPP_REDEFINING_BUILTIN_MACRO_ERROR,
    CPP_SYNTAX_ERROR,
    CPP_UNREACHABLE_ERROR,
    CPP_UNIMPLEMENTED_ERROR,
};

static
int
cpp_push_tok(CPreprocessor* cpp, CPPTokens* dst, CPPToken tok){
    int err = ma_push (CPPToken)(dst, cpp->allocator, tok);
    return err;
}

static
int
cpp_define_macro(CPreprocessor* cpp, StringView name, size_t ntoks, size_t nparams, CMacro*_Nullable*_Nonnull outmacro){
    Atom key = AT_atomize(cpp->at, name.text, name.length);
    if(!key) return CPP_OOM_ERROR;
    CMacro* macro = AM_get(&cpp->macros, key);
    if(macro) return macro->is_builtin?CPP_REDEFINING_BUILTIN_MACRO_ERROR:CPP_MACRO_ALREADY_EXISTS_ERROR;
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
void
cpp_free_macro(CPreprocessor* cpp, CMacro * macro){
    size_t size;
    if(macro->is_builtin){
        size = sizeof *macro + sizeof(CPPToken)*2 + sizeof(Atom)*macro->nparams;
    }
    else
        size = sizeof *macro + sizeof(CPPToken)*macro->nreplace + sizeof(Atom)*macro->nparams;
    Allocator_free(cpp->allocator, macro, size);
}

static
int
cpp_undef_macro(CPreprocessor* cpp, StringView name){
    Atom key = AT_get_atom(cpp->at, name.text, name.length);
    if(!key) return 0;
    CMacro* macro = AM_get(&cpp->macros, key);
    if(!macro) return 0;
    cpp_free_macro(cpp, macro);
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
int
cpp_define_builtin_obj_macro(CPreprocessor* cpp, StringView name, CppObjMacroFn* fn, void*_Null_unspecified ctx){
    CMacro* macro;
    int err = cpp_define_macro(cpp, name, 2, 0, &macro);
    if(err) return err;
    macro->is_builtin = 1;
    macro->nreplace = 0;
    _Static_assert(sizeof(uint64_t) >= sizeof(void(*)(void)), "ctx doesn't fit in uint64");
    macro->data[0] = (uint64_t)fn;
    _Static_assert(sizeof(uint64_t) >= sizeof(void*), "fn doesn't fit in uint64");
    macro->data[1] = (uint64_t)ctx;
    return 0;
}

static
int
cpp_define_builtin_func_macro(CPreprocessor* cpp, StringView name, CppFuncMacroFn* fn, void*_Null_unspecified ctx, size_t nparams, _Bool variadic, _Bool no_expand){
    CMacro* macro;
    int err = cpp_define_macro(cpp, name, 2, nparams, &macro);
    if(err) return err;
    macro->is_builtin = 1;
    macro->is_function_like = 1;
    macro->is_variadic = variadic;
    macro->no_expand_args = no_expand;
    macro->nreplace = 0;
    _Static_assert(sizeof(uint64_t) >= sizeof(void(*)(void)), "ctx doesn't fit in uint64");
    macro->data[0] = (uint64_t)fn;
    _Static_assert(sizeof(uint64_t) >= sizeof(void*), "fn doesn't fit in uint64");
    macro->data[1] = (uint64_t)ctx;
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
static int cpp_expand_obj_macro(CPreprocessor *cpp, CMacro *macro, CPPTokens *dst);
static int cpp_expand_func_macro(CPreprocessor *cpp, CMacro *macro, const CPPTokens *args, const Marray(size_t) *arg_seps, CPPTokens *dst);

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
                        err = cpp_push_tok(cpp, &cpp->pending, next);
                        if(err) return err;
                        goto noexp;
                    }
                    CPPTokens *args = cpp_get_scratch(cpp);
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
                        err = cpp_push_tok(cpp, args, next);
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
        return cpp_push_tok(cpp, &cpp->pending, tok);
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
            if(err == CPP_REDEFINING_BUILTIN_MACRO_ERROR)
                return cpp_error(cpp, tok.loc, "Redefining builtin macro (%.*s)", sv_p(name));
            if(err == CPP_MACRO_ALREADY_EXISTS_ERROR){
                Atom a = AT_get_atom(cpp->at, name.text, name.length);
                if(!a) return CPP_UNREACHABLE_ERROR;
                CMacro* m = AM_get(&cpp->macros, a);
                if(!m) return CPP_UNREACHABLE_ERROR;
                if(m->nparams || m->is_function_like || m->nreplace)
                    return cpp_error(cpp, tok.loc, "Duplicate object-like macro (%.*s) with different definitions", sv_p(name));
                err = 0;
            }
            if(err) return err;
            // push it back so dispatch loop sees newline
            return cpp_push_tok(cpp, &cpp->pending, tok);
        }
        else if(tok.type == CPP_PUNCTUATOR && tok.punct == '('){
            CPPTokens *names = cpp_get_scratch(cpp);
            if(!names) return CPP_OOM_ERROR;
            CPPTokens *repl = cpp_get_scratch(cpp);
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
                err = cpp_push_tok(cpp, names, tok);
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
                    err = cpp_push_tok(cpp, repl, tok);
                    if(err) return err;
                }
                err = cpp_next_raw_token(cpp, &tok);
                if(err) return err;
            }
            // push it back so dispatch loop sees newline
            err = cpp_push_tok(cpp, &cpp->pending, tok);
            if(err) return err;
            while(repl->count && ma_tail(*repl).type == CPP_WHITESPACE)
                repl->count--;
            CMacro* m;
            err = cpp_define_macro(cpp, name, repl->count, names->count, &m);
            if(err == CPP_REDEFINING_BUILTIN_MACRO_ERROR)
                return cpp_error(cpp, tok.loc, "Redefining builtin macro (%.*s)", sv_p(name));
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
            // Check ## not at start/end of replacement list (C23 6.10.4.3)
            if(repl->count){
                size_t first = 0;
                while(first < repl->count && repl->data[first].type == CPP_WHITESPACE) first++;
                if(first < repl->count && repl->data[first].type == CPP_PUNCTUATOR && repl->data[first].punct == '##'){
                    cpp_release_scratch(cpp, repl);
                    cpp_release_scratch(cpp, names);
                    return cpp_error(cpp, repl->data[first].loc, "'##' cannot appear at start of replacement list");
                }
                size_t last = repl->count;
                while(last > 0 && repl->data[last-1].type == CPP_WHITESPACE) last--;
                if(last > 0 && repl->data[last-1].type == CPP_PUNCTUATOR && repl->data[last-1].punct == '##'){
                    cpp_release_scratch(cpp, repl);
                    cpp_release_scratch(cpp, names);
                    return cpp_error(cpp, repl->data[last-1].loc, "'##' cannot appear at end of replacement list");
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
                err = cpp_push_tok(cpp, &cpp->pending, tok);
                if(err) return err;
            }
            CPPTokens *repl = cpp_get_scratch(cpp);
            if(!repl) return CPP_OOM_ERROR;
            for(;;){
                err = cpp_next_raw_token(cpp, &tok);
                if(err) return err;
                if(tok.type == CPP_EOF || tok.type == CPP_NEWLINE){
                    // push it back so dispatch loop sees newline
                    err = cpp_push_tok(cpp, &cpp->pending, tok);
                    if(err) return err;
                    break;
                }
                if(tok.type == CPP_WHITESPACE && repl->count && ma_tail(*repl).type == CPP_WHITESPACE){
                    // coalesce whitespace in #defines
                }
                else {
                    err = cpp_push_tok(cpp, repl, tok);
                    if(err) return err;
                }
            }
            while(repl->count && ma_tail(*repl).type == CPP_WHITESPACE)
                repl->count--;
            // Check ## not at start/end of replacement list (C23 6.10.4.3)
            if(repl->count){
                size_t first = 0;
                while(first < repl->count && repl->data[first].type == CPP_WHITESPACE) first++;
                if(first < repl->count && repl->data[first].type == CPP_PUNCTUATOR && repl->data[first].punct == '##'){
                    cpp_release_scratch(cpp, repl);
                    return cpp_error(cpp, repl->data[first].loc, "'##' cannot appear at start of replacement list");
                }
                size_t last = repl->count;
                while(last > 0 && repl->data[last-1].type == CPP_WHITESPACE) last--;
                if(last > 0 && repl->data[last-1].type == CPP_PUNCTUATOR && repl->data[last-1].punct == '##'){
                    cpp_release_scratch(cpp, repl);
                    return cpp_error(cpp, repl->data[last-1].loc, "'##' cannot appear at end of replacement list");
                }
            }
            err = cpp_define_obj_macro(cpp, name, repl->data, repl->count);
            if(err == CPP_REDEFINING_BUILTIN_MACRO_ERROR)
                return cpp_error(cpp, tok.loc, "Redefining builtin macro (%.*s)", sv_p(name));
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
                err = cpp_push_tok(cpp, &cpp->pending, tok);
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
cpp_expand_obj_macro(CPreprocessor *cpp, CMacro *macro, CPPTokens *dst){
    if(macro->is_builtin){
        CppObjMacroFn* fn = (CppObjMacroFn*)macro->data[0];
        void* ctx = (void*) macro->data[1];
        if(!fn) return CPP_UNREACHABLE_ERROR;
        SrcLoc loc = {0}; // TODO thread expansion loc through with chaining
        return fn(ctx, cpp, loc, dst);
    }
    macro->is_disabled = 1;
    CPPToken reenable = {.type = CPP_REENABLE, .data1 = macro};
    int err = cpp_push_tok(cpp, dst, reenable);
    if(err) return err;

    CPPToken* repl = cpp_cmacro_replacement(macro);

    // Check if replacement list contains ## (needs paste processing)
    _Bool has_paste = 0;
    for(size_t i = 0; i < macro->nreplace; i++){
        if(repl[i].type == CPP_PUNCTUATOR && repl[i].punct == '##'){
            has_paste = 1;
            break;
        }
    }

    if(has_paste){
        // Process ## pasting via cpp_substitute_and_paste.
        // Object-like macros have no params, so args/expanded_args are unused.
        CPPTokens empty_args = {0};
        Marray(size_t) empty_seps = {0};
        CPPTokens *result = cpp_get_scratch(cpp);
        if(!result) return CPP_OOM_ERROR;
        err = cpp_substitute_and_paste(cpp, repl, macro->nreplace, macro, &empty_args, &empty_seps, NULL, result, 0);
        if(err) goto finally_obj;
        for(size_t i = result->count; i-- > 0;){
            CPPToken tok = result->data[i];
            if(tok.type == CPP_PLACEMARKER) continue;
            if(tok.type == CPP_IDENTIFIER && !tok.disabled){
                Atom a = AT_get_atom(cpp->at, tok.txt.text, tok.txt.length);
                if(a){
                    CMacro* m = AM_get(&cpp->macros, a);
                    if(m && m->is_disabled)
                        tok.disabled = 1;
                }
            }
            err = cpp_push_tok(cpp, dst, tok);
            if(err) goto finally_obj;
        }
    finally_obj:
        cpp_release_scratch(cpp, result);
        return err;
    }

    // Fast path: no ## processing needed
    for(size_t i = macro->nreplace; i-- > 0;){
        CPPToken tok = repl[i];
        if(tok.type == CPP_IDENTIFIER && !tok.disabled){
            Atom a = AT_get_atom(cpp->at, tok.txt.text, tok.txt.length);
            if(a){
                CMacro* m = AM_get(&cpp->macros, a);
                if(m && m->is_disabled)
                    tok.disabled = 1;
            }
        }
        err = cpp_push_tok(cpp, dst, tok);
        if(err) return err;
    }
    return 0;
}

// Helper: Get argument N from args array using arg_seps indices
// arg_seps[i] points to the comma token in args; arg0 = args[0..arg_seps[0]),
// arg1 = args[arg_seps[0]+1..arg_seps[1]), etc. (skip the comma)
static
void
cpp_get_argument(const CPPTokens *args, const Marray(size_t) *arg_seps, size_t arg_idx, CPPToken*_Nullable*_Nonnull out_start, size_t *out_count){
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
cpp_get_va_args(const CPPTokens *args, const Marray(size_t) *arg_seps, size_t nparams, CPPToken*_Nullable*_Nonnull out_start, size_t *out_count){
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

static int cpp_expand_argument(CPreprocessor *cpp, CPPToken*_Null_unspecified toks, size_t count, CPPTokens *out);

// Helper: Check if VA_ARGS is non-empty after expansion (C23 6.10.4.1).
// Uses the expanded_args cache so the expansion is done at most once.
static
_Bool
cpp_va_args_nonempty(CPreprocessor *cpp, const CMacro *macro, const CPPTokens *args, const Marray(size_t) *arg_seps, CPPTokens *_Nullable*_Null_unspecified expanded_args){
    CPPToken* start;
    size_t count;
    cpp_get_va_args(args, arg_seps, macro->nparams, &start, &count);
    if(!count) return 0;

    size_t va_idx = macro->nparams; // VA_ARGS slot in expanded_args
    if(!expanded_args[va_idx]){
        CPPTokens *ea = cpp_get_scratch(cpp);
        if(!ea) return 0; // conservative: treat as empty on OOM
        int err = cpp_expand_argument(cpp, start, count, ea);
        if(err){
            cpp_release_scratch(cpp, ea);
            return 0;
        }
        expanded_args[va_idx] = ea;
    }
    CPPTokens *expanded = expanded_args[va_idx];
    for(size_t i = 0; i < expanded->count; i++){
        if(expanded->data[i].type != CPP_WHITESPACE &&
           expanded->data[i].type != CPP_NEWLINE &&
           expanded->data[i].type != CPP_PLACEMARKER)
            return 1;
    }
    return 0;
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
        return cpp_error(cpp, loc, "pasting \"%.*s\" and \"%.*s\" does not give a valid preprocessing token", sv_p(left.txt), sv_p(right.txt));
    }

    tok.loc = loc;
    *result = tok;
    return 0;
}

// Helper: Expand argument through prescan
static
int
cpp_expand_argument(CPreprocessor *cpp, CPPToken*_Null_unspecified toks, size_t count, CPPTokens *out){
    if(!count) return 0;

    // Push EOF marker first (will be at bottom of stack)
    CPPToken end_marker = {.type = CPP_EOF};
    int err = cpp_push_tok(cpp, &cpp->pending, end_marker);
    if(err) return err;

    // Push tokens in reverse order (so they come out in forward order)
    for(size_t i = count; i-- > 0;){
        err = cpp_push_tok(cpp, &cpp->pending, toks[i]);
        if(err) return err;
    }

    // Pump through cpp_next_pp_token until we hit the EOF marker
    for(;;){
        CPPToken tok;
        err = cpp_next_pp_token(cpp, &tok);
        if(err) return err;
        if(tok.type == CPP_EOF) break;
        err = cpp_push_tok(cpp, out, tok);
        if(err) return err;
    }
    return 0;
}

// Helper: Get raw argument tokens for a parameter index, dispatching
// between variadic and regular arguments.
static inline
void
cpp_get_param_arg(const CMacro *macro, const CPPTokens *args, const Marray(size_t) *arg_seps, size_t pidx, CPPToken*_Nullable*_Nonnull out_start, size_t *out_count){
    if(pidx == macro->nparams && macro->is_variadic)
        cpp_get_va_args(args, arg_seps, macro->nparams, out_start, out_count);
    else
        cpp_get_argument(args, arg_seps, pidx, out_start, out_count);
}

// Helper: Parse __VA_OPT__(content) starting after the __VA_OPT__ identifier.
// Sets *out_content_start to the first token after '(' and *out_close_paren
// to the index of the matching ')'.
static
int
cpp_parse_va_opt_content(CPreprocessor *cpp, const CPPToken *repl, size_t nreplace, size_t after_va_opt, SrcLoc loc, size_t *out_content_start, size_t *out_close_paren){
    size_t k = after_va_opt;
    while(k < nreplace && repl[k].type == CPP_WHITESPACE) k++;
    if(k >= nreplace || repl[k].type != CPP_PUNCTUATOR || repl[k].punct != '(')
        return cpp_error(cpp, loc, "__VA_OPT__ must be followed by (content)");
    k++; // skip '('
    *out_content_start = k;
    int paren = 1;
    while(k < nreplace && paren > 0){
        if(repl[k].type == CPP_PUNCTUATOR){
            if(repl[k].punct == '(') paren++;
            else if(repl[k].punct == ')') paren--;
        }
        if(paren > 0) k++;
    }
    if(paren != 0)
        return cpp_error(cpp, loc, "unterminated __VA_OPT__");
    *out_close_paren = k;
    return 0;
}

// Single-pass helper: substitute parameters and resolve ## pasting.
// Walks repl[0..nreplace) left-to-right, handling:
//   - # stringification (param and __VA_OPT__)
//   - __VA_OPT__ (recursive)
//   - ## token pasting
//   - parameter substitution (expanded vs raw based on local ## adjacency)
// If raw_only is set, all params use raw (unexpanded) tokens (for # __VA_OPT__).
static
int
cpp_substitute_and_paste(
    CPreprocessor *cpp,
    const CPPToken *repl, size_t nreplace,
    const CMacro *macro,
    const CPPTokens *args,
    const Marray(size_t) *arg_seps,
    CPPTokens *_Nullable*_Null_unspecified expanded_args,
    CPPTokens *out,
    _Bool raw_only)
{
    int err;
    for(size_t i = 0; i < nreplace; i++){
        CPPToken t = repl[i];

        // # (stringify) — only in function-like macros
        if(t.type == CPP_PUNCTUATOR && t.punct == '#' && macro->is_function_like){
            size_t j = i + 1;
            while(j < nreplace && repl[j].type == CPP_WHITESPACE) j++;

            // # __VA_OPT__(content)
            if(j < nreplace && repl[j].type == CPP_IDENTIFIER && sv_equals(repl[j].txt, SV("__VA_OPT__"))){
                size_t cstart, cparen;
                err = cpp_parse_va_opt_content(cpp, repl, nreplace, j+1, repl[j].loc, &cstart, &cparen);
                if(err) return err;
                CPPToken stringified;
                if(cpp_va_args_nonempty(cpp, macro, args, arg_seps, expanded_args)){
                    CPPTokens *temp = cpp_get_scratch(cpp);
                    if(!temp) return CPP_OOM_ERROR;
                    err = cpp_substitute_and_paste(cpp, repl+cstart, cparen-cstart, macro, args, arg_seps, expanded_args, temp, 1);
                    if(err){ cpp_release_scratch(cpp, temp); return err; }
                    // Strip placemarkers in-place before stringifying
                    size_t w = 0;
                    for(size_t m = 0; m < temp->count; m++)
                        if(temp->data[m].type != CPP_PLACEMARKER)
                            temp->data[w++] = temp->data[m];
                    stringified = cpp_stringify_argument(cpp, temp->data, w, t.loc);
                    cpp_release_scratch(cpp, temp);
                }
                else {
                    stringified = (CPPToken){.type = CPP_STRING, .txt = SV("\"\""), .loc = t.loc};
                }
                err = cpp_push_tok(cpp, out, stringified);
                if(err) return err;
                i = cparen;
                continue;
            }

            // # param
            if(j >= nreplace || repl[j].param_idx == 0)
                return cpp_error(cpp, t.loc, "'#' is not followed by a macro parameter");
            size_t pidx = repl[j].param_idx - 1;
            CPPToken *arg_start; size_t arg_count;
            cpp_get_param_arg(macro, args, arg_seps, pidx, &arg_start, &arg_count);
            CPPToken stringified = cpp_stringify_argument(cpp, arg_start, arg_count, t.loc);
            err = cpp_push_tok(cpp, out, stringified);
            if(err) return err;
            i = j;
            continue;
        }

        // ## (paste)
        if(t.type == CPP_PUNCTUATOR && t.punct == '##'){
            // C23 6.10.4.1p5: if ## has no left operand (empty __VA_OPT__
            // vanished), delete the ## and its trailing whitespace, letting
            // the right operand be processed normally by the next iteration.
            if(out->count == 0){
                while(i + 1 < nreplace && repl[i + 1].type == CPP_WHITESPACE) i++;
                continue;
            }
            CPPToken left = ma_tail(*out);
            out->count--;

            size_t j = i + 1;
            while(j < nreplace && repl[j].type == CPP_WHITESPACE) j++;
            // Similarly, if ## has no right operand, delete it and keep left.
            if(j >= nreplace){
                err = cpp_push_tok(cpp, out, left);
                if(err) return err;
                continue;
            }

            CPPToken right;
            size_t skip_to = j;

            if(repl[j].param_idx > 0){
                // Right operand is a param — use raw tokens
                size_t pidx = repl[j].param_idx - 1;
                CPPToken *arg_start; size_t arg_count;
                cpp_get_param_arg(macro, args, arg_seps, pidx, &arg_start, &arg_count);
                // GNU extension: , ## __VA_ARGS__
                // When left is comma and right is __VA_ARGS__:
                //   empty: suppress both comma and args
                //   non-empty: emit comma then args (no pasting)
                if(pidx == macro->nparams && macro->is_variadic
                        && left.type == CPP_PUNCTUATOR && left.punct == ','){
                    if(arg_count > 0){
                        err = cpp_push_tok(cpp, out, left);
                        if(err) return err;
                        for(size_t m = 0; m < arg_count; m++){
                            err = cpp_push_tok(cpp, out, arg_start[m]);
                            if(err) return err;
                        }
                    }
                    i = skip_to;
                    continue;
                }
                right = (arg_count == 0)
                    ? (CPPToken){.type = CPP_PLACEMARKER, .loc = repl[j].loc}
                    : arg_start[0];
                CPPToken pr;
                err = cpp_paste_tokens(cpp, left, right, &pr, t.loc);
                if(err) return err;
                err = cpp_push_tok(cpp, out, pr);
                if(err) return err;
                for(size_t m = 1; m < arg_count; m++){
                    err = cpp_push_tok(cpp, out, arg_start[m]);
                    if(err) return err;
                }
                i = skip_to;
                continue;
            }

            if(repl[j].type == CPP_IDENTIFIER && sv_equals(repl[j].txt, SV("__VA_OPT__"))){
                // Right operand is __VA_OPT__
                size_t cstart, cparen;
                err = cpp_parse_va_opt_content(cpp, repl, nreplace, j+1, repl[j].loc, &cstart, &cparen);
                if(err) return err;
                if(cpp_va_args_nonempty(cpp, macro, args, arg_seps, expanded_args)){
                    CPPTokens *temp = cpp_get_scratch(cpp);
                    if(!temp) return CPP_OOM_ERROR;
                    err = cpp_substitute_and_paste(cpp, repl+cstart, cparen-cstart, macro, args, arg_seps, expanded_args, temp, raw_only);
                    if(err) goto finally_paste_va_opt;
                    right = (temp->count == 0)
                        ? (CPPToken){.type = CPP_PLACEMARKER, .loc = repl[j].loc}
                        : temp->data[0];
                    CPPToken pr;
                    err = cpp_paste_tokens(cpp, left, right, &pr, t.loc);
                    if(err) goto finally_paste_va_opt;
                    err = cpp_push_tok(cpp, out, pr);
                    if(err) goto finally_paste_va_opt;
                    for(size_t m = 1; m < temp->count; m++){
                        err = cpp_push_tok(cpp, out, temp->data[m]);
                        if(err) goto finally_paste_va_opt;
                    }
                finally_paste_va_opt:
                    cpp_release_scratch(cpp, temp);
                    if(err) return err;
                }
                else {
                    right = (CPPToken){.type = CPP_PLACEMARKER, .loc = repl[j].loc};
                    CPPToken pr;
                    err = cpp_paste_tokens(cpp, left, right, &pr, t.loc);
                    if(err) return err;
                    err = cpp_push_tok(cpp, out, pr);
                    if(err) return err;
                }
                i = cparen;
                continue;
            }

            // Regular token as right operand
            right = repl[j];
            CPPToken pr;
            err = cpp_paste_tokens(cpp, left, right, &pr, t.loc);
            if(err) return err;
            err = cpp_push_tok(cpp, out, pr);
            if(err) return err;
            i = skip_to;
            continue;
        }

        // __VA_OPT__
        if(t.type == CPP_IDENTIFIER && sv_equals(t.txt, SV("__VA_OPT__"))){
            size_t cstart, cparen;
            err = cpp_parse_va_opt_content(cpp, repl, nreplace, i+1, t.loc, &cstart, &cparen);
            if(err) return err;
            if(cpp_va_args_nonempty(cpp, macro, args, arg_seps, expanded_args)){
                err = cpp_substitute_and_paste(cpp, repl+cstart, cparen-cstart, macro, args, arg_seps, expanded_args, out, raw_only);
                if(err) return err;
            }
            i = cparen;
            continue;
        }

        // Parameter substitution
        if(t.param_idx > 0){
            size_t pidx = t.param_idx - 1;
            CPPToken *arg_start; size_t arg_count;
            cpp_get_param_arg(macro, args, arg_seps, pidx, &arg_start, &arg_count);

            // Check if right-adjacent to ## (next non-WS in repl is ##).
            // Left-adjacency is handled by the ## case pulling the right operand directly.
            _Bool use_raw = raw_only;
            if(!use_raw){
                for(size_t j = i + 1; j < nreplace; j++){
                    if(repl[j].type == CPP_WHITESPACE) continue;
                    if(repl[j].type == CPP_PUNCTUATOR && repl[j].punct == '##') use_raw = 1;
                    break;
                }
            }

            if(arg_count == 0){
                CPPToken pm = {.type = CPP_PLACEMARKER, .loc = t.loc};
                err = cpp_push_tok(cpp, out, pm);
                if(err) return err;
            }
            else if(use_raw){
                for(size_t j = 0; j < arg_count; j++){
                    err = cpp_push_tok(cpp, out, arg_start[j]);
                    if(err) return err;
                }
            }
            else {
                // Expand argument (lazily cached)
                if(!expanded_args[pidx]){
                    CPPTokens *ea = cpp_get_scratch(cpp);
                    if(!ea) return CPP_OOM_ERROR;
                    err = cpp_expand_argument(cpp, arg_start, arg_count, ea);
                    if(err) return err;
                    expanded_args[pidx] = ea;
                }
                CPPTokens *expanded = expanded_args[pidx];
                if(expanded->count == 0){
                    CPPToken pm = {.type = CPP_PLACEMARKER, .loc = t.loc};
                    err = cpp_push_tok(cpp, out, pm);
                    if(err) return err;
                }
                else {
                    for(size_t j = 0; j < expanded->count; j++){
                        err = cpp_push_tok(cpp, out, expanded->data[j]);
                        if(err) return err;
                    }
                }
            }
            continue;
        }

        // Skip whitespace before ##
        if(t.type == CPP_WHITESPACE){
            size_t j = i + 1;
            while(j < nreplace && repl[j].type == CPP_WHITESPACE) j++;
            if(j < nreplace && repl[j].type == CPP_PUNCTUATOR && repl[j].punct == '##')
                continue;
        }

        // Regular token
        err = cpp_push_tok(cpp, out, t);
        if(err) return err;
    }
    return 0;
}

static
int
cpp_expand_func_macro(CPreprocessor *cpp, CMacro *macro, const CPPTokens *args, const Marray(size_t) *arg_seps, CPPTokens *dst){
    if(macro->is_builtin){
        CppFuncMacroFn *fn = (CppFuncMacroFn*)macro->data[0];
        void* ctx = (void*)macro->data[1];
        if(!fn) return CPP_UNREACHABLE_ERROR;
        SrcLoc loc = {0}; // TODO thread expansion loc through with chaining
        if(macro->no_expand_args)
            return fn(ctx, cpp, loc, dst, args, arg_seps);
        else
            return CPP_UNIMPLEMENTED_ERROR;
    }
    int err;
    size_t total_params = macro->nparams + (macro->is_variadic ? 1 : 0);
    CPPTokens **expanded_args = NULL;
    CPPTokens *result = NULL;

    if(total_params){
        expanded_args = Allocator_zalloc(cpp->allocator, sizeof(CPPTokens*) * total_params);
        if(!expanded_args){ err = CPP_OOM_ERROR; goto finally; }
    }

    result = cpp_get_scratch(cpp);
    if(!result){ err = CPP_OOM_ERROR; goto finally; }

    err = cpp_substitute_and_paste(cpp, cpp_cmacro_replacement(macro), macro->nreplace, macro, args, arg_seps, expanded_args, result, 0);
    if(err) goto finally;

    // Push reenable token and results (reversed, painted blue, placemarkers stripped)
    macro->is_disabled = 1;
    CPPToken reenable = {.type = CPP_REENABLE, .data1 = macro};
    err = cpp_push_tok(cpp, dst, reenable);
    if(err) goto finally;

    for(size_t i = result->count; i-- > 0;){
        CPPToken t = result->data[i];
        if(t.type == CPP_PLACEMARKER)
            continue;
        if(t.type == CPP_IDENTIFIER && !t.disabled){
            Atom a = AT_get_atom(cpp->at, t.txt.text, t.txt.length);
            if(a){
                CMacro* m = AM_get(&cpp->macros, a);
                if(m && m->is_disabled)
                    t.disabled = 1;
            }
        }
        err = cpp_push_tok(cpp, dst, t);
        if(err) goto finally;
    }

finally:
    if(result)
        cpp_release_scratch(cpp, result);
    for(size_t i = 0; i < total_params; i++)
        if(expanded_args && expanded_args[i])
            cpp_release_scratch(cpp, expanded_args[i]);
    if(expanded_args)
        Allocator_free(cpp->allocator, expanded_args, sizeof(CPPTokens*) * total_params);
    return err;
}

static
CPPTokens*_Nullable
cpp_get_scratch(CPreprocessor *cpp){
    CPPTokens *scratch = fl_pop(&cpp->scratch_list);
    if(!scratch) scratch = Allocator_zalloc(cpp->allocator, sizeof *scratch);
    if(!scratch) return NULL;
    scratch->count = 0;
    return scratch;
}
static
void
cpp_release_scratch(CPreprocessor *cpp, CPPTokens *scratch){
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


static CppObjMacroFn cpp_builtin_file,
                     cpp_builtin_line,
                     cpp_builtin_counter
                     ;

static
int
cpp_define_builtin_macros(CPreprocessor* cpp){
    static struct {
        StringView name; CppObjMacroFn* fn;
    } obj_builtins[] = {
        {SV("__FILE__"), cpp_builtin_file},
        {SV("__LINE__"), cpp_builtin_line},
        {SV("__COUNTER__"), cpp_builtin_counter},
    };
    for(size_t i = 0; i < sizeof obj_builtins / sizeof obj_builtins[0]; i++){
        int err = cpp_define_builtin_obj_macro(cpp, obj_builtins[i].name, obj_builtins[i].fn, NULL);
        if(err) return err;
    }
    return 0;
}
static
int
cpp_builtin_file(void* _Null_unspecified ctx, CPreprocessor* cpp, SrcLoc loc, CPPTokens* outtoks){
    (void)ctx;
    if(!cpp->frames.count) return CPP_UNREACHABLE_ERROR;
    uint32_t file_id = ma_tail(cpp->frames).file_id;
    LongString path = file_id < cpp->fc->map.count?cpp->fc->map.data[file_id].path:LS("???");
    Atom a = cpp_atomizef(cpp, "\"%s\"", path.text);
    if(!a) return CPP_OOM_ERROR;
    CPPToken tok = {
        .txt = {a->length, a->data},
        .loc = loc,
        .type = CPP_STRING,
    };
    int err = cpp_push_tok(cpp, outtoks, tok);
    return err;
}

static
int
cpp_builtin_line(void* _Null_unspecified ctx, CPreprocessor* cpp, SrcLoc loc, CPPTokens* outtoks){
    (void)ctx;
    if(!cpp->frames.count) return CPP_UNREACHABLE_ERROR;
    Atom a = cpp_atomizef(cpp, "%u", (unsigned)ma_tail(cpp->frames).line);
    if(!a) return CPP_OOM_ERROR;
    CPPToken tok = {
        .txt = {a->length, a->data},
        .loc = loc,
        .type = CPP_STRING,
    };
    int err = cpp_push_tok(cpp, outtoks, tok);
    return err;
}
static
int
cpp_builtin_counter(void* _Null_unspecified ctx, CPreprocessor* cpp, SrcLoc loc, CPPTokens* outtoks){
    (void)ctx;
    uint64_t c = cpp->counter++;
    Atom a = cpp_atomizef(cpp, "%llu", (unsigned long long)c);
    if(!a) return CPP_OOM_ERROR;
    CPPToken tok = {
        .txt = {a->length, a->data},
        .loc = loc,
        .type = CPP_STRING,
    };
    int err = cpp_push_tok(cpp, outtoks, tok);
    return err;
}
static
Atom _Nullable
cpp_atomizef(CPreprocessor* cpp, const char* fmt, ...){
    Atom a = NULL;
    MStringBuilder sb = {.allocator = allocator_from_arena(&cpp->synth_arena)};
    va_list va;
    va_start(va, fmt);
    msb_vsprintf(&sb, fmt, va);
    va_end(va);
    if(sb.errored)
        goto finally;
    StringView sv = msb_borrow_sv(&sb);
    a = AT_atomize(cpp->at, sv.text, sv.length);
    finally:
    msb_destroy(&sb);
    return a;
}

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#ifdef __clang__
#pragma clang assume_nonnull end
#endif
#endif
