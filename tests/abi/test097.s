.text
.section __TEXT,__cstring,cstring_literals
Lstr1:
    .asciz "%d"
.text
.align 2
.global _main
_main:
    stp x29, x30, [sp, #-16]!
    mov x29, sp
    sub sp, sp, #32
    movz x0, #42
    str x0, [sp, #-16]!
    adrp x0, Lstr1@PAGE
    add  x0, x0, Lstr1@PAGEOFF
    str x0, [sp, #-16]!
    sub x9, x29, #32
    mov x0, x9
    str x0, [sp, #-16]!
    ldr x0, [sp, #0]
    ldr x1, [sp, #16]
    add sp, sp, #32
    ldr x9, [sp, #0]
    str x9, [sp, #0]
    bl _sprintf
    add sp, sp, #16
    movz x0, #0
    str x0, [sp, #-16]!
    ldr x0, [sp], #16
    sxtw x0, w0
    sub x9, x29, #32
    mov x2, x9
    ldrb w0, [x2, x0]
    str x0, [sp, #-16]!
    movz x0, #48
    str x0, [sp, #-16]!
    ldr x0, [sp], #16
    ldr x1, [sp], #16
    sub x0, x1, x0
    str x0, [sp, #-16]!
    movz x0, #10
    str x0, [sp, #-16]!
    ldr x0, [sp], #16
    ldr x1, [sp], #16
    mul x0, x1, x0
    str x0, [sp, #-16]!
    movz x0, #1
    str x0, [sp, #-16]!
    ldr x0, [sp], #16
    sxtw x0, w0
    sub x9, x29, #32
    mov x2, x9
    ldrb w0, [x2, x0]
    str x0, [sp, #-16]!
    movz x0, #48
    str x0, [sp, #-16]!
    ldr x0, [sp], #16
    ldr x1, [sp], #16
    sub x0, x1, x0
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
