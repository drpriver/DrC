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
#include "Drp/logger_ti_printer.h"
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
    CPreprocessor* cpp = ap->dest.user_pointer->user_data;
    const StringView* sv = arg;
    const char* eq = memchr(sv->text, '=', sv->length);
    StringView name = eq?(StringView){eq-sv->text, sv->text}:*sv;
    CPPToken tok = {
        .type = CPP_IDENTIFIER,
        .txt = eq?(StringView){sv->text+sv->length-eq-1, eq+1}: SV("1"),
    };
    int err = cpp_define_obj_macro(cpp, name, &tok, 1);
    return err;
}

static
ArgParseDestination
cpp_macro_dest(CPreprocessor* cpp){
    static _Bool init;
    static ArgParseUserDefinedType t;
    if(!init){
        t = (ArgParseUserDefinedType){
            .type_name = SV("macro_def"),
            .user_data = cpp,
        };
        init = 1;
    }
    ArgParseDestination dest = {
        .type = ARG_USER_DEFINED,
        .pointer = cpp,
        .user_pointer = &t,
    };
    return dest;
}


static int cpp_next_pp_token(CPreprocessor* cpp, CPPToken* ptok);

int main(int argc, char** argv, char** envp){
    Logger logger = std_logger();
    AtomTable at = {.allocator=MALLOCATOR};
    {
        int err = register_type_atoms(&at);
        if(err) return 1;
    }
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
        {
            .name = SV("-D"),
            .dest = cpp_macro_dest(&cpp),
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
    struct TypeInfoMarray TI_ma_StringView = {
        .name = (Atom)AT_atomize(&at, "StringView", sizeof "StringView" - 1),
        .size = sizeof(Marray(StringView)),
        .align = _Alignof(Marray(StringView)),
        .kind = TIK_MARRAY,
        .type = &TI_SV.type_info,
    };
    struct {
        union { TypeInfo type_info; struct { STRUCTINFO; }; };
        MemberInfo members[4];
    } TI_CPPToken = {
        .name = (Atom)AT_atomize(&at, "CPPToken", sizeof "CPPToken" -1),
        .size = sizeof(CPPToken),
        .align = _Alignof(CPPToken),
        .kind = TIK_STRUCT,
        .length = arrlen(TI_CPPToken.members),
        .members = {
            {
                .name = (Atom)AT_atomize(&at, "type", sizeof "type" -1),
                .type = &TI_uint64_t.type_info,
                .bitfield = {
                    .kind = MK_BITFIELD,
                    .offset = offsetof(CPPToken, _bits),
                    .bitsize = 4,
                    .bitoffset = 0,
                },
            },
            {
                .name = (Atom)AT_atomize(&at, "disabled", sizeof "disabled" -1),
                .type = &TI_uint64_t.type_info,
                .bitfield = {
                    .kind = MK_BITFIELD,
                    .offset = offsetof(CPPToken, _bits),
                    .bitsize = 1,
                    .bitoffset = 8,
                },
            },
            {
                .name = (Atom)AT_atomize(&at, "punct", sizeof "punct" -1),
                .type = &TI_uint64_t.type_info,
                .bitfield = {
                    .kind = MK_BITFIELD,
                    .offset = offsetof(CPPToken, _bits),
                    .bitsize = 32,
                    .bitoffset = 32,
                },
            },
            {
                .name = (Atom)AT_atomize(&at, "txt", sizeof "txt" -1),
                .type = &TI_SV.type_info,
                .offset = offsetof(CPPToken, txt),
            },
        },
    };
    struct TypeInfoMarray TI_ma_CPPToken = {
        .name = (Atom)AT_atomize(&at, "CPPToken", sizeof "CPPToken" - 1),
        .size = sizeof(Marray(CPPToken)),
        .align = _Alignof(Marray(CPPToken)),
        .kind = TIK_MARRAY,
        .type = &TI_CPPToken.type_info,
    };
    struct {
        union { TypeInfo type_info; struct { STRUCTINFO; }; };
        MemberInfo members[3];
    } TI_CPPFrame = {
        .name = (Atom)AT_atomize(&at, "CPPFrame", sizeof "CPPFrame" -1),
        .size = sizeof(CPPFrame),
        .align = _Alignof(CPPFrame),
        .kind = TIK_STRUCT,
        .length = arrlen(TI_CPPFrame.members),
        .members = {
            {
                .name = (Atom)AT_atomize(&at, "file_id", sizeof "file_id" -1),
                .type = &TI_uint32_t.type_info,
                .offset = offsetof(CPPFrame, file_id),
            },
            {
                .name = (Atom)AT_atomize(&at, "txt", sizeof "txt" -1),
                .type = &TI_SV.type_info,
                .offset = offsetof(CPPFrame, txt),
                .noprint=1,
            },
            {
                .name = (Atom)AT_atomize(&at, "cursor", sizeof "cursor" -1),
                .type = &TI_size_t.type_info,
                .offset = offsetof(CPPFrame, cursor),
            },
        },
    };
    struct TypeInfoMarray TI_ma_CPPFrame = {
        .name = (Atom)AT_atomize(&at, "CPPFrame", sizeof "CPPFrame" - 1),
        .size = sizeof(Marray(CPPFrame)),
        .align = _Alignof(Marray(CPPFrame)),
        .kind = TIK_MARRAY,
        .type = &TI_CPPFrame.type_info,
    };
    struct {
        union { TypeInfo type_info; struct { STRUCTINFO; }; };
        MemberInfo members[7];
    } TI_CMacro = {
        .name = (Atom)AT_atomize(&at, "CMacro", sizeof "CMacro" -1),
        .size = sizeof(CMacro),
        .align = _Alignof(CMacro),
        .kind = TIK_STRUCT,
        .length = arrlen(TI_CMacro.members),
        .is_dynamically_sized = 1,
        .members = {
            [0] = {
                .name = (Atom)AT_atomize(&at, "is_function_like", sizeof "is_function_like" -1),
                .type = &TI_uint64_t.type_info,
                .bitfield = {
                    .offset = offsetof(CMacro, _bits),
                    .kind = MK_BITFIELD,
                    .bitsize = 1,
                    .bitoffset = 0,
                },
            },
            [1] = {
                .name = (Atom)AT_atomize(&at, "is_variadic", sizeof "is_variadic" -1),
                .type = &TI_uint64_t.type_info,
                .bitfield = {
                    .offset = offsetof(CMacro, _bits),
                    .kind = MK_BITFIELD,
                    .bitsize = 1,
                    .bitoffset = 1,
                },
            },
            [2] = {
                .name = (Atom)AT_atomize(&at, "is_builtin", sizeof "is_builtin" -1),
                .type = &TI_uint64_t.type_info,
                .bitfield = {
                    .offset = offsetof(CMacro, _bits),
                    .kind = MK_BITFIELD,
                    .bitsize = 1,
                    .bitoffset = 2,
                },
            },
            [3] = {
                .name = (Atom)AT_atomize(&at, "nparams", sizeof "nparams" -1),
                .type = &TI_uint64_t.type_info,
                .bitfield = {
                    .offset = offsetof(CMacro, _bits),
                    .kind = MK_BITFIELD,
                    .bitsize = 16,
                    .bitoffset = 16,
                    .noprint=1,
                },
            },
            [4] = {
                .name = (Atom)AT_atomize(&at, "nreplace", sizeof "nreplace" -1),
                .type = &TI_uint64_t.type_info,
                .bitfield = {
                    .offset = offsetof(CMacro, _bits),
                    .kind = MK_BITFIELD,
                    .bitsize = 32,
                    .bitoffset = 32,
                    .noprint=1,
                },
            },
            [5] = {
                .name = (Atom)AT_atomize(&at, "params", sizeof "params" -1),
                .type = &TI_Atom.type_info,
                .flexible = {
                    .offset = offsetof(CMacro, data),
                    .kind = MK_FLEXIBLE_ARRAY,
                    .length_mi = 3,
                    .after_mi = TI_AFTER_MI_NONE,
                },
            },
            [6] = {
                .name = (Atom)AT_atomize(&at, "expand", sizeof "expand" -1),
                .type = &TI_CPPToken.type_info,
                .flexible = {
                    .offset = offsetof(CMacro, data),
                    .kind = MK_FLEXIBLE_ARRAY,
                    .length_mi = 4,
                    .after_mi = 5,
                },
            },
        },
    };
    struct TypeInfoAtomMap TI_AtomMap_Macro = {
        .name = (Atom)AT_atomize(&at, "Macro", sizeof "Macro" - 1),
        .size = sizeof(AtomMap),
        .align = _Alignof(AtomMap),
        .kind = TIK_ATOM_MAP,
        .type = &TI_CMacro.type_info,
    };
    struct {
        union { TypeInfo type_info; struct { STRUCTINFO; }; };
        MemberInfo members[9];
    } TI_CPreprocessor = {
        .name = (Atom)AT_atomize(&at, "CPreprocessor", sizeof "CPreprocessor" -1),
        .size = sizeof(CPreprocessor),
        .align = _Alignof(CPreprocessor),
        .kind = TIK_STRUCT,
        .length = arrlen(TI_CPreprocessor.members),
        .members = {
            {
                .name = (Atom)AT_atomize(&at, "iquote_paths", sizeof "iquote_paths" -1),
                .type = &TI_ma_StringView.type_info,
                .offset = offsetof(CPreprocessor, iquote_paths),
            },
            {
                .name = (Atom)AT_atomize(&at, "Ipaths", sizeof "Ipaths" -1),
                .type = &TI_ma_StringView.type_info,
                .offset = offsetof(CPreprocessor, Ipaths),
            },
            {
                .name = (Atom)AT_atomize(&at, "isystem_paths", sizeof "isystem_paths" -1),
                .type = &TI_ma_StringView.type_info,
                .offset = offsetof(CPreprocessor, isystem_paths),
            },
            {
                .name = (Atom)AT_atomize(&at, "istandard_system_paths", sizeof "istandard_system_paths" -1),
                .type = &TI_ma_StringView.type_info,
                .offset = offsetof(CPreprocessor, istandard_system_paths),
            },
            {
                .name = (Atom)AT_atomize(&at, "idirafter_paths", sizeof "idirafter_paths" -1),
                .type = &TI_ma_StringView.type_info,
                .offset = offsetof(CPreprocessor, idirafter_paths),
            },
            {
                .name = (Atom)AT_atomize(&at, "framework_paths", sizeof "framework_paths" -1),
                .type = &TI_ma_StringView.type_info,
                .offset = offsetof(CPreprocessor, framework_paths),
            },
            {
                .name = (Atom)AT_atomize(&at, "macros", sizeof "macros" -1),
                .type = &TI_AtomMap_Macro.type_info,
                .offset = offsetof(CPreprocessor, macros),
            },
            {
                .name = (Atom)AT_atomize(&at, "frames", sizeof "frames" -1),
                .type = &TI_ma_CPPFrame.type_info,
                .offset = offsetof(CPreprocessor, frames),
            },
            {
                .name = (Atom)AT_atomize(&at, "pending", sizeof "pending" -1),
                .type = &TI_ma_CPPToken.type_info,
                .offset = offsetof(CPreprocessor, pending),
            },
        },
    };

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
        log_printf(&logger, "%s:%d: err: %d\n", __FILE__,__LINE__,err);
        if(err) return err;
    }
    fc_write_path(fc, filename, strlen(filename));
    StringView txt;
    err = fc_read_file(fc, &txt);
    if(err) return err;
    CPPFrame init = {
        .file_id = (uint32_t)fc->map.count-1,
        .txt = txt,
        .line = 1,
        .column = 1,
    };
    err = ma_push(CPPFrame)(&cpp.frames, MALLOCATOR, init);
    if(err) return err;
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
