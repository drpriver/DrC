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
#include "Drp/dre.h"
#ifdef __clang__
#pragma clang assume_nonnull begin
#endif

static _Bool repl_builtin_command(CcParser* parser, StringView input);

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
        .current = &cc_parser.global,
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
    if(!filename && !repl){
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
    if(filename){
        err = cpp_include_file_via_file_cache(&cc_parser.lexer.cpp, (StringView){strlen(filename), filename});
        if(err){
            log_error(logger, "Unable to read '%s'", filename);
            return err;
        }
    }
    _Bool finished = 0;
    while(!finished){
        err = cc_parse_top_level(&cc_parser, &finished);
        if(err) return err;
    }
    if(repl){
        cc_parser.repl = 1;
        err = cpp_cli_defines(&cc_parser.lexer.cpp);
        if(err) return err;
        fc->may_read_real_files = 1;
        struct ReplCompleterCtx completer_ctx = {.parser = &cc_parser};
        GetInputCtx gi = {
            .tab_completion_func = repl_tab_complete,
            .tab_completion_user_data = &completer_ctx,
            .tab_indent_width = 4,
        };
        MStringBuilder msb = {.allocator=MALLOCATOR};
        int input_num = 0;
        for(;;){
            gi.prompt = msb.cursor ? SV("... ") : SV("cc> ");
            ssize_t n = gi_get_input(&gi);
            if(n < 0) break; // ctrl-d
            StringView line = {n, gi.buff};
            line = stripped(line);
            if(!sv_startswith(line, SV("/*"))){ // comment, not command
                if(repl_builtin_command(&cc_parser, line))
                    continue;
            }
            if(n == 0){
                log_sprintf(logger, "\r");
                // Empty line = double newline = submit.
                if(!msb.cursor) continue; // nothing to submit
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
                finished = 0;
                while(!finished){
                    err = cc_parse_top_level(&cc_parser, &finished);
                    if(err) break;
                }
                if(err) cc_parser_discard_input(&cc_parser);
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
    else {
        repl_builtin_command(&cc_parser, SV("/dump symbols"));
    }
    return 0;
}

static void cc_print_type(MStringBuilder*, CcQualType t);
static void cc_print_expr(MStringBuilder*sb, CcExpr* e);

static
_Bool
repl_builtin_command(CcParser* parser, StringView input){
    Logger* l = parser->lexer.cpp.logger;
    input = stripped(input);
    if(!input.length) return 0;
    if(!sv_startswith(input, SV("/"))) return 0;
    input = sv_slice(input, 1, input.length);
    input = stripped(input);
    if(!input.length)
        input = SV("help");
    if(sv_iequals(input, SV("quit")) || sv_iequals(input, SV("exit")) || sv_iequals(input, SV("q"))){
        log_printf(l, "\n");
        exit(0);
    }
    CcScope* scope = &parser->global;
    enum {
        DUMP_NONE     = 0,
        DUMP_STRUCTS  = 0x1,
        DUMP_UNIONS   = 0x2,
        DUMP_ENUMS    = 0x4,
        DUMP_TYPEDEFS = 0x8,
        DUMP_VARS     = 0x10,
        DUMP_FUNCS    = 0x20,
        DUMP_MACROS   = 0x40,
        DUMP_TYPES = DUMP_STRUCTS|DUMP_UNIONS|DUMP_ENUMS|DUMP_TYPEDEFS,
        DUMP_SYMBOLS = DUMP_TYPES|DUMP_VARS|DUMP_FUNCS,
        DUMP_ALL = DUMP_SYMBOLS | DUMP_MACROS,
    } dump = DUMP_NONE;

    if(sv_iequals(input, SV("help")) || sv_equals(input, SV("?"))){
        log_printf(l, "[regex] in a command filters the output.\n"
             "The regex matches the entire symbol name, so pad with .* if you need it.\n"
             "REPL commands:\n"
             "  help                  - show this message\n"
             "  dump types    [regex] - dump typedefs, structs, unions, enums\n"
             "  dump typedefs [regex] - dump typedefs\n"
             "  dump structs  [regex] - dump struct tags (detailed)\n"
             "  dump unions   [regex] - dump union tags (detailed)\n"
             "  dump enums    [regex] - dump enum tags (detailed)\n"
             "  dump vars     [regex] - dump global variables\n"
             "  dump funcs    [regex] - dump global functions\n"
             "  dump macros   [regex] - dump preprocessor macros\n"
             "  dump symbols  [regex] - dump everything but macros\n"
             "  dump          [regex] - dump everything\n"
             "  dump all      [regex] - dump everything\n"
        );
        return 1;
    }
    StringView tail = {0};
    sv_split1(input, ' ', &input, &tail);
    if(!sv_iequals(input, SV("dump")) && !sv_iequals(input, SV("d")))
        return 1;
    input = stripped(tail);
    StringView re = {0};
    sv_split1(input, ' ', &input, &tail);
    re = stripped(tail);
    if(sv_iequals(input, SV("all")))
        dump = DUMP_ALL;
    if(sv_iequals(input, SV("symbols")))
        dump = DUMP_SYMBOLS;
    else if(sv_iequals(input, SV("types")) || sv_iequals(input, SV("t")))
        dump = DUMP_TYPES;
    else if(sv_iequals(input, SV("typedefs")))
        dump = DUMP_TYPEDEFS;
    else if(sv_iequals(input, SV("structs")))
        dump = DUMP_STRUCTS;
    else if(sv_iequals(input, SV("unions")))
        dump = DUMP_UNIONS;
    else if(sv_iequals(input, SV("enums")))
        dump = DUMP_ENUMS;
    else if(sv_iequals(input, SV("vars")) || sv_iequals(input, SV("v")))
        dump = DUMP_VARS;
    else if(sv_iequals(input, SV("funcs")) || sv_iequals(input, SV("f")))
        dump = DUMP_FUNCS;
    else if(sv_iequals(input, SV("macros")) || sv_iequals(input, SV("m")))
        dump = DUMP_MACROS;
    else {
        dump = DUMP_ALL;
        re = stripped(input);
    }
    if(dump & DUMP_STRUCTS){
        AtomMapItems mi = AM_items(&scope->structs);
        if(mi.count){
            _Bool detailed = dump == DUMP_STRUCTS;
            log_sprintf(l, "Structs (%zu):\n", mi.count);
            for(size_t i = 0; i < mi.count; i++){
                Atom a = mi.data[i].atom;
                if(re.length && !dre_match_entire(re.text, re.length, a->data, a->length))
                    continue;
                CcStruct* s = mi.data[i].p;
                if(s->is_incomplete){
                    log_sprintf(l, "  struct %.*s (incomplete)\n", (int)a->length, a->data);
                    continue;
                }
                log_sprintf(l, "  struct %.*s (%u bytes, align %u, %u fields)\n",
                    (int)a->length, a->data, s->size, s->alignment, s->field_count);
                if(detailed){
                    for(uint32_t j = 0; j < s->field_count; j++){
                        CcField* f = &s->fields[j];
                        if(f->name)
                            log_sprintf(l, "    .%.*s (offset %u)\n",
                                (int)f->name->length, f->name->data, f->offset);
                        else
                            log_sprintf(l, "    <anonymous> (offset %u)\n", f->offset);
                    }
                }
            }
        }
    }
    if(dump & DUMP_UNIONS){
        AtomMapItems mi = AM_items(&scope->unions);
        if(mi.count){
            _Bool detailed = dump == DUMP_UNIONS;
            log_sprintf(l, "Unions (%zu):\n", mi.count);
            for(size_t i = 0; i < mi.count; i++){
                Atom a = mi.data[i].atom;
                if(re.length && !dre_match_entire(re.text, re.length, a->data, a->length))
                    continue;
                CcUnion* u = mi.data[i].p;
                if(u->is_incomplete){
                    log_sprintf(l, "  union %.*s (incomplete)\n", (int)a->length, a->data);
                    continue;
                }
                log_sprintf(l, "  union %.*s (%u bytes, align %u, %u fields)\n",
                    (int)a->length, a->data, u->size, u->alignment, u->field_count);
                if(detailed){
                    for(uint32_t j = 0; j < u->field_count; j++){
                        CcField* f = &u->fields[j];
                        if(f->name)
                            log_sprintf(l, "    .%.*s\n", (int)f->name->length, f->name->data);
                        else
                            log_sprintf(l, "    <anonymous>\n");
                    }
                }
            }
        }
    }
    if(dump & DUMP_ENUMS){
        AtomMapItems mi = AM_items(&scope->enums);
        if(mi.count){
            _Bool detailed = dump == DUMP_ENUMS;
            log_sprintf(l, "Enums (%zu):\n", mi.count);
            for(size_t i = 0; i < mi.count; i++){
                Atom a = mi.data[i].atom;
                if(re.length && !dre_match_entire(re.text, re.length, a->data, a->length))
                    continue;
                CcEnum* e = mi.data[i].p;
                if(e->is_incomplete){
                    log_sprintf(l, "  enum %.*s (incomplete)\n", (int)a->length, a->data);
                    continue;
                }
                log_sprintf(l, "  enum %.*s (%zu enumerators)\n", (int)a->length, a->data, e->enumerator_count);
                if(detailed){
                    for(uint32_t j = 0; j < e->enumerator_count; j++)
                        log_sprintf(l, "    %.*s = %lld\n", (int)e->enumerators[j]->name->length, e->enumerators[j]->name->data, (long long)e->enumerators[j]->value);
                }
            }
        }
    }
    if(dump & DUMP_TYPEDEFS){
        AtomMapItems mi = AM_items(&scope->typedefs);
        if(mi.count){
            log_sprintf(l, "Typedefs (%zu):\n", mi.count);
            for(size_t i = 0; i < mi.count; i++){
                Atom a = mi.data[i].atom;
                if(re.length && !dre_match_entire(re.text, re.length, a->data, a->length))
                    continue;
                log_sprintf(l, "  %.*s: ", (int)a->length, a->data);
                CcQualType t = {.bits = (uintptr_t)mi.data[i].p};
                cc_print_type(&l->buff, t);
                log_sprintf(l, "\n");
            }
        }
    }
    if(dump & DUMP_VARS){
        AtomMapItems mi = AM_items(&scope->variables);
        log_sprintf(l, "Variables (%zu):\n", mi.count);
        for(size_t i = 0; i < mi.count; i++){
            Atom a = mi.data[i].atom;
            if(re.length && !dre_match_entire(re.text, re.length, a->data, a->length))
                continue;
            CcVariable* v = mi.data[i].p;
            log_sprintf(l, "  %.*s: ", (int)a->length, a->data);
            cc_print_type(&l->buff, v->type);
            if(v->mangle) log_sprintf(l, " asm(\"%.*s\")", (int)v->mangle->length, v->mangle->data);
            if(v->extern_) log_sprintf(l, " (extern)");
            if(v->static_) log_sprintf(l, " (static)");
            if(v->initializer){
                CcExpr* init = v->initializer;
                log_sprintf(l, " = ");
                cc_print_expr(&l->buff, init);
            }
            log_sprintf(l, "\n");
        }
    }
    if(dump & DUMP_FUNCS){
        AtomMapItems mi = AM_items(&scope->functions);
        log_sprintf(l, "Functions (%zu):\n", mi.count);
        for(size_t i = 0; i < mi.count; i++){
            Atom a = mi.data[i].atom;
            if(re.length && !dre_match_entire(re.text, re.length, a->data, a->length))
                continue;
            CcFunc* fn = mi.data[i].p;
            log_sprintf(l, "  %.*s(", (int)a->length, a->data);
            CcFunction* ft = fn->type;
            for(uint32_t j = 0; j < ft->param_count; j++){
                if(j) log_sprintf(l, ", ");
                cc_print_type(&l->buff, ft->params[j]);
                if(fn->params.data && j < fn->params.count && fn->params.data[j]){
                    log_sprintf(l, " %s", fn->params.data[j]->data);
                }
            }
            if(ft->is_variadic){
                if(ft->param_count) log_sprintf(l, ", ");
                log_sprintf(l, "...");
            }
            log_sprintf(l, ") -> ");
            cc_print_type(&l->buff, ft->return_type);
            if(fn->mangle) log_sprintf(l, " asm(\"%.*s\")", (int)fn->mangle->length, fn->mangle->data);
            if(fn->defined) log_sprintf(l, " [defined]");
            log_sprintf(l, "\n");
        }
    }
    if(dump & DUMP_MACROS){
        AtomMapItems mi = AM_items(&parser->lexer.cpp.macros);
        log_sprintf(l, "Macros (%zu):\n", mi.count);
        for(size_t i = 0; i < mi.count; i++){
            Atom a = mi.data[i].atom;
            if(!mi.data[i].p) continue;
            StringView mre = re.length?re:SV("[^_].*");
            if(mre.length && !dre_match_entire(mre.text, mre.length, a->data, a->length))
                continue;
            log_sprintf(l, "  %.*s\n", (int)a->length, a->data);
        }
    }
    if(msb_peek(&l->buff) != '\n')
        msb_write_char(&l->buff, '\n');
    log_flush(l, LOG_PRINT);
    return 1;
}

#ifdef __clang__
#pragma clang assume_nonnull end
#endif
#include "Drp/Allocators/allocator.c"
#include "Drp/file_cache.c"
#include "C/cpp_preprocessor.c"
#include "C/cc_lexer.c"
#include "C/cc_parser.c"
#include "Drp/get_input.c"
#include "C/native_call.c"
#include "Drp/dre.c"
