//
// Copyright © 2026-2026, David Priver <david@davidpriver.com>
//
#ifdef __has_include
#if __has_include(<ffi/ffi.h>)
#include <ffi/ffi.h>
#elif __has_include(<ffi.h>)
#include <ffi.h>
#endif
#else
#include <ffi.h>
#endif
#include "native_call.h"
#include "cc_func.h"

#ifdef __clang__
#pragma clang assume_nonnull begin
#endif

static
ffi_type* // FIXME: return error, use outparam
cctype_to_ffi_type(CcQualType t){
    switch(ccqt_kind(t)){
        case CC_POINTER:
        case CC_ARRAY:
        case CC_FUNCTION:
            return &ffi_type_pointer;
        case CC_ENUM:
            return &ffi_type_sint32;
        case CC_STRUCT:
        case CC_UNION: {
            // CcStruct and CcUnion have the same layout up through ffi_cache.
            CcStruct* s = (CcStruct*)(t.bits & ~(uintptr_t)7); // FIXME: wtf?
            if(s->ffi_cache) return s->ffi_cache;
            uint32_t n = s->field_count;
            ffi_type* st = Allocator_alloc(MALLOCATOR, sizeof(ffi_type) + sizeof(ffi_type*) * (n + 1));
            if(!st) return &ffi_type_void; // FIXME: return error
            ffi_type** elements = (ffi_type**)(st + 1);
            st->size = 0;
            st->alignment = 0;
            st->type = FFI_TYPE_STRUCT;
            st->elements = elements;
            for(uint32_t i = 0; i < n; i++){
                elements[i] = cctype_to_ffi_type(s->fields[i].type);
            }
            elements[n] = NULL;
            s->ffi_cache = st;
            return st;
        }
        case CC_VECTOR:
            return &ffi_type_pointer; // TODO: proper vector support
        case CC_BASIC:
            switch(t.basic.kind){
                case CCBT_void:               return &ffi_type_void;
                case CCBT_bool:               return &ffi_type_uint8;
                case CCBT_char:               return &ffi_type_sint8;
                case CCBT_signed_char:        return &ffi_type_sint8;
                case CCBT_unsigned_char:      return &ffi_type_uint8;
                case CCBT_short:              return &ffi_type_sint16;
                case CCBT_unsigned_short:     return &ffi_type_uint16;
                case CCBT_int:                return &ffi_type_sint32;
                case CCBT_unsigned:           return &ffi_type_uint32;
                case CCBT_long:               return &ffi_type_sint64;
                case CCBT_unsigned_long:      return &ffi_type_uint64;
                case CCBT_long_long:          return &ffi_type_sint64;
                case CCBT_unsigned_long_long: return &ffi_type_uint64;
                case CCBT_int128:             return &ffi_type_void; // FIXME: return error
                case CCBT_unsigned_int128:    return &ffi_type_void; // FIXME: return error
                case CCBT_float16:            return &ffi_type_void; // FIXME: return error
                case CCBT_float:              return &ffi_type_float;
                case CCBT_double:             return &ffi_type_double;
                case CCBT_long_double:        return &ffi_type_longdouble;
                case CCBT_float_complex:      return &ffi_type_complex_float;
                case CCBT_double_complex:     return &ffi_type_complex_double;
                case CCBT_long_double_complex:return &ffi_type_complex_longdouble;
                case CCBT_nullptr_t:          return &ffi_type_pointer;
                case CCBT_INVALID:            return &ffi_type_void;
                case CCBT_COUNT:              return &ffi_type_void;
            }
            break;
    }
    return &ffi_type_void; // FIXME: return error
}

// Cached cif for non-variadic calls.
// Stored as func->native_call_cache.
typedef struct NativeCallCache NativeCallCache;
struct NativeCallCache {
    ffi_cif cif;
    uint32_t nparams;
    ffi_type*_Nonnull arg_types[];
};

static
NativeCallCache*_Nullable
native_call_cache_create(Allocator a, CcFunction* func_type){
    uint32_t n = func_type->param_count;
    NativeCallCache* c = Allocator_zalloc(a, sizeof *c + n * sizeof *c->arg_types);
    if(!c) return NULL;
    c->nparams = n;
    ffi_type* rtype = cctype_to_ffi_type(func_type->return_type);
    for(uint32_t i = 0; i < n; i++){
        c->arg_types[i] = cctype_to_ffi_type(func_type->params[i]);
    }
    ffi_status s = ffi_prep_cif(&c->cif, FFI_DEFAULT_ABI, n, rtype, c->arg_types);
    if(s != FFI_OK){
        Allocator_free(a, c, sizeof *c + n * sizeof *c->arg_types);
        return NULL;
    }
    return c;
}

static
int
native_call(Allocator a, CcFunc* func, void*_Nonnull*_Nonnull args, int nargs, const CcQualType*_Nullable vararg_types, void* rvalue){
    CcFunction* func_type = func->type;
    // Variadic calls can't use the cache (nargs varies).
    if(func_type->is_variadic){
        if(nargs > 64) return -1;
        ffi_cif cif;
        ffi_type* arg_types[64];
        ffi_type* rtype = cctype_to_ffi_type(func_type->return_type);
        for(int i = 0; i < nargs && (uint32_t)i < func_type->param_count; i++){
            arg_types[i] = cctype_to_ffi_type(func_type->params[i]);
        }
        for(int i = (int)func_type->param_count; i < nargs; i++){
            if(vararg_types)
                arg_types[i] = cctype_to_ffi_type(vararg_types[i - func_type->param_count]);
            else
                arg_types[i] = &ffi_type_pointer;
        }
        ffi_status s = ffi_prep_cif_var(&cif, FFI_DEFAULT_ABI, (unsigned)func_type->param_count, (unsigned)nargs, rtype, arg_types);
        if(s != FFI_OK) return (int)s;
        ffi_call(&cif, func->native_func, rvalue, args);
        return 0;
    }
    // Non-variadic: use or create cache.
    NativeCallCache* c = func->native_call_cache;
    if(!c){
        c = native_call_cache_create(a, func_type);
        if(!c) return -1;
        func->native_call_cache = c;
    }
    ffi_call(&c->cif, func->native_func, rvalue, args);
    return 0;
}

static
void
native_call_cache_free(Allocator a, CcFunc* func){
    NativeCallCache* c = func->native_call_cache;
    if(!c) return;
    Allocator_free(a, c, sizeof *c + c->nparams * sizeof *c->arg_types);
    func->native_call_cache = NULL;
}

struct NativeClosure {
    ffi_cif cif;
    uint32_t nparams;
    ffi_closure* closure;
    void (*fn)(void);
    NativeClosureCallback* cb;
    void*_Null_unspecified userdata;
    ffi_type*_Nonnull arg_types[];
};

static
void
closure_trampoline(ffi_cif* cif, void* rvalue, void*_Nonnull*_Nonnull args, void* userdata){
    (void)cif;
    NativeClosure* nc = userdata;
    nc->cb(rvalue, args, nc->userdata);
}

static
NativeClosure*_Nullable
native_closure_create(Allocator a, CcFunction* func_type, NativeClosureCallback* cb, void*_Nullable userdata){
    uint32_t nparams = func_type->param_count;
    NativeClosure* nc = Allocator_zalloc(a, sizeof *nc + nparams * sizeof *nc->arg_types);
    if(!nc) return NULL;
    nc->nparams = nparams;
    nc->cb = cb;
    nc->userdata = userdata;
    ffi_type* rtype = cctype_to_ffi_type(func_type->return_type);
    for(uint32_t i = 0; i < nparams; i++){
        nc->arg_types[i] = cctype_to_ffi_type(func_type->params[i]);
    }
    ffi_status s = ffi_prep_cif(&nc->cif, FFI_DEFAULT_ABI, nparams, rtype, nc->arg_types);
    if(s != FFI_OK)
        goto fail;
    nc->closure = ffi_closure_alloc(sizeof(ffi_closure), (void**)&nc->fn);
    if(!nc->closure)
        goto fail;
    s = ffi_prep_closure_loc(nc->closure, &nc->cif, closure_trampoline, nc, (void*)nc->fn);
    if(s != FFI_OK){
        ffi_closure_free(nc->closure);
        goto fail;
    }
    return nc;
    fail:
    Allocator_free(a, nc, sizeof *nc + nparams * sizeof nc->arg_types[0]);
    return NULL;
}

static
void (*native_closure_fn(NativeClosure* closure))(void){
    return closure->fn;
}

static
void
native_closure_destroy(Allocator a, NativeClosure* closure){
    ffi_closure_free(closure->closure);
    Allocator_free(a, closure, sizeof *closure + closure->nparams * sizeof *closure->arg_types);
}

#ifdef __clang__
#pragma clang assume_nonnull end
#endif
