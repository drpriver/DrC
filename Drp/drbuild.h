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
// Defining this X-macro allows you to add extra fields to the TargetSettings (ctx->target).
// It will be configurable in the CLI and is remembered for each invocation.
//
// This example is for information needed for cross-compiling to windows targeting msvc.
//
// X(type, field, help, default_value)
#define TARGET_SETTINGS_EXTRA_FIELDS(X) \
    X(Atom, winsdkinc, "Windows sdk include. Include the version number. For cross-compilation.", nil_atom) \
    X(Atom, winsdklib, "Windows sdk libs. Include the version number. For cross-compilation.", nil_atom) \
    X(Atom, msvcinc, "Path to msvc include. For cross-compilation.", nil_atom) \
    X(Atom, msvclib, "Path to msvc libs. For cross-compilation.", nil_atom) \

#include "Drp/build.h"

static int prep_targets(BuildCtx*);

int main(int argc, char** argv, char** envp){
    // build_ctx() returns a fully initialized ctx or NULL on error.
    // build_ctx will rebuild this program if needed and exec the resulting
    // binary so this program is always up-to-date and you don't have to
    // re-bootstrap.
    BuildCtx* ctx = build_ctx(argc, argv, envp, __FILE__);
    if(!ctx) return 1;
    // Can also just prep targets in main.
    int err = prep_targets(ctx);
    if(err) return err;
    return execute_targets(ctx);
}

struct SdlDep {
    const char* name, *othername, *version;
};

static const struct SdlDep SDL = {"SDL2", "SDL", "2.30.9"};
// Async C script

static
int
prep_targets(BuildCtx* ctx){
    BuildTarget* all = phony_target(ctx, "all");
    // For tools that only run on the build machine, pass OS_NATIVE instead of ctx->target.os
    BuildTarget* cg = exe_target(ctx, "codegen", "Tools/codegen.c", OS_NATIVE);
    BuildTarget* codegen = cmd_target(ctx, "generate code");
    target_prog(ctx, codegen, cg); // cg is the program invoked by codegen
    target_src_inp(ctx, codegen, "decls.idl");
    // decls_h is a generated source file
    BuildTarget* decls_h = gen_src_file(ctx, "decls.h");
    target_argout(ctx, codegen, "-o", decls_h);

    enum OS target_os = ctx->target.os;
    if(target_os == OS_NATIVE)
        target_os = BUILD_OS;
    BuildTarget* game = exe_target(ctx, "game", "main,c", target_os);
    add_dep(ctx, all, game);
    // For other includes, we can pick up the dependency from parsing compiler
    // output. But as this is generated, we need to know about the dependency *before*
    // we compile the target.
    add_dep(ctx, game, decls_h);

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
            cmd_cargs(&game->cmd, "-Wl,/subsystem:windows,/entry:mainCRTStartup");
            cmd_cargs(&game->cmd, "-fuse-ld=lld");
            const char* winc = ctx->target.winsdkinc->data;
            const char* wlib = ctx->target.winsdklib->data;
            const char* minc = ctx->target.msvcinc->data;
            const char* mlib = ctx->target.msvclib->data;
            cmd_cargs(&game->cmd, "--no-standard-libraries");
            cmd_argf(&game->cmd, "-I%s/ucrt", winc);
            cmd_argf(&game->cmd, "-I%s/shared", winc);
            cmd_argf(&game->cmd, "-I%s/um", winc);
            cmd_argf(&game->cmd, "-I%s/winrt", winc);
            cmd_argf(&game->cmd, "-I%s", minc);
            cmd_argf(&game->cmd, "-L%s/um/x64", wlib);
            cmd_argf(&game->cmd, "%s/um/x64/kernel32.Lib", wlib);
            cmd_argf(&game->cmd, "%s/um/x64/Uuid.Lib", wlib);
            cmd_argf(&game->cmd, "%s/ucrt/x64/libucrt.lib", wlib);
            cmd_argf(&game->cmd, "%s/x64/libvcruntime.lib", mlib);
            cmd_argf(&game->cmd, "%s/x64/libcmt.lib", mlib);
        }
        else {
            cmd_cargs(&game->cmd, "/link", "/subsystem:windows", "/entry:mainCRTStartup");
        }
    }

}

#include "Drp/build.c"
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
guess_compiler_flavor(Atom cc);
// ---------------------------
// Guesses the flavor of the compiler based on its name.
//

typedef struct TargetSettings TargetSettings;
// ------------------------------------------
// Settings for configuring the final target program.
struct TargetSettings {
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
#define X(type, field, help, def) type field;
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

    TargetSettings target;
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
    const void* user_data;
    BCoro coro;
};


static
BuildCtx*_Nullable
build_ctx(int argc, char*_Null_unspecified*_Nonnull argv, char*_Null_unspecified*_Nonnull envp, const char*_Nonnull basefile);

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
mkdir_if_not_exists(BuildCtx* ctx, const char* path);

static
int
mkdirs_if_not_exists(BuildCtx* ctx, LongString path);

static
int
move_file(BuildCtx* ctx, const char* from, const char* to);

static
int
copy_file(BuildCtx* ctx, const char* from, const char* to);

static
int
copy_directory(BuildCtx* ctx, const char* from, const char* to);

static
int
rm_file(BuildCtx* ctx, const char* path);

static
int
rm_directory(BuildCtx* ctx, const char* path);

static
void
print_command(BuildCtx* ctx, CmdBuilder* cmd);

static
void
parse_depfiles(BuildCtx* ctx);

static
int
execute_targets(BuildCtx* ctx);

static Atom get_git_hash(BuildCtx*);

static inline
void
ta_pusha(BuildCtx* ctx, Marray(Atom)* m, Atom a);

static inline
BuildTarget*
alloc_targeta(BuildCtx* ctx, Atom name);

static inline
BuildTarget*
alloc_target(BuildCtx* ctx, const char* name);

static
inline
BuildTarget* _Nullable
get_target(BuildCtx* ctx, const char* name);

static
inline
BuildTarget* _Nullable
get_targeta(BuildCtx* ctx, Atom a);

static inline
BuildTarget*
src_filea(BuildCtx* ctx, Atom path);

static inline
BuildTarget*
src_file(BuildCtx* ctx, const char* src);

static inline
BuildTarget*
gen_src_file(BuildCtx* ctx, const char* src);

static inline
void
add_dep(BuildCtx* ctx, BuildTarget* tgt, BuildTarget* dep);

static inline
void
add_out(BuildCtx* ctx, BuildTarget* tgt, BuildTarget* out);

static inline
void
add_deps_(BuildCtx* ctx, BuildTarget* tgt, size_t dep_count, BuildTarget*_Nonnull*_Nonnull dep);

#define add_deps(ctx, tgt, ...) add_deps_(ctx, tgt, sizeof (BuildTarget*[]){__VA_ARGS__} / sizeof(BuildTarget*), (BuildTarget*[]){__VA_ARGS__})

static inline
void
add_src_depa(BuildCtx* ctx, BuildTarget* tgt, Atom dep);

static inline
void
add_src_dep(BuildCtx* ctx, BuildTarget* tgt, const char* dep);

static inline
BuildTarget*
cmd_target(BuildCtx* ctx, const char* name);

static inline
BuildTarget*
exec_target(BuildCtx* ctx, const char* name, BuildTarget*);

static inline
BuildTarget*
exe_target(BuildCtx* ctx, const char* name, const char* sr, enum OS target_os);

static inline
BuildTarget*
bin_target(BuildCtx* ctx, const char* name);

static inline
BuildTarget*
directory_target(BuildCtx* ctx, const char* name);

static
void
print_target(BuildCtx* ctx, BuildTarget* tgt);

static
void
target_prog(BuildCtx* ctx, BuildTarget* tgt, BuildTarget* prog);

static
void
target_inp(BuildCtx* ctx, BuildTarget* tgt, BuildTarget* inp);

static
BuildTarget*
phony_target(BuildCtx* ctx, const char* name);

static
BuildTarget*
phony_targeta(BuildCtx* ctx, Atom name);

static
void
target_inps_(BuildCtx* ctx, BuildTarget* tgt, size_t n, BuildTarget*_Nonnull*_Nonnull inp);
#define target_inps(ctx, tgt, ...) target_inps_(ctx, tgt, sizeof (BuildTarget*[]){__VA_ARGS__} / sizeof(BuildTarget*), (BuildTarget*[]){__VA_ARGS__})

static
void
target_src_inp(BuildCtx* ctx, BuildTarget* tgt, const char* inp_);
static
void
target_src_inps_(BuildCtx* ctx, BuildTarget* tgt, size_t n, const char*_Nonnull*_Nonnull inp_);
#define target_src_inps(ctx, tgt, ...) target_src_inps_(ctx, tgt, sizeof (const char*[]){__VA_ARGS__} / sizeof(const char*), (const char*[]){__VA_ARGS__})
static
void
target_arg(BuildCtx* ctx, BuildTarget* tgt, const char* arg);

static
void
target_argf(BuildCtx* ctx, BuildTarget* tgt, const char* fmt, ...);

static
void
target_out(BuildCtx* ctx, BuildTarget* tgt, BuildTarget* out);

static
void
target_argout(BuildCtx* ctx, BuildTarget *tgt, const char* arg, BuildTarget* out);

static
void
target_arginp(BuildCtx* ctx, BuildTarget *tgt, const char* arg, BuildTarget* inp);

static
void
target_argsrc(BuildCtx* ctx, BuildTarget *tgt, const char* arg, const char* inp);

static
void
target_linkarg(BuildCtx* ctx, BuildTarget* tgt, const char* arg);

static
void
target_linkargf(BuildCtx* ctx, BuildTarget* tgt, const char* fmt, ...);

static
void
target_linkinp(BuildCtx* ctx, BuildTarget* tgt, BuildTarget* inp);

static
void
target_linkarginp(BuildCtx* ctx, BuildTarget* tgt, const char* arg, BuildTarget* inp);

static
void
target_linklib(BuildCtx* ctx, BuildTarget* tgt, const char* arg);

static inline
BuildTarget*
script_target(BuildCtx* ctx, const char* name, int (*script)(BuildCtx*, BuildTarget*), const void* _Null_unspecified);

static inline
BuildTarget*
script_targeta(BuildCtx* ctx, Atom name, int (*script)(BuildCtx*, BuildTarget*), const void* _Null_unspecified);

static inline
BuildTarget*
coro_target(BuildCtx* ctx, const char* name, int (*coro)(BuildCtx*, BuildTarget*), const void*_Null_unspecified ud);

static inline
BuildTarget*
coro_targeta(BuildCtx* ctx, Atom name, int (*coro)(BuildCtx*, BuildTarget*), const void*_Null_unspecified ud);

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
