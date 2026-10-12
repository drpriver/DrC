#ifndef C_NATIVE_CALL_C
#define C_NATIVE_CALL_C
//
// Copyright © 2026-2026, David Priver <david@davidpriver.com>
//

#if defined _WIN32 && !defined __MINGW32__
// Use the MSVC ABI with clang targeting the Microsoft CRT.
#pragma push_macro("__GNUC__")
#undef __GNUC__
#endif
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
#if defined _WIN32 && !defined __MINGW32__
#pragma pop_macro("__GNUC__")
#endif

#ifdef _WIN32
#define NATIVE_CALL_NO_COMPLEX_TYPE
#endif

#include "cc_errors.h"
#include "native_call.h"
#include "cc_func.h"
#include "cc_target.h"
#include "cc_rt_types.h"

enum {
    NC_NO_ERROR = _cc_no_error,
    NC_OOM_ERROR = _cc_oom_error,
    NC_UNSUPPORTED_TYPE = _cc_unimplemented_error,
    NC_UNIMPLEMENTED_ERROR = _cc_unimplemented_error,
};

#ifdef __clang__
#pragma clang assume_nonnull begin
#endif
// Only use this when it is absolutely impossible, prefer an unreachable error
// if it could be a bug.
#ifndef nc_unsafe_unreachable
#ifdef __GNUC__
#define nc_unsafe_unreachable() __builtin_unreachable()
#elif defined _MSC_VER
#define nc_unsafe_unreachable() __assume(0)
#else
#define nc_unsafe_unreachable() abort()
#endif
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
    *st = (ffi_type){.size = sz, .alignment = (unsigned short)al, .type = FFI_TYPE_STRUCT, .elements = elements};
    for(uint32_t i = 0; i < rep_count; i++) elements[i] = rep_elem;
    elements[rep_count] = NULL;
    *out = st;
    return NC_NO_ERROR;
}

static
int
cctype_sysv_ffi(Allocator a, const CcStruct* s, ffi_type*_Nonnull*_Nonnull out){
    const CcSysvAggregate* abi = &s->sysv;
    if(abi->is_memory_class){
        // libffi's public type API cannot express an unaligned field. Do not
        // accidentally turn a small memory-class record into integer registers.
        if(s->size <= 16) return NC_UNSUPPORTED_TYPE;
        return cctype_integer_ffi(a, s->size, s->alignment, out);
    }
    if(!s->size) return NC_UNSUPPORTED_TYPE;
    // A single x87 record has the same argument/result representation as
    // long double. Older libffi versions mishandle x87 struct return flags.
    if(abi->eightbyte_count == 2 && cc_sysv_class(abi, 0) == CC_SYSV_X87 && cc_sysv_class(abi, 1) == CC_SYSV_X87UP){
        *out = &ffi_type_longdouble;
        return NC_NO_ERROR;
    }
    ffi_type* elems[16];
    uint32_t n = 0;
    for(uint32_t i = 0; i < abi->eightbyte_count; i++){
        CcSysVEightByte c = cc_sysv_class(abi, i);
        uint32_t remaining = s->size-i*8;
        if(remaining > 8) remaining = 8;
        if(c == CC_SYSV_NO_CLASS){
            // Preserve tail padding through explicit size/alignment. Internal
            // holes cannot be represented by synthesizing integer members.
            for(uint32_t j = i+1; j < abi->eightbyte_count; j++)
                if(cc_sysv_class(abi, j) != CC_SYSV_NO_CLASS)
                    return NC_UNSUPPORTED_TYPE;
            break;
        }
        if(c != CC_SYSV_INTEGER && c != CC_SYSV_SSE)
            return NC_UNSUPPORTED_TYPE; // SSEUP/vector/quad has no libffi type
        while(remaining){
            ffi_type* elem;
            if(c == CC_SYSV_SSE){
                if(remaining < 4) return NC_UNSUPPORTED_TYPE;
                elem = &ffi_type_float;
            }
            else if(remaining >= 4) elem = &ffi_type_uint32;
            else if(remaining >= 2) elem = &ffi_type_uint16;
            else elem = &ffi_type_uint8;
            elems[n++] = elem;
            remaining -= (uint32_t)elem->size;
        }
    }
    ffi_type* st = Allocator_zalloc(a, sizeof *st + sizeof(ffi_type*)*(n+1));
    if(!st) return NC_OOM_ERROR;
    ffi_type** elements = (ffi_type**)(st+1);
    *st = (ffi_type){
        .size=s->size,
        .alignment=(unsigned short)s->alignment,
        .type=FFI_TYPE_STRUCT,
        .elements=elements,
    };
    if(n) memcpy(elements, elems, n * sizeof *elements);
    *out = st;
    return NC_NO_ERROR;
}

static
int
cctype_to_ffi_type(Allocator a, CcQualType t, ffi_type*_Nonnull*_Nonnull out){
    switch(ccqt_kind(t)){
        case CC_ALIGNED: nc_unsafe_unreachable();
        case CC_SLICE:
            return cctype_integer_ffi(a, sizeof(CiRtSlice), _Alignof(CiRtSlice), out);
        case CC_BLOCK_POINTER:
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
        case CC_STRUCT:
        case CC_UNION:{
            CcStruct* s = ccqt_as_struct(t);
            if(s->has_wide_vector) return NC_UNSUPPORTED_TYPE;
            if(s->ffi_cache){ *out = s->ffi_cache; return NC_NO_ERROR; }
            int err;
            switch(CC_TARGET_NATIVE){
                case CC_TARGET_X86_64_LINUX:
                case CC_TARGET_X86_64_MACOS:
                    err = cctype_sysv_ffi(a, s, out);
                    break;
                case CC_TARGET_AARCH64_LINUX:
                case CC_TARGET_AARCH64_MACOS:
                    // libffi has no short-vector type.
                    if(s->arm64.hva_count) return NC_UNSUPPORTED_TYPE;
                    if(s->arm64.hfa_count){
                        ffi_type* base;
                        switch(s->arm64.hfa_type){
                            case CCBT_float: base = &ffi_type_float; break;
                            case CCBT_double: base = &ffi_type_double; break;
                            case CCBT_long_double: base = &ffi_type_longdouble; break;
                            default: return NC_UNSUPPORTED_TYPE;
                        }
                        uint32_t n = s->arm64.hfa_count;
                        ffi_type* st = Allocator_zalloc(a, sizeof *st + sizeof(ffi_type*)*(n+1));
                        if(!st) return NC_OOM_ERROR;
                        ffi_type** elements = (ffi_type**)(st+1);
                        *st = (ffi_type){
                            .size=s->size,
                            .alignment=(unsigned short)s->alignment,
                            .type=FFI_TYPE_STRUCT,
                            .elements=elements
                        };
                        for(uint32_t i = 0; i < n; i++) elements[i] = base;
                        *out = st;
                        err = NC_NO_ERROR;
                    }
                    else err = cctype_integer_ffi(a, s->size, s->arm64.copy_alignment, out);
                    break;
                case CC_TARGET_X86_64_WINDOWS_MSVC:
                case CC_TARGET_X86_64_WINDOWS_MINGW:
                case CC_TARGET_TEST:
                    err = cctype_integer_ffi(a, s->size, s->alignment, out);
                    break;
                case CC_TARGET_COUNT:
                    return NC_UNSUPPORTED_TYPE;
            }
            if(err) return err;
            s->ffi_cache = *out;
            return NC_NO_ERROR;
        }
        case CC_BASIC:
            switch(ccqt_basic_kind(t)){
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
                case CCBT_float16_complex:
                case CCBT_char_complex:
                case CCBT_signed_char_complex:
                case CCBT_unsigned_char_complex:
                case CCBT_short_complex:
                case CCBT_unsigned_short_complex:
                case CCBT_int_complex:
                case CCBT_unsigned_complex:
                case CCBT_long_complex:
                case CCBT_unsigned_long_complex:
                case CCBT_long_long_complex:
                case CCBT_unsigned_long_long_complex:
                case CCBT_int128_complex:
                case CCBT_unsigned_int128_complex:
                    return NC_UNSUPPORTED_TYPE;
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
                case CCBT_bfloat16:
                case CCBT_float16:
                    return NC_UNSUPPORTED_TYPE;
                case CCBT__Any:
                    return cctype_integer_ffi(a, sizeof(CiRtAny), _Alignof(CiRtAny), out);
                case CCBT_INVALID:
                case CCBT_COUNT:
                    return NC_UNSUPPORTED_TYPE;
            }
            break;
    }
    return NC_UNSUPPORTED_TYPE;
}

// Older SysV libffi closure decoders advance a register for NO_CLASS padding.
// Use an eight-byte view for a record with a padding-only second eightbyte
// when it is actually in registers. Spilled arguments must retain all 16 bytes.
static
_Bool
native_sysv_arg_demand(CcQualType type, uint32_t* gp, uint32_t* fp){
    *gp = *fp = 0;
    while(ccqt_kind(type) == CC_ENUM) type = ccqt_as_enum(type)->underlying;
    switch(ccqt_kind(type)){
        case CC_STRUCT:
        case CC_UNION:{
            const CcSysvAggregate* abi = &ccqt_as_struct(type)->sysv;
            *gp = abi->gp_count;
            *fp = abi->fp_count;
            return !abi->argument_on_stack;
        }
        case CC_BASIC:{
            CcBasicTypeKind k = ccqt_basic_kind(type);
            if(k == CCBT_void) return 1;
            if(k == CCBT_long_double || k == CCBT_long_double_complex)
                return 0; // native SysV long double uses x87, never argument registers
            if(k == CCBT_double_complex){ *fp = 2; return 1; }
            if(ccbt_is_float(k) || k == CCBT_float_complex || k == CCBT_float16_complex){
                *fp = 1;
                return 1;
            }
            uint32_t size = cc_target_native.sizeof_[k];
            if(size > 16) return 0;
            *gp = (size+7)/8;
            return 1;
        }
        case CC_BLOCK_POINTER:
        case CC_POINTER:
        case CC_FUNCTION:
            *gp = 1;
            return 1;
        case CC_SLICE:
            *gp = 2;
            return 1;
        case CC_ARRAY: return 0; // native vector arguments are currently unsupported
        case CC_ENUM:
        case CC_ALIGNED: nc_unsafe_unreachable();
    }
    return 0;
}

static
void
native_sysv_register_view(CcQualType type, ffi_type*_Nonnull*_Nonnull ft, ffi_type* view){
    CcTypeKind kind = ccqt_kind(type);
    if(kind != CC_STRUCT && kind != CC_UNION)
        return;
    const CcStruct* s = ccqt_as_struct(type);
    if(s->size != 16 || s->sysv.eightbyte_count != 2 || cc_sysv_class(&s->sysv, 1) != CC_SYSV_NO_CLASS)
        return;
    *view = **ft;
    view->size = 8;
    *ft = view;
}

static
void
native_ffi_signature_views(CcFunction* type, ffi_type*_Nonnull*_Nonnull rtype, ffi_type*_Nonnull*_Nonnull args, ffi_type* views){
    if(CC_TARGET_NATIVE != CC_TARGET_X86_64_LINUX && CC_TARGET_NATIVE != CC_TARGET_X86_64_MACOS)
        return;
    uint32_t gp_used = 0,
             fp_used = 0;
    CcTypeKind ret_kind = ccqt_kind(type->return_type);
    if(ret_kind == CC_STRUCT || ret_kind == CC_UNION)
        gp_used = ccqt_as_struct(type->return_type)->sysv.is_memory_class ? 1 : 0;
    native_sysv_register_view(type->return_type, rtype, &views[type->param_count]);
    for(uint32_t i = 0; i < type->param_count; i++){
        uint32_t gp, fp;
        if(!native_sysv_arg_demand(type->params[i], &gp, &fp) || gp_used+gp > 6 || fp_used+fp > 8)
            continue;
        gp_used += gp;
        fp_used += fp;
        native_sysv_register_view(type->params[i], &args[i], &views[i]);
    }
}

struct NativeCallCache {
    ffi_cif cif;
    uint32_t nparams;
    ffi_type*_Nonnull arg_types[];
};

static
int
native_call_cache_create(Allocator a, CcFunction* func_type, NativeCallCache*_Nullable*_Nonnull out){
    *out = NULL;
    uint32_t fixed = func_type->fixed_param_count;
    uint32_t total = func_type->param_count;
    size_t alloc_size = sizeof(NativeCallCache) + total * sizeof(ffi_type*) + ((size_t)total+1)*sizeof(ffi_type);
    NativeCallCache* c = Allocator_zalloc(a, alloc_size);
    if(!c) return NC_OOM_ERROR;
    c->nparams = total;
    ffi_type* rtype;
    int err = cctype_to_ffi_type(a, func_type->return_type, &rtype);
    if(err) goto fail;
    for(uint32_t i = 0; i < total; i++){
        err = cctype_to_ffi_type(a, func_type->params[i], &c->arg_types[i]);
        if(err) goto fail;
    }
    native_ffi_signature_views(func_type, &rtype, c->arg_types, (ffi_type*)(c->arg_types+total));
    ffi_status s;
    if(total > fixed)
        s = ffi_prep_cif_var(&c->cif, FFI_DEFAULT_ABI, fixed, total, rtype, c->arg_types);
    else
        s = ffi_prep_cif(&c->cif, FFI_DEFAULT_ABI, total, rtype, c->arg_types);
    if(s != FFI_OK){ err = NC_UNSUPPORTED_TYPE; goto fail; }
    *out = c;
    return NC_NO_ERROR;
    fail:
    Allocator_free(a, c, alloc_size);
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
    Allocator_free(a, c, sizeof *c + c->nparams * sizeof *c->arg_types + ((size_t)c->nparams+1)*sizeof(ffi_type));
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
    size_t alloc_size = sizeof(NativeClosure) + nparams * sizeof(ffi_type*) + ((size_t)nparams+1)*sizeof(ffi_type);
    NativeClosure* nc = Allocator_zalloc(a, alloc_size);
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
    native_ffi_signature_views(func_type, &rtype, nc->arg_types, (ffi_type*)(nc->arg_types+nparams));
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
    Allocator_free(a, nc, alloc_size);
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
    Allocator_free(a, closure, sizeof *closure + closure->nparams * sizeof *closure->arg_types + ((size_t)closure->nparams+1)*sizeof(ffi_type));
}
#else

static
int
native_call_cache_create(Allocator a, CcFunction* func_type, NativeCallCache*_Nullable*_Nonnull out){
    (void)a;
    (void)func_type;
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
