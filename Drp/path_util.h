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
#if defined __GNUC__ || defined __clang__ || defined __DRC__
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
path_is_sep(char c, _Bool windows){
    if(windows)
        return c == '/' || c == '\\';
    else
        return c == '/';
}

// Helper to find the next slash in a string, but also finding backslashes
// on Windows.
force_inline
const void*_Nullable
path_memsep(const char* str, size_t length, _Bool windows){
    const char* slash = memchr(str, '/', length);
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
        return path_is_sep(path.text[0], windows);
    if(path_is_sep(path.text[0], windows))
        return 1;
    if(path.length > 2){
        if(path.text[1] == ':')
            return path_is_sep(path.text[2], windows);
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
        const char* slash = path_memsep(basename, end-basename, windows);
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
        const char* slash = path_memsep(basename, end-basename, windows);
        if(!slash)
            break;
        basename = slash+1;
    }
    StringView result = {basename - path.text, path.text};
    while(result.length > 1+drive_len && path_is_sep(result.text[result.length-1], windows)){
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
            if(path_is_sep(path.text[offset-1], windows)) return SV("");

            return (StringView){path.length-offset, path.text+offset};
        }
        if(path_is_sep(path.text[offset], windows))
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
            if(path_is_sep(path.text[offset-1], windows)) return path;
            return (StringView){offset, path.text};
        }
        if(path_is_sep(path.text[offset], windows))
            return path;
    }
    return path;
}


static
size_t
path_skip_seps(const char *buff, size_t pos, size_t len, _Bool is_windows){
    while (pos < len && path_is_sep(buff[pos], is_windows))
        ++pos;

    return pos;
}

typedef struct PathRoot PathRoot;
struct PathRoot {
    size_t length; // Includes trailing separators
    _Bool absolute;
    _Bool unc;
};

static
PathRoot
path_parse_root(StringView path, _Bool windows){
    const char* text = path.text;
    size_t n = path.length;
    if(!n) return (PathRoot){0};
    if(windows && n >= 2 && (text[0] | 0x20) >= 'a' && (text[0] | 0x20) <= 'z' && text[1] == ':'){
        size_t end = path_skip_seps(text, 2, n, windows);
        return (PathRoot){.length = end, .absolute = end > 2};
    }
    if(!path_is_sep(text[0], windows)) return (PathRoot){0};
    if(!windows || n < 2 || !path_is_sep(text[1], windows))
        return (PathRoot){.length = path_skip_seps(text, 1, n, windows), .absolute = 1};

    // //server/share, //?/C:, //./C:, or //?/UNC/server/share.
    StringView components[2] = {0};
    size_t p = 2, count = 0, required = 2;
    while(count < required){
        p = path_skip_seps(text, p, n, windows);
        if(p == n) break;
        size_t start = p;
        while(p < n && !path_is_sep(text[p], windows)) p++;
        if(count < 2) components[count] = (StringView){p - start, text + start};
        count++;
        if(count == 2 && components[0].length == 1
        && (components[0].text[0] == '?' || components[0].text[0] == '.')
        && sv_iequals2(SV("UNC"), components[1].text, components[1].length))
            required = 4;
    }
    return (PathRoot){.length = path_skip_seps(text, p, n, windows), .absolute = 1, .unc = 1};
}

static
void
path_normalize_keep_relative(char *buff, size_t *len, _Bool is_windows){
    if(*len == 0) return;
    const size_t n = *len;
    PathRoot root = path_parse_root((StringView){n, buff}, is_windows);
    size_t r = 0, w = 0;
    if(root.unc){
        buff[w++] = '/';
        buff[w++] = '/';
        r = path_skip_seps(buff, 2, root.length, is_windows);
    }
    while(r < root.length){
        if(path_is_sep(buff[r], is_windows)){
            r = path_skip_seps(buff, r, root.length, is_windows);
            if(!root.unc || r < root.length) buff[w++] = '/';
        }
        else buff[w++] = buff[r++];
    }
    size_t floor = w;
    _Bool absolute = root.absolute;
    _Bool root_needs_sep = root.unc && w > 2;
    while(r < n){
        r = path_skip_seps(buff, r, n, is_windows);
        if(r == n) break;
        const size_t start = r;
        while(r < n && !path_is_sep(buff[r], is_windows))
            ++r;
        const size_t clen = r - start;
        if(clen == 1 && buff[start] == '.')
            continue;
        if(clen == 2 && buff[start] == '.' && buff[start + 1] == '.'){
            size_t prev = w;
            while (prev > floor && buff[prev - 1] != '/')
                --prev;
            const size_t prev_len = w - prev;
            if(prev_len != 0 && !(prev_len == 2 && buff[prev] == '.' && buff[prev + 1] == '.')){
                w = (prev > floor) ? prev - 1 : floor;
                continue;
            }
            if (absolute)
                continue;
        }
        if(w > floor || (w == floor && root_needs_sep))
            buff[w++] = '/';
        for (size_t i = 0; i < clen; ++i)
            buff[w + i] = buff[start + i];
        w += clen;
    }
    if(w == 0){
        buff[0] = '.';
        w = 1;
    }
    *len = w;
}

#ifdef __clang__
#pragma clang assume_nonnull end
#endif
#endif
