#include <stdio.h>
#include <string.h>

#include "tcc.h"
#include "target.h"
#include "codegen.h"

static AsmDialect current_asm_dialect = ASM_DIALECT_DEFAULT;
static LinkModel current_link_model = LINK_DYNAMIC;

void codegen_set_asm_dialect(AsmDialect d) { current_asm_dialect = d; }
AsmDialect codegen_get_asm_dialect(void) { return current_asm_dialect; }
void codegen_set_link_model(LinkModel m) { current_link_model = m; }
LinkModel codegen_get_link_model(void) { return current_link_model; }
static int use_gas_intel(void) { return current_asm_dialect == ASM_DIALECT_GAS_INTEL; }



/* Debug info state for GAS .file/.loc directives.
 *
 * The x86 backend can emit either NASM-style or GAS Intel syntax.  The
 * .file/.loc directives are only valid for the GAS path, so -g is a no-op
 * for the default NASM dialect until NASM-specific debug directives are
 * added.
 */
static int x86_debug_enabled = 0;
static const char *x86_debug_files[256];
static int x86_debug_file_count = 0;

void x86_set_debug(int enabled) {
    x86_debug_enabled = enabled;
    if (!enabled) {
        x86_debug_file_count = 0;
    }
}

static int x86_get_file_index(const char *filename) {
    if (!filename || !filename[0])
        return 0;

    for (int i = 0; i < x86_debug_file_count; i++) {
        if (x86_debug_files[i] && strcmp(x86_debug_files[i], filename) == 0)
            return i + 1;
    }

    if (x86_debug_file_count < 255) {
        x86_debug_files[x86_debug_file_count] = filename;
        int idx = x86_debug_file_count + 1;
        x86_debug_file_count++;
        printf("    .file %d \"%s\"\n", idx, filename);
        return idx;
    }

    return 0;
}

static void x86_emit_source_loc(const char *file, int line) {
    if (!x86_debug_enabled || !use_gas_intel() || !file || line <= 0)
        return;

    int idx = x86_get_file_index(file);
    if (idx > 0)
        printf("    .loc %d %d 0\n", idx, line);
}

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

static void x86_stack_save_acc(void) {
    printf("    mov eax, esp\n");
}

static void x86_stack_restore_acc(void) {
    printf("    mov esp, eax\n");
}

static void x86_stack_alloc_acc(void) {
    printf("    add eax, 15\n");
    printf("    and eax, -16\n");
    printf("    sub esp, eax\n");
    printf("    mov eax, esp\n");
}

static void x86_load_imm(long value) {
    printf("    mov eax, %ld\n", value);
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

static void x86_udiv_op(void) {
    printf("    mov ecx, eax\n");
    printf("    mov eax, ebx\n");
    printf("    xor edx, edx\n");
    printf("    div ecx\n");
}

static void x86_umod_op(void) {
    printf("    mov ecx, eax\n");
    printf("    mov eax, ebx\n");
    printf("    xor edx, edx\n");
    printf("    div ecx\n");
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

static void x86_ushr_op(void) {
    printf("    mov ecx, eax\n");
    printf("    mov eax, ebx\n");
    printf("    shr eax, cl\n");
}

static void x86_shl_imm_op(int imm) {
    printf("    shl eax, %d\n", imm);
}

static void x86_shr_imm_op(int imm) {
    printf("    sar eax, %d\n", imm);
}

static void x86_ushr_imm_op(int imm) {
    printf("    shr eax, %d\n", imm);
}

static void x86_cast_op(int size, int is_unsigned) {
    if (size == 1) {
        if (is_unsigned)
            printf("    and eax, 255\n");
        else
            printf("    movsx eax, al\n");
    } else if (size == 2) {
        if (is_unsigned)
            printf("    and eax, 65535\n");
        else
            printf("    movsx eax, ax\n");
    }
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
    else if (size == 2)
        printf("    movzx eax, word [ebp%d]\n", offset);
    else
        printf("    mov eax, [ebp%d]\n", offset);
}

static void x86_load_local_casted(int offset, int load_size, int is_unsigned) {
    if (load_size == 1) {
        if (is_unsigned)
            printf("    movzx eax, byte [ebp%d]\n", offset);
        else
            printf("    movsx eax, byte [ebp%d]\n", offset);
    } else if (load_size == 2) {
        if (is_unsigned)
            printf("    movzx eax, word [ebp%d]\n", offset);
        else
            printf("    movsx eax, word [ebp%d]\n", offset);
    } else {
        printf("    mov eax, [ebp%d]\n", offset);
    }
}

static void x86_load_local_ptr_member(int local_offset, int member_offset, int size) {
    printf("    mov ebx, [ebp%d]\n", local_offset);
    if (size == 1)
        printf("    movzx eax, byte [ebx+%d]\n", member_offset);
    else if (size == 2)
        printf("    movzx eax, word [ebx+%d]\n", member_offset);
    else
        printf("    mov eax, [ebx+%d]\n", member_offset);
}

static void x86_load_local_ptr_member_casted(int local_offset, int member_offset,
                                             int load_size, int cast_size, int is_unsigned) {
    (void)load_size;
    printf("    mov ebx, [ebp%d]\n", local_offset);
    if (cast_size == 1) {
        if (is_unsigned)
            printf("    movzx eax, byte [ebx+%d]\n", member_offset);
        else
            printf("    movsx eax, byte [ebx+%d]\n", member_offset);
    } else if (cast_size == 2) {
        if (is_unsigned)
            printf("    movzx eax, word [ebx+%d]\n", member_offset);
        else
            printf("    movsx eax, word [ebx+%d]\n", member_offset);
    } else {
        printf("    mov eax, [ebx+%d]\n", member_offset);
    }
}

static void x86_store_local_sized(int offset, int size) {
    if (size == 1)
        printf("    mov [ebp%d], al\n", offset);
    else if (size == 2)
        printf("    mov [ebp%d], ax\n", offset);
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
    case 8:
        printf("    mov eax, DWORD PTR [%s]\n", name);
        break;
    default:
        ICE("unsupported global load size for x86");
        break;
    }
}

static void x86_load_global_member(const char *name, int offset, int size, int is_extern) {
    (void)is_extern;

    if (offset) {
        if (size == 1)
            printf("    movzx eax, BYTE PTR [%s+%d]\n", name, offset);
        else if (size == 2)
            printf("    movzx eax, WORD PTR [%s+%d]\n", name, offset);
        else
            printf("    mov eax, DWORD PTR [%s+%d]\n", name, offset);
    } else {
        if (size == 1)
            printf("    movzx eax, BYTE PTR [%s]\n", name);
        else if (size == 2)
            printf("    movzx eax, WORD PTR [%s]\n", name);
        else
            printf("    mov eax, DWORD PTR [%s]\n", name);
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
    case 8:
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
    else if (elem_size == 2)
        printf("    movzx eax, word [%s+ebx]\n", name);
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
    else if (elem_size == 2)
        printf("    mov [%s+ebx], ax\n", name);
    else
        printf("    mov [%s+ebx], eax\n", name);
}

static void x86_store_param(int index, int offset) {
    int param_offset = 8 + index * 4;
    printf("    mov eax, [ebp+%d]\n", param_offset);
    printf("    mov [ebp%d], eax\n", offset);
}

static void x86_copy_incoming_param(int stack_offset, int local_offset, int size) {
    int off = 0;
    while (off < size) {
        int chunk = (size - off >= 4) ? 4 : ((size - off >= 2) ? 2 : 1);
        if (chunk == 4) {
            printf("    mov eax, [ebp+%d]\n", stack_offset + off);
            printf("    mov [ebp%d], eax\n", local_offset + off);
        } else if (chunk == 2) {
            printf("    movzx eax, word [ebp+%d]\n", stack_offset + off);
            printf("    mov [ebp%d], ax\n", local_offset + off);
        } else {
            printf("    movzx eax, byte [ebp+%d]\n", stack_offset + off);
            printf("    mov [ebp%d], al\n", local_offset + off);
        }
        off += chunk;
    }
}


static void x86_copy_local(int dst_offset, int src_offset, int size) {
    for (int off = 0; off < size; off += 4) {
        printf("    mov eax, [ebp%d]\n", src_offset + off);
        printf("    mov [ebp%d], eax\n", dst_offset + off);
    }

}

static void x86_ptr_copy(int size) {
    int off = 0;

    while (size - off >= 4) {
        printf("    mov edx, DWORD PTR [eax+%d]\n", off);
        printf("    mov DWORD PTR [ecx+%d], edx\n", off);
        off += 4;
    }
    if (size - off >= 2) {
        printf("    movzx edx, WORD PTR [eax+%d]\n", off);
        printf("    mov WORD PTR [ecx+%d], dx\n", off);
        off += 2;
    }
    if (size - off >= 1) {
        printf("    movzx edx, BYTE PTR [eax+%d]\n", off);
        printf("    mov BYTE PTR [ecx+%d], dl\n", off);
    }
}

static void x86_push_struct_arg(int size) {
    int stack_size = (size + 3) & ~3;
    int off = 0;

    printf("    mov edx, eax\n");
    printf("    sub esp, %d\n", stack_size);
    while (off < size) {
        int chunk = (size - off >= 4) ? 4 : ((size - off >= 2) ? 2 : 1);
        if (chunk == 4) {
            printf("    mov eax, [edx+%d]\n", off);
            printf("    mov [esp+%d], eax\n", off);
        } else if (chunk == 2) {
            printf("    movzx eax, word [edx+%d]\n", off);
            printf("    mov [esp+%d], ax\n", off);
        } else {
            printf("    movzx eax, byte [edx+%d]\n", off);
            printf("    mov [esp+%d], al\n", off);
        }
        off += chunk;
    }
}

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
    else if (elem_size == 2)
        printf("    movzx eax, word [ebp%d+ebx]\n", base_offset);
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
    else if (elem_size == 2)
        printf("    mov [ebp%d+ebx], ax\n", base_offset);
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
    else if (size == 2)
        printf("    movzx eax, word [eax]\n");
    else
        printf("    mov eax, [eax]\n");
}

static void x86_store_deref(int size) {
    if (size == 1)
        printf("    mov [ebx], al\n");
    else if (size == 2)
        printf("    mov [ebx], ax\n");
    else
        printf("    mov [ebx], eax\n");
}

static void x86_load_member_ptr(int offset, int size) {
    if (size == 1)
        printf("    movzx eax, byte [eax+%d]\n", offset);
    else if (size == 2)
        printf("    movzx eax, word [eax+%d]\n", offset);
    else
        printf("    mov eax, [eax+%d]\n", offset);
}

static void x86_store_member_ptr(int offset, int size) {
    if (size == 1)
        printf("    mov [ebx+%d], al\n", offset);
    else if (size == 2)
        printf("    mov [ebx+%d], ax\n", offset);
    else
        printf("    mov [ebx+%d], eax\n", offset);
}

static void x86_store_local_ptr_member(int local_offset, int member_offset, int size) {
    printf("    mov ebx, [ebp%d]\n", local_offset);
    if (size == 1)
        printf("    mov [ebx+%d], al\n", member_offset);
    else if (size == 2)
        printf("    mov [ebx+%d], ax\n", member_offset);
    else
        printf("    mov [ebx+%d], eax\n", member_offset);
}

static void x86_string_literal(int label, const char *value, size_t len, int width) {
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
    if (use_gas_intel())
        printf(".text\n");
}

static void x86_load_string(int label) {
    printf("    mov eax, .Lstr%d\n", label);
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

static void x86_emit_cmp_branch_impl(const char *op, int size, int label) {
    const char *jump = NULL;

    (void)size;

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
        ICE("unsupported x86 compare branch op: %s", op ? op : "<null>");

    printf("    cmp ebx, eax\n");
    printf("    %s L%d\n", jump, label);
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
    x86_load_global(name, 4);  /* extern refs are pointer-sized on 32-bit x86 */
}
static void x86_store_global_extern(const char *name) {
    x86_store_global(name, 4);
}

Codegen x86_codegen = {
    x86_preamble,
    x86_func_start,
    x86_func_end,
    x86_stack_alloc,
    x86_stack_save_acc,
    x86_stack_restore_acc,
    x86_stack_alloc_acc,
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
    NULL,
    x86_bitor_op,
    x86_bitnot_op,
    x86_bitxor_op,
    x86_shl_op,
    x86_shr_op,
    x86_udiv_op,
    x86_umod_op,
    x86_ushr_op,
    x86_shl_imm_op,
    x86_shr_imm_op,
    x86_ushr_imm_op,
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
    x86_copy_incoming_param,
    x86_copy_local,
    x86_ptr_copy,
    x86_copy_local_to_ptr,
    x86_push_struct_arg,
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
    codegen_noop_call_args,
    x86_call,
    x86_call_saved,
    x86_cleanup_call_args,
    x86_emit_label_named_impl,
    x86_emit_branch_named_impl,
    x86_emit_source_loc,
    x86_cmp_lt_u,
    x86_cmp_le_u,
    x86_cmp_gt_u,
    x86_cmp_ge_u,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL, /* emit_binop_sized */
    .emit_cmp_branch = x86_emit_cmp_branch_impl,
    .emit_load_global_member = x86_load_global_member,
    .emit_load_ptr_indexed = NULL,
    .emit_load_member_ptr_casted = NULL,
    .emit_load_local_casted = x86_load_local_casted,
    .emit_load_local_ptr_member = x86_load_local_ptr_member,
    .emit_load_local_ptr_member_casted = x86_load_local_ptr_member_casted,
    .emit_store_local_ptr_member = x86_store_local_ptr_member,
    .emit_store_local_ptr_member_from_local = NULL,
    .emit_push_zero = NULL,
    .emit_cmp_branch_imm = NULL,
    .emit_update_local_ptr_member_imm = NULL
};
