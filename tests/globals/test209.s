.text
.data
    .align 4
.global _arr
_arr:
    .byte 1
    .byte 0
    .byte 0
    .byte 0
    .byte 41
    .byte 0
    .byte 0
    .byte 0
.text
.align 2
.global _main
_main:
    stp x29, x30, [sp, #-16]!
    mov x29, sp
    sub sp, sp, #0
    movz x0, #0
    str x0, [sp, #-16]!
    adrp x0, _arr@PAGE
    add  x0, x0, _arr@PAGEOFF
    mov x1, x0
    ldr x0, [sp], #16
    sxtw x0, w0
    add x0, x1, x0
    str x0, [sp, #-16]!
    ldr x0, [sp], #16
    ldrb w0, [x0]
    str x0, [sp, #-16]!
    movz x0, #4
    str x0, [sp, #-16]!
    adrp x0, _arr@PAGE
    add  x0, x0, _arr@PAGEOFF
    mov x1, x0
    ldr x0, [sp], #16
    sxtw x0, w0
    add x0, x1, x0
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
