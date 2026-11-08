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
#include "asm/ci_interp_layout.h"
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

static void ci_closure_cleanup(CiInterpreter*);

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


#define CI_RESULT_TOO_SMALL(ci, loc, sz, size) \
    ci_error((ci), (loc), "interpreter:%s:%d: result buffer too small (need %zu, have %zu)", __FILE__, __LINE__, (size_t)(sz), (size_t)(size))

static Allocator ci_allocator(CiInterpreter*);
static Allocator ci_scratch_allocator(CiInterpreter*);

static Allocator cc_allocator(CcParser*);

static const CcTargetConfig* ci_target(const CiInterpreter*);
static int ci_dlsym(CiInterpreter*, SrcLoc, Atom, const char* what, void*_Nullable*_Nonnull);
static const CcFunc*_Nullable ci_hotswap_target(const CcFunc*);
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
    void (*const _Nonnull* _Nonnull helpers)(void);
    size_t flags;
    NcClosure* _Nullable closure;
    void (*_Nullable asm_entry)(void);
    CiClosureData* _Nullable next;
};
_Static_assert(offsetof(CiClosureData, helpers) == 16 && offsetof(CiClosureData, flags) == 24, "assembly callback metadata");
static void (*const _Nonnull ci_asm_helpers[CI_ASM_HELPER_COUNT])(void);
static CiStack* _Nullable ci_prepare_callback(CiInterpreter*);

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
        #ifndef NO_NATIVE_CALL
        if(cd->asm_entry) nc_destroy_thunk((void(*_Nonnull)(void))cd->asm_entry);
        #endif
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
    void (*entry)(void) = NULL;
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
        NcClosure* nc = NULL;
        if(ci->force_c_interp || ci_target(ci)->target != CC_TARGET_NATIVE){
            err = nc_create_closure(al, func->interp_ops->native_type, ci_closure_callback, cd, &nc);
            if(!err) entry = nc_closure_get_fn(nc);
        }
        else {
            cd->helpers = ci_asm_helpers;
            cd->flags = 1 | ((size_t)ci->poison_frame_slots << 1);
            err = nc_create_thunk(cd, ci_interp_callback_asm, &cd->asm_entry);
            if(!err) entry = cd->asm_entry;
        }
        if(err){
            Allocator_free(al, cd, 1, sizeof *cd, _Alignof(CiClosureData));
            if(err == CI_UNIMPLEMENTED_ERROR)
                ci_ice(ci, func->loc, "function should've already been rejected in lowering%s", "");
            return err;
        }
    #endif
    err = ci_closure_map_put(ci, func, (void*)entry);
    if(err){
        #ifndef NO_NATIVE_CALL
        if(nc) nc_destroy_closure(al, nc);
        if(cd->asm_entry) nc_destroy_thunk((void(*_Nonnull)(void))cd->asm_entry);
        Allocator_free(al, cd, 1, sizeof *cd, _Alignof(CiClosureData));
        #endif
        return err == 1 ? CI_OOM_ERROR : CI_RUNTIME_ERROR;
    }
    // Publish only after registration succeeds, so preparation can be retried.
    func->native_func = entry;
    #ifndef NO_NATIVE_CALL
    func->native_closure = cd;
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
ci_get_stack(CiInterpreter* ci, CiStack*_Nullable*_Nonnull out){
    CiTlsThread* state;
    int err = ci_tls_get(ci, &state);
    if(err) return err;
    *out = &state->stack;
    return 0;
}

static
size_t
ci_frame_prefix_size(uint32_t alignment){
    return (sizeof(CiInterpFrame) + alignment - 1) & -(size_t)alignment;
}

static
int
ci_alloc_frame(CiInterpreter* ci, CiStack* stack, CiInterpFrame*_Nullable caller, size_t data_size, uint32_t alignment, CiInterpFrame*_Nullable*_Nonnull out){
    if(alignment < 16) alignment = 16;
    size_t prefix = ci_frame_prefix_size(alignment);
    size_t allocation_size;
    if(add_overflow(prefix, data_size, &allocation_size)) return CI_OOM_ERROR;
    void* allocation = ci_stack_alloc(stack, allocation_size, alignment);
    if(!allocation) return CI_OOM_ERROR;
    void* slots = (char*)allocation + prefix;
    CiInterpFrame* frame = (CiInterpFrame*)slots - 1;
    *frame = (CiInterpFrame){
        .parent = caller,
    };
    if(ci->poison_frame_slots) memset((frame + 1), 0xCC, data_size);
    *out = frame;
    return 0;
}

static
int
ci_new_frame(CiInterpreter* ci, CiStack* stack, CiInterpFrame*_Nullable caller, size_t data_size, uint32_t alignment, CiInterpFrame*_Nullable*_Nonnull out){
    int err = ci_stack_save(stack, 1);
    if(err) return err;
    err = ci_alloc_frame(ci, stack, caller, data_size, alignment, out);
    if(err) ci_stack_unwind_frame(stack);
    return err;
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
    CiUint128 mask;
    if(bit_width == 128) mask = ci_uint128_from_int64(-1);
    else mask = ci_uint128_sub(ci_uint128_shl(ci_uint128_from_uint64(1), bit_width), ci_uint128_from_uint64(1));
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
    CiUint128 mask;
    if(bit_width == 128) mask = ci_uint128_from_int64(-1);
    else mask = ci_uint128_sub(ci_uint128_shl(ci_uint128_from_uint64(1), bit_width), ci_uint128_from_uint64(1));
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
ci_alloc_call_frame(CiInterpreter* ci, CiStack* stack, CiInterpFrame* _Nullable caller, const CcFunc* func, CiInterpFrame* _Nullable* _Nonnull out){
    if(!func->parsed)
        return ci_ice(ci, func->loc, "function '%s' not parsed before execution", func->name->data);
    if(!func->interp_ops)
        return ci_ice(ci, func->loc, "function '%s' not lowered before execution", func->name->data);
    CiInterpFrame* frame;
    int err = ci_new_frame(ci, stack, caller, func->interp_ops->code->frame_size, (uint32_t)func->interp_ops->code->alignment, &frame);
    if(err) return err;
    frame->code = func->interp_ops->code;
    // Bootstrap a root invocation from the embedding API.
    const CiOp* entry = &frame->code->ops[1];
    assert(frame->code->ops[0].kind == CI_OP_FRAME_SAVE);
    assert(entry->kind == CI_OP_FRAME_ENTER);
    uint32_t incoming_slot = entry->frame_enter.incoming_slot;
    frame->incoming = incoming_slot == UINT32_MAX ? NULL : (NcCallState*)((char*)(frame + 1) + incoming_slot);
    frame->pc = 2;
    *out = frame;
    return 0;
}

_Static_assert(CI_OOM_ERROR == 1, "assembly allocation failure");
#define CI_ASM_OFFSET(member, offset) \
    _Static_assert(offsetof(CiOp, member) == offset, "assembly operand layout: " #member)
CI_ASM_OFFSET(alu.slot, 4);
CI_ASM_OFFSET(call_resolve.slot, 4);
CI_ASM_OFFSET(alu.src, 8);
CI_ASM_OFFSET(alu.src2, 12);
CI_ASM_OFFSET(checked.result, 4);
CI_ASM_OFFSET(checked.overflow, 8);
CI_ASM_OFFSET(checked.src, 12);
CI_ASM_OFFSET(checked.src2, 16);
CI_ASM_OFFSET(alu_imm.slot, 4);
CI_ASM_OFFSET(alu_imm.src, 8);
CI_ASM_OFFSET(alu_imm.immediate, 16);
CI_ASM_OFFSET(cmp.slot, 4);
CI_ASM_OFFSET(cmp.src, 12);
CI_ASM_OFFSET(cmp.src2, 16);
CI_ASM_OFFSET(cmp_jump.jump, 4);
CI_ASM_OFFSET(cmp_jump.src, 12);
CI_ASM_OFFSET(cmp_jump.src2, 16);
CI_ASM_OFFSET(falu32.slot, 4);
CI_ASM_OFFSET(falu32.slot_size, 8);
CI_ASM_OFFSET(falu32.src, 12);
CI_ASM_OFFSET(falu32.src2, 16);
CI_ASM_OFFSET(fcmp32.slot, 4);
CI_ASM_OFFSET(fcmp32.src, 12);
CI_ASM_OFFSET(fcmp32.src2, 16);
CI_ASM_OFFSET(constant.slot, 4);
CI_ASM_OFFSET(constant.immediate, 8);
CI_ASM_OFFSET(copy.slot, 8);
CI_ASM_OFFSET(copy.slot_size, 12);
CI_ASM_OFFSET(copy.src, 16);
CI_ASM_OFFSET(load.slot, 8);
CI_ASM_OFFSET(load.slot_size, 12);
CI_ASM_OFFSET(load.src, 16);
CI_ASM_OFFSET(load.offset, 20);
CI_ASM_OFFSET(store.slot, 8);
CI_ASM_OFFSET(store.src, 12);
CI_ASM_OFFSET(store.src_size, 16);
CI_ASM_OFFSET(store.offset, 20);
CI_ASM_OFFSET(store_imm.slot, 4);
CI_ASM_OFFSET(store_imm.offset, 8);
CI_ASM_OFFSET(store_imm.immediate, 16);
CI_ASM_OFFSET(convert.slot, 8);
CI_ASM_OFFSET(convert.src, 16);
CI_ASM_OFFSET(index.slot, 4);
CI_ASM_OFFSET(index.base, 8);
CI_ASM_OFFSET(index.index, 12);
CI_ASM_OFFSET(index.scale, 16);
CI_ASM_OFFSET(slot_addr.slot, 4);
CI_ASM_OFFSET(slot_addr.src, 12);
CI_ASM_OFFSET(jump.jump, 4);
CI_ASM_OFFSET(jump_false.slot, 8);
CI_ASM_OFFSET(return_slot.src, 8);
CI_ASM_OFFSET(return_slot.src_size, 12);
CI_ASM_OFFSET(abi_move.slot, 4);
CI_ASM_OFFSET(abi_move.reg, 8);
CI_ASM_OFFSET(abi_transfer.src, 4);
CI_ASM_OFFSET(abi_transfer.dst, 8);
CI_ASM_OFFSET(abi_transfer.size, 12);
CI_ASM_OFFSET(abi_transfer.src_pointer, 16);
CI_ASM_OFFSET(abi_transfer.dst_pointer, 20);
CI_ASM_OFFSET(istrue.slot, 8);
CI_ASM_OFFSET(istrue.src, 16);
CI_ASM_OFFSET(bounds.src, 8);
CI_ASM_OFFSET(bounds.src2, 16);
CI_ASM_OFFSET(load_bf.slot, 8);
CI_ASM_OFFSET(load_bf.src, 16);
CI_ASM_OFFSET(load_bf.offset, 20);
CI_ASM_OFFSET(store_bf.slot, 8);
CI_ASM_OFFSET(store_bf.src, 12);
CI_ASM_OFFSET(store_bf.offset, 20);
CI_ASM_OFFSET(var_addr.slot, 8);
CI_ASM_OFFSET(var_addr.var, 16);
CI_ASM_OFFSET(func_addr.slot, 8);
CI_ASM_OFFSET(func_addr.func, 16);
CI_ASM_OFFSET(memcopy.size, 4);
CI_ASM_OFFSET(memcopy.slot, 8);
CI_ASM_OFFSET(memcopy.offset, 12);
CI_ASM_OFFSET(memcopy.src, 16);
CI_ASM_OFFSET(memcopy.src_offset, 20);
CI_ASM_OFFSET(zero.size, 4);
CI_ASM_OFFSET(zero.slot, 8);
CI_ASM_OFFSET(zero.offset, 12);
CI_ASM_OFFSET(switch_.jump, 4);
CI_ASM_OFFSET(switch_.slot, 8);
CI_ASM_OFFSET(switch_.table, 16);
CI_ASM_OFFSET(bswap.slot, 8);
CI_ASM_OFFSET(bswap.src, 12);
CI_ASM_OFFSET(bit_builtin.slot, 4);
CI_ASM_OFFSET(bit_builtin.src, 12);
CI_ASM_OFFSET(bit_builtin.src2, 16);
CI_ASM_OFFSET(propagate_error.src, 4);
CI_ASM_OFFSET(check.src, 4);
CI_ASM_OFFSET(atomic_load.slot, 8);
CI_ASM_OFFSET(atomic_load.src, 16);
CI_ASM_OFFSET(atomic_load.offset, 20);
CI_ASM_OFFSET(atomic_store.slot, 8);
CI_ASM_OFFSET(atomic_store.src, 12);
CI_ASM_OFFSET(atomic_store.offset, 20);
CI_ASM_OFFSET(atomic_rmw.offset, 4);
CI_ASM_OFFSET(atomic_rmw.slot, 8);
CI_ASM_OFFSET(atomic_rmw.src, 16);
CI_ASM_OFFSET(atomic_rmw.src2, 20);
CI_ASM_OFFSET(atomic_cas.offset, 4);
CI_ASM_OFFSET(atomic_cas.slot, 8);
CI_ASM_OFFSET(atomic_cas.src, 12);
CI_ASM_OFFSET(atomic_cas.expected, 16);
CI_ASM_OFFSET(atomic_cas.desired, 20);
#undef CI_ASM_OFFSET
_Static_assert(offsetof(CiStack, registers) == 0, "assembly ABI register base");
_Static_assert(offsetof(CcSwitchEntry, low) == 0 && offsetof(CcSwitchEntry, high) == 8, "assembly switch interval layout");

_Static_assert(offsetof(CiInterpFrame, code) == CI_ASM_code, "assembly layout: code");
_Static_assert(offsetof(CiCode, ops) == CI_ASM_code_ops, "assembly layout: code ops");
_Static_assert(offsetof(CiCode, op_count) == CI_ASM_code_count, "assembly layout: code count");
_Static_assert(offsetof(CiInterpFrame, pc) == CI_ASM_pc, "assembly layout: pc");
_Static_assert(offsetof(CiInterpFrame, incoming) == CI_ASM_incoming, "assembly layout: incoming");
_Static_assert(offsetof(CiInterpFrame, parent) == CI_ASM_parent, "assembly layout: parent");
_Static_assert(sizeof(CiInterpFrame) == CI_ASM_frame_size, "assembly layout: frame_size");
_Static_assert(offsetof(CiStack, top) == CI_ASM_stack_top, "assembly layout: stack_top");
_Static_assert(offsetof(CiStack, marks) == CI_ASM_stack_marks && offsetof(CiStack, mark_count) == CI_ASM_stack_mark_count
    && offsetof(CiStack, mark_capacity) == CI_ASM_stack_mark_capacity && offsetof(CiStack, boundary) == CI_ASM_stack_boundary,
    "assembly mark stack");
_Static_assert(sizeof(CiStackSave) == 24 && offsetof(CiStackSave, boundary) == 16, "assembly saved mark");
_Static_assert(offsetof(CiStackSegment, capacity) == 8 && offsetof(CiStackSegment, data) == 16, "assembly frame allocation: segment layout");
_Static_assert(offsetof(CiStackMark, segment) == 0 && offsetof(CiStackMark, used) == 8, "assembly frame allocation: stack mark");
_Static_assert(offsetof(CiCode, frame_size) == 8 && offsetof(CiCode, alignment) == 16, "assembly frame allocation: code layout");
_Static_assert(offsetof(CiFuncOps, code) == 8, "assembly function code");
_Static_assert(offsetof(CcVariable, interp_val) == CI_ASM_var_value, "assembly layout: var_value");
_Static_assert(offsetof(CcFunc, interp_ops) == CI_ASM_func_code && offsetof(CcFunc, hotswap) == CI_ASM_func_hotswap, "assembly function entry");
_Static_assert(offsetof(CcFunc, native_func) == CI_ASM_native_func, "assembly layout: native_func");
_Static_assert(offsetof(CcFunc, frame_size) == CI_ASM_func_flags + 4, "assembly function flags");
_Static_assert(offsetof(Atom_, data) == CI_ASM_atom_data, "assembly atom text");
_Static_assert(sizeof(NcCallState) == 320, "assembly native call capture");
_Static_assert(offsetof(CiCallDescriptor, abi) == 0 && offsetof(CiCallDescriptor, invoke) == 8
    && offsetof(CiCallDescriptor, func) == 16, "assembly call descriptor");
_Static_assert(offsetof(NcCallPlan, target) == 0, "assembly native call target");
_Static_assert(offsetof(NcCallPlan, stack_size) == CI_ASM_plan_stack_size, "assembly native stack size");
_Static_assert(offsetof(NcCallPlan, stack_alignment) == CI_ASM_plan_stack_alignment, "assembly native stack alignment");
_Static_assert(offsetof(NcSysvVaListTag, gp_offset) == 0 && offsetof(NcSysvVaListTag, fp_offset) == 4
    && offsetof(NcSysvVaListTag, overflow_arg_area) == 8 && offsetof(NcSysvVaListTag, reg_save_area) == 16, "assembly SysV va_list");
_Static_assert(offsetof(NcAapcs64VaList, __stack) == 0 && offsetof(NcAapcs64VaList, __gr_top) == 8
    && offsetof(NcAapcs64VaList, __vr_top) == 16 && offsetof(NcAapcs64VaList, __gr_offs) == 24
    && offsetof(NcAapcs64VaList, __vr_offs) == 28, "assembly AAPCS64 va_list");
_Static_assert(offsetof(CiSwitchTable, count) == CI_ASM_switch_count, "assembly layout: switch_count");
_Static_assert(offsetof(CiSwitchTable, data) == CI_ASM_switch_data, "assembly layout: switch_data");
_Static_assert(sizeof(CcSwitchEntry) == CI_ASM_entry_size, "assembly layout: entry_size");
_Static_assert(offsetof(CcSwitchEntry, target) == CI_ASM_entry_target, "assembly layout: entry_target");
_Static_assert(CI_OP_COUNT == CI_ASM_op_count, "assembly opcode count");

#if defined __x86_64__ && !defined _WIN32
#define CI_CACHE_SERVICE NULL
#elif defined __APPLE__
extern void sys_icache_invalidate(void*, size_t);
#define CI_CACHE_SERVICE sys_icache_invalidate
#elif defined _WIN32
#define CI_CACHE_SERVICE FlushInstructionCache
#else
void clear_cache(void* a, void* b){
    __builtin___clear_cache(a, b);
}
#define CI_CACHE_SERVICE clear_cache
#endif
_Static_assert(sizeof(void (*)(void)) == 8, "assembly helper pointer size");
static void (*const _Nonnull ci_asm_helpers[CI_ASM_HELPER_COUNT])(void) = {
#define CI_HELPER_ENTRY(name, function) [CI_ASM_HELPER_##name] = (void (*)(void))function,
    CI_ASM_HELPERS(CI_HELPER_ENTRY)
#undef CI_HELPER_ENTRY
};
#undef CI_CACHE_SERVICE

static
CiStack* _Nullable
ci_prepare_callback(CiInterpreter* ci){
    CiStack* stack;
    if(ci_get_stack(ci, &stack)) return NULL;
    if(ci_stack_save(stack, 1)) return NULL;
    return stack;
}

#ifdef __clang__
// clang hits some pathological cases apparently in these funcs,
// disabling these warnings triples the speed of compilation.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wuninitialized"
#pragma clang diagnostic ignored "-Wsometimes-uninitialized"
#pragma clang diagnostic ignored "-Wconditional-uninitialized"
#endif
static
int
ci_interp_run_c(CiInterpreter* ci, CiInterpFrame* root, CiStack* stack, void* _Nullable root_result, size_t root_result_size){
    int error = 0;
    CiInterpFrame* frame = root;
    size_t pc = frame->pc;
    dispatch:;
    if(pc >= frame->code->op_count) goto finish;
    const CiOp* op = &frame->code->ops[pc];
    execute_op:;
    switch(op->kind){
    #define CI_OP_BEGIN(name) case name:
    #define CI_OP_END(name) goto dispatch
    #define CI_OP_SYNC_PC() (frame->pc = pc)
    #define CI_OP_CALL(code) do { op = (code)->ops; goto execute_op; } while(0)
    #define CI_OP_ENTER(child) do { frame = (child); pc = frame->pc; goto dispatch; } while(0)
    #define CI_OP_ERROR(value) do { frame->pc = pc; error = (value); goto cleanup; } while(0)
        #ifdef __clang__
        #pragma clang assume_nonnull end
        #endif
        #include "ci_interp_step.h"
        #ifdef __clang__
        #pragma clang assume_nonnull begin
        #endif
    case CI_OP_COUNT: break;
    }
    frame->pc = pc;
    error = ci_unimplemented(ci, op->loc, "unsupported op kind");
    goto cleanup;
    finish:
    frame->pc = pc;
    if(frame == root){
        if(root->code->ops[0].kind == CI_OP_FRAME_SAVE) ci_stack_unwind_frame(stack);
        return 0;
    }
    CiInterpFrame* parent = frame->parent;
    ci_stack_unwind_frame(stack);
    frame = parent;
    pc = frame->pc + 1;
    goto dispatch;
    cleanup:
    while(frame != root){
        parent = frame->parent;
        ci_stack_unwind_frame(stack);
        frame = parent;
    }
    if(root->code->ops[0].kind == CI_OP_FRAME_SAVE) ci_stack_unwind_frame(stack);
    return error;
}


static
int
ci_interp_step(CiInterpreter* ci, CiInterpFrame* root, CiInterpFrame* _Nonnull* _Nonnull current, void* _Nullable root_result, size_t root_result_size){
    CiInterpFrame* frame = *current;
    if(frame == root && frame->pc >= frame->code->op_count) return 0;
    int error = 0;
    CiStack* stack;
    error = ci_get_stack(ci, &stack);
    if(error) return error;
    size_t pc = frame->pc;
    if(pc >= frame->code->op_count) goto finish;
    const CiOp* op = &frame->code->ops[pc];
    execute_op:;
    switch(op->kind){
    #define CI_OP_BEGIN(name) case name:
    #define CI_OP_END(name) frame->pc = pc; goto finish
    #define CI_OP_SYNC_PC() ((void)0)
    #define CI_OP_CALL(code) do { op = (code)->ops; goto execute_op; } while(0)
    #define CI_OP_ENTER(child) do { frame = (child); goto finish; } while(0)
    #define CI_OP_ERROR(value) do { error = (value); goto finish; } while(0)
        #ifdef __clang__
        #pragma clang assume_nonnull end
        #endif
        #include "ci_interp_step.h"
        #ifdef __clang__
        #pragma clang assume_nonnull begin
        #endif
    case CI_OP_COUNT: break;
    }
    error = ci_unimplemented(ci, op->loc, "unsupported op kind");
    finish:
    while(frame != root && (error || frame->pc >= frame->code->op_count)){
        CiInterpFrame* parent = frame->parent;
        ci_stack_unwind_frame(stack);
        frame = parent;
        if(!error) frame->pc++;
    }
    *current = frame;
    if(frame == root && (error || frame->pc >= frame->code->op_count)
        && root->code->ops[0].kind == CI_OP_FRAME_SAVE) ci_stack_unwind_frame(stack);
    return error;
}
#ifdef __clang__
#pragma clang diagnostic pop
#endif

static
int
ci_interp_execute(CiInterpreter* ci, CiInterpFrame* root, CiStack* stack, void* _Nullable result, size_t result_size){
    int err;
    DRP_PROFILE_BEGIN(profile, "interpreter.run");
    if(!ci->force_c_interp && ci_target(ci)->target == CC_TARGET_NATIVE)
        err = ci_interp_execute_asm(ci, root, ci_asm_helpers, stack, result, result_size, (size_t)ci->poison_frame_slots << 1);
    else
        err = ci_interp_run_c(ci, root, stack, result, result_size);
    DRP_PROFILE_END(profile);
    return err;
}

static
int
ci_interp_run(CiInterpreter* ci, CiInterpFrame* root){
    CiStack* stack;
    int err = ci_get_stack(ci, &stack);
    if(err) return err;
    return ci_interp_execute(ci, root, stack, &ci->exit_code, sizeof ci->exit_code);
}

static
int
ci_call_argv(CiInterpreter* ci, CiInterpFrame* _Nullable caller, const CcFunc* func, void* _Nonnull* _Nonnull argv, uint32_t nargs, void* result, size_t size, SrcLoc loc){
    NcCallPlan* plan = func->interp_ops->abi;
    if(plan->result.size > size) return CI_RESULT_TOO_SMALL(ci, loc, plan->result.size, size);
    if(nargs != func->interp_ops->native_type->param_count) return ci_error(ci, loc, "argument count mismatch");
    CiStack* stack;
    int err = ci_get_stack(ci, &stack);
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
    int err = ci_get_stack(ci, &stack);
    if(err) return err;
    size_t capture_size = nc_capture_size(target->interp_ops->abi->target);
    if(state != &stack->registers) memcpy(&stack->registers, state, capture_size);
    CiInterpFrame* frame = NULL;
    err = ci_alloc_call_frame(ci, stack, caller, target, &frame);
    if(err) return err;
    err = ci_interp_execute(ci, frame, stack, NULL, 0);
    if(state != &stack->registers) memcpy(state, &stack->registers, capture_size);
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
            .flags = ARGPARSE_FLAGS_UNKNOWN_KWARGS_AS_ARGS
                   | ARGPARSE_FLAGS_ALLOW_KWARG_SEP_TO_BE_OPTIONAL
                   | ARGPARSE_FLAGS_IGNORE_EXCESS_ARGS,
        };
        enum ArgParseError ap_err = parse_args_cmdline(&pkg_parser, cmdline);
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
    err = ci_eval_lowered_expr(ci, ci->top_frame, expr, result, result_sz);
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
        if(!f || !f->code || f->pc >= f->code->op_count) return 1;
        const CiOp* op = &f->code->ops[f->pc];
        ci_error(ci, op->loc, "%s: %d", f->code->name?f->code->name->data:"(top level)", level);
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
    CiInterpFrame* frame;
    CiStack* stack;
    int err = ci_get_stack(ci, &stack);
    if(err) return err;
    err = ci_new_frame(ci, stack, parent, code->code->frame_size, (uint32_t)code->code->alignment, &frame);
    if(err) return err;
    frame->code = code->code;
    err = ci_interp_execute(ci, frame, stack, result, size);
    if(!err && code->value_size)
        memcpy(result, (char*)(frame + 1) + code->value_slot, code->value_size);
    ci_stack_unwind_frame(stack);
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
