//
// Copyright © 2021-2024, David Priver <david@davidpriver.com>
//
#ifndef DRP_PATH_UTIL_H
#define DRP_PATH_UTIL_H
// size_t
#include <stddef.h>
// memchr
#include <string.h>
#include "stringview.h"

#ifndef force_inline
#if defined(__GNUC__) || defined(__clang__)
#define force_inline static inline __attribute__((always_inline))
#elif defined(_MSC_VER)
#define force_inline static inline __forceinline
#else
#define force_inline static inline
#endif
#endif

#ifdef __clang__
#pragma clang assume_nonnull begin
#else
#ifndef _Nullable
#define _Nullable
#endif
#endif

// Helper to distinguish what is a path separator.
force_inline
_Bool
is_sep(char c, _Bool windows){
    if(windows)
        return c == '/' || c == '\\';
    else
        return c == '/';
}

// Helper to find the next slash in a string, but also finding backslashes
// on Windows.
force_inline
void*_Nullable
memsep(const char* str, size_t length, _Bool windows){
    char* slash = memchr(str, '/', length);
    if(windows && !slash)
        slash = memchr(str, '\\', length);
    return slash;
}

//
// Returns if the path is an absolute path (aka starts from /).
//
static inline
_Bool
path_is_abspath(StringView path, _Bool windows){
    if(!path.length)
        return 0;
    if(!windows)
        return is_sep(path.text[0], windows);
    if(is_sep(path.text[0], windows))
        return 1;
    if(path.length > 2){
        if(path.text[1] == ':')
            return is_sep(path.text[2], windows);
    }
    return 0;
}

//
// Returns the filename component of a path. If the path ends
// with a slash, returns the empty string.
// This is different than the posix utility basename.
//
static inline
StringView
path_basename(StringView path, _Bool windows){
    if(!path.length)
        return path;
    const char* basename = path.text;
    const char* end = path.text + path.length;
    // probably more efficient way of doing these.
    // Wish there was a reverse memchr
    for(;basename != end;){
        const char* slash = memsep(basename, end-basename, windows);
        if(!slash)
            break;
        basename = slash+1;
    }
    return (StringView){end-basename, basename};
}

//
// Returns the directory portion of the filename.
// Trailing slashes are stripped from the result, unless the final
// path is exactly equal to '/'. This allows you to distinguish
// '/foo' from 'foo' without needing to return a '.' that's not in
// the original string (as doing so would break pointer arithmetic).
//
// On windows, includes the leading drive if included
//
static inline
StringView
path_dirname(StringView path, _Bool windows){
    size_t drive_len = 0;
    if(windows && path.length > 1 && path.text[1] == ':' && (path.text[0] | 0x20) >= 'a' && (path.text[0] | 0x20) <= 'z')
        drive_len = 2;
    if(path.length == drive_len)
        return path;
    const char* basename = path.text;
    const char* end = path.text + path.length;
    for(;basename != end;){
        const char* slash = memsep(basename, end-basename, windows);
        if(!slash)
            break;
        basename = slash+1;
    }
    StringView result = {basename - path.text, path.text};
    while(result.length > 1+drive_len && is_sep(result.text[result.length-1], windows)){
        result.length--;
    }
    return result;
}

//
// Returns the extension part of a string.
//
// Turns "/foo/bar/baz.html" into ".html"
// Turns "/foo/bar" into ""
//
static inline
StringView
path_extension(StringView path, _Bool windows){
    if(!path.length) return path;
    size_t offset = path.length;
    while(--offset){
        if(path.text[offset] == '.'){
            if(offset == path.length-1) return SV("");
            if(is_sep(path.text[offset-1], windows)) return SV("");

            return (StringView){path.length-offset, path.text+offset};
        }
        if(is_sep(path.text[offset], windows))
            return SV("");
    }
    return SV("");
}

//
// Removes the extension part of a string.
//
// Turns /foo/bar/baz.html into /foo/bar/baz
// Turns /foo/bar into /foo/bar
//
static inline
StringView
path_strip_extension(StringView path, _Bool windows){
    if(!path.length)
        return path;
    size_t offset = path.length;
    while(--offset){
        if(path.text[offset] == '.'){
            if(offset == path.length-1) return path;
            if(is_sep(path.text[offset-1], windows)) return path;
            return (StringView){offset, path.text};
        }
        if(is_sep(path.text[offset], windows))
            return path;
    }
    return path;
}

#ifdef __clang__
#pragma clang assume_nonnull end
#endif
#endif
