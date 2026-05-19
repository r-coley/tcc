.text
.align 2
.global _main
_main:
    stp x29, x30, [sp, #-16]!
    mov x29, sp
    sub sp, sp, #48
    movz x0, #0
    str x0, [sp, #-16]!
    ldr x0, [sp], #16
    str w0, [x29, #-32]
L1:
    ldrsw x0, [x29, #-32]
    str x0, [sp, #-16]!
    movz x0, #5
    str x0, [sp, #-16]!
    ldr x0, [sp], #16
    ldr x1, [sp], #16
    cmp w1, w0
    cset w0, lt
    str x0, [sp, #-16]!
    ldr x0, [sp], #16
    cmp w0, #0
    b.eq L2
    ldrsw x0, [x29, #-32]
    str x0, [sp, #-16]!
    ldrsw x0, [x29, #-32]
    str x0, [sp, #-16]!
    movz x0, #1
    str x0, [sp, #-16]!
    ldr x0, [sp], #16
    ldr x1, [sp], #16
    add x0, x1, x0
    str x0, [sp, #-16]!
    ldr x0, [sp], #16
    ldr x1, [sp], #16
    sxtw x1, w1
    lsl x1, x1, #2
    sub x9, x29, #20
    mov x2, x9
    str w0, [x2, x1]
    ldrsw x0, [x29, #-32]
    str x0, [sp, #-16]!
    movz x0, #1
    str x0, [sp, #-16]!
    ldr x0, [sp], #16
    ldr x1, [sp], #16
    add x0, x1, x0
    str x0, [sp, #-16]!
    ldr x0, [sp], #16
    str w0, [x29, #-32]
    b L1
L2:
    sub x9, x29, #20
    mov x0, x9
    str x0, [sp, #-16]!
    ldr x0, [sp], #16
    str x0, [x29, #-28]
    movz x0, #0
    str x0, [sp, #-16]!
    ldr x0, [sp], #16
    str w0, [x29, #-32]
    movz x0, #0
    str x0, [sp, #-16]!
    ldr x0, [sp], #16
    str w0, [x29, #-36]
L3:
    ldrsw x0, [x29, #-32]
    str x0, [sp, #-16]!
    movz x0, #5
    str x0, [sp, #-16]!
    ldr x0, [sp], #16
    ldr x1, [sp], #16
    cmp w1, w0
    cset w0, lt
    str x0, [sp, #-16]!
    ldr x0, [sp], #16
    cmp w0, #0
    b.eq L4
    ldrsw x0, [x29, #-36]
    str x0, [sp, #-16]!
    ldr x0, [x29, #-28]
    str x0, [sp, #-16]!
    ldrsw x0, [x29, #-32]
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
    ldr x1, [sp], #16
    add x0, x1, x0
    str x0, [sp, #-16]!
    ldr x0, [sp], #16
    str w0, [x29, #-36]
    ldrsw x0, [x29, #-32]
    str x0, [sp, #-16]!
    movz x0, #1
    str x0, [sp, #-16]!
    ldr x0, [sp], #16
    ldr x1, [sp], #16
    add x0, x1, x0
    str x0, [sp, #-16]!
    ldr x0, [sp], #16
    str w0, [x29, #-32]
    b L3
L4:
    ldrsw x0, [x29, #-36]
    str x0, [sp, #-16]!
    ldr x0, [sp], #16
    mov sp, x29
    ldp x29, x30, [sp], #16
    ret
    movz x0, #0
    mov sp, x29
    ldp x29, x30, [sp], #16
    ret
