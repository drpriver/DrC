//
// Copyright © 2022-2025, David Priver <david@davidpriver.com>
//
#ifndef DEBUGGING_H
#define DEBUGGING_H

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-function"
#elif defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#endif

#ifdef __clang__
#pragma clang assume_nonnull begin
#else
#ifndef _Nonnull
#define _Nonnull
#endif
#endif

#ifndef dbg_noinline
#if defined(__GNUC__) || defined(__clang__)
#if !defined(__clang__)
#define dbg_noinline __attribute__((__noinline__))
#else
#define dbg_noinline __attribute__((__noinline__))
#endif
#else
#define dbg_noinline
#endif
#endif

typedef struct BacktraceArray BacktraceArray;
#ifdef __wasm__
extern
void
bt(void);

extern
BacktraceArray*
get_bt(void);

extern
void
dump_bt(BacktraceArray* a);

extern
void
free_bt(BacktraceArray* a);

#else

typedef struct BacktraceArray BacktraceArray;
struct BacktraceArray {
    int count;
    void*_Nonnull symbols[];
};

static
dbg_noinline
BacktraceArray*
get_bt(void);

static
dbg_noinline
void
dump_bt(BacktraceArray* a);

static
dbg_noinline
void
bt(void);

static inline
void
free_bt(BacktraceArray* a){
    if(a) free(a);
}

#if defined(__APPLE__) || defined(__linux__)
#ifdef __clang__
#pragma clang assume_nonnull end
#endif

#include <execinfo.h>
#include <stdlib.h>

#ifdef __clang__
#pragma clang assume_nonnull begin
#endif

static
dbg_noinline
BacktraceArray*
get_bt(void){
    enum {bufflen=256};
    void* array[bufflen];
    int n = backtrace(array, bufflen);
    BacktraceArray* result = malloc(sizeof(*result)+n*sizeof(void*));
    result->count = n;
    __builtin_memcpy(result->symbols, array, n*sizeof(void*));
    return result;
}

static
dbg_noinline
void
dump_bt(BacktraceArray*_Nonnull a){
    backtrace_symbols_fd(a->symbols+4, a->count-5, 2);
}

static
dbg_noinline
void
bt(void){
    enum {bufflen=256};
    void* array[bufflen];
    int n = backtrace(array, bufflen);
    backtrace_symbols_fd(array, n-2, 2);
}
#elif defined(_WIN32)

#ifdef __clang__
#pragma clang assume_nonnull end
#endif

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#define WIN32_EXTRA_LEAN
#include <Windows.h>
#endif
#include <dbghelp.h>
#include <stdlib.h>
#include <stdio.h>
#pragma comment(lib, "dbghelp.lib")

#ifdef __clang__
#pragma clang assume_nonnull begin
#endif

static int Dbg_sym_is_init = 0;
static HANDLE Dbg_process;

static
dbg_noinline
void
dbg_ensure_init(void){
    if(Dbg_sym_is_init) return;
    Dbg_sym_is_init = 1;
    Dbg_process = GetCurrentProcess();
    SymInitialize(Dbg_process, NULL, TRUE);
}

static
dbg_noinline
void
bt(void){
    // Apparently this stuff is only safe to call from a single thread at a time.
    // But putting a critical section in here seemed like it'd be annoying.
    // Idk.
    enum {bufflen=256};
    void* array[bufflen];
    dbg_ensure_init();

    unsigned frames = CaptureStackBackTrace(0, bufflen, array, NULL);
    struct {
        SYMBOL_INFO symbol;
        char buff[256];
    } sym = {0};
    sym.symbol.MaxNameLen   = 255;
    sym.symbol.SizeOfStruct = sizeof(SYMBOL_INFO);

    IMAGEHLP_LINE64 line = {
        .SizeOfStruct = sizeof(IMAGEHLP_LINE64),
    };
    DWORD disp;

    for(unsigned i = 0; i < frames; i++ ){
        BOOL addrsuccess = SymFromAddr(Dbg_process, (DWORD64)array[i], 0, &sym.symbol);
        if(!addrsuccess){
            fprintf(stderr, "%2d  %-24s  0x%p\n",
                frames-i-1, "???", array[i]);
            continue;
        }
        BOOL linesuccess = SymGetLineFromAddr64(Dbg_process, (DWORD64)array[i], &disp, &line);
        if(linesuccess){
            fprintf(stderr, "%2d  %-24s  at %s:%-4lu\n",
                frames-i-1, sym.symbol.Name,
                line.FileName, line.LineNumber);
        }
        else {
            fprintf(stderr, "%2d  %-24s  0x%p\n",
                frames-i-1, sym.symbol.Name,
                (void*)sym.symbol.Address);
        }
    }
}
static
dbg_noinline
BacktraceArray*
get_bt(void){
    enum {bufflen=256};
    void* array[bufflen];
    unsigned frames = CaptureStackBackTrace(0, bufflen, array, NULL);
    BacktraceArray* result = malloc(sizeof(*result)+sizeof(void*)*frames);
    result->count = frames;
    memcpy(result->symbols, array, sizeof(void*)*frames);
    return result;
}

static
dbg_noinline
void
dump_bt(BacktraceArray* bta){
    dbg_ensure_init();
    int count = bta->count;
    void** array = bta->symbols;

    struct {
        SYMBOL_INFO symbol;
        char buff[256];
    } sym = {0};
    sym.symbol.MaxNameLen   = 255;
    sym.symbol.SizeOfStruct = sizeof(SYMBOL_INFO);

    IMAGEHLP_LINE64 line = {
        .SizeOfStruct = sizeof(IMAGEHLP_LINE64),
    };
    DWORD disp;

    for(int i = 0; i < count; i++ ){
        BOOL addrsuccess = SymFromAddr(Dbg_process, (DWORD64)bta->symbols[i], 0, &sym.symbol);
        if(!addrsuccess){
            fprintf(stderr, "%2d  %-24s  0x%p\n",
                count-i-1, "???", array[i]);
            continue;
        }
        BOOL linesuccess = SymGetLineFromAddr64(Dbg_process, (DWORD64)array[i], &disp, &line);
        if(linesuccess){
            fprintf(stderr, "%2d  %-24s  at %s:%-4lu\n",
                count-i-1, sym.symbol.Name,
                line.FileName, line.LineNumber);
        }
        else {
            fprintf(stderr, "%2d  %-24s  0x%p\n",
                count-i-1, sym.symbol.Name,
                (void*)sym.symbol.Address);
        }
    }
}


#else
static
dbg_noinline
void
bt(void){
    // do nothing
}
static
dbg_noinline
BacktraceArray*
get_bt(void){
    static BacktraceArray bta;
    return &bta;
}

static
dbg_noinline
void
dump_bt(BacktraceArray* bta){
    (void)bta;
}

#endif

#endif

#ifdef __clang__
#pragma clang assume_nonnull end
#endif

#ifdef __clang__
#pragma clang diagnostic pop
#elif defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

#endif
