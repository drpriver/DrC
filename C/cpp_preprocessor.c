#ifndef C_CPP_PREPROCESSOR_C
#define C_CPP_PREPROCESSOR_C
//
// Copyright © 2026-2026, David Priver <david@davidpriver.com>
//
#include "cpp_preprocessor.h"
#ifdef __clang__
#pragma clang assume_nonnull begin
#endif

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
    (void)cpp;
    *tok = (CPPToken){
        .type = CPP_EOF,
    };
    return 0;
}

#ifdef __clang__
#pragma clang assume_nonnull end
#endif
#endif
