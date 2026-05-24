//
// Copyright © 2025-2025, David Priver <david@davidpriver.com>
//
#ifndef DRP_BUILD_H
#define DRP_BUILD_H
//
// An implementation of a "nobuild" build system for C.
// Provides a system to describe your build graph in C and rebuild any parts
// when needed.
//
// Features include:
//   - Rebuilds and execs itself if it detects the build program or any
//     of its includes has changed.
//   - Incremental builds (can parse .d files produced by gcc/clang and
//     soure dependecies json files produced by cl.
//   - Detects non-source file changes that would imply rebuilds.
//     - When a command is executed to build a target, the command-line
//       is recorded. The command-line is then diffed when considering whether
//       a target needs to be rebuilt.
//   - Provides a standard but extensible CLI to the build system (handles argparsing).
//      - Certain configuration arguments are recorded so you don't have to repeat CLI
//        args each time.
//   - Out-of-source builds.
//   - Custom command support
//     - Write the command in C (write a C function).
//     - Custom commands can be async, use the BCoro abstraction to yield to
//       the build system when you need to wait for a command to finish.
//   - Cross-compiling supported.
//   - Remembers working directory so you can execute from anywhere once bootstrapped.
//   - Parallel builds (subprocess-based parallelism, build program is single-threaded).
//   - Can generate compile_commands.json
//   - Auto-bootrapping.

//
// Usage:
// -----
// This system liberally depends on the rest of "Drp" and is not currently a single header
// system. The idea is that you are using "Drp" anyway in your project.
//
#ifdef DRP_BUILD_EXAMPLE
// Make a 'build.c' file with the following contents:
//
// To bootstrap (posix):
// ```console
// $ cc build.c -o build
// $ ./build -b builddir
// # Now that we're bootstrapped, you can just rebuild as:
// $ ./build
// ```
//
// To bootstrap (windows):
// ```console
// cl build.c /std:c11
// build -b builddir
// :: Now that we're bootstrapped, you can just rebuild as:
// build
// ```

//
// Defining this X-macro allows you to add extra fields to the BuildTargetSettings (ctx->target).
// It will be configurable in the CLI and is remembered for each invocation.
//
// This example is for information needed for cross-compiling to windows targeting msvc.
//
// X(type, field, cli, help, default_value)
#define TARGET_SETTINGS_EXTRA_FIELDS(X) \
    X(Atom, winsdkinc, "--winsdkinc", "Windows sdk include. Include the version number. For cross-compilation.", nil_atom) \
    X(Atom, winsdklib, "--winsdklib", "Windows sdk libs. Include the version number. For cross-compilation.", nil_atom) \
    X(Atom, msvcinc, "--msvcinc", "Path to msvc include. For cross-compilation.", nil_atom) \
    X(Atom, msvclib, "--msvclib", "Path to msvc libs. For cross-compilation.", nil_atom) \

#include "Drp/drbuild.h"

static int prep_targets(BuildCtx*);

int main(int argc, char** argv, char** envp){
    // b_build_ctx() returns a fully initialized ctx or NULL on error.
    // b_build_ctx will rebuild this program if needed and exec the resulting
    // binary so this program is always up-to-date and you don't have to
    // re-bootstrap.
    BuildCtx* ctx = b_build_ctx(argc, argv, envp, __FILE__);
    if(!ctx) return 1;
    // Can also just prep targets in main.
    int err = prep_targets(ctx);
    if(err) return err;
    return b_execute_targets(ctx);
}

static
int
prep_targets(BuildCtx* ctx){
    BuildTarget* all = b_phony_target(ctx, "all");
    // For tools that only run on the build machine, pass OS_NATIVE instead of ctx->target.os
    BuildTarget* cg = b_exe_target(ctx, "codegen", "Tools/codegen.c", OS_NATIVE);
    BuildTarget* codegen = b_cmd_target_prog(ctx, "generate code", cg); // cg is the program invoked by codegen
    b_src_inp(ctx, codegen, "decls.idl");
    // decls_h is a generated source file
    BuildTarget* decls_h = b_gen_src_file(ctx, "decls.h");
    b_argout(ctx, codegen, "-o", decls_h);

    enum OS target_os = ctx->target.os;
    if(target_os == OS_NATIVE)
        target_os = BUILD_OS;
    BuildTarget* game = b_exe_target(ctx, "game", "main.c", target_os);
    b_add_dep(ctx, all, game);
    // For other includes, we can pick up the dependency from parsing compiler
    // output. But as this is generated, we need to know about the dependency *before*
    // we compile the target.
    b_add_dep(ctx, game, decls_h);

    if(target_os == OS_WINDOWS){
        if(BUILD_OS != OS_WINDOWS){ // cross-compiling
            if(ctx->target.compiler_flavor != COMPILER_CLANG){
                b_loglvl(BLOG_ERROR, ctx, "Cross-compilation to windows only supported with clang\n");
                return 1;
            }
            int werr = 0;
            #define WINCHECK(f) do { \
                if(!ctx->target.f || ctx->target.f == nil_atom){ \
                    b_loglvl(BLOG_ERROR, ctx, "Must specify " #f " when cross-compiling with clang.\n"); \
                    werr++; \
                } \
            }while(0);
            WINCHECK(winsdkinc);
            WINCHECK(winsdklib);
            WINCHECK(msvcinc);
            WINCHECK(msvclib);
            #undef WINCHECK
            if(werr) return 1;
            b_args(ctx, game, "-Wl,/subsystem:windows,/entry:mainCRTStartup");
            b_args(ctx, game, "-fuse-ld=lld");
            b_args(ctx, game, "--no-standard-libraries");
            const char* winc = ctx->target.winsdkinc->data;
            b_argf(ctx, game, "-I%s/ucrt", winc);
            b_argf(ctx, game, "-I%s/shared", winc);
            b_argf(ctx, game, "-I%s/um", winc);
            b_argf(ctx, game, "-I%s/winrt", winc);
            const char* minc = ctx->target.msvcinc->data;
            b_argf(ctx, game, "-I%s", minc);
            const char* wlib = ctx->target.winsdklib->data;
            b_argf(ctx, game, "-L%s/um/x64", wlib);
            b_argf(ctx, game, "%s/um/x64/kernel32.Lib", wlib);
            b_argf(ctx, game, "%s/um/x64/Uuid.Lib", wlib);
            b_argf(ctx, game, "%s/ucrt/x64/libucrt.lib", wlib);
            const char* mlib = ctx->target.msvclib->data;
            b_argf(ctx, game, "%s/x64/libvcruntime.lib", mlib);
            b_argf(ctx, game, "%s/x64/libcmt.lib", mlib);
        }
        else {
            b_args(ctx, game, "/link", "/subsystem:windows", "/entry:mainCRTStartup");
        }
    }
    return 0;
}

#include "Drp/drbuild.c"
#include "Drp/Allocators/allocator.c"
#endif

#ifndef STB_SPRINTF_STATIC
#define STB_SPRINTF_STATIC
#endif
#ifndef DRJSON_API
#define DRJSON_API static
#endif
#include "windowsheader.h"
#include "posixheader.h"
#include <stdint.h>
#include <stdarg.h>
#include "atom.h"
#include "atom_table.h"
#include "atom_map.h"
#include "env.h"
#include "Allocators/arena_allocator.h"
#include "stringview.h"
#include "long_string.h"
#include "cmd_builder.h"
#include "MStringBuilder.h"
#include "msb_sprintf.h"
#include "bcoro.h"

#ifndef MARRAY_T_Atom
#define MARRAY_T_Atom
#define MARRAY_T Atom
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wnullability-completeness"
#endif
#include "Marray.h"
#ifdef __clang__
#pragma clang diagnostic pop
#endif
#endif

#ifdef __clang__
#pragma clang assume_nonnull begin
#endif

static inline LongString AT_to_LS(Atom a){ return (LongString){a->length, a->data}; }
static inline StringView AT_to_SV(Atom a){ return (StringView){a->length, a->data}; }

//
//

enum OS {
// ------
// An enumeration listing supported operating systems, of either the build
// machine or the host machine.
//
// Note the special value OS_NATIVE, which means the OS of the build machine.
//
#define XOS(X) \
    X(NATIVE,  "native") \
    X(APPLE,   "macos") \
    X(LINUX,   "linux") \
    X(WINDOWS, "windows") \

#define X(e, s) OS_##e,
    XOS(X)
#undef X
    OS__MAX,
};


static const StringView OSSVs[] = {
#define X(e, s) [OS_##e] = SVI(s),
    XOS(X)
#undef X
};

#if defined(_WIN32)
static const enum OS BUILD_OS = OS_WINDOWS;
#elif defined(__linux__)
static const enum OS BUILD_OS = OS_LINUX;
#elif defined(__APPLE__)
static const enum OS BUILD_OS = OS_APPLE;
#else
static const enum OS BUILD_OS = OS_OTHER;
#endif

enum ArchFam {
// ------------
// Arch family 
//   NATIVE: architecture of the build machine.
//   x86:    Intel family processors
//   ARM:    A modernish arm processor. 
//           This is underspecified, practically means apples silicon.
//   APPLE_UNIVERSAL: Both x64 and arm64.
//         
//
#define XARCHFAM(X) \
    X(NATIVE, "native") \
    X(x86, "x86") \
    X(ARM, "arm") \
    X(APPLE_UNIVERSAL, "universal-x64-arm64") \

#define X(e, s) AFAM_##e,
    XARCHFAM(X)
#undef X
    AFAM__MAX,
};

static const StringView ArchFamSVs[] = {
#define X(e, s) [AFAM_##e] = SVI(s),
    XARCHFAM(X)
#undef X
};

enum ArchBits {
// ------------
// Number of bits of the compilation target.
//   NATIVE: bitness of the build machine
//   32:     32bit
//   64:     64bit
#define XARCHBITS(X) \
    X(NATIVE, "native") \
    X(32, "32bit") \
    X(64, "64bit") \

#define X(e, s) ABITS_##e,
    XARCHBITS(X)
#undef X
    ABITS__MAX,
};

static const StringView ArchBitsSVs[] = {
#define X(e, s) [ABITS_##e] = SVI(s),
    XARCHBITS(X)
#undef X
};

enum CompilerFlavor {
// -----------------
// Flavor of Compiler. Used to determine what its command line looks like.
//   UNKNOWN: unknown C compiler, assumed to work like a posix "cc"
//   GCC: gcc compiler
//   GCC_MINGW: gcc but configured for the mingw target
//   CLANG: clang (might be apple clang)
//   CLANG_CL: clang-cl (clang pretending to be cl)
//   CL: Microsoft's C compiler.
//
#define XFLAVOR(X) \
    X(UNKNOWN, "cc") \
    X(GCC, "gcc") \
    X(GCC_MINGW, "gcc-mingw") \
    X(CLANG, "clang") \
    X(CLANG_CL, "clang-cl") \
    X(CL, "cl") \

#define X(e, s) COMPILER_##e,
    XFLAVOR(X)
#undef X
    COMPILER__MAX,
};
static const StringView CompilerFlavorSVs[] = {
#define X(e, s) [COMPILER_##e] = SVI(s),
    XFLAVOR(X)
#undef X
};

//
// Default build compiler implied by introspecting
// preset macros.
//
// Define this at the command-line when first bootstrapping the build
// to use a different compiler.
//
#ifndef DEFAULT_BUILD_COMPILER
#if defined __clang__ && defined __SSP_STRONG__ // fragile, but different
#define DEFAULT_BUILD_COMPILER "clang-cl"
#elif defined __clang__
#define DEFAULT_BUILD_COMPILER "clang"
#elif defined _MSC_VER
#define DEFAULT_BUILD_COMPILER "cl"
#elif defined __GNUC__
#define DEFAULT_BUILD_COMPILER "gcc"
#else
#define DEFAULT_BUILD_COMPILER "cc"
#endif
#endif

static
enum CompilerFlavor
b_guess_compiler_flavor(Atom cc);
// ---------------------------
// Guesses the flavor of the compiler based on its name.
//

typedef struct BuildTargetSettings BuildTargetSettings;
// ------------------------------------------
// Settings for configuring the final target program.
struct BuildTargetSettings {
    Atom cc;
    enum CompilerFlavor compiler_flavor;
    enum OS os;
    enum ArchFam arch;
    enum ArchBits bits;
    _Bool optimize;
    _Bool sanitize;
    _Bool native_sanitize;
    _Bool tsan;
    _Bool no_debug_symbols;
#ifdef TARGET_SETTINGS_EXTRA_FIELDS
#define X(type, field, cli, help, def) type field;
    TARGET_SETTINGS_EXTRA_FIELDS(X)
#undef X
#endif
};
// settings remembered between invocations
typedef struct GlobalCachedSettings GlobalCachedSettings;
struct GlobalCachedSettings {
    Atom build_cc;
    enum CompilerFlavor build_compiler_flavor;
    Atom build_dir;
    Atom cwd;
    Atom src_path;
    int32_t njobs;
};

typedef struct BuildTarget BuildTarget;
typedef struct BuildJob BuildJob;
struct BuildJob {
    BuildTarget* target;
    _Bool started;
    int status;
};

typedef struct BuildCtx BuildCtx;
struct BuildCtx {
    AtomTable at;
    AtomMap(FileInfo*) file_infos;
    AtomMap(BuildTarget*) targets;
    AtomMap(Marray(Atom)*) cmd_history;
    Environment env;
    ArenaAllocator tmp_aa;
    ArenaAllocator perm_aa;
    void* envp;
    Marray(Atom) build_targets;
    Marray(Atom) dash_dash_args; // args after the "--", for exec commands

    BuildTargetSettings target;
    union {
        GlobalCachedSettings cached;
        struct {
            Atom build_cc;
            enum CompilerFlavor build_compiler_flavor;
            Atom build_dir;
            Atom cwd;
            Atom src_path;
            int32_t njobs;
        };
    };

    Atom gen_dir;
    Atom deps_dir;
    Atom git_hash;
    Atom exe_path;
    Atom cache_path;
    Atom cmd_cache_path;
    Atom settings_cache_path;

    struct {
        intptr_t* processes;
        BuildJob* jobs;
        size_t count;
        size_t cap;
    } jobs;

    struct {
        intptr_t errhandle;
        intptr_t outhandle;
        int (*func)(intptr_t, const char*, size_t);
        int level;
    } logger;
};

enum BLogLevel {
#define XBLOGLEVEL(X) \
    X(DEBUG, "DEBUG") \
    X(INFO, "INFO") \
    X(WARN, "WARN") \
    X(ERROR, "ERROR") \

#define X(e, s) BLOG_##e,
    XBLOGLEVEL(X)
#undef X
    BLOG__MAX,
};

static const StringView BLogLevelSVs[] = {
#define X(e, s) [BLOG_##e] = SVI(s),
    XBLOGLEVEL(X)
#undef X
};

typedef struct BuildTarget BuildTarget;
struct BuildTarget {
    Atom name;
    Marray(Atom) dependencies;
    Marray(Atom) outputs;
    Marray(Atom) linker_args;
    enum CompilerFlavor compiler_flavor;
    CmdBuilder cmd;
    union {
        int (*script)(BuildCtx*, BuildTarget*);
        int (*corop)(BuildCtx*, BuildTarget*);
    };
    union {
        struct {
            // This target is a source file. It is always up to date (unless it is generated),
            // but things that depend on it are not up to date if it is newer than them.
            _Bool is_src: 1;
            // This target is generated input.
            _Bool is_generated: 1;
            // This target represents a compiled artifact
            _Bool is_binary: 1;
            // Run cmd to update to this target.
            _Bool is_cmd: 1;
            // Is up to date if all dependencies are up to date.
            _Bool is_phony: 1;
            _Bool is_script: 1;
            _Bool should_exec: 1;
            _Bool order_only: 1;
        };
        uint8_t _bits;
    };
    union {
        struct {
            // is a command that should be output to compile_commands.json
            _Bool is_compile_command: 1;
            _Bool is_coro: 1;
            uint8_t visit_state: 2;
            uint8_t _pad: 4;
        };
        uint8_t _bits2;
    };
    uint16_t user_bits;
    const void* user_data;
    BCoro coro;
};


static
BuildCtx*_Nullable
b_build_ctx(int argc, char*_Null_unspecified*_Nonnull argv, char*_Null_unspecified*_Nonnull envp, const char*_Nonnull basefile);

static
Atom
b_atomize(BuildCtx* ctx, const char*);

static
Atom
#ifdef __GNUC__
__attribute__((format(printf, 2, 3)))
#endif
b_atomize_f(BuildCtx* ctx, const char*, ...);

static
Atom
b_atomize_fv(BuildCtx* ctx, const char* fmt, va_list ap);

static
Atom
b_atomize2(BuildCtx* ctx, const char*, size_t len);


static
void
b_normalize_path(BuildCtx* ctx, StringView p, MStringBuilder* out);

static
Atom
b_normalize_patha(BuildCtx* ctx, Atom path);

#ifdef _WIN32
typedef FILETIME MTime;
#else
typedef struct timespec MTime;
#endif

typedef struct BuildFileInfo BuildFileInfo;
struct BuildFileInfo {
    MTime mtime;
    _Bool exists: 1;
    _Bool is_file: 1;
    _Bool valid: 1;
};

static
BuildFileInfo*
b_file_info(BuildCtx* ctx, const char* path, size_t length);

static
BuildFileInfo*
b_file_info_uncached(BuildCtx* ctx, const char* path, size_t length);

static
_Bool
b_file_exists(void* _ctx, const char* path, size_t length);

static
int
b_read_file(BuildCtx* ctx, const char* path, MStringBuilder* out);

static
void
#ifdef __GNUC__
__attribute__((format(printf, 2, 3)))
#endif
b_log(BuildCtx* ctx, const char* fmt, ...);

static
void
#ifdef __GNUC__
__attribute__((format(printf, 3, 4)))
#endif
b_loglvl(int, BuildCtx* ctx, const char* fmt, ...);

static int b_num_cpus(void);

static
void
b_loglvlv(int, BuildCtx* ctx, const char* fmt, va_list);

static
int
#ifdef __GNUC__
__attribute__((format(printf, 2, 3)))
#endif
b_printf(BuildCtx* ctx, const char* fmt, ...);

static
int
b_printfv(BuildCtx* ctx, const char* fmt, va_list vap);

static
int
b_mkdir_if_not_exists(BuildCtx* ctx, const char* path);

static
int
b_mkdirs_if_not_exists(BuildCtx* ctx, LongString path);

static
int
b_move_file(BuildCtx* ctx, const char* from, const char* to);

static
int
b_copy_file(BuildCtx* ctx, const char* from, const char* to);

static
int
b_copy_directory(BuildCtx* ctx, const char* from, const char* to);

static
int
b_rm_file(BuildCtx* ctx, const char* path);

static
int
b_rm_directory(BuildCtx* ctx, const char* path);

static
void
b_print_command(BuildCtx* ctx, CmdBuilder* cmd);

static
void
b_parse_depfiles(BuildCtx* ctx);

static
int
b_execute_targets(BuildCtx* ctx);

static Atom b_get_git_hash(BuildCtx*);

static inline
void
b_ta_pusha(BuildCtx* ctx, Marray(Atom)* m, Atom a);

static inline
BuildTarget*
b_targeta(BuildCtx* ctx, Atom name);

static inline
BuildTarget*
b_target(BuildCtx* ctx, const char* name);

static
inline
BuildTarget* _Nullable
b_get_target(BuildCtx* ctx, const char* name);

static
inline
BuildTarget* _Nullable
b_get_targeta(BuildCtx* ctx, Atom a);

static inline
BuildTarget*
b_src_filea(BuildCtx* ctx, Atom path);

static inline
BuildTarget*
b_src_file(BuildCtx* ctx, const char* src);

static inline
BuildTarget*
b_gen_src_file(BuildCtx* ctx, const char* src);

static inline
void
b_add_dep(BuildCtx* ctx, BuildTarget* tgt, BuildTarget* dep);

static inline
void
b_add_out(BuildCtx* ctx, BuildTarget* tgt, BuildTarget* out);

static inline
void
b_add_deps_(BuildCtx* ctx, BuildTarget* tgt, size_t dep_count, BuildTarget*_Nonnull*_Nonnull dep);

#define b_add_deps(ctx, tgt, ...) b_add_deps_(ctx, tgt, sizeof (BuildTarget*[]){__VA_ARGS__} / sizeof(BuildTarget*), (BuildTarget*[]){__VA_ARGS__})

static inline
void
b_add_src_depa(BuildCtx* ctx, BuildTarget* tgt, Atom dep);

static inline
void
b_add_src_dep(BuildCtx* ctx, BuildTarget* tgt, const char* dep);

static inline
BuildTarget*
b_cmd_target(BuildCtx* ctx, const char* name, const char* prog);

static inline
BuildTarget*
b_cmd_target_prog(BuildCtx* ctx, const char* name, BuildTarget* prog);

static inline
BuildTarget*
b_exec_target(BuildCtx* ctx, const char* name, BuildTarget*);

static inline
BuildTarget*
b_exe_target(BuildCtx* ctx, const char* name, const char* sr, enum OS target_os);

static inline
BuildTarget*
b_bin_target(BuildCtx* ctx, const char* name);

static inline
BuildTarget*
b_directory_target(BuildCtx* ctx, const char* name);

static
void
b_print_target(BuildCtx* ctx, BuildTarget* tgt);

static
void
b_prog(BuildCtx* ctx, BuildTarget* tgt, BuildTarget* prog);

static
void
b_inp(BuildCtx* ctx, BuildTarget* tgt, BuildTarget* inp);

static
BuildTarget*
b_phony_target(BuildCtx* ctx, const char* name);

static
BuildTarget*
b_phony_targeta(BuildCtx* ctx, Atom name);

static
void
b_inps_(BuildCtx* ctx, BuildTarget* tgt, size_t n, BuildTarget*_Nonnull*_Nonnull inp);
#define b_inps(ctx, tgt, ...) b_inps_(ctx, tgt, sizeof (BuildTarget*[]){__VA_ARGS__} / sizeof(BuildTarget*), (BuildTarget*[]){__VA_ARGS__})

static
void
b_src_inp(BuildCtx* ctx, BuildTarget* tgt, const char* inp_);
static
void
b_src_inps_(BuildCtx* ctx, BuildTarget* tgt, size_t n, const char*_Nonnull*_Nonnull inp_);
#define b_src_inps(ctx, tgt, ...) b_src_inps_(ctx, tgt, sizeof (const char*[]){__VA_ARGS__} / sizeof(const char*), (const char*[]){__VA_ARGS__})

static
void
b_arg(BuildCtx* ctx, BuildTarget* tgt, const char* arg);

static
void
b_aarg(BuildCtx* ctx, BuildTarget* tgt, Atom arg);

static
void
b_args_(BuildCtx* ctx, BuildTarget* tgt, size_t n, const char*_Nonnull*_Nonnull args);
#define b_args(ctx, tgt, ...) b_args_(ctx, tgt, sizeof (const char*[]){__VA_ARGS__} / sizeof(const char*), (const char*[]){__VA_ARGS__})

static
void
b_argf(BuildCtx* ctx, BuildTarget* tgt, const char* fmt, ...);

static
void
b_out(BuildCtx* ctx, BuildTarget* tgt, BuildTarget* out);

static
void
b_argout(BuildCtx* ctx, BuildTarget *tgt, const char* arg, BuildTarget* out);

static
void
b_arginp(BuildCtx* ctx, BuildTarget *tgt, const char* arg, BuildTarget* inp);

static
void
b_argsrc(BuildCtx* ctx, BuildTarget *tgt, const char* arg, const char* inp);

static
void
b_linkarg(BuildCtx* ctx, BuildTarget* tgt, const char* arg);

static
void
b_linkargf(BuildCtx* ctx, BuildTarget* tgt, const char* fmt, ...);

static
void
b_linkinp(BuildCtx* ctx, BuildTarget* tgt, BuildTarget* inp);

static
void
b_linkarginp(BuildCtx* ctx, BuildTarget* tgt, const char* arg, BuildTarget* inp);

static
void
b_linklib(BuildCtx* ctx, BuildTarget* tgt, const char* arg);

static inline
BuildTarget*
b_script_target(BuildCtx* ctx, const char* name, int (*script)(BuildCtx*, BuildTarget*), const void* _Null_unspecified);

static inline
BuildTarget*
b_script_targeta(BuildCtx* ctx, Atom name, int (*script)(BuildCtx*, BuildTarget*), const void* _Null_unspecified);

static inline
BuildTarget*
b_coro_target(BuildCtx* ctx, const char* name, int (*coro)(BuildCtx*, BuildTarget*), const void*_Null_unspecified ud);

static inline
BuildTarget*
b_coro_targeta(BuildCtx* ctx, Atom name, int (*coro)(BuildCtx*, BuildTarget*), const void*_Null_unspecified ud);

static
int
b_run_cmd_async(BuildCtx*, BuildTarget*, CmdBuilder*);

static
int
b_run_cmd_sync(BuildCtx* ctx, CmdBuilder* cmd);

static void b_debug_break(BuildCtx*, const char*);
// ----------------------
// Inserts a debug trap instruction, with the given reason.

static _Noreturn void b_abort(BuildCtx*, const char*);
// ----------------------
// Aborts the process, with the given reason.
//
static _Noreturn void b_oom(BuildCtx*);
// ------------------
// Aborts the process due to exhaustion of memory.

#ifdef __clang__
#pragma clang assume_nonnull end
#endif

#endif
