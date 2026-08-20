#ifndef PARSER_H
#define PARSER_H

#include "ast.h"

/* Heap-allocated initialiser data — only created when a global has initialisers.
 * Replaces the former init_values[1024] and init_syms[128][64] embedded arrays,
 * which consumed ~12 KB per Global regardless of whether any initialiser existed. */
typedef struct GlobalInit {
	long long *values;       /* byte-level initialiser data, length = cap */
	char **syms;        /* symbol names for pointer-slot relocations, length = sym_cap */
	int   count;        /* number of initialised bytes */
	int   cap;          /* allocated capacity of values[] */
	int   sym_count;    /* number of symbol relocation slots in use */
	int   sym_cap;      /* allocated capacity of syms[] (in 8-byte slots) */
} GlobalInit;

typedef struct Global {
	char name[64];
	long long init_value;
	int has_initializer;
	int is_array;
	int array_len;
	int elem_size;
	int array_dim_count;
	int array_dims[8];
	int is_unsigned;
	int ptr_elem_size;
	Type *type;
	int is_string;
	int string_label;
	char *string_value;
	unsigned int string_len;
	int is_string_array;
	int is_addr;
	char addr_name[64];
	int addr_offset; /* byte offset from addr_name for &name[N] initializers */
	int is_struct;
	int is_extern;
	int is_dylib;   /* symbol lives in a dylib and needs GOT access on arm64 */
	int is_static;
	int is_thread_local;
	int align;
	char struct_name[64];
	GlobalInit *init;   /* NULL when no initialiser data; heap-allocated otherwise */
} Global;

typedef struct ParserStringLiteral {
	char *value;
	size_t len;
	int width;
} ParserStringLiteral;

typedef enum {
	PARSER_PROF_TOPLEVEL = 0,
	PARSER_PROF_FUNCTION,
	PARSER_PROF_FUNCTION_HEAD,
	PARSER_PROF_FUNCTION_PARAMS,
	PARSER_PROF_FUNCTION_BODY,
	PARSER_PROF_PARAM_LIST,
	PARSER_PROF_PARAM_DECL,
	PARSER_PROF_PROTOTYPE,
	PARSER_PROF_GLOBAL_DECL,
	PARSER_PROF_TYPE_NAME,
	PARSER_PROF_TYPE_BASE,
	PARSER_PROF_TYPE_RECORD,
	PARSER_PROF_TYPE_ENUM,
	PARSER_PROF_TYPE_TYPEDEF,
	PARSER_PROF_TYPE_POINTER,
	PARSER_PROF_BLOCK,
	PARSER_PROF_EXPR,
	PARSER_PROF_EXPR_ASSIGN,
	PARSER_PROF_EXPR_COND,
	PARSER_PROF_EXPR_UNARY,
	PARSER_PROF_EXPR_POSTFIX,
	PARSER_PROF_EXPR_FACTOR,
	PARSER_PROF_EXPR_IDENT,
	PARSER_PROF_EXPR_IDENT_CALL,
	PARSER_PROF_EXPR_IDENT_INDEX,
	PARSER_PROF_EXPR_IDENT_DOT,
	PARSER_PROF_EXPR_IDENT_ARROW,
	PARSER_PROF_EXPR_IDENT_VALUE,
	PARSER_PROF_EXPR_PAREN,
	PARSER_PROF_EXPR_INDEX,
	PARSER_PROF_EXPR_DOT,
	PARSER_PROF_EXPR_ARROW,
	PARSER_PROF_EXPR_CALL,
	PARSER_PROF_FIND_FIELD,
	PARSER_PROF_STRUCT_DEF,
	PARSER_PROF_UNION_DEF,
	PARSER_PROF_ENUM_SPEC,
	PARSER_PROFILE_BUCKET_COUNT
} ParserProfileBucket;

typedef struct ParserProfile {
	double bucket_time[PARSER_PROFILE_BUCKET_COUNT];
	unsigned long bucket_count[PARSER_PROFILE_BUCKET_COUNT];
} ParserProfile;



/* Grow the globals table by one and return a pointer to the new (zeroed) entry. */
Global *globals_push(void);

/* Free the init data for a single global (does not free the Global itself). */
void global_init_free(Global *g);

/* GlobalInit field accessors — used by parser.c and data_emit.c */
int         global_init_count(const Global *g);
void        global_set_init_count(Global *g, int count);
long long   global_init_byte(const Global *g, int idx);
void        global_set_init_byte(Global *g, int idx, long long value);
const char *global_init_sym(const Global *g, int slot);
void        global_set_init_sym(Global *g, int slot, const char *sym);
void        set_global_string_initializer_len(Global *g, const char *value, size_t len);
void        set_global_string_array_initializer_len(Global *g, const char *value, size_t len);

Node *parse_program(const char *filename,const char *source);
void  parser_reset(void); /* zero all parser state; call before reuse */
extern int parser_emit_debug;
extern int parser_profile_enabled_flag;
void  parser_profile_get(ParserProfile *out);
const char *parser_profile_bucket_name(ParserProfileBucket bucket);
void parser_register_string_literal(int label, const char *value, size_t len, int width);
int parser_lookup_string_literal(int label, const char **value_out, size_t *len_out,
                                 int *width_out);
void parser_note_block_scope_function_declaration(const char *name, Type *ret_type);

/* Global variable table accessors — used by ir.c and data_emit.c */
Global     *parser_global_at(int index);
int         parser_global_count(void);
int         parser_global_is_thread_local(const char *name);
int         parser_func_is_noreturn(const char *name);

/* Field size query for struct return copy — used by ir.c */
int struct_field_count(const char *struct_name);
int struct_field_offset(const char *struct_name, int field_index);
int struct_field_size(const char *struct_name, int field_index);
int struct_field_debug_type_id(const char *struct_name, int field_index);
const char *struct_field_struct_name(const char *struct_name, int field_index);
int struct_field_array_len(const char *struct_name, int field_index);
int struct_field_array_elem_debug_type_id(const char *struct_name, int field_index);
const char *struct_field_array_elem_struct_name(const char *struct_name, int field_index);
const char *struct_field_name(const char *struct_name, int field_index);
int struct_size(const char *struct_name);
int struct_hfa_info(const char *struct_name, int *elem_size, int *elem_count);
int parser_arm64_hfa_info_name(const char *name, int *elem_size, int *elem_count);
int parser_arm64_hfa_info_type(const Type *type, int *elem_size, int *elem_count);

/* Variadic call info */
int func_fixed_params(const char *name); /* -1 if not variadic, else fixed param count */

#endif /* PARSER_H */
