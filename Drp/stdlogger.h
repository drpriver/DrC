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
    ssize_t err = write(fd, data, len);
    (void)err;
}
#endif

static
Logger*
std_logger(void){
#ifdef _WIN32
    HANDLE handles[] = {GetStdHandle(STD_OUTPUT_HANDLE), GetStdHandle(STD_ERROR_HANDLE)};
    Logger* l = Allocator_zalloc_extra(MALLOCATOR, 1, sizeof *l, sizeof handles, _Alignof(Logger));
    if(!l) return l;
    ((HANDLE*)(l+1))[0] = handles[0];
    ((HANDLE*)(l+1))[1] = handles[1];
    l->buff.allocator = MALLOCATOR;
    l->sink = log_win32;
#else
    int handles[] = {STDOUT_FILENO, STDERR_FILENO};
    Logger* l = Allocator_zalloc_extra(MALLOCATOR, 1, sizeof *l, sizeof handles, _Alignof(Logger));
    if(!l) return l;
    ((int*)(l+1))[0] = handles[0];
    ((int*)(l+1))[1] = handles[1];
    l->buff.allocator = MALLOCATOR;
    l->sink = log_posix;
#endif
    return l;
}

#ifdef __clang__
#pragma clang assume_nonnull end
#endif
#endif
