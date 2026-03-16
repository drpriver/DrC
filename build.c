//usr/bin/cc "$0" -o build && exec ./build "$@" -b Bin || exit
//
// Copyright © 2026-2026, David Priver <david@davidpriver.com>
//
// Nobuild build script.
//
// Bootstrap with `cc build.c -o build && ./build -b Bin`
// and then just `./build` from then on.
//
// Do `./build --help` to see full options.
//
// There's some support for generating shell completions,
// but you need an accompanying shell function.
//
#include "Drp/compiler_warnings.h"
#include "Drp/drbuild.h"
#include "Drp/path_util.h"
#include "Drp/MStringBuilder.h"
#include "Drp/file_util.h"

#ifdef __clang__
#pragma clang assume_nonnull begin
#endif

static int mkfile(BuildCtx*, BuildTarget*);

int main(int argc, char** argv, char** envp){
    BuildCtx* ctx = build_ctx(argc, argv, envp, __FILE__);
    if(!ctx) return 1;
    BuildTarget* all = phony_target(ctx, "all");

    BuildTarget* Makefile = script_target(ctx, "Makefile", mkfile, NULL);
    Makefile->is_phony = 1;

    BuildTarget* cpp = exe_target(ctx, "cpp", "cpp.c", ctx->target.os);
    add_dep(ctx, all, cpp);

    BuildTarget* cc = exe_target(ctx, "cc", "cc.c", ctx->target.os);
    if(ctx->target.os == OS_LINUX || (ctx->target.os == OS_NATIVE && BUILD_OS == OS_LINUX)){
        cmd_carg(&cc->cmd, "-ldl");
    }
    cmd_carg(&cc->cmd, "-lffi");
    add_dep(ctx, all, cc);

    BuildTarget* tests = phony_target(ctx, "tests");
    BuildTarget* test = phony_target(ctx, "test");
    BuildTarget* selftests = phony_target(ctx, "selftests");
    add_dep(ctx, test, tests);
    static const struct {
        const char* file;
        const char* name;
        const char* cmd_name;
        _Bool needs_lffi;
    } test_files[] = {
        {"C/cpp_test.c", "cpp_test", "run_cpp_test", 0},
        {"C/cc_lex_test.c", "cc_lex_test", "run_cc_lex_test", 0},
        {"C/cc_test.c", "cc_test", "run_cc_test", 0},
        {"C/ci_test.c", "ci_test", "run_ci_test", 0},
        {"C/ci_native_test.c", "ci_native_test", "run_ci_native_test", 1},
    };
    {
        for(size_t i = 0; i < sizeof test_files / sizeof test_files[0]; i++){
            const char* file = test_files[i].file;
            const char* name = test_files[i].name;
            const char* cmd_name = test_files[i].cmd_name;
            BuildTarget* bin = exe_target(ctx, name, file, OS_NATIVE);
            if(test_files[i].needs_lffi){
                if(BUILD_OS == OS_LINUX)
                    cmd_carg(&bin->cmd, "-ldl");
                cmd_carg(&bin->cmd, "-lffi");
            }
            add_dep(ctx, all, bin);
            BuildTarget* cmd = cmd_target(ctx, cmd_name);
            cmd->is_phony = 1;
            target_prog(ctx, cmd, bin);
            if(!ctx->dash_dash_args.count)
                cmd_carg(&cmd->cmd, "--multithreaded");
            else
                for(size_t j = 0; j < ctx->dash_dash_args.count; j++){
                    Atom a = ctx->dash_dash_args.data[j];
                    cmd_arg(&cmd->cmd, (LongString){a->length, a->data});
                }
            add_dep(ctx, tests, cmd);
        }
    }
    BuildTarget* selfhost;
    {
        // Optimized cc without sanitizers for self-hosted tests
        _Bool saved_ns = ctx->target.native_sanitize;
        ctx->target.native_sanitize = 0;
        BuildTarget* cc_opt = exe_target(ctx, "cc_opt", "cc.c", OS_NATIVE);
        add_dep(ctx, all, cc_opt);
        ctx->target.native_sanitize = saved_ns;
        cmd_carg(&cc_opt->cmd, "-O2");
        if(BUILD_OS == OS_LINUX)
            cmd_carg(&cc_opt->cmd, "-ldl");
        cmd_carg(&cc_opt->cmd, "-lffi");

        selfhost = cmd_target(ctx, "selfhost");
        selfhost->is_phony = 1;
        add_dep(ctx, selfhost, cc_opt);
        cmd_prog(&selfhost->cmd, (LongString){cc_opt->name->length, cc_opt->name->data});
        cmd_cargs(&selfhost->cmd, "cc.c", "Samples/hello.c");
        add_dep(ctx, selftests, selfhost);

        for(size_t i = 0; i < sizeof test_files / sizeof test_files[0]; i++){
            if(test_files[i].needs_lffi) continue;
            Atom name = b_atomize_f(ctx, "self_%s", test_files[i].name);
            BuildTarget* cmd = cmd_target(ctx, name->data);
            cmd->is_phony = 1;
            add_dep(ctx, cmd, cc_opt);
            cmd_prog(&cmd->cmd, (LongString){cc_opt->name->length, cc_opt->name->data});
            cmd_carg(&cmd->cmd, test_files[i].file);
            if(!ctx->dash_dash_args.count)
                cmd_carg(&cmd->cmd, "--multithreaded");
            else
                for(size_t j = 0; j < ctx->dash_dash_args.count; j++){
                    Atom a = ctx->dash_dash_args.data[j];
                    cmd_arg(&cmd->cmd, (LongString){a->length, a->data});
                }
            add_dep(ctx, selftests, cmd);
        }
        add_dep(ctx, test, selftests);
    }

    {
        BuildTarget* bins[] = {cpp, cc};
        const char* names[] = {"cpp", "cc"};
        for(size_t i = 0; i < sizeof bins / sizeof bins[0]; i++){
            BuildTarget* bin = bins[i];
            const char* name = names[i];

            Atom run_name = b_atomize_f(ctx, "run_%s", name);
            BuildTarget* run = exec_target(ctx, run_name->data, bin);
            run->is_phony = 1;
            for(size_t j = 0; j < ctx->dash_dash_args.count; j++){
                Atom a = ctx->dash_dash_args.data[j];
                cmd_arg(&run->cmd, (LongString){a->length, a->data});
            }

            Atom debug_name = b_atomize_f(ctx, "debug_%s", name);
            BuildTarget* debug = cmd_target(ctx, debug_name->data);
            add_dep(ctx, debug, bin);
            debug->should_exec = 1;
            debug->is_phony = 1;
            cmd_prog(&debug->cmd, LS("lldb"));
            cmd_arg(&debug->cmd, (LongString){bin->name->length, bin->name->data});
            cmd_args(&debug->cmd, LS("-o"), LS("run"), LS("--batch"));
            if(ctx->dash_dash_args.count){
                cmd_arg(&debug->cmd, LS("--"));
                for(size_t j = 0; j < ctx->dash_dash_args.count; j++){
                    Atom a = ctx->dash_dash_args.data[j];
                    cmd_arg(&debug->cmd, (LongString){a->length, a->data});
                }
            }
        }
    }
    {
        BuildTarget* repl = exec_target(ctx, "repl", cc);
        repl->is_phony = 1;
        cmd_arg(&repl->cmd, LS("--repl"));
        for(size_t i = 0; i < ctx->dash_dash_args.count; i++){
            Atom a = ctx->dash_dash_args.data[i];
            cmd_arg(&repl->cmd, (LongString){a->length, a->data});
        }
    }
    {
        BuildTarget* tags = cmd_target(ctx, "tags");
        tags->is_phony = 1;
        cmd_prog(&tags->cmd, BUILD_OS == OS_WINDOWS?LS("py") : LS("python3"));
        cmd_arg(&tags->cmd, LS("Tools/ct.py"));
        BuildTarget* compile_commands_json = get_target(ctx, "compile_commands.json");
        add_dep(ctx, tags, compile_commands_json);
    }
    return execute_targets(ctx);
}

static
int
mkfile(BuildCtx* ctx, BuildTarget* _tgt){
    (void)_tgt;
    MStringBuilder sb = {.allocator = allocator_from_arena(&ctx->tmp_aa)};
    AtomMapItems items = AM_items(&ctx->targets);
    msb_write_literal(&sb, "BUILDTARGETS:=");
    size_t len = sb.cursor;
    for(size_t i = 0; i < items.count; i++){
        BuildTarget* tgt = items.data[i].p;
        StringView name = {tgt->name->length, tgt->name->data};
        if(name.text[0] == '.') continue;
        if(sv_startswith(name, (StringView){ctx->build_dir->length, ctx->build_dir->data}))
            continue;
        if(sv_equals(name, (SV("fish-completions"))))
            continue;
        if(tgt == _tgt) continue;
        if(len + tgt->name->length + 1 > 60){
            msb_write_literal(&sb, "\\\n  ");
            len = 2;
        }
        msb_write_str(&sb, tgt->name->data, tgt->name->length);
        msb_write_char(&sb, ' ');
        len += tgt->name->length+1;
    }
    if(msb_peek(&sb) == ' ') msb_erase(&sb, 1);
    msb_write_char(&sb, '\n');
    msb_write_literal(&sb,
        ".PHONY: $(BUILDTARGETS)\n"
        "\n"
        "ifeq ($(OS),Windows_NT)\n"
        "ifeq ($(origin CC),default)\n"
        "CC:=$(firstword $(foreach c,clang gcc cl,$(if $(shell where $(c) 2>nul),$(c))))\n"
        "endif\n"
        "$(BUILDTARGETS): | build.exe\n"
            "\t@build $@\n"
        "build.exe:\n"
        "ifeq ($(CC),cl)\n"
            "\t$(CC) build.c /Fe:$@\n"
        "else\n"
             "\t$(CC) build.c -o $@\n"
        "endif\n"
             "\t./build -b Bin\n"
        "else\n"
        "$(BUILDTARGETS): | build\n"
             "\t@./build $@\n"
        "build:\n"
             "\t$(CC) build.c -o $@\n"
             "\t./build -b Bin\n"
        "endif\n"
        ".DEFAULT_GOAL:=all\n");
    if(sb.errored) return sb.errored;
    StringView sv = msb_borrow_sv(&sb);
    FileError fe = write_file("Makefile", sv.text, sv.length);
    (void)fe;
    msb_destroy(&sb);
    return 0;
}

#ifdef __clang__
#pragma clang assume_nonnull end
#endif

#include "Drp/drbuild.c"
