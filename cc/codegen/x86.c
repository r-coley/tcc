#include <stdio.h>

#include "tcc.h"
#include "codegen.h"

static AsmDialect current_asm_dialect = ASM_DIALECT_DEFAULT;
static LinkModel current_link_model = LINK_DYNAMIC;

void codegen_set_asm_dialect(AsmDialect d) { current_asm_dialect = d; }
AsmDialect codegen_get_asm_dialect(void) { return current_asm_dialect; }
void codegen_set_link_model(LinkModel m) { current_link_model = m; }
LinkModel codegen_get_link_model(void) { return current_link_model; }
static int use_gas_intel(void) { return current_asm_dialect == ASM_DIALECT_GAS_INTEL; }


static void x86_preamble(void) {
    if (use_gas_intel()) {
        printf(".intel_syntax noprefix\n");
        printf(".globl main\n");
        printf(".text\n");
    } else {
        printf("global main\n");
        printf("section .text\n");
    }
}

static void x86_func_start(const char *name, int is_static) {
    (void)is_static;
    printf("%s:\n", name);
    printf("    push ebp\n");
    printf("    mov ebp, esp\n");
}

static void x86_func_end(void) {
    printf("    mov esp, ebp\n");
    printf("    pop ebp\n");
    printf("    ret\n");
}

static void x86_stack_alloc(int size) {
    if (size > 0)
        printf("    sub esp, %d\n", size);
}

static void x86_load_imm(int value) {
    printf("    mov eax, %d\n", value);
}

static void x86_push_acc(void) {
    printf("    push eax\n");
}

static void x86_pop_to_tmp(void) {
    printf("    pop ebx\n");
}

static void x86_pop_to_acc(void) {
    printf("    pop eax\n");
}

static void x86_acc_to_tmp(void) {
    printf("    mov ebx, eax\n");
}

static void x86_tmp_to_acc(void) {
    printf("    mov eax, ebx\n");
}

static void x86_acc_to_saved(void) {
    printf("    mov ecx, eax\n");
}

static void x86_saved_to_acc(void) {
    printf("    mov eax, ecx\n");
}

static void x86_load_via_saved(int size) {
    if (size == 1)
        printf("    movsx eax, byte [ecx]\n");
    else
        printf("    mov eax, [ecx]\n");
}

static void x86_store_via_saved(int size) {
    if (size == 1)
        printf("    mov [ecx], al\n");
    else
        printf("    mov [ecx], eax\n");
}

static void x86_add(void) {
    printf("    add eax, ebx\n");
}

static void x86_sub(void) {
    printf("    sub ebx, eax\n");
    printf("    mov eax, ebx\n");
}

static void x86_negate(void) {
    printf("    neg  eax\n");
}

static void x86_mul(void) {
    printf("    imul eax, ebx\n");
}

static void x86_div_op(void) {
    printf("    mov ecx, eax\n");
    printf("    mov eax, ebx\n");
    printf("    cdq\n");
    printf("    idiv ecx\n");
}

static void x86_mod_op(void) {
    printf("    mov ecx, eax\n");
    printf("    mov eax, ebx\n");
    printf("    cdq\n");
    printf("    idiv ecx\n");
    printf("    mov eax, edx\n");
}


static void x86_bitand_op(void) {
    printf("    and eax, ebx\n");
}

static void x86_bitor_op(void) {
    printf("    or eax, ebx\n");
}

static void x86_bitnot_op(void) {
    printf("    not eax\n");
}

static void x86_bitxor_op(void) {
    printf("    xor eax, ebx\n");
}

static void x86_shl_op(void) {
    printf("    mov ecx, eax\n");
    printf("    mov eax, ebx\n");
    printf("    shl eax, cl\n");
}

static void x86_shr_op(void) {
    printf("    mov ecx, eax\n");
    printf("    mov eax, ebx\n");
    printf("    sar eax, cl\n");
}

static void x86_cast_op(int size) {
    if (size == 1)
        printf("    and eax, 255\n");
}

static void x86_ptr_add(int scale) {
    if (scale == 4)
        printf("    shl eax, 2\n");
    else if (scale != 1)
        printf("    imul eax, %d\n", scale);
    printf("    add eax, ebx\n");
}

static void x86_ptr_sub(int scale) {
    if (scale == 4)
        printf("    shl eax, 2\n");
    else if (scale != 1)
        printf("    imul eax, %d\n", scale);
    printf("    sub ebx, eax\n");
    printf("    mov eax, ebx\n");
}

static void x86_cmp_common(const char *op) {
    printf("    cmp ebx, eax\n");
    printf("    %s al\n", op);
    printf("    movzx eax, al\n");
}

static void x86_cmp_eq(void) { x86_cmp_common("sete"); }
static void x86_cmp_ne(void) { x86_cmp_common("setne"); }
static void x86_cmp_lt(void) { x86_cmp_common("setl"); }
static void x86_cmp_le(void) { x86_cmp_common("setle"); }
static void x86_cmp_gt(void) { x86_cmp_common("setg"); }
static void x86_cmp_ge(void) { x86_cmp_common("setge"); }

static void x86_cmp_lt_u(void) { x86_cmp_common("setb"); }
static void x86_cmp_le_u(void) { x86_cmp_common("setbe"); }
static void x86_cmp_gt_u(void) { x86_cmp_common("seta"); }
static void x86_cmp_ge_u(void) { x86_cmp_common("setae"); }


static void x86_load_local(int offset) {
    printf("    mov eax, [ebp%d]\n", offset);
}

static void x86_store_local(int offset) {
    printf("    mov [ebp%d], eax\n", offset);
}

static void x86_load_local_sized(int offset, int size) {
    if (size == 1)
        printf("    movzx eax, byte [ebp%d]\n", offset);
    else
        printf("    mov eax, [ebp%d]\n", offset);
}

static void x86_store_local_sized(int offset, int size) {
    if (size == 1)
        printf("    mov [ebp%d], al\n", offset);
    else
        printf("    mov [ebp%d], eax\n", offset);
}

static void x86_load_global(const char *name, int size) {
    switch (size) {
    case 1:
        printf("    movzx eax, BYTE PTR [%s]\n", name);
        break;
    case 2:
        printf("    movzx eax, WORD PTR [%s]\n", name);
        break;
    case 4:
        printf("    mov eax, DWORD PTR [%s]\n", name);
        break;
    default:
        ICE("unsupported global load size for x86");
        break;
    }
}

static void x86_store_global(const char *name, int size) {
    switch (size) {
    case 1:
        printf("    mov BYTE PTR [%s], al\n", name);
        break;
    case 2:
        printf("    mov WORD PTR [%s], ax\n", name);
        break;
    case 4:
        printf("    mov DWORD PTR [%s], eax\n", name);
        break;
    default:
        ICE("unsupported global store size for x86");
        break;
    }
}

static void x86_load_global_indexed(const char *name, int elem_size) {
    printf("    mov ebx, eax\n");
    if (elem_size == 4)
        printf("    shl ebx, 2\n");
    else if (elem_size != 1)
        printf("    imul ebx, %d\n", elem_size);
    if (elem_size == 1)
        printf("    movzx eax, byte [%s+ebx]\n", name);
    else
        printf("    mov eax, [%s+ebx]\n", name);
}

static void x86_store_global_indexed(const char *name, int elem_size) {
    if (elem_size == 4)
        printf("    shl ebx, 2\n");
    else if (elem_size != 1)
        printf("    imul ebx, %d\n", elem_size);
    if (elem_size == 1)
        printf("    mov [%s+ebx], al\n", name);
    else
        printf("    mov [%s+ebx], eax\n", name);
}

static void x86_store_param(int index, int offset) {
    int param_offset = 8 + index * 4;
    printf("    mov eax, [ebp+%d]\n", param_offset);
    printf("    mov [ebp%d], eax\n", offset);
}


static void x86_copy_local(int dst_offset, int src_offset, int size) {
    for (int off = 0; off < size; off += 4) {
        printf("    mov eax, [ebp%d]\n", src_offset + off);
        printf("    mov [ebp%d], eax\n", dst_offset + off);
    }

}

static void x86_ptr_copy(int size) { (void)size; }

static void x86_copy_local_to_ptr(int ptr_offset, int src_offset, int size) {
    printf("    mov edx, [ebp%d]\n", ptr_offset);
    for (int off = 0; off < size; off += 4) {
        printf("    mov eax, [ebp%d]\n", src_offset + off);
        printf("    mov [edx+%d], eax\n", off);
    }
}

static void x86_load_indexed(int base_offset, int elem_size) {
    printf("    mov ebx, eax\n");
    if (elem_size == 4)
        printf("    shl ebx, 2\n");
    else if (elem_size != 1)
        printf("    imul ebx, %d\n", elem_size);
    if (elem_size == 1)
        printf("    movzx eax, byte [ebp%d+ebx]\n", base_offset);
    else
        printf("    mov eax, [ebp%d+ebx]\n", base_offset);
}

static void x86_store_indexed(int base_offset, int elem_size) {
    if (elem_size == 4)
        printf("    shl ebx, 2\n");
    else if (elem_size != 1)
        printf("    imul ebx, %d\n", elem_size);
    if (elem_size == 1)
        printf("    mov [ebp%d+ebx], al\n", base_offset);
    else
        printf("    mov [ebp%d+ebx], eax\n", base_offset);
}

static void x86_addr_local(int offset) {
    printf("    lea eax, [ebp%d]\n", offset);
}

static void x86_addr_indexed(int base_offset, int elem_size) {
    printf("    mov ebx, eax\n");
    if (elem_size == 4)
        printf("    shl ebx, 2\n");
    else if (elem_size != 1)
        printf("    imul ebx, %d\n", elem_size);
    printf("    lea eax, [ebp%d+ebx]\n", base_offset);
}

static void x86_add_offset(int offset) {
    if (offset)
        printf("    add eax, %d\n", offset);
}

static void x86_load_ptr_local(int offset) {
    printf("    mov eax, [ebp%d]\n", offset);
}

static void x86_store_ptr_local(int offset) {
    printf("    mov [ebp%d], eax\n", offset);
}

static void x86_load_deref(int size) {
    if (size == 1)
        printf("    movzx eax, byte [eax]\n");
    else
        printf("    mov eax, [eax]\n");
}

static void x86_store_deref(int size) {
    if (size == 1)
        printf("    mov [ebx], al\n");
    else
        printf("    mov [ebx], eax\n");
}

static void x86_load_member_ptr(int offset, int size) {
    (void)size;
    printf("    mov eax, [eax+%d]\n", offset);
}

static void x86_store_member_ptr(int offset, int size) {
    (void)size;
    printf("    mov [ebx+%d], eax\n", offset);
}

static void x86_string_literal(int label, const char *value) {
    if (use_gas_intel()) {
        printf(".section .rodata\n");
        printf("Lstr%d: .byte ", label);
        for (const unsigned char *p = (const unsigned char *)value; *p; p++)
            printf("%u, ", *p);
        printf("0\n");
        printf(".text\n");
        return;
    }
    printf("section .rodata\n");
    printf("Lstr%d: db ", label);
    for (const unsigned char *p = (const unsigned char *)value; *p; p++)
        printf("%u, ", *p);
    printf("0\n");
    printf("section .text\n");
}

static void x86_load_string(int label) {
    printf("    mov eax, Lstr%d\n", label);
}


static void x86_load_func_addr(const char *name) {
    printf("    mov eax, %s\n", name);
}

static void x86_inline_asm(const char *text) {
    if (!text || !*text)
        return;

    printf("    %s\n", text);
}

static void x86_branch_if_zero(int label) {
    printf("    cmp eax, 0\n");
    printf("    je L%d\n", label);
}

static void x86_branch_if_nonzero(int label) {
    printf("    cmp eax, 0\n");
    printf("    jne L%d\n", label);
}

static void x86_branch(int label) {
    printf("    jmp L%d\n", label);
}

static void x86_label(int label) {
    printf("L%d:\n", label);
}

static void x86_emit_label_named_impl(const char *name) {
    printf(".L%s:\n", name);
}

static void x86_emit_branch_named_impl(const char *name) {
    printf("    jmp .L%s\n", name);
}


static void x86_prepare_call_args(int count, int fixed_params) {
    (void)fixed_params;
    (void)count;
}

static void x86_call(const char *name) {
    printf("    call %s\n", name);
}

static void x86_call_saved(void) {
    printf("    call ecx\n");
}

static void x86_cleanup_call_args(int count, int fixed_params) {
    (void)fixed_params;
    if (count > 0)
        printf("    add esp, %d\n", count * 4);
}

static void x86_load_global_extern(const char *name) {
    x86_load_global(name, 8);  /* extern refs are always pointer-sized */
}
static void x86_store_global_extern(const char *name) {
    x86_store_global(name, 8);
}

Codegen x86_codegen = {
    x86_preamble,
    x86_func_start,
    x86_func_end,
    x86_stack_alloc,
    x86_load_imm,
    x86_push_acc,
    x86_pop_to_tmp,
    x86_pop_to_acc,
    NULL,
    x86_acc_to_tmp,
    x86_tmp_to_acc,
    x86_acc_to_saved,
    x86_saved_to_acc,
    x86_load_via_saved,
    x86_store_via_saved,
    x86_add,
    x86_sub,
    x86_negate,
    x86_mul,
    x86_div_op,
    x86_mod_op,
    x86_bitand_op,
    x86_bitor_op,
    x86_bitnot_op,
    x86_bitxor_op,
    x86_shl_op,
    x86_shr_op,
    x86_cast_op,
    x86_ptr_add,
    x86_ptr_sub,
    x86_cmp_eq,
    x86_cmp_ne,
    x86_cmp_lt,
    x86_cmp_le,
    x86_cmp_gt,
    x86_cmp_ge,
    x86_load_local,
    x86_store_local,
    x86_load_local_sized,
    x86_store_local_sized,
    x86_load_global,
    x86_load_global_extern,   /* load_global_extern: no GOT needed */
    x86_store_global,
    x86_store_global_extern,  /* store_global_extern: no GOT needed */
    x86_load_global_indexed,
    x86_store_global_indexed,
    x86_store_param,
    x86_copy_local,
    x86_ptr_copy,
    x86_copy_local_to_ptr,
    x86_load_indexed,
    x86_store_indexed,
    x86_addr_local,
    x86_addr_indexed,
    x86_add_offset,
    x86_load_ptr_local,
    x86_store_ptr_local,
    x86_load_deref,
    x86_store_deref,
    x86_load_member_ptr,
    x86_store_member_ptr,
    x86_string_literal,
    x86_load_string,
    x86_load_func_addr,
    x86_inline_asm,
    x86_branch_if_zero,
    x86_branch_if_nonzero,
    x86_branch,
    x86_label,
    x86_prepare_call_args,
    x86_call,
    x86_call_saved,
    x86_cleanup_call_args,
    x86_emit_label_named_impl,
    x86_emit_branch_named_impl,
    NULL,               /* emit_source_loc: not implemented for x86 */
    x86_cmp_lt_u,
    x86_cmp_le_u,
    x86_cmp_gt_u,
    x86_cmp_ge_u
};
