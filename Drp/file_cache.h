#ifndef DRP_FILE_CACHE_H
#define DRP_FILE_CACHE_H
//
// Copyright © 2026-2026, David Priver <david@davidpriver.com>
//
#include <stddef.h>
#include "long_string.h"
#include "MStringBuilder.h"
#include "Allocators/allocator.h"
#ifdef __clang__
#pragma clang assume_nonnull begin
#else
#ifndef _Nullable
#define _Nullable
#endif
#ifndef _Null_unspecified
#define _Null_unspecified
#endif
#endif

enum {
    FC_OK                   = 0,  // _cc_no_error
    FC_ERROR_OOM            = 1,  // _cc_oom_error
    FC_ERROR_NOT_FOUND      = 5,  // _cc_file_not_found_error
    FC_ERROR_IO             = 11, // _cc_io_error
    FC_ERROR_NOT_FILE       = 12, // _cc_not_a_file_error
    FC_ERROR_ALREADY_CACHED = 13, // _cc_already_cached_error
};

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
// Returns FC_OK if the file exists and is a regular file.
// Returns FC_ERROR_NOT_FOUND if not found, FC_ERROR_OOM on allocation failure, etc.
static int fc_is_file(FileCache*);
// Attempts to read the file into data. Might be cached, might not be.
// You might need to skip any BOM yourself. Not nul-terminated.
// Returns FC_OK on success, FC_ERROR_* on failure.
static int fc_read_file(FileCache*, StringView* data);
// Attempts to obtain the file's size.
// Might be cached and so there is a possible race condition, but usually that is ok.
// Returns FC_OK on success, FC_ERROR_* on failure.
static int fc_get_size(FileCache*, size_t* sz);
// Caches a file into the cache, bypassing the filesystem
// For virtual files or for overriding what the system thinks is actually
// in the file.
// Data is copied.
// Still need to go through the `fc_write_path` API.
// Returns FC_ERROR_ALREADY_CACHED if the file is already in the cache.
static int fc_cache_file(FileCache*, StringView data);

typedef struct CachedFile CachedFile;
struct CachedFile {
    LongString path;
    uint32_t hash;

    uint32_t valid:       1;
    uint32_t exists:      1;
    uint32_t unreadable:  1;
    uint32_t is_file:     1;
    uint32_t data_cached: 1;
    uint32_t size_cached: 1;
    uint32_t _padding:   26;
    size_t data_size; // from stat
    struct {
        const void *_Null_unspecified buff;
        size_t n_bytes;
    } data;
};
typedef struct FileCache FileCache;
struct FileCache {
    _Bool may_read_real_files: 1;
    Allocator allocator;
    MStringBuilder path_builder;
    struct {
        CachedFile *data;
        size_t count;
        size_t cap;
    } map;
};

#ifdef __clang__
#pragma clang assume_nonnull end
#endif
#endif
