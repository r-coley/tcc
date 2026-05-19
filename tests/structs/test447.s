.text
.data
    .align 4
_entries:
    .zero 144
.text
.section __TEXT,__cstring,cstring_literals
Lstr2:
    .asciz "world"
.text
.section __TEXT,__cstring,cstring_literals
Lstr1:
    .asciz "hello"
.text
.align 2
.global _set_name
_set_name:
    stp x29, x30, [sp, #-16]!
    mov x29, sp
    sub sp, sp, #16
    str x0, [x29, #-8]
    str x1, [x29, #-16]
    movz x0, #31
    str x0, [sp, #-16]!
    ldr x0, [x29, #-16]
    str x0, [sp, #-16]!
    ldr x0, [x29, #-8]
    str x0, [sp, #-16]!
    ldr x0, [sp], #16
    ldrb w0, [x0]
    str x0, [sp, #-16]!
    ldr x0, [sp, #0]
    ldr x1, [sp, #16]
    ldr x2, [sp, #32]
    add sp, sp, #48
    bl _strncpy
    mov sp, x29
    ldp x29, x30, [sp], #16
    ret
.align 2
.global _main
_main:
    stp x29, x30, [sp, #-16]!
    mov x29, sp
    sub sp, sp, #0
    adrp x0, Lstr1@PAGE
    add  x0, x0, Lstr1@PAGEOFF
    str x0, [sp, #-16]!
    movz x0, #0
    str x0, [sp, #-16]!
    adrp x0, _entries@PAGE
    add  x0, x0, _entries@PAGEOFF
    mov x1, x0
    ldr x0, [sp], #16
    sxtw x0, w0
    movz x2, #36
    mul x0, x0, x2
    add x0, x1, x0
    str x0, [sp, #-16]!
    ldr x0, [sp, #0]
    ldr x1, [sp, #16]
    add sp, sp, #32
    bl _set_name
    adrp x0, Lstr2@PAGE
    add  x0, x0, Lstr2@PAGEOFF
    str x0, [sp, #-16]!
    movz x0, #1
    str x0, [sp, #-16]!
    adrp x0, _entries@PAGE
    add  x0, x0, _entries@PAGEOFF
    mov x1, x0
    ldr x0, [sp], #16
    sxtw x0, w0
    movz x2, #36
    mul x0, x0, x2
    add x0, x1, x0
    str x0, [sp, #-16]!
    ldr x0, [sp, #0]
    ldr x1, [sp, #16]
    add sp, sp, #32
    bl _set_name
    movz x0, #1
    str x0, [sp, #-16]!
    ldr x0, [sp], #16
    sxtw x0, w0
    adrp x2, _entries@PAGE
    add x2, x2, _entries@PAGEOFF
    ldrb w0, [x2, x0]
    str x0, [sp, #-16]!
    ldr x0, [sp, #0]
    add sp, sp, #16
    bl _strlen
    str x0, [sp, #-16]!
    ldr x0, [sp], #16
    mov sp, x29
    ldp x29, x30, [sp], #16
    ret
    movz x0, #0
    mov sp, x29
    ldp x29, x30, [sp], #16
    ret
