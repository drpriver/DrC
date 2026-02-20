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
#include "C/cc_tok.h"
#include "C/cc_lexer.h"
#include "C/cc_parser.h"
#include "cpp_args.h"
#include "Drp/get_input.h"
#include "cc_repl_completion.h"
#include "C/native_call.h"
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
    CcParser cc_parser = {
        .lexer = {
            .cpp = {
                .allocator = MALLOCATOR,
                .fc = fc,
                .at = &at,
                .logger = logger,
                .target = cc_target_funcs[CC_TARGET_NATIVE](),
                .env = &env,
            },
        },
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
    _Bool repl = 0;
    ArgToParse kw_args[] = {
        {
            .name = SV("-o"),
            .dest = ARGDEST(&output),
            .help = "Where to write to",
            .min_num = 0, .max_num = 1,
        },
        {
            .name = SV("--repl"),
            .dest = ARGDEST(&repl),
            .help = "Start an interactive C REPL.",
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
        .name = argv[0]?argv[0]:"cc",
        .description = "cc",
        .positional.args = pos_args,
        .positional.count = arrlen(pos_args),
        .keyword.args = kw_args,
        .keyword.count = arrlen(kw_args),
        .keyword.next = cpp_kwargs(&cc_parser.lexer.cpp),
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
    // Re-apply target in case --target was given (overrides native default)
    cc_parser.lexer.cpp.target = cc_target_funcs[cc_target_arg]();
    err = cpp_define_builtin_macros(&cc_parser.lexer.cpp);
    if(err) return err;
    if(!cpp_nostdinc)
        err = cpp_setup_default_includes(&cc_parser.lexer.cpp);
    if(err) return err;
    if(repl){
        cc_parser.repl = 1;
        err = cpp_cli_defines(&cc_parser.lexer.cpp);
        if(err) return err;
        fc->may_read_real_files = 1;
        struct ReplCompleterCtx completer_ctx = {.parser = &cc_parser};
        GetInputCtx gi = {
            .tab_completion_func = repl_tab_complete,
            .tab_completion_user_data = &completer_ctx,
        };
        MStringBuilder msb = {.allocator=MALLOCATOR};
        int input_num = 0;
        for(;;){
            gi.prompt = msb.cursor ? SV("... ") : SV("cc> ");
            ssize_t n = gi_get_input(&gi);
            if(n < 0) break; // ctrl-d
            if(n == 0){
                // Empty line = double newline = submit.
                if(!msb.cursor) continue; // nothing to submit
                gi_add_line_to_history_len(&gi, msb.data, msb.cursor);
                msb_write_char(&msb, '\n');
                StringView src = msb_borrow_sv(&msb);
                char name[32];
                int namelen = (snprintf)(name, sizeof name, "(repl:%d)", input_num++);
                fc_write_path(fc, name, namelen);
                err = fc_cache_file(fc, src);
                if(err) return err;
                err = cpp_include_file_via_file_cache(&cc_parser.lexer.cpp, (StringView){namelen, name});
                if(err){
                    log_error(logger, "REPL error");
                    msb.cursor = 0;
                    continue;
                }
                _Bool finished = 0;
                while(!finished){
                    err = cc_parse_top_level(&cc_parser, &finished);
                    if(err) break;
                }
                if(err){
                    log_error(logger, "Parse error");
                    cc_parser_discard_input(&cc_parser);
                }
                msb.cursor = 0;
                continue;
            }
            // Accumulate line into msb.
            if(msb.cursor)
                msb_write_char(&msb, '\n');
            msb_write_str(&msb, gi.buff, n);
            gi_add_line_to_history_len(&gi, gi.buff, n);
        }
        gi_destroy_ctx(&gi);
        ma_cleanup(StringView)(&completer_ctx.ordered, MALLOCATOR);
        msb_destroy(&msb);
        return 0;
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
    err = cpp_cli_defines(&cc_parser.lexer.cpp);
    if(err) return err;
    fc->may_read_real_files = 1;
    err = cpp_include_file_via_file_cache(&cc_parser.lexer.cpp, (StringView){strlen(filename), filename});
    if(err){
        log_error(logger, "Unable to read '%s'", filename);
        return err;
    }
    CcToken tok;
    for(;;){
        err = cc_lex_next_token(&cc_parser.lexer, &tok);
        if(err) continue;
        if(err) return err;
        if(tok.type == CC_EOF) break;
    }
    return 0;
}


#ifdef __clang__
#pragma clang assume_nonnull end
#endif
#include "Drp/Allocators/allocator.c"
#include "Drp/file_cache.c"
#include "C/cpp_preprocessor.c"
#include "C/cc_lexer.c"
#include "C/cc_type_cache.c"
#include "C/cc_scope.c"
#include "C/cc_parser.c"
#include "Drp/get_input.c"
#include "C/native_call.c"
