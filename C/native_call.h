#ifndef C_NATIVE_CALL_H
#define C_NATIVE_CALL_H
//
// Copyright © 2026-2026, David Priver <david@davidpriver.com>
//
#include "../Drp/Allocators/allocator.h"
#include "cc_type.h"

#ifdef __clang__
#pragma clang assume_nonnull begin
#endif

typedef struct CcFunc CcFunc;

static
int
native_call(Allocator al, CcFunc* func, void*_Nonnull*_Nonnull args,
    int nargs, const CcQualType*_Nullable vararg_types, void* rvalue);
// ---------------------------------
// Call a native function through a CcFunc.
//
// Arguments:
// ----------
// al:
//    Allocator used for creating/caching the ffi call interface.
//
// func:
//    The CcFunc to call. Must have native_func set to a valid function pointer.
//
// args:
//    Array of pointers to argument values (nargs elements).
//
// nargs:
//    Number of actual arguments (>= param_count for variadic).
//
// vararg_types:
//    Types of the variadic arguments (nargs - param_count elements).
//    NULL for non-variadic calls.
//
// rvalue:
//    Storage for the return value.
//
// Returns:
// --------
// 0 on success or an error code on failure.

static
void
native_call_cache_free(Allocator al, CcFunc* func);
// ---------------------------------
// Free the cached ffi call interface on a CcFunc.
//
// Arguments:
// ----------
// al:
//    The allocator that was used to create the cache.
//
// func:
//    The CcFunc whose cache should be freed.

typedef void (NativeClosureCallback)(void* rvalue, void*_Nonnull*_Nonnull args, void* userdata);
// ---------------------------------
// Callback type for closures. Called when native code invokes the closure.
//
// Arguments:
// ----------
// rvalue:
//    Write the return value here.
//
// args:
//    Array of pointers to the argument values (param_count elements).
//
// userdata:
//    The userdata passed to native_closure_create.

typedef struct NativeClosure NativeClosure;
// ------------
// Opaque handle to a native closure. A closure wraps a callback so it can be
// called through a C function pointer with a specific signature. Created with
// native_closure_create, freed with native_closure_destroy.

static
int
native_closure_create(Allocator al, CcFunction* func_type,
    NativeClosureCallback* cb, void*_Nullable userdata,
    NativeClosure*_Nullable*_Nonnull out);
// ---------------------------------
// Create a closure: a native function pointer that, when called, invokes cb.
//
// Arguments:
// ----------
// al:
//    Allocator for the closure's internal data.
//
// func_type:
//    The function signature the closure's function pointer should have.
//
// cb:
//    The callback to invoke when the closure is called.
//
// userdata:
//    Opaque pointer passed through to cb on every call.
//
// out:
//    On success, receives the created closure. Must be freed with
//    native_closure_destroy.
//
// Returns:
// --------
// 0 on success or an error code on failure.

static void (*native_closure_fn(NativeClosure* closure))(void);
// ---------------------------------
// Get the callable function pointer from a closure.
//
// Arguments:
// ----------
// closure:
//    The closure to get the function pointer from.
//
// Returns:
// --------
// A function pointer that can be called by native code.

static
void
native_closure_destroy(Allocator al, NativeClosure* closure);
// ---------------------------------
// Destroy a closure and free its resources.
//
// Arguments:
// ----------
// al:
//    The allocator that was used to create the closure.
//
// closure:
//    The closure to destroy.

#ifdef __clang__
#pragma clang assume_nonnull end
#endif
#endif
