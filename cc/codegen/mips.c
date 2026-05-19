#include <stdio.h>

#include "tcc.h"
#include "codegen.h"

/*
 * v84 MIPS backend scaffold
 *
 * This backend emits readable MIPS32-style assembly. It is intentionally a
 * scaffold: it covers the full Codegen interface so the target can be selected,
 * compiled, and inspected, but it has not yet been validated through a MIPS
 * assembler/runtime in the test suite.
 *
 * Register convention used by this scaffold:
 *   $v0  accumulator / return value
 *   $t0  temporary lhs
 *   $t1  saved scratch
 *   $fp  frame pointer
 *   $sp  stack pointer
 *   $ra  return address
 */

static void mips_preamble(void) {
    printf(".text\n");
    printf(".set noreorder\n");
}

static void mips_func_start(const char *name, int is_static) {
    if (!is_static)
        printf(".globl %s\n", name);
    printf("%s:\n", name);
    printf("    addiu $sp, $sp, -8\n");
    printf("    sw $ra, 4($sp)\n");
    printf("    sw $fp, 0($sp)\n");
    printf("    move $fp, $sp\n");
}

static void mips_func_end(void) {
    printf("    move $sp, $fp\n");
    printf("    lw $fp, 0($sp)\n");
    printf("    lw $ra, 4($sp)\n");
    printf("    addiu $sp, $sp, 8\n");
    printf("    jr $ra\n");
    printf("    nop\n");
}

static int align8(int n) {
    return (n + 7) & ~7;
}

static void mips_stack_alloc(int size) {
    size = align8(size);
    if (size > 0)
        printf("    addiu $sp, $sp, -%d\n", size);
}

static void mips_load_imm(int value) {
    printf("    li $v0, %d\n", value);
}

static void mips_push_acc(void) {
    printf("    addiu $sp, $sp, -4\n");
    printf("    sw $v0, 0($sp)\n");
}

static void mips_pop_to_tmp(void) {
    printf("    lw $t0, 0($sp)\n");
    printf("    addiu $sp, $sp, 4\n");
}

static void mips_pop_to_acc(void) {
    printf("    lw $v0, 0($sp)\n");
    printf("    addiu $sp, $sp, 4\n");
}

static void mips_acc_to_arg(int index) {
    if (index >= 0 && index < 4)
        printf("    move $a%d, $v0\n", index);
}

static void mips_acc_to_tmp(void) {
    printf("    move $t0, $v0\n");
}

static void mips_tmp_to_acc(void) {
    printf("    move $v0, $t0\n");
}

static void mips_acc_to_saved(void) {
    printf("    move $t1, $v0\n");
}

static void mips_saved_to_acc(void) {
    printf("    move $v0, $t1\n");
}

static void mips_load_via_saved(int size) {
    if (size == 1)
        printf("    lb $v0, 0($t1)\n");
    else if (size == 2)
        printf("    lh $v0, 0($t1)\n");
    else
        printf("    lw $v0, 0($t1)\n");
}

static void mips_store_via_saved(int size) {
    if (size == 1)
        printf("    sb $v0, 0($t1)\n");
    else if (size == 2)
        printf("    sh $v0, 0($t1)\n");
    else
        printf("    sw $v0, 0($t1)\n");
}

static void mips_add(void) { printf("    addu $v0, $t0, $v0\n"); }
static void mips_sub(void) { printf("    subu $v0, $t0, $v0\n"); }

static void mips_negate(void) {
    printf("    subu $v0, $zero, $v0\n");
}
static void mips_mul(void) { printf("    mul $v0, $t0, $v0\n"); }

static void mips_div_op(void) {
    printf("    div $t0, $v0\n");
    printf("    mflo $v0\n");
}

static void mips_mod_op(void) {
    printf("    div $t0, $v0\n");
    printf("    mfhi $v0\n");
}

static void mips_bitand_op(void) { printf("    and $v0, $t0, $v0\n"); }
static void mips_bitor_op(void) { printf("    or $v0, $t0, $v0\n"); }
static void mips_bitnot_op(void) { printf("    nor $v0, $v0, $zero\n"); }
static void mips_bitxor_op(void) { printf("    xor $v0, $t0, $v0\n"); }
static void mips_shl_op(void) { printf("    sllv $v0, $t0, $v0\n"); }
static void mips_shr_op(void) { printf("    srav $v0, $t0, $v0\n"); }

static void mips_cast_op(int size) {
    if (size == 1)
        printf("    andi $v0, $v0, 255\n");
}

static void mips_ptr_add(int scale) {
    if (scale == 4)
        printf("    sll $v0, $v0, 2\n");
    else if (scale != 1) {
        printf("    li $t1, %d\n", scale);
        printf("    mul $v0, $v0, $t1\n");
    }
    printf("    addu $v0, $t0, $v0\n");
}

static void mips_ptr_sub(int scale) {
    if (scale == 4)
        printf("    sll $v0, $v0, 2\n");
    else if (scale != 1) {
        printf("    li $t1, %d\n", scale);
        printf("    mul $v0, $v0, $t1\n");
    }
    printf("    subu $v0, $t0, $v0\n");
}

static void mips_cmp_eq(void) {
    printf("    xor $v0, $t0, $v0\n");
    printf("    sltiu $v0, $v0, 1\n");
}

static void mips_cmp_ne(void) {
    printf("    xor $v0, $t0, $v0\n");
    printf("    sltu $v0, $zero, $v0\n");
}

static void mips_cmp_lt(void) { printf("    slt $v0, $t0, $v0\n"); }

static void mips_cmp_le(void) {
    printf("    slt $v0, $v0, $t0\n");
    printf("    xori $v0, $v0, 1\n");
}

static void mips_cmp_gt(void) { printf("    slt $v0, $v0, $t0\n"); }

static void mips_cmp_ge(void) {
    printf("    slt $v0, $t0, $v0\n");
    printf("    xori $v0, $v0, 1\n");
}

static void mips_cmp_lt_u(void) {
    printf("    sltu $v0, $t0, $v0\n");
}

static void mips_cmp_gt_u(void) {
    printf("    sltu $v0, $v0, $t0\n");
}

static void mips_cmp_le_u(void) {
    printf("    sltu $v0, $v0, $t0\n");
    printf("    xori $v0, $v0, 1\n");
}

static void mips_cmp_ge_u(void) {
    printf("    sltu $v0, $t0, $v0\n");
    printf("    xori $v0, $v0, 1\n");
}


static void mips_load_local(int offset) {
    printf("    lw $v0, %d($fp)\n", offset);
}

static void mips_store_local(int offset) {
    printf("    sw $v0, %d($fp)\n", offset);
}

static void mips_load_local_sized(int offset, int size) {
    if (size == 1)
        printf("    lbu $v0, %d($fp)\n", offset);
    else
        printf("    lw $v0, %d($fp)\n", offset);
}

static void mips_store_local_sized(int offset, int size) {
    if (size == 1)
        printf("    sb $v0, %d($fp)\n", offset);
    else
        printf("    sw $v0, %d($fp)\n", offset);
}

static void mips_load_global(const char *name, int size) {
    printf("    la $t0, %s\n", name);

    switch (size) {
    case 1:
        printf("    lbu $v0, 0($t0)\n");
        break;
    case 2:
        printf("    lhu $v0, 0($t0)\n");
        break;
    case 4:
        printf("    lw $v0, 0($t0)\n");
        break;
    default:
        ICE("unsupported global load size for MIPS");
        break;
    }
}

static void mips_store_global(const char *name, int size) {
    printf("    la $t0, %s\n", name);

    switch (size) {
    case 1:
        printf("    sb $v0, 0($t0)\n");
        break;
    case 2:
        printf("    sh $v0, 0($t0)\n");
        break;
    case 4:
        printf("    sw $v0, 0($t0)\n");
        break;
    default:
        /*
         * The current MIPS backend appears to use $v0 as the scalar result
         * register, so unsupported sizes should not silently emit a bad store.
         */
        ICE("unsupported global store size for MIPS");
        break;
    }
}

static void mips_load_global_indexed(const char *name, int elem_size) {
    if (elem_size == 4)
        printf("    sll $v0, $v0, 2\n");
    else if (elem_size != 1) {
        printf("    li $t1, %d\n", elem_size);
        printf("    mul $v0, $v0, $t1\n");
    }
    printf("    la $t0, %s\n", name);
    printf("    addu $t0, $t0, $v0\n");
    if (elem_size == 1)
        printf("    lbu $v0, 0($t0)\n");
    else
        printf("    lw $v0, 0($t0)\n");
}

static void mips_store_global_indexed(const char *name, int elem_size) {
    printf("    # store_global_indexed scaffold for %s size %d\n", name, elem_size);
}

static void mips_store_param(int index, int offset) {
    if (index < 4)
        printf("    sw $a%d, %d($fp)\n", index, offset);
    else
        printf("    # stack parameter %d -> %d($fp) scaffold\n", index, offset);
}

static void mips_copy_local(int dst_offset, int src_offset, int size) {
    for (int i = 0; i < size; i += 4) {
        printf("    lw $t0, %d($fp)\n", src_offset + i);
        printf("    sw $t0, %d($fp)\n", dst_offset + i);
    }

}

static void mips_ptr_copy(int size) { (void)size; }

static void mips_copy_local_to_ptr(int ptr_offset, int src_offset, int size) {
    printf("    lw $t1, %d($fp)\n", ptr_offset);
    for (int i = 0; i < size; i += 4) {
        printf("    lw $t0, %d($fp)\n", src_offset + i);
        printf("    sw $t0, %d($t1)\n", i);
    }
}

static void mips_load_indexed(int base_offset, int elem_size) {
    if (elem_size == 4)
        printf("    sll $v0, $v0, 2\n");
    else if (elem_size != 1) {
        printf("    li $t1, %d\n", elem_size);
        printf("    mul $v0, $v0, $t1\n");
    }
    printf("    addu $t0, $fp, $zero\n");
    printf("    addiu $t0, $t0, %d\n", base_offset);
    printf("    addu $t0, $t0, $v0\n");
    if (elem_size == 1)
        printf("    lbu $v0, 0($t0)\n");
    else
        printf("    lw $v0, 0($t0)\n");
}

static void mips_store_indexed(int base_offset, int elem_size) {
    printf("    # store_indexed scaffold base=%d size=%d\n", base_offset, elem_size);
}

static void mips_addr_local(int offset) {
    printf("    addiu $v0, $fp, %d\n", offset);
}

static void mips_addr_indexed(int base_offset, int elem_size) {
    if (elem_size == 4)
        printf("    sll $v0, $v0, 2\n");
    else if (elem_size != 1) {
        printf("    li $t1, %d\n", elem_size);
        printf("    mul $v0, $v0, $t1\n");
    }
    printf("    addiu $t0, $fp, %d\n", base_offset);
    printf("    addu $v0, $t0, $v0\n");
}

static void mips_add_offset(int offset) {
    if (offset)
        printf("    addiu $v0, $v0, %d\n", offset);
}

static void mips_load_ptr_local(int offset) {
    printf("    lw $t0, %d($fp)\n", offset);
    printf("    lw $v0, 0($t0)\n");
}

static void mips_store_ptr_local(int offset) {
    printf("    lw $t0, %d($fp)\n", offset);
    printf("    sw $v0, 0($t0)\n");
}

static void mips_load_deref(int size) {
    if (size == 1)
        printf("    lbu $v0, 0($v0)\n");
    else
        printf("    lw $v0, 0($v0)\n");
}

static void mips_store_deref(int size) {
    printf("    # store_deref scaffold size=%d; address expected in $t0 or stack path\n", size);
    printf("    sw $v0, 0($t0)\n");
}

static void mips_load_member_ptr(int offset, int size) {
    (void)size;
    printf("    lw $v0, %d($v0)\n", offset);
}

static void mips_store_member_ptr(int offset, int size) {
    (void)size;
    printf("    sw $v0, %d($t0)\n", offset);
}

static void mips_string_literal(int label, const char *value) {
    printf(".data\n");
    printf("Lstr%d:\n", label);
    printf("    .asciiz \"");
    for (const unsigned char *p = (const unsigned char *)value; *p; p++) {
        if (*p == '\\' || *p == '"')
            printf("\\%c", *p);
        else if (*p == '\n')
            printf("\\n");
        else if (*p == '\t')
            printf("\\t");
        else
            printf("%c", *p);
    }
    printf("\"\n");
    printf(".text\n");
}

static void mips_load_string(int label) {
    printf("    la $v0, Lstr%d\n", label);
}


static void mips_load_func_addr(const char *name) {
    printf("    la $v0, %s\n", name);
}

static void mips_inline_asm(const char *text) {
    if (!text || !*text)
        return;

    printf("    %s\n", text);
}

static void mips_branch_if_zero(int label) {
    printf("    beq $v0, $zero, L%d\n", label);
    printf("    nop\n");
}

static void mips_branch_if_nonzero(int label) {
    printf("    bne $v0, $zero, L%d\n", label);
    printf("    nop\n");
}

static void mips_branch(int label) {
    printf("    b L%d\n", label);
    printf("    nop\n");
}

static void mips_label(int label) {
    printf("L%d:\n", label);
}

static void mips_emit_label_named_impl(const char *name) {
    printf(".L%s:\n", name);
}

static void mips_emit_branch_named_impl(const char *name) {
    printf("    j .L%s\n", name);
    printf("    nop\n");
}


static void mips_prepare_call_args(int count, int fixed_params) {
    (void)fixed_params;
    printf("    # prepare %d call args scaffold\n", count);
}

static void mips_call(const char *name) {
    printf("    jal %s\n", name);
    printf("    nop\n");
}

static void mips_call_saved(void) {
    printf("    jalr $t1\n");
    printf("    nop\n");
}

static void mips_cleanup_call_args(int count, int fixed_params) {
    (void)fixed_params;
    (void)count;
}

static void mips_load_global_extern(const char *name) {
    mips_load_global(name, 8);  /* extern refs are always pointer-sized */
}
static void mips_store_global_extern(const char *name) {
    mips_store_global(name, 8);
}

Codegen mips_codegen = {
    mips_preamble,
    mips_func_start,
    mips_func_end,
    mips_stack_alloc,
    mips_load_imm,
    mips_push_acc,
    mips_pop_to_tmp,
    mips_pop_to_acc,
    mips_acc_to_arg,
    mips_acc_to_tmp,
    mips_tmp_to_acc,
    mips_acc_to_saved,
    mips_saved_to_acc,
    mips_load_via_saved,
    mips_store_via_saved,
    mips_add,
    mips_sub,
    mips_negate,
    mips_mul,
    mips_div_op,
    mips_mod_op,
    mips_bitand_op,
    mips_bitor_op,
    mips_bitnot_op,
    mips_bitxor_op,
    mips_shl_op,
    mips_shr_op,
    mips_cast_op,
    mips_ptr_add,
    mips_ptr_sub,
    mips_cmp_eq,
    mips_cmp_ne,
    mips_cmp_lt,
    mips_cmp_le,
    mips_cmp_gt,
    mips_cmp_ge,
    mips_load_local,
    mips_store_local,
    mips_load_local_sized,
    mips_store_local_sized,
    mips_load_global,
    mips_load_global_extern,   /* load_global_extern: no GOT needed */
    mips_store_global,
    mips_store_global_extern,  /* store_global_extern: no GOT needed */
    mips_load_global_indexed,
    mips_store_global_indexed,
    mips_store_param,
    mips_copy_local,
    mips_ptr_copy,
    mips_copy_local_to_ptr,
    mips_load_indexed,
    mips_store_indexed,
    mips_addr_local,
    mips_addr_indexed,
    mips_add_offset,
    mips_load_ptr_local,
    mips_store_ptr_local,
    mips_load_deref,
    mips_store_deref,
    mips_load_member_ptr,
    mips_store_member_ptr,
    mips_string_literal,
    mips_load_string,
    mips_load_func_addr,
    mips_inline_asm,
    mips_branch_if_zero,
    mips_branch_if_nonzero,
    mips_branch,
    mips_label,
    mips_prepare_call_args,
    mips_call,
    mips_call_saved,
    mips_cleanup_call_args,
    mips_emit_label_named_impl,
    mips_emit_branch_named_impl,
    NULL,               /* emit_source_loc: not implemented for mips */
    mips_cmp_lt_u,
    mips_cmp_le_u,
    mips_cmp_gt_u,
    mips_cmp_ge_u
};
