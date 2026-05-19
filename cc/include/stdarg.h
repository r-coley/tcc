#ifndef TCC_STUB_STDARG_H
#define TCC_STUB_STDARG_H

#ifdef __TCC__
/*
 * stdarg implementation for tcc-compiled arm64 code (stage1 self-host).
 *
 * tcc's arm64 ABI packs variadic arguments on the stack in 8-byte slots
 * immediately above the callee's saved fp/lr pair:
 *
 *   [x29 + 16]  = first variadic arg
 *   [x29 + 24]  = second variadic arg
 *   ...
 *
 * Named (fixed) parameters arrive in registers (x0..x7) and are stored by
 * the callee to negative-offset locals; they do NOT occupy the vararg area.
 *
 * __tcc_va_base() is called from inside the variadic function.  Its own
 * prologue saves x29/x30 and sets x29 = sp, so [x29] holds the *caller's*
 * saved x29 — i.e. the variadic function's frame pointer.  Adding 16 gives
 * the address of the first variadic argument in that frame.
 *
 * va_arg advances the pointer by 8 bytes per argument (all arguments are
 * promoted to 8-byte slots by tcc's caller-side packing).
 */
static char *__tcc_va_base(void)
{
    asm volatile ("ldr x0, [x29]");   /* load caller's (variadic fn's) saved fp */
    asm volatile ("add x0, x0, #16"); /* first vararg = caller's fp + 16        */
}

typedef char *va_list;

#define va_start(ap, last)  ((ap) = __tcc_va_base())
#define va_arg(ap, type)    (*(type *)((ap) = (ap) + 8, (ap) - 8))
#define va_end(ap)          ((void)0)

#else
/* Host compiler: use the compiler's own built-ins. */
typedef __builtin_va_list va_list;
#define va_start(ap, last)  __builtin_va_start(ap, last)
#define va_arg(ap, type)    __builtin_va_arg(ap, type)
#define va_end(ap)          __builtin_va_end(ap)
#endif

#endif /* TCC_STUB_STDARG_H */
