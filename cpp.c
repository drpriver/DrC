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
#ifdef __clang__
#pragma clang assume_nonnull begin
#endif
int main(int argc, char** argv, char** envp){
    Logger logger = std_logger();
    AtomTable at = {.allocator=MALLOCATOR};
    Environment env = {.allocator=MALLOCATOR, .at=&at};
    int err = env_parse_posix(&env, envp);
    if(err){
        log_error(&logger, "Unable to parse environment");
        return 1;
    }
    const char* filename = NULL;
    const char* extra_include_paths[32] = {0};
    ArgToParse pos_args[] = {
        {
            .name = SV("file"),
            .dest = ARGDEST(&filename),
            .help = "The file to preprocess.",
            .min_num = 1, .max_num = 1,
        }
    };
    ArgToParse kw_args[] = {
        {
            .name = SV("-I"),
            .dest = ARGDEST(extra_include_paths),
            .max_num = arrlen(extra_include_paths),
            .help = "Extra include paths.",
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
    enum ArgParseError parse_err = parse_args(&parser, &args, 0);
    if(parse_err){
        print_argparse_error(&parser, parse_err);
        return 1;
    }
    FileCache* fc = fc_create(MALLOCATOR);
    StringView this[10] = {0};
    for(int i = 0; i < 10; i++){
        fc_write_path(fc, __FILE__, sizeof __FILE__ - 1);
        err = fc_read_file(fc, this+i);
        if(err){
            log_error(&logger, "Unable to read %s: %d (%s)", __FILE__, err, strerror(err));
            return err;
        }
        if(!sv_equals(this[0], this[i])){
            log_error(&logger, "uhh... %d", i);
            return 1;
        }
        if(this[0].text != this[i].text){
            log_error(&logger, "uhh... %d", i);
            return 1;
        }
    }
    // log_printf(&logger, "%.*s", (int)this[0].length, this[0].text);
    fc_destroy(fc);
    return 0;
}


#ifdef __clang__
#pragma clang assume_nonnull end
#endif
#include "Drp/Allocators/allocator.c"
#include "Drp/file_cache.c"
