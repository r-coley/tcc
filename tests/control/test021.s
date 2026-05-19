.text
.align 2
.global _main
_main:
    stp x29, x30, [sp, #-16]!
    mov x29, sp
    sub sp, sp, #32
    movz x0, #0
    str x0, [sp, #-16]!
    movz x0, #11
    str x0, [sp, #-16]!
    ldr x0, [sp], #16
    ldr x1, [sp], #16
    sxtw x1, w1
    lsl x1, x1, #2
    sub x9, x29, #16
    mov x2, x9
    str w0, [x2, x1]
    movz x0, #1
    str x0, [sp, #-16]!
    movz x0, #22
    str x0, [sp, #-16]!
    ldr x0, [sp], #16
    ldr x1, [sp], #16
    sxtw x1, w1
    lsl x1, x1, #2
    sub x9, x29, #16
    mov x2, x9
    str w0, [x2, x1]
    movz x0, #2
    str x0, [sp, #-16]!
    movz x0, #99
    str x0, [sp, #-16]!
    ldr x0, [sp], #16
    ldr x1, [sp], #16
    sxtw x1, w1
    lsl x1, x1, #2
    sub x9, x29, #16
    mov x2, x9
    str w0, [x2, x1]
    movz x0, #3
    str x0, [sp, #-16]!
    movz x0, #44
    str x0, [sp, #-16]!
    ldr x0, [sp], #16
    ldr x1, [sp], #16
    sxtw x1, w1
    lsl x1, x1, #2
    sub x9, x29, #16
    mov x2, x9
    str w0, [x2, x1]
    sub x9, x29, #16
    mov x0, x9
    str x0, [sp, #-16]!
    ldr x0, [sp], #16
    str x0, [x29, #-24]
    ldr x0, [x29, #-24]
    str x0, [sp, #-16]!
    movz x0, #2
    str x0, [sp, #-16]!
    ldr x0, [sp], #16
    ldr x1, [sp], #16
    sxtw x0, w0
    lsl x0, x0, #2
    add x0, x1, x0
    str x0, [sp, #-16]!
    ldr x0, [sp], #16
    ldrsw x0, [x0]
    str x0, [sp, #-16]!
    ldr x0, [sp], #16
    mov sp, x29
    ldp x29, x30, [sp], #16
    ret
    movz x0, #0
    mov sp, x29
    ldp x29, x30, [sp], #16
    ret
