; Copyright © 2026-2026, David Priver <david@davidpriver.com>
; Windows x64 threaded interpreter.
; Requires x86-64-v3.
; R12: slots
; R13: bytecode PC
; R14: relative dispatch table
; R15: CiStack.
; RBX: execution record
; RBP: fixed unwind anchor.
IFNDEF CI_LLVM_ML
    OPTION CASEMAP:NONE
    OPTION NOSCOPED
ENDIF

CI_ENDBR MACRO
    DB 0F3h, 00Fh, 01Eh, 0FAh
ENDM

; Explicit RIP-relative operands also support the LLVM MASM assembler.
CI_REG_r8 EQU 8
CI_REG_r9 EQU 9
CI_REG_r10 EQU 10
CI_REG_r14 EQU 14
CI_LEA MACRO p_register, p_target
    LOCAL displacement
    DB 048h + (CI_REG_&p_register / 8) * 4, 08Dh
    DB 5 + (CI_REG_&p_register AND 7) * 8
displacement:
    DD p_target - displacement - 4
ENDM

CI_EXECUTE_CALL MACRO
    LOCAL displacement
    DB 0E8h
displacement:
    DD Lci_execute_entry - displacement - 4
ENDM

CI_ASM_code EQU 16
CI_ASM_code_ops EQU 32
CI_ASM_code_count EQU 24
CI_ASM_pc EQU 8
CI_ASM_incoming EQU 24
CI_ASM_parent EQU 0
CI_ASM_frame_size EQU 32
CI_ASM_stack_top EQU 344
CI_ASM_stack_marks EQU 360
CI_ASM_stack_mark_count EQU 368
CI_ASM_stack_mark_capacity EQU 376
CI_ASM_stack_boundary EQU 384
CI_ASM_var_value EQU 64
CI_ASM_native_func EQU 176
CI_ASM_func_flags EQU 56
CI_ASM_atom_data EQU 8
CI_ASM_switch_count EQU 0
CI_ASM_switch_data EQU 8
CI_ASM_entry_size EQU 24
CI_ASM_entry_target EQU 16
CI_ASM_op_count EQU 1152
CI_ASM_func_code EQU 80
CI_ASM_func_hotswap EQU 168
CI_ASM_plan_stack_size EQU 48
CI_ASM_plan_stack_alignment EQU 72

CI_ASM_HELPER_float16_add EQU 0
CI_ASM_HELPER_float16_sub EQU 1
CI_ASM_HELPER_float16_mul EQU 2
CI_ASM_HELPER_float16_div EQU 3
CI_ASM_HELPER_float16_eq EQU 4
CI_ASM_HELPER_float16_lt EQU 5
CI_ASM_HELPER_float16_le EQU 6
CI_ASM_HELPER_float80_add EQU 7
CI_ASM_HELPER_float80_sub EQU 8
CI_ASM_HELPER_float80_mul EQU 9
CI_ASM_HELPER_float80_div EQU 10
CI_ASM_HELPER_float80_eq EQU 11
CI_ASM_HELPER_float80_lt EQU 12
CI_ASM_HELPER_float80_le EQU 13
CI_ASM_HELPER_float128_add EQU 14
CI_ASM_HELPER_float128_sub EQU 15
CI_ASM_HELPER_float128_mul EQU 16
CI_ASM_HELPER_float128_div EQU 17
CI_ASM_HELPER_float128_eq EQU 18
CI_ASM_HELPER_float128_lt EQU 19
CI_ASM_HELPER_float128_le EQU 20
CI_ASM_HELPER_uint128_div EQU 21
CI_ASM_HELPER_uint128_mod EQU 22
CI_ASM_HELPER_int128_div EQU 23
CI_ASM_HELPER_int128_mod EQU 24
CI_ASM_HELPER_float16_from_int32 EQU 25
CI_ASM_HELPER_float16_from_uint32 EQU 26
CI_ASM_HELPER_float16_from_int64 EQU 27
CI_ASM_HELPER_float16_from_uint64 EQU 28
CI_ASM_HELPER_float16_from_uint128 EQU 29
CI_ASM_HELPER_uint128_to_float EQU 30
CI_ASM_HELPER_uint128_to_double EQU 31
CI_ASM_HELPER_float80_from_uint128 EQU 32
CI_ASM_HELPER_float128_from_uint128 EQU 33
CI_ASM_HELPER_float16_to_int32 EQU 34
CI_ASM_HELPER_float16_to_uint32 EQU 35
CI_ASM_HELPER_float16_to_int64 EQU 36
CI_ASM_HELPER_float16_to_uint64 EQU 37
CI_ASM_HELPER_float16_to_uint128 EQU 38
CI_ASM_HELPER_uint128_from_double EQU 39
CI_ASM_HELPER_float80_to_uint128 EQU 40
CI_ASM_HELPER_float128_to_uint128 EQU 41
CI_ASM_HELPER_float16_to_float EQU 42
CI_ASM_HELPER_float16_from_float EQU 43
CI_ASM_HELPER_float16_to_double EQU 44
CI_ASM_HELPER_float16_to_float80 EQU 45
CI_ASM_HELPER_float16_to_quad EQU 46
CI_ASM_HELPER_float80_from_float EQU 47
CI_ASM_HELPER_float128_from_float EQU 48
CI_ASM_HELPER_float16_from_double EQU 49
CI_ASM_HELPER_float80_from_double EQU 50
CI_ASM_HELPER_float128_from_double EQU 51
CI_ASM_HELPER_float16_from_float80 EQU 52
CI_ASM_HELPER_float80_to_float EQU 53
CI_ASM_HELPER_float80_to_double EQU 54
CI_ASM_HELPER_float128_from_float80 EQU 55
CI_ASM_HELPER_float16_from_quad EQU 56
CI_ASM_HELPER_float128_to_float EQU 57
CI_ASM_HELPER_float128_to_double EQU 58
CI_ASM_HELPER_float80_from_float128 EQU 59
CI_ASM_HELPER_backtrace EQU 60
CI_ASM_HELPER_cyclecounter EQU 61
CI_ASM_HELPER_closure_lookup EQU 62
CI_ASM_HELPER_new_frame EQU 63
CI_ASM_HELPER_stack_save EQU 64
CI_ASM_HELPER_callback_stack EQU 65
CI_ASM_HELPER_ensure_var EQU 66
CI_ASM_HELPER_tls_address EQU 67
CI_ASM_HELPER_stack_alloc EQU 68
CI_ASM_HELPER_stack_zalloc EQU 69
CI_ASM_HELPER_error EQU 70
CI_ASM_HELPER_clear_cache EQU 71
CI_ASM_HELPER_memmove EQU 72
CI_ASM_HELPER_memset EQU 73
CI_ASM_HELPER_crc EQU 74

CI_OP_FRAME_SAVE EQU 232
CI_SCRATCH_SIZE EQU 80
CI_LOCAL_SIZE EQU CI_SCRATCH_SIZE + 64 + 8
CI_FRAME_OFFSET EQU 0
CI_ARGS_OFFSET EQU 48
CI_ARGS_SIZE EQU 144


.code

; ABI scratch: RAX, R10, R11, XMM4.
;
; Scratch peak: 32-byte home area + two 16-byte arguments + 16-byte result.
; RBX follows scratch: ci, root, frame, helpers, result, size, target, flags.
; Eight trailing bytes align RSP after the six saved registers.
CI_ABI_TMP TEXTEQU <xmm4>
CI_GP_COUNT EQU 4
CI_FP_COUNT EQU 4
CI_FP_BASE EQU 32

CI_GP_APPLY MACRO p_register, p_action, p_operand, p_reverse
    IF p_reverse
        p_action p_operand, p_register
    ELSE
        p_action p_register, p_operand
    ENDIF
ENDM
CI_ARG_GP MACRO p_reg, p_action, p_operand, p_reverse:=<0>
    IF p_reg EQ 0
        CI_GP_APPLY rcx, p_action, p_operand, p_reverse
    ELSEIF p_reg EQ 1
        CI_GP_APPLY rdx, p_action, p_operand, p_reverse
    ELSEIF p_reg EQ 2
        CI_GP_APPLY r8, p_action, p_operand, p_reverse
    ELSEIF p_reg EQ 3
        CI_GP_APPLY r9, p_action, p_operand, p_reverse
    ENDIF
ENDM
CI_ARGS MACRO p_load, p_base, p_offset:=<0>
    IF p_load
        mov rcx, QWORD PTR [p_base + p_offset]
        mov rdx, QWORD PTR [p_base + p_offset + 8]
        mov r8, QWORD PTR [p_base + p_offset + 16]
        mov r9, QWORD PTR [p_base + p_offset + 24]
    ELSE
        mov QWORD PTR [p_base + p_offset], rcx
        mov QWORD PTR [p_base + p_offset + 8], rdx
        mov QWORD PTR [p_base + p_offset + 16], r8
        mov QWORD PTR [p_base + p_offset + 24], r9
    ENDIF
    FOR p_reg, <0, 1, 2, 3>
        IF p_load
            movdqu xmm&p_reg, XMMWORD PTR [p_base + p_offset + 32 + p_reg*16]
        ELSE
            movdqu XMMWORD PTR [p_base + p_offset + 32 + p_reg*16], xmm&p_reg
        ENDIF
    ENDM
ENDM
CI_ARGS_SAVE MACRO
    ; 48 helper-argument bytes + 96 saved GP/FP bytes. Restore preserves EAX.
    sub rsp, CI_ARGS_SIZE
    CI_ARGS 0, rsp, CI_ARGS_OFFSET
ENDM
CI_ARGS_RESTORE MACRO
    CI_ARGS 1, rsp, CI_ARGS_OFFSET
    add rsp, CI_ARGS_SIZE
ENDM
CI_DISPATCH MACRO
    movzx eax, WORD PTR [r13]
    movsxd rax, DWORD PTR [r14 + rax*4]
    add rax, r14
    jmp rax
ENDM

CI_NEXT MACRO
    add r13, 32
    CI_DISPATCH
ENDM

CI_LABEL MACRO p_name
    Lci_handler_&p_name EQU Lci_&p_name
Lci_&p_name:
    CI_ENDBR
ENDM

CI_SLOT MACRO p_reg, p_reg32, p_offset
    mov p_reg32, DWORD PTR [r13 + p_offset]
    add p_reg, r12
ENDM

CI_READ MACRO p_bytes, p_sign:=<0>, p_base:=<rax>
    IF (p_bytes EQ 1)
        IF p_sign
            movsx rax, BYTE PTR [p_base]
        ELSE
            movzx eax, BYTE PTR [p_base]
        ENDIF
    ELSEIF (p_bytes EQ 2)
        IF p_sign
            movsx rax, WORD PTR [p_base]
        ELSE
            movzx eax, WORD PTR [p_base]
        ENDIF
    ELSEIF (p_bytes EQ 4)
        IF p_sign
            movsxd rax, DWORD PTR [p_base]
        ELSE
            mov eax, DWORD PTR [p_base]
        ENDIF
    ELSE
        mov rax, QWORD PTR [p_base]
    ENDIF
ENDM
CI_LOAD_SLOT MACRO p_offset, p_bytes, p_sign:=<0>
    mov eax, DWORD PTR [r13 + p_offset]
    CI_READ p_bytes, p_sign, <r12+rax>
ENDM

CI_STORE MACRO p_bytes, p_base:=<r10>
    IF (p_bytes EQ 1)
        mov BYTE PTR [p_base], al
    ELSEIF (p_bytes EQ 2)
        mov WORD PTR [p_base], ax
    ELSEIF (p_bytes EQ 4)
        mov DWORD PTR [p_base], eax
    ELSE
        mov QWORD PTR [p_base], rax
    ENDIF
ENDM

CI_HELPER MACRO p_name
    mov r11, QWORD PTR [rbx + 24]
    call QWORD PTR [r11 + CI_ASM_HELPER_&p_name*8]
ENDM

CI_PUBLISH_PC MACRO
    mov rax, QWORD PTR [r12 + CI_ASM_code - CI_ASM_frame_size]
    lea rax, [rax + CI_ASM_code_ops]
    mov r10, r13
    sub r10, rax
    shr r10, 5
    mov QWORD PTR [r12 + CI_ASM_pc - CI_ASM_frame_size], r10
ENDM

CI_STACK_RESTORE MACRO p_boundary
    IF p_boundary
        mov rcx, QWORD PTR [r15 + CI_ASM_stack_boundary]
    ELSE
        mov rcx, QWORD PTR [r15 + CI_ASM_stack_mark_count]
    ENDIF
    dec rcx
    mov QWORD PTR [r15 + CI_ASM_stack_mark_count], rcx
    lea rcx, [rcx + rcx*2]
    mov r10, QWORD PTR [r15 + CI_ASM_stack_marks]
    lea r10, [r10 + rcx*8]
    movdqu xmm0, XMMWORD PTR [r10]
    movdqu XMMWORD PTR [r15 + CI_ASM_stack_top], xmm0
    mov rcx, QWORD PTR [r10 + 16]
    mov QWORD PTR [r15 + CI_ASM_stack_boundary], rcx
ENDM
ALIGN 16
PUBLIC ci_interp_execute_asm
ci_interp_execute_asm PROC FRAME
Lci_execute_entry:
    CI_ENDBR
    push rbp
    .pushreg rbp
    push rbx
    .pushreg rbx
    push r12
    .pushreg r12
    push r13
    .pushreg r13
    push r14
    .pushreg r14
    push r15
    .pushreg r15
    sub rsp, CI_LOCAL_SIZE
    .allocstack CI_LOCAL_SIZE
    lea rbp, [rsp + CI_FRAME_OFFSET]
    .setframe rbp, CI_FRAME_OFFSET
    .endprolog
    lea rbx, [rsp + CI_SCRATCH_SIZE]
    mov QWORD PTR [rbx], rcx
    mov QWORD PTR [rbx + 8], rdx
    mov QWORD PTR [rbx + 16], rdx
    mov QWORD PTR [rbx + 24], r8
    mov r15, r9
    mov rax, QWORD PTR [rbp + CI_LOCAL_SIZE + 88]
    mov QWORD PTR [rbx + 32], rax
    mov rax, QWORD PTR [rbp + CI_LOCAL_SIZE + 96]
    mov QWORD PTR [rbx + 40], rax
CI_LEA r14, Lci_dispatch_table
    mov rax, QWORD PTR [rbp + CI_LOCAL_SIZE + 104]
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
    lea rsp, [rbp - CI_FRAME_OFFSET + CI_LOCAL_SIZE]
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    pop rbp
    ret

Lci_error:
    mov r9d, eax
    mov rdx, QWORD PTR [rbx + 16]
Lci_error_parent:
    cmp rdx, QWORD PTR [rbx + 8]
    je Lci_error_root
    mov rdx, QWORD PTR [rdx + CI_ASM_parent]
CI_STACK_RESTORE 1
    jmp Lci_error_parent
Lci_error_root:
    mov rax, QWORD PTR [rdx + CI_ASM_code]
    cmp WORD PTR [rax + CI_ASM_code_ops], CI_OP_FRAME_SAVE
    jne Lci_error_exit
CI_STACK_RESTORE 1
Lci_error_exit:
    mov eax, r9d
    jmp Lci_exit

CI_LABEL RETURN
CI_LABEL FRAME_RESTORE
    mov rdx, QWORD PTR [rbx + 16]
    mov rax, QWORD PTR [rdx + CI_ASM_code]
    mov rax, QWORD PTR [rax + CI_ASM_code_count]
    mov QWORD PTR [rdx + CI_ASM_pc], rax
    cmp rdx, QWORD PTR [rbx + 8]
    je Lci_return_root
    mov rdx, QWORD PTR [rdx + CI_ASM_parent]
    mov QWORD PTR [rbx + 16], rdx
    inc QWORD PTR [rdx + CI_ASM_pc]
CI_STACK_RESTORE 1
    jmp Lci_load_frame
Lci_return_root:
    mov rax, QWORD PTR [rdx + CI_ASM_code]
    cmp WORD PTR [rax + CI_ASM_code_ops], CI_OP_FRAME_SAVE
    jne Lci_complete
CI_STACK_RESTORE 1
    jmp Lci_complete

CI_LABEL STACK_RESTORE
CI_STACK_RESTORE 0
    CI_NEXT

CI_STACK_SAVE MACRO p_name, p_boundary
    LOCAL grow
    CI_LABEL p_name
    IF p_boundary EQ 0
        CI_PUBLISH_PC
    ENDIF
    mov rax, QWORD PTR [r15 + CI_ASM_stack_mark_count]
    cmp rax, QWORD PTR [r15 + CI_ASM_stack_mark_capacity]
    jae grow
    lea r11, [rax + rax*2]
    mov r10, QWORD PTR [r15 + CI_ASM_stack_marks]
    lea r10, [r10 + r11*8]
    movdqu xmm4, XMMWORD PTR [r15 + CI_ASM_stack_top]
    movdqu XMMWORD PTR [r10], xmm4
    mov r11, QWORD PTR [r15 + CI_ASM_stack_boundary]
    mov QWORD PTR [r10 + 16], r11
    inc rax
    mov QWORD PTR [r15 + CI_ASM_stack_mark_count], rax
    IF p_boundary
        mov QWORD PTR [r15 + CI_ASM_stack_boundary], rax
    ENDIF
    CI_NEXT
grow:
    CI_ARGS_SAVE
    mov rcx, r15
    mov edx, p_boundary
    CI_HELPER stack_save
    CI_ARGS_RESTORE
    test eax, eax
    jne Lci_error
    CI_NEXT
ENDM
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
    mov BYTE PTR [r10 + rax], 0CCh
    jmp Lci_frame_poison
Lci_frame_grow:
    CI_ARGS_SAVE
    lea r10, [r13 - CI_ASM_code_ops - 32]
    mov rcx, QWORD PTR [rbx]
    mov rdx, r15
    mov r8, QWORD PTR [rbx + 16]
    mov r9, QWORD PTR [r10 + 8]
    mov eax, DWORD PTR [r10 + 16]
    mov QWORD PTR [rsp + 32], rax
    lea rax, [rbx + 16]
    mov QWORD PTR [rsp + 40], rax
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
    je Lci_frame_no_incoming
    lea rax, [r12 + rax + CI_ASM_frame_size]
    jmp Lci_frame_incoming
Lci_frame_no_incoming:
    xor eax, eax
Lci_frame_incoming:
    mov QWORD PTR [r12 + CI_ASM_incoming], rax
    add r12, CI_ASM_frame_size
    CI_NEXT
CI_LABEL FRAME_READY
    CI_NEXT

CI_CALL MACRO p_name, p_indirect
    LOCAL hotswap, ready
    CI_LABEL p_name
    CI_PUBLISH_PC
    IF p_indirect
        mov eax, DWORD PTR [r13 + 12]
        mov r11, QWORD PTR [r12 + rax - 8]
        mov r10, QWORD PTR [r12 + rax - 16]
    ELSE
        mov rax, QWORD PTR [r13 + 16]
        mov r11, QWORD PTR [rax + 16]
        mov r10, QWORD PTR [r11 + CI_ASM_native_func]
    ENDIF
    test r11, r11
    je Lci_native_call
    test BYTE PTR [r11 + CI_ASM_func_flags], 64
    je Lci_native_call
    mov r10d, 33
hotswap:
    mov rax, QWORD PTR [r11 + CI_ASM_func_hotswap]
    test rax, rax
    jz ready
    mov r11, rax
    dec r10d
    jnz hotswap
    CI_LEA r8, Lci_msg_hotswap
    jmp Lci_call_error
ready:
    mov rax, QWORD PTR [r11 + CI_ASM_func_code]
    mov r13, QWORD PTR [rax + 8]
    add r13, CI_ASM_code_ops
    CI_DISPATCH
ENDM
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
    mov r11, QWORD PTR [rax + CI_ASM_plan_stack_alignment]
    mov r10, rsp
    sub r10, QWORD PTR [rsp + 8]
    sub r10, 32
    neg r11
    and r10, r11
    and r10, -16
Lci_native_probe:
    mov rax, rsp
    sub rax, r10
    cmp rax, 4096
    jb Lci_native_stack
    sub rsp, 4096
    test BYTE PTR [rsp], 0
    jmp Lci_native_probe
Lci_native_stack:
    mov rsp, r10
    mov rax, QWORD PTR [rbp - CI_FRAME_OFFSET + 8]
    mov r10, QWORD PTR [r15 + 96]
    lea r11, [rsp + 32]
    call Lci_abi_copy
    call QWORD PTR [rbx + 48]
    mov QWORD PTR [r15 + 104], rax
    movdqu XMMWORD PTR [r15 + 112], xmm0
    lea rsp, [rbp - CI_FRAME_OFFSET]
    CI_NEXT
Lci_null_call:
    CI_LEA r8, Lci_msg_null_call
Lci_call_error:
    mov rcx, QWORD PTR [rbx]
    mov rdx, QWORD PTR [r13 + 24]
    CI_HELPER error
    jmp Lci_error

CI_ALU_VALUE MACRO p_bytes, p_operation, p_base, p_index, p_offset:=<0>
    IFIDN <p_operation>, <mul>
        IF (p_bytes EQ 1)
            mul BYTE PTR [p_base + p_index + p_offset]
        ELSEIF (p_bytes EQ 2)
            imul ax, WORD PTR [p_base + p_index + p_offset]
        ELSEIF (p_bytes EQ 4)
            imul eax, DWORD PTR [p_base + p_index + p_offset]
        ELSE
            imul rax, QWORD PTR [p_base + p_index + p_offset]
        ENDIF
    ELSE
        IF (p_bytes EQ 1)
            p_operation al, BYTE PTR [p_base + p_index + p_offset]
        ELSEIF (p_bytes EQ 2)
            p_operation ax, WORD PTR [p_base + p_index + p_offset]
        ELSEIF (p_bytes EQ 4)
            p_operation eax, DWORD PTR [p_base + p_index + p_offset]
        ELSE
            p_operation rax, QWORD PTR [p_base + p_index + p_offset]
        ENDIF
    ENDIF
ENDM
CI_ALU MACRO p_name, p_bytes, p_operation, p_immediate:=<0>
    CI_LABEL p_name
    mov r10d, DWORD PTR [r13 + 4]
    CI_LOAD_SLOT 8, p_bytes
    IF p_immediate
        CI_ALU_VALUE p_bytes, p_operation, r13, 0, 16
    ELSE
        mov ecx, DWORD PTR [r13 + 12]
        CI_ALU_VALUE p_bytes, p_operation, r12, rcx
    ENDIF
    CI_STORE p_bytes, r12+r10
    CI_NEXT
ENDM
FOR p_bits, <8, 16, 32, 64>
    CI_ALU ADD&p_bits, %(p_bits/8), add
    CI_ALU SUB&p_bits, %(p_bits/8), sub
    CI_ALU MUL&p_bits, %(p_bits/8), mul
    CI_ALU AND&p_bits, %(p_bits/8), and
    CI_ALU OR&p_bits, %(p_bits/8), or
    CI_ALU XOR&p_bits, %(p_bits/8), xor
ENDM
FOR p_bits, <32, 64>
    CI_ALU ADD_IMM&p_bits, %(p_bits/8), add, 1
    CI_ALU SUB_IMM&p_bits, %(p_bits/8), sub, 1
    CI_ALU MUL_IMM&p_bits, %(p_bits/8), mul, 1
    CI_ALU AND_IMM&p_bits, %(p_bits/8), and, 1
    CI_ALU OR_IMM&p_bits, %(p_bits/8), or, 1
    CI_ALU XOR_IMM&p_bits, %(p_bits/8), xor, 1
ENDM

CI_UNARY MACRO p_name, p_bytes, p_operation
    CI_LABEL p_name
    mov r10d, DWORD PTR [r13 + 4]
    CI_LOAD_SLOT 8, %p_bytes
    p_operation rax
    CI_STORE %p_bytes, r12+r10
    CI_NEXT
ENDM
FOR p_bits, <8, 16, 32, 64>
    CI_UNARY NEG&p_bits, %(p_bits/8), neg
    CI_UNARY NOT&p_bits, %(p_bits/8), not
ENDM

CI_COMPARE MACRO p_name, p_bits, p_condition, p_branch:=<0>
    LOCAL fallthrough
    CI_LABEL p_name
    mov eax, DWORD PTR [r13 + 12]
    mov ecx, DWORD PTR [r13 + 16]
    IF (p_bits EQ 32)
        mov eax, DWORD PTR [r12 + rax]
        cmp eax, DWORD PTR [r12 + rcx]
    ELSE
        mov rax, QWORD PTR [r12 + rax]
        cmp rax, QWORD PTR [r12 + rcx]
    ENDIF
    set&p_condition al
    IF p_branch
        xor al, BYTE PTR [r13 + 2]
        test al, 1
        jne fallthrough
        movsxd rax, DWORD PTR [r13 + 4]
        shl rax, 5
        add r13, rax
        CI_DISPATCH
fallthrough:
        CI_NEXT
    ELSE
        mov ecx, DWORD PTR [r13 + 4]
        mov BYTE PTR [r12 + rcx], al
        CI_NEXT
    ENDIF
ENDM
FOR p_bits, <32, 64>
    CI_COMPARE EQ&p_bits, p_bits, e
    CI_COMPARE NE&p_bits, p_bits, ne
    CI_COMPARE SLT&p_bits, p_bits, l
    CI_COMPARE ULT&p_bits, p_bits, b
    CI_COMPARE SLE&p_bits, p_bits, le
    CI_COMPARE ULE&p_bits, p_bits, be
    CI_COMPARE SGT&p_bits, p_bits, g
    CI_COMPARE UGT&p_bits, p_bits, a
    CI_COMPARE SGE&p_bits, p_bits, ge
    CI_COMPARE UGE&p_bits, p_bits, ae
    CI_COMPARE EQ_JUMP&p_bits, p_bits, e, 1
    CI_COMPARE NE_JUMP&p_bits, p_bits, ne, 1
    CI_COMPARE SLT_JUMP&p_bits, p_bits, l, 1
    CI_COMPARE ULT_JUMP&p_bits, p_bits, b, 1
    CI_COMPARE SLE_JUMP&p_bits, p_bits, le, 1
    CI_COMPARE ULE_JUMP&p_bits, p_bits, be, 1
    CI_COMPARE SGT_JUMP&p_bits, p_bits, g, 1
    CI_COMPARE UGT_JUMP&p_bits, p_bits, a, 1
    CI_COMPARE SGE_JUMP&p_bits, p_bits, ge, 1
    CI_COMPARE UGE_JUMP&p_bits, p_bits, ae, 1
ENDM

CI_SHIFT MACRO p_name, p_bytes, p_operation, p_sign:=<0>, p_immediate:=<0>
    CI_LABEL p_name
    mov r10d, DWORD PTR [r13 + 4]
    IF p_immediate
        IF (p_bytes EQ 4)
            mov ecx, DWORD PTR [r13 + 16]
        ELSE
            mov rcx, QWORD PTR [r13 + 16]
        ENDIF
    ELSE
        mov ecx, DWORD PTR [r13 + 12]
        IF (p_bytes EQ 1)
            movzx ecx, BYTE PTR [r12 + rcx]
        ELSEIF (p_bytes EQ 2)
            movzx ecx, WORD PTR [r12 + rcx]
        ELSEIF (p_bytes EQ 4)
            mov ecx, DWORD PTR [r12 + rcx]
        ELSE
            mov rcx, QWORD PTR [r12 + rcx]
        ENDIF
    ENDIF
    CI_LOAD_SLOT 8, p_bytes, p_sign
    p_operation rax, cl
    CI_STORE p_bytes, r12+r10
    CI_NEXT
ENDM

CI_DIV MACRO p_name, p_bytes, p_sign, p_remainder, p_immediate:=<0>
    LOCAL branch_1, branch_2, branch_3
    CI_LABEL p_name
    mov r10d, DWORD PTR [r13 + 4]
    IF p_immediate
        CI_READ p_bytes, p_sign, r13+16
    ELSE
        CI_LOAD_SLOT 12, p_bytes, p_sign
    ENDIF
    mov rcx, rax
    CI_LOAD_SLOT 8, %p_bytes, p_sign
    ; Avoid x86 #DE for both zero divisors and INT64_MIN / -1.
    test rcx, rcx
    jz branch_2
    IF p_sign
        IF (p_bytes EQ 8)
            cmp rcx, -1
            jne branch_1
            IF p_remainder
                xor eax, eax
            ELSE
                neg rax
            ENDIF
            jmp branch_3
branch_1:
        ENDIF
        IF (p_bytes LT 4)
            cdq
            idiv ecx
        ELSE
            cqo
            idiv rcx
        ENDIF
    ELSE
        xor edx, edx
        IF (p_bytes LT 8)
            div ecx
        ELSE
            div rcx
        ENDIF
    ENDIF
    IF p_remainder
        mov rax, rdx
    ENDIF
    jmp branch_3
branch_2:
    xor eax, eax
branch_3:
    CI_STORE %p_bytes, r12+r10
    CI_NEXT
ENDM
FOR p_bits, <8, 16, 32, 64>
    CI_SHIFT SHL&p_bits, %(p_bits/8), shl
    CI_SHIFT ASR&p_bits, %(p_bits/8), sar, 1
    CI_SHIFT LSR&p_bits, %(p_bits/8), shr
    CI_DIV SDIV&p_bits, %(p_bits/8), 1, 0
    CI_DIV UDIV&p_bits, %(p_bits/8), 0, 0
    CI_DIV SMOD&p_bits, %(p_bits/8), 1, 1
    CI_DIV UMOD&p_bits, %(p_bits/8), 0, 1
ENDM
FOR p_bits, <32, 64>
    CI_SHIFT SHL_IMM&p_bits, %(p_bits/8), shl, 0, 1
    CI_SHIFT ASR_IMM&p_bits, %(p_bits/8), sar, 1, 1
    CI_SHIFT LSR_IMM&p_bits, %(p_bits/8), shr, 0, 1
    CI_DIV SDIV_IMM&p_bits, %(p_bits/8), 1, 0, 1
    CI_DIV UDIV_IMM&p_bits, %(p_bits/8), 0, 0, 1
    CI_DIV SMOD_IMM&p_bits, %(p_bits/8), 1, 1, 1
    CI_DIV UMOD_IMM&p_bits, %(p_bits/8), 0, 1, 1
ENDM

CI_MEMORY_READ MACRO p_bytes, p_base
    IF p_bytes EQ 16
        movdqu xmm0, XMMWORD PTR [p_base]
    ELSE
        CI_READ p_bytes, 0, p_base
    ENDIF
ENDM
CI_MEMORY_WRITE MACRO p_bytes, p_base
    IF p_bytes EQ 16
        movdqu XMMWORD PTR [p_base], xmm0
    ELSE
        CI_STORE p_bytes, p_base
    ENDIF
ENDM
CI_MEMORY_FIXED MACRO p_mode, p_bytes
    IF p_mode EQ 0
        CI_MEMORY_READ p_bytes, <r13+8>
    ELSEIF p_mode EQ 1
        mov eax, DWORD PTR [r13 + 16]
        CI_MEMORY_READ p_bytes, <r12+rax>
    ELSEIF (p_mode EQ 2) OR (p_mode EQ 5)
        CI_LOAD_SLOT 16, 8
        mov ecx, DWORD PTR [r13 + 20]
        CI_MEMORY_READ p_bytes, <rax+rcx>
    ELSEIF p_mode EQ 3
        mov eax, DWORD PTR [r13 + 12]
        CI_MEMORY_READ p_bytes, <r12+rax>
    ELSEIF p_mode EQ 4
        CI_MEMORY_READ p_bytes, <r13+16>
    ELSE
        IF p_bytes EQ 16
            pxor xmm0, xmm0
        ELSE
            xor eax, eax
        ENDIF
    ENDIF
    IF p_mode LE 2
        IF p_mode EQ 0
            mov r10d, DWORD PTR [r13 + 4]
        ELSE
            mov r10d, DWORD PTR [r13 + 8]
        ENDIF
        CI_MEMORY_WRITE p_bytes, <r12+r10>
    ELSE
        IF p_mode EQ 4
            mov r10d, DWORD PTR [r13 + 4]
            mov ecx, DWORD PTR [r13 + 8]
        ELSE
            mov r10d, DWORD PTR [r13 + 8]
            IF p_mode EQ 3
                mov ecx, DWORD PTR [r13 + 20]
            ELSE
                mov ecx, DWORD PTR [r13 + 12]
            ENDIF
        ENDIF
        mov r10, QWORD PTR [r12 + r10]
        CI_MEMORY_WRITE p_bytes, <r10+rcx>
    ENDIF
ENDM
CI_MEMORY MACRO p_name, p_mode, p_bytes
    CI_LABEL p_name
    IF (p_bytes GT 0) AND (p_bytes LE 16)
        CI_MEMORY_FIXED p_mode, p_bytes
        CI_NEXT
        EXITM
    ENDIF
    IF (p_mode EQ 0)
        CI_SLOT r10, r10d, 4
        lea rax, [r13 + 8]
    ELSEIF (p_mode EQ 1) OR (p_mode EQ 2)
        CI_SLOT r10, r10d, 8
        CI_SLOT rax, eax, 16
        IF (p_mode EQ 2)
            mov rax, QWORD PTR [rax]
            mov ecx, DWORD PTR [r13 + 20]
            add rax, rcx
        ENDIF
    ELSEIF (p_mode EQ 3)
        mov r10d, DWORD PTR [r13 + 8]
        mov r10, QWORD PTR [r12 + r10]
        mov ecx, DWORD PTR [r13 + 20]
        add r10, rcx
        CI_SLOT rax, eax, 12
    ELSEIF (p_mode EQ 4)
        mov r10d, DWORD PTR [r13 + 4]
        mov r10, QWORD PTR [r12 + r10]
        mov ecx, DWORD PTR [r13 + 8]
        add r10, rcx
        lea rax, [r13 + 16]
    ELSEIF (p_mode EQ 5) OR (p_mode EQ 6)
        mov r10d, DWORD PTR [r13 + 8]
        mov r10, QWORD PTR [r12 + r10]
        mov ecx, DWORD PTR [r13 + 12]
        add r10, rcx
        IF (p_mode EQ 5)
            CI_LOAD_SLOT 16, 8
            mov ecx, DWORD PTR [r13 + 20]
            add rax, rcx
        ENDIF
    ENDIF
    IF (p_mode EQ 0)
        movzx edx, BYTE PTR [r13 + 3]
    ELSEIF (p_mode EQ 1) OR (p_mode EQ 2)
        mov edx, DWORD PTR [r13 + 12]
    ELSEIF (p_mode EQ 3)
        mov edx, DWORD PTR [r13 + 16]
    ELSEIF (p_mode EQ 4)
        movzx edx, BYTE PTR [r13 + 2]
    ELSE
        mov edx, DWORD PTR [r13 + 4]
        IF (p_bytes EQ -1)
            mov rdx, QWORD PTR [r12 + rdx]
        ENDIF
    ENDIF
    mov r11, QWORD PTR [rbx + 24]
    mov rcx, r10
    mov r8, rdx
    IF (p_mode EQ 6)
        xor edx, edx
    ELSE
        mov rdx, rax
    ENDIF
    IF (p_mode EQ 6)
        call QWORD PTR [r11 + CI_ASM_HELPER_memset*8]
    ELSE
        call QWORD PTR [r11 + CI_ASM_HELPER_memmove*8]
    ENDIF
    CI_NEXT
ENDM
CI_MEMORY CONST8, 0, 1
CI_MEMORY CONST16, 0, 2
CI_MEMORY CONST32, 0, 4
CI_MEMORY CONST64, 0, 8
CI_MEMORY CONST128, 0, 16
CI_MEMORY CONST_N, 0, 0

CI_MEMORY COPY8, 1, 1
CI_MEMORY COPY16_A, 1, 2
CI_MEMORY COPY16_U, 1, 2
CI_MEMORY COPY32_A, 1, 4
CI_MEMORY COPY32_U, 1, 4
CI_MEMORY COPY64_A, 1, 8
CI_MEMORY COPY64_U, 1, 8
CI_MEMORY COPY128_A, 1, 16
CI_MEMORY COPY128_U, 1, 16
CI_MEMORY COPY_N, 1, 0
CI_MEMORY COPY_N_A2, 1, 0
CI_MEMORY COPY_N_A4, 1, 0
CI_MEMORY COPY_N_A8, 1, 0
CI_MEMORY COPY_N_A16, 1, 0

CI_MEMORY LOAD8, 2, 1
CI_MEMORY LOAD16_A, 2, 2
CI_MEMORY LOAD16_U, 2, 2
CI_MEMORY LOAD32_A, 2, 4
CI_MEMORY LOAD32_U, 2, 4
CI_MEMORY LOAD64_A, 2, 8
CI_MEMORY LOAD64_U, 2, 8
CI_MEMORY LOAD128_A, 2, 16
CI_MEMORY LOAD128_U, 2, 16
CI_MEMORY LOAD_N, 2, 0
CI_MEMORY LOAD_N_A2, 2, 0
CI_MEMORY LOAD_N_A4, 2, 0
CI_MEMORY LOAD_N_A8, 2, 0
CI_MEMORY LOAD_N_A16, 2, 0

CI_MEMORY STORE8, 3, 1
CI_MEMORY STORE16_A, 3, 2
CI_MEMORY STORE16_U, 3, 2
CI_MEMORY STORE32_A, 3, 4
CI_MEMORY STORE32_U, 3, 4
CI_MEMORY STORE64_A, 3, 8
CI_MEMORY STORE64_U, 3, 8
CI_MEMORY STORE128_A, 3, 16
CI_MEMORY STORE128_U, 3, 16
CI_MEMORY STORE_N, 3, 0
CI_MEMORY STORE_N_A2, 3, 0
CI_MEMORY STORE_N_A4, 3, 0
CI_MEMORY STORE_N_A8, 3, 0
CI_MEMORY STORE_N_A16, 3, 0

CI_MEMORY STORE_IMM8, 4, 1
CI_MEMORY STORE_IMM16_A, 4, 2
CI_MEMORY STORE_IMM16_U, 4, 2
CI_MEMORY STORE_IMM32_A, 4, 4
CI_MEMORY STORE_IMM32_U, 4, 4
CI_MEMORY STORE_IMM64_A, 4, 8
CI_MEMORY STORE_IMM64_U, 4, 8
CI_MEMORY STORE_IMM_N, 4, 0
CI_MEMORY STORE_IMM_N_A2, 4, 0
CI_MEMORY STORE_IMM_N_A4, 4, 0
CI_MEMORY STORE_IMM_N_A8, 4, 0
CI_MEMORY STORE_IMM_N_A16, 4, 0

CI_MEMORY MEMCOPY8, 5, 1
CI_MEMORY MEMCOPY16_A, 5, 2
CI_MEMORY MEMCOPY16_U, 5, 2
CI_MEMORY MEMCOPY32_A, 5, 4
CI_MEMORY MEMCOPY32_U, 5, 4
CI_MEMORY MEMCOPY64_A, 5, 8
CI_MEMORY MEMCOPY64_U, 5, 8
CI_MEMORY MEMCOPY128_A, 5, 16
CI_MEMORY MEMCOPY128_U, 5, 16
CI_MEMORY MEMCOPY_N, 5, 0
CI_MEMORY MEMCOPY_N_A2, 5, 0
CI_MEMORY MEMCOPY_N_A4, 5, 0
CI_MEMORY MEMCOPY_N_A8, 5, 0
CI_MEMORY MEMCOPY_N_A16, 5, 0
CI_MEMORY MEMCOPY_VAR, 5, -1
CI_MEMORY MEMCOPY_VAR_A2, 5, -1
CI_MEMORY MEMCOPY_VAR_A4, 5, -1
CI_MEMORY MEMCOPY_VAR_A8, 5, -1
CI_MEMORY MEMCOPY_VAR_A16, 5, -1

CI_MEMORY ZERO8, 6, 1
CI_MEMORY ZERO16_A, 6, 2
CI_MEMORY ZERO16_U, 6, 2
CI_MEMORY ZERO32_A, 6, 4
CI_MEMORY ZERO32_U, 6, 4
CI_MEMORY ZERO64_A, 6, 8
CI_MEMORY ZERO64_U, 6, 8
CI_MEMORY ZERO128_A, 6, 16
CI_MEMORY ZERO128_U, 6, 16
CI_MEMORY ZERO_N, 6, 0
CI_MEMORY ZERO_N_A2, 6, 0
CI_MEMORY ZERO_N_A4, 6, 0
CI_MEMORY ZERO_N_A8, 6, 0
CI_MEMORY ZERO_N_A16, 6, 0

CI_EXTEND MACRO p_name, p_from, p_to, p_sign
    CI_LABEL p_name
    mov r10d, DWORD PTR [r13 + 8]
    CI_LOAD_SLOT 16, %p_from, p_sign
    CI_STORE %p_to, r12+r10
    IF (p_to EQ 16)
        IF p_sign
            sar rax, 63
        ELSE
            xor eax, eax
        ENDIF
        mov QWORD PTR [r12 + r10 + 8], rax
    ENDIF
    CI_NEXT
ENDM
CI_EXTEND SEXT8_16, 1, 2, 1
CI_EXTEND ZEXT8_16, 1, 2, 0
CI_EXTEND SEXT8_32, 1, 4, 1
CI_EXTEND ZEXT8_32, 1, 4, 0
CI_EXTEND SEXT8_64, 1, 8, 1
CI_EXTEND ZEXT8_64, 1, 8, 0
CI_EXTEND SEXT8_128, 1, 16, 1
CI_EXTEND ZEXT8_128, 1, 16, 0
CI_EXTEND SEXT16_32, 2, 4, 1
CI_EXTEND ZEXT16_32, 2, 4, 0
CI_EXTEND SEXT16_64, 2, 8, 1
CI_EXTEND ZEXT16_64, 2, 8, 0
CI_EXTEND SEXT16_128, 2, 16, 1
CI_EXTEND ZEXT16_128, 2, 16, 0
CI_EXTEND SEXT32_64, 4, 8, 1
CI_EXTEND ZEXT32_64, 4, 8, 0
CI_EXTEND SEXT32_128, 4, 16, 1
CI_EXTEND ZEXT32_128, 4, 16, 0
CI_EXTEND SEXT64_128, 8, 16, 1
CI_EXTEND ZEXT64_128, 8, 16, 0

CI_FLOAT MACRO p_name, p_suffix, p_operation, p_type
    CI_LABEL p_name
    mov eax, DWORD PTR [r13 + 12]
    mov ecx, DWORD PTR [r13 + 16]
    mov&p_suffix xmm0, p_type PTR [r12 + rax]
    p_operation&p_suffix xmm0, p_type PTR [r12 + rcx]
    mov eax, DWORD PTR [r13 + 4]
    mov&p_suffix p_type PTR [r12 + rax], xmm0
    CI_NEXT
ENDM
CI_FLOAT FADD32, ss, add, DWORD
CI_FLOAT FSUB32, ss, sub, DWORD
CI_FLOAT FMUL32, ss, mul, DWORD
CI_FLOAT FDIV32, ss, div, DWORD
CI_FLOAT FADD64, sd, add, QWORD
CI_FLOAT FSUB64, sd, sub, QWORD
CI_FLOAT FMUL64, sd, mul, QWORD
CI_FLOAT FDIV64, sd, div, QWORD

CI_FLOAT_COMPARE MACRO p_name, p_suffix, p_condition, p_unordered, p_type, p_reverse:=<0>
    CI_LABEL p_name
    mov eax, DWORD PTR [r13 + 12]
    mov ecx, DWORD PTR [r13 + 16]
    IF p_reverse
        mov&p_suffix xmm0, p_type PTR [r12 + rcx]
        ucomi&p_suffix xmm0, p_type PTR [r12 + rax]
    ELSE
        mov&p_suffix xmm0, p_type PTR [r12 + rax]
        ucomi&p_suffix xmm0, p_type PTR [r12 + rcx]
    ENDIF
    set&p_condition al
    IF p_unordered
        setp cl
        or al, cl
    ELSE
        IFIDN <p_condition>, <e>
            setnp cl
            and al, cl
        ENDIF
    ENDIF
    mov ecx, DWORD PTR [r13 + 4]
    mov BYTE PTR [r12 + rcx], al
    CI_NEXT
ENDM
CI_FLOAT_COMPARE FEQ32, ss, e, 0, DWORD
CI_FLOAT_COMPARE FNE32, ss, ne, 1, DWORD
CI_FLOAT_COMPARE FLT32, ss, a, 0, DWORD, 1
CI_FLOAT_COMPARE FLE32, ss, ae, 0, DWORD, 1
CI_FLOAT_COMPARE FGT32, ss, a, 0, DWORD
CI_FLOAT_COMPARE FGE32, ss, ae, 0, DWORD
CI_FLOAT_COMPARE FEQ64, sd, e, 0, QWORD
CI_FLOAT_COMPARE FNE64, sd, ne, 1, QWORD
CI_FLOAT_COMPARE FLT64, sd, a, 0, QWORD, 1
CI_FLOAT_COMPARE FLE64, sd, ae, 0, QWORD, 1
CI_FLOAT_COMPARE FGT64, sd, a, 0, QWORD
CI_FLOAT_COMPARE FGE64, sd, ae, 0, QWORD

CI_CHECKED MACRO p_name, p_bits, p_operand, p_other, p_uns, p_operation
    CI_LABEL p_name
    CI_LOAD_SLOT 12, %(p_bits/8)
    mov ecx, DWORD PTR [r13 + 16]
    IFIDN <p_operation>, <mul>
        IF p_uns
            IF p_bits EQ 8
                mul BYTE PTR [r12 + rcx]
            ELSEIF p_bits EQ 16
                mul WORD PTR [r12 + rcx]
            ELSEIF p_bits EQ 32
                mul DWORD PTR [r12 + rcx]
            ELSE
                mul QWORD PTR [r12 + rcx]
            ENDIF
        ELSEIF p_bits EQ 8
            imul BYTE PTR [r12 + rcx]
        ELSE
            CI_ALU_VALUE %(p_bits/8), mul, r12, rcx
        ENDIF
    ELSE
        CI_ALU_VALUE %(p_bits/8), p_operation, r12, rcx
    ENDIF
    IF p_uns
        setc r9b
    ELSE
        seto r9b
    ENDIF
    mov r10d, DWORD PTR [r13 + 4]
    CI_STORE %(p_bits/8), r12+r10
    mov eax, DWORD PTR [r13 + 8]
    mov BYTE PTR [r12 + rax], r9b
    CI_NEXT
ENDM

CI_CHECKED_WIDTH MACRO p_bits, p_operand, p_other
    CI_CHECKED CHECKED_ADD_S&p_bits, p_bits, p_operand, p_other, 0, add
    CI_CHECKED CHECKED_ADD_U&p_bits, p_bits, p_operand, p_other, 1, add
    CI_CHECKED CHECKED_SUB_S&p_bits, p_bits, p_operand, p_other, 0, sub
    CI_CHECKED CHECKED_SUB_U&p_bits, p_bits, p_operand, p_other, 1, sub
    CI_CHECKED CHECKED_MUL_S&p_bits, p_bits, p_operand, p_other, 0, mul
    CI_CHECKED CHECKED_MUL_U&p_bits, p_bits, p_operand, p_other, 1, mul
ENDM
CI_CHECKED_WIDTH 8, al, cl
CI_CHECKED_WIDTH 16, ax, cx
CI_CHECKED_WIDTH 32, eax, ecx
CI_CHECKED_WIDTH 64, rax, rcx

CI_OVERFLOW MACRO p_name, p_bits, p_uns
    CI_LABEL p_name
    mov eax, DWORD PTR [r13 + 12]
    IF p_uns AND (p_bits EQ 64)
        cmp QWORD PTR [r12 + rax + 8], 0
    ELSE
        mov rcx, QWORD PTR [r12 + rax + 8]
        mov rax, QWORD PTR [r12 + rax]
    ENDIF
    IF p_uns
        IF (p_bits LT 64)
            shr rax, p_bits
            or rcx, rax
        ENDIF
        setne al
    ELSE
        IF p_bits EQ 8
            movsx rdx, al
        ELSEIF p_bits EQ 16
            movsx rdx, ax
        ELSEIF p_bits EQ 32
            movsxd rdx, eax
        ELSE
            mov rdx, rax
        ENDIF
        IF p_bits LT 64
            cmp rdx, rax
            setne r9b
        ENDIF
        sar rdx, 63
        cmp rcx, rdx
        setne al
        IF p_bits LT 64
            or al, r9b
        ENDIF
    ENDIF
    mov ecx, DWORD PTR [r13 + 8]
    mov BYTE PTR [r12 + rcx], al
    CI_NEXT
ENDM
CI_OVERFLOW OVERFLOW_S8, 8, 0
CI_OVERFLOW OVERFLOW_U8, 8, 1
CI_OVERFLOW OVERFLOW_S16, 16, 0
CI_OVERFLOW OVERFLOW_U16, 16, 1
CI_OVERFLOW OVERFLOW_S32, 32, 0
CI_OVERFLOW OVERFLOW_U32, 32, 1
CI_OVERFLOW OVERFLOW_S64, 64, 0
CI_OVERFLOW OVERFLOW_U64, 64, 1

CI_WIDE MACRO p_name, p_operation, p_carry
    CI_LABEL p_name
    mov eax, DWORD PTR [r13 + 8]
    mov rdx, QWORD PTR [r12 + rax + 8]
    mov rax, QWORD PTR [r12 + rax]
    mov r10d, DWORD PTR [r13 + 12]
    p_operation rax, QWORD PTR [r12 + r10]
    p_carry rdx, QWORD PTR [r12 + r10 + 8]
    mov r10d, DWORD PTR [r13 + 4]
    mov QWORD PTR [r12 + r10], rax
    mov QWORD PTR [r12 + r10 + 8], rdx
    CI_NEXT
ENDM
CI_WIDE ADD128, add, adc
CI_WIDE SUB128, sub, sbb
CI_WIDE_BIT MACRO p_name, p_operation
    CI_LABEL p_name
    mov eax, DWORD PTR [r13 + 8]
    mov ecx, DWORD PTR [r13 + 12]
    vmovdqu xmm4, XMMWORD PTR [r12 + rax]
    p_operation xmm4, xmm4, XMMWORD PTR [r12 + rcx]
    mov eax, DWORD PTR [r13 + 4]
    vmovdqu XMMWORD PTR [r12 + rax], xmm4
    CI_NEXT
ENDM
CI_WIDE_BIT AND128, vpand
CI_WIDE_BIT OR128, vpor
CI_WIDE_BIT XOR128, vpxor
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

CI_WIDE_UNARY MACRO p_name, p_negate
    CI_LABEL p_name
    mov r10d, DWORD PTR [r13 + 8]
    mov rax, QWORD PTR [r12 + r10]
    mov rdx, QWORD PTR [r12 + r10 + 8]
    IF p_negate
        neg rax
        adc rdx, 0
        neg rdx
    ELSE
        not rax
        not rdx
    ENDIF
    mov r10d, DWORD PTR [r13 + 4]
    mov QWORD PTR [r12 + r10], rax
    mov QWORD PTR [r12 + r10 + 8], rdx
    CI_NEXT
ENDM
CI_WIDE_UNARY NEG128, 1
CI_WIDE_UNARY NOT128, 0

CI_WIDE_COMPARE MACRO p_name, p_condition
    LOCAL branch_1
    CI_LABEL p_name
    mov r10d, DWORD PTR [r13 + 12]
    mov r11d, DWORD PTR [r13 + 16]
    mov rax, QWORD PTR [r12 + r10 + 8]
    cmp rax, QWORD PTR [r12 + r11 + 8]
    set&p_condition al
    jne branch_1
    mov rax, QWORD PTR [r12 + r10]
    cmp rax, QWORD PTR [r12 + r11]
    IFIDN <p_condition>, <l>
        setb al
    ELSE
        IFIDN <p_condition>, <le>
            setbe al
        ELSE
            IFIDN <p_condition>, <g>
                seta al
            ELSE
                IFIDN <p_condition>, <ge>
                    setae al
                ELSE
                    set&p_condition al
                ENDIF
            ENDIF
        ENDIF
    ENDIF
branch_1:
    mov ecx, DWORD PTR [r13 + 4]
    mov BYTE PTR [r12 + rcx], al
    CI_NEXT
ENDM
CI_WIDE_EQUAL MACRO p_name, p_condition
    CI_LABEL p_name
    mov r10d, DWORD PTR [r13 + 12]
    mov r11d, DWORD PTR [r13 + 16]
    mov rax, QWORD PTR [r12 + r10]
    mov rdx, QWORD PTR [r12 + r10 + 8]
    xor rax, QWORD PTR [r12 + r11]
    xor rdx, QWORD PTR [r12 + r11 + 8]
    or rax, rdx
    set&p_condition al
    mov ecx, DWORD PTR [r13 + 4]
    mov BYTE PTR [r12 + rcx], al
    CI_NEXT
ENDM
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

CI_WIDE_SHIFT MACRO p_name, p_right, p_sign
    LOCAL branch_1, branch_3
    CI_LABEL p_name
    mov r10d, DWORD PTR [r13 + 8]
    mov rax, QWORD PTR [r12 + r10]
    mov rdx, QWORD PTR [r12 + r10 + 8]
    mov r10d, DWORD PTR [r13 + 12]
    mov rcx, QWORD PTR [r12 + r10]
    cmp ecx, 64
    jae branch_1
    IF p_right
        shrd rax, rdx, cl
        IF p_sign
            sar rdx, cl
        ELSE
            shr rdx, cl
        ENDIF
    ELSE
        shld rdx, rax, cl
        shl rax, cl
    ENDIF
    jmp branch_3
branch_1:
    ; Hardware masking of CL gives exactly the required count minus 64 here.
    IF p_right
        mov rax, rdx
        IF p_sign
            sar rax, cl
            sar rdx, 63
        ELSE
            shr rax, cl
            xor edx, edx
        ENDIF
    ELSE
        mov rdx, rax
        shl rdx, cl
        xor eax, eax
    ENDIF
branch_3:
    mov r10d, DWORD PTR [r13 + 4]
    mov QWORD PTR [r12 + r10], rax
    mov QWORD PTR [r12 + r10 + 8], rdx
    CI_NEXT
ENDM
CI_WIDE_SHIFT SHL128, 0, 0
CI_WIDE_SHIFT LSR128, 1, 0
CI_WIDE_SHIFT ASR128, 1, 1

CI_COND_JUMP MACRO p_name, p_bytes, p_nonzero
    CI_LABEL p_name
    mov eax, DWORD PTR [r13 + 8]
    IF p_bytes EQ 16
        mov rcx, QWORD PTR [r12 + rax]
        or rcx, QWORD PTR [r12 + rax + 8]
    ELSEIF p_bytes EQ 1
        cmp BYTE PTR [r12 + rax], 0
    ELSEIF p_bytes EQ 2
        cmp WORD PTR [r12 + rax], 0
    ELSEIF p_bytes EQ 4
        cmp DWORD PTR [r12 + rax], 0
    ELSE
        cmp QWORD PTR [r12 + rax], 0
    ENDIF
    IF p_nonzero
        jnz Lci_JUMP
    ELSE
        jz Lci_JUMP
    ENDIF
    CI_NEXT
ENDM
CI_COND_JUMP JZ8, 1, 0
CI_COND_JUMP JZ16, 2, 0
CI_COND_JUMP JZ32, 4, 0
CI_COND_JUMP JZ64, 8, 0
CI_COND_JUMP JZ128, 16, 0

CI_COND_JUMP JNZ8, 1, 1
CI_COND_JUMP JNZ16, 2, 1
CI_COND_JUMP JNZ32, 4, 1
CI_COND_JUMP JNZ64, 8, 1
CI_COND_JUMP JNZ128, 16, 1

CI_INDEX MACRO p_name, p_pointer_bytes, p_index_bytes, p_sign
    CI_LABEL p_name
    CI_LOAD_SLOT 12, %p_index_bytes, p_sign
    mov ecx, DWORD PTR [r13 + 16]
    imul rcx, rax
    CI_LOAD_SLOT 8, %p_pointer_bytes
    add rax, rcx
    mov r10d, DWORD PTR [r13 + 4]
    CI_STORE %p_pointer_bytes, r12+r10
    CI_NEXT
ENDM
CI_INDEX INDEX32_S8, 4, 1, 1
CI_INDEX INDEX32_U8, 4, 1, 0
CI_INDEX INDEX32_S16, 4, 2, 1
CI_INDEX INDEX32_U16, 4, 2, 0
CI_INDEX INDEX32, 4, 4, 0
CI_INDEX INDEX64_S8, 8, 1, 1
CI_INDEX INDEX64_U8, 8, 1, 0
CI_INDEX INDEX64_S16, 8, 2, 1
CI_INDEX INDEX64_U16, 8, 2, 0
CI_INDEX INDEX64_S32, 8, 4, 1
CI_INDEX INDEX64_U32, 8, 4, 0
CI_INDEX INDEX64, 8, 8, 0

CI_BSWAP MACRO p_name, p_bytes
    CI_LABEL p_name
    CI_LOAD_SLOT 12, %p_bytes
    IF (p_bytes EQ 2)
        rol ax, 8
    ELSEIF (p_bytes EQ 4)
        bswap eax
    ELSE
        bswap rax
    ENDIF
    mov r10d, DWORD PTR [r13 + 8]
    CI_STORE %p_bytes, r12+r10
    CI_NEXT
ENDM
CI_BSWAP BSWAP16, 2
CI_BSWAP BSWAP32, 4
CI_BSWAP BSWAP64, 8

CI_TRUTH MACRO p_name, p_format, p_bits, p_negate
    CI_LABEL p_name
    CI_SLOT rax, eax, 16
    IF (p_bits EQ 128)
        mov rcx, QWORD PTR [rax + 8]
        mov rax, QWORD PTR [rax]
        IF p_format
            btr rcx, 63
        ENDIF
        or rax, rcx
    ELSEIF (p_bits EQ 80)
        movzx ecx, WORD PTR [rax + 8]
        and ecx, 07fffh
        or rcx, QWORD PTR [rax]
    ELSE
        CI_READ %(p_bits/8)
        IF p_format
            ; Drop the sign bit and set ZF without a separate test.
            IF p_bits EQ 16
                add ax, ax
            ELSEIF p_bits EQ 32
                add eax, eax
            ELSE
                add rax, rax
            ENDIF
        ELSE
            test rax, rax
        ENDIF
    ENDIF
    IF p_negate
        sete al
    ELSE
        setne al
    ENDIF
    mov ecx, DWORD PTR [r13 + 8]
    mov BYTE PTR [r12 + rcx], al
    CI_NEXT
ENDM
CI_TRUTH ISTRUE_I8, 0, 8, 0
CI_TRUTH ISFALSE_I8, 0, 8, 1
CI_TRUTH ISTRUE_I16, 0, 16, 0
CI_TRUTH ISFALSE_I16, 0, 16, 1
CI_TRUTH ISTRUE_I32, 0, 32, 0
CI_TRUTH ISFALSE_I32, 0, 32, 1
CI_TRUTH ISTRUE_I64, 0, 64, 0
CI_TRUTH ISFALSE_I64, 0, 64, 1
CI_TRUTH ISTRUE_I128, 0, 128, 0
CI_TRUTH ISFALSE_I128, 0, 128, 1
CI_TRUTH ISTRUE_F16, 1, 16, 0
CI_TRUTH ISFALSE_F16, 1, 16, 1
CI_TRUTH ISTRUE_F32, 1, 32, 0
CI_TRUTH ISFALSE_F32, 1, 32, 1
CI_TRUTH ISTRUE_F64, 1, 64, 0
CI_TRUTH ISFALSE_F64, 1, 64, 1
CI_TRUTH ISTRUE_F80, 1, 80, 0
CI_TRUTH ISFALSE_F80, 1, 80, 1
CI_TRUTH ISTRUE_F128, 1, 128, 0
CI_TRUTH ISFALSE_F128, 1, 128, 1

CI_BIT_LOAD MACRO p_bits
    mov eax, DWORD PTR [r13 + 12]
    IF (p_bits EQ 128)
        mov rdx, QWORD PTR [r12 + rax + 8]
        mov rax, QWORD PTR [r12 + rax]
    ELSE
        CI_READ %(p_bits/8), 0, r12+rax
    ENDIF
ENDM

CI_BIT_MASK MACRO p_bits
    IF (p_bits EQ 8)
        movzx eax, al
    ELSEIF (p_bits EQ 16)
        movzx eax, ax
    ELSEIF (p_bits EQ 32)
        mov eax, eax
    ENDIF
ENDM

CI_CLZ MACRO p_bits
    xor ecx, ecx
    IF (p_bits EQ 128)
        xor r9d, r9d
        lzcnt rcx, rdx
        lzcnt r9, rax
        add r9d, 64
        test rdx, rdx
        cmovz ecx, r9d
    ELSEIF p_bits LE 32
        lzcnt ecx, eax
        IF p_bits LT 32
            sub ecx, (32-p_bits)
        ENDIF
    ELSE
        lzcnt rcx, rax
    ENDIF
ENDM

CI_CTZ MACRO p_bits
    xor ecx, ecx
    IF (p_bits EQ 128)
        xor r9d, r9d
        tzcnt rcx, rax
        tzcnt r9, rdx
        add r9d, 64
        test rax, rax
        cmovz ecx, r9d
    ELSEIF p_bits LT 32
        ; A sentinel bit gives the bytecode width for a zero input.
        mov r9d, eax
        or r9d, (1 SHL p_bits)
        tzcnt ecx, r9d
    ELSEIF p_bits EQ 32
        tzcnt ecx, eax
    ELSE
        tzcnt rcx, rax
    ENDIF
ENDM

CI_POPCOUNT MACRO p_bits
    ; Clear the false destination dependency of POPCNT on older Intel cores.
    xor ecx, ecx
    IF p_bits LE 32
        popcnt ecx, eax
    ELSE
        popcnt rcx, rax
    ENDIF
    IF (p_bits EQ 128)
        xor r9d, r9d
        popcnt r9, rdx
        add ecx, r9d
    ENDIF
ENDM
CI_BIT_FFS EQU 0
CI_BIT_CLRSB EQU 1
CI_BIT_PARITY EQU 2
CI_BIT_CLZ EQU 3
CI_BIT_CTZ EQU 4
CI_BIT_POPCOUNT EQU 5
CI_BIT_BIT_WIDTH EQU 8
CI_BIT_COUNT_ZEROS EQU 10
CI_BIT_FIRST_LEADING_ONE EQU 11
CI_BIT_FIRST_LEADING_ZERO EQU 12
CI_BIT_FIRST_TRAILING_ZERO EQU 14
CI_BIT_LEADING_ONES EQU 16
CI_BIT_TRAILING_ONES EQU 18

CI_BIT MACRO p_name, p_bits, p_operation
    CI_LABEL p_name
    CI_BIT_LOAD %p_bits
    IF (p_operation EQ 1)
        IF (p_bits EQ 128)
            mov r9, rdx
            sar r9, 63
            xor rdx, r9
        ELSE
            IF p_bits EQ 8
                movsx r9, al
            ELSEIF p_bits EQ 16
                movsx r9, ax
            ELSEIF p_bits EQ 32
                movsxd r9, eax
            ELSE
                mov r9, rax
            ENDIF
            sar r9, 63
        ENDIF
        xor rax, r9
        CI_BIT_MASK %p_bits
        CI_CLZ %p_bits
        dec ecx
    ELSEIF (p_operation EQ 2) OR (p_operation EQ 5) OR (p_operation EQ 10) OR (p_operation EQ 15)
        CI_POPCOUNT %p_bits
        IF (p_operation EQ 2)
            and ecx, 1
        ELSEIF (p_operation EQ 10)
            neg ecx
            add ecx, p_bits
        ELSEIF (p_operation EQ 15)
            cmp ecx, 1
            sete cl
            movzx ecx, cl
        ENDIF
    ELSE
        IF (p_operation EQ 12) OR (p_operation EQ 14) OR (p_operation EQ 16) OR (p_operation EQ 18)
            not rax
            IF (p_bits EQ 128)
                not rdx
            ENDIF
            CI_BIT_MASK %p_bits
        ENDIF
        IF (p_operation EQ 0) OR (p_operation EQ 4) OR (p_operation EQ 14) OR (p_operation EQ 18)
            CI_CTZ %p_bits
        ELSE
            CI_CLZ %p_bits
        ENDIF
        IF (p_operation EQ 8)
            neg ecx
            add ecx, p_bits
        ELSEIF (p_operation EQ 0) OR (p_operation EQ 11) OR (p_operation EQ 12) OR (p_operation EQ 14)
            inc ecx
            IF (p_bits EQ 128)
                or rax, rdx
            ELSE
                test rax, rax
            ENDIF
            mov r9d, 0
            cmovz ecx, r9d
        ENDIF
    ENDIF
    mov eax, DWORD PTR [r13 + 4]
    mov DWORD PTR [r12 + rax], ecx
    CI_NEXT
ENDM
CI_BIT FFS8, 8, CI_BIT_FFS
CI_BIT FFS16, 16, CI_BIT_FFS
CI_BIT FFS32, 32, CI_BIT_FFS
CI_BIT FFS64, 64, CI_BIT_FFS
CI_BIT FFS128, 128, CI_BIT_FFS
CI_BIT CLRSB8, 8, CI_BIT_CLRSB
CI_BIT CLRSB16, 16, CI_BIT_CLRSB
CI_BIT CLRSB32, 32, CI_BIT_CLRSB
CI_BIT CLRSB64, 64, CI_BIT_CLRSB
CI_BIT CLRSB128, 128, CI_BIT_CLRSB
CI_BIT PARITY8, 8, CI_BIT_PARITY
CI_BIT PARITY16, 16, CI_BIT_PARITY
CI_BIT PARITY32, 32, CI_BIT_PARITY
CI_BIT PARITY64, 64, CI_BIT_PARITY
CI_BIT PARITY128, 128, CI_BIT_PARITY
CI_BIT CLZ8, 8, CI_BIT_CLZ
CI_BIT CLZ16, 16, CI_BIT_CLZ
CI_BIT CLZ32, 32, CI_BIT_CLZ
CI_BIT CLZ64, 64, CI_BIT_CLZ
CI_BIT CLZ128, 128, CI_BIT_CLZ
CI_BIT CTZ8, 8, CI_BIT_CTZ
CI_BIT CTZ16, 16, CI_BIT_CTZ
CI_BIT CTZ32, 32, CI_BIT_CTZ
CI_BIT CTZ64, 64, CI_BIT_CTZ
CI_BIT CTZ128, 128, CI_BIT_CTZ
CI_BIT POPCOUNT8, 8, CI_BIT_POPCOUNT
CI_BIT POPCOUNT16, 16, CI_BIT_POPCOUNT
CI_BIT POPCOUNT32, 32, CI_BIT_POPCOUNT
CI_BIT POPCOUNT64, 64, CI_BIT_POPCOUNT
CI_BIT POPCOUNT128, 128, CI_BIT_POPCOUNT
CI_BIT BIT_WIDTH8, 8, CI_BIT_BIT_WIDTH
CI_BIT BIT_WIDTH16, 16, CI_BIT_BIT_WIDTH
CI_BIT BIT_WIDTH32, 32, CI_BIT_BIT_WIDTH
CI_BIT BIT_WIDTH64, 64, CI_BIT_BIT_WIDTH
CI_BIT BIT_WIDTH128, 128, CI_BIT_BIT_WIDTH
CI_BIT COUNT_ZEROS8, 8, CI_BIT_COUNT_ZEROS
CI_BIT COUNT_ZEROS16, 16, CI_BIT_COUNT_ZEROS
CI_BIT COUNT_ZEROS32, 32, CI_BIT_COUNT_ZEROS
CI_BIT COUNT_ZEROS64, 64, CI_BIT_COUNT_ZEROS
CI_BIT COUNT_ZEROS128, 128, CI_BIT_COUNT_ZEROS
CI_BIT FIRST_LEADING_ONE8, 8, CI_BIT_FIRST_LEADING_ONE
CI_BIT FIRST_LEADING_ONE16, 16, CI_BIT_FIRST_LEADING_ONE
CI_BIT FIRST_LEADING_ONE32, 32, CI_BIT_FIRST_LEADING_ONE
CI_BIT FIRST_LEADING_ONE64, 64, CI_BIT_FIRST_LEADING_ONE
CI_BIT FIRST_LEADING_ONE128, 128, CI_BIT_FIRST_LEADING_ONE
CI_BIT FIRST_LEADING_ZERO8, 8, CI_BIT_FIRST_LEADING_ZERO
CI_BIT FIRST_LEADING_ZERO16, 16, CI_BIT_FIRST_LEADING_ZERO
CI_BIT FIRST_LEADING_ZERO32, 32, CI_BIT_FIRST_LEADING_ZERO
CI_BIT FIRST_LEADING_ZERO64, 64, CI_BIT_FIRST_LEADING_ZERO
CI_BIT FIRST_LEADING_ZERO128, 128, CI_BIT_FIRST_LEADING_ZERO
CI_BIT FIRST_TRAILING_ZERO8, 8, CI_BIT_FIRST_TRAILING_ZERO
CI_BIT FIRST_TRAILING_ZERO16, 16, CI_BIT_FIRST_TRAILING_ZERO
CI_BIT FIRST_TRAILING_ZERO32, 32, CI_BIT_FIRST_TRAILING_ZERO
CI_BIT FIRST_TRAILING_ZERO64, 64, CI_BIT_FIRST_TRAILING_ZERO
CI_BIT FIRST_TRAILING_ZERO128, 128, CI_BIT_FIRST_TRAILING_ZERO
CI_BIT LEADING_ONES8, 8, CI_BIT_LEADING_ONES
CI_BIT LEADING_ONES16, 16, CI_BIT_LEADING_ONES
CI_BIT LEADING_ONES32, 32, CI_BIT_LEADING_ONES
CI_BIT LEADING_ONES64, 64, CI_BIT_LEADING_ONES
CI_BIT LEADING_ONES128, 128, CI_BIT_LEADING_ONES
CI_BIT TRAILING_ONES8, 8, CI_BIT_TRAILING_ONES
CI_BIT TRAILING_ONES16, 16, CI_BIT_TRAILING_ONES
CI_BIT TRAILING_ONES32, 32, CI_BIT_TRAILING_ONES
CI_BIT TRAILING_ONES64, 64, CI_BIT_TRAILING_ONES
CI_BIT TRAILING_ONES128, 128, CI_BIT_TRAILING_ONES

CI_BIT HAS_SINGLE_BIT8, 8, 15
CI_BIT HAS_SINGLE_BIT16, 16, 15
CI_BIT HAS_SINGLE_BIT32, 32, 15
CI_BIT HAS_SINGLE_BIT64, 64, 15
CI_BIT HAS_SINGLE_BIT128, 128, 15

CI_BIT_POWER MACRO p_name, p_bits, p_ceil
    LOCAL branch_1, branch_2, branch_3, Lci_power_one
    CI_LABEL p_name
    CI_BIT_LOAD %p_bits
    IF (p_ceil EQ 0) AND (p_bits LE 64)
        xor ecx, ecx
        IF p_bits LE 32
            bsr ecx, eax
        ELSE
            bsr rcx, rax
        ENDIF
        setnz al
        movzx eax, al
        shl rax, cl
        mov r10d, DWORD PTR [r13 + 4]
        CI_STORE %(p_bits/8), r12+r10
        CI_NEXT
        EXITM
    ENDIF
    IF p_ceil
        IF (p_bits EQ 128)
            test rdx, rdx
            jnz branch_1
        ENDIF
        cmp rax, 1
        jbe Lci_power_one
branch_1:
        sub rax, 1
        IF (p_bits EQ 128)
            sbb rdx, 0
        ENDIF
    ENDIF
    CI_CLZ %p_bits
    neg ecx
    IF p_ceil AND (p_bits LE 64)
        ; Shifting two by exponent-1 also yields zero on overflow.
        add ecx, (p_bits - 1)
        mov eax, 2
        shl rax, cl
    ELSE
        add ecx, (p_bits - 1 + p_ceil)
        xor eax, eax
        IF (p_bits EQ 128)
            xor edx, edx
        ENDIF
        cmp ecx, p_bits
        jae branch_3
        IF (p_bits EQ 128)
            cmp ecx, 64
            jb branch_2
            mov edx, 1
            shl rdx, cl
            jmp branch_3
        ENDIF
branch_2:
        mov eax, 1
        shl rax, cl
    ENDIF
    IF p_ceil
        jmp branch_3
Lci_power_one:
        mov eax, 1
        IF (p_bits EQ 128)
            xor edx, edx
        ENDIF
    ENDIF
branch_3:
    mov r10d, DWORD PTR [r13 + 4]
    CI_STORE %(p_bits/8), r12+r10
    IF (p_bits EQ 128)
        mov QWORD PTR [r12 + r10 + 8], rdx
    ENDIF
    CI_NEXT
ENDM
CI_BIT_POWER BIT_CEIL8, 8, 1
CI_BIT_POWER BIT_CEIL16, 16, 1
CI_BIT_POWER BIT_CEIL32, 32, 1
CI_BIT_POWER BIT_CEIL64, 64, 1
CI_BIT_POWER BIT_CEIL128, 128, 1

CI_BIT_POWER BIT_FLOOR8, 8, 0
CI_BIT_POWER BIT_FLOOR16, 16, 0
CI_BIT_POWER BIT_FLOOR32, 32, 0
CI_BIT_POWER BIT_FLOOR64, 64, 0
CI_BIT_POWER BIT_FLOOR128, 128, 0

CI_ROTATE MACRO p_name, p_bits, p_operand, p_right
    LOCAL branch_1
    CI_LABEL p_name
    CI_BIT_LOAD %p_bits
    mov r10d, DWORD PTR [r13 + 16]
    mov rcx, QWORD PTR [r12 + r10]
    IF (p_bits EQ 128)
        IF p_right
            neg rcx
        ENDIF
        test cl, 64
        jz branch_1
        xchg rax, rdx
branch_1:
        mov r9, rax
        shld rax, rdx, cl
        shld rdx, r9, cl
    ELSE
        IF p_right
            ror p_operand, cl
        ELSE
            rol p_operand, cl
        ENDIF
    ENDIF
    mov r10d, DWORD PTR [r13 + 4]
    CI_STORE %(p_bits/8), r12+r10
    IF (p_bits EQ 128)
        mov QWORD PTR [r12 + r10 + 8], rdx
    ENDIF
    CI_NEXT
ENDM

CI_ROTATE_WIDTH MACRO p_bits, p_operand
    CI_ROTATE ROTL&p_bits, p_bits, p_operand, 0
    CI_ROTATE ROTR&p_bits, p_bits, p_operand, 1
ENDM
CI_ROTATE_WIDTH 8, al
CI_ROTATE_WIDTH 16, ax
CI_ROTATE_WIDTH 32, eax
CI_ROTATE_WIDTH 64, rax
CI_ROTATE_WIDTH 128, rax

CI_ATOMIC_LOAD MACRO p_name, p_bits
    CI_LABEL p_name
    mov eax, DWORD PTR [r13 + 16]
    mov r11, QWORD PTR [r12 + rax]
    mov ecx, DWORD PTR [r13 + 20]
    mov r10d, DWORD PTR [r13 + 8]
    IF (p_bits EQ 128)
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
    ELSE
        CI_READ %(p_bits/8), 0, r11+rcx
        CI_STORE %(p_bits/8), r12+r10
    ENDIF
    CI_NEXT
ENDM
CI_ATOMIC_LOAD ATOMIC_LOAD8, 8
CI_ATOMIC_LOAD ATOMIC_LOAD16, 16
CI_ATOMIC_LOAD ATOMIC_LOAD32, 32
CI_ATOMIC_LOAD ATOMIC_LOAD64, 64
CI_ATOMIC_LOAD ATOMIC_LOAD128, 128

CI_ATOMIC_STORE MACRO p_name, p_bits, p_operand
    CI_LABEL p_name
    mov eax, DWORD PTR [r13 + 8]
    mov r11, QWORD PTR [r12 + rax]
    mov ecx, DWORD PTR [r13 + 20]
    CI_LOAD_SLOT 12, %(p_bits/8)
    ; XCHG supplies the store/load barrier required by a seq_cst store.
    xchg [r11 + rcx], p_operand
    CI_NEXT
ENDM
CI_ATOMIC_STORE ATOMIC_STORE8, 8, al
CI_ATOMIC_STORE ATOMIC_STORE16, 16, ax
CI_ATOMIC_STORE ATOMIC_STORE32, 32, eax
CI_ATOMIC_STORE ATOMIC_STORE64, 64, rax

CI_ATOMIC_RMW MACRO p_name, p_bits, p_operand, p_desired, p_operation
    LOCAL branch_1
    CI_LABEL p_name
    mov eax, DWORD PTR [r13 + 16]
    mov r11, QWORD PTR [r12 + rax]
    mov ecx, DWORD PTR [r13 + 4]
    add r11, rcx
    mov r10d, DWORD PTR [r13 + 8]
    CI_LOAD_SLOT 20, %(p_bits/8)
    IFIDN <p_operation>, <xchg>
        xchg [r11], p_operand
    ELSE
        IFIDN <p_operation>, <add>
            lock xadd [r11], p_operand
        ELSE
            IFIDN <p_operation>, <sub>
                neg p_operand
                lock xadd [r11], p_operand
            ELSE
                mov r9, rax
                CI_READ %(p_bits/8), 0, <r11>
branch_1:
                mov rcx, rax
                p_operation rcx, r9
                lock cmpxchg [r11], p_desired
                jne branch_1
            ENDIF
        ENDIF
    ENDIF
    CI_STORE %(p_bits/8), r12+r10
    CI_NEXT
ENDM

CI_ATOMIC_CAS MACRO p_name, p_bits, p_operand, p_desired
    CI_LABEL p_name
    mov eax, DWORD PTR [r13 + 12]
    mov r11, QWORD PTR [r12 + rax]
    mov ecx, DWORD PTR [r13 + 4]
    add r11, rcx
    CI_LOAD_SLOT 20, %(p_bits/8)
    mov rcx, rax
    mov r10d, DWORD PTR [r13 + 16]
    CI_READ %(p_bits/8), 0, r12+r10
    lock cmpxchg [r11], p_desired
    sete r9b
    CI_STORE %(p_bits/8), r12+r10
    mov eax, DWORD PTR [r13 + 8]
    mov BYTE PTR [r12 + rax], r9b
    CI_NEXT
ENDM

CI_ATOMIC_WIDTH MACRO p_bits, p_operand, p_desired
    CI_ATOMIC_RMW ATOMIC_XCHG&p_bits, p_bits, p_operand, p_desired, xchg
    CI_ATOMIC_RMW ATOMIC_ADD&p_bits, p_bits, p_operand, p_desired, add
    CI_ATOMIC_RMW ATOMIC_SUB&p_bits, p_bits, p_operand, p_desired, sub
    CI_ATOMIC_RMW ATOMIC_AND&p_bits, p_bits, p_operand, p_desired, and
    CI_ATOMIC_RMW ATOMIC_OR&p_bits, p_bits, p_operand, p_desired, or
    CI_ATOMIC_RMW ATOMIC_XOR&p_bits, p_bits, p_operand, p_desired, xor
    CI_ATOMIC_CAS ATOMIC_CAS&p_bits, p_bits, p_operand, p_desired
    CI_ATOMIC_CAS ATOMIC_CAS_WEAK&p_bits, p_bits, p_operand, p_desired
ENDM
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
Lci_atomic_store128_retry:
    lock cmpxchg16b XMMWORD PTR [r11]
    jne Lci_atomic_store128_retry
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
Lci_atomic_exchange128_retry:
    lock cmpxchg16b XMMWORD PTR [r11]
    jne Lci_atomic_exchange128_retry
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

CI_BF_EXTRACT MACRO p_bytes, p_sign, p_packed
    LOCAL Lci_bf_zero, Lci_bf_high, Lci_bf_done, Lci_bf_upper, Lci_bf_mask
    IF (p_packed EQ 0) AND (p_bytes LT 16) AND (p_sign EQ 0)
        ; The adjacent offset/width bytes are already a BEXTR control word.
        movzx ecx, WORD PTR [r13 + 2]
        CI_READ %p_bytes
        bextr rax, rax, rcx
        EXITM
    ENDIF
    movzx r10d, BYTE PTR [r13 + 3]
    test r10d, r10d
    jz Lci_bf_zero
    IF (p_packed EQ 0) AND (p_bytes LT 16)
        CI_READ %p_bytes
        movzx ecx, BYTE PTR [r13 + 2]
        shrx rax, rax, rcx
    ELSEIF (p_packed EQ 0)
        mov rdx, QWORD PTR [rax + 8]
        mov rax, QWORD PTR [rax]
        movzx ecx, BYTE PTR [r13 + 2]
        cmp ecx, 64
        jae Lci_bf_upper
        shrd rax, rdx, cl
        shr rdx, cl
        jmp Lci_bf_mask
Lci_bf_upper:
        mov rax, rdx
        shr rax, cl
        xor edx, edx
Lci_bf_mask:
    ELSE
        ; Stage only the bytes touched by the field, including a possible
        ; seventeenth byte. Never overread a packed object at a page boundary.
        mov QWORD PTR [rsp], rsi
        mov QWORD PTR [rsp + 8], rdi
        pxor xmm0, xmm0
        movdqu XMMWORD PTR [rsp + 16], xmm0
        IF p_bytes EQ 16
            mov QWORD PTR [rsp + 32], 0
        ENDIF
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
        IF p_bytes EQ 16
            mov r8, QWORD PTR [rsp + 32]
        ENDIF
        mov ecx, r9d
        shrd rax, rdx, cl
        IF p_bytes EQ 16
            shrd rdx, r8, cl
        ENDIF
    ENDIF
    IF p_bytes EQ 16
        cmp r10d, 64
        ja Lci_bf_high
    ENDIF
    IF p_sign
        mov ecx, 64
        sub ecx, r10d
        shl rax, cl
        sar rax, cl
        IF p_bytes EQ 16
            mov rdx, rax
            sar rdx, 63
        ENDIF
    ELSE
        ; BZHI preserves all 64 bits when the field fills the limb.
        bzhi rax, rax, r10
        IF p_bytes EQ 16
            xor edx, edx
        ENDIF
    ENDIF
    IF p_bytes EQ 16
        jmp Lci_bf_done
Lci_bf_high:
        IF p_sign
            mov ecx, 128
            sub ecx, r10d
            shl rdx, cl
            sar rdx, cl
        ELSE
            lea ecx, [r10 - 64]
            bzhi rdx, rdx, rcx
        ENDIF
    ENDIF
    jmp Lci_bf_done
Lci_bf_zero:
    xor eax, eax
    IF p_bytes EQ 16
        xor edx, edx
    ENDIF
Lci_bf_done:
ENDM

CI_BF_LOAD MACRO p_name, p_bytes, p_sign, p_packed, p_slot:=<0>
    CI_LABEL p_name
    CI_SLOT rax, eax, 16
    IF (p_slot EQ 0)
        mov rax, QWORD PTR [rax]
    ENDIF
    mov ecx, DWORD PTR [r13 + 20]
    add rax, rcx
    CI_BF_EXTRACT p_bytes, p_sign, p_packed
    CI_SLOT r10, r10d, 8
    CI_STORE %p_bytes
    IF (p_bytes EQ 16)
        mov QWORD PTR [r10 + 8], rdx
    ENDIF
    CI_NEXT
ENDM
CI_BF_LOAD LDBF_U8, 1, 0, 0
CI_BF_LOAD LDBF_S8, 1, 1, 0
CI_BF_LOAD LDBF_U16, 2, 0, 0
CI_BF_LOAD LDBF_U16_A, 2, 0, 0
CI_BF_LOAD LDBF_S16, 2, 1, 0
CI_BF_LOAD LDBF_S16_A, 2, 1, 0
CI_BF_LOAD LDBF_U32, 4, 0, 0
CI_BF_LOAD LDBF_U32_A, 4, 0, 0
CI_BF_LOAD LDBF_S32, 4, 1, 0
CI_BF_LOAD LDBF_S32_A, 4, 1, 0
CI_BF_LOAD LDBF_U64, 8, 0, 0
CI_BF_LOAD LDBF_U64_A, 8, 0, 0
CI_BF_LOAD LDBF_S64, 8, 1, 0
CI_BF_LOAD LDBF_S64_A, 8, 1, 0

CI_BF_LOAD LDBF_U128, 16, %(1-1), 0
CI_BF_LOAD LDBF_U128_A, 16, %(1-1), 0
CI_BF_LOAD LDBF_S128, 16, %(1-0), 0
CI_BF_LOAD LDBF_S128_A, 16, %(1-0), 0

CI_BF_LOAD LDBFB_U8, 1, 0, 1
CI_BF_LOAD LDBFB_S8, 1, 1, 1
CI_BF_LOAD LDBFB_U16, 2, 0, 1
CI_BF_LOAD LDBFB_S16, 2, 1, 1
CI_BF_LOAD LDBFB_U32, 4, 0, 1
CI_BF_LOAD LDBFB_S32, 4, 1, 1
CI_BF_LOAD LDBFB_U64, 8, 0, 1
CI_BF_LOAD LDBFB_S64, 8, 1, 1

CI_BF_LOAD LDBFB_U128, 16, %(1-1), 1
CI_BF_LOAD LDBFB_S128, 16, %(1-0), 1

CI_BF_LOAD LDBFS_U8, 1, 0, 0, 1
CI_BF_LOAD LDBFS_S8, 1, 1, 0, 1
CI_BF_LOAD LDBFS_U16, 2, 0, 0, 1
CI_BF_LOAD LDBFS_U16_A, 2, 0, 0, 1
CI_BF_LOAD LDBFS_S16, 2, 1, 0, 1
CI_BF_LOAD LDBFS_S16_A, 2, 1, 0, 1
CI_BF_LOAD LDBFS_U32, 4, 0, 0, 1
CI_BF_LOAD LDBFS_U32_A, 4, 0, 0, 1
CI_BF_LOAD LDBFS_S32, 4, 1, 0, 1
CI_BF_LOAD LDBFS_S32_A, 4, 1, 0, 1
CI_BF_LOAD LDBFS_U64, 8, 0, 0, 1
CI_BF_LOAD LDBFS_U64_A, 8, 0, 0, 1
CI_BF_LOAD LDBFS_S64, 8, 1, 0, 1
CI_BF_LOAD LDBFS_S64_A, 8, 1, 0, 1

CI_BF_LOAD LDBFS_U128, 16, %(1-1), 0, 1
CI_BF_LOAD LDBFS_U128_A, 16, %(1-1), 0, 1
CI_BF_LOAD LDBFS_S128, 16, %(1-0), 0, 1
CI_BF_LOAD LDBFS_S128_A, 16, %(1-0), 0, 1

CI_BF_LOAD LDBFBS_U8, 1, 0, 1, 1
CI_BF_LOAD LDBFBS_S8, 1, 1, 1, 1
CI_BF_LOAD LDBFBS_U16, 2, 0, 1, 1
CI_BF_LOAD LDBFBS_S16, 2, 1, 1, 1
CI_BF_LOAD LDBFBS_U32, 4, 0, 1, 1
CI_BF_LOAD LDBFBS_S32, 4, 1, 1, 1
CI_BF_LOAD LDBFBS_U64, 8, 0, 1, 1
CI_BF_LOAD LDBFBS_S64, 8, 1, 1, 1

CI_BF_LOAD LDBFBS_U128, 16, %(1-1), 1, 1
CI_BF_LOAD LDBFBS_S128, 16, %(1-0), 1, 1

CI_BF_STORE MACRO p_name, p_bytes, p_packed
    LOCAL Lci_bf_write, Lci_bf_mask_low, Lci_bf_mask_ready, Lci_bf_shift_upper, Lci_bf_merge
    CI_LABEL p_name
    CI_SLOT rax, eax, 12
    IF (p_bytes EQ 16)
        mov rdx, QWORD PTR [rax + 8]
        mov rax, QWORD PTR [rax]
    ELSE
        CI_READ %p_bytes
        xor edx, edx
    ENDIF
    CI_SLOT r10, r10d, 8
    mov r8, QWORD PTR [r10]
    mov ecx, DWORD PTR [r13 + 20]
    add r8, rcx
    movzx ecx, BYTE PTR [r13 + 2]
    movzx r10d, BYTE PTR [r13 + 3]
    test r10d, r10d
    jz Lci_next
    IF (p_packed EQ 0) AND (p_bytes LT 16)
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
        CI_READ %p_bytes
        and rax, r9
        or rax, rdx
        mov r10, r8
        CI_STORE %p_bytes
    ELSEIF (p_packed EQ 0)
        mov r9, -1
        xor r11d, r11d
        cmp r10d, 64
        jbe Lci_bf_mask_low
        mov r11, r9
        sub r10d, 64
        bzhi r11, r11, r10
        jmp Lci_bf_mask_ready
Lci_bf_mask_low:
        bzhi r9, r9, r10
Lci_bf_mask_ready:
        and rax, r9
        and rdx, r11
        cmp ecx, 64
        jae Lci_bf_shift_upper
        shld rdx, rax, cl
        shl rax, cl
        shld r11, r9, cl
        shl r9, cl
        jmp Lci_bf_merge
Lci_bf_shift_upper:
        mov rdx, rax
        shl rdx, cl
        xor eax, eax
        mov r11, r9
        shl r11, cl
        xor r9d, r9d
Lci_bf_merge:
        not r9
        not r11
        and r9, QWORD PTR [r8]
        and r11, QWORD PTR [r8 + 8]
        or rax, r9
        or rdx, r11
        mov QWORD PTR [r8], rax
        mov QWORD PTR [r8 + 8], rdx
    ELSE
        mov QWORD PTR [rsp + 8], r10
        mov r9d, ecx
        shr ecx, 3
        add r8, rcx
        and r9d, 7
        mov DWORD PTR [rsp], r9d
Lci_bf_write:
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
        IF p_bytes EQ 16
            shrd rax, rdx, cl
            shr rdx, cl
        ELSE
            shr rax, cl
        ENDIF
        mov DWORD PTR [rsp], 0
        inc r8
        sub DWORD PTR [rsp + 8], r11d
        jnz Lci_bf_write
    ENDIF
    CI_NEXT
ENDM
CI_BF_STORE STBF8, 1, 0
CI_BF_STORE STBF16, 2, 0
CI_BF_STORE STBF16_A, 2, 0
CI_BF_STORE STBF32, 4, 0
CI_BF_STORE STBF32_A, 4, 0
CI_BF_STORE STBF64, 8, 0
CI_BF_STORE STBF64_A, 8, 0

CI_BF_STORE STBF128, 16, 0
CI_BF_STORE STBF128_A, 16, 0

CI_BF_STORE STBFB8, 1, 1
CI_BF_STORE STBFB16, 2, 1
CI_BF_STORE STBFB32, 4, 1
CI_BF_STORE STBFB64, 8, 1

CI_BF_STORE STBFB128, 16, 1

CI_BF_JUMP MACRO p_name, p_bytes, p_slot, p_nonzero
    CI_LABEL p_name
    CI_SLOT rax, eax, 8
    IF (p_slot EQ 0)
        mov rax, QWORD PTR [rax]
    ENDIF
    mov ecx, DWORD PTR [r13 + 12]
    add rax, rcx
    IF p_bytes
        movzx ecx, WORD PTR [r13 + 2]
        CI_READ %p_bytes
        bextr rax, rax, rcx
    ELSE
        CI_BF_EXTRACT 16, 0, 1
        or rax, rdx
    ENDIF
    IF p_nonzero
        jnz Lci_JUMP
    ELSE
        jz Lci_JUMP
    ENDIF
    CI_NEXT
ENDM
CI_BF_JUMP BF_JZ8, 1, 0, 0
CI_BF_JUMP BF_JNZ8, 1, 0, 1
CI_BF_JUMP BF_JZ16, 2, 0, 0
CI_BF_JUMP BF_JNZ16, 2, 0, 1
CI_BF_JUMP BF_JZ32, 4, 0, 0
CI_BF_JUMP BF_JNZ32, 4, 0, 1
CI_BF_JUMP BF_JZ64, 8, 0, 0
CI_BF_JUMP BF_JNZ64, 8, 0, 1
CI_BF_JUMP BF_JZ_BYTES, 0, 0, 0
CI_BF_JUMP BF_JNZ_BYTES, 0, 0, 1
CI_BF_JUMP BFS_JZ8, 1, 1, 0
CI_BF_JUMP BFS_JNZ8, 1, 1, 1
CI_BF_JUMP BFS_JZ16, 2, 1, 0
CI_BF_JUMP BFS_JNZ16, 2, 1, 1
CI_BF_JUMP BFS_JZ32, 4, 1, 0
CI_BF_JUMP BFS_JNZ32, 4, 1, 1
CI_BF_JUMP BFS_JZ64, 8, 1, 0
CI_BF_JUMP BFS_JNZ64, 8, 1, 1
CI_BF_JUMP BFS_JZ_BYTES, 0, 1, 0
CI_BF_JUMP BFS_JNZ_BYTES, 0, 1, 1

CI_ADDR_SLOT EQU 0
CI_ADDR_REG EQU 1
CI_ADDR_INCOMING EQU 2
CI_ADDR_REG_PTR EQU 3
CI_ADDR_INCOMING_PTR EQU 4
CI_ADDR_REG_PTR_INDIRECT EQU 5
CI_ADDR_SLOT_PTR EQU 7
CI_ADDR_INCOMING_SAVE EQU 8
CI_XFER_COPY EQU 0
CI_XFER_ADDRESS EQU 1
CI_XFER_ZERO EQU 2
CI_XFER_CONSTANT EQU 3

; Copy: R10 = src, R11 = dst, RAX = size. Overlap permitted.
; Zero: R11 = dst, RAX = size.
; Clobbers: RAX, R10, R11, XMM4, flags. Preserves argument registers.
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
    movdqu xmm4, XMMWORD PTR [r10]
    movdqu XMMWORD PTR [r11], xmm4
    sub rax, 16
    jmp Lci_abi_copy_backward
Lci_abi_copy_backward_tail:
    test rax, rax
    jz Lci_abi_copy_done
    dec r10
    dec r11
    pinsrb xmm4, BYTE PTR [r10], 0
    pextrb BYTE PTR [r11], xmm4, 0
    dec rax
    jmp Lci_abi_copy_backward_tail
Lci_abi_copy_forward:
    cmp rax, 16
    jb Lci_abi_copy_forward_tail
    movdqu xmm4, XMMWORD PTR [r10]
    movdqu XMMWORD PTR [r11], xmm4
    add r10, 16
    add r11, 16
    sub rax, 16
    jmp Lci_abi_copy_forward
Lci_abi_copy_forward_tail:
    test rax, rax
    jz Lci_abi_copy_done
    pinsrb xmm4, BYTE PTR [r10], 0
    pextrb BYTE PTR [r11], xmm4, 0
    inc r10
    inc r11
    dec rax
    jmp Lci_abi_copy_forward_tail
Lci_abi_copy_done:
    ret
Lci_abi_zero:
    pxor xmm4, xmm4
Lci_abi_zero_chunks:
    cmp rax, 16
    jb Lci_abi_zero_tail
    movdqu XMMWORD PTR [r11], xmm4
    add r11, 16
    sub rax, 16
    jmp Lci_abi_zero_chunks
Lci_abi_zero_tail:
    test rax, rax
    jz Lci_abi_zero_done
    mov BYTE PTR [r11], 0
    inc r11
    dec rax
    jmp Lci_abi_zero_tail
Lci_abi_zero_done:
    ret

; Argument offsets select registers; result/metadata offsets select CiStack.
; Get: R11D = offset -> XMM4. Clobbers flags.
; Put: R11D = offset, EAX = size, XMM4 = value.
; Put clobbers RAX, R10, XMM4, flags, and the destination.
Lci_abi_get_bank:
    FOR p_reg, <0, 1, 2, 3>
        cmp r11d, (p_reg*8)
        je Lci_abi_get_gp&p_reg
    ENDM
    FOR p_reg, <0, 1, 2, 3>
        cmp r11d, (32 + p_reg*16)
        je Lci_abi_get_fp&p_reg
    ENDM
    movdqu xmm4, XMMWORD PTR [r15 + r11]
    ret
    FOR p_reg, <0, 1, 2, 3>
Lci_abi_get_gp&p_reg:
        CI_ARG_GP p_reg, movq, xmm4, 1
        ret
    ENDM
    FOR p_reg, <0, 1, 2, 3>
Lci_abi_get_fp&p_reg:
        movdqa xmm4, xmm&p_reg
        ret
    ENDM
Lci_abi_put_bank:
    FOR p_reg, <0, 1, 2, 3>
        cmp r11d, (p_reg*8)
        je Lci_abi_put_gp&p_reg
    ENDM
    FOR p_reg, <0, 1, 2, 3>
        cmp r11d, (32 + p_reg*16)
        je Lci_abi_put_fp&p_reg
    ENDM
    lea r10, [r15 + r11]
    cmp eax, 16
    je Lci_abi_put_16
    cmp eax, 8
    je Lci_abi_put_8
    cmp eax, 4
    je Lci_abi_put_4
    cmp eax, 2
    je Lci_abi_put_2
    cmp eax, 1
    je Lci_abi_put_1
Lci_abi_put_bytes:
    test eax, eax
    jz Lci_abi_put_done
    pextrb BYTE PTR [r10], xmm4, 0
    psrldq xmm4, 1
    inc r10
    dec eax
    jmp Lci_abi_put_bytes
Lci_abi_put_16:
    movdqu XMMWORD PTR [r10], xmm4
    ret
Lci_abi_put_8:
    movq QWORD PTR [r10], xmm4
    ret
Lci_abi_put_4:
    movd DWORD PTR [r10], xmm4
Lci_abi_put_done:
    ret
Lci_abi_put_2:
    pextrw WORD PTR [r10], xmm4, 0
    ret
Lci_abi_put_1:
    pextrb BYTE PTR [r10], xmm4, 0
    ret
    FOR p_reg, <0, 1, 2, 3>
Lci_abi_put_gp&p_reg:
        CI_ARG_GP p_reg, movq, xmm4
        ret
    ENDM
    FOR p_reg, <0, 1, 2, 3>
Lci_abi_put_fp&p_reg:
        movdqa xmm&p_reg, xmm4
        ret
    ENDM

CI_ABI_VALUE_LOAD MACRO p_bytes
    IF p_bytes EQ 16
        movdqu xmm4, XMMWORD PTR [r10]
    ELSEIF p_bytes EQ 8
        movq xmm4, QWORD PTR [r10]
    ELSEIF p_bytes EQ 4
        movd xmm4, DWORD PTR [r10]
    ELSEIF p_bytes EQ 2
        movzx r11d, WORD PTR [r10]
        movq xmm4, r11
    ELSEIF p_bytes EQ 1
        movzx r11d, BYTE PTR [r10]
        movq xmm4, r11
    ELSE
        pxor xmm4, xmm4
        lea r11, [rbp - CI_FRAME_OFFSET]
        movdqu XMMWORD PTR [r11], xmm4
        mov eax, DWORD PTR [r13]
        shr eax, 18
        and eax, 31
        call Lci_abi_copy
        movdqu xmm4, XMMWORD PTR [rbp - CI_FRAME_OFFSET]
    ENDIF
ENDM
CI_ABI_VALUE_STORE MACRO p_bytes
    IF p_bytes EQ 16
        movdqu XMMWORD PTR [r10], xmm4
    ELSEIF p_bytes EQ 8
        movq QWORD PTR [r10], xmm4
    ELSEIF p_bytes EQ 4
        movd DWORD PTR [r10], xmm4
    ELSEIF p_bytes EQ 2
        pextrw WORD PTR [r10], xmm4, 0
    ELSEIF p_bytes EQ 1
        pextrb BYTE PTR [r10], xmm4, 0
    ELSE
        movdqu XMMWORD PTR [rbp - CI_FRAME_OFFSET], xmm4
        mov r11, r10
        lea r10, [rbp - CI_FRAME_OFFSET]
        mov eax, DWORD PTR [r13]
        shr eax, 18
        and eax, 31
        call Lci_abi_copy
    ENDIF
ENDM
CI_ABI_ADDRESS MACRO p_mode, p_offset, p_pointer
    IF p_mode EQ 0
        CI_SLOT rax, eax, p_offset
    ELSEIF p_mode EQ 1
        movzx r11d, WORD PTR [r13 + p_offset + 2]
        call Lci_abi_get_bank
        movdqu XMMWORD PTR [rsp], xmm4
        lea rax, [rsp]
    ELSEIF p_mode EQ 2
        movzx r11d, WORD PTR [r13 + p_offset + 2]
        mov rax, QWORD PTR [r12 + CI_ASM_incoming - CI_ASM_frame_size]
        add rax, r11
    ELSEIF p_mode EQ 7
        mov eax, DWORD PTR [r13 + p_pointer]
        mov rax, QWORD PTR [r12 + rax]
        movsxd r11, DWORD PTR [r13 + p_offset]
        add rax, r11
    ELSE
        movzx r11d, WORD PTR [r13 + p_pointer + 2]
        IF (p_mode EQ 3) OR (p_mode EQ 5)
            call Lci_abi_get_bank
            movq rax, xmm4
        ELSE
            mov rax, QWORD PTR [r12 + CI_ASM_incoming - CI_ASM_frame_size]
            add rax, r11
            IF p_mode LT 8
                mov rax, QWORD PTR [rax]
            ENDIF
        ENDIF
        IF p_mode LT 8
            movsxd r11, DWORD PTR [r13 + p_offset]
        ELSE
            mov r11d, DWORD PTR [r13 + p_offset]
        ENDIF
        add rax, r11
        IF (p_mode EQ 5) OR (p_mode EQ 6)
            mov rax, QWORD PTR [rax]
        ENDIF
    ENDIF
ENDM
CI_ABI_TRANSFER MACRO p_name, p_operation, p_src, p_dst, p_bytes
    CI_LABEL p_name
    IF p_dst NE 1
        CI_ABI_ADDRESS %p_dst, 8, 20
        mov QWORD PTR [rsp + 16], rax
    ENDIF
    IF p_operation EQ 3
        mov eax, DWORD PTR [r13 + 4]
        movq xmm4, rax
    ELSEIF p_operation EQ 2
        IF (p_dst NE 1) AND (p_bytes EQ 0)
            mov r11, QWORD PTR [rsp + 16]
            mov eax, DWORD PTR [r13 + 12]
            call Lci_abi_zero
            CI_NEXT
            EXITM
        ENDIF
        pxor xmm4, xmm4
    ELSE
        CI_ABI_ADDRESS %p_src, 4, 16
        IF p_operation EQ 1
            movq xmm4, rax
        ELSEIF p_bytes EQ 0
            mov r10, rax
            mov r11, QWORD PTR [rsp + 16]
            mov eax, DWORD PTR [r13 + 12]
            call Lci_abi_copy
            CI_NEXT
            EXITM
        ELSE
            mov r10, rax
            CI_ABI_VALUE_LOAD %p_bytes
        ENDIF
    ENDIF
    IF p_dst EQ 1
        movzx r11d, WORD PTR [r13 + 10]
        IF p_operation EQ 1
            mov eax, 8
        ELSE
            mov eax, p_bytes
        ENDIF
        call Lci_abi_put_bank
    ELSE
        mov r10, QWORD PTR [rsp + 16]
        IF p_operation EQ 1
            CI_ABI_VALUE_STORE 8
        ELSE
            CI_ABI_VALUE_STORE %p_bytes
        ENDIF
    ENDIF
    CI_NEXT
ENDM
CI_ABI_TRANSFER ABI_COPY_SLOT_TO_REG_PTR_8, CI_XFER_COPY, CI_ADDR_SLOT, CI_ADDR_REG_PTR, 1
CI_ABI_TRANSFER ABI_COPY_SLOT_TO_REG_PTR_16, CI_XFER_COPY, CI_ADDR_SLOT, CI_ADDR_REG_PTR, 2
CI_ABI_TRANSFER ABI_COPY_SLOT_TO_REG_PTR_16_A, CI_XFER_COPY, CI_ADDR_SLOT, CI_ADDR_REG_PTR, 2
CI_ABI_TRANSFER ABI_COPY_SLOT_TO_REG_PTR_32, CI_XFER_COPY, CI_ADDR_SLOT, CI_ADDR_REG_PTR, 4
CI_ABI_TRANSFER ABI_COPY_SLOT_TO_REG_PTR_32_A, CI_XFER_COPY, CI_ADDR_SLOT, CI_ADDR_REG_PTR, 4
CI_ABI_TRANSFER ABI_COPY_SLOT_TO_REG_PTR_64, CI_XFER_COPY, CI_ADDR_SLOT, CI_ADDR_REG_PTR, 8
CI_ABI_TRANSFER ABI_COPY_SLOT_TO_REG_PTR_64_A, CI_XFER_COPY, CI_ADDR_SLOT, CI_ADDR_REG_PTR, 8
CI_ABI_TRANSFER ABI_COPY_SLOT_TO_REG_PTR_128, CI_XFER_COPY, CI_ADDR_SLOT, CI_ADDR_REG_PTR, 16
CI_ABI_TRANSFER ABI_COPY_SLOT_TO_REG_PTR_128_A, CI_XFER_COPY, CI_ADDR_SLOT, CI_ADDR_REG_PTR, 16
CI_ABI_TRANSFER ABI_COPY_SLOT_TO_REG_PTR_N, CI_XFER_COPY, CI_ADDR_SLOT, CI_ADDR_REG_PTR, 0
CI_ABI_TRANSFER ABI_COPY_SLOT_TO_REG_PTR_N_A2, CI_XFER_COPY, CI_ADDR_SLOT, CI_ADDR_REG_PTR, 0
CI_ABI_TRANSFER ABI_COPY_SLOT_TO_REG_PTR_N_A4, CI_XFER_COPY, CI_ADDR_SLOT, CI_ADDR_REG_PTR, 0
CI_ABI_TRANSFER ABI_COPY_SLOT_TO_REG_PTR_N_A8, CI_XFER_COPY, CI_ADDR_SLOT, CI_ADDR_REG_PTR, 0
CI_ABI_TRANSFER ABI_COPY_SLOT_TO_REG_PTR_N_A16, CI_XFER_COPY, CI_ADDR_SLOT, CI_ADDR_REG_PTR, 0
CI_ABI_TRANSFER ABI_COPY_REG_PTR_TO_SLOT_8, CI_XFER_COPY, CI_ADDR_REG_PTR, CI_ADDR_SLOT, 1
CI_ABI_TRANSFER ABI_COPY_REG_PTR_TO_SLOT_16, CI_XFER_COPY, CI_ADDR_REG_PTR, CI_ADDR_SLOT, 2
CI_ABI_TRANSFER ABI_COPY_REG_PTR_TO_SLOT_16_A, CI_XFER_COPY, CI_ADDR_REG_PTR, CI_ADDR_SLOT, 2
CI_ABI_TRANSFER ABI_COPY_REG_PTR_TO_SLOT_32, CI_XFER_COPY, CI_ADDR_REG_PTR, CI_ADDR_SLOT, 4
CI_ABI_TRANSFER ABI_COPY_REG_PTR_TO_SLOT_32_A, CI_XFER_COPY, CI_ADDR_REG_PTR, CI_ADDR_SLOT, 4
CI_ABI_TRANSFER ABI_COPY_REG_PTR_TO_SLOT_64, CI_XFER_COPY, CI_ADDR_REG_PTR, CI_ADDR_SLOT, 8
CI_ABI_TRANSFER ABI_COPY_REG_PTR_TO_SLOT_64_A, CI_XFER_COPY, CI_ADDR_REG_PTR, CI_ADDR_SLOT, 8
CI_ABI_TRANSFER ABI_COPY_REG_PTR_TO_SLOT_128, CI_XFER_COPY, CI_ADDR_REG_PTR, CI_ADDR_SLOT, 16
CI_ABI_TRANSFER ABI_COPY_REG_PTR_TO_SLOT_128_A, CI_XFER_COPY, CI_ADDR_REG_PTR, CI_ADDR_SLOT, 16
CI_ABI_TRANSFER ABI_COPY_REG_PTR_TO_SLOT_N, CI_XFER_COPY, CI_ADDR_REG_PTR, CI_ADDR_SLOT, 0
CI_ABI_TRANSFER ABI_COPY_REG_PTR_TO_SLOT_N_A2, CI_XFER_COPY, CI_ADDR_REG_PTR, CI_ADDR_SLOT, 0
CI_ABI_TRANSFER ABI_COPY_REG_PTR_TO_SLOT_N_A4, CI_XFER_COPY, CI_ADDR_REG_PTR, CI_ADDR_SLOT, 0
CI_ABI_TRANSFER ABI_COPY_REG_PTR_TO_SLOT_N_A8, CI_XFER_COPY, CI_ADDR_REG_PTR, CI_ADDR_SLOT, 0
CI_ABI_TRANSFER ABI_COPY_REG_PTR_TO_SLOT_N_A16, CI_XFER_COPY, CI_ADDR_REG_PTR, CI_ADDR_SLOT, 0
CI_ABI_TRANSFER ABI_COPY_REG_PTR_INDIRECT_TO_SLOT_8, CI_XFER_COPY, CI_ADDR_REG_PTR_INDIRECT, CI_ADDR_SLOT, 1
CI_ABI_TRANSFER ABI_COPY_REG_PTR_INDIRECT_TO_SLOT_16, CI_XFER_COPY, CI_ADDR_REG_PTR_INDIRECT, CI_ADDR_SLOT, 2
CI_ABI_TRANSFER ABI_COPY_REG_PTR_INDIRECT_TO_SLOT_16_A, CI_XFER_COPY, CI_ADDR_REG_PTR_INDIRECT, CI_ADDR_SLOT, 2
CI_ABI_TRANSFER ABI_COPY_REG_PTR_INDIRECT_TO_SLOT_32, CI_XFER_COPY, CI_ADDR_REG_PTR_INDIRECT, CI_ADDR_SLOT, 4
CI_ABI_TRANSFER ABI_COPY_REG_PTR_INDIRECT_TO_SLOT_32_A, CI_XFER_COPY, CI_ADDR_REG_PTR_INDIRECT, CI_ADDR_SLOT, 4
CI_ABI_TRANSFER ABI_COPY_REG_PTR_INDIRECT_TO_SLOT_64, CI_XFER_COPY, CI_ADDR_REG_PTR_INDIRECT, CI_ADDR_SLOT, 8
CI_ABI_TRANSFER ABI_COPY_REG_PTR_INDIRECT_TO_SLOT_64_A, CI_XFER_COPY, CI_ADDR_REG_PTR_INDIRECT, CI_ADDR_SLOT, 8
CI_ABI_TRANSFER ABI_COPY_REG_PTR_INDIRECT_TO_SLOT_128, CI_XFER_COPY, CI_ADDR_REG_PTR_INDIRECT, CI_ADDR_SLOT, 16
CI_ABI_TRANSFER ABI_COPY_REG_PTR_INDIRECT_TO_SLOT_128_A, CI_XFER_COPY, CI_ADDR_REG_PTR_INDIRECT, CI_ADDR_SLOT, 16
CI_ABI_TRANSFER ABI_COPY_REG_PTR_INDIRECT_TO_SLOT_N, CI_XFER_COPY, CI_ADDR_REG_PTR_INDIRECT, CI_ADDR_SLOT, 0
CI_ABI_TRANSFER ABI_COPY_REG_PTR_INDIRECT_TO_SLOT_N_A2, CI_XFER_COPY, CI_ADDR_REG_PTR_INDIRECT, CI_ADDR_SLOT, 0
CI_ABI_TRANSFER ABI_COPY_REG_PTR_INDIRECT_TO_SLOT_N_A4, CI_XFER_COPY, CI_ADDR_REG_PTR_INDIRECT, CI_ADDR_SLOT, 0
CI_ABI_TRANSFER ABI_COPY_REG_PTR_INDIRECT_TO_SLOT_N_A8, CI_XFER_COPY, CI_ADDR_REG_PTR_INDIRECT, CI_ADDR_SLOT, 0
CI_ABI_TRANSFER ABI_COPY_REG_PTR_INDIRECT_TO_SLOT_N_A16, CI_XFER_COPY, CI_ADDR_REG_PTR_INDIRECT, CI_ADDR_SLOT, 0
CI_ABI_TRANSFER ABI_COPY_SLOT_TO_SLOT_PTR_8, CI_XFER_COPY, CI_ADDR_SLOT, CI_ADDR_SLOT_PTR, 1
CI_ABI_TRANSFER ABI_COPY_SLOT_TO_SLOT_PTR_16, CI_XFER_COPY, CI_ADDR_SLOT, CI_ADDR_SLOT_PTR, 2
CI_ABI_TRANSFER ABI_COPY_SLOT_TO_SLOT_PTR_16_A, CI_XFER_COPY, CI_ADDR_SLOT, CI_ADDR_SLOT_PTR, 2
CI_ABI_TRANSFER ABI_COPY_SLOT_TO_SLOT_PTR_32, CI_XFER_COPY, CI_ADDR_SLOT, CI_ADDR_SLOT_PTR, 4
CI_ABI_TRANSFER ABI_COPY_SLOT_TO_SLOT_PTR_32_A, CI_XFER_COPY, CI_ADDR_SLOT, CI_ADDR_SLOT_PTR, 4
CI_ABI_TRANSFER ABI_COPY_SLOT_TO_SLOT_PTR_64, CI_XFER_COPY, CI_ADDR_SLOT, CI_ADDR_SLOT_PTR, 8
CI_ABI_TRANSFER ABI_COPY_SLOT_TO_SLOT_PTR_64_A, CI_XFER_COPY, CI_ADDR_SLOT, CI_ADDR_SLOT_PTR, 8
CI_ABI_TRANSFER ABI_COPY_SLOT_TO_SLOT_PTR_128, CI_XFER_COPY, CI_ADDR_SLOT, CI_ADDR_SLOT_PTR, 16
CI_ABI_TRANSFER ABI_COPY_SLOT_TO_SLOT_PTR_128_A, CI_XFER_COPY, CI_ADDR_SLOT, CI_ADDR_SLOT_PTR, 16
CI_ABI_TRANSFER ABI_COPY_SLOT_TO_SLOT_PTR_N, CI_XFER_COPY, CI_ADDR_SLOT, CI_ADDR_SLOT_PTR, 0
CI_ABI_TRANSFER ABI_COPY_SLOT_TO_SLOT_PTR_N_A2, CI_XFER_COPY, CI_ADDR_SLOT, CI_ADDR_SLOT_PTR, 0
CI_ABI_TRANSFER ABI_COPY_SLOT_TO_SLOT_PTR_N_A4, CI_XFER_COPY, CI_ADDR_SLOT, CI_ADDR_SLOT_PTR, 0
CI_ABI_TRANSFER ABI_COPY_SLOT_TO_SLOT_PTR_N_A8, CI_XFER_COPY, CI_ADDR_SLOT, CI_ADDR_SLOT_PTR, 0
CI_ABI_TRANSFER ABI_COPY_SLOT_TO_SLOT_PTR_N_A16, CI_XFER_COPY, CI_ADDR_SLOT, CI_ADDR_SLOT_PTR, 0
CI_ABI_TRANSFER ABI_COPY_INCOMING_TO_INCOMING_PTR_64_A, CI_XFER_COPY, CI_ADDR_INCOMING, CI_ADDR_INCOMING_PTR, 8
CI_ABI_TRANSFER ABI_ADDRESS_REG_PTR_TO_REG_PTR, CI_XFER_ADDRESS, CI_ADDR_REG_PTR, CI_ADDR_REG, 0
CI_ABI_TRANSFER ABI_ADDRESS_REG_PTR_TO_REG_PTR_A, CI_XFER_ADDRESS, CI_ADDR_REG_PTR, CI_ADDR_REG, 0
CI_ABI_TRANSFER ABI_ADDRESS_REG_PTR_TO_REG_PTR_PTR, CI_XFER_ADDRESS, CI_ADDR_REG_PTR, CI_ADDR_REG_PTR, 0
CI_ABI_TRANSFER ABI_ADDRESS_REG_PTR_TO_REG_PTR_PTR_A, CI_XFER_ADDRESS, CI_ADDR_REG_PTR, CI_ADDR_REG_PTR, 0
CI_ABI_TRANSFER ABI_ADDRESS_SLOT_PTR_TO_REG_PTR, CI_XFER_ADDRESS, CI_ADDR_SLOT_PTR, CI_ADDR_REG, 0
CI_ABI_TRANSFER ABI_ADDRESS_SLOT_PTR_TO_REG_PTR_A, CI_XFER_ADDRESS, CI_ADDR_SLOT_PTR, CI_ADDR_REG, 0
CI_ABI_TRANSFER ABI_ADDRESS_SLOT_TO_REG_PTR, CI_XFER_ADDRESS, CI_ADDR_SLOT, CI_ADDR_REG, 0
CI_ABI_TRANSFER ABI_ADDRESS_SLOT_TO_REG_PTR_A, CI_XFER_ADDRESS, CI_ADDR_SLOT, CI_ADDR_REG, 0
CI_ABI_TRANSFER ABI_ADDRESS_SLOT_TO_SLOT_PTR, CI_XFER_ADDRESS, CI_ADDR_SLOT, CI_ADDR_SLOT, 0
CI_ABI_TRANSFER ABI_ADDRESS_SLOT_TO_SLOT_PTR_A, CI_XFER_ADDRESS, CI_ADDR_SLOT, CI_ADDR_SLOT, 0
CI_ABI_TRANSFER ABI_ADDRESS_INCOMING_PTR_TO_SLOT_PTR_PTR, CI_XFER_ADDRESS, CI_ADDR_INCOMING_PTR, CI_ADDR_SLOT_PTR, 0
CI_ABI_TRANSFER ABI_ADDRESS_INCOMING_PTR_TO_SLOT_PTR_PTR_A, CI_XFER_ADDRESS, CI_ADDR_INCOMING_PTR, CI_ADDR_SLOT_PTR, 0
CI_ABI_TRANSFER ABI_ADDRESS_INCOMING_SAVE_TO_SLOT_PTR_PTR, CI_XFER_ADDRESS, CI_ADDR_INCOMING_SAVE, CI_ADDR_SLOT_PTR, 0
CI_ABI_TRANSFER ABI_ADDRESS_INCOMING_SAVE_TO_SLOT_PTR_PTR_A, CI_XFER_ADDRESS, CI_ADDR_INCOMING_SAVE, CI_ADDR_SLOT_PTR, 0
CI_ABI_TRANSFER ABI_ZERO_SLOT_8, CI_XFER_ZERO, CI_ADDR_SLOT, CI_ADDR_SLOT, 1
CI_ABI_TRANSFER ABI_ZERO_SLOT_16, CI_XFER_ZERO, CI_ADDR_SLOT, CI_ADDR_SLOT, 2
CI_ABI_TRANSFER ABI_ZERO_SLOT_16_A, CI_XFER_ZERO, CI_ADDR_SLOT, CI_ADDR_SLOT, 2
CI_ABI_TRANSFER ABI_ZERO_SLOT_32, CI_XFER_ZERO, CI_ADDR_SLOT, CI_ADDR_SLOT, 4
CI_ABI_TRANSFER ABI_ZERO_SLOT_32_A, CI_XFER_ZERO, CI_ADDR_SLOT, CI_ADDR_SLOT, 4
CI_ABI_TRANSFER ABI_ZERO_SLOT_64, CI_XFER_ZERO, CI_ADDR_SLOT, CI_ADDR_SLOT, 8
CI_ABI_TRANSFER ABI_ZERO_SLOT_64_A, CI_XFER_ZERO, CI_ADDR_SLOT, CI_ADDR_SLOT, 8
CI_ABI_TRANSFER ABI_ZERO_SLOT_128, CI_XFER_ZERO, CI_ADDR_SLOT, CI_ADDR_SLOT, 16
CI_ABI_TRANSFER ABI_ZERO_SLOT_128_A, CI_XFER_ZERO, CI_ADDR_SLOT, CI_ADDR_SLOT, 16
CI_ABI_TRANSFER ABI_ZERO_SLOT_N, CI_XFER_ZERO, CI_ADDR_SLOT, CI_ADDR_SLOT, 0
CI_ABI_TRANSFER ABI_ZERO_SLOT_N_A2, CI_XFER_ZERO, CI_ADDR_SLOT, CI_ADDR_SLOT, 0
CI_ABI_TRANSFER ABI_ZERO_SLOT_N_A4, CI_XFER_ZERO, CI_ADDR_SLOT, CI_ADDR_SLOT, 0
CI_ABI_TRANSFER ABI_ZERO_SLOT_N_A8, CI_XFER_ZERO, CI_ADDR_SLOT, CI_ADDR_SLOT, 0
CI_ABI_TRANSFER ABI_ZERO_SLOT_N_A16, CI_XFER_ZERO, CI_ADDR_SLOT, CI_ADDR_SLOT, 0
CI_ABI_TRANSFER ABI_CONSTANT_REG_8, CI_XFER_CONSTANT, CI_ADDR_SLOT, CI_ADDR_REG, 1
CI_ABI_TRANSFER ABI_CONSTANT_SLOT_PTR_32, CI_XFER_CONSTANT, CI_ADDR_SLOT, CI_ADDR_SLOT_PTR, 4
CI_ABI_TRANSFER ABI_CONSTANT_SLOT_PTR_32_A, CI_XFER_CONSTANT, CI_ADDR_SLOT, CI_ADDR_SLOT_PTR, 4

; GP/FP opcodes serve both argument and result banks; capture_offset selects.
CI_ABI_MOVE MACRO p_name, p_incoming, p_write, p_operation, p_bytes, p_alignment, p_source
    CI_LABEL p_name
    IF p_incoming
        movzx r11d, WORD PTR [r13 + 10]
        mov r10, QWORD PTR [r12 + CI_ASM_incoming - CI_ASM_frame_size]
        add r10, r11
        CI_ABI_VALUE_LOAD %p_bytes
    ELSEIF p_write
        CI_SLOT r10, r10d, 4
        IF p_source
            mov rax, r10
            IFIDN <p_operation>, <SIGN_EXTEND>
                CI_READ %p_source, 1
            ELSE
                CI_READ %p_source
            ENDIF
            movq xmm4, rax
        ELSE
            CI_ABI_VALUE_LOAD %p_bytes
        ENDIF
        movzx r11d, WORD PTR [r13 + 10]
        IF p_source
            mov eax, 8
        ELSEIF p_bytes
            mov eax, p_bytes
        ELSE
            mov eax, DWORD PTR [r13]
            shr eax, 18
            and eax, 31
        ENDIF
        call Lci_abi_put_bank
        CI_NEXT
        EXITM
    ELSE
        movzx r11d, WORD PTR [r13 + 10]
        call Lci_abi_get_bank
    ENDIF
    CI_SLOT r10, r10d, 4
    CI_ABI_VALUE_STORE %p_bytes
    CI_NEXT
ENDM
CI_ABI_MOVE ABI_READ_IN8, 1, 0, COPY, 1, 1, 0
CI_ABI_MOVE ABI_READ_IN16, 1, 0, COPY, 2, 1, 0
CI_ABI_MOVE ABI_READ_IN16_A, 1, 0, COPY, 2, 2, 0
CI_ABI_MOVE ABI_READ_IN32, 1, 0, COPY, 4, 1, 0
CI_ABI_MOVE ABI_READ_IN32_A, 1, 0, COPY, 4, 4, 0
CI_ABI_MOVE ABI_READ_IN64, 1, 0, COPY, 8, 1, 0
CI_ABI_MOVE ABI_READ_IN64_A, 1, 0, COPY, 8, 8, 0
CI_ABI_MOVE ABI_READ_IN128, 1, 0, COPY, 16, 1, 0
CI_ABI_MOVE ABI_READ_IN128_A, 1, 0, COPY, 16, 16, 0
CI_ABI_MOVE ABI_READ_IN_N, 1, 0, COPY, 0, 1, 0
CI_ABI_MOVE ABI_READ_IN_N_A2, 1, 0, COPY, 0, 2, 0
CI_ABI_MOVE ABI_READ_IN_N_A4, 1, 0, COPY, 0, 4, 0
CI_ABI_MOVE ABI_READ_IN_N_A8, 1, 0, COPY, 0, 8, 0
CI_ABI_MOVE ABI_READ_IN_N_A16, 1, 0, COPY, 0, 16, 0
CI_ABI_MOVE ABI_READ_OUT8, 0, 0, COPY, 1, 1, 0
CI_ABI_MOVE ABI_READ_OUT16, 0, 0, COPY, 2, 1, 0
CI_ABI_MOVE ABI_READ_OUT16_A, 0, 0, COPY, 2, 2, 0
CI_ABI_MOVE ABI_READ_OUT32, 0, 0, COPY, 4, 1, 0
CI_ABI_MOVE ABI_READ_OUT32_A, 0, 0, COPY, 4, 4, 0
CI_ABI_MOVE ABI_READ_OUT64, 0, 0, COPY, 8, 1, 0
CI_ABI_MOVE ABI_READ_OUT64_A, 0, 0, COPY, 8, 8, 0
CI_ABI_MOVE ABI_READ_OUT128, 0, 0, COPY, 16, 1, 0
CI_ABI_MOVE ABI_READ_OUT128_A, 0, 0, COPY, 16, 16, 0
CI_ABI_MOVE ABI_READ_OUT_N, 0, 0, COPY, 0, 1, 0
CI_ABI_MOVE ABI_READ_OUT_N_A2, 0, 0, COPY, 0, 2, 0
CI_ABI_MOVE ABI_READ_OUT_N_A4, 0, 0, COPY, 0, 4, 0
CI_ABI_MOVE ABI_READ_OUT_N_A8, 0, 0, COPY, 0, 8, 0
CI_ABI_MOVE ABI_READ_OUT_N_A16, 0, 0, COPY, 0, 16, 0
CI_ABI_MOVE ABI_WRITE_OUT8, 0, 1, COPY, 1, 1, 0
CI_ABI_MOVE ABI_WRITE_OUT16, 0, 1, COPY, 2, 1, 0
CI_ABI_MOVE ABI_WRITE_OUT16_A, 0, 1, COPY, 2, 2, 0
CI_ABI_MOVE ABI_WRITE_OUT32, 0, 1, COPY, 4, 1, 0
CI_ABI_MOVE ABI_WRITE_OUT32_A, 0, 1, COPY, 4, 4, 0
CI_ABI_MOVE ABI_WRITE_OUT64, 0, 1, COPY, 8, 1, 0
CI_ABI_MOVE ABI_WRITE_OUT64_A, 0, 1, COPY, 8, 8, 0
CI_ABI_MOVE ABI_WRITE_OUT128, 0, 1, COPY, 16, 1, 0
CI_ABI_MOVE ABI_WRITE_OUT128_A, 0, 1, COPY, 16, 16, 0
CI_ABI_MOVE ABI_WRITE_OUT_N, 0, 1, COPY, 0, 1, 0
CI_ABI_MOVE ABI_WRITE_OUT_N_A2, 0, 1, COPY, 0, 2, 0
CI_ABI_MOVE ABI_WRITE_OUT_N_A4, 0, 1, COPY, 0, 4, 0
CI_ABI_MOVE ABI_WRITE_OUT_N_A8, 0, 1, COPY, 0, 8, 0
CI_ABI_MOVE ABI_WRITE_OUT_N_A16, 0, 1, COPY, 0, 16, 0
CI_ABI_MOVE ABI_WRITE_SEXT8, 0, 1, SIGN_EXTEND, 8, 1, 1
CI_ABI_MOVE ABI_WRITE_SEXT16, 0, 1, SIGN_EXTEND, 8, 1, 2
CI_ABI_MOVE ABI_WRITE_SEXT16_A, 0, 1, SIGN_EXTEND, 8, 2, 2
CI_ABI_MOVE ABI_WRITE_SEXT32, 0, 1, SIGN_EXTEND, 8, 1, 4
CI_ABI_MOVE ABI_WRITE_SEXT32_A, 0, 1, SIGN_EXTEND, 8, 4, 4
CI_ABI_MOVE ABI_WRITE_ZEXT8, 0, 1, ZERO_EXTEND, 8, 1, 1
CI_ABI_MOVE ABI_WRITE_ZEXT16, 0, 1, ZERO_EXTEND, 8, 1, 2
CI_ABI_MOVE ABI_WRITE_ZEXT16_A, 0, 1, ZERO_EXTEND, 8, 2, 2
CI_ABI_MOVE ABI_WRITE_ZEXT32, 0, 1, ZERO_EXTEND, 8, 1, 4
CI_ABI_MOVE ABI_WRITE_ZEXT32_A, 0, 1, ZERO_EXTEND, 8, 4, 4

CI_ABI_GP_MOVE MACRO p_name, p_write, p_reg, p_alignment
    LOCAL record
    CI_LABEL p_name
    IF p_reg LT 4
        cmp WORD PTR [r13 + 10], (p_reg*8)
        jne record
        mov eax, DWORD PTR [r13 + 4]
        IF p_write
            CI_ARG_GP p_reg, mov, [r12 + rax]
        ELSE
            CI_ARG_GP p_reg, mov, [r12 + rax], 1
        ENDIF
        CI_NEXT
record:
    ENDIF
    movzx r11d, WORD PTR [r13 + 10]
    CI_SLOT r10, r10d, 4
    IF p_write
        movq xmm4, QWORD PTR [r10]
        mov eax, 8
        call Lci_abi_put_bank
    ELSE
        call Lci_abi_get_bank
        movq QWORD PTR [r10], xmm4
    ENDIF
    CI_NEXT
ENDM
CI_ABI_GP_MOVE ABI_READ_GP0_64, 0, 0, 1
CI_ABI_GP_MOVE ABI_READ_GP0_64_A, 0, 0, 8
CI_ABI_GP_MOVE ABI_WRITE_GP0_64, 1, 0, 1
CI_ABI_GP_MOVE ABI_WRITE_GP0_64_A, 1, 0, 8
CI_ABI_GP_MOVE ABI_READ_GP1_64, 0, 1, 1
CI_ABI_GP_MOVE ABI_READ_GP1_64_A, 0, 1, 8
CI_ABI_GP_MOVE ABI_WRITE_GP1_64, 1, 1, 1
CI_ABI_GP_MOVE ABI_WRITE_GP1_64_A, 1, 1, 8
CI_ABI_GP_MOVE ABI_READ_GP2_64, 0, 2, 1
CI_ABI_GP_MOVE ABI_READ_GP2_64_A, 0, 2, 8
CI_ABI_GP_MOVE ABI_WRITE_GP2_64, 1, 2, 1
CI_ABI_GP_MOVE ABI_WRITE_GP2_64_A, 1, 2, 8
CI_ABI_GP_MOVE ABI_READ_GP3_64, 0, 3, 1
CI_ABI_GP_MOVE ABI_READ_GP3_64_A, 0, 3, 8
CI_ABI_GP_MOVE ABI_WRITE_GP3_64, 1, 3, 1
CI_ABI_GP_MOVE ABI_WRITE_GP3_64_A, 1, 3, 8
CI_ABI_GP_MOVE ABI_READ_GP4_64, 0, 4, 1
CI_ABI_GP_MOVE ABI_READ_GP4_64_A, 0, 4, 8
CI_ABI_GP_MOVE ABI_WRITE_GP4_64, 1, 4, 1
CI_ABI_GP_MOVE ABI_WRITE_GP4_64_A, 1, 4, 8
CI_ABI_GP_MOVE ABI_READ_GP5_64, 0, 5, 1
CI_ABI_GP_MOVE ABI_READ_GP5_64_A, 0, 5, 8
CI_ABI_GP_MOVE ABI_WRITE_GP5_64, 1, 5, 1
CI_ABI_GP_MOVE ABI_WRITE_GP5_64_A, 1, 5, 8
CI_ABI_GP_MOVE ABI_READ_GP6_64, 0, 6, 1
CI_ABI_GP_MOVE ABI_READ_GP6_64_A, 0, 6, 8
CI_ABI_GP_MOVE ABI_WRITE_GP6_64, 1, 6, 1
CI_ABI_GP_MOVE ABI_WRITE_GP6_64_A, 1, 6, 8
CI_ABI_GP_MOVE ABI_READ_GP7_64, 0, 7, 1
CI_ABI_GP_MOVE ABI_READ_GP7_64_A, 0, 7, 8
CI_ABI_GP_MOVE ABI_WRITE_GP7_64, 1, 7, 1
CI_ABI_GP_MOVE ABI_WRITE_GP7_64_A, 1, 7, 8

CI_ABI_REG_MOVE MACRO p_name, p_write, p_reg, p_bytes, p_alignment
    LOCAL not_gp, not_fp, unsigned_value, gp_value, record_copy, record_unsigned, record_extended, record_put
    CI_LABEL p_name
    movzx r11d, WORD PTR [r13 + 10]
    IF p_reg LT 4
        cmp r11d, (p_reg*8)
        jne not_gp
        CI_SLOT r10, r10d, 4
        IF p_write
            mov rax, r10
            mov r11d, DWORD PTR [r13]
            shr r11d, 29
            cmp r11d, 5
            jne unsigned_value
            CI_READ %p_bytes, 1
            jmp gp_value
unsigned_value:
            CI_READ %p_bytes
gp_value:
            CI_ARG_GP p_reg, mov, rax
        ELSE
            CI_ARG_GP p_reg, mov, rax, 1
            CI_STORE %p_bytes
        ENDIF
        CI_NEXT
not_gp:
        cmp r11d, (32 + p_reg*16)
        jne not_fp
        CI_SLOT r10, r10d, 4
        IF p_write
            IF p_bytes EQ 16
                movdqu xmm&p_reg, XMMWORD PTR [r10]
            ELSEIF p_bytes EQ 8
                movq xmm&p_reg, QWORD PTR [r10]
            ELSEIF p_bytes EQ 4
                movss xmm&p_reg, DWORD PTR [r10]
            ELSE
                CI_ABI_VALUE_LOAD %p_bytes
                movdqa xmm&p_reg, xmm4
            ENDIF
        ELSE
            IF p_bytes EQ 16
                movdqu XMMWORD PTR [r10], xmm&p_reg
            ELSEIF p_bytes EQ 8
                movq QWORD PTR [r10], xmm&p_reg
            ELSEIF p_bytes EQ 4
                movss DWORD PTR [r10], xmm&p_reg
            ELSE
                movdqa xmm4, xmm&p_reg
                CI_ABI_VALUE_STORE %p_bytes
            ENDIF
        ENDIF
        CI_NEXT
not_fp:
    ENDIF
    IF p_write
        CI_SLOT r10, r10d, 4
        mov r11d, DWORD PTR [r13]
        shr r11d, 29
        test r11d, r11d
        jz record_copy
        mov rax, r10
        cmp r11d, 5
        jne record_unsigned
        CI_READ %p_bytes, 1
        jmp record_extended
record_unsigned:
        CI_READ %p_bytes
record_extended:
        movq xmm4, rax
        mov eax, 8
        jmp record_put
record_copy:
        CI_ABI_VALUE_LOAD %p_bytes
        mov eax, p_bytes
record_put:
        movzx r11d, WORD PTR [r13 + 10]
        call Lci_abi_put_bank
    ELSE
        call Lci_abi_get_bank
        CI_SLOT r10, r10d, 4
        CI_ABI_VALUE_STORE %p_bytes
    ENDIF
    CI_NEXT
ENDM
CI_ABI_REG_MOVE ABI_READ_GP0_8, 0, 0, 1, 1
CI_ABI_REG_MOVE ABI_WRITE_GP0_8, 1, 0, 1, 1
CI_ABI_REG_MOVE ABI_READ_GP0_32, 0, 0, 4, 1
CI_ABI_REG_MOVE ABI_READ_GP0_32_A, 0, 0, 4, 4
CI_ABI_REG_MOVE ABI_WRITE_GP0_32, 1, 0, 4, 1
CI_ABI_REG_MOVE ABI_WRITE_GP0_32_A, 1, 0, 4, 4
CI_ABI_REG_MOVE ABI_SEXT_GP0_8, 1, 0, 1, 1
CI_ABI_REG_MOVE ABI_ZEXT_GP0_8, 1, 0, 1, 1
CI_ABI_REG_MOVE ABI_SEXT_GP0_32, 1, 0, 4, 1
CI_ABI_REG_MOVE ABI_SEXT_GP0_32_A, 1, 0, 4, 4
CI_ABI_REG_MOVE ABI_ZEXT_GP0_32, 1, 0, 4, 1
CI_ABI_REG_MOVE ABI_ZEXT_GP0_32_A, 1, 0, 4, 4
CI_ABI_REG_MOVE ABI_READ_GP1_8, 0, 1, 1, 1
CI_ABI_REG_MOVE ABI_WRITE_GP1_8, 1, 1, 1, 1
CI_ABI_REG_MOVE ABI_READ_GP1_32, 0, 1, 4, 1
CI_ABI_REG_MOVE ABI_READ_GP1_32_A, 0, 1, 4, 4
CI_ABI_REG_MOVE ABI_WRITE_GP1_32, 1, 1, 4, 1
CI_ABI_REG_MOVE ABI_WRITE_GP1_32_A, 1, 1, 4, 4
CI_ABI_REG_MOVE ABI_SEXT_GP1_8, 1, 1, 1, 1
CI_ABI_REG_MOVE ABI_ZEXT_GP1_8, 1, 1, 1, 1
CI_ABI_REG_MOVE ABI_SEXT_GP1_32, 1, 1, 4, 1
CI_ABI_REG_MOVE ABI_SEXT_GP1_32_A, 1, 1, 4, 4
CI_ABI_REG_MOVE ABI_ZEXT_GP1_32, 1, 1, 4, 1
CI_ABI_REG_MOVE ABI_ZEXT_GP1_32_A, 1, 1, 4, 4
CI_ABI_REG_MOVE ABI_READ_GP2_8, 0, 2, 1, 1
CI_ABI_REG_MOVE ABI_WRITE_GP2_8, 1, 2, 1, 1
CI_ABI_REG_MOVE ABI_READ_GP2_32, 0, 2, 4, 1
CI_ABI_REG_MOVE ABI_READ_GP2_32_A, 0, 2, 4, 4
CI_ABI_REG_MOVE ABI_WRITE_GP2_32, 1, 2, 4, 1
CI_ABI_REG_MOVE ABI_WRITE_GP2_32_A, 1, 2, 4, 4
CI_ABI_REG_MOVE ABI_SEXT_GP2_8, 1, 2, 1, 1
CI_ABI_REG_MOVE ABI_ZEXT_GP2_8, 1, 2, 1, 1
CI_ABI_REG_MOVE ABI_SEXT_GP2_32, 1, 2, 4, 1
CI_ABI_REG_MOVE ABI_SEXT_GP2_32_A, 1, 2, 4, 4
CI_ABI_REG_MOVE ABI_ZEXT_GP2_32, 1, 2, 4, 1
CI_ABI_REG_MOVE ABI_ZEXT_GP2_32_A, 1, 2, 4, 4
CI_ABI_REG_MOVE ABI_READ_GP3_8, 0, 3, 1, 1
CI_ABI_REG_MOVE ABI_WRITE_GP3_8, 1, 3, 1, 1
CI_ABI_REG_MOVE ABI_READ_GP3_32, 0, 3, 4, 1
CI_ABI_REG_MOVE ABI_READ_GP3_32_A, 0, 3, 4, 4
CI_ABI_REG_MOVE ABI_WRITE_GP3_32, 1, 3, 4, 1
CI_ABI_REG_MOVE ABI_WRITE_GP3_32_A, 1, 3, 4, 4
CI_ABI_REG_MOVE ABI_SEXT_GP3_8, 1, 3, 1, 1
CI_ABI_REG_MOVE ABI_ZEXT_GP3_8, 1, 3, 1, 1
CI_ABI_REG_MOVE ABI_SEXT_GP3_32, 1, 3, 4, 1
CI_ABI_REG_MOVE ABI_SEXT_GP3_32_A, 1, 3, 4, 4
CI_ABI_REG_MOVE ABI_ZEXT_GP3_32, 1, 3, 4, 1
CI_ABI_REG_MOVE ABI_ZEXT_GP3_32_A, 1, 3, 4, 4
CI_ABI_REG_MOVE ABI_READ_GP4_8, 0, 4, 1, 1
CI_ABI_REG_MOVE ABI_WRITE_GP4_8, 1, 4, 1, 1
CI_ABI_REG_MOVE ABI_READ_GP4_32, 0, 4, 4, 1
CI_ABI_REG_MOVE ABI_READ_GP4_32_A, 0, 4, 4, 4
CI_ABI_REG_MOVE ABI_WRITE_GP4_32, 1, 4, 4, 1
CI_ABI_REG_MOVE ABI_WRITE_GP4_32_A, 1, 4, 4, 4
CI_ABI_REG_MOVE ABI_SEXT_GP4_8, 1, 4, 1, 1
CI_ABI_REG_MOVE ABI_ZEXT_GP4_8, 1, 4, 1, 1
CI_ABI_REG_MOVE ABI_SEXT_GP4_32, 1, 4, 4, 1
CI_ABI_REG_MOVE ABI_SEXT_GP4_32_A, 1, 4, 4, 4
CI_ABI_REG_MOVE ABI_ZEXT_GP4_32, 1, 4, 4, 1
CI_ABI_REG_MOVE ABI_ZEXT_GP4_32_A, 1, 4, 4, 4
CI_ABI_REG_MOVE ABI_READ_GP5_8, 0, 5, 1, 1
CI_ABI_REG_MOVE ABI_WRITE_GP5_8, 1, 5, 1, 1
CI_ABI_REG_MOVE ABI_READ_GP5_32, 0, 5, 4, 1
CI_ABI_REG_MOVE ABI_READ_GP5_32_A, 0, 5, 4, 4
CI_ABI_REG_MOVE ABI_WRITE_GP5_32, 1, 5, 4, 1
CI_ABI_REG_MOVE ABI_WRITE_GP5_32_A, 1, 5, 4, 4
CI_ABI_REG_MOVE ABI_SEXT_GP5_8, 1, 5, 1, 1
CI_ABI_REG_MOVE ABI_ZEXT_GP5_8, 1, 5, 1, 1
CI_ABI_REG_MOVE ABI_SEXT_GP5_32, 1, 5, 4, 1
CI_ABI_REG_MOVE ABI_SEXT_GP5_32_A, 1, 5, 4, 4
CI_ABI_REG_MOVE ABI_ZEXT_GP5_32, 1, 5, 4, 1
CI_ABI_REG_MOVE ABI_ZEXT_GP5_32_A, 1, 5, 4, 4
CI_ABI_REG_MOVE ABI_READ_GP6_8, 0, 6, 1, 1
CI_ABI_REG_MOVE ABI_WRITE_GP6_8, 1, 6, 1, 1
CI_ABI_REG_MOVE ABI_READ_GP6_32, 0, 6, 4, 1
CI_ABI_REG_MOVE ABI_READ_GP6_32_A, 0, 6, 4, 4
CI_ABI_REG_MOVE ABI_WRITE_GP6_32, 1, 6, 4, 1
CI_ABI_REG_MOVE ABI_WRITE_GP6_32_A, 1, 6, 4, 4
CI_ABI_REG_MOVE ABI_SEXT_GP6_8, 1, 6, 1, 1
CI_ABI_REG_MOVE ABI_ZEXT_GP6_8, 1, 6, 1, 1
CI_ABI_REG_MOVE ABI_SEXT_GP6_32, 1, 6, 4, 1
CI_ABI_REG_MOVE ABI_SEXT_GP6_32_A, 1, 6, 4, 4
CI_ABI_REG_MOVE ABI_ZEXT_GP6_32, 1, 6, 4, 1
CI_ABI_REG_MOVE ABI_ZEXT_GP6_32_A, 1, 6, 4, 4
CI_ABI_REG_MOVE ABI_READ_GP7_8, 0, 7, 1, 1
CI_ABI_REG_MOVE ABI_WRITE_GP7_8, 1, 7, 1, 1
CI_ABI_REG_MOVE ABI_READ_GP7_32, 0, 7, 4, 1
CI_ABI_REG_MOVE ABI_READ_GP7_32_A, 0, 7, 4, 4
CI_ABI_REG_MOVE ABI_WRITE_GP7_32, 1, 7, 4, 1
CI_ABI_REG_MOVE ABI_WRITE_GP7_32_A, 1, 7, 4, 4
CI_ABI_REG_MOVE ABI_SEXT_GP7_8, 1, 7, 1, 1
CI_ABI_REG_MOVE ABI_ZEXT_GP7_8, 1, 7, 1, 1
CI_ABI_REG_MOVE ABI_SEXT_GP7_32, 1, 7, 4, 1
CI_ABI_REG_MOVE ABI_SEXT_GP7_32_A, 1, 7, 4, 4
CI_ABI_REG_MOVE ABI_ZEXT_GP7_32, 1, 7, 4, 1
CI_ABI_REG_MOVE ABI_ZEXT_GP7_32_A, 1, 7, 4, 4
CI_ABI_REG_MOVE ABI_READ_FP0_32, 0, 0, 4, 1
CI_ABI_REG_MOVE ABI_READ_FP0_32_A, 0, 0, 4, 4
CI_ABI_REG_MOVE ABI_WRITE_FP0_32, 1, 0, 4, 1
CI_ABI_REG_MOVE ABI_WRITE_FP0_32_A, 1, 0, 4, 4
CI_ABI_REG_MOVE ABI_READ_FP0_64, 0, 0, 8, 1
CI_ABI_REG_MOVE ABI_READ_FP0_64_A, 0, 0, 8, 8
CI_ABI_REG_MOVE ABI_WRITE_FP0_64, 1, 0, 8, 1
CI_ABI_REG_MOVE ABI_WRITE_FP0_64_A, 1, 0, 8, 8
CI_ABI_REG_MOVE ABI_READ_FP0_128, 0, 0, 16, 1
CI_ABI_REG_MOVE ABI_READ_FP0_128_A, 0, 0, 16, 16
CI_ABI_REG_MOVE ABI_WRITE_FP0_128, 1, 0, 16, 1
CI_ABI_REG_MOVE ABI_WRITE_FP0_128_A, 1, 0, 16, 16
CI_ABI_REG_MOVE ABI_READ_FP1_32, 0, 1, 4, 1
CI_ABI_REG_MOVE ABI_READ_FP1_32_A, 0, 1, 4, 4
CI_ABI_REG_MOVE ABI_WRITE_FP1_32, 1, 1, 4, 1
CI_ABI_REG_MOVE ABI_WRITE_FP1_32_A, 1, 1, 4, 4
CI_ABI_REG_MOVE ABI_READ_FP1_64, 0, 1, 8, 1
CI_ABI_REG_MOVE ABI_READ_FP1_64_A, 0, 1, 8, 8
CI_ABI_REG_MOVE ABI_WRITE_FP1_64, 1, 1, 8, 1
CI_ABI_REG_MOVE ABI_WRITE_FP1_64_A, 1, 1, 8, 8
CI_ABI_REG_MOVE ABI_READ_FP1_128, 0, 1, 16, 1
CI_ABI_REG_MOVE ABI_READ_FP1_128_A, 0, 1, 16, 16
CI_ABI_REG_MOVE ABI_WRITE_FP1_128, 1, 1, 16, 1
CI_ABI_REG_MOVE ABI_WRITE_FP1_128_A, 1, 1, 16, 16
CI_ABI_REG_MOVE ABI_READ_FP2_32, 0, 2, 4, 1
CI_ABI_REG_MOVE ABI_READ_FP2_32_A, 0, 2, 4, 4
CI_ABI_REG_MOVE ABI_WRITE_FP2_32, 1, 2, 4, 1
CI_ABI_REG_MOVE ABI_WRITE_FP2_32_A, 1, 2, 4, 4
CI_ABI_REG_MOVE ABI_READ_FP2_64, 0, 2, 8, 1
CI_ABI_REG_MOVE ABI_READ_FP2_64_A, 0, 2, 8, 8
CI_ABI_REG_MOVE ABI_WRITE_FP2_64, 1, 2, 8, 1
CI_ABI_REG_MOVE ABI_WRITE_FP2_64_A, 1, 2, 8, 8
CI_ABI_REG_MOVE ABI_READ_FP2_128, 0, 2, 16, 1
CI_ABI_REG_MOVE ABI_READ_FP2_128_A, 0, 2, 16, 16
CI_ABI_REG_MOVE ABI_WRITE_FP2_128, 1, 2, 16, 1
CI_ABI_REG_MOVE ABI_WRITE_FP2_128_A, 1, 2, 16, 16
CI_ABI_REG_MOVE ABI_READ_FP3_32, 0, 3, 4, 1
CI_ABI_REG_MOVE ABI_READ_FP3_32_A, 0, 3, 4, 4
CI_ABI_REG_MOVE ABI_WRITE_FP3_32, 1, 3, 4, 1
CI_ABI_REG_MOVE ABI_WRITE_FP3_32_A, 1, 3, 4, 4
CI_ABI_REG_MOVE ABI_READ_FP3_64, 0, 3, 8, 1
CI_ABI_REG_MOVE ABI_READ_FP3_64_A, 0, 3, 8, 8
CI_ABI_REG_MOVE ABI_WRITE_FP3_64, 1, 3, 8, 1
CI_ABI_REG_MOVE ABI_WRITE_FP3_64_A, 1, 3, 8, 8
CI_ABI_REG_MOVE ABI_READ_FP3_128, 0, 3, 16, 1
CI_ABI_REG_MOVE ABI_READ_FP3_128_A, 0, 3, 16, 16
CI_ABI_REG_MOVE ABI_WRITE_FP3_128, 1, 3, 16, 1
CI_ABI_REG_MOVE ABI_WRITE_FP3_128_A, 1, 3, 16, 16
CI_ABI_REG_MOVE ABI_READ_FP4_32, 0, 4, 4, 1
CI_ABI_REG_MOVE ABI_READ_FP4_32_A, 0, 4, 4, 4
CI_ABI_REG_MOVE ABI_WRITE_FP4_32, 1, 4, 4, 1
CI_ABI_REG_MOVE ABI_WRITE_FP4_32_A, 1, 4, 4, 4
CI_ABI_REG_MOVE ABI_READ_FP4_64, 0, 4, 8, 1
CI_ABI_REG_MOVE ABI_READ_FP4_64_A, 0, 4, 8, 8
CI_ABI_REG_MOVE ABI_WRITE_FP4_64, 1, 4, 8, 1
CI_ABI_REG_MOVE ABI_WRITE_FP4_64_A, 1, 4, 8, 8
CI_ABI_REG_MOVE ABI_READ_FP4_128, 0, 4, 16, 1
CI_ABI_REG_MOVE ABI_READ_FP4_128_A, 0, 4, 16, 16
CI_ABI_REG_MOVE ABI_WRITE_FP4_128, 1, 4, 16, 1
CI_ABI_REG_MOVE ABI_WRITE_FP4_128_A, 1, 4, 16, 16
CI_ABI_REG_MOVE ABI_READ_FP5_32, 0, 5, 4, 1
CI_ABI_REG_MOVE ABI_READ_FP5_32_A, 0, 5, 4, 4
CI_ABI_REG_MOVE ABI_WRITE_FP5_32, 1, 5, 4, 1
CI_ABI_REG_MOVE ABI_WRITE_FP5_32_A, 1, 5, 4, 4
CI_ABI_REG_MOVE ABI_READ_FP5_64, 0, 5, 8, 1
CI_ABI_REG_MOVE ABI_READ_FP5_64_A, 0, 5, 8, 8
CI_ABI_REG_MOVE ABI_WRITE_FP5_64, 1, 5, 8, 1
CI_ABI_REG_MOVE ABI_WRITE_FP5_64_A, 1, 5, 8, 8
CI_ABI_REG_MOVE ABI_READ_FP5_128, 0, 5, 16, 1
CI_ABI_REG_MOVE ABI_READ_FP5_128_A, 0, 5, 16, 16
CI_ABI_REG_MOVE ABI_WRITE_FP5_128, 1, 5, 16, 1
CI_ABI_REG_MOVE ABI_WRITE_FP5_128_A, 1, 5, 16, 16
CI_ABI_REG_MOVE ABI_READ_FP6_32, 0, 6, 4, 1
CI_ABI_REG_MOVE ABI_READ_FP6_32_A, 0, 6, 4, 4
CI_ABI_REG_MOVE ABI_WRITE_FP6_32, 1, 6, 4, 1
CI_ABI_REG_MOVE ABI_WRITE_FP6_32_A, 1, 6, 4, 4
CI_ABI_REG_MOVE ABI_READ_FP6_64, 0, 6, 8, 1
CI_ABI_REG_MOVE ABI_READ_FP6_64_A, 0, 6, 8, 8
CI_ABI_REG_MOVE ABI_WRITE_FP6_64, 1, 6, 8, 1
CI_ABI_REG_MOVE ABI_WRITE_FP6_64_A, 1, 6, 8, 8
CI_ABI_REG_MOVE ABI_READ_FP6_128, 0, 6, 16, 1
CI_ABI_REG_MOVE ABI_READ_FP6_128_A, 0, 6, 16, 16
CI_ABI_REG_MOVE ABI_WRITE_FP6_128, 1, 6, 16, 1
CI_ABI_REG_MOVE ABI_WRITE_FP6_128_A, 1, 6, 16, 16
CI_ABI_REG_MOVE ABI_READ_FP7_32, 0, 7, 4, 1
CI_ABI_REG_MOVE ABI_READ_FP7_32_A, 0, 7, 4, 4
CI_ABI_REG_MOVE ABI_WRITE_FP7_32, 1, 7, 4, 1
CI_ABI_REG_MOVE ABI_WRITE_FP7_32_A, 1, 7, 4, 4
CI_ABI_REG_MOVE ABI_READ_FP7_64, 0, 7, 8, 1
CI_ABI_REG_MOVE ABI_READ_FP7_64_A, 0, 7, 8, 8
CI_ABI_REG_MOVE ABI_WRITE_FP7_64, 1, 7, 8, 1
CI_ABI_REG_MOVE ABI_WRITE_FP7_64_A, 1, 7, 8, 8
CI_ABI_REG_MOVE ABI_READ_FP7_128, 0, 7, 16, 1
CI_ABI_REG_MOVE ABI_READ_FP7_128_A, 0, 7, 16, 16
CI_ABI_REG_MOVE ABI_WRITE_FP7_128, 1, 7, 16, 1
CI_ABI_REG_MOVE ABI_WRITE_FP7_128_A, 1, 7, 16, 16

CI_ABI_PAIR MACRO p_name, p_write, p_alignment, p_contiguous
    LOCAL record
    CI_LABEL p_name
    cmp WORD PTR [r13 + 14], 0
    jne record
    cmp WORD PTR [r13 + 18], 8
    jne record
    mov eax, DWORD PTR [r13 + 4]
    IF p_write
        CI_ARG_GP 0, mov, [r12 + rax]
    ELSE
        CI_ARG_GP 0, mov, [r12 + rax], 1
    ENDIF
    mov eax, DWORD PTR [r13 + 8]
    IF p_write
        CI_ARG_GP 1, mov, [r12 + rax]
    ELSE
        CI_ARG_GP 1, mov, [r12 + rax], 1
    ENDIF
    CI_NEXT
record:
    FOR p_member, <0, 1>
        IF p_write
            CI_SLOT r10, r10d, (4 + p_member*4)
            movq xmm4, QWORD PTR [r10]
            movzx r11d, WORD PTR [r13 + 14 + p_member*4]
            mov eax, 8
            call Lci_abi_put_bank
        ELSE
            movzx r11d, WORD PTR [r13 + 14 + p_member*4]
            call Lci_abi_get_bank
            CI_SLOT r10, r10d, (4 + p_member*4)
            movq QWORD PTR [r10], xmm4
        ENDIF
    ENDM
    CI_NEXT
ENDM
CI_ABI_PAIR ABI_READ_GP01_64, 0, 0, 1
CI_ABI_PAIR ABI_READ_GP01_64_A, 0, 0, 8
CI_ABI_PAIR ABI_READ_GP01_64_CONTIG, 0, 1, 1
CI_ABI_PAIR ABI_READ_GP01_64_CONTIG_A, 0, 1, 8
CI_ABI_PAIR ABI_WRITE_GP01_64, 1, 0, 1
CI_ABI_PAIR ABI_WRITE_GP01_64_A, 1, 0, 8
CI_ABI_PAIR ABI_WRITE_GP01_64_CONTIG, 1, 1, 1
CI_ABI_PAIR ABI_WRITE_GP01_64_CONTIG_A, 1, 1, 8

CI_LABEL SLOT_ADDR
    mov eax, DWORD PTR [r13 + 12]
    add rax, r12
    mov ecx, DWORD PTR [r13 + 4]
    mov QWORD PTR [r12 + rcx], rax
    CI_NEXT
CI_LABEL THREAD_FENCE
; A locked RMW on our own stack orders loads and stores without the stronger
; instruction-stream serialization of MFENCE on some x86 implementations.
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

CI_ERROR MACRO p_message
    CI_PUBLISH_PC
    mov rcx, QWORD PTR [rbx]
    mov rdx, QWORD PTR [r13 + 24]
    CI_LEA r8, p_message
    CI_HELPER error
    jmp Lci_error
ENDM
CI_LABEL CHECK
    mov eax, DWORD PTR [r13 + 4]
    cmp BYTE PTR [r12 + rax], 0
    jne Lci_next
    mov r9, QWORD PTR [r13 + 8]
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
    jnz Lci_var_address_ready
    CI_PUBLISH_PC
    mov rcx, QWORD PTR [rbx]
    mov rdx, QWORD PTR [r13 + 16]
CI_HELPER ensure_var
    test eax, eax
    jne Lci_error
    mov rax, QWORD PTR [r13 + 16]
    mov rax, QWORD PTR [rax + CI_ASM_var_value]
Lci_var_address_ready:
CI_SLOT r10, r10d, 8
    mov QWORD PTR [r10], rax
    CI_NEXT
CI_LABEL TLS_ADDR
    CI_PUBLISH_PC
CI_SLOT r10, r10d, 8
    mov rcx, QWORD PTR [rbx]
    mov rdx, QWORD PTR [r13 + 16]
    mov r8, r10
CI_HELPER tls_address
    test eax, eax
    jne Lci_error
    CI_NEXT
CI_LABEL FUNC_ADDR
    mov r10, QWORD PTR [r13 + 16]
    mov rax, QWORD PTR [r10 + CI_ASM_native_func]
    test rax, rax
    jz Lci_function_missing
CI_SLOT r10, r10d, 8
    mov QWORD PTR [r10], rax
    CI_NEXT
Lci_function_missing:
    mov rax, QWORD PTR [r10 + 32]
CI_LEA r9, Lci_msg_unknown
    test rax, rax
    jz Lci_function_missing_named
    lea r9, [rax + CI_ASM_atom_data]
Lci_function_missing_named:
CI_ERROR Lci_msg_missing_function
CI_LABEL CALL_RESOLVE
CI_SLOT rax, eax, 4
    mov rdx, QWORD PTR [rax]
    mov rcx, QWORD PTR [rbx]
CI_HELPER closure_lookup
CI_SLOT r10, r10d, 4
    mov QWORD PTR [r10 + 8], rax
    CI_NEXT
CI_LABEL RETURN_SLOT
    mov r10, QWORD PTR [rbx + 16]
    cmp r10, QWORD PTR [rbx + 8]
    je Lci_return_root_buffer
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
    jmp Lci_return_copy_result
Lci_return_root_buffer:
    mov r10, QWORD PTR [rbx + 32]
    mov r9, QWORD PTR [rbx + 40]
    test r10, r10
    jz Lci_RETURN
Lci_return_copy_result:
    mov r8d, DWORD PTR [r13 + 12]
    cmp r8, r9
    ja Lci_return_result_too_small
CI_SLOT rax, eax, 8
    mov rcx, r10
    mov rdx, rax
    mov r8, r8
CI_HELPER memmove
    jmp Lci_RETURN
Lci_return_result_too_small:
    mov QWORD PTR [rsp + 32], r9
    mov r9, r9
    mov r9, r8
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
    mov rcx, QWORD PTR [rbx]
    mov rdx, QWORD PTR [rbx + 16]
    xor r8d, r8d
CI_HELPER backtrace
    CI_NEXT
CI_LABEL CLEAR_CACHE
CI_SLOT rax, eax, 8
    mov r10, QWORD PTR [rax]
CI_SLOT rax, eax, 12
    mov rax, QWORD PTR [rax]
    sub rax, r10
    mov rcx, -1
    mov rdx, r10
    mov r8, rax
CI_HELPER clear_cache
    test eax, eax
    jnz Lci_next
CI_ERROR Lci_msg_clear_cache

CI_RUNTIME_ERROR MACRO p_name, p_message
    CI_LABEL p_name
    CI_ERROR p_message
ENDM
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

CI_ALLOC MACRO p_name, p_helper
    LOCAL branch_1
    CI_LABEL p_name
    CI_PUBLISH_PC
    mov eax, DWORD PTR [r13 + 12]
    mov rdx, QWORD PTR [r12 + rax]
    mov rcx, r15
    mov r8d, DWORD PTR [r13 + 4]
    CI_HELPER p_helper
    test rax, rax
    jnz branch_1
    mov eax, 1
    jmp Lci_error
branch_1:
    mov r10d, DWORD PTR [r13 + 8]
    mov QWORD PTR [r12 + r10], rax
    CI_NEXT
ENDM
CI_ALLOC STACK_ALLOC, stack_alloc
CI_ALLOC STACK_ZALLOC, stack_zalloc

CI_CRC MACRO p_name, p_bytes
    CI_LABEL p_name
    mov eax, DWORD PTR [r13 + 16]
    mov r10d, DWORD PTR [r13 + 12]
    mov ecx, DWORD PTR [r12 + r10]
    IF p_bytes EQ 1
        crc32 ecx, BYTE PTR [r12 + rax]
    ELSEIF p_bytes EQ 2
        crc32 ecx, WORD PTR [r12 + rax]
    ELSEIF p_bytes EQ 4
        crc32 ecx, DWORD PTR [r12 + rax]
    ELSE
        crc32 rcx, QWORD PTR [r12 + rax]
    ENDIF
    mov r10d, DWORD PTR [r13 + 4]
    mov DWORD PTR [r12 + r10], ecx
    CI_NEXT
ENDM
CI_CRC CRC32C8, 1
CI_CRC CRC32C16, 2
CI_CRC CRC32C32, 4
CI_CRC CRC32C64, 8

CI_BOUNDS MACRO p_name, p_bytes, p_inclusive
    LOCAL branch_1
    CI_LABEL p_name
    CI_LOAD_SLOT 8, %p_bytes
    mov r8, rax
    CI_LOAD_SLOT 16, %p_bytes
    cmp r8, rax
    IF p_inclusive
        jbe Lci_next
        CI_LEA r10, Lci_msg_close_inclusive
    ELSE
        jb Lci_next
        CI_LEA r10, Lci_msg_close_exclusive
    ENDIF
    mov r9, r8
    mov QWORD PTR [rsp + 32], rax
    mov QWORD PTR [rsp + 40], r10
    test BYTE PTR [r13 + 2], 2
    jnz branch_1
    CI_ERROR Lci_msg_bounds_unsigned
branch_1:
    CI_ERROR Lci_msg_bounds_signed
ENDM
CI_BOUNDS BOUNDS32, 4, 0
CI_BOUNDS BOUNDS_INCLUSIVE32, 4, 1
CI_BOUNDS BOUNDS64, 8, 0
CI_BOUNDS BOUNDS_INCLUSIVE64, 8, 1

CI_SWITCH MACRO p_name, p_dense, p_sign
    LOCAL Lci_search, Lci_right, Lci_left, Lci_found
    CI_LABEL p_name
    CI_LOAD_SLOT 8, 8
    mov r10, QWORD PTR [r13 + 16]
    mov r9, QWORD PTR [r10]
    add r10, CI_ASM_switch_data
    IF p_dense
        sub rax, QWORD PTR [r10]
        cmp rax, r9
        jae Lci_JUMP
        lea rax, [rax + rax*2]
        lea r11, [r10 + rax*8]
    ELSE
        xor r8d, r8d
Lci_search:
        cmp r8, r9
        jae Lci_JUMP
        lea rcx, [r8 + r9]
        shr rcx, 1
        lea r11, [rcx + rcx*2]
        lea r11, [r10 + r11*8]
        cmp QWORD PTR [r11 + 8], rax
        IF p_sign
            jl Lci_right
        ELSE
            jb Lci_right
        ENDIF
        cmp QWORD PTR [r11], rax
        IF p_sign
            jg Lci_left
        ELSE
            ja Lci_left
        ENDIF
        jmp Lci_found
Lci_right:
        lea r8, [rcx + 1]
        jmp Lci_search
Lci_left:
        mov r9, rcx
        jmp Lci_search
    ENDIF
Lci_found:
    movsxd rax, DWORD PTR [r11 + CI_ASM_entry_target]
    shl rax, 5
    add r13, rax
    CI_DISPATCH
ENDM
CI_SWITCH SWITCH_DENSE, 1, 0
CI_SWITCH SWITCH_SIGNED, 0, 1
CI_SWITCH SWITCH_UNSIGNED, 0, 0
; Wide numeric helpers: aggregate returns use a hidden result pointer.
; Scalar type tags: format width for floats, 1000 + width for signed
; integers, 2000 + width for unsigned integers, 3000 + width for bit patterns.
CI_TYPE_F16 EQU 16
CI_TYPE_BF16 EQU 17
CI_TYPE_F32 EQU 32
CI_TYPE_F64 EQU 64
CI_TYPE_F80 EQU 80
CI_TYPE_F128 EQU 128
CI_TYPE_S32 EQU 1032
CI_TYPE_U32 EQU 2032
CI_TYPE_S64 EQU 1064
CI_TYPE_U64 EQU 2064
CI_TYPE_S128 EQU 1128
CI_TYPE_U128 EQU 2128
CI_TYPE_I8 EQU 3008
CI_TYPE_I16 EQU 3016
CI_TYPE_I32 EQU 3032
CI_TYPE_I64 EQU 3064
CI_TYPE_I128 EQU 3128

CI_SOFT_BEGIN MACRO p_result
    ci_gp = 0
    ci_temp = 32
    ci_sret = 0
    IF (p_result EQ 80)
        ci_sret = 1
    ENDIF
    IF (p_result EQ 128)
        ci_sret = 1
    ENDIF
    IFNDEF CI_ASM_NATIVE_INT128
        IF (p_result GT 1000) AND (((p_result - (p_result / 1000) * 1000)) EQ 128)
            ci_sret = 1
        ENDIF
    ENDIF
    IF ci_sret
        lea rcx, [rsp + 64]
        ci_gp = 1
    ENDIF
ENDM

CI_SOFT_GP MACRO p_value:=<rax>
    IF (ci_gp EQ 0)
        mov rcx, p_value
    ELSEIF (ci_gp EQ 1)
        mov rdx, p_value
    ELSEIF (ci_gp EQ 2)
        mov r8, p_value
    ELSEIF (ci_gp EQ 3)
        mov r9, p_value
    ELSE
        .ERR <numeric helper stack arguments overlap wide-argument storage>
    ENDIF
    ci_gp = ci_gp + 1
ENDM

CI_SOFT_IMM MACRO p_value
    ci_imm = (p_value) AND 1
    CI_SOFT_GP ci_imm
ENDM

CI_SOFT_ARG MACRO p_type, p_offset, p_wide:=<0>, p_direct:=<0>
    IF p_direct
        lea r10, [rsp + p_direct]
    ELSE
        CI_SLOT r10, r10d, p_offset
    ENDIF
    IF p_wide OR ((p_type GT 1000) AND (((p_type - (p_type / 1000) * 1000)) EQ 128)) OR (p_type EQ 80) OR (p_type EQ 128)
        IF p_wide AND (((p_type - (p_type / 1000) * 1000)) LT 128)
            CI_READ %(((p_type - (p_type / 1000) * 1000))/8), %((p_type LT 2000)), <r10>
            mov QWORD PTR [rsp + ci_temp], rax
            IF (p_type LT 2000)
                sar rax, 63
            ELSE
                xor eax, eax
            ENDIF
            mov QWORD PTR [rsp + ci_temp + 8], rax
        ELSEIF (p_type EQ 80)
            mov rax, QWORD PTR [r10]
            mov QWORD PTR [rsp + ci_temp], rax
            movzx eax, WORD PTR [r10 + 8]
            mov QWORD PTR [rsp + ci_temp + 8], rax
        ELSE
            movdqu xmm4, XMMWORD PTR [r10]
            movdqa XMMWORD PTR [rsp + ci_temp], xmm4
        ENDIF
        lea rax, [rsp + ci_temp]
        CI_SOFT_GP
        ci_temp = ci_temp + 16
        IF ci_temp GT 64
            .ERR <numeric helper arguments overlap result storage>
        ENDIF
    ELSEIF (p_type EQ 32) OR (p_type EQ 64)
        IF (ci_gp EQ 0)
            IF (p_type EQ 32)
                movss xmm0, DWORD PTR [r10]
            ELSE
                movsd xmm0, QWORD PTR [r10]
            ENDIF
        ELSEIF (ci_gp EQ 1)
            IF (p_type EQ 32)
                movss xmm1, DWORD PTR [r10]
            ELSE
                movsd xmm1, QWORD PTR [r10]
            ENDIF
        ELSEIF (ci_gp EQ 2)
            IF (p_type EQ 32)
                movss xmm2, DWORD PTR [r10]
            ELSE
                movsd xmm2, QWORD PTR [r10]
            ENDIF
        ELSE
            IF (p_type EQ 32)
                movss xmm3, DWORD PTR [r10]
            ELSE
                movsd xmm3, QWORD PTR [r10]
            ENDIF
        ENDIF
        ci_gp = ci_gp + 1
    ELSE
        IF (p_type LT 1000)
            CI_READ 2, 0, r10
        ELSE
            CI_READ %(((p_type - (p_type / 1000) * 1000))/8), %((p_type LT 2000)), r10
        ENDIF
        CI_SOFT_GP
    ENDIF
ENDM

CI_GET_I128 MACRO
    IFDEF CI_ASM_NATIVE_INT128
        movq rax, xmm0
        pextrq rdx, xmm0, 1
    ELSE
        mov rax, QWORD PTR [rsp + 64]
        mov rdx, QWORD PTR [rsp + 72]
    ENDIF
ENDM

CI_SOFT_RESULT MACRO p_type, p_offset, p_size_offset:=<12>
    CI_SLOT r10, r10d, p_offset
    IF (p_type EQ 16) OR (p_type EQ 17)
        mov WORD PTR [r10], ax
    ELSEIF (p_type EQ 32)
        movss DWORD PTR [r10], xmm0
    ELSEIF (p_type EQ 64)
        movsd QWORD PTR [r10], xmm0
    ELSEIF (p_type EQ 80)
        mov rax, QWORD PTR [rsp + 64]
        mov dx, WORD PTR [rsp + 72]
        mov QWORD PTR [r10], rax
        mov WORD PTR [r10 + 8], dx
        mov ecx, 10
        mov edx, DWORD PTR [r13 + p_size_offset]
        jmp Lci_numeric_padding
    ELSEIF (p_type EQ 128)
        mov rax, QWORD PTR [rsp + 64]
        mov rdx, QWORD PTR [rsp + 72]
        mov QWORD PTR [r10], rax
        mov QWORD PTR [r10 + 8], rdx
        mov ecx, 16
        mov edx, DWORD PTR [r13 + p_size_offset]
        jmp Lci_numeric_padding
    ELSE
        IF (((p_type - (p_type / 1000) * 1000)) EQ 128)
            CI_GET_I128
            mov QWORD PTR [r10], rax
            mov QWORD PTR [r10 + 8], rdx
        ELSE
            CI_STORE %(((p_type - (p_type / 1000) * 1000))/8)
        ENDIF
    ENDIF
ENDM
Lci_numeric_padding:
    cmp ecx, edx
    jae Lci_next
    mov BYTE PTR [r10 + rcx], 0
    inc ecx
    jmp Lci_numeric_padding

CI_WIDE_DIV MACRO p_name, p_helper
    CI_LABEL p_name
    CI_SOFT_BEGIN %3128
    CI_SOFT_ARG 3128, 8
    CI_SOFT_ARG 3128, 12
    CI_HELPER p_helper
    CI_SOFT_RESULT 3128, 4
    CI_NEXT
ENDM
CI_WIDE_DIV UDIV128, uint128_div
CI_WIDE_DIV UMOD128, uint128_mod
CI_WIDE_DIV SDIV128, int128_div
CI_WIDE_DIV SMOD128, int128_mod

CI_SOFT_BINARY MACRO p_name, p_type, p_helper, p_compare:=<0>, p_reverse:=<0>, p_invert:=<0>
    CI_LABEL p_name
    IF p_compare
        CI_SOFT_BEGIN %3008
    ELSE
        CI_SOFT_BEGIN %p_type
    ENDIF
    CI_SOFT_ARG p_type, %(12 + p_reverse*4)
    CI_SOFT_ARG p_type, %(16 - p_reverse*4)
    IF (p_type EQ 16) OR (p_type EQ 17)
        CI_SOFT_IMM %((p_type EQ 17))
    ENDIF
    CI_HELPER p_helper
    IF p_compare
        IF p_invert
            xor eax, 1
        ENDIF
        CI_SLOT r10, r10d, 4
        mov BYTE PTR [r10], al
    ELSE
        CI_SOFT_RESULT p_type, 4, 8
    ENDIF
    IF p_compare OR ((p_type NE 80) AND (p_type NE 128))
        CI_NEXT
    ENDIF
ENDM

CI_SOFT_FAMILY MACRO p_suffix, p_type, p_helper
    CI_SOFT_BINARY FADD&p_suffix, p_type, p_helper&_add
    CI_SOFT_BINARY FSUB&p_suffix, p_type, p_helper&_sub
    CI_SOFT_BINARY FMUL&p_suffix, p_type, p_helper&_mul
    CI_SOFT_BINARY FDIV&p_suffix, p_type, p_helper&_div
    CI_SOFT_BINARY FEQ&p_suffix, p_type, p_helper&_eq, 1
    CI_SOFT_BINARY FNE&p_suffix, p_type, p_helper&_eq, 1, 0, 1
    CI_SOFT_BINARY FLT&p_suffix, p_type, p_helper&_lt, 1
    CI_SOFT_BINARY FGT&p_suffix, p_type, p_helper&_lt, 1, 1
    CI_SOFT_BINARY FLE&p_suffix, p_type, p_helper&_le, 1
    CI_SOFT_BINARY FGE&p_suffix, p_type, p_helper&_le, 1, 1
ENDM
CI_SOFT_FAMILY 16, 16, float16
CI_SOFT_FAMILY _BF16, 17, float16
CI_SOFT_FAMILY 80, 80, float80
CI_SOFT_FAMILY 128, 128, float128

CI_FNEG MACRO p_name, p_bytes
    CI_LABEL p_name
    CI_SLOT rax, eax, 12
    CI_SLOT r10, r10d, 4
    IF (p_bytes LE 8)
        CI_READ %p_bytes
        btc rax, (p_bytes*8 - 1)
        CI_STORE %p_bytes
    ELSE
        mov rdx, QWORD PTR [rax]
        mov QWORD PTR [r10], rdx
        IF (p_bytes EQ 10)
            movzx eax, WORD PTR [rax + 8]
            xor eax, 08000h
            mov WORD PTR [r10 + 8], ax
        ELSE
            mov rax, QWORD PTR [rax + 8]
            btc rax, 63
            mov QWORD PTR [r10 + 8], rax
        ENDIF
        mov ecx, p_bytes
        mov edx, DWORD PTR [r13 + 8]
        jmp Lci_numeric_padding
        EXITM
    ENDIF
    CI_NEXT
ENDM
CI_FNEG FNEG16, 2
CI_FNEG FNEG_BF16, 2
CI_FNEG FNEG32, 4
CI_FNEG FNEG64, 8
CI_FNEG FNEG80, 10
CI_FNEG FNEG128, 16

CI_ITOF MACRO p_name, p_source, p_dest
    LOCAL Lci_utof_high, Lci_utof_done
    CI_LABEL p_name
    ci_unsigned = ((p_source GE 2000))
    ci_bits = ((p_source - (p_source / 1000) * 1000))
    IF ((p_dest EQ 32) OR (p_dest EQ 64)) AND (ci_bits LT 128)
        CI_LOAD_SLOT 16, %(ci_bits/8), %(ci_unsigned EQ 0)
        IF ci_unsigned AND (ci_bits EQ 64)
            test rax, rax
            js Lci_utof_high
        ENDIF
        IF (p_dest EQ 32)
            cvtsi2ss xmm0, rax
        ELSE
            cvtsi2sd xmm0, rax
        ENDIF
        IF ci_unsigned AND (ci_bits EQ 64)
            jmp Lci_utof_done
Lci_utof_high:
            mov rcx, rax
            and ecx, 1
            shr rax, 1
            or rax, rcx
            IF (p_dest EQ 32)
                cvtsi2ss xmm0, rax
                addss xmm0, xmm0
            ELSE
                cvtsi2sd xmm0, rax
                addsd xmm0, xmm0
            ENDIF
Lci_utof_done:
        ENDIF
    ELSE
        CI_SOFT_BEGIN %p_dest
        IF (p_dest EQ 16) OR (p_dest EQ 17)
            CI_SOFT_ARG p_source, 16
            IF (ci_bits EQ 128)
                CI_SOFT_IMM %ci_unsigned
                CI_SOFT_IMM %((p_dest EQ 17))
                CI_HELPER float16_from_uint128
            ELSE
                CI_SOFT_IMM %((p_dest EQ 17))
                IF (ci_bits EQ 32)
                    IF ci_unsigned
                        CI_HELPER float16_from_uint32
                    ELSE
                        CI_HELPER float16_from_int32
                    ENDIF
                ELSE
                    IF ci_unsigned
                        CI_HELPER float16_from_uint64
                    ELSE
                        CI_HELPER float16_from_int64
                    ENDIF
                ENDIF
            ENDIF
        ELSE
            CI_SOFT_ARG p_source, 16, 1
            CI_SOFT_IMM %ci_unsigned
            IF (p_dest EQ 32)
                CI_HELPER uint128_to_float
            ELSEIF (p_dest EQ 64)
                CI_HELPER uint128_to_double
            ELSEIF (p_dest EQ 80)
                CI_HELPER float80_from_uint128
            ELSE
                CI_HELPER float128_from_uint128
            ENDIF
        ENDIF
    ENDIF
    CI_SOFT_RESULT p_dest, 8
    IF (p_dest NE 80) AND (p_dest NE 128)
        CI_NEXT
    ENDIF
ENDM
CI_ITOF S32_TO_F16, CI_TYPE_S32, CI_TYPE_F16
CI_ITOF U32_TO_F16, CI_TYPE_U32, CI_TYPE_F16
CI_ITOF S64_TO_F16, CI_TYPE_S64, CI_TYPE_F16
CI_ITOF U64_TO_F16, CI_TYPE_U64, CI_TYPE_F16
CI_ITOF S128_TO_F16, CI_TYPE_S128, CI_TYPE_F16
CI_ITOF U128_TO_F16, CI_TYPE_U128, CI_TYPE_F16
CI_ITOF S32_TO_BF16, CI_TYPE_S32, CI_TYPE_BF16
CI_ITOF U32_TO_BF16, CI_TYPE_U32, CI_TYPE_BF16
CI_ITOF S64_TO_BF16, CI_TYPE_S64, CI_TYPE_BF16
CI_ITOF U64_TO_BF16, CI_TYPE_U64, CI_TYPE_BF16
CI_ITOF S128_TO_BF16, CI_TYPE_S128, CI_TYPE_BF16
CI_ITOF U128_TO_BF16, CI_TYPE_U128, CI_TYPE_BF16
CI_ITOF S32_TO_F32, CI_TYPE_S32, CI_TYPE_F32
CI_ITOF U32_TO_F32, CI_TYPE_U32, CI_TYPE_F32
CI_ITOF S64_TO_F32, CI_TYPE_S64, CI_TYPE_F32
CI_ITOF U64_TO_F32, CI_TYPE_U64, CI_TYPE_F32
CI_ITOF S128_TO_F32, CI_TYPE_S128, CI_TYPE_F32
CI_ITOF U128_TO_F32, CI_TYPE_U128, CI_TYPE_F32
CI_ITOF S32_TO_F64, CI_TYPE_S32, CI_TYPE_F64
CI_ITOF U32_TO_F64, CI_TYPE_U32, CI_TYPE_F64
CI_ITOF S64_TO_F64, CI_TYPE_S64, CI_TYPE_F64
CI_ITOF U64_TO_F64, CI_TYPE_U64, CI_TYPE_F64
CI_ITOF S128_TO_F64, CI_TYPE_S128, CI_TYPE_F64
CI_ITOF U128_TO_F64, CI_TYPE_U128, CI_TYPE_F64
CI_ITOF S32_TO_F80, CI_TYPE_S32, CI_TYPE_F80
CI_ITOF U32_TO_F80, CI_TYPE_U32, CI_TYPE_F80
CI_ITOF S64_TO_F80, CI_TYPE_S64, CI_TYPE_F80
CI_ITOF U64_TO_F80, CI_TYPE_U64, CI_TYPE_F80
CI_ITOF S128_TO_F80, CI_TYPE_S128, CI_TYPE_F80
CI_ITOF U128_TO_F80, CI_TYPE_U128, CI_TYPE_F80
CI_ITOF S32_TO_F128, CI_TYPE_S32, CI_TYPE_F128
CI_ITOF U32_TO_F128, CI_TYPE_U32, CI_TYPE_F128
CI_ITOF S64_TO_F128, CI_TYPE_S64, CI_TYPE_F128
CI_ITOF U64_TO_F128, CI_TYPE_U64, CI_TYPE_F128
CI_ITOF S128_TO_F128, CI_TYPE_S128, CI_TYPE_F128
CI_ITOF U128_TO_F128, CI_TYPE_U128, CI_TYPE_F128

CI_FTOI MACRO p_name, p_source, p_dest, p_bytes
    LOCAL Lci_ftoi_done, Lci_ftoi_low
    CI_LABEL p_name
    IF ((p_source EQ 32) OR (p_source EQ 64)) AND (p_bytes LE 8)
        mov eax, DWORD PTR [r13 + 16]
        IF (p_dest GE 2000) AND (p_bytes EQ 8)
            IF (p_source EQ 32)
                cvtss2sd xmm0, DWORD PTR [r12 + rax]
            ELSE
                movsd xmm0, QWORD PTR [r12 + rax]
            ENDIF
            mov rcx, 043e0000000000000h
            movq xmm1, rcx
            ucomisd xmm0, xmm1
            jb Lci_ftoi_low
            subsd xmm0, xmm1
            cvttsd2si rax, xmm0
            btc rax, 63
            jmp Lci_ftoi_done
Lci_ftoi_low:
            cvttsd2si rax, xmm0
        ELSE
            IF (p_source EQ 32)
                cvttss2si rax, DWORD PTR [r12 + rax]
            ELSE
                cvttsd2si rax, QWORD PTR [r12 + rax]
            ENDIF
        ENDIF
Lci_ftoi_done:
        mov r10d, DWORD PTR [r13 + 8]
        CI_STORE %p_bytes, r12+r10
    ELSE
        IF (p_source EQ 16) OR (p_source EQ 17)
            CI_SOFT_BEGIN %p_dest
            CI_SOFT_ARG p_source, 16
            CI_SOFT_IMM %((p_source EQ 17))
            IF (p_bytes EQ 16)
                CI_HELPER float16_to_uint128
            ELSEIF (p_bytes EQ 4)
                IF (p_dest GE 2000)
                    CI_HELPER float16_to_uint32
                ELSE
                    CI_HELPER float16_to_int32
                ENDIF
            ELSE
                IF (p_dest GE 2000)
                    CI_HELPER float16_to_uint64
                ELSE
                    CI_HELPER float16_to_int64
                ENDIF
            ENDIF
            CI_SOFT_RESULT p_dest, 8
        ELSE
            CI_SOFT_BEGIN %3128
            IF (p_source EQ 32)
                mov eax, DWORD PTR [r13 + 16]
                cvtss2sd xmm4, DWORD PTR [r12 + rax]
                movsd QWORD PTR [rsp + 32], xmm4
                CI_SOFT_ARG 64, 16, 0, 32
            ELSE
                CI_SOFT_ARG p_source, 16
            ENDIF
            IF (p_source EQ 80)
                CI_HELPER float80_to_uint128
            ELSEIF (p_source EQ 128)
                CI_HELPER float128_to_uint128
            ELSE
                CI_SOFT_IMM %((p_dest GE 2000))
                CI_HELPER uint128_from_double
            ENDIF
            CI_GET_I128
            mov r10d, DWORD PTR [r13 + 8]
            CI_STORE %p_bytes, r12+r10
            IF (p_bytes EQ 16)
                mov QWORD PTR [r12 + r10 + 8], rdx
            ENDIF
        ENDIF
    ENDIF
    CI_NEXT
ENDM
CI_FTOI F16_TO_S32, CI_TYPE_F16, CI_TYPE_S32, 4
CI_FTOI F16_TO_U32, CI_TYPE_F16, CI_TYPE_U32, 4
CI_FTOI F16_TO_S64, CI_TYPE_F16, CI_TYPE_S64, 8
CI_FTOI F16_TO_U64, CI_TYPE_F16, CI_TYPE_U64, 8
CI_FTOI F16_TO_I128, CI_TYPE_F16, CI_TYPE_I128, 16
CI_FTOI BF16_TO_S32, CI_TYPE_BF16, CI_TYPE_S32, 4
CI_FTOI BF16_TO_U32, CI_TYPE_BF16, CI_TYPE_U32, 4
CI_FTOI BF16_TO_S64, CI_TYPE_BF16, CI_TYPE_S64, 8
CI_FTOI BF16_TO_U64, CI_TYPE_BF16, CI_TYPE_U64, 8
CI_FTOI BF16_TO_I128, CI_TYPE_BF16, CI_TYPE_I128, 16
CI_FTOI F32_TO_S32, CI_TYPE_F32, CI_TYPE_S32, 4
CI_FTOI F32_TO_U32, CI_TYPE_F32, CI_TYPE_U32, 4
CI_FTOI F32_TO_S64, CI_TYPE_F32, CI_TYPE_S64, 8
CI_FTOI F32_TO_U64, CI_TYPE_F32, CI_TYPE_U64, 8
CI_FTOI F32_TO_S128, CI_TYPE_F32, CI_TYPE_S128, 16
CI_FTOI F32_TO_U128, CI_TYPE_F32, CI_TYPE_U128, 16
CI_FTOI F64_TO_S32, CI_TYPE_F64, CI_TYPE_S32, 4
CI_FTOI F64_TO_U32, CI_TYPE_F64, CI_TYPE_U32, 4
CI_FTOI F64_TO_S64, CI_TYPE_F64, CI_TYPE_S64, 8
CI_FTOI F64_TO_U64, CI_TYPE_F64, CI_TYPE_U64, 8
CI_FTOI F64_TO_S128, CI_TYPE_F64, CI_TYPE_S128, 16
CI_FTOI F64_TO_U128, CI_TYPE_F64, CI_TYPE_U128, 16
CI_FTOI F80_TO_I8, CI_TYPE_F80, CI_TYPE_I8, 1
CI_FTOI F80_TO_I16, CI_TYPE_F80, CI_TYPE_I16, 2
CI_FTOI F80_TO_I32, CI_TYPE_F80, CI_TYPE_I32, 4
CI_FTOI F80_TO_I64, CI_TYPE_F80, CI_TYPE_I64, 8
CI_FTOI F80_TO_I128, CI_TYPE_F80, CI_TYPE_I128, 16
CI_FTOI F128_TO_I8, CI_TYPE_F128, CI_TYPE_I8, 1
CI_FTOI F128_TO_I16, CI_TYPE_F128, CI_TYPE_I16, 2
CI_FTOI F128_TO_I32, CI_TYPE_F128, CI_TYPE_I32, 4
CI_FTOI F128_TO_I64, CI_TYPE_F128, CI_TYPE_I64, 8
CI_FTOI F128_TO_I128, CI_TYPE_F128, CI_TYPE_I128, 16

CI_FTOF MACRO p_name, p_source, p_dest
    CI_LABEL p_name
    IF ((p_source EQ 32) AND (p_dest EQ 64)) OR ((p_source EQ 64) AND (p_dest EQ 32))
        mov eax, DWORD PTR [r13 + 16]
        IF (p_dest EQ 32)
            cvtsd2ss xmm0, QWORD PTR [r12 + rax]
        ELSE
            cvtss2sd xmm0, DWORD PTR [r12 + rax]
        ENDIF
    ELSEIF ((p_source EQ 16) OR (p_source EQ 17)) AND ((p_dest EQ 16) OR (p_dest EQ 17))
        CI_SOFT_BEGIN %32
        CI_SOFT_ARG p_source, 16
        CI_SOFT_IMM %((p_source EQ 17))
        CI_HELPER float16_to_float
        CI_SOFT_BEGIN %p_dest
        ci_gp = 1
        CI_SOFT_IMM %((p_dest EQ 17))
        CI_HELPER float16_from_float
    ELSE
        CI_SOFT_BEGIN %p_dest
        CI_SOFT_ARG p_source, 16
        IF (p_source EQ 16) OR (p_source EQ 17)
            CI_SOFT_IMM %((p_source EQ 17))
            IF (p_dest EQ 32)
                CI_HELPER float16_to_float
            ELSEIF (p_dest EQ 64)
                CI_HELPER float16_to_double
            ELSEIF (p_dest EQ 80)
                CI_HELPER float16_to_float80
            ELSE
                CI_HELPER float16_to_quad
            ENDIF
        ELSEIF (p_dest EQ 16) OR (p_dest EQ 17)
            CI_SOFT_IMM %((p_dest EQ 17))
            IF (p_source EQ 32)
                CI_HELPER float16_from_float
            ELSEIF (p_source EQ 64)
                CI_HELPER float16_from_double
            ELSEIF (p_source EQ 80)
                CI_HELPER float16_from_float80
            ELSE
                CI_HELPER float16_from_quad
            ENDIF
        ELSEIF (p_source EQ 32)
            IF (p_dest EQ 80)
                CI_HELPER float80_from_float
            ELSE
                CI_HELPER float128_from_float
            ENDIF
        ELSEIF (p_source EQ 64)
            IF (p_dest EQ 80)
                CI_HELPER float80_from_double
            ELSE
                CI_HELPER float128_from_double
            ENDIF
        ELSEIF (p_source EQ 80)
            IF (p_dest EQ 32)
                CI_HELPER float80_to_float
            ELSEIF (p_dest EQ 64)
                CI_HELPER float80_to_double
            ELSE
                CI_HELPER float128_from_float80
            ENDIF
        ELSE
            IF (p_dest EQ 32)
                CI_HELPER float128_to_float
            ELSEIF (p_dest EQ 64)
                CI_HELPER float128_to_double
            ELSE
                CI_HELPER float80_from_float128
            ENDIF
        ENDIF
    ENDIF
    CI_SOFT_RESULT p_dest, 8
    IF (p_dest NE 80) AND (p_dest NE 128)
        CI_NEXT
    ENDIF
ENDM
CI_FTOF F16_TO_BF16, CI_TYPE_F16, CI_TYPE_BF16
CI_FTOF F16_TO_F32, CI_TYPE_F16, CI_TYPE_F32
CI_FTOF F16_TO_F64, CI_TYPE_F16, CI_TYPE_F64
CI_FTOF F16_TO_F80, CI_TYPE_F16, CI_TYPE_F80
CI_FTOF F16_TO_F128, CI_TYPE_F16, CI_TYPE_F128
CI_FTOF BF16_TO_F16, CI_TYPE_BF16, CI_TYPE_F16
CI_FTOF BF16_TO_F32, CI_TYPE_BF16, CI_TYPE_F32
CI_FTOF BF16_TO_F64, CI_TYPE_BF16, CI_TYPE_F64
CI_FTOF BF16_TO_F80, CI_TYPE_BF16, CI_TYPE_F80
CI_FTOF BF16_TO_F128, CI_TYPE_BF16, CI_TYPE_F128
CI_FTOF F32_TO_F16, CI_TYPE_F32, CI_TYPE_F16
CI_FTOF F32_TO_BF16, CI_TYPE_F32, CI_TYPE_BF16
CI_FTOF F32_TO_F64, CI_TYPE_F32, CI_TYPE_F64
CI_FTOF F32_TO_F80, CI_TYPE_F32, CI_TYPE_F80
CI_FTOF F32_TO_F128, CI_TYPE_F32, CI_TYPE_F128
CI_FTOF F64_TO_F16, CI_TYPE_F64, CI_TYPE_F16
CI_FTOF F64_TO_BF16, CI_TYPE_F64, CI_TYPE_BF16
CI_FTOF F64_TO_F32, CI_TYPE_F64, CI_TYPE_F32
CI_FTOF F64_TO_F80, CI_TYPE_F64, CI_TYPE_F80
CI_FTOF F64_TO_F128, CI_TYPE_F64, CI_TYPE_F128
CI_FTOF F80_TO_F16, CI_TYPE_F80, CI_TYPE_F16
CI_FTOF F80_TO_BF16, CI_TYPE_F80, CI_TYPE_BF16
CI_FTOF F80_TO_F32, CI_TYPE_F80, CI_TYPE_F32
CI_FTOF F80_TO_F64, CI_TYPE_F80, CI_TYPE_F64
CI_FTOF F80_TO_F128, CI_TYPE_F80, CI_TYPE_F128
CI_FTOF F128_TO_F16, CI_TYPE_F128, CI_TYPE_F16
CI_FTOF F128_TO_BF16, CI_TYPE_F128, CI_TYPE_BF16
CI_FTOF F128_TO_F32, CI_TYPE_F128, CI_TYPE_F32
CI_FTOF F128_TO_F64, CI_TYPE_F128, CI_TYPE_F64
CI_FTOF F128_TO_F80, CI_TYPE_F128, CI_TYPE_F80

; Keep the va_list cursor in RAX and the destination pointer pair in R10.
CI_VA_BEGIN MACRO
    CI_LOAD_SLOT 12, 8
    CI_SLOT r10, r10d, 4
ENDM

CI_VA_STACK MACRO p_indirect:=<0>, p_skip:=<0>
    mov ecx, DWORD PTR [r13 + 16]
    mov rdx, QWORD PTR [rax]
    add rdx, rcx
    not rcx
    and rdx, rcx
    mov ecx, DWORD PTR [r13 + 20]
    add rcx, rdx
    mov QWORD PTR [rax], rcx
    IF p_indirect
        mov rdx, QWORD PTR [rdx]
    ENDIF
    mov QWORD PTR [r10], rdx
    IF p_skip
        movzx eax, BYTE PTR [r13 + 2]
        add eax, 2
        shl eax, 5
        add r13, rax
        CI_DISPATCH
    ELSE
        CI_NEXT
    ENDIF
ENDM
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

CI_VA_ARM MACRO p_name, p_align16:=<0>
    LOCAL branch_1, branch_2, branch_3, branch_4, branch_5, branch_6, branch_7, branch_8, branch_9
    CI_LABEL p_name
    CI_VA_BEGIN
    movzx r8d, BYTE PTR [r13 + 3]
    mov r9d, r8d
    and r8d, 15
    shr r9d, 4
    shl r8d, 3
    shl r9d, 4
    movsxd rcx, DWORD PTR [rax + 24]
    movsxd rdx, DWORD PTR [rax + 28]
    IF p_align16
        add rcx, 15
        and rcx, -16
    ENDIF
    test r8d, r8d
    jz branch_1
    test ecx, ecx
    jns branch_7
    add r8d, ecx
    jg branch_7
branch_1:
    test r9d, r9d
    jz branch_2
    test edx, edx
    jns branch_7
    add r9d, edx
    jg branch_7
branch_2:
    test BYTE PTR [r13 + 3], 15
    jz branch_3
    mov DWORD PTR [rax + 24], r8d
    add rcx, QWORD PTR [rax + 8]
    jmp branch_4
branch_3:
    xor ecx, ecx
branch_4:
    mov QWORD PTR [r10], rcx
    test BYTE PTR [r13 + 3], 240
    jz branch_5
    mov DWORD PTR [rax + 28], r9d
    add rdx, QWORD PTR [rax + 16]
    jmp branch_6
branch_5:
    xor edx, edx
branch_6:
    mov QWORD PTR [r10 + 8], rdx
    CI_NEXT
branch_7:
    test BYTE PTR [r13 + 3], 15
    jz branch_8
    mov DWORD PTR [rax + 24], 0
branch_8:
    test BYTE PTR [r13 + 3], 240
    jz branch_9
    mov DWORD PTR [rax + 28], 0
branch_9:
    CI_VA_STACK 0, 1
ENDM
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

ci_interp_execute_asm ENDP

; Callback: R10 = thunk metadata; [R10] = CiClosureData.
; PC 2 skips FRAME_SAVE/FRAME_ENTER, performed here before execute entry.
ALIGN 16
PUBLIC ci_interp_callback_asm
ci_interp_callback_asm PROC FRAME
    CI_ENDBR
    push rbx
    .pushreg rbx
    push r12
    .pushreg r12
    push r13
    .pushreg r13
    push r15
    .pushreg r15
    sub rsp, 440
    .allocstack 440
    .endprolog
    mov QWORD PTR [rsp + 64], rcx
    mov QWORD PTR [rsp + 72], rdx
    mov QWORD PTR [rsp + 80], r8
    mov QWORD PTR [rsp + 88], r9
    movdqu XMMWORD PTR [rsp + 96], xmm0
    movdqu XMMWORD PTR [rsp + 112], xmm1
    movdqu XMMWORD PTR [rsp + 128], xmm2
    movdqu XMMWORD PTR [rsp + 144], xmm3
    lea rax, [rsp + 512]
    mov QWORD PTR [rsp + 160], rax
    mov QWORD PTR [rsp + 168], 0
    pxor xmm0, xmm0
    movdqu XMMWORD PTR [rsp + 176], xmm0
    movdqu XMMWORD PTR [rsp + 192], xmm0
    movdqu XMMWORD PTR [rsp + 208], xmm0
    movdqu XMMWORD PTR [rsp + 224], xmm0
    movdqu XMMWORD PTR [rsp + 240], xmm0
FOR p_offset, <256, 272, 288, 304, 320, 336, 352, 368>
    movdqu XMMWORD PTR [rsp + p_offset], xmm0
ENDM
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
    mov rcx, QWORD PTR [rbx]
    mov rdx, QWORD PTR [r13 + 48]
CI_LEA r8, Lci_msg_hotswap
    call QWORD PTR [r12 + CI_ASM_HELPER_error*8]
    jmp Lcb_export
Lcb_resolved:
    mov r13, QWORD PTR [r13 + CI_ASM_func_code]
    mov r13, QWORD PTR [r13 + 8]
    mov rcx, QWORD PTR [rbx]
    call QWORD PTR [r12 + CI_ASM_HELPER_callback_stack*8]
    test rax, rax
    jz Lcb_export
    mov r15, rax
FOR p_offset, <0, 16, 32, 48, 64, 80, 96, 112, 128, 144, 160, 176, 192, 208, 224, 240, 256, 272, 288, 304>
    movdqu xmm0, XMMWORD PTR [rsp + 64 + p_offset]
    movdqu XMMWORD PTR [r15 + p_offset], xmm0
ENDM
    mov rcx, QWORD PTR [rbx]
    mov rdx, r15
    xor r8d, r8d
    mov r9, QWORD PTR [r13 + 8]
    mov eax, DWORD PTR [r13 + 16]
    mov QWORD PTR [rsp + 32], rax
    lea rax, [rsp + 384]
    mov QWORD PTR [rsp + 40], rax
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
    je Lci_callback_no_incoming
    lea rax, [r10 + rax + CI_ASM_frame_size]
    jmp Lci_callback_incoming_ready
Lci_callback_no_incoming:
    xor eax, eax
Lci_callback_incoming_ready:
    mov QWORD PTR [r10 + CI_ASM_incoming], rax
    mov rcx, QWORD PTR [rbx]
    mov rdx, r10
    mov r8, r12
    mov r9, r15
    mov QWORD PTR [rsp + 32], 0
    mov QWORD PTR [rsp + 40], 0
    mov rax, QWORD PTR [rbx + 24]
    mov QWORD PTR [rsp + 48], rax
    CI_EXECUTE_CALL
    test eax, eax
    jnz Lcb_export
    mov rax, QWORD PTR [r15 + 104]
    mov QWORD PTR [rsp + 168], rax
    movdqu xmm0, XMMWORD PTR [r15 + 112]
    movdqu XMMWORD PTR [rsp + 176], xmm0
Lcb_export:
    mov rax, QWORD PTR [rsp + 168]
    movdqu xmm0, XMMWORD PTR [rsp + 176]
    add rsp, 440
    pop r15
    pop r13
    pop r12
    pop rbx
    ret
ci_interp_callback_asm ENDP

ALIGN 4
Lci_dispatch_table:

CI_ENTRY MACRO p_name
    IFNDEF Lci_handler_&p_name
        .ERR <missing assembly opcode handler: p_name>
    ENDIF
    DD Lci_handler_&p_name - Lci_dispatch_table
ENDM
CI_ENTRY ADD_IMM32
CI_ENTRY SUB_IMM32
CI_ENTRY MUL_IMM32
CI_ENTRY SDIV_IMM32
CI_ENTRY UDIV_IMM32
CI_ENTRY SMOD_IMM32
CI_ENTRY UMOD_IMM32
CI_ENTRY AND_IMM32
CI_ENTRY OR_IMM32
CI_ENTRY XOR_IMM32
CI_ENTRY SHL_IMM32
CI_ENTRY ASR_IMM32
CI_ENTRY LSR_IMM32
CI_ENTRY ADD_IMM64
CI_ENTRY SUB_IMM64
CI_ENTRY MUL_IMM64
CI_ENTRY SDIV_IMM64
CI_ENTRY UDIV_IMM64
CI_ENTRY SMOD_IMM64
CI_ENTRY UMOD_IMM64
CI_ENTRY AND_IMM64
CI_ENTRY OR_IMM64
CI_ENTRY XOR_IMM64
CI_ENTRY SHL_IMM64
CI_ENTRY ASR_IMM64
CI_ENTRY LSR_IMM64
CI_ENTRY ADD8
CI_ENTRY SUB8
CI_ENTRY MUL8
CI_ENTRY SDIV8
CI_ENTRY UDIV8
CI_ENTRY SMOD8
CI_ENTRY UMOD8
CI_ENTRY AND8
CI_ENTRY OR8
CI_ENTRY XOR8
CI_ENTRY SHL8
CI_ENTRY ASR8
CI_ENTRY LSR8
CI_ENTRY NEG8
CI_ENTRY NOT8
CI_ENTRY ADD16
CI_ENTRY SUB16
CI_ENTRY MUL16
CI_ENTRY SDIV16
CI_ENTRY UDIV16
CI_ENTRY SMOD16
CI_ENTRY UMOD16
CI_ENTRY AND16
CI_ENTRY OR16
CI_ENTRY XOR16
CI_ENTRY SHL16
CI_ENTRY ASR16
CI_ENTRY LSR16
CI_ENTRY NEG16
CI_ENTRY NOT16
CI_ENTRY ADD32
CI_ENTRY SUB32
CI_ENTRY MUL32
CI_ENTRY SDIV32
CI_ENTRY UDIV32
CI_ENTRY SMOD32
CI_ENTRY UMOD32
CI_ENTRY AND32
CI_ENTRY OR32
CI_ENTRY XOR32
CI_ENTRY SHL32
CI_ENTRY ASR32
CI_ENTRY LSR32
CI_ENTRY NEG32
CI_ENTRY NOT32
CI_ENTRY ADD64
CI_ENTRY SUB64
CI_ENTRY MUL64
CI_ENTRY SDIV64
CI_ENTRY UDIV64
CI_ENTRY SMOD64
CI_ENTRY UMOD64
CI_ENTRY AND64
CI_ENTRY OR64
CI_ENTRY XOR64
CI_ENTRY SHL64
CI_ENTRY ASR64
CI_ENTRY LSR64
CI_ENTRY NEG64
CI_ENTRY NOT64
CI_ENTRY ADD128
CI_ENTRY SUB128
CI_ENTRY MUL128
CI_ENTRY SDIV128
CI_ENTRY UDIV128
CI_ENTRY SMOD128
CI_ENTRY UMOD128
CI_ENTRY AND128
CI_ENTRY OR128
CI_ENTRY XOR128
CI_ENTRY SHL128
CI_ENTRY ASR128
CI_ENTRY LSR128
CI_ENTRY NEG128
CI_ENTRY NOT128
CI_ENTRY EQ32
CI_ENTRY NE32
CI_ENTRY SLT32
CI_ENTRY ULT32
CI_ENTRY SGT32
CI_ENTRY UGT32
CI_ENTRY SLE32
CI_ENTRY ULE32
CI_ENTRY SGE32
CI_ENTRY UGE32
CI_ENTRY EQ64
CI_ENTRY NE64
CI_ENTRY SLT64
CI_ENTRY ULT64
CI_ENTRY SGT64
CI_ENTRY UGT64
CI_ENTRY SLE64
CI_ENTRY ULE64
CI_ENTRY SGE64
CI_ENTRY UGE64
CI_ENTRY EQ128
CI_ENTRY NE128
CI_ENTRY SLT128
CI_ENTRY ULT128
CI_ENTRY SGT128
CI_ENTRY UGT128
CI_ENTRY SLE128
CI_ENTRY ULE128
CI_ENTRY SGE128
CI_ENTRY UGE128
CI_ENTRY EQ_JUMP32
CI_ENTRY NE_JUMP32
CI_ENTRY SLT_JUMP32
CI_ENTRY ULT_JUMP32
CI_ENTRY SGT_JUMP32
CI_ENTRY UGT_JUMP32
CI_ENTRY SLE_JUMP32
CI_ENTRY ULE_JUMP32
CI_ENTRY SGE_JUMP32
CI_ENTRY UGE_JUMP32
CI_ENTRY EQ_JUMP64
CI_ENTRY NE_JUMP64
CI_ENTRY SLT_JUMP64
CI_ENTRY ULT_JUMP64
CI_ENTRY SGT_JUMP64
CI_ENTRY UGT_JUMP64
CI_ENTRY SLE_JUMP64
CI_ENTRY ULE_JUMP64
CI_ENTRY SGE_JUMP64
CI_ENTRY UGE_JUMP64
CI_ENTRY FADD_BF16
CI_ENTRY FSUB_BF16
CI_ENTRY FMUL_BF16
CI_ENTRY FDIV_BF16
CI_ENTRY FNEG_BF16
CI_ENTRY FADD16
CI_ENTRY FSUB16
CI_ENTRY FMUL16
CI_ENTRY FDIV16
CI_ENTRY FNEG16
CI_ENTRY FADD32
CI_ENTRY FSUB32
CI_ENTRY FMUL32
CI_ENTRY FDIV32
CI_ENTRY FNEG32
CI_ENTRY FADD64
CI_ENTRY FSUB64
CI_ENTRY FMUL64
CI_ENTRY FDIV64
CI_ENTRY FNEG64
CI_ENTRY FADD80
CI_ENTRY FSUB80
CI_ENTRY FMUL80
CI_ENTRY FDIV80
CI_ENTRY FNEG80
CI_ENTRY FADD128
CI_ENTRY FSUB128
CI_ENTRY FMUL128
CI_ENTRY FDIV128
CI_ENTRY FNEG128
CI_ENTRY FEQ_BF16
CI_ENTRY FNE_BF16
CI_ENTRY FLT_BF16
CI_ENTRY FGT_BF16
CI_ENTRY FLE_BF16
CI_ENTRY FGE_BF16
CI_ENTRY FEQ16
CI_ENTRY FNE16
CI_ENTRY FLT16
CI_ENTRY FGT16
CI_ENTRY FLE16
CI_ENTRY FGE16
CI_ENTRY FEQ32
CI_ENTRY FNE32
CI_ENTRY FLT32
CI_ENTRY FGT32
CI_ENTRY FLE32
CI_ENTRY FGE32
CI_ENTRY FEQ64
CI_ENTRY FNE64
CI_ENTRY FLT64
CI_ENTRY FGT64
CI_ENTRY FLE64
CI_ENTRY FGE64
CI_ENTRY FEQ80
CI_ENTRY FNE80
CI_ENTRY FLT80
CI_ENTRY FGT80
CI_ENTRY FLE80
CI_ENTRY FGE80
CI_ENTRY FEQ128
CI_ENTRY FNE128
CI_ENTRY FLT128
CI_ENTRY FGT128
CI_ENTRY FLE128
CI_ENTRY FGE128
CI_ENTRY CHECK
CI_ENTRY PROPAGATE_ERROR
CI_ENTRY SLOT_ADDR
CI_ENTRY VAR_ADDR
CI_ENTRY TLS_ADDR
CI_ENTRY FUNC_ADDR
CI_ENTRY JUMP
CI_ENTRY RETURN
CI_ENTRY RETURN_SLOT
CI_ENTRY CLEAR_CACHE
CI_ENTRY STACK_SAVE
CI_ENTRY STACK_RESTORE
CI_ENTRY CAPTURE_BACKTRACE
CI_ENTRY FRAME_ENTER
CI_ENTRY FRAME_READY
CI_ENTRY FRAME_SAVE
CI_ENTRY FRAME_RESTORE
CI_ENTRY CALL_RESOLVE

CI_ENTRY CONST8
CI_ENTRY CONST16
CI_ENTRY CONST32
CI_ENTRY CONST64
CI_ENTRY CONST128
CI_ENTRY CONST_N
CI_ENTRY LOAD8
CI_ENTRY LOAD16_A
CI_ENTRY LOAD16_U
CI_ENTRY LOAD32_A
CI_ENTRY LOAD32_U
CI_ENTRY LOAD64_A
CI_ENTRY LOAD64_U
CI_ENTRY LOAD128_A
CI_ENTRY LOAD128_U
CI_ENTRY LOAD_N
CI_ENTRY LOAD_N_A2
CI_ENTRY LOAD_N_A4
CI_ENTRY LOAD_N_A8
CI_ENTRY LOAD_N_A16
CI_ENTRY STORE8
CI_ENTRY STORE16_A
CI_ENTRY STORE16_U
CI_ENTRY STORE32_A
CI_ENTRY STORE32_U
CI_ENTRY STORE64_A
CI_ENTRY STORE64_U
CI_ENTRY STORE128_A
CI_ENTRY STORE128_U
CI_ENTRY STORE_N
CI_ENTRY STORE_N_A2
CI_ENTRY STORE_N_A4
CI_ENTRY STORE_N_A8
CI_ENTRY STORE_N_A16
CI_ENTRY STORE_IMM8
CI_ENTRY STORE_IMM16_A
CI_ENTRY STORE_IMM16_U
CI_ENTRY STORE_IMM32_A
CI_ENTRY STORE_IMM32_U
CI_ENTRY STORE_IMM64_A
CI_ENTRY STORE_IMM64_U
CI_ENTRY STORE_IMM_N
CI_ENTRY STORE_IMM_N_A2
CI_ENTRY STORE_IMM_N_A4
CI_ENTRY STORE_IMM_N_A8
CI_ENTRY STORE_IMM_N_A16
CI_ENTRY ZERO8
CI_ENTRY ZERO16_A
CI_ENTRY ZERO16_U
CI_ENTRY ZERO32_A
CI_ENTRY ZERO32_U
CI_ENTRY ZERO64_A
CI_ENTRY ZERO64_U
CI_ENTRY ZERO128_A
CI_ENTRY ZERO128_U
CI_ENTRY ZERO_N
CI_ENTRY ZERO_N_A2
CI_ENTRY ZERO_N_A4
CI_ENTRY ZERO_N_A8
CI_ENTRY ZERO_N_A16
CI_ENTRY COPY8
CI_ENTRY COPY16_A
CI_ENTRY COPY16_U
CI_ENTRY COPY32_A
CI_ENTRY COPY32_U
CI_ENTRY COPY64_A
CI_ENTRY COPY64_U
CI_ENTRY COPY128_A
CI_ENTRY COPY128_U
CI_ENTRY COPY_N
CI_ENTRY COPY_N_A2
CI_ENTRY COPY_N_A4
CI_ENTRY COPY_N_A8
CI_ENTRY COPY_N_A16
CI_ENTRY MEMCOPY8
CI_ENTRY MEMCOPY16_A
CI_ENTRY MEMCOPY16_U
CI_ENTRY MEMCOPY32_A
CI_ENTRY MEMCOPY32_U
CI_ENTRY MEMCOPY64_A
CI_ENTRY MEMCOPY64_U
CI_ENTRY MEMCOPY128_A
CI_ENTRY MEMCOPY128_U
CI_ENTRY MEMCOPY_N
CI_ENTRY MEMCOPY_N_A2
CI_ENTRY MEMCOPY_N_A4
CI_ENTRY MEMCOPY_N_A8
CI_ENTRY MEMCOPY_N_A16
CI_ENTRY MEMCOPY_VAR
CI_ENTRY MEMCOPY_VAR_A2
CI_ENTRY MEMCOPY_VAR_A4
CI_ENTRY MEMCOPY_VAR_A8
CI_ENTRY MEMCOPY_VAR_A16
CI_ENTRY SEXT8_16
CI_ENTRY ZEXT8_16
CI_ENTRY SEXT8_32
CI_ENTRY ZEXT8_32
CI_ENTRY SEXT8_64
CI_ENTRY ZEXT8_64
CI_ENTRY SEXT8_128
CI_ENTRY ZEXT8_128
CI_ENTRY SEXT16_32
CI_ENTRY ZEXT16_32
CI_ENTRY SEXT16_64
CI_ENTRY ZEXT16_64
CI_ENTRY SEXT16_128
CI_ENTRY ZEXT16_128
CI_ENTRY SEXT32_64
CI_ENTRY ZEXT32_64
CI_ENTRY SEXT32_128
CI_ENTRY ZEXT32_128
CI_ENTRY SEXT64_128
CI_ENTRY ZEXT64_128
CI_ENTRY OVERFLOW_S8
CI_ENTRY OVERFLOW_U8
CI_ENTRY OVERFLOW_S16
CI_ENTRY OVERFLOW_U16
CI_ENTRY OVERFLOW_S32
CI_ENTRY OVERFLOW_U32
CI_ENTRY OVERFLOW_S64
CI_ENTRY OVERFLOW_U64
CI_ENTRY CHECKED_ADD_S8
CI_ENTRY CHECKED_ADD_U8
CI_ENTRY CHECKED_ADD_S16
CI_ENTRY CHECKED_ADD_U16
CI_ENTRY CHECKED_ADD_S32
CI_ENTRY CHECKED_ADD_U32
CI_ENTRY CHECKED_ADD_S64
CI_ENTRY CHECKED_ADD_U64
CI_ENTRY CHECKED_SUB_S8
CI_ENTRY CHECKED_SUB_U8
CI_ENTRY CHECKED_SUB_S16
CI_ENTRY CHECKED_SUB_U16
CI_ENTRY CHECKED_SUB_S32
CI_ENTRY CHECKED_SUB_U32
CI_ENTRY CHECKED_SUB_S64
CI_ENTRY CHECKED_SUB_U64
CI_ENTRY CHECKED_MUL_S8
CI_ENTRY CHECKED_MUL_U8
CI_ENTRY CHECKED_MUL_S16
CI_ENTRY CHECKED_MUL_U16
CI_ENTRY CHECKED_MUL_S32
CI_ENTRY CHECKED_MUL_U32
CI_ENTRY CHECKED_MUL_S64
CI_ENTRY CHECKED_MUL_U64
CI_ENTRY ISTRUE_I8
CI_ENTRY ISFALSE_I8
CI_ENTRY ISTRUE_I16
CI_ENTRY ISFALSE_I16
CI_ENTRY ISTRUE_I32
CI_ENTRY ISFALSE_I32
CI_ENTRY ISTRUE_I64
CI_ENTRY ISFALSE_I64
CI_ENTRY ISTRUE_I128
CI_ENTRY ISFALSE_I128
CI_ENTRY ISTRUE_F16
CI_ENTRY ISFALSE_F16
CI_ENTRY ISTRUE_F32
CI_ENTRY ISFALSE_F32
CI_ENTRY ISTRUE_F64
CI_ENTRY ISFALSE_F64
CI_ENTRY ISTRUE_F80
CI_ENTRY ISFALSE_F80
CI_ENTRY ISTRUE_F128
CI_ENTRY ISFALSE_F128
CI_ENTRY S32_TO_F16
CI_ENTRY U32_TO_F16
CI_ENTRY S64_TO_F16
CI_ENTRY U64_TO_F16
CI_ENTRY S128_TO_F16
CI_ENTRY U128_TO_F16
CI_ENTRY S32_TO_BF16
CI_ENTRY U32_TO_BF16
CI_ENTRY S64_TO_BF16
CI_ENTRY U64_TO_BF16
CI_ENTRY S128_TO_BF16
CI_ENTRY U128_TO_BF16
CI_ENTRY S32_TO_F32
CI_ENTRY U32_TO_F32
CI_ENTRY S64_TO_F32
CI_ENTRY U64_TO_F32
CI_ENTRY S128_TO_F32
CI_ENTRY U128_TO_F32
CI_ENTRY S32_TO_F64
CI_ENTRY U32_TO_F64
CI_ENTRY S64_TO_F64
CI_ENTRY U64_TO_F64
CI_ENTRY S128_TO_F64
CI_ENTRY U128_TO_F64
CI_ENTRY S32_TO_F80
CI_ENTRY U32_TO_F80
CI_ENTRY S64_TO_F80
CI_ENTRY U64_TO_F80
CI_ENTRY S128_TO_F80
CI_ENTRY U128_TO_F80
CI_ENTRY S32_TO_F128
CI_ENTRY U32_TO_F128
CI_ENTRY S64_TO_F128
CI_ENTRY U64_TO_F128
CI_ENTRY S128_TO_F128
CI_ENTRY U128_TO_F128
CI_ENTRY F16_TO_S32
CI_ENTRY F16_TO_U32
CI_ENTRY F16_TO_S64
CI_ENTRY F16_TO_U64
CI_ENTRY F16_TO_I128
CI_ENTRY BF16_TO_S32
CI_ENTRY BF16_TO_U32
CI_ENTRY BF16_TO_S64
CI_ENTRY BF16_TO_U64
CI_ENTRY BF16_TO_I128
CI_ENTRY F32_TO_S32
CI_ENTRY F32_TO_U32
CI_ENTRY F32_TO_S64
CI_ENTRY F32_TO_U64
CI_ENTRY F32_TO_S128
CI_ENTRY F32_TO_U128
CI_ENTRY F64_TO_S32
CI_ENTRY F64_TO_U32
CI_ENTRY F64_TO_S64
CI_ENTRY F64_TO_U64
CI_ENTRY F64_TO_S128
CI_ENTRY F64_TO_U128
CI_ENTRY F80_TO_I8
CI_ENTRY F80_TO_I16
CI_ENTRY F80_TO_I32
CI_ENTRY F80_TO_I64
CI_ENTRY F80_TO_I128
CI_ENTRY F128_TO_I8
CI_ENTRY F128_TO_I16
CI_ENTRY F128_TO_I32
CI_ENTRY F128_TO_I64
CI_ENTRY F128_TO_I128
CI_ENTRY F16_TO_BF16
CI_ENTRY F16_TO_F32
CI_ENTRY F16_TO_F64
CI_ENTRY F16_TO_F80
CI_ENTRY F16_TO_F128
CI_ENTRY BF16_TO_F16
CI_ENTRY BF16_TO_F32
CI_ENTRY BF16_TO_F64
CI_ENTRY BF16_TO_F80
CI_ENTRY BF16_TO_F128
CI_ENTRY F32_TO_F16
CI_ENTRY F32_TO_BF16
CI_ENTRY F32_TO_F64
CI_ENTRY F32_TO_F80
CI_ENTRY F32_TO_F128
CI_ENTRY F64_TO_F16
CI_ENTRY F64_TO_BF16
CI_ENTRY F64_TO_F32
CI_ENTRY F64_TO_F80
CI_ENTRY F64_TO_F128
CI_ENTRY F80_TO_F16
CI_ENTRY F80_TO_BF16
CI_ENTRY F80_TO_F32
CI_ENTRY F80_TO_F64
CI_ENTRY F80_TO_F128
CI_ENTRY F128_TO_F16
CI_ENTRY F128_TO_BF16
CI_ENTRY F128_TO_F32
CI_ENTRY F128_TO_F64
CI_ENTRY F128_TO_F80
CI_ENTRY ABI_READ_IN8
CI_ENTRY ABI_READ_IN16
CI_ENTRY ABI_READ_IN16_A
CI_ENTRY ABI_READ_IN32
CI_ENTRY ABI_READ_IN32_A
CI_ENTRY ABI_READ_IN64
CI_ENTRY ABI_READ_IN64_A
CI_ENTRY ABI_READ_IN128
CI_ENTRY ABI_READ_IN128_A
CI_ENTRY ABI_READ_IN_N
CI_ENTRY ABI_READ_IN_N_A2
CI_ENTRY ABI_READ_IN_N_A4
CI_ENTRY ABI_READ_IN_N_A8
CI_ENTRY ABI_READ_IN_N_A16
CI_ENTRY ABI_READ_OUT8
CI_ENTRY ABI_READ_OUT16
CI_ENTRY ABI_READ_OUT16_A
CI_ENTRY ABI_READ_OUT32
CI_ENTRY ABI_READ_OUT32_A
CI_ENTRY ABI_READ_OUT64
CI_ENTRY ABI_READ_OUT64_A
CI_ENTRY ABI_READ_OUT128
CI_ENTRY ABI_READ_OUT128_A
CI_ENTRY ABI_READ_OUT_N
CI_ENTRY ABI_READ_OUT_N_A2
CI_ENTRY ABI_READ_OUT_N_A4
CI_ENTRY ABI_READ_OUT_N_A8
CI_ENTRY ABI_READ_OUT_N_A16
CI_ENTRY ABI_WRITE_OUT8
CI_ENTRY ABI_WRITE_OUT16
CI_ENTRY ABI_WRITE_OUT16_A
CI_ENTRY ABI_WRITE_OUT32
CI_ENTRY ABI_WRITE_OUT32_A
CI_ENTRY ABI_WRITE_OUT64
CI_ENTRY ABI_WRITE_OUT64_A
CI_ENTRY ABI_WRITE_OUT128
CI_ENTRY ABI_WRITE_OUT128_A
CI_ENTRY ABI_WRITE_OUT_N
CI_ENTRY ABI_WRITE_OUT_N_A2
CI_ENTRY ABI_WRITE_OUT_N_A4
CI_ENTRY ABI_WRITE_OUT_N_A8
CI_ENTRY ABI_WRITE_OUT_N_A16
CI_ENTRY ABI_WRITE_SEXT8
CI_ENTRY ABI_WRITE_SEXT16
CI_ENTRY ABI_WRITE_SEXT16_A
CI_ENTRY ABI_WRITE_SEXT32
CI_ENTRY ABI_WRITE_SEXT32_A
CI_ENTRY ABI_WRITE_ZEXT8
CI_ENTRY ABI_WRITE_ZEXT16
CI_ENTRY ABI_WRITE_ZEXT16_A
CI_ENTRY ABI_WRITE_ZEXT32
CI_ENTRY ABI_WRITE_ZEXT32_A
CI_ENTRY ABI_COPY_SLOT_TO_REG_PTR_8
CI_ENTRY ABI_COPY_SLOT_TO_REG_PTR_16
CI_ENTRY ABI_COPY_SLOT_TO_REG_PTR_16_A
CI_ENTRY ABI_COPY_SLOT_TO_REG_PTR_32
CI_ENTRY ABI_COPY_SLOT_TO_REG_PTR_32_A
CI_ENTRY ABI_COPY_SLOT_TO_REG_PTR_64
CI_ENTRY ABI_COPY_SLOT_TO_REG_PTR_64_A
CI_ENTRY ABI_COPY_SLOT_TO_REG_PTR_128
CI_ENTRY ABI_COPY_SLOT_TO_REG_PTR_128_A
CI_ENTRY ABI_COPY_SLOT_TO_REG_PTR_N
CI_ENTRY ABI_COPY_SLOT_TO_REG_PTR_N_A2
CI_ENTRY ABI_COPY_SLOT_TO_REG_PTR_N_A4
CI_ENTRY ABI_COPY_SLOT_TO_REG_PTR_N_A8
CI_ENTRY ABI_COPY_SLOT_TO_REG_PTR_N_A16
CI_ENTRY ABI_COPY_REG_PTR_TO_SLOT_8
CI_ENTRY ABI_COPY_REG_PTR_TO_SLOT_16
CI_ENTRY ABI_COPY_REG_PTR_TO_SLOT_16_A
CI_ENTRY ABI_COPY_REG_PTR_TO_SLOT_32
CI_ENTRY ABI_COPY_REG_PTR_TO_SLOT_32_A
CI_ENTRY ABI_COPY_REG_PTR_TO_SLOT_64
CI_ENTRY ABI_COPY_REG_PTR_TO_SLOT_64_A
CI_ENTRY ABI_COPY_REG_PTR_TO_SLOT_128
CI_ENTRY ABI_COPY_REG_PTR_TO_SLOT_128_A
CI_ENTRY ABI_COPY_REG_PTR_TO_SLOT_N
CI_ENTRY ABI_COPY_REG_PTR_TO_SLOT_N_A2
CI_ENTRY ABI_COPY_REG_PTR_TO_SLOT_N_A4
CI_ENTRY ABI_COPY_REG_PTR_TO_SLOT_N_A8
CI_ENTRY ABI_COPY_REG_PTR_TO_SLOT_N_A16
CI_ENTRY ABI_COPY_REG_PTR_INDIRECT_TO_SLOT_8
CI_ENTRY ABI_COPY_REG_PTR_INDIRECT_TO_SLOT_16
CI_ENTRY ABI_COPY_REG_PTR_INDIRECT_TO_SLOT_16_A
CI_ENTRY ABI_COPY_REG_PTR_INDIRECT_TO_SLOT_32
CI_ENTRY ABI_COPY_REG_PTR_INDIRECT_TO_SLOT_32_A
CI_ENTRY ABI_COPY_REG_PTR_INDIRECT_TO_SLOT_64
CI_ENTRY ABI_COPY_REG_PTR_INDIRECT_TO_SLOT_64_A
CI_ENTRY ABI_COPY_REG_PTR_INDIRECT_TO_SLOT_128
CI_ENTRY ABI_COPY_REG_PTR_INDIRECT_TO_SLOT_128_A
CI_ENTRY ABI_COPY_REG_PTR_INDIRECT_TO_SLOT_N
CI_ENTRY ABI_COPY_REG_PTR_INDIRECT_TO_SLOT_N_A2
CI_ENTRY ABI_COPY_REG_PTR_INDIRECT_TO_SLOT_N_A4
CI_ENTRY ABI_COPY_REG_PTR_INDIRECT_TO_SLOT_N_A8
CI_ENTRY ABI_COPY_REG_PTR_INDIRECT_TO_SLOT_N_A16
CI_ENTRY ABI_COPY_SLOT_TO_SLOT_PTR_8
CI_ENTRY ABI_COPY_SLOT_TO_SLOT_PTR_16
CI_ENTRY ABI_COPY_SLOT_TO_SLOT_PTR_16_A
CI_ENTRY ABI_COPY_SLOT_TO_SLOT_PTR_32
CI_ENTRY ABI_COPY_SLOT_TO_SLOT_PTR_32_A
CI_ENTRY ABI_COPY_SLOT_TO_SLOT_PTR_64
CI_ENTRY ABI_COPY_SLOT_TO_SLOT_PTR_64_A
CI_ENTRY ABI_COPY_SLOT_TO_SLOT_PTR_128
CI_ENTRY ABI_COPY_SLOT_TO_SLOT_PTR_128_A
CI_ENTRY ABI_COPY_SLOT_TO_SLOT_PTR_N
CI_ENTRY ABI_COPY_SLOT_TO_SLOT_PTR_N_A2
CI_ENTRY ABI_COPY_SLOT_TO_SLOT_PTR_N_A4
CI_ENTRY ABI_COPY_SLOT_TO_SLOT_PTR_N_A8
CI_ENTRY ABI_COPY_SLOT_TO_SLOT_PTR_N_A16
CI_ENTRY ABI_COPY_INCOMING_TO_INCOMING_PTR_64_A
CI_ENTRY ABI_ADDRESS_REG_PTR_TO_REG_PTR
CI_ENTRY ABI_ADDRESS_REG_PTR_TO_REG_PTR_A
CI_ENTRY ABI_ADDRESS_REG_PTR_TO_REG_PTR_PTR
CI_ENTRY ABI_ADDRESS_REG_PTR_TO_REG_PTR_PTR_A
CI_ENTRY ABI_ADDRESS_SLOT_PTR_TO_REG_PTR
CI_ENTRY ABI_ADDRESS_SLOT_PTR_TO_REG_PTR_A
CI_ENTRY ABI_ADDRESS_SLOT_TO_REG_PTR
CI_ENTRY ABI_ADDRESS_SLOT_TO_REG_PTR_A
CI_ENTRY ABI_ADDRESS_SLOT_TO_SLOT_PTR
CI_ENTRY ABI_ADDRESS_SLOT_TO_SLOT_PTR_A
CI_ENTRY ABI_ADDRESS_INCOMING_PTR_TO_SLOT_PTR_PTR
CI_ENTRY ABI_ADDRESS_INCOMING_PTR_TO_SLOT_PTR_PTR_A
CI_ENTRY ABI_ADDRESS_INCOMING_SAVE_TO_SLOT_PTR_PTR
CI_ENTRY ABI_ADDRESS_INCOMING_SAVE_TO_SLOT_PTR_PTR_A
CI_ENTRY ABI_ZERO_SLOT_8
CI_ENTRY ABI_ZERO_SLOT_16
CI_ENTRY ABI_ZERO_SLOT_16_A
CI_ENTRY ABI_ZERO_SLOT_32
CI_ENTRY ABI_ZERO_SLOT_32_A
CI_ENTRY ABI_ZERO_SLOT_64
CI_ENTRY ABI_ZERO_SLOT_64_A
CI_ENTRY ABI_ZERO_SLOT_128
CI_ENTRY ABI_ZERO_SLOT_128_A
CI_ENTRY ABI_ZERO_SLOT_N
CI_ENTRY ABI_ZERO_SLOT_N_A2
CI_ENTRY ABI_ZERO_SLOT_N_A4
CI_ENTRY ABI_ZERO_SLOT_N_A8
CI_ENTRY ABI_ZERO_SLOT_N_A16
CI_ENTRY ABI_CONSTANT_REG_8
CI_ENTRY ABI_CONSTANT_SLOT_PTR_32
CI_ENTRY ABI_CONSTANT_SLOT_PTR_32_A
CI_ENTRY JZ8
CI_ENTRY JZ16
CI_ENTRY JZ32
CI_ENTRY JZ64
CI_ENTRY JZ128
CI_ENTRY JNZ8
CI_ENTRY JNZ16
CI_ENTRY JNZ32
CI_ENTRY JNZ64
CI_ENTRY JNZ128
CI_ENTRY BSWAP16
CI_ENTRY BSWAP32
CI_ENTRY BSWAP64
CI_ENTRY BOUNDS32
CI_ENTRY BOUNDS_INCLUSIVE32
CI_ENTRY BOUNDS64
CI_ENTRY BOUNDS_INCLUSIVE64
CI_ENTRY INDEX32_S8
CI_ENTRY INDEX32_U8
CI_ENTRY INDEX32_S16
CI_ENTRY INDEX32_U16
CI_ENTRY INDEX32
CI_ENTRY INDEX64_S8
CI_ENTRY INDEX64_U8
CI_ENTRY INDEX64_S16
CI_ENTRY INDEX64_U16
CI_ENTRY INDEX64_S32
CI_ENTRY INDEX64_U32
CI_ENTRY INDEX64
CI_ENTRY LDBF_U8
CI_ENTRY LDBF_S8
CI_ENTRY LDBF_U16
CI_ENTRY LDBF_U16_A
CI_ENTRY LDBF_S16
CI_ENTRY LDBF_S16_A
CI_ENTRY LDBF_U32
CI_ENTRY LDBF_U32_A
CI_ENTRY LDBF_S32
CI_ENTRY LDBF_S32_A
CI_ENTRY LDBF_U64
CI_ENTRY LDBF_U64_A
CI_ENTRY LDBF_S64
CI_ENTRY LDBF_S64_A
CI_ENTRY LDBF_U128
CI_ENTRY LDBF_U128_A
CI_ENTRY LDBF_S128
CI_ENTRY LDBF_S128_A
CI_ENTRY LDBFB_U8
CI_ENTRY LDBFB_S8
CI_ENTRY LDBFB_U16
CI_ENTRY LDBFB_S16
CI_ENTRY LDBFB_U32
CI_ENTRY LDBFB_S32
CI_ENTRY LDBFB_U64
CI_ENTRY LDBFB_S64
CI_ENTRY LDBFB_U128
CI_ENTRY LDBFB_S128
CI_ENTRY STBF8
CI_ENTRY STBF16
CI_ENTRY STBF16_A
CI_ENTRY STBF32
CI_ENTRY STBF32_A
CI_ENTRY STBF64
CI_ENTRY STBF64_A
CI_ENTRY STBF128
CI_ENTRY STBF128_A
CI_ENTRY STBFB8
CI_ENTRY STBFB16
CI_ENTRY STBFB32
CI_ENTRY STBFB64
CI_ENTRY STBFB128
CI_ENTRY CALL_DIRECT
CI_ENTRY CALL_INDIRECT
CI_ENTRY SWITCH_DENSE
CI_ENTRY SWITCH_SIGNED
CI_ENTRY SWITCH_UNSIGNED
CI_ENTRY TRAP
CI_ENTRY UNREACHABLE
CI_ENTRY ABORT
CI_ENTRY DEBUGTRAP
CI_ENTRY BACKTRACE
CI_ENTRY READCYCLECOUNTER
CI_ENTRY READCYCLECOUNTER_DISCARD
CI_ENTRY STACK_ALLOC
CI_ENTRY STACK_ZALLOC
CI_ENTRY THREAD_FENCE
CI_ENTRY SIGNAL_FENCE
CI_ENTRY FFS8
CI_ENTRY FFS16
CI_ENTRY FFS32
CI_ENTRY FFS64
CI_ENTRY FFS128
CI_ENTRY CLRSB8
CI_ENTRY CLRSB16
CI_ENTRY CLRSB32
CI_ENTRY CLRSB64
CI_ENTRY CLRSB128
CI_ENTRY PARITY8
CI_ENTRY PARITY16
CI_ENTRY PARITY32
CI_ENTRY PARITY64
CI_ENTRY PARITY128
CI_ENTRY CLZ8
CI_ENTRY CLZ16
CI_ENTRY CLZ32
CI_ENTRY CLZ64
CI_ENTRY CLZ128
CI_ENTRY CTZ8
CI_ENTRY CTZ16
CI_ENTRY CTZ32
CI_ENTRY CTZ64
CI_ENTRY CTZ128
CI_ENTRY POPCOUNT8
CI_ENTRY POPCOUNT16
CI_ENTRY POPCOUNT32
CI_ENTRY POPCOUNT64
CI_ENTRY POPCOUNT128
CI_ENTRY BIT_WIDTH8
CI_ENTRY BIT_WIDTH16
CI_ENTRY BIT_WIDTH32
CI_ENTRY BIT_WIDTH64
CI_ENTRY BIT_WIDTH128
CI_ENTRY COUNT_ZEROS8
CI_ENTRY COUNT_ZEROS16
CI_ENTRY COUNT_ZEROS32
CI_ENTRY COUNT_ZEROS64
CI_ENTRY COUNT_ZEROS128
CI_ENTRY FIRST_LEADING_ONE8
CI_ENTRY FIRST_LEADING_ONE16
CI_ENTRY FIRST_LEADING_ONE32
CI_ENTRY FIRST_LEADING_ONE64
CI_ENTRY FIRST_LEADING_ONE128
CI_ENTRY FIRST_LEADING_ZERO8
CI_ENTRY FIRST_LEADING_ZERO16
CI_ENTRY FIRST_LEADING_ZERO32
CI_ENTRY FIRST_LEADING_ZERO64
CI_ENTRY FIRST_LEADING_ZERO128
CI_ENTRY FIRST_TRAILING_ZERO8
CI_ENTRY FIRST_TRAILING_ZERO16
CI_ENTRY FIRST_TRAILING_ZERO32
CI_ENTRY FIRST_TRAILING_ZERO64
CI_ENTRY FIRST_TRAILING_ZERO128
CI_ENTRY LEADING_ONES8
CI_ENTRY LEADING_ONES16
CI_ENTRY LEADING_ONES32
CI_ENTRY LEADING_ONES64
CI_ENTRY LEADING_ONES128
CI_ENTRY TRAILING_ONES8
CI_ENTRY TRAILING_ONES16
CI_ENTRY TRAILING_ONES32
CI_ENTRY TRAILING_ONES64
CI_ENTRY TRAILING_ONES128
CI_ENTRY HAS_SINGLE_BIT8
CI_ENTRY HAS_SINGLE_BIT16
CI_ENTRY HAS_SINGLE_BIT32
CI_ENTRY HAS_SINGLE_BIT64
CI_ENTRY HAS_SINGLE_BIT128
CI_ENTRY BIT_CEIL8
CI_ENTRY BIT_CEIL16
CI_ENTRY BIT_CEIL32
CI_ENTRY BIT_CEIL64
CI_ENTRY BIT_CEIL128
CI_ENTRY BIT_FLOOR8
CI_ENTRY BIT_FLOOR16
CI_ENTRY BIT_FLOOR32
CI_ENTRY BIT_FLOOR64
CI_ENTRY BIT_FLOOR128
CI_ENTRY ROTL8
CI_ENTRY ROTR8
CI_ENTRY ROTL16
CI_ENTRY ROTR16
CI_ENTRY ROTL32
CI_ENTRY ROTR32
CI_ENTRY ROTL64
CI_ENTRY ROTR64
CI_ENTRY ROTL128
CI_ENTRY ROTR128
CI_ENTRY CRC32C8
CI_ENTRY CRC32C16
CI_ENTRY CRC32C32
CI_ENTRY CRC32C64
CI_ENTRY ATOMIC_LOAD8
CI_ENTRY ATOMIC_LOAD16
CI_ENTRY ATOMIC_LOAD32
CI_ENTRY ATOMIC_LOAD64
CI_ENTRY ATOMIC_LOAD128
CI_ENTRY ATOMIC_STORE8
CI_ENTRY ATOMIC_STORE16
CI_ENTRY ATOMIC_STORE32
CI_ENTRY ATOMIC_STORE64
CI_ENTRY ATOMIC_STORE128
CI_ENTRY ATOMIC_XCHG8
CI_ENTRY ATOMIC_XCHG16
CI_ENTRY ATOMIC_XCHG32
CI_ENTRY ATOMIC_XCHG64
CI_ENTRY ATOMIC_XCHG128
CI_ENTRY ATOMIC_ADD8
CI_ENTRY ATOMIC_ADD16
CI_ENTRY ATOMIC_ADD32
CI_ENTRY ATOMIC_ADD64
CI_ENTRY ATOMIC_SUB8
CI_ENTRY ATOMIC_SUB16
CI_ENTRY ATOMIC_SUB32
CI_ENTRY ATOMIC_SUB64
CI_ENTRY ATOMIC_AND8
CI_ENTRY ATOMIC_AND16
CI_ENTRY ATOMIC_AND32
CI_ENTRY ATOMIC_AND64
CI_ENTRY ATOMIC_OR8
CI_ENTRY ATOMIC_OR16
CI_ENTRY ATOMIC_OR32
CI_ENTRY ATOMIC_OR64
CI_ENTRY ATOMIC_XOR8
CI_ENTRY ATOMIC_XOR16
CI_ENTRY ATOMIC_XOR32
CI_ENTRY ATOMIC_XOR64
CI_ENTRY ATOMIC_CAS8
CI_ENTRY ATOMIC_CAS_WEAK8
CI_ENTRY ATOMIC_CAS16
CI_ENTRY ATOMIC_CAS_WEAK16
CI_ENTRY ATOMIC_CAS32
CI_ENTRY ATOMIC_CAS_WEAK32
CI_ENTRY ATOMIC_CAS64
CI_ENTRY ATOMIC_CAS_WEAK64
CI_ENTRY ATOMIC_CAS128
CI_ENTRY ATOMIC_CAS_WEAK128
CI_ENTRY VA_ARG_SYSV
CI_ENTRY VA_ARG_ARM
CI_ENTRY VA_ARG_ARM_ALIGN16
CI_ENTRY VA_ARG_ARM_INDIRECT
CI_ENTRY VA_ARG_STACK
CI_ENTRY VA_ARG_STACK_INDIRECT
CI_ENTRY VA_ARG_SYSV_STACK
CI_ENTRY VA_ARG_ARM_STACK
CI_ENTRY ABI_READ_GP0_64
CI_ENTRY ABI_READ_GP0_64_A
CI_ENTRY ABI_WRITE_GP0_64
CI_ENTRY ABI_WRITE_GP0_64_A
CI_ENTRY ABI_READ_GP1_64
CI_ENTRY ABI_READ_GP1_64_A
CI_ENTRY ABI_WRITE_GP1_64
CI_ENTRY ABI_WRITE_GP1_64_A
CI_ENTRY ABI_READ_GP2_64
CI_ENTRY ABI_READ_GP2_64_A
CI_ENTRY ABI_WRITE_GP2_64
CI_ENTRY ABI_WRITE_GP2_64_A
CI_ENTRY ABI_READ_GP3_64
CI_ENTRY ABI_READ_GP3_64_A
CI_ENTRY ABI_WRITE_GP3_64
CI_ENTRY ABI_WRITE_GP3_64_A
CI_ENTRY ABI_READ_GP4_64
CI_ENTRY ABI_READ_GP4_64_A
CI_ENTRY ABI_WRITE_GP4_64
CI_ENTRY ABI_WRITE_GP4_64_A
CI_ENTRY ABI_READ_GP5_64
CI_ENTRY ABI_READ_GP5_64_A
CI_ENTRY ABI_WRITE_GP5_64
CI_ENTRY ABI_WRITE_GP5_64_A
CI_ENTRY ABI_READ_GP6_64
CI_ENTRY ABI_READ_GP6_64_A
CI_ENTRY ABI_WRITE_GP6_64
CI_ENTRY ABI_WRITE_GP6_64_A
CI_ENTRY ABI_READ_GP7_64
CI_ENTRY ABI_READ_GP7_64_A
CI_ENTRY ABI_WRITE_GP7_64
CI_ENTRY ABI_WRITE_GP7_64_A
CI_ENTRY ABI_READ_GP0_8
CI_ENTRY ABI_WRITE_GP0_8
CI_ENTRY ABI_READ_GP0_32
CI_ENTRY ABI_READ_GP0_32_A
CI_ENTRY ABI_WRITE_GP0_32
CI_ENTRY ABI_WRITE_GP0_32_A
CI_ENTRY ABI_SEXT_GP0_8
CI_ENTRY ABI_ZEXT_GP0_8
CI_ENTRY ABI_SEXT_GP0_32
CI_ENTRY ABI_SEXT_GP0_32_A
CI_ENTRY ABI_ZEXT_GP0_32
CI_ENTRY ABI_ZEXT_GP0_32_A
CI_ENTRY ABI_READ_GP1_8
CI_ENTRY ABI_WRITE_GP1_8
CI_ENTRY ABI_READ_GP1_32
CI_ENTRY ABI_READ_GP1_32_A
CI_ENTRY ABI_WRITE_GP1_32
CI_ENTRY ABI_WRITE_GP1_32_A
CI_ENTRY ABI_SEXT_GP1_8
CI_ENTRY ABI_ZEXT_GP1_8
CI_ENTRY ABI_SEXT_GP1_32
CI_ENTRY ABI_SEXT_GP1_32_A
CI_ENTRY ABI_ZEXT_GP1_32
CI_ENTRY ABI_ZEXT_GP1_32_A
CI_ENTRY ABI_READ_GP2_8
CI_ENTRY ABI_WRITE_GP2_8
CI_ENTRY ABI_READ_GP2_32
CI_ENTRY ABI_READ_GP2_32_A
CI_ENTRY ABI_WRITE_GP2_32
CI_ENTRY ABI_WRITE_GP2_32_A
CI_ENTRY ABI_SEXT_GP2_8
CI_ENTRY ABI_ZEXT_GP2_8
CI_ENTRY ABI_SEXT_GP2_32
CI_ENTRY ABI_SEXT_GP2_32_A
CI_ENTRY ABI_ZEXT_GP2_32
CI_ENTRY ABI_ZEXT_GP2_32_A
CI_ENTRY ABI_READ_GP3_8
CI_ENTRY ABI_WRITE_GP3_8
CI_ENTRY ABI_READ_GP3_32
CI_ENTRY ABI_READ_GP3_32_A
CI_ENTRY ABI_WRITE_GP3_32
CI_ENTRY ABI_WRITE_GP3_32_A
CI_ENTRY ABI_SEXT_GP3_8
CI_ENTRY ABI_ZEXT_GP3_8
CI_ENTRY ABI_SEXT_GP3_32
CI_ENTRY ABI_SEXT_GP3_32_A
CI_ENTRY ABI_ZEXT_GP3_32
CI_ENTRY ABI_ZEXT_GP3_32_A
CI_ENTRY ABI_READ_GP4_8
CI_ENTRY ABI_WRITE_GP4_8
CI_ENTRY ABI_READ_GP4_32
CI_ENTRY ABI_READ_GP4_32_A
CI_ENTRY ABI_WRITE_GP4_32
CI_ENTRY ABI_WRITE_GP4_32_A
CI_ENTRY ABI_SEXT_GP4_8
CI_ENTRY ABI_ZEXT_GP4_8
CI_ENTRY ABI_SEXT_GP4_32
CI_ENTRY ABI_SEXT_GP4_32_A
CI_ENTRY ABI_ZEXT_GP4_32
CI_ENTRY ABI_ZEXT_GP4_32_A
CI_ENTRY ABI_READ_GP5_8
CI_ENTRY ABI_WRITE_GP5_8
CI_ENTRY ABI_READ_GP5_32
CI_ENTRY ABI_READ_GP5_32_A
CI_ENTRY ABI_WRITE_GP5_32
CI_ENTRY ABI_WRITE_GP5_32_A
CI_ENTRY ABI_SEXT_GP5_8
CI_ENTRY ABI_ZEXT_GP5_8
CI_ENTRY ABI_SEXT_GP5_32
CI_ENTRY ABI_SEXT_GP5_32_A
CI_ENTRY ABI_ZEXT_GP5_32
CI_ENTRY ABI_ZEXT_GP5_32_A
CI_ENTRY ABI_READ_GP6_8
CI_ENTRY ABI_WRITE_GP6_8
CI_ENTRY ABI_READ_GP6_32
CI_ENTRY ABI_READ_GP6_32_A
CI_ENTRY ABI_WRITE_GP6_32
CI_ENTRY ABI_WRITE_GP6_32_A
CI_ENTRY ABI_SEXT_GP6_8
CI_ENTRY ABI_ZEXT_GP6_8
CI_ENTRY ABI_SEXT_GP6_32
CI_ENTRY ABI_SEXT_GP6_32_A
CI_ENTRY ABI_ZEXT_GP6_32
CI_ENTRY ABI_ZEXT_GP6_32_A
CI_ENTRY ABI_READ_GP7_8
CI_ENTRY ABI_WRITE_GP7_8
CI_ENTRY ABI_READ_GP7_32
CI_ENTRY ABI_READ_GP7_32_A
CI_ENTRY ABI_WRITE_GP7_32
CI_ENTRY ABI_WRITE_GP7_32_A
CI_ENTRY ABI_SEXT_GP7_8
CI_ENTRY ABI_ZEXT_GP7_8
CI_ENTRY ABI_SEXT_GP7_32
CI_ENTRY ABI_SEXT_GP7_32_A
CI_ENTRY ABI_ZEXT_GP7_32
CI_ENTRY ABI_ZEXT_GP7_32_A
CI_ENTRY ABI_READ_FP0_32
CI_ENTRY ABI_READ_FP0_32_A
CI_ENTRY ABI_WRITE_FP0_32
CI_ENTRY ABI_WRITE_FP0_32_A
CI_ENTRY ABI_READ_FP0_64
CI_ENTRY ABI_READ_FP0_64_A
CI_ENTRY ABI_WRITE_FP0_64
CI_ENTRY ABI_WRITE_FP0_64_A
CI_ENTRY ABI_READ_FP0_128
CI_ENTRY ABI_READ_FP0_128_A
CI_ENTRY ABI_WRITE_FP0_128
CI_ENTRY ABI_WRITE_FP0_128_A
CI_ENTRY ABI_READ_FP1_32
CI_ENTRY ABI_READ_FP1_32_A
CI_ENTRY ABI_WRITE_FP1_32
CI_ENTRY ABI_WRITE_FP1_32_A
CI_ENTRY ABI_READ_FP1_64
CI_ENTRY ABI_READ_FP1_64_A
CI_ENTRY ABI_WRITE_FP1_64
CI_ENTRY ABI_WRITE_FP1_64_A
CI_ENTRY ABI_READ_FP1_128
CI_ENTRY ABI_READ_FP1_128_A
CI_ENTRY ABI_WRITE_FP1_128
CI_ENTRY ABI_WRITE_FP1_128_A
CI_ENTRY ABI_READ_FP2_32
CI_ENTRY ABI_READ_FP2_32_A
CI_ENTRY ABI_WRITE_FP2_32
CI_ENTRY ABI_WRITE_FP2_32_A
CI_ENTRY ABI_READ_FP2_64
CI_ENTRY ABI_READ_FP2_64_A
CI_ENTRY ABI_WRITE_FP2_64
CI_ENTRY ABI_WRITE_FP2_64_A
CI_ENTRY ABI_READ_FP2_128
CI_ENTRY ABI_READ_FP2_128_A
CI_ENTRY ABI_WRITE_FP2_128
CI_ENTRY ABI_WRITE_FP2_128_A
CI_ENTRY ABI_READ_FP3_32
CI_ENTRY ABI_READ_FP3_32_A
CI_ENTRY ABI_WRITE_FP3_32
CI_ENTRY ABI_WRITE_FP3_32_A
CI_ENTRY ABI_READ_FP3_64
CI_ENTRY ABI_READ_FP3_64_A
CI_ENTRY ABI_WRITE_FP3_64
CI_ENTRY ABI_WRITE_FP3_64_A
CI_ENTRY ABI_READ_FP3_128
CI_ENTRY ABI_READ_FP3_128_A
CI_ENTRY ABI_WRITE_FP3_128
CI_ENTRY ABI_WRITE_FP3_128_A
CI_ENTRY ABI_READ_FP4_32
CI_ENTRY ABI_READ_FP4_32_A
CI_ENTRY ABI_WRITE_FP4_32
CI_ENTRY ABI_WRITE_FP4_32_A
CI_ENTRY ABI_READ_FP4_64
CI_ENTRY ABI_READ_FP4_64_A
CI_ENTRY ABI_WRITE_FP4_64
CI_ENTRY ABI_WRITE_FP4_64_A
CI_ENTRY ABI_READ_FP4_128
CI_ENTRY ABI_READ_FP4_128_A
CI_ENTRY ABI_WRITE_FP4_128
CI_ENTRY ABI_WRITE_FP4_128_A
CI_ENTRY ABI_READ_FP5_32
CI_ENTRY ABI_READ_FP5_32_A
CI_ENTRY ABI_WRITE_FP5_32
CI_ENTRY ABI_WRITE_FP5_32_A
CI_ENTRY ABI_READ_FP5_64
CI_ENTRY ABI_READ_FP5_64_A
CI_ENTRY ABI_WRITE_FP5_64
CI_ENTRY ABI_WRITE_FP5_64_A
CI_ENTRY ABI_READ_FP5_128
CI_ENTRY ABI_READ_FP5_128_A
CI_ENTRY ABI_WRITE_FP5_128
CI_ENTRY ABI_WRITE_FP5_128_A
CI_ENTRY ABI_READ_FP6_32
CI_ENTRY ABI_READ_FP6_32_A
CI_ENTRY ABI_WRITE_FP6_32
CI_ENTRY ABI_WRITE_FP6_32_A
CI_ENTRY ABI_READ_FP6_64
CI_ENTRY ABI_READ_FP6_64_A
CI_ENTRY ABI_WRITE_FP6_64
CI_ENTRY ABI_WRITE_FP6_64_A
CI_ENTRY ABI_READ_FP6_128
CI_ENTRY ABI_READ_FP6_128_A
CI_ENTRY ABI_WRITE_FP6_128
CI_ENTRY ABI_WRITE_FP6_128_A
CI_ENTRY ABI_READ_FP7_32
CI_ENTRY ABI_READ_FP7_32_A
CI_ENTRY ABI_WRITE_FP7_32
CI_ENTRY ABI_WRITE_FP7_32_A
CI_ENTRY ABI_READ_FP7_64
CI_ENTRY ABI_READ_FP7_64_A
CI_ENTRY ABI_WRITE_FP7_64
CI_ENTRY ABI_WRITE_FP7_64_A
CI_ENTRY ABI_READ_FP7_128
CI_ENTRY ABI_READ_FP7_128_A
CI_ENTRY ABI_WRITE_FP7_128
CI_ENTRY ABI_WRITE_FP7_128_A
CI_ENTRY LDBFS_U8
CI_ENTRY LDBFS_S8
CI_ENTRY LDBFS_U16
CI_ENTRY LDBFS_U16_A
CI_ENTRY LDBFS_S16
CI_ENTRY LDBFS_S16_A
CI_ENTRY LDBFS_U32
CI_ENTRY LDBFS_U32_A
CI_ENTRY LDBFS_S32
CI_ENTRY LDBFS_S32_A
CI_ENTRY LDBFS_U64
CI_ENTRY LDBFS_U64_A
CI_ENTRY LDBFS_S64
CI_ENTRY LDBFS_S64_A
CI_ENTRY LDBFS_U128
CI_ENTRY LDBFS_U128_A
CI_ENTRY LDBFS_S128
CI_ENTRY LDBFS_S128_A
CI_ENTRY LDBFBS_U8
CI_ENTRY LDBFBS_S8
CI_ENTRY LDBFBS_U16
CI_ENTRY LDBFBS_S16
CI_ENTRY LDBFBS_U32
CI_ENTRY LDBFBS_S32
CI_ENTRY LDBFBS_U64
CI_ENTRY LDBFBS_S64
CI_ENTRY LDBFBS_U128
CI_ENTRY LDBFBS_S128
CI_ENTRY BF_JZ8
CI_ENTRY BF_JNZ8
CI_ENTRY BF_JZ16
CI_ENTRY BF_JNZ16
CI_ENTRY BF_JZ32
CI_ENTRY BF_JNZ32
CI_ENTRY BF_JZ64
CI_ENTRY BF_JNZ64
CI_ENTRY BF_JZ_BYTES
CI_ENTRY BF_JNZ_BYTES
CI_ENTRY BFS_JZ8
CI_ENTRY BFS_JNZ8
CI_ENTRY BFS_JZ16
CI_ENTRY BFS_JNZ16
CI_ENTRY BFS_JZ32
CI_ENTRY BFS_JNZ32
CI_ENTRY BFS_JZ64
CI_ENTRY BFS_JNZ64
CI_ENTRY BFS_JZ_BYTES
CI_ENTRY BFS_JNZ_BYTES
CI_ENTRY ABI_READ_GP01_64
CI_ENTRY ABI_READ_GP01_64_A
CI_ENTRY ABI_READ_GP01_64_CONTIG
CI_ENTRY ABI_READ_GP01_64_CONTIG_A
CI_ENTRY ABI_WRITE_GP01_64
CI_ENTRY ABI_WRITE_GP01_64_A
CI_ENTRY ABI_WRITE_GP01_64_CONTIG
CI_ENTRY ABI_WRITE_GP01_64_CONTIG_A

Lci_dispatch_end:
IF (Lci_dispatch_end - Lci_dispatch_table) NE (CI_ASM_op_count * 4)
    .ERR <dispatch table does not cover every opcode>
ENDIF

Lci_msg_null_call:
    DB "call through null function pointer", 0
Lci_msg_hotswap:
    DB "hotswap cycle detected", 0
Lci_msg_string:
    DB "%s", 0
Lci_msg_unknown:
    DB "<unknown>", 0
Lci_msg_missing_function:
    DB "function '%s' not resolved before execution", 0
Lci_msg_result_size:
    DB "result buffer too small (need %zu, have %zu)", 0
Lci_msg_capture_null:
    DB "__builtin_capture_backtrace buffer must not be NULL", 0
Lci_msg_clear_cache:
    DB "__builtin___clear_cache failed", 0
Lci_msg_trap:
    DB "__builtin_trap", 0
Lci_msg_unreachable:
    DB "__builtin_unreachable reached", 0
Lci_msg_abort:
    DB "__builtin_abort called", 0
Lci_msg_close_inclusive:
    DB "]", 0
Lci_msg_close_exclusive:
    DB ")", 0
Lci_msg_bounds_unsigned:
    DB "array subscript out of bounds: index %llu not in [0, %llu%s", 0
Lci_msg_bounds_signed:
    DB "array subscript out of bounds: index %lld not in [0, %llu%s", 0
END
