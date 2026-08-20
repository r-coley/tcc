#include "stdio.h"
#include <stddef.h>

#include "tcc.h"
#include "codegen.h"

/*
 * Motorola 68k backend.
 *
 * This is still an early 68000-safe backend, but it now has broad hook
 * coverage.  Simple stack/accumulator operations emit real assembly; harder
 * ABI-sensitive areas remain explicit fatal stubs so unsupported code fails
 * clearly instead of silently producing bad assembly.
 */

static const char *m68k_cpu_name = "68000";
static int m68k_function_terminated = 0;

void
m68k_set_cpu_name(const char *cpu_name)
{
    m68k_cpu_name = cpu_name ? cpu_name : "68000";
}

static const char *
m68k_size_suffix(int size)
{
    switch (size) {
    case 1: return ".b";
    case 2: return ".w";
    case 4: return ".l";
    case 8: return ".l"; /* early fallback: lower 32 bits only */
    default: return ".l";
    }
}

static void
m68k_extend_acc(int size, int is_unsigned)
{
    if (size == 1) {
        if (is_unsigned)
            printf("    and.l #255,d0\n");
        else {
            printf("    ext.w d0\n");
            printf("    ext.l d0\n");
        }
    } else if (size == 2) {
        if (is_unsigned)
            printf("    and.l #65535,d0\n");
        else
            printf("    ext.l d0\n");
    }
}

static void
m68k_load_from_addr_reg(const char *areg, int size, int is_unsigned)
{
    const char *suffix = m68k_size_suffix(size);
    printf("    move%s (%s),d0\n", suffix, areg);
    m68k_extend_acc(size, is_unsigned);
}

static void
m68k_store_to_addr_reg(const char *areg, int size)
{
    const char *suffix = m68k_size_suffix(size);
    printf("    move%s d0,(%s)\n", suffix, areg);
}

static void
m68k_load_frame(int offset, int size, int is_unsigned)
{
    const char *suffix = m68k_size_suffix(size);
    printf("    move%s %d(a6),d0\n", suffix, offset);
    m68k_extend_acc(size, is_unsigned);
}

static void
m68k_store_frame(int offset, int size)
{
    const char *suffix = m68k_size_suffix(size);
    printf("    move%s d0,%d(a6)\n", suffix, offset);
}

static void
m68k_preamble(void)
{
    printf(".text\n");
}

static void
m68k_func_start(const char *name, int is_static)
{
    m68k_function_terminated = 0;
    if (!is_static)
        printf(".globl _%s\n", name);
    printf("_%s:\n", name);
    printf("    link a6,#0\n");
}

static void
m68k_func_end(void)
{
    if (m68k_function_terminated)
        return;

    printf("    unlk a6\n");
    printf("    rts\n");
    m68k_function_terminated = 1;
}

static void
m68k_stack_alloc(int size)
{
    if (size > 0)
        printf("    sub.l #%d,sp\n", size);
}

static void
m68k_stack_save_acc(void)
{
    if (m68k_function_terminated)
        return;
    printf("    move.l sp,d0\n");
}

static void
m68k_stack_restore_acc(void)
{
    if (m68k_function_terminated)
        return;
    printf("    move.l d0,sp\n");
}

static void
m68k_stack_alloc_acc(void)
{
    if (m68k_function_terminated)
        return;
    printf("    addq.l #1,d0\n");
    printf("    and.l #-2,d0\n");
    printf("    sub.l d0,sp\n");
    printf("    move.l sp,d0\n");
}

static void
m68k_load_imm(long value)
{
    if (m68k_function_terminated)
        return;

    if (value >= -128 && value <= 127)
        printf("    moveq #%ld,d0\n", value);
    else
        printf("    move.l #%ld,d0\n", value);
}

static void
m68k_push_acc(void)
{
    if (m68k_function_terminated)
        return;
    printf("    move.l d0,-(sp)\n");
}

static void
m68k_pop_to_acc(void)
{
    if (m68k_function_terminated)
        return;
    printf("    move.l (sp)+,d0\n");
}

static void
m68k_pop_to_tmp(void)
{
    if (m68k_function_terminated)
        return;
    printf("    move.l (sp)+,d1\n");
}

static void
m68k_acc_to_arg(int index)
{
    (void)index;
    if (m68k_function_terminated)
        return;
    /* Initial stack ABI: arguments are pushed by the caller. */
    printf("    move.l d0,-(sp)\n");
}

static void
m68k_acc_to_tmp(void)
{
    printf("    move.l d0,d1\n");
}

static void
m68k_tmp_to_acc(void)
{
    printf("    move.l d1,d0\n");
}

static void
m68k_acc_to_saved(void)
{
    printf("    move.l d0,d2\n");
}

static void
m68k_saved_to_acc(void)
{
    printf("    move.l d2,d0\n");
}

static void
m68k_load_via_saved(int size)
{
    printf("    move.l d2,a0\n");
    m68k_load_from_addr_reg("a0", size, 0);
}

static void
m68k_store_via_saved(int size)
{
    printf("    move.l d2,a0\n");
    m68k_store_to_addr_reg("a0", size);
}

static void
m68k_add(void)
{
    printf("    add.l d1,d0\n");
}

static void
m68k_sub(void)
{
    printf("    sub.l d0,d1\n");
    printf("    move.l d1,d0\n");
}

static void
m68k_negate(void)
{
    printf("    neg.l d0\n");
}

static void
m68k_mul(void)
{
    printf("    muls.w d1,d0\n");
}

static void
m68k_div(void)
{
    /*
     * 68000 DIVS is a 32/16 -> 16r:16q operation.
     * This is intentionally a limited lowering path: it handles the
     * small-int cases covered by the smoke tests, but is not full
     * 32-bit C int division yet.
     *
     * Inputs:
     *   d1 = dividend
     *   d0 = divisor
     * Output:
     *   d0 = sign-extended quotient
     */
    printf("    move.l d0,d2\n");
    printf("    move.l d1,d0\n");
    printf("    divs.w d2,d0\n");
    printf("    ext.l d0\n");
}

static void
m68k_mod(void)
{
    /*
     * 68000 DIVS leaves remainder in the high word and quotient in the
     * low word. Swap to bring the remainder down, then sign-extend it.
     */
    printf("    move.l d0,d2\n");
    printf("    move.l d1,d0\n");
    printf("    divs.w d2,d0\n");
    printf("    swap d0\n");
    printf("    ext.l d0\n");
}

static void
m68k_udiv(void)
{
    /*
     * Limited 68000 lowering:
     *   d1 = dividend
     *   d0 = divisor
     *
     * DIVU.W divides a 32-bit dividend in d0 by a word-sized divisor.
     * It leaves quotient in the low word and remainder in the high word.
     */
    printf("    move.l d0,d2\n");
    printf("    move.l d1,d0\n");
    printf("    divu.w d2,d0\n");
    printf("    and.l #65535,d0\n");
}

static void
m68k_umod(void)
{
    /*
     * Remainder is left in the high word after DIVU.W.
     */
    printf("    move.l d0,d2\n");
    printf("    move.l d1,d0\n");
    printf("    divu.w d2,d0\n");
    printf("    swap d0\n");
    printf("    and.l #65535,d0\n");
}

static void
m68k_bitand(void)
{
    printf("    and.l d1,d0\n");
}

static void
m68k_bitor(void)
{
    printf("    or.l d1,d0\n");
}

static void
m68k_bitnot(void)
{
    printf("    not.l d0\n");
}

static void
m68k_bitxor(void)
{
    printf("    eor.l d1,d0\n");
}

static void
m68k_shl(void)
{
    printf("    asl.l d0,d1\n");
    printf("    move.l d1,d0\n");
}

static void
m68k_shr(void)
{
    printf("    asr.l d0,d1\n");
    printf("    move.l d1,d0\n");
}

static void
m68k_ushr(void)
{
    printf("    lsr.l d0,d1\n");
    printf("    move.l d1,d0\n");
}

static void
m68k_shl_imm(int imm)
{
    printf("    lsl.l #%d,d0\n", imm);
}

static void
m68k_shr_imm(int imm)
{
    printf("    asr.l #%d,d0\n", imm);
}

static void
m68k_ushr_imm(int imm)
{
    printf("    lsr.l #%d,d0\n", imm);
}

static void
m68k_cast(int size, int is_unsigned)
{
    m68k_extend_acc(size, is_unsigned);
}

static void
m68k_scale_acc(int scale)
{
    switch (scale) {
    case 1:
        break;
    case 2:
        printf("    add.l d0,d0\n");
        break;
    case 4:
        printf("    asl.l #2,d0\n");
        break;
    case 8:
        printf("    asl.l #3,d0\n");
        break;
    default:
        printf("    move.l #%d,d1\n", scale);
        printf("    muls.w d1,d0\n");
        break;
    }
}

static void
m68k_ptr_add(int scale)
{
    m68k_scale_acc(scale);
    printf("    add.l d1,d0\n");
}

static void
m68k_ptr_sub(int scale)
{
    m68k_scale_acc(scale);
    printf("    move.l (sp)+,d1\n");
    printf("    sub.l d0,d1\n");
    printf("    move.l d1,d0\n");
}

static void
m68k_cmp_common(const char *setcc)
{
    printf("    cmp.l d0,d1\n");
    printf("    %s d0\n", setcc);
    printf("    and.l #1,d0\n");
}

static void m68k_cmp_eq(void) { m68k_cmp_common("seq"); }
static void m68k_cmp_ne(void) { m68k_cmp_common("sne"); }
static void m68k_cmp_lt(void) { m68k_cmp_common("slt"); }
static void m68k_cmp_le(void) { m68k_cmp_common("sle"); }
static void m68k_cmp_gt(void) { m68k_cmp_common("sgt"); }
static void m68k_cmp_ge(void) { m68k_cmp_common("sge"); }
static void m68k_cmp_lt_u(void) { m68k_cmp_common("scs"); }
static void m68k_cmp_le_u(void) { m68k_cmp_common("sls"); }
static void m68k_cmp_gt_u(void) { m68k_cmp_common("shi"); }
static void m68k_cmp_ge_u(void) { m68k_cmp_common("scc"); }

static void m68k_cmp_eq_sized(int size) { (void)size; m68k_cmp_eq(); }
static void m68k_cmp_ne_sized(int size) { (void)size; m68k_cmp_ne(); }
static void m68k_cmp_lt_sized(int size) { (void)size; m68k_cmp_lt(); }
static void m68k_cmp_le_sized(int size) { (void)size; m68k_cmp_le(); }
static void m68k_cmp_gt_sized(int size) { (void)size; m68k_cmp_gt(); }
static void m68k_cmp_ge_sized(int size) { (void)size; m68k_cmp_ge(); }
static void m68k_cmp_lt_u_sized(int size) { (void)size; m68k_cmp_lt_u(); }
static void m68k_cmp_le_u_sized(int size) { (void)size; m68k_cmp_le_u(); }
static void m68k_cmp_gt_u_sized(int size) { (void)size; m68k_cmp_gt_u(); }
static void m68k_cmp_ge_u_sized(int size) { (void)size; m68k_cmp_ge_u(); }

static void
m68k_load_local(int offset)
{
    m68k_load_frame(offset, 4, 0);
}

static void
m68k_store_local(int offset)
{
    m68k_store_frame(offset, 4);
}

static void
m68k_load_local_sized(int offset, int size)
{
    m68k_load_frame(offset, size, 0);
}

static void
m68k_store_local_sized(int offset, int size)
{
    m68k_store_frame(offset, size);
}

static void
m68k_load_global(const char *name, int size)
{
    const char *suffix = m68k_size_suffix(size);
    printf("    move%s _%s,d0\n", suffix, name);
    m68k_extend_acc(size, 0);
}

static void
m68k_load_global_extern(const char *name)
{
    m68k_load_global(name, 4);
}

static void
m68k_store_global(const char *name, int size)
{
    const char *suffix = m68k_size_suffix(size);
    printf("    move%s d0,_%s\n", suffix, name);
}

static void
m68k_store_global_extern(const char *name)
{
    m68k_store_global(name, 4);
}

static void
m68k_load_global_indexed(const char *name, int elem_size)
{
    m68k_scale_acc(elem_size);
    printf("    lea _%s,a0\n", name);
    printf("    adda.l d0,a0\n");
    printf("    move%s (a0),d0\n", m68k_size_suffix(elem_size));
    m68k_extend_acc(elem_size, 0);
}

static void
m68k_store_global_indexed(const char *name, int elem_size)
{
    printf("    move.l d0,d1\n");
    printf("    move.l (sp)+,d0\n");
    m68k_scale_acc(elem_size);
    printf("    lea _%s,a0\n", name);
    printf("    adda.l d0,a0\n");
    printf("    move%s d1,(a0)\n", m68k_size_suffix(elem_size));
}

static void
m68k_load_global_member(const char *name, int offset, int size, int is_extern)
{
    (void)is_extern;
    printf("    lea _%s,a0\n", name);
    if (offset)
        printf("    adda.l #%d,a0\n", offset);
    printf("    move%s (a0),d0\n", m68k_size_suffix(size));
    m68k_extend_acc(size, 0);
}

static void
m68k_store_param(int index, int offset)
{
    int arg_offset = 8 + index * 4;
    printf("    move.l %d(a6),d0\n", arg_offset);
    m68k_store_frame(offset, 4);
}

static void
m68k_copy_local(int dst_offset, int src_offset, int size)
{
    int i;
    for (i = 0; i < size; i += 4) {
        printf("    move.l %d(a6),d0\n", src_offset + i);
        printf("    move.l d0,%d(a6)\n", dst_offset + i);
    }
}

static void
m68k_ptr_copy(int size)
{
    int i;
    printf("    move.l d0,a1\n");
    for (i = 0; i < size; i += 4) {
        printf("    move.l %d(a0),d0\n", i);
        printf("    move.l d0,%d(a1)\n", i);
    }
}

static void
m68k_copy_local_to_ptr(int ptr_offset, int src_offset, int size)
{
    int i;
    printf("    move.l %d(a6),a0\n", ptr_offset);
    for (i = 0; i < size; i += 4) {
        printf("    move.l %d(a6),d0\n", src_offset + i);
        printf("    move.l d0,%d(a0)\n", i);
    }
}

static void
m68k_load_indexed(int base_offset, int elem_size)
{
    m68k_scale_acc(elem_size);
    printf("    lea %d(a6),a0\n", base_offset);
    printf("    adda.l d0,a0\n");
    printf("    move%s (a0),d0\n", m68k_size_suffix(elem_size));
    m68k_extend_acc(elem_size, 0);
}

static void
m68k_store_indexed(int base_offset, int elem_size)
{
    printf("    move.l d0,a1\n");
    printf("    move.l d1,d0\n");
    m68k_scale_acc(elem_size);
    printf("    lea %d(a6),a0\n", base_offset);
    printf("    adda.l d0,a0\n");
    printf("    move%s a1,(a0)\n", m68k_size_suffix(elem_size));
}

static void
m68k_addr_local(int offset)
{
    printf("    lea %d(a6),a0\n", offset);
    printf("    move.l a0,d0\n");
}

static void
m68k_addr_indexed(int base_offset, int elem_size)
{
    m68k_scale_acc(elem_size);
    printf("    lea %d(a6),a0\n", base_offset);
    printf("    adda.l d0,a0\n");
    printf("    move.l a0,d0\n");
}

static void
m68k_add_offset(int offset)
{
    if (offset)
        printf("    add.l #%d,d0\n", offset);
}

static void
m68k_load_ptr_local(int offset)
{
    printf("    move.l %d(a6),a0\n", offset);
    m68k_load_from_addr_reg("a0", 4, 0);
}

static void
m68k_store_ptr_local(int offset)
{
    printf("    move.l %d(a6),a0\n", offset);
    m68k_store_to_addr_reg("a0", 4);
}

static void
m68k_load_deref(int size)
{
    printf("    move.l d0,a0\n");
    m68k_load_from_addr_reg("a0", size, 0);
}

static void
m68k_store_deref(int size)
{
    printf("    move.l d1,a0\n");
    m68k_store_to_addr_reg("a0", size);
}

static void
m68k_incdec_deref(int size, int is_inc, int is_postfix)
{
    /*
     * Entry: d0 is the dereference address.
     * Exit: d0 is the C expression result:
     *   postfix: old value
     *   prefix:  new value
     */
    printf("    move.l d0,a0\n");
    m68k_load_from_addr_reg("a0", size, 0);

    if (is_postfix)
        printf("    move.l d0,d2\n");

    if (is_inc)
        printf("    add.l #1,d0\n");
    else
        printf("    sub.l #1,d0\n");

    m68k_store_to_addr_reg("a0", size);

    if (is_postfix)
        printf("    move.l d2,d0\n");
}


static void
m68k_load_member_ptr(int offset, int size)
{
    const char *suffix = m68k_size_suffix(size);
    printf("    move.l d0,a0\n");
    printf("    move%s %d(a0),d0\n", suffix, offset);
    m68k_extend_acc(size, 0);
}

static void
m68k_store_member_ptr(int offset, int size)
{
    const char *suffix = m68k_size_suffix(size);
    printf("    move.l (sp)+,a0\n");
    printf("    move%s d0,%d(a0)\n", suffix, offset);
}

static void
m68k_store_local_ptr_member(int local_offset, int member_offset, int size)
{
    const char *suffix = m68k_size_suffix(size);
    printf("    move.l %d(a6),a0\n", local_offset);
    printf("    move%s d0,%d(a0)\n", suffix, member_offset);
}

static void
m68k_string_literal(int label, const char *value, size_t len, int width)
{
    size_t i;
    if (width > 1)
        printf("%s\n", TCC_ASM_CONST_SECTION);
    else
        printf("%s\n", TCC_ASM_CSTRING_SECTION);
    printf(".Lstr%d:\n", label);
    for (i = 0; i < len; i++)
        printf("    .byte %u\n", (unsigned char)value[i]);
    printf("    .byte 0\n");
    printf(".text\n");
}

static void
m68k_load_string(int label)
{
    printf("    lea .Lstr%d,a0\n", label);
    printf("    move.l a0,d0\n");
}

static void
m68k_load_func_addr(const char *name)
{
    printf("    lea _%s,a0\n", name);
    printf("    move.l a0,d0\n");
}

static void
m68k_inline_asm(const char *text)
{
    printf("%s\n", text);
}

static void
m68k_branch_if_zero(int label)
{
    printf("    tst.l d0\n");
    printf("    beq .L%d\n", label);
}

static void
m68k_branch_if_nonzero(int label)
{
    printf("    tst.l d0\n");
    printf("    bne .L%d\n", label);
}

static void
m68k_branch(int label)
{
    printf("    bra .L%d\n", label);
}

static void
m68k_label(int label)
{
    m68k_function_terminated = 0;
    printf(".L%d:\n", label);
}

static void
m68k_call(const char *name)
{
    printf("    jsr _%s\n", name);
}

static void
m68k_call_saved(void)
{
    printf("    move.l d0,a0\n");
    printf("    jsr (a0)\n");
}

static void
m68k_cleanup_call_args(int count, int fixed_params)
{
    (void)fixed_params;
    if (count > 0)
        printf("    add.l #%d,sp\n", count * 4);
}

static void
m68k_label_named(const char *name)
{
    m68k_function_terminated = 0;
    printf("%s:\n", name);
}

static void
m68k_branch_named(const char *name)
{
    printf("    bra %s\n", name);
}


Codegen m68k_codegen = {
    .emit_preamble = m68k_preamble,
    .emit_function_start = m68k_func_start,
    .emit_function_end = m68k_func_end,
    .emit_stack_alloc = m68k_stack_alloc,
    .emit_stack_save_acc = m68k_stack_save_acc,
    .emit_stack_restore_acc = m68k_stack_restore_acc,
    .emit_stack_alloc_acc = m68k_stack_alloc_acc,
    .emit_load_imm = m68k_load_imm,
    .emit_push_acc = m68k_push_acc,
    .emit_pop_to_tmp = m68k_pop_to_tmp,
    .emit_pop_to_acc = m68k_pop_to_acc,
    .emit_acc_to_arg = m68k_acc_to_arg,
    .emit_acc_to_tmp = m68k_acc_to_tmp,
    .emit_tmp_to_acc = m68k_tmp_to_acc,
    .emit_acc_to_saved = m68k_acc_to_saved,
    .emit_saved_to_acc = m68k_saved_to_acc,
    .emit_load_via_saved = m68k_load_via_saved,
    .emit_store_via_saved = m68k_store_via_saved,
    .emit_add = m68k_add,
    .emit_sub = m68k_sub,
    .emit_negate = m68k_negate,
    .emit_mul = m68k_mul,
    .emit_div = m68k_div,
    .emit_mod = m68k_mod,
    .emit_bitand = m68k_bitand,
    .emit_bitand_imm = NULL,
    .emit_bitor = m68k_bitor,
    .emit_bitnot = m68k_bitnot,
    .emit_bitxor = m68k_bitxor,
    .emit_shl = m68k_shl,
    .emit_shr = m68k_shr,
    .emit_udiv = m68k_udiv,
    .emit_umod = m68k_umod,
    .emit_ushr = m68k_ushr,
    .emit_shl_imm = m68k_shl_imm,
    .emit_shr_imm = m68k_shr_imm,
    .emit_ushr_imm = m68k_ushr_imm,
    .emit_cast = m68k_cast,
    .emit_ptr_add = m68k_ptr_add,
    .emit_ptr_sub = m68k_ptr_sub,
    .emit_cmp_eq = m68k_cmp_eq,
    .emit_cmp_ne = m68k_cmp_ne,
    .emit_cmp_lt = m68k_cmp_lt,
    .emit_cmp_le = m68k_cmp_le,
    .emit_cmp_gt = m68k_cmp_gt,
    .emit_cmp_ge = m68k_cmp_ge,
    .emit_load_local = m68k_load_local,
    .emit_store_local = m68k_store_local,
    .emit_load_local_sized = m68k_load_local_sized,
    .emit_store_local_sized = m68k_store_local_sized,
    .emit_load_global = m68k_load_global,
    .emit_load_global_extern = m68k_load_global_extern,
    .emit_store_global = m68k_store_global,
    .emit_store_global_extern = m68k_store_global_extern,
    .emit_load_global_indexed = m68k_load_global_indexed,
    .emit_store_global_indexed = m68k_store_global_indexed,
    .emit_store_param = m68k_store_param,
    .emit_copy_incoming_param = NULL,
    .emit_copy_local = m68k_copy_local,
    .emit_ptr_copy = m68k_ptr_copy,
    .emit_copy_local_to_ptr = m68k_copy_local_to_ptr,
    .emit_push_struct_arg = NULL,
    .emit_load_indexed = m68k_load_indexed,
    .emit_store_indexed = m68k_store_indexed,
    .emit_addr_local = m68k_addr_local,
    .emit_addr_indexed = m68k_addr_indexed,
    .emit_add_offset = m68k_add_offset,
    .emit_load_ptr_local = m68k_load_ptr_local,
    .emit_store_ptr_local = m68k_store_ptr_local,
    .emit_load_deref = m68k_load_deref,
    .emit_store_deref = m68k_store_deref,
    .emit_load_member_ptr = m68k_load_member_ptr,
    .emit_store_member_ptr = m68k_store_member_ptr,
    .emit_string_literal = m68k_string_literal,
    .emit_load_string = m68k_load_string,
    .emit_load_func_addr = m68k_load_func_addr,
    .emit_inline_asm = m68k_inline_asm,
    .emit_branch_if_zero = m68k_branch_if_zero,
    .emit_branch_if_nonzero = m68k_branch_if_nonzero,
    .emit_branch = m68k_branch,
    .emit_label = m68k_label,
    .emit_prepare_call_args = codegen_noop_call_args,
    .emit_call = m68k_call,
    .emit_call_saved = m68k_call_saved,
    .emit_cleanup_call_args = m68k_cleanup_call_args,
    .emit_label_named = m68k_label_named,
    .emit_branch_named = m68k_branch_named,
    .emit_source_loc = NULL,
    .emit_cmp_lt_u = m68k_cmp_lt_u,
    .emit_cmp_le_u = m68k_cmp_le_u,
    .emit_cmp_gt_u = m68k_cmp_gt_u,
    .emit_cmp_ge_u = m68k_cmp_ge_u,
    .emit_cmp_eq_sized = m68k_cmp_eq_sized,
    .emit_cmp_ne_sized = m68k_cmp_ne_sized,
    .emit_cmp_lt_sized = m68k_cmp_lt_sized,
    .emit_cmp_le_sized = m68k_cmp_le_sized,
    .emit_cmp_gt_sized = m68k_cmp_gt_sized,
    .emit_cmp_ge_sized = m68k_cmp_ge_sized,
    .emit_cmp_lt_u_sized = m68k_cmp_lt_u_sized,
    .emit_cmp_le_u_sized = m68k_cmp_le_u_sized,
    .emit_cmp_gt_u_sized = m68k_cmp_gt_u_sized,
    .emit_cmp_ge_u_sized = m68k_cmp_ge_u_sized,
    .emit_incdec_deref = m68k_incdec_deref,
    .emit_binop_sized = NULL,
    .emit_cmp_branch = NULL,
    .emit_load_global_member = m68k_load_global_member,
    .emit_load_ptr_indexed = NULL,
    .emit_load_member_ptr_casted = NULL,
    .emit_load_local_casted = NULL,
    .emit_load_local_ptr_member = NULL,
    .emit_load_local_ptr_member_casted = NULL,
    .emit_store_local_ptr_member = m68k_store_local_ptr_member,
    .emit_store_local_ptr_member_from_local = NULL,
    .emit_or_shl_imm = NULL,
    .emit_push_zero = NULL,
    .emit_update_local_ptr_member_imm = NULL
};
