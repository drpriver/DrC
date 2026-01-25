#ifndef DRP_STDLOGGER_H
#define DRP_STDLOGGER_H
//
// Copyright © 2026-2026, David Priver <david@davidpriver.com>
//
#include <stdint.h>
#ifdef _WIN32
#include "windowsheader.h"
#else
#include "posixheader.h"
#endif
#include "logger.h"
#include "Allocators/mallocator.h"
#ifdef __clang__
#pragma clang assume_nonnull begin
#endif


#ifdef _WIN32
static
void
log_win32(void* handle, const void* data, size_t len){
    DWORD bytes_written;
    WriteFile(handle, data, (DWORD)len, &bytes_written, NULL);
}
#else
static
void
log_posix(void* handle, const void* data, size_t len){
    int fd = (int)(intptr_t)handle;
    write(fd, data, len);
}
#endif

static
Logger std_logger(void){
    Logger l = {
        .buff.allocator = MALLOCATOR,
        #ifdef _WIN32
        .up = GetStdHandle(STD_OUTPUT_HANDLE),
        .sink = log_win32,
        #else
        .up = (void*)(intptr_t)STDOUT_FILENO,
        .sink = log_posix,
        #endif
    };
    return l;
}

#ifdef __clang__
#pragma clang assume_nonnull end
#endif
#endif
