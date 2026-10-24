#ifndef NC_ABI_C
#define NC_ABI_C
#include "nc_native_call.h"
#ifdef __clang__
#pragma clang assume_nonnull begin
#endif
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
        case CC_VLA: return _cc_invalid_value_error;
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
        case CC_ARRAY: return _cc_unimplemented_error;
        case CC_VECTOR: {
            const CcVector* arr = ccqt_as_vector(qt);
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

#ifdef __clang__
#pragma clang assume_nonnull end
#endif
#endif
