.text
.align 2
.global _main
_main:
    stp x29, x30, [sp, #-16]!
    mov x29, sp
    sub sp, sp, #16
    movz x0, #0
    str x0, [sp, #-16]!
    movz x0, #65
    str x0, [sp, #-16]!
    ldr x0, [sp], #16
    ldr x1, [sp], #16
    sxtw x1, w1
    sub x9, x29, #4
    mov x2, x9
    strb w0, [x2, x1]
    movz x0, #1
    str x0, [sp, #-16]!
    movz x0, #66
    str x0, [sp, #-16]!
    ldr x0, [sp], #16
    ldr x1, [sp], #16
    sxtw x1, w1
    sub x9, x29, #4
    mov x2, x9
    strb w0, [x2, x1]
    movz x0, #2
    str x0, [sp, #-16]!
    movz x0, #67
    str x0, [sp, #-16]!
    ldr x0, [sp], #16
    ldr x1, [sp], #16
    sxtw x1, w1
    sub x9, x29, #4
    mov x2, x9
    strb w0, [x2, x1]
    movz x0, #3
    str x0, [sp, #-16]!
    movz x0, #68
    str x0, [sp, #-16]!
    ldr x0, [sp], #16
    ldr x1, [sp], #16
    sxtw x1, w1
    sub x9, x29, #4
    mov x2, x9
    strb w0, [x2, x1]
    sub x9, x29, #4
    mov x0, x9
    str x0, [sp, #-16]!
    ldr x0, [sp], #16
    str x0, [x29, #-12]
    ldr x0, [x29, #-12]
    str x0, [sp, #-16]!
    movz x0, #2
    str x0, [sp, #-16]!
    ldr x0, [sp], #16
    ldr x1, [sp], #16
    sxtw x0, w0
    add x0, x1, x0
    str x0, [sp, #-16]!
    ldr x0, [sp], #16
    ldrb w0, [x0]
    str x0, [sp, #-16]!
    ldr x0, [sp], #16
    mov sp, x29
    ldp x29, x30, [sp], #16
    ret
    movz x0, #0
    mov sp, x29
    ldp x29, x30, [sp], #16
    ret
