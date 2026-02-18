#ifndef CPP_ARGS_H
#define CPP_ARGS_H
//
// Copyright © 2026-2026, David Priver <david@davidpriver.com>
//
// For handling common cli args for the CPP
// This is not thread-safe, but it should basically only be used from main, so who cares.
//
#include "Drp/argument_parsing.h"
#include "C/cpp_preprocessor.h"
#ifndef __clang__
#pragma clang assume_nonnull begin
#endif

static
int
ma_sv_appender(ArgToParse* ap, const void* arg){
    CPreprocessor* cpp = ap->dest.user_pointer->user_data;
    Marray(StringView)* dst = ap->dest.pointer;
    const StringView* sv = arg;
    int err = ma_push(StringView)(dst, cpp->allocator, *sv);
    return err;
}

static
ArgParseDestination
cpp_ma_sv_dest(const ArgParseUserDefinedType* t, Marray(StringView)*dst){
    ArgParseDestination dest = {
        .type = ARG_USER_DEFINED,
        .pointer = dst,
        .user_pointer = t,
    };
    return dest;
}
static
int
cli_macro(ArgToParse* ap, const void* arg){
    MStringBuilder* sb = ap->dest.user_pointer->user_data;
    const StringView* sv = arg;
    const char* eq = memchr(sv->text, '=', sv->length);
    StringView name = eq?(StringView){eq-sv->text, sv->text}:*sv;
    if(!name.length)
        return ARGPARSE_CONVERSION_ERROR;
    msb_write_literal(sb, "#define ");
    msb_write_str(sb, name.text, name.length);
    if(eq){
        msb_write_char(sb, ' ');
        msb_write_str(sb, eq+1, sv->text+sv->length-eq-1);
        msb_write_char(sb, '\n');
    }
    else
        msb_write_literal(sb, " 1\n");
    return sb->errored?ARGPARSE_INTERNAL_ERROR:0;
}

static
ArgParseDestination
cpp_macro_dest(MStringBuilder* sb){
    static _Bool init;
    static ArgParseUserDefinedType t;
    if(!init){
        t = (ArgParseUserDefinedType){
            .type_name = SV("macro_def"),
            .user_data = sb,
        };
        init = 1;
    }
    ArgParseDestination dest = {
        .type = ARG_USER_DEFINED,
        .pointer = sb,
        .user_pointer = &t,
    };
    return dest;
}

static MStringBuilder cli_macros = {.allocator=MALLOCATOR};
static
ArgParseKwParams*
cpp_kwargs(CPreprocessor* cpp){
    static ArgParseUserDefinedType t = {
        .type_name = SV("path"),
    };
    t.user_data = cpp;
    static ArgToParse kw_args[] = {
        [0] = {
            .name = SV("-I"),
            .append_proc = ma_sv_appender,
            .help = "Extra include paths.",
            .max_num = 1000,
            .one_at_a_time = 1,
            .space_sep_is_optional = 1,
        },
        [1] = {
            .name = SV("-isystem"),
            .append_proc = ma_sv_appender,
            .help = "Extra system include paths.",
            .max_num = 1000,
            .one_at_a_time = 1,
            .space_sep_is_optional = 1,
        },
        [2] = {
            .name = SV("-iquote"),
            .append_proc = ma_sv_appender,
            .help = "Extra quote include paths.",
            .max_num = 1000,
            .one_at_a_time = 1,
            .space_sep_is_optional = 1,
        },
        [3] = {
            .name = SV("-idirafter"),
            .append_proc = ma_sv_appender,
            .help = "Extra dirafter include paths.",
            .max_num = 1000,
            .one_at_a_time = 1,
            .space_sep_is_optional = 1,
        },
        [4] = {
            .name = SV("-F"),
            .append_proc = ma_sv_appender,
            .help = "Extra framework paths.",
            .max_num = 1000,
            .one_at_a_time = 1,
            .space_sep_is_optional = 1,
            #ifndef __APPLE__
            .hidden = 1,
            #endif
        },
        [5] = {
            .name = SV("-D"),
            .append_proc = cli_macro,
            .help = "Predefined macros.",
            .max_num=1000,
            .one_at_a_time=1,
            .space_sep_is_optional=1,
        },
    };
    kw_args[0].dest = cpp_ma_sv_dest(&t, &cpp->Ipaths);
    kw_args[1].dest = cpp_ma_sv_dest(&t, &cpp->isystem_paths);
    kw_args[2].dest = cpp_ma_sv_dest(&t, &cpp->iquote_paths);
    kw_args[3].dest = cpp_ma_sv_dest(&t, &cpp->idirafter_paths);
    kw_args[4].dest = cpp_ma_sv_dest(&t, &cpp->framework_paths);
    kw_args[5].dest = cpp_macro_dest(&cli_macros);
    static ArgParseKwParams kwargs = {
        .args = kw_args,
        .count = sizeof kw_args / sizeof kw_args[0],
    };

    return &kwargs;
}

static
int
cpp_cli_defines(CPreprocessor* cpp){
    int err = 0;
    if(cli_macros.cursor){
        fc_write_path(cpp->fc, "(command line)", sizeof "(command line)" -1);
        err = fc_cache_file(cpp->fc, msb_borrow_sv(&cli_macros));
        if(err) return err;
        err = cpp_include_file_via_file_cache(cpp, SV("(command line)"));
        if(err) return err;
        CPPToken tok;
        for(;;){
            err = cpp_next_pp_token(cpp, &tok);
            if(err) return err;
            if(tok.type == CPP_EOF) break;
        }
    }
    return err;
}

#ifndef __clang__
#pragma clang assume_nonnull end
#endif
#endif
