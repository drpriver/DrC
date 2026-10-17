.section .text.nc_thunks,"ax",@progbits
.p2align NC_CAPTURE_TABLE_SHIFT
.hidden nc_capture_slots
.hidden nc_capture_slots_end
.hidden nc_capture_entry
.hidden nc_abi_invoke
.globl nc_capture_slots
nc_capture_slots:
.rept NC_CAPTURE_SLOT_COUNT
0:
    endbr64
    leaq 0b(%rip), %r11
    leaq NC_CAPTURE_TABLE_SIZE(%r11), %r10
    movq (NC_CAPTURE_TABLE_SIZE + 16)(%r11), %r11
    jmp *%r11
    .balign 32, 0x90
.endr
.globl nc_capture_slots_end
nc_capture_slots_end:
.text
.p2align 4
.globl nc_capture_entry
nc_capture_entry:
    endbr64
    .cfi_startproc
    pushq %rbp
    .cfi_def_cfa_offset 16
    .cfi_offset %rbp, -16
    movq %rsp, %rbp
    .cfi_def_cfa_register %rbp
    subq $288, %rsp
    movq %rdi, 0(%rsp)
    movq %rsi, 8(%rsp)
    movq %rdx, 16(%rsp)
    movq %rcx, 24(%rsp)
    movq %r8, 32(%rsp)
    movq %r9, 40(%rsp)
    movq %rax, 176(%rsp)
    leaq 16(%rbp), %rax
    movq %rax, 184(%rsp)
    movdqu %xmm0, 48(%rsp)
    movdqu %xmm1, 64(%rsp)
    movdqu %xmm2, 80(%rsp)
    movdqu %xmm3, 96(%rsp)
    movdqu %xmm4, 112(%rsp)
    movdqu %xmm5, 128(%rsp)
    movdqu %xmm6, 144(%rsp)
    movdqu %xmm7, 160(%rsp)
    movq 8(%r10), %r11
    movq 0(%r10), %rdi
    movq %rsp, %rsi
    call *%r11
    movq 192(%rsp), %rax
    movq 200(%rsp), %rdx
    movdqu 208(%rsp), %xmm0
    movdqu 224(%rsp), %xmm1
    cmpq $0, 272(%rsp)
    je 1f
    cmpq $2, 272(%rsp)
    jne 0f
    fldt 256(%rsp)
0:
    fldt 240(%rsp)
1:
    leave
    .cfi_def_cfa %rsp, 8
    .cfi_restore %rbp
    ret
    .cfi_endproc
.section .note.GNU-stack,"",@progbits

.text
.p2align 4
.globl nc_abi_invoke
nc_abi_invoke:
    endbr64
    .cfi_startproc
    pushq %rbp
    .cfi_def_cfa_offset 16
    .cfi_offset %rbp, -16
    movq %rsp, %rbp
    .cfi_def_cfa_register %rbp
    pushq %r12
    pushq %r13
    .cfi_offset %r12, -24
    .cfi_offset %r13, -32
    movq %rdi, %r12
    movq %rsi, %r13
    subq %rdx, %rsp
    negq %rcx
    andq %rcx, %rsp
    movq %rdx, %rcx
    movq 184(%r12), %rsi
    movq %rsp, %rdi
    rep movsb
    movdqu 48(%r12), %xmm0
    movdqu 64(%r12), %xmm1
    movdqu 80(%r12), %xmm2
    movdqu 96(%r12), %xmm3
    movdqu 112(%r12), %xmm4
    movdqu 128(%r12), %xmm5
    movdqu 144(%r12), %xmm6
    movdqu 160(%r12), %xmm7
    movq 0(%r12), %rdi
    movq 8(%r12), %rsi
    movq 16(%r12), %rdx
    movq 24(%r12), %rcx
    movq 32(%r12), %r8
    movq 40(%r12), %r9
    movq 176(%r12), %rax
    call *%r13
    movq %rax, 192(%r12)
    movq %rdx, 200(%r12)
    movdqu %xmm0, 208(%r12)
    movdqu %xmm1, 224(%r12)
    cmpq $0, 272(%r12)
    je 1f
    fstpt 240(%r12)
    cmpq $2, 272(%r12)
    jne 1f
    fstpt 256(%r12)
1:
    leaq -16(%rbp), %rsp
    popq %r13
    popq %r12
    popq %rbp
    .cfi_def_cfa %rsp, 8
    ret
    .cfi_endproc
