#ifndef TCC_STUB_STDARG_H
#define TCC_STUB_STDARG_H

/*
 * If the system stdarg.h has already been included (it defines _VA_LIST
 * or __va_list__ or similar guard), or if we're not compiling under TCC,
 * skip the stub entirely so we don't conflict with the system definition.
 */
#if defined(__TCC__) && !defined(_VA_LIST) && !defined(_VA_LIST_DEFINED) && \
    !defined(__va_list__) && !defined(_VA_LIST_T_H) && !defined(__VA_LIST__) && \
    !defined(__DARWIN_VA_LIST_DEFINED) && !defined(_ANSI_STDARG_H_) && \
    !defined(_STDARG_H)
/*
 * Bootstrap stdarg implementation for tcc-compiled arm64 code.
 *
 * Scope:
 *   This header is intentionally a small bootstrap subset used by this
 *   compiler's current arm64 self-hosting/tests.  It is not a complete
 *   platform ABI stdarg implementation and should not be treated as a
 *   drop-in replacement for the system <stdarg.h>.
 *
 * Supported today:
 *   - tcc-generated arm64 variadic callees
 *   - integer/pointer-like varargs that fit in the compiler's 8-byte
 *     vararg slots
 *   - arm64 floating-point unnamed arguments after default promotions
 *     (float is passed as double)
 *   - the current tcc call lowering where unnamed arguments are packed
 *     onto the caller stack before the branch-and-link
 *
 * Known omissions / future work:
 *   - struct/union or aggregate variadic arguments
 *   - over-aligned variadic arguments
 *   - target-specific va_list layouts for x64, x86, and mips
 *   - full ABI compatibility with the host platform's libc headers
 *
 * tcc's arm64 calling convention for this bootstrap subset:
 *   - Named params go in x0..x(N-1), stored to [x29-8]..[x29-8N]
 *   - Variadic args are pushed on the stack at the call site before bl,
 *     packed in 8-byte slots: [sp]=first_vararg, [sp+8]=second_vararg, ...
 *   - The callee prologue: stp x29,x30,[sp-16]!; mov x29,sp
 *     making x29 = caller_sp - 16, so first_vararg = [x29+16].
 *
 * __tcc_va_base() is a helper that reads the variadic function's x29
 * (our caller's saved x29, stored at [__tcc_va_base's x29]) and returns
 * that value + 16.
 *
 * __tcc_va_base frame:
 *   stp x29, x30, [sp-16]!   <-- saves variadic fn's x29 at [new_x29]
 *   mov x29, sp
 *   ldr x0, [x29]            --> reads variadic fn's x29
 *   add x0, x0, #16          --> variadic fn's x29 + 16 = first vararg
 */
static char *__tcc_va_base(void)
{
#if defined(__x86_64__)
    /*
     * tcc's current x64 variadic-call lowering leaves unnamed arguments
     * packed in 8-byte stack slots at the call site.  A call pushes the
     * return address and the callee prologue pushes rbp, so the first
     * unnamed argument is at the variadic callee's rbp + 16.  This helper's
     * own [rbp] slot contains the caller/variadic function's rbp.
     */
#if defined(__TCC_ASM_ATT__)
    asm volatile ("movq (%rbp), %rax");
    asm volatile ("addq $16, %rax");
#else
    asm volatile ("mov rax, QWORD PTR [rbp]");
    asm volatile ("add rax, 16");
#endif
#elif defined(__mips__)
    /*
     * MIPS backend prologue saves the caller/variadic function's frame
     * pointer at 0($fp) in this helper's frame.  The current compile-only
     * MIPS varargs model treats unnamed arguments as 8-byte stack slots
     * starting at the variadic function's frame pointer + 16.
     */
    asm volatile ("lw $v0, 0($fp)");
    asm volatile ("addiu $v0, $v0, 16");
#else
    asm volatile ("ldr x0, [x29]");   /* variadic fn's fp (our caller's saved x29) */
    asm volatile ("add x0, x0, #16"); /* fp + 16 = caller's sp = first vararg       */
#endif
}

typedef char *va_list;
#define _VA_LIST_DEFINED
#define _VA_LIST

#if defined(__x86_64__)
/*
 * x86-64 (SysV): variadic arguments arrive in the integer registers, which
 * the callee prologue spills into a register save area. __builtin_va_start
 * yields the address of the first variadic slot in that area, so we do not
 * use the __tcc_va_base stack helper here.
 */
#define va_start(ap, last)  ((ap) = __builtin_va_start((ap), (last)))
#else
#define va_start(ap, last)  ((ap) = __tcc_va_base())
#endif
#define va_arg(ap, type)    (*(type *)((ap) += 8, (ap) - 8))
#define va_end(ap)          ((void)0)

#endif /* __TCC__ && !defined(_VA_LIST) ... */

#endif /* TCC_STUB_STDARG_H */
