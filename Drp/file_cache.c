#ifndef DRP_FILE_CACHE_C
#define DRP_FILE_CACHE_C
//
// Copyright © 2026-2026, David Priver <david@davidpriver.com>
//
#include <stdarg.h>
#include <string.h>
#include "file_cache.h"
#include "MStringBuilder.h"
#include "MStringBuilder16.h"
#include "msb_sprintf.h"
#include "hash_func.h"
#include "ByteBuffer.h"
#ifdef __clang__
#pragma clang assume_nonnull begin
#else
#ifndef _Nullable
#define _Nullable
#endif
#endif

static
FileCache*_Nullable
fc_create(Allocator a){
    FileCache* fc = Allocator_zalloc(a, sizeof *fc);
    if(!fc) return fc;
    fc->allocator = a;
    fc->path_builder.allocator = a;
    return fc;
}
static void fc_destroy(FileCache* fc){
    (void)fc; // TODO
}
static
void
fc_write_path(FileCache* fc, const char* txt, size_t len){
    msb_write_str(&fc->path_builder, txt, len);
}

static
void
fc_write_pathf(FileCache* fc, const char* fmt, ...){
    va_list va;
    va_start(va, fmt);
    msb_vsprintf(&fc->path_builder, fmt, va);
    va_end(va);
}
static
MStringBuilder*
fc_path_builder(FileCache* fc){
    return &fc->path_builder;
}

static
CachedFile *_Nullable
fc_get_entry(FileCache* fc){
    if(!fc->map.count) return NULL;
    StringView path = msb_borrow_sv(&fc->path_builder);
    uint32_t cap2 = (uint32_t)fc->map.cap*2;
    uint32_t hash = hash_align1(path.text, path.length);
    uint32_t *idxes = (uint32_t*)(void*)(fc->map.data + fc->map.cap);
    uint32_t idx = fast_reduce32(hash, cap2);
    for(;;){
        uint32_t i = idxes[idx];
        if(!i) return NULL;
        i--;
        CachedFile* f = &fc->map.data[i];
        if(f->hash == hash && sv_equals(LS_to_SV(f->path), path))
            return f;
        idx++;
        if(idx >= cap2) idx = 0;
    }
}

static
CachedFile *_Nullable
fc_create_entry(FileCache* fc){
    if(fc->map.count >= fc->map.cap){
        size_t old_cap = fc->map.cap;
        size_t old_size = old_cap*2*sizeof(uint32_t)+old_cap*sizeof *fc->map.data;
        size_t new_cap = old_cap?old_cap*2:32;
        size_t new_size = new_cap*2*sizeof(uint32_t)+new_cap*sizeof *fc->map.data;
        void *data = Allocator_realloc(fc->allocator, fc->map.data, old_size, new_size);
        if(!data) return NULL;
        CachedFile* items = data;
        uint32_t *idxes = (uint32_t*)(void*)(items + new_cap);
        memset(idxes, 0, new_cap*2*sizeof *idxes);
        uint32_t count = (uint32_t)fc->map.count;
        uint32_t new_count = 0; // this will end up being count, but if we add deletion on resize this will be useful
        uint32_t cap2 = (uint32_t)new_cap*2;
        for(uint32_t i = 0; i < count; i++){
            uint32_t hash = items[i].hash;
            uint32_t idx = fast_reduce32(hash, cap2);
            while(idxes[idx]){
                idx++;
                if(idx >= cap2) idx = 0;
            }
            idxes[idx] = ++new_count;
        }
        fc->map.count = new_count;
        fc->map.data = data;
        fc->map.cap = new_cap;
    }
    LongString path = msb_detach_ls(&fc->path_builder);
    uint32_t hash = hash_align1(path.text, path.length);
    uint32_t cap2 = (uint32_t)fc->map.cap*2;
    uint32_t *idxes = (uint32_t*)(void*)(fc->map.data + fc->map.cap);
    uint32_t idx = fast_reduce32(hash, cap2);
    CachedFile* f = &fc->map.data[fc->map.count++];
    memset(f, 0, sizeof *f);
    f->path = path;
    f->hash = hash;
    f->valid = 1;
    for(;;){
        uint32_t i = idxes[idx];
        if(!i){
            idxes[idx] = (uint32_t)fc->map.count;
            return f;
        }
        idx++;
        if(idx >= cap2) idx = 0;
    }
}

static
_Bool
fc_is_file(FileCache* fc){
    _Bool result = 0;
    CachedFile* f = fc_get_entry(fc);
    if(f && f->valid){
        result = f->is_file;
        goto finally;
    }
    if(!fc->may_read_real_files) goto finally;
    if(!f) f = fc_create_entry(fc);
    #ifdef _WIN32
    {
        MStringBuilder16 wb = {.allocator = fc->allocator};
        msb16_write_utf8(&wb, f->path.text, f->path.length);
        if(wb.errored){ msb16_destroy(&wb); goto finally; }
        msb16_nul_terminate(&wb);
        DWORD attrs = GetFileAttributesW((LPCWSTR)wb.data);
        msb16_destroy(&wb);
        f->valid = 1;
        if(attrs == INVALID_FILE_ATTRIBUTES){
            f->exists = 0;
            f->is_file = 0;
        }
        else {
            f->exists = 1;
            result = !(attrs & FILE_ATTRIBUTE_DIRECTORY);
            f->is_file = result;
        }
    }
    #else
    struct stat s;
    int err = stat(f->path.text, &s);
    if(err){
        f->valid = 1;
        f->exists = 0;
        f->is_file = 0;
    }
    else {
        f->valid = 1;
        f->exists = 1;
        result = S_ISREG(s.st_mode);
        f->is_file = result;
        f->size_cached = 1;
        f->data_size = s.st_size;
    }
    #endif
    finally:
    msb_reset(&fc->path_builder);
    return result;
}

static
int
fc_read_file(FileCache* fc, StringView* outdata){
    int result = ENOENT;
    #ifdef _WIN32
    HANDLE fh = INVALID_HANDLE_VALUE;
    #else
    int fd = -1;
    #endif
    CachedFile* f = fc_get_entry(fc);
    if(f && f->valid){
        if(f->data_cached){
            *outdata = (StringView){f->data.n_bytes, f->data.buff};
            result = 0;
            goto finally;
        }
    }
    if(!fc->may_read_real_files) goto finally;
    if(!f) f = fc_create_entry(fc);
    #ifdef _WIN32
    {
        MStringBuilder16 wb = {.allocator = fc->allocator};
        msb16_write_utf8(&wb, f->path.text, f->path.length);
        if(wb.errored){ msb16_destroy(&wb); goto finally; }
        msb16_nul_terminate(&wb);
        fh = CreateFileW((LPCWSTR)wb.data, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        msb16_destroy(&wb);
    }
    if(fh == INVALID_HANDLE_VALUE){
        f->valid = 1;
        f->unreadable = 1;
        f->is_file = 0;
        result = ENOENT;
        goto finally;
    }
    LARGE_INTEGER size;
    if(!GetFileSizeEx(fh, &size)){
        f->valid = 1;
        f->exists = 0;
        f->is_file = 0;
        f->size_cached = 0;
        f->data_cached = 0;
        result = EIO;
        goto finally;
    }
    f->valid = 1;
    f->exists = 1;
    f->is_file = 1;
    f->size_cached = 1;
    f->data_size = (size_t)size.QuadPart;
    void* data = Allocator_alloc(fc->allocator, f->data_size);
    if(!data){
        f->data_cached = 0;
        result = ENOMEM;
        goto finally;
    }
    size_t nread = 0;
    while(nread < f->data_size){
        DWORD to_read = f->data_size - nread;
        if(to_read > 0x80000000u) to_read = 0x80000000u;
        DWORD n;
        if(!ReadFile(fh, (char*)data + nread, to_read, &n, NULL)){
            Allocator_free(fc->allocator, data, f->data_size);
            result = EIO;
            goto finally;
        }
        if(n == 0){
            Allocator_free(fc->allocator, data, f->data_size);
            result = EIO;
            goto finally;
        }
        nread += n;
    }
    f->data_cached = 1;
    f->data.buff = data;
    f->data.n_bytes = nread;
    *outdata = (StringView){f->data.n_bytes, f->data.buff};
    result = 0;
    #else
    fd = open(f->path.text, O_RDONLY);
    if(fd < 0){
        f->valid = 1;
        f->unreadable = 1;
        f->is_file = 0;
        result = errno;
        goto finally;
    }
    struct stat s;
    int err = fstat(fd, &s);
    if(err){
        f->valid = 1;
        f->exists = 0;
        f->is_file = 0;
        f->size_cached = 0;
        f->data_cached = 0;
        result = errno;
        goto finally;
    }
    else {
        f->valid = 1;
        f->exists = 1;
        if(!S_ISREG(s.st_mode)){
            f->size_cached = 0;
            f->data_cached = 0;
            f->is_file = 0;
            result = ENOTBLK;
            goto finally;
        }
        f->is_file = 1;
        f->size_cached = 1;
        f->data_size = s.st_size;
    }
    void* data = Allocator_alloc(fc->allocator, f->data_size);
    if(!data){
        f->data_cached = 0;
        result = ENOMEM;
        goto finally;
    }
    size_t nread = 0;
    while(nread < f->data_size){
        char* p = (char*)data + nread;
        errno = 0;
        ssize_t e = read(fd, p, f->data_size - nread);
        if(e < 0){
            if(errno == EINTR)
                continue;
            result = errno;
            Allocator_free(fc->allocator, data, f->data_size);
            goto finally;
        }
        if(e == 0){
            result = EIO; // XXX: right code, but file size changed on us.
            Allocator_free(fc->allocator, data, f->data_size);
            goto finally;
        }
        nread += e;
    }
    f->data_cached = 1;
    f->data.buff = data;
    f->data.n_bytes = nread;
    *outdata = (StringView){f->data.n_bytes, f->data.buff};
    result = 0;
    #endif
    finally:
    #ifdef _WIN32
    if(fh != INVALID_HANDLE_VALUE) CloseHandle(fh);
    #else
    if(fd >= 0) close(fd);
    #endif
    msb_reset(&fc->path_builder);
    return result;
}

static
int
fc_get_size(FileCache* fc, size_t* sz){
    int result = ENOENT;
    CachedFile* f = fc_get_entry(fc);
    if(f && f->valid){
        if(f->size_cached){
            *sz = f->data_size;
            result = 0;
            goto finally;
        }
    }
    if(!fc->may_read_real_files) goto finally;
    if(!f) f = fc_create_entry(fc);
    #ifdef _WIN32
    {
        MStringBuilder16 wb = {.allocator = fc->allocator};
        msb16_write_utf8(&wb, f->path.text, f->path.length);
        if(wb.errored){ msb16_destroy(&wb); goto finally; }
        msb16_nul_terminate(&wb);
        HANDLE fh = CreateFileW((LPCWSTR)wb.data, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        msb16_destroy(&wb);
        if(fh == INVALID_HANDLE_VALUE){
            f->valid = 1;
            f->exists = 0;
            f->is_file = 0;
            result = ENOENT;
            goto finally;
        }
        LARGE_INTEGER size;
        if(!GetFileSizeEx(fh, &size)){
            CloseHandle(fh);
            f->valid = 1;
            f->exists = 1;
            f->is_file = 0;
            result = EIO;
            goto finally;
        }
        CloseHandle(fh);
        f->valid = 1;
        f->exists = 1;
        f->is_file = 1;
        f->size_cached = 1;
        f->data_size = (size_t)size.QuadPart;
        *sz = f->data_size;
        result = 0;
    }
    #else
    struct stat s;
    int err = stat(f->path.text, &s);
    if(err){
        f->valid = 1;
        f->exists = 0;
        f->is_file = 0;
        result = errno;
    }
    else {
        f->valid = 1;
        f->exists = 1;
        if(!S_ISREG(s.st_mode)){
            f->size_cached = 0;
            f->data_cached = 0;
            f->is_file = 0;
            result = ENOTBLK;
            goto finally;
        }
        f->is_file = 1;
        f->size_cached = 1;
        f->data_size = s.st_size;
        *sz = f->data_size;
        result = 0;
        goto finally;
    }
    #endif
    finally:
    msb_reset(&fc->path_builder);
    return result;
}

static
int
fc_cache_file(FileCache* fc, StringView data){
    int result = 0;
    CachedFile* f = fc_get_entry(fc);
    if(f && f->valid){
        result = 1;
        goto finally;
    }
    if(!f) f = fc_create_entry(fc);
    if(!f){
        result = 1;
        goto finally;
    }
    void* p = Allocator_dupe(fc->allocator, data.text, data.length);
    if(!p){
        f->valid = 0;
        result = 1;
        goto finally;
    }
    f->data.buff = p;
    f->data.n_bytes = data.length;
    f->data_size = data.length;
    f->exists = 1;
    f->valid = 1;
    f->is_file = 1;
    f->size_cached = 1;
    f->data_cached = 1;
    finally:
    msb_reset(&fc->path_builder);
    return result;
}


#ifdef __clang__
#pragma clang assume_nonnull end
#endif
#endif
