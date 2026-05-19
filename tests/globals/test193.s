.text
.data
.global _x
_x:
    .quad 42
    .align 8
.global _p
_p:
    .quad _x
.text
.align 2
.global _main
_main:
    stp x29, x30, [sp, #-16]!
    mov x29, sp
    sub sp, sp, #0
    adrp x1, _p@PAGE
    add x1, x1, _p@PAGEOFF
    ldr x0, [x1]
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
