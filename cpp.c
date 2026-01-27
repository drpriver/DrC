//
// Copyright © 2026-2026, David Priver <david@davidpriver.com>
//
#define STB_SPRINTF_STATIC
#define STB_SPRINTF_IMPLEMENTATION
#include "Drp/compiler_warnings.h"
#include "Drp/argument_parsing.h"
#include "Drp/env.h"
#include "Drp/Allocators/mallocator.h"
#include "Drp/stdlogger.h"
#include "Drp/atom_table.h"
#include "Drp/term_util.h"
#include "Drp/file_cache.h"
#include "c/pp_tok.h"
#include "c/pp_preprocessor.h"
#ifdef __clang__
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



int main(int argc, char** argv, char** envp){
    Logger logger = std_logger();
    AtomTable at = {.allocator=MALLOCATOR};
    Environment env = {.allocator=MALLOCATOR, .at=&at};
    FileCache* fc = fc_create(MALLOCATOR);

    int err = env_parse_posix(&env, envp);
    if(err){
        log_error(&logger, "Unable to parse environment");
        return 1;
    }
    CPreprocessor cpp = {
        .allocator = MALLOCATOR,
        .fc = fc,
        .at = &at,
        .logger = &logger,
        .env = &env,
    };
    ArgParseUserDefinedType t = {
        .type_name = SV("path"),
        .user_data = &cpp,
    };
    const char* filename = NULL;
    ArgToParse pos_args[] = {
        {
            .name = SV("file"),
            .dest = ARGDEST(&filename),
            .help = "The file to preprocess.",
            .min_num = 0, .max_num = 1,
        }
    };
    ArgToParse kw_args[] = {
        {
            .name = SV("-I"),
            .dest = cpp_ma_sv_dest(&t, &cpp.Ipaths),
            .append_proc = ma_sv_appender,
            .help = "Extra include paths.",
            .max_num = 1000,
            .one_at_a_time = 1,
            .space_sep_is_optional = 1,
        },
        {
            .name = SV("-isystem"),
            .dest = cpp_ma_sv_dest(&t, &cpp.isystem_paths),
            .append_proc = ma_sv_appender,
            .help = "Extra system include paths.",
            .max_num = 1000,
            .one_at_a_time = 1,
            .space_sep_is_optional = 1,
        },
        {
            .name = SV("-iquote"),
            .dest = cpp_ma_sv_dest(&t, &cpp.iquote_paths),
            .append_proc = ma_sv_appender,
            .help = "Extra quote include paths.",
            .max_num = 1000,
            .one_at_a_time = 1,
            .space_sep_is_optional = 1,
        },
        {
            .name = SV("-idirafter"),
            .dest = cpp_ma_sv_dest(&t, &cpp.idirafter_paths),
            .append_proc = ma_sv_appender,
            .help = "Extra dirafter include paths.",
            .max_num = 1000,
            .one_at_a_time = 1,
            .space_sep_is_optional = 1,
        },
        {
            .name = SV("-F"),
            .dest = cpp_ma_sv_dest(&t, &cpp.framework_paths),
            .append_proc = ma_sv_appender,
            .help = "Extra framework paths.",
            .max_num = 1000,
            .one_at_a_time = 1,
            .space_sep_is_optional = 1,
        },
    };
    enum {HELP, HIDDEN_HELP, FISH};
    ArgToParse early_args[] = {
        [HELP] = {
            .name = SV("-h"),
            .altname1 = SV("--help"),
            .help = "Print this help and exit.",
        },
        [HIDDEN_HELP] = {
            .name = SV("-H"),
            .altname1 = SV("--hidden-help"),
            .help = "Print out help for the hidden arguments and exit.",
        },
        [FISH] = {
            .name = SV("--fish-completions"),
            .help = "Print out commands for fish shell completions.",
            .hidden = 1,
        },
    };
    ArgParser parser = {
        .name = argv[0]?argv[0]:"cpp",
        .description = "cpp",
        .positional.args = pos_args,
        .positional.count = arrlen(pos_args),
        .keyword.args = kw_args,
        .keyword.count = arrlen(kw_args),
        .early_out.args = early_args,
        .early_out.count = arrlen(early_args),
        .styling.plain = !stdout_is_terminal(),
    };
    Args args = argc?(Args){argc-1, (const char*const*)argv+1}:(Args){0, 0};
    switch(check_for_early_out_args(&parser, &args)){
        case HELP:{
            print_argparse_help(&parser, get_terminal_size().columns);
            fflush(stdout);
            return 0;
        }
        case HIDDEN_HELP:{
            print_argparse_hidden_help(&parser, get_terminal_size().columns);
            fflush(stdout);
            return 0;
        }
        case FISH:{
            print_argparse_fish_completions(&parser);
            fflush(stdout);
            return 0;
        }
        default:
            break;
    }
    enum ArgParseError parse_err = parse_args(&parser, &args, ARGPARSE_FLAGS_ALLOW_KWARG_SEP_TO_BE_OPTIONAL);
    if(parse_err){
        print_argparse_error(&parser, parse_err);
        return 1;
    }
    MARRAY_FOR_EACH(StringView, p, cpp.iquote_paths){
        log_printf(&logger, "-iquote %zd) %.*s", p-cpp.iquote_paths.data, (int)p->length, p->text);
    }
    MARRAY_FOR_EACH(StringView, p, cpp.Ipaths){
        log_printf(&logger, "-I %zd) %.*s", p-cpp.Ipaths.data, (int)p->length, p->text);
    }
    MARRAY_FOR_EACH(StringView, p, cpp.isystem_paths){
        log_printf(&logger, "-isystem %zd) %.*s", p-cpp.isystem_paths.data, (int)p->length, p->text);
    }
    MARRAY_FOR_EACH(StringView, p, cpp.istandard_system_paths){
        log_printf(&logger, "standard_system_paths %zd) %.*s", p-cpp.istandard_system_paths.data, (int)p->length, p->text);
    }
    MARRAY_FOR_EACH(StringView, p, cpp.idirafter_paths){
        log_printf(&logger, "-idirafter %zd) %.*s", p-cpp.idirafter_paths.data, (int)p->length, p->text);
    }
    MARRAY_FOR_EACH(StringView, p, cpp.framework_paths){
        log_printf(&logger, "-F %zd) %.*s", p-cpp.framework_paths.data, (int)p->length, p->text);
    }
    log_printf(&logger, "has_include(\"%s\") : %s", filename, cpp_has_include(&cpp, 1, (StringView){strlen(filename), filename})?"true":"false");
    log_printf(&logger, "has_include(<%s>) : %s", filename, cpp_has_include(&cpp, 0, (StringView){strlen(filename), filename})?"true":"false");

    return 0;
}


#ifdef __clang__
#pragma clang assume_nonnull end
#endif
#include "Drp/Allocators/allocator.c"
#include "Drp/file_cache.c"
