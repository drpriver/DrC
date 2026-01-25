//
// Copyright © 2021-2025, David Priver <david@davidpriver.com>
//
#ifndef FILE_UTIL_H
#define FILE_UTIL_H

// If this is defined, use libc's FILE* to do everything instead
// of native apis like read or ReadFile.

// #define USE_C_STDIO

#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <stdint.h>
#include "posixheader.h"
#include "windowsheader.h"
#if !defined(_WIN32) || defined(USE_C_STDIO)
#endif
#include "long_string.h"
#include "ByteBuffer.h"
#include "Allocators/allocator.h"

#ifdef __clang__
#pragma clang assume_nonnull begin
#endif

#ifndef force_inline
#if defined(__GNUC__) || defined(__clang__)
#define force_inline static inline __attribute__((always_inline))
#elif defined(_MSC_VER)
#define force_inline static inline __forceinline
#else
#define force_inline static inline
#endif
#endif

#ifndef warn_unused

#if defined(__GNUC__) || defined(__clang__)
#define warn_unused __attribute__((warn_unused_result))
#elif defined(_MSC_VER)
#define warn_unused
#else
#define warn_unused
#endif

#endif

#if defined(_WIN32) && defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-qual"
#endif


enum {
    // Catch-all file error, use os-specific means to retrieve
    // specific error.
    FILE_ERROR = 1,
    // Failure happened when trying to open the file. Use
    // os-specific means to retrieive specific error.
    FILE_NOT_OPENED = 2,
    // Allocator failed, operations up to that point succeeded.
    // No further information is available.
    FILE_RESULT_ALLOC_FAILURE = 3,
    // File is not a normal file (could be something like /dev/stdin).
    FILE_IS_NOT_A_FILE = 4,
};

// Re: the `native_error` member of the following:
//   On POSIX or with C_STDIO, this is errno.
//   On WINDOWS, this is the value from GetLastError()
//

typedef struct FileError FileError;
struct FileError {
    int errored;
    int native_error;
};

// Read an entire file into a string. Reads it in binary mode, so all
// bytes are preserved, but we do nul-terminate.
// Doesn't convert CRLF to newlines or anything like that. The caller
// should handle both the presence of a carriage return or its absence. Even
// on Posix platforms, you will encounter files with CRLF, or mixed!
// Most algorithms want to ignore trailing spaces anyway, so this isn't
// that big an imposition.
//
// For convenience, the result is nul-terminated.
static inline
warn_unused
FileError
read_file(const char* filepath, Allocator a, LongString* outstr);

// Read an entire file into a byte buffer. Not guranteed nul-terminated.
static inline
warn_unused
FileError
read_bin_file(const char* filepath, Allocator a, ByteBuffer* outbuff);

// Write an entire file. Agnostic as to text and binary, opens the file in binary
// mode. Writes whatever you give it as is, so we don't convert unix newlines to CRLF
// or anything like that.
static inline
warn_unused
FileError
write_file(const char* filename, const void* data, size_t data_length);

#ifdef USE_C_STDIO
force_inline
warn_unused
FileError
file_size_from_fp(FILE* fp, size_t* size){
    FileError result = {0};
    // sadly, the only way in standard c to do this.
    if(fseek(fp, 0, SEEK_END))
        goto errored;
    int64_t length = ftell(fp);
    if(length < 0)
        goto errored;
    if(fseek(fp, 0, SEEK_SET))
        goto errored;
    *size = (size_t)length;
    return result;

    errored:
    result.errored = FILE_ERROR;
    result.native_error = errno;
    return result;
}

static inline
warn_unused
FileError
read_file(const char* filepath, Allocator a, LongString* outstr){
    FileError result = {0};
    FILE* fp = fopen(filepath, "rb");
    if(!fp)
        return (FileError){.errored=FILE_NOT_OPENED, .native_error=errno};
    size_t nbytes;
    FileError size_e = file_size_from_fp(fp, &nbytes);
    if(size_e.errored){
        fclose(fp);
        return size_e;
    }
    char* text = Allocator_alloc(a, nbytes+1);
    if(!text){
        result.errored = FILE_RESULT_ALLOC_FAILURE;
        goto finally;
    }
    size_t fread_result = fread(text, 1, nbytes, fp);
    if(fread_result != nbytes){
        result.errored = FILE_ERROR;
        result.native_error = errno;
        Allocator_free(a, text, nbytes+1);
        goto finally;
    }
    text[nbytes] = '\0';
    *outstr = (LongString){nbytes, text};
finally:
    fclose(fp);
    return result;
}

static inline
warn_unused
FileError
read_bin_file(const char* filepath, Allocator a, ByteBuffer* outbuff){
    FileError result = {0};
    FILE* fp = fopen(filepath, "rb");
    if(!fp)
        return (FileError){.errored=FILE_NOT_OPENED, .native_error=errno};
    size_t nbytes;
    FileError size_e = file_size_from_fp(fp, &nbytes);
    if(size_e.errored){
        result = size_e;
        goto finally;
    }
    void* data = Allocator_alloc(a, nbytes);
    if(!data){
        result.errored = FILE_RESULT_ALLOC_FAILURE;
        goto finally;
    }
    assert(data);
    size_t fread_result = fread(data, 1, nbytes, fp);
    if(fread_result != nbytes){
        Allocator_free(a, data, nbytes);
        result.errored = FILE_ERROR;
        result.native_error = errno;
        goto finally;
    }
    assert(fread_result == nbytes);
    *outbuff = (ByteBuffer){nbytes, data};
finally:
    fclose(fp);
    return result;
}

static inline
warn_unused
FileError
write_file(const char* filename, const void* data, size_t data_length){
    FILE* fp = fopen(filename, "wb");
    if(!fp)
        return (FileError){.errored=FILE_NOT_OPENED, .native_error=errno};

    size_t nwrit = fwrite(data, 1, data_length, fp);
    if(nwrit != data_length){
        fclose(fp);
        return (FileError){.errored=FILE_ERROR, .native_error=errno};
    }
    fflush(fp);
    fclose(fp);
    return (FileError){0};
}

#elif defined(__linux__) || defined(__APPLE__)

force_inline
warn_unused
FileError
file_size_from_fd(int fd, size_t* length){
    FileError result = {0};
    struct stat s;
    int err = fstat(fd, &s);
    if(err == -1){
        result.errored = FILE_ERROR;
        result.native_error = errno;
        return result;
    }
    if(!S_ISREG(s.st_mode)){
        result.errored = FILE_IS_NOT_A_FILE;
        return result;
    }
    *length = s.st_size;
    return result;
}

static inline
warn_unused
FileError
read_file(const char* filepath, Allocator a, LongString* outstr){
    FileError result = {0};
    enum { flags = O_RDONLY
        // #ifdef __linux__
            // | O_NOATIME
        // #endif
    };
    int fd = open(filepath, flags);
    if(fd < 0){
        result.errored = FILE_NOT_OPENED;
        result.native_error = errno;
        return result;
    }
    size_t nbytes;
    FileError size_e = file_size_from_fd(fd, &nbytes);
    if(size_e.errored){
        result = size_e;
        goto finally;
    }
    char* text = Allocator_alloc(a, nbytes+1);
    if(!text){
        result.errored = FILE_RESULT_ALLOC_FAILURE;
        goto finally;
    }
    ssize_t read_result = read(fd, text, nbytes);
    if((size_t)read_result != nbytes){
        Allocator_free(a, text, nbytes+1);
        result.errored = FILE_ERROR;
        result.native_error = errno;
        goto finally;
    }
    assert((size_t)read_result == nbytes);
    text[nbytes] = '\0';
    *outstr = (LongString){nbytes, text};
finally:
    close(fd);
    return result;
}


static inline
warn_unused
FileError
read_bin_file(const char* filepath, Allocator a, ByteBuffer* outbuff){
    FileError result = {0};
    int fd = open(filepath, O_RDONLY);
    if(fd < 0){
        result.errored = FILE_NOT_OPENED;
        result.native_error = errno;
        return result;
    }
    size_t nbytes;
    FileError size_e = file_size_from_fd(fd, &nbytes);
    if(size_e.errored){
        result = size_e;
        goto finally;
    }
    void* data = Allocator_alloc(a, nbytes);
    if(!data){
        result.errored = FILE_RESULT_ALLOC_FAILURE;
        goto finally;
    }
    assert(data);
    ssize_t read_result = read(fd, data, nbytes);
    if(read_result != (ssize_t)nbytes){
        Allocator_free(a, data, nbytes);
        result.errored = FILE_ERROR;
        result.native_error = errno;
        goto finally;
    }
    assert(read_result == (ssize_t)nbytes);
    *outbuff = (ByteBuffer){nbytes, data};
finally:
    close(fd);
    return result;
}

static inline
warn_unused
FileError
write_file(const char* filename, const void* data, size_t data_length){
    int fd = open(
            filename,
            O_WRONLY | O_CREAT | O_TRUNC,
            S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    if(fd < 0)
        return (FileError){.errored=FILE_NOT_OPENED, .native_error=errno};
    ssize_t nwrit = write(fd, data, data_length);
    if((size_t)nwrit != data_length){
        int native = errno;
        close(fd);
        return (FileError){.errored=FILE_ERROR, .native_error=native};
    }
    close(fd);
    return (FileError){0};
}

#elif defined(_WIN32)
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wcast-qual"
#endif

// NOTE: In windows implementation we cast result of alloc
//       for C++ compat.

static inline
warn_unused
FileError
read_file(const char* filepath, Allocator a, LongString* outstr){
    FileError result = {0};
    HANDLE handle = CreateFileA(
            (char*)filepath,
            GENERIC_READ,
            FILE_SHARE_READ,
            NULL,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            NULL
            );
    if(handle == INVALID_HANDLE_VALUE){
        result.errored = FILE_NOT_OPENED;
        result.native_error = GetLastError();
        return result;
    }
    LARGE_INTEGER size;
    BOOL size_success = GetFileSizeEx(handle, &size);
    if(!size_success){
        result.errored = FILE_ERROR;
        result.native_error = GetLastError();
        goto finally;
    }
    size_t nbytes = size.QuadPart;
    char* text = (char*)Allocator_alloc(a, nbytes+1);
    if(!text){
        result.errored = FILE_RESULT_ALLOC_FAILURE;
        goto finally;
    }
    DWORD nread;
    BOOL read_success = ReadFile(handle, text, (DWORD)nbytes, &nread, NULL);
    if(!read_success){
        Allocator_free(a, text, nbytes+1);
        result.errored = FILE_ERROR;
        result.native_error = GetLastError();
        goto finally;
    }
    assert(nread == nbytes);
    text[nbytes] = '\0';
    *outstr = (LongString){nbytes, text};
finally:
    CloseHandle(handle);
    return result;
}

static inline
warn_unused
FileError
read_file_w(const wchar_t* filepath, Allocator a, LongString* outstr){
    FileError result = {0};
    // TODO: first param is declared without const.
    // Do I need to pass a local buffer or is it just
    // old-fashioned?
    HANDLE handle = CreateFileW(
            (wchar_t*)filepath,
            GENERIC_READ,
            FILE_SHARE_READ,
            NULL,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            NULL
            );
    if(handle == INVALID_HANDLE_VALUE){
        result.errored = FILE_NOT_OPENED;
        return result;
    }
    LARGE_INTEGER size;
    BOOL size_success = GetFileSizeEx(handle, &size);
    if(!size_success){
        result.errored = FILE_ERROR;
        result.native_error = GetLastError();
        goto finally;
    }
    size_t nbytes = size.QuadPart;
    char* text = (char*)Allocator_alloc(a, nbytes+1);
    if(!text){
        result.errored = FILE_RESULT_ALLOC_FAILURE;
        goto finally;
    }
    DWORD nread;
    BOOL read_success = ReadFile(handle, text, (DWORD)nbytes, &nread, NULL);
    if(!read_success){
        Allocator_free(a, text, nbytes+1);
        result.errored = FILE_ERROR;
        goto finally;
    }
    assert(nread == nbytes);
    text[nbytes] = '\0';
    *outstr = (LongString){nbytes, text};
finally:
    CloseHandle(handle);
    return result;
}

static inline
warn_unused
FileError
read_bin_file(const char* filepath, Allocator a, ByteBuffer* outbuff){
    FileError result = {0};
    HANDLE handle = CreateFileA(
            (char*)filepath,
            GENERIC_READ,
            FILE_SHARE_READ,
            NULL,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            NULL
            );
    if(handle == INVALID_HANDLE_VALUE){
        result.errored = FILE_NOT_OPENED;
        return result;
    }
    LARGE_INTEGER size;
    BOOL size_success = GetFileSizeEx(handle, &size);
    if(!size_success){
        result.errored = FILE_ERROR;
        result.native_error = GetLastError();
        goto finally;
    }
    size_t nbytes = size.QuadPart;
    void* data = Allocator_alloc(a, nbytes);
    if(!data){
        result.errored = FILE_RESULT_ALLOC_FAILURE;
        goto finally;
    }
    DWORD nread;
    BOOL read_success = ReadFile(handle, data, (DWORD)nbytes, &nread, NULL);
    if(!read_success){
        Allocator_free(a, data, nbytes);
        result.errored = FILE_ERROR;
        goto finally;
    }
    assert(nread == nbytes);
    *outbuff = (ByteBuffer){nbytes, data};
finally:
    CloseHandle(handle);
    return result;
}

static inline
warn_unused
FileError
read_bin_file_w(const wchar_t* filepath, Allocator a, ByteBuffer* outbuff){
    FileError result = {0};
    HANDLE handle = CreateFileW(
            (wchar_t*)filepath,
            GENERIC_READ,
            FILE_SHARE_READ,
            NULL,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            NULL
            );
    if(handle == INVALID_HANDLE_VALUE){
        result.errored = FILE_NOT_OPENED;
        return result;
    }
    LARGE_INTEGER size;
    BOOL size_success = GetFileSizeEx(handle, &size);
    if(!size_success){
        result.errored = FILE_ERROR;
        result.native_error = GetLastError();
        goto finally;
    }
    size_t nbytes = size.QuadPart;
    void* data = Allocator_alloc(a, nbytes);
    if(!data){
        result.errored = FILE_RESULT_ALLOC_FAILURE;
        goto finally;
    }
    DWORD nread;
    BOOL read_success = ReadFile(handle, data, (DWORD)nbytes, &nread, NULL);
    if(!read_success){
        Allocator_free(a, data, nbytes);
        result.errored = FILE_ERROR;
        goto finally;
    }
    assert(nread == nbytes);
    *outbuff = (ByteBuffer){nbytes, data};
finally:
    CloseHandle(handle);
    return result;
}

static inline
warn_unused
FileError
write_file(const char* filename, const void* data, size_t data_length){
    FileError result = {0};
    HANDLE handle = CreateFileA(
            (char*)filename,
            GENERIC_WRITE,
            0,
            NULL,
            CREATE_ALWAYS,
            FILE_ATTRIBUTE_NORMAL,
            NULL
            );
    if(handle == INVALID_HANDLE_VALUE){
        result.errored = FILE_NOT_OPENED;
        result.native_error = GetLastError();
        return result;
    }
    DWORD bytes_written;
    BOOL write_success = WriteFile(
            handle,
            data,
            (DWORD)data_length,
            &bytes_written,
            NULL);
    if(!write_success){
        result.errored = FILE_ERROR;
        result.native_error = GetLastError();
        goto finally;
    }
    assert(bytes_written == data_length);
finally:
    CloseHandle(handle);
    return result;
}

static inline
warn_unused
FileError
write_file_w(const wchar_t* filename, const void* data, size_t data_length){
    FileError result = {0};
    HANDLE handle = CreateFileW(
            (wchar_t*)filename,
            GENERIC_WRITE,
            0,
            NULL,
            CREATE_ALWAYS,
            FILE_ATTRIBUTE_NORMAL,
            NULL
            );
    if(handle == INVALID_HANDLE_VALUE){
        result.errored = FILE_NOT_OPENED;
        result.native_error = GetLastError();
        return result;
    }
    DWORD bytes_written;
    BOOL write_success = WriteFile(
            handle,
            data,
            (DWORD)data_length,
            &bytes_written,
            NULL);
    if(!write_success){
        result.errored = FILE_ERROR;
        result.native_error = GetLastError();
        goto finally;
    }
    assert(bytes_written == data_length);
finally:
    CloseHandle(handle);
    return result;
}

#ifdef __clang__
#pragma clang diagnostic pop
#endif

#elif defined(__wasm__)
static inline
warn_unused
FileError
read_file(const char* filepath, Allocator a, LongString* outstr){
    (void)a;
    (void)filepath;
    FileError result = {.errored=FILE_ERROR};
    return result;
}

static inline
warn_unused
FileError
read_bin_file(const char* filepath, Allocator a, ByteBuffer* outbuff){
    (void)a;
    (void)filepath;
    FileError result = {.errored=FILE_ERROR};
    return result;
}

static inline
warn_unused
FileError
write_file(const char* filename, const void* data, size_t data_length){
    (void)filename;
    (void)data;
    (void)data_length;
    return (FileError){.errored=FILE_ERROR};
}
#endif

#ifdef __clang__
#pragma clang assume_nonnull end
#endif

#if defined(_WIN32) && defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

#endif
