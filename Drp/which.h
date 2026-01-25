



#ifndef DRP_WHICH_H
#define DRP_WHICH_H
#include <string.h>
#include <stddef.h>
#include "MStringBuilder.h"
#include "env.h"
#include "stringview.h"

#ifdef __clang__
#pragma clang assume_nonnull begin
#endif

typedef _Bool exist_func(void*_Null_unspecified, const char*, size_t);

static
int
_env_resolve_windows_ext(StringView exts, MStringBuilder* outsb, exist_func* exists, void*_Null_unspecified exist_ctx){
    // Check if already ends with an executable extension.
    for(const char* e = exts.text, *sep = memchr(exts.text, ';', exts.length);; e = sep+1, sep=memchr(e, ';', exts.length-(e-exts.text))){
        if(!sep) sep = exts.text + exts.length;
        StringView ex = {sep-e, e};
        StringView sv = msb_borrow_sv(outsb);
        if(sv_iendswith(sv, ex)){
            if(exists(exist_ctx, sv.text, sv.length))
                return 0;
            return 1;
        }
        if(sep == exts.text + exts.length) break;
    }
    // Check if appending .exe, etc. makes it a valid path.
    for(const char* e = exts.text, *sep = memchr(exts.text, ';', exts.length);; e = sep+1, sep=memchr(e, ';', exts.length-(e-exts.text))){
        if(!sep) sep = exts.text + exts.length;
        StringView ex = {sep-e, e};
        msb_write_str(outsb, ex.text, ex.length);
        msb_nul_terminate(outsb);
        if(outsb->errored) return 1;
        StringView sv = msb_borrow_sv(outsb);
        if(exists(exist_ctx, sv.text, sv.length))
            return 0;
        msb_erase(outsb, ex.length);
        if(sep == exts.text + exts.length) break;
    }
    return 1;
}

static
int
env_resolve_prog_path_win32(Environment* env, StringView cmd, MStringBuilder* outsb, exist_func* exists, void*_Null_unspecified exist_ctx){
    if(!cmd.length) return 1;
    int err;
    StringView dirpart = {0}, filepart = {0};
    for(size_t i = cmd.length; i--; ){
        if(cmd.text[i] == '/' || cmd.text[i] == '\\'){
            dirpart = (StringView){i+1, cmd.text};
            filepart = (StringView){cmd.length-i-1, cmd.text+i+1};
            break;
        }
    }
    if(!dirpart.length && !filepart.length){
        filepart = cmd;
    }
    if(!filepart.length) return 1;
    StringView exts = {0};
    exts = SV(".exe");
    Atom pathexts = env_getenv2(env, "PATHEXT", sizeof "PATHEXT" -1);
    if(pathexts && pathexts->length)
        exts = (StringView){pathexts->length, pathexts->data};
    if(dirpart.length){
        msb_write_str(outsb, cmd.text, cmd.length);
        msb_nul_terminate(outsb);
        if(outsb->errored) return 1;
        err = _env_resolve_windows_ext(exts, outsb, exists, exist_ctx);
        if(err) msb_reset(outsb);
        return err;
    }
    {
        // check cwd
        msb_write_str(outsb, cmd.text, cmd.length);
        msb_nul_terminate(outsb);
        if(outsb->errored) return 1;
        err = _env_resolve_windows_ext(exts, outsb, exists, exist_ctx);
        if(!err) return 0;
    }
    Atom path = env_getenv2(env, "PATH", sizeof "PATH" - 1);
    if(!path || !path->length)
        return 1;
    StringView PATH = {path->length, path->data};
    char separator = ';';
    for(const char* p = PATH.text; ;){
        size_t plen = PATH.text+PATH.length-p;
        const char* s = memchr(p, separator, plen);
        StringView directory;
        if(!s)
            directory = (StringView){plen, p};
        else{
            directory = (StringView){s-p, p};
            p = s+1;
        }
        if(!directory.length){
            if(!s) break;
            else continue;
        }
        msb_reset(outsb);
        msb_write_str(outsb, directory.text, directory.length);
        char c = directory.text[directory.length];
        _Bool append_slash = 1;
        if(c == '\\')
            append_slash = 0;
        if(append_slash && c == '/')
            append_slash = 0;
        if(append_slash)
            msb_write_char(outsb, '/');
        msb_write_str(outsb, cmd.text, cmd.length);
        msb_nul_terminate(outsb);
        if(outsb->errored) return 1;
        err = _env_resolve_windows_ext(exts, outsb, exists, exist_ctx);
        if(!err) return 0;
        if(!s) break;
    }
    msb_reset(outsb);
    return 1;
}

static
int
env_resolve_prog_posix(Environment* env, StringView cmd, MStringBuilder* outsb, exist_func* exists, void*_Null_unspecified exist_ctx){
    if(!cmd.length) return 1;
    StringView dirpart = {0}, filepart = {0};
    for(size_t i = cmd.length; i--; ){
        if(cmd.text[i] == '/'){
            dirpart = (StringView){i+1, cmd.text};
            filepart = (StringView){cmd.length-i-1, cmd.text+i+1};
            break;
        }
    }
    if(!dirpart.length && !filepart.length){
        filepart = cmd;
    }
    if(!filepart.length) return 1;
    if(dirpart.length){
        msb_write_str(outsb, cmd.text, cmd.length);
        msb_nul_terminate(outsb);
        if(outsb->errored) return 1;
        StringView sv = msb_borrow_sv(outsb);
        if(exists(exist_ctx, sv.text, sv.length))
            return 0;
        msb_reset(outsb);
        return 1;
    }
    Atom path = env_getenv2(env, "PATH", sizeof "PATH" - 1);
    if(!path || !path->length)
        return 1;
    StringView PATH = {path->length, path->data};
    char separator = ':';
    for(const char* p = PATH.text; ;){
        size_t plen = PATH.text+PATH.length-p;
        const char* s = memchr(p, separator, plen);
        StringView directory;
        if(!s)
            directory = (StringView){plen, p};
        else{
            directory = (StringView){s-p, p};
            p = s+1;
        }
        if(!directory.length){
            if(!s) break;
            else continue;
        }
        msb_reset(outsb);
        msb_write_str(outsb, directory.text, directory.length);
        char c = directory.text[directory.length];
        _Bool append_slash = 1;
        if(append_slash && c == '/')
            append_slash = 0;
        if(append_slash)
            msb_write_char(outsb, '/');
        msb_write_str(outsb, cmd.text, cmd.length);
        msb_nul_terminate(outsb);
        if(outsb->errored) return 1;
        StringView sv = msb_borrow_sv(outsb);
        if(exists(exist_ctx, sv.text, sv.length))
            return 0;
        if(!s) break;
    }
    msb_reset(outsb);
    return 1;
}

static
int
env_resolve_prog_path(Environment* env, StringView cmd, MStringBuilder* outsb, exist_func* exists, void*_Null_unspecified exist_ctx){
    if(env->windows)
        return env_resolve_prog_path_win32(env, cmd, outsb, exists, exist_ctx);
    else
        return env_resolve_prog_posix(env, cmd, outsb, exists, exist_ctx);
}

#ifdef __clang__
#pragma clang assume_nonnull end
#endif

#endif
