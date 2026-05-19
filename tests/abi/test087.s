.text
.align 2
.global _mix8
_mix8:
    stp x29, x30, [sp, #-16]!
    mov x29, sp
    sub sp, sp, #64
    str x0, [x29, #-8]
    str x1, [x29, #-16]
    str x2, [x29, #-24]
    str x3, [x29, #-32]
    str x4, [x29, #-40]
    str x5, [x29, #-48]
    str x6, [x29, #-56]
    str x7, [x29, #-64]
    ldrsw x0, [x29, #-64]
    str x0, [sp, #-16]!
    ldrsw x0, [x29, #-56]
    str x0, [sp, #-16]!
    ldr x0, [sp], #16
    ldr x1, [sp], #16
    sub x0, x1, x0
    str x0, [sp, #-16]!
    ldrsw x0, [x29, #-48]
    str x0, [sp, #-16]!
    ldr x0, [sp], #16
    ldr x1, [sp], #16
    add x0, x1, x0
    str x0, [sp, #-16]!
    ldrsw x0, [x29, #-40]
    str x0, [sp, #-16]!
    ldr x0, [sp], #16
    ldr x1, [sp], #16
    sub x0, x1, x0
    str x0, [sp, #-16]!
    ldrsw x0, [x29, #-32]
    str x0, [sp, #-16]!
    ldr x0, [sp], #16
    ldr x1, [sp], #16
    add x0, x1, x0
    str x0, [sp, #-16]!
    ldrsw x0, [x29, #-24]
    str x0, [sp, #-16]!
    ldr x0, [sp], #16
    ldr x1, [sp], #16
    sub x0, x1, x0
    str x0, [sp, #-16]!
    ldrsw x0, [x29, #-16]
    str x0, [sp, #-16]!
    ldr x0, [sp], #16
    ldr x1, [sp], #16
    add x0, x1, x0
    str x0, [sp, #-16]!
    ldrsw x0, [x29, #-8]
    str x0, [sp, #-16]!
    ldr x0, [sp], #16
    ldr x1, [sp], #16
    sub x0, x1, x0
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
    movz x0, #19
    str x0, [sp, #-16]!
    movz x0, #17
    str x0, [sp, #-16]!
    movz x0, #13
    str x0, [sp, #-16]!
    movz x0, #11
    str x0, [sp, #-16]!
    movz x0, #7
    str x0, [sp, #-16]!
    movz x0, #5
    str x0, [sp, #-16]!
    movz x0, #3
    str x0, [sp, #-16]!
    movz x0, #1
    str x0, [sp, #-16]!
    ldr x0, [sp, #0]
    ldr x1, [sp, #16]
    ldr x2, [sp, #32]
    ldr x3, [sp, #48]
    ldr x4, [sp, #64]
    ldr x5, [sp, #80]
    ldr x6, [sp, #96]
    ldr x7, [sp, #112]
    add sp, sp, #128
    bl _mix8
    str x0, [sp, #-16]!
    ldr x0, [sp], #16
    mov sp, x29
    ldp x29, x30, [sp], #16
    ret
    movz x0, #0
    mov sp, x29
    ldp x29, x30, [sp], #16
    ret
