#ifndef DRP_FILE_CACHE_H
#define DRP_FILE_CACHE_H
//
// Copyright © 2026-2026, David Priver <david@davidpriver.com>
//
#include <stddef.h>
#include "long_string.h"
#include "Allocators/allocator.h"
#ifdef __clang__
#pragma clang assume_nonnull begin
#else
#ifndef _Nullable
#define _Nullable
#endif
#endif

typedef struct FileCache FileCache;
// Create a new file cache, allocated by the allocator.
// The allocator is retained by the file cache for memory allocation.
static FileCache*_Nullable fc_create(Allocator);
// Deallocates the filecache and all resources referenced by the file cache.
// This deallocs any read files!
static void fc_destroy(FileCache*);
// Writes a path component to the file cache's buffer. utf-8
static void fc_write_path(FileCache*, const char*, size_t);
// Writes a formatted path component to the file cache's buffer. utf-8.
static void fc_write_pathf(FileCache*, const char* fmt, ...);
// For complicated path building, provides access to the caches path builder.
typedef struct MStringBuilder MStringBuilder;
static MStringBuilder* fc_path_builder(FileCache*);
// Returns if the file exists and is a file, false otherwise.
static _Bool fc_is_file(FileCache*);
// Attempts to read the file into data. Might be cached, might not be.
// You might need to skip any BOM yourself. Not nul-terminated.
// Returns 0 on success, errno or GetLastError() on failure.
static int fc_read_file(FileCache*, StringView* data);
// Attempts to obtain the file's size.
// Might be cached and so there is a possible race condition, but usually that is ok.
// Returns 0 on success, errno or GetLastError() on failure.
static int fc_get_size(FileCache*, size_t* sz);

#ifdef __clang__
#pragma clang assume_nonnull end
#endif
#endif
