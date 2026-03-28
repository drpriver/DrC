#ifndef C_NATIVE_CALL_C
#define C_NATIVE_CALL_C
//
// Copyright © 2026-2026, David Priver <david@davidpriver.com>
//

#ifndef NO_NATIVE_CALL
#ifdef __has_include
#if __has_include(<ffi/ffi.h>)
#include <ffi/ffi.h>
#elif __has_include(<ffi.h>)
#include <ffi.h>
#else
#pragma message("<ffi.h> not found")
#define NO_NATIVE_CALL
#endif
#else
#include <ffi.h>
#endif
#endif

#ifdef _WIN32
#define NATIVE_CALL_NO_COMPLEX_TYPE
#endif

#include "cc_errors.h"
#include "native_call.h"
#include "cc_func.h"
#include "cc_target.h"

enum {
    NC_NO_ERROR = _cc_no_error,
    NC_OOM_ERROR = _cc_oom_error,
    NC_UNSUPPORTED_TYPE = _cc_unimplemented_error,
    NC_UNIMPLEMENTED_ERROR = _cc_unimplemented_error,
};

#ifdef __clang__
#pragma clang assume_nonnull begin
#endif

#ifndef NO_NATIVE_CALL

static
int
cctype_integer_ffi(Allocator a, uint32_t sz, uint32_t al, ffi_type*_Nonnull*_Nonnull out){
    ffi_type* rep_elem;
    uint32_t rep_count;
    if(al >= 8 && sz % 8 == 0){       rep_elem = &ffi_type_uint64; rep_count = sz / 8; }
    else if(al >= 4 && sz % 4 == 0){  rep_elem = &ffi_type_uint32; rep_count = sz / 4; }
    else if(al >= 2 && sz % 2 == 0){  rep_elem = &ffi_type_uint16; rep_count = sz / 2; }
    else {                            rep_elem = &ffi_type_uint8;  rep_count = sz;     }
    if(!rep_count) rep_count = 1;
    ffi_type* st = Allocator_zalloc(a, sizeof(ffi_type) + sizeof(ffi_type*) * (rep_count + 1));
    if(!st) return NC_OOM_ERROR;
    ffi_type** elements = (ffi_type**)(st + 1);
    *st = (ffi_type){.size = 0, .alignment = 0, .type = FFI_TYPE_STRUCT, .elements = elements};
    for(uint32_t i = 0; i < rep_count; i++) elements[i] = rep_elem;
    elements[rep_count] = NULL;
    *out = st;
    return NC_NO_ERROR;
}

static
int
cctype_to_ffi_type(Allocator a, CcQualType t, ffi_type*_Nonnull*_Nonnull out){
    switch(ccqt_kind(t)){
        case CC_POINTER:
        case CC_FUNCTION:
            *out = &ffi_type_pointer;
            return NC_NO_ERROR;
        case CC_ARRAY:{
            CcArray* arr = ccqt_as_array(t);
            if(arr->is_vector) return NC_UNSUPPORTED_TYPE;
            uint32_t n = (uint32_t)arr->length;
            ffi_type* elem;
            int err = cctype_to_ffi_type(a, arr->element, &elem);
            if(err) return err;
            ffi_type* st = Allocator_zalloc(a, sizeof(ffi_type) + sizeof(ffi_type*) * (n + 1));
            if(!st) return NC_OOM_ERROR;
            ffi_type** elements = (ffi_type**)(st + 1);
            *st = (ffi_type){.size = 0, .alignment = 0, .type = FFI_TYPE_STRUCT, .elements = elements};
            for(uint32_t i = 0; i < n; i++)
                elements[i] = elem;
            elements[n] = NULL;
            *out = st;
            return NC_NO_ERROR;
        }
        case CC_ENUM:{
            CcEnum* e = ccqt_as_enum(t);
            return cctype_to_ffi_type(a, e->underlying, out);
        }
        case CC_STRUCT:{
            CcStruct* s = ccqt_as_struct(t);
            if(s->ffi_cache){ *out = s->ffi_cache; return NC_NO_ERROR; }
            switch((CcTarget)CC_TARGET_NATIVE){
            case CC_TARGET_AARCH64_LINUX:
            case CC_TARGET_AARCH64_MACOS:
                if(s->arm64.hfa_count){
                    ffi_type* base;
                    switch((CcBasicTypeKind)s->arm64.hfa_type){
                        case CCBT_float:       base = &ffi_type_float; break;
                        case CCBT_double:      base = &ffi_type_double; break;
                        case CCBT_long_double: base = &ffi_type_longdouble; break;
                        case CCBT_COUNT:
                        case CCBT_INVALID:
                        case CCBT__Type:
                        case CCBT_bool:
                        case CCBT_char:
                        case CCBT_double_complex:
                        case CCBT_float128:
                        case CCBT_float16:
                        case CCBT_float_complex:
                        case CCBT_int:
                        case CCBT_int128:
                        case CCBT_long:
                        case CCBT_long_double_complex:
                        case CCBT_long_long:
                        case CCBT_nullptr_t:
                        case CCBT_short:
                        case CCBT_signed_char:
                        case CCBT_unsigned:
                        case CCBT_unsigned_char:
                        case CCBT_unsigned_int128:
                        case CCBT_unsigned_long:
                        case CCBT_unsigned_long_long:
                        case CCBT_unsigned_short:
                        case CCBT_void:
                        return NC_UNSUPPORTED_TYPE;
                        CASES_EXHAUSTED;
                    }
                    uint32_t n = s->arm64.hfa_count;
                    ffi_type* st = Allocator_zalloc(a, sizeof(ffi_type) + sizeof(ffi_type*) * (n + 1));
                    if(!st) return NC_OOM_ERROR;
                    ffi_type** elements = (ffi_type**)(st + 1);
                    *st = (ffi_type){.size = 0, .alignment = 0, .type = FFI_TYPE_STRUCT, .elements = elements};
                    for(uint32_t i = 0; i < n; i++) elements[i] = base;
                    elements[n] = NULL;
                    s->ffi_cache = st;
                    *out = st;
                    return NC_NO_ERROR;
                }
                break;
            case CC_TARGET_X86_64_LINUX:
            case CC_TARGET_X86_64_MACOS:
                if(!s->sysv.is_memory_class
                    && (s->sysv.class0 == CC_SYSV_SSE || (s->size > 8 && s->sysv.class1 == CC_SYSV_SSE)))
                {
                    uint32_t sz = s->size;
                    uint32_t eb0 = sz > 8 ? 8 : sz;
                    uint32_t eb1 = sz > 8 ? sz - 8 : 0;
                    uint32_t n0 = eb0 / 4;
                    uint32_t n1 = eb1 / 4;
                    ffi_type* t0 = s->sysv.class0 == CC_SYSV_SSE ? &ffi_type_float : &ffi_type_uint32;
                    ffi_type* t1 = s->sysv.class1 == CC_SYSV_SSE ? &ffi_type_float : &ffi_type_uint32;
                    uint32_t total = n0 + n1;
                    ffi_type* st = Allocator_zalloc(a, sizeof(ffi_type) + sizeof(ffi_type*) * (total + 1));
                    if(!st) return NC_OOM_ERROR;
                    ffi_type** elements = (ffi_type**)(st + 1);
                    *st = (ffi_type){.size = 0, .alignment = 0, .type = FFI_TYPE_STRUCT, .elements = elements};
                    for(uint32_t i = 0; i < n0; i++) elements[i] = t0;
                    for(uint32_t i = 0; i < n1; i++) elements[n0 + i] = t1;
                    elements[total] = NULL;
                    s->ffi_cache = st;
                    *out = st;
                    return NC_NO_ERROR;
                }
                break;
            case CC_TARGET_X86_64_WINDOWS:
            case CC_TARGET_TEST:
            case CC_TARGET_COUNT:
                break;
            }
            {
                int err = cctype_integer_ffi(a, s->size, s->alignment, out);
                if(err) return err;
                s->ffi_cache = *out;
                return NC_NO_ERROR;
            }
        }
        case CC_UNION:{
            CcUnion* u = ccqt_as_union(t);
            if(u->ffi_cache){ *out = u->ffi_cache; return NC_NO_ERROR; }
            switch((CcTarget)CC_TARGET_NATIVE){
            case CC_TARGET_AARCH64_LINUX:
            case CC_TARGET_AARCH64_MACOS:
                if(u->arm64.hfa_count){
                    ffi_type* base;
                    switch((CcBasicTypeKind)u->arm64.hfa_type){
                        case CCBT_float:       base = &ffi_type_float; break;
                        case CCBT_double:      base = &ffi_type_double; break;
                        case CCBT_long_double: base = &ffi_type_longdouble; break;
                        case CCBT_COUNT:
                        case CCBT_INVALID:
                        case CCBT__Type:
                        case CCBT_bool:
                        case CCBT_char:
                        case CCBT_double_complex:
                        case CCBT_float128:
                        case CCBT_float16:
                        case CCBT_float_complex:
                        case CCBT_int:
                        case CCBT_int128:
                        case CCBT_long:
                        case CCBT_long_double_complex:
                        case CCBT_long_long:
                        case CCBT_nullptr_t:
                        case CCBT_short:
                        case CCBT_signed_char:
                        case CCBT_unsigned:
                        case CCBT_unsigned_char:
                        case CCBT_unsigned_int128:
                        case CCBT_unsigned_long:
                        case CCBT_unsigned_long_long:
                        case CCBT_unsigned_short:
                        case CCBT_void:
                        return NC_UNSUPPORTED_TYPE;
                        CASES_EXHAUSTED;
                    }
                    uint32_t n = u->arm64.hfa_count;
                    ffi_type* st = Allocator_zalloc(a, sizeof(ffi_type) + sizeof(ffi_type*) * (n + 1));
                    if(!st) return NC_OOM_ERROR;
                    ffi_type** elements = (ffi_type**)(st + 1);
                    *st = (ffi_type){.size = 0, .alignment = 0, .type = FFI_TYPE_STRUCT, .elements = elements};
                    for(uint32_t i = 0; i < n; i++) elements[i] = base;
                    elements[n] = NULL;
                    u->ffi_cache = st;
                    *out = st;
                    return NC_NO_ERROR;
                }
                break;
            case CC_TARGET_X86_64_LINUX:
            case CC_TARGET_X86_64_MACOS:
                if(!u->sysv.is_memory_class
                    && (u->sysv.class0 == CC_SYSV_SSE || (u->size > 8 && u->sysv.class1 == CC_SYSV_SSE)))
                {
                    uint32_t sz = u->size;
                    uint32_t eb0 = sz > 8 ? 8 : sz;
                    uint32_t eb1 = sz > 8 ? sz - 8 : 0;
                    uint32_t n0 = eb0 / 4;
                    uint32_t n1 = eb1 / 4;
                    ffi_type* t0 = u->sysv.class0 == CC_SYSV_SSE ? &ffi_type_float : &ffi_type_uint32;
                    ffi_type* t1 = u->sysv.class1 == CC_SYSV_SSE ? &ffi_type_float : &ffi_type_uint32;
                    uint32_t total = n0 + n1;
                    ffi_type* st = Allocator_zalloc(a, sizeof(ffi_type) + sizeof(ffi_type*) * (total + 1));
                    if(!st) return NC_OOM_ERROR;
                    ffi_type** elements = (ffi_type**)(st + 1);
                    *st = (ffi_type){.size = 0, .alignment = 0, .type = FFI_TYPE_STRUCT, .elements = elements};
                    for(uint32_t i = 0; i < n0; i++) elements[i] = t0;
                    for(uint32_t i = 0; i < n1; i++) elements[n0 + i] = t1;
                    elements[total] = NULL;
                    u->ffi_cache = st;
                    *out = st;
                    return NC_NO_ERROR;
                }
                break;
            case CC_TARGET_X86_64_WINDOWS:
            case CC_TARGET_TEST:
            case CC_TARGET_COUNT:
                break;
            }
            {
                int err = cctype_integer_ffi(a, u->size, u->alignment, out);
                if(err) return err;
                u->ffi_cache = *out;
                return NC_NO_ERROR;
            }
        }
        case CC_BASIC:
            switch(t.basic.kind){
                case CCBT_void:
                    *out = &ffi_type_void;
                    return NC_NO_ERROR;
                case CCBT_bool:
                    *out = &ffi_type_uint8;
                    return NC_NO_ERROR;
                case CCBT_char:
                    *out = &ffi_type_schar;
                    return NC_NO_ERROR;
                case CCBT_signed_char:
                    *out = &ffi_type_schar;
                    return NC_NO_ERROR;
                case CCBT_unsigned_char:
                    *out = &ffi_type_uchar;
                    return NC_NO_ERROR;
                case CCBT_short:
                    *out = &ffi_type_sshort;
                    return NC_NO_ERROR;
                case CCBT_unsigned_short:
                    *out = &ffi_type_ushort;
                    return NC_NO_ERROR;
                case CCBT_int:
                    *out = &ffi_type_sint;
                    return NC_NO_ERROR;
                case CCBT_unsigned:
                    *out = &ffi_type_uint;
                    return NC_NO_ERROR;
                case CCBT_long:
                    *out = &ffi_type_slong;
                    return NC_NO_ERROR;
                case CCBT_unsigned_long:
                    *out = &ffi_type_ulong;
                    return NC_NO_ERROR;
                case CCBT_long_long:
                    *out = &ffi_type_sint64;
                    return NC_NO_ERROR;
                case CCBT_unsigned_long_long:
                    *out = &ffi_type_uint64;
                    return NC_NO_ERROR;
                case CCBT_float:
                    *out = &ffi_type_float;
                    return NC_NO_ERROR;
                case CCBT_double:
                    *out = &ffi_type_double;
                    return NC_NO_ERROR;
                case CCBT_long_double:
                    *out = &ffi_type_longdouble;
                    return NC_NO_ERROR;
                case CCBT_float_complex:
                    #if defined FFI_TARGET_HAS_COMPLEX_TYPE && !defined NATIVE_CALL_NO_COMPLEX_TYPE
                    *out = &ffi_type_complex_float;
                    return NC_NO_ERROR;
                    #else
                    return NC_UNSUPPORTED_TYPE;
                    #endif
                case CCBT_double_complex:
                    #if defined FFI_TARGET_HAS_COMPLEX_TYPE && !defined NATIVE_CALL_NO_COMPLEX_TYPE
                    *out = &ffi_type_complex_double;
                    return NC_NO_ERROR;
                    #else
                    return NC_UNSUPPORTED_TYPE;
                    #endif
                case CCBT_long_double_complex:
                    #if defined FFI_TARGET_HAS_COMPLEX_TYPE && !defined NATIVE_CALL_NO_COMPLEX_TYPE
                    *out = &ffi_type_complex_longdouble;
                    return NC_NO_ERROR;
                    #else
                    return NC_UNSUPPORTED_TYPE;
                    #endif
                case CCBT_nullptr_t:
                    *out = &ffi_type_pointer;
                    return NC_NO_ERROR;
                case CCBT__Type:
                    *out = &ffi_type_pointer;
                    return NC_NO_ERROR;
                case CCBT_int128:
                case CCBT_unsigned_int128:
                    return cctype_integer_ffi(a, 16, 16, out);
                case CCBT_float128:
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

struct NativeCallCache {
    ffi_cif cif;
    uint32_t nparams;
    ffi_type*_Nonnull arg_types[];
};

static
int
native_call_cache_create(Allocator a, CcFunction* func_type, uint32_t nvarargs, const CcQualType*_Nullable vararg_types, NativeCallCache*_Nullable*_Nonnull out){
    *out = NULL;
    uint32_t fixed = func_type->param_count;
    uint32_t total = fixed + nvarargs;
    NativeCallCache* c = Allocator_zalloc(a, sizeof *c + total * sizeof *c->arg_types);
    if(!c) return NC_OOM_ERROR;
    c->nparams = total;
    ffi_type* rtype;
    int err = cctype_to_ffi_type(a, func_type->return_type, &rtype);
    if(err) goto fail;
    for(uint32_t i = 0; i < fixed; i++){
        err = cctype_to_ffi_type(a, func_type->params[i], &c->arg_types[i]);
        if(err) goto fail;
    }
    for(uint32_t i = 0; i < nvarargs; i++){
        err = cctype_to_ffi_type(a, vararg_types[i], &c->arg_types[fixed + i]);
        if(err) goto fail;
    }
    ffi_status s;
    if(nvarargs)
        s = ffi_prep_cif_var(&c->cif, FFI_DEFAULT_ABI, fixed, total, rtype, c->arg_types);
    else
        s = ffi_prep_cif(&c->cif, FFI_DEFAULT_ABI, total, rtype, c->arg_types);
    if(s != FFI_OK){ err = NC_UNSUPPORTED_TYPE; goto fail; }
    *out = c;
    return NC_NO_ERROR;
    fail:
    Allocator_free(a, c, sizeof *c + total * sizeof *c->arg_types);
    return err;
}

static
void
native_call(NativeCallCache* c, void (*fn)(void), void*_Nonnull*_Nonnull args, void* rvalue){
    // libffi writes at least sizeof(ffi_arg) bytes for the return value,
    // even for smaller types (int, short, etc.). Use a temp buffer to
    // prevent overwriting past the caller's rvalue buffer.
    if(c->cif.rtype->size < sizeof(ffi_arg)){
        ffi_arg tmp = 0;
        ffi_call(&c->cif, fn, &tmp, args);
        memcpy(rvalue, &tmp, c->cif.rtype->size);
    }
    else {
        ffi_call(&c->cif, fn, rvalue, args);
    }
}

static
void
native_call_cache_destroy(Allocator a, NativeCallCache* c){
    if(!c) return;
    // FIXME: this doesn't free allocated types
    // but also, no one calls this function so it kind of doesn't matter and in
    // practice the users just use an arena?
    Allocator_free(a, c, sizeof *c + c->nparams * sizeof *c->arg_types);
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
    int err = cctype_to_ffi_type(a, func_type->return_type, &rtype);
    if(err) goto fail;
    for(uint32_t i = 0; i < nparams; i++){
        err = cctype_to_ffi_type(a, func_type->params[i], &nc->arg_types[i]);
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
#else

static
int
native_call_cache_create(Allocator a, CcFunction* func_type, uint32_t nvarargs, const CcQualType*_Nullable vararg_types, NativeCallCache*_Nullable*_Nonnull out){
    (void)a;
    (void)func_type;
    (void)nvarargs;
    (void)vararg_types;
    (void)out;
    return NC_UNIMPLEMENTED_ERROR;
}
static
int
native_closure_create(Allocator al, CcFunction* func_type, NativeClosureCallback* cb, void*_Nullable userdata, NativeClosure*_Nullable*_Nonnull out){
    (void)al;
    (void)func_type;
    (void)cb;
    (void)userdata;
    (void)out;
    return NC_UNIMPLEMENTED_ERROR;
}

static
void (*native_closure_fn(NativeClosure* closure))(void){
    (void)closure;
    // should be unreachable
    return (void(*)(void))(uintptr_t)1;
}
static
void
native_call(NativeCallCache* nc, void (*fn)(void),
    void*_Nonnull*_Nonnull args, void* rvalue){
    (void)nc;
    (void)fn;
    (void)args;
    (void)rvalue;
    // ... uh no way to return an error, but also should be unreachable.
}
#endif

#ifdef __clang__
#pragma clang assume_nonnull end
#endif
#endif
