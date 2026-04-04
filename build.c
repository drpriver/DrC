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
#define TARGET_SETTINGS_EXTRA_FIELDS(X) \
    X(Atom, fuzz_cc, "--fuzz-cc", "Clang with libFuzzer support for building fuzz targets.", nil_atom) \
    X(Atom, fuzz_sysroot, "--fuzz-sysroot", "Sysroot for the fuzz compiler.", nil_atom)
#include "Drp/drbuild.h"
#include "Drp/path_util.h"
#include "Drp/MStringBuilder.h"
#include "Drp/file_util.h"

#define LIBFFI_RELEASE_URL "https://github.com/libffi/libffi/releases/download/v3.5.2/libffi-3.5.2-x86-64bit-msvc-binaries.zip"
#define LIBFFI_LIB "libffi-8.lib"
#define LIBFFI_DLL "libffi-8.dll"

#ifdef __clang__
#pragma clang assume_nonnull begin
#endif
enum {
    EXCLUDE_FROM_MAKEFILE = 0x1,
};

static int mkfile(BuildCtx*, BuildTarget*);
static int do_install(BuildCtx*, BuildTarget*);
static int fetch_libffi(BuildCtx*, BuildTarget*);
static int copy_libffi_dll(BuildCtx*, BuildTarget*);

int main(int argc, char** argv, char** envp){
    BuildCtx* ctx = build_ctx(argc, argv, envp, __FILE__);
    if(!ctx) return 1;
    BuildTarget* all = phony_target(ctx, "all");

    BuildTarget* Makefile = script_target(ctx, "Makefile", mkfile, NULL);
    Makefile->is_phony = 1;

    BuildTarget* cpp = exe_target(ctx, "drcpp", "cpp.c", ctx->target.os);
    add_dep(ctx, all, cpp);

    BuildTarget* cc = exe_target(ctx, "drc", "cc.c", ctx->target.os);
    if(ctx->target.os == OS_LINUX || (ctx->target.os == OS_NATIVE && BUILD_OS == OS_LINUX)){
        target_linkarg(ctx, cc, "-ldl");
    }
    BuildTarget* ffi_lib = NULL;
    BuildTarget* ffi_dll = NULL;
    BuildTarget* fetch_ffi = coro_target(ctx, "fetch-libffi", fetch_libffi, NULL);
    if(BUILD_OS == OS_WINDOWS){
        ffi_lib = alloc_targeta(ctx, b_atomize(ctx, "Fetched/libffi/" LIBFFI_LIB));
        add_dep(ctx, ffi_lib, fetch_ffi);
        add_out(ctx, fetch_ffi, ffi_lib);
        ffi_dll = bin_target(ctx, LIBFFI_DLL);
        BuildTarget* copy_ffi = script_target(ctx, "copy-libffi-dll", copy_libffi_dll, NULL);
        add_dep(ctx, copy_ffi, ffi_lib);
        add_dep(ctx, ffi_dll, copy_ffi);
        add_out(ctx, copy_ffi, ffi_dll);
        cmd_carg(&cc->cmd, "-IFetched/libffi");
        target_linkinp(ctx, cc, ffi_lib);
    }
    else {
        fetch_ffi->is_phony = 1;
        target_linkarg(ctx, cc, "-lffi");
    }
    add_dep(ctx, all, cc);

    BuildTarget* tests = phony_target(ctx, "tests");
    BuildTarget* test = phony_target(ctx, "test");
    BuildTarget* selftests = phony_target(ctx, "selftests");
    BuildTarget* coverage_tests = phony_target(ctx, "coverage-tests");
    coverage_tests->user_bits |= EXCLUDE_FROM_MAKEFILE;
    add_dep(ctx, test, tests);
    static const struct {
        const char* file;
        const char* name;
        const char* cmd_name;
        _Bool needs_lffi;
        _Bool skip_self_hosted;
    } test_files[] = {
        {"C/cpp_test.c", "cpp_test", "run_cpp_test", 0, 0},
        {"C/cc_lex_test.c", "cc_lex_test", "run_cc_lex_test", 0, 0},
        {"C/cc_test.c", "cc_test", "run_cc_test", 0, 0},
        {"C/ci_test.c", "ci_test", "run_ci_test", 0, 0},
        {"C/ci_oom_test.c", "ci_oom_test", "run_ci_oom_test", 0, 1},
        {"C/ci_native_test.c", "ci_native_test", "run_ci_native_test", 1, 0},
    };
    {
        for(size_t i = 0; i < sizeof test_files / sizeof test_files[0]; i++){
            const char* file = test_files[i].file;
            const char* name = test_files[i].name;
            const char* cmd_name = test_files[i].cmd_name;
            BuildTarget* bin = exe_target(ctx, name, file, OS_NATIVE);
            if(test_files[i].needs_lffi){
                if(BUILD_OS == OS_WINDOWS){
                    cmd_carg(&bin->cmd, "-IFetched/libffi");
                    target_linkinp(ctx, bin, ffi_lib);
                }
                else {
                    if(BUILD_OS == OS_LINUX)
                        target_linkarg(ctx, bin, "-ldl");
                    target_linkarg(ctx, bin, "-lffi");
                }
            }
            add_dep(ctx, all, bin);
            BuildTarget* cmd = cmd_target(ctx, cmd_name);
            cmd->is_phony = 1;
            target_prog(ctx, cmd, bin);
            if(test_files[i].needs_lffi && ffi_dll)
                add_dep(ctx, cmd, ffi_dll);
            if(!ctx->dash_dash_args.count)
                cmd_carg(&cmd->cmd, "--multithreaded");
            else
                for(size_t j = 0; j < ctx->dash_dash_args.count; j++){
                    Atom a = ctx->dash_dash_args.data[j];
                    cmd_arg(&cmd->cmd, (LongString){a->length, a->data});
                }
            add_dep(ctx, tests, cmd);

            // Coverage variant
            Atom cov_name = b_atomize_f(ctx, "coverage_%s", name);
            BuildTarget* cov_bin = exe_target(ctx, cov_name->data, file, OS_NATIVE);
            get_targeta(ctx, cov_name)->user_bits |= EXCLUDE_FROM_MAKEFILE;
            if(cov_bin->compiler_flavor != COMPILER_CL){
                if(cov_bin->compiler_flavor == COMPILER_CLANG_CL)
                    cmd_carg(&cov_bin->cmd, "/clang:--coverage");
                else
                    cmd_carg(&cov_bin->cmd, "--coverage");
            }
            if(test_files[i].needs_lffi){
                if(BUILD_OS == OS_WINDOWS){
                    cmd_carg(&cov_bin->cmd, "-IFetched/libffi");
                    target_linkinp(ctx, cov_bin, ffi_lib);
                }
                else {
                    if(BUILD_OS == OS_LINUX)
                        target_linkarg(ctx, cov_bin, "-ldl");
                    target_linkarg(ctx, cov_bin, "-lffi");
                }
            }
            Atom cov_cmd_name = b_atomize_f(ctx, "run_coverage_%s", name);
            BuildTarget* cov_cmd = cmd_target(ctx, cov_cmd_name->data);
            cov_cmd->is_phony = 1;
            cov_cmd->user_bits |= EXCLUDE_FROM_MAKEFILE;
            target_prog(ctx, cov_cmd, cov_bin);
            if(test_files[i].needs_lffi && ffi_dll)
                add_dep(ctx, cov_cmd, ffi_dll);
            cmd_carg(&cov_cmd->cmd, "--multithreaded");
            add_dep(ctx, coverage_tests, cov_cmd);
        }
        Atom cov_dir_name = b_atomize_f(ctx, "%s/coverage", ctx->build_dir->data);
        BuildTarget* coverage_dir = directory_target(ctx, cov_dir_name->data);
        BuildTarget* coverage = cmd_target(ctx, "coverage");
        coverage->is_phony = 1;
        add_deps(ctx, coverage, coverage_tests, coverage_dir);
        cmd_prog(&coverage->cmd, BUILD_OS == OS_WINDOWS ? LS("py") : LS("python3"));
        cmd_cargs(&coverage->cmd, "Tools/coverage.py", "--root", ".",
            "--merge-mode-functions=merge-use-line-0",
            "--gcov-ignore-parse-errors=negative_hits.warn_once_per_file",
            "--exclude", "Drp/", "--exclude", "Vendored/", "--exclude", ".*_test\\.c",
            "--markdown");
        cmd_argf(&coverage->cmd, "--txt=%s/coverage/coverage.txt", ctx->build_dir->data);
        cmd_argf(&coverage->cmd, "--html-details=%s/coverage/index.html", ctx->build_dir->data);
        cmd_argf(&coverage->cmd, "--object-directory=%s", ctx->build_dir->data);
    }
    {
        // TODO: there should be a helper in drbuild for custom compilers.
        const char* ext = BUILD_OS == OS_WINDOWS ? ".exe" : "";
        Atom fuzz_name = b_atomize_f(ctx, "%s/cc_fuzz%s", ctx->build_dir->data, ext);
        BuildTarget* fuzz = alloc_targeta(ctx, fuzz_name);
        fuzz->is_binary = 1;
        fuzz->is_cmd = 1;
        fuzz->is_compile_command = 1;
        Atom fuzz_cc = ctx->target.fuzz_cc->length ? ctx->target.fuzz_cc : b_atomize(ctx, "clang");
        CmdBuilder* cmd = &fuzz->cmd;
        cmd->allocator = allocator_from_arena(&ctx->perm_aa);
        cmd_prog(cmd, (LongString){fuzz_cc->length, fuzz_cc->data});
        cmd_cargs(cmd,
            "-g", "-O1", "-march=native",
            "-fsanitize=fuzzer,address,undefined",
            "-fno-sanitize-recover=undefined");
        if(ctx->target.fuzz_sysroot->length)
            cmd_argf(cmd, "--sysroot=%s", ctx->target.fuzz_sysroot->data);
        target_src_inp(ctx, fuzz, "C/cc_fuzz.c");
        cmd_cargs(cmd, "-o", fuzz_name->data);
        cmd_cargs(cmd, "-MT", fuzz_name->data, "-MMD", "-MP", "-MF");
        cmd_argf(cmd, "%s/cc_fuzz.deps", ctx->deps_dir->data);
        BuildTarget* fuzz_phony = phony_target(ctx, "cc_fuzz");
        add_dep(ctx, fuzz_phony, fuzz);
        Atom corpus_name = b_atomize_f(ctx, "%s/fuzz_corpus", ctx->build_dir->data);
        BuildTarget* corpus_dir = directory_target(ctx, corpus_name->data);
        BuildTarget* run_fuzz = exec_target(ctx, "run_cc_fuzz", fuzz);
        run_fuzz->is_phony = 1;
        add_dep(ctx, run_fuzz, corpus_dir);
        cmd_aarg(&run_fuzz->cmd, corpus_name);
        cmd_cargs(&run_fuzz->cmd, "-max_len=10000");
        cmd_argf(&run_fuzz->cmd, "-fork=%d", b_num_cpus());
        if(ctx->dash_dash_args.count){
            for(size_t j = 0; j < ctx->dash_dash_args.count; j++)
                cmd_aarg(&run_fuzz->cmd, ctx->dash_dash_args.data[j]);
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
        if(BUILD_OS == OS_WINDOWS){
            cmd_carg(&cc_opt->cmd, "-IFetched/libffi");
            target_linkinp(ctx, cc_opt, ffi_lib);
        }
        else {
            if(BUILD_OS == OS_LINUX)
                target_linkarg(ctx, cc_opt, "-ldl");
            target_linkarg(ctx, cc_opt, "-lffi");
        }

        selfhost = cmd_target(ctx, "selfhost");
        selfhost->is_phony = 1;
        add_dep(ctx, selfhost, cc_opt);
        if(ffi_dll)
            add_dep(ctx, selfhost, ffi_dll);
        cmd_prog(&selfhost->cmd, (LongString){cc_opt->name->length, cc_opt->name->data});
        cmd_cargs(&selfhost->cmd, "cc.c", "Samples/hello.c");
        if(BUILD_OS != OS_WINDOWS)
            add_dep(ctx, selftests, selfhost);

        for(size_t i = 0; i < sizeof test_files / sizeof test_files[0]; i++){
            if(test_files[i].skip_self_hosted) continue;
            Atom name = b_atomize_f(ctx, "self_%s", test_files[i].name);
            BuildTarget* cmd = cmd_target(ctx, name->data);
            cmd->is_phony = 1;
            add_dep(ctx, cmd, cc_opt);
            cmd_prog(&cmd->cmd, (LongString){cc_opt->name->length, cc_opt->name->data});
            if(test_files[i].needs_lffi && ffi_lib)
                cmd_carg(&cmd->cmd, "-IFetched/libffi");
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
        const char* names[] = {"drcpp", "drc"};
        for(size_t i = 0; i < sizeof bins / sizeof bins[0]; i++){
            BuildTarget* bin = bins[i];
            const char* name = names[i];

            Atom run_name = b_atomize_f(ctx, "run_%s", name);
            BuildTarget* run = exec_target(ctx, run_name->data, bin);
            run->is_phony = 1;
            if(ffi_dll && bin == cc)
                add_dep(ctx, run, ffi_dll);
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
        static BuildTarget* bins[2]; bins[0] = cpp; bins[1] = cc;
        BuildTarget* install = script_target(ctx, "install", do_install, bins);
        install->is_phony = 1;
        add_dep(ctx, install, cpp);
        add_dep(ctx, install, cc);
    }
    {
        BuildTarget* tags = cmd_target(ctx, "tags");
        tags->is_phony = 1;
        cmd_prog(&tags->cmd, BUILD_OS == OS_WINDOWS?LS("py") : LS("python3"));
        cmd_arg(&tags->cmd, LS("Tools/ct.py"));
        BuildTarget* compile_commands_json = get_target(ctx, "compile_commands.json");
        add_dep(ctx, tags, compile_commands_json);
    }
    {
        BuildTarget* docs = phony_target(ctx, "docs");
        static const char* doc_files[] = {"README", "EXTENSIONS"};
        for(size_t i = 0; i < sizeof doc_files / sizeof doc_files[0]; i++){
            Atom md_name = b_atomize_f(ctx, "%s.md", doc_files[i]);
            BuildTarget* out = alloc_targeta(ctx, md_name);
            out->is_generated = 1;
            out->user_bits |= EXCLUDE_FROM_MAKEFILE;
            Atom cmd_name = b_atomize_f(ctx, "compile_%s", doc_files[i]);
            BuildTarget* cmd = cmd_target(ctx, cmd_name->data);
            cmd->user_bits |= EXCLUDE_FROM_MAKEFILE;
            cmd_prog(&cmd->cmd, LS("dndc"));
            Atom dnd_name = b_atomize_f(ctx, "%s.dnd", doc_files[i]);
            target_src_inp(ctx, cmd, dnd_name->data);
            cmd_cargs(&cmd->cmd, "--md", "--no-css");
            target_argout(ctx, cmd, "-o", out);
            add_dep(ctx, docs, out);
        }
    }
    get_target(ctx, "fish-completions")->user_bits |= EXCLUDE_FROM_MAKEFILE;
    return execute_targets(ctx);
}

static
int
do_install(BuildCtx* ctx, BuildTarget* tgt){
    BuildTarget*const* bins = (BuildTarget*const*)tgt->user_data;
    Atom prefix = env_getenv2(&ctx->env, "PREFIX", sizeof "PREFIX" - 1);
    Atom destdir = env_getenv2(&ctx->env, "DESTDIR", sizeof "DESTDIR" - 1);
    Atom bindir_env = env_getenv2(&ctx->env, "bindir", sizeof "bindir" - 1);
    if(!prefix && !bindir_env && BUILD_OS == OS_WINDOWS){
        b_loglvl(BLOG_ERROR, ctx, "PREFIX or bindir must be set on Windows for install\n");
        return 1;
    }
    Atom bindir;
    if(bindir_env)
        bindir = bindir_env;
    else
        bindir = b_atomize_f(ctx, "%s/bin", prefix ? prefix->data : "/usr/local");
    if(destdir)
        bindir = b_atomize_f(ctx, "%s%s", destdir->data, bindir->data);
    int err;
    if(BUILD_OS == OS_WINDOWS){
        err = mkdirs_if_not_exists(ctx, AT_to_LS(bindir));
        if(err) return err;
        for(int i = 0; i < 2; i++){
            BuildTarget* c = bins[i];
            Atom dst = b_atomize_f(ctx, "%s/%s", bindir->data, path_basename(AT_to_SV(c->name), BUILD_OS==OS_WINDOWS).text);
            err = copy_file(ctx, c->name->data, dst->data);
            if(err) return err;
        }
    }
    else {
        CmdBuilder cmd = {.allocator = allocator_from_arena(&ctx->tmp_aa)};
        cmd_prog(&cmd, LS("install"));
        cmd_cargs(&cmd, "-d", bindir->data);
        err = b_run_cmd_sync(ctx, &cmd);
        if(err) return err;
        cmd_clear(&cmd);
        cmd_prog(&cmd, LS("install"));
        cmd_cargs(&cmd, "-m", "755");
        for(int i = 0; i < 2; i++)
            cmd_aarg(&cmd, bins[i]->name);
        cmd_aarg(&cmd, bindir);
        err = b_run_cmd_sync(ctx, &cmd);
        cmd_destroy(&cmd);
        if(err) return err;
    }
    return 0;
}

static
int
fetch_libffi(BuildCtx* ctx, BuildTarget* tgt){
    BCoro* coro = &tgt->coro;
    BSTATE(fetch_libffi,
        char _pad;
    );
    CmdBuilder* cmd = &tgt->cmd;
    switch(coro->step){ BGO(BFINISHED); BGO(0); BGO(1); BGO(2); default: b_debug_break(ctx, "Invalid coro step");}
    L0:;
    if(b_file_info_uncached(ctx, "Fetched/libffi", sizeof "Fetched/libffi" - 1)->exists)
        goto finish;
    if(!b_file_info_uncached(ctx, "Fetched", sizeof "Fetched" - 1)->exists){
        b_log(ctx, "mkdir Fetched\n");
        int err = mkdir_if_not_exists(ctx, "Fetched");
        if(err) return BERROR;
    }
    {
        int err = mkdir_if_not_exists(ctx, "Fetched/libffi");
        if(err) return BERROR;
    }
    {
        cmd_clear(cmd);
        cmd_prog(cmd, LS("curl"));
        cmd_cargs(cmd, "-L", LIBFFI_RELEASE_URL, "-f", "-s", "-o", "Fetched/libffi.zip");
        int err = b_run_cmd_async(ctx, tgt, cmd);
        if(err) return BERROR;
        BYIELD(1);
    }
    {
        cmd_clear(cmd);
        cmd_prog(cmd, LS("tar"));
        cmd_cargs(cmd, "-xf", "Fetched/libffi.zip", "-C", "Fetched/libffi");
        int err = b_run_cmd_async(ctx, tgt, cmd);
        if(err) return BERROR;
        BYIELD(2);
    }
    {
        b_log(ctx, "rm Fetched/libffi.zip\n");
        int err = rm_file(ctx, "Fetched/libffi.zip");
        if(err) return BERROR;
    }
    finish:
    BFINISH();
}

static
int
copy_libffi_dll(BuildCtx* ctx, BuildTarget* tgt){
    (void)tgt;
    Atom dst = b_atomize_f(ctx, "%s/" LIBFFI_DLL, ctx->build_dir->data);
    b_log(ctx, "cp Fetched/libffi/" LIBFFI_DLL " %s\n", dst->data);
    int err = copy_file(ctx, "Fetched/libffi/" LIBFFI_DLL, dst->data);
    if(err)
        b_loglvl(BLOG_ERROR, ctx, "Failed to copy " LIBFFI_DLL "\n");
    return err;
}

static
int
mkfile(BuildCtx* ctx, BuildTarget* _tgt){
    MStringBuilder sb = {.allocator = allocator_from_arena(&ctx->tmp_aa)};
    AtomMapItems items = AM_items(&ctx->targets);
    msb_write_literal(&sb, "BUILDTARGETS:=");
    size_t len = sb.cursor;
    for(size_t i = 0; i < items.count; i++){
        BuildTarget* tgt = items.data[i].p;
        if(tgt == _tgt) continue;
        if(tgt->user_bits & EXCLUDE_FROM_MAKEFILE)
            continue;
        StringView name = {tgt->name->length, tgt->name->data};
        if(name.text[0] == '.') continue;
        if(sv_startswith(name, (StringView){ctx->build_dir->length, ctx->build_dir->data}))
            continue;
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
        "UNKNOWN:=$(filter-out $(BUILDTARGETS) build build.exe Makefile,$(MAKECMDGOALS))\n"
        ".PHONY: $(BUILDTARGETS) $(UNKNOWN)\n"
        "\n"
        "ifeq ($(OS),Windows_NT)\n"
        "ifeq ($(origin CC),default)\n"
        "CC:=$(firstword $(foreach c,cl clang,$(if $(shell where $(c) 2>/dev/null),$(c))))\n"
        "endif\n"
        "$(BUILDTARGETS) $(UNKNOWN): | build.exe\n"
            "\t@build $@\n"
        "build.exe:\n"
        "ifeq ($(CC),cl)\n"
            "\t$(CC) /nologo /std:c11 /Zc:preprocessor /wd5105 build.c /Fe:$@\n"
        "else\n"
             "\t$(CC) -march=native build.c -o $@\n"
        "endif\n"
             "\t./build -b Bin\n"
        "else\n"
        "$(BUILDTARGETS) $(UNKNOWN): | build\n"
             "\t@./build $@\n"
        "build:\n"
             "\t$(CC) -march=native build.c -o $@\n"
             "\t./build -b Bin\n"
        "endif\n"
        ".DEFAULT_GOAL:=all\n");
    if(sb.errored) return sb.errored;
    StringView sv = msb_borrow_sv(&sb);
    FileError fe = write_file("Makefile", sv.text, sv.length);
    msb_destroy(&sb);
    if(fe.errored){
        b_loglvl(BLOG_ERROR, ctx, "Failed to write Makefile\n");
        return 1;
    }
    return 0;
}

#ifdef __clang__
#pragma clang assume_nonnull end
#endif

#include "Drp/drbuild.c"
