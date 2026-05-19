#include <stdio.h>
#include <string.h>

#include "tcc.h"
#include "codegen.h"

static void emit_load_imm64(const char *reg, long value) {
    unsigned long v = (unsigned long)value;

    unsigned long h0 = (v >> 0)  & 0xffff;
    unsigned long h1 = (v >> 16) & 0xffff;
    unsigned long h2 = (v >> 32) & 0xffff;
    unsigned long h3 = (v >> 48) & 0xffff;

    printf("    movz %s, #%lu\n", reg, h0);

    if (h1)
        printf("    movk %s, #%lu, lsl #16\n", reg, h1);

    if (h2)
        printf("    movk %s, #%lu, lsl #32\n", reg, h2);

    if (h3)
        printf("    movk %s, #%lu, lsl #48\n", reg, h3);
}

/* Debug info state for .loc/.file directives */
static int arm64_debug_enabled = 0;
static const char *arm64_debug_files[256];
static int arm64_debug_file_count = 0;

void arm64_set_debug(int enabled) {
    arm64_debug_enabled = enabled;
}

static int arm64_get_file_index(const char *filename) {
    if (!filename || !filename[0])
        return 0;
    for (int i = 0; i < arm64_debug_file_count; i++) {
        if (arm64_debug_files[i] && strcmp(arm64_debug_files[i], filename) == 0)
            return i + 1;
    }
    if (arm64_debug_file_count < 255) {
        arm64_debug_files[arm64_debug_file_count] = filename;
        int idx = arm64_debug_file_count + 1;
        arm64_debug_file_count++;
        /* Emit the .file directive now */
        printf("    .file %d \"%s\"\n", idx, filename);
        return idx;
    }
    return 0;
}

static void arm64_emit_source_loc(const char *file, int line) {
    if (!arm64_debug_enabled || !file || line <= 0)
        return;
    int idx = arm64_get_file_index(file);
    if (idx > 0)
        printf("    .loc %d %d 0\n", idx, line);
}

static void arm64_preamble(void) {
    printf(".text\n");
}

static void arm64_func_start(const char *name, int is_static) {
    printf(".align 2\n");
    if (!is_static)
        printf(".global _%s\n", name);
    printf("_%s:\n", name);
    printf("    stp x29, x30, [sp, #-16]!\n");
    printf("    mov x29, sp\n");
}

static void arm64_func_end(void) {
    printf("    mov sp, x29\n");
    printf("    ldp x29, x30, [sp], #16\n");
    printf("    ret\n");
}

static int align16(int n) {
    return (n + 15) & ~15;
}

static void arm64_stack_alloc(int size) {
    size = align16(size);
    if (size <= 4095) {
     	printf("    sub sp, sp, #%d\n", size);
    } else {
	emit_load_imm64("x9",size);
     	printf("    sub sp, sp, x9\n");
    }
}

static void arm64_load_imm(int value) {
    emit_load_imm64("x0", value);
}

static void arm64_push_acc(void) {
    printf("    str x0, [sp, #-16]!\n");
}

static void arm64_pop_to_tmp(void) {
    printf("    ldr x1, [sp], #16\n");
}

static void arm64_pop_to_acc(void) {
    printf("    ldr x0, [sp], #16\n");
}

static void arm64_acc_to_arg(int index) {
    if (index == 0)
        return; /* accumulator is already x0 */
    if (index > 0 && index < 8)
        printf("    mov x%d, x0\n", index);
}

static void arm64_acc_to_tmp(void) {
    printf("    mov x1, x0\n");
}

static void arm64_tmp_to_acc(void) {
    printf("    mov x0, x1\n");
}

static void arm64_acc_to_saved(void) {
    printf("    mov x16, x0\n");
}

static void arm64_saved_to_acc(void) {
    printf("    mov x0, x16\n");
}

static void arm64_load_via_saved(int size) {
    if (size == 1)
        printf("    ldrb w0, [x16]\n");
    else if (size == 2)
        printf("    ldrh w0, [x16]\n");
    else if (size == 4)
        printf("    ldr w0, [x16]\n");
    else
        printf("    ldr x0, [x16]\n");
}

static void arm64_store_via_saved(int size) {
    /* arm64_acc_to_saved() saves the lvalue address in x16.
     * Store back through the same saved register.  Older code used x2 here,
     * which only worked accidentally when x2 still happened to contain the
     * address; ordinary indexed lvalue updates such as t[i]++ leave x2 stale
     * and can crash the generated program.
     */
    if (size == 1)
        printf("    strb w0, [x16]\n");
    else if (size == 2)
        printf("    strh w0, [x16]\n");
    else if (size == 4)
        printf("    str w0, [x16]\n");
    else
        printf("    str x0, [x16]\n");
}

static void arm64_add(void) {
    printf("    add x0, x1, x0\n");
}

static void arm64_sub(void) {
    printf("    sub x0, x1, x0\n");
}

static void arm64_negate(void) {
    printf("    neg x0, x0\n");
}

static void arm64_mul(void) {
    printf("    mul x0, x1, x0\n");
}

static void arm64_div_op(void) {
    printf("    sdiv x0, x1, x0\n");
}

static void arm64_mod_op(void) {
    /* remainder = dividend - (dividend / divisor) * divisor */
    printf("    sdiv x2, x1, x0\n");
    printf("    msub x0, x2, x0, x1\n");
}


static void arm64_bitand_op(void) {
    printf("    and x0, x1, x0\n");
}

static void arm64_bitor_op(void) {
    printf("    orr x0, x1, x0\n");
}

static void arm64_bitnot_op(void) {
    printf("    mvn x0, x0\n");
}

static void arm64_bitxor_op(void) {
    printf("    eor x0, x1, x0\n");
}

static void arm64_shl_op(void) {
    printf("    lsl x0, x1, x0\n");
}

static void arm64_shr_op(void) {
    printf("    asr x0, x1, x0\n");
}

static void arm64_cast_op(int size) {
    if (size == 1)
        printf("    and x0, x0, #255\n");
}

static void arm64_ptr_add(int scale) {
    /* x0 = index (right operand), x1 = base (left operand saved by acc_to_tmp)
     * Compute: x0 = base + index * scale */
    printf("    sxtw x0, w0\n");
    if (scale == 4)
        printf("    lsl x0, x0, #2\n");
    else if (scale != 1) {
        emit_load_imm64("x2", scale);
        printf("    mul x0, x0, x2\n");
    }
    printf("    add x0, x1, x0\n");
}

static void arm64_ptr_sub(int scale) {
    printf("    sxtw x0, w0\n");
    if (scale == 4)
        printf("    lsl x0, x0, #2\n");
    else if (scale != 1) {
        emit_load_imm64("x2", scale);
        printf("    mul x0, x0, x2\n");
    }
    printf("    sub x0, x1, x0\n");
}

static void arm64_cmp_common(const char *cond) {
    printf("    cmp w1, w0\n");
    printf("    cset w0, %s\n", cond);
}

static void arm64_cmp_eq(void) { arm64_cmp_common("eq"); }
static void arm64_cmp_ne(void) { arm64_cmp_common("ne"); }
static void arm64_cmp_lt(void) { arm64_cmp_common("lt"); }
static void arm64_cmp_le(void) { arm64_cmp_common("le"); }
static void arm64_cmp_gt(void) { arm64_cmp_common("gt"); }
static void arm64_cmp_ge(void) { arm64_cmp_common("ge"); }

static void arm64_cmp_lt_u(void) { arm64_cmp_common("lo"); }
static void arm64_cmp_le_u(void) { arm64_cmp_common("ls"); }
static void arm64_cmp_gt_u(void) { arm64_cmp_common("hi"); }
static void arm64_cmp_ge_u(void) { arm64_cmp_common("hs"); }


static int arm64_simm9(int offset) {
    return offset >= -256 && offset <= 255;
}

static void emit_add_imm64(const char *dst, const char *src, long value) {
    if (value == 0) {
        if (STRCMP(dst, src) != 0)
            printf("    mov %s, %s\n", dst, src);
        return;
    }

    if (value > 0 && value <= 4095) {
        printf("    add %s, %s, #%ld\n", dst, src, value);
        return;
    }

    if (value < 0 && -value <= 4095) {
        printf("    sub %s, %s, #%ld\n", dst, src, -value);
        return;
    }

    emit_load_imm64("x9", value);
    printf("    add %s, %s, x9\n", dst, src);
}


static void emit_frame_offset_addr(int offset) {
    emit_add_imm64("x9", "x29", offset);
}

static void arm64_load_local_sized(int offset, int size) {
    if (arm64_simm9(offset)) {
        if (size == 1)
            printf("    ldrb w0, [x29, #%d]\n", offset);
        else if (size == 2)
            printf("    ldrh w0, [x29, #%d]\n", offset);
        else if (size == 8)
            printf("    ldr x0, [x29, #%d]\n", offset);
        else
            /* ldrsw: sign-extends 32-bit load into 64-bit x0 register.
             * This is critical for passing int values to long parameters. */
            printf("    ldrsw x0, [x29, #%d]\n", offset);
        return;
    }

    emit_frame_offset_addr(offset);
    if (size == 1)
        printf("    ldrb w0, [x9]\n");
    else if (size == 2)
        printf("    ldrh w0, [x9]\n");
    else if (size == 8)
        printf("    ldr x0, [x9]\n");
    else
        printf("    ldrsw x0, [x9]\n");
}

static void arm64_store_local_sized(int offset, int size) {
    if (arm64_simm9(offset)) {
        if (size == 1)
            printf("    strb w0, [x29, #%d]\n", offset);
        else if (size == 2)
            printf("    strh w0, [x29, #%d]\n", offset);
        else if (size == 8)
            printf("    str x0, [x29, #%d]\n", offset);
        else
            printf("    str w0, [x29, #%d]\n", offset);
        return;
    }

    emit_frame_offset_addr(offset);
    if (size == 1)
        printf("    strb w0, [x9]\n");
    else if (size == 2)
        printf("    strh w0, [x9]\n");
    else if (size == 8)
        printf("    str x0, [x9]\n");
    else
        printf("    str w0, [x9]\n");
}

static void arm64_load_local(int offset) {
    arm64_load_local_sized(offset, 4);
}

static void arm64_store_local(int offset) {
    arm64_store_local_sized(offset, 4);
}


static int arm64_symbol_needs_got(const char *name) {
    /*
     * Mach-O imported data symbols do not have a link-time address for
     * direct @PAGE/@PAGEOFF addressing. They must be reached through
     * the GOT. Most external globals are marked in IR, but stdio macros
     * such as stderr -> __stderrp can still reach the plain global-load
     * path in complex lowered code, so keep the backend safe here too.
     */
    if (!name)
        return 0;
    return strcmp(name, "__stderrp") == 0 ||
           strcmp(name, "__stdoutp") == 0 ||
           strcmp(name, "__stdinp") == 0;
}

static void arm64_load_global_address_reg(const char *reg, const char *name) {
    if (arm64_symbol_needs_got(name)) {
        printf("    adrp %s, _%s@GOTPAGE\n", reg, name);
        printf("    ldr %s, [%s, _%s@GOTPAGEOFF]\n", reg, reg, name);
    } else {
        printf("    adrp %s, _%s@PAGE\n", reg, name);
        printf("    add %s, %s, _%s@PAGEOFF\n", reg, reg, name);
    }
}

static void arm64_load_global(const char *name, int size) {
    arm64_load_global_address_reg("x1", name);
    if (size == 1)
        printf("    ldrb w0, [x1]\n");
    else if (size == 2)
        printf("    ldrh w0, [x1]\n");
    else if (size == 8)
        printf("    ldr x0, [x1]\n");
    else
        printf("    ldrsw x0, [x1]\n");
}

static void arm64_load_global_extern(const char *name) {
    /* External dylib symbols need GOT in dynamic binaries; direct in static. */
    if (codegen_get_link_model() == LINK_DYNAMIC) {
        printf("    adrp x1, _%s@GOTPAGE\n", name);
        printf("    ldr x1, [x1, _%s@GOTPAGEOFF]\n", name);
    } else {
        printf("    adrp x1, _%s@PAGE\n", name);
        printf("    add x1, x1, _%s@PAGEOFF\n", name);
    }
    printf("    ldr x0, [x1]\n");  /* extern is always pointer-sized */
}

static void arm64_store_global(const char *name, int size) {
    arm64_load_global_address_reg("x1", name);
    if (size == 1)
        printf("    strb w0, [x1]\n");
    else if (size == 2)
        printf("    strh w0, [x1]\n");
    else if (size == 8)
        printf("    str x0, [x1]\n");
    else
        printf("    str w0, [x1]\n");
}

static void arm64_store_global_extern(const char *name) {
    if (codegen_get_link_model() == LINK_DYNAMIC) {
        printf("    adrp x1, _%s@GOTPAGE\n", name);
        printf("    ldr x1, [x1, _%s@GOTPAGEOFF]\n", name);
    } else {
        printf("    adrp x1, _%s@PAGE\n", name);
        printf("    add x1, x1, _%s@PAGEOFF\n", name);
    }
    printf("    str x0, [x1]\n");  /* extern is always pointer-sized */
}

static void arm64_load_global_indexed(const char *name, int elem_size) {
    printf("    sxtw x0, w0\n");
    if (elem_size == 8)
        printf("    lsl x0, x0, #3\n");
    else if (elem_size == 4)
        printf("    lsl x0, x0, #2\n");
    else if (elem_size != 1) {
        emit_load_imm64("x3", elem_size);
        printf("    mul x0, x0, x3\n");
    }
    arm64_load_global_address_reg("x2", name);
    if (elem_size == 1)
        printf("    ldrb w0, [x2, x0]\n");
    else if (elem_size == 8)
        printf("    ldr x0, [x2, x0]\n");
    else
        printf("    ldrsw x0, [x2, x0]\n");
}

static void arm64_store_global_indexed(const char *name, int elem_size) {
    printf("    sxtw x1, w1\n");
    if (elem_size == 8)
        printf("    lsl x1, x1, #3\n");
    else if (elem_size == 4)
        printf("    lsl x1, x1, #2\n");
    else if (elem_size != 1) {
        emit_load_imm64("x3", elem_size);
        printf("    mul x1, x1, x3\n");
    }
    arm64_load_global_address_reg("x2", name);
    if (elem_size == 1)
        printf("    strb w0, [x2, x1]\n");
    else if (elem_size == 8)
        printf("    str x0, [x2, x1]\n");
    else
        printf("    str w0, [x2, x1]\n");
}

static void arm64_store_param(int index, int offset) {
    if (index < 8) {
        if (arm64_simm9(offset)) {
            printf("    str x%d, [x29, #%d]\n", index, offset);
        } else {
            emit_frame_offset_addr(offset);
            printf("    str x%d, [x9]\n", index);
        }
    } else {
        int stack_offset = 16 + (index - 8) * 16;
        if (arm64_simm9(stack_offset))
            printf("    ldr x0, [x29, #%d]\n", stack_offset);
        else {
            emit_frame_offset_addr(stack_offset);
            printf("    ldr x0, [x9]\n");
        }
        arm64_store_local_sized(offset, 8);
    }
}

static void arm64_copy_local(int dst_offset, int src_offset, int size) {
    for (int off = 0; off < size; off += 4) {
        arm64_load_local_sized(src_offset + off, 4);
        arm64_store_local_sized(dst_offset + off, 4);
    }
}

static void arm64_copy_local_to_ptr(int ptr_offset, int src_offset, int size) {
    if (arm64_simm9(ptr_offset))
        printf("    ldr x2, [x29, #%d]\n", ptr_offset);
    else {
        emit_frame_offset_addr(ptr_offset);
        printf("    ldr x2, [x9]\n");
    }

    for (int off = 0; off < size; off += 4) {
        arm64_load_local_sized(src_offset + off, 4);
        printf("    str w0, [x2, #%d]\n", off);
    }
}

static void __attribute__((unused)) arm64_copy_deref_large(int size) {
    /* Copy `size` bytes from src (x0=acc) to dst (x1=tmp), word by word.
     * Uses x2=dst, x3=src as stable base registers across the loop. */
    printf("    mov x2, x1\n");   /* dst */
    printf("    mov x3, x0\n");   /* src */
    for (int off = 0; off < size; off += 4) {
        if (size - off >= 8 && (off % 8) == 0) {
            printf("    ldr x0, [x3, #%d]\n", off);
            printf("    str x0, [x2, #%d]\n", off);
            off += 4; /* extra 4, loop adds 4 = 8 total */
        } else {
            printf("    ldr w0, [x3, #%d]\n", off);
            printf("    str w0, [x2, #%d]\n", off);
        }
    }
}

static void arm64_ptr_copy(int size) {
    /* Copy `size` bytes from src (x0=acc) to dst (saved reg x2 via acc_to_saved).
     * x2=dst (already saved by acc_to_saved), x3=src */
    printf("    mov x3, x0\n");   /* save src (acc) */
    printf("    mov x2, x16\n"); /* dst saved by acc_to_saved */
    int off = 0;
    while (off + 8 <= size) {
        printf("    ldr x0, [x3, #%d]\n", off);
        printf("    str x0, [x2, #%d]\n", off);
        off += 8;
    }
    while (off < size) {
        printf("    ldr w0, [x3, #%d]\n", off);
        printf("    str w0, [x2, #%d]\n", off);
        off += 4;
    }
}

static void arm64_load_indexed(int base_offset, int elem_size) {
    printf("    sxtw x0, w0\n");
    if (elem_size == 8)
        printf("    lsl x0, x0, #3\n");
    else if (elem_size == 4)
        printf("    lsl x0, x0, #2\n");
    else if (elem_size == 2)
        printf("    lsl x0, x0, #1\n");
    else if (elem_size != 1) {
        emit_load_imm64("x3", elem_size);
        printf("    mul x0, x0, x3\n");
    }
    emit_frame_offset_addr(base_offset);
    printf("    mov x2, x9\n");
    if (elem_size == 1)
        printf("    ldrb w0, [x2, x0]\n");
    else if (elem_size == 2)
        printf("    ldrh w0, [x2, x0]\n");
    else if (elem_size == 8)
        printf("    ldr x0, [x2, x0]\n");
    else
        printf("    ldrsw x0, [x2, x0]\n");
}

static void arm64_store_indexed(int base_offset, int elem_size) {
    printf("    sxtw x1, w1\n");
    if (elem_size == 8)
        printf("    lsl x1, x1, #3\n");
    else if (elem_size == 4)
        printf("    lsl x1, x1, #2\n");
    else if (elem_size == 2)
        printf("    lsl x1, x1, #1\n");
    else if (elem_size != 1) {
        emit_load_imm64("x3", elem_size);
        printf("    mul x1, x1, x3\n");
    }
    emit_frame_offset_addr(base_offset);
    printf("    mov x2, x9\n");
    if (elem_size == 1)
        printf("    strb w0, [x2, x1]\n");
    else if (elem_size == 2)
        printf("    strh w0, [x2, x1]\n");
    else if (elem_size == 8)
        printf("    str x0, [x2, x1]\n");
    else
        printf("    str w0, [x2, x1]\n");
}

static void arm64_addr_local(int offset) {
    emit_frame_offset_addr(offset);
    printf("    mov x0, x9\n");
}

static void arm64_addr_indexed(int base_offset, int elem_size) {
    printf("    sxtw x0, w0\n");
    if (elem_size == 8)
        printf("    lsl x0, x0, #3\n");
    else if (elem_size == 4)
        printf("    lsl x0, x0, #2\n");
    else if (elem_size == 2)
        printf("    lsl x0, x0, #1\n");
    else if (elem_size != 1) {
        emit_load_imm64("x3", elem_size);
        printf("    mul x0, x0, x3\n");
    }
    emit_frame_offset_addr(base_offset);
    printf("    mov x2, x9\n");
    printf("    add x0, x2, x0\n");
}

static void arm64_add_offset(int offset) {
    emit_add_imm64("x0", "x0", offset);
}

static void arm64_load_ptr_local(int offset) {
    arm64_load_local_sized(offset, 8);
}

static void arm64_store_ptr_local(int offset) {
    arm64_store_local_sized(offset, 8);
}

static void arm64_load_deref(int size) {
    if (size == 1)
        printf("    ldrb w0, [x0]\n");
    else if (size == 2)
        printf("    ldrh w0, [x0]\n");
    else if (size == 8)
        printf("    ldr x0, [x0]\n");
    else
        printf("    ldrsw x0, [x0]\n");
}

static void arm64_store_deref(int size) {
    if (size == 1)
        printf("    strb w0, [x1]\n");
    else if (size == 2)
        printf("    strh w0, [x1]\n");
    else if (size == 8)
        printf("    str x0, [x1]\n");
    else
        printf("    str w0, [x1]\n");
}

static void arm64_load_member_ptr(int offset, int size) {
    if (arm64_simm9(offset)) {
        if (size == 1)
            printf("    ldrb w0, [x0, #%d]\n", offset);
        else if (size == 2)
            printf("    ldrh w0, [x0, #%d]\n", offset);
        else if (size == 8)
            printf("    ldr x0, [x0, #%d]\n", offset);
        else
            printf("    ldr w0, [x0, #%d]\n", offset);
    } else {
        emit_load_imm64("x9", offset);
        if (size == 8)
            printf("    ldr x0, [x0, x9]\n");
        else
            printf("    ldr w0, [x0, x9]\n");
    }
}

static void arm64_store_member_ptr(int offset, int size) {
    if (arm64_simm9(offset)) {
        if (size == 1)
            printf("    strb w0, [x1, #%d]\n", offset);
        else if (size == 2)
            printf("    strh w0, [x1, #%d]\n", offset);
        else if (size == 8)
            printf("    str x0, [x1, #%d]\n", offset);
        else
            printf("    str w0, [x1, #%d]\n", offset);
    } else {
        emit_load_imm64("x9", offset);
        if (size == 8)
            printf("    str x0, [x1, x9]\n");
        else
            printf("    str w0, [x1, x9]\n");
    }
}

static void arm64_string_literal(int label, const char *value) {
    printf(".section __TEXT,__cstring,cstring_literals\n");
    printf("Lstr%d:\n", label);
    printf("    .asciz \"");
    for (const unsigned char *p = (const unsigned char *)value; *p; p++) {
        if (*p == '\\' || *p == '\"')
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

static void arm64_load_string(int label) {
    printf("    adrp x0, Lstr%d@PAGE\n", label);
    printf("    add  x0, x0, Lstr%d@PAGEOFF\n", label);
}


static void arm64_load_func_addr(const char *name) {
    printf("    adrp x0, _%s@PAGE\n", name);
    printf("    add  x0, x0, _%s@PAGEOFF\n", name);
}

static void arm64_inline_asm(const char *text) {
    if (!text || !*text)
        return;

    printf("    %s\n", text);
}

static void arm64_branch_if_zero(int label) {
    printf("    cmp w0, #0\n");
    printf("    b.eq L%d\n", label);
}

static void arm64_branch_if_nonzero(int label) {
    printf("    cmp w0, #0\n");
    printf("    b.ne L%d\n", label);
}

static void arm64_branch(int label) {
    printf("    b L%d\n", label);
}

static void arm64_label(int label) {
    printf("L%d:\n", label);
}

static void arm64_emit_label_named_impl(const char *name) {
    printf("L%s:\n", name);
}

static void arm64_emit_branch_named_impl(const char *name) {
    printf("    b L%s\n", name);
}


/*
 * ARM64 ABI note:
 *
 * SP must be 16-byte aligned at every BL instruction.
 *
 * This backend uses 16-byte temporary expression-stack slots while lowering
 * arguments:
 *
 *     str x0, [sp, #-16]!
 *
 * prepare_call_args() loads register arguments out of those temporary slots.
 * For macOS arm64 variadic calls, however, unnamed arguments are passed on
 * the real ABI stack in packed 8-byte slots.  Therefore, after advancing
 * past the fixed parameters, we repack the remaining 16-byte temporary slots
 * into 8-byte ABI slots in place before emitting BL.
 *
 * cleanup_call_args() still frees the original 16-byte temporary allocation.
 */
static void arm64_prepare_call_args(int count, int fixed_params) {
    /* macOS arm64 calling convention:
     *
     * Internal temporary layout (16 bytes per slot):
     *   [sp + 0]        = arg0 (first arg)
     *   [sp + 16]       = arg1
     *   [sp + i*16]     = arg_i
     *
     * Non-variadic (fixed_params == -1):
     *   Args 0-7 go in x0-x7 (registers).
     *   Args 8+ remain on the stack.
     *
     * Variadic (fixed_params >= 0):
     *   Fixed named params go in x0..x(fixed-1).
     *   Unnamed variadic args must be packed at [sp], [sp+8], ...
     *   for the callee after the fixed params have been consumed.
     */
    if (count == 0) return;

    int is_variadic = (fixed_params >= 0);
    int fixed = is_variadic ? fixed_params : 8;
    if (fixed > count) fixed = count;

    /* Load fixed/register params into x0..x(fixed-1) */
    for (int i = 0; i < fixed && i < 8; i++)
        printf("    ldr x%d, [sp, #%d]\n", i, i * 16);

    /* Advance sp past the fixed/register args. */
    int advance = fixed * 16;
    if (advance > 0) {
        if (advance <= 4095)
            printf("    add sp, sp, #%d\n", advance);
        else {
            emit_load_imm64("x9", advance);
            printf("    add sp, sp, x9\n");
        }
    }

    /*
     * Now [sp] is arg(fixed), the first variadic arg for variadic calls.
     * Our internal slots are still 16 bytes wide:
     *
     *   [sp + 0]   = vararg0
     *   [sp + 16]  = vararg1
     *
     * macOS arm64 va_arg expects packed 8-byte stack slots:
     *
     *   [sp + 0]   = vararg0
     *   [sp + 8]   = vararg1
     *
     * Repack in-place from low to high addresses.  This is safe because the
     * destination for slot i is never above its source.
     */
    if (is_variadic && count > fixed) {
        int var_count = count - fixed;

        for (int i = 0; i < var_count; i++) {
            printf("    ldr x9, [sp, #%d]\n", i * 16);
            printf("    str x9, [sp, #%d]\n", i * 8);
        }
    }
}


static void arm64_cleanup_call_args(int count, int fixed_params) {
    /* Remove overflow/variadic args from stack after call.
     * prepare_call_args already advanced sp past 'fixed' (or 8) register args.
     * The remaining args (overflow for non-variadic, variadic for variadic) are at [sp].
     */
    if (count <= 0) return;

    int is_variadic = (fixed_params >= 0);
    int fixed = is_variadic ? fixed_params : 8;
    if (fixed > count) fixed = count;

    int remaining = count - fixed;
    if (remaining > 0) {
        int size = remaining * 16;
        if (size <= 4095)
            printf("    add sp, sp, #%d\n", size);
        else {
            emit_load_imm64("x9", size);
            printf("    add sp, sp, x9\n");
        }
    }
}


static void arm64_call(const char *name) {
    printf("    bl _%s\n", name);
}

static void arm64_call_saved(void) {
    printf("    blr x16\n");
}

Codegen arm64_codegen = {
    arm64_preamble,
    arm64_func_start,
    arm64_func_end,
    arm64_stack_alloc,
    arm64_load_imm,
    arm64_push_acc,
    arm64_pop_to_tmp,
    arm64_pop_to_acc,
    arm64_acc_to_arg,
    arm64_acc_to_tmp,
    arm64_tmp_to_acc,
    arm64_acc_to_saved,
    arm64_saved_to_acc,
    arm64_load_via_saved,
    arm64_store_via_saved,
    arm64_add,
    arm64_sub,
    arm64_negate,
    arm64_mul,
    arm64_div_op,
    arm64_mod_op,
    arm64_bitand_op,
    arm64_bitor_op,
    arm64_bitnot_op,
    arm64_bitxor_op,
    arm64_shl_op,
    arm64_shr_op,
    arm64_cast_op,
    arm64_ptr_add,
    arm64_ptr_sub,
    arm64_cmp_eq,
    arm64_cmp_ne,
    arm64_cmp_lt,
    arm64_cmp_le,
    arm64_cmp_gt,
    arm64_cmp_ge,
    arm64_load_local,
    arm64_store_local,
    arm64_load_local_sized,
    arm64_store_local_sized,
    arm64_load_global,
    arm64_load_global_extern,
    arm64_store_global,
    arm64_store_global_extern,
    arm64_load_global_indexed,
    arm64_store_global_indexed,
    arm64_store_param,
    arm64_copy_local,
    arm64_ptr_copy,
    arm64_copy_local_to_ptr,
    arm64_load_indexed,
    arm64_store_indexed,
    arm64_addr_local,
    arm64_addr_indexed,
    arm64_add_offset,
    arm64_load_ptr_local,
    arm64_store_ptr_local,
    arm64_load_deref,
    arm64_store_deref,
    arm64_load_member_ptr,
    arm64_store_member_ptr,
    arm64_string_literal,
    arm64_load_string,
    arm64_load_func_addr,
    arm64_inline_asm,
    arm64_branch_if_zero,
    arm64_branch_if_nonzero,
    arm64_branch,
    arm64_label,
    arm64_prepare_call_args,
    arm64_call,
    arm64_call_saved,
    arm64_cleanup_call_args,
    arm64_emit_label_named_impl,
    arm64_emit_branch_named_impl,
    arm64_emit_source_loc,
    arm64_cmp_lt_u,
    arm64_cmp_le_u,
    arm64_cmp_gt_u,
    arm64_cmp_ge_u
};
