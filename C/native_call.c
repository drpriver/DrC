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
#include "cc_errors.h"
#include "native_call.h"
#include "cc_func.h"

enum {
    NC_NO_ERROR = _cc_no_error,
    NC_OOM_ERROR = _cc_oom_error,
    NC_UNSUPPORTED_TYPE = _cc_unimplemented_error,
};

#ifdef __clang__
#pragma clang assume_nonnull begin
#endif

static
int
cctype_to_ffi_type(CcQualType t, ffi_type*_Nonnull*_Nonnull out){
    switch(ccqt_kind(t)){
        case CC_POINTER:
        case CC_ARRAY:
        case CC_FUNCTION:
            *out = &ffi_type_pointer;
            return NC_NO_ERROR;
        case CC_ENUM: {
            CcEnum* e = ccqt_as_enum(t);
            return cctype_to_ffi_type(e->underlying, out);
        }
        case CC_STRUCT: {
            CcStruct* s = ccqt_as_struct(t);
            if(s->ffi_cache){ *out = s->ffi_cache; return NC_NO_ERROR; }
            uint32_t n = s->field_count;
            ffi_type* st = Allocator_zalloc(MALLOCATOR, sizeof(ffi_type) + sizeof(ffi_type*) * (n + 1));
            if(!st) return NC_OOM_ERROR;
            ffi_type** elements = (ffi_type**)(st + 1);
            *st = (ffi_type){.size = 0, .alignment = 0, .type = FFI_TYPE_STRUCT, .elements = elements};
            for(uint32_t i = 0; i < n; i++){
                int err = cctype_to_ffi_type(s->fields[i].type, &elements[i]);
                if(err){ Allocator_free(MALLOCATOR, st, sizeof(ffi_type) + sizeof(ffi_type*) * (n + 1)); return err; }
            }
            elements[n] = NULL;
            s->ffi_cache = st;
            *out = st;
            return NC_NO_ERROR;
        }
        case CC_UNION: {
            CcUnion* u = ccqt_as_union(t);
            if(u->ffi_cache){ *out = u->ffi_cache; return NC_NO_ERROR; }
            uint32_t n = u->field_count;
            ffi_type* st = Allocator_zalloc(MALLOCATOR, sizeof(ffi_type) + sizeof(ffi_type*) * (n + 1));
            if(!st) return NC_OOM_ERROR;
            ffi_type** elements = (ffi_type**)(st + 1);
            *st = (ffi_type){.size = 0, .alignment = 0, .type = FFI_TYPE_STRUCT, .elements = elements};
            for(uint32_t i = 0; i < n; i++){
                int err = cctype_to_ffi_type(u->fields[i].type, &elements[i]);
                if(err){ Allocator_free(MALLOCATOR, st, sizeof(ffi_type) + sizeof(ffi_type*) * (n + 1)); return err; }
            }
            elements[n] = NULL;
            u->ffi_cache = st;
            *out = st;
            return NC_NO_ERROR;
        }
        case CC_VECTOR:
            return NC_UNSUPPORTED_TYPE;
        case CC_BASIC:
            switch(t.basic.kind){
                case CCBT_void:               *out = &ffi_type_void; return NC_NO_ERROR;
                case CCBT_bool:               *out = &ffi_type_uint8; return NC_NO_ERROR;
                case CCBT_char:               *out = &ffi_type_sint8; return NC_NO_ERROR;
                case CCBT_signed_char:        *out = &ffi_type_sint8; return NC_NO_ERROR;
                case CCBT_unsigned_char:      *out = &ffi_type_uint8; return NC_NO_ERROR;
                case CCBT_short:              *out = &ffi_type_sint16; return NC_NO_ERROR;
                case CCBT_unsigned_short:     *out = &ffi_type_uint16; return NC_NO_ERROR;
                case CCBT_int:                *out = &ffi_type_sint32; return NC_NO_ERROR;
                case CCBT_unsigned:           *out = &ffi_type_uint32; return NC_NO_ERROR;
                case CCBT_long:               *out = &ffi_type_sint64; return NC_NO_ERROR;
                case CCBT_unsigned_long:      *out = &ffi_type_uint64; return NC_NO_ERROR;
                case CCBT_long_long:          *out = &ffi_type_sint64; return NC_NO_ERROR;
                case CCBT_unsigned_long_long: *out = &ffi_type_uint64; return NC_NO_ERROR;
                case CCBT_float:              *out = &ffi_type_float; return NC_NO_ERROR;
                case CCBT_double:             *out = &ffi_type_double; return NC_NO_ERROR;
                case CCBT_long_double:        *out = &ffi_type_longdouble; return NC_NO_ERROR;
                case CCBT_float_complex:      *out = &ffi_type_complex_float; return NC_NO_ERROR;
                case CCBT_double_complex:     *out = &ffi_type_complex_double; return NC_NO_ERROR;
                case CCBT_long_double_complex:*out = &ffi_type_complex_longdouble; return NC_NO_ERROR;
                case CCBT_nullptr_t:          *out = &ffi_type_pointer; return NC_NO_ERROR;
                case CCBT_int128:
                case CCBT_unsigned_int128:
                case CCBT_float16:
                    return NC_UNSUPPORTED_TYPE;
                case CCBT_INVALID:
                case CCBT_COUNT:
                    return NC_UNSUPPORTED_TYPE;
            }
            break;
    }
    return NC_UNSUPPORTED_TYPE;
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
int
native_call_cache_create(Allocator a, CcFunction* func_type, NativeCallCache*_Nullable*_Nonnull out){
    *out = NULL;
    uint32_t n = func_type->param_count;
    NativeCallCache* c = Allocator_zalloc(a, sizeof *c + n * sizeof *c->arg_types);
    if(!c) return NC_OOM_ERROR;
    c->nparams = n;
    ffi_type* rtype;
    int err = cctype_to_ffi_type(func_type->return_type, &rtype);
    if(err) goto fail;
    for(uint32_t i = 0; i < n; i++){
        err = cctype_to_ffi_type(func_type->params[i], &c->arg_types[i]);
        if(err) goto fail;
    }
    ffi_status s = ffi_prep_cif(&c->cif, FFI_DEFAULT_ABI, n, rtype, c->arg_types);
    if(s != FFI_OK){ err = NC_UNSUPPORTED_TYPE; goto fail; }
    *out = c;
    return NC_NO_ERROR;
    fail:
    Allocator_free(a, c, sizeof *c + n * sizeof *c->arg_types);
    return err;
}

static
int
native_call(Allocator a, CcFunc* func, void*_Nonnull*_Nonnull args, int nargs, const CcQualType*_Nullable vararg_types, void* rvalue){
    CcFunction* func_type = func->type;
    int err;
    // Variadic calls can't use the cache (nargs varies).
    if(func_type->is_variadic){
        if(nargs > 64) return -1;
        ffi_cif cif;
        ffi_type* arg_types[64];
        ffi_type* rtype;
        err = cctype_to_ffi_type(func_type->return_type, &rtype);
        if(err) return err;
        for(int i = 0; i < nargs && (uint32_t)i < func_type->param_count; i++){
            err = cctype_to_ffi_type(func_type->params[i], &arg_types[i]);
            if(err) return err;
        }
        for(int i = (int)func_type->param_count; i < nargs; i++){
            if(vararg_types){
                err = cctype_to_ffi_type(vararg_types[i - func_type->param_count], &arg_types[i]);
                if(err) return err;
            }
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
        err = native_call_cache_create(a, func_type, &c);
        if(err) return err;
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
int
native_closure_create(Allocator a, CcFunction* func_type, NativeClosureCallback* cb, void*_Nullable userdata, NativeClosure*_Nullable*_Nonnull out){
    *out = NULL;
    uint32_t nparams = func_type->param_count;
    NativeClosure* nc = Allocator_zalloc(a, sizeof *nc + nparams * sizeof *nc->arg_types);
    if(!nc) return NC_OOM_ERROR;
    nc->nparams = nparams;
    nc->cb = cb;
    nc->userdata = userdata;
    ffi_type* rtype;
    int err = cctype_to_ffi_type(func_type->return_type, &rtype);
    if(err) goto fail;
    for(uint32_t i = 0; i < nparams; i++){
        err = cctype_to_ffi_type(func_type->params[i], &nc->arg_types[i]);
        if(err) goto fail;
    }
    ffi_status s = ffi_prep_cif(&nc->cif, FFI_DEFAULT_ABI, nparams, rtype, nc->arg_types);
    if(s != FFI_OK){ err = NC_UNSUPPORTED_TYPE; goto fail; }
    nc->closure = ffi_closure_alloc(sizeof(ffi_closure), (void**)&nc->fn);
    if(!nc->closure){ err = NC_OOM_ERROR; goto fail; }
    s = ffi_prep_closure_loc(nc->closure, &nc->cif, closure_trampoline, nc, (void*)nc->fn);
    if(s != FFI_OK){
        ffi_closure_free(nc->closure);
        err = NC_UNSUPPORTED_TYPE;
        goto fail;
    }
    *out = nc;
    return NC_NO_ERROR;
    fail:
    Allocator_free(a, nc, sizeof *nc + nparams * sizeof nc->arg_types[0]);
    return err;
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
