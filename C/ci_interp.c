//
// Copyright © 2026-2026, David Priver <david@davidpriver.com>
//
#include <string.h>
#include <stddef.h>
#ifdef _WIN32
#include "../Drp/windowsheader.h"
#include <psapi.h>
#else
#include <dlfcn.h>
#if defined __GLIBC__ && !defined RTLD_DEFAULT
#define RTLD_DEFAULT ((void *)0)
#endif
#endif
#include "ci_interp.h"
#include "ci_rt.h"
#include "ci_op.h"
#include "cc_memory_order.h"
#include "cc_errors.h"
#include "cc_type_check.h"
#include "cc_var.h"
#include "cc_expr.h"
#include "cc_target.h"
#include "cc_scope.h"
#include "cpp_preprocessor.h"
#include "nc_native_call.h"
#include "nc_va_list.h"
#include "ci_softnum.h"
#include "../Drp/measure_time.h"
#include "../Drp/drp_profile.h"
#include "../Drp/Allocators/allocator.h"
#include "../Drp/Allocators/mallocator.h"
#include "../Drp/Allocators/arena_allocator.h"
#include "../Drp/MStringBuilder.h"
#include "../Drp/MStringBuilder16.h"
#include "../Drp/cmd_run.h"
#include "../Drp/stringview.h"
#include "../Drp/argument_parsing.h"
#include "../Drp/bit_util.h"
#include "../Drp/ckdint.h"
#include "../Drp/msb_atomize.h"
#include "../Drp/switch_macros.h"
#include "../Drp/atomics.h"
#include "cc_printer.h"
#ifdef __clang__
#pragma clang assume_nonnull begin
#endif

#ifndef force_inline
#if defined __GNUC__ || defined __clang__ || defined __DRC__
#define force_inline static inline __attribute__((always_inline))
#elif defined _MSC_VER
#define force_inline static inline __forceinline
#else
#define force_inline static inline
#endif
#endif

force_inline int _ci_interp_step(CiInterpreter* ci, CiInterpFrame* frame, CiInterpFrame*_Nullable*_Nonnull child);
static void ci_free_call_frame(CiInterpreter*, CiInterpFrame*);
static void ci_closure_cleanup(CiInterpreter*);

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
#ifndef ci_ice
#define ci_ice(ci, loc, fmt, ...) ci_error(ci, loc, "ICE: " fmt " at %s:%d", __VA_ARGS__, __FILE__, __LINE__)
#endif

static int ci_lower_module(CiInterpreter*, CiModule*, CiLowerDeps*);

static char ci_discard_buf[8192];

#define CI_RESULT_TOO_SMALL(ci, loc, sz, size) \
    ci_error((ci), (loc), "interpreter:%s:%d: result buffer too small (need %zu, have %zu)", __FILE__, __LINE__, (size_t)(sz), (size_t)(size))

static Allocator ci_allocator(CiInterpreter*);
static Allocator ci_scratch_allocator(CiInterpreter*);

static Allocator cc_allocator(CcParser*);

static const CcTargetConfig* ci_target(const CiInterpreter*);
static int ci_dlsym(CiInterpreter*, SrcLoc, Atom, const char* what, void*_Nullable*_Nonnull);
static const CcFunc*_Nullable ci_hotswap_target(const CcFunc*);
static int ci_make_abi_call_frame(CiInterpreter*, CiInterpFrame*, const CcFunc*, const CiOp*, CiInterpFrame* _Nullable* _Nonnull);
static int ci_call_abi(CiInterpreter*, CiInterpFrame* _Nullable, const CcFunc*, NcCallState*, SrcLoc);
static int ci_call_argv(CiInterpreter*, CiInterpFrame* _Nullable caller, const CcFunc*, void* _Nonnull* _Nonnull argv, uint32_t nargs, void* result, size_t size, SrcLoc loc);
static int ci_try_dlsym(CiInterpreter*, Atom, void*_Nullable*_Nonnull);
static void ci_lock_resolver(CiInterpreter*);
static void ci_unlock_resolver(CiInterpreter*);
// re-declare here as I'm not sure if this should be used in the interpreter or not
static int cc_sizeof_as_uint(CcParser* p, CcQualType t, SrcLoc loc, uint32_t* out);
static int cc_expr_sizeof_as_uint(CcParser* p, const CcExpr* e, uint32_t* out);
static int cc_alignof_as_uint(CcParser* p, CcQualType t, SrcLoc loc, uint32_t* out);
static inline int cc_align_to(CcParser* p, SrcLoc loc, uint32_t offset, uint32_t alignment, uint32_t* out);
static int cc_lookup_field_offset(CcParser*, CcQualType, Atom, uint64_t*, CcQualType*, CcField*_Nullable*_Nonnull);
static int ci_eval_lowered_expr(CiInterpreter*, CiInterpFrame*_Nullable, CcExpr*, void*, size_t);
static int cc_parse_expr(CcParser* p, CcParseFlags, CcExpr* _Nullable* _Nonnull out, CcQualType expected);
static void cc_release_expr(CcParser* p, CcExpr* e);

static CppFuncMacroFn ci_shell, ci_procmacro_expand;

typedef struct { _Alignas(16) char bytes[16]; } CiAtomic16;
#define ci_unimplemented(p, loc, msg) (ci_error(p, loc, "UNIMPLEMENTED: " msg " at %s:%d", __FILE__, __LINE__), CI_UNIMPLEMENTED_ERROR)
#define ci_unreachable(p, loc, msg) (ci_error(p, loc, "UNREACHABLE: " msg " at %s:%d", __FILE__, __LINE__), CI_UNREACHABLE_ERROR)

// Only use this when it is absolutely impossible, prefer ci_ice() above
// if it could be a bug.
#ifdef __GNUC__
#define ci_unsafe_unreachable() __builtin_unreachable()
#elif defined _MSC_VER
#define ci_unsafe_unreachable() __assume(0)
#else
#define ci_unsafe_unreachable() abort()
#endif

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

// Userdata for interpreted function closures.
typedef struct CiClosureData CiClosureData;
struct CiClosureData {
    CiInterpreter* ci;
    CcFunc* func;
    uint32_t ret_sz;
    NcClosure* _Nullable closure;
    CiClosureData* _Nullable next;
};

static
CcFunc*_Nullable
ci_closure_map_get(CiInterpreter* ci, void*_Null_unspecified entry){
    LOCK_T_lock(&ci->closure_map_lock);
    CcFunc* func = BPM_rget(&ci->closure_map, entry);
    LOCK_T_unlock(&ci->closure_map_lock);
    return func;
}

static
int
ci_closure_map_put(CiInterpreter* ci, CcFunc* func, void* entry){
    LOCK_T_lock(&ci->closure_map_lock);
    int err = BPM_put(&ci->closure_map, ci_allocator(ci), func, entry);
    LOCK_T_unlock(&ci->closure_map_lock);
    return err;
}

static
void
ci_closure_cleanup(CiInterpreter* ci){
    Allocator al = ci_allocator(ci);
    while(ci->closures){
        CiClosureData* cd = ci->closures;
        ci->closures = cd->next;
        if(cd->closure) nc_destroy_closure(al, (NcClosure* _Nonnull)cd->closure);
        cd->func->native_closure = NULL;
        cd->func->native_func = NULL;
        Allocator_free(al, cd, 1, sizeof *cd, _Alignof(CiClosureData));
    }
    LOCK_T_lock(&ci->closure_map_lock);
    BidiPointerMap map = ci->closure_map;
    ci->closure_map = (BidiPointerMap){0};
    LOCK_T_unlock(&ci->closure_map_lock);
    if(map.cap) Allocator_free(al, map.data, BPM_alloc_size(map.cap), 1, _Alignof(BidiPointerMapItem));
}

static
void
ci_closure_callback(const NcCallPlan* plan, NcCallState* state, void* userdata){
    // The compiled function already embeds its parameter import plan in bytecode.
    (void)plan;
    CiClosureData* cd = userdata;
    ci_call_abi(cd->ci, NULL, cd->func, state, cd->func->loc);
}

static
int
ci_create_closure(CiInterpreter* ci, CcFunc* func){
    DRP_PROFILE_BEGIN(profile, "resolve.native_closure");
    int err;
    void (*entry)(void);
    #ifdef NO_NATIVE_CALL
        // No native code can call this, so just use the CcFunc* itself as a
        // fake function pointer. The closure_map reverse lookup will catch it
        // at interpreted call sites.
        entry = (void(*)(void))(uintptr_t)func;
    #else
        Allocator al = ci_allocator(ci);
        CiClosureData* cd = Allocator_zalloc(al, 1, sizeof *cd, _Alignof(CiClosureData));
        if(!cd) return CI_OOM_ERROR;
        cd->ci = ci;
        cd->func = func;
        if(!(ccqt_bt_eq(func->type->return_type, CCBT_void))){
            err = cc_sizeof_as_uint(&ci->parser, func->type->return_type, func->loc, &cd->ret_sz);
            // This should never happen, but who knows.
            if(err){
                Allocator_free(al, cd, 1, sizeof *cd, _Alignof(CiClosureData));
                return err;
            }
        }
        NcClosure* nc;
        err = nc_create_closure(al, func->interp_ops->native_type, ci_closure_callback, cd, &nc);
        if(err){
            Allocator_free(al, cd, 1, sizeof *cd, _Alignof(CiClosureData));
            if(err == CI_UNIMPLEMENTED_ERROR)
                ci_ice(ci, func->loc, "function should've already been rejected in lowering%s", "");
            return err;
        }
        entry = nc_closure_get_fn(nc);
    #endif
    err = ci_closure_map_put(ci, func, (void*)entry);
    if(err){
        #ifndef NO_NATIVE_CALL
        nc_destroy_closure(al, nc);
        Allocator_free(al, cd, 1, sizeof *cd, _Alignof(CiClosureData));
        #endif
        return err == 1 ? CI_OOM_ERROR : CI_RUNTIME_ERROR;
    }
    // Publish only after registration succeeds, so preparation can be retried.
    func->native_func = entry;
    #ifndef NO_NATIVE_CALL
    func->native_closure = nc;
    cd->closure = nc;
    cd->next = ci->closures;
    ci->closures = cd;
#endif
    return 0;
    DRP_PROFILE_END(profile);
}

static inline
uint64_t
ci_read_uint(const void* buf, uint32_t sz){
    switch(sz){
        case 1: return *(const uint8_t*)buf;
        case 2: { uint16_t v; CI_INLINE_MEMCPY(&v, buf, sizeof v); return v; }
        case 4: { uint32_t v; CI_INLINE_MEMCPY(&v, buf, sizeof v); return v; }
        case 8: { uint64_t v; CI_INLINE_MEMCPY(&v, buf, sizeof v); return v; }
        default: ci_unsafe_unreachable();
    }
}

static inline
int64_t
ci_read_int(const void* buf, uint32_t sz){
    switch(sz){
        case 1: return *(const int8_t*)buf;
        case 2: { int16_t v; CI_INLINE_MEMCPY(&v, buf, sizeof v); return v; }
        case 4: { int32_t v; CI_INLINE_MEMCPY(&v, buf, sizeof v); return v; }
        case 8: { int64_t v; CI_INLINE_MEMCPY(&v, buf, sizeof v); return v; }
        default: ci_unsafe_unreachable();
    }
}

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

static
int
ci_ensure_var_storage(CiInterpreter* ci, CcVariable* var){
    if(var->automatic) return 0;
    if(var->interp_val) return 0;
    return ci_ice(ci, var->loc, "variable '%s' storage not resolved before execution", var->name->data);
}

static
int
ci_frame_stack(CiInterpreter* ci, CiInterpFrame*_Nullable frame, CiStack*_Nullable*_Nonnull out){
    if(frame && frame->stack){
        *out = frame->stack;
        return 0;
    }
    CiTlsThread* state;
    int err = ci_tls_get(ci, &state);
    if(err) return err;
    *out = &state->stack;
    if(frame) frame->stack = *out;
    return 0;
}

static
int
ci_new_frame(CiInterpreter* ci, CiInterpFrame*_Nullable caller, size_t data_size, uint32_t alignment, CiInterpFrame*_Nullable*_Nonnull out){
    CiStack* stack;
    int err = ci_frame_stack(ci, caller, &stack);
    if(err) return err;
    CiStackMark mark = ci_stack_mark(stack);
    CiInterpFrame* frame = ci_stack_alloc(stack, sizeof *frame, _Alignof(CiInterpFrame));
    if(!frame) return CI_OOM_ERROR;
    void* slots = ci_stack_alloc(stack, data_size, alignment > 16 ? alignment : 16);
    if(!slots){
        ci_stack_rewind(stack, mark);
        return CI_OOM_ERROR;
    }
    *frame = (CiInterpFrame){
        .parent = caller, .stack = stack, .stack_mark = mark,
        .slots = slots, .data_length = data_size,
    };
    if(ci->poison_frame_slots) memset(frame->slots, 0xCC, data_size);
    *out = frame;
    return 0;
}

static inline
uint64_t
ci_bitfield_read(void* storage_addr, uint32_t storage_sz, uint8_t bit_offset, uint8_t bit_width){
    uint64_t storage = 0;
    memcpy(&storage, storage_addr, storage_sz);
    uint64_t mask = bit_width >= 64 ? ~UINT64_C(0) : (UINT64_C(1) << bit_width)-1;
    return (storage >> bit_offset) & mask;
}

static inline
CiUint128
ci_bitfield_read128(void* storage_addr, uint8_t bit_offset, uint8_t bit_width){
    CiUint128 storage = {0};
    memcpy(&storage, storage_addr, sizeof storage);
    if(bit_width == 128) return storage;
    CiUint128 mask = ci_uint128_sub(ci_uint128_shl(ci_uint128_from_uint64(1), bit_width), ci_uint128_from_uint64(1));
    return ci_uint128_and(ci_uint128_shr(storage, bit_offset), mask);
}

static inline
void
ci_bitfield_write(void* storage_addr, uint32_t storage_sz, uint8_t bit_offset, uint8_t bit_width, uint64_t val){
    uint64_t mask = bit_width >= 64 ? ~UINT64_C(0) : (UINT64_C(1) << bit_width)-1;
    uint64_t storage = 0;
    memcpy(&storage, storage_addr, storage_sz);
    storage = (storage & ~(mask << bit_offset)) | ((val & mask) << bit_offset);
    memcpy(storage_addr, &storage, storage_sz);
}

static inline
void
ci_bitfield_write_128(void* storage_addr, uint8_t bit_offset, uint8_t bit_width, CiUint128 val){
    CiUint128 mask = bit_width == 128 ? ci_uint128_from_int64(-1) : ci_uint128_sub(ci_uint128_shl(ci_uint128_from_uint64(1), bit_width), ci_uint128_from_uint64(1));
    CiUint128 storage = {0};
    memcpy(&storage, storage_addr, sizeof storage);
    storage = ci_uint128_and(storage, ci_uint128_not(ci_uint128_shl(mask, bit_offset)));
    storage = ci_uint128_or(storage, ci_uint128_shl(ci_uint128_and(val, mask), bit_offset));
    memcpy(storage_addr, &storage, sizeof storage);
}

// Read an exotic bitfield, like in a packed struct
static inline
uint64_t
ci_bitfield_read_bytes(void* storage_addr, uint8_t bit_offset, uint8_t bit_width){
    const unsigned char* bytes = storage_addr;
    uint64_t value = 0;
    for(uint32_t done = 0; done < bit_width;){
        uint32_t pos = bit_offset+done, shift = pos%8;
        uint32_t n = bit_width-done;
        if(n > 8-shift) n = 8-shift;
        uint64_t part = (bytes[pos/8] >> shift) & ((1u << n)-1);
        value |= part << done;
        done += n;
    }
    return value;
}

static inline
CiUint128
ci_bitfield_read128_bytes(void* storage_addr, uint8_t bit_offset, uint8_t bit_width){
    const unsigned char* bytes = storage_addr;
    CiUint128 value = {0};
    for(uint32_t done = 0; done < bit_width;){
        uint32_t pos = bit_offset+done, shift = pos%8;
        uint32_t n = bit_width-done;
        if(n > 8-shift) n = 8-shift;
        uint64_t part = (bytes[pos/8] >> shift) & ((1u << n)-1);
        value = ci_uint128_or(value, ci_uint128_shl(ci_uint128_from_uint64(part), done));
        done += n;
    }
    return value;
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
CiUint128
ci_bitfield_extend128(CiUint128 val, uint8_t bit_width, _Bool is_signed){
    CiUint128 mask = bit_width == 128 ? ci_uint128_from_int64(-1) : ci_uint128_sub(ci_uint128_shl(ci_uint128_from_uint64(1), bit_width), ci_uint128_from_uint64(1));
    val = ci_uint128_and(val, mask);
    if(is_signed){
        CiUint128 sign_bit = ci_uint128_shl(ci_uint128_from_uint64(1), bit_width-1);
        if(ci_uint128_nonzero(ci_uint128_and(val, sign_bit)))
            val = ci_uint128_or(val, ci_uint128_not(mask));
    }
    return val;
}

// Write an exotic bitfield, like in a packed struct
static inline
void
ci_bitfield_write_bytes(void* storage_addr, uint8_t bit_offset, uint8_t bit_width, uint64_t val){
    unsigned char* bytes = storage_addr;
    for(uint32_t done = 0; done < bit_width;){
        uint32_t pos = bit_offset+done, shift = pos%8;
        uint32_t n = bit_width-done;
        if(n > 8-shift) n = 8-shift;
        uint32_t mask = ((1u << n)-1) << shift;
        bytes[pos/8] = (unsigned char)((bytes[pos/8] & ~mask) | (((val >> done) << shift) & mask));
        done += n;
    }
}

static inline
void
ci_bitfield_write_128_bytes(void* storage_addr, uint8_t bit_offset, uint8_t bit_width, CiUint128 val){
    unsigned char* bytes = storage_addr;
    for(uint32_t done = 0; done < bit_width;){
        uint32_t pos = bit_offset+done, shift = pos%8;
        uint32_t n = bit_width-done;
        if(n > 8-shift) n = 8-shift;
        uint32_t mask = ((1u << n)-1) << shift;
        uint64_t part = ci_uint128_lo(ci_uint128_shr(val, done));
        bytes[pos/8] = (unsigned char)((bytes[pos/8] & ~mask) | ((part << shift) & mask));
        done += n;
    }
}

static inline
void*
ci_va_stack_source(void* _Nonnull* _Nonnull stack, uint32_t mask, uint32_t stride){
    uintptr_t cursor = ((uintptr_t)*stack + mask) & ~(uintptr_t)mask;
    *stack = (void*)(cursor + stride);
    return (void*)cursor;
}

static inline
CiUint128
ci_read_u128(const void* src){
    CiUint128 value;
    memcpy(&value, src, 16);
    return value;
}

static inline
float
ci_read_f32(const void* src){
    float value;
    memcpy(&value, src, 4);
    return value;
}

static inline
double
ci_read_f64(const void* src){
    double value;
    memcpy(&value, src, 8);
    return value;
}

#if defined _MSC_VER
#if defined(_M_ARM64) || defined(_M_ARM64EC)
#define CI_ATOMIC_LOAD8(p, d) do { *(uint8_t*)(d) = (uint8_t)__iso_volatile_load8((const volatile __int8*)(p)); __dmb(_ARM64_BARRIER_ISH); } while(0)
#define CI_ATOMIC_STORE8(p, s) do { __dmb(_ARM64_BARRIER_ISH); __iso_volatile_store8((volatile __int8*)(p), *(const __int8*)(s)); __dmb(_ARM64_BARRIER_ISH); } while(0)
#else
#define CI_ATOMIC_LOAD8(p, d) do { *(uint8_t*)(d) = *(volatile uint8_t*)(p); _ReadWriteBarrier(); } while(0)
#define CI_ATOMIC_STORE8(p, s) (void)_InterlockedExchange8((volatile char*)(p), *(const char*)(s))
#endif
#define CI_ATOMIC_XCHG8(p, v, o) (*(uint8_t*)(o) = (uint8_t)_InterlockedExchange8((volatile char*)(p), *(const char*)(v)))
#define CI_ATOMIC_ADD8(p, v, o) (*(uint8_t*)(o) = (uint8_t)_InterlockedExchangeAdd8((volatile char*)(p), *(const char*)(v)))
#define CI_ATOMIC_SUB8(p, v, o) (*(uint8_t*)(o) = (uint8_t)_InterlockedExchangeAdd8((volatile char*)(p), -*(const char*)(v)))
#define CI_ATOMIC_AND8(p, v, o) (*(uint8_t*)(o) = (uint8_t)_InterlockedAnd8((volatile char*)(p), *(const char*)(v)))
#define CI_ATOMIC_OR8(p, v, o) (*(uint8_t*)(o) = (uint8_t)_InterlockedOr8((volatile char*)(p), *(const char*)(v)))
#define CI_ATOMIC_XOR8(p, v, o) (*(uint8_t*)(o) = (uint8_t)_InterlockedXor8((volatile char*)(p), *(const char*)(v)))
#define CI_ATOMIC_CAS8(p, e, d, weak, result) do { uint8_t exp = *(uint8_t*)(e); uint8_t old = (uint8_t)_InterlockedCompareExchange8((volatile char*)(p), *(const char*)(d), (char)exp); (result) = old == exp; *(uint8_t*)(e) = old; } while(0)
#if defined(_M_ARM64) || defined(_M_ARM64EC)
#define CI_ATOMIC_LOAD16(p, d) do { *(uint16_t*)(d) = (uint16_t)__iso_volatile_load16((const volatile __int16*)(p)); __dmb(_ARM64_BARRIER_ISH); } while(0)
#define CI_ATOMIC_STORE16(p, s) do { __dmb(_ARM64_BARRIER_ISH); __iso_volatile_store16((volatile __int16*)(p), *(const __int16*)(s)); __dmb(_ARM64_BARRIER_ISH); } while(0)
#else
#define CI_ATOMIC_LOAD16(p, d) do { *(uint16_t*)(d) = *(volatile uint16_t*)(p); _ReadWriteBarrier(); } while(0)
#define CI_ATOMIC_STORE16(p, s) (void)_InterlockedExchange16((volatile short*)(p), *(const short*)(s))
#endif
#define CI_ATOMIC_XCHG16(p, v, o) (*(uint16_t*)(o) = (uint16_t)_InterlockedExchange16((volatile short*)(p), *(const short*)(v)))
#define CI_ATOMIC_ADD16(p, v, o) (*(uint16_t*)(o) = (uint16_t)_InterlockedExchangeAdd16((volatile short*)(p), *(const short*)(v)))
#define CI_ATOMIC_SUB16(p, v, o) (*(uint16_t*)(o) = (uint16_t)_InterlockedExchangeAdd16((volatile short*)(p), -*(const short*)(v)))
#define CI_ATOMIC_AND16(p, v, o) (*(uint16_t*)(o) = (uint16_t)_InterlockedAnd16((volatile short*)(p), *(const short*)(v)))
#define CI_ATOMIC_OR16(p, v, o) (*(uint16_t*)(o) = (uint16_t)_InterlockedOr16((volatile short*)(p), *(const short*)(v)))
#define CI_ATOMIC_XOR16(p, v, o) (*(uint16_t*)(o) = (uint16_t)_InterlockedXor16((volatile short*)(p), *(const short*)(v)))
#define CI_ATOMIC_CAS16(p, e, d, weak, result) do { uint16_t exp = *(uint16_t*)(e); uint16_t old = (uint16_t)_InterlockedCompareExchange16((volatile short*)(p), *(const short*)(d), (short)exp); (result) = old == exp; *(uint16_t*)(e) = old; } while(0)
#if defined(_M_ARM64) || defined(_M_ARM64EC)
#define CI_ATOMIC_LOAD32(p, d) do { *(uint32_t*)(d) = (uint32_t)__iso_volatile_load32((const volatile __int32*)(p)); __dmb(_ARM64_BARRIER_ISH); } while(0)
#define CI_ATOMIC_STORE32(p, s) do { __dmb(_ARM64_BARRIER_ISH); __iso_volatile_store32((volatile __int32*)(p), *(const __int32*)(s)); __dmb(_ARM64_BARRIER_ISH); } while(0)
#else
#define CI_ATOMIC_LOAD32(p, d) do { *(uint32_t*)(d) = *(volatile uint32_t*)(p); _ReadWriteBarrier(); } while(0)
#define CI_ATOMIC_STORE32(p, s) (void)_InterlockedExchange((volatile long*)(p), *(const long*)(s))
#endif
#define CI_ATOMIC_XCHG32(p, v, o) (*(uint32_t*)(o) = (uint32_t)_InterlockedExchange((volatile long*)(p), *(const long*)(v)))
#define CI_ATOMIC_ADD32(p, v, o) (*(uint32_t*)(o) = (uint32_t)_InterlockedExchangeAdd((volatile long*)(p), *(const long*)(v)))
#define CI_ATOMIC_SUB32(p, v, o) (*(uint32_t*)(o) = (uint32_t)_InterlockedExchangeAdd((volatile long*)(p), (long)(0u - *(const uint32_t*)(v))))
#define CI_ATOMIC_AND32(p, v, o) (*(uint32_t*)(o) = (uint32_t)_InterlockedAnd((volatile long*)(p), *(const long*)(v)))
#define CI_ATOMIC_OR32(p, v, o) (*(uint32_t*)(o) = (uint32_t)_InterlockedOr((volatile long*)(p), *(const long*)(v)))
#define CI_ATOMIC_XOR32(p, v, o) (*(uint32_t*)(o) = (uint32_t)_InterlockedXor((volatile long*)(p), *(const long*)(v)))
#define CI_ATOMIC_CAS32(p, e, d, weak, result) do { uint32_t exp = *(uint32_t*)(e); uint32_t old = (uint32_t)_InterlockedCompareExchange((volatile long*)(p), *(const long*)(d), (long)exp); (result) = old == exp; *(uint32_t*)(e) = old; } while(0)
#if defined(_M_ARM64) || defined(_M_ARM64EC)
#define CI_ATOMIC_LOAD64(p, d) do { *(uint64_t*)(d) = (uint64_t)__iso_volatile_load64((const volatile __int64*)(p)); __dmb(_ARM64_BARRIER_ISH); } while(0)
#define CI_ATOMIC_STORE64(p, s) do { __dmb(_ARM64_BARRIER_ISH); __iso_volatile_store64((volatile __int64*)(p), *(const __int64*)(s)); __dmb(_ARM64_BARRIER_ISH); } while(0)
#else
#define CI_ATOMIC_LOAD64(p, d) do { *(uint64_t*)(d) = *(volatile uint64_t*)(p); _ReadWriteBarrier(); } while(0)
#define CI_ATOMIC_STORE64(p, s) (void)_InterlockedExchange64((volatile __int64*)(p), *(const __int64*)(s))
#endif
#define CI_ATOMIC_XCHG64(p, v, o) (*(uint64_t*)(o) = (uint64_t)_InterlockedExchange64((volatile __int64*)(p), *(const __int64*)(v)))
#define CI_ATOMIC_ADD64(p, v, o) (*(uint64_t*)(o) = (uint64_t)_InterlockedExchangeAdd64((volatile __int64*)(p), *(const __int64*)(v)))
#define CI_ATOMIC_SUB64(p, v, o) (*(uint64_t*)(o) = (uint64_t)_InterlockedExchangeAdd64((volatile __int64*)(p), (__int64)(UINT64_C(0) - *(const uint64_t*)(v))))
#define CI_ATOMIC_AND64(p, v, o) (*(uint64_t*)(o) = (uint64_t)_InterlockedAnd64((volatile __int64*)(p), *(const __int64*)(v)))
#define CI_ATOMIC_OR64(p, v, o) (*(uint64_t*)(o) = (uint64_t)_InterlockedOr64((volatile __int64*)(p), *(const __int64*)(v)))
#define CI_ATOMIC_XOR64(p, v, o) (*(uint64_t*)(o) = (uint64_t)_InterlockedXor64((volatile __int64*)(p), *(const __int64*)(v)))
#define CI_ATOMIC_CAS64(p, e, d, weak, result) do { uint64_t exp = *(uint64_t*)(e); uint64_t old = (uint64_t)_InterlockedCompareExchange64((volatile __int64*)(p), *(const __int64*)(d), (__int64)exp); (result) = old == exp; *(uint64_t*)(e) = old; } while(0)
#define CI_ATOMIC_LOAD128(p, d) do { __int64* out = (d); out[0] = 0; out[1] = 0; _InterlockedCompareExchange128((volatile __int64*)(p), 0, 0, out); } while(0)
#define CI_ATOMIC_XCHG128(p, v, o) do { __int64 ci_atomic_desired[2], previous[2] = {0}; memcpy(ci_atomic_desired, (v), 16); while(!_InterlockedCompareExchange128((volatile __int64*)(p), ci_atomic_desired[1], ci_atomic_desired[0], previous)){} memcpy((o), previous, 16); } while(0)
#define CI_ATOMIC_STORE128(p, v) do { __int64 ignored[2]; CI_ATOMIC_XCHG128(p, v, ignored); } while(0)
#define CI_ATOMIC_CAS128(p, e, d, weak, result) do { __int64 ci_atomic_desired[2]; memcpy(ci_atomic_desired, (d), 16); (result) = (_Bool)_InterlockedCompareExchange128((volatile __int64*)(p), ci_atomic_desired[1], ci_atomic_desired[0], (__int64*)(e)); } while(0)
#define CI_THREAD_FENCE() MemoryBarrier()
#else
#define CI_ATOMIC_LOAD8(p, d) __atomic_load((uint8_t*)(p), (uint8_t*)(d), __ATOMIC_SEQ_CST)
#define CI_ATOMIC_STORE8(p, s) __atomic_store((uint8_t*)(p), (uint8_t*)(s), __ATOMIC_SEQ_CST)
#define CI_ATOMIC_XCHG8(p, v, o) __atomic_exchange((uint8_t*)(p), (uint8_t*)(v), (uint8_t*)(o), __ATOMIC_SEQ_CST)
#define CI_ATOMIC_CAS8(p, e, d, weak, result) ((result) = __atomic_compare_exchange((uint8_t*)(p), (uint8_t*)(e), (uint8_t*)(d), weak, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST))
#define CI_ATOMIC_ADD8(p, v, o) (*(uint8_t*)(o) = __atomic_fetch_add((uint8_t*)(p), *(const uint8_t*)(v), __ATOMIC_SEQ_CST))
#define CI_ATOMIC_SUB8(p, v, o) (*(uint8_t*)(o) = __atomic_fetch_sub((uint8_t*)(p), *(const uint8_t*)(v), __ATOMIC_SEQ_CST))
#define CI_ATOMIC_AND8(p, v, o) (*(uint8_t*)(o) = __atomic_fetch_and((uint8_t*)(p), *(const uint8_t*)(v), __ATOMIC_SEQ_CST))
#define CI_ATOMIC_OR8(p, v, o) (*(uint8_t*)(o) = __atomic_fetch_or((uint8_t*)(p), *(const uint8_t*)(v), __ATOMIC_SEQ_CST))
#define CI_ATOMIC_XOR8(p, v, o) (*(uint8_t*)(o) = __atomic_fetch_xor((uint8_t*)(p), *(const uint8_t*)(v), __ATOMIC_SEQ_CST))
#define CI_ATOMIC_LOAD16(p, d) __atomic_load((uint16_t*)(p), (uint16_t*)(d), __ATOMIC_SEQ_CST)
#define CI_ATOMIC_STORE16(p, s) __atomic_store((uint16_t*)(p), (uint16_t*)(s), __ATOMIC_SEQ_CST)
#define CI_ATOMIC_XCHG16(p, v, o) __atomic_exchange((uint16_t*)(p), (uint16_t*)(v), (uint16_t*)(o), __ATOMIC_SEQ_CST)
#define CI_ATOMIC_CAS16(p, e, d, weak, result) ((result) = __atomic_compare_exchange((uint16_t*)(p), (uint16_t*)(e), (uint16_t*)(d), weak, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST))
#define CI_ATOMIC_ADD16(p, v, o) (*(uint16_t*)(o) = __atomic_fetch_add((uint16_t*)(p), *(const uint16_t*)(v), __ATOMIC_SEQ_CST))
#define CI_ATOMIC_SUB16(p, v, o) (*(uint16_t*)(o) = __atomic_fetch_sub((uint16_t*)(p), *(const uint16_t*)(v), __ATOMIC_SEQ_CST))
#define CI_ATOMIC_AND16(p, v, o) (*(uint16_t*)(o) = __atomic_fetch_and((uint16_t*)(p), *(const uint16_t*)(v), __ATOMIC_SEQ_CST))
#define CI_ATOMIC_OR16(p, v, o) (*(uint16_t*)(o) = __atomic_fetch_or((uint16_t*)(p), *(const uint16_t*)(v), __ATOMIC_SEQ_CST))
#define CI_ATOMIC_XOR16(p, v, o) (*(uint16_t*)(o) = __atomic_fetch_xor((uint16_t*)(p), *(const uint16_t*)(v), __ATOMIC_SEQ_CST))
#define CI_ATOMIC_LOAD32(p, d) __atomic_load((uint32_t*)(p), (uint32_t*)(d), __ATOMIC_SEQ_CST)
#define CI_ATOMIC_STORE32(p, s) __atomic_store((uint32_t*)(p), (uint32_t*)(s), __ATOMIC_SEQ_CST)
#define CI_ATOMIC_XCHG32(p, v, o) __atomic_exchange((uint32_t*)(p), (uint32_t*)(v), (uint32_t*)(o), __ATOMIC_SEQ_CST)
#define CI_ATOMIC_CAS32(p, e, d, weak, result) ((result) = __atomic_compare_exchange((uint32_t*)(p), (uint32_t*)(e), (uint32_t*)(d), weak, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST))
#define CI_ATOMIC_ADD32(p, v, o) (*(uint32_t*)(o) = __atomic_fetch_add((uint32_t*)(p), *(const uint32_t*)(v), __ATOMIC_SEQ_CST))
#define CI_ATOMIC_SUB32(p, v, o) (*(uint32_t*)(o) = __atomic_fetch_sub((uint32_t*)(p), *(const uint32_t*)(v), __ATOMIC_SEQ_CST))
#define CI_ATOMIC_AND32(p, v, o) (*(uint32_t*)(o) = __atomic_fetch_and((uint32_t*)(p), *(const uint32_t*)(v), __ATOMIC_SEQ_CST))
#define CI_ATOMIC_OR32(p, v, o) (*(uint32_t*)(o) = __atomic_fetch_or((uint32_t*)(p), *(const uint32_t*)(v), __ATOMIC_SEQ_CST))
#define CI_ATOMIC_XOR32(p, v, o) (*(uint32_t*)(o) = __atomic_fetch_xor((uint32_t*)(p), *(const uint32_t*)(v), __ATOMIC_SEQ_CST))
#define CI_ATOMIC_LOAD64(p, d) __atomic_load((uint64_t*)(p), (uint64_t*)(d), __ATOMIC_SEQ_CST)
#define CI_ATOMIC_STORE64(p, s) __atomic_store((uint64_t*)(p), (uint64_t*)(s), __ATOMIC_SEQ_CST)
#define CI_ATOMIC_XCHG64(p, v, o) __atomic_exchange((uint64_t*)(p), (uint64_t*)(v), (uint64_t*)(o), __ATOMIC_SEQ_CST)
#define CI_ATOMIC_CAS64(p, e, d, weak, result) ((result) = __atomic_compare_exchange((uint64_t*)(p), (uint64_t*)(e), (uint64_t*)(d), weak, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST))
#define CI_ATOMIC_ADD64(p, v, o) (*(uint64_t*)(o) = __atomic_fetch_add((uint64_t*)(p), *(const uint64_t*)(v), __ATOMIC_SEQ_CST))
#define CI_ATOMIC_SUB64(p, v, o) (*(uint64_t*)(o) = __atomic_fetch_sub((uint64_t*)(p), *(const uint64_t*)(v), __ATOMIC_SEQ_CST))
#define CI_ATOMIC_AND64(p, v, o) (*(uint64_t*)(o) = __atomic_fetch_and((uint64_t*)(p), *(const uint64_t*)(v), __ATOMIC_SEQ_CST))
#define CI_ATOMIC_OR64(p, v, o) (*(uint64_t*)(o) = __atomic_fetch_or((uint64_t*)(p), *(const uint64_t*)(v), __ATOMIC_SEQ_CST))
#define CI_ATOMIC_XOR64(p, v, o) (*(uint64_t*)(o) = __atomic_fetch_xor((uint64_t*)(p), *(const uint64_t*)(v), __ATOMIC_SEQ_CST))
#define CI_ATOMIC_LOAD128(p, d) __atomic_load((CiAtomic16*)(p), (CiAtomic16*)(d), __ATOMIC_SEQ_CST)
#define CI_ATOMIC_STORE128(p, s) __atomic_store((CiAtomic16*)(p), (CiAtomic16*)(s), __ATOMIC_SEQ_CST)
#define CI_ATOMIC_XCHG128(p, v, o) __atomic_exchange((CiAtomic16*)(p), (CiAtomic16*)(v), (CiAtomic16*)(o), __ATOMIC_SEQ_CST)
#define CI_ATOMIC_CAS128(p, e, d, weak, result) ((result) = __atomic_compare_exchange((CiAtomic16*)(p), (CiAtomic16*)(e), (CiAtomic16*)(d), weak, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST))
#define CI_THREAD_FENCE() __atomic_thread_fence(__ATOMIC_SEQ_CST)
#endif

force_inline
int
_ci_interp_step(CiInterpreter* ci, CiInterpFrame* frame, CiInterpFrame*_Nullable*_Nonnull child){
    if(frame->pc >= frame->op_count)
        return 0;
    const CiOp* op = &frame->ops[frame->pc];
    switch(op->kind){
        case CI_OP_COUNT: break;
#if (defined __clang__ || defined __GNUC__) && !defined __DRC__
#define CI_ALIGNED(p, a) __builtin_assume_aligned((p), (a))
#else
#define CI_ALIGNED(p, a) (p)
#endif

#define CI_CONST_CASE(name, bytes, da, sa) \
        case CI_OP_##name: { \
            memcpy((char*)frame->slots + op->constant.slot, op->constant.immediate, bytes); \
            frame->pc++; \
            return 0; \
        }
        CI_CONST_OPS(CI_CONST_CASE)
#undef CI_CONST_CASE

#define CI_LOAD_CASE(name, bytes, da, sa) \
        case CI_OP_##name: { \
            char* ptr; memcpy(&ptr, (char*)frame->slots + op->load.src, sizeof ptr); \
            memmove(CI_ALIGNED((char*)frame->slots + op->load.slot, sa), CI_ALIGNED(ptr + op->load.offset, da), bytes); \
            frame->pc++; \
            return 0; \
        }
        CI_LOAD_OPS(CI_LOAD_CASE)
#undef CI_LOAD_CASE

#define CI_STORE_CASE(name, bytes, da, sa) \
        case CI_OP_##name: { \
            char* ptr; memcpy(&ptr, (char*)frame->slots + op->store.slot, sizeof ptr); \
            memmove(CI_ALIGNED(ptr + op->store.offset, da), CI_ALIGNED((char*)frame->slots + op->store.src, sa), bytes); \
            frame->pc++; \
            return 0; \
        }
        CI_STORE_OPS(CI_STORE_CASE)
#undef CI_STORE_CASE

#define CI_STORE_IMM_CASE(name, bytes, da, sa) \
        case CI_OP_##name: { \
            char* ptr; memcpy(&ptr, (char*)frame->slots + op->store_imm.slot, sizeof ptr); \
            memcpy(CI_ALIGNED(ptr + op->store_imm.offset, da), &op->store_imm.immediate, bytes); \
            frame->pc++; \
            return 0; \
        }
        CI_STORE_IMM_OPS(CI_STORE_IMM_CASE)
#undef CI_STORE_IMM_CASE

#define CI_ZERO_CASE(name, bytes, da, sa) \
        case CI_OP_##name: { \
            char* ptr; memcpy(&ptr, (char*)frame->slots + op->zero.slot, sizeof ptr); \
            memset(CI_ALIGNED(ptr + op->zero.offset, da), 0, bytes); \
            frame->pc++; \
            return 0; \
        }
        CI_ZERO_OPS(CI_ZERO_CASE)
#undef CI_ZERO_CASE

#define CI_COPY_CASE(name, bytes, da, sa) \
        case CI_OP_##name: { \
            memmove(CI_ALIGNED((char*)frame->slots + op->copy.slot, da), CI_ALIGNED((char*)frame->slots + op->copy.src, sa), bytes); \
            frame->pc++; \
            return 0; \
        }
        CI_COPY_OPS(CI_COPY_CASE)
#undef CI_COPY_CASE

#define CI_MEMCOPY_CASE(name, bytes, da, sa) \
        case CI_OP_##name: { \
            char *dst, *src; \
            memcpy(&dst, (char*)frame->slots + op->memcopy.slot, sizeof dst); \
            memcpy(&src, (char*)frame->slots + op->memcopy.src, sizeof src); \
            memmove(CI_ALIGNED(dst + op->memcopy.offset, da), CI_ALIGNED(src + op->memcopy.src_offset, sa), bytes); \
            frame->pc++; \
            return 0; \
        }
        CI_MEMCOPY_OPS(CI_MEMCOPY_CASE)
#undef CI_MEMCOPY_CASE

#define CI_EXTEND_CASE(name, from, to, value) \
        case CI_OP_##name: { \
            const void* src = (char*)frame->slots + op->convert.src; \
            CiUint128 result = value; \
            ci_uint128_write((char*)frame->slots + op->convert.slot, to, result); \
            frame->pc++; \
            return 0; \
        }
        CI_EXTEND_OPS(CI_EXTEND_CASE)
#undef CI_EXTEND_CASE

#define CI_CHECKED_CASE(name, type, wide_type, operation) \
        case CI_OP_##name: { \
            type a, b; \
            memcpy(&a, (char*)frame->slots + op->checked.src, sizeof a); \
            memcpy(&b, (char*)frame->slots + op->checked.src2, sizeof b); \
            wide_type wide; \
            _Bool overflow = operation((wide_type)a, (wide_type)b, &wide); \
            type result = (type)wide; \
            overflow |= wide != (wide_type)result; \
            memcpy((char*)frame->slots + op->checked.result, &result, sizeof result); \
            *((unsigned char*)frame->slots + op->checked.overflow) = overflow; \
            frame->pc++; \
            return 0; \
        }
        CI_CHECKED_OPS(CI_CHECKED_CASE)
#undef CI_CHECKED_CASE

#define CI_OVERFLOW_CASE(name, bits, uns) \
        case CI_OP_##name: { \
            CiUint128 value; \
            ci_uint128_read(&value, (char*)frame->slots + op->checked.src, 16); \
            *((unsigned char*)frame->slots + op->checked.overflow) = !ci_uint128_fits_integer(value, bits, uns); \
            frame->pc++; \
            return 0; \
        }
        CI_OVERFLOW_OPS(CI_OVERFLOW_CASE)
#undef CI_OVERFLOW_CASE

#define CI_TRUTH_CASE(name, test) \
        case CI_OP_##name: { \
            const void* src = (char*)frame->slots + op->istrue.src; \
            *((unsigned char*)frame->slots + op->istrue.slot) = (test); \
            frame->pc++; \
            return 0; \
        }
        CI_TRUTH_OPS(CI_TRUTH_CASE)
#undef CI_TRUTH_CASE

#define CI_ITOF_CASE(name, type, expression, write) \
        case CI_OP_##name: { \
            const void* src = (char*)frame->slots + op->convert.src; \
            void* dest = (char*)frame->slots + op->convert.slot; \
            type result = expression; \
            write; \
            frame->pc++; \
            return 0; \
        }
        CI_ITOF_OPS(CI_ITOF_CASE)
#undef CI_ITOF_CASE

#define CI_FTOI_CASE(name, bytes, expression) \
        case CI_OP_##name: { \
            const void* src = (char*)frame->slots + op->convert.src; \
            CiUint128 result = expression; \
            ci_uint128_write((char*)frame->slots + op->convert.slot, bytes, result); \
            frame->pc++; \
            return 0; \
        }
        CI_FTOI_OPS(CI_FTOI_CASE)
#undef CI_FTOI_CASE

#define CI_FTOF_CASE(name, type, expression, write) \
        case CI_OP_##name: { \
            const void* src = (char*)frame->slots + op->convert.src; \
            void* dest = (char*)frame->slots + op->convert.slot; \
            type result = expression; \
            write; \
            frame->pc++; \
            return 0; \
        }
        CI_FTOF_OPS(CI_FTOF_CASE)
#undef CI_FTOF_CASE

#define CI_ABI_MOVE_CASE(name, state, bytes, alignment, transfer) \
        case CI_OP_##name: { \
            void* reg = CI_ALIGNED((char*)(state) + op->abi_move.reg.capture_offset, alignment); \
            void* slot = CI_ALIGNED((char*)frame->slots + op->abi_move.slot, alignment); \
            transfer; \
            frame->pc++; \
            return 0; \
        }
        CI_ABI_MOVE_OPS(CI_ABI_MOVE_CASE)
#undef CI_ABI_MOVE_CASE

#define CI_ABI_TRANSFER_CASE(name, transfer) \
        case CI_OP_##name: { \
            transfer \
            frame->pc++; \
            return 0; \
        }
        CI_ABI_TRANSFER_OPS(CI_ABI_TRANSFER_CASE)
#undef CI_ABI_TRANSFER_CASE


#define CI_JUMP_FALSE_CASE(name, bytes) \
        case CI_OP_##name: { \
            CiUint128 value; \
            ci_uint128_read(&value, (char*)frame->slots + op->jump_false.slot, bytes); \
            frame->pc = !ci_uint128_nonzero(value) ? op->jump_false.jump : frame->pc + 1; \
            return 0; \
        }
        CI_JUMP_FALSE_OPS(CI_JUMP_FALSE_CASE)
#undef CI_JUMP_FALSE_CASE

#define CI_JUMP_TRUE_CASE(name, bytes) \
        case CI_OP_##name: { \
            CiUint128 value; \
            ci_uint128_read(&value, (char*)frame->slots + op->jump_true.slot, bytes); \
            frame->pc = ci_uint128_nonzero(value) ? op->jump_true.jump : frame->pc + 1; \
            return 0; \
        }
        CI_JUMP_TRUE_OPS(CI_JUMP_TRUE_CASE)
#undef CI_JUMP_TRUE_CASE

#define CI_BSWAP_CASE(name, bytes, expression) \
        case CI_OP_##name: { \
            const void* src = (char*)frame->slots + op->bswap.src; \
            ci_write_uint((char*)frame->slots + op->bswap.slot, bytes, expression); \
            frame->pc++; \
            return 0; \
        }
        CI_BSWAP_OPS(CI_BSWAP_CASE)
#undef CI_BSWAP_CASE

#define CI_BOUNDS_CASE(name, bytes, comparison, signed_index) \
        case CI_OP_##name: { \
            uint64_t index = ci_read_uint((char*)frame->slots + op->bounds.src, bytes); \
            uint64_t length = ci_read_uint((char*)frame->slots + op->bounds.src2, bytes); \
            if(index comparison length){ \
                const char* close = op->bounds.inclusive ? "]" : ")"; \
                if(signed_index) return ci_error(ci, op->loc, "array subscript out of bounds: index %lld not in [0, %llu%s", (long long)index, (unsigned long long)length, close); \
                return ci_error(ci, op->loc, "array subscript out of bounds: index %llu not in [0, %llu%s", (unsigned long long)index, (unsigned long long)length, close); \
            } \
            frame->pc++; \
            return 0; \
        }
        CI_BOUNDS_OPS(CI_BOUNDS_CASE)
#undef CI_BOUNDS_CASE

#define CI_INDEX_CASE(name, ptr_size, read_index) \
        case CI_OP_##name: { \
            const void* src = (char*)frame->slots + op->index.index; \
            uint64_t index = read_index; \
            uint64_t base = ci_read_uint((char*)frame->slots + op->index.base, ptr_size); \
            ci_write_uint((char*)frame->slots + op->index.slot, ptr_size, base + index * op->index.scale); \
            frame->pc++; \
            return 0; \
        }
        CI_INDEX_OPS(CI_INDEX_CASE)
#undef CI_INDEX_CASE

#define CI_LOAD_BITFIELD_CASE(name, size, value, alignment) \
        case CI_OP_##name: { \
            char* ptr; \
            memcpy(&ptr, (char*)frame->slots + op->load_bf.src, sizeof ptr); \
            uint64_t result = value; \
            memcpy(CI_ALIGNED((char*)frame->slots + op->load_bf.slot, alignment), &result, size); \
            frame->pc++; \
            return 0; \
        }
        CI_LOAD_BITFIELD_OPS(CI_LOAD_BITFIELD_CASE)
#undef CI_LOAD_BITFIELD_CASE

#define CI_LOAD_BITFIELD_128_CASE(name, uns, alignment) \
        case CI_OP_##name: { \
            char* ptr; \
            memcpy(&ptr, (char*)frame->slots + op->load_bf.src, sizeof ptr); \
            CiUint128 result = ci_bitfield_read128(CI_ALIGNED(ptr + op->load_bf.offset, alignment), op->load_bf.bit_offset, op->load_bf.bit_width); \
            result = ci_bitfield_extend128(result, op->load_bf.bit_width, !uns); \
            memcpy(CI_ALIGNED((char*)frame->slots + op->load_bf.slot, alignment), &result, 16); \
            frame->pc++; \
            return 0; \
        }
        CI_LOAD_BITFIELD_128_OPS(CI_LOAD_BITFIELD_128_CASE)
#undef CI_LOAD_BITFIELD_128_CASE

#define CI_LOAD_BITFIELD_BYTES_CASE(name, size, value) \
        case CI_OP_##name: { \
            char* ptr; \
            memcpy(&ptr, (char*)frame->slots + op->load_bf.src, sizeof ptr); \
            uint64_t result = value; \
            memcpy((char*)frame->slots + op->load_bf.slot, &result, size); \
            frame->pc++; \
            return 0; \
        }
        CI_LOAD_BITFIELD_BYTES_OPS(CI_LOAD_BITFIELD_BYTES_CASE)
#undef CI_LOAD_BITFIELD_BYTES_CASE

#define CI_LOAD_BITFIELD_BYTES_128_CASE(name, uns) \
        case CI_OP_##name: { \
            char* ptr; \
            memcpy(&ptr, (char*)frame->slots + op->load_bf.src, sizeof ptr); \
            CiUint128 result = ci_bitfield_read128_bytes(ptr + op->load_bf.offset, op->load_bf.bit_offset, op->load_bf.bit_width); \
            result = ci_bitfield_extend128(result, op->load_bf.bit_width, !uns); \
            memcpy((char*)frame->slots + op->load_bf.slot, &result, 16); \
            frame->pc++; \
            return 0; \
        }
        CI_LOAD_BITFIELD_BYTES_128_OPS(CI_LOAD_BITFIELD_BYTES_128_CASE)
#undef CI_LOAD_BITFIELD_BYTES_128_CASE

#define CI_STORE_BITFIELD_CASE(name, size, alignment) \
        case CI_OP_##name: { \
            char* ptr; \
            memcpy(&ptr, (char*)frame->slots + op->store_bf.slot, sizeof ptr); \
            uint64_t value = ci_read_uint(CI_ALIGNED((char*)frame->slots + op->store_bf.src, alignment), size); \
            ci_bitfield_write(CI_ALIGNED(ptr + op->store_bf.offset, alignment), size, op->store_bf.bit_offset, op->store_bf.bit_width, value); \
            frame->pc++; \
            return 0; \
        }
        CI_STORE_BITFIELD_OPS(CI_STORE_BITFIELD_CASE)
#undef CI_STORE_BITFIELD_CASE

#define CI_STORE_BITFIELD_128_CASE(name, alignment) \
        case CI_OP_##name: { \
            char* ptr; \
            memcpy(&ptr, (char*)frame->slots + op->store_bf.slot, sizeof ptr); \
            CiUint128 value = ci_read_u128(CI_ALIGNED((char*)frame->slots + op->store_bf.src, alignment)); \
            ci_bitfield_write_128(CI_ALIGNED(ptr + op->store_bf.offset, alignment), op->store_bf.bit_offset, op->store_bf.bit_width, value); \
            frame->pc++; \
            return 0; \
        }
        CI_STORE_BITFIELD_128_OPS(CI_STORE_BITFIELD_128_CASE)
#undef CI_STORE_BITFIELD_128_CASE

#define CI_STORE_BITFIELD_BYTES_CASE(name, size) \
        case CI_OP_##name: { \
            char* ptr; \
            memcpy(&ptr, (char*)frame->slots + op->store_bf.slot, sizeof ptr); \
            uint64_t value = ci_read_uint((char*)frame->slots + op->store_bf.src, size); \
            ci_bitfield_write_bytes(ptr + op->store_bf.offset, op->store_bf.bit_offset, op->store_bf.bit_width, value); \
            frame->pc++; \
            return 0; \
        }
        CI_STORE_BITFIELD_BYTES_OPS(CI_STORE_BITFIELD_BYTES_CASE)
#undef CI_STORE_BITFIELD_BYTES_CASE

#define CI_STORE_BITFIELD_BYTES_128_CASE(name, unused) \
        case CI_OP_##name: { \
            char* ptr; \
            memcpy(&ptr, (char*)frame->slots + op->store_bf.slot, sizeof ptr); \
            CiUint128 value = ci_read_u128((char*)frame->slots + op->store_bf.src); \
            ci_bitfield_write_128_bytes(ptr + op->store_bf.offset, op->store_bf.bit_offset, op->store_bf.bit_width, value); \
            frame->pc++; \
            return 0; \
        }
        CI_STORE_BITFIELD_BYTES_128_OPS(CI_STORE_BITFIELD_BYTES_128_CASE)
#undef CI_STORE_BITFIELD_BYTES_128_CASE

#define CI_CALL_CASE(name, resolve) \
        case CI_OP_##name: { \
            CiCallDescriptor* d = op->call.descrip; \
            NcCallState* state = &frame->stack->registers; \
            void (*fn)(void); \
            const CcFunc* func; \
            resolve \
            if(func && func->defined){ \
                int err = ci_make_abi_call_frame(ci, frame, func, op, child); \
                return err ? err : CI_STEP_ENTER_FRAME; \
            } \
            if(!fn) return ci_error(ci, op->loc, "call through null function pointer"); \
            NcCallState native = *state; \
            int err = d->invoke(d->abi, fn, &native); \
            if(err) return err; \
            size_t offset = nc_capture_result_offset(d->abi->target); \
            memcpy((char*)state + offset, (char*)&native + offset, nc_capture_size(d->abi->target) - offset); \
            frame->pc++; \
            return 0; \
        }
        CI_CALL_OPS(CI_CALL_CASE)
#undef CI_CALL_CASE

#define CI_SWITCH_DENSE_CASE(name, unused) \
        case CI_OP_##name: { \
            uint64_t value = ci_read_uint((char*)frame->slots + op->switch_.slot, 8); \
            const CiSwitchTable* table = op->switch_.table; \
            uint64_t index = value - table->data[0].low; \
            frame->pc = index < table->count ? table->data[index].target : op->switch_.jump; \
            return 0; \
        }
        CI_SWITCH_DENSE_OPS(CI_SWITCH_DENSE_CASE)
#undef CI_SWITCH_DENSE_CASE

#define CI_SWITCH_RANGE_CASE(name, sign) \
        case CI_OP_##name: { \
            uint64_t value = ci_read_uint((char*)frame->slots + op->switch_.slot, 8) ^ (sign); \
            const CiSwitchTable* table = op->switch_.table; \
            size_t lo = 0, hi = table->count; \
            while(lo < hi){ \
                size_t mid = lo + (hi - lo) / 2; \
                if((table->data[mid].high ^ (sign)) < value) lo = mid + 1; \
                else if((table->data[mid].low ^ (sign)) > value) hi = mid; \
                else { frame->pc = table->data[mid].target; return 0; } \
            } \
            frame->pc = op->switch_.jump; \
            return 0; \
        }
        CI_SWITCH_RANGE_OPS(CI_SWITCH_RANGE_CASE)
#undef CI_SWITCH_RANGE_CASE

#define CI_BUILTIN_CASE(name, action) \
        case CI_OP_##name: { \
            action \
            frame->pc++; \
            return 0; \
        }
        CI_BUILTIN_OPS(CI_BUILTIN_CASE)
#undef CI_BUILTIN_CASE

#define CI_ALLOCA_CASE(name, allocate) \
        case CI_OP_##name: { \
            size_t size; \
            memcpy(&size, (char*)frame->slots + op->alloca.src, sizeof size); \
            CiStack* stack; \
            int err = ci_frame_stack(ci, frame, &stack); \
            if(err) return err; \
            void* ptr = allocate(stack, size, op->alloca.alignment); \
            if(!ptr) return CI_OOM_ERROR; \
            memcpy((char*)frame->slots + op->alloca.slot, &ptr, sizeof ptr); \
            frame->pc++; \
            return 0; \
        }
        CI_ALLOCA_OPS(CI_ALLOCA_CASE)
#undef CI_ALLOCA_CASE

#define CI_FENCE_CASE(name, fence) \
        case CI_OP_##name: { \
            fence \
            frame->pc++; \
            return 0; \
        }
        CI_FENCE_OPS(CI_FENCE_CASE)
#undef CI_FENCE_CASE

#define CI_BIT_COUNT_CASE(name, bits, mask, expression) \
        case CI_OP_##name: { \
            CiUint128 value; \
            ci_uint128_read(&value, (char*)frame->slots + op->bit_builtin.src, bits / 8); \
            uint64_t lo = ci_uint128_lo(value), hi = ci_uint128_hi(value); \
            (void)lo; (void)hi; \
            uint32_t result = (uint32_t)(expression); \
            memcpy((char*)frame->slots + op->bit_builtin.slot, &result, 4); \
            frame->pc++; \
            return 0; \
        }
        CI_BIT_COUNT_OPS(CI_BIT_COUNT_CASE)
#undef CI_BIT_COUNT_CASE

#define CI_SINGLE_BIT_CASE(name, bits) \
        case CI_OP_##name: { \
            CiUint128 value; \
            ci_uint128_read(&value, (char*)frame->slots + op->bit_builtin.src, bits / 8); \
            uint32_t result = popcount_64(ci_uint128_lo(value)) + popcount_64(ci_uint128_hi(value)) == 1; \
            memcpy((char*)frame->slots + op->bit_builtin.slot, &result, 4); \
            frame->pc++; \
            return 0; \
        }
        CI_SINGLE_BIT_OPS(CI_SINGLE_BIT_CASE)
#undef CI_SINGLE_BIT_CASE

#define CI_BIT_CEIL_CASE(name, bits, mask, calculate) \
        case CI_OP_##name: { \
            CiUint128 value; \
            ci_uint128_read(&value, (char*)frame->slots + op->bit_builtin.src, bits / 8); \
            uint64_t lo = ci_uint128_lo(value), hi = ci_uint128_hi(value); \
            CiUint128 result = ci_uint128_from_uint64(0); \
            calculate \
            ci_uint128_write((char*)frame->slots + op->bit_builtin.slot, bits / 8, result); \
            frame->pc++; \
            return 0; \
        }
        CI_BIT_CEIL_OPS(CI_BIT_CEIL_CASE)
#undef CI_BIT_CEIL_CASE

#define CI_BIT_FLOOR_CASE(name, bits, mask, calculate) \
        case CI_OP_##name: { \
            CiUint128 value; \
            ci_uint128_read(&value, (char*)frame->slots + op->bit_builtin.src, bits / 8); \
            uint64_t lo = ci_uint128_lo(value), hi = ci_uint128_hi(value); \
            CiUint128 result = ci_uint128_from_uint64(0); \
            calculate \
            ci_uint128_write((char*)frame->slots + op->bit_builtin.slot, bits / 8, result); \
            frame->pc++; \
            return 0; \
        }
        CI_BIT_FLOOR_OPS(CI_BIT_FLOOR_CASE)
#undef CI_BIT_FLOOR_CASE

#define CI_ROTATE_CASE(name, bits, left, right) \
        case CI_OP_##name: { \
            CiUint128 value; \
            ci_uint128_read(&value, (char*)frame->slots + op->bit_builtin.src, bits / 8); \
            uint32_t shift = (uint32_t)(ci_read_uint((char*)frame->slots + op->bit_builtin.src2, 8) % bits); \
            CiUint128 result = ci_uint128_or(left, right); \
            ci_uint128_write((char*)frame->slots + op->bit_builtin.slot, bits / 8, result); \
            frame->pc++; \
            return 0; \
        }
        CI_ROTATE_OPS(CI_ROTATE_CASE)
#undef CI_ROTATE_CASE

#define CI_CRC_CASE(name, bytes) \
        case CI_OP_##name: { \
            uint32_t seed = (uint32_t)ci_read_uint((char*)frame->slots + op->bit_builtin.src, 4); \
            uint64_t value = ci_read_uint((char*)frame->slots + op->bit_builtin.src2, bytes); \
            uint32_t result = drp_crc32c(seed, value, bytes); \
            memcpy((char*)frame->slots + op->bit_builtin.slot, &result, 4); \
            frame->pc++; \
            return 0; \
        }
        CI_CRC_OPS(CI_CRC_CASE)
#undef CI_CRC_CASE

#define CI_ATOMIC_LOAD_CASE(name, bits) \
        case CI_OP_##name: { \
            char* ptr; \
            memcpy(&ptr, (char*)frame->slots + op->atomic_load.src, sizeof ptr); \
            ptr += op->atomic_load.offset; \
            void* dest = (char*)frame->slots + op->atomic_load.slot; \
            CI_ATOMIC_LOAD##bits(ptr, dest); \
            frame->pc++; \
            return 0; \
        }
        CI_ATOMIC_LOAD_OPS(CI_ATOMIC_LOAD_CASE)
#undef CI_ATOMIC_LOAD_CASE

#define CI_ATOMIC_STORE_CASE(name, bits) \
        case CI_OP_##name: { \
            char* ptr; \
            memcpy(&ptr, (char*)frame->slots + op->atomic_store.slot, sizeof ptr); \
            ptr += op->atomic_store.offset; \
            void* src = (char*)frame->slots + op->atomic_store.src; \
            CI_ATOMIC_STORE##bits(ptr, src); \
            frame->pc++; \
            return 0; \
        }
        CI_ATOMIC_STORE_OPS(CI_ATOMIC_STORE_CASE)
#undef CI_ATOMIC_STORE_CASE

#define CI_ATOMIC_RMW_CASE(name, action) \
        case CI_OP_##name: { \
            char* ptr; \
            memcpy(&ptr, (char*)frame->slots + op->atomic_rmw.src, sizeof ptr); \
            ptr += op->atomic_rmw.offset; \
            void* old = (char*)frame->slots + op->atomic_rmw.slot; \
            void* value = (char*)frame->slots + op->atomic_rmw.src2; \
            action(ptr, value, old); \
            frame->pc++; \
            return 0; \
        }
        CI_ATOMIC_RMW_OPS(CI_ATOMIC_RMW_CASE)
#undef CI_ATOMIC_RMW_CASE

#define CI_ATOMIC_CAS_CASE(name, bits, weak) \
        case CI_OP_##name: { \
            char* ptr; \
            memcpy(&ptr, (char*)frame->slots + op->atomic_cas.src, sizeof ptr); \
            ptr += op->atomic_cas.offset; \
            void* expected = (char*)frame->slots + op->atomic_cas.expected; \
            void* desired = (char*)frame->slots + op->atomic_cas.desired; \
            _Bool result; \
            CI_ATOMIC_CAS##bits(ptr, expected, desired, weak, result); \
            *((unsigned char*)frame->slots + op->atomic_cas.slot) = result; \
            frame->pc++; \
            return 0; \
        }
        CI_ATOMIC_CAS_OPS(CI_ATOMIC_CAS_CASE)
#undef CI_ATOMIC_CAS_CASE

#define CI_VA_SYSV_CASE(name, unused) \
        case CI_OP_##name: { \
            void* ap; \
            memcpy(&ap, (char*)frame->slots + op->va_arg_.src, sizeof ap); \
            void** pointers = (void**)((char*)frame->slots + op->va_arg_.pointers); \
            NcSysvVaListTag* va = ap; \
            unsigned ng = op->va_arg_.gp, nf = op->va_arg_.fp; \
            if(va->gp_offset + ng * 8 <= 48 && va->fp_offset + nf * 16 <= 176){ \
                pointers[0] = (char*)va->reg_save_area + va->gp_offset; \
                pointers[1] = (char*)va->reg_save_area + va->fp_offset; \
                va->gp_offset += ng * 8; \
                va->fp_offset += nf * 16; \
                frame->pc++; \
                return 0; \
            } \
            void** stack = &va->overflow_arg_area; \
            void* source = ci_va_stack_source(stack, op->va_arg_.stack_mask, op->va_arg_.stack_stride); \
            pointers[0] = source; \
            frame->pc += 2 + op->va_arg_.count; \
            return 0; \
        }
        CI_VA_SYSV_OPS(CI_VA_SYSV_CASE)
#undef CI_VA_SYSV_CASE

#define CI_VA_ARM_CASE(name, align_gp) \
        case CI_OP_##name: { \
            void* ap; \
            memcpy(&ap, (char*)frame->slots + op->va_arg_.src, sizeof ap); \
            void** pointers = (void**)((char*)frame->slots + op->va_arg_.pointers); \
            NcAapcs64VaList* va = ap; \
            unsigned ng = op->va_arg_.gp, nf = op->va_arg_.fp; \
            int go = va->__gr_offs, fo = va->__vr_offs; \
            align_gp \
            if((!ng || (go < 0 && go + (int)ng * 8 <= 0)) && (!nf || (fo < 0 && fo + (int)nf * 16 <= 0))){ \
                pointers[0] = ng ? (char*)va->__gr_top + go : NULL; \
                pointers[1] = nf ? (char*)va->__vr_top + fo : NULL; \
                if(ng) va->__gr_offs = go + (int)ng * 8; \
                if(nf) va->__vr_offs = fo + (int)nf * 16; \
                frame->pc++; \
                return 0; \
            } \
            if(ng) va->__gr_offs = 0; \
            if(nf) va->__vr_offs = 0; \
            void** stack = &va->__stack; \
            void* source = ci_va_stack_source(stack, op->va_arg_.stack_mask, op->va_arg_.stack_stride); \
            pointers[0] = source; \
            frame->pc += 2 + op->va_arg_.count; \
            return 0; \
        }
        CI_VA_ARM_OPS(CI_VA_ARM_CASE)
#undef CI_VA_ARM_CASE

#define CI_VA_ARM_INDIRECT_CASE(name, unused) \
        case CI_OP_##name: { \
            void* ap; \
            memcpy(&ap, (char*)frame->slots + op->va_arg_.src, sizeof ap); \
            void** pointers = (void**)((char*)frame->slots + op->va_arg_.pointers); \
            NcAapcs64VaList* va = ap; \
            void* location; \
            if(va->__gr_offs < 0 && va->__gr_offs + 8 <= 0){ \
                location = (char*)va->__gr_top + va->__gr_offs; \
                va->__gr_offs += 8; \
            } \
            else { \
                va->__gr_offs = 0; \
                location = ci_va_stack_source(&va->__stack, 7, 8); \
            } \
            void* source; \
            memcpy(&source, location, sizeof source); \
            pointers[0] = source; \
            frame->pc++; \
            return 0; \
        }
        CI_VA_ARM_INDIRECT_OPS(CI_VA_ARM_INDIRECT_CASE)
#undef CI_VA_ARM_INDIRECT_CASE

#define CI_VA_STACK_CASE(name, stack_address, indirection) \
        case CI_OP_##name: { \
            void* ap; \
            memcpy(&ap, (char*)frame->slots + op->va_arg_.src, sizeof ap); \
            void** pointers = (void**)((char*)frame->slots + op->va_arg_.pointers); \
            void* source = ci_va_stack_source(stack_address, op->va_arg_.stack_mask, op->va_arg_.stack_stride); \
            indirection \
            pointers[0] = source; \
            frame->pc++; \
            return 0; \
        }
        CI_VA_STACK_OPS(CI_VA_STACK_CASE)
#undef CI_VA_STACK_CASE

#undef CI_ALIGNED
        case CI_OP_CAPTURE_BACKTRACE: {
            void* result = (char*)frame->slots + op->capture_backtrace.slot;
            _Static_assert(sizeof(SrcLoc) == sizeof(void*), "backtrace entry size");
            void** buffer;
            int capacity, count = 0;
            CI_INLINE_MEMCPY(&buffer, (char*)frame->slots + op->capture_backtrace.buffer, sizeof buffer);
            CI_INLINE_MEMCPY(&capacity, (char*)frame->slots + op->capture_backtrace.capacity, sizeof capacity);
            if(capacity > 0 && !buffer)
                return ci_error(ci, op->loc, "__builtin_capture_backtrace buffer must not be NULL");
            for(CiInterpFrame* f = frame; f && count < capacity; f = f->parent){
                if(!f->ops || f->pc >= f->op_count) continue;
                CI_INLINE_MEMCPY(&buffer[count++], &f->ops[f->pc].loc, sizeof(SrcLoc));
            }
            CI_INLINE_MEMCPY(result, &count, sizeof count);
            frame->pc++;
            return 0;
        }

#define CI_INTEGER(name, suffix, bits, type, rhs, expression) \
        case CI_OP_##name##suffix: { \
            type a = (type)ci_read_uint((char*)frame->slots + op->alu.src, bits / 8); \
            type b = (type)(rhs); \
            (void)b; \
            uint64_t result = (expression); \
            ci_write_uint((char*)frame->slots + op->alu.slot, bits / 8, result); \
            frame->pc++; \
            return 0; \
        }
#define CI_INTEGER_BINARY(suffix, bits, type, rhs) \
        CI_INTEGER(ADD, suffix, bits, type, rhs, a + b) \
        CI_INTEGER(SUB, suffix, bits, type, rhs, a - b) \
        CI_INTEGER(MUL, suffix, bits, type, rhs, a * b) \
        CI_INTEGER(SDIV, suffix, bits, type, rhs, ci_int64_divrem((int##bits##_t)a, (int##bits##_t)b, 0)) \
        CI_INTEGER(UDIV, suffix, bits, type, rhs, b ? a / b : 0) \
        CI_INTEGER(SMOD, suffix, bits, type, rhs, ci_int64_divrem((int##bits##_t)a, (int##bits##_t)b, 1)) \
        CI_INTEGER(UMOD, suffix, bits, type, rhs, b ? a % b : 0) \
        CI_INTEGER(AND, suffix, bits, type, rhs, a & b) \
        CI_INTEGER(OR, suffix, bits, type, rhs, a | b) \
        CI_INTEGER(XOR, suffix, bits, type, rhs, a ^ b) \
        CI_INTEGER(SHL, suffix, bits, type, rhs, ci_lsl64(a, b)) \
        CI_INTEGER(ASR, suffix, bits, type, rhs, ci_asr64((int##bits##_t)a, b)) \
        CI_INTEGER(LSR, suffix, bits, type, rhs, ci_lsr64(a, b))
        CI_INTEGER_BINARY(8, 8, uint32_t, ci_read_uint((char*)frame->slots + op->alu.src2, 1))
        CI_INTEGER(NEG, 8, 8, uint32_t, 0, (uint32_t)0 - a)
        CI_INTEGER(NOT, 8, 8, uint32_t, 0, ~a)
        CI_INTEGER_BINARY(16, 16, uint32_t, ci_read_uint((char*)frame->slots + op->alu.src2, 2))
        CI_INTEGER(NEG, 16, 16, uint32_t, 0, (uint32_t)0 - a)
        CI_INTEGER(NOT, 16, 16, uint32_t, 0, ~a)
        CI_INTEGER_BINARY(32, 32, uint32_t, ci_read_uint((char*)frame->slots + op->alu.src2, 4))
        CI_INTEGER(NEG, 32, 32, uint32_t, 0, (uint32_t)0 - a)
        CI_INTEGER(NOT, 32, 32, uint32_t, 0, ~a)
        CI_INTEGER_BINARY(64, 64, uint64_t, ci_read_uint((char*)frame->slots + op->alu.src2, 8))
        CI_INTEGER(NEG, 64, 64, uint64_t, 0, (uint64_t)0 - a)
        CI_INTEGER(NOT, 64, 64, uint64_t, 0, ~a)
        CI_INTEGER_BINARY(_IMM32, 32, uint32_t, op->alu_imm.immediate)
        CI_INTEGER_BINARY(_IMM64, 64, uint64_t, op->alu_imm.immediate)
#undef CI_INTEGER_BINARY
#undef CI_INTEGER
#define CI_INTEGER128(name, expression) \
        case CI_OP_##name##128: { \
            CiUint128 a, b; \
            ci_uint128_read(&a, (char*)frame->slots + op->alu.src, 16); \
            ci_uint128_read(&b, (char*)frame->slots + op->alu.src2, 16); \
            CiUint128 result = (expression); \
            ci_uint128_write((char*)frame->slots + op->alu.slot, 16, result); \
            frame->pc++; \
            return 0; \
        }
        CI_INTEGER128(ADD, ci_uint128_add(a, b))
        CI_INTEGER128(SUB, ci_uint128_sub(a, b))
        CI_INTEGER128(MUL, ci_uint128_mul(a, b))
        CI_INTEGER128(SDIV, ci_uint128_from_int128(ci_int128_div(ci_int128_from_uint128(a), ci_int128_from_uint128(b))))
        CI_INTEGER128(UDIV, ci_uint128_div(a, b))
        CI_INTEGER128(SMOD, ci_uint128_from_int128(ci_int128_mod(ci_int128_from_uint128(a), ci_int128_from_uint128(b))))
        CI_INTEGER128(UMOD, ci_uint128_mod(a, b))
        CI_INTEGER128(AND, ci_uint128_and(a, b))
        CI_INTEGER128(OR, ci_uint128_or(a, b))
        CI_INTEGER128(XOR, ci_uint128_xor(a, b))
        CI_INTEGER128(SHL, ci_uint128_shl(a, ci_uint128_hi(b) ? UINT64_MAX : ci_uint128_lo(b)))
        CI_INTEGER128(ASR, ci_uint128_from_int128(ci_int128_shr(ci_int128_from_uint128(a), ci_uint128_hi(b) ? UINT64_MAX : ci_uint128_lo(b))))
        CI_INTEGER128(LSR, ci_uint128_shr(a, ci_uint128_hi(b) ? UINT64_MAX : ci_uint128_lo(b)))
        CI_INTEGER128(NEG, ci_uint128_sub(ci_uint128_from_uint64(0), a))
        CI_INTEGER128(NOT, ci_uint128_xor(a, ci_uint128_from_int64(-1)))
#undef CI_INTEGER128
#define CI_COMPARE(name, suffix, bits, expression, branch) \
        case CI_OP_##name##suffix: { \
            uint##bits##_t a, b; \
            CI_INLINE_MEMCPY(&a, (char*)frame->slots + op->cmp.src, sizeof a); \
            CI_INLINE_MEMCPY(&b, (char*)frame->slots + op->cmp.src2, sizeof b); \
            _Bool result = (expression); \
            if(branch){ \
                frame->pc = result ? op->cmp_jump.jump : frame->pc + 1; \
            } \
            else { \
                *((unsigned char*)frame->slots + op->cmp.slot) = (unsigned char)result; \
                frame->pc++; \
            } \
            return 0; \
        }
#define CI_COMPARE_WIDTH(suffix, bits, branch) \
        CI_COMPARE(EQ, suffix, bits, a == b, branch) \
        CI_COMPARE(NE, suffix, bits, a != b, branch) \
        CI_COMPARE(SLT, suffix, bits, (int##bits##_t)a < (int##bits##_t)b, branch) \
        CI_COMPARE(ULT, suffix, bits, a < b, branch) \
        CI_COMPARE(SGT, suffix, bits, (int##bits##_t)a > (int##bits##_t)b, branch) \
        CI_COMPARE(UGT, suffix, bits, a > b, branch) \
        CI_COMPARE(SLE, suffix, bits, (int##bits##_t)a <= (int##bits##_t)b, branch) \
        CI_COMPARE(ULE, suffix, bits, a <= b, branch) \
        CI_COMPARE(SGE, suffix, bits, (int##bits##_t)a >= (int##bits##_t)b, branch) \
        CI_COMPARE(UGE, suffix, bits, a >= b, branch)
        CI_COMPARE_WIDTH(32, 32, 0)
        CI_COMPARE_WIDTH(_JUMP32, 32, 1)
        CI_COMPARE_WIDTH(64, 64, 0)
        CI_COMPARE_WIDTH(_JUMP64, 64, 1)
#undef CI_COMPARE_WIDTH
#undef CI_COMPARE
#define CI_COMPARE128(name, expression) \
        case CI_OP_##name##128: { \
            CiUint128 a, b; \
            ci_uint128_read(&a, (char*)frame->slots + op->cmp.src, 16); \
            ci_uint128_read(&b, (char*)frame->slots + op->cmp.src2, 16); \
            _Bool result = (expression); \
            *((unsigned char*)frame->slots + op->cmp.slot) = (unsigned char)result; \
            frame->pc++; \
            return 0; \
        }
        CI_COMPARE128(EQ, ci_uint128_eq(a, b))
        CI_COMPARE128(NE, ci_uint128_ne(a, b))
        CI_COMPARE128(SLT, ci_int128_lt(ci_int128_from_uint128(a), ci_int128_from_uint128(b)))
        CI_COMPARE128(ULT, ci_uint128_lt(a, b))
        CI_COMPARE128(SGT, ci_int128_gt(ci_int128_from_uint128(a), ci_int128_from_uint128(b)))
        CI_COMPARE128(UGT, ci_uint128_gt(a, b))
        CI_COMPARE128(SLE, ci_int128_le(ci_int128_from_uint128(a), ci_int128_from_uint128(b)))
        CI_COMPARE128(ULE, ci_uint128_le(a, b))
        CI_COMPARE128(SGE, ci_int128_ge(ci_int128_from_uint128(a), ci_int128_from_uint128(b)))
        CI_COMPARE128(UGE, ci_uint128_ge(a, b))
#undef CI_COMPARE128
#define CI_FALU16_CASE(name, suffix, expression) \
        case CI_OP_##name##suffix: { \
            uint16_t a = (uint16_t)ci_read_uint((char*)frame->slots + op->falu32.src, 2); \
            uint16_t b = (uint16_t)ci_read_uint((char*)frame->slots + op->falu32.src2, 2); \
            (void)b; \
            uint16_t r; \
            r = (expression); \
            ci_write_uint((char*)frame->slots + op->falu32.slot, 2, r); \
            frame->pc++; \
            return 0; \
        }
        CI_FALU16_CASE(FADD, 16, ci_float16_add(a, b, 0))
        CI_FALU16_CASE(FSUB, 16, ci_float16_sub(a, b, 0))
        CI_FALU16_CASE(FMUL, 16, ci_float16_mul(a, b, 0))
        CI_FALU16_CASE(FDIV, 16, ci_float16_div(a, b, 0))
        CI_FALU16_CASE(FNEG, 16, ci_float16_neg(a))
        CI_FALU16_CASE(FADD, _BF16, ci_float16_add(a, b, 1))
        CI_FALU16_CASE(FSUB, _BF16, ci_float16_sub(a, b, 1))
        CI_FALU16_CASE(FMUL, _BF16, ci_float16_mul(a, b, 1))
        CI_FALU16_CASE(FDIV, _BF16, ci_float16_div(a, b, 1))
        CI_FALU16_CASE(FNEG, _BF16, ci_float16_neg(a))
#undef CI_FALU16_CASE
#define CI_FALU32_CASE(name, expression) \
        case CI_OP_##name##32: { \
            float a, b; \
            CI_INLINE_MEMCPY(&a, (char*)frame->slots + op->falu32.src, sizeof a); \
            CI_INLINE_MEMCPY(&b, (char*)frame->slots + op->falu32.src2, sizeof b); \
            void* dest = (char*)frame->slots + op->falu32.slot; \
            float res; \
            res = (expression); \
            CI_INLINE_MEMCPY(dest, &res, sizeof res); \
            frame->pc++; \
            return 0; \
        }
        CI_FALU32_CASE(FADD, a + b)
        CI_FALU32_CASE(FSUB, a - b)
        CI_FALU32_CASE(FMUL, a * b)
        CI_FALU32_CASE(FDIV, a / b)
        CI_FALU32_CASE(FNEG, -a)
#undef CI_FALU32_CASE
#define CI_FALU64_CASE(name, expression) \
        case CI_OP_##name##64: { \
            double a, b; \
            CI_INLINE_MEMCPY(&a, (char*)frame->slots + op->falu64.src, sizeof a); \
            CI_INLINE_MEMCPY(&b, (char*)frame->slots + op->falu64.src2, sizeof b); \
            void* dest = (char*)frame->slots + op->falu64.slot; \
            double res; \
            res = (expression); \
            CI_INLINE_MEMCPY(dest, &res, sizeof res); \
            frame->pc++; \
            return 0; \
        }
        CI_FALU64_CASE(FADD, a + b)
        CI_FALU64_CASE(FSUB, a - b)
        CI_FALU64_CASE(FMUL, a * b)
        CI_FALU64_CASE(FDIV, a / b)
        CI_FALU64_CASE(FNEG, -a)
#undef CI_FALU64_CASE
#define CI_FALU80_CASE(name, expression) \
        case CI_OP_##name##80: { \
            CiFloat80 a = ci_float80_read((char*)frame->slots + op->falu32.src); \
            CiFloat80 b = ci_float80_read((char*)frame->slots + op->falu32.src2), r; \
            (void)b; \
            r = (expression); \
            ci_float80_write((char*)frame->slots + op->falu32.slot, op->falu32.slot_size, r); \
            frame->pc++; return 0; \
        }
        CI_FALU80_CASE(FADD, ci_float80_add(a, b))
        CI_FALU80_CASE(FSUB, ci_float80_sub(a, b))
        CI_FALU80_CASE(FMUL, ci_float80_mul(a, b))
        CI_FALU80_CASE(FDIV, ci_float80_div(a, b))
        CI_FALU80_CASE(FNEG, ci_float80_neg(a))
#undef CI_FALU80_CASE
#define CI_FALU128_CASE(name, expression) \
        case CI_OP_##name##128: { \
            CiFloat128 a = ci_float128_read((char*)frame->slots + op->falu32.src); \
            CiFloat128 b = ci_float128_read((char*)frame->slots + op->falu32.src2), r; \
            (void)b; \
            r = (expression); \
            ci_float128_write((char*)frame->slots + op->falu32.slot, op->falu32.slot_size, r); \
            frame->pc++; return 0; \
        }
        CI_FALU128_CASE(FADD, ci_float128_add(a, b))
        CI_FALU128_CASE(FSUB, ci_float128_sub(a, b))
        CI_FALU128_CASE(FMUL, ci_float128_mul(a, b))
        CI_FALU128_CASE(FDIV, ci_float128_div(a, b))
        CI_FALU128_CASE(FNEG, ci_float128_neg(a))
#undef CI_FALU128_CASE
#define CI_FCMP16_CASE(name, suffix, expression) \
        case CI_OP_##name##suffix: { \
            uint16_t a = (uint16_t)ci_read_uint((char*)frame->slots + op->fcmp32.src, 2); \
            uint16_t b = (uint16_t)ci_read_uint((char*)frame->slots + op->fcmp32.src2, 2); \
            _Bool r; \
            r = (expression); \
            *((unsigned char*)frame->slots + op->fcmp32.slot) = (unsigned char)r; \
            frame->pc++; \
            return 0; \
        }
        CI_FCMP16_CASE(FEQ, 16,  ci_float16_eq(a, b, 0))
        CI_FCMP16_CASE(FNE, 16, !ci_float16_eq(a, b, 0))
        CI_FCMP16_CASE(FLT, 16,  ci_float16_lt(a, b, 0))
        CI_FCMP16_CASE(FGT, 16,  ci_float16_lt(b, a, 0))
        CI_FCMP16_CASE(FLE, 16,  ci_float16_le(a, b, 0))
        CI_FCMP16_CASE(FGE, 16,  ci_float16_le(b, a, 0))
        CI_FCMP16_CASE(FEQ, _BF16,  ci_float16_eq(a, b, 1))
        CI_FCMP16_CASE(FNE, _BF16, !ci_float16_eq(a, b, 1))
        CI_FCMP16_CASE(FLT, _BF16,  ci_float16_lt(a, b, 1))
        CI_FCMP16_CASE(FGT, _BF16,  ci_float16_lt(b, a, 1))
        CI_FCMP16_CASE(FLE, _BF16,  ci_float16_le(a, b, 1))
        CI_FCMP16_CASE(FGE, _BF16,  ci_float16_le(b, a, 1))
#undef CI_FCMP16_CASE
#define CI_FCMP32_CASE(name, expression) \
        case CI_OP_##name##32: { \
            float a, b; \
            CI_INLINE_MEMCPY(&a, (char*)frame->slots + op->fcmp32.src, sizeof a); \
            CI_INLINE_MEMCPY(&b, (char*)frame->slots + op->fcmp32.src2, sizeof b); \
            uint64_t res; \
            res = (expression); \
            *((unsigned char*)frame->slots + op->fcmp32.slot) = (unsigned char)res; \
            frame->pc++; \
            return 0; \
        }
        CI_FCMP32_CASE(FEQ, a == b)
        CI_FCMP32_CASE(FNE, a != b)
        CI_FCMP32_CASE(FLT, a <  b)
        CI_FCMP32_CASE(FGT, a >  b)
        CI_FCMP32_CASE(FLE, a <= b)
        CI_FCMP32_CASE(FGE, a >= b)
#undef CI_FCMP32_CASE
#define CI_FCMP64_CASE(name, expression) \
        case CI_OP_##name##64: { \
            double a, b; \
            CI_INLINE_MEMCPY(&a, (char*)frame->slots + op->fcmp64.src, sizeof a); \
            CI_INLINE_MEMCPY(&b, (char*)frame->slots + op->fcmp64.src2, sizeof b); \
            uint64_t res; \
            res = (expression); \
            *((unsigned char*)frame->slots + op->fcmp64.slot) = (unsigned char)res; \
            frame->pc++; \
            return 0; \
        }
        CI_FCMP64_CASE(FEQ, a == b)
        CI_FCMP64_CASE(FNE, a != b)
        CI_FCMP64_CASE(FLT, a <  b)
        CI_FCMP64_CASE(FGT, a >  b)
        CI_FCMP64_CASE(FLE, a <= b)
        CI_FCMP64_CASE(FGE, a >= b)
#undef CI_FCMP64_CASE
#define CI_FCMP80_CASE(name, expression) \
        case CI_OP_##name##80: { \
            CiFloat80 a = ci_float80_read((char*)frame->slots + op->fcmp32.src); \
            CiFloat80 b = ci_float80_read((char*)frame->slots + op->fcmp32.src2); \
            _Bool r; \
            r = (expression); \
            *((unsigned char*)frame->slots + op->fcmp32.slot) = (unsigned char)r; \
            frame->pc++; return 0; \
        }
        CI_FCMP80_CASE(FEQ, ci_float80_eq(a, b))
        CI_FCMP80_CASE(FNE, !ci_float80_eq(a, b))
        CI_FCMP80_CASE(FLT, ci_float80_lt(a, b))
        CI_FCMP80_CASE(FLE, ci_float80_le(a, b))
        CI_FCMP80_CASE(FGT, ci_float80_lt(b, a))
        CI_FCMP80_CASE(FGE, ci_float80_le(b, a))
#undef CI_FCMP80_CASE
#define CI_FCMP128_CASE(name, expression) \
        case CI_OP_##name##128: { \
            CiFloat128 a = ci_float128_read((char*)frame->slots + op->fcmp32.src); \
            CiFloat128 b = ci_float128_read((char*)frame->slots + op->fcmp32.src2); \
            _Bool r; \
            r = (expression); \
            *((unsigned char*)frame->slots + op->fcmp32.slot) = (unsigned char)r; \
            frame->pc++; return 0; \
        }
        CI_FCMP128_CASE(FEQ, ci_float128_eq(a, b))
        CI_FCMP128_CASE(FNE, !ci_float128_eq(a, b))
        CI_FCMP128_CASE(FLT, ci_float128_lt(a, b))
        CI_FCMP128_CASE(FLE, ci_float128_le(a, b))
        CI_FCMP128_CASE(FGT, ci_float128_lt(b, a))
        CI_FCMP128_CASE(FGE, ci_float128_le(b, a))
#undef CI_FCMP128_CASE

        case CI_OP_PROPAGATE_ERROR: {
            int err;
            CI_INLINE_MEMCPY(&err, (char*)frame->slots + op->propagate_error.src, sizeof err);
            if(err) return err;
            frame->pc++;
            return 0;
        }
        case CI_OP_CHECK:
            if(!*((unsigned char*)frame->slots + op->check.src))
                return ci_error(ci, op->loc, "%s", op->check.message);
            frame->pc++;
            return 0;
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
        case CI_OP_TLS_ADDR: {
            void* address;
            int err = ci_tls_address(ci, op->var_addr.var, &address);
            if(err) return err;
            CI_INLINE_MEMCPY((char*)frame->slots + op->var_addr.slot, &address, sizeof address);
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
        case CI_OP_JUMP:
            frame->pc = op->jump.jump;
            return 0;

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
        case CI_OP_CLEAR_CACHE: {
            char *begin, *end;
            CI_INLINE_MEMCPY(&begin, (char*)frame->slots + op->clear_cache.begin, sizeof begin);
            CI_INLINE_MEMCPY(&end, (char*)frame->slots + op->clear_cache.end, sizeof end);
            #ifdef _WIN32
            if(!FlushInstructionCache(GetCurrentProcess(), begin, (SIZE_T)((uintptr_t)end - (uintptr_t)begin)))
                return ci_error(ci, op->loc, "__builtin___clear_cache failed");
            #else
            __builtin___clear_cache(begin, end);
            #endif
            frame->pc++;
            return 0;
        }
        case CI_OP_STACK_SAVE: {
            CiStack* stack;
            int err = ci_frame_stack(ci, frame, &stack);
            if(err) return err;
            CiStackMark mark = ci_stack_mark(stack);
            CI_INLINE_MEMCPY((char*)frame->slots + op->stack_save.slot, &mark, sizeof mark);
            frame->pc++;
            return 0;
        }
        case CI_OP_STACK_RESTORE: {
            CiStackMark mark;
            CI_INLINE_MEMCPY(&mark, (char*)frame->slots + op->stack_restore.slot, sizeof mark);
            ci_stack_rewind(frame->stack, mark);
            frame->pc++;
            return 0;
        }
    }
    return ci_unimplemented(ci, op->loc, "unsupported op kind");
}

static
const CcFunc*_Nullable
ci_hotswap_target(const CcFunc* func){
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
ci_alloc_call_frame(CiInterpreter* ci, CiInterpFrame* _Nullable caller, const CcFunc* func, CiInterpFrame* _Nullable* _Nonnull out){
    if(!func->parsed)
        return ci_ice(ci, func->loc, "function '%s' not parsed before execution", func->name->data);
    if(!func->interp_ops)
        return ci_ice(ci, func->loc, "function '%s' not lowered before execution", func->name->data);
    CiInterpFrame* frame;
    int err = ci_new_frame(ci, caller, func->frame_size, func->interp_ops->frame_alignment, &frame);
    if(err) return err;
    frame->name = func->name;
    frame->ops = func->interp_ops->code.data;
    frame->op_count = func->interp_ops->code.count;
    frame->abi = func->interp_ops->abi;
    frame->incoming = &frame->stack->registers;
    if(func->type->is_variadic){
        NcCallState* saved = ci_stack_alloc(frame->stack, sizeof *saved, _Alignof(NcCallState));
        if(!saved){
            ci_stack_rewind(frame->stack, frame->stack_mark);
            return CI_OOM_ERROR;
        }
        *saved = *frame->incoming;
        frame->incoming = saved;
    }
    *out = frame;
    return 0;
}

static
int
ci_make_abi_call_frame(CiInterpreter* ci, CiInterpFrame* caller, const CcFunc* func, const CiOp* op, CiInterpFrame* _Nullable* _Nonnull out){
    const CcFunc* target = ci_hotswap_target(func);
    if(!target) return ci_error(ci, op->loc, "hotswap cycle detected");
    func = target;
    CiInterpFrame* frame = NULL;
    int err = ci_alloc_call_frame(ci, caller, func, &frame);
    if(err) return err;
    frame->return_buf = op->call.ret_size ? (char*)caller->slots + op->call.ret_slot : ci_discard_buf;
    frame->return_size = op->call.ret_size ? op->call.ret_size : sizeof ci_discard_buf;
    *out = frame;
    return 0;
}

static
void
ci_free_call_frame(CiInterpreter* ci, CiInterpFrame* frame){
    (void)ci;
    ci_stack_rewind(frame->stack, frame->stack_mark);
}

static
int
ci_interp_run(CiInterpreter* ci, CiInterpFrame* root){
    DRP_PROFILE_BEGIN(profile, "interpreter.run");
    int err = 0;
    if(!root->stack){
        CiStack* stack;
        err = ci_frame_stack(ci, root, &stack);
        if(err) return err;
    }
    CiInterpFrame* frame = root;
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
    DRP_PROFILE_END(profile);
}

static
int
ci_interp_step(CiInterpreter* ci, CiInterpFrame* frame){
    int err;
    if(!frame->stack){
        CiStack* stack;
        err = ci_frame_stack(ci, frame, &stack);
        if(err) return err;
    }
    CiInterpFrame* child = NULL;
    err = _ci_interp_step(ci, frame, &child);
    if(err == CI_STEP_ENTER_FRAME){
        err = ci_interp_run(ci, child);
        ci_free_call_frame(ci, child);
        if(!err) frame->pc++;
    }
    return err;
}

static
int
ci_call_argv(CiInterpreter* ci, CiInterpFrame* _Nullable caller, const CcFunc* func, void* _Nonnull* _Nonnull argv, uint32_t nargs, void* result, size_t size, SrcLoc loc){
    NcCallPlan* plan = func->interp_ops->abi;
    if(plan->result.size > size) return CI_RESULT_TOO_SMALL(ci, loc, plan->result.size, size);
    if(nargs != func->interp_ops->native_type->param_count) return ci_error(ci, loc, "argument count mismatch");
    CiStack* stack;
    int err = ci_frame_stack(ci, caller, &stack);
    if(err) return err;
    CiStackMark mark = ci_stack_mark(stack);
    void* storage = ci_stack_alloc(stack, nc_stack_storage_size(plan), plan->stack_alignment);
    if(!storage) return CI_OOM_ERROR;
    NcCallState* state = &stack->registers;
    nc_registers_init(state, plan, storage, result);
    for(uint32_t i = 0; i < func->params.count; i++){
        uint32_t n = func->params.data[i].native_index;
        if(n != UINT32_MAX) nc_arg_write(state, plan, i, argv[n]);
    }
    err = ci_call_abi(ci, caller, func, state, loc);
    if(!err) nc_result_read(state, plan, result);
    ci_stack_rewind(stack, mark);
    return err;
}

static
int
ci_call_abi(CiInterpreter* ci, CiInterpFrame* _Nullable caller, const CcFunc* func, NcCallState* state, SrcLoc loc){
    const CcFunc* target = ci_hotswap_target(func);
    if(!target) return ci_error(ci, loc, "hotswap cycle detected");
    CiStack* stack;
    int err = ci_frame_stack(ci, caller, &stack);
    if(err) return err;
    size_t capture_size = nc_capture_size(target->interp_ops->abi->target);
    if(state != &stack->registers) memcpy(&stack->registers, state, capture_size);
    CiInterpFrame* frame = NULL;
    err = ci_alloc_call_frame(ci, caller, target, &frame);
    if(err) return err;
    err = ci_interp_run(ci, frame);
    if(state != &stack->registers) memcpy(state, &stack->registers, capture_size);
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
    const CcFunc* func = cc_scope_lookup_func(&ci->root.cc.scope, atom, CC_SCOPE_NO_WALK);
    if(!func || !func->defined)
        return CI_SYMBOL_NOT_FOUND;
    const CcFunc* target = ci_hotswap_target(func);
    if(!target)
        return ci_error(ci, (SrcLoc){0}, "hotswap cycle detected");
    func = target;
    if(!func->parsed || !func->interp_ops)
        return CI_SYMBOL_UNRESOLVED;
    const CcFunction* ftype = func->interp_ops->native_type;
    if(nargs != ftype->param_count)
        return ci_error(ci, func->loc, "ci_call_by_name '%.*s': expected %u args, got %u",
            (int)name.length, name.text, ftype->param_count, nargs);
    // Ignore top-level qualifiers and alignment when comparing value types.
    for(uint32_t i = 0; i < nargs; i++){
        CcQualType argument = cc_parameter_value_type(args[i].type);
        CcQualType parameter = cc_parameter_value_type(ftype->params[i]);
        if(!cc_types_same(argument, parameter))
            return ci_error(ci, func->loc, "ci_call_by_name '%.*s': arg %u type mismatch", (int)name.length, name.text, i);
    }
    for(uint32_t i = 0; i < nargs; i++){
        uint32_t param_sz;
        int err = cc_sizeof_as_uint(&ci->parser, ftype->params[i], func->loc, &param_sz);
        if(err) return err;
        if(args[i].size < param_sz)
            return ci_error(ci, func->loc, "ci_call_by_name '%.*s': arg %u buffer too small", (int)name.length, name.text, i);
    }
    size_t argv_count = nargs ? nargs : 1;
    void** argv = Allocator_alloc(ci_allocator(ci), argv_count, sizeof *argv, _Alignof(void*));
    if(!argv) return CI_OOM_ERROR;
    for(uint32_t i = 0; i < nargs; i++)
        argv[i] = (void*)(uintptr_t)args[i].data;
    int err = ci_call_argv(ci, NULL, func, argv, nargs, result, size, func->loc);
    Allocator_free(ci_allocator(ci), argv, argv_count, sizeof *argv, _Alignof(void*));
    return err;
}

static
int
ci_call_main(CiInterpreter* ci, int argc, char*_Null_unspecified*_Null_unspecified argv, char*_Null_unspecified*_Null_unspecified envp, int* out_ret){
    AtomTable* at = ci_lock_atoms(ci);
    Atom atom = AT_atomize(at, "main", 4);
    ci_unlock_atoms(ci, at);
    if(!atom) return CI_OOM_ERROR;
    CcFunc* func = cc_scope_lookup_func(&ci->root.cc.scope, atom, CC_SCOPE_NO_WALK);
    if(!func || !func->defined)
        return CI_SYMBOL_NOT_FOUND;
    const CcFunction* ftype = func->type;
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
    _Bool found = cc_scope_lookup_symbol(&ci->root.cc.scope, atom, CC_SCOPE_NO_WALK, &sym);
    if(!found) return CI_SYMBOL_NOT_FOUND;
    int err = 0;
    switch(sym.kind){
        case CC_SYM_VAR:
            err = ci_deps_add_var(&ci->deps, ci_allocator(ci), sym.var, CI_VAR_DEP_USED);
            break;
        case CC_SYM_FUNC:
            err = ci_deps_add_func(&ci->deps, ci_allocator(ci), sym.func, CC_FUNC_DEP_USED);
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
    return ci_resolve_deps(ci, &ci->deps);
}

static
void
ci_apply_static_relocs(unsigned char* bytes, const CiStaticReloc* relocs, size_t count){
    for(size_t i = 0; i < count; i++){
        const CiStaticReloc* r = &relocs[i];
        uint64_t address = 0;
        switch(r->kind){
            case CI_STATIC_RELOC_ABSOLUTE: break;
            case CI_STATIC_RELOC_VAR: address = (uintptr_t)r->var->interp_val; break;
            case CI_STATIC_RELOC_FUNC: address = (uintptr_t)r->func->native_func; break;
            case CI_STATIC_RELOC_LITERAL: address = (uintptr_t)r->literal; break;
        }
        address += r->addend;
        memcpy(bytes + r->offset, &address, r->size);
    }
}

static
int
ci_prepare_toplevel(CiInterpreter* ci){
    int err = ci_lower_toplevel(ci, &ci->deps);
    if(err) return err;
    err = ci_resolve_deps(ci, &ci->deps);
    if(err) return err;
    return 0;
}

static
void
ci_relocate_static_data(CiStaticData* data){
    memcpy(data->var->interp_val, data->blob->data, data->size);
    ci_apply_static_relocs(data->var->interp_val, data->relocs.data, data->relocs.count);
    data->var->interp_initialized = 1;
}

static
int
ci_resolve_deps(CiInterpreter* ci, CiLowerDeps* deps){
    DRP_PROFILE_BEGIN(profile, "resolve.dependencies");
    int err = ci_tls_prepare(ci);
    if(err) return err;
    Allocator al = ci_allocator(ci);
    Marray(CiStaticData) initializers = {0};
    size_t fi = 0, vi = 0;
    while(fi < deps->funcs.count || vi < deps->vars.count){
        size_t init_count = initializers.count;
        while(fi < deps->funcs.count){
            CcFunc* func = (CcFunc*)(uintptr_t)PM_items(&deps->funcs).data[fi++].key;
            if(!func->defined){
                if(!func->native_func && func->name){
                    Atom sym = func->mangle ? func->mangle : func->name;
                    void* addr;
                    err = ci_dlsym(ci, func->loc, sym, "function", &addr);
                    if(err) goto cleanup;
                    func->native_func = (void(*)(void))addr;
                }
                continue;
            }
            if(func->parse_failed){ err = CI_SYNTAX_ERROR; goto cleanup; }
            if(!func->parsed){
                err = cc_parse_func_body(&ci->parser, func);
                if(err) goto cleanup;
            }
            err = ci_lower_func(ci, func, deps);
            if(err) goto cleanup;
        }
        while(vi < deps->vars.count){
            PointerMapItem item = PM_items(&deps->vars).data[vi++];
            CcVariable* var = (CcVariable*)(uintptr_t)item.key;
            if(var->thread_local_ && var->extern_ && !var->initializer){
                err = ci_error(ci, var->loc, "native extern thread_local variable '%s' is not supported", var->name->data);
                goto cleanup;
            }
            if(!var->interp_val){
                if(var->extern_ && !var->initializer){
                    Atom sym = var->mangle ? var->mangle : var->name;
                    void* addr;
                    err = ci_dlsym(ci, var->loc, sym, "extern variable", &addr);
                    if(err) goto cleanup;
                    var->interp_val = addr;
                }
                else {
                    uint32_t sz;
                    if(ccqt_kind(var->type) == CC_VLA){
                        sz = sizeof(void*);
                        err = 0;
                    }
                    else err = cc_sizeof_as_uint(&ci->parser, var->type, var->loc, &sz);
                    if(err) goto cleanup;
                    uint32_t alignment;
                    err = cc_alignof_as_uint(&ci->parser, var->type, var->loc, &alignment);
                    if(err) goto cleanup;
                    if(ccqt_kind(var->type) == CC_VLA)
                        alignment = _Alignof(void*);
                    if(var->alignment > alignment) alignment = var->alignment;
                    if(!alignment) alignment = 1;
                    var->interp_val = Allocator_zalloc(al, sz ? sz : 1, 1, alignment);
                    if(!var->interp_val){ err = CI_OOM_ERROR; goto cleanup; }
                }
            }
            if(var->thread_local_ && !var->tls_slot){
                err = cc_sizeof_as_uint(&ci->parser, var->type, var->loc, &var->tls_size);
                if(err) goto cleanup;
                err = cc_alignof_as_uint(&ci->parser, var->type, var->loc, &var->tls_alignment);
                if(err) goto cleanup;
                if(var->alignment > var->tls_alignment) var->tls_alignment = var->alignment;
                if(!var->tls_alignment) var->tls_alignment = 1;
                var->tls_slot = ++ci->tls_slot_count;
            }
            if(!(var->interp_preinit || ((uintptr_t)item.value & CI_VAR_DEP_INITIALIZE))
                || !var->initializer || var->interp_initialized) continue;
            _Bool queued = 0;
            for(size_t i = 0; i < initializers.count; i++)
                if(initializers.data[i].var == var){ queued = 1; break; }
            if(queued) continue;
            CiStaticData* init;
            err = ma_alloc(CiStaticData)(&initializers, al, &init);
            if(err){ err = CI_OOM_ERROR; goto cleanup; }
            *init = (CiStaticData){.var = var};
            err = ci_build_static_data(ci, init, deps);
            if(err) goto cleanup;
        }
        // Building one initializer can upgrade an already visited dependency.
        if(initializers.count != init_count) vi = 0;
    }
    for(size_t i = 0; i < deps->funcs.count; i++){
        PointerMapItem item = PM_items(&deps->funcs).data[i];
        CcFunc* func = (CcFunc*)(uintptr_t)item.key;
        if(func->interp_ops){
            if(!func->native_func && ((uintptr_t)item.value & CC_FUNC_DEP_ADDR_TAKEN)){
                err = ci_create_closure(ci, func);
                if(err) goto cleanup;
            }
        }
    }
    PointerMapItems templates = PM_items(&deps->templates);
    for(size_t i = 0; i < templates.count; i++){
        CiInitTemplate* source = (CiInitTemplate*)(uintptr_t)templates.data[i].key;
        if(!source->linked){
            ci_apply_static_relocs(source->bytes, source->relocs, source->reloc_count);
            source->linked = 1;
        }
    }
    for(size_t i = 0; i < initializers.count; i++)
        ci_relocate_static_data(&initializers.data[i]);
    cleanup:
    for(size_t i = 0; i < initializers.count; i++)
        ci_static_data_cleanup(&initializers.data[i], al);
    ma_cleanup(CiStaticData)(&initializers, al);
    return err;
    DRP_PROFILE_END(profile);
}

static
int
ci_resolve_refs(CiInterpreter* ci){
    int err = ci_lower_toplevel(ci, &ci->deps);
    if(err) return err;
    AtomTable* at = ci_lock_atoms(ci);
    Atom main_atom = AT_atomize(at, "main", 4);
    ci_unlock_atoms(ci, at);
    if(!main_atom) return CI_OOM_ERROR;
    CcFunc* main_func = cc_scope_lookup_func(&ci->root.cc.scope, main_atom, CC_SCOPE_NO_WALK);
    if(main_func && main_func->defined){
        err = ci_deps_add_func(&ci->deps, ci_allocator(ci), main_func, CC_FUNC_DEP_USED);
        if(err) return err;
    }
    return ci_resolve_deps(ci, &ci->deps);
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

static CppPragmaFn ci_pragma_lib, ci_pragma_lib_path, ci_pragma_framework, ci_pragma_pkg_config, ci_pragma_procmacro, ci_pragma_comment;
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
    if(ci->procedural_macros && !ci->nofun){
        err = cpp_register_pragma(&ci->parser.cpp, SV("procmacro"), ci_pragma_procmacro, ci);
        if(err) return err;
    }
    return 0;
}

static
int
ci_register_macros(CiInterpreter* ci){
    int err = 0;
    if(!ci->nofun){
        err = cpp_define_builtin_func_macro(&ci->parser.cpp, SV("__shell"), ci_shell, ci, 1, 1, 0);
        if(err) return err;
        err = cpp_define_builtin_func_macro(&ci->parser.cpp, SV("__SHELL__"), ci_shell, ci, 1, 1, 0);
        if(err) return err;
    }
    return err;
}

static
int
ci_try_load_library(CiInterpreter* ci, CStringView lib, _Bool* success){
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
        CStringViewUtf16 wlib = msb16_borrow_csv(&sb);
        handle = LoadLibraryW((const wchar_t*)wlib.text);
        msb16_destroy(&sb);
        if(!handle) {*success = 0; return 0;}
        #else
        handle = dlopen(a->data, RTLD_GLOBAL | RTLD_LAZY);
        if(!handle) {*success = 0; return 0;}
        #endif
        int err = AM_put(&ci->opened_libs, ci_allocator(ci), a, handle);
        if(err){
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
            err = ci_try_load_library(ci, msb_borrow_csv(&sb), &success);
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
            err = ci_try_load_library(ci, msb_borrow_csv(&sb), &success);
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
            err = ci_try_load_library(ci, msb_borrow_csv(&sb), &success);
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
        err = ci_try_load_library(ci, msb_borrow_csv(&sb), &success);
        if(err) goto finally;
        if(success) goto finally;
    }
    {
        msb_reset(&sb);
        msb_write_str(&sb, sv.text, sv.length);
        msb_nul_terminate(&sb);
        if(sb.errored){ err = CI_OOM_ERROR; goto finally; }
        err = ci_try_load_library(ci, msb_borrow_csv(&sb), &success);
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
        err = ci_try_load_library(ci, msb_borrow_csv(&sb), &success);
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
    CStringView output = {0};
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
        cmd_prog(&cmd, CSV("pkg-config"));
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
        void* envp = env_to_envp(cpp->env, scratch, &envp_size, NULL, 0);
        if(!envp){
            err = CI_OOM_ERROR;
            goto finally;
        }
        err = cmd_run_capture(&cmd, envp, scratch, &output, CMD_CAPTURE_STDOUT);
        cmd_destroy(&cmd);
        env_free_envp(cpp->env, scratch, envp, envp_size);
        if(err){
            if(optional){
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
            .keyword = {.args = pkg_kwargs, .count = countof(pkg_kwargs)},
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
                err = cpp_define_obj_macro(cpp, mname, &valtok, 1, NULL);
            }
            else {
                CppToken onetok = {.type = CPP_NUMBER, .txt = {1, "1"}};
                err = cpp_define_obj_macro(cpp, val, &onetok, 1, NULL);
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
    if(output.text) Allocator_free(scratch, output.text, output.length+1, 1, 1);
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
    const CcFunction* ftype = func->type;
    CcExpr* expr = NULL;
    int err = 0;
    Allocator al = p->cpp.allocator;
    Marray(CcToken)* scratch = cc_get_scratch(p);
    Marray(CcToken) pending = p->pending;
    CcToken eof = {.type = CC_EOF, .loc = loc};
    err = ma_push(CcToken)(scratch, al, eof);
    if(err) goto restore;
    CcToken rparen = {.punct = {.type = CC_PUNCTUATOR, .punct = CC_rparen, .loc = loc}};
    err = ma_push(CcToken)(scratch, al, rparen);
    if(err) goto restore;
    size_t idx = scratch->count;
    if(args->count){
        const CppToken* arg_toks = args->data;
        const CppToken* end = args->data + args->count;
        while(arg_toks < end){
            CcToken tok;
            #if defined(__GNUC__) && !defined(__clang__) && !defined(__DRC__) && __GNUC__ == 13
                // this version had bad static analysis of uninitialized variables
                tok = (CcToken){0};
            #endif
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
    CcToken lparen = {.punct = {.type = CC_PUNCTUATOR, .punct = CC_lparen, .loc = loc}};
    err = ma_push(CcToken)(scratch, al, lparen);
    if(err) goto restore;
    CcToken name_tok = {.ident = {.type = CC_IDENTIFIER, .ident = func->name, .loc = loc}};
    err = ma_push(CcToken)(scratch, al, name_tok);
    if(err)goto restore;
    p->pending = *scratch;
    // FIXME: should really be constexpr, but functions aren't supported...
    err = cc_parse_expr(p, (CcParseFlags){.vc=CC_RUNTIME_VALUE}, &expr, CCQT_NONE);
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
    uint32_t result_alignment;
    err = cc_alignof_as_uint(p, ftype->return_type, loc, &result_alignment);
    if(err) return err;
    _Alignas(16) char result_buf[16];
    char* result = result_buf;
    uint32_t orig_result_sz = result_sz ? result_sz : 1;
    if(result_sz > sizeof result_buf || result_alignment > 16){
        result = Allocator_zalloc(ci_scratch_allocator(ci), orig_result_sz, 1, result_alignment);
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
        case CC_VLA: err = ci_ice(ci, loc, "%s", "invalid VLA return type"); goto cleanup;
        case CC_ALIGNED: ci_unsafe_unreachable();
        case CC_BASIC:
            switch(ccqt_basic_kind(rt)){
                case CCBT_nullptr_t:
                    a = AT_ATOMIZE(cpp->at, "nullptr");
                    tok_type = CPP_IDENTIFIER;
                    break;
                case CCBT__Any:{
                    CiRtAny any;
                    CI_INLINE_MEMCPY(&any, result, sizeof any);
                    if(!cc_any_payload_type(ci_target(ci), any.type)){
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
                case CCBT_bfloat16:
                case CCBT_fp16:
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
            if(ccqt_is_basic(ptr->pointee) && ccqt_basic_kind(ptr->pointee) == CCBT_char){
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
        case CC_SLICE:{
            CcSlice* sl = ccqt_as_slice(rt);
            if(!ccqt_bt_eq((CcQualType){.unqual=sl->pointee.unqual}, CCBT_char)){
                err = ci_unimplemented(ci, loc, "Unsupported return type: slice");
                goto cleanup;
            }
            CiRtSlice s = *(CiRtSlice*)result;
            MStringBuilder sb = {.allocator = ci_scratch_allocator(ci)};
            msb_write_char(&sb, '"');
            for(size_t i = 0, slen = s.count; i < slen; i++){
                unsigned char c = (unsigned char)((char*)s.data)[i];
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
            if(!a){
                err = CI_OOM_ERROR;
                goto cleanup;
            }
            break;
        }
        case CC_BLOCK_POINTER:
            err = ci_unimplemented(ci, loc, "Unsupported return type");
            goto cleanup;
        case CC_VECTOR:
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
    if(orig_result != result_buf) Allocator_free(ci_scratch_allocator(ci), orig_result, orig_result_sz, 1, result_alignment);
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
    StringView macro_name = name;
    toks++;
    ntoks--;
    while(ntoks && toks->type == CPP_WHITESPACE){ toks++; ntoks--; }
    if(ntoks && toks->type == CPP_IDENTIFIER)
        name = toks->txt;
    Atom atom = AT_get_atom(cpp->at, name.text, name.length);
    if(!atom)
        return cpp_error(cpp, loc, "#pragma procmacro: unknown function '%.*s'", (int)name.length, name.text);
    CcFunc* func = cc_scope_lookup_func(&ci->root.cc.scope, atom, CC_SCOPE_NO_WALK);
    if(!func || !func->defined)
        return cpp_error(cpp, loc, "#pragma procmacro: function '%.*s' not defined", (int)name.length, name.text);
    err = ci_deps_add_func(&ci->deps, ci_allocator(ci), func, CC_FUNC_DEP_USED);
    if(!err) err = ci_resolve_deps(ci, &ci->deps);
    if(err) return err;
    return cpp_define_builtin_func_macro(cpp, macro_name, ci_procmacro_expand, func, func->type->param_count, 0, 0);
}
static
const CcTargetConfig*
ci_target(const CiInterpreter* ci){
    return ci->parser.cpp.target;
}
static
int
ci_dlsym(CiInterpreter* ci, SrcLoc loc, Atom sym, const char* what, void*_Nullable*_Nonnull out){
    DRP_PROFILE_BEGIN(profile, "resolve.symbol");
    void* p = NULL;
    int err = ci_try_dlsym(ci, sym, &p);
    if(err) return err;
    if(!p){
        if(ci_target(ci)->target != CC_TARGET_NATIVE)
            return ci_error(ci, loc, "%s '%s' not found (cross-interpreting)", what, sym->data);
        #ifdef NO_NATIVE_CALL
        return ci_error(ci, loc, "%s '%s' not found (native calls disabled)", what, sym->data);
        #elif defined _WIN32
        ci_error(ci, loc, "%s '%s' not found", what, sym->data);
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
        return ci_error(ci, loc, "%s '%s' not found: %s", what, sym->data, dlerror());
        #endif
    }
    *out = p;
    return 0;
    DRP_PROFILE_END(profile);
}

static
int
ci_try_dlsym(CiInterpreter* ci, Atom sym, void*_Nullable*_Nonnull out){
    *out = NULL;
    {
        AtomMapItems vlibs = AM_items(&ci->virtual_libs);
        for(size_t i = 0; i < vlibs.count; i++){
            AtomMap(void*)* symbols = vlibs.data[i].p;
            void* p = AM_get(symbols, sym);
            if(p){
                *out = p;
                return 0;
            }
        }
    }
    if(ci_target(ci)->target != CC_TARGET_NATIVE)
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
                    p = (void*)GetProcAddress(modules[i], sym->data);
                    if(p) break;
                }
            }
        }
    #else
        p = dlsym(RTLD_DEFAULT, sym->data);
        if(!p && sym->data[0] == '_')
            p = dlsym(RTLD_DEFAULT, sym->data+1);
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
    CStringView output = {0};
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
            cmd_prog(&cmd, (CStringView){a->length, a->data});
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
    void* envp = env_to_envp(cpp->env, scratch, &envp_size, NULL, 0);
    if(!envp){
        cmd_destroy(&cmd);
        return CI_OOM_ERROR;
    }
    err = cmd_run_capture(&cmd, envp, scratch, &output, CMD_CAPTURE_STDOUT);
    cmd_destroy(&cmd);
    env_free_envp(cpp->env, scratch, envp, envp_size);
    if(err){
        if(output.text) Allocator_free(scratch, output.text, output.length + 1, 1, 1);
        return cpp_error(cpp, loc, "__SHELL__: command failed");
    }
    // Strip trailing newlines.
    size_t output_alloc_size = output.length + 1;
    while(output.length > 0 && (output.text[output.length-1] == '\n' || output.text[output.length-1] == '\r'))
        output.length--;
    Atom v = cpp_quote_string(cpp, CSV_to_SV(output));
    Allocator_free(scratch, output.text, output_alloc_size, 1, 1);
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
        lib = Allocator_zalloc(ci_allocator(ci), 1, sizeof *lib, _Alignof(CiVirtualLib));
        if(!lib) return CI_OOM_ERROR;
        err = AM_put(&ci->virtual_libs, ci_allocator(ci), lname, lib);
        if(err){
            Allocator_free(ci_allocator(ci), lib, 1, sizeof *lib, _Alignof(CiVirtualLib));
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

// Execute prepared code only after its dependencies have been resolved.
static
int
ci_run_lowered_expr(CiInterpreter*_Nonnull ci, CiInterpFrame*_Nullable parent, const CiLoweredExpr*_Nonnull code, SrcLoc loc, void*_Nonnull result, size_t size){
    if(code->value_size > size)
        return CI_RESULT_TOO_SMALL(ci, loc, code->value_size, size);
    CiInterpFrame frame = {
        .parent = parent, .return_buf = result, .return_size = size,
        .ops = code->ops.data, .op_count = code->ops.count,
    };
    int err = ci_frame_stack(ci, NULL, &frame.stack);
    if(err) return err;
    frame.stack_mark = ci_stack_mark(frame.stack);
    if(code->frame_size){
        uint32_t alignment = code->frame_alignment > 16 ? code->frame_alignment : 16;
        frame.slots = ci_stack_alloc(frame.stack, code->frame_size, alignment);
        if(!frame.slots) return CI_OOM_ERROR;
        if(ci->poison_frame_slots) memset(frame.slots, 0xCC, code->frame_size);
    }
    err = ci_interp_run(ci, &frame);
    if(!err && code->value_size)
        memcpy(result, (char*)frame.slots + code->value_slot, code->value_size);
    ci_stack_rewind(frame.stack, frame.stack_mark);
    return err;
}

static
int
ci_eval_lowered_expr(CiInterpreter*_Nonnull ci, CiInterpFrame*_Nullable parent, CcExpr*_Nonnull expr, void*_Nonnull result, size_t size){
    CiLoweredExpr code = {0};
    int err = ci_lower_standalone_expr(ci, expr, &code, &ci->deps);
    if(err) goto cleanup;
    err = ci_resolve_deps(ci, &ci->deps);
    if(err) goto cleanup;
    err = ci_run_lowered_expr(ci, parent, &code, expr->loc, result, size);
    cleanup:
    ci_lowered_expr_cleanup(&code, ci_allocator(ci));
    return err;
}

#include "ci_rt.c"
