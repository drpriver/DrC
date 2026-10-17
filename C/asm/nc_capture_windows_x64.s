.section .nc_thunks,"xr"
.p2align NC_CAPTURE_TABLE_SHIFT
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
.seh_proc nc_capture_entry
nc_capture_entry:
    endbr64
    subq $168, %rsp // 32-byte outgoing shadow space, 128-byte capture, padding.
    .seh_stackalloc 168
    .seh_endprologue
    movq %rcx, 32(%rsp)
    movq %rdx, 40(%rsp)
    movq %r8, 48(%rsp)
    movq %r9, 56(%rsp)
    movdqu %xmm0, 64(%rsp)
    movdqu %xmm1, 80(%rsp)
    movdqu %xmm2, 96(%rsp)
    movdqu %xmm3, 112(%rsp)
    // Home GP arguments: a native Win64 va_list can traverse these and
    // subsequent stack arguments as one contiguous sequence of 8-byte slots.
    movq %rcx, 176(%rsp)
    movq %rdx, 184(%rsp)
    movq %r8, 192(%rsp)
    movq %r9, 200(%rsp)
    leaq 208(%rsp), %rax
    movq %rax, 128(%rsp)
    movq 8(%r10), %r11
    movq 0(%r10), %rcx
    leaq 32(%rsp), %rdx
    call *%r11
    movq 136(%rsp), %rax
    movdqu 144(%rsp), %xmm0
    addq $168, %rsp
    ret
.seh_endproc

.text
.p2align 4
.globl nc_abi_invoke
.seh_proc nc_abi_invoke
nc_abi_invoke:
    endbr64
    pushq %rbp
    .seh_pushreg %rbp
    pushq %r12
    .seh_pushreg %r12
    pushq %r13
    .seh_pushreg %r13
    pushq %rsi
    .seh_pushreg %rsi
    pushq %rdi
    .seh_pushreg %rdi
    movq %rsp, %rbp
    .seh_setframe %rbp, 0
    .seh_endprologue
    movq %rcx, %r12
    movq %rdx, %r13
    leaq 95(%r8), %rax
    andq $-64, %rax
#ifdef __MINGW32__
    call ___chkstk_ms
#else
    call __chkstk
#endif
    subq %rax, %rsp
    andq $-64, %rsp
    movq %r8, %rcx
    movq 96(%r12), %rsi
    leaq 32(%rsp), %rdi
    rep movsb
    movdqu 32(%r12), %xmm0
    movdqu 48(%r12), %xmm1
    movdqu 64(%r12), %xmm2
    movdqu 80(%r12), %xmm3
    movq 0(%r12), %rcx
    movq 8(%r12), %rdx
    movq 16(%r12), %r8
    movq 24(%r12), %r9
    call *%r13
    movq %rax, 104(%r12)
    movdqu %xmm0, 112(%r12)
    leaq 0(%rbp), %rsp // Recognizable Windows unwind epilog.
    popq %rdi
    popq %rsi
    popq %r13
    popq %r12
    popq %rbp
    ret
.seh_endproc
