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

struct CcFunc;

// Call a native function through a CcFunc.
// Uses func->native_func, func->type for the signature, and
// func->native_call_cache to avoid re-preparing the cif each call.
// args:           array of pointers to argument values (nargs elements).
// nargs:          number of actual arguments (>= param_count for variadic).
// vararg_types:   types of the variadic arguments (nargs - param_count elements).
//                 NULL for non-variadic calls.
// rvalue:         storage for the return value.
// Returns 0 on success.
static
int
native_call(Allocator, struct CcFunc* func,
            void*_Nonnull*_Nonnull args, int nargs,
            const CcQualType*_Nullable vararg_types,
            void* rvalue);

// Free the cached cif on a CcFunc (call when destroying a CcFunc).
static
void
native_call_cache_free(Allocator, struct CcFunc* func);

// Callback type for closures. Called when native code invokes the closure.
// rvalue:   write the return value here.
// args:     array of pointers to the argument values (param_count elements).
// userdata: the userdata passed to native_closure_create.
typedef void (NativeClosureCallback)(void* rvalue, void*_Nonnull*_Nonnull args, void* userdata);

// Opaque handle to a closure. Do not inspect.
typedef struct NativeClosure NativeClosure;

// Create a closure: a native function pointer that, when called, invokes cb.
// func_type: the function signature the pointer should have.
// cb:        your callback (e.g. "interpret this CcFunc").
// userdata:  passed through to cb on every call.
// Returns NULL on failure.
// The returned NativeClosure must be freed with native_closure_destroy.
static
NativeClosure*_Nullable
native_closure_create(Allocator, CcFunction* func_type, NativeClosureCallback* cb, void*_Nullable userdata);

// Get the callable function pointer from a closure.
static void (*native_closure_fn(NativeClosure* closure))(void);

// Destroy a closure and free its resources.
static void native_closure_destroy(Allocator, NativeClosure* closure);

#ifdef __clang__
#pragma clang assume_nonnull end
#endif
#endif
