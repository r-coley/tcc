.text
.data
    .align 8
.global _s
_s:
    .byte 0
    .byte 0
    .byte 0
    .byte 0
    .byte 42
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
    ldr x0, [sp], #16
    sxtw x0, w0
    lsl x0, x0, #2
    adrp x2, _s@PAGE
    add x2, x2, _s@PAGEOFF
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
