.text
.data
.global _x
_x:
    .quad 40
.global _c
_c:
    .byte 2
.text
.align 2
.global _main
_main:
    stp x29, x30, [sp, #-16]!
    mov x29, sp
    sub sp, sp, #0
    adrp x1, _x@PAGE
    add x1, x1, _x@PAGEOFF
    ldrsw x0, [x1]
    str x0, [sp, #-16]!
    adrp x1, _c@PAGE
    add x1, x1, _c@PAGEOFF
    ldrb w0, [x1]
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
