// Copyright © 2026-2026, David Priver <david@davidpriver.com>
#include "ci_interp_layout.h"
#include "../ci_op_list.h"

.set ci_op_index, 0
.irp opcode, CI_OPCODE_NATIVE_LIST(CI_OPCODE_ENUM) CI_NATIVE_OP_END
    .set \opcode, ci_op_index
    .set ci_op_index, ci_op_index + 1
.endr

//
// Register allocation:
// --------------------
//   - x19: pointer to an interpreter record, laid out as follows on the stack:
//          [0, 8): CiInterpreter*
//          [8, 16): root CiFrame*
//          [16, 24): pointer to table of helper functions
//          [24, 32): scratch pointer
//          [32, 40): pointer to result buffer
//          [40, 48): result buffer size
//          [48, 56): flags
//          [56, 64): free real estate
//   - x23: pointer to the beginning of this frames slots
//   - x24: PC
//   - x26: ops
//   - x28: VM stack
//   - x20: outgoing stack args
//   - x18: reserved (?)
//   Stack has 64 bytes for saved registers then the 64 byte interpreter
//   record. The saved reg and interpreter record are built up on entry
//   to this intepreter, not per interpreted call.
//
.macro CI_FRAME reg
    sub \reg, x23, #CI_ASM_frame_size
.endm
.macro CI_PUBLISH_PC
    CI_FRAME x15
    ldr x9, [x15, #CI_ASM_code]
    add x9, x9, #CI_ASM_code_ops
    sub x9, x24, x9
    lsr x9, x9, #5
    str x9, [x15, #CI_ASM_pc]
.endm
.macro CI_HELPER offset
    ldr x16, [x19, #16]
    ldr x16, [x16, #(\offset * 8)]
    blr x16
.endm
.macro CI_DISPATCH
    ldrh w9, [x24]
    ldr x9, [x26, x9, lsl #3]
    br x9
.endm
.macro CI_NEXT
    add x24, x24, #32
    CI_DISPATCH
.endm

.macro CI_ADDRESS reg, label
    adrp \reg, \label
    add \reg, \reg, :lo12:\label
.endm
.macro CI_ERROR message
    ldr x0, [x19]
    ldr x1, [x24, #24]
    CI_ADDRESS x2, \message
    CI_HELPER CI_ASM_HELPER_error
    b Lci_error
.endm


.text
.p2align 2
.globl ci_interp_execute_asm
.hidden ci_interp_execute_asm
.type ci_interp_execute_asm, %function
ci_interp_execute_asm:
.cfi_startproc
    bti c
    stp x29, x30, [sp, #-128]!
.cfi_def_cfa_offset 128
.cfi_offset 29, -128
.cfi_offset 30, -120
    mov x29, sp
    stp x19, x23, [sp, #16]
    stp x24, x26, [sp, #32]
    stp x28, x20, [sp, #48]
.cfi_offset 19, -112
.cfi_offset 23, -104
.cfi_offset 24, -96
.cfi_offset 26, -88
.cfi_offset 28, -80
.cfi_offset 20, -72
    stp x0, x1, [sp, #64]
    str x2, [sp, #80]
    stp x4, x5, [sp, #96]
    str x6, [sp, #112]
    add x19, sp, #64
.cfi_def_cfa x19, 64
    mov x15, x1
    mov x28, x3
    adrp x26, Lci_dispatch_table
    add x26, x26, :lo12:Lci_dispatch_table
    ldp x0, x1, [x28]
    ldp x2, x3, [x28, #16]
    ldp x4, x5, [x28, #32]
    ldp x6, x7, [x28, #48]
    ldp x8, x20, [x28, #64]
    ldp q0, q1, [x28, #80]
    ldp q2, q3, [x28, #112]
    ldp q4, q5, [x28, #144]
    ldp q6, q7, [x28, #176]
Lci_load_frame:
    add x23, x15, #CI_ASM_frame_size
    ldr x24, [x15, #CI_ASM_pc]
    ldr x9, [x15, #CI_ASM_code]
    ldr x10, [x9, #CI_ASM_code_count]
    cmp x24, x10
    b.hs Lci_finish
    add x9, x9, #CI_ASM_code_ops
    add x24, x9, x24, lsl #5
    b Lci_dispatch
Lci_next:
    add x24, x24, #32
Lci_dispatch:
    CI_DISPATCH
Lci_invalid:
    ldrh w3, [x24]
    CI_ERROR Lci_msg_invalid
Lci_exit:
    sub sp, x19, #64
    ldp x19, x23, [sp, #16]
    ldp x24, x26, [sp, #32]
    ldp x28, x20, [sp, #48]
    ldp x29, x30, [sp], #128
.cfi_restore 19
.cfi_restore 23
.cfi_restore 24
.cfi_restore 26
.cfi_restore 28
.cfi_restore 20
.cfi_restore 29
.cfi_restore 30
.cfi_def_cfa sp, 0
    ret
.cfi_def_cfa x19, 64
.cfi_offset 29, -128
.cfi_offset 30, -120
.cfi_offset 19, -112
.cfi_offset 23, -104
.cfi_offset 24, -96
.cfi_offset 26, -88
.cfi_offset 28, -80
.cfi_offset 20, -72
.macro CI_LABEL name
Lci_\name:
    bti j
.endm
.macro CI_SLOT reg, offset
    ldr w9, [x24, #\offset]
    add \reg, x23, x9
.endm
.macro CI_READ bytes, sign=0, reg=1, base=x1, index=#0
    .if \bytes == 1
        .if \sign
            ldrsb x\reg, [\base, \index]
        .else
            ldrb w\reg, [\base, \index]
        .endif
    .elseif \bytes == 2
        .if \sign
            ldrsh x\reg, [\base, \index]
        .else
            ldrh w\reg, [\base, \index]
        .endif
    .elseif \bytes == 4
        .if \sign
            ldrsw x\reg, [\base, \index]
        .else
            ldr w\reg, [\base, \index]
        .endif
    .elseif \bytes == 8
        ldr x\reg, [\base, \index]
    .else
        .error "invalid scalar width"
    .endif
.endm
.macro CI_READ1 bytes, sign=0
    CI_READ \bytes, \sign
.endm
.macro CI_LOAD_SLOT offset, bytes, sign=0, reg=1
    ldr w9, [x24, #\offset]
    CI_READ \bytes, \sign, \reg, x23, x9
.endm
.macro CI_STORE1 bytes, base=x0, index=#0
    .if \bytes == 1
        strb w1, [\base, \index]
    .elseif \bytes == 2
        strh w1, [\base, \index]
    .elseif \bytes == 4
        str w1, [\base, \index]
    .elseif \bytes == 8
        str x1, [\base, \index]
    .elseif \bytes == 16
        stp x1, x2, [\base, \index]
    .else
        .error "invalid scalar width"
    .endif
.endm
.macro CI_STORE_SLOT offset, bytes
    ldr w0, [x24, #\offset]
    .if \bytes == 16
        add x0, x23, x0
        CI_STORE1 \bytes
    .else
        CI_STORE1 \bytes, x23, x0
    .endif
.endm
.macro CI_MEMORY_FIXED mode, load, store, value
    .if \mode == 0
        ldr w0, [x24, #4]
        \load \value, [x24, #8]
    .elseif \mode <= 2
        ldr w0, [x24, #8]
        ldr w9, [x24, #16]
        .if \mode == 1
            \load \value, [x23, x9]
        .else
            ldr x1, [x23, x9]
            ldr w9, [x24, #20]
            \load \value, [x1, x9]
        .endif
    .elseif \mode == 3
        ldr w9, [x24, #8]
        ldr x0, [x23, x9]
        ldr w9, [x24, #12]
        \load \value, [x23, x9]
        ldr w9, [x24, #20]
    .elseif \mode == 4
        ldr w9, [x24, #4]
        ldr x0, [x23, x9]
        \load \value, [x24, #16]
        ldr w9, [x24, #8]
    .else
        ldr w9, [x24, #8]
        ldr x0, [x23, x9]
        .if \mode == 5
            ldr w9, [x24, #16]
            ldr x1, [x23, x9]
            ldr w9, [x24, #20]
            \load \value, [x1, x9]
        .endif
        ldr w9, [x24, #12]
        .if \mode == 6
            .ifc \value, q1
                add x0, x0, x9
                stp xzr, xzr, [x0]
            .else
                .ifc \value, x1
                    \store xzr, [x0, x9]
                .else
                    \store wzr, [x0, x9]
                .endif
            .endif
            .exitm
        .endif
    .endif
    .if \mode <= 2
        \store \value, [x23, x0]
    .else
        \store \value, [x0, x9]
    .endif
.endm
.macro CI_MEMORY name, mode, bytes
    CI_LABEL \name
    .if \bytes > 0 && \bytes <= 16
        .if \bytes == 1
            CI_MEMORY_FIXED \mode, ldrb, strb, w1
        .elseif \bytes == 2
            CI_MEMORY_FIXED \mode, ldrh, strh, w1
        .elseif \bytes == 4
            CI_MEMORY_FIXED \mode, ldr, str, w1
        .elseif \bytes == 8
            CI_MEMORY_FIXED \mode, ldr, str, x1
        .else
            CI_MEMORY_FIXED \mode, ldr, str, q1
        .endif
        CI_NEXT
        .exitm
    .endif
    .if \mode == 0
        CI_SLOT x0, 4
        add x1, x24, #8
    .elseif \mode == 1
        CI_SLOT x0, 8
        CI_SLOT x1, 16
    .elseif \mode == 2
        CI_SLOT x0, 8
        CI_LOAD_SLOT 16, 8, 0, 1
        ldr w9, [x24, #20]
        add x1, x1, x9
    .elseif \mode == 3
        CI_LOAD_SLOT 8, 8, 0, 0
        ldr w9, [x24, #20]
        add x0, x0, x9
        CI_SLOT x1, 12
    .elseif \mode == 4
        CI_LOAD_SLOT 4, 8, 0, 0
        ldr w9, [x24, #8]
        add x0, x0, x9
        add x1, x24, #16
    .elseif \mode == 5
        CI_LOAD_SLOT 8, 8, 0, 0
        ldr w9, [x24, #12]
        add x0, x0, x9
        CI_LOAD_SLOT 16, 8, 0, 1
        ldr w9, [x24, #20]
        add x1, x1, x9
    .elseif \mode == 6
        CI_LOAD_SLOT 8, 8, 0, 0
        ldr w9, [x24, #12]
        add x0, x0, x9
    .endif
    .if \mode == 0
        ldrb w2, [x24, #3]
    .elseif \mode == 1 || \mode == 2
        ldr w2, [x24, #12]
    .elseif \mode == 3
        ldr w2, [x24, #16]
    .elseif \mode == 4
        ldrb w2, [x24, #2]
    .else
        ldr w2, [x24, #4]
        .if \bytes == -1
            ldr x2, [x23, x2]
        .endif
    .endif
    .if \mode == 6
        mov x1, #0
        CI_HELPER CI_ASM_HELPER_memset
    .else
        CI_HELPER CI_ASM_HELPER_memmove
    .endif
    CI_NEXT
.endm
.macro CI_EXTEND name, from, to, sign
    CI_LABEL \name
    CI_LOAD_SLOT 16, \from, \sign
    .if \to == 16
        .if \sign
            asr x2, x1, #63
        .else
            mov x2, #0
        .endif
    .endif
    CI_STORE_SLOT 8, \to
    CI_NEXT
.endm
.macro CI_OVERFLOW name, bits, uns
    CI_LABEL \name
    CI_SLOT x1, 12
    .if \uns && \bits == 64
        ldr x2, [x1, #8]
    .else
        ldp x1, x2, [x1]
    .endif
    .if \uns
        .if \bits < 64
            orr x2, x2, x1, lsr #\bits
        .endif
        cmp x2, #0
    .else
        .if \bits < 64
            sbfx x3, x1, #0, #\bits
            cmp x1, x3
            asr x3, x3, #63
            ccmp x2, x3, #0, eq
        .else
            cmp x2, x1, asr #63
        .endif
    .endif
    cset w1, ne
    ldr w9, [x24, #8]
    strb w1, [x23, x9]
    CI_NEXT
.endm
.macro CI_CHECKED name, bytes, uns, operation
    CI_LABEL \name
    CI_LOAD_SLOT 12, \bytes, (1-\uns), 4
    CI_LOAD_SLOT 16, \bytes, (1-\uns)
    .if \bytes < 8 && (\bytes < 4 || \operation == 2)
        .if \operation == 0
            add x1, x4, x1
        .elseif \operation == 1
            sub x1, x4, x1
        .else
            mul x1, x4, x1
        .endif
    .elseif \operation == 0
        .if \bytes == 4
            adds w1, w4, w1
        .else
            adds x1, x4, x1
        .endif
        .if \uns
            cset w3, cs
        .else
            cset w3, vs
        .endif
    .elseif \operation == 1
        .if \bytes == 4
            subs w1, w4, w1
        .else
            subs x1, x4, x1
        .endif
        .if \uns
            cset w3, cc
        .else
            cset w3, vs
        .endif
    .else
        .if \uns
            umulh x2, x4, x1
            mul x1, x4, x1
            cmp x2, #0
        .else
            smulh x2, x4, x1
            mul x1, x4, x1
            cmp x2, x1, asr #63
        .endif
        cset w3, ne
    .endif
    .if \bytes < 8 && (\bytes < 4 || \operation == 2)
        .if \uns
            tst x1, #(-1 << (\bytes*8))
        .else
            .if \bytes == 1
                cmp x1, w1, sxtb
            .elseif \bytes == 2
                cmp x1, w1, sxth
            .else
                cmp x1, w1, sxtw
            .endif
        .endif
        cset w3, ne
    .endif
    CI_STORE_SLOT 4, \bytes
    ldr w9, [x24, #8]
    strb w3, [x23, x9]
    CI_NEXT
.endm
.macro CI_TRUTH name, bits, floating, negate
    CI_LABEL \name
    CI_SLOT x1, 16
    .if \bits == 128
        ldp x1, x2, [x1]
        .if \floating
            and x2, x2, #0x7fffffffffffffff
        .endif
        orr x1, x1, x2
        cmp x1, #0
    .elseif \bits == 80
        ldr x2, [x1]
        ldrh w1, [x1, #8]
        and w1, w1, #0x7fff
        orr x1, x1, x2
        cmp x1, #0
    .elseif \floating && \bits == 32
        ldr s1, [x1]
        fcmp s1, #0.0
    .elseif \floating && \bits == 64
        ldr d1, [x1]
        fcmp d1, #0.0
    .else
        CI_READ1 (\bits/8)
        .if \floating
            tst w1, #0x7fff
        .else
            cmp x1, #0
        .endif
    .endif
    .if \negate
        cset w1, eq
    .else
        cset w1, ne
    .endif
    ldr w9, [x24, #8]
    strb w1, [x23, x9]
    CI_NEXT
.endm
.macro CI_COND_JUMP name, bytes, nonzero
    CI_LABEL \name
    .if \bytes == 16
        CI_SLOT x1, 8
        ldp x1, x2, [x1]
        orr x1, x1, x2
    .else
        CI_LOAD_SLOT 8, \bytes
    .endif
    .if \nonzero
        cbnz x1, Lci_JUMP
    .else
        cbz x1, Lci_JUMP
    .endif
    CI_NEXT
.endm
.macro CI_BSWAP name, bytes
    CI_LABEL \name
    CI_LOAD_SLOT 12, \bytes
    .if \bytes == 2
        rev16 w1, w1
    .elseif \bytes == 4
        rev w1, w1
    .else
        rev x1, x1
    .endif
    CI_STORE_SLOT 8, \bytes
    CI_NEXT
.endm
.macro CI_INDEX name, pointer_bytes, index_bytes, sign
    CI_LABEL \name
    CI_LOAD_SLOT 12, \index_bytes, \sign, 2
    CI_LOAD_SLOT 8, \pointer_bytes
    ldr w3, [x24, #16]
    madd x1, x2, x3, x1
    CI_STORE_SLOT 4, \pointer_bytes
    CI_NEXT
.endm
.macro CI_ABI_LOAD bytes, clear_high=1
    .if \bytes == 16
        ldp x11, x12, [x10]
    .elseif \bytes == 8
        ldr x11, [x10]
    .elseif \bytes == 4
        ldr w11, [x10]
    .elseif \bytes == 2
        ldrh w11, [x10]
    .elseif \bytes == 1
        ldrb w11, [x10]
    .else
        bl Lci_abi_load_value
    .endif
    .if \clear_high && \bytes > 0 && \bytes < 16
        mov x12, #0
    .endif
.endm
.macro CI_ABI_STORE bytes
    .if \bytes == 16
        stp x11, x12, [x14]
    .elseif \bytes == 8
        str x11, [x14]
    .elseif \bytes == 4
        str w11, [x14]
    .elseif \bytes == 2
        strh w11, [x14]
    .elseif \bytes == 1
        strb w11, [x14]
    .else
        bl Lci_abi_store_value
    .endif
.endm

.macro CI_ABI_GP_MOVE name, write, reg
    CI_LABEL \name
    ldr w9, [x24, #4]
    .if \write
        ldr x\reg, [x23, x9]
    .else
        str x\reg, [x23, x9]
    .endif
    CI_NEXT
.endm

.macro CI_ABI_MOVE name, incoming, write, bytes, source_bytes=0, sign=0
    CI_LABEL \name
    .if !\bytes
        ldr w15, [x24]
        ubfx w15, w15, #18, #5
    .endif
    .if \incoming
        ldr x10, [x23, #(CI_ASM_incoming - CI_ASM_frame_size)]
        ldrh w9, [x24, #10]
        add x10, x10, x9
        CI_ABI_LOAD \bytes, \write
    .elseif \write
        ldr w9, [x24, #4]
        add x10, x23, x9
        .if \source_bytes
            .if \source_bytes == 1
                .if \sign
                    ldrsb x11, [x10]
                .else
                    ldrb w11, [x10]
                .endif
            .elseif \source_bytes == 2
                .if \sign
                    ldrsh x11, [x10]
                .else
                    ldrh w11, [x10]
                .endif
            .else
                .if \sign
                    ldrsw x11, [x10]
                .else
                    ldr w11, [x10]
                .endif
            .endif
            mov x12, #0
        .else
            CI_ABI_LOAD \bytes
        .endif
    .else
        ldr w13, [x24, #8]
        bl Lci_abi_read_register
    .endif
    .if \write
        ldr w13, [x24, #8]
        bl Lci_abi_write_register
    .else
        ldr w9, [x24, #4]
        add x14, x23, x9
        CI_ABI_STORE \bytes
    .endif
    CI_NEXT
.endm

.macro CI_ABI_ADDRESS reg, mode, offset, pointer
    .if \mode == 0
        ldr w9, [x24, #\offset]
        add \reg, x23, x9
    .elseif \mode == 2
        ldrh w9, [x24, #(\offset+2)]
        ldr \reg, [x23, #(CI_ASM_incoming - CI_ASM_frame_size)]
        add \reg, \reg, x9
    .elseif \mode == 7
        ldr w9, [x24, #\pointer]
        ldr \reg, [x23, x9]
        ldrsw x9, [x24, #\offset]
        add \reg, \reg, x9
    .else
        .if \mode == 3 || \mode == 5
            ldr w13, [x24, #\pointer]
            bl Lci_abi_read_register
            mov \reg, x11
        .else
            ldrh w9, [x24, #(\pointer+2)]
            ldr \reg, [x23, #(CI_ASM_incoming - CI_ASM_frame_size)]
            add \reg, \reg, x9
            .if \mode < 8
                ldr \reg, [\reg]
            .endif
        .endif
        .if \mode < 8
            ldrsw x9, [x24, #\offset]
        .else
            ldr w9, [x24, #\offset]
        .endif
        add \reg, \reg, x9
        .if \mode == 5 || \mode == 6
            ldr \reg, [\reg]
        .endif
    .endif
.endm
.macro CI_ABI_TRANSFER name, operation, dst, src, bytes
    CI_LABEL \name
    .if \dst != 1
        CI_ABI_ADDRESS x14, \dst, 8, 20
    .endif
    .if !\bytes && \operation != 1
        ldr w15, [x24, #12]
    .endif
    .if \operation == 2
        .if \bytes == 1
            strb wzr, [x14]
        .elseif \bytes == 2
            strh wzr, [x14]
        .elseif \bytes == 4
            str wzr, [x14]
        .elseif \bytes == 8
            str xzr, [x14]
        .elseif \bytes == 16
            stp xzr, xzr, [x14]
        .else
            cbz w15, Lci_next
Lci_abi_zero_\name:
            strb wzr, [x14], #1
            subs w15, w15, #1
            b.ne Lci_abi_zero_\name
        .endif
    .else
        .if \operation == 3
            ldr w11, [x24, #4]
            .if \dst == 1 || \bytes == 16
                mov x12, #0
            .endif
        .elseif \src == 1
            ldr w13, [x24, #4]
            bl Lci_abi_read_register
        .else
            CI_ABI_ADDRESS x10, \src, 4, 16
            .if \operation == 1
                mov x11, x10
                .if \dst == 1
                    mov x12, #0
                .endif
            .elseif \dst == 1
                CI_ABI_LOAD \bytes
            .else
                .if \bytes
                    CI_ABI_LOAD \bytes, 0
                    CI_ABI_STORE \bytes
                .else
                    bl Lci_abi_copy
                .endif
            .endif
        .endif
        .if \dst == 1
            ldr w13, [x24, #8]
            bl Lci_abi_write_register
        .elseif \operation == 1
            str x11, [x14]
        .elseif \operation == 3 || \src == 1
            CI_ABI_STORE \bytes
        .endif
    .endif
    CI_NEXT
.endm

Lci_abi_read_register:
    and w9, w13, #255
    ubfx w13, w13, #8, #8
    mov x12, #0
    cmp w9, #5
    b.eq Lci_abi_read_stack
    cmp w9, #6
    b.eq Lci_abi_read_indirect
    cmp w9, #1
    b.eq Lci_abi_read_fp
    cmp w9, #3
    b.eq Lci_abi_read_fp
    adr x9, Lci_abi_read_x_table
    add x9, x9, x13, lsl #4
    br x9
.p2align 4
Lci_abi_read_x_table:
    .irp reg, 0,1,2,3,4,5,6,7
Lci_abi_read_x\reg:
    bti j
        mov x11, x\reg
        ret
        nop
    .endr
Lci_abi_read_fp:
    adr x9, Lci_abi_read_v_table
    add x9, x9, x13, lsl #4
    br x9
.p2align 4
Lci_abi_read_v_table:
    .irp reg, 0,1,2,3,4,5,6,7
Lci_abi_read_v\reg:
    bti j
        mov x11, v\reg\().d[0]
        mov x12, v\reg\().d[1]
        ret
    .endr
Lci_abi_read_stack:
    mov x11, x20
    ret
Lci_abi_read_indirect:
    mov x11, x8
    ret
Lci_abi_write_register:
    and w9, w13, #255
    ubfx w13, w13, #8, #8
    cmp w9, #5
    b.eq Lci_abi_write_stack
    cmp w9, #6
    b.eq Lci_abi_write_indirect
    cmp w9, #1
    b.eq Lci_abi_write_fp
    cmp w9, #3
    b.eq Lci_abi_write_fp
    adr x9, Lci_abi_write_x_table
    add x9, x9, x13, lsl #4
    br x9
.p2align 4
Lci_abi_write_x_table:
    .irp reg, 0,1,2,3,4,5,6,7
Lci_abi_write_x\reg:
    bti j
        mov x\reg, x11
        ret
        nop
    .endr
Lci_abi_write_fp:
    adr x9, Lci_abi_write_v_table
    add x9, x9, x13, lsl #4
    br x9
.p2align 4
Lci_abi_write_v_table:
    .irp reg, 0,1,2,3,4,5,6,7
Lci_abi_write_v\reg:
    bti j
        mov v\reg\().d[0], x11
        mov v\reg\().d[1], x12
        ret
    .endr
Lci_abi_write_stack:
    mov x20, x11
    ret
Lci_abi_write_indirect:
    mov x8, x11
    ret

Lci_abi_load_value:
    cmp w15, #8
    b.eq Lci_abi_load_8
    cmp w15, #16
    b.eq Lci_abi_load_16
    cmp w15, #4
    b.eq Lci_abi_load_4
    cmp w15, #2
    b.eq Lci_abi_load_2
    cmp w15, #1
    b.eq Lci_abi_load_1
    mov x11, #0
    mov x12, #0
    mov w16, #0
Lci_abi_load_loop:
    cmp w16, w15
    b.hs Lci_abi_value_done
    ldrb w9, [x10, x16]
    lsl w17, w16, #3
    lsl x9, x9, x17
    cmp w16, #8
    b.hs Lci_abi_load_high
    orr x11, x11, x9
    b Lci_abi_load_next
Lci_abi_load_high:
    orr x12, x12, x9
Lci_abi_load_next:
    add w16, w16, #1
    b Lci_abi_load_loop
Lci_abi_load_16:
    ldp x11, x12, [x10]
    ret
Lci_abi_load_8:
    ldr x11, [x10]
    mov x12, #0
    ret
Lci_abi_load_4:
    ldr w11, [x10]
    mov x12, #0
    ret
Lci_abi_load_2:
    ldrh w11, [x10]
    mov x12, #0
    ret
Lci_abi_load_1:
    ldrb w11, [x10]
    mov x12, #0
    ret
Lci_abi_store_value:
    cmp w15, #8
    b.eq Lci_abi_store_8
    cmp w15, #16
    b.eq Lci_abi_store_16
    cmp w15, #4
    b.eq Lci_abi_store_4
    cmp w15, #2
    b.eq Lci_abi_store_2
    cmp w15, #1
    b.eq Lci_abi_store_1
    mov w16, #0
Lci_abi_store_loop:
    cmp w16, w15
    b.hs Lci_abi_value_done
    cmp w16, #8
    csel x9, x11, x12, lo
    lsl w17, w16, #3
    lsr x9, x9, x17
    strb w9, [x14, x16]
    add w16, w16, #1
    b Lci_abi_store_loop
Lci_abi_store_16:
    stp x11, x12, [x14]
    ret
Lci_abi_store_8:
    str x11, [x14]
    ret
Lci_abi_store_4:
    str w11, [x14]
    ret
Lci_abi_store_2:
    strh w11, [x14]
    ret
Lci_abi_store_1:
    strb w11, [x14]
    ret
Lci_abi_value_done:
    ret
Lci_abi_copy:
    cbz w15, Lci_abi_value_done
    cmp x14, x10
    b.eq Lci_abi_value_done
    b.lo Lci_abi_copy_forward
    add x10, x10, x15
    add x14, x14, x15
Lci_abi_copy_backward:
    cmp w15, #16
    b.lo Lci_abi_copy_backward_tail
    ldp x11, x12, [x10, #-16]!
    stp x11, x12, [x14, #-16]!
    sub w15, w15, #16
    b Lci_abi_copy_backward
Lci_abi_copy_backward_tail:
    tbz w15, #3, 1f
    ldr x11, [x10, #-8]!
    str x11, [x14, #-8]!
1:
    tbz w15, #2, 2f
    ldr w11, [x10, #-4]!
    str w11, [x14, #-4]!
2:
    tbz w15, #1, 3f
    ldrh w11, [x10, #-2]!
    strh w11, [x14, #-2]!
3:
    tbz w15, #0, Lci_abi_value_done
    ldrb w11, [x10, #-1]
    strb w11, [x14, #-1]
    ret
Lci_abi_copy_forward:
    cmp w15, #16
    b.lo Lci_abi_copy_forward_tail
    ldp x11, x12, [x10], #16
    stp x11, x12, [x14], #16
    sub w15, w15, #16
    b Lci_abi_copy_forward
Lci_abi_copy_forward_tail:
    tbz w15, #3, 1f
    ldr x11, [x10], #8
    str x11, [x14], #8
1:
    tbz w15, #2, 2f
    ldr w11, [x10], #4
    str w11, [x14], #4
2:
    tbz w15, #1, 3f
    ldrh w11, [x10], #2
    strh w11, [x14], #2
3:
    tbz w15, #0, Lci_abi_value_done
    ldrb w11, [x10]
    strb w11, [x14]
    ret

.macro CI_VA_DECODE name
    CI_LABEL \name
    CI_LOAD_SLOT 12, 8, 0, 0
    CI_SLOT x1, 4
    ldrb w2, [x24, #3]
    lsr w3, w2, #4
    and w2, w2, #15
    ldr w4, [x24, #16]
    ldr w5, [x24, #20]
.endm
Lci_soft_padding:
    cmp w5, w4
    b.hs Lci_next
    strb wzr, [x3, x5]
    add w5, w5, #1
    b Lci_soft_padding

.macro CI_VA_STACK_SOURCE indirect=0, skip=0
    ldr x2, [x0]
    add x2, x2, x4
    bic x2, x2, x4
    add x3, x2, x5
    str x3, [x0]
    .if \indirect
        ldr x2, [x2]
    .endif
    str x2, [x1]
    .if \skip
        ldrb w9, [x24, #2]
        add x9, x9, #2
        add x24, x24, x9, lsl #5
        CI_DISPATCH
    .else
        CI_NEXT
    .endif
.endm
.macro CI_VA_ARM name, align_gp=0
    CI_VA_DECODE \name
    ldp w6, w7, [x0, #24]
    .if \align_gp
        add w6, w6, #15
        and w6, w6, #0xfffffff0
    .endif
    cbz w2, Lci_va_gp_ok_\name
    cmp w6, #0
    b.ge Lci_va_overflow_\name
    add w8, w6, w2, lsl #3
    cmp w8, #0
    b.gt Lci_va_overflow_\name
Lci_va_gp_ok_\name:
    cbz w3, Lci_va_fp_ok_\name
    cmp w7, #0
    b.ge Lci_va_overflow_\name
    add w9, w7, w3, lsl #4
    cmp w9, #0
    b.gt Lci_va_overflow_\name
Lci_va_fp_ok_\name:
    mov x10, #0
    mov x11, #0
    cbz w2, Lci_va_no_gp_\name
    ldr x10, [x0, #8]
    add x10, x10, w6, sxtw
    str w8, [x0, #24]
Lci_va_no_gp_\name:
    cbz w3, Lci_va_no_fp_\name
    ldr x11, [x0, #16]
    add x11, x11, w7, sxtw
    str w9, [x0, #28]
Lci_va_no_fp_\name:
    stp x10, x11, [x1]
    CI_NEXT
Lci_va_overflow_\name:
    cbz w2, Lci_va_keep_gp_\name
    str wzr, [x0, #24]
Lci_va_keep_gp_\name:
    cbz w3, Lci_va_keep_fp_\name
    str wzr, [x0, #28]
Lci_va_keep_fp_\name:
    CI_VA_STACK_SOURCE 0, 1
.endm


.macro CI_RESOLVE_FUNCTION
    mov w11, #33
Lci_resolve_loop_\@:
    add x9, x10, #CI_ASM_func_hotswap
    ldar x9, [x9]
    cbz x9, Lci_resolve_done_\@
    mov x10, x9
    subs w11, w11, #1
    b.ne Lci_resolve_loop_\@
    b Lci_hotswap_cycle
Lci_resolve_done_\@:
.endm
.macro CI_CALL_SERVICE name, indirect
    CI_LABEL \name
    CI_PUBLISH_PC
    .if \indirect
        ldr w9, [x24, #12]
        add x9, x23, x9
        ldp x17, x10, [x9, #-16]
    .else
        ldr x9, [x24, #16]
        ldr x10, [x9, #16]
        ldr x17, [x10, #CI_ASM_native_func]
    .endif
    cbz x10, Lci_native_call
    ldrb w9, [x10, #CI_ASM_func_flags]
    tst w9, #64
    b.eq Lci_native_call
    CI_RESOLVE_FUNCTION
    ldr x9, [x10, #CI_ASM_func_code]
    ldr x24, [x9, #8]
    add x24, x24, #CI_ASM_code_ops
    b Lci_dispatch
.endm

Lci_native_call:
    cbz x17, Lci_null_call
    str x17, [x19, #24]
    ldr x9, [x24, #16]
    ldr x9, [x9]
    ldr x15, [x9, #CI_ASM_plan_stack_size]
    cbz x15, Lci_native_invoke
    ldr x12, [x9, #CI_ASM_plan_stack_alignment]
    add x9, x15, #15
    and x9, x9, #-16
    sub x9, sp, x9
    neg x12, x12
    and x9, x9, x12
    mov sp, x9
.cfi_def_cfa x19, 64
    mov x14, sp
    mov x10, x20
    bl Lci_abi_copy
Lci_native_invoke:
    ldr x16, [x19, #24]
    blr x16
    mov sp, x29
.cfi_def_cfa x19, 64
    b Lci_next
Lci_null_call:
    CI_ERROR Lci_msg_null_call

.macro CI_BIT_MASK bits
    .if \bits < 64
        and x4, x4, #((1 << \bits)-1)
    .endif
.endm
.macro CI_CLZ bits
    .if \bits == 128
        clz x1, x5
        clz x2, x4
        add x2, x2, #64
        cmp x5, #0
        csel x1, x2, x1, eq
    .elseif \bits == 32
        clz w1, w4
    .else
        clz x1, x4
        .if \bits < 64
            sub x1, x1, #(64-\bits)
        .endif
    .endif
.endm
.macro CI_CTZ bits
    .if \bits <= 32
        .if \bits < 32
            // The sentinel gives the source width when the input is zero.
            orr w1, w4, #(1 << \bits)
            rbit w1, w1
        .else
            rbit w1, w4
        .endif
        clz w1, w1
    .else
        rbit x1, x4
        clz x1, x1
        .if \bits == 128
            rbit x2, x5
            clz x2, x2
            add x2, x2, #64
            cmp x4, #0
            csel x1, x2, x1, eq
        .endif
    .endif
.endm
.macro CI_POPCOUNT bits
    fmov d0, x4
    .if \bits == 128
        mov v0.d[1], x5
        cnt v0.16b, v0.16b
        addv b0, v0.16b
    .else
        cnt v0.8b, v0.8b
        addv b0, v0.8b
    .endif
    fmov w1, s0
.endm
.macro CI_BIT name, bits, operation
    CI_LABEL \name
    .if \bits == 128
        CI_SLOT x1, 12
        ldp x4, x5, [x1]
    .else
        CI_LOAD_SLOT 12, (\bits/8), (\operation == 1), 4
    .endif
    .if \operation == 1 && \bits <= 64
        .if \bits <= 32
            cls w1, w4
            .if \bits < 32
                sub w1, w1, #(32-\bits)
            .endif
        .else
            cls x1, x4
        .endif
    .elseif \operation == 1
        asr x2, x5, #63
        eor x5, x5, x2
        eor x4, x4, x2
        CI_CLZ \bits
        sub w1, w1, #1
    .elseif \operation == 15 && \bits <= 64
        sub x1, x4, #1
        tst x1, x4
        ccmp x4, #0, #4, eq
        cset w1, ne
    .elseif \operation == 7 && \bits <= 64
        clz x1, x4
        mov x2, #0x8000000000000000
        lsr x1, x2, x1
        cmp x4, #0
        csel x1, x1, xzr, ne
        CI_STORE_SLOT 4, (\bits/8)
        CI_NEXT
        .exitm
    .elseif \operation == 2 || \operation == 5 || \operation == 9 || \operation == 10 || \operation == 15
        CI_POPCOUNT \bits
        .if \operation == 2
            and w1, w1, #1
        .elseif \operation == 10
            mov w2, #\bits
            sub w1, w2, w1
        .elseif \operation == 15
            cmp w1, #1
            cset w1, eq
        .endif
    .elseif \operation == 6 || \operation == 7
        .if \operation == 6
            .if \bits == 128
                cmp x5, #0
                ccmp x4, #1, #2, eq
            .else
                cmp x4, #1
            .endif
            b.ls Lci_bit_one_\name
            subs x4, x4, #1
            .if \bits == 128
                sbc x5, x5, xzr
            .endif
        .endif
        CI_CLZ \bits
        .if \operation == 7 || \bits <= 64
            mov w2, #(\bits-1)
        .else
            mov w2, #\bits
        .endif
        sub w3, w2, w1
        .if \operation == 6 && \bits <= 64
            // Shift two by exponent-1, including the overflow-to-zero case.
            mov x1, #2
            lsl x1, x1, x3
        .else
            mov x1, #0
            .if \bits == 128
                mov x2, #0
            .endif
            cmp w3, #\bits
            b.hs Lci_bit_value_\name
            mov x9, #1
            lsl x9, x9, x3
            .if \bits == 128
                cmp w3, #64
                csel x1, x9, xzr, lo
                csel x2, x9, xzr, hs
            .else
                mov x1, x9
            .endif
        .endif
        .if \operation == 6
            b Lci_bit_value_\name
Lci_bit_one_\name:
            mov x1, #1
            .if \bits == 128
                mov x2, #0
            .endif
        .endif
Lci_bit_value_\name:
        CI_STORE_SLOT 4, (\bits/8)
        CI_NEXT
        .exitm
    .elseif \operation == 20 || \operation == 21
        CI_LOAD_SLOT 16, 8, 0, 3
        .if \bits == 32 || \bits == 64
            .if \operation == 20
                neg x3, x3
            .endif
            .if \bits == 32
                ror w1, w4, w3
            .else
                ror x1, x4, x3
            .endif
        .elseif \bits < 32
            // Repeat the byte/halfword so a word rotate has the same low bits.
            .if \bits == 8
                orr w4, w4, w4, lsl #8
            .endif
            orr w4, w4, w4, lsl #16
            .if \operation == 20
                neg w3, w3
            .endif
            ror w1, w4, w3
        .else
            .if \operation == 21
                neg x3, x3
            .endif
            .if \bits == 128
                tbz x3, #6, Lci_rotate_low_\name
                mov x9, x4
                mov x4, x5
                mov x5, x9
Lci_rotate_low_\name:
                and x3, x3, #63
                mov x1, x4
                mov x2, x5
                cbz x3, Lci_rotate_store_\name
                neg x9, x3
                lsr x10, x5, x9
                lsl x1, x4, x3
                orr x1, x1, x10
                lsr x10, x4, x9
                lsl x2, x5, x3
                orr x2, x2, x10
            .endif
        .endif
Lci_rotate_store_\name:
        CI_STORE_SLOT 4, (\bits/8)
        CI_NEXT
        .exitm
    .else
        .if \operation == 12 || \operation == 14 || \operation == 16 || \operation == 18
            mvn x4, x4
            .if \bits == 128
                mvn x5, x5
            .endif
            CI_BIT_MASK \bits
        .endif
        .if \operation == 0 || \operation == 4 || \operation == 13 || \operation == 14 || \operation == 18 || \operation == 19
            CI_CTZ \bits
        .else
            CI_CLZ \bits
        .endif
        .if \operation == 8
            mov w2, #\bits
            sub w1, w2, w1
        .elseif \operation == 0 || \operation == 11 || \operation == 12 || \operation == 13 || \operation == 14
            add w1, w1, #1
            .if \bits == 128
                orr x2, x4, x5
                cmp x2, #0
            .else
                cmp x4, #0
            .endif
            csel w1, w1, wzr, ne
        .endif
    .endif
    ldr w9, [x24, #4]
    str w1, [x23, x9]
    CI_NEXT
.endm
.macro CI_CRC name, bytes
    CI_LABEL \name
    CI_LOAD_SLOT 16, \bytes
    CI_LOAD_SLOT 12, 4, 0, 0
    mov w2, #\bytes
    CI_HELPER CI_ASM_HELPER_crc
    ldr w1, [x24, #4]
    str w0, [x23, x1]
    CI_NEXT
.endm
.macro CI_BF_JUMP name, bytes, from_slot, nonzero
    CI_LABEL \name
    CI_SLOT x1, 8
    .if \from_slot
        mov x3, x1
    .else
        ldr x3, [x1]
    .endif
    ldr w9, [x24, #12]
    add x3, x3, x9
    ldrb w4, [x24, #2]
    ldrb w7, [x24, #3]
    .if \bytes
        CI_READ \bytes, 0, 1, x3
        lsr x5, x1, x4
        neg x9, x7
        lsl x5, x5, x9
    .else
        mov x5, #0
Lci_bf_test_\name:
        lsr w8, w4, #3
        ldrb w8, [x3, w8, uxtw]
        and w9, w4, #7
        lsr w8, w8, w9
        mov w10, #8
        sub w10, w10, w9
        cmp w10, w7
        csel w10, w10, w7, lo
        mov w11, #1
        lsl w11, w11, w10
        sub w11, w11, #1
        and w8, w8, w11
        orr w5, w5, w8
        add w4, w4, w10
        subs w7, w7, w10
        b.ne Lci_bf_test_\name
    .endif
    .if \nonzero
        cbnz x5, Lci_JUMP
    .else
        cbz x5, Lci_JUMP
    .endif
    CI_NEXT
.endm
.macro CI_BF_LOAD name, bytes, sign, packed, from_slot=0
    CI_LABEL \name
    CI_SLOT x0, 8
    CI_SLOT x1, 16
    .if \from_slot
        mov x3, x1
    .else
        ldr x3, [x1]
    .endif
    ldr w9, [x24, #20]
    add x3, x3, x9
    ldrb w4, [x24, #2]
    ldrb w7, [x24, #3]
    .if !\packed && \bytes < 16
        CI_READ \bytes, 0, 1, x3
        lsr x5, x1, x4
        neg x9, x7
        lsl x5, x5, x9
        .if \sign
            asr x5, x5, x9
        .else
            lsr x5, x5, x9
        .endif
    .elseif !\packed
        mov x5, #0
        mov x6, #0
        cbz w7, Lci_bf_store_\name
        ldp x5, x6, [x3]
        cbz w4, Lci_bf_mask_\name
        cmp w4, #64
        b.hs Lci_bf_upper_\name
        neg x9, x4
        lsl x9, x6, x9
        lsr x5, x5, x4
        orr x5, x5, x9
        lsr x6, x6, x4
        b Lci_bf_mask_\name
Lci_bf_upper_\name:
        lsr x5, x6, x4
        mov x6, #0
Lci_bf_mask_\name:
        neg x9, x7
        cmp w7, #64
        b.hi Lci_bf_mask_high_\name
        lsl x5, x5, x9
        .if \sign
            asr x5, x5, x9
            asr x6, x5, #63
        .else
            lsr x5, x5, x9
            mov x6, #0
        .endif
        b Lci_bf_store_\name
Lci_bf_mask_high_\name:
        lsl x6, x6, x9
        .if \sign
            asr x6, x6, x9
        .else
            lsr x6, x6, x9
        .endif
    .else
        mov x5, #0
        .if \bytes == 16
            mov x6, #0
        .endif
        cbz w7, Lci_bf_store_\name
Lci_bf_read_\name:
        add w8, w4, w7
        sub w9, w8, #1
        lsr w11, w9, #3
        and w9, w9, #7
        add w9, w9, #1
        cmp w9, w7
        csel w9, w9, w7, lo
        ldrb w11, [x3, w11, uxtw]
        sub w8, w8, w9
        and w8, w8, #7
        lsr w11, w11, w8
        mov w12, #1
        lsl w12, w12, w9
        sub w12, w12, #1
        and w11, w11, w12
        .if \bytes == 16
            neg x12, x9
            lsr x12, x5, x12
            lsl x6, x6, x9
            orr x6, x6, x12
        .endif
        lsl x5, x5, x9
        orr x5, x5, x11
        subs w7, w7, w9
        b.ne Lci_bf_read_\name
        .if \sign
            ldrb w7, [x24, #3]
            .if \bytes == 16
                cmp w7, #64
                b.hi Lci_bf_sign_high_\name
            .endif
            neg w9, w7
            lsl x5, x5, x9
            asr x5, x5, x9
            .if \bytes == 16
                asr x6, x5, #63
                b Lci_bf_store_\name
Lci_bf_sign_high_\name:
                neg w9, w7
                lsl x6, x6, x9
                asr x6, x6, x9
            .endif
        .endif
    .endif
Lci_bf_store_\name:
    mov x1, x5
    .if \bytes == 16
        mov x2, x6
    .endif
    CI_STORE1 \bytes
    CI_NEXT
.endm
.macro CI_BF_STORE name, bytes, packed
    CI_LABEL \name
    .if \bytes == 16
        CI_SLOT x1, 12
        ldp x5, x6, [x1]
    .else
        CI_LOAD_SLOT 12, \bytes, 0, 5
    .endif
    CI_SLOT x3, 8
    ldr x3, [x3]
    ldr w9, [x24, #20]
    add x3, x3, x9
    ldrb w4, [x24, #2]
    ldrb w7, [x24, #3]
    .if !\packed && \bytes < 16
        CI_READ \bytes, 0, 1, x3
        neg x9, x7
        mov x2, #-1
        lsr x2, x2, x9
        and x5, x5, x2
        lsl x5, x5, x4
        lsl x2, x2, x4
        bic x1, x1, x2
        orr x1, x1, x5
        mov x0, x3
        CI_STORE1 \bytes
    .elseif !\packed
        cbz w7, Lci_next
        mov x10, #-1
        mov x11, #0
        neg x9, x7
        cmp w7, #64
        b.ls Lci_bf_mask_low_\name
        lsr x11, x10, x9
        b Lci_bf_mask_ready_\name
Lci_bf_mask_low_\name:
        lsr x10, x10, x9
Lci_bf_mask_ready_\name:
        and x5, x5, x10
        and x6, x6, x11
        cbz w4, Lci_bf_merge_\name
        cmp w4, #64
        b.hs Lci_bf_shift_upper_\name
        neg x9, x4
        lsr x12, x5, x9
        lsl x6, x6, x4
        orr x6, x6, x12
        lsl x5, x5, x4
        lsr x12, x10, x9
        lsl x11, x11, x4
        orr x11, x11, x12
        lsl x10, x10, x4
        b Lci_bf_merge_\name
Lci_bf_shift_upper_\name:
        lsl x6, x5, x4
        mov x5, #0
        lsl x11, x10, x4
        mov x10, #0
Lci_bf_merge_\name:
        ldp x1, x2, [x3]
        bic x1, x1, x10
        bic x2, x2, x11
        orr x1, x1, x5
        orr x2, x2, x6
        stp x1, x2, [x3]
    .else
        cbz w7, Lci_next
Lci_bf_write_\name:
        and w8, w4, #7
        mov w9, #8
        sub w9, w9, w8
        cmp w9, w7
        csel w9, w9, w7, lo
        lsr w11, w4, #3
        ldrb w12, [x3, w11, uxtw]
        mov w13, #1
        lsl w13, w13, w9
        sub w13, w13, #1
        and w14, w5, w13
        lsl w14, w14, w8
        lsl w13, w13, w8
        bic w12, w12, w13
        orr w12, w12, w14
        strb w12, [x3, w11, uxtw]
        .if \bytes == 16
            neg x12, x9
            lsl x12, x6, x12
            lsr x5, x5, x9
            orr x5, x5, x12
            lsr x6, x6, x9
        .else
            lsr x5, x5, x9
        .endif
        add w4, w4, w9
        subs w7, w7, w9
        b.ne Lci_bf_write_\name
    .endif
    CI_NEXT
.endm

.macro CI_ATOMIC_LOAD name, bytes
    CI_LABEL \name
    CI_LOAD_SLOT 16, 8, 0, 3
    ldr w9, [x24, #20]
    add x3, x3, x9
    .if \bytes == 1
        ldarb w1, [x3]
    .elseif \bytes == 2
        ldarh w1, [x3]
    .elseif \bytes == 4
        ldar w1, [x3]
    .elseif \bytes == 8
        ldar x1, [x3]
    .else
Lci_atomic_load_\name:
        ldaxp x1, x2, [x3]
        stlxp w9, x1, x2, [x3]
        cbnz w9, Lci_atomic_load_\name
    .endif
    CI_STORE_SLOT 8, \bytes
    CI_NEXT
.endm
.macro CI_ATOMIC_STORE name, bytes
    CI_LABEL \name
    CI_LOAD_SLOT 8, 8, 0, 3
    ldr w9, [x24, #20]
    add x3, x3, x9
    .if \bytes == 16
        CI_SLOT x1, 12
        ldp x4, x5, [x1]
Lci_atomic_store_\name:
        ldaxp x1, x2, [x3]
        stlxp w9, x4, x5, [x3]
        cbnz w9, Lci_atomic_store_\name
    .else
        CI_LOAD_SLOT 12, \bytes
        .if \bytes == 1
            stlrb w1, [x3]
        .elseif \bytes == 2
            stlrh w1, [x3]
        .elseif \bytes == 4
            stlr w1, [x3]
        .else
            stlr x1, [x3]
        .endif
    .endif
    CI_NEXT
.endm
.macro CI_EXCLUSIVE_READ bytes
    .if \bytes == 1
        ldaxrb w1, [x3]
    .elseif \bytes == 2
        ldaxrh w1, [x3]
    .elseif \bytes == 4
        ldaxr w1, [x3]
    .elseif \bytes == 8
        ldaxr x1, [x3]
    .else
        ldaxp x1, x2, [x3]
    .endif
.endm
.macro CI_EXCLUSIVE_WRITE bytes
    .if \bytes == 1
        stlxrb w9, w7, [x3]
    .elseif \bytes == 2
        stlxrh w9, w7, [x3]
    .elseif \bytes == 4
        stlxr w9, w7, [x3]
    .elseif \bytes == 8
        stlxr w9, x7, [x3]
    .else
        stlxp w9, x7, x8, [x3]
    .endif
.endm
.macro CI_ATOMIC_RMW name, bytes, operation
    CI_LABEL \name
    CI_LOAD_SLOT 16, 8, 0, 3
    ldr w9, [x24, #4]
    add x3, x3, x9
    .if \bytes == 16
        CI_SLOT x1, 20
        ldp x7, x8, [x1]
    .elseif \operation == 0
        CI_LOAD_SLOT 20, \bytes, 0, 7
    .else
        CI_LOAD_SLOT 20, \bytes, 0, 4
    .endif
Lci_rmw_\name:
    CI_EXCLUSIVE_READ \bytes
    .if \operation == 1
        add x7, x1, x4
    .elseif \operation == 2
        sub x7, x1, x4
    .elseif \operation == 3
        and x7, x1, x4
    .elseif \operation == 4
        orr x7, x1, x4
    .elseif \operation == 5
        eor x7, x1, x4
    .endif
    CI_EXCLUSIVE_WRITE \bytes
    cbnz w9, Lci_rmw_\name
    CI_STORE_SLOT 8, \bytes
    CI_NEXT
.endm
.macro CI_ATOMIC_CAS name, bytes, weak
    CI_LABEL \name
    CI_LOAD_SLOT 12, 8, 0, 3
    ldr w9, [x24, #4]
    add x3, x3, x9
    CI_SLOT x0, 16
    .if \bytes == 16
        ldp x4, x5, [x0]
    .else
        CI_READ \bytes, 0, 4, x0
    .endif
    .if \bytes == 16
        CI_SLOT x1, 20
        ldp x7, x8, [x1]
    .else
        CI_LOAD_SLOT 20, \bytes, 0, 7
    .endif
Lci_cas_\name:
    CI_EXCLUSIVE_READ \bytes
    cmp x1, x4
    .if \bytes == 16
        ccmp x2, x5, #0, eq
    .endif
    b.ne Lci_cas_fail_\name
    CI_EXCLUSIVE_WRITE \bytes
    .if \weak
        cbnz w9, Lci_cas_fail_\name
    .else
        cbnz w9, Lci_cas_\name
    .endif
    mov w1, #1
    b Lci_cas_result_\name
Lci_cas_fail_\name:
    clrex
    CI_STORE1 \bytes
    mov w1, #0
Lci_cas_result_\name:
    ldr w9, [x24, #8]
    strb w1, [x23, x9]
    CI_NEXT
.endm
.macro CI_ITOF name, bytes, uns, float_bits
    CI_LABEL \name
    CI_LOAD_SLOT 16, \bytes, (1-\uns)
    .if \float_bits == 32
        .if \uns
            ucvtf s1, x1
        .else
            scvtf s1, x1
        .endif
        ldr w0, [x24, #8]
        str s1, [x23, x0]
    .else
        .if \uns
            ucvtf d1, x1
        .else
            scvtf d1, x1
        .endif
        ldr w0, [x24, #8]
        str d1, [x23, x0]
    .endif
    CI_NEXT
.endm
.macro CI_FTOI name, float_bits, bytes, uns
    CI_LABEL \name
    ldr w9, [x24, #16]
    .if \float_bits == 32
        ldr s1, [x23, x9]
        .if \uns
            fcvtzu x1, s1
        .else
            fcvtzs x1, s1
        .endif
    .else
        ldr d1, [x23, x9]
        .if \uns
            fcvtzu x1, d1
        .else
            fcvtzs x1, d1
        .endif
    .endif
    CI_STORE_SLOT 8, \bytes
    CI_NEXT
.endm
.macro CI_FTOF name, source, dest
    CI_LABEL \name
    ldr w9, [x24, #16]
    .if \source == 32
        ldr s1, [x23, x9]
        fcvt d1, s1
    .else
        ldr d1, [x23, x9]
        fcvt s1, d1
    .endif
    ldr w0, [x24, #8]
    .if \dest == 32
        str s1, [x23, x0]
    .else
        str d1, [x23, x0]
    .endif
    CI_NEXT
.endm
.macro CI_BOUNDS name, bytes, inclusive
    CI_LABEL \name
    CI_LOAD_SLOT 8, \bytes, 0, 4
    CI_LOAD_SLOT 16, \bytes
    cmp x4, x1
    .if \inclusive
        b.hi Lci_bounds_error_\name
    .else
        b.hs Lci_bounds_error_\name
    .endif
    CI_NEXT
Lci_bounds_error_\name:
    mov x3, x4
    mov x4, x1
    .if \inclusive
        CI_ADDRESS x5, Lci_msg_close_inclusive
    .else
        CI_ADDRESS x5, Lci_msg_close_exclusive
    .endif
    ldrb w9, [x24, #2]
    tbnz w9, #1, Lci_bounds_signed_\name
    CI_ERROR Lci_msg_bounds_unsigned
Lci_bounds_signed_\name:
    CI_ERROR Lci_msg_bounds_signed
.endm
.macro CI_SWITCH name, dense, sign
    CI_LABEL \name
    CI_LOAD_SLOT 8, 8
    ldr x3, [x24, #16]
    ldr x4, [x3, #CI_ASM_switch_count]
    add x3, x3, #CI_ASM_switch_data
    .if \dense
        ldr x5, [x3]
        sub x1, x1, x5
        cmp x1, x4
        b.hs Lci_switch_miss_\name
        mov x9, #CI_ASM_entry_size
        madd x3, x1, x9, x3
        b Lci_switch_found_\name
    .else
        mov x5, #0
Lci_switch_search_\name:
        cmp x5, x4
        b.hs Lci_switch_miss_\name
        sub x6, x4, x5
        add x6, x5, x6, lsr #1
        mov x9, #CI_ASM_entry_size
        madd x7, x6, x9, x3
        ldp x8, x9, [x7]
        cmp x9, x1
        .if \sign
            b.lt Lci_switch_right_\name
        .else
            b.lo Lci_switch_right_\name
        .endif
        cmp x8, x1
        .if \sign
            b.gt Lci_switch_left_\name
        .else
            b.hi Lci_switch_left_\name
        .endif
        mov x3, x7
        b Lci_switch_found_\name
Lci_switch_right_\name:
        add x5, x6, #1
        b Lci_switch_search_\name
Lci_switch_left_\name:
        mov x4, x6
        b Lci_switch_search_\name
    .endif
Lci_switch_found_\name:
    ldrsw x9, [x3, #CI_ASM_entry_target]
    add x24, x24, x9, lsl #5
    CI_DISPATCH
Lci_switch_miss_\name:
    b Lci_JUMP
.endm

Lci_ADD8:
    bti j
    ldr w0, [x24, #4]
    ldr w9, [x24, #8]
    ldrb w1, [x23, x9]
    ldr w9, [x24, #12]
    ldrb w2, [x23, x9]
    add x1, x1, x2
    strb w1, [x23, x0]
    CI_NEXT
Lci_SUB8:
    bti j
    ldr w0, [x24, #4]
    ldr w9, [x24, #8]
    ldrb w1, [x23, x9]
    ldr w9, [x24, #12]
    ldrb w2, [x23, x9]
    sub x1, x1, x2
    strb w1, [x23, x0]
    CI_NEXT
Lci_MUL8:
    bti j
    ldr w0, [x24, #4]
    ldr w9, [x24, #8]
    ldrb w1, [x23, x9]
    ldr w9, [x24, #12]
    ldrb w2, [x23, x9]
    mul x1, x1, x2
    strb w1, [x23, x0]
    CI_NEXT
Lci_SDIV8:
    bti j
    ldr w0, [x24, #4]
    ldr w9, [x24, #8]
    ldrsb x1, [x23, x9]
    ldr w9, [x24, #12]
    ldrsb x2, [x23, x9]
    sdiv x1, x1, x2
    strb w1, [x23, x0]
    CI_NEXT
Lci_UDIV8:
    bti j
    ldr w0, [x24, #4]
    ldr w9, [x24, #8]
    ldrb w1, [x23, x9]
    ldr w9, [x24, #12]
    ldrb w2, [x23, x9]
    udiv x1, x1, x2
    strb w1, [x23, x0]
    CI_NEXT
Lci_SMOD8:
    bti j
    ldr w0, [x24, #4]
    ldr w9, [x24, #8]
    ldrsb x1, [x23, x9]
    ldr w9, [x24, #12]
    ldrsb x2, [x23, x9]
    sdiv x3, x1, x2
    msub x1, x3, x2, x1
    strb w1, [x23, x0]
    CI_NEXT
Lci_UMOD8:
    bti j
    ldr w0, [x24, #4]
    ldr w9, [x24, #8]
    ldrb w1, [x23, x9]
    ldr w9, [x24, #12]
    ldrb w2, [x23, x9]
    udiv x3, x1, x2
    msub x1, x3, x2, x1
    strb w1, [x23, x0]
    CI_NEXT
Lci_AND8:
    bti j
    ldr w0, [x24, #4]
    ldr w9, [x24, #8]
    ldrb w1, [x23, x9]
    ldr w9, [x24, #12]
    ldrb w2, [x23, x9]
    and x1, x1, x2
    strb w1, [x23, x0]
    CI_NEXT
Lci_OR8:
    bti j
    ldr w0, [x24, #4]
    ldr w9, [x24, #8]
    ldrb w1, [x23, x9]
    ldr w9, [x24, #12]
    ldrb w2, [x23, x9]
    orr x1, x1, x2
    strb w1, [x23, x0]
    CI_NEXT
Lci_XOR8:
    bti j
    ldr w0, [x24, #4]
    ldr w9, [x24, #8]
    ldrb w1, [x23, x9]
    ldr w9, [x24, #12]
    ldrb w2, [x23, x9]
    eor x1, x1, x2
    strb w1, [x23, x0]
    CI_NEXT
Lci_SHL8:
    bti j
    ldr w0, [x24, #4]
    ldr w9, [x24, #8]
    ldrb w1, [x23, x9]
    ldr w9, [x24, #12]
    ldrb w2, [x23, x9]
    lsl x1, x1, x2
    strb w1, [x23, x0]
    CI_NEXT
Lci_ASR8:
    bti j
    ldr w0, [x24, #4]
    ldr w9, [x24, #8]
    ldrsb x1, [x23, x9]
    ldr w9, [x24, #12]
    ldrb w2, [x23, x9]
    asr x1, x1, x2
    strb w1, [x23, x0]
    CI_NEXT
Lci_LSR8:
    bti j
    ldr w0, [x24, #4]
    ldr w9, [x24, #8]
    ldrb w1, [x23, x9]
    ldr w9, [x24, #12]
    ldrb w2, [x23, x9]
    lsr x1, x1, x2
    strb w1, [x23, x0]
    CI_NEXT
Lci_NEG8:
    bti j
    ldr w0, [x24, #4]
    ldr w9, [x24, #8]
    ldrb w1, [x23, x9]
    neg x1, x1
    strb w1, [x23, x0]
    CI_NEXT
Lci_NOT8:
    bti j
    ldr w0, [x24, #4]
    ldr w9, [x24, #8]
    ldrb w1, [x23, x9]
    mvn x1, x1
    strb w1, [x23, x0]
    CI_NEXT
Lci_ADD16:
    bti j
    ldr w0, [x24, #4]
    ldr w9, [x24, #8]
    ldrh w1, [x23, x9]
    ldr w9, [x24, #12]
    ldrh w2, [x23, x9]
    add x1, x1, x2
    strh w1, [x23, x0]
    CI_NEXT
Lci_SUB16:
    bti j
    ldr w0, [x24, #4]
    ldr w9, [x24, #8]
    ldrh w1, [x23, x9]
    ldr w9, [x24, #12]
    ldrh w2, [x23, x9]
    sub x1, x1, x2
    strh w1, [x23, x0]
    CI_NEXT
Lci_MUL16:
    bti j
    ldr w0, [x24, #4]
    ldr w9, [x24, #8]
    ldrh w1, [x23, x9]
    ldr w9, [x24, #12]
    ldrh w2, [x23, x9]
    mul x1, x1, x2
    strh w1, [x23, x0]
    CI_NEXT
Lci_SDIV16:
    bti j
    ldr w0, [x24, #4]
    ldr w9, [x24, #8]
    ldrsh x1, [x23, x9]
    ldr w9, [x24, #12]
    ldrsh x2, [x23, x9]
    sdiv x1, x1, x2
    strh w1, [x23, x0]
    CI_NEXT
Lci_UDIV16:
    bti j
    ldr w0, [x24, #4]
    ldr w9, [x24, #8]
    ldrh w1, [x23, x9]
    ldr w9, [x24, #12]
    ldrh w2, [x23, x9]
    udiv x1, x1, x2
    strh w1, [x23, x0]
    CI_NEXT
Lci_SMOD16:
    bti j
    ldr w0, [x24, #4]
    ldr w9, [x24, #8]
    ldrsh x1, [x23, x9]
    ldr w9, [x24, #12]
    ldrsh x2, [x23, x9]
    sdiv x3, x1, x2
    msub x1, x3, x2, x1
    strh w1, [x23, x0]
    CI_NEXT
Lci_UMOD16:
    bti j
    ldr w0, [x24, #4]
    ldr w9, [x24, #8]
    ldrh w1, [x23, x9]
    ldr w9, [x24, #12]
    ldrh w2, [x23, x9]
    udiv x3, x1, x2
    msub x1, x3, x2, x1
    strh w1, [x23, x0]
    CI_NEXT
Lci_AND16:
    bti j
    ldr w0, [x24, #4]
    ldr w9, [x24, #8]
    ldrh w1, [x23, x9]
    ldr w9, [x24, #12]
    ldrh w2, [x23, x9]
    and x1, x1, x2
    strh w1, [x23, x0]
    CI_NEXT
Lci_OR16:
    bti j
    ldr w0, [x24, #4]
    ldr w9, [x24, #8]
    ldrh w1, [x23, x9]
    ldr w9, [x24, #12]
    ldrh w2, [x23, x9]
    orr x1, x1, x2
    strh w1, [x23, x0]
    CI_NEXT
Lci_XOR16:
    bti j
    ldr w0, [x24, #4]
    ldr w9, [x24, #8]
    ldrh w1, [x23, x9]
    ldr w9, [x24, #12]
    ldrh w2, [x23, x9]
    eor x1, x1, x2
    strh w1, [x23, x0]
    CI_NEXT
Lci_SHL16:
    bti j
    ldr w0, [x24, #4]
    ldr w9, [x24, #8]
    ldrh w1, [x23, x9]
    ldr w9, [x24, #12]
    ldrh w2, [x23, x9]
    lsl x1, x1, x2
    strh w1, [x23, x0]
    CI_NEXT
Lci_ASR16:
    bti j
    ldr w0, [x24, #4]
    ldr w9, [x24, #8]
    ldrsh x1, [x23, x9]
    ldr w9, [x24, #12]
    ldrh w2, [x23, x9]
    asr x1, x1, x2
    strh w1, [x23, x0]
    CI_NEXT
Lci_LSR16:
    bti j
    ldr w0, [x24, #4]
    ldr w9, [x24, #8]
    ldrh w1, [x23, x9]
    ldr w9, [x24, #12]
    ldrh w2, [x23, x9]
    lsr x1, x1, x2
    strh w1, [x23, x0]
    CI_NEXT
Lci_NEG16:
    bti j
    ldr w0, [x24, #4]
    ldr w9, [x24, #8]
    ldrh w1, [x23, x9]
    neg x1, x1
    strh w1, [x23, x0]
    CI_NEXT
Lci_NOT16:
    bti j
    ldr w0, [x24, #4]
    ldr w9, [x24, #8]
    ldrh w1, [x23, x9]
    mvn x1, x1
    strh w1, [x23, x0]
    CI_NEXT
Lci_ADD32:
    bti j
    ldr w0, [x24, #4]
    ldr w9, [x24, #8]
    ldr w1, [x23, x9]
    ldr w9, [x24, #12]
    ldr w2, [x23, x9]
    add x1, x1, x2
    str w1, [x23, x0]
    CI_NEXT
Lci_SUB32:
    bti j
    ldr w0, [x24, #4]
    ldr w9, [x24, #8]
    ldr w1, [x23, x9]
    ldr w9, [x24, #12]
    ldr w2, [x23, x9]
    sub x1, x1, x2
    str w1, [x23, x0]
    CI_NEXT
Lci_MUL32:
    bti j
    ldr w0, [x24, #4]
    ldr w9, [x24, #8]
    ldr w1, [x23, x9]
    ldr w9, [x24, #12]
    ldr w2, [x23, x9]
    mul x1, x1, x2
    str w1, [x23, x0]
    CI_NEXT
Lci_SDIV32:
    bti j
    ldr w0, [x24, #4]
    ldr w9, [x24, #8]
    ldr w1, [x23, x9]
    ldr w9, [x24, #12]
    ldr w2, [x23, x9]
    sdiv w1, w1, w2
    str w1, [x23, x0]
    CI_NEXT
Lci_UDIV32:
    bti j
    ldr w0, [x24, #4]
    ldr w9, [x24, #8]
    ldr w1, [x23, x9]
    ldr w9, [x24, #12]
    ldr w2, [x23, x9]
    udiv w1, w1, w2
    str w1, [x23, x0]
    CI_NEXT
Lci_SMOD32:
    bti j
    ldr w0, [x24, #4]
    ldr w9, [x24, #8]
    ldr w1, [x23, x9]
    ldr w9, [x24, #12]
    ldr w2, [x23, x9]
    sdiv w3, w1, w2
    msub w1, w3, w2, w1
    str w1, [x23, x0]
    CI_NEXT
Lci_UMOD32:
    bti j
    ldr w0, [x24, #4]
    ldr w9, [x24, #8]
    ldr w1, [x23, x9]
    ldr w9, [x24, #12]
    ldr w2, [x23, x9]
    udiv w3, w1, w2
    msub w1, w3, w2, w1
    str w1, [x23, x0]
    CI_NEXT
Lci_AND32:
    bti j
    ldr w0, [x24, #4]
    ldr w9, [x24, #8]
    ldr w1, [x23, x9]
    ldr w9, [x24, #12]
    ldr w2, [x23, x9]
    and x1, x1, x2
    str w1, [x23, x0]
    CI_NEXT
Lci_OR32:
    bti j
    ldr w0, [x24, #4]
    ldr w9, [x24, #8]
    ldr w1, [x23, x9]
    ldr w9, [x24, #12]
    ldr w2, [x23, x9]
    orr x1, x1, x2
    str w1, [x23, x0]
    CI_NEXT
Lci_XOR32:
    bti j
    ldr w0, [x24, #4]
    ldr w9, [x24, #8]
    ldr w1, [x23, x9]
    ldr w9, [x24, #12]
    ldr w2, [x23, x9]
    eor x1, x1, x2
    str w1, [x23, x0]
    CI_NEXT
Lci_SHL32:
    bti j
    ldr w0, [x24, #4]
    ldr w9, [x24, #8]
    ldr w1, [x23, x9]
    ldr w9, [x24, #12]
    ldr w2, [x23, x9]
    lsl x1, x1, x2
    str w1, [x23, x0]
    CI_NEXT
Lci_ASR32:
    bti j
    ldr w0, [x24, #4]
    ldr w9, [x24, #8]
    ldrsw x1, [x23, x9]
    ldr w9, [x24, #12]
    ldr w2, [x23, x9]
    asr x1, x1, x2
    str w1, [x23, x0]
    CI_NEXT
Lci_LSR32:
    bti j
    ldr w0, [x24, #4]
    ldr w9, [x24, #8]
    ldr w1, [x23, x9]
    ldr w9, [x24, #12]
    ldr w2, [x23, x9]
    lsr x1, x1, x2
    str w1, [x23, x0]
    CI_NEXT
Lci_ADD64:
    bti j
    ldr w0, [x24, #4]
    ldr w9, [x24, #8]
    ldr x1, [x23, x9]
    ldr w9, [x24, #12]
    ldr x2, [x23, x9]
    add x1, x1, x2
    str x1, [x23, x0]
    CI_NEXT
Lci_SUB64:
    bti j
    ldr w0, [x24, #4]
    ldr w9, [x24, #8]
    ldr x1, [x23, x9]
    ldr w9, [x24, #12]
    ldr x2, [x23, x9]
    sub x1, x1, x2
    str x1, [x23, x0]
    CI_NEXT
Lci_MUL64:
    bti j
    ldr w0, [x24, #4]
    ldr w9, [x24, #8]
    ldr x1, [x23, x9]
    ldr w9, [x24, #12]
    ldr x2, [x23, x9]
    mul x1, x1, x2
    str x1, [x23, x0]
    CI_NEXT
Lci_SDIV64:
    bti j
    ldr w0, [x24, #4]
    ldr w9, [x24, #8]
    ldr x1, [x23, x9]
    ldr w9, [x24, #12]
    ldr x2, [x23, x9]
    sdiv x1, x1, x2
    str x1, [x23, x0]
    CI_NEXT
Lci_UDIV64:
    bti j
    ldr w0, [x24, #4]
    ldr w9, [x24, #8]
    ldr x1, [x23, x9]
    ldr w9, [x24, #12]
    ldr x2, [x23, x9]
    udiv x1, x1, x2
    str x1, [x23, x0]
    CI_NEXT
Lci_SMOD64:
    bti j
    ldr w0, [x24, #4]
    ldr w9, [x24, #8]
    ldr x1, [x23, x9]
    ldr w9, [x24, #12]
    ldr x2, [x23, x9]
    sdiv x3, x1, x2
    msub x1, x3, x2, x1
    str x1, [x23, x0]
    CI_NEXT
Lci_UMOD64:
    bti j
    ldr w0, [x24, #4]
    ldr w9, [x24, #8]
    ldr x1, [x23, x9]
    ldr w9, [x24, #12]
    ldr x2, [x23, x9]
    udiv x3, x1, x2
    msub x1, x3, x2, x1
    str x1, [x23, x0]
    CI_NEXT
Lci_AND64:
    bti j
    ldr w0, [x24, #4]
    ldr w9, [x24, #8]
    ldr x1, [x23, x9]
    ldr w9, [x24, #12]
    ldr x2, [x23, x9]
    and x1, x1, x2
    str x1, [x23, x0]
    CI_NEXT
Lci_OR64:
    bti j
    ldr w0, [x24, #4]
    ldr w9, [x24, #8]
    ldr x1, [x23, x9]
    ldr w9, [x24, #12]
    ldr x2, [x23, x9]
    orr x1, x1, x2
    str x1, [x23, x0]
    CI_NEXT
Lci_XOR64:
    bti j
    ldr w0, [x24, #4]
    ldr w9, [x24, #8]
    ldr x1, [x23, x9]
    ldr w9, [x24, #12]
    ldr x2, [x23, x9]
    eor x1, x1, x2
    str x1, [x23, x0]
    CI_NEXT
Lci_SHL64:
    bti j
    ldr w0, [x24, #4]
    ldr w9, [x24, #8]
    ldr x1, [x23, x9]
    ldr w9, [x24, #12]
    ldr x2, [x23, x9]
    lsl x1, x1, x2
    str x1, [x23, x0]
    CI_NEXT
Lci_ASR64:
    bti j
    ldr w0, [x24, #4]
    ldr w9, [x24, #8]
    ldr x1, [x23, x9]
    ldr w9, [x24, #12]
    ldr x2, [x23, x9]
    asr x1, x1, x2
    str x1, [x23, x0]
    CI_NEXT
Lci_LSR64:
    bti j
    ldr w0, [x24, #4]
    ldr w9, [x24, #8]
    ldr x1, [x23, x9]
    ldr w9, [x24, #12]
    ldr x2, [x23, x9]
    lsr x1, x1, x2
    str x1, [x23, x0]
    CI_NEXT
Lci_ADD_IMM32:
    bti j
    ldr w0, [x24, #4]
    ldr w9, [x24, #8]
    ldr w1, [x23, x9]
    ldr w2, [x24, #16]
    add x1, x1, x2
    str w1, [x23, x0]
    CI_NEXT
Lci_SUB_IMM32:
    bti j
    ldr w0, [x24, #4]
    ldr w9, [x24, #8]
    ldr w1, [x23, x9]
    ldr w2, [x24, #16]
    sub x1, x1, x2
    str w1, [x23, x0]
    CI_NEXT
Lci_MUL_IMM32:
    bti j
    ldr w0, [x24, #4]
    ldr w9, [x24, #8]
    ldr w1, [x23, x9]
    ldr w2, [x24, #16]
    mul x1, x1, x2
    str w1, [x23, x0]
    CI_NEXT
Lci_SDIV_IMM32:
    bti j
    ldr w0, [x24, #4]
    ldr w9, [x24, #8]
    ldr w1, [x23, x9]
    ldr w2, [x24, #16]
    sdiv w1, w1, w2
    str w1, [x23, x0]
    CI_NEXT
Lci_UDIV_IMM32:
    bti j
    ldr w0, [x24, #4]
    ldr w9, [x24, #8]
    ldr w1, [x23, x9]
    ldr w2, [x24, #16]
    udiv w1, w1, w2
    str w1, [x23, x0]
    CI_NEXT
Lci_SMOD_IMM32:
    bti j
    ldr w0, [x24, #4]
    ldr w9, [x24, #8]
    ldr w1, [x23, x9]
    ldr w2, [x24, #16]
    sdiv w3, w1, w2
    msub w1, w3, w2, w1
    str w1, [x23, x0]
    CI_NEXT
Lci_UMOD_IMM32:
    bti j
    ldr w0, [x24, #4]
    ldr w9, [x24, #8]
    ldr w1, [x23, x9]
    ldr w2, [x24, #16]
    udiv w3, w1, w2
    msub w1, w3, w2, w1
    str w1, [x23, x0]
    CI_NEXT
Lci_AND_IMM32:
    bti j
    ldr w0, [x24, #4]
    ldr w9, [x24, #8]
    ldr w1, [x23, x9]
    ldr w2, [x24, #16]
    and x1, x1, x2
    str w1, [x23, x0]
    CI_NEXT
Lci_OR_IMM32:
    bti j
    ldr w0, [x24, #4]
    ldr w9, [x24, #8]
    ldr w1, [x23, x9]
    ldr w2, [x24, #16]
    orr x1, x1, x2
    str w1, [x23, x0]
    CI_NEXT
Lci_XOR_IMM32:
    bti j
    ldr w0, [x24, #4]
    ldr w9, [x24, #8]
    ldr w1, [x23, x9]
    ldr w2, [x24, #16]
    eor x1, x1, x2
    str w1, [x23, x0]
    CI_NEXT
Lci_SHL_IMM32:
    bti j
    ldr w0, [x24, #4]
    ldr w9, [x24, #8]
    ldr w1, [x23, x9]
    ldr w2, [x24, #16]
    lsl x1, x1, x2
    str w1, [x23, x0]
    CI_NEXT
Lci_ASR_IMM32:
    bti j
    ldr w0, [x24, #4]
    ldr w9, [x24, #8]
    ldrsw x1, [x23, x9]
    ldr w2, [x24, #16]
    asr x1, x1, x2
    str w1, [x23, x0]
    CI_NEXT
Lci_LSR_IMM32:
    bti j
    ldr w0, [x24, #4]
    ldr w9, [x24, #8]
    ldr w1, [x23, x9]
    ldr w2, [x24, #16]
    lsr x1, x1, x2
    str w1, [x23, x0]
    CI_NEXT
Lci_ADD_IMM64:
    bti j
    ldr w0, [x24, #4]
    ldr w9, [x24, #8]
    ldr x1, [x23, x9]
    ldr x2, [x24, #16]
    add x1, x1, x2
    str x1, [x23, x0]
    CI_NEXT
Lci_SUB_IMM64:
    bti j
    ldr w0, [x24, #4]
    ldr w9, [x24, #8]
    ldr x1, [x23, x9]
    ldr x2, [x24, #16]
    sub x1, x1, x2
    str x1, [x23, x0]
    CI_NEXT
Lci_MUL_IMM64:
    bti j
    ldr w0, [x24, #4]
    ldr w9, [x24, #8]
    ldr x1, [x23, x9]
    ldr x2, [x24, #16]
    mul x1, x1, x2
    str x1, [x23, x0]
    CI_NEXT
Lci_SDIV_IMM64:
    bti j
    ldr w0, [x24, #4]
    ldr w9, [x24, #8]
    ldr x1, [x23, x9]
    ldr x2, [x24, #16]
    sdiv x1, x1, x2
    str x1, [x23, x0]
    CI_NEXT
Lci_UDIV_IMM64:
    bti j
    ldr w0, [x24, #4]
    ldr w9, [x24, #8]
    ldr x1, [x23, x9]
    ldr x2, [x24, #16]
    udiv x1, x1, x2
    str x1, [x23, x0]
    CI_NEXT
Lci_SMOD_IMM64:
    bti j
    ldr w0, [x24, #4]
    ldr w9, [x24, #8]
    ldr x1, [x23, x9]
    ldr x2, [x24, #16]
    sdiv x3, x1, x2
    msub x1, x3, x2, x1
    str x1, [x23, x0]
    CI_NEXT
Lci_UMOD_IMM64:
    bti j
    ldr w0, [x24, #4]
    ldr w9, [x24, #8]
    ldr x1, [x23, x9]
    ldr x2, [x24, #16]
    udiv x3, x1, x2
    msub x1, x3, x2, x1
    str x1, [x23, x0]
    CI_NEXT
Lci_AND_IMM64:
    bti j
    ldr w0, [x24, #4]
    ldr w9, [x24, #8]
    ldr x1, [x23, x9]
    ldr x2, [x24, #16]
    and x1, x1, x2
    str x1, [x23, x0]
    CI_NEXT
Lci_OR_IMM64:
    bti j
    ldr w0, [x24, #4]
    ldr w9, [x24, #8]
    ldr x1, [x23, x9]
    ldr x2, [x24, #16]
    orr x1, x1, x2
    str x1, [x23, x0]
    CI_NEXT
Lci_XOR_IMM64:
    bti j
    ldr w0, [x24, #4]
    ldr w9, [x24, #8]
    ldr x1, [x23, x9]
    ldr x2, [x24, #16]
    eor x1, x1, x2
    str x1, [x23, x0]
    CI_NEXT
Lci_SHL_IMM64:
    bti j
    ldr w0, [x24, #4]
    ldr w9, [x24, #8]
    ldr x1, [x23, x9]
    ldr x2, [x24, #16]
    lsl x1, x1, x2
    str x1, [x23, x0]
    CI_NEXT
Lci_ASR_IMM64:
    bti j
    ldr w0, [x24, #4]
    ldr w9, [x24, #8]
    ldr x1, [x23, x9]
    ldr x2, [x24, #16]
    asr x1, x1, x2
    str x1, [x23, x0]
    CI_NEXT
Lci_LSR_IMM64:
    bti j
    ldr w0, [x24, #4]
    ldr w9, [x24, #8]
    ldr x1, [x23, x9]
    ldr x2, [x24, #16]
    lsr x1, x1, x2
    str x1, [x23, x0]
    CI_NEXT
Lci_NEG32:
    bti j
    ldr w0, [x24, #4]
    ldr w9, [x24, #8]
    ldr w1, [x23, x9]
    neg x1, x1
    str w1, [x23, x0]
    CI_NEXT
Lci_NOT32:
    bti j
    ldr w0, [x24, #4]
    ldr w9, [x24, #8]
    ldr w1, [x23, x9]
    mvn x1, x1
    str w1, [x23, x0]
    CI_NEXT
Lci_NEG64:
    bti j
    ldr w0, [x24, #4]
    ldr w9, [x24, #8]
    ldr x1, [x23, x9]
    neg x1, x1
    str x1, [x23, x0]
    CI_NEXT
Lci_NOT64:
    bti j
    ldr w0, [x24, #4]
    ldr w9, [x24, #8]
    ldr x1, [x23, x9]
    mvn x1, x1
    str x1, [x23, x0]
    CI_NEXT
Lci_EQ32:
    bti j
    ldr w9, [x24, #12]
    ldr w1, [x23, x9]
    ldr w9, [x24, #16]
    ldr w2, [x23, x9]
    cmp w1, w2
    cset w1, eq
    ldr w9, [x24, #4]
    strb w1, [x23, x9]
    CI_NEXT
Lci_NE32:
    bti j
    ldr w9, [x24, #12]
    ldr w1, [x23, x9]
    ldr w9, [x24, #16]
    ldr w2, [x23, x9]
    cmp w1, w2
    cset w1, ne
    ldr w9, [x24, #4]
    strb w1, [x23, x9]
    CI_NEXT
Lci_SLT32:
    bti j
    ldr w9, [x24, #12]
    ldr w1, [x23, x9]
    ldr w9, [x24, #16]
    ldr w2, [x23, x9]
    cmp w1, w2
    cset w1, lt
    ldr w9, [x24, #4]
    strb w1, [x23, x9]
    CI_NEXT
Lci_ULT32:
    bti j
    ldr w9, [x24, #12]
    ldr w1, [x23, x9]
    ldr w9, [x24, #16]
    ldr w2, [x23, x9]
    cmp w1, w2
    cset w1, lo
    ldr w9, [x24, #4]
    strb w1, [x23, x9]
    CI_NEXT
Lci_SGT32:
    bti j
    ldr w9, [x24, #12]
    ldr w1, [x23, x9]
    ldr w9, [x24, #16]
    ldr w2, [x23, x9]
    cmp w1, w2
    cset w1, gt
    ldr w9, [x24, #4]
    strb w1, [x23, x9]
    CI_NEXT
Lci_UGT32:
    bti j
    ldr w9, [x24, #12]
    ldr w1, [x23, x9]
    ldr w9, [x24, #16]
    ldr w2, [x23, x9]
    cmp w1, w2
    cset w1, hi
    ldr w9, [x24, #4]
    strb w1, [x23, x9]
    CI_NEXT
Lci_SLE32:
    bti j
    ldr w9, [x24, #12]
    ldr w1, [x23, x9]
    ldr w9, [x24, #16]
    ldr w2, [x23, x9]
    cmp w1, w2
    cset w1, le
    ldr w9, [x24, #4]
    strb w1, [x23, x9]
    CI_NEXT
Lci_ULE32:
    bti j
    ldr w9, [x24, #12]
    ldr w1, [x23, x9]
    ldr w9, [x24, #16]
    ldr w2, [x23, x9]
    cmp w1, w2
    cset w1, ls
    ldr w9, [x24, #4]
    strb w1, [x23, x9]
    CI_NEXT
Lci_SGE32:
    bti j
    ldr w9, [x24, #12]
    ldr w1, [x23, x9]
    ldr w9, [x24, #16]
    ldr w2, [x23, x9]
    cmp w1, w2
    cset w1, ge
    ldr w9, [x24, #4]
    strb w1, [x23, x9]
    CI_NEXT
Lci_UGE32:
    bti j
    ldr w9, [x24, #12]
    ldr w1, [x23, x9]
    ldr w9, [x24, #16]
    ldr w2, [x23, x9]
    cmp w1, w2
    cset w1, hs
    ldr w9, [x24, #4]
    strb w1, [x23, x9]
    CI_NEXT
Lci_EQ64:
    bti j
    ldr w9, [x24, #12]
    ldr x1, [x23, x9]
    ldr w9, [x24, #16]
    ldr x2, [x23, x9]
    cmp x1, x2
    cset w1, eq
    ldr w9, [x24, #4]
    strb w1, [x23, x9]
    CI_NEXT
Lci_NE64:
    bti j
    ldr w9, [x24, #12]
    ldr x1, [x23, x9]
    ldr w9, [x24, #16]
    ldr x2, [x23, x9]
    cmp x1, x2
    cset w1, ne
    ldr w9, [x24, #4]
    strb w1, [x23, x9]
    CI_NEXT
Lci_SLT64:
    bti j
    ldr w9, [x24, #12]
    ldr x1, [x23, x9]
    ldr w9, [x24, #16]
    ldr x2, [x23, x9]
    cmp x1, x2
    cset w1, lt
    ldr w9, [x24, #4]
    strb w1, [x23, x9]
    CI_NEXT
Lci_ULT64:
    bti j
    ldr w9, [x24, #12]
    ldr x1, [x23, x9]
    ldr w9, [x24, #16]
    ldr x2, [x23, x9]
    cmp x1, x2
    cset w1, lo
    ldr w9, [x24, #4]
    strb w1, [x23, x9]
    CI_NEXT
Lci_SGT64:
    bti j
    ldr w9, [x24, #12]
    ldr x1, [x23, x9]
    ldr w9, [x24, #16]
    ldr x2, [x23, x9]
    cmp x1, x2
    cset w1, gt
    ldr w9, [x24, #4]
    strb w1, [x23, x9]
    CI_NEXT
Lci_UGT64:
    bti j
    ldr w9, [x24, #12]
    ldr x1, [x23, x9]
    ldr w9, [x24, #16]
    ldr x2, [x23, x9]
    cmp x1, x2
    cset w1, hi
    ldr w9, [x24, #4]
    strb w1, [x23, x9]
    CI_NEXT
Lci_SLE64:
    bti j
    ldr w9, [x24, #12]
    ldr x1, [x23, x9]
    ldr w9, [x24, #16]
    ldr x2, [x23, x9]
    cmp x1, x2
    cset w1, le
    ldr w9, [x24, #4]
    strb w1, [x23, x9]
    CI_NEXT
Lci_ULE64:
    bti j
    ldr w9, [x24, #12]
    ldr x1, [x23, x9]
    ldr w9, [x24, #16]
    ldr x2, [x23, x9]
    cmp x1, x2
    cset w1, ls
    ldr w9, [x24, #4]
    strb w1, [x23, x9]
    CI_NEXT
Lci_SGE64:
    bti j
    ldr w9, [x24, #12]
    ldr x1, [x23, x9]
    ldr w9, [x24, #16]
    ldr x2, [x23, x9]
    cmp x1, x2
    cset w1, ge
    ldr w9, [x24, #4]
    strb w1, [x23, x9]
    CI_NEXT
Lci_UGE64:
    bti j
    ldr w9, [x24, #12]
    ldr x1, [x23, x9]
    ldr w9, [x24, #16]
    ldr x2, [x23, x9]
    cmp x1, x2
    cset w1, hs
    ldr w9, [x24, #4]
    strb w1, [x23, x9]
    CI_NEXT
Lci_EQ_JUMP32:
    bti j
    ldr w9, [x24, #12]
    ldr w1, [x23, x9]
    ldr w9, [x24, #16]
    ldr w2, [x23, x9]
    cmp w1, w2
    cset w1, eq
    ldr w9, [x24]
    eor w9, w9, w1, lsl #16
    tbnz w9, #16, Lci_next
    ldrsw x9, [x24, #4]
    add x24, x24, x9, lsl #5
    b Lci_dispatch
Lci_NE_JUMP32:
    bti j
    ldr w9, [x24, #12]
    ldr w1, [x23, x9]
    ldr w9, [x24, #16]
    ldr w2, [x23, x9]
    cmp w1, w2
    cset w1, ne
    ldr w9, [x24]
    eor w9, w9, w1, lsl #16
    tbnz w9, #16, Lci_next
    ldrsw x9, [x24, #4]
    add x24, x24, x9, lsl #5
    b Lci_dispatch
Lci_SLT_JUMP32:
    bti j
    ldr w9, [x24, #12]
    ldr w1, [x23, x9]
    ldr w9, [x24, #16]
    ldr w2, [x23, x9]
    cmp w1, w2
    cset w1, lt
    ldr w9, [x24]
    eor w9, w9, w1, lsl #16
    tbnz w9, #16, Lci_next
    ldrsw x9, [x24, #4]
    add x24, x24, x9, lsl #5
    b Lci_dispatch
Lci_ULT_JUMP32:
    bti j
    ldr w9, [x24, #12]
    ldr w1, [x23, x9]
    ldr w9, [x24, #16]
    ldr w2, [x23, x9]
    cmp w1, w2
    cset w1, lo
    ldr w9, [x24]
    eor w9, w9, w1, lsl #16
    tbnz w9, #16, Lci_next
    ldrsw x9, [x24, #4]
    add x24, x24, x9, lsl #5
    b Lci_dispatch
Lci_SGT_JUMP32:
    bti j
    ldr w9, [x24, #12]
    ldr w1, [x23, x9]
    ldr w9, [x24, #16]
    ldr w2, [x23, x9]
    cmp w1, w2
    cset w1, gt
    ldr w9, [x24]
    eor w9, w9, w1, lsl #16
    tbnz w9, #16, Lci_next
    ldrsw x9, [x24, #4]
    add x24, x24, x9, lsl #5
    b Lci_dispatch
Lci_UGT_JUMP32:
    bti j
    ldr w9, [x24, #12]
    ldr w1, [x23, x9]
    ldr w9, [x24, #16]
    ldr w2, [x23, x9]
    cmp w1, w2
    cset w1, hi
    ldr w9, [x24]
    eor w9, w9, w1, lsl #16
    tbnz w9, #16, Lci_next
    ldrsw x9, [x24, #4]
    add x24, x24, x9, lsl #5
    b Lci_dispatch
Lci_SLE_JUMP32:
    bti j
    ldr w9, [x24, #12]
    ldr w1, [x23, x9]
    ldr w9, [x24, #16]
    ldr w2, [x23, x9]
    cmp w1, w2
    cset w1, le
    ldr w9, [x24]
    eor w9, w9, w1, lsl #16
    tbnz w9, #16, Lci_next
    ldrsw x9, [x24, #4]
    add x24, x24, x9, lsl #5
    b Lci_dispatch
Lci_ULE_JUMP32:
    bti j
    ldr w9, [x24, #12]
    ldr w1, [x23, x9]
    ldr w9, [x24, #16]
    ldr w2, [x23, x9]
    cmp w1, w2
    cset w1, ls
    ldr w9, [x24]
    eor w9, w9, w1, lsl #16
    tbnz w9, #16, Lci_next
    ldrsw x9, [x24, #4]
    add x24, x24, x9, lsl #5
    b Lci_dispatch
Lci_SGE_JUMP32:
    bti j
    ldr w9, [x24, #12]
    ldr w1, [x23, x9]
    ldr w9, [x24, #16]
    ldr w2, [x23, x9]
    cmp w1, w2
    cset w1, ge
    ldr w9, [x24]
    eor w9, w9, w1, lsl #16
    tbnz w9, #16, Lci_next
    ldrsw x9, [x24, #4]
    add x24, x24, x9, lsl #5
    b Lci_dispatch
Lci_UGE_JUMP32:
    bti j
    ldr w9, [x24, #12]
    ldr w1, [x23, x9]
    ldr w9, [x24, #16]
    ldr w2, [x23, x9]
    cmp w1, w2
    cset w1, hs
    ldr w9, [x24]
    eor w9, w9, w1, lsl #16
    tbnz w9, #16, Lci_next
    ldrsw x9, [x24, #4]
    add x24, x24, x9, lsl #5
    b Lci_dispatch
Lci_EQ_JUMP64:
    bti j
    ldr w9, [x24, #12]
    ldr x1, [x23, x9]
    ldr w9, [x24, #16]
    ldr x2, [x23, x9]
    cmp x1, x2
    cset w1, eq
    ldr w9, [x24]
    eor w9, w9, w1, lsl #16
    tbnz w9, #16, Lci_next
    ldrsw x9, [x24, #4]
    add x24, x24, x9, lsl #5
    b Lci_dispatch
Lci_NE_JUMP64:
    bti j
    ldr w9, [x24, #12]
    ldr x1, [x23, x9]
    ldr w9, [x24, #16]
    ldr x2, [x23, x9]
    cmp x1, x2
    cset w1, ne
    ldr w9, [x24]
    eor w9, w9, w1, lsl #16
    tbnz w9, #16, Lci_next
    ldrsw x9, [x24, #4]
    add x24, x24, x9, lsl #5
    b Lci_dispatch
Lci_SLT_JUMP64:
    bti j
    ldr w9, [x24, #12]
    ldr x1, [x23, x9]
    ldr w9, [x24, #16]
    ldr x2, [x23, x9]
    cmp x1, x2
    cset w1, lt
    ldr w9, [x24]
    eor w9, w9, w1, lsl #16
    tbnz w9, #16, Lci_next
    ldrsw x9, [x24, #4]
    add x24, x24, x9, lsl #5
    b Lci_dispatch
Lci_ULT_JUMP64:
    bti j
    ldr w9, [x24, #12]
    ldr x1, [x23, x9]
    ldr w9, [x24, #16]
    ldr x2, [x23, x9]
    cmp x1, x2
    cset w1, lo
    ldr w9, [x24]
    eor w9, w9, w1, lsl #16
    tbnz w9, #16, Lci_next
    ldrsw x9, [x24, #4]
    add x24, x24, x9, lsl #5
    b Lci_dispatch
Lci_SGT_JUMP64:
    bti j
    ldr w9, [x24, #12]
    ldr x1, [x23, x9]
    ldr w9, [x24, #16]
    ldr x2, [x23, x9]
    cmp x1, x2
    cset w1, gt
    ldr w9, [x24]
    eor w9, w9, w1, lsl #16
    tbnz w9, #16, Lci_next
    ldrsw x9, [x24, #4]
    add x24, x24, x9, lsl #5
    b Lci_dispatch
Lci_UGT_JUMP64:
    bti j
    ldr w9, [x24, #12]
    ldr x1, [x23, x9]
    ldr w9, [x24, #16]
    ldr x2, [x23, x9]
    cmp x1, x2
    cset w1, hi
    ldr w9, [x24]
    eor w9, w9, w1, lsl #16
    tbnz w9, #16, Lci_next
    ldrsw x9, [x24, #4]
    add x24, x24, x9, lsl #5
    b Lci_dispatch
Lci_SLE_JUMP64:
    bti j
    ldr w9, [x24, #12]
    ldr x1, [x23, x9]
    ldr w9, [x24, #16]
    ldr x2, [x23, x9]
    cmp x1, x2
    cset w1, le
    ldr w9, [x24]
    eor w9, w9, w1, lsl #16
    tbnz w9, #16, Lci_next
    ldrsw x9, [x24, #4]
    add x24, x24, x9, lsl #5
    b Lci_dispatch
Lci_ULE_JUMP64:
    bti j
    ldr w9, [x24, #12]
    ldr x1, [x23, x9]
    ldr w9, [x24, #16]
    ldr x2, [x23, x9]
    cmp x1, x2
    cset w1, ls
    ldr w9, [x24]
    eor w9, w9, w1, lsl #16
    tbnz w9, #16, Lci_next
    ldrsw x9, [x24, #4]
    add x24, x24, x9, lsl #5
    b Lci_dispatch
Lci_SGE_JUMP64:
    bti j
    ldr w9, [x24, #12]
    ldr x1, [x23, x9]
    ldr w9, [x24, #16]
    ldr x2, [x23, x9]
    cmp x1, x2
    cset w1, ge
    ldr w9, [x24]
    eor w9, w9, w1, lsl #16
    tbnz w9, #16, Lci_next
    ldrsw x9, [x24, #4]
    add x24, x24, x9, lsl #5
    b Lci_dispatch
Lci_UGE_JUMP64:
    bti j
    ldr w9, [x24, #12]
    ldr x1, [x23, x9]
    ldr w9, [x24, #16]
    ldr x2, [x23, x9]
    cmp x1, x2
    cset w1, hs
    ldr w9, [x24]
    eor w9, w9, w1, lsl #16
    tbnz w9, #16, Lci_next
    ldrsw x9, [x24, #4]
    add x24, x24, x9, lsl #5
    b Lci_dispatch
Lci_FADD32:
    bti j
    ldr w9, [x24, #12]
    ldr s1, [x23, x9]
    ldr w9, [x24, #16]
    ldr s2, [x23, x9]
    fadd s1, s1, s2
    ldr w9, [x24, #4]
    str s1, [x23, x9]
    CI_NEXT
Lci_FSUB32:
    bti j
    ldr w9, [x24, #12]
    ldr s1, [x23, x9]
    ldr w9, [x24, #16]
    ldr s2, [x23, x9]
    fsub s1, s1, s2
    ldr w9, [x24, #4]
    str s1, [x23, x9]
    CI_NEXT
Lci_FMUL32:
    bti j
    ldr w9, [x24, #12]
    ldr s1, [x23, x9]
    ldr w9, [x24, #16]
    ldr s2, [x23, x9]
    fmul s1, s1, s2
    ldr w9, [x24, #4]
    str s1, [x23, x9]
    CI_NEXT
Lci_FDIV32:
    bti j
    ldr w9, [x24, #12]
    ldr s1, [x23, x9]
    ldr w9, [x24, #16]
    ldr s2, [x23, x9]
    fdiv s1, s1, s2
    ldr w9, [x24, #4]
    str s1, [x23, x9]
    CI_NEXT
Lci_FNEG32:
    bti j
    ldr w9, [x24, #12]
    ldr s1, [x23, x9]
    ldr w9, [x24, #16]
    ldr s2, [x23, x9]
    fneg s1, s1
    ldr w9, [x24, #4]
    str s1, [x23, x9]
    CI_NEXT
Lci_FADD64:
    bti j
    ldr w9, [x24, #12]
    ldr d1, [x23, x9]
    ldr w9, [x24, #16]
    ldr d2, [x23, x9]
    fadd d1, d1, d2
    ldr w9, [x24, #4]
    str d1, [x23, x9]
    CI_NEXT
Lci_FSUB64:
    bti j
    ldr w9, [x24, #12]
    ldr d1, [x23, x9]
    ldr w9, [x24, #16]
    ldr d2, [x23, x9]
    fsub d1, d1, d2
    ldr w9, [x24, #4]
    str d1, [x23, x9]
    CI_NEXT
Lci_FMUL64:
    bti j
    ldr w9, [x24, #12]
    ldr d1, [x23, x9]
    ldr w9, [x24, #16]
    ldr d2, [x23, x9]
    fmul d1, d1, d2
    ldr w9, [x24, #4]
    str d1, [x23, x9]
    CI_NEXT
Lci_FDIV64:
    bti j
    ldr w9, [x24, #12]
    ldr d1, [x23, x9]
    ldr w9, [x24, #16]
    ldr d2, [x23, x9]
    fdiv d1, d1, d2
    ldr w9, [x24, #4]
    str d1, [x23, x9]
    CI_NEXT
Lci_FNEG64:
    bti j
    ldr w9, [x24, #12]
    ldr d1, [x23, x9]
    ldr w9, [x24, #16]
    ldr d2, [x23, x9]
    fneg d1, d1
    ldr w9, [x24, #4]
    str d1, [x23, x9]
    CI_NEXT
Lci_FNEG_BF16:
Lci_FNEG16:
    bti j
    ldr w9, [x24, #12]
    ldrh w1, [x23, x9]
    eor w1, w1, #0x8000
    ldr w9, [x24, #4]
    strh w1, [x23, x9]
    CI_NEXT
Lci_FNEG80:
    bti j
    ldr w9, [x24, #12]
    add x9, x23, x9
    ldr x1, [x9]
    ldrh w3, [x9, #8]
    eor w3, w3, #0x8000
    ldr w9, [x24, #4]
    add x0, x23, x9
    str x1, [x0]
    strh w3, [x0, #8]
    mov w3, #10
    b Lci_fneg_padding
Lci_FNEG128:
    bti j
    ldr w9, [x24, #12]
    add x9, x23, x9
    ldp x1, x3, [x9]
    eor x3, x3, #0x8000000000000000
    ldr w9, [x24, #4]
    add x0, x23, x9
    stp x1, x3, [x0]
    mov w3, #16
Lci_fneg_padding:
    ldr w2, [x24, #8]
    cmp w3, w2
    b.hs Lci_next
    strb wzr, [x0, w3, uxtw]
    add w3, w3, #1
    b Lci_fneg_padding
CI_LABEL FADD16
    ldr w9, [x24, #12]
    ldrh w0, [x23, x9]
    ldr w10, [x24, #16]
    ldrh w1, [x23, x10]
    mov w2, #0
    CI_HELPER CI_ASM_HELPER_float16_add
    ldr w3, [x24, #4]
    strh w0, [x23, x3]
    CI_NEXT
CI_LABEL FSUB16
    ldr w9, [x24, #12]
    ldrh w0, [x23, x9]
    ldr w10, [x24, #16]
    ldrh w1, [x23, x10]
    mov w2, #0
    CI_HELPER CI_ASM_HELPER_float16_sub
    ldr w3, [x24, #4]
    strh w0, [x23, x3]
    CI_NEXT
CI_LABEL FMUL16
    ldr w9, [x24, #12]
    ldrh w0, [x23, x9]
    ldr w10, [x24, #16]
    ldrh w1, [x23, x10]
    mov w2, #0
    CI_HELPER CI_ASM_HELPER_float16_mul
    ldr w3, [x24, #4]
    strh w0, [x23, x3]
    CI_NEXT
CI_LABEL FDIV16
    ldr w9, [x24, #12]
    ldrh w0, [x23, x9]
    ldr w10, [x24, #16]
    ldrh w1, [x23, x10]
    mov w2, #0
    CI_HELPER CI_ASM_HELPER_float16_div
    ldr w3, [x24, #4]
    strh w0, [x23, x3]
    CI_NEXT
CI_LABEL FEQ16
    ldr w9, [x24, #12]
    ldrh w0, [x23, x9]
    ldr w10, [x24, #16]
    ldrh w1, [x23, x10]
    mov w2, #0
    CI_HELPER CI_ASM_HELPER_float16_eq
    ldr w3, [x24, #4]
    strb w0, [x23, x3]
    CI_NEXT
CI_LABEL FNE16
    ldr w9, [x24, #12]
    ldrh w0, [x23, x9]
    ldr w10, [x24, #16]
    ldrh w1, [x23, x10]
    mov w2, #0
    CI_HELPER CI_ASM_HELPER_float16_eq
    eor w0, w0, #1
    ldr w3, [x24, #4]
    strb w0, [x23, x3]
    CI_NEXT
CI_LABEL FLT16
    ldr w9, [x24, #12]
    ldrh w0, [x23, x9]
    ldr w10, [x24, #16]
    ldrh w1, [x23, x10]
    mov w2, #0
    CI_HELPER CI_ASM_HELPER_float16_lt
    ldr w3, [x24, #4]
    strb w0, [x23, x3]
    CI_NEXT
CI_LABEL FGT16
    ldr w9, [x24, #16]
    ldrh w0, [x23, x9]
    ldr w10, [x24, #12]
    ldrh w1, [x23, x10]
    mov w2, #0
    CI_HELPER CI_ASM_HELPER_float16_lt
    ldr w3, [x24, #4]
    strb w0, [x23, x3]
    CI_NEXT
CI_LABEL FLE16
    ldr w9, [x24, #12]
    ldrh w0, [x23, x9]
    ldr w10, [x24, #16]
    ldrh w1, [x23, x10]
    mov w2, #0
    CI_HELPER CI_ASM_HELPER_float16_le
    ldr w3, [x24, #4]
    strb w0, [x23, x3]
    CI_NEXT
CI_LABEL FGE16
    ldr w9, [x24, #16]
    ldrh w0, [x23, x9]
    ldr w10, [x24, #12]
    ldrh w1, [x23, x10]
    mov w2, #0
    CI_HELPER CI_ASM_HELPER_float16_le
    ldr w3, [x24, #4]
    strb w0, [x23, x3]
    CI_NEXT
CI_LABEL FADD80
    CI_SLOT x9, 12
    ldr x0, [x9]
    ldrh w1, [x9, #8]
    CI_SLOT x10, 16
    ldr x2, [x10]
    ldrh w3, [x10, #8]
    CI_HELPER CI_ASM_HELPER_float80_add
    CI_SLOT x3, 4
    str x0, [x3]
    strh w1, [x3, #8]
    mov w5, #10
    ldr w4, [x24, #8]
    b Lci_soft_padding
CI_LABEL FSUB80
    CI_SLOT x9, 12
    ldr x0, [x9]
    ldrh w1, [x9, #8]
    CI_SLOT x10, 16
    ldr x2, [x10]
    ldrh w3, [x10, #8]
    CI_HELPER CI_ASM_HELPER_float80_sub
    CI_SLOT x3, 4
    str x0, [x3]
    strh w1, [x3, #8]
    mov w5, #10
    ldr w4, [x24, #8]
    b Lci_soft_padding
CI_LABEL FMUL80
    CI_SLOT x9, 12
    ldr x0, [x9]
    ldrh w1, [x9, #8]
    CI_SLOT x10, 16
    ldr x2, [x10]
    ldrh w3, [x10, #8]
    CI_HELPER CI_ASM_HELPER_float80_mul
    CI_SLOT x3, 4
    str x0, [x3]
    strh w1, [x3, #8]
    mov w5, #10
    ldr w4, [x24, #8]
    b Lci_soft_padding
CI_LABEL FDIV80
    CI_SLOT x9, 12
    ldr x0, [x9]
    ldrh w1, [x9, #8]
    CI_SLOT x10, 16
    ldr x2, [x10]
    ldrh w3, [x10, #8]
    CI_HELPER CI_ASM_HELPER_float80_div
    CI_SLOT x3, 4
    str x0, [x3]
    strh w1, [x3, #8]
    mov w5, #10
    ldr w4, [x24, #8]
    b Lci_soft_padding
CI_LABEL FEQ80
    CI_SLOT x9, 12
    ldr x0, [x9]
    ldrh w1, [x9, #8]
    CI_SLOT x10, 16
    ldr x2, [x10]
    ldrh w3, [x10, #8]
    CI_HELPER CI_ASM_HELPER_float80_eq
    ldr w3, [x24, #4]
    strb w0, [x23, x3]
    CI_NEXT
CI_LABEL FNE80
    CI_SLOT x9, 12
    ldr x0, [x9]
    ldrh w1, [x9, #8]
    CI_SLOT x10, 16
    ldr x2, [x10]
    ldrh w3, [x10, #8]
    CI_HELPER CI_ASM_HELPER_float80_eq
    eor w0, w0, #1
    ldr w3, [x24, #4]
    strb w0, [x23, x3]
    CI_NEXT
CI_LABEL FLT80
    CI_SLOT x9, 12
    ldr x0, [x9]
    ldrh w1, [x9, #8]
    CI_SLOT x10, 16
    ldr x2, [x10]
    ldrh w3, [x10, #8]
    CI_HELPER CI_ASM_HELPER_float80_lt
    ldr w3, [x24, #4]
    strb w0, [x23, x3]
    CI_NEXT
CI_LABEL FGT80
    CI_SLOT x9, 16
    ldr x0, [x9]
    ldrh w1, [x9, #8]
    CI_SLOT x10, 12
    ldr x2, [x10]
    ldrh w3, [x10, #8]
    CI_HELPER CI_ASM_HELPER_float80_lt
    ldr w3, [x24, #4]
    strb w0, [x23, x3]
    CI_NEXT
CI_LABEL FLE80
    CI_SLOT x9, 12
    ldr x0, [x9]
    ldrh w1, [x9, #8]
    CI_SLOT x10, 16
    ldr x2, [x10]
    ldrh w3, [x10, #8]
    CI_HELPER CI_ASM_HELPER_float80_le
    ldr w3, [x24, #4]
    strb w0, [x23, x3]
    CI_NEXT
CI_LABEL FGE80
    CI_SLOT x9, 16
    ldr x0, [x9]
    ldrh w1, [x9, #8]
    CI_SLOT x10, 12
    ldr x2, [x10]
    ldrh w3, [x10, #8]
    CI_HELPER CI_ASM_HELPER_float80_le
    ldr w3, [x24, #4]
    strb w0, [x23, x3]
    CI_NEXT
CI_LABEL FADD128
    ldr w9, [x24, #12]
    ldr q0, [x23, x9]
    ldr w10, [x24, #16]
    ldr q1, [x23, x10]
    CI_HELPER CI_ASM_HELPER_float128_add
    CI_SLOT x3, 4
    str q0, [x3]
    mov w5, #16
    ldr w4, [x24, #8]
    b Lci_soft_padding
CI_LABEL FSUB128
    ldr w9, [x24, #12]
    ldr q0, [x23, x9]
    ldr w10, [x24, #16]
    ldr q1, [x23, x10]
    CI_HELPER CI_ASM_HELPER_float128_sub
    CI_SLOT x3, 4
    str q0, [x3]
    mov w5, #16
    ldr w4, [x24, #8]
    b Lci_soft_padding
CI_LABEL FMUL128
    ldr w9, [x24, #12]
    ldr q0, [x23, x9]
    ldr w10, [x24, #16]
    ldr q1, [x23, x10]
    CI_HELPER CI_ASM_HELPER_float128_mul
    CI_SLOT x3, 4
    str q0, [x3]
    mov w5, #16
    ldr w4, [x24, #8]
    b Lci_soft_padding
CI_LABEL FDIV128
    ldr w9, [x24, #12]
    ldr q0, [x23, x9]
    ldr w10, [x24, #16]
    ldr q1, [x23, x10]
    CI_HELPER CI_ASM_HELPER_float128_div
    CI_SLOT x3, 4
    str q0, [x3]
    mov w5, #16
    ldr w4, [x24, #8]
    b Lci_soft_padding
CI_LABEL FEQ128
    ldr w9, [x24, #12]
    ldr q0, [x23, x9]
    ldr w10, [x24, #16]
    ldr q1, [x23, x10]
    CI_HELPER CI_ASM_HELPER_float128_eq
    ldr w3, [x24, #4]
    strb w0, [x23, x3]
    CI_NEXT
CI_LABEL FNE128
    ldr w9, [x24, #12]
    ldr q0, [x23, x9]
    ldr w10, [x24, #16]
    ldr q1, [x23, x10]
    CI_HELPER CI_ASM_HELPER_float128_eq
    eor w0, w0, #1
    ldr w3, [x24, #4]
    strb w0, [x23, x3]
    CI_NEXT
CI_LABEL FLT128
    ldr w9, [x24, #12]
    ldr q0, [x23, x9]
    ldr w10, [x24, #16]
    ldr q1, [x23, x10]
    CI_HELPER CI_ASM_HELPER_float128_lt
    ldr w3, [x24, #4]
    strb w0, [x23, x3]
    CI_NEXT
CI_LABEL FGT128
    ldr w9, [x24, #16]
    ldr q0, [x23, x9]
    ldr w10, [x24, #12]
    ldr q1, [x23, x10]
    CI_HELPER CI_ASM_HELPER_float128_lt
    ldr w3, [x24, #4]
    strb w0, [x23, x3]
    CI_NEXT
CI_LABEL FLE128
    ldr w9, [x24, #12]
    ldr q0, [x23, x9]
    ldr w10, [x24, #16]
    ldr q1, [x23, x10]
    CI_HELPER CI_ASM_HELPER_float128_le
    ldr w3, [x24, #4]
    strb w0, [x23, x3]
    CI_NEXT
CI_LABEL FGE128
    ldr w9, [x24, #16]
    ldr q0, [x23, x9]
    ldr w10, [x24, #12]
    ldr q1, [x23, x10]
    CI_HELPER CI_ASM_HELPER_float128_le
    ldr w3, [x24, #4]
    strb w0, [x23, x3]
    CI_NEXT
CI_LABEL UDIV128
    CI_SLOT x9, 8
    ldp x0, x1, [x9]
    CI_SLOT x10, 12
    ldp x2, x3, [x10]
    CI_HELPER CI_ASM_HELPER_uint128_div
    CI_SLOT x3, 4
    stp x0, x1, [x3]
    CI_NEXT
CI_LABEL UMOD128
    CI_SLOT x9, 8
    ldp x0, x1, [x9]
    CI_SLOT x10, 12
    ldp x2, x3, [x10]
    CI_HELPER CI_ASM_HELPER_uint128_mod
    CI_SLOT x3, 4
    stp x0, x1, [x3]
    CI_NEXT
CI_LABEL SDIV128
    CI_SLOT x9, 8
    ldp x0, x1, [x9]
    CI_SLOT x10, 12
    ldp x2, x3, [x10]
    CI_HELPER CI_ASM_HELPER_int128_div
    CI_SLOT x3, 4
    stp x0, x1, [x3]
    CI_NEXT
CI_LABEL SMOD128
    CI_SLOT x9, 8
    ldp x0, x1, [x9]
    CI_SLOT x10, 12
    ldp x2, x3, [x10]
    CI_HELPER CI_ASM_HELPER_int128_mod
    CI_SLOT x3, 4
    stp x0, x1, [x3]
    CI_NEXT
Lci_FEQ32:
    bti j
    ldr w9, [x24, #12]
    ldr s1, [x23, x9]
    ldr w9, [x24, #16]
    ldr s2, [x23, x9]
    fcmp s1, s2
    cset w1, eq
    ldr w9, [x24, #4]
    strb w1, [x23, x9]
    CI_NEXT
Lci_FNE32:
    bti j
    ldr w9, [x24, #12]
    ldr s1, [x23, x9]
    ldr w9, [x24, #16]
    ldr s2, [x23, x9]
    fcmp s1, s2
    cset w1, ne
    ldr w9, [x24, #4]
    strb w1, [x23, x9]
    CI_NEXT
Lci_FLT32:
    bti j
    ldr w9, [x24, #12]
    ldr s1, [x23, x9]
    ldr w9, [x24, #16]
    ldr s2, [x23, x9]
    fcmp s1, s2
    cset w1, mi
    ldr w9, [x24, #4]
    strb w1, [x23, x9]
    CI_NEXT
Lci_FGT32:
    bti j
    ldr w9, [x24, #12]
    ldr s1, [x23, x9]
    ldr w9, [x24, #16]
    ldr s2, [x23, x9]
    fcmp s1, s2
    cset w1, gt
    ldr w9, [x24, #4]
    strb w1, [x23, x9]
    CI_NEXT
Lci_FLE32:
    bti j
    ldr w9, [x24, #12]
    ldr s1, [x23, x9]
    ldr w9, [x24, #16]
    ldr s2, [x23, x9]
    fcmp s1, s2
    cset w1, ls
    ldr w9, [x24, #4]
    strb w1, [x23, x9]
    CI_NEXT
Lci_FGE32:
    bti j
    ldr w9, [x24, #12]
    ldr s1, [x23, x9]
    ldr w9, [x24, #16]
    ldr s2, [x23, x9]
    fcmp s1, s2
    cset w1, ge
    ldr w9, [x24, #4]
    strb w1, [x23, x9]
    CI_NEXT
Lci_FEQ64:
    bti j
    ldr w9, [x24, #12]
    ldr d1, [x23, x9]
    ldr w9, [x24, #16]
    ldr d2, [x23, x9]
    fcmp d1, d2
    cset w1, eq
    ldr w9, [x24, #4]
    strb w1, [x23, x9]
    CI_NEXT
Lci_FNE64:
    bti j
    ldr w9, [x24, #12]
    ldr d1, [x23, x9]
    ldr w9, [x24, #16]
    ldr d2, [x23, x9]
    fcmp d1, d2
    cset w1, ne
    ldr w9, [x24, #4]
    strb w1, [x23, x9]
    CI_NEXT
Lci_FLT64:
    bti j
    ldr w9, [x24, #12]
    ldr d1, [x23, x9]
    ldr w9, [x24, #16]
    ldr d2, [x23, x9]
    fcmp d1, d2
    cset w1, mi
    ldr w9, [x24, #4]
    strb w1, [x23, x9]
    CI_NEXT
Lci_FGT64:
    bti j
    ldr w9, [x24, #12]
    ldr d1, [x23, x9]
    ldr w9, [x24, #16]
    ldr d2, [x23, x9]
    fcmp d1, d2
    cset w1, gt
    ldr w9, [x24, #4]
    strb w1, [x23, x9]
    CI_NEXT
Lci_FLE64:
    bti j
    ldr w9, [x24, #12]
    ldr d1, [x23, x9]
    ldr w9, [x24, #16]
    ldr d2, [x23, x9]
    fcmp d1, d2
    cset w1, ls
    ldr w9, [x24, #4]
    strb w1, [x23, x9]
    CI_NEXT
Lci_FGE64:
    bti j
    ldr w9, [x24, #12]
    ldr d1, [x23, x9]
    ldr w9, [x24, #16]
    ldr d2, [x23, x9]
    fcmp d1, d2
    cset w1, ge
    ldr w9, [x24, #4]
    strb w1, [x23, x9]
    CI_NEXT
Lci_MUL128:
    bti j
    ldr w9, [x24, #4]
    add x0, x23, x9
    ldr w9, [x24, #8]
    add x9, x23, x9
    ldp x1, x2, [x9]
    ldr w9, [x24, #12]
    add x9, x23, x9
    ldp x3, x4, [x9]
    umulh x5, x1, x3
    madd x5, x1, x4, x5
    madd x2, x2, x3, x5
    mul x1, x1, x3
    stp x1, x2, [x0]
    CI_NEXT
Lci_LSR128:
    bti j
    ldr w9, [x24, #4]
    add x0, x23, x9
    ldr w9, [x24, #8]
    add x9, x23, x9
    ldp x1, x2, [x9]
    ldr w9, [x24, #12]
    add x9, x23, x9
    ldr x3, [x9]
    cbz x3, Lci_shift_done_LSR
    cmp x3, #64
    b.hs Lci_shift_upper_LSR
    neg x4, x3
    lsl x4, x2, x4
    lsr x1, x1, x3
    orr x1, x1, x4
    lsr x2, x2, x3
    b Lci_shift_done_LSR
Lci_shift_upper_LSR:
    lsr x1, x2, x3
    mov x2, #0
Lci_shift_done_LSR:
    stp x1, x2, [x0]
    CI_NEXT
Lci_ADD128:
    bti j
    ldr w9, [x24, #4]
    add x0, x23, x9
    ldr w9, [x24, #8]
    add x9, x23, x9
    ldp x1, x2, [x9]
    ldr w9, [x24, #12]
    add x9, x23, x9
    ldp x3, x4, [x9]
    adds x1, x1, x3
    adc x2, x2, x4
    stp x1, x2, [x0]
    CI_NEXT
Lci_SUB128:
    bti j
    ldr w9, [x24, #4]
    add x0, x23, x9
    ldr w9, [x24, #8]
    add x9, x23, x9
    ldp x1, x2, [x9]
    ldr w9, [x24, #12]
    add x9, x23, x9
    ldp x3, x4, [x9]
    subs x1, x1, x3
    sbc x2, x2, x4
    stp x1, x2, [x0]
    CI_NEXT
Lci_AND128:
    bti j
    ldr w9, [x24, #4]
    add x0, x23, x9
    ldr w9, [x24, #8]
    add x9, x23, x9
    ldp x1, x2, [x9]
    ldr w9, [x24, #12]
    add x9, x23, x9
    ldp x3, x4, [x9]
    and x1, x1, x3
    and x2, x2, x4
    stp x1, x2, [x0]
    CI_NEXT
Lci_OR128:
    bti j
    ldr w9, [x24, #4]
    add x0, x23, x9
    ldr w9, [x24, #8]
    add x9, x23, x9
    ldp x1, x2, [x9]
    ldr w9, [x24, #12]
    add x9, x23, x9
    ldp x3, x4, [x9]
    orr x1, x1, x3
    orr x2, x2, x4
    stp x1, x2, [x0]
    CI_NEXT
Lci_XOR128:
    bti j
    ldr w9, [x24, #4]
    add x0, x23, x9
    ldr w9, [x24, #8]
    add x9, x23, x9
    ldp x1, x2, [x9]
    ldr w9, [x24, #12]
    add x9, x23, x9
    ldp x3, x4, [x9]
    eor x1, x1, x3
    eor x2, x2, x4
    stp x1, x2, [x0]
    CI_NEXT
Lci_NEG128:
    bti j
    ldr w9, [x24, #4]
    add x0, x23, x9
    ldr w9, [x24, #8]
    add x9, x23, x9
    ldp x1, x2, [x9]
    ldr w9, [x24, #12]
    add x9, x23, x9
    ldp x3, x4, [x9]
    negs x1, x1
    ngc x2, x2
    stp x1, x2, [x0]
    CI_NEXT
Lci_NOT128:
    bti j
    ldr w9, [x24, #4]
    add x0, x23, x9
    ldr w9, [x24, #8]
    add x9, x23, x9
    ldp x1, x2, [x9]
    ldr w9, [x24, #12]
    add x9, x23, x9
    ldp x3, x4, [x9]
    mvn x1, x1
    mvn x2, x2
    stp x1, x2, [x0]
    CI_NEXT
Lci_SHL128:
    bti j
    ldr w9, [x24, #4]
    add x0, x23, x9
    ldr w9, [x24, #8]
    add x9, x23, x9
    ldp x1, x2, [x9]
    ldr w9, [x24, #12]
    add x9, x23, x9
    ldp x3, x4, [x9]
    cbnz x4, Lci_shl128_zero
    cmp x3, #128
    b.hs Lci_shl128_zero
    cbz x3, Lci_shl128_done
    cmp x3, #64
    b.hs Lci_shl128_upper
    neg x4, x3
    lsr x4, x1, x4
    lsl x2, x2, x3
    orr x2, x2, x4
    lsl x1, x1, x3
    b Lci_shl128_done
Lci_shl128_upper:
    lsl x2, x1, x3
    mov x1, #0
    b Lci_shl128_done
Lci_shl128_zero:
    mov x1, #0
    mov x2, #0
Lci_shl128_done:
    stp x1, x2, [x0]
    CI_NEXT
Lci_EQ128:
    bti j
    ldr w9, [x24, #12]
    add x9, x23, x9
    ldp x1, x2, [x9]
    ldr w9, [x24, #16]
    add x9, x23, x9
    ldp x3, x4, [x9]
    cmp x1, x3
    cset w5, eq
    cmp x2, x4
    cset w1, eq
    csel w1, w5, w1, eq
    ldr w9, [x24, #4]
    strb w1, [x23, x9]
    CI_NEXT
Lci_NE128:
    bti j
    ldr w9, [x24, #12]
    add x9, x23, x9
    ldp x1, x2, [x9]
    ldr w9, [x24, #16]
    add x9, x23, x9
    ldp x3, x4, [x9]
    cmp x1, x3
    cset w5, ne
    cmp x2, x4
    cset w1, ne
    csel w1, w5, w1, eq
    ldr w9, [x24, #4]
    strb w1, [x23, x9]
    CI_NEXT
Lci_SLT128:
    bti j
    ldr w9, [x24, #12]
    add x9, x23, x9
    ldp x1, x2, [x9]
    ldr w9, [x24, #16]
    add x9, x23, x9
    ldp x3, x4, [x9]
    cmp x1, x3
    cset w5, lo
    cmp x2, x4
    cset w1, lt
    csel w1, w5, w1, eq
    ldr w9, [x24, #4]
    strb w1, [x23, x9]
    CI_NEXT
Lci_ULT128:
    bti j
    ldr w9, [x24, #12]
    add x9, x23, x9
    ldp x1, x2, [x9]
    ldr w9, [x24, #16]
    add x9, x23, x9
    ldp x3, x4, [x9]
    cmp x1, x3
    cset w5, lo
    cmp x2, x4
    cset w1, lo
    csel w1, w5, w1, eq
    ldr w9, [x24, #4]
    strb w1, [x23, x9]
    CI_NEXT
Lci_SGT128:
    bti j
    ldr w9, [x24, #12]
    add x9, x23, x9
    ldp x1, x2, [x9]
    ldr w9, [x24, #16]
    add x9, x23, x9
    ldp x3, x4, [x9]
    cmp x1, x3
    cset w5, hi
    cmp x2, x4
    cset w1, gt
    csel w1, w5, w1, eq
    ldr w9, [x24, #4]
    strb w1, [x23, x9]
    CI_NEXT
Lci_UGT128:
    bti j
    ldr w9, [x24, #12]
    add x9, x23, x9
    ldp x1, x2, [x9]
    ldr w9, [x24, #16]
    add x9, x23, x9
    ldp x3, x4, [x9]
    cmp x1, x3
    cset w5, hi
    cmp x2, x4
    cset w1, hi
    csel w1, w5, w1, eq
    ldr w9, [x24, #4]
    strb w1, [x23, x9]
    CI_NEXT
Lci_SLE128:
    bti j
    ldr w9, [x24, #12]
    add x9, x23, x9
    ldp x1, x2, [x9]
    ldr w9, [x24, #16]
    add x9, x23, x9
    ldp x3, x4, [x9]
    cmp x1, x3
    cset w5, ls
    cmp x2, x4
    cset w1, lt
    csel w1, w5, w1, eq
    ldr w9, [x24, #4]
    strb w1, [x23, x9]
    CI_NEXT
Lci_ULE128:
    bti j
    ldr w9, [x24, #12]
    add x9, x23, x9
    ldp x1, x2, [x9]
    ldr w9, [x24, #16]
    add x9, x23, x9
    ldp x3, x4, [x9]
    cmp x1, x3
    cset w5, ls
    cmp x2, x4
    cset w1, lo
    csel w1, w5, w1, eq
    ldr w9, [x24, #4]
    strb w1, [x23, x9]
    CI_NEXT
Lci_SGE128:
    bti j
    ldr w9, [x24, #12]
    add x9, x23, x9
    ldp x1, x2, [x9]
    ldr w9, [x24, #16]
    add x9, x23, x9
    ldp x3, x4, [x9]
    cmp x1, x3
    cset w5, hs
    cmp x2, x4
    cset w1, gt
    csel w1, w5, w1, eq
    ldr w9, [x24, #4]
    strb w1, [x23, x9]
    CI_NEXT
Lci_UGE128:
    bti j
    ldr w9, [x24, #12]
    add x9, x23, x9
    ldp x1, x2, [x9]
    ldr w9, [x24, #16]
    add x9, x23, x9
    ldp x3, x4, [x9]
    cmp x1, x3
    cset w5, hs
    cmp x2, x4
    cset w1, hi
    csel w1, w5, w1, eq
    ldr w9, [x24, #4]
    strb w1, [x23, x9]
    CI_NEXT
Lci_ASR128:
    bti j
    ldr w9, [x24, #4]
    add x0, x23, x9
    ldr w9, [x24, #8]
    add x9, x23, x9
    ldp x1, x2, [x9]
    ldr w9, [x24, #12]
    add x9, x23, x9
    ldr x3, [x9]
    cbz x3, Lci_shift_done_ASR
    cmp x3, #64
    b.hs Lci_shift_upper_ASR
    neg x4, x3
    lsl x4, x2, x4
    lsr x1, x1, x3
    orr x1, x1, x4
    asr x2, x2, x3
    b Lci_shift_done_ASR
Lci_shift_upper_ASR:
    asr x1, x2, x3
    asr x2, x2, #63
Lci_shift_done_ASR:
    stp x1, x2, [x0]
    CI_NEXT
CI_LABEL FADD_BF16
    ldr w9, [x24, #12]
    ldrh w0, [x23, x9]
    ldr w10, [x24, #16]
    ldrh w1, [x23, x10]
    mov w2, #1
    CI_HELPER CI_ASM_HELPER_float16_add
    ldr w3, [x24, #4]
    strh w0, [x23, x3]
    CI_NEXT
CI_LABEL FSUB_BF16
    ldr w9, [x24, #12]
    ldrh w0, [x23, x9]
    ldr w10, [x24, #16]
    ldrh w1, [x23, x10]
    mov w2, #1
    CI_HELPER CI_ASM_HELPER_float16_sub
    ldr w3, [x24, #4]
    strh w0, [x23, x3]
    CI_NEXT
CI_LABEL FMUL_BF16
    ldr w9, [x24, #12]
    ldrh w0, [x23, x9]
    ldr w10, [x24, #16]
    ldrh w1, [x23, x10]
    mov w2, #1
    CI_HELPER CI_ASM_HELPER_float16_mul
    ldr w3, [x24, #4]
    strh w0, [x23, x3]
    CI_NEXT
CI_LABEL FDIV_BF16
    ldr w9, [x24, #12]
    ldrh w0, [x23, x9]
    ldr w10, [x24, #16]
    ldrh w1, [x23, x10]
    mov w2, #1
    CI_HELPER CI_ASM_HELPER_float16_div
    ldr w3, [x24, #4]
    strh w0, [x23, x3]
    CI_NEXT
CI_LABEL FEQ_BF16
    ldr w9, [x24, #12]
    ldrh w0, [x23, x9]
    ldr w10, [x24, #16]
    ldrh w1, [x23, x10]
    mov w2, #1
    CI_HELPER CI_ASM_HELPER_float16_eq
    ldr w3, [x24, #4]
    strb w0, [x23, x3]
    CI_NEXT
CI_LABEL FNE_BF16
    ldr w9, [x24, #12]
    ldrh w0, [x23, x9]
    ldr w10, [x24, #16]
    ldrh w1, [x23, x10]
    mov w2, #1
    CI_HELPER CI_ASM_HELPER_float16_eq
    eor w0, w0, #1
    ldr w3, [x24, #4]
    strb w0, [x23, x3]
    CI_NEXT
CI_LABEL FLT_BF16
    ldr w9, [x24, #12]
    ldrh w0, [x23, x9]
    ldr w10, [x24, #16]
    ldrh w1, [x23, x10]
    mov w2, #1
    CI_HELPER CI_ASM_HELPER_float16_lt
    ldr w3, [x24, #4]
    strb w0, [x23, x3]
    CI_NEXT
CI_LABEL FGT_BF16
    ldr w9, [x24, #16]
    ldrh w0, [x23, x9]
    ldr w10, [x24, #12]
    ldrh w1, [x23, x10]
    mov w2, #1
    CI_HELPER CI_ASM_HELPER_float16_lt
    ldr w3, [x24, #4]
    strb w0, [x23, x3]
    CI_NEXT
CI_LABEL FLE_BF16
    ldr w9, [x24, #12]
    ldrh w0, [x23, x9]
    ldr w10, [x24, #16]
    ldrh w1, [x23, x10]
    mov w2, #1
    CI_HELPER CI_ASM_HELPER_float16_le
    ldr w3, [x24, #4]
    strb w0, [x23, x3]
    CI_NEXT
CI_LABEL FGE_BF16
    CI_SLOT x9, 16
    ldrh w0, [x9]
    CI_SLOT x10, 12
    ldrh w1, [x10]
    mov w2, #1
    CI_HELPER CI_ASM_HELPER_float16_le
    ldr w3, [x24, #4]
    strb w0, [x23, x3]
    CI_NEXT

.macro CI_STACK_RESTORE frame
    .if \frame
        ldr x9, [x28, #CI_ASM_stack_boundary]
    .else
        ldr x9, [x28, #CI_ASM_stack_mark_count]
    .endif
    sub x9, x9, #1
    ldr x10, [x28, #CI_ASM_stack_marks]
    add x11, x9, x9, lsl #1
    add x10, x10, x11, lsl #3
    ldp x11, x12, [x10]
    ldr x13, [x10, #16]
    stp x11, x12, [x28, #CI_ASM_stack_top]
    str x9, [x28, #CI_ASM_stack_mark_count]
    str x13, [x28, #CI_ASM_stack_boundary]
.endm

.macro CI_STACK_SAVE name, boundary
    CI_LABEL \name
    .if \boundary
        cbz x28, Lci_enter_after_save
    .endif
    ldp x9, x10, [x28, #CI_ASM_stack_mark_count]
    cmp x9, x10
    b.hs Lci_save_grow_\name
    ldr x11, [x28, #CI_ASM_stack_marks]
    add x12, x9, x9, lsl #1
    add x11, x11, x12, lsl #3
    ldp x12, x13, [x28, #CI_ASM_stack_top]
    stp x12, x13, [x11]
    ldr x14, [x28, #CI_ASM_stack_boundary]
    str x14, [x11, #16]
    add x9, x9, #1
    str x9, [x28, #CI_ASM_stack_mark_count]
    .if \boundary
        str x9, [x28, #CI_ASM_stack_boundary]
    .endif
    .if \boundary
        b Lci_enter_after_save
    .else
        CI_NEXT
    .endif
Lci_save_grow_\name:
    .if \boundary
        sub sp, sp, #208
        stp x0, x1, [sp]
        stp x2, x3, [sp, #16]
        stp x4, x5, [sp, #32]
        stp x6, x7, [sp, #48]
        stp x8, x20, [sp, #64]
        stp q0, q1, [sp, #80]
        stp q2, q3, [sp, #112]
        stp q4, q5, [sp, #144]
        stp q6, q7, [sp, #176]
    .endif
    mov x0, x28
    mov w1, #\boundary
    CI_HELPER CI_ASM_HELPER_stack_save
    .if \boundary
        mov w17, w0
        ldp x0, x1, [sp]
        ldp x2, x3, [sp, #16]
        ldp x4, x5, [sp, #32]
        ldp x6, x7, [sp, #48]
        ldp x8, x20, [sp, #64]
        ldp q0, q1, [sp, #80]
        ldp q2, q3, [sp, #112]
        ldp q4, q5, [sp, #144]
        ldp q6, q7, [sp, #176]
        add sp, sp, #208
        cbnz w17, Lci_entry_failed
    .else
        cbnz w0, Lci_error
    .endif
    .if \boundary
        b Lci_enter_after_save
    .else
        CI_NEXT
    .endif
.endm

Lci_error:
    CI_PUBLISH_PC
    ldr x17, [x19, #8]
Lci_unwind:
    cmp x15, x17
    b.eq Lci_root_error
    ldr x15, [x15, #CI_ASM_parent]
    CI_STACK_RESTORE 1
    b Lci_unwind
Lci_finish:
    CI_FRAME x15
    ldr x9, [x15, #CI_ASM_code]
    ldr x9, [x9, #CI_ASM_code_count]
    str x9, [x15, #CI_ASM_pc]
    ldr x9, [x19, #8]
    cmp x15, x9
    b.eq Lci_success
    ldr x10, [x15, #CI_ASM_parent]
    add x23, x10, #CI_ASM_frame_size
    ldr x9, [x10, #CI_ASM_code]
    ldr x24, [x10, #CI_ASM_pc]
    add x9, x9, #(CI_ASM_code_ops + 32)
    add x24, x9, x24, lsl #5
    CI_STACK_RESTORE 1
    b Lci_dispatch
Lci_root_error:
    cbz x28, Lci_callback_zero
    ldr x15, [x19, #8]
    ldr x9, [x15, #CI_ASM_code]
    ldrh w9, [x9, #CI_ASM_code_ops]
    cmp w9, #CI_OP_FRAME_SAVE
    b.ne Lci_root_error_result
    CI_STACK_RESTORE 1
Lci_root_error_result:
    ldr x9, [x19, #48]
    tbz x9, #0, Lci_exit
Lci_callback_zero:
    mov x0, #0
    mov x1, #0
    movi v0.16b, #0
    movi v1.16b, #0
    movi v2.16b, #0
    movi v3.16b, #0
    b Lci_exit
Lci_success:
    ldr x15, [x19, #8]
    ldr x9, [x15, #CI_ASM_code]
    ldrh w9, [x9, #CI_ASM_code_ops]
    cmp w9, #CI_OP_FRAME_SAVE
    b.ne Lci_success_result
    CI_STACK_RESTORE 1
Lci_success_result:
    ldr x9, [x19, #48]
    tbz x9, #0, Lci_export_result
    b Lci_exit
Lci_export_result:
    stp x0, x1, [x28, #208]
    stp q0, q1, [x28, #224]
    stp q2, q3, [x28, #256]
    str x8, [x28, #64]
    mov w0, #0
    b Lci_exit

CI_STACK_SAVE FRAME_SAVE, 1
CI_LABEL FRAME_RESTORE
    b Lci_finish

Lci_enter_after_save:
    add x24, x24, #32
CI_LABEL FRAME_ENTER
    sub x9, x24, #(CI_ASM_code_ops + 32)
    cbz x28, Lci_frame_native
    ldp x10, x11, [x28, #CI_ASM_stack_top]
    cbz x10, Lci_frame_grow
    ldr x12, [x10, #8]
    ldr x16, [x9, #16]
    add x13, x10, #16
    add x13, x13, x11
    add x13, x13, #CI_ASM_frame_size
    sub x17, x16, #1
    add x13, x13, x17
    neg x16, x16
    and x13, x13, x16
    ldr x17, [x9, #8]
    add x14, x13, x17
    sub x14, x14, x10
    sub x14, x14, #16
    cmp x14, x12
    b.hi Lci_frame_grow
    str x14, [x28, #(CI_ASM_stack_top + 8)]
    sub x15, x13, #CI_ASM_frame_size
    CI_FRAME x12
    str x12, [x15, #CI_ASM_parent]
    b Lci_frame_init
Lci_frame_native:
    ldr x17, [x9, #8]
    ldr x16, [x9, #16]
    mov x13, sp
    sub x13, x13, x17
    neg x16, x16
    and x13, x13, x16
    sub x15, x13, #CI_ASM_frame_size
Lci_frame_probe:
    sub x12, sp, x15
    cmp x12, #4096
    b.lo Lci_frame_native_ready
    sub sp, sp, #4096
    str xzr, [sp]
    b Lci_frame_probe
Lci_frame_native_ready:
    mov sp, x15
    mov x29, sp
    str xzr, [x15, #CI_ASM_parent]
    str x15, [x19, #8]
Lci_frame_init:
    str x9, [x15, #CI_ASM_code]
    mov x12, #2
    str x12, [x15, #CI_ASM_pc]
    mov x23, x13
    ldr w12, [x24, #4]
    cmn w12, #1
    add x12, x23, x12
    csel x12, xzr, x12, eq
    str x12, [x15, #CI_ASM_incoming]
    ldr x12, [x19, #48]
    tbz x12, #1, Lci_next
    mov w12, #0xcc
Lci_frame_poison:
    cbz x17, Lci_next
    strb w12, [x13], #1
    sub x17, x17, #1
    b Lci_frame_poison

Lci_frame_grow:
    sub sp, sp, #208
    stp x0, x1, [sp]
    stp x2, x3, [sp, #16]
    stp x4, x5, [sp, #32]
    stp x6, x7, [sp, #48]
    stp x8, x20, [sp, #64]
    stp q0, q1, [sp, #80]
    stp q2, q3, [sp, #112]
    stp q4, q5, [sp, #144]
    stp q6, q7, [sp, #176]
    ldr x0, [x19]
    mov x1, x28
    CI_FRAME x2
    sub x9, x24, #(CI_ASM_code_ops + 32)
    ldr x3, [x9, #8]
    ldr w4, [x9, #16]
    add x5, x19, #24
    CI_HELPER CI_ASM_HELPER_new_frame
    mov w17, w0
    ldp x0, x1, [sp]
    ldp x2, x3, [sp, #16]
    ldp x4, x5, [sp, #32]
    ldp x6, x7, [sp, #48]
    ldp x8, x20, [sp, #64]
    ldp q0, q1, [sp, #80]
    ldp q2, q3, [sp, #112]
    ldp q4, q5, [sp, #144]
    ldp q6, q7, [sp, #176]
    add sp, sp, #208
    cbnz w17, Lci_frame_failed
    ldr x15, [x19, #24]
    sub x9, x24, #(CI_ASM_code_ops + 32)
    str x9, [x15, #CI_ASM_code]
    mov x12, #2
    str x12, [x15, #CI_ASM_pc]
    add x23, x15, #CI_ASM_frame_size
    ldr w12, [x24, #4]
    cmn w12, #1
    add x12, x23, x12
    csel x12, xzr, x12, eq
    str x12, [x15, #CI_ASM_incoming]
    CI_NEXT
Lci_frame_failed:
    CI_STACK_RESTORE 1
Lci_entry_failed:
    CI_FRAME x15
    ldr x9, [x15, #CI_ASM_code]
    ldr x24, [x15, #CI_ASM_pc]
    add x9, x9, #CI_ASM_code_ops
    add x24, x9, x24, lsl #5
    mov w0, w17
    b Lci_error

CI_LABEL FRAME_READY
    cbnz x28, Lci_next
    ldr x0, [x19]
    CI_HELPER CI_ASM_HELPER_callback_stack
    mov x28, x0
    cbnz x28, Lci_next
    mov w0, #1
    b Lci_error

Lci_hotswap_cycle:
    ldr x0, [x19]
    ldr x1, [x10, #48]
    adrp x2, Lci_msg_hotswap
    add x2, x2, :lo12:Lci_msg_hotswap
    CI_HELPER CI_ASM_HELPER_error
    ldr x9, [x19, #8]
    cbz x9, Lci_callback_zero
    b Lci_error
CI_LABEL VAR_ADDR
    ldr x9, [x24, #16]
    ldr x1, [x9, #CI_ASM_var_value]
    cbz x1, Lci_VAR_ADDR_support
    ldr w9, [x24, #8]
    str x1, [x23, x9]
    CI_NEXT
CI_LABEL VAR_ADDR_support
    ldr x0, [x19]
    ldr x1, [x24, #16]
    CI_HELPER CI_ASM_HELPER_ensure_var
    cbnz w0, Lci_error
    ldr x9, [x24, #16]
    ldr x1, [x9, #CI_ASM_var_value]
    ldr w9, [x24, #8]
    str x1, [x23, x9]
    CI_NEXT
CI_LABEL TLS_ADDR
    ldr x0, [x19]
    ldr x1, [x24, #16]
    CI_SLOT x2, 8
    CI_HELPER CI_ASM_HELPER_tls_address
    cbnz w0, Lci_error
    CI_NEXT
CI_LABEL FUNC_ADDR
    ldr x9, [x24, #16]
    ldr x1, [x9, #CI_ASM_native_func]
    cbz x1, Lci_FUNC_ADDR_support
    ldr w9, [x24, #8]
    str x1, [x23, x9]
    CI_NEXT
CI_LABEL FUNC_ADDR_support
    ldr x9, [x24, #16]
    ldr x9, [x9, #32]
    CI_ADDRESS x3, Lci_msg_unknown
    cbz x9, Lci_missing_name
    add x3, x9, #CI_ASM_atom_data
Lci_missing_name:
    CI_ERROR Lci_msg_missing_function
CI_LABEL CAPTURE_BACKTRACE
    CI_PUBLISH_PC
    CI_SLOT x2, 12
    ldr x2, [x2]
    CI_SLOT x3, 16
    ldr w3, [x3]
    mov w4, #0
    cmp w3, #0
    b.le Lci_capture_done
    cbz x2, Lci_capture_null
Lci_capture_loop:
    cbz x15, Lci_capture_done
    ldr x5, [x15, #CI_ASM_code]
    cbz x5, Lci_capture_parent
    add x5, x5, #CI_ASM_code_ops
    ldr x6, [x15, #CI_ASM_pc]
    ldr x7, [x15, #CI_ASM_code]
    ldr x7, [x7, #CI_ASM_code_count]
    cmp x6, x7
    b.hs Lci_capture_parent
    add x5, x5, x6, lsl #5
    ldr x5, [x5, #24]
    str x5, [x2, w4, uxtw #3]
    add w4, w4, #1
    cmp w4, w3
    b.ge Lci_capture_done
Lci_capture_parent:
    ldr x15, [x15, #CI_ASM_parent]
    b Lci_capture_loop
Lci_capture_done:
    ldr w9, [x24, #4]
    str w4, [x23, x9]
    CI_NEXT
Lci_capture_null:
    CI_ERROR Lci_msg_capture_null
CI_LABEL CLEAR_CACHE
    CI_SLOT x0, 8
    ldr x0, [x0]
    CI_LOAD_SLOT 12, 8
    CI_HELPER CI_ASM_HELPER_clear_cache
    CI_NEXT
CI_LABEL CHECK
    CI_SLOT x1, 4
    ldrb w1, [x1]
    cbz w1, Lci_CHECK_support
    CI_NEXT
CI_LABEL CHECK_support
    ldr x3, [x24, #8]
    CI_ERROR Lci_msg_string
CI_LABEL RETURN_SLOT
    CI_FRAME x15
    ldr x9, [x19, #8]
    cmp x15, x9
    b.eq Lci_RETURN_SLOT_root
    ldr x10, [x15, #CI_ASM_parent]
    ldr x9, [x10, #CI_ASM_code]
    ldr x11, [x10, #CI_ASM_pc]
    add x9, x9, #CI_ASM_code_ops
    add x9, x9, x11, lsl #5
    ldr w4, [x9, #8]
    cbz w4, Lci_finish
    ldr w0, [x9, #4]
    add x0, x10, x0
    add x0, x0, #CI_ASM_frame_size
    b Lci_RETURN_SLOT_copy
Lci_RETURN_SLOT_root:
    ldp x0, x4, [x19, #32]
    cbz x0, Lci_finish
Lci_RETURN_SLOT_copy:
    ldr w2, [x24, #12]
    cmp x2, x4
    b.hi Lci_RETURN_SLOT_support
    CI_SLOT x1, 8
    CI_HELPER CI_ASM_HELPER_memmove
    b Lci_finish
CI_LABEL RETURN_SLOT_support
    ldr w3, [x24, #12]
    CI_ERROR Lci_msg_result_size
Lci_SLOT_ADDR:
    bti j
    ldr w9, [x24, #12]
    add x1, x23, x9
    ldr w9, [x24, #4]
    str x1, [x23, x9]
    CI_NEXT
Lci_JUMP:
    bti j
    ldrsw x9, [x24, #4]
    add x24, x24, x9, lsl #5
    b Lci_dispatch
Lci_RETURN:
    bti j
    b Lci_finish
Lci_PROPAGATE_ERROR:
    bti j
    ldr w9, [x24, #4]
    ldr w0, [x23, x9]
    cbnz w0, Lci_error
    CI_NEXT
CI_STACK_SAVE STACK_SAVE, 0
CI_LABEL STACK_RESTORE
    CI_STACK_RESTORE 0
    CI_NEXT

// CI_CONST_OPS
CI_MEMORY CONST8, 0, 1
CI_MEMORY CONST16, 0, 2
CI_MEMORY CONST32, 0, 4
CI_MEMORY CONST64, 0, 8
CI_MEMORY CONST128, 0, 16
CI_MEMORY CONST_N, 0, 0
// CI_LOAD_OPS
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
// CI_STORE_OPS
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
// CI_STORE_IMM_OPS
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
// CI_ZERO_OPS
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
// CI_COPY_OPS
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
// CI_MEMCOPY_OPS
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
// CI_EXTEND_OPS
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
// CI_CHECKED_OPS
CI_CHECKED CHECKED_ADD_S8, 1, 0, 0
CI_CHECKED CHECKED_ADD_U8, 1, 1, 0
CI_CHECKED CHECKED_ADD_S16, 2, 0, 0
CI_CHECKED CHECKED_ADD_U16, 2, 1, 0
CI_CHECKED CHECKED_ADD_S32, 4, 0, 0
CI_CHECKED CHECKED_ADD_U32, 4, 1, 0
CI_CHECKED CHECKED_ADD_S64, 8, 0, 0
CI_CHECKED CHECKED_ADD_U64, 8, 1, 0
CI_CHECKED CHECKED_SUB_S8, 1, 0, 1
CI_CHECKED CHECKED_SUB_U8, 1, 1, 1
CI_CHECKED CHECKED_SUB_S16, 2, 0, 1
CI_CHECKED CHECKED_SUB_U16, 2, 1, 1
CI_CHECKED CHECKED_SUB_S32, 4, 0, 1
CI_CHECKED CHECKED_SUB_U32, 4, 1, 1
CI_CHECKED CHECKED_SUB_S64, 8, 0, 1
CI_CHECKED CHECKED_SUB_U64, 8, 1, 1
CI_CHECKED CHECKED_MUL_S8, 1, 0, 2
CI_CHECKED CHECKED_MUL_U8, 1, 1, 2
CI_CHECKED CHECKED_MUL_S16, 2, 0, 2
CI_CHECKED CHECKED_MUL_U16, 2, 1, 2
CI_CHECKED CHECKED_MUL_S32, 4, 0, 2
CI_CHECKED CHECKED_MUL_U32, 4, 1, 2
CI_CHECKED CHECKED_MUL_S64, 8, 0, 2
CI_CHECKED CHECKED_MUL_U64, 8, 1, 2
// CI_OVERFLOW_OPS
CI_OVERFLOW OVERFLOW_S8, 8, 0
CI_OVERFLOW OVERFLOW_U8, 8, 1
CI_OVERFLOW OVERFLOW_S16, 16, 0
CI_OVERFLOW OVERFLOW_U16, 16, 1
CI_OVERFLOW OVERFLOW_S32, 32, 0
CI_OVERFLOW OVERFLOW_U32, 32, 1
CI_OVERFLOW OVERFLOW_S64, 64, 0
CI_OVERFLOW OVERFLOW_U64, 64, 1
// CI_TRUTH_OPS
CI_TRUTH ISTRUE_I8, 8, 0, 0
CI_TRUTH ISFALSE_I8, 8, 0, 1
CI_TRUTH ISTRUE_I16, 16, 0, 0
CI_TRUTH ISFALSE_I16, 16, 0, 1
CI_TRUTH ISTRUE_I32, 32, 0, 0
CI_TRUTH ISFALSE_I32, 32, 0, 1
CI_TRUTH ISTRUE_I64, 64, 0, 0
CI_TRUTH ISFALSE_I64, 64, 0, 1
CI_TRUTH ISTRUE_I128, 128, 0, 0
CI_TRUTH ISFALSE_I128, 128, 0, 1
CI_TRUTH ISTRUE_F16, 16, 1, 0
CI_TRUTH ISFALSE_F16, 16, 1, 1
CI_TRUTH ISTRUE_F32, 32, 1, 0
CI_TRUTH ISFALSE_F32, 32, 1, 1
CI_TRUTH ISTRUE_F64, 64, 1, 0
CI_TRUTH ISFALSE_F64, 64, 1, 1
CI_TRUTH ISTRUE_F80, 80, 1, 0
CI_TRUTH ISFALSE_F80, 80, 1, 1
CI_TRUTH ISTRUE_F128, 128, 1, 0
CI_TRUTH ISFALSE_F128, 128, 1, 1
// CI_ITOF_OPS
CI_LABEL S32_TO_F16
    ldr w9, [x24, #16]
    ldr w0, [x23, x9]
    mov w1, #0
    CI_HELPER CI_ASM_HELPER_float16_from_int32
    ldr w3, [x24, #8]
    strh w0, [x23, x3]
    CI_NEXT
CI_LABEL U32_TO_F16
    ldr w9, [x24, #16]
    ldr w0, [x23, x9]
    mov w1, #0
    CI_HELPER CI_ASM_HELPER_float16_from_uint32
    ldr w3, [x24, #8]
    strh w0, [x23, x3]
    CI_NEXT
CI_LABEL S64_TO_F16
    ldr w9, [x24, #16]
    ldr x0, [x23, x9]
    mov w1, #0
    CI_HELPER CI_ASM_HELPER_float16_from_int64
    ldr w3, [x24, #8]
    strh w0, [x23, x3]
    CI_NEXT
CI_LABEL U64_TO_F16
    ldr w9, [x24, #16]
    ldr x0, [x23, x9]
    mov w1, #0
    CI_HELPER CI_ASM_HELPER_float16_from_uint64
    ldr w3, [x24, #8]
    strh w0, [x23, x3]
    CI_NEXT
CI_LABEL S128_TO_F16
    CI_SLOT x9, 16
    ldp x0, x1, [x9]
    mov w2, #0
    mov w3, #0
    CI_HELPER CI_ASM_HELPER_float16_from_uint128
    ldr w3, [x24, #8]
    strh w0, [x23, x3]
    CI_NEXT
CI_LABEL U128_TO_F16
    CI_SLOT x9, 16
    ldp x0, x1, [x9]
    mov w2, #1
    mov w3, #0
    CI_HELPER CI_ASM_HELPER_float16_from_uint128
    ldr w3, [x24, #8]
    strh w0, [x23, x3]
    CI_NEXT
CI_LABEL S32_TO_BF16
    ldr w9, [x24, #16]
    ldr w0, [x23, x9]
    mov w1, #1
    CI_HELPER CI_ASM_HELPER_float16_from_int32
    ldr w3, [x24, #8]
    strh w0, [x23, x3]
    CI_NEXT
CI_LABEL U32_TO_BF16
    ldr w9, [x24, #16]
    ldr w0, [x23, x9]
    mov w1, #1
    CI_HELPER CI_ASM_HELPER_float16_from_uint32
    ldr w3, [x24, #8]
    strh w0, [x23, x3]
    CI_NEXT
CI_LABEL S64_TO_BF16
    ldr w9, [x24, #16]
    ldr x0, [x23, x9]
    mov w1, #1
    CI_HELPER CI_ASM_HELPER_float16_from_int64
    ldr w3, [x24, #8]
    strh w0, [x23, x3]
    CI_NEXT
CI_LABEL U64_TO_BF16
    ldr w9, [x24, #16]
    ldr x0, [x23, x9]
    mov w1, #1
    CI_HELPER CI_ASM_HELPER_float16_from_uint64
    ldr w3, [x24, #8]
    strh w0, [x23, x3]
    CI_NEXT
CI_LABEL S128_TO_BF16
    CI_SLOT x9, 16
    ldp x0, x1, [x9]
    mov w2, #0
    mov w3, #1
    CI_HELPER CI_ASM_HELPER_float16_from_uint128
    ldr w3, [x24, #8]
    strh w0, [x23, x3]
    CI_NEXT
CI_LABEL U128_TO_BF16
    CI_SLOT x9, 16
    ldp x0, x1, [x9]
    mov w2, #1
    mov w3, #1
    CI_HELPER CI_ASM_HELPER_float16_from_uint128
    ldr w3, [x24, #8]
    strh w0, [x23, x3]
    CI_NEXT
CI_ITOF S32_TO_F32, 4, 0, 32
CI_ITOF U32_TO_F32, 4, 1, 32
CI_ITOF S64_TO_F32, 8, 0, 32
CI_ITOF U64_TO_F32, 8, 1, 32
CI_LABEL S128_TO_F32
    CI_SLOT x9, 16
    ldp x0, x1, [x9]
    mov w2, #0
    CI_HELPER CI_ASM_HELPER_uint128_to_float
    ldr w3, [x24, #8]
    str s0, [x23, x3]
    CI_NEXT
CI_LABEL U128_TO_F32
    CI_SLOT x9, 16
    ldp x0, x1, [x9]
    mov w2, #1
    CI_HELPER CI_ASM_HELPER_uint128_to_float
    ldr w3, [x24, #8]
    str s0, [x23, x3]
    CI_NEXT
CI_ITOF S32_TO_F64, 4, 0, 64
CI_ITOF U32_TO_F64, 4, 1, 64
CI_ITOF S64_TO_F64, 8, 0, 64
CI_ITOF U64_TO_F64, 8, 1, 64
CI_LABEL S128_TO_F64
    CI_SLOT x9, 16
    ldp x0, x1, [x9]
    mov w2, #0
    CI_HELPER CI_ASM_HELPER_uint128_to_double
    ldr w3, [x24, #8]
    str d0, [x23, x3]
    CI_NEXT
CI_LABEL U128_TO_F64
    CI_SLOT x9, 16
    ldp x0, x1, [x9]
    mov w2, #1
    CI_HELPER CI_ASM_HELPER_uint128_to_double
    ldr w3, [x24, #8]
    str d0, [x23, x3]
    CI_NEXT
CI_LABEL S32_TO_F80
    ldr w9, [x24, #16]
    ldr w0, [x23, x9]
    sxtw x0, w0
    asr x1, x0, #63
    mov w2, #0
    CI_HELPER CI_ASM_HELPER_float80_from_uint128
    CI_SLOT x3, 8
    str x0, [x3]
    strh w1, [x3, #8]
    mov w5, #10
    ldr w4, [x24, #12]
    b Lci_soft_padding
CI_LABEL U32_TO_F80
    ldr w9, [x24, #16]
    ldr w0, [x23, x9]
    mov w0, w0
    mov x1, #0
    mov w2, #1
    CI_HELPER CI_ASM_HELPER_float80_from_uint128
    CI_SLOT x3, 8
    str x0, [x3]
    strh w1, [x3, #8]
    mov w5, #10
    ldr w4, [x24, #12]
    b Lci_soft_padding
CI_LABEL S64_TO_F80
    ldr w9, [x24, #16]
    ldr x0, [x23, x9]
    asr x1, x0, #63
    mov w2, #0
    CI_HELPER CI_ASM_HELPER_float80_from_uint128
    CI_SLOT x3, 8
    str x0, [x3]
    strh w1, [x3, #8]
    mov w5, #10
    ldr w4, [x24, #12]
    b Lci_soft_padding
CI_LABEL U64_TO_F80
    ldr w9, [x24, #16]
    ldr x0, [x23, x9]
    mov x1, #0
    mov w2, #1
    CI_HELPER CI_ASM_HELPER_float80_from_uint128
    CI_SLOT x3, 8
    str x0, [x3]
    strh w1, [x3, #8]
    mov w5, #10
    ldr w4, [x24, #12]
    b Lci_soft_padding
CI_LABEL S128_TO_F80
    CI_SLOT x9, 16
    ldp x0, x1, [x9]
    mov w2, #0
    CI_HELPER CI_ASM_HELPER_float80_from_uint128
    CI_SLOT x3, 8
    str x0, [x3]
    strh w1, [x3, #8]
    mov w5, #10
    ldr w4, [x24, #12]
    b Lci_soft_padding
CI_LABEL U128_TO_F80
    CI_SLOT x9, 16
    ldp x0, x1, [x9]
    mov w2, #1
    CI_HELPER CI_ASM_HELPER_float80_from_uint128
    CI_SLOT x3, 8
    str x0, [x3]
    strh w1, [x3, #8]
    mov w5, #10
    ldr w4, [x24, #12]
    b Lci_soft_padding
CI_LABEL S32_TO_F128
    ldr w9, [x24, #16]
    ldr w0, [x23, x9]
    sxtw x0, w0
    asr x1, x0, #63
    mov w2, #0
    CI_HELPER CI_ASM_HELPER_float128_from_uint128
    ldr w3, [x24, #8]
    str q0, [x23, x3]
    CI_NEXT
CI_LABEL U32_TO_F128
    ldr w9, [x24, #16]
    ldr w0, [x23, x9]
    mov w0, w0
    mov x1, #0
    mov w2, #1
    CI_HELPER CI_ASM_HELPER_float128_from_uint128
    ldr w3, [x24, #8]
    str q0, [x23, x3]
    CI_NEXT
CI_LABEL S64_TO_F128
    ldr w9, [x24, #16]
    ldr x0, [x23, x9]
    asr x1, x0, #63
    mov w2, #0
    CI_HELPER CI_ASM_HELPER_float128_from_uint128
    ldr w3, [x24, #8]
    str q0, [x23, x3]
    CI_NEXT
CI_LABEL U64_TO_F128
    ldr w9, [x24, #16]
    ldr x0, [x23, x9]
    mov x1, #0
    mov w2, #1
    CI_HELPER CI_ASM_HELPER_float128_from_uint128
    ldr w3, [x24, #8]
    str q0, [x23, x3]
    CI_NEXT
CI_LABEL S128_TO_F128
    CI_SLOT x9, 16
    ldp x0, x1, [x9]
    mov w2, #0
    CI_HELPER CI_ASM_HELPER_float128_from_uint128
    ldr w3, [x24, #8]
    str q0, [x23, x3]
    CI_NEXT
CI_LABEL U128_TO_F128
    CI_SLOT x9, 16
    ldp x0, x1, [x9]
    mov w2, #1
    CI_HELPER CI_ASM_HELPER_float128_from_uint128
    ldr w3, [x24, #8]
    str q0, [x23, x3]
    CI_NEXT
// CI_FTOI_OPS
CI_LABEL F16_TO_S32
    ldr w9, [x24, #16]
    ldrh w0, [x23, x9]
    mov w1, #0
    CI_HELPER CI_ASM_HELPER_float16_to_int32
    ldr w3, [x24, #8]
    str w0, [x23, x3]
    CI_NEXT
CI_LABEL F16_TO_U32
    ldr w9, [x24, #16]
    ldrh w0, [x23, x9]
    mov w1, #0
    CI_HELPER CI_ASM_HELPER_float16_to_uint32
    ldr w3, [x24, #8]
    str w0, [x23, x3]
    CI_NEXT
CI_LABEL F16_TO_S64
    ldr w9, [x24, #16]
    ldrh w0, [x23, x9]
    mov w1, #0
    CI_HELPER CI_ASM_HELPER_float16_to_int64
    ldr w3, [x24, #8]
    str x0, [x23, x3]
    CI_NEXT
CI_LABEL F16_TO_U64
    ldr w9, [x24, #16]
    ldrh w0, [x23, x9]
    mov w1, #0
    CI_HELPER CI_ASM_HELPER_float16_to_uint64
    ldr w3, [x24, #8]
    str x0, [x23, x3]
    CI_NEXT
CI_LABEL F16_TO_I128
    ldr w9, [x24, #16]
    ldrh w0, [x23, x9]
    mov w1, #0
    CI_HELPER CI_ASM_HELPER_float16_to_uint128
    CI_SLOT x3, 8
    stp x0, x1, [x3]
    CI_NEXT
CI_LABEL BF16_TO_S32
    ldr w9, [x24, #16]
    ldrh w0, [x23, x9]
    mov w1, #1
    CI_HELPER CI_ASM_HELPER_float16_to_int32
    ldr w3, [x24, #8]
    str w0, [x23, x3]
    CI_NEXT
CI_LABEL BF16_TO_U32
    ldr w9, [x24, #16]
    ldrh w0, [x23, x9]
    mov w1, #1
    CI_HELPER CI_ASM_HELPER_float16_to_uint32
    ldr w3, [x24, #8]
    str w0, [x23, x3]
    CI_NEXT
CI_LABEL BF16_TO_S64
    ldr w9, [x24, #16]
    ldrh w0, [x23, x9]
    mov w1, #1
    CI_HELPER CI_ASM_HELPER_float16_to_int64
    ldr w3, [x24, #8]
    str x0, [x23, x3]
    CI_NEXT
CI_LABEL BF16_TO_U64
    ldr w9, [x24, #16]
    ldrh w0, [x23, x9]
    mov w1, #1
    CI_HELPER CI_ASM_HELPER_float16_to_uint64
    ldr w3, [x24, #8]
    str x0, [x23, x3]
    CI_NEXT
CI_LABEL BF16_TO_I128
    ldr w9, [x24, #16]
    ldrh w0, [x23, x9]
    mov w1, #1
    CI_HELPER CI_ASM_HELPER_float16_to_uint128
    CI_SLOT x3, 8
    stp x0, x1, [x3]
    CI_NEXT
CI_FTOI F32_TO_S32, 32, 4, 0
CI_FTOI F32_TO_U32, 32, 4, 1
CI_FTOI F32_TO_S64, 32, 8, 0
CI_FTOI F32_TO_U64, 32, 8, 1
CI_LABEL F32_TO_S128
    ldr w9, [x24, #16]
    ldr s0, [x23, x9]
    fcvt d0, s0
    mov w0, #0
    CI_HELPER CI_ASM_HELPER_uint128_from_double
    CI_SLOT x3, 8
    stp x0, x1, [x3]
    CI_NEXT
CI_LABEL F32_TO_U128
    ldr w9, [x24, #16]
    ldr s0, [x23, x9]
    fcvt d0, s0
    mov w0, #1
    CI_HELPER CI_ASM_HELPER_uint128_from_double
    CI_SLOT x3, 8
    stp x0, x1, [x3]
    CI_NEXT
CI_FTOI F64_TO_S32, 64, 4, 0
CI_FTOI F64_TO_U32, 64, 4, 1
CI_FTOI F64_TO_S64, 64, 8, 0
CI_FTOI F64_TO_U64, 64, 8, 1
CI_LABEL F64_TO_S128
    ldr w9, [x24, #16]
    ldr d0, [x23, x9]
    mov w0, #0
    CI_HELPER CI_ASM_HELPER_uint128_from_double
    CI_SLOT x3, 8
    stp x0, x1, [x3]
    CI_NEXT
CI_LABEL F64_TO_U128
    ldr w9, [x24, #16]
    ldr d0, [x23, x9]
    mov w0, #1
    CI_HELPER CI_ASM_HELPER_uint128_from_double
    CI_SLOT x3, 8
    stp x0, x1, [x3]
    CI_NEXT
CI_LABEL F80_TO_I8
    CI_SLOT x9, 16
    ldr x0, [x9]
    ldrh w1, [x9, #8]
    CI_HELPER CI_ASM_HELPER_float80_to_uint128
    ldr w3, [x24, #8]
    strb w0, [x23, x3]
    CI_NEXT
CI_LABEL F80_TO_I16
    CI_SLOT x9, 16
    ldr x0, [x9]
    ldrh w1, [x9, #8]
    CI_HELPER CI_ASM_HELPER_float80_to_uint128
    ldr w3, [x24, #8]
    strh w0, [x23, x3]
    CI_NEXT
CI_LABEL F80_TO_I32
    CI_SLOT x9, 16
    ldr x0, [x9]
    ldrh w1, [x9, #8]
    CI_HELPER CI_ASM_HELPER_float80_to_uint128
    ldr w3, [x24, #8]
    str w0, [x23, x3]
    CI_NEXT
CI_LABEL F80_TO_I64
    CI_SLOT x9, 16
    ldr x0, [x9]
    ldrh w1, [x9, #8]
    CI_HELPER CI_ASM_HELPER_float80_to_uint128
    ldr w3, [x24, #8]
    str x0, [x23, x3]
    CI_NEXT
CI_LABEL F80_TO_I128
    CI_SLOT x9, 16
    ldr x0, [x9]
    ldrh w1, [x9, #8]
    CI_HELPER CI_ASM_HELPER_float80_to_uint128
    CI_SLOT x3, 8
    stp x0, x1, [x3]
    CI_NEXT
CI_LABEL F128_TO_I8
    ldr w9, [x24, #16]
    ldr q0, [x23, x9]
    CI_HELPER CI_ASM_HELPER_float128_to_uint128
    ldr w3, [x24, #8]
    strb w0, [x23, x3]
    CI_NEXT
CI_LABEL F128_TO_I16
    ldr w9, [x24, #16]
    ldr q0, [x23, x9]
    CI_HELPER CI_ASM_HELPER_float128_to_uint128
    ldr w3, [x24, #8]
    strh w0, [x23, x3]
    CI_NEXT
CI_LABEL F128_TO_I32
    ldr w9, [x24, #16]
    ldr q0, [x23, x9]
    CI_HELPER CI_ASM_HELPER_float128_to_uint128
    ldr w3, [x24, #8]
    str w0, [x23, x3]
    CI_NEXT
CI_LABEL F128_TO_I64
    ldr w9, [x24, #16]
    ldr q0, [x23, x9]
    CI_HELPER CI_ASM_HELPER_float128_to_uint128
    ldr w3, [x24, #8]
    str x0, [x23, x3]
    CI_NEXT
CI_LABEL F128_TO_I128
    ldr w9, [x24, #16]
    ldr q0, [x23, x9]
    CI_HELPER CI_ASM_HELPER_float128_to_uint128
    CI_SLOT x3, 8
    stp x0, x1, [x3]
    CI_NEXT
// CI_FTOF_OPS
CI_LABEL F16_TO_BF16
    ldr w9, [x24, #16]
    ldrh w0, [x23, x9]
    mov w1, #0
    CI_HELPER CI_ASM_HELPER_float16_to_float
    mov w0, #1
    CI_HELPER CI_ASM_HELPER_float16_from_float
    ldr w3, [x24, #8]
    strh w0, [x23, x3]
    CI_NEXT
CI_LABEL F16_TO_F32
    ldr w9, [x24, #16]
    ldrh w0, [x23, x9]
    mov w1, #0
    CI_HELPER CI_ASM_HELPER_float16_to_float
    ldr w3, [x24, #8]
    str s0, [x23, x3]
    CI_NEXT
CI_LABEL F16_TO_F64
    ldr w9, [x24, #16]
    ldrh w0, [x23, x9]
    mov w1, #0
    CI_HELPER CI_ASM_HELPER_float16_to_double
    ldr w3, [x24, #8]
    str d0, [x23, x3]
    CI_NEXT
CI_LABEL F16_TO_F80
    ldr w9, [x24, #16]
    ldrh w0, [x23, x9]
    mov w1, #0
    CI_HELPER CI_ASM_HELPER_float16_to_float80
    CI_SLOT x3, 8
    str x0, [x3]
    strh w1, [x3, #8]
    mov w5, #10
    ldr w4, [x24, #12]
    b Lci_soft_padding
CI_LABEL F16_TO_F128
    ldr w9, [x24, #16]
    ldrh w0, [x23, x9]
    mov w1, #0
    CI_HELPER CI_ASM_HELPER_float16_to_quad
    ldr w3, [x24, #8]
    str q0, [x23, x3]
    CI_NEXT
CI_LABEL BF16_TO_F16
    ldr w9, [x24, #16]
    ldrh w0, [x23, x9]
    mov w1, #1
    CI_HELPER CI_ASM_HELPER_float16_to_float
    mov w0, #0
    CI_HELPER CI_ASM_HELPER_float16_from_float
    ldr w3, [x24, #8]
    strh w0, [x23, x3]
    CI_NEXT
CI_LABEL BF16_TO_F32
    ldr w9, [x24, #16]
    ldrh w0, [x23, x9]
    mov w1, #1
    CI_HELPER CI_ASM_HELPER_float16_to_float
    ldr w3, [x24, #8]
    str s0, [x23, x3]
    CI_NEXT
CI_LABEL BF16_TO_F64
    ldr w9, [x24, #16]
    ldrh w0, [x23, x9]
    mov w1, #1
    CI_HELPER CI_ASM_HELPER_float16_to_double
    ldr w3, [x24, #8]
    str d0, [x23, x3]
    CI_NEXT
CI_LABEL BF16_TO_F80
    ldr w9, [x24, #16]
    ldrh w0, [x23, x9]
    mov w1, #1
    CI_HELPER CI_ASM_HELPER_float16_to_float80
    CI_SLOT x3, 8
    str x0, [x3]
    strh w1, [x3, #8]
    mov w5, #10
    ldr w4, [x24, #12]
    b Lci_soft_padding
CI_LABEL BF16_TO_F128
    ldr w9, [x24, #16]
    ldrh w0, [x23, x9]
    mov w1, #1
    CI_HELPER CI_ASM_HELPER_float16_to_quad
    ldr w3, [x24, #8]
    str q0, [x23, x3]
    CI_NEXT
CI_LABEL F32_TO_F16
    ldr w9, [x24, #16]
    ldr s0, [x23, x9]
    mov w0, #0
    CI_HELPER CI_ASM_HELPER_float16_from_float
    ldr w3, [x24, #8]
    strh w0, [x23, x3]
    CI_NEXT
CI_LABEL F32_TO_BF16
    ldr w9, [x24, #16]
    ldr s0, [x23, x9]
    mov w0, #1
    CI_HELPER CI_ASM_HELPER_float16_from_float
    ldr w3, [x24, #8]
    strh w0, [x23, x3]
    CI_NEXT
CI_FTOF F32_TO_F64, 32, 64
CI_LABEL F32_TO_F80
    ldr w9, [x24, #16]
    ldr s0, [x23, x9]
    CI_HELPER CI_ASM_HELPER_float80_from_float
    CI_SLOT x3, 8
    str x0, [x3]
    strh w1, [x3, #8]
    mov w5, #10
    ldr w4, [x24, #12]
    b Lci_soft_padding
CI_LABEL F32_TO_F128
    ldr w9, [x24, #16]
    ldr s0, [x23, x9]
    CI_HELPER CI_ASM_HELPER_float128_from_float
    ldr w3, [x24, #8]
    str q0, [x23, x3]
    CI_NEXT
CI_LABEL F64_TO_F16
    ldr w9, [x24, #16]
    ldr d0, [x23, x9]
    mov w0, #0
    CI_HELPER CI_ASM_HELPER_float16_from_double
    ldr w3, [x24, #8]
    strh w0, [x23, x3]
    CI_NEXT
CI_LABEL F64_TO_BF16
    ldr w9, [x24, #16]
    ldr d0, [x23, x9]
    mov w0, #1
    CI_HELPER CI_ASM_HELPER_float16_from_double
    ldr w3, [x24, #8]
    strh w0, [x23, x3]
    CI_NEXT
CI_FTOF F64_TO_F32, 64, 32
CI_LABEL F64_TO_F80
    ldr w9, [x24, #16]
    ldr d0, [x23, x9]
    CI_HELPER CI_ASM_HELPER_float80_from_double
    CI_SLOT x3, 8
    str x0, [x3]
    strh w1, [x3, #8]
    mov w5, #10
    ldr w4, [x24, #12]
    b Lci_soft_padding
CI_LABEL F64_TO_F128
    ldr w9, [x24, #16]
    ldr d0, [x23, x9]
    CI_HELPER CI_ASM_HELPER_float128_from_double
    ldr w3, [x24, #8]
    str q0, [x23, x3]
    CI_NEXT
CI_LABEL F80_TO_F16
    CI_SLOT x9, 16
    ldr x0, [x9]
    ldrh w1, [x9, #8]
    mov w2, #0
    CI_HELPER CI_ASM_HELPER_float16_from_float80
    ldr w3, [x24, #8]
    strh w0, [x23, x3]
    CI_NEXT
CI_LABEL F80_TO_BF16
    CI_SLOT x9, 16
    ldr x0, [x9]
    ldrh w1, [x9, #8]
    mov w2, #1
    CI_HELPER CI_ASM_HELPER_float16_from_float80
    ldr w3, [x24, #8]
    strh w0, [x23, x3]
    CI_NEXT
CI_LABEL F80_TO_F32
    CI_SLOT x9, 16
    ldr x0, [x9]
    ldrh w1, [x9, #8]
    CI_HELPER CI_ASM_HELPER_float80_to_float
    ldr w3, [x24, #8]
    str s0, [x23, x3]
    CI_NEXT
CI_LABEL F80_TO_F64
    CI_SLOT x9, 16
    ldr x0, [x9]
    ldrh w1, [x9, #8]
    CI_HELPER CI_ASM_HELPER_float80_to_double
    ldr w3, [x24, #8]
    str d0, [x23, x3]
    CI_NEXT
CI_LABEL F80_TO_F128
    CI_SLOT x9, 16
    ldr x0, [x9]
    ldrh w1, [x9, #8]
    CI_HELPER CI_ASM_HELPER_float128_from_float80
    ldr w3, [x24, #8]
    str q0, [x23, x3]
    CI_NEXT
CI_LABEL F128_TO_F16
    ldr w9, [x24, #16]
    ldr q0, [x23, x9]
    mov w0, #0
    CI_HELPER CI_ASM_HELPER_float16_from_quad
    ldr w3, [x24, #8]
    strh w0, [x23, x3]
    CI_NEXT
CI_LABEL F128_TO_BF16
    ldr w9, [x24, #16]
    ldr q0, [x23, x9]
    mov w0, #1
    CI_HELPER CI_ASM_HELPER_float16_from_quad
    ldr w3, [x24, #8]
    strh w0, [x23, x3]
    CI_NEXT
CI_LABEL F128_TO_F32
    ldr w9, [x24, #16]
    ldr q0, [x23, x9]
    CI_HELPER CI_ASM_HELPER_float128_to_float
    ldr w3, [x24, #8]
    str s0, [x23, x3]
    CI_NEXT
CI_LABEL F128_TO_F64
    ldr w9, [x24, #16]
    ldr q0, [x23, x9]
    CI_HELPER CI_ASM_HELPER_float128_to_double
    ldr w3, [x24, #8]
    str d0, [x23, x3]
    CI_NEXT
CI_LABEL F128_TO_F80
    ldr w9, [x24, #16]
    ldr q0, [x23, x9]
    CI_HELPER CI_ASM_HELPER_float80_from_float128
    CI_SLOT x3, 8
    str x0, [x3]
    strh w1, [x3, #8]
    mov w5, #10
    ldr w4, [x24, #12]
    b Lci_soft_padding
CI_LABEL ABI_READ_GP01_64
    ldr w9, [x24, #4]
    add x9, x23, x9
    ldr w10, [x24, #8]
    add x10, x23, x10
    str x0, [x9]
    str x1, [x10]
    CI_NEXT

CI_LABEL ABI_READ_GP01_64_A
    ldr w9, [x24, #4]
    add x9, x23, x9
    ldr w10, [x24, #8]
    add x10, x23, x10
    str x0, [x9]
    str x1, [x10]
    CI_NEXT

CI_LABEL ABI_READ_GP01_64_CONTIG
    ldr w9, [x24, #4]
    add x9, x23, x9
    stp x0, x1, [x9]
    CI_NEXT

CI_LABEL ABI_READ_GP01_64_CONTIG_A
    ldr w9, [x24, #4]
    add x9, x23, x9
    stp x0, x1, [x9]
    CI_NEXT

CI_LABEL ABI_WRITE_GP01_64
    ldr w9, [x24, #4]
    add x9, x23, x9
    ldr w10, [x24, #8]
    add x10, x23, x10
    ldr x0, [x9]
    ldr x1, [x10]
    CI_NEXT

CI_LABEL ABI_WRITE_GP01_64_A
    ldr w9, [x24, #4]
    add x9, x23, x9
    ldr w10, [x24, #8]
    add x10, x23, x10
    ldr x0, [x9]
    ldr x1, [x10]
    CI_NEXT

CI_LABEL ABI_WRITE_GP01_64_CONTIG
    ldr w9, [x24, #4]
    add x9, x23, x9
    ldp x0, x1, [x9]
    CI_NEXT

CI_LABEL ABI_WRITE_GP01_64_CONTIG_A
    ldr w9, [x24, #4]
    add x9, x23, x9
    ldp x0, x1, [x9]
    CI_NEXT

CI_LABEL ABI_READ_GP0_8
    ldr w9, [x24, #4]
    strb w0, [x23, x9]
    CI_NEXT

CI_LABEL ABI_WRITE_GP0_8
    ldr w9, [x24, #4]
    ldrb w0, [x23, x9]
    CI_NEXT

CI_LABEL ABI_READ_GP0_32
    ldr w9, [x24, #4]
    str w0, [x23, x9]
    CI_NEXT

CI_LABEL ABI_READ_GP0_32_A
    ldr w9, [x24, #4]
    str w0, [x23, x9]
    CI_NEXT

CI_LABEL ABI_WRITE_GP0_32
    ldr w9, [x24, #4]
    ldr w0, [x23, x9]
    CI_NEXT

CI_LABEL ABI_WRITE_GP0_32_A
    ldr w9, [x24, #4]
    ldr w0, [x23, x9]
    CI_NEXT

CI_LABEL ABI_SEXT_GP0_8
    ldr w9, [x24, #4]
    ldrsb x0, [x23, x9]
    CI_NEXT

CI_LABEL ABI_ZEXT_GP0_8
    ldr w9, [x24, #4]
    ldrb w0, [x23, x9]
    CI_NEXT

CI_LABEL ABI_SEXT_GP0_32
    ldr w9, [x24, #4]
    ldrsw x0, [x23, x9]
    CI_NEXT

CI_LABEL ABI_SEXT_GP0_32_A
    ldr w9, [x24, #4]
    ldrsw x0, [x23, x9]
    CI_NEXT

CI_LABEL ABI_ZEXT_GP0_32
    ldr w9, [x24, #4]
    ldr w0, [x23, x9]
    CI_NEXT

CI_LABEL ABI_ZEXT_GP0_32_A
    ldr w9, [x24, #4]
    ldr w0, [x23, x9]
    CI_NEXT

CI_LABEL ABI_READ_GP1_8
    ldr w9, [x24, #4]
    strb w1, [x23, x9]
    CI_NEXT

CI_LABEL ABI_WRITE_GP1_8
    ldr w9, [x24, #4]
    ldrb w1, [x23, x9]
    CI_NEXT

CI_LABEL ABI_READ_GP1_32
    ldr w9, [x24, #4]
    str w1, [x23, x9]
    CI_NEXT

CI_LABEL ABI_READ_GP1_32_A
    ldr w9, [x24, #4]
    str w1, [x23, x9]
    CI_NEXT

CI_LABEL ABI_WRITE_GP1_32
    ldr w9, [x24, #4]
    ldr w1, [x23, x9]
    CI_NEXT

CI_LABEL ABI_WRITE_GP1_32_A
    ldr w9, [x24, #4]
    ldr w1, [x23, x9]
    CI_NEXT

CI_LABEL ABI_SEXT_GP1_8
    ldr w9, [x24, #4]
    ldrsb x1, [x23, x9]
    CI_NEXT

CI_LABEL ABI_ZEXT_GP1_8
    ldr w9, [x24, #4]
    ldrb w1, [x23, x9]
    CI_NEXT

CI_LABEL ABI_SEXT_GP1_32
    ldr w9, [x24, #4]
    ldrsw x1, [x23, x9]
    CI_NEXT

CI_LABEL ABI_SEXT_GP1_32_A
    ldr w9, [x24, #4]
    ldrsw x1, [x23, x9]
    CI_NEXT

CI_LABEL ABI_ZEXT_GP1_32
    ldr w9, [x24, #4]
    ldr w1, [x23, x9]
    CI_NEXT

CI_LABEL ABI_ZEXT_GP1_32_A
    ldr w9, [x24, #4]
    ldr w1, [x23, x9]
    CI_NEXT

CI_LABEL ABI_READ_GP2_8
    ldr w9, [x24, #4]
    strb w2, [x23, x9]
    CI_NEXT

CI_LABEL ABI_WRITE_GP2_8
    ldr w9, [x24, #4]
    ldrb w2, [x23, x9]
    CI_NEXT

CI_LABEL ABI_READ_GP2_32
    ldr w9, [x24, #4]
    str w2, [x23, x9]
    CI_NEXT

CI_LABEL ABI_READ_GP2_32_A
    ldr w9, [x24, #4]
    str w2, [x23, x9]
    CI_NEXT

CI_LABEL ABI_WRITE_GP2_32
    ldr w9, [x24, #4]
    ldr w2, [x23, x9]
    CI_NEXT

CI_LABEL ABI_WRITE_GP2_32_A
    ldr w9, [x24, #4]
    ldr w2, [x23, x9]
    CI_NEXT

CI_LABEL ABI_SEXT_GP2_8
    ldr w9, [x24, #4]
    ldrsb x2, [x23, x9]
    CI_NEXT

CI_LABEL ABI_ZEXT_GP2_8
    ldr w9, [x24, #4]
    ldrb w2, [x23, x9]
    CI_NEXT

CI_LABEL ABI_SEXT_GP2_32
    ldr w9, [x24, #4]
    ldrsw x2, [x23, x9]
    CI_NEXT

CI_LABEL ABI_SEXT_GP2_32_A
    ldr w9, [x24, #4]
    ldrsw x2, [x23, x9]
    CI_NEXT

CI_LABEL ABI_ZEXT_GP2_32
    ldr w9, [x24, #4]
    ldr w2, [x23, x9]
    CI_NEXT

CI_LABEL ABI_ZEXT_GP2_32_A
    ldr w9, [x24, #4]
    ldr w2, [x23, x9]
    CI_NEXT

CI_LABEL ABI_READ_GP3_8
    ldr w9, [x24, #4]
    strb w3, [x23, x9]
    CI_NEXT

CI_LABEL ABI_WRITE_GP3_8
    ldr w9, [x24, #4]
    ldrb w3, [x23, x9]
    CI_NEXT

CI_LABEL ABI_READ_GP3_32
    ldr w9, [x24, #4]
    str w3, [x23, x9]
    CI_NEXT

CI_LABEL ABI_READ_GP3_32_A
    ldr w9, [x24, #4]
    str w3, [x23, x9]
    CI_NEXT

CI_LABEL ABI_WRITE_GP3_32
    ldr w9, [x24, #4]
    ldr w3, [x23, x9]
    CI_NEXT

CI_LABEL ABI_WRITE_GP3_32_A
    ldr w9, [x24, #4]
    ldr w3, [x23, x9]
    CI_NEXT

CI_LABEL ABI_SEXT_GP3_8
    ldr w9, [x24, #4]
    ldrsb x3, [x23, x9]
    CI_NEXT

CI_LABEL ABI_ZEXT_GP3_8
    ldr w9, [x24, #4]
    ldrb w3, [x23, x9]
    CI_NEXT

CI_LABEL ABI_SEXT_GP3_32
    ldr w9, [x24, #4]
    ldrsw x3, [x23, x9]
    CI_NEXT

CI_LABEL ABI_SEXT_GP3_32_A
    ldr w9, [x24, #4]
    ldrsw x3, [x23, x9]
    CI_NEXT

CI_LABEL ABI_ZEXT_GP3_32
    ldr w9, [x24, #4]
    ldr w3, [x23, x9]
    CI_NEXT

CI_LABEL ABI_ZEXT_GP3_32_A
    ldr w9, [x24, #4]
    ldr w3, [x23, x9]
    CI_NEXT

CI_LABEL ABI_READ_GP4_8
    ldr w9, [x24, #4]
    strb w4, [x23, x9]
    CI_NEXT

CI_LABEL ABI_WRITE_GP4_8
    ldr w9, [x24, #4]
    ldrb w4, [x23, x9]
    CI_NEXT

CI_LABEL ABI_READ_GP4_32
    ldr w9, [x24, #4]
    str w4, [x23, x9]
    CI_NEXT

CI_LABEL ABI_READ_GP4_32_A
    ldr w9, [x24, #4]
    str w4, [x23, x9]
    CI_NEXT

CI_LABEL ABI_WRITE_GP4_32
    ldr w9, [x24, #4]
    ldr w4, [x23, x9]
    CI_NEXT

CI_LABEL ABI_WRITE_GP4_32_A
    ldr w9, [x24, #4]
    ldr w4, [x23, x9]
    CI_NEXT

CI_LABEL ABI_SEXT_GP4_8
    ldr w9, [x24, #4]
    ldrsb x4, [x23, x9]
    CI_NEXT

CI_LABEL ABI_ZEXT_GP4_8
    ldr w9, [x24, #4]
    ldrb w4, [x23, x9]
    CI_NEXT

CI_LABEL ABI_SEXT_GP4_32
    ldr w9, [x24, #4]
    ldrsw x4, [x23, x9]
    CI_NEXT

CI_LABEL ABI_SEXT_GP4_32_A
    ldr w9, [x24, #4]
    ldrsw x4, [x23, x9]
    CI_NEXT

CI_LABEL ABI_ZEXT_GP4_32
    ldr w9, [x24, #4]
    ldr w4, [x23, x9]
    CI_NEXT

CI_LABEL ABI_ZEXT_GP4_32_A
    ldr w9, [x24, #4]
    ldr w4, [x23, x9]
    CI_NEXT

CI_LABEL ABI_READ_GP5_8
    ldr w9, [x24, #4]
    strb w5, [x23, x9]
    CI_NEXT

CI_LABEL ABI_WRITE_GP5_8
    ldr w9, [x24, #4]
    ldrb w5, [x23, x9]
    CI_NEXT

CI_LABEL ABI_READ_GP5_32
    ldr w9, [x24, #4]
    str w5, [x23, x9]
    CI_NEXT

CI_LABEL ABI_READ_GP5_32_A
    ldr w9, [x24, #4]
    str w5, [x23, x9]
    CI_NEXT

CI_LABEL ABI_WRITE_GP5_32
    ldr w9, [x24, #4]
    ldr w5, [x23, x9]
    CI_NEXT

CI_LABEL ABI_WRITE_GP5_32_A
    ldr w9, [x24, #4]
    ldr w5, [x23, x9]
    CI_NEXT

CI_LABEL ABI_SEXT_GP5_8
    ldr w9, [x24, #4]
    ldrsb x5, [x23, x9]
    CI_NEXT

CI_LABEL ABI_ZEXT_GP5_8
    ldr w9, [x24, #4]
    ldrb w5, [x23, x9]
    CI_NEXT

CI_LABEL ABI_SEXT_GP5_32
    ldr w9, [x24, #4]
    ldrsw x5, [x23, x9]
    CI_NEXT

CI_LABEL ABI_SEXT_GP5_32_A
    ldr w9, [x24, #4]
    ldrsw x5, [x23, x9]
    CI_NEXT

CI_LABEL ABI_ZEXT_GP5_32
    ldr w9, [x24, #4]
    ldr w5, [x23, x9]
    CI_NEXT

CI_LABEL ABI_ZEXT_GP5_32_A
    ldr w9, [x24, #4]
    ldr w5, [x23, x9]
    CI_NEXT

CI_LABEL ABI_READ_GP6_8
    ldr w9, [x24, #4]
    strb w6, [x23, x9]
    CI_NEXT

CI_LABEL ABI_WRITE_GP6_8
    ldr w9, [x24, #4]
    ldrb w6, [x23, x9]
    CI_NEXT

CI_LABEL ABI_READ_GP6_32
    ldr w9, [x24, #4]
    str w6, [x23, x9]
    CI_NEXT

CI_LABEL ABI_READ_GP6_32_A
    ldr w9, [x24, #4]
    str w6, [x23, x9]
    CI_NEXT

CI_LABEL ABI_WRITE_GP6_32
    ldr w9, [x24, #4]
    ldr w6, [x23, x9]
    CI_NEXT

CI_LABEL ABI_WRITE_GP6_32_A
    ldr w9, [x24, #4]
    ldr w6, [x23, x9]
    CI_NEXT

CI_LABEL ABI_SEXT_GP6_8
    ldr w9, [x24, #4]
    ldrsb x6, [x23, x9]
    CI_NEXT

CI_LABEL ABI_ZEXT_GP6_8
    ldr w9, [x24, #4]
    ldrb w6, [x23, x9]
    CI_NEXT

CI_LABEL ABI_SEXT_GP6_32
    ldr w9, [x24, #4]
    ldrsw x6, [x23, x9]
    CI_NEXT

CI_LABEL ABI_SEXT_GP6_32_A
    ldr w9, [x24, #4]
    ldrsw x6, [x23, x9]
    CI_NEXT

CI_LABEL ABI_ZEXT_GP6_32
    ldr w9, [x24, #4]
    ldr w6, [x23, x9]
    CI_NEXT

CI_LABEL ABI_ZEXT_GP6_32_A
    ldr w9, [x24, #4]
    ldr w6, [x23, x9]
    CI_NEXT

CI_LABEL ABI_READ_GP7_8
    ldr w9, [x24, #4]
    strb w7, [x23, x9]
    CI_NEXT

CI_LABEL ABI_WRITE_GP7_8
    ldr w9, [x24, #4]
    ldrb w7, [x23, x9]
    CI_NEXT

CI_LABEL ABI_READ_GP7_32
    ldr w9, [x24, #4]
    str w7, [x23, x9]
    CI_NEXT

CI_LABEL ABI_READ_GP7_32_A
    ldr w9, [x24, #4]
    str w7, [x23, x9]
    CI_NEXT

CI_LABEL ABI_WRITE_GP7_32
    ldr w9, [x24, #4]
    ldr w7, [x23, x9]
    CI_NEXT

CI_LABEL ABI_WRITE_GP7_32_A
    ldr w9, [x24, #4]
    ldr w7, [x23, x9]
    CI_NEXT

CI_LABEL ABI_SEXT_GP7_8
    ldr w9, [x24, #4]
    ldrsb x7, [x23, x9]
    CI_NEXT

CI_LABEL ABI_ZEXT_GP7_8
    ldr w9, [x24, #4]
    ldrb w7, [x23, x9]
    CI_NEXT

CI_LABEL ABI_SEXT_GP7_32
    ldr w9, [x24, #4]
    ldrsw x7, [x23, x9]
    CI_NEXT

CI_LABEL ABI_SEXT_GP7_32_A
    ldr w9, [x24, #4]
    ldrsw x7, [x23, x9]
    CI_NEXT

CI_LABEL ABI_ZEXT_GP7_32
    ldr w9, [x24, #4]
    ldr w7, [x23, x9]
    CI_NEXT

CI_LABEL ABI_ZEXT_GP7_32_A
    ldr w9, [x24, #4]
    ldr w7, [x23, x9]
    CI_NEXT

CI_LABEL ABI_READ_FP0_32
    ldr w9, [x24, #4]
    str s0, [x23, x9]
    CI_NEXT

CI_LABEL ABI_READ_FP0_32_A
    ldr w9, [x24, #4]
    str s0, [x23, x9]
    CI_NEXT

CI_LABEL ABI_WRITE_FP0_32
    ldr w9, [x24, #4]
    ldr s0, [x23, x9]
    CI_NEXT

CI_LABEL ABI_WRITE_FP0_32_A
    ldr w9, [x24, #4]
    ldr s0, [x23, x9]
    CI_NEXT

CI_LABEL ABI_READ_FP0_64
    ldr w9, [x24, #4]
    str d0, [x23, x9]
    CI_NEXT

CI_LABEL ABI_READ_FP0_64_A
    ldr w9, [x24, #4]
    str d0, [x23, x9]
    CI_NEXT

CI_LABEL ABI_WRITE_FP0_64
    ldr w9, [x24, #4]
    ldr d0, [x23, x9]
    CI_NEXT

CI_LABEL ABI_WRITE_FP0_64_A
    ldr w9, [x24, #4]
    ldr d0, [x23, x9]
    CI_NEXT

CI_LABEL ABI_READ_FP0_128
    ldr w9, [x24, #4]
    str q0, [x23, x9]
    CI_NEXT

CI_LABEL ABI_READ_FP0_128_A
    ldr w9, [x24, #4]
    str q0, [x23, x9]
    CI_NEXT

CI_LABEL ABI_WRITE_FP0_128
    ldr w9, [x24, #4]
    ldr q0, [x23, x9]
    CI_NEXT

CI_LABEL ABI_WRITE_FP0_128_A
    ldr w9, [x24, #4]
    ldr q0, [x23, x9]
    CI_NEXT

CI_LABEL ABI_READ_FP1_32
    ldr w9, [x24, #4]
    str s1, [x23, x9]
    CI_NEXT

CI_LABEL ABI_READ_FP1_32_A
    ldr w9, [x24, #4]
    str s1, [x23, x9]
    CI_NEXT

CI_LABEL ABI_WRITE_FP1_32
    ldr w9, [x24, #4]
    ldr s1, [x23, x9]
    CI_NEXT

CI_LABEL ABI_WRITE_FP1_32_A
    ldr w9, [x24, #4]
    ldr s1, [x23, x9]
    CI_NEXT

CI_LABEL ABI_READ_FP1_64
    ldr w9, [x24, #4]
    str d1, [x23, x9]
    CI_NEXT

CI_LABEL ABI_READ_FP1_64_A
    ldr w9, [x24, #4]
    str d1, [x23, x9]
    CI_NEXT

CI_LABEL ABI_WRITE_FP1_64
    ldr w9, [x24, #4]
    ldr d1, [x23, x9]
    CI_NEXT

CI_LABEL ABI_WRITE_FP1_64_A
    ldr w9, [x24, #4]
    ldr d1, [x23, x9]
    CI_NEXT

CI_LABEL ABI_READ_FP1_128
    ldr w9, [x24, #4]
    str q1, [x23, x9]
    CI_NEXT

CI_LABEL ABI_READ_FP1_128_A
    ldr w9, [x24, #4]
    str q1, [x23, x9]
    CI_NEXT

CI_LABEL ABI_WRITE_FP1_128
    ldr w9, [x24, #4]
    ldr q1, [x23, x9]
    CI_NEXT

CI_LABEL ABI_WRITE_FP1_128_A
    ldr w9, [x24, #4]
    ldr q1, [x23, x9]
    CI_NEXT

CI_LABEL ABI_READ_FP2_32
    ldr w9, [x24, #4]
    str s2, [x23, x9]
    CI_NEXT

CI_LABEL ABI_READ_FP2_32_A
    ldr w9, [x24, #4]
    str s2, [x23, x9]
    CI_NEXT

CI_LABEL ABI_WRITE_FP2_32
    ldr w9, [x24, #4]
    ldr s2, [x23, x9]
    CI_NEXT

CI_LABEL ABI_WRITE_FP2_32_A
    ldr w9, [x24, #4]
    ldr s2, [x23, x9]
    CI_NEXT

CI_LABEL ABI_READ_FP2_64
    ldr w9, [x24, #4]
    str d2, [x23, x9]
    CI_NEXT

CI_LABEL ABI_READ_FP2_64_A
    ldr w9, [x24, #4]
    str d2, [x23, x9]
    CI_NEXT

CI_LABEL ABI_WRITE_FP2_64
    ldr w9, [x24, #4]
    ldr d2, [x23, x9]
    CI_NEXT

CI_LABEL ABI_WRITE_FP2_64_A
    ldr w9, [x24, #4]
    ldr d2, [x23, x9]
    CI_NEXT

CI_LABEL ABI_READ_FP2_128
    ldr w9, [x24, #4]
    str q2, [x23, x9]
    CI_NEXT

CI_LABEL ABI_READ_FP2_128_A
    ldr w9, [x24, #4]
    str q2, [x23, x9]
    CI_NEXT

CI_LABEL ABI_WRITE_FP2_128
    ldr w9, [x24, #4]
    ldr q2, [x23, x9]
    CI_NEXT

CI_LABEL ABI_WRITE_FP2_128_A
    ldr w9, [x24, #4]
    ldr q2, [x23, x9]
    CI_NEXT

CI_LABEL ABI_READ_FP3_32
    ldr w9, [x24, #4]
    str s3, [x23, x9]
    CI_NEXT

CI_LABEL ABI_READ_FP3_32_A
    ldr w9, [x24, #4]
    str s3, [x23, x9]
    CI_NEXT

CI_LABEL ABI_WRITE_FP3_32
    ldr w9, [x24, #4]
    ldr s3, [x23, x9]
    CI_NEXT

CI_LABEL ABI_WRITE_FP3_32_A
    ldr w9, [x24, #4]
    ldr s3, [x23, x9]
    CI_NEXT

CI_LABEL ABI_READ_FP3_64
    ldr w9, [x24, #4]
    str d3, [x23, x9]
    CI_NEXT

CI_LABEL ABI_READ_FP3_64_A
    ldr w9, [x24, #4]
    str d3, [x23, x9]
    CI_NEXT

CI_LABEL ABI_WRITE_FP3_64
    ldr w9, [x24, #4]
    ldr d3, [x23, x9]
    CI_NEXT

CI_LABEL ABI_WRITE_FP3_64_A
    ldr w9, [x24, #4]
    ldr d3, [x23, x9]
    CI_NEXT

CI_LABEL ABI_READ_FP3_128
    ldr w9, [x24, #4]
    str q3, [x23, x9]
    CI_NEXT

CI_LABEL ABI_READ_FP3_128_A
    ldr w9, [x24, #4]
    str q3, [x23, x9]
    CI_NEXT

CI_LABEL ABI_WRITE_FP3_128
    ldr w9, [x24, #4]
    ldr q3, [x23, x9]
    CI_NEXT

CI_LABEL ABI_WRITE_FP3_128_A
    ldr w9, [x24, #4]
    ldr q3, [x23, x9]
    CI_NEXT

CI_LABEL ABI_READ_FP4_32
    ldr w9, [x24, #4]
    str s4, [x23, x9]
    CI_NEXT

CI_LABEL ABI_READ_FP4_32_A
    ldr w9, [x24, #4]
    str s4, [x23, x9]
    CI_NEXT

CI_LABEL ABI_WRITE_FP4_32
    ldr w9, [x24, #4]
    ldr s4, [x23, x9]
    CI_NEXT

CI_LABEL ABI_WRITE_FP4_32_A
    ldr w9, [x24, #4]
    ldr s4, [x23, x9]
    CI_NEXT

CI_LABEL ABI_READ_FP4_64
    ldr w9, [x24, #4]
    str d4, [x23, x9]
    CI_NEXT

CI_LABEL ABI_READ_FP4_64_A
    ldr w9, [x24, #4]
    str d4, [x23, x9]
    CI_NEXT

CI_LABEL ABI_WRITE_FP4_64
    ldr w9, [x24, #4]
    ldr d4, [x23, x9]
    CI_NEXT

CI_LABEL ABI_WRITE_FP4_64_A
    ldr w9, [x24, #4]
    ldr d4, [x23, x9]
    CI_NEXT

CI_LABEL ABI_READ_FP4_128
    ldr w9, [x24, #4]
    str q4, [x23, x9]
    CI_NEXT

CI_LABEL ABI_READ_FP4_128_A
    ldr w9, [x24, #4]
    str q4, [x23, x9]
    CI_NEXT

CI_LABEL ABI_WRITE_FP4_128
    ldr w9, [x24, #4]
    ldr q4, [x23, x9]
    CI_NEXT

CI_LABEL ABI_WRITE_FP4_128_A
    ldr w9, [x24, #4]
    ldr q4, [x23, x9]
    CI_NEXT

CI_LABEL ABI_READ_FP5_32
    ldr w9, [x24, #4]
    str s5, [x23, x9]
    CI_NEXT

CI_LABEL ABI_READ_FP5_32_A
    ldr w9, [x24, #4]
    str s5, [x23, x9]
    CI_NEXT

CI_LABEL ABI_WRITE_FP5_32
    ldr w9, [x24, #4]
    ldr s5, [x23, x9]
    CI_NEXT

CI_LABEL ABI_WRITE_FP5_32_A
    ldr w9, [x24, #4]
    ldr s5, [x23, x9]
    CI_NEXT

CI_LABEL ABI_READ_FP5_64
    ldr w9, [x24, #4]
    str d5, [x23, x9]
    CI_NEXT

CI_LABEL ABI_READ_FP5_64_A
    ldr w9, [x24, #4]
    str d5, [x23, x9]
    CI_NEXT

CI_LABEL ABI_WRITE_FP5_64
    ldr w9, [x24, #4]
    ldr d5, [x23, x9]
    CI_NEXT

CI_LABEL ABI_WRITE_FP5_64_A
    ldr w9, [x24, #4]
    ldr d5, [x23, x9]
    CI_NEXT

CI_LABEL ABI_READ_FP5_128
    ldr w9, [x24, #4]
    str q5, [x23, x9]
    CI_NEXT

CI_LABEL ABI_READ_FP5_128_A
    ldr w9, [x24, #4]
    str q5, [x23, x9]
    CI_NEXT

CI_LABEL ABI_WRITE_FP5_128
    ldr w9, [x24, #4]
    ldr q5, [x23, x9]
    CI_NEXT

CI_LABEL ABI_WRITE_FP5_128_A
    ldr w9, [x24, #4]
    ldr q5, [x23, x9]
    CI_NEXT

CI_LABEL ABI_READ_FP6_32
    ldr w9, [x24, #4]
    str s6, [x23, x9]
    CI_NEXT

CI_LABEL ABI_READ_FP6_32_A
    ldr w9, [x24, #4]
    str s6, [x23, x9]
    CI_NEXT

CI_LABEL ABI_WRITE_FP6_32
    ldr w9, [x24, #4]
    ldr s6, [x23, x9]
    CI_NEXT

CI_LABEL ABI_WRITE_FP6_32_A
    ldr w9, [x24, #4]
    ldr s6, [x23, x9]
    CI_NEXT

CI_LABEL ABI_READ_FP6_64
    ldr w9, [x24, #4]
    str d6, [x23, x9]
    CI_NEXT

CI_LABEL ABI_READ_FP6_64_A
    ldr w9, [x24, #4]
    str d6, [x23, x9]
    CI_NEXT

CI_LABEL ABI_WRITE_FP6_64
    ldr w9, [x24, #4]
    ldr d6, [x23, x9]
    CI_NEXT

CI_LABEL ABI_WRITE_FP6_64_A
    ldr w9, [x24, #4]
    ldr d6, [x23, x9]
    CI_NEXT

CI_LABEL ABI_READ_FP6_128
    ldr w9, [x24, #4]
    str q6, [x23, x9]
    CI_NEXT

CI_LABEL ABI_READ_FP6_128_A
    ldr w9, [x24, #4]
    str q6, [x23, x9]
    CI_NEXT

CI_LABEL ABI_WRITE_FP6_128
    ldr w9, [x24, #4]
    ldr q6, [x23, x9]
    CI_NEXT

CI_LABEL ABI_WRITE_FP6_128_A
    ldr w9, [x24, #4]
    ldr q6, [x23, x9]
    CI_NEXT

CI_LABEL ABI_READ_FP7_32
    ldr w9, [x24, #4]
    str s7, [x23, x9]
    CI_NEXT

CI_LABEL ABI_READ_FP7_32_A
    ldr w9, [x24, #4]
    str s7, [x23, x9]
    CI_NEXT

CI_LABEL ABI_WRITE_FP7_32
    ldr w9, [x24, #4]
    ldr s7, [x23, x9]
    CI_NEXT

CI_LABEL ABI_WRITE_FP7_32_A
    ldr w9, [x24, #4]
    ldr s7, [x23, x9]
    CI_NEXT

CI_LABEL ABI_READ_FP7_64
    ldr w9, [x24, #4]
    str d7, [x23, x9]
    CI_NEXT

CI_LABEL ABI_READ_FP7_64_A
    ldr w9, [x24, #4]
    str d7, [x23, x9]
    CI_NEXT

CI_LABEL ABI_WRITE_FP7_64
    ldr w9, [x24, #4]
    ldr d7, [x23, x9]
    CI_NEXT

CI_LABEL ABI_WRITE_FP7_64_A
    ldr w9, [x24, #4]
    ldr d7, [x23, x9]
    CI_NEXT

CI_LABEL ABI_READ_FP7_128
    ldr w9, [x24, #4]
    str q7, [x23, x9]
    CI_NEXT

CI_LABEL ABI_READ_FP7_128_A
    ldr w9, [x24, #4]
    str q7, [x23, x9]
    CI_NEXT

CI_LABEL ABI_WRITE_FP7_128
    ldr w9, [x24, #4]
    ldr q7, [x23, x9]
    CI_NEXT

CI_LABEL ABI_WRITE_FP7_128_A
    ldr w9, [x24, #4]
    ldr q7, [x23, x9]
    CI_NEXT

CI_ABI_GP_MOVE ABI_READ_GP0_64, 0, 0
CI_ABI_GP_MOVE ABI_READ_GP0_64_A, 0, 0
CI_ABI_GP_MOVE ABI_WRITE_GP0_64, 1, 0
CI_ABI_GP_MOVE ABI_WRITE_GP0_64_A, 1, 0
CI_ABI_GP_MOVE ABI_READ_GP1_64, 0, 1
CI_ABI_GP_MOVE ABI_READ_GP1_64_A, 0, 1
CI_ABI_GP_MOVE ABI_WRITE_GP1_64, 1, 1
CI_ABI_GP_MOVE ABI_WRITE_GP1_64_A, 1, 1
CI_ABI_GP_MOVE ABI_READ_GP2_64, 0, 2
CI_ABI_GP_MOVE ABI_READ_GP2_64_A, 0, 2
CI_ABI_GP_MOVE ABI_WRITE_GP2_64, 1, 2
CI_ABI_GP_MOVE ABI_WRITE_GP2_64_A, 1, 2
CI_ABI_GP_MOVE ABI_READ_GP3_64, 0, 3
CI_ABI_GP_MOVE ABI_READ_GP3_64_A, 0, 3
CI_ABI_GP_MOVE ABI_WRITE_GP3_64, 1, 3
CI_ABI_GP_MOVE ABI_WRITE_GP3_64_A, 1, 3
CI_ABI_GP_MOVE ABI_READ_GP4_64, 0, 4
CI_ABI_GP_MOVE ABI_READ_GP4_64_A, 0, 4
CI_ABI_GP_MOVE ABI_WRITE_GP4_64, 1, 4
CI_ABI_GP_MOVE ABI_WRITE_GP4_64_A, 1, 4
CI_ABI_GP_MOVE ABI_READ_GP5_64, 0, 5
CI_ABI_GP_MOVE ABI_READ_GP5_64_A, 0, 5
CI_ABI_GP_MOVE ABI_WRITE_GP5_64, 1, 5
CI_ABI_GP_MOVE ABI_WRITE_GP5_64_A, 1, 5
CI_ABI_GP_MOVE ABI_READ_GP6_64, 0, 6
CI_ABI_GP_MOVE ABI_READ_GP6_64_A, 0, 6
CI_ABI_GP_MOVE ABI_WRITE_GP6_64, 1, 6
CI_ABI_GP_MOVE ABI_WRITE_GP6_64_A, 1, 6
CI_ABI_GP_MOVE ABI_READ_GP7_64, 0, 7
CI_ABI_GP_MOVE ABI_READ_GP7_64_A, 0, 7
CI_ABI_GP_MOVE ABI_WRITE_GP7_64, 1, 7
CI_ABI_GP_MOVE ABI_WRITE_GP7_64_A, 1, 7

// CI_ABI_MOVE_OPS
CI_ABI_MOVE ABI_READ_IN8, 1, 0, 1
CI_ABI_MOVE ABI_READ_IN16, 1, 0, 2
CI_ABI_MOVE ABI_READ_IN16_A, 1, 0, 2
CI_ABI_MOVE ABI_READ_IN32, 1, 0, 4
CI_ABI_MOVE ABI_READ_IN32_A, 1, 0, 4
CI_ABI_MOVE ABI_READ_IN64, 1, 0, 8
CI_ABI_MOVE ABI_READ_IN64_A, 1, 0, 8
CI_ABI_MOVE ABI_READ_IN128, 1, 0, 16
CI_ABI_MOVE ABI_READ_IN128_A, 1, 0, 16
CI_ABI_MOVE ABI_READ_IN_N, 1, 0, 0
CI_ABI_MOVE ABI_READ_IN_N_A2, 1, 0, 0
CI_ABI_MOVE ABI_READ_IN_N_A4, 1, 0, 0
CI_ABI_MOVE ABI_READ_IN_N_A8, 1, 0, 0
CI_ABI_MOVE ABI_READ_IN_N_A16, 1, 0, 0
CI_ABI_MOVE ABI_READ_OUT8, 0, 0, 1
CI_ABI_MOVE ABI_READ_OUT16, 0, 0, 2
CI_ABI_MOVE ABI_READ_OUT16_A, 0, 0, 2
CI_ABI_MOVE ABI_READ_OUT32, 0, 0, 4
CI_ABI_MOVE ABI_READ_OUT32_A, 0, 0, 4
CI_ABI_MOVE ABI_READ_OUT64, 0, 0, 8
CI_ABI_MOVE ABI_READ_OUT64_A, 0, 0, 8
CI_ABI_MOVE ABI_READ_OUT128, 0, 0, 16
CI_ABI_MOVE ABI_READ_OUT128_A, 0, 0, 16
CI_ABI_MOVE ABI_READ_OUT_N, 0, 0, 0
CI_ABI_MOVE ABI_READ_OUT_N_A2, 0, 0, 0
CI_ABI_MOVE ABI_READ_OUT_N_A4, 0, 0, 0
CI_ABI_MOVE ABI_READ_OUT_N_A8, 0, 0, 0
CI_ABI_MOVE ABI_READ_OUT_N_A16, 0, 0, 0
CI_ABI_MOVE ABI_WRITE_OUT8, 0, 1, 1
CI_ABI_MOVE ABI_WRITE_OUT16, 0, 1, 2
CI_ABI_MOVE ABI_WRITE_OUT16_A, 0, 1, 2
CI_ABI_MOVE ABI_WRITE_OUT32, 0, 1, 4
CI_ABI_MOVE ABI_WRITE_OUT32_A, 0, 1, 4
CI_ABI_MOVE ABI_WRITE_OUT64, 0, 1, 8
CI_ABI_MOVE ABI_WRITE_OUT64_A, 0, 1, 8
CI_ABI_MOVE ABI_WRITE_OUT128, 0, 1, 16
CI_ABI_MOVE ABI_WRITE_OUT128_A, 0, 1, 16
CI_ABI_MOVE ABI_WRITE_OUT_N, 0, 1, 0
CI_ABI_MOVE ABI_WRITE_OUT_N_A2, 0, 1, 0
CI_ABI_MOVE ABI_WRITE_OUT_N_A4, 0, 1, 0
CI_ABI_MOVE ABI_WRITE_OUT_N_A8, 0, 1, 0
CI_ABI_MOVE ABI_WRITE_OUT_N_A16, 0, 1, 0
CI_ABI_MOVE ABI_WRITE_SEXT8, 0, 1, 8, 1, 1
CI_ABI_MOVE ABI_WRITE_SEXT16, 0, 1, 8, 2, 1
CI_ABI_MOVE ABI_WRITE_SEXT16_A, 0, 1, 8, 2, 1
CI_ABI_MOVE ABI_WRITE_SEXT32, 0, 1, 8, 4, 1
CI_ABI_MOVE ABI_WRITE_SEXT32_A, 0, 1, 8, 4, 1
CI_ABI_MOVE ABI_WRITE_ZEXT8, 0, 1, 8, 1, 0
CI_ABI_MOVE ABI_WRITE_ZEXT16, 0, 1, 8, 2, 0
CI_ABI_MOVE ABI_WRITE_ZEXT16_A, 0, 1, 8, 2, 0
CI_ABI_MOVE ABI_WRITE_ZEXT32, 0, 1, 8, 4, 0
CI_ABI_MOVE ABI_WRITE_ZEXT32_A, 0, 1, 8, 4, 0
// CI_ABI_TRANSFER_OPS
CI_ABI_TRANSFER ABI_COPY_SLOT_TO_REG_PTR_8, 0, 3, 0, 1
CI_ABI_TRANSFER ABI_COPY_SLOT_TO_REG_PTR_16, 0, 3, 0, 2
CI_ABI_TRANSFER ABI_COPY_SLOT_TO_REG_PTR_16_A, 0, 3, 0, 2
CI_ABI_TRANSFER ABI_COPY_SLOT_TO_REG_PTR_32, 0, 3, 0, 4
CI_ABI_TRANSFER ABI_COPY_SLOT_TO_REG_PTR_32_A, 0, 3, 0, 4
CI_ABI_TRANSFER ABI_COPY_SLOT_TO_REG_PTR_64, 0, 3, 0, 8
CI_ABI_TRANSFER ABI_COPY_SLOT_TO_REG_PTR_64_A, 0, 3, 0, 8
CI_ABI_TRANSFER ABI_COPY_SLOT_TO_REG_PTR_128, 0, 3, 0, 16
CI_ABI_TRANSFER ABI_COPY_SLOT_TO_REG_PTR_128_A, 0, 3, 0, 16
CI_ABI_TRANSFER ABI_COPY_SLOT_TO_REG_PTR_N, 0, 3, 0, 0
CI_ABI_TRANSFER ABI_COPY_SLOT_TO_REG_PTR_N_A2, 0, 3, 0, 0
CI_ABI_TRANSFER ABI_COPY_SLOT_TO_REG_PTR_N_A4, 0, 3, 0, 0
CI_ABI_TRANSFER ABI_COPY_SLOT_TO_REG_PTR_N_A8, 0, 3, 0, 0
CI_ABI_TRANSFER ABI_COPY_SLOT_TO_REG_PTR_N_A16, 0, 3, 0, 0
CI_ABI_TRANSFER ABI_COPY_REG_PTR_TO_SLOT_8, 0, 0, 3, 1
CI_ABI_TRANSFER ABI_COPY_REG_PTR_TO_SLOT_16, 0, 0, 3, 2
CI_ABI_TRANSFER ABI_COPY_REG_PTR_TO_SLOT_16_A, 0, 0, 3, 2
CI_ABI_TRANSFER ABI_COPY_REG_PTR_TO_SLOT_32, 0, 0, 3, 4
CI_ABI_TRANSFER ABI_COPY_REG_PTR_TO_SLOT_32_A, 0, 0, 3, 4
CI_ABI_TRANSFER ABI_COPY_REG_PTR_TO_SLOT_64, 0, 0, 3, 8
CI_ABI_TRANSFER ABI_COPY_REG_PTR_TO_SLOT_64_A, 0, 0, 3, 8
CI_ABI_TRANSFER ABI_COPY_REG_PTR_TO_SLOT_128, 0, 0, 3, 16
CI_ABI_TRANSFER ABI_COPY_REG_PTR_TO_SLOT_128_A, 0, 0, 3, 16
CI_ABI_TRANSFER ABI_COPY_REG_PTR_TO_SLOT_N, 0, 0, 3, 0
CI_ABI_TRANSFER ABI_COPY_REG_PTR_TO_SLOT_N_A2, 0, 0, 3, 0
CI_ABI_TRANSFER ABI_COPY_REG_PTR_TO_SLOT_N_A4, 0, 0, 3, 0
CI_ABI_TRANSFER ABI_COPY_REG_PTR_TO_SLOT_N_A8, 0, 0, 3, 0
CI_ABI_TRANSFER ABI_COPY_REG_PTR_TO_SLOT_N_A16, 0, 0, 3, 0
CI_ABI_TRANSFER ABI_COPY_REG_PTR_INDIRECT_TO_SLOT_8, 0, 0, 5, 1
CI_ABI_TRANSFER ABI_COPY_REG_PTR_INDIRECT_TO_SLOT_16, 0, 0, 5, 2
CI_ABI_TRANSFER ABI_COPY_REG_PTR_INDIRECT_TO_SLOT_16_A, 0, 0, 5, 2
CI_ABI_TRANSFER ABI_COPY_REG_PTR_INDIRECT_TO_SLOT_32, 0, 0, 5, 4
CI_ABI_TRANSFER ABI_COPY_REG_PTR_INDIRECT_TO_SLOT_32_A, 0, 0, 5, 4
CI_ABI_TRANSFER ABI_COPY_REG_PTR_INDIRECT_TO_SLOT_64, 0, 0, 5, 8
CI_ABI_TRANSFER ABI_COPY_REG_PTR_INDIRECT_TO_SLOT_64_A, 0, 0, 5, 8
CI_ABI_TRANSFER ABI_COPY_REG_PTR_INDIRECT_TO_SLOT_128, 0, 0, 5, 16
CI_ABI_TRANSFER ABI_COPY_REG_PTR_INDIRECT_TO_SLOT_128_A, 0, 0, 5, 16
CI_ABI_TRANSFER ABI_COPY_REG_PTR_INDIRECT_TO_SLOT_N, 0, 0, 5, 0
CI_ABI_TRANSFER ABI_COPY_REG_PTR_INDIRECT_TO_SLOT_N_A2, 0, 0, 5, 0
CI_ABI_TRANSFER ABI_COPY_REG_PTR_INDIRECT_TO_SLOT_N_A4, 0, 0, 5, 0
CI_ABI_TRANSFER ABI_COPY_REG_PTR_INDIRECT_TO_SLOT_N_A8, 0, 0, 5, 0
CI_ABI_TRANSFER ABI_COPY_REG_PTR_INDIRECT_TO_SLOT_N_A16, 0, 0, 5, 0
CI_ABI_TRANSFER ABI_COPY_SLOT_TO_SLOT_PTR_8, 0, 7, 0, 1
CI_ABI_TRANSFER ABI_COPY_SLOT_TO_SLOT_PTR_16, 0, 7, 0, 2
CI_ABI_TRANSFER ABI_COPY_SLOT_TO_SLOT_PTR_16_A, 0, 7, 0, 2
CI_ABI_TRANSFER ABI_COPY_SLOT_TO_SLOT_PTR_32, 0, 7, 0, 4
CI_ABI_TRANSFER ABI_COPY_SLOT_TO_SLOT_PTR_32_A, 0, 7, 0, 4
CI_ABI_TRANSFER ABI_COPY_SLOT_TO_SLOT_PTR_64, 0, 7, 0, 8
CI_ABI_TRANSFER ABI_COPY_SLOT_TO_SLOT_PTR_64_A, 0, 7, 0, 8
CI_ABI_TRANSFER ABI_COPY_SLOT_TO_SLOT_PTR_128, 0, 7, 0, 16
CI_ABI_TRANSFER ABI_COPY_SLOT_TO_SLOT_PTR_128_A, 0, 7, 0, 16
CI_ABI_TRANSFER ABI_COPY_SLOT_TO_SLOT_PTR_N, 0, 7, 0, 0
CI_ABI_TRANSFER ABI_COPY_SLOT_TO_SLOT_PTR_N_A2, 0, 7, 0, 0
CI_ABI_TRANSFER ABI_COPY_SLOT_TO_SLOT_PTR_N_A4, 0, 7, 0, 0
CI_ABI_TRANSFER ABI_COPY_SLOT_TO_SLOT_PTR_N_A8, 0, 7, 0, 0
CI_ABI_TRANSFER ABI_COPY_SLOT_TO_SLOT_PTR_N_A16, 0, 7, 0, 0
CI_ABI_TRANSFER ABI_COPY_INCOMING_TO_INCOMING_PTR_64_A, 0, 4, 2, 8
CI_ABI_TRANSFER ABI_ADDRESS_REG_PTR_TO_REG_PTR, 1, 1, 3, 0
CI_ABI_TRANSFER ABI_ADDRESS_REG_PTR_TO_REG_PTR_A, 1, 1, 3, 0
CI_ABI_TRANSFER ABI_ADDRESS_REG_PTR_TO_REG_PTR_PTR, 1, 3, 3, 0
CI_ABI_TRANSFER ABI_ADDRESS_REG_PTR_TO_REG_PTR_PTR_A, 1, 3, 3, 0
CI_ABI_TRANSFER ABI_ADDRESS_SLOT_PTR_TO_REG_PTR, 1, 1, 7, 0
CI_ABI_TRANSFER ABI_ADDRESS_SLOT_PTR_TO_REG_PTR_A, 1, 1, 7, 0
CI_ABI_TRANSFER ABI_ADDRESS_SLOT_TO_REG_PTR, 1, 1, 0, 0
CI_ABI_TRANSFER ABI_ADDRESS_SLOT_TO_REG_PTR_A, 1, 1, 0, 0
CI_ABI_TRANSFER ABI_ADDRESS_SLOT_TO_SLOT_PTR, 1, 0, 0, 0
CI_ABI_TRANSFER ABI_ADDRESS_SLOT_TO_SLOT_PTR_A, 1, 0, 0, 0
CI_ABI_TRANSFER ABI_ADDRESS_INCOMING_PTR_TO_SLOT_PTR_PTR, 1, 7, 4, 0
CI_ABI_TRANSFER ABI_ADDRESS_INCOMING_PTR_TO_SLOT_PTR_PTR_A, 1, 7, 4, 0
CI_ABI_TRANSFER ABI_ADDRESS_INCOMING_SAVE_TO_SLOT_PTR_PTR, 1, 7, 8, 0
CI_ABI_TRANSFER ABI_ADDRESS_INCOMING_SAVE_TO_SLOT_PTR_PTR_A, 1, 7, 8, 0
CI_ABI_TRANSFER ABI_ZERO_SLOT_8, 2, 0, 0, 1
CI_ABI_TRANSFER ABI_ZERO_SLOT_16, 2, 0, 0, 2
CI_ABI_TRANSFER ABI_ZERO_SLOT_16_A, 2, 0, 0, 2
CI_ABI_TRANSFER ABI_ZERO_SLOT_32, 2, 0, 0, 4
CI_ABI_TRANSFER ABI_ZERO_SLOT_32_A, 2, 0, 0, 4
CI_ABI_TRANSFER ABI_ZERO_SLOT_64, 2, 0, 0, 8
CI_ABI_TRANSFER ABI_ZERO_SLOT_64_A, 2, 0, 0, 8
CI_ABI_TRANSFER ABI_ZERO_SLOT_128, 2, 0, 0, 16
CI_ABI_TRANSFER ABI_ZERO_SLOT_128_A, 2, 0, 0, 16
CI_ABI_TRANSFER ABI_ZERO_SLOT_N, 2, 0, 0, 0
CI_ABI_TRANSFER ABI_ZERO_SLOT_N_A2, 2, 0, 0, 0
CI_ABI_TRANSFER ABI_ZERO_SLOT_N_A4, 2, 0, 0, 0
CI_ABI_TRANSFER ABI_ZERO_SLOT_N_A8, 2, 0, 0, 0
CI_ABI_TRANSFER ABI_ZERO_SLOT_N_A16, 2, 0, 0, 0
CI_ABI_TRANSFER ABI_CONSTANT_REG_8, 3, 1, 0, 1
CI_ABI_TRANSFER ABI_CONSTANT_SLOT_PTR_32, 3, 7, 0, 4
CI_ABI_TRANSFER ABI_CONSTANT_SLOT_PTR_32_A, 3, 7, 0, 4
// CI_JUMP_FALSE_OPS
CI_COND_JUMP JZ8, 1, 0
CI_COND_JUMP JZ16, 2, 0
CI_COND_JUMP JZ32, 4, 0
CI_COND_JUMP JZ64, 8, 0
CI_COND_JUMP JZ128, 16, 0
// CI_JUMP_TRUE_OPS
CI_COND_JUMP JNZ8, 1, 1
CI_COND_JUMP JNZ16, 2, 1
CI_COND_JUMP JNZ32, 4, 1
CI_COND_JUMP JNZ64, 8, 1
CI_COND_JUMP JNZ128, 16, 1
// CI_BSWAP_OPS
CI_BSWAP BSWAP16, 2
CI_BSWAP BSWAP32, 4
CI_BSWAP BSWAP64, 8
// CI_BOUNDS_OPS
CI_BOUNDS BOUNDS32, 4, 0
CI_BOUNDS BOUNDS_INCLUSIVE32, 4, 1
CI_BOUNDS BOUNDS64, 8, 0
CI_BOUNDS BOUNDS_INCLUSIVE64, 8, 1
// CI_INDEX_OPS
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
// CI_LOAD_BITFIELD_OPS
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
CI_BF_LOAD LDBFS_U128, 16, 0, 0, 1
CI_BF_LOAD LDBFS_U128_A, 16, 0, 0, 1
CI_BF_LOAD LDBFS_S128, 16, 1, 0, 1
CI_BF_LOAD LDBFS_S128_A, 16, 1, 0, 1
CI_BF_LOAD LDBFBS_U8, 1, 0, 1, 1
CI_BF_LOAD LDBFBS_S8, 1, 1, 1, 1
CI_BF_LOAD LDBFBS_U16, 2, 0, 1, 1
CI_BF_LOAD LDBFBS_S16, 2, 1, 1, 1
CI_BF_LOAD LDBFBS_U32, 4, 0, 1, 1
CI_BF_LOAD LDBFBS_S32, 4, 1, 1, 1
CI_BF_LOAD LDBFBS_U64, 8, 0, 1, 1
CI_BF_LOAD LDBFBS_S64, 8, 1, 1, 1
CI_BF_LOAD LDBFBS_U128, 16, 0, 1, 1
CI_BF_LOAD LDBFBS_S128, 16, 1, 1, 1

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
// CI_LOAD_BITFIELD_128_OPS
CI_BF_LOAD LDBF_U128, 16, 0, 0
CI_BF_LOAD LDBF_U128_A, 16, 0, 0
CI_BF_LOAD LDBF_S128, 16, 1, 0
CI_BF_LOAD LDBF_S128_A, 16, 1, 0
// CI_LOAD_BITFIELD_BYTES_OPS
CI_BF_LOAD LDBFB_U8, 1, 0, 1
CI_BF_LOAD LDBFB_S8, 1, 1, 1
CI_BF_LOAD LDBFB_U16, 2, 0, 1
CI_BF_LOAD LDBFB_S16, 2, 1, 1
CI_BF_LOAD LDBFB_U32, 4, 0, 1
CI_BF_LOAD LDBFB_S32, 4, 1, 1
CI_BF_LOAD LDBFB_U64, 8, 0, 1
CI_BF_LOAD LDBFB_S64, 8, 1, 1
// CI_LOAD_BITFIELD_BYTES_128_OPS
CI_BF_LOAD LDBFB_U128, 16, 0, 1
CI_BF_LOAD LDBFB_S128, 16, 1, 1
// CI_STORE_BITFIELD_OPS
CI_BF_STORE STBF8, 1, 0
CI_BF_STORE STBF16, 2, 0
CI_BF_STORE STBF16_A, 2, 0
CI_BF_STORE STBF32, 4, 0
CI_BF_STORE STBF32_A, 4, 0
CI_BF_STORE STBF64, 8, 0
CI_BF_STORE STBF64_A, 8, 0
// CI_STORE_BITFIELD_128_OPS
CI_BF_STORE STBF128, 16, 0
CI_BF_STORE STBF128_A, 16, 0
// CI_STORE_BITFIELD_BYTES_OPS
CI_BF_STORE STBFB8, 1, 1
CI_BF_STORE STBFB16, 2, 1
CI_BF_STORE STBFB32, 4, 1
CI_BF_STORE STBFB64, 8, 1
// CI_STORE_BITFIELD_BYTES_128_OPS
CI_BF_STORE STBFB128, 16, 1
// CI_CALL_OPS
CI_LABEL CALL_RESOLVE
    ldr w9, [x24, #4]
    ldr x1, [x23, x9]
    ldr x0, [x19]
    CI_HELPER CI_ASM_HELPER_closure_lookup
    ldr w9, [x24, #4]
    add x9, x23, x9
    str x0, [x9, #8]
    CI_NEXT

CI_CALL_SERVICE CALL_DIRECT, 0
CI_CALL_SERVICE CALL_INDIRECT, 1
// CI_SWITCH_DENSE_OPS
CI_SWITCH SWITCH_DENSE, 1, 0
// CI_SWITCH_RANGE_OPS
CI_SWITCH SWITCH_SIGNED, 0, 1
CI_SWITCH SWITCH_UNSIGNED, 0, 0
// CI_BUILTIN_OPS
CI_LABEL TRAP
    CI_ERROR Lci_msg_trap
CI_LABEL UNREACHABLE
    CI_ERROR Lci_msg_unreachable
CI_LABEL ABORT
    CI_ERROR Lci_msg_abort
CI_LABEL DEBUGTRAP
    CI_NEXT
CI_LABEL BACKTRACE
    CI_PUBLISH_PC
    ldr x0, [x19]
    mov x1, x15
    mov w2, #0
    CI_HELPER CI_ASM_HELPER_backtrace
    CI_NEXT
CI_LABEL READCYCLECOUNTER
    CI_HELPER CI_ASM_HELPER_cyclecounter
    ldr w9, [x24, #4]
    str x0, [x23, x9]
    CI_NEXT
CI_LABEL READCYCLECOUNTER_DISCARD
    CI_HELPER CI_ASM_HELPER_cyclecounter
    CI_NEXT
// CI_ALLOCA_OPS
Lci_alloc_failed:
    mov w0, #1
    b Lci_error
CI_LABEL STACK_ALLOC
    mov x0, x28
    CI_LOAD_SLOT 12, 8
    ldr w2, [x24, #4]
    CI_HELPER CI_ASM_HELPER_stack_alloc
    cbz x0, Lci_alloc_failed
    ldr w9, [x24, #8]
    str x0, [x23, x9]
    CI_NEXT
CI_LABEL STACK_ZALLOC
    mov x0, x28
    CI_LOAD_SLOT 12, 8
    ldr w2, [x24, #4]
    CI_HELPER CI_ASM_HELPER_stack_zalloc
    cbz x0, Lci_alloc_failed
    ldr w9, [x24, #8]
    str x0, [x23, x9]
    CI_NEXT
// CI_FENCE_OPS
CI_LABEL THREAD_FENCE
    dmb ish
    CI_NEXT
CI_LABEL SIGNAL_FENCE
    CI_NEXT
// CI_BIT_COUNT_OPS
CI_BIT FFS8, 8, 0
CI_BIT FFS16, 16, 0
CI_BIT FFS32, 32, 0
CI_BIT FFS64, 64, 0
CI_BIT FFS128, 128, 0
CI_BIT CLRSB8, 8, 1
CI_BIT CLRSB16, 16, 1
CI_BIT CLRSB32, 32, 1
CI_BIT CLRSB64, 64, 1
CI_BIT CLRSB128, 128, 1
CI_BIT PARITY8, 8, 2
CI_BIT PARITY16, 16, 2
CI_BIT PARITY32, 32, 2
CI_BIT PARITY64, 64, 2
CI_BIT PARITY128, 128, 2
CI_BIT CLZ8, 8, 3
CI_BIT CLZ16, 16, 3
CI_BIT CLZ32, 32, 3
CI_BIT CLZ64, 64, 3
CI_BIT CLZ128, 128, 3
CI_BIT CTZ8, 8, 4
CI_BIT CTZ16, 16, 4
CI_BIT CTZ32, 32, 4
CI_BIT CTZ64, 64, 4
CI_BIT CTZ128, 128, 4
CI_BIT POPCOUNT8, 8, 5
CI_BIT POPCOUNT16, 16, 5
CI_BIT POPCOUNT32, 32, 5
CI_BIT POPCOUNT64, 64, 5
CI_BIT POPCOUNT128, 128, 5
CI_BIT BIT_WIDTH8, 8, 8
CI_BIT BIT_WIDTH16, 16, 8
CI_BIT BIT_WIDTH32, 32, 8
CI_BIT BIT_WIDTH64, 64, 8
CI_BIT BIT_WIDTH128, 128, 8
CI_BIT COUNT_ZEROS8, 8, 10
CI_BIT COUNT_ZEROS16, 16, 10
CI_BIT COUNT_ZEROS32, 32, 10
CI_BIT COUNT_ZEROS64, 64, 10
CI_BIT COUNT_ZEROS128, 128, 10
CI_BIT FIRST_LEADING_ONE8, 8, 11
CI_BIT FIRST_LEADING_ONE16, 16, 11
CI_BIT FIRST_LEADING_ONE32, 32, 11
CI_BIT FIRST_LEADING_ONE64, 64, 11
CI_BIT FIRST_LEADING_ONE128, 128, 11
CI_BIT FIRST_LEADING_ZERO8, 8, 12
CI_BIT FIRST_LEADING_ZERO16, 16, 12
CI_BIT FIRST_LEADING_ZERO32, 32, 12
CI_BIT FIRST_LEADING_ZERO64, 64, 12
CI_BIT FIRST_LEADING_ZERO128, 128, 12
CI_BIT FIRST_TRAILING_ZERO8, 8, 14
CI_BIT FIRST_TRAILING_ZERO16, 16, 14
CI_BIT FIRST_TRAILING_ZERO32, 32, 14
CI_BIT FIRST_TRAILING_ZERO64, 64, 14
CI_BIT FIRST_TRAILING_ZERO128, 128, 14
CI_BIT LEADING_ONES8, 8, 16
CI_BIT LEADING_ONES16, 16, 16
CI_BIT LEADING_ONES32, 32, 16
CI_BIT LEADING_ONES64, 64, 16
CI_BIT LEADING_ONES128, 128, 16
CI_BIT TRAILING_ONES8, 8, 18
CI_BIT TRAILING_ONES16, 16, 18
CI_BIT TRAILING_ONES32, 32, 18
CI_BIT TRAILING_ONES64, 64, 18
CI_BIT TRAILING_ONES128, 128, 18
// CI_SINGLE_BIT_OPS
CI_BIT HAS_SINGLE_BIT8, 8, 15
CI_BIT HAS_SINGLE_BIT16, 16, 15
CI_BIT HAS_SINGLE_BIT32, 32, 15
CI_BIT HAS_SINGLE_BIT64, 64, 15
CI_BIT HAS_SINGLE_BIT128, 128, 15
// CI_BIT_CEIL_OPS
CI_BIT BIT_CEIL8, 8, 6
CI_BIT BIT_CEIL16, 16, 6
CI_BIT BIT_CEIL32, 32, 6
CI_BIT BIT_CEIL64, 64, 6
CI_BIT BIT_CEIL128, 128, 6
// CI_BIT_FLOOR_OPS
CI_BIT BIT_FLOOR8, 8, 7
CI_BIT BIT_FLOOR16, 16, 7
CI_BIT BIT_FLOOR32, 32, 7
CI_BIT BIT_FLOOR64, 64, 7
CI_BIT BIT_FLOOR128, 128, 7
// CI_ROTATE_OPS
CI_BIT ROTL8, 8, 20
CI_BIT ROTR8, 8, 21
CI_BIT ROTL16, 16, 20
CI_BIT ROTR16, 16, 21
CI_BIT ROTL32, 32, 20
CI_BIT ROTR32, 32, 21
CI_BIT ROTL64, 64, 20
CI_BIT ROTR64, 64, 21
CI_BIT ROTL128, 128, 20
CI_BIT ROTR128, 128, 21
// CI_CRC_OPS
CI_CRC CRC32C8, 1
CI_CRC CRC32C16, 2
CI_CRC CRC32C32, 4
CI_CRC CRC32C64, 8
// CI_ATOMIC_LOAD_OPS
CI_ATOMIC_LOAD ATOMIC_LOAD8, 1
CI_ATOMIC_LOAD ATOMIC_LOAD16, 2
CI_ATOMIC_LOAD ATOMIC_LOAD32, 4
CI_ATOMIC_LOAD ATOMIC_LOAD64, 8
CI_ATOMIC_LOAD ATOMIC_LOAD128, 16
// CI_ATOMIC_STORE_OPS
CI_ATOMIC_STORE ATOMIC_STORE8, 1
CI_ATOMIC_STORE ATOMIC_STORE16, 2
CI_ATOMIC_STORE ATOMIC_STORE32, 4
CI_ATOMIC_STORE ATOMIC_STORE64, 8
CI_ATOMIC_STORE ATOMIC_STORE128, 16
// CI_ATOMIC_RMW_OPS
CI_ATOMIC_RMW ATOMIC_XCHG8, 1, 0
CI_ATOMIC_RMW ATOMIC_XCHG16, 2, 0
CI_ATOMIC_RMW ATOMIC_XCHG32, 4, 0
CI_ATOMIC_RMW ATOMIC_XCHG64, 8, 0
CI_ATOMIC_RMW ATOMIC_XCHG128, 16, 0
CI_ATOMIC_RMW ATOMIC_ADD8, 1, 1
CI_ATOMIC_RMW ATOMIC_ADD16, 2, 1
CI_ATOMIC_RMW ATOMIC_ADD32, 4, 1
CI_ATOMIC_RMW ATOMIC_ADD64, 8, 1
CI_ATOMIC_RMW ATOMIC_SUB8, 1, 2
CI_ATOMIC_RMW ATOMIC_SUB16, 2, 2
CI_ATOMIC_RMW ATOMIC_SUB32, 4, 2
CI_ATOMIC_RMW ATOMIC_SUB64, 8, 2
CI_ATOMIC_RMW ATOMIC_AND8, 1, 3
CI_ATOMIC_RMW ATOMIC_AND16, 2, 3
CI_ATOMIC_RMW ATOMIC_AND32, 4, 3
CI_ATOMIC_RMW ATOMIC_AND64, 8, 3
CI_ATOMIC_RMW ATOMIC_OR8, 1, 4
CI_ATOMIC_RMW ATOMIC_OR16, 2, 4
CI_ATOMIC_RMW ATOMIC_OR32, 4, 4
CI_ATOMIC_RMW ATOMIC_OR64, 8, 4
CI_ATOMIC_RMW ATOMIC_XOR8, 1, 5
CI_ATOMIC_RMW ATOMIC_XOR16, 2, 5
CI_ATOMIC_RMW ATOMIC_XOR32, 4, 5
CI_ATOMIC_RMW ATOMIC_XOR64, 8, 5
// CI_ATOMIC_CAS_OPS
CI_ATOMIC_CAS ATOMIC_CAS8, 1, 0
CI_ATOMIC_CAS ATOMIC_CAS_WEAK8, 1, 1
CI_ATOMIC_CAS ATOMIC_CAS16, 2, 0
CI_ATOMIC_CAS ATOMIC_CAS_WEAK16, 2, 1
CI_ATOMIC_CAS ATOMIC_CAS32, 4, 0
CI_ATOMIC_CAS ATOMIC_CAS_WEAK32, 4, 1
CI_ATOMIC_CAS ATOMIC_CAS64, 8, 0
CI_ATOMIC_CAS ATOMIC_CAS_WEAK64, 8, 1
CI_ATOMIC_CAS ATOMIC_CAS128, 16, 0
CI_ATOMIC_CAS ATOMIC_CAS_WEAK128, 16, 1
// CI_VA_SYSV_OPS
    CI_VA_DECODE VA_ARG_SYSV
    ldp w6, w7, [x0]
    add w8, w6, w2, lsl #3
    cmp w8, #48
    b.hi Lci_va_sysv_stack
    add w9, w7, w3, lsl #4
    cmp w9, #176
    b.hi Lci_va_sysv_stack
    ldr x10, [x0, #16]
    add x11, x10, w6, uxtw
    add x12, x10, w7, uxtw
    stp x11, x12, [x1]
    stp w8, w9, [x0]
    CI_NEXT
Lci_va_sysv_stack:
    add x0, x0, #8
    CI_VA_STACK_SOURCE 0, 1
// CI_VA_ARM_OPS
CI_VA_ARM VA_ARG_ARM
CI_VA_ARM VA_ARG_ARM_ALIGN16, 1
// CI_VA_ARM_INDIRECT_OPS
    CI_VA_DECODE VA_ARG_ARM_INDIRECT
    ldr w6, [x0, #24]
    cmp w6, #0
    b.ge Lci_va_arm_indirect_stack
    adds w7, w6, #8
    b.gt Lci_va_arm_indirect_stack
    str w7, [x0, #24]
    ldr x2, [x0, #8]
    add x2, x2, w6, sxtw
    ldr x2, [x2]
    str x2, [x1]
    CI_NEXT
Lci_va_arm_indirect_stack:
    str wzr, [x0, #24]
    mov w4, #7
    mov w5, #8
    CI_VA_STACK_SOURCE 1
// CI_VA_STACK_OPS
    CI_VA_DECODE VA_ARG_STACK
    CI_VA_STACK_SOURCE 0
    CI_VA_DECODE VA_ARG_STACK_INDIRECT
    CI_VA_STACK_SOURCE 1
    CI_VA_DECODE VA_ARG_SYSV_STACK
    add x0, x0, #8
    CI_VA_STACK_SOURCE 0
    CI_VA_DECODE VA_ARG_ARM_STACK
    CI_VA_STACK_SOURCE 0
.cfi_endproc
.size ci_interp_execute_asm, .-ci_interp_execute_asm
.pushsection .data.rel.ro,"aw",@progbits
.p2align 3
Lci_dispatch_table:
#define CI_NATIVE_ENTRY(name, suffix) Lci_##name##suffix,
#define CI_FLAT_ENTRY(name, ...) Lci_##name,
    .quad CI_OPCODE_NATIVE_LIST(CI_NATIVE_ENTRY) CI_OPCODE_FLAT_LIST(CI_FLAT_ENTRY) Lci_invalid
#undef CI_NATIVE_ENTRY
#undef CI_FLAT_ENTRY
.if (. - Lci_dispatch_table) != (CI_ASM_op_count + 1) * 8
.error "ARM64 dispatch table opcode count mismatch"
.endif
.popsection
.section .note.GNU-stack,"",%progbits

.section .rodata
Lci_msg_null_call:
    .asciz "call through null function pointer"
Lci_msg_invalid:
    .asciz "invalid assembly bytecode operands for opcode %u"
Lci_msg_close_inclusive:
    .asciz "]"
Lci_msg_close_exclusive:
    .asciz ")"
Lci_msg_bounds_unsigned:
    .asciz "array subscript out of bounds: index %llu not in [0, %llu%s"
Lci_msg_bounds_signed:
    .asciz "array subscript out of bounds: index %lld not in [0, %llu%s"
Lci_msg_unknown:
    .asciz "<unknown>"
Lci_msg_missing_function:
    .asciz "function '%s' not resolved before execution"
Lci_msg_string:
    .asciz "%s"
Lci_msg_result_size:
    .asciz "result buffer too small (need %zu, have %zu)"
Lci_msg_capture_null:
    .asciz "__builtin_capture_backtrace buffer must not be NULL"
Lci_msg_trap:
    .asciz "__builtin_trap"
Lci_msg_unreachable:
    .asciz "__builtin_unreachable reached"
Lci_msg_abort:
    .asciz "__builtin_abort called"


Lci_msg_hotswap:
    .asciz "hotswap cycle detected"

.text
.p2align 2
.globl ci_interp_callback_asm
.hidden ci_interp_callback_asm
ci_interp_callback_asm:
.cfi_startproc
    bti c
    stp x29, x30, [sp, #-128]!
.cfi_def_cfa_offset 128
.cfi_offset 29, -128
.cfi_offset 30, -120
    mov x29, sp
    stp x19, x23, [sp, #16]
    stp x24, x26, [sp, #32]
    stp x28, x20, [sp, #48]
.cfi_offset 19, -112
.cfi_offset 23, -104
.cfi_offset 24, -96
.cfi_offset 26, -88
.cfi_offset 28, -80
.cfi_offset 20, -72
    add x19, sp, #64
.cfi_def_cfa x19, 64
    ldr x17, [x16]
    ldr x9, [x17]
    stp x9, xzr, [x19]
    ldr x9, [x17, #16]
    str x9, [x19, #16]
    stp xzr, xzr, [x19, #32]
    ldr x9, [x17, #24]
    str x9, [x19, #48]
    add x20, sp, #128
    mov x23, #0
    mov x28, #0
    adrp x26, Lci_dispatch_table
    add x26, x26, :lo12:Lci_dispatch_table
    ldr x10, [x17, #8]
    CI_RESOLVE_FUNCTION
    ldr x9, [x10, #CI_ASM_func_code]
    ldr x24, [x9, #8]
    add x24, x24, #CI_ASM_code_ops
    b Lci_dispatch
.cfi_endproc

.size ci_interp_callback_asm, .-ci_interp_callback_asm
