#ifndef C_CI_VA_LIST_H
#define C_CI_VA_LIST_H
//
// Copyright © 2026-2026, David Priver <david@davidpriver.com>
//
#include <stddef.h>
#include <string.h>
#include "nc_native_call.h"

#ifdef __clang__
#pragma clang assume_nonnull begin
#else
#ifndef _Nullable
#define _Nullable
#endif
#endif

typedef struct NcSysvVaListTag NcSysvVaListTag;
struct NcSysvVaListTag {
    unsigned gp_offset;
    unsigned fp_offset;
    void* overflow_arg_area;
    void* _Nullable reg_save_area;
};

typedef struct NcAapcs64VaList NcAapcs64VaList;
struct NcAapcs64VaList {
    void* __stack;
    void* _Nullable __gr_top;
    void* _Nullable __vr_top;
    int __gr_offs; // negative, counts up toward 0
    int __vr_offs;
};

static
void
nc_sysv_va_start(NcCaptureSysV64* state, const NcCallPlan* p, void* dest){
    NcSysvVaListTag ap = {
        .gp_offset = p->named_gp * 8,
        .fp_offset = 48 + p->named_fp * 16,
        .overflow_arg_area = state->stack_args + p->named_stack_size,
        .reg_save_area = state->gp,
    };
    memcpy(dest, &ap, sizeof ap);
}
static
void
nc_aapcs64_va_start(NcCaptureArm64* state, const NcCallPlan* p, void* dest){
    NcAapcs64VaList ap = {
        .__stack = state->stack_args + p->named_stack_size,
        .__gr_top = state->gp + 8,
        .__vr_top = state->fp + 8,
        .__gr_offs = ((int)p->named_gp - 8) * 8,
        .__vr_offs = ((int)p->named_fp - 8) * 16,
    };
    memcpy(dest, &ap, sizeof ap);
}

static
void
nc_win64_va_start(NcCaptureWin64* state, const NcCallPlan* p, void* dest){
    for(unsigned i=p->named_gp; i < 4; i++)
        memcpy(state->stack_args - 32 + i*8, &state->gp[i], 8);
    void* cursor = p->named_gp < 4
        ? state->stack_args - 32 + p->named_gp * 8
        : state->stack_args + _nc_align_up(p->named_stack_size, 8);
    memcpy(dest, &cursor, sizeof cursor);
}

static
void
nc_va_start(NcCallState* state, const NcCallPlan* p, void* dest){
    switch(p->target){
        case CC_TARGET_X86_64_LINUX:
        case CC_TARGET_X86_64_MACOS:
            nc_sysv_va_start(&state->sysv, p, dest);
            break;
        case CC_TARGET_AARCH64_LINUX:
            nc_aapcs64_va_start(&state->arm, p, dest);
            break;
        case CC_TARGET_X86_64_WINDOWS_MSVC:
        case CC_TARGET_X86_64_WINDOWS_MINGW:
            nc_win64_va_start(&state->win, p, dest);
            break;
        case CC_TARGET_AARCH64_MACOS: {
            void* cursor = state->arm.stack_args + _nc_align_up(p->named_stack_size, 8);
            memcpy(dest, &cursor, sizeof cursor);
            break;
        }
        case CC_TARGET_TEST: {
            void* cursor = state->test.stack_args + _nc_align_up(p->named_stack_size, 8);
            memcpy(dest, &cursor, sizeof cursor);
            break;
        }
        case CC_TARGET_COUNT:
            unreachable();
    }
}

static
void
nc_va_copy_pieces(NcAbiType type, void* dest, const void* gp, const void* fp){
    const unsigned char* g = gp;
    const unsigned char* f = fp;
    for(unsigned i = 0; i < type.count; i++){
        NcAbiPiece piece = type.pieces[i];
        size_t offset = piece.value_offset, size = piece.size;
        const void* source;
        if(piece.bank == NC_ABI_FP){
            source = f;
            f += 16;
        }
        else {
            source = g;
            g += 8;
        }
        memcpy((char*)dest + offset, source, size);
    }
}

static
void
nc_va_arg(NcAbiType type, CcTarget target, void* ap, void* dest){
    if(!type.size || type.mode == NC_ABI_IGNORED) return;
    void** stack = ap;
    switch(target){
        case CC_TARGET_X86_64_LINUX:
        case CC_TARGET_X86_64_MACOS: {
            NcSysvVaListTag* va = ap;
            unsigned nf = _nc_abi_fp_count(type),
                     ng = _nc_abi_bank_count(type, NC_ABI_GP);
            if(type.mode != NC_ABI_STACK_VALUE && va->gp_offset + ng * 8 <= 48 && va->fp_offset + nf * 16 <= 176){
                nc_va_copy_pieces(type, dest, (char*)va->reg_save_area + va->gp_offset, (char*)va->reg_save_area + va->fp_offset);
                va->gp_offset += ng * 8;
                va->fp_offset += nf * 16;
                return;
            }
            stack = &va->overflow_arg_area;
            break;
        }
        case CC_TARGET_AARCH64_LINUX: {
            NcAapcs64VaList* va = ap;
            unsigned nf = _nc_abi_fp_count(type);
            unsigned ng = type.mode == NC_ABI_INDIRECT ? 1 : _nc_abi_bank_count(type, NC_ABI_GP);
            int go = va->__gr_offs, fo = va->__vr_offs;
            if(type.align_log2 == 4 && ng && type.mode != NC_ABI_INDIRECT) go = (go + 15) & ~15;
            if(type.mode != NC_ABI_STACK_VALUE && (!ng || (go < 0 && go + (int)ng * 8 <= 0)) &&
               (!nf || (fo < 0 && fo + (int)nf * 16 <= 0))){
                const void* gp = ng ? (char*)va->__gr_top + go : NULL;
                const void* fp = nf ? (char*)va->__vr_top + fo : NULL;
                if(type.mode == NC_ABI_INDIRECT){
                    const void* source;
                    memcpy(&source, gp, sizeof source);
                    memcpy(dest, source, type.size);
                }
                else
                    nc_va_copy_pieces(type, dest, gp, fp);
                if(ng) va->__gr_offs = go + (int)ng * 8;
                if(nf) va->__vr_offs = fo + (int)nf * 16;
                return;
            }
            if(ng) va->__gr_offs = 0;
            if(nf) va->__vr_offs = 0;
            stack = &va->__stack;
            break;
        }
        case CC_TARGET_AARCH64_MACOS:
        case CC_TARGET_X86_64_WINDOWS_MSVC:
        case CC_TARGET_X86_64_WINDOWS_MINGW:
        case CC_TARGET_TEST:
            stack = ap;
            break;
        case CC_TARGET_COUNT:
            unreachable();
    }
    _Bool indirect = type.mode == NC_ABI_INDIRECT;
    size_t alignment = indirect ? 8 : (size_t)1 << type.align_log2;
    size_t size = indirect ? 8 : type.size;
    if(alignment < 8) alignment = 8;
    uintptr_t cursor = _nc_align_up((uintptr_t)*stack, alignment);
    void* source = (void*)cursor;
    *stack = (void*)(cursor + _nc_align_up(size, 8));
    if(indirect) memcpy(&source, source, sizeof source);
    memcpy(dest, source, type.size);
}

#ifdef __clang__
#pragma clang assume_nonnull end
#endif
#endif
