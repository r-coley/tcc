#ifndef PARSER_H
#define PARSER_H

#include "ast.h"

typedef struct Global {
	char name[64];
	int init_value;
	int is_array;
	int array_len;
	int elem_size;
	int array_dim_count;
	int array_dims[8];
	int is_unsigned;
	int ptr_elem_size;
	int is_string;
	int string_label;
	char string_value[256];
	int init_count;
	int init_values[1024];
	int is_string_array;
	int is_addr;
	char addr_name[64];
	int is_struct;
	int is_extern;
	int is_dylib;   /* symbol lives in a dylib and needs GOT access on arm64 */
	int is_static;
	char struct_name[64];
	/* For struct globals: per-slot symbol names for function pointer fields.
	 * init_syms[i] is non-empty when slot i holds a symbol address(not raw int). */
	char init_syms[128][64];
	int init_sym_count;
} Global;

extern Global globals[512];
extern int global_count;

Node *parse_program(const char *filename,const char *source);

/* Field size query for struct return copy — used by ir.c */
int struct_field_count(const char *struct_name);
int struct_field_offset(const char *struct_name, int field_index);
int struct_field_size(const char *struct_name, int field_index);

/* Variadic call info */
int func_fixed_params(const char *name); /* -1 if not variadic, else fixed param count */

#endif /* PARSER_H */
