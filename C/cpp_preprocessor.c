#ifndef C_CPP_PREPROCESSOR_C
#define C_CPP_PREPROCESSOR_C
//
// Copyright © 2026-2026, David Priver <david@davidpriver.com>
//
#include <stdarg.h>
#include <limits.h>
#ifndef _WIN32
#include <sys/stat.h>
#else
#include "../Drp/MStringBuilder16.h"
#endif
#include "cpp_preprocessor.h"
#include "cpp_tok.h"
#include "cc_errors.h"
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
static int cpp_next_raw_token(CPreprocessor*, CppToken*);
static int cpp_next_pp_token(CPreprocessor*, CppToken*);
LOG_PRINTF(3, 4) static int cpp_error(CPreprocessor*, SrcLoc, const char*, ...);
LOG_PRINTF(3, 4) static void cpp_warn(CPreprocessor*, SrcLoc, const char*, ...);
LOG_PRINTF(3, 4) static void cpp_info(CPreprocessor*, SrcLoc, const char*, ...);
static int cpp_push_if(CPreprocessor* cpp, CppPoundIf s);

static CppTokens*_Nullable cpp_get_scratch(CPreprocessor*);
static Marray(size_t)*_Nullable cpp_get_scratch_idxes(CPreprocessor*);
static void cpp_release_scratch(CPreprocessor*, CppTokens*);
static void cpp_release_scratch_idxes(CPreprocessor*, Marray(size_t)*);
static int cpp_substitute_and_paste(CPreprocessor*, const CppToken*, size_t, const CMacro*, const CppTokens*, const Marray(size_t)*, CppTokens*_Nullable*_Null_unspecified, CppTokens*, _Bool, SrcLocExp*);
static int cpp_expand_argument(CPreprocessor *cpp, const CppToken*_Null_unspecified toks, size_t count, CppTokens *out);
LOG_PRINTF(2, 3) static Atom _Nullable cpp_atomizef(CPreprocessor*, const char* fmt, ...);
static int cpp_eval_tokens(CPreprocessor*, CppToken*_Null_unspecified toks, size_t count, int64_t* value);
// str should exclude outer quotes
static int cpp_mixin_string(CPreprocessor* cpp, SrcLoc loc, StringView str, CppTokens* out);
enum {
    CPP_NO_ERROR                       = _cc_no_error,
    CPP_OOM_ERROR                      = _cc_oom_error,
    CPP_SYNTAX_ERROR                   = _cc_syntax_error,
    CPP_UNREACHABLE_ERROR              = _cc_unreachable_error,
    CPP_UNIMPLEMENTED_ERROR            = _cc_unimplemented_error,
    CPP_FILE_NOT_FOUND_ERROR           = _cc_file_not_found_error,
    CPP_MACRO_ALREADY_EXISTS_ERROR     = _cc_macro_already_exists_error,
    CPP_REDEFINING_BUILTIN_MACRO_ERROR = _cc_redefining_builtin_macro_error,
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
    size_t size = sizeof *macro + sizeof(CppToken)*ntoks + sizeof(Atom)*nparams;
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
        size = sizeof *macro + sizeof(CppToken)*2 + sizeof(Atom)*macro->nparams;
    }
    else
        size = sizeof *macro + sizeof(CppToken)*macro->nreplace + sizeof(Atom)*macro->nparams;
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
    if(sv_equals(name, SV("__has_include_next"))) return 1;
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
cpp_define_obj_macro(CPreprocessor* cpp, StringView name, CppToken*_Null_unspecified toks, size_t ntoks){
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
_Bool
cpp_is_pragma_once(CPreprocessor* cpp, uint32_t file_id){
    // Binary search in sorted list
    size_t lo = 0, hi = cpp->pragma_once_files.count;
    while(lo < hi){
        size_t mid = lo + (hi - lo) / 2;
        if(cpp->pragma_once_files.data[mid] < file_id)
            lo = mid + 1;
        else
            hi = mid;
    }
    return lo < cpp->pragma_once_files.count && cpp->pragma_once_files.data[lo] == file_id;
}

static
int
cpp_add_pragma_once(CPreprocessor* cpp, uint32_t file_id){
    // Insert into sorted position
    size_t lo = 0, hi = cpp->pragma_once_files.count;
    while(lo < hi){
        size_t mid = lo + (hi - lo) / 2;
        if(cpp->pragma_once_files.data[mid] < file_id)
            lo = mid + 1;
        else
            hi = mid;
    }
    if(lo < cpp->pragma_once_files.count && cpp->pragma_once_files.data[lo] == file_id)
        return 0; // already present
    int err = ma_push(uint32_t)(&cpp->pragma_once_files, cpp->allocator, file_id);
    if(err) return CPP_OOM_ERROR;
    // Shift elements to maintain sorted order
    size_t tail = cpp->pragma_once_files.count - 1 - lo;
    if(tail)
        memmove(&cpp->pragma_once_files.data[lo+1], &cpp->pragma_once_files.data[lo], tail * sizeof *cpp->pragma_once_files.data);
    cpp->pragma_once_files.data[lo] = file_id;
    return 0;
}

// Check if the remaining bytes in a frame (from cursor to end) are only
// whitespace, newlines, and comments. Returns 1 if so, 0 otherwise.
static
_Bool
cpp_frame_only_whitespace_left(CppFrame* f){
    const char* p = f->txt.text + f->cursor;
    const char* end = f->txt.text + f->txt.length;
    while(p < end){
        char c = *p;
        if(c == ' ' || c == '\t' || c == '\n' || c == '\r'){
            p++;
            continue;
        }
        if(c == '/' && p + 1 < end){
            if(p[1] == '/'){
                // line comment - skip to end of line
                p += 2;
                while(p < end && *p != '\n') p++;
                continue;
            }
            if(p[1] == '*'){
                // block comment - skip to */
                p += 2;
                while(p < end){
                    if(*p == '*' && p + 1 < end && p[1] == '/'){
                        p += 2;
                        break;
                    }
                    p++;
                }
                continue;
            }
        }
        return 0;
    }
    return 1;
}

static
Atom _Nullable
cpp_get_include_guard(CPreprocessor* cpp, uint32_t file_id){
    size_t lo = 0, hi = cpp->include_guard_files.count;
    while(lo < hi){
        size_t mid = lo + (hi - lo) / 2;
        if(cpp->include_guard_files.data[mid] < file_id)
            lo = mid + 1;
        else
            hi = mid;
    }
    if(lo < cpp->include_guard_files.count && cpp->include_guard_files.data[lo] == file_id)
        return cpp->include_guard_macros.data[lo];
    return NULL;
}

static
int
cpp_add_include_guard(CPreprocessor* cpp, uint32_t file_id, Atom guard){
    size_t lo = 0, hi = cpp->include_guard_files.count;
    while(lo < hi){
        size_t mid = lo + (hi - lo) / 2;
        if(cpp->include_guard_files.data[mid] < file_id)
            lo = mid + 1;
        else
            hi = mid;
    }
    if(lo < cpp->include_guard_files.count && cpp->include_guard_files.data[lo] == file_id)
        return 0; // already present
    int err = ma_push(uint32_t)(&cpp->include_guard_files, cpp->allocator, file_id);
    if(err) return CPP_OOM_ERROR;
    err = ma_push(Atom)(&cpp->include_guard_macros, cpp->allocator, guard);
    if(err){
        cpp->include_guard_files.count--;
        return CPP_OOM_ERROR;
    }
    size_t tail = cpp->include_guard_files.count - 1 - lo;
    if(tail){
        memmove(&cpp->include_guard_files.data[lo+1], &cpp->include_guard_files.data[lo], tail * sizeof *cpp->include_guard_files.data);
        memmove(&cpp->include_guard_macros.data[lo+1], &cpp->include_guard_macros.data[lo], tail * sizeof *cpp->include_guard_macros.data);
    }
    cpp->include_guard_files.data[lo] = file_id;
    cpp->include_guard_macros.data[lo] = guard;
    return 0;
}

// Find an include file. Returns 0 on success, with the resolved path left
// in fc->path_builder (caller must call fc_read_file or msb_reset).
// out_pos is set to the position in the include path arrays for include_next.
static
int
cpp_find_include(CPreprocessor* cpp, _Bool quote, _Bool is_next, StringView header_name, IncludePosition* out_pos){
    MStringBuilder* sb = fc_path_builder(cpp->fc);
    // For quoted includes (not include_next), first search the current file's directory
    if(quote && !is_next){
        CppFrame* frame = &ma_tail(cpp->frames);
        if(frame->file_id < cpp->fc->map.count){
            LongString file_path = cpp->fc->map.data[frame->file_id].path;
            StringView dir = path_dirname(LS_to_SV(file_path), 0);
            msb_reset(sb);
            if(dir.length){
                msb_write_str(sb, dir.text, dir.length);
                if(dir.text[dir.length-1] != '/')
                    msb_write_char(sb, '/');
            }
            msb_write_str(sb, header_name.text, header_name.length);
            if(fc_is_file(cpp->fc)){
                // fc_is_file consumed the path, rebuild it
                if(dir.length){
                    msb_write_str(sb, dir.text, dir.length);
                    if(dir.text[dir.length-1] != '/')
                        msb_write_char(sb, '/');
                }
                msb_write_str(sb, header_name.text, header_name.length);
                *out_pos = (IncludePosition){0, 0};
                return 0;
            }
        }
    }
    // Search include path arrays
    size_t start_array = quote ? 0 : 1;
    size_t start_idx = 0;
    if(is_next){
        CppFrame* frame = &ma_tail(cpp->frames);
        start_array = frame->include_position.array;
        start_idx = frame->include_position.idx + 1;
    }
    for(size_t i = start_array; i < arrlen(cpp->include_paths); i++){
        Marray(StringView)* dirs = &cpp->include_paths[i];
        size_t j_start = (i == start_array && is_next) ? start_idx : 0;
        for(size_t j = j_start; j < dirs->count; j++){
            StringView d = dirs->data[j];
            msb_reset(sb);
            if(!d.length) continue;
            msb_write_str(sb, d.text, d.length);
            if(msb_peek(sb) != '/')
                msb_write_char(sb, '/');
            msb_write_str(sb, header_name.text, header_name.length);
            if(fc_is_file(cpp->fc)){
                // fc_is_file consumed the path, rebuild it
                msb_write_str(sb, d.text, d.length);
                if(d.text[d.length-1] != '/')
                    msb_write_char(sb, '/');
                msb_write_str(sb, header_name.text, header_name.length);
                *out_pos = (IncludePosition){i, j};
                return 0;
            }
        }
    }
    // Framework search: #include <Foo/Bar.h> -> {fwk}/Foo.framework/Headers/Bar.h
    if(cpp->framework_paths.count){
        const char* slash = memchr(header_name.text, '/', header_name.length);
        if(slash && !memchr(slash+1, '/', header_name.text+header_name.length-slash-1)){
            size_t fw_len = (size_t)(slash - header_name.text);
            const char* remaining = slash + 1;
            size_t remaining_len = header_name.length - fw_len - 1;
            for(size_t i = 0; i < cpp->framework_paths.count; i++){
                StringView d = cpp->framework_paths.data[i];
                msb_reset(sb);
                msb_write_str(sb, d.text, d.length);
                if(d.text[d.length-1] != '/')
                    msb_write_char(sb, '/');
                msb_write_str(sb, header_name.text, fw_len);
                msb_write_str(sb, ".framework/Headers/", 19);
                msb_write_str(sb, remaining, remaining_len);
                if(fc_is_file(cpp->fc)){
                    // fc_is_file consumed the path, rebuild it
                    msb_write_str(sb, d.text, d.length);
                    if(d.text[d.length-1] != '/')
                        msb_write_char(sb, '/');
                    msb_write_str(sb, header_name.text, fw_len);
                    msb_write_str(sb, ".framework/Headers/", 19);
                    msb_write_str(sb, remaining, remaining_len);
                    *out_pos = (IncludePosition){0, 0};
                    return 0;
                }
            }
        }
    }
    return CPP_FILE_NOT_FOUND_ERROR;
}

static
StringView
cpp_str_prefix(StringView sv){
    const char* q = memchr(sv.text, '"', sv.length);
    if(!q) return SV("");
    return (StringView){q-sv.text, sv.text};
}

static
int
cpp_merge_str_prefix(StringView sv, StringView* prefix){
    StringView p = cpp_str_prefix(sv);
    if(!p.length) return 0;
    if(!prefix->length){
        *prefix = p;
        return 0;
    }
    if(sv_equals(p, *prefix))
        return 0;
    return 1;
}

static
int
cpp_next_token(CPreprocessor* cpp, CppToken* tok){
    // phase 6, part of 7 (elide phase 5 of character set conversion)
    for(;;){
        int err = cpp_next_pp_token(cpp, tok);
        if(err) return err;
        switch(tok->type){
            // whitespace no longer significant, remove
            case CPP_WHITESPACE:
            case CPP_NEWLINE:
                continue;
            case CPP_PLACEMARKER:
            case CPP_REENABLE:
            case CPP_HEADER_NAME:
                return CPP_UNREACHABLE_ERROR;
            case CPP_PUNCTUATOR:
            case CPP_EOF:
            case CPP_OTHER:
            case CPP_NUMBER:
            case CPP_IDENTIFIER:
                return 0;
            case CPP_CHAR: // TODO: merge adjacent char literals? is that a thing?
                return 0;
            case CPP_STRING:{ // concatenate adjacent strings
                CppToken next;
                do {
                    err = cpp_next_pp_token(cpp, &next);
                    if(err) return err;
                }while(next.type == CPP_WHITESPACE || next.type == CPP_NEWLINE);
                if(next.type != CPP_STRING){
                    err = cpp_push_tok(cpp, &cpp->pending, next);
                    if(err) return err;
                    return 0;
                }
                CppTokens* strings = cpp_get_scratch(cpp);
                if(!strings) return CPP_OOM_ERROR;
                err = cpp_push_tok(cpp, strings, *tok);
                StringView prefix = cpp_str_prefix(tok->txt);
                if(err) goto string_finally;
                err = cpp_merge_str_prefix(next.txt, &prefix);
                if(err){
                    err = cpp_error(cpp, next.loc, "Invalid string concatenation (different prefixes)");
                    goto string_finally;
                }
                err = cpp_push_tok(cpp, strings, next);
                if(err) goto string_finally;
                for(;;){
                    do {
                        err = cpp_next_pp_token(cpp, &next);
                        if(err) goto string_finally;
                    }while(next.type == CPP_WHITESPACE || next.type == CPP_NEWLINE);
                    if(next.type != CPP_STRING){
                        err = cpp_push_tok(cpp, &cpp->pending, next);
                        if(err) goto string_finally;
                        break;
                    }
                    err = cpp_merge_str_prefix(next.txt, &prefix);
                    if(err){
                        err = cpp_error(cpp, next.loc, "Invalid string concatenation (different prefixes)");
                        goto string_finally;
                    }
                    err = cpp_push_tok(cpp, strings, next);
                    if(err) goto string_finally;
                }
                MStringBuilder sb = {.allocator=allocator_from_arena(&cpp->synth_arena)};
                msb_write_str(&sb, prefix.text, prefix.length);
                msb_write_char(&sb, '"');
                for(size_t i = 0; i < strings->count; i++){
                    StringView s = strings->data[i].txt;
                    while(s.text[0] != '"'){
                        s.text++;
                        s.length--;
                    }
                    s.text++;
                    s.length--;
                    s.length--;
                    if(s.length) msb_write_str(&sb, s.text, s.length);
                }
                msb_write_char(&sb, '"');
                if(sb.errored){
                    msb_destroy(&sb);
                    goto string_finally;
                }
                tok->txt = msb_detach_sv(&sb);
                string_finally:
                cpp_release_scratch(cpp, strings);
                return err;
            }
        }
        return 0;
    }
}
static int cpp_handle_directive(CPreprocessor *cpp);
static int cpp_handle_directive_in_inactive_region(CPreprocessor *cpp);
static int cpp_expand_obj_macro(CPreprocessor *cpp, CMacro *macro, SrcLoc expansion_loc, CppTokens *dst);
static int cpp_expand_func_macro(CPreprocessor *cpp, CMacro *macro, SrcLoc expansion_loc, const CppTokens *args, const Marray(size_t) *arg_seps, CppTokens *dst);

static
int
cpp_next_pp_token(CPreprocessor* cpp, CppToken* ptok){
    // phase 4
    for(;;){
        CppToken tok;
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
                    CppToken next;
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
                    CppTokens *args = cpp_get_scratch(cpp);
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
                return cpp_error(cpp, tok.loc, "%.*s token escaped", sv_p(CppTokenTypeSV[tok.type]));
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
cpp_handle_continuation(CppFrame* f){
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
cpp_next_char(CppFrame* f){
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
cpp_peek_char(CppFrame* f){
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
cpp_match_char(CppFrame* f, int ch){
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
cpp_match_2char(CppFrame* f, int ch1, int ch2){
    struct CppFrameLoc loc = f->loc;
    if(cpp_match_char(f, ch1) && cpp_match_char(f, ch2)){
        return 1;
    }
    f->loc = loc;
    return 0;
}

static
inline
_Bool
cpp_match_oneof(CppFrame* f, const char* set){
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
cpp_tokenize_from_frame(CPreprocessor* cpp, CppFrame* f, CppToken* tok){
    retry:;
    SrcLoc loc = {.file_id = f->file_id, .line = f->line, .column = f->column};
    cpp_handle_continuation(f);
    size_t start = f->cursor;
    int c = cpp_next_char(f);
    switch(c){
        default:
            default_:
            *tok = (CppToken){.type = CPP_OTHER, .txt = {1, &f->txt.text[start]}, .loc = loc};
            return 0;
        case -1:
            *tok = (CppToken){.type = CPP_EOF, .loc = loc};
            return 0;
        case '\n':
            *tok = (CppToken){.type = CPP_NEWLINE, .txt = SV("\n"), .loc = loc};
            return 0;
        // Whitespace
        case ' ': case '\t': case '\r': case '\f': case '\v':
            while(cpp_match_oneof(f, " \t\r\f\v"))
                ;                                            // don't clean whitespace
            *tok = (CppToken){.type = CPP_WHITESPACE, .txt = {f->cursor-start, &f->txt.text[start]}, .loc = loc};
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
            *tok = (CppToken){.type = CPP_IDENTIFIER, .txt = cpp_clean_token(cpp, f->cursor-start, &f->txt.text[start]), .loc = loc};
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
            *tok = (CppToken){.type = CPP_NUMBER, .txt = cpp_clean_token(cpp, f->cursor-start, &f->txt.text[start]), .loc = loc};
            return 0;
        case '.':{
            c = cpp_peek_char(f);
            if(c >= '0' && c <= '9'){
                f->cursor++;
                f->column++;
                goto pp_number;
            }
            if(cpp_match_2char(f, '.', '.')){
                *tok = (CppToken){.type = CPP_PUNCTUATOR, .txt = SV("..."), .punct='...', .loc = loc};
                return 0;
            }
            *tok = (CppToken){.type = CPP_PUNCTUATOR, .txt = SV("."), .punct='.', .loc = loc};
            return 0;
        }
        // String / character literal
        case '"':
        case '\'':
        string_or_char:{
            int terminator = (c == '"') ? '"' : '\'';
            CppTokenType type = (terminator == '"') ? CPP_STRING : CPP_CHAR;
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
            *tok = (CppToken){.type = type, .txt = cpp_clean_token(cpp, f->cursor-start, &f->txt.text[start]), .loc = loc};
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
                *tok = (CppToken){.type = CPP_WHITESPACE, .txt = {f->cursor-start, &f->txt.text[start]}, .loc = loc};
                return 0;
            }
            else if(cpp_match_char(f, '*')){ // C comment
                for(;;){
                    c = cpp_next_char(f);
                    if(c == -1)
                        return cpp_error(cpp, loc, "Unterminated comment");
                    if(c == '*' && cpp_match_char(f, '/')){       // don't bother cleaning continuations, it's a comment
                        *tok = (CppToken){.type = CPP_WHITESPACE, .txt = {f->cursor-start, &f->txt.text[start]}, .loc = loc};
                        return 0;
                    }
                }
            }
            if(cpp_match_char(f, '='))
                *tok = (CppToken){.type = CPP_PUNCTUATOR, .txt = SV("/="), .punct='/=', .loc = loc};
            else
                *tok = (CppToken){.type = CPP_PUNCTUATOR, .txt = SV("/"), .punct='/', .loc = loc};
            return 0;
        }
        case '#':{
            if(cpp_match_char(f, '#'))
                *tok = (CppToken){.type = CPP_PUNCTUATOR, .txt = SV("##"), .punct='##', .loc = loc};
            else
                *tok = (CppToken){.type = CPP_PUNCTUATOR, .txt = SV("#"), .punct='#', .loc = loc};
            return 0;
        }
        case '*':
            if(cpp_match_char(f, '='))
                *tok = (CppToken){.type = CPP_PUNCTUATOR, .txt=SV("*="), .punct='*=', .loc = loc};
            else
                *tok = (CppToken){.type = CPP_PUNCTUATOR, .txt=SV("*"), .punct='*', .loc = loc};
            return 0;
        case '~':
            *tok = (CppToken){.type = CPP_PUNCTUATOR, .txt = SV("~"), .punct='~', .loc = loc};
            return 0;
        case '!':
            if(cpp_match_char(f, '='))
                *tok = (CppToken){.type = CPP_PUNCTUATOR, .txt = SV("!="), .punct='!=', .loc = loc};
            else
                *tok = (CppToken){.type = CPP_PUNCTUATOR, .txt = SV("!"), .punct='!', .loc = loc};
            return 0;
        case '%':
            if(cpp_match_char(f, ':')){
                // %:%:
                if(cpp_match_2char(f, '%', ':')){
                    *tok = (CppToken){.type = CPP_PUNCTUATOR, .txt = SV("##"), .punct='##', .loc = loc};
                    return 0;
                }
                *tok = (CppToken){.type = CPP_PUNCTUATOR, .txt = SV("#"), .punct='#', .loc = loc};
                return 0;
            }
            if(cpp_match_char(f, '>')){
                *tok = (CppToken){.type = CPP_PUNCTUATOR, .txt = SV("}"), .punct='}', .loc = loc};
                return 0;
            }
            if(cpp_match_char(f, '='))
                *tok = (CppToken){.type = CPP_PUNCTUATOR, .txt = SV("%="), .punct='%=', .loc = loc};
            else
                *tok = (CppToken){.type = CPP_PUNCTUATOR, .txt = SV("%"), .punct='%', .loc = loc};
            return 0;
        case '^':
            if(cpp_match_char(f, '='))
                *tok = (CppToken){.type = CPP_PUNCTUATOR, .txt = SV("^="), .punct='^=', .loc = loc};
            else
                *tok = (CppToken){.type = CPP_PUNCTUATOR, .txt = SV("^"), .punct='^', .loc = loc};
            return 0;
        case '=':
            if(cpp_match_char(f, '='))
                *tok = (CppToken){.type = CPP_PUNCTUATOR, .txt = SV("=="), .punct='==', .loc = loc};
            else
                *tok = (CppToken){.type = CPP_PUNCTUATOR, .txt = SV("="), .punct='=', .loc = loc};
            return 0;
        case '-':
            if(cpp_match_char(f, '>')){
                *tok = (CppToken){.type = CPP_PUNCTUATOR, .txt = SV("->"), .punct='->', .loc = loc};
                return 0;
            }
            if(cpp_match_char(f, '-')){
                *tok = (CppToken){.type = CPP_PUNCTUATOR, .txt = SV("--"), .punct='--', .loc = loc};
                return 0;
            }
            if(cpp_match_char(f, '='))
                *tok = (CppToken){.type = CPP_PUNCTUATOR, .txt = SV("-="), .punct='-=', .loc = loc};
            else
                *tok = (CppToken){.type = CPP_PUNCTUATOR, .txt = SV("-"), .punct='-', .loc = loc};
            return 0;
        case '+':
            if(cpp_match_char(f, '+')){
                *tok = (CppToken){.type = CPP_PUNCTUATOR, .txt = SV("++"), .punct='++', .loc = loc};
                return 0;
            }
            if(cpp_match_char(f, '='))
                *tok = (CppToken){.type = CPP_PUNCTUATOR, .txt = SV("+="), .punct='+=', .loc = loc};
            else
                *tok = (CppToken){.type = CPP_PUNCTUATOR, .txt = SV("+"), .punct='+', .loc = loc};
            return 0;
        case '<':
            if(cpp_match_char(f, ':')){
                *tok = (CppToken){.type = CPP_PUNCTUATOR, .txt = SV("["), .punct='[', .loc = loc};
                return 0;
            }
            if(cpp_match_char(f, '%')){
                *tok = (CppToken){.type = CPP_PUNCTUATOR, .txt = SV("{"), .punct='{', .loc = loc};
                return 0;
            }
            if(cpp_match_char(f, '<')){
                if(cpp_match_char(f, '='))
                    *tok = (CppToken){.type = CPP_PUNCTUATOR, .txt = SV("<<="), .punct='<<=', .loc = loc};
                else
                    *tok = (CppToken){.type = CPP_PUNCTUATOR, .txt = SV("<<"), .punct='<<', .loc = loc};
                return 0;
            }
            if(cpp_match_char(f, '='))
                *tok = (CppToken){.type = CPP_PUNCTUATOR, .txt = SV("<="), .punct='<=', .loc = loc};
            else
                *tok = (CppToken){.type = CPP_PUNCTUATOR, .txt = SV("<"), .punct='<', .loc = loc};
            return 0;
        case '>':
            if(cpp_match_char(f, '>')){
                if(cpp_match_char(f, '='))
                    *tok = (CppToken){.type = CPP_PUNCTUATOR, .txt = SV(">>="), .punct='>>=', .loc = loc};
                else
                    *tok = (CppToken){.type = CPP_PUNCTUATOR, .txt = SV(">>"), .punct='>>', .loc = loc};
                return 0;
            }
            if(cpp_match_char(f, '='))
                *tok = (CppToken){.type = CPP_PUNCTUATOR, .txt = SV(">="), .punct='>=', .loc = loc};
            else
                *tok = (CppToken){.type = CPP_PUNCTUATOR, .txt = SV(">"), .punct='>', .loc = loc};
            return 0;
        case '&':
            if(cpp_match_char(f, '&')){
                *tok = (CppToken){.type = CPP_PUNCTUATOR, .txt = SV("&&"), .punct='&&', .loc = loc};
                return 0;
            }
            if(cpp_match_char(f, '='))
                *tok = (CppToken){.type = CPP_PUNCTUATOR, .txt = SV("&="), .punct='&=', .loc = loc};
            else
                *tok = (CppToken){.type = CPP_PUNCTUATOR, .txt = SV("&"), .punct='&', .loc = loc};
            return 0;
        case '|':
            if(cpp_match_char(f, '|')){
                *tok = (CppToken){.type = CPP_PUNCTUATOR, .txt = SV("||"), .punct='||', .loc = loc};
                return 0;
            }
            if(cpp_match_char(f, '='))
                *tok = (CppToken){.type = CPP_PUNCTUATOR, .txt = SV("|="), .punct='|=', .loc = loc};
            else
                *tok = (CppToken){.type = CPP_PUNCTUATOR, .txt = SV("|"), .punct='|', .loc = loc};
            return 0;
        case ':':
            if(cpp_match_char(f, '>')){
                *tok = (CppToken){.type = CPP_PUNCTUATOR, .txt = SV("]"), .punct=']', .loc = loc};
                return 0;
            }
            if(cpp_match_char(f, ':'))
                *tok = (CppToken){.type = CPP_PUNCTUATOR, .txt = SV("::"), .punct='::', .loc = loc};
            else
                *tok = (CppToken){.type = CPP_PUNCTUATOR, .txt = SV(":"), .punct=':', .loc = loc};
            return 0;
        case '(':
            *tok = (CppToken){.type = CPP_PUNCTUATOR, .txt = SV("("), .punct='(', .loc = loc};
            return 0;
        case ')':
            *tok = (CppToken){.type = CPP_PUNCTUATOR, .txt = SV(")"), .punct=')', .loc = loc};
            return 0;
        case '[':
            *tok = (CppToken){.type = CPP_PUNCTUATOR, .txt = SV("["), .punct='[', .loc = loc};
            return 0;
        case ']':
            *tok = (CppToken){.type = CPP_PUNCTUATOR, .txt = SV("]"), .punct=']', .loc = loc};
            return 0;
        case '{':
            *tok = (CppToken){.type = CPP_PUNCTUATOR, .txt = SV("{"), .punct='{', .loc = loc};
            return 0;
        case '}':
            *tok = (CppToken){.type = CPP_PUNCTUATOR, .txt = SV("}"), .punct='}', .loc = loc};
            return 0;
        case '?':
            *tok = (CppToken){.type = CPP_PUNCTUATOR, .txt = SV("?"), .punct='?', .loc = loc};
            return 0;
        case ';':
            *tok = (CppToken){.type = CPP_PUNCTUATOR, .txt = SV(";"), .punct=';', .loc = loc};
            return 0;
        case ',':
            *tok = (CppToken){.type = CPP_PUNCTUATOR, .txt = SV(","), .punct=',', .loc = loc};
            return 0;
    }
}

static
int
cpp_next_raw_token(CPreprocessor* cpp, CppToken* tok){
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
        *tok = (CppToken){.type = CPP_EOF};
        return 0;
    }
    CppFrame* f = &ma_tail(cpp->frames);
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
        e = (SrcLocExp*)(loc.bits & ~1);
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
    CppToken tok;
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
                if(m->nparams || m->is_function_like || m->nreplace){
                    cpp_error(cpp, tok.loc, "Duplicate object-like macro (%.*s) with different definitions", sv_p(name));
                    return cpp_error(cpp, m->def_loc, "... previously defined here");
                }
                err = 0;
            }
            if(err) return err;
            // push it back so dispatch loop sees newline
            return cpp_push_tok(cpp, &cpp->pending, tok);
        }
        else if(tok.type == CPP_PUNCTUATOR && tok.punct == '('){
            CppTokens *names = cpp_get_scratch(cpp);
            if(!names) return CPP_OOM_ERROR;
            CppTokens *repl = cpp_get_scratch(cpp);
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
                if(!!variadic != !!m->is_variadic){
                    cpp_error(cpp, tok.loc, "Duplicate function-like macro (%.*s) with different definitions", sv_p(name));
                    return cpp_error(cpp, m->def_loc, "... previously defined here");
                }
                if(names->count != m->nparams){
                    cpp_error(cpp, tok.loc, "Duplicate function-like macro (%.*s) with different definitions", sv_p(name));
                    return cpp_error(cpp, m->def_loc, "... previously defined here");
                }
                for(size_t i = 0; i < names->count; i++){
                    CppToken tname = names->data[i];
                    Atom pname = cpp_cmacro_params(m)[i];
                    if(AT_get_atom(cpp->at, tname.txt.text, tname.txt.length) != pname){
                        cpp_error(cpp, tok.loc, "Duplicate function-like macro (%.*s) with different definitions", sv_p(name));
                        return cpp_error(cpp, m->def_loc, "... previously defined here");
                    }
                }
                if(repl->count != m->nreplace){
                    return cpp_error(cpp, tok.loc, "%d: Duplicate function-like macro (%.*s) with different definitions %zu %zu", __LINE__, sv_p(name), repl->count, (size_t)m->nreplace);
                }
                for(size_t i = 0; i < repl->count; i++){
                    CppToken r = repl->data[i];
                    CppToken pre = cpp_cmacro_replacement(m)[i];
                    if(r.type == CPP_WHITESPACE && pre.type == CPP_WHITESPACE)
                        continue;
                    if(r.type != pre.type){
                        cpp_error(cpp, tok.loc, "%d: Duplicate function-like macro (%.*s) with different definitions", __LINE__, sv_p(name));
                        return cpp_error(cpp, m->def_loc, "... previously defined here");
                    }
                    if(!sv_equals(r.txt, pre.txt)){
                        cpp_error(cpp, tok.loc, "%d: Duplicate function-like macro (%.*s) with different definitions", __LINE__, sv_p(name));
                        return cpp_error(cpp, m->def_loc, "... previously defined here");
                    }
                }
                err = 0;
                goto finish_func_macro;
            }
            if(err) return err;
            m->def_loc = tok.loc;
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
            MARRAY_FOR_EACH(CppToken, t, *repl){
                if(t->type != CPP_IDENTIFIER)
                    continue;
                // Tag __VA_ARGS__ in variadic macros
                if(variadic && sv_equals(t->txt, SV("__VA_ARGS__"))){
                    t->param_idx = m->nparams + 1;
                    continue;
                }
                // Tag __VA_COUNT__ in variadic macros
                if(variadic && sv_equals(t->txt, SV("__VA_COUNT__"))){
                    t->param_idx = m->nparams + 2;
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
            CppTokens *repl = cpp_get_scratch(cpp);
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
                if(m->is_function_like){
                    cpp_error(cpp, tok.loc, "Redefinition of function-like macro (%.*s) as an object-like macro", sv_p(name));
                    return cpp_error(cpp, m->def_loc, "... previously defined here");
                }
                if(m->nreplace != repl->count){
                    cpp_error(cpp, tok.loc, "Duplicate object-like macro (%.*s) with different definitions, %zu != %zu", sv_p(name), repl->count, (size_t)m->nreplace);
                    return cpp_error(cpp, m->def_loc, "... previously defined here");
                }
                for(size_t i = 0; i < repl->count; i++){
                    CppToken r = repl->data[i];
                    CppToken pre = cpp_cmacro_replacement(m)[i];
                    if(r.type == CPP_WHITESPACE && pre.type == CPP_WHITESPACE)
                        continue;
                    if(r.type != pre.type){
                        cpp_error(cpp, tok.loc, "Duplicate object-like macro (%.*s) with different definitions (%zu different type)", sv_p(name), i);
                        return cpp_error(cpp, m->def_loc, "... previously defined here");
                    }
                    if(!sv_equals(r.txt, pre.txt)){
                        cpp_error(cpp, tok.loc, "Duplicate object-like macro (%.*s) with different definitions (%zu different content)", sv_p(name), i);
                        return cpp_error(cpp, m->def_loc, "... previously defined here");
                    }
                }
                // Duplicate macro definition, ok
                err = 0;
            }
            if(err) return err;
            ((CMacro*)AM_get(&cpp->macros, (Atom)AT_get_atom(cpp->at, name.text, name.length)))->def_loc = tok.loc;
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
        CppTokens *toks = cpp_get_scratch(cpp);
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
        s.true_taken = !!value;
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
        s->guard_macro = NULL;
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
        s->guard_macro = NULL;
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
        s->guard_macro = NULL;
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
        s->guard_macro = NULL;
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
        {
            CppPoundIf* s = &ma_tail(cpp->if_stack);
            if(s->guard_macro && cpp->frames.count){
                CppFrame* f = &ma_tail(cpp->frames);
                if(cpp_frame_only_whitespace_left(f)){
                    err = cpp_add_include_guard(cpp, f->file_id, (Atom)s->guard_macro);
                    if(err) return err;
                }
            }
        }
        cpp->if_stack.count--;
    }
    else if(sv_equals(tok.txt, SV("error")) || sv_equals(tok.txt, SV("warning"))){
        _Bool is_error = tok.txt.text[0] == 'e';
        SrcLoc directive_loc = tok.loc;
        // Collect the rest of the line as the message text
        MStringBuilder sb = {.allocator = allocator_from_arena(&cpp->synth_arena)};
        _Bool leading = 1;
        for(;;){
            err = cpp_next_raw_token(cpp, &tok);
            if(err){ msb_destroy(&sb); return err; }
            if(tok.type == CPP_EOF || tok.type == CPP_NEWLINE){
                err = cpp_push_tok(cpp, &cpp->pending, tok);
                if(err){ msb_destroy(&sb); return err; }
                break;
            }
            if(leading && tok.type == CPP_WHITESPACE)
                continue;
            leading = 0;
            msb_write_str(&sb, tok.txt.text, tok.txt.length);
        }
        StringView msg = msb_borrow_sv(&sb);
        if(is_error){
            int e = cpp_error(cpp, directive_loc, "#error %.*s", sv_p(msg));
            msb_destroy(&sb);
            return e;
        }
        cpp_warn(cpp, directive_loc, "#warning %.*s", sv_p(msg));
        msb_destroy(&sb);
    }
    else if(sv_equals(tok.txt, SV("pragma"))){
        SrcLoc pragma_loc = tok.loc;
        // Skip whitespace
        do {
            err = cpp_next_raw_token(cpp, &tok);
            if(err) return err;
        } while(tok.type == CPP_WHITESPACE);
        if(tok.type == CPP_NEWLINE || tok.type == CPP_EOF){
            // bare #pragma - just ignore
            return cpp_push_tok(cpp, &cpp->pending, tok);
        }
        if(tok.type != CPP_IDENTIFIER){
            // skip rest of line
            do {
                err = cpp_next_raw_token(cpp, &tok);
                if(err) return err;
            } while(tok.type != CPP_EOF && tok.type != CPP_NEWLINE);
            return cpp_push_tok(cpp, &cpp->pending, tok);
        }
        // Look up registered pragma
        Atom prag_name = AT_get_atom(cpp->at, tok.txt.text, tok.txt.length);
        CppPragma* prag = prag_name ? AM_get(&cpp->pragmas, prag_name) : NULL;
        if(!prag){
            // Unknown pragma - skip rest of line
            do {
                err = cpp_next_raw_token(cpp, &tok);
                if(err) return err;
            } while(tok.type != CPP_EOF && tok.type != CPP_NEWLINE);
            return cpp_push_tok(cpp, &cpp->pending, tok);
        }
        // Collect remaining tokens on the line for the pragma handler
        CppTokens* prag_toks = cpp_get_scratch(cpp);
        if(!prag_toks) return CPP_OOM_ERROR;
        for(;;){
            err = cpp_next_raw_token(cpp, &tok);
            if(err){ cpp_release_scratch(cpp, prag_toks); return err; }
            if(tok.type == CPP_EOF || tok.type == CPP_NEWLINE){
                err = cpp_push_tok(cpp, &cpp->pending, tok);
                if(err){ cpp_release_scratch(cpp, prag_toks); return err; }
                break;
            }
            if(tok.type == CPP_WHITESPACE && !prag_toks->count)
                continue; // skip leading whitespace
            err = cpp_push_tok(cpp, prag_toks, tok);
            if(err){ cpp_release_scratch(cpp, prag_toks); return err; }
        }
        // Strip trailing whitespace
        while(prag_toks->count && ma_tail(*prag_toks).type == CPP_WHITESPACE)
            prag_toks->count--;
        err = prag->fn(prag->ctx, cpp, pragma_loc, prag_toks->data, prag_toks->count);
        cpp_release_scratch(cpp, prag_toks);
        if(err) return err;
    }
    else if(sv_equals(tok.txt, SV("include")) || sv_equals(tok.txt, SV("include_next"))){
        _Bool is_next = tok.txt.length > 7 && tok.txt.text[7] == '_';
        SrcLoc directive_loc = tok.loc;
        // Skip whitespace after directive name
        do {
            err = cpp_next_raw_token(cpp, &tok);
            if(err) return err;
        } while(tok.type == CPP_WHITESPACE);
        _Bool quote = 0;
        StringView header_name = {0};
        MStringBuilder header_sb = {.allocator = allocator_from_arena(&cpp->synth_arena)};
        if(tok.type == CPP_STRING){
            // "header.h" form - strip quotes
            quote = 1;
            header_name = (StringView){tok.txt.length - 2, tok.txt.text + 1};
            // Consume remaining tokens on the #include line
            for(;;){
                err = cpp_next_raw_token(cpp, &tok);
                if(err){ msb_destroy(&header_sb); return err; }
                if(tok.type == CPP_WHITESPACE) continue;
                if(tok.type == CPP_NEWLINE || tok.type == CPP_EOF){
                    err = cpp_push_tok(cpp, &cpp->pending, tok);
                    if(err){ msb_destroy(&header_sb); return err; }
                    break;
                }
                cpp_warn(cpp, tok.loc, "Trailing tokens after #include");
            }
        }
        else if(tok.type == CPP_PUNCTUATOR && tok.punct == '<'){
            // <header.h> form - collect raw tokens until >
            quote = 0;
            for(;;){
                err = cpp_next_raw_token(cpp, &tok);
                if(err){ msb_destroy(&header_sb); return err; }
                if(tok.type == CPP_EOF || tok.type == CPP_NEWLINE){
                    msb_destroy(&header_sb);
                    return cpp_error(cpp, directive_loc, "Unterminated < in #include");
                }
                if(tok.type == CPP_PUNCTUATOR && tok.punct == '>')
                    break;
                msb_write_str(&header_sb, tok.txt.text, tok.txt.length);
            }
            header_name = msb_borrow_sv(&header_sb);
            // Consume remaining tokens on the #include line
            for(;;){
                err = cpp_next_raw_token(cpp, &tok);
                if(err){ msb_destroy(&header_sb); return err; }
                if(tok.type == CPP_WHITESPACE) continue;
                if(tok.type == CPP_NEWLINE || tok.type == CPP_EOF){
                    err = cpp_push_tok(cpp, &cpp->pending, tok);
                    if(err){ msb_destroy(&header_sb); return err; }
                    break;
                }
                cpp_warn(cpp, tok.loc, "Trailing tokens after #include");
            }
        }
        else {
            // Macro-expanded include: collect remaining tokens, expand, then parse
            CppTokens* line_toks = cpp_get_scratch(cpp);
            if(!line_toks){ msb_destroy(&header_sb); return CPP_OOM_ERROR; }
            // Push back current token
            err = cpp_push_tok(cpp, line_toks, tok);
            if(err){ cpp_release_scratch(cpp, line_toks); msb_destroy(&header_sb); return err; }
            for(;;){
                err = cpp_next_raw_token(cpp, &tok);
                if(err){ cpp_release_scratch(cpp, line_toks); msb_destroy(&header_sb); return err; }
                if(tok.type == CPP_EOF || tok.type == CPP_NEWLINE)
                    break;
                err = cpp_push_tok(cpp, line_toks, tok);
                if(err){ cpp_release_scratch(cpp, line_toks); msb_destroy(&header_sb); return err; }
            }
            // Push back the newline/eof
            err = cpp_push_tok(cpp, &cpp->pending, tok);
            if(err){ cpp_release_scratch(cpp, line_toks); msb_destroy(&header_sb); return err; }
            // Macro-expand the collected tokens
            CppTokens* expanded = cpp_get_scratch(cpp);
            if(!expanded){ cpp_release_scratch(cpp, line_toks); msb_destroy(&header_sb); return CPP_OOM_ERROR; }
            err = cpp_expand_argument(cpp, line_toks->data, line_toks->count, expanded);
            cpp_release_scratch(cpp, line_toks);
            if(err){ cpp_release_scratch(cpp, expanded); msb_destroy(&header_sb); return err; }
            // Find the first non-whitespace expanded token
            size_t ei = 0;
            while(ei < expanded->count && expanded->data[ei].type == CPP_WHITESPACE) ei++;
            if(ei >= expanded->count){
                cpp_release_scratch(cpp, expanded);
                msb_destroy(&header_sb);
                return cpp_error(cpp, directive_loc, "Empty #include after macro expansion");
            }
            CppToken etok = expanded->data[ei];
            if(etok.type == CPP_STRING){
                quote = 1;
                header_name = (StringView){etok.txt.length - 2, etok.txt.text + 1};
            }
            else if(etok.type == CPP_PUNCTUATOR && etok.punct == '<'){
                quote = 0;
                for(ei++; ei < expanded->count; ei++){
                    CppToken t = expanded->data[ei];
                    if(t.type == CPP_PUNCTUATOR && t.punct == '>')
                        break;
                    msb_write_str(&header_sb, t.txt.text, t.txt.length);
                }
                if(ei >= expanded->count){
                    cpp_release_scratch(cpp, expanded);
                    msb_destroy(&header_sb);
                    return cpp_error(cpp, directive_loc, "Unterminated < in macro-expanded #include");
                }
                header_name = msb_borrow_sv(&header_sb);
            }
            else {
                cpp_release_scratch(cpp, expanded);
                msb_destroy(&header_sb);
                return cpp_error(cpp, directive_loc, "Expected header name in #include after macro expansion");
            }
            cpp_release_scratch(cpp, expanded);
        }
        if(!header_name.length){
            msb_destroy(&header_sb);
            return cpp_error(cpp, directive_loc, "Empty header name in #include");
        }
        // Find the file
        IncludePosition inc_pos = {0};
        err = cpp_find_include(cpp, quote, is_next, header_name, &inc_pos);
        if(err){
            int e = cpp_error(cpp, directive_loc, "'%.*s' file not found", (int)header_name.length, header_name.text);
            msb_destroy(&header_sb);
            return e;
        }
        // cpp_find_include left the resolved path in fc->path_builder.
        // Read the file (may be cached) and push a new frame.
        StringView file_txt;
        err = fc_read_file(cpp->fc, &file_txt);
        if(err){
            msb_destroy(&header_sb);
            return cpp_error(cpp, directive_loc, "Could not read '%.*s'", (int)header_name.length, header_name.text);
        }
        // Find the file_id by matching the data pointer returned by fc_read_file
        uint32_t file_id = 0;
        for(size_t i = 0; i < cpp->fc->map.count; i++){
            if(cpp->fc->map.data[i].data_cached && cpp->fc->map.data[i].data.buff == file_txt.text){
                file_id = (uint32_t)i;
                break;
            }
        }
        // Check pragma once - skip if already included with #pragma once
        if(cpp_is_pragma_once(cpp, file_id)){
            msb_destroy(&header_sb);
            return 0;
        }
        // Check include guard - skip if guard macro is still defined
        {
            Atom guard = cpp_get_include_guard(cpp, file_id);
            if(guard && AM_get(&cpp->macros, guard)){
                // cpp_info(cpp, (SrcLoc){0}, "Skipping include: \"%s\"", guard->data);
                msb_destroy(&header_sb);
                return 0;
            }
        }
        CppFrame frame = {
            .file_id = file_id,
            .txt = file_txt,
            .line = 1,
            .column = 1,
            .include_position = inc_pos,
        };
        err = ma_push(CppFrame)(&cpp->frames, cpp->allocator, frame);
        msb_destroy(&header_sb);
        if(err) return CPP_OOM_ERROR;
        // Scan for #ifndef IDENTIFIER
        {
            CppTokens* scratch = cpp_get_scratch(cpp);
            if(!scratch) return CPP_OOM_ERROR;
            do {
                err = cpp_next_raw_token(cpp, &tok);
                if(err) goto scan_finally;
                err = cpp_push_tok(cpp, scratch, tok);
                if(err) goto scan_finally;
            }while(tok.type == CPP_WHITESPACE || tok.type == CPP_NEWLINE);
            if(tok.type == CPP_PUNCTUATOR && tok.punct == '#'){
                do {
                    err = cpp_next_raw_token(cpp, &tok);
                    if(err) goto scan_finally;
                    err = cpp_push_tok(cpp, scratch, tok);
                    if(err) goto scan_finally;
                }while(tok.type == CPP_WHITESPACE);
                if(tok.type == CPP_IDENTIFIER && sv_equals(tok.txt, SV("ifndef"))){
                    do {
                        err = cpp_next_raw_token(cpp, &tok);
                        if(err) goto scan_finally;
                        err = cpp_push_tok(cpp, scratch, tok);
                        if(err) goto scan_finally;
                    }while(tok.type == CPP_WHITESPACE);
                    if(tok.type == CPP_IDENTIFIER){
                        if(ma_tail(cpp->frames).file_id != frame.file_id)
                            goto scan_finally;
                        cpp_release_scratch(cpp, scratch);
                        Atom guard = AT_atomize(cpp->at, tok.txt.text, tok.txt.length);
                        if(!guard) return CPP_OOM_ERROR;
                        CppPoundIf s = {
                            .start = tok.loc,
                            .guard_macro = guard,
                        };
                        s.true_taken = !cpp_isdef(cpp, tok.txt);
                        s.is_active = s.true_taken;
                        err = cpp_push_if(cpp, s);
                        if(err) return CPP_OOM_ERROR;
                        do {
                            err = cpp_next_raw_token(cpp, &tok);
                            if(err) return err;
                        }
                        while(tok.type != CPP_EOF && tok.type != CPP_NEWLINE);
                        // push it back so dispatch loop sees newline
                        return cpp_push_tok(cpp, &cpp->pending, tok);
                    }
                }
            }
            while(scratch->count){
                err = cpp_push_tok(cpp, &cpp->pending, ma_pop_(*scratch));
                if(err) goto scan_finally;
            }
            scan_finally:;
            if(scratch) cpp_release_scratch(cpp, scratch);
            return err;
        }
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
    CppToken tok;
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
        {
            CppPoundIf* s = &ma_tail(cpp->if_stack);
            if(s->guard_macro && cpp->frames.count){
                CppFrame* f = &ma_tail(cpp->frames);
                if(cpp_frame_only_whitespace_left(f)){
                    err = cpp_add_include_guard(cpp, f->file_id, (Atom)s->guard_macro);
                    if(err) return err;
                }
            }
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
        s->guard_macro = NULL;
        if(s->seen_else)
            return cpp_error(cpp, tok.loc, "#elif after #else");
        CppTokens *toks = cpp_get_scratch(cpp);
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
        s->guard_macro = NULL;
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
        s->guard_macro = NULL;
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
        s->guard_macro = NULL;
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
cpp_expand_obj_macro(CPreprocessor *cpp, CMacro *macro, SrcLoc expansion_loc, CppTokens *dst){
    if(macro->is_builtin){
        CppObjMacroFn* fn = (CppObjMacroFn*)macro->data[0];
        void* ctx = (void*) macro->data[1];
        if(!fn) return CPP_UNREACHABLE_ERROR;
        return fn(ctx, cpp, expansion_loc, dst);
    }
    macro->is_disabled = 1;
    CppToken reenable = {.type = CPP_REENABLE, .data1 = macro};
    int err = cpp_push_tok(cpp, dst, reenable);
    if(err) return err;

    SrcLocExp* parent = cpp_srcloc_to_exp(cpp, expansion_loc);
    if(!parent) return CPP_OOM_ERROR;

    CppToken* repl = cpp_cmacro_replacement(macro);

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
        CppTokens empty_args = {0};
        Marray(size_t) empty_seps = {0};
        CppTokens *result = cpp_get_scratch(cpp);
        if(!result) return CPP_OOM_ERROR;
        err = cpp_substitute_and_paste(cpp, repl, macro->nreplace, macro, &empty_args, &empty_seps, NULL, result, 0, parent);
        if(err) goto finally_obj;
        for(size_t i = result->count; i-- > 0;){
            CppToken tok = result->data[i];
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
        CppToken tok = repl[i];
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
cpp_get_argument(const CppTokens *args, const Marray(size_t) *arg_seps, size_t arg_idx, CppToken*_Nullable*_Nonnull out_start, size_t *out_count){
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
cpp_get_va_args(const CppTokens *args, const Marray(size_t) *arg_seps, size_t nparams, CppToken*_Nullable*_Nonnull out_start, size_t *out_count){
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
cpp_va_args_nonempty(CPreprocessor *cpp, const CMacro *macro, const CppTokens *args, const Marray(size_t) *arg_seps, CppTokens *_Nullable*_Null_unspecified expanded_args){
    CppToken* start;
    size_t count;
    cpp_get_va_args(args, arg_seps, macro->nparams, &start, &count);
    if(!count) return 0;

    size_t va_idx = macro->nparams; // VA_ARGS slot in expanded_args
    if(!expanded_args[va_idx]){
        CppTokens *ea = cpp_get_scratch(cpp);
        if(!ea) return 0; // conservative: treat as empty on OOM
        int err = cpp_expand_argument(cpp, start, count, ea);
        if(err){
            cpp_release_scratch(cpp, ea);
            return 0;
        }
        expanded_args[va_idx] = ea;
    }
    CppTokens *expanded = expanded_args[va_idx];
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
CppToken
cpp_stringify_argument(CPreprocessor *cpp, CppToken*_Nullable toks, size_t count, SrcLoc loc){
    MStringBuilder sb = {.allocator = allocator_from_arena(&cpp->synth_arena)};
    msb_write_char(&sb, '"');
    for(size_t i = 0; i < count; i++){
        CppToken t = toks[i];
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
    return (CppToken){
        .type = CPP_STRING,
        .txt = msb_detach_sv(&sb),
        .loc = loc
    };
}

// Forward declaration for tokenizing from a frame directly
static int cpp_tokenize_from_frame(CPreprocessor *cpp, CppFrame *f, CppToken *tok);

// Helper: Paste two tokens (C23 6.10.4.3)
static
int
cpp_paste_tokens(CPreprocessor *cpp, CppToken left, CppToken right, CppToken *result, SrcLoc loc, SrcLocExp* expansion_parent){
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
    CppFrame temp_frame = {
        .txt = pasted,
        .cursor = 0,
        .line = loc.line,
        .column = loc.column,
        .file_id = loc.file_id
    };

    CppToken tok;
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
cpp_expand_argument(CPreprocessor *cpp, const CppToken*_Null_unspecified toks, size_t count, CppTokens *out){
    if(!count) return 0;
    int err = 0;
    CppToken tok;
    CppTokens* pending = cpp_get_scratch(cpp);
    CppTokens* args = NULL;
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
        CppToken next = {.type = CPP_EOF};
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
                next = (CppToken){.type=CPP_EOF};
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
                next = (CppToken){.type=CPP_EOF};
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
cpp_get_param_arg(const CMacro *macro, const CppTokens *args, const Marray(size_t) *arg_seps, size_t pidx, CppToken*_Nullable*_Nonnull out_start, size_t *out_count){
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
cpp_parse_va_opt_content(CPreprocessor *cpp, const CppToken *repl, size_t nreplace, size_t after_va_opt, SrcLoc loc, size_t *out_content_start, size_t *out_close_paren, SrcLocExp* expansion_parent){
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

// Helper: Parse __VA_ARG__(expr), evaluate the index, and retrieve the raw
// variadic argument tokens. `after` is the position after the __VA_ARG__ ident.
// On success, *out_start/*out_count point to the raw arg tokens and
// *out_cparen is the index of the closing ')'.
static
int
cpp_resolve_va_arg(
    CPreprocessor *cpp,
    const CppToken *repl, size_t nreplace, size_t after,
    const CMacro *macro,
    const CppTokens *args, const Marray(size_t) *arg_seps,
    CppTokens *_Nullable*_Null_unspecified expanded_args,
    SrcLoc loc, SrcLocExp *expansion_parent,
    CppToken *_Nullable *_Nonnull out_start, size_t *out_count, size_t *out_cparen)
{
    size_t k = after;
    while(k < nreplace && repl[k].type == CPP_WHITESPACE) k++;
    if(k >= nreplace || repl[k].type != CPP_PUNCTUATOR || repl[k].punct != '('){
        SrcLoc eloc = cpp_chain_loc(cpp, loc, expansion_parent);
        return cpp_error(cpp, eloc, "__VA_ARG__ must be followed by (index)");
    }
    k++; // skip '('
    size_t expr_start = k;
    int paren = 1;
    while(k < nreplace){
        if(repl[k].type == CPP_PUNCTUATOR){
            if(repl[k].punct == '(') paren++;
            else if(repl[k].punct == ')'){
                paren--;
                if(paren == 0) break;
            }
        }
        k++;
    }
    if(paren != 0){
        SrcLoc eloc = cpp_chain_loc(cpp, loc, expansion_parent);
        return cpp_error(cpp, eloc, "unterminated __VA_ARG__");
    }
    size_t cparen = k;
    size_t expr_count = cparen - expr_start;

    // Substitute parameters in the expression, then evaluate
    CppTokens *expr_subst = cpp_get_scratch(cpp);
    if(!expr_subst) return CPP_OOM_ERROR;
    int err = cpp_substitute_and_paste(cpp, repl + expr_start, expr_count, macro, args, arg_seps, expanded_args, expr_subst, 0, expansion_parent);
    if(err){ cpp_release_scratch(cpp, expr_subst); return err; }

    int64_t index;
    err = cpp_eval_tokens(cpp, expr_subst->data, expr_subst->count, &index);
    cpp_release_scratch(cpp, expr_subst);
    if(err) return err;

    size_t nargs_total = args->count ? arg_seps->count + 1 : 0;
    size_t va_count = nargs_total > macro->nparams ? nargs_total - macro->nparams : 0;
    if(index < 0 || (size_t)index >= va_count){
        SrcLoc eloc = cpp_chain_loc(cpp, loc, expansion_parent);
        return cpp_error(cpp, eloc, "__VA_ARG__ index out of range");
    }

    cpp_get_argument(args, arg_seps, macro->nparams + (size_t)index, out_start, out_count);
    *out_cparen = cparen;
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
    const CppToken *repl, size_t nreplace,
    const CMacro *macro,
    const CppTokens *args,
    const Marray(size_t) *arg_seps,
    CppTokens *_Nullable*_Null_unspecified expanded_args,
    CppTokens *out,
    _Bool raw_only,
    SrcLocExp* expansion_parent)
{
    int err;
    for(size_t i = 0; i < nreplace; i++){
        CppToken t = repl[i];

        // # (stringify) — only in function-like macros
        if(t.type == CPP_PUNCTUATOR && t.punct == '#' && macro->is_function_like){
            size_t j = i + 1;
            while(j < nreplace && repl[j].type == CPP_WHITESPACE) j++;

            // # __VA_OPT__(content)
            if(j < nreplace && repl[j].type == CPP_IDENTIFIER && sv_equals(repl[j].txt, SV("__VA_OPT__"))){
                size_t cstart, cparen;
                err = cpp_parse_va_opt_content(cpp, repl, nreplace, j+1, repl[j].loc, &cstart, &cparen, expansion_parent);
                if(err) return err;
                CppToken stringified;
                if(cpp_va_args_nonempty(cpp, macro, args, arg_seps, expanded_args)){
                    CppTokens *temp = cpp_get_scratch(cpp);
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
                    stringified = (CppToken){.type = CPP_STRING, .txt = SV("\"\""), .loc = t.loc};
                }
                err = cpp_push_tok(cpp, out, stringified);
                if(err) return err;
                i = cparen;
                continue;
            }

            // # __VA_ARG__(expr)
            if(j < nreplace && macro->is_variadic && repl[j].type == CPP_IDENTIFIER && sv_equals(repl[j].txt, SV("__VA_ARG__"))){
                CppToken *arg_start; size_t arg_count; size_t cparen;
                err = cpp_resolve_va_arg(cpp, repl, nreplace, j+1, macro, args, arg_seps, expanded_args, repl[j].loc, expansion_parent, &arg_start, &arg_count, &cparen);
                if(err) return err;
                CppToken stringified = cpp_stringify_argument(cpp, arg_start, arg_count, t.loc);
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
            // # __VA_COUNT__
            if(pidx == (uint64_t)macro->nparams + 1 && macro->is_variadic){
                size_t nargs = args->count ? arg_seps->count + 1 : 0;
                size_t va_count = nargs > macro->nparams ? nargs - macro->nparams : 0;
                Atom a = cpp_atomizef(cpp, "\"%zu\"", va_count);
                if(!a) return CPP_OOM_ERROR;
                CppToken stringified = {.type = CPP_STRING, .txt = {a->length, a->data}, .loc = t.loc};
                err = cpp_push_tok(cpp, out, stringified);
                if(err) return err;
                i = j;
                continue;
            }
            CppToken *arg_start; size_t arg_count;
            cpp_get_param_arg(macro, args, arg_seps, pidx, &arg_start, &arg_count);
            CppToken stringified = cpp_stringify_argument(cpp, arg_start, arg_count, t.loc);
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
            CppToken left = ma_tail(*out);
            out->count--;

            size_t j = i + 1;
            while(j < nreplace && repl[j].type == CPP_WHITESPACE) j++;
            // Similarly, if ## has no right operand, delete it and keep left.
            if(j >= nreplace){
                err = cpp_push_tok(cpp, out, left);
                if(err) return err;
                continue;
            }

            CppToken right;
            size_t skip_to = j;

            if(repl[j].param_idx > 0){
                size_t pidx = repl[j].param_idx - 1;
                // ## __VA_COUNT__
                if(pidx == (uint64_t)macro->nparams + 1 && macro->is_variadic){
                    size_t nargs = args->count ? arg_seps->count + 1 : 0;
                    size_t va_count = nargs > macro->nparams ? nargs - macro->nparams : 0;
                    Atom a = cpp_atomizef(cpp, "%zu", va_count);
                    if(!a) return CPP_OOM_ERROR;
                    right = (CppToken){.type = CPP_NUMBER, .txt = {a->length, a->data}, .loc = repl[j].loc};
                    CppToken pr;
                    err = cpp_paste_tokens(cpp, left, right, &pr, t.loc, expansion_parent);
                    if(err) return err;
                    err = cpp_push_tok(cpp, out, pr);
                    if(err) return err;
                    i = skip_to;
                    continue;
                }
                // Right operand is a param — use raw tokens
                CppToken *arg_start; size_t arg_count;
                cpp_get_param_arg(macro, args, arg_seps, pidx, &arg_start, &arg_count);
                // GNU extension: , ## __VA_ARGS__
                // When left is comma and right is __VA_ARGS__:
                //   empty: suppress both comma and args
                //   non-empty: emit comma then args (no pasting)
                if(pidx == (uint64_t)macro->nparams && macro->is_variadic && left.type == CPP_PUNCTUATOR && left.punct == ','){
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
                    ? (CppToken){.type = CPP_PLACEMARKER, .loc = repl[j].loc}
                    : arg_start[0];
                CppToken pr;
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
                    CppTokens *temp = cpp_get_scratch(cpp);
                    if(!temp) return CPP_OOM_ERROR;
                    err = cpp_substitute_and_paste(cpp, repl+cstart, cparen-cstart, macro, args, arg_seps, expanded_args, temp, raw_only, expansion_parent);
                    if(err) goto finally_paste_va_opt;
                    right = (temp->count == 0)
                        ? (CppToken){.type = CPP_PLACEMARKER, .loc = repl[j].loc}
                        : temp->data[0];
                    CppToken pr;
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
                    right = (CppToken){.type = CPP_PLACEMARKER, .loc = repl[j].loc};
                    CppToken pr;
                    err = cpp_paste_tokens(cpp, left, right, &pr, t.loc, expansion_parent);
                    if(err) return err;
                    err = cpp_push_tok(cpp, out, pr);
                    if(err) return err;
                }
                i = cparen;
                continue;
            }

            // ## __VA_ARG__(expr)
            if(macro->is_variadic && repl[j].type == CPP_IDENTIFIER && sv_equals(repl[j].txt, SV("__VA_ARG__"))){
                CppToken *arg_start; size_t arg_count; size_t cparen;
                err = cpp_resolve_va_arg(cpp, repl, nreplace, j+1, macro, args, arg_seps, expanded_args, repl[j].loc, expansion_parent, &arg_start, &arg_count, &cparen);
                if(err) return err;
                right = (arg_count == 0)
                    ? (CppToken){.type = CPP_PLACEMARKER, .loc = repl[j].loc}
                    : arg_start[0];
                CppToken pr;
                err = cpp_paste_tokens(cpp, left, right, &pr, t.loc, expansion_parent);
                if(err) return err;
                err = cpp_push_tok(cpp, out, pr);
                if(err) return err;
                for(size_t m = 1; m < arg_count; m++){
                    err = cpp_push_tok(cpp, out, arg_start[m]);
                    if(err) return err;
                }
                i = cparen;
                continue;
            }

            // Regular token as right operand
            right = repl[j];
            CppToken pr;
            err = cpp_paste_tokens(cpp, left, right, &pr, t.loc, expansion_parent);
            if(err) return err;
            err = cpp_push_tok(cpp, out, pr);
            if(err) return err;
            i = skip_to;
            continue;
        }

        // __VA_ARG__(expr)
        if(macro->is_variadic && t.type == CPP_IDENTIFIER && sv_equals(t.txt, SV("__VA_ARG__"))){
            CppToken *arg_start; size_t arg_count; size_t cparen;
            err = cpp_resolve_va_arg(cpp, repl, nreplace, i+1, macro, args, arg_seps, expanded_args, t.loc, expansion_parent, &arg_start, &arg_count, &cparen);
            if(err) return err;
            // Check if right-adjacent to ##
            _Bool use_raw = raw_only;
            if(!use_raw){
                for(size_t j = cparen + 1; j < nreplace; j++){
                    if(repl[j].type == CPP_WHITESPACE) continue;
                    if(repl[j].type == CPP_PUNCTUATOR && repl[j].punct == '##') use_raw = 1;
                    break;
                }
            }
            if(arg_count == 0){
                CppToken pm = {.type = CPP_PLACEMARKER, .loc = t.loc};
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
                CppTokens *ea = cpp_get_scratch(cpp);
                if(!ea) return CPP_OOM_ERROR;
                err = cpp_expand_argument(cpp, arg_start, arg_count, ea);
                if(err){ cpp_release_scratch(cpp, ea); return err; }
                if(ea->count == 0){
                    CppToken pm = {.type = CPP_PLACEMARKER, .loc = t.loc};
                    err = cpp_push_tok(cpp, out, pm);
                }
                else {
                    for(size_t j = 0; j < ea->count; j++){
                        err = cpp_push_tok(cpp, out, ea->data[j]);
                        if(err) break;
                    }
                }
                cpp_release_scratch(cpp, ea);
                if(err) return err;
            }
            i = cparen;
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
            // __VA_COUNT__: emit the number of variadic arguments
            if(pidx == (uint64_t)macro->nparams + 1 && macro->is_variadic){
                size_t nargs = args->count ? arg_seps->count + 1 : 0;
                size_t va_count = nargs > macro->nparams ? nargs - macro->nparams : 0;
                Atom a = cpp_atomizef(cpp, "%zu", va_count);
                if(!a) return CPP_OOM_ERROR;
                CppToken tok = {.type = CPP_NUMBER, .txt = {a->length, a->data}, .loc = t.loc};
                err = cpp_push_tok(cpp, out, tok);
                if(err) return err;
                continue;
            }
            CppToken *arg_start; size_t arg_count;
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
                CppToken pm = {.type = CPP_PLACEMARKER, .loc = t.loc};
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
                    CppTokens *ea = cpp_get_scratch(cpp);
                    if(!ea) return CPP_OOM_ERROR;
                    err = cpp_expand_argument(cpp, arg_start, arg_count, ea);
                    if(err) return err;
                    expanded_args[pidx] = ea;
                }
                CppTokens *expanded = expanded_args[pidx];
                if(expanded->count == 0){
                    CppToken pm = {.type = CPP_PLACEMARKER, .loc = t.loc};
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
cpp_expand_func_macro(CPreprocessor *cpp, CMacro *macro, SrcLoc expansion_loc, const CppTokens *args, const Marray(size_t) *arg_seps, CppTokens *dst){
    if(macro->is_builtin){
        CppFuncMacroFn *fn = (CppFuncMacroFn*)macro->data[0];
        void* ctx = (void*)macro->data[1];
        if(!fn) return CPP_UNREACHABLE_ERROR;
        if(macro->no_expand_args){
            CppTokens *result = cpp_get_scratch(cpp);
            if(!result) return CPP_OOM_ERROR;
            int err = fn(ctx, cpp, expansion_loc, result, args, arg_seps);
            if(err){ cpp_release_scratch(cpp, result); return err; }
            SrcLocExp* parent = cpp_srcloc_to_exp(cpp, expansion_loc);
            if(!parent){ cpp_release_scratch(cpp, result); return CPP_OOM_ERROR; }
            macro->is_disabled = 1;
            CppToken reenable = {.type = CPP_REENABLE, .data1 = macro};
            err = cpp_push_tok(cpp, dst, reenable);
            if(err){ cpp_release_scratch(cpp, result); return err; }
            for(size_t i = result->count; i-- > 0;){
                CppToken t = result->data[i];
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
                if(err){ cpp_release_scratch(cpp, result); return err; }
            }
            cpp_release_scratch(cpp, result);
            return 0;
        }
        else{
            int err;
            CppTokens *exp_args = cpp_get_scratch(cpp);
            CppTokens *result = cpp_get_scratch(cpp);
            Marray(size_t) *idxes = cpp_get_scratch_idxes(cpp);
            if(!result || !exp_args || !idxes){
                err = CPP_OOM_ERROR;
                goto func_finally;
            }
            SrcLocExp* parent = cpp_srcloc_to_exp(cpp, expansion_loc);
            if(!parent){ err = CPP_OOM_ERROR; goto func_finally; }
            size_t total_params = macro->nparams + (macro->is_variadic ? 1 : 0);
            for(size_t i = 0; i < total_params; i++){
                CppToken* start;
                size_t count;
                cpp_get_param_arg(macro, args, arg_seps, i, &start, &count);
                err = cpp_expand_argument(cpp, start, count, exp_args);
                if(err) goto func_finally;
                if(i < total_params - 1){
                    err = ma_push(size_t)(idxes, cpp->allocator, exp_args->count);
                    if(err) goto func_finally;
                    err = cpp_push_tok(cpp, exp_args, (CppToken){.type = CPP_PUNCTUATOR, .punct = ','});
                    if(err) goto func_finally;
                }
            }
            err = fn(ctx, cpp, expansion_loc, result, exp_args, idxes);
            if(err) goto func_finally;
            macro->is_disabled = 1;
            CppToken reenable = {.type = CPP_REENABLE, .data1 = macro};
            err = cpp_push_tok(cpp, dst, reenable);
            if(err) goto func_finally;

            for(size_t i = result->count; i-- > 0;){
                CppToken t = result->data[i];
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
    CppTokens **expanded_args = NULL;
    CppTokens *result = NULL;

    if(total_params){
        expanded_args = Allocator_zalloc(cpp->allocator, sizeof(CppTokens*) * total_params);
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
    CppToken reenable = {.type = CPP_REENABLE, .data1 = macro};
    err = cpp_push_tok(cpp, dst, reenable);
    if(err) goto finally;

    for(size_t i = result->count; i-- > 0;){
        CppToken t = result->data[i];
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
        Allocator_free(cpp->allocator, expanded_args, sizeof(CppTokens*) * total_params);
    return err;
}

static
CppTokens*_Nullable
cpp_get_scratch(CPreprocessor *cpp){
    CppTokens *scratch = fl_pop(&cpp->scratch_list);
    if(!scratch) scratch = Allocator_zalloc(cpp->allocator, sizeof *scratch);
    if(!scratch) return NULL;
    scratch->count = 0;
    return scratch;
}
static
void
cpp_release_scratch(CPreprocessor *cpp, CppTokens *scratch){
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
                     cpp_builtin_time,
                     cpp_builtin_rand,
                     cpp_builtin_base_file,
                     cpp_builtin_timestamp
                     ;
static CppFuncMacroFn cpp_builtin_eval,
                      cpp_builtin_mixin,
                      cpp_builtin_env,
                      cpp_builtin_if,
                      cpp_builtin_ident,
                      cpp_builtin_fmt,
                      cpp_builtin_print,
                      cpp_builtin_set,
                      cpp_builtin_get,
                      cpp_builtin_append,
                      cpp_builtin_for,
                      cpp_builtin_map,
                      cpp_builtin_let,
                      cpp_builtin__Pragma,
                      cpp_builtin_where
                      ;
static CppPragmaFn cpp_builtin_pragma_once,
                   cpp_builtin_pragma_message,
                   cpp_builtin_pragma_include_path
                   ;

// Define a macro that expands to a type name from a CcBasicTypeKind.
// E.g. CCBT_unsigned_long -> "long unsigned int" (3 identifier tokens).
static
int
cpp_define_type_name_macro(CPreprocessor* cpp, StringView name, CcBasicTypeKind kind){
    CppToken toks[7];
    size_t ntoks = 0;
    switch(kind){
        case CCBT_signed_char:
            toks[ntoks++] = (CppToken){.type=CPP_IDENTIFIER, .txt=SV("signed")};
            toks[ntoks++] = (CppToken){.type=CPP_WHITESPACE, .txt=SV(" ")};
            toks[ntoks++] = (CppToken){.type=CPP_IDENTIFIER, .txt=SV("char")};
            break;
        case CCBT_unsigned_char:
            toks[ntoks++] = (CppToken){.type=CPP_IDENTIFIER, .txt=SV("unsigned")};
            toks[ntoks++] = (CppToken){.type=CPP_WHITESPACE, .txt=SV(" ")};
            toks[ntoks++] = (CppToken){.type=CPP_IDENTIFIER, .txt=SV("char")};
            break;
        case CCBT_short:
            toks[ntoks++] = (CppToken){.type=CPP_IDENTIFIER, .txt=SV("short")};
            break;
        case CCBT_unsigned_short:
            toks[ntoks++] = (CppToken){.type=CPP_IDENTIFIER, .txt=SV("unsigned")};
            toks[ntoks++] = (CppToken){.type=CPP_WHITESPACE, .txt=SV(" ")};
            toks[ntoks++] = (CppToken){.type=CPP_IDENTIFIER, .txt=SV("short")};
            break;
        case CCBT_int:
            toks[ntoks++] = (CppToken){.type=CPP_IDENTIFIER, .txt=SV("int")};
            break;
        case CCBT_unsigned:
            toks[ntoks++] = (CppToken){.type=CPP_IDENTIFIER, .txt=SV("unsigned")};
            toks[ntoks++] = (CppToken){.type=CPP_WHITESPACE, .txt=SV(" ")};
            toks[ntoks++] = (CppToken){.type=CPP_IDENTIFIER, .txt=SV("int")};
            break;
        case CCBT_long:
            toks[ntoks++] = (CppToken){.type=CPP_IDENTIFIER, .txt=SV("long")};
            toks[ntoks++] = (CppToken){.type=CPP_WHITESPACE, .txt=SV(" ")};
            toks[ntoks++] = (CppToken){.type=CPP_IDENTIFIER, .txt=SV("int")};
            break;
        case CCBT_unsigned_long:
            toks[ntoks++] = (CppToken){.type=CPP_IDENTIFIER, .txt=SV("long")};
            toks[ntoks++] = (CppToken){.type=CPP_WHITESPACE, .txt=SV(" ")};
            toks[ntoks++] = (CppToken){.type=CPP_IDENTIFIER, .txt=SV("unsigned")};
            toks[ntoks++] = (CppToken){.type=CPP_WHITESPACE, .txt=SV(" ")};
            toks[ntoks++] = (CppToken){.type=CPP_IDENTIFIER, .txt=SV("int")};
            break;
        case CCBT_long_long:
            toks[ntoks++] = (CppToken){.type=CPP_IDENTIFIER, .txt=SV("long")};
            toks[ntoks++] = (CppToken){.type=CPP_WHITESPACE, .txt=SV(" ")};
            toks[ntoks++] = (CppToken){.type=CPP_IDENTIFIER, .txt=SV("long")};
            toks[ntoks++] = (CppToken){.type=CPP_WHITESPACE, .txt=SV(" ")};
            toks[ntoks++] = (CppToken){.type=CPP_IDENTIFIER, .txt=SV("int")};
            break;
        case CCBT_unsigned_long_long:
            toks[ntoks++] = (CppToken){.type=CPP_IDENTIFIER, .txt=SV("long")};
            toks[ntoks++] = (CppToken){.type=CPP_WHITESPACE, .txt=SV(" ")};
            toks[ntoks++] = (CppToken){.type=CPP_IDENTIFIER, .txt=SV("long")};
            toks[ntoks++] = (CppToken){.type=CPP_WHITESPACE, .txt=SV(" ")};
            toks[ntoks++] = (CppToken){.type=CPP_IDENTIFIER, .txt=SV("unsigned")};
            toks[ntoks++] = (CppToken){.type=CPP_WHITESPACE, .txt=SV(" ")};
            toks[ntoks++] = (CppToken){.type=CPP_IDENTIFIER, .txt=SV("int")};
            break;
        default:
            return -1;
    }
    return cpp_define_obj_macro(cpp, name, toks, ntoks);
}

// Helper: return the unsigned counterpart of a basic type kind.
static
CcBasicTypeKind
ccbt_unsigned_of(CcBasicTypeKind kind){
    switch(kind){
        case CCBT_char: case CCBT_signed_char: case CCBT_unsigned_char: return CCBT_unsigned_char;
        case CCBT_short: case CCBT_unsigned_short: return CCBT_unsigned_short;
        case CCBT_int: case CCBT_unsigned: return CCBT_unsigned;
        case CCBT_long: case CCBT_unsigned_long: return CCBT_unsigned_long;
        case CCBT_long_long: case CCBT_unsigned_long_long: return CCBT_unsigned_long_long;
        default: return kind;
    }
}

// Helper: return the signed counterpart of a basic type kind.
static
CcBasicTypeKind
ccbt_signed_of(CcBasicTypeKind kind){
    switch(kind){
        case CCBT_char: case CCBT_signed_char: case CCBT_unsigned_char: return CCBT_signed_char;
        case CCBT_short: case CCBT_unsigned_short: return CCBT_short;
        case CCBT_int: case CCBT_unsigned: return CCBT_int;
        case CCBT_long: case CCBT_unsigned_long: return CCBT_long;
        case CCBT_long_long: case CCBT_unsigned_long_long: return CCBT_long_long;
        default: return kind;
    }
}

// Helper: return the integer literal suffix for a basic type kind.
// E.g. CCBT_long -> "L", CCBT_unsigned_long_long -> "ULL"
static
const char*
ccbt_literal_suffix(CcBasicTypeKind kind){
    switch(kind){
        case CCBT_unsigned: return "U";
        case CCBT_long: return "L";
        case CCBT_unsigned_long: return "UL";
        case CCBT_long_long: return "LL";
        case CCBT_unsigned_long_long: return "ULL";
        default: return "";
    }
}

// Helper: sizeof a basic type given the target config.
static
uint8_t
ccbt_sizeof(CcTargetConfig t, CcBasicTypeKind kind){
    switch(kind){
        case CCBT_bool: return 1;
        case CCBT_char: case CCBT_signed_char: case CCBT_unsigned_char: return 1;
        case CCBT_short: case CCBT_unsigned_short: return t.sizeof_short;
        case CCBT_int: case CCBT_unsigned: return t.sizeof_int;
        case CCBT_long: case CCBT_unsigned_long: return t.sizeof_long;
        case CCBT_long_long: case CCBT_unsigned_long_long: return t.sizeof_long_long;
        default: return 0;
    }
}

static
int
cpp_def_1(CPreprocessor* cpp, StringView name){
    return cpp_define_obj_macro(cpp, name, (CppToken[]){{.type=CPP_NUMBER, .txt=SV("1")}}, 1);
}
static
int
cpp_def_int(CPreprocessor* cpp, StringView name, int val){
    Atom a = cpp_atomizef(cpp, "%d", val);
    if(!a) return CPP_OOM_ERROR;
    return cpp_define_obj_macro(cpp, name, (CppToken[]){{.type=CPP_NUMBER, .txt={a->length, a->data}}}, 1);
}
static
int
cpp_def_num(CPreprocessor* cpp, StringView name, StringView num){
    return cpp_define_obj_macro(cpp, name, (CppToken[]){{.type=CPP_NUMBER, .txt=num}}, 1);
}

static
int
cpp_define_target_macros(CPreprocessor* cpp){
    int err;
    CcTargetConfig t = cpp->target;

    #define DEF1(name) do { \
        err = cpp_def_1(cpp, SV(name)); \
        if(err) return err; \
    } while(0)
    #define DEFNUM(name, val) do { \
        err = cpp_def_num(cpp, SV(name), (StringView){strlen(val), val}); \
        if(err) return err; \
    } while(0)
    #define DEFINT(name, val) do { \
        Atom _da = cpp_atomizef(cpp, "%d", (int)(val)); \
        if(!_da) return CPP_OOM_ERROR; \
        err = cpp_define_obj_macro(cpp, SV(name), (CppToken[]){{.type=CPP_NUMBER, .txt={_da->length, _da->data}}}, 1); \
        if(err) return err; \
    } while(0)
    #define DEFTYPE(name, kind) do { \
        err = cpp_define_type_name_macro(cpp, SV(name), kind); \
        if(err) return err; \
    } while(0)

    // __GNUC__ compatibility
    DEFINT("__GNUC__", 4);
    DEFINT("__GNUC_MINOR__", 2);
    DEFINT("__GNUC_PATCHLEVEL__", 1);

    // __SIZEOF_*__
    DEFINT("__SIZEOF_SHORT__",       t.sizeof_short);
    DEFINT("__SIZEOF_INT__",         t.sizeof_int);
    DEFINT("__SIZEOF_LONG__",        t.sizeof_long);
    DEFINT("__SIZEOF_LONG_LONG__",   t.sizeof_long_long);
    DEFINT("__SIZEOF_FLOAT__",       t.sizeof_float);
    DEFINT("__SIZEOF_DOUBLE__",      t.sizeof_double);
    DEFINT("__SIZEOF_LONG_DOUBLE__", t.sizeof_long_double);
    DEFINT("__SIZEOF_POINTER__",     t.sizeof_pointer);
    DEFINT("__SIZEOF_SIZE_T__",      ccbt_sizeof(t, t.size_type));
    DEFINT("__SIZEOF_PTRDIFF_T__",   ccbt_sizeof(t, t.ptrdiff_type));
    DEFINT("__SIZEOF_WCHAR_T__",     ccbt_sizeof(t, t.wchar_type));
    DEFINT("__SIZEOF_WINT_T__",      ccbt_sizeof(t, t.wint_type));

    DEFINT("__CHAR_BIT__", 8);

    if(t.sizeof_pointer == 8){
        DEF1("__LP64__");
        DEF1("_LP64");
    }

    // Signedness
    if(!t.char_is_signed) {
        DEF1("__CHAR_UNSIGNED__");
        DEF1("_CHAR_UNSIGNED");
    }

    {
        CcBasicTypeKind wk = t.wchar_type;
        if(wk == CCBT_unsigned || wk == CCBT_unsigned_short || wk == CCBT_unsigned_long || wk == CCBT_unsigned_long_long)
            DEF1("__WCHAR_UNSIGNED__");
    }

    // __USER_LABEL_PREFIX__
    switch(t.target){
        case CC_TARGET_X86_64_MACOS:
        case CC_TARGET_AARCH64_MACOS:
            err = cpp_define_obj_macro(cpp, SV("__USER_LABEL_PREFIX__"),
                (CppToken[]){{.type=CPP_IDENTIFIER, .txt=SV("_")}}, 1);
            if(err) return err;
            break;
        default:
            // Empty replacement list
            err = cpp_define_obj_macro(cpp, SV("__USER_LABEL_PREFIX__"), NULL, 0);
            if(err) return err;
            break;
    }

    // __*_TYPE__ macros
    DEFTYPE("__SIZE_TYPE__",       t.size_type);
    DEFTYPE("__PTRDIFF_TYPE__",    t.ptrdiff_type);
    DEFTYPE("__WCHAR_TYPE__",      t.wchar_type);
    DEFTYPE("__WINT_TYPE__",       t.wint_type);
    DEFTYPE("__INTMAX_TYPE__",     t.intmax_type);
    DEFTYPE("__UINTMAX_TYPE__",    ccbt_unsigned_of(t.intmax_type));
    DEFTYPE("__SIG_ATOMIC_TYPE__", CCBT_int);
    DEFTYPE("__INT8_TYPE__",       CCBT_signed_char);
    DEFTYPE("__INT16_TYPE__",      CCBT_short);
    DEFTYPE("__INT32_TYPE__",      CCBT_int);
    DEFTYPE("__INT64_TYPE__",      ccbt_signed_of(t.intmax_type));
    DEFTYPE("__UINT8_TYPE__",      CCBT_unsigned_char);
    DEFTYPE("__UINT16_TYPE__",     CCBT_unsigned_short);
    DEFTYPE("__UINT32_TYPE__",     CCBT_unsigned);
    DEFTYPE("__UINT64_TYPE__",     ccbt_unsigned_of(t.intmax_type));
    DEFTYPE("__INT_LEAST8_TYPE__",  CCBT_signed_char);
    DEFTYPE("__INT_LEAST16_TYPE__", CCBT_short);
    DEFTYPE("__INT_LEAST32_TYPE__", CCBT_int);
    DEFTYPE("__INT_LEAST64_TYPE__", ccbt_signed_of(t.intmax_type));
    DEFTYPE("__UINT_LEAST8_TYPE__",  CCBT_unsigned_char);
    DEFTYPE("__UINT_LEAST16_TYPE__", CCBT_unsigned_short);
    DEFTYPE("__UINT_LEAST32_TYPE__", CCBT_unsigned);
    DEFTYPE("__UINT_LEAST64_TYPE__", ccbt_unsigned_of(t.intmax_type));
    DEFTYPE("__INTPTR_TYPE__",     t.intptr_type);
    DEFTYPE("__UINTPTR_TYPE__",    ccbt_unsigned_of(t.intptr_type));
    // Fast types: on Linux all fast >= 16 use long, on macOS/Windows use the natural size.
    switch(t.target){
        case CC_TARGET_X86_64_LINUX:
        case CC_TARGET_AARCH64_LINUX:
            DEFTYPE("__INT_FAST8_TYPE__",   CCBT_signed_char);
            DEFTYPE("__INT_FAST16_TYPE__",  CCBT_long);
            DEFTYPE("__INT_FAST32_TYPE__",  CCBT_long);
            DEFTYPE("__INT_FAST64_TYPE__",  CCBT_long);
            DEFTYPE("__UINT_FAST8_TYPE__",  CCBT_unsigned_char);
            DEFTYPE("__UINT_FAST16_TYPE__", CCBT_unsigned_long);
            DEFTYPE("__UINT_FAST32_TYPE__", CCBT_unsigned_long);
            DEFTYPE("__UINT_FAST64_TYPE__", CCBT_unsigned_long);
            break;
        default:
            DEFTYPE("__INT_FAST8_TYPE__",   CCBT_signed_char);
            DEFTYPE("__INT_FAST16_TYPE__",  CCBT_short);
            DEFTYPE("__INT_FAST32_TYPE__",  CCBT_int);
            DEFTYPE("__INT_FAST64_TYPE__",  ccbt_signed_of(t.intmax_type));
            DEFTYPE("__UINT_FAST8_TYPE__",  CCBT_unsigned_char);
            DEFTYPE("__UINT_FAST16_TYPE__", CCBT_unsigned_short);
            DEFTYPE("__UINT_FAST32_TYPE__", CCBT_unsigned);
            DEFTYPE("__UINT_FAST64_TYPE__", ccbt_unsigned_of(t.intmax_type));
            break;
    }

    // __*_MAX__ macros
    // Helper: signed max for N-byte type = 2^(N*8-1) - 1
    // We hardcode the well-known values to avoid 128-bit arithmetic.
    {
        const char* schar_max  = "127";
        const char* shrt_max   = "32767";
        const char* int_max    = "2147483647";
        const char* l_suffix   = ccbt_literal_suffix(CCBT_long);
        const char* ll_suffix  = ccbt_literal_suffix(CCBT_long_long);
        const char* u_suffix   = "U";
        const char* ul_suffix  = ccbt_literal_suffix(CCBT_unsigned_long);
        const char* ull_suffix = ccbt_literal_suffix(CCBT_unsigned_long_long);

        Atom long_max, llong_max;
        if(t.sizeof_long == 8)
            long_max = cpp_atomizef(cpp, "9223372036854775807%s", l_suffix);
        else
            long_max = cpp_atomizef(cpp, "2147483647%s", l_suffix);
        if(!long_max) return CPP_OOM_ERROR;
        llong_max = cpp_atomizef(cpp, "9223372036854775807%s", ll_suffix);
        if(!llong_max) return CPP_OOM_ERROR;

        // Signed MAX
        DEFNUM("__SCHAR_MAX__", schar_max);
        DEFNUM("__SHRT_MAX__",  shrt_max);
        DEFNUM("__INT_MAX__",   int_max);
        {
            Atom a = long_max;
            err = cpp_define_obj_macro(cpp, SV("__LONG_MAX__"), (CppToken[]){{.type=CPP_NUMBER, .txt={a->length, a->data}}}, 1);
            if(err) return err;
        }
        {
            Atom a = llong_max;
            err = cpp_define_obj_macro(cpp, SV("__LONG_LONG_MAX__"), (CppToken[]){{.type=CPP_NUMBER, .txt={a->length, a->data}}}, 1);
            if(err) return err;
        }

        // Fixed-width MAX
        DEFNUM("__INT8_MAX__",  schar_max);
        DEFNUM("__INT16_MAX__", shrt_max);
        DEFNUM("__INT32_MAX__", int_max);
        // INT64_MAX = same value as intmax max
        {
            Atom a;
            if(t.sizeof_long == 8)
                a = long_max;
            else
                a = llong_max;
            err = cpp_define_obj_macro(cpp, SV("__INT64_MAX__"),
                (CppToken[]){{.type=CPP_NUMBER, .txt={a->length, a->data}}}, 1);
            if(err) return err;
        }

        // Unsigned MAX
        #define DEFUMAX(name, val) do { \
            Atom a = cpp_atomizef(cpp, "%s", val); \
            if(!a) return CPP_OOM_ERROR; \
            err = cpp_define_obj_macro(cpp, SV(name), (CppToken[]){{.type=CPP_NUMBER, .txt={a->length, a->data}}}, 1); \
            if(err) return err; \
        } while(0)

        DEFUMAX("__UINT8_MAX__",  "255");
        DEFUMAX("__UINT16_MAX__", "65535");
        {
            Atom a = cpp_atomizef(cpp, "4294967295%s", u_suffix);
            if(!a) return CPP_OOM_ERROR;
            err = cpp_define_obj_macro(cpp, SV("__UINT32_MAX__"), (CppToken[]){{.type=CPP_NUMBER, .txt={a->length, a->data}}}, 1);
            if(err) return err;
        }
        {
            const char* suf = t.sizeof_long == 8 ? ul_suffix : ull_suffix;
            Atom a = cpp_atomizef(cpp, "18446744073709551615%s", suf);
            if(!a) return CPP_OOM_ERROR;
            err = cpp_define_obj_macro(cpp, SV("__UINT64_MAX__"), (CppToken[]){{.type=CPP_NUMBER, .txt={a->length, a->data}}}, 1);
            if(err) return err;
        }

        // LEAST MAX (same as fixed-width)
        DEFNUM("__INT_LEAST8_MAX__",  schar_max);
        DEFNUM("__INT_LEAST16_MAX__", shrt_max);
        DEFNUM("__INT_LEAST32_MAX__", int_max);
        {
            Atom a = t.sizeof_long == 8 ? long_max : llong_max;
            err = cpp_define_obj_macro(cpp, SV("__INT_LEAST64_MAX__"), (CppToken[]){{.type=CPP_NUMBER, .txt={a->length, a->data}}}, 1);
            if(err) return err;
        }
        DEFUMAX("__UINT_LEAST8_MAX__",  "255");
        DEFUMAX("__UINT_LEAST16_MAX__", "65535");
        {
            Atom a = cpp_atomizef(cpp, "4294967295%s", u_suffix);
            if(!a) return CPP_OOM_ERROR;
            err = cpp_define_obj_macro(cpp, SV("__UINT_LEAST32_MAX__"),
                (CppToken[]){{.type=CPP_NUMBER, .txt={a->length, a->data}}}, 1);
            if(err) return err;
        }
        {
            const char* suf = t.sizeof_long == 8 ? ul_suffix : ull_suffix;
            Atom a = cpp_atomizef(cpp, "18446744073709551615%s", suf);
            if(!a) return CPP_OOM_ERROR;
            err = cpp_define_obj_macro(cpp, SV("__UINT_LEAST64_MAX__"), (CppToken[]){{.type=CPP_NUMBER, .txt={a->length, a->data}}}, 1);
            if(err) return err;
        }

        // FAST MAX
        switch(t.target){
            case CC_TARGET_X86_64_LINUX:
            case CC_TARGET_AARCH64_LINUX:
                DEFNUM("__INT_FAST8_MAX__", schar_max);
                {
                    Atom a = long_max;
                    #define DEFFAST(name) \
                        err = cpp_define_obj_macro(cpp, SV(name), \
                            (CppToken[]){{.type=CPP_NUMBER, .txt={a->length, a->data}}}, 1); \
                        if(err) return err
                    DEFFAST("__INT_FAST16_MAX__");
                    DEFFAST("__INT_FAST32_MAX__");
                    DEFFAST("__INT_FAST64_MAX__");
                    #undef DEFFAST
                }
                DEFUMAX("__UINT_FAST8_MAX__", "255");
                {
                    Atom a = cpp_atomizef(cpp, "18446744073709551615%s", ul_suffix);
                    if(!a) return CPP_OOM_ERROR;
                    #define DEFFAST(name) \
                        err = cpp_define_obj_macro(cpp, SV(name), \
                            (CppToken[]){{.type=CPP_NUMBER, .txt={a->length, a->data}}}, 1); \
                        if(err) return err
                    DEFFAST("__UINT_FAST16_MAX__");
                    DEFFAST("__UINT_FAST32_MAX__");
                    DEFFAST("__UINT_FAST64_MAX__");
                    #undef DEFFAST
                }
                break;
            default:
                DEFNUM("__INT_FAST8_MAX__",  schar_max);
                DEFNUM("__INT_FAST16_MAX__", shrt_max);
                DEFNUM("__INT_FAST32_MAX__", int_max);
                {
                    Atom a = t.sizeof_long == 8 ? long_max : llong_max;
                    err = cpp_define_obj_macro(cpp, SV("__INT_FAST64_MAX__"), (CppToken[]){{.type=CPP_NUMBER, .txt={a->length, a->data}}}, 1);
                    if(err) return err;
                }
                DEFUMAX("__UINT_FAST8_MAX__",  "255");
                DEFUMAX("__UINT_FAST16_MAX__", "65535");
                {
                    Atom a = cpp_atomizef(cpp, "4294967295%s", u_suffix);
                    if(!a) return CPP_OOM_ERROR;
                    err = cpp_define_obj_macro(cpp, SV("__UINT_FAST32_MAX__"), (CppToken[]){{.type=CPP_NUMBER, .txt={a->length, a->data}}}, 1);
                    if(err) return err;
                }
                {
                    const char* suf = t.sizeof_long == 8 ? ul_suffix : ull_suffix;
                    Atom a = cpp_atomizef(cpp, "18446744073709551615%s", suf);
                    if(!a) return CPP_OOM_ERROR;
                    err = cpp_define_obj_macro(cpp, SV("__UINT_FAST64_MAX__"), (CppToken[]){{.type=CPP_NUMBER, .txt={a->length, a->data}}}, 1);
                    if(err) return err;
                }
                break;
        }

        // INTPTR_MAX, UINTPTR_MAX, INTMAX_MAX, UINTMAX_MAX, SIZE_MAX, PTRDIFF_MAX
        {
            const char* isuf = ccbt_literal_suffix(t.intptr_type);
            const char* usuf = ccbt_literal_suffix(ccbt_unsigned_of(t.intptr_type));
            Atom a = cpp_atomizef(cpp, "9223372036854775807%s", isuf);
            if(!a) return CPP_OOM_ERROR;
            err = cpp_define_obj_macro(cpp, SV("__INTPTR_MAX__"), (CppToken[]){{.type=CPP_NUMBER, .txt={a->length, a->data}}}, 1);
            if(err) return err;
            a = cpp_atomizef(cpp, "18446744073709551615%s", usuf);
            if(!a) return CPP_OOM_ERROR;
            err = cpp_define_obj_macro(cpp, SV("__UINTPTR_MAX__"), (CppToken[]){{.type=CPP_NUMBER, .txt={a->length, a->data}}}, 1);
            if(err) return err;
        }
        {
            const char* isuf = ccbt_literal_suffix(t.intmax_type);
            const char* usuf = ccbt_literal_suffix(ccbt_unsigned_of(t.intmax_type));
            Atom a = cpp_atomizef(cpp, "9223372036854775807%s", isuf);
            if(!a) return CPP_OOM_ERROR;
            err = cpp_define_obj_macro(cpp, SV("__INTMAX_MAX__"), (CppToken[]){{.type=CPP_NUMBER, .txt={a->length, a->data}}}, 1);
            if(err) return err;
            a = cpp_atomizef(cpp, "18446744073709551615%s", usuf);
            if(!a) return CPP_OOM_ERROR;
            err = cpp_define_obj_macro(cpp, SV("__UINTMAX_MAX__"), (CppToken[]){{.type=CPP_NUMBER, .txt={a->length, a->data}}}, 1);
            if(err) return err;
        }
        {
            const char* suf = ccbt_literal_suffix(t.size_type);
            Atom a = cpp_atomizef(cpp, "18446744073709551615%s", suf);
            if(!a) return CPP_OOM_ERROR;
            err = cpp_define_obj_macro(cpp, SV("__SIZE_MAX__"), (CppToken[]){{.type=CPP_NUMBER, .txt={a->length, a->data}}}, 1);
            if(err) return err;
        }
        {
            const char* suf = ccbt_literal_suffix(t.ptrdiff_type);
            Atom a = cpp_atomizef(cpp, "9223372036854775807%s", suf);
            if(!a) return CPP_OOM_ERROR;
            err = cpp_define_obj_macro(cpp, SV("__PTRDIFF_MAX__"), (CppToken[]){{.type=CPP_NUMBER, .txt={a->length, a->data}}}, 1);
            if(err) return err;
        }

        // WCHAR_MAX, WCHAR_MIN, WINT_MAX, WINT_MIN, SIG_ATOMIC_MAX, SIG_ATOMIC_MIN
        {
            CcBasicTypeKind wk = t.wchar_type;
            uint8_t wsz = ccbt_sizeof(t, wk);
            _Bool wu = (wk == CCBT_unsigned || wk == CCBT_unsigned_short ||
                        wk == CCBT_unsigned_long || wk == CCBT_unsigned_long_long ||
                        wk == CCBT_unsigned_char);
            if(wu){
                Atom a;
                if(wsz == 4)
                    a = cpp_atomizef(cpp, "4294967295%s", ccbt_literal_suffix(wk));
                else
                    a = cpp_atomizef(cpp, "65535");
                if(!a) return CPP_OOM_ERROR;
                err = cpp_define_obj_macro(cpp, SV("__WCHAR_MAX__"), (CppToken[]){{.type=CPP_NUMBER, .txt={a->length, a->data}}}, 1);
                if(err) return err;
                DEFNUM("__WCHAR_MIN__", "0");
            }
            else {
                Atom a;
                if(wsz == 4)
                    a = cpp_atomizef(cpp, "2147483647");
                else
                    a = cpp_atomizef(cpp, "32767");
                if(!a) return CPP_OOM_ERROR;
                err = cpp_define_obj_macro(cpp, SV("__WCHAR_MAX__"), (CppToken[]){{.type=CPP_NUMBER, .txt={a->length, a->data}}}, 1);
                if(err) return err;
                if(wsz == 4)
                    DEFNUM("__WCHAR_MIN__", "(-__WCHAR_MAX__-1)");
                else
                    DEFNUM("__WCHAR_MIN__", "(-32767-1)");
            }
        }
        {
            CcBasicTypeKind wk = t.wint_type;
            uint8_t wsz = ccbt_sizeof(t, wk);
            _Bool wu = (wk == CCBT_unsigned || wk == CCBT_unsigned_short ||
                        wk == CCBT_unsigned_long || wk == CCBT_unsigned_long_long ||
                        wk == CCBT_unsigned_char);
            if(wu){
                Atom a;
                if(wsz == 4)
                    a = cpp_atomizef(cpp, "4294967295%s", ccbt_literal_suffix(wk));
                else
                    a = cpp_atomizef(cpp, "65535");
                if(!a) return CPP_OOM_ERROR;
                err = cpp_define_obj_macro(cpp, SV("__WINT_MAX__"), (CppToken[]){{.type=CPP_NUMBER, .txt={a->length, a->data}}}, 1);
                if(err) return err;
                DEFNUM("__WINT_MIN__", "0");
            }
            else {
                DEFNUM("__WINT_MAX__", "2147483647");
                DEFNUM("__WINT_MIN__", "(-__WINT_MAX__-1)");
            }
        }
        DEFNUM("__SIG_ATOMIC_MAX__", "2147483647");
        DEFNUM("__SIG_ATOMIC_MIN__", "(-__SIG_ATOMIC_MAX__-1)");

        #undef DEFUMAX
    }

    // __*_WIDTH__ macros
    DEFINT("__SCHAR_WIDTH__",      8);
    DEFINT("__SHRT_WIDTH__",       t.sizeof_short * 8);
    DEFINT("__INT_WIDTH__",        t.sizeof_int * 8);
    DEFINT("__LONG_WIDTH__",       t.sizeof_long * 8);
    DEFINT("__LONG_LONG_WIDTH__",  t.sizeof_long_long * 8);
    DEFINT("__PTRDIFF_WIDTH__",    ccbt_sizeof(t, t.ptrdiff_type) * 8);
    DEFINT("__SIG_ATOMIC_WIDTH__", t.sizeof_int * 8);
    DEFINT("__SIZE_WIDTH__",       ccbt_sizeof(t, t.size_type) * 8);
    DEFINT("__WCHAR_WIDTH__",      ccbt_sizeof(t, t.wchar_type) * 8);
    DEFINT("__WINT_WIDTH__",       ccbt_sizeof(t, t.wint_type) * 8);
    DEFINT("__INT_LEAST8_WIDTH__",  8);
    DEFINT("__INT_LEAST16_WIDTH__", 16);
    DEFINT("__INT_LEAST32_WIDTH__", 32);
    DEFINT("__INT_LEAST64_WIDTH__", 64);
    DEFINT("__INTPTR_WIDTH__",     t.sizeof_pointer * 8);
    DEFINT("__INTMAX_WIDTH__",     ccbt_sizeof(t, t.intmax_type) * 8);
    switch(t.target){
        case CC_TARGET_X86_64_LINUX:
        case CC_TARGET_AARCH64_LINUX:
            DEFINT("__INT_FAST8_WIDTH__",  8);
            DEFINT("__INT_FAST16_WIDTH__", t.sizeof_long * 8);
            DEFINT("__INT_FAST32_WIDTH__", t.sizeof_long * 8);
            DEFINT("__INT_FAST64_WIDTH__", t.sizeof_long * 8);
            break;
        default:
            DEFINT("__INT_FAST8_WIDTH__",  8);
            DEFINT("__INT_FAST16_WIDTH__", 16);
            DEFINT("__INT_FAST32_WIDTH__", 32);
            DEFINT("__INT_FAST64_WIDTH__", 64);
            break;
    }

    // __*_C function-like macros (token pasting)
    {
        // Suffix for 64-bit literal depends on whether long is 64-bit
        const char* i64_suf = t.sizeof_long == 8 ? "L"  : "LL";
        const char* u64_suf = t.sizeof_long == 8 ? "UL" : "ULL";
        // No-suffix: __INT8_C(c) -> c
        // With-suffix: __INT64_C(c) -> c ## L
        struct { StringView name; const char*_Nullable suffix; } c_macros[] = {
            {SV("__INT8_C"),    NULL},
            {SV("__INT16_C"),   NULL},
            {SV("__INT32_C"),   NULL},
            {SV("__INT64_C"),   i64_suf},
            {SV("__UINT8_C"),   NULL},
            {SV("__UINT16_C"),  "U"},
            {SV("__UINT32_C"),  "U"},
            {SV("__UINT64_C"),  u64_suf},
            {SV("__INTMAX_C"),  i64_suf},
            {SV("__UINTMAX_C"), u64_suf},
        };
        Atom c_param = AT_atomize(cpp->at, "c", 1);
        if(!c_param) return CPP_OOM_ERROR;
        for(size_t i = 0; i < sizeof c_macros / sizeof c_macros[0]; i++){
            size_t ntoks = c_macros[i].suffix ? 3 : 1;
            CMacro* macro;
            err = cpp_define_macro(cpp, c_macros[i].name, ntoks, 1, &macro);
            if(err) return err;
            macro->is_function_like = 1;
            cpp_cmacro_params(macro)[0] = c_param;
            CppToken* repl = cpp_cmacro_replacement(macro);
            repl[0] = (CppToken){.type=CPP_IDENTIFIER, .param_idx=1};
            if(c_macros[i].suffix){
                Atom suf_atom = cpp_atomizef(cpp, "%s", c_macros[i].suffix);
                if(!suf_atom) return CPP_OOM_ERROR;
                repl[1] = (CppToken){.type=CPP_PUNCTUATOR, .txt=SV("##"), .punct='##'};
                repl[2] = (CppToken){.type=CPP_IDENTIFIER, .txt={suf_atom->length, suf_atom->data}};
            }
        }
    }

    // Byte order
    DEFNUM("__LITTLE_ENDIAN__", "1234");
    DEFNUM("__ORDER_LITTLE_ENDIAN__", "1234");
    DEFNUM("__ORDER_BIG_ENDIAN__",    "4321");
    DEFNUM("__ORDER_PDP_ENDIAN__",    "3412");
    // All our supported targets are little-endian
    DEFNUM("__BYTE_ORDER__",       "1234");
    DEFNUM("__FLOAT_WORD_ORDER__", "1234");

    // GCC sync builtins
    if(0){ // we don't support these yet
        DEF1("__GCC_HAVE_SYNC_COMPARE_AND_SWAP_1");
        DEF1("__GCC_HAVE_SYNC_COMPARE_AND_SWAP_2");
        DEF1("__GCC_HAVE_SYNC_COMPARE_AND_SWAP_4");
        DEF1("__GCC_HAVE_SYNC_COMPARE_AND_SWAP_8");
        DEF1("__GCC_HAVE_SYNC_COMPARE_AND_SWAP_16");
    }

    // Platform macros
    switch(t.target){
        case CC_TARGET_X86_64_LINUX:
            DEF1("__linux__");
            DEF1("__linux");
            DEF1("linux");
            DEF1("__gnu_linux__");
            DEF1("__unix__");
            DEF1("__unix");
            DEF1("unix");
            DEF1("__x86_64__");
            DEF1("__x86_64");
            DEF1("__amd64__");
            DEF1("__amd64");
            break;
        case CC_TARGET_AARCH64_LINUX:
            DEF1("__linux__");
            DEF1("__linux");
            DEF1("linux");
            DEF1("__gnu_linux__");
            DEF1("__unix__");
            DEF1("__unix");
            DEF1("unix");
            DEF1("__aarch64__");
            break;
        case CC_TARGET_X86_64_MACOS:
            DEF1("__APPLE__");
            DEF1("__MACH__");
            DEF1("__x86_64__");
            DEF1("__x86_64");
            DEF1("__amd64__");
            DEF1("__amd64");
            DEF1("TARGET_CPU_X86_64");
            DEF1("TARGET_OS_MAC");
            DEF1("TARGET_OS_OSX");
            DEFINT("__ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__", 101400);
            break;
        case CC_TARGET_AARCH64_MACOS:
            DEF1("__APPLE__");
            DEF1("__MACH__");
            DEF1("__aarch64__");
            DEF1("__arm64__");
            DEF1("TARGET_CPU_ARM64");
            DEF1("TARGET_OS_MAC");
            DEF1("TARGET_OS_OSX");
            DEFINT("__ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__", 110000);
            break;
        case CC_TARGET_X86_64_WINDOWS:
            DEF1("_WIN32");
            DEF1("_WIN64");
            DEF1("__x86_64__");
            DEF1("__x86_64");
            DEF1("__amd64__");
            DEF1("__amd64");
            DEF1("_M_X64");
            DEF1("_M_AMD64");
            break;
        default:
            break;
    }

    #undef DEF1
    #undef DEFNUM
    #undef DEFINT
    #undef DEFTYPE
    return 0;
}

static
_Bool
cpp_dir_exists(CPreprocessor* cpp, const char* path){
    #ifdef _WIN32
    MStringBuilder16 sb = {.allocator=allocator_from_arena(&cpp->synth_arena)};
    msb16_write_utf8(&sb, path, strlen(path));
    msb16_nul_terminate(&sb);
    if(sb.errored){
        msb16_destroy(&sb);
        return 0;
    }
    DWORD attrs = GetFileAttributesW((LPCWSTR)sb.data);
    msb16_destroy(&sb);
    return attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY);
    #else
    (void)cpp;
    struct stat st;
    return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
    #endif
}

static
int
cpp_add_default_include(CPreprocessor* cpp, Marray(StringView)* arr, const char* path){
    if(!cpp_dir_exists(cpp, path))
        return 0; // silently skip non-existent paths
    size_t len = strlen(path);
    // Atomize so the string lives as long as the atom table.
    Atom a = AT_atomize(cpp->at, path, len);
    if(!a) return CPP_OOM_ERROR;
    StringView sv = {a->length, a->data};
    int err = ma_push(StringView)(arr, cpp->allocator, sv);
    return err ? CPP_OOM_ERROR : 0;
}

static
int
cpp_cache_builtin_header(CPreprocessor* cpp, StringView name, StringView content){
    MStringBuilder* sb = fc_path_builder(cpp->fc);
    msb_write_str(sb, "<builtin>/", 10);
    msb_write_str(sb, name.text, name.length);
    if(fc_cache_file(cpp->fc, content))
        return CPP_OOM_ERROR;
    return 0;
}

static
int
cpp_setup_builtin_headers(CPreprocessor* cpp){
    int err;
    // Cache virtual built-in headers
    static const struct { StringView name; StringView content; } headers[] = {
        {SV("<no source>"), SV("")},
        {SV("stdarg.h"),   SV("#pragma once\n"
                              "#define va_start __builtin_va_start\n"
                              "#define va_copy __builtin_va_copy\n"
                              "#define va_arg __builtin_va_arg\n"
                              "#define va_end __builtin_va_end\n"
                              "typedef __builtin_va_list va_list;\n"
                            )},
        {SV("stddef.h"),   SV("#pragma once\n"
                              "typedef __SIZE_TYPE__ size_t;\n"
                              "typedef __PTRDIFF_TYPE__ ptrdiff_t;\n"
                              "typedef __WCHAR_TYPE__ wchar_t;\n"
                              "typedef typeof(nullptr) nullptr_t;\n"
                              "#ifndef NULL\n"
                              "#define NULL ((void*)0)\n"
                              "#endif\n"
                              "#define offsetof(type, member) ((__SIZE_TYPE__)&((type*)0)->member)\n"
                           )},
        {SV("stdbool.h"),  SV("#pragma once\n"
                              "#define __bool_true_false_are_defined 1\n"
                              "#ifndef bool\n"
                              "#define bool bool\n"
                              "#endif\n"
                              "#ifndef true\n"
                              "#define true true\n"
                              "#endif\n"
                              "#ifndef false\n"
                              "#define false false\n"
                              "#endif\n"
                           )},
        {SV("float.h"),    SV("#pragma once\n"
                              "#if __has_include_next(<float.h>)\n"
                              "#include_next <float.h>\n"
                              "#endif\n"
                            )},
        {SV("limits.h"),   SV("#pragma once\n"
                              "#if __has_include_next(<limits.h>)\n"
                              "#include_next <limits.h>\n"
                              "#endif\n"
                            )},
        {SV("std.h"),      SV("#pragma once\n"
                              "#include <stdarg.h>\n"
                              "#include <stddef.h>\n"
                              "#include <stdbool.h>\n"
                              "#include <float.h>\n"
                              "#include <limits.h>\n"
                              "#include <stdint.h>\n"
                              "#include <stdio.h>\n"
                              "#include <stdlib.h>\n"
                              "#include <string.h>\n"
                              "#include <math.h>\n"
                              "#include <ctype.h>\n"
                              "#include <errno.h>\n"
                              "#include <assert.h>\n"
                              "#include <signal.h>\n"
                              "#include <time.h>\n"
                              "#include <inttypes.h>\n"
                          )},
    };
    for(size_t i = 0; i < sizeof headers / sizeof headers[0]; i++){
        err = cpp_cache_builtin_header(cpp, headers[i].name, headers[i].content);
        if(err) return err;
    }
    // Add <builtin> to isystem_paths so it's searched before platform headers
    StringView sv = SV("<builtin>");
    err = ma_push(StringView)(&cpp->isystem_paths, cpp->allocator, sv);
    if(err) return CPP_OOM_ERROR;
    return 0;
}

static
int
cpp_setup_default_includes(CPreprocessor* cpp){
    int err = cpp_setup_builtin_headers(cpp);
    if(err) return err;
    CcTargetConfig t = cpp->target;
    MStringBuilder sb = {.allocator=allocator_from_arena(&cpp->synth_arena)};
    if((CcTarget)CC_TARGET_NATIVE != t.target)
        goto finally;
    switch(t.target){
        case CC_TARGET_AARCH64_MACOS:
        case CC_TARGET_X86_64_MACOS: {
            err = cpp_add_default_include(cpp, &cpp->istandard_system_paths, "/usr/local/include");
            if(err) goto finally;
            err = cpp_add_default_include(cpp, &cpp->framework_paths, "/Library/Frameworks");
            if(err) goto finally;
            // Find SDK path: $SDKROOT > well-known paths
            const char* sdk = NULL;
            Atom sdkroot = env_getenv2(cpp->env, "SDKROOT", 7);
            if(sdkroot && sdkroot->length){
                sdk = sdkroot->data;
            }
            else {
                static const char* const known_paths[] = {
                    "/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk",
                    "/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk",
                };
                for(size_t i = 0; i < sizeof known_paths / sizeof known_paths[0]; i++){
                    if(cpp_dir_exists(cpp, known_paths[i])){
                        sdk = known_paths[i];
                        break;
                    }
                }
            }
            if(sdk){
                msb_sprintf(&sb, "%s/usr/include", sdk);
                if(!sb.errored){
                    LongString path = msb_borrow_ls(&sb);
                    err = cpp_add_default_include(cpp, &cpp->istandard_system_paths, path.text);
                    if(err) goto finally;
                }
                msb_reset(&sb);
                msb_sprintf(&sb, "%s/System/Library/Frameworks", sdk);
                if(!sb.errored){
                    LongString path = msb_borrow_ls(&sb);
                    err = cpp_add_default_include(cpp, &cpp->framework_paths, path.text);
                    if(err) goto finally;
                }
            }
            break;
        }
        case CC_TARGET_X86_64_LINUX:
        case CC_TARGET_AARCH64_LINUX:
            err = cpp_add_default_include(cpp, &cpp->istandard_system_paths, "/usr/local/include");
            if(err) goto finally;
            err = cpp_add_default_include(cpp, &cpp->istandard_system_paths, "/usr/include");
            if(err) goto finally;
            // arch-specific include path
            if(t.target == CC_TARGET_X86_64_LINUX){
                err = cpp_add_default_include(cpp, &cpp->istandard_system_paths, "/usr/include/x86_64-linux-gnu");
                if(err) goto finally;
            }
            else {
                err = cpp_add_default_include(cpp, &cpp->istandard_system_paths, "/usr/include/aarch64-linux-gnu");
                if(err) goto finally;
            }
            break;
        case CC_TARGET_X86_64_WINDOWS: {
            // Use the INCLUDE env var (set by vcvarsall.bat / Developer Command Prompt).
            // It's a semicolon-separated list of include paths.
            Atom include_env = env_getenv2(cpp->env, "INCLUDE", 7);
            if(include_env && include_env->length){
                const char* s = include_env->data;
                size_t len = include_env->length;
                while(len){
                    const char* semi = memchr(s, ';', len);
                    size_t part = semi ? (size_t)(semi - s) : len;
                    if(part){
                        msb_reset(&sb);
                        msb_write_str(&sb, s, part);
                        if(!sb.errored){
                            LongString path = msb_borrow_ls(&sb);
                            err = cpp_add_default_include(cpp, &cpp->istandard_system_paths, path.text);
                            if(err) goto finally;
                        }
                    }
                    if(semi){
                        s = semi + 1;
                        len -= part + 1;
                    }
                    else
                        break;
                }
            }
            break;
        }
        default:
            break;
    }
    finally:
    msb_destroy(&sb);
    return err;
}

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
        {SV("__RAND__"), cpp_builtin_rand},
        {SV("__RANDOM__"), cpp_builtin_rand},
        {SV("__BASE_FILE__"), cpp_builtin_base_file},
        {SV("__TIMESTAMP__"), cpp_builtin_timestamp},
    };
    for(size_t i = 0; i < sizeof obj_builtins / sizeof obj_builtins[0]; i++){
        err = cpp_define_builtin_obj_macro(cpp, obj_builtins[i].name, obj_builtins[i].fn, NULL);
        if(err) return err;
    }
    err = cpp_define_obj_macro(cpp, SV("__STDC__"), (CppToken[]){{.type=CPP_NUMBER, .txt=SV("1")}}, 1);
    if(err) return err;
    err = cpp_define_obj_macro(cpp, SV("__VERSION__"), (CppToken[]){{.type=CPP_STRING, .txt=SV("\"dvm cc 0.1\"")}}, 1);
    if(err) return err;
    if(0){
        err = cpp_define_obj_macro(cpp, SV("__STDC_VERSION__"), (CppToken[]){{.type=CPP_NUMBER, .txt=SV("202602l")}}, 1);
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
        {SV("__print"), cpp_builtin_print, 0, 1, 0},
        {SV("__PRINT__"), cpp_builtin_print, 0, 1, 0},
        {SV("__set"), cpp_builtin_set, 1, 1, 0},
        {SV("__SET__"), cpp_builtin_set, 1, 1, 0},
        {SV("__get"), cpp_builtin_get, 1, 0, 0},
        {SV("__GET__"), cpp_builtin_get, 1, 0, 0},
        {SV("__append"), cpp_builtin_append, 1, 1, 0},
        {SV("__APPEND__"), cpp_builtin_append, 1, 1, 0},
        {SV("__for"), cpp_builtin_for, 3, 0, 0},
        {SV("__FOR__"), cpp_builtin_for, 3, 0, 0},
        {SV("__map"), cpp_builtin_map, 1, 1, 0},
        {SV("__MAP__"), cpp_builtin_map, 1, 1, 0},
        {SV("__let"), cpp_builtin_let, 3, 0, 1},
        {SV("__LET__"), cpp_builtin_let, 3, 0, 1},
        {SV("_Pragma"), cpp_builtin__Pragma, 1, 0, 0},
        {SV("__WHERE__"), cpp_builtin_where, 1, 0, 1},
        {SV("__where"), cpp_builtin_where, 1, 0, 1},
    };
    for(size_t i = 0; i < sizeof func_builtins / sizeof func_builtins[0]; i++){
        err = cpp_define_builtin_func_macro(cpp, func_builtins[i].name, func_builtins[i].fn, NULL, func_builtins[i].nparams, func_builtins[i].variadic, func_builtins[i].no_expand);
        if(err) return err;
    }
    err = cpp_register_pragma(cpp, SV("once"), cpp_builtin_pragma_once, NULL);
    if(err) return err;
    err = cpp_register_pragma(cpp, SV("message"), cpp_builtin_pragma_message, NULL);
    if(err) return err;
    err = cpp_register_pragma(cpp, SV("include_path"), cpp_builtin_pragma_include_path, NULL);
    if(err) return err;

    err = cpp_define_target_macros(cpp);
    return err;
}
static
int
cpp_builtin_file(void* _Null_unspecified ctx, CPreprocessor* cpp, SrcLoc loc, CppTokens* outtoks){
    (void)ctx;
    uint64_t file_id = 0;
    if(loc.is_actually_a_pointer){
        SrcLocExp* e = (SrcLocExp*)((uintptr_t)loc.bits<<1);
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
    CppToken tok = {
        .txt = {a->length, a->data},
        .loc = loc,
        .type = CPP_STRING,
    };
    int err = cpp_push_tok(cpp, outtoks, tok);
    return err;
}

static
int
cpp_builtin_filename(void* _Null_unspecified ctx, CPreprocessor* cpp, SrcLoc loc, CppTokens* outtoks){
    (void)ctx;
    uint64_t file_id = 0;
    if(loc.is_actually_a_pointer){
        SrcLocExp* e = (SrcLocExp*)((uintptr_t)loc.pointer.bits<<1);
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
    CppToken tok = {
        .txt = {a->length, a->data},
        .loc = loc,
        .type = CPP_STRING,
    };
    int err = cpp_push_tok(cpp, outtoks, tok);
    return err;
}

static
int
cpp_builtin_date(void* _Null_unspecified ctx, CPreprocessor* cpp, SrcLoc loc, CppTokens* outtoks){
    (void)ctx;
    CppToken tok = {
        .txt = cpp->date?(StringView){cpp->date->length, cpp->date->data}: SV("\"Jan 01 1900\""),
        .loc = loc,
        .type = CPP_STRING,
    };
    int err = cpp_push_tok(cpp, outtoks, tok);
    return err;
}

static
int
cpp_builtin_time(void* _Null_unspecified ctx, CPreprocessor* cpp, SrcLoc loc, CppTokens* outtoks){
    (void)ctx;
    CppToken tok = {
        .txt = cpp->time?(StringView){cpp->time->length, cpp->time->data}: SV("\"01:02:03\""),
        .loc = loc,
        .type = CPP_STRING,
    };
    int err = cpp_push_tok(cpp, outtoks, tok);
    return err;
}

static
int
cpp_builtin_line(void* _Null_unspecified ctx, CPreprocessor* cpp, SrcLoc loc, CppTokens* outtoks){
    (void)ctx;
    unsigned line = 0;
    if(loc.is_actually_a_pointer){
        SrcLocExp* e = (SrcLocExp*)((uintptr_t)loc.pointer.bits<<1);
        while(e->parent)
            e = e->parent;
        line = (unsigned)e->line;
    }
    else {
        line = (unsigned)loc.line;
    }
    Atom a = cpp_atomizef(cpp, "%u", line);
    if(!a) return CPP_OOM_ERROR;
    CppToken tok = {
        .txt = {a->length, a->data},
        .loc = loc,
        .type = CPP_NUMBER,
    };
    int err = cpp_push_tok(cpp, outtoks, tok);
    return err;
}
static
int
cpp_builtin_include_level(void* _Null_unspecified ctx, CPreprocessor* cpp, SrcLoc loc, CppTokens* outtoks){
    (void)ctx;
    if(!cpp->frames.count) return CPP_UNREACHABLE_ERROR;
    Atom a = cpp_atomizef(cpp, "%zu", cpp->frames.count);
    if(!a) return CPP_OOM_ERROR;
    CppToken tok = {
        .txt = {a->length, a->data},
        .loc = loc,
        .type = CPP_NUMBER,
    };
    int err = cpp_push_tok(cpp, outtoks, tok);
    return err;
}
static
int
cpp_builtin_counter(void* _Null_unspecified ctx, CPreprocessor* cpp, SrcLoc loc, CppTokens* outtoks){
    (void)ctx;
    uint64_t c = cpp->counter++;
    Atom a = cpp_atomizef(cpp, "%llu", (unsigned long long)c);
    if(!a) return CPP_OOM_ERROR;
    CppToken tok = {
        .txt = {a->length, a->data},
        .loc = loc,
        .type = CPP_NUMBER,
    };
    int err = cpp_push_tok(cpp, outtoks, tok);
    return err;
}
static
int
cpp_builtin_rand(void* _Null_unspecified ctx, CPreprocessor* cpp, SrcLoc loc, CppTokens* outtoks){
    (void)ctx;
    if(!cpp->rng.state && !cpp->rng.inc)
        seed_rng_auto(&cpp->rng);
    uint32_t r = rng_random32(&cpp->rng);
    Atom a = cpp_atomizef(cpp, "%u", (unsigned)r);
    if(!a) return CPP_OOM_ERROR;
    CppToken tok = {
        .txt = {a->length, a->data},
        .loc = loc,
        .type = CPP_NUMBER,
    };
    int err = cpp_push_tok(cpp, outtoks, tok);
    return err;
}

static
int
cpp_builtin_base_file(void* _Null_unspecified ctx, CPreprocessor* cpp, SrcLoc loc, CppTokens* outtoks){
    (void)ctx;
    // __BASE_FILE__ is the outermost file (bottom of frame stack).
    uint64_t file_id = 0;
    if(cpp->frames.count)
        file_id = cpp->frames.data[0].file_id;
    LongString path = file_id < cpp->fc->map.count?cpp->fc->map.data[file_id].path:LS("???");
    Atom a = cpp_atomizef(cpp, "\"%s\"", path.text);
    if(!a) return CPP_OOM_ERROR;
    CppToken tok = {
        .txt = {a->length, a->data},
        .loc = loc,
        .type = CPP_STRING,
    };
    return cpp_push_tok(cpp, outtoks, tok);
}

static
int
cpp_builtin_timestamp(void* _Null_unspecified ctx, CPreprocessor* cpp, SrcLoc loc, CppTokens* outtoks){
    (void)ctx;
    // __TIMESTAMP__ is the modification time of the current source file.
    // For simplicity, use the same as __DATE__ __TIME__ combined.
    // GCC format: "Sun Sep 16 01:03:52 1973"
    // We just return a placeholder.
    CppToken tok = {
        .txt = SV("\"??? ??? ?? ??:??:?? ????\""),
        .loc = loc,
        .type = CPP_STRING,
    };
    return cpp_push_tok(cpp, outtoks, tok);
}

static
int
cpp_builtin_eval(void* _Null_unspecified ctx, CPreprocessor* cpp, SrcLoc loc, CppTokens* outtoks, const CppTokens* args, const Marray(size_t)* arg_seps){
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
    CppToken tok = {
        .txt = {a->length, a->data},
        .loc = loc,
        .type = CPP_NUMBER,
    };
    err = cpp_push_tok(cpp, outtoks, tok);
    return err;
}

static
int
cpp_builtin_mixin(void* _Null_unspecified ctx, CPreprocessor* cpp, SrcLoc loc, CppTokens* outtoks, const CppTokens* args, const Marray(size_t)*arg_seps){
    (void)ctx;
    (void)arg_seps;
    int err = 0;
    MStringBuilder sb = {.allocator = allocator_from_arena(&cpp->synth_arena)};
    for(size_t i = 0; i < args->count; i++){
        CppToken tok = args->data[i];
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
cpp_builtin_if(void* _Null_unspecified ctx, CPreprocessor* cpp, SrcLoc loc, CppTokens* outtoks, const CppTokens* args, const Marray(size_t)*arg_seps){
    (void)ctx;
    (void)loc;
    CppToken *toks; size_t count;
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
cpp_builtin_ident(void* _Null_unspecified ctx, CPreprocessor* cpp, SrcLoc loc, CppTokens* outtoks, const CppTokens* args, const Marray(size_t)*arg_seps){
    (void)ctx;
    (void)arg_seps;
    int err = 0;
    MStringBuilder sb = {.allocator = allocator_from_arena(&cpp->synth_arena)};
    for(size_t i = 0; i < args->count; i++){
        CppToken tok = args->data[i];
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
    CppToken tok = {
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
cpp_builtin_fmt(void* _Null_unspecified ctx, CPreprocessor* cpp, SrcLoc loc, CppTokens* outtoks, const CppTokens* args, const Marray(size_t)*arg_seps){
    (void)ctx;
    int err = 0;
    MStringBuilder sb = {.allocator = allocator_from_arena(&cpp->synth_arena)};
    msb_write_char(&sb, '"');
    CppToken* fmts; size_t fmt_count;
    CppToken* va_args; size_t va_count;
    cpp_get_argument(args, arg_seps, 0, &fmts, &fmt_count);
    cpp_get_va_args(args, arg_seps, 1, &va_args, &va_count);
    for(size_t f = 0; f < fmt_count; f++){
        CppToken fmt = fmts[f];
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
                            CppToken tok = *va_args;
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
                            CppToken tok = *va_args;
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
    CppToken tok = {
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
cpp_builtin_print(void* _Null_unspecified ctx, CPreprocessor* cpp, SrcLoc loc, CppTokens* outtoks, const CppTokens* args, const Marray(size_t)*arg_seps){
    (void)outtoks;
    (void)ctx;
    (void)arg_seps;
    MStringBuilder* sb = &cpp->logger->buff;
    uint64_t line = 0;
    uint64_t column = 0;
    uint64_t file_id = 0;
    SrcLocExp* e = NULL;
    if(loc.is_actually_a_pointer){
        e = (SrcLocExp*)((uintptr_t)loc.pointer.bits<<1);
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
    msb_sprintf(sb, "%s:%d:%d: ", path.text, (int)line, (int)column);
    for(size_t i = 0; i < args->count; i++){
        CppToken tok = args->data[i];
        msb_write_str(sb, tok.txt.text, tok.txt.length);
    }
    log_flush(cpp->logger, LOG_PRINT_ERROR);
    return 0;
}
static
int
cpp_builtin_where(void* _Null_unspecified ctx, CPreprocessor* cpp, SrcLoc loc, CppTokens* outtoks, const CppTokens* args, const Marray(size_t)*arg_seps){
    (void)ctx; (void)outtoks; (void)arg_seps;
    // Find the first non-whitespace token — should be an identifier
    const CppToken* name_tok = NULL;
    for(size_t i = 0; i < args->count; i++){
        if(args->data[i].type != CPP_WHITESPACE){
            name_tok = &args->data[i];
            break;
        }
    }
    if(!name_tok || name_tok->type != CPP_IDENTIFIER)
        return cpp_error(cpp, loc, "__where__: expected a macro name");
    Atom a = AT_get_atom(cpp->at, name_tok->txt.text, name_tok->txt.length);
    CMacro* m = a ? AM_get(&cpp->macros, a) : NULL;
    if(!m){
        cpp_warn(cpp, loc, "__where__: '%.*s' is not defined", (int)name_tok->txt.length, name_tok->txt.text);
        return 0;
    }
    cpp_info(cpp, m->def_loc, "%s defined", a->data);
    return 0;
}
static
int
cpp_builtin_env(void* _Null_unspecified ctx, CPreprocessor* cpp, SrcLoc loc, CppTokens* outtoks, const CppTokens* args, const Marray(size_t)*arg_seps){
    (void)ctx;
    (void)arg_seps;
    int err = 0;
    MStringBuilder sb = {.allocator = allocator_from_arena(&cpp->synth_arena)};
    for(size_t i = 0; i < args->count; i++){
        CppToken tok = args->data[i];
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
    CppToken tok = {
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
cpp_builtin_set(void* _Null_unspecified ctx, CPreprocessor* cpp, SrcLoc loc, CppTokens* outtoks, const CppTokens* args, const Marray(size_t)*arg_seps){
    (void)ctx;
    (void)outtoks;
    // First arg is the key (an identifier)
    CppToken* key_toks; size_t key_count;
    cpp_get_argument(args, arg_seps, 0, &key_toks, &key_count);
    // Find the identifier token for the key
    Atom key = NULL;
    for(size_t i = 0; i < key_count; i++){
        if(key_toks[i].type == CPP_WHITESPACE || key_toks[i].type == CPP_NEWLINE) continue;
        if(key_toks[i].type != CPP_IDENTIFIER)
            return cpp_error(cpp, key_toks[i].loc, "Expected identifier as key to __set");
        if(key)
            return cpp_error(cpp, args->data[i].loc, "Only one identifier as key to __set");
        key = AT_atomize(cpp->at, key_toks[i].txt.text, key_toks[i].txt.length);
        if(!key) return CPP_OOM_ERROR;
    }
    if(!key) return cpp_error(cpp, loc, "Missing key argument to __set");
    // Get the value tokens (variadic args after the key)
    CppToken* val_toks; size_t val_count;
    cpp_get_va_args(args, arg_seps, 1, &val_toks, &val_count);
    // Strip leading/trailing whitespace from value tokens
    while(val_count && (val_toks[0].type == CPP_WHITESPACE || val_toks[0].type == CPP_NEWLINE)){
        val_toks++; val_count--;
    }
    while(val_count && (val_toks[val_count-1].type == CPP_WHITESPACE || val_toks[val_count-1].type == CPP_NEWLINE)){
        val_count--;
    }
    // Check if there's already a stored value for this key
    CppTokens* existing = AM_get(&cpp->kv_store, key);
    if(existing){
        // Reuse the existing array
        existing->count = 0;
        if(val_count){
            int err = ma_extend(CppToken)(existing, cpp->allocator, val_toks, val_count);
            if(err) return CPP_OOM_ERROR;
        }
    }
    else {
        // Allocate a new CppTokens
        CppTokens* stored = Allocator_zalloc(cpp->allocator, sizeof(CppTokens));
        if(!stored) return CPP_OOM_ERROR;
        if(val_count){
            int err = ma_extend(CppToken)(stored, cpp->allocator, val_toks, val_count);
            if(err) return CPP_OOM_ERROR;
        }
        int err = AM_put(&cpp->kv_store, cpp->allocator, key, stored);
        if(err) return CPP_OOM_ERROR;
    }
    // __set expands to nothing
    return 0;
}

static
int
cpp_builtin_get(void* _Null_unspecified ctx, CPreprocessor* cpp, SrcLoc loc, CppTokens* outtoks, const CppTokens* args, const Marray(size_t)*arg_seps){
    (void)ctx;
    (void)arg_seps;
    // The single arg is the key (an identifier)
    Atom key = NULL;
    for(size_t i = 0; i < args->count; i++){
        if(args->data[i].type == CPP_WHITESPACE || args->data[i].type == CPP_NEWLINE) continue;
        if(args->data[i].type != CPP_IDENTIFIER)
            return cpp_error(cpp, args->data[i].loc, "Expected identifier as key to __get");
        if(key)
            return cpp_error(cpp, args->data[i].loc, "Only one identifier as key to __get");
        key = AT_get_atom(cpp->at, args->data[i].txt.text, args->data[i].txt.length);
    }
    if(!key){
        // Key was never set, expand to nothing
        return 0;
    }
    CppTokens* stored = AM_get(&cpp->kv_store, key);
    if(!stored) return 0; // not set, expand to nothing
    for(size_t i = 0; i < stored->count; i++){
        CppToken tok = stored->data[i];
        tok.loc = loc;
        int err = cpp_push_tok(cpp, outtoks, tok);
        if(err) return err;
    }
    return 0;
}

static
int
cpp_builtin_append(void* _Null_unspecified ctx, CPreprocessor* cpp, SrcLoc loc, CppTokens* outtoks, const CppTokens* args, const Marray(size_t)*arg_seps){
    (void)ctx;
    (void)outtoks;
    // First arg is the key (an identifier)
    CppToken* key_toks; size_t key_count;
    cpp_get_argument(args, arg_seps, 0, &key_toks, &key_count);
    Atom key = NULL;
    for(size_t i = 0; i < key_count; i++){
        if(key_toks[i].type == CPP_WHITESPACE || key_toks[i].type == CPP_NEWLINE) continue;
        if(key_toks[i].type != CPP_IDENTIFIER)
            return cpp_error(cpp, key_toks[i].loc, "Expected identifier as key to __append");
        if(key)
            return cpp_error(cpp, args->data[i].loc, "Only one identifier as key to __append");
        key = AT_atomize(cpp->at, key_toks[i].txt.text, key_toks[i].txt.length);
        if(!key) return CPP_OOM_ERROR;
    }
    if(!key) return cpp_error(cpp, loc, "Missing key argument to __append");
    // Get the value tokens (variadic args after the key)
    CppToken* val_toks; size_t val_count;
    cpp_get_va_args(args, arg_seps, 1, &val_toks, &val_count);
    if(!val_count) return 0; // nothing to append
    CppTokens* existing = AM_get(&cpp->kv_store, key);
    if(existing){
        int err = ma_extend(CppToken)(existing, cpp->allocator, val_toks, val_count);
        if(err) return CPP_OOM_ERROR;
    }
    else {
        CppTokens* stored = Allocator_zalloc(cpp->allocator, sizeof(CppTokens));
        if(!stored) return CPP_OOM_ERROR;
        int err = ma_extend(CppToken)(stored, cpp->allocator, val_toks, val_count);
        if(err) return CPP_OOM_ERROR;
        err = AM_put(&cpp->kv_store, cpp->allocator, key, stored);
        if(err) return CPP_OOM_ERROR;
    }
    return 0;
}

static
int
cpp_builtin_for(void* _Null_unspecified ctx, CPreprocessor* cpp, SrcLoc loc, CppTokens* outtoks, const CppTokens* args, const Marray(size_t)*arg_seps){
    (void)ctx;
    int err;
    // arg 0: start expression
    // arg 1: end expression
    // arg 2: macro name (identifier)
    CppToken* start_toks; size_t start_count;
    CppToken* end_toks; size_t end_count;
    CppToken* macro_toks; size_t macro_count;
    cpp_get_argument(args, arg_seps, 0, &start_toks, &start_count);
    cpp_get_argument(args, arg_seps, 1, &end_toks, &end_count);
    cpp_get_argument(args, arg_seps, 2, &macro_toks, &macro_count);
    int64_t start_val, end_val;
    err = cpp_eval_tokens(cpp, start_toks, start_count, &start_val);
    if(err) return err;
    err = cpp_eval_tokens(cpp, end_toks, end_count, &end_val);
    if(err) return err;
    // Find the macro name identifier
    CppToken macro_ident = {0};
    _Bool found = 0;
    for(size_t i = 0; i < macro_count; i++){
        if(macro_toks[i].type == CPP_WHITESPACE || macro_toks[i].type == CPP_NEWLINE) continue;
        if(macro_toks[i].type != CPP_IDENTIFIER){
            return cpp_error(cpp, macro_toks[i].loc, "Expected macro name as third argument to __for");
        }
        macro_ident = macro_toks[i];
        found = 1;
        break;
    }
    if(!found) return cpp_error(cpp, loc, "Missing macro name argument to __for");
    // For each value in [start, end), emit MACRO(value) tokens
    for(int64_t i = start_val; i < end_val; i++){
        Atom num = cpp_atomizef(cpp, "%lld", (long long)i);
        if(!num) return CPP_OOM_ERROR;
        CppToken num_tok = {
            .txt = {num->length, num->data},
            .loc = loc,
            .type = CPP_NUMBER,
        };
        CppToken lparen = {.type = CPP_PUNCTUATOR, .punct = '(', .loc = loc, .txt = SV("(")};
        CppToken rparen = {.type = CPP_PUNCTUATOR, .punct = ')', .loc = loc, .txt = SV(")")};
        CppToken space = {.type = CPP_WHITESPACE, .loc = loc, .txt = SV(" ")};
        if(i > start_val){
            err = cpp_push_tok(cpp, outtoks, space);
            if(err) return err;
        }
        err = cpp_push_tok(cpp, outtoks, macro_ident);
        if(err) return err;
        err = cpp_push_tok(cpp, outtoks, lparen);
        if(err) return err;
        err = cpp_push_tok(cpp, outtoks, num_tok);
        if(err) return err;
        err = cpp_push_tok(cpp, outtoks, rparen);
        if(err) return err;
    }
    return 0;
}

static
int
cpp_builtin_map(void* _Null_unspecified ctx, CPreprocessor* cpp, SrcLoc loc, CppTokens* outtoks, const CppTokens* args, const Marray(size_t)*arg_seps){
    (void)ctx;
    int err;
    // arg 0: macro name (identifier)
    // variadic: items to map over, comma-separated
    // The variadic portion arrives as a single token blob with embedded commas,
    // so we split on comma punctuators ourselves.
    CppToken* macro_toks; size_t macro_count;
    cpp_get_argument(args, arg_seps, 0, &macro_toks, &macro_count);
    CppToken macro_ident = {0};
    _Bool found = 0;
    for(size_t i = 0; i < macro_count; i++){
        if(macro_toks[i].type == CPP_WHITESPACE || macro_toks[i].type == CPP_NEWLINE) continue;
        if(macro_toks[i].type != CPP_IDENTIFIER)
            return cpp_error(cpp, macro_toks[i].loc, "Expected macro name as first argument to __map");
        if(found)
            return cpp_error(cpp, macro_toks[i].loc, "Expected single macro name as first argument to __map");
        macro_ident = macro_toks[i];
        found = 1;
    }
    if(!found) return cpp_error(cpp, loc, "Missing macro name argument to __map");
    // Get the variadic portion as a raw token blob
    CppToken* va_toks; size_t va_count;
    cpp_get_va_args(args, arg_seps, 1, &va_toks, &va_count);
    if(!va_count) return 0;
    CppToken lparen = {.type = CPP_PUNCTUATOR, .punct = '(', .loc = loc, .txt = SV("(")};
    CppToken rparen = {.type = CPP_PUNCTUATOR, .punct = ')', .loc = loc, .txt = SV(")")};
    CppToken space = {.type = CPP_WHITESPACE, .loc = loc, .txt = SV(" ")};
    // Iterate over va_toks, splitting on top-level commas (respecting parens)
    size_t arg_start = 0;
    size_t arg_idx = 0;
    int paren_depth = 0;
    for(size_t i = 0; i <= va_count; i++){
        if(i < va_count && va_toks[i].type == CPP_PUNCTUATOR){
            if(va_toks[i].punct == '(') paren_depth++;
            else if(va_toks[i].punct == ')') paren_depth--;
        }
        _Bool is_comma = i < va_count && paren_depth == 0 && va_toks[i].type == CPP_PUNCTUATOR && va_toks[i].punct == ',';
        _Bool is_end = i == va_count;
        if(!is_comma && !is_end) continue;
        // We have an argument from arg_start to i (exclusive)
        CppToken* atoks = va_toks + arg_start;
        size_t acount = i - arg_start;
        // Strip leading/trailing whitespace
        while(acount && (atoks[0].type == CPP_WHITESPACE || atoks[0].type == CPP_NEWLINE)){
            atoks++; acount--;
        }
        while(acount && (atoks[acount-1].type == CPP_WHITESPACE || atoks[acount-1].type == CPP_NEWLINE)){
            acount--;
        }
        if(!acount){
            arg_start = i + 1;
            continue;
        }
        if(arg_idx > 0){
            err = cpp_push_tok(cpp, outtoks, space);
            if(err) return err;
        }
        err = cpp_push_tok(cpp, outtoks, macro_ident);
        if(err) return err;
        err = cpp_push_tok(cpp, outtoks, lparen);
        if(err) return err;
        for(size_t t = 0; t < acount; t++){
            err = cpp_push_tok(cpp, outtoks, atoks[t]);
            if(err) return err;
        }
        err = cpp_push_tok(cpp, outtoks, rparen);
        if(err) return err;
        arg_start = i + 1;
        arg_idx++;
    }
    return 0;
}

static
int
cpp_builtin_let(void* _Null_unspecified ctx, CPreprocessor* cpp, SrcLoc loc, CppTokens* outtoks, const CppTokens* args, const Marray(size_t)*arg_seps){
    (void)ctx;
    int err;
    // arg 0: NAME or NAME(params...)
    // arg 1: replacement tokens
    // arg 2: body to expand
    CppToken* sig_toks; size_t sig_count;
    CppToken* repl_toks; size_t repl_count;
    CppToken* body_toks; size_t body_count;
    cpp_get_argument(args, arg_seps, 0, &sig_toks, &sig_count);
    cpp_get_argument(args, arg_seps, 1, &repl_toks, &repl_count);
    cpp_get_argument(args, arg_seps, 2, &body_toks, &body_count);
    // Parse signature: find macro name, then optional (params)
    StringView macro_name = {0};
    SrcLoc name_loc = loc;
    size_t sig_pos = 0;
    // Skip leading whitespace
    while(sig_pos < sig_count && (sig_toks[sig_pos].type == CPP_WHITESPACE || sig_toks[sig_pos].type == CPP_NEWLINE))
        sig_pos++;
    if(sig_pos >= sig_count || sig_toks[sig_pos].type != CPP_IDENTIFIER)
        return cpp_error(cpp, loc, "Expected macro name in __let");
    macro_name = sig_toks[sig_pos].txt;
    name_loc = sig_toks[sig_pos].loc;
    sig_pos++;
    // Check for function-like: NAME(params...)
    _Bool is_function_like = 0;
    Atom param_names[64];
    size_t nparams = 0;
    // Skip whitespace between name and potential (
    while(sig_pos < sig_count && sig_toks[sig_pos].type == CPP_WHITESPACE)
        sig_pos++;
    if(sig_pos < sig_count && sig_toks[sig_pos].type == CPP_PUNCTUATOR && sig_toks[sig_pos].punct == '('){
        is_function_like = 1;
        sig_pos++; // skip (
        for(;;){
            while(sig_pos < sig_count && (sig_toks[sig_pos].type == CPP_WHITESPACE || sig_toks[sig_pos].type == CPP_NEWLINE))
                sig_pos++;
            if(sig_pos >= sig_count)
                return cpp_error(cpp, loc, "Unterminated parameter list in __let");
            if(sig_toks[sig_pos].type == CPP_PUNCTUATOR && sig_toks[sig_pos].punct == ')')
                break;
            if(nparams > 0){
                if(sig_toks[sig_pos].type != CPP_PUNCTUATOR || sig_toks[sig_pos].punct != ',')
                    return cpp_error(cpp, sig_toks[sig_pos].loc, "Expected ',' between params in __let");
                sig_pos++;
                while(sig_pos < sig_count && sig_toks[sig_pos].type == CPP_WHITESPACE)
                    sig_pos++;
            }
            if(sig_pos >= sig_count || sig_toks[sig_pos].type != CPP_IDENTIFIER)
                return cpp_error(cpp, loc, "Expected param name in __let");
            if(nparams >= 64)
                return cpp_error(cpp, loc, "Too many params in __let (max 64)");
            Atom a = AT_atomize(cpp->at, sig_toks[sig_pos].txt.text, sig_toks[sig_pos].txt.length);
            if(!a) return CPP_OOM_ERROR;
            param_names[nparams++] = a;
            sig_pos++;
        }
    }
    // Strip whitespace from replacement tokens
    while(repl_count && (repl_toks[0].type == CPP_WHITESPACE || repl_toks[0].type == CPP_NEWLINE)){
        repl_toks++; repl_count--;
    }
    while(repl_count && (repl_toks[repl_count-1].type == CPP_WHITESPACE || repl_toks[repl_count-1].type == CPP_NEWLINE)){
        repl_count--;
    }
    // Define the temporary macro
    if(is_function_like){
        CMacro* m;
        err = cpp_define_macro(cpp, macro_name, repl_count, nparams, &m);
        if(err == CPP_MACRO_ALREADY_EXISTS_ERROR)
            return cpp_error(cpp, name_loc, "__let macro name '%.*s' already defined", sv_p(macro_name));
        if(err) return err;
        m->is_function_like = 1;
        Atom* params = cpp_cmacro_params(m);
        for(size_t i = 0; i < nparams; i++)
            params[i] = param_names[i];
        // Copy replacement tokens, tagging param_idx
        CppToken* repl = cpp_cmacro_replacement(m);
        for(size_t i = 0; i < repl_count; i++){
            repl[i] = repl_toks[i];
            if(repl[i].type == CPP_IDENTIFIER){
                Atom a = AT_get_atom(cpp->at, repl[i].txt.text, repl[i].txt.length);
                if(a){
                    for(size_t p = 0; p < nparams; p++){
                        if(a == params[p]){
                            repl[i].param_idx = p + 1;
                            break;
                        }
                    }
                }
            }
        }
    }
    else {
        err = cpp_define_obj_macro(cpp, macro_name, repl_toks, repl_count);
        if(err == CPP_MACRO_ALREADY_EXISTS_ERROR)
            return cpp_error(cpp, name_loc, "__let macro name '%.*s' already defined", sv_p(macro_name));
        if(err) return err;
    }
    err = cpp_expand_argument(cpp, body_toks, body_count, outtoks);
    cpp_undef_macro(cpp, macro_name);
    return err;
}

static
int
cpp_builtin_pragma_once(void* _Null_unspecified ctx, CPreprocessor* cpp, SrcLoc loc, const CppToken*_Null_unspecified toks, size_t ntoks){
    (void)ctx;
    (void)toks;
    if(ntoks)
        cpp_warn(cpp, loc, "Trailing tokens after #pragma once");
    if(!cpp->frames.count) return 0;
    uint32_t file_id = ma_tail(cpp->frames).file_id;
    return cpp_add_pragma_once(cpp, file_id);
}

static
int
cpp_builtin_pragma_message(void* _Null_unspecified ctx, CPreprocessor* cpp, SrcLoc loc, const CppToken*_Null_unspecified toks, size_t ntoks){
    (void)ctx;
    // Macro-expand the tokens
    CppTokens* expanded = cpp_get_scratch(cpp);
    if(!expanded) return CPP_OOM_ERROR;
    int err = cpp_expand_argument(cpp, toks, ntoks, expanded);
    if(err){ cpp_release_scratch(cpp, expanded); return err; }
    const CppToken* etoks = expanded->data;
    size_t en = expanded->count;
    // Strip outer parens if present: #pragma message("hello") -> "hello"
    if(en >= 2
    && etoks[0].type == CPP_PUNCTUATOR && etoks[0].punct == '('
    && etoks[en-1].type == CPP_PUNCTUATOR && etoks[en-1].punct == ')'){
        etoks++;
        en -= 2;
    }
    MStringBuilder sb = {.allocator = allocator_from_arena(&cpp->synth_arena)};
    for(size_t i = 0; i < en; i++){
        CppToken tok = etoks[i];
        if(tok.type == CPP_STRING){
            // Strip quotes
            if(tok.txt.length > 2)
                msb_write_str(&sb, tok.txt.text + 1, tok.txt.length - 2);
        }
        else {
            msb_write_str(&sb, tok.txt.text, tok.txt.length);
        }
    }
    StringView msg = msb_borrow_sv(&sb);
    cpp_info(cpp, loc, "%.*s", sv_p(msg));
    msb_destroy(&sb);
    cpp_release_scratch(cpp, expanded);
    return 0;
}

static
int
cpp_builtin_pragma_include_path(void* _Null_unspecified ctx, CPreprocessor* cpp, SrcLoc loc, const CppToken*_Null_unspecified toks, size_t ntoks){
    (void)ctx;
    // Macro-expand the tokens
    CppTokens* expanded = cpp_get_scratch(cpp);
    if(!expanded) return CPP_OOM_ERROR;
    int err = cpp_expand_argument(cpp, toks, ntoks, expanded);
    if(err){ cpp_release_scratch(cpp, expanded); return err; }
    const CppToken* etoks = expanded->data;
    size_t en = expanded->count;
    // Find the string literal
    size_t i = 0;
    while(i < en && etoks[i].type == CPP_WHITESPACE) i++;
    if(i >= en || etoks[i].type != CPP_STRING){
        cpp_release_scratch(cpp, expanded);
        return cpp_error(cpp, loc, "#pragma include_path requires a string literal path");
    }
    CppToken strtok = etoks[i];
    i++;
    // Warn on trailing tokens
    while(i < en && etoks[i].type == CPP_WHITESPACE) i++;
    if(i < en)
        cpp_warn(cpp, loc, "Trailing tokens after #pragma include_path");
    // Extract path from string literal (strip quotes)
    StringView path = {strtok.txt.length - 2, strtok.txt.text + 1};
    err = ma_push(StringView)(&cpp->Ipaths, cpp->allocator, path);
    cpp_release_scratch(cpp, expanded);
    if(err) return CPP_OOM_ERROR;
    return 0;
}

static
int
cpp_builtin__Pragma(void* _Null_unspecified ctx, CPreprocessor* cpp, SrcLoc loc, CppTokens* outtoks, const CppTokens* args, const Marray(size_t)*arg_seps){
    (void)ctx;
    (void)outtoks;
    (void)arg_seps;
    // Find the string literal argument
    CppToken strtok = {0};
    for(size_t i = 0; i < args->count; i++){
        if(args->data[i].type == CPP_WHITESPACE) continue;
        if(args->data[i].type != CPP_STRING)
            return cpp_error(cpp, args->data[i].loc, "_Pragma requires a string literal");
        strtok = args->data[i];
        break;
    }
    if(!strtok.type)
        return cpp_error(cpp, loc, "_Pragma requires a string literal");
    // Destringify: strip quotes, unescape backslash-quote and backslash-backslash
    StringView str = {strtok.txt.length - 2, strtok.txt.text + 1};
    MStringBuilder sb = {.allocator = allocator_from_arena(&cpp->synth_arena)};
    for(size_t i = 0; i < str.length; i++){
        char c = str.text[i];
        if(c == '\\' && i + 1 < str.length && (str.text[i+1] == '\\' || str.text[i+1] == '"')){
            msb_write_char(&sb, str.text[++i]);
            continue;
        }
        msb_write_char(&sb, c);
    }
    // Tokenize the destringified content
    CppTokens* toks = cpp_get_scratch(cpp);
    if(!toks){ msb_destroy(&sb); return CPP_OOM_ERROR; }
    int err = cpp_mixin_string(cpp, loc, msb_borrow_sv(&sb), toks);
    msb_destroy(&sb);
    if(err){ cpp_release_scratch(cpp, toks); return err; }
    // Find pragma name (first non-whitespace identifier)
    size_t ti = 0;
    while(ti < toks->count && toks->data[ti].type == CPP_WHITESPACE) ti++;
    if(ti >= toks->count){
        cpp_release_scratch(cpp, toks);
        return 0; // empty _Pragma - do nothing
    }
    if(toks->data[ti].type != CPP_IDENTIFIER){
        cpp_release_scratch(cpp, toks);
        return cpp_error(cpp, loc, "Expected pragma name in _Pragma");
    }
    StringView prag_name_sv = toks->data[ti].txt;
    Atom prag_name = AT_get_atom(cpp->at, prag_name_sv.text, prag_name_sv.length);
    CppPragma* prag = prag_name ? AM_get(&cpp->pragmas, prag_name) : NULL;
    if(!prag){
        // Unknown pragma - silently ignore
        cpp_release_scratch(cpp, toks);
        return 0;
    }
    // Collect remaining tokens (skip leading whitespace after pragma name)
    ti++;
    while(ti < toks->count && toks->data[ti].type == CPP_WHITESPACE) ti++;
    // Strip trailing whitespace
    size_t end = toks->count;
    while(end > ti && toks->data[end-1].type == CPP_WHITESPACE) end--;
    err = prag->fn(prag->ctx, cpp, loc, toks->data + ti, end - ti);
    cpp_release_scratch(cpp, toks);
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

static
int
cpp_push_tok(CPreprocessor* cpp, CppTokens* dst, CppToken tok){
    int err = ma_push(CppToken)(dst, cpp->allocator, tok);
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
        return (SrcLocExp*)((uintptr_t)loc.pointer.bits << 1);
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
    CppFrame init = {
        .file_id = (uint32_t)cpp->fc->map.count-1,
        .txt = txt,
        .line = 1,
        .column = 1,
    };
    err = ma_push(CppFrame)(&cpp->frames, cpp->allocator, init);
    return err?CPP_OOM_ERROR:0;
}

typedef struct CppTokenStream CppTokenStream;
struct CppTokenStream {
    CppToken* toks;
    size_t count;
    size_t cursor;
    CppTokens *pending;
};

static CppToken cpp_ts_next(CppTokenStream* s);

static
int
cpp_eval_ts_next(CPreprocessor* cpp, CppTokenStream* s, CppToken *tok);
static
int
cpp_recursive_eval(CPreprocessor* cpp, CppTokenStream* s, int64_t* value);
static
int
cpp_recursive_eval_prec(CPreprocessor* cpp, CppTokenStream* s, int64_t* value, int min_prec);

static
int
cpp_eval_tokens(CPreprocessor* cpp, CppToken*_Null_unspecified toks, size_t count, int64_t* value){
    int err = 0;
    CppTokens *pending = NULL;
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
cpp_eval_ts_next(CPreprocessor* cpp, CppTokenStream* s, CppToken *outtok){
    int err;
    CppToken tok;
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
            *outtok = (CppToken){.type=CPP_NUMBER, .txt = SV("1"), .loc = tok.loc};
            return 0;
        }
        if(sv_equals(name, SV("__has_include")) || sv_equals(name, SV("__has_include_next")) || sv_equals(name, SV("__has_embed"))){
            _Bool is_embed = name.text[6] == 'e';
            _Bool is_next = name.length > 14 && name.text[14] == '_';
            CppToken next;
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
                IncludePosition dummy;
                _Bool result = !is_embed && cpp_find_include(cpp, quote, is_next, header_name, &dummy) == 0;
                if(result) msb_reset(fc_path_builder(cpp->fc));
                msb_destroy(&sb);
                do { next = cpp_ts_next(s); } while(next.type == CPP_WHITESPACE);
                if(next.type != CPP_PUNCTUATOR || next.punct != ')')
                    return cpp_error(cpp, tok.loc, "Expected ')' after %.*s", (int)name.length, name.text);
                *outtok = (CppToken){.type=CPP_NUMBER, .txt = result?SV("1"):SV("0"), .loc = tok.loc};
                return 0;
            }
            else {
                // Macro-expanded form: collect tokens until ), expand, re-parse
                CppTokens* raw = cpp_get_scratch(cpp);
                if(!raw) return CPP_OOM_ERROR;
                err = cpp_push_tok(cpp, raw, next);
                if(err){ cpp_release_scratch(cpp, raw); return err; }
                int paren = 1;
                for(;;){
                    next = cpp_ts_next(s);
                    if(next.type == CPP_EOF){
                        cpp_release_scratch(cpp, raw);
                        return cpp_error(cpp, tok.loc, "Unterminated %.*s(", (int)name.length, name.text);
                    }
                    if(next.type == CPP_PUNCTUATOR && next.punct == '(') paren++;
                    else if(next.type == CPP_PUNCTUATOR && next.punct == ')'){
                        if(--paren == 0) break;
                    }
                    err = cpp_push_tok(cpp, raw, next);
                    if(err){ cpp_release_scratch(cpp, raw); return err; }
                }
                CppTokens* expanded = cpp_get_scratch(cpp);
                if(!expanded){ cpp_release_scratch(cpp, raw); return CPP_OOM_ERROR; }
                err = cpp_expand_argument(cpp, raw->data, raw->count, expanded);
                cpp_release_scratch(cpp, raw);
                if(err){ cpp_release_scratch(cpp, expanded); return err; }
                // Find first non-whitespace expanded token
                size_t ei = 0;
                while(ei < expanded->count && expanded->data[ei].type == CPP_WHITESPACE) ei++;
                if(ei >= expanded->count){
                    cpp_release_scratch(cpp, expanded);
                    return cpp_error(cpp, tok.loc, "Empty argument to %.*s", (int)name.length, name.text);
                }
                CppToken etok = expanded->data[ei];
                if(etok.type == CPP_STRING){
                    quote = 1;
                    header_name = (StringView){etok.txt.length - 2, etok.txt.text + 1};
                    cpp_release_scratch(cpp, expanded);
                    IncludePosition dummy3;
                    _Bool result = !is_embed && cpp_find_include(cpp, quote, is_next, header_name, &dummy3) == 0;
                    if(result) msb_reset(fc_path_builder(cpp->fc));
                    *outtok = (CppToken){.type=CPP_NUMBER, .txt = result?SV("1"):SV("0"), .loc = tok.loc};
                    return 0;
                }
                else if(etok.type == CPP_PUNCTUATOR && etok.punct == '<'){
                    quote = 0;
                    MStringBuilder hsb = {.allocator = allocator_from_arena(&cpp->synth_arena)};
                    for(ei++; ei < expanded->count; ei++){
                        CppToken t = expanded->data[ei];
                        if(t.type == CPP_PUNCTUATOR && t.punct == '>')
                            break;
                        msb_write_str(&hsb, t.txt.text, t.txt.length);
                    }
                    if(ei >= expanded->count){
                        cpp_release_scratch(cpp, expanded);
                        msb_destroy(&hsb);
                        return cpp_error(cpp, tok.loc, "Unterminated < in %.*s after expansion", (int)name.length, name.text);
                    }
                    header_name = msb_borrow_sv(&hsb);
                    IncludePosition dummy3;
                    _Bool result = !is_embed && cpp_find_include(cpp, quote, is_next, header_name, &dummy3) == 0;
                    if(result) msb_reset(fc_path_builder(cpp->fc));
                    cpp_release_scratch(cpp, expanded);
                    msb_destroy(&hsb);
                    *outtok = (CppToken){.type=CPP_NUMBER, .txt = result?SV("1"):SV("0"), .loc = tok.loc};
                    return 0;
                }
                else {
                    cpp_release_scratch(cpp, expanded);
                    return cpp_error(cpp, tok.loc, "Expected header name in %.*s after expansion", (int)name.length, name.text);
                }
            }
            do { next = cpp_ts_next(s); } while(next.type == CPP_WHITESPACE);
            if(next.type != CPP_PUNCTUATOR || next.punct != ')')
                return cpp_error(cpp, tok.loc, "Expected ')' after %.*s", (int)name.length, name.text);
            IncludePosition dummy2;
            _Bool result = !is_embed && cpp_find_include(cpp, quote, is_next, header_name, &dummy2) == 0;
            if(result) msb_reset(fc_path_builder(cpp->fc));
            *outtok = (CppToken){.type=CPP_NUMBER, .txt = result?SV("1"):SV("0"), .loc = tok.loc};
            return 0;
        }
        if(sv_equals(name, SV("__has_c_attribute"))){
            // consume (attr) and always return 0 for now
            CppToken next;
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
            *outtok = (CppToken){.type=CPP_NUMBER, .txt = SV("0"), .loc = tok.loc};
            return 0;
        }
        if(sv_equals(name, SV("defined"))){
            CppToken next;
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
            CppToken t = {
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
            *outtok = (CppToken){.type=CPP_NUMBER, .txt = SV("0"), .loc = tok.loc};
            return 0;
        }
        CMacro* m = AM_get(&cpp->macros, a);
        if(!m || m->is_disabled){
            *outtok = (CppToken){.type=CPP_NUMBER, .txt = SV("0"), .loc = tok.loc};
            return 0;
        }
        if(!m->is_function_like){
            err = cpp_expand_obj_macro(cpp, m, tok.loc, s->pending);
            if(err) return err;
            continue;
        }
        // function-like macro: check for '('
        {
            CppToken next;
            do {
                next = cpp_ts_next(s);
            } while(next.type == CPP_WHITESPACE);
            if(next.type != CPP_PUNCTUATOR || next.punct != '('){
                // not an invocation, push back and treat as 0
                err = cpp_push_tok(cpp, s->pending, next);
                if(err) return err;
                *outtok = (CppToken){.type=CPP_NUMBER, .txt = SV("0"), .loc = tok.loc};
                return 0;
            }
            CppTokens *args = cpp_get_scratch(cpp);
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
CppToken
cpp_ts_next(CppTokenStream* s){
    CppToken tok;
    for(;;){
        if(s->pending->count)
            tok = ma_pop_(*s->pending);
        else if(s->cursor < s->count)
            tok = s->toks[s->cursor++];
        else
            tok = (CppToken){0};
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
cpp_eval_parse_number(CPreprocessor* cpp, CppToken tok, int64_t* value){
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
        Uint64Result u = parse_octal_inner(s+1, len-1);
        if(u.errored) return cpp_error(cpp, tok.loc, "Invalid octal digit in number");
        v = u.result;
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
cpp_eval_parse_char(CPreprocessor* cpp, CppToken tok, int64_t* value){
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
cpp_eval_binop_prec(CppToken tok){
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
    CppToken tok;
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
        CppToken tok;
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
            CppToken colon;
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
    CppPragma* prag = AM_get(&cpp->pragmas, a);
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
cpp_mixin_string(CPreprocessor* cpp, SrcLoc loc, StringView str, CppTokens* out){
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
                    t = (unsigned char)((t << 3)|((unsigned char)str.text[i++] - '0'));
                }
                msb_write_char(&sb, t);
                break;
            default:
                return CPP_UNIMPLEMENTED_ERROR;
        }
    }

    CppFrame frame = {
        .txt = sb.cursor?msb_detach_sv(&sb):SV(""),
        .file_id = loc.is_actually_a_pointer?((SrcLocExp*)((uintptr_t)loc.pointer.bits<<1))->file_id:loc.file_id,
        .line = loc.is_actually_a_pointer?((SrcLocExp*)((uintptr_t)loc.pointer.bits<<1))->line:loc.line,
        .column = loc.is_actually_a_pointer?((SrcLocExp*)((uintptr_t)loc.pointer.bits<<1))->column:loc.column,
    };
    for(;;){
        CppToken tok;
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

static
void
cpp_discard_all_input(CPreprocessor* cpp){
    cpp->frames.count = 0;
    cpp->if_stack.count = 0;
    cpp->pending.count = 0;
    cpp->at_line_start = 1;
}

#ifdef __clang__
#pragma clang assume_nonnull end
#endif
#include "../Drp/rng.c"
#endif
