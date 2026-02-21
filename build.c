//usr/bin/cc "$0" -o build && exec ./build "$@" -b Bin || exit
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
    cmd_cargs(&cc->cmd, "-lffi");
    add_dep(ctx, all, cc);

    BuildTarget* tests = phony_target(ctx, "tests");
    BuildTarget* test = phony_target(ctx, "test");
    add_dep(ctx, test, tests);
    {
        static const struct {
            const char* file;
            const char* name;
            const char* cmd_name;
        } test_files[] = {
            {"C/cpp_test.c", "cpp_test", "run_cpp_test"},
            {"C/cc_lex_test.c", "cc_lex_test", "run_cc_lex_test"},
            {"C/cc_test.c", "cc_test", "run_cc_test"},
        };
        for(size_t i = 0; i < sizeof test_files / sizeof test_files[0]; i++){
            const char* file = test_files[i].file;
            const char* name = test_files[i].name;
            const char* cmd_name = test_files[i].cmd_name;
            BuildTarget* bin = exe_target(ctx, name, file, OS_NATIVE);
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

    {
        BuildTarget* run = exec_target(ctx, "run", cpp);
        run->is_phony = 1;
        for(size_t i = 0; i < ctx->dash_dash_args.count; i++){
            Atom a = ctx->dash_dash_args.data[i];
            cmd_arg(&run->cmd, (LongString){a->length, a->data});
        }
    }
    {
        BuildTarget* debug = cmd_target(ctx, "debug");
        add_dep(ctx, debug, cpp);
        debug->should_exec = 1;
        debug->is_phony = 1;
        cmd_prog(&debug->cmd, LS("lldb"));
        cmd_arg(&debug->cmd, (LongString){cpp->name->length, cpp->name->data});
        cmd_args(&debug->cmd, LS("-o"), LS("run"), LS("--batch"));
        if(ctx->dash_dash_args.count){
            cmd_arg(&debug->cmd, LS("--"));
            for(size_t i = 0; i < ctx->dash_dash_args.count; i++){
                Atom a = ctx->dash_dash_args.data[i];
                cmd_arg(&debug->cmd, (LongString){a->length, a->data});
            }
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
        if(tgt == _tgt) continue;
        if(len > 80){
            msb_write_literal(&sb, "\\\n    ");
            len = 4;
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
