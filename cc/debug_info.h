#ifndef DEBUG_INFO_H
#define DEBUG_INFO_H

#include "ast.h"

/*
 * Skeleton model for future full DWARF debug information.
 *
 * The current -g path remains assembler-assisted .file/.loc emission in the
 * backends.  These structures intentionally do not emit any DWARF yet; they
 * provide a small, parser/codegen-independent place to grow compile-unit and
 * function metadata before wiring anything into object output.
 */

typedef struct DebugFunction {
	const char *name;
	const char *file;
	int line;
	int low_pc_label;
	int high_pc_label;
	int param_count;       /* number of formal parameters */
	const char **param_names;
	int *param_type_ids;
	int *param_offsets;
	int *param_regs;
	const char **param_struct_names;
	const char **param_pointer_struct_names;
	int *param_pointer_depths;
	NodeDebugLocal *locals;
	int local_count;
	int stack_size;        /* total stack frame size in bytes */
	int frame_base_reg;    /* DWARF register number for DW_AT_frame_base */
	int frame_base_bias;   /* added to raw stack offsets for DW_OP_fbreg */
} DebugFunction;

typedef struct DebugString {
	char *text;
} DebugString;

typedef struct DebugStructMember {
	const char *name;
	int offset;
	int type_id;
	const char *struct_name;
	int array_len;
	int array_elem_type_id;
	const char *array_elem_struct_name;
} DebugStructMember;

typedef struct DebugStructType {
	const char *name;
	int byte_size;
	DebugStructMember *members;
	int member_count;
	int member_cap;
} DebugStructType;

typedef struct DebugArrayType {
	int elem_type_id;
	const char *elem_struct_name;
	int count;
} DebugArrayType;

typedef struct DebugStructPointerType {
	const char *struct_name;
	int pointer_depth;
} DebugStructPointerType;

typedef struct DebugUnit {
	const char *source_file;
	const char *comp_dir;    /* DW_AT_comp_dir: absolute compilation directory */
	const char *producer;
	DebugFunction *functions;
	int function_count;
	int function_cap;
	DebugString *strings;
	int string_count;
	int string_cap;
	DebugStructType *structs;
	int struct_count;
	int struct_cap;
	DebugArrayType *arrays;
	int array_count;
	int array_cap;
	DebugStructPointerType *struct_ptrs;
	int struct_ptr_count;
	int struct_ptr_cap;
} DebugUnit;

void debug_unit_init(DebugUnit *du, const char *source_file, const char *producer);
void debug_unit_free(DebugUnit *du);
const char *debug_unit_intern_string(DebugUnit *du, const char *text);
void debug_unit_set_source(DebugUnit *du, const char *source_file);
void debug_unit_set_comp_dir(DebugUnit *du, const char *comp_dir);
DebugFunction *debug_unit_add_function(DebugUnit *du, const char *name,
                                        const char *file, int line);
void debug_function_set_range(DebugFunction *fn, int low_pc_label,
                              int high_pc_label);
void debug_function_set_params(DebugFunction *fn, int param_count, int stack_size);
void debug_function_set_frame_base(DebugFunction *fn, int dwarf_reg, int bias);
void debug_function_set_param_names(DebugUnit *du, DebugFunction *fn,
                                    char **param_names, int param_count);
void debug_function_set_param_types(DebugFunction *fn,
                                    const int *type_ids, int param_count);
void debug_function_set_param_offsets(DebugFunction *fn,
                                      const int *offsets, int param_count);
void debug_function_set_param_regs(DebugFunction *fn,
                                   const int *regs, int param_count);
void debug_function_set_param_structs(DebugUnit *du, DebugFunction *fn,
                                      char **struct_names, int param_count);
void debug_function_set_param_pointer_structs(DebugUnit *du, DebugFunction *fn,
                                               char **struct_names, const int *depths,
                                               int param_count);
void debug_function_set_locals(DebugUnit *du, DebugFunction *fn,
                               const NodeDebugLocal *locals, int local_count);
DebugStructType *debug_unit_add_struct_type(DebugUnit *du, const char *name, int byte_size);
void debug_struct_type_add_member(DebugUnit *du, DebugStructType *st, const char *name,
                                  int offset, int type_id);
void debug_struct_type_add_struct_member(DebugUnit *du, DebugStructType *st, const char *name,
                                         int offset, const char *struct_name);
void debug_struct_type_add_array_member(DebugUnit *du, DebugStructType *st, const char *name,
                                        int offset, int elem_type_id,
                                        const char *elem_struct_name, int count);
void debug_unit_add_array_type(DebugUnit *du, int elem_type_id, const char *elem_struct_name, int count);
void debug_unit_add_struct_pointer_type(DebugUnit *du, const char *struct_name);
void debug_unit_add_struct_pointer_type_depth(DebugUnit *du, const char *struct_name, int pointer_depth);
int debug_unit_string_offset(const DebugUnit *du, const char *text);
int debug_unit_debug_str_size(const DebugUnit *du);
void debug_unit_debug_dump_strings(const DebugUnit *du);
int debug_unit_debug_abbrev_size(void);
void debug_unit_debug_dump_abbrevs(void);
void debug_unit_debug_dump(const DebugUnit *du);
void debug_unit_intern_builtin_type_strings(DebugUnit *du);
void debug_unit_emit_debug_str_section(const DebugUnit *du);
void debug_unit_emit_debug_abbrev_section(void);
int debug_unit_debug_info_size(const DebugUnit *du);
void debug_unit_debug_dump_info(const DebugUnit *du);
void debug_unit_emit_debug_info_section(const DebugUnit *du);
void debug_reset_addr_index(void);

#endif /* DEBUG_INFO_H */
