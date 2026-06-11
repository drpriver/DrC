#ifndef DRP_ATOMICS_H
#define DRP_ATOMICS_H
//
// Copyright © 2026-2026, David Priver <david@davidpriver.com>
//
// Compatibility wrapper for atomic pointer load/store.
//

#include <stddef.h>

#if defined(_MSC_VER) && !defined(__clang__)
#include <intrin.h>
#pragma intrinsic(_InterlockedCompareExchangePointer)
#pragma intrinsic(_InterlockedExchangePointer)
#define drp_atomic_ptr_load(p) _InterlockedCompareExchangePointer((void* volatile*)(p), NULL, NULL)
#define drp_atomic_ptr_store(p, v) ((void)_InterlockedExchangePointer((void* volatile*)(p), (void*)(v)))
#else
#define drp_atomic_ptr_load(p) __atomic_load_n((p), __ATOMIC_ACQUIRE)
#define drp_atomic_ptr_store(p, v) __atomic_store_n((p), (v), __ATOMIC_RELEASE)
#endif

#endif
