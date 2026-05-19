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
    movz x0, #0
    str x0, [sp, #-16]!
    ldr x0, [sp], #16
    str x0, [x29, #-8]
    ldr x0, [x29, #-8]
    str x0, [sp, #-16]!
    ldr x0, [sp], #16
    cmp w0, #0
    b.eq L3
    adrp x0, Lstr1@PAGE
    add  x0, x0, Lstr1@PAGEOFF
    str x0, [sp, #-16]!
    ldr x0, [x29, #-8]
    str x0, [sp, #-16]!
    ldr x0, [sp, #0]
    ldr x1, [sp, #16]
    add sp, sp, #32
    bl _strcmp
    str x0, [sp, #-16]!
    movz x0, #0
    str x0, [sp, #-16]!
    ldr x0, [sp], #16
    ldr x1, [sp], #16
    cmp w1, w0
    cset w0, eq
    str x0, [sp, #-16]!
    ldr x0, [sp], #16
    cmp w0, #0
    b.eq L3
    movz x0, #1
    str x0, [sp, #-16]!
    b L4
L3:
    movz x0, #0
    str x0, [sp, #-16]!
L4:
    ldr x0, [sp], #16
    cmp w0, #0
    b.eq L1
    movz x0, #1
    str x0, [sp, #-16]!
    ldr x0, [sp], #16
    mov sp, x29
    ldp x29, x30, [sp], #16
    ret
    b L2
L1:
L2:
    movz x0, #42
    str x0, [sp, #-16]!
    ldr x0, [sp], #16
    mov sp, x29
    ldp x29, x30, [sp], #16
    ret
    movz x0, #0
    mov sp, x29
    ldp x29, x30, [sp], #16
    ret
