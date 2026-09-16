//
// Copyright © 2021-2026, David Priver <david@davidpriver.com>
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
#include <limits.h>
#include "posixheader.h"
#include "windowsheader.h"
#if defined USE_C_STDIO
#include <errno.h>
#endif
#include "cstring_view.h"
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

#if defined(USE_C_STDIO)
typedef FILE* FileUtilHandle;
#define FU_STDIN stdin
#define FU_STDERR stderr
#define FU_STDOUT stdout
#else
typedef OsFileHandle FileUtilHandle;
#ifdef _WIN32
#define FU_STDIN GetStdHandle(STD_INPUT_HANDLE)
#define FU_STDERR GetStdHandle(STD_ERROR_HANDLE)
#define FU_STDOUT GetStdHandle(STD_OUTPUT_HANDLE)
#else
#define FU_STDIN STDIN_FILENO
#define FU_STDERR STDERR_FILENO
#define FU_STDOUT STDOUT_FILENO
#endif
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
read_file(const char* filepath, Allocator a, CStringView* outstr);

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

// Reads a currently open handle in appropriate mode until exhausted.
// Works on streams.
// nul-terminates.
static inline
warn_unused
FileError
read_file_handle(FileUtilHandle fd, Allocator a, CStringView* outstr);

// Like read_file_handle, but doesn't nul-terminate.
static inline
warn_unused
FileError
read_bin_file_handle(FileUtilHandle fd, Allocator a, ByteBuffer* outbuff);

// Like write_file, but on an already open handle, which should
// be in the appropriate mode.
// You would think you can just call the native `write()` equivalent,
// but you have to do dumb retry loops and win32 limits the size etc.
static inline
warn_unused
FileError
write_file_handle(FileUtilHandle fd, const void* data, size_t data_length);

#if defined(USE_C_STDIO) || defined(__linux__) || defined(__APPLE__) || defined(_WIN32)
static inline
warn_unused
FileError
read_bin_file_handle(FileUtilHandle fd, Allocator a, ByteBuffer* outbuff){
    enum {CHUNK_SIZE = 65536};
    size_t capacity = CHUNK_SIZE;
    size_t length = 0;
    char* buffer = Allocator_alloc(a, capacity);
    if(!buffer)
        return (FileError){.errored=FILE_RESULT_ALLOC_FAILURE};
    FileError result = {0};
    for(;;){
        if(length == capacity){
            if(capacity > SIZE_MAX / 2){
                result.errored = FILE_RESULT_ALLOC_FAILURE;
                goto errored;
            }
            size_t new_capacity = capacity * 2;
            char* new_buffer = Allocator_realloc(a, buffer, capacity, new_capacity);
            if(!new_buffer){
                result.errored = FILE_RESULT_ALLOC_FAILURE;
                goto errored;
            }
            buffer = new_buffer;
            capacity = new_capacity;
        }
        size_t to_read = capacity - length;
        if(to_read > CHUNK_SIZE) to_read = CHUNK_SIZE;
        #ifdef USE_C_STDIO
            size_t nread = fread(buffer + length, 1, to_read, fd);
            if(ferror(fd)){
                result = (FileError){.errored=FILE_ERROR, .native_error=errno};
                goto errored;
            }
            length += nread;
            if(feof(fd)) break;
        #elif defined _WIN32
            DWORD nread;
            if(!ReadFile(fd, buffer + length, (DWORD)to_read, &nread, NULL)){
                DWORD native = GetLastError();
                if(native == ERROR_BROKEN_PIPE) break;
                result = (FileError){.errored=FILE_ERROR, .native_error=native};
                goto errored;
            }
            if(!nread) break;
            length += nread;
        #else
            ssize_t nread = read(fd, buffer + length, to_read);
            if(nread < 0){
                int native = errno;
                if(native == EINTR) continue;
                result = (FileError){.errored=FILE_ERROR, .native_error=native};
                goto errored;
            }
            if(!nread) break;
            length += (size_t)nread;
        #endif
    }
    if(!length){
        Allocator_free(a, buffer, capacity);
        *outbuff = (ByteBuffer){0};
        return result;
    }
    if(length != capacity){
        char* new_buffer = Allocator_realloc(a, buffer, capacity, length);
        if(!new_buffer){
            result.errored = FILE_RESULT_ALLOC_FAILURE;
            goto errored;
        }
        buffer = new_buffer;
    }
    *outbuff = (ByteBuffer){length, buffer};
    return result;
errored:
    Allocator_free(a, buffer, capacity);
    return result;
}
#endif

#ifdef USE_C_STDIO
force_inline
warn_unused
FileError
file_size_from_handle(FILE* fp, size_t* size){
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
read_file(const char* filepath, Allocator a, CStringView* outstr){
    FileError result = {0};
    FILE* fp = fopen(filepath, "rb");
    if(!fp)
        return (FileError){.errored=FILE_NOT_OPENED, .native_error=errno};
    size_t nbytes;
    FileError size_e = file_size_from_handle(fp, &nbytes);
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
    *outstr = (CStringView){nbytes, text};
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
    FileError size_e = file_size_from_handle(fp, &nbytes);
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
read_file_handle(FILE* fp, Allocator a, CStringView* outstr){
    FileError result = {0};
    enum {CHUNK_SIZE = 65536};
    size_t capacity = CHUNK_SIZE;
    size_t length = 0;
    char* buffer = Allocator_alloc(a, capacity);
    if(!buffer)
        return (FileError){.errored=FILE_RESULT_ALLOC_FAILURE};
    for(;;){
        if(length == capacity){
            if(capacity > SIZE_MAX / 2){
                error.errored = FILE_RESULT_ALLOC_FAILURE;
                goto fail;
            }
            size_t new_capacity = capacity * 2;
            char* new_buffer = Allocator_realloc(a, buffer, capacity, new_capacity);
            if(!new_buffer){
                error.errored = FILE_RESULT_ALLOC_FAILURE;
                goto fail;
            }
            buffer = new_buffer;
            capacity = new_capacity;
        }
        size_t nread = fread(buffer + length, 1, capacity - length, fp);
        length += nread;
        if(ferror(fp)){
            result = (FileError){.errored=FILE_ERROR, .native_error=native};
            goto fail;
        }
        if(feof(fp)) break;
    }
    char* new_buffer = Allocator_realloc(a, buffer, capacity, length+1);
    if(!new_buffer){
        result = (FileError){.errored=FILE_RESULT_ALLOC_FAILURE};
        goto fail;
    }
    new_buffer[length] = '\0';
    *outstr = (CStringView){length, new_buffer};
    return result;
    fail:
    Allocator_free(a, buffer, capacity);
    return result;
}

static inline
warn_unused
FileError
write_file(const char* filename, const void* data, size_t data_length){
    FILE* fp = fopen(filename, "wb");
    if(!fp)
        return (FileError){.errored=FILE_NOT_OPENED, .native_error=errno};
    FileError result = write_file_handle(fp, data, data_length);
    if(fclose(fp) && !result.errored)
        result = (FileError){.errored=FILE_ERROR, .native_error=errno};
    return result;
}

static inline
warn_unused
FileError
write_file_handle(FILE* fp, const void* data, size_t data_length){
    size_t nwrit = fwrite(data, 1, data_length, fp);
    if(nwrit != data_length)
        return (FileError){.errored=FILE_ERROR, .native_error=errno};
    if(fflush(fp))
        return (FileError){.errored=FILE_ERROR, .native_error=errno};
    return (FileError){0};
}

#elif defined __linux__ || defined __APPLE__

force_inline
warn_unused
FileError
file_size_from_handle(int fd, size_t* length){
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
read_file(const char* filepath, Allocator a, CStringView* outstr){
    FileError result = {0};
    // O_NONBLOCK prevents blocking on a FIFO without a writer
    // We'll reject in file_size_from_handle() anyway.
    int fd = open(filepath, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
    if(fd < 0){
        result.errored = FILE_NOT_OPENED;
        result.native_error = errno;
        return result;
    }
    size_t nbytes;
    FileError size_e = file_size_from_handle(fd, &nbytes);
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
    *outstr = (CStringView){nbytes, text};
finally:
    close(fd);
    return result;
}


static inline
warn_unused
FileError
read_bin_file(const char* filepath, Allocator a, ByteBuffer* outbuff){
    FileError result = {0};
    // Avoid waiting for a FIFO writer before rejecting non-regular files.
    int fd = open(filepath, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
    if(fd < 0){
        result.errored = FILE_NOT_OPENED;
        result.native_error = errno;
        return result;
    }
    size_t nbytes;
    FileError size_e = file_size_from_handle(fd, &nbytes);
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
read_file_handle(FileUtilHandle fd, Allocator a, CStringView* outstr){
    FileError result = {0};
    size_t nbytes;
    FileError size_e = file_size_from_handle(fd, &nbytes);
    if(!size_e.errored){
        // Regular file with known size
        char* text = Allocator_alloc(a, nbytes+1);
        if(!text){
            result.errored = FILE_RESULT_ALLOC_FAILURE;
            return result;
        }
        size_t total_read = 0;
        while(total_read < nbytes){
            ssize_t nread = read(fd, text + total_read, nbytes - total_read);
            if(nread < 0){
                if(errno == EINTR) continue; // Retry on interrupt
                Allocator_free(a, text, nbytes+1);
                result.errored = FILE_ERROR;
                result.native_error = errno;
                return result;
            }
            if(nread == 0) break; // EOF
            total_read += nread;
        }
        if(total_read != nbytes){
            char* new_text = Allocator_realloc(a, text, nbytes+1, total_read+1);
            if(!new_text){
                Allocator_free(a, text, nbytes+1);
                return (FileError){.errored=FILE_RESULT_ALLOC_FAILURE};
            }
            text = new_text;
        }
        text[total_read] = '\0';
        *outstr = (CStringView){total_read, text};
        return result;
    }
    enum {CHUNK_SIZE = 65536};
    size_t capacity = CHUNK_SIZE;
    size_t length = 0;
    char* buffer = Allocator_alloc(a, capacity);
    if(!buffer){
        result.errored = FILE_RESULT_ALLOC_FAILURE;
        return result;
    }
    for(;;){
        if(length + CHUNK_SIZE > capacity){
            size_t new_capacity = capacity * 2;
            char* new_buffer = Allocator_realloc(a, buffer, capacity, new_capacity);
            if(!new_buffer){
                Allocator_free(a, buffer, capacity);
                result.errored = FILE_RESULT_ALLOC_FAILURE;
                return result;
            }
            buffer = new_buffer;
            capacity = new_capacity;
        }
        ssize_t nread = read(fd, buffer + length, CHUNK_SIZE);
        if(nread < 0){
            if(errno == EINTR) continue; // Retry on interrupt
            Allocator_free(a, buffer, capacity);
            result.errored = FILE_ERROR;
            result.native_error = errno;
            return result;
        }
        if(nread == 0) break; // EOF
        length += nread;
    }
    // shrink to fit
    char* new_buffer = Allocator_realloc(a, buffer, capacity, length+1);
    if(!new_buffer){
        Allocator_free(a, buffer, capacity);
        result.errored = FILE_RESULT_ALLOC_FAILURE;
        return result;
    }
    new_buffer[length] = 0;
    *outstr = (CStringView){length, new_buffer};
    return result;
}

static inline
warn_unused
FileError
write_file(const char* filename, const void* data, size_t data_length){
    int fd = open(
            filename,
            O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC,
            S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    if(fd < 0)
        return (FileError){.errored=FILE_NOT_OPENED, .native_error=errno};
    FileError result = write_file_handle(fd, data, data_length);
    close(fd);
    return result;
}

static inline
warn_unused
FileError
write_file_handle(int fd, const void* data, size_t data_length){
    size_t total_written = 0;
    while(total_written < data_length){
        size_t to_write = data_length - total_written;
        if(to_write > (size_t)SSIZE_MAX) to_write = (size_t)SSIZE_MAX;
        ssize_t nwrit = write(fd, (const char*)data + total_written, to_write);
        if(nwrit < 0){
            int native = errno;
            if(native == EINTR) continue;
            return (FileError){.errored=FILE_ERROR, .native_error=native};
        }
        if(!nwrit)
            return (FileError){.errored=FILE_ERROR, .native_error=EIO};
        total_written += (size_t)nwrit;
    }
    return (FileError){0};
}

#elif defined(_WIN32)
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wcast-qual"
#endif

static inline
warn_unused
FileError
file_util_read_exact(HANDLE handle, void* data, size_t length){
    size_t total_read = 0;
    while(total_read < length){
        size_t to_read = length - total_read;
        if(to_read > (DWORD)-1) to_read = (DWORD)-1;
        DWORD nread;
        if(!ReadFile(handle, (char*)data + total_read, (DWORD)to_read, &nread, NULL))
            return (FileError){.errored=FILE_ERROR, .native_error=GetLastError()};
        if(!nread)
            return (FileError){.errored=FILE_ERROR, .native_error=ERROR_HANDLE_EOF};
        total_read += nread;
    }
    return (FileError){0};
}

static inline
warn_unused
FileError
read_file(const char* filepath, Allocator a, CStringView* outstr){
    FileError result = {0};
    HANDLE handle = CreateFileA(
            filepath,
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
    char* text = Allocator_alloc(a, nbytes+1);
    if(!text){
        result.errored = FILE_RESULT_ALLOC_FAILURE;
        goto finally;
    }
    result = file_util_read_exact(handle, text, nbytes);
    if(result.errored){
        Allocator_free(a, text, nbytes+1);
        goto finally;
    }
    text[nbytes] = '\0';
    *outstr = (CStringView){nbytes, text};
finally:
    CloseHandle(handle);
    return result;
}

static inline
warn_unused
FileError
read_file_w(const wchar_t* filepath, Allocator a, CStringView* outstr){
    FileError result = {0};
    HANDLE handle = CreateFileW(
            filepath,
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
    char* text = Allocator_alloc(a, nbytes+1);
    if(!text){
        result.errored = FILE_RESULT_ALLOC_FAILURE;
        goto finally;
    }
    result = file_util_read_exact(handle, text, nbytes);
    if(result.errored){
        Allocator_free(a, text, nbytes+1);
        goto finally;
    }
    text[nbytes] = '\0';
    *outstr = (CStringView){nbytes, text};
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
            filepath,
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
    result = file_util_read_exact(handle, data, nbytes);
    if(result.errored){
        Allocator_free(a, data, nbytes);
        goto finally;
    }
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
            filepath,
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
    result = file_util_read_exact(handle, data, nbytes);
    if(result.errored){
        Allocator_free(a, data, nbytes);
        goto finally;
    }
    *outbuff = (ByteBuffer){nbytes, data};
finally:
    CloseHandle(handle);
    return result;
}

static inline
warn_unused
FileError
read_file_handle(FileUtilHandle handle, Allocator a, CStringView* outstr){
    FileError result = {0};
    LARGE_INTEGER size;
    BOOL size_success = GetFileType(handle) == FILE_TYPE_DISK && GetFileSizeEx(handle, &size);
    if(size_success){
        // Regular file with known size
        size_t nbytes = size.QuadPart;
        char* text = Allocator_alloc(a, nbytes+1);
        if(!text){
            result.errored = FILE_RESULT_ALLOC_FAILURE;
            return result;
        }
        size_t total_read = 0;
        while(total_read < nbytes){
            size_t to_read = nbytes - total_read;
            if(to_read > (DWORD)-1) to_read = (DWORD)-1; // Clamp to DWORD max
            DWORD nread;
            BOOL read_success = ReadFile(handle, text + total_read, (DWORD)to_read, &nread, NULL);
            if(!read_success){
                Allocator_free(a, text, nbytes+1);
                result.errored = FILE_ERROR;
                result.native_error = GetLastError();
                return result;
            }
            if(nread == 0) break; // EOF
            total_read += nread;
        }
        if(total_read != nbytes){
            char* new_text = Allocator_realloc(a, text, nbytes+1, total_read+1);
            if(!new_text){
                Allocator_free(a, text, nbytes+1);
                return (FileError){.errored=FILE_RESULT_ALLOC_FAILURE};
            }
            text = new_text;
        }
        text[total_read] = '\0';
        *outstr = (CStringView){total_read, text};
        return result;
    }
    enum {CHUNK_SIZE = 65536}; // 64KB chunks
    size_t capacity = CHUNK_SIZE;
    size_t length = 0;
    char* buffer = Allocator_alloc(a, capacity);
    if(!buffer){
        result.errored = FILE_RESULT_ALLOC_FAILURE;
        return result;
    }
    for(;;){
        if(length + CHUNK_SIZE > capacity){
            size_t new_capacity = capacity * 2;
            char* new_buffer = Allocator_realloc(a, buffer, capacity, new_capacity);
            if(!new_buffer){
                Allocator_free(a, buffer, capacity);
                result.errored = FILE_RESULT_ALLOC_FAILURE;
                return result;
            }
            buffer = new_buffer;
            capacity = new_capacity;
        }
        DWORD nread;
        BOOL read_success = ReadFile(handle, buffer + length, CHUNK_SIZE, &nread, NULL);
        if(!read_success){
            DWORD e = GetLastError();
            // end-of-stream for a pipe is ERROR_BROKEN_PIPE,
            // which is not actually an error.
            if(e == ERROR_BROKEN_PIPE) break;
            Allocator_free(a, buffer, capacity);
            result.errored = FILE_ERROR;
            result.native_error = e;
            return result;
        }
        if(nread == 0) break; // EOF
        length += nread;
    }
    // shrink to fit
    char* new_buffer = Allocator_realloc(a, buffer, capacity, length+1);
    if(!new_buffer){
        Allocator_free(a, buffer, capacity);
        result.errored = FILE_RESULT_ALLOC_FAILURE;
        return result;
    }
    new_buffer[length] = 0;
    *outstr = (CStringView){length, new_buffer};
    return result;
}

static inline
warn_unused
FileError
write_file(const char* filename, const void* data, size_t data_length){
    FileError result = {0};
    HANDLE handle = CreateFileA(
            filename,
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
    result = write_file_handle(handle, data, data_length);
    CloseHandle(handle);
    return result;
}

static inline
warn_unused
FileError
write_file_w(const wchar_t* filename, const void* data, size_t data_length){
    FileError result = {0};
    HANDLE handle = CreateFileW(
            filename,
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
    result = write_file_handle(handle, data, data_length);
    CloseHandle(handle);
    return result;
}

static inline
warn_unused
FileError
write_file_handle(HANDLE handle, const void* data, size_t data_length){
    size_t total_written = 0;
    while(total_written < data_length){
        size_t to_write = data_length - total_written;
        if(to_write > (DWORD)-1) to_write = (DWORD)-1;
        DWORD bytes_written;
        if(!WriteFile(handle, (const char*)data + total_written, (DWORD)to_write, &bytes_written, NULL))
            return (FileError){.errored=FILE_ERROR, .native_error=GetLastError()};
        if(!bytes_written)
            return (FileError){.errored=FILE_ERROR, .native_error=ERROR_WRITE_FAULT};
        total_written += bytes_written;
    }
    return (FileError){0};
}

#ifdef __clang__
#pragma clang diagnostic pop
#endif

#elif defined(__wasm__)
static inline
warn_unused
FileError
read_file(const char* filepath, Allocator a, CStringView* outstr){
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
