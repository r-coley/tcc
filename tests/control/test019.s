.text
.align 2
.global _main
_main:
    stp x29, x30, [sp, #-16]!
    mov x29, sp
    sub sp, sp, #16
    movz x0, #11
    str x0, [sp, #-16]!
    ldr x0, [sp], #16
    str w0, [x29, #-4]
    movz x0, #77
    str x0, [sp, #-16]!
    ldr x0, [sp], #16
    str w0, [x29, #-8]
    sub x9, x29, #8
    mov x0, x9
    str x0, [sp, #-16]!
    ldr x0, [sp], #16
    str x0, [x29, #-16]
    ldr x0, [x29, #-16]
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
