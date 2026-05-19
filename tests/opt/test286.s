.text
.align 2
.global _add3
_add3:
    stp x29, x30, [sp, #-16]!
    mov x29, sp
    sub sp, sp, #32
    str x0, [x29, #-8]
    str x1, [x29, #-16]
    str x2, [x29, #-24]
    ldrsw x0, [x29, #-8]
    str x0, [sp, #-16]!
    ldrsw x0, [x29, #-16]
    str x0, [sp, #-16]!
    ldr x0, [sp], #16
    ldr x1, [sp], #16
    add x0, x1, x0
    str x0, [sp, #-16]!
    ldrsw x0, [x29, #-24]
    str x0, [sp, #-16]!
    ldr x0, [sp], #16
    ldr x1, [sp], #16
    add x0, x1, x0
    str x0, [sp, #-16]!
    ldr x0, [sp], #16
    mov sp, x29
    ldp x29, x30, [sp], #16
    ret
    mov sp, x29
    ldp x29, x30, [sp], #16
    ret
.align 2
.global _main
_main:
    stp x29, x30, [sp, #-16]!
    mov x29, sp
    sub sp, sp, #0
    movz x0, #12
    str x0, [sp, #-16]!
    movz x0, #20
    str x0, [sp, #-16]!
    movz x0, #10
    str x0, [sp, #-16]!
    ldr x0, [sp, #0]
    ldr x1, [sp, #16]
    ldr x2, [sp, #32]
    add sp, sp, #48
    bl _add3
    str x0, [sp, #-16]!
    ldr x0, [sp], #16
    mov sp, x29
    ldp x29, x30, [sp], #16
    ret
    movz x0, #0
    mov sp, x29
    ldp x29, x30, [sp], #16
    ret
