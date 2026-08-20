.text
.data
    .align 4
.global _s
_s:
    .byte 1, 0, 0, 0, 41, 0, 0, 0
.text
.align 2
___tcc_va_base:
    stp x29, x30, [sp, #-16]!
    mov x29, sp
    sub sp, sp, #0
    ldr x0, [x29]
    add x0, x0, #16
    mov sp, x29
    ldp x29, x30, [sp], #16
    ret
.align 2
.global _main
_main:
    stp x29, x30, [sp, #-16]!
    mov x29, sp
    sub sp, sp, #0
    movz x0, #0
    str x0, [sp, #-16]!
    adrp x0, _s@GOTPAGE
    ldr  x0, [x0, _s@GOTPAGEOFF]
    mov x1, x0
    ldr x0, [sp], #16
    sxtw x0, w0
    add x0, x1, x0
    str x0, [sp, #-16]!
    ldr x0, [sp], #16
    ldrb w0, [x0]
    str x0, [sp, #-16]!
    ldr x0, [sp], #16
    sxtb x0, w0
    str x0, [sp, #-16]!
    movz x0, #4
    str x0, [sp, #-16]!
    adrp x0, _s@GOTPAGE
    ldr  x0, [x0, _s@GOTPAGEOFF]
    mov x1, x0
    ldr x0, [sp], #16
    sxtw x0, w0
    add x0, x1, x0
    str x0, [sp, #-16]!
    ldr x0, [sp], #16
    ldrsw x0, [x0]
    str x0, [sp, #-16]!
    ldr x0, [sp], #16
    sxtw x0, w0
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
