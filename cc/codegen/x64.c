#include <stdio.h>
#include <string.h>

#include "tcc.h"
#include "target.h"
#include "codegen.h"
#include "parser.h"   /* func_fixed_params */

static int x64_frame_stack_size;
static int x64_internal_label_counter;
static int x64_call_stack_adjust;

/* Variadic register save area (SysV x86-64).
 *
 * A variadic callee must be able to recover its variable arguments, which
 * the caller passed in the integer registers rdi,rsi,rdx,rcx,r8,r9. We spill
 * all six into a 48-byte area appended below the ordinary frame (arg0 at the
 * lowest address, arg5 at the highest) and point va_start at the slot for the
 * first variadic argument. This is what lets tcc-compiled variadic functions
 * and glibc variadics share one SysV-correct calling convention.
 *
 * x64_va_fixed_params: fixed (named) param count for the current function,
 *                      or -1 when the current function is not variadic.
 * x64_va_start_offset: signed rbp-relative byte offset of the first variadic
 *                      slot, valid only while emitting a variadic function.
 */
#define X64_VA_SAVE_BYTES 48
static int x64_va_fixed_params = -1;
static int x64_va_start_offset = 0;

/* Debug info state for .loc/.file directives.
 *
 * This mirrors the current ARM64 implementation: the compiler emits
 * assembler directives and leaves actual DWARF line-table generation to
 * the system assembler.
 */
static int x64_debug_enabled = 0;
static const char *x64_debug_files[256];
static int x64_debug_file_count = 0;

static void x64_call(const char *name);
static void x64_call_saved(void);
static void x64_cleanup_call_args(int count, int fixed_params);

static const char *
x64_arg_reg(int index)
{
    switch (index) {
    case 0: return "rdi";
    case 1: return "rsi";
    case 2: return "rdx";
    case 3: return "rcx";
    case 4: return "r8";
    case 5: return "r9";
    default:
        ICE("invalid x64 argument register index");
        return "rdi";
    }
}

void x64_set_debug(int enabled) {
    x64_debug_enabled = enabled;
}

static int x64_get_file_index(const char *filename) {
    if (!filename || !filename[0])
        return 0;

    for (int i = 0; i < x64_debug_file_count; i++) {
        if (x64_debug_files[i] && strcmp(x64_debug_files[i], filename) == 0)
            return i + 1;
    }

    if (x64_debug_file_count < 255) {
        int idx = x64_debug_file_count + 1;
        x64_debug_files[x64_debug_file_count] = filename;
        x64_debug_file_count++;
        printf("    .file %d \"%s\"\n", idx, filename);
        return idx;
    }

    return 0;
}

static void x64_emit_source_loc(const char *file, int line) {
    if (!x64_debug_enabled || !file || line <= 0)
        return;

    int idx = x64_get_file_index(file);
    if (idx > 0)
        printf("    .loc %d %d 0\n", idx, line);
}


static void x64_preamble(void) {
    printf(".intel_syntax noprefix\n");
    printf(".text\n");
}

static void x64_func_start(const char *name, int is_static) {
    x64_frame_stack_size = 0;
    /* Determine whether this function is variadic so the prologue can build
     * a register save area. func_fixed_params returns -1 for non-variadic. */
    x64_va_fixed_params = func_fixed_params(name);
    x64_va_start_offset = 0;
    if (!is_static)
        printf(".global %s%s\n", TCC_ASM_SYM_PREFIX, name);
    printf("%s%s:\n", TCC_ASM_SYM_PREFIX, name);
    printf("    push rbp\n");
    printf("    mov rbp, rsp\n");
}

static void x64_func_end(void) {
    printf("    mov rsp, rbp\n");
    printf("    pop rbp\n");
    printf("    ret\n");
}

static void x64_stack_alloc(int size) {
    /* Round the reserved frame up to a 16-byte multiple. SysV requires rsp
     * be 16-byte aligned at each call; with push rbp already aligning rsp,
     * a 16-multiple frame keeps it aligned. It also makes the per-call
     * alignment padding (which divides frame_stack_size by 8) exact — an
     * odd frame like 12 (char[10]) would otherwise leave rsp off the 8-byte
     * grid entirely and corrupt every subsequent push/pop and call. */
    if (size > 0)
        size = (size + 15) & ~15;

    /* Variadic functions append a 48-byte GPR save area below the frame.
     * Locals occupy [rbp-size .. rbp-8]; the save area then occupies
     * [rbp-size-48 .. rbp-size-8], with save[k] (the k-th integer arg
     * register) at rbp-size-48 + k*8, ascending. va_start points at the
     * first variadic slot: rbp - size - 48 + fixed_params*8. */
    if (x64_va_fixed_params >= 0) {
        int base = size + X64_VA_SAVE_BYTES;
        x64_frame_stack_size = base;
        printf("    sub rsp, %d\n", base);
        static const char *va_regs[6] = {"rdi", "rsi", "rdx", "rcx", "r8", "r9"};
        for (int k = 0; k < 6; k++)
            printf("    mov QWORD PTR [rbp%+d], %s\n",
                   -base + k * 8, va_regs[k]);
        x64_va_start_offset = -base + x64_va_fixed_params * 8;
        return;
    }

    x64_frame_stack_size = size;
    if (size > 0)
        printf("    sub rsp, %d\n", size);
}

static void x64_stack_save_acc(void) {
    printf("    mov rax, rsp\n");
}

static void x64_stack_restore_acc(void) {
    printf("    mov rsp, rax\n");
}

static void x64_stack_alloc_acc(void) {
    printf("    add rax, 15\n");
    printf("    and rax, -16\n");
    printf("    sub rsp, rax\n");
    printf("    mov rax, rsp\n");
}

static void x64_load_imm(long value) {
    printf("    mov rax, %ld\n", value);
}

static void x64_push_acc(void) {
    printf("    sub rsp, 8\n");
    printf("    mov [rsp], rax\n");
}

static void x64_pop_to_tmp(void) {
    printf("    mov r10, [rsp]\n");
    printf("    add rsp, 8\n");
}

static void x64_pop_to_acc(void) {
    printf("    mov rax, [rsp]\n");
    printf("    add rsp, 8\n");
}

static void x64_acc_to_arg(int index) {
    static const char *regs[] = {"rdi", "rsi", "rdx", "rcx", "r8", "r9"};
    if (index >= 0 && index < 6)
        printf("    mov %s, rax\n", regs[index]);
}

static void x64_acc_to_tmp(void) {
    printf("    mov r10, rax\n");
}

static void x64_tmp_to_acc(void) {
    printf("    mov rax, r10\n");
}

static void x64_acc_to_saved(void) {
    printf("    mov r11, rax\n");
}

static void x64_saved_to_acc(void) {
    printf("    mov rax, r11\n");
}

static void x64_load_via_saved(int size) {
    if (size == 1)
        printf("    movsx rax, byte [r11]\n");
    else if (size == 4)
        printf("    mov eax, [r11]\n");
    else
        printf("    mov rax, [r11]\n");
}

static void x64_store_via_saved(int size) {
    if (size == 1)
        printf("    mov [r11], al\n");
    else if (size == 4)
        printf("    mov [r11], eax\n");
    else
        printf("    mov [r11], rax\n");
}

static void x64_add(void) {
    printf("    add rax, r10\n");
}

static void x64_sub(void) {
    printf("    sub r10, rax\n");
    printf("    mov rax, r10\n");
}

static void x64_negate(void) {
    printf("    neg rax\n");
}

static void x64_mul(void) {
    printf("    imul rax, r10\n");
}

static void x64_div_op(void) {
    printf("    mov r11, rax\n");
    printf("    mov rax, r10\n");
    printf("    cqo\n");
    printf("    idiv r11\n");
}

static void x64_mod_op(void) {
    printf("    mov r11, rax\n");
    printf("    mov rax, r10\n");
    printf("    cqo\n");
    printf("    idiv r11\n");
    printf("    mov rax, rdx\n");
}

static void x64_div_sized_op(int size) {
    if (size == 4) {
        printf("    mov r11d, eax\n");
        printf("    mov eax, r10d\n");
        printf("    cdq\n");
        printf("    idiv r11d\n");
        return;
    }
    x64_div_op();
}

static void x64_mod_sized_op(int size) {
    if (size == 4) {
        printf("    mov r11d, eax\n");
        printf("    mov eax, r10d\n");
        printf("    cdq\n");
        printf("    idiv r11d\n");
        printf("    mov eax, edx\n");
        return;
    }
    x64_mod_op();
}

static void x64_udiv_op(void) {
    printf("    mov r11, rax\n");
    printf("    mov rax, r10\n");
    printf("    xor rdx, rdx\n");
    printf("    div r11\n");
}

static void x64_umod_op(void) {
    printf("    mov r11, rax\n");
    printf("    mov rax, r10\n");
    printf("    xor rdx, rdx\n");
    printf("    div r11\n");
    printf("    mov rax, rdx\n");
}

static void x64_udiv_sized_op(int size) {
    if (size == 4) {
        printf("    mov r11d, eax\n");
        printf("    mov eax, r10d\n");
        printf("    xor edx, edx\n");
        printf("    div r11d\n");
        return;
    }
    x64_udiv_op();
}

static void x64_umod_sized_op(int size) {
    if (size == 4) {
        printf("    mov r11d, eax\n");
        printf("    mov eax, r10d\n");
        printf("    xor edx, edx\n");
        printf("    div r11d\n");
        printf("    mov eax, edx\n");
        return;
    }
    x64_umod_op();
}


static void x64_bitand_op(void) {
    printf("    and rax, r10\n");
}

static void x64_bitor_op(void) {
    printf("    or rax, r10\n");
}

static void x64_bitnot_op(void) {
    printf("    not rax\n");
}

static void x64_bitxor_op(void) {
    printf("    xor rax, r10\n");
}

static void x64_shl_op(void) {
    printf("    mov rcx, rax\n");
    printf("    mov rax, r10\n");
    printf("    shl rax, cl\n");
}

static void x64_shr_op(void) {
    printf("    mov rcx, rax\n");
    printf("    mov rax, r10\n");
    printf("    sar rax, cl\n");
}

static void x64_ushr_op(void) {
    printf("    mov rcx, rax\n");
    printf("    mov rax, r10\n");
    printf("    shr rax, cl\n");
}

static void x64_shl_imm_op(int imm) {
    printf("    shl rax, %d\n", imm);
}

static void x64_shr_imm_op(int imm) {
    printf("    sar rax, %d\n", imm);
}

static void x64_ushr_imm_op(int imm) {
    printf("    shr rax, %d\n", imm);
}

static void x64_cast_op(int size, int is_unsigned) {
    if (size == 1) {
        if (is_unsigned)
            printf("    and rax, 255\n");
        else
            printf("    movsx rax, al\n");
    } else if (size == 2) {
        if (is_unsigned)
            printf("    and rax, 65535\n");
        else
            printf("    movsx rax, ax\n");
    } else if (size == 4) {
        if (is_unsigned)
            printf("    mov eax, eax\n");
        else
            printf("    movsxd rax, eax\n");
    }
}

static int x64_next_internal_label(void) {
    return ++x64_internal_label_counter;
}

static void x64_acc_bits_to_fp_reg(int xmm, int size) {
    if (size == 4)
        printf("    movd xmm%d, eax\n", xmm);
    else
        printf("    movq xmm%d, rax\n", xmm);
}

static void x64_tmp_bits_to_fp_reg(int xmm, int size) {
    if (size == 4)
        printf("    movd xmm%d, r10d\n", xmm);
    else
        printf("    movq xmm%d, r10\n", xmm);
}

static void x64_fp_to_acc_bits(int size) {
    if (size == 4)
        printf("    movd eax, xmm0\n");
    else
        printf("    movq rax, xmm0\n");
}

static void x64_acc_bits_to_fp_return(int size) {
    x64_acc_bits_to_fp_reg(0, size == 4 ? 4 : 8);
}

static void x64_int_to_fp_bits(int size, int is_unsigned) {
    const char *suffix = (size == 4) ? "ss" : "sd";

    if (is_unsigned && size == 8) {
        int done = x64_next_internal_label();
        printf("    test rax, rax\n");
        printf("    jns Lx64fp%d\n", done);
        printf("    mov r10, rax\n");
        printf("    and eax, 1\n");
        printf("    shr r10, 1\n");
        printf("    movzx eax, al\n");
        printf("    or r10, rax\n");
        printf("    cvtsi2s%s xmm0, r10\n", suffix);
        printf("    add%s xmm0, xmm0\n", suffix);
        printf("    jmp Lx64fp%d_done\n", done);
        printf("Lx64fp%d:\n", done);
        printf("    cvtsi2s%s xmm0, rax\n", suffix);
        printf("Lx64fp%d_done:\n", done);
    } else {
        if (is_unsigned && size == 4)
            printf("    mov eax, eax\n");
        printf("    cvtsi2s%s xmm0, %s\n", suffix, size == 4 ? "eax" : "rax");
    }
    x64_fp_to_acc_bits(size);
}

static void x64_fp_bits_to_int(int size, int is_unsigned) {
    x64_acc_bits_to_fp_reg(0, size == 4 ? 4 : 8);
    if (size == 4) {
        printf("    cvttss2si eax, xmm0\n");
        if (is_unsigned)
            printf("    mov eax, eax\n");
    } else {
        printf("    cvttsd2si rax, xmm0\n");
        if (is_unsigned) {
            /* Current x64 floating lowering only validates small positive
             * unsigned conversions; broader exact u64 semantics remain part
             * of the remaining x64 floating conformance work. */
        }
    }
}

static void x64_fp_cast_bits(int src_size, int dst_size) {
    if (src_size == dst_size)
        return;
    x64_acc_bits_to_fp_reg(0, src_size == 4 ? 4 : 8);
    if (src_size == 4 && dst_size == 8)
        printf("    cvtss2sd xmm0, xmm0\n");
    else if (src_size == 8 && dst_size == 4)
        printf("    cvtsd2ss xmm0, xmm0\n");
    else
        ICE("unsupported x64 floating cast size %d -> %d", src_size, dst_size);
    x64_fp_to_acc_bits(dst_size);
}

static void x64_emit_fp_setcc(const char *setcc, int ordered_only) {
    if (ordered_only) {
        printf("    set%s al\n", setcc);
        printf("    setnp dl\n");
        printf("    and al, dl\n");
        printf("    movzx eax, al\n");
    } else {
        printf("    set%s al\n", setcc);
        printf("    movzx eax, al\n");
    }
}

static void x64_fp_binop(const char *op, int size) {
    const char *suffix = (size == 4) ? "ss" : "sd";

    x64_tmp_bits_to_fp_reg(1, size);
    x64_acc_bits_to_fp_reg(0, size);

    if (strcmp(op, "add") == 0) {
        printf("    add%s xmm1, xmm0\n", suffix);
        if (size == 4)
            printf("    movd eax, xmm1\n");
        else
            printf("    movq rax, xmm1\n");
        return;
    }
    if (strcmp(op, "sub") == 0) {
        printf("    sub%s xmm1, xmm0\n", suffix);
        if (size == 4)
            printf("    movd eax, xmm1\n");
        else
            printf("    movq rax, xmm1\n");
        return;
    }
    if (strcmp(op, "mul") == 0) {
        printf("    mul%s xmm1, xmm0\n", suffix);
        if (size == 4)
            printf("    movd eax, xmm1\n");
        else
            printf("    movq rax, xmm1\n");
        return;
    }
    if (strcmp(op, "div") == 0) {
        printf("    div%s xmm1, xmm0\n", suffix);
        if (size == 4)
            printf("    movd eax, xmm1\n");
        else
            printf("    movq rax, xmm1\n");
        return;
    }

    printf("    ucomi%s xmm1, xmm0\n", suffix);
    if (strcmp(op, "eq") == 0 || strcmp(op, "je") == 0)
        x64_emit_fp_setcc("e", 1);
    else if (strcmp(op, "ne") == 0 || strcmp(op, "jne") == 0) {
        int done = x64_next_internal_label();
        printf("    jp Lx64fp%d\n", done);
        printf("    setne al\n");
        printf("    movzx eax, al\n");
        printf("    jmp Lx64fp%d_done\n", done);
        printf("Lx64fp%d:\n", done);
        printf("    mov eax, 1\n");
        printf("Lx64fp%d_done:\n", done);
    } else if (strcmp(op, "lt") == 0 || strcmp(op, "jlt") == 0)
        x64_emit_fp_setcc("b", 1);
    else if (strcmp(op, "le") == 0 || strcmp(op, "jle") == 0)
        x64_emit_fp_setcc("be", 1);
    else if (strcmp(op, "gt") == 0 || strcmp(op, "jgt") == 0)
        x64_emit_fp_setcc("a", 1);
    else if (strcmp(op, "ge") == 0 || strcmp(op, "jge") == 0)
        x64_emit_fp_setcc("ae", 1);
    else
        ICE("unsupported x64 floating binop: %s", op ? op : "<null>");
}

static void x64_fp_cmp_branch(const char *op, int size, int label) {
    const char *suffix = (size == 4) ? "ss" : "sd";
    int skip = x64_next_internal_label();

    x64_tmp_bits_to_fp_reg(1, size);
    x64_acc_bits_to_fp_reg(0, size);
    printf("    ucomi%s xmm1, xmm0\n", suffix);

    if (strcmp(op, "eq") == 0 || strcmp(op, "je") == 0) {
        printf("    jp Lx64fp%d\n", skip);
        printf("    je L%d\n", label);
    } else if (strcmp(op, "ne") == 0 || strcmp(op, "jne") == 0) {
        printf("    jp L%d\n", label);
        printf("    jne L%d\n", label);
    } else if (strcmp(op, "lt") == 0 || strcmp(op, "jlt") == 0) {
        printf("    jp Lx64fp%d\n", skip);
        printf("    jb L%d\n", label);
    } else if (strcmp(op, "le") == 0 || strcmp(op, "jle") == 0) {
        printf("    jp Lx64fp%d\n", skip);
        printf("    jbe L%d\n", label);
    } else if (strcmp(op, "gt") == 0 || strcmp(op, "jgt") == 0) {
        printf("    jp Lx64fp%d\n", skip);
        printf("    ja L%d\n", label);
    } else if (strcmp(op, "ge") == 0 || strcmp(op, "jge") == 0) {
        printf("    jp Lx64fp%d\n", skip);
        printf("    jae L%d\n", label);
    } else {
        ICE("unsupported x64 floating compare branch op: %s", op ? op : "<null>");
    }

    if (!(strcmp(op, "ne") == 0 || strcmp(op, "jne") == 0))
        printf("Lx64fp%d:\n", skip);
}

static void
x64_load_fp_call_reg(int fp_reg, int arg_slot, int size)
{
    printf("    mov%s xmm%d, %s PTR [rsp+%d]\n",
           size == 4 ? "ss" : "sd",
           fp_reg,
           size == 4 ? "DWORD" : "QWORD",
           arg_slot * 8);
}

static void
x64_prepare_fp_call_args(int count, int fixed_params,
                         unsigned int fp_arg_mask,
                         unsigned int fp_arg_double_mask)
{
    int is_variadic = (fixed_params >= 0);
    int int_reg = 0;
    int fp_reg = 0;
    int overflow[32];
    int overflow_count = 0;
    int align_pad;
    int i;

    if (count < 0 || count > 32)
        ICE("unsupported x64 floating call arg count");

    for (i = 0; i < count; i++) {
        if (fp_arg_mask & (1u << i)) {
            int size = (fp_arg_double_mask & (1u << i)) ? 8 : 4;
            if (fp_reg < 8) {
                x64_load_fp_call_reg(fp_reg, i, size);
                fp_reg++;
            } else {
                overflow[overflow_count++] = i;
            }
        } else {
            if (int_reg < 6) {
                printf("    mov %s, QWORD PTR [rsp+%d]\n", x64_arg_reg(int_reg), i * 8);
                int_reg++;
            } else {
                overflow[overflow_count++] = i;
            }
        }
    }

    for (i = 0; i < overflow_count; i++) {
        if (overflow[i] == i)
            continue;
        printf("    mov rax, QWORD PTR [rsp+%d]\n", overflow[i] * 8);
        printf("    mov QWORD PTR [rsp+%d], rax\n", i * 8);
    }

    align_pad = ((x64_frame_stack_size / 8) + count) & 1;
    x64_call_stack_adjust = count * 8 + align_pad * 8;

    if (align_pad) {
        printf("    sub rsp, 8\n");
        for (i = 0; i < overflow_count; i++) {
            printf("    mov rax, QWORD PTR [rsp+%d]\n", 8 + i * 8);
            printf("    mov QWORD PTR [rsp+%d], rax\n", i * 8);
        }
    }

    if (is_variadic)
        printf("    mov eax, %d\n", fp_reg);
}

static void
x64_call_fp_args(const char *name, int count, int fixed_params,
                 unsigned int fp_arg_mask, unsigned int fp_arg_double_mask)
{
    x64_prepare_fp_call_args(count, fixed_params, fp_arg_mask, fp_arg_double_mask);
    x64_call(name);
    x64_cleanup_call_args(count, fixed_params);
}

static void
x64_call_saved_fp_args(int count, int fixed_params,
                       unsigned int fp_arg_mask, unsigned int fp_arg_double_mask)
{
    x64_prepare_fp_call_args(count, fixed_params, fp_arg_mask, fp_arg_double_mask);
    x64_call_saved();
    x64_cleanup_call_args(count, fixed_params);
}

static void x64_ptr_add(int scale) {
    if (scale == 4)
        printf("    shl rax, 2\n");
    else if (scale != 1)
        printf("    imul rax, rax, %d\n", scale);
    printf("    add rax, r10\n");
}

static void x64_ptr_sub(int scale) {
    if (scale == 4)
        printf("    shl rax, 2\n");
    else if (scale != 1)
        printf("    imul rax, rax, %d\n", scale);
    printf("    sub r10, rax\n");
    printf("    mov rax, r10\n");
}

static void x64_cmp_common(const char *op) {
    printf("    cmp r10, rax\n");
    printf("    %s al\n", op);
    printf("    movzx rax, al\n");
}

static void x64_cmp_eq(void) { x64_cmp_common("sete"); }
static void x64_cmp_ne(void) { x64_cmp_common("setne"); }

static void x64_cmp_sized_common(const char *op, int size) {
    if (size <= 4)
        printf("    cmp r10d, eax\n");
    else
        printf("    cmp r10, rax\n");
    printf("    %s al\n", op);
    printf("    movzx rax, al\n");
}

static void x64_cmp_eq_sized(int size) { x64_cmp_sized_common("sete", size); }
static void x64_cmp_ne_sized(int size) { x64_cmp_sized_common("setne", size); }
static void x64_cmp_lt_sized(int size) { x64_cmp_sized_common("setl", size); }
static void x64_cmp_le_sized(int size) { x64_cmp_sized_common("setle", size); }
static void x64_cmp_gt_sized(int size) { x64_cmp_sized_common("setg", size); }
static void x64_cmp_ge_sized(int size) { x64_cmp_sized_common("setge", size); }
static void x64_cmp_lt_u_sized(int size) { x64_cmp_sized_common("setb", size); }
static void x64_cmp_le_u_sized(int size) { x64_cmp_sized_common("setbe", size); }
static void x64_cmp_gt_u_sized(int size) { x64_cmp_sized_common("seta", size); }
static void x64_cmp_ge_u_sized(int size) { x64_cmp_sized_common("setae", size); }
static void x64_cmp_lt(void) { x64_cmp_common("setl"); }
static void x64_cmp_le(void) { x64_cmp_common("setle"); }
static void x64_cmp_gt(void) { x64_cmp_common("setg"); }
static void x64_cmp_ge(void) { x64_cmp_common("setge"); }

static void x64_cmp_lt_u(void) { x64_cmp_common("setb"); }
static void x64_cmp_le_u(void) { x64_cmp_common("setbe"); }
static void x64_cmp_gt_u(void) { x64_cmp_common("seta"); }
static void x64_cmp_ge_u(void) { x64_cmp_common("setae"); }


static char x64_slot_buf[32];

static const char *x64_slot(int offset) {
    if (offset < 0)
        snprintf(x64_slot_buf, sizeof(x64_slot_buf), "[rbp-%d]", -offset);
    else
        snprintf(x64_slot_buf, sizeof(x64_slot_buf), "[rbp+%d]", offset);
    return x64_slot_buf;
}

static void x64_load_local(int offset) {
    printf("    mov eax, DWORD PTR %s\n", x64_slot(offset));
}

static void x64_store_local(int offset) {
    printf("    mov DWORD PTR %s, eax\n", x64_slot(offset));
}

static void x64_load_local_sized(int offset, int size) {
    if (size == 1)
        printf("    movzx eax, BYTE PTR %s\n", x64_slot(offset));
    else if (size == 2)
        printf("    movzx eax, WORD PTR %s\n", x64_slot(offset));
    else if (size == 8)
        printf("    mov rax, QWORD PTR %s\n", x64_slot(offset));
    else
        printf("    mov eax, DWORD PTR %s\n", x64_slot(offset));
}

static void x64_load_local_casted(int offset, int load_size, int is_unsigned) {
    if (load_size == 1) {
        if (is_unsigned)
            printf("    movzx eax, BYTE PTR %s\n", x64_slot(offset));
        else
            printf("    movsx eax, BYTE PTR %s\n", x64_slot(offset));
    } else if (load_size == 2) {
        if (is_unsigned)
            printf("    movzx eax, WORD PTR %s\n", x64_slot(offset));
        else
            printf("    movsx eax, WORD PTR %s\n", x64_slot(offset));
    } else if (load_size == 8) {
        printf("    mov rax, QWORD PTR %s\n", x64_slot(offset));
    } else if (is_unsigned) {
        printf("    mov eax, DWORD PTR %s\n", x64_slot(offset));
    } else {
        printf("    movsxd rax, DWORD PTR %s\n", x64_slot(offset));
    }
}

static void x64_load_local_ptr_member(int local_offset, int member_offset, int size) {
    printf("    mov r10, QWORD PTR %s\n", x64_slot(local_offset));
    if (size == 1)
        printf("    movzx eax, BYTE PTR [r10+%d]\n", member_offset);
    else if (size == 2)
        printf("    movzx eax, WORD PTR [r10+%d]\n", member_offset);
    else if (size == 8)
        printf("    mov rax, QWORD PTR [r10+%d]\n", member_offset);
    else
        printf("    mov eax, DWORD PTR [r10+%d]\n", member_offset);
}

static void x64_load_local_ptr_member_casted(int local_offset, int member_offset,
                                             int load_size, int cast_size, int is_unsigned) {
    (void)load_size;
    printf("    mov r10, QWORD PTR %s\n", x64_slot(local_offset));
    if (cast_size == 1) {
        if (is_unsigned)
            printf("    movzx eax, BYTE PTR [r10+%d]\n", member_offset);
        else
            printf("    movsx eax, BYTE PTR [r10+%d]\n", member_offset);
    } else if (cast_size == 2) {
        if (is_unsigned)
            printf("    movzx eax, WORD PTR [r10+%d]\n", member_offset);
        else
            printf("    movsx eax, WORD PTR [r10+%d]\n", member_offset);
    } else if (cast_size == 8) {
        printf("    mov rax, QWORD PTR [r10+%d]\n", member_offset);
    } else if (is_unsigned) {
        printf("    mov eax, DWORD PTR [r10+%d]\n", member_offset);
    } else {
        printf("    movsxd rax, DWORD PTR [r10+%d]\n", member_offset);
    }
}

static void x64_store_local_sized(int offset, int size) {
    if (size == 1)
        printf("    mov BYTE PTR %s, al\n", x64_slot(offset));
    else if (size == 2)
        printf("    mov WORD PTR %s, ax\n", x64_slot(offset));
    else if (size == 8)
        printf("    mov QWORD PTR %s, rax\n", x64_slot(offset));
    else
        printf("    mov DWORD PTR %s, eax\n", x64_slot(offset));
}

static void x64_load_global(const char *name, int size) {
    if (size == 1)
        printf("    movzx eax, BYTE PTR [rip + \"%s%s\"]\n", TCC_ASM_SYM_PREFIX, name);
    else if (size == 2)
        printf("    movzx eax, WORD PTR [rip + \"%s%s\"]\n", TCC_ASM_SYM_PREFIX, name);
    else if (size == 8)
        printf("    mov rax, QWORD PTR [rip + \"%s%s\"]\n", TCC_ASM_SYM_PREFIX, name);
    else
        printf("    mov eax, DWORD PTR [rip + \"%s%s\"]\n", TCC_ASM_SYM_PREFIX, name);
}

static void x64_load_global_member(const char *name, int offset, int size, int is_extern) {
    (void)is_extern;

    printf("    lea r10, [rip + \"%s%s\"]\n", TCC_ASM_SYM_PREFIX, name);
    if (offset) {
        if (size == 1)
            printf("    movzx eax, BYTE PTR [r10 + %d]\n", offset);
        else if (size == 2)
            printf("    movzx eax, WORD PTR [r10 + %d]\n", offset);
        else if (size == 8)
            printf("    mov rax, QWORD PTR [r10 + %d]\n", offset);
        else
            printf("    movsxd rax, DWORD PTR [r10 + %d]\n", offset);
    } else {
        if (size == 1)
            printf("    movzx eax, BYTE PTR [r10]\n");
        else if (size == 2)
            printf("    movzx eax, WORD PTR [r10]\n");
        else if (size == 8)
            printf("    mov rax, QWORD PTR [r10]\n");
        else
            printf("    movsxd rax, DWORD PTR [r10]\n");
    }
}

static void x64_store_global(const char *name, int size) {
    if (size == 1)
        printf("    mov BYTE PTR [rip + \"%s%s\"], al\n", TCC_ASM_SYM_PREFIX, name);
    else if (size == 2)
        printf("    mov WORD PTR [rip + \"%s%s\"], ax\n", TCC_ASM_SYM_PREFIX, name);
    else if (size == 8)
        printf("    mov QWORD PTR [rip + \"%s%s\"], rax\n", TCC_ASM_SYM_PREFIX, name);
    else
        printf("    mov DWORD PTR [rip + \"%s%s\"], eax\n", TCC_ASM_SYM_PREFIX, name);
}

static void x64_load_global_indexed(const char *name, int elem_size) {
    printf("    movsxd rax, eax\n");
    if (elem_size == 8)
        printf("    shl rax, 3\n");
    else if (elem_size == 4)
        printf("    shl rax, 2\n");
    else if (elem_size != 1)
        printf("    imul rax, rax, %d\n", elem_size);
    printf("    lea r11, [rip + \"%s%s\"]\n", TCC_ASM_SYM_PREFIX, name);
    if (elem_size == 1)
        printf("    movzx eax, BYTE PTR [r11 + rax]\n");
    else if (elem_size == 2)
        printf("    movzx eax, WORD PTR [r11 + rax]\n");
    else if (elem_size == 8)
        printf("    mov rax, QWORD PTR [r11 + rax]\n");
    else
        printf("    mov eax, DWORD PTR [r11 + rax]\n");
}

static void x64_store_global_indexed(const char *name, int elem_size) {
    printf("    movsxd r10, r10d\n");
    if (elem_size == 8)
        printf("    shl r10, 3\n");
    else if (elem_size == 4)
        printf("    shl r10, 2\n");
    else if (elem_size != 1)
        printf("    imul r10, r10, %d\n", elem_size);
    printf("    lea r11, [rip + \"%s%s\"]\n", TCC_ASM_SYM_PREFIX, name);
    if (elem_size == 1)
        printf("    mov BYTE PTR [r11 + r10], al\n");
    else if (elem_size == 2)
        printf("    mov WORD PTR [r11 + r10], ax\n");
    else if (elem_size == 8)
        printf("    mov QWORD PTR [r11 + r10], rax\n");
    else
        printf("    mov DWORD PTR [r11 + r10], eax\n");
}

static void x64_store_param(int index, int offset) {
    if (index < 0) {
        ICE("invalid x64 parameter index");
        return;
    }

    if (index < 6) {
        switch (index) {
        case 0:
            printf("    mov QWORD PTR [rbp%+d], rdi\n", offset);
            break;
        case 1:
            printf("    mov QWORD PTR [rbp%+d], rsi\n", offset);
            break;
        case 2:
            printf("    mov QWORD PTR [rbp%+d], rdx\n", offset);
            break;
        case 3:
            printf("    mov QWORD PTR [rbp%+d], rcx\n", offset);
            break;
        case 4:
            printf("    mov QWORD PTR [rbp%+d], r8\n", offset);
            break;
        case 5:
            printf("    mov QWORD PTR [rbp%+d], r9\n", offset);
            break;
        default:
            ICE("invalid x64 parameter index");
            break;
        }
        return;
    }

    /*
     * SysV x86-64 passes integer/pointer parameters 7+ on the caller's
     * stack.  After this function's prologue:
     *   [rbp+0]  = saved rbp
     *   [rbp+8]  = return address
     *   [rbp+16] = parameter 6 (the seventh C parameter)
     */
    printf("    mov rax, QWORD PTR [rbp+%d]\n", 16 + (index - 6) * 8);
    printf("    mov QWORD PTR [rbp%+d], rax\n", offset);
}

static void x64_store_fp_param(int fp_index, int offset, int size) {
    if (fp_index < 0) {
        ICE("invalid x64 floating parameter index");
        return;
    }

    if (size != 4 && size != 8)
        size = 8;

    if (fp_index < 8) {
        printf("    mov%s %s PTR [rbp%+d], xmm%d\n",
               size == 4 ? "ss" : "sd",
               size == 4 ? "DWORD" : "QWORD",
               offset,
               fp_index);
        return;
    }

    /*
     * SysV x86-64 stack-passed floating parameters are spilled by the caller
     * in 8-byte slots after the return address, just like other overflow
     * arguments. This backend currently tracks floating register ordinal
     * independently from integer register ordinal, so the overflow case is
     * only exact for float-only/double-only overflow tails. That is still
     * enough to unblock the current scalar floating bootstrap path and gives
     * a precise implementation point for later mixed overflow work.
     */
    if (size == 4) {
        printf("    movss xmm15, DWORD PTR [rbp+%d]\n", 16 + (fp_index - 8) * 8);
        printf("    movss DWORD PTR [rbp%+d], xmm15\n", offset);
    } else {
        printf("    movsd xmm15, QWORD PTR [rbp+%d]\n", 16 + (fp_index - 8) * 8);
        printf("    movsd QWORD PTR [rbp%+d], xmm15\n", offset);
    }
}

static void x64_copy_local(int dst_offset, int src_offset, int size) {
    int off = 0;
    int remaining = size;

    while (remaining >= 8) {
        printf("    mov rax, QWORD PTR %s\n", x64_slot(src_offset + off));
        printf("    mov QWORD PTR %s, rax\n", x64_slot(dst_offset + off));
        off += 8;
        remaining -= 8;
    }
    if (remaining >= 4) {
        printf("    mov eax, DWORD PTR %s\n", x64_slot(src_offset + off));
        printf("    mov DWORD PTR %s, eax\n", x64_slot(dst_offset + off));
        off += 4;
        remaining -= 4;
    }
    if (remaining >= 2) {
        printf("    movzx eax, WORD PTR %s\n", x64_slot(src_offset + off));
        printf("    mov WORD PTR %s, ax\n", x64_slot(dst_offset + off));
        off += 2;
        remaining -= 2;
    }
    if (remaining >= 1) {
        printf("    movzx eax, BYTE PTR %s\n", x64_slot(src_offset + off));
        printf("    mov BYTE PTR %s, al\n", x64_slot(dst_offset + off));
    }
}

static void x64_ptr_copy(int size) {
    /* Copy `size` bytes from src (acc=rax) to dst (saved=r11).
     * Uses r10 as the data scratch register. Exact-size tail handling
     * so we never write past the end of the destination object. */
    int off = 0;
    int remaining = size;

    while (remaining >= 8) {
        printf("    mov r10, QWORD PTR [rax+%d]\n", off);
        printf("    mov QWORD PTR [r11+%d], r10\n", off);
        off += 8;
        remaining -= 8;
    }
    if (remaining >= 4) {
        printf("    mov r10d, DWORD PTR [rax+%d]\n", off);
        printf("    mov DWORD PTR [r11+%d], r10d\n", off);
        off += 4;
        remaining -= 4;
    }
    if (remaining >= 2) {
        printf("    movzx r10d, WORD PTR [rax+%d]\n", off);
        printf("    mov WORD PTR [r11+%d], r10w\n", off);
        off += 2;
        remaining -= 2;
    }
    if (remaining >= 1) {
        printf("    movzx r10d, BYTE PTR [rax+%d]\n", off);
        printf("    mov BYTE PTR [r11+%d], r10b\n", off);
    }
}

static void x64_copy_local_to_ptr(int ptr_offset, int src_offset, int size) {
    printf("    mov r11, QWORD PTR %s\n", x64_slot(ptr_offset));
    for (int off = 0; off < size; off += 4) {
        printf("    mov eax, DWORD PTR %s\n", x64_slot(src_offset + off));
        printf("    mov DWORD PTR [r11+%d], eax\n", off);
    }
}

static void x64_index_scale(const char *reg, int elem_size) {
    if (elem_size == 2)
        printf("    shl %s, 1\n", reg);
    else if (elem_size == 4)
        printf("    shl %s, 2\n", reg);
    else if (elem_size == 8)
        printf("    shl %s, 3\n", reg);
    else if (elem_size != 1)
        printf("    imul %s, %s, %d\n", reg, reg, elem_size);
}

static void x64_load_indexed(int base_offset, int elem_size) {
    printf("    movsxd r10, eax\n");
    x64_index_scale("r10", elem_size);
    if (elem_size == 1)
        printf("    movzx eax, BYTE PTR [rbp%+d+r10]\n", base_offset);
    else if (elem_size == 2)
        printf("    movsx eax, WORD PTR [rbp%+d+r10]\n", base_offset);
    else if (elem_size == 8)
        printf("    mov rax, QWORD PTR [rbp%+d+r10]\n", base_offset);
    else
        printf("    mov eax, DWORD PTR [rbp%+d+r10]\n", base_offset);
}

static void x64_store_indexed(int base_offset, int elem_size) {
    printf("    movsxd r10, r10d\n");
    x64_index_scale("r10", elem_size);
    if (elem_size == 1)
        printf("    mov BYTE PTR [rbp%+d+r10], al\n", base_offset);
    else if (elem_size == 2)
        printf("    mov WORD PTR [rbp%+d+r10], ax\n", base_offset);
    else if (elem_size == 8)
        printf("    mov QWORD PTR [rbp%+d+r10], rax\n", base_offset);
    else
        printf("    mov DWORD PTR [rbp%+d+r10], eax\n", base_offset);
}

static void x64_addr_local(int offset) {
    printf("    lea rax, [rbp%+d]\n", offset);
}

static void x64_addr_indexed(int base_offset, int elem_size) {
    printf("    movsxd r10, eax\n");
    if (elem_size == 4)
        printf("    shl r10, 2\n");
    else if (elem_size != 1)
        printf("    imul r10, r10, %d\n", elem_size);
    printf("    lea rax, [rbp%+d+r10]\n", base_offset);
}

static void x64_add_offset(int offset) {
    if (offset)
        printf("    add rax, %d\n", offset);
}

static void x64_load_ptr_local(int offset) {
    printf("    mov rax, QWORD PTR %s\n", x64_slot(offset));
}

static void x64_store_ptr_local(int offset) {
    printf("    mov QWORD PTR %s, rax\n", x64_slot(offset));
}

static void x64_load_deref(int size) {
    if (size == 1)
        printf("    movzx eax, BYTE PTR [rax]\n");
    else if (size == 2)
        printf("    movsx eax, WORD PTR [rax]\n");
    else if (size == 8)
        printf("    mov rax, QWORD PTR [rax]\n");
    else
        printf("    mov eax, DWORD PTR [rax]\n");
}

static void x64_store_deref(int size) {
    if (size == 1)
        printf("    mov BYTE PTR [r10], al\n");
    else if (size == 2)
        printf("    mov WORD PTR [r10], ax\n");
    else if (size == 8)
        printf("    mov QWORD PTR [r10], rax\n");
    else
        printf("    mov DWORD PTR [r10], eax\n");
}

static void x64_load_member_ptr(int offset, int size) {
    if (size == 1)
        printf("    movsx eax, BYTE PTR [rax+%d]\n", offset);
    else if (size == 2)
        printf("    movsx eax, WORD PTR [rax+%d]\n", offset);
    else if (size == 8)
        printf("    mov rax, QWORD PTR [rax+%d]\n", offset);
    else
        printf("    mov eax, DWORD PTR [rax+%d]\n", offset);
}

static void x64_store_member_ptr(int offset, int size) {
    if (size == 1)
        printf("    mov BYTE PTR [r10+%d], al\n", offset);
    else if (size == 2)
        printf("    mov WORD PTR [r10+%d], ax\n", offset);
    else if (size == 8)
        printf("    mov QWORD PTR [r10+%d], rax\n", offset);
    else
        printf("    mov DWORD PTR [r10+%d], eax\n", offset);
}

static void x64_store_local_ptr_member(int local_offset, int member_offset, int size) {
    printf("    mov r10, QWORD PTR %s\n", x64_slot(local_offset));
    if (size == 1)
        printf("    mov BYTE PTR [r10+%d], al\n", member_offset);
    else if (size == 2)
        printf("    mov WORD PTR [r10+%d], ax\n", member_offset);
    else if (size == 8)
        printf("    mov QWORD PTR [r10+%d], rax\n", member_offset);
    else
        printf("    mov DWORD PTR [r10+%d], eax\n", member_offset);
}

static void x64_string_literal(int label, const char *value, size_t len, int width) {
    if (width > 1) {
        printf("%s\n", TCC_ASM_CONST_SECTION);
        if (width >= 4)
            printf(".align 4\n");
        else
            printf(".align 2\n");
    } else {
        printf("%s\n", TCC_ASM_CSTRING_SECTION);
    }
    printf(".Lstr%d:\n", label);
    printf("    .byte ");
    for (size_t i = 0; i < len; i++) {
        if (i)
            printf(",");
        printf("%u", (unsigned char)value[i]);
    }
    if (len)
        printf(",");
    printf("0\n");
    printf(".text\n");
}

static void x64_load_string(int label) {
    printf("    lea rax, [rip + .Lstr%d]\n", label);
}


static void x64_load_func_addr(const char *name) {
    printf("    lea rax, [rip + \"%s%s\"]\n", TCC_ASM_SYM_PREFIX, name);
}

static void x64_inline_asm(const char *text) {
    if (!text || !*text)
        return;

    printf("    %s\n", text);
}

static void x64_branch_if_zero(int label) {
    printf("    cmp eax, 0\n");
    printf("    je L%d\n", label);
}

static void x64_branch_if_nonzero(int label) {
    printf("    cmp eax, 0\n");
    printf("    jne L%d\n", label);
}

static void x64_branch(int label) {
    printf("    jmp L%d\n", label);
}

static void x64_label(int label) {
    printf("L%d:\n", label);
}

static void x64_emit_label_named_impl(const char *name) {
    printf(".L%s:\n", name);
}

static void x64_emit_branch_named_impl(const char *name) {
    printf("    jmp .L%s\n", name);
}

static void x64_emit_cmp_branch_impl(const char *op, int size, int label) {
    const char *jump = NULL;

    if (strcmp(op, "je") == 0)
        jump = "je";
    else if (strcmp(op, "jne") == 0)
        jump = "jne";
    else if (strcmp(op, "jlt") == 0)
        jump = "jl";
    else if (strcmp(op, "jle") == 0)
        jump = "jle";
    else if (strcmp(op, "jgt") == 0)
        jump = "jg";
    else if (strcmp(op, "jge") == 0)
        jump = "jge";
    else if (strcmp(op, "jult") == 0)
        jump = "jb";
    else if (strcmp(op, "jule") == 0)
        jump = "jbe";
    else if (strcmp(op, "jugt") == 0)
        jump = "ja";
    else if (strcmp(op, "juge") == 0)
        jump = "jae";
    else
        ICE("unsupported x64 compare branch op: %s", op ? op : "<null>");

    if (size <= 4)
        printf("    cmp r10d, eax\n");
    else
        printf("    cmp r10, rax\n");
    printf("    %s L%d\n", jump, label);
}


static void x64_prepare_call_args(int count, int fixed_params) {
    /* static const char *regs[] = {"rdi", "rsi", "rdx", "rcx", "r8", "r9"}; */
    int is_variadic = (fixed_params >= 0);
    int reg_count;
    int stack_count;

    /*
     * SysV x86-64 passes the first six integer/pointer arguments in
     * registers whether named or variadic. Variadic callees recover the
     * register args from a save area built in their prologue (see
     * x64_stack_alloc / x64_va_start), so the caller no longer needs to
     * push variadic args on the stack. fixed_params now only governs the
     * AL (SSE-count) requirement below.
     */
    int reg_limit = 6;

    reg_count = count < reg_limit ? count : reg_limit;
    stack_count = count - reg_count;
    if (stack_count < 0)
        stack_count = 0;

    /*
     * At function entry SysV gives rsp % 16 == 8.  The prologue's
     * push rbp makes it aligned, then local stack allocation and any
     * remaining overflow call arguments may misalign it again.  Before
     * the call instruction, rsp itself must be 16-byte aligned.
     */
    int align_pad = ((x64_frame_stack_size / 8) + stack_count) & 1;

    x64_call_stack_adjust = stack_count * 8 + align_pad * 8;

    /*
     * The generic emitter has pushed arguments right-to-left, so [rsp]
     * is arg0 here.  Pop the first six into SysV register arguments and
     * leave arg6..argN on the stack for the callee.
     */
    for (int i = 0; i < reg_count; i++) {
        /*
         * For variadic calls, only the fixed named arguments are moved into
         * registers; unnamed arguments remain packed at [rsp] for va_arg.
         */
        printf("    mov %s, QWORD PTR [rsp]\n", x64_arg_reg(i));
        printf("    add rsp, 8\n");
    }

    /* Keep rsp 16-byte aligned before call when an odd number of overflow
     * arguments remains on the stack.  The first overflow argument must stay
     * at [rsp] for the SysV callee, so make room and slide overflow args down;
     * the unused pad slot ends up above the last stack argument.
     */
    if (align_pad) {
        printf("    sub rsp, 8\n");
        for (int i = 0; i < stack_count; i++)
            printf("    mov rax, QWORD PTR [rsp+%d]\n"
                   "    mov QWORD PTR [rsp+%d], rax\n",
                   8 + i * 8, i * 8);
    }

    /* SysV x86-64 variadic calls require AL to contain the number of
     * vector registers used for floating-point arguments.  This compiler
     * currently passes only integer/pointer arguments here, so the count is 0.
     */
    if (is_variadic)
        printf("    mov eax, 0\n");
}

static void x64_call(const char *name) {
    printf("    call %s%s\n", TCC_ASM_SYM_PREFIX, name);
}

static void x64_call_saved(void) {
    printf("    call r11\n");
}

static void x64_cleanup_call_args(int count, int fixed_params) {
    (void)fixed_params;
    (void)count;
    if (x64_call_stack_adjust > 0) {
        printf("    add rsp, %d\n", x64_call_stack_adjust);
        x64_call_stack_adjust = 0;
    }
}

static void x64_load_global_extern(const char *name) {
    x64_load_global(name, 8);  /* extern refs are always pointer-sized */
}
static void x64_store_global_extern(const char *name) {
    x64_store_global(name, 8);
}

static void x64_va_start(void) {
    /* Leave the address of the first variadic argument's save slot in rax.
     * x64_va_start_offset was computed in the prologue for this function. */
    printf("    lea rax, [rbp%+d]\n", x64_va_start_offset);
}

Codegen x64_codegen = {
    x64_preamble,
    x64_func_start,
    x64_func_end,
    x64_stack_alloc,
    x64_stack_save_acc,
    x64_stack_restore_acc,
    x64_stack_alloc_acc,
    x64_load_imm,
    x64_push_acc,
    x64_pop_to_tmp,
    x64_pop_to_acc,
    x64_acc_to_arg,
    x64_acc_to_tmp,
    x64_tmp_to_acc,
    x64_acc_to_saved,
    x64_saved_to_acc,
    x64_load_via_saved,
    x64_store_via_saved,
    x64_add,
    x64_sub,
    x64_negate,
    x64_mul,
    x64_div_op,
    x64_mod_op,
    x64_bitand_op,
    NULL,
    x64_bitor_op,
    x64_bitnot_op,
    x64_bitxor_op,
    x64_shl_op,
    x64_shr_op,
    x64_udiv_op,
    x64_umod_op,
    x64_ushr_op,
    x64_shl_imm_op,
    x64_shr_imm_op,
    x64_ushr_imm_op,
    x64_cast_op,
    x64_ptr_add,
    x64_ptr_sub,
    x64_cmp_eq,
    x64_cmp_ne,
    x64_cmp_lt,
    x64_cmp_le,
    x64_cmp_gt,
    x64_cmp_ge,
    x64_load_local,
    x64_store_local,
    x64_load_local_sized,
    x64_store_local_sized,
    x64_load_global,
    x64_load_global_extern,   /* load_global_extern: no GOT needed */
    x64_store_global,
    x64_store_global_extern,  /* store_global_extern: no GOT needed */
    x64_load_global_indexed,
    x64_store_global_indexed,
    x64_store_param,
    NULL,
    x64_copy_local,
    x64_ptr_copy,
    x64_copy_local_to_ptr,
    NULL,
    x64_load_indexed,
    x64_store_indexed,
    x64_addr_local,
    x64_addr_indexed,
    x64_add_offset,
    x64_load_ptr_local,
    x64_store_ptr_local,
    x64_load_deref,
    x64_store_deref,
    x64_load_member_ptr,
    x64_store_member_ptr,
    x64_string_literal,
    x64_load_string,
    x64_load_func_addr,
    x64_inline_asm,
    x64_branch_if_zero,
    x64_branch_if_nonzero,
    x64_branch,
    x64_label,
    x64_prepare_call_args,
    x64_call,
    x64_call_saved,
    x64_cleanup_call_args,
    x64_emit_label_named_impl,
    x64_emit_branch_named_impl,
    x64_emit_source_loc,
    x64_cmp_lt_u,
    x64_cmp_le_u,
    x64_cmp_gt_u,
    x64_cmp_ge_u,
    x64_cmp_eq_sized,
    x64_cmp_ne_sized,
    x64_cmp_lt_sized,
    x64_cmp_le_sized,
    x64_cmp_gt_sized,
    x64_cmp_ge_sized,
    x64_cmp_lt_u_sized,
    x64_cmp_le_u_sized,
    x64_cmp_gt_u_sized,
    x64_cmp_ge_u_sized,
    NULL,
    x64_div_sized_op,
    x64_mod_sized_op,
    x64_udiv_sized_op,
    x64_umod_sized_op,
    NULL, /* emit_binop_sized */
    .emit_cmp_branch = x64_emit_cmp_branch_impl,
    .emit_fp_binop = x64_fp_binop,
    .emit_fp_cmp_branch = x64_fp_cmp_branch,
    .emit_fp_cast_bits = x64_fp_cast_bits,
    .emit_fp_to_acc_bits = x64_fp_to_acc_bits,
    .emit_acc_bits_to_fp_return = x64_acc_bits_to_fp_return,
    .emit_int_to_fp_bits = x64_int_to_fp_bits,
    .emit_fp_bits_to_int = x64_fp_bits_to_int,
    .emit_call_fp_args = x64_call_fp_args,
    .emit_call_saved_fp_args = x64_call_saved_fp_args,
    .emit_load_global_member = x64_load_global_member,
    .emit_load_ptr_indexed = NULL,
    .emit_load_member_ptr_casted = NULL,
    .emit_load_local_casted = x64_load_local_casted,
    .emit_load_local_ptr_member = x64_load_local_ptr_member,
    .emit_load_local_ptr_member_casted = x64_load_local_ptr_member_casted,
    .emit_store_fp_param = x64_store_fp_param,
    .emit_store_local_ptr_member = x64_store_local_ptr_member,
    .emit_store_local_ptr_member_from_local = NULL,
    .emit_va_start = x64_va_start,
    .emit_push_zero = NULL,
    .emit_cmp_branch_imm = NULL,
    .emit_update_local_ptr_member_imm = NULL
};
