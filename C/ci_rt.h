//
// Copyright © 2026-2026, David Priver <david@davidpriver.com>
//
#ifndef C_CI_RT_H
#define C_CI_RT_H

#include "ci_interp.h"

#ifdef __clang__
#pragma clang assume_nonnull begin
#endif

typedef struct CiTlsSlot CiTlsSlot;

struct CiTlsThread {
    CiInterpreter* owner;
    Allocator allocator;
    CiTlsThread*_Nullable next;
    CiTlsThread*_Nullable prev;
    CiTlsSlot*_Nullable slots;
    size_t capacity;
    CiStack stack;
};

LOG_PRINTF(3, 4) static int ci_error(CiInterpreter*, SrcLoc, const char*, ...);
static int ci_tls_prepare(CiInterpreter*);
static int ci_tls_get(CiInterpreter*, CiTlsThread*_Nullable*_Nonnull);
static int ci_tls_address(CiInterpreter*, CcVariable*, void*_Nonnull*_Nonnull);
// Call before releasing the interpreter or its parser.
// However, this requires you to know there aren't any outstanding
// threads, so in practice you can't really call this unless you have
// control over all of the interpreted code.
static void ci_tls_cleanup(CiInterpreter*);

// Initialize before parsing or starting threads.
static int ci_init_runtime_calls(CiInterpreter*);

#ifdef __clang__
#pragma clang assume_nonnull end
#endif

#endif
