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
#include "Drp/file_util.h"
#include "C/cpp_tok.h"
#include "C/cpp_preprocessor.h"
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


static int cpp_next_pp_token(CPreprocessor* cpp, CPPToken* ptok);

int main(int argc, char** argv, char** envp){
    MStringBuilder cli_macros = {.allocator=MALLOCATOR};
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
    err = cpp_define_builtin_macros(&cpp);
    if(err) return err;
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
        {
            .name = SV("-D"),
            .dest = cpp_macro_dest(&cli_macros),
            .append_proc = cli_macro,
            .help = "Predefined macros.",
            .max_num=1000,
            .one_at_a_time=1,
            .space_sep_is_optional=1,
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
    if(!filename){
        LongString txt;
        #ifdef _WIN32
        HANDLE handle = GetStdHandle(STD_INPUT_HANDLE);
        #else
        int handle = STDIN_FILENO;
        #endif
        FileError fe = read_file_handle(handle, MALLOCATOR, &txt);
        if(fe.errored)
            return 1;
        filename = "(stdin)";
        fc_write_path(fc, filename, strlen(filename));
        err = fc_cache_file(fc, LS_to_SV(txt));
        Allocator_free(MALLOCATOR, txt.text, txt.length+1);
        if(err) return err;
    }
    if(cli_macros.cursor){
        fc_write_path(fc, "(command line)", sizeof "(command line)" -1);
        err = fc_cache_file(fc, msb_borrow_sv(&cli_macros));
        if(err) return err;
        err = cpp_include_file_via_file_cache(&cpp, SV("(command line)"));
        if(err) return err;
        CPPToken tok;
        for(;;){
            err = cpp_next_pp_token(&cpp, &tok);
            if(err) return err;
            if(tok.type == CPP_EOF) break;
        }
    }
    fc->may_read_real_files = 1;
    err = cpp_include_file_via_file_cache(&cpp, (StringView){strlen(filename), filename});
    if(err){
        log_error(&logger, "Unable to read '%s'", filename);
        return err;
    }
    CPPToken tok;
    if(1){
        for(;;){
            err = cpp_next_pp_token(&cpp, &tok);
            if(err) return err;
            if(tok.type == CPP_EOF) break;
            if(tok.type == CPP_WHITESPACE)
                log_sprintf(&logger, " ");
            else log_sprintf(&logger, "%.*s", sv_p(tok.txt));
        }
        log_flush(&logger, LOG_PRINT);
    }
    else
    for(;;){
        err = cpp_next_token(&cpp, &tok);
        if(err) return err;
        if(tok.type == CPP_EOF) break;
        int line = tok.loc.line;
        int col = tok.loc.column;
        LongString file = fc->map.data[tok.loc.file_id].path;
        if(tok.loc.is_actually_a_pointer){
        }
        if(tok.type == CPP_NEWLINE)
            log_printf(&logger, "%s:%d:%d %.*s '\\n'", file.text, line, col, sv_p(CPPTokenTypeSV[tok.type]));
        else if(tok.type == CPP_WHITESPACE)
            log_printf(&logger, "%s:%d:%d %.*s ' '", file.text, line, col, sv_p(CPPTokenTypeSV[tok.type]));
        else log_printf(&logger, "%s:%d:%d %.*s '%.*s'", file.text, line, col, sv_p(CPPTokenTypeSV[tok.type]), sv_p(tok.txt));
    }
    // ti_print_logger(&cpp, &TI_CPreprocessor.type_info, &logger);
    return 0;
}


#ifdef __clang__
#pragma clang assume_nonnull end
#endif
#include "Drp/Allocators/allocator.c"
#include "Drp/file_cache.c"
#include "C/cpp_preprocessor.c"
