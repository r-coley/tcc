.text
.data
.global ___static_f_x
___static_f_x:
    .quad 0
.text
.align 2
.global _f
_f:
    stp x29, x30, [sp, #-16]!
    mov x29, sp
    sub sp, sp, #0
    adrp x1, ___static_f_x@PAGE
    add x1, x1, ___static_f_x@PAGEOFF
    ldrsw x0, [x1]
    str x0, [sp, #-16]!
    movz x0, #42
    str x0, [sp, #-16]!
    ldr x0, [sp], #16
    ldr x1, [sp], #16
    add x0, x1, x0
    str x0, [sp, #-16]!
    ldr x0, [sp], #16
    adrp x1, ___static_f_x@PAGE
    add x1, x1, ___static_f_x@PAGEOFF
    str w0, [x1]
    adrp x1, ___static_f_x@PAGE
    add x1, x1, ___static_f_x@PAGEOFF
    ldrsw x0, [x1]
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
