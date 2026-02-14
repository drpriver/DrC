//usr/bin/cc "$0" -o build && exec ./build "$@" -b Bin || exit
#include "Drp/compiler_warnings.h"
#include "Drp/drbuild.h"
#include "Drp/path_util.h"

#ifdef __clang__
#pragma clang assume_nonnull begin
#endif

int main(int argc, char** argv, char** envp){
    BuildCtx* ctx = build_ctx(argc, argv, envp, __FILE__);
    if(!ctx) return 1;
    BuildTarget* cpp = exe_target(ctx, "cpp", "cpp.c", OS_NATIVE);
    BuildTarget* all = phony_target(ctx, "all");
    add_deps(ctx, all, cpp);

    BuildTarget* tests = phony_target(ctx, "tests");
    {
        static const struct {
            const char* file;
            const char* name;
            const char* cmd_name;
        } test_files[] = {
            {"C/cpp_test.c", "cpp_test", "run_cpp_test"},
        };
        for(size_t i = 0; i < sizeof test_files / sizeof test_files[0]; i++){
            const char* file = test_files[i].file;
            const char* name = test_files[i].name;
            const char* cmd_name = test_files[i].cmd_name;
            BuildTarget* bin = exe_target(ctx, name, file, OS_NATIVE);
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
        add_deps(ctx, debug, cpp);
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
        BuildTarget* cc = get_target(ctx, "compile_commands.json");
        add_dep(ctx, tags, cc);
    }
    return execute_targets(ctx);
}

#ifdef __clang__
#pragma clang assume_nonnull end
#endif

#include "Drp/drbuild.c"
