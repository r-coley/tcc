.text
.data
    .align 4
_table:
    .zero 32
.text
.align 2
.global _table_set
_table_set:
    stp x29, x30, [sp, #-16]!
    mov x29, sp
    sub sp, sp, #16
    str x0, [x29, #-8]
    str x1, [x29, #-16]
    ldrsw x0, [x29, #-8]
    str x0, [sp, #-16]!
    ldrsw x0, [x29, #-16]
    str x0, [sp, #-16]!
    ldr x0, [sp], #16
    ldr x1, [sp], #16
    sxtw x1, w1
    lsl x1, x1, #2
    adrp x2, _table@PAGE
    add x2, x2, _table@PAGEOFF
    str w0, [x2, x1]
    mov sp, x29
    ldp x29, x30, [sp], #16
    ret
.align 2
.global _table_get
_table_get:
    stp x29, x30, [sp, #-16]!
    mov x29, sp
    sub sp, sp, #16
    str x0, [x29, #-8]
    ldrsw x0, [x29, #-8]
    str x0, [sp, #-16]!
    ldr x0, [sp], #16
    sxtw x0, w0
    lsl x0, x0, #2
    adrp x2, _table@PAGE
    add x2, x2, _table@PAGEOFF
    ldrsw x0, [x2, x0]
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
    movz x0, #21
    str x0, [sp, #-16]!
    movz x0, #0
    str x0, [sp, #-16]!
    ldr x0, [sp, #0]
    ldr x1, [sp, #16]
    add sp, sp, #32
    bl _table_set
    movz x0, #21
    str x0, [sp, #-16]!
    movz x0, #1
    str x0, [sp, #-16]!
    ldr x0, [sp, #0]
    ldr x1, [sp, #16]
    add sp, sp, #32
    bl _table_set
    movz x0, #0
    str x0, [sp, #-16]!
    ldr x0, [sp, #0]
    add sp, sp, #16
    bl _table_get
    str x0, [sp, #-16]!
    movz x0, #1
    str x0, [sp, #-16]!
    ldr x0, [sp, #0]
    add sp, sp, #16
    bl _table_get
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
