#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tcc.h"
#include "target.h"
#include "codegen.h"
#include "parser.h"

static void arm64_load_member_ptr(int offset, int size);
static void arm64_load_member_ptr_casted(int offset, int load_size, int cast_size, int is_unsigned);
static void arm64_load_ptr_indexed_casted(int elem_size, int load_size, int cast_size, int is_unsigned);
static void arm64_load_ptr_indexed_casted_base(const char *base_reg, int elem_size,
                                               int load_size, int cast_size,
                                               int is_unsigned);
static void arm64_load_ptr_indexed_casted_base_into_reg(const char *dst_x,
                                                        const char *dst_w,
                                                        const char *base_reg,
                                                        const char *index_x,
                                                        const char *index_w,
                                                        int elem_size,
                                                        int load_size,
                                                        int cast_size,
                                                        int is_unsigned);
static void arm64_store_member_ptr(int offset, int size);
static void arm64_load_local_casted_into_reg(const char *xreg, const char *wreg,
                                             int offset, int load_size, int is_unsigned);
static void arm64_store_local_sized(int offset, int size);
static void arm64_load_local_ptr_into_reg(const char *reg, int offset);
static void arm64_load_local_into_arg(int index, int offset, int size);
static void arm64_load_local_ptr_member_into_arg(int index, int local_offset,
                                                 int member_offset, int size,
                                                 int is_unsigned);
static void arm64_addr_local_ptr_member_indexed_into_arg(int arg_index,
                                                         int ptr_local_offset,
                                                         int member_offset,
                                                         int index_offset,
                                                         int index_load_size,
                                                         int index_is_unsigned,
                                                         int elem_size,
                                                         int final_offset);
static void arm64_addr_lpm_postinc_midx(int ptr_local_offset,
                                                               int base_member_offset,
                                                               int index_member_offset,
                                                               int index_load_size,
                                                               int index_is_unsigned,
                                                               int elem_size);
static void arm64_addr_lpm_postinc_lidx(int ptr_local_offset,
                                                               int member_offset,
                                                               int index_local_offset,
                                                               int index_load_size,
                                                               int index_is_unsigned,
                                                               int elem_size);
static void arm64_local_ptr_add_sub_locals_to_arg(int arg_index,
                                                  int ptr_local_offset,
                                                  int add_local_offset,
                                                  int add_local_size,
                                                  int add_is_unsigned,
                                                  int sub_local_offset,
                                                  int sub_local_size,
                                                  int sub_is_unsigned);
static void arm64_store_lpm_postinc_lidx_imm(int ptr_local_offset,
                                                                 int member_offset,
                                                                 int index_local_offset,
                                                                 int index_load_size,
                                                                 int index_is_unsigned,
                                                                 int elem_size,
                                                                 int store_size,
                                                                 long imm);
static void arm64_load_local_into_saved(int offset, int size);
static void arm64_load_string_into_arg(int index, int label);
static void arm64_load_func_addr_into_arg(int index, const char *name);
static void arm64_load_global_into_arg(int index, const char *name, int size,
                                       int is_extern, int is_unsigned);
static void arm64_load_global_member_into_arg(int index, const char *name,
                                              int offset, int size,
                                              int is_extern, int is_unsigned);
static void arm64_load_global_member_into_reg(const char *xreg, const char *wreg,
                                              const char *name, int offset,
                                              int size, int is_extern,
                                              int is_unsigned);
static void arm64_load_global_member_indexed(const char *name, int base_offset,
                                             int elem_size, int member_offset,
                                             int load_size, int cast_size,
                                             int is_unsigned, int index_offset,
                                             int index_load_size,
                                             int index_is_unsigned,
                                             int is_extern,
                                             int base_is_pointer);
static void arm64_load_global_member_indexed_into_arg(int arg_index,
                                                      const char *name,
                                                      int base_offset,
                                                      int elem_size,
                                                      int member_offset,
                                                      int load_size,
                                                      int cast_size,
                                                      int is_unsigned,
                                                      int index_offset,
                                                      int index_load_size,
                                                      int index_is_unsigned,
                                                      int is_extern,
                                                      int base_is_pointer);
static const unsigned int arm64_param_bits[8] = {
    1u << 0, 1u << 1, 1u << 2, 1u << 3,
    1u << 4, 1u << 5, 1u << 6, 1u << 7
};
static const char *const arm64_arg_xregs[8] = {
    "x0", "x1", "x2", "x3", "x4", "x5", "x6", "x7"
};
static const char *const arm64_arg_wregs[8] = {
    "w0", "w1", "w2", "w3", "w4", "w5", "w6", "w7"
};
static const unsigned int arm64_scratch_bits[7] = {
    1u << 0, 1u << 1, 1u << 2, 1u << 3,
    1u << 4, 1u << 5, 1u << 6
};
static void arm64_addr_global_member_indexed_into_arg(int arg_index,
                                                      const char *name,
                                                      int base_offset,
                                                      int elem_size,
                                                      int index_offset,
                                                      int index_load_size,
                                                      int index_is_unsigned,
                                                      int final_offset,
                                                      int is_extern,
                                                      int base_is_pointer);
static int arm64_symbol_is_thread_local(const char *name);
static void arm64_load_local_ptr_offset_indexed_casted_into_arg(int arg_index,
                                                                int ptr_local_offset,
                                                                int base_offset,
                                                                int index_offset,
                                                                int index_load_size,
                                                                int index_is_unsigned,
                                                                int elem_size,
                                                                int member_offset,
                                                                int load_size,
                                                                int cast_size,
                                                                int is_unsigned);
static void arm64_update_local_ptr_offset_indexed_from_local_ptr_offset(
    int dst_ptr_local_offset, int dst_base_offset,
    int src_ptr_local_offset, int src_base_offset,
    int index_offset, int index_load_size, int index_is_unsigned,
    int elem_size, int load_size, const char *op, int fp_size);
static void arm64_zero_local_range(int start_offset, int size);
static void arm64_store_local_ptr_member_imm(int local_offset, int member_offset,
                                             int size, long value);
static void arm64_store_saved_offset_imm(int offset, int size, long value);
static void arm64_cmp_local_global_addr_bool(int local_offset, const char *name,
                                             int is_extern, const char *op);
static void arm64_cmp_local_ptr_member_global_addr_bool(int local_offset, int member_offset,
                                                        int load_size, const char *name,
                                                        int is_extern, const char *op);
static void arm64_update_local_ptr_member_imm(int local_offset, int member_offset,
                                              int load_size, const char *op, long imm);
static void arm64_update_local_ptr_member_from_local_ptr_member(
    int dst_ptr_local_offset, int dst_member_offset,
    int src_ptr_local_offset, int src_member_offset,
    int load_size, const char *op, int fp_size);
static void arm64_update_global_member_imm(const char *name, int member_offset,
                                           int load_size, const char *op, long imm);
static void arm64_update_global_imm(const char *name, int load_size,
                                    const char *op, long imm);
static void arm64_update_global_from_local(const char *name, int load_size,
                                           int local_offset, int local_size,
                                           int local_is_unsigned,
                                           int is_sub);
static void arm64_update_global_member_from_local(const char *name, int member_offset,
                                                  int load_size,
                                                  int local_offset, int local_size,
                                                  int local_is_unsigned,
                                                  int is_sub);
static void arm64_store_global_member_from_local(const char *name, int member_offset,
                                                 int store_size,
                                                 int local_offset, int local_size,
                                                 int local_is_unsigned);
static void arm64_store_reg_global_member(const char *name, int member_offset,
                                          int store_size,
                                          const char *xreg, const char *wreg);
static void arm64_store_gm_local_imm(const char *name, int member_offset,
                                     int store_size,
                                     int local_offset, int local_size,
                                     int local_is_unsigned,
                                     long imm);
static void arm64_store_gm_local_local_mask(const char *name, int member_offset,
                                            int store_size,
                                            int lhs_offset, int lhs_size,
                                            int lhs_is_unsigned,
                                            int rhs_offset, int rhs_size,
                                            int rhs_is_unsigned,
                                            int mask);
static void arm64_update_global_member_indexed_imm_from_local(const char *name,
                                                              int base_offset, int index_scale,
                                                              int load_size,
                                                              int index_offset, int index_load_size,
                                                              int index_is_unsigned,
                                                              const char *op, long imm);
static void arm64_accumulate_global_member_indexed_double_from_local_ptr(const char *name,
                                                                         int base_offset, int index_scale,
                                                                         int frame_local_offset,
                                                                         int index_member_offset,
                                                                         int local_now_offset,
                                                                         int frame_time_member_offset);
static void arm64_store_global_member_indexed_from_global_member_local(const char *name,
                                                                       int base_offset, int index_scale,
                                                                       int member_offset, int store_size,
                                                                       const char *index_name,
                                                                       int index_member_offset, int index_load_size,
                                                                       int index_is_unsigned, int index_add,
                                                                       int value_offset, int value_load_size,
                                                                       int value_is_unsigned);
static void arm64_store_local_deref_from_local_ptr_member(int dst_local_offset,
                                                          int src_ptr_local_offset,
                                                          int member_offset,
                                                          int load_size, int cast_size,
                                                          int is_unsigned,
                                                          int store_offset, int store_size);
static void arm64_load_imm_into_arg(int index, long value);
static void arm64_add_arg_offset(int dst_index, int src_index, int offset);
static void arm64_call(const char *name);
static void arm64_call_saved(void);
static void arm64_fp_binop(const char *op, int size);
static void arm64_fp_cmp_branch(const char *op, int size, int label);
static void arm64_fp_cast_bits(int src_size, int dst_size);
static void arm64_fp_to_acc_bits(int size);
static void arm64_int_to_fp_bits(int size, int is_unsigned);
static void arm64_fp_bits_to_int(int size, int is_unsigned);
static void arm64_call_fp_args(const char *name, int count, int fixed_params,
                               unsigned int fp_arg_mask,
                               unsigned int fp_arg_double_mask);
static void arm64_call_saved_fp_args(int count, int fixed_params,
                                     unsigned int fp_arg_mask,
                                     unsigned int fp_arg_double_mask);
static int arm64_simm9(int offset);
static int arm64_uimm_scaled(int offset, int size);
static int arm64_scale_shift(int scale);
static void arm64_print_sxtw_shift_suffix(int shift);
static int arm64_symbol_needs_got(const char *name);
static int arm64_load_imm_offset_ok(int size, int offset);
static void arm64_load_global_address_reg(const char *reg, const char *name);
static void emit_add_imm64(const char *dst, const char *src, long value);
static int arm64_reg_num(const char *reg);
static void arm64_fill_reg_name(char *buf, char prefix, int reg);
static void arm64_store_partial_reg_to_addr(int reg, const char *base, int offset, int size);
static int arm64_alloc_scratch_reg(int avoid_reg0, int avoid_reg1);
static void arm64_release_scratch_reg(int reg);
static const char *arm64_frame_base_reg(void);
static int arm64_frame_offset(int offset);
static void emit_frame_offset_addr_reg(const char *reg, int offset);

static void arm64_store_partial_reg_to_addr(int reg, const char *base, int offset, int size) {
    if (size >= 8) {
        printf("    str x%d, [%s, #%d]\n", reg, base, offset);
        return;
    }
    if (size >= 4) {
        printf("    str w%d, [%s, #%d]\n", reg, base, offset);
        if (size > 4) {
            int tail = size - 4;
            printf("    lsr x10, x%d, #32\n", reg);
            if (tail >= 2) {
                printf("    strh w10, [%s, #%d]\n", base, offset + 4);
                if (tail > 2) {
                    printf("    lsr x10, x10, #16\n");
                    printf("    strb w10, [%s, #%d]\n", base, offset + 6);
                }
            } else {
                printf("    strb w10, [%s, #%d]\n", base, offset + 4);
            }
        }
        return;
    }
    if (size >= 2) {
        printf("    strh w%d, [%s, #%d]\n", reg, base, offset);
        if (size > 2) {
            printf("    lsr x10, x%d, #16\n", reg);
            printf("    strb w10, [%s, #%d]\n", base, offset + 2);
        }
        return;
    }
    printf("    strb w%d, [%s, #%d]\n", reg, base, offset);
}

static void emit_load_imm64(const char *reg, long value) {
    unsigned long v = (unsigned long)value;
    unsigned long h[4];
    int nonzero_count = 0;
    int nonffff_count = 0;
    int i;
    int first;

    h[0] = (v >> 0)  & 0xffff;
    h[1] = (v >> 16) & 0xffff;
    h[2] = (v >> 32) & 0xffff;
    h[3] = (v >> 48) & 0xffff;

    for (i = 0; i < 4; i++) {
        if (h[i] != 0)
            nonzero_count++;
        if (h[i] != 0xffff)
            nonffff_count++;
    }

    if (nonffff_count < nonzero_count) {
        first = -1;
        for (i = 0; i < 4; i++) {
            if (h[i] != 0xffff) {
                first = i;
                break;
            }
        }
        if (first < 0)
            first = 0;
        printf("    movn %s, #%lu", reg, (~h[first]) & 0xfffful);
        if (first > 0)
            printf(", lsl #%d", first * 16);
        printf("\n");
        for (i = 0; i < 4; i++) {
            if (i == first || h[i] == 0xffff)
                continue;
            printf("    movk %s, #%lu, lsl #%d\n", reg, h[i], i * 16);
        }
        return;
    }

    first = -1;
    for (i = 0; i < 4; i++) {
        if (h[i] != 0) {
            first = i;
            break;
        }
    }
    if (first < 0)
        first = 0;
    printf("    movz %s, #%lu", reg, h[first]);
    if (first > 0)
        printf(", lsl #%d", first * 16);
    printf("\n");
    for (i = 0; i < 4; i++) {
        if (i == first || h[i] == 0)
            continue;
        printf("    movk %s, #%lu, lsl #%d\n", reg, h[i], i * 16);
    }
}

/* Debug info state for .loc/.file directives */
static int arm64_debug_enabled = 0;
static const char *arm64_debug_files[256];
static int arm64_debug_file_count = 0;
int arm64_simple_leaf = 0;
int arm64_leaf_sp_frame_size = 0;
static int arm64_fixed_frame_size = 0;
static int arm64_expect_fixed_frame_alloc = 0;
static int arm64_needs_sp_restore = 0;
static int arm64_large_frame_base = 0;
static unsigned int arm64_live_param_reg_mask = 0;
static unsigned int arm64_saved_param_reg_mask = 0;
static unsigned int arm64_scratch_param_reg_mask = 0;
static unsigned int arm64_scratch_reg_mask = 0;
static int arm64_saved_param_stack_base = 0;
static int arm64_saved_param_save_size = 0;
static int arm64_param_home_reg[8] = { -1, -1, -1, -1, -1, -1, -1, -1 };

static void arm64_reset_param_home_regs(void) {
    int i;

    arm64_live_param_reg_mask = 0;
    arm64_saved_param_reg_mask = 0;
    arm64_scratch_param_reg_mask = 0;
    arm64_saved_param_stack_base = 0;
    arm64_saved_param_save_size = 0;
    for (i = 0; i < 8; i++)
        arm64_param_home_reg[i] = -1;
}

void arm64_set_debug(int enabled) {
    arm64_debug_enabled = enabled;
}

void arm64_clear_live_param_regs(void) {
    arm64_reset_param_home_regs();
}

void arm64_begin_fixed_frame(void) {
    arm64_expect_fixed_frame_alloc = 1;
    arm64_fixed_frame_size = 0;
}

void arm64_mark_live_param_reg(int index) {
    unsigned int uindex = (unsigned int)index;

    if (uindex >= 8)
        return;
    arm64_live_param_reg_mask |= 1u << uindex;
    arm64_param_home_reg[uindex] = index;
}

void arm64_configure_scratch_param_regs(unsigned int scratch_mask) {
    static const int scratch_regs[7] = { 15, 14, 13, 12, 11, 10, 9 };
    int next_reg = 0;
    int i;
    int count = 0;

    arm64_scratch_param_reg_mask = 0;
    if ((scratch_mask & 0xffu) == 0)
        return;
    for (i = 0; i < 8; i++) {
        if ((scratch_mask & arm64_param_bits[i]) != 0)
            count++;
    }
    if (count > 7)
        return;

    arm64_scratch_param_reg_mask = scratch_mask & 0xffu;
    for (i = 0; i < 8; i++) {
        if ((arm64_scratch_param_reg_mask & arm64_param_bits[i]) == 0)
            continue;
        arm64_param_home_reg[i] = scratch_regs[next_reg++];
    }
}

void arm64_configure_saved_param_regs(int local_stack_size, unsigned int saved_mask) {
    unsigned int mask;
    int highest = -1;
    int i;

    arm64_saved_param_reg_mask = saved_mask & 0xffu;
    arm64_saved_param_stack_base = local_stack_size;
    arm64_saved_param_save_size = 0;
    if (arm64_saved_param_reg_mask == 0)
        return;

    mask = arm64_saved_param_reg_mask;
    for (i = 0; mask != 0 && i < 8; i++, mask >>= 1) {
        if ((mask & 1u) == 0)
            continue;
        arm64_param_home_reg[i] = 19 + i;
        highest = i;
    }

    if (highest >= 0)
        arm64_saved_param_save_size = ((highest + 1) * 8 + 15) & ~15;
}

int arm64_saved_param_stack_size(void) {
    return arm64_saved_param_save_size;
}

int arm64_saved_param_dwarf_reg(int index) {
    if (index < 0 || index >= 8)
        return -1;
    return arm64_param_home_reg[index];
}

static int arm64_saved_param_offset(int index) {
    return -(arm64_saved_param_stack_base + ((index + 1) * 8));
}

static void arm64_store_reg_to_local_slot(const char *reg, int offset) {
    int frame_offset = arm64_frame_offset(offset);
    const char *base = arm64_frame_base_reg();

    if (arm64_simm9(frame_offset) || arm64_uimm_scaled(frame_offset, 8)) {
        printf("    str %s, [%s, #%d]\n", reg, base, frame_offset);
        return;
    }

    {
        int scratch = arm64_alloc_scratch_reg(arm64_reg_num(reg), -1);
        char scratch_reg[4];

        arm64_fill_reg_name(scratch_reg, 'x', scratch);
        emit_frame_offset_addr_reg(scratch_reg, offset);
        printf("    str %s, [%s]\n", reg, scratch_reg);
        arm64_release_scratch_reg(scratch);
    }
}

static void arm64_load_reg_from_local_slot(const char *reg, int offset) {
    int frame_offset = arm64_frame_offset(offset);
    const char *base = arm64_frame_base_reg();

    if (arm64_simm9(frame_offset) || arm64_uimm_scaled(frame_offset, 8)) {
        printf("    ldr %s, [%s, #%d]\n", reg, base, frame_offset);
        return;
    }

    {
        int scratch = arm64_alloc_scratch_reg(arm64_reg_num(reg), -1);
        char scratch_reg[4];

        arm64_fill_reg_name(scratch_reg, 'x', scratch);
        emit_frame_offset_addr_reg(scratch_reg, offset);
        printf("    ldr %s, [%s]\n", reg, scratch_reg);
        arm64_release_scratch_reg(scratch);
    }
}

void arm64_emit_scratch_param_setup(void) {
    int i;

    for (i = 0; i < 8; i++) {
        char home_reg[4];
        char param_reg[4];
        int reg;

        if ((arm64_scratch_param_reg_mask & arm64_param_bits[i]) == 0)
            continue;
        reg = arm64_param_home_reg[i];
        if (reg < 9 || reg > 15)
            continue;
        arm64_scratch_reg_mask |= arm64_scratch_bits[reg - 9];
        arm64_fill_reg_name(home_reg, 'x', reg);
        arm64_fill_reg_name(param_reg, 'x', i);
        printf("    mov %s, %s\n", home_reg, param_reg);
    }
}

void arm64_emit_saved_param_setup(void) {
    unsigned int mask;
    int i;

    mask = arm64_saved_param_reg_mask;
    for (i = 0; mask != 0 && i < 8; i++, mask >>= 1) {
        char home_reg[4];
        char param_reg[4];

        if ((mask & 1u) == 0)
            continue;
        arm64_fill_reg_name(home_reg, 'x', 19 + i);
        arm64_fill_reg_name(param_reg, 'x', i);
        arm64_store_reg_to_local_slot(home_reg, arm64_saved_param_offset(i));
        printf("    mov %s, %s\n", home_reg, param_reg);
    }
}

static int arm64_live_param_reg_for_offset(int offset) {
    int index;

    if (offset >= 0 || (offset & 7) != 0)
        return -1;
    index = (-offset / 8) - 1;
    if (index < 0 || index >= 8)
        return -1;
    return arm64_param_home_reg[index];
}

static int arm64_reg_num(const char *reg) {
    if (!reg || (reg[0] != 'x' && reg[0] != 'w'))
        return -1;
    if (reg[1] < '0' || reg[1] > '9')
        return -1;
    if (reg[2] == '\0')
        return reg[1] - '0';
    if (reg[2] >= '0' && reg[2] <= '9' && reg[3] == '\0')
        return (reg[1] - '0') * 10 + (reg[2] - '0');
    return -1;
}

static void arm64_fill_reg_name(char *buf, char prefix, int reg) {
    buf[0] = prefix;
    if (reg >= 10) {
        buf[1] = (char)('0' + (reg / 10));
        buf[2] = (char)('0' + (reg % 10));
        buf[3] = '\0';
    } else {
        buf[1] = (char)('0' + reg);
        buf[2] = '\0';
    }
}

static int arm64_alloc_scratch_reg(int avoid_reg0, int avoid_reg1) {
    int reg;

    for (reg = 9; reg <= 15; reg++) {
        unsigned int bit = arm64_scratch_bits[reg - 9];
        if (reg == avoid_reg0 || reg == avoid_reg1)
            continue;
        if ((arm64_scratch_reg_mask & bit) != 0)
            continue;
        arm64_scratch_reg_mask |= bit;
        return reg;
    }
    arm64_scratch_reg_mask |= arm64_scratch_bits[15 - 9];
    return 15;
}

static void arm64_release_scratch_reg(int reg) {
    unsigned int ureg;

    ureg = (unsigned int)(reg - 9);
    if (ureg >= 7)
        return;
    arm64_scratch_reg_mask &= ~arm64_scratch_bits[ureg];
}

int arm64_acquire_scratch_reg(int avoid_reg0, int avoid_reg1) {
    return arm64_alloc_scratch_reg(avoid_reg0, avoid_reg1);
}

void arm64_release_scratch_reg_public(int reg) {
    arm64_release_scratch_reg(reg);
}

void arm64_format_reg_name(char *buf, char prefix, int reg) {
    arm64_fill_reg_name(buf, prefix, reg);
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
    arm64_needs_sp_restore = 0;
    arm64_scratch_reg_mask = 0;
    arm64_fixed_frame_size = 0;
    arm64_large_frame_base = 0;
    printf(".align 2\n");
    if (!is_static)
        printf(".global _%s\n", name);
    printf("_%s:\n", name);
    if (arm64_leaf_sp_frame_size > 0) {
        if (arm64_leaf_sp_frame_size <= 4095)
            printf("    sub sp, sp, #%d\n", arm64_leaf_sp_frame_size);
        else {
            emit_load_imm64("x9", arm64_leaf_sp_frame_size);
            printf("    sub sp, sp, x9\n");
        }
        return;
    }
    if (arm64_simple_leaf)
        return;
    if (arm64_expect_fixed_frame_alloc) {
        printf("    stp x29, x30, [sp, #-16]!\n");
        printf("    mov x29, sp\n");
        return;
    }
    printf("    stp x29, x30, [sp, #-16]!\n");
    printf("    mov x29, sp\n");
}

static void arm64_func_end(void) {
    int i;

    if (arm64_leaf_sp_frame_size > 0) {
        if (arm64_leaf_sp_frame_size <= 4095)
            printf("    add sp, sp, #%d\n", arm64_leaf_sp_frame_size);
        else {
            emit_load_imm64("x9", arm64_leaf_sp_frame_size);
            printf("    add sp, sp, x9\n");
        }
        arm64_leaf_sp_frame_size = 0;
        arm64_scratch_reg_mask = 0;
        printf("    ret\n");
        return;
    }
    if (arm64_simple_leaf) {
        arm64_reset_param_home_regs();
        arm64_scratch_reg_mask = 0;
        arm64_expect_fixed_frame_alloc = 0;
        arm64_large_frame_base = 0;
        printf("    ret\n");
        return;
    }
    for (i = 0; i < 8; i++) {
        char home_reg[4];

        if ((arm64_saved_param_reg_mask & (1u << i)) == 0)
            continue;
        arm64_fill_reg_name(home_reg, 'x', 19 + i);
        arm64_load_reg_from_local_slot(home_reg, arm64_saved_param_offset(i));
    }
    if (arm64_fixed_frame_size > 0) {
        if (arm64_large_frame_base)
            printf("    ldr x28, [x28]\n");
        printf("    mov sp, x29\n");
        arm64_reset_param_home_regs();
        arm64_scratch_reg_mask = 0;
        arm64_expect_fixed_frame_alloc = 0;
        arm64_fixed_frame_size = 0;
        arm64_large_frame_base = 0;
        printf("    ldp x29, x30, [sp], #16\n");
        printf("    ret\n");
        return;
    }
    if (arm64_needs_sp_restore)
        printf("    mov sp, x29\n");
    arm64_reset_param_home_regs();
    arm64_scratch_reg_mask = 0;
    arm64_expect_fixed_frame_alloc = 0;
    arm64_large_frame_base = 0;
    printf("    ldp x29, x30, [sp], #16\n");
    printf("    ret\n");
}

static int align16(int n) {
    return (n + 15) & ~15;
}

static const char *arm64_frame_base_reg(void) {
    if (arm64_leaf_sp_frame_size > 0)
        return "sp";
    if (arm64_large_frame_base)
        return "x28";
    return "x29";
}

static int arm64_frame_offset(int offset) {
    if (arm64_leaf_sp_frame_size > 0)
        return arm64_leaf_sp_frame_size + offset;
    if (arm64_large_frame_base)
        return arm64_fixed_frame_size + 16 + offset;
    return offset;
}

static void arm64_stack_alloc(int size) {
    size = align16(size);
    if (size <= 0)
        return;
    arm64_needs_sp_restore = 1;
    if (size <= 4095) {
     	printf("    sub sp, sp, #%d\n", size);
    } else {
	emit_load_imm64("x9",size);
     	printf("    sub sp, sp, x9\n");
    }
    if (arm64_expect_fixed_frame_alloc) {
        arm64_fixed_frame_size = size;
        arm64_expect_fixed_frame_alloc = 0;
        if (size > 512) {
            printf("    str x28, [sp, #-16]!\n");
            printf("    mov x28, sp\n");
            arm64_large_frame_base = 1;
        }
    }
}

static void arm64_stack_save_acc(void) {
    printf("    mov x0, sp\n");
}

static void arm64_stack_restore_acc(void) {
    arm64_needs_sp_restore = 1;
    printf("    mov sp, x0\n");
}

static void arm64_stack_alloc_acc(void) {
    arm64_needs_sp_restore = 1;
    printf("    add x0, x0, #15\n");
    printf("    and x0, x0, #0xfffffffffffffff0\n");
    printf("    sub sp, sp, x0\n");
    printf("    mov x0, sp\n");
}

static void arm64_load_imm(long value) {
    emit_load_imm64("x0", value);
}

static void arm64_push_acc(void) {
    arm64_needs_sp_restore = 1;
    printf("    str x0, [sp, #-16]!\n");
}

static void arm64_push_zero(void) {
    arm64_needs_sp_restore = 1;
    printf("    str xzr, [sp, #-16]!\n");
}

static void arm64_pop_to_tmp(void) {
    arm64_needs_sp_restore = 1;
    printf("    ldr x1, [sp], #16\n");
}

static void arm64_pop_to_acc(void) {
    arm64_needs_sp_restore = 1;
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
    printf("    mov x17, x0\n");
}

static void arm64_saved_to_acc(void) {
    printf("    mov x0, x17\n");
}

static void arm64_load_via_saved(int size) {
    if (size == 1)
        printf("    ldrb w0, [x17]\n");
    else if (size == 2)
        printf("    ldrh w0, [x17]\n");
    else if (size == 4)
        printf("    ldrsw x0, [x17]\n");
    else
        printf("    ldr x0, [x17]\n");
}

static void arm64_store_via_saved(int size) {
    /* arm64_acc_to_saved() saves the lvalue address in x17.
     * Store back through the same saved register.  Older code used x2 here,
     * which only worked accidentally when x2 still happened to contain the
     * address; ordinary indexed lvalue updates such as t[i]++ leave x2 stale
     * and can crash the generated program.
     */
    if (size == 1)
        printf("    strb w0, [x17]\n");
    else if (size == 2)
        printf("    strh w0, [x17]\n");
    else if (size == 4)
        printf("    str w0, [x17]\n");
    else
        printf("    str x0, [x17]\n");
}

static void arm64_store_saved_offset_imm(int offset, int size, long value) {
    const char *base = "x17";
    char base_buf[4];
    char value_x[4];
    char value_w[4];
    int base_scratch = -1;
    int value_scratch = -1;

    if (!(arm64_simm9(offset) || arm64_uimm_scaled(offset, size))) {
        base_scratch = arm64_alloc_scratch_reg(17, -1);
        arm64_fill_reg_name(base_buf, 'x', base_scratch);
        emit_add_imm64(base_buf, "x17", offset);
        base = base_buf;
        offset = 0;
    }

    if (value == 0) {
        if (size == 1)
            printf("    strb wzr, [%s, #%d]\n", base, offset);
        else if (size == 2)
            printf("    strh wzr, [%s, #%d]\n", base, offset);
        else if (size == 8)
            printf("    str xzr, [%s, #%d]\n", base, offset);
        else
            printf("    str wzr, [%s, #%d]\n", base, offset);
        if (base_scratch >= 0)
            arm64_release_scratch_reg(base_scratch);
        return;
    }

    value_scratch = arm64_alloc_scratch_reg(base_scratch, -1);
    arm64_fill_reg_name(value_x, 'x', value_scratch);
    arm64_fill_reg_name(value_w, 'w', value_scratch);
    emit_load_imm64(value_x, value);
    if (size == 1)
        printf("    strb %s, [%s, #%d]\n", value_w, base, offset);
    else if (size == 2)
        printf("    strh %s, [%s, #%d]\n", value_w, base, offset);
    else if (size == 8)
        printf("    str %s, [%s, #%d]\n", value_x, base, offset);
    else
        printf("    str %s, [%s, #%d]\n", value_w, base, offset);
    arm64_release_scratch_reg(value_scratch);
    if (base_scratch >= 0)
        arm64_release_scratch_reg(base_scratch);
}

static void arm64_const_addr_gidx_global_member_ptr_add_return(const char *base_name,
                                                               int base_offset,
                                                               const char *index_name,
                                                               int member_offset,
                                                               int load_size,
                                                               int cast_size,
                                                               int is_unsigned,
                                                               int scale,
                                                               int index_is_extern) {
    int shift = arm64_scale_shift(scale);

    if (index_is_extern || arm64_symbol_needs_got(index_name) ||
        arm64_symbol_needs_got(base_name) || strcmp(base_name, index_name) != 0) {
        arm64_load_global_address_reg("x8", base_name);
        arm64_load_global_address_reg("x11", index_name);
        if (member_offset)
            emit_add_imm64("x11", "x11", member_offset);
    } else {
        arm64_load_global_address_reg("x8", base_name);
        if (member_offset > 0 && member_offset <= 4095) {
            if (load_size == 1) {
                if (cast_size == 1 && !is_unsigned)
                    printf("    ldrsb x9, [x8, #%d]\n", member_offset);
                else
                    printf("    ldrb w9, [x8, #%d]\n", member_offset);
            } else if (load_size == 2) {
                if (cast_size == 2 && !is_unsigned)
                    printf("    ldrsh x9, [x8, #%d]\n", member_offset);
                else
                    printf("    ldrh w9, [x8, #%d]\n", member_offset);
            } else if (load_size == 8) {
                printf("    ldr x9, [x8, #%d]\n", member_offset);
            } else if (is_unsigned) {
                printf("    ldr w9, [x8, #%d]\n", member_offset);
            } else {
                printf("    ldrsw x9, [x8, #%d]\n", member_offset);
            }
            goto have_index;
        }
        emit_add_imm64("x11", "x8", member_offset);
    }

    if (load_size == 1) {
        if (cast_size == 1 && !is_unsigned)
            printf("    ldrsb x9, [x11]\n");
        else
            printf("    ldrb w9, [x11]\n");
    } else if (load_size == 2) {
        if (cast_size == 2 && !is_unsigned)
            printf("    ldrsh x9, [x11]\n");
        else
            printf("    ldrh w9, [x11]\n");
    } else if (load_size == 8) {
        printf("    ldr x9, [x11]\n");
    } else if (is_unsigned) {
        printf("    ldr w9, [x11]\n");
    } else {
        printf("    ldrsw x9, [x11]\n");
    }

have_index:
    if (base_offset)
        emit_add_imm64("x8", "x8", base_offset);

    if (shift >= 0 && shift <= 4) {
        printf("    add x0, x8, w9");
        arm64_print_sxtw_shift_suffix(shift);
        printf("\n");
        return;
    }

    if (cast_size != 8)
        printf("    sxtw x9, w9\n");
    emit_load_imm64("x10", scale);
    printf("    madd x0, x9, x10, x8\n");
}

static void arm64_gidx_gm_local_mask_ptr_return(
    const char *base_name,
    int base_offset,
    const char *global_index_name,
    int global_member_offset,
    int global_load_size,
    int global_cast_size,
    int global_is_unsigned,
    int global_index_is_extern,
    int local_offset,
    int local_load_size,
    int local_cast_size,
    int local_is_unsigned,
    int mask,
    int scale) {
    int shift = arm64_scale_shift(scale);

    arm64_load_global_address_reg("x8", base_name);

    if (!global_index_is_extern && !arm64_symbol_needs_got(global_index_name) &&
        !arm64_symbol_needs_got(base_name) && strcmp(base_name, global_index_name) == 0) {
        if (global_member_offset > 0 && global_member_offset <= 4095) {
            if (global_load_size == 1) {
                if (global_cast_size == 1 && !global_is_unsigned)
                    printf("    ldrsb x9, [x8, #%d]\n", global_member_offset);
                else
                    printf("    ldrb w9, [x8, #%d]\n", global_member_offset);
            } else if (global_load_size == 2) {
                if (global_cast_size == 2 && !global_is_unsigned)
                    printf("    ldrsh x9, [x8, #%d]\n", global_member_offset);
                else
                    printf("    ldrh w9, [x8, #%d]\n", global_member_offset);
            } else if (global_load_size == 8) {
                printf("    ldr x9, [x8, #%d]\n", global_member_offset);
            } else if (global_is_unsigned) {
                printf("    ldr w9, [x8, #%d]\n", global_member_offset);
            } else {
                printf("    ldrsw x9, [x8, #%d]\n", global_member_offset);
            }
        } else {
            emit_add_imm64("x11", "x8", global_member_offset);
            if (global_load_size == 1) {
                if (global_cast_size == 1 && !global_is_unsigned)
                    printf("    ldrsb x9, [x11]\n");
                else
                    printf("    ldrb w9, [x11]\n");
            } else if (global_load_size == 2) {
                if (global_cast_size == 2 && !global_is_unsigned)
                    printf("    ldrsh x9, [x11]\n");
                else
                    printf("    ldrh w9, [x11]\n");
            } else if (global_load_size == 8) {
                printf("    ldr x9, [x11]\n");
            } else if (global_is_unsigned) {
                printf("    ldr w9, [x11]\n");
            } else {
                printf("    ldrsw x9, [x11]\n");
            }
        }
    } else {
        arm64_load_global_member_into_reg("x9", "w9", global_index_name,
                                          global_member_offset, global_load_size,
                                          global_index_is_extern, global_is_unsigned);
    }

    arm64_load_local_casted_into_reg("x10", "w10", local_offset,
                                     local_load_size, local_is_unsigned);
    if (global_cast_size == 8 || local_cast_size == 8) {
        printf("    add x9, x9, x10\n");
        printf("    and x9, x9, #0x%x\n", mask);
    } else {
        printf("    add w9, w9, w10\n");
        printf("    and w9, w9, #0x%x\n", mask);
    }

    if (base_offset)
        emit_add_imm64("x8", "x8", base_offset);

    if (shift >= 0 && shift <= 4) {
        if (global_cast_size == 8 || local_cast_size == 8) {
            printf("    add x0, x8, x9");
            if (shift > 0)
                printf(", lsl #%d", shift);
            printf("\n");
        } else {
            printf("    add x0, x8, w9, uxtw");
            if (shift > 0)
                printf(" #%d", shift);
            printf("\n");
        }
        return;
    }

    if (global_cast_size != 8 && local_cast_size != 8)
        printf("    uxtw x9, w9\n");
    emit_load_imm64("x10", scale);
    printf("    madd x0, x9, x10, x8\n");
}

static void arm64_const_addr_gidx_local_ptr_add_return(const char *base_name,
                                                       int base_offset,
                                                       int index_offset,
                                                       int load_size,
                                                       int cast_size,
                                                       int is_unsigned,
                                                       int scale) {
    int shift = arm64_scale_shift(scale);

    arm64_load_global_address_reg("x8", base_name);
    if (base_offset)
        emit_add_imm64("x8", "x8", base_offset);

    arm64_load_local_casted_into_reg("x9", "w9", index_offset, load_size, is_unsigned);

    if (shift >= 0 && shift <= 4) {
        if (cast_size == 8) {
            printf("    add x0, x8, x9");
            if (shift > 0)
                printf(", lsl #%d", shift);
            printf("\n");
        } else {
            printf("    add x0, x8, w9, %s", is_unsigned ? "uxtw" : "sxtw");
            if (shift > 0)
                printf(" #%d", shift);
            printf("\n");
        }
        return;
    }

    emit_load_imm64("x10", scale);
    printf("    madd x0, x9, x10, x8\n");
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

static void arm64_udiv_op(void) {
    printf("    udiv x0, x1, x0\n");
}

static void arm64_umod_op(void) {
    printf("    udiv x2, x1, x0\n");
    printf("    msub x0, x2, x0, x1\n");
}

static void arm64_div_sized_op(int size) {
    if (size == 4) {
        printf("    sdiv w0, w1, w0\n");
        printf("    sxtw x0, w0\n");
        return;
    }
    arm64_div_op();
}

static void arm64_mod_sized_op(int size) {
    if (size == 4) {
        printf("    sdiv w2, w1, w0\n");
        printf("    msub w0, w2, w0, w1\n");
        printf("    sxtw x0, w0\n");
        return;
    }
    arm64_mod_op();
}

static void arm64_udiv_sized_op(int size) {
    if (size == 4) {
        printf("    udiv w0, w1, w0\n");
        return;
    }
    arm64_udiv_op();
}

static void arm64_umod_sized_op(int size) {
    if (size == 4) {
        printf("    udiv w2, w1, w0\n");
        printf("    msub w0, w2, w0, w1\n");
        return;
    }
    arm64_umod_op();
}


static void arm64_bitand_op(void) {
    printf("    and x0, x1, x0\n");
}

static void arm64_bitand_imm_op(long value) {
    unsigned long mask = (unsigned long)value;

    if (mask != 0 &&
        (((value >= 0) && (((mask + 1UL) & mask) == 0)) ||
         ((value < 0) && ((((~mask) + 1UL) & (~mask)) == 0)))) {
        printf("    and x0, x0, #0x%lx\n", mask);
        return;
    }

    emit_load_imm64("x9", value);
    printf("    and x0, x0, x9\n");
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

static void arm64_ushr_op(void) {
    printf("    lsr x0, x1, x0\n");
}

static void arm64_shl_imm_op(int imm) {
    printf("    lsl x0, x0, #%d\n", imm);
}

static void arm64_or_shl_imm_op(int imm) {
    arm64_needs_sp_restore = 1;
    printf("    ldr x0, [sp], #16\n");
    printf("    ldr x1, [sp], #16\n");
    printf("    orr x0, x1, x0, lsl #%d\n", imm);
}

static void arm64_shr_imm_op(int imm) {
    printf("    asr x0, x0, #%d\n", imm);
}

static void arm64_ushr_imm_op(int imm) {
    printf("    lsr x0, x0, #%d\n", imm);
}

static void arm64_cast_op(int size, int is_unsigned) {
    if (size == 1) {
        if (is_unsigned)
            printf("    and x0, x0, #255\n");
        else
            printf("    sxtb x0, w0\n");
    } else if (size == 2) {
        if (is_unsigned)
            printf("    and x0, x0, #65535\n");
        else
            printf("    sxth x0, w0\n");
    } else if (size == 4) {
        if (is_unsigned)
            printf("    mov w0, w0\n");
        else
            printf("    sxtw x0, w0\n");
    }
}

static void arm64_fp_to_acc_bits(int size) {
    if (size == 4)
        printf("    fmov w0, s0\n");
    else
        printf("    fmov x0, d0\n");
}

static void arm64_acc_bits_to_fp_return(int size) {
    if (size == 4)
        printf("    fmov s0, w0\n");
    else
        printf("    fmov d0, x0\n");
}

static void arm64_int_to_fp_bits(int size, int is_unsigned) {
    printf("    %s %c0, x0\n",
           is_unsigned ? "ucvtf" : "scvtf",
           size == 4 ? 's' : 'd');
    arm64_fp_to_acc_bits(size);
}

static void arm64_fp_bits_to_int(int size, int is_unsigned) {
    if (size == 4)
        printf("    fmov s0, w0\n");
    else
        printf("    fmov d0, x0\n");
    printf("    %s x0, %c0\n",
           is_unsigned ? "fcvtzu" : "fcvtzs",
           size == 4 ? 's' : 'd');
}

static void arm64_fp_cast_bits(int src_size, int dst_size) {
    if (src_size == dst_size)
        return;
    if (src_size == 4)
        printf("    fmov s0, w0\n");
    else
        printf("    fmov d0, x0\n");
    printf("    fcvt %c0, %c0\n",
           dst_size == 4 ? 's' : 'd',
           src_size == 4 ? 's' : 'd');
    arm64_fp_to_acc_bits(dst_size);
}

static void arm64_acc_bits_to_fp_regs(int size) {
    if (size == 4) {
        printf("    fmov s0, w0\n");
        printf("    fmov s1, w1\n");
    } else {
        printf("    fmov d0, x0\n");
        printf("    fmov d1, x1\n");
    }
}

static const char *arm64_fp_cond_from_op(const char *op) {
    if (strcmp(op, "eq") == 0 || strcmp(op, "je") == 0)
        return "eq";
    if (strcmp(op, "ne") == 0 || strcmp(op, "jne") == 0)
        return "ne";
    if (strcmp(op, "lt") == 0 || strcmp(op, "jlt") == 0)
        return "lt";
    if (strcmp(op, "le") == 0 || strcmp(op, "jle") == 0)
        return "le";
    if (strcmp(op, "gt") == 0 || strcmp(op, "jgt") == 0)
        return "gt";
    if (strcmp(op, "ge") == 0 || strcmp(op, "jge") == 0)
        return "ge";
    return NULL;
}

static void arm64_fp_emit_compare(const char *op, int size) {
    const char *cond = arm64_fp_cond_from_op(op);

    if (!cond)
        ICE("unsupported arm64 floating compare op: %s", op ? op : "<null>");

    arm64_acc_bits_to_fp_regs(size);
    printf("    fcmp %c1, %c0\n", size == 4 ? 's' : 'd', size == 4 ? 's' : 'd');
    printf("    cset w0, %s\n", cond);
}

static void arm64_fp_binop(const char *op, int size) {
    arm64_acc_bits_to_fp_regs(size);

    if (strcmp(op, "add") == 0)
        printf("    fadd %c0, %c1, %c0\n", size == 4 ? 's' : 'd', size == 4 ? 's' : 'd', size == 4 ? 's' : 'd');
    else if (strcmp(op, "sub") == 0)
        printf("    fsub %c0, %c1, %c0\n", size == 4 ? 's' : 'd', size == 4 ? 's' : 'd', size == 4 ? 's' : 'd');
    else if (strcmp(op, "mul") == 0)
        printf("    fmul %c0, %c1, %c0\n", size == 4 ? 's' : 'd', size == 4 ? 's' : 'd', size == 4 ? 's' : 'd');
    else if (strcmp(op, "div") == 0)
        printf("    fdiv %c0, %c1, %c0\n", size == 4 ? 's' : 'd', size == 4 ? 's' : 'd', size == 4 ? 's' : 'd');
    else {
        arm64_fp_emit_compare(op, size);
        return;
    }

    arm64_fp_to_acc_bits(size);
}

static void arm64_fp_cmp_branch(const char *op, int size, int label) {
    const char *cond = arm64_fp_cond_from_op(op);

    if (!cond)
        ICE("unsupported arm64 floating compare branch op: %s", op ? op : "<null>");

    arm64_acc_bits_to_fp_regs(size);
    printf("    fcmp %c1, %c0\n", size == 4 ? 's' : 'd', size == 4 ? 's' : 'd');
    printf("    b.%s L%d\n", cond, label);
}

static int arm64_setup_fp_call_args(const char *name, int use_saved_call,
                                     int count, int fixed_params,
                                     unsigned int fp_arg_mask,
                                     unsigned int fp_arg_double_mask) {
    int int_reg = 0;
    int fp_reg = 0;
    int i;

    if (count > 0)
        arm64_needs_sp_restore = 1;

    if (fixed_params >= 0) {
        if (fixed_params > count)
            fixed_params = count;

        for (i = 0; i < fixed_params; i++) {
            if (fp_arg_mask & (1u << i)) {
                int size = (fp_arg_double_mask & (1u << i)) ? 8 : 4;
                if (fp_reg >= 8)
                    ICE("arm64 fixed floating call register overflow");
                printf("    ldr %c%d, [sp, #%d]\n",
                       size == 4 ? 's' : 'd', fp_reg, i * 16);
                fp_reg++;
            } else {
                if (int_reg >= 8)
                    ICE("arm64 fixed integer call register overflow");
                printf("    ldr x%d, [sp, #%d]\n", int_reg, i * 16);
                int_reg++;
            }
        }

        for (i = fixed_params; i < count; i++) {
            int dst_off = (i - fixed_params) * 8;
            int src_off = i * 16;
            if (fp_arg_mask & (1u << i)) {
                if (fp_arg_double_mask & (1u << i)) {
                    printf("    ldr d16, [sp, #%d]\n", src_off);
                } else {
                    printf("    ldr s16, [sp, #%d]\n", src_off);
                    printf("    fcvt d16, s16\n");
                }
                printf("    str d16, [sp, #%d]\n", dst_off);
            } else {
                printf("    ldr x9, [sp, #%d]\n", src_off);
                printf("    str x9, [sp, #%d]\n", dst_off);
            }
        }

        if (use_saved_call)
            arm64_call_saved();
        else
            arm64_call(name);
        if (count > 0)
            printf("    add sp, sp, #%d\n", count * 16);
        return 1;
    }

    for (i = 0; i < count; i++) {
        if (fp_arg_mask & (1u << i)) {
            int size = (fp_arg_double_mask & (1u << i)) ? 8 : 4;
            if (fp_reg >= 8)
                ICE("arm64 floating call register overflow");
            printf("    ldr %c%d, [sp, #%d]\n",
                   size == 4 ? 's' : 'd', fp_reg, i * 16);
            fp_reg++;
        } else {
            if (int_reg >= 8)
                ICE("arm64 integer call register overflow");
            printf("    ldr x%d, [sp, #%d]\n", int_reg, i * 16);
            int_reg++;
        }
    }

    if (count > 0)
        printf("    add sp, sp, #%d\n", count * 16);
    return 0;
}

static void arm64_call_fp_args(const char *name, int count, int fixed_params,
                               unsigned int fp_arg_mask,
                               unsigned int fp_arg_double_mask) {
    if (!arm64_setup_fp_call_args(name, 0, count, fixed_params, fp_arg_mask,
                                  fp_arg_double_mask))
        arm64_call(name);
}

static void arm64_call_saved_fp_args(int count, int fixed_params,
                                     unsigned int fp_arg_mask,
                                     unsigned int fp_arg_double_mask) {
    if (!arm64_setup_fp_call_args(NULL, 1, count, fixed_params, fp_arg_mask,
                                  fp_arg_double_mask))
        arm64_call_saved();
}

static int arm64_scale_shift(int scale) {
    int shift = 0;

    if (scale <= 0 || (scale & (scale - 1)) != 0)
        return -1;
    while ((scale >>= 1) != 0)
        shift++;
    return shift;
}

static void arm64_print_sxtw_shift_suffix(int shift) {
    if (shift > 0)
        printf(", sxtw #%d", shift);
    else
        printf(", sxtw");
}

static void arm64_ptr_add(int scale) {
    /* x0 = index (right operand), x1 = base (left operand saved by acc_to_tmp)
     * Compute: x0 = base + index * scale */
    int shift;
    shift = arm64_scale_shift(scale);
    if (shift >= 0 && shift <= 4) {
        printf("    add x0, x1, w0");
        arm64_print_sxtw_shift_suffix(shift);
        printf("\n");
        return;
    }
    printf("    sxtw x0, w0\n");
    emit_load_imm64("x2", scale);
    printf("    madd x0, x0, x2, x1\n");
}

static void arm64_ptr_sub(int scale) {
    int shift;
    shift = arm64_scale_shift(scale);
    if (shift >= 0 && shift <= 4) {
        printf("    sub x0, x1, w0");
        arm64_print_sxtw_shift_suffix(shift);
        printf("\n");
        return;
    }
    printf("    sxtw x0, w0\n");
    emit_load_imm64("x2", scale);
    printf("    msub x0, x0, x2, x1\n");
}

static void arm64_cmp_common(const char *cond) {
    printf("    cmp x1, x0\n");
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

static void arm64_cmp_common_sized(const char *cond, int size) {
    if (size == 8) {
        printf("    cmp x1, x0\n");
    } else {
        printf("    cmp w1, w0\n");
    }
    printf("    cset w0, %s\n", cond);
}

static void arm64_cmp_eq_sized(int sz) { arm64_cmp_common_sized("eq", sz); }
static void arm64_cmp_ne_sized(int sz) { arm64_cmp_common_sized("ne", sz); }
static void arm64_cmp_lt_sized(int sz) { arm64_cmp_common_sized("lt", sz); }
static void arm64_cmp_le_sized(int sz) { arm64_cmp_common_sized("le", sz); }
static void arm64_cmp_gt_sized(int sz) { arm64_cmp_common_sized("gt", sz); }
static void arm64_cmp_ge_sized(int sz) { arm64_cmp_common_sized("ge", sz); }
static void arm64_cmp_lt_u_sized(int sz) { arm64_cmp_common_sized("lo", sz); }
static void arm64_cmp_le_u_sized(int sz) { arm64_cmp_common_sized("ls", sz); }
static void arm64_cmp_gt_u_sized(int sz) { arm64_cmp_common_sized("hi", sz); }
static void arm64_cmp_ge_u_sized(int sz) { arm64_cmp_common_sized("hs", sz); }

static void arm64_test_imm_setcc(long imm, const char *cond, int size) {
    emit_load_imm64("x9", imm);
    if (size == 8)
        printf("    tst x0, x9\n");
    else
        printf("    tst w0, w9\n");
    printf("    cset w0, %s\n", cond);
}

static void arm64_cmp_imm_setcc(long imm, const char *cond, int size) {
    emit_load_imm64("x9", imm);
    if (size == 8)
        printf("    cmp x0, x9\n");
    else
        printf("    cmp w0, w9\n");
    printf("    cset w0, %s\n", cond);
}


static int arm64_simm9(int offset) {
    return offset >= -256 && offset <= 255;
}

static int arm64_uimm_scaled(int offset, int size) {
    int scale;

    if (offset < 0)
        return 0;
    if (size <= 1)
        scale = 1;
    else if (size == 2)
        scale = 2;
    else if (size == 4)
        scale = 4;
    else
        scale = 8;
    return (offset % scale) == 0 && (offset / scale) <= 4095;
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

    if (value > 0 && (value % 4096) == 0 && (value / 4096) <= 4095) {
        printf("    add %s, %s, #%ld, lsl #12\n", dst, src, value / 4096);
        return;
    }

    if (value < 0 && -value <= 4095) {
        printf("    sub %s, %s, #%ld\n", dst, src, -value);
        return;
    }

    if (value < 0 && ((-value) % 4096) == 0 && ((-value) / 4096) <= 4095) {
        printf("    sub %s, %s, #%ld, lsl #12\n", dst, src, (-value) / 4096);
        return;
    }

    {
        int scratch = arm64_alloc_scratch_reg(arm64_reg_num(dst), arm64_reg_num(src));
        char scratch_reg[4];
        arm64_fill_reg_name(scratch_reg, 'x', scratch);
        emit_load_imm64(scratch_reg, value);
        printf("    add %s, %s, %s\n", dst, src, scratch_reg);
        arm64_release_scratch_reg(scratch);
    }
}

static int arm64_split_large_frame_mem_offset(int frame_offset, int size,
                                              long *base_delta_out,
                                              int *mem_offset_out) {
    long pages;
    long base_delta;
    long mem_offset;

    if (frame_offset >= 0 ||
        arm64_simm9(frame_offset) ||
        arm64_uimm_scaled(frame_offset, size))
        return 0;

    pages = frame_offset / 4096;
    if ((frame_offset % 4096) != 0)
        pages--;
    base_delta = pages * 4096;
    mem_offset = (long)frame_offset - base_delta;

    if ((long)(int)mem_offset != mem_offset)
        return 0;
    if (!arm64_simm9((int)mem_offset) &&
        !arm64_uimm_scaled((int)mem_offset, size))
        return 0;

    *base_delta_out = base_delta;
    *mem_offset_out = (int)mem_offset;
    return 1;
}


static void emit_frame_offset_addr(int offset) {
    emit_add_imm64("x9", arm64_frame_base_reg(), arm64_frame_offset(offset));
}

static void emit_frame_offset_addr_reg(const char *reg, int offset) {
    emit_add_imm64(reg, arm64_frame_base_reg(), arm64_frame_offset(offset));
}

static void arm64_load_local_sized(int offset, int size) {
    int param_reg = arm64_live_param_reg_for_offset(offset);
    int frame_offset = arm64_frame_offset(offset);
    const char *base = arm64_frame_base_reg();
    char xsrc[4];
    char wsrc[4];

    if (param_reg >= 0 && (size == 4 || size == 8)) {
        arm64_fill_reg_name(xsrc, 'x', param_reg);
        arm64_fill_reg_name(wsrc, 'w', param_reg);
        if (size == 8)
            printf("    mov x0, %s\n", xsrc);
        else
            printf("    sxtw x0, %s\n", wsrc);
        return;
    }

    if (arm64_simm9(frame_offset) || arm64_uimm_scaled(frame_offset, size)) {
        if (size == 1)
            printf("    ldrb w0, [%s, #%d]\n", base, frame_offset);
        else if (size == 2)
            printf("    ldrh w0, [%s, #%d]\n", base, frame_offset);
        else if (size == 8)
            printf("    ldr x0, [%s, #%d]\n", base, frame_offset);
        else
            /* ldrsw: sign-extends 32-bit load into 64-bit x0 register.
             * This is critical for passing int values to long parameters. */
            printf("    ldrsw x0, [%s, #%d]\n", base, frame_offset);
        return;
    }

    {
        int scratch = arm64_alloc_scratch_reg(0, 1);
        char scratch_reg[4];
        long base_delta;
        int mem_offset;

        arm64_fill_reg_name(scratch_reg, 'x', scratch);
        if (arm64_split_large_frame_mem_offset(frame_offset, size,
                                               &base_delta, &mem_offset)) {
            emit_add_imm64(scratch_reg, base, base_delta);
            if (size == 1)
                printf("    ldrb w0, [%s, #%d]\n", scratch_reg, mem_offset);
            else if (size == 2)
                printf("    ldrh w0, [%s, #%d]\n", scratch_reg, mem_offset);
            else if (size == 8)
                printf("    ldr x0, [%s, #%d]\n", scratch_reg, mem_offset);
            else
                printf("    ldrsw x0, [%s, #%d]\n", scratch_reg, mem_offset);
            arm64_release_scratch_reg(scratch);
            return;
        }
        emit_frame_offset_addr_reg(scratch_reg, offset);
        if (size == 1)
            printf("    ldrb w0, [%s]\n", scratch_reg);
        else if (size == 2)
            printf("    ldrh w0, [%s]\n", scratch_reg);
        else if (size == 8)
            printf("    ldr x0, [%s]\n", scratch_reg);
        else
            printf("    ldrsw x0, [%s]\n", scratch_reg);
        arm64_release_scratch_reg(scratch);
    }
}

static void arm64_zero_local_range(int start_offset, int size) {
    if (size <= 0)
        return;

    if (size > 32) {
        emit_frame_offset_addr_reg("x0", start_offset);
        printf("    mov w1, #0\n");
        emit_load_imm64("x2", size);
        arm64_call("memset");
        return;
    }

    {
        int scratch = arm64_alloc_scratch_reg(-1, -1);
        char addr_reg[4];

        arm64_fill_reg_name(addr_reg, 'x', scratch);
        emit_frame_offset_addr_reg(addr_reg, start_offset);
        while (size >= 8) {
            printf("    str xzr, [%s], #8\n", addr_reg);
            size -= 8;
        }
        while (size > 0) {
            printf("    strb wzr, [%s], #1\n", addr_reg);
            size--;
        }
        arm64_release_scratch_reg(scratch);
    }
}

static void arm64_load_local_into_arg(int index, int offset, int size) {
    const char *xreg;
    const char *wreg;

    if (index < 0 || index > 7)
        ICE("arm64 arg register out of range");

    xreg = arm64_arg_xregs[index];
    wreg = arm64_arg_wregs[index];
    arm64_load_local_casted_into_reg(xreg, wreg, offset, size, size != 8);
}

static void arm64_load_local_ptr_member_into_arg(int index, int local_offset,
                                                 int member_offset, int size,
                                                 int is_unsigned) {
    const char *xreg;
    const char *wreg;

    if (index < 0 || index > 7)
        ICE("arm64 arg register out of range");

    xreg = arm64_arg_xregs[index];
    wreg = arm64_arg_wregs[index];
    arm64_load_local_ptr_into_reg(xreg, local_offset);

    if (member_offset) {
        if (member_offset > 0 && member_offset <= 4095) {
            if (size == 1)
                printf("    ldrb %s, [%s, #%d]\n", wreg, xreg, member_offset);
            else if (size == 2)
                printf("    ldrh %s, [%s, #%d]\n", wreg, xreg, member_offset);
            else if (size == 8)
                printf("    ldr %s, [%s, #%d]\n", xreg, xreg, member_offset);
            else if (is_unsigned)
                printf("    ldr %s, [%s, #%d]\n", wreg, xreg, member_offset);
            else
                printf("    ldrsw %s, [%s, #%d]\n", xreg, xreg, member_offset);
            return;
        }
        emit_add_imm64(xreg, xreg, member_offset);
    }

    if (size == 1)
        printf("    ldrb %s, [%s]\n", wreg, xreg);
    else if (size == 2)
        printf("    ldrh %s, [%s]\n", wreg, xreg);
    else if (size == 8)
        printf("    ldr %s, [%s]\n", xreg, xreg);
    else if (is_unsigned)
        printf("    ldr %s, [%s]\n", wreg, xreg);
    else
        printf("    ldrsw %s, [%s]\n", xreg, xreg);
}

static void arm64_load_local_into_saved(int offset, int size) {
    (void)size;
    arm64_load_local_ptr_into_reg("x17", offset);
}

static void arm64_load_string_into_arg(int index, int label) {
    const char *xreg;

    if (index < 0 || index > 7)
        ICE("arm64 arg register out of range");

    xreg = arm64_arg_xregs[index];
    printf("    adrp %s, .Lstr%d@PAGE\n", xreg, label);
    printf("    add  %s, %s, .Lstr%d@PAGEOFF\n", xreg, xreg, label);
}

static void arm64_load_func_addr_into_arg(int index, const char *name) {
    const char *xreg;

    if (index < 0 || index > 7)
        ICE("arm64 arg register out of range");

    xreg = arm64_arg_xregs[index];
    if (codegen_get_link_model() == LINK_DYNAMIC) {
        printf("    adrp %s, _%s@GOTPAGE\n", xreg, name);
        printf("    ldr  %s, [%s, _%s@GOTPAGEOFF]\n", xreg, xreg, name);
    } else {
        printf("    adrp %s, _%s@PAGE\n", xreg, name);
        printf("    add  %s, %s, _%s@PAGEOFF\n", xreg, xreg, name);
    }
}

static void arm64_load_global_into_arg(int index, const char *name, int size,
                                       int is_extern, int is_unsigned) {
    const char *xreg;
    const char *wreg;

    if (index < 0 || index > 7)
        ICE("arm64 arg register out of range");

    xreg = arm64_arg_xregs[index];
    wreg = arm64_arg_wregs[index];

    if (arm64_symbol_is_thread_local(name)) {
        arm64_load_global_address_reg(xreg, name);
        if (size == 1)
            printf("    ldrb %s, [%s]\n", wreg, xreg);
        else if (size == 2)
            printf("    ldrh %s, [%s]\n", wreg, xreg);
        else if (size == 8)
            printf("    ldr %s, [%s]\n", xreg, xreg);
        else if (is_unsigned)
            printf("    ldr %s, [%s]\n", wreg, xreg);
        else
            printf("    ldrsw %s, [%s]\n", xreg, xreg);
        return;
    }

    if (is_extern || arm64_symbol_needs_got(name)) {
        if (is_extern && codegen_get_link_model() != LINK_DYNAMIC) {
            printf("    adrp %s, _%s@PAGE\n", xreg, name);
            printf("    add %s, %s, _%s@PAGEOFF\n", xreg, xreg, name);
        } else {
            printf("    adrp %s, _%s@GOTPAGE\n", xreg, name);
            printf("    ldr %s, [%s, _%s@GOTPAGEOFF]\n", xreg, xreg, name);
        }
        if (size == 1)
            printf("    ldrb %s, [%s]\n", wreg, xreg);
        else if (size == 2)
            printf("    ldrh %s, [%s]\n", wreg, xreg);
        else if (size == 8)
            printf("    ldr %s, [%s]\n", xreg, xreg);
        else if (is_unsigned)
            printf("    ldr %s, [%s]\n", wreg, xreg);
        else
            printf("    ldrsw %s, [%s]\n", xreg, xreg);
        return;
    }

    printf("    adrp %s, _%s@PAGE\n", xreg, name);
    if (size == 1)
        printf("    ldrb %s, [%s, _%s@PAGEOFF]\n", wreg, xreg, name);
    else if (size == 2)
        printf("    ldrh %s, [%s, _%s@PAGEOFF]\n", wreg, xreg, name);
    else if (size == 8)
        printf("    ldr %s, [%s, _%s@PAGEOFF]\n", xreg, xreg, name);
    else if (is_unsigned)
        printf("    ldr %s, [%s, _%s@PAGEOFF]\n", wreg, xreg, name);
    else
        printf("    ldrsw %s, [%s, _%s@PAGEOFF]\n", xreg, xreg, name);
}

static void arm64_load_global_member_into_arg(int index, const char *name,
                                              int offset, int size,
                                              int is_extern, int is_unsigned) {
    const char *xreg;
    const char *wreg;

    if (index < 0 || index > 7)
        ICE("arm64 arg register out of range");

    xreg = arm64_arg_xregs[index];
    wreg = arm64_arg_wregs[index];
    (void)is_extern;

    if (!arm64_symbol_is_thread_local(name) &&
        !arm64_symbol_needs_got(name) && offset == 0) {
        printf("    adrp %s, _%s@PAGE\n", xreg, name);
        if (size == 1)
            printf("    ldrb %s, [%s, _%s@PAGEOFF]\n", wreg, xreg, name);
        else if (size == 2)
            printf("    ldrh %s, [%s, _%s@PAGEOFF]\n", wreg, xreg, name);
        else if (size == 8)
            printf("    ldr %s, [%s, _%s@PAGEOFF]\n", xreg, xreg, name);
        else if (is_unsigned)
            printf("    ldr %s, [%s, _%s@PAGEOFF]\n", wreg, xreg, name);
        else
            printf("    ldrsw %s, [%s, _%s@PAGEOFF]\n", xreg, xreg, name);
        return;
    }

    arm64_load_global_address_reg(xreg, name);
    if (offset) {
        if (arm64_load_imm_offset_ok(size, offset)) {
            if (size == 1)
                printf("    ldrb %s, [%s, #%d]\n", wreg, xreg, offset);
            else if (size == 2)
                printf("    ldrh %s, [%s, #%d]\n", wreg, xreg, offset);
            else if (size == 8)
                printf("    ldr %s, [%s, #%d]\n", xreg, xreg, offset);
            else if (is_unsigned)
                printf("    ldr %s, [%s, #%d]\n", wreg, xreg, offset);
            else
                printf("    ldrsw %s, [%s, #%d]\n", xreg, xreg, offset);
            return;
        }
        emit_add_imm64(xreg, xreg, offset);
    }
    if (size == 1)
        printf("    ldrb %s, [%s]\n", wreg, xreg);
    else if (size == 2)
        printf("    ldrh %s, [%s]\n", wreg, xreg);
    else if (size == 8)
        printf("    ldr %s, [%s]\n", xreg, xreg);
    else if (is_unsigned)
        printf("    ldr %s, [%s]\n", wreg, xreg);
    else
        printf("    ldrsw %s, [%s]\n", xreg, xreg);
}

static void arm64_load_global_member_into_reg(const char *xreg, const char *wreg,
                                              const char *name, int offset,
                                              int size, int is_extern,
                                              int is_unsigned) {
    (void)is_extern;

    if (!arm64_symbol_is_thread_local(name) &&
        !arm64_symbol_needs_got(name) && offset == 0) {
        printf("    adrp %s, _%s@PAGE\n", xreg, name);
        if (size == 1)
            printf("    ldrb %s, [%s, _%s@PAGEOFF]\n", wreg, xreg, name);
        else if (size == 2)
            printf("    ldrh %s, [%s, _%s@PAGEOFF]\n", wreg, xreg, name);
        else if (size == 8)
            printf("    ldr %s, [%s, _%s@PAGEOFF]\n", xreg, xreg, name);
        else if (is_unsigned)
            printf("    ldr %s, [%s, _%s@PAGEOFF]\n", wreg, xreg, name);
        else
            printf("    ldrsw %s, [%s, _%s@PAGEOFF]\n", xreg, xreg, name);
        return;
    }

    arm64_load_global_address_reg(xreg, name);
    if (offset) {
        if (arm64_load_imm_offset_ok(size, offset)) {
            if (size == 1)
                printf("    ldrb %s, [%s, #%d]\n", wreg, xreg, offset);
            else if (size == 2)
                printf("    ldrh %s, [%s, #%d]\n", wreg, xreg, offset);
            else if (size == 8)
                printf("    ldr %s, [%s, #%d]\n", xreg, xreg, offset);
            else if (is_unsigned)
                printf("    ldr %s, [%s, #%d]\n", wreg, xreg, offset);
            else
                printf("    ldrsw %s, [%s, #%d]\n", xreg, xreg, offset);
            return;
        }
        emit_add_imm64(xreg, xreg, offset);
    }

    if (size == 1)
        printf("    ldrb %s, [%s]\n", wreg, xreg);
    else if (size == 2)
        printf("    ldrh %s, [%s]\n", wreg, xreg);
    else if (size == 8)
        printf("    ldr %s, [%s]\n", xreg, xreg);
    else if (is_unsigned)
        printf("    ldr %s, [%s]\n", wreg, xreg);
    else
        printf("    ldrsw %s, [%s]\n", xreg, xreg);
}

static void arm64_load_local_casted(int offset, int load_size, int is_unsigned) {
    arm64_load_local_casted_into_reg("x0", "w0", offset, load_size, is_unsigned);
}

static void arm64_emit_local_int_to_fp_const_div_return(int offset, int load_size,
                                                        int is_unsigned,
                                                        unsigned long long const_bits,
                                                        int fp_size) {
    arm64_load_local_casted_into_reg("x9", "w9", offset, load_size, is_unsigned);
    printf("    %s %c0, x9\n",
           is_unsigned ? "ucvtf" : "scvtf",
           fp_size == 4 ? 's' : 'd');
    emit_load_imm64("x10", (long)const_bits);
    if (fp_size == 4)
        printf("    fmov s1, w10\n");
    else
        printf("    fmov d1, x10\n");
    printf("    fdiv %c0, %c0, %c1\n",
           fp_size == 4 ? 's' : 'd',
           fp_size == 4 ? 's' : 'd',
           fp_size == 4 ? 's' : 'd');
}

static void arm64_emit_store_global_member_indexed_from_global_member(const char *name,
                                                                      int base_offset, int index_scale,
                                                                      const char *index_name,
                                                                      int index_member_offset,
                                                                      int index_load_size,
                                                                      int index_is_unsigned,
                                                                      const char *value_name,
                                                                      int value_offset,
                                                                      int value_load_size,
                                                                      int value_is_unsigned,
                                                                      int store_size) {
    int shift = arm64_scale_shift(index_scale);
    int max_shift = (store_size == 8) ? 3 : 2;
    int use_indexed = (shift >= 0 && shift <= max_shift);
    int same_base = (strcmp(name, index_name) == 0 && strcmp(name, value_name) == 0);

    if (same_base) {
        arm64_load_global_address_reg("x11", name);

        if (value_load_size == 8) {
            if (value_offset != 0)
                printf("    ldr x10, [x11, #%d]\n", value_offset);
            else
                printf("    ldr x10, [x11]\n");
        } else if (value_load_size == 4) {
            if (value_is_unsigned) {
                if (value_offset != 0)
                    printf("    ldr w10, [x11, #%d]\n", value_offset);
                else
                    printf("    ldr w10, [x11]\n");
            } else {
                if (value_offset != 0)
                    printf("    ldrsw x10, [x11, #%d]\n", value_offset);
                else
                    printf("    ldrsw x10, [x11]\n");
            }
        } else {
            ICE("arm64 global-member value load unsupported size=%d", value_load_size);
        }

        if (index_load_size == 8) {
            if (index_member_offset != 0)
                printf("    ldr x9, [x11, #%d]\n", index_member_offset);
            else
                printf("    ldr x9, [x11]\n");
        } else if (index_load_size == 4) {
            if (index_is_unsigned) {
                if (index_member_offset != 0)
                    printf("    ldr w9, [x11, #%d]\n", index_member_offset);
                else
                    printf("    ldr w9, [x11]\n");
            } else {
                if (index_member_offset != 0)
                    printf("    ldrsw x9, [x11, #%d]\n", index_member_offset);
                else
                    printf("    ldrsw x9, [x11]\n");
            }
        } else {
            ICE("arm64 global-member index load unsupported size=%d", index_load_size);
        }

        if (base_offset != 0) {
            emit_load_imm64("x12", base_offset);
            printf("    add x11, x11, x12\n");
        }
    } else {
        arm64_load_global_address_reg("x12", index_name);
        if (index_member_offset != 0)
            emit_add_imm64("x12", "x12", index_member_offset);
        if (index_load_size == 8)
            printf("    ldr x9, [x12]\n");
        else if (index_load_size == 4) {
            if (index_is_unsigned)
                printf("    ldr w9, [x12]\n");
            else
                printf("    ldrsw x9, [x12]\n");
        } else {
            ICE("arm64 global-member index load unsupported size=%d", index_load_size);
        }

        arm64_load_global_address_reg("x13", value_name);
        if (value_offset != 0)
            emit_add_imm64("x13", "x13", value_offset);
        if (value_load_size == 8)
            printf("    ldr x10, [x13]\n");
        else if (value_load_size == 4) {
            if (value_is_unsigned)
                printf("    ldr w10, [x13]\n");
            else
                printf("    ldrsw x10, [x13]\n");
        } else {
            ICE("arm64 global-member value load unsupported size=%d", value_load_size);
        }

        arm64_load_global_address_reg("x11", name);
        if (base_offset != 0)
            emit_add_imm64("x11", "x11", base_offset);
    }

    if (use_indexed) {
        if (store_size == 8) {
            if (shift > 0)
                printf("    str x10, [x11, x9, lsl #%d]\n", shift);
            else
                printf("    str x10, [x11, x9]\n");
        } else if (store_size == 4) {
            if (shift > 0)
                printf("    str w10, [x11, x9, lsl #%d]\n", shift);
            else
                printf("    str w10, [x11, x9]\n");
        } else {
            ICE("arm64 indexed global-member store unsupported store_size=%d", store_size);
        }
        return;
    }

    emit_load_imm64("x12", index_scale);
    printf("    madd x11, x9, x12, x11\n");
    if (store_size == 8)
        printf("    str x10, [x11]\n");
    else if (store_size == 4)
        printf("    str w10, [x11]\n");
    else
        ICE("arm64 indexed global-member store unsupported store_size=%d", store_size);
}

static void arm64_emit_include_cache_insert_index_body(int idx_offset) {
    int idx_num = arm64_alloc_scratch_reg(-1, -1);
    int entry_num = arm64_alloc_scratch_reg(-1, -1);
    int bucket_num = arm64_alloc_scratch_reg(-1, -1);
    int head_num = arm64_alloc_scratch_reg(-1, -1);
    char idx_x[4], idx_w[4], entry_x[4], bucket_x[4], head_w[4];

    arm64_fill_reg_name(idx_x, 'x', idx_num);
    arm64_fill_reg_name(idx_w, 'w', idx_num);
    arm64_fill_reg_name(entry_x, 'x', entry_num);
    arm64_fill_reg_name(bucket_x, 'x', bucket_num);
    arm64_fill_reg_name(head_w, 'w', head_num);

    arm64_load_local_casted_into_reg(idx_x, idx_w, idx_offset, 4, 0);
    arm64_load_global_address_reg(entry_x, "include_cache_entries");
    printf("    add %s, %s, %s, lsl #2\n", bucket_x, idx_x, idx_x);
    printf("    add %s, %s, %s, lsl #3\n", entry_x, entry_x, bucket_x);
    printf("    ldr x0, [%s]\n", entry_x);
    printf("    ldr w1, [%s, #8]\n", entry_x);
    printf("    ldr w2, [%s, #12]\n", entry_x);
    arm64_call("include_cache_hash_key");

    arm64_load_local_casted_into_reg(idx_x, idx_w, idx_offset, 4, 0);
    arm64_load_global_address_reg(entry_x, "include_cache_entries");
    printf("    add %s, %s, %s, lsl #2\n", bucket_x, idx_x, idx_x);
    printf("    add %s, %s, %s, lsl #3\n", entry_x, entry_x, bucket_x);
    arm64_load_global_address_reg(bucket_x, "include_cache_buckets");
    printf("    ldr %s, [%s, w0, uxtw #2]\n", head_w, bucket_x);
    printf("    str %s, [%s, #32]\n", head_w, entry_x);
    printf("    str %s, [%s, w0, uxtw #2]\n", idx_w, bucket_x);

    arm64_release_scratch_reg(head_num);
    arm64_release_scratch_reg(bucket_num);
    arm64_release_scratch_reg(entry_num);
    arm64_release_scratch_reg(idx_num);
}

static void arm64_emit_include_cache_insert_index_tail(int idx_offset, int bucket_offset) {
    int idx_num = arm64_alloc_scratch_reg(-1, -1);
    int entry_num = arm64_alloc_scratch_reg(-1, -1);
    int bucket_idx_num = arm64_alloc_scratch_reg(-1, -1);
    int bucket_num = arm64_alloc_scratch_reg(-1, -1);
    int head_num = arm64_alloc_scratch_reg(-1, -1);
    char idx_x[4], idx_w[4], entry_x[4], bucket_idx_x[4], bucket_idx_w[4];
    char bucket_x[4], head_w[4];

    arm64_fill_reg_name(idx_x, 'x', idx_num);
    arm64_fill_reg_name(idx_w, 'w', idx_num);
    arm64_fill_reg_name(entry_x, 'x', entry_num);
    arm64_fill_reg_name(bucket_idx_x, 'x', bucket_idx_num);
    arm64_fill_reg_name(bucket_idx_w, 'w', bucket_idx_num);
    arm64_fill_reg_name(bucket_x, 'x', bucket_num);
    arm64_fill_reg_name(head_w, 'w', head_num);

    arm64_load_local_casted_into_reg(idx_x, idx_w, idx_offset, 4, 0);
    arm64_load_global_address_reg(entry_x, "include_cache_entries");
    printf("    add %s, %s, %s, lsl #2\n", bucket_x, idx_x, idx_x);
    printf("    add %s, %s, %s, lsl #3\n", entry_x, entry_x, bucket_x);
    arm64_load_local_casted_into_reg(bucket_idx_x, bucket_idx_w, bucket_offset, 4, 1);
    arm64_load_global_address_reg(bucket_x, "include_cache_buckets");
    printf("    ldr %s, [%s, %s, uxtw #2]\n", head_w, bucket_x, bucket_idx_w);
    printf("    str %s, [%s, #32]\n", head_w, entry_x);
    printf("    str %s, [%s, %s, uxtw #2]\n", idx_w, bucket_x, bucket_idx_w);

    arm64_release_scratch_reg(head_num);
    arm64_release_scratch_reg(bucket_num);
    arm64_release_scratch_reg(bucket_idx_num);
    arm64_release_scratch_reg(entry_num);
    arm64_release_scratch_reg(idx_num);
}

static void arm64_emit_ifc_ident_body(int idx_offset) {
    int idx_num = arm64_alloc_scratch_reg(-1, -1);
    int entry_num = arm64_alloc_scratch_reg(-1, -1);
    int path_num = arm64_alloc_scratch_reg(-1, -1);
    int hash_num = arm64_alloc_scratch_reg(-1, -1);
    int temp_num = arm64_alloc_scratch_reg(-1, -1);
    int bucket_idx_num = arm64_alloc_scratch_reg(-1, -1);
    char idx_x[4], idx_w[4], entry_x[4], path_x[4], path_w[4], hash_w[4];
    char temp_x[4], temp_w[4], bucket_idx_w[4];

    arm64_fill_reg_name(idx_x, 'x', idx_num);
    arm64_fill_reg_name(idx_w, 'w', idx_num);
    arm64_fill_reg_name(entry_x, 'x', entry_num);
    arm64_fill_reg_name(path_x, 'x', path_num);
    arm64_fill_reg_name(path_w, 'w', path_num);
    arm64_fill_reg_name(hash_w, 'w', hash_num);
    arm64_fill_reg_name(temp_x, 'x', temp_num);
    arm64_fill_reg_name(temp_w, 'w', temp_num);
    arm64_fill_reg_name(bucket_idx_w, 'w', bucket_idx_num);

    arm64_load_local_casted_into_reg(idx_x, idx_w, idx_offset, 4, 0);
    arm64_load_global_address_reg(entry_x, "include_file_cache_entries");
    printf("    add %s, %s, %s, lsl #1\n", temp_x, idx_x, idx_x);
    printf("    add %s, %s, %s, lsl #2\n", temp_x, temp_x, temp_x);
    printf("    add %s, %s, %s, lsl #3\n", entry_x, entry_x, temp_x);
    printf("    ldr %s, [%s, #32]\n", path_x, entry_x);
    printf("    ldr %s, [%s, #24]\n", hash_w, entry_x);
    printf("    eor %s, %s, %s\n", hash_w, hash_w, path_w);
    printf("    lsr %s, %s, #32\n", temp_x, path_x);
    printf("    eor %s, %s, %s\n", hash_w, hash_w, temp_w);
    emit_load_imm64(temp_x, 16777619);
    printf("    mul %s, %s, %s\n", hash_w, hash_w, temp_w);
    printf("    and %s, %s, #0x3ff\n", bucket_idx_w, hash_w);
    arm64_load_global_address_reg(path_x, "include_file_cache_identity_buckets");
    printf("    ldr %s, [%s, %s, uxtw #2]\n", temp_w, path_x, bucket_idx_w);
    printf("    str %s, [%s, #116]\n", temp_w, entry_x);
    printf("    str %s, [%s, %s, uxtw #2]\n", idx_w, path_x, bucket_idx_w);

    arm64_release_scratch_reg(bucket_idx_num);
    arm64_release_scratch_reg(temp_num);
    arm64_release_scratch_reg(hash_num);
    arm64_release_scratch_reg(path_num);
    arm64_release_scratch_reg(entry_num);
    arm64_release_scratch_reg(idx_num);
}

static void arm64_emit_push_loop_tail(int index_offset, int break_offset,
                                      int continue_offset, int depth_ptr_offset) {
    int idx_num = arm64_alloc_scratch_reg(-1, -1);
    int value_num = arm64_alloc_scratch_reg(-1, -1);
    int base_num = arm64_alloc_scratch_reg(-1, -1);
    char idx_x[4], idx_w[4], value_x[4], value_w[4], base_x[4];

    arm64_fill_reg_name(idx_x, 'x', idx_num);
    arm64_fill_reg_name(idx_w, 'w', idx_num);
    arm64_fill_reg_name(value_x, 'x', value_num);
    arm64_fill_reg_name(value_w, 'w', value_num);
    arm64_fill_reg_name(base_x, 'x', base_num);

    arm64_load_local_casted_into_reg(idx_x, idx_w, index_offset, 4, 1);

    arm64_load_local_casted_into_reg(value_x, value_w, break_offset, 4, 0);
    arm64_load_global_address_reg(base_x, "break_labels");
    printf("    str %s, [%s, %s, uxtw #2]\n", value_w, base_x, idx_w);

    arm64_load_local_casted_into_reg(value_x, value_w, continue_offset, 4, 0);
    arm64_load_global_address_reg(base_x, "continue_labels");
    printf("    str %s, [%s, %s, uxtw #2]\n", value_w, base_x, idx_w);

    printf("    add %s, %s, #1\n", value_w, idx_w);
    arm64_load_local_ptr_into_reg(base_x, depth_ptr_offset);
    printf("    str %s, [%s]\n", value_w, base_x);

    arm64_release_scratch_reg(base_num);
    arm64_release_scratch_reg(value_num);
    arm64_release_scratch_reg(idx_num);
}

static void arm64_emit_ir_push_loop_tail(int index_offset, int break_offset,
                                         int continue_offset, int depth_ptr_offset) {
    int idx_num = arm64_alloc_scratch_reg(-1, -1);
    int base_num = arm64_alloc_scratch_reg(-1, -1);
    int offset_num = arm64_alloc_scratch_reg(-1, -1);
    int slot_num = arm64_alloc_scratch_reg(-1, -1);
    int value_num = arm64_alloc_scratch_reg(-1, -1);
    char idx_x[4], idx_w[4], base_x[4], offset_x[4], slot_x[4], value_x[4], value_w[4];

    arm64_fill_reg_name(idx_x, 'x', idx_num);
    arm64_fill_reg_name(idx_w, 'w', idx_num);
    arm64_fill_reg_name(base_x, 'x', base_num);
    arm64_fill_reg_name(offset_x, 'x', offset_num);
    arm64_fill_reg_name(slot_x, 'x', slot_num);
    arm64_fill_reg_name(value_x, 'x', value_num);
    arm64_fill_reg_name(value_w, 'w', value_num);

    arm64_load_local_casted_into_reg(idx_x, idx_w, index_offset, 4, 1);
    arm64_load_global_address_reg(base_x, "irgen");
    emit_load_imm64(offset_x, 212);
    printf("    add %s, %s, %s\n", base_x, base_x, offset_x);
    printf("    add %s, %s, %s, lsl #3\n", slot_x, base_x, idx_x);

    arm64_load_local_casted_into_reg(value_x, value_w, break_offset, 4, 0);
    printf("    str %s, [%s]\n", value_w, slot_x);

    arm64_load_local_casted_into_reg(value_x, value_w, continue_offset, 4, 0);
    printf("    str %s, [%s, #4]\n", value_w, slot_x);

    printf("    add %s, %s, #1\n", value_w, idx_w);
    arm64_load_local_ptr_into_reg(base_x, depth_ptr_offset);
    printf("    str %s, [%s]\n", value_w, base_x);

    arm64_release_scratch_reg(value_num);
    arm64_release_scratch_reg(slot_num);
    arm64_release_scratch_reg(offset_num);
    arm64_release_scratch_reg(base_num);
    arm64_release_scratch_reg(idx_num);
}

static void arm64_load_local_casted_into_reg(const char *xreg, const char *wreg,
                                             int offset, int load_size, int is_unsigned) {
    int param_reg = arm64_live_param_reg_for_offset(offset);
    int frame_offset = arm64_frame_offset(offset);
    const char *base = arm64_frame_base_reg();
    char xsrc[4];
    char wsrc[4];

    if (param_reg >= 0 && (load_size == 4 || load_size == 8)) {
        arm64_fill_reg_name(xsrc, 'x', param_reg);
        arm64_fill_reg_name(wsrc, 'w', param_reg);
        if (load_size == 8) {
            if (strcmp(xreg, xsrc) != 0)
                printf("    mov %s, %s\n", xreg, xsrc);
        } else if (is_unsigned) {
            if (strcmp(wreg, wsrc) != 0)
                printf("    mov %s, %s\n", wreg, wsrc);
        } else {
            printf("    sxtw %s, %s\n", xreg, wsrc);
        }
        return;
    }

    if (arm64_simm9(frame_offset) || arm64_uimm_scaled(frame_offset, load_size)) {
        if (load_size == 1) {
            if (is_unsigned)
                printf("    ldrb %s, [%s, #%d]\n", wreg, base, frame_offset);
            else
                printf("    ldrsb %s, [%s, #%d]\n", xreg, base, frame_offset);
        } else if (load_size == 2) {
            if (is_unsigned)
                printf("    ldrh %s, [%s, #%d]\n", wreg, base, frame_offset);
            else
                printf("    ldrsh %s, [%s, #%d]\n", xreg, base, frame_offset);
        } else if (load_size == 8) {
            printf("    ldr %s, [%s, #%d]\n", xreg, base, frame_offset);
        } else if (is_unsigned) {
            printf("    ldr %s, [%s, #%d]\n", wreg, base, frame_offset);
        } else {
            printf("    ldrsw %s, [%s, #%d]\n", xreg, base, frame_offset);
        }
        return;
    }

    {
        int scratch = arm64_alloc_scratch_reg(arm64_reg_num(xreg), arm64_reg_num(wreg));
        char scratch_reg[4];
        long base_delta;
        int mem_offset;

        arm64_fill_reg_name(scratch_reg, 'x', scratch);
        if (arm64_split_large_frame_mem_offset(frame_offset, load_size,
                                               &base_delta, &mem_offset)) {
            emit_add_imm64(scratch_reg, base, base_delta);
            if (load_size == 1) {
                if (is_unsigned)
                    printf("    ldrb %s, [%s, #%d]\n", wreg, scratch_reg, mem_offset);
                else
                    printf("    ldrsb %s, [%s, #%d]\n", xreg, scratch_reg, mem_offset);
            } else if (load_size == 2) {
                if (is_unsigned)
                    printf("    ldrh %s, [%s, #%d]\n", wreg, scratch_reg, mem_offset);
                else
                    printf("    ldrsh %s, [%s, #%d]\n", xreg, scratch_reg, mem_offset);
            } else if (load_size == 8) {
                printf("    ldr %s, [%s, #%d]\n", xreg, scratch_reg, mem_offset);
            } else if (is_unsigned) {
                printf("    ldr %s, [%s, #%d]\n", wreg, scratch_reg, mem_offset);
            } else {
                printf("    ldrsw %s, [%s, #%d]\n", xreg, scratch_reg, mem_offset);
            }
            arm64_release_scratch_reg(scratch);
            return;
        }
        emit_frame_offset_addr_reg(scratch_reg, offset);
        if (load_size == 1) {
            if (is_unsigned)
                printf("    ldrb %s, [%s]\n", wreg, scratch_reg);
            else
                printf("    ldrsb %s, [%s]\n", xreg, scratch_reg);
        } else if (load_size == 2) {
            if (is_unsigned)
                printf("    ldrh %s, [%s]\n", wreg, scratch_reg);
            else
                printf("    ldrsh %s, [%s]\n", xreg, scratch_reg);
        } else if (load_size == 8) {
            printf("    ldr %s, [%s]\n", xreg, scratch_reg);
        } else if (is_unsigned) {
            printf("    ldr %s, [%s]\n", wreg, scratch_reg);
        } else {
            printf("    ldrsw %s, [%s]\n", xreg, scratch_reg);
        }
        arm64_release_scratch_reg(scratch);
    }
}

static void arm64_load_local_ptr_into_reg(const char *reg, int offset) {
    int param_reg = arm64_live_param_reg_for_offset(offset);
    int frame_offset = arm64_frame_offset(offset);
    const char *base = arm64_frame_base_reg();
    char xsrc[4];

    if (param_reg >= 0) {
        arm64_fill_reg_name(xsrc, 'x', param_reg);
        if (strcmp(reg, xsrc) != 0)
            printf("    mov %s, %s\n", reg, xsrc);
        return;
    }

    if (arm64_simm9(frame_offset) || arm64_uimm_scaled(frame_offset, TCC_SIZEOF_PTR))
        printf("    ldr %s, [%s, #%d]\n", reg, base, frame_offset);
    else {
        int scratch = arm64_alloc_scratch_reg(arm64_reg_num(reg), -1);
        char scratch_reg[4];
        arm64_fill_reg_name(scratch_reg, 'x', scratch);
        emit_frame_offset_addr_reg(scratch_reg, offset);
        printf("    ldr %s, [%s]\n", reg, scratch_reg);
        arm64_release_scratch_reg(scratch);
    }
}

static void arm64_load_local_ptr_member(int local_offset, int member_offset, int size) {
    arm64_load_local_ptr_into_reg("x0", local_offset);
    arm64_load_member_ptr(member_offset, size);
}

static void arm64_load_local_ptr_member_casted(int local_offset, int member_offset,
                                               int load_size, int cast_size, int is_unsigned) {
    arm64_load_local_ptr_into_reg("x0", local_offset);
    arm64_load_member_ptr_casted(member_offset, load_size, cast_size, is_unsigned);
}

static void arm64_cmp_local_ptr_member_imm_bool(int local_offset, int member_offset,
                                                int load_size, int cast_size, int is_unsigned,
                                                const char *op, long imm) {
    const char *cond = NULL;

    if (strcmp(op, "eq") == 0)
        cond = "eq";
    else if (strcmp(op, "ne") == 0)
        cond = "ne";
    else
        ICE("unsupported arm64 local ptr member compare op: %s", op ? op : "<null>");

    arm64_load_local_ptr_member_casted(local_offset, member_offset, load_size, cast_size, is_unsigned);
    if (imm >= 0 && imm <= 4095) {
        if (cast_size <= 4)
            printf("    cmp w0, #%ld\n", imm);
        else
            printf("    cmp x0, #%ld\n", imm);
    } else {
        emit_load_imm64("x9", imm);
        if (cast_size <= 4)
            printf("    cmp w0, w9\n");
        else
            printf("    cmp x0, x9\n");
    }
    printf("    cset w0, %s\n", cond);
}

static void arm64_cmp_local_global_addr_bool(int local_offset, const char *name,
                                             int is_extern, const char *op) {
    const char *cond = NULL;
    (void)is_extern;

    if (strcmp(op, "eq") == 0)
        cond = "eq";
    else if (strcmp(op, "ne") == 0)
        cond = "ne";
    else
        ICE("unsupported arm64 local/global addr compare op: %s", op ? op : "<null>");

    arm64_load_local_ptr_into_reg("x1", local_offset);
    if (is_extern || codegen_get_link_model() == LINK_DYNAMIC) {
        printf("    adrp x9, _%s@GOTPAGE\n", name);
        printf("    ldr  x9, [x9, _%s@GOTPAGEOFF]\n", name);
    } else {
        printf("    adrp x9, _%s@PAGE\n", name);
        printf("    add  x9, x9, _%s@PAGEOFF\n", name);
    }
    printf("    cmp x1, x9\n");
    printf("    cset w0, %s\n", cond);
}

static void arm64_cmp_local_ptr_member_global_addr_bool(int local_offset, int member_offset,
                                                        int load_size, const char *name,
                                                        int is_extern, const char *op) {
    const char *cond = NULL;
    (void)is_extern;
    (void)load_size;

    if (strcmp(op, "eq") == 0)
        cond = "eq";
    else if (strcmp(op, "ne") == 0)
        cond = "ne";
    else
        ICE("unsupported arm64 local ptr/global addr compare op: %s", op ? op : "<null>");

    arm64_load_local_ptr_member(local_offset, member_offset, 8);
    if (is_extern || codegen_get_link_model() == LINK_DYNAMIC) {
        printf("    adrp x9, _%s@GOTPAGE\n", name);
        printf("    ldr  x9, [x9, _%s@GOTPAGEOFF]\n", name);
    } else {
        printf("    adrp x9, _%s@PAGE\n", name);
        printf("    add  x9, x9, _%s@PAGEOFF\n", name);
    }
    printf("    cmp x0, x9\n");
    printf("    cset w0, %s\n", cond);
}

static void arm64_load_local_ptr_member_bitfield(int local_offset, int member_offset,
                                                 int load_size, int bit_offset,
                                                 int bit_width, int is_unsigned) {
    unsigned long mask;
    int narrow_size = load_size;
    int narrow_bits;
    int sign_shift;

    if (bit_offset >= 0 && bit_width > 0) {
        if (bit_offset + bit_width <= 8)
            narrow_size = 1;
        else if (bit_offset + bit_width <= 16)
            narrow_size = 2;
        else if (bit_offset + bit_width <= 32)
            narrow_size = 4;
        else
            narrow_size = 8;
    }

    arm64_load_local_ptr_into_reg("x1", local_offset);
    if (narrow_size == 1)
        printf("    ldrb w0, [x1, #%d]\n", member_offset);
    else if (narrow_size == 2)
        printf("    ldrh w0, [x1, #%d]\n", member_offset);
    else if (narrow_size == 4)
        printf("    ldr w0, [x1, #%d]\n", member_offset);
    else
        printf("    ldr x0, [x1, #%d]\n", member_offset);

    if (bit_offset > 0) {
        if (narrow_size == 8)
            printf("    lsr x0, x0, #%d\n", bit_offset);
        else
            printf("    lsr w0, w0, #%d\n", bit_offset);
    }

    narrow_bits = narrow_size * 8;
    if (bit_width > 0 && bit_width < narrow_bits) {
        mask = (bit_width >= 64) ? ~0UL : ((1UL << bit_width) - 1UL);
        if (narrow_size == 8)
            printf("    and x0, x0, #0x%lx\n", mask);
        else
            printf("    and w0, w0, #0x%lx\n", mask);
    }

    if (!is_unsigned && bit_width > 0 && bit_width < narrow_bits) {
        sign_shift = narrow_bits - bit_width;
        if (narrow_size == 8) {
            printf("    lsl x0, x0, #%d\n", sign_shift);
            printf("    asr x0, x0, #%d\n", sign_shift);
        } else {
            printf("    lsl w0, w0, #%d\n", sign_shift);
            printf("    asr w0, w0, #%d\n", sign_shift);
        }
    }
}

static void arm64_update_local_ptr_member_bitfield_const(int local_offset, int member_offset,
                                                         int load_size, long clear_mask,
                                                         long set_bits) {
    arm64_load_local_ptr_into_reg("x1", local_offset);
    if (load_size == 1)
        printf("    ldrb w0, [x1, #%d]\n", member_offset);
    else if (load_size == 2)
        printf("    ldrh w0, [x1, #%d]\n", member_offset);
    else if (load_size == 4)
        printf("    ldr w0, [x1, #%d]\n", member_offset);
    else
        printf("    ldr x0, [x1, #%d]\n", member_offset);

    if (load_size == 8) {
        emit_load_imm64("x9", clear_mask);
        printf("    and x0, x0, x9\n");
        if (set_bits != 0) {
            emit_load_imm64("x9", set_bits);
            printf("    orr x0, x0, x9\n");
        }
        printf("    str x0, [x1, #%d]\n", member_offset);
        return;
    }

    emit_load_imm64("x9", clear_mask);
    printf("    and x0, x0, x9\n");
    if (set_bits != 0) {
        emit_load_imm64("x9", set_bits);
        printf("    orr x0, x0, x9\n");
    }

    if (load_size == 1)
        printf("    strb w0, [x1, #%d]\n", member_offset);
    else if (load_size == 2)
        printf("    strh w0, [x1, #%d]\n", member_offset);
    else
        printf("    str w0, [x1, #%d]\n", member_offset);
}

static void arm64_update_local_ptr_member_imm(int local_offset, int member_offset,
                                              int load_size, const char *op, long imm) {
    long delta = 0;
    int use_w = (load_size <= 4);

    arm64_load_local_ptr_into_reg("x1", local_offset);
    if (load_size == 1)
        printf("    ldrb w0, [x1, #%d]\n", member_offset);
    else if (load_size == 2)
        printf("    ldrh w0, [x1, #%d]\n", member_offset);
    else if (load_size == 4)
        printf("    ldrsw x0, [x1, #%d]\n", member_offset);
    else if (load_size == 8)
        printf("    ldr x0, [x1, #%d]\n", member_offset);
    else
        ICE("arm64 local pointer member immediate update unsupported load_size=%d", load_size);

    if (strcmp(op, "add") == 0 || strcmp(op, "sub") == 0) {
        delta = (strcmp(op, "sub") == 0) ? -imm : imm;
        emit_add_imm64("x0", "x0", delta);
    } else if (strcmp(op, "and") == 0) {
        unsigned long mask = (unsigned long)imm;
        if (mask != 0 &&
            (((imm >= 0) && (((mask + 1UL) & mask) == 0)) ||
             ((imm < 0) && ((((~mask) + 1UL) & (~mask)) == 0)))) {
            printf("    and %s0, %s0, #0x%lx\n", use_w ? "w" : "x", use_w ? "w" : "x", mask);
        } else {
            int mask_scratch = arm64_alloc_scratch_reg(0, 1);
            char mask_x[4];
            char mask_w[4];
            arm64_fill_reg_name(mask_x, 'x', mask_scratch);
            arm64_fill_reg_name(mask_w, 'w', mask_scratch);
            emit_load_imm64(mask_x, imm);
            printf("    and %s0, %s0, %s\n", use_w ? "w" : "x", use_w ? "w" : "x",
                   use_w ? mask_w : mask_x);
            arm64_release_scratch_reg(mask_scratch);
        }
    } else if (strcmp(op, "or") == 0) {
        unsigned long mask = (unsigned long)imm;
        if (mask != 0 && ((mask & (mask - 1UL)) == 0)) {
            printf("    orr %s0, %s0, #0x%lx\n", use_w ? "w" : "x", use_w ? "w" : "x", mask);
        } else {
            int mask_scratch = arm64_alloc_scratch_reg(0, 1);
            char mask_x[4];
            char mask_w[4];
            arm64_fill_reg_name(mask_x, 'x', mask_scratch);
            arm64_fill_reg_name(mask_w, 'w', mask_scratch);
            emit_load_imm64(mask_x, imm);
            printf("    orr %s0, %s0, %s\n", use_w ? "w" : "x", use_w ? "w" : "x",
                   use_w ? mask_w : mask_x);
            arm64_release_scratch_reg(mask_scratch);
        }
    } else {
        ICE("arm64 local pointer member immediate update unsupported op=%s", op ? op : "<null>");
    }

    if (load_size == 1)
        printf("    strb w0, [x1, #%d]\n", member_offset);
    else if (load_size == 2)
        printf("    strh w0, [x1, #%d]\n", member_offset);
    else if (load_size == 4)
        printf("    str w0, [x1, #%d]\n", member_offset);
    else
        printf("    str x0, [x1, #%d]\n", member_offset);
}

static void arm64_update_global_member_imm(const char *name, int member_offset,
                                           int load_size, const char *op, long imm) {
    long delta = 0;
    int use_w = (load_size <= 4);
    const char *base = "x1";
    int offset = member_offset;

    arm64_load_global_address_reg("x1", name);
    if (!(arm64_simm9(offset) || arm64_uimm_scaled(offset, load_size))) {
        emit_add_imm64("x1", "x1", offset);
        offset = 0;
    }

    if (load_size == 1)
        printf("    ldrb w0, [%s, #%d]\n", base, offset);
    else if (load_size == 2)
        printf("    ldrh w0, [%s, #%d]\n", base, offset);
    else if (load_size == 4)
        printf("    ldrsw x0, [%s, #%d]\n", base, offset);
    else if (load_size == 8)
        printf("    ldr x0, [%s, #%d]\n", base, offset);
    else
        ICE("arm64 global member immediate update unsupported load_size=%d", load_size);

    if (strcmp(op, "add") == 0 || strcmp(op, "sub") == 0) {
        delta = (strcmp(op, "sub") == 0) ? -imm : imm;
        emit_add_imm64("x0", "x0", delta);
    } else if (strcmp(op, "and") == 0) {
        unsigned long mask = (unsigned long)imm;
        if (mask != 0 &&
            (((imm >= 0) && (((mask + 1UL) & mask) == 0)) ||
             ((imm < 0) && ((((~mask) + 1UL) & (~mask)) == 0)))) {
            printf("    and %s0, %s0, #0x%lx\n", use_w ? "w" : "x", use_w ? "w" : "x", mask);
        } else {
            int mask_scratch = arm64_alloc_scratch_reg(0, 1);
            char mask_x[4];
            char mask_w[4];
            arm64_fill_reg_name(mask_x, 'x', mask_scratch);
            arm64_fill_reg_name(mask_w, 'w', mask_scratch);
            emit_load_imm64(mask_x, imm);
            printf("    and %s0, %s0, %s\n", use_w ? "w" : "x", use_w ? "w" : "x",
                   use_w ? mask_w : mask_x);
            arm64_release_scratch_reg(mask_scratch);
        }
    } else if (strcmp(op, "or") == 0) {
        unsigned long mask = (unsigned long)imm;
        if (mask != 0 && ((mask & (mask - 1UL)) == 0)) {
            printf("    orr %s0, %s0, #0x%lx\n", use_w ? "w" : "x", use_w ? "w" : "x", mask);
        } else {
            int mask_scratch = arm64_alloc_scratch_reg(0, 1);
            char mask_x[4];
            char mask_w[4];
            arm64_fill_reg_name(mask_x, 'x', mask_scratch);
            arm64_fill_reg_name(mask_w, 'w', mask_scratch);
            emit_load_imm64(mask_x, imm);
            printf("    orr %s0, %s0, %s\n", use_w ? "w" : "x", use_w ? "w" : "x",
                   use_w ? mask_w : mask_x);
            arm64_release_scratch_reg(mask_scratch);
        }
    } else {
        ICE("arm64 global member immediate update unsupported op=%s", op ? op : "<null>");
    }

    if (load_size == 1)
        printf("    strb w0, [%s, #%d]\n", base, offset);
    else if (load_size == 2)
        printf("    strh w0, [%s, #%d]\n", base, offset);
    else if (load_size == 4)
        printf("    str w0, [%s, #%d]\n", base, offset);
    else
        printf("    str x0, [%s, #%d]\n", base, offset);
}

static void arm64_update_global_imm(const char *name, int load_size,
                                    const char *op, long imm) {
    arm64_update_global_member_imm(name, 0, load_size, op, imm);
}

static void arm64_postinc_global_member_to_local(const char *name, int member_offset,
                                                 int load_size, long step,
                                                 int local_offset, int local_size) {
    const char *base = "x1";
    int offset = member_offset;

    arm64_load_global_address_reg("x1", name);
    if (!(arm64_simm9(offset) || arm64_uimm_scaled(offset, load_size))) {
        emit_add_imm64("x1", "x1", offset);
        offset = 0;
    }

    if (load_size == 1)
        printf("    ldrsb x8, [%s, #%d]\n", base, offset);
    else if (load_size == 2)
        printf("    ldrsh x8, [%s, #%d]\n", base, offset);
    else if (load_size == 4)
        printf("    ldrsw x8, [%s, #%d]\n", base, offset);
    else if (load_size == 8)
        printf("    ldr x8, [%s, #%d]\n", base, offset);
    else
        ICE("arm64 global postinc unsupported load_size=%d", load_size);

    emit_add_imm64("x9", "x8", step);
    if (load_size == 1)
        printf("    strb w9, [%s, #%d]\n", base, offset);
    else if (load_size == 2)
        printf("    strh w9, [%s, #%d]\n", base, offset);
    else if (load_size == 4)
        printf("    str w9, [%s, #%d]\n", base, offset);
    else
        printf("    str x9, [%s, #%d]\n", base, offset);

    if (local_size == 8)
        printf("    mov x0, x8\n");
    else
        printf("    mov w0, w8\n");
    arm64_store_local_sized(local_offset, local_size);
}

static void arm64_update_global_from_local(const char *name, int load_size,
                                           int local_offset, int local_size,
                                           int local_is_unsigned,
                                           int is_sub) {
    arm64_update_global_member_from_local(name, 0, load_size,
                                          local_offset, local_size,
                                          local_is_unsigned, is_sub);
}

static void arm64_update_global_member_from_local(const char *name, int member_offset,
                                                  int load_size,
                                                  int local_offset, int local_size,
                                                  int local_is_unsigned,
                                                  int is_sub) {
    const char *base = "x1";
    int offset = member_offset;

    arm64_load_global_address_reg("x1", name);
    if (!(arm64_simm9(offset) || arm64_uimm_scaled(offset, load_size))) {
        emit_add_imm64("x1", "x1", offset);
        offset = 0;
    }

    if (load_size == 1)
        printf("    ldrb w0, [%s, #%d]\n", base, offset);
    else if (load_size == 2)
        printf("    ldrh w0, [%s, #%d]\n", base, offset);
    else if (load_size == 4)
        printf("    ldrsw x0, [%s, #%d]\n", base, offset);
    else if (load_size == 8)
        printf("    ldr x0, [%s, #%d]\n", base, offset);
    else
        ICE("arm64 global member/local update unsupported load_size=%d", load_size);

    arm64_load_local_casted_into_reg("x9", "w9", local_offset, local_size, local_is_unsigned);
    if (load_size <= 4) {
        if (is_sub)
            printf("    sub w0, w0, w9\n");
        else
            printf("    add w0, w0, w9\n");
    } else {
        if (is_sub)
            printf("    sub x0, x0, x9\n");
        else
            printf("    add x0, x0, x9\n");
    }

    if (load_size == 1)
        printf("    strb w0, [%s, #%d]\n", base, offset);
    else if (load_size == 2)
        printf("    strh w0, [%s, #%d]\n", base, offset);
    else if (load_size == 4)
        printf("    str w0, [%s, #%d]\n", base, offset);
    else
        printf("    str x0, [%s, #%d]\n", base, offset);
}

static void arm64_store_global_member_from_local(const char *name, int member_offset,
                                                 int store_size,
                                                 int local_offset, int local_size,
                                                 int local_is_unsigned) {
    arm64_load_local_casted_into_reg("x0", "w0", local_offset, local_size, local_is_unsigned);
    arm64_store_reg_global_member(name, member_offset, store_size, "x0", "w0");
}

static void arm64_store_reg_global_member(const char *name, int member_offset,
                                          int store_size,
                                          const char *xreg, const char *wreg) {
    const char *base = "x11";
    int offset = member_offset;

    arm64_load_global_address_reg("x11", name);
    if (!(arm64_simm9(offset) || arm64_uimm_scaled(offset, store_size))) {
        emit_add_imm64("x11", "x11", offset);
        offset = 0;
    }

    if (store_size == 1)
        printf("    strb %s, [%s, #%d]\n", wreg, base, offset);
    else if (store_size == 2)
        printf("    strh %s, [%s, #%d]\n", wreg, base, offset);
    else if (store_size == 4)
        printf("    str %s, [%s, #%d]\n", wreg, base, offset);
    else if (store_size == 8)
        printf("    str %s, [%s, #%d]\n", xreg, base, offset);
    else
        ICE("arm64 global member register store unsupported store_size=%d", store_size);
}

static void arm64_store_gm_local_imm(const char *name, int member_offset,
                                     int store_size,
                                     int local_offset, int local_size,
                                     int local_is_unsigned,
                                     long imm) {
    arm64_load_local_casted_into_reg("x0", "w0", local_offset, local_size, local_is_unsigned);
    if (imm)
        emit_add_imm64("x0", "x0", imm);
    arm64_store_reg_global_member(name, member_offset, store_size, "x0", "w0");
}

static void arm64_store_gm_local_local_mask(const char *name, int member_offset,
                                            int store_size,
                                            int lhs_offset, int lhs_size,
                                            int lhs_is_unsigned,
                                            int rhs_offset, int rhs_size,
                                            int rhs_is_unsigned,
                                            int mask) {
    arm64_load_local_casted_into_reg("x0", "w0", lhs_offset, lhs_size, lhs_is_unsigned);
    arm64_load_local_casted_into_reg("x1", "w1", rhs_offset, rhs_size, rhs_is_unsigned);
    printf("    add w0, w0, w1\n");
    printf("    and w0, w0, #0x%x\n", mask);
    arm64_store_reg_global_member(name, member_offset, store_size, "x0", "w0");
}

static void arm64_store_local_deref_from_local_ptr_member(int dst_local_offset,
                                                          int src_ptr_local_offset,
                                                          int member_offset,
                                                          int load_size, int cast_size,
                                                          int is_unsigned,
                                                          int store_offset, int store_size) {
    arm64_load_local_ptr_member_casted(src_ptr_local_offset, member_offset,
                                       load_size, cast_size, is_unsigned);
    arm64_load_local_ptr_into_reg("x1", dst_local_offset);
    arm64_store_member_ptr(store_offset, store_size);
}

static void arm64_load_local_ptr_indexed_casted(int ptr_local_offset,
                                                int index_offset, int index_load_size,
                                                int index_is_unsigned, int index_add,
                                                int elem_size, int load_size,
                                                int cast_size, int is_unsigned) {
    arm64_load_local_ptr_into_reg("x11", ptr_local_offset);
    arm64_load_local_casted(index_offset, index_load_size, index_is_unsigned);
    if (index_add > 0)
        printf("    add w0, w0, #%d\n", index_add);
    else if (index_add < 0)
        printf("    sub w0, w0, #%d\n", -index_add);
    arm64_load_ptr_indexed_casted_base("x11", elem_size, load_size, cast_size, is_unsigned);
}

static void arm64_load_local_ptr_indexed_casted_into_arg(int arg_index,
                                                         int ptr_local_offset,
                                                         int index_offset,
                                                         int index_load_size,
                                                         int index_is_unsigned,
                                                         int index_add,
                                                         int elem_size,
                                                         int load_size,
                                                         int cast_size,
                                                         int is_unsigned) {
    char dst_x[4];
    char dst_w[4];

    arm64_fill_reg_name(dst_x, 'x', arg_index);
    arm64_fill_reg_name(dst_w, 'w', arg_index);
    arm64_load_local_ptr_into_reg("x11", ptr_local_offset);
    arm64_load_local_casted_into_reg("x12", "w12", index_offset,
                                     index_load_size, index_is_unsigned);
    if (index_add != 0)
        emit_add_imm64("x12", "x12", index_add);
    arm64_load_ptr_indexed_casted_base_into_reg(dst_x, dst_w, "x11", "x12", "w12",
                                                elem_size, load_size, cast_size,
                                                is_unsigned);
}

static void arm64_load_ptr_indexed_casted_base_into_reg(const char *dst_x,
                                                        const char *dst_w,
                                                        const char *base_reg,
                                                        const char *index_x,
                                                        const char *index_w,
                                                        int elem_size,
                                                        int load_size,
                                                        int cast_size,
                                                        int is_unsigned) {
    int shift = arm64_scale_shift(elem_size);
    int direct_ok = 0;

    if (shift >= 0) {
        if (cast_size == 1)
            direct_ok = (shift == 0);
        else if (cast_size == 2)
            direct_ok = (shift == 0 || shift == 1);
        else if (cast_size == 4)
            direct_ok = (shift == 0 || shift == 2);
        else if (cast_size == 8)
            direct_ok = (shift == 0 || shift == 3);
    }

    if (direct_ok) {
        if (cast_size == 1) {
            if (is_unsigned)
                printf("    ldrb %s, [%s, %s", dst_w, base_reg, index_w);
            else
                printf("    ldrsb %s, [%s, %s", dst_x, base_reg, index_w);
        } else if (cast_size == 2) {
            if (is_unsigned)
                printf("    ldrh %s, [%s, %s", dst_w, base_reg, index_w);
            else
                printf("    ldrsh %s, [%s, %s", dst_x, base_reg, index_w);
        } else if (cast_size == 4) {
            if (is_unsigned)
                printf("    ldr %s, [%s, %s", dst_w, base_reg, index_w);
            else
                printf("    ldrsw %s, [%s, %s", dst_x, base_reg, index_w);
        } else {
            printf("    ldr %s, [%s, %s", dst_x, base_reg, index_w);
        }
        arm64_print_sxtw_shift_suffix(shift);
        printf("]\n");
        return;
    }

    if (STRCMP(index_x, dst_x) != 0)
        printf("    mov %s, %s\n", dst_x, index_x);
    if (load_size != 8)
        printf("    sxtw %s, %s\n", dst_x, dst_w);
    {
        int scale_scratch = arm64_alloc_scratch_reg(arm64_reg_num(dst_x),
                                                    arm64_reg_num(base_reg));
        char scale_x[4];
        arm64_fill_reg_name(scale_x, 'x', scale_scratch);
        emit_load_imm64(scale_x, elem_size);
        printf("    madd %s, %s, %s, %s\n", dst_x, dst_x, scale_x, base_reg);
        arm64_release_scratch_reg(scale_scratch);
    }

    if (cast_size == 1) {
        if (is_unsigned)
            printf("    ldrb %s, [%s]\n", dst_w, dst_x);
        else
            printf("    ldrsb %s, [%s]\n", dst_x, dst_x);
    } else if (cast_size == 2) {
        if (is_unsigned)
            printf("    ldrh %s, [%s]\n", dst_w, dst_x);
        else
            printf("    ldrsh %s, [%s]\n", dst_x, dst_x);
    } else if (cast_size == 4) {
        if (is_unsigned)
            printf("    ldr %s, [%s]\n", dst_w, dst_x);
        else
            printf("    ldrsw %s, [%s]\n", dst_x, dst_x);
    } else {
        printf("    ldr %s, [%s]\n", dst_x, dst_x);
    }
}

static void arm64_update_local_ptr_member_from_local_ptr_member(
    int dst_ptr_local_offset, int dst_member_offset,
    int src_ptr_local_offset, int src_member_offset,
    int load_size, const char *op, int fp_size) {
    arm64_load_local_ptr_into_reg("x10", dst_ptr_local_offset);
    arm64_load_local_ptr_into_reg("x11", src_ptr_local_offset);

    if (fp_size) {
        char fpreg = fp_size == 4 ? 's' : 'd';

        if (fp_size != load_size || (fp_size != 4 && fp_size != 8))
            ICE("arm64 local ptr member fp update unsupported size=%d/%d", load_size, fp_size);

        printf("    ldr %c1, [x11, #%d]\n", fpreg, src_member_offset);
        printf("    ldr %c0, [x10, #%d]\n", fpreg, dst_member_offset);

        if (strcmp(op, "add") == 0)
            printf("    fadd %c0, %c0, %c1\n", fpreg, fpreg, fpreg);
        else if (strcmp(op, "sub") == 0)
            printf("    fsub %c0, %c0, %c1\n", fpreg, fpreg, fpreg);
        else if (strcmp(op, "mul") == 0)
            printf("    fmul %c0, %c0, %c1\n", fpreg, fpreg, fpreg);
        else if (strcmp(op, "div") == 0)
            printf("    fdiv %c0, %c0, %c1\n", fpreg, fpreg, fpreg);
        else
            ICE("arm64 local ptr member fp update unsupported op=%s", op ? op : "<null>");

        printf("    str %c0, [x10, #%d]\n", fpreg, dst_member_offset);
        return;
    }

    if (load_size == 8) {
        printf("    ldr x12, [x11, #%d]\n", src_member_offset);
        printf("    ldr x13, [x10, #%d]\n", dst_member_offset);
        if (strcmp(op, "add") == 0)
            printf("    add x13, x13, x12\n");
        else if (strcmp(op, "sub") == 0)
            printf("    sub x13, x13, x12\n");
        else if (strcmp(op, "and") == 0)
            printf("    and x13, x13, x12\n");
        else if (strcmp(op, "or") == 0)
            printf("    orr x13, x13, x12\n");
        else if (strcmp(op, "xor") == 0)
            printf("    eor x13, x13, x12\n");
        else
            ICE("arm64 local ptr member update unsupported op=%s", op ? op : "<null>");
        printf("    str x13, [x10, #%d]\n", dst_member_offset);
        return;
    }

    if (load_size == 4) {
        printf("    ldrsw x12, [x11, #%d]\n", src_member_offset);
        printf("    ldrsw x13, [x10, #%d]\n", dst_member_offset);
        if (strcmp(op, "add") == 0)
            printf("    add x13, x13, x12\n");
        else if (strcmp(op, "sub") == 0)
            printf("    sub x13, x13, x12\n");
        else if (strcmp(op, "and") == 0)
            printf("    and x13, x13, x12\n");
        else if (strcmp(op, "or") == 0)
            printf("    orr x13, x13, x12\n");
        else if (strcmp(op, "xor") == 0)
            printf("    eor x13, x13, x12\n");
        else
            ICE("arm64 local ptr member update unsupported op=%s", op ? op : "<null>");
        printf("    str w13, [x10, #%d]\n", dst_member_offset);
        return;
    }

    ICE("arm64 local ptr member update unsupported load_size=%d", load_size);
}

static void arm64_load_local_ptr_member_plus_local_into_arg(
    int arg_index, int ptr_local_offset, int member_offset, int member_size,
    int rhs_local_offset, int rhs_size, int rhs_is_unsigned) {
    char dst_x[4];
    char dst_w[4];
    int offset = member_offset;

    arm64_fill_reg_name(dst_x, 'x', arg_index);
    arm64_fill_reg_name(dst_w, 'w', arg_index);
    arm64_load_local_ptr_into_reg("x11", ptr_local_offset);
    if (!(arm64_simm9(offset) || arm64_uimm_scaled(offset, member_size))) {
        emit_add_imm64("x11", "x11", offset);
        offset = 0;
    }

    if (member_size == 8)
        printf("    ldr %s, [x11, #%d]\n", dst_x, offset);
    else if (member_size == 4)
        printf("    ldrsw %s, [x11, #%d]\n", dst_x, offset);
    else if (member_size == 2)
        printf("    ldrsh %s, [x11, #%d]\n", dst_x, offset);
    else if (member_size == 1)
        printf("    ldrsb %s, [x11, #%d]\n", dst_x, offset);
    else
        ICE("arm64 local ptr member plus local unsupported member_size=%d", member_size);

    arm64_load_local_casted_into_reg("x12", "w12", rhs_local_offset,
                                     rhs_size, rhs_is_unsigned);
    if (member_size == 8 || rhs_size == 8)
        printf("    add %s, %s, x12\n", dst_x, dst_x);
    else
        printf("    add %s, %s, w12\n", dst_w, dst_w);
}

static void arm64_accumulate_local_ptr_member_double_call_delta(
    int ptr_local_offset, int member_offset,
    const char *call_name, int start_local_offset) {
    const char *base = "x9";
    int offset = member_offset;

    arm64_call(call_name);
    arm64_load_local_casted_into_reg("x10", "w10", start_local_offset, 8, 0);
    printf("    fmov d1, x10\n");
    arm64_load_local_ptr_into_reg("x9", ptr_local_offset);
    if (!(arm64_simm9(offset) || arm64_uimm_scaled(offset, 8))) {
        emit_add_imm64("x9", "x9", offset);
        offset = 0;
    }
    printf("    ldr d2, [%s, #%d]\n", base, offset);
    printf("    fsub d0, d0, d1\n");
    printf("    fadd d0, d2, d0\n");
    printf("    str d0, [%s, #%d]\n", base, offset);
}

static void arm64_accumulate_local_ptr_member_member_double_call_delta(
    int ptr_local_offset, int ptr_member_offset, int double_member_offset,
    const char *call_name, int start_local_offset) {
    const char *base = "x9";
    int offset = double_member_offset;

    arm64_call(call_name);
    arm64_load_local_casted_into_reg("x10", "w10", start_local_offset, 8, 0);
    printf("    fmov d1, x10\n");
    arm64_load_local_ptr_into_reg("x9", ptr_local_offset);
    if (!(arm64_simm9(ptr_member_offset) || arm64_uimm_scaled(ptr_member_offset, 8))) {
        emit_add_imm64("x9", "x9", ptr_member_offset);
        printf("    ldr x9, [x9]\n");
    } else {
        printf("    ldr x9, [x9, #%d]\n", ptr_member_offset);
    }
    if (!(arm64_simm9(offset) || arm64_uimm_scaled(offset, 8))) {
        emit_add_imm64("x9", "x9", offset);
        offset = 0;
    }
    printf("    ldr d2, [%s, #%d]\n", base, offset);
    printf("    fsub d0, d0, d1\n");
    printf("    fadd d0, d2, d0\n");
    printf("    str d0, [%s, #%d]\n", base, offset);
}

static void arm64_load_local_ptr_offset_indexed_casted_into_arg(int arg_index,
                                                                int ptr_local_offset,
                                                                int base_offset,
                                                                int index_offset,
                                                                int index_load_size,
                                                                int index_is_unsigned,
                                                                int elem_size,
                                                                int member_offset,
                                                                int load_size,
                                                                int cast_size,
                                                                int is_unsigned) {
    char dst_x[4];
    char dst_w[4];
    int total_offset = base_offset + member_offset;

    arm64_fill_reg_name(dst_x, 'x', arg_index);
    arm64_fill_reg_name(dst_w, 'w', arg_index);
    arm64_load_local_ptr_into_reg("x11", ptr_local_offset);
    if (total_offset != 0)
        emit_add_imm64("x11", "x11", total_offset);
    arm64_load_local_casted_into_reg("x12", "w12", index_offset,
                                     index_load_size, index_is_unsigned);
    arm64_load_ptr_indexed_casted_base_into_reg(dst_x, dst_w, "x11", "x12", "w12",
                                                elem_size, load_size, cast_size,
                                                is_unsigned);
}

static void arm64_addr_local_ptr_offset_indexed_into_arg(int arg_index,
                                                         int ptr_local_offset,
                                                         int base_offset,
                                                         int index_offset,
                                                         int index_load_size,
                                                         int index_is_unsigned,
                                                         int elem_size,
                                                         int final_offset) {
    char dst_x[4];
    char index_x[4];
    char index_w[4];
    int shift = arm64_scale_shift(elem_size);

    arm64_fill_reg_name(dst_x, 'x', arg_index);
    arm64_fill_reg_name(index_x, 'x', 12);
    arm64_fill_reg_name(index_w, 'w', 12);

    arm64_load_local_ptr_into_reg(dst_x, ptr_local_offset);
    if (base_offset != 0)
        emit_add_imm64(dst_x, dst_x, base_offset);
    arm64_load_local_casted_into_reg(index_x, index_w, index_offset,
                                     index_load_size, index_is_unsigned);

    if (shift >= 0 && shift <= 4) {
        printf("    add %s, %s, %s", dst_x, dst_x, index_w);
        arm64_print_sxtw_shift_suffix(shift);
        printf("\n");
    } else {
        int scale_scratch = arm64_alloc_scratch_reg(arm64_reg_num(dst_x), 12);
        char scale_x[4];
        arm64_fill_reg_name(scale_x, 'x', scale_scratch);
        if (index_load_size != 8)
            printf("    sxtw %s, %s\n", index_x, index_w);
        emit_load_imm64(scale_x, elem_size);
        printf("    madd %s, %s, %s, %s\n", dst_x, index_x, scale_x, dst_x);
        arm64_release_scratch_reg(scale_scratch);
    }

    if (final_offset != 0)
        emit_add_imm64(dst_x, dst_x, final_offset);
}

static void arm64_addr_local_ptr_indexed_into_reg(const char *dst_x, int dst_reg,
                                                  int ptr_local_offset,
                                                  int index_offset,
                                                  int index_load_size,
                                                  int index_is_unsigned,
                                                  int index_add,
                                                  int elem_size) {
    char index_x[4];
    char index_w[4];
    int shift = arm64_scale_shift(elem_size);

    arm64_fill_reg_name(index_x, 'x', 12);
    arm64_fill_reg_name(index_w, 'w', 12);
    arm64_load_local_ptr_into_reg(dst_x, ptr_local_offset);
    arm64_load_local_casted_into_reg(index_x, index_w, index_offset,
                                     index_load_size, index_is_unsigned);
    if (index_add != 0)
        emit_add_imm64(index_x, index_x, index_add);

    if (shift >= 0 && shift <= 4) {
        if (index_load_size == 8) {
            printf("    add %s, %s, %s", dst_x, dst_x, index_x);
            if (shift > 0)
                printf(", lsl #%d", shift);
            printf("\n");
        } else {
            printf("    add %s, %s, %s, %s", dst_x, dst_x, index_w,
                   index_is_unsigned ? "uxtw" : "sxtw");
            if (shift > 0)
                printf(" #%d", shift);
            printf("\n");
        }
    } else {
        int scale_scratch = arm64_alloc_scratch_reg(dst_reg, 12);
        char scale_x[4];
        arm64_fill_reg_name(scale_x, 'x', scale_scratch);
        if (index_load_size != 8) {
            if (index_is_unsigned)
                printf("    and %s, %s, #0xffffffff\n", index_x, index_x);
            else
                printf("    sxtw %s, %s\n", index_x, index_w);
        }
        emit_load_imm64(scale_x, elem_size);
        printf("    madd %s, %s, %s, %s\n", dst_x, index_x, scale_x, dst_x);
        arm64_release_scratch_reg(scale_scratch);
    }
}

static void arm64_addr_local_ptr_member_indexed_into_arg(int arg_index,
                                                         int ptr_local_offset,
                                                         int member_offset,
                                                         int index_offset,
                                                         int index_load_size,
                                                         int index_is_unsigned,
                                                         int elem_size,
                                                         int final_offset) {
    char dst_x[4];
    char index_x[4];
    char index_w[4];
    int shift = arm64_scale_shift(elem_size);

    arm64_fill_reg_name(dst_x, 'x', arg_index);
    arm64_fill_reg_name(index_x, 'x', 12);
    arm64_fill_reg_name(index_w, 'w', 12);

    arm64_load_local_ptr_into_reg(dst_x, ptr_local_offset);
    if (member_offset != 0) {
        if (arm64_load_imm_offset_ok(TCC_SIZEOF_PTR, member_offset))
            printf("    ldr %s, [%s, #%d]\n", dst_x, dst_x, member_offset);
        else {
            emit_add_imm64(dst_x, dst_x, member_offset);
            printf("    ldr %s, [%s]\n", dst_x, dst_x);
        }
    } else {
        printf("    ldr %s, [%s]\n", dst_x, dst_x);
    }
    arm64_load_local_casted_into_reg(index_x, index_w, index_offset,
                                     index_load_size, index_is_unsigned);

    if (shift >= 0 && shift <= 4) {
        printf("    add %s, %s, %s", dst_x, dst_x, index_w);
        arm64_print_sxtw_shift_suffix(shift);
        printf("\n");
    } else {
        int scale_scratch = arm64_alloc_scratch_reg(arm64_reg_num(dst_x), 12);
        char scale_x[4];
        arm64_fill_reg_name(scale_x, 'x', scale_scratch);
        if (index_load_size != 8)
            printf("    sxtw %s, %s\n", index_x, index_w);
        emit_load_imm64(scale_x, elem_size);
        printf("    madd %s, %s, %s, %s\n", dst_x, index_x, scale_x, dst_x);
        arm64_release_scratch_reg(scale_scratch);
    }

    if (final_offset != 0)
        emit_add_imm64(dst_x, dst_x, final_offset);
}

static void arm64_load_local_ptr_member_member_indexed_to_arg(
    int arg_index, int ptr_local_offset, int first_member_offset,
    int second_member_offset, int index_offset, int index_load_size,
    int index_is_unsigned, int elem_size, int load_size, int is_unsigned)
{
    char dst_x[4];
    char dst_w[4];
    int shift = arm64_scale_shift(elem_size);

    arm64_fill_reg_name(dst_x, 'x', arg_index);
    arm64_fill_reg_name(dst_w, 'w', arg_index);

    arm64_load_local_ptr_into_reg(dst_x, ptr_local_offset);
    if (arm64_load_imm_offset_ok(TCC_SIZEOF_PTR, first_member_offset))
        printf("    ldr %s, [%s, #%d]\n", dst_x, dst_x, first_member_offset);
    else {
        emit_add_imm64(dst_x, dst_x, first_member_offset);
        printf("    ldr %s, [%s]\n", dst_x, dst_x);
    }
    if (arm64_load_imm_offset_ok(TCC_SIZEOF_PTR, second_member_offset))
        printf("    ldr %s, [%s, #%d]\n", dst_x, dst_x, second_member_offset);
    else {
        emit_add_imm64(dst_x, dst_x, second_member_offset);
        printf("    ldr %s, [%s]\n", dst_x, dst_x);
    }

    arm64_load_local_casted_into_reg("x12", "w12", index_offset,
                                     index_load_size, index_is_unsigned);
    if (shift >= 0 && shift <= 4) {
        printf("    add %s, %s, w12", dst_x, dst_x);
        arm64_print_sxtw_shift_suffix(shift);
        printf("\n");
    } else {
        if (index_load_size != 8)
            printf("    sxtw x12, w12\n");
        emit_load_imm64("x10", elem_size);
        printf("    madd %s, x12, x10, %s\n", dst_x, dst_x);
    }

    if (load_size == 8)
        printf("    ldr %s, [%s]\n", dst_x, dst_x);
    else if (load_size == 4) {
        if (is_unsigned)
            printf("    ldr %s, [%s]\n", dst_w, dst_x);
        else
            printf("    ldrsw %s, [%s]\n", dst_x, dst_x);
    } else if (load_size == 2) {
        if (is_unsigned)
            printf("    ldrh %s, [%s]\n", dst_w, dst_x);
        else
            printf("    ldrsh %s, [%s]\n", dst_x, dst_x);
    } else if (load_size == 1) {
        if (is_unsigned)
            printf("    ldrb %s, [%s]\n", dst_w, dst_x);
        else
            printf("    ldrsb %s, [%s]\n", dst_x, dst_x);
    } else {
        ICE("arm64 local ptr member member indexed load unsupported load_size=%d", load_size);
    }
}

static void arm64_addr_lpm_postinc_midx(int ptr_local_offset,
                                                               int base_member_offset,
                                                               int index_member_offset,
                                                               int index_load_size,
                                                               int index_is_unsigned,
                                                               int elem_size) {
    int shift = arm64_scale_shift(elem_size);

    if (index_load_size != 4 && index_load_size != 8)
        ICE("arm64 postinc member indexed address unsupported index size=%d", index_load_size);

    arm64_load_local_ptr_into_reg("x9", ptr_local_offset);
    if (base_member_offset) {
        if (arm64_load_imm_offset_ok(TCC_SIZEOF_PTR, base_member_offset))
            printf("    ldr x0, [x9, #%d]\n", base_member_offset);
        else {
            emit_add_imm64("x10", "x9", base_member_offset);
            printf("    ldr x0, [x10]\n");
        }
    } else {
        printf("    ldr x0, [x9]\n");
    }

    if (index_load_size == 8)
        printf("    ldr x10, [x9, #%d]\n", index_member_offset);
    else if (index_is_unsigned)
        printf("    ldr w10, [x9, #%d]\n", index_member_offset);
    else
        printf("    ldrsw x10, [x9, #%d]\n", index_member_offset);

    if (index_load_size == 8) {
        printf("    add x11, x10, #1\n");
        printf("    str x11, [x9, #%d]\n", index_member_offset);
    } else {
        printf("    add w11, w10, #1\n");
        printf("    str w11, [x9, #%d]\n", index_member_offset);
    }

    if (shift >= 0 && shift <= 4 && index_load_size != 8) {
        printf("    add x0, x0, w10, %s", index_is_unsigned ? "uxtw" : "sxtw");
        if (shift > 0)
            printf(" #%d", shift);
        printf("\n");
    } else {
        emit_load_imm64("x11", elem_size);
        printf("    madd x0, x10, x11, x0\n");
    }
}

static void arm64_local_ptr_add_sub_locals_to_arg(int arg_index,
                                                  int ptr_local_offset,
                                                  int add_local_offset,
                                                  int add_local_size,
                                                  int add_is_unsigned,
                                                  int sub_local_offset,
                                                  int sub_local_size,
                                                  int sub_is_unsigned) {
    char dst_x[4];

    if (arg_index < 0 || arg_index > 7)
        ICE("arm64 arg register out of range");

    arm64_fill_reg_name(dst_x, 'x', arg_index);
    arm64_load_local_ptr_into_reg(dst_x, ptr_local_offset);
    arm64_load_local_casted_into_reg("x9", "w9", add_local_offset,
                                     add_local_size, add_is_unsigned);
    printf("    add %s, %s, x9\n", dst_x, dst_x);
    arm64_load_local_casted_into_reg("x9", "w9", sub_local_offset,
                                     sub_local_size, sub_is_unsigned);
    printf("    sub %s, %s, x9\n", dst_x, dst_x);
}

static void arm64_store_lpm_postinc_lidx_imm(int ptr_local_offset,
                                                                 int member_offset,
                                                                 int index_local_offset,
                                                                 int index_load_size,
                                                                 int index_is_unsigned,
                                                                 int elem_size,
                                                                 int store_size,
                                                                 long imm) {
    int shift = arm64_scale_shift(elem_size);

    if (index_load_size != 4 && index_load_size != 8)
        ICE("arm64 postinc local indexed store unsupported index size=%d", index_load_size);

    arm64_load_local_ptr_into_reg("x9", ptr_local_offset);
    if (member_offset) {
        if (arm64_load_imm_offset_ok(TCC_SIZEOF_PTR, member_offset))
            printf("    ldr x9, [x9, #%d]\n", member_offset);
        else {
            emit_add_imm64("x9", "x9", member_offset);
            printf("    ldr x9, [x9]\n");
        }
    } else {
        printf("    ldr x9, [x9]\n");
    }

    arm64_load_local_casted_into_reg("x10", "w10", index_local_offset,
                                     index_load_size, index_is_unsigned);
    if (index_load_size == 8) {
        printf("    add x11, x10, #1\n");
        printf("    mov x0, x11\n");
        arm64_store_local_sized(index_local_offset, 8);
    } else {
        printf("    add w11, w10, #1\n");
        printf("    mov w0, w11\n");
        arm64_store_local_sized(index_local_offset, 4);
    }

    if (shift >= 0 && shift <= 4 && index_load_size != 8) {
        printf("    add x9, x9, w10, %s", index_is_unsigned ? "uxtw" : "sxtw");
        if (shift > 0)
            printf(" #%d", shift);
        printf("\n");
    } else {
        emit_load_imm64("x12", elem_size);
        printf("    madd x9, x10, x12, x9\n");
    }

    emit_load_imm64("x10", imm);
    if (store_size == 1)
        printf("    strb w10, [x9]\n");
    else if (store_size == 2)
        printf("    strh w10, [x9]\n");
    else if (store_size == 8)
        printf("    str x10, [x9]\n");
    else
        printf("    str w10, [x9]\n");
}

static void arm64_addr_lpm_postinc_lidx(int ptr_local_offset,
                                                             int member_offset,
                                                             int index_ptr_local_offset,
                                                             int index_load_size,
                                                             int index_is_unsigned,
                                                             int elem_size) {
    int shift = arm64_scale_shift(elem_size);

    if (index_load_size != 4 && index_load_size != 8)
        ICE("arm64 postinc local indexed address unsupported index size=%d", index_load_size);

    arm64_load_local_ptr_into_reg("x9", ptr_local_offset);
    if (member_offset) {
        if (arm64_load_imm_offset_ok(TCC_SIZEOF_PTR, member_offset))
            printf("    ldr x9, [x9, #%d]\n", member_offset);
        else {
            emit_add_imm64("x9", "x9", member_offset);
            printf("    ldr x9, [x9]\n");
        }
    } else {
        printf("    ldr x9, [x9]\n");
    }

    arm64_load_local_ptr_into_reg("x12", index_ptr_local_offset);
    if (index_load_size == 8) {
        printf("    ldr x10, [x12]\n");
        printf("    add x11, x10, #1\n");
        printf("    str x11, [x12]\n");
    } else {
        printf("    ldr w10, [x12]\n");
        printf("    add w11, w10, #1\n");
        printf("    str w11, [x12]\n");
    }

    if (shift >= 0 && shift <= 4 && index_load_size != 8) {
        printf("    add x0, x9, w10, %s", index_is_unsigned ? "uxtw" : "sxtw");
        if (shift > 0)
            printf(" #%d", shift);
        printf("\n");
    } else {
        if (index_load_size == 4)
            printf("    %s x10, w10\n", index_is_unsigned ? "uxtw" : "sxtw");
        emit_load_imm64("x11", elem_size);
        printf("    madd x0, x10, x11, x9\n");
    }
}

static void arm64_store_local_ptr_indexed_from_acc(int ptr_local_offset,
                                                   int index_offset, int index_load_size,
                                                   int index_is_unsigned, int index_divisor,
                                                   int index_add, int elem_size,
                                                   int store_size) {
    int value_scratch = arm64_alloc_scratch_reg(0, 1);
    char value_x[4];
    char value_w[4];

    arm64_fill_reg_name(value_x, 'x', value_scratch);
    arm64_fill_reg_name(value_w, 'w', value_scratch);
    arm64_load_local_ptr_into_reg("x1", ptr_local_offset);
    printf("    mov %s, x0\n", value_x);
    arm64_load_local_casted(index_offset, index_load_size, index_is_unsigned);
    if (index_divisor > 1) {
        int div_scratch = arm64_alloc_scratch_reg(0, 1);
        char div_x[4];
        arm64_fill_reg_name(div_x, 'x', div_scratch);
        printf("    movz %s, #%d\n", div_x, index_divisor);
        if (index_is_unsigned)
            printf("    udiv x0, x0, %s\n", div_x);
        else
            printf("    sdiv x0, x0, %s\n", div_x);
        arm64_release_scratch_reg(div_scratch);
    }
    if (index_add > 0)
        printf("    add w0, w0, #%d\n", index_add);
    else if (index_add < 0)
        printf("    sub w0, w0, #%d\n", -index_add);
    if (elem_size > 1) {
        int scale_scratch = arm64_alloc_scratch_reg(0, 1);
        char scale_x[4];
        arm64_fill_reg_name(scale_x, 'x', scale_scratch);
        printf("    movz %s, #%d\n", scale_x, elem_size);
        printf("    mul x0, x0, %s\n", scale_x);
        arm64_release_scratch_reg(scale_scratch);
    }
    if (store_size == 1)
        printf("    strb %s, [x1, w0, sxtw]\n", value_w);
    else if (store_size == 2)
        printf("    strh %s, [x1, w0, sxtw]\n", value_w);
    else if (store_size == 8)
        printf("    str %s, [x1, w0, sxtw]\n", value_x);
    else
        printf("    str %s, [x1, w0, sxtw]\n", value_w);
    arm64_release_scratch_reg(value_scratch);
}

static void arm64_store_sized_reg_at_addr(const char *addr_x, const char *value_x,
                                          const char *value_w, int store_size) {
    if (store_size == 1)
        printf("    strb %s, [%s]\n", value_w, addr_x);
    else if (store_size == 2)
        printf("    strh %s, [%s]\n", value_w, addr_x);
    else if (store_size == 8)
        printf("    str %s, [%s]\n", value_x, addr_x);
    else
        printf("    str %s, [%s]\n", value_w, addr_x);
}

static void arm64_store_local_ptr_indexed_member_imm(int ptr_local_offset,
                                                     int index_offset,
                                                     int index_load_size,
                                                     int index_is_unsigned,
                                                     int index_add,
                                                     int elem_size,
                                                     int member_offset,
                                                     int store_size,
                                                     long value) {
    (void)index_add;
    arm64_addr_local_ptr_offset_indexed_into_arg(11, ptr_local_offset, 0,
                                                 index_offset, index_load_size,
                                                 index_is_unsigned, elem_size,
                                                 member_offset);
    emit_load_imm64("x10", value);
    arm64_store_sized_reg_at_addr("x11", "x10", "w10", store_size);
}

static void arm64_store_local_ptr_indexed_member_from_local(int ptr_local_offset,
                                                            int index_offset,
                                                            int index_load_size,
                                                            int index_is_unsigned,
                                                            int index_add,
                                                            int elem_size,
                                                            int member_offset,
                                                            int store_size,
                                                            int src_local_offset,
                                                            int src_load_size,
                                                            int src_is_unsigned) {
    (void)index_add;
    arm64_addr_local_ptr_offset_indexed_into_arg(11, ptr_local_offset, 0,
                                                 index_offset, index_load_size,
                                                 index_is_unsigned, elem_size,
                                                 member_offset);
    arm64_load_local_casted_into_reg("x10", "w10", src_local_offset,
                                     src_load_size, src_is_unsigned);
    arm64_store_sized_reg_at_addr("x11", "x10", "w10", store_size);
}

static void arm64_update_local_ptr_offset_indexed_from_local_ptr_offset(
    int dst_ptr_local_offset, int dst_base_offset,
    int src_ptr_local_offset, int src_base_offset,
    int index_offset, int index_load_size, int index_is_unsigned,
    int elem_size, int load_size, const char *op, int fp_size) {
    int shift = arm64_scale_shift(elem_size);

    if (shift < 0)
        ICE("arm64 local ptr indexed update unsupported scale=%d", elem_size);

    arm64_load_local_casted_into_reg("x9", "w9", index_offset, index_load_size, index_is_unsigned);
    arm64_load_local_ptr_into_reg("x10", dst_ptr_local_offset);
    if (dst_base_offset != 0)
        emit_add_imm64("x10", "x10", dst_base_offset);
    arm64_load_local_ptr_into_reg("x11", src_ptr_local_offset);
    if (src_base_offset != 0)
        emit_add_imm64("x11", "x11", src_base_offset);

    if (fp_size) {
        char fpreg = fp_size == 4 ? 's' : 'd';

        if (fp_size != load_size || (fp_size != 4 && fp_size != 8))
            ICE("arm64 local ptr indexed fp update unsupported size=%d/%d", load_size, fp_size);

        printf("    ldr %c1, [x11, x9", fpreg);
        if (shift > 0)
            printf(", lsl #%d", shift);
        printf("]\n");

        printf("    ldr %c0, [x10, x9", fpreg);
        if (shift > 0)
            printf(", lsl #%d", shift);
        printf("]\n");

        if (strcmp(op, "add") == 0)
            printf("    fadd %c0, %c0, %c1\n", fpreg, fpreg, fpreg);
        else if (strcmp(op, "sub") == 0)
            printf("    fsub %c0, %c0, %c1\n", fpreg, fpreg, fpreg);
        else if (strcmp(op, "mul") == 0)
            printf("    fmul %c0, %c0, %c1\n", fpreg, fpreg, fpreg);
        else if (strcmp(op, "div") == 0)
            printf("    fdiv %c0, %c0, %c1\n", fpreg, fpreg, fpreg);
        else
            ICE("arm64 local ptr indexed fp update unsupported op=%s", op ? op : "<null>");

        printf("    str %c0, [x10, x9", fpreg);
        if (shift > 0)
            printf(", lsl #%d", shift);
        printf("]\n");
        return;
    }

    if (load_size == 8) {
        printf("    ldr x12, [x11, x9");
        if (shift > 0)
            printf(", lsl #%d", shift);
        printf("]\n");

        printf("    ldr x13, [x10, x9");
        if (shift > 0)
            printf(", lsl #%d", shift);
        printf("]\n");

        if (strcmp(op, "add") == 0)
            printf("    add x13, x13, x12\n");
        else if (strcmp(op, "sub") == 0)
            printf("    sub x13, x13, x12\n");
        else if (strcmp(op, "and") == 0)
            printf("    and x13, x13, x12\n");
        else if (strcmp(op, "or") == 0)
            printf("    orr x13, x13, x12\n");
        else if (strcmp(op, "xor") == 0)
            printf("    eor x13, x13, x12\n");
        else
            ICE("arm64 local ptr indexed update unsupported op=%s", op ? op : "<null>");

        printf("    str x13, [x10, x9");
        if (shift > 0)
            printf(", lsl #%d", shift);
        printf("]\n");
        return;
    }

    if (load_size == 4) {
        printf("    ldrsw x12, [x11, x9");
        if (shift > 0)
            printf(", lsl #%d", shift);
        printf("]\n");

        printf("    ldrsw x13, [x10, x9");
        if (shift > 0)
            printf(", lsl #%d", shift);
        printf("]\n");

        if (strcmp(op, "add") == 0)
            printf("    add x13, x13, x12\n");
        else if (strcmp(op, "sub") == 0)
            printf("    sub x13, x13, x12\n");
        else if (strcmp(op, "and") == 0)
            printf("    and x13, x13, x12\n");
        else if (strcmp(op, "or") == 0)
            printf("    orr x13, x13, x12\n");
        else if (strcmp(op, "xor") == 0)
            printf("    eor x13, x13, x12\n");
        else
            ICE("arm64 local ptr indexed update unsupported op=%s", op ? op : "<null>");

        printf("    str w13, [x10, x9");
        if (shift > 0)
            printf(", lsl #%d", shift);
        printf("]\n");
        return;
    }

    ICE("arm64 local ptr indexed update unsupported load_size=%d", load_size);
}

static void arm64_store_local_sized(int offset, int size) {
    int frame_offset = arm64_frame_offset(offset);
    const char *base = arm64_frame_base_reg();
    long base_delta;
    int mem_offset;

    if (arm64_simm9(frame_offset) || arm64_uimm_scaled(frame_offset, size)) {
        if (size == 1)
            printf("    strb w0, [%s, #%d]\n", base, frame_offset);
        else if (size == 2)
            printf("    strh w0, [%s, #%d]\n", base, frame_offset);
        else if (size == 8)
            printf("    str x0, [%s, #%d]\n", base, frame_offset);
        else
            printf("    str w0, [%s, #%d]\n", base, frame_offset);
        return;
    }

    if (arm64_split_large_frame_mem_offset(frame_offset, size,
                                           &base_delta, &mem_offset)) {
        emit_add_imm64("x9", base, base_delta);
        if (size == 1)
            printf("    strb w0, [x9, #%d]\n", mem_offset);
        else if (size == 2)
            printf("    strh w0, [x9, #%d]\n", mem_offset);
        else if (size == 8)
            printf("    str x0, [x9, #%d]\n", mem_offset);
        else
            printf("    str w0, [x9, #%d]\n", mem_offset);
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

static int arm64_load_imm_offset_ok(int size, int offset) {
    if (offset < 0 || offset > 4095)
        return 0;
    if (size == 1)
        return 1;
    if (size == 2)
        return (offset & 1) == 0;
    if (size == 8)
        return (offset & 7) == 0;
    return (offset & 3) == 0;
}

static int
arm64_symbol_is_thread_local(const char *name)
{
    return name && parser_global_is_thread_local(name);
}

static void
arm64_load_thread_local_address_reg(const char *reg, const char *name)
{
    if (STRCMP(reg, "x0") == 0) {
        printf("    adrp x0, _%s@TLVPPAGE\n", name);
        printf("    ldr x0, [x0, _%s@TLVPPAGEOFF]\n", name);
    } else {
        printf("    adrp %s, _%s@TLVPPAGE\n", reg, name);
        printf("    ldr %s, [%s, _%s@TLVPPAGEOFF]\n", reg, reg, name);
        printf("    mov x0, %s\n", reg);
    }
    printf("    ldr x8, [x0]\n");
    if (arm64_simple_leaf || arm64_leaf_sp_frame_size > 0)
        printf("    str x30, [sp, #-16]!\n");
    printf("    blr x8\n");
    if (arm64_simple_leaf || arm64_leaf_sp_frame_size > 0)
        printf("    ldr x30, [sp], #16\n");
    if (STRCMP(reg, "x0") != 0)
        printf("    mov %s, x0\n", reg);
}

static void arm64_load_global_address_reg(const char *reg, const char *name) {
    if (arm64_symbol_is_thread_local(name)) {
        arm64_load_thread_local_address_reg(reg, name);
        return;
    }
    if (arm64_symbol_needs_got(name)) {
        printf("    adrp %s, _%s@GOTPAGE\n", reg, name);
        printf("    ldr %s, [%s, _%s@GOTPAGEOFF]\n", reg, reg, name);
    } else {
        printf("    adrp %s, _%s@PAGE\n", reg, name);
        printf("    add %s, %s, _%s@PAGEOFF\n", reg, reg, name);
    }
}

static void arm64_load_global(const char *name, int size) {
    if (arm64_symbol_is_thread_local(name)) {
        arm64_load_global_address_reg("x0", name);
        if (size == 1)
            printf("    ldrb w0, [x0]\n");
        else if (size == 2)
            printf("    ldrh w0, [x0]\n");
        else if (size == 8)
            printf("    ldr x0, [x0]\n");
        else
            printf("    ldrsw x0, [x0]\n");
        return;
    }
    if (arm64_symbol_needs_got(name)) {
        arm64_load_global_address_reg("x1", name);
        if (size == 1)
            printf("    ldrb w0, [x1]\n");
        else if (size == 2)
            printf("    ldrh w0, [x1]\n");
        else if (size == 8)
            printf("    ldr x0, [x1]\n");
        else
            printf("    ldrsw x0, [x1]\n");
        return;
    }

    printf("    adrp x1, _%s@PAGE\n", name);
    if (size == 1)
        printf("    ldrb w0, [x1, _%s@PAGEOFF]\n", name);
    else if (size == 2)
        printf("    ldrh w0, [x1, _%s@PAGEOFF]\n", name);
    else if (size == 8)
        printf("    ldr x0, [x1, _%s@PAGEOFF]\n", name);
    else
        printf("    ldrsw x0, [x1, _%s@PAGEOFF]\n", name);
}

static void arm64_load_global_member(const char *name, int offset, int size, int is_extern) {
    (void)is_extern;

    if (!arm64_symbol_is_thread_local(name) &&
        !arm64_symbol_needs_got(name) && offset == 0) {
        printf("    adrp x1, _%s@PAGE\n", name);
        if (size == 1)
            printf("    ldrb w0, [x1, _%s@PAGEOFF]\n", name);
        else if (size == 2)
            printf("    ldrh w0, [x1, _%s@PAGEOFF]\n", name);
        else if (size == 8)
            printf("    ldr x0, [x1, _%s@PAGEOFF]\n", name);
        else
            printf("    ldrsw x0, [x1, _%s@PAGEOFF]\n", name);
        return;
    }

    arm64_load_global_address_reg("x1", name);
    if (offset) {
        if (arm64_load_imm_offset_ok(size, offset)) {
            if (size == 1)
                printf("    ldrb w0, [x1, #%d]\n", offset);
            else if (size == 2)
                printf("    ldrh w0, [x1, #%d]\n", offset);
            else if (size == 8)
                printf("    ldr x0, [x1, #%d]\n", offset);
            else
                printf("    ldrsw x0, [x1, #%d]\n", offset);
            return;
        }
        emit_load_imm64("x9", offset);
        printf("    add x1, x1, x9\n");
    }
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
    if (arm64_symbol_is_thread_local(name)) {
        printf("    str x0, [sp, #-16]!\n");
        arm64_load_global_address_reg("x1", name);
        printf("    ldr x9, [sp], #16\n");
        if (size == 1)
            printf("    strb w9, [x1]\n");
        else if (size == 2)
            printf("    strh w9, [x1]\n");
        else if (size == 8)
            printf("    str x9, [x1]\n");
        else
            printf("    str w9, [x1]\n");
        return;
    }
    if (arm64_symbol_needs_got(name)) {
        arm64_load_global_address_reg("x1", name);
        if (size == 1)
            printf("    strb w0, [x1]\n");
        else if (size == 2)
            printf("    strh w0, [x1]\n");
        else if (size == 8)
            printf("    str x0, [x1]\n");
        else
            printf("    str w0, [x1]\n");
        return;
    }

    printf("    adrp x1, _%s@PAGE\n", name);
    if (size == 1)
        printf("    strb w0, [x1, _%s@PAGEOFF]\n", name);
    else if (size == 2)
        printf("    strh w0, [x1, _%s@PAGEOFF]\n", name);
    else if (size == 8)
        printf("    str x0, [x1, _%s@PAGEOFF]\n", name);
    else
        printf("    str w0, [x1, _%s@PAGEOFF]\n", name);
}

static void arm64_emit_store_global_zero(const char *name, int size) {
    if (arm64_symbol_is_thread_local(name)) {
        arm64_load_global_address_reg("x0", name);
        if (size == 1)
            printf("    strb wzr, [x0]\n");
        else if (size == 2)
            printf("    strh wzr, [x0]\n");
        else if (size == 8)
            printf("    str xzr, [x0]\n");
        else
            printf("    str wzr, [x0]\n");
        return;
    }
    if (arm64_symbol_needs_got(name)) {
        arm64_load_global_address_reg("x1", name);
        if (size == 1)
            printf("    strb wzr, [x1]\n");
        else if (size == 2)
            printf("    strh wzr, [x1]\n");
        else if (size == 8)
            printf("    str xzr, [x1]\n");
        else
            printf("    str wzr, [x1]\n");
        return;
    }

    printf("    adrp x1, _%s@PAGE\n", name);
    if (size == 1)
        printf("    strb wzr, [x1, _%s@PAGEOFF]\n", name);
    else if (size == 2)
        printf("    strh wzr, [x1, _%s@PAGEOFF]\n", name);
    else if (size == 8)
        printf("    str xzr, [x1, _%s@PAGEOFF]\n", name);
    else
        printf("    str wzr, [x1, _%s@PAGEOFF]\n", name);
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
    arm64_load_global_address_reg("x2", name);
    if (elem_size == 1 || elem_size == 2 || elem_size == 4 || elem_size == 8) {
        if (elem_size == 1)
            printf("    ldrb w0, [x2, w0, sxtw]\n");
        else if (elem_size == 2)
            printf("    ldrh w0, [x2, w0, sxtw #1]\n");
        else if (elem_size == 8)
            printf("    ldr x0, [x2, w0, sxtw #3]\n");
        else
            printf("    ldrsw x0, [x2, w0, sxtw #2]\n");
        return;
    }
    printf("    sxtw x0, w0\n");
    emit_load_imm64("x3", elem_size);
    printf("    madd x0, x0, x3, x2\n");
    if (elem_size == 1)
        printf("    ldrb w0, [x0]\n");
    else if (elem_size == 2)
        printf("    ldrh w0, [x0]\n");
    else if (elem_size == 8)
        printf("    ldr x0, [x0]\n");
    else
        printf("    ldrsw x0, [x0]\n");
}

static void arm64_load_global_member_indexed(const char *name, int base_offset,
                                             int elem_size, int member_offset,
                                             int load_size, int cast_size, int is_unsigned,
                                             int index_offset, int index_load_size,
                                             int index_is_unsigned, int is_extern,
                                             int base_is_pointer) {
    int total_offset = base_offset + member_offset;

    arm64_load_local_casted(index_offset, index_load_size, index_is_unsigned);
    if (base_is_pointer) {
        arm64_load_global_member_into_reg("x11", "w11", name, total_offset,
                                          TCC_SIZEOF_PTR, is_extern, 0);
    } else {
        arm64_load_global_address_reg("x11", name);
        if (total_offset != 0)
            emit_add_imm64("x11", "x11", total_offset);
    }
    arm64_load_ptr_indexed_casted_base("x11", elem_size, load_size, cast_size, is_unsigned);
}

static void arm64_load_global_member_indexed_into_arg(int arg_index,
                                                      const char *name,
                                                      int base_offset,
                                                      int elem_size,
                                                      int member_offset,
                                                      int load_size,
                                                      int cast_size,
                                                      int is_unsigned,
                                                      int index_offset,
                                                      int index_load_size,
                                                      int index_is_unsigned,
                                                      int is_extern,
                                                      int base_is_pointer) {
    char dst_x[4];
    char dst_w[4];
    int total_offset = base_offset + member_offset;

    arm64_fill_reg_name(dst_x, 'x', arg_index);
    arm64_fill_reg_name(dst_w, 'w', arg_index);

    arm64_load_local_casted_into_reg("x12", "w12", index_offset,
                                     index_load_size, index_is_unsigned);
    if (base_is_pointer) {
        arm64_load_global_member_into_reg("x11", "w11", name, total_offset,
                                          TCC_SIZEOF_PTR, is_extern, 0);
    } else {
        arm64_load_global_address_reg("x11", name);
        if (total_offset != 0)
            emit_add_imm64("x11", "x11", total_offset);
    }
    arm64_load_ptr_indexed_casted_base_into_reg(dst_x, dst_w, "x11", "x12", "w12",
                                                elem_size, load_size, cast_size,
                                                is_unsigned);
}

static void arm64_addr_global_member_indexed_into_arg(int arg_index,
                                                      const char *name,
                                                      int base_offset,
                                                      int elem_size,
                                                      int index_offset,
                                                      int index_load_size,
                                                      int index_is_unsigned,
                                                      int final_offset,
                                                      int is_extern,
                                                      int base_is_pointer) {
    char dst_x[4];
    char dst_w[4];
    char index_x[4];
    char index_w[4];
    int shift = arm64_scale_shift(elem_size);

    arm64_fill_reg_name(dst_x, 'x', arg_index);
    arm64_fill_reg_name(dst_w, 'w', arg_index);
    arm64_fill_reg_name(index_x, 'x', 12);
    arm64_fill_reg_name(index_w, 'w', 12);

    if (base_is_pointer) {
        arm64_load_global_member_into_reg(dst_x, dst_w,
                                          name, base_offset, TCC_SIZEOF_PTR,
                                          is_extern, 0);
    } else {
        arm64_load_global_address_reg(dst_x, name);
        if (base_offset != 0)
            emit_add_imm64(dst_x, dst_x, base_offset);
    }
    arm64_load_local_casted_into_reg(index_x, index_w, index_offset,
                                     index_load_size, index_is_unsigned);

    if (shift >= 0 && shift <= 4) {
        printf("    add %s, %s, %s", dst_x, dst_x, index_w);
        arm64_print_sxtw_shift_suffix(shift);
        printf("\n");
    } else {
        int scale_scratch = arm64_alloc_scratch_reg(arg_index, 12);
        char scale_x[4];

        arm64_fill_reg_name(scale_x, 'x', scale_scratch);
        if (index_load_size != 8)
            printf("    sxtw %s, %s\n", index_x, index_w);
        emit_load_imm64(scale_x, elem_size);
        printf("    madd %s, %s, %s, %s\n", dst_x, index_x, scale_x, dst_x);
        arm64_release_scratch_reg(scale_scratch);
    }

    if (final_offset != 0)
        emit_add_imm64(dst_x, dst_x, final_offset);
}

static void arm64_store_global_indexed(const char *name, int elem_size) {
    arm64_load_global_address_reg("x2", name);
    if (elem_size == 1 || elem_size == 2 || elem_size == 4 || elem_size == 8) {
        if (elem_size == 1)
            printf("    strb w0, [x2, w1, sxtw]\n");
        else if (elem_size == 2)
            printf("    strh w0, [x2, w1, sxtw #1]\n");
        else if (elem_size == 8)
            printf("    str x0, [x2, w1, sxtw #3]\n");
        else
            printf("    str w0, [x2, w1, sxtw #2]\n");
        return;
    }
    printf("    sxtw x1, w1\n");
    emit_load_imm64("x3", elem_size);
    printf("    madd x1, x1, x3, x2\n");
    if (elem_size == 1)
        printf("    strb w0, [x1]\n");
    else if (elem_size == 2)
        printf("    strh w0, [x1]\n");
    else if (elem_size == 8)
        printf("    str x0, [x1]\n");
    else
        printf("    str w0, [x1]\n");
}

static void arm64_store_global_indexed_imm_from_local(const char *name,
                                                      int elem_size,
                                                      int index_offset,
                                                      int index_load_size,
                                                      int index_is_unsigned,
                                                      long value) {
    int shift = arm64_scale_shift(elem_size);

    arm64_load_local_casted_into_reg("x9", "w9", index_offset,
                                     index_load_size, index_is_unsigned);
    arm64_load_global_address_reg("x11", name);
    emit_load_imm64("x10", value);

    if (shift >= 0 && shift <= 4) {
        if (elem_size == 8)
            printf("    str x10, [x11, x9%s]\n", shift ? ", lsl #3" : "");
        else if (elem_size == 4)
            printf("    str w10, [x11, x9%s]\n", shift ? ", lsl #2" : "");
        else if (elem_size == 2)
            printf("    strh w10, [x11, x9%s]\n", shift ? ", lsl #1" : "");
        else if (elem_size == 1)
            printf("    strb w10, [x11, x9]\n");
        else
            ICE("arm64 indexed global immediate store unsupported elem_size=%d", elem_size);
        return;
    }

    emit_load_imm64("x12", elem_size);
    printf("    madd x11, x9, x12, x11\n");
    if (elem_size == 8)
        printf("    str x10, [x11]\n");
    else if (elem_size == 4)
        printf("    str w10, [x11]\n");
    else if (elem_size == 2)
        printf("    strh w10, [x11]\n");
    else if (elem_size == 1)
        printf("    strb w10, [x11]\n");
    else
        ICE("arm64 indexed global immediate store unsupported elem_size=%d", elem_size);
}

static void arm64_update_global_indexed_from_local(const char *name, int elem_size,
                                                   int index_offset, int index_load_size, int index_is_unsigned,
                                                   int rhs_offset, int rhs_load_size, int rhs_is_unsigned,
                                                   int is_sub) {
    arm64_load_local_casted_into_reg("x9", "w9", index_offset, index_load_size, index_is_unsigned);
    arm64_load_local_casted_into_reg("x10", "w10", rhs_offset, rhs_load_size, rhs_is_unsigned);

    printf("    adrp x11, _%s@PAGE\n", name);
    printf("    add  x11, x11, _%s@PAGEOFF\n", name);

    if (elem_size == 8) {
        printf("    ldr x12, [x11, x9, lsl #3]\n");
        printf("    %s x12, x12, x10\n", is_sub ? "sub" : "add");
        printf("    str x12, [x11, x9, lsl #3]\n");
    } else if (elem_size == 4) {
        printf("    ldrsw x12, [x11, x9, lsl #2]\n");
        printf("    %s x12, x12, x10\n", is_sub ? "sub" : "add");
        printf("    str w12, [x11, x9, lsl #2]\n");
    } else {
        ICE("arm64 indexed global update unsupported elem_size=%d", elem_size);
    }
}

static void arm64_update_global_member_indexed_from_local(const char *name,
                                                          int base_offset, int index_scale,
                                                          int member_offset, int load_size,
                                                          int index_offset, int index_load_size, int index_is_unsigned,
                                                          int rhs_offset, int rhs_load_size, int rhs_is_unsigned,
                                                          int is_sub) {
    int total_offset = base_offset + member_offset;
    int shift = arm64_scale_shift(index_scale);

    arm64_load_local_casted_into_reg("x9", "w9", index_offset, index_load_size, index_is_unsigned);
    arm64_load_local_casted_into_reg("x10", "w10", rhs_offset, rhs_load_size, rhs_is_unsigned);

    printf("    adrp x11, _%s@PAGE\n", name);
    printf("    add  x11, x11, _%s@PAGEOFF\n", name);

    if (total_offset != 0)
        emit_add_imm64("x11", "x11", total_offset);
    if (shift < 0)
        ICE("arm64 indexed global member update unsupported scale=%d", index_scale);

    if (load_size == 8) {
        printf("    ldr x12, [x11, x9");
        if (shift > 0)
            printf(", lsl #%d", shift);
        printf("]\n");
        printf("    %s x12, x12, x10\n", is_sub ? "sub" : "add");
        printf("    str x12, [x11, x9");
        if (shift > 0)
            printf(", lsl #%d", shift);
        printf("]\n");
    } else if (load_size == 4) {
        printf("    ldrsw x12, [x11, x9");
        if (shift > 0)
            printf(", lsl #%d", shift);
        printf("]\n");
        printf("    %s x12, x12, x10\n", is_sub ? "sub" : "add");
        printf("    str w12, [x11, x9");
        if (shift > 0)
            printf(", lsl #%d", shift);
        printf("]\n");
    } else {
        ICE("arm64 indexed global member update unsupported load_size=%d", load_size);
    }
}

static void arm64_update_global_member_indexed_imm_from_local(const char *name,
                                                              int base_offset, int index_scale,
                                                              int load_size,
                                                              int index_offset, int index_load_size,
                                                              int index_is_unsigned,
                                                              const char *op, long imm) {
    int shift = arm64_scale_shift(index_scale);
    int max_shift = (load_size == 8) ? 3 : 2;
    int use_indexed = (shift >= 0 && shift <= max_shift);

    arm64_load_local_casted_into_reg("x9", "w9", index_offset, index_load_size, index_is_unsigned);

    printf("    adrp x11, _%s@PAGE\n", name);
    printf("    add  x11, x11, _%s@PAGEOFF\n", name);
    if (base_offset != 0)
        emit_add_imm64("x11", "x11", base_offset);

    if (use_indexed) {
        if (load_size == 8) {
            printf("    ldr x12, [x11, x9");
            if (shift > 0)
                printf(", lsl #%d", shift);
            printf("]\n");
        } else if (load_size == 4) {
            printf("    ldrsw x12, [x11, x9");
            if (shift > 0)
                printf(", lsl #%d", shift);
            printf("]\n");
        }
        else
            ICE("arm64 indexed global member imm update unsupported load_size=%d", load_size);
    } else {
        emit_load_imm64("x13", index_scale);
        printf("    madd x11, x9, x13, x11\n");
        if (load_size == 8)
            printf("    ldr x12, [x11]\n");
        else if (load_size == 4)
            printf("    ldrsw x12, [x11]\n");
        else
            ICE("arm64 indexed global member imm update unsupported load_size=%d", load_size);
    }

    if (strcmp(op, "add") == 0 || strcmp(op, "sub") == 0) {
        long delta = (strcmp(op, "sub") == 0) ? -imm : imm;
        emit_add_imm64("x12", "x12", delta);
    } else {
        ICE("arm64 indexed global member imm update unsupported op=%s", op ? op : "<null>");
    }

    if (use_indexed) {
        if (load_size == 8) {
            if (shift > 0)
                printf("    str x12, [x11, x9, lsl #%d]\n", shift);
            else
                printf("    str x12, [x11, x9]\n");
        } else {
            if (shift > 0)
                printf("    str w12, [x11, x9, lsl #%d]\n", shift);
            else
                printf("    str w12, [x11, x9]\n");
        }
    } else {
        if (load_size == 8)
            printf("    str x12, [x11]\n");
        else
            printf("    str w12, [x11]\n");
    }
}

static void arm64_accumulate_global_member_indexed_double_from_local_ptr(const char *name,
                                                                         int base_offset, int index_scale,
                                                                         int frame_local_offset,
                                                                         int index_member_offset,
                                                                         int local_now_offset,
                                                                         int frame_time_member_offset) {
    int shift = arm64_scale_shift(index_scale);

    arm64_load_local_ptr_into_reg("x13", frame_local_offset);
    if (index_member_offset == 0)
        printf("    ldrsw x9, [x13]\n");
    else
        printf("    ldrsw x9, [x13, #%d]\n", index_member_offset);
    arm64_load_local_casted_into_reg("x10", "w10", local_now_offset, 8, 0);

    printf("    adrp x11, _%s@PAGE\n", name);
    printf("    add  x11, x11, _%s@PAGEOFF\n", name);
    if (base_offset != 0)
        emit_add_imm64("x11", "x11", base_offset);

    if (shift >= 0 && shift <= 3) {
        printf("    ldr x12, [x11, x9");
        if (shift > 0)
            printf(", lsl #%d", shift);
        printf("]\n");
    } else {
        emit_load_imm64("x12", index_scale);
        printf("    madd x11, x9, x12, x11\n");
        printf("    ldr x12, [x11]\n");
    }

    if (frame_time_member_offset == 0)
        printf("    ldr x13, [x13]\n");
    else
        printf("    ldr x13, [x13, #%d]\n", frame_time_member_offset);

    printf("    fmov d0, x12\n");
    printf("    fmov d1, x10\n");
    printf("    fmov d2, x13\n");
    printf("    fsub d1, d1, d2\n");
    printf("    fadd d0, d0, d1\n");
    printf("    fmov x12, d0\n");

    if (shift >= 0 && shift <= 3) {
        printf("    str x12, [x11, x9");
        if (shift > 0)
            printf(", lsl #%d", shift);
        printf("]\n");
    } else {
        printf("    str x12, [x11]\n");
    }
}

static void arm64_store_global_member_indexed_from_global_member_local(const char *name,
                                                                       int base_offset, int index_scale,
                                                                       int member_offset, int store_size,
                                                                       const char *index_name,
                                                                       int index_member_offset, int index_load_size,
                                                                       int index_is_unsigned, int index_add,
                                                                       int value_offset, int value_load_size,
                                                                       int value_is_unsigned) {
    int total_offset = base_offset + member_offset;
    int shift = arm64_scale_shift(index_scale);
    int max_shift = (store_size == 8) ? 3 : 2;
    int use_indexed = (shift >= 0 && shift <= max_shift);

    arm64_load_global_address_reg("x12", index_name);
    if (index_member_offset != 0)
        emit_add_imm64("x12", "x12", index_member_offset);
    if (index_load_size == 8)
        printf("    ldr x9, [x12]\n");
    else if (index_load_size == 4) {
        if (index_is_unsigned)
            printf("    ldr w9, [x12]\n");
        else
            printf("    ldrsw x9, [x12]\n");
    } else if (index_load_size == 2) {
        if (index_is_unsigned)
            printf("    ldrh w9, [x12]\n");
        else
            printf("    ldrsh x9, [x12]\n");
    } else if (index_load_size == 1) {
        if (index_is_unsigned)
            printf("    ldrb w9, [x12]\n");
        else
            printf("    ldrsb x9, [x12]\n");
    } else {
        ICE("arm64 indexed global-member store unsupported index_load_size=%d", index_load_size);
    }
    if (index_add != 0)
        emit_add_imm64("x9", "x9", index_add);

    arm64_load_local_casted_into_reg("x10", "w10", value_offset, value_load_size, value_is_unsigned);

    printf("    adrp x11, _%s@PAGE\n", name);
    printf("    add  x11, x11, _%s@PAGEOFF\n", name);
    if (total_offset != 0)
        emit_add_imm64("x11", "x11", total_offset);

    if (use_indexed) {
        if (store_size == 8) {
            if (shift > 0)
                printf("    str x10, [x11, x9, lsl #%d]\n", shift);
            else
                printf("    str x10, [x11, x9]\n");
        } else if (store_size == 4) {
            if (shift > 0)
                printf("    str w10, [x11, x9, lsl #%d]\n", shift);
            else
                printf("    str w10, [x11, x9]\n");
        } else {
            ICE("arm64 indexed global-member store unsupported store_size=%d", store_size);
        }
    } else {
        emit_load_imm64("x12", index_scale);
        printf("    madd x11, x9, x12, x11\n");
        if (store_size == 8)
            printf("    str x10, [x11]\n");
        else if (store_size == 4)
            printf("    str w10, [x11]\n");
        else
            ICE("arm64 indexed global-member store unsupported store_size=%d", store_size);
    }
}

static void arm64_store_global_member_indexed_imm_from_local(const char *name,
                                                             int base_offset, int index_scale,
                                                             int member_offset, int store_size,
                                                             int index_offset, int index_load_size,
                                                             int index_is_unsigned, long value) {
    int total_offset = base_offset + member_offset;
    int shift = arm64_scale_shift(index_scale);
    int max_shift = (store_size == 8) ? 3 : ((store_size == 2) ? 1 : 2);
    int use_indexed = (shift >= 0 && shift <= max_shift);

    arm64_load_local_casted_into_reg("x9", "w9", index_offset, index_load_size, index_is_unsigned);

    printf("    adrp x11, _%s@PAGE\n", name);
    printf("    add  x11, x11, _%s@PAGEOFF\n", name);
    if (total_offset != 0)
        emit_add_imm64("x11", "x11", total_offset);

    emit_load_imm64("x10", value);

    if (use_indexed) {
        if (store_size == 8)
            printf("    str x10, [x11, x9%s]\n", shift > 0 ? ", lsl #3" : "");
        else if (store_size == 4)
            printf("    str w10, [x11, x9%s]\n", shift > 0 ? ", lsl #2" : "");
        else if (store_size == 2)
            printf("    strh w10, [x11, x9%s]\n", shift > 0 ? ", lsl #1" : "");
        else if (store_size == 1)
            printf("    strb w10, [x11, x9]\n");
        else
            ICE("arm64 indexed global-member immediate store unsupported store_size=%d", store_size);
        return;
    }

    emit_load_imm64("x12", index_scale);
    printf("    madd x11, x9, x12, x11\n");
    if (store_size == 8)
        printf("    str x10, [x11]\n");
    else if (store_size == 4)
        printf("    str w10, [x11]\n");
    else if (store_size == 2)
        printf("    strh w10, [x11]\n");
    else if (store_size == 1)
        printf("    strb w10, [x11]\n");
    else
        ICE("arm64 indexed global-member immediate store unsupported store_size=%d", store_size);
}

static void arm64_store_global_ptr_member_indexed_imm_from_local(const char *name,
                                                                 int base_offset, int index_scale,
                                                                 int member_offset, int store_size,
                                                                 int index_offset, int index_load_size,
                                                                 int index_is_unsigned,
                                                                 int is_extern, long value) {
    int shift = arm64_scale_shift(index_scale);

    arm64_load_global_member_into_reg("x11", "w11", name, base_offset,
                                      TCC_SIZEOF_PTR, is_extern, 0);
    arm64_load_local_casted_into_reg("x9", "w9", index_offset,
                                     index_load_size, index_is_unsigned);

    if (shift >= 0 && shift <= 4) {
        printf("    add x11, x11, w9");
        arm64_print_sxtw_shift_suffix(shift);
        printf("\n");
    } else {
        if (index_load_size != 8)
            printf("    sxtw x9, w9\n");
        emit_load_imm64("x10", index_scale);
        printf("    madd x11, x9, x10, x11\n");
    }

    if (member_offset != 0)
        emit_add_imm64("x11", "x11", member_offset);

    if (value == 0) {
        if (store_size == 8)
            printf("    str xzr, [x11]\n");
        else if (store_size == 4)
            printf("    str wzr, [x11]\n");
        else if (store_size == 2)
            printf("    strh wzr, [x11]\n");
        else if (store_size == 1)
            printf("    strb wzr, [x11]\n");
        else
            ICE("arm64 indexed global-pointer-member immediate store unsupported store_size=%d", store_size);
        return;
    }

    emit_load_imm64("x10", value);
    if (store_size == 8)
        printf("    str x10, [x11]\n");
    else if (store_size == 4)
        printf("    str w10, [x11]\n");
    else if (store_size == 2)
        printf("    strh w10, [x11]\n");
    else if (store_size == 1)
        printf("    strb w10, [x11]\n");
    else
        ICE("arm64 indexed global-pointer-member immediate store unsupported store_size=%d", store_size);
}

static void arm64_store_global_ptr_member_indexed_from_local(const char *name,
                                                            int base_offset, int index_scale,
                                                            int member_offset, int store_size,
                                                            int index_offset, int index_load_size,
                                                            int index_is_unsigned,
                                                            int rhs_offset, int rhs_load_size,
                                                            int rhs_is_unsigned,
                                                            int is_extern) {
    int shift = arm64_scale_shift(index_scale);

    arm64_load_global_member_into_reg("x11", "w11", name, base_offset,
                                      TCC_SIZEOF_PTR, is_extern, 0);
    arm64_load_local_casted_into_reg("x9", "w9", index_offset,
                                     index_load_size, index_is_unsigned);

    if (shift >= 0 && shift <= 4) {
        printf("    add x11, x11, w9");
        arm64_print_sxtw_shift_suffix(shift);
        printf("\n");
    } else {
        if (index_load_size != 8)
            printf("    sxtw x9, w9\n");
        emit_load_imm64("x10", index_scale);
        printf("    madd x11, x9, x10, x11\n");
    }

    if (member_offset != 0)
        emit_add_imm64("x11", "x11", member_offset);

    arm64_load_local_casted_into_reg("x10", "w10", rhs_offset,
                                     rhs_load_size, rhs_is_unsigned);
    if (store_size == 8)
        printf("    str x10, [x11]\n");
    else if (store_size == 4)
        printf("    str w10, [x11]\n");
    else if (store_size == 2)
        printf("    strh w10, [x11]\n");
    else if (store_size == 1)
        printf("    strb w10, [x11]\n");
    else
        ICE("arm64 indexed global-pointer-member local store unsupported store_size=%d", store_size);
}

static void arm64_store_global_member_indexed_if_greater(const char *name,
                                                         int src_base_offset, int dst_base_offset,
                                                         int index_scale, int load_size,
                                                         int index_offset, int index_load_size, int index_is_unsigned,
                                                         int skip_label, int is_unsigned_cmp) {
    int src_total_offset = src_base_offset;
    int dst_total_offset = dst_base_offset;
    int shift = arm64_scale_shift(index_scale);

    arm64_load_local_casted_into_reg("x9", "w9", index_offset, index_load_size, index_is_unsigned);

    printf("    adrp x11, _%s@PAGE\n", name);
    printf("    add  x11, x11, _%s@PAGEOFF\n", name);

    if (shift < 0)
        ICE("arm64 indexed global member conditional store unsupported scale=%d", index_scale);

    if (load_size == 8) {
        if (src_total_offset != 0)
            emit_add_imm64("x12", "x11", src_total_offset);
        else
            printf("    mov x12, x11\n");
        if (dst_total_offset != 0)
            emit_add_imm64("x13", "x11", dst_total_offset);
        else
            printf("    mov x13, x11\n");
        printf("    ldr x14, [x12, x9");
        if (shift > 0)
            printf(", lsl #%d", shift);
        printf("]\n");
        printf("    ldr x15, [x13, x9");
        if (shift > 0)
            printf(", lsl #%d", shift);
        printf("]\n");
        printf("    cmp x14, x15\n");
        printf("    b.%s L%d\n", is_unsigned_cmp ? "ls" : "le", skip_label);
        printf("    str x14, [x13, x9");
        if (shift > 0)
            printf(", lsl #%d", shift);
        printf("]\n");
    } else if (load_size == 4) {
        if (src_total_offset != 0)
            emit_add_imm64("x12", "x11", src_total_offset);
        else
            printf("    mov x12, x11\n");
        if (dst_total_offset != 0)
            emit_add_imm64("x13", "x11", dst_total_offset);
        else
            printf("    mov x13, x11\n");
        if (is_unsigned_cmp) {
            printf("    ldr w14, [x12, x9");
            if (shift > 0)
                printf(", lsl #%d", shift);
            printf("]\n");
            printf("    ldr w15, [x13, x9");
            if (shift > 0)
                printf(", lsl #%d", shift);
            printf("]\n");
            printf("    cmp w14, w15\n");
            printf("    b.ls L%d\n", skip_label);
        } else {
            printf("    ldrsw x14, [x12, x9");
            if (shift > 0)
                printf(", lsl #%d", shift);
            printf("]\n");
            printf("    ldrsw x15, [x13, x9");
            if (shift > 0)
                printf(", lsl #%d", shift);
            printf("]\n");
            printf("    cmp x14, x15\n");
            printf("    b.le L%d\n", skip_label);
        }
        printf("    str w14, [x13, x9");
        if (shift > 0)
            printf(", lsl #%d", shift);
        printf("]\n");
    } else {
        ICE("arm64 indexed global member conditional store unsupported load_size=%d", load_size);
    }
}

static void arm64_set_local_ptr_indexed_bit_from_local(int ptr_local_offset,
                                                       int index_offset, int index_load_size, int index_is_unsigned,
                                                       int bit_offset, int bit_load_size, int bit_is_unsigned,
                                                       int divisor, int bit_mask) {
    const char *idx_ext = index_is_unsigned ? "uxtw" : "sxtw";

    if (divisor <= 0)
        ICE("arm64 local ptr bitset update invalid divisor=%d", divisor);
    if (bit_mask < 0 || bit_mask > 4095)
        ICE("arm64 local ptr bitset update invalid bit_mask=%d", bit_mask);

    arm64_load_local_sized(ptr_local_offset, TCC_SIZEOF_PTR);
    printf("    mov x9, x0\n");

    arm64_load_local_casted_into_reg("x10", "w10", index_offset, index_load_size, index_is_unsigned);
    printf("    mov w14, #%d\n", divisor);
    if (index_is_unsigned)
        printf("    udiv w10, w10, w14\n");
    else
        printf("    sdiv w10, w10, w14\n");

    arm64_load_local_casted_into_reg("x11", "w11", bit_offset, bit_load_size, bit_is_unsigned);
    printf("    and w11, w11, #%d\n", bit_mask);
    printf("    mov w12, #1\n");
    printf("    lsl w12, w12, w11\n");
    printf("    ldrb w13, [x9, w10, %s]\n", idx_ext);
    printf("    orr w13, w13, w12\n");
    printf("    strb w13, [x9, w10, %s]\n", idx_ext);
}

static void arm64_set_local_ptr_member_indexed_bit_from_local(int ptr_local_offset, int member_offset,
                                                              int index_offset, int index_load_size, int index_is_unsigned,
                                                              int bit_offset, int bit_load_size, int bit_is_unsigned,
                                                              int divisor, int bit_mask) {
    const char *idx_ext = index_is_unsigned ? "uxtw" : "sxtw";

    if (divisor <= 0)
        ICE("arm64 local ptr member bitset update invalid divisor=%d", divisor);
    if (bit_mask < 0 || bit_mask > 4095)
        ICE("arm64 local ptr member bitset update invalid bit_mask=%d", bit_mask);

    arm64_load_local_ptr_member(ptr_local_offset, member_offset, TCC_SIZEOF_PTR);
    printf("    mov x9, x0\n");

    arm64_load_local_casted_into_reg("x10", "w10", index_offset, index_load_size, index_is_unsigned);
    printf("    mov w14, #%d\n", divisor);
    if (index_is_unsigned)
        printf("    udiv w10, w10, w14\n");
    else
        printf("    sdiv w10, w10, w14\n");

    arm64_load_local_casted_into_reg("x11", "w11", bit_offset, bit_load_size, bit_is_unsigned);
    printf("    and w11, w11, #%d\n", bit_mask);
    printf("    mov w12, #1\n");
    printf("    lsl w12, w12, w11\n");
    printf("    ldrb w13, [x9, w10, %s]\n", idx_ext);
    printf("    orr w13, w13, w12\n");
    printf("    strb w13, [x9, w10, %s]\n", idx_ext);
}

static void arm64_test_local_ptr_indexed_bit_from_local(int ptr_local_offset,
                                                        int index_offset, int index_load_size, int index_is_unsigned,
                                                        int bit_offset, int bit_load_size, int bit_is_unsigned,
                                                        int divisor, int bit_mask) {
    const char *idx_ext = index_is_unsigned ? "uxtw" : "sxtw";

    if (divisor <= 0)
        ICE("arm64 local ptr bit test invalid divisor=%d", divisor);
    if (bit_mask < 0 || bit_mask > 4095)
        ICE("arm64 local ptr bit test invalid bit_mask=%d", bit_mask);

    arm64_load_local_sized(ptr_local_offset, TCC_SIZEOF_PTR);
    printf("    mov x9, x0\n");

    arm64_load_local_casted_into_reg("x10", "w10", index_offset, index_load_size, index_is_unsigned);
    printf("    mov w14, #%d\n", divisor);
    if (index_is_unsigned)
        printf("    udiv w10, w10, w14\n");
    else
        printf("    sdiv w10, w10, w14\n");

    arm64_load_local_casted_into_reg("x11", "w11", bit_offset, bit_load_size, bit_is_unsigned);
    printf("    and w11, w11, #%d\n", bit_mask);
    printf("    mov w12, #1\n");
    printf("    lsl w12, w12, w11\n");
    printf("    ldrb w13, [x9, w10, %s]\n", idx_ext);
    printf("    and w0, w13, w12\n");
}

static void arm64_test_local_ptr_member_indexed_bit_from_local(int ptr_local_offset, int member_offset,
                                                               int index_offset, int index_load_size, int index_is_unsigned,
                                                               int bit_offset, int bit_load_size, int bit_is_unsigned,
                                                               int divisor, int bit_mask) {
    const char *idx_ext = index_is_unsigned ? "uxtw" : "sxtw";

    if (divisor <= 0)
        ICE("arm64 local ptr member bit test invalid divisor=%d", divisor);
    if (bit_mask < 0 || bit_mask > 4095)
        ICE("arm64 local ptr member bit test invalid bit_mask=%d", bit_mask);

    arm64_load_local_ptr_member(ptr_local_offset, member_offset, TCC_SIZEOF_PTR);
    printf("    mov x9, x0\n");

    arm64_load_local_casted_into_reg("x10", "w10", index_offset, index_load_size, index_is_unsigned);
    printf("    mov w14, #%d\n", divisor);
    if (index_is_unsigned)
        printf("    udiv w10, w10, w14\n");
    else
        printf("    sdiv w10, w10, w14\n");

    arm64_load_local_casted_into_reg("x11", "w11", bit_offset, bit_load_size, bit_is_unsigned);
    printf("    and w11, w11, #%d\n", bit_mask);
    printf("    mov w12, #1\n");
    printf("    lsl w12, w12, w11\n");
    printf("    ldrb w13, [x9, w10, %s]\n", idx_ext);
    printf("    and w0, w13, w12\n");
}

static void arm64_branch_local_ptr_member_bit_test(int ptr_local_offset, int member_offset,
                                                   int load_size, int bit_index,
                                                   int branch_if_zero, int label) {
    if (bit_index < 0 || bit_index >= 64 || bit_index >= load_size * 8)
        ICE("arm64 local ptr member bit branch invalid bit_index=%d size=%d", bit_index, load_size);

    arm64_load_local_ptr_member(ptr_local_offset, member_offset, load_size);
    printf("    %s x0, #%d, L%d\n", branch_if_zero ? "tbz" : "tbnz", bit_index, label);
}

static void arm64_store_param_sized(int index, int offset, int size) {
    if (size <= 0)
        size = 8;
    if (size > 8)
        size = 8;

    if (index < 8) {
        int frame_offset = arm64_frame_offset(offset);
        const char *base = arm64_frame_base_reg();

        if (size == 1 || size == 2 || size == 4 || size == 8) {
            if (arm64_simm9(frame_offset)) {
                if (size == 8)
                    printf("    str x%d, [%s, #%d]\n", index, base, frame_offset);
                else if (size == 4)
                    printf("    str w%d, [%s, #%d]\n", index, base, frame_offset);
                else if (size == 2)
                    printf("    strh w%d, [%s, #%d]\n", index, base, frame_offset);
                else
                    printf("    strb w%d, [%s, #%d]\n", index, base, frame_offset);
            } else {
                emit_frame_offset_addr(offset);
                if (size == 8)
                    printf("    str x%d, [x9]\n", index);
                else if (size == 4)
                    printf("    str w%d, [x9]\n", index);
                else if (size == 2)
                    printf("    strh w%d, [x9]\n", index);
                else
                    printf("    strb w%d, [x9]\n", index);
            }
        } else {
            if (arm64_simm9(frame_offset) && arm64_simm9(frame_offset + 6))
                arm64_store_partial_reg_to_addr(index, base, frame_offset, size);
            else {
                emit_frame_offset_addr(offset);
                arm64_store_partial_reg_to_addr(index, "x9", 0, size);
            }
        }
    } else {
        /* AAPCS64 stack-passed integer args occupy 8-byte slots.
         * After our standard prologue, the first spilled arg is at x29+16.
         */
        int stack_offset = 16 + (index - 8) * 8;
        if (arm64_simm9(stack_offset)) {
            if (size >= 8)
                printf("    ldr x0, [x29, #%d]\n", stack_offset);
            else if (size >= 4) {
                printf("    ldr w0, [x29, #%d]\n", stack_offset);
                if (size > 4) {
                    int tail = size - 4;
                    if (tail >= 2) {
                        printf("    ldrh w10, [x29, #%d]\n", stack_offset + 4);
                        printf("    orr x0, x0, x10, lsl #32\n");
                        if (tail > 2) {
                            printf("    ldrb w10, [x29, #%d]\n", stack_offset + 6);
                            printf("    orr x0, x0, x10, lsl #48\n");
                        }
                    } else {
                        printf("    ldrb w10, [x29, #%d]\n", stack_offset + 4);
                        printf("    orr x0, x0, x10, lsl #32\n");
                    }
                }
            } else if (size >= 2) {
                printf("    ldrh w0, [x29, #%d]\n", stack_offset);
                if (size > 2) {
                    printf("    ldrb w10, [x29, #%d]\n", stack_offset + 2);
                    printf("    orr x0, x0, x10, lsl #16\n");
                }
            } else
                printf("    ldrb w0, [x29, #%d]\n", stack_offset);
        } else {
            emit_frame_offset_addr(stack_offset);
            if (size >= 8)
                printf("    ldr x0, [x9]\n");
            else if (size >= 4) {
                printf("    ldr w0, [x9]\n");
                if (size > 4) {
                    int tail = size - 4;
                    if (tail >= 2) {
                        printf("    ldrh w10, [x9, #4]\n");
                        printf("    orr x0, x0, x10, lsl #32\n");
                        if (tail > 2) {
                            printf("    ldrb w10, [x9, #6]\n");
                            printf("    orr x0, x0, x10, lsl #48\n");
                        }
                    } else {
                        printf("    ldrb w10, [x9, #4]\n");
                        printf("    orr x0, x0, x10, lsl #32\n");
                    }
                }
            } else if (size >= 2) {
                printf("    ldrh w0, [x9]\n");
                if (size > 2) {
                    printf("    ldrb w10, [x9, #2]\n");
                    printf("    orr x0, x0, x10, lsl #16\n");
                }
            } else
                printf("    ldrb w0, [x9]\n");
        }
        arm64_store_local_sized(offset, size);
    }
}

static void arm64_store_param(int index, int offset) {
    arm64_store_param_sized(index, offset, 8);
}

static void arm64_store_fp_param(int fp_index, int offset, int size) {
    int frame_offset = arm64_frame_offset(offset);
    const char *base = arm64_frame_base_reg();

    if (fp_index < 0)
        ICE("arm64 floating parameter register overflow");
    if (size != 4 && size != 8)
        size = 8;

    if (fp_index < 8) {
        if (arm64_simm9(frame_offset)) {
            printf("    str %c%d, [%s, #%d]\n",
                   size == 4 ? 's' : 'd', fp_index, base, frame_offset);
        } else {
            emit_frame_offset_addr(offset);
            printf("    str %c%d, [x9]\n", size == 4 ? 's' : 'd', fp_index);
        }
        return;
    }

    /*
     * AAPCS64 stack-passed floating/HFA elements occupy 8-byte slots after the
     * saved FP/LR pair in the caller frame. Reload the spilled slot and store
     * it into the callee local slot using the normal local-frame helpers.
     */
    {
        int stack_offset = 16 + (fp_index - 8) * 8;

        if (arm64_simm9(stack_offset)) {
            printf("    ldr %c16, [x29, #%d]\n",
                   size == 4 ? 's' : 'd', stack_offset);
        } else {
            emit_add_imm64("x9", "x29", stack_offset);
            printf("    ldr %c16, [x9]\n", size == 4 ? 's' : 'd');
        }

        if (arm64_simm9(frame_offset)) {
            printf("    str %c16, [%s, #%d]\n",
                   size == 4 ? 's' : 'd', base, frame_offset);
        } else {
            emit_frame_offset_addr(offset);
            printf("    str %c16, [x9]\n", size == 4 ? 's' : 'd');
        }
    }
}

static void arm64_emit_inline_copy(const char *dst_reg, const char *src_reg, int size) {
    int off = 0;

    while (off + 16 <= size && off <= 504) {
        printf("    ldp x4, x5, [%s, #%d]\n", src_reg, off);
        printf("    stp x4, x5, [%s, #%d]\n", dst_reg, off);
        off += 16;
    }
    while (off + 8 <= size) {
        printf("    ldr x6, [%s, #%d]\n", src_reg, off);
        printf("    str x6, [%s, #%d]\n", dst_reg, off);
        off += 8;
    }
    if (off + 4 <= size) {
        printf("    ldr w6, [%s, #%d]\n", src_reg, off);
        printf("    str w6, [%s, #%d]\n", dst_reg, off);
        off += 4;
    }
    if (off + 2 <= size) {
        printf("    ldrh w6, [%s, #%d]\n", src_reg, off);
        printf("    strh w6, [%s, #%d]\n", dst_reg, off);
        off += 2;
    }
    if (off < size) {
        printf("    ldrb w6, [%s, #%d]\n", src_reg, off);
        printf("    strb w6, [%s, #%d]\n", dst_reg, off);
    }
}

static void arm64_copy_local(int dst_offset, int src_offset, int size) {
    emit_frame_offset_addr_reg("x0", dst_offset);
    emit_frame_offset_addr_reg("x1", src_offset);
    arm64_emit_inline_copy("x0", "x1", size);
}

static void arm64_copy_local_to_ptr(int ptr_offset, int src_offset, int size) {
    if (arm64_simm9(ptr_offset))
        printf("    ldr x2, [x29, #%d]\n", ptr_offset);
    else {
        emit_frame_offset_addr(ptr_offset);
        printf("    ldr x2, [x9]\n");
    }

    emit_frame_offset_addr_reg("x1", src_offset);
    arm64_emit_inline_copy("x2", "x1", size);
}

static void arm64_copy_local_ptr_indexed_to_local(int dst_local_offset,
                                                  int src_ptr_local_offset,
                                                  int src_index_offset,
                                                  int src_index_load_size,
                                                  int src_index_is_unsigned,
                                                  int src_index_add,
                                                  int elem_size,
                                                  int copy_size) {
    emit_frame_offset_addr_reg("x0", dst_local_offset);
    arm64_addr_local_ptr_indexed_into_reg("x1", 1, src_ptr_local_offset,
                                          src_index_offset, src_index_load_size,
                                          src_index_is_unsigned, src_index_add,
                                          elem_size);
    arm64_emit_inline_copy("x0", "x1", copy_size);
}

static void arm64_copy_local_to_local_ptr_indexed(int dst_ptr_local_offset,
                                                  int dst_index_offset,
                                                  int dst_index_load_size,
                                                  int dst_index_is_unsigned,
                                                  int dst_index_add,
                                                  int elem_size,
                                                  int src_local_offset,
                                                  int copy_size) {
    arm64_addr_local_ptr_indexed_into_reg("x0", 0, dst_ptr_local_offset,
                                          dst_index_offset, dst_index_load_size,
                                          dst_index_is_unsigned, dst_index_add,
                                          elem_size);
    emit_frame_offset_addr_reg("x1", src_local_offset);
    arm64_emit_inline_copy("x0", "x1", copy_size);
}

static void arm64_copy_local_ptr_indexed_to_local_ptr_indexed(int dst_ptr_local_offset,
                                                              int dst_index_offset,
                                                              int dst_index_load_size,
                                                              int dst_index_is_unsigned,
                                                              int dst_index_add,
                                                              int src_ptr_local_offset,
                                                              int src_index_offset,
                                                              int src_index_load_size,
                                                              int src_index_is_unsigned,
                                                              int src_index_add,
                                                              int elem_size,
                                                              int copy_size) {
    arm64_addr_local_ptr_indexed_into_reg("x0", 0, dst_ptr_local_offset,
                                          dst_index_offset, dst_index_load_size,
                                          dst_index_is_unsigned, dst_index_add,
                                          elem_size);
    arm64_addr_local_ptr_indexed_into_reg("x1", 1, src_ptr_local_offset,
                                          src_index_offset, src_index_load_size,
                                          src_index_is_unsigned, src_index_add,
                                          elem_size);
    arm64_emit_inline_copy("x0", "x1", copy_size);
}

static void arm64_addr_local_ptr_member_indexed_member_into_reg(
    const char *dst_x, int dst_reg, int ptr_local_offset, int ptr_member_offset,
    int index_offset, int index_load_size, int index_is_unsigned, int index_add,
    int elem_size, int member_offset)
{
    char index_x[4];
    char index_w[4];
    int shift = arm64_scale_shift(elem_size);
    int index_reg = (dst_reg == 12) ? 9 : 12;

    arm64_load_local_ptr_into_reg(dst_x, ptr_local_offset);
    if (ptr_member_offset >= 0) {
        if (arm64_load_imm_offset_ok(TCC_SIZEOF_PTR, ptr_member_offset))
            printf("    ldr %s, [%s, #%d]\n", dst_x, dst_x, ptr_member_offset);
        else {
            emit_add_imm64(dst_x, dst_x, ptr_member_offset);
            printf("    ldr %s, [%s]\n", dst_x, dst_x);
        }
    }

    arm64_fill_reg_name(index_x, 'x', index_reg);
    arm64_fill_reg_name(index_w, 'w', index_reg);
    arm64_load_local_casted_into_reg(index_x, index_w, index_offset,
                                     index_load_size, index_is_unsigned);
    if (index_add)
        emit_add_imm64(index_x, index_x, index_add);

    if (shift >= 0 && shift <= 4) {
        printf("    add %s, %s, %s", dst_x, dst_x, index_w);
        arm64_print_sxtw_shift_suffix(shift);
        printf("\n");
    } else {
        int scale_scratch = arm64_alloc_scratch_reg(dst_reg, index_reg);
        char scale_x[4];

        arm64_fill_reg_name(scale_x, 'x', scale_scratch);
        if (index_load_size != 8)
            printf("    sxtw %s, %s\n", index_x, index_w);
        emit_load_imm64(scale_x, elem_size);
        printf("    madd %s, %s, %s, %s\n", dst_x, index_x, scale_x, dst_x);
        arm64_release_scratch_reg(scale_scratch);
    }

    if (member_offset)
        emit_add_imm64(dst_x, dst_x, member_offset);
}

static void arm64_copy_lpmidx_member_from_lpidx_member(
    int dst_ptr_local_offset, int dst_ptr_member_offset,
    int dst_index_offset, int dst_index_load_size, int dst_index_is_unsigned,
    int dst_index_add, int dst_elem_size, int dst_member_offset, int store_size,
    int src_ptr_local_offset, int src_index_offset, int src_index_load_size,
    int src_index_is_unsigned, int src_index_add, int src_elem_size,
    int src_member_offset, int load_size, int cast_size, int is_unsigned)
{
    (void)cast_size;

    arm64_addr_local_ptr_member_indexed_member_into_reg(
        "x11", 11, dst_ptr_local_offset, dst_ptr_member_offset,
        dst_index_offset, dst_index_load_size, dst_index_is_unsigned,
        dst_index_add, dst_elem_size, dst_member_offset);
    arm64_addr_local_ptr_member_indexed_member_into_reg(
        "x12", 12, src_ptr_local_offset, -1,
        src_index_offset, src_index_load_size, src_index_is_unsigned,
        src_index_add, src_elem_size, 0);
    if (src_member_offset)
        emit_add_imm64("x12", "x12", src_member_offset);
    if (load_size == 8)
        printf("    ldr x10, [x12]\n");
    else if (load_size == 4) {
        if (is_unsigned)
            printf("    ldr w10, [x12]\n");
        else
            printf("    ldrsw x10, [x12]\n");
    } else if (load_size == 2) {
        if (is_unsigned)
            printf("    ldrh w10, [x12]\n");
        else
            printf("    ldrsh x10, [x12]\n");
    } else if (load_size == 1) {
        if (is_unsigned)
            printf("    ldrb w10, [x12]\n");
        else
            printf("    ldrsb x10, [x12]\n");
    } else {
        ICE("arm64 indexed member copy unsupported load_size=%d", load_size);
    }

    if (store_size == 8)
        printf("    str x10, [x11]\n");
    else if (store_size == 4)
        printf("    str w10, [x11]\n");
    else if (store_size == 2)
        printf("    strh w10, [x11]\n");
    else if (store_size == 1)
        printf("    strb w10, [x11]\n");
    else
        ICE("arm64 indexed member copy unsupported store_size=%d", store_size);
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
    printf("    mov x1, x0\n");   /* src */
    printf("    mov x0, x17\n");  /* dst saved by acc_to_saved */
    if (size > 64) {
        emit_load_imm64("x2", size);
        arm64_call("memcpy");
        return;
    }
    arm64_emit_inline_copy("x0", "x1", size);
}

static void arm64_load_indexed(int base_offset, int elem_size) {
    emit_frame_offset_addr(base_offset);
    if (elem_size == 1 || elem_size == 2 || elem_size == 4 || elem_size == 8) {
        if (elem_size == 1)
            printf("    ldrb w0, [x9, w0, sxtw]\n");
        else if (elem_size == 2)
            printf("    ldrh w0, [x9, w0, sxtw #1]\n");
        else if (elem_size == 8)
            printf("    ldr x0, [x9, w0, sxtw #3]\n");
        else
            printf("    ldrsw x0, [x9, w0, sxtw #2]\n");
        return;
    }
    printf("    sxtw x0, w0\n");
    emit_load_imm64("x3", elem_size);
    printf("    madd x0, x0, x3, x9\n");
    if (elem_size == 1)
        printf("    ldrb w0, [x0]\n");
    else if (elem_size == 2)
        printf("    ldrh w0, [x0]\n");
    else if (elem_size == 8)
        printf("    ldr x0, [x0]\n");
    else
        printf("    ldrsw x0, [x0]\n");
}

static void arm64_store_indexed(int base_offset, int elem_size) {
    emit_frame_offset_addr(base_offset);
    if (elem_size == 1 || elem_size == 2 || elem_size == 4 || elem_size == 8) {
        if (elem_size == 1)
            printf("    strb w0, [x9, w1, sxtw]\n");
        else if (elem_size == 2)
            printf("    strh w0, [x9, w1, sxtw #1]\n");
        else if (elem_size == 8)
            printf("    str x0, [x9, w1, sxtw #3]\n");
        else
            printf("    str w0, [x9, w1, sxtw #2]\n");
        return;
    }
    printf("    sxtw x1, w1\n");
    emit_load_imm64("x3", elem_size);
    printf("    madd x1, x1, x3, x9\n");
    if (elem_size == 1)
        printf("    strb w0, [x1]\n");
    else if (elem_size == 2)
        printf("    strh w0, [x1]\n");
    else if (elem_size == 8)
        printf("    str x0, [x1]\n");
    else
        printf("    str w0, [x1]\n");
}

static void arm64_addr_local(int offset) {
    emit_add_imm64("x0", arm64_frame_base_reg(), arm64_frame_offset(offset));
}

static void arm64_addr_indexed(int base_offset, int elem_size) {
    int shift;
    shift = arm64_scale_shift(elem_size);
    emit_frame_offset_addr(base_offset);
    if (shift >= 0 && shift <= 4) {
        printf("    add x0, x9, w0");
        arm64_print_sxtw_shift_suffix(shift);
        printf("\n");
        return;
    }
    printf("    sxtw x0, w0\n");
    {
        int scale_scratch = arm64_alloc_scratch_reg(0, 9);
        char scale_x[4];
        arm64_fill_reg_name(scale_x, 'x', scale_scratch);
        emit_load_imm64(scale_x, elem_size);
        printf("    madd x0, x0, %s, x9\n", scale_x);
        arm64_release_scratch_reg(scale_scratch);
    }
}

static void arm64_add_offset(int offset) {
    /*
     * Route through the arg-register helper instead of forwarding the raw
     * `int` directly to emit_add_imm64(). Self-hosted builds already compile
     * arm64_add_arg_offset() correctly for negative immediates, which keeps
     * x0 +/- const call-arg materialization stable until the narrower
     * int-to-long forwarding issue is fixed generically.
     */
    arm64_add_arg_offset(0, 0, offset);
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

static void arm64_load_ptr_indexed(int elem_size, int load_size) {
    int shift = arm64_scale_shift(elem_size);
    int direct_ok = 0;

    if (shift >= 0) {
        if (load_size == 1)
            direct_ok = (shift == 0);
        else if (load_size == 2)
            direct_ok = (shift == 0 || shift == 1);
        else if (load_size == 4)
            direct_ok = (shift == 0 || shift == 2);
        else if (load_size == 8)
            direct_ok = (shift == 0 || shift == 3);
    }

    if (direct_ok) {
        if (load_size == 1)
            printf("    ldrb w0, [x1, w0");
        else if (load_size == 2)
            printf("    ldrh w0, [x1, w0");
        else if (load_size == 8)
            printf("    ldr x0, [x1, w0");
        else
            printf("    ldrsw x0, [x1, w0");
        arm64_print_sxtw_shift_suffix(shift);
        printf("]\n");
        return;
    }

    printf("    sxtw x0, w0\n");
    {
        int scale_scratch = arm64_alloc_scratch_reg(0, 1);
        char scale_x[4];
        arm64_fill_reg_name(scale_x, 'x', scale_scratch);
        emit_load_imm64(scale_x, elem_size);
        printf("    madd x0, x0, %s, x1\n", scale_x);
        arm64_release_scratch_reg(scale_scratch);
    }
    arm64_load_deref(load_size);
}

static void arm64_load_ptr_indexed_casted_base(const char *base_reg, int elem_size,
                                               int load_size, int cast_size,
                                               int is_unsigned) {
    int shift = arm64_scale_shift(elem_size);
    int direct_ok = 0;

    (void)load_size;
    if (shift >= 0) {
        if (cast_size == 1)
            direct_ok = (shift == 0);
        else if (cast_size == 2)
            direct_ok = (shift == 0 || shift == 1);
        else if (cast_size == 4)
            direct_ok = (shift == 0 || shift == 2);
        else if (cast_size == 8)
            direct_ok = (shift == 0 || shift == 3);
    }

    if (direct_ok) {
        if (cast_size == 1) {
            if (is_unsigned)
                printf("    ldrb w0, [%s, w0", base_reg);
            else
                printf("    ldrsb x0, [%s, w0", base_reg);
        } else if (cast_size == 2) {
            if (is_unsigned)
                printf("    ldrh w0, [%s, w0", base_reg);
            else
                printf("    ldrsh x0, [%s, w0", base_reg);
        } else if (cast_size == 4) {
            if (is_unsigned)
                printf("    ldr w0, [%s, w0", base_reg);
            else
                printf("    ldrsw x0, [%s, w0", base_reg);
        } else {
            printf("    ldr x0, [%s, w0", base_reg);
        }
        arm64_print_sxtw_shift_suffix(shift);
        printf("]\n");
        return;
    }

    printf("    sxtw x0, w0\n");
    {
        int scale_scratch = arm64_alloc_scratch_reg(0, arm64_reg_num(base_reg));
        char scale_x[4];
        arm64_fill_reg_name(scale_x, 'x', scale_scratch);
        emit_load_imm64(scale_x, elem_size);
        printf("    madd x0, x0, %s, %s\n", scale_x, base_reg);
        arm64_release_scratch_reg(scale_scratch);
    }
    arm64_load_member_ptr_casted(0, load_size, cast_size, is_unsigned);
}

static void arm64_load_ptr_indexed_casted(int elem_size, int load_size, int cast_size, int is_unsigned) {
    arm64_load_ptr_indexed_casted_base("x1", elem_size, load_size, cast_size, is_unsigned);
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
            printf("    ldrsw x0, [x0, #%d]\n", offset);
    } else if (arm64_uimm_scaled(offset, size)) {
        if (size == 1)
            printf("    ldrb w0, [x0, #%d]\n", offset);
        else if (size == 2)
            printf("    ldrh w0, [x0, #%d]\n", offset);
        else if (size == 8)
            printf("    ldr x0, [x0, #%d]\n", offset);
        else
            printf("    ldrsw x0, [x0, #%d]\n", offset);
    } else {
        int offset_scratch = arm64_alloc_scratch_reg(0, -1);
        char offset_x[4];
        arm64_fill_reg_name(offset_x, 'x', offset_scratch);
        emit_load_imm64(offset_x, offset);
        if (size == 8)
            printf("    ldr x0, [x0, %s]\n", offset_x);
        else
            printf("    ldrsw x0, [x0, %s]\n", offset_x);
        arm64_release_scratch_reg(offset_scratch);
    }
}

static void arm64_load_member_ptr_casted(int offset, int load_size, int cast_size, int is_unsigned) {
    (void)load_size;
    if (arm64_simm9(offset) || arm64_uimm_scaled(offset, cast_size)) {
        if (cast_size == 1) {
            if (is_unsigned)
                printf("    ldrb w0, [x0, #%d]\n", offset);
            else
                printf("    ldrsb x0, [x0, #%d]\n", offset);
            return;
        }
        if (cast_size == 2) {
            if (is_unsigned)
                printf("    ldrh w0, [x0, #%d]\n", offset);
            else
                printf("    ldrsh x0, [x0, #%d]\n", offset);
            return;
        }
        if (cast_size == 4) {
            if (is_unsigned)
                printf("    ldr w0, [x0, #%d]\n", offset);
            else
                printf("    ldrsw x0, [x0, #%d]\n", offset);
            return;
        }
        if (cast_size == 8) {
            printf("    ldr x0, [x0, #%d]\n", offset);
            return;
        }
    } else {
        int offset_scratch = arm64_alloc_scratch_reg(0, -1);
        char offset_x[4];
        arm64_fill_reg_name(offset_x, 'x', offset_scratch);
        emit_load_imm64(offset_x, offset);
        if (cast_size == 1) {
            if (is_unsigned)
                printf("    ldrb w0, [x0, %s]\n", offset_x);
            else
                printf("    ldrsb x0, [x0, %s]\n", offset_x);
            arm64_release_scratch_reg(offset_scratch);
            return;
        }
        if (cast_size == 2) {
            if (is_unsigned)
                printf("    ldrh w0, [x0, %s]\n", offset_x);
            else
                printf("    ldrsh x0, [x0, %s]\n", offset_x);
            arm64_release_scratch_reg(offset_scratch);
            return;
        }
        if (cast_size == 4) {
            if (is_unsigned)
                printf("    ldr w0, [x0, %s]\n", offset_x);
            else
                printf("    ldrsw x0, [x0, %s]\n", offset_x);
            arm64_release_scratch_reg(offset_scratch);
            return;
        }
        if (cast_size == 8) {
            printf("    ldr x0, [x0, %s]\n", offset_x);
            arm64_release_scratch_reg(offset_scratch);
            return;
        }
        arm64_release_scratch_reg(offset_scratch);
    }

    arm64_load_member_ptr(offset, load_size);
    arm64_cast_op(cast_size, is_unsigned);
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
    } else if (arm64_uimm_scaled(offset, size)) {
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

static void arm64_store_local_ptr_member(int local_offset, int member_offset, int size) {
    arm64_load_local_ptr_into_reg("x1", local_offset);
    arm64_store_member_ptr(member_offset, size);
}

static void arm64_store_local_ptr_member_from_local(int ptr_local_offset, int member_offset,
                                                    int value_local_offset, int value_load_size,
                                                    int value_is_unsigned, int shift_right,
                                                    int shift_is_unsigned, int store_size) {
    if (value_load_size == 8)
        arm64_load_local_sized(value_local_offset, value_load_size);
    else
        arm64_load_local_casted(value_local_offset, value_load_size, value_is_unsigned);

    if (shift_right > 0) {
        if (value_load_size == 8)
            printf("    %s x0, x0, #%d\n", shift_is_unsigned ? "lsr" : "asr", shift_right);
        else
            printf("    %s w0, w0, #%d\n", shift_is_unsigned ? "lsr" : "asr", shift_right);
    }

    arm64_load_local_ptr_into_reg("x1", ptr_local_offset);
    if (store_size == 1)
        printf("    strb w0, [x1, #%d]\n", member_offset);
    else if (store_size == 2)
        printf("    strh w0, [x1, #%d]\n", member_offset);
    else if (store_size == 8)
        printf("    str x0, [x1, #%d]\n", member_offset);
    else
        printf("    str w0, [x1, #%d]\n", member_offset);
}

static void arm64_store_local_ptr_member_imm(int local_offset, int member_offset,
                                             int size, long value) {
    arm64_load_local_ptr_into_reg("x1", local_offset);
    if (value == 0) {
        if (size == 1)
            printf("    strb wzr, [x1, #%d]\n", member_offset);
        else if (size == 2)
            printf("    strh wzr, [x1, #%d]\n", member_offset);
        else if (size == 8)
            printf("    str xzr, [x1, #%d]\n", member_offset);
        else
            printf("    str wzr, [x1, #%d]\n", member_offset);
        return;
    }

    emit_load_imm64("x9", value);
    if (size == 1)
        printf("    strb w9, [x1, #%d]\n", member_offset);
    else if (size == 2)
        printf("    strh w9, [x1, #%d]\n", member_offset);
    else if (size == 8)
        printf("    str x9, [x1, #%d]\n", member_offset);
    else
        printf("    str w9, [x1, #%d]\n", member_offset);
}

static void arm64_load_imm_into_arg(int index, long value) {
    const char *xreg;

    if (index < 0 || index > 7)
        ICE("arm64 arg register out of range");
    xreg = arm64_arg_xregs[index];
    emit_load_imm64(xreg, value);
}

static void arm64_add_arg_offset(int dst_index, int src_index, int offset) {
    const char *dst;
    const char *src;

    if (dst_index < 0 || dst_index > 7 || src_index < 0 || src_index > 7)
        ICE("arm64 arg register out of range");
    dst = arm64_arg_xregs[dst_index];
    src = arm64_arg_xregs[src_index];
    emit_add_imm64(dst, src, offset);
}

static int arm64_string_literal_has_embedded_nul(const char *value, size_t len) {
    for (size_t i = 0; i < len; i++) {
        if (value[i] == '\0')
            return 1;
    }
    return 0;
}

static void arm64_string_literal(int label, const char *value, size_t len, int width) {
    if (width > 1 || arm64_string_literal_has_embedded_nul(value, len)) {
        printf("%s\n", TCC_ASM_CONST_SECTION);
        if (width >= 4)
            printf(".align 2\n");
        else
            printf(".align 1\n");
    } else {
        printf(".section __TEXT,__cstring,cstring_literals\n");
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

static void arm64_load_string(int label) {
    printf("    adrp x0, .Lstr%d@PAGE\n", label);
    printf("    add  x0, x0, .Lstr%d@PAGEOFF\n", label);
}


static void arm64_load_func_addr(const char *name) {
    if (codegen_get_link_model() == LINK_DYNAMIC) {
        printf("    adrp x0, _%s@GOTPAGE\n", name);
        printf("    ldr  x0, [x0, _%s@GOTPAGEOFF]\n", name);
    } else {
        printf("    adrp x0, _%s@PAGE\n", name);
        printf("    add  x0, x0, _%s@PAGEOFF\n", name);
    }
}

static void arm64_inline_asm(const char *text) {
    if (!text || !*text)
        return;

    printf("    %s\n", text);
}

static void arm64_branch_if_zero(int label) {
    printf("    cbz x0, L%d\n", label);
}

static void arm64_branch_if_nonzero(int label) {
    printf("    cbnz x0, L%d\n", label);
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

static int arm64_try_emit_signbit_cmp_branch_imm(const char *op, int size, long imm, int label) {
    const char *bitop = NULL;
    const char *reg = NULL;
    int sign_bit;

    if (imm != 0)
        return 0;
    if (size != 4 && size != 8)
        return 0;

    if (strcmp(op, "jlt") == 0)
        bitop = "tbnz";
    else if (strcmp(op, "jge") == 0)
        bitop = "tbz";
    else
        return 0;

    reg = (size <= 4) ? "w0" : "x0";
    sign_bit = size * 8 - 1;
    printf("    %s %s, #%d, L%d\n", bitop, reg, sign_bit, label);
    return 1;
}

static const char *arm64_cmp_branch_cond(const char *op) {
    if (strcmp(op, "je") == 0)
        return "eq";
    else if (strcmp(op, "jne") == 0)
        return "ne";
    else if (strcmp(op, "jlt") == 0)
        return "lt";
    else if (strcmp(op, "jle") == 0)
        return "le";
    else if (strcmp(op, "jgt") == 0)
        return "gt";
    else if (strcmp(op, "jge") == 0)
        return "ge";
    else if (strcmp(op, "jult") == 0)
        return "lo";
    else if (strcmp(op, "jule") == 0)
        return "ls";
    else if (strcmp(op, "jugt") == 0)
        return "hi";
    else if (strcmp(op, "juge") == 0)
        return "hs";
    else
        ICE("unsupported arm64 compare branch op: %s", op ? op : "<null>");
}

static void arm64_emit_cmp_branch_imm_impl(const char *op, int size, long imm, int label) {
    const char *cond = arm64_cmp_branch_cond(op);

    if (arm64_try_emit_signbit_cmp_branch_imm(op, size, imm, label))
        return;

    if (imm >= 0 && imm <= 4095) {
        if (size <= 4)
            printf("    cmp w0, #%ld\n", imm);
        else
            printf("    cmp x0, #%ld\n", imm);
    } else {
        emit_load_imm64("x9", imm);
        if (size <= 4)
            printf("    cmp w0, w9\n");
        else
            printf("    cmp x0, x9\n");
    }
    printf("    b.%s L%d\n", cond, label);
}

static void arm64_emit_cmp_branch_impl(const char *op, int size, int label) {
    const char *cond = arm64_cmp_branch_cond(op);

    if (size <= 4)
        printf("    cmp w1, w0\n");
    else
        printf("    cmp x1, x0\n");
    printf("    b.%s L%d\n", cond, label);
}

static void arm64_emit_local_global_cmp_branch(int local_offset,
                                               int local_load_size,
                                               int local_is_unsigned,
                                               const char *global_name,
                                               int global_load_size,
                                               int global_is_unsigned,
                                               int global_is_extern,
                                               const char *op,
                                               int cmp_size,
                                               int label) {
    const char *cond = arm64_cmp_branch_cond(op);

    arm64_load_local_casted_into_reg("x9", "w9", local_offset,
                                     local_load_size, local_is_unsigned);
    arm64_load_global_member_into_reg("x10", "w10", global_name, 0,
                                      global_load_size, global_is_extern,
                                      global_is_unsigned);
    if (cmp_size <= 4)
        printf("    cmp w9, w10\n");
    else
        printf("    cmp x9, x10\n");
    printf("    b.%s L%d\n", cond, label);
}

static void arm64_emit_local_global_member_cmp_branch(int local_offset,
                                                      int local_load_size,
                                                      int local_is_unsigned,
                                                      const char *global_name,
                                                      int global_member_offset,
                                                      int global_load_size,
                                                      int global_is_unsigned,
                                                      int global_is_extern,
                                                      const char *op,
                                                      int cmp_size,
                                                      int label) {
    const char *cond = arm64_cmp_branch_cond(op);

    arm64_load_local_casted_into_reg("x11", "w11", local_offset,
                                     local_load_size, local_is_unsigned);
    arm64_load_global_member_into_reg("x10", "w10", global_name,
                                      global_member_offset, global_load_size,
                                      global_is_extern, global_is_unsigned);
    if (cmp_size <= 4)
        printf("    cmp w11, w10\n");
    else
        printf("    cmp x11, x10\n");
    printf("    b.%s L%d\n", cond, label);
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
 * Any arguments that remain on the real ABI stack, whether they are overflow
 * fixed arguments or variadic tail arguments, must be packed into 8-byte
 * slots. Therefore, after advancing past the register arguments, we repack
 * the remaining 16-byte temporary slots into 8-byte ABI slots in place before
 * emitting BL.
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
    arm64_needs_sp_restore = 1;

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
     * Now [sp] is arg(fixed), the first argument that must remain on the
     * real ABI stack.
     * Our internal slots are still 16 bytes wide:
     *
     *   [sp + 0]   = stack_arg0
     *   [sp + 16]  = stack_arg1
     *
     * AArch64 call ABI expects packed 8-byte stack slots:
     *
     *   [sp + 0]   = stack_arg0
     *   [sp + 8]   = stack_arg1
     *
     * Repack in-place from low to high addresses.  This is safe because the
     * destination for slot i is never above its source.
     */
    if (count > fixed) {
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
    arm64_needs_sp_restore = 1;

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
    printf("    blr x17\n");
}

Codegen arm64_codegen = {
    arm64_preamble,
    arm64_func_start,
    arm64_func_end,
    arm64_stack_alloc,
    arm64_stack_save_acc,
    arm64_stack_restore_acc,
    arm64_stack_alloc_acc,
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
    arm64_bitand_imm_op,
    arm64_bitor_op,
    arm64_bitnot_op,
    arm64_bitxor_op,
    arm64_shl_op,
    arm64_shr_op,
    arm64_udiv_op,
    arm64_umod_op,
    arm64_ushr_op,
    arm64_shl_imm_op,
    arm64_shr_imm_op,
    arm64_ushr_imm_op,
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
    NULL,
    arm64_copy_local,
    arm64_ptr_copy,
    arm64_copy_local_to_ptr,
    NULL,
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
    arm64_cmp_ge_u,
    arm64_cmp_eq_sized,
    arm64_cmp_ne_sized,
    arm64_cmp_lt_sized,
    arm64_cmp_le_sized,
    arm64_cmp_gt_sized,
    arm64_cmp_ge_sized,
    arm64_cmp_lt_u_sized,
    arm64_cmp_le_u_sized,
    arm64_cmp_gt_u_sized,
    arm64_cmp_ge_u_sized,
    NULL,
    arm64_div_sized_op,
    arm64_mod_sized_op,
    arm64_udiv_sized_op,
    arm64_umod_sized_op,
    NULL, /* emit_binop_sized */
    .emit_cmp_branch = arm64_emit_cmp_branch_impl,
    .emit_cmp_branch_imm = arm64_emit_cmp_branch_imm_impl,
    .emit_load_global_member = arm64_load_global_member,
    .emit_load_ptr_indexed = arm64_load_ptr_indexed,
    .emit_load_ptr_indexed_casted = arm64_load_ptr_indexed_casted,
    .emit_load_member_ptr_casted = arm64_load_member_ptr_casted,
    .emit_load_local_casted = arm64_load_local_casted,
    .emit_load_local_ptr_member = arm64_load_local_ptr_member,
    .emit_load_local_ptr_member_casted = arm64_load_local_ptr_member_casted,
    .emit_load_local_ptr_member_plus_local_into_arg =
        arm64_load_local_ptr_member_plus_local_into_arg,
    .emit_update_local_ptr_member_from_local_ptr_member =
        arm64_update_local_ptr_member_from_local_ptr_member,
    .emit_accumulate_local_ptr_member_double_call_delta =
        arm64_accumulate_local_ptr_member_double_call_delta,
    .emit_accumulate_local_ptr_member_member_double_call_delta =
        arm64_accumulate_local_ptr_member_member_double_call_delta,
    .emit_test_imm_setcc = arm64_test_imm_setcc,
    .emit_cmp_imm_setcc = arm64_cmp_imm_setcc,
    .emit_load_local_ptr_member_bitfield = arm64_load_local_ptr_member_bitfield,
    .emit_update_local_ptr_member_bitfield_const = arm64_update_local_ptr_member_bitfield_const,
    .emit_load_local_ptr_indexed_casted = arm64_load_local_ptr_indexed_casted,
    .emit_load_local_ptr_indexed_casted_into_arg =
        arm64_load_local_ptr_indexed_casted_into_arg,
    .emit_load_local_ptr_offset_indexed_casted_into_arg =
        arm64_load_local_ptr_offset_indexed_casted_into_arg,
    .emit_addr_local_ptr_offset_indexed_into_arg =
        arm64_addr_local_ptr_offset_indexed_into_arg,
    .emit_addr_local_ptr_member_indexed_into_arg =
        arm64_addr_local_ptr_member_indexed_into_arg,
    .emit_load_local_ptr_member_member_indexed_to_arg =
        arm64_load_local_ptr_member_member_indexed_to_arg,
    .emit_addr_lpm_postinc_midx =
        arm64_addr_lpm_postinc_midx,
    .emit_addr_lpm_postinc_lidx =
        arm64_addr_lpm_postinc_lidx,
    .emit_store_local_ptr_indexed_from_acc = arm64_store_local_ptr_indexed_from_acc,
    .emit_store_local_ptr_indexed_member_imm =
        arm64_store_local_ptr_indexed_member_imm,
    .emit_store_local_ptr_indexed_member_from_local =
        arm64_store_local_ptr_indexed_member_from_local,
    .emit_copy_local_ptr_indexed_to_local = arm64_copy_local_ptr_indexed_to_local,
    .emit_copy_local_to_local_ptr_indexed = arm64_copy_local_to_local_ptr_indexed,
    .emit_copy_local_ptr_indexed_to_local_ptr_indexed =
        arm64_copy_local_ptr_indexed_to_local_ptr_indexed,
    .emit_copy_lpmidx_member_from_lpidx_member =
        arm64_copy_lpmidx_member_from_lpidx_member,
    .emit_update_local_ptr_offset_indexed_from_local_ptr_offset =
        arm64_update_local_ptr_offset_indexed_from_local_ptr_offset,
    .emit_store_local_ptr_member = arm64_store_local_ptr_member,
    .emit_store_local_ptr_member_from_local = arm64_store_local_ptr_member_from_local,
    .emit_or_shl_imm = arm64_or_shl_imm_op,
    .emit_update_global_indexed_from_local = arm64_update_global_indexed_from_local,
    .emit_update_global_member_indexed_from_local = arm64_update_global_member_indexed_from_local,
    .emit_update_global_member_indexed_imm_from_local = arm64_update_global_member_indexed_imm_from_local,
    .emit_accumulate_global_member_indexed_double_from_local_ptr = arm64_accumulate_global_member_indexed_double_from_local_ptr,
    .emit_store_global_member_indexed_from_global_member_local = arm64_store_global_member_indexed_from_global_member_local,
    .emit_store_global_member_indexed_imm_from_local = arm64_store_global_member_indexed_imm_from_local,
    .emit_store_global_ptr_member_indexed_imm_from_local =
        arm64_store_global_ptr_member_indexed_imm_from_local,
    .emit_store_global_ptr_member_indexed_from_local =
        arm64_store_global_ptr_member_indexed_from_local,
    .emit_store_global_member_indexed_if_greater = arm64_store_global_member_indexed_if_greater,
    .emit_set_local_ptr_indexed_bit_from_local = arm64_set_local_ptr_indexed_bit_from_local,
    .emit_set_local_ptr_member_indexed_bit_from_local = arm64_set_local_ptr_member_indexed_bit_from_local,
    .emit_test_local_ptr_indexed_bit_from_local = arm64_test_local_ptr_indexed_bit_from_local,
    .emit_test_local_ptr_member_indexed_bit_from_local = arm64_test_local_ptr_member_indexed_bit_from_local,
    .emit_branch_local_ptr_member_bit_test = arm64_branch_local_ptr_member_bit_test,
    .emit_push_zero = arm64_push_zero,
    .emit_fp_binop = arm64_fp_binop,
    .emit_fp_cmp_branch = arm64_fp_cmp_branch,
    .emit_fp_cast_bits = arm64_fp_cast_bits,
    .emit_fp_to_acc_bits = arm64_fp_to_acc_bits,
    .emit_acc_bits_to_fp_return = arm64_acc_bits_to_fp_return,
    .emit_int_to_fp_bits = arm64_int_to_fp_bits,
    .emit_fp_bits_to_int = arm64_fp_bits_to_int,
    .emit_call_fp_args = arm64_call_fp_args,
    .emit_store_fp_param = arm64_store_fp_param,
    .emit_load_local_to_arg = arm64_load_local_into_arg,
    .emit_load_local_ptr_member_to_arg = arm64_load_local_ptr_member_into_arg,
    .emit_load_local_to_saved = arm64_load_local_into_saved,
    .emit_store_local_ptr_member_imm = arm64_store_local_ptr_member_imm,
    .emit_load_imm_to_arg = arm64_load_imm_into_arg,
    .emit_add_arg_offset = arm64_add_arg_offset
    ,
    .emit_load_global_member_indexed = arm64_load_global_member_indexed,
    .emit_load_global_member_indexed_into_arg =
        arm64_load_global_member_indexed_into_arg,
    .emit_addr_global_member_indexed_into_arg =
        arm64_addr_global_member_indexed_into_arg,
    .emit_cmp_local_ptr_member_imm_bool = arm64_cmp_local_ptr_member_imm_bool,
    .emit_cmp_local_global_addr_bool = arm64_cmp_local_global_addr_bool,
    .emit_cmp_local_ptr_member_global_addr_bool = arm64_cmp_local_ptr_member_global_addr_bool,
    .emit_update_local_ptr_member_imm = arm64_update_local_ptr_member_imm,
    .emit_update_global_member_imm = arm64_update_global_member_imm,
    .emit_update_global_imm = arm64_update_global_imm,
    .emit_postinc_global_member_to_local = arm64_postinc_global_member_to_local,
    .emit_update_global_from_local = arm64_update_global_from_local,
    .emit_update_global_member_from_local = arm64_update_global_member_from_local,
    .emit_store_global_member_from_local = arm64_store_global_member_from_local,
    .emit_store_gm_local_imm = arm64_store_gm_local_imm,
    .emit_store_gm_local_local_mask = arm64_store_gm_local_local_mask,
    .emit_store_local_deref_from_local_ptr_member = arm64_store_local_deref_from_local_ptr_member,
    .emit_store_saved_offset_imm = arm64_store_saved_offset_imm,
    .emit_const_addr_gidx_global_member_ptr_add_return = arm64_const_addr_gidx_global_member_ptr_add_return,
    .emit_gidx_gm_local_mask_ptr_return =
        arm64_gidx_gm_local_mask_ptr_return,
    .emit_zero_local_range = arm64_zero_local_range
    ,.emit_const_addr_gidx_local_ptr_add_return = arm64_const_addr_gidx_local_ptr_add_return
    ,.emit_store_param_sized = arm64_store_param_sized
    ,.emit_load_string_to_arg = arm64_load_string_into_arg
    ,.emit_load_func_addr_to_arg = arm64_load_func_addr_into_arg
    ,.emit_load_global_to_arg = arm64_load_global_into_arg
    ,.emit_load_global_member_to_arg = arm64_load_global_member_into_arg
    ,.emit_local_ptr_add_sub_locals_to_arg = arm64_local_ptr_add_sub_locals_to_arg
    ,.emit_store_lpm_postinc_lidx_imm =
        arm64_store_lpm_postinc_lidx_imm
    ,.emit_store_global_zero = arm64_emit_store_global_zero
    ,.emit_local_int_to_fp_const_div_return =
        arm64_emit_local_int_to_fp_const_div_return
    ,.emit_store_global_member_indexed_from_global_member =
        arm64_emit_store_global_member_indexed_from_global_member
    ,.emit_include_cache_insert_index_body =
        arm64_emit_include_cache_insert_index_body
    ,.emit_include_cache_insert_index_tail =
        arm64_emit_include_cache_insert_index_tail
    ,.emit_ifc_ident_body = arm64_emit_ifc_ident_body
    ,.emit_push_loop_tail = arm64_emit_push_loop_tail
    ,.emit_ir_push_loop_tail = arm64_emit_ir_push_loop_tail
    ,.emit_local_global_cmp_branch = arm64_emit_local_global_cmp_branch
    ,.emit_local_global_member_cmp_branch =
        arm64_emit_local_global_member_cmp_branch
    ,.emit_store_global_indexed_imm_from_local =
        arm64_store_global_indexed_imm_from_local
    ,.emit_call_saved_fp_args = arm64_call_saved_fp_args
};
