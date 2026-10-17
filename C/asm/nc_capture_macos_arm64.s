.section __TEXT,__nc_thunks,regular,pure_instructions
.p2align NC_CAPTURE_TABLE_SHIFT
.private_extern _nc_capture_slots
.private_extern _nc_capture_slots_end
.private_extern _nc_capture_entry
.private_extern _nc_abi_invoke
.globl _nc_capture_slots
_nc_capture_slots:
.rept NC_CAPTURE_SLOT_COUNT
0:
    hint #34 // BTI c, a no-op on CPUs without BTI.
    adr x16, 0b
    add x16, x16, #(NC_CAPTURE_TABLE_SIZE >> 12), lsl #12
    ldr x17, [x16, #16]
    br x17
    .space 12
.endr
.globl _nc_capture_slots_end
_nc_capture_slots_end:
.text
.p2align 2
.globl _nc_capture_entry
_nc_capture_entry:
    hint #34
    .cfi_startproc
    sub sp, sp, #304
    .cfi_def_cfa_offset 304
    stp x29, x30, [sp, #288]
    .cfi_offset 29, -16
    .cfi_offset 30, -8
    add x29, sp, #288
    stp x0, x1, [sp, #0]
    stp x2, x3, [sp, #16]
    stp x4, x5, [sp, #32]
    stp x6, x7, [sp, #48]
    str x8, [sp, #64]
    add x9, sp, #304
    str x9, [sp, #72]
    stp q0, q1, [sp, #80]
    stp q2, q3, [sp, #112]
    stp q4, q5, [sp, #144]
    stp q6, q7, [sp, #176]
    ldr x2, [x16, #8]
    ldr x0, [x16]
    mov x1, sp
    blr x2
    ldp x0, x1, [sp, #208]
    ldp q0, q1, [sp, #224]
    ldp q2, q3, [sp, #256]
    ldp x29, x30, [sp, #288]
    .cfi_restore 29
    .cfi_restore 30
    add sp, sp, #304
    .cfi_def_cfa_offset 0
    ret
    .cfi_endproc

.text
.p2align 2
.globl _nc_abi_invoke
_nc_abi_invoke:
    hint #34
    .cfi_startproc
    stp x29, x30, [sp, #-48]!
    .cfi_def_cfa_offset 48
    .cfi_offset 29, -48
    .cfi_offset 30, -40
    mov x29, sp
    .cfi_def_cfa_register 29
    stp x19, x20, [sp, #16]
    stp x21, x22, [sp, #32]
    .cfi_offset 19, -32
    .cfi_offset 20, -24
    .cfi_offset 21, -16
    .cfi_offset 22, -8
    mov x19, x0
    mov x20, x1
    mov x21, x2
    add x9, x2, #15
    and x9, x9, #-16
    sub sp, sp, x9
    mov x9, sp
    neg x3, x3
    and x9, x9, x3
    mov sp, x9
    ldr x10, [x19, #72]
    mov x11, x21
1:
    cbz x11, 2f
    ldrb w12, [x10], #1
    strb w12, [x9], #1
    sub x11, x11, #1
    b 1b
2:
    ldp q0, q1, [x19, #80]
    ldp q2, q3, [x19, #112]
    ldp q4, q5, [x19, #144]
    ldp q6, q7, [x19, #176]
    ldp x0, x1, [x19]
    ldp x2, x3, [x19, #16]
    ldp x4, x5, [x19, #32]
    ldp x6, x7, [x19, #48]
    ldr x8, [x19, #64]
    blr x20
    stp x0, x1, [x19, #208]
    stp q0, q1, [x19, #224]
    stp q2, q3, [x19, #256]
    mov sp, x29
    ldp x19, x20, [sp, #16]
    ldp x21, x22, [sp, #32]
    ldp x29, x30, [sp], #48
    .cfi_def_cfa sp, 0
    ret
    .cfi_endproc
