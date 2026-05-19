.text
.data
.global ___static_f_s
___static_f_s:
    .asciz "ABC"
.text
.align 2
.global _f
_f:
    stp x29, x30, [sp, #-16]!
    mov x29, sp
    sub sp, sp, #0
    movz x0, #0
    str x0, [sp, #-16]!
    ldr x0, [sp], #16
    sxtw x0, w0
    adrp x2, ___static_f_s@PAGE
    add x2, x2, ___static_f_s@PAGEOFF
    ldrb w0, [x2, x0]
    str x0, [sp, #-16]!
    movz x0, #23
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
    bl _f
    str x0, [sp, #-16]!
    ldr x0, [sp], #16
    mov sp, x29
    ldp x29, x30, [sp], #16
    ret
    movz x0, #0
    mov sp, x29
    ldp x29, x30, [sp], #16
    ret
