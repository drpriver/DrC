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
#include "logger.h"
#ifdef __clang__
#pragma clang assume_nonnull begin
#endif


#ifdef _WIN32
static
void
log_win32(Logger* logger, LogLevel lvl, const void* data, size_t len){
    HANDLE handle = ((HANDLE*)(logger+1))[lvl==LOG_PRINT?0:1];
    DWORD bytes_written;
    WriteFile(handle, data, (DWORD)len, &bytes_written, NULL);
}
#else
static
void
log_posix(Logger* logger, LogLevel lvl, const void* data, size_t len){
    int fd = ((int*)(logger+1))[lvl==LOG_PRINT?0:1];
    int err = write(fd, data, len);
    (void)err;
}
#endif

static
Logger*
std_logger(void){
#ifdef _WIN32
    Logger* l = Allocator_zalloc(MALLOCATOR, sizeof *l + 2*sizeof(HANDLE));
    if(!l) return l;
    ((HANDLE*)(l+1))[0] = GetStdHandle(STD_OUTPUT_HANDLE);
    ((HANDLE*)(l+1))[1] = GetStdHandle(STD_ERROR_HANDLE);
    l->buff.allocator = MALLOCATOR;
    l->sink = log_win32;
#else
    Logger* l = Allocator_zalloc(MALLOCATOR, sizeof *l + 2*sizeof(int));
    if(!l) return l;
    ((int*)(l+1))[0] = STDOUT_FILENO;
    ((int*)(l+1))[1] = STDERR_FILENO;
    l->buff.allocator = MALLOCATOR;
    l->sink = log_posix;
#endif
    return l;
}

#ifdef __clang__
#pragma clang assume_nonnull end
#endif
#endif
