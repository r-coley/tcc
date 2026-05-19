.text
.data
.section __TEXT,__cstring,cstring_literals
Lstr1:
    .asciz "hello"
.text
.data
    .align 8
.global _s
_s:
    .quad Lstr1
.text
.align 2
.global _main
_main:
    stp x29, x30, [sp, #-16]!
    mov x29, sp
    sub sp, sp, #0
    adrp x1, _s@PAGE
    add x1, x1, _s@PAGEOFF
    ldr x0, [x1]
    str x0, [sp, #-16]!
    movz x0, #4
    str x0, [sp, #-16]!
    ldr x0, [sp], #16
    ldr x1, [sp], #16
    sxtw x0, w0
    add x0, x1, x0
    str x0, [sp, #-16]!
    ldr x0, [sp], #16
    ldrb w0, [x0]
    str x0, [sp, #-16]!
    ldr x0, [sp], #16
    mov sp, x29
    ldp x29, x30, [sp], #16
    ret
    movz x0, #0
    mov sp, x29
    ldp x29, x30, [sp], #16
    ret
