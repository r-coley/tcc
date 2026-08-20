#ifndef CODEGEN_H
#define CODEGEN_H

#include <stddef.h>

typedef enum {
    ASM_DIALECT_DEFAULT = 0,
    ASM_DIALECT_NASM,
    ASM_DIALECT_GAS_INTEL,
    ASM_DIALECT_GAS_ATT
} AsmDialect;

typedef enum {
    LINK_DYNAMIC,   /* default: link against dylibs, extern symbols need GOT on arm64 */
    LINK_STATIC     /* all symbols resolved at link time, no GOT needed */
} LinkModel;

void codegen_set_asm_dialect(AsmDialect d);
AsmDialect codegen_get_asm_dialect(void);
void codegen_set_link_model(LinkModel m);
LinkModel codegen_get_link_model(void);
void codegen_noop_call_args(int count, int fixed_params);
extern int arm64_simple_leaf;
extern int arm64_leaf_sp_frame_size;
void arm64_begin_fixed_frame(void);
int arm64_acquire_scratch_reg(int avoid_reg0, int avoid_reg1);
void arm64_release_scratch_reg_public(int reg);
void arm64_format_reg_name(char *buf, char prefix, int reg);


typedef struct Codegen {
    void (*emit_preamble)(void);
    void (*emit_function_start)(const char *name, int is_static);
    void (*emit_function_end)(void);

    void (*emit_stack_alloc)(int size);
    void (*emit_stack_save_acc)(void);
    void (*emit_stack_restore_acc)(void);
    void (*emit_stack_alloc_acc)(void);

    void (*emit_load_imm)(long value);
    void (*emit_push_acc)(void);
    void (*emit_pop_to_tmp)(void);
    void (*emit_pop_to_acc)(void);
    void (*emit_acc_to_arg)(int index);
    void (*emit_acc_to_tmp)(void);
    void (*emit_tmp_to_acc)(void);
    void (*emit_acc_to_saved)(void);
    void (*emit_saved_to_acc)(void);
    void (*emit_load_via_saved)(int size);
    void (*emit_store_via_saved)(int size);

    void (*emit_add)(void);
    void (*emit_sub)(void);
    void (*emit_negate)(void);
    void (*emit_mul)(void);
    void (*emit_div)(void);
    void (*emit_mod)(void);
    void (*emit_bitand)(void);
    void (*emit_bitand_imm)(long value);
    void (*emit_bitor)(void);
    void (*emit_bitnot)(void);
    void (*emit_bitxor)(void);
    void (*emit_shl)(void);
    void (*emit_shr)(void);
    void (*emit_udiv)(void);
    void (*emit_umod)(void);
    void (*emit_ushr)(void);
    void (*emit_shl_imm)(int imm);
    void (*emit_shr_imm)(int imm);
    void (*emit_ushr_imm)(int imm);
    void (*emit_cast)(int size, int is_unsigned);

    void (*emit_ptr_add)(int scale);
    void (*emit_ptr_sub)(int scale);

    void (*emit_cmp_eq)(void);
    void (*emit_cmp_ne)(void);
    void (*emit_cmp_lt)(void);
    void (*emit_cmp_le)(void);
    void (*emit_cmp_gt)(void);
    void (*emit_cmp_ge)(void);

    void (*emit_load_local)(int offset);
    void (*emit_store_local)(int offset);
    void (*emit_load_local_sized)(int offset, int size);
    void (*emit_store_local_sized)(int offset, int size);
    void (*emit_load_global)(const char *name, int size);
    void (*emit_load_global_extern)(const char *name);  /* GOT-indirect for dylib symbols */
    void (*emit_store_global)(const char *name, int size);
    void (*emit_store_global_extern)(const char *name); /* GOT-indirect for dylib symbols */
    void (*emit_load_global_indexed)(const char *name, int elem_size);
    void (*emit_store_global_indexed)(const char *name, int elem_size);
    void (*emit_store_param)(int index, int offset);
    void (*emit_copy_incoming_param)(int stack_offset, int local_offset, int size);
    void (*emit_copy_local)(int dst_offset, int src_offset, int size);
    void (*emit_ptr_copy)(int size); /* copy size bytes: acc=src ptr, saved=dst ptr */
    void (*emit_copy_local_to_ptr)(int ptr_offset, int src_offset, int size);
    void (*emit_push_struct_arg)(int size); /* acc=src ptr; pushes/copies one by-value struct arg */

    void (*emit_load_indexed)(int base_offset, int elem_size);
    void (*emit_store_indexed)(int base_offset, int elem_size);

    void (*emit_addr_local)(int offset);
    void (*emit_addr_indexed)(int base_offset, int elem_size);
    void (*emit_add_offset)(int offset);
    void (*emit_load_ptr_local)(int offset);
    void (*emit_store_ptr_local)(int offset);
    void (*emit_load_deref)(int size);
    void (*emit_store_deref)(int size);
    void (*emit_load_member_ptr)(int offset, int size);
    void (*emit_store_member_ptr)(int offset, int size);

    void (*emit_string_literal)(int label, const char *value, size_t len, int width);
    void (*emit_load_string)(int label);
    void (*emit_load_func_addr)(const char *name);
    void (*emit_inline_asm)(const char *text);

    void (*emit_branch_if_zero)(int label);
    void (*emit_branch_if_nonzero)(int label);
    void (*emit_branch)(int label);
    void (*emit_label)(int label);

    void (*emit_prepare_call_args)(int count, int fixed_params);
    void (*emit_call)(const char *name);
    void (*emit_call_saved)(void);
    void (*emit_cleanup_call_args)(int count, int fixed_params);
    void (*emit_label_named)(const char *name);
    void (*emit_branch_named)(const char *name);
    void (*emit_source_loc)(const char *file, int line); /* NULL if debug not enabled */
    void (*emit_cmp_lt_u)(void);
    void (*emit_cmp_le_u)(void);
    void (*emit_cmp_gt_u)(void);
    void (*emit_cmp_ge_u)(void);

    /* Optional: size-aware comparison callbacks (size=4 or size=8).
     * When set, ir_emit_binop calls these instead of the zero-arity variants
     * for comparisons where the operand elem_size is known. */
    void (*emit_cmp_eq_sized)(int size);
    void (*emit_cmp_ne_sized)(int size);
    void (*emit_cmp_lt_sized)(int size);
    void (*emit_cmp_le_sized)(int size);
    void (*emit_cmp_gt_sized)(int size);
    void (*emit_cmp_ge_sized)(int size);
    void (*emit_cmp_lt_u_sized)(int size);
    void (*emit_cmp_le_u_sized)(int size);
    void (*emit_cmp_gt_u_sized)(int size);
    void (*emit_cmp_ge_u_sized)(int size);

    /* Optional backend override for ++/-- on dereference lvalues.
     * Entry: accumulator contains the dereference address.
     * Exit: accumulator contains the C expression result:
     *   - old value for postfix
     *   - new value for prefix
     */
    void (*emit_incdec_deref)(int size, int is_inc, int is_postfix);

    /* Optional size-aware integer division/modulo callbacks (size=4 or 8).
     * Backends can use these to select 32-bit vs 64-bit divide instructions.
     * When NULL, ir_emit_binop falls back to the legacy unsized callbacks. */
    void (*emit_div_sized)(int size);
    void (*emit_mod_sized)(int size);
    void (*emit_udiv_sized)(int size);
    void (*emit_umod_sized)(int size);

    /* Optional generic size-aware integer binop override.
     * Appended at the end so existing positional backend initializers remain
     * source-compatible and default this hook to NULL.
     *
     * When set, ir_emit_binop gives the backend first refusal for sized integer
     * operations such as i64 mul/div/mod before falling back to the older hooks.
     */
    void (*emit_binop_sized)(const char *op, int size);

    /* Optional direct compare-and-branch override for control-flow lowering.
     * The compare operands are already in the backend's tmp/acc registers in
     * the same order used by the ordinary compare emitters.
     *
     * op is one of:
     *   eq ne lt le gt ge ult ule ugt uge
     *
     * size is the operand width in bytes (typically 4 or 8).
     * When NULL, IR emission falls back to materializing the compare result
     * and then branching on zero/nonzero without re-pushing the temporary.
     */
    void (*emit_cmp_branch)(const char *op, int size, int label);

    /* Optional direct global field load for expressions like global.field.
     * The backend should load the scalar object at symbol+offset into the
     * accumulator using the same integer width/sign conventions as its
     * ordinary global loads.
     */
    void (*emit_load_global_member)(const char *name, int offset, int size, int is_extern);

    /* Optional fused load for *(base + index * scale).
     * Entry: x1/tmp holds the base pointer and x0/acc holds the runtime index
     * exactly as produced by emit_ptr_add before the final add instruction.
     * The backend may emit a direct indexed memory operand instead of
     * materializing the intermediate address in the accumulator first.
     *
     * elem_size is the pointer arithmetic scale in bytes.
     * load_size is the final scalar load width in bytes.
     */
    void (*emit_load_ptr_indexed)(int elem_size, int load_size);

    /* Optional fused indexed scalar load with integer promotion/sign handling.
     * Entry: x1/tmp holds the base pointer and x0/acc holds the runtime index
     * exactly as produced by emit_ptr_add before the final add instruction.
     */
    void (*emit_load_ptr_indexed_casted)(int elem_size, int load_size,
                                         int cast_size, int is_unsigned);

    /* Optional fused scalar member load with integer promotion/sign handling.
     * Entry: x0/acc holds the base pointer.
     * The backend may fold a following integer cast into the load itself
     * (for example, ldrsh instead of ldrh + sign-extend).
     *
     * offset is the byte offset from the base pointer.
     * load_size is the storage width in bytes.
     * cast_size is the promoted integer width requested by the following cast.
     * is_unsigned matches the cast's unsignedness flag.
     */
    void (*emit_load_member_ptr_casted)(int offset, int load_size, int cast_size, int is_unsigned);

    /* Optional fused typed local load with sign/zero extension.
     * load_size is the stored object width in bytes.
     * is_unsigned matches the source integer type's unsignedness.
     */
    void (*emit_load_local_casted)(int offset, int load_size, int is_unsigned);

    /* Optional direct load/store through a local pointer slot plus
     * constant member/byte offset.
     */
    void (*emit_load_local_ptr_member)(int local_offset, int member_offset, int size);
    void (*emit_load_local_ptr_member_casted)(int local_offset, int member_offset,
                                              int load_size, int cast_size, int is_unsigned);
    void (*emit_load_local_ptr_member_plus_local_into_arg)(
        int arg_index, int ptr_local_offset, int member_offset, int member_size,
        int rhs_local_offset, int rhs_size, int rhs_is_unsigned);
    void (*emit_update_local_ptr_member_from_local_ptr_member)(
        int dst_ptr_local_offset, int dst_member_offset,
        int src_ptr_local_offset, int src_member_offset,
        int load_size, const char *op, int fp_size);
    void (*emit_accumulate_local_ptr_member_double_call_delta)(
        int ptr_local_offset, int member_offset,
        const char *call_name, int start_local_offset);
    void (*emit_accumulate_local_ptr_member_member_double_call_delta)(
        int ptr_local_offset, int ptr_member_offset, int double_member_offset,
        const char *call_name, int start_local_offset);
    void (*emit_test_imm_setcc)(long imm, const char *cond, int size);
    void (*emit_cmp_imm_setcc)(long imm, const char *cond, int size);
    void (*emit_load_local_ptr_indexed_casted)(int ptr_local_offset,
                                               int index_offset, int index_load_size,
                                               int index_is_unsigned, int index_add,
                                               int elem_size, int load_size,
                                               int cast_size, int is_unsigned);
    void (*emit_load_local_ptr_indexed_casted_into_arg)(int arg_index,
                                                        int ptr_local_offset,
                                                        int index_offset,
                                                        int index_load_size,
                                                        int index_is_unsigned,
                                                        int index_add,
                                                        int elem_size,
                                                        int load_size,
                                                        int cast_size,
                                                        int is_unsigned);
    void (*emit_load_local_ptr_offset_indexed_casted_into_arg)(int arg_index,
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
    void (*emit_addr_local_ptr_offset_indexed_into_arg)(int arg_index,
                                                        int ptr_local_offset,
                                                        int base_offset,
                                                        int index_offset,
                                                        int index_load_size,
                                                        int index_is_unsigned,
                                                        int elem_size,
                                                        int final_offset);
    void (*emit_addr_local_ptr_member_indexed_into_arg)(int arg_index,
                                                        int ptr_local_offset,
                                                        int member_offset,
                                                        int index_offset,
                                                        int index_load_size,
                                                        int index_is_unsigned,
                                                        int elem_size,
                                                        int final_offset);
    void (*emit_load_local_ptr_member_member_indexed_to_arg)(int arg_index,
                                                             int ptr_local_offset,
                                                             int first_member_offset,
                                                             int second_member_offset,
                                                             int index_offset,
                                                             int index_load_size,
                                                             int index_is_unsigned,
                                                             int elem_size,
                                                             int load_size,
                                                             int is_unsigned);
    void (*emit_store_local_ptr_indexed_from_acc)(int ptr_local_offset,
                                                  int index_offset, int index_load_size,
                                                  int index_is_unsigned, int index_divisor,
                                                  int index_add, int elem_size,
                                                  int store_size);
    void (*emit_store_local_ptr_indexed_member_imm)(int ptr_local_offset,
                                                    int index_offset,
                                                    int index_load_size,
                                                    int index_is_unsigned,
                                                    int index_add,
                                                    int elem_size,
                                                    int member_offset,
                                                    int store_size,
                                                    long value);
    void (*emit_store_local_ptr_indexed_member_from_local)(int ptr_local_offset,
                                                           int index_offset,
                                                           int index_load_size,
                                                           int index_is_unsigned,
                                                           int index_add,
                                                           int elem_size,
                                                           int member_offset,
                                                           int store_size,
                                                           int src_local_offset,
                                                           int src_load_size,
                                                           int src_is_unsigned);
    void (*emit_copy_local_ptr_indexed_to_local)(int dst_local_offset,
                                                 int src_ptr_local_offset,
                                                 int src_index_offset,
                                                 int src_index_load_size,
                                                 int src_index_is_unsigned,
                                                 int src_index_add,
                                                 int elem_size, int copy_size);
    void (*emit_copy_local_to_local_ptr_indexed)(int dst_ptr_local_offset,
                                                 int dst_index_offset,
                                                 int dst_index_load_size,
                                                 int dst_index_is_unsigned,
                                                 int dst_index_add,
                                                 int elem_size,
                                                 int src_local_offset,
                                                 int copy_size);
    void (*emit_copy_local_ptr_indexed_to_local_ptr_indexed)(int dst_ptr_local_offset,
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
                                                             int copy_size);
    void (*emit_copy_lpmidx_member_from_lpidx_member)(
        int dst_ptr_local_offset, int dst_ptr_member_offset,
        int dst_index_offset, int dst_index_load_size, int dst_index_is_unsigned,
        int dst_index_add, int dst_elem_size, int dst_member_offset, int store_size,
        int src_ptr_local_offset, int src_index_offset, int src_index_load_size,
        int src_index_is_unsigned, int src_index_add, int src_elem_size,
        int src_member_offset, int load_size, int cast_size, int is_unsigned);
    void (*emit_update_local_ptr_offset_indexed_from_local_ptr_offset)(
        int dst_ptr_local_offset, int dst_base_offset,
        int src_ptr_local_offset, int src_base_offset,
        int index_offset, int index_load_size, int index_is_unsigned,
        int elem_size, int load_size, const char *op, int fp_size);
    void (*emit_store_local_ptr_member)(int local_offset, int member_offset, int size);
    void (*emit_store_local_ptr_member_from_local)(int ptr_local_offset, int member_offset,
                                                   int value_local_offset, int value_load_size,
                                                   int value_is_unsigned, int shift_right,
                                                   int shift_is_unsigned, int store_size);

    /* Optional direct combine for lhs | (rhs << imm).
     * Entry: accumulator holds rhs, and the expression stack top holds lhs.
     * Exit: result is in the accumulator and the stacked lhs is consumed.
     */
    void (*emit_or_shl_imm)(int imm);

    /* Optional: compute the va_list base for va_start and leave it in acc.
     * Only x64 implements this (SysV register save area); other targets keep
     * the __tcc_va_base stack model and leave this NULL. */
    void (*emit_va_start)(void);

    /* Optional fused update for:
     *   global[index] = global[index] +/- local
     * where both index and rhs come from fixed local stack slots.
     *
     * index_load_size / rhs_load_size are the source widths in bytes.
     * index_is_unsigned / rhs_is_unsigned describe the local source types.
     * elem_size is the global array element width in bytes.
     * is_sub selects subtraction instead of addition.
     */
    void (*emit_update_global_indexed_from_local)(const char *name, int elem_size,
                                                  int index_offset, int index_load_size, int index_is_unsigned,
                                                  int rhs_offset, int rhs_load_size, int rhs_is_unsigned,
                                                  int is_sub);

    /* Optional fused update for:
     *   ((global + base_offset) + index*index_scale + member_offset) op= local
     * where index and rhs both come from fixed local stack slots.
     */
    void (*emit_update_global_member_indexed_from_local)(const char *name,
                                                         int base_offset, int index_scale,
                                                         int member_offset, int load_size,
                                                         int index_offset, int index_load_size, int index_is_unsigned,
                                                         int rhs_offset, int rhs_load_size, int rhs_is_unsigned,
                                                         int is_sub);
    void (*emit_update_global_member_indexed_imm_from_local)(const char *name,
                                                             int base_offset, int index_scale,
                                                             int load_size,
                                                             int index_offset, int index_load_size, int index_is_unsigned,
                                                             const char *op, long imm);
    void (*emit_accumulate_global_member_indexed_double_from_local_ptr)(const char *name,
                                                                        int base_offset, int index_scale,
                                                                        int frame_local_offset,
                                                                        int index_member_offset,
                                                                        int local_now_offset,
                                                                        int frame_time_member_offset);
    void (*emit_store_global_member_indexed_from_global_member_local)(const char *name,
                                                                      int base_offset, int index_scale,
                                                                      int member_offset, int store_size,
                                                                      const char *index_name,
                                                                      int index_member_offset, int index_load_size,
                                                                      int index_is_unsigned, int index_add,
                                                                      int value_offset, int value_load_size,
                                                                      int value_is_unsigned);
    void (*emit_store_global_member_indexed_imm_from_local)(const char *name,
                                                            int base_offset, int index_scale,
                                                            int member_offset, int store_size,
                                                            int index_offset, int index_load_size,
                                                            int index_is_unsigned, long value);
    void (*emit_store_global_ptr_member_indexed_imm_from_local)(const char *name,
                                                                int base_offset, int index_scale,
                                                                int member_offset, int store_size,
                                                                int index_offset, int index_load_size,
                                                                int index_is_unsigned,
                                                                int is_extern, long value);
    void (*emit_store_global_ptr_member_indexed_from_local)(const char *name,
                                                            int base_offset, int index_scale,
                                                            int member_offset, int store_size,
                                                            int index_offset, int index_load_size,
                                                            int index_is_unsigned,
                                                            int rhs_offset, int rhs_load_size,
                                                            int rhs_is_unsigned,
                                                            int is_extern);
    void (*emit_store_global_member_indexed_if_greater)(const char *name,
                                                        int src_base_offset, int dst_base_offset,
                                                        int index_scale, int load_size,
                                                        int index_offset, int index_load_size, int index_is_unsigned,
                                                        int skip_label, int is_unsigned_cmp);

    /* Optional fused local-pointer bitset update for:
     *   ptr[index/divisor] |= (1 << (bit_value & bit_mask))
     * where ptr and bit/index inputs come from fixed local stack slots.
     */
    void (*emit_set_local_ptr_indexed_bit_from_local)(int ptr_local_offset,
                                                      int index_offset, int index_load_size, int index_is_unsigned,
                                                      int bit_offset, int bit_load_size, int bit_is_unsigned,
                                                      int divisor, int bit_mask);
    void (*emit_set_local_ptr_member_indexed_bit_from_local)(int ptr_local_offset, int member_offset,
                                                             int index_offset, int index_load_size, int index_is_unsigned,
                                                             int bit_offset, int bit_load_size, int bit_is_unsigned,
                                                             int divisor, int bit_mask);
    void (*emit_test_local_ptr_indexed_bit_from_local)(int ptr_local_offset,
                                                       int index_offset, int index_load_size, int index_is_unsigned,
                                                       int bit_offset, int bit_load_size, int bit_is_unsigned,
                                                       int divisor, int bit_mask);
    void (*emit_test_local_ptr_member_indexed_bit_from_local)(int ptr_local_offset, int member_offset,
                                                              int index_offset, int index_load_size, int index_is_unsigned,
                                                              int bit_offset, int bit_load_size, int bit_is_unsigned,
                                                              int divisor, int bit_mask);
    void (*emit_branch_local_ptr_member_bit_test)(int ptr_local_offset, int member_offset,
                                                  int load_size, int bit_index,
                                                  int branch_if_zero, int label);

    /* Optional fast-path for IR_CONST 0 when the value is immediately pushed
     * onto the backend expression stack. */
    void (*emit_push_zero)(void);

    /* Optional arm64-style bridge hooks for keeping floating-point payloads
     * in the integer accumulator used by the IR path. */
    void (*emit_fp_binop)(const char *op, int size);
    void (*emit_fp_cmp_branch)(const char *op, int size, int label);
    void (*emit_fp_cast_bits)(int src_size, int dst_size);
    void (*emit_fp_to_acc_bits)(int size);
    void (*emit_acc_bits_to_fp_return)(int size);
    void (*emit_int_to_fp_bits)(int size, int is_unsigned);
    void (*emit_fp_bits_to_int)(int size, int is_unsigned);
    void (*emit_call_fp_args)(const char *name, int count, int fixed_params,
                              unsigned int fp_arg_mask,
                              unsigned int fp_arg_double_mask);
    void (*emit_store_fp_param)(int fp_index, int offset, int size);

    /* Optional direct compare-with-immediate branch.
     * Entry: accumulator holds the left operand.
     */
    void (*emit_cmp_branch_imm)(const char *op, int size, long imm, int label);

    /* Optional direct local load into a call argument register. */
    void (*emit_load_local_to_arg)(int index, int offset, int size);

    /* Optional direct local-pointer-member load into a call argument register. */
    void (*emit_load_local_ptr_member_to_arg)(int index, int local_offset,
                                              int member_offset, int size,
                                              int is_unsigned);

    /* Optional direct local load into the backend's indirect-call register. */
    void (*emit_load_local_to_saved)(int offset, int size);

    /* Optional direct immediate store through a local pointer slot. */
    void (*emit_store_local_ptr_member_imm)(int local_offset, int member_offset,
                                            int size, long value);

    /* Optional direct immediate load into a call argument register. */
    void (*emit_load_imm_to_arg)(int index, long value);

    /* Optional direct address derivation between call argument registers.
     * dst = src + offset
     */
    void (*emit_add_arg_offset)(int dst_index, int src_index, int offset);

    /* Optional direct global member indexed load:
     *   ((global + base_offset) + index*elem_size + member_offset)
     */
    void (*emit_load_global_member_indexed)(const char *name, int base_offset,
                                            int elem_size, int member_offset,
                                            int load_size, int cast_size, int is_unsigned,
                                            int index_offset, int index_load_size,
                                            int index_is_unsigned, int is_extern,
                                            int base_is_pointer);
    void (*emit_load_global_member_indexed_into_arg)(int arg_index,
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
    void (*emit_addr_global_member_indexed_into_arg)(int arg_index,
                                                     const char *name,
                                                     int base_offset,
                                                     int elem_size,
                                                     int index_offset,
                                                     int index_load_size,
                                                     int index_is_unsigned,
                                                     int final_offset,
                                                     int is_extern,
                                                     int base_is_pointer);

    /* Optional direct local pointer bitfield extraction load. */
    void (*emit_load_local_ptr_member_bitfield)(int local_offset, int member_offset,
                                                int load_size, int bit_offset,
                                                int bit_width, int is_unsigned);
    void (*emit_update_local_ptr_member_bitfield_const)(int local_offset, int member_offset,
                                                        int load_size, long clear_mask,
                                                        long set_bits);

    /* Optional direct local pointer member compare-immediate boolean result.
     * Leaves the boolean 0/1 result in the accumulator register.
     */
    void (*emit_cmp_local_ptr_member_imm_bool)(int local_offset, int member_offset,
                                               int load_size, int cast_size, int is_unsigned,
                                               const char *op, long imm);

    /* Optional direct local pointer vs global-address compare returning 0/1. */
    void (*emit_cmp_local_global_addr_bool)(int local_offset, const char *name,
                                            int is_extern, const char *op);

    /* Optional direct local pointer member vs global-address compare returning 0/1. */
    void (*emit_cmp_local_ptr_member_global_addr_bool)(int local_offset, int member_offset,
                                                       int load_size, const char *name,
                                                       int is_extern, const char *op);

    /* Optional direct local pointer member update:
     *   *(ptr + member_offset) op= imm
     * where ptr comes from a fixed local stack slot.
     */
    void (*emit_update_local_ptr_member_imm)(int local_offset, int member_offset,
                                             int load_size, const char *op, long imm);

    /* Optional direct global member update:
     *   *(global + member_offset) op= imm
     */
    void (*emit_update_global_member_imm)(const char *name, int member_offset,
                                          int load_size, const char *op, long imm);

    /* Optional direct global update:
     *   global op= imm
     */
    void (*emit_update_global_imm)(const char *name, int load_size,
                                   const char *op, long imm);
    void (*emit_postinc_global_member_to_local)(const char *name, int member_offset,
                                                int load_size, long step,
                                                int local_offset, int local_size);

    /* Optional direct global update:
     *   global op= local
     * where the rhs comes from a fixed local stack slot with an optional cast.
     */
    void (*emit_update_global_from_local)(const char *name, int load_size,
                                          int local_offset, int local_size,
                                          int local_is_unsigned,
                                          int is_sub);

    /* Optional direct global member update:
     *   *(global + member_offset) op= local
     * where the rhs comes from a fixed local stack slot with an optional cast.
     */
    void (*emit_update_global_member_from_local)(const char *name, int member_offset,
                                                 int load_size,
                                                 int local_offset, int local_size,
                                                 int local_is_unsigned,
                                                 int is_sub);

    /* Optional direct global member store:
     *   *(global + member_offset) = local
     * where the rhs comes from a fixed local stack slot with an optional cast.
     */
    void (*emit_store_global_member_from_local)(const char *name, int member_offset,
                                                int store_size,
                                                int local_offset, int local_size,
                                                int local_is_unsigned);
    void (*emit_store_gm_local_imm)(const char *name, int member_offset,
                                    int store_size,
                                    int local_offset, int local_size,
                                    int local_is_unsigned,
                                    long imm);
    void (*emit_store_gm_local_local_mask)(const char *name, int member_offset,
                                           int store_size,
                                           int lhs_offset, int lhs_size,
                                           int lhs_is_unsigned,
                                           int rhs_offset, int rhs_size,
                                           int rhs_is_unsigned,
                                           int mask);

    /* Optional direct:
     *   *(dst_local_ptr + store_offset) = src_local_ptr->member
     * with optional integer cast on the loaded member value.
     */
    void (*emit_store_local_deref_from_local_ptr_member)(int dst_local_offset,
                                                         int src_ptr_local_offset,
                                                         int member_offset,
                                                         int load_size, int cast_size,
                                                         int is_unsigned,
                                                         int store_offset, int store_size);

    /* Optional direct immediate store through the backend saved-address register:
     *   *(saved + offset) = value
     */
    void (*emit_store_saved_offset_imm)(int offset, int size, long value);

    /* Optional direct:
     *   return (base_name + base_offset) + ((global member) * scale)
     * with optional integer cast on the loaded global member index.
     */
    void (*emit_const_addr_gidx_global_member_ptr_add_return)(const char *base_name,
                                                              int base_offset,
                                                              const char *index_name,
                                                              int member_offset,
                                                              int load_size,
                                                              int cast_size,
                                                              int is_unsigned,
                                                              int scale,
                                                              int index_is_extern);

    /* Optional direct:
     *   return (base_name + base_offset) + (((global member + local) & mask) * scale)
     * with optional integer casts on both loaded indices.
     */
    void (*emit_gidx_gm_local_mask_ptr_return)(
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
        int scale);

    /* Optional direct:
     *   return (base_name + base_offset) + ((local index) * scale)
     * with optional integer cast on the loaded local index.
     */
    void (*emit_const_addr_gidx_local_ptr_add_return)(const char *base_name,
                                                      int base_offset,
                                                      int index_offset,
                                                      int load_size,
                                                      int cast_size,
                                                      int is_unsigned,
                                                      int scale);

    /* Optional direct zero-fill of a contiguous local stack byte range. */
    void (*emit_zero_local_range)(int start_offset, int size);

    /* Optional size-aware incoming parameter store for function prologues.
     * size is the source-language slot width to preserve in the callee local.
     * Backends may still receive the incoming value in a wider ABI register.
     */
    void (*emit_store_param_sized)(int index, int offset, int size);

    /* Optional direct argument materialization helpers for simple-call
     * lowering. These should write the final value straight into xN/wN
     * without routing through the accumulator, so earlier fixed arguments do
     * not get clobbered while later ones are loaded.
     */
    void (*emit_load_string_to_arg)(int index, int label);
    void (*emit_load_func_addr_to_arg)(int index, const char *name);
    void (*emit_load_global_to_arg)(int index, const char *name, int size,
                                    int is_extern, int is_unsigned);
    void (*emit_load_global_member_to_arg)(int index, const char *name,
                                           int offset, int size, int is_extern,
                                           int is_unsigned);

    /* Optional direct address:
     *   ptr->base_member + (ptr->index_member++) * elem_size
     * Result is left in the backend accumulator.
     */
    void (*emit_addr_lpm_postinc_midx)(int ptr_local_offset,
                                                              int base_member_offset,
                                                              int index_member_offset,
                                                              int index_load_size,
                                                              int index_is_unsigned,
                                                              int elem_size);
    void (*emit_addr_lpm_postinc_lidx)(int ptr_local_offset,
                                                              int member_offset,
                                                              int index_local_offset,
                                                              int index_load_size,
                                                              int index_is_unsigned,
                                                              int elem_size);
    void (*emit_local_ptr_add_sub_locals_to_arg)(int arg_index,
                                                 int ptr_local_offset,
                                                 int add_local_offset,
                                                 int add_local_size,
                                                 int add_is_unsigned,
                                                 int sub_local_offset,
                                                 int sub_local_size,
                                                 int sub_is_unsigned);
    void (*emit_store_lpm_postinc_lidx_imm)(int ptr_local_offset,
                                                                int member_offset,
                                                                int index_local_offset,
                                                                int index_load_size,
                                                                int index_is_unsigned,
                                                                int elem_size,
                                                                int store_size,
                                                                long imm);

    /* Optional backend-specific fast emitters used by IR pattern matchers.
     * Keeping these behind Codegen avoids direct target calls from ir.c. */
    void (*emit_store_global_zero)(const char *name, int size);
    void (*emit_local_int_to_fp_const_div_return)(int offset, int load_size,
                                                  int is_unsigned,
                                                  unsigned long long const_bits,
                                                  int fp_size);
    void (*emit_store_global_member_indexed_from_global_member)(const char *name,
                                                                int base_offset,
                                                                int index_scale,
                                                                const char *index_name,
                                                                int index_member_offset,
                                                                int index_load_size,
                                                                int index_is_unsigned,
                                                                const char *value_name,
                                                                int value_offset,
                                                                int value_load_size,
                                                                int value_is_unsigned,
                                                                int store_size);
    void (*emit_include_cache_insert_index_body)(int idx_offset);
    void (*emit_include_cache_insert_index_tail)(int idx_offset,
                                                 int bucket_offset);
    void (*emit_ifc_ident_body)(int idx_offset);
    void (*emit_push_loop_tail)(int index_offset, int break_offset,
                                int continue_offset, int depth_ptr_offset);
    void (*emit_ir_push_loop_tail)(int index_offset, int break_offset,
                                   int continue_offset, int depth_ptr_offset);
    void (*emit_local_global_cmp_branch)(int local_offset,
                                         int local_load_size,
                                         int local_is_unsigned,
                                         const char *global_name,
                                         int global_load_size,
                                         int global_is_unsigned,
                                         int global_is_extern,
                                         const char *op,
                                         int cmp_size,
                                         int label);
    void (*emit_local_global_member_cmp_branch)(int local_offset,
                                                int local_load_size,
                                                int local_is_unsigned,
                                                const char *global_name,
                                                int global_member_offset,
                                                int global_load_size,
                                                int global_is_unsigned,
                                                int global_is_extern,
                                                const char *op,
                                                int cmp_size,
                                                int label);
    void (*emit_store_global_indexed_imm_from_local)(const char *name,
                                                     int elem_size,
                                                     int index_offset,
                                                     int index_load_size,
                                                     int index_is_unsigned,
                                                     long value);
    void (*emit_call_saved_fp_args)(int count, int fixed_params,
                                    unsigned int fp_arg_mask,
                                    unsigned int fp_arg_double_mask);
} Codegen;

extern Codegen arm64_codegen;
extern Codegen x86_codegen;
extern Codegen x64_codegen;
extern Codegen mips_codegen;
extern Codegen m68k_codegen;
void arm64_set_debug(int enabled);
void arm64_clear_live_param_regs(void);
void arm64_mark_live_param_reg(int index);
void arm64_configure_scratch_param_regs(unsigned int scratch_mask);
void arm64_configure_saved_param_regs(int local_stack_size, unsigned int saved_mask);
int arm64_saved_param_stack_size(void);
int arm64_saved_param_dwarf_reg(int index);
void arm64_emit_scratch_param_setup(void);
void arm64_emit_saved_param_setup(void);
void x64_set_debug(int enabled);
void x86_set_debug(int enabled);
void m68k_set_cpu_name(const char *cpu_name);

#endif
