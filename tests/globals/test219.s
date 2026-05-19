.text
.data
    .align 4
_points:
    .zero 32
.text
.align 2
.global _get_point
_get_point:
    stp x29, x30, [sp, #-16]!
    mov x29, sp
    sub sp, sp, #16
    str x0, [x29, #-8]
    ldrsw x0, [x29, #-8]
    str x0, [sp, #-16]!
    adrp x0, _points@PAGE
    add  x0, x0, _points@PAGEOFF
    mov x1, x0
    ldr x0, [sp], #16
    sxtw x0, w0
    movz x2, #8
    mul x0, x0, x2
    add x0, x1, x0
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
    sub sp, sp, #16
    movz x0, #0
    str x0, [sp, #-16]!
    adrp x0, _points@PAGE
    add  x0, x0, _points@PAGEOFF
    mov x1, x0
    ldr x0, [sp], #16
    sxtw x0, w0
    add x0, x1, x0
    str x0, [sp, #-16]!
    movz x0, #10
    str x0, [sp, #-16]!
    ldr x0, [sp], #16
    ldr x1, [sp], #16
    str w0, [x1]
    movz x0, #4
    str x0, [sp, #-16]!
    adrp x0, _points@PAGE
    add  x0, x0, _points@PAGEOFF
    mov x1, x0
    ldr x0, [sp], #16
    sxtw x0, w0
    add x0, x1, x0
    str x0, [sp, #-16]!
    movz x0, #11
    str x0, [sp, #-16]!
    ldr x0, [sp], #16
    ldr x1, [sp], #16
    str w0, [x1]
    movz x0, #8
    str x0, [sp, #-16]!
    adrp x0, _points@PAGE
    add  x0, x0, _points@PAGEOFF
    mov x1, x0
    ldr x0, [sp], #16
    sxtw x0, w0
    add x0, x1, x0
    str x0, [sp, #-16]!
    movz x0, #12
    str x0, [sp, #-16]!
    ldr x0, [sp], #16
    ldr x1, [sp], #16
    str w0, [x1]
    movz x0, #12
    str x0, [sp, #-16]!
    adrp x0, _points@PAGE
    add  x0, x0, _points@PAGEOFF
    mov x1, x0
    ldr x0, [sp], #16
    sxtw x0, w0
    add x0, x1, x0
    str x0, [sp, #-16]!
    movz x0, #13
    str x0, [sp, #-16]!
    ldr x0, [sp], #16
    ldr x1, [sp], #16
    str w0, [x1]
    movz x0, #16
    str x0, [sp, #-16]!
    adrp x0, _points@PAGE
    add  x0, x0, _points@PAGEOFF
    mov x1, x0
    ldr x0, [sp], #16
    sxtw x0, w0
    add x0, x1, x0
    str x0, [sp, #-16]!
    movz x0, #20
    str x0, [sp, #-16]!
    ldr x0, [sp], #16
    ldr x1, [sp], #16
    str w0, [x1]
    movz x0, #20
    str x0, [sp, #-16]!
    adrp x0, _points@PAGE
    add  x0, x0, _points@PAGEOFF
    mov x1, x0
    ldr x0, [sp], #16
    sxtw x0, w0
    add x0, x1, x0
    str x0, [sp, #-16]!
    movz x0, #22
    str x0, [sp, #-16]!
    ldr x0, [sp], #16
    ldr x1, [sp], #16
    str w0, [x1]
    movz x0, #2
    str x0, [sp, #-16]!
    ldr x0, [sp, #0]
    add sp, sp, #16
    bl _get_point
    str x0, [sp, #-16]!
    ldr x0, [sp], #16
    str x0, [x29, #-8]
    ldr x0, [x29, #-8]
    str x0, [sp, #-16]!
    ldr x0, [sp], #16
    ldrsw x0, [x0]
    str x0, [sp, #-16]!
    ldr x0, [x29, #-8]
    str x0, [sp, #-16]!
    ldr x0, [sp], #16
    add x0, x0, #4
    str x0, [sp, #-16]!
    ldr x0, [sp], #16
    ldrsw x0, [x0]
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
