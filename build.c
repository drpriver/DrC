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
    X(Atom, fuzz_sysroot, "--fuzz-sysroot", "Sysroot for the fuzz compiler.", nil_atom) \
    X(Atom, prefix, "--prefix", "Install prefix. Overrides the PREFIX env var.", nil_atom) \
    X(Atom, bindir, "--bindir", "Directory to install binaries into. Overrides the bindir env var.", nil_atom) \
    X(Atom, destdir, "--destdir", "Staging directory prepended to install paths. Overrides the DESTDIR env var.", nil_atom)
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
static void link_libffi(BuildCtx*, BuildTarget*, enum OS, BuildTarget* _Null_unspecified);

int main(int argc, char** argv, char** envp){
    BuildCtx* ctx = b_build_ctx(argc, argv, envp, __FILE__);
    if(!ctx) return 1;
    BuildTarget* all = b_phony_target(ctx, "all");

    BuildTarget* Makefile = b_script_target(ctx, "Makefile", mkfile, NULL);
    Makefile->is_phony = 1;

    BuildTarget* cpp = b_exe_target(ctx, "drcpp", "cpp.c", ctx->target.os);
    b_add_dep(ctx, all, cpp);

    BuildTarget* cc = b_exe_target(ctx, "drc", "cc.c", ctx->target.os);
    BuildTarget* ffi_lib = NULL;
    BuildTarget* ffi_dll = NULL;
    BuildTarget* fetch_ffi = b_coro_target(ctx, "fetch-libffi", fetch_libffi, NULL);
    if(BUILD_OS == OS_WINDOWS){
        ffi_lib = b_targeta(ctx, b_atomize(ctx, "Fetched/libffi/" LIBFFI_LIB));
        b_add_dep(ctx, ffi_lib, fetch_ffi);
        b_add_out(ctx, fetch_ffi, ffi_lib);
        ffi_dll = b_bin_target(ctx, LIBFFI_DLL);
        BuildTarget* copy_ffi = b_script_target(ctx, "copy-libffi-dll", copy_libffi_dll, NULL);
        b_add_dep(ctx, copy_ffi, ffi_lib);
        b_add_dep(ctx, ffi_dll, copy_ffi);
        b_add_out(ctx, copy_ffi, ffi_dll);
    }
    else {
        fetch_ffi->is_phony = 1;
    }
    link_libffi(ctx, cc, ctx->target.os, ffi_lib);
    b_add_dep(ctx, all, cc);

    BuildTarget* tests = b_phony_target(ctx, "tests");
    BuildTarget* test = b_phony_target(ctx, "test");
    BuildTarget* selftests = b_phony_target(ctx, "selftests");
    BuildTarget* coverage_tests = b_phony_target(ctx, "coverage-tests");
    coverage_tests->user_bits |= EXCLUDE_FROM_MAKEFILE;
    b_add_dep(ctx, test, tests);
    BuildTarget* cc_opt, *cc_cov;
    {
        // Optimized cc without sanitizers for self-hosted tests
        _Bool saved_ns = ctx->target.native_sanitize;
        ctx->target.native_sanitize = 0;

        {
            cc_opt = b_exe_target(ctx, "cc_opt", "cc.c", OS_NATIVE);
            b_add_dep(ctx, all, cc_opt);
            b_arg(ctx, cc_opt, "-O2");
            link_libffi(ctx, cc_opt, OS_NATIVE, ffi_lib);
        }
        {
            cc_cov = b_exe_target(ctx, "cc_cov", "cc.c", OS_NATIVE);
            b_get_target(ctx, "cc_cov")->user_bits |= EXCLUDE_FROM_MAKEFILE;
            if(cc_cov->compiler_flavor != COMPILER_CL){
                if(cc_cov->compiler_flavor == COMPILER_CLANG_CL){
                    b_arg(ctx, cc_cov, "/clang:--coverage");
                    b_arg(ctx, cc_cov, "/clang:-fprofile-update=atomic");
                }
                else{
                    b_arg(ctx, cc_cov, "--coverage");
                    b_arg(ctx, cc_cov, "-fprofile-update=atomic");
                }
            }
            link_libffi(ctx, cc_cov, OS_NATIVE, ffi_lib);
        }
        ctx->target.native_sanitize = saved_ns;
    }
    static const struct {
        const char* file;
        const char* name;
        const char* cmd_name;
        _Bool needs_lffi;
        _Bool skip_self_hosted;
        _Bool needs_drc_path;
    } test_files[] = {
        {"C/cpp_test.c", "cpp_test", "run_cpp_test", 0, 0, 0},
        {"C/cc_lex_test.c", "cc_lex_test", "run_cc_lex_test", 0, 0, 0},
        {"C/cc_test.c", "cc_test", "run_cc_test", 0, 0, 0},
        {"C/ci_test.c", "ci_test", "run_ci_test", 0, 0, 0},
        {"C/ci_oom_test.c", "ci_oom_test", "run_ci_oom_test", 0, 1, 0},
        {"C/ci_native_test.c", "ci_native_test", "run_ci_native_test", 1, 0, 0},
        {"C/ci_concurrent_test.c", "ci_concurrent_test", "run_ci_concurrent_test", 1, 0, 0},
        {"drc_test.c", "drc_test", "run_drc_test", 0, 0, 1},
    };
    {
        for(size_t i = 0; i < sizeof test_files / sizeof test_files[0]; i++){
            const char* file = test_files[i].file;
            const char* name = test_files[i].name;
            const char* cmd_name = test_files[i].cmd_name;
            BuildTarget* bin = b_exe_target(ctx, name, file, OS_NATIVE);
            if(test_files[i].needs_lffi)
                link_libffi(ctx, bin, OS_NATIVE, ffi_lib);
            b_add_dep(ctx, all, bin);
            BuildTarget* cmd = b_cmd_target_prog(ctx, cmd_name, bin);
            cmd->is_phony = 1;
            if(test_files[i].needs_lffi && ffi_dll)
                b_add_dep(ctx, cmd, ffi_dll);
            if(test_files[i].needs_drc_path)
                b_arginp(ctx, cmd, "--drc", cc_opt);
            if(!ctx->dash_dash_args.count)
                b_arg(ctx, cmd, "--multithreaded");
            else
                for(size_t j = 0; j < ctx->dash_dash_args.count; j++)
                    b_aarg(ctx, cmd, ctx->dash_dash_args.data[j]);
            b_add_dep(ctx, tests, cmd);

            // Coverage variant
            Atom cov_name = b_atomize_f(ctx, "coverage_%s", name);
            BuildTarget* cov_bin = b_exe_target(ctx, cov_name->data, file, OS_NATIVE);
            b_get_targeta(ctx, cov_name)->user_bits |= EXCLUDE_FROM_MAKEFILE;
            if(cov_bin->compiler_flavor != COMPILER_CL){
                if(cov_bin->compiler_flavor == COMPILER_CLANG_CL){
                    b_arg(ctx, cov_bin, "/clang:--coverage");
                    b_arg(ctx, cov_bin, "/clang:-fprofile-update=atomic");
                }
                else{
                    b_arg(ctx, cov_bin, "--coverage");
                    b_arg(ctx, cov_bin, "-fprofile-update=atomic");
                }
            }
            if(test_files[i].needs_lffi)
                link_libffi(ctx, cov_bin, OS_NATIVE, ffi_lib);
            Atom cov_cmd_name = b_atomize_f(ctx, "run_coverage_%s", name);
            BuildTarget* cov_cmd = b_cmd_target_prog(ctx, cov_cmd_name->data, cov_bin);
            cov_cmd->is_phony = 1;
            cov_cmd->user_bits |= EXCLUDE_FROM_MAKEFILE;
            if(test_files[i].needs_lffi && ffi_dll)
                b_add_dep(ctx, cov_cmd, ffi_dll);
            if(test_files[i].needs_drc_path){
                b_arginp(ctx, cov_cmd, "--drc", cc_cov);
                b_arg(ctx, cov_cmd, "--covdir");
                b_argf(ctx, cov_cmd, "%s/drc_test_coverage", ctx->build_dir->data);
            }
            b_arg(ctx, cov_cmd, "--multithreaded");
            b_add_dep(ctx, coverage_tests, cov_cmd);
        }
        Atom cov_dir_name = b_atomize_f(ctx, "%s/coverage", ctx->build_dir->data);
        BuildTarget* coverage_dir = b_directory_target(ctx, cov_dir_name->data);
        BuildTarget* coverage = b_cmd_target(ctx, "coverage", BUILD_OS==OS_WINDOWS?"py":"python3");
        coverage->is_phony = 1;
        b_add_deps(ctx, coverage, coverage_tests, coverage_dir);
        b_args(ctx, coverage, "Tools/coverage.py", "--root", ".",
            "--merge-mode-functions=merge-use-line-0",
            "--exclude", "Drp/", "--exclude", "Vendored/", "--exclude", ".*_test\\.c",
            "--exclude", "cc\\.c",
            "--exclude", "cpp\\.c",
            "--exclude", "cpp_args\\.h",
            "--exclude", "cc_repl_completion\\.h",
            "--markdown");
        b_argf(ctx, coverage, "--txt=%s/coverage/coverage.txt", ctx->build_dir->data);
        b_argf(ctx, coverage, "--html-details=%s/coverage/index.html", ctx->build_dir->data);
        b_argf(ctx, coverage, "--object-directory=%s", ctx->build_dir->data);
    }
    {
        // TODO: there should be a helper in drbuild for custom compilers.
        const char* ext = BUILD_OS == OS_WINDOWS ? ".exe" : "";
        Atom fuzz_name = b_atomize_f(ctx, "%s/cc_fuzz%s", ctx->build_dir->data, ext);
        BuildTarget* fuzz = b_cmd_target(ctx, fuzz_name->data, ctx->target.fuzz_cc->length? ctx->target.fuzz_cc->data:"clang");
        fuzz->is_binary = 1;
        fuzz->is_compile_command = 1;
        b_args(ctx, fuzz,
            "-g", "-O1", "-march=native",
            "-fsanitize=fuzzer,address,undefined",
            "-fno-sanitize-recover=undefined");
        if(ctx->target.fuzz_sysroot->length)
            b_argf(ctx, fuzz, "--sysroot=%s", ctx->target.fuzz_sysroot->data);
        b_src_inp(ctx, fuzz, "C/cc_fuzz.c");
        b_args(ctx, fuzz, "-o", fuzz_name->data);
        b_args(ctx, fuzz, "-MT", fuzz_name->data, "-MMD", "-MP", "-MF");
        b_argf(ctx, fuzz, "%s/cc_fuzz.deps", ctx->deps_dir->data);
        BuildTarget* fuzz_phony = b_phony_target(ctx, "cc_fuzz");
        b_add_dep(ctx, fuzz_phony, fuzz);
        Atom corpus_name = b_atomize_f(ctx, "%s/fuzz_corpus", ctx->build_dir->data);
        BuildTarget* corpus_dir = b_directory_target(ctx, corpus_name->data);
        BuildTarget* run_fuzz = b_exec_target(ctx, "run_cc_fuzz", fuzz);
        run_fuzz->is_phony = 1;
        b_add_dep(ctx, run_fuzz, corpus_dir);
        b_aarg(ctx, run_fuzz, corpus_name);
        b_args(ctx, run_fuzz, "-max_len=10000");
        b_argf(ctx, run_fuzz, "-fork=%d", b_num_cpus());
        if(ctx->dash_dash_args.count){
            for(size_t j = 0; j < ctx->dash_dash_args.count; j++)
                b_aarg(ctx, run_fuzz, ctx->dash_dash_args.data[j]);
        }
    }

    BuildTarget* selfhost;
    {
        selfhost = b_cmd_target_prog(ctx, "selfhost", cc_opt);
        selfhost->is_phony = 1;
        if(ffi_dll) b_add_dep(ctx, selfhost, ffi_dll);
        b_args(ctx, selfhost, "cc.c", "Samples/hello.c");
        if(BUILD_OS != OS_WINDOWS)
            b_add_dep(ctx, selftests, selfhost);

        for(size_t i = 0; i < sizeof test_files / sizeof test_files[0]; i++){
            if(test_files[i].skip_self_hosted) continue;
            Atom name = b_atomize_f(ctx, "self_%s", test_files[i].name);
            BuildTarget* cmd = b_cmd_target_prog(ctx, name->data, cc_opt);
            cmd->is_phony = 1;
            if(test_files[i].needs_lffi && ffi_lib)
                b_arg(ctx, cmd, "-IFetched/libffi");
            b_arg(ctx, cmd, test_files[i].file);
            if(test_files[i].needs_drc_path)
                b_arginp(ctx, cmd, "--drc", cc_opt);
            if(!ctx->dash_dash_args.count)
                b_arg(ctx, cmd, "--multithreaded");
            else
                for(size_t j = 0; j < ctx->dash_dash_args.count; j++)
                    b_aarg(ctx, cmd, ctx->dash_dash_args.data[j]);
            b_add_dep(ctx, selftests, cmd);
        }
        b_add_dep(ctx, test, selftests);
    }

    {
        BuildTarget* bins[] = {cpp, cc};
        const char* names[] = {"drcpp", "drc"};
        for(size_t i = 0; i < sizeof bins / sizeof bins[0]; i++){
            BuildTarget* bin = bins[i];
            const char* name = names[i];

            Atom run_name = b_atomize_f(ctx, "run_%s", name);
            BuildTarget* run = b_exec_target(ctx, run_name->data, bin);
            run->is_phony = 1;
            if(ffi_dll && bin == cc)
                b_add_dep(ctx, run, ffi_dll);
            for(size_t j = 0; j < ctx->dash_dash_args.count; j++)
                b_aarg(ctx, run, ctx->dash_dash_args.data[j]);

            Atom debug_name = b_atomize_f(ctx, "debug_%s", name);
            BuildTarget* debug = b_cmd_target(ctx, debug_name->data, "lldb");
            debug->should_exec = 1;
            debug->is_phony = 1;
            b_inp(ctx, debug, bin);
            b_args(ctx, debug, "-o", "run", "--batch");
            if(ctx->dash_dash_args.count){
                b_arg(ctx, debug, "--");
                for(size_t j = 0; j < ctx->dash_dash_args.count; j++)
                    b_aarg(ctx, debug, ctx->dash_dash_args.data[j]);
            }
        }
    }
    {
        BuildTarget* repl = b_exec_target(ctx, "repl", cc);
        repl->is_phony = 1;
        b_arg(ctx, repl, "--repl");
        for(size_t j = 0; j < ctx->dash_dash_args.count; j++)
            b_aarg(ctx, repl, ctx->dash_dash_args.data[j]);
    }
    {
        static BuildTarget* bins[2]; bins[0] = cpp; bins[1] = cc;
        BuildTarget* install = b_script_target(ctx, "install", do_install, bins);
        install->is_phony = 1;
        b_add_dep(ctx, install, cpp);
        b_add_dep(ctx, install, cc);
    }
    {
        BuildTarget* tags = b_cmd_target(ctx, "tags", BUILD_OS==OS_WINDOWS?"py":"python3");
        tags->is_phony = 1;
        b_arg(ctx, tags, "Tools/ct.py");
        BuildTarget* compile_commands_json = b_get_target(ctx, "compile_commands.json");
        b_add_dep(ctx, tags, compile_commands_json);
    }
    {
        BuildTarget* docs = b_phony_target(ctx, "docs");
        static const char* doc_files[] = {"README", "EXTENSIONS"};
        for(size_t i = 0; i < sizeof doc_files / sizeof doc_files[0]; i++){
            Atom md_name = b_atomize_f(ctx, "%s.md", doc_files[i]);
            BuildTarget* out = b_targeta(ctx, md_name);
            out->is_generated = 1;
            out->user_bits |= EXCLUDE_FROM_MAKEFILE;
            Atom cmd_name = b_atomize_f(ctx, "compile_%s", doc_files[i]);
            BuildTarget* cmd = b_cmd_target(ctx, cmd_name->data, "dndc");
            cmd->user_bits |= EXCLUDE_FROM_MAKEFILE;
            Atom dnd_name = b_atomize_f(ctx, "%s.dnd", doc_files[i]);
            b_src_inp(ctx, cmd, dnd_name->data);
            b_args(ctx, cmd, "--md", "--no-css");
            b_argout(ctx, cmd, "-o", out);
            b_add_dep(ctx, docs, out);
        }
    }
    b_get_target(ctx, "fish-completions")->user_bits |= EXCLUDE_FROM_MAKEFILE;
    return b_execute_targets(ctx);
}

static
int
do_install(BuildCtx* ctx, BuildTarget* tgt){
    BuildTarget*const* bins = (BuildTarget*const*)tgt->user_data;
    Atom prefix = ctx->target.prefix != nil_atom ? ctx->target.prefix : env_getenv2(&ctx->env, "PREFIX", sizeof "PREFIX" - 1);
    Atom destdir = ctx->target.destdir != nil_atom ? ctx->target.destdir : env_getenv2(&ctx->env, "DESTDIR", sizeof "DESTDIR" - 1);
    Atom bindir = ctx->target.bindir != nil_atom ? ctx->target.bindir : env_getenv2(&ctx->env, "bindir", sizeof "bindir" - 1);
    if(!prefix && !bindir && BUILD_OS == OS_WINDOWS){
        b_loglvl(BLOG_ERROR, ctx, "PREFIX or bindir must be set on Windows for install\n");
        return 1;
    }
    if(!bindir)
        bindir = b_atomize_f(ctx, "%s/bin", prefix ? prefix->data : "/usr/local");
    if(destdir)
        bindir = b_atomize_f(ctx, "%s%s", destdir->data, bindir->data);
    int err;
    if(BUILD_OS == OS_WINDOWS){
        err = b_mkdirs_if_not_exists(ctx, AT_to_LS(bindir));
        if(err) return err;
        for(int i = 0; i < 2; i++){
            BuildTarget* c = bins[i];
            Atom dst = b_atomize_f(ctx, "%s/%s", bindir->data, path_basename(AT_to_SV(c->name), BUILD_OS==OS_WINDOWS).text);
            err = b_copy_file(ctx, c->name->data, dst->data);
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
    if(b_file_info_uncached(ctx, "Fetched/libffi/" LIBFFI_LIB, sizeof "Fetched/libffi/" LIBFFI_LIB - 1)->exists)
        goto finish;
    if(!b_file_info_uncached(ctx, "Fetched", sizeof "Fetched" - 1)->exists){
        b_log(ctx, "mkdir Fetched\n");
        int err = b_mkdir_if_not_exists(ctx, "Fetched");
        if(err) return BERROR;
    }
    {
        int err = b_mkdir_if_not_exists(ctx, "Fetched/libffi");
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
        int err = b_rm_file(ctx, "Fetched/libffi.zip");
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
    int err = b_copy_file(ctx, "Fetched/libffi/" LIBFFI_DLL, dst->data);
    if(err)
        b_loglvl(BLOG_ERROR, ctx, "Failed to copy " LIBFFI_DLL "\n");
    return err;
}

static
void
link_libffi(BuildCtx* ctx, BuildTarget* tgt, enum OS os, BuildTarget* _Null_unspecified ffi_lib){
    if(os == OS_WINDOWS || (os == OS_NATIVE && BUILD_OS == OS_WINDOWS)){
        b_arg(ctx, tgt, "-IFetched/libffi");
        b_linkinp(ctx, tgt, ffi_lib);
    }
    else {
        if(os == OS_LINUX || (os == OS_NATIVE && BUILD_OS == OS_LINUX))
            b_linkarg(ctx, tgt, "-ldl");
        b_linkarg(ctx, tgt, "-lffi");
    }
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
