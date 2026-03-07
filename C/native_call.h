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

typedef struct NativeCallCache NativeCallCache;

static
int
native_call_cache_create(Allocator al, CcFunction* func_type,
    uint32_t nvarargs, const CcQualType*_Nullable vararg_types,
    NativeCallCache*_Nullable*_Nonnull out);
// ---------------------------------
// Create a CIF cache for a function call signature.
//
// Arguments:
// ----------
// al:
//    Allocator for the cache.
//
// func_type:
//    The base function type.
//
// nvarargs:
//    Number of variadic arguments (0 for non-variadic calls).
//
// vararg_types:
//    Types of the variadic arguments (nvarargs elements).
//    NULL for non-variadic calls.
//
// out:
//    On success, receives the created cache.
//
// Returns:
// --------
// 0 on success or an error code on failure.

static
void
native_call_cache_destroy(Allocator al, NativeCallCache*);
// ---------------------------------
// Destroy a CIF cache and free its resources.
//
// Arguments:
// ----------
// al:
//    The allocator that was used to create the cache.

static
void
native_call(NativeCallCache*, void (*fn)(void),
    void*_Nonnull*_Nonnull args, void* rvalue);
// ---------------------------------
// Call a native function through a pre-built CIF. Read-only, thread-safe.
//
// Arguments:
// ----------
// cache:
//    The pre-built CIF cache for this call signature.
//
// fn:
//    The native function pointer to call.
//
// args:
//    Array of pointers to argument values.
//
// rvalue:
//    Storage for the return value.

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
