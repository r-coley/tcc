.text
.align 2
.global _main
_main:
    stp x29, x30, [sp, #-16]!
    mov x29, sp
    sub sp, sp, #16
    movz x0, #0
    str x0, [sp, #-16]!
    movz x0, #3
    ldr x1, [sp], #16
    sxtw x1, w1
    lsl x1, x1, #2
    sub x9, x29, #4
    mov x2, x9
    str w0, [x2, x1]
    movz x0, #0
    sxtw x0, w0
    lsl x0, x0, #2
    sub x9, x29, #4
    mov x2, x9
    add x0, x2, x0
    str x0, [sp, #-16]!
    ldrsw x0, [x0]
    mov x2, x0
    mov x1, x0
    movz x0, #1
    add x0, x1, x0
    ldr x1, [sp], #16
    str w0, [x1]
    mov x0, x2
    str w0, [x29, #-8]
    ldrsw x0, [x29, #-8]
    mov x1, x0
    movz x0, #10
    mul x0, x1, x0
    mov x1, x0
    movz x0, #0
    sxtw x0, w0
    lsl x0, x0, #2
    sub x9, x29, #4
    mov x2, x9
    ldrsw x0, [x2, x0]
    add x0, x1, x0
    b L1
    movz x0, #0
L1:
    mov sp, x29
    ldp x29, x30, [sp], #16
    ret
