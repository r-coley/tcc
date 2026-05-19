.text
.data
    .align 4
.global _g
_g:
    .zero 12
.text
.align 2
.global _main
_main:
    stp x29, x30, [sp, #-16]!
    mov x29, sp
    sub sp, sp, #0
    movz x0, #0
    str x0, [sp, #-16]!
    movz x0, #40
    str x0, [sp, #-16]!
    ldr x0, [sp], #16
    ldr x1, [sp], #16
    sxtw x1, w1
    lsl x1, x1, #2
    adrp x2, _g@PAGE
    add x2, x2, _g@PAGEOFF
    str w0, [x2, x1]
    movz x0, #1
    str x0, [sp, #-16]!
    movz x0, #2
    str x0, [sp, #-16]!
    ldr x0, [sp], #16
    ldr x1, [sp], #16
    sxtw x1, w1
    lsl x1, x1, #2
    adrp x2, _g@PAGE
    add x2, x2, _g@PAGEOFF
    str w0, [x2, x1]
    movz x0, #0
    str x0, [sp, #-16]!
    ldr x0, [sp], #16
    sxtw x0, w0
    lsl x0, x0, #2
    adrp x2, _g@PAGE
    add x2, x2, _g@PAGEOFF
    ldrsw x0, [x2, x0]
    str x0, [sp, #-16]!
    movz x0, #1
    str x0, [sp, #-16]!
    ldr x0, [sp], #16
    sxtw x0, w0
    lsl x0, x0, #2
    adrp x2, _g@PAGE
    add x2, x2, _g@PAGEOFF
    ldrsw x0, [x2, x0]
    str x0, [sp, #-16]!
    ldr x0, [sp], #16
    ldr x1, [sp], #16
    add x0, x1, x0
    str x0, [sp, #-16]!
    ldr x0, [sp], #16
    mov sp, x29
    ldp x29, x30, [sp], #16
    ret
    movz x0, #0
    mov sp, x29
    ldp x29, x30, [sp], #16
    ret
