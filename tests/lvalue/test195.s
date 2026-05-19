.text
.align 2
.global _main
_main:
    stp x29, x30, [sp, #-16]!
    mov x29, sp
    sub sp, sp, #32
    movz x0, #8
    str x0, [sp, #-16]!
    ldr x0, [sp], #16
    str w0, [x29, #-4]
    sub x9, x29, #8
    mov x0, x9
    str x0, [sp, #-16]!
    ldr x0, [sp], #16
    str x0, [x29, #-16]
    ldr x0, [x29, #-16]
    str x0, [sp, #-16]!
    ldr x0, [sp], #16
    add x0, x0, #4
    str x0, [sp, #-16]!
    mov x2, x0
    ldr x0, [sp], #16
    ldr w0, [x2]
    str x0, [sp, #-16]!
    movz x0, #65535
    movk x0, #65535, lsl #16
    movk x0, #65535, lsl #32
    movk x0, #65535, lsl #48
    str x0, [sp, #-16]!
    ldr x0, [sp], #16
    ldr x1, [sp], #16
    add x0, x1, x0
    str x0, [sp, #-16]!
    str w0, [x2]
    ldr x0, [x29, #-16]
    str x0, [sp, #-16]!
    ldr x0, [sp], #16
    add x0, x0, #4
    str x0, [sp, #-16]!
    ldr x0, [sp], #16
    ldrsw x0, [x0]
    str x0, [sp, #-16]!
    ldr x0, [sp], #16
    str w0, [x29, #-20]
    ldrsw x0, [x29, #-20]
    str x0, [sp, #-16]!
    movz x0, #10
    str x0, [sp, #-16]!
    ldr x0, [sp], #16
    ldr x1, [sp], #16
    mul x0, x1, x0
    str x0, [sp, #-16]!
    ldrsw x0, [x29, #-4]
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
