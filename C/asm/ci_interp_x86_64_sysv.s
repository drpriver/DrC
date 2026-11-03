// Copyright © 2026-2026, David Priver <david@davidpriver.com>
// Requires x86-64-v3.
// Dispatch entries are signed offsets from r14, so the table needs no
// per-handler dynamic relocations.
// TODO: delete slop comments
#include "ci_interp_layout.h"
#include "../ci_op_list.h"

.set ci_op_index, 0
.irp opcode, CI_OPCODE_NATIVE_LIST(CI_OPCODE_ENUM) CI_NATIVE_OP_END
    .set \opcode, ci_op_index
    .set ci_op_index, ci_op_index + 1
.endr

#ifdef __APPLE__
#define CI_SYMBOL(name) _##name
#else
#define CI_SYMBOL(name) name
#endif
#define CI_SCRATCH_SIZE 48
#define CI_FRAME_OFFSET (CI_LOCAL_SIZE + 40)
#define CI_ARGS_OFFSET 0
#define CI_ARGS_SIZE 176

#define CI_GP_COUNT 6
#define CI_FP_BASE 48
#define CI_FP_COUNT 8

#define CI_LOCAL_SIZE (CI_SCRATCH_SIZE + 64 + 8)

.intel_syntax noprefix
.text

// r12: slots; r13: PC; r14: dispatch table; r15: CiStack.
// rbx: execution record; rbp: unwind anchor.
// ABI scratch: rax, r10, r11, xmm8.
//
// Scratch peak: 32 bytes of x87 arguments + 16-byte staging buffer.
// rbx follows scratch: ci, root, frame, helpers, result, size, target, flags.
// Eight trailing bytes align RSP after the six saved registers.
.macro CI_GP_APPLY register, action, operand, reverse
    .if \reverse
        \action \operand, \register
    .else
        \action \register, \operand
    .endif
.endm
.macro CI_ARG_GP reg, action, operand, reverse=0
    .if \reg == 0
        CI_GP_APPLY rdi, \action, \operand, \reverse
    .elseif \reg == 1
        CI_GP_APPLY rsi, \action, \operand, \reverse
    .elseif \reg == 2
        CI_GP_APPLY rdx, \action, \operand, \reverse
    .elseif \reg == 3
        CI_GP_APPLY rcx, \action, \operand, \reverse
    .elseif \reg == 4
        CI_GP_APPLY r8, \action, \operand, \reverse
    .elseif \reg == 5
        CI_GP_APPLY r9, \action, \operand, \reverse
    .endif
.endm
.macro CI_ARGS load, base, offset=0
    .if \load
        mov rdi, QWORD PTR [\base + \offset]
        mov rsi, QWORD PTR [\base + \offset + 8]
        mov rdx, QWORD PTR [\base + \offset + 16]
        mov rcx, QWORD PTR [\base + \offset + 24]
        mov r8, QWORD PTR [\base + \offset + 32]
        mov r9, QWORD PTR [\base + \offset + 40]
    .else
        mov QWORD PTR [\base + \offset], rdi
        mov QWORD PTR [\base + \offset + 8], rsi
        mov QWORD PTR [\base + \offset + 16], rdx
        mov QWORD PTR [\base + \offset + 24], rcx
        mov QWORD PTR [\base + \offset + 32], r8
        mov QWORD PTR [\base + \offset + 40], r9
    .endif
    .irp reg, 0, 1, 2, 3, 4, 5, 6, 7
        .if \reg < CI_FP_COUNT
            .if \load
                movdqu xmm\reg, XMMWORD PTR [\base + \offset + CI_FP_BASE + \reg*16]
            .else
                movdqu XMMWORD PTR [\base + \offset + CI_FP_BASE + \reg*16], xmm\reg
            .endif
        .endif
    .endr
.endm
.macro CI_ARGS_SAVE
    // 48 saved GP bytes + 128 saved FP bytes.
    sub rsp, CI_ARGS_SIZE
    CI_ARGS 0, rsp, CI_ARGS_OFFSET
.endm
.macro CI_ARGS_RESTORE
    CI_ARGS 1, rsp, CI_ARGS_OFFSET
    add rsp, CI_ARGS_SIZE
.endm
.macro CI_DISPATCH
    movzx eax, WORD PTR [r13]
    movsxd rax, DWORD PTR [r14 + rax*4]
    add rax, r14
    jmp rax
.endm
.macro CI_NEXT
    add r13, 32
    CI_DISPATCH
.endm
.macro CI_LABEL name
    .set Lci_handler_\name, Lci_\name
Lci_\name:
    endbr64
.endm
.macro CI_SLOT reg, reg32, offset
    mov \reg32, DWORD PTR [r13 + \offset]
    add \reg, r12
.endm
.macro CI_READ bytes, sign=0, base=rax
    .if \bytes == 1
        .if \sign
            movsx rax, BYTE PTR [\base]
        .else
            movzx eax, BYTE PTR [\base]
        .endif
    .elseif \bytes == 2
        .if \sign
            movsx rax, WORD PTR [\base]
        .else
            movzx eax, WORD PTR [\base]
        .endif
    .elseif \bytes == 4
        .if \sign
            movsxd rax, DWORD PTR [\base]
        .else
            mov eax, DWORD PTR [\base]
        .endif
    .else
        mov rax, QWORD PTR [\base]
    .endif
.endm
.macro CI_LOAD_SLOT offset, bytes, sign=0
    mov eax, DWORD PTR [r13 + \offset]
    CI_READ \bytes, \sign, r12+rax
.endm
.macro CI_STORE bytes, base=r10
    .if \bytes == 1
        mov BYTE PTR [\base], al
    .elseif \bytes == 2
        mov WORD PTR [\base], ax
    .elseif \bytes == 4
        mov DWORD PTR [\base], eax
    .else
        mov QWORD PTR [\base], rax
    .endif
.endm
.macro CI_HELPER name
    mov r11, QWORD PTR [rbx + 24]
    call QWORD PTR [r11 + CI_ASM_HELPER_\name*8]
.endm
.macro CI_PUBLISH_PC
    mov rax, QWORD PTR [r12 + CI_ASM_code - CI_ASM_frame_size]
    lea rax, [rax + CI_ASM_code_ops]
    mov r10, r13
    sub r10, rax
    shr r10, 5
    mov QWORD PTR [r12 + CI_ASM_pc - CI_ASM_frame_size], r10
.endm
.macro CI_STACK_RESTORE boundary
    .if \boundary
        mov rcx, QWORD PTR [r15 + CI_ASM_stack_boundary]
    .else
        mov rcx, QWORD PTR [r15 + CI_ASM_stack_mark_count]
    .endif
    dec rcx
    mov QWORD PTR [r15 + CI_ASM_stack_mark_count], rcx
    lea rcx, [rcx + rcx*2]
    mov r10, QWORD PTR [r15 + CI_ASM_stack_marks]
    lea r10, [r10 + rcx*8]
    movdqu xmm0, XMMWORD PTR [r10]
    movdqu XMMWORD PTR [r15 + CI_ASM_stack_top], xmm0
    mov rcx, QWORD PTR [r10 + 16]
    mov QWORD PTR [r15 + CI_ASM_stack_boundary], rcx
.endm

.p2align 4
.globl CI_SYMBOL(ci_interp_execute_asm)
#ifdef __linux__
.hidden ci_interp_execute_asm
.type ci_interp_execute_asm, @function
#endif
CI_SYMBOL(ci_interp_execute_asm):
.cfi_startproc
    endbr64
    push rbp
    .cfi_def_cfa_offset 16
    .cfi_offset rbp, -16
    push rbx
    .cfi_def_cfa_offset 24
    .cfi_offset rbx, -24
    push r12
    .cfi_def_cfa_offset 32
    .cfi_offset r12, -32
    push r13
    .cfi_def_cfa_offset 40
    .cfi_offset r13, -40
    push r14
    .cfi_def_cfa_offset 48
    .cfi_offset r14, -48
    push r15
    .cfi_def_cfa_offset 56
    .cfi_offset r15, -56
    sub rsp, CI_LOCAL_SIZE
    .cfi_def_cfa_offset (CI_LOCAL_SIZE + 56)
    lea rbp, [rsp + CI_FRAME_OFFSET]
    .cfi_def_cfa rbp, 16
    lea rbx, [rsp + CI_SCRATCH_SIZE]
    mov QWORD PTR [rbx], rdi
    mov QWORD PTR [rbx + 8], rsi
    mov QWORD PTR [rbx + 16], rsi
    mov QWORD PTR [rbx + 24], rdx
    mov QWORD PTR [rbx + 32], r8
    mov QWORD PTR [rbx + 40], r9
    mov r15, rcx
    lea r14, [rip + Lci_dispatch_table]
    mov rax, QWORD PTR [rbp + 16]
    mov QWORD PTR [rbx + 56], rax
    CI_ARGS 1, r15
Lci_load_frame:
    mov r12, QWORD PTR [rbx + 16]
    mov rax, QWORD PTR [r12 + CI_ASM_code]
    mov r13, QWORD PTR [r12 + CI_ASM_pc]
    cmp r13, QWORD PTR [rax + CI_ASM_code_count]
    jae Lci_complete
    shl r13, 5
    lea r13, [rax + r13 + CI_ASM_code_ops]
    add r12, CI_ASM_frame_size
    CI_DISPATCH
Lci_complete:
    xor eax, eax
Lci_exit:
    .cfi_remember_state
    lea rsp, [rbp - CI_FRAME_OFFSET + CI_LOCAL_SIZE]
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    pop rbp
    .cfi_def_cfa rsp, 8
    .cfi_restore rbp
    .cfi_restore rbx
    .cfi_restore r12
    .cfi_restore r13
    .cfi_restore r14
    .cfi_restore r15
    ret
    .cfi_restore_state

Lci_error:
    mov r9d, eax
    mov rdx, QWORD PTR [rbx + 16]
1:
    cmp rdx, QWORD PTR [rbx + 8]
    je 2f
    mov rdx, QWORD PTR [rdx + CI_ASM_parent]
    CI_STACK_RESTORE 1
    jmp 1b
2:
    mov rax, QWORD PTR [rdx + CI_ASM_code]
    cmp WORD PTR [rax + CI_ASM_code_ops], CI_OP_FRAME_SAVE
    jne 3f
    CI_STACK_RESTORE 1
3:
    mov eax, r9d
    jmp Lci_exit

CI_LABEL RETURN
CI_LABEL FRAME_RESTORE
    mov rdx, QWORD PTR [rbx + 16]
    mov rax, QWORD PTR [rdx + CI_ASM_code]
    mov rax, QWORD PTR [rax + CI_ASM_code_count]
    mov QWORD PTR [rdx + CI_ASM_pc], rax
    cmp rdx, QWORD PTR [rbx + 8]
    je 1f
    mov rdx, QWORD PTR [rdx + CI_ASM_parent]
    mov QWORD PTR [rbx + 16], rdx
    inc QWORD PTR [rdx + CI_ASM_pc]
    CI_STACK_RESTORE 1
    jmp Lci_load_frame
1:
    mov rax, QWORD PTR [rdx + CI_ASM_code]
    cmp WORD PTR [rax + CI_ASM_code_ops], CI_OP_FRAME_SAVE
    jne Lci_complete
    CI_STACK_RESTORE 1
    jmp Lci_complete

CI_LABEL STACK_RESTORE
    CI_STACK_RESTORE 0
    CI_NEXT
.macro CI_STACK_SAVE name, boundary
    CI_LABEL \name
    .if !\boundary
        CI_PUBLISH_PC
    .endif
    mov rax, QWORD PTR [r15 + CI_ASM_stack_mark_count]
    cmp rax, QWORD PTR [r15 + CI_ASM_stack_mark_capacity]
    jae Lci_save_grow_\@
    lea r11, [rax + rax*2]
    mov r10, QWORD PTR [r15 + CI_ASM_stack_marks]
    lea r10, [r10 + r11*8]
    movdqu xmm8, XMMWORD PTR [r15 + CI_ASM_stack_top]
    movdqu XMMWORD PTR [r10], xmm8
    mov r11, QWORD PTR [r15 + CI_ASM_stack_boundary]
    mov QWORD PTR [r10 + 16], r11
    inc rax
    mov QWORD PTR [r15 + CI_ASM_stack_mark_count], rax
    .if \boundary
        mov QWORD PTR [r15 + CI_ASM_stack_boundary], rax
    .endif
    CI_NEXT
Lci_save_grow_\@:
    CI_ARGS_SAVE
    mov rdi, r15
    mov esi, \boundary
    CI_HELPER stack_save
    CI_ARGS_RESTORE
    test eax, eax
    jne Lci_error
    CI_NEXT
.endm
CI_STACK_SAVE FRAME_SAVE, 1
CI_STACK_SAVE STACK_SAVE, 0
CI_LABEL FRAME_ENTER
    lea rax, [r13 - CI_ASM_code_ops - 32]
    mov QWORD PTR [rsp], rax
    mov r10, QWORD PTR [r15 + CI_ASM_stack_top]
    test r10, r10
    jz Lci_frame_grow
    mov r11, QWORD PTR [r15 + CI_ASM_stack_top + 8]
    lea r11, [r10 + r11 + 16 + CI_ASM_frame_size]
    mov eax, DWORD PTR [rax + 16]
    dec rax
    add r11, rax
    not rax
    and r11, rax
    mov QWORD PTR [rsp + 8], r11
    mov rax, QWORD PTR [rsp]
    add r11, QWORD PTR [rax + 8]
    sub r11, r10
    sub r11, 16
    cmp r11, QWORD PTR [r10 + 8]
    ja Lci_frame_grow
    mov QWORD PTR [r15 + CI_ASM_stack_top + 8], r11
    mov r12, QWORD PTR [rsp + 8]
    sub r12, CI_ASM_frame_size
    mov rax, QWORD PTR [rbx + 16]
    mov QWORD PTR [r12 + CI_ASM_parent], rax
    mov QWORD PTR [rbx + 16], r12
    test BYTE PTR [rbx + 56], 2
    jz Lci_frame_init
    mov rax, QWORD PTR [rsp]
    mov rax, QWORD PTR [rax + 8]
    lea r10, [r12 + CI_ASM_frame_size]
Lci_frame_poison:
    test rax, rax
    jz Lci_frame_init
    dec rax
    mov BYTE PTR [r10 + rax], 0xcc
    jmp Lci_frame_poison
Lci_frame_grow:
    CI_ARGS_SAVE
    lea r10, [r13 - CI_ASM_code_ops - 32]
    mov rdi, QWORD PTR [rbx]
    mov rsi, r15
    mov rdx, QWORD PTR [rbx + 16]
    mov rcx, QWORD PTR [r10 + 8]
    mov r8d, DWORD PTR [r10 + 16]
    lea r9, [rbx + 16]
    CI_HELPER new_frame
    CI_ARGS_RESTORE
    test eax, eax
    jz Lci_frame_allocated
    CI_STACK_RESTORE 1
    jmp Lci_error
Lci_frame_allocated:
    mov r12, QWORD PTR [rbx + 16]
Lci_frame_init:
    lea rax, [r13 - CI_ASM_code_ops - 32]
    mov QWORD PTR [r12 + CI_ASM_code], rax
    mov QWORD PTR [r12 + CI_ASM_pc], 2
    mov eax, DWORD PTR [r13 + 4]
    cmp eax, -1
    je 1f
    lea rax, [r12 + rax + CI_ASM_frame_size]
    jmp 2f
1:
    xor eax, eax
2:
    mov QWORD PTR [r12 + CI_ASM_incoming], rax
    add r12, CI_ASM_frame_size
    CI_NEXT
CI_LABEL FRAME_READY
    CI_NEXT

.macro CI_CALL name, indirect
    CI_LABEL \name
    CI_PUBLISH_PC
    .if \indirect
        mov eax, DWORD PTR [r13 + 12]
        mov r11, QWORD PTR [r12 + rax - 8]
        mov r10, QWORD PTR [r12 + rax - 16]
    .else
        mov rax, QWORD PTR [r13 + 16]
        mov r11, QWORD PTR [rax + 16]
        mov r10, QWORD PTR [r11 + CI_ASM_native_func]
    .endif
    test r11, r11
    je Lci_native_call
    test BYTE PTR [r11 + CI_ASM_func_flags], 64
    je Lci_native_call
    mov r10d, 33
1:
    mov rax, QWORD PTR [r11 + CI_ASM_func_hotswap]
    test rax, rax
    jz 2f
    mov r11, rax
    dec r10d
    jnz 1b
    lea r8, [rip + Lci_msg_hotswap]
    jmp Lci_call_error
2:
    mov rax, QWORD PTR [r11 + CI_ASM_func_code]
    mov r13, QWORD PTR [rax + 8]
    add r13, CI_ASM_code_ops
    CI_DISPATCH
.endm
CI_CALL CALL_DIRECT, 0
CI_CALL CALL_INDIRECT, 1
Lci_native_call:
    test r10, r10
    jz Lci_null_call
    mov QWORD PTR [rbx + 48], r10
    mov rax, QWORD PTR [r13 + 16]
    mov rax, QWORD PTR [rax]

    mov r10, QWORD PTR [rax + CI_ASM_plan_stack_size]
    mov QWORD PTR [rsp + 8], r10
    // Count x87 return pieces using scratch registers only. The plan is
    // private to this call, so nested callbacks cannot change the pop count.
    mov r10d, DWORD PTR [rax + 16]
    shr r10d, 7
    and r10d, 7
    mov QWORD PTR [rsp], 0
1:
    test r10d, r10d
    jz 2f
    dec r10d
    movzx r11d, WORD PTR [rax + r10*2 + 20]
    and r11d, 0x1800
    cmp r11d, 0x1000
    sete r11b
    movzx r11d, r11b
    add QWORD PTR [rsp], r11
    jmp 1b
2:
    mov r11, QWORD PTR [rax + CI_ASM_plan_stack_alignment]
    mov r10, rsp
    sub r10, QWORD PTR [rsp + 8]
    neg r11
    and r10, r11
    and r10, -16
3:
    mov rax, rsp
    sub rax, r10
    cmp rax, 4096
    jb 4f
    sub rsp, 4096
    test BYTE PTR [rsp], 0
    jmp 3b
4:
    mov rsp, r10
    mov rax, QWORD PTR [rbp - CI_FRAME_OFFSET + 8]
    mov r10, QWORD PTR [r15 + 184]
    mov r11, rsp
    call Lci_abi_copy
    mov rax, QWORD PTR [r15 + 176]
    call QWORD PTR [rbx + 48]
    mov QWORD PTR [r15 + 192], rax
    mov QWORD PTR [r15 + 200], rdx
    movdqu XMMWORD PTR [r15 + 208], xmm0
    movdqu XMMWORD PTR [r15 + 224], xmm1
    mov rax, QWORD PTR [rbp - CI_FRAME_OFFSET]
    mov QWORD PTR [r15 + 272], rax
    test eax, eax
    jz 5f
    fstp TBYTE PTR [r15 + 240]
    cmp eax, 2
    jne 5f
    fstp TBYTE PTR [r15 + 256]
5:
    lea rsp, [rbp - CI_FRAME_OFFSET]
    CI_NEXT
Lci_null_call:
    lea r8, [rip + Lci_msg_null_call]
Lci_call_error:
    mov rdi, QWORD PTR [rbx]
    mov rsi, QWORD PTR [r13 + 24]
    mov rdx, r8
    xor eax, eax
    CI_HELPER error
    jmp Lci_error

.macro CI_ALU_VALUE bytes, operation, base, index, offset=0
    .ifc \operation, mul
        .if \bytes == 1
            mul BYTE PTR [\base + \index + \offset]
        .elseif \bytes == 2
            imul ax, WORD PTR [\base + \index + \offset]
        .elseif \bytes == 4
            imul eax, DWORD PTR [\base + \index + \offset]
        .else
            imul rax, QWORD PTR [\base + \index + \offset]
        .endif
    .else
        .if \bytes == 1
            \operation al, BYTE PTR [\base + \index + \offset]
        .elseif \bytes == 2
            \operation ax, WORD PTR [\base + \index + \offset]
        .elseif \bytes == 4
            \operation eax, DWORD PTR [\base + \index + \offset]
        .else
            \operation rax, QWORD PTR [\base + \index + \offset]
        .endif
    .endif
.endm
.macro CI_ALU name, bytes, operation, immediate=0
    CI_LABEL \name
    mov r10d, DWORD PTR [r13 + 4]
    CI_LOAD_SLOT 8, \bytes
    .if \immediate
        CI_ALU_VALUE \bytes, \operation, r13, 0, 16
    .else
        mov ecx, DWORD PTR [r13 + 12]
        CI_ALU_VALUE \bytes, \operation, r12, rcx
    .endif
    CI_STORE \bytes, r12+r10
    CI_NEXT
.endm
.irp bits, 8, 16, 32, 64
    CI_ALU ADD\bits, (\bits/8), add
    CI_ALU SUB\bits, (\bits/8), sub
    CI_ALU MUL\bits, (\bits/8), mul
    CI_ALU AND\bits, (\bits/8), and
    CI_ALU OR\bits, (\bits/8), or
    CI_ALU XOR\bits, (\bits/8), xor
.endr
.irp bits, 32, 64
    CI_ALU ADD_IMM\bits, (\bits/8), add, 1
    CI_ALU SUB_IMM\bits, (\bits/8), sub, 1
    CI_ALU MUL_IMM\bits, (\bits/8), mul, 1
    CI_ALU AND_IMM\bits, (\bits/8), and, 1
    CI_ALU OR_IMM\bits, (\bits/8), or, 1
    CI_ALU XOR_IMM\bits, (\bits/8), xor, 1
.endr

.macro CI_UNARY name, bytes, operation
    CI_LABEL \name
    mov r10d, DWORD PTR [r13 + 4]
    CI_LOAD_SLOT 8, \bytes
    \operation rax
    CI_STORE \bytes, r12+r10
    CI_NEXT
.endm
.irp bits, 8, 16, 32, 64
    CI_UNARY NEG\bits, (\bits/8), neg
    CI_UNARY NOT\bits, (\bits/8), not
.endr

.macro CI_COMPARE name, bits, condition, branch=0
    CI_LABEL \name
    mov eax, DWORD PTR [r13 + 12]
    mov ecx, DWORD PTR [r13 + 16]
    .if \bits == 32
        mov eax, DWORD PTR [r12 + rax]
        cmp eax, DWORD PTR [r12 + rcx]
    .else
        mov rax, QWORD PTR [r12 + rax]
        cmp rax, QWORD PTR [r12 + rcx]
    .endif
    set\condition al
    .if \branch
        xor al, BYTE PTR [r13 + 2]
        test al, 1
        jne 1f
        movsxd rax, DWORD PTR [r13 + 4]
        shl rax, 5
        add r13, rax
        CI_DISPATCH
1:
        CI_NEXT
    .else
        mov ecx, DWORD PTR [r13 + 4]
        mov BYTE PTR [r12 + rcx], al
        CI_NEXT
    .endif
.endm
.irp bits, 32, 64
    CI_COMPARE EQ\bits, \bits, e
    CI_COMPARE NE\bits, \bits, ne
    CI_COMPARE SLT\bits, \bits, l
    CI_COMPARE ULT\bits, \bits, b
    CI_COMPARE SLE\bits, \bits, le
    CI_COMPARE ULE\bits, \bits, be
    CI_COMPARE SGT\bits, \bits, g
    CI_COMPARE UGT\bits, \bits, a
    CI_COMPARE SGE\bits, \bits, ge
    CI_COMPARE UGE\bits, \bits, ae
    CI_COMPARE EQ_JUMP\bits, \bits, e, 1
    CI_COMPARE NE_JUMP\bits, \bits, ne, 1
    CI_COMPARE SLT_JUMP\bits, \bits, l, 1
    CI_COMPARE ULT_JUMP\bits, \bits, b, 1
    CI_COMPARE SLE_JUMP\bits, \bits, le, 1
    CI_COMPARE ULE_JUMP\bits, \bits, be, 1
    CI_COMPARE SGT_JUMP\bits, \bits, g, 1
    CI_COMPARE UGT_JUMP\bits, \bits, a, 1
    CI_COMPARE SGE_JUMP\bits, \bits, ge, 1
    CI_COMPARE UGE_JUMP\bits, \bits, ae, 1
.endr

.macro CI_SHIFT name, bytes, operation, sign=0, immediate=0
    CI_LABEL \name
    mov r10d, DWORD PTR [r13 + 4]
    .if \immediate
        .if \bytes == 4
            mov ecx, DWORD PTR [r13 + 16]
        .else
            mov rcx, QWORD PTR [r13 + 16]
        .endif
    .else
        mov ecx, DWORD PTR [r13 + 12]
        mov ecx, DWORD PTR [r12 + rcx]
    .endif
    CI_LOAD_SLOT 8, \bytes, \sign
    \operation rax, cl
    CI_STORE \bytes, r12+r10
    CI_NEXT
.endm
.macro CI_DIV name, bytes, sign, remainder, immediate=0
    CI_LABEL \name
    mov r10d, DWORD PTR [r13 + 4]
    .if \immediate
        CI_READ \bytes, \sign, r13+16
    .else
        CI_LOAD_SLOT 12, \bytes, \sign
    .endif
    mov rcx, rax
    CI_LOAD_SLOT 8, \bytes, \sign
    // Avoid x86 #DE for both zero divisors and INT64_MIN / -1.
    test rcx, rcx
    jz 3f
    .if \sign
        .if \bytes == 8
            cmp rcx, -1
            jne 1f
            .if \remainder
                xor eax, eax
            .else
                neg rax
            .endif
            jmp 2f
1:
        .endif
        .if \bytes < 4
            cdq
            idiv ecx
        .else
            cqo
            idiv rcx
        .endif
    .else
        xor edx, edx
        .if \bytes < 8
            div ecx
        .else
            div rcx
        .endif
    .endif
    .if \remainder
        mov rax, rdx
    .endif
    jmp 2f
3:
    xor eax, eax
2:
    CI_STORE \bytes, r12+r10
    CI_NEXT
.endm
.irp bits, 8, 16, 32, 64
    CI_SHIFT SHL\bits, (\bits/8), shl
    CI_SHIFT ASR\bits, (\bits/8), sar, 1
    CI_SHIFT LSR\bits, (\bits/8), shr
    CI_DIV SDIV\bits, (\bits/8), 1, 0
    CI_DIV UDIV\bits, (\bits/8), 0, 0
    CI_DIV SMOD\bits, (\bits/8), 1, 1
    CI_DIV UMOD\bits, (\bits/8), 0, 1
.endr
.irp bits, 32, 64
    CI_SHIFT SHL_IMM\bits, (\bits/8), shl, 0, 1
    CI_SHIFT ASR_IMM\bits, (\bits/8), sar, 1, 1
    CI_SHIFT LSR_IMM\bits, (\bits/8), shr, 0, 1
    CI_DIV SDIV_IMM\bits, (\bits/8), 1, 0, 1
    CI_DIV UDIV_IMM\bits, (\bits/8), 0, 0, 1
    CI_DIV SMOD_IMM\bits, (\bits/8), 1, 1, 1
    CI_DIV UMOD_IMM\bits, (\bits/8), 0, 1, 1
.endr

.macro CI_MEMORY_READ bytes, base
    .if \bytes == 16
        movdqu xmm0, XMMWORD PTR [\base]
    .else
        CI_READ \bytes, 0, \base
    .endif
.endm
.macro CI_MEMORY_WRITE bytes, base
    .if \bytes == 16
        movdqu XMMWORD PTR [\base], xmm0
    .else
        CI_STORE \bytes, \base
    .endif
.endm
.macro CI_MEMORY_FIXED mode, bytes
    .if \mode == 0
        CI_MEMORY_READ \bytes, r13+8
    .elseif \mode == 1
        mov eax, DWORD PTR [r13 + 16]
        CI_MEMORY_READ \bytes, r12+rax
    .elseif \mode == 2 || \mode == 5
        CI_LOAD_SLOT 16, 8
        mov ecx, DWORD PTR [r13 + 20]
        CI_MEMORY_READ \bytes, rax+rcx
    .elseif \mode == 3
        mov eax, DWORD PTR [r13 + 12]
        CI_MEMORY_READ \bytes, r12+rax
    .elseif \mode == 4
        CI_MEMORY_READ \bytes, r13+16
    .else
        .if \bytes == 16
            pxor xmm0, xmm0
        .else
            xor eax, eax
        .endif
    .endif
    .if \mode <= 2
        .if \mode == 0
            mov r10d, DWORD PTR [r13 + 4]
        .else
            mov r10d, DWORD PTR [r13 + 8]
        .endif
        CI_MEMORY_WRITE \bytes, r12+r10
    .else
        .if \mode == 4
            mov r10d, DWORD PTR [r13 + 4]
            mov ecx, DWORD PTR [r13 + 8]
        .else
            mov r10d, DWORD PTR [r13 + 8]
            .if \mode == 3
                mov ecx, DWORD PTR [r13 + 20]
            .else
                mov ecx, DWORD PTR [r13 + 12]
            .endif
        .endif
        mov r10, QWORD PTR [r12 + r10]
        CI_MEMORY_WRITE \bytes, r10+rcx
    .endif
.endm
.macro CI_MEMORY name, mode, bytes
    CI_LABEL \name
    .if \bytes > 0 && \bytes <= 16
        CI_MEMORY_FIXED \mode, \bytes
        CI_NEXT
        .exitm
    .endif
    .if \mode == 0
        CI_SLOT r10, r10d, 4
        lea rax, [r13 + 8]
    .elseif \mode == 1 || \mode == 2
        CI_SLOT r10, r10d, 8
        CI_SLOT rax, eax, 16
        .if \mode == 2
            mov rax, QWORD PTR [rax]
            mov ecx, DWORD PTR [r13 + 20]
            add rax, rcx
        .endif
    .elseif \mode == 3
        mov r10d, DWORD PTR [r13 + 8]
        mov r10, QWORD PTR [r12 + r10]
        mov ecx, DWORD PTR [r13 + 20]
        add r10, rcx
        CI_SLOT rax, eax, 12
    .elseif \mode == 4
        mov r10d, DWORD PTR [r13 + 4]
        mov r10, QWORD PTR [r12 + r10]
        mov ecx, DWORD PTR [r13 + 8]
        add r10, rcx
        lea rax, [r13 + 16]
    .elseif \mode == 5 || \mode == 6
        mov r10d, DWORD PTR [r13 + 8]
        mov r10, QWORD PTR [r12 + r10]
        mov ecx, DWORD PTR [r13 + 12]
        add r10, rcx
        .if \mode == 5
            CI_LOAD_SLOT 16, 8
            mov ecx, DWORD PTR [r13 + 20]
            add rax, rcx
        .endif
    .endif
    .if \mode == 0
        movzx edx, BYTE PTR [r13 + 3]
    .elseif \mode == 1 || \mode == 2
        mov edx, DWORD PTR [r13 + 12]
    .elseif \mode == 3
        mov edx, DWORD PTR [r13 + 16]
    .elseif \mode == 4
        movzx edx, BYTE PTR [r13 + 2]
    .else
        mov edx, DWORD PTR [r13 + 4]
        .if \bytes == -1
            mov rdx, QWORD PTR [r12 + rdx]
        .endif
    .endif
    mov r11, QWORD PTR [rbx + 24]
    mov rdi, r10
    .if \mode == 6
        xor esi, esi
    .else
        mov rsi, rax
    .endif
    .if \mode == 6
        call QWORD PTR [r11 + CI_ASM_HELPER_memset*8]
    .else
        call QWORD PTR [r11 + CI_ASM_HELPER_memmove*8]
    .endif
    CI_NEXT
.endm
#define CI_CONST(name, bytes, ...) CI_MEMORY name, 0, bytes ;
CI_CONST_OPS(CI_CONST)
#undef CI_CONST
#define CI_COPY(name, bytes, ...) CI_MEMORY name, 1, bytes ;
CI_COPY_OPS(CI_COPY)
#undef CI_COPY
#define CI_LOAD(name, bytes, ...) CI_MEMORY name, 2, bytes ;
CI_LOAD_OPS(CI_LOAD)
#undef CI_LOAD
#define CI_STORE_OP(name, bytes, ...) CI_MEMORY name, 3, bytes ;
CI_STORE_OPS(CI_STORE_OP)
#undef CI_STORE_OP
#define CI_STORE_IMM(name, bytes, ...) CI_MEMORY name, 4, bytes ;
CI_STORE_IMM_OPS(CI_STORE_IMM)
#undef CI_STORE_IMM
#define CI_MEMCOPY(name, bytes, ...) CI_MEMORY name, 5, bytes ;
CI_MEMCOPY_OPS(CI_MEMCOPY)
#undef CI_MEMCOPY
#define CI_ZERO(name, bytes, ...) CI_MEMORY name, 6, bytes ;
CI_ZERO_OPS(CI_ZERO)
#undef CI_ZERO

.macro CI_EXTEND name, from, to, sign
    CI_LABEL \name
    mov r10d, DWORD PTR [r13 + 8]
    CI_LOAD_SLOT 16, \from, \sign
    CI_STORE \to, r12+r10
    .if \to == 16
        .if \sign
            sar rax, 63
        .else
            xor eax, eax
        .endif
        mov QWORD PTR [r12 + r10 + 8], rax
    .endif
    CI_NEXT
.endm
#define CI_EXTEND_OP(name, from, to, sign) CI_EXTEND name, from, to, sign ;
CI_EXTEND_OPS(CI_EXTEND_OP)
#undef CI_EXTEND_OP

.macro CI_FLOAT name, suffix, operation
    CI_LABEL \name
    mov eax, DWORD PTR [r13 + 12]
    mov ecx, DWORD PTR [r13 + 16]
    mov\suffix xmm0, [r12 + rax]
    \operation\suffix xmm0, [r12 + rcx]
    mov eax, DWORD PTR [r13 + 4]
    mov\suffix [r12 + rax], xmm0
    CI_NEXT
.endm
CI_FLOAT FADD32, ss, add
CI_FLOAT FSUB32, ss, sub
CI_FLOAT FMUL32, ss, mul
CI_FLOAT FDIV32, ss, div
CI_FLOAT FADD64, sd, add
CI_FLOAT FSUB64, sd, sub
CI_FLOAT FMUL64, sd, mul
CI_FLOAT FDIV64, sd, div

.macro CI_FLOAT_COMPARE name, suffix, condition, unordered, reverse=0
    CI_LABEL \name
    mov eax, DWORD PTR [r13 + 12]
    mov ecx, DWORD PTR [r13 + 16]
    .if \reverse
        mov\suffix xmm0, [r12 + rcx]
        ucomi\suffix xmm0, [r12 + rax]
    .else
        mov\suffix xmm0, [r12 + rax]
        ucomi\suffix xmm0, [r12 + rcx]
    .endif
    set\condition al
    .if \unordered
        setp cl
        or al, cl
    .else
        .ifc \condition, e
            setnp cl
            and al, cl
        .endif
    .endif
    mov ecx, DWORD PTR [r13 + 4]
    mov BYTE PTR [r12 + rcx], al
    CI_NEXT
.endm
CI_FLOAT_COMPARE FEQ32, ss, e, 0
CI_FLOAT_COMPARE FNE32, ss, ne, 1
CI_FLOAT_COMPARE FLT32, ss, a, 0, 1
CI_FLOAT_COMPARE FLE32, ss, ae, 0, 1
CI_FLOAT_COMPARE FGT32, ss, a, 0
CI_FLOAT_COMPARE FGE32, ss, ae, 0
CI_FLOAT_COMPARE FEQ64, sd, e, 0
CI_FLOAT_COMPARE FNE64, sd, ne, 1
CI_FLOAT_COMPARE FLT64, sd, a, 0, 1
CI_FLOAT_COMPARE FLE64, sd, ae, 0, 1
CI_FLOAT_COMPARE FGT64, sd, a, 0
CI_FLOAT_COMPARE FGE64, sd, ae, 0

.macro CI_CHECKED name, bits, operand, other, uns, operation
    CI_LABEL \name
    CI_LOAD_SLOT 12, (\bits/8)
    mov ecx, DWORD PTR [r13 + 16]
    .ifc \operation, mul
        .if \uns
            .if \bits == 8
                mul BYTE PTR [r12 + rcx]
            .elseif \bits == 16
                mul WORD PTR [r12 + rcx]
            .elseif \bits == 32
                mul DWORD PTR [r12 + rcx]
            .else
                mul QWORD PTR [r12 + rcx]
            .endif
        .elseif \bits == 8
            imul BYTE PTR [r12 + rcx]
        .else
            CI_ALU_VALUE (\bits/8), mul, r12, rcx
        .endif
    .else
        CI_ALU_VALUE (\bits/8), \operation, r12, rcx
    .endif
    .if \uns
        setc r9b
    .else
        seto r9b
    .endif
    mov r10d, DWORD PTR [r13 + 4]
    CI_STORE (\bits/8), r12+r10
    mov eax, DWORD PTR [r13 + 8]
    mov BYTE PTR [r12 + rax], r9b
    CI_NEXT
.endm
.macro CI_CHECKED_WIDTH bits, operand, other
    CI_CHECKED CHECKED_ADD_S\bits, \bits, \operand, \other, 0, add
    CI_CHECKED CHECKED_ADD_U\bits, \bits, \operand, \other, 1, add
    CI_CHECKED CHECKED_SUB_S\bits, \bits, \operand, \other, 0, sub
    CI_CHECKED CHECKED_SUB_U\bits, \bits, \operand, \other, 1, sub
    CI_CHECKED CHECKED_MUL_S\bits, \bits, \operand, \other, 0, mul
    CI_CHECKED CHECKED_MUL_U\bits, \bits, \operand, \other, 1, mul
.endm
CI_CHECKED_WIDTH 8, al, cl
CI_CHECKED_WIDTH 16, ax, cx
CI_CHECKED_WIDTH 32, eax, ecx
CI_CHECKED_WIDTH 64, rax, rcx

.macro CI_OVERFLOW name, bits, uns
    CI_LABEL \name
    mov eax, DWORD PTR [r13 + 12]
    .if \uns && \bits == 64
        cmp QWORD PTR [r12 + rax + 8], 0
    .else
        mov rcx, QWORD PTR [r12 + rax + 8]
        mov rax, QWORD PTR [r12 + rax]
    .endif
    .if \uns
        .if \bits < 64
            shr rax, \bits
            or rcx, rax
        .endif
        setne al
    .else
        .if \bits == 8
            movsx rdx, al
        .elseif \bits == 16
            movsx rdx, ax
        .elseif \bits == 32
            movsxd rdx, eax
        .else
            mov rdx, rax
        .endif
        .if \bits < 64
            cmp rdx, rax
            setne r9b
        .endif
        sar rdx, 63
        cmp rcx, rdx
        setne al
        .if \bits < 64
            or al, r9b
        .endif
    .endif
    mov ecx, DWORD PTR [r13 + 8]
    mov BYTE PTR [r12 + rcx], al
    CI_NEXT
.endm
#define CI_OVERFLOW_OP(name, bits, uns) CI_OVERFLOW name, bits, uns ;
CI_OVERFLOW_OPS(CI_OVERFLOW_OP)
#undef CI_OVERFLOW_OP

.macro CI_WIDE name, operation, carry
    CI_LABEL \name
    mov eax, DWORD PTR [r13 + 8]
    mov rdx, QWORD PTR [r12 + rax + 8]
    mov rax, QWORD PTR [r12 + rax]
    mov r10d, DWORD PTR [r13 + 12]
    \operation rax, QWORD PTR [r12 + r10]
    \carry rdx, QWORD PTR [r12 + r10 + 8]
    mov r10d, DWORD PTR [r13 + 4]
    mov QWORD PTR [r12 + r10], rax
    mov QWORD PTR [r12 + r10 + 8], rdx
    CI_NEXT
.endm
CI_WIDE ADD128, add, adc
CI_WIDE SUB128, sub, sbb
.macro CI_WIDE_BIT name, operation
    CI_LABEL \name
    mov eax, DWORD PTR [r13 + 8]
    mov ecx, DWORD PTR [r13 + 12]
    vmovdqu xmm8, XMMWORD PTR [r12 + rax]
    \operation xmm8, xmm8, XMMWORD PTR [r12 + rcx]
    mov eax, DWORD PTR [r13 + 4]
    vmovdqu XMMWORD PTR [r12 + rax], xmm8
    CI_NEXT
.endm
CI_WIDE_BIT AND128, vpand
CI_WIDE_BIT OR128, vpor
CI_WIDE_BIT XOR128, vpxor
CI_LABEL UMUL32_64
    CI_SLOT r10, r10d, 8
    CI_SLOT r11, r11d, 12
    mov eax, DWORD PTR [r10]
    mov ecx, DWORD PTR [r11]
    imul rax, rcx
    CI_SLOT r10, r10d, 4
    mov QWORD PTR [r10], rax
    CI_NEXT
CI_LABEL UMUL64_128
    CI_SLOT r10, r10d, 8
    CI_SLOT r11, r11d, 12
    mov rax, QWORD PTR [r10]
    mul QWORD PTR [r11]
    CI_SLOT r10, r10d, 4
    mov QWORD PTR [r10], rax
    mov QWORD PTR [r10 + 8], rdx
    CI_NEXT
CI_LABEL MUL128
    CI_SLOT r10, r10d, 8
    CI_SLOT r11, r11d, 12
    mov rax, QWORD PTR [r10]
    mov rcx, QWORD PTR [r10 + 8]
    imul rcx, QWORD PTR [r11]
    mov r9, QWORD PTR [r11 + 8]
    imul r9, rax
    add rcx, r9
    mul QWORD PTR [r11]
    add rdx, rcx
    CI_SLOT r10, r10d, 4
    mov QWORD PTR [r10], rax
    mov QWORD PTR [r10 + 8], rdx
    CI_NEXT
.macro CI_WIDE_UNARY name, negate
    CI_LABEL \name
    mov r10d, DWORD PTR [r13 + 8]
    mov rax, QWORD PTR [r12 + r10]
    mov rdx, QWORD PTR [r12 + r10 + 8]
    .if \negate
        neg rax
        adc rdx, 0
        neg rdx
    .else
        not rax
        not rdx
    .endif
    mov r10d, DWORD PTR [r13 + 4]
    mov QWORD PTR [r12 + r10], rax
    mov QWORD PTR [r12 + r10 + 8], rdx
    CI_NEXT
.endm
CI_WIDE_UNARY NEG128, 1
CI_WIDE_UNARY NOT128, 0
.macro CI_WIDE_COMPARE name, condition
    CI_LABEL \name
    mov r10d, DWORD PTR [r13 + 12]
    mov r11d, DWORD PTR [r13 + 16]
    mov rax, QWORD PTR [r12 + r10 + 8]
    cmp rax, QWORD PTR [r12 + r11 + 8]
    set\condition al
    jne 1f
    mov rax, QWORD PTR [r12 + r10]
    cmp rax, QWORD PTR [r12 + r11]
    .ifc \condition, l
        setb al
    .else
    .ifc \condition, le
        setbe al
    .else
    .ifc \condition, g
        seta al
    .else
    .ifc \condition, ge
        setae al
    .else
        set\condition al
    .endif
    .endif
    .endif
    .endif
1:
    mov ecx, DWORD PTR [r13 + 4]
    mov BYTE PTR [r12 + rcx], al
    CI_NEXT
.endm
.macro CI_WIDE_EQUAL name, condition
    CI_LABEL \name
    mov r10d, DWORD PTR [r13 + 12]
    mov r11d, DWORD PTR [r13 + 16]
    mov rax, QWORD PTR [r12 + r10]
    mov rdx, QWORD PTR [r12 + r10 + 8]
    xor rax, QWORD PTR [r12 + r11]
    xor rdx, QWORD PTR [r12 + r11 + 8]
    or rax, rdx
    set\condition al
    mov ecx, DWORD PTR [r13 + 4]
    mov BYTE PTR [r12 + rcx], al
    CI_NEXT
.endm
CI_WIDE_EQUAL EQ128, e
CI_WIDE_EQUAL NE128, ne
CI_WIDE_COMPARE SLT128, l
CI_WIDE_COMPARE SLE128, le
CI_WIDE_COMPARE SGT128, g
CI_WIDE_COMPARE SGE128, ge
CI_WIDE_COMPARE ULT128, b
CI_WIDE_COMPARE ULE128, be
CI_WIDE_COMPARE UGT128, a
CI_WIDE_COMPARE UGE128, ae

.macro CI_WIDE_SHIFT name, right, sign, immediate=0
    CI_LABEL \name
    mov r10d, DWORD PTR [r13 + 8]
    mov rax, QWORD PTR [r12 + r10]
    mov rdx, QWORD PTR [r12 + r10 + 8]
    .if \immediate
        mov ecx, DWORD PTR [r13 + 16]
    .else
        mov r10d, DWORD PTR [r13 + 12]
        mov ecx, DWORD PTR [r12 + r10]
    .endif
    cmp ecx, 64
    jae 2f
    .if \right
        shrd rax, rdx, cl
        .if \sign
            sar rdx, cl
        .else
            shr rdx, cl
        .endif
    .else
        shld rdx, rax, cl
        shl rax, cl
    .endif
    jmp 4f
2:
    // Here CL's hardware masking is exactly the required count minus 64.
    .if \right
        mov rax, rdx
        .if \sign
            sar rax, cl
            sar rdx, 63
        .else
            shr rax, cl
            xor edx, edx
        .endif
    .else
        mov rdx, rax
        shl rdx, cl
        xor eax, eax
    .endif
4:
    mov r10d, DWORD PTR [r13 + 4]
    mov QWORD PTR [r12 + r10], rax
    mov QWORD PTR [r12 + r10 + 8], rdx
    CI_NEXT
.endm
CI_WIDE_SHIFT SHL128, 0, 0
CI_WIDE_SHIFT LSR128, 1, 0
CI_WIDE_SHIFT ASR128, 1, 1
CI_WIDE_SHIFT SHL_IMM128, 0, 0, 1
CI_WIDE_SHIFT ASR_IMM128, 1, 1, 1
CI_WIDE_SHIFT LSR_IMM128, 1, 0, 1

.macro CI_COND_JUMP name, bytes, nonzero
    CI_LABEL \name
    mov eax, DWORD PTR [r13 + 8]
    .if \bytes == 16
        mov rcx, QWORD PTR [r12 + rax]
        or rcx, QWORD PTR [r12 + rax + 8]
    .elseif \bytes == 1
        cmp BYTE PTR [r12 + rax], 0
    .elseif \bytes == 2
        cmp WORD PTR [r12 + rax], 0
    .elseif \bytes == 4
        cmp DWORD PTR [r12 + rax], 0
    .else
        cmp QWORD PTR [r12 + rax], 0
    .endif
    .if \nonzero
        jnz Lci_JUMP
    .else
        jz Lci_JUMP
    .endif
    CI_NEXT
.endm
#define CI_JZ_OP(name, bytes) CI_COND_JUMP name, bytes, 0 ;
CI_JUMP_FALSE_OPS(CI_JZ_OP)
#undef CI_JZ_OP
#define CI_JNZ_OP(name, bytes) CI_COND_JUMP name, bytes, 1 ;
CI_JUMP_TRUE_OPS(CI_JNZ_OP)
#undef CI_JNZ_OP

.macro CI_INDEX name, pointer_bytes, index_bytes, sign
    CI_LABEL \name
    CI_LOAD_SLOT 12, \index_bytes, \sign
    mov ecx, DWORD PTR [r13 + 16]
    imul rcx, rax
    CI_LOAD_SLOT 8, \pointer_bytes
    add rax, rcx
    mov r10d, DWORD PTR [r13 + 4]
    CI_STORE \pointer_bytes, r12+r10
    CI_NEXT
.endm
#define CI_INDEX_OP(name, pointer_bytes, index_bytes, sign) CI_INDEX name, pointer_bytes, index_bytes, sign ;
CI_INDEX_OPS(CI_INDEX_OP)
#undef CI_INDEX_OP

.macro CI_BSWAP name, bytes
    CI_LABEL \name
    CI_LOAD_SLOT 12, \bytes
    .if \bytes == 2
        rol ax, 8
    .elseif \bytes == 4
        bswap eax
    .else
        bswap rax
    .endif
    mov r10d, DWORD PTR [r13 + 8]
    CI_STORE \bytes, r12+r10
    CI_NEXT
.endm
#define CI_BSWAP_OP(name, bytes, ...) CI_BSWAP name, bytes ;
CI_BSWAP_OPS(CI_BSWAP_OP)
#undef CI_BSWAP_OP

.macro CI_TRUTH name, format, bits, negate
    CI_LABEL \name
    CI_SLOT rax, eax, 16
    .if \bits == 128
        mov rcx, QWORD PTR [rax + 8]
        mov rax, QWORD PTR [rax]
        .ifc \format, F
            btr rcx, 63
        .endif
        or rax, rcx
    .elseif \bits == 80
        movzx ecx, WORD PTR [rax + 8]
        and ecx, 0x7fff
        or rcx, QWORD PTR [rax]
    .else
        CI_READ (\bits/8)
        .ifc \format, F
            // Drop the sign bit and set ZF without a separate test.
            .if \bits == 16
                add ax, ax
            .elseif \bits == 32
                add eax, eax
            .else
                add rax, rax
            .endif
        .else
            test rax, rax
        .endif
    .endif
    .if \negate
        sete al
    .else
        setne al
    .endif
    mov ecx, DWORD PTR [r13 + 8]
    mov BYTE PTR [r12 + rcx], al
    CI_NEXT
.endm
#define CI_TRUTH_OP(name, format, bits, negate) CI_TRUTH name, format, bits, negate ;
CI_TRUTH_OPS(CI_TRUTH_OP)
#undef CI_TRUTH_OP

.macro CI_BIT_LOAD bits
    mov eax, DWORD PTR [r13 + 12]
    .if \bits == 128
        mov rdx, QWORD PTR [r12 + rax + 8]
        mov rax, QWORD PTR [r12 + rax]
    .else
        CI_READ (\bits/8), 0, r12+rax
    .endif
.endm
.macro CI_BIT_MASK bits
    .if \bits == 8
        movzx eax, al
    .elseif \bits == 16
        movzx eax, ax
    .elseif \bits == 32
        mov eax, eax
    .endif
.endm
.macro CI_CLZ bits
    xor ecx, ecx
    .if \bits == 128
        xor r9d, r9d
        lzcnt rcx, rdx
        lzcnt r9, rax
        add r9d, 64
        test rdx, rdx
        cmovz ecx, r9d
    .elseif \bits <= 32
        lzcnt ecx, eax
        .if \bits < 32
            sub ecx, (32-\bits)
        .endif
    .else
        lzcnt rcx, rax
    .endif
.endm
.macro CI_CTZ bits
    xor ecx, ecx
    .if \bits == 128
        xor r9d, r9d
        tzcnt rcx, rax
        tzcnt r9, rdx
        add r9d, 64
        test rax, rax
        cmovz ecx, r9d
    .elseif \bits < 32
        // A sentinel bit gives the bytecode width for a zero input.
        mov r9d, eax
        or r9d, (1 << \bits)
        tzcnt ecx, r9d
    .elseif \bits == 32
        tzcnt ecx, eax
    .else
        tzcnt rcx, rax
    .endif
.endm
.macro CI_POPCOUNT bits
    // Break POPCNT's false destination dependency on older Intel cores.
    xor ecx, ecx
    .if \bits <= 32
        popcnt ecx, eax
    .else
        popcnt rcx, rax
    .endif
    .if \bits == 128
        xor r9d, r9d
        popcnt r9, rdx
        add ecx, r9d
    .endif
.endm
.set CI_BIT_FFS, 0
.set CI_BIT_CLRSB, 1
.set CI_BIT_PARITY, 2
.set CI_BIT_CLZ, 3
.set CI_BIT_CTZ, 4
.set CI_BIT_POPCOUNT, 5
.set CI_BIT_BIT_WIDTH, 8
.set CI_BIT_COUNT_ZEROS, 10
.set CI_BIT_FIRST_LEADING_ONE, 11
.set CI_BIT_FIRST_LEADING_ZERO, 12
.set CI_BIT_FIRST_TRAILING_ZERO, 14
.set CI_BIT_LEADING_ONES, 16
.set CI_BIT_TRAILING_ONES, 18
.macro CI_BIT name, bits, operation
    CI_LABEL \name
    CI_BIT_LOAD \bits
    .if \operation == 1
        .if \bits == 128
            mov r9, rdx
            sar r9, 63
            xor rdx, r9
        .else
            .if \bits == 8
                movsx r9, al
            .elseif \bits == 16
                movsx r9, ax
            .elseif \bits == 32
                movsxd r9, eax
            .else
                mov r9, rax
            .endif
            sar r9, 63
        .endif
        xor rax, r9
        CI_BIT_MASK \bits
        CI_CLZ \bits
        dec ecx
    .elseif \operation == 2 || \operation == 5 || \operation == 10 || \operation == 15
        CI_POPCOUNT \bits
        .if \operation == 2
            and ecx, 1
        .elseif \operation == 10
            neg ecx
            add ecx, \bits
        .elseif \operation == 15
            cmp ecx, 1
            sete cl
            movzx ecx, cl
        .endif
    .else
        .if \operation == 12 || \operation == 14 || \operation == 16 || \operation == 18
            not rax
            .if \bits == 128
                not rdx
            .endif
            CI_BIT_MASK \bits
        .endif
        .if \operation == 0 || \operation == 4 || \operation == 14 || \operation == 18
            CI_CTZ \bits
        .else
            CI_CLZ \bits
        .endif
        .if \operation == 8
            neg ecx
            add ecx, \bits
        .elseif \operation == 0 || \operation == 11 || \operation == 12 || \operation == 14
            inc ecx
            .if \bits == 128
                or rax, rdx
            .else
                test rax, rax
            .endif
            mov r9d, 0
            cmovz ecx, r9d
        .endif
    .endif
    mov eax, DWORD PTR [r13 + 4]
    mov DWORD PTR [r12 + rax], ecx
    CI_NEXT
.endm
#define CI_BIT_OP(name, bits, operation) CI_BIT name, bits, CI_BIT_##operation ;
CI_BIT_COUNT_OPS(CI_BIT_OP)
#undef CI_BIT_OP
#define CI_SINGLE_BIT_OP(name, bits) CI_BIT name, bits, 15 ;
CI_SINGLE_BIT_OPS(CI_SINGLE_BIT_OP)
#undef CI_SINGLE_BIT_OP

.macro CI_BIT_POWER name, bits, ceil
    CI_LABEL \name
    CI_BIT_LOAD \bits
    .if !\ceil && \bits <= 64
        xor ecx, ecx
        .if \bits <= 32
            bsr ecx, eax
        .else
            bsr rcx, rax
        .endif
        setnz al
        movzx eax, al
        shl rax, cl
        mov r10d, DWORD PTR [r13 + 4]
        CI_STORE (\bits/8), r12+r10
        CI_NEXT
        .exitm
    .endif
    .if \ceil
        .if \bits == 128
            test rdx, rdx
            jnz 1f
        .endif
        cmp rax, 1
        jbe Lci_power_one_\@
1:
        sub rax, 1
        .if \bits == 128
            sbb rdx, 0
        .endif
    .endif
    CI_CLZ \bits
    neg ecx
    .if \ceil && \bits <= 64
        // Shifting two by exponent-1 also yields zero on overflow.
        add ecx, (\bits - 1)
        mov eax, 2
        shl rax, cl
    .else
        add ecx, (\bits - 1 + \ceil)
        xor eax, eax
        .if \bits == 128
            xor edx, edx
        .endif
        cmp ecx, \bits
        jae 4f
        .if \bits == 128
            cmp ecx, 64
            jb 3f
            mov edx, 1
            shl rdx, cl
            jmp 4f
        .endif
3:
        mov eax, 1
        shl rax, cl
    .endif
    .if \ceil
        jmp 4f
Lci_power_one_\@:
        mov eax, 1
        .if \bits == 128
            xor edx, edx
        .endif
    .endif
4:
    mov r10d, DWORD PTR [r13 + 4]
    CI_STORE (\bits/8), r12+r10
    .if \bits == 128
        mov QWORD PTR [r12 + r10 + 8], rdx
    .endif
    CI_NEXT
.endm
#define CI_BIT_CEIL_OP(name, bits) CI_BIT_POWER name, bits, 1 ;
CI_BIT_CEIL_OPS(CI_BIT_CEIL_OP)
#undef CI_BIT_CEIL_OP
#define CI_BIT_FLOOR_OP(name, bits) CI_BIT_POWER name, bits, 0 ;
CI_BIT_FLOOR_OPS(CI_BIT_FLOOR_OP)
#undef CI_BIT_FLOOR_OP

.macro CI_ROTATE name, bits, operand, right
    CI_LABEL \name
    CI_BIT_LOAD \bits
    mov r10d, DWORD PTR [r13 + 16]
    mov rcx, QWORD PTR [r12 + r10]
    .if \bits == 128
        .if \right
            neg rcx
        .endif
        test cl, 64
        jz 1f
        xchg rax, rdx
1:
        mov r9, rax
        shld rax, rdx, cl
        shld rdx, r9, cl
    .else
        .if \right
            ror \operand, cl
        .else
            rol \operand, cl
        .endif
    .endif
    mov r10d, DWORD PTR [r13 + 4]
    CI_STORE (\bits/8), r12+r10
    .if \bits == 128
        mov QWORD PTR [r12 + r10 + 8], rdx
    .endif
    CI_NEXT
.endm
.macro CI_ROTATE_WIDTH bits, operand
    CI_ROTATE ROTL\bits, \bits, \operand, 0
    CI_ROTATE ROTR\bits, \bits, \operand, 1
.endm
CI_ROTATE_WIDTH 8, al
CI_ROTATE_WIDTH 16, ax
CI_ROTATE_WIDTH 32, eax
CI_ROTATE_WIDTH 64, rax
CI_ROTATE_WIDTH 128, rax

.macro CI_ATOMIC_LOAD name, bits
    CI_LABEL \name
    mov eax, DWORD PTR [r13 + 16]
    mov r11, QWORD PTR [r12 + rax]
    mov ecx, DWORD PTR [r13 + 20]
    mov r10d, DWORD PTR [r13 + 8]
    .if \bits == 128
        add r11, rcx
        mov r8, rbx
        xor eax, eax
        xor edx, edx
        xor ebx, ebx
        xor ecx, ecx
        lock cmpxchg16b XMMWORD PTR [r11]
        mov rbx, r8
        mov QWORD PTR [r12 + r10], rax
        mov QWORD PTR [r12 + r10 + 8], rdx
    .else
        CI_READ (\bits/8), 0, r11+rcx
        CI_STORE (\bits/8), r12+r10
    .endif
    CI_NEXT
.endm
#define CI_ATOMIC_LOAD_OP(name, bits) CI_ATOMIC_LOAD name, bits ;
CI_ATOMIC_LOAD_OPS(CI_ATOMIC_LOAD_OP)
#undef CI_ATOMIC_LOAD_OP

.macro CI_ATOMIC_STORE name, bits, operand
    CI_LABEL \name
    mov eax, DWORD PTR [r13 + 8]
    mov r11, QWORD PTR [r12 + rax]
    mov ecx, DWORD PTR [r13 + 20]
    CI_LOAD_SLOT 12, (\bits/8)
    // XCHG supplies the store/load barrier required by a seq_cst store.
    xchg [r11 + rcx], \operand
    CI_NEXT
.endm
CI_ATOMIC_STORE ATOMIC_STORE8, 8, al
CI_ATOMIC_STORE ATOMIC_STORE16, 16, ax
CI_ATOMIC_STORE ATOMIC_STORE32, 32, eax
CI_ATOMIC_STORE ATOMIC_STORE64, 64, rax

.macro CI_ATOMIC_RMW name, bits, operand, desired, operation
    CI_LABEL \name
    mov eax, DWORD PTR [r13 + 16]
    mov r11, QWORD PTR [r12 + rax]
    mov ecx, DWORD PTR [r13 + 4]
    add r11, rcx
    mov r10d, DWORD PTR [r13 + 8]
    CI_LOAD_SLOT 20, (\bits/8)
    .ifc \operation, xchg
        xchg [r11], \operand
    .else
        .ifc \operation, add
            lock xadd [r11], \operand
        .else
            .ifc \operation, sub
                neg \operand
                lock xadd [r11], \operand
            .else
                mov r9, rax
                CI_READ (\bits/8), 0, r11
1:
                mov rcx, rax
                \operation rcx, r9
                lock cmpxchg [r11], \desired
                jne 1b
            .endif
        .endif
    .endif
    CI_STORE (\bits/8), r12+r10
    CI_NEXT
.endm
.macro CI_ATOMIC_CAS name, bits, operand, desired
    CI_LABEL \name
    mov eax, DWORD PTR [r13 + 12]
    mov r11, QWORD PTR [r12 + rax]
    mov ecx, DWORD PTR [r13 + 4]
    add r11, rcx
    CI_LOAD_SLOT 20, (\bits/8)
    mov rcx, rax
    mov r10d, DWORD PTR [r13 + 16]
    CI_READ (\bits/8), 0, r12+r10
    lock cmpxchg [r11], \desired
    sete r9b
    CI_STORE (\bits/8), r12+r10
    mov eax, DWORD PTR [r13 + 8]
    mov BYTE PTR [r12 + rax], r9b
    CI_NEXT
.endm
.macro CI_ATOMIC_WIDTH bits, operand, desired
    CI_ATOMIC_RMW ATOMIC_XCHG\bits, \bits, \operand, \desired, xchg
    CI_ATOMIC_RMW ATOMIC_ADD\bits, \bits, \operand, \desired, add
    CI_ATOMIC_RMW ATOMIC_SUB\bits, \bits, \operand, \desired, sub
    CI_ATOMIC_RMW ATOMIC_AND\bits, \bits, \operand, \desired, and
    CI_ATOMIC_RMW ATOMIC_OR\bits, \bits, \operand, \desired, or
    CI_ATOMIC_RMW ATOMIC_XOR\bits, \bits, \operand, \desired, xor
    CI_ATOMIC_CAS ATOMIC_CAS\bits, \bits, \operand, \desired
    CI_ATOMIC_CAS ATOMIC_CAS_WEAK\bits, \bits, \operand, \desired
.endm
CI_ATOMIC_WIDTH 8, al, cl
CI_ATOMIC_WIDTH 16, ax, cx
CI_ATOMIC_WIDTH 32, eax, ecx
CI_ATOMIC_WIDTH 64, rax, rcx

CI_LABEL ATOMIC_STORE128
    CI_SLOT rax, eax, 8
    mov r11, QWORD PTR [rax]
    mov ecx, DWORD PTR [r13 + 20]
    add r11, rcx
    CI_SLOT r10, r10d, 12
    mov r8, rbx
    mov rbx, QWORD PTR [r10]
    mov rcx, QWORD PTR [r10 + 8]
    xor eax, eax
    xor edx, edx
1:
    lock cmpxchg16b XMMWORD PTR [r11]
    jne 1b
    mov rbx, r8
    CI_NEXT
CI_LABEL ATOMIC_XCHG128
    CI_SLOT rax, eax, 16
    mov r11, QWORD PTR [rax]
    mov ecx, DWORD PTR [r13 + 4]
    add r11, rcx
    CI_SLOT r10, r10d, 20
    mov r8, rbx
    mov rbx, QWORD PTR [r10]
    mov rcx, QWORD PTR [r10 + 8]
    xor eax, eax
    xor edx, edx
1:
    lock cmpxchg16b XMMWORD PTR [r11]
    jne 1b
    mov rbx, r8
    CI_SLOT r10, r10d, 8
    mov QWORD PTR [r10], rax
    mov QWORD PTR [r10 + 8], rdx
    CI_NEXT
CI_LABEL ATOMIC_CAS_WEAK128
CI_LABEL ATOMIC_CAS128
    CI_SLOT rax, eax, 12
    mov r11, QWORD PTR [rax]
    mov ecx, DWORD PTR [r13 + 4]
    add r11, rcx
    CI_SLOT rax, eax, 20
    mov r8, rbx
    mov rbx, QWORD PTR [rax]
    mov rcx, QWORD PTR [rax + 8]
    CI_SLOT r10, r10d, 16
    mov rax, QWORD PTR [r10]
    mov rdx, QWORD PTR [r10 + 8]
    lock cmpxchg16b XMMWORD PTR [r11]
    sete r9b
    mov rbx, r8
    mov QWORD PTR [r10], rax
    mov QWORD PTR [r10 + 8], rdx
    mov eax, DWORD PTR [r13 + 8]
    mov BYTE PTR [r12 + rax], r9b
    CI_NEXT

.macro CI_BF_EXTRACT bytes, sign, packed
    .if !\packed && \bytes < 16 && !\sign
        // The adjacent offset/width bytes are already a BEXTR control word.
        movzx ecx, WORD PTR [r13 + 2]
        CI_READ \bytes
        bextr rax, rax, rcx
        .exitm
    .endif
    movzx r10d, BYTE PTR [r13 + 3]
    test r10d, r10d
    jz Lci_bf_zero_\@
    .if !\packed && \bytes < 16
        CI_READ \bytes
        movzx ecx, BYTE PTR [r13 + 2]
        shrx rax, rax, rcx
    .elseif !\packed
        mov rdx, QWORD PTR [rax + 8]
        mov rax, QWORD PTR [rax]
        movzx ecx, BYTE PTR [r13 + 2]
        cmp ecx, 64
        jae Lci_bf_upper_\@
        shrd rax, rdx, cl
        shr rdx, cl
        jmp Lci_bf_mask_\@
Lci_bf_upper_\@:
        mov rax, rdx
        shr rax, cl
        xor edx, edx
Lci_bf_mask_\@:
    .else
        // Stage only the bytes touched by the field, including a possible
        // seventeenth byte. Never overread a packed object at a page boundary.
        mov QWORD PTR [rsp], rsi
        mov QWORD PTR [rsp + 8], rdi
        pxor xmm0, xmm0
        movdqu XMMWORD PTR [rsp + 16], xmm0
        .if \bytes == 16
            mov QWORD PTR [rsp + 32], 0
        .endif
        movzx r9d, BYTE PTR [r13 + 2]
        mov ecx, r9d
        shr ecx, 3
        lea rsi, [rax + rcx]
        and r9d, 7
        lea ecx, [r9 + r10 + 7]
        shr ecx, 3
        lea rdi, [rsp + 16]
        rep movsb
        mov rsi, QWORD PTR [rsp]
        mov rdi, QWORD PTR [rsp + 8]
        mov rax, QWORD PTR [rsp + 16]
        mov rdx, QWORD PTR [rsp + 24]
        .if \bytes == 16
            mov r8, QWORD PTR [rsp + 32]
        .endif
        mov ecx, r9d
        shrd rax, rdx, cl
        .if \bytes == 16
            shrd rdx, r8, cl
        .endif
    .endif
    .if \bytes == 16
        cmp r10d, 64
        ja Lci_bf_high_\@
    .endif
    .if \sign
        mov ecx, 64
        sub ecx, r10d
        shl rax, cl
        sar rax, cl
        .if \bytes == 16
            mov rdx, rax
            sar rdx, 63
        .endif
    .else
        // BZHI preserves all 64 bits when the field fills the limb.
        bzhi rax, rax, r10
        .if \bytes == 16
            xor edx, edx
        .endif
    .endif
    .if \bytes == 16
        jmp Lci_bf_done_\@
Lci_bf_high_\@:
        .if \sign
            mov ecx, 128
            sub ecx, r10d
            shl rdx, cl
            sar rdx, cl
        .else
            lea ecx, [r10 - 64]
            bzhi rdx, rdx, rcx
        .endif
    .endif
    jmp Lci_bf_done_\@
Lci_bf_zero_\@:
    xor eax, eax
    .if \bytes == 16
        xor edx, edx
    .endif
Lci_bf_done_\@:
.endm
.macro CI_BF_LOAD name, bytes, sign, packed, slot=0
    CI_LABEL \name
    CI_SLOT rax, eax, 16
    .if !\slot
        mov rax, QWORD PTR [rax]
    .endif
    mov ecx, DWORD PTR [r13 + 20]
    add rax, rcx
    CI_BF_EXTRACT \bytes, \sign, \packed
    CI_SLOT r10, r10d, 8
    CI_STORE \bytes
    .if \bytes == 16
        mov QWORD PTR [r10 + 8], rdx
    .endif
    CI_NEXT
.endm
#define CI_BF_LOAD_OP(name, bytes, sign, align) CI_BF_LOAD name, bytes, sign, 0 ;
CI_LOAD_BITFIELD_OPS(CI_BF_LOAD_OP)
#undef CI_BF_LOAD_OP
#define CI_BF_LOAD_OP(name, uns, align) CI_BF_LOAD name, 16, (1-uns), 0 ;
CI_LOAD_BITFIELD_128_OPS(CI_BF_LOAD_OP)
#undef CI_BF_LOAD_OP
#define CI_BF_LOAD_OP(name, bytes, sign) CI_BF_LOAD name, bytes, sign, 1 ;
CI_LOAD_BITFIELD_BYTES_OPS(CI_BF_LOAD_OP)
#undef CI_BF_LOAD_OP
#define CI_BF_LOAD_OP(name, uns) CI_BF_LOAD name, 16, (1-uns), 1 ;
CI_LOAD_BITFIELD_BYTES_128_OPS(CI_BF_LOAD_OP)
#undef CI_BF_LOAD_OP
#define CI_BF_LOAD_OP(name, bytes, sign, align) CI_BF_LOAD name, bytes, sign, 0, 1 ;
CI_LOAD_SLOT_BITFIELD_OPS(CI_BF_LOAD_OP)
#undef CI_BF_LOAD_OP
#define CI_BF_LOAD_OP(name, uns, align) CI_BF_LOAD name, 16, (1-uns), 0, 1 ;
CI_LOAD_SLOT_BITFIELD_128_OPS(CI_BF_LOAD_OP)
#undef CI_BF_LOAD_OP
#define CI_BF_LOAD_OP(name, bytes, sign) CI_BF_LOAD name, bytes, sign, 1, 1 ;
CI_LOAD_SLOT_BITFIELD_BYTES_OPS(CI_BF_LOAD_OP)
#undef CI_BF_LOAD_OP
#define CI_BF_LOAD_OP(name, uns) CI_BF_LOAD name, 16, (1-uns), 1, 1 ;
CI_LOAD_SLOT_BITFIELD_BYTES_128_OPS(CI_BF_LOAD_OP)
#undef CI_BF_LOAD_OP

.macro CI_BF_STORE name, bytes, packed
    CI_LABEL \name
    CI_SLOT rax, eax, 12
    .if \bytes == 16
        mov rdx, QWORD PTR [rax + 8]
        mov rax, QWORD PTR [rax]
    .else
        CI_READ \bytes
        xor edx, edx
    .endif
    CI_SLOT r10, r10d, 8
    mov r8, QWORD PTR [r10]
    mov ecx, DWORD PTR [r13 + 20]
    add r8, rcx
    movzx ecx, BYTE PTR [r13 + 2]
    movzx r10d, BYTE PTR [r13 + 3]
    test r10d, r10d
    jz Lci_next
    .if !\packed && \bytes < 16
        mov r11d, ecx
        mov ecx, 64
        sub ecx, r10d
        mov r9, -1
        shr r9, cl
        and rax, r9
        mov ecx, r11d
        shl rax, cl
        shl r9, cl
        not r9
        mov rdx, rax
        mov rax, r8
        CI_READ \bytes
        and rax, r9
        or rax, rdx
        mov r10, r8
        CI_STORE \bytes
    .elseif !\packed
        mov r9, -1
        xor r11d, r11d
        cmp r10d, 64
        jbe Lci_bf_mask_low_\@
        mov r11, r9
        sub r10d, 64
        bzhi r11, r11, r10
        jmp Lci_bf_mask_ready_\@
Lci_bf_mask_low_\@:
        bzhi r9, r9, r10
Lci_bf_mask_ready_\@:
        and rax, r9
        and rdx, r11
        cmp ecx, 64
        jae Lci_bf_shift_upper_\@
        shld rdx, rax, cl
        shl rax, cl
        shld r11, r9, cl
        shl r9, cl
        jmp Lci_bf_merge_\@
Lci_bf_shift_upper_\@:
        mov rdx, rax
        shl rdx, cl
        xor eax, eax
        mov r11, r9
        shl r11, cl
        xor r9d, r9d
Lci_bf_merge_\@:
        not r9
        not r11
        and r9, QWORD PTR [r8]
        and r11, QWORD PTR [r8 + 8]
        or rax, r9
        or rdx, r11
        mov QWORD PTR [r8], rax
        mov QWORD PTR [r8 + 8], rdx
    .else
        mov QWORD PTR [rsp + 8], r10
        mov r9d, ecx
        shr ecx, 3
        add r8, rcx
        and r9d, 7
        mov DWORD PTR [rsp], r9d
Lci_bf_write_\@:
        mov ecx, DWORD PTR [rsp]
        mov r11d, 8
        sub r11d, ecx
        cmp r11d, DWORD PTR [rsp + 8]
        cmova r11d, DWORD PTR [rsp + 8]
        mov ecx, r11d
        mov r9d, 1
        shl r9d, cl
        dec r9d
        mov r10d, eax
        and r10d, r9d
        mov ecx, DWORD PTR [rsp]
        shl r10d, cl
        shl r9d, cl
        not r9d
        and r9b, BYTE PTR [r8]
        or r9b, r10b
        mov BYTE PTR [r8], r9b
        mov ecx, r11d
        .if \bytes == 16
            shrd rax, rdx, cl
            shr rdx, cl
        .else
            shr rax, cl
        .endif
        mov DWORD PTR [rsp], 0
        inc r8
        sub DWORD PTR [rsp + 8], r11d
        jnz Lci_bf_write_\@
    .endif
    CI_NEXT
.endm
#define CI_BF_STORE_OP(name, bytes, align) CI_BF_STORE name, bytes, 0 ;
CI_STORE_BITFIELD_OPS(CI_BF_STORE_OP)
#undef CI_BF_STORE_OP
#define CI_BF_STORE_OP(name, align) CI_BF_STORE name, 16, 0 ;
CI_STORE_BITFIELD_128_OPS(CI_BF_STORE_OP)
#undef CI_BF_STORE_OP
#define CI_BF_STORE_OP(name, bytes) CI_BF_STORE name, bytes, 1 ;
CI_STORE_BITFIELD_BYTES_OPS(CI_BF_STORE_OP)
#undef CI_BF_STORE_OP
#define CI_BF_STORE_OP(name, unused) CI_BF_STORE name, 16, 1 ;
CI_STORE_BITFIELD_BYTES_128_OPS(CI_BF_STORE_OP)
#undef CI_BF_STORE_OP

.macro CI_BF_JUMP name, bytes, slot, nonzero
    CI_LABEL \name
    CI_SLOT rax, eax, 8
    .if !\slot
        mov rax, QWORD PTR [rax]
    .endif
    mov ecx, DWORD PTR [r13 + 12]
    add rax, rcx
    .if \bytes
        movzx ecx, WORD PTR [r13 + 2]
        CI_READ \bytes
        bextr rax, rax, rcx
    .else
        CI_BF_EXTRACT 16, 0, 1
        or rax, rdx
    .endif
    .if \nonzero
        jnz Lci_JUMP
    .else
        jz Lci_JUMP
    .endif
    CI_NEXT
.endm
#define CI_BF_JUMP_OP(name, bytes, slot, nonzero) CI_BF_JUMP name, bytes, slot, nonzero ;
CI_BITFIELD_JUMP_OPS(CI_BF_JUMP_OP)
#undef CI_BF_JUMP_OP

.set CI_ADDR_SLOT, 0
.set CI_ADDR_REG, 1
.set CI_ADDR_INCOMING, 2
.set CI_ADDR_REG_PTR, 3
.set CI_ADDR_INCOMING_PTR, 4
.set CI_ADDR_REG_PTR_INDIRECT, 5
.set CI_ADDR_SLOT_PTR, 7
.set CI_ADDR_INCOMING_SAVE, 8
.set CI_XFER_COPY, 0
.set CI_XFER_ADDRESS, 1
.set CI_XFER_ZERO, 2
.set CI_XFER_CONSTANT, 3
// Copy: r10 = src, r11 = dst, rax = size. Overlap permitted.
// Zero: r11 = dst, rax = size.
// Clobbers: rax, r10, r11, xmm8, flags. Preserves argument registers.
Lci_abi_copy:
    test rax, rax
    jz Lci_abi_copy_done
    cmp r11, r10
    jbe Lci_abi_copy_forward
    add r10, rax
    add r11, rax
Lci_abi_copy_backward:
    cmp rax, 16
    jb Lci_abi_copy_backward_tail
    sub r10, 16
    sub r11, 16
    movdqu xmm8, XMMWORD PTR [r10]
    movdqu XMMWORD PTR [r11], xmm8
    sub rax, 16
    jmp Lci_abi_copy_backward
Lci_abi_copy_backward_tail:
    test rax, rax
    jz Lci_abi_copy_done
    dec r10
    dec r11
    pinsrb xmm8, BYTE PTR [r10], 0
    pextrb BYTE PTR [r11], xmm8, 0
    dec rax
    jmp Lci_abi_copy_backward_tail
Lci_abi_copy_forward:
    cmp rax, 16
    jb Lci_abi_copy_forward_tail
    movdqu xmm8, XMMWORD PTR [r10]
    movdqu XMMWORD PTR [r11], xmm8
    add r10, 16
    add r11, 16
    sub rax, 16
    jmp Lci_abi_copy_forward
Lci_abi_copy_forward_tail:
    test rax, rax
    jz Lci_abi_copy_done
    pinsrb xmm8, BYTE PTR [r10], 0
    pextrb BYTE PTR [r11], xmm8, 0
    inc r10
    inc r11
    dec rax
    jmp Lci_abi_copy_forward_tail
Lci_abi_copy_done:
    ret
Lci_abi_zero:
    pxor xmm8, xmm8
1:
    cmp rax, 16
    jb 2f
    movdqu XMMWORD PTR [r11], xmm8
    add r11, 16
    sub rax, 16
    jmp 1b
2:
    test rax, rax
    jz 3f
    mov BYTE PTR [r11], 0
    inc r11
    dec rax
    jmp 2b
3:
    ret

// Argument offsets select registers; result/metadata offsets select CiStack.
// Get: r11d = offset -> xmm8. Clobbers flags.
// Put: r11d = offset, eax = size, xmm8 = value.
// Put clobbers rax, r10, xmm8, flags, and the destination.
Lci_abi_get_bank:
    .irp reg, 0, 1, 2, 3, 4, 5
        .if \reg < CI_GP_COUNT
            cmp r11d, (\reg*8)
            je Lci_abi_get_gp\reg
        .endif
    .endr
    .irp reg, 0, 1, 2, 3, 4, 5, 6, 7
        .if \reg < CI_FP_COUNT
            cmp r11d, (CI_FP_BASE + \reg*16)
            je Lci_abi_get_fp\reg
        .endif
    .endr
    movdqu xmm8, XMMWORD PTR [r15 + r11]
    ret
    .irp reg, 0, 1, 2, 3, 4, 5
        .if \reg < CI_GP_COUNT
Lci_abi_get_gp\reg:
            CI_ARG_GP \reg, movq, xmm8, 1
            ret
        .endif
    .endr
    .irp reg, 0, 1, 2, 3, 4, 5, 6, 7
        .if \reg < CI_FP_COUNT
Lci_abi_get_fp\reg:
            movdqa xmm8, xmm\reg
            ret
        .endif
    .endr
Lci_abi_put_bank:
    .irp reg, 0, 1, 2, 3, 4, 5
        .if \reg < CI_GP_COUNT
            cmp r11d, (\reg*8)
            je Lci_abi_put_gp\reg
        .endif
    .endr
    .irp reg, 0, 1, 2, 3, 4, 5, 6, 7
        .if \reg < CI_FP_COUNT
            cmp r11d, (CI_FP_BASE + \reg*16)
            je Lci_abi_put_fp\reg
        .endif
    .endr
    lea r10, [r15 + r11]
    cmp eax, 16
    je 1f
    cmp eax, 8
    je 2f
    cmp eax, 4
    je 3f
    cmp eax, 2
    je 6f
    cmp eax, 1
    je 7f
4:
    test eax, eax
    jz 5f
    pextrb BYTE PTR [r10], xmm8, 0
    psrldq xmm8, 1
    inc r10
    dec eax
    jmp 4b
1:
    movdqu XMMWORD PTR [r10], xmm8
    ret
2:
    movq QWORD PTR [r10], xmm8
    ret
3:
    movd DWORD PTR [r10], xmm8
5:
    ret
6:
    pextrw WORD PTR [r10], xmm8, 0
    ret
7:
    pextrb BYTE PTR [r10], xmm8, 0
    ret
    .irp reg, 0, 1, 2, 3, 4, 5
        .if \reg < CI_GP_COUNT
Lci_abi_put_gp\reg:
            CI_ARG_GP \reg, movq, xmm8
            ret
        .endif
    .endr
    .irp reg, 0, 1, 2, 3, 4, 5, 6, 7
        .if \reg < CI_FP_COUNT
Lci_abi_put_fp\reg:
            movdqa xmm\reg, xmm8
            ret
        .endif
    .endr

.macro CI_ABI_VALUE_LOAD bytes
    .if \bytes == 16
        movdqu xmm8, XMMWORD PTR [r10]
    .elseif \bytes == 8
        movq xmm8, QWORD PTR [r10]
    .elseif \bytes == 4
        movd xmm8, DWORD PTR [r10]
    .elseif \bytes == 2
        movzx r11d, WORD PTR [r10]
        movq xmm8, r11
    .elseif \bytes == 1
        movzx r11d, BYTE PTR [r10]
        movq xmm8, r11
    .else
        pxor xmm8, xmm8
        lea r11, [rbp - CI_FRAME_OFFSET]
        movdqu XMMWORD PTR [r11], xmm8
        mov eax, DWORD PTR [r13]
        shr eax, 18
        and eax, 31
        call Lci_abi_copy
        movdqu xmm8, XMMWORD PTR [rbp - CI_FRAME_OFFSET]
    .endif
.endm
.macro CI_ABI_VALUE_STORE bytes
    .if \bytes == 16
        movdqu XMMWORD PTR [r10], xmm8
    .elseif \bytes == 8
        movq QWORD PTR [r10], xmm8
    .elseif \bytes == 4
        movd DWORD PTR [r10], xmm8
    .elseif \bytes == 2
        pextrw WORD PTR [r10], xmm8, 0
    .elseif \bytes == 1
        pextrb BYTE PTR [r10], xmm8, 0
    .else
        movdqu XMMWORD PTR [rbp - CI_FRAME_OFFSET], xmm8
        mov r11, r10
        lea r10, [rbp - CI_FRAME_OFFSET]
        mov eax, DWORD PTR [r13]
        shr eax, 18
        and eax, 31
        call Lci_abi_copy
    .endif
.endm
.macro CI_ABI_ADDRESS mode, offset, pointer
    .if \mode == 0
        CI_SLOT rax, eax, \offset
    .elseif \mode == 1
        movzx r11d, WORD PTR [r13 + \offset + 2]
        call Lci_abi_get_bank
        movdqu XMMWORD PTR [rsp], xmm8
        lea rax, [rsp]
    .elseif \mode == 2
        movzx r11d, WORD PTR [r13 + \offset + 2]
        mov rax, QWORD PTR [r12 + CI_ASM_incoming - CI_ASM_frame_size]
        add rax, r11
    .elseif \mode == 7
        mov eax, DWORD PTR [r13 + \pointer]
        mov rax, QWORD PTR [r12 + rax]
        movsxd r11, DWORD PTR [r13 + \offset]
        add rax, r11
    .else
        movzx r11d, WORD PTR [r13 + \pointer + 2]
        .if \mode == 3 || \mode == 5
            call Lci_abi_get_bank
            movq rax, xmm8
        .else
            mov rax, QWORD PTR [r12 + CI_ASM_incoming - CI_ASM_frame_size]
            add rax, r11
            .if \mode < 8
                mov rax, QWORD PTR [rax]
            .endif
        .endif
        .if \mode < 8
            movsxd r11, DWORD PTR [r13 + \offset]
        .else
            mov r11d, DWORD PTR [r13 + \offset]
        .endif
        add rax, r11
        .if \mode == 5 || \mode == 6
            mov rax, QWORD PTR [rax]
        .endif
    .endif
.endm
.macro CI_ABI_TRANSFER name, operation, src, dst, bytes
    CI_LABEL \name
    .if \dst != 1
        CI_ABI_ADDRESS \dst, 8, 20
        mov QWORD PTR [rsp + 16], rax
    .endif
    .if \operation == 3
        mov eax, DWORD PTR [r13 + 4]
        movq xmm8, rax
    .elseif \operation == 2
        .if \dst != 1 && !\bytes
            mov r11, QWORD PTR [rsp + 16]
            mov eax, DWORD PTR [r13 + 12]
            call Lci_abi_zero
            CI_NEXT
            .exitm
        .endif
        pxor xmm8, xmm8
    .else
        CI_ABI_ADDRESS \src, 4, 16
        .if \operation == 1
            movq xmm8, rax
        .elseif !\bytes
            mov r10, rax
            mov r11, QWORD PTR [rsp + 16]
            mov eax, DWORD PTR [r13 + 12]
            call Lci_abi_copy
            CI_NEXT
            .exitm
        .else
            mov r10, rax
            CI_ABI_VALUE_LOAD \bytes
        .endif
    .endif
    .if \dst == 1
        movzx r11d, WORD PTR [r13 + 10]
        .if \operation == 1
            mov eax, 8
        .else
            mov eax, \bytes
        .endif
        call Lci_abi_put_bank
    .else
        mov r10, QWORD PTR [rsp + 16]
        .if \operation == 1
            CI_ABI_VALUE_STORE 8
        .else
            CI_ABI_VALUE_STORE \bytes
        .endif
    .endif
    CI_NEXT
.endm
#define CI_ABI_TRANSFER_OP(name, operation, src, dst, bytes, alignment) CI_ABI_TRANSFER name, CI_XFER_##operation, CI_ADDR_##src, CI_ADDR_##dst, bytes ;
CI_ABI_TRANSFER_OPS(CI_ABI_TRANSFER_OP)
#undef CI_ABI_TRANSFER_OP

// GP/FP opcodes serve both argument and result banks; capture_offset selects.
.macro CI_ABI_MOVE name, incoming, write, operation, bytes, alignment, source
    CI_LABEL \name
    .if \incoming
        movzx r11d, WORD PTR [r13 + 10]
        mov r10, QWORD PTR [r12 + CI_ASM_incoming - CI_ASM_frame_size]
        add r10, r11
        CI_ABI_VALUE_LOAD \bytes
    .elseif \write
        CI_SLOT r10, r10d, 4
        .if \source
            mov rax, r10
            .ifc \operation, SIGN_EXTEND
                CI_READ \source, 1
            .else
                CI_READ \source
            .endif
            movq xmm8, rax
        .else
            CI_ABI_VALUE_LOAD \bytes
        .endif
        movzx r11d, WORD PTR [r13 + 10]
        .if \source
            mov eax, 8
        .elseif \bytes
            mov eax, \bytes
        .else
            mov eax, DWORD PTR [r13]
            shr eax, 18
            and eax, 31
        .endif
        call Lci_abi_put_bank
        CI_NEXT
        .exitm
    .else
        movzx r11d, WORD PTR [r13 + 10]
        call Lci_abi_get_bank
    .endif
    CI_SLOT r10, r10d, 4
    CI_ABI_VALUE_STORE \bytes
    CI_NEXT
.endm
#define CI_ABI_MOVE_OP(name, incoming, write, operation, bytes, alignment, source) CI_ABI_MOVE name, incoming, write, operation, bytes, alignment, source ;
CI_ABI_MOVE_OPS(CI_ABI_MOVE_OP)
#undef CI_ABI_MOVE_OP
.macro CI_ABI_GP_MOVE name, write, reg, alignment
    CI_LABEL \name
    .if \reg < CI_GP_COUNT
        cmp WORD PTR [r13 + 10], (\reg*8)
        jne Lci_gp_record_\@
        mov eax, DWORD PTR [r13 + 4]
        .if \write
            CI_ARG_GP \reg, mov, [r12 + rax]
        .else
            CI_ARG_GP \reg, mov, [r12 + rax], 1
        .endif
        CI_NEXT
Lci_gp_record_\@:
    .endif
    movzx r11d, WORD PTR [r13 + 10]
    CI_SLOT r10, r10d, 4
    .if \write
        movq xmm8, QWORD PTR [r10]
        mov eax, 8
        call Lci_abi_put_bank
    .else
        call Lci_abi_get_bank
        movq QWORD PTR [r10], xmm8
    .endif
    CI_NEXT
.endm
#define CI_ABI_GP_OP(name, write, reg, alignment) CI_ABI_GP_MOVE name, write, reg, alignment ;
CI_ABI_GP_MOVE_OPS(CI_ABI_GP_OP)
#undef CI_ABI_GP_OP

.macro CI_ABI_REG_MOVE name, write, reg, bytes, alignment
    CI_LABEL \name
    movzx r11d, WORD PTR [r13 + 10]
    .if \reg < CI_GP_COUNT
        cmp r11d, (\reg*8)
        jne Lci_abi_not_gp_\@
        CI_SLOT r10, r10d, 4
        .if \write
            mov rax, r10
            mov r11d, DWORD PTR [r13]
            shr r11d, 29
            cmp r11d, 5
            jne 1f
            CI_READ \bytes, 1
            jmp 2f
1:
            CI_READ \bytes
2:
            CI_ARG_GP \reg, mov, rax
        .else
            CI_ARG_GP \reg, mov, rax, 1
            CI_STORE \bytes
        .endif
        CI_NEXT
Lci_abi_not_gp_\@:
    .endif
    .if \reg < CI_FP_COUNT
        cmp r11d, (CI_FP_BASE + \reg*16)
        jne Lci_abi_not_fp_\@
        CI_SLOT r10, r10d, 4
        .if \write
            .if \bytes == 16
                movdqu xmm\reg, XMMWORD PTR [r10]
            .elseif \bytes == 8
                movq xmm\reg, QWORD PTR [r10]
            .elseif \bytes == 4
                movss xmm\reg, DWORD PTR [r10]
            .else
                CI_ABI_VALUE_LOAD \bytes
                movdqa xmm\reg, xmm8
            .endif
        .else
            .if \bytes == 16
                movdqu XMMWORD PTR [r10], xmm\reg
            .elseif \bytes == 8
                movq QWORD PTR [r10], xmm\reg
            .elseif \bytes == 4
                movss DWORD PTR [r10], xmm\reg
            .else
                movdqa xmm8, xmm\reg
                CI_ABI_VALUE_STORE \bytes
            .endif
        .endif
        CI_NEXT
Lci_abi_not_fp_\@:
    .endif
    .if \write
        CI_SLOT r10, r10d, 4
        mov r11d, DWORD PTR [r13]
        shr r11d, 29
        test r11d, r11d
        jz Lci_abi_record_copy_\@
        mov rax, r10
        cmp r11d, 5
        jne Lci_abi_record_unsigned_\@
        CI_READ \bytes, 1
        jmp Lci_abi_record_extended_\@
Lci_abi_record_unsigned_\@:
        CI_READ \bytes
Lci_abi_record_extended_\@:
        movq xmm8, rax
        mov eax, 8
        jmp Lci_abi_record_put_\@
Lci_abi_record_copy_\@:
        CI_ABI_VALUE_LOAD \bytes
        mov eax, \bytes
Lci_abi_record_put_\@:
        movzx r11d, WORD PTR [r13 + 10]
        call Lci_abi_put_bank
    .else
        call Lci_abi_get_bank
        CI_SLOT r10, r10d, 4
        CI_ABI_VALUE_STORE \bytes
    .endif
    CI_NEXT
.endm
#define CI_ABI_REG_OP(name, write, reg, bytes, alignment) CI_ABI_REG_MOVE name, write, reg, bytes, alignment ;
CI_ABI_REG_MOVE_OPS(CI_ABI_REG_OP)
#undef CI_ABI_REG_OP

.macro CI_ABI_PAIR name, write, alignment, contiguous
    CI_LABEL \name
    cmp WORD PTR [r13 + 14], 0
    jne Lci_pair_record_\@
    cmp WORD PTR [r13 + 18], 8
    jne Lci_pair_record_\@
    mov eax, DWORD PTR [r13 + 4]
    .if \write
        CI_ARG_GP 0, mov, [r12 + rax]
    .else
        CI_ARG_GP 0, mov, [r12 + rax], 1
    .endif
    mov eax, DWORD PTR [r13 + 8]
    .if \write
        CI_ARG_GP 1, mov, [r12 + rax]
    .else
        CI_ARG_GP 1, mov, [r12 + rax], 1
    .endif
    CI_NEXT
Lci_pair_record_\@:
    .irp member, 0, 1
        .if \write
            CI_SLOT r10, r10d, (4 + \member*4)
            movq xmm8, QWORD PTR [r10]
            movzx r11d, WORD PTR [r13 + 14 + \member*4]
            mov eax, 8
            call Lci_abi_put_bank
        .else
            movzx r11d, WORD PTR [r13 + 14 + \member*4]
            call Lci_abi_get_bank
            CI_SLOT r10, r10d, (4 + \member*4)
            movq QWORD PTR [r10], xmm8
        .endif
    .endr
    CI_NEXT
.endm
#define CI_ABI_PAIR_OP(name, write, alignment, contiguous) CI_ABI_PAIR name, write, alignment, contiguous ;
CI_ABI_PAIR_OPS(CI_ABI_PAIR_OP)
#undef CI_ABI_PAIR_OP

CI_LABEL SLOT_ADDR
    mov eax, DWORD PTR [r13 + 12]
    add rax, r12
    mov ecx, DWORD PTR [r13 + 4]
    mov QWORD PTR [r12 + rcx], rax
    CI_NEXT
CI_LABEL THREAD_FENCE
    // A locked RMW on our own stack orders loads and stores without MFENCE's
    // stronger instruction-stream serialization on some x86 implementations.
    lock or DWORD PTR [rsp], 0
    CI_NEXT
CI_LABEL SIGNAL_FENCE
    CI_NEXT

CI_LABEL JUMP
    movsxd rax, DWORD PTR [r13 + 4]
    shl rax, 5
    add r13, rax
    CI_DISPATCH
Lci_next:
    CI_NEXT

.macro CI_ERROR message
    CI_PUBLISH_PC
    mov rdi, QWORD PTR [rbx]
    mov rsi, QWORD PTR [r13 + 24]
    lea rdx, [rip + \message]
    xor eax, eax
    CI_HELPER error
    jmp Lci_error
.endm
CI_LABEL CHECK
    mov eax, DWORD PTR [r13 + 4]
    cmp BYTE PTR [r12 + rax], 0
    jne Lci_next
    mov rcx, QWORD PTR [r13 + 8]
    CI_ERROR Lci_msg_string
CI_LABEL PROPAGATE_ERROR
    mov eax, DWORD PTR [r13 + 4]
    mov eax, DWORD PTR [r12 + rax]
    test eax, eax
    je Lci_next
    mov r8d, eax
    CI_PUBLISH_PC
    mov eax, r8d
    jmp Lci_error
CI_LABEL VAR_ADDR
    mov rax, QWORD PTR [r13 + 16]
    mov rax, QWORD PTR [rax + CI_ASM_var_value]
    test rax, rax
    jnz 1f
    CI_PUBLISH_PC
    mov rdi, QWORD PTR [rbx]
    mov rsi, QWORD PTR [r13 + 16]
    CI_HELPER ensure_var
    test eax, eax
    jne Lci_error
    mov rax, QWORD PTR [r13 + 16]
    mov rax, QWORD PTR [rax + CI_ASM_var_value]
1:
    CI_SLOT r10, r10d, 8
    mov QWORD PTR [r10], rax
    CI_NEXT
CI_LABEL TLS_ADDR
    CI_PUBLISH_PC
    CI_SLOT r10, r10d, 8
    mov rdi, QWORD PTR [rbx]
    mov rsi, QWORD PTR [r13 + 16]
    mov rdx, r10
    CI_HELPER tls_address
    test eax, eax
    jne Lci_error
    CI_NEXT
CI_LABEL FUNC_ADDR
    mov r10, QWORD PTR [r13 + 16]
    mov rax, QWORD PTR [r10 + CI_ASM_native_func]
    test rax, rax
    jz 1f
    CI_SLOT r10, r10d, 8
    mov QWORD PTR [r10], rax
    CI_NEXT
1:
    mov rax, QWORD PTR [r10 + 32]
    lea rcx, [rip + Lci_msg_unknown]
    test rax, rax
    jz 2f
    lea rcx, [rax + CI_ASM_atom_data]
2:
    CI_ERROR Lci_msg_missing_function
CI_LABEL CALL_RESOLVE
    CI_SLOT rax, eax, 4
    mov rsi, QWORD PTR [rax]
    mov rdi, QWORD PTR [rbx]
    CI_HELPER closure_lookup
    CI_SLOT r10, r10d, 4
    mov QWORD PTR [r10 + 8], rax
    CI_NEXT
CI_LABEL RETURN_SLOT
    mov r10, QWORD PTR [rbx + 16]
    cmp r10, QWORD PTR [rbx + 8]
    je 1f
    mov r10, QWORD PTR [r10 + CI_ASM_parent]
    mov rax, QWORD PTR [r10 + CI_ASM_code]
    mov rcx, QWORD PTR [r10 + CI_ASM_pc]
    shl rcx, 5
    lea rax, [rax + rcx + CI_ASM_code_ops]
    mov r9d, DWORD PTR [rax + 8]
    test r9d, r9d
    jz Lci_RETURN
    mov eax, DWORD PTR [rax + 4]
    lea r10, [r10 + rax + CI_ASM_frame_size]
    jmp 2f
1:
    mov r10, QWORD PTR [rbx + 32]
    mov r9, QWORD PTR [rbx + 40]
    test r10, r10
    jz Lci_RETURN
2:
    mov r8d, DWORD PTR [r13 + 12]
    cmp r8, r9
    ja 3f
    CI_SLOT rax, eax, 8
    mov rdi, r10
    mov rsi, rax
    mov rdx, r8
    CI_HELPER memmove
    jmp Lci_RETURN
3:
    mov rax, r8
    mov r8, r9
    mov r9, rax
    mov rcx, r9
    CI_ERROR Lci_msg_result_size
CI_LABEL CAPTURE_BACKTRACE
    CI_PUBLISH_PC
    CI_SLOT rax, eax, 12
    mov r8, QWORD PTR [rax]
    CI_SLOT rax, eax, 16
    mov r9d, DWORD PTR [rax]
    xor edx, edx
    test r9d, r9d
    jle Lci_capture_done
    test r8, r8
    jz Lci_capture_null
    mov r10, QWORD PTR [rbx + 16]
Lci_capture_loop:
    test r10, r10
    jz Lci_capture_done
    mov rax, QWORD PTR [r10 + CI_ASM_code]
    test rax, rax
    jz Lci_capture_parent
    mov rcx, QWORD PTR [r10 + CI_ASM_pc]
    cmp rcx, QWORD PTR [rax + CI_ASM_code_count]
    jae Lci_capture_parent
    shl rcx, 5
    mov r11, QWORD PTR [rax + rcx + CI_ASM_code_ops + 24]
    mov QWORD PTR [r8 + rdx*8], r11
    inc edx
    cmp edx, r9d
    jge Lci_capture_done
Lci_capture_parent:
    mov r10, QWORD PTR [r10 + CI_ASM_parent]
    jmp Lci_capture_loop
Lci_capture_done:
    mov eax, DWORD PTR [r13 + 4]
    mov DWORD PTR [r12 + rax], edx
    CI_NEXT
Lci_capture_null:
    CI_ERROR Lci_msg_capture_null
CI_LABEL BACKTRACE
    CI_PUBLISH_PC
    mov rdi, QWORD PTR [rbx]
    mov rsi, QWORD PTR [rbx + 16]
    xor edx, edx
    CI_HELPER backtrace
    CI_NEXT
CI_LABEL CLEAR_CACHE
    CI_NEXT
.macro CI_RUNTIME_ERROR name, message
    CI_LABEL \name
    CI_ERROR \message
.endm
CI_RUNTIME_ERROR TRAP, Lci_msg_trap
CI_RUNTIME_ERROR UNREACHABLE, Lci_msg_unreachable
CI_RUNTIME_ERROR ABORT, Lci_msg_abort
CI_LABEL DEBUGTRAP
    CI_NEXT
CI_LABEL READCYCLECOUNTER
    CI_HELPER cyclecounter
    mov ecx, DWORD PTR [r13 + 4]
    mov QWORD PTR [r12 + rcx], rax
    CI_NEXT
CI_LABEL READCYCLECOUNTER_DISCARD
    CI_HELPER cyclecounter
    CI_NEXT
.macro CI_ALLOC name, helper
    CI_LABEL \name
    CI_PUBLISH_PC
    mov eax, DWORD PTR [r13 + 12]
    mov rsi, QWORD PTR [r12 + rax]
    mov rdi, r15
    mov edx, DWORD PTR [r13 + 4]
    CI_HELPER \helper
    test rax, rax
    jnz 1f
    mov eax, 1
    jmp Lci_error
1:
    mov r10d, DWORD PTR [r13 + 8]
    mov QWORD PTR [r12 + r10], rax
    CI_NEXT
.endm
CI_ALLOC STACK_ALLOC, stack_alloc
CI_ALLOC STACK_ZALLOC, stack_zalloc
.macro CI_CRC name, bytes
    CI_LABEL \name
    mov eax, DWORD PTR [r13 + 16]
    mov r10d, DWORD PTR [r13 + 12]
    mov ecx, DWORD PTR [r12 + r10]
    .if \bytes == 1
        crc32 ecx, BYTE PTR [r12 + rax]
    .elseif \bytes == 2
        crc32 ecx, WORD PTR [r12 + rax]
    .elseif \bytes == 4
        crc32 ecx, DWORD PTR [r12 + rax]
    .else
        crc32 rcx, QWORD PTR [r12 + rax]
    .endif
    mov r10d, DWORD PTR [r13 + 4]
    mov DWORD PTR [r12 + r10], ecx
    CI_NEXT
.endm
#define CI_CRC_OP(name, bytes) CI_CRC name, bytes ;
CI_CRC_OPS(CI_CRC_OP)
#undef CI_CRC_OP
.macro CI_BOUNDS name, bytes, inclusive
    CI_LABEL \name
    CI_LOAD_SLOT 8, \bytes
    mov r8, rax
    CI_LOAD_SLOT 16, \bytes
    cmp r8, rax
    .if \inclusive
        jbe Lci_next
        lea r10, [rip + Lci_msg_close_inclusive]
    .else
        jb Lci_next
        lea r10, [rip + Lci_msg_close_exclusive]
    .endif
    mov rcx, r8
    mov r8, rax
    mov r9, r10
    test BYTE PTR [r13 + 2], 2
    jnz 1f
    CI_ERROR Lci_msg_bounds_unsigned
1:
    CI_ERROR Lci_msg_bounds_signed
.endm
CI_BOUNDS BOUNDS32, 4, 0
CI_BOUNDS BOUNDS_INCLUSIVE32, 4, 1
CI_BOUNDS BOUNDS64, 8, 0
CI_BOUNDS BOUNDS_INCLUSIVE64, 8, 1
.macro CI_SWITCH name, dense, sign
    CI_LABEL \name
    CI_LOAD_SLOT 8, 8
    mov r10, QWORD PTR [r13 + 16]
    mov r9, QWORD PTR [r10]
    add r10, CI_ASM_switch_data
    .if \dense
        sub rax, QWORD PTR [r10]
        cmp rax, r9
        jae Lci_JUMP
        lea rax, [rax + rax*2]
        lea r11, [r10 + rax*8]
    .else
        xor r8d, r8d
Lci_search_\@:
        cmp r8, r9
        jae Lci_JUMP
        lea rcx, [r8 + r9]
        shr rcx, 1
        lea r11, [rcx + rcx*2]
        lea r11, [r10 + r11*8]
        cmp QWORD PTR [r11 + 8], rax
        .if \sign
            jl Lci_right_\@
        .else
            jb Lci_right_\@
        .endif
        cmp QWORD PTR [r11], rax
        .if \sign
            jg Lci_left_\@
        .else
            ja Lci_left_\@
        .endif
        jmp Lci_found_\@
Lci_right_\@:
        lea r8, [rcx + 1]
        jmp Lci_search_\@
Lci_left_\@:
        mov r9, rcx
        jmp Lci_search_\@
    .endif
Lci_found_\@:
    movsxd rax, DWORD PTR [r11 + CI_ASM_entry_target]
    shl rax, 5
    add r13, rax
    CI_DISPATCH
.endm
CI_SWITCH SWITCH_DENSE, 1, 0
CI_SWITCH SWITCH_SIGNED, 0, 1
CI_SWITCH SWITCH_UNSIGNED, 0, 0
// Wide numeric helpers.
.set CI_TYPE_F16, 16
.set CI_TYPE_BF16, 17
.set CI_TYPE_F32, 32
.set CI_TYPE_F64, 64
.set CI_TYPE_F80, 80
.set CI_TYPE_F128, 128
.set CI_TYPE_S32, 1032
.set CI_TYPE_U32, 2032
.set CI_TYPE_S64, 1064
.set CI_TYPE_U64, 2064
.set CI_TYPE_S128, 1128
.set CI_TYPE_U128, 2128
.set CI_TYPE_I8, 3008
.set CI_TYPE_I16, 3016
.set CI_TYPE_I32, 3032
.set CI_TYPE_I64, 3064
.set CI_TYPE_I128, 3128
.macro CI_SOFT_BEGIN result
    .set ci_gp, 0
    .set ci_fp, 0
    .set ci_stack, 0
.endm
.macro CI_SOFT_GP value=rax
    .if ci_gp == 0
        mov rdi, \value
    .elseif ci_gp == 1
        mov rsi, \value
    .elseif ci_gp == 2
        mov rdx, \value
    .elseif ci_gp == 3
        mov rcx, \value
    .elseif ci_gp == 4
        mov r8, \value
    .else
        mov r9, \value
    .endif
    .set ci_gp, ci_gp + 1
.endm
.macro CI_SOFT_IMM value
    .set ci_imm, (\value) & 1
    CI_SOFT_GP ci_imm
.endm
.macro CI_SOFT_ARG type, offset, wide=0, direct=0
    .if \direct
        lea r10, [rsp + \direct]
    .else
        CI_SLOT r10, r10d, \offset
    .endif
    .if \type == 80
        // x87 arguments are passed on the stack; copy just the ten value bytes.
        mov rax, QWORD PTR [r10]
        mov QWORD PTR [rsp + ci_stack], rax
        movzx eax, WORD PTR [r10 + 8]
        mov QWORD PTR [rsp + ci_stack + 8], rax
        .set ci_stack, ci_stack + 16
        .if ci_stack > 32
            .error "numeric helper stack arguments overlap conversion scratch"
        .endif
#ifdef __SIZEOF_FLOAT128__
    .elseif \type == 128
        .if ci_fp == 0
            movdqu xmm0, XMMWORD PTR [r10]
        .else
            movdqu xmm1, XMMWORD PTR [r10]
        .endif
        .set ci_fp, ci_fp + 1
#endif
    .elseif \wide && (\type % 1000) < 128
        CI_READ ((\type % 1000)/8), (\type < 2000), r10
        CI_SOFT_GP
        .if \type < 2000
            sar rax, 63
            CI_SOFT_GP
        .else
            CI_SOFT_GP 0
        .endif
    .elseif (\type > 1000 && (\type % 1000) == 128) || \type == 128
        CI_SOFT_GP [r10]
        CI_SOFT_GP "[r10 + 8]"
    .elseif \type == 32 || \type == 64
        .if \type == 32
            movss xmm0, DWORD PTR [r10]
        .else
            movsd xmm0, QWORD PTR [r10]
        .endif
        .set ci_fp, ci_fp + 1
    .else
        .if \type < 1000
            CI_READ 2, 0, r10
        .else
            CI_READ ((\type % 1000)/8), (\type < 2000), r10
        .endif
        CI_SOFT_GP
    .endif
.endm
.macro CI_SOFT_RESULT type, offset, size_offset=12
    CI_SLOT r10, r10d, \offset
    .if \type == 16 || \type == 17
        mov WORD PTR [r10], ax
    .elseif \type == 32
        movss DWORD PTR [r10], xmm0
    .elseif \type == 64
        movsd QWORD PTR [r10], xmm0
    .elseif \type == 80
        fstp TBYTE PTR [r10]
        mov ecx, 10
        mov edx, DWORD PTR [r13 + \size_offset]
        jmp Lci_numeric_padding
    .elseif \type == 128
#ifdef __SIZEOF_FLOAT128__
        movdqu XMMWORD PTR [r10], xmm0
#else
        mov QWORD PTR [r10], rax
        mov QWORD PTR [r10 + 8], rdx
#endif
        mov ecx, 16
        mov edx, DWORD PTR [r13 + \size_offset]
        jmp Lci_numeric_padding
    .else
        .if (\type % 1000) == 128
            mov QWORD PTR [r10], rax
            mov QWORD PTR [r10 + 8], rdx
        .else
            CI_STORE ((\type % 1000)/8)
        .endif
    .endif
.endm
Lci_numeric_padding:
    cmp ecx, edx
    jae Lci_next
    mov BYTE PTR [r10 + rcx], 0
    inc ecx
    jmp Lci_numeric_padding

.macro CI_WIDE_DIV name, helper
    CI_LABEL \name
    CI_SOFT_BEGIN 3128
    CI_SOFT_ARG 3128, 8
    CI_SOFT_ARG 3128, 12
    CI_HELPER \helper
    CI_SOFT_RESULT 3128, 4
    CI_NEXT
.endm
CI_WIDE_DIV UDIV128, uint128_div
CI_WIDE_DIV UMOD128, uint128_mod
CI_WIDE_DIV SDIV128, int128_div
CI_WIDE_DIV SMOD128, int128_mod

.macro CI_SOFT_BINARY name, type, helper, compare=0, reverse=0, invert=0
    CI_LABEL \name
    .if \compare
        CI_SOFT_BEGIN 3008
    .else
        CI_SOFT_BEGIN \type
    .endif
    CI_SOFT_ARG \type, (12 + \reverse*4)
    CI_SOFT_ARG \type, (16 - \reverse*4)
    .if \type == 16 || \type == 17
        CI_SOFT_IMM (\type == 17)
    .endif
    CI_HELPER \helper
    .if \compare
        .if \invert
            xor eax, 1
        .endif
        CI_SLOT r10, r10d, 4
        mov BYTE PTR [r10], al
    .else
        CI_SOFT_RESULT \type, 4, 8
    .endif
    .if \compare || (\type != 80 && \type != 128)
        CI_NEXT
    .endif
.endm
.macro CI_SOFT_FAMILY suffix, type, helper
    CI_SOFT_BINARY FADD\suffix, \type, \helper\()_add
    CI_SOFT_BINARY FSUB\suffix, \type, \helper\()_sub
    CI_SOFT_BINARY FMUL\suffix, \type, \helper\()_mul
    CI_SOFT_BINARY FDIV\suffix, \type, \helper\()_div
    CI_SOFT_BINARY FEQ\suffix, \type, \helper\()_eq, 1
    CI_SOFT_BINARY FNE\suffix, \type, \helper\()_eq, 1, 0, 1
    CI_SOFT_BINARY FLT\suffix, \type, \helper\()_lt, 1
    CI_SOFT_BINARY FGT\suffix, \type, \helper\()_lt, 1, 1
    CI_SOFT_BINARY FLE\suffix, \type, \helper\()_le, 1
    CI_SOFT_BINARY FGE\suffix, \type, \helper\()_le, 1, 1
.endm
CI_SOFT_FAMILY 16, 16, float16
CI_SOFT_FAMILY _BF16, 17, float16
CI_SOFT_FAMILY 80, 80, float80
CI_SOFT_FAMILY 128, 128, float128
.macro CI_FNEG name, bytes
    CI_LABEL \name
    CI_SLOT rax, eax, 12
    CI_SLOT r10, r10d, 4
    .if \bytes <= 8
        CI_READ \bytes
        btc rax, (\bytes*8 - 1)
        CI_STORE \bytes
    .else
        mov rdx, QWORD PTR [rax]
        mov QWORD PTR [r10], rdx
        .if \bytes == 10
            movzx eax, WORD PTR [rax + 8]
            xor eax, 0x8000
            mov WORD PTR [r10 + 8], ax
        .else
            mov rax, QWORD PTR [rax + 8]
            btc rax, 63
            mov QWORD PTR [r10 + 8], rax
        .endif
        mov ecx, \bytes
        mov edx, DWORD PTR [r13 + 8]
        jmp Lci_numeric_padding
        .exitm
    .endif
    CI_NEXT
.endm
CI_FNEG FNEG16, 2
CI_FNEG FNEG_BF16, 2
CI_FNEG FNEG32, 4
CI_FNEG FNEG64, 8
CI_FNEG FNEG80, 10
CI_FNEG FNEG128, 16

.macro CI_ITOF name, source, dest
    CI_LABEL \name
    .set ci_unsigned, (\source >= 2000)
    .set ci_bits, (\source % 1000)
    .if (\dest == 32 || \dest == 64) && ci_bits < 128
        CI_LOAD_SLOT 16, (ci_bits/8), !ci_unsigned
        .if ci_unsigned && ci_bits == 64
            test rax, rax
            js Lci_utof_high_\@
        .endif
        .if \dest == 32
            cvtsi2ss xmm0, rax
        .else
            cvtsi2sd xmm0, rax
        .endif
        .if ci_unsigned && ci_bits == 64
            jmp Lci_utof_done_\@
Lci_utof_high_\@:
            mov rcx, rax
            and ecx, 1
            shr rax, 1
            or rax, rcx
            .if \dest == 32
                cvtsi2ss xmm0, rax
                addss xmm0, xmm0
            .else
                cvtsi2sd xmm0, rax
                addsd xmm0, xmm0
            .endif
Lci_utof_done_\@:
        .endif
    .else
        CI_SOFT_BEGIN \dest
        .if \dest == 16 || \dest == 17
            CI_SOFT_ARG \source, 16
            .if ci_bits == 128
                CI_SOFT_IMM ci_unsigned
                CI_SOFT_IMM (\dest == 17)
                CI_HELPER float16_from_uint128
            .else
                CI_SOFT_IMM (\dest == 17)
                .if ci_bits == 32
                    .if ci_unsigned
                        CI_HELPER float16_from_uint32
                    .else
                        CI_HELPER float16_from_int32
                    .endif
                .else
                    .if ci_unsigned
                        CI_HELPER float16_from_uint64
                    .else
                        CI_HELPER float16_from_int64
                    .endif
                .endif
            .endif
        .else
            CI_SOFT_ARG \source, 16, 1
            CI_SOFT_IMM ci_unsigned
            .if \dest == 32
                CI_HELPER uint128_to_float
            .elseif \dest == 64
                CI_HELPER uint128_to_double
            .elseif \dest == 80
                CI_HELPER float80_from_uint128
            .else
                CI_HELPER float128_from_uint128
            .endif
        .endif
    .endif
    CI_SOFT_RESULT \dest, 8
    .if \dest != 80 && \dest != 128
        CI_NEXT
    .endif
.endm
#define CI_ITOF_OP(name, source, dest) CI_ITOF name, CI_TYPE_##source, CI_TYPE_##dest ;
CI_ITOF_OPS(CI_ITOF_OP)
#undef CI_ITOF_OP

.macro CI_FTOI name, source, dest, bytes
    CI_LABEL \name
    .if (\source == 32 || \source == 64) && \bytes <= 8
        mov eax, DWORD PTR [r13 + 16]
        .if \dest >= 2000 && \bytes == 8
            .if \source == 32
                cvtss2sd xmm0, DWORD PTR [r12 + rax]
            .else
                movsd xmm0, QWORD PTR [r12 + rax]
            .endif
            mov rcx, 0x43e0000000000000
            movq xmm1, rcx
            ucomisd xmm0, xmm1
            jb Lci_ftoi_low_\@
            subsd xmm0, xmm1
            cvttsd2si rax, xmm0
            btc rax, 63
            jmp Lci_ftoi_done_\@
Lci_ftoi_low_\@:
            cvttsd2si rax, xmm0
        .else
            .if \source == 32
                cvttss2si rax, DWORD PTR [r12 + rax]
            .else
                cvttsd2si rax, QWORD PTR [r12 + rax]
            .endif
        .endif
Lci_ftoi_done_\@:
        mov r10d, DWORD PTR [r13 + 8]
        CI_STORE \bytes, r12+r10
    .else
        .if \source == 16 || \source == 17
            CI_SOFT_BEGIN \dest
            CI_SOFT_ARG \source, 16
            CI_SOFT_IMM (\source == 17)
            .if \bytes == 16
                CI_HELPER float16_to_uint128
            .elseif \bytes == 4
                .if \dest >= 2000
                    CI_HELPER float16_to_uint32
                .else
                    CI_HELPER float16_to_int32
                .endif
            .else
                .if \dest >= 2000
                    CI_HELPER float16_to_uint64
                .else
                    CI_HELPER float16_to_int64
                .endif
            .endif
            CI_SOFT_RESULT \dest, 8
        .else
            CI_SOFT_BEGIN 3128
            .if \source == 32
                mov eax, DWORD PTR [r13 + 16]
                cvtss2sd xmm4, DWORD PTR [r12 + rax]
                movsd QWORD PTR [rsp + 32], xmm4
                CI_SOFT_ARG 64, 16, 0, 32
            .else
                CI_SOFT_ARG \source, 16
            .endif
            .if \source == 80
                CI_HELPER float80_to_uint128
            .elseif \source == 128
                CI_HELPER float128_to_uint128
            .else
                CI_SOFT_IMM (\dest >= 2000)
                CI_HELPER uint128_from_double
            .endif
            mov r10d, DWORD PTR [r13 + 8]
            CI_STORE \bytes, r12+r10
            .if \bytes == 16
                mov QWORD PTR [r12 + r10 + 8], rdx
            .endif
        .endif
    .endif
    CI_NEXT
.endm
#define CI_FTOI_OP(name, source, dest, bytes) CI_FTOI name, CI_TYPE_##source, CI_TYPE_##dest, bytes ;
CI_FTOI_OPS(CI_FTOI_OP)
#undef CI_FTOI_OP

.macro CI_FTOF name, source, dest
    CI_LABEL \name
    .if (\source == 32 && \dest == 64) || (\source == 64 && \dest == 32)
        mov eax, DWORD PTR [r13 + 16]
        .if \dest == 32
            cvtsd2ss xmm0, QWORD PTR [r12 + rax]
        .else
            cvtss2sd xmm0, DWORD PTR [r12 + rax]
        .endif
    .elseif (\source == 16 || \source == 17) && (\dest == 16 || \dest == 17)
        CI_SOFT_BEGIN 32
        CI_SOFT_ARG \source, 16
        CI_SOFT_IMM (\source == 17)
        CI_HELPER float16_to_float
        CI_SOFT_BEGIN \dest
        CI_SOFT_IMM (\dest == 17)
        CI_HELPER float16_from_float
    .else
        CI_SOFT_BEGIN \dest
        CI_SOFT_ARG \source, 16
        .if \source == 16 || \source == 17
            CI_SOFT_IMM (\source == 17)
            .if \dest == 32
                CI_HELPER float16_to_float
            .elseif \dest == 64
                CI_HELPER float16_to_double
            .elseif \dest == 80
                CI_HELPER float16_to_float80
            .else
                CI_HELPER float16_to_quad
            .endif
        .elseif \dest == 16 || \dest == 17
            CI_SOFT_IMM (\dest == 17)
            .if \source == 32
                CI_HELPER float16_from_float
            .elseif \source == 64
                CI_HELPER float16_from_double
            .elseif \source == 80
                CI_HELPER float16_from_float80
            .else
                CI_HELPER float16_from_quad
            .endif
        .elseif \source == 32
            .if \dest == 80
                CI_HELPER float80_from_float
            .else
                CI_HELPER float128_from_float
            .endif
        .elseif \source == 64
            .if \dest == 80
                CI_HELPER float80_from_double
            .else
                CI_HELPER float128_from_double
            .endif
        .elseif \source == 80
            .if \dest == 32
                CI_HELPER float80_to_float
            .elseif \dest == 64
                CI_HELPER float80_to_double
            .else
                CI_HELPER float128_from_float80
            .endif
        .else
            .if \dest == 32
                CI_HELPER float128_to_float
            .elseif \dest == 64
                CI_HELPER float128_to_double
            .else
                CI_HELPER float80_from_float128
            .endif
        .endif
    .endif
    CI_SOFT_RESULT \dest, 8
    .if \dest != 80 && \dest != 128
        CI_NEXT
    .endif
.endm
#define CI_FTOF_OP(name, source, dest) CI_FTOF name, CI_TYPE_##source, CI_TYPE_##dest ;
CI_FTOF_OPS(CI_FTOF_OP)
#undef CI_FTOF_OP
// Keep the va_list cursor in RAX and the destination pointer pair in R10.
.macro CI_VA_BEGIN
    CI_LOAD_SLOT 12, 8
    CI_SLOT r10, r10d, 4
.endm
.macro CI_VA_STACK indirect=0, skip=0
    mov ecx, DWORD PTR [r13 + 16]
    mov rdx, QWORD PTR [rax]
    add rdx, rcx
    not rcx
    and rdx, rcx
    mov ecx, DWORD PTR [r13 + 20]
    add rcx, rdx
    mov QWORD PTR [rax], rcx
    .if \indirect
        mov rdx, QWORD PTR [rdx]
    .endif
    mov QWORD PTR [r10], rdx
    .if \skip
        movzx eax, BYTE PTR [r13 + 2]
        add eax, 2
        shl eax, 5
        add r13, rax
        CI_DISPATCH
    .else
        CI_NEXT
    .endif
.endm
CI_LABEL VA_ARG_STACK
    CI_VA_BEGIN
    CI_VA_STACK
CI_LABEL VA_ARG_ARM_STACK
    CI_VA_BEGIN
    CI_VA_STACK
CI_LABEL VA_ARG_STACK_INDIRECT
    CI_VA_BEGIN
    CI_VA_STACK 1
CI_LABEL VA_ARG_SYSV_STACK
    CI_VA_BEGIN
    add rax, 8
    CI_VA_STACK
CI_LABEL VA_ARG_SYSV
    CI_VA_BEGIN
    movzx r8d, BYTE PTR [r13 + 3]
    mov r9d, r8d
    and r8d, 15
    shr r9d, 4
    shl r8d, 3
    shl r9d, 4
    mov ecx, DWORD PTR [rax]
    mov edx, DWORD PTR [rax + 4]
    add r8d, ecx
    add r9d, edx
    cmp r8d, 48
    ja Lci_va_sysv_stack
    cmp r9d, 176
    ja Lci_va_sysv_stack
    mov DWORD PTR [rax], r8d
    mov DWORD PTR [rax + 4], r9d
    mov rax, QWORD PTR [rax + 16]
    add rcx, rax
    add rdx, rax
    mov QWORD PTR [r10], rcx
    mov QWORD PTR [r10 + 8], rdx
    CI_NEXT
Lci_va_sysv_stack:
    add rax, 8
    CI_VA_STACK 0, 1
.macro CI_VA_ARM name, align16=0
    CI_LABEL \name
    CI_VA_BEGIN
    movzx r8d, BYTE PTR [r13 + 3]
    mov r9d, r8d
    and r8d, 15
    shr r9d, 4
    shl r8d, 3
    shl r9d, 4
    movsxd rcx, DWORD PTR [rax + 24]
    movsxd rdx, DWORD PTR [rax + 28]
    .if \align16
        add rcx, 15
        and rcx, -16
    .endif
    test r8d, r8d
    jz 1f
    test ecx, ecx
    jns 5f
    add r8d, ecx
    jg 5f
1:
    test r9d, r9d
    jz 2f
    test edx, edx
    jns 5f
    add r9d, edx
    jg 5f
2:
    test BYTE PTR [r13 + 3], 15
    jz 3f
    mov DWORD PTR [rax + 24], r8d
    add rcx, QWORD PTR [rax + 8]
    jmp 4f
3:
    xor ecx, ecx
4:
    mov QWORD PTR [r10], rcx
    test BYTE PTR [r13 + 3], 240
    jz 6f
    mov DWORD PTR [rax + 28], r9d
    add rdx, QWORD PTR [rax + 16]
    jmp 7f
6:
    xor edx, edx
7:
    mov QWORD PTR [r10 + 8], rdx
    CI_NEXT
5:
    test BYTE PTR [r13 + 3], 15
    jz 8f
    mov DWORD PTR [rax + 24], 0
8:
    test BYTE PTR [r13 + 3], 240
    jz 9f
    mov DWORD PTR [rax + 28], 0
9:
    CI_VA_STACK 0, 1
.endm
CI_VA_ARM VA_ARG_ARM
CI_VA_ARM VA_ARG_ARM_ALIGN16, 1
CI_LABEL VA_ARG_ARM_INDIRECT
    CI_VA_BEGIN
    movsxd rcx, DWORD PTR [rax + 24]
    cmp ecx, -8
    jg Lci_va_arm_indirect_stack
    mov rdx, QWORD PTR [rax + 8]
    add rdx, rcx
    add ecx, 8
    mov DWORD PTR [rax + 24], ecx
    mov rdx, QWORD PTR [rdx]
    mov QWORD PTR [r10], rdx
    CI_NEXT
Lci_va_arm_indirect_stack:
    mov DWORD PTR [rax + 24], 0
    mov rdx, QWORD PTR [rax]
    add rdx, 7
    and rdx, -8
    lea rcx, [rdx + 8]
    mov QWORD PTR [rax], rcx
    mov rdx, QWORD PTR [rdx]
    mov QWORD PTR [r10], rdx
    CI_NEXT

.cfi_endproc
#ifdef __linux__
.size ci_interp_execute_asm, .-ci_interp_execute_asm
#endif

// Callback: r10 = thunk metadata; [r10] = CiClosureData.
// PC 2 skips FRAME_SAVE/FRAME_ENTER, performed here before execute entry.
.p2align 4
.globl CI_SYMBOL(ci_interp_callback_asm)
#ifdef __linux__
.hidden ci_interp_callback_asm
.type ci_interp_callback_asm, @function
#endif
CI_SYMBOL(ci_interp_callback_asm):
.cfi_startproc
    endbr64
    push rbx
    .cfi_def_cfa_offset 16
    .cfi_offset rbx, -16
    push r12
    .cfi_def_cfa_offset 24
    .cfi_offset r12, -24
    push r13
    .cfi_def_cfa_offset 32
    .cfi_offset r13, -32
    push r15
    .cfi_def_cfa_offset 40
    .cfi_offset r15, -40
    sub rsp, 440
    .cfi_def_cfa_offset 480
    mov QWORD PTR [rsp + 64], rdi
    mov QWORD PTR [rsp + 72], rsi
    mov QWORD PTR [rsp + 80], rdx
    mov QWORD PTR [rsp + 88], rcx
    mov QWORD PTR [rsp + 96], r8
    mov QWORD PTR [rsp + 104], r9
    mov QWORD PTR [rsp + 240], rax
    lea rax, [rsp + 480]
    mov QWORD PTR [rsp + 248], rax
    movdqu XMMWORD PTR [rsp + 112], xmm0
    movdqu XMMWORD PTR [rsp + 128], xmm1
    movdqu XMMWORD PTR [rsp + 144], xmm2
    movdqu XMMWORD PTR [rsp + 160], xmm3
    movdqu XMMWORD PTR [rsp + 176], xmm4
    movdqu XMMWORD PTR [rsp + 192], xmm5
    movdqu XMMWORD PTR [rsp + 208], xmm6
    movdqu XMMWORD PTR [rsp + 224], xmm7
    pxor xmm0, xmm0
    .irp offset, 256, 272, 288, 304, 320, 336, 352, 368
        movdqu XMMWORD PTR [rsp + \offset], xmm0
    .endr
    mov QWORD PTR [rsp + 400], 0
    mov rbx, QWORD PTR [r10]
    mov r12, QWORD PTR [rbx + 16]
    mov r13, QWORD PTR [rbx + 8]
    mov ecx, 33
Lcb_resolve:
    mov rax, QWORD PTR [r13 + CI_ASM_func_hotswap]
    test rax, rax
    jz Lcb_resolved
    mov r13, rax
    dec ecx
    jnz Lcb_resolve
    mov rdi, QWORD PTR [rbx]
    mov rsi, QWORD PTR [r13 + 48]
    lea rdx, [rip + Lci_msg_hotswap]
    xor eax, eax
    call QWORD PTR [r12 + CI_ASM_HELPER_error*8]
    jmp Lcb_export
Lcb_resolved:
    mov r13, QWORD PTR [r13 + CI_ASM_func_code]
    mov rax, QWORD PTR [r13]
    mov edx, DWORD PTR [rax + 16]
    shr edx, 7
    and edx, 7
    xor ecx, ecx
    xor r8d, r8d
1:
    cmp ecx, edx
    jae 2f
    movzx r9d, WORD PTR [rax + rcx*2 + 20]
    and r9d, 0x1800
    cmp r9d, 0x1000
    sete r9b
    movzx r9d, r9b
    add r8d, r9d
    inc ecx
    jmp 1b
2:
    mov QWORD PTR [rsp + 400], r8
    mov r13, QWORD PTR [r13 + 8]
    mov rdi, QWORD PTR [rbx]
    call QWORD PTR [r12 + CI_ASM_HELPER_callback_stack*8]
    test rax, rax
    jz Lcb_export
    mov r15, rax
    .irp offset, 0, 16, 32, 48, 64, 80, 96, 112, 128, 144, 160, 176, 192, 208, 224, 240, 256, 272, 288, 304
        movdqu xmm0, XMMWORD PTR [rsp + 64 + \offset]
        movdqu XMMWORD PTR [r15 + \offset], xmm0
    .endr
    mov rdi, QWORD PTR [rbx]
    mov rsi, r15
    xor edx, edx
    mov rcx, QWORD PTR [r13 + 8]
    mov r8d, DWORD PTR [r13 + 16]
    lea r9, [rsp + 384]
    call QWORD PTR [r12 + CI_ASM_HELPER_new_frame*8]
    test eax, eax
    jz Lcb_frame
    CI_STACK_RESTORE 1
    jmp Lcb_export
Lcb_frame:
    mov r10, QWORD PTR [rsp + 384]
    mov QWORD PTR [r10 + CI_ASM_code], r13
    mov QWORD PTR [r10 + CI_ASM_pc], 2
    mov eax, DWORD PTR [r13 + CI_ASM_code_ops + 32 + 4]
    cmp eax, -1
    je 1f
    lea rax, [r10 + rax + CI_ASM_frame_size]
    jmp 2f
1:
    xor eax, eax
2:
    mov QWORD PTR [r10 + CI_ASM_incoming], rax
    mov rdi, QWORD PTR [rbx]
    mov rsi, r10
    mov rdx, r12
    mov rcx, r15
    xor r8d, r8d
    xor r9d, r9d
    mov rax, QWORD PTR [rbx + 24]
    mov QWORD PTR [rsp], rax
    call CI_SYMBOL(ci_interp_execute_asm)
    test eax, eax
    jnz Lcb_export
    .irp offset, 192, 208, 224, 240, 256
        movdqu xmm0, XMMWORD PTR [r15 + \offset]
        movdqu XMMWORD PTR [rsp + 64 + \offset], xmm0
    .endr
Lcb_export:
    mov rax, QWORD PTR [rsp + 256]
    mov rdx, QWORD PTR [rsp + 264]
    movdqu xmm0, XMMWORD PTR [rsp + 272]
    movdqu xmm1, XMMWORD PTR [rsp + 288]
    cmp QWORD PTR [rsp + 400], 0
    je 2f
    cmp QWORD PTR [rsp + 400], 2
    jne 1f
    fld TBYTE PTR [rsp + 320]
1:
    fld TBYTE PTR [rsp + 304]
2:
    add rsp, 440
    .cfi_def_cfa_offset 40
    pop r15
    .cfi_def_cfa_offset 32
    .cfi_restore r15
    pop r13
    .cfi_def_cfa_offset 24
    .cfi_restore r13
    pop r12
    .cfi_def_cfa_offset 16
    .cfi_restore r12
    pop rbx
    .cfi_def_cfa_offset 8
    .cfi_restore rbx
    ret
.cfi_endproc
#ifdef __linux__
.size ci_interp_callback_asm, .-ci_interp_callback_asm
#endif

.p2align 2
Lci_dispatch_table:
.macro CI_ENTRY name
    .ifndef Lci_handler_\name
        .error "missing assembly opcode handler: \name"
    .endif
    .long Lci_handler_\name - Lci_dispatch_table
.endm
#define CI_NATIVE_ENTRY(name, suffix) CI_ENTRY name##suffix ;
CI_OPCODE_NATIVE_LIST(CI_NATIVE_ENTRY)
#undef CI_NATIVE_ENTRY
#define CI_FLAT_ENTRY(name, ...) CI_ENTRY name ;
CI_OPCODE_FLAT_LIST(CI_FLAT_ENTRY)
#undef CI_FLAT_ENTRY

Lci_msg_null_call:
    .asciz "call through null function pointer"
Lci_msg_hotswap:
    .asciz "hotswap cycle detected"
Lci_msg_string:
    .asciz "%s"
Lci_msg_unknown:
    .asciz "<unknown>"
Lci_msg_missing_function:
    .asciz "function '%s' not resolved before execution"
Lci_msg_result_size:
    .asciz "result buffer too small (need %zu, have %zu)"
Lci_msg_capture_null:
    .asciz "__builtin_capture_backtrace buffer must not be NULL"
Lci_msg_clear_cache:
    .asciz "__builtin___clear_cache failed"
Lci_msg_trap:
    .asciz "__builtin_trap"
Lci_msg_unreachable:
    .asciz "__builtin_unreachable reached"
Lci_msg_abort:
    .asciz "__builtin_abort called"
Lci_msg_close_inclusive:
    .asciz "]"
Lci_msg_close_exclusive:
    .asciz ")"
Lci_msg_bounds_unsigned:
    .asciz "array subscript out of bounds: index %llu not in [0, %llu%s"
Lci_msg_bounds_signed:
    .asciz "array subscript out of bounds: index %lld not in [0, %llu%s"
#if defined __linux__
.section .note.GNU-stack,"",@progbits
#endif
