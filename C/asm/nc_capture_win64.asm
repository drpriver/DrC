IFNDEF NC_LLVM_ML
OPTION CASEMAP:NONE
OPTION DOTNAME

ENDIF
PUBLIC nc_capture_slots, nc_capture_slots_end, nc_capture_entry
NC_THUNKS SEGMENT ALIGN(NC_CAPTURE_TABLE_SIZE) 'CODE'
nc_capture_slots:
NC_SLOT MACRO
    DB 0F3h, 00Fh, 01Eh, 0FAh ; ENDBR64
    ; lea r11, [rip-11]: current slot, with no image-address relocation.
    ; LLVM MASM otherwise encodes a label operand with an absolute address.
    DB 04Ch, 08Dh, 01Dh
    DD -11
    lea r10, [r11+NC_CAPTURE_TABLE_SIZE]
    mov r11, QWORD PTR [r11+NC_CAPTURE_TABLE_SIZE+16]
    jmp r11
    ALIGN 32
ENDM
REPT NC_CAPTURE_TABLE_SIZE / 32
    NC_SLOT
ENDM
nc_capture_slots_end:
NC_THUNKS ENDS
.code
ALIGN 16
nc_capture_entry PROC FRAME
    DB 0F3h, 00Fh, 01Eh, 0FAh
    sub rsp, 168
    .allocstack 168
    .endprolog
    mov QWORD PTR [rsp+32], rcx
    mov QWORD PTR [rsp+40], rdx
    mov QWORD PTR [rsp+48], r8
    mov QWORD PTR [rsp+56], r9
    movdqu XMMWORD PTR [rsp+64], xmm0
    movdqu XMMWORD PTR [rsp+80], xmm1
    movdqu XMMWORD PTR [rsp+96], xmm2
    movdqu XMMWORD PTR [rsp+112], xmm3
    mov QWORD PTR [rsp+176], rcx
    mov QWORD PTR [rsp+184], rdx
    mov QWORD PTR [rsp+192], r8
    mov QWORD PTR [rsp+200], r9
    lea rax, [rsp+208]
    mov QWORD PTR [rsp+128], rax
    mov r11, QWORD PTR [r10+8]
    mov rcx, QWORD PTR [r10]
    lea rdx, [rsp+32]
    call r11
    mov rax, QWORD PTR [rsp+136]
    movdqu xmm0, XMMWORD PTR [rsp+144]
    add rsp, 168
    ret
nc_capture_entry ENDP
EXTERN __chkstk:PROC
PUBLIC nc_abi_invoke
nc_abi_invoke PROC FRAME
    db 0F3h, 00Fh, 01Eh, 0FAh
    push rbp
    .pushreg rbp
    push r12
    .pushreg r12
    push r13
    .pushreg r13
    push rsi
    .pushreg rsi
    push rdi
    .pushreg rdi
    mov rbp, rsp
    .setframe rbp, 0
    .endprolog
    mov r12, rcx
    mov r13, rdx
    lea rax, [r8+95]
    and rax, -64
    call __chkstk
    sub rsp, rax
    and rsp, -64
    mov rcx, r8
    mov rsi, QWORD PTR [r12+96]
    lea rdi, [rsp+32]
    rep movsb
    movdqu xmm0, XMMWORD PTR [r12+32]
    movdqu xmm1, XMMWORD PTR [r12+48]
    movdqu xmm2, XMMWORD PTR [r12+64]
    movdqu xmm3, XMMWORD PTR [r12+80]
    mov rcx, QWORD PTR [r12]
    mov rdx, QWORD PTR [r12+8]
    mov r8, QWORD PTR [r12+16]
    mov r9, QWORD PTR [r12+24]
    call r13
    mov QWORD PTR [r12+104], rax
    movdqu XMMWORD PTR [r12+112], xmm0
    lea rsp, [rbp] ; Recognizable Windows unwind epilog.
    pop rdi
    pop rsi
    pop r13
    pop r12
    pop rbp
    ret
nc_abi_invoke ENDP

END
