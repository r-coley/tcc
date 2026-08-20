#ifndef AST_H
#define AST_H

#include <stdio.h>
#include <stddef.h>

typedef enum TypeKind {
	TY_VOID,
	TY_INT,
	TY_CHAR,
	TY_SHORT,
	TY_FLOAT,
	TY_DOUBLE,
	TY_PTR,
	TY_ARRAY,
	TY_FUNC,
	TY_STRUCT,
	TY_UNION,
	TY_ENUM
} TypeKind;

typedef enum TypeSourceKind {
	TYPE_SOURCE_DEFAULT = 0,
	TYPE_SOURCE_VOID,
	TYPE_SOURCE_BOOL,
	TYPE_SOURCE_SCHAR,
	TYPE_SOURCE_LONG,
	TYPE_SOURCE_ULONG,
	TYPE_SOURCE_LLONG,
	TYPE_SOURCE_ULLONG,
	TYPE_SOURCE_FLOAT,
	TYPE_SOURCE_DOUBLE,
	TYPE_SOURCE_LONG_DOUBLE,
	TYPE_SOURCE_COMPLEX,
	TYPE_SOURCE_IMAGINARY,
	TYPE_SOURCE_TYPEDEF
} TypeSourceKind;

enum {
	TYPE_QUAL_CONST    = 1 << 0,
	TYPE_QUAL_VOLATILE = 1 << 1,
	TYPE_QUAL_RESTRICT = 1 << 2,
	TYPE_QUAL_ATOMIC   = 1 << 3
};

typedef struct Type {
	TypeKind kind;
	struct Type *base;
	int size;
	int is_unsigned;
	int qualifiers;
	int array_len;
	char struct_name[64];
	int source_kind;
	char source_name[64];
	int is_vm_type;
	char vla_bound_name[64];
	struct Type *vla_elem_type;
} Type;

Type *type_void(void);
Type *type_int(void);
Type *type_uint(void);
Type *type_char(void);
Type *type_uchar(void);
Type *type_short(void);
Type *type_ushort(void);
Type *type_float(void);
Type *type_double(void);
Type *type_long(void);
Type *type_ulong(void);
Type *type_llong(void);
Type *type_ullong(void);
Type *type_ptr(Type *base);
Type *type_array(Type *base, int len);
Type *type_func(Type *ret_type);
Type *type_func_proto(Type *ret_type, Type **param_types, int param_count,
                      int is_variadic, int fixed_param_count);
int type_func_metadata(const Type *type, Type ***out_param_types,
                       int *out_param_count, int *out_is_variadic,
                       int *out_fixed_param_count);
Type *type_struct(const char *name, int size);
Type *type_union(const char *name, int size);
Type *type_enum(const char *name);
Type *type_with_source(Type *type, int source_kind, const char *source_name);
Type *type_with_qualifiers(Type *type, int qualifiers);

int type_sizeof(const Type *type);
int type_alignof(const Type *type);
int type_is_void(const Type *type);
int type_is_integer(const Type *type);
int type_is_floating(const Type *type);
int type_is_fp_scalar(const Type *type);
int type_is_complex(const Type *type);
int type_is_imaginary(const Type *type);
int type_is_pointer(const Type *type);
int type_is_array(const Type *type);
int type_is_function(const Type *type);
int type_is_struct(const Type *type);
int type_is_union(const Type *type);
int type_is_enum(const Type *type);
int type_is_scalar(const Type *type);
int type_is_unsigned(const Type *type);
Type *type_pointee(const Type *type);
int type_array_len(const Type *type);
int type_elem_sizeof(const Type *type);
int type_debug_type_id(const Type *type);
int type_source_kind(const Type *type);
const char *type_source_name(const Type *type);
int type_has_source(const Type *type);
int type_source_is(const Type *type, int source_kind);
int type_source_is_typedef(const Type *type);
int type_source_is_void_spelling(const Type *type);
int type_source_is_bool_spelling(const Type *type);
int type_source_is_integer_spelling(const Type *type);
int type_source_is_floating_spelling(const Type *type);
int type_source_is_collapsed_scalar(const Type *type);
const char *type_source_display_name(const Type *type);
int type_qualifiers(const Type *type);
int type_has_qualifier(const Type *type, int qualifier);
int type_equal_unqualified(const Type *a, const Type *b);
int type_equal_qualified(const Type *a, const Type *b);
int type_function_compatible_unqualified(const Type *a, const Type *b);
int type_function_compatible_qualified(const Type *a, const Type *b);
int type_pointer_assignment_compatible(const Type *dst, const Type *src,
                                       int src_is_null_pointer_constant);
Type *type_pointer_conditional_result(Type *a, Type *b);

typedef enum {
	ND_NUM,
	ND_VAR,
	ND_GLOBAL,
	ND_GLOBAL_INDEX,
	ND_MEMBER,
	ND_MEMBER_PTR,
	ND_INDEX,
	ND_ADDR,
	ND_DEREF,
	ND_STRING,
	ND_FUNC_ADDR,

	ND_ADD,
	ND_SUB,
	ND_MUL,
	ND_DIV,
	ND_MOD,
	ND_BITAND,
	ND_BITOR,
	ND_BITXOR,
	ND_SHL,
	ND_SHR,

	ND_EQ,
	ND_NE,
	ND_LT,
	ND_LE,
	ND_GT,
	ND_GE,

	ND_LOGICAL_AND,
	ND_LOGICAL_OR,
	ND_COND,

	ND_NEG,
	ND_BITNOT,
	ND_NOT,
	ND_CAST,

	ND_PRE_INC,
	ND_PRE_DEC,
	ND_POST_INC,
	ND_POST_DEC,

	ND_ASSIGN,
	ND_STRUCT_ASSIGN,
	ND_DECL,
	ND_ARRAY_DECL,
	ND_PTR_DECL,
	ND_STRUCT_DECL,
	ND_RETURN,
	ND_LABEL,
	ND_GOTO,
	ND_IF,
	ND_WHILE,
	ND_FOR,
	ND_DO_WHILE,
	ND_SWITCH,
	ND_CASE,
	ND_DEFAULT,
	ND_BREAK,
	ND_CONTINUE,
	ND_BLOCK,
	ND_FUNC,
	ND_CALL,
	ND_ASM,
	ND_COMMA   /* eval left for side effects, result is right */
} NodeKind;

typedef enum DebugTypeId {
	DBG_TYPE_NONE = 0,
	DBG_TYPE_INT,
	DBG_TYPE_UINT,
	DBG_TYPE_CHAR,
	DBG_TYPE_UCHAR,
	DBG_TYPE_SHORT,
	DBG_TYPE_USHORT,
	DBG_TYPE_FLOAT,
	DBG_TYPE_DOUBLE,
	DBG_TYPE_PTR_VOID,
	DBG_TYPE_PTR_INT,
	DBG_TYPE_PTR_UINT,
	DBG_TYPE_PTR_CHAR,
	DBG_TYPE_PTR_UCHAR,
	DBG_TYPE_PTR_SHORT,
	DBG_TYPE_PTR_USHORT,
	DBG_TYPE_COUNT
} DebugTypeId;

typedef struct NodeDebugLocal {
	char *name;
	int offset;
	int type_id;
	char struct_name[64];
	char pointer_struct_name[64];
	int pointer_depth;
	int array_len;
	int array_elem_type_id;
	char array_elem_struct_name[64];
} NodeDebugLocal;

typedef struct Node {
	NodeKind kind;
	Type *type;
	Type *return_type;

	/* Source location captured when the parser created this AST node.
	 * filename/line/column follow #line directives and are intended for
	 * user-facing diagnostics.  pp_* records the physical token stream
	 * location, which is useful when diagnosing preprocessor mapping bugs. */
	int filename_id;
	int line;
	int column;
	int pp_filename_id;
	int pp_line;
	int pp_column;

	int value;
	int offset;
	int stack_size;
	int param_count;
	char **param_names;
	int *param_type_ids;
	int *param_offsets;
	int *param_abi_sizes;
	char **param_struct_names;
	char **param_pointer_struct_names;
	int *param_pointer_depths;
	NodeDebugLocal *debug_locals;
	int debug_local_count;
	int suppress_debug_loc; /* synthetic compiler-generated code: do not advance .loc */
	int fixed_params;   /* for variadic calls: number of named fixed params */
	int is_array_field; /* member is an array field — decays to pointer (address) */
	int is_const_lvalue;
	int is_bitfield;
	int bit_offset;
	int bit_width;
	int bit_storage_size;
	int array_len;
	int is_pointer;
	int elem_size;
	int is_unsigned;
	int string_label;
	int by_ref_arg;
	int returns_struct;
	int aggregate_abi_class;
	int aggregate_abi_reg_count;
	int struct_return_size;
	int asm_is_volatile;
	int is_static;
	char return_struct_name[64];
	char name[64];
	char struct_name[64];
	char *string_value;
	size_t string_len;
	int string_width;
	int is_fp_num;

	struct Node *left;
	struct Node *right;

	struct Node *init;
	struct Node *cond;
	struct Node *inc;
	struct Node *then_body;
	struct Node *else_body;

	struct Node *body;
	struct Node *args;
	struct Node *next;

	/* 64-bit numeric value for ND_NUM literals that exceed int range. */
	long long_value;
} Node;

Node *new_num(int value);
Node *new_num_long(long value);
Node *new_num_fp(Type *type, const char *text);
Node *new_var(const char *name, int offset);
Node *new_global(const char *name);
Node *new_global_index(const char *name, Node *index, int elem_size);
Node *new_member(const char *name, int offset);
Node *new_member_ptr(const char *name, Node *base, int field_offset);
Node *new_index(const char *name, int offset, Node *index);
Node *new_addr(Node *target);
Node *new_deref(Node *expr);
Node *new_string(const char *value, int label);
Node *new_string_len_width(const char *value, size_t len, int label, int width);
Node *new_binary(NodeKind kind, Node *left, Node *right);
Node *new_unary(NodeKind kind, Node *expr);
Node *new_cast(Node *expr, Type *type);
Node *new_incdec(NodeKind kind, Node *target);
Node *new_assign(Node *var, Node *expr);
Node *new_struct_assign(Node *dst, Node *src, int size);
Node *new_decl(const char *name, int offset);
Node *new_array_decl(const char *name, int offset, int array_len);
Node *new_ptr_decl(const char *name, int offset);
Node *new_struct_decl(const char *name, int offset);
Node *new_return(Node *expr);
Node *new_label_stmt(const char *name);
Node *new_goto_stmt(const char *name);
Node *new_if(Node *cond, Node *then_body, Node *else_body);
Node *new_conditional(Node *cond, Node *then_expr, Node *else_expr);
Node *new_while(Node *cond, Node *body);
Node *new_for(Node *init, Node *cond, Node *inc, Node *body);
Node *new_do_while(Node *body, Node *cond);
Node *new_switch(Node *cond, Node *cases);
Node *new_case(int value, Node *body);
Node *new_default(Node *body);
Node *new_break(void);
Node *new_continue(void);
Node *new_block(Node *body);
Node *new_func(const char *name, Node *body, int stack_size, int param_count);
void node_set_param_names(Node *node, char **param_names, int param_count);
void node_set_param_type_ids(Node *node, const int *type_ids, int param_count);
void node_set_param_offsets(Node *node, const int *offsets, int param_count);
void node_set_param_abi_sizes(Node *node, const int *sizes, int param_count);
void node_set_param_structs(Node *node, char **struct_names, int param_count);
void node_set_param_pointer_structs(Node *node, char **struct_names,
                                    const int *depths, int param_count);
void node_set_debug_locals(Node *node, const char **names, const int *offsets,
                           const int *type_ids, const char **struct_names,
                           const char **pointer_struct_names,
                           const int *pointer_depths,
                           const int *array_lens,
                           const int *array_elem_type_ids,
                           const char **array_elem_struct_names, int count);
Node *new_call(const char *name, Node *args);
Node *new_indirect_call(Node *callee, Node *args);
Node *new_func_addr(const char *name);
Node *new_asm(const char *text, int is_volatile);
Node *fold_constants(Node *node);
Node *eliminate_dead_code(Node *node);
int node_is_null_pointer_constant(const Node *node);
void free_ast(Node *node);
void dump_ast(Node *node, int indent);

const char *node_kind_name(NodeKind kind);
void node_print_location(FILE *out, const Node *node);
void node_error_at(const Node *node, const char *fmt, ...) __attribute__((noreturn));

#endif
