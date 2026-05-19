.text
.data
    .align 4
.global _arr
_arr:
    .zero 16
.text
.align 2
.global _set
_set:
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
    adrp x2, _arr@PAGE
    add x2, x2, _arr@PAGEOFF
    str w0, [x2, x1]
    movz x0, #0
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
    movz x0, #42
    str x0, [sp, #-16]!
    movz x0, #2
    str x0, [sp, #-16]!
    ldr x0, [sp, #0]
    ldr x1, [sp, #16]
    add sp, sp, #32
    bl _set
    movz x0, #2
    str x0, [sp, #-16]!
    ldr x0, [sp], #16
    sxtw x0, w0
    lsl x0, x0, #2
    adrp x2, _arr@PAGE
    add x2, x2, _arr@PAGEOFF
    ldrsw x0, [x2, x0]
    str x0, [sp, #-16]!
    ldr x0, [sp], #16
    mov sp, x29
    ldp x29, x30, [sp], #16
    ret
    movz x0, #0
    mov sp, x29
    ldp x29, x30, [sp], #16
    ret
