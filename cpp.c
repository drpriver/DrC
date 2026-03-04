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
#include "cpp_args.h"
#ifdef __clang__
#pragma clang assume_nonnull begin
#endif

int main(int argc, char** argv, char** envp){
    Logger* logger = std_logger();
    if(!logger) return 1;
    AtomTable at = {.allocator=MALLOCATOR};
    Environment env = {.allocator=MALLOCATOR, .at=&at};
    FileCache* fc = fc_create(MALLOCATOR);

    int err = env_parse_posix(&env, envp);
    if(err){
        log_error(logger, "Unable to parse environment");
        return 1;
    }
    CppPreprocessor cpp = {
        .allocator = MALLOCATOR,
        .fc = fc,
        .at = &at,
        .logger = logger,
        .env = &env,
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
    StringView output = {0};
    ArgToParse kw_args[] = {
        {
            .name = SV("-o"),
            .dest = ARGDEST(&output),
            .help = "Where to output the preprocessed file to",
            .min_num = 0, .max_num = 1,
        }
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
        .keyword.next = cpp_kwargs(&cpp),
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
    cpp.target = cc_target_funcs[cc_target_arg]();
    err = cpp_define_builtin_macros(&cpp);
    if(err) return err;
    if(!cpp_nostdinc){
        err = cpp_setup_default_includes(&cpp);
        if(err) return err;
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
    err = cpp_cli_defines(&cpp);
    if(err) return err;
    fc->may_read_real_files = 1;
    err = cpp_include_file_via_file_cache(&cpp, (StringView){strlen(filename), filename});
    if(err){
        log_error(logger, "Unable to read '%s'", filename);
        return err;
    }
    CppToken tok;
    MStringBuilder sb = {.allocator = MALLOCATOR};
    int blank_line = 1;
    for(;;){
        err = cpp_next_pp_token(&cpp, &tok);
        if(err) return err;
        if(tok.type == CPP_EOF) break;
        if(tok.type == CPP_NEWLINE){
            if(blank_line) continue;
            blank_line = 1;
            while(msb_peek(&sb) == ' ')
                msb_erase(&sb, 1);
            msb_write_char(&sb, '\n');
        }
        else if(tok.type == CPP_WHITESPACE){
            msb_write_char(&sb, ' ');
        }
        else {
            blank_line = 0;
            msb_write_str(&sb, tok.txt.text, tok.txt.length);
        }
    }
    if(output.length){
        FileError fe = write_file(output.text, sb.data, sb.cursor);
        if(fe.errored){
            log_error(logger, "Failed to write to '%s'", output.text);
            return fe.errored;
        }
    }
    else
        log_log(logger, LOG_PRINT, sb.data, sb.cursor);
    msb_destroy(&sb);
    return 0;
}


#ifdef __clang__
#pragma clang assume_nonnull end
#endif
#include "Drp/Allocators/allocator.c"
#include "Drp/file_cache.c"
#include "C/cpp_preprocessor.c"
