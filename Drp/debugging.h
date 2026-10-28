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
struct BacktraceArray {
    int count;
    void*_Nonnull symbols[];
};

static
dbg_noinline
BacktraceArray* _Null_unspecified
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

#if defined __DRC__ && !defined __DRC_JIT__
#include <stdlib.h>
#include <stdio.h>
static
dbg_noinline
BacktraceArray* _Null_unspecified
get_bt(void){
    // Opaque entries, youngest first. Keep the interpreter's source metadata
    // alive until the captured trace has been symbolicated.
    void *buffer[256];
    int n = __builtin_capture_backtrace(buffer, _Countof buffer);
    BacktraceArray* a = malloc(sizeof *a + sizeof a->symbols[0]*n);
    if(a){
        a->count = n;
        __builtin_memcpy(a->symbols, buffer, sizeof buffer[0]*n);
    }
    return a;
}

static
dbg_noinline
void
dump_bt(BacktraceArray* a){
    if(!a) return;
    for(int i = 0; i < a->count; i++){
        _SrcLoc loc;
        __builtin_symbolicate(a->symbols[i], &loc);
        fprintf(stderr, "%2d) %.*s:%-4lu\n", a->count-i-1, (int)loc.file.count, loc.file.data, loc.line);
    }
}

static
dbg_noinline
void
bt(void){
    __bt();
}


#elif defined __APPLE__ || defined __linux__
#ifdef __clang__
#pragma clang assume_nonnull end
#endif

#include <execinfo.h>
#include <stdlib.h>

#if defined __linux__
#include <pthread.h>
#include <stdint.h>
#if !defined _GNU_SOURCE
extern int pthread_getattr_np(pthread_t, pthread_attr_t* _Nonnull);
#endif
#endif

#ifdef __clang__
#pragma clang assume_nonnull begin
#endif

#if defined __linux__ && (defined __x86_64__ || defined __aarch64__) && !defined __DRC__
static dbg_noinline
int
dbg_capture_stack(void* _Nonnull* _Nonnull array, int capacity){
    static _Thread_local uintptr_t stack_low, stack_high;
    if(!stack_high){
        pthread_attr_t attr;
        void* base;
        size_t size;
        if(pthread_getattr_np(pthread_self(), &attr)) return 0;
        int err = pthread_attr_getstack(&attr, &base, &size);
        pthread_attr_destroy(&attr);
        if(err || size < 2*sizeof(uintptr_t)) return 0;
        stack_low = (uintptr_t)base;
        stack_high = stack_low + size;
        if(stack_high < stack_low){
            stack_high = 0;
            return 0;
        }
    }
    uintptr_t fp = (uintptr_t)__builtin_frame_address(0);
    int n = 0;
    while(n < capacity){
        if(fp < stack_low || fp > stack_high - 2*sizeof(uintptr_t) || fp % sizeof(uintptr_t))
            break;
        const uintptr_t* frame = (const uintptr_t*)fp;
        uintptr_t caller = frame[0],
                  pc = frame[1];
        if(!pc) break;
        array[n++] = (void*)pc;
        if(caller <= fp) break;
        fp = caller;
    }
    return n;
}
#else
#define dbg_capture_stack backtrace
#endif

static
dbg_noinline
BacktraceArray* _Null_unspecified
get_bt(void){
    enum {bufflen=256};
    void* array[bufflen];
    int n = dbg_capture_stack(array, bufflen);
    BacktraceArray* result = malloc(sizeof(*result)+n*sizeof(void*));
    if(result){
        result->count = n;
        __builtin_memcpy(result->symbols, array, n*sizeof(void*));
    }
    return result;
}

static
dbg_noinline
void
dump_bt(BacktraceArray*_Nonnull a){
    if(!a) return;
    if(a->count > 4)
        backtrace_symbols_fd(a->symbols+4, a->count-4, 2);
}

static
dbg_noinline
void
bt(void){
    enum {bufflen=256};
    void* array[bufflen];
    int n = dbg_capture_stack(array, bufflen);
    backtrace_symbols_fd(array, n, 2);
}
#elif defined _WIN32

#ifdef __clang__
#pragma clang assume_nonnull end
#endif

#include "windowsheader.h"
#include "thread_utils.h"
#include <dbghelp.h>
#include <stdlib.h>
#include <stdio.h>
#if !defined __GNUC__ || defined __clang__ || defined __DRC__
#pragma comment(lib, "dbghelp.lib")
#endif

#ifdef __clang__
#pragma clang assume_nonnull begin
#endif

static LOCK_T Dbg_lock = LOCK_T_INIT;
static int Dbg_sym_is_init = 0;
static HANDLE Dbg_process;

static
_Bool
dbg_can_capture_stack(void){
    static LONG capture_state;
    LONG state = InterlockedCompareExchange(&capture_state, 0, 0);
    if(!state){
        HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
        // Wine's stack capture can fault while unwinding its ELF-based DLLs.
        state = ntdll && GetProcAddress(ntdll, "wine_get_version") ? -1 : 1;
        InterlockedExchange(&capture_state, state);
    }
    return state > 0;
}

static
dbg_noinline
void
dbg_ensure_init(void){
    LOCK_T_lock(&Dbg_lock);
    if(!Dbg_sym_is_init){
        Dbg_sym_is_init = 1;
        Dbg_process = GetCurrentProcess();
        SymInitialize(Dbg_process, NULL, TRUE);
    }
    LOCK_T_unlock(&Dbg_lock);
}

static
dbg_noinline
void
bt(void){
    if(!dbg_can_capture_stack()) return;
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
    IMAGEHLP_LINE64 line = { .SizeOfStruct = sizeof(IMAGEHLP_LINE64), };
    DWORD disp;
    LOCK_T_lock(&Dbg_lock);
    for(unsigned i = 0; i < frames; i++ ){
        BOOL ok;
        ok = SymFromAddr(Dbg_process, (DWORD64)array[i], 0, &sym.symbol);
        if(!ok){
            fprintf(stderr, "%2d  %-24s  0x%p\n", frames-i-1, "???", array[i]);
            continue;
        }
        ok = SymGetLineFromAddr64(Dbg_process, (DWORD64)array[i], &disp, &line);
        if(ok)
            fprintf(stderr, "%2d  %-24s  at %s:%-4lu\n", frames-i-1, sym.symbol.Name, line.FileName, line.LineNumber);
        else
            fprintf(stderr, "%2d  %-24s  0x%p\n", frames-i-1, sym.symbol.Name, (void*)sym.symbol.Address);
    }
    LOCK_T_unlock(&Dbg_lock);
}
static
dbg_noinline
BacktraceArray* _Null_unspecified
get_bt(void){
    enum {bufflen=256};
    void* array[bufflen];
    unsigned frames = 0;
    if(dbg_can_capture_stack())
        frames = CaptureStackBackTrace(0, bufflen, array, NULL);
    BacktraceArray* result = malloc(sizeof(*result)+sizeof(void*)*frames);
    if(result){
        result->count = frames;
        memcpy(result->symbols, array, sizeof(void*)*frames);
    }
    return result;
}

static
dbg_noinline
void
dump_bt(BacktraceArray* bta){
    if(!bta) return;
    if(!bta->count) return;
    dbg_ensure_init();
    int count = bta->count;
    void** array = bta->symbols;
    struct {
        SYMBOL_INFO symbol;
        char buff[256];
    } sym = {0};
    sym.symbol.MaxNameLen   = 255;
    sym.symbol.SizeOfStruct = sizeof(SYMBOL_INFO);
    IMAGEHLP_LINE64 line = { .SizeOfStruct = sizeof(IMAGEHLP_LINE64) };
    DWORD disp;
    LOCK_T_lock(&Dbg_lock);
    for(int i = 0; i < count; i++ ){
        BOOL ok;
        ok = SymFromAddr(Dbg_process, (DWORD64)bta->symbols[i], 0, &sym.symbol);
        if(!ok){
            fprintf(stderr, "%2d  %-24s  0x%p\n", count-i-1, "???", array[i]);
            continue;
        }
        ok = SymGetLineFromAddr64(Dbg_process, (DWORD64)array[i], &disp, &line);
        if(ok)
            fprintf(stderr, "%2d  %-24s  at %s:%-4lu\n", count-i-1, sym.symbol.Name, line.FileName, line.LineNumber);
        else
            fprintf(stderr, "%2d  %-24s  0x%p\n", count-i-1, sym.symbol.Name, (void*)sym.symbol.Address);
    }
    LOCK_T_unlock(&Dbg_lock);
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
