.text
.align 2
.global _main
_main:
    stp x29, x30, [sp, #-16]!
    mov x29, sp
    sub sp, sp, #16
    movz x0, #0
    str x0, [sp, #-16]!
    ldr x0, [sp], #16
    str w0, [x29, #-4]
L1:
    ldrsw x0, [x29, #-4]
    str x0, [sp, #-16]!
    movz x0, #10
    str x0, [sp, #-16]!
    ldr x0, [sp], #16
    ldr x1, [sp], #16
    cmp w1, w0
    cset w0, lt
    str x0, [sp, #-16]!
    ldr x0, [sp], #16
    cmp w0, #0
    b.eq L2
    ldrsw x0, [x29, #-4]
    str x0, [sp, #-16]!
    movz x0, #4
    str x0, [sp, #-16]!
    ldr x0, [sp], #16
    ldr x1, [sp], #16
    cmp w1, w0
    cset w0, eq
    str x0, [sp, #-16]!
    ldr x0, [sp], #16
    cmp w0, #0
    b.eq L3
    b L2
    b L4
L3:
L4:
    ldrsw x0, [x29, #-4]
    str x0, [sp, #-16]!
    movz x0, #1
    str x0, [sp, #-16]!
    ldr x0, [sp], #16
    ldr x1, [sp], #16
    add x0, x1, x0
    str x0, [sp, #-16]!
    ldr x0, [sp], #16
    str w0, [x29, #-4]
    b L1
L2:
    ldrsw x0, [x29, #-4]
    str x0, [sp, #-16]!
    ldr x0, [sp], #16
    mov sp, x29
    ldp x29, x30, [sp], #16
    ret
    movz x0, #0
    mov sp, x29
    ldp x29, x30, [sp], #16
    ret
