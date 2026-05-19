.text
.section __TEXT,__cstring,cstring_literals
Lstr1:
    .asciz "hello"
.text
.align 2
.global _main
_main:
    stp x29, x30, [sp, #-16]!
    mov x29, sp
    sub sp, sp, #16
    adrp x0, Lstr1@PAGE
    add  x0, x0, Lstr1@PAGEOFF
    str x0, [sp, #-16]!
    ldr x0, [sp], #16
    str x0, [x29, #-8]
    movz x0, #1
    str x0, [sp, #-16]!
    ldr x0, [sp], #16
    mov sp, x29
    ldp x29, x30, [sp], #16
    ret
    movz x0, #0
    mov sp, x29
    ldp x29, x30, [sp], #16
    ret
