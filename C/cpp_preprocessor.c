#ifndef C_CPP_PREPROCESSOR_C
#define C_CPP_PREPROCESSOR_C
//
// Copyright © 2026-2026, David Priver <david@davidpriver.com>
//
#include <stdarg.h>
#include "cpp_preprocessor.h"
#include "cpp_tok.h"
#include "../Drp/msb_sprintf.h"
#include "../Drp/path_util.h"
#include "../Drp/parse_numbers.h"
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
static int cpp_push_if(CPreprocessor* cpp, CppPoundIf s);

static CPPTokens*_Nullable cpp_get_scratch(CPreprocessor*);
static Marray(size_t)*_Nullable cpp_get_scratch_idxes(CPreprocessor*);
static void cpp_release_scratch(CPreprocessor*, CPPTokens*);
static void cpp_release_scratch_idxes(CPreprocessor*, Marray(size_t)*);
static int cpp_substitute_and_paste(CPreprocessor*, const CPPToken*, size_t, const CMacro*, const CPPTokens*, const Marray(size_t)*, CPPTokens*_Nullable*_Null_unspecified, CPPTokens*, _Bool, SrcLocExp*);
static int cpp_expand_argument(CPreprocessor *cpp, CPPToken*_Null_unspecified toks, size_t count, CPPTokens *out);
LOG_PRINTF(2, 3) static Atom _Nullable cpp_atomizef(CPreprocessor*, const char* fmt, ...);
static int cpp_eval_tokens(CPreprocessor*, CPPToken*_Null_unspecified toks, size_t count, int64_t* value);
// str should exclude outer quotes
static int cpp_mixin_string(CPreprocessor* cpp, SrcLoc loc, StringView str, CPPTokens* out);
enum {
    CPP_NO_ERROR = 0,
    CPP_OOM_ERROR,
    CPP_MACRO_ALREADY_EXISTS_ERROR,
    CPP_REDEFINING_BUILTIN_MACRO_ERROR,
    CPP_SYNTAX_ERROR,
    CPP_UNREACHABLE_ERROR,
    CPP_UNIMPLEMENTED_ERROR,
    CPP_FILE_NOT_FOUND_ERROR,
};
static SrcLocExp*_Nullable cpp_srcloc_to_exp(CPreprocessor* cpp, SrcLoc loc);
static SrcLoc cpp_chain_loc(CPreprocessor* cpp, SrcLoc tok_loc, SrcLocExp* parent);

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
_Bool
cpp_has_macro(CPreprocessor* cpp, StringView name){
    Atom key = AT_get_atom(cpp->at, name.text, name.length);
    if(!key) return 0;
    CMacro* macro = AM_get(&cpp->macros, key);
    if(!macro) return 0;
    return 1;
}

static
_Bool
cpp_isdef(CPreprocessor* cpp, StringView name){
    if(cpp_has_macro(cpp, name)) return 1;
    if(sv_equals(name, SV("__has_include"))) return 1;
    if(sv_equals(name, SV("__has_embed"))) return 1;
    if(sv_equals(name, SV("__has_c_attribute"))) return 1;
    return 0;
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
static int cpp_handle_directive_in_inactive_region(CPreprocessor *cpp);
static int cpp_expand_obj_macro(CPreprocessor *cpp, CMacro *macro, SrcLoc expansion_loc, CPPTokens *dst);
static int cpp_expand_func_macro(CPreprocessor *cpp, CMacro *macro, SrcLoc expansion_loc, const CPPTokens *args, const Marray(size_t) *arg_seps, CPPTokens *dst);

static
int
cpp_next_pp_token(CPreprocessor* cpp, CPPToken* ptok){
    // phase 4
    for(;;){
        CPPToken tok;
        int err = cpp_next_raw_token(cpp, &tok);
        if(err) return err;
        if(cpp->if_stack.count && !ma_tail(cpp->if_stack).is_active){
            if(tok.type == CPP_NEWLINE){
                cpp->at_line_start = 1;
                *ptok = tok;
                return 0;
            }
            if(tok.type == CPP_EOF)
                return cpp_error(cpp, ma_tail(cpp->if_stack).start, "Unterminated conditional directive");
            if(tok.type == CPP_PUNCTUATOR && cpp->at_line_start && tok.punct == '#'){
                cpp->at_line_start = 0;
                err = cpp_handle_directive_in_inactive_region(cpp);
                if(err) return err;
                continue;
            }
            if(tok.type != CPP_WHITESPACE)
                cpp->at_line_start = 0;
            continue; // swallow all other tokens
        }
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
                                    return cpp_error(cpp, next.loc, "Too many arguments to function-like macro %s()", a->data);
                            }
                        }
                        err = cpp_push_tok(cpp, args, next);
                        if(err) return err;
                    }
                    if(args->count && !macro->nparams && !macro->is_variadic)
                        return cpp_error(cpp, args->data[0].loc, "Too many arguments to function-like macro %s()", a->data);
                    if(arg_seps->count+1 < macro->nparams)
                        return cpp_error(cpp, args->data[0].loc, "Too few arguments to function-like macro %s()", a->data);
                    err = cpp_expand_func_macro(cpp, macro, tok.loc, args, arg_seps, &cpp->pending);
                    if(err) return err;
                    cpp_release_scratch_idxes(cpp, arg_seps);
                    cpp_release_scratch(cpp, args);
                    continue;
                }
                err = cpp_expand_obj_macro(cpp, macro, tok.loc, &cpp->pending);
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
                *tok = (CPPToken){.type = CPP_PUNCTUATOR, .txt = SV("||"), .punct='||', .loc = loc};
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
void
cpp_msg(CPreprocessor* cpp, SrcLoc loc, LogLevel level, const char* prefix, const char* fmt, va_list va){
    uint64_t line = 0;
    uint64_t column = 0;
    uint64_t file_id = 0;
    SrcLocExp* e = NULL;
    if(loc.is_actually_a_pointer){
        e = (SrcLocExp*)(loc.pointer.bits<<1);
        line = e->line;
        column = e->column;
        file_id = e->file_id;
    }
    else {
        line = loc.line;
        column = loc.column;
        file_id = loc.file_id;
    }
    LongString path = file_id < cpp->fc->map.count?cpp->fc->map.data[file_id].path:LS("???");
    log_sprintf(cpp->logger, "%s:%d:%d: %s: ", path.text, (int)line, (int)column, prefix);
    log_logv(cpp->logger, level, fmt, va);
    if(e){
        while(e->parent){
            e = e->parent;
            line = e->line;
            column = e->column;
            file_id = e->file_id;
            path = file_id < cpp->fc->map.count?cpp->fc->map.data[file_id].path:LS("???");
            log_logf(cpp->logger, level, "%s:%d:%d: ... expanded from here", path.text, (int)line, (int)column);
        }
    }
}

static
int
cpp_error(CPreprocessor* cpp, SrcLoc loc, const char* fmt, ...){
    va_list va;
    va_start(va, fmt);
    cpp_msg(cpp, loc, LOG_PRINT_ERROR, "error", fmt, va);
    va_end(va);
    return CPP_SYNTAX_ERROR;
}

static
void
cpp_warn(CPreprocessor* cpp, SrcLoc loc, const char* fmt, ...){
    va_list va;
    va_start(va, fmt);
    cpp_msg(cpp, loc, LOG_PRINT_ERROR, "warning", fmt, va);
    va_end(va);
}

static
void
cpp_info(CPreprocessor* cpp, SrcLoc loc, const char* fmt, ...){
    va_list va;
    va_start(va, fmt);
    cpp_msg(cpp, loc, LOG_PRINT_ERROR, "info", fmt, va);
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
    else if(sv_equals(tok.txt, SV("if"))){
        CppPoundIf s = {
            .start = tok.loc,
        };
        CPPTokens *toks = cpp_get_scratch(cpp);
        // just scan to eol for now
        for(;;){
            err = cpp_next_raw_token(cpp, &tok);
            if(err) return err;
            if(tok.type == CPP_EOF || tok.type == CPP_NEWLINE)
                break;
            err = cpp_push_tok(cpp, toks, tok);
            if(err) {
                cpp_release_scratch(cpp, toks);
                return err;
            }
        }
        // push it back so dispatch loop sees newline
        err = cpp_push_tok(cpp, &cpp->pending, tok);
        if(err){
            cpp_release_scratch(cpp, toks);
            return CPP_OOM_ERROR;
        }
        int64_t value;
        err = cpp_eval_tokens(cpp, toks->data, toks->count, &value);
        cpp_release_scratch(cpp, toks);
        if(err) return err;
        s.true_taken = !!value; // TODO: eval
        s.is_active = s.true_taken;
        err = cpp_push_if(cpp, s);
        if(err) return CPP_OOM_ERROR;
    }
    else if(sv_equals(tok.txt, SV("ifdef"))){
        CppPoundIf s = {
            .start = tok.loc,
        };
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
            cpp_warn(cpp, tok.loc, "Trailing tokens after #ifdef");
        }
        s.true_taken = cpp_isdef(cpp, name);
        s.is_active = s.true_taken;
        err = cpp_push_if(cpp, s);
        if(err) return CPP_OOM_ERROR;
    }
    else if(sv_equals(tok.txt, SV("ifndef"))){
        CppPoundIf s = {
            .start = tok.loc,
        };
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
            cpp_warn(cpp, tok.loc, "Trailing tokens after #ifndef");
        }
        s.true_taken = !cpp_isdef(cpp, name);
        s.is_active = s.true_taken;
        err = cpp_push_if(cpp, s);
        if(err) return CPP_OOM_ERROR;
    }
    else if(sv_equals(tok.txt, SV("elif"))){
        if(!cpp->if_stack.count)
            return cpp_error(cpp, tok.loc, "#elif outside of #if (or similar construct)");
        CppPoundIf* s = &ma_tail(cpp->if_stack);
        if(s->seen_else)
            return cpp_error(cpp, tok.loc, "#elif after #else");
        // just scan to eol for now
        do {
            err = cpp_next_raw_token(cpp, &tok);
            if(err) return err;
        }while(tok.type != CPP_EOF && tok.type != CPP_NEWLINE);
        // push it back so dispatch loop sees newline
        err = cpp_push_tok(cpp, &cpp->pending, tok);
        if(err) return CPP_OOM_ERROR;
        s->is_active = 0;
    }
    else if(sv_equals(tok.txt, SV("else"))){
        if(!cpp->if_stack.count)
            return cpp_error(cpp, tok.loc, "#else outside of #if (or similar construct)");
        CppPoundIf* s = &ma_tail(cpp->if_stack);
        if(s->seen_else)
            return cpp_error(cpp, tok.loc, "another #else");
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
            cpp_warn(cpp, tok.loc, "Trailing tokens after #else");
        }
        s->seen_else = 1;
        s->is_active = !s->true_taken;
    }
    else if(sv_equals(tok.txt, SV("elifdef"))){
        if(!cpp->if_stack.count)
            return cpp_error(cpp, tok.loc, "#elifdef outside of #if (or similar construct)");
        CppPoundIf* s = &ma_tail(cpp->if_stack);
        if(s->seen_else)
            return cpp_error(cpp, tok.loc, "#elifdef after #else");
        err = cpp_next_raw_token(cpp, &tok);
        if(err) return err;
        if(tok.type != CPP_WHITESPACE) return cpp_error(cpp, tok.loc, "macro name missing");
        err = cpp_next_raw_token(cpp, &tok);
        if(err) return err;
        if(tok.type != CPP_IDENTIFIER) return cpp_error(cpp, tok.loc, "macro name missing");
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
            cpp_warn(cpp, tok.loc, "Trailing tokens after #elifdef");
        }
        s->is_active = 0;
    }
    else if(sv_equals(tok.txt, SV("elifndef"))){
        if(!cpp->if_stack.count)
            return cpp_error(cpp, tok.loc, "#elifndef outside of #if (or similar construct)");
        CppPoundIf* s = &ma_tail(cpp->if_stack);
        if(s->seen_else)
            return cpp_error(cpp, tok.loc, "#elifndef after #else");
        err = cpp_next_raw_token(cpp, &tok);
        if(err) return err;
        if(tok.type != CPP_WHITESPACE) return cpp_error(cpp, tok.loc, "macro name missing");
        err = cpp_next_raw_token(cpp, &tok);
        if(err) return err;
        if(tok.type != CPP_IDENTIFIER) return cpp_error(cpp, tok.loc, "macro name missing");
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
            cpp_warn(cpp, tok.loc, "Trailing tokens after #elifndef");
        }
        s->is_active = 0;
    }
    else if(sv_equals(tok.txt, SV("endif"))){
        if(!cpp->if_stack.count)
            return cpp_error(cpp, tok.loc, "#endif outside of #if (or similar construct)");
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
            cpp_warn(cpp, tok.loc, "Trailing tokens after #endif");
        }
        cpp->if_stack.count--;
    }
    else {
        cpp_warn(cpp, tok.loc, "Unhandled directive: '#%.*s'", sv_p(tok.txt));
        // unknown or unhandled directive
        do {
            err = cpp_next_raw_token(cpp, &tok);
            if(err) return err;
        }
        while(tok.type != CPP_EOF && tok.type != CPP_NEWLINE);
        // push it back so dispatch loop sees newline
        return cpp_push_tok(cpp, &cpp->pending, tok);
    }
    return 0;
}

static
int
cpp_handle_directive_in_inactive_region(CPreprocessor *cpp){
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
    else if(sv_equals(tok.txt, SV("if")) || sv_equals(tok.txt, SV("ifdef")) || sv_equals(tok.txt, SV("ifndef"))){
        CppPoundIf s = {.start = tok.loc, .is_dummy = 1};
        do{
            err = cpp_next_raw_token(cpp, &tok);
            if(err) return err;
        } while(tok.type != CPP_EOF && tok.type != CPP_NEWLINE);
        // push it back so dispatch loop sees newline
        err = cpp_push_tok(cpp, &cpp->pending, tok);
        if(err) return err;
        return cpp_push_if(cpp, s);
    }
    else if(sv_equals(tok.txt, SV("endif"))){
        if(!cpp->if_stack.count) return CPP_UNREACHABLE_ERROR;
            // return cpp_error(cpp, tok.loc, "#endif outside of #if (or similar construct)");
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
            cpp_warn(cpp, tok.loc, "Trailing tokens after #endif");
        }
        cpp->if_stack.count--;
        return 0;
    }
    if(!cpp->if_stack.count) return CPP_UNREACHABLE_ERROR;
    if(ma_tail(cpp->if_stack).is_dummy) {
        // unknown or unhandled directive
        do {
            err = cpp_next_raw_token(cpp, &tok);
            if(err) return err;
        }
        while(tok.type != CPP_EOF && tok.type != CPP_NEWLINE);
        // push it back so dispatch loop sees newline
        return cpp_push_tok(cpp, &cpp->pending, tok);
    }
    if(sv_equals(tok.txt, SV("elif"))){
        CppPoundIf* s = &ma_tail(cpp->if_stack);
        if(s->seen_else)
            return cpp_error(cpp, tok.loc, "#elif after #else");
        CPPTokens *toks = cpp_get_scratch(cpp);
        for(;;){
            err = cpp_next_raw_token(cpp, &tok);
            if(err){ cpp_release_scratch(cpp, toks); return err; }
            if(tok.type == CPP_EOF || tok.type == CPP_NEWLINE)
                break;
            err = cpp_push_tok(cpp, toks, tok);
            if(err){ cpp_release_scratch(cpp, toks); return err; }
        }
        // push it back so dispatch loop sees newline
        err = cpp_push_tok(cpp, &cpp->pending, tok);
        if(err){ cpp_release_scratch(cpp, toks); return CPP_OOM_ERROR; }
        s->is_active = 0;
        if(!s->true_taken){
            int64_t value;
            err = cpp_eval_tokens(cpp, toks->data, toks->count, &value);
            cpp_release_scratch(cpp, toks);
            if(err) return err;
            s->true_taken = !!value;
            s->is_active = s->true_taken;
        }
        else
            cpp_release_scratch(cpp, toks);
    }
    else if(sv_equals(tok.txt, SV("else"))){
        CppPoundIf* s = &ma_tail(cpp->if_stack);
        if(s->seen_else)
            return cpp_error(cpp, tok.loc, "another #else");
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
            cpp_warn(cpp, tok.loc, "Trailing tokens after #else");
        }
        s->seen_else = 1;
        s->is_active = !s->true_taken;
        return 0;
    }
    else if(sv_equals(tok.txt, SV("elifdef"))){
        CppPoundIf* s = &ma_tail(cpp->if_stack);
        if(s->seen_else)
            return cpp_error(cpp, tok.loc, "#elifdef after #else");
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
            cpp_warn(cpp, tok.loc, "Trailing tokens after #elifdef");
        }
        s->is_active = 0;
        if(!s->true_taken){
            s->true_taken = cpp_isdef(cpp, name);
            s->is_active = s->true_taken;
        }
        return 0;
    }
    else if(sv_equals(tok.txt, SV("elifndef"))){
        CppPoundIf* s = &ma_tail(cpp->if_stack);
        if(s->seen_else)
            return cpp_error(cpp, tok.loc, "#elifndef after #else");
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
            cpp_warn(cpp, tok.loc, "Trailing tokens after #elifndef");
        }
        s->is_active = 0;
        if(!s->true_taken){
            s->true_taken = !cpp_isdef(cpp, name);
            s->is_active = s->true_taken;
        }
        return 0;
    }
    // unknown or unhandled directive
    do {
        err = cpp_next_raw_token(cpp, &tok);
        if(err) return err;
    }
    while(tok.type != CPP_EOF && tok.type != CPP_NEWLINE);
    // push it back so dispatch loop sees newline
    return cpp_push_tok(cpp, &cpp->pending, tok);
}
static
int
cpp_expand_obj_macro(CPreprocessor *cpp, CMacro *macro, SrcLoc expansion_loc, CPPTokens *dst){
    if(macro->is_builtin){
        CppObjMacroFn* fn = (CppObjMacroFn*)macro->data[0];
        void* ctx = (void*) macro->data[1];
        if(!fn) return CPP_UNREACHABLE_ERROR;
        return fn(ctx, cpp, expansion_loc, dst);
    }
    macro->is_disabled = 1;
    CPPToken reenable = {.type = CPP_REENABLE, .data1 = macro};
    int err = cpp_push_tok(cpp, dst, reenable);
    if(err) return err;

    SrcLocExp* parent = cpp_srcloc_to_exp(cpp, expansion_loc);
    if(!parent) return CPP_OOM_ERROR;

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
        err = cpp_substitute_and_paste(cpp, repl, macro->nreplace, macro, &empty_args, &empty_seps, NULL, result, 0, parent);
        if(err) goto finally_obj;
        for(size_t i = result->count; i-- > 0;){
            CPPToken tok = result->data[i];
            if(tok.type == CPP_PLACEMARKER) continue;
            if(parent) tok.loc = cpp_chain_loc(cpp, tok.loc, parent);
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
        if(parent) tok.loc = cpp_chain_loc(cpp, tok.loc, parent);
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
cpp_paste_tokens(CPreprocessor *cpp, CPPToken left, CPPToken right, CPPToken *result, SrcLoc loc, SrcLocExp* expansion_parent){
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
        SrcLoc eloc = cpp_chain_loc(cpp, loc, expansion_parent);
        return cpp_error(cpp, eloc, "pasting \"%.*s\" and \"%.*s\" does not give a valid preprocessing token", sv_p(left.txt), sv_p(right.txt));
    }

    tok.loc = loc;
    *result = tok;
    return 0;
}

// Expands macros in a slice of tokens. Assumes the tokens doesn't have directives, like for a function-like macro's arguments
static
int
cpp_expand_argument(CPreprocessor *cpp, CPPToken*_Null_unspecified toks, size_t count, CPPTokens *out){
    if(!count) return 0;
    int err = 0;
    CPPToken tok;
    CPPTokens* pending = cpp_get_scratch(cpp);
    CPPTokens* args = NULL;
    Marray(size_t) *arg_seps = NULL;
    if(!pending) return CPP_OOM_ERROR;
    for(size_t i = 0;;){
        if(pending->count){
            tok = ma_pop_(*pending);
            if(tok.type == CPP_REENABLE){
                ((CMacro*)tok.data1)->is_disabled = 0;
                continue;
            }
        }
        else if(i < count)
            tok = toks[i++];
        else
            break;
        if(tok.type != CPP_IDENTIFIER || tok.disabled){
            err = cpp_push_tok(cpp, out, tok);
            if(err) goto finally;
            continue;
        }
        Atom a = AT_get_atom(cpp->at, tok.txt.text, tok.txt.length);
        CMacro* macro = NULL;
        if(a) macro = AM_get(&cpp->macros, a);
        if(!macro || macro->is_disabled){
            err = cpp_push_tok(cpp, out, tok);
            if(err) goto finally;
            continue;
        }
        if(!macro->is_function_like){
            err = cpp_expand_obj_macro(cpp, macro, tok.loc, pending);
            if(err) goto finally;
            continue;
        }
        CPPToken next = {.type = CPP_EOF};
        for(;;){
            while(pending->count){
                next = ma_pop_(*pending);
                if(next.type == CPP_REENABLE){
                    ((CMacro*)next.data1)->is_disabled = 0;
                    continue;
                }
                goto got_next;
            }
            if(i < count)
                next = toks[i++];
            else
                next = (CPPToken){.type=CPP_EOF};
            got_next:;
            if(next.type != CPP_WHITESPACE && next.type != CPP_NEWLINE)
                break;
        }
        if(next.type != CPP_PUNCTUATOR || next.punct != '('){
            // not function invocation
            if(next.type != CPP_EOF){
                err = cpp_push_tok(cpp, pending, next);
                if(err) goto finally;
            }
            err = cpp_push_tok(cpp, out, tok);
            if(err) goto finally;
            continue;
        }
        args = cpp_get_scratch(cpp);
        arg_seps = cpp_get_scratch_idxes(cpp);
        if(!args || !arg_seps){
            err = CPP_OOM_ERROR;
            goto finally;
        }
        for(int paren = 1;;){
            while(pending->count){
                next = ma_pop_(*pending);
                if(next.type == CPP_REENABLE){
                    ((CMacro*)next.data1)->is_disabled = 0;
                    continue;
                }
                goto got_arg_tok;
            }
            if(i < count)
                next = toks[i++];
            else
                next = (CPPToken){.type=CPP_EOF};
            got_arg_tok:;
            if(next.type == CPP_EOF){
                err = cpp_error(cpp, next.loc, "EOF in function-like macro invocation %s()", a->data);
                goto finally;
            }
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
                        if(err) goto finally;
                    }
                    else{
                        err = cpp_error(cpp, next.loc, "Too many arguments to function-like macro %s()", a->data);
                        goto finally;
                    }
                }
            }
            err = cpp_push_tok(cpp, args, next);
            if(err) goto finally;
        }
        if(args->count && !macro->nparams && !macro->is_variadic){
            err = cpp_error(cpp, args->data[0].loc, "Too many arguments to function-like macro %s()", a->data);
            goto finally;
        }
        size_t nargs = args->count ? arg_seps->count + 1 : 0;
        if(nargs < macro->nparams){
            err = cpp_error(cpp, tok.loc, "Too few arguments to function-like macro %s()", a->data);
            goto finally;
        }
        err = cpp_expand_func_macro(cpp, macro, tok.loc, args, arg_seps, pending);
        if(err) goto finally;
        cpp_release_scratch_idxes(cpp, arg_seps);
        arg_seps = NULL;
        cpp_release_scratch(cpp, args);
        args = NULL;
    }
    finally:
    if(arg_seps) cpp_release_scratch_idxes(cpp, arg_seps);
    if(args) cpp_release_scratch(cpp, args);
    if(pending)cpp_release_scratch(cpp, pending);
    return err;
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
cpp_parse_va_opt_content(CPreprocessor *cpp, const CPPToken *repl, size_t nreplace, size_t after_va_opt, SrcLoc loc, size_t *out_content_start, size_t *out_close_paren, SrcLocExp* expansion_parent){
    size_t k = after_va_opt;
    while(k < nreplace && repl[k].type == CPP_WHITESPACE) k++;
    if(k >= nreplace || repl[k].type != CPP_PUNCTUATOR || repl[k].punct != '('){
        SrcLoc eloc = cpp_chain_loc(cpp, loc, expansion_parent);
        return cpp_error(cpp, eloc, "__VA_OPT__ must be followed by (content)");
    }
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
    if(paren != 0){
        SrcLoc eloc = cpp_chain_loc(cpp, loc, expansion_parent);
        return cpp_error(cpp, eloc, "unterminated __VA_OPT__");
    }
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
    _Bool raw_only,
    SrcLocExp* expansion_parent)
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
                err = cpp_parse_va_opt_content(cpp, repl, nreplace, j+1, repl[j].loc, &cstart, &cparen, expansion_parent);
                if(err) return err;
                CPPToken stringified;
                if(cpp_va_args_nonempty(cpp, macro, args, arg_seps, expanded_args)){
                    CPPTokens *temp = cpp_get_scratch(cpp);
                    if(!temp) return CPP_OOM_ERROR;
                    err = cpp_substitute_and_paste(cpp, repl+cstart, cparen-cstart, macro, args, arg_seps, expanded_args, temp, 1, expansion_parent);
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
            if(j >= nreplace || repl[j].param_idx == 0){
                SrcLoc eloc = cpp_chain_loc(cpp, t.loc, expansion_parent);
                return cpp_error(cpp, eloc, "'#' is not followed by a macro parameter");
            }
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
                err = cpp_paste_tokens(cpp, left, right, &pr, t.loc, expansion_parent);
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
                err = cpp_parse_va_opt_content(cpp, repl, nreplace, j+1, repl[j].loc, &cstart, &cparen, expansion_parent);
                if(err) return err;
                if(cpp_va_args_nonempty(cpp, macro, args, arg_seps, expanded_args)){
                    CPPTokens *temp = cpp_get_scratch(cpp);
                    if(!temp) return CPP_OOM_ERROR;
                    err = cpp_substitute_and_paste(cpp, repl+cstart, cparen-cstart, macro, args, arg_seps, expanded_args, temp, raw_only, expansion_parent);
                    if(err) goto finally_paste_va_opt;
                    right = (temp->count == 0)
                        ? (CPPToken){.type = CPP_PLACEMARKER, .loc = repl[j].loc}
                        : temp->data[0];
                    CPPToken pr;
                    err = cpp_paste_tokens(cpp, left, right, &pr, t.loc, expansion_parent);
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
                    err = cpp_paste_tokens(cpp, left, right, &pr, t.loc, expansion_parent);
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
            err = cpp_paste_tokens(cpp, left, right, &pr, t.loc, expansion_parent);
            if(err) return err;
            err = cpp_push_tok(cpp, out, pr);
            if(err) return err;
            i = skip_to;
            continue;
        }

        // __VA_OPT__
        if(t.type == CPP_IDENTIFIER && sv_equals(t.txt, SV("__VA_OPT__"))){
            size_t cstart, cparen;
            err = cpp_parse_va_opt_content(cpp, repl, nreplace, i+1, t.loc, &cstart, &cparen, expansion_parent);
            if(err) return err;
            if(cpp_va_args_nonempty(cpp, macro, args, arg_seps, expanded_args)){
                err = cpp_substitute_and_paste(cpp, repl+cstart, cparen-cstart, macro, args, arg_seps, expanded_args, out, raw_only, expansion_parent);
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
cpp_expand_func_macro(CPreprocessor *cpp, CMacro *macro, SrcLoc expansion_loc, const CPPTokens *args, const Marray(size_t) *arg_seps, CPPTokens *dst){
    if(macro->is_builtin){
        CppFuncMacroFn *fn = (CppFuncMacroFn*)macro->data[0];
        void* ctx = (void*)macro->data[1];
        if(!fn) return CPP_UNREACHABLE_ERROR;
        if(macro->no_expand_args)
            return fn(ctx, cpp, expansion_loc, dst, args, arg_seps);
        else{
            int err;
            CPPTokens *exp_args = cpp_get_scratch(cpp);
            CPPTokens *result = cpp_get_scratch(cpp);
            Marray(size_t) *idxes = cpp_get_scratch_idxes(cpp);
            if(!result || !exp_args || !idxes){
                err = CPP_OOM_ERROR;
                goto func_finally;
            }
            SrcLocExp* parent = cpp_srcloc_to_exp(cpp, expansion_loc);
            if(!parent){ err = CPP_OOM_ERROR; goto func_finally; }
            size_t total_params = macro->nparams + (macro->is_variadic ? 1 : 0);
            for(size_t i = 0; i < total_params; i++){
                CPPToken* start;
                size_t count;
                cpp_get_param_arg(macro, args, arg_seps, i, &start, &count);
                err = cpp_expand_argument(cpp, start, count, exp_args);
                if(err) goto func_finally;
                if(i < arg_seps->count){
                    err = ma_push(size_t)(idxes, cpp->allocator, exp_args->count);
                    if(err) goto func_finally;
                    err = cpp_push_tok(cpp, exp_args, (CPPToken){.type = CPP_PUNCTUATOR, .punct = ','});
                    if(err) goto func_finally;
                }
            }
            err = fn(ctx, cpp, expansion_loc, result, exp_args, idxes);
            if(err) goto func_finally;
            macro->is_disabled = 1;
            CPPToken reenable = {.type = CPP_REENABLE, .data1 = macro};
            err = cpp_push_tok(cpp, dst, reenable);
            if(err) goto func_finally;

            for(size_t i = result->count; i-- > 0;){
                CPPToken t = result->data[i];
                if(t.type == CPP_PLACEMARKER)
                    continue;
                t.loc = cpp_chain_loc(cpp, t.loc, parent);
                if(t.type == CPP_IDENTIFIER && !t.disabled){
                    Atom a = AT_get_atom(cpp->at, t.txt.text, t.txt.length);
                    if(a){
                        CMacro* m = AM_get(&cpp->macros, a);
                        if(m && m->is_disabled)
                            t.disabled = 1;
                    }
                }
                err = cpp_push_tok(cpp, dst, t);
                if(err) goto func_finally;
            }

            func_finally:
            if(result) cpp_release_scratch(cpp, result);
            if(exp_args) cpp_release_scratch(cpp, exp_args);
            if(idxes) cpp_release_scratch_idxes(cpp, idxes);
            return err;
        }
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

    SrcLocExp* parent = cpp_srcloc_to_exp(cpp, expansion_loc);
    if(!parent){ err = CPP_OOM_ERROR; goto finally; }

    err = cpp_substitute_and_paste(cpp, cpp_cmacro_replacement(macro), macro->nreplace, macro, args, arg_seps, expanded_args, result, 0, parent);
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
        t.loc = cpp_chain_loc(cpp, t.loc, parent);
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
                     cpp_builtin_counter,
                     cpp_builtin_filename,
                     cpp_builtin_include_level,
                     cpp_builtin_date,
                     cpp_builtin_time
                     ;
static CppFuncMacroFn cpp_builtin_eval,
                      cpp_builtin_mixin,
                      cpp_builtin_env,
                      cpp_builtin_if,
                      cpp_builtin_ident,
                      cpp_builtin_fmt
                      ;
static CppPragmaFn cpp_builtin_pragma_once
                   ;

static
int
cpp_define_builtin_macros(CPreprocessor* cpp){
    int err;
    static const struct {
        StringView name; CppObjMacroFn* fn;
    } obj_builtins[] = {
        {SV("__FILE__"), cpp_builtin_file},
        {SV("__LINE__"), cpp_builtin_line},
        {SV("__COUNTER__"), cpp_builtin_counter},
        {SV("__FILE_NAME__"), cpp_builtin_filename},
        {SV("__INCLUDE_LEVEL__"), cpp_builtin_include_level},
        {SV("__DATE__"), cpp_builtin_date},
        {SV("__TIME__"), cpp_builtin_time},
    };
    for(size_t i = 0; i < sizeof obj_builtins / sizeof obj_builtins[0]; i++){
        err = cpp_define_builtin_obj_macro(cpp, obj_builtins[i].name, obj_builtins[i].fn, NULL);
        if(err) return err;
    }
    static const struct {
        StringView name; CppFuncMacroFn* fn; size_t nparams; _Bool variadic, no_expand;
    } func_builtins[] = {
        {SV("__EVAL__"), cpp_builtin_eval, 1, 0, 1},
        {SV("__eval"), cpp_builtin_eval, 1, 0, 1},
        {SV("__MIXIN__"), cpp_builtin_mixin, 1, 0, 0},
        {SV("__mixin"), cpp_builtin_mixin, 1, 0, 0},
        {SV("__env"), cpp_builtin_env, 1, 0, 0},
        {SV("__ENV__"), cpp_builtin_env, 1, 0, 0},
        {SV("__IF__"), cpp_builtin_if, 3, 0, 1},
        {SV("__if"), cpp_builtin_if, 3, 0, 1},
        {SV("__ident"), cpp_builtin_ident, 1, 0, 0},
        {SV("__IDENT__"), cpp_builtin_ident, 1, 0, 0},
        {SV("__format"), cpp_builtin_fmt, 1, 1, 0},
        {SV("__FORMAT__"), cpp_builtin_fmt, 1, 1, 0},
    };
    for(size_t i = 0; i < sizeof func_builtins / sizeof func_builtins[0]; i++){
        err = cpp_define_builtin_func_macro(cpp, func_builtins[i].name, func_builtins[i].fn, NULL, func_builtins[i].nparams, func_builtins[i].variadic, func_builtins[i].no_expand);
        if(err) return err;
    }
    err = cpp_register_pragma(cpp, SV("once"), cpp_builtin_pragma_once, NULL);
    if(err) return err;
    return 0;
}
static
int
cpp_builtin_file(void* _Null_unspecified ctx, CPreprocessor* cpp, SrcLoc loc, CPPTokens* outtoks){
    (void)ctx;
    uint64_t file_id = 0;
    if(loc.is_actually_a_pointer){
        SrcLocExp* e = (SrcLocExp*)(loc.pointer.bits<<1);
        while(e->parent)
            e = e->parent;
        file_id = e->file_id;
    }
    else {
        file_id = loc.file_id;
    }
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
cpp_builtin_filename(void* _Null_unspecified ctx, CPreprocessor* cpp, SrcLoc loc, CPPTokens* outtoks){
    (void)ctx;
    uint64_t file_id = 0;
    if(loc.is_actually_a_pointer){
        SrcLocExp* e = (SrcLocExp*)(loc.pointer.bits<<1);
        while(e->parent)
            e = e->parent;
        file_id = e->file_id;
    }
    else {
        file_id = loc.file_id;
    }
    LongString path = file_id < cpp->fc->map.count?cpp->fc->map.data[file_id].path:LS("???");
    _Bool windows = 0;
    #ifdef _WIN32
    windows = 1;
    #endif
    StringView basename = path_basename(LS_to_SV(path), windows);
    Atom a = cpp_atomizef(cpp, "\"%.*s\"", sv_p(basename));
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
cpp_builtin_date(void* _Null_unspecified ctx, CPreprocessor* cpp, SrcLoc loc, CPPTokens* outtoks){
    (void)ctx;
    CPPToken tok = {
        .txt = cpp->date?(StringView){cpp->date->length, cpp->date->data}: SV("\"Jan 01 1900\""),
        .loc = loc,
        .type = CPP_STRING,
    };
    int err = cpp_push_tok(cpp, outtoks, tok);
    return err;
}

static
int
cpp_builtin_time(void* _Null_unspecified ctx, CPreprocessor* cpp, SrcLoc loc, CPPTokens* outtoks){
    (void)ctx;
    CPPToken tok = {
        .txt = cpp->time?(StringView){cpp->time->length, cpp->time->data}: SV("\"01:02:03\""),
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
    unsigned line = 0;
    if(loc.is_actually_a_pointer){
        SrcLocExp* e = (SrcLocExp*)(loc.pointer.bits<<1);
        while(e->parent)
            e = e->parent;
        line = (unsigned)e->line;
    }
    else {
        line = (unsigned)loc.line;
    }
    Atom a = cpp_atomizef(cpp, "%u", line);
    if(!a) return CPP_OOM_ERROR;
    CPPToken tok = {
        .txt = {a->length, a->data},
        .loc = loc,
        .type = CPP_NUMBER,
    };
    int err = cpp_push_tok(cpp, outtoks, tok);
    return err;
}
static
int
cpp_builtin_include_level(void* _Null_unspecified ctx, CPreprocessor* cpp, SrcLoc loc, CPPTokens* outtoks){
    (void)ctx;
    if(!cpp->frames.count) return CPP_UNREACHABLE_ERROR;
    Atom a = cpp_atomizef(cpp, "%zu", cpp->frames.count);
    if(!a) return CPP_OOM_ERROR;
    CPPToken tok = {
        .txt = {a->length, a->data},
        .loc = loc,
        .type = CPP_NUMBER,
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
        .type = CPP_NUMBER,
    };
    int err = cpp_push_tok(cpp, outtoks, tok);
    return err;
}
static
int
cpp_builtin_eval(void* _Null_unspecified ctx, CPreprocessor* cpp, SrcLoc loc, CPPTokens* outtoks, const CPPTokens* args, const Marray(size_t)* arg_seps){
    (void)ctx;
    (void)arg_seps;
    int err;
    int64_t value;
    err = cpp_eval_tokens(cpp, args->data, args->count, &value);
    if(err) return err;
    Atom a;
    if(value < INT_MAX)
        a = cpp_atomizef(cpp, "%u", (unsigned)value);
    else
        a = cpp_atomizef(cpp, "%llullu", (unsigned long long)value);
    if(!a) return CPP_OOM_ERROR;
    CPPToken tok = {
        .txt = {a->length, a->data},
        .loc = loc,
        .type = CPP_NUMBER,
    };
    err = cpp_push_tok(cpp, outtoks, tok);
    return err;
}

static
int
cpp_builtin_mixin(void* _Null_unspecified ctx, CPreprocessor* cpp, SrcLoc loc, CPPTokens* outtoks, const CPPTokens* args, const Marray(size_t)*arg_seps){
    (void)ctx;
    (void)arg_seps;
    int err = 0;
    MStringBuilder sb = {.allocator = allocator_from_arena(&cpp->synth_arena)};
    for(size_t i = 0; i < args->count; i++){
        CPPToken tok = args->data[i];
        if(tok.type == CPP_WHITESPACE || tok.type == CPP_NEWLINE) continue;
        if(tok.type == CPP_STRING){
            if(tok.txt.length > 2)
                msb_write_str(&sb, tok.txt.text+1, tok.txt.length-2);
            continue;
        }
        err = cpp_error(cpp, tok.loc, "Only string literals supported as arg to mixin");
        goto finally;
    }
    err = cpp_mixin_string(cpp, loc, msb_borrow_sv(&sb), outtoks);
    finally:
    msb_destroy(&sb);
    return err;
}

static
int
cpp_builtin_if(void* _Null_unspecified ctx, CPreprocessor* cpp, SrcLoc loc, CPPTokens* outtoks, const CPPTokens* args, const Marray(size_t)*arg_seps){
    (void)ctx;
    (void)loc;
    CPPToken *toks; size_t count;
    cpp_get_argument(args, arg_seps, 0, &toks, &count);
    int64_t value;
    int err = cpp_eval_tokens(cpp, toks, count, &value);
    if(err) return err;
    cpp_get_argument(args, arg_seps, value?1:2, &toks, &count);
    err = cpp_expand_argument(cpp, toks, count, outtoks);
    return err;
}

static
int
cpp_builtin_ident(void* _Null_unspecified ctx, CPreprocessor* cpp, SrcLoc loc, CPPTokens* outtoks, const CPPTokens* args, const Marray(size_t)*arg_seps){
    (void)ctx;
    (void)arg_seps;
    int err = 0;
    MStringBuilder sb = {.allocator = allocator_from_arena(&cpp->synth_arena)};
    for(size_t i = 0; i < args->count; i++){
        CPPToken tok = args->data[i];
        if(tok.type == CPP_WHITESPACE || tok.type == CPP_NEWLINE) continue;
        if(tok.type == CPP_STRING){
            if(tok.txt.length > 2)
                msb_write_str(&sb, tok.txt.text+1, tok.txt.length-2);
            continue;
        }
        err = cpp_error(cpp, tok.loc, "Only string literals supported as arg to ident");
        goto finally;
    }
    StringView sv = msb_borrow_sv(&sb);
    Atom a = cpp_atomizef(cpp, "%.*s", sv_p(sv));
    if(!a) return CPP_OOM_ERROR;
    CPPToken tok = {
        .loc = loc,
        .type = CPP_IDENTIFIER,
        .txt = {a->length, a->data},
    };
    err = cpp_push_tok(cpp, outtoks, tok);
    finally:
    msb_destroy(&sb);
    return err;
}

static
int
cpp_builtin_fmt(void* _Null_unspecified ctx, CPreprocessor* cpp, SrcLoc loc, CPPTokens* outtoks, const CPPTokens* args, const Marray(size_t)*arg_seps){
    (void)ctx;
    int err = 0;
    MStringBuilder sb = {.allocator = allocator_from_arena(&cpp->synth_arena)};
    msb_write_char(&sb, '"');
    CPPToken* fmts; size_t fmt_count;
    CPPToken* va_args; size_t va_count;
    cpp_get_argument(args, arg_seps, 0, &fmts, &fmt_count);
    cpp_get_va_args(args, arg_seps, 1, &va_args, &va_count);
    for(size_t f = 0; f < fmt_count; f++){
        CPPToken fmt = fmts[f];
        if(fmt.type == CPP_WHITESPACE || fmt.type == CPP_NEWLINE) continue;
        if(fmt.type != CPP_STRING){
            err = cpp_error(cpp, fmt.loc, "Only string literals supported as fmt to format");
            goto finally;
        }
        StringView s = sv_slice(fmt.txt, 1, fmt.txt.length-1);
        for(size_t i = 0; i < s.length;){
            char c = s.text[i++];
            if(c == '%' && i < s.length){
                c = s.text[i++];
                switch(c){
                    case '%': msb_write_char(&sb, '%'); break;
                    case 's':{
                        if(!va_count){
                            err = cpp_error(cpp, loc, "Run out of va_args");
                            goto finally;
                        }
                        for(;va_count;++va_args, --va_count){
                            CPPToken tok = *va_args;
                            if(tok.type == CPP_PUNCTUATOR && tok.punct == ','){
                                ++va_args; --va_count;
                                break;
                            }
                            if(tok.type == CPP_WHITESPACE || tok.type == CPP_NEWLINE) continue;
                            if(tok.type == CPP_STRING){
                                msb_write_str(&sb, tok.txt.text+1, tok.txt.length-2);
                                continue;
                            }
                            err = cpp_error(cpp, tok.loc, "Invalid arg to format (expected string)");
                            goto finally;
                        }
                    }break;
                    case 'd':{
                        if(!va_count){
                            err = cpp_error(cpp, loc, "Run out of va_args");
                            goto finally;
                        }
                        _Bool wrote_number = 0;
                        for(;va_count;++va_args, --va_count){
                            CPPToken tok = *va_args;
                            if(tok.type == CPP_PUNCTUATOR && tok.punct == ','){
                                ++va_args; --va_count;
                                break;
                            }
                            if(tok.type == CPP_WHITESPACE || tok.type == CPP_NEWLINE) continue;
                            if(tok.type == CPP_NUMBER){
                                if(wrote_number){
                                    err = cpp_error(cpp, tok.loc, "Too many number args to format");
                                    goto finally;
                                }
                                Uint64Result u = parse_unsigned_human(tok.txt.text, tok.txt.length);
                                if(u.errored){
                                    err = cpp_error(cpp, tok.loc, "Invalid arg to format (expected int)");
                                    goto finally;
                                }
                                msb_sprintf(&sb, "%llu", (unsigned long long)u.result);
                                wrote_number = 1;
                                continue;
                            }
                            err = cpp_error(cpp, tok.loc, "Invalid arg to format (expected int)");
                            goto finally;
                        }
                    }break;
                    default:
                        msb_write_char(&sb, '%');
                        msb_write_char(&sb, c);
                        break;
                }
            }
            else
                msb_write_char(&sb, c);
        }
    }
    msb_write_char(&sb, '"');
    StringView sv = msb_borrow_sv(&sb);
    Atom a = AT_atomize(cpp->at, sv.text, sv.length);
    if(!a){
        err = CPP_OOM_ERROR;
        goto finally;
    }
    CPPToken tok = {
        .txt = {a->length, a->data},
        .loc = loc,
        .type = CPP_STRING,
    };
    err = cpp_push_tok(cpp, outtoks, tok);
    finally:;
    msb_destroy(&sb);
    return err;
}
static
int
cpp_builtin_env(void* _Null_unspecified ctx, CPreprocessor* cpp, SrcLoc loc, CPPTokens* outtoks, const CPPTokens* args, const Marray(size_t)*arg_seps){
    (void)ctx;
    (void)arg_seps;
    int err = 0;
    MStringBuilder sb = {.allocator = allocator_from_arena(&cpp->synth_arena)};
    for(size_t i = 0; i < args->count; i++){
        CPPToken tok = args->data[i];
        if(tok.type == CPP_WHITESPACE) continue;
        if(tok.type == CPP_STRING){
            if(tok.txt.length > 2)
                msb_write_str(&sb, tok.txt.text+1, tok.txt.length-2);
            continue;
        }
        err = cpp_error(cpp, tok.loc, "Only string literals supported as arg to env");
        goto finally;
    }
    StringView sv = msb_borrow_sv(&sb);
    Atom v = env_getenv2(cpp->env, sv.text, sv.length);
    if(!v) v = cpp_atomizef(cpp, "\"\"");
    else v = cpp_atomizef(cpp, "\"%s\"", v->data);
    if(!v) {
        err = CPP_OOM_ERROR;
        goto finally;
    }
    CPPToken tok = {
        .txt = {v->length, v->data},
        .loc = loc,
        .type = CPP_STRING,
    };
    err = cpp_push_tok(cpp, outtoks, tok);
    finally:
    msb_destroy(&sb);
    return err;

}
static
int
cpp_builtin_pragma_once(void* _Null_unspecified ctx, CPreprocessor* cpp, SrcLoc loc, const CPPToken*_Null_unspecified toks, size_t ntoks){
    (void)ctx;
    (void)toks;
    if(ntoks)
        cpp_warn(cpp, loc, "Trailing tokens after #pragma once");
    return 0;
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

static
int
cpp_push_tok(CPreprocessor* cpp, CPPTokens* dst, CPPToken tok){
    int err = ma_push(CPPToken)(dst, cpp->allocator, tok);
    return err;
}

static
int
cpp_push_if(CPreprocessor* cpp, CppPoundIf s){
    int err = ma_push(CppPoundIf)(&cpp->if_stack, cpp->allocator, s);
    return err;
}

static
SrcLocExp*_Nullable
cpp_srcloc_to_exp(CPreprocessor* cpp, SrcLoc loc){
    if(loc.is_actually_a_pointer)
        return (SrcLocExp*)(loc.pointer.bits << 1);
    SrcLocExp* exp = ArenaAllocator_alloc(&cpp->synth_arena, sizeof *exp);
    if(!exp) return NULL;
    *exp = (SrcLocExp){.file_id = loc.file_id, .column = loc.column, .line = loc.line};
    return exp;
}

static
SrcLoc
cpp_chain_loc(CPreprocessor* cpp, SrcLoc tok_loc, SrcLocExp* parent){
    SrcLocExp* exp = ArenaAllocator_alloc(&cpp->synth_arena, sizeof *exp);
    if(!exp) return tok_loc;
    *exp = (SrcLocExp){.file_id = tok_loc.file_id, .column = tok_loc.column, .line = tok_loc.line, .parent = parent};
    SrcLoc result = {.pointer = {.bits = (uint64_t)exp >> 1, .is_actually_a_pointer = 1}};
    return result;
}

static
int
cpp_include_file_via_file_cache(CPreprocessor* cpp, StringView path){
    int err;
    fc_write_path(cpp->fc, path.text, path.length);
    StringView txt;
    err = fc_read_file(cpp->fc, &txt);
    if(err) return CPP_FILE_NOT_FOUND_ERROR;
    CPPFrame init = {
        .file_id = (uint32_t)cpp->fc->map.count-1,
        .txt = txt,
        .line = 1,
        .column = 1,
    };
    err = ma_push(CPPFrame)(&cpp->frames, cpp->allocator, init);
    return err?CPP_OOM_ERROR:0;
}

typedef struct CppTokenStream CppTokenStream;
struct CppTokenStream {
    CPPToken* toks;
    size_t count;
    size_t cursor;
    CPPTokens *pending;
};

static CPPToken cpp_ts_next(CppTokenStream* s);

static
int
cpp_eval_ts_next(CPreprocessor* cpp, CppTokenStream* s, CPPToken *tok);
static
int
cpp_recursive_eval(CPreprocessor* cpp, CppTokenStream* s, int64_t* value);
static
int
cpp_recursive_eval_prec(CPreprocessor* cpp, CppTokenStream* s, int64_t* value, int min_prec);

static
int
cpp_eval_tokens(CPreprocessor* cpp, CPPToken*_Null_unspecified toks, size_t count, int64_t* value){
    int err = 0;
    CPPTokens *pending = NULL;
    pending = cpp_get_scratch(cpp);

    if(!pending){
        err = CPP_OOM_ERROR;
        goto finally;
    }
    CppTokenStream stream = {
        .toks = toks,
        .count = count,
        .pending = pending,
    };
    err = cpp_recursive_eval(cpp, &stream, value);
    finally:
    cpp_release_scratch(cpp, pending);
    return err;
}

static
int
cpp_eval_ts_next(CPreprocessor* cpp, CppTokenStream* s, CPPToken *outtok){
    int err;
    CPPToken tok;
    for(;;){
        tok = cpp_ts_next(s);
        switch(tok.type){
            case CPP_EOF:
                *outtok = tok;
                return 0;
            case CPP_HEADER_NAME:
                return CPP_UNREACHABLE_ERROR;
            case CPP_IDENTIFIER:
                break;
            case CPP_NUMBER:
                *outtok = tok;
                return 0;
            case CPP_CHAR:
                *outtok = tok;
                return 0;
            case CPP_STRING:
                return cpp_error(cpp, tok.loc, "String literal in #if evaluation");
            case CPP_PUNCTUATOR:
                *outtok = tok;
                return 0;
            case CPP_WHITESPACE:
                continue;
            case CPP_NEWLINE:
                return CPP_UNREACHABLE_ERROR;
            case CPP_OTHER:
                return cpp_error(cpp, tok.loc, "Invalid token kind");
            case CPP_PLACEMARKER:
                return CPP_UNREACHABLE_ERROR;
            case CPP_REENABLE:
                ((CMacro*)tok.data1)->is_disabled = 0;
                continue;
        }
        StringView name = tok.txt;
        if(sv_equals(name, SV("true"))){
            *outtok = (CPPToken){.type=CPP_NUMBER, .txt = SV("1"), .loc = tok.loc};
            return 0;
        }
        if(sv_equals(name, SV("__has_include")) || sv_equals(name, SV("__has_embed"))){
            _Bool is_embed = name.text[6] == 'e';
            CPPToken next;
            do { next = cpp_ts_next(s); } while(next.type == CPP_WHITESPACE);
            if(next.type != CPP_PUNCTUATOR || next.punct != '(')
                return cpp_error(cpp, tok.loc, "Expected '(' after %.*s", (int)name.length, name.text);
            do { next = cpp_ts_next(s); } while(next.type == CPP_WHITESPACE);
            _Bool quote;
            StringView header_name;
            if(next.type == CPP_STRING){
                // "header" form — strip quotes
                quote = 1;
                header_name = (StringView){next.txt.length - 2, next.txt.text + 1};
            }
            else if(next.type == CPP_PUNCTUATOR && next.punct == '<'){
                // <header> form — collect tokens until >
                quote = 0;
                MStringBuilder sb = {.allocator = allocator_from_arena(&cpp->synth_arena)};
                for(;;){
                    next = cpp_ts_next(s);
                    if(next.type == CPP_EOF){
                        msb_destroy(&sb);
                        return cpp_error(cpp, tok.loc, "Unterminated < in %.*s", (int)name.length, name.text);
                    }
                    if(next.type == CPP_PUNCTUATOR && next.punct == '>')
                        break;
                    msb_write_str(&sb, next.txt.text, next.txt.length);
                }
                header_name = msb_borrow_sv(&sb);
                _Bool result = !is_embed && cpp_has_include(cpp, quote, header_name);
                msb_destroy(&sb);
                do { next = cpp_ts_next(s); } while(next.type == CPP_WHITESPACE);
                if(next.type != CPP_PUNCTUATOR || next.punct != ')')
                    return cpp_error(cpp, tok.loc, "Expected ')' after %.*s", (int)name.length, name.text);
                *outtok = (CPPToken){.type=CPP_NUMBER, .txt = result?SV("1"):SV("0"), .loc = tok.loc};
                return 0;
            }
            else {
                return cpp_error(cpp, next.loc, "Expected header name for %.*s", (int)name.length, name.text);
            }
            do { next = cpp_ts_next(s); } while(next.type == CPP_WHITESPACE);
            if(next.type != CPP_PUNCTUATOR || next.punct != ')')
                return cpp_error(cpp, tok.loc, "Expected ')' after %.*s", (int)name.length, name.text);
            _Bool result = !is_embed && cpp_has_include(cpp, quote, header_name);
            *outtok = (CPPToken){.type=CPP_NUMBER, .txt = result?SV("1"):SV("0"), .loc = tok.loc};
            return 0;
        }
        if(sv_equals(name, SV("__has_c_attribute"))){
            // consume (attr) and always return 0 for now
            CPPToken next;
            do { next = cpp_ts_next(s); } while(next.type == CPP_WHITESPACE);
            if(next.type != CPP_PUNCTUATOR || next.punct != '(')
                return cpp_error(cpp, tok.loc, "Expected '(' after __has_c_attribute");
            for(int paren = 1; paren;){
                next = cpp_ts_next(s);
                if(next.type == CPP_EOF)
                    return cpp_error(cpp, tok.loc, "Unterminated __has_c_attribute(");
                if(next.type == CPP_PUNCTUATOR && next.punct == '(') paren++;
                else if(next.type == CPP_PUNCTUATOR && next.punct == ')') paren--;
            }
            *outtok = (CPPToken){.type=CPP_NUMBER, .txt = SV("0"), .loc = tok.loc};
            return 0;
        }
        if(sv_equals(name, SV("defined"))){
            CPPToken next;
            for(;;){
                next = cpp_ts_next(s);
                if(next.type == CPP_WHITESPACE)
                    continue;
                break;
            }
            _Bool paren = 0;
            if(next.type == CPP_PUNCTUATOR && next.punct == '('){
                paren = 1;
                for(;;){
                    next = cpp_ts_next(s);
                    if(next.type == CPP_WHITESPACE)
                        continue;
                    break;
                }
            }
            if(next.type != CPP_IDENTIFIER){
                return cpp_error(cpp, next.loc, "Need an identifier for defined");
            }
            CPPToken t = {
                .type = CPP_NUMBER,
                .loc = tok.loc,
                .txt = cpp_isdef(cpp, next.txt)?SV("1"):SV("0"),
            };
            if(paren){
                for(;;){
                    next = cpp_ts_next(s);
                    if(next.type == CPP_WHITESPACE)
                        continue;
                    break;
                }
                if(next.type != CPP_PUNCTUATOR || next.punct != ')'){
                    return cpp_error(cpp, next.loc, "Needed ')' for defined()");
                }
            }
            *outtok = t;
            return 0;
        }
        Atom a = AT_get_atom(cpp->at, name.text, name.length);
        if(!a){
            *outtok = (CPPToken){.type=CPP_NUMBER, .txt = SV("0"), .loc = tok.loc};
            return 0;
        }
        CMacro* m = AM_get(&cpp->macros, a);
        if(!m || m->is_disabled){
            *outtok = (CPPToken){.type=CPP_NUMBER, .txt = SV("0"), .loc = tok.loc};
            return 0;
        }
        if(!m->is_function_like){
            err = cpp_expand_obj_macro(cpp, m, tok.loc, s->pending);
            if(err) return err;
            continue;
        }
        // function-like macro: check for '('
        {
            CPPToken next;
            do {
                next = cpp_ts_next(s);
            } while(next.type == CPP_WHITESPACE);
            if(next.type != CPP_PUNCTUATOR || next.punct != '('){
                // not an invocation, push back and treat as 0
                err = cpp_push_tok(cpp, s->pending, next);
                if(err) return err;
                *outtok = (CPPToken){.type=CPP_NUMBER, .txt = SV("0"), .loc = tok.loc};
                return 0;
            }
            CPPTokens *args = cpp_get_scratch(cpp);
            Marray(size_t) *arg_seps = cpp_get_scratch_idxes(cpp);
            if(!args || !arg_seps) return CPP_OOM_ERROR;
            for(int paren = 1;;){
                next = cpp_ts_next(s);
                if(next.type == CPP_EOF){
                    err = cpp_error(cpp, tok.loc, "EOF in function-like macro invocation");
                    goto func_cleanup;
                }
                if(next.type == CPP_PUNCTUATOR){
                    if(next.punct == ')'){
                        paren--;
                        if(!paren) break;
                    }
                    else if(next.punct == '(')
                        paren++;
                    else if(next.punct == ',' && paren == 1){
                        if(m->is_variadic || (m->nparams > 1 && arg_seps->count < (size_t)m->nparams-1)){
                            err = ma_push(size_t)(arg_seps, cpp->allocator, args->count);
                            if(err) goto func_cleanup;
                        }
                        else {
                            err = cpp_error(cpp, next.loc, "Too many arguments to function-like macro");
                            goto func_cleanup;
                        }
                    }
                }
                err = cpp_push_tok(cpp, args, next);
                if(err) goto func_cleanup;
            }
            if(args->count && !m->nparams && !m->is_variadic){
                err = cpp_error(cpp, tok.loc, "Too many arguments to function-like macro");
                goto func_cleanup;
            }
            if(arg_seps->count+1 < m->nparams){
                err = cpp_error(cpp, tok.loc, "Too few arguments to function-like macro");
                goto func_cleanup;
            }
            err = cpp_expand_func_macro(cpp, m, tok.loc, args, arg_seps, s->pending);
            func_cleanup:
            cpp_release_scratch_idxes(cpp, arg_seps);
            cpp_release_scratch(cpp, args);
            if(err) return err;
            continue;
        }
    }
}

static
CPPToken
cpp_ts_next(CppTokenStream* s){
    CPPToken tok;
    for(;;){
        if(s->pending->count)
            tok = ma_pop_(*s->pending);
        else if(s->cursor < s->count)
            tok = s->toks[s->cursor++];
        else
            tok = (CPPToken){0};
        if(tok.type == CPP_REENABLE){
            ((CMacro*)tok.data1)->is_disabled = 0;
            continue;
        }
        break;
    }
    return tok;
}

static
int
cpp_eval_parse_number(CPreprocessor* cpp, CPPToken tok, int64_t* value){
    const char* s = tok.txt.text;
    size_t len = tok.txt.length;
    // Strip integer suffixes: [uUlL]+
    while(len && (s[len-1] == 'u' || s[len-1] == 'U' || s[len-1] == 'l' || s[len-1] == 'L'))
        len--;
    if(!len)
        return cpp_error(cpp, tok.loc, "Invalid number literal");
    uint64_t v = 0;
    if(len > 2 && s[0] == '0' && (s[1] == 'x' || s[1] == 'X')){
        Uint64Result u = parse_hex(s, len);
        if(u.errored) return cpp_error(cpp, tok.loc, "Invalid hex digit in number");
        v = u.result;
    }
    else if(len > 2 && s[0] == '0' && (s[1] == 'b' || s[1] == 'B')){
        Uint64Result u = parse_binary(s, len);
        if(u.errored) return cpp_error(cpp, tok.loc, "Invalid binary digit in number");
        v = u.result;
    }
    else if(len > 1 && s[0] == '0'){
        // TODO: overflow handling etc.
        for(size_t i = 1; i < len; i++){
            if(s[i] < '0' || s[i] > '7')
                return cpp_error(cpp, tok.loc, "Invalid octal digit in number");
            v = (v << 3) | (uint64_t)(s[i] - '0');
        }
    }
    else{
        Uint64Result u = parse_uint64(s, len);
        if(u.errored) return cpp_error(cpp, tok.loc, "Invalid digit in number");
        v = u.result;
    }
    *value = (int64_t)v;
    return 0;
}

static
int
cpp_eval_parse_char(CPreprocessor* cpp, CPPToken tok, int64_t* value){
    const char* s = tok.txt.text;
    size_t len = tok.txt.length;
    if(len < 3 || s[0] != '\'' || s[len-1] != '\'')
        return cpp_error(cpp, tok.loc, "Invalid character constant");
    const char* p = s + 1;
    const char* e = s + len - 1;
    int64_t v = 0;
    while(p < e){
        unsigned char c;
        if(*p == '\\'){
            p++;
            if(p == e)
                return cpp_error(cpp, tok.loc, "Invalid escape in character constant");
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
                default:
                    c = (unsigned char)*p; p++; break;
            }
        }
        else
            c = (unsigned char)*p++;
        v = (v << 8) | c;
    }
    *value = v;
    return 0;
}

static
int
cpp_eval_binop_prec(CPPToken tok){
    if(tok.type != CPP_PUNCTUATOR) return 0;
    switch(tok.punct){
        case '*':
        case '/':
        case '%':
            return 11;
        case '+':
        case '-':
            return 10;
        case '<<':
        case '>>':
            return 9;
        case '<':
        case '>':
        case '<=':
        case '>=':
            return 8;
        case '==':
        case '!=':
            return 7;
        case '&':  return 6;
        case '^':  return 5;
        case '|':  return 4;
        case '&&': return 3;
        case '||': return 2;
        case '?':  return 1;
        default:   return 0;
    }
}

static
int
cpp_eval_atom(CPreprocessor* cpp, CppTokenStream* s, int64_t* value){
    int err;
    CPPToken tok;
    err = cpp_eval_ts_next(cpp, s, &tok);
    if(err) return err;
    if(tok.type == CPP_EOF)
        return cpp_error(cpp, tok.loc, "Unexpected end of #if expression");
    switch(tok.type){
        case CPP_NUMBER:
            return cpp_eval_parse_number(cpp, tok, value);
        case CPP_CHAR:
            return cpp_eval_parse_char(cpp, tok, value);
        case CPP_PUNCTUATOR:
            switch(tok.punct){
                case '(':
                    err = cpp_recursive_eval_prec(cpp, s, value, 1);
                    if(err) return err;
                    err = cpp_eval_ts_next(cpp, s, &tok);
                    if(err) return err;
                    if(tok.type != CPP_PUNCTUATOR || tok.punct != ')')
                        return cpp_error(cpp, tok.loc, "Expected ')' in expression");
                    return 0;
                case '!':
                    err = cpp_eval_atom(cpp, s, value);
                    if(err) return err;
                    *value = !*value;
                    return 0;
                case '~':
                    err = cpp_eval_atom(cpp, s, value);
                    if(err) return err;
                    *value = ~*value;
                    return 0;
                case '+':
                    return cpp_eval_atom(cpp, s, value);
                case '-':
                    err = cpp_eval_atom(cpp, s, value);
                    if(err) return err;
                    *value = -*value;
                    return 0;
                default:
                    return cpp_error(cpp, tok.loc, "Unexpected punctuator in #if expression");
            }
        default:
            return cpp_error(cpp, tok.loc, "Unexpected token in #if expression");
    }
}

static
int
cpp_recursive_eval_prec(CPreprocessor* cpp, CppTokenStream* s, int64_t* value, int min_prec){
    int err;
    err = cpp_eval_atom(cpp, s, value);
    if(err) return err;
    for(;;){
        CPPToken tok;
        err = cpp_eval_ts_next(cpp, s, &tok);
        if(err) return err;
        if(tok.type == CPP_EOF)
            break;
        int prec = cpp_eval_binop_prec(tok);
        if(!prec || prec < min_prec){
            err = cpp_push_tok(cpp, s->pending, tok);
            if(err) return err;
            break;
        }
        if(tok.punct == '?'){
            int64_t mid, right;
            err = cpp_recursive_eval_prec(cpp, s, &mid, 1);
            if(err) return err;
            CPPToken colon;
            err = cpp_eval_ts_next(cpp, s, &colon);
            if(err) return err;
            if(colon.type != CPP_PUNCTUATOR || colon.punct != ':')
                return cpp_error(cpp, colon.loc, "Expected ':' in ternary expression");
            err = cpp_recursive_eval_prec(cpp, s, &right, 1);
            if(err) return err;
            *value = *value ? mid : right;
        }
        else{
            // TODO overflow etc.
            int64_t rhs;
            err = cpp_recursive_eval_prec(cpp, s, &rhs, prec + 1);
            if(err) return err;
            switch(tok.punct){
                case '*':  *value = *value * rhs; break;
                case '/':
                    if(!rhs) return cpp_error(cpp, tok.loc, "Division by zero in #if");
                    *value = *value / rhs;
                    break;
                case '%':
                    if(!rhs) return cpp_error(cpp, tok.loc, "Modulo by zero in #if");
                    *value = *value % rhs;
                    break;
                case '+':  *value = *value + rhs; break;
                case '-':  *value = *value - rhs; break;
                case '<<': *value = *value << rhs; break;
                case '>>': *value = *value >> rhs; break;
                case '<':  *value = (*value < rhs); break;
                case '>':  *value = (*value > rhs); break;
                case '<=': *value = (*value <= rhs); break;
                case '>=': *value = (*value >= rhs); break;
                case '==': *value = (*value == rhs); break;
                case '!=': *value = (*value != rhs); break;
                case '&':  *value = *value & rhs; break;
                case '^':  *value = *value ^ rhs; break;
                case '|':  *value = *value | rhs; break;
                case '&&': *value = *value && rhs; break;
                case '||': *value = *value || rhs; break;
                default:
                    return cpp_error(cpp, tok.loc, "Unknown operator in #if");
            }
        }
    }
    return 0;
}

static
int
cpp_recursive_eval(CPreprocessor* cpp, CppTokenStream* s, int64_t* value){
    return cpp_recursive_eval_prec(cpp, s, value, 1);
}

static
int
cpp_register_pragma(CPreprocessor* cpp, StringView name, CppPragmaFn* fn, void* _Null_unspecified ctx){
    Atom a = AT_atomize(cpp->at, name.text, name.length);
    if(!a) return CPP_OOM_ERROR;
    CPragma* prag = AM_get(&cpp->pragmas, a);
    if(!prag){
        prag = Allocator_zalloc(cpp->allocator, sizeof *prag);
        if(!prag) return CPP_OOM_ERROR;
        int err = AM_put(&cpp->pragmas, cpp->allocator, a, prag);
        if(err) {
            Allocator_free(cpp->allocator, prag, sizeof *prag);
            return CPP_OOM_ERROR;
        }
    }
    prag->fn = fn;
    prag->ctx = ctx;
    return 0;
}

static
int
cpp_mixin_string(CPreprocessor* cpp, SrcLoc loc, StringView str, CPPTokens* out){
    MStringBuilder sb = {.allocator=allocator_from_arena(&cpp->synth_arena)};
    for(size_t i = 0; i < str.length;){
        unsigned char c = (unsigned char)str.text[i++];
        if(c != '\\'){
            msb_write_char(&sb, c);
            continue;
        }
        if(i >= str.length) break;
        c = (unsigned char)str.text[i++];
        unsigned char t;
        switch(c){
            case '\\': msb_write_char(&sb, c); break;
            case 'n': msb_write_char(&sb, '\n'); break;
            case 't': msb_write_char(&sb, '\t'); break;
            case 'r': msb_write_char(&sb, '\r'); break;
            case '\'': msb_write_char(&sb, '\''); break;
            case '"': msb_write_char(&sb, '"'); break;
            case 'a': msb_write_char(&sb, '\a'); break;
            case 'b': msb_write_char(&sb, '\b'); break;
            case 'f': msb_write_char(&sb, '\f'); break;
            case 'v': msb_write_char(&sb, '\v'); break;
            case '0': case '1': case '2': case '3':
            case '4': case '5': case '6': case '7':
                t = 0;
                for(size_t j = 0; j < 3 && i < str.length; j++){
                    t = (unsigned char)((t << 3)|(unsigned char)str.text[i++] - '0');
                }
                msb_write_char(&sb, t);
                break;
            default:
                return CPP_UNIMPLEMENTED_ERROR;
        }
    }

    CPPFrame frame = {
        .txt = sb.cursor?msb_detach_sv(&sb):SV(""),
        .file_id = loc.is_actually_a_pointer?((SrcLocExp*)(loc.pointer.bits<<1))->file_id:loc.file_id,
        .line = loc.is_actually_a_pointer?((SrcLocExp*)(loc.pointer.bits<<1))->line:loc.line,
        .column = loc.is_actually_a_pointer?((SrcLocExp*)(loc.pointer.bits<<1))->column:loc.column,
    };
    for(;;){
        CPPToken tok;
        int err = cpp_tokenize_from_frame(cpp, &frame, &tok);
        if(err) return err;
        if(tok.type == CPP_EOF) break;
        tok.loc = loc;
        err = cpp_push_tok(cpp, out, tok);
        if(err) return err;
    }
    return 0;
}

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#ifdef __clang__
#pragma clang assume_nonnull end
#endif
#endif
