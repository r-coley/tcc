/*
 * parser_internal.h — shared state and cross-file prototypes for the parser.
 *
 * Included by parser.c, expr.c, and stmt.c.  NOT part of the public API
 * (use parser.h for that).
 */
#ifndef PARSER_INTERNAL_H
#define PARSER_INTERNAL_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "tcc.h"
#include "lexer.h"
#include "ast.h"
#include "parser.h"
#include "preprocess.h"

#define MAX_ARRAY_DIMS 8

typedef enum AggregateAbiClass {
	AGGREGATE_ABI_NONE = 0,
	AGGREGATE_ABI_BYREF,
	AGGREGATE_ABI_HFA,
	AGGREGATE_ABI_INTREGS,
	AGGREGATE_ABI_X64_COMPLEX_FLOAT,
	AGGREGATE_ABI_X64_COMPLEX_DOUBLE
} AggregateAbiClass;

typedef struct {
	int  struct_arg_temp_id;
	int  compound_arg_temp_id;
	int  global_compound_literal_id;
	int  returns_struct;
	int  return_abi_class;
	int  return_abi_reg_count;
	char func_name[64];
	int  return_size;
	char return_struct_name[64];
	int  returns_pointer;
	int  return_elem_size;
	Type *return_type;
	int  is_noreturn;
	char function_name[64];
	int  file_static;
	int  file_thread_local;
	int  gnu_extern_inline_definition;
} ParserFunc;

extern ParserFunc pfunc;
int parser_classify_aggregate_abi(Type *type, int *reg_count_out);
int parser_complex_function_signature_supported(const Type *type);
int parser_direct_complex_lane_info(const Type *type, int *elem_size, int *elem_count);
Node *expr_coerce_value_for_type(Node *value, Type *type);
Node *expr_coerce_scalar_condition(Node *value);

#define parser_alloc_struct_arg_temp_id() (pfunc.struct_arg_temp_id++)
#define parser_alloc_compound_arg_temp_id() (pfunc.compound_arg_temp_id++)


static inline void
reject_c89_c99_keyword_token(TokenKind kind)
{
	if (kind == TOK_ATOMIC) {
		if (tcc_lang_is_c89_or_c90())
			fatal_cur("_Atomic is not allowed in C89/C90 mode\n");
		if (!tcc_lang_at_least(LANG_C11))
			fatal_cur("_Atomic is not allowed before C11\n");
	}
	if (kind == TOK_NORETURN) {
		if (!tcc_lang_at_least(LANG_C11))
			fatal_cur("_Noreturn is not allowed before C11\n");
	}

	if (!tcc_lang_is_c89_or_c90())
		return;

	if (kind == TOK_BOOL)
		fatal_cur("_Bool is not allowed in C89/C90 mode\n");
	if (kind == TOK_RESTRICT)
		fatal_cur("restrict is not allowed in C89/C90 mode\n");
}

static inline void
reject_thread_local_storage_specifier(void)
{
	if (!tcc_lang_at_least(LANG_C11))
		fatal_cur("_Thread_local is not allowed before C11\n");
}

static inline void
reject_plain_thread_local_keyword_before_c23(const Token *token)
{
	if (!tcc_lang_at_least(LANG_C23) &&
	    token &&
	    token->kind == TOK_IDENT &&
	    token->text &&
	    STRCMP(token->text, "thread_local") == 0)
		fatal_token(token, "thread_local is not allowed before C23\n");
}

static inline void
reject_empty_initializer_before_c23(void)
{
	if (!tcc_lang_at_least(LANG_C23) && lexer_peek()->kind == TOK_RBRACE)
		fatal_cur("empty initializer is not allowed before C23\n");
}

static inline int
token_is_static_assert_keyword(const Token *token)
{
	return token &&
	       token->kind == TOK_IDENT &&
	       token->text &&
	       (STRCMP(token->text, "_Static_assert") == 0 ||
	        STRCMP(token->text, "static_assert") == 0);
}

static inline int
token_is_c23_bool_keyword(const Token *token)
{
	return token &&
	       token->kind == TOK_IDENT &&
	       token->text &&
	       STRCMP(token->text, "bool") == 0;
}

static inline int
token_is_c23_true_keyword(const Token *token)
{
	return token &&
	       token->kind == TOK_IDENT &&
	       token->text &&
	       STRCMP(token->text, "true") == 0;
}

static inline int
token_is_c23_false_keyword(const Token *token)
{
	return token &&
	       token->kind == TOK_IDENT &&
	       token->text &&
	       STRCMP(token->text, "false") == 0;
}

static inline int
token_is_c23_nullptr_keyword(const Token *token)
{
	return token &&
	       token->kind == TOK_IDENT &&
	       token->text &&
	       STRCMP(token->text, "nullptr") == 0;
}

static inline int
token_is_alignas_keyword(const Token *token)
{
	return token &&
	       token->kind == TOK_IDENT &&
	       token->text &&
	       (STRCMP(token->text, "_Alignas") == 0 ||
	        STRCMP(token->text, "alignas") == 0);
}

static inline int
token_is_c23_alignof_keyword(const Token *token)
{
	return token &&
	       token->kind == TOK_IDENT &&
	       token->text &&
	       STRCMP(token->text, "alignof") == 0;
}

static inline int
token_is_c23_nullptr_t_keyword(const Token *token)
{
	return token &&
	       token->kind == TOK_IDENT &&
	       token->text &&
	       STRCMP(token->text, "nullptr_t") == 0;
}

static inline int
token_is_c23_thread_local_keyword(const Token *token)
{
	return token &&
	       token->kind == TOK_IDENT &&
	       token->text &&
	       STRCMP(token->text, "thread_local") == 0;
}

static inline int
token_is_thread_local_storage_specifier(const Token *token)
{
	return (token && token->kind == TOK_THREAD_LOCAL) ||
	       token_is_c23_thread_local_keyword(token);
}

static inline int
token_starts_plain_thread_local_storage_specifier(const Token *token,
                                                  const Token *next,
                                                  const Token *next2)
{
	if (!token_is_c23_thread_local_keyword(token))
		return 0;
	if (!next)
		return 0;

	switch (next->kind) {
	case TOK_IDENT:
	case TOK_STAR:
	case TOK_STATIC:
	case TOK_EXTERN:
	case TOK_AUTO:
	case TOK_REGISTER:
	case TOK_TYPEDEF:
	case TOK_INLINE:
	case TOK_NORETURN:
	case TOK_CONST:
	case TOK_VOLATILE:
	case TOK_RESTRICT:
	case TOK_ATOMIC:
	case TOK_VOID:
	case TOK_CHAR:
	case TOK_SHORT:
	case TOK_INT:
	case TOK_LONG:
	case TOK_FLOAT:
	case TOK_DOUBLE:
	case TOK_SIGNED:
	case TOK_UNSIGNED:
	case TOK_STRUCT:
	case TOK_UNION:
	case TOK_ENUM:
		return 1;
	case TOK_LPAREN:
		return next2 && next2->kind == TOK_STAR;
	default:
		return 0;
	}
}

static inline void
reject_plain_bool_keyword_before_c23(const Token *token)
{
	if (!tcc_lang_at_least(LANG_C23) &&
	    token_is_c23_bool_keyword(token))
		fatal_token(token, "bool is not allowed before C23\n");
}

static inline int
token_starts_plain_bool_type_specifier(const Token *token,
                                       const Token *next,
                                       const Token *next2)
{
	if (!token_is_c23_bool_keyword(token))
		return 0;
	if (!next)
		return 0;

	switch (next->kind) {
	case TOK_IDENT:
	case TOK_STAR:
	case TOK_CONST:
	case TOK_VOLATILE:
	case TOK_RESTRICT:
	case TOK_ATOMIC:
	case TOK_STATIC:
	case TOK_EXTERN:
	case TOK_AUTO:
	case TOK_REGISTER:
	case TOK_TYPEDEF:
	case TOK_INLINE:
	case TOK_NORETURN:
	case TOK_LBRACKET:
	case TOK_SEMI:
	case TOK_ASSIGN:
	case TOK_COMMA:
	case TOK_RPAREN:
		return 1;
	case TOK_LPAREN:
		return next2 && next2->kind == TOK_STAR;
	default:
		return 0;
	}
}

static inline int
token_is_reserved_identifier_in_c23(const Token *token)
{
	if (!tcc_lang_at_least(LANG_C23) ||
	    !token ||
	    token->kind != TOK_IDENT ||
	    !token->text)
		return 0;

	return token_is_static_assert_keyword(token) ||
	       token_is_c23_bool_keyword(token) ||
	       token_is_c23_true_keyword(token) ||
	       token_is_c23_false_keyword(token) ||
	       token_is_c23_nullptr_keyword(token) ||
	       token_is_c23_nullptr_t_keyword(token) ||
	       token_is_alignas_keyword(token) ||
	       token_is_c23_alignof_keyword(token) ||
	       token_is_c23_thread_local_keyword(token);
}

static inline int
identifier_text_is_reserved_in_c23(const char *text)
{
	if (!tcc_lang_at_least(LANG_C23) || !text)
		return 0;

	return STRCMP(text, "bool") == 0 ||
	       STRCMP(text, "true") == 0 ||
	       STRCMP(text, "false") == 0 ||
	       STRCMP(text, "nullptr") == 0 ||
	       STRCMP(text, "nullptr_t") == 0 ||
	       STRCMP(text, "static_assert") == 0 ||
	       STRCMP(text, "_Static_assert") == 0 ||
	       STRCMP(text, "alignas") == 0 ||
	       STRCMP(text, "_Alignas") == 0 ||
	       STRCMP(text, "alignof") == 0 ||
	       STRCMP(text, "thread_local") == 0;
}

static inline const char *
token_reserved_identifier_name_in_c23(const Token *token)
{
	if (!tcc_lang_at_least(LANG_C23) || !token)
		return NULL;

	if (token->kind == TOK_IDENT && identifier_text_is_reserved_in_c23(token->text))
		return token->text;

	if (token->kind == TOK_ALIGNOF)
		return "alignof";

	return NULL;
}

static inline void
parser_reject_reserved_decl_identifier_name(const char *name, const char *what)
{
	if (identifier_text_is_reserved_in_c23(name)) {
		fatal_cur("'%s' is a reserved identifier in C23 and cannot be used as %s\n",
		          name, what);
	}
}

static inline void
parser_require_decl_identifier(const Token *token, const char *what)
{
	const char *reserved_name = token_reserved_identifier_name_in_c23(token);

	if (reserved_name) {
		fatal_token(token,
		            "'%s' is a reserved identifier in C23 and cannot be used as %s\n",
		            reserved_name, what);
	}

	if (!token || token->kind != TOK_IDENT || !token->text)
		fatal_token(token, "Expected %s\n", what);

	if (token_is_reserved_identifier_in_c23(token)) {
		fatal_token(token,
		            "'%s' is a reserved identifier in C23 and cannot be used as %s\n",
		            token->text, what);
	}
}

static inline int
token_starts_alignas_specifier(const Token *token, const Token *next)
{
	return token_is_alignas_keyword(token) &&
	       next &&
	       next->kind == TOK_LPAREN;
}

static inline int
token_is_typeof_keyword(const Token *token)
{
	return token &&
	       token->kind == TOK_IDENT &&
	       token->text &&
	       (STRCMP(token->text, "__typeof__") == 0 ||
	        STRCMP(token->text, "typeof") == 0);
}


typedef struct {
	char name[64];
	int offset;
	int size;
	int is_struct;
	int is_array;
	int is_bitfield;
	int bit_offset;
	int bit_width;
	int bit_storage_size;
	int elem_size;
	char struct_name[64];
	Type *type;
	Type *cached_effective_type;
} Field;

typedef struct {
	char name[64];
	Field *fields;
	int field_count;
	int field_cap;
	int size;
	int align;
	int is_union;
	int is_complete;
	int has_flexible_array_member;
} StructDef;

typedef struct {
	char name[64];
	int offset;
	int is_array;
	int array_len;
	int align;
	int is_pointer;
	int is_function_decl;
	int is_vla;
	int is_vm_type;
	int elem_size;
	int is_struct;
	char struct_name[64];
	char vla_bound_name[64];
	char vla_stack_name[64];
	int vla_stack_offset;
	Type *type;
	Type *vla_elem_type;
	int struct_by_ref;
	int is_static;
	int is_register;
	char static_global_name[64];
} Local;

typedef struct ParamCopy {
	char dst_name[64];
	char hidden_name[64];
	char struct_name[64];
} ParamCopy;

typedef struct PendingStructParam {
	char param_name[64];
	char hidden_name[64];
	char struct_name[64];
	int param_index;
} PendingStructParam;

typedef struct {
	char name[64];
	int is_static;
	int is_noreturn;
	int has_definition;
	int returns_struct;
	int return_abi_class;
	int return_abi_reg_count;
	int struct_size;
	char struct_name[64];
	int returns_pointer;
	int return_elem_size;
	Type *return_type;
	int return_pointee_kind;
	int return_pointee_size;
	int return_pointee_is_unsigned;
	int return_pointer_depth;
	int return_pointee_source_kind;
	int return_pointee_is_union;
	char return_pointee_struct_name[64];
	char return_pointee_source_name[64];
	int has_prototype;
	int is_variadic;
	int fixed_param_count;
	Type **param_types;
	char **param_struct_names;
	int param_type_count;
} FuncInfo;

typedef struct {
	char name[64];
	Type *type;
} TypedefName;

typedef struct {
	char name[64];
	int value;
} EnumConst;

typedef struct {
	char name[64];
	int is_complete;
} EnumTag;

typedef struct VLASnapshotEntry {
	char name[64];
	int offset;
	int elem_size;
	int local_index;
} VLASnapshotEntry;

typedef struct ParserScopeMark {
	int local_count;
	int typedef_count;
	int struct_count;
	int enum_tag_count;
	int enum_const_count;
} ParserScopeMark;


/* ---------------------------------------------------------------------------
 * Internal function prototypes (parser.c → expr.c / stmt.c and vice versa)
 * --------------------------------------------------------------------------- */

/* --- table helpers (parser.c) --- */
int         parser_struct_count(void);
StructDef  *parser_struct_at(int index);
int         parser_has_struct_capacity(void);
StructDef  *structs_push(void);

/* --- lookup / query (parser.c) --- */
int         parser_find_enum_const(const char *name, int *out_value);
void        parser_add_enum_const(const char *name, int value);
int         parser_trace_toplevel_enabled(void);
int         parser_has_visible_enum_tag(const char *name);
int         parser_enum_tag_is_complete(const char *name);
void        parser_declare_enum_tag(const char *name);
void        parser_define_enum_tag(const char *name);
int         parser_alloc_string_label(void);
extern int  parser_anon_struct_id;
int         parser_current_local_count(void);
const char *parser_current_function_name(void);
Type       *parser_current_function_return_type(void);
int         parser_try_consume_pragma_pack(void);
int         parser_apply_pack_alignment(int align);
void        parser_override_local_type(const char *name, int offset,
                                       Type *type, int elem_size);
Type       *clone_type(Type *src);
Type       *parser_make_function_type(Type *ret_type, Type **param_types,
                                      int param_count, int is_variadic,
                                      int fixed_param_count);
Type       *parser_find_typedef(const char *name);
int         parser_is_typedef_name(const char *name);
void        parser_add_typedef_name(const char *name, Type *type);
int         find_local(const char *name);
Local      *parser_find_local_info_latest(const char *name);
Global     *parser_find_global_info(const char *name);
void        parser_mark_local_scope(ParserScopeMark *mark);
void        parser_restore_local_scope_keep_statics(const ParserScopeMark *mark);
int         parser_has_vla_since_local_count(int saved_local_count);
int         parser_max_active_vla_local_index(void);
void        parser_configure_last_local_scalar(int elem_size, int is_unsigned);
void        parser_configure_last_local_type(Type *type);
int         is_struct_local(const char *name);
int         is_register_local(const char *name);
int         is_global_struct(const char *name);
const char *struct_name_local(const char *name);
StructDef  *find_struct(const char *name);
StructDef  *find_struct_or_null(const char *name);
FuncInfo   *find_func(const char *name);
Field      *find_field(const char *struct_name, const char *field_name);
int         is_global(const char *name);
int         is_global_array(const char *name);
int         global_elem_size(const char *name);
int         is_global_unsigned(const char *name);
int         is_global_pointer(const char *name);
int         global_pointer_elem_size(const char *name);
Type       *global_type(const char *name);
const char *parser_resolve_struct_type_name(Type *type);
int         is_struct_assign_node(Node *node);

/* --- type construction (parser.c) --- */
Type       *type_for_size(int size);
Type       *type_for_size_unsigned(int size, int is_unsigned);
Type       *build_array_type_from_dims_allow_incomplete(Type *base_type,
                                                        int *dims,
                                                        int dim_count,
                                                        int allow_unsized_first);
Type       *parser_canonicalize_pointer_type(Type *type, int elem_size, const char *struct_name);
Type       *parser_canonicalize_decl_type(Type *type);
Type       *type_local(const char *name);
int         type_elem_size(Type *type);
Type       *global_array_remaining_ptr_type(Global *g, int consumed_dims, int *out_elem_size);
Type       *global_array_decay_type(const char *name, int *out_elem_size);

/* --- local/global slot management (parser.c) --- */
int         add_local(const char *name);
int         add_char_local(const char *name);
int         add_struct_local(const char *name, const char *struct_name);
int         add_struct_pointer_local(const char *name, const char *struct_name);
int         add_struct_pointer_local_depth(const char *name, const char *struct_name, int pointer_depth);
int         add_typed_local(const char *name, Type *type);
void        parser_mark_local_vla(const char *name, const char *bound_name,
                                  const char *stack_name, int stack_offset,
                                  Type *elem_type, int elem_size);
void        parser_mark_local_vm_type(const char *name, const char *bound_name,
                                      Type *elem_type, int elem_size);
int         add_struct_byref_param_local(const char *name, const char *struct_name);
Global     *new_global_object(const char *name, int elem_size);
Global     *find_global(const char *name);
Global     *new_global_slot(const char *name);
int         parser_global_index(Global *g);
Global     *parser_global_at(int index);
int         parser_global_count(void);
void        parser_commit_reserved_global(void);
void        commit_global_definition(Global *g);
void        apply_type_to_global(Global *g, Type *type);
void        parser_declare_extern_object(const char *name, Type *type);
void        parser_declare_function(const char *name, Type *ret_type,
                                    int has_prototype, Type **param_types,
                                    int param_count, int is_variadic,
                                    int fixed_param_count, int is_noreturn);
int         parse_alignment_specifiers(void);
void        parser_validate_decl_alignment(int requested_align, Type *type);
void        parser_set_decl_align_request(int align);
void        parser_clear_decl_align_request(void);
void        parser_set_decl_register_request(int is_register);
void        parser_clear_decl_register_request(void);
void        parser_clear_trailing_decl_specifier_flags(void);
void        parser_set_trailing_decl_specifier_tracking(int enabled);
int         parser_type_name_saw_trailing_function_specifier(void);
int         parser_type_name_saw_trailing_inline_specifier(void);
int         parser_type_name_saw_trailing_noreturn_specifier(void);
int         parser_type_name_saw_trailing_storage_class(void);
TokenKind   parser_type_name_trailing_storage_class(void);
int         parser_type_name_saw_thread_local_storage_specifier(void);
int         parser_type_name_saw_multiple_trailing_storage_classes(void);
void        parser_set_pending_decl_noreturn(int enabled);
int         parser_consume_pending_decl_noreturn(void);
int         parse_typedef_declaration_after_base_type(Type *type);
int         struct_alignof_name(const char *name);
Node *      parser_collect_vla_scope_cleanup(int saved_local_count);
int         parser_snapshot_active_vlas(VLASnapshotEntry **out_entries);
Node *      parser_make_vla_restore_call(const char *stack_name, int stack_offset);

/* --- expression utilities (parser.c / expr.c) --- */
void        expect(TokenKind kind);
Node       *append_node(Node *head, Node *node);
Node       *make_scalar_var_node(const char *name);
Node       *make_scalar_var_node_resolved(const char *name, Local *local, Global *global);
Node       *parser_make_function_designator(const char *name);
Node       *clone_node_tree(Node *node);
Node       *clone_lvalue_for_read(Node *n);
Node       *make_incdec(Node *node, TokenKind op, int is_postfix);
int         aggregate_align(StructDef *def);
void        skip_pointer_qualifiers(void);
int         consume_type_qualifiers(void);
int         try_parse_null_pointer_constant(void);
Node       *parse_arg_list(FuncInfo *callee_info);
Node       *parse_arg_list_for_type(Type *func_type, const char *callee_name);
void        parse_prototype_param_list(Type ***out_types, int *out_count,
                                       int *is_variadic, int *fixed_params,
                                       int *out_has_prototype,
                                       int allow_oldstyle_empty);
Node       *make_struct_scalar_member(Node *base, Field *field, int offset_base);
Node       *append_struct_field_comparisons(Node *head, Node *lhs, Node *rhs, const char *sname, int invert);
Node       *build_struct_equality_expr(Node *left, Node *right, int invert);
int         is_assignable(Node *node);
int         sizeof_identifier(const char *name);
int         sizeof_array_elem_identifier(const char *name);
int         alignof_identifier(const char *name);
int         sizeof_node(Node *node);
Node       *scale_index_to_bytes(Node *idx, int stride);
Node       *append_byte_index(Node *acc, Node *idx, int stride);
int         global_array_stride_bytes_for_dim(Global *g, int dim);
Node       *expr_value_statement(Node *setup, Node *stmt);
int         expr_is_unsigned_for_compare(Node *node);

/* --- struct helpers (parser.c) --- */
int         field_index_by_name_offset(StructDef *def, Field *field);
Node       *parse_struct_initializer_block(const char *var_name, const char *struct_name, int base_offset, Node *decl_node);
Node       *append_struct_copy_from_ptr_fields(Node *head, const char *dst_name, int dst_base, Node *src_base, const char *struct_name, int offset_base);
Node       *append_struct_copy_from_ptr_fields_at(Node *head, int dst_base, Node *src_base, const char *struct_name, int src_base_offset);
Node       *build_struct_param_copy(const char *dst_name, const char *hidden_name, const char *struct_name);
int         try_parse_global_struct_compound_initializer(int g_idx, StructDef *def, const char *expected_struct_name, int base_offset);
void        parse_global_struct_initializer_body_ex(int g_idx, StructDef *def, int base_offset, int allow_unbraced_end);
void        parse_global_struct_initializer_body(int g_idx, StructDef *def, int base_offset);
int         parse_static_assert_declaration(void);
extern int  parser_profile_enabled_flag;
void        parser_profile_scope_enter_slow(ParserProfileBucket bucket);
void        parser_profile_scope_leave_slow(ParserProfileBucket bucket);

static inline void
parser_profile_scope_enter(ParserProfileBucket bucket)
{
	if (__builtin_expect(!parser_profile_enabled_flag, 1))
		return;
	parser_profile_scope_enter_slow(bucket);
}

static inline void
parser_profile_scope_leave(ParserProfileBucket bucket)
{
	if (__builtin_expect(!parser_profile_enabled_flag, 1))
		return;
	parser_profile_scope_leave_slow(bucket);
}

/* --- expr.c entry points --- */
Node       *parse_statement_expression(void);
Node       *parse_factor(void);
Node       *parse_postfix(void);
int         parse_sizeof_type_or_expr(void);
int         parse_alignof_type_or_expr(const Token *op_token);
Node       *parse_unary(void);
Node       *parse_term(void);
Node       *parse_additive(void);
Node       *parse_shift(void);
Node       *parse_relational(void);
Node       *parse_equality(void);
Node       *parse_bitand(void);
Node       *parse_bitxor(void);
Node       *parse_bitor(void);
Node       *parse_logical_and(void);
Node       *parse_logical_or(void);
Node       *parse_conditional(void);
Node       *parse_assignment(void);
Node       *parse_comma_expr(void);
Node       *parse_expr(void);

/* --- stmt.c entry points --- */
Node       *parse_block_contents(void);
Node       *parse_statement(void);
Node       *parse_switch_statement(void);
int         parse_typedef_declaration(void);
int         is_type_start_token(TokenKind kind, const char *text);
Type       *parse_type_name(void);
Type       *parse_enum_specifier(void);
Type       *parser_find_typedef_linear(const char *name);
void        parse_struct_definition(void);
Node       *parse_struct_compound_assignment_statement(void);
Node       *parse_struct_compound_literal_member_expr(void);
Node       *parse_struct_return_assignment_statement(void);
Node       *parse_struct_return_discard_statement(void);
Node       *parse_call_statement_with_compound_literal_arg(void);
Node       *parse_return_call_with_struct_return_arg(void);
Node       *parse_return_struct_call_value(void);
Node       *parse_v124_call_statement_with_temps(void);
Node       *parse_if_with_struct_assign_member_condition(void);
Node       *parse_call_statement_with_struct_assign_member_arg(void);

/* --- misc (parser.c) --- */
int         consume_all_zero_initializer(void);
Node       *parse_local_scalar_initializer_expr(int target_size);
int         parse_array_dimensions(int dims[MAX_ARRAY_DIMS], int allow_unsized_first,
                                   int allow_parameter_qualifiers);
int         parser_array_bound_contains_nonconstant_identifier(void);
long long   parser_eval_const_int_expr(void);
long long   parser_eval_const_int_expr_checked(int *is_constant);
Type       *build_array_type_from_dims(Type *base_type, int *dims, int dim_count);
void        parse_union_definition(void);
int         try_parse_abstract_function_pointer_declarator(Type **ptype);

/* --- stmt.c internal helpers (called from parser.c) --- */
Node       *append_local_zero_fill(Node *head, const char *name, int offset, int bytes);
Node       *build_local_zero_fill_block(const char *name, int offset, int bytes, Node *decl);
Node       *make_local_array_store(const char *name, int offset, int elem_size, int index, int value);
void        stmt_begin_function(void);
void        stmt_resolve_function_gotos(void);

/* --- parser.c internal helpers called from expr.c --- */
int         is_pointer_local_optional(const char *name);
int         add_local(const char *name);
int         add_char_local(const char *name);
Node       *parse_struct_assign_member_expr_core(Node **setup_out);
Node       *parse_v124_return_call_with_temps(void);
Node       *parse_return_struct_assign_member_expr(void);
Node       *parse_struct_assign_member_sum(Node **setup_out);

/* --- additional parser.c helpers used by expr.c --- */
const char *global_struct_name(const char *name);
const char *struct_name_local_optional(const char *name);

/* --- additional parser.c helpers (undefined reference fixes) --- */
void         apply_field_type(Node *node, Field *field);
int          elem_size_local(const char *name);
Type *       type_local_optional(const char *name);
int          is_array_local(const char *name);
int          is_vla_local(const char *name);
int          is_vm_local(const char *name);
const char * vla_bound_name_local(const char *name);
Type *       vla_elem_type_local(const char *name);
int          is_static_local(const char *name);
int          is_static_array_local(const char *name);
int          is_struct_by_ref_local(const char *name);
Node *       make_var_node(const char *name);
const char * static_global_name_local(const char *name);
int          static_local_elem_size(const char *name);
void         skip_inline_qualifiers(void);
int          add_local_sized(const char *name, int slots, int is_array);
int          add_decl_typed_local(int requested_align, const char *name, Type *type);
int          add_pointer_local(const char *name, int elem_size);
int          add_static_local(const char *name, const char *global_name, Type *type, int elem_size, int is_array, int array_len, int align);
int          add_typed_array_local(const char *name, Type *base_type, int len);
StructDef *  get_or_add_forward_struct(const char *name);
int          try_parse_prototype(void);
int          v124_call_starts_with_struct_temp_arg(void);
Node *       parse_asm_statement(void);
void         globals_ensure_spare(int spare);
void         parse_struct_body_into(StructDef *def);
Node *       parse_struct_array_initializer_block(const char *var_name, const char *struct_name, int base_offset, int array_len, Node *decl_node);
Node *       parse_struct_initializer_values(StructDef *def, const char *struct_name, int base_offset, Node *head);
void         parser_expect_local_aggregate_initializer_close(StructDef *def);
long long    parse_global_scalar_initializer_value_or_die(const char *message);
void         parse_pointer_global_initializer(Global *g, const char *message,
                    int require_known_global, int allow_string, int allow_bare_ident);
int          global_init_count(const Global *g);
void         global_set_init_count(Global *g, int count);
void         global_set_init_byte(Global *g, int idx, long long value);
void         global_set_init_sym(Global *g, int slot, const char *sym);
void         set_global_integer_initializer(Global *g, long long value);
void         parser_set_global_address_initializer(Global *g, const char *name);
void         set_global_string_initializer(Global *g, const char *value);
void         set_global_string_array_initializer(Global *g, const char *value);

#endif /* PARSER_INTERNAL_H */
