.text
.align 2
.global _id_char
_id_char:
    stp x29, x30, [sp, #-16]!
    mov x29, sp
    sub sp, sp, #16
    str x0, [x29, #-8]
    ldrsw x0, [x29, #-8]
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
    movz x0, #42
    str x0, [sp, #-16]!
    ldr x0, [sp, #0]
    add sp, sp, #16
    bl _id_char
    str x0, [sp, #-16]!
    ldr x0, [sp], #16
    strb w0, [x29, #-4]
    ldrb w0, [x29, #-4]
    str x0, [sp, #-16]!
    ldr x0, [sp], #16
    mov sp, x29
    ldp x29, x30, [sp], #16
    ret
    movz x0, #0
    mov sp, x29
    ldp x29, x30, [sp], #16
    ret
