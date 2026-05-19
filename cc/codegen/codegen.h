#ifndef CODEGEN_H
#define CODEGEN_H

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


typedef struct Codegen {
    void (*emit_preamble)(void);
    void (*emit_function_start)(const char *name, int is_static);
    void (*emit_function_end)(void);

    void (*emit_stack_alloc)(int size);

    void (*emit_load_imm)(int value);
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
    void (*emit_bitor)(void);
    void (*emit_bitnot)(void);
    void (*emit_bitxor)(void);
    void (*emit_shl)(void);
    void (*emit_shr)(void);
    void (*emit_cast)(int size);

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
    void (*emit_copy_local)(int dst_offset, int src_offset, int size);
    void (*emit_ptr_copy)(int size); /* copy size bytes: acc=src ptr, saved=dst ptr */
    void (*emit_copy_local_to_ptr)(int ptr_offset, int src_offset, int size);

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

    void (*emit_string_literal)(int label, const char *value);
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
} Codegen;

extern Codegen arm64_codegen;
extern Codegen x86_codegen;
extern Codegen x64_codegen;
extern Codegen mips_codegen;
void arm64_set_debug(int enabled);

#endif
