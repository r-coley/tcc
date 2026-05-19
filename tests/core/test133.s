.text
.data
.global _g
_g:
    .quad 0
.text
.align 2
.global _main
_main:
    stp x29, x30, [sp, #-16]!
    mov x29, sp
    sub sp, sp, #0
    movz x0, #40
    str x0, [sp, #-16]!
    ldr x0, [sp], #16
    adrp x1, _g@PAGE
    add x1, x1, _g@PAGEOFF
    str w0, [x1]
    adrp x1, _g@PAGE
    add x1, x1, _g@PAGEOFF
    ldrsw x0, [x1]
    str x0, [sp, #-16]!
    adrp x1, _g@PAGE
    add x1, x1, _g@PAGEOFF
    ldrsw x0, [x1]
    str x0, [sp, #-16]!
    movz x0, #1
    str x0, [sp, #-16]!
    ldr x0, [sp], #16
    ldr x1, [sp], #16
    add x0, x1, x0
    str x0, [sp, #-16]!
    ldr x0, [sp], #16
    adrp x1, _g@PAGE
    add x1, x1, _g@PAGEOFF
    str w0, [x1]
    ldr x0, [sp], #16
    adrp x1, _g@PAGE
    add x1, x1, _g@PAGEOFF
    ldrsw x0, [x1]
    str x0, [sp, #-16]!
    movz x0, #1
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
