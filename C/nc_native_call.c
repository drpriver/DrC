#ifndef C_NC_NATIVE_CALL_C
#define C_NC_NATIVE_CALL_C
//
// Copyright © 2026-2026, David Priver <david@davidpriver.com>
//
#include "nc_native_call.h"
#include "asm/nc_capture_config.h"
#include "nc_asm_bridge.h"
#include "cc_errors.h"
#include "cc_rt_types.h"
#include "../Drp/Allocators/mallocator.h"
#include "../Drp/bit_util.h"
#include "../Drp/ckdint.h"
#include "../Drp/thread_utils.h"
#include "../Drp/windowsheader.h"
#include "../Drp/posixheader.h"
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#ifdef _MSC_VER
#pragma comment(lib, "onecore.lib")
#endif
#ifdef __APPLE__
#include <sys/mman.h>
#include <mach/mach.h>
#include <mach/mach_vm.h>
#endif
#ifdef __linux__
#include <sys/mman.h>
#include <sys/syscall.h>
#include <linux/memfd.h>
#ifndef MREMAP_DONTUNMAP
#define MREMAP_DONTUNMAP 4
#endif
#endif
#ifdef __clang__
#pragma clang assume_nonnull begin
#endif
enum {
    NC_NO_ERROR = 0,
    NC_OOM_ERROR = _cc_oom_error,
    NC_UNSUPPORTED_TYPE = _cc_unimplemented_error,
    NC_RUNTIME_ERROR = _cc_runtime_error
};

static inline
_Bool
nc_abi_windows(CcTarget t){
    return t == CC_TARGET_X86_64_WINDOWS_MSVC
        || t == CC_TARGET_X86_64_WINDOWS_MINGW;
}

static inline
_Bool
nc_abi_sysv(CcTarget t){
    return t == CC_TARGET_X86_64_LINUX || t == CC_TARGET_X86_64_MACOS;
}

static inline
void
nc_abi_piece(NcAbiType* t, unsigned off, unsigned size, NcAbiBank bank){
    t->pieces[t->count++] = (NcAbiPiece){
        .value_offset = (uint16_t)off,
        .size = (uint16_t)size,
        .bank = bank
    };
}

static
int
nc_abi_type(const CcTargetConfig* tc, CcQualType qt, NcAbiRole role, NcAbiType* out){
    qt = ccqt_unaligned(qt);
    NcAbiType t = {.mode = NC_ABI_REGISTERS};
    uint32_t alignment = 8;
    CcTarget target = tc->target;
    _Bool aggregate = 0, fp = 0, complex = 0;
    const CcStruct* record = NULL;
    switch(ccqt_kind(qt)){
        case CC_ENUM:
            return nc_abi_type(tc, ccqt_as_enum(qt)->underlying, role, out);
        case CC_POINTER:
        case CC_BLOCK_POINTER:
        case CC_FUNCTION:
            t.size = 8;
            break;
        case CC_SLICE:
            t.size = sizeof(CiRtSlice);
            aggregate = 1;
            break;
        case CC_STRUCT:
        case CC_UNION:
            record = ccqt_as_struct(qt);
            if(record->is_incomplete) return _cc_unimplemented_error;
            t.size = record->size;
            alignment = record->alignment;
            aggregate = 1;
            break;
        case CC_ARRAY: {
            const CcArray* arr = ccqt_as_array(qt);
            if(!arr->is_vector) return _cc_unimplemented_error;
            t.size = alignment = arr->vector_size;
            if(t.size > 16) return _cc_unimplemented_error;
            if(nc_abi_windows(target)){
                if(role == NC_ABI_RETURN)
                    nc_abi_piece(&t, 0, t.size, NC_ABI_FP);
                else
                    t.mode = NC_ABI_INDIRECT;
            }
            else
                nc_abi_piece(&t, 0, t.size, NC_ABI_FP);
            goto finish;
        }
        case CC_BASIC: {
            CcBasicTypeKind k = ccqt_basic_kind(qt);
            if(k == CCBT_void){
                t.mode = NC_ABI_IGNORED;
                goto finish;
            }
            if(k == CCBT__Any){
                t.size = sizeof(CiRtAny);
                aggregate = 1;
                break;
            }
            if(k == CCBT_INVALID || k == CCBT_COUNT)
                return _cc_unimplemented_error;
            t.size = tc->sizeof_[k];
            alignment = tc->alignof_[k];
            complex = ccbt_is_complex(k);
            fp = ccbt_is_float(k);
            t.sign_extend = ccqt_is_integer(qt)
                && !ccqt_is_unsigned(qt, !tc->char_is_signed);
            // The x64 compiler extension passes int128 by reference but
            // returns its complete bit pattern in XMM0.
            if(nc_abi_windows(target) && role == NC_ABI_RETURN
            && ccqt_is_integer(qt) && t.size == 16){
                nc_abi_piece(&t, 0, 16, NC_ABI_FP);
                goto finish;
            }
            if(nc_abi_sysv(target) && tc->long_double_format == CC_LONG_DOUBLE_X87
            && (k == CCBT_long_double || k == CCBT_long_double_complex)){
                if(role == NC_ABI_RETURN){
                    nc_abi_piece(&t, 0, 10, NC_ABI_X87);
                    if(complex) nc_abi_piece(&t, 16, 10, NC_ABI_X87);
                }
                else
                    t.mode = NC_ABI_STACK_VALUE;
                goto finish;
            }
            if(complex){
                unsigned part = t.size / 2;
                if(nc_abi_windows(target)){
                    aggregate = 1;
                    break;
                }
                if(nc_abi_sysv(target) && t.size <= 8)
                    nc_abi_piece(&t, 0, t.size, NC_ABI_FP);
                else {
                    nc_abi_piece(&t, 0, part, NC_ABI_FP);
                    nc_abi_piece(&t, part, part, NC_ABI_FP);
                }
                goto finish;
            }
            if(fp){
                if(nc_abi_windows(target) && t.size > 8)
                    t.mode = NC_ABI_INDIRECT;
                else
                    nc_abi_piece(&t, 0, t.size, NC_ABI_FP);
                goto finish;
            }
            break;
        }
        case CC_ALIGNED:
            return _cc_unimplemented_error;
    }
    if(!t.size){
        t.mode = NC_ABI_IGNORED;
        goto finish;
    }
    if(target == CC_TARGET_TEST){
        // TEST has a deterministic stack-only argument ABI. Results <=16
        // bytes use two GP cells, larger results use a separate result pointer.
        if(role != NC_ABI_RETURN)
            t.mode = NC_ABI_STACK_VALUE;
        else if(t.size > 16)
            t.mode = NC_ABI_INDIRECT;
    }
    else if(nc_abi_sysv(target)){
        if(record){
            const CcSysvAggregate* a = &record->sysv;
            if(a->is_memory_class || (role != NC_ABI_RETURN && a->argument_on_stack)){
                t.mode = NC_ABI_STACK_VALUE;
                goto finish;
            }
            for(unsigned i = 0; i < a->eightbyte_count; i++){
                CcSysVEightByte c = cc_sysv_class(a, i);
                unsigned off = i * 8, size = t.size - off;
                if(size > 8) size = 8;
                if(c == CC_SYSV_NO_CLASS || c == CC_SYSV_X87UP) continue;
                if(c == CC_SYSV_SSEUP){
                    if(!t.count || t.pieces[t.count - 1].size + size > 16)
                        return _cc_unimplemented_error;
                    t.pieces[t.count - 1].size += (uint8_t)size;
                    continue;
                }
                if(t.count == 4) return _cc_unimplemented_error;
                if(c == CC_SYSV_X87)
                    nc_abi_piece(&t, off, 10, NC_ABI_X87);
                else if(c == CC_SYSV_INTEGER || c == CC_SYSV_SSE)
                    nc_abi_piece(&t, off, size, c == CC_SYSV_SSE? NC_ABI_FP : NC_ABI_GP);
                else
                    return _cc_unimplemented_error;
            }
            if(!t.count) t.mode = NC_ABI_IGNORED;
            goto finish;
        }
        if(t.size > 16) t.mode = NC_ABI_STACK_VALUE;
    }
    else if(nc_abi_windows(target)){
        if(t.size > 8 || (aggregate && (t.size & (t.size - 1))))
            t.mode = NC_ABI_INDIRECT;
    }
    else {
        if(record){
            const CcArm64Aggregate* a = &record->arm64;
            alignment = a->copy_alignment;
            if(target == CC_TARGET_AARCH64_MACOS && a->is_empty){
                t.mode = NC_ABI_IGNORED;
                goto finish;
            }
            unsigned n = a->hfa_count + a->hva_count;
            if(n){
                for(unsigned i = 0; i < n; i++)
                    nc_abi_piece(&t, cc_arm64_member_offset(a, i), a->member_size, NC_ABI_FP);
                goto finish;
            }
        }
        if(t.size > 16) t.mode = NC_ABI_INDIRECT;
    }
    if(t.mode == NC_ABI_REGISTERS){
        for(unsigned i = 0; i < t.size; i += 8)
            nc_abi_piece(&t, i, t.size - i < 8? t.size - i : 8, NC_ABI_GP);
    }
finish:
    if(!alignment) alignment = 1;
    t.align_log2 = ctz_32(alignment);
    if(role == NC_ABI_RETURN && t.mode == NC_ABI_STACK_VALUE)
        t.mode = NC_ABI_INDIRECT;
    if(role == NC_ABI_VARIADIC_ARGUMENT
    && (target == CC_TARGET_AARCH64_MACOS || target == CC_TARGET_TEST)
    && t.mode != NC_ABI_INDIRECT
    && t.mode != NC_ABI_IGNORED)
        t.mode = NC_ABI_STACK_VALUE;
    if(target == CC_TARGET_TEST
    && role == NC_ABI_NAMED_ARGUMENT
    && t.mode != NC_ABI_IGNORED)
        t.mode = NC_ABI_STACK_VALUE;
    if(t.mode != NC_ABI_REGISTERS) t.count = 0;
    *out = t;
    return 0;
}

static
int
nc_abi_assign(const CcTargetConfig* tc, NcCallPlan* p, NcAbiArgument* a, NcAbiRole role){
    NcAbiType t = a->type;
    size_t alignment = (size_t)1 << t.align_log2;
    if(t.mode == NC_ABI_IGNORED) return 0;
    unsigned gp_limit = nc_abi_sysv(tc->target)? 6 : nc_abi_windows(tc->target)? 4 : 8;
    unsigned ng = t.mode == NC_ABI_INDIRECT? 1 : _nc_abi_bank_count(t, NC_ABI_GP);
    unsigned nf = _nc_abi_fp_count(t);
    if(nc_abi_windows(tc->target)){
        ng = 1;
        nf = 0;
    }
    if(tc->target == CC_TARGET_AARCH64_LINUX && alignment >= 16 && ng && t.mode != NC_ABI_INDIRECT)
        p->gp = (p->gp + 1) & ~1u;
    a->gp = p->gp;
    a->fp = p->fp;
    a->on_stack = t.mode == NC_ABI_STACK_VALUE
        || p->gp + ng > gp_limit
        || p->fp + nf > 8
        || (tc->target == CC_TARGET_AARCH64_MACOS && role == NC_ABI_VARIADIC_ARGUMENT)
        || tc->target == CC_TARGET_TEST;
    if(a->on_stack){
        size_t align = t.mode == NC_ABI_INDIRECT? 8 : alignment;
        size_t size = t.mode == NC_ABI_INDIRECT? 8 : t.size;
        if(tc->target != CC_TARGET_AARCH64_MACOS || role == NC_ABI_VARIADIC_ARGUMENT){
            if(align < 8) align = 8;
            size = _nc_align_up(size, 8);
        }
        if(align > p->stack_alignment) p->stack_alignment = align;
        size_t offset;
        if(add_overflow(p->stack_size, align - 1, &offset))
            return _cc_overflow_error;
        offset &= ~(align - 1);
        if(add_overflow(offset, size, &p->stack_size) || p->stack_size > UINT32_MAX)
            return _cc_overflow_error;
        a->stack_offset = (uint32_t)offset;
        if(tc->target == CC_TARGET_AARCH64_LINUX || tc->target == CC_TARGET_AARCH64_MACOS){
            if(ng) p->gp = gp_limit;
            if(nf) p->fp = 8;
        }
    }
    else {
        p->gp += ng;
        p->fp += nf;
    }
    if(t.mode == NC_ABI_INDIRECT){
        if(alignment > p->stack_alignment)
            p->stack_alignment = alignment;
        size_t align = alignment > 16? alignment : 16;
        size_t offset;
        if(add_overflow(p->copy_size, align - 1, &offset))
            return _cc_overflow_error;
        offset &= ~(align - 1);
        if(add_overflow(offset, _nc_align_up(t.size, 16), &p->copy_size)
        || p->copy_size > UINT32_MAX)
            return _cc_overflow_error;
        a->copy_offset = (uint32_t)offset;
    }
    return 0;
}

static
int
nc_plan_create(Allocator al, const CcTargetConfig* tc, CcFunction* f, NcCallPlan* _Nullable* _Nonnull out){
    size_t bytes = sizeof(NcCallPlan) + (size_t)f->param_count*sizeof(NcAbiArgument);
    NcCallPlan* p = Allocator_zalloc(al, bytes);
    if(!p) return _cc_oom_error;
    p->target = tc->target;
    p->count = f->param_count;
    p->fixed_count = f->fixed_param_count;
    p->stack_alignment = 16;
    int err = nc_abi_type(tc, f->return_type, NC_ABI_RETURN, &p->result);
    if(err) goto fail;
    if(p->result.mode == NC_ABI_INDIRECT && (nc_abi_sysv(tc->target) || nc_abi_windows(tc->target)))
        p->gp = 1;
    for(unsigned i = 0; i <= p->count; i++){
        if(i == p->fixed_count){
            p->named_gp = p->gp;
            p->named_fp = p->fp;
            p->named_stack_size = p->stack_size;
        }
        if(i == p->count) break;
        NcAbiRole role = i < p->fixed_count? NC_ABI_NAMED_ARGUMENT : NC_ABI_VARIADIC_ARGUMENT;
        err = nc_abi_type(tc, f->params[i], role, &p->args[i].type);
        if(err) goto fail;
        err = nc_abi_assign(tc, p, &p->args[i], role);
        if(err) goto fail;
    }
    *out = p;
    return 0;
fail:
    Allocator_free(al, p, bytes);
    return err;
}

static
void
nc_plan_destroy(Allocator al, NcCallPlan* p){
    Allocator_free(al, p, sizeof *p + (size_t)p->count * sizeof p->args[0]);
}

static
size_t
nc_state_size(const NcCallPlan* p){
    size_t alignment = p->stack_alignment > 16? p->stack_alignment : 16;
    return sizeof(NcCallState) + 32 + alignment - 1
        + _nc_align_up(p->stack_size, alignment) + p->copy_size;
}

static
void
nc_state_init(NcCallState* s, const NcCallPlan* p, void* _Nullable result){
    memset(s, 0, nc_state_size(p));
    size_t alignment = p->stack_alignment > 16? p->stack_alignment : 16;
    unsigned char* stack = (void*)_nc_align_up((uintptr_t)s + sizeof *s + 32, alignment);
    if(nc_abi_sysv(p->target)){
        s->sysv.stack_args = stack;
        s->sysv.incoming_rax = p->fp;
        s->sysv.result_x87_count = _nc_abi_bank_count(p->result, NC_ABI_X87);
        if(p->result.mode == NC_ABI_INDIRECT)
            s->sysv.gp[0] = (uintptr_t)result;
    }
    else if(nc_abi_windows(p->target)){
        s->win.stack_args = stack;
        if(p->result.mode == NC_ABI_INDIRECT)
            s->win.gp[0] = (uintptr_t)result;
    }
    else if(p->target == CC_TARGET_TEST){
        s->test.stack_args = stack;
        s->test.indirect_result = result;
    }
    else {
        s->arm.stack_args = stack;
        s->arm.indirect_result = result;
    }
}

static
void
nc_sysv_arg_write(NcCaptureSysV64* s, const NcCallPlan* p, const NcAbiArgument* a, const void* source){
    NcAbiType t = a->type;
    if(t.mode == NC_ABI_INDIRECT){
        size_t alignment = p->stack_alignment > 16? p->stack_alignment : 16;
        void* copy = s->stack_args + _nc_align_up(p->stack_size, alignment) + a->copy_offset;
        memcpy(copy, source, t.size);
        if(a->on_stack)
            memcpy(s->stack_args + a->stack_offset, &copy, sizeof copy);
        else
            s->gp[a->gp] = (uintptr_t)copy;
    }
    else if(a->on_stack)
        memcpy(s->stack_args + a->stack_offset, source, t.size);
    else {
        unsigned gp = a->gp, fp = a->fp;
        for(unsigned i = 0; i < t.count; i++){
            NcAbiPiece piece = t.pieces[i];
            void *dest;
            if(piece.bank == NC_ABI_FP)
                dest = (void*)s->fp[fp++];
            else
                dest = (void*)&s->gp[gp++];
            memcpy(dest, (const char*)source + piece.value_offset, piece.size);
        }
        if(t.sign_extend && t.size < 8 && t.count && t.pieces[0].bank == NC_ABI_GP){
            unsigned shift = (8 - t.size) * 8;
            s->gp[a->gp] = (uint64_t)((int64_t)(s->gp[a->gp] << shift) >> shift);
        }
    }
}

static
void
nc_sysv_arg_read(const NcCaptureSysV64* s, const NcAbiArgument* a, void* dest){
    NcAbiType t = a->type;
    if(a->on_stack){
        const void* source = s->stack_args + a->stack_offset;
        if(t.mode == NC_ABI_INDIRECT) memcpy(&source, source, sizeof source);
        memcpy(dest, source, t.size);
    }
    else if(t.mode == NC_ABI_INDIRECT)
        memcpy(dest, (const void*)(uintptr_t)s->gp[a->gp], t.size);
    else {
        unsigned gp = a->gp, fp = a->fp;
        for(unsigned i = 0; i < t.count; i++){
            NcAbiPiece piece = t.pieces[i];
            const void *source;
            if(piece.bank == NC_ABI_FP)
                source = (const void*)s->fp[fp++];
            else
                source = (const void*)&s->gp[gp++];
            memcpy((char*)dest + piece.value_offset, source, piece.size);
        }
    }
}

static
void
nc_arm64_arg_write(NcCaptureArm64* s, const NcCallPlan* p, const NcAbiArgument* a, const void* source){
    NcAbiType t = a->type;
    if(t.mode == NC_ABI_INDIRECT){
        size_t alignment = p->stack_alignment > 16? p->stack_alignment : 16;
        void* copy = s->stack_args + _nc_align_up(p->stack_size, alignment) + a->copy_offset;
        memcpy(copy, source, t.size);
        if(a->on_stack)
            memcpy(s->stack_args + a->stack_offset, &copy, sizeof copy);
        else
            s->gp[a->gp] = (uintptr_t)copy;
    }
    else if(a->on_stack)
        memcpy(s->stack_args + a->stack_offset, source, t.size);
    else {
        unsigned gp = a->gp, fp = a->fp;
        for(unsigned i = 0; i < t.count; i++){
            NcAbiPiece piece = t.pieces[i];
            void *dest;
            if(piece.bank == NC_ABI_FP)
                dest = (void*)s->fp[fp++];
            else
                dest = (void*)&s->gp[gp++];
            memcpy(dest, (const char*)source + piece.value_offset, piece.size);
        }
        if(t.sign_extend && t.size < 8 && t.count && t.pieces[0].bank == NC_ABI_GP){
            unsigned shift = (8 - t.size) * 8;
            s->gp[a->gp] = (uint64_t)((int64_t)(s->gp[a->gp] << shift) >> shift);
        }
    }
}

static
void
nc_arm64_arg_read(const NcCaptureArm64* s, const NcAbiArgument* a, void* dest){
    NcAbiType t = a->type;
    if(a->on_stack){
        const void* source = s->stack_args + a->stack_offset;
        if(t.mode == NC_ABI_INDIRECT) memcpy(&source, source, sizeof source);
        memcpy(dest, source, t.size);
    }
    else if(t.mode == NC_ABI_INDIRECT)
        memcpy(dest, (const void*)(uintptr_t)s->gp[a->gp], t.size);
    else {
        unsigned gp = a->gp, fp = a->fp;
        for(unsigned i = 0; i < t.count; i++){
            NcAbiPiece piece = t.pieces[i];
            const void *source;
            if(piece.bank == NC_ABI_FP)
                source = (const void*)s->fp[fp++];
            else
                source = (const void*)&s->gp[gp++];
            memcpy((char*)dest + piece.value_offset, source, piece.size);
        }
    }
}

static
void
nc_win64_arg_write(NcCaptureWin64* s, const NcCallPlan* p, const NcAbiArgument* a, const void* source){
    NcAbiType t = a->type;
    if(t.mode == NC_ABI_INDIRECT){
        size_t alignment = p->stack_alignment > 16? p->stack_alignment : 16;
        void* copy = s->stack_args + _nc_align_up(p->stack_size, alignment) + a->copy_offset;
        memcpy(copy, source, t.size);
        if(a->on_stack)
            memcpy(s->stack_args + a->stack_offset, &copy, sizeof copy);
        else
            s->gp[a->gp] = (uintptr_t)copy;
    }
    else if(a->on_stack)
        memcpy(s->stack_args + a->stack_offset, source, t.size);
    else {
        memcpy(&s->gp[a->gp], source, t.size);
        // Variadic FP arguments are supplied in both the GP and XMM banks.
        memcpy(s->fp[a->gp], source, t.size);
    }
    memcpy(s->stack_args - 32, s->gp, 32);
}

static
void
nc_win64_arg_read(const NcCaptureWin64* s, const NcAbiArgument* a, void* dest){
    NcAbiType t = a->type;
    if(a->on_stack){
        const void* source = s->stack_args + a->stack_offset;
        if(t.mode == NC_ABI_INDIRECT) memcpy(&source, source, sizeof source);
        memcpy(dest, source, t.size);
    }
    else if(t.mode == NC_ABI_INDIRECT)
        memcpy(dest, (const void*)(uintptr_t)s->gp[a->gp], t.size);
    else {
        const void *source;
        if(t.count && t.pieces[0].bank == NC_ABI_FP)
            source = (const void*)s->fp[a->gp];
        else
            source = (const void*)&s->gp[a->gp];
        memcpy(dest, source, t.size);
    }
}

static
void
nc_test_arg_write(NcCaptureTest* s, const NcAbiArgument* a, const void* source){
    memcpy(s->stack_args + a->stack_offset, source, a->type.size);
}

static
void
nc_test_arg_read(const NcCaptureTest* s, const NcAbiArgument* a, void* dest){
    memcpy(dest, s->stack_args + a->stack_offset, a->type.size);
}

static
void
nc_arg_write(NcCallState* s, const NcCallPlan* p, unsigned index, const void* source){
    const NcAbiArgument* a = &p->args[index];
    if(a->type.mode == NC_ABI_IGNORED) return;
    switch(p->target){
        case CC_TARGET_X86_64_LINUX:
        case CC_TARGET_X86_64_MACOS:
            nc_sysv_arg_write(&s->sysv, p, a, source);
            break;
        case CC_TARGET_X86_64_WINDOWS_MSVC:
        case CC_TARGET_X86_64_WINDOWS_MINGW:
            nc_win64_arg_write(&s->win, p, a, source);
            break;
        case CC_TARGET_AARCH64_LINUX:
        case CC_TARGET_AARCH64_MACOS:
            nc_arm64_arg_write(&s->arm, p, a, source);
            break;
        case CC_TARGET_TEST:
            nc_test_arg_write(&s->test, a, source);
            break;
        case CC_TARGET_COUNT:
            unreachable();
    }
}

static
void
nc_arg_read(NcCallState* s, const NcCallPlan* p, unsigned index, void* dest){
    const NcAbiArgument* a = &p->args[index];
    if(a->type.mode == NC_ABI_IGNORED) return;
    memset(dest, 0, a->type.size);
    switch(p->target){
        case CC_TARGET_X86_64_LINUX:
        case CC_TARGET_X86_64_MACOS:
            nc_sysv_arg_read(&s->sysv, a, dest);
            break;
        case CC_TARGET_X86_64_WINDOWS_MSVC:
        case CC_TARGET_X86_64_WINDOWS_MINGW:
            nc_win64_arg_read(&s->win, a, dest);
            break;
        case CC_TARGET_AARCH64_LINUX:
        case CC_TARGET_AARCH64_MACOS:
            nc_arm64_arg_read(&s->arm, a, dest);
            break;
        case CC_TARGET_TEST:
            nc_test_arg_read(&s->test, a, dest);
            break;
        case CC_TARGET_COUNT:
            unreachable();
    }
}

static
void
nc_sysv_result_write(NcCaptureSysV64* s, NcAbiType t, const void* value){
    if(t.mode == NC_ABI_INDIRECT){
        void* result = (void*)(uintptr_t)s->gp[0];
        memmove(result, value, t.size);
        s->result_gp[0] = (uintptr_t)result;
        return;
    }
    unsigned gp = 0, fp = 0, x87 = 0;
    for(unsigned i = 0; i < t.count; i++){
        NcAbiPiece piece = t.pieces[i];
        void* reg;
        if(piece.bank == NC_ABI_FP)
            reg = s->result_fp[fp++];
        else if(piece.bank == NC_ABI_X87)
            reg = s->result_x87[x87++];
        else
            reg = &s->result_gp[gp++];
        const void* obj = (const char*)value + piece.value_offset;
        memcpy(reg, obj, piece.size);
    }
    s->result_x87_count = x87;
    if(t.sign_extend && t.size < 8 && gp){
        unsigned shift = (8 - t.size) * 8;
        s->result_gp[0] = (uint64_t)((int64_t)(s->result_gp[0] << shift) >> shift);
    }
}

static
void
nc_sysv_result_read(const NcCaptureSysV64* s, NcAbiType t, void* value){
    if(t.mode == NC_ABI_INDIRECT){
        void* result = (void*)(uintptr_t)s->gp[0];
        memmove(value, result, t.size);
        return;
    }
    unsigned gp = 0, fp = 0, x87 = 0;
    for(unsigned i = 0; i < t.count; i++){
        NcAbiPiece piece = t.pieces[i];
        const void* reg;
        if(piece.bank == NC_ABI_FP)
            reg = s->result_fp[fp++];
        else if(piece.bank == NC_ABI_X87)
            reg = s->result_x87[x87++];
        else
            reg = &s->result_gp[gp++];
        void* obj = (char*)value + piece.value_offset;
        memcpy(obj, reg, piece.size);
    }
}

static
void
nc_arm64_result_write(NcCaptureArm64* s, NcAbiType t, const void* value){
    if(t.mode == NC_ABI_INDIRECT){
        void* result = s->indirect_result;
        memmove(result, value, t.size);
        return;
    }
    unsigned gp = 0, fp = 0;
    for(unsigned i = 0; i < t.count; i++){
        NcAbiPiece piece = t.pieces[i];
        void* reg;
        if(piece.bank == NC_ABI_FP)
            reg = s->result_fp[fp++];
        else
            reg = &s->result_gp[gp++];
        const void* obj = (const char*)value + piece.value_offset;
        memcpy(reg, obj, piece.size);
    }
    if(t.sign_extend && t.size < 8 && gp){
        unsigned shift = (8 - t.size) * 8;
        s->result_gp[0] = (uint64_t)((int64_t)(s->result_gp[0] << shift) >> shift);
    }
}

static
void
nc_arm64_result_read(const NcCaptureArm64* s, NcAbiType t, void* value){
    if(t.mode == NC_ABI_INDIRECT){
        void* result = s->indirect_result;
        memmove(value, result, t.size);
        return;
    }
    unsigned gp = 0, fp = 0;
    for(unsigned i = 0; i < t.count; i++){
        NcAbiPiece piece = t.pieces[i];
        const void* reg;
        if(piece.bank == NC_ABI_FP)
            reg = s->result_fp[fp++];
        else
            reg = &s->result_gp[gp++];
        void* obj = (char*)value + piece.value_offset;
        memcpy(obj, reg, piece.size);
    }
}

static
void
nc_win64_result_write(NcCaptureWin64* s, NcAbiType t, const void* value){
    if(t.mode == NC_ABI_INDIRECT){
        void* result = (void*)(uintptr_t)s->gp[0];
        memmove(result, value, t.size);
        s->result_gp[0] = (uintptr_t)result;
        return;
    }
    unsigned gp = 0;
    for(unsigned i = 0; i < t.count; i++){
        NcAbiPiece piece = t.pieces[i];
        void* reg;
        if(piece.bank == NC_ABI_FP)
            reg = s->result_fp;
        else
            reg = &s->result_gp[gp++];
        const void* obj = (const char*)value + piece.value_offset;
        memcpy(reg, obj, piece.size);
    }
    if(t.sign_extend && t.size < 8 && gp){
        unsigned shift = (8 - t.size) * 8;
        s->result_gp[0] = (uint64_t)((int64_t)(s->result_gp[0] << shift) >> shift);
    }
}

static
void
nc_win64_result_read(const NcCaptureWin64* s, NcAbiType t, void* value){
    if(t.mode == NC_ABI_INDIRECT){
        void* result = (void*)(uintptr_t)s->gp[0];
        memmove(value, result, t.size);
        return;
    }
    unsigned gp = 0;
    for(unsigned i = 0; i < t.count; i++){
        NcAbiPiece piece = t.pieces[i];
        const void* reg;
        if(piece.bank == NC_ABI_FP)
            reg = s->result_fp;
        else
            reg = &s->result_gp[gp++];
        void* obj = (char*)value + piece.value_offset;
        memcpy(obj, reg, piece.size);
    }
}

static
void
nc_test_result_write(NcCaptureTest* s, NcAbiType t, const void* value){
    if(t.mode == NC_ABI_INDIRECT){
        memmove(s->indirect_result, value, t.size);
        return;
    }
    unsigned gp = 0, fp = 0;
    for(unsigned i = 0; i < t.count; i++){
        NcAbiPiece piece = t.pieces[i];
        void *cell;
        if(piece.bank == NC_ABI_FP)
            cell = (void*)s->result_fp[fp++];
        else
            cell = (void*)&s->result_gp[gp++];
        const void* obj = (const char*)value + piece.value_offset;
        memcpy(cell, obj, piece.size);
    }
    if(t.sign_extend && t.size < 8 && gp){
        unsigned shift = (8 - t.size) * 8;
        s->result_gp[0] = (uint64_t)((int64_t)(s->result_gp[0] << shift) >> shift);
    }
}

static
void
nc_test_result_read(const NcCaptureTest* s, NcAbiType t, void* value){
    if(t.mode == NC_ABI_INDIRECT){
        memmove(value, s->indirect_result, t.size);
        return;
    }
    unsigned gp = 0, fp = 0;
    for(unsigned i = 0; i < t.count; i++){
        NcAbiPiece piece = t.pieces[i];
        const void *cell;
        if(piece.bank == NC_ABI_FP)
            cell = (const void*)s->result_fp[fp++];
        else
            cell = (const void*)&s->result_gp[gp++];
        void *obj = (char*)value + piece.value_offset;
        memcpy(obj, cell, piece.size);
    }
}

static
void
nc_result_write(NcCallState* s, const NcCallPlan* p, const void* value){
    NcAbiType t = p->result;
    if(t.mode == NC_ABI_IGNORED) return;
    switch(p->target){
        case CC_TARGET_X86_64_LINUX:
        case CC_TARGET_X86_64_MACOS:
            nc_sysv_result_write(&s->sysv, t, value);
            break;
        case CC_TARGET_X86_64_WINDOWS_MSVC:
        case CC_TARGET_X86_64_WINDOWS_MINGW:
            nc_win64_result_write(&s->win, t, value);
            break;
        case CC_TARGET_AARCH64_LINUX:
        case CC_TARGET_AARCH64_MACOS:
            nc_arm64_result_write(&s->arm, t, value);
            break;
        case CC_TARGET_TEST:
            nc_test_result_write(&s->test, t, value);
            break;
        case CC_TARGET_COUNT:
            unreachable();
    }
}

static
void
nc_result_read(const NcCallState* s, const NcCallPlan* p, void* value){
    NcAbiType t = p->result;
    if(t.mode == NC_ABI_IGNORED) return;
    if(t.mode != NC_ABI_INDIRECT) memset(value, 0, t.size);
    switch(p->target){
        case CC_TARGET_X86_64_LINUX:
        case CC_TARGET_X86_64_MACOS:
            nc_sysv_result_read(&s->sysv, t, value);
            break;
        case CC_TARGET_X86_64_WINDOWS_MSVC:
        case CC_TARGET_X86_64_WINDOWS_MINGW:
            nc_win64_result_read(&s->win, t, value);
            break;
        case CC_TARGET_AARCH64_LINUX:
        case CC_TARGET_AARCH64_MACOS:
            nc_arm64_result_read(&s->arm, t, value);
            break;
        case CC_TARGET_TEST:
            nc_test_result_read(&s->test, t, value);
            break;
        case CC_TARGET_COUNT:
            unreachable();
    }
}

#ifndef NO_NATIVE_CALL
static
int
nc_invoke(const NcCallPlan* p, void (*fn)(void), NcCallState* state){
    nc_abi_invoke(state, fn, p->stack_size, p->stack_alignment);
    return 0;
}
typedef struct NcCaptureSlot NcCaptureSlot;
struct NcCaptureSlot {
    NcClosure* _Nullable closure;
    void (*dispatch)(NcClosure* closure, NcCallState* capture);
    void (*entry)(void);
    uint64_t reserved;
};
_Static_assert(offsetof(NcCaptureSlot, closure) == 0, "Closure offset");
_Static_assert(offsetof(NcCaptureSlot, dispatch) == 8, "Dispatch offset");
_Static_assert(offsetof(NcCaptureSlot, entry) == 16, "Entry offset");
_Static_assert(sizeof(NcCaptureSlot) == 32, "Slot stride");
enum NcCaptureMapError {
    NC_MAP_ERROR_OK,
    NC_MAP_ERROR_INVALID,
    NC_MAP_ERROR_OOM,
    NC_MAP_ERROR_UNSUPPORTED,
    NC_MAP_ERROR_SYSTEM,
};

typedef struct NcCaptureTable NcCaptureTable;
typedef struct NcCaptureHandle NcCaptureHandle;
struct NcCaptureHandle {
    void (*_Nullable fn)(void); // Cast to the callback's actual function pointer type.
};

struct NcClosure {
    void (*fn)(void);
    NcClosureCallback* cb;
    NcCallPlan* plan;
    void* userdata;
};
static
void
nc_capture_dispatch(NcClosure* c, NcCallState* state){
    // Assembly fills only the incoming registers. In particular, a void
    // callback must not accidentally request an x87 result from stack garbage.
    size_t offset, size;
    if(nc_abi_sysv(CC_TARGET_NATIVE)){
        offset = offsetof(NcCaptureSysV64, result_gp);
        size = sizeof(NcCaptureSysV64);
    }
    else if(nc_abi_windows(CC_TARGET_NATIVE)){
        offset = offsetof(NcCaptureWin64, result_gp);
        size = sizeof(NcCaptureWin64);
    }
    else {
        offset = offsetof(NcCaptureArm64, result_gp);
        size = sizeof(NcCaptureArm64);
    }
    memset((unsigned char*)state + offset, 0, size - offset);
    c->cb(c->plan, state, c->userdata);
}
static int native_capture_allocate(NcClosure*, NcCaptureHandle*);
static int native_capture_release(NcCaptureHandle);
static struct {
    LOCK_T lock;
    NcCaptureTable* _Nullable table;
} NC_CAPTURE_ALLOCATOR = {LOCK_T_INIT};

static
int
nc_create_closure(Allocator al, CcFunction* type, NcClosureCallback* cb, void*_Null_unspecified userdata, NcClosure* _Nullable* _Nonnull out){
    NcCallPlan* plan = NULL;
    int err = nc_plan_create(al, &cc_target_native, type, &plan);
    if(err) return err;
    NcClosure* c = Allocator_zalloc(al, sizeof *c);
    if(!c){
        nc_plan_destroy(al, plan);
        return NC_OOM_ERROR;
    }
    c->cb = cb;
    c->plan = plan;
    c->userdata = userdata;
    NcCaptureHandle handle;
    err = native_capture_allocate(c, &handle);
    if(err){
        nc_plan_destroy(al, plan);
        Allocator_free(al, c, sizeof *c);
        return err == NC_MAP_ERROR_OOM? NC_OOM_ERROR : NC_RUNTIME_ERROR;
    }
    c->fn = (void (*_Nonnull)(void))handle.fn;
    *out = c;
    return 0;
}
static void (*nc_closure_get_fn(NcClosure* c))(void){
    return c->fn;
}
static
void
nc_destroy_closure(Allocator al, NcClosure* c){
    native_capture_release((NcCaptureHandle){c->fn});
    nc_plan_destroy(al, c->plan);
    Allocator_free(al, c, sizeof *c);
}
struct NcCaptureTable {
    NcCaptureTable* _Nullable next;
    unsigned char* code;
    unsigned used;
    uint64_t occupied[NC_CAPTURE_SLOT_COUNT / 64];
};
_Static_assert(NC_CAPTURE_SLOT_COUNT % 64 == 0, "bitmap");

#ifdef _WIN32
static
int
nc_capture_map_region(unsigned char* _Nonnull* _Nonnull out){
    HANDLE this_proc = GetCurrentProcess();
    BOOL ok = 1;
    SYSTEM_INFO info;
    GetSystemInfo(&info);
    if(!info.dwPageSize || NC_CAPTURE_TABLE_SIZE % info.dwPageSize)
        return NC_MAP_ERROR_UNSUPPORTED;
    // shared template, we're under the NC_CAPTURE_ALLOCATOR.lock, so this is safe.
    static HANDLE section;
    if(!section){
        HANDLE created = CreateFileMappingW(
            INVALID_HANDLE_VALUE, NULL,
            PAGE_EXECUTE_READWRITE, 0,
            NC_CAPTURE_TABLE_SIZE, NULL
        );
        if(!created) return NC_MAP_ERROR_SYSTEM;
        void* writable = MapViewOfFile(created, FILE_MAP_WRITE, 0, 0, NC_CAPTURE_TABLE_SIZE);
        if(!writable){
            CloseHandle(created);
            return NC_MAP_ERROR_SYSTEM;
        }
        memcpy(writable, nc_capture_slots, NC_CAPTURE_TABLE_SIZE);
        if(!UnmapViewOfFile(writable)){
            CloseHandle(created);
            return NC_MAP_ERROR_SYSTEM;
        }
        section = created;
    }

    unsigned char* base = VirtualAlloc2(
        this_proc, NULL,
        NC_CAPTURE_TABLE_SIZE * 2, MEM_RESERVE | MEM_RESERVE_PLACEHOLDER,
        PAGE_NOACCESS, NULL, 0
    );
    if(!base) return NC_MAP_ERROR_SYSTEM;
    _Bool split = 0,
          mapped = 0;
    ok = VirtualFree(base, NC_CAPTURE_TABLE_SIZE, MEM_RELEASE | MEM_PRESERVE_PLACEHOLDER);
    if(!ok) goto fail;
    split = 1;
    ok = MapViewOfFile3(
        section, this_proc,
        base, 0, NC_CAPTURE_TABLE_SIZE,
        MEM_REPLACE_PLACEHOLDER, PAGE_EXECUTE_READ,
        NULL, 0
    ) != NULL;
    if(!ok) goto fail;
    mapped = 1;
    if(!FlushInstructionCache(this_proc, base, NC_CAPTURE_TABLE_SIZE))
        goto fail;
    ok = VirtualAlloc2(
        this_proc,
        base + NC_CAPTURE_TABLE_SIZE, NC_CAPTURE_TABLE_SIZE,
        MEM_RESERVE | MEM_COMMIT | MEM_REPLACE_PLACEHOLDER,
        PAGE_READWRITE, NULL, 0
    ) != NULL;
    if(!ok) goto fail;
    *out = base;
    return NC_MAP_ERROR_OK;
fail:
    if(mapped)
        UnmapViewOfFile(base);
    else
        VirtualFree(base, 0, MEM_RELEASE);
    if(split) VirtualFree(base + NC_CAPTURE_TABLE_SIZE, 0, MEM_RELEASE);
    return NC_MAP_ERROR_SYSTEM;
}

static
void
nc_capture_unmap_region(unsigned char* base){
    UnmapViewOfFile(base);
    VirtualFree(base + NC_CAPTURE_TABLE_SIZE, 0, MEM_RELEASE);
}
#else

static
int
nc_capture_map_region(unsigned char* _Nonnull* _Nonnull out){
    long page_size = sysconf(_SC_PAGESIZE);
    if(page_size <= 0
    || NC_CAPTURE_TABLE_SIZE % page_size
    || (uintptr_t)nc_capture_slots % (uintptr_t)page_size)
        return NC_MAP_ERROR_UNSUPPORTED;
    unsigned char* base = mmap(
        NULL, NC_CAPTURE_TABLE_SIZE * 2,
        PROT_READ | PROT_WRITE,
        MAP_PRIVATE | MAP_ANON,
        -1, 0
    );
    if(base == MAP_FAILED) return NC_MAP_ERROR_SYSTEM;
    #ifdef __APPLE__
        mach_vm_address_t target = (mach_vm_address_t)base;
        vm_prot_t current, maximum;
        kern_return_t err = mach_vm_remap(
            mach_task_self(), &target,
            NC_CAPTURE_TABLE_SIZE, 0,
            VM_FLAGS_FIXED | VM_FLAGS_OVERWRITE, mach_task_self(),
            (mach_vm_address_t)nc_capture_slots, FALSE,
            &current, &maximum, VM_INHERIT_NONE
        );
        if(err != KERN_SUCCESS)
            goto fail;
        if(target != (mach_vm_address_t)base
        || !(current & VM_PROT_READ)
        || !(current & VM_PROT_EXECUTE)
        || (current & VM_PROT_WRITE))
            goto fail;
    #else
        static _Bool mremap_failed = 0;
        void* code = MAP_FAILED;
        if(!mremap_failed){
            // try to just directly mremap the backing into place.
            code = mremap(
                (void*)(uintptr_t)nc_capture_slots,
                NC_CAPTURE_TABLE_SIZE, NC_CAPTURE_TABLE_SIZE,
                MREMAP_MAYMOVE | MREMAP_FIXED | MREMAP_DONTUNMAP,
                base
            );
            if(code != MAP_FAILED)
                goto success;
            mremap_failed = 1;
        }
        static _Bool fd_failed = 0;
        if(!fd_failed){
            static int fd = -1;
            if(fd < 0){
                fd = (int)syscall(SYS_memfd_create, "nc-thunks", MFD_CLOEXEC | MFD_ALLOW_SEALING);
                if(fd < 0){
                    fd_failed = 1;
                    goto try_template;
                }
                for(ssize_t written = 0; written < (ssize_t)NC_CAPTURE_TABLE_SIZE;){
                    ssize_t n = write(fd, nc_capture_slots+written, NC_CAPTURE_TABLE_SIZE - written);
                    if(n < 0 && errno == EINTR) continue;
                    if(n <= 0){
                        close(fd);
                        fd_failed = 1;
                        goto try_template;
                    }
                    written += n;
                }
                if(fcntl(fd, F_ADD_SEALS, F_SEAL_WRITE | F_SEAL_GROW | F_SEAL_SHRINK | F_SEAL_SEAL) < 0){
                    close(fd);
                    fd_failed = 1;
                    goto try_template;
                }
            }
            code = mmap(
                base, NC_CAPTURE_TABLE_SIZE,
                PROT_READ | PROT_EXEC,
                MAP_SHARED | MAP_FIXED,
                fd, 0
            );
            if(code != MAP_FAILED)
                goto success;
            close(fd);
            fd_failed = 1;
        }
        try_template:;
        if(code == MAP_FAILED){
            // Fallback: use a shared RX template.

            // A failed fixed mapping may have removed the code half. Reserve
            // it again before allocating the template so they cannot overlap.
            code = mmap(
                base, NC_CAPTURE_TABLE_SIZE, PROT_NONE,
                MAP_PRIVATE | MAP_ANON | MAP_FIXED, -1, 0
            );
            if(code == MAP_FAILED)
                goto fail;
            static void* template;
            if(!template){
                void *p = mmap(
                    NULL, NC_CAPTURE_TABLE_SIZE,
                    PROT_READ | PROT_WRITE,
                    MAP_SHARED | MAP_ANON,
                    -1, 0
                );
                if(p == MAP_FAILED) goto fail;
                memcpy(p, nc_capture_slots, NC_CAPTURE_TABLE_SIZE);
                __builtin___clear_cache((char*)p, (char*)p+NC_CAPTURE_TABLE_SIZE);
                if(mprotect(p, NC_CAPTURE_TABLE_SIZE, PROT_READ|PROT_EXEC) != 0){
                    munmap(p, NC_CAPTURE_TABLE_SIZE);
                    goto fail;
                }
                template = p;
            }
            code = mremap(
                template, 0, NC_CAPTURE_TABLE_SIZE,
                MREMAP_MAYMOVE | MREMAP_FIXED,
                base
            );
            if(code == MAP_FAILED)
                goto fail;
        }
    success:
    #endif
    *out = base;
    return NC_MAP_ERROR_OK;
fail:
    munmap(base, NC_CAPTURE_TABLE_SIZE * 2);
    return NC_MAP_ERROR_SYSTEM;
}

static
void
nc_capture_unmap_region(unsigned char* base){
    munmap(base, NC_CAPTURE_TABLE_SIZE * 2);
}
#endif

static
int
native_capture_allocate(NcClosure* closure, NcCaptureHandle* out){
    int err;
    if((uintptr_t)nc_capture_slots_end - (uintptr_t)nc_capture_slots != NC_CAPTURE_TABLE_SIZE)
        return NC_MAP_ERROR_UNSUPPORTED;
    LOCK_T_lock(&NC_CAPTURE_ALLOCATOR.lock);
    NcCaptureTable* table = NC_CAPTURE_ALLOCATOR.table;
    for(; table && table->used == NC_CAPTURE_SLOT_COUNT - 1; table = table->next){
    }
    if(!table){
        table = Allocator_zalloc(MALLOCATOR, sizeof *table);
        if(!table){
            err = NC_MAP_ERROR_OOM;
            goto unlock;
        }
        err = nc_capture_map_region(&table->code);
        if(err){
            Allocator_free(MALLOCATOR, table, sizeof *table);
            goto unlock;
        }
        // Reserve the first trampoline so funcsan doesn't
        // fault trying to probe.
        table->occupied[0] = 1;
        table->next = NC_CAPTURE_ALLOCATOR.table;
        NC_CAPTURE_ALLOCATOR.table = table;
    }
    NcCaptureSlot* slots = (NcCaptureSlot*)(table->code + NC_CAPTURE_TABLE_SIZE);
    unsigned word = 0;
    while(table->occupied[word] == UINT64_MAX)
        word++;
    unsigned bit = (unsigned)ctz_64(~table->occupied[word]);
    unsigned index = word * 64 + bit;
    table->occupied[word] |= UINT64_C(1) << bit;
    slots[index] = (NcCaptureSlot){closure, nc_capture_dispatch, nc_capture_entry, 0};
    table->used++;
    out->fn = (void (*)(void))(uintptr_t)(table->code + index * sizeof(NcCaptureSlot));
    err = NC_MAP_ERROR_OK;
unlock:
    LOCK_T_unlock(&NC_CAPTURE_ALLOCATOR.lock);
    return err;
}

static
int
native_capture_release(NcCaptureHandle handle){
    int err;
    uintptr_t address = (uintptr_t)handle.fn;
    LOCK_T_lock(&NC_CAPTURE_ALLOCATOR.lock);
    for(NcCaptureTable** link = &NC_CAPTURE_ALLOCATOR.table; *link; link = &(*link)->next){
        NcCaptureTable* table = *link;
        uintptr_t start = (uintptr_t)table->code;
        if(address < start || address - start >= NC_CAPTURE_TABLE_SIZE)
            continue;
        size_t offset = address - start;
        if(!offset || offset % sizeof(NcCaptureSlot)){
            err = NC_MAP_ERROR_INVALID;
            goto unlock;
        }
        unsigned index = (unsigned)(offset / sizeof(NcCaptureSlot));
        NcCaptureSlot* slots = (NcCaptureSlot*)(table->code + NC_CAPTURE_TABLE_SIZE);
        uint64_t bit = UINT64_C(1) << (index % 64);
        if(!(table->occupied[index / 64] & bit)){
            err = NC_MAP_ERROR_INVALID;
            goto unlock;
        }
        memset(&slots[index], 0, sizeof slots[index]);
        table->occupied[index / 64] &= ~bit;
        table->used--;
        if(!table->used){
            *link = table->next;
            nc_capture_unmap_region(table->code);
            Allocator_free(MALLOCATOR, table, sizeof *table);
        }
        err = NC_MAP_ERROR_OK;
        goto unlock;
    }
    err = NC_MAP_ERROR_INVALID;
unlock:
    LOCK_T_unlock(&NC_CAPTURE_ALLOCATOR.lock);
    return err;
}

#else
static
int
nc_invoke(const NcCallPlan* p, void (*fn)(void), NcCallState* s){
    (void)p;
    (void)fn;
    (void)s;
    return _cc_unreachable_error;
}
static
int
nc_create_closure(Allocator al, CcFunction* t, NcClosureCallback* cb,
    void*_Null_unspecified ud, NcClosure* _Nullable* _Nonnull out){
    (void)al;
    (void)t;
    (void)cb;
    (void)ud;
    (void)out;
    return NC_UNSUPPORTED_TYPE;
}
static void (*nc_closure_get_fn(NcClosure* c))(void){
    return (void (*)(void))(uintptr_t)c;
}
static
void
nc_destroy_closure(Allocator al, NcClosure* c){
    (void)al;
    (void)c;
}
#endif
#ifdef __clang__
#pragma clang assume_nonnull end
#endif
#endif
