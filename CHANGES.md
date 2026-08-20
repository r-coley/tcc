# tcc x64 review — change set

Developed/validated on a Linux x86_64 host. Confirmed green on the arm64/macOS
primary target (`make stage2`: stage0→stage1→stage2, stage1==stage2 equality,
cross-target asm PASS arm64+x64). x64 stage0 suite went from 100 failures to 16;
the remaining 16 are all pre-existing (x64 floating-point codegen unimplemented,
plus a couple of target-specific preproc/debug tests). Zero regressions.

Apply by extracting over the source root so the `cc/` tree overlays in place.

## Changed files
- cc/codegen/x64.c
- cc/codegen/codegen.h
- cc/codegen/mips.c
- cc/ir.c
- cc/ir.h
- cc/expr.c
- cc/preprocess.c
- cc/include/stdarg.h
- cc/parser.c

## Fixes

1. **x64 struct-by-value copy (x64.c).** `x64_ptr_copy` was a silent no-op, so
   struct assignment emitted nothing. Implemented a real 8/4/2/1-byte descending
   copy (src=rax, dst=r11). `x64_copy_local` given an exact-size tail instead of
   over-copying in 4-byte steps.

2. **x64 sized member/indexed access (x64.c).** `load/store_member_ptr` and
   `load/store_indexed` ignored element size (always DWORD). Now honor 1/2/4/8
   with correct sign-extension and index scaling.

3. **frf frame under-allocation (parser.c, 3 sites).** Function-returning-
   function-pointer definitions placed params at `-(n+1)*8` slots but grew
   `stack_size` only 4 bytes each, so the prologue under-reserved and the first
   accumulator spill clobbered the last parameter (segfault). Fixed by raising
   `stack_size` to at least `frf_param_n*8`. This is the only edit in shared
   parser code; it only grows an under-allocated frame, confirmed safe on arm64.

4. **x64 stack alignment (x64.c).** The frame wasn't rounded to 16 bytes, so an
   odd frame (e.g. `char[10]` -> 12) left `rsp` off the 16-byte grid and crashed
   inside libc at the next call. Now rounds `(size+15)&~15`.

5. **x64 variadic ABI (x64.c, codegen.h, ir.c/ir.h, expr.c, preprocess.c,
   include/stdarg.h) — x86_64-gated.** tcc's variadic callees used an all-on-
   stack va_list (`__tcc_va_base = [rbp]+16`), but SysV glibc callees expect
   variadic args in registers, so no single caller convention satisfied both.
   Implemented a proper SysV register save area: a variadic x64 callee prologue
   appends a 48-byte area below the frame and spills rdi,rsi,rdx,rcx,r8,r9 in
   ascending order; a new `__builtin_va_start` intrinsic (modeled on the existing
   `__builtin_stack_save`) leaves the address of the first variadic slot in the
   accumulator; the caller now passes the first six integer args in registers
   (`reg_limit = 6`). The `va_start` macro branches on `#if defined(__x86_64__)`,
   so arm64/mips/m68k keep their existing stack model untouched (verified: arm64
   still emits `__tcc_va_base`, no save-area leakage). Known limit: >6 integer
   args into a tcc-compiled x64 variadic callee is not yet handled (tcc's own
   variadics are all <=6).

6. **mips warning (mips.c).** Appending `emit_va_start` to the `Codegen` struct
   tripped `-Wmissing-field-initializers` only on mips, which uses positional
   initializers with trailing bare `NULL`s. Made the final terminator explicit.

7. **Preprocessor heap overrun (preprocess.c).** Self-compiling the two largest
   TUs (parser.c, stmt.c) aborted with glibc "malloc(): invalid size".
   `append_physical_line` (line-continuation splicing) reallocs the growable line
   buffer, but the caller only resynced `line_buf`/`line_buf_cap` when realloc
   *changed the pointer*. On an in-place realloc the sync was skipped, leaving
   `line_buf_cap` stale-small while the buffer was larger; a later fill then
   overran the block into glibc's chunk metadata. Fixed by having
   `append_physical_line` return the true allocated capacity and having the caller
   always resync both the pointer and the capacity. (This overrun was also masking
   the x64 FP wall on those TUs.)
