#ifndef DRP_BUILD_C
#define DRP_BUILD_C
#define TI_NO_STDIO
#define AP_NO_STDIO
#include "drbuild.h"
#include "Allocators/mallocator.h"
#include "MStringBuilder.h"
#include "argparse_atom.h"
#include "argument_parsing.h"
#include "atomf.h"
#include "drjson.h"
#include "msb_sprintf.h"
#include "path_util.h"
#include "typeinfo.h"
#include "typeinfo_jsondeser.h"
#include "typeinfo_print.h"
#include "cmd_run.h"
#include "MStringBuilder16.h"
#include "offsetof.h"
#include "bcoro.h"

#ifdef __APPLE__
#include <mach-o/dyld.h> // For _NSGetExecutablePath
#include <copyfile.h>
#endif

#ifndef _WIN32
#include <glob.h>
#endif

#ifdef __linux__
#include <sys/types.h>
#endif

#ifndef __builtin_debugtrap
#if defined(__GNUC__) && ! defined(__clang__)
#define __builtin_debugtrap() __builtin_trap()
#elif defined(_MSC_VER)
#define __builtin_debugtrap() __debugbreak()
#endif
#endif

#ifndef __builtin_trap
#if defined(_MSC_VER) && !defined(__clang__)
#define __builtin_trap() __fastfail(5)
#endif
#endif


#ifdef __clang__
#pragma clang assume_nonnull begin
#endif

#ifdef __linux__
ssize_t copy_file_range(int fd_in, loff_t* off_in, int fd_out, loff_t* off_out, size_t len, unsigned int flags);
#endif

#define APET(E) {.enum_size = sizeof(enum E), .enum_count=arrlen(E##SVs), .enum_names=E##SVs,}
static const ArgParseEnumType OSEnum = APET(OS);
static const ArgParseEnumType CompilerFlavorEnum = APET(CompilerFlavor);
static const ArgParseEnumType ArchFamEnum = APET(ArchFam);
static const ArgParseEnumType ArchBitsEnum = APET(ArchBits);
static const ArgParseEnumType LogLevelEnum = APET(BLogLevel);
static
int
b_cmp_mtime(const MTime* a, const MTime* b);

//
// Logically "removes" a section of an array by shifting the stuff after
// it forward.
//
// This is less error prone than doing the pointer arithmetic at each call
// site. You can pass a buffer + length and where and how much to remove.
//
// Arguments:
// ----------
// whence:
//   The offset to the beginning of the section to remove, in bytes.
//
// buff:
//   The buffer to remove the section from.
//
// bufflen:
//   The length of the buffer, in bytes.
//
// nremove:
//   Length of the section to remove, in bytes.
//
static inline
void
b_memremove(size_t whence, void* buff, size_t bufflen, size_t nremove){
    if(nremove + whence > bufflen) __builtin_debugtrap();
    size_t tail = bufflen - whence - nremove;
    char* p = buff;
    if(tail) memmove(p+whence, p+whence+nremove, tail);
}
static int b_num_cpus(void);
#ifndef _WIN32
static
int
b_num_cpus(void){
    int num = (int)sysconf(_SC_NPROCESSORS_ONLN);
    return num;
}
#else
static
int
b_num_cpus(void){
    SYSTEM_INFO sysinfo;
    GetSystemInfo(&sysinfo);
    int num = sysinfo.dwNumberOfProcessors;
    return num;
}
#endif

static
void
b_normalize_path(BuildCtx* ctx, StringView p, MStringBuilder* out){
    if(!path_is_abspath(p, BUILD_OS == OS_WINDOWS)){
        msb_write_str(out, ctx->cwd->data, ctx->cwd->length);
        msb_write_char(out, BUILD_OS==OS_WINDOWS?'\\':'/');
    }
    int slash_distance = 0;
    int ndots = 0;
    for(size_t i = 0; i < p.length; i++){
        char c = p.text[i];
        if(is_sep(c, BUILD_OS==OS_WINDOWS)){
            if(out->cursor && !slash_distance) continue;
            if(slash_distance == 1 && ndots == 1){
                out->cursor -= 2;
            }
            if(slash_distance == 2 && ndots == 2){
                out->cursor -= 3;
                while(out->cursor && !is_sep(out->data[--out->cursor], BUILD_OS==OS_WINDOWS))
                    ;
            }
            msb_write_char(out, BUILD_OS==OS_WINDOWS?'\\':'/');
            slash_distance = 0;
            ndots = 0;
        }
        // this is wrong, it depends on the filesystem, not the os, but whatever
        else if(c >= 'A' && c <= 'Z' && (BUILD_OS == OS_WINDOWS || BUILD_OS==OS_APPLE)){
            msb_write_char(out, c | 0x20);
            ndots = 0;
            slash_distance++;
        }
        else {
            if(c == '.')
                ndots++;
            else
                ndots = 0;
            slash_distance++;
            msb_write_char(out, c);
        }
    }
    StringView sv = msb_borrow_sv(out);
    if(sv_istartswith(sv, (StringView){ctx->cwd->length, ctx->cwd->data})){
        if(sv.length == ctx->cwd->length){
            msb_reset(out);
            msb_write_literal(out, ".");
            return;
        }
        if(is_sep(sv.text[ctx->cwd->length], BUILD_OS==OS_WINDOWS)){
            b_memremove(1, out->data, out->cursor, ctx->cwd->length-1);
            out->data[0] = '.';
            out->cursor -= ctx->cwd->length-1;
        }
    }
}

static
Atom
b_normalize_patha(BuildCtx* ctx, Atom path){
    MStringBuilder tmp = {.allocator = allocator_from_arena(&ctx->tmp_aa)};
    StringView p = {path->length, path->data};
    b_normalize_path(ctx, p, &tmp);
    StringView sv = msb_borrow_sv(&tmp);
    Atom out = b_atomize2(ctx, sv.text, sv.length);
    msb_destroy(&tmp);
    return out;
}

static
enum CompilerFlavor
guess_compiler_flavor(Atom cc_){
    StringView cc = AT_to_SV(cc_);
    if(sv_contains(cc, SV("gcc"))){
        if(sv_contains(cc, SV("mingw")))
            return COMPILER_GCC_MINGW;
        return COMPILER_GCC;
    }
    if(sv_contains(cc, SV("clang-cl")))
        return COMPILER_CLANG_CL;
    if(sv_contains(cc, SV("clang")))
        return COMPILER_CLANG;
    if(sv_contains(cc, SV("cl")))
        return COMPILER_CL;
    return COMPILER_UNKNOWN;
}


static
BuildFileInfo*
b_file_info(BuildCtx* ctx, const char* path, size_t length){
    Atom key = b_atomize2(ctx, path, length);
    key = b_normalize_patha(ctx, key);
    BuildFileInfo* fi = AM_get(&ctx->file_infos, key);
    if(fi){
        if(fi->valid) return fi;
    }
    else {
        fi = Allocator_zalloc(allocator_from_arena(&ctx->perm_aa), sizeof *fi);
        if(!fi) b_oom(ctx);

        {
            int err = AM_put(&ctx->file_infos, allocator_from_arena(&ctx->perm_aa), key, fi);
            if(err) b_oom(ctx);
        }
    }
    if(BUILD_OS==OS_WINDOWS){
        MStringBuilder16 sb = {.allocator=allocator_from_arena(&ctx->tmp_aa)};
        msb16_write_utf8(&sb, path, length);
        msb16_write_uint16_t(&sb, u'\0');
        if(sb.errored){
            fi->valid = 0;
        }
        else {
            #ifdef _WIN32
            HANDLE hnd = CreateFileW(
                (wchar_t*)sb.data,
                GENERIC_READ,
                FILE_SHARE_READ,
                NULL,
                OPEN_EXISTING,
                FILE_FLAG_BACKUP_SEMANTICS,
                NULL
            );
            if(hnd == INVALID_HANDLE_VALUE){
                fi->exists = 0;
            }
            else {
                fi->exists = 1;
                FILE_BASIC_INFO fileBasicInfo;
                BOOL ok = GetFileInformationByHandleEx(
                    hnd,
                    FileBasicInfo,
                    &fileBasicInfo,
                    sizeof fileBasicInfo
                );
                if(!ok){
                    fi->valid = 0;
                }
                else {
                    DWORD attributes = fileBasicInfo.FileAttributes;
                    if(!(attributes & FILE_ATTRIBUTE_DIRECTORY))
                        fi->is_file = 1;
                    ok = GetFileTime(hnd, NULL, NULL, &fi->mtime);
                    if(!ok) fi->valid = 0;
                }
                CloseHandle(hnd);
            }
            #endif
        }
        msb16_destroy(&sb);
    }
    else {
        #ifndef _WIN32
        struct stat s;
        int err = stat(path, &s);
        if(err){
            fi->exists = 0;
        }
        else {
            fi->exists = 1;
            fi->is_file = S_ISREG(s.st_mode);
            #ifdef __linux__
            fi->mtime = s.st_mtim;
            #elif defined(__APPLE__)
            fi->mtime = s.st_mtimespec;
            #else
            // TODO
            fi->mtime = s.st_mtim;
            #endif
        }
        #endif
    }
    fi->valid = 1;
    return fi;
}

static
BuildFileInfo*
b_file_info_uncached(BuildCtx* ctx, const char* path, size_t length){
    Atom key = b_atomize2(ctx, path, length);
    key = b_normalize_patha(ctx, key);
    BuildFileInfo* fi = AM_get(&ctx->file_infos, key);
    if(fi){
        if(fi->valid) return fi;
    }
    else {
        fi = Allocator_zalloc(allocator_from_arena(&ctx->perm_aa), sizeof *fi);
        if(!fi) b_oom(ctx);
        {
            int err = AM_put(&ctx->file_infos, allocator_from_arena(&ctx->perm_aa), key, fi);
            if(err) b_oom(ctx);
        }
    }
    if(BUILD_OS==OS_WINDOWS){
        MStringBuilder16 sb = {.allocator=allocator_from_arena(&ctx->tmp_aa)};
        msb16_write_utf8(&sb, path, length);
        msb16_write_uint16_t(&sb, u'\0');
        if(sb.errored){
            fi->valid = 0;
        }
        else {
            #ifdef _WIN32
            HANDLE hnd = CreateFileW(
                (wchar_t*)sb.data,
                GENERIC_READ,
                FILE_SHARE_READ,
                NULL,
                OPEN_EXISTING,
                FILE_FLAG_BACKUP_SEMANTICS,
                NULL
            );
            if(hnd == INVALID_HANDLE_VALUE){
                fi->exists = 0;
            }
            else {
                fi->exists = 1;
                FILE_BASIC_INFO fileBasicInfo;
                BOOL ok = GetFileInformationByHandleEx(
                    hnd,
                    FileBasicInfo,
                    &fileBasicInfo,
                    sizeof fileBasicInfo
                );
                if(!ok){
                    fi->valid = 0;
                }
                else {
                    DWORD attributes = fileBasicInfo.FileAttributes;
                    if(!(attributes & FILE_ATTRIBUTE_DIRECTORY))
                        fi->is_file = 1;
                    ok = GetFileTime(hnd, NULL, NULL, &fi->mtime);
                    if(!ok) fi->valid = 0;
                }
                CloseHandle(hnd);
            }
            #endif
        }
        msb16_destroy(&sb);
    }
    else {
        #ifndef _WIN32
        struct stat s;
        int err = stat(path, &s);
        if(err){
            fi->exists = 0;
        }
        else {
            fi->exists = 1;
            fi->is_file = S_ISREG(s.st_mode);
            #ifdef __linux__
            fi->mtime = s.st_mtim;
            #elif defined(__APPLE__)
            fi->mtime = s.st_mtimespec;
            #else
            // TODO
            fi->mtime = s.st_mtim;
            #endif
        }
        #endif
    }
    fi->valid = 0;
    return fi;
}

static
_Bool
b_file_exists(void* _ctx, const char* path, size_t length){
    BuildFileInfo* fi = b_file_info(_ctx, path, length);
    return fi->exists && fi->is_file;
}

static
int
b_read_file(BuildCtx* ctx, const char* path, MStringBuilder* out){
    int err = 0;
    if(BUILD_OS == OS_WINDOWS){
        MStringBuilder16 path_sb = {.allocator = allocator_from_arena(&ctx->tmp_aa)};
        #ifdef _WIN32
        HANDLE fh = INVALID_HANDLE_VALUE;
        #endif
        msb16_write_utf8(&path_sb, path, strlen(path));
        msb16_nul_terminate(&path_sb);
        if(path_sb.errored) goto wfinally;
        #ifdef _WIN32
        fh = CreateFileW((wchar_t*)path_sb.data, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if(fh == INVALID_HANDLE_VALUE){
            err = 1;
            goto wfinally;
        }
        uint64_t size;
        BOOL ok = GetFileSizeEx(fh, (LARGE_INTEGER*)&size);
        if(!ok){
            err = 1;
            goto wfinally;
        }
        err = msb_ensure_additional(out, size+1);
        if(err) goto wfinally;
        DWORD nread;
        ok = ReadFile(fh, out->data+out->cursor, size, &nread, NULL);
        if(!ok){
            err = 1;
            goto wfinally;
        }
        out->cursor += size;
        #endif
        wfinally:
        #ifdef _WIN32
        if(fh != INVALID_HANDLE_VALUE){
            CloseHandle(fh);
        }
        #endif
        msb16_destroy(&path_sb);
    }
    else {
        #ifndef _WIN32
        int fd = -1;
        enum {flags = O_RDONLY};
        fd = open(path, flags);
        if(fd < 0){
            err = 1;
            goto finally;
        }
        struct stat s;
        err = fstat(fd, &s);
        if(err) goto pfinally;
        if(!S_ISREG(s.st_mode)){
            // TODO
            err = 1;
            goto pfinally;
        }
        size_t sz = s.st_size;
        err = msb_ensure_additional(out, sz+1);
        if(err) goto pfinally;
        for(size_t pos = out->cursor;sz;){
            ssize_t rd = read(fd, out->data + pos, sz);
            if(rd < 0 && errno == EINTR) continue;
            if(!rd){
                err = 1;
                goto pfinally;
            }
            pos += rd;
            sz -= rd;
        }
        out->cursor += s.st_size;
        pfinally:
        if(fd >= 0) close(fd);
        #endif
        goto finally;
    }
    finally:
    return err;
}

static
int
b_cmp_mtime(const MTime* a, const MTime* b){
    #ifdef _WIN32
        return CompareFileTime(a, b);
    #else
        if(a->tv_sec  < b->tv_sec) return -1;
        if(a->tv_sec  > b->tv_sec) return 1;
        if(a->tv_nsec < b->tv_nsec) return -1;
        if(a->tv_nsec > b->tv_nsec) return 1;
        return 0;
    #endif
}

static
int
fi_mtime_implies_rebuild(const BuildFileInfo* src, const BuildFileInfo* exe){
    if(!exe->exists) return 1;
    if(!src->exists) return -1;
    return b_cmp_mtime(&src->mtime, &exe->mtime) > 0;
}


static
int
ap_ma_atom_append(ArgToParse* dst, const void* parg){
    const StringView* arg = parg;
    BuildCtx* ctx = dst->dest.user_pointer->user_data;
    Marray(Atom)* targets = dst->dest.pointer;
    if(!dst->num_parsed) targets->count = 0;
    Atom a = b_atomize2(ctx, arg->text, arg->length);
    int err = ma_push(Atom)(targets, allocator_from_arena(&ctx->perm_aa), a);
    return err;
}

static struct BctxInfo {
    union { TypeInfo type_info; struct { STRUCTINFO; }; };
    MemberInfo members[13];
} TI_BuildCtxGlobal;

enum {
#ifdef TARGET_SETTINGS_EXTRA_FIELDS
#define X(type, field, help, def) _tsef_##field,
    TARGET_SETTINGS_EXTRA_FIELDS(X)
#undef X
#endif
    TARGET_SETTINGS_EXTRA_FIELDS_COUNT,
};

static struct TargetSettingsInfo {
    union { TypeInfo type_info; struct { STRUCTINFO; }; };
    MemberInfo members[10 + TARGET_SETTINGS_EXTRA_FIELDS_COUNT];
} TI_TargetSettings;
static struct GlobalCachedSettingsInfo {
    union { TypeInfo type_info; struct { STRUCTINFO; }; };
    MemberInfo members[6];
} TI_GlobalCachedSettings;

static struct OSInfo {
    union {TypeInfo type_info; struct { ENUMINFO; }; };
    Atom _Nonnull names[OS__MAX];
} TI_OS;
static struct ArchFamInfo {
    union {TypeInfo type_info; struct { ENUMINFO; }; };
    Atom _Nonnull names[AFAM__MAX];
} TI_ARCHFAM;
static struct ArchBitsInfo {
    union {TypeInfo type_info; struct { ENUMINFO; }; };
    Atom _Nonnull names[ABITS__MAX];
} TI_ARCHBITS;
static struct CompilerFlavorInfo {
    union {TypeInfo type_info; struct { ENUMINFO; }; };
    Atom _Nonnull names[COMPILER__MAX];
} TI_CompilerFlavor;

static struct BuildTargetInfo {
    union { TypeInfo type_info; struct { STRUCTINFO; }; };
    MemberInfo members[14];
} TI_BuildTarget;

static TypeInfoMarray TI_MA_Atom = {
    .size = sizeof(Marray(Atom)),
    .align = _Alignof(Marray(Atom)),
    .kind = TIK_MARRAY,
    .type = &TI_Atom.type_info,
};
static TypeInfoMarray TI_MA_LS = {
    .size = sizeof(Marray(LongString)),
    .align = _Alignof(Marray(LongString)),
    .kind = TIK_MARRAY,
    .type = &TI_SV.type_info,
};

static TypeInfoAtomMap TI_AM_BuildTarget = {
    .size = sizeof(AtomMap),
    .align = _Alignof(AtomMap),
    .kind = TIK_ATOM_MAP,
    .type = &TI_BuildTarget.type_info,
};
static TypeInfoAtomMap TI_AM_MA_Atom = {
    .size = sizeof(AtomMap),
    .align = _Alignof(AtomMap),
    .kind = TIK_ATOM_MAP,
    .type = &TI_MA_Atom.type_info,
};

static int maybe_recompile_this(BuildCtx*, int, char*_Null_unspecified*_Nonnull);
static int list_targets(BuildCtx* ctx, BuildTarget* tgt);
static int print_ctx(BuildCtx* ctx, BuildTarget* tgt);
static int compile_commands(BuildCtx* ctx, BuildTarget* tgt);
static int fish_completions(BuildCtx* ctx, BuildTarget* tgt);

static int write_to_json_file(BuildCtx* ctx, const void* src, const TypeInfo* ti, Atom path);
static int read_from_json_file(BuildCtx* ctx, void* dst, const TypeInfo* ti, Atom path);

static
BuildCtx*_Nullable
build_ctx(int argc, char*_Null_unspecified*_Nonnull argv, char*_Null_unspecified*_Nonnull envp, const char*_Nonnull basefile){
    BuildCtx* ctx = Allocator_zalloc(MALLOCATOR, sizeof *ctx);
    if(!ctx) return NULL;
    if(0){
        fail:
        if(ctx){
            ArenaAllocator_free_all(&ctx->perm_aa);
            ArenaAllocator_free_all(&ctx->tmp_aa);
            Allocator_free(MALLOCATOR, ctx, sizeof *ctx);
            ctx = NULL;
        }
        return NULL;
    }
    Allocator perm = allocator_from_arena(&ctx->perm_aa);
    ctx->at.allocator = perm;
    ctx->env.allocator = perm;
    ctx->env.at = &ctx->at;
    ctx->env.windows = BUILD_OS == OS_WINDOWS;
    ctx->build_dir = nil_atom;
    ctx->cwd = nil_atom;
    ctx->njobs = -1;
    #ifdef _WIN32
    ctx->logger.errhandle = (intptr_t)(uintptr_t)GetStdHandle(STD_ERROR_HANDLE);
    ctx->logger.outhandle = (intptr_t)(uintptr_t)GetStdHandle(STD_OUTPUT_HANDLE);
    #else
    ctx->logger.errhandle = (intptr_t)STDERR_FILENO;
    ctx->logger.outhandle = (intptr_t)STDOUT_FILENO;
    #endif
    ctx->logger.level = BLOG_INFO;
    {
        int err = register_type_atoms(&ctx->at);
        if(err) goto fail;
        TI_MA_Atom.name = b_atomize(ctx, "Marray(Atom)");
        TI_MA_LS.name = b_atomize(ctx, "Marray(LongString)");
        TI_AM_BuildTarget.name = b_atomize(ctx, "AtomMap(BuildTarget)");
        TI_AM_MA_Atom.name = b_atomize(ctx, "AtomMap(Marray(Atom))");
        TI_OS = (struct OSInfo){
            .name = b_atomize(ctx, "OS"),
            .size = sizeof(enum OS),
            .align = _Alignof(enum OS),
            .kind = TIK_ENUM,
            .length = OS__MAX,
            .named = 1,
            .names = {
                #define X(e, s) b_atomize(ctx, s),
                    XOS(X)
                #undef X
            },
        };
        TI_ARCHFAM = (struct ArchFamInfo){
            .name = b_atomize(ctx, "ArchFam"),
            .size = sizeof(enum ArchFam),
            .align = _Alignof(enum ArchFam),
            .kind = TIK_ENUM,
            .length = AFAM__MAX,
            .named = 1,
            .names = {
                #define X(e, s) b_atomize(ctx, s),
                    XARCHFAM(X)
                #undef X
            },
        };
        TI_ARCHBITS = (struct ArchBitsInfo){
            .name = b_atomize(ctx, "ArchBits"),
            .size = sizeof(enum ArchBits),
            .align = _Alignof(enum ArchBits),
            .kind = TIK_ENUM,
            .length = ABITS__MAX,
            .named = 1,
            .names = {
                #define X(e, s) b_atomize(ctx, s),
                    XARCHBITS(X)
                #undef X
            },
        };
        TI_CompilerFlavor = (struct CompilerFlavorInfo){
            .name = b_atomize(ctx, "CompilerFlavor"),
            .size = sizeof(enum CompilerFlavor),
            .align = _Alignof(enum CompilerFlavor),
            .kind = TIK_ENUM,
            .length = COMPILER__MAX,
            .named = 1,
            .names = {
                #define X(e, s) b_atomize_f(ctx, s),
                    XFLAVOR(X)
                #undef X
            },
        };
        TI_BuildCtxGlobal = (struct BctxInfo){
            .name = b_atomize(ctx, "BuildCtx"),
            .size = sizeof *ctx,
            .align = _Alignof(BuildCtx),
            .kind = TIK_STRUCT,
            .length = sizeof TI_BuildCtxGlobal.members / sizeof TI_BuildCtxGlobal.members[0],
            .members = {
                {
                    .name = b_atomize(ctx, "targets"),
                    .type = &TI_AM_BuildTarget.type_info,
                    .offset = offsetof(BuildCtx, targets),
                    .noser = 1,
                    .nodeser = 1,
                },
                {
                    .name = b_atomize(ctx, "cmd_history"),
                    .type = &TI_AM_MA_Atom.type_info,
                    .offset = offsetof(BuildCtx, cmd_history),
                    .noser = 1,
                    .nodeser = 1,
                },
                {
                    .name = b_atomize(ctx, "gen_dir"),
                    .type = &TI_Atom.type_info,
                    .offset = offsetof(BuildCtx, gen_dir),
                    .noser = 1,
                    .nodeser = 1,
                },
                {
                    .name = b_atomize(ctx, "deps_dir"),
                    .type = &TI_Atom.type_info,
                    .offset = offsetof(BuildCtx, deps_dir),
                    .noser = 1,
                    .nodeser = 1,
                },
                {
                    .name = b_atomize(ctx, "git_hash"),
                    .type = &TI_Atom.type_info,
                    .offset = offsetof(BuildCtx, git_hash),
                    .noser = 1,
                    .nodeser = 1,
                },
                {
                    .name = b_atomize(ctx, "build_targets"),
                    .type = &TI_MA_Atom.type_info,
                    .offset = offsetof(BuildCtx, build_targets),
                    .noser = 1,
                    .nodeser = 1,
                },
                {
                    .name = b_atomize(ctx, "target"),
                    .type = &TI_TargetSettings.type_info,
                    .offset = offsetof(BuildCtx, target),
                    .noser = 1,
                    .nodeser = 1,
                },
                {
                    .name = b_atomize(ctx, "cached"),
                    .type = &TI_GlobalCachedSettings.type_info,
                    .offset = offsetof(BuildCtx, cached),
                    .noser = 1,
                    .nodeser = 1,
                },
                {
                    .name = b_atomize(ctx, "exe_path"),
                    .type = &TI_Atom.type_info,
                    .offset = offsetof(BuildCtx, exe_path),
                    .noser = 1,
                    .nodeser = 1,
                },
                {
                    .name = b_atomize(ctx, "cache_path"),
                    .type = &TI_Atom.type_info,
                    .offset = offsetof(BuildCtx, cache_path),
                    .nodeser = 1,
                    .noser = 1,
                },
                {
                    .name = b_atomize(ctx, "cmd_cache_path"),
                    .type = &TI_Atom.type_info,
                    .offset = offsetof(BuildCtx, cmd_cache_path),
                    .nodeser = 1,
                    .noser = 1,
                },
                {
                    .name = b_atomize(ctx, "settings_cache_path"),
                    .type = &TI_Atom.type_info,
                    .offset = offsetof(BuildCtx, settings_cache_path),
                    .nodeser = 1,
                    .noser = 1,
                },
                {
                    .name = b_atomize(ctx, "njobs"),
                    .type = &TI_int32_t.type_info,
                    .offset = offsetof(BuildCtx, njobs),
                },
            },
        };
        TI_TargetSettings = (struct TargetSettingsInfo){
            .name = b_atomize(ctx, "TargetSettings"),
            .size = sizeof(TargetSettings),
            .align = _Alignof(TargetSettings),
            .kind = TIK_STRUCT,
            .length = sizeof TI_TargetSettings.members / sizeof TI_TargetSettings.members[0],
            .members = {
                {
                    .name = b_atomize(ctx, "cc"),
                    .type = &TI_Atom.type_info,
                    .offset = offsetof(TargetSettings, cc),
                },
                {
                    .name = b_atomize(ctx, "compiler_flavor"),
                    .type = &TI_CompilerFlavor.type_info,
                    .offset = offsetof(TargetSettings, compiler_flavor),
                },
                {
                    .name = b_atomize(ctx, "os"),
                    .type = &TI_OS.type_info,
                    .offset = offsetof(TargetSettings, os),
                },
                {
                    .name = b_atomize(ctx, "arch"),
                    .type = &TI_ARCHFAM.type_info,
                    .offset = offsetof(TargetSettings, arch),
                },
                {
                    .name = b_atomize(ctx, "bits"),
                    .type = &TI_ARCHBITS.type_info,
                    .offset = offsetof(TargetSettings, bits),
                },
                {
                    .name = b_atomize(ctx, "optimize"),
                    .type = &TI__Bool.type_info,
                    .offset = offsetof(TargetSettings, optimize),
                },
                {
                    .name = b_atomize(ctx, "no_debug_symbols"),
                    .type = &TI__Bool.type_info,
                    .offset = offsetof(TargetSettings, no_debug_symbols),
                },
                {
                    .name = b_atomize(ctx, "sanitize"),
                    .type = &TI__Bool.type_info,
                    .offset = offsetof(TargetSettings, sanitize),
                },
                {
                    .name = b_atomize(ctx, "native_sanitize"),
                    .type = &TI__Bool.type_info,
                    .offset = offsetof(TargetSettings, native_sanitize),
                },
                {
                    .name = b_atomize(ctx, "tsan"),
                    .type = &TI__Bool.type_info,
                    .offset = offsetof(TargetSettings, tsan),
                },
                #ifdef TARGET_SETTINGS_EXTRA_FIELDS
                    #define X(ty, field, help, def) { \
                        .name = b_atomize(ctx, #field), \
                        .type = BTypeInfo(ty), \
                        .offset = offsetof(TargetSettings, field), \
                    },
                    TARGET_SETTINGS_EXTRA_FIELDS(X)
                    #undef X
                #endif
            },
        };
        TI_GlobalCachedSettings = (struct GlobalCachedSettingsInfo){
            .name = b_atomize(ctx, "GlobalCachedSettings"),
            .size = sizeof(GlobalCachedSettings),
            .align = _Alignof(GlobalCachedSettings),
            .kind = TIK_STRUCT,
            .length = sizeof TI_GlobalCachedSettings.members / sizeof TI_GlobalCachedSettings.members[0],
            .members = {
                {
                    .name = b_atomize(ctx, "build_cc"),
                    .type = &TI_Atom.type_info,
                    .offset = offsetof(GlobalCachedSettings, build_cc),
                },
                {
                    .name = b_atomize(ctx, "build_compiler_flavor"),
                    .type = &TI_CompilerFlavor.type_info,
                    .offset = offsetof(GlobalCachedSettings, build_compiler_flavor),
                },
                {
                    .name = b_atomize(ctx, "build_dir"),
                    .type = &TI_Atom.type_info,
                    .offset = offsetof(GlobalCachedSettings, build_dir),
                },
                {
                    .name = b_atomize(ctx, "cwd"),
                    .type = &TI_Atom.type_info,
                    .offset = offsetof(GlobalCachedSettings, cwd),
                },
                {
                    .name = b_atomize(ctx, "src_path"),
                    .type = &TI_Atom.type_info,
                    .offset = offsetof(GlobalCachedSettings, src_path),
                },
                {
                    .name = b_atomize(ctx, "njobs"),
                    .type = &TI_int32_t.type_info,
                    .offset = offsetof(GlobalCachedSettings, njobs),
                },
            },
        };
        TI_BuildTarget = (struct BuildTargetInfo){
            .name = b_atomize(ctx, "BuildTarget"),
            .size = sizeof(BuildTarget),
            .align = _Alignof(BuildTarget),
            .kind = TIK_STRUCT,
            .length = sizeof TI_BuildTarget.members / sizeof TI_BuildTarget.members[0],
            .members = {
                {
                    .name = b_atomize(ctx, "name"),
                    .type = &TI_Atom.type_info,
                    .offset = offsetof(BuildTarget, name),
                },
                {
                    .name = b_atomize(ctx, "dependencies"),
                    .type = &TI_MA_Atom.type_info,
                    .offset = offsetof(BuildTarget, dependencies),
                },
                {
                    .name = b_atomize(ctx, "outputs"),
                    .type = &TI_MA_Atom.type_info,
                    .offset = offsetof(BuildTarget, outputs),
                },
                {
                    .name = b_atomize(ctx, "is_src"),
                    .type = &TI__Bool.type_info,
                    .bitfield = {
                        .offset = offsetof(BuildTarget, _bits),
                        .kind = MK_BITFIELD,
                        .bitsize = 1,
                        .bitoffset = 0,
                    },
                },
                {
                    .name = b_atomize(ctx, "is_generated"),
                    .type = &TI__Bool.type_info,
                    .bitfield = {
                        .offset = offsetof(BuildTarget, _bits),
                        .kind = MK_BITFIELD,
                        .bitsize = 1,
                        .bitoffset = 1,
                    },
                },
                {
                    .name = b_atomize(ctx, "is_binary"),
                    .type = &TI__Bool.type_info,
                    .bitfield = {
                        .offset = offsetof(BuildTarget, _bits),
                        .kind = MK_BITFIELD,
                        .bitsize = 1,
                        .bitoffset = 2,
                    },
                },
                {
                    .name = b_atomize(ctx, "is_cmd"),
                    .type = &TI__Bool.type_info,
                    .bitfield = {
                        .offset = offsetof(BuildTarget, _bits),
                        .kind = MK_BITFIELD,
                        .bitsize = 1,
                        .bitoffset = 3,
                    },
                },
                {
                    .name = b_atomize(ctx, "is_phony"),
                    .type = &TI__Bool.type_info,
                    .bitfield = {
                        .offset = offsetof(BuildTarget, _bits),
                        .kind = MK_BITFIELD,
                        .bitsize = 1,
                        .bitoffset = 4,
                    },
                },
                {
                    .name = b_atomize(ctx, "is_script"),
                    .type = &TI__Bool.type_info,
                    .bitfield = {
                        .offset = offsetof(BuildTarget, _bits),
                        .kind = MK_BITFIELD,
                        .bitsize = 1,
                        .bitoffset = 5,
                    },
                },
                {
                    .name = b_atomize(ctx, "should_exec"),
                    .type = &TI__Bool.type_info,
                    .bitfield = {
                        .offset = offsetof(BuildTarget, _bits),
                        .kind = MK_BITFIELD,
                        .bitsize = 1,
                        .bitoffset = 6,
                    },
                },
                {
                    .name = b_atomize(ctx, "is_compile_command"),
                    .type = &TI__Bool.type_info,
                    .bitfield = {
                        .offset = offsetof(BuildTarget, _bits2),
                        .kind = MK_BITFIELD,
                        .bitsize = 1,
                        .bitoffset = 0,
                    },
                },
                {
                    .name = b_atomize(ctx, "is_coro"),
                    .type = &TI__Bool.type_info,
                    .bitfield = {
                        .offset = offsetof(BuildTarget, _bits2),
                        .kind = MK_BITFIELD,
                        .bitsize = 1,
                        .bitoffset = 1,
                    },
                },
                {
                    .name = b_atomize(ctx, "visit_state"),
                    .type = &TI_uint8_t.type_info,
                    .bitfield = {
                        .offset = offsetof(BuildTarget, _bits2),
                        .kind = MK_BITFIELD,
                        .bitsize = 2,
                        .bitoffset = 2,
                    },
                },
                {
                    .name = b_atomize(ctx, "cmd.args"),
                    .type = &TI_MA_LS.type_info,
                    .offset = offsetof(BuildTarget, cmd.args),
                },
            },
        };
    }
    {
        const char* progname = argv[0];
        size_t len = strlen(progname);
        MStringBuilder sb = {.allocator = allocator_from_arena(&ctx->tmp_aa)};
        int err = msb_ensure_additional(&sb, 2048);
        if(err) goto fail;
        // don't trust progname, try to get the path using system apis
        if(BUILD_OS == OS_WINDOWS){
            uint32_t sz = 0;
            {
                MStringBuilder16 wsb = {.allocator=allocator_from_arena(&ctx->tmp_aa)};
                err = msb16_ensure_additional(&wsb, 2048);
                if(err) goto fail;
                #ifdef _WIN32
                // FIXME: the behavior on buffer too small is really weird
                sz = GetModuleFileNameW(NULL, (wchar_t*)wsb.data, wsb.capacity);
                #endif
                wsb.cursor = sz;
                StringViewUtf16 sv16 = msb16_borrow_sv(&wsb);
                msb_write_utf16(&sb, sv16.text, sv16.length);
                msb16_destroy(&wsb);
                if(sb.errored) goto fail;
                sz = (uint32_t)sb.cursor;
            }
            if(sz){
                for(uint32_t i = sz; i--;){
                    if(sb.data[i] == '.') break;
                    sb.data[i] |= 0x20;
                }
                sb.cursor = sz;
            }
        }
        else if(BUILD_OS == OS_APPLE){
            uint32_t bufsize = (uint32_t)sb.capacity;
            #ifdef __APPLE__
            err = _NSGetExecutablePath(sb.data, &bufsize);
            #else
            (void)bufsize;
            #endif
            if(err) goto fail;
            sb.cursor = strlen(sb.data);
        }
        else if(BUILD_OS == OS_LINUX){
            int64_t linklen = 0;
            #ifndef _WIN32
            linklen = readlink("/proc/self/exe", sb.data, sb.capacity);
            #endif
            if(linklen <= 0) goto fail;
            sb.cursor = (size_t)linklen;
        }
        else {
            BuildFileInfo* exe = b_file_info(ctx, progname, len);
            // Didn't use system APIs to get real path, fallback to looking it up in PATH
            if(!exe->exists){
                err = env_resolve_prog_path(&ctx->env, (StringView){len, progname}, &sb, b_file_exists, ctx);
                if(err) goto fail;
            }
        }
        if(sb.cursor != 0){
            LongString p = msb_borrow_ls(&sb);
            progname = p.text;
            len = p.length;
        }
        Atom exe = b_atomize2(ctx, progname, len);
        ctx->exe_path = exe;
        msb_destroy(&sb);
    }
    {
        int err;
        Allocator tmp = allocator_from_arena(&ctx->tmp_aa);
        {
            MStringBuilder sb = {.allocator=tmp};
            msb_write_str(&sb, ctx->exe_path->data, ctx->exe_path->length);
            msb_write_literal(&sb, ".cache.json");
            msb_nul_terminate(&sb);
            if(sb.errored) goto fail;
            LongString path;
            path = msb_borrow_ls(&sb);
            ctx->cache_path = b_atomize2(ctx, path.text, path.length);
            msb_destroy(&sb);
        }
        err = read_from_json_file(ctx, &ctx->cached, &TI_GlobalCachedSettings.type_info, ctx->cache_path);
        if(err) goto fail;
        if(ctx->cwd != nil_atom){
            if(BUILD_OS == OS_WINDOWS){
                MStringBuilder16 sb = {.allocator=allocator_from_arena(&ctx->tmp_aa)};
                msb16_write_utf8(&sb, ctx->cwd->data, ctx->cwd->length);
                msb16_nul_terminate(&sb);
                if(sb.errored){
                    err = 1;
                }
                else {
                    #ifdef _WIN32
                    BOOL ok = SetCurrentDirectoryW((wchar_t*)sb.data);
                    if(!ok) err = 1;
                    #endif
                }
                msb16_destroy(&sb);
            }
            else {
                #ifndef _WIN32
                err = chdir(ctx->cwd->data);
                #endif
            }
            if(err) ctx->cwd = nil_atom;
        }
    }
    if(ctx->cwd == nil_atom){
        MStringBuilder sb = {.allocator = allocator_from_arena(&ctx->tmp_aa)};
        int err = msb_ensure_additional(&sb, 2048);
        if(err) goto fail;
        size_t len = 0;
        if(BUILD_OS != OS_WINDOWS){
            #ifndef _WIN32
            void* ok = getcwd(sb.data, sb.capacity);
            if(!ok) b_abort(ctx, "getcwd failed");
            len = strlen(sb.data);
            #endif
        }
        else {
            MStringBuilder16 wsb = {.allocator = allocator_from_arena(&ctx->tmp_aa)};
            err = msb16_ensure_additional(&wsb, 2048);
            if(err) goto fail;
            #ifdef _WIN32
            len = GetCurrentDirectoryW(wsb.capacity, (wchar_t*)wsb.data);
            #endif
            msb_write_utf16(&sb, wsb.data, len);
            msb16_destroy(&wsb);
        }
        if(!len) goto fail;
        while(len && is_sep(sb.data[len-1], BUILD_OS==OS_WINDOWS))
            len--;
        ctx->cwd = b_atomize2(ctx, sb.data, len);
        msb_destroy(&sb);
    }
    if(!ctx->src_path || ctx->src_path == nil_atom){
        MStringBuilder sb = {.allocator = allocator_from_arena(&ctx->tmp_aa)};
        int err = msb_ensure_additional(&sb, 2048);
        if(err) goto fail;
        StringView file = {strlen(basefile), basefile};
        if(!path_is_abspath(file, BUILD_OS==OS_WINDOWS)){
            StringView root = path_dirname((StringView){ctx->exe_path->length, ctx->exe_path->data}, BUILD_OS==OS_WINDOWS);
            msb_write_str(&sb, root.text, root.length);
            msb_write_char(&sb, BUILD_OS==OS_WINDOWS?'\\':'/');
            msb_write_str(&sb, file.text, file.length);
            msb_nul_terminate(&sb);
            if(sb.errored) goto fail;
            StringView f = msb_borrow_sv(&sb);
            if(b_file_exists(ctx, f.text, f.length))
                file = f;
            else {
                msb_erase(&sb, file.length);
                msb_write_literal(&sb, "..");
                msb_write_char(&sb, BUILD_OS==OS_WINDOWS?'\\':'/');
                msb_write_str(&sb, file.text, file.length);
                msb_nul_terminate(&sb);
                if(sb.errored) goto fail;
                f = msb_borrow_sv(&sb);
                if(b_file_exists(ctx, f.text, f.length))
                    file = f;
            }
        }
        if(b_file_exists(ctx, file.text, file.length)){
            ctx->src_path = b_atomize2(ctx, file.text, file.length);
        }
        else {
            b_loglvl(BLOG_ERROR, ctx, "Unable to resolve src for this build program.\nRecompile with absolute path if you need to execute from a different directory.\n");
            ctx->src_path = nil_atom;
        }
        msb_destroy(&sb);
    }
    int err;
    {
        err = env_parse_posix(&ctx->env, envp);
    }
    if(err) goto fail;
    #ifdef TARGET_SETTINGS_EXTRA_FIELDS
        #define X(ty, field, help, def) \
            ctx->target.field = def;
        TARGET_SETTINGS_EXTRA_FIELDS(X)
        #undef X
    #endif
    if(ctx->build_dir && ctx->build_dir != nil_atom){
        ctx->settings_cache_path = b_atomize_f(ctx, "%s/settings.json", ctx->build_dir->data);
        err = read_from_json_file(ctx, &ctx->target, &TI_TargetSettings.type_info, ctx->settings_cache_path);
        (void)err;
        err = 0;
    }
    {
        if(!ctx->build_cc || ctx->build_cc == nil_atom) ctx->build_cc = b_atomize(ctx, DEFAULT_BUILD_COMPILER);

    }
    if(!ctx->target.cc || ctx->target.cc == nil_atom) ctx->target.cc = ctx->build_cc;
    {
        size_t alloc_size = 0;
        ctx->envp = (void*)env_to_envp(&ctx->env, allocator_from_arena(&ctx->perm_aa), &alloc_size);
        if(!ctx->envp) goto fail;
    }
    {
        err = ma_push(Atom)(&ctx->build_targets, allocator_from_arena(&ctx->perm_aa), b_atomize(ctx, "all"));
        (void)err;
        err = 0;
    }
    ArgToParse pos_args[] = {
        {
            .name = SV("target"),
            .dest = {
                .type = ARG_USER_DEFINED,
                .pointer = &ctx->build_targets,
                .user_pointer = &(ArgParseUserDefinedType){
                    .type_name = SV("string"),
                    .user_data = ctx,
                },
            },
            .help = "The target to build. Specify 'list' to see them all.",
            .show_default = 1,
            .max_num = 20,
            .append_proc = &ap_ma_atom_append,
        }
    };
    _Bool no_optimize = 0;
    _Bool no_sanitize = 0;
    _Bool no_native_sanitize = 0;
    _Bool no_tsan = 0;
    _Bool debug_symbols = 0;
    _Bool no_rebuild = 0;
    TargetSettings before_ap = ctx->target;
    #define BARGDEST(x) _Generic(x, \
        int64_t*: ARGDEST((int64_t*)x), \
        uint64_t*: ARGDEST((uint64_t*)x), \
        float*: ARGDEST((float*)x), \
        double*: ARGDEST((double*)x), \
        int*: ARGDEST((int*)x), \
        _Bool*: ARGDEST((_Bool*)x), \
        StringView*: ARGDEST((StringView*)x), \
        LongString*: ARGDEST((StringView*)x), \
        Atom*: ArgAtomDest((Atom*)x, &ctx->at))

    enum {HCC_IDX=2, HCC_FLAVOR_IDX=3, BCC_IDX=0, BCC_FLAVOR_IDX=1, JOBS_IDX=18};

    ArgToParse kw_args[] = {
        [BCC_IDX] = {
            .name = SV("BCC"),
            .altname1 = SV("--build-cc"),
            .dest = BARGDEST(&ctx->build_cc),
            .help = "C compiler for code to run on the build machine",
            .show_default = 1,
            .min_num = 1, .max_num = 1,
        },
        [BCC_FLAVOR_IDX] = {
            .name = SV("--build-cc-flavor"),
            .dest = ArgEnumDest(&ctx->build_compiler_flavor, &CompilerFlavorEnum),
            .help = "compiler flavor. Will be guessed from compiler name if not specified",
            .show_default = 1,
            .min_num = 1, .max_num = 1,
        },
        [HCC_IDX] = {
            .name = SV("HCC"),
            .altname1 = SV("--host-cc"),
            .dest = BARGDEST(&ctx->target.cc),
            // .dest = ArgAtomDest(&ctx->target.cc, &ctx->at),
            .help = "C compiler for code to run on the host (target) machine",
            .show_default = 1,
            .min_num = 1, .max_num = 1,
        },
        [HCC_FLAVOR_IDX] = {
            .name = SV("--host-cc-flavor"),
            .dest = ArgEnumDest(&ctx->target.compiler_flavor, &CompilerFlavorEnum),
            .help = "compiler flavor. Will be guessed from compiler name if not specified",
            .show_default = 1,
            .min_num = 1, .max_num = 1,
        },
        {
            .name = SV("--os"),
            .dest = ArgEnumDest(&ctx->target.os, &OSEnum),
            .help = "The target os. Defaults to build machine OS.",
            .show_default = 1,
            .min_num = 1, .max_num = 1,
        },
        {
            .name = SV("--arch"),
            .dest = ArgEnumDest(&ctx->target.arch, &ArchFamEnum),
            .help = "The target arch. Defaults to build machine arch.",
            .show_default = 1,
            .min_num = 1, .max_num = 1,
        },
        {
            .name = SV("--bits"),
            .dest = ArgEnumDest(&ctx->target.bits, &ArchBitsEnum),
            .help = "The target bitness. Defaults to build machine bitness.",
            .show_default = 1,
            .min_num = 1, .max_num = 1,
        },
        {
            .name = SV("-O"),
            .altname1 = SV("--optimize"),
            .dest = BARGDEST(&ctx->target.optimize),
            .help = "Build with optimizations",
        },
        {
            .name = SV("-O0"),
            .altname1 = SV("--no-optimize"),
            .dest = BARGDEST(&no_optimize),
            .help = "Build without optimizations",
        },
        {
            .name = SV("-g0"),
            .altname1 = SV("--no-debug-symbols"),
            .dest = BARGDEST(&ctx->target.no_debug_symbols),
            .help = "Build with out debug symbols",
        },
        {
            .name = SV("-g"),
            .altname1 = SV("--debug-symbols"),
            .dest = BARGDEST(&debug_symbols),
            .help = "Build with debug symbols",
        },
        {
            .name = SV("--sanitize"),
            .dest = BARGDEST(&ctx->target.sanitize),
            .help = "Build with sanitizers",
        },
        {
            .name = SV("--no-sanitize"),
            .dest = BARGDEST(&no_sanitize),
            .help = "Build without sanitizers",
        },
        {
            .name = SV("--sanitize-native"),
            .dest = BARGDEST(&ctx->target.native_sanitize),
            .help = "Build native tools with sanitizers",
        },
        {
            .name = SV("--no-sanitize-native"),
            .dest = BARGDEST(&no_native_sanitize),
            .help = "Build native tools without sanitizers",
        },
        {
            .name = SV("--tsan"),
            .dest = BARGDEST(&ctx->target.tsan),
            .help = "Build with thread sanitizer (incompatible with --sanitize)",
        },
        {
            .name = SV("--no-tsan"),
            .dest = BARGDEST(&no_tsan),
            .help = "Build without thread sanitizer",
        },
        {
            .name = SV("-b"),
            .altname1 = SV("--builddir"),
            .dest = BARGDEST(&ctx->build_dir),
            .help = "Directory to build in",
            .show_default = 1,
            .min_num = 1, .max_num = 1,
        },
        [JOBS_IDX] = {
            .name = SV("-j"),
            .altname1 = SV("--jobs"),
            .dest = BARGDEST(&ctx->njobs),
            .help = "How many concurrent jobs. -1 (or no arg) means auto. 0 is the same as 1.",
            .show_default = 1,
            .min_num = 0,
            .max_num = 1,
        },
        {
            .name = SV("-L"),
            .altname1 = SV("--loglevel"),
            .dest = ArgEnumDest(&ctx->logger.level, &LogLevelEnum),
            .show_default = 1,
            .help = "Logging level.",
            .min_num = 1, .max_num = 1,
        },
        #ifdef TARGET_SETTINGS_EXTRA_FIELDS
            #define X(ty, field, h, def) { \
                .name = SV(#field), \
                .dest = BARGDEST(&ctx->target.field), \
                .help = h, \
                .min_num = 0, .max_num = 1, \
                .show_default = 1, \
            },
            TARGET_SETTINGS_EXTRA_FIELDS(X)
            #undef X
        #endif
        {
            .name = SV("--"),
            .dest = {
                .type = ARG_USER_DEFINED,
                .pointer = &ctx->dash_dash_args,
                .user_pointer = &(ArgParseUserDefinedType){
                    .type_name = SV("string"),
                    .user_data = ctx,
                },
            },
            .help = "Args for commands like run",
            .show_default = 0,
            .max_num = 100,
            .append_proc = &ap_ma_atom_append,
            .hidden = 1,
        },
        {
            .name = SV("--no-rebuild"),
            .dest = BARGDEST(&no_rebuild),
            .help = "Don't rebuild the build program.",
            .hidden = 1,
        }
        #undef BARGDEST
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
    _Bool stdout_is_terminal = 1;
    int columns = 80;
    #ifdef _WIN32
    {
        HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
        if(h == INVALID_HANDLE_VALUE || h == NULL){
            stdout_is_terminal = 0;
        }
        else {
            stdout_is_terminal = GetFileType(h) == FILE_TYPE_CHAR;
            CONSOLE_SCREEN_BUFFER_INFO csbi;
            BOOL ok = GetConsoleScreenBufferInfo(h, &csbi);
            if(ok) columns = csbi.srWindow.Right - csbi.srWindow.Left + 1;
        }
    }
    #else
    {
        stdout_is_terminal = isatty(STDOUT_FILENO);
        struct winsize w;
        if(ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == 0){
            columns = w.ws_col;
        }
    }
    #endif
    if(columns > 80) columns = 80;
    ArgParser parser = {
        .name = argv[0]?argv[0]:"build",
        .description = "build script",
        .positional.args = pos_args,
        .positional.count = arrlen(pos_args),
        .keyword.args = kw_args,
        .keyword.count = arrlen(kw_args),
        .early_out.args = early_args,
        .early_out.count = arrlen(early_args),
        .styling.plain = !stdout_is_terminal,
        #ifndef __DRC__ // workaround libffi limitation
        .print = (int(*)(void*, const char*, ...))b_printf,
        #endif
        .hout = ctx,
        .herr = ctx,
    };
    Args args = argc?(Args){argc-1, (const char*const*)argv+1}:(Args){0, 0};
    switch(check_for_early_out_args(&parser, &args)){
        case HELP:{
            print_argparse_help(&parser, columns);
            fflush(stdout);
            void _Exit(int);
            _Exit(0);
            // return 0;
        }
        case HIDDEN_HELP:{
            print_argparse_hidden_help(&parser, columns);
            fflush(stdout);
            void _Exit(int);
            _Exit(0);
            // return 0;
        }
        case FISH:{
            print_argparse_fish_completions(&parser);
            fflush(stdout);
            void _Exit(int);
            _Exit(0);
            // return 0;
        }
        default:
            break;
    }
    enum ArgParseError parse_err = parse_args(&parser, &args, ARGPARSE_FLAGS_KWARGS_WITHOUT_PREFIX|ARGPARSE_FLAGS_UNKNOWN_KWARGS_AS_ARGS);
    if(parse_err){
        print_argparse_error(&parser, parse_err);
        goto fail;
    }
    if(no_sanitize) ctx->target.sanitize = 0;
    if(no_native_sanitize) ctx->target.native_sanitize = 0;
    if(no_tsan) ctx->target.tsan = 0;
    if(no_optimize) ctx->target.optimize = 0;
    if(debug_symbols) ctx->target.no_debug_symbols = 0;
    if(kw_args[HCC_IDX].visited && !kw_args[HCC_FLAVOR_IDX].visited)
        ctx->target.compiler_flavor = 0;
    if(kw_args[BCC_IDX].visited && !kw_args[BCC_FLAVOR_IDX].visited)
        ctx->build_compiler_flavor = 0;

    if(!ctx->build_dir || ctx->build_dir == nil_atom){
        b_loglvl(BLOG_ERROR, ctx, "Must specify build directory (or have it be in build_cache.json)\n");
        goto fail;
    }
    {
        ctx->gen_dir = b_atomize_f(ctx, "%s/Generated", ctx->build_dir->data);
        ctx->deps_dir = b_atomize_f(ctx, "%s/Depends", ctx->build_dir->data);
        ctx->cmd_cache_path = b_atomize_f(ctx, "%s/cmd_cache.json", ctx->build_dir->data);
        err = read_from_json_file(ctx, &ctx->cmd_history, &TI_AM_MA_Atom.type_info, ctx->cmd_cache_path);
        (void)err;
        Atom settings_cache_path = b_atomize_f(ctx, "%s/settings.json", ctx->build_dir->data);
        if(settings_cache_path != ctx->settings_cache_path){
            TargetSettings after_ap = ctx->target;
            ctx->settings_cache_path = settings_cache_path;
            err = read_from_json_file(ctx, &ctx->target, &TI_TargetSettings.type_info, ctx->settings_cache_path);
            (void)err;
            if(after_ap.cc != before_ap.cc) ctx->target.cc = after_ap.cc;
            if(after_ap.compiler_flavor != before_ap.compiler_flavor) ctx->target.compiler_flavor = after_ap.compiler_flavor;
            if(after_ap.os != before_ap.os) ctx->target.os = after_ap.os;
            if(after_ap.arch != before_ap.arch) ctx->target.arch = after_ap.arch;
            if(after_ap.bits != before_ap.bits) ctx->target.bits = after_ap.bits;
            if(after_ap.optimize != before_ap.optimize) ctx->target.optimize = after_ap.optimize;
            if(after_ap.sanitize != before_ap.sanitize) ctx->target.sanitize = after_ap.sanitize;
            if(after_ap.tsan != before_ap.tsan) ctx->target.tsan = after_ap.tsan;
            #ifdef TARGET_SETTINGS_EXTRA_FIELDS
                #define X(ty, field, help, def) \
                if(after_ap.field != before_ap.field) \
                    ctx->target.field = after_ap.field;
                TARGET_SETTINGS_EXTRA_FIELDS(X)
                #undef X
            #endif
        }
        err = 0;
    }
    if(!ctx->target.compiler_flavor)
        ctx->target.compiler_flavor = guess_compiler_flavor(ctx->target.cc);
    if(!ctx->build_compiler_flavor)
        ctx->build_compiler_flavor = guess_compiler_flavor(ctx->build_cc);
    if(!no_rebuild){
        err = maybe_recompile_this(ctx, argc, argv);
        if(err) goto fail;
    }

    _Bool j_no_arg = kw_args[JOBS_IDX].visited && kw_args[JOBS_IDX].num_parsed == 0;
    if(j_no_arg) ctx->njobs = -1;
    write_to_json_file(ctx, &ctx->cached, &TI_GlobalCachedSettings.type_info, ctx->cache_path);

    _Bool has_clean = 0;
    Atom clean = b_atomize(ctx, "clean");
    for(size_t i = 0; i < ctx->build_targets.count; i++){
        if(ctx->build_targets.data[i] == clean){
            has_clean = 1;
            ma_remove_at(Atom)(&ctx->build_targets, i);
            i--;
        }
    }
    if(has_clean) rm_directory(ctx, ctx->build_dir->data);
    mkdirs_if_not_exists(ctx, AT_to_LS(ctx->build_dir));
    mkdirs_if_not_exists(ctx, AT_to_LS(ctx->gen_dir));
    mkdirs_if_not_exists(ctx, AT_to_LS(ctx->deps_dir));
    write_to_json_file(ctx, &ctx->target, &TI_TargetSettings.type_info, ctx->settings_cache_path);
    if(ctx->njobs == 0) ctx->njobs = 1;
    if(ctx->njobs < 0) ctx->njobs = b_num_cpus();
    if(ctx->target.os == OS_NATIVE)
        ctx->target.os = BUILD_OS;
    // TODO: sniff what native means
#if 0
    if(ctx->target.arch == AFAM_NATIVE)
        ctx->target.arch = AFAM_x86;
    if(ctx->target.bits == ABITS_NATIVE)
        ctx->target.bits = ABITS_64;
#endif

    (void)phony_target(ctx, "clean");

    {
        BuildTarget* list = script_target(ctx, "list", list_targets, NULL);
        list->is_phony = 1;
    }
    {
        BuildTarget* print = script_target(ctx, "print", print_ctx, NULL);
        print->is_phony = 1;
    }
    {
        BuildTarget* ccjs = script_target(ctx, "compile_commands.json", compile_commands, NULL);
        ccjs->is_phony = 1;
    }
    {
        BuildTarget* fc = script_target(ctx, "fish-completions", fish_completions, NULL);
        fc->is_phony = 1;
    }
    return ctx;
}

static
int
list_targets(BuildCtx* ctx, BuildTarget* tgt){
    (void)tgt;
    AtomMapItems items = AM_items(&ctx->targets);
    b_printf(ctx, "targets:\n");
    MARRAY_FOR_EACH_VALUE(AtomMapItem, item, items){
        BuildTarget* t = item.p;
        if(t->is_src) continue;
        StringView sv = {item.atom->length, item.atom->data};
        if(sv_startswith(sv, SV("./"))) continue;
        b_printf(ctx, "  %s\n", item.atom->data);
    }
    return 0;
}

static
int
fish_completions(BuildCtx* ctx, BuildTarget* tgt){
    (void)tgt;
    StringView exe = {ctx->exe_path->length, ctx->exe_path->data};
    StringView name = path_basename(exe, 0);
    AtomMapItems items = AM_items(&ctx->targets);
    MARRAY_FOR_EACH_VALUE(AtomMapItem, item, items){
        BuildTarget* t = item.p;
        if(t->is_src) continue;
        StringView sv = {item.atom->length, item.atom->data};
        if(sv_startswith(sv, SV("./"))) continue;
        if(sv_startswith(sv, (StringView){ctx->build_dir->length, ctx->build_dir->data}))
            continue;
        if(t == tgt) continue;
        b_printf(ctx, "complete -c %.*s -f -a %s\n", (int)name.length, name.text, item.atom->data);
    }
    return 0;
}

static
int
print_ctx(BuildCtx* ctx, BuildTarget* tgt){
    (void)tgt;
    TiPrinter printer = {
        #ifndef __DRC__ // libffi limitation
        .printer = (int(*)(void*, const char*, ...))b_printf,
        #endif
        .ctx = ctx,
    };
    ti_print_any(ctx, &TI_BuildCtxGlobal.type_info, &printer);
    return 0;
}
typedef struct CompileCommand CompileCommand;
struct CompileCommand {
    Atom directory;
    Atom file;
    Marray(LongString) arguments;
    Atom output;
};

#ifdef __clang__
#pragma clang assume_nonnull end
#endif
#define MARRAY_T CompileCommand
#include "Marray.h"
#ifdef __clang__
#pragma clang assume_nonnull begin
#endif

static
int
compile_commands(BuildCtx* ctx, BuildTarget* t){
    (void)t;
    struct CompileCommandInfo {
        union { TypeInfo type_info; struct { STRUCTINFO; }; };
        MemberInfo members[4];
    } TI_CompileCommand = {
        .name = b_atomize(ctx, "CompileCommand"),
        .size = sizeof(CompileCommand),
        .align = _Alignof(CompileCommand),
        .kind = TIK_STRUCT,
        .length = sizeof TI_CompileCommand.members / sizeof TI_CompileCommand.members[0],
        .members = {
            {
                .name = b_atomize(ctx, "directory"),
                .offset = offsetof(CompileCommand, directory),
                .type = &TI_Atom.type_info,
            },
            {
                .name = b_atomize(ctx, "file"),
                .offset = offsetof(CompileCommand, file),
                .type = &TI_Atom.type_info,
            },
            {
                .name = b_atomize(ctx, "arguments"),
                .offset = offsetof(CompileCommand, arguments),
                .type = &TI_MA_LS.type_info,
            },
            {
                .name = b_atomize(ctx, "output"),
                .offset = offsetof(CompileCommand, output),
                .type = &TI_Atom.type_info,
            },
        },
    };
    struct TypeInfoMarray TI_MA_CC = {
        .name = b_atomize(ctx, "Marray(CompileCommand)"),
        .size = sizeof(Marray(CompileCommand)),
        .align = _Alignof(Marray(CompileCommand)),
        .kind = TIK_MARRAY,
        .type = &TI_CompileCommand.type_info,
    };
    Marray(CompileCommand) commands = {0};
    Allocator tmp = allocator_from_arena(&ctx->tmp_aa);
    AtomMapItems items = AM_items(&ctx->targets);
    for(size_t i = 0; i < items.count; i++){
        BuildTarget* tgt = items.data[i].p;
        if(!tgt->is_compile_command) continue;
        CompileCommand* cc; int err = ma_alloc(CompileCommand)(&commands, tmp, &cc);
        if(err) return err;
        if(tgt->dependencies.count < 1){
            b_loglvl(BLOG_ERROR, ctx, "'%s' has no dependencies.\n", tgt->name->data);
            return 1;
        }
        *cc = (CompileCommand){
            .directory = ctx->cwd,
            .arguments = tgt->cmd.args,
            .file = tgt->dependencies.data[0],
            .output = tgt->name,
        };
    }
    int err = write_to_json_file(ctx, &commands, &TI_MA_CC.type_info, b_atomize(ctx, "compile_commands.json"));

    if(err) b_loglvl(BLOG_ERROR, ctx, "Failed to make compile_commands.json\n");
    return err;
}


static
void
b_log_(BuildCtx* ctx, const char* msg, size_t len){
    if(ctx->logger.func){
        int err = ctx->logger.func(ctx->logger.errhandle, msg, len);
        (void)err;
    }
    #ifdef _WIN32
    else {
        DWORD n = 0;
        WriteFile((HANDLE)(uintptr_t)ctx->logger.errhandle, msg, len, &n, NULL);
    }
    #else
    else {
        write((int)ctx->logger.errhandle, msg, len);
    }
    #endif
}

static
void
b_loglvlv(int loglvl, BuildCtx* ctx, const char* fmt, va_list vap){
    Allocator tmp = allocator_from_arena(&ctx->tmp_aa);
    MStringBuilder sb = {.allocator=tmp};
    if(0){
        StringView lvl = loglvl >= 0 && loglvl < BLOG__MAX?BLogLevelSVs[loglvl] : SV("");
        if(lvl.length){
            msb_write_str(&sb, lvl.text, lvl.length);
            msb_write_literal(&sb, ": ");
        }
    }

    msb_vsprintf(&sb, fmt, vap);
    StringView sv = msb_borrow_sv(&sb);
    b_log_(ctx, sv.text, sv.length);
    msb_destroy(&sb);
}

static
void
#ifdef __GNUC__
__attribute__((format(printf, 2, 3)))
#endif
b_log(BuildCtx* ctx, const char* fmt, ...){
    if(BLOG_INFO < ctx->logger.level)
        return;
    va_list vap;
    va_start(vap, fmt);
    b_loglvlv(BLOG_INFO, ctx, fmt, vap);
    va_end(vap);
}
static
void
#ifdef __GNUC__
__attribute__((format(printf, 3, 4)))
#endif
b_loglvl(int loglvl, BuildCtx* ctx, const char* fmt, ...){
    if((int)loglvl < ctx->logger.level)
        return;
    va_list vap;
    va_start(vap, fmt);
    b_loglvlv(loglvl, ctx, fmt, vap);
    va_end(vap);
}

static
int
#ifdef __GNUC__
__attribute__((format(printf, 2, 3)))
#endif
b_printf(BuildCtx* ctx, const char* fmt, ...){
    va_list vap;
    va_start(vap, fmt);
    b_printfv(ctx, fmt, vap);
    va_end(vap);
    return 0;
}
static
int
b_printfv(BuildCtx* ctx, const char* fmt, va_list vap){
    Allocator tmp = allocator_from_arena(&ctx->tmp_aa);
    MStringBuilder sb = {.allocator=tmp};
    msb_vsprintf(&sb, fmt, vap);
    StringView sv = msb_borrow_sv(&sb);
    if(ctx->logger.func){
        int err = ctx->logger.func(ctx->logger.outhandle, sv.text, sv.length);
        (void)err;
    }
    #ifdef _WIN32
    else {
        DWORD n = 0;
        WriteFile((HANDLE)(uintptr_t)ctx->logger.outhandle, sv.text, sv.length, &n, NULL);
    }
    #else
    else {
        write((int)ctx->logger.outhandle, sv.text, sv.length);
    }
    #endif
    msb_destroy(&sb);
    return 0;
}

static
int
mkdirs_if_not_exists(BuildCtx* ctx, LongString path){
    Allocator tmp = allocator_from_arena(&ctx->tmp_aa);
    MStringBuilder sb = {.allocator=tmp};
    int ret = 0;
    const char* end = path.text + path.length;
    const char* p = path.text;
    for(;p < end;){
        const char* sep = memsep(p, end - p, BUILD_OS == OS_WINDOWS);
        if(!sep) break;
        msb_write_str(&sb, p, sep-p);
        LongString d = msb_borrow_ls(&sb);
        int err = mkdir_if_not_exists(ctx, d.text);
        if(err){
            ret = err;
            goto finally;
        }
        p = sep+1;
        msb_write_char(&sb, BUILD_OS==OS_WINDOWS?'\\':'/');
    }
    mkdir_if_not_exists(ctx, path.text);
    finally:
    msb_destroy(&sb);
    return ret;
}

static
int
mkdir_if_not_exists(BuildCtx* ctx, const char* path){
    b_loglvl(BLOG_DEBUG, ctx, "mkdir(%s)\n", path);
    int ret = 0;
    if(BUILD_OS == OS_WINDOWS){
        MStringBuilder16 msb = {.allocator = allocator_from_arena(&ctx->tmp_aa)};
        msb16_write_utf8(&msb, path, strlen(path));
        msb16_write_uint16_t(&msb, u'\0');
        if(msb.errored) ret = 1;
        else {
            #ifdef _WIN32
            int ok = CreateDirectoryW((wchar_t*)msb.data, NULL);
            if(!ok){
                DWORD err = GetLastError();
                if(err == ERROR_ALREADY_EXISTS)
                    ret = 0;
                else
                    ret = err;
            }
            #endif
        }
        msb16_destroy(&msb);
    }
    else {
        #ifndef _WIN32
        ret = mkdir(path, 0755);
        if(ret < 0){
            ret = errno;
            if(ret == EEXIST) return 0;
        }
        #endif
    }
    return ret;
}

static
int
move_file(BuildCtx* ctx, const char* from, const char* to){
    if(BUILD_OS == OS_WINDOWS){
        MStringBuilder16 msb = {.allocator = allocator_from_arena(&ctx->tmp_aa)};
        int ok = 1;
        msb16_write_utf8(&msb, from, strlen(from));
        msb16_write_uint16_t(&msb, u'\0');
        size_t to_start = msb.cursor;
        msb16_write_utf8(&msb, to, strlen(to));
        msb16_write_uint16_t(&msb, u'\0');
        if(msb.errored){
            ok = 0;
        }
        else {
            uint16_t* from16 = msb.data;
            uint16_t* to16 = msb.data + to_start;
            #ifdef _WIN32
            ok = MoveFileExW((wchar_t*)from16, (wchar_t*)to16, MOVEFILE_COPY_ALLOWED|MOVEFILE_REPLACE_EXISTING);
            #else
            (void)from16; (void)to16;
            #endif
        }
        msb16_destroy(&msb);
        return ok == 0;
    }
    else {
        int err = 0;
        #ifndef _WIN32
        err = rename(from, to);
        #endif
        return err;
    }
}

static
int
copy_file(BuildCtx* ctx, const char* from, const char* to){
    if(BUILD_OS == OS_WINDOWS){
        MStringBuilder16 msb = {.allocator = allocator_from_arena(&ctx->tmp_aa)};
        int ok = 1;
        msb16_write_utf8(&msb, from, strlen(from));
        msb16_write_uint16_t(&msb, u'\0');
        size_t to_start = msb.cursor;
        msb16_write_utf8(&msb, to, strlen(to));
        msb16_write_uint16_t(&msb, u'\0');
        if(msb.errored){
            ok = 0;
        }
        else {
            uint16_t* from16 = msb.data;
            uint16_t* to16 = msb.data + to_start;
            #ifdef _WIN32
            ok = CopyFileW((wchar_t*)from16, (wchar_t*)to16, 0);
            #else
            (void)from16; (void)to16;
            #endif
        }
        msb16_destroy(&msb);
        return ok == 0;
    }
    else if(BUILD_OS == OS_APPLE){
        int ret = 0;
        #if defined(__APPLE__)
        copyfile_state_t s = copyfile_state_alloc();
        copyfile_flags_t flags = COPYFILE_ALL;
        ret = copyfile(from, to, s, flags);
        copyfile_state_free(s);
        #endif
        return ret;
    }
    else if(BUILD_OS == OS_LINUX){
        int err = 0;
        #if defined(__linux__)
        struct stat sfrom;
        int tfd = -1;
        int ffd = -1;
        ffd = open(from, O_RDONLY);
        if(ffd < 0){
            err = 1;
            goto finally;
        }
        err = fstat(ffd, &sfrom);
        if(err) goto finally;

        tfd = open(to, O_WRONLY|O_CREAT|O_TRUNC,  sfrom.st_mode & 0777);
        if(tfd < 0){
            err = 1;
            goto finally;
        }
        for(off_t offset = 0; offset < sfrom.st_size;){
            loff_t off_in = offset;
            loff_t off_out = offset;
            ssize_t copied = copy_file_range(ffd, &off_in, tfd, &off_out, sfrom.st_size - offset, 0);
            if(copied == -1){
                if(errno == EINTR) continue;
                err = 1;
                goto finally;
            }
            if(copied == 0){
                err = 1;
                goto finally;
            }
            offset += copied;
        }
        err = 0;

        finally:
        if(ffd != -1) close(ffd);
        if(tfd != -1) close(tfd);
        #endif
        return err;
    }
    else {
        return -1;
    }
}

static
int
copy_directory(BuildCtx* ctx, const char* from, const char* to){
    if(BUILD_OS == OS_WINDOWS){
        MStringBuilder16 msb = {.allocator = allocator_from_arena(&ctx->tmp_aa)};
        int ok = 1;
        msb16_write_utf8(&msb, from, strlen(from));
        msb16_write_uint16_t(&msb, u'\0');
        size_t to_start = msb.cursor;
        msb16_write_utf8(&msb, to, strlen(to));
        msb16_write_uint16_t(&msb, u'\0');
        if(msb.errored){
            ok = 0;
        }
        else {
            uint16_t* from16 = msb.data;
            uint16_t* to16 = msb.data + to_start;
            #ifdef _WIN32
            // unimplemented
            ok = 0;
            (void)from16;
            (void)to16;
            #else
            (void)from16; (void)to16;
            #endif
        }
        msb16_destroy(&msb);
        return ok == 0;
    }
    else if(BUILD_OS == OS_APPLE){
        int ret = 0;
        #if defined(__APPLE__)
        copyfile_state_t s = copyfile_state_alloc();
        copyfile_flags_t flags = COPYFILE_ALL | COPYFILE_RECURSIVE;
        ret = copyfile(from, to, s, flags);
        copyfile_state_free(s);
        #endif
        return ret;
    }
    else if(BUILD_OS == OS_LINUX){
        int err = 0;
        #if defined(__linux__)
        CmdBuilder cmd = {.allocator = allocator_from_arena(&ctx->tmp_aa)};
        cmd_prog(&cmd, LS("cp"));
        cmd_cargs(&cmd, "-r", from, to);
        err = b_run_cmd_sync(ctx, &cmd);
        cmd_destroy(&cmd);
        #endif
        return err;
    }
    else {
        return -1;
    }
}

static
int
rm_file(BuildCtx* ctx, const char* path){
    if(BUILD_OS == OS_WINDOWS){
        MStringBuilder16 msb = {.allocator = allocator_from_arena(&ctx->tmp_aa)};
        int ok = 1;
        msb16_write_utf8(&msb, path, strlen(path));
        msb16_write_uint16_t(&msb, u'\0');
        if(msb.errored) ok = 0;
        else {
            #ifdef _WIN32
            ok = DeleteFileW((wchar_t*)msb.data);
            #endif
        }
        msb16_destroy(&msb);
        return ok == 0;
    }
    else {
        int err = 0;
        #ifndef _WIN32
        err =  unlink(path);
        #endif
        return err;
    }
}

static
int
rm_directory(BuildCtx* ctx, const char* path){
    CmdBuilder cmd = {.allocator = allocator_from_arena(&ctx->tmp_aa)};
    cmd_prog(&cmd, LS("rm"));
    cmd_cargs(&cmd, "-rf", path);
    int err = b_run_cmd_sync(ctx, &cmd);
    cmd_destroy(&cmd);
    if(err) return err;
    return err;
}

static
void
print_command(BuildCtx* ctx, CmdBuilder* cmd){
    if(ctx->logger.level <= BLOG_DEBUG){
        msb_nul_terminate(&cmd->prog);
        b_log(ctx, "%s: ", cmd->prog.data);
    }
    MARRAY_FOR_EACH(LongString, ls, cmd->args)
        b_log(ctx, "%s ", ls->text);
    b_log(ctx, "\n");
}

static
void
parse_vs_json_text(BuildCtx* ctx, LongString text){
    Allocator tmp = allocator_from_arena(&ctx->tmp_aa);
    DrJsonContext* jsctx = drjson_create_ctx(tmp, &ctx->at);
    unsigned flags = 0;
    DrJsonValue val = drjson_parse_string(jsctx, text.text, text.length, flags);
    if(val.kind == DRJSON_ERROR)
        goto finally;
    DrJsonValue version = drjson_checked_query(jsctx, val, DRJSON_STRING, "Version", sizeof "Version"-1);
    if(version.kind == DRJSON_ERROR){
        goto finally;
    }
    if(version.atom != b_atomize(ctx, "1.2")){
        b_loglvl(BLOG_WARN, ctx, "version mismatch, expected '1.2', got '%s'\n", version.atom->data);
        goto finally;
    }
    DrJsonValue source = drjson_checked_query(jsctx, val, DRJSON_STRING, "Data.Source", sizeof "Data.Source"-1);
    if(source.kind == DRJSON_ERROR)
        goto finally;
    Atom src_path = b_normalize_patha(ctx, source.atom);
    BuildTarget* target = NULL;
    AtomMapItems items = AM_items(&ctx->targets);
    MARRAY_FOR_EACH_VALUE(AtomMapItem, item, items){
        BuildTarget* t = item.p;
        MARRAY_FOR_EACH_VALUE(Atom, a, t->dependencies){
            if(a == src_path){
                target = t;
                goto Break;
            }
        }
    }
    b_loglvl(BLOG_WARN, ctx, "target not found for '%s'\n", source.atom->data);
    goto finally;
    Break:;
    DrJsonValue includes = drjson_checked_query(jsctx, val, DRJSON_ARRAY, "Data.Includes", sizeof "Data.Includes" - 1);
    if(includes.kind == DRJSON_ERROR)
        goto finally;
    int64_t len = drjson_len(jsctx, includes);
    for(int64_t i = 0; i < len; i++){
        DrJsonValue inc = drjson_get_by_index(jsctx, includes, i);
        if(inc.kind != DRJSON_STRING) continue;
        StringView s = {inc.atom->length, inc.atom->data};
        if(sv_startswith(s, SV("c:\\\\program files")))
            continue;
        Atom path = b_normalize_patha(ctx, inc.atom);
        BuildTarget* src = src_filea(ctx, path);
        add_dep(ctx, target, src);
    }
    finally:
    drjson_ctx_free_all(jsctx);
}

static
void
parse_makefile_text(BuildCtx* ctx, LongString text){
    Allocator tmp = allocator_from_arena(&ctx->tmp_aa);
    MStringBuilder target = {.allocator=tmp};
    MStringBuilder prereq = {.allocator=tmp};
    MStringBuilder* current = &target;
    BuildTarget* current_target = NULL;
    _Bool backslash = 0;
    for(const char* p = text.text, *end = text.text+text.length; p < end; p++){
        switch(*p){
            case '\\':
                if(!backslash){
                    backslash = 1;
                    continue;
                }
                msb_write_char(current, '\\');
                break;
            case '#':
                while(p < end && *p != '\n')
                    p++;
                goto delimiter;
            case '\r':
                if(p[1] == '\n') p++;
                goto fallthrough;
            case '\n':
                fallthrough:;
                goto delimiter;
            case '\t':
            case ' ':
                if(backslash){
                    msb_write_char(current, *p);
                    backslash = 0;
                    continue;
                }
                delimiter:
                if(!current->cursor) continue;
                if(current == &prereq){
                    if(current->cursor){
                        if(current_target){
                            StringView s = msb_borrow_sv(current);
                            // Strip trailing whitespace
                            while(s.length && s.text[s.length-1] == ' '){
                                msb_erase(current, 1);
                                s = msb_borrow_sv(current);
                            }
                            // Strip leading whitespace (from line continuations)
                            while(s.length && (s.text[0] == ' ' || s.text[0] == '\t')){
                                s.text++;
                                s.length--;
                            }
                            _Bool skip = s.length==0;
                            if(!skip && sv_contains(s, SV("Frameworks")))
                                skip = 1;
                            if(!skip && sv_contains(s, SV("Fetched")))
                                skip = 1;
                            if(!skip){
                                if(0)b_loglvl(BLOG_DEBUG, ctx, "%d: '%s'\n", __LINE__, msb_borrow_ls(current).text);
                                Atom a = b_atomize2(ctx, s.text, s.length);
                                if(0)b_loglvl(BLOG_DEBUG, ctx, "%d: '%s' depends on '%s'\n", __LINE__, current_target->name->data, a->data);
                                add_src_depa(ctx, current_target, a);
                            }
                        }
                        else {
                            b_loglvl(BLOG_DEBUG, ctx, "%d: No target for '%s'\n", __LINE__, msb_borrow_ls(current).text);
                        }
                        msb_reset(current);
                    }
                    break;
                }
                else {
                    b_loglvl(BLOG_WARN, ctx, "whitespace delimiter without current target\n");
                    StringView s = msb_borrow_sv(current);
                    b_loglvl(BLOG_WARN, ctx, "'%.*s'\n", (int)s.length, s.text);
                }
                if(*p == '\n'){
                    if(!backslash){
                        current_target = NULL;
                        msb_reset(&prereq);
                        msb_reset(&target);
                        current = &target;
                        break;
                    }
                    backslash = 0;
                }
                break;
            case ':':
                if(backslash){
                    msb_write_char(current, '\\');
                    backslash = 0;
                }
                if(current == &target){
                    current = &prereq;
                    StringView s = msb_borrow_sv(&target);
                    Atom a = b_atomize2(ctx, s.text, s.length);
                    if(0)b_loglvl(BLOG_DEBUG, ctx, "%d: '%.*s'\n", __LINE__, (int)a->length, a->data);
                    current_target = get_targeta(ctx, a);
                    if(!current_target)
                        current_target = get_targeta(ctx, b_normalize_patha(ctx, a));
                    if(0)b_loglvl(BLOG_DEBUG, ctx, "%d: current_target: %p\n", __LINE__, (void*)current_target);
                    break;
                }
                // idk, ignore it
                break;
            default:
                if(backslash){
                    msb_write_char(current, '\\');
                    backslash = 0;
                }
                msb_write_char(current, *p);
                break;
        }

    }
    msb_destroy(&prereq);
    msb_destroy(&target);
}

static
int
parse_depfile(BuildCtx* ctx, const char* filename){
    int e = 0;
    b_loglvl(BLOG_DEBUG, ctx, "parsing '%s'\n", filename);
    MStringBuilder sb = {.allocator=allocator_from_arena(&ctx->tmp_aa)};
    e = b_read_file(ctx, filename, &sb);
    if(e) goto finally;
    LongString text = msb_borrow_ls(&sb);
    StringView fn = {strlen(filename), filename};
    if(sv_endswith(fn, SV(".json")))
        parse_vs_json_text(ctx, text);
    else
        parse_makefile_text(ctx, text);
    finally:
    msb_destroy(&sb);
    return e;
}

static
void
parse_depfiles(BuildCtx* ctx){
    if(BUILD_OS != OS_WINDOWS){
        MStringBuilder pattern = {.allocator = allocator_from_arena(&ctx->tmp_aa)};
        msb_write_str(&pattern, ctx->deps_dir->data, ctx->deps_dir->length);
        msb_write_literal(&pattern, "/*.{d,dep,deps,json}");
        LongString pat = msb_borrow_ls(&pattern);
        char** paths = NULL;
        size_t path_c = 0;
        int err = 0;
        #ifndef _WIN32
            glob_t g = {0};
            err = glob(pat.text, GLOB_BRACE, NULL, &g);
            if(err && err != GLOB_NOMATCH) b_loglvl(BLOG_DEBUG, ctx, "err globbing: %s\n", strerror(errno));
            paths = g.gl_pathv;
            path_c = g.gl_pathc;
        #else
        (void)pat;
        #endif
        if(!err){
            for(size_t i = 0; i < path_c; i++){
                char* path = paths[i];
                parse_depfile(ctx, path);
            }
        }
        #ifndef _WIN32
        globfree(&g);
        #endif
        msb_destroy(&pattern);
    }
    else {
        MStringBuilder16 pattern = {.allocator = allocator_from_arena(&ctx->tmp_aa)};
        MStringBuilder path = {.allocator = allocator_from_arena(&ctx->tmp_aa)};
        msb16_write_utf8(&pattern, ctx->deps_dir->data, ctx->deps_dir->length);
        msb16_write_uint16_t(&pattern, u'/');
        static const StringViewUtf16 exts[] = {
            SV16I("*.d"),
            SV16I("*.dep"),
            SV16I("*.deps"),
            SV16I("*.json"),
        };
        for(size_t i = 0; i < sizeof exts / sizeof exts[0]; i++){
            StringViewUtf16 ext = exts[i];
            pattern.cursor = ctx->deps_dir->length+1;
            msb16_write_str(&pattern, ext.text, ext.length);
            LongStringUtf16 ls = msb16_borrow_ls(&pattern);
            #ifdef _WIN32
            WIN32_FIND_DATAW fd = {0};
            HANDLE h = FindFirstFileW(ls.text, &fd);
            if(h == INVALID_HANDLE_VALUE)
                continue;
            do {
                if(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
                    continue;
                pattern.cursor = ctx->deps_dir->length+1;
                msb16_write_str(&pattern, fd.cFileName, wcslen(fd.cFileName));
                ls = msb16_borrow_ls(&pattern);
                msb_write_utf16(&path, ls.text, ls.length);
                parse_depfile(ctx, msb_borrow_ls(&path).text);
            } while(FindNextFileW(h, &fd));
            FindClose(h);
            #else
            (void)ls;
            #endif
        }
        msb_destroy(&path);
        msb16_destroy(&pattern);
    }
}

enum {
    NOT_RUNNING = 0,
    RUNNING = 1,
    UP_TO_DATE = 2,
};


#ifdef _WIN32
_Static_assert(sizeof(intptr_t) == sizeof(HANDLE), "");
#endif

static
int
sort_targets(BuildCtx* ctx, Atom a, Marray(Atom)* out){
    BuildTarget* t = get_targeta(ctx, a);
    if(!t) return 1;
    enum {
        UNVISITED,
        VISITING,
        VISITED,
    };
    if(t->visit_state == VISITED)
        return 0;
    t->visit_state = VISITING;
    MARRAY_FOR_EACH_VALUE(Atom, d, t->dependencies){
        BuildTarget* t2 = get_targeta(ctx, d);
        if(t2->visit_state == UNVISITED){
            int err = sort_targets(ctx, d, out);
            if(err) return err;
        }
        else if(t2->visit_state == VISITING){
            b_loglvl(BLOG_ERROR, ctx, "Circular dependency with '%s' and '%s'\n", d->data, a->data);
            return 1;
        }
    }
    t->visit_state = VISITED;
    int err = 0;
    err = ma_push(Atom)(out, allocator_from_arena(&ctx->perm_aa), a);
    return err;
}

static
int
execute_targets(BuildCtx* ctx){
    int err = 0;
    parse_depfiles(ctx);
    MARRAY_FOR_EACH_VALUE(Atom, b, ctx->build_targets){
        BuildTarget* t = get_targeta(ctx, b);
        if(!t){
            b_loglvl(BLOG_ERROR, ctx, "Unknown target: '%s'\n", b->data);
            err = 1;
        }
    }
    if(err) return err;
    Marray(Atom) sorted = {0};
    MARRAY_FOR_EACH_VALUE(Atom, b, ctx->build_targets){
        err = sort_targets(ctx, b, &sorted);
        if(err) return err;
    }
    {
        AtomMapItems items = AM_items(&ctx->targets);
        MARRAY_FOR_EACH_VALUE(AtomMapItem, it, items){
            BuildTarget* t = it.p;
            t->visit_state = 0;
        }
    }
    if(0){
        b_loglvl(BLOG_DEBUG, ctx, "%d: sorted.count: %zu\n", __LINE__, sorted.count);
        for(size_t i = 0; i < sorted.count; i++){
            b_loglvl(BLOG_DEBUG, ctx, "%d: sorted[%zu] = '%s'\n", __LINE__, i, sorted.data[i]->data);
        }
    }
    if(0){
        AtomMapItems items = AM_items(&ctx->targets);
        MARRAY_FOR_EACH_VALUE(AtomMapItem, it, items){
            BuildTarget* t = it.p;
            b_loglvl(BLOG_DEBUG, ctx, "%d: tgt = '%s'\n", __LINE__, t->name->data);
            for(size_t i = 0; i < t->dependencies.count; i++){
                b_loglvl(BLOG_DEBUG, ctx, "%d: '%s'.dependencies[%zu] = '%s'\n", __LINE__, t->name->data, i, t->dependencies.data[i]->data);
            }
        }
    }
    for(;;){
        if(ctx->jobs.count != (size_t)ctx->njobs){
            for(size_t i = 0; i < sorted.count; i++){
                BuildTarget* tgt = get_targeta(ctx, sorted.data[i]);
                if(tgt->is_src){
                    if(0) b_loglvl(BLOG_DEBUG, ctx, "%d: '%s' is_src\n", __LINE__, tgt->name->data);
                    tgt->visit_state = UP_TO_DATE;
                    ma_remove_at(Atom)(&sorted, i);
                    i--;
                    continue;
                }
                if(tgt->order_only){
                    BuildFileInfo* fi = b_file_info(ctx, tgt->name->data, tgt->name->length);
                    if(fi->exists){
                        tgt->visit_state = UP_TO_DATE;
                        ma_remove_at(Atom)(&sorted, i);
                        i--;
                        continue;
                    }
                }
                MARRAY_FOR_EACH_VALUE(Atom, d, tgt->dependencies){
                    BuildTarget* t = get_targeta(ctx, d);
                    if(t->visit_state != UP_TO_DATE)
                        goto Skip;
                }
                if(tgt->is_phony && (tgt->is_cmd || tgt->is_script || tgt->is_coro))
                    goto do_command;
                if(tgt->is_cmd && !tgt->should_exec){
                    if(0) b_loglvl(BLOG_DEBUG, ctx, "tgt->is_cmd: '%s'\n", tgt->name->data);
                    Marray(Atom)* old = AM_get(&ctx->cmd_history, tgt->name);
                    if(!old)
                        goto do_command;
                    if(old->count != tgt->cmd.args.count)
                        goto do_command;
                    for(size_t j = 0; j < old->count; j++){
                        LongString arg = tgt->cmd.args.data[j];
                        Atom o = old->data[j];
                        if(0){
                            b_loglvl(BLOG_DEBUG, ctx, "%s->cmd[%zu] = '%s'\n", tgt->name->data, j, arg.text);
                            b_loglvl(BLOG_DEBUG, ctx, "old->data[%zu] = '%s'\n", j, o->data);
                        }
                        if(!LS_equals(arg, AT_to_LS(o)))
                            goto do_command;
                    }
                }
                if(tgt->is_cmd || tgt->is_script || tgt->is_coro){
                    if(0) b_loglvl(BLOG_DEBUG, ctx, "%d: '%s' is_cmd || is_script || is_coro, checking mtimes\n", __LINE__, tgt->name->data);
                    const MTime* newest_in_mtime = NULL;
                    const MTime* oldest_out_mtime = NULL;
                    MARRAY_FOR_EACH_VALUE(Atom, d, tgt->dependencies){
                        BuildTarget* t = get_targeta(ctx, d);
                        if(!t) goto finally;
                        if(t->order_only) continue;
                        BuildFileInfo* fi = b_file_info(ctx, t->name->data, t->name->length);
                        if(!fi->exists)
                            goto do_command;
                        if(!newest_in_mtime) newest_in_mtime = &fi->mtime;
                        if(b_cmp_mtime(&fi->mtime, newest_in_mtime) > 0)
                            newest_in_mtime = &fi->mtime;
                    }
                    MARRAY_FOR_EACH_VALUE(Atom, o, tgt->outputs){
                        BuildTarget* t = get_targeta(ctx, o);
                        if(!t) goto finally;
                        BuildFileInfo* fi = b_file_info(ctx, t->name->data, t->name->length);
                        if(!fi->exists)
                            goto do_command;
                        if(!oldest_out_mtime) oldest_out_mtime = &fi->mtime;
                        if(b_cmp_mtime(&fi->mtime, oldest_out_mtime) > 0)
                            oldest_out_mtime = &fi->mtime;
                    }
                    if(tgt->is_binary){
                        BuildFileInfo* fi = b_file_info(ctx, tgt->name->data, tgt->name->length);
                        if(!fi->exists)
                            goto do_command;
                        if(!oldest_out_mtime) oldest_out_mtime = &fi->mtime;
                        if(b_cmp_mtime(&fi->mtime, oldest_out_mtime) > 0)
                            oldest_out_mtime = &fi->mtime;
                    }
                    if(!oldest_out_mtime || !newest_in_mtime){
                    }
                    else if(b_cmp_mtime(oldest_out_mtime, newest_in_mtime) < 0)
                        goto do_command;
                }
                if(tgt->is_phony && tgt->is_cmd){
                    do_command:;
                    assert(tgt->visit_state == 0);
                    tgt->visit_state = RUNNING;
                    ma_remove_at(Atom)(&sorted, i);
                    i--;
                    if(tgt->is_coro){
                        int b = tgt->corop(ctx, tgt);
                        if(b == BERROR) goto finally;
                        if(ctx->jobs.count == (size_t)ctx->njobs)
                            goto Break;
                    }
                    else if(tgt->is_script){
                        err = tgt->script(ctx, tgt);
                        if(err) return err;
                        tgt->visit_state = UP_TO_DATE;
                    }
                    else {
                        if(tgt->should_exec){
                            // flush caches etc.
                            write_to_json_file(ctx, &ctx->cmd_history, &TI_AM_MA_Atom.type_info, ctx->cmd_cache_path);
                            cmd_resolve_prog_path(&tgt->cmd, &ctx->env, b_file_exists, ctx);
                            err = cmd_exec(&tgt->cmd, ctx->envp);
                            if(err) goto finally;
                        }
                        else {
                            err = b_run_cmd_async(ctx, tgt, &tgt->cmd);
                            if(err){
                                b_loglvl(BLOG_ERROR, ctx, "%s failed\n", tgt->name->data);
                                goto finally;
                            }
                            Marray(Atom)* cached = AM_get(&ctx->cmd_history, tgt->name);
                            if(!cached){
                                cached = Allocator_zalloc(allocator_from_arena(&ctx->perm_aa), sizeof *cached);
                                if(!cached) goto finally;
                                err = AM_put(&ctx->cmd_history, allocator_from_arena(&ctx->perm_aa), tgt->name, cached);
                                if(err) goto finally;
                            }
                            cached->count = 0;
                            MARRAY_FOR_EACH_VALUE(LongString, arg, tgt->cmd.args){
                                Atom a = b_atomize2(ctx, arg.text, arg.length);
                                err = ma_push(Atom)(cached, allocator_from_arena(&ctx->perm_aa), a);
                                if(err) goto finally;
                            }
                        }
                        if(ctx->jobs.count == (size_t)ctx->njobs)
                            goto Break;
                    }
                }
                else {
                    b_loglvl(BLOG_DEBUG, ctx, "'%s' is up to date\n", tgt->name->data);
                    tgt->visit_state = UP_TO_DATE;
                    ma_remove_at(Atom)(&sorted, i);
                    i--;
                }
                Skip:;
            }
            Break:;
        }

        if(!ctx->jobs.count && !sorted.count){
            break;
        }
        size_t idx;
        int exit_code;
        for(;ctx->jobs.count;){
            err = cmd_wait_many(ctx->jobs.processes, ctx->jobs.count, &idx, &exit_code, 0);
            if(err){
                b_loglvl(BLOG_ERROR, ctx, "Error while waiting\n");
                goto finally;
            }
            if(idx == (size_t)-1)
                break;
            BuildJob finished = ctx->jobs.jobs[idx];
            b_loglvl(BLOG_DEBUG, ctx, "'%s' finished\n", finished.target->name->data);
            b_memremove(idx * sizeof *ctx->jobs.jobs,      ctx->jobs.jobs,      ctx->jobs.count * sizeof *ctx->jobs.jobs,      sizeof *ctx->jobs.jobs);
            b_memremove(idx * sizeof *ctx->jobs.processes, ctx->jobs.processes, ctx->jobs.count * sizeof *ctx->jobs.processes, sizeof *ctx->jobs.processes);
            ctx->jobs.count--;
            if(exit_code){
                err = exit_code;
                b_loglvl(BLOG_ERROR, ctx, "'%s' exited with code %d\n", finished.target->name->data, exit_code);
                goto finally;
            }
            if(finished.target->is_coro){
                int b = finished.target->coro.func(ctx, finished.target);
                if(b == BERROR){
                    b_loglvl(BLOG_ERROR, ctx, "Error in coro '%s'\n", finished.target->name->data);
                    goto finally;
                }
                if(finished.target->coro.step != BFINISHED)
                    continue;
            }
            finished.target->visit_state = UP_TO_DATE;
            if(finished.target->is_binary)
                b_file_info(ctx, finished.target->name->data, finished.target->name->length)->valid = 0;
            MARRAY_FOR_EACH_VALUE(Atom, o, finished.target->outputs){
                b_loglvl(BLOG_DEBUG, ctx, "%s is now finished\n", o->data);
                get_targeta(ctx, o)->visit_state = UP_TO_DATE;
                b_file_info(ctx, o->data, o->length)->valid = 0;
            }
        }
        // usleep(1);
    }
    finally:
    if(ctx->jobs.count){
        b_loglvl(BLOG_DEBUG, ctx, "%zu jobs need to be cleaned up\n", ctx->jobs.count);
        // Cleanup jobs in progress
        for(size_t j = 0; j < 3; j++){
            if(BUILD_OS == OS_WINDOWS) j = 3;
            for(size_t i = 0; i < ctx->jobs.count; i++){
                intptr_t proc = ctx->jobs.processes[i];
                if(BUILD_OS == OS_WINDOWS){
                    #ifdef _WIN32
                    HANDLE h = (HANDLE)proc;
                    TerminateProcess(h, 1);
                    CloseHandle(h);
                    #endif
                }
                else {
                    #ifndef _WIN32
                    pid_t pid = (pid_t)proc;
                    switch(j){
                        case 0:
                            if(waitpid(pid, NULL, WNOHANG) == 0){
                                kill(pid, SIGTERM);
                                continue;
                            }
                            break;
                        case 1:
                            if(waitpid(pid, NULL, WNOHANG) == 0){
                                kill(pid, SIGKILL);
                                continue;
                            }
                            break;
                        default:
                        case 2:
                            waitpid(pid, NULL, 0);
                            break;
                    }
                    #endif
                }
                b_memremove(i * sizeof *ctx->jobs.processes, ctx->jobs.processes, ctx->jobs.count * sizeof *ctx->jobs.processes, sizeof *ctx->jobs.processes);
                b_memremove(i * sizeof *ctx->jobs.jobs,      ctx->jobs.jobs,      ctx->jobs.count * sizeof *ctx->jobs.jobs,      sizeof *ctx->jobs.jobs);
                ctx->jobs.count--;
                i--;
            }
        }
        ctx->jobs.count = 0;
    }
    write_to_json_file(ctx, &ctx->cmd_history, &TI_AM_MA_Atom.type_info, ctx->cmd_cache_path);
    return err;
}

static
int
maybe_recompile_this(BuildCtx* ctx, int argc, char*_Null_unspecified*_Nonnull argv){
    if(!ctx->src_path || ctx->src_path == nil_atom){
        b_loglvl(BLOG_WARN, ctx, "Unable to check to recompile this as src path is unknown\n");
        return 0;
    }
    Atom progpath = ctx->exe_path;
    Atom old = b_atomize_f(ctx, "%s.old", progpath->data);
    int err;
    err = rm_file(ctx, old->data);
    (void)err;
    err = 0;
    BuildTarget* build = alloc_targeta(ctx, (Atom)b_normalize_patha(ctx, ctx->exe_path));
    build->is_compile_command = 1;
    Atom src = b_normalize_patha(ctx, ctx->src_path);
    Atom depfile = nil_atom;
    {
        build->is_binary = 1;
        build->is_cmd = 1;
        CmdBuilder* cmd = &build->cmd;
        cmd->allocator = allocator_from_arena(&ctx->perm_aa);
        cmd_prog(cmd, AT_to_LS(ctx->build_cc));
        target_src_inp(ctx, build, src->data);
        switch(ctx->build_compiler_flavor){
            case COMPILER__MAX:
            case COMPILER_UNKNOWN:
            case COMPILER_GCC_MINGW:
            case COMPILER_GCC:
            case COMPILER_CLANG:
                cmd_cargs(cmd, "-g");
                goto fallthrough;
            case COMPILER_CLANG_CL:
                fallthrough:;
                cmd_cargs(cmd, "-o", ctx->exe_path->data);
                cmd_cargs(cmd, "-MT", b_normalize_patha(ctx, ctx->exe_path)->data, "-MMD", "-MP", "-MF");
                depfile = b_atomize_f(ctx, "%s.deps", ctx->exe_path->data);
                cmd_aarg(cmd, depfile);
                break;
            case COMPILER_CL:
                cmd_cargs(cmd, "/nologo", "/std:c11");
                cmd_cargs(cmd, "/Zc:preprocessor");
                cmd_cargs(cmd, "/Zi", "/DEBUG");
                cmd_argf(cmd, "/Fd:%s.pdb", b_normalize_patha(ctx, ctx->exe_path)->data);
                cmd_argf(cmd, "/Fe:%s", b_normalize_patha(ctx, ctx->exe_path)->data);
                depfile = b_atomize_f(ctx, "%s.deps.json", ctx->exe_path->data);
                cmd_cargs(cmd, "/sourceDependencies", depfile->data);
                break;
        }
        cmd_argf(cmd, "-DDEFAULT_BUILD_COMPILER=\"%s\"", ctx->build_cc->data);
        if(cmd->errored) return 1;
    }
    _Bool should_recompile = 0;
    if(depfile != nil_atom){
        err = parse_depfile(ctx, depfile->data);
        if(err){
            err = 0;
            should_recompile = 1;
        }
    }
    BuildFileInfo* fi = b_file_info(ctx, ctx->exe_path->data, ctx->exe_path->length);
    if(fi->exists){
        MARRAY_FOR_EACH_VALUE(Atom, d, build->dependencies){
            BuildFileInfo* di = b_file_info(ctx, d->data, d->length);
            if(!di->exists) continue;
            if(!di->valid) continue;
            if(b_cmp_mtime(&fi->mtime, &di->mtime) < 0){
                should_recompile = 1;
                break;
            }
        }
    }
    else {
        should_recompile = 1;
    }
    if(!should_recompile) return 0;
    err = move_file(ctx, progpath->data, old->data);
    if(err){
        b_loglvl(BLOG_ERROR, ctx, "Unable to move '%s' to '%s'\n", progpath->data, old->data);
        return err;
    }
    cmd_resolve_prog_path(&build->cmd, &ctx->env, b_file_exists, ctx);
    print_command(ctx, &build->cmd);
    err = cmd_run(&build->cmd, ctx->envp, NULL);
    if(err){
        b_loglvl(BLOG_ERROR, ctx, "cmd failed: %d\n", err);
        move_file(ctx, old->data, progpath->data);
        return err;
    }
    else {
        err = rm_file(ctx, old->data);
        if(err)
            b_loglvl(BLOG_ERROR, ctx, "Error removing %s\n", old->data);
    }
    CmdBuilder cmd = {
        .allocator = allocator_from_arena(&ctx->tmp_aa),
    };
    cmd_prog(&cmd, AT_to_LS(ctx->exe_path));
    for(int i = 1; i < argc; i++)
        cmd_carg(&cmd, argv[i]);
    cmd_resolve_prog_path(&cmd, &ctx->env, b_file_exists, ctx);
    err = cmd_exec(&cmd, ctx->envp);
    return err;
}

static
Atom
get_git_hash(BuildCtx* ctx){
    if(ctx->git_hash) return ctx->git_hash;
    Atom result = nil_atom;
    Allocator tmp = allocator_from_arena(&ctx->tmp_aa);
    MStringBuilder pathsb = {.allocator=tmp}, headsb = {.allocator=tmp}, refsb = {.allocator=tmp};
    int err = b_read_file(ctx, ".git/HEAD", &headsb);
    if(err){
        b_loglvl(BLOG_WARN, ctx, "Unable to read '.git/HEAD'\n");
        goto finally;
    }
    StringView txt = stripped(msb_borrow_sv(&headsb));
    if(sv_startswith(txt, SV("ref: "))){
        txt = sv_slice(txt, 5, txt.length);
        msb_write_literal(&pathsb, ".git/");
        msb_write_str(&pathsb, txt.text, txt.length);
        LongString path = msb_borrow_ls(&pathsb);
        err = b_read_file(ctx, path.text, &refsb);
        if(err){
            b_loglvl(BLOG_WARN, ctx, "Unable to read '%s'\n", path.text);
            goto finally;
        }
        txt = stripped(msb_borrow_sv(&refsb));
    }
    result = b_atomize2(ctx, txt.text, txt.length);

    finally:
    msb_destroy(&refsb);
    msb_destroy(&pathsb);
    msb_destroy(&headsb);
    ctx->git_hash = result;
    return result;
}

static inline
void
ta_pusha(BuildCtx* ctx, Marray(Atom)* m, Atom a){
    int err = ma_push(Atom)(m, allocator_from_arena(&ctx->perm_aa), a);
    if(err) b_oom(ctx);
}

static inline
BuildTarget*
alloc_targeta(BuildCtx* ctx, Atom name){
    BuildTarget* t = AM_get(&ctx->targets, name);
    if(t) b_debug_break(ctx, "target already allocated");
    t = Allocator_zalloc(allocator_from_arena(&ctx->perm_aa), sizeof *t);
    if(!t) b_oom(ctx);
    int err = AM_put(&ctx->targets, allocator_from_arena(&ctx->perm_aa), name, t);
    if(err) b_oom(ctx);
    t->name = name;
    t->cmd.allocator = allocator_from_arena(&ctx->perm_aa);
    return t;
}

static inline
BuildTarget*
alloc_target(BuildCtx* ctx, const char* name){
    Atom a = b_atomize(ctx, name);
    return alloc_targeta(ctx, a);
}

static
BuildTarget*
phony_target(BuildCtx* ctx, const char* name){
    Atom a = b_atomize(ctx, name);
    return phony_targeta(ctx, a);
}

static
BuildTarget*
phony_targeta(BuildCtx* ctx, Atom name){
    BuildTarget* tgt = alloc_targeta(ctx, name);
    tgt->is_phony = 1;
    return tgt;
}

static
inline
BuildTarget* _Nullable
get_target(BuildCtx* ctx, const char* name){
    Atom a = b_atomize(ctx, name);
    return AM_get(&ctx->targets, a);
}

static
inline
BuildTarget* _Nullable
get_targeta(BuildCtx* ctx, Atom a){
    return AM_get(&ctx->targets, a);
}

static inline
BuildTarget*
src_filea(BuildCtx* ctx, Atom path){
    path = (Atom)b_normalize_patha(ctx, path);
    BuildTarget* t = get_targeta(ctx, path);
    if(t) return t;
    t = alloc_targeta(ctx, path);
    t->is_src = 1;
    return t;
}
static inline
BuildTarget*
src_file(BuildCtx* ctx, const char* src){
    Atom path = b_atomize(ctx, src);
    return src_filea(ctx, path);
}

static inline
BuildTarget*
gen_src_file(BuildCtx* ctx, const char* src){
    Atom path = b_atomize_f(ctx, "%s/%s", ctx->gen_dir->data, src);
    BuildTarget* t = src_filea(ctx, path);
    t->is_src = 0;
    t->is_generated = 1;
    return t;
}

static inline
void
add_dep(BuildCtx* ctx, BuildTarget* tgt, BuildTarget* dep){
    MARRAY_FOR_EACH_VALUE(Atom, d, tgt->dependencies)
        if(d == dep->name) return;
    ta_pusha(ctx, &tgt->dependencies, dep->name);
}
static inline
void
add_out(BuildCtx* ctx, BuildTarget* tgt, BuildTarget* out){
    MARRAY_FOR_EACH_VALUE(Atom, o, tgt->outputs)
        if(o == out->name) return;
    ta_pusha(ctx, &tgt->outputs, out->name);
}
static inline
void
add_deps_(BuildCtx* ctx, BuildTarget* tgt, size_t dep_count, BuildTarget*_Nonnull*_Nonnull dep){
    for(size_t i = 0; i < dep_count; i++)
        add_dep(ctx, tgt, dep[i]);
}


static inline
void
add_src_depa(BuildCtx* ctx, BuildTarget* tgt, Atom dep){
    BuildTarget* t = get_targeta(ctx, dep);
    if(!t) t = src_filea(ctx, dep);
    if(!t->is_src && !t->is_generated) b_debug_break(ctx, "t should be a generated src file");
    add_dep(ctx, tgt, t);
}
static inline
void
add_src_dep(BuildCtx* ctx, BuildTarget* tgt, const char* dep){
    Atom a = b_atomize(ctx, dep);
    add_src_depa(ctx, tgt, a);
}

static inline
BuildTarget*
exe_target(BuildCtx* ctx, const char* name, const char* src_dep, enum OS target_os){
    // char sep = BUILD_OS == OS_WINDOWS?'\\':'/';
    char sep = '/';
    Atom cc = ctx->target.cc;
    enum CompilerFlavor flavor = ctx->target.compiler_flavor;
    _Bool sanitize = ctx->target.sanitize;
    _Bool tsan = ctx->target.tsan;
    _Bool optimize = ctx->target.optimize;
    _Bool native = 0;
    _Bool debug = !ctx->target.no_debug_symbols;
    if(target_os == OS_NATIVE){
        target_os = BUILD_OS;
        cc = ctx->build_cc;
        flavor = ctx->build_compiler_flavor;
        optimize = 0;
        sanitize = ctx->target.native_sanitize;
        native = 1;
        debug = 1;
    }
    enum ArchFam arch = ctx->target.arch;
    if(arch == AFAM_NATIVE)
        native = 1;
    if(arch == AFAM_APPLE_UNIVERSAL){
        if(target_os != OS_APPLE){
            b_loglvl(BLOG_WARN, ctx, "Arch set to '%.*s', but not targeting apple os. Changing to x86.\n", (int)ArchFamSVs[arch].length, ArchFamSVs[arch].text);
            arch = AFAM_x86;
        }
    }
    const char* ext = target_os == OS_WINDOWS?".exe":"";
    Atom binary = b_atomize_f(ctx, "%s%c%s%s", ctx->build_dir->data, sep, name, ext);
    BuildTarget* target = alloc_targeta(ctx, binary);
    target->is_binary = 1;
    target->is_cmd = 1;
    target->is_compile_command = 1;
    CmdBuilder* cmd = &target->cmd;
    cmd->allocator = allocator_from_arena(&ctx->perm_aa);
    cmd_prog(cmd, AT_to_LS(cc));
    switch(flavor){
        case COMPILER_GCC_MINGW:
            cmd_cargs(cmd, "-std=gnu11");
            cmd_cargs(cmd, "-D__USE_MINGW_ANSI_STDIO=1");
            goto fallthrough;
        case COMPILER__MAX:
        case COMPILER_UNKNOWN:
        case COMPILER_GCC:
        case COMPILER_CLANG:
            fallthrough:;
            if(flavor == COMPILER_CLANG && target_os != BUILD_OS && target_os == OS_WINDOWS){
                cmd_cargs(cmd,
                    "--target=x86_64-pc-windows-msvc",
                    "-nostdinc");
            }
            if(debug) cmd_cargs(cmd, "-g");
            goto fallthrough2;
        case COMPILER_CLANG_CL:
            fallthrough2:;
            if(native)
                cmd_cargs(cmd, "-march=native");
            else if(arch == AFAM_x86){
                if(!(target_os == OS_APPLE && flavor == COMPILER_CLANG)) // should be apple clang, but we don't sniff that
                    // cmd_cargs(cmd, "-march=x86-64-v3");
                    // Old compilers don't support the above flag.
                    cmd_cargs(cmd, "-march=haswell");
            }
            else if(arch == AFAM_APPLE_UNIVERSAL){
                cmd_cargs(cmd,
                    "-arch", "x86_64",
                    "-Xarch_x86_64", "-march=haswell",
                    "-Xarch_x86_64", "-mmacosx-version-min=10.11",
                    "-arch", "arm64",
                    "-Xarch_arm64", "-mcpu=apple-m1",
                    "-Xarch_arm64", "-mmacosx-version-min=11.0"
                );
            }
            cmd_cargs(cmd, "-o", binary->data);
            cmd_cargs(cmd, "-MT", binary->data, "-MMD", "-MP", "-MF");
            cmd_argf(cmd, "%s/%s.deps", ctx->deps_dir->data, name);
            if(tsan && sanitize) cmd_cargs(cmd, "-fsanitize=thread,undefined");
            else if(tsan) cmd_cargs(cmd, "-fsanitize=thread");
            else if(sanitize) cmd_cargs(cmd, "-fsanitize=address,undefined");
            if(optimize) cmd_cargs(cmd, "-O2");
            break;
        case COMPILER_CL:
            cmd_cargs(cmd, "/nologo", "/std:c11");
            if(debug){
                cmd_cargs(cmd, "/Zi", "/DEBUG");
                cmd_argf(cmd, "/Fd:%s/%s.pdb", ctx->build_dir->data, name);
            }
            if(optimize) cmd_cargs(cmd, "/O2");
            cmd_argf(cmd, "/Fe:%s", binary->data);
            cmd_argf(cmd, "/Fo:%s/%s.obj", ctx->build_dir->data, name);
            cmd_carg(cmd, "/sourceDependencies");
            cmd_argf(cmd, "%s/%s.deps.json", ctx->deps_dir->data, name);
            break;
    }
    cmd_argf(cmd, "-I%s", ctx->gen_dir->data);
    target_src_inp(ctx, target, src_dep);
    if(target_os == OS_LINUX){
        cmd_cargs(cmd, "-lm", "-lpthread");
        if(flavor == COMPILER_GCC) cmd_carg(cmd, "-latomic");
    }
    BuildTarget* phony = phony_target(ctx, name);
    add_dep(ctx, phony, target);
    return target;
}

static inline
BuildTarget*
cmd_target(BuildCtx* ctx, const char* name){
    BuildTarget* target = alloc_target(ctx, name);
    target->is_cmd = 1;
    return target;
}
static inline
BuildTarget*
exec_target(BuildCtx* ctx, const char* name, BuildTarget* t){
    BuildTarget* target = alloc_target(ctx, name);
    target->is_cmd = 1;
    target->should_exec = 1;
    target_prog(ctx, target, t);
    return target;
}

static inline
BuildTarget*
bin_target(BuildCtx* ctx, const char* name){
    Atom a = b_atomize_f(ctx, "%s/%s", ctx->build_dir->data, name);
    BuildTarget* target = alloc_targeta(ctx, a);
    target->is_binary = 1;
    target->is_generated = 1;
    return target;
}

static
int
mkdir_script(BuildCtx* ctx, BuildTarget* tgt){
    b_log(ctx, "mkdir -p '%s'\n", tgt->name->data);
    return mkdirs_if_not_exists(ctx, (LongString){tgt->name->length, tgt->name->data});
}

static inline
BuildTarget*
directory_target(BuildCtx* ctx, const char* name){
    BuildTarget* t = script_target(ctx, name, mkdir_script, NULL);
    t->order_only = 1;
    ta_pusha(ctx, &t->outputs, t->name);
    return t;
}

static
void
print_target(BuildCtx* ctx, BuildTarget* tgt){
    b_log(ctx, "%s:", tgt->name->data);
    MARRAY_FOR_EACH_VALUE(Atom, d, tgt->dependencies)
        b_log(ctx, " \\\n  %s", d->data);
    b_log(ctx, "\n");
}
static
void
target_prog(BuildCtx* ctx, BuildTarget* tgt, BuildTarget* prog){
    if(!tgt->is_cmd) b_debug_break(ctx, "tgt is not command");
    if(!prog->is_binary) b_debug_break(ctx, "prog is not a binary");
    add_dep(ctx, tgt, prog);
    cmd_prog(&tgt->cmd, (LongString){prog->name->length, prog->name->data});
}

static
void
target_inp(BuildCtx* ctx, BuildTarget* tgt, BuildTarget* inp){
    if(!tgt->is_cmd) b_debug_break(ctx, "tgt is not command");
    add_dep(ctx, tgt, inp);
    cmd_arg(&tgt->cmd, (LongString){inp->name->length, inp->name->data});
}
static
void
target_inps_(BuildCtx* ctx, BuildTarget* tgt, size_t n, BuildTarget*_Nonnull*_Nonnull inp){
    for(size_t i = 0; i < n; i++)
        target_inp(ctx, tgt, inp[i]);
}

static
void
target_src_inp(BuildCtx* ctx, BuildTarget* tgt, const char* inp_){
    BuildTarget* inp = src_file(ctx, inp_);
    if(!tgt->is_cmd) b_debug_break(ctx, "tgt is not command");
    add_dep(ctx, tgt, inp);
    cmd_arg(&tgt->cmd, (LongString){inp->name->length, inp->name->data});
}
static
void
target_src_inps_(BuildCtx* ctx, BuildTarget* tgt, size_t n, const char*_Nonnull*_Nonnull inp_){
    for(size_t i = 0; i < n; i++)
        target_src_inp(ctx, tgt, inp_[i]);
}
static
void
target_arg(BuildCtx* ctx, BuildTarget* tgt, const char* arg){
    (void)ctx;
    cmd_arg(&tgt->cmd, (LongString){strlen(arg), arg});
}
static
void
target_argf(BuildCtx* ctx, BuildTarget* tgt, const char* fmt, ...){
    (void)ctx;
    va_list vap;
    va_start(vap, fmt);
    cmd_vargf(&tgt->cmd, fmt, vap);
    va_end(vap);
}
static
void
target_out(BuildCtx* ctx, BuildTarget* tgt, BuildTarget* out){
    if(!tgt->is_cmd) b_debug_break(ctx, "tgt is not command");
    if(!out->is_generated) b_debug_break(ctx, "out is is not generated");
    add_dep(ctx, out, tgt);
    cmd_arg(&tgt->cmd, (LongString){out->name->length, out->name->data});
    add_out(ctx, tgt, out);
}

static
void
target_argout(BuildCtx* ctx, BuildTarget *tgt, const char* arg, BuildTarget* out){
    target_arg(ctx, tgt, arg);
    target_out(ctx, tgt, out);
}
static
void
target_arginp(BuildCtx* ctx, BuildTarget *tgt, const char* arg, BuildTarget* inp){
    target_arg(ctx, tgt, arg);
    target_inp(ctx, tgt, inp);
}
static
void
target_argsrc(BuildCtx* ctx, BuildTarget *tgt, const char* arg, const char* inp){
    target_arg(ctx, tgt, arg);
    target_src_inp(ctx, tgt, inp);
}

static inline
BuildTarget*
script_targeta(BuildCtx* ctx, Atom name, int (*script)(BuildCtx*, BuildTarget*), const void*_Null_unspecified ud){
    BuildTarget* t = alloc_targeta(ctx, name);
    t->is_script = 1;
    t->script = script;
    t->user_data = ud;
    return t;
}

static inline
BuildTarget*
script_target(BuildCtx* ctx, const char* name, int (*script)(BuildCtx*, BuildTarget*), const void*_Null_unspecified ud){
    Atom a = b_atomize_f(ctx, "%s", name);
    return script_targeta(ctx, a, script, ud);
}

static inline
BuildTarget*
coro_targeta(BuildCtx* ctx, Atom name, int (*coro)(BuildCtx*, BuildTarget*), const void*_Null_unspecified ud){
    BuildTarget* t = alloc_targeta(ctx, name);
    t->is_coro = 1;
    t->corop = coro;
    t->user_data = ud;
    return t;
}

static inline
BuildTarget*
coro_target(BuildCtx* ctx, const char* name, int (*coro)(BuildCtx*, BuildTarget*), const void*_Null_unspecified ud){
    Atom a = b_atomize_f(ctx, "%s", name);
    return coro_targeta(ctx, a, coro, ud);
}


static
Atom
b_atomize(BuildCtx* ctx, const char* txt){
    size_t len = strlen(txt);
    return b_atomize2(ctx, txt, len);
}

static
Atom
#ifdef __GNUC__
__attribute__((format(printf, 2, 3)))
#endif
b_atomize_f(BuildCtx* ctx, const char* fmt, ...){
    va_list vap;
    va_start(vap, fmt);
    Atom a = AT_atomize_fv(&ctx->at, fmt, vap);
    va_end(vap);
    if(!a) b_oom(ctx);
    return a;
}

static
Atom
b_atomize2(BuildCtx* ctx, const char* txt, size_t len){
    Atom a = AT_atomize(&ctx->at, txt, len);
    if(!a) b_oom(ctx);
    return a;
}

static
int
write_to_json_file(BuildCtx* ctx, const void* src, const TypeInfo* ti, Atom path){
    DrJsonContext* jsctx = NULL;
    jsctx = drjson_create_ctx(allocator_from_arena(&ctx->tmp_aa), &ctx->at);
    int err = 0;
    if(jsctx){
        DrJsonValue cache = any_to_json(src, ti, jsctx);
        if(cache.kind != DRJSON_ERROR){
            MStringBuilder tmp_path = {.allocator = allocator_from_arena(&ctx->tmp_aa)};
            msb_write_str(&tmp_path, path->data, path->length);
            msb_write_literal(&tmp_path, ".tmp");
            msb_nul_terminate(&tmp_path);
            if(!tmp_path.errored){
                LongString t = msb_borrow_ls(&tmp_path);
                enum {printflags = DRJSON_PRINT_PRETTY | DRJSON_PRINT_APPEND_NEWLINE};
                if(BUILD_OS == OS_WINDOWS){
                    MStringBuilder16 sb = {.allocator=allocator_from_arena(&ctx->tmp_aa)};
                    msb16_write_utf8(&sb, t.text, t.length);
                    msb16_nul_terminate(&sb);
                    #ifdef _WIN32
                    HANDLE h = CreateFileW((wchar_t*)sb.data, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
                    if(h && h != INVALID_HANDLE_VALUE){
                        drjson_print_value_HANDLE(jsctx, h, cache, 0, printflags);
                        CloseHandle(h);
                    }
                    else{
                        err = 1;
                        if(err) b_loglvl(BLOG_DEBUG, ctx, "%d: fuck '%s'\n", __LINE__, path->data);
                    }
                    #endif
                    msb16_destroy(&sb);
                }
                else {
                    #ifndef _WIN32
                    int fd = open(t.text, O_CREAT|O_TRUNC|O_WRONLY, 0644);
                    if(fd >= 0){
                        err = drjson_print_value_fd(jsctx, fd, cache, 0, printflags);
                        close(fd);
                        if(err) b_loglvl(BLOG_DEBUG, ctx, "%d: fuck '%s'\n", __LINE__, path->data);
                    }
                    else{
                        err = 1;
                        if(err)
                            b_loglvl(BLOG_DEBUG, ctx, "%d: fuck '%s'\n", __LINE__, path->data);
                    }
                    #endif
                }
                if(!err){
                    err = move_file(ctx, t.text, path->data);
                    if(err) b_loglvl(BLOG_DEBUG, ctx, "%d: fuck '%s'\n", __LINE__, path->data);
                }
            }
            else {
                err = 1;
                if(err) b_loglvl(BLOG_DEBUG, ctx, "%d: fuck '%s'\n", __LINE__, path->data);
            }
            msb_destroy(&tmp_path);
        }
        else{
            err = 1;
            if(err) b_loglvl(BLOG_DEBUG, ctx, "%d: fuck '%s'\n", __LINE__, path->data);
        }
        drjson_ctx_free_all(jsctx);
    }
    else{
        err = 1;
        if(err) b_loglvl(BLOG_DEBUG, ctx, "%d: fuck '%s'\n", __LINE__, path->data);
    }
    return err;
}

static
int
read_from_json_file(BuildCtx* ctx, void* dst, const TypeInfo* ti, Atom path){
    int err = 0;
    Allocator tmp = allocator_from_arena(&ctx->tmp_aa);
    MStringBuilder sb = {.allocator=tmp};
    err = b_read_file(ctx, path->data, &sb);
    if(err){
        err = 0;
        goto finally;
    }
    LongString text = msb_borrow_ls(&sb);
    err = any_from_json_txt(dst, ti, text, &ctx->at, tmp, allocator_from_arena(&ctx->perm_aa));
    if(err) b_loglvl(BLOG_ERROR, ctx, "Error parsing %s\n", path->data);
    finally:
    msb_destroy(&sb);
    return err;
}

static
int
b_run_cmd_async(BuildCtx* ctx, BuildTarget* target, CmdBuilder* cmd){
    if(!ctx->jobs.cap){
        ctx->jobs.cap = (size_t)ctx->njobs;
        Allocator a = allocator_from_arena(&ctx->perm_aa);
        void* p;

        p = Allocator_alloc(a, ctx->jobs.cap * sizeof *ctx->jobs.jobs);
        if(!p) return 1;
        ctx->jobs.jobs = p;

        p = Allocator_alloc(a, ctx->jobs.cap * sizeof *ctx->jobs.processes);
        if(!p) return 1;
        ctx->jobs.processes = p;
    }
    if(ctx->jobs.count >= ctx->jobs.cap)
        b_debug_break(ctx, "jobs queue is full");
    intptr_t* proc = &ctx->jobs.processes[ctx->jobs.count];
    BuildJob* job = &ctx->jobs.jobs[ctx->jobs.count];
    cmd_resolve_prog_path(cmd, &ctx->env, b_file_exists, ctx);
    print_command(ctx, cmd);
    int err = cmd_run(cmd, ctx->envp, proc);
    if(err){
        b_loglvl(BLOG_ERROR, ctx, "Error running command: ");
        print_command(ctx, cmd);
        return err;
    }
    ctx->jobs.count++;
    *job = (BuildJob){
        .started = 1,
        .status = -1,
        .target = target,
    };
    return 0;
}

static
int
b_run_cmd_sync(BuildCtx* ctx, CmdBuilder* cmd){
    cmd_resolve_prog_path(cmd, &ctx->env, b_file_exists, ctx);
    print_command(ctx, cmd);
    int err = cmd_run(cmd, ctx->envp, NULL);
    if(err){
        b_loglvl(BLOG_ERROR, ctx, "Error running command: ");
        print_command(ctx, cmd);
        return err;
    }
    return 0;
}

static
void
b_debug_break(BuildCtx* ctx, const char* reason){
    b_loglvl(BLOG_ERROR, ctx, "Breakpoint: %s\n", reason);
    __builtin_debugtrap();
}

static
_Noreturn
void
b_abort(BuildCtx* ctx, const char* reason){
    b_log_(ctx, reason, strlen(reason));
    __builtin_trap();
}

static
_Noreturn
void
b_oom(BuildCtx* ctx){
    b_abort(ctx, "Aborting due to out of memory\n");
}

#ifdef __clang__
#pragma clang assume_nonnull end
#endif
#include "Allocators/allocator.c"
#define STB_SPRINTF_IMPLEMENTATION 1
#include "../Vendored/stb/stb_sprintf.h"
#include "drjson.c"
#endif
