#ifndef AST_H
#define AST_H

#include <stdio.h>

typedef enum TypeKind {
	TY_INT,
	TY_CHAR,
	TY_SHORT,
	TY_PTR,
	TY_ARRAY,
	TY_STRUCT
} TypeKind;

typedef struct Type {
	TypeKind kind;
	struct Type *base;
	int size;
	int is_unsigned;
	int array_len;
	char struct_name[64];
} Type;

Type *type_int(void);
Type *type_uint(void);
Type *type_char(void);
Type *type_uchar(void);
Type *type_short(void);
Type *type_ushort(void);
Type *type_ptr(Type *base);
Type *type_array(Type *base, int len);
Type *type_struct(const char *name, int size);

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

typedef struct Node {
	NodeKind kind;
	Type *type;

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
	int fixed_params;   /* for variadic calls: number of named fixed params */
	int is_array_field; /* member is an array field — decays to pointer (address) */
	int array_len;
	int is_pointer;
	int elem_size;
	int is_unsigned;
	int string_label;
	int by_ref_arg;
	int returns_struct;
	int struct_return_size;
	int asm_is_volatile;
	int is_static;
	char return_struct_name[64];
	char name[64];
	char struct_name[64];
	char string_value[4096];

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
} Node;

Node *new_num(int value);
Node *new_var(const char *name, int offset);
Node *new_global(const char *name);
Node *new_global_index(const char *name, Node *index, int elem_size);
Node *new_member(const char *name, int offset);
Node *new_member_ptr(const char *name, Node *base, int field_offset);
Node *new_index(const char *name, int offset, Node *index);
Node *new_addr(Node *target);
Node *new_deref(Node *expr);
Node *new_string(const char *value, int label);
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
Node *new_call(const char *name, Node *args);
Node *new_indirect_call(Node *callee, Node *args);
Node *new_func_addr(const char *name);
Node *new_asm(const char *text, int is_volatile);
Node *fold_constants(Node *node);
Node *eliminate_dead_code(Node *node);
void free_ast(Node *node);
void dump_ast(Node *node, int indent);

const char *node_kind_name(NodeKind kind);
void node_print_location(FILE *out, const Node *node);
void node_error_at(const Node *node, const char *fmt, ...) __attribute__((noreturn));

#endif
