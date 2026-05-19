.text
.align 2
.global _main
_main:
    stp x29, x30, [sp, #-16]!
    mov x29, sp
    sub sp, sp, #16
    movz x0, #2
    str x0, [sp, #-16]!
    ldr x0, [sp], #16
    str w0, [x29, #-4]
    ldrsw x0, [x29, #-4]
    str x0, [sp, #-16]!
    movz x0, #1
    str x0, [sp, #-16]!
    ldr x0, [sp], #16
    ldr x1, [sp], #16
    cmp w1, w0
    cset w0, eq
    str x0, [sp, #-16]!
    ldr x0, [sp], #16
    cmp w0, #0
    b.ne L2
    ldrsw x0, [x29, #-4]
    str x0, [sp, #-16]!
    movz x0, #2
    str x0, [sp, #-16]!
    ldr x0, [sp], #16
    ldr x1, [sp], #16
    cmp w1, w0
    cset w0, eq
    str x0, [sp, #-16]!
    ldr x0, [sp], #16
    cmp w0, #0
    b.ne L3
    b L4
L2:
    movz x0, #10
    str x0, [sp, #-16]!
    ldr x0, [sp], #16
    mov sp, x29
    ldp x29, x30, [sp], #16
    ret
L3:
    movz x0, #42
    str x0, [sp, #-16]!
    ldr x0, [sp], #16
    mov sp, x29
    ldp x29, x30, [sp], #16
    ret
L4:
    movz x0, #0
    str x0, [sp, #-16]!
    ldr x0, [sp], #16
    mov sp, x29
    ldp x29, x30, [sp], #16
    ret
L1:
    movz x0, #0
    mov sp, x29
    ldp x29, x30, [sp], #16
    ret
