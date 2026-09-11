//
// Copyright © 2026-2026, David Priver <david@davidpriver.com>
//
#include <string.h>
#include <stddef.h>
#ifdef _WIN32
#include "../Drp/windowsheader.h"
#include <Psapi.h>
#else
#include <dlfcn.h>
#if defined __GLIBC__ && !defined RTLD_DEFAULT
#define RTLD_DEFAULT ((void *)0)
#endif
#endif
#include "ci_interp.h"
#include "ci_op.h"
#include "cc_memory_order.h"
#include "cc_errors.h"
#include "cc_var.h"
#include "cc_expr.h"
#include "cc_target.h"
#include "cc_scope.h"
#include "cpp_preprocessor.h"
#include "native_call.h"
#include "ci_softnum.h"
#include "../Drp/Allocators/allocator.h"
#include "../Drp/Allocators/mallocator.h"
#include "../Drp/Allocators/arena_allocator.h"
#include "../Drp/MStringBuilder.h"
#include "../Drp/MStringBuilder16.h"
#include "../Drp/cmd_run.h"
#include "../Drp/stringview.h"
#include "../Drp/argument_parsing.h"
#include "../Drp/bit_util.h"
#include "../Drp/msb_atomize.h"
#include "../Drp/switch_macros.h"
#include "../Drp/atomics.h"
#ifdef __clang__
#pragma clang assume_nonnull begin
#endif

#ifndef force_inline
#if defined __GNUC__ || defined __clang__
#define force_inline static inline __attribute__((always_inline))
#elif defined _MSC_VER
#define force_inline static inline __forceinline
#else
#define force_inline static inline
#endif
#endif

force_inline int _ci_interp_step(CiInterpreter* ci, CiInterpFrame* frame, CiInterpFrame*_Nullable*_Nonnull child);
static void ci_free_call_frame(CiInterpreter*, CiInterpFrame*);


// Internal opcode result: enter the child frame
enum { CI_STEP_ENTER_FRAME = -1 };

enum {
    CI_NO_ERROR = _cc_no_error,
    CI_OOM_ERROR = _cc_oom_error,
    CI_SYNTAX_ERROR = _cc_syntax_error,
    CI_UNREACHABLE_ERROR = _cc_unreachable_error,
    CI_UNIMPLEMENTED_ERROR = _cc_unimplemented_error,
    CI_RUNTIME_ERROR = _cc_runtime_error,
    CI_INVALID_VALUE_ERROR = _cc_invalid_value_error,
    CI_LIBRARY_NOT_FOUND_ERROR = _cc_file_not_found_error,
    CI_SYMBOL_NOT_FOUND = _cc_symbol_not_found_error,
    CI_SYMBOL_UNRESOLVED = _cc_symbol_unresolved_error,
};
LOG_PRINTF(3, 4) static int ci_error(CiInterpreter*, SrcLoc, const char*, ...);
#ifndef ci_ice
#define ci_ice(ci, loc, fmt, ...) ci_error(ci, loc, "ICE: " fmt " at %s:%d", __VA_ARGS__, __FILE__, __LINE__)
#endif

struct CiModule {
    CcScope scope;
    Parray(CcStmtNode) nodes; // toplevel trees; swapped with the parser's during compile
    Marray(CiOp) ops;
    AtomMap(uintptr_t) labels; // label -> op index + 1
    size_t lowered; // count of nodes already lowered into ops
    uint32_t slot_size; // bytes of slot storage the ops need
    StringView source;
};
static int ci_lower_module(CiInterpreter*, CiModule*);

static char ci_discard_buf[8192];

#define CI_RESULT_TOO_SMALL(ci, loc, sz, size) \
    ci_error((ci), (loc), "interpreter:%s:%d: result buffer too small (need %zu, have %zu)", __FILE__, __LINE__, (size_t)(sz), (size_t)(size))

// SysV x86_64 va_list layout.
typedef struct CiSysvVaListTag CiSysvVaListTag;
struct CiSysvVaListTag {
    unsigned gp_offset;
    unsigned fp_offset;
    void* overflow_arg_area;
    void* reg_save_area;
};

// AAPCS64 (Linux ARM64) va_list layout.
typedef struct CiAapcs64VaList CiAapcs64VaList;
struct CiAapcs64VaList {
    void* __stack;
    void* __gr_top;
    void* __vr_top;
    int __gr_offs; // negative, counts up toward 0
    int __vr_offs;
};
static Allocator ci_allocator(CiInterpreter*);
static Allocator ci_scratch_allocator(CiInterpreter*);

static Allocator cc_allocator(CcParser*);

static
void
ci_free_alloca_list(Allocator al, CiAllocaBlock*_Null_unspecified list){
    while(list){
        CiAllocaBlock* next = list->next;
        Allocator_free(al, list, sizeof(CiAllocaBlock) + list->size);
        list = next;
    }
}
static const CcTargetConfig* ci_target(const CiInterpreter*);
static int ci_dlsym(CiInterpreter*, SrcLoc, LongString, const char* what, void*_Nullable*_Nonnull);
static CcFunc*_Nullable ci_hotswap_target(CcFunc*);
static int ci_make_call_frame(CiInterpreter*, CiInterpFrame*_Nullable, CcFunc*, void*_Nonnull*_Null_unspecified, uint32_t, const uint32_t*_Nullable, void*, size_t, SrcLoc, CiInterpFrame*_Nullable*_Nonnull);
static int ci_call_argv(CiInterpreter*, CiInterpFrame*_Nullable caller, CcFunc*, void*_Nonnull*_Nonnull argv, uint32_t nargs, const uint32_t*_Nullable arg_sizes, void* result, size_t size, SrcLoc loc);
static int ci_lookup_symbol(CiInterpreter*, SrcLoc, CiModule*_Nullable, const char*, CcQualType, void*_Nullable*_Nonnull);
static int ci_compile_module(CiInterpreter*, const char*, CiModule*_Nullable*_Nonnull);
static int ci_resolve_module(CiInterpreter*, CiModule*);
static int ci_parse_module_type(CiInterpreter*, SrcLoc, CiModule*_Nullable, const char*, CcQualType*);
static int ci_reflect_module(CiInterpreter*, SrcLoc, CiModule*_Nullable, CcModuleOp, size_t, CiRtModuleMember*);
static int ci_try_dlsym(CiInterpreter*, LongString, void*_Nullable*_Nonnull);
static void ci_lock_resolver(CiInterpreter*);
static void ci_unlock_resolver(CiInterpreter*);
// re-declare here as I'm not sure if this should be used in the interpreter or not
static int cc_sizeof_as_uint(CcParser* p, CcQualType t, SrcLoc loc, uint32_t* out);
static int cc_alignof_as_uint(CcParser* p, CcQualType t, SrcLoc loc, uint32_t* out);
static _Bool cc_any_payload_type(CcParser* p, CcQualType t);
static _Bool cc_implicit_convertible(CcParser* p, CcQualType from, CcQualType to);
static _Bool cc_explicit_castable(CcParser* p, CcQualType from, CcQualType to);
static int ci_eval_lowered_expr(CiInterpreter*, CiInterpFrame*_Nullable, CcExpr*, void*, size_t);
static int cc_parse_expr(CcParser* p, CcValueClass, CcExpr* _Nullable* _Nonnull out);
static void cc_release_expr(CcParser* p, CcExpr* e);

static CppFuncMacroFn ci_shell, ci_procmacro_expand;

typedef struct { _Alignas(16) char bytes[16]; } CiAtomic16;
static
int
ci_error(CiInterpreter* ci, SrcLoc loc, const char* fmt, ...){
    LOCK_T_lock(&ci->error_lock);
    va_list va;
    va_start(va, fmt);
    cpp_msg(&ci->parser.cpp, loc, LOG_PRINT_ERROR, "error", fmt, va);
    va_end(va);
    LOCK_T_unlock(&ci->error_lock);
    return CI_RUNTIME_ERROR;
}
#define ci_unimplemented(p, loc, msg) (ci_error(p, loc, "UNIMPLEMENTED: " msg " at %s:%d", __FILE__, __LINE__), CI_UNIMPLEMENTED_ERROR)
#define ci_unreachable(p, loc, msg) (ci_error(p, loc, "UNREACHABLE: " msg " at %s:%d", __FILE__, __LINE__), CI_UNREACHABLE_ERROR)

static inline
uint64_t
ci_read_uint(const void* buf, uint32_t sz){
    switch(sz){
        case 1: return *(const uint8_t*)buf;
        case 2: return *(const uint16_t*)buf;
        case 4: return *(const uint32_t*)buf;
        case 8: return *(const uint64_t*)buf;
    }
    assert(sz == 1 || sz == 2 || sz == 4 || sz == 8);
    return 0;
}

static inline
int64_t
ci_read_int(const void* buf, uint32_t sz){
    switch(sz){
        case 1: return *(const int8_t*)buf;
        case 2: return *(const int16_t*)buf;
        case 4: return *(const int32_t*)buf;
        case 8: return *(const int64_t*)buf;
    }
    assert(sz == 1 || sz == 2 || sz == 4 || sz == 8);
    return 0;
}


#if defined __has_builtin
#if __has_builtin(__builtin_memcpy_inline)
#define CI_INLINE_MEMCPY(dst, src, size) __builtin_memcpy_inline(dst, src, size)
#elif __has_builtin(__builtin_memcpy)
#define CI_INLINE_MEMCPY(dst, src, size) __builtin_memcpy(dst, src, size)
#endif
#endif
#ifndef CI_INLINE_MEMCPY
#ifdef __GNUC__
#define CI_INLINE_MEMCPY(dst, src, size) __builtin_memcpy(dst, src, size)
#else
#define CI_INLINE_MEMCPY(dst, src, size) memcpy(dst, src, size)
#endif
#endif

static inline
void
ci_write_uint(void* buf, uint32_t sz, uint64_t val){
    switch(sz){
        case 1: CI_INLINE_MEMCPY(buf, &val, 1); return;
        case 2: CI_INLINE_MEMCPY(buf, &val, 2); return;
        case 4: CI_INLINE_MEMCPY(buf, &val, 4); return;
        case 8: CI_INLINE_MEMCPY(buf, &val, 8); return;
        default:
            if(sz > 8){
                CI_INLINE_MEMCPY(buf, &val, 8);
                memset((char*)buf+8, 0, sz-8);
            }
            else {
                memcpy(buf, &val, sz);
            }
            return;
    }
}

static inline
void
ci_copy(void* dst, const void* src, uint32_t sz){
    switch(sz){
        case 1:  CI_INLINE_MEMCPY(dst, src, 1);  return;
        case 2:  CI_INLINE_MEMCPY(dst, src, 2);  return;
        case 4:  CI_INLINE_MEMCPY(dst, src, 4);  return;
        case 8:  CI_INLINE_MEMCPY(dst, src, 8);  return;
        case 16: CI_INLINE_MEMCPY(dst, src, 16); return;
        default: memmove(dst, src, sz); return;
    }
}



static inline
double
ci_read_float(const void* buf, CcBasicTypeKind k){
    if(k == CCBT_float){
        float f;
        CI_INLINE_MEMCPY(&f, buf, sizeof f);
        return (double)f;
    }
    double d;
    CI_INLINE_MEMCPY(&d, buf, sizeof d);
    return d;
}

static
int
ci_ensure_var_storage(CiInterpreter* ci, CcVariable* var){
    if(var->automatic) return 0;
    if(var->interp_val) return 0;
    return ci_ice(ci, var->loc, "variable '%s' storage not resolved before execution", var->name->data);
}

static
int
ci_atomic_check_size(CiInterpreter* ci, SrcLoc loc, uint32_t sz){
    if(!sz || (sz & (sz - 1)))
        return ci_error(ci, loc, "atomic operand size %u is not a power of 2", sz);
    if(sz > ci_target(ci)->atomic_lock_free_max)
        return ci_error(ci, loc, "atomic operand size %u exceeds target's maximum lock-free size %u", sz, ci_target(ci)->atomic_lock_free_max);
    switch(sz){
        case 1: case 2: case 4: case 8: case 16:
            return 0;
        default:
            return ci_error(ci, loc, "unsupported atomic operand size %u", sz);
    }
}

static inline
uint64_t
ci_bitfield_read(void* storage_addr, uint32_t storage_sz, uint8_t bit_offset, uint8_t bit_width){
    uint64_t storage = 0;
    memcpy(&storage, storage_addr, storage_sz);
    uint64_t mask = bit_width >= 64 ? ~(uint64_t)0 : ((uint64_t)1 << bit_width) - 1;
    return (storage >> bit_offset) & mask;
}

// Sign-extend a bitfield value if it is a signed type.
static inline
uint64_t
ci_bitfield_extend(uint64_t val, uint8_t bit_width, _Bool is_signed){
    uint64_t mask = bit_width >= 64 ? ~(uint64_t)0 : ((uint64_t)1 << bit_width) - 1;
    val &= mask;
    if(is_signed){
        uint64_t sign_bit = (uint64_t)1 << (bit_width - 1);
        if(val & sign_bit)
            val |= ~mask;
    }
    return val;
}

static inline
void
ci_bitfield_write(void* storage_addr, uint32_t storage_sz, uint8_t bit_offset, uint8_t bit_width, uint64_t val){
    uint64_t mask = bit_width >= 64 ? ~(uint64_t)0 : ((uint64_t)1 << bit_width) - 1;
    uint64_t storage = 0;
    memcpy(&storage, storage_addr, storage_sz);
    storage &= ~(mask << bit_offset);
    storage |= (val & mask) << bit_offset;
    memcpy(storage_addr, &storage, storage_sz);
}

// Userdata for interpreted function closures.
typedef struct CiClosureData CiClosureData;
struct CiClosureData {
    CiInterpreter* ci;
    CcFunc* func;
};

static
void
ci_closure_callback(void* rvalue, void*_Nonnull*_Nonnull args, void* userdata){
    CiClosureData* cd = userdata;
    CiInterpreter* ci = cd->ci;
    CcFunc* func = cd->func;
    CcFunction* ftype = func->type;
    uint32_t ret_sz = 0;
    if(!(ccqt_is_basic(ftype->return_type) && ftype->return_type.basic.kind == CCBT_void)){
        int err = cc_sizeof_as_uint(&ci->parser, ftype->return_type, func->loc, &ret_sz);
        if(err) return;
    }
    void* result = ret_sz ? rvalue : ci_discard_buf;
    size_t size = ret_sz ? ret_sz : sizeof ci_discard_buf;
    ci_call_argv(ci, NULL, func, args, ftype->param_count, NULL, result, size, func->loc);
}

// Create a native closure for an interpreted function, storing the
// resulting function pointer in func->native_func.
static
int
ci_create_closure(CiInterpreter* ci, CcFunc* func){
    #ifdef NO_NATIVE_CALL
        // No native code can call this, so just use the CcFunc* itself as a
        // fake function pointer. The closure_map reverse lookup will catch it
        // at interpreted call sites.
        func->native_func = (void(*)(void))(uintptr_t)func;
    #else
        if(func->type->is_variadic)
            return ci_error(ci, func->loc, "cannot take address of variadic interpreted function");
        CiClosureData* cd = Allocator_zalloc(ci_allocator(ci), sizeof *cd);
        if(!cd) return CI_OOM_ERROR;
        cd->ci = ci;
        cd->func = func;
        NativeClosure* nc = NULL;
        int err = native_closure_create(ci_allocator(ci), func->type, ci_closure_callback, cd, &nc);
        if(err){
            Allocator_free(ci_allocator(ci), cd, sizeof *cd);
            return err;
        }
        func->native_func = native_closure_fn(nc);
        func->native_closure = nc;
    #endif
    int e = BPM_put(&ci->closure_map, ci_allocator(ci), func, (void*)func->native_func);
    if(e) return e == 1 ? CI_OOM_ERROR : CI_RUNTIME_ERROR;
    return 0;
}

static
int
ci_type_reflect_validate(CiInterpreter* ci, SrcLoc loc, CcTypeIntrospectionOp op, CcQualType qt){
    if(op == CC_TYPE_FIELD && ccqt_kind(qt) != CC_STRUCT && ccqt_kind(qt) != CC_UNION)
        return ci_error(ci, loc, "_Type.field: not a struct or union type");
    if(op == CC_TYPE_ENUMERATOR && ccqt_kind(qt) != CC_ENUM)
        return ci_error(ci, loc, "_Type.enumerator: not an enum type");
    if(op == CC_TYPE_PARAM_TYPE){
        if(ccqt_kind(qt) == CC_POINTER) qt = ccqt_as_ptr(qt)->pointee;
        if(ccqt_kind(qt) != CC_FUNCTION)
            return ci_error(ci, loc, "_Type.param_type: not a function type");
    }
    return 0;
}

static
int
ci_type_reflect(CiInterpreter* ci, SrcLoc loc, CcTypeIntrospectionOp op, CcQualType qt, uintptr_t arg, void* result){
    int err;
    switch(op){
        case CC_TYPE_NONE:
            return CI_UNREACHABLE_ERROR;
        case CC_TYPE_NAME: {
            MStringBuilder sb = {.allocator = ci_allocator(ci)};
            cc_print_type(&sb, qt);
            Atom a;
            {
                AtomTable* at = ci_lock_atoms(ci);
                a = msb_atomize(&sb, at);
                ci_unlock_atoms(ci, at);
            }
            msb_destroy(&sb);
            if(!a) return CI_OOM_ERROR;
            *(const char**)result = a->data;
            return 0;
        }
        case CC_TYPE_TAG: {
            Atom tag = 0;
            CcTypeKind k = ccqt_kind(qt);
            if(k == CC_STRUCT)     tag = ccqt_as_struct(qt)->name;
            else if(k == CC_UNION) tag = ccqt_as_union(qt)->name;
            else if(k == CC_ENUM)  tag = ccqt_as_enum(qt)->name;
            *(const char**)result = tag ? tag->data : "";
            return 0;
        }
        case CC_TYPE_IS_VALID:
        case CC_TYPE_IS_INVALID:
            *(_Bool*)result = (qt.bits != 0) == (op == CC_TYPE_IS_VALID);
            return 0;
        case CC_TYPE_IS_INTEGER: {
            CcQualType st = qt;
            while(ccqt_kind(st) == CC_ENUM) st = ccqt_as_enum(st)->underlying;
            *(_Bool*)result = ccqt_is_basic(st) && ccbt_is_integer(st.basic.kind);
            return 0;
        }
        case CC_TYPE_IS_FLOAT: {
            *(_Bool*)result = ccqt_is_basic(qt) && ccbt_is_float(qt.basic.kind);
            return 0;
        }
        case CC_TYPE_IS_ARITHMETIC: {
            *(_Bool*)result = (ccqt_is_basic(qt) && ccbt_is_arithmetic(qt.basic.kind)) || ccqt_kind(qt) == CC_ENUM;
            return 0;
        }
        case CC_TYPE_IS_POINTER: {
            *(_Bool*)result = ccqt_kind(qt) == CC_POINTER;
            return 0;
        }
        case CC_TYPE_IS_STRUCT: {
            *(_Bool*)result = ccqt_kind(qt) == CC_STRUCT;
            return 0;
        }
        case CC_TYPE_IS_UNION: {
            *(_Bool*)result = ccqt_kind(qt) == CC_UNION;
            return 0;
        }
        case CC_TYPE_IS_ARRAY: {
            *(_Bool*)result = ccqt_kind(qt) == CC_ARRAY && !ccqt_as_array(qt)->is_vector;
            return 0;
        }
        case CC_TYPE_IS_VECTOR: {
            *(_Bool*)result = ccqt_kind(qt) == CC_ARRAY && ccqt_as_array(qt)->is_vector;
            return 0;
        }
        case CC_TYPE_IS_SLICE: {
            *(_Bool*)result = ccqt_kind(qt) == CC_SLICE;
            return 0;
        }
        case CC_TYPE_IS_FUNCTION: {
            *(_Bool*)result = ccqt_kind(qt) == CC_FUNCTION;
            return 0;
        }
        case CC_TYPE_IS_ENUM: {
            *(_Bool*)result = ccqt_kind(qt) == CC_ENUM;
            return 0;
        }
        case CC_TYPE_IS_CONST: {
            *(_Bool*)result = qt.is_const;
            return 0;
        }
        case CC_TYPE_IS_VOLATILE: {
            *(_Bool*)result = qt.is_volatile;
            return 0;
        }
        case CC_TYPE_IS_ATOMIC: {
            *(_Bool*)result = qt.is_atomic;
            return 0;
        }
        case CC_TYPE_IS_UNSIGNED: {
            *(_Bool*)result = ccqt_is_unsigned(qt, !ci_target(ci)->char_is_signed);
            return 0;
        }
        case CC_TYPE_IS_SIGNED: {
            CcQualType st = qt;
            while(ccqt_kind(st) == CC_ENUM) st = ccqt_as_enum(st)->underlying;
            *(_Bool*)result = ccqt_is_basic(st) && ccbt_is_integer(st.basic.kind) && !ccbt_is_unsigned(st.basic.kind, !ci_target(ci)->char_is_signed);
            return 0;
        }
        case CC_TYPE_SIZEOF: {
            uint32_t sz;
            err = cc_sizeof_as_uint(&ci->parser, qt, loc, &sz);
            if(err) return err;
            *(size_t*)result = sz;
            return 0;
        }
        case CC_TYPE_ALIGNOF: {
            uint32_t al;
            err = cc_alignof_as_uint(&ci->parser, qt, loc, &al);
            if(err) return err;
            *(size_t*)result = al;
            return 0;
        }
        case CC_TYPE_POINTEE: {
            if(ccqt_kind(qt) != CC_POINTER)
                return ci_error(ci, loc, "_Type.pointee: not a pointer type");
            CcPointer* ptr = ccqt_as_ptr(qt);
            *(uintptr_t*)result = ptr->pointee.bits;
            return 0;
        }
        case CC_TYPE_IS_CALLABLE: {
            CcTypeKind k = ccqt_kind(qt);
            *(_Bool*)result = k == CC_FUNCTION || (k == CC_POINTER && ccqt_kind(ccqt_as_ptr(qt)->pointee) == CC_FUNCTION);
            return 0;
        }
        case CC_TYPE_IS_INCOMPLETE: {
            CcTypeKind k = ccqt_kind(qt);
            switch(k){
                DRP_CASES_EXHAUSTED;
                case CC_STRUCT:
                    *(_Bool*)result = ccqt_as_struct(qt)->is_incomplete;
                    break;
                case CC_UNION:
                    *(_Bool*)result = ccqt_as_union(qt)->is_incomplete;
                    break;
                case CC_ARRAY:
                    *(_Bool*)result = ccqt_as_array(qt)->is_incomplete;
                    break;
                case CC_ENUM:
                    *(_Bool*)result = ccqt_as_enum(qt)->is_incomplete;
                    break;
                case CC_FUNCTION:
                case CC_BASIC:
                case CC_POINTER:
                case CC_BLOCK_POINTER:
                case CC_SLICE:
                    *(_Bool*)result = 0;
            }
            return 0;
        }
        case CC_TYPE_IS_VARIADIC: {
            CcQualType ft = qt;
            if(ccqt_kind(ft) == CC_POINTER) ft = ccqt_as_ptr(ft)->pointee;
            *(_Bool*)result = ccqt_kind(ft) == CC_FUNCTION && ccqt_as_function(ft)->is_variadic;
            return 0;
        }
        case CC_TYPE_UNQUAL: {
            CcQualType uq = qt;
            uq.quals = 0;
            *(uintptr_t*)result = uq.bits;
            return 0;
        }
        case CC_TYPE_COUNT: {
            if(ccqt_kind(qt) != CC_ARRAY)
                return ci_error(ci, loc, "_Type.count: not an array type");
            *(size_t*)result = ccqt_as_array(qt)->length;
            return 0;
        }
        case CC_TYPE_IS_CALLABLE_WITH: {
            uintptr_t arg_bits = arg;
            CcQualType arg_type = {.bits = arg_bits};
            CcQualType ft = qt;
            if(ccqt_kind(ft) == CC_POINTER) ft = ccqt_as_ptr(ft)->pointee;
            _Bool v = 0;
            if(ccqt_kind(ft) == CC_FUNCTION){
                CcFunction* f = ccqt_as_function(ft);
                if(f->param_count == 1)
                    v = cc_implicit_convertible(&ci->parser, arg_type, f->params[0]);
            }
            *(_Bool*)result = v;
            return 0;
        }
        case CC_TYPE_CASTABLE_TO: {
            uintptr_t arg_bits = arg;
            CcQualType target = {.bits = arg_bits};
            *(_Bool*)result = cc_explicit_castable(&ci->parser, qt, target);
            return 0;
        }
        case CC_TYPE_MAKE_ANY:{
            CiRtAny* any = result;
            memset(any, 0, sizeof *any);
            uint32_t sz;
            err = cc_sizeof_as_uint(&ci->parser, qt, loc, &sz);
            if(err) return err;
            if(sz > sizeof any->payload){
                return ci_error(ci, loc, "Type doesn't fit in an _Any");
            }
            if(sz && !arg){
                return ci_error(ci, loc, "Null pointer as arg to make_any");
            }
            any->type.unqual = qt.unqual;
            if(sz) memcpy(any->payload, (const void*)arg, sz);
            return 0;
        }
        case CC_TYPE_FIELD:{
            CcTypeKind k = ccqt_kind(qt);
            if(k != CC_STRUCT && k != CC_UNION)
                return ci_error(ci, loc, "_Type.field: not a struct or union type");
            uintptr_t idx = arg;
            CcField* f;
            if(k == CC_STRUCT){
                CcStruct* s = ccqt_as_struct(qt);
                if(idx >= s->field_count)
                    return ci_error(ci, loc, "_Type.field: index out of range");
                f = &s->fields[idx];
            }
            else {
                CcUnion* s = ccqt_as_union(qt);
                if(idx >= s->field_count)
                    return ci_error(ci, loc, "_Type.field: index out of range");
                f = &s->fields[idx];
            }
            CiRtField* out = (CiRtField*)result;
            if(f->is_method){
                *out = (CiRtField){
                    .type = f->type,
                    .name = f->method->name ? f->method->name->data : "",
                    .name_length = f->method->name ? f->method->name->length : 0,
                    .offset = 0,
                    .bitwidth = 0,
                    .bitoffset = 0,
                    .is_bitfield = 0,
                };
            }
            else {
                *out = (CiRtField){
                    .type = f->type,
                    .name = f->name ? f->name->data : "",
                    .name_length = f->name ? f->name->length : 0,
                    .offset = f->offset,
                    .bitwidth = f->bitwidth,
                    .bitoffset = f->bitoffset,
                    .is_bitfield = f->is_bitfield,
                };
            }
            return 0;
        }
        case CC_TYPE_PUSH_METHOD:
            return ci_error(ci, loc, "push_method should be handled at parse time");
        case CC_TYPE_ENUMERATORS: {
            if(ccqt_kind(qt) != CC_ENUM)
                return ci_error(ci, loc, "_Type.enumerators: not an enum type");
            CcEnum* en = ccqt_as_enum(qt);
            *(size_t*)result = en->enumerator_count;
            return 0;
        }
        case CC_TYPE_ENUMERATOR: {
            if(ccqt_kind(qt) != CC_ENUM)
                return ci_error(ci, loc, "_Type.enumerator: not an enum type");
            CcEnum* enum_ = ccqt_as_enum(qt);
            uintptr_t idx = arg;
            if(idx >= enum_->enumerator_count)
                return ci_error(ci, loc, "_Type.enumerator: index out of range");
            CcEnumerator* enumerator = enum_->enumerators[idx];
            CiRtEnumerator* out = (CiRtEnumerator*)result;
            out->name = enumerator->name ? enumerator->name->data : "";
            out->name_length = enumerator->name ? enumerator->name->length: 0;
            out->value = (long long)enumerator->value;
            return 0;
        }
        case CC_TYPE_RETURN_TYPE: {
            CcQualType ft = qt;
            if(ccqt_kind(ft) == CC_POINTER) ft = ccqt_as_ptr(ft)->pointee;
            if(ccqt_kind(ft) != CC_FUNCTION)
                return ci_error(ci, loc, "_Type.return_type: not a function type");
            *(uintptr_t*)result = ccqt_as_function(ft)->return_type.bits;
            return 0;
        }
        case CC_TYPE_PARAM_COUNT: {
            CcQualType ft = qt;
            if(ccqt_kind(ft) == CC_POINTER) ft = ccqt_as_ptr(ft)->pointee;
            if(ccqt_kind(ft) != CC_FUNCTION)
                return ci_error(ci, loc, "_Type.param_count: not a function type");
            *(size_t*)result = ccqt_as_function(ft)->param_count;
            return 0;
        }
        case CC_TYPE_PARAM_TYPE: {
            CcQualType ft = qt;
            if(ccqt_kind(ft) == CC_POINTER) ft = ccqt_as_ptr(ft)->pointee;
            if(ccqt_kind(ft) != CC_FUNCTION)
                return ci_error(ci, loc, "_Type.param_type: not a function type");
            CcFunction* f = ccqt_as_function(ft);
            uintptr_t idx = arg;
            if(idx >= f->param_count)
                return ci_error(ci, loc, "_Type.param_type: index out of range");
            *(uintptr_t*)result = f->params[idx].bits;
            return 0;
        }
        case CC_TYPE_ELEMENT_TYPE: {
            CcTypeKind k = ccqt_kind(qt);
            if(k == CC_ARRAY){
                *(uintptr_t*)result = ccqt_as_array(qt)->element.bits;
                return 0;
            }
            if(k == CC_SLICE){
                *(uintptr_t*)result = ccqt_as_slice(qt)->pointee.bits;
                return 0;
            }
            return ci_error(ci, loc, "_Type.element_type: not an array, slice or vector type");
        }
        case CC_TYPE_UNDERLYING_TYPE: {
            if(ccqt_kind(qt) != CC_ENUM)
                return ci_error(ci, loc, "_Type.underlying_type: not an enum type");
            *(uintptr_t*)result = ccqt_as_enum(qt)->underlying.bits;
            return 0;
        }
        case CC_TYPE_FIELDS:{
            CcTypeKind k = ccqt_kind(qt);
            if(k != CC_STRUCT && k != CC_UNION)
                return ci_error(ci, loc, "_Type.fields: not a struct or union type");
            CcStruct* s = ccqt_as_struct(qt);
            *(size_t*)result = s->field_count;
            return 0;
        }
    }
    return ci_error(ci, loc, "interpreter: unsupported type introspection op");
}

static
int
ci_module_reflect_validate(CiInterpreter* ci, SrcLoc loc, CiModule*_Null_unspecified module){
    if(module && PM_get(&ci->modules, module) != module)
        return ci_error(ci, loc, "_Module is not valid");
    return 0;
}

static
int
ci_module_reflect(CiInterpreter* ci, CiInterpFrame* frame, SrcLoc loc, CcModuleOp op, CiModule*_Null_unspecified module, uintptr_t arg, CcQualType expected, void* result, size_t size, CiInterpFrame*_Nullable*_Nonnull child){
    int err;
    size_t idx = arg;
    switch(op){
        case CC_MODULE_FUNC:
        case CC_MODULE_VAR:
        case CC_MODULE_TYPE:
            break;
        case CC_MODULE_FUNC_COUNT:
        case CC_MODULE_VAR_COUNT:
        case CC_MODULE_TYPE_COUNT:
        case CC_MODULE_NONE:
            break;
        case CC_MODULE_SYMBOL:{
            const char* name = (const char*)arg;
            if(!name)
                return ci_error(ci, loc, "_Module.symbol name must not be NULL");
            void* sym = NULL;
            err = ci_lookup_symbol(ci, loc, module, name, expected, &sym);
            if(err) return err;
            if(sizeof sym > size)
                return CI_RESULT_TOO_SMALL(ci, loc, sizeof sym, size);
            CI_INLINE_MEMCPY(result, &sym, sizeof sym);
            return 0;
        }
        case CC_MODULE_RUN:{
            err = ci_resolve_module(ci, module);
            if(err) return err;
            err = ci_lower_module(ci, module);
            if(err) return err;
            int ret = 0;
            if(result != ci_discard_buf && sizeof ret > size)
                return CI_RESULT_TOO_SMALL(ci, loc, sizeof ret, size);
            CiInterpFrame* module_frame = Allocator_zalloc(ci_allocator(ci), sizeof *module_frame + module->slot_size);
            if(!module_frame) return CI_OOM_ERROR;
            *module_frame = (CiInterpFrame){
                .parent = frame,
                .ops = module->ops.data,
                .op_count = module->ops.count,
                .slots = module_frame + 1,
                .data_length = module->slot_size,
                .return_buf = ci_discard_buf,
                .return_size = sizeof ci_discard_buf,
            };
            if(result != ci_discard_buf) CI_INLINE_MEMCPY(result, &ret, sizeof ret);
            *child = module_frame;
            return CI_STEP_ENTER_FRAME;
        }
        case CC_MODULE_PARSE_TYPE:{
            const char* name = (const char*)arg;
            if(!name)
                return ci_error(ci, loc, "_Module.parse_type name must not be NULL");
            CcQualType type = {0};
            err = ci_parse_module_type(ci, loc, module, name, &type);
            if(err) return err;
            uintptr_t bits = type.bits;
            if(sizeof bits > size)
                return CI_RESULT_TOO_SMALL(ci, loc, sizeof bits, size);
            CI_INLINE_MEMCPY(result, &bits, sizeof bits);
            return 0;
        }
    }
    CiRtModuleMember member = {0};
    err = ci_reflect_module(ci, loc, module, op, idx, &member);
    if(err) return err;
    switch(op){
        case CC_MODULE_FUNC_COUNT:
        case CC_MODULE_VAR_COUNT:
        case CC_MODULE_TYPE_COUNT:
            if(sizeof member.name_length > size)
                return CI_RESULT_TOO_SMALL(ci, loc, sizeof member.name_length, size);
            memcpy(result, &member.name_length, sizeof member.name_length);
            return 0;
        case CC_MODULE_FUNC:
        case CC_MODULE_VAR:
        case CC_MODULE_TYPE:
            if(sizeof member > size)
                return CI_RESULT_TOO_SMALL(ci, loc, sizeof member, size);
            CI_INLINE_MEMCPY(result, &member, sizeof member);
            return 0;
        case CC_MODULE_NONE:
        default:
            return CI_UNREACHABLE_ERROR;
    }
    return CI_UNREACHABLE_ERROR;
}

force_inline
int
_ci_interp_step(CiInterpreter* ci, CiInterpFrame* frame, CiInterpFrame*_Nullable*_Nonnull child){
    if(frame->pc >= frame->op_count)
        return 0;
    const CiOp* op = &frame->ops[frame->pc];
    switch(op->kind){
        case CI_OP_RT_CALL: {
            void* result = op->rt_call.slot_size
                ? (char*)frame->slots + op->rt_call.slot
                : ci_discard_buf;
            int err = 0;
            switch(op->rt_call.op){
                case CI_RT_TYPE_VALIDATE:
                case CI_RT_MODULE_VALIDATE:
                case CI_RT_TYPE_REFLECT:
                case CI_RT_MODULE_REFLECT: {
                    uintptr_t receiver, arg = 0;
                    CI_INLINE_MEMCPY(&receiver, (char*)frame->slots + op->rt_call.args[0], sizeof receiver);
                    if(op->rt_call.nargs >= 2)
                        CI_INLINE_MEMCPY(&arg, (char*)frame->slots + op->rt_call.args[1], sizeof arg);
                    CcQualType expected = {0};
                    if(op->rt_call.nargs == 3)
                        CI_INLINE_MEMCPY(&expected.bits, (char*)frame->slots + op->rt_call.args[2], sizeof expected.bits);
                    CcQualType qt = {.bits = receiver};
                    if(op->rt_call.op == CI_RT_TYPE_VALIDATE)
                        err = ci_type_reflect_validate(ci, op->rt_call.loc, op->rt_call.reflect_op, qt);
                    else if(op->rt_call.op == CI_RT_MODULE_VALIDATE)
                        err = ci_module_reflect_validate(ci, op->rt_call.loc, (CiModule*)receiver);
                    else if(op->rt_call.op == CI_RT_TYPE_REFLECT)
                        err = ci_type_reflect(ci, op->rt_call.loc, op->rt_call.reflect_op, qt, arg, result);
                    else
                        err = ci_module_reflect(ci, frame, op->rt_call.loc, op->rt_call.reflect_op, (CiModule*)receiver, arg, expected, result, op->rt_call.slot_size ? op->rt_call.slot_size : sizeof ci_discard_buf, child);
                    break;
                }
                case CI_RT_INTERN: {
                    const char* s;
                    CI_INLINE_MEMCPY(&s, (char*)frame->slots + op->rt_call.args[0], sizeof s);
                    const char* interned = NULL;
                    if(s){
                        Atom a;
                        AtomTable* at = ci_lock_atoms(ci);
                        a = AT_atomize(at, s, strlen(s));
                        ci_unlock_atoms(ci, at);
                        if(!a) return CI_OOM_ERROR;
                        interned = a->data;
                    }
                    if(op->rt_call.slot_size)
                        CI_INLINE_MEMCPY(result, &interned, sizeof interned);
                    break;
                }
                case CI_RT_HOTSWAP: {
                    void (*old_ptr)(void), (*new_ptr)(void);
                    CI_INLINE_MEMCPY(&old_ptr, (char*)frame->slots + op->rt_call.args[0], sizeof old_ptr);
                    CI_INLINE_MEMCPY(&new_ptr, (char*)frame->slots + op->rt_call.args[1], sizeof new_ptr);
                    int ret = 1;
                    CcFunc* old_func = BPM_rget(&ci->closure_map, (void*)old_ptr);
                    CcFunc* new_func = BPM_rget(&ci->closure_map, (void*)new_ptr);
                    if(old_func == new_func) ret = 0;
                    else if(old_func && new_func && old_func->type == new_func->type){
                        drp_atomic_ptr_store(&old_func->hotswap, new_func);
                        ret = 0;
                    }
                    if(op->rt_call.slot_size)
                        CI_INLINE_MEMCPY(result, &ret, sizeof ret);
                    break;
                }
                case CI_RT_COMPILE: {
                    const char* source;
                    CI_INLINE_MEMCPY(&source, (char*)frame->slots + op->rt_call.args[0], sizeof source);
                    CiModule* module = NULL;
                    if(source){
                        err = ci_compile_module(ci, source, &module);
                        if(err == CI_OOM_ERROR) return err;
                        // Compilation diagnostics produce a null module, not
                        // an interpreter execution failure.
                        err = 0;
                    }
                    if(op->rt_call.slot_size)
                        CI_INLINE_MEMCPY(result, &module, sizeof module);
                    break;
                }
            }
            if(err != CI_STEP_ENTER_FRAME) frame->pc++;
            return err;
        }
        case CI_OP_CONST: {
            ci_copy((char*)frame->slots + op->constant.slot, op->constant.immediate, op->constant.immsize);
            frame->pc++;
            return 0;
        }
        case CI_OP_COPY: {
            ci_copy((char*)frame->slots + op->copy.slot, (char*)frame->slots + op->copy.src, op->copy.slot_size);
            frame->pc++;
            return 0;
        }
        case CI_OP_INDEX: {
            const void* src = (char*)frame->slots + op->index.index;
            if(op->index.ptr_size == 8 && (op->index.index_size == 4 || op->index.index_size == 8)){
                uint64_t index;
                if(op->index.index_size == 8)
                    index = ci_read_uint(src, 8);
                else if(op->index.index_unsigned)
                    index = ci_read_uint(src, 4);
                else
                    index = (uint64_t)ci_read_int(src, 4);
                uint64_t base = ci_read_uint((char*)frame->slots + op->index.base, 8);
                ci_write_uint((char*)frame->slots + op->index.slot, 8, base + index * op->index.scale);
            }
            else {
                uint64_t index = op->index.index_unsigned ? ci_read_uint(src, op->index.index_size) : (uint64_t)ci_read_int(src, op->index.index_size);
                uint64_t base = ci_read_uint((char*)frame->slots + op->index.base, op->index.ptr_size);
                ci_write_uint((char*)frame->slots + op->index.slot, op->index.ptr_size, base + index * op->index.scale);
            }
            frame->pc++;
            return 0;
        }
        case CI_OP_CMP_JUMP32:
        case CI_OP_CMP32: {
            const void* s1 = (char*)frame->slots + op->cmp.src;
            const void* s2 = (char*)frame->slots + op->cmp.src2;
            _Bool is_unsigned = op->cmp.is_unsigned;
            uint32_t lu, ru;
            if(is_unsigned){
                lu = (uint32_t)ci_read_uint(s1, 4);
                ru = (uint32_t)ci_read_uint(s2, 4);
            }
            else {
                lu = (uint32_t)ci_read_int(s1, 4);
                ru = (uint32_t)ci_read_int(s2, 4);
            }
            uint64_t res;
            switch((CiCmpOp)(op->cmp.op)){
                case CI_CMP_EQ: res = lu == ru; break;
                case CI_CMP_NE: res = lu != ru; break;
                case CI_CMP_LT: res = is_unsigned ? (lu < ru) : ((int32_t)lu < (int32_t)ru); break;
                case CI_CMP_GT: res = is_unsigned ? (lu > ru) : ((int32_t)lu > (int32_t)ru); break;
                case CI_CMP_LE: res = is_unsigned ? (lu <= ru) : ((int32_t)lu <= (int32_t)ru); break;
                case CI_CMP_GE: res = is_unsigned ? (lu >= ru) : ((int32_t)lu >= (int32_t)ru); break;
                DRP_CASES_EXHAUSTED;
            }
            if(op->kind == CI_OP_CMP_JUMP32){
                frame->pc = ((res != 0) == op->cmp_jump.when_true) ? op->cmp_jump.jump : frame->pc + 1;
            }
            else {
                ci_write_uint((char*)frame->slots + op->cmp.slot, op->cmp.slot_size, res);
                frame->pc++;
            }
            return 0;
        }
        case CI_OP_CMP_JUMP64:
        case CI_OP_CMP64: {
            const void* s1 = (char*)frame->slots + op->cmp.src;
            const void* s2 = (char*)frame->slots + op->cmp.src2;
            _Bool is_unsigned = op->cmp.is_unsigned;
            uint64_t lu, ru;
            if(is_unsigned){
                lu = ci_read_uint(s1, 8);
                ru = ci_read_uint(s2, 8);
            }
            else {
                lu = (uint64_t)ci_read_int(s1, 8);
                ru = (uint64_t)ci_read_int(s2, 8);
            }
            uint64_t res;
            switch((CiCmpOp)(op->cmp.op)){
                case CI_CMP_EQ: res = lu == ru; break;
                case CI_CMP_NE: res = lu != ru; break;
                case CI_CMP_LT: res = is_unsigned ? (lu < ru) : ((int64_t)lu < (int64_t)ru); break;
                case CI_CMP_GT: res = is_unsigned ? (lu > ru) : ((int64_t)lu > (int64_t)ru); break;
                case CI_CMP_LE: res = is_unsigned ? (lu <= ru) : ((int64_t)lu <= (int64_t)ru); break;
                case CI_CMP_GE: res = is_unsigned ? (lu >= ru) : ((int64_t)lu >= (int64_t)ru); break;
                DRP_CASES_EXHAUSTED;
            }
            if(op->kind == CI_OP_CMP_JUMP64){
                frame->pc = ((res != 0) == op->cmp_jump.when_true) ? op->cmp_jump.jump : frame->pc + 1;
            }
            else {
                ci_write_uint((char*)frame->slots + op->cmp.slot, op->cmp.slot_size, res);
                frame->pc++;
            }
            return 0;
        }
        case CI_OP_CMP128: {
            const void* s1 = (char*)frame->slots + op->cmp.src;
            const void* s2 = (char*)frame->slots + op->cmp.src2;
            _Bool is_unsigned = op->cmp.is_unsigned;
            CiUint128 lu, ru;
            ci_uint128_read(&lu, s1, 16);
            ci_uint128_read(&ru, s2, 16);
            uint64_t res;
            switch((CiCmpOp)(op->cmp.op)){
                case CI_CMP_EQ: res = ci_uint128_eq(lu, ru); break;
                case CI_CMP_NE: res = ci_uint128_ne(lu, ru); break;
                case CI_CMP_LT:
                    res = is_unsigned ? ci_uint128_lt(lu, ru) : ci_int128_lt(ci_int128_from_uint128(lu), ci_int128_from_uint128(ru));
                    break;
                case CI_CMP_GT:
                    res = is_unsigned ? ci_uint128_gt(lu, ru) : ci_int128_gt(ci_int128_from_uint128(lu), ci_int128_from_uint128(ru));
                    break;
                case CI_CMP_LE:
                    res = is_unsigned ? ci_uint128_le(lu, ru) : ci_int128_le(ci_int128_from_uint128(lu), ci_int128_from_uint128(ru));
                    break;
                case CI_CMP_GE:
                    res = is_unsigned ? ci_uint128_ge(lu, ru) : ci_int128_ge(ci_int128_from_uint128(lu), ci_int128_from_uint128(ru));
                    break;
                DRP_CASES_EXHAUSTED;
            }
            ci_write_uint((char*)frame->slots + op->cmp.slot, op->cmp.slot_size, res);
            frame->pc++;
            return 0;
        }
        case CI_OP_ALU8: {
            const void* s1 = (char*)frame->slots + op->alu.src;
            const void* s2 = (char*)frame->slots + op->alu.src2;
            _Bool is_unsigned = op->alu.is_unsigned;
            uint32_t lu, ru;
            if(is_unsigned){
                lu = (uint32_t)ci_read_uint(s1, 1);
                ru = (uint32_t)ci_read_uint(s2, 1);
            }
            else {
                lu = (uint32_t)ci_read_int(s1, 1);
                ru = (uint32_t)ci_read_int(s2, 1);
            }
            uint64_t res;
            switch((CiAluOp)(op->alu.op)){
                case CI_ALU_ADD: res = lu + ru; break;
                case CI_ALU_SUB: res = lu - ru; break;
                case CI_ALU_MUL: res = lu * ru; break;
                case CI_ALU_DIV:
                    if(is_unsigned)
                        res = ru ? lu / ru : 0;
                    else
                        res = ru ? (uint8_t)((int8_t)lu / (int8_t)ru) : 0;
                    break;
                case CI_ALU_MOD:
                    if(is_unsigned)
                        res = ru ? lu % ru : 0;
                    else
                        res = ru ? (uint8_t)((int8_t)lu % (int8_t)ru) : 0;
                    break;
                case CI_ALU_AND: res = lu & ru; break;
                case CI_ALU_OR:  res = lu | ru; break;
                case CI_ALU_XOR: res = lu ^ ru; break;
                case CI_ALU_SHL: res = lu << ru; break;
                case CI_ALU_SHR:
                    if(is_unsigned)
                        res = lu >> ru;
                    else
                        res = (uint8_t)((int8_t)lu >> ru);
                    break;
                case CI_ALU_NEG: res = (uint8_t)-(int8_t)lu; break;
                case CI_ALU_NOT: res = (uint8_t)~lu; break;
                DRP_CASES_EXHAUSTED;
            }
            ci_write_uint((char*)frame->slots + op->alu.slot, 1, res);
            frame->pc++;
            return 0;
        }
        case CI_OP_ALU16: {
            const void* s1 = (char*)frame->slots + op->alu.src;
            const void* s2 = (char*)frame->slots + op->alu.src2;
            _Bool is_unsigned = op->alu.is_unsigned;
            uint32_t lu, ru;
            if(is_unsigned){
                lu = (uint32_t)ci_read_uint(s1, 2);
                ru = (uint32_t)ci_read_uint(s2, 2);
            }
            else {
                lu = (uint32_t)ci_read_int(s1, 2);
                ru = (uint32_t)ci_read_int(s2, 2);
            }
            uint64_t res;
            switch((CiAluOp)(op->alu.op)){
                case CI_ALU_ADD: res = lu + ru; break;
                case CI_ALU_SUB: res = lu - ru; break;
                case CI_ALU_MUL: res = lu * ru; break;
                case CI_ALU_DIV:
                    if(is_unsigned)
                        res = ru ? lu / ru : 0;
                    else
                        res = ru ? (uint16_t)((int16_t)lu / (int16_t)ru) : 0;
                    break;
                case CI_ALU_MOD:
                    if(is_unsigned)
                        res = ru ? lu % ru : 0;
                    else
                        res = ru ? (uint16_t)((int16_t)lu % (int16_t)ru) : 0;
                    break;
                case CI_ALU_AND: res = lu & ru; break;
                case CI_ALU_OR:  res = lu | ru; break;
                case CI_ALU_XOR: res = lu ^ ru; break;
                case CI_ALU_SHL: res = lu << ru; break;
                case CI_ALU_SHR:
                    if(is_unsigned)
                        res = lu >> ru;
                    else
                        res = (uint16_t)((int16_t)lu >> ru);
                    break;
                case CI_ALU_NEG: res = (uint16_t)-(int16_t)lu; break;
                case CI_ALU_NOT: res = (uint16_t)~lu; break;
                DRP_CASES_EXHAUSTED;
            }
            ci_write_uint((char*)frame->slots + op->alu.slot, 2, res);
            frame->pc++;
            return 0;
        }
        case CI_OP_ALU32: {
            _Bool is_unsigned;
            uint32_t lu, ru;
            is_unsigned = op->alu.is_unsigned;
            lu = (uint32_t)ci_read_uint((char*)frame->slots + op->alu.src, 4);
            ru = (uint32_t)ci_read_uint((char*)frame->slots + op->alu.src2, 4);
            goto alu32;
        case CI_OP_ALU_IMM32:
            is_unsigned = op->alu_imm.is_unsigned;
            lu = (uint32_t)ci_read_uint((char*)frame->slots + op->alu_imm.src, 4);
            ru = (uint32_t)op->alu_imm.immediate;
            alu32:;
            uint64_t res;
            switch((CiAluOp)(op->alu.op)){
                case CI_ALU_ADD: res = lu + ru; break;
                case CI_ALU_SUB: res = lu - ru; break;
                case CI_ALU_MUL: res = lu * ru; break;
                case CI_ALU_DIV:
                    if(is_unsigned)
                        res = ru ? lu / ru : 0;
                    else
                        res = ru ? (uint32_t)((int32_t)lu / (int32_t)ru) : 0;
                    break;
                case CI_ALU_MOD:
                    if(is_unsigned)
                        res = ru ? lu % ru : 0;
                    else
                        res = ru ? (uint32_t)((int32_t)lu % (int32_t)ru) : 0;
                    break;
                case CI_ALU_AND: res = lu & ru; break;
                case CI_ALU_OR:  res = lu | ru; break;
                case CI_ALU_XOR: res = lu ^ ru; break;
                case CI_ALU_SHL: res = lu << ru; break;
                case CI_ALU_SHR:
                    if(is_unsigned)
                        res = lu >> ru;
                    else
                        res = (uint32_t)((int32_t)lu >> ru);
                    break;
                case CI_ALU_NEG: res = (uint32_t)-(int32_t)lu; break;
                case CI_ALU_NOT: res = ~lu; break;
                DRP_CASES_EXHAUSTED;
            }
            ci_write_uint((char*)frame->slots + op->alu.slot, 4, res);
            frame->pc++;
            return 0;
        }
        case CI_OP_ALU64: {
            _Bool is_unsigned;
            uint64_t lu, ru;
            is_unsigned = op->alu.is_unsigned;
            lu = ci_read_uint((char*)frame->slots + op->alu.src, 8);
            ru = ci_read_uint((char*)frame->slots + op->alu.src2, 8);
            goto alu64;
        case CI_OP_ALU_IMM64:
            is_unsigned = op->alu_imm.is_unsigned;
            lu = ci_read_uint((char*)frame->slots + op->alu_imm.src, 8);
            ru = op->alu_imm.immediate;
            alu64:;
            uint64_t res;
            switch((CiAluOp)(op->alu.op)){
                case CI_ALU_ADD: res = lu + ru; break;
                case CI_ALU_SUB: res = lu - ru; break;
                case CI_ALU_MUL: res = lu * ru; break;
                case CI_ALU_DIV:
                    if(is_unsigned)
                        res = ru ? lu / ru : 0;
                    else
                        res = ru ? (uint64_t)((int64_t)lu / (int64_t)ru) : 0;
                    break;
                case CI_ALU_MOD:
                    if(is_unsigned)
                        res = ru ? lu % ru : 0;
                    else
                        res = ru ? (uint64_t)((int64_t)lu % (int64_t)ru) : 0;
                    break;
                case CI_ALU_AND: res = lu & ru; break;
                case CI_ALU_OR:  res = lu | ru; break;
                case CI_ALU_XOR: res = lu ^ ru; break;
                case CI_ALU_SHL: res = lu << ru; break;
                case CI_ALU_SHR:
                    if(is_unsigned)
                        res = lu >> ru;
                    else
                        res = (uint64_t)((int64_t)lu >> ru);
                    break;
                case CI_ALU_NEG: res = (uint64_t)-(int64_t)lu; break;
                case CI_ALU_NOT: res = ~lu; break;
                DRP_CASES_EXHAUSTED;
            }
            ci_write_uint((char*)frame->slots + op->alu.slot, 8, res);
            frame->pc++;
            return 0;
        }
        case CI_OP_ALU128: {
            const void* s1 = (char*)frame->slots + op->alu.src;
            const void* s2 = (char*)frame->slots + op->alu.src2;
            _Bool is_unsigned = op->alu.is_unsigned;
            CiUint128 lu, ru;
            ci_uint128_read(&lu, s1, 16);
            ci_uint128_read(&ru, s2, 16);
            CiUint128 res;
            switch((CiAluOp)(op->alu.op)){
                case CI_ALU_ADD: res = ci_uint128_add(lu, ru); break;
                case CI_ALU_SUB: res = ci_uint128_sub(lu, ru); break;
                case CI_ALU_MUL: res = ci_uint128_mul(lu, ru); break;
                case CI_ALU_DIV:
                    if(is_unsigned)
                        res = ci_uint128_div(lu, ru);
                    else
                        res = ci_uint128_from_int128(ci_int128_div(ci_int128_from_uint128(lu), ci_int128_from_uint128(ru)));
                    break;
                case CI_ALU_MOD:
                    if(is_unsigned)
                        res = ci_uint128_mod(lu, ru);
                    else
                        res = ci_uint128_from_int128(ci_int128_mod(ci_int128_from_uint128(lu), ci_int128_from_uint128(ru)));
                    break;
                case CI_ALU_AND: res = ci_uint128_and(lu, ru); break;
                case CI_ALU_OR:  res = ci_uint128_or(lu, ru); break;
                case CI_ALU_XOR: res = ci_uint128_xor(lu, ru); break;
                case CI_ALU_SHL: res = ci_uint128_shl(lu, ci_uint128_lo(ru)); break;
                case CI_ALU_SHR:
                    if(is_unsigned)
                        res = ci_uint128_shr(lu, ci_uint128_lo(ru));
                    else
                        res = ci_uint128_from_int128(ci_int128_shr(ci_int128_from_uint128(lu), ci_uint128_lo(ru)));
                    break;
                case CI_ALU_NEG: res = ci_uint128_sub(ci_uint128_from_uint64(0), lu); break;
                case CI_ALU_NOT: res = ci_uint128_xor(lu, ci_uint128_from_int64(-1)); break;
                DRP_CASES_EXHAUSTED;
            }
            ci_uint128_write((char*)frame->slots + op->alu.slot, 16, res);
            frame->pc++;
            return 0;
        }
        case CI_OP_FALU32: {
            float a, b;
            CI_INLINE_MEMCPY(&a, (char*)frame->slots + op->falu32.src, sizeof a);
            CI_INLINE_MEMCPY(&b, (char*)frame->slots + op->falu32.src2, sizeof b);
            void* dest = (char*)frame->slots + op->falu32.slot;
            float res;
            switch(op->falu32.op){
                case CI_FALU_ADD: res = a + b; break;
                case CI_FALU_SUB: res = a - b; break;
                case CI_FALU_MUL: res = a * b; break;
                case CI_FALU_DIV: res = a / b; break;
                case CI_FALU_NEG: res = -a; break;
                DRP_CASES_EXHAUSTED;
            }
            CI_INLINE_MEMCPY(dest, &res, sizeof res);
            frame->pc++;
            return 0;
        }
        case CI_OP_FALU64: {
            double a, b;
            CI_INLINE_MEMCPY(&a, (char*)frame->slots + op->falu64.src, sizeof a);
            CI_INLINE_MEMCPY(&b, (char*)frame->slots + op->falu64.src2, sizeof b);
            void* dest = (char*)frame->slots + op->falu64.slot;
            double res;
            switch(op->falu64.op){
                case CI_FALU_ADD: res = a + b; break;
                case CI_FALU_SUB: res = a - b; break;
                case CI_FALU_MUL: res = a * b; break;
                case CI_FALU_DIV: res = a / b; break;
                case CI_FALU_NEG: res = -a; break;
                DRP_CASES_EXHAUSTED;
            }
            CI_INLINE_MEMCPY(dest, &res, sizeof res);
            frame->pc++;
            return 0;
        }
        case CI_OP_FCMP32: {
            float a, b;
            CI_INLINE_MEMCPY(&a, (char*)frame->slots + op->fcmp32.src, sizeof a);
            CI_INLINE_MEMCPY(&b, (char*)frame->slots + op->fcmp32.src2, sizeof b);
            uint64_t res;
            switch(op->fcmp32.op){
                case CI_CMP_EQ: res = a == b; break;
                case CI_CMP_NE: res = a != b; break;
                case CI_CMP_LT: res = a <  b; break;
                case CI_CMP_GT: res = a >  b; break;
                case CI_CMP_LE: res = a <= b; break;
                case CI_CMP_GE: res = a >= b; break;
                DRP_CASES_EXHAUSTED;
            }
            ci_write_uint((char*)frame->slots + op->fcmp32.slot, op->fcmp32.slot_size, res);
            frame->pc++;
            return 0;
        }
        case CI_OP_FCMP64: {
            double a, b;
            CI_INLINE_MEMCPY(&a, (char*)frame->slots + op->fcmp64.src, sizeof a);
            CI_INLINE_MEMCPY(&b, (char*)frame->slots + op->fcmp64.src2, sizeof b);
            uint64_t res;
            switch(op->fcmp64.op){
                case CI_CMP_EQ: res = a == b; break;
                case CI_CMP_NE: res = a != b; break;
                case CI_CMP_LT: res = a <  b; break;
                case CI_CMP_GT: res = a >  b; break;
                case CI_CMP_LE: res = a <= b; break;
                case CI_CMP_GE: res = a >= b; break;
                DRP_CASES_EXHAUSTED;
            }
            ci_write_uint((char*)frame->slots + op->fcmp64.slot, op->fcmp64.slot_size, res);
            frame->pc++;
            return 0;
        }
        case CI_OP_CHECKED: {
            // __builtin_{add,sub,mul}_overflow: compute in exact 128-bit
            // precision (operands are <= 64-bit), truncate to the destination
            // type, and flag overflow when the exact value doesn't fit.
            const void* s1 = (char*)frame->slots + op->checked.src;
            const void* s2 = (char*)frame->slots + op->checked.src2;
            CiInt128 a = op->checked.src_unsigned
                ? ci_int128_from_uint64(ci_read_uint(s1, op->checked.src_size))
                : ci_int128_from_int64(ci_read_int(s1, op->checked.src_size));
            CiInt128 b = op->checked.src2_unsigned
                ? ci_int128_from_uint64(ci_read_uint(s2, op->checked.src2_size))
                : ci_int128_from_int64(ci_read_int(s2, op->checked.src2_size));
            CiInt128 r;
            switch((CiCheckedOp)op->checked.op){
                case CI_CHK_ADD: r = ci_int128_add(a, b); break;
                case CI_CHK_SUB: r = ci_int128_sub(a, b); break;
                case CI_CHK_MUL: r = ci_int128_mul(a, b); break;
                DRP_CASES_EXHAUSTED;
            }
            uint32_t dsz = op->checked.res_size;
            uint64_t truncated = ci_int128_lo(r);
            ci_write_uint((char*)frame->slots + op->checked.result, dsz, truncated);
            // re-extend the truncated value per the destination signedness and
            // compare to the exact result
            CiInt128 back;
            if(op->checked.res_unsigned){
                if(dsz >= 8)
                    back = ci_int128_from_uint64(truncated);
                else
                    back = ci_int128_from_uint64(truncated & (((uint64_t)1 << (dsz * 8)) - 1));
            }
            else {
                int64_t sval;
                switch(dsz){
                    case 1: sval = (int8_t)truncated; break;
                    case 2: sval = (int16_t)truncated; break;
                    case 4: sval = (int32_t)truncated; break;
                    default: sval = (int64_t)truncated; break;
                }
                back = ci_int128_from_int64(sval);
            }
            _Bool overflowed = !ci_int128_eq(r, back);
            *((char*)frame->slots + op->checked.overflow) = (char)overflowed;
            frame->pc++;
            return 0;
        }
        case CI_OP_BITCOUNT: {
            uint32_t sz = op->bitcount.src_size;
            uint64_t val = ci_read_uint((char*)frame->slots + op->bitcount.src, sz);
            uint64_t count;
            switch((CiBitCountOp)op->bitcount.op){
                case CI_BITCNT_POPCOUNT:
                    count = (uint64_t)popcount_64(val);
                    break;
                case CI_BITCNT_CTZ:
                    // ctz(0) is UB per the spec; return the operand bit width
                    count = val? (uint64_t)ctz_64(val) : (uint64_t)(sz * 8);
                    break;
                case CI_BITCNT_CLZ:
                    // clz counts from the operand width, not 64; clz(0) returns
                    // the operand bit width
                    count = val? (uint64_t)(clz_64(val) - (int)(64 - sz * 8)) : (uint64_t)(sz * 8);
                    break;
                DRP_CASES_EXHAUSTED;
            }
            ci_write_uint((char*)frame->slots + op->bitcount.slot, op->bitcount.slot_size, count);
            frame->pc++;
            return 0;
        }
        case CI_OP_ITOF: {
            const void* src = (char*)frame->slots + op->itof.src;
            void* dest = (char*)frame->slots + op->itof.slot;
            if(op->itof.src_size > 8){
                CiUint128 v;
                ci_uint128_read(&v, src, op->itof.src_size);
                double d = ci_uint128_to_double(v, op->itof.is_unsigned);
                if(op->itof.slot_size == 4){
                    float f = (float)d;
                    CI_INLINE_MEMCPY(dest, &f, sizeof f);
                }
                else {
                    CI_INLINE_MEMCPY(dest, &d, sizeof d);
                }
            }
            else if(op->itof.slot_size == 4){
                float f;
                if(op->itof.is_unsigned)
                    f = (float)ci_read_uint(src, op->itof.src_size);
                else
                    f = (float)ci_read_int(src, op->itof.src_size);
                CI_INLINE_MEMCPY(dest, &f, sizeof f);
            }
            else {
                double d;
                if(op->itof.is_unsigned)
                    d = (double)ci_read_uint(src, op->itof.src_size);
                else
                    d = (double)ci_read_int(src, op->itof.src_size);
                CI_INLINE_MEMCPY(dest, &d, sizeof d);
            }
            frame->pc++;
            return 0;
        }
        case CI_OP_FTOI: {
            const void* src = (char*)frame->slots + op->ftoi.src;
            double d;
            if(op->ftoi.src_size == 4){
                float f;
                CI_INLINE_MEMCPY(&f, src, sizeof f);
                d = (double)f;
            }
            else {
                CI_INLINE_MEMCPY(&d, src, sizeof d);
            }
            void* dest = (char*)frame->slots + op->ftoi.slot;
            if(op->ftoi.slot_size > 8){
                CiUint128 v = ci_uint128_from_double(d, op->ftoi.is_unsigned);
                ci_uint128_write(dest, op->ftoi.slot_size, v);
            }
            else {
                uint64_t v = op->ftoi.is_unsigned ? (uint64_t)d : (uint64_t)(int64_t)d;
                ci_write_uint(dest, op->ftoi.slot_size, v);
            }
            frame->pc++;
            return 0;
        }
        case CI_OP_FTOF: {
            const void* src = (char*)frame->slots + op->ftof.src;
            void* dest = (char*)frame->slots + op->ftof.slot;
            double d;
            if(op->ftof.src_size == 4){
                float f;
                CI_INLINE_MEMCPY(&f, src, sizeof f);
                d = (double)f;
            }
            else {
                CI_INLINE_MEMCPY(&d, src, sizeof d);
            }
            if(op->ftof.slot_size == 4){
                float f = (float)d;
                CI_INLINE_MEMCPY(dest, &f, sizeof f);
            }
            else {
                CI_INLINE_MEMCPY(dest, &d, sizeof d);
            }
            frame->pc++;
            return 0;
        }
        case CI_OP_SLOT_ADDR: {
            void* addr = (char*)frame->slots + op->slot_addr.src;
            CI_INLINE_MEMCPY((char*)frame->slots + op->slot_addr.slot, &addr, sizeof addr);
            frame->pc++;
            return 0;
        }
        case CI_OP_VAR_ADDR: {
            CcVariable* var = op->var_addr.var;
            int err = ci_ensure_var_storage(ci, var);
            if(err) return err;
            CI_INLINE_MEMCPY((char*)frame->slots + op->var_addr.slot, &var->interp_val, sizeof(void*));
            frame->pc++;
            return 0;
        }
        case CI_OP_FUNC_ADDR: {
            CcFunc* func = op->func_addr.func;
            if(!func->native_func)
                return ci_ice(ci, op->loc, "function '%s' not resolved before execution", func->name ? func->name->data : "<unknown>");
            void (*fn)(void) = func->native_func;
            CI_INLINE_MEMCPY((char*)frame->slots + op->func_addr.slot, &fn, sizeof fn);
            frame->pc++;
            return 0;
        }
        case CI_OP_BOUNDS: {
            uint64_t idx = ci_read_uint((char*)frame->slots + op->bounds.src, op->bounds.src_size);
            uint64_t len = ci_read_uint((char*)frame->slots + op->bounds.src2, op->bounds.src2_size);
            _Bool oob = op->bounds.inclusive ? (idx > len) : (idx >= len);
            if(oob){
                const char* close = op->bounds.inclusive ? "]" : ")";
                if(op->bounds.index_signed)
                    return ci_error(ci, op->loc,
                        "array subscript out of bounds: index %lld not in [0, %llu%s",
                        (long long)idx, (unsigned long long)len, close);
                return ci_error(ci, op->loc,
                    "array subscript out of bounds: index %llu not in [0, %llu%s",
                    (unsigned long long)idx, (unsigned long long)len, close);
            }
            frame->pc++;
            return 0;
        }
        case CI_OP_CALL: {
            CiCallDescriptor* d = op->call.descrip;
            if(op->call.is_indirect){
                void* result;
                size_t rsize;
                if(op->call.ret_size){
                    result = (char*)frame->slots + op->call.ret_slot;
                    rsize = op->call.ret_size;
                }
                else {
                    result = ci_discard_buf;
                    rsize = sizeof ci_discard_buf;
                }
                void (*fn)(void);
                CI_INLINE_MEMCPY(&fn, (char*)frame->slots + op->call.argv_slot, sizeof fn);
                void** argv = (void**)((uintptr_t)frame->slots + op->call.argv_slot + 8);
                CcFunc* interp_func = BPM_rget(&ci->closure_map, (void*)fn);
                if(interp_func){
                    int err = ci_make_call_frame(ci, frame, interp_func, argv, d->nargs, d->arg_sizes, result, rsize, op->loc, child);
                    return err ? err : CI_STEP_ENTER_FRAME;
                }
                NativeCallCache* cache;
                if(op->call.is_variadic)
                    cache = PM_get(&ci->ffi_cache, d->expr);
                else
                    cache = PM_get(&ci->ffi_cache, d->func_type);
                if(!cache)
                    return ci_ice(ci, op->loc, "ffi_cache not populated for call type%s", "");
                native_call(cache, fn, argv, result);
                frame->pc++;
                return 0;
            }
            else {
                CcFunc* func = d->func;
                void* result;
                size_t rsize;
                if(op->call.ret_size){
                    result = (char*)frame->slots + op->call.ret_slot;
                    rsize = op->call.ret_size;
                }
                else {
                    result = ci_discard_buf;
                    rsize = sizeof ci_discard_buf;
                }
                void** argv = (void**)((uintptr_t)frame->slots + op->call.argv_slot);
                if(func->defined){
                    int err = ci_make_call_frame(ci, frame, func, argv, d->nargs, d->arg_sizes, result, rsize, op->loc, child);
                    return err ? err : CI_STEP_ENTER_FRAME;
                }
                void (*fn)(void) = func->native_func;
                if(!fn)
                    return ci_ice(ci, op->loc, "function '%s' not resolved before execution", func->name ? func->name->data : "<unknown>");
                NativeCallCache* cache;
                if(op->call.is_variadic)
                    cache = PM_get(&ci->ffi_cache, d->expr);
                else
                    cache = PM_get(&ci->ffi_cache, func->type);
                if(!cache)
                    return ci_ice(ci, op->loc, "ffi_cache not populated for call type%s", "");
                native_call(cache, fn, argv, result);
                frame->pc++;
                return 0;
            }
        }
        case CI_OP_LOAD: {
            char* ptr;
            CI_INLINE_MEMCPY(&ptr, (char*)frame->slots + op->load.src, sizeof ptr);
            ci_copy((char*)frame->slots + op->load.slot, ptr + op->load.offset, op->load.slot_size);
            frame->pc++;
            return 0;
        }
        case CI_OP_LOAD_BITFIELD: {
            char* ptr;
            CI_INLINE_MEMCPY(&ptr, (char*)frame->slots+op->load_bf.src, sizeof ptr);
            uint64_t val = ci_bitfield_read(ptr + op->load_bf.offset, op->load_bf.slot_size, op->load_bf.bit_offset, op->load_bf.bit_width);
            val = ci_bitfield_extend(val, op->load_bf.bit_width, op->load_bf.is_signed);
            memcpy((char*)frame->slots + op->load_bf.slot, &val, op->load_bf.slot_size);
            frame->pc++;
            return 0;
        }
        case CI_OP_STORE_IMM: {
            char* ptr;
            CI_INLINE_MEMCPY(&ptr, (char*)frame->slots + op->store_imm.slot, sizeof ptr);
            ci_copy(ptr + op->store_imm.offset, &op->store_imm.immediate, op->store_imm.size);
            frame->pc++;
            return 0;
        }
        case CI_OP_STORE: {
            char* ptr;
            CI_INLINE_MEMCPY(&ptr, (char*)frame->slots + op->store.slot, sizeof ptr);
            ci_copy(ptr + op->store.offset, (char*)frame->slots + op->store.src, op->store.src_size);
            frame->pc++;
            return 0;
        }
        case CI_OP_MEMCOPY: {
            char* dst;
            char* src;
            CI_INLINE_MEMCPY(&dst, (char*)frame->slots + op->memcopy.slot, sizeof dst);
            CI_INLINE_MEMCPY(&src, (char*)frame->slots + op->memcopy.src, sizeof src);
            memmove(dst + op->memcopy.offset, src + op->memcopy.src_offset, op->memcopy.size);
            frame->pc++;
            return 0;
        }
        case CI_OP_ZERO: {
            char* dst;
            CI_INLINE_MEMCPY(&dst, (char*)frame->slots + op->zero.slot, sizeof dst);
            memset(dst+op->zero.offset, 0, op->zero.size);
            frame->pc++;
            return 0;
        }
        case CI_OP_STORE_BITFIELD: {
            char* ptr;
            CI_INLINE_MEMCPY(&ptr, (char*)frame->slots + op->store_bf.slot, sizeof ptr);
            uint64_t val = 0;
            memcpy(&val, (char*)frame->slots + op->store_bf.src, op->store_bf.src_size);
            ci_bitfield_write(ptr + op->store_bf.offset, op->store_bf.src_size, op->store_bf.bit_offset, op->store_bf.bit_width, val);
            frame->pc++;
            return 0;
        }
        case CI_OP_JUMP:
            frame->pc = op->jump.jump;
            return 0;
        case CI_OP_CONVERT: {
            const void* src = (char*)frame->slots + op->convert.src;
            void* dest = (char*)frame->slots + op->convert.slot;
            if(op->convert.src_size > 8 || op->convert.slot_size > 8){
                // 128-bit path, extending per the source's signedness
                CiUint128 v;
                if(op->convert.src_size > 8)
                    ci_uint128_read(&v, src, op->convert.src_size);
                else if(op->convert.is_unsigned)
                    v = ci_uint128_from_uint64(ci_read_uint(src, op->convert.src_size));
                else
                    v = ci_uint128_from_int64(ci_read_int(src, op->convert.src_size));
                if(op->convert.slot_size <= 8)
                    ci_write_uint(dest, op->convert.slot_size, ci_uint128_lo(v));
                else
                    ci_uint128_write(dest, op->convert.slot_size, v);
                frame->pc++;
                return 0;
            }
            uint64_t v;
            if(op->convert.is_unsigned)
                v = ci_read_uint(src, op->convert.src_size);
            else
                v = (uint64_t)ci_read_int(src, op->convert.src_size);
            ci_write_uint(dest, op->convert.slot_size, v);
            frame->pc++;
            return 0;
        }
        case CI_OP_ISTRUE: {
            const void* src = (char*)frame->slots + op->istrue.src;
            uint32_t float_kind = op->istrue.float_kind;
            _Bool v;
            if(float_kind)
                v = ci_read_float(src, (CcBasicTypeKind)float_kind) != 0.0;
            else if(op->istrue.src_size > 8){
                CiUint128 u;
                ci_uint128_read(&u, src, op->istrue.src_size);
                v = ci_uint128_nonzero(u);
            }
            else
                v = ci_read_uint(src, op->istrue.src_size) != 0;
            v ^= (_Bool)op->istrue.negate;
            ci_write_uint((char*)frame->slots + op->istrue.slot, op->istrue.slot_size, v);
            frame->pc++;
            return 0;
        }
        case CI_OP_JUMP_FALSE: {
            const void* cond = (char*)frame->slots + op->jump_false.slot;
            if(ci_read_uint(cond, op->jump_false.slot_size) == 0)
                frame->pc = op->jump_false.jump;
            else
                frame->pc++;
            return 0;
        }
        case CI_OP_JUMP_TRUE: {
            const void* cond = (char*)frame->slots + op->jump_true.slot;
            if(ci_read_uint(cond, op->jump_true.slot_size) != 0)
                frame->pc = op->jump_true.jump;
            else
                frame->pc++;
            return 0;
        }
        case CI_OP_RETURN: {
            frame->pc = frame->op_count;
            return 0;
        }
        case CI_OP_RETURN_SLOT: {
            if(op->return_slot.src_size > frame->return_size)
                return CI_RESULT_TOO_SMALL(ci, op->loc, op->return_slot.src_size, frame->return_size);
            memcpy(frame->return_buf, (char*)frame->slots + op->return_slot.src, op->return_slot.src_size);
            frame->pc = frame->op_count;
            return 0;
        }
        case CI_OP_SWITCH: {
            const void* src = (char*)frame->slots + op->switch_.slot;
            // Sign-extend or zero-extend to 64 bits;
            // decided at lowering.
            uint64_t val;
            if(op->switch_.is_unsigned)
                val = ci_read_uint(src, op->switch_.slot_size);
            else
                val = (uint64_t)ci_read_int(src, op->switch_.slot_size);
            size_t count = op->switch_.table->count;
            const CcSwitchEntry* table = op->switch_.table->data;
            // Binary search for matching case
            size_t lo = 0, hi = count;
            while(lo < hi){
                size_t mid = lo + (hi - lo) / 2;
                if(table[mid].value < val)
                    lo = mid + 1;
                else if(table[mid].value > val)
                    hi = mid;
                else {
                    frame->pc = table[mid].target;
                    return 0;
                }
            }
            // No match — jump to default or exit
            frame->pc = op->switch_.jump;
            return 0;
        }
        case CI_OP_ATOMIC_LOAD:{
            char *ptr, *dest;
            CI_INLINE_MEMCPY(&ptr, (char*)frame->slots+op->atomic_load.src, sizeof ptr);
            ptr += op->atomic_load.offset;
            dest = (char*)frame->slots + op->atomic_load.slot;
            uint32_t sz = op->atomic_load.slot_size;
            switch(sz){
                // TODO: is this the best way to do an atomic load with msvc?
                #ifdef _MSC_VER
                    #if defined(_M_ARM64) || defined(_M_ARM64EC)
                        case 1:  *(uint8_t*)(dest)  = (uint8_t)__iso_volatile_load8((const volatile __int8*)ptr); __dmb(_ARM64_BARRIER_ISH); break;
                        case 2:  *(uint16_t*)(dest) = (uint16_t)__iso_volatile_load16((const volatile __int16*)ptr); __dmb(_ARM64_BARRIER_ISH); break;
                        case 4:  *(uint32_t*)(dest) = (uint32_t)__iso_volatile_load32((const volatile __int32*)ptr); __dmb(_ARM64_BARRIER_ISH); break;
                        case 8:  *(uint64_t*)(dest) = (uint64_t)__iso_volatile_load64((const volatile __int64*)ptr); __dmb(_ARM64_BARRIER_ISH); break;
                        case 16: {
                            __int64* d = (__int64*)(dest);
                            d[0] = 0; d[1] = 0;
                            _InterlockedCompareExchange128((volatile __int64*)ptr, 0, 0, d);
                            break;
                        }
                        default: return ci_ice(ci, op->loc, "unsupported atomic operand size %u", sz);
                    #else
                        case 1:  *(uint8_t*)(dest)  = *(volatile uint8_t*)ptr; _ReadWriteBarrier(); break;
                        case 2:  *(uint16_t*)(dest) = *(volatile uint16_t*)ptr; _ReadWriteBarrier(); break;
                        case 4:  *(uint32_t*)(dest) = *(volatile uint32_t*)ptr; _ReadWriteBarrier(); break;
                        case 8:  *(uint64_t*)(dest) = *(volatile uint64_t*)ptr; _ReadWriteBarrier(); break;
                        case 16: {
                            __int64* d = (__int64*)(dest);
                            d[0] = 0; d[1] = 0;
                            _InterlockedCompareExchange128((volatile __int64*)ptr, 0, 0, d);
                            break;
                        }
                        default: return ci_ice(ci, op->loc, "unsupported atomic operand size %u", sz);
                    #endif
                #else
                    case 1:  __atomic_load(( uint8_t*)ptr, ( uint8_t*)(dest), __ATOMIC_SEQ_CST); break;
                    case 2:  __atomic_load((uint16_t*)ptr, (uint16_t*)(dest), __ATOMIC_SEQ_CST); break;
                    case 4:  __atomic_load((uint32_t*)ptr, (uint32_t*)(dest), __ATOMIC_SEQ_CST); break;
                    case 8:  __atomic_load((uint64_t*)ptr, (uint64_t*)(dest), __ATOMIC_SEQ_CST); break;
                    case 16: __atomic_load((CiAtomic16*)ptr, (CiAtomic16*)(dest), __ATOMIC_SEQ_CST); break;
                    default: return ci_ice(ci, op->loc, "unsupported atomic operand size %u", sz);
                #endif
            }
            frame->pc++;
            return 0;
        }
        case CI_OP_ATOMIC_STORE:{
            char *src, *dest;
            CI_INLINE_MEMCPY(&dest, (char*)frame->slots + op->atomic_store.slot, sizeof src);
            dest += op->atomic_store.offset;
            src = (char*)frame->slots + op->atomic_store.src;
            uint32_t sz = op->atomic_store.src_size;
            switch(sz){
                // TODO: is this the best way to do an atomic store with msvc?
                #ifdef _MSC_VER
                    #if defined(_M_ARM64) || defined(_M_ARM64EC)
                        case 1:  __dmb(_ARM64_BARRIER_ISH); __iso_volatile_store8((volatile __int8*)dest, *(const __int8*)src); __dmb(_ARM64_BARRIER_ISH); break;
                        case 2:  __dmb(_ARM64_BARRIER_ISH); __iso_volatile_store16((volatile __int16*)dest, *(const __int16*)src); __dmb(_ARM64_BARRIER_ISH); break;
                        case 4:  __dmb(_ARM64_BARRIER_ISH); __iso_volatile_store32((volatile __int32*)dest, *(const __int32*)src); __dmb(_ARM64_BARRIER_ISH); break;
                        case 8:  __dmb(_ARM64_BARRIER_ISH); __iso_volatile_store64((volatile __int64*)dest, *(const __int64*)src); __dmb(_ARM64_BARRIER_ISH); break;
                        case 16: {
                            __int64 _tmp[2];
                            CI_INLINE_MEMCPY(_tmp, src, 16);
                            __int64 _old[2] = {0};
                            while (!_InterlockedCompareExchange128((volatile __int64*)dest, _tmp[1], _tmp[0], _old)) {
                            }
                            break;
                        }
                        default: return ci_ice(ci, op->loc, "unsupported atomic operand size %u", sz);
                    #else
                        case 1:  _InterlockedExchange8((volatile char*)dest, *(const char*)src); break;
                        case 2:  _InterlockedExchange16((volatile short*)dest, *(const short*)src); break;
                        case 4:  _InterlockedExchange((volatile long*)dest, *(const long*)src); break;
                        case 8:  _InterlockedExchange64((volatile long long*)dest, *(const long long*)src); break;
                        case 16: {
                            __int64 _tmp[2];
                            CI_INLINE_MEMCPY(_tmp, src, 16);
                            __int64 _old[2] = {0};
                            while (!_InterlockedCompareExchange128((volatile __int64*)dest, _tmp[1], _tmp[0], _old)) {
                            }
                            break;
                        }
                        default: return ci_ice(ci, op->loc, "unsupported atomic operand size %u", sz);
                    #endif
                #else
                    case 1:  __atomic_store(( uint8_t*)dest, ( uint8_t*)(src), __ATOMIC_SEQ_CST); break;
                    case 2:  __atomic_store((uint16_t*)dest, (uint16_t*)(src), __ATOMIC_SEQ_CST); break;
                    case 4:  __atomic_store((uint32_t*)dest, (uint32_t*)(src), __ATOMIC_SEQ_CST); break;
                    case 8:  __atomic_store((uint64_t*)dest, (uint64_t*)(src), __ATOMIC_SEQ_CST); break;
                    case 16: __atomic_store((CiAtomic16*)dest, (CiAtomic16*)(src), __ATOMIC_SEQ_CST); break;
                    default: return ci_ice(ci, op->loc, "unsupported atomic operand size %u", sz);
                #endif
            }
            frame->pc++;
            return 0;
        }
        case CI_OP_ATOMIC_RMW:{
            // The interpreter runs every order as seq_cst; the encoded order
            // is for the JIT.
            char* ptr;
            CI_INLINE_MEMCPY(&ptr, (char*)frame->slots + op->atomic_rmw.src, sizeof ptr);
            ptr += op->atomic_rmw.offset;
            char* old = (char*)frame->slots + op->atomic_rmw.slot;
            char* val = (char*)frame->slots + op->atomic_rmw.src2;
            uint32_t sz = op->atomic_rmw.slot_size;
            if(op->atomic_rmw.op == CI_ARMW_XCHG){
                switch(sz){
                    #ifdef _MSC_VER
                    case 1:  *(uint8_t*)old  = (uint8_t)_InterlockedExchange8((volatile char*)ptr, *(char*)val); break;
                    case 2:  *(uint16_t*)old = (uint16_t)_InterlockedExchange16((volatile short*)ptr, *(short*)val); break;
                    case 4:  *(uint32_t*)old = (uint32_t)_InterlockedExchange((volatile long*)ptr, *(long*)val); break;
                    case 8:  *(uint64_t*)old = (uint64_t)_InterlockedExchange64((volatile long long*)ptr, *(long long*)val); break;
                    case 16: {
                        __int64 _tmp[2];
                        CI_INLINE_MEMCPY(_tmp, val, 16);
                        __int64 _old[2] = {0};
                        while (!_InterlockedCompareExchange128((volatile __int64*)ptr, _tmp[1], _tmp[0], _old)) {
                        }
                        CI_INLINE_MEMCPY(old, _old, 16);
                        break;
                    }
                    #else
                    case 1:  __atomic_exchange(( uint8_t*)ptr, ( uint8_t*)val, ( uint8_t*)old, __ATOMIC_SEQ_CST); break;
                    case 2:  __atomic_exchange((uint16_t*)ptr, (uint16_t*)val, (uint16_t*)old, __ATOMIC_SEQ_CST); break;
                    case 4:  __atomic_exchange((uint32_t*)ptr, (uint32_t*)val, (uint32_t*)old, __ATOMIC_SEQ_CST); break;
                    case 8:  __atomic_exchange((uint64_t*)ptr, (uint64_t*)val, (uint64_t*)old, __ATOMIC_SEQ_CST); break;
                    case 16: __atomic_exchange((CiAtomic16*)ptr, (CiAtomic16*)val, (CiAtomic16*)old, __ATOMIC_SEQ_CST); break;
                    #endif
                    default: return ci_ice(ci, op->loc, "unsupported atomic operand size %u", sz);
                }
                frame->pc++;
                return 0;
            }
            #ifdef _MSC_VER
            #define CI_ARMW_DISPATCH(f8, f16, f32, f64, neg) \
                switch(sz){ \
                    case 1:  *(uint8_t*)old  = (uint8_t)f8((volatile char*)ptr, neg *(char*)val); break; \
                    case 2:  *(uint16_t*)old = (uint16_t)f16((volatile short*)ptr, neg *(short*)val); break; \
                    case 4:  *(uint32_t*)old = (uint32_t)f32((volatile long*)ptr, neg *(long*)val); break; \
                    case 8:  *(uint64_t*)old = (uint64_t)f64((volatile long long*)ptr, neg *(long long*)val); break; \
                    default: return ci_ice(ci, op->loc, "unsupported atomic operand size %u", sz); \
                }
            switch(op->atomic_rmw.op){
                case CI_ARMW_ADD: CI_ARMW_DISPATCH(_InterlockedExchangeAdd8, _InterlockedExchangeAdd16, _InterlockedExchangeAdd, _InterlockedExchangeAdd64, +); break;
                case CI_ARMW_SUB: CI_ARMW_DISPATCH(_InterlockedExchangeAdd8, _InterlockedExchangeAdd16, _InterlockedExchangeAdd, _InterlockedExchangeAdd64, -); break;
                case CI_ARMW_AND: CI_ARMW_DISPATCH(_InterlockedAnd8, _InterlockedAnd16, _InterlockedAnd, _InterlockedAnd64, +); break;
                case CI_ARMW_OR:  CI_ARMW_DISPATCH(_InterlockedOr8, _InterlockedOr16, _InterlockedOr, _InterlockedOr64, +); break;
                case CI_ARMW_XOR: CI_ARMW_DISPATCH(_InterlockedXor8, _InterlockedXor16, _InterlockedXor, _InterlockedXor64, +); break;
                case CI_ARMW_XCHG: break; // handled above
            }
            #else
            #define CI_ARMW_DISPATCH(fetch) \
                switch(sz){ \
                    case 1:  *( uint8_t*)old = fetch(( uint8_t*)ptr, *( uint8_t*)val, __ATOMIC_SEQ_CST); break; \
                    case 2:  *(uint16_t*)old = fetch((uint16_t*)ptr, *(uint16_t*)val, __ATOMIC_SEQ_CST); break; \
                    case 4:  *(uint32_t*)old = fetch((uint32_t*)ptr, *(uint32_t*)val, __ATOMIC_SEQ_CST); break; \
                    case 8:  *(uint64_t*)old = fetch((uint64_t*)ptr, *(uint64_t*)val, __ATOMIC_SEQ_CST); break; \
                    default: return ci_ice(ci, op->loc, "unsupported atomic operand size %u", sz); \
                }
            switch(op->atomic_rmw.op){
                case CI_ARMW_ADD: CI_ARMW_DISPATCH(__atomic_fetch_add); break;
                case CI_ARMW_SUB: CI_ARMW_DISPATCH(__atomic_fetch_sub); break;
                case CI_ARMW_AND: CI_ARMW_DISPATCH(__atomic_fetch_and); break;
                case CI_ARMW_OR:  CI_ARMW_DISPATCH(__atomic_fetch_or); break;
                case CI_ARMW_XOR: CI_ARMW_DISPATCH(__atomic_fetch_xor); break;
                case CI_ARMW_XCHG: break; // handled above
            }
            #endif
            #undef CI_ARMW_DISPATCH
            frame->pc++;
            return 0;
        }
        case CI_OP_ATOMIC_CAS:{
            char* ptr;
            CI_INLINE_MEMCPY(&ptr, (char*)frame->slots + op->atomic_cas.src, sizeof ptr);
            ptr += op->atomic_cas.offset;
            char* expected = (char*)frame->slots + op->atomic_cas.expected;
            char* desired = (char*)frame->slots + op->atomic_cas.desired;
            uint32_t sz = op->atomic_cas.size;
            _Bool r;
            #ifdef _MSC_VER
            switch(sz){
                case 1: { uint8_t exp = *(uint8_t*)expected;
                          uint8_t o = (uint8_t)_InterlockedCompareExchange8((volatile char*)ptr, *(char*)desired, (char)exp);
                          r = o == exp; *(uint8_t*)expected = o; break; }
                case 2: { uint16_t exp = *(uint16_t*)expected;
                          uint16_t o = (uint16_t)_InterlockedCompareExchange16((volatile short*)ptr, *(short*)desired, (short)exp);
                          r = o == exp; *(uint16_t*)expected = o; break; }
                case 4: { uint32_t exp = *(uint32_t*)expected;
                          uint32_t o = (uint32_t)_InterlockedCompareExchange((volatile long*)ptr, *(long*)desired, (long)exp);
                          r = o == exp; *(uint32_t*)expected = o; break; }
                case 8: { uint64_t exp = *(uint64_t*)expected;
                          uint64_t o = (uint64_t)_InterlockedCompareExchange64((volatile long long*)ptr, *(long long*)desired, (long long)exp);
                          r = o == exp; *(uint64_t*)expected = o; break; }
                case 16: {
                    __int64 _des[2];
                    CI_INLINE_MEMCPY(_des, desired, 16);
                    r = (_Bool)_InterlockedCompareExchange128((volatile __int64*)ptr, _des[1], _des[0], (__int64*)expected);
                    break;
                }
                default: return ci_ice(ci, op->loc, "unsupported atomic operand size %u", sz);
            }
            #else
            _Bool weak = op->atomic_cas.weak;
            if(weak){
                switch(sz){
                    case 1:  r = __atomic_compare_exchange(( uint8_t*)ptr, ( uint8_t*)expected, ( uint8_t*)desired, 1, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST); break;
                    case 2:  r = __atomic_compare_exchange((uint16_t*)ptr, (uint16_t*)expected, (uint16_t*)desired, 1, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST); break;
                    case 4:  r = __atomic_compare_exchange((uint32_t*)ptr, (uint32_t*)expected, (uint32_t*)desired, 1, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST); break;
                    case 8:  r = __atomic_compare_exchange((uint64_t*)ptr, (uint64_t*)expected, (uint64_t*)desired, 1, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST); break;
                    case 16: r = __atomic_compare_exchange((CiAtomic16*)ptr, (CiAtomic16*)expected, (CiAtomic16*)desired, 1, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST); break;
                    default: return ci_ice(ci, op->loc, "unsupported atomic operand size %u", sz);
                }
            }
            else {
                switch(sz){
                    case 1:  r = __atomic_compare_exchange(( uint8_t*)ptr, ( uint8_t*)expected, ( uint8_t*)desired, 0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST); break;
                    case 2:  r = __atomic_compare_exchange((uint16_t*)ptr, (uint16_t*)expected, (uint16_t*)desired, 0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST); break;
                    case 4:  r = __atomic_compare_exchange((uint32_t*)ptr, (uint32_t*)expected, (uint32_t*)desired, 0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST); break;
                    case 8:  r = __atomic_compare_exchange((uint64_t*)ptr, (uint64_t*)expected, (uint64_t*)desired, 0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST); break;
                    case 16: r = __atomic_compare_exchange((CiAtomic16*)ptr, (CiAtomic16*)expected, (CiAtomic16*)desired, 0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST); break;
                    default: return ci_ice(ci, op->loc, "unsupported atomic operand size %u", sz);
                }
            }
            #endif
            *((char*)frame->slots + op->atomic_cas.slot) = r;
            frame->pc++;
            return 0;
        }
        case CI_OP_FENCE:
            if(!op->fence.is_signal){
                #ifdef _MSC_VER
                MemoryBarrier();
                #else
                __atomic_thread_fence(__ATOMIC_SEQ_CST);
                #endif
            }
            frame->pc++;
            return 0;
        case CI_OP_ALLOCA:{
            size_t sz;
            void* dest = (char*)frame->slots + op->alloca.slot;
            CI_INLINE_MEMCPY(&sz, (char*)frame->slots + op->alloca.src, sizeof sz);
            CiAllocaBlock* block = Allocator_zalloc(ci_allocator(ci), sizeof(CiAllocaBlock) + sz);
            if(!block) return CI_OOM_ERROR;
            block->size = sz;
            block->next = frame->alloca_list;
            frame->alloca_list = block;
            void* ptr = block + 1;
            CI_INLINE_MEMCPY(dest, &ptr, sizeof dest);
            frame->pc++;
            return 0;
        }
        case CI_OP_BSWAP:{
            size_t sz = op->bswap.size;
            void* dest = (char*)frame->slots + op->bswap.slot;
            void* src = (char*)frame->slots + op->bswap.src;
            switch(sz){
                case 2: *(uint16_t*)dest = bswap16(*(uint16_t*)src); break;
                case 4: *(uint32_t*)dest = bswap32(*(uint32_t*)src); break;
                case 8: *(uint64_t*)dest = bswap64(*(uint64_t*)src); break;
                default:
                    return ci_unreachable(ci, op->loc, "bswap other than 2,4,8 bytes");
            }
            frame->pc++;
            return 0;
        }
        case CI_OP_BUILTIN:{
            switch(op->builtin.op){
                case CC_BUILTIN_UNREACHABLE:
                    return ci_error(ci, op->loc, "__builtin_unreachable reached");
                case CC_BUILTIN_TRAP:
                    return ci_error(ci, op->loc, "__builtin_trap");
                case CC_BUILTIN_DEBUGTRAP:
                    frame->pc++;
                    return 0;
                case CC_BUILTIN_ABORT:
                    return ci_error(ci, op->loc, "__builtin_abort called");
                case CC_BUILTIN_BACKTRACE:
                    ci_backtrace(ci, frame, 0);
                    frame->pc++;
                    return 0;
                DRP_CASES_EXHAUSTED;
            }
        }
        case CI_OP_VA_START:{
            void* ap_ptr;
            CI_INLINE_MEMCPY(&ap_ptr, (char*)frame->slots + op->va_start_.slot, sizeof ap_ptr);
            if(!frame->varargs_buf)
                return ci_error(ci, op->loc, "va_start used in non-variadic function");
            switch(op->va_start_.target){
            case CC_TARGET_AARCH64_MACOS:
            case CC_TARGET_X86_64_WINDOWS:
            case CC_TARGET_TEST: {
                void* va_ptr = frame->varargs_buf;
                CI_INLINE_MEMCPY(ap_ptr, &va_ptr, sizeof(void*));
                frame->pc++;
                return 0;
            }
            case CC_TARGET_AARCH64_LINUX: {
                CiAapcs64VaList* va = ap_ptr;
                va->__stack = frame->varargs_buf;
                va->__gr_top = NULL;
                va->__vr_top = NULL;
                va->__gr_offs = 0;
                va->__vr_offs = 0;
                frame->pc++;
                return 0;
            }
            case CC_TARGET_X86_64_LINUX:
            case CC_TARGET_X86_64_MACOS: {
                CiSysvVaListTag* tag = ap_ptr;
                tag->gp_offset = 48;
                tag->fp_offset = 48 + 128;
                tag->overflow_arg_area = frame->varargs_buf;
                tag->reg_save_area = NULL;
                frame->pc++;
                return 0;
            }
            case CC_TARGET_COUNT:
                break;
            }
            return ci_error(ci, op->loc, "va_start: unsupported target");
        }
        case CI_OP_VA_ARG:{
            void* ap_ptr;
            CI_INLINE_MEMCPY(&ap_ptr, (char*)frame->slots + op->va_arg_.src, sizeof ap_ptr);
            uint32_t sz = op->va_arg_.slot_size;
            uint32_t advance = sz < 8 ? 8 : (sz + 7) & ~7u;
            void* dest = (char*)frame->slots + op->va_arg_.slot;
            switch(op->va_arg_.target){
            case CC_TARGET_AARCH64_MACOS:
            case CC_TARGET_X86_64_WINDOWS:
            case CC_TARGET_TEST: {
                void* cur;
                CI_INLINE_MEMCPY(&cur, ap_ptr, sizeof(void*));
                memcpy(dest, cur, sz);
                cur = (char*)cur + advance;
                CI_INLINE_MEMCPY(ap_ptr, &cur, sizeof(void*));
                frame->pc++;
                return 0;
            }
            case CC_TARGET_AARCH64_LINUX: {
                CiAapcs64VaList* va = ap_ptr;
                const void* src;
                if(op->va_arg_.is_fp){
                    if(va->__vr_offs < 0){
                        src = (char*)va->__vr_top + va->__vr_offs;
                        va->__vr_offs += 16;
                    }
                    else {
                        src = va->__stack;
                        va->__stack = (char*)va->__stack + advance;
                    }
                }
                else {
                    if(va->__gr_offs < 0){
                        src = (char*)va->__gr_top + va->__gr_offs;
                        va->__gr_offs += 8;
                    }
                    else {
                        src = va->__stack;
                        va->__stack = (char*)va->__stack + advance;
                    }
                }
                memcpy(dest, src, sz);
                frame->pc++;
                return 0;
            }
            case CC_TARGET_X86_64_LINUX:
            case CC_TARGET_X86_64_MACOS: {
                CiSysvVaListTag* tag = ap_ptr;
                const void* src;
                if(op->va_arg_.is_fp){
                    if(tag->fp_offset < 176){
                        src = (char*)tag->reg_save_area + tag->fp_offset;
                        tag->fp_offset += 16;
                    }
                    else {
                        src = tag->overflow_arg_area;
                        tag->overflow_arg_area = (char*)tag->overflow_arg_area + advance;
                    }
                }
                else {
                    if(tag->gp_offset < 48){
                        src = (char*)tag->reg_save_area + tag->gp_offset;
                        tag->gp_offset += 8;
                    }
                    else {
                        src = tag->overflow_arg_area;
                        tag->overflow_arg_area = (char*)tag->overflow_arg_area + advance;
                    }
                }
                memcpy(dest, src, sz);
                frame->pc++;
                return 0;
            }
            case CC_TARGET_COUNT:
                break;
            }
            return ci_error(ci, op->loc, "va_arg: unsupported target");
        }
    }
    return ci_unimplemented(ci, op->loc, "unsupported op kind");
}

static
CcFunc*_Nullable
ci_hotswap_target(CcFunc* func){
    for(int depth = 0; func; depth++){
        CcFunc* next = drp_atomic_ptr_load(&func->hotswap);
        if(!next) break;
        if(depth >= 32) return NULL;
        func = next;
    }
    return func;
}

static
int
ci_make_call_frame(CiInterpreter* ci, CiInterpFrame*_Nullable caller, CcFunc* func, void*_Nonnull*_Null_unspecified argv, uint32_t nargs, const uint32_t*_Nullable arg_sizes, void* result, size_t size, SrcLoc loc, CiInterpFrame*_Nullable*_Nonnull out){
    int err;
    CcFunc* target = ci_hotswap_target(func);
    if(!target)
        return ci_error(ci, loc, "hotswap cycle detected");
    func = target;
    if(!func->parsed)
        return ci_ice(ci, func->loc, "function '%s' not parsed before execution", func->name->data);
    if(!func->interp_ops)
        return ci_ice(ci, func->loc, "function '%s' not lowered before execution", func->name->data);
    CcFunction* ftype = func->type;
    uint32_t nfixed = ftype->param_count;
    size_t varargs_size = 0;
    if(ftype->is_variadic && nargs > nfixed){
        if(!arg_sizes)
            return ci_ice(ci, loc, "variadic call of %s staged without argument sizes", func->name->data);
        for(uint32_t i = nfixed; i < nargs; i++){
            uint32_t arg_sz = arg_sizes[i];
            if(arg_sz < 8) arg_sz = 8;
            arg_sz = (arg_sz + 7) & ~7u;
            varargs_size += arg_sz;
        }
    }
    size_t alloc_size = sizeof(CiInterpFrame) + func->frame_size + varargs_size;
    CiInterpFrame* frame = Allocator_zalloc(ci_allocator(ci), alloc_size);
    if(!frame) return CI_OOM_ERROR;
    *frame = (CiInterpFrame){
        .name = func->name,
        .parent = caller,
        .ops = func->interp_ops->code.data,
        .op_count = func->interp_ops->code.count,
        .slots = frame + 1,
        .return_buf = result,
        .return_size = size,
        .data_length = func->frame_size + varargs_size,
        .varargs_buf = ftype->is_variadic ? (char*)(frame + 1) + func->frame_size : NULL,
    };
    for(uint32_t i = 0; i < nfixed && i < nargs; i++){
        CcVariable* var = func->param_vars[i];
        if(!var) continue;
        uint32_t param_sz;
        err = cc_sizeof_as_uint(&ci->parser, ftype->params[i], func->loc, &param_sz);
        if(err){ Allocator_free(ci_allocator(ci), frame, alloc_size); return err; }
        memcpy((char*)frame->slots + var->frame_offset, argv[i], param_sz);
    }
    if(varargs_size){
        char* va_buf = frame->varargs_buf;
        for(uint32_t i = nfixed; i < nargs; i++){
            uint32_t arg_sz = arg_sizes[i];
            memcpy(va_buf, argv[i], arg_sz);
            va_buf += arg_sz < 8 ? 8 : (arg_sz + 7) & ~7u;
        }
    }
    *out = frame;
    return 0;
}

static
void
ci_free_call_frame(CiInterpreter* ci, CiInterpFrame* frame){
    ci_free_alloca_list(ci_allocator(ci), frame->alloca_list);
    Allocator_free(ci_allocator(ci), frame, sizeof *frame + frame->data_length);
}

static
int
ci_interp_run(CiInterpreter* ci, CiInterpFrame* root){
    CiInterpFrame* frame = root;
    int err = 0;
    for(;;){
        CiInterpFrame* child = NULL;
        while(frame->pc < frame->op_count){
            err = _ci_interp_step(ci, frame, &child);
            if(err) break;
        }
        if(err == CI_STEP_ENTER_FRAME){ frame = child; err = 0; continue; }
        for(;;){
            if(frame == root) return err;
            CiInterpFrame* parent = frame->parent;
            ci_free_call_frame(ci, frame);
            frame = parent;
            if(!err){ frame->pc++; break; }
        }
    }
}

static
int
ci_interp_step(CiInterpreter* ci, CiInterpFrame* frame){
    CiInterpFrame* child = NULL;
    int err = _ci_interp_step(ci, frame, &child);
    if(err == CI_STEP_ENTER_FRAME){
        err = ci_interp_run(ci, child);
        ci_free_call_frame(ci, child);
        if(!err) frame->pc++;
    }
    return err;
}

static
int
ci_call_argv(CiInterpreter* ci, CiInterpFrame*_Nullable caller, CcFunc* func, void*_Nonnull*_Nonnull argv, uint32_t nargs, const uint32_t*_Nullable arg_sizes, void* result, size_t size, SrcLoc loc){
    CiInterpFrame* frame = NULL;
    int err = ci_make_call_frame(ci, caller, func, argv, nargs, arg_sizes, result, size, loc, &frame);
    if(err) return err;
    err = ci_interp_run(ci, frame);
    ci_free_call_frame(ci, frame);
    return err;
}

static
int
ci_call_by_name(CiInterpreter* ci, StringView name, const CiArg* _Nullable args, uint32_t nargs, void* result, size_t size){
    AtomTable* at = ci_lock_atoms(ci);
    Atom atom = AT_atomize(at, name.text, name.length);
    ci_unlock_atoms(ci, at);
    if(!atom) return CI_OOM_ERROR;
    CcFunc* func = cc_scope_lookup_func(&ci->parser.global, atom, CC_SCOPE_NO_WALK);
    if(!func || !func->defined)
        return CI_SYMBOL_NOT_FOUND;
    CcFunc* target = ci_hotswap_target(func);
    if(!target)
        return ci_error(ci, (SrcLoc){0}, "hotswap cycle detected");
    func = target;
    if(!func->parsed || !func->interp_ops)
        return CI_SYMBOL_UNRESOLVED;
    CcFunction* ftype = func->type;
    // Check arg count.
    if(nargs != ftype->param_count)
        return ci_error(ci, func->loc, "ci_call_by_name '%.*s': expected %u args, got %u",
            (int)name.length, name.text, ftype->param_count, nargs);
    // Type-check args against parameter types.
    for(uint32_t i = 0; i < nargs; i++){
        if(args[i].type.bits != ftype->params[i].bits)
            return ci_error(ci, func->loc, "ci_call_by_name '%.*s': arg %u type mismatch",
                (int)name.length, name.text, i);
    }
    // The caller's buffers are the argument values; hand ci_call_argv
    // pointers to them.
    for(uint32_t i = 0; i < nargs; i++){
        uint32_t param_sz;
        int err = cc_sizeof_as_uint(&ci->parser, ftype->params[i], func->loc, &param_sz);
        if(err) return err;
        if(args[i].size < param_sz)
            return ci_error(ci, func->loc, "ci_call_by_name '%.*s': arg %u buffer too small",
                (int)name.length, name.text, i);
    }
    size_t argv_size = (nargs ? nargs : 1) * sizeof(void*);
    void** argv = Allocator_alloc(ci_allocator(ci), argv_size);
    if(!argv) return CI_OOM_ERROR;
    for(uint32_t i = 0; i < nargs; i++)
        argv[i] = (void*)(uintptr_t)args[i].data;
    int err = ci_call_argv(ci, NULL, func, argv, nargs, NULL, result, size, func->loc);
    Allocator_free(ci_allocator(ci), argv, argv_size);
    return err;
}

static
int
ci_call_main(CiInterpreter* ci, int argc, char*_Null_unspecified*_Null_unspecified argv, char*_Null_unspecified*_Null_unspecified envp, int* out_ret){
    AtomTable* at = ci_lock_atoms(ci);
    Atom atom = AT_atomize(at, "main", 4);
    ci_unlock_atoms(ci, at);
    if(!atom) return CI_OOM_ERROR;
    CcFunc* func = cc_scope_lookup_func(&ci->parser.global, atom, CC_SCOPE_NO_WALK);
    if(!func || !func->defined)
        return CI_SYMBOL_NOT_FOUND;
    CcFunction* ftype = func->type;
    uint32_t nparams = ftype->param_count;
    CiArg args[3];
    switch(nparams){
        case 0:
            break;
        case 3:
            args[2] = (CiArg){.data = &envp, .size = sizeof envp, .type = ftype->params[2]};
            goto argv;
        case 2:
            argv:
            args[0] = (CiArg){.data = &argc, .size = sizeof argc, .type = ftype->params[0]};
            args[1] = (CiArg){.data = &argv, .size = sizeof argv, .type = ftype->params[1]};
            break;
        default:
            return ci_error(ci, func->loc, "main has unsupported signature (%u params)", nparams);
    }
    int ret = 0;
    int err = ci_call_by_name(ci, SV("main"), args, nparams, &ret, sizeof ret);
    if(err) return err;
    *out_ret = ret;
    return 0;
}
static
int
ci_add_root(CiInterpreter* ci, StringView name){
    AtomTable* at = ci_lock_atoms(ci);
    Atom atom = AT_atomize(at, name.text, name.length);
    ci_unlock_atoms(ci, at);
    if(!atom) return CI_OOM_ERROR;
    CcSymbol sym;
    _Bool found = cc_scope_lookup_symbol(&ci->parser.global, atom, CC_SCOPE_NO_WALK, &sym);
    if(!found) return CI_SYMBOL_NOT_FOUND;
    int err = 0;
    switch(sym.kind){
        case CC_SYM_VAR:
            err = PM_put(&ci->parser.used_vars, ci_allocator(ci), sym.var, sym.var);
            break;
        case CC_SYM_FUNC:
            err = PM_put(&ci->parser.used_funcs, ci_allocator(ci), sym.func, sym.func);
            break;
        case CC_SYM_TYPEDEF:
        case CC_SYM_ENUMERATOR:
            break;
    }
    return err;
}

static
int
ci_resolve_root(CiInterpreter* ci, StringView name){
    int err = ci_add_root(ci, name);
    if(err) return err;
    return ci_resolve_refs(ci, 0);
}

static
int
ci_compile_module(CiInterpreter* ci, const char* source, CiModule*_Nullable*_Nonnull out){
    int err = 0;
    *out = NULL;
    ci_lock_resolver(ci);
    CcParser* p = &ci->parser;
    _Bool eager = p->eager_parsing;
    CcScope* old_current = p->current;
    Parray(CcStmtNode) old = p->toplevel_nodes;

    CiModule* module = Allocator_zalloc(ci_allocator(ci), sizeof *module);
    if(!module){
        err = CI_OOM_ERROR;
        goto done;
    }
    module->scope.parent = &ci->parser.global;
    size_t source_len = strlen(source);
    char* source_copy = "";
    if(source_len){
        // TODO: maybe this should just go through the file cache?
        source_copy = Allocator_dupe(ci_allocator(ci), source, source_len);
        if(!source_copy){
            err = CI_OOM_ERROR;
            goto done;
        }
    }
    module->source = (StringView){source_len, source_copy};
    p->toplevel_nodes = module->nodes;
    p->current = &module->scope;
    cc_parser_discard_input(p);

    fc_write_pathf(p->cpp.fc, "<__compile:%zu>", ci->next_module_id++);
    uint32_t file_id = 0;
    err = fc_intern_path(p->cpp.fc, &file_id);
    if(err) goto done;
    CppFrame frame = {
        .file_id = file_id,
        .txt = module->source,
        .line = 1,
        .column = 1,
    };
    err = ma_push(CppFrame)(&p->cpp.frames, p->cpp.allocator, frame);
    if(err){ err = CI_OOM_ERROR; goto done; }
    p->eager_parsing = 1;
    err = cc_parse_all(p);
    module->nodes = p->toplevel_nodes;
    if(err) goto done;

    err = PM_put(&ci->modules, ci_allocator(ci), module, module);
    if(err){
        err = CI_OOM_ERROR;
        goto done;
    }
    *out = module;
    done:
    p->eager_parsing = eager;
    p->current = old_current;
    p->toplevel_nodes = old;
    if(err){
        cc_parser_discard_input(p);
        // XXX: cleanup module
        //   probably just its scope, nodes, and its storage itself.
    }
    ci_unlock_resolver(ci);
    return err;
}

static
int
ci_resolve_module_unlocked(CiInterpreter* ci, CiModule* module){
    Allocator al = ci_allocator(ci);
    AtomMapItems vars = AM_items(&module->scope.variables);
    for(size_t i = 0; i < vars.count; i++){
        CcVariable* var = vars.data[i].p;
        if(!var || var->automatic) continue;
        if(var->extern_ && !var->initializer) continue;
        int err = PM_put(&ci->parser.used_vars, al, var, var);
        if(err) return CI_OOM_ERROR;
    }
    // XXX: why do we only iterate over vars?
    return ci_resolve_refs(ci, 0);
}

static
int
ci_resolve_module(CiInterpreter* ci, CiModule* module){
    ci_lock_resolver(ci);
    int err = ci_resolve_module_unlocked(ci, module);
    ci_unlock_resolver(ci);
    return err;
}

static
int
ci_parse_module_type(CiInterpreter* ci, SrcLoc loc, CiModule*_Nullable module, const char* source, CcQualType* out){
    *out = (CcQualType){0};
    CcParser* p = &ci->parser;
    int err = 0;

    ci_lock_resolver(ci);
    err = cc_parse_type_string(p, module ? &module->scope : &p->global, loc, (StringView){strlen(source), source}, out);
    ci_unlock_resolver(ci);
    (void)loc;
    return err;
}

static
size_t
ci_count_atom_items(AtomMapItems items){
    size_t count = 0;
    for(size_t i = 0; i < items.count; i++)
        count += items.data[i].p != NULL;
    return count;
}

static
int
ci_reflect_func_unlocked(CiInterpreter* ci, SrcLoc loc, CcFunc* func, CiRtModuleMember* out){
    void* address = NULL;
    if(!func->defined){
        if(!func->native_func){
            LongString fsym = func->mangle
                ? (LongString){func->mangle->length, func->mangle->data}
                : (LongString){func->name->length, func->name->data};
            int err = ci_try_dlsym(ci, fsym, &address);
            if(err) return err;
            if(address)
                func->native_func = (void(*)(void))address;
        }
        address = (void*)func->native_func;
    }
    else {
        func->addr_taken = 1;
        int err = PM_put(&ci->parser.used_funcs, ci_allocator(ci), func, func);
        if(err) return CI_OOM_ERROR;
        err = ci_resolve_refs(ci, 0);
        if(err) return err;
        address = (void*)func->native_func;
    }
    *out = (CiRtModuleMember){
        .type = (CcQualType){.bits = (uintptr_t)func->type},
        .name = func->name ? func->name->data : "",
        .name_length = func->name ? func->name->length : 0,
        .address = address,
    };
    (void)loc;
    return 0;
}

static
int
ci_reflect_var_unlocked(CiInterpreter* ci, SrcLoc loc, CcVariable* var, CiRtModuleMember* out){
    void* address = NULL;
    if(!var->automatic){
        if(var->extern_ && !var->initializer){
            if(!var->interp_val){
                LongString vsym = var->mangle
                    ? (LongString){var->mangle->length, var->mangle->data}
                    : (LongString){var->name->length, var->name->data};
                int err = ci_try_dlsym(ci, vsym, &address);
                if(err) return err;
                if(address)
                    var->interp_val = address;
            }
            address = var->interp_val;
        }
        else {
            int err = PM_put(&ci->parser.used_vars, ci_allocator(ci), var, var);
            if(err) return CI_OOM_ERROR;
            err = ci_resolve_refs(ci, 0);
            if(err) return err;
            address = var->interp_val;
        }
    }
    *out = (CiRtModuleMember){
        .type = var->type,
        .name = var->name ? var->name->data : "",
        .name_length = var->name ? var->name->length : 0,
        .address = address,
    };
    (void)loc;
    return 0;
}

static
int
ci_reflect_type_from_maps(CiRtModuleMember* out, CcScope* scope, size_t idx){
    AtomMapItems typedefs = AM_items(&scope->typedefs);
    for(size_t i = 0; i < typedefs.count; i++){
        if(!typedefs.data[i].p) continue;
        if(idx--) continue;
        *out = (CiRtModuleMember){
            .type = (CcQualType){.bits = (uintptr_t)typedefs.data[i].p},
            .name = typedefs.data[i].atom ? typedefs.data[i].atom->data : "",
            .name_length = typedefs.data[i].atom ? typedefs.data[i].atom->length : 0,
            .address = NULL,
        };
        return 0;
    }
    AtomMapItems structs = AM_items(&scope->structs);
    for(size_t i = 0; i < structs.count; i++){
        if(!structs.data[i].p) continue;
        if(idx--) continue;
        CcStruct* s = structs.data[i].p;
        *out = (CiRtModuleMember){
            .type = (CcQualType){.bits = (uintptr_t)s},
            .name = s->name ? s->name->data : "",
            .name_length = s->name ? s->name->length : 0,
            .address = NULL,
        };
        return 0;
    }
    AtomMapItems unions = AM_items(&scope->unions);
    for(size_t i = 0; i < unions.count; i++){
        if(!unions.data[i].p) continue;
        if(idx--) continue;
        CcUnion* u = unions.data[i].p;
        *out = (CiRtModuleMember){
            .type = (CcQualType){.bits = (uintptr_t)u},
            .name = u->name ? u->name->data : "",
            .name_length = u->name ? u->name->length : 0,
            .address = NULL,
        };
        return 0;
    }
    AtomMapItems enums = AM_items(&scope->enums);
    for(size_t i = 0; i < enums.count; i++){
        if(!enums.data[i].p) continue;
        if(idx--) continue;
        CcEnum* e = enums.data[i].p;
        *out = (CiRtModuleMember){
            .type = (CcQualType){.bits = (uintptr_t)e},
            .name = e->name ? e->name->data : "",
            .name_length = e->name ? e->name->length : 0,
            .address = NULL,
        };
        return 0;
    }
    return CI_SYMBOL_NOT_FOUND;
}

static
int
ci_reflect_module(CiInterpreter* ci, SrcLoc loc, CiModule*_Nullable module, CcModuleOp op, size_t idx, CiRtModuleMember* out){
    memset(out, 0, sizeof *out);
    CcScope* scope = module ? &module->scope : &ci->parser.global;
    int ret = 0;
    ci_lock_resolver(ci);
    switch(op){
        case CC_MODULE_FUNC_COUNT:
            out->name_length = ci_count_atom_items(AM_items(&scope->functions));
            break;
        case CC_MODULE_VAR_COUNT:
            out->name_length = ci_count_atom_items(AM_items(&scope->variables));
            break;
        case CC_MODULE_TYPE_COUNT:
            out->name_length =
                ci_count_atom_items(AM_items(&scope->typedefs))
                + ci_count_atom_items(AM_items(&scope->structs))
                + ci_count_atom_items(AM_items(&scope->unions))
                + ci_count_atom_items(AM_items(&scope->enums));
            break;
        case CC_MODULE_FUNC: {
            AtomMapItems items = AM_items(&scope->functions);
            CcFunc* func = NULL;
            for(size_t i = 0; i < items.count; i++){
                if(!items.data[i].p) continue;
                if(idx--) continue;
                func = items.data[i].p;
                break;
            }
            if(!func){ ret = ci_error(ci, loc, "_Module.func index out of range"); break; }
            ret = ci_reflect_func_unlocked(ci, loc, func, out);
            break;
        }
        case CC_MODULE_VAR: {
            AtomMapItems items = AM_items(&scope->variables);
            CcVariable* var = NULL;
            for(size_t i = 0; i < items.count; i++){
                if(!items.data[i].p) continue;
                if(idx--) continue;
                var = items.data[i].p;
                break;
            }
            if(!var){ ret = ci_error(ci, loc, "_Module.var index out of range"); break; }
            ret = ci_reflect_var_unlocked(ci, loc, var, out);
            break;
        }
        case CC_MODULE_TYPE:
            ret = ci_reflect_type_from_maps(out, scope, idx);
            if(ret == CI_SYMBOL_NOT_FOUND)
                ret = ci_error(ci, loc, "_Module.type index out of range");
            break;
        case CC_MODULE_SYMBOL:
        case CC_MODULE_RUN:
        case CC_MODULE_PARSE_TYPE:
        case CC_MODULE_NONE:
            ret = CI_UNREACHABLE_ERROR;
            break;
    }
    ci_unlock_resolver(ci);
    return ret;
}

static
int
ci_lookup_symbol(CiInterpreter* ci, SrcLoc loc, CiModule*_Nullable module, const char* name, CcQualType expected, void*_Nullable*_Nonnull out){
    size_t len = strlen(name);
    *out = NULL;
    AtomTable* at = ci_lock_atoms(ci);
    Atom atom = AT_atomize(at, name, len);
    ci_unlock_atoms(ci, at);
    if(!atom) return CI_OOM_ERROR;

    int ret = 0;
    ci_lock_resolver(ci);
    CcSymbol sym;
    CcScope* scope = module ? &module->scope : &ci->parser.global;
    int walk = module ? CC_SCOPE_WALK_CHAIN : CC_SCOPE_NO_WALK;
    if(cc_scope_lookup_symbol(scope, atom, walk, &sym)){
        switch(sym.kind){
            case CC_SYM_FUNC: {
                CcFunc* func = sym.func;
                CcQualType func_type = {.bits = (uintptr_t)func->type};
                if(func_type.bits != expected.bits)
                    goto done;
                if(!func->defined){
                    if(!func->native_func){
                        LongString fsym = func->mangle
                            ? (LongString){func->mangle->length, func->mangle->data}
                            : (LongString){func->name->length, func->name->data};
                        void* addr = NULL;
                        int err = ci_try_dlsym(ci, fsym, &addr);
                        if(err){ ret = err; goto done; }
                        if(!addr) goto done;
                        func->native_func = (void(*)(void))addr;
                    }
                    *out = (void*)func->native_func;
                    goto done;
                }
                if(func->defined)
                    func->addr_taken = 1;
                int err = PM_put(&ci->parser.used_funcs, ci_allocator(ci), func, func);
                if(err){ ret = CI_OOM_ERROR; goto done; }
                err = ci_resolve_refs(ci, 0);
                if(err){ ret = err; goto done; }
                if(!func->native_func)
                    { ret = ci_error(ci, loc, "_Module.symbol: function '%s' has no address", name); goto done; }
                *out = (void*)func->native_func;
                goto done;
            }
            case CC_SYM_VAR: {
                CcVariable* var = sym.var;
                if(var->type.bits != expected.bits)
                    goto done;
                if(var->automatic)
                    goto done;
                if(var->extern_ && !var->initializer){
                    if(!var->interp_val){
                        LongString vsym = var->mangle
                            ? (LongString){var->mangle->length, var->mangle->data}
                            : (LongString){var->name->length, var->name->data};
                        void* addr = NULL;
                        int err = ci_try_dlsym(ci, vsym, &addr);
                        if(err){ ret = err; goto done; }
                        if(!addr) goto done;
                        var->interp_val = addr;
                    }
                    *out = var->interp_val;
                    goto done;
                }
                int err = PM_put(&ci->parser.used_vars, ci_allocator(ci), var, var);
                if(err){ ret = CI_OOM_ERROR; goto done; }
                err = ci_resolve_refs(ci, 0);
                if(err){ ret = err; goto done; }
                if(!var->interp_val)
                    { ret = ci_error(ci, loc, "_Module.symbol: variable '%s' has no storage", name); goto done; }
                *out = var->interp_val;
                goto done;
            }
            case CC_SYM_TYPEDEF:
            case CC_SYM_ENUMERATOR:
                goto done;
        }
    }
done:
    ci_unlock_resolver(ci);
    return ret;
}

static
int
ci_resolve_refs(CiInterpreter* ci, _Bool libc_only){
    CcParser* p = &ci->parser;
    Allocator al = ci_allocator(ci);
    if(!libc_only){
        // Add main() as a root if it exists.
        {
            AtomTable* at = ci_lock_atoms(ci);
            Atom main_atom = AT_atomize(at, "main", 4);
            ci_unlock_atoms(ci, at);
            if(!main_atom) return CI_OOM_ERROR;
            CcFunc* main_func = cc_scope_lookup_func(&p->global, main_atom, CC_SCOPE_NO_WALK);
            if(main_func && main_func->defined){
                int err = PM_put(&p->used_funcs, al, main_func, main_func);
                if(err) return CI_OOM_ERROR;
            }
        }
    }
    for(size_t i = libc_only ? ci->resolved_libc : ci->resolved_funcs; i < p->used_funcs.count; i++){
        PointerMapItems items = PM_items(&p->used_funcs);
        CcFunc* func = (CcFunc*)(uintptr_t)items.data[i].key;
        if(libc_only && !func->libc_builtin) continue;
        if(!func->defined){
            if(!func->native_func && func->name){
                LongString sym = func->mangle
                    ? (LongString){func->mangle->length, func->mangle->data}
                    : (LongString){func->name->length, func->name->data};
                void* addr;
                int err = ci_dlsym(ci, func->loc, sym, "function", &addr);
                if(err) return err;
                func->native_func = (void(*)(void))addr;
            }
            continue;
        }
        if(!libc_only){
            if(func->parse_failed) continue;
            if(!func->parsed){
                int err = cc_parse_func_body(p, func);
                if(err) return err;
            }
            if(!func->interp_ops){
                int err = ci_lower_func(ci, func);
                if(err) return err;
            }
            if(!func->native_func && func->addr_taken){
                int err = ci_create_closure(ci, func);
                if(err) return err;
            }
        }
    }
    if(libc_only)
        ci->resolved_libc = p->used_funcs.count;
    else
        ci->resolved_funcs = p->used_funcs.count;
    // Resolve non-automatic variable storage.
    if(!libc_only){
        {
            PointerMapItems items = PM_items(&p->used_vars);
            for(size_t i = ci->resolved_vars; i < items.count; i++){
                CcVariable* var = (CcVariable*)(uintptr_t)items.data[i].key;
                if(var->interp_val) continue;
                if(var->extern_ && !var->initializer){
                    LongString sym = var->mangle
                        ? (LongString){var->mangle->length, var->mangle->data}
                        : (LongString){var->name->length, var->name->data};
                    void* addr;
                    int err = ci_dlsym(ci, var->loc, sym, "extern variable", &addr);
                    if(err) return err;
                    var->interp_val = addr;
                }
                else {
                    uint32_t sz;
                    int err = cc_sizeof_as_uint(p, var->type, var->loc, &sz);
                    if(err) return err;
                    void* storage = Allocator_zalloc(al, sz);
                    if(!storage) return CI_OOM_ERROR;
                    var->interp_val = storage;
                }
            }
            ci->resolved_vars = p->used_vars.count;
        }
        // Evaluate initializers for non-automatic variables.
        {
            PointerMapItems items = PM_items(&p->used_vars);
            for(size_t i = 0; i < items.count; i++){
                CcVariable* var = (CcVariable*)(uintptr_t)items.data[i].key;
                if(!var->interp_preinit) continue;
                if(!var->initializer) continue;
                if(!var->interp_val) continue;
                if(var->interp_initialized) continue;
                uint32_t sz;
                int err = cc_sizeof_as_uint(p, var->type, var->loc, &sz);
                if(err) return err;
                err = ci_eval_lowered_expr(ci, NULL, var->initializer, var->interp_val, sz);
                if(err) return err;
                var->interp_initialized = 1;
            }
        }
    }
    #ifndef NO_NATIVE_CALL
    // Pre-populate ffi_cache for non-variadic call types.
    {
        PointerMapItems funcs = PM_items(&p->used_funcs);
        for(size_t i = 0; i < funcs.count; i++){
            CcFunc* func = (CcFunc*)(uintptr_t)funcs.data[i].key;
            if(func->defined) continue;
            CcFunction* ftype = func->type;
            if(PM_get(&ci->ffi_cache, ftype)) continue;
            NativeCallCache* cache = NULL;
            int err = native_call_cache_create(al, ftype, 0, NULL, &cache);
            if(err) return err;
            err = PM_put(&ci->ffi_cache, al, ftype, cache);
            if(err) return CI_OOM_ERROR;
        }
        PointerMapItems ctypes = PM_items(&p->used_call_types);
        for(size_t i = 0; i < ctypes.count; i++){
            CcFunction* ftype = (CcFunction*)(uintptr_t)ctypes.data[i].key;
            if(PM_get(&ci->ffi_cache, ftype)) continue;
            NativeCallCache* cache = NULL;
            int err = native_call_cache_create(al, ftype, 0, NULL, &cache);
            if(err) return err;
            err = PM_put(&ci->ffi_cache, al, ftype, cache);
            if(err) return CI_OOM_ERROR;
        }
    }
    // Pre-populate ffi_cache for variadic call expressions.
    {
        PointerMapItems vcalls = PM_items(&p->used_var_calls);
        for(size_t i = ci->resolved_variadic; i < vcalls.count; i++){
            CcExpr* call_expr = (CcExpr*)(uintptr_t)vcalls.data[i].key;
            if(call_expr->lhs->kind == CC_EXPR_FUNCTION && call_expr->lhs->func->defined)
                continue;
            CcQualType ct = call_expr->lhs->type;
            CcFunction* ftype;
            if(ccqt_kind(ct) == CC_POINTER)
                ftype = ccqt_as_function(ccqt_as_ptr(ct)->pointee);
            else
                ftype = ccqt_as_function(ct);
            uint32_t nvarargs = call_expr->call.nargs - ftype->param_count;
            CcQualType* vararg_types = Allocator_alloc(al, nvarargs * sizeof(CcQualType));
            if(!vararg_types) return CI_OOM_ERROR;
            for(uint32_t j = 0; j < nvarargs; j++)
                vararg_types[j] = call_expr->values[ftype->param_count + j]->type;
            NativeCallCache* cache = NULL;
            int err = native_call_cache_create(al, ftype, nvarargs, vararg_types, &cache);
            Allocator_free(al, vararg_types, nvarargs * sizeof(CcQualType));
            if(err) return err;
            err = PM_put(&ci->ffi_cache, al, call_expr, cache);
            if(err) return CI_OOM_ERROR;
        }
        ci->resolved_variadic = p->used_var_calls.count;
    }
    #endif
    return 0;
}

static
Allocator
ci_allocator(CiInterpreter* ci){
#ifndef CI_THREAD_UNSAFE_ALLOCATOR
    (void)ci;
    return MALLOCATOR;
#else
    return ci->parser.cpp.allocator;
#endif
}
static
Allocator
ci_scratch_allocator(CiInterpreter* ci){
    return allocator_from_arena(&ci->parser.scratch_arena);
}

static
int
ci_append_lib_path(CiInterpreter* ci, StringView sv){
    if(!sv.length) return CI_INVALID_VALUE_ERROR;
    Atom a = AT_atomize(ci->parser.cpp.at, sv.text, sv.length);
    if(!a) return CI_OOM_ERROR;
    int err = AM_put(&ci->lib_paths, ci_allocator(ci), a, (void*)(uintptr_t)1);
    if(err) return CI_OOM_ERROR;
    return 0;
}

static
int
ci_preload_system_libs(CiInterpreter* ci){
    #if defined(_WIN32) && !defined(NO_NATIVE_CALL)
    static const char*const libs[] = {"ucrtbase", "kernel32", "ntdll"};
    for(size_t i = 0; i < sizeof libs / sizeof libs[0]; i++)
        LoadLibraryA(libs[i]);
    #endif
    (void)ci;
    return 0;
}

static CppPragmaFn ci_pragma_lib, ci_pragma_lib_path, ci_pragma_framework, ci_pragma_pkg_config, ci_pragma_procmacro, ci_pragma_comment, ci_pragma_resolve;
static
int
ci_register_pragmas(CiInterpreter*ci){
    int err = 0;
    err = cpp_register_pragma(&ci->parser.cpp, SV("comment"), ci_pragma_comment, ci);
    if(err) return err;
    err = cpp_register_pragma(&ci->parser.cpp, SV("lib"), ci_pragma_lib, ci);
    if(err) return err;
    err = cpp_register_pragma(&ci->parser.cpp, SV("lib_path"), ci_pragma_lib_path, ci);
    if(err) return err;
    err = cpp_register_pragma(&ci->parser.cpp, SV("framework"), ci_pragma_framework, ci);
    if(err) return err;
    err = cpp_register_pragma(&ci->parser.cpp, SV("pkg_config"), ci_pragma_pkg_config, ci);
    if(err) return err;
    if(ci->procedural_macros){
        err = cpp_register_pragma(&ci->parser.cpp, SV("procmacro"), ci_pragma_procmacro, ci);
        if(err) return err;
        err = cpp_register_pragma(&ci->parser.cpp, SV("resolve"), ci_pragma_resolve, ci);
        if(err) return err;
    }
    return 0;
}

static
int
ci_register_macros(CiInterpreter* ci){
    int err = 0;
    err = cpp_define_builtin_func_macro(&ci->parser.cpp, SV("__shell"), ci_shell, ci, 1, 1, 0);
    if(err) return err;
    err = cpp_define_builtin_func_macro(&ci->parser.cpp, SV("__SHELL__"), ci_shell, ci, 1, 1, 0);
    if(err) return err;
    return err;
}

static
int
ci_try_load_library(CiInterpreter* ci, LongString lib, _Bool* success){
    if(!ci->can_dlopen) return CI_RUNTIME_ERROR;
    #ifdef NO_NATIVE_CALL
        (void)ci; (void)lib;
        *success = 0;
        return 0;
    #else
        Atom a = AT_atomize(ci->parser.cpp.at, lib.text, lib.length);
        if(!a) return CI_OOM_ERROR;
        void* handle = AM_get(&ci->opened_libs, a);
        if(handle) {*success = 1; return 0;}
        #ifdef _WIN32
        MStringBuilder16 sb = {.allocator = ci_scratch_allocator(ci)};
        msb16_write_utf8(&sb, a->data, a->length);
        msb16_nul_terminate(&sb);
        if(sb.errored){
            msb16_destroy(&sb);
            return CI_OOM_ERROR;
        }
        LongStringUtf16 wlib = msb16_borrow_ls(&sb);
        handle = LoadLibraryW((const wchar_t*)wlib.text);
        msb16_destroy(&sb);
        if(!handle) {*success = 0; return 0;}
        #else
        handle = dlopen(a->data, RTLD_GLOBAL | RTLD_LAZY);
        if(!handle) {*success = 0; return 0;}
        #endif
        int err = AM_put(&ci->opened_libs, ci_allocator(ci), a, handle);
        if(err) {
            #ifdef _WIN32
            FreeLibrary(handle);
            #else
            dlclose(handle);
            #endif
            return CI_OOM_ERROR;
        }
        *success = 1;
        return 0;
    #endif
}

static
int
ci_load_library(CiInterpreter* ci, StringView sv){
    if(!ci->can_dlopen) return CI_RUNTIME_ERROR;
    MStringBuilder sb = {.allocator=ci_scratch_allocator(ci)};
    int err = 0;
    _Bool success = 0;
    StringView prefix = SV("lib");
    StringView suffixes[2] = {{0}, {0}};
    size_t nsuffixes = 0;
    switch(ci_target(ci)->os){
        case CC_OS_MACOS:
            suffixes[nsuffixes++] = SV(".dylib");
            suffixes[nsuffixes++] = SV(".so");
            break;
        case CC_OS_LINUX:
        case CC_OS_TEST:
            suffixes[nsuffixes++] = SV(".so");
            break;
        case CC_OS_WINDOWS:
            prefix = SV("");
            suffixes[nsuffixes++] = SV(".dll");
            break;
    }
    AtomMapItems items = AM_items(&ci->lib_paths);
    for(size_t i = 0; i < items.count; i++){
        if(!items.data[i].p) continue;
        Atom path = items.data[i].atom;
        for(size_t s = 0; s < nsuffixes; s++){
            msb_reset(&sb);
            msb_write_str(&sb, path->data, path->length);
            if(msb_peek(&sb) != '/') msb_write_char(&sb, '/');
            msb_write_str(&sb, prefix.text, prefix.length);
            msb_write_str(&sb, sv.text, sv.length);
            msb_write_str(&sb, suffixes[s].text, suffixes[s].length);
            msb_nul_terminate(&sb);
            if(sb.errored){ err = CI_OOM_ERROR; goto finally; }
            err = ci_try_load_library(ci, msb_borrow_ls(&sb), &success);
            if(err) goto finally;
            if(success) goto finally;
        }
        if(sv_endswith(sv, SV(".dylib"))
        || sv_endswith(sv, SV(".dll"))
        || sv_contains(sv, SV(".so"))){
            msb_reset(&sb);
            msb_write_str(&sb, path->data, path->length);
            if(msb_peek(&sb) != '/') msb_write_char(&sb, '/');
            msb_write_str(&sb, sv.text, sv.length);
            msb_nul_terminate(&sb);
            if(sb.errored){ err = CI_OOM_ERROR; goto finally; }
            err = ci_try_load_library(ci, msb_borrow_ls(&sb), &success);
            if(err) goto finally;
            if(success) goto finally;
        }
    }
    if(ci_target(ci)->os == CC_OS_MACOS){
        for(size_t i = 0; !success && i < ci->parser.cpp.framework_paths.count; i++){
            StringView fp = ci->parser.cpp.framework_paths.data[i];
            msb_reset(&sb);
            msb_write_str(&sb, fp.text, fp.length);
            if(msb_peek(&sb) != '/') msb_write_char(&sb, '/');
            msb_write_str(&sb, sv.text, sv.length);
            msb_write_literal(&sb, ".framework/");
            msb_write_str(&sb, sv.text, sv.length);
            msb_nul_terminate(&sb);
            if(sb.errored){ err = CI_OOM_ERROR; goto finally; }
            err = ci_try_load_library(ci, msb_borrow_ls(&sb), &success);
            if(err) goto finally;
            if(success) goto finally;
        }
    }
    for(size_t s = 0; s < nsuffixes; s++){
        msb_reset(&sb);
        msb_write_str(&sb, prefix.text, prefix.length);
        msb_write_str(&sb, sv.text, sv.length);
        msb_write_str(&sb, suffixes[s].text, suffixes[s].length);
        msb_nul_terminate(&sb);
        if(sb.errored){ err = CI_OOM_ERROR; goto finally; }
        err = ci_try_load_library(ci, msb_borrow_ls(&sb), &success);
        if(err) goto finally;
        if(success) goto finally;
    }
    {
        msb_reset(&sb);
        msb_write_str(&sb, sv.text, sv.length);
        msb_nul_terminate(&sb);
        if(sb.errored){ err = CI_OOM_ERROR; goto finally; }
        err = ci_try_load_library(ci, msb_borrow_ls(&sb), &success);
        if(err) goto finally;
        if(success) goto finally;
    }
    err = CI_LIBRARY_NOT_FOUND_ERROR;
    finally:
    msb_destroy(&sb);
    return err;
}

static
int
ci_load_framework(CiInterpreter* ci, StringView sv){
    if(!ci->can_dlopen) return CI_RUNTIME_ERROR;
    if(ci_target(ci)->os != CC_OS_MACOS) return CI_LIBRARY_NOT_FOUND_ERROR;
    MStringBuilder sb = {.allocator=ci_scratch_allocator(ci)};
    int err = 0;
    _Bool success = 0;
    for(size_t i = 0; i < ci->parser.cpp.framework_paths.count; i++){
        StringView fp = ci->parser.cpp.framework_paths.data[i];
        msb_reset(&sb);
        msb_write_str(&sb, fp.text, fp.length);
        if(msb_peek(&sb) != '/') msb_write_char(&sb, '/');
        msb_write_str(&sb, sv.text, sv.length);
        msb_write_literal(&sb, ".framework/");
        msb_write_str(&sb, sv.text, sv.length);
        msb_nul_terminate(&sb);
        if(sb.errored){ err = CI_OOM_ERROR; goto finally; }
        err = ci_try_load_library(ci, msb_borrow_ls(&sb), &success);
        if(err) goto finally;
        if(success) goto finally;
    }
    err = CI_LIBRARY_NOT_FOUND_ERROR;
    finally:
    msb_destroy(&sb);
    return err;
}

static
int
ci_pragma_comment(void* _Null_unspecified ctx, CppPreprocessor* cpp, SrcLoc loc, const CppToken*_Null_unspecified toks, size_t ntoks){
    // #pragma comment(lib, "name")
    // Skip whitespace between tokens.
    CiInterpreter* ci = ctx;
    size_t i = 0;
    while(i < ntoks && toks[i].type == CPP_WHITESPACE) i++;
    if(i >= ntoks || toks[i].type != CPP_PUNCTUATOR || toks[i].punct != '(')
        return 0; // ignore unknown comment pragmas
    i++;
    while(i < ntoks && toks[i].type == CPP_WHITESPACE) i++;
    if(i >= ntoks || toks[i].type != CPP_IDENTIFIER)
        return 0;
    StringView kind = toks[i].txt;
    i++;
    if(!sv_equals(kind, SV("lib")))
        return 0; // ignore non-lib comment pragmas
    while(i < ntoks && toks[i].type == CPP_WHITESPACE) i++;
    if(i >= ntoks || toks[i].type != CPP_PUNCTUATOR || toks[i].punct != ',')
        return cpp_error(cpp, loc, "#pragma comment(lib, ...) expected ','");
    i++;
    while(i < ntoks && toks[i].type == CPP_WHITESPACE) i++;
    if(i >= ntoks || toks[i].type != CPP_STRING || toks[i].txt.length < 2)
        return cpp_error(cpp, loc, "#pragma comment(lib, ...) expected string literal");
    StringView name = {toks[i].txt.length-2, toks[i].txt.text+1};
    if(!ci->can_dlopen)
        return 0; // silently ignore when dlopen is disabled
    if(sv_endswith(name, SV(".lib")))
        name.length -= 4;
    int err = ci_load_library(ci, name);
    if(err == CI_LIBRARY_NOT_FOUND_ERROR)
        err = cpp_error(cpp, loc, "failed to load library '%.*s'", (int)name.length, name.text);
    return err;
}

static
int
ci_pragma_lib(void* _Null_unspecified ctx, CppPreprocessor* cpp, SrcLoc loc, const CppToken*_Null_unspecified toks, size_t ntoks){
    CiInterpreter* ci = ctx;
    if(!ci->can_dlopen)
        return cpp_error(cpp, loc, "Loading libraries is disabled");
    CppTokens* expanded = cpp_get_scratch(cpp);
    if(!expanded) return CI_OOM_ERROR;
    int err = 0;
    err = cpp_expand_argument(cpp, toks, ntoks, expanded);
    if(err) goto finally;
    toks = expanded->data;
    ntoks = expanded->count;
    while(ntoks && toks->type == CPP_WHITESPACE){
        toks++;
        ntoks--;
    }
    while(ntoks && toks[ntoks-1].type == CPP_WHITESPACE)
        ntoks--;
    if(!ntoks){
        err = cpp_error(cpp, loc, "#pragma lib without any arguments");
        goto finally;
    }
    if(toks->type != CPP_STRING || toks->txt.length < 2){
        err = cpp_error(cpp, loc, "#pragma lib requires a string literal library name");
        goto finally;
    }
    StringView name = {toks->txt.length-2, toks->txt.text+1};
    err = ci_load_library(ci, name);
    if(err == CI_LIBRARY_NOT_FOUND_ERROR)
        err = cpp_error(cpp, loc, "failed to load library '%.*s'", (int)name.length, name.text);
    finally:
    cpp_release_scratch(cpp, expanded);
    return err;
}
static
int
ci_pragma_lib_path(void* _Null_unspecified ctx, CppPreprocessor* cpp, SrcLoc loc, const CppToken*_Null_unspecified toks, size_t ntoks){
    CiInterpreter* ci = ctx;
    CppTokens* expanded = cpp_get_scratch(cpp);
    if(!expanded) return CI_OOM_ERROR;
    int err = 0;
    err = cpp_expand_argument(cpp, toks, ntoks, expanded);
    if(err) goto finally;
    toks = expanded->data;
    ntoks = expanded->count;
    while(ntoks && toks->type == CPP_WHITESPACE){
        toks++;
        ntoks--;
    }
    while(ntoks && toks[ntoks-1].type == CPP_WHITESPACE)
        ntoks--;
    if(!ntoks){
        err = cpp_error(cpp, loc, "#pragma lib_path without any arguments");
        goto finally;
    }
    if(toks->type != CPP_STRING || toks->txt.length < 2){
        err = cpp_error(cpp, loc, "#pragma lib_path requires a string literal library name");
        goto finally;
    }
    StringView name = {toks->txt.length-2, toks->txt.text+1};
    err = ci_append_lib_path(ci, name);
    finally:
    cpp_release_scratch(cpp, expanded);
    return err;
}
static
int
ci_pragma_framework(void* _Null_unspecified ctx, CppPreprocessor* cpp, SrcLoc loc, const CppToken*_Null_unspecified toks, size_t ntoks){
    CiInterpreter* ci = ctx;
    if(!ci->can_dlopen)
        return cpp_error(cpp, loc, "Loading libraries is disabled");
    CppTokens* expanded = cpp_get_scratch(cpp);
    if(!expanded) return CI_OOM_ERROR;
    int err = 0;
    err = cpp_expand_argument(cpp, toks, ntoks, expanded);
    if(err) goto finally;
    toks = expanded->data;
    ntoks = expanded->count;
    while(ntoks && toks->type == CPP_WHITESPACE){
        toks++;
        ntoks--;
    }
    while(ntoks && toks[ntoks-1].type == CPP_WHITESPACE)
        ntoks--;
    if(!ntoks){
        err = cpp_error(cpp, loc, "#pragma framework without any arguments");
        goto finally;
    }
    if(toks->type != CPP_STRING || toks->txt.length < 2){
        err = cpp_error(cpp, loc, "#pragma framework requires a string literal framework name");
        goto finally;
    }
    StringView name = {toks->txt.length-2, toks->txt.text+1};
    err = ci_load_framework(ci, name);
    if(err == CI_LIBRARY_NOT_FOUND_ERROR)
        err = cpp_error(cpp, loc, "failed to load framework '%.*s'", (int)name.length, name.text);
    finally:
    cpp_release_scratch(cpp, expanded);
    return err;
}
static
_Bool
ci_file_exists(void* _Null_unspecified ctx, const char* path, size_t length){
    (void)ctx;
    (void)length;
    #ifdef _WIN32
    DWORD attrs = GetFileAttributesA(path);
    return attrs != INVALID_FILE_ATTRIBUTES && !(attrs & FILE_ATTRIBUTE_DIRECTORY);
    #else
    return access(path, F_OK) == 0;
    #endif
}

static
int
ci_pragma_pkg_config(void* _Null_unspecified ctx, CppPreprocessor* cpp, SrcLoc loc, const CppToken*_Null_unspecified toks, size_t ntoks){
    CiInterpreter* ci = ctx;
    CppTokens* expanded = cpp_get_scratch(cpp);
    if(!expanded) return CI_OOM_ERROR;
    int err = 0;
    LongString output = {0};
    Allocator scratch = ci_scratch_allocator(ci);
    err = cpp_expand_argument(cpp, toks, ntoks, expanded);
    if(err) goto finally;
    toks = expanded->data;
    ntoks = expanded->count;
    while(ntoks && toks->type == CPP_WHITESPACE){
        toks++;
        ntoks--;
    }
    while(ntoks && toks[ntoks-1].type == CPP_WHITESPACE)
        ntoks--;
    if(!ntoks){
        err = cpp_error(cpp, loc, "#pragma pkg_config without any arguments");
        goto finally;
    }
    _Bool optional = 0;
    if(toks->type == CPP_IDENTIFIER){
        if(sv_equals(toks->txt, SV("optional"))){
            optional = 1;
            do {
                toks++;
                ntoks--;
            } while(ntoks && toks->type == CPP_WHITESPACE);
        }
    }
    if(!ntoks || toks->type != CPP_STRING || toks->txt.length < 2){
        err = cpp_error(cpp, loc, "#pragma pkg_config requires a string literal package name");
        goto finally;
    }
    {
        StringView pkg_name = {toks->txt.length-2, toks->txt.text+1};
        CmdBuilder cmd = {.allocator = scratch};
        cmd_prog(&cmd, LS("pkg-config"));
        cmd_resolve_prog_path(&cmd, cpp->env, ci_file_exists, NULL);
        if(cmd.errored){
            if(optional){
                cpp_warn(cpp, loc, "`pkg-config` not found in PATH");
                err = 0;
                goto finally;
            }
            err = cpp_error(cpp, loc, "`pkg-config` not found in PATH");
            cmd_destroy(&cmd);
            goto finally;
        }
        cmd_cargs(&cmd, "--cflags", "--libs");
        {
            Atom a = AT_atomize(ci->parser.cpp.at, pkg_name.text, pkg_name.length);
            if(!a){
                err = CI_OOM_ERROR;
                goto finally;
            }
            cmd_aarg(&cmd, a);
        }
        size_t envp_size = 0;
        void* envp = env_to_envp(cpp->env, scratch, &envp_size);
        if(!envp){
            err = CI_OOM_ERROR;
            goto finally;
        }
        err = cmd_run_capture(&cmd, envp, scratch, &output);
        cmd_destroy(&cmd);
        Allocator_free(scratch, envp, envp_size);
        if(err){
            if(optional) {
                err = 0;
                goto finally;
            }
            err = cpp_error(cpp, loc, "pkg-config failed for '%.*s'", (int)pkg_name.length, pkg_name.text);
            goto finally;
        }
        #if defined(__clang__) || defined(__GNUC__)
            #pragma GCC diagnostic push
            #pragma GCC diagnostic ignored "-Wcast-qual"
            #if !defined(__clang__)
                #pragma GCC diagnostic ignored "-Wdiscarded-qualifiers"
            #endif
        #elif defined(_MSC_VER)
            #pragma warning(push)
            #pragma warning(disable: 4090)
        #else
        #endif
        char* cmdline = (char*)output.text;
        #if defined(__clang__) || defined(__GNUC__)
            #pragma GCC diagnostic pop
        #elif defined(_MSC_VER)
            #pragma warning(pop)
        #endif
        enum { MAX_PKG_FLAGS = 64 };
        StringView pkg_I[MAX_PKG_FLAGS], pkg_L[MAX_PKG_FLAGS], pkg_l[MAX_PKG_FLAGS];
        StringView pkg_F[MAX_PKG_FLAGS], pkg_D[MAX_PKG_FLAGS], pkg_fw[MAX_PKG_FLAGS];
        size_t npkg_I=0, npkg_L=0, npkg_l=0, npkg_F=0, npkg_D=0, npkg_fw=0;
        #define PKG_KW(flag_, name_) { \
            .name = SV(flag_), \
            .max_num = MAX_PKG_FLAGS, \
            .one_at_a_time = 1, \
            .space_sep_is_optional = 1, \
            .dest = {.type = ARG_STRING, .pointer = name_}, \
            .pnum_parsed = &n##name_, \
        }
        ArgToParse pkg_kwargs[] = {
            PKG_KW("-I", pkg_I),
            PKG_KW("-L", pkg_L),
            PKG_KW("-l", pkg_l),
            PKG_KW("-F", pkg_F),
            PKG_KW("-D", pkg_D),
            {
                .name = SV("-framework"),
                .max_num = MAX_PKG_FLAGS,
                .one_at_a_time = 1,
                .dest = {.type = ARG_STRING, .pointer = pkg_fw},
                .pnum_parsed = &npkg_fw,
            },
        };
        #undef PKG_KW
        ArgParser pkg_parser = {
            .keyword = {.args = pkg_kwargs, .count = arrlen(pkg_kwargs)},
        };
        enum ArgParseError ap_err = parse_args_cmdline(&pkg_parser, cmdline,
            ARGPARSE_FLAGS_UNKNOWN_KWARGS_AS_ARGS
            | ARGPARSE_FLAGS_ALLOW_KWARG_SEP_TO_BE_OPTIONAL
            | ARGPARSE_FLAGS_IGNORE_EXCESS_ARGS);
        if(ap_err){
            err = cpp_error(cpp, loc, "pkg-config: failed to parse flags for '%.*s'", (int)pkg_name.length, pkg_name.text);
            goto finally;
        }
        for(size_t i = 0; i < npkg_I; i++){
            Atom a = AT_atomize(ci->parser.cpp.at, pkg_I[i].text, pkg_I[i].length);
            if(!a){ err = CI_OOM_ERROR; goto finally; }
            err = cpp_add_default_includea(cpp, &cpp->Ipaths, a);
            if(err) goto finally;
        }
        for(size_t i = 0; i < npkg_L; i++){
            err = ci_append_lib_path(ci, pkg_L[i]);
            if(err) goto finally;
        }
        for(size_t i = 0; i < npkg_l; i++){
            err = ci_load_library(ci, pkg_l[i]);
            if(err == CI_LIBRARY_NOT_FOUND_ERROR)
                err = cpp_error(cpp, loc, "pkg-config: failed to load library '%.*s'", (int)pkg_l[i].length, pkg_l[i].text);
            if(err == CI_RUNTIME_ERROR)
                err = cpp_error(cpp, loc, "pkg-config: Loading libraries is disabled");
            if(err) goto finally;
        }
        for(size_t i = 0; i < npkg_F; i++){
            Atom a = AT_atomize(ci->parser.cpp.at, pkg_F[i].text, pkg_F[i].length);
            if(!a){ err = CI_OOM_ERROR; goto finally; }
            err = cpp_add_default_includea(cpp, &cpp->framework_paths, a);
            if(err) goto finally;
        }
        for(size_t i = 0; i < npkg_D; i++){
            StringView val = pkg_D[i];
            const char* eq = memchr(val.text, '=', val.length);
            if(eq){
                StringView mname = {(size_t)(eq - val.text), val.text};
                StringView mval = {val.length - mname.length - 1, eq + 1};
                CppToken valtok = {.type = CPP_NUMBER, .txt = {mval.length, mval.text}};
                err = cpp_define_obj_macro(cpp, mname, &valtok, 1);
            }
            else {
                CppToken onetok = {.type = CPP_NUMBER, .txt = {1, "1"}};
                err = cpp_define_obj_macro(cpp, val, &onetok, 1);
            }
            if(err == _cc_macro_already_exists_error)
                err = cpp_error(cpp, loc, "pkg-config: macro '%.*s' already defined with a different value", (int)val.length, val.text);
            if(err) goto finally;
        }
        for(size_t i = 0; i < npkg_fw; i++){
            err = ci_load_library(ci, pkg_fw[i]);
            if(err == CI_LIBRARY_NOT_FOUND_ERROR)
                err = cpp_error(cpp, loc, "pkg-config: failed to load framework '%.*s'", (int)pkg_fw[i].length, pkg_fw[i].text);
            if(err == CI_RUNTIME_ERROR)
                err = cpp_error(cpp, loc, "pkg-config: Loading libraries is disabled");
            if(err) goto finally;
        }
    }
    finally:
    if(output.text) Allocator_free(scratch, output.text, output.length+1);
    cpp_release_scratch(cpp, expanded);
    return err;
}
static CiInterpreter*
ci_from_cpp(CppPreprocessor* cpp){
    return (CiInterpreter*)((char*)cpp - offsetof(CcParser, cpp) - offsetof(CiInterpreter, parser));
}

static Marray(CcToken)* cc_get_scratch(CcParser* p);
static void cc_release_scratch(CcParser* p, Marray(CcToken)*);

static
int
ci_procmacro_expand(void* _Null_unspecified ctx, CppPreprocessor* cpp, SrcLoc loc, CppTokens* outtoks, const CppTokens* args, const Marray(size_t)* arg_seps){
    (void)arg_seps;
    CcFunc* func = ctx;
    CiInterpreter* ci = ci_from_cpp(cpp);
    CcParser* p = &ci->parser;
    CcFunction* ftype = func->type;
    CcExpr* expr = NULL;
    int err = 0;
    Allocator al = p->cpp.allocator;
    Marray(CcToken)* scratch = cc_get_scratch(p);
    Marray(CcToken) pending = p->pending;
    CcToken eof = {.type = CC_EOF, .loc = loc};
    err = ma_push(CcToken)(scratch, al, eof);
    if(err) goto restore;
    CcToken rparen = {.punct = {.type = CC_PUNCTUATOR, .punct = CC_rparen}, .loc = loc};
    err = ma_push(CcToken)(scratch, al, rparen);
    if(err) goto restore;
    size_t idx = scratch->count;
    {
        const CppToken* arg_toks = args->data;
        const CppToken* end = args->data + args->count;
        while(arg_toks < end){
            CcToken tok;
            err = cpp_next_c_token_array(cpp, &arg_toks, end, &tok);
            if(err) goto restore;
            if(tok.type == CC_EOF)
                break;
            err = ma_push(CcToken)(scratch, al, tok);
            if(err) goto restore;
        }
    }
    for(size_t i = idx, j = scratch->count - 1; i < j; i++, j--){
        CcToken tok = scratch->data[j];
        scratch->data[j] = scratch->data[i];
        scratch->data[i] = tok;
    }
    CcToken lparen = {.punct = {.type = CC_PUNCTUATOR, .punct = CC_lparen}, .loc = loc};
    err = ma_push(CcToken)(scratch, al, lparen);
    if(err) goto restore;
    CcToken name_tok = {.ident = {.type = CC_IDENTIFIER, .ident = func->name}, .loc = loc};
    err = ma_push(CcToken)(scratch, al, name_tok);
    if(err)goto restore;
    p->pending = *scratch;
    // FIXME: should really be constexpr, but functions aren't supported...
    err = cc_parse_expr(p, CC_RUNTIME_VALUE, &expr);
    {
        restore:
        *scratch = p->pending;
        p->pending = pending;
        cc_release_scratch(p, scratch);
        if(err) return err;
    }
    uint32_t result_sz;
    err = cc_sizeof_as_uint(p, ftype->return_type, loc, &result_sz);
    if(err) return err;
    _Alignas(16) char result_buf[16];
    char* result = result_buf;
    uint32_t orig_result_sz = result_sz;
    if(result_sz > 16){
        result = Allocator_zalloc(ci_scratch_allocator(ci), result_sz);
        if(!result){
            return CI_OOM_ERROR;
        }
    }
    char* orig_result = result;
    err = ci_eval_lowered_expr(ci, &ci->top_frame, expr, result, result_sz);
    if(err) goto cleanup;
    CcQualType rt = ftype->return_type;
    Atom a = NULL;
    int tok_type = CPP_NUMBER;
    retry:;
    switch(ccqt_kind(rt)){
        case CC_BASIC:
            switch(rt.basic.kind){
                case CCBT_nullptr_t:
                    a = AT_ATOMIZE(cpp->at, "nullptr");
                    tok_type = CPP_IDENTIFIER;
                    break;
                case CCBT__Any:{
                    CiRtAny any;
                    CI_INLINE_MEMCPY(&any, result, sizeof any);
                    if(!cc_any_payload_type(p, any.type)){
                        err = ci_error(ci, loc, "Invalid _Any payload type in procedural macro result");
                        goto cleanup;
                    }
                    rt = any.type;
                    result += offsetof(CiRtAny, payload);
                    goto retry;
                }
                case CCBT__Type:
                    err = ci_unimplemented(ci, loc, "TODO: _Type to tokens");
                    goto cleanup;
                case CCBT_COUNT:
                case CCBT_INVALID:
                    err = ci_error(ci, loc, "Invalid return type");
                    goto cleanup;
                case CCBT_void:
                    goto cleanup;
                case CCBT_bool:
                    a = *(_Bool*)result?AT_ATOMIZE(cpp->at, "true"):AT_ATOMIZE(cpp->at, "false");
                    tok_type = CPP_IDENTIFIER;
                    break;
                case CCBT_char:
                    if(ci_target(ci)->char_is_signed)
                        goto signed_char;
                    else goto unsigned_char;
                case CCBT_signed_char:
                    signed_char:
                    a = cpp_atomizef(cpp, "%d", (int)ci_read_int(result, 1));
                    break;
                case CCBT_unsigned_char:
                    unsigned_char:
                    a = cpp_atomizef(cpp, "%d", (int)ci_read_uint(result, 1));
                    break;
                case CCBT_short:
                    a = cpp_atomizef(cpp, "%d", (int)ci_read_int(result, ci_target(ci)->sizeof_[CCBT_short]));
                    break;
                case CCBT_unsigned_short:
                    a = cpp_atomizef(cpp, "%d", (int)ci_read_uint(result, ci_target(ci)->sizeof_[CCBT_unsigned_short]));
                    break;
                case CCBT_int:
                    a = cpp_atomizef(cpp, "%d", (int)ci_read_int(result, ci_target(ci)->sizeof_[CCBT_int]));
                    break;
                case CCBT_unsigned:
                    a = cpp_atomizef(cpp, "%u", (unsigned)ci_read_uint(result, ci_target(ci)->sizeof_[CCBT_unsigned]));
                    break;
                case CCBT_long:
                    a = cpp_atomizef(cpp, "%lld", (long long)ci_read_int(result, ci_target(ci)->sizeof_[CCBT_long]));
                    break;
                case CCBT_unsigned_long:
                    a = cpp_atomizef(cpp, "%llu", (unsigned long long)ci_read_uint(result, ci_target(ci)->sizeof_[CCBT_unsigned_long]));
                    break;
                case CCBT_long_long:
                    a = cpp_atomizef(cpp, "%lld", (long long)ci_read_int(result, ci_target(ci)->sizeof_[CCBT_long_long]));
                    break;
                case CCBT_unsigned_long_long:
                    a = cpp_atomizef(cpp, "%llu", (unsigned long long)ci_read_uint(result, ci_target(ci)->sizeof_[CCBT_unsigned_long_long]));
                    break;
                case CCBT_int128:
                case CCBT_unsigned_int128:
                    err = ci_error(ci, loc, "Invalid return type: int128 not expressable as cpp token");
                    goto cleanup;
                case CCBT_float16:
                    err = ci_unimplemented(ci, loc, "TODO: float16a");
                    goto cleanup;
                case CCBT_float:
                    a = cpp_atomizef(cpp, "%.9gf", (double)*(float*)result);
                    break;
                case CCBT_double:
                    a = cpp_atomizef(cpp, "%.17g", (double)*(double*)result);
                    break;
                case CCBT_long_double:
                case CCBT_float128:
                case CCBT_float_complex:
                case CCBT_double_complex:
                case CCBT_long_double_complex:
                    err = ci_error(ci, loc, "Invalid return type");
                    goto cleanup;
            }
            break;
        case CC_ENUM:
            err = ci_unimplemented(ci, loc, "TODO: enum to token");
            goto cleanup;
        case CC_POINTER:{
            CcPointer* ptr = ccqt_as_ptr(rt);
            size_t max_slen = -1;
            if(ccqt_kind(ptr->pointee) == CC_ARRAY && ccqt_bt_eq((CcQualType){.unqual=ccqt_as_array(ptr->pointee)->element.unqual}, CCBT_char)){
                max_slen = ccqt_as_array(ptr->pointee)->length;
                goto handle_string;
            }
            if(ccqt_is_basic(ptr->pointee) && ptr->pointee.basic.kind == CCBT_char){
                handle_string:;
                // string literal
                const char* s = *(const char**)result;
                if(!s){
                    a = AT_ATOMIZE(cpp->at, "nullptr");
                    tok_type = CPP_IDENTIFIER;
                }
                else {
                    // Escape the string so cpp_mixin_string's
                    // escape processing reconstructs the original bytes.
                    MStringBuilder sb = {.allocator = ci_scratch_allocator(ci)};
                    msb_write_char(&sb, '"');
                    for(size_t i = 0, slen = strnlen(s, max_slen); i < slen; i++){
                        unsigned char c = (unsigned char)s[i];
                        switch(c){
                            case '\\': msb_write_literal(&sb, "\\\\"); break;
                            case '"':  msb_write_literal(&sb, "\\\""); break;
                            case '\n': msb_write_literal(&sb, "\\n"); break;
                            case '\t': msb_write_literal(&sb, "\\t"); break;
                            case '\r': msb_write_literal(&sb, "\\r"); break;
                            case '\a': msb_write_literal(&sb, "\\a"); break;
                            case '\b': msb_write_literal(&sb, "\\b"); break;
                            case '\f': msb_write_literal(&sb, "\\f"); break;
                            case '\v': msb_write_literal(&sb, "\\v"); break;
                            case '\0': msb_write_literal(&sb, "\\0"); break;
                            default:   msb_write_char(&sb, c); break;
                        }
                    }
                    msb_write_char(&sb, '"');
                    a = msb_atomize(&sb, cpp->at);
                    msb_destroy(&sb);
                    tok_type = CPP_STRING;
                }
                if(!a){
                    err = CI_OOM_ERROR;
                    goto cleanup;
                }
                break;
            }
            err = ci_unimplemented(ci, loc, "Unsupported return type pointer");
            goto cleanup;
        }
        case CC_STRUCT:
            err = ci_unimplemented(ci, loc, "struct to cpp tokens");
            goto cleanup;
        case CC_UNION:
            err = ci_unimplemented(ci, loc, "union to cpp tokens");
            goto cleanup;
        case CC_BLOCK_POINTER:
        case CC_SLICE:
            err = ci_unimplemented(ci, loc, "Unsupported return type");
            goto cleanup;
        case CC_ARRAY:
            err = ci_ice(ci,loc, "Somehow returning an %s?", "array");
            goto cleanup;
        case CC_FUNCTION:
            err = ci_ice(ci,loc, "Somehow returning a %s?", "function");
            goto cleanup;
    }
    if(!a) {err = CI_OOM_ERROR; goto cleanup;}
    CppToken tok = {
        .type = tok_type,
        .txt = {a->length, a->data},
        .loc = loc,
    };
    err = cpp_push_tok(cpp, outtoks, tok);
    goto cleanup;
    cleanup:
    if(expr) cc_release_expr(&ci->parser, expr);
    if(orig_result != result_buf) Allocator_free(ci_scratch_allocator(ci), orig_result, orig_result_sz);
    return err;
}

static
int
ci_pragma_procmacro(void* _Null_unspecified ctx, CppPreprocessor* cpp, SrcLoc loc, const CppToken*_Null_unspecified toks, size_t ntoks){
    int err;
    CiInterpreter* ci = ctx;
    while(ntoks && toks->type == CPP_WHITESPACE){ toks++; ntoks--; }
    if(!ntoks || toks->type != CPP_IDENTIFIER)
        return cpp_error(cpp, loc, "#pragma procmacro: expected function name");
    StringView name = toks->txt;
    Atom atom = AT_get_atom(cpp->at, name.text, name.length);
    if(!atom)
        return cpp_error(cpp, loc, "#pragma procmacro: unknown function '%.*s'", (int)name.length, name.text);
    CcFunc* func = cc_scope_lookup_func(&ci->parser.global, atom, CC_SCOPE_NO_WALK);
    if(!func || !func->defined)
        return cpp_error(cpp, loc, "#pragma procmacro: function '%.*s' not defined", (int)name.length, name.text);
    if(!func->parsed){
        err = cc_parse_func_body(&ci->parser, func);
        if(err) return err;
    }
    if(!func->interp_ops){
        err = ci_lower_func(ci, func);
        if(err) return err;
    }
    err = ci_resolve_refs(ci, 1);
    if(err) return err;
    return cpp_define_builtin_func_macro(cpp, name, ci_procmacro_expand, func, func->type->param_count, 0, 0);
}

static
int
ci_pragma_resolve(void* _Null_unspecified ctx, CppPreprocessor* cpp, SrcLoc loc, const CppToken*_Null_unspecified toks, size_t ntoks){
    int err;
    CiInterpreter* ci = ctx;
    while(ntoks){
    // Skip whitespace.
        while(ntoks && toks->type == CPP_WHITESPACE){ toks++; ntoks--; }
        if(!ntoks) return 0;
        if(toks->type != CPP_IDENTIFIER)
            return cpp_error(cpp, loc, "#pragma resolve: expected symbol name");
        StringView name = toks->txt;
        // Look up the function.
        Atom atom = AT_get_atom(cpp->at, name.text, name.length);
        if(!atom)
            return cpp_error(cpp, loc, "#pragma resolve: unknown symbol '%.*s'", (int)name.length, name.text);
        CcSymbol sym;
        _Bool found = cc_scope_lookup_symbol(&ci->parser.global, atom, CC_SCOPE_NO_WALK, &sym);
        if(!found)
            return cpp_error(cpp, loc, "#pragma resolve: unknown symbol '%.*s'", (int)name.length, name.text);
        switch(sym.kind){
            case CC_SYM_VAR:
                if(sym.var->interp_val) break;
                if(sym.var->extern_ && !sym.var->initializer){
                    LongString s = sym.var->mangle
                        ? (LongString){sym.var->mangle->length, sym.var->mangle->data}
                        : (LongString){sym.var->name->length, sym.var->name->data};
                    void* addr;
                    err = ci_dlsym(ci, sym.var->loc, s, "extern variable", &addr);
                    if(err) return err;
                    sym.var->interp_val = addr;
                    break;
                }
                else {
                    uint32_t sz;
                    err = cc_sizeof_as_uint(&ci->parser, sym.var->type, sym.var->loc, &sz);
                    if(err) return err;
                    Allocator al = ci_allocator(ci);
                    void* storage = Allocator_zalloc(al, sz);
                    if(!storage) return CI_OOM_ERROR;
                    sym.var->interp_val = storage;
                    break;
                }
            case CC_SYM_FUNC:
                if(!sym.func->defined){
                    if(!sym.func->native_func && sym.func->name){
                        LongString s = sym.func->mangle
                            ? (LongString){sym.func->mangle->length, sym.func->mangle->data}
                            : (LongString){sym.func->name->length, sym.func->name->data};
                        void* addr;
                        err = ci_dlsym(ci, sym.func->loc, s, "function", &addr);
                        if(err) return err;
                        sym.func->native_func = (void(*)(void))addr;
                    }
                    break;
                }
                if(!sym.func->parsed){
                    err = cc_parse_func_body(&ci->parser, sym.func);
                    if(err) return err;
                }
                if(!sym.func->interp_ops){
                    err = ci_lower_func(ci, sym.func);
                    if(err) return err;
                }
                if(!sym.func->native_func && sym.func->addr_taken){
                    err = ci_create_closure(ci, sym.func);
                    if(err) return err;
                }
                break;
            case CC_SYM_TYPEDEF:
            case CC_SYM_ENUMERATOR:
                break;
        }
        toks++; ntoks--;
    }
    return 0;
}

static
const CcTargetConfig*
ci_target(const CiInterpreter* ci){
    return &ci->parser.cpp.target;
}
static
int
ci_dlsym(CiInterpreter* ci, SrcLoc loc, LongString sym, const char* what, void*_Nullable*_Nonnull out){
    void* p = NULL;
    int err = ci_try_dlsym(ci, sym, &p);
    if(err) return err;
    if(!p){
        if(ci_target(ci)->target != (CcTarget)CC_TARGET_NATIVE)
            return ci_error(ci, loc, "%s '%s' not found (cross-interpreting)", what, sym.text);
        #ifdef NO_NATIVE_CALL
        return ci_error(ci, loc, "%s '%s' not found (native calls disabled)", what, sym.text);
        #elif defined _WIN32
        ci_error(ci, loc, "%s '%s' not found", what, sym.text);
        HMODULE modules[256];
        DWORD needed = 0;
        if(K32EnumProcessModules(GetCurrentProcess(), modules, sizeof modules, &needed)){
            DWORD count = needed / sizeof(HMODULE);
            if(count > 256) count = 256;
            Logger* logger = ci->parser.cpp.logger;
            for(DWORD i = 0; i < count; i++){
                char name[256];
                if(K32GetModuleBaseNameA(GetCurrentProcess(), modules[i], name, sizeof name))
                    log_logf(logger, LOG_PRINT_ERROR, "  loaded: %s\n", name);
            }
        }
        return CI_RUNTIME_ERROR;
        #else
        return ci_error(ci, loc, "%s '%s' not found: %s", what, sym.text, dlerror());
        #endif
    }
    *out = p;
    return 0;
}

static
int
ci_try_dlsym(CiInterpreter* ci, LongString sym, void*_Nullable*_Nonnull out){
    *out = NULL;
    // Try virtual libs first.
    Atom a = AT_get_atom(ci->parser.cpp.at, sym.text, sym.length);
    if(a){
        AtomMapItems vlibs = AM_items(&ci->virtual_libs);
        for(size_t i = 0; i < vlibs.count; i++){
            AtomMap(void*)* symbols = vlibs.data[i].p;
            void* p = AM_get(symbols, a);
            if(p){
                *out = p;
                return 0;
            }
        }
    }
    if(ci_target(ci)->target != (CcTarget)CC_TARGET_NATIVE)
        return 0;
    void* p = NULL;
    #ifdef NO_NATIVE_CALL
        (void)p;
        return 0;
    #elif defined _WIN32
        // Search all loaded modules (like dlsym(RTLD_DEFAULT, ...)).
        {
            HMODULE modules[256];
            DWORD needed = 0;
            BOOL ok = K32EnumProcessModules(GetCurrentProcess(), modules, sizeof modules, &needed);
            if(ok){
                DWORD count = needed / sizeof(HMODULE);
                if(count > 256) count = 256;
                for(DWORD i = 0; i < count; i++){
                    p = (void*)GetProcAddress(modules[i], sym.text);
                    if(p) break;
                }
            }
        }
    #else
        p = dlsym(RTLD_DEFAULT, sym.text);
        if(!p && sym.text[0] == '_')
            p = dlsym(RTLD_DEFAULT, sym.text+1);
    #endif
    *out = p;
    return 0;
}
static
int
ci_shell(void* _Null_unspecified ctx, CppPreprocessor* cpp, SrcLoc loc, CppTokens* outtoks, const CppTokens* args, const Marray(size_t)* arg_seps){
    (void)arg_seps;
    int err = 0;
    CiInterpreter* ci = ctx;
    Allocator scratch = ci_scratch_allocator(ci);
    LongString output = {0};
    if(!cpp->env){
        return cpp_error(cpp, loc, "__SHELL__: no environment available");
    }
    // Each string token across all args becomes a separate command argument.
    // The first string is the program.
    CmdBuilder cmd = {.allocator = scratch};
    for(size_t i = 0; i < args->count; i++){
        CppToken t = args->data[i];
        if(t.type == CPP_WHITESPACE || t.type == CPP_NEWLINE || t.type == CPP_PUNCTUATOR) continue;
        if(t.type != CPP_STRING || t.txt.length < 2){
            err = cpp_error(cpp, t.loc, "__SHELL__: arguments must be string literals");
            cmd_destroy(&cmd);
            return err;
        }
        MStringBuilder decoded = {.allocator = scratch};
        err = cpp_decode_text(cpp, t, &decoded);
        if(!err && decoded.cursor && memchr(decoded.data, 0, decoded.cursor))
            err = cpp_error(cpp, t.loc, "__SHELL__: arguments must not contain NUL bytes");
        if(err){
            msb_destroy(&decoded);
            cmd_destroy(&cmd);
            return err;
        }
        Atom a = AT_atomize(cpp->at, decoded.data ? decoded.data : "", decoded.cursor);
        msb_destroy(&decoded);
        if(!a){ cmd_destroy(&cmd); return CI_OOM_ERROR; }
        if(!cmd.args.count){
            cmd_prog(&cmd, (LongString){a->length, a->data});
            cmd_resolve_prog_path(&cmd, cpp->env, ci_file_exists, NULL);
            if(cmd.errored){
                err = cpp_error(cpp, loc, "__SHELL__: '%s' not found in PATH", a->data);
                cmd_destroy(&cmd);
                return err;
            }
        }
        else {
            cmd_aarg(&cmd, a);
        }
    }
    if(!cmd.args.count){
        return cpp_error(cpp, loc, "__SHELL__: requires at least one argument");
    }
    size_t envp_size = 0;
    void* envp = env_to_envp(cpp->env, scratch, &envp_size);
    if(!envp){
        cmd_destroy(&cmd);
        return CI_OOM_ERROR;
    }
    err = cmd_run_capture(&cmd, envp, scratch, &output);
    cmd_destroy(&cmd);
    Allocator_free(scratch, envp, envp_size);
    if(err){
        if(output.text) Allocator_free(scratch, output.text, output.length + 1);
        return cpp_error(cpp, loc, "__SHELL__: command failed");
    }
    // Strip trailing newlines.
    size_t output_alloc_size = output.length + 1;
    while(output.length > 0 && (output.text[output.length-1] == '\n' || output.text[output.length-1] == '\r'))
        output.length--;
    Atom v = cpp_quote_string(cpp, LS_to_SV(output));
    Allocator_free(scratch, output.text, output_alloc_size);
    if(!v) return CI_OOM_ERROR;
    CppToken result = {
        .txt = {v->length, v->data},
        .loc = loc,
        .type = CPP_STRING,
    };
    err = cpp_push_tok(cpp, outtoks, result);
    return err;
}

static
int
ci_backtrace(CiInterpreter* ci, CiInterpFrame* f, int level){
    for(;;){
        if(!f || !f->ops || f->pc >= f->op_count) return 1;
        const CiOp* op = &f->ops[f->pc];
        ci_error(ci, op->loc, "%s: %d", f->name?f->name->data:"(top level)", level);
        if(!f->parent) return 0;
        f = f->parent;
        level++;
    }
}
static
int
ci_register_sym(CiInterpreter*ci, StringView libname, StringView symname, void* sym){
    int err;
    Atom lname = AT_atomize(ci->parser.cpp.at, libname.text, libname.length);
    if(!lname) return CI_OOM_ERROR;
    Atom name = AT_atomize(ci->parser.cpp.at, symname.text, symname.length);
    if(!name) return CI_OOM_ERROR;
    CiVirtualLib* lib = AM_get(&ci->virtual_libs, lname);
    if(!lib){
        lib = Allocator_zalloc(ci_allocator(ci), sizeof *lib);
        if(!lib) return CI_OOM_ERROR;
        err = AM_put(&ci->virtual_libs, ci_allocator(ci), lname, lib);
        if(err){
            Allocator_free(ci_allocator(ci), lib, sizeof *lib);
            return CI_OOM_ERROR;
        }
    }
    err = AM_put(&lib->symbols, ci_allocator(ci), name, sym);
    if(err) return CI_OOM_ERROR;
    return 0;
}
static
AtomTable*
ci_lock_atoms(CiInterpreter* ci){
    LOCK_T_lock(&ci->atom_lock);
    AtomTable* at = ci->parser.cpp.at;
    ci->parser.cpp.at = NULL;
    return at;
}
static
void
ci_unlock_atoms(CiInterpreter* ci, AtomTable* at){
    ci->parser.cpp.at = at;
    LOCK_T_unlock(&ci->atom_lock);
}
static
void
ci_lock_resolver(CiInterpreter* ci){
    LOCK_T_lock(&ci->resolve_lock);
}
static
void
ci_unlock_resolver(CiInterpreter* ci){
    LOCK_T_unlock(&ci->resolve_lock);
}
#ifdef __clang__
#pragma clang assume_nonnull end
#endif
#include "ci_lower.c"
