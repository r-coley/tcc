/*
 * stmt.c — extracted from parser.c
 */
#include "parser_internal.h"

#define STMT_SCOPE_STACK_MAX 256
#define STMT_VLA_SNAPSHOT_MAX 64
#define STMT_JUMP_TABLE_MAX 256
#define STMT_PARSE_DEPTH_LIMIT 1024

static int stmt_block_scope_stack[STMT_SCOPE_STACK_MAX];
static int stmt_block_scope_depth;
static int stmt_control_scope_stack[STMT_SCOPE_STACK_MAX];
static int stmt_control_is_loop_stack[STMT_SCOPE_STACK_MAX];
static int stmt_control_scope_depth;
static int stmt_parse_depth;
static int stmt_use_function_scope_for_next_block;

static Node *make_local_array_assign_expr(const char *name, int offset, Type *elem_type, int index, Node *expr);
static int stmt_try_parse_local_array_designator(int *out_lo, int *out_hi);
static void stmt_local_array_init_reserve(int needed, int *cap, int **values, Node ***exprs, unsigned char **seen);
static void stmt_copy_local_array_init_span(int pointer_elements,
	int dst_base_index, int span_len,
	int *dst_values_cap, int **dst_values,
	int *dst_exprs_cap, Node ***dst_exprs,
	unsigned char **dst_seen,
	const int *src_values, int src_values_cap,
	Node **src_exprs, int src_exprs_cap,
	const unsigned char *src_seen, int src_seen_cap,
	int *max_init_index, int *init_count);
static void stmt_parse_braced_local_array_initializer(Type *elem_type, int pointer_elements,
	int array_len, int allow_enum_constants, int reject_multidim_designators,
	int *next_init_index, int *max_init_index, int *init_count,
	int *init_values_cap, int **init_values, int *init_exprs_cap,
	Node ***init_exprs, unsigned char **init_seen);
static void stmt_parse_local_multidim_array_initializer(Type *array_type,
	int pointer_elements, int allow_enum_constants, int base_index,
	int *init_values_cap, int **init_values, int *init_exprs_cap,
	Node ***init_exprs, unsigned char **init_seen, int *max_init_index,
	int *init_count);
static int stmt_string_literal_elem_width(const Token *value);
static void stmt_require_string_literal_array_match(const Token *value,
	int elem_size, const char *message);
static int return_expr_may_need_special_lowering(void);
static void validate_return_statement_form(int has_expr);
static void validate_pointer_initializer_compatibility(Type *dst_type, Node *expr);
static Node *append_stmt(Node **cur, Node *stmt);
static Node *stmt_node_list_tail(Node *node);
static void stmt_append_node_to_tail(Node **head, Node **tail, Node *node);
static Node *materialize_struct_return_call_arg(const Token *inner, Node **out_arg);
static Node *materialize_compound_literal_call_arg(Node **out_arg);
static int stmt_arm64_struct_arg_passes_by_value(Type *type);
static Node *parse_v124_call_after_name(const Token *func, int as_return);
static Node *parse_function_specifier_declaration_statement(void);
static Node *parse_static_declaration_statement(int requested_align);
static Node *parse_extern_declaration_statement(void);
static Node *stmt_try_parse_function_declaration_after_name(const char *name,
                                                            Type *ret_type,
                                                            int is_noreturn);
static Node *stmt_parse_function_declaration_after_name(const char *name,
                                                        Type *ret_type,
                                                        int is_noreturn);
static Node *parse_static_declaration_after_base_type(Type *base_type, int elem_size,
	int requested_align, int saw_function_specifier,
	int saw_noreturn_specifier);
static Node *parse_extern_declaration_after_base_type(Type *base_type);
static Type *stmt_finish_parenthesized_pointer_object_type(Type *base_type,
	int inner_dims[MAX_ARRAY_DIMS], int inner_dim_count);
static int array_decl_looks_runtime_vla(void);
static void stmt_require_c99_for_runtime_vla(void);
static Node *stmt_parse_pointer_to_runtime_vla_local(Type *array_base_type,
	int requested_align, const char *name);
static Node *stmt_try_parse_local_vm_array_typedef(Type *type,
	int requested_align);
static Node *stmt_try_parse_local_vm_pointer_array_typedef(Type *type,
	int requested_align);
static Node *stmt_try_parse_array_compound_literal_pointer_initializer(
	Node *decl, const char *name, int offset, Type *decl_type);
static Node *stmt_build_initializer_assign(Node *lhs, Node *expr, Type *decl_type);
static Node *stmt_try_parse_struct_pointer_compound_literal_initializer(
	const char *var_name, int offset, Type *decl_type,
	const char *struct_name, Node *decl_node);
static Node *stmt_try_parse_function_pointer_local_after_decl_type(Type *ret_type,
	int requested_align);
static int stmt_type_is_variably_modified(Type *type);

typedef struct {
	char name[64];
	int local_count;
	int max_vla_local_index;
} StmtLabelInfo;

typedef struct {
	Node *node;
	char label[64];
	int local_count;
	VLASnapshotEntry vla_entries[STMT_VLA_SNAPSHOT_MAX];
	int vla_entry_count;
} StmtGotoInfo;

typedef struct {
	int *case_values;
	int case_count;
	int case_cap;
	int has_default;
} SwitchLabelSet;

static StmtLabelInfo stmt_labels[STMT_JUMP_TABLE_MAX];
static int stmt_label_count;
static StmtGotoInfo stmt_gotos[STMT_JUMP_TABLE_MAX];
static int stmt_goto_count;
static int stmt_decl_register_request;
static int stmt_switch_depth;
static int stmt_track_trailing_decl_specifiers;
static int stmt_saw_trailing_function_specifier;
static int stmt_saw_trailing_inline_specifier;
static int stmt_saw_trailing_noreturn_specifier;
static TokenKind stmt_trailing_storage_class;
static int stmt_saw_thread_local_storage_specifier;
static int stmt_saw_multiple_trailing_storage_classes;
static Node *stmt_last_typedef_decl_node;

#define STMT_ENUM_INT_MIN (-2147483647LL - 1LL)
#define STMT_ENUM_INT_MAX 2147483647LL

static void
stmt_validate_enum_value(long long value)
{
	if (value < STMT_ENUM_INT_MIN || value > STMT_ENUM_INT_MAX)
		fatal_cur("enumerator value is not representable as int\n");
}

static int
stmt_merge_requested_alignment(int requested_align)
{
	int post_align = parse_alignment_specifiers();

	return post_align > requested_align ? post_align : requested_align;
}

static int
stmt_typedef_is_file_scope(void)
{
	return pfunc.function_name[0] == '\0';
}

static void
stmt_reject_file_scope_vm_typedef_array_bound(void)
{
	if (!stmt_typedef_is_file_scope())
		return;
	if (lexer_peek()->kind == TOK_LBRACKET &&
	    lexer_peek_ahead(1)->kind != TOK_RBRACKET &&
	    (lexer_peek_ahead(1)->kind == TOK_STAR ||
	     parser_array_bound_contains_nonconstant_identifier()))
		fatal_cur("file-scope typedef array bound must be an integer constant expression\n");
}

static void
switch_label_set_free(SwitchLabelSet *set)
{
	if (!set)
		return;
	xfree(set->case_values);
	memset(set, 0, sizeof(*set));
}

static void
switch_label_set_add_case(SwitchLabelSet *set, int value)
{
	if (!set)
		return;
	for (int i = 0; i < set->case_count; i++) {
		if (set->case_values[i] == value)
			fatal_cur("duplicate case value in switch\n");
	}
	if (set->case_count >= set->case_cap) {
		int new_cap = set->case_cap ? set->case_cap * 2 : 8;
		set->case_values = xrealloc(set->case_values,
		                            sizeof(int) * (size_t)new_cap);
		set->case_cap = new_cap;
	}
	set->case_values[set->case_count++] = value;
}

static void
switch_label_set_add_default(SwitchLabelSet *set)
{
	if (!set)
		return;
	if (set->has_default)
		fatal_cur("multiple default labels in switch\n");
	set->has_default = 1;
}

static void
stmt_skip_type_name_noise(void)
{
	for (;;) {
		const Token *t = lexer_peek();
		int plain_thread_local_storage =
		    token_starts_plain_thread_local_storage_specifier(
		        t, lexer_peek_ahead(1), lexer_peek_ahead(2));

		if (t->kind == TOK_INLINE || t->kind == TOK_NORETURN) {
			if (stmt_track_trailing_decl_specifiers) {
				stmt_saw_trailing_function_specifier = 1;
				if (t->kind == TOK_INLINE)
					stmt_saw_trailing_inline_specifier = 1;
			}
			if (t->kind == TOK_NORETURN)
				stmt_saw_trailing_noreturn_specifier = 1;
			if (t->kind == TOK_INLINE && tcc_lang_is_c89_or_c90())
				fatal_cur("inline is not allowed in C89/C90 mode\n");
			reject_c89_c99_keyword_token(t->kind);
			lexer_next();
			continue;
		}

		if (t->kind == TOK_STATIC || t->kind == TOK_EXTERN ||
		    t->kind == TOK_AUTO || t->kind == TOK_REGISTER ||
		    t->kind == TOK_THREAD_LOCAL || plain_thread_local_storage ||
		    t->kind == TOK_TYPEDEF) {
			if (stmt_track_trailing_decl_specifiers) {
				if (t->kind == TOK_THREAD_LOCAL || plain_thread_local_storage) {
					if (stmt_saw_thread_local_storage_specifier ||
					    stmt_trailing_storage_class != TOK_EOF)
						stmt_saw_multiple_trailing_storage_classes = 1;
					stmt_saw_thread_local_storage_specifier = 1;
					if (stmt_trailing_storage_class == TOK_EOF)
						stmt_trailing_storage_class = TOK_THREAD_LOCAL;
				} else {
					if (stmt_trailing_storage_class != TOK_EOF ||
					    stmt_saw_thread_local_storage_specifier)
						stmt_saw_multiple_trailing_storage_classes = 1;
					if (stmt_trailing_storage_class == TOK_EOF)
						stmt_trailing_storage_class = t->kind;
				}
			}
			if (plain_thread_local_storage)
				reject_plain_thread_local_keyword_before_c23(t);
			if (t->kind == TOK_THREAD_LOCAL) {
				reject_thread_local_storage_specifier();
			}
			reject_c89_c99_keyword_token(t->kind);
			lexer_next();
			continue;
		}

		if (t->kind == TOK_IDENT && t->text && STRCMP(t->text, "__attribute__") == 0) {
			/* Safe here because the current token is not a type qualifier. */
			skip_inline_qualifiers();
			continue;
		}

		break;
	}
}

static void
stmt_reject_block_scope_static_function_declaration(void)
{
	fatal_cur("static storage class is not allowed on block-scope function declarations\n");
}

static void
stmt_expect_decl_semi(void)
{
	if (lexer_peek()->kind == TOK_IDENT)
		fatal_cur("Expected ';' after declaration\n");
	expect(TOK_SEMI);
}

void
parser_clear_trailing_decl_specifier_flags(void)
{
	stmt_saw_trailing_function_specifier = 0;
	stmt_saw_trailing_inline_specifier = 0;
	stmt_saw_trailing_noreturn_specifier = 0;
	stmt_trailing_storage_class = TOK_EOF;
	stmt_saw_thread_local_storage_specifier = 0;
	stmt_saw_multiple_trailing_storage_classes = 0;
}

void
parser_set_trailing_decl_specifier_tracking(int enabled)
{
	stmt_track_trailing_decl_specifiers = enabled ? 1 : 0;
}

int
parser_type_name_saw_trailing_function_specifier(void)
{
	return stmt_saw_trailing_function_specifier;
}

int
parser_type_name_saw_trailing_inline_specifier(void)
{
	return stmt_saw_trailing_inline_specifier;
}

int
parser_type_name_saw_trailing_noreturn_specifier(void)
{
	return stmt_saw_trailing_noreturn_specifier;
}

int
parser_type_name_saw_trailing_storage_class(void)
{
	return stmt_trailing_storage_class != TOK_EOF;
}

TokenKind
parser_type_name_trailing_storage_class(void)
{
	return stmt_trailing_storage_class;
}

int
parser_type_name_saw_thread_local_storage_specifier(void)
{
	return stmt_saw_thread_local_storage_specifier;
}

int
parser_type_name_saw_multiple_trailing_storage_classes(void)
{
	return stmt_saw_multiple_trailing_storage_classes;
}

static int
stmt_type_suffix_needs_scan(const Token *t)
{
	if (!t)
		return 0;

	if (t->kind == TOK_STAR ||
	    t->kind == TOK_CONST ||
	    t->kind == TOK_VOLATILE ||
	    t->kind == TOK_RESTRICT ||
	    t->kind == TOK_ATOMIC ||
	    t->kind == TOK_INLINE ||
	    t->kind == TOK_NORETURN ||
	    t->kind == TOK_AUTO ||
	    t->kind == TOK_REGISTER ||
	    t->kind == TOK_STATIC ||
	    t->kind == TOK_EXTERN ||
	    t->kind == TOK_THREAD_LOCAL ||
	    t->kind == TOK_TYPEDEF)
		return 1;

	if (token_starts_plain_thread_local_storage_specifier(t,
	                                                     lexer_peek_ahead(1),
	                                                     lexer_peek_ahead(2)))
		return 1;

	if (t->kind != TOK_IDENT || !t->text)
		return 0;

	return STRCMP(t->text, "__attribute__") == 0 ||
	       STRCMP(t->text, "volatile") == 0 ||
	       STRCMP(t->text, "__volatile__") == 0 ||
	       STRCMP(t->text, "__volatile") == 0 ||
	       STRCMP(t->text, "__restrict") == 0 ||
	       STRCMP(t->text, "__restrict__") == 0;
}

static Type *
stmt_apply_post_base_qualifiers(Type *type)
{
	int qualifiers = 0;

	parser_clear_trailing_decl_specifier_flags();
	parser_set_trailing_decl_specifier_tracking(1);
	qualifiers |= consume_type_qualifiers();
	stmt_skip_type_name_noise();
	qualifiers |= consume_type_qualifiers();
	parser_set_trailing_decl_specifier_tracking(0);

	return qualifiers ? type_with_qualifiers(type, qualifiers) : type;
}

static Type *
stmt_apply_type_source(Type *type, int source_kind, const char *source_name)
{
	const char *current_name;

	if (!type || source_kind == TYPE_SOURCE_DEFAULT)
		return type;

	current_name = type_source_name(type);
	if (type_source_kind(type) == source_kind &&
	    STRCMP(current_name ? current_name : "", source_name ? source_name : "") == 0)
		return type;

	return type_with_source(type, source_kind, source_name);
}

static Type *
stmt_apply_post_base_complex_specifier(Type *type, int *complex_source_kind,
                                       int long_count)
{
	const Token *token = lexer_peek();
	char source_name[64] = {0};
	int source_kind;

	if (!type || !complex_source_kind || *complex_source_kind != TYPE_SOURCE_DEFAULT)
		return type;
	if (!tcc_lang_at_least(LANG_C99) ||
	    token->kind != TOK_IDENT || !token->text ||
	    (STRCMP(token->text, "_Complex") != 0 &&
	     STRCMP(token->text, "_Imaginary") != 0))
		return type;
	if (!(type->kind == TY_FLOAT || type->kind == TY_DOUBLE))
		fatal_cur("invalid type specifier combination\n");

	source_kind = (STRCMP(token->text, "_Complex") == 0)
	                ? TYPE_SOURCE_COMPLEX
	                : TYPE_SOURCE_IMAGINARY;
	*complex_source_kind = source_kind;
	if (type->kind == TY_FLOAT) {
		STRNCPY(source_name,
		        source_kind == TYPE_SOURCE_COMPLEX ? "_Complex float"
		                                           : "_Imaginary float",
		        sizeof(source_name) - 1);
	} else {
		STRNCPY(source_name,
		        source_kind == TYPE_SOURCE_COMPLEX
		            ? (long_count == 1 ? "_Complex long double"
		                               : "_Complex double")
		            : (long_count == 1 ? "_Imaginary long double"
		                               : "_Imaginary double"),
		        sizeof(source_name) - 1);
	}

	lexer_next();
	return stmt_apply_type_source(type, source_kind, source_name);
}

static void
stmt_consume_scalar_type_modifiers(int *saw_short_type_modifier,
                                   int *saw_signed_type_modifier,
                                   int *saw_unsigned_type_modifier,
                                   int *saw_int_modifier,
                                   int *long_count,
                                   int *complex_source_kind)
{
	for (;;) {
		const Token *token = lexer_peek();

		if (token->kind == TOK_SIGNED || token->kind == TOK_UNSIGNED ||
		    token->kind == TOK_SHORT || token->kind == TOK_LONG) {
			if (tcc_lang_is_c89_or_c90() &&
			    token->kind == TOK_LONG &&
			    lexer_peek_ahead(1)->kind == TOK_LONG)
				fatal_cur("long long is not allowed in C89/C90 mode\n");
			if (token->kind == TOK_SHORT) {
				if (*saw_short_type_modifier || *long_count > 0)
					fatal_cur("invalid type specifier combination\n");
				*saw_short_type_modifier = 1;
			}
			if (token->kind == TOK_SIGNED) {
				if (*saw_signed_type_modifier || *saw_unsigned_type_modifier)
					fatal_cur("invalid type specifier combination\n");
				*saw_signed_type_modifier = 1;
			}
			if (token->kind == TOK_UNSIGNED) {
				if (*saw_unsigned_type_modifier || *saw_signed_type_modifier)
					fatal_cur("invalid type specifier combination\n");
				*saw_unsigned_type_modifier = 1;
			}
			if (token->kind == TOK_LONG) {
				if (*saw_short_type_modifier || *long_count >= 2)
					fatal_cur("invalid type specifier combination\n");
				(*long_count)++;
			}
			*saw_int_modifier = 1;
			lexer_next();
			continue;
		}

		if (tcc_lang_at_least(LANG_C99) &&
		    token->kind == TOK_IDENT &&
		    token->text &&
		    (STRCMP(token->text, "_Complex") == 0 ||
		     STRCMP(token->text, "_Imaginary") == 0)) {
			int source_kind = (STRCMP(token->text, "_Complex") == 0)
			                    ? TYPE_SOURCE_COMPLEX
			                    : TYPE_SOURCE_IMAGINARY;
			if (*complex_source_kind != TYPE_SOURCE_DEFAULT)
				fatal_cur("invalid type specifier combination\n");
			*complex_source_kind = source_kind;
			lexer_next();
			continue;
		}

		break;
	}
}

static void
stmt_reject_unsupported_special_type(const Type *type)
{
	(void)type;
}

static Type *
stmt_clone_typedef_type(Type *type, const char *typedef_name)
{
	Type *copy;

	if (!type)
		return NULL;

	switch (type->kind) {
	case TY_VOID:
	case TY_INT:
	case TY_CHAR:
	case TY_SHORT:
	case TY_FLOAT:
	case TY_DOUBLE:
	case TY_ENUM:
		if (type_source_is(type, TYPE_SOURCE_BOOL))
			return clone_type(type);
		if (type_source_is(type, TYPE_SOURCE_SCHAR))
			return clone_type(type);
		if (type_source_is(type, TYPE_SOURCE_COMPLEX))
			return type_with_source(clone_type(type), TYPE_SOURCE_COMPLEX,
			                        typedef_name && typedef_name[0]
			                            ? typedef_name
			                            : type_source_name(type));
		if (type_source_is(type, TYPE_SOURCE_IMAGINARY))
			return type_with_source(clone_type(type), TYPE_SOURCE_IMAGINARY,
			                        typedef_name && typedef_name[0]
			                            ? typedef_name
			                            : type_source_name(type));
		copy = xcalloc(1, sizeof(Type));
		*copy = *type;
		copy->source_kind = TYPE_SOURCE_TYPEDEF;
		copy->source_name[0] = '\0';
		if (typedef_name && typedef_name[0])
			STRNCPY(copy->source_name, typedef_name, sizeof(copy->source_name) - 1);
		return copy;
	default:
		break;
	}

	copy = clone_type(type);
	return type_with_source(copy, TYPE_SOURCE_TYPEDEF, typedef_name);
}

static Type *
stmt_parse_pointer_declarator_type(Type *base_type, int *out_ptr_depth)
{
	Type *type = clone_type(base_type);
	int ptr_depth = 0;
	int prev_tracking = stmt_track_trailing_decl_specifiers;

	parser_set_trailing_decl_specifier_tracking(1);
	while (lexer_peek()->kind == TOK_STAR) {
		int qualifiers = 0;

		lexer_next();
		ptr_depth++;
		type = type_ptr(type);
		qualifiers |= consume_type_qualifiers();
		stmt_skip_type_name_noise();
		qualifiers |= consume_type_qualifiers();
		if (qualifiers)
			type = type_with_qualifiers(type, qualifiers);
	}
	parser_set_trailing_decl_specifier_tracking(prev_tracking);

	if (out_ptr_depth)
		*out_ptr_depth = ptr_depth;
	return type;
}

static Type *
stmt_apply_abstract_type_suffixes(Type *type, int reject_vm_typedef_array_bound)
{
	if (lexer_peek()->kind == TOK_LBRACKET) {
		int dims[MAX_ARRAY_DIMS] = {0};
		int dim_count;

		if (reject_vm_typedef_array_bound)
			stmt_reject_file_scope_vm_typedef_array_bound();
		dim_count = parse_array_dimensions(dims, 0, 0);
		type = build_array_type_from_dims(type, dims, dim_count);
	}

	if (lexer_peek()->kind == TOK_LPAREN) {
		Type **param_types = NULL;
		int param_count = 0;
		int is_variadic = 0;
		int fixed_params = 0;
		int has_prototype = 0;

		parse_prototype_param_list(&param_types, &param_count,
		                          &is_variadic, &fixed_params,
		                          &has_prototype, 1);
		type = has_prototype
		     ? parser_make_function_type(type, param_types, param_count,
		                                 is_variadic, fixed_params)
		     : type_func(clone_type(type));
		if (lexer_peek()->kind == TOK_LBRACKET)
			fatal_cur("function return array declarators are not supported\n");
		if (lexer_peek()->kind == TOK_LPAREN)
			fatal_cur("function cannot return function type\n");
	}

	return type;
}

enum {
	STMT_COMMA_BASE_ONE_PTR_IF_PTR = 0,
	STMT_COMMA_BASE_STRIP_ALL_PTRS = 1,
};

static Type *
stmt_comma_decl_base_type(Type *decl_type, int base_mode)
{
	Type *base_type;

	if (!decl_type)
		return NULL;

	if (base_mode == STMT_COMMA_BASE_ONE_PTR_IF_PTR) {
		if (decl_type->kind == TY_PTR && decl_type->base)
			return decl_type->base;
		return decl_type;
	}

	base_type = decl_type;
	while (base_type && base_type->kind == TY_PTR)
		base_type = base_type->base;
	return base_type ? base_type : decl_type;
}

static Node *
stmt_append_comma_typed_declarators(Node *head, Type *decl_type, int requested_align,
                                    int base_mode, int pointer_decl_nodes,
                                    int copy_pointer_struct_name, int pointer_lhs_flags)
{
	Node *tail = head;

	while (tail && tail->next)
		tail = tail->next;

	while (lexer_peek()->kind == TOK_COMMA) {
		int np = 0;
		const Token *name;
		Type *base_type;
		Type *local_type;
		int offset;
		Node *decl;

		lexer_next();
		while (lexer_peek()->kind == TOK_STAR) {
			lexer_next();
			np++;
		}

		name = lexer_peek();
		if (name->kind != TOK_IDENT)
			break;
		lexer_next();

		base_type = stmt_comma_decl_base_type(decl_type, base_mode);
		local_type = clone_type(base_type);
		for (int i = 0; i < np; i++)
			local_type = type_ptr(local_type);

		if (lexer_peek()->kind == TOK_LPAREN) {
			if (!stmt_try_parse_function_declaration_after_name(
			        name->text, local_type,
			        parser_type_name_saw_trailing_noreturn_specifier()))
				fatal_cur("internal error: expected block-scope function declarator\n");
			continue;
		}

		offset = add_decl_typed_local(requested_align, name->text, local_type);
		if (pointer_decl_nodes && local_type->kind == TY_PTR) {
			decl = new_ptr_decl(name->text, offset);
			decl->is_pointer = 1;
			if (copy_pointer_struct_name && local_type->base && type_is_struct(local_type->base)) {
				STRNCPY(decl->struct_name, local_type->base->struct_name,
				        sizeof(decl->struct_name) - 1);
			}
		} else {
			decl = new_decl(name->text, offset);
		}
		decl->type = clone_type(local_type);
		decl->elem_size = type_elem_size(local_type);
		if (tail)
			tail->next = decl;
		else
			head = decl;
		tail = decl;

		if (lexer_peek()->kind == TOK_ASSIGN) {
			Node *expr;
			Node *lhs;
			Node *assign;
			int init_size;

			lexer_next();
			init_size = type_sizeof(local_type);
			expr = parse_local_scalar_initializer_expr(init_size > 0 ? init_size : 0);
			lhs = new_var(name->text, offset);
			lhs->type = clone_type(local_type);
			lhs->elem_size = type_elem_size(local_type);
			if (pointer_lhs_flags && local_type->kind == TY_PTR) {
				lhs->is_pointer = 1;
				if (copy_pointer_struct_name && local_type->base && type_is_struct(local_type->base)) {
					STRNCPY(lhs->struct_name, local_type->base->struct_name,
					        sizeof(lhs->struct_name) - 1);
				}
			}
			assign = stmt_build_initializer_assign(lhs, expr, local_type);
			tail->next = assign;
			tail = assign;
		}
	}

	return head;
}

static Node *
stmt_parse_pointer_to_runtime_vla_local(Type *array_base_type, int requested_align,
	const char *name)
{
	Type *elem_type;
	int elem_size;
	int tail_dims[MAX_ARRAY_DIMS] = {0};
	int tail_dim_count = 0;
	char bound_name[64];
	int bound_offset;
	Node *bound_decl;
	Node *bound_lhs;
	Node *bound_assign;
	Type *array_type;
	Type *ptr_type;
	int offset;
	Node *decl;
	Node *head;
	Node *lhs;
	Node *expr;
	Node *assign;

	expect(TOK_LBRACKET);
	if (lexer_peek()->kind == TOK_RBRACKET)
		fatal_cur("runtime VLA requires a bound expression\n");
	expr = parse_assignment();
	expect(TOK_RBRACKET);

	elem_type = clone_type(array_base_type);
	if (lexer_peek()->kind == TOK_LBRACKET) {
		if (array_decl_looks_runtime_vla())
			fatal_cur("only the first dimension of a runtime VLA may be variably modified\n");
		tail_dim_count = parse_array_dimensions(tail_dims, 0, 0);
		if (tail_dim_count > 0)
			elem_type = build_array_type_from_dims(elem_type, tail_dims, tail_dim_count);
	}

	elem_size = type_sizeof(elem_type);
	if (elem_size <= 0)
		fatal_cur("runtime VLA element type must be complete\n");

	snprintf(bound_name, sizeof(bound_name), "__vla_len_%d",
	         parser_alloc_compound_arg_temp_id());
	parser_set_decl_align_request(requested_align);
	bound_offset = add_local(bound_name);
	parser_clear_decl_align_request();

	bound_decl = new_decl(bound_name, bound_offset);
	bound_decl->type = type_int();
	bound_decl->elem_size = TCC_SIZEOF_INT;
	bound_decl->suppress_debug_loc = 1;

	bound_lhs = new_var(bound_name, bound_offset);
	bound_lhs->type = type_int();
	bound_lhs->elem_size = TCC_SIZEOF_INT;
	bound_lhs->suppress_debug_loc = 1;
	bound_assign = new_assign(bound_lhs, expr);
	bound_assign->suppress_debug_loc = 1;

	array_type = type_array(elem_type, 0);
	ptr_type = type_ptr(array_type);
	offset = add_decl_typed_local(requested_align, name, ptr_type);
	parser_override_local_type(name, offset, ptr_type, elem_size);
	parser_mark_local_vm_type(name, bound_name, elem_type, elem_size);

	decl = new_ptr_decl(name, offset);
	decl->type = ptr_type;
	decl->elem_size = elem_size;

	head = append_node(bound_decl, bound_assign);
	head = append_node(head, decl);

	if (lexer_peek()->kind == TOK_ASSIGN) {
		lexer_next();
		expr = parse_expr();
		expect(TOK_SEMI);

		lhs = new_var(name, offset);
		lhs->is_pointer = 1;
		lhs->elem_size = elem_size;
		lhs->type = clone_type(ptr_type);
		validate_pointer_initializer_compatibility(ptr_type, expr);
		assign = stmt_build_initializer_assign(lhs, expr, ptr_type);
		return new_block(append_node(head, assign));
	}

	expect(TOK_SEMI);
	return new_block(head);
}

static Node *
stmt_parse_pointer_to_array_local(Type *array_base_type, int requested_align)
{
	const Token *name;
	int dims[MAX_ARRAY_DIMS] = {0};
	int dim_count;
	Type *array_type;
	Type *ptr_type;
	int offset;
	Node *node;

	if (lexer_peek()->kind != TOK_LPAREN || lexer_peek_ahead(1)->kind != TOK_STAR)
		return NULL;

	lexer_next();
	lexer_next();
	skip_pointer_qualifiers();
	name = lexer_peek();
	if (name->kind != TOK_IDENT) {
		fatal_cur("Expected identifier in pointer-to-array declarator\n");
	}
	lexer_next();
	expect(TOK_RPAREN);
	if (lexer_peek()->kind != TOK_LBRACKET) {
		fatal_cur("Expected array declarator after pointer declarator\n");
	}
	if (array_decl_looks_runtime_vla()) {
		stmt_require_c99_for_runtime_vla();
		return stmt_parse_pointer_to_runtime_vla_local(array_base_type,
		                                              requested_align,
		                                              name->text);
	}

	dim_count = parse_array_dimensions(dims, 0, 0);
	array_type = build_array_type_from_dims(clone_type(array_base_type), dims, dim_count);
	ptr_type = type_ptr(array_type);
	offset = add_decl_typed_local(requested_align, name->text, ptr_type);
	node = new_ptr_decl(name->text, offset);
	node->type = ptr_type;
	node->elem_size = array_type->size;

	if (lexer_peek()->kind == TOK_ASSIGN) {
		Node *expr;
		Node *lhs;

		lexer_next();
		expr = parse_expr();
		expect(TOK_SEMI);

		lhs = new_var(name->text, offset);
		lhs->is_pointer = 1;
		lhs->elem_size = array_type->size;
		lhs->type = clone_type(ptr_type);
		validate_pointer_initializer_compatibility(ptr_type, expr);

		return new_block(append_node(node,
		                             stmt_build_initializer_assign(lhs, expr, ptr_type)));
	}

	expect(TOK_SEMI);
	return node;
}

static int
stmt_looks_like_pointer_to_array_declarator(void)
{
	int index = 0;

	if (lexer_peek_ahead(index)->kind != TOK_LPAREN ||
	    lexer_peek_ahead(index + 1)->kind != TOK_STAR)
		return 0;

	index += 2;
	while (lexer_peek_ahead(index)->kind == TOK_CONST ||
	       lexer_peek_ahead(index)->kind == TOK_VOLATILE ||
	       lexer_peek_ahead(index)->kind == TOK_RESTRICT ||
	       lexer_peek_ahead(index)->kind == TOK_ATOMIC)
		index++;

	if (lexer_peek_ahead(index)->kind != TOK_IDENT)
		return 0;
	index++;

	return lexer_peek_ahead(index)->kind == TOK_RPAREN &&
	       lexer_peek_ahead(index + 1)->kind == TOK_LBRACKET;
}

static void
stmt_apply_pointer_type_metadata(Node *node, Type *type, int copy_pointer_struct_name)
{
	if (!node || !type || type->kind != TY_PTR)
		return;

	node->is_pointer = 1;
	if (copy_pointer_struct_name && type->base && type_is_struct(type->base)) {
		STRNCPY(node->struct_name, type->base->struct_name,
		        sizeof(node->struct_name) - 1);
	}
}

static Node *
stmt_build_initializer_lhs(const char *name, int offset, Type *decl_type,
                           int copy_pointer_struct_name)
{
	Node *lhs = new_var(name, offset);

	lhs->type = clone_type(decl_type);
	lhs->elem_size = type_elem_size(decl_type);
	stmt_apply_pointer_type_metadata(lhs, decl_type, copy_pointer_struct_name);
	return lhs;
}

static Node *
stmt_build_initializer_assign(Node *lhs, Node *expr, Type *decl_type)
{
	if (expr && decl_type)
		expr = expr_coerce_value_for_type(expr, decl_type);

	if (lhs && expr && decl_type &&
	    type_is_complex(decl_type) &&
	    expr->type &&
	    type_is_complex(expr->type) &&
	    type_equal_unqualified(decl_type, expr->type) &&
	    parser_classify_aggregate_abi(decl_type, NULL) !=
	        AGGREGATE_ABI_X64_COMPLEX_FLOAT)
		return new_struct_assign(lhs, expr, type_sizeof(decl_type));

	return new_assign(lhs, expr);
}

static int
stmt_looks_like_array_compound_literal(void)
{
	int saw_lbracket = 0;
	int bracket_depth = 0;

	if (lexer_peek()->kind != TOK_LPAREN)
		return 0;

	for (int i = 1; i < 32; i++) {
		TokenKind kind = lexer_peek_ahead(i)->kind;

		if (kind == TOK_LPAREN)
			return 0;
		if (kind == TOK_LBRACKET) {
			saw_lbracket = 1;
			bracket_depth++;
			continue;
		}
		if (kind == TOK_RBRACKET && bracket_depth > 0) {
			bracket_depth--;
			continue;
		}
		if (kind == TOK_RPAREN && bracket_depth == 0)
			return saw_lbracket &&
			       lexer_peek_ahead(i + 1)->kind == TOK_LBRACE;
		if (kind == TOK_SEMI || kind == TOK_COMMA || kind == TOK_ASSIGN)
			return 0;
	}

	return 0;
}

static Node *
stmt_try_parse_array_compound_literal_pointer_initializer(
	Node *decl, const char *name, int offset, Type *decl_type)
{
	int dims[MAX_ARRAY_DIMS] = {0};
	int dim_count;
	int array_len;
	int init_values_cap = 0;
	int init_count = 0;
	int *init_values = NULL;
	Node **init_exprs = NULL;
	int init_exprs_cap = 0;
	unsigned char *init_seen = NULL;
	int next_init_index = 0;
	int max_init_index = -1;
	Type *elem_type;
	Type *compound_elem_type;
	Type *array_type;
	char temp_name[64];
	int temp_offset;
	Node *array_decl;
	Node head = {0};
	Node *cur = &head;
	Node *rhs;
	Node *lhs;
	Node *assign;

	if (!decl_type || !type_is_pointer(decl_type) ||
	    lexer_peek()->kind != TOK_LPAREN)
		return NULL;

	elem_type = type_pointee(decl_type);
	if (!elem_type || !type_is_scalar(elem_type))
		return NULL;

	if (!stmt_looks_like_array_compound_literal())
		return NULL;

	lexer_next(); /* ( */
	compound_elem_type = parse_type_name();
	if (!compound_elem_type || !type_equal_unqualified(compound_elem_type, elem_type))
		fatal_cur("Array compound literal initializer type mismatch\n");
	if (lexer_peek()->kind != TOK_LBRACKET)
		fatal_cur("Expected array declarator in array compound literal\n");
	dim_count = parse_array_dimensions(dims, 1, 0);
	if (dim_count != 1)
		fatal_cur("Only one-dimensional array compound literals are supported for now\n");
	array_len = dims[0];
	expect(TOK_RPAREN);
	if (lexer_peek()->kind != TOK_LBRACE)
		fatal_cur("Expected initializer list in array compound literal\n");

	lexer_next(); /* { */
	stmt_parse_braced_local_array_initializer(compound_elem_type,
	    type_is_pointer(compound_elem_type), array_len, 0, 0,
	    &next_init_index, &max_init_index, &init_count,
	    &init_values_cap, &init_values, &init_exprs_cap, &init_exprs,
	    &init_seen);

	if (array_len == 0)
		array_len = init_count;
	if (array_len <= 0)
		fatal_cur("Array compound literal length must be positive\n");
	if (init_count > array_len)
		fatal_cur("Too many initializers for local array\n");

	dims[0] = array_len;
	array_type = build_array_type_from_dims(clone_type(compound_elem_type), dims, 1);
	snprintf(temp_name, sizeof(temp_name), "__compound_array_%d",
	         parser_alloc_compound_arg_temp_id());
	temp_offset = add_decl_typed_local(0, temp_name, array_type);
	array_decl = new_array_decl(temp_name, temp_offset, array_len);
	array_decl->elem_size = type_elem_size(array_type);
	array_decl->type = array_type;

	append_stmt(&cur, decl);
	append_stmt(&cur, array_decl);
	for (int i = 0; i < array_len; i++) {
		if (type_is_pointer(compound_elem_type)) {
			Node *expr = (i < init_exprs_cap && init_seen && init_seen[i])
			             ? init_exprs[i] : new_num(0);
			append_stmt(&cur, make_local_array_assign_expr(temp_name, temp_offset,
			                                               compound_elem_type, i, expr));
		} else {
			int value = (i < init_values_cap && init_seen && init_seen[i])
			            ? init_values[i] : 0;
			append_stmt(&cur, make_local_array_store(temp_name, temp_offset,
			                                         type_sizeof(compound_elem_type),
			                                         i, value));
		}
	}

	rhs = new_var(temp_name, temp_offset);
	rhs->type = array_type;
	rhs->elem_size = type_elem_size(array_type);
	lhs = stmt_build_initializer_lhs(name, offset, decl_type, 0);
	validate_pointer_initializer_compatibility(decl_type, rhs);
	assign = new_assign(lhs, rhs);
	append_stmt(&cur, assign);

	xfree(init_values);
	xfree(init_exprs);
	xfree(init_seen);

	return new_block(head.next);
}

static Node *
stmt_finish_typed_decl_statement(Node *decl, const char *name, int offset, Type *decl_type,
                                 int requested_align, int comma_base_mode,
                                 int pointer_decl_nodes,
                                 int copy_pointer_struct_name, int pointer_lhs_flags)
{
	if (lexer_peek()->kind == TOK_ASSIGN) {
		Node *expr;
		Node *lhs;
			Node *block_head;

			lexer_next();
			if (decl_type && type_is_pointer(decl_type)) {
				Node *compound_init =
				    stmt_try_parse_array_compound_literal_pointer_initializer(
				        decl, name, offset, decl_type);
				if (compound_init) {
					compound_init = new_block(append_node(
					    compound_init->body,
					    stmt_append_comma_typed_declarators(NULL, decl_type,
					        requested_align, comma_base_mode,
					        pointer_decl_nodes, copy_pointer_struct_name,
					        pointer_lhs_flags)));
					stmt_expect_decl_semi();
					return compound_init;
				}
			}
			expr = parse_local_scalar_initializer_expr(type_sizeof(decl_type));
			lhs = stmt_build_initializer_lhs(name, offset, decl_type, copy_pointer_struct_name);
		validate_pointer_initializer_compatibility(decl_type, expr);
		if (pointer_lhs_flags && decl_type->kind == TY_PTR)
			lhs->is_pointer = 1;

		block_head = append_node(decl, stmt_build_initializer_assign(lhs, expr, decl_type));
		block_head = stmt_append_comma_typed_declarators(block_head, decl_type,
		                                                 requested_align, comma_base_mode,
		                                                 pointer_decl_nodes,
		                                                 copy_pointer_struct_name,
		                                                 pointer_lhs_flags);
		stmt_expect_decl_semi();
		return new_block(block_head);
	}

	if (lexer_peek()->kind == TOK_COMMA) {
		Node *block_head = stmt_append_comma_typed_declarators(decl, decl_type,
		                                                       requested_align, comma_base_mode,
		                                                       pointer_decl_nodes,
		                                                       copy_pointer_struct_name,
		                                                       pointer_lhs_flags);
		stmt_expect_decl_semi();
		return new_block(block_head);
	}

	stmt_expect_decl_semi();
	return decl;
}

static void
stmt_copy_node_location(Node *dst, const Node *src)
{
	if (!dst || !src)
		return;

	dst->filename_id = src->filename_id;
	dst->line = src->line;
	dst->column = src->column;
	dst->pp_filename_id = src->pp_filename_id;
	dst->pp_line = src->pp_line;
	dst->pp_column = src->pp_column;
}

static Type *
stmt_pointer_compat_src_type(Node *expr)
{
	Type *type;

	if (!expr)
		return NULL;

	type = expr->type;
	if (!type)
		return NULL;
	if (type_is_array(type) && type_pointee(type))
		return type_ptr(type_pointee(type));
	if (type_is_function(type))
		return type_ptr(type);
	return type;
}

static void
stmt_capture_goto_vla_snapshot(StmtGotoInfo *jump)
{
	VLASnapshotEntry *entries = NULL;
	int count;

	if (!jump)
		return;

	count = parser_snapshot_active_vlas(&entries);
	if (count > STMT_VLA_SNAPSHOT_MAX)
		ICE("too many active VLAs for goto snapshot");

	jump->vla_entry_count = count;
	for (int i = 0; i < count; i++) {
		STRNCPY(jump->vla_entries[i].name, entries[i].name,
		        sizeof(jump->vla_entries[i].name) - 1);
		jump->vla_entries[i].offset = entries[i].offset;
		jump->vla_entries[i].elem_size = entries[i].elem_size;
		jump->vla_entries[i].local_index = entries[i].local_index;
	}

	xfree(entries);
}

static void
stmt_reset_function_jumps(void)
{
	/* Per-function jump bookkeeping is reset by dropping the tables. */
	stmt_label_count = 0;
	stmt_goto_count = 0;
}

void
stmt_begin_function(void)
{
	stmt_block_scope_depth = 0;
	stmt_control_scope_depth = 0;
	stmt_use_function_scope_for_next_block = 1;
	stmt_reset_function_jumps();
}

static StmtLabelInfo *
stmt_find_label_info(const char *name)
{
	for (int i = 0; i < stmt_label_count; i++) {
		if (STRCMP(stmt_labels[i].name, name) == 0)
			return &stmt_labels[i];
	}

	return NULL;
}

static void
stmt_register_label_site(const char *name, int local_count)
{
	StmtLabelInfo *label;

	label = stmt_find_label_info(name);
	if (label) {
		fatal_cur("duplicate label: %s\n", name);
	}

	if (stmt_label_count >= STMT_JUMP_TABLE_MAX)
		ICE("too many labels in one function");

	label = &stmt_labels[stmt_label_count++];
	memset(label, 0, sizeof(*label));
	STRNCPY(label->name, name, sizeof(label->name) - 1);
	label->local_count = local_count;
	label->max_vla_local_index = parser_max_active_vla_local_index();
}

static void
stmt_register_goto_site(Node *node, const char *label, int local_count)
{
	StmtGotoInfo *jump;

	if (stmt_goto_count >= STMT_JUMP_TABLE_MAX)
		ICE("too many gotos in one function");

	jump = &stmt_gotos[stmt_goto_count++];
	memset(jump, 0, sizeof(*jump));
	jump->node = node;
	STRNCPY(jump->label, label, sizeof(jump->label) - 1);
	jump->local_count = local_count;
	stmt_capture_goto_vla_snapshot(jump);
}

static void
stmt_wrap_goto_with_cleanup(StmtGotoInfo *jump, int min_local_index)
{
	Node *head = NULL;
	Node *goto_copy;
	Node *next;

	if (!jump || !jump->node)
		return;

	for (int i = jump->vla_entry_count - 1; i >= 0; i--) {
		Node *call;

		if (jump->vla_entries[i].local_index < min_local_index)
			continue;

		call = parser_make_vla_restore_call(jump->vla_entries[i].name,
		                                    jump->vla_entries[i].offset);
		head = append_node(head, call);
	}

	if (!head)
		return;

	goto_copy = clone_node_tree(jump->node);
	next = jump->node->next;

	memset(jump->node, 0, sizeof(*jump->node));
	jump->node->kind = ND_BLOCK;
	jump->node->body = append_node(head, goto_copy);
	jump->node->next = next;
	jump->node->suppress_debug_loc = 1;
	stmt_copy_node_location(jump->node, goto_copy);
}

static void
stmt_resolve_goto(StmtGotoInfo *jump)
{
	StmtLabelInfo *label = stmt_find_label_info(jump->label);

	if (!label)
		tcc_error("Undefined label: %s\n", jump->label);

	if (jump->local_count < label->local_count &&
	    label->max_vla_local_index >= jump->local_count)
		tcc_error("goto into scope of variably modified local: %s\n", jump->label);

	if (jump->local_count > label->local_count)
		stmt_wrap_goto_with_cleanup(jump, label->local_count);
}

void
stmt_resolve_function_gotos(void)
{
	for (int i = 0; i < stmt_goto_count; i++)
		stmt_resolve_goto(&stmt_gotos[i]);

	stmt_reset_function_jumps();
}

static void
stmt_push_block_scope(int saved_local_count)
{
	int *depth = &stmt_block_scope_depth;
	int index = *depth;

	if (index >= STMT_SCOPE_STACK_MAX)
		ICE("statement block scope overflow");
	stmt_block_scope_stack[index] = saved_local_count;
	*depth = index + 1;
}

static void
stmt_pop_block_scope(void)
{
	int *depth = &stmt_block_scope_depth;

	if (*depth <= 0)
		ICE("statement block scope underflow");
	(*depth)--;
}

static void
validate_pointer_return_compatibility(Node *expr)
{
	FuncInfo *fi = find_func(parser_current_function_name());
	Type *ret_type = fi ? fi->return_type : NULL;
	Type *src_type = stmt_pointer_compat_src_type(expr);

	if (!ret_type || !expr)
		return;
	if (type_source_is_bool_spelling(ret_type))
		return;
	if (type_is_integer(ret_type) && src_type && type_is_pointer(src_type))
		fatal_cur("Incompatible pointer to integer conversion in return\n");
	if (type_is_pointer(ret_type) && !node_is_null_pointer_constant(expr) &&
	    src_type && type_is_integer(src_type))
		fatal_cur("Incompatible integer to pointer conversion in return\n");
	if (!type_is_pointer(ret_type))
		return;
	if (!node_is_null_pointer_constant(expr) &&
	    (!src_type || !type_is_pointer(src_type)))
		return;

	if (type_pointer_assignment_compatible(ret_type,
	                                       src_type,
	                                       node_is_null_pointer_constant(expr)))
		return;

	fatal_cur("Incompatible pointer types in return\n");
}

static void
validate_pointer_initializer_compatibility(Type *dst_type, Node *expr)
{
	Type *src_type = stmt_pointer_compat_src_type(expr);

	if (!dst_type || !expr)
		return;
	if (type_source_is_bool_spelling(dst_type))
		return;
	if (type_is_integer(dst_type) && src_type && type_is_pointer(src_type))
		fatal_cur("Incompatible pointer to integer conversion in initializer\n");
	if (!type_is_pointer(dst_type))
		return;
	if (!node_is_null_pointer_constant(expr) &&
	    src_type && type_is_integer(src_type))
		fatal_cur("Incompatible integer to pointer conversion in initializer\n");
	if (!node_is_null_pointer_constant(expr) &&
	    (!src_type || !type_is_pointer(src_type)))
		return;

	if (type_pointer_assignment_compatible(dst_type,
	                                       src_type,
	                                       node_is_null_pointer_constant(expr)))
		return;
	fatal_cur("Incompatible pointer types in initializer\n");
}

static Node *
stmt_canonicalize_return_expr(Node *expr)
{
	if (!expr)
		return NULL;
	if (pfunc.return_type)
		return expr_coerce_value_for_type(expr, pfunc.return_type);
	return expr;
}

static Node *
stmt_parse_function_pointer_initializer_expr(const char **out_symbol)
{
	const Token *tok = lexer_peek();
	const char *symbol = NULL;
	Node *expr;

	if (tok->kind == TOK_AMP &&
	    lexer_peek_ahead(1)->kind == TOK_IDENT &&
	    find_func(lexer_peek_ahead(1)->text)) {
		symbol = lexer_peek_ahead(1)->text;
		lexer_next();
		lexer_next();
		expr = parser_make_function_designator(symbol);
	} else if (tok->kind == TOK_IDENT &&
	           find_func(tok->text) &&
	           lexer_peek_ahead(1)->kind != TOK_LPAREN) {
		symbol = tok->text;
		lexer_next();
		expr = parser_make_function_designator(symbol);
	} else if (tok->kind == TOK_NUM && tok->long_value == 0) {
		lexer_next();
		expr = new_num(0);
	} else {
		expr = parse_local_scalar_initializer_expr(TCC_SIZEOF_PTR);
	}

	if (out_symbol)
		*out_symbol = symbol;
	return expr;
}

static void
stmt_store_static_pointer_slot(Global *g, int index, const char *symbol)
{
	int offset;

	if (!g || index < 0)
		return;

	offset = index * TCC_SIZEOF_PTR;
	for (int b = 0; b < TCC_SIZEOF_PTR; b++)
		global_set_init_byte(g, offset + b, 0);
	if (symbol && symbol[0])
		global_set_init_sym(g, index, symbol);
	if (global_init_count(g) < offset + TCC_SIZEOF_PTR)
		global_set_init_count(g, offset + TCC_SIZEOF_PTR);
}

static Node *
stmt_build_local_function_pointer_array_initializer(const char *name, int offset,
                                                    Type *elem_type, int array_len)
{
	Node *head = NULL;
	Node *tail = NULL;
	Node **init_exprs = NULL;
	unsigned char *init_seen = NULL;
	int init_cap = 0;
	int next_index = 0;
	int max_index = -1;

	expect(TOK_LBRACE);

	while (lexer_peek()->kind != TOK_RBRACE) {
		const char *symbol = NULL;
		Node *expr;
		int first_index = next_index;
		int last_index = next_index;

		if (stmt_try_parse_local_array_designator(&first_index, &last_index))
			next_index = last_index + 1;
		else
			next_index++;

		if (first_index < 0 || last_index < first_index)
			fatal_cur("Invalid array designator range\n");
		if (array_len > 0 && last_index >= array_len)
			fatal_cur("Array designator index exceeds array length\n");

		stmt_local_array_init_reserve(last_index + 1, &init_cap, NULL, &init_exprs, &init_seen);
		expr = stmt_parse_function_pointer_initializer_expr(&symbol);
		validate_pointer_initializer_compatibility(elem_type, expr);
		for (int i = first_index; i <= last_index; i++) {
			init_exprs[i] = (i == first_index) ? expr : clone_node_tree(expr);
			init_seen[i] = 1;
		}
		if (last_index > max_index)
			max_index = last_index;

		if (lexer_peek()->kind == TOK_COMMA)
			lexer_next();
		else
			break;
	}

	expect(TOK_RBRACE);

	if (array_len <= 0)
		array_len = max_index + 1;

	for (int i = 0; i < array_len; i++) {
		Node *assign_expr = (init_seen && init_seen[i] && init_exprs[i])
		                  ? init_exprs[i]
		                  : new_num(0);
		Node *assign = make_local_array_assign_expr(name, offset, elem_type, i, assign_expr);
		if (!head) {
			head = assign;
			tail = assign;
		} else {
			tail->next = assign;
			tail = assign;
		}
	}

	xfree(init_exprs);
	xfree(init_seen);
	return head;
}

static void
stmt_parse_static_function_pointer_array_initializer(Global *g, Type *elem_type, int array_len)
{
	int next_index = 0;
	int max_index = -1;

	expect(TOK_LBRACE);

	while (lexer_peek()->kind != TOK_RBRACE) {
		const char *symbol = NULL;
		Node *expr;
		int first_index = next_index;
		int last_index = next_index;

		if (stmt_try_parse_local_array_designator(&first_index, &last_index))
			next_index = last_index + 1;
		else
			next_index++;

		if (first_index < 0 || last_index < first_index)
			fatal_cur("Invalid array designator range\n");
		if (array_len > 0 && last_index >= array_len)
			fatal_cur("Array designator index exceeds array length\n");

		expr = stmt_parse_function_pointer_initializer_expr(&symbol);
		validate_pointer_initializer_compatibility(elem_type, expr);
		for (int i = first_index; i <= last_index; i++)
			stmt_store_static_pointer_slot(g, i, symbol);
		if (last_index > max_index)
			max_index = last_index;

		if (lexer_peek()->kind == TOK_COMMA)
			lexer_next();
		else
			break;
	}

	expect(TOK_RBRACE);

	if (array_len == 0)
		g->array_len = max_index + 1;
}

static const char *
stmt_resolve_struct_type_name(Type *type)
{
	Type *typedef_type;

	if (!type || !type_is_struct(type))
		return "";
	if (type->struct_name[0])
		return type->struct_name;
	if (!type_source_is_typedef(type) || !type_source_name(type)[0])
		return "";
	typedef_type = parser_find_typedef(type_source_name(type));
	if (typedef_type && typedef_type->struct_name[0])
		return typedef_type->struct_name;
	return "";
}

static int
stmt_parse_static_struct_array_initializer(Global **pg, int *g_committed,
                                           Type *base_type, int *array_len,
                                           int *elem_size)
{
	const char *struct_tag;
	StructDef *sdef;
	int g_idx;
	int elem_count;

	if (lexer_peek()->kind != TOK_LBRACE || !type_is_struct(base_type))
		return 0;

	struct_tag = stmt_resolve_struct_type_name(base_type);
	if (!struct_tag || !struct_tag[0])
		struct_tag = base_type->struct_name;
	sdef = struct_tag && struct_tag[0] ? find_struct_or_null(struct_tag) : NULL;
	if (!sdef)
		fatal_cur("Unsupported static struct array initializer\n");

	g_idx = parser_global_index(*pg);
	if (!*g_committed) {
		parser_commit_reserved_global();
		*g_committed = 1;
	}
	*pg = parser_global_at(g_idx);
	(*pg)->is_struct = 1;
	(*pg)->is_array = 1;
	(*pg)->elem_size = sdef->size;
	STRNCPY((*pg)->struct_name, struct_tag, sizeof((*pg)->struct_name) - 1);

	lexer_next(); /* consume outer { */
	elem_count = 0;
	while (lexer_peek()->kind != TOK_RBRACE) {
		int base_offset = elem_count * sdef->size;

		if (lexer_peek()->kind == TOK_LBRACE) {
			lexer_next();
			parse_global_struct_initializer_body(g_idx, sdef, base_offset);
			expect(TOK_RBRACE);
		} else {
			parse_global_struct_initializer_body_ex(g_idx, sdef, base_offset, 1);
		}

		*pg = parser_global_at(g_idx);
		elem_count++;
		if (lexer_peek()->kind == TOK_COMMA) {
			lexer_next();
			if (lexer_peek()->kind == TOK_RBRACE)
				break;
		} else if (lexer_peek()->kind == TOK_RBRACE) {
			break;
		}
	}

	expect(TOK_RBRACE);

	if (*array_len == 0)
		*array_len = elem_count > 0 ? elem_count : 1;
	(*pg)->array_len = *array_len;
	apply_type_to_global(*pg, type_array(clone_type(base_type), *array_len));
	global_set_init_count(*pg, (*array_len) * sdef->size);
	*elem_size = sdef->size;
	return 1;
}

static void
stmt_push_control_scope(int saved_local_count, int is_loop)
{
	int *depth = &stmt_control_scope_depth;
	int index = *depth;

	if (index >= STMT_SCOPE_STACK_MAX)
		ICE("statement control scope overflow");
	stmt_control_scope_stack[index] = saved_local_count;
	stmt_control_is_loop_stack[index] = is_loop;
	*depth = index + 1;
}

static void
stmt_pop_control_scope(void)
{
	int *depth = &stmt_control_scope_depth;

	if (*depth <= 0)
		ICE("statement control scope underflow");
	(*depth)--;
}

static int
stmt_nearest_control_scope_saved(int require_loop)
{
	for (int i = stmt_control_scope_depth - 1; i >= 0; i--) {
		if (!require_loop || stmt_control_is_loop_stack[i])
			return stmt_control_scope_stack[i];
	}

	return -1;
}

static Node *
stmt_wrap_scope_cleanup(Node *tail_stmt, int saved_local_count)
{
	Node *cleanup;

	if (!tail_stmt)
		return NULL;

	cleanup = parser_collect_vla_scope_cleanup(saved_local_count);
	if (!cleanup)
		return tail_stmt;

	return new_block(append_node(cleanup, tail_stmt));
}

static Node *
stmt_wrap_return_with_cleanup(Node *expr)
{
	Node *cleanup = parser_collect_vla_scope_cleanup(0);

	if (!cleanup)
		return new_return(expr);

	if (!expr)
		return new_block(append_node(cleanup, new_return(NULL)));

	if (expr->type && type_is_struct(expr->type)) {
		const char *sname = stmt_resolve_struct_type_name(expr->type);
		StructDef *def = sname[0] ? find_struct(sname) : NULL;
		int temp_off = add_struct_local("__ret_cleanup_tmp", sname);
		Node *decl = new_struct_decl("__ret_cleanup_tmp", temp_off);
		Node *lhs = new_var("__ret_cleanup_tmp", temp_off);
		Node *temp_ref = new_var("__ret_cleanup_tmp", temp_off);
		Node *assign = new_assign(lhs, expr);

			decl->type = def && def->is_union ? type_union(sname, def->size)
			                                  : clone_type(expr->type);
			lhs->type = def && def->is_union ? type_union(sname, def->size)
			                                 : clone_type(expr->type);
		lhs->elem_size = def ? def->size : expr->type->size;
		STRNCPY(lhs->struct_name, sname, sizeof(lhs->struct_name) - 1);
			temp_ref->type = def && def->is_union ? type_union(sname, def->size)
			                                      : clone_type(expr->type);
		temp_ref->elem_size = def ? def->size : expr->type->size;
		STRNCPY(temp_ref->struct_name, sname, sizeof(temp_ref->struct_name) - 1);

		return new_block(append_node(decl,
		                             append_node(assign,
		                                         append_node(cleanup, new_return(temp_ref)))));
	}

	{
		Type *temp_type = expr->type ? clone_type(expr->type)
		                             : (expr->is_pointer ? type_ptr(type_char()) : type_int());
		int temp_off = add_typed_local("__ret_cleanup_tmp", temp_type);
		Node *decl = temp_type->kind == TY_PTR
			? new_ptr_decl("__ret_cleanup_tmp", temp_off)
			: new_decl("__ret_cleanup_tmp", temp_off);
		Node *lhs = new_var("__ret_cleanup_tmp", temp_off);
		Node *temp_ref = new_var("__ret_cleanup_tmp", temp_off);
		Node *assign = new_assign(lhs, expr);

		decl->type = temp_type;
		decl->elem_size = type_elem_size(temp_type);
		lhs->type = clone_type(temp_type);
		lhs->elem_size = type_elem_size(temp_type);
		lhs->is_pointer = temp_type->kind == TY_PTR;
		temp_ref->type = clone_type(temp_type);
		temp_ref->elem_size = type_elem_size(temp_type);
		temp_ref->is_pointer = temp_type->kind == TY_PTR;

		return new_block(append_node(decl,
		                             append_node(assign,
		                                         append_node(cleanup, new_return(temp_ref)))));
	}
}

int
add_decl_typed_local(int requested_align, const char *name, Type *type)
{
	int offset;
	parser_reject_reserved_decl_identifier_name(name, "a local variable name");
	parser_validate_decl_alignment(requested_align, type);
	parser_set_decl_align_request(requested_align);
	parser_set_decl_register_request(stmt_decl_register_request);
	offset = add_typed_local(name, type);
	parser_clear_decl_align_request();
	parser_clear_decl_register_request();
	return offset;
}

static int
add_decl_scalar_local(int requested_align, const char *name, int base_size)
{
	int offset;

	parser_reject_reserved_decl_identifier_name(name, "a local variable name");
	parser_set_decl_align_request(requested_align);
	parser_set_decl_register_request(stmt_decl_register_request);
	if (base_size == 1)
		offset = add_char_local(name);
	else if (base_size == 8)
		offset = add_local_sized(name, 2, 0);
	else
		offset = add_local(name);
	parser_clear_decl_align_request();
	parser_clear_decl_register_request();
	return offset;
}


static int
array_decl_looks_runtime_vla(void)
{
	int depth = 0;

	for (int i = 0;; i++) {
		const Token *tok = lexer_peek_ahead(i);

		if (tok->kind == TOK_EOF)
			return 0;

		if (tok->kind == TOK_LBRACKET) {
			depth++;
			continue;
		}

		if (tok->kind == TOK_RBRACKET) {
			depth--;
			if (depth == 0)
				return 0;
			continue;
		}

		if (depth != 1)
			continue;

		if (tok->kind == TOK_STAR)
			return 1;

		if (tok->kind == TOK_IDENT) {
			int enum_value = 0;
			if (!parser_find_enum_const(tok->text, &enum_value))
				return 1;
		}
	}
}

static void
stmt_require_c99_for_runtime_vla(void)
{
	if (tcc_lang_is_c89_or_c90())
		fatal_cur("variable length array syntax is not allowed in C89/C90 mode\n");
}

static Node *make_pointer_index_store(const char *name, int offset, Type *ptr_type,
                                      Type *elem_type, int elem_size, Node *index,
                                      Node *value);
static Node *build_runtime_vla_zero_fill_loop(const char *name, int offset,
                                              Type *ptr_type, Type *elem_type,
                                              int elem_size, int start_index,
                                              const char *bound_name, int bound_offset);
static Node *build_runtime_vla_decl_stmt_list(int requested_align, const char *name,
                                              Type *base_type, int base_elem_size);
static Node *build_runtime_vm_typedef_array_decl_stmt_list(int requested_align,
                                                           const char *name,
                                                           Type *array_type);
static Node *build_local_scalar_array_initializer_stmt(const char *name, int offset,
                                                       Type *array_type, int base_size,
                                                       Node *decl, Type *comma_base_type,
                                                       int requested_align);
static Node *stmt_try_parse_named_aggregate_array_initializer(const char *name,
                                                              int offset,
                                                              Type *array_type,
                                                              Node *decl);
static Node *stmt_append_comma_after_array_initializer(Node *head, Type *base_type,
                                                       int requested_align);
static Node *build_local_struct_initializer_stmt(const char *name, int offset,
                                                 Type *decl_type, Node *decl_node);

static Node *
make_decl_node_for_type(const char *name, int offset, Type *type)
{
	Node *node;

	if (type->kind == TY_PTR)
		node = new_ptr_decl(name, offset);
	else if (type->kind == TY_ARRAY)
		node = new_array_decl(name, offset, type->array_len);
	else if (type_is_struct(type))
		node = new_struct_decl(name, offset);
	else
		node = new_decl(name, offset);

	node->type = type;
	node->elem_size = type_elem_size(type);
	return node;
}

static Node *
make_local_lvalue_for_type(const char *name, int offset, Type *type)
{
	Node *lhs = new_var(name, offset);
	lhs->type = clone_type(type);
	lhs->elem_size = type_elem_size(type);
	lhs->is_pointer = type->kind == TY_PTR;
	return lhs;
}

static int
token_is_plain_type_specifier(TokenKind kind)
{
	return kind == TOK_VOID || kind == TOK_BOOL || kind == TOK_CHAR ||
	       kind == TOK_INT || kind == TOK_FLOAT || kind == TOK_DOUBLE ||
	       kind == TOK_STRUCT || kind == TOK_UNION || kind == TOK_ENUM ||
	       kind == TOK_SIGNED || kind == TOK_UNSIGNED ||
	       kind == TOK_SHORT || kind == TOK_LONG;
}

static int
stmt_type_is_variably_modified(Type *type)
{
	if (!type)
		return 0;
	if (type->kind == TY_ARRAY && type->is_vm_type && type->vla_bound_name[0])
		return 1;
	if (type->kind == TY_PTR)
		return stmt_type_is_variably_modified(type->base);
	return 0;
}

static Node *
build_runtime_vla_decl_block(int requested_align, const char *name, Type *elem_type, int elem_size)
{
	Node *head = build_runtime_vla_decl_stmt_list(requested_align, name, elem_type, elem_size);
	Node *tail = head;
	Type *decl_base_type = clone_type(elem_type);
	int decl_base_elem_size = elem_size;

	while (tail && tail->next)
		tail = tail->next;

	while (lexer_peek()->kind == TOK_COMMA) {
		int ptr_depth = 0;
		const Token *next_name;
		char next_decl_name[64] = {0};
		Node *extra = NULL;

		lexer_next();

		while (lexer_peek()->kind == TOK_STAR) {
			lexer_next();
			skip_pointer_qualifiers();
			skip_inline_qualifiers();
			ptr_depth++;
		}

		next_name = lexer_peek();
		if (next_name->kind != TOK_IDENT)
			fatal_cur("Expected identifier in declaration list\n");
		STRNCPY(next_decl_name, next_name->text ? next_name->text : "", sizeof(next_decl_name) - 1);
		lexer_next();

		if (ptr_depth == 0 && lexer_peek()->kind == TOK_LBRACKET &&
		    array_decl_looks_runtime_vla()) {
			stmt_require_c99_for_runtime_vla();
			extra = build_runtime_vla_decl_stmt_list(requested_align, next_decl_name,
			                                         decl_base_type, decl_base_elem_size);
		} else if (lexer_peek()->kind == TOK_LBRACKET) {
			int dims[MAX_ARRAY_DIMS] = {0};
			int dim_count = parse_array_dimensions(dims, 0, 0);
			Type *decl_type = clone_type(decl_base_type);
			Node *decl_node;
			int offset;

			for (int i = 0; i < ptr_depth; i++)
				decl_type = type_ptr(decl_type);
			decl_type = build_array_type_from_dims(decl_type, dims, dim_count);

			offset = add_decl_typed_local(requested_align, next_decl_name, decl_type);
			decl_node = make_decl_node_for_type(next_decl_name, offset, decl_type);

			if (lexer_peek()->kind == TOK_ASSIGN)
				extra = build_local_scalar_array_initializer_stmt(next_decl_name, offset,
				                                                  decl_type, decl_base_elem_size,
				                                                  decl_node, decl_base_type,
				                                                  requested_align);
			else
				extra = decl_node;
		} else {
			Type *decl_type = clone_type(decl_base_type);
			Node *decl_node;
			int offset;

			for (int i = 0; i < ptr_depth; i++)
				decl_type = type_ptr(decl_type);

			offset = add_decl_typed_local(requested_align, next_decl_name, decl_type);
			decl_node = make_decl_node_for_type(next_decl_name, offset, decl_type);

			if (lexer_peek()->kind == TOK_ASSIGN) {
				Node *lhs;
				Node *expr;
				Node *assign;

				if (type_is_struct(decl_type))
					extra = build_local_struct_initializer_stmt(next_decl_name, offset,
					                                            decl_type, decl_node);
				else {
					lexer_next();
					expr = parse_local_scalar_initializer_expr(type_sizeof(decl_type));
					lhs = make_local_lvalue_for_type(next_decl_name, offset, decl_type);
					assign = stmt_build_initializer_assign(lhs, expr, decl_type);
					extra = append_node(decl_node, assign);
				}
			} else {
				extra = decl_node;
			}
		}

		if (!head) {
			head = extra;
			tail = extra;
		} else {
			tail->next = extra;
		}
		while (tail && tail->next)
			tail = tail->next;
	}

expect(TOK_SEMI);
return new_block(head);
}

static Node *
build_runtime_vm_typedef_array_decl_stmt_list(int requested_align, const char *name,
                                              Type *array_type)
{
	Type *elem_type;
	Type *ptr_type;
	const char *bound_name;
	int elem_size;
	char stack_name[64];
	int stack_offset;
	int offset;
	Node *stack_decl;
	Node *stack_lhs;
	Node *stack_assign;
	Node *decl;
	Node *lhs;
	Node *bound_expr;
	Node *size_expr;
	Node *call;
	Node *assign;

	if (!array_type || array_type->kind != TY_ARRAY || !array_type->is_vm_type ||
	    !array_type->vla_bound_name[0])
		return NULL;

	elem_type = clone_type(array_type->vla_elem_type
	                       ? array_type->vla_elem_type
	                       : array_type->base);
	elem_size = type_sizeof(elem_type);
	if (elem_size <= 0)
		fatal_cur("runtime VLA element type must be complete\n");

	bound_name = array_type->vla_bound_name;

	snprintf(stack_name, sizeof(stack_name), "__vla_sp_%d",
	         parser_alloc_compound_arg_temp_id());
	parser_set_decl_align_request(requested_align);
	stack_offset = add_pointer_local(stack_name, 1);
	parser_clear_decl_align_request();

	stack_decl = new_ptr_decl(stack_name, stack_offset);
	stack_decl->type = type_ptr(type_char());
	stack_decl->elem_size = 1;
	stack_decl->suppress_debug_loc = 1;

	stack_lhs = new_var(stack_name, stack_offset);
	stack_lhs->type = type_ptr(type_char());
	stack_lhs->is_pointer = 1;
	stack_lhs->elem_size = 1;
	stack_lhs->suppress_debug_loc = 1;
	stack_assign = new_assign(stack_lhs, new_call("__builtin_stack_save", NULL));
	stack_assign->suppress_debug_loc = 1;

	ptr_type = type_ptr(clone_type(elem_type));
	offset = add_decl_typed_local(requested_align, name, ptr_type);
	parser_mark_local_vla(name, bound_name, stack_name, stack_offset,
	                      elem_type, elem_size);

	decl = new_ptr_decl(name, offset);
	decl->type = ptr_type;
	decl->elem_size = elem_size;

	lhs = new_var(name, offset);
	lhs->is_pointer = 1;
	lhs->elem_size = elem_size;
	lhs->type = type_ptr(clone_type(elem_type));

	bound_expr = make_scalar_var_node(bound_name);
	bound_expr->suppress_debug_loc = 1;
	size_expr = new_binary(ND_MUL, bound_expr, new_num(elem_size));
	call = new_call("__builtin_stack_alloc", size_expr);
	call->is_pointer = 1;
	call->elem_size = elem_size;
	call->type = type_ptr(clone_type(elem_type));
	assign = new_assign(lhs, call);

	return append_node(stack_decl,
	                   append_node(stack_assign,
	                               append_node(decl, assign)));
}

static Node *
build_runtime_vla_decl_stmt_list(int requested_align, const char *name, Type *elem_type, int elem_size)
{
	Type *ptr_type;
	int tail_dims[MAX_ARRAY_DIMS] = {0};
	int tail_dim_count = 0;
	int *init_values = NULL;
	int init_values_cap = 0;
	int init_count = 0;
	int string_init = 0;
	int string_width = 1;
	int zero_fill_only = 0;
	char *string_value = NULL;
	size_t string_len = 0;
	char stack_name[64];
	int stack_offset;
	Node *stack_decl;
	Node *stack_lhs;
	Node *stack_assign;
	char bound_name[64];
	int bound_offset;
	Node *bound_decl;
	Node *bound_lhs;
	Node *bound_assign;
	int offset;
	Node *decl;
	Node *lhs;
	Node *len_expr;
	Node *len_expr_copy;
	Node *size_expr;
	Node *call;
	Node *assign;

	expect(TOK_LBRACKET);
	if (lexer_peek()->kind == TOK_RBRACKET)
		fatal_cur("runtime VLA requires a bound expression\n");

	len_expr = parse_assignment();
	expect(TOK_RBRACKET);

	if (lexer_peek()->kind == TOK_LBRACKET) {
		if (array_decl_looks_runtime_vla())
			fatal_cur("only the first dimension of a runtime VLA may be variably modified\n");
		tail_dim_count = parse_array_dimensions(tail_dims, 0, 0);
		if (tail_dim_count > 0) {
			elem_type = build_array_type_from_dims(elem_type, tail_dims, tail_dim_count);
			elem_size = type_sizeof(elem_type);
		}
	}

	if (lexer_peek()->kind == TOK_ASSIGN) {
		lexer_next();

		if (zero_fill_only || tail_dim_count > 0) {
			if (!(lexer_peek_ahead(1)->kind == TOK_NUM &&
			      lexer_peek_ahead(1)->value == 0 &&
			      consume_all_zero_initializer()))
				fatal_cur("only {0} is supported for multidimensional runtime VLA initializers\n");
			zero_fill_only = 1;
		} else if (lexer_peek()->kind == TOK_LBRACE) {
			if (lexer_peek_ahead(1)->kind == TOK_NUM &&
			    lexer_peek_ahead(1)->value == 0 &&
			    consume_all_zero_initializer()) {
				zero_fill_only = 1;
			} else {
				lexer_next();

				while (lexer_peek()->kind != TOK_RBRACE) {
					const Token *value = lexer_peek();
					if (value->kind != TOK_NUM)
						fatal_cur("runtime VLA initializer must contain constant integers\n");

					if (init_count >= init_values_cap) {
						int new_cap = init_values_cap ? init_values_cap * 2 : 64;
						init_values = (int *)xrealloc(init_values, (size_t)new_cap * sizeof(int));
						init_values_cap = new_cap;
					}

					init_values[init_count++] = value->value;
					lexer_next();

					if (lexer_peek()->kind == TOK_COMMA)
						lexer_next();
					else
						break;
				}

				expect(TOK_RBRACE);
			}
		} else if (lexer_peek()->kind == TOK_STRING && tail_dim_count == 0) {
			const Token *value = lexer_peek();
			stmt_require_string_literal_array_match(value, elem_size,
			                                        "String literal element width does not match runtime VLA element type");
			string_init = 1;
			string_width = stmt_string_literal_elem_width(value);
			string_len = value->text_len;
			string_value = xmalloc(string_len + 1);
			memcpy(string_value, value->text, string_len);
			string_value[string_len] = '\0';
			init_count = string_width > 1
				? (int)(string_len / (size_t)string_width) + 1
				: (int)string_len + 1;
			lexer_next();
		} else {
			fatal_cur("Unsupported runtime VLA initializer\n");
		}
	}
	snprintf(stack_name, sizeof(stack_name), "__vla_sp_%d",
	         parser_alloc_compound_arg_temp_id());
	parser_set_decl_align_request(requested_align);
	stack_offset = add_pointer_local(stack_name, 1);
	parser_clear_decl_align_request();
	stack_decl = new_ptr_decl(stack_name, stack_offset);
	stack_decl->type = type_ptr(type_char());
	stack_decl->elem_size = 1;
	stack_decl->suppress_debug_loc = 1;

	stack_lhs = new_var(stack_name, stack_offset);
	stack_lhs->type = type_ptr(type_char());
	stack_lhs->is_pointer = 1;
	stack_lhs->elem_size = 1;
	stack_lhs->suppress_debug_loc = 1;
	stack_assign = new_assign(stack_lhs, new_call("__builtin_stack_save", NULL));
	stack_assign->suppress_debug_loc = 1;

	snprintf(bound_name, sizeof(bound_name), "__vla_len_%d",
	         parser_alloc_compound_arg_temp_id());
	parser_set_decl_align_request(requested_align);
	bound_offset = add_local(bound_name);
	parser_clear_decl_align_request();
	bound_decl = new_decl(bound_name, bound_offset);
	bound_decl->type = type_int();
	bound_decl->elem_size = TCC_SIZEOF_INT;
	bound_decl->suppress_debug_loc = 1;

	bound_lhs = new_var(bound_name, bound_offset);
	bound_lhs->type = type_int();
	bound_lhs->elem_size = TCC_SIZEOF_INT;
	bound_lhs->suppress_debug_loc = 1;
	bound_assign = new_assign(bound_lhs, len_expr);
	bound_assign->suppress_debug_loc = 1;

	ptr_type = type_ptr(clone_type(elem_type));
	offset = add_decl_typed_local(requested_align, name, ptr_type);
	parser_mark_local_vla(name, bound_name, stack_name, stack_offset,
	                      elem_type, elem_size);

	decl = new_ptr_decl(name, offset);
	decl->type = ptr_type;
	decl->elem_size = elem_size;

	lhs = new_var(name, offset);
	lhs->is_pointer = 1;
	lhs->elem_size = elem_size;
	lhs->type = type_ptr(clone_type(elem_type));

	len_expr_copy = new_var(bound_name, bound_offset);
	len_expr_copy->type = type_int();
	len_expr_copy->elem_size = TCC_SIZEOF_INT;
	len_expr_copy->suppress_debug_loc = 1;

	size_expr = new_binary(ND_MUL, len_expr_copy, new_num(elem_size));
	call = new_call("__builtin_stack_alloc", size_expr);
	call->is_pointer = 1;
	call->elem_size = elem_size;
	call->type = type_ptr(clone_type(elem_type));

	assign = new_assign(lhs, call);

	{
		Node *head = stack_decl;
		Node *tail = append_node(head, stack_assign);
		tail = append_node(tail, bound_decl);
		tail = append_node(tail, bound_assign);
		tail = append_node(tail, decl);
		tail = append_node(tail, assign);

		if (zero_fill_only) {
			char byte_bound_name[64];
			int byte_bound_offset;
			Node *byte_bound_decl;
			Node *byte_bound_lhs;
			Node *byte_bound_rhs;
			Node *byte_bound_assign;
			Type *byte_ptr_type = type_ptr(type_char());
			Type *byte_type = type_char();

			snprintf(byte_bound_name, sizeof(byte_bound_name), "__vla_zero_bytes_%d",
			         parser_alloc_compound_arg_temp_id());
			parser_set_decl_align_request(0);
			byte_bound_offset = add_local(byte_bound_name);
			parser_clear_decl_align_request();
			byte_bound_decl = new_decl(byte_bound_name, byte_bound_offset);
			byte_bound_decl->type = type_int();
			byte_bound_decl->elem_size = TCC_SIZEOF_INT;
			byte_bound_decl->suppress_debug_loc = 1;

			byte_bound_lhs = new_var(byte_bound_name, byte_bound_offset);
			byte_bound_lhs->type = type_int();
			byte_bound_lhs->elem_size = TCC_SIZEOF_INT;
			byte_bound_lhs->suppress_debug_loc = 1;
			byte_bound_rhs = new_var(bound_name, bound_offset);
			byte_bound_rhs->type = type_int();
			byte_bound_rhs->elem_size = TCC_SIZEOF_INT;
			byte_bound_rhs->suppress_debug_loc = 1;
			if (elem_size != 1)
				byte_bound_rhs = new_binary(ND_MUL, byte_bound_rhs, new_num(elem_size));
			byte_bound_assign = new_assign(byte_bound_lhs, byte_bound_rhs);
			byte_bound_assign->suppress_debug_loc = 1;

			tail = append_node(tail, byte_bound_decl);
			tail = append_node(tail, byte_bound_assign);
			tail = append_node(tail,
			                   build_runtime_vla_zero_fill_loop(name, offset,
			                                                   byte_ptr_type, byte_type, 1,
			                                                   0, byte_bound_name, byte_bound_offset));
		} else {
			if (string_init) {
				for (int i = 0; i < init_count; i++) {
					int val = 0;
					if (string_width > 1) {
						for (int bi = 0; bi < string_width; bi++) {
							size_t byte_idx = (size_t)i * (size_t)string_width + (size_t)bi;
							if (byte_idx < string_len)
								val |= (unsigned char)string_value[byte_idx] << (8 * bi);
						}
					} else {
						val = (unsigned char)string_value[i];
					}
					tail = append_node(tail,
					                   make_pointer_index_store(name, offset, ptr_type, elem_type,
					                                            elem_size, new_num(i), new_num(val)));
				}
			} else {
				for (int i = 0; i < init_count; i++) {
					tail = append_node(tail,
					                   make_pointer_index_store(name, offset, ptr_type, elem_type,
					                                            elem_size, new_num(i), new_num(init_values[i])));
				}
			}

			if (string_init || init_count > 0) {
				tail = append_node(tail,
				                   build_runtime_vla_zero_fill_loop(name, offset, ptr_type, elem_type,
				                                                   elem_size, init_count,
				                                                   bound_name, bound_offset));
			}
		}

		xfree(init_values);
		xfree(string_value);
		return head;
	}
}

static int
block_item_starts_declaration(void)
{
	const Token *tok = lexer_peek();

	if (token_starts_plain_bool_type_specifier(tok,
	                                          lexer_peek_ahead(1),
	                                          lexer_peek_ahead(2)))
		return 1;
	if (token_is_static_assert_keyword(tok))
		return 1;
	if (token_starts_alignas_specifier(tok, lexer_peek_ahead(1)))
		return 1;

	return is_type_start_token(tok->kind, tok->text) ||
	       tok->kind == TOK_AUTO ||
	       tok->kind == TOK_REGISTER ||
	       tok->kind == TOK_THREAD_LOCAL;
}

static void
reject_declaration_after_label_before_c23(void)
{
	if (tcc_lang_at_least(LANG_C23))
		return;
	if (!block_item_starts_declaration())
		return;
	fatal_cur("declaration after label is not allowed before C23\n");
}

static Node *
parse_embedded_statement(void)
{
	if (token_starts_plain_bool_type_specifier(lexer_peek(),
	                                          lexer_peek_ahead(1),
	                                          lexer_peek_ahead(2)))
		reject_plain_bool_keyword_before_c23(lexer_peek());
	if (block_item_starts_declaration())
		fatal_cur("declaration is not allowed as a controlled statement\n");
	return parse_statement();
}

static void
reject_static_assert_token(const Token *token)
{
	if (!token_is_static_assert_keyword(token))
		return;

	if (STRCMP(token->text, "_Static_assert") == 0) {
		if (!tcc_lang_at_least(LANG_C11))
			fatal_token(token, "_Static_assert is not allowed before C11\n");
		return;
	}

	if (!tcc_lang_at_least(LANG_C23))
		fatal_token(token, "static_assert is not allowed before C23\n");
}

int
parse_static_assert_declaration(void)
{
	const Token *keyword = lexer_peek();
	Node *expr;
	const char *message = NULL;
	int has_message = 0;

	if (!token_is_static_assert_keyword(keyword))
		return 0;

	reject_static_assert_token(keyword);
	lexer_next();
	expect(TOK_LPAREN);

	expr = fold_constants(parse_assignment());
	if (!expr || expr->kind != ND_NUM)
		fatal_token(keyword, "static assertion requires an integer constant expression\n");

	if (lexer_peek()->kind == TOK_COMMA) {
		const Token *msg;
		lexer_next();
		msg = lexer_peek();
		if (msg->kind != TOK_STRING)
			fatal_cur("Expected string literal in static assertion\n");
		message = msg->text ? msg->text : "";
		has_message = 1;
		lexer_next();
	} else if (!tcc_lang_at_least(LANG_C23)) {
		fatal_token(keyword, "static assertion message is required before C23\n");
	}

	expect(TOK_RPAREN);
	expect(TOK_SEMI);

	if (expr->long_value == 0) {
		if (has_message)
			fatal_token(keyword, "static assertion failed: %s\n", message);
		fatal_token(keyword, "static assertion failed\n");
	}

	return 1;
}

Node *
parse_block_contents(void)
{
	Node *tmp;
	parser_profile_scope_enter(PARSER_PROF_BLOCK);
	Node head = {0};
	Node *cur = &head;
	ParserScopeMark saved_scope = {0};
	int reuse_function_scope = stmt_use_function_scope_for_next_block;

	if (reuse_function_scope) {
		stmt_use_function_scope_for_next_block = 0;
		saved_scope.local_count = parser_current_local_count();
		saved_scope.typedef_count = 0;
		saved_scope.struct_count = 0;
		saved_scope.enum_tag_count = 0;
		saved_scope.enum_const_count = 0;
	} else {
		parser_mark_local_scope(&saved_scope);
	}
	int enforce_decl_order = tcc_lang_is_c89_or_c90();
	int saw_statement = 0;

	stmt_push_block_scope(saved_scope.local_count);

	while (lexer_peek()->kind != TOK_RBRACE &&
	       lexer_peek()->kind != TOK_EOF) {
		int is_decl = 0;
		if (enforce_decl_order) {
			is_decl = block_item_starts_declaration();
			if (is_decl && saw_statement)
				fatal_cur("mixed declarations and statements are not allowed in C89/C90 mode\n");
		}

		Node *stmt = parse_statement();
		if (!stmt) break;
		if (enforce_decl_order && !is_decl)
			saw_statement = 1;
		cur->next = stmt;
		cur = stmt;
	}

	{
		Node *cleanup = parser_collect_vla_scope_cleanup(saved_scope.local_count);
		if (cleanup) {
			if (!head.next)
				head.next = cleanup;
			else
				cur->next = cleanup;
		}
	}

	/* Restore scope: keep statics declared in this block visible to outer
	 * scope (they map to globals), but remove non-static locals. */
	stmt_pop_block_scope();
	if (!reuse_function_scope)
		parser_restore_local_scope_keep_statics(&saved_scope);

	tmp = new_block(head.next);
	parser_profile_scope_leave(PARSER_PROF_BLOCK);
	return tmp;
}

Node *
make_local_array_store(const char *name, int offset, int elem_size, int index, int value)
{
	Node *idx = new_num(index);
	Node *lhs = new_index(name, offset, idx);
	lhs->elem_size = elem_size;
	lhs->type = type_for_size(elem_size);
	return new_assign(lhs, new_num(value));
}

static Node *
make_local_array_assign_expr(const char *name, int offset, Type *elem_type, int index, Node *expr)
{
	Node *idx = new_num(index);
	Node *lhs = new_index(name, offset, idx);

	lhs->elem_size = type_sizeof(elem_type);
	lhs->type = clone_type(elem_type);
	if (type_is_pointer(elem_type))
		lhs->is_pointer = 1;
	if (type_is_pointer(elem_type) &&
	    type_pointee(elem_type) &&
	    type_is_struct(type_pointee(elem_type))) {
		STRNCPY(lhs->struct_name, type_pointee(elem_type)->struct_name,
		        sizeof(lhs->struct_name) - 1);
	}

	if (type_is_pointer(elem_type))
		validate_pointer_initializer_compatibility(elem_type, expr);

	return stmt_build_initializer_assign(lhs, expr, elem_type);
}

static int
stmt_try_parse_local_array_designator(int *out_lo, int *out_hi)
{
	int lo;
	int hi;

	if (lexer_peek()->kind != TOK_LBRACKET)
		return 0;
	if (tcc_lang_is_c89_or_c90())
		fatal_cur("designated initializers are not allowed in C89/C90 mode\n");
	if (parser_array_bound_contains_nonconstant_identifier())
		fatal_cur("Expected constant index in designated initializer\n");

	lexer_next();
	lo = (int)parser_eval_const_int_expr();
	hi = lo;

	if (lexer_peek()->kind == TOK_DOT &&
	    lexer_peek_ahead(1)->kind == TOK_DOT &&
	    lexer_peek_ahead(2)->kind == TOK_DOT) {
		lexer_next();
		lexer_next();
		lexer_next();
		hi = (int)parser_eval_const_int_expr();
	}

	expect(TOK_RBRACKET);
	expect(TOK_ASSIGN);

	*out_lo = lo;
	*out_hi = hi;
	return 1;
}

static void
stmt_local_array_init_reserve(int needed, int *cap, int **values, Node ***exprs, unsigned char **seen)
{
	int old_cap;
	int new_cap;

	if (needed <= *cap)
		return;

	old_cap = *cap;
	new_cap = old_cap ? old_cap : 64;
	while (needed > new_cap)
		new_cap *= 2;

	if (values) {
		*values = (int *)xrealloc(*values, (size_t)new_cap * sizeof(int));
		memset(*values + old_cap, 0, (size_t)(new_cap - old_cap) * sizeof(int));
	}
	if (exprs) {
		*exprs = (Node **)xrealloc(*exprs, (size_t)new_cap * sizeof(Node *));
		memset(*exprs + old_cap, 0, (size_t)(new_cap - old_cap) * sizeof(Node *));
	}
	if (seen) {
		*seen = (unsigned char *)xrealloc(*seen, (size_t)new_cap * sizeof(unsigned char));
		memset(*seen + old_cap, 0, (size_t)(new_cap - old_cap) * sizeof(unsigned char));
	}

	*cap = new_cap;
}

Node *
append_local_zero_fill(Node *head, const char *name, int offset, int bytes)
{
	Node *tail = stmt_node_list_tail(head);

	if (bytes <= 0)
		return head;

	if (bytes > 16) {
		char index_name[64];
		int index_offset;
		Node *index_decl;
		Node *index_lhs;
		Node *index_init;
		Node *cond_lhs;
		Node *cond;
		Node *store_index;
		Node *lhs;
		Node *store;
		Node *inc_lhs;
		Node *inc_rhs_lhs;
		Node *inc_rhs;
		Node *inc;
		Node *body;
		Node *loop;

		snprintf(index_name, sizeof(index_name), "__zero_i_%d",
		         parser_alloc_compound_arg_temp_id());
		parser_set_decl_align_request(0);
		index_offset = add_local(index_name);
		parser_clear_decl_align_request();

		index_decl = new_decl(index_name, index_offset);
		index_decl->type = type_int();
		index_decl->elem_size = TCC_SIZEOF_INT;
		index_decl->suppress_debug_loc = 1;

		index_lhs = new_var(index_name, index_offset);
		index_lhs->type = type_int();
		index_lhs->elem_size = TCC_SIZEOF_INT;
		index_lhs->suppress_debug_loc = 1;
		index_init = new_assign(index_lhs, new_num(0));
		index_init->suppress_debug_loc = 1;

		cond_lhs = new_var(index_name, index_offset);
		cond_lhs->type = type_int();
		cond_lhs->elem_size = TCC_SIZEOF_INT;
		cond_lhs->suppress_debug_loc = 1;
		cond = new_binary(ND_LT, cond_lhs, new_num(bytes));
		cond->suppress_debug_loc = 1;

		store_index = new_var(index_name, index_offset);
		store_index->type = type_int();
		store_index->elem_size = TCC_SIZEOF_INT;
		store_index->suppress_debug_loc = 1;
		lhs = new_index(name, offset, store_index);
		lhs->elem_size = 1;
		lhs->type = type_char();
		lhs->suppress_debug_loc = 1;
		store = new_assign(lhs, new_num(0));
		store->suppress_debug_loc = 1;

		inc_lhs = new_var(index_name, index_offset);
		inc_lhs->type = type_int();
		inc_lhs->elem_size = TCC_SIZEOF_INT;
		inc_lhs->suppress_debug_loc = 1;
		inc_rhs_lhs = new_var(index_name, index_offset);
		inc_rhs_lhs->type = type_int();
		inc_rhs_lhs->elem_size = TCC_SIZEOF_INT;
		inc_rhs_lhs->suppress_debug_loc = 1;
		inc_rhs = new_binary(ND_ADD, inc_rhs_lhs, new_num(1));
		inc_rhs->suppress_debug_loc = 1;
		inc = new_assign(inc_lhs, inc_rhs);
		inc->suppress_debug_loc = 1;

		store->next = inc;
		body = new_block(store);
		body->suppress_debug_loc = 1;
		loop = new_while(cond, body);
		loop->suppress_debug_loc = 1;

		index_decl->next = index_init;
		index_init->next = loop;
		stmt_append_node_to_tail(&head, &tail, new_block(index_decl));
		return head;
	}

	for (int i = 0; i < bytes; i++)
		stmt_append_node_to_tail(&head, &tail, make_local_array_store(name, offset, 1, i, 0));
	return head;
}

Node *
build_local_zero_fill_block(const char *name, int offset, int bytes, Node *decl)
{
	Node *head = decl;
	head = append_local_zero_fill(head, name, offset, bytes);
	return new_block(head);
}

static void
stmt_parse_braced_local_array_initializer(Type *elem_type, int pointer_elements,
	int array_len, int allow_enum_constants, int reject_multidim_designators,
	int *next_init_index, int *max_init_index, int *init_count,
	int *init_values_cap, int **init_values, int *init_exprs_cap,
	Node ***init_exprs, unsigned char **init_seen)
{
	reject_empty_initializer_before_c23();

	while (lexer_peek()->kind != TOK_RBRACE) {
		const Token *value;
		int init_value = 0;
		int lo = *next_init_index;
		int hi = *next_init_index;
		int has_designator;

		has_designator = stmt_try_parse_local_array_designator(&lo, &hi);
		if (has_designator && reject_multidim_designators)
			fatal_cur("Local array designators are only supported for one-dimensional arrays\n");
		if (lo < 0)
			fatal_cur("Array designator index out of range\n");
		if (hi < lo)
			fatal_cur("Invalid array designator range\n");
		if (array_len > 0 && hi >= array_len && has_designator)
			fatal_cur("Array designator index out of range\n");
		if (array_len > 0 && hi >= array_len)
			fatal_cur("Too many initializers for local array\n");

		value = lexer_peek();
		if (pointer_elements) {
			Node *expr = parse_local_scalar_initializer_expr(type_sizeof(elem_type));
			stmt_local_array_init_reserve(hi + 1, init_exprs_cap, NULL,
			                              init_exprs, init_seen);
			for (int di = lo; di <= hi; di++) {
				(*init_exprs)[di] = (di == lo) ? expr : clone_node_tree(expr);
				(*init_seen)[di] = 1;
			}
		} else {
			if (value->kind == TOK_NUM) {
				init_value = value->value;
			} else if (allow_enum_constants && value->kind == TOK_IDENT) {
				if (!parser_find_enum_const(value->text, &init_value))
					fatal_cur("Local array initializer must contain constant integers\n");
			} else {
				fatal_cur("Local array initializer must contain constant integers\n");
			}

			stmt_local_array_init_reserve(hi + 1, init_values_cap, init_values,
			                              NULL, init_seen);
			for (int di = lo; di <= hi; di++) {
				(*init_values)[di] = init_value;
				(*init_seen)[di] = 1;
			}
			lexer_next();
		}

		if (hi > *max_init_index)
			*max_init_index = hi;
		*next_init_index = hi + 1;
		*init_count = *max_init_index + 1;

		if (lexer_peek()->kind == TOK_COMMA)
			lexer_next();
		else
			break;
	}

	expect(TOK_RBRACE);
}

static int
stmt_local_array_flat_len(Type *type)
{
	int count = 1;

	for (Type *t = type; t && t->kind == TY_ARRAY; t = t->base) {
		if (t->array_len <= 0)
			return count;
		count *= t->array_len;
	}

	return count;
}

static int
stmt_local_array_leaf_elem_size(Type *type)
{
	Type *t = type;

	while (t && t->kind == TY_ARRAY)
		t = t->base;

	return t ? type_sizeof(t) : type_sizeof(type);
}

static int
stmt_string_literal_elem_width(const Token *value)
{
	return value->string_width > 1 ? value->string_width : 1;
}

static void
stmt_require_string_literal_array_match(const Token *value, int elem_size,
                                        const char *message)
{
	if (stmt_string_literal_elem_width(value) != elem_size)
		fatal_cur("%s\n", message);
}

static void
stmt_store_local_array_init_value(int pointer_elements, Type *elem_type,
	int allow_enum_constants, int index, int *init_values_cap, int **init_values,
	int *init_exprs_cap, Node ***init_exprs, unsigned char **init_seen,
	int *max_init_index, int *init_count)
{
	const Token *value = lexer_peek();

	if (pointer_elements) {
		Node *expr = parse_local_scalar_initializer_expr(type_sizeof(elem_type));
		stmt_local_array_init_reserve(index + 1, init_exprs_cap, NULL,
		                              init_exprs, init_seen);
		(*init_exprs)[index] = expr;
		(*init_seen)[index] = 1;
	} else {
		int init_value = 0;

		if (value->kind == TOK_NUM) {
			init_value = value->value;
			lexer_next();
		} else if (allow_enum_constants && value->kind == TOK_IDENT) {
			if (!parser_find_enum_const(value->text, &init_value))
				fatal_cur("Local array initializer must contain constant integers\n");
			lexer_next();
		} else {
			fatal_cur("Local array initializer must contain constant integers\n");
		}

		stmt_local_array_init_reserve(index + 1, init_values_cap, init_values,
		                              NULL, init_seen);
		(*init_values)[index] = init_value;
		(*init_seen)[index] = 1;
	}

	if (index > *max_init_index)
		*max_init_index = index;
	*init_count = *max_init_index + 1;
}

static void
stmt_copy_local_array_init_span(int pointer_elements,
	int dst_base_index, int span_len,
	int *dst_values_cap, int **dst_values,
	int *dst_exprs_cap, Node ***dst_exprs,
	unsigned char **dst_seen,
	const int *src_values, int src_values_cap,
	Node **src_exprs, int src_exprs_cap,
	const unsigned char *src_seen, int src_seen_cap,
	int *max_init_index, int *init_count)
{
	for (int i = 0; i < span_len; i++) {
		int dst_index = dst_base_index + i;
		int present = src_seen && i < src_seen_cap && src_seen[i];

		if (!present)
			continue;

		if (pointer_elements) {
			stmt_local_array_init_reserve(dst_index + 1, dst_exprs_cap, NULL,
			                              dst_exprs, dst_seen);
			(*dst_exprs)[dst_index] =
			    (src_exprs && i < src_exprs_cap && src_exprs[i])
			        ? clone_node_tree(src_exprs[i])
			        : NULL;
			(*dst_seen)[dst_index] = 1;
		} else {
			stmt_local_array_init_reserve(dst_index + 1, dst_values_cap, dst_values,
			                              NULL, dst_seen);
			(*dst_values)[dst_index] =
			    (src_values && i < src_values_cap) ? src_values[i] : 0;
			(*dst_seen)[dst_index] = 1;
		}

		if (dst_index > *max_init_index)
			*max_init_index = dst_index;
	}

	*init_count = *max_init_index + 1;
}

static void
stmt_parse_local_multidim_array_initializer(Type *array_type,
	int pointer_elements, int allow_enum_constants, int base_index,
	int *init_values_cap, int **init_values, int *init_exprs_cap,
	Node ***init_exprs, unsigned char **init_seen, int *max_init_index,
	int *init_count)
{
	int braced = 0;
	Type *elem_type;
	int elem_span;
	int next_init_index = 0;

	if (!array_type || array_type->kind != TY_ARRAY)
		fatal_cur("internal error: expected array type in local initializer\n");

	elem_type = parser_canonicalize_decl_type(array_type->base);
	elem_span = stmt_local_array_flat_len(array_type->base);

	if (lexer_peek()->kind == TOK_LBRACE) {
		braced = 1;
		lexer_next();
		reject_empty_initializer_before_c23();
		if (lexer_peek()->kind == TOK_RBRACE) {
			lexer_next();
			return;
		}
	}

	while (next_init_index < array_type->array_len) {
		int lo = next_init_index;
		int hi = next_init_index;
		int has_designator;

		if (braced && lexer_peek()->kind == TOK_RBRACE)
			break;

		has_designator = stmt_try_parse_local_array_designator(&lo, &hi);
		if (lo < 0)
			fatal_cur("Array designator index out of range\n");
		if (hi < lo)
			fatal_cur("Invalid array designator range\n");
		if (array_type->array_len > 0 && hi >= array_type->array_len && has_designator)
			fatal_cur("Array designator index out of range\n");
		if (array_type->array_len > 0 && hi >= array_type->array_len)
			fatal_cur("Too many initializers for local array\n");

		if (elem_type && elem_type->kind == TY_ARRAY) {
			int tmp_values_cap = 0;
			int tmp_exprs_cap = 0;
			int tmp_init_count = 0;
			int tmp_max_init_index = -1;
			int *tmp_values = NULL;
			Node **tmp_exprs = NULL;
			unsigned char *tmp_seen = NULL;

			stmt_parse_local_multidim_array_initializer(elem_type,
			                                            pointer_elements,
			                                            allow_enum_constants,
			                                            0,
			                                            &tmp_values_cap,
			                                            &tmp_values,
			                                            &tmp_exprs_cap,
			                                            &tmp_exprs,
			                                            &tmp_seen,
			                                            &tmp_max_init_index,
			                                            &tmp_init_count);
			for (int di = lo; di <= hi; di++) {
				stmt_copy_local_array_init_span(pointer_elements,
					base_index + di * elem_span, elem_span,
					init_values_cap, init_values,
					init_exprs_cap, init_exprs,
					init_seen,
					tmp_values, tmp_values_cap,
					tmp_exprs, tmp_exprs_cap,
					tmp_seen, tmp_max_init_index + 1,
					max_init_index, init_count);
			}

			xfree(tmp_values);
			xfree(tmp_exprs);
			xfree(tmp_seen);
		} else if (lexer_peek()->kind == TOK_LBRACE) {
			int tmp_values_cap = 0;
			int tmp_exprs_cap = 0;
			int tmp_init_count = 0;
			int tmp_max_init_index = -1;
			int *tmp_values = NULL;
			Node **tmp_exprs = NULL;
			unsigned char *tmp_seen = NULL;

			lexer_next();
			reject_empty_initializer_before_c23();
			stmt_store_local_array_init_value(pointer_elements, elem_type,
			                                  allow_enum_constants, 0,
			                                  &tmp_values_cap, &tmp_values,
			                                  &tmp_exprs_cap, &tmp_exprs,
			                                  &tmp_seen, &tmp_max_init_index,
			                                  &tmp_init_count);
			expect(TOK_RBRACE);
			for (int di = lo; di <= hi; di++) {
				stmt_copy_local_array_init_span(pointer_elements,
					base_index + di * elem_span, 1,
					init_values_cap, init_values,
					init_exprs_cap, init_exprs,
					init_seen,
					tmp_values, tmp_values_cap,
					tmp_exprs, tmp_exprs_cap,
					tmp_seen, tmp_max_init_index + 1,
					max_init_index, init_count);
			}
			xfree(tmp_values);
			xfree(tmp_exprs);
			xfree(tmp_seen);
		} else {
			const Token *value = lexer_peek();

			if (pointer_elements) {
				Node *expr = parse_local_scalar_initializer_expr(type_sizeof(elem_type));

				for (int di = lo; di <= hi; di++) {
					int dst_index = base_index + di * elem_span;

					stmt_local_array_init_reserve(dst_index + 1, init_exprs_cap, NULL,
					                              init_exprs, init_seen);
					(*init_exprs)[dst_index] = (di == lo)
					                         ? expr : clone_node_tree(expr);
					(*init_seen)[dst_index] = 1;
					if (dst_index > *max_init_index)
						*max_init_index = dst_index;
				}
			} else {
				int init_value = 0;

				if (value->kind == TOK_NUM) {
					init_value = value->value;
					lexer_next();
				} else if (allow_enum_constants && value->kind == TOK_IDENT) {
					if (!parser_find_enum_const(value->text, &init_value))
						fatal_cur("Local array initializer must contain constant integers\n");
					lexer_next();
				} else {
					fatal_cur("Local array initializer must contain constant integers\n");
				}

				for (int di = lo; di <= hi; di++) {
					int dst_index = base_index + di * elem_span;

					stmt_local_array_init_reserve(dst_index + 1, init_values_cap, init_values,
					                              NULL, init_seen);
					(*init_values)[dst_index] = init_value;
					(*init_seen)[dst_index] = 1;
					if (dst_index > *max_init_index)
						*max_init_index = dst_index;
				}
			}

			*init_count = *max_init_index + 1;
		}
		next_init_index = hi + 1;

		if (braced) {
			if (lexer_peek()->kind == TOK_COMMA) {
				lexer_next();
				if (lexer_peek()->kind == TOK_RBRACE)
					break;
			}
		} else if (next_init_index < array_type->array_len) {
			if (lexer_peek()->kind == TOK_COMMA)
				lexer_next();
			else if (lexer_peek()->kind == TOK_RBRACE)
				break;
		}
	}

	if (braced)
		expect(TOK_RBRACE);
}

static Node *
build_local_scalar_array_initializer_stmt(const char *name, int offset, Type *array_type,
                                          int base_size, Node *decl, Type *comma_base_type,
                                          int requested_align)
{
	int init_values_cap = 0;
	int init_count = 0;
	int *init_values = NULL;
	Node **init_exprs = NULL;
	int init_exprs_cap = 0;
	unsigned char *init_seen = NULL;
	int string_init = 0;
	int string_width = 1;
	int zero_fill_only = 0;
	char *string_value = NULL;
	size_t string_len = 0;
	int dim_count = 0;
	int total_len;
	int next_init_index = 0;
	int max_init_index = -1;
	Type *elem_type = array_type && array_type->base
	                ? parser_canonicalize_decl_type(array_type->base)
	                : type_for_size(base_size);
	int pointer_elements = elem_type && type_is_pointer(elem_type);
	Node head = {0};
	Node *cur = &head;

	for (Type *t = array_type; t && t->kind == TY_ARRAY; t = t->base)
		dim_count++;

	lexer_next();

	if (dim_count > 1 && lexer_peek()->kind == TOK_LBRACE) {
		stmt_parse_local_multidim_array_initializer(array_type,
		                                            pointer_elements,
		                                            0,
		                                            0,
		                                            &init_values_cap,
		                                            &init_values,
		                                            &init_exprs_cap,
		                                            &init_exprs,
		                                            &init_seen,
		                                            &max_init_index,
		                                            &init_count);
	} else if (lexer_peek()->kind == TOK_LBRACE) {
		lexer_next();
		stmt_parse_braced_local_array_initializer(elem_type, pointer_elements,
			array_type->array_len, 0, dim_count > 1,
			&next_init_index, &max_init_index, &init_count,
			&init_values_cap, &init_values, &init_exprs_cap, &init_exprs,
			&init_seen);
	} else if (lexer_peek()->kind == TOK_STRING) {
		const Token *value = lexer_peek();
		stmt_require_string_literal_array_match(value, base_size,
		                                        "String literal element width does not match local array element type");
		string_init = 1;
		string_width = stmt_string_literal_elem_width(value);
		string_len = value->text_len;
		string_value = xmalloc(string_len + 1);
		memcpy(string_value, value->text, string_len);
		string_value[string_len] = '\0';
		init_count = string_width > 1
			? (int)(string_len / (size_t)string_width) + 1
			: (int)string_len + 1;
		lexer_next();
	} else {
		fatal_cur("Unsupported local array initializer\n");
	}

	total_len = array_type->size / base_size;
	if (!zero_fill_only && init_count > total_len)
		fatal_cur("Too many initializers for local array\n");

	append_stmt(&cur, decl);

	if (zero_fill_only) {
		for (int i = 0; i < total_len; i++)
			append_stmt(&cur, make_local_array_store(name, offset, base_size, i, 0));
	} else if (string_init) {
		for (int i = 0; i < init_count; i++) {
			int val = 0;
			if (string_width > 1) {
				for (int bi = 0; bi < string_width; bi++) {
					size_t byte_idx = (size_t)i * (size_t)string_width + (size_t)bi;
					if (byte_idx < string_len)
						val |= (unsigned char)string_value[byte_idx] << (8 * bi);
				}
			} else {
				val = (unsigned char)string_value[i];
			}
			append_stmt(&cur, make_local_array_store(name, offset, base_size, i, val));
		}
	} else if (pointer_elements) {
		for (int i = 0; i < total_len; i++) {
			if (i < init_exprs_cap && init_seen && init_seen[i])
				append_stmt(&cur, make_local_array_assign_expr(name, offset, elem_type, i, init_exprs[i]));
			else
				append_stmt(&cur, make_local_array_assign_expr(name, offset, elem_type, i, new_num(0)));
		}
	} else {
		for (int i = 0; i < total_len; i++) {
			if (i < init_values_cap && init_seen && init_seen[i])
				append_stmt(&cur, make_local_array_store(name, offset, base_size, i, init_values[i]));
			else
				append_stmt(&cur, make_local_array_store(name, offset, base_size, i, 0));
		}
	}

	xfree(init_values);
	xfree(init_exprs);
	xfree(init_seen);
	xfree(string_value);
	return new_block(stmt_append_comma_after_array_initializer(head.next,
	                                                           comma_base_type,
	                                                           requested_align));
}

static Node *
stmt_append_comma_after_array_initializer(Node *head, Type *base_type,
                                          int requested_align)
{
	Node *tail = stmt_node_list_tail(head);

	while (lexer_peek()->kind == TOK_COMMA) {
		int ptr_depth = 0;
		const Token *name;
		Type *decl_type = clone_type(base_type);
		int offset;
		Node *decl;
		Node *entry;

		lexer_next();
		while (lexer_peek()->kind == TOK_STAR) {
			lexer_next();
			ptr_depth++;
		}

		name = lexer_peek();
		if (name->kind != TOK_IDENT)
			fatal_cur("Expected identifier in declaration list\n");
		lexer_next();

		for (int i = 0; i < ptr_depth; i++)
			decl_type = type_ptr(decl_type);

		if (lexer_peek()->kind == TOK_LPAREN) {
			if (!stmt_try_parse_function_declaration_after_name(
			        name->text, decl_type,
			        parser_type_name_saw_trailing_noreturn_specifier()))
				fatal_cur("internal error: expected block-scope function declarator\n");
			continue;
		}

		if (lexer_peek()->kind == TOK_LBRACKET) {
			int dims[MAX_ARRAY_DIMS] = {0};
			int dim_count = parse_array_dimensions(dims, 1, 0);
			Type *array_type = build_array_type_from_dims(decl_type, dims, dim_count);
			Type *elem_type = type_pointee(array_type);
			int elem_size = elem_type ? type_sizeof(elem_type) : type_elem_size(array_type);

			offset = add_decl_typed_local(requested_align, name->text, array_type);
			decl = make_decl_node_for_type(name->text, offset, array_type);
			entry = lexer_peek()->kind == TOK_ASSIGN
			      ? build_local_scalar_array_initializer_stmt(name->text, offset,
			                                                  array_type, elem_size,
			                                                  decl, base_type,
			                                                  requested_align)
			      : decl;
		} else if (ptr_depth == 0 && type_is_array(decl_type)) {
			offset = add_decl_typed_local(requested_align, name->text, decl_type);
			decl = make_decl_node_for_type(name->text, offset, decl_type);
			entry = lexer_peek()->kind == TOK_ASSIGN
			      ? build_local_scalar_array_initializer_stmt(name->text, offset,
			                                                  decl_type,
			                                                  type_sizeof(type_pointee(decl_type)),
			                                                  decl, decl_type,
			                                                  requested_align)
			      : decl;
		} else {
			offset = add_decl_typed_local(requested_align, name->text, decl_type);
			decl = make_decl_node_for_type(name->text, offset, decl_type);
			entry = decl;

			if (lexer_peek()->kind == TOK_ASSIGN) {
				Node *lhs;
				Node *expr;
				int init_size;

				lexer_next();
				init_size = type_sizeof(decl_type);
				expr = parse_local_scalar_initializer_expr(init_size > 0 ? init_size : 0);
				lhs = stmt_build_initializer_lhs(name->text, offset, decl_type,
				                                 type_is_pointer(decl_type));
				validate_pointer_initializer_compatibility(decl_type, expr);
				entry = append_node(entry, stmt_build_initializer_assign(lhs, expr,
				                                                       decl_type));
			}
		}

		if (tail)
			tail->next = entry;
		else
			head = entry;
		tail = stmt_node_list_tail(entry);
	}

	return head;
}

static Node *
build_local_struct_initializer_stmt(const char *name, int offset, Type *decl_type,
                                    Node *decl_node)
{
	StructDef *def;
	Node *lhs;

	if (!decl_type || !type_is_struct(decl_type))
		fatal_cur("aggregate initializer needs aggregate declarator type\n");

	def = find_struct(decl_type->struct_name);
	if (!def)
		fatal_cur("Unknown aggregate type in local initializer\n");
	if (def->has_flexible_array_member && tcc_iso_diagnostics)
		fatal_cur("initializer for struct with flexible array member is not supported\n");

	lhs = new_var(name, offset);
	lhs->type = def->is_union ? type_union(decl_type->struct_name, def->size)
	                          : type_struct(decl_type->struct_name, def->size);
	lhs->elem_size = def->size;
	STRNCPY(lhs->struct_name, decl_type->struct_name, sizeof(lhs->struct_name) - 1);

	lexer_next();

	if (lexer_peek()->kind != TOK_LBRACE) {
		Node *rhs = parse_expr();

		if (!rhs->type || !type_is_struct(rhs->type) ||
		    STRCMP(rhs->type->struct_name, decl_type->struct_name) != 0) {
			fatal_cur("Struct initializer expression type mismatch\n");
		}

		return new_block(append_node(decl_node, new_struct_assign(lhs, rhs, def->size)));
	}

	lexer_next();

	if (consume_all_zero_initializer())
		return build_local_zero_fill_block(name, offset, def->size, decl_node);

	Node *head = append_local_zero_fill(decl_node, name, offset, def->size);
	head = parse_struct_initializer_values(def, decl_type->struct_name, offset, head);
	parser_expect_local_aggregate_initializer_close(def);
	return new_block(head);
}

static Node *
make_pointer_index_store(const char *name, int offset, Type *ptr_type, Type *elem_type,
                         int elem_size, Node *index, Node *value)
{
	Node *base = new_var(name, offset);
	base->is_pointer = 1;
	base->elem_size = elem_size;
	base->type = clone_type(ptr_type);

	Node *addr = new_binary(ND_ADD, base, index);
	addr->is_pointer = 1;
	addr->elem_size = elem_size;
	addr->type = clone_type(ptr_type);

	Node *lhs = new_deref(addr);
	lhs->elem_size = elem_size;
	lhs->type = clone_type(elem_type);

	return new_assign(lhs, value);
}

static Node *
build_runtime_vla_zero_fill_loop(const char *name, int offset, Type *ptr_type, Type *elem_type,
                                 int elem_size, int start_index,
                                 const char *bound_name, int bound_offset)
{
	char index_name[64];
	int index_offset;
	Node *index_decl;
	Node *index_lhs;
	Node *index_init;
	Node *cond_lhs;
	Node *cond_rhs;
	Node *cond;
	Node *store_index;
	Node *store;
	Node *inc_lhs;
	Node *inc_rhs_lhs;
	Node *inc_rhs;
	Node *inc;
	Node *body;
	Node *loop;

	snprintf(index_name, sizeof(index_name), "__vla_init_i_%d",
	         parser_alloc_compound_arg_temp_id());
	parser_set_decl_align_request(0);
	index_offset = add_local(index_name);
	parser_clear_decl_align_request();

	index_decl = new_decl(index_name, index_offset);
	index_decl->type = type_int();
	index_decl->elem_size = TCC_SIZEOF_INT;
	index_decl->suppress_debug_loc = 1;

	index_lhs = new_var(index_name, index_offset);
	index_lhs->type = type_int();
	index_lhs->elem_size = TCC_SIZEOF_INT;
	index_lhs->suppress_debug_loc = 1;
	index_init = new_assign(index_lhs, new_num(start_index));
	index_init->suppress_debug_loc = 1;

	cond_lhs = new_var(index_name, index_offset);
	cond_lhs->type = type_int();
	cond_lhs->elem_size = TCC_SIZEOF_INT;
	cond_lhs->suppress_debug_loc = 1;
	cond_rhs = new_var(bound_name, bound_offset);
	cond_rhs->type = type_int();
	cond_rhs->elem_size = TCC_SIZEOF_INT;
	cond_rhs->suppress_debug_loc = 1;
	cond = new_binary(ND_LT, cond_lhs, cond_rhs);
	cond->suppress_debug_loc = 1;

	store_index = new_var(index_name, index_offset);
	store_index->type = type_int();
	store_index->elem_size = TCC_SIZEOF_INT;
	store_index->suppress_debug_loc = 1;
	store = make_pointer_index_store(name, offset, ptr_type, elem_type, elem_size,
	                                 store_index, new_num(0));
	store->suppress_debug_loc = 1;

	inc_lhs = new_var(index_name, index_offset);
	inc_lhs->type = type_int();
	inc_lhs->elem_size = TCC_SIZEOF_INT;
	inc_lhs->suppress_debug_loc = 1;
	inc_rhs_lhs = new_var(index_name, index_offset);
	inc_rhs_lhs->type = type_int();
	inc_rhs_lhs->elem_size = TCC_SIZEOF_INT;
	inc_rhs_lhs->suppress_debug_loc = 1;
	inc_rhs = new_binary(ND_ADD, inc_rhs_lhs, new_num(1));
	inc_rhs->suppress_debug_loc = 1;
	inc = new_assign(inc_lhs, inc_rhs);
	inc->suppress_debug_loc = 1;

	store->next = inc;
	body = new_block(store);
	body->suppress_debug_loc = 1;
	loop = new_while(cond, body);
	loop->suppress_debug_loc = 1;

	index_decl->next = index_init;
	index_init->next = loop;
	return new_block(index_decl);
}

static Node *
append_stmt(Node **cur, Node *stmt)
{
	Node *tail = *cur;

	tail->next = stmt;
	*cur = stmt;
	return stmt;
}

static Node *
stmt_node_list_tail(Node *node)
{
	if (!node)
		return NULL;
	while (node->next)
		node = node->next;
	return node;
}

static void
stmt_append_node_to_tail(Node **head, Node **tail, Node *node)
{
	Node *node_tail;

	if (!node)
		return;

	node_tail = stmt_node_list_tail(node);
	if (!*head)
		*head = node;
	else
		(*tail)->next = node;
	*tail = node_tail;
}

Type *
parse_enum_specifier(void)
{
	char enum_name[64] = {0};
	int has_body = 0;

	expect(TOK_ENUM);

	if (lexer_peek()->kind == TOK_IDENT) {
		STRNCPY(enum_name, lexer_peek()->text ? lexer_peek()->text : "",
		        sizeof(enum_name) - 1);
		lexer_next();
	}

	if (lexer_peek()->kind == TOK_LBRACE) {
		has_body = 1;
		if (enum_name[0]) {
			if (parser_trace_toplevel_enabled()) {
				fprintf(stderr, "tcc stmt: enum body enter name=%s before-define\n",
				        enum_name);
			}
			parser_define_enum_tag(enum_name);
			if (parser_trace_toplevel_enabled()) {
				fprintf(stderr, "tcc stmt: enum body enter name=%s after-define\n",
				        enum_name);
			}
		}
		lexer_next();
		if (lexer_peek()->kind == TOK_RBRACE)
			fatal_cur("enum must contain at least one enumerator\n");

		long long value = 0;
		while (lexer_peek()->kind != TOK_RBRACE) {
			const Token *name = lexer_peek();
			if (name->kind != TOK_IDENT) {
				fatal_cur("Expected enum constant name\n");
			}
			char enum_const_name[64] = {0};
			STRNCPY(enum_const_name, name->text ? name->text : "", sizeof(enum_const_name) - 1);
			if (parser_trace_toplevel_enabled()) {
				fprintf(stderr, "tcc stmt: enum const name=%s current_value=%lld\n",
				        enum_const_name, value);
			}

			lexer_next();

			if (lexer_peek()->kind == TOK_ASSIGN) {
				Node *expr;
				lexer_next();
				expr = fold_constants(parse_assignment());
				if (!expr || expr->kind != ND_NUM || expr->is_fp_num)
					fatal_cur("Enum value must be a constant integer\n");
				value = expr->long_value;
			}

			stmt_validate_enum_value(value);
			if (parser_trace_toplevel_enabled()) {
				fprintf(stderr, "tcc stmt: enum add const name=%s value=%lld before-add\n",
				        enum_const_name, value);
			}
			parser_add_enum_const(enum_const_name, (int)value);
			if (parser_trace_toplevel_enabled()) {
				fprintf(stderr, "tcc stmt: enum add const name=%s value=%lld after-add\n",
				        enum_const_name, value);
			}
			if (value == STMT_ENUM_INT_MAX &&
			    lexer_peek()->kind == TOK_COMMA &&
			    lexer_peek_ahead(1)->kind != TOK_RBRACE &&
			    lexer_peek_ahead(2)->kind != TOK_ASSIGN)
				fatal_cur("enumerator value is not representable as int\n");
			value++;

			if (lexer_peek()->kind == TOK_COMMA) {
				lexer_next();
				if (lexer_peek()->kind == TOK_RBRACE) {
					if (tcc_lang_is_c89_or_c90())
						fatal_cur("trailing comma in enum list is not allowed in C89/C90 mode\n");
					break;
				}
			} else {
				break;
			}
		}

		expect(TOK_RBRACE);
		if (parser_trace_toplevel_enabled()) {
			fprintf(stderr, "tcc stmt: enum body done name=%s\n",
			        enum_name[0] ? enum_name : "<anon>");
		}
	}

	if (!has_body) {
		if (!enum_name[0])
			fatal_cur("Expected enum tag or definition\n");
		if (!parser_has_visible_enum_tag(enum_name)) {
			if (parser_trace_toplevel_enabled()) {
				fprintf(stderr, "tcc stmt: enum forward declare name=%s\n",
				        enum_name);
			}
			parser_declare_enum_tag(enum_name);
		}
	}

	if (parser_trace_toplevel_enabled()) {
		fprintf(stderr, "tcc stmt: enum return type name=%s\n",
		        enum_name[0] ? enum_name : "<anon>");
	}
	return type_enum(enum_name);
}

static Type *
stmt_parse_typeof_specifier(void)
{
	Type *type = NULL;

	if (!token_is_typeof_keyword(lexer_peek()))
		fatal_cur("Expected typeof specifier\n");

	lexer_next();
	expect(TOK_LPAREN);

	if (is_type_start_token(lexer_peek()->kind, lexer_peek()->text)) {
		type = parse_type_name();
	} else {
		Node *expr = parse_expr();
		if (!expr || !expr->type)
			fatal_cur("cannot resolve typeof operand type\n");
		type = clone_type(expr->type);
	}

	expect(TOK_RPAREN);

	if (!type)
		fatal_cur("cannot resolve typeof result type\n");
	return type;
}

static int
stmt_union_has_flexible_array_member(const StructDef *def)
{
	if (!def)
		return 0;
	if (def->has_flexible_array_member)
		return 1;
	for (int i = 0; i < def->field_count; i++) {
		const Field *field = &def->fields[i];
		if (field->is_array && field->size == 0)
			return 1;
	}
	return 0;
}

Type *
parse_type_name(void)
{
	Type *type = NULL;
	Type *typedef_type = NULL;
	int base_qualifiers = 0;

	if (getenv("TCC_TRACE_PARSE_TOPLEVEL")) {
		const Token *tok = lexer_peek();
		fprintf(stderr,
		        "tcc stmt: parse-type entry kind=%s text=%s\n",
		        token_debug_name(tok->kind),
		        tok->text ? tok->text : "<null>");
	}

	parser_clear_trailing_decl_specifier_flags();
	parser_set_trailing_decl_specifier_tracking(0);
	parser_profile_scope_enter(PARSER_PROF_TYPE_NAME);
	parser_profile_scope_enter(PARSER_PROF_TYPE_BASE);

	base_qualifiers |= consume_type_qualifiers();
	stmt_skip_type_name_noise();
	base_qualifiers |= consume_type_qualifiers();
	if (getenv("TCC_TRACE_PARSE_TOPLEVEL")) {
		const Token *tok = lexer_peek();
		fprintf(stderr,
		        "tcc stmt: parse-type post-qual kind=%s text=%s quals=%d\n",
		        token_debug_name(tok->kind),
		        tok->text ? tok->text : "<null>",
		        base_qualifiers);
	}

	int saw_short_type_modifier = 0;
	int saw_signed_type_modifier = 0;
	int saw_unsigned_type_modifier = 0;
	int saw_int_modifier = 0;
	int long_count = 0;
	int source_kind = TYPE_SOURCE_DEFAULT;
	int complex_source_kind = TYPE_SOURCE_DEFAULT;
	char source_name[64] = {0};
	stmt_consume_scalar_type_modifiers(&saw_short_type_modifier,
	                                  &saw_signed_type_modifier,
	                                  &saw_unsigned_type_modifier,
	                                  &saw_int_modifier,
	                                  &long_count,
	                                  &complex_source_kind);

		const Token *token = lexer_peek();
		if (getenv("TCC_TRACE_PARSE_TOPLEVEL")) {
			fprintf(stderr,
			        "tcc stmt: parse-type token kind=%s text=%s mods short=%d signed=%d unsigned=%d intmod=%d long=%d\n",
			        token_debug_name(token->kind),
			        token->text ? token->text : "<null>",
			        saw_short_type_modifier,
			        saw_signed_type_modifier,
			        saw_unsigned_type_modifier,
			        saw_int_modifier,
			        long_count);
		}
		const Token *next;
		reject_c89_c99_keyword_token(token->kind);
		if (saw_int_modifier) {
			if (token->kind == TOK_VOID || token->kind == TOK_BOOL ||
			    token->kind == TOK_STRUCT || token->kind == TOK_UNION ||
			    token->kind == TOK_ENUM || token->kind == TOK_FLOAT)
				fatal_cur("invalid type specifier combination\n");
			if (token->kind == TOK_DOUBLE &&
			    (long_count != 1 || saw_short_type_modifier ||
			     saw_signed_type_modifier || saw_unsigned_type_modifier))
				fatal_cur("invalid type specifier combination\n");
		}

	if (long_count >= 2) {
		source_kind = saw_unsigned_type_modifier ? TYPE_SOURCE_ULLONG : TYPE_SOURCE_LLONG;
		STRNCPY(source_name, saw_unsigned_type_modifier ? "unsigned long long" : "long long", sizeof(source_name) - 1);
	} else if (long_count == 1) {
		source_kind = saw_unsigned_type_modifier ? TYPE_SOURCE_ULONG : TYPE_SOURCE_LONG;
		STRNCPY(source_name, saw_unsigned_type_modifier ? "unsigned long" : "long", sizeof(source_name) - 1);
	}

		if (token->kind == TOK_VOID) {
			lexer_next();
			type = type_void();
		} else if (token_is_typeof_keyword(token)) {
			type = stmt_parse_typeof_specifier();
		} else if (token->kind == TOK_ATOMIC && lexer_peek_ahead(1)->kind == TOK_LPAREN) {
			reject_c89_c99_keyword_token(token->kind);
			lexer_next();
			expect(TOK_LPAREN);
			type = parse_type_name();
			type = stmt_apply_abstract_type_suffixes(type, 0);
			expect(TOK_RPAREN);
			if (type && type->kind == TY_ARRAY)
				fatal_cur("atomic type specifier cannot be applied to array type\n");
			if (type && type->kind == TY_FUNC)
				fatal_cur("atomic type specifier cannot be applied to function type\n");
			if (type && (type_qualifiers(type) & TYPE_QUAL_ATOMIC))
				fatal_cur("atomic type specifier cannot be applied to atomic type\n");
			type = type_with_qualifiers(type, type_qualifiers(type) | TYPE_QUAL_ATOMIC);
		} else if (token->kind == TOK_INT) {
			lexer_next();
			int sz = long_count > 0 ? 8 : (saw_short_type_modifier ? 2 : 4);
			type = type_for_size_unsigned(sz, saw_unsigned_type_modifier);
		} else if (token->kind == TOK_BOOL || token_is_c23_bool_keyword(token)) {
			if (token_is_c23_bool_keyword(token))
				reject_plain_bool_keyword_before_c23(token);
			lexer_next();
			type = type_with_source(type_uchar(), TYPE_SOURCE_BOOL,
			                        token->kind == TOK_BOOL ? "_Bool" : "bool");
		} else if (token->kind == TOK_CHAR) {
			lexer_next();
			if (saw_unsigned_type_modifier)
				type = type_uchar();
			else if (saw_signed_type_modifier)
				type = type_with_source(type_char(), TYPE_SOURCE_SCHAR, "signed char");
			else
				type = type_char();
	} else if (token->kind == TOK_STRUCT || token->kind == TOK_UNION) {
		int type_is_union = (token->kind == TOK_UNION);

		parser_profile_scope_enter(PARSER_PROF_TYPE_RECORD);
		lexer_next();

		const Token *name = lexer_peek();
		char struct_type_name[64] = {0};
		if (name->kind == TOK_LBRACE) {
			/* Anonymous struct/union as a field type */
			snprintf(struct_type_name, sizeof(struct_type_name),
			         "__anon_%s_%d", type_is_union ? "union" : "struct",
			         ++parser_anon_struct_id);
			if (parser_has_struct_capacity()) {
				StructDef *adef = structs_push();
				memset(adef, 0, sizeof(*adef));
				STRNCPY(adef->name, struct_type_name, sizeof(adef->name) - 1);
				adef->is_union = type_is_union;
				parse_struct_body_into(adef);
				if (type_is_union && stmt_union_has_flexible_array_member(adef))
					fatal_cur("Flexible array member not allowed in union\n");
			}
		} else if (name->kind == TOK_IDENT) {
			STRNCPY(struct_type_name, name->text ? name->text : "", sizeof(struct_type_name) - 1);
			lexer_next();
			/* Inline definition: "struct S { ... }" as a type */
			if (lexer_peek()->kind == TOK_LBRACE) {
				StructDef *idef = get_or_add_forward_struct(struct_type_name);
				memset(idef, 0, sizeof(*idef));
				STRNCPY(idef->name, struct_type_name, sizeof(idef->name) - 1);
				if (idef) {
					idef->is_union = type_is_union;
					parse_struct_body_into(idef);
					if (type_is_union && stmt_union_has_flexible_array_member(idef))
						fatal_cur("Flexible array member not allowed in union\n");
				}
			}
		} else {
			fatal_cur("Expected struct name in type\n");
		}

			StructDef *def = find_struct_or_null(struct_type_name);
			if (!def)
				def = get_or_add_forward_struct(struct_type_name);
			if (type_is_union)
				type = type_union(struct_type_name, def->size);
			else
				type = type_struct(struct_type_name, def->size);
			parser_profile_scope_leave(PARSER_PROF_TYPE_RECORD);

		} else if (token->kind == TOK_FLOAT) {
			lexer_next();
			type = type_float();
			if (complex_source_kind == TYPE_SOURCE_COMPLEX)
				STRNCPY(source_name, "_Complex float", sizeof(source_name) - 1);
			else if (complex_source_kind == TYPE_SOURCE_IMAGINARY)
				STRNCPY(source_name, "_Imaginary float", sizeof(source_name) - 1);
	} else if (token->kind == TOK_DOUBLE) {
		lexer_next();
		type = type_double();
		if (long_count == 1) {
			source_kind = TYPE_SOURCE_LONG_DOUBLE;
			STRNCPY(source_name, "long double", sizeof(source_name) - 1);
		} else {
			source_kind = TYPE_SOURCE_DEFAULT;
			source_name[0] = '\0';
		}
		if (complex_source_kind == TYPE_SOURCE_COMPLEX)
			STRNCPY(source_name, long_count == 1 ? "_Complex long double" : "_Complex double",
			        sizeof(source_name) - 1);
		else if (complex_source_kind == TYPE_SOURCE_IMAGINARY)
			STRNCPY(source_name, long_count == 1 ? "_Imaginary long double" : "_Imaginary double",
			        sizeof(source_name) - 1);
		} else if (token->kind == TOK_ENUM) {
			parser_profile_scope_enter(PARSER_PROF_TYPE_ENUM);
			type = parse_enum_specifier();
			parser_profile_scope_leave(PARSER_PROF_TYPE_ENUM);
		} else if (saw_int_modifier &&
		           token->kind == TOK_DOUBLE &&
		           long_count == 1 &&
		           !saw_short_type_modifier &&
		           !saw_signed_type_modifier &&
		           !saw_unsigned_type_modifier) {
			lexer_next();
			type = type_double();
			source_kind = TYPE_SOURCE_DEFAULT;
			source_name[0] = '\0';
		} else if (saw_int_modifier) {
		/*
		 * If we already consumed integer declaration specifiers such as
		 * "unsigned" or "long", the following identifier is the declarator
		 * name, even if that spelling is already known as a typedef.
		 *
		 * This permits valid redeclarations like:
		 *
		 *     typedef unsigned long size_t;
		 *
		 * after a system header has already typedef'd size_t. The old ordering
		 * treated the second size_t as another type-name, consumed it here, and
		 * then parse_typedef_declaration() failed at the semicolon with
		 * "Expected typedef name".
		 */
			type = type_for_size_unsigned(long_count > 0 ? 8 : (saw_short_type_modifier ? 2 : 4),
			                             saw_unsigned_type_modifier);
		} else if (token->kind == TOK_IDENT) {
			if (token->text && parser_has_visible_enum_tag(token->text)) {
				char typedef_name[64] = {0};

				if (getenv("TCC_TRACE_PARSE_TOPLEVEL")) {
					fprintf(stderr,
					        "tcc stmt: parse-type ident enum-tag token=%s\n",
					        token->text);
				}
				parser_profile_scope_enter(PARSER_PROF_TYPE_TYPEDEF);
				STRNCPY(typedef_name, token->text ? token->text : "", sizeof(typedef_name) - 1);
				lexer_next();
				type = stmt_clone_typedef_type(type_enum(typedef_name), typedef_name);
				if (getenv("TCC_TRACE_PARSE_TOPLEVEL")) {
					fprintf(stderr,
					        "tcc stmt: parse-type ident enum-tag cloned typedef_name=%s type=%p kind=%d\n",
					        typedef_name, (void *)type, type ? type->kind : -1);
				}
				parser_profile_scope_leave(PARSER_PROF_TYPE_TYPEDEF);
			} else {
			if (getenv("TCC_TRACE_PARSE_TOPLEVEL")) {
				fprintf(stderr,
				        "tcc stmt: parse-type ident token=%s\n",
				        token->text ? token->text : "<null>");
			}
			if (token->text)
				typedef_type = parser_find_typedef(token->text);
			if (getenv("TCC_TRACE_PARSE_TOPLEVEL")) {
				fprintf(stderr,
				        "tcc stmt: parse-type ident lookup token=%s typedef_type=%p\n",
				        token->text ? token->text : "<null>", (void *)typedef_type);
			}
			if (typedef_type) {
				char typedef_name[64] = {0};
				parser_profile_scope_enter(PARSER_PROF_TYPE_TYPEDEF);
				STRNCPY(typedef_name, token->text ? token->text : "", sizeof(typedef_name) - 1);
				lexer_next();
				if (getenv("TCC_TRACE_PARSE_TOPLEVEL")) {
					fprintf(stderr,
					        "tcc stmt: parse-type ident clone typedef_name=%s typedef_type=%p kind=%d\n",
					        typedef_name, (void *)typedef_type, typedef_type ? typedef_type->kind : -1);
				}
				type = stmt_clone_typedef_type(typedef_type, typedef_name);
				if (getenv("TCC_TRACE_PARSE_TOPLEVEL")) {
					fprintf(stderr,
					        "tcc stmt: parse-type ident cloned typedef_name=%s type=%p kind=%d\n",
					        typedef_name, (void *)type, type ? type->kind : -1);
				}
				parser_profile_scope_leave(PARSER_PROF_TYPE_TYPEDEF);
			} else {
				fatal_cur("Expected type name\n");
			}
			}
		} else {
			fatal_cur("Expected type name\n");
		}

			if (complex_source_kind != TYPE_SOURCE_DEFAULT) {
				if (!type || !(type->kind == TY_FLOAT || type->kind == TY_DOUBLE))
					fatal_cur("invalid type specifier combination\n");
				type = stmt_apply_type_source(type, complex_source_kind, source_name);
			} else if (type && source_kind != TYPE_SOURCE_DEFAULT)
				type = stmt_apply_type_source(type, source_kind, source_name);
			type = stmt_apply_post_base_complex_specifier(type,
			                                             &complex_source_kind,
			                                             long_count);
			if (type && base_qualifiers)
				type = type_with_qualifiers(type, base_qualifiers);
			if (token_is_plain_type_specifier(lexer_peek()->kind))
				fatal_cur("invalid type specifier combination\n");

	next = lexer_peek();
	if (!stmt_type_suffix_needs_scan(next)) {
			parser_set_trailing_decl_specifier_tracking(0);
			parser_profile_scope_leave(PARSER_PROF_TYPE_BASE);
			parser_profile_scope_leave(PARSER_PROF_TYPE_NAME);
			return type;
		}

	parser_set_trailing_decl_specifier_tracking(1);
	base_qualifiers = 0;
	base_qualifiers |= consume_type_qualifiers();
	stmt_skip_type_name_noise();
	base_qualifiers |= consume_type_qualifiers();
	if (type && base_qualifiers)
		type = type_with_qualifiers(type, base_qualifiers);

	parser_profile_scope_leave(PARSER_PROF_TYPE_BASE);
	parser_profile_scope_enter(PARSER_PROF_TYPE_POINTER);
	while (lexer_peek()->kind == TOK_STAR) {
		int pointer_qualifiers = 0;

		lexer_next();
		type = type_ptr(type);
		if (stmt_type_suffix_needs_scan(lexer_peek())) {
			pointer_qualifiers |= consume_type_qualifiers();
			skip_pointer_qualifiers();
			pointer_qualifiers |= consume_type_qualifiers();
			stmt_skip_type_name_noise();
			pointer_qualifiers |= consume_type_qualifiers();
		}
		type = type_with_qualifiers(type, pointer_qualifiers);
	}

	if (stmt_type_suffix_needs_scan(lexer_peek())) {
		(void)consume_type_qualifiers();
		stmt_skip_type_name_noise();
		skip_pointer_qualifiers();
	}
	parser_set_trailing_decl_specifier_tracking(0);
	parser_profile_scope_leave(PARSER_PROF_TYPE_POINTER);

	parser_profile_scope_leave(PARSER_PROF_TYPE_NAME);
	return type;
}

int
is_type_start_token(TokenKind kind, const char *text)
{
	return kind == TOK_INT ||
	       kind == TOK_CHAR ||
	       kind == TOK_VOID ||
	       kind == TOK_BOOL ||
	       kind == TOK_SHORT ||
	       kind == TOK_LONG ||
	       kind == TOK_SIGNED ||
	       kind == TOK_UNSIGNED ||
	       kind == TOK_FLOAT ||
	       kind == TOK_DOUBLE ||
	       kind == TOK_STRUCT ||
	       kind == TOK_UNION ||
	       kind == TOK_ENUM ||
	       kind == TOK_STATIC ||
	       kind == TOK_EXTERN ||
	       kind == TOK_CONST ||
	       kind == TOK_VOLATILE ||
	       kind == TOK_RESTRICT ||
	       kind == TOK_ATOMIC ||
	       kind == TOK_INLINE ||
	       kind == TOK_NORETURN ||
	       (kind == TOK_IDENT && text && tcc_lang_at_least(LANG_C99) &&
	        (STRCMP(text, "_Complex") == 0 ||
	         STRCMP(text, "_Imaginary") == 0)) ||
	       (kind == TOK_IDENT && text &&
	        (STRCMP(text, "__typeof__") == 0 ||
	         STRCMP(text, "typeof") == 0)) ||
	       (kind == TOK_IDENT && text && tcc_lang_at_least(LANG_C23) &&
	        STRCMP(text, "bool") == 0) ||
	       (kind == TOK_IDENT && text && STRCMP(text, "__attribute__") == 0) ||
	       (kind == TOK_IDENT && parser_is_typedef_name(text));
}

static Type *
parse_typedef_alias_type(Type *base_type)
{
	Type *alias_type = base_type;

	return stmt_apply_abstract_type_suffixes(alias_type, 1);
}

static Node *
stmt_try_parse_local_vm_array_typedef(Type *type, int requested_align)
{
	const Token *alias;
	char bound_name[64];
	int bound_offset;
	Node *bound_decl;
	Node *bound_lhs;
	Node *bound_assign;
	Type *elem_type;
	Type *array_type;
	int elem_size;
	int tail_dims[MAX_ARRAY_DIMS] = {0};
	int tail_dim_count = 0;
	Node *expr;

	if (lexer_peek()->kind != TOK_IDENT)
		return NULL;
	if (lexer_peek_ahead(1)->kind != TOK_LBRACKET ||
	    !array_decl_looks_runtime_vla())
		return NULL;

	alias = lexer_peek();
	lexer_next();

	expect(TOK_LBRACKET);
	if (lexer_peek()->kind == TOK_RBRACKET)
		fatal_cur("runtime VLA requires a bound expression\n");
	expr = parse_assignment();
	expect(TOK_RBRACKET);

	elem_type = clone_type(type);
	if (lexer_peek()->kind == TOK_LBRACKET) {
		if (array_decl_looks_runtime_vla())
			fatal_cur("only the first dimension of a runtime VLA may be variably modified\n");
		tail_dim_count = parse_array_dimensions(tail_dims, 0, 0);
		if (tail_dim_count > 0)
			elem_type = build_array_type_from_dims(elem_type, tail_dims, tail_dim_count);
	}

	elem_size = type_sizeof(elem_type);
	if (elem_size <= 0)
		fatal_cur("runtime VLA element type must be complete\n");

	snprintf(bound_name, sizeof(bound_name), "__vla_len_%d",
	         parser_alloc_compound_arg_temp_id());
	parser_set_decl_align_request(requested_align);
	bound_offset = add_local(bound_name);
	parser_clear_decl_align_request();

	bound_decl = new_decl(bound_name, bound_offset);
	bound_decl->type = type_int();
	bound_decl->elem_size = TCC_SIZEOF_INT;
	bound_decl->suppress_debug_loc = 1;

	bound_lhs = new_var(bound_name, bound_offset);
	bound_lhs->type = type_int();
	bound_lhs->elem_size = TCC_SIZEOF_INT;
	bound_lhs->suppress_debug_loc = 1;
	bound_assign = new_assign(bound_lhs, expr);
	bound_assign->suppress_debug_loc = 1;

	array_type = type_array(elem_type, 0);
	array_type->is_vm_type = 1;
	STRNCPY(array_type->vla_bound_name, bound_name,
	        sizeof(array_type->vla_bound_name) - 1);
	array_type->vla_elem_type = clone_type(elem_type);

	expect(TOK_SEMI);
	parser_add_typedef_name(alias->text, array_type);

	return new_block(append_node(bound_decl, bound_assign));
}

static Node *
stmt_try_parse_local_vm_pointer_array_typedef(Type *type, int requested_align)
{
	const Token *alias;
	const Token *tok;
	Type *elem_type;
	int elem_size;
	int tail_dims[MAX_ARRAY_DIMS] = {0};
	int tail_dim_count = 0;
	char bound_name[64];
	int bound_offset;
	int bracket_depth;
	int saw_runtime_bound;
	int i;
	Node *bound_decl;
	Node *bound_lhs;
	Node *bound_assign;
	Type *array_type;
	Type *ptr_type;
	Node *expr;

	if (lexer_peek()->kind != TOK_LPAREN || lexer_peek_ahead(1)->kind != TOK_STAR)
		return NULL;

	tok = lexer_peek_ahead(2);
	if (tok->kind != TOK_IDENT)
		return NULL;
	if (lexer_peek_ahead(3)->kind != TOK_RPAREN ||
	    lexer_peek_ahead(4)->kind != TOK_LBRACKET)
		return NULL;

	bracket_depth = 0;
	saw_runtime_bound = 0;
	for (i = 4;; i++) {
		tok = lexer_peek_ahead(i);
		if (tok->kind == TOK_EOF)
			return NULL;
		if (tok->kind == TOK_LBRACKET) {
			bracket_depth++;
			continue;
		}
		if (tok->kind == TOK_RBRACKET) {
			bracket_depth--;
			if (bracket_depth == 0)
				break;
			continue;
		}
		if (bracket_depth != 1)
			continue;
		if (tok->kind == TOK_STAR) {
			saw_runtime_bound = 1;
			break;
		}
		if (tok->kind == TOK_IDENT) {
			int enum_value = 0;
			if (!parser_find_enum_const(tok->text, &enum_value)) {
				saw_runtime_bound = 1;
				break;
			}
		}
	}
	if (!saw_runtime_bound)
		return NULL;

	lexer_next();
	lexer_next();
	skip_pointer_qualifiers();
	alias = lexer_peek();
	if (alias->kind != TOK_IDENT)
		fatal_cur("Expected typedef function pointer name\n");
	lexer_next();
	expect(TOK_RPAREN);
	if (lexer_peek()->kind != TOK_LBRACKET || !array_decl_looks_runtime_vla())
		return NULL;

	expect(TOK_LBRACKET);
	if (lexer_peek()->kind == TOK_RBRACKET)
		fatal_cur("runtime VLA requires a bound expression\n");
	expr = parse_assignment();
	expect(TOK_RBRACKET);

	elem_type = clone_type(type);
	if (lexer_peek()->kind == TOK_LBRACKET) {
		if (array_decl_looks_runtime_vla())
			fatal_cur("only the first dimension of a runtime VLA may be variably modified\n");
		tail_dim_count = parse_array_dimensions(tail_dims, 0, 0);
		if (tail_dim_count > 0)
			elem_type = build_array_type_from_dims(elem_type, tail_dims, tail_dim_count);
	}

	elem_size = type_sizeof(elem_type);
	if (elem_size <= 0)
		fatal_cur("runtime VLA element type must be complete\n");

	snprintf(bound_name, sizeof(bound_name), "__vla_len_%d",
	         parser_alloc_compound_arg_temp_id());
	parser_set_decl_align_request(requested_align);
	bound_offset = add_local(bound_name);
	parser_clear_decl_align_request();

	bound_decl = new_decl(bound_name, bound_offset);
	bound_decl->type = type_int();
	bound_decl->elem_size = TCC_SIZEOF_INT;
	bound_decl->suppress_debug_loc = 1;

	bound_lhs = new_var(bound_name, bound_offset);
	bound_lhs->type = type_int();
	bound_lhs->elem_size = TCC_SIZEOF_INT;
	bound_lhs->suppress_debug_loc = 1;
	bound_assign = new_assign(bound_lhs, expr);
	bound_assign->suppress_debug_loc = 1;

	array_type = type_array(elem_type, 0);
	array_type->is_vm_type = 1;
	STRNCPY(array_type->vla_bound_name, bound_name,
	        sizeof(array_type->vla_bound_name) - 1);
	array_type->vla_elem_type = clone_type(elem_type);
	ptr_type = type_ptr(array_type);

	expect(TOK_SEMI);
	parser_add_typedef_name(alias->text, ptr_type);

	return new_block(append_node(bound_decl, bound_assign));
}

int
parse_typedef_declaration_after_base_type(Type *type)
{
	Node *vm_typedef = NULL;

	if (token_starts_alignas_specifier(lexer_peek(), lexer_peek_ahead(1))) {
		parse_alignment_specifiers();
		fatal_cur("alignment specifier cannot be applied to a typedef declaration\n");
	}

	if (!stmt_typedef_is_file_scope()) {
		vm_typedef = stmt_try_parse_local_vm_array_typedef(type, 0);
		if (vm_typedef) {
			stmt_last_typedef_decl_node = vm_typedef;
			return 1;
		}
		vm_typedef = stmt_try_parse_local_vm_pointer_array_typedef(type, 0);
		if (vm_typedef) {
			stmt_last_typedef_decl_node = vm_typedef;
			return 1;
		}
	}

	/* Function pointer typedef: "typedef int (*fptr)(int);" */
	if (lexer_peek()->kind == TOK_LPAREN && lexer_peek_ahead(1)->kind == TOK_STAR) {
		lexer_next(); /* ( */
		lexer_next(); /* * */
		const Token *fp_alias = lexer_peek();
		if (fp_alias->kind != TOK_IDENT) {
			fatal_cur("Expected typedef function pointer name\n");
		}
		char fp_tname[64] = {0};
		STRNCPY(fp_tname, fp_alias->text, sizeof(fp_tname) - 1);
		lexer_next(); /* name */

		/*
		 * C declarator suffixes bind to the identifier even when the
		 * identifier is inside the parenthesized pointer declarator:
		 *
		 *     typedef int (*fptr[4])(int);
		 *
		 * This is an array of function pointers, not a function pointer
		 * named fptr followed by a stray '[' token.
		 */
		int fp_dims[MAX_ARRAY_DIMS] = {0};
		int fp_dim_count = 0;
		if (lexer_peek()->kind == TOK_LBRACKET) {
			stmt_reject_file_scope_vm_typedef_array_bound();
			fp_dim_count = parse_array_dimensions(fp_dims, 0, 0);
		}

		expect(TOK_RPAREN);
		if (lexer_peek()->kind == TOK_LBRACKET) {
			int outer_dims[MAX_ARRAY_DIMS] = {0};
			int outer_dim_count = 0;
			Type *ptr_type;

			if (stmt_typedef_is_file_scope() && array_decl_looks_runtime_vla())
				fatal_cur("file-scope typedef array bound must be an integer constant expression\n");

			outer_dim_count = parse_array_dimensions(outer_dims, 0, 0);
			expect(TOK_SEMI);

			ptr_type = type_ptr(build_array_type_from_dims(clone_type(type),
			                                               outer_dims,
			                                               outer_dim_count));
			if (fp_dim_count > 0)
				ptr_type = build_array_type_from_dims(ptr_type, fp_dims, fp_dim_count);
			parser_add_typedef_name(fp_tname, ptr_type);
			return 1;
		}
		Type **fp_param_types = NULL;
		int fp_param_count = 0;
		int fp_is_variadic = 0;
		int fp_fixed_params = 0;
		int fp_has_prototype = 0;

		if (lexer_peek()->kind == TOK_LPAREN) {
			parse_prototype_param_list(&fp_param_types, &fp_param_count,
			                          &fp_is_variadic, &fp_fixed_params,
			                          &fp_has_prototype, 1);
		}
		expect(TOK_SEMI);
		Type *fp_type = type_ptr(fp_has_prototype
		                         ? parser_make_function_type(type, fp_param_types,
		                                                     fp_param_count,
		                                                     fp_is_variadic,
		                                                     fp_fixed_params)
		                         : type_func(clone_type(type)));
		if (fp_dim_count > 0)
			fp_type = build_array_type_from_dims(fp_type, fp_dims, fp_dim_count);
		parser_add_typedef_name(fp_tname, fp_type);
		return 1;
	}

	const Token *name = lexer_peek();
	if (getenv("TCC_TRACE_PARSE_TOPLEVEL")) {
		fprintf(stderr,
		        "tcc stmt: typedef-after-base next-kind=%s text=%s base-kind=%d\n",
		        token_debug_name(name->kind),
		        name->text ? name->text : "<null>",
		        type ? type->kind : -1);
	}
	if (name->kind != TOK_IDENT) {
		fatal_cur("Expected typedef name\n");
	}
	char tdef_name[64] = {0};
	STRNCPY(tdef_name, name->text ? name->text : "", sizeof(tdef_name) - 1);

	lexer_next();
	type = parse_typedef_alias_type(type);
	expect(TOK_SEMI);
	if (getenv("TCC_TRACE_PARSE_TOPLEVEL")) {
		fprintf(stderr,
		        "tcc stmt: typedef-after-base add name=%s final-kind=%d type=%p\n",
		        tdef_name, type ? type->kind : -1, (void *)type);
	}

	parser_add_typedef_name(tdef_name, type);
	return 1;
}

int
parse_typedef_declaration(void)
{
	if (lexer_peek()->kind != TOK_TYPEDEF) {
		return 0;
	}

	lexer_next();

	if (lexer_peek()->kind == TOK_STATIC ||
	    lexer_peek()->kind == TOK_EXTERN ||
	    lexer_peek()->kind == TOK_AUTO ||
	    lexer_peek()->kind == TOK_REGISTER)
		fatal_cur("multiple storage classes in declaration\n");
	if (lexer_peek()->kind == TOK_THREAD_LOCAL) {
		reject_thread_local_storage_specifier();
		fatal_cur("multiple storage classes in declaration\n");
	}

	if (token_starts_alignas_specifier(lexer_peek(), lexer_peek_ahead(1))) {
		parse_alignment_specifiers();
		fatal_cur("alignment specifier cannot be applied to a typedef declaration\n");
	}

	if (lexer_peek()->kind == TOK_INLINE)
		fatal_cur("function specifier is only valid on function declarations\n");
	if (lexer_peek()->kind == TOK_NORETURN)
		fatal_cur("function specifier is only valid on function declarations\n");

	if (lexer_peek()->kind == TOK_STRUCT || lexer_peek()->kind == TOK_UNION) {
		int is_union = (lexer_peek()->kind == TOK_UNION);
		lexer_next(); /* consume struct/union keyword */

		/* Skip __attribute__((packed)) etc. */
		skip_inline_qualifiers();

		char struct_name[64] = {0};
		int has_tag = 0;

		if (lexer_peek()->kind == TOK_IDENT) {
			const Token *tag = lexer_peek();
			STRNCPY(struct_name, tag->text, sizeof(struct_name) - 1);
			has_tag = 1;
			lexer_next();
		}

		if (lexer_peek()->kind == TOK_LBRACE) {
			{
				StructDef *def;
				if (has_tag) {
					def = get_or_add_forward_struct(struct_name);
				} else {
					def = structs_push();
					memset(def, 0, sizeof(*def));
				}
				def->is_union = is_union;

				parse_struct_body_into(def);

				/* Accept attributes between a struct/union body and its typedef name:
				 *   typedef union U { ... } __attribute__((packed)) U;
				 */
				skip_inline_qualifiers();

				const Token *alias = lexer_peek();
				if (alias->kind != TOK_IDENT) {
					fatal_cur("Expected typedef name after struct body\n");
				}

				if (!has_tag) {
					STRNCPY(def->name, alias->text, sizeof(def->name) - 1);
					STRNCPY(struct_name, alias->text, sizeof(struct_name) - 1);
				}

				lexer_next();
				Type *alias_type = parse_typedef_alias_type(def->is_union
				                                            ? type_union(struct_name, def->size)
				                                            : type_struct(struct_name, def->size));
				expect(TOK_SEMI);
				parser_add_typedef_name(alias->text,
				                        alias_type);
				return 1;
			}
		}

		/* Handle pointer/function-pointer typedef: "typedef struct S * alias;" or
		 * "typedef struct S * (*fty)();" */
		int ptr_depth = 0;
		while (lexer_peek()->kind == TOK_STAR) { lexer_next(); ptr_depth++; }

		/* Function pointer typedef: "typedef struct S * (*fty)(...);" */
		if (lexer_peek()->kind == TOK_LPAREN && lexer_peek_ahead(1)->kind == TOK_STAR) {
			lexer_next(); /* ( */
			lexer_next(); /* * */
			const Token *fp_alias = lexer_peek();
			if (fp_alias->kind != TOK_IDENT)
				fatal_cur("Expected typedef function pointer name\n");
			char fp_name[64] = {0};
			STRNCPY(fp_name, fp_alias->text, sizeof(fp_name) - 1);
			lexer_next(); /* name */
			expect(TOK_RPAREN);
			Type **fp_param_types = NULL;
			int fp_param_count = 0;
			int fp_is_variadic = 0;
			int fp_fixed_params = 0;
			int fp_has_prototype = 0;

			parse_prototype_param_list(&fp_param_types, &fp_param_count,
			                          &fp_is_variadic, &fp_fixed_params,
			                          &fp_has_prototype, 1);
			expect(TOK_SEMI);
			/* Register as pointer type (function pointers are 8-byte pointers) */
			StructDef *def = get_or_add_forward_struct(struct_name);
			Type *ret_type = def->is_union ? type_union(struct_name, def->size)
			                              : type_struct(struct_name, def->size);
			for (int pi = 0; pi < ptr_depth; pi++)
				ret_type = type_ptr(ret_type);
			parser_add_typedef_name(fp_name,
			                        type_ptr(fp_has_prototype
			                                 ? parser_make_function_type(ret_type,
			                                                             fp_param_types,
			                                                             fp_param_count,
			                                                             fp_is_variadic,
			                                                             fp_fixed_params)
			                                 : type_func(clone_type(ret_type))));
			return 1;
		}

		const Token *alias = lexer_peek();
		if (alias->kind != TOK_IDENT) {
			fatal_cur("Expected typedef name after struct tag\n");
		}
		char alias_name[64] = {0};
		STRNCPY(alias_name, alias->text ? alias->text : "", sizeof(alias_name) - 1);

		lexer_next();

		StructDef *def = get_or_add_forward_struct(struct_name);
		Type *base_type = def->is_union ? type_union(struct_name, def->size)
		                                : type_struct(struct_name, def->size);
		for (int pi = 0; pi < ptr_depth; pi++) base_type = type_ptr(base_type);
		base_type = parse_typedef_alias_type(base_type);
		expect(TOK_SEMI);
		parser_add_typedef_name(alias_name, base_type);
		return 1;
	}

	{
		Type *base_type = parse_type_name();
		Node *vm_typedef = NULL;
		stmt_reject_unsupported_special_type(base_type);
		if (parser_type_name_saw_trailing_function_specifier())
			fatal_cur("function specifier is only valid on function declarations\n");
		if (parser_type_name_saw_thread_local_storage_specifier() ||
		    parser_type_name_trailing_storage_class() == TOK_STATIC ||
		    parser_type_name_trailing_storage_class() == TOK_EXTERN ||
		    parser_type_name_trailing_storage_class() == TOK_AUTO ||
		    parser_type_name_trailing_storage_class() == TOK_REGISTER ||
		    parser_type_name_trailing_storage_class() == TOK_TYPEDEF)
			fatal_cur("multiple storage classes in declaration\n");

		if (!stmt_typedef_is_file_scope())
			vm_typedef = stmt_try_parse_local_vm_array_typedef(base_type, 0);
		if (vm_typedef) {
			stmt_last_typedef_decl_node = vm_typedef;
			return 1;
		}
		if (!stmt_typedef_is_file_scope())
			vm_typedef = stmt_try_parse_local_vm_pointer_array_typedef(base_type, 0);
		if (vm_typedef) {
			stmt_last_typedef_decl_node = vm_typedef;
			return 1;
		}
		return parse_typedef_declaration_after_base_type(base_type);
	}
}

Node *
parse_return_call_with_compound_literal_arg(void)
{

	/*
	 * v120 focused temporary materialization:
	 *
	 *   return sum((struct Point){ 1, 41 });
	 *
	 * Lower to:
	 *
	 *   struct Point __compound_arg_N = { 1, 41 };
	 *   return sum(__compound_arg_N);
	 *
	 * The existing argument lowering then passes the temp using the target ABI.
	 */
	if (lexer_peek()->kind != TOK_IDENT ||
	        lexer_peek_ahead(1)->kind != TOK_LPAREN ||
	        lexer_peek_ahead(2)->kind != TOK_LPAREN ||
	        lexer_peek_ahead(3)->kind != TOK_STRUCT) {
		return NULL;
	}

	const Token *func = lexer_peek();
	lexer_next(); /* function name */
	lexer_next(); /* call '(' */
	lexer_next(); /* compound literal '(' */
	lexer_next(); /* struct */

	const Token *struct_name = lexer_peek();
	if (struct_name->kind != TOK_IDENT) {
		fatal_cur("Expected struct name in compound literal argument\n");
	}
	lexer_next();

	expect(TOK_RPAREN);

	if (lexer_peek()->kind != TOK_LBRACE) {
		fatal_cur("Expected initializer list in compound literal argument\n");
	}
	lexer_next();

	char temp_name[64];
	snprintf(temp_name, sizeof(temp_name), "__compound_arg_%d", parser_alloc_compound_arg_temp_id());

	int offset = add_struct_local(temp_name, struct_name->text);
	StructDef *def = find_struct(struct_name->text);

	Node *decl = new_struct_decl(temp_name, offset);
	decl->type = type_struct(struct_name->text, def->size);

	Node *head = parse_struct_initializer_values(def, struct_name->text, offset, decl);

	parser_expect_local_aggregate_initializer_close(def);
	expect(TOK_RPAREN);
	expect(TOK_SEMI);

	Node *temp = new_var(temp_name, offset);
	temp->type = type_struct(struct_name->text, def->size);
	temp->elem_size = def->size;
	STRNCPY(temp->struct_name, struct_name->text, sizeof(temp->struct_name) - 1);

	Node *arg = temp;
	if (!stmt_arm64_struct_arg_passes_by_value(temp->type)) {
		arg = new_addr(temp);
		arg->by_ref_arg = 1;
	}

	Node *call = new_call(func->text, arg);
	FuncInfo *fi = find_func(func->text);
	if (fi && fi->returns_pointer) {
		call->is_pointer = 1;
		call->elem_size = fi->return_elem_size;
		call->type = type_ptr(type_for_size(fi->return_elem_size));
	}

	head = append_node(head, new_return(call));
	Node *tmp=new_block(head);
	return tmp;
}

Node *
make_struct_temp_from_compound_literal(const char *struct_name, const char *prefix, Node **out_temp_var)
{
	char temp_name[64];
	snprintf(temp_name, sizeof(temp_name), "%s%d", prefix, parser_alloc_struct_arg_temp_id());

	int offset = add_struct_local(temp_name, struct_name);
	StructDef *def = find_struct(struct_name);

	Node *decl = new_struct_decl(temp_name, offset);
	decl->type = type_struct(struct_name, def->size);

	Node *head = parse_struct_initializer_values(def, struct_name, offset, decl);

	Node *temp = new_var(temp_name, offset);
	temp->type = type_struct(struct_name, def->size);
	temp->elem_size = def->size;
	STRNCPY(temp->struct_name, struct_name, sizeof(temp->struct_name) - 1);

	if (out_temp_var)
		*out_temp_var = temp;

	return head;
}

static int
stmt_arm64_struct_arg_passes_by_value(Type *type)
{
	AggregateAbiClass abi;

	if (preprocess_get_target() != PP_TARGET_ARM64 || !type || !type_is_struct(type))
		return 0;

	abi = parser_classify_aggregate_abi(type, NULL);
	return abi == AGGREGATE_ABI_INTREGS || abi == AGGREGATE_ABI_HFA;
}

Node *
parse_call_statement_with_compound_literal_arg(void)
{
	/*
	 * v122:
	 *
	 *   sink((struct Point){ 1, 41 });
	 *
	 * Lower to a hidden local temporary and then call sink(temp).
	 */
	if (lexer_peek()->kind != TOK_IDENT ||
	        lexer_peek_ahead(1)->kind != TOK_LPAREN ||
	        lexer_peek_ahead(2)->kind != TOK_LPAREN ||
	        lexer_peek_ahead(3)->kind != TOK_STRUCT)
		return NULL;

	const Token *func = lexer_peek();
	lexer_next(); /* function name */
	lexer_next(); /* call '(' */
	lexer_next(); /* compound literal '(' */
	lexer_next(); /* struct */

	const Token *struct_name = lexer_peek();
	if (struct_name->kind != TOK_IDENT) {
		fatal_cur("Expected struct name in compound literal argument\n");
	}
	lexer_next();

	expect(TOK_RPAREN);

	if (lexer_peek()->kind != TOK_LBRACE) {
		fatal_cur("Expected initializer list in compound literal argument\n");
	}
	lexer_next();

	Node *temp = NULL;
	Node *head = make_struct_temp_from_compound_literal(struct_name->text, "__compound_stmt_arg_", &temp);

	expect(TOK_RBRACE);
	expect(TOK_RPAREN);
	expect(TOK_SEMI);

	Node *arg = temp;
	if (!stmt_arm64_struct_arg_passes_by_value(temp->type)) {
		arg = new_addr(temp);
		arg->by_ref_arg = 1;
	}

	Node *call = new_call(func->text, arg);
	head = append_node(head, call);
	return new_block(head);
}

Node *
parse_return_call_with_struct_return_arg(void)
{

	/*
	 * v123:
	 *
	 *   return sum(make_point());
	 *
	 * Lower to:
	 *
	 *   struct Point __tmp;
	 *   __tmp = make_point();
	 *   return sum(__tmp);
	 */
	if (lexer_peek()->kind != TOK_IDENT ||
	        lexer_peek_ahead(1)->kind != TOK_LPAREN ||
	        lexer_peek_ahead(2)->kind != TOK_IDENT ||
	        lexer_peek_ahead(3)->kind != TOK_LPAREN) {
		return NULL;
	}

	const Token *outer = lexer_peek();
	const Token *inner = lexer_peek_ahead(2);

	FuncInfo *inner_info = find_func(inner->text);
	if (!inner_info || !inner_info->returns_struct) {
		return NULL;
	}

	lexer_next(); /* outer */
	lexer_next(); /* outer '(' */
	lexer_next(); /* inner */
	lexer_next(); /* inner '(' */

	Node *inner_args = NULL;
	if (lexer_peek()->kind != TOK_RPAREN)
		inner_args = parse_arg_list(inner_info);

	expect(TOK_RPAREN);
	expect(TOK_RPAREN);
	expect(TOK_SEMI);

	char temp_name[64];
	snprintf(temp_name, sizeof(temp_name), "__struct_call_arg_%d", parser_alloc_struct_arg_temp_id());

	int offset = add_struct_local(temp_name, inner_info->struct_name);
	StructDef *def = find_struct(inner_info->struct_name);

	Node *decl = new_struct_decl(temp_name, offset);
	decl->type = type_struct(inner_info->struct_name, def->size);

	Node *temp_lhs = new_var(temp_name, offset);
	temp_lhs->type = type_struct(inner_info->struct_name, def->size);
	temp_lhs->elem_size = def->size;
	STRNCPY(temp_lhs->struct_name, inner_info->struct_name, sizeof(temp_lhs->struct_name) - 1);

	Node *inner_call = new_call(inner->text, inner_args);
	inner_call->returns_struct = 1;
	inner_call->aggregate_abi_class = inner_info->return_abi_class;
	inner_call->aggregate_abi_reg_count =
	    inner_call->aggregate_abi_class == AGGREGATE_ABI_INTREGS
	        ? inner_info->return_abi_reg_count
	        : 0;
	inner_call->struct_return_size = inner_info->struct_size;
		inner_call->type = inner_info->return_type ? clone_type(inner_info->return_type)
		                                           : type_struct(inner_info->struct_name, inner_info->struct_size);
	STRNCPY(inner_call->return_struct_name, inner_info->struct_name, sizeof(inner_call->return_struct_name) - 1);

	Node *assign = new_assign(temp_lhs, inner_call);

	Node *temp_arg = new_var(temp_name, offset);
	temp_arg->type = type_struct(inner_info->struct_name, def->size);
	temp_arg->elem_size = def->size;
	STRNCPY(temp_arg->struct_name, inner_info->struct_name, sizeof(temp_arg->struct_name) - 1);

	Node *arg = temp_arg;
	if (!stmt_arm64_struct_arg_passes_by_value(temp_arg->type)) {
		arg = new_addr(temp_arg);
		arg->by_ref_arg = 1;
	}

	Node *outer_call = new_call(outer->text, arg);

	Node *head = decl;
	head = append_node(head, assign);
	head = append_node(head, new_return(outer_call));
	Node *tmp= new_block(head);
	return tmp;
}

static Node *
materialize_struct_return_call_arg(const Token *inner, Node **out_arg)
{
	FuncInfo *inner_info = find_func(inner->text);
	if (!inner_info || !inner_info->returns_struct)
		return NULL;

	lexer_next(); /* inner function name */
	expect(TOK_LPAREN);

	Node *inner_args = NULL;
	if (lexer_peek()->kind != TOK_RPAREN)
		inner_args = parse_arg_list(inner_info);

	expect(TOK_RPAREN);

	char temp_name[64];
	snprintf(temp_name, sizeof(temp_name), "__v124_call_arg_%d", parser_alloc_struct_arg_temp_id());

	int offset = add_struct_local(temp_name, inner_info->struct_name);
	StructDef *def = find_struct(inner_info->struct_name);

	Node *decl = new_struct_decl(temp_name, offset);
	decl->type = type_struct(inner_info->struct_name, def->size);

	Node *lhs = new_var(temp_name, offset);
	lhs->type = type_struct(inner_info->struct_name, def->size);
	lhs->elem_size = def->size;
	STRNCPY(lhs->struct_name, inner_info->struct_name, sizeof(lhs->struct_name) - 1);

	Node *inner_call = new_call(inner->text, inner_args);
	inner_call->returns_struct = 1;
	inner_call->aggregate_abi_class = inner_info->return_abi_class;
	inner_call->aggregate_abi_reg_count =
	    inner_call->aggregate_abi_class == AGGREGATE_ABI_INTREGS
	        ? inner_info->return_abi_reg_count
	        : 0;
	inner_call->struct_return_size = inner_info->struct_size;
		inner_call->type = inner_info->return_type ? clone_type(inner_info->return_type)
		                                           : type_struct(inner_info->struct_name, inner_info->struct_size);
	STRNCPY(inner_call->return_struct_name, inner_info->struct_name, sizeof(inner_call->return_struct_name) - 1);

	Node *assign = new_assign(lhs, inner_call);

	Node *temp = new_var(temp_name, offset);
	temp->type = type_struct(inner_info->struct_name, def->size);
	temp->elem_size = def->size;
	STRNCPY(temp->struct_name, inner_info->struct_name, sizeof(temp->struct_name) - 1);

	Node *arg = temp;
	if (!stmt_arm64_struct_arg_passes_by_value(temp->type)) {
		arg = new_addr(temp);
		arg->by_ref_arg = 1;
	}
	*out_arg = arg;

	return append_node(decl, assign);
}

static Node *
materialize_compound_literal_call_arg(Node **out_arg)
{
	if (lexer_peek()->kind != TOK_LPAREN || lexer_peek_ahead(1)->kind != TOK_STRUCT)
		return NULL;

	lexer_next(); /* ( */
	lexer_next(); /* struct */

	const Token *struct_name = lexer_peek();
	if (struct_name->kind != TOK_IDENT) {
		fatal_cur("Expected struct name in compound literal argument\n");
	}
	lexer_next();

	expect(TOK_RPAREN);

	if (lexer_peek()->kind != TOK_LBRACE) {
		fatal_cur("Expected initializer list in compound literal argument\n");
	}
	lexer_next();

	Node *temp = NULL;
	Node *head = make_struct_temp_from_compound_literal(struct_name->text, "__v124_compound_arg_", &temp);

	expect(TOK_RBRACE);

	Node *arg = temp;
	if (!stmt_arm64_struct_arg_passes_by_value(temp->type)) {
		arg = new_addr(temp);
		arg->by_ref_arg = 1;
	}
	*out_arg = arg;
	return head;
}

static Node *
parse_v124_call_after_name(const Token *func, int as_return)
{

	expect(TOK_LPAREN);

	Node *prefix = NULL;
	Node *args = NULL;
	Node *arg_tail = NULL;

	if (lexer_peek()->kind != TOK_RPAREN) {
		for (;;) {
			Node *arg = NULL;
			Node *setup = NULL;

			if (lexer_peek()->kind == TOK_LPAREN && lexer_peek_ahead(1)->kind == TOK_STRUCT) {
				setup = materialize_compound_literal_call_arg(&arg);
			} else if (lexer_peek()->kind == TOK_IDENT &&
			           lexer_peek_ahead(1)->kind == TOK_LPAREN) {
				const Token *inner = lexer_peek();
				setup = materialize_struct_return_call_arg(inner, &arg);
				if (!setup)
					arg = parse_expr();
			} else {
				arg = parse_expr();
			}

			if (setup)
				prefix = append_node(prefix, setup);

			if (arg && arg->type && type_is_struct(arg->type) &&
			    !stmt_arm64_struct_arg_passes_by_value(arg->type)) {
				arg = new_addr(arg);
				arg->by_ref_arg = 1;
			}

			if (!args) {
				args = arg;
				arg_tail = arg;
			} else {
				arg_tail->next = arg;
				arg_tail = arg;
			}

			if (lexer_peek()->kind != TOK_COMMA)
				break;

			lexer_next();
		}
	}

	expect(TOK_RPAREN);
	expect(TOK_SEMI);

	Node *call = new_call(func->text, args);
	FuncInfo *fi = find_func(func->text);
	if (fi) {
		if (fi->returns_struct) {
			call->returns_struct = 1;
			call->aggregate_abi_class = fi->return_abi_class;
			call->aggregate_abi_reg_count = call->aggregate_abi_class == AGGREGATE_ABI_INTREGS
			                                 ? fi->return_abi_reg_count
			                                 : 0;
			call->struct_return_size = fi->struct_size;
			call->type = fi->return_type ? clone_type(fi->return_type)
			                             : type_struct(fi->struct_name, fi->struct_size);
			STRNCPY(call->return_struct_name, fi->struct_name, sizeof(call->return_struct_name) - 1);
		} else if (fi->returns_pointer) {
			call->is_pointer = 1;
			call->elem_size = fi->return_elem_size;
			call->type = type_ptr(type_for_size(fi->return_elem_size));
		}
	}

	if (as_return && call->returns_struct) {
		/* Spill struct-returning call to temp before returning */
		char temp_name[64];
		FuncInfo *rfi;
		const char *sname;
		StructDef *def;
		int sz;
		int temp_off;
		Node *lhs;
		Node *assign;
		Node *temp_ref;
		Node *ret_expr;

		snprintf(temp_name, sizeof(temp_name), "__struct_ret_v124_%d", parser_alloc_struct_arg_temp_id());
		rfi = find_func(func->text);
		sname = (rfi && rfi->struct_name[0]) ? rfi->struct_name : call->return_struct_name;
		def = sname[0] ? find_struct(sname) : NULL;
		sz = def ? def->size : 8;
		temp_off = add_struct_local(temp_name, sname);

		lhs = new_var(temp_name, temp_off);
		lhs->type = type_struct(sname, sz);
		lhs->elem_size = sz;
		STRNCPY(lhs->struct_name, sname, sizeof(lhs->struct_name) - 1);

		assign = new_assign(lhs, call);

		temp_ref = new_var(temp_name, temp_off);
		temp_ref->type = type_struct(sname, sz);
		temp_ref->elem_size = sz;
		STRNCPY(temp_ref->struct_name, sname, sizeof(temp_ref->struct_name) - 1);

		ret_expr = new_binary(ND_COMMA, assign, temp_ref);
		ret_expr->type = temp_ref->type;
		call = new_return(ret_expr);
	} else if (as_return) {
		call = new_return(call);
	}

	if (prefix) {
		Node *tmp= new_block(append_node(prefix, call));
		return tmp;
	}

	Node *tmp= as_return ? call : call;
	return tmp;
}

Node *
parse_v124_return_call_with_temps(void)
{

	if (!v124_call_starts_with_struct_temp_arg()) {
		return NULL;
	}

	const Token *func = lexer_peek();
	lexer_next();
	Node *tmp=parse_v124_call_after_name(func, 1);
	return tmp;
}

Node *
parse_v124_call_statement_with_temps(void)
{

	if (!v124_call_starts_with_struct_temp_arg()) {
		return NULL;
	}
	const Token *func = lexer_peek();
	lexer_next();
	Node *tmp= parse_v124_call_after_name(func, 0);
	return tmp;
}

Node *
make_struct_member_read_from_local(const char *var_name)
{
	const char *struct_name = struct_name_local(var_name);
	int offset = find_local(var_name);
	Field *field = NULL;

	if (lexer_peek()->kind != TOK_DOT) {
		fatal_cur("Expected member access after struct assignment expression\n");
	}

	while (lexer_peek()->kind == TOK_DOT) {
		lexer_next();

		const Token *field_name = lexer_peek();
		if (field_name->kind != TOK_IDENT) {
			fatal_cur("Expected field name after '.'\n");
		}
		lexer_next();

		field = find_field(struct_name, field_name->text);
		offset += field->offset;

		if (field->is_struct) {
			struct_name = field->struct_name;
			continue;
		}

		if (lexer_peek()->kind == TOK_DOT) {
			fatal_cur("Nested member access through non-struct field\n");
		}

		Node *member = new_member(field->name, offset);
		member->elem_size = field->size;
		member->type = type_for_size(field->size);
		STRNCPY(member->struct_name, struct_name, sizeof(member->struct_name) - 1);
		return member;
	}

	fatal_cur("Struct assignment member expression must select scalar field\n");
}

Node *
parse_return_struct_assign_member_expr(void)
{

	if (lexer_peek()->kind != TOK_LPAREN ||
	        lexer_peek_ahead(1)->kind != TOK_IDENT ||
	        lexer_peek_ahead(2)->kind != TOK_ASSIGN ||
	        lexer_peek_ahead(3)->kind != TOK_IDENT ||
	        lexer_peek_ahead(4)->kind != TOK_RPAREN ||
	        lexer_peek_ahead(5)->kind != TOK_DOT) {
		return NULL;
	}

	lexer_next(); /* ( */

	const Token *dst_tok = lexer_peek();
	lexer_next();

	expect(TOK_ASSIGN);

	const Token *src_tok = lexer_peek();
	if (src_tok->kind != TOK_IDENT) {
		fatal_cur("Expected RHS struct identifier in assignment expression\n");
	}
	lexer_next();

	expect(TOK_RPAREN);

	if (!is_struct_local(dst_tok->text) || !is_struct_local(src_tok->text)) {
		return NULL;
	}

	const char *dst_struct = struct_name_local(dst_tok->text);
	const char *src_struct = struct_name_local(src_tok->text);
	if (STRCMP(dst_struct, src_struct) != 0) {
		fatal_cur("Struct assignment expression type mismatch\n");
	}

	StructDef *def = find_struct(dst_struct);

	Node *dst = make_scalar_var_node(dst_tok->text);
	Node *src = make_scalar_var_node(src_tok->text);
	Node *assign = new_struct_assign(dst, src, def->size);

	Node *member = make_struct_member_read_from_local(dst_tok->text);
	expect(TOK_SEMI);

	Node *tmp= new_block(append_node(assign, new_return(member)));
	return tmp;
}

Node *
parse_struct_assign_member_expr_core(Node **setup_out)
{

	if (lexer_peek()->kind != TOK_LPAREN ||
	        lexer_peek_ahead(1)->kind != TOK_IDENT ||
	        lexer_peek_ahead(2)->kind != TOK_ASSIGN ||
	        lexer_peek_ahead(3)->kind != TOK_IDENT ||
	        lexer_peek_ahead(4)->kind != TOK_RPAREN ||
	        lexer_peek_ahead(5)->kind != TOK_DOT) {
		return NULL;
	}
	lexer_next(); /* ( */

	const Token *dst_tok = lexer_peek();
	lexer_next();

	expect(TOK_ASSIGN);

	const Token *src_tok = lexer_peek();
	if (src_tok->kind != TOK_IDENT) {
		fatal_cur("Expected RHS struct identifier in assignment expression\n");
	}
	lexer_next();

	expect(TOK_RPAREN);

	if (!is_struct_local(dst_tok->text) || !is_struct_local(src_tok->text)) {
		return NULL;
	}
	const char *dst_struct = struct_name_local(dst_tok->text);
	const char *src_struct = struct_name_local(src_tok->text);
	if (STRCMP(dst_struct, src_struct) != 0) {
		fatal_cur("Struct assignment expression type mismatch\n");
	}

	StructDef *def = find_struct(dst_struct);
	Node *dst = make_scalar_var_node(dst_tok->text);
	Node *src = make_scalar_var_node(src_tok->text);
	Node *assign = new_struct_assign(dst, src, def->size);

	Node *member = make_struct_member_read_from_local(dst_tok->text);

	if (setup_out)
		*setup_out = assign;

	return member;
}

Node *
parse_call_statement_with_struct_assign_member_arg(void)
{

	/*
	 * v129b:
	 *
	 *   sink2((a = b).x, (c = d).y);
	 *
	 * Lower setup expressions before the call, preserving source order for
	 * struct assignments.
	 */
	if (lexer_peek()->kind != TOK_IDENT ||
	        lexer_peek_ahead(1)->kind != TOK_LPAREN) {
		return NULL;
	}
	if (!(lexer_peek_ahead(2)->kind == TOK_LPAREN &&
	        lexer_peek_ahead(3)->kind == TOK_IDENT &&
	        lexer_peek_ahead(4)->kind == TOK_ASSIGN &&
	        lexer_peek_ahead(5)->kind == TOK_IDENT &&
	        lexer_peek_ahead(6)->kind == TOK_RPAREN &&
	        lexer_peek_ahead(7)->kind == TOK_DOT) &&
	        !(lexer_peek_ahead(2)->kind == TOK_IDENT &&
	          lexer_peek_ahead(3)->kind == TOK_LPAREN &&
	          lexer_peek_ahead(4)->kind == TOK_RPAREN &&
	          lexer_peek_ahead(5)->kind == TOK_DOT)) {
		return NULL;
	}

	const Token *func = lexer_peek();
	lexer_next();
	expect(TOK_LPAREN);

	Node *prefix = NULL;
	Node *args = NULL;
	Node *arg_tail = NULL;
	int saw_struct_assign_member = 0;

	if (lexer_peek()->kind != TOK_RPAREN) {
		for (;;) {
			Node *setup = NULL;
			Node *arg = parse_struct_assign_member_sum(&setup);

			if (arg) {
				saw_struct_assign_member = 1;
				if (setup)
					prefix = append_node(prefix, setup);
			} else {
				arg = parse_expr();
			}

			if (!args) {
				args = arg;
				arg_tail = arg;
			} else {
				arg_tail->next = arg;
				arg_tail = arg;
			}

			if (lexer_peek()->kind != TOK_COMMA)
				break;

			lexer_next();
		}
	}

	expect(TOK_RPAREN);
	expect(TOK_SEMI);

	if (!saw_struct_assign_member) {
		return NULL;
	}

	Node *call = new_call(func->text, args);
	Node *tmp=new_block(append_node(prefix, call));
	return tmp;
}

Node *
parse_if_with_struct_assign_member_condition(void)
{

	if (lexer_peek()->kind != TOK_IF ||
	        lexer_peek_ahead(1)->kind != TOK_LPAREN) {
		return NULL;
	}
	if (!(lexer_peek_ahead(2)->kind == TOK_LPAREN &&
	        lexer_peek_ahead(3)->kind == TOK_IDENT &&
	        lexer_peek_ahead(4)->kind == TOK_ASSIGN &&
	        lexer_peek_ahead(5)->kind == TOK_IDENT &&
	        lexer_peek_ahead(6)->kind == TOK_RPAREN &&
	        lexer_peek_ahead(7)->kind == TOK_DOT) &&
	        !(lexer_peek_ahead(2)->kind == TOK_IDENT &&
	          lexer_peek_ahead(3)->kind == TOK_LPAREN &&
	          lexer_peek_ahead(4)->kind == TOK_RPAREN &&
	          lexer_peek_ahead(5)->kind == TOK_DOT)) {
		return NULL;
	}

	lexer_next();
	expect(TOK_LPAREN);

	Node *setup = NULL;
	Node *left = parse_struct_assign_member_sum(&setup);
	if (!left) {
		return NULL;
	}

	TokenKind op = lexer_peek()->kind;
	Node *cond = left;

	if (op == TOK_EQ || op == TOK_NE ||
	        op == TOK_LT || op == TOK_LE || op == TOK_GT || op == TOK_GE) {
		lexer_next();
		Node *rhs_setup = NULL;
		Node *right = parse_struct_assign_member_sum(&rhs_setup);
		if (!right)
			right = parse_expr();
		if (rhs_setup)
			setup = append_node(setup, rhs_setup);

		NodeKind kind = ND_EQ;
		if (op == TOK_NE) kind = ND_NE;
		else if (op == TOK_LT) kind = ND_LT;
		else if (op == TOK_LE) kind = ND_LE;
		else if (op == TOK_GT) kind = ND_GT;
		else if (op == TOK_GE) kind = ND_GE;

		cond = new_binary(kind, left, right);
	}

	expect(TOK_RPAREN);

	Node *then_body = parse_statement();
	Node *else_body = NULL;

	if (lexer_peek()->kind == TOK_ELSE) {
		lexer_next();
		else_body = parse_statement();
	}

	Node *tmp= new_block(append_node(setup, new_if(cond, then_body, else_body)));
	return tmp;
}

static int
case_sizeof_operand_is_vla(int index)
{
	if (lexer_peek_ahead(index)->kind == TOK_IDENT &&
	    lexer_peek_ahead(index)->text &&
	    is_vla_local(lexer_peek_ahead(index)->text))
		return 1;

	if (lexer_peek_ahead(index)->kind == TOK_LPAREN &&
	    lexer_peek_ahead(index + 1)->kind == TOK_IDENT &&
	    lexer_peek_ahead(index + 1)->text &&
	    lexer_peek_ahead(index + 2)->kind == TOK_RPAREN &&
	    is_vla_local(lexer_peek_ahead(index + 1)->text))
		return 1;

	return 0;
}

static int
skip_case_constant_operand_after_unary(int index)
{
	TokenKind kind;

	for (;;) {
		kind = lexer_peek_ahead(index)->kind;
		if (kind == TOK_STAR || kind == TOK_AMP || kind == TOK_PLUS ||
		    kind == TOK_MINUS || kind == TOK_NOT || kind == TOK_TILDE ||
		    kind == TOK_PLUSPLUS || kind == TOK_MINUSMINUS) {
			index++;
			continue;
		}
		break;
	}

	if (lexer_peek_ahead(index)->kind == TOK_LPAREN) {
		int depth = 1;
		index++;
		while (depth > 0 && lexer_peek_ahead(index)->kind != TOK_EOF) {
			kind = lexer_peek_ahead(index)->kind;
			if (kind == TOK_LPAREN)
				depth++;
			else if (kind == TOK_RPAREN)
				depth--;
			index++;
		}
	} else if (lexer_peek_ahead(index)->kind != TOK_EOF) {
		index++;
	}

	for (;;) {
		kind = lexer_peek_ahead(index)->kind;
		if (kind == TOK_LBRACKET) {
			int depth = 1;
			index++;
			while (depth > 0 && lexer_peek_ahead(index)->kind != TOK_EOF) {
				kind = lexer_peek_ahead(index)->kind;
				if (kind == TOK_LBRACKET)
					depth++;
				else if (kind == TOK_RBRACKET)
					depth--;
				index++;
			}
			continue;
		}
		if ((kind == TOK_DOT || kind == TOK_ARROW) &&
		    lexer_peek_ahead(index + 1)->kind == TOK_IDENT) {
			index += 2;
			continue;
		}
		break;
	}

	return index;
}

static int
case_label_tokens_form_pointer_type_cast(int start_index)
{
	const Token *type_tok = lexer_peek_ahead(start_index + 1);
	int depth = 1;
	int i = start_index + 1;
	int saw_star = 0;
	int saw_typedef = 0;

	if (type_tok->kind == TOK_IDENT && type_tok->text &&
	    parser_is_typedef_name(type_tok->text)) {
		Type *typedef_type = parser_find_typedef(type_tok->text);
		Type *canonical = parser_canonicalize_decl_type(typedef_type);
		if (canonical && type_is_pointer(canonical))
			saw_typedef = 1;
	}

	if (!is_type_start_token(type_tok->kind, type_tok->text) && !saw_typedef)
		return 0;

	for (; lexer_peek_ahead(i)->kind != TOK_EOF; i++) {
		TokenKind kind = lexer_peek_ahead(i)->kind;

		if (kind == TOK_LPAREN) {
			depth++;
			continue;
		}
		if (kind == TOK_RPAREN) {
			depth--;
			if (depth == 0)
				return saw_star || saw_typedef;
			continue;
		}
		if (depth == 1 && kind == TOK_STAR)
			saw_star = 1;
	}

	return 0;
}

static int
eval_case_label_const(int *out_value)
{
	int depth = 0;
	int is_constant = 1;

	/* Case labels require integer constant expressions.  The shared constant
	 * evaluator is intentionally permissive for legacy array bounds, so first
	 * reject forms that cannot be case-label constants. */
	for (int i = 0;; i++) {
		const Token *tok = lexer_peek_ahead(i);

		if (tok->kind == TOK_EOF)
			return 0;
		if (depth == 0 && tok->kind == TOK_COLON)
			break;
		if (tok->kind == TOK_SIZEOF || tok->kind == TOK_ALIGNOF) {
			if (tok->kind == TOK_SIZEOF && case_sizeof_operand_is_vla(i + 1))
				return 0;
			i = skip_case_constant_operand_after_unary(i + 1) - 1;
			continue;
		}
		if (tok->kind == TOK_IDENT && tok->text &&
		    STRCMP(tok->text, "offsetof") == 0 &&
		    lexer_peek_ahead(i + 1)->kind == TOK_LPAREN) {
			int paren_depth = 1;

			i += 2;
			while (paren_depth > 0 &&
			       lexer_peek_ahead(i)->kind != TOK_EOF) {
				TokenKind kind = lexer_peek_ahead(i)->kind;

				if (kind == TOK_LPAREN)
					paren_depth++;
				else if (kind == TOK_RPAREN)
					paren_depth--;
				i++;
			}
			i--;
			continue;
		}
		if (tok->kind == TOK_LPAREN) {
			if (case_label_tokens_form_pointer_type_cast(i))
				return 0;
			depth++;
			continue;
		}
		if (tok->kind == TOK_RPAREN) {
			if (depth > 0)
				depth--;
			continue;
		}
		if (tok->kind == TOK_NUM && tok->num_is_fp)
			return 0;
	}

	*out_value = (int)parser_eval_const_int_expr_checked(&is_constant);
	return is_constant;
}


Node *
parse_duff_switch_body(Node *cond)
{
	/*
	 * Parse Duff's-device style switches:
	 *
	 *   switch (x) {
	 *   case 0: do { stmt0;
	 *   case 7:      stmt7;
	 *          ...
	 *   case 1:      stmt1;
	 *           } while (expr);
	 *   }
	 *
	 * The case labels live inside the do-body and are valid C labels.
	 * Represent this as a switch whose case bodies fall through, with the
	 * do/while condition stored in switch->inc for the emitter.
	 */
	if (lexer_peek()->kind != TOK_CASE ||
	    lexer_peek_ahead(1)->kind != TOK_NUM ||
	    lexer_peek_ahead(2)->kind != TOK_COLON ||
	    lexer_peek_ahead(3)->kind != TOK_DO ||
	    lexer_peek_ahead(4)->kind != TOK_LBRACE)
		return NULL;

	/* Verify this is actually Duff's device by checking that a case or
	 * default label appears inside the do { } body.  A plain
	 * "case N: do { stmt; } while(0);" must NOT be treated as Duff's
	 * device — the do-while is just a statement, not a Duff body. */
	{
		int depth = 0;
		int n = 4; /* tok_ring index relative to current: 0=case,1=NUM,2=:,3=do,4={ */
		int found_inner_case = 0;
		for (;;) {
			n++;
			const Token *t = lexer_peek_ahead(n);
			if (t->kind == TOK_EOF) break;
			if (t->kind == TOK_LBRACE) { depth++; continue; }
			if (t->kind == TOK_RBRACE) {
				if (depth == 0) break; /* closing } of the do body */
				depth--;
				continue;
			}
			if (depth == 0 &&
			    (t->kind == TOK_CASE || t->kind == TOK_DEFAULT)) {
				found_inner_case = 1;
				break;
			}
		}
		if (!found_inner_case)
			return NULL;
	}

	Node *cases = NULL;
	SwitchLabelSet labels = {0};

	/* First case: case N: do { */
	lexer_next(); /* case */
	int case_value = lexer_peek()->value;
	switch_label_set_add_case(&labels, case_value);
	lexer_next(); /* number */
	expect(TOK_COLON);
	reject_declaration_after_label_before_c23();
	expect(TOK_DO);
	expect(TOK_LBRACE);

	for (;;) {
		Node *body = NULL;
		while (lexer_peek()->kind != TOK_CASE &&
		       lexer_peek()->kind != TOK_DEFAULT &&
		       lexer_peek()->kind != TOK_RBRACE &&
		       lexer_peek()->kind != TOK_EOF) {
			body = append_node(body, parse_statement());
		}

		cases = append_node(cases, new_case(case_value, body));

		if (lexer_peek()->kind == TOK_RBRACE)
			break;

		if (lexer_peek()->kind == TOK_DEFAULT) {
			lexer_next();
			expect(TOK_COLON);
			reject_declaration_after_label_before_c23();
			switch_label_set_add_default(&labels);
			Node *dbody = NULL;
			while (lexer_peek()->kind != TOK_CASE &&
			       lexer_peek()->kind != TOK_RBRACE &&
			       lexer_peek()->kind != TOK_EOF)
				dbody = append_node(dbody, parse_statement());
			cases = append_node(cases, new_default(dbody));
			if (lexer_peek()->kind == TOK_RBRACE)
				break;
		}

		expect(TOK_CASE);
		if (!eval_case_label_const(&case_value)) {
			fatal_cur("case label must be a constant integer\n");
		}
		switch_label_set_add_case(&labels, case_value);
		expect(TOK_COLON);
	}

	expect(TOK_RBRACE);
	expect(TOK_WHILE);
	expect(TOK_LPAREN);
	Node *while_cond = expr_coerce_scalar_condition(parse_comma_expr());
	expect(TOK_RPAREN);
	expect(TOK_SEMI);
	expect(TOK_RBRACE);

	Node *sw = new_switch(cond, cases);
	sw->inc = while_cond;
	switch_label_set_free(&labels);
	return sw;
}

Node *
parse_switch_statement(void)
{
	int saved_local_count = parser_current_local_count();

	expect(TOK_SWITCH);
	expect(TOK_LPAREN);
	Node *cond = parse_comma_expr();
	expect(TOK_RPAREN);
	if (!cond || !cond->type || !type_is_integer(cond->type))
		fatal_cur("switch expression must have integer type\n");
	stmt_push_control_scope(saved_local_count, 0);
	stmt_switch_depth++;

	/* C allows switch without braces: switch(x) case 0: stmt */
	if (lexer_peek()->kind != TOK_LBRACE) {
		/* Parse the body as a single statement which may be a case label */
		Node *cases = NULL;
		SwitchLabelSet labels = {0};
		if (lexer_peek()->kind == TOK_CASE) {
			lexer_next();
			int case_value = 0;
			if (!eval_case_label_const(&case_value))
				fatal_cur("case label must be a constant integer\n");
			switch_label_set_add_case(&labels, case_value);
			expect(TOK_COLON);
			reject_declaration_after_label_before_c23();
			Node *body = NULL;
			if (lexer_peek()->kind != TOK_SEMI &&
			    lexer_peek()->kind != TOK_CASE &&
			    lexer_peek()->kind != TOK_DEFAULT &&
			    lexer_peek()->kind != TOK_RBRACE) {
				body = parse_statement();
			} else if (lexer_peek()->kind == TOK_SEMI) {
				lexer_next();
			}
			cases = new_case(case_value, body);
		} else if (lexer_peek()->kind == TOK_DEFAULT) {
			lexer_next();
			switch_label_set_add_default(&labels);
			expect(TOK_COLON);
			reject_declaration_after_label_before_c23();
			Node *body = parse_statement();
			cases = new_default(body);
		} else {
			/* No case label: just parse as a statement */
			cases = parse_embedded_statement();
		}
		stmt_pop_control_scope();
		stmt_switch_depth--;
		switch_label_set_free(&labels);
		return new_switch(cond, cases);
	}

	lexer_next(); /* consume { */

	Node *duff = parse_duff_switch_body(cond);
	if (duff) {
		stmt_pop_control_scope();
		stmt_switch_depth--;
		return duff;
	}

	Node *cases = NULL;
	SwitchLabelSet labels = {0};

	/* Consume any leading non-case statements (e.g. declarations, goto targets).
	 * Track brace depth to handle nested blocks containing case labels. */
	int switch_depth = 1; /* we already consumed outer { */
	Node *pre_stmts = NULL;
	while (switch_depth > 0 &&
	       !(switch_depth == 1 && (lexer_peek()->kind == TOK_CASE ||
	                                lexer_peek()->kind == TOK_DEFAULT ||
	                                lexer_peek()->kind == TOK_RBRACE))) {
		if (lexer_peek()->kind == TOK_LBRACE) {
			switch_depth++;
			lexer_next();
			continue;
		}
		if (lexer_peek()->kind == TOK_RBRACE) {
			switch_depth--;
			if (switch_depth > 0)
				lexer_next();
			continue;
		}
		/* Skip case/default labels at inner depths */
		if (lexer_peek()->kind == TOK_CASE || lexer_peek()->kind == TOK_DEFAULT) {
			lexer_next();
			if (lexer_peek()->kind == TOK_NUM || lexer_peek()->kind == TOK_IDENT)
				lexer_next();
			if (lexer_peek()->kind == TOK_COLON)
				lexer_next();
			continue;
		}
		/* "do { ... case N: ... } while(cond);" — consume "do" and enter brace scope */
		if (lexer_peek()->kind == TOK_DO) {
			lexer_next(); /* consume do */
			continue;     /* next iter will see { and increment depth */
		}
		/* "while(cond)" at depth > 1 — close of do-while; consume it */
		if (lexer_peek()->kind == TOK_WHILE && switch_depth > 1) {
			lexer_next(); /* consume while */
			if (lexer_peek()->kind == TOK_LPAREN) {
				int wd = 1; lexer_next();
				while (wd > 0 && lexer_peek()->kind != TOK_EOF) {
					if (lexer_peek()->kind == TOK_LPAREN) wd++;
					else if (lexer_peek()->kind == TOK_RPAREN) wd--;
					lexer_next();
				}
			}
			if (lexer_peek()->kind == TOK_SEMI) lexer_next();
			continue;
		}
		pre_stmts = append_node(pre_stmts, parse_statement());
	}
	(void)pre_stmts; /* pre-case statements fall through; we discard AST but parse them */

	while (lexer_peek()->kind != TOK_RBRACE) {
		Node *case_node = NULL;

		if (lexer_peek()->kind == TOK_CASE) {
			lexer_next();

			int case_value = 0;
			if (!eval_case_label_const(&case_value)) {
				fatal_cur("case label must be a constant integer\n");
			}
			switch_label_set_add_case(&labels, case_value);
			expect(TOK_COLON);
			reject_declaration_after_label_before_c23();

			Node *body = NULL;
			/*
			 * Collect case body statements via parse_statement(), which
			 * handles all nested constructs ({} blocks, do-while, for,
			 * if, compound statements) correctly through recursive
			 * descent.  Stop at the next case/default label or the
			 * closing } of the switch, both of which only appear at the
			 * top level of the case body (parse_statement() consumes
			 * nested braces internally so they never surface here).
			 */
			while (lexer_peek()->kind != TOK_CASE &&
			       lexer_peek()->kind != TOK_DEFAULT &&
			       lexer_peek()->kind != TOK_RBRACE) {
				body = append_node(body, parse_statement());
			}

			case_node = new_case(case_value, body);
		} else if (lexer_peek()->kind == TOK_DEFAULT) {
			lexer_next();
			switch_label_set_add_default(&labels);
			expect(TOK_COLON);
			reject_declaration_after_label_before_c23();

			Node *body = NULL;
			while (lexer_peek()->kind != TOK_CASE &&
			        lexer_peek()->kind != TOK_DEFAULT &&
			        lexer_peek()->kind != TOK_RBRACE) {
				body = append_node(body, parse_statement());
			}

			case_node = new_default(body);
		} else {
			fatal_cur("Expected case/default label in switch\n");
		}

		cases = append_node(cases, case_node);
	}

	expect(TOK_RBRACE);
	stmt_pop_control_scope();
	stmt_switch_depth--;
	Node *tmp= new_switch(cond, cases);
	switch_label_set_free(&labels);
	return tmp;
}

static Node *
parse_non_declaration_statement(const Token *token)
{
	if (token->kind == TOK_RETURN) {
		lexer_next();
		Node *expr;

		if (return_expr_may_need_special_lowering()) {
			Node *struct_call_return = parse_return_struct_call_value();
			if (struct_call_return)
				return struct_call_return;

			Node *setup = NULL;
			expr = parse_struct_assign_member_sum(&setup);
			if (expr) {
				expect(TOK_SEMI);
				return expr_value_statement(setup, stmt_wrap_return_with_cleanup(expr));
			}

			Node *struct_assign_member_return = parse_return_struct_assign_member_expr();
			if (struct_assign_member_return)
				return struct_assign_member_return;

			Node *v124_return_call = parse_v124_return_call_with_temps();
			if (v124_return_call)
				return v124_return_call;

			Node *struct_return_arg = parse_return_call_with_struct_return_arg();
			if (struct_return_arg)
				return struct_return_arg;

			Node *compound_arg_return = parse_return_call_with_compound_literal_arg();
			if (compound_arg_return)
				return compound_arg_return;
			}

			if (lexer_peek()->kind == TOK_SEMI) {
				validate_return_statement_form(0);
				lexer_next();
				return stmt_wrap_scope_cleanup(new_return(NULL), 0);
			}

			validate_return_statement_form(1);
			expr = parse_comma_expr();
		expect(TOK_SEMI);
		validate_pointer_return_compatibility(expr);
		expr = stmt_canonicalize_return_expr(expr);

		if (expr && expr->type && type_is_struct(expr->type) &&
		    !(expr->kind == ND_VAR)) {
			char temp_name[64];
			snprintf(temp_name, sizeof(temp_name), "__struct_ret_%d", parser_alloc_struct_arg_temp_id());
			const char *sname = expr->return_struct_name[0] ? expr->return_struct_name
			                    : expr->type->struct_name;
			StructDef *def = find_struct(sname);
			int sz = def ? def->size : 8;
			int temp_off = add_struct_local(temp_name, sname);

			Node *lhs = new_var(temp_name, temp_off);
			lhs->type = type_struct(sname, sz);
			lhs->elem_size = sz;
			STRNCPY(lhs->struct_name, sname, sizeof(lhs->struct_name) - 1);

			Node *assign = stmt_build_initializer_assign(lhs, expr, lhs->type);

			Node *temp_ref = new_var(temp_name, temp_off);
			temp_ref->type = type_struct(sname, sz);
			temp_ref->elem_size = sz;
			STRNCPY(temp_ref->struct_name, sname, sizeof(temp_ref->struct_name) - 1);

			expr = new_binary(ND_COMMA, assign, temp_ref);
			expr->type = temp_ref->type;
		}

		return stmt_wrap_return_with_cleanup(expr);
	}

	if (token->kind == TOK_BREAK) {
		int saved_local_count;

		lexer_next();
		expect(TOK_SEMI);
		saved_local_count = stmt_nearest_control_scope_saved(0);
		if (saved_local_count < 0)
			fatal_cur("break statement not within loop or switch\n");
		return stmt_wrap_scope_cleanup(new_break(), saved_local_count >= 0 ? saved_local_count : 0);
	}

	if (token->kind == TOK_CONTINUE) {
		int saved_local_count;

		lexer_next();
		expect(TOK_SEMI);
		saved_local_count = stmt_nearest_control_scope_saved(1);
		if (saved_local_count < 0)
			fatal_cur("continue statement not within loop\n");
		return stmt_wrap_scope_cleanup(new_continue(), saved_local_count >= 0 ? saved_local_count : 0);
	}

	if (token->kind == TOK_SWITCH)
		return parse_switch_statement();

	if (token->kind == TOK_IF) {
		Node *if_assign_member = parse_if_with_struct_assign_member_condition();
		if (if_assign_member)
			return if_assign_member;

		lexer_next();

		expect(TOK_LPAREN);
		Node *cond = expr_coerce_scalar_condition(parse_comma_expr());
		expect(TOK_RPAREN);

		Node *then_body = parse_embedded_statement();
		Node *else_body = NULL;

		if (lexer_peek()->kind == TOK_ELSE) {
			lexer_next();
			else_body = parse_embedded_statement();
		}

		return new_if(cond, then_body, else_body);
	}

	if (token->kind == TOK_DO) {
		int saved_local_count = parser_current_local_count();

		lexer_next();
		stmt_push_control_scope(saved_local_count, 1);
		Node *body = parse_embedded_statement();
		stmt_pop_control_scope();

		expect(TOK_WHILE);
		expect(TOK_LPAREN);
		Node *cond = expr_coerce_scalar_condition(parse_comma_expr());
		expect(TOK_RPAREN);
		expect(TOK_SEMI);

		return new_do_while(body, cond);
	}

	if (token->kind == TOK_WHILE) {
		int saved_local_count = parser_current_local_count();

		lexer_next();

		expect(TOK_LPAREN);
		Node *cond = expr_coerce_scalar_condition(parse_comma_expr());
		expect(TOK_RPAREN);

		stmt_push_control_scope(saved_local_count, 1);
		Node *body = parse_embedded_statement();
		stmt_pop_control_scope();
		return new_while(cond, body);
	}

	if (token->kind == TOK_FOR) {
		int saved_local_count;

		lexer_next();

		expect(TOK_LPAREN);

		Node *init = NULL;
		if (lexer_peek()->kind == TOK_SEMI) {
			lexer_next();
		} else if (block_item_starts_declaration()) {
			if (tcc_lang_is_c89_or_c90())
				fatal_cur("for-loop declarations are not allowed in C89/C90 mode\n");
			init = parse_statement();
		} else {
			init = parse_comma_expr();
			expect(TOK_SEMI);
		}

		Node *cond = NULL;
		if (lexer_peek()->kind != TOK_SEMI)
			cond = expr_coerce_scalar_condition(parse_comma_expr());
		expect(TOK_SEMI);

		Node *inc = NULL;
		if (lexer_peek()->kind != TOK_RPAREN)
			inc = parse_comma_expr();
		expect(TOK_RPAREN);

		saved_local_count = parser_current_local_count();
		stmt_push_control_scope(saved_local_count, 1);
		Node *body = parse_embedded_statement();
		stmt_pop_control_scope();
		return new_for(init, cond, inc, body);
	}

	if (token->kind == TOK_LBRACE) {
		lexer_next();

		Node *block = parse_block_contents();
		expect(TOK_RBRACE);

		return block;
	}

	if (token->kind == TOK_IDENT || token->kind == TOK_NUM || token->kind == TOK_STRING ||
	    token->kind == TOK_LPAREN || token->kind == TOK_MINUS || token->kind == TOK_NOT ||
	    token->kind == TOK_TILDE || token->kind == TOK_STAR || token->kind == TOK_AMP ||
	    token->kind == TOK_SIZEOF || token->kind == TOK_ALIGNOF ||
	    token->kind == TOK_PLUSPLUS || token->kind == TOK_MINUSMINUS) {
		Node *assign_member_call_stmt = parse_call_statement_with_struct_assign_member_arg();
		if (assign_member_call_stmt)
			return assign_member_call_stmt;

		Node *v124_call_stmt = parse_v124_call_statement_with_temps();
		if (v124_call_stmt)
			return v124_call_stmt;

		Node *compound_call_stmt = parse_call_statement_with_compound_literal_arg();
		if (compound_call_stmt)
			return compound_call_stmt;

		Node *expr = parse_comma_expr();
		expect(TOK_SEMI);
		return expr;
	}

	if (token->kind == TOK_CASE) {
		if (stmt_switch_depth <= 0)
			fatal_cur("case label not within switch statement\n");
		lexer_next();
		if (lexer_peek()->kind == TOK_NUM || lexer_peek()->kind == TOK_IDENT)
			lexer_next();
		expect(TOK_COLON);
		reject_declaration_after_label_before_c23();
		return new_num(0);
	}

	if (token->kind == TOK_DEFAULT) {
		if (stmt_switch_depth <= 0)
			fatal_cur("default label not within switch statement\n");
		lexer_next();
		expect(TOK_COLON);
		reject_declaration_after_label_before_c23();
		return new_num(0);
	}

	fatal_cur("Unexpected token while parsing statement\n");
}

static Node *
parse_integer_modifier_declaration_statement(const Token *token, int requested_align)
{
	int saw_trailing_function_specifier;
	TokenKind trailing_storage_class;

	if (token->kind != TOK_SIGNED && token->kind != TOK_UNSIGNED &&
	    token->kind != TOK_SHORT && token->kind != TOK_LONG)
		return NULL;

	int base_size = TCC_SIZEOF_INT;
	int saw_short = 0;
	int long_count = 0;
	int saw_signed = 0;
	int saw_unsigned = 0;
	int saw_int_modifier = 0;
	int complex_source_kind = TYPE_SOURCE_DEFAULT;
	int source_kind = TYPE_SOURCE_DEFAULT;
	char source_name[64] = {0};
	Type *base_type;
	Type *decl_type;
	stmt_consume_scalar_type_modifiers(&saw_short,
	                                  &saw_signed,
	                                  &saw_unsigned,
	                                  &saw_int_modifier,
	                                  &long_count,
	                                  &complex_source_kind);
	if (long_count >= 2) {
		source_kind = saw_unsigned ? TYPE_SOURCE_ULLONG : TYPE_SOURCE_LLONG;
		STRNCPY(source_name, saw_unsigned ? "unsigned long long" : "long long",
		        sizeof(source_name) - 1);
	} else if (long_count == 1) {
		source_kind = saw_unsigned ? TYPE_SOURCE_ULONG : TYPE_SOURCE_LONG;
		STRNCPY(source_name, saw_unsigned ? "unsigned long" : "long",
		        sizeof(source_name) - 1);
	}
	if (lexer_peek()->kind == TOK_CHAR) {
		base_size = 1;
		lexer_next();
	} else if (lexer_peek()->kind == TOK_INT) {
		lexer_next();
		if (long_count > 0)
			base_size = TCC_SIZEOF_LONG;
		else if (saw_short)
			base_size = 2;
	} else if (lexer_peek()->kind == TOK_DOUBLE) {
		if (long_count != 1 || saw_short || saw_signed || saw_unsigned)
			fatal_cur("invalid type specifier combination\n");
		lexer_next();
		base_type = type_double();
		if (complex_source_kind == TYPE_SOURCE_COMPLEX)
			STRNCPY(source_name, "_Complex long double", sizeof(source_name) - 1);
		else if (complex_source_kind == TYPE_SOURCE_IMAGINARY)
			STRNCPY(source_name, "_Imaginary long double", sizeof(source_name) - 1);
		else {
			source_kind = TYPE_SOURCE_LONG_DOUBLE;
			STRNCPY(source_name, "long double", sizeof(source_name) - 1);
		}
		goto have_base_type;
	} else if (lexer_peek()->kind == TOK_FLOAT) {
		fatal_cur("invalid type specifier combination\n");
	} else if (long_count > 0) {
		base_size = TCC_SIZEOF_LONG;
	} else if (saw_short) {
		base_size = 2;
	}

	base_type = type_for_size_unsigned(base_size, saw_unsigned);
	if (base_size == 1 && saw_signed && !saw_unsigned)
		base_type = stmt_apply_type_source(base_type, TYPE_SOURCE_SCHAR, "signed char");
	else if (complex_source_kind == TYPE_SOURCE_DEFAULT)
		base_type = stmt_apply_type_source(base_type, source_kind, source_name);
have_base_type:
	if (complex_source_kind != TYPE_SOURCE_DEFAULT) {
		if (!(base_type->kind == TY_FLOAT || base_type->kind == TY_DOUBLE))
			fatal_cur("invalid type specifier combination\n");
		base_type = stmt_apply_type_source(base_type, complex_source_kind, source_name);
	}
	base_type = stmt_apply_post_base_complex_specifier(base_type,
	                                                  &complex_source_kind,
	                                                  long_count);
	stmt_reject_unsupported_special_type(base_type);
	base_type = stmt_apply_post_base_qualifiers(base_type);
	if (token_starts_alignas_specifier(lexer_peek(), lexer_peek_ahead(1)))
		requested_align = stmt_merge_requested_alignment(requested_align);
	saw_trailing_function_specifier =
	    parser_type_name_saw_trailing_function_specifier();
	trailing_storage_class =
	    parser_type_name_trailing_storage_class();
	int saw_thread_local =
	    parser_type_name_saw_thread_local_storage_specifier();
	if (parser_type_name_saw_multiple_trailing_storage_classes())
		fatal_cur("multiple storage classes in declaration\n");
	if (trailing_storage_class == TOK_TYPEDEF) {
		if (saw_thread_local)
			fatal_cur("multiple storage classes in declaration\n");
		if (saw_trailing_function_specifier)
			fatal_cur("function specifier is only valid on function declarations\n");
		{
			Node *vm_typedef =
			    stmt_try_parse_local_vm_array_typedef(base_type, requested_align);
			if (vm_typedef)
				return vm_typedef;
		}
		{
			Node *vm_typedef =
			    stmt_try_parse_local_vm_pointer_array_typedef(base_type, requested_align);
			if (vm_typedef)
				return vm_typedef;
		}
		parse_typedef_declaration_after_base_type(base_type);
		return new_block(NULL);
	}
	if (saw_thread_local) {
		if (trailing_storage_class == TOK_THREAD_LOCAL &&
		    lexer_peek()->kind != TOK_IDENT &&
		    lexer_peek()->kind != TOK_STAR &&
		    lexer_peek()->kind != TOK_LPAREN)
			fatal_cur("'thread_local' is a reserved identifier in C23 and cannot be used as a local variable name\n");
		if (trailing_storage_class == TOK_STATIC ||
		    trailing_storage_class == TOK_EXTERN)
			fatal_cur("thread-local storage is not supported\n");
		fatal_cur("_Thread_local variables must have global storage\n");
	}
	if (trailing_storage_class == TOK_REGISTER)
		stmt_decl_register_request = 1;
	else if (trailing_storage_class == TOK_AUTO)
		;
	else if (trailing_storage_class == TOK_EXTERN)
		return parse_extern_declaration_after_base_type(base_type);
	else if (trailing_storage_class == TOK_STATIC)
		return parse_static_declaration_after_base_type(base_type, base_size,
		                                               requested_align,
		                                               saw_trailing_function_specifier,
		                                               parser_type_name_saw_trailing_noreturn_specifier());
	else if (trailing_storage_class == TOK_STATIC ||
	         trailing_storage_class == TOK_EXTERN ||
	         trailing_storage_class == TOK_AUTO)
		fatal_cur("multiple storage classes in declaration\n");
	int ptr_depth = 0;
	decl_type = stmt_parse_pointer_declarator_type(base_type, &ptr_depth);
	int is_ptr_decl = ptr_depth > 0;
	{
		Node *fp_decl = stmt_try_parse_function_pointer_local_after_decl_type(decl_type,
		                                                                     requested_align);
		if (fp_decl)
			return fp_decl;
	}
	if (!is_ptr_decl) {
		Node *pa_decl = stmt_parse_pointer_to_array_local(base_type, requested_align);
		if (pa_decl)
			return pa_decl;
	}

	const Token *name = lexer_peek();
	parser_require_decl_identifier(name, "local variable name");

	lexer_next();
	if (saw_trailing_function_specifier && lexer_peek()->kind != TOK_LPAREN)
		fatal_cur("function specifier is only valid on function declarations\n");
	{
		Node *func_decl =
		    stmt_try_parse_function_declaration_after_name(
		        name->text, decl_type,
		        parser_type_name_saw_trailing_noreturn_specifier());
		if (func_decl)
			return func_decl;
	}

	if (is_ptr_decl) {
		Type *ptr_type = decl_type;
		int offset = add_decl_typed_local(requested_align, name->text, ptr_type);
		Node *node = new_ptr_decl(name->text, offset);
		node->type = ptr_type;
		return stmt_finish_typed_decl_statement(node, name->text, offset, ptr_type,
		                                        requested_align,
		                                        STMT_COMMA_BASE_STRIP_ALL_PTRS,
		                                        1, 0, 1);
	}

	if (lexer_peek()->kind == TOK_LBRACKET) {
		int dims[MAX_ARRAY_DIMS] = {0};
		int dim_count = parse_array_dimensions(dims, 0, 0);
		Type *array_type = build_array_type_from_dims(clone_type(base_type), dims, dim_count);
		int offset = add_decl_typed_local(requested_align, name->text, array_type);
		Node *node = new_array_decl(name->text, offset, dims[0]);
		node->elem_size = type_elem_size(array_type);
		node->type = array_type;
		Node *head = node;
		Node *tail = node;
		while (lexer_peek()->kind == TOK_COMMA) {
			lexer_next();
			if (lexer_peek()->kind == TOK_LPAREN &&
			    lexer_peek_ahead(1)->kind == TOK_STAR) {
				lexer_next();
				lexer_next();
				const Token *pa_name = lexer_peek();
				if (pa_name->kind != TOK_IDENT) break;
				lexer_next();
				expect(TOK_RPAREN);
				Type *pa_base = clone_type(base_type);
				if (lexer_peek()->kind == TOK_LBRACKET) {
					int pa_dims[MAX_ARRAY_DIMS] = {0};
					int pa_dc = parse_array_dimensions(pa_dims, 0, 0);
					Type *pa_arr = build_array_type_from_dims_allow_incomplete(pa_base, pa_dims,
					                                                          pa_dc, 1);
					Type *pa_ptr = type_ptr(pa_arr);
					int pa_off = add_decl_typed_local(requested_align, pa_name->text, pa_ptr);
					Node *pa_node = new_ptr_decl(pa_name->text, pa_off);
					pa_node->type = pa_ptr;
					pa_node->elem_size = pa_arr->size;
					tail->next = pa_node; tail = pa_node;
				} else {
					Type *pa_ptr = type_ptr(pa_base);
					int pa_off = add_decl_typed_local(requested_align, pa_name->text, pa_ptr);
					Node *pa_node = new_ptr_decl(pa_name->text, pa_off);
					pa_node->type = pa_ptr;
					tail->next = pa_node; tail = pa_node;
				}
				continue;
			}
			int next_ptr_depth = 0;
			while (lexer_peek()->kind == TOK_STAR) { lexer_next(); next_ptr_depth++; }
			const Token *next_name = lexer_peek();
			if (next_name->kind != TOK_IDENT) break;
			lexer_next();
			if (lexer_peek()->kind == TOK_LBRACKET) {
				int next_dims[MAX_ARRAY_DIMS] = {0};
				Type *next_base = parser_canonicalize_decl_type(base_type);
				int next_dim_count = parse_array_dimensions(next_dims, 0, 0);
				Type *next_arr = build_array_type_from_dims(next_base, next_dims, next_dim_count);
				int next_off = add_decl_typed_local(requested_align, next_name->text, next_arr);
				Node *next_node = new_array_decl(next_name->text, next_off, next_dims[0]);
				next_node->elem_size = type_elem_size(next_arr);
				next_node->type = next_arr;
				tail->next = next_node; tail = next_node;
			} else if (next_ptr_depth > 0) {
				Type *ptr_type = parser_canonicalize_decl_type(base_type);
				for (int pi = 0; pi < next_ptr_depth; pi++) ptr_type = type_ptr(ptr_type);
				int next_off = add_decl_typed_local(requested_align, next_name->text, ptr_type);
				Node *next_node = new_ptr_decl(next_name->text, next_off);
				next_node->type = ptr_type;
				tail->next = next_node; tail = next_node;
			} else {
				Type *next_type = parser_canonicalize_decl_type(base_type);
				int next_off = add_decl_typed_local(requested_align, next_name->text, next_type);
				Node *next_node = new_decl(next_name->text, next_off);
				next_node->elem_size = base_size;
				next_node->type = next_type;
				tail->next = next_node; tail = next_node;
			}
		}
		stmt_expect_decl_semi();
		return head;
	}

	if (base_type->kind == TY_ARRAY && base_type->is_vm_type &&
	    base_type->vla_bound_name[0]) {
		Node *head = build_runtime_vm_typedef_array_decl_stmt_list(requested_align,
		                                                           name->text,
		                                                           base_type);
		Node *tail = stmt_node_list_tail(head);

		if (lexer_peek()->kind == TOK_ASSIGN)
			fatal_cur("Unsupported runtime VLA initializer\n");

		for (;;) {
			if (lexer_peek()->kind != TOK_COMMA)
				break;
			lexer_next();
			name = lexer_peek();
			if (name->kind != TOK_IDENT)
				fatal_cur("Expected identifier in declaration list\n");
			lexer_next();
			if (lexer_peek()->kind == TOK_LPAREN) {
				if (!stmt_try_parse_function_declaration_after_name(
				        name->text, decl_type,
				        parser_type_name_saw_trailing_noreturn_specifier()))
					fatal_cur("internal error: expected block-scope function declarator\n");
				continue;
			}
			if (lexer_peek()->kind == TOK_ASSIGN)
				fatal_cur("Unsupported runtime VLA initializer\n");
			tail = append_node(tail,
			                   build_runtime_vm_typedef_array_decl_stmt_list(
			                       requested_align, name->text, base_type));
		}
		stmt_expect_decl_semi();
		return new_block(head);
	}

	int offset = add_decl_typed_local(requested_align, name->text, base_type);
	parser_configure_last_local_type(base_type);
	Node *node = new_decl(name->text, offset);
	node->elem_size = base_size;
	node->is_unsigned = saw_unsigned;
	node->type = parser_canonicalize_decl_type(base_type);

	Node *dhead = node;
	Node *dtail = node;

	for (;;) {
			if (lexer_peek()->kind == TOK_ASSIGN) {
				Type *init_type = parser_canonicalize_decl_type(base_type);

				lexer_next();
				Node *expr = parse_local_scalar_initializer_expr(type_sizeof(init_type));
				Node *lhs = new_var(name->text, offset);
				lhs->elem_size = type_elem_size(init_type);
				lhs->is_unsigned = saw_unsigned;
				lhs->type = init_type;
				validate_pointer_initializer_compatibility(lhs->type, expr);
				dtail = append_node(dtail,
				                    stmt_build_initializer_assign(lhs, expr,
				                                                  init_type));
		}
		if (lexer_peek()->kind != TOK_COMMA)
			break;
		lexer_next();
		name = lexer_peek();
		if (name->kind != TOK_IDENT) {
			fatal_cur("Expected identifier in declaration list\n");
		}
		lexer_next();
		if (lexer_peek()->kind == TOK_LPAREN) {
			if (!stmt_try_parse_function_declaration_after_name(
			        name->text, decl_type,
			        parser_type_name_saw_trailing_noreturn_specifier()))
				fatal_cur("internal error: expected block-scope function declarator\n");
			continue;
		}
		offset = add_decl_typed_local(requested_align, name->text, base_type);
		parser_configure_last_local_type(base_type);
		Node *ndecl = new_decl(name->text, offset);
		ndecl->elem_size = base_size;
		ndecl->is_unsigned = saw_unsigned;
		ndecl->type = parser_canonicalize_decl_type(base_type);
		dtail = append_node(dtail, ndecl);
	}
	stmt_expect_decl_semi();
	return new_block(dhead);
}

static Node *
parse_scalar_declaration_statement(const Token *token, int requested_align)
{
	int saw_trailing_function_specifier;
	TokenKind trailing_storage_class;

	if (token->kind != TOK_INT && token->kind != TOK_CHAR &&
	    token->kind != TOK_BOOL &&
	    token->kind != TOK_FLOAT && token->kind != TOK_DOUBLE)
		return NULL;

	int pointer_to_array_decl = 0;
	if (lexer_peek_ahead(1)->kind == TOK_LPAREN &&
	    lexer_peek_ahead(2)->kind == TOK_STAR) {
		int i = 3;
		const Token *t = lexer_peek_ahead(i);

		while (t->kind == TOK_STAR) {
			i++;
			t = lexer_peek_ahead(i);
		}
		while (t->kind == TOK_CONST || t->kind == TOK_VOLATILE ||
		       t->kind == TOK_RESTRICT || t->kind == TOK_ATOMIC) {
			i++;
			t = lexer_peek_ahead(i);
		}
		pointer_to_array_decl =
		    t->kind == TOK_IDENT &&
		    lexer_peek_ahead(i + 1)->kind == TOK_RPAREN &&
		    lexer_peek_ahead(i + 2)->kind == TOK_LBRACKET;
	}
	reject_c89_c99_keyword_token(token->kind);
	if (!pointer_to_array_decl &&
	    parser_current_function_name()[0] == '\0' &&
	    try_parse_prototype())
		return new_block(NULL);
	int base_size = (token->kind == TOK_CHAR) ? 1 :
	                (token->kind == TOK_BOOL) ? 1 :
	                (token->kind == TOK_DOUBLE) ? 8 : 4;
	Type *scalar_type = (token->kind == TOK_CHAR) ? type_char() :
	                   (token->kind == TOK_BOOL) ? type_with_source(type_uchar(), TYPE_SOURCE_BOOL, "_Bool") :
	                   (token->kind == TOK_FLOAT) ? type_float() :
	                   (token->kind == TOK_DOUBLE) ? type_double() :
	                   type_int();
	lexer_next();

	scalar_type = stmt_apply_post_base_qualifiers(scalar_type);
	if (token_starts_alignas_specifier(lexer_peek(), lexer_peek_ahead(1)))
		requested_align = stmt_merge_requested_alignment(requested_align);
	saw_trailing_function_specifier =
	    parser_type_name_saw_trailing_function_specifier();
	trailing_storage_class =
	    parser_type_name_trailing_storage_class();
	int saw_thread_local =
	    parser_type_name_saw_thread_local_storage_specifier();
	if (parser_type_name_saw_multiple_trailing_storage_classes())
		fatal_cur("multiple storage classes in declaration\n");
	if (trailing_storage_class == TOK_TYPEDEF) {
		if (saw_thread_local)
			fatal_cur("multiple storage classes in declaration\n");
		if (saw_trailing_function_specifier)
			fatal_cur("function specifier is only valid on function declarations\n");
		parse_typedef_declaration_after_base_type(scalar_type);
		return new_block(NULL);
	}
	if (saw_thread_local) {
		if (trailing_storage_class == TOK_THREAD_LOCAL &&
		    lexer_peek()->kind != TOK_IDENT &&
		    lexer_peek()->kind != TOK_STAR &&
		    lexer_peek()->kind != TOK_LPAREN)
			fatal_cur("'thread_local' is a reserved identifier in C23 and cannot be used as a local variable name\n");
		if (trailing_storage_class == TOK_STATIC ||
		    trailing_storage_class == TOK_EXTERN)
			fatal_cur("thread-local storage is not supported\n");
		fatal_cur("_Thread_local variables must have global storage\n");
	}
	if (trailing_storage_class == TOK_REGISTER)
		stmt_decl_register_request = 1;
	else if (trailing_storage_class == TOK_AUTO)
		;
	else if (trailing_storage_class == TOK_EXTERN)
		return parse_extern_declaration_after_base_type(scalar_type);
	else if (trailing_storage_class == TOK_STATIC)
		return parse_static_declaration_after_base_type(scalar_type, base_size,
		                                               requested_align,
		                                               saw_trailing_function_specifier,
		                                               parser_type_name_saw_trailing_noreturn_specifier());
	else if (trailing_storage_class == TOK_STATIC ||
	         trailing_storage_class == TOK_EXTERN ||
	         trailing_storage_class == TOK_AUTO)
		fatal_cur("multiple storage classes in declaration\n");

	int ptr_depth = 0;
	Type *decl_type = stmt_parse_pointer_declarator_type(scalar_type, &ptr_depth);
	int is_ptr_decl = ptr_depth > 0;
	{
		Node *fp_decl = stmt_try_parse_function_pointer_local_after_decl_type(decl_type,
		                                                                     requested_align);
		if (fp_decl)
			return fp_decl;
	}
	if (!is_ptr_decl) {
		Node *pa_decl = stmt_parse_pointer_to_array_local(parser_canonicalize_decl_type(scalar_type),
		                                                 requested_align);
		if (pa_decl)
			return pa_decl;
	}

	const Token *name = lexer_peek();
	parser_require_decl_identifier(name, "local variable name");
	char local_name[64] = {0};
	STRNCPY(local_name, name->text ? name->text : "", sizeof(local_name) - 1);

	lexer_next();
	if (saw_trailing_function_specifier && lexer_peek()->kind != TOK_LPAREN)
		fatal_cur("function specifier is only valid on function declarations\n");
	{
		Node *func_decl =
		    stmt_parse_function_declaration_after_name(
		        local_name, decl_type,
		        parser_type_name_saw_trailing_noreturn_specifier());
		if (func_decl)
			return func_decl;
	}

	if (is_ptr_decl) {
		Type *ptr_type = decl_type;

		if (lexer_peek()->kind == TOK_LBRACKET) {
			int dims[MAX_ARRAY_DIMS] = {0};
			int dim_count = parse_array_dimensions(dims, 0, 0);
			Type *array_type = build_array_type_from_dims(ptr_type, dims, dim_count);
			int offset = add_decl_typed_local(requested_align, local_name, array_type);
			Node *node = new_array_decl(local_name, offset, dims[0]);
			node->elem_size = type_elem_size(array_type);
			node->type = array_type;
			if (lexer_peek()->kind == TOK_LPAREN)
				fatal_cur("array elements cannot have function type\n");
			stmt_expect_decl_semi();
			return node;
		}

		int offset = add_decl_typed_local(requested_align, local_name, ptr_type);
		Node *node = new_ptr_decl(local_name, offset);
		node->type = ptr_type;
		return stmt_finish_typed_decl_statement(node, local_name, offset, ptr_type,
		                                        requested_align,
		                                        STMT_COMMA_BASE_STRIP_ALL_PTRS,
		                                        1, 0, 1);
	}

	if (lexer_peek()->kind == TOK_LBRACKET) {
		if (array_decl_looks_runtime_vla()) {
			stmt_require_c99_for_runtime_vla();
			Type *base_type = parser_canonicalize_decl_type(scalar_type);
			return build_runtime_vla_decl_block(requested_align, local_name,
			                                    base_type, base_size);
		}

			int dims[MAX_ARRAY_DIMS] = {0};
			int dim_count = parse_array_dimensions(dims, 1, 0);
			if (lexer_peek()->kind == TOK_LPAREN)
				fatal_cur("array elements cannot have function type\n");
			int array_len = dims[0];
			int total_len = 1;
		for (int d = 0; d < dim_count; d++) {
			if (dims[d] == 0) {
				if (d != 0)
					fatal_cur("Only the first array dimension may be omitted\n");
			} else {
				total_len *= dims[d];
			}
		}

		int *init_values = NULL;
		int init_values_cap = 0;
		int init_count = 0;
		Node **init_exprs = NULL;
		int init_exprs_cap = 0;
		unsigned char *init_seen = NULL;
		int string_init = 0;
		int string_width = 1;
		int zero_fill_only = 0;
		int empty_braced_init = 0;
		char *string_value = NULL;
		size_t string_len = 0;
		Type *elem_type = parser_canonicalize_decl_type(scalar_type);
		int pointer_elements = elem_type && type_is_pointer(elem_type);
		int next_init_index = 0;
		int max_init_index = -1;

		if (lexer_peek()->kind == TOK_ASSIGN) {
			lexer_next();

			if (dim_count > 1 && lexer_peek()->kind == TOK_LBRACE) {
				Type *array_type = build_array_type_from_dims(parser_canonicalize_decl_type(scalar_type), dims, dim_count);
				stmt_parse_local_multidim_array_initializer(array_type,
				                                            pointer_elements,
				                                            0,
				                                            0,
				                                            &init_values_cap,
				                                            &init_values,
				                                            &init_exprs_cap,
				                                            &init_exprs,
				                                            &init_seen,
				                                            &max_init_index,
				                                            &init_count);
			} else if (lexer_peek()->kind == TOK_LBRACE) {
				lexer_next();
				empty_braced_init = (lexer_peek()->kind == TOK_RBRACE);
				stmt_parse_braced_local_array_initializer(elem_type, pointer_elements,
					array_len, 0, 0,
					&next_init_index, &max_init_index, &init_count,
					&init_values_cap, &init_values, &init_exprs_cap, &init_exprs,
					&init_seen);
			} else if (lexer_peek()->kind == TOK_STRING) {
				const Token *value = lexer_peek();
				stmt_require_string_literal_array_match(value, base_size,
				                                        "String literal element width does not match local array element type");
				string_init = 1;
				string_width = stmt_string_literal_elem_width(value);
				string_len = value->text_len;
				string_value = xmalloc(string_len + 1);
				memcpy(string_value, value->text, string_len);
				string_value[string_len] = '\0';
				init_count = string_width > 1
					? (int)(string_len / (size_t)string_width) + 1
					: (int)string_len + 1;
				lexer_next();
			} else {
				fatal_cur("Unsupported local array initializer\n");
			}
		}

		if (array_len == 0)
			array_len = init_count;

		if (array_len <= 0) {
			fatal_cur("Array length must be positive: %s\n", local_name);
		}

		if (empty_braced_init) {
			zero_fill_only = 1;
			init_count = 1;
		}

		if (dims[0] == 0)
			dims[0] = array_len;

		if (dim_count > 1)
			total_len = array_len * (total_len / (dims[0] ? dims[0] : 1));
		else
			total_len = array_len;

		if (!zero_fill_only && init_count > total_len) {
			fatal_cur("Too many initializers for local array\n");
		}

		Type *array_type = build_array_type_from_dims(parser_canonicalize_decl_type(scalar_type), dims, dim_count);
		int offset = add_decl_typed_local(requested_align, local_name, array_type);
		Node *decl = new_array_decl(local_name, offset, array_len);
		decl->elem_size = type_elem_size(array_type);
		decl->type = array_type;

		Node *comma_extra = NULL;
		Node *comma_tail = NULL;
		if (init_count == 0) {
			while (lexer_peek()->kind == TOK_COMMA) {
				lexer_next();
				if (lexer_peek()->kind == TOK_LPAREN &&
				    lexer_peek_ahead(1)->kind == TOK_STAR) {
					lexer_next();
					lexer_next();
					const Token *pa2_name = lexer_peek();
					if (pa2_name->kind != TOK_IDENT) break;
					lexer_next();
					expect(TOK_RPAREN);
					Type *pa2_base = type_for_size(base_size);
					if (lexer_peek()->kind == TOK_LBRACKET) {
						int pa2_dims[MAX_ARRAY_DIMS] = {0};
						int pa2_dc = parse_array_dimensions(pa2_dims, 0, 0);
						Type *pa2_arr = build_array_type_from_dims_allow_incomplete(pa2_base,
						                                                           pa2_dims,
						                                                           pa2_dc, 1);
						Type *pa2_ptr = type_ptr(pa2_arr);
						int pa2_off = add_decl_typed_local(requested_align, pa2_name->text, pa2_ptr);
						Node *pa2_node = new_ptr_decl(pa2_name->text, pa2_off);
						pa2_node->type = pa2_ptr;
						pa2_node->elem_size = pa2_arr->size;
						if (!comma_extra) comma_extra = pa2_node; else comma_tail->next = pa2_node;
						comma_tail = pa2_node;
					} else {
						Type *pa2_ptr = type_ptr(pa2_base);
						int pa2_off = add_decl_typed_local(requested_align, pa2_name->text, pa2_ptr);
						Node *pa2_node = new_ptr_decl(pa2_name->text, pa2_off);
						pa2_node->type = pa2_ptr;
						if (!comma_extra) comma_extra = pa2_node; else comma_tail->next = pa2_node;
						comma_tail = pa2_node;
					}
					continue;
				}
				int nstar = 0;
				while (lexer_peek()->kind == TOK_STAR) { lexer_next(); nstar++; }
				const Token *next_name = lexer_peek();
				if (next_name->kind != TOK_IDENT) break;
				lexer_next();
				if (lexer_peek()->kind == TOK_LPAREN) {
					if (!stmt_try_parse_function_declaration_after_name(
					        next_name->text, parser_canonicalize_decl_type(scalar_type),
					        parser_type_name_saw_trailing_noreturn_specifier()))
						fatal_cur("internal error: expected block-scope function declarator\n");
					continue;
				}
				if (nstar > 0) {
					Type *npt = parser_canonicalize_decl_type(scalar_type);
					for (int pi = 0; pi < nstar; pi++) npt = type_ptr(npt);
					int noff = add_decl_typed_local(requested_align, next_name->text, npt);
					Node *nd = new_ptr_decl(next_name->text, noff);
					nd->type = npt;
					if (!comma_extra) comma_extra = nd; else comma_tail->next = nd;
					comma_tail = nd;
					continue;
				}
				if (lexer_peek()->kind == TOK_LBRACKET) {
					int ndims[MAX_ARRAY_DIMS] = {0};
					int ndc = parse_array_dimensions(ndims, 1, 0);
					Type *narr = build_array_type_from_dims(parser_canonicalize_decl_type(scalar_type), ndims, ndc);
					int noff = add_decl_typed_local(requested_align, next_name->text, narr);
					Node *nd = new_array_decl(next_name->text, noff, ndims[0]);
					nd->elem_size = type_elem_size(narr); nd->type = narr;
					if (!comma_extra) comma_extra = nd; else comma_tail->next = nd;
					comma_tail = nd;
				} else {
					int noff;
					noff = add_decl_scalar_local(requested_align, next_name->text, base_size);
					Node *nd = new_decl(next_name->text, noff);
					nd->elem_size = base_size; nd->type = type_for_size(base_size);
					if (!comma_extra) comma_extra = nd; else comma_tail->next = nd;
					comma_tail = nd;
				}
			}
		}

		if (init_count == 0) {
			stmt_expect_decl_semi();
			if (comma_extra) { decl->next = comma_extra; return decl; }
			return decl;
		}

		Node head = {0};
		Node *cur = &head;
		append_stmt(&cur, decl);

		if (zero_fill_only) {
			for (int i = 0; i < total_len; i++) {
				append_stmt(&cur,
				            make_local_array_store(local_name, offset, base_size, i, 0));
			}
		} else if (string_init) {
			for (int i = 0; i < init_count; i++) {
				int val = 0;
				if (string_width > 1) {
					int bi;
					for (bi = 0; bi < string_width; bi++) {
						size_t byte_idx = (size_t)i * (size_t)string_width + (size_t)bi;
						if (byte_idx < string_len)
							val |= (unsigned char)string_value[byte_idx] << (8 * bi);
					}
				} else {
					val = (unsigned char)string_value[i];
				}
				append_stmt(&cur,
				            make_local_array_store(local_name, offset, base_size, i, val));
			}
		} else {
			if (pointer_elements) {
				for (int i = 0; i < total_len; i++) {
					if (i < init_exprs_cap && init_seen && init_seen[i])
						append_stmt(&cur, make_local_array_assign_expr(local_name, offset, elem_type, i, init_exprs[i]));
					else
						append_stmt(&cur, make_local_array_assign_expr(local_name, offset, elem_type, i, new_num(0)));
				}
			} else {
				for (int i = 0; i < total_len; i++) {
					if (i < init_values_cap && init_seen && init_seen[i]) {
						append_stmt(&cur,
						            make_local_array_store(local_name, offset, base_size, i, init_values[i]));
					} else {
						append_stmt(&cur,
						            make_local_array_store(local_name, offset, base_size, i, 0));
					}
				}
			}
		}

		xfree(init_values);
		xfree(init_exprs);
		xfree(init_seen);
		xfree(string_value);
		head.next = stmt_append_comma_after_array_initializer(head.next,
		                                                      scalar_type,
		                                                      requested_align);
		stmt_expect_decl_semi();
		return new_block(head.next);
	}

	int offset;
	offset = add_decl_scalar_local(requested_align, local_name, base_size);
	parser_configure_last_local_type(scalar_type);
	Node *node = new_decl(local_name, offset);
	node->elem_size = base_size;
	node->type = parser_canonicalize_decl_type(scalar_type);

	Node *head = node;
	Node *cur_tail = node;

	for (;;) {
		if (lexer_peek()->kind == TOK_ASSIGN) {
			lexer_next();

			Node *setup = NULL;
			Node *expr = parse_struct_assign_member_sum(&setup);
				if (!expr)
					expr = parse_local_scalar_initializer_expr(base_size);

			Node *lhs = new_var(local_name, offset);
			lhs->elem_size = base_size;
			lhs->type = parser_canonicalize_decl_type(scalar_type);
			validate_pointer_initializer_compatibility(lhs->type, expr);
			Node *assign = stmt_build_initializer_assign(lhs, expr, scalar_type);

			if (setup)
				cur_tail = append_node(cur_tail, setup);
			cur_tail = append_node(cur_tail, assign);
		}

		if (lexer_peek()->kind != TOK_COMMA)
			break;
		lexer_next();

		int list_ptr_depth = 0;
		while (lexer_peek()->kind == TOK_STAR) {
			lexer_next();
			list_ptr_depth++;
		}

		name = lexer_peek();
		parser_require_decl_identifier(name, "local variable name");
		STRNCPY(local_name, name->text ? name->text : "", sizeof(local_name) - 1);
		lexer_next();

		if (lexer_peek()->kind == TOK_LPAREN) {
			if (!stmt_try_parse_function_declaration_after_name(
			        local_name, scalar_type,
			        parser_type_name_saw_trailing_noreturn_specifier()))
				fatal_cur("internal error: expected block-scope function declarator\n");
			continue;
		}

		if (list_ptr_depth > 0) {
			Type *lptr_type = parser_canonicalize_decl_type(clone_type(scalar_type));
			for (int i = 0; i < list_ptr_depth; i++)
				lptr_type = type_ptr(lptr_type);
			offset = add_decl_typed_local(requested_align, local_name, lptr_type);
			Node *pdecl = new_ptr_decl(local_name, offset);
			pdecl->type = lptr_type;
			cur_tail = append_node(cur_tail, pdecl);
		} else {
			offset = add_decl_scalar_local(requested_align, local_name, base_size);
			parser_configure_last_local_type(scalar_type);
			Node *ndecl = new_decl(local_name, offset);
			ndecl->elem_size = base_size;
			ndecl->type = parser_canonicalize_decl_type(clone_type(scalar_type));
			cur_tail = append_node(cur_tail, ndecl);
		}
	}

	expect(TOK_SEMI);
	return new_block(head);
}

static int
stmt_type_is_named_aggregate(const Type *type)
{
	return type &&
	       (type_is_struct(type) || type_is_union(type)) &&
	       type->struct_name[0];
}

static Node *
stmt_try_parse_named_aggregate_array_initializer(const char *name, int offset,
                                                 Type *array_type, Node *decl)
{
	Type *elem_type;

	if (!array_type || !type_is_array(array_type) ||
	    lexer_peek()->kind != TOK_ASSIGN)
		return NULL;

	elem_type = parser_canonicalize_decl_type(type_pointee(array_type));
	if (!stmt_type_is_named_aggregate(elem_type))
		return NULL;

	return parse_struct_array_initializer_block(name, elem_type->struct_name,
	                                            offset, array_type->array_len,
	                                            decl);
}

static Node *
stmt_try_parse_function_pointer_local_after_decl_type(Type *ret_type,
                                                      int requested_align)
{
	int extra_star_count = 0;
	int fp_dims[MAX_ARRAY_DIMS] = {0};
	int fp_dim_count = 0;
	Type **fp_param_types = NULL;
	int fp_param_count = 0;
	int fp_is_variadic = 0;
	int fp_fixed_params = 0;
	int fp_has_prototype = 0;
	Type *fp_ptr_type;
	Type *decl_type;
	int elem_size;
	int offset;
	Node *decl;
	const Token *name;

	if (lexer_peek()->kind != TOK_LPAREN ||
	    lexer_peek_ahead(1)->kind != TOK_STAR)
		return NULL;
	{
		int i = 2;
		const Token *t;

		while (lexer_peek_ahead(i)->kind == TOK_STAR)
			i++;
		t = lexer_peek_ahead(i);
		while (t->kind == TOK_CONST || t->kind == TOK_VOLATILE ||
		       t->kind == TOK_RESTRICT || t->kind == TOK_ATOMIC) {
			i++;
			t = lexer_peek_ahead(i);
		}
		if (t->kind == TOK_IDENT &&
		    lexer_peek_ahead(i + 1)->kind == TOK_RPAREN &&
		    lexer_peek_ahead(i + 2)->kind == TOK_LBRACKET) {
			return NULL;
		}
	}

	lexer_next();
	lexer_next();
	while (lexer_peek()->kind == TOK_STAR) {
		lexer_next();
		extra_star_count++;
	}
	skip_pointer_qualifiers();

	name = lexer_peek();
	if (name->kind == TOK_LPAREN) {
		Type **outer_param_types = NULL;
		int outer_param_count = 0;
		int outer_is_variadic = 0;
		int outer_fixed_params = 0;
		int outer_has_prototype = 0;
		Type **retfp_param_types[8] = {0};
		int retfp_param_count[8] = {0};
		int retfp_is_variadic[8] = {0};
		int retfp_fixed_params[8] = {0};
		int retfp_has_prototype[8] = {0};
		int retfp_level_count = 0;
		Type *nested_ret_type;

		lexer_next();
		expect(TOK_STAR);
		skip_pointer_qualifiers();
		name = lexer_peek();
		if (name->kind != TOK_IDENT)
			return NULL;
		lexer_next();

		expect(TOK_RPAREN);
		parse_prototype_param_list(&outer_param_types, &outer_param_count,
		                          &outer_is_variadic, &outer_fixed_params,
		                          &outer_has_prototype, 1);
		expect(TOK_RPAREN);

		while (lexer_peek()->kind == TOK_LPAREN) {
			if (retfp_level_count >= 8)
				fatal_cur("Too many nested function-pointer return declarators\n");
			parse_prototype_param_list(&retfp_param_types[retfp_level_count],
			                          &retfp_param_count[retfp_level_count],
			                          &retfp_is_variadic[retfp_level_count],
			                          &retfp_fixed_params[retfp_level_count],
			                          &retfp_has_prototype[retfp_level_count], 1);
			retfp_level_count++;
		}

		nested_ret_type = clone_type(ret_type);
		for (int level = retfp_level_count - 1; level >= 0; level--) {
			nested_ret_type =
			    type_ptr(retfp_has_prototype[level]
			                 ? parser_make_function_type(nested_ret_type,
			                                             retfp_param_types[level],
			                                             retfp_param_count[level],
			                                             retfp_is_variadic[level],
			                                             retfp_fixed_params[level])
			                 : type_func(clone_type(nested_ret_type)));
		}

		decl_type = type_ptr(outer_has_prototype
		                         ? parser_make_function_type(nested_ret_type,
		                                                     outer_param_types,
		                                                     outer_param_count,
		                                                     outer_is_variadic,
		                                                     outer_fixed_params)
		                         : type_func(clone_type(nested_ret_type)));
		offset = add_decl_typed_local(requested_align, name->text, decl_type);
		parser_override_local_type(name->text, offset, decl_type, TCC_SIZEOF_PTR);
		decl = new_ptr_decl(name->text, offset);
		decl->type = decl_type;
		decl->elem_size = TCC_SIZEOF_PTR;

		if (lexer_peek()->kind == TOK_ASSIGN) {
			Node *rhs;

			lexer_next();
			if (lexer_peek()->kind == TOK_IDENT && find_func(lexer_peek()->text)) {
				const Token *fn = lexer_peek();
				lexer_next();
				rhs = parser_make_function_designator(fn->text);
			} else {
				rhs = parse_expr();
			}
			expect(TOK_SEMI);
			{
				Node *lhs = new_var(name->text, offset);
				lhs->is_pointer = 1;
				lhs->elem_size = TCC_SIZEOF_PTR;
				lhs->type = decl_type;
				validate_pointer_initializer_compatibility(decl_type, rhs);
				return new_block(append_node(decl, new_assign(lhs, rhs)));
			}
		}

		expect(TOK_SEMI);
		return decl;
	}
	if (name->kind != TOK_IDENT)
		fatal_cur("Expected function pointer name\n");
	lexer_next();

	if (lexer_peek()->kind == TOK_LBRACKET)
		fp_dim_count = parse_array_dimensions(fp_dims, 0, 0);

	expect(TOK_RPAREN);
	parse_prototype_param_list(&fp_param_types, &fp_param_count,
	                          &fp_is_variadic, &fp_fixed_params,
	                          &fp_has_prototype, 1);

	fp_ptr_type = type_ptr(fp_has_prototype
	                       ? parser_make_function_type(clone_type(ret_type),
	                                                   fp_param_types,
	                                                   fp_param_count,
	                                                   fp_is_variadic,
	                                                   fp_fixed_params)
	                       : type_func(clone_type(ret_type)));
	for (int i = 0; i < extra_star_count; i++)
		fp_ptr_type = type_ptr(fp_ptr_type);

	decl_type = fp_ptr_type;
	if (fp_dim_count > 0)
		decl_type = build_array_type_from_dims(fp_ptr_type, fp_dims, fp_dim_count);

	elem_size = type_sizeof(ret_type);
	if (elem_size <= 0)
		elem_size = TCC_SIZEOF_PTR;

	offset = add_decl_typed_local(requested_align, name->text, decl_type);
	if (fp_dim_count > 0) {
		decl = new_array_decl(name->text, offset, fp_dims[0]);
		decl->elem_size = type_elem_size(decl_type);
	} else {
		decl = new_ptr_decl(name->text, offset);
		decl->is_pointer = 1;
		decl->elem_size = elem_size;
	}
	decl->type = decl_type;
	parser_override_local_type(name->text, offset, decl_type,
	                           fp_dim_count > 0 ? type_elem_size(decl_type) : elem_size);

	if (lexer_peek()->kind == TOK_ASSIGN) {
		lexer_next();
		if (fp_dim_count > 0) {
			Node *init = stmt_build_local_function_pointer_array_initializer(name->text, offset,
			                                                                fp_ptr_type,
			                                                                fp_dims[0]);
			expect(TOK_SEMI);
			return new_block(append_node(decl, init));
		} else {
			Node *rhs = stmt_parse_function_pointer_initializer_expr(NULL);
			Node *lhs;

			expect(TOK_SEMI);
			lhs = new_var(name->text, offset);
			lhs->is_pointer = 1;
			lhs->elem_size = elem_size;
			lhs->type = decl_type;
			validate_pointer_initializer_compatibility(decl_type, rhs);
			return new_block(append_node(decl, new_assign(lhs, rhs)));
		}
	}

	expect(TOK_SEMI);
	return decl;
}


static Node *
stmt_try_parse_struct_pointer_compound_literal_initializer(
	const char *var_name, int offset, Type *decl_type,
	const char *struct_name, Node *decl_node)
{
	int outer_paren = 0;
	const Token *compound_struct_name;
	StructDef *def;
	char temp_name[64];
	int temp_offset;
	Node *temp_decl;
	Node *head;
	Node *temp_ref;
	Node *addr;
	Node *lhs;
	Node *assign;

	if (lexer_peek()->kind != TOK_AMP || lexer_peek_ahead(1)->kind != TOK_LPAREN)
		return NULL;
	if (lexer_peek_ahead(2)->kind == TOK_LPAREN &&
	    lexer_peek_ahead(3)->kind == TOK_STRUCT) {
		outer_paren = 1;
	} else if (lexer_peek_ahead(2)->kind != TOK_STRUCT) {
		return NULL;
	}

	if (tcc_lang_is_c89_or_c90())
		fatal_cur("compound literals are not allowed in C89/C90 mode\n");

	lexer_next(); /* & */
	if (outer_paren)
		lexer_next(); /* outer ( */
	expect(TOK_LPAREN);
	expect(TOK_STRUCT);

	compound_struct_name = lexer_peek();
	if (compound_struct_name->kind != TOK_IDENT)
		fatal_cur("Expected struct name in compound literal\n");
	if (STRCMP(compound_struct_name->text, struct_name) != 0)
		fatal_cur("Struct pointer compound literal initializer type mismatch\n");
	lexer_next();
	expect(TOK_RPAREN);
	expect(TOK_LBRACE);

	def = find_struct(struct_name);
	snprintf(temp_name, sizeof(temp_name), "__compound_ptr_%d",
	         parser_alloc_compound_arg_temp_id());
	temp_offset = add_struct_local(temp_name, struct_name);
	temp_decl = new_struct_decl(temp_name, temp_offset);
	temp_decl->type = type_struct(struct_name, def->size);

	head = append_local_zero_fill(temp_decl, temp_name, temp_offset, def->size);
	head = parse_struct_initializer_values(def, struct_name, temp_offset, head);
	parser_expect_local_aggregate_initializer_close(def);
	if (outer_paren)
		expect(TOK_RPAREN);

	temp_ref = new_var(temp_name, temp_offset);
	temp_ref->type = type_struct(struct_name, def->size);
	temp_ref->elem_size = def->size;
	STRNCPY(temp_ref->struct_name, struct_name, sizeof(temp_ref->struct_name) - 1);
	addr = new_addr(temp_ref);

	lhs = new_var(var_name, offset);
	lhs->is_pointer = 1;
	lhs->type = clone_type(decl_type);
	validate_pointer_initializer_compatibility(decl_type, addr);
	assign = new_assign(lhs, addr);

	expect(TOK_SEMI);
	return new_block(append_node(append_node(decl_node, head), assign));
}


static int
return_expr_may_need_special_lowering(void)
{
	const Token *tok = lexer_peek();

	if (tok->kind == TOK_SEMI)
		return 0;

	if (tok->kind == TOK_IDENT &&
	    lexer_peek_ahead(1)->kind == TOK_LPAREN) {
		FuncInfo *fi = find_func(tok->text);
		if (fi && fi->returns_struct)
			return 1;
	}

	if (tok->kind == TOK_LPAREN &&
	    lexer_peek_ahead(1)->kind == TOK_IDENT &&
	    lexer_peek_ahead(2)->kind == TOK_ASSIGN &&
	    lexer_peek_ahead(3)->kind == TOK_IDENT &&
	    lexer_peek_ahead(4)->kind == TOK_RPAREN &&
	    lexer_peek_ahead(5)->kind == TOK_DOT)
		return 1;

	if (v124_call_starts_with_struct_temp_arg())
		return 1;

	if (tok->kind == TOK_IDENT &&
	    lexer_peek_ahead(1)->kind == TOK_LPAREN &&
	    lexer_peek_ahead(2)->kind == TOK_IDENT &&
	    lexer_peek_ahead(3)->kind == TOK_LPAREN) {
		FuncInfo *inner = find_func(lexer_peek_ahead(2)->text);
		if (inner && inner->returns_struct)
			return 1;
	}

	if (tok->kind == TOK_IDENT &&
	    lexer_peek_ahead(1)->kind == TOK_LPAREN &&
	    lexer_peek_ahead(2)->kind == TOK_LPAREN &&
	    lexer_peek_ahead(3)->kind == TOK_STRUCT)
		return 1;

	return 0;
}

static void
validate_return_statement_form(int has_expr)
{
	Type *ret_type = parser_current_function_return_type();

	if (!ret_type)
		return;
	if (pfunc.is_noreturn)
		fatal_cur("function '%s' declared 'noreturn' should not return\n",
		          parser_current_function_name());
	if (type_is_void(ret_type)) {
		if (has_expr)
			fatal_cur("void function should not return a value\n");
	} else if (!has_expr) {
		fatal_cur("non-void function should return a value\n");
	}
}

static Node *
parse_struct_union_declaration_statement(const Token *token, int requested_align)
{
	if (token->kind != TOK_STRUCT && token->kind != TOK_UNION)
		return NULL;

	int is_union = (token->kind == TOK_UNION);
	lexer_next();

	/* Anonymous struct/union: "struct { ... } var" */
	char anon_name[64];
	const Token *struct_name = lexer_peek();
	const char *resolved_struct_name = NULL;
	if (struct_name->kind == TOK_LBRACE) {
		snprintf(anon_name, sizeof(anon_name), "__anon_%s_%d",
		         is_union ? "union" : "struct", ++parser_anon_struct_id);
		if (parser_has_struct_capacity()) {
			StructDef *adef = structs_push();
			memset(adef, 0, sizeof(*adef));
			STRNCPY(adef->name, anon_name, sizeof(adef->name) - 1);
			adef->is_union = is_union;
			parse_struct_body_into(adef);
			/* parse_struct_body_into may grow ptab.structs[], so don't keep adef. */
			resolved_struct_name = anon_name;
		} else {
			resolved_struct_name = anon_name;
		}
	} else if (struct_name->kind != TOK_IDENT) {
		fatal_cur("Expected struct name\n");
	} else {
		resolved_struct_name = struct_name->text;
		lexer_next();
		/* "struct S { ... } var" — inline struct definition */
		if (lexer_peek()->kind == TOK_LBRACE) {
			StructDef *idef = get_or_add_forward_struct(resolved_struct_name);
			memset(idef, 0, sizeof(*idef));
			STRNCPY(idef->name, resolved_struct_name, sizeof(idef->name) - 1);
			idef->is_union = is_union;
			parse_struct_body_into(idef);
		}
	}

	if (token_starts_alignas_specifier(lexer_peek(), lexer_peek_ahead(1)))
		requested_align = stmt_merge_requested_alignment(requested_align);

	/* "struct T { ... };" or "struct T;" as a standalone statement — no variable */
	if (lexer_peek()->kind == TOK_SEMI) {
		lexer_next();
		return new_block(NULL);
	}
	/* "struct T { ... };" with a body but no var — already defined inline above.
	 * If next is still LBRACE it means we did NOT parse a body (named, forward ref).
	 * Parse and discard if the struct already exists (scoped redefinition). */
	if (lexer_peek()->kind == TOK_LBRACE && find_struct_or_null(resolved_struct_name)) {
		/* Shadow: skip the body without clobbering the existing definition */
		int depth = 1;
		lexer_next();
		while (depth > 0 && lexer_peek()->kind != TOK_EOF) {
			if (lexer_peek()->kind == TOK_LBRACE) depth++;
			else if (lexer_peek()->kind == TOK_RBRACE) depth--;
			lexer_next();
		}
		expect(TOK_SEMI);
		return new_block(NULL);
	}

	int ptr_depth = 0;
	while (lexer_peek()->kind == TOK_STAR) {
		lexer_next();
		ptr_depth++;
	}
	int is_ptr_decl = ptr_depth > 0;

	const Token *var_name = lexer_peek();
	if (var_name->kind != TOK_IDENT) {
		fatal_cur("Expected variable name after aggregate type\n");
	}

	lexer_next();

	StructDef *def = find_struct(resolved_struct_name);
	if (!is_ptr_decl && (!def || !def->is_complete))
		fatal_cur("variable cannot have incomplete type\n");

	if (!is_ptr_decl && lexer_peek()->kind == TOK_LBRACKET) {
		int infer_len = 0;
		int array_len = 0;

		lexer_next();

		const Token *len = lexer_peek();
		if (len->kind == TOK_RBRACKET) {
			infer_len = 1;
			lexer_next();
		} else {
			if (len->kind != TOK_NUM) {
				fatal_cur("Expected numeric struct array length\n");
			}
			array_len = len->value;
			lexer_next();
			expect(TOK_RBRACKET);
		}

		if (infer_len) {
			if (lexer_peek()->kind != TOK_ASSIGN)
				fatal_cur("Unsized local struct array requires initializer\n");
			/* We cannot know the exact length until the initializer is parsed.
			 * Allocate a generous temporary bound, then correct the AST/local
			 * metadata to the actual initializer count after parsing. */
			array_len = 256;
		}

		Type *base_type = def->is_union ? type_union(resolved_struct_name, def->size)
		                                : type_struct(resolved_struct_name, def->size);
		parser_set_decl_align_request(requested_align);
		int offset = add_typed_array_local(var_name->text, base_type, array_len);
		parser_clear_decl_align_request();
		Node *node = new_array_decl(var_name->text, offset, infer_len ? -1 : array_len);
		node->elem_size = def->size;
		node->type = type_array(base_type, infer_len ? -1 : array_len);

		Debug(1, "LOCAL_STRUCT_ARRAY_DECL name=%s struct=%s len=%d infer=%d tok=%s next=%s\n",
		      var_name->text, resolved_struct_name ? resolved_struct_name : "<anon>",
		      infer_len ? -1 : array_len, infer_len,
		      token_debug_name(lexer_peek()->kind),
		      token_debug_name(lexer_peek_ahead(1)->kind));

		if (lexer_peek()->kind == TOK_ASSIGN)
			return parse_struct_array_initializer_block(var_name->text, resolved_struct_name,
			        offset, array_len, node);

		if (lexer_peek()->kind == TOK_COMMA) {
			Node *extra_head = node;
			Type *base_elem_type = def->is_union ? type_union(resolved_struct_name, def->size)
			                                     : type_struct(resolved_struct_name, def->size);

			while (lexer_peek()->kind == TOK_COMMA) {
				Type **param_types = NULL;
				int param_count = 0;
				int is_variadic = 0;
				int fixed_params = 0;
				int has_prototype = 0;
				int extra_stars = 0;
				const Token *next_name;
				Type *decl_type2;
				int noff2;
				Node *nnode2;

				lexer_next();
				while (lexer_peek()->kind == TOK_STAR) {
					lexer_next();
					extra_stars++;
				}

				next_name = lexer_peek();
				if (next_name->kind != TOK_IDENT)
					break;
				lexer_next();

				decl_type2 = clone_type(base_elem_type);
				for (int i = 0; i < extra_stars; i++)
					decl_type2 = type_ptr(decl_type2);

				if (lexer_peek()->kind == TOK_LPAREN) {
					parse_prototype_param_list(&param_types, &param_count,
					                           &is_variadic, &fixed_params,
					                           &has_prototype, 1);
					if (lexer_peek()->kind == TOK_LBRACKET)
						fatal_cur("function return array declarators are not supported\n");
					if (lexer_peek()->kind == TOK_LPAREN)
						fatal_cur("function cannot return function type\n");
					if (lexer_peek()->kind == TOK_ASSIGN)
						fatal_cur("function declaration cannot have initializer\n");
					parser_declare_function(next_name->text, decl_type2, has_prototype,
					                        param_types, param_count, is_variadic,
					                        fixed_params, 0);
					parser_note_block_scope_function_declaration(next_name->text, decl_type2);
					continue;
				}

				if (lexer_peek()->kind == TOK_LBRACKET) {
					int dims2[MAX_ARRAY_DIMS] = {0};
					int dc2 = parse_array_dimensions(dims2, 1, 0);
					Type *array_type2 = build_array_type_from_dims(decl_type2, dims2, dc2);
					noff2 = add_decl_typed_local(requested_align, next_name->text, array_type2);
					nnode2 = new_array_decl(next_name->text, noff2, dims2[0]);
					nnode2->elem_size = type_elem_size(array_type2);
					nnode2->type = array_type2;
				} else if (extra_stars > 0) {
					parser_set_decl_align_request(requested_align);
					noff2 = add_struct_pointer_local_depth(next_name->text,
					                                      resolved_struct_name, extra_stars);
					parser_clear_decl_align_request();
					nnode2 = new_struct_decl(next_name->text, noff2);
					nnode2->type = decl_type2;
				} else {
					parser_set_decl_align_request(requested_align);
					noff2 = add_struct_local(next_name->text, resolved_struct_name);
					parser_clear_decl_align_request();
					nnode2 = new_struct_decl(next_name->text, noff2);
					nnode2->type = decl_type2;
					STRNCPY(nnode2->struct_name, resolved_struct_name,
					        sizeof(nnode2->struct_name) - 1);
				}

				extra_head = append_node(extra_head, nnode2);
			}

			expect(TOK_SEMI);
			return new_block(extra_head);
		}

		expect(TOK_SEMI);
		return node;
	}

	int offset;
	Type *decl_type = def->is_union ? type_union(resolved_struct_name, def->size)
	                                : type_struct(resolved_struct_name, def->size);
	if (is_ptr_decl) {
		for (int i = 0; i < ptr_depth; i++)
			decl_type = type_ptr(decl_type);
		parser_set_decl_align_request(requested_align);
		offset = add_struct_pointer_local_depth(var_name->text, resolved_struct_name, ptr_depth);
		parser_clear_decl_align_request();
	} else {
		parser_set_decl_align_request(requested_align);
		offset = add_struct_local(var_name->text, resolved_struct_name);
		parser_clear_decl_align_request();
	}

	Node *node = new_struct_decl(var_name->text, offset);
	node->type = decl_type;

	if (!is_ptr_decl)
		return parse_struct_initializer_block(var_name->text, resolved_struct_name, offset, node);

	/* struct pointer local: handle optional initializer */
	if (lexer_peek()->kind == TOK_ASSIGN) {
		Node *compound_init;

		lexer_next();
		compound_init = stmt_try_parse_struct_pointer_compound_literal_initializer(
		    var_name->text, offset, decl_type, resolved_struct_name, node);
		if (compound_init)
			return compound_init;

		Node *rhs = parse_assignment();
		Node *decl = new_struct_decl(var_name->text, offset);
		Node *lhs = new_var(var_name->text, offset);
		lhs->is_pointer = 1;
		lhs->type = clone_type(decl_type);
		validate_pointer_initializer_compatibility(decl_type, rhs);
		Node *assign = new_assign(lhs, rhs);
		expect(TOK_SEMI);
		return new_block(append_node(decl, assign));
	}

	/* Handle comma-separated declarators: struct T *p1, *p2; */
	if (lexer_peek()->kind == TOK_COMMA) {
		Node *extra_head = node;
		while (lexer_peek()->kind == TOK_COMMA) {
			int extra_stars3 = 0;
			const Token *nn3;
			Type *decl_type3;
			int noff3;
			Node *nnode3;
			int dims3[MAX_ARRAY_DIMS] = {0};
			int dc3 = 0;
			Type *elem_t3 = NULL;
			int ei3b = 0;
			lexer_next();
			while (lexer_peek()->kind == TOK_STAR) { lexer_next(); extra_stars3++; }
			nn3 = lexer_peek();
			if (nn3->kind != TOK_IDENT) break;
			lexer_next();
			decl_type3 = def->is_union ? type_union(resolved_struct_name, def->size)
			                            : type_struct(resolved_struct_name, def->size);
			for (ei3b = 0; ei3b < extra_stars3; ei3b++)
				decl_type3 = type_ptr(decl_type3);
			if (lexer_peek()->kind == TOK_LPAREN) {
				if (!stmt_try_parse_function_declaration_after_name(
				        nn3->text, decl_type3,
				        parser_type_name_saw_trailing_noreturn_specifier()))
					fatal_cur("internal error: expected block-scope function declarator\n");
				continue;
			}
			if (lexer_peek()->kind == TOK_LBRACKET) {
				dc3 = parse_array_dimensions(dims3, 1, 0);
				elem_t3 = def->is_union ? type_union(resolved_struct_name, def->size)
				                         : type_struct(resolved_struct_name, def->size);
				for (ei3b = 0; ei3b < extra_stars3; ei3b++) elem_t3 = type_ptr(elem_t3);
				decl_type3 = build_array_type_from_dims(elem_t3, dims3, dc3);
				noff3 = add_decl_typed_local(requested_align, nn3->text, decl_type3);
				nnode3 = new_array_decl(nn3->text, noff3, dims3[0]);
				nnode3->elem_size = type_elem_size(elem_t3);
				nnode3->type = decl_type3;
			} else if (extra_stars3 > 0) {
				parser_set_decl_align_request(requested_align);
				noff3 = add_struct_pointer_local_depth(nn3->text,
				                                      resolved_struct_name, extra_stars3);
				parser_clear_decl_align_request();
				nnode3 = new_struct_decl(nn3->text, noff3);
				nnode3->type = decl_type3;
			} else {
				decl_type3 = def->is_union ? type_union(resolved_struct_name, def->size)
				                            : type_struct(resolved_struct_name, def->size);
				parser_set_decl_align_request(requested_align);
				noff3 = add_struct_local(nn3->text, resolved_struct_name);
				parser_clear_decl_align_request();
				nnode3 = new_struct_decl(nn3->text, noff3);
				nnode3->type = decl_type3;
			}
			extra_head = append_node(extra_head, nnode3);
		}
		expect(TOK_SEMI);
		return new_block(extra_head);
	}
	expect(TOK_SEMI);
	return node;
}

static Node *
parse_typedef_or_enum_declaration_statement(int requested_align)
{
	Type *base_type = parse_type_name();
	stmt_reject_unsupported_special_type(base_type);
	int saw_trailing_function_specifier =
	    parser_type_name_saw_trailing_function_specifier();
	TokenKind trailing_storage_class =
	    parser_type_name_trailing_storage_class();
	int saw_thread_local =
	    parser_type_name_saw_thread_local_storage_specifier();
	if (parser_type_name_saw_multiple_trailing_storage_classes())
		fatal_cur("multiple storage classes in declaration\n");

	if (token_starts_alignas_specifier(lexer_peek(), lexer_peek_ahead(1))) {
		if (trailing_storage_class == TOK_TYPEDEF) {
			parse_alignment_specifiers();
			fatal_cur("alignment specifier cannot be applied to a typedef declaration\n");
		}
		requested_align = stmt_merge_requested_alignment(requested_align);
	}

	if (saw_thread_local) {
		if (trailing_storage_class == TOK_THREAD_LOCAL &&
		    lexer_peek()->kind != TOK_IDENT &&
		    lexer_peek()->kind != TOK_STAR &&
		    lexer_peek()->kind != TOK_LPAREN)
			fatal_cur("'thread_local' is a reserved identifier in C23 and cannot be used as a local variable name\n");
		if (trailing_storage_class == TOK_STATIC ||
		    trailing_storage_class == TOK_EXTERN)
			fatal_cur("thread-local storage is not supported\n");
		if (trailing_storage_class == TOK_TYPEDEF)
			fatal_cur("multiple storage classes in declaration\n");
		fatal_cur("_Thread_local variables must have global storage\n");
	}

	if (trailing_storage_class == TOK_REGISTER)
		stmt_decl_register_request = 1;
	else if (trailing_storage_class == TOK_AUTO)
		;
	else if (trailing_storage_class == TOK_EXTERN)
		return parse_extern_declaration_after_base_type(base_type);
	else if (trailing_storage_class == TOK_STATIC)
		return parse_static_declaration_after_base_type(base_type,
		                                               type_sizeof(base_type) > 0
		                                                   ? type_sizeof(base_type)
		                                                   : 4,
		                                               requested_align,
		                                               saw_trailing_function_specifier,
		                                               parser_type_name_saw_trailing_noreturn_specifier());
	else if (trailing_storage_class == TOK_STATIC ||
	         trailing_storage_class == TOK_EXTERN ||
	         trailing_storage_class == TOK_AUTO)
		fatal_cur("multiple storage classes in declaration\n");
	if (trailing_storage_class == TOK_TYPEDEF) {
		if (saw_trailing_function_specifier)
			fatal_cur("function specifier is only valid on function declarations\n");
		parse_typedef_declaration_after_base_type(base_type);
		return new_block(NULL);
	}

	if (lexer_peek()->kind == TOK_SEMI) {
		if (saw_trailing_function_specifier)
			fatal_cur("function specifier is only valid on function declarations\n");
		if (trailing_storage_class == TOK_TYPEDEF)
			fatal_cur("Expected identifier after declaration type\n");
		lexer_next();
		return new_block(NULL);
	}

	{
		Node *fp_decl = stmt_try_parse_function_pointer_local_after_decl_type(base_type,
		                                                                     requested_align);
		if (fp_decl)
			return fp_decl;
	}

	const Token *name = lexer_peek();
	if (name->kind != TOK_IDENT) {
		fatal_cur("Expected identifier after declaration type\n");
	}

	char decl_var_name[64] = {0};
	STRNCPY(decl_var_name, name->text ? name->text : "", sizeof(decl_var_name) - 1);
	lexer_next();
	if (saw_trailing_function_specifier && lexer_peek()->kind != TOK_LPAREN)
		fatal_cur("function specifier is only valid on function declarations\n");
	{
		Node *func_decl =
		    stmt_parse_function_declaration_after_name(
		        decl_var_name, base_type,
		        parser_type_name_saw_trailing_noreturn_specifier());
		if (func_decl)
			return func_decl;
	}

	if (type_is_array(base_type)) {
		if (base_type->is_vm_type && base_type->vla_bound_name[0]) {
			Node *head = build_runtime_vm_typedef_array_decl_stmt_list(requested_align,
			                                                           decl_var_name,
			                                                           base_type);
			Node *tail = stmt_node_list_tail(head);

			if (lexer_peek()->kind == TOK_ASSIGN)
				fatal_cur("Unsupported runtime VLA initializer\n");

			while (lexer_peek()->kind == TOK_COMMA) {
				const Token *next_name;
				Node *extra;

				lexer_next();
				next_name = lexer_peek();
				if (next_name->kind != TOK_IDENT)
					fatal_cur("Expected identifier in declaration list\n");
				lexer_next();
				if (lexer_peek()->kind == TOK_LPAREN) {
					if (!stmt_try_parse_function_declaration_after_name(
					        next_name->text, base_type,
					        parser_type_name_saw_trailing_noreturn_specifier()))
						fatal_cur("internal error: expected block-scope function declarator\n");
					continue;
				}
				if (lexer_peek()->kind == TOK_ASSIGN)
					fatal_cur("Unsupported runtime VLA initializer\n");
				extra = build_runtime_vm_typedef_array_decl_stmt_list(requested_align,
				                                                      next_name->text,
				                                                      base_type);
				tail = append_node(tail, extra);
			}

			stmt_expect_decl_semi();
			return new_block(head);
		}

		int offset = add_decl_typed_local(requested_align, decl_var_name, base_type);
		Node *node = make_decl_node_for_type(decl_var_name, offset, base_type);
		int elem_size = stmt_local_array_leaf_elem_size(base_type);

		if (lexer_peek()->kind == TOK_ASSIGN) {
			Node *aggregate_init =
			    stmt_try_parse_named_aggregate_array_initializer(decl_var_name, offset,
			                                                    base_type, node);
			if (aggregate_init)
				return aggregate_init;
			return build_local_scalar_array_initializer_stmt(decl_var_name, offset,
			                                                 base_type, elem_size, node,
			                                                 base_type,
			                                                 requested_align);
		}

		if (lexer_peek()->kind == TOK_COMMA) {
			Node *block_head = stmt_append_comma_typed_declarators(node, base_type,
			                                                       requested_align,
			                                                       STMT_COMMA_BASE_STRIP_ALL_PTRS,
			                                                       0, 0, 0);
			stmt_expect_decl_semi();
			return new_block(block_head);
		}

		stmt_expect_decl_semi();
		return node;
	}

	if (lexer_peek()->kind == TOK_LBRACKET) {
		if (array_decl_looks_runtime_vla()) {
			stmt_require_c99_for_runtime_vla();
			if (type_sizeof(base_type) <= 0)
				fatal_cur("runtime VLA element type must be complete\n");
			return build_runtime_vla_decl_block(requested_align, decl_var_name,
			                                    base_type, type_elem_size(base_type));
		}

		int dims[MAX_ARRAY_DIMS] = {0};
		int dim_count = parse_array_dimensions(dims, 1, 0);
		int array_len = dims[0];
		int *init_values = NULL;
		int init_values_cap = 0;
		int init_count = 0;
		Node **init_exprs = NULL;
		int init_exprs_cap = 0;
		unsigned char *init_seen = NULL;
		int string_init = 0;
		int zero_fill_only = 0;
		int empty_braced_init = 0;
		int aggregate_init_pending = 0;
		char *string_value = NULL;
		size_t string_len = 0;
		Type *elem_type = parser_canonicalize_decl_type(base_type);
		int pointer_elements = elem_type && type_is_pointer(elem_type);
		int next_init_index = 0;
		int max_init_index = -1;

		int string_width = 1;  /* element width of the string literal */
		if (lexer_peek()->kind == TOK_ASSIGN) {
			if (dim_count == 1 && stmt_type_is_named_aggregate(elem_type)) {
				aggregate_init_pending = 1;
			} else {
				lexer_next();

				if (lexer_peek()->kind == TOK_STRING) {
					const Token *value = lexer_peek();
					stmt_require_string_literal_array_match(value,
					                                        stmt_local_array_leaf_elem_size(base_type),
					                                        "String literal element width does not match local array element type");
					string_init = 1;
					string_width = stmt_string_literal_elem_width(value);
					string_len = value->text_len;
					string_value = xmalloc(string_len + 1);
				memcpy(string_value, value->text, string_len);
				string_value[string_len] = '\0';
				init_count = string_width > 1
					? (int)(string_len / (size_t)string_width) + 1
					: (int)string_len + 1;
				lexer_next();
			} else if (lexer_peek()->kind == TOK_LBRACE) {
				if (dim_count > 1) {
					Type *init_array_type = build_array_type_from_dims(base_type, dims, dim_count);
					stmt_parse_local_multidim_array_initializer(init_array_type,
					                                            pointer_elements,
					                                            1,
					                                            0,
					                                            &init_values_cap,
					                                            &init_values,
					                                            &init_exprs_cap,
					                                            &init_exprs,
					                                            &init_seen,
					                                            &max_init_index,
					                                            &init_count);
				} else {
					lexer_next();
					empty_braced_init = (lexer_peek()->kind == TOK_RBRACE);
					stmt_parse_braced_local_array_initializer(elem_type, pointer_elements,
						array_len, 1, 0,
						&next_init_index, &max_init_index, &init_count,
						&init_values_cap, &init_values, &init_exprs_cap, &init_exprs,
						&init_seen);
				}
			} else if (consume_all_zero_initializer()) {
				zero_fill_only = 1;
				init_count = array_len;
			} else {
				fatal_cur("Unsupported typedef-based local array initializer\n");
			}
			}
		}

		if (array_len == 0)
			array_len = init_count;

		if (array_len <= 0) {
			fatal_cur("Array length must be known for typedef-based local array\n");
		}

		if (empty_braced_init) {
			zero_fill_only = 1;
			init_count = 1;
		}

		if (dims[0] == 0)
			dims[0] = array_len;

		Type *array_type = build_array_type_from_dims(base_type, dims, dim_count);
		int total_len = array_type->size / stmt_local_array_leaf_elem_size(base_type);
		int offset = add_decl_typed_local(requested_align, decl_var_name, array_type);
		Node *node = new_array_decl(decl_var_name, offset, array_len);
		node->elem_size = stmt_local_array_leaf_elem_size(base_type);
		node->type = array_type;

		if (aggregate_init_pending)
			return parse_struct_array_initializer_block(decl_var_name,
			                                           elem_type->struct_name,
			                                           offset, array_len, node);

		if (lexer_peek()->kind == TOK_COMMA) {
			Node *extra_head = node;
			Type *elem_base = (base_type->kind == TY_PTR && base_type->base)
			                   ? base_type->base : base_type;
			while (lexer_peek()->kind == TOK_COMMA) {
				int extra_stars2 = 0;
				const Token *nn2;
				Type *decl_type2;
				int noff2;
				Node *nnode2;
				lexer_next();
				while (lexer_peek()->kind == TOK_STAR) { lexer_next(); extra_stars2++; }
				nn2 = lexer_peek();
				if (nn2->kind != TOK_IDENT)
					break;
				lexer_next();
				decl_type2 = clone_type(elem_base);
				for (int ei2 = 0; ei2 < extra_stars2; ei2++)
					decl_type2 = type_ptr(decl_type2);
				if (lexer_peek()->kind == TOK_LPAREN) {
					if (!stmt_try_parse_function_declaration_after_name(
					        nn2->text, decl_type2,
					        parser_type_name_saw_trailing_noreturn_specifier()))
						fatal_cur("internal error: expected block-scope function declarator\n");
					continue;
				}
				if (lexer_peek()->kind == TOK_LBRACKET) {
					int dims2[MAX_ARRAY_DIMS] = {0};
					int dc2 = parse_array_dimensions(dims2, 1, 0);
					decl_type2 = build_array_type_from_dims(decl_type2, dims2, dc2);
					noff2 = add_decl_typed_local(requested_align, nn2->text, decl_type2);
					nnode2 = new_array_decl(nn2->text, noff2, dims2[0]);
					nnode2->elem_size = type_elem_size(elem_base);
					nnode2->type = decl_type2;
				} else {
					const char *sn2 = NULL;
					noff2 = add_decl_typed_local(requested_align, nn2->text, decl_type2);
					nnode2 = new_struct_decl(nn2->text, noff2);
					nnode2->type = decl_type2;
					if (elem_base->struct_name[0])
						sn2 = elem_base->struct_name;
					if (sn2)
						STRNCPY(nnode2->struct_name, sn2,
						        sizeof(nnode2->struct_name) - 1);
				}
				extra_head = append_node(extra_head, nnode2);
			}
			stmt_expect_decl_semi();
			return new_block(extra_head);
		}

		stmt_expect_decl_semi();

		if (zero_fill_only)
			return build_local_zero_fill_block(decl_var_name, offset, array_type->size, node);

		if (string_init) {
			Node head = {0};
			Node *cur = &head;
			int elem_size = stmt_local_array_leaf_elem_size(base_type);

			append_stmt(&cur, node);
			for (int i = 0; i < init_count; i++) {
				int val = 0;
				if (string_width > 1) {
					for (int bi = 0; bi < string_width; bi++) {
						size_t byte_idx = (size_t)i * (size_t)string_width + (size_t)bi;
						if (byte_idx < string_len)
							val |= (unsigned char)string_value[byte_idx] << (8 * bi);
					}
				} else {
					val = (unsigned char)string_value[i];
				}
				append_stmt(&cur,
				            make_local_array_store(decl_var_name, offset, elem_size, i, val));
			}
			for (int i = init_count; i < total_len; i++) {
				append_stmt(&cur,
				            make_local_array_store(decl_var_name, offset, elem_size, i, 0));
			}
			xfree(string_value);
			return new_block(head.next);
		}

		if (init_count > 0) {
			Node head = {0};
			Node *cur = &head;
			int elem_size = stmt_local_array_leaf_elem_size(base_type);

			append_stmt(&cur, node);
			if (pointer_elements) {
				for (int i = 0; i < total_len; i++) {
					if (i < init_exprs_cap && init_seen && init_seen[i])
						append_stmt(&cur, make_local_array_assign_expr(decl_var_name, offset, elem_type, i, init_exprs[i]));
					else
						append_stmt(&cur, make_local_array_assign_expr(decl_var_name, offset, elem_type, i, new_num(0)));
				}
			} else {
				for (int i = 0; i < total_len; i++) {
					if (i < init_values_cap && init_seen && init_seen[i]) {
						append_stmt(&cur,
						            make_local_array_store(decl_var_name, offset, elem_size, i, init_values[i]));
					} else {
						append_stmt(&cur,
						            make_local_array_store(decl_var_name, offset, elem_size, i, 0));
					}
				}
			}
			xfree(init_values);
			xfree(init_exprs);
			xfree(init_seen);
			return new_block(head.next);
		}

		xfree(init_values);
		xfree(init_exprs);
		xfree(init_seen);

		return node;
	}

	int offset = add_decl_typed_local(requested_align, decl_var_name, base_type);
	Node *node;

	if (base_type->kind == TY_PTR) {
		node = new_ptr_decl(decl_var_name, offset);
		node->is_pointer = 1;
		node->elem_size = type_elem_size(base_type);
		if (base_type->base && type_is_struct(base_type->base))
			STRNCPY(node->struct_name, base_type->base->struct_name, sizeof(node->struct_name) - 1);
	} else if (type_is_struct(base_type)) {
		node = new_struct_decl(decl_var_name, offset);
	} else {
		node = new_decl(decl_var_name, offset);
		node->elem_size = type_elem_size(base_type);
	}

	node->type = clone_type(base_type);

	if (type_is_struct(base_type))
		return parse_struct_initializer_block(decl_var_name, base_type->struct_name, offset, node);

	if (lexer_peek()->kind == TOK_ASSIGN) {
		return stmt_finish_typed_decl_statement(node, decl_var_name, offset, base_type,
		                                        requested_align,
		                                        STMT_COMMA_BASE_ONE_PTR_IF_PTR,
		                                        1, 1, 1);
	}

	if (lexer_peek()->kind == TOK_COMMA) {
		Node *block_head = stmt_append_comma_typed_declarators(node, base_type,
		                                                       requested_align,
		                                                       STMT_COMMA_BASE_STRIP_ALL_PTRS,
		                                                       0, 0, 0);
		stmt_expect_decl_semi();
		return new_block(block_head);
	}

	stmt_expect_decl_semi();
	return node;
}

static Node *
parse_statement_inner(void)
{
	const Token *token = lexer_peek();
	int requested_align = 0;

	if (parser_try_consume_pragma_pack())
		return new_block(NULL);

	stmt_decl_register_request = 0;

	/* Skip automatic-storage specifiers. `register` is still semantic:
	 * unary & cannot be applied to such an object. */
	while (token->kind == TOK_AUTO || token->kind == TOK_REGISTER ||
	       (token->kind == TOK_IDENT && token->text && STRCMP(token->text, "register") == 0)) {
		if (token->kind == TOK_REGISTER ||
		    (token->kind == TOK_IDENT && token->text && STRCMP(token->text, "register") == 0))
			stmt_decl_register_request = 1;
		lexer_next();
		token = lexer_peek();
	}

	if (token_starts_plain_thread_local_storage_specifier(token,
	                                                     lexer_peek_ahead(1),
	                                                     lexer_peek_ahead(2)))
		reject_plain_thread_local_keyword_before_c23(token);

	if (token->kind == TOK_THREAD_LOCAL) {
		reject_thread_local_storage_specifier();
		if (lexer_peek_ahead(1)->kind == TOK_STATIC ||
		    lexer_peek_ahead(1)->kind == TOK_EXTERN)
			fatal_cur("thread-local storage is not supported\n");
		fatal_cur("_Thread_local variables must have global storage\n");
	}

	/* v175 empty statement */
	if (token->kind == TOK_SEMI) {
		lexer_next();
		return new_block(NULL);
	}

	if (token->kind == TOK_IDENT && lexer_peek_ahead(1)->kind == TOK_COLON) {
		char label_name[64] = {0};
		STRNCPY(label_name, token->text ? token->text : "", sizeof(label_name) - 1);
		stmt_register_label_site(label_name, parser_current_local_count());
		lexer_next(); /* label */
		lexer_next(); /* ':' */
		reject_declaration_after_label_before_c23();
		return new_label_stmt(label_name);
	}

	if (token->kind == TOK_GOTO) {
		Node *jump;
		char label_name[64] = {0};

		lexer_next();

		const Token *label = lexer_peek();
		if (label->kind != TOK_IDENT) {
			fatal_cur("Expected label after goto\n");
		}
		STRNCPY(label_name, label->text ? label->text : "", sizeof(label_name) - 1);

		lexer_next();
		expect(TOK_SEMI);
		jump = new_goto_stmt(label_name);
		stmt_register_goto_site(jump, label_name, parser_current_local_count());
		return jump;
	}

	if (token->kind == TOK_ASM)
		return parse_asm_statement();

	if (token_starts_alignas_specifier(token, lexer_peek_ahead(1))) {
		requested_align = parse_alignment_specifiers();
		token = lexer_peek();
	}

	if (parse_static_assert_declaration())
		return new_block(NULL);

	stmt_last_typedef_decl_node = NULL;
	if (parse_typedef_declaration())
		return stmt_last_typedef_decl_node ? stmt_last_typedef_decl_node : new_block(NULL);

	if (token->kind != TOK_EXTERN &&
	    token->kind != TOK_STATIC &&
	    parser_current_function_name()[0] == '\0' &&
	    try_parse_prototype())
		return new_block(NULL);

	if (token->kind == TOK_INLINE || token->kind == TOK_NORETURN)
		return parse_function_specifier_declaration_statement();

	Node *compound_assign = parse_struct_compound_assignment_statement();
	if (compound_assign)
		return compound_assign;

	Node *struct_ret_assign = parse_struct_return_assignment_statement();
	if (struct_ret_assign)
		return struct_ret_assign;

	Node *struct_ret_discard = parse_struct_return_discard_statement();
	if (struct_ret_discard)
		return struct_ret_discard;

	if (token->kind == TOK_ENUM ||
	    (token->kind == TOK_IDENT && parser_is_typedef_name(token->text)))
		return parse_typedef_or_enum_declaration_statement(requested_align);

	if (token->kind == TOK_CONST || token->kind == TOK_VOLATILE ||
	    token->kind == TOK_RESTRICT || token->kind == TOK_ATOMIC ||
	    token->kind == TOK_VOID || token->kind == TOK_FLOAT ||
	    token->kind == TOK_DOUBLE ||
	    (token->kind == TOK_IDENT && token->text && tcc_lang_at_least(LANG_C99) &&
	     (STRCMP(token->text, "_Complex") == 0 ||
	      STRCMP(token->text, "_Imaginary") == 0)) ||
	    token_is_typeof_keyword(token) ||
	    token_starts_plain_bool_type_specifier(token,
	                                          lexer_peek_ahead(1),
	                                          lexer_peek_ahead(2)) ||
	    (tcc_lang_at_least(LANG_C23) &&
	     (token_is_c23_bool_keyword(token) ||
	      token_is_c23_nullptr_t_keyword(token)))) {
		Type *decl_type = parse_type_name();
		stmt_reject_unsupported_special_type(decl_type);
		int saw_trailing_function_specifier =
		    parser_type_name_saw_trailing_function_specifier();
		TokenKind trailing_storage_class =
		    parser_type_name_trailing_storage_class();
		int saw_thread_local =
		    parser_type_name_saw_thread_local_storage_specifier();
		if (parser_type_name_saw_multiple_trailing_storage_classes())
			fatal_cur("multiple storage classes in declaration\n");

		if (trailing_storage_class == TOK_TYPEDEF) {
			if (saw_thread_local)
				fatal_cur("multiple storage classes in declaration\n");
			if (saw_trailing_function_specifier)
				fatal_cur("function specifier is only valid on function declarations\n");
			parse_typedef_declaration_after_base_type(decl_type);
			return new_block(NULL);
		}
		if (saw_thread_local) {
			if (trailing_storage_class == TOK_THREAD_LOCAL &&
			    lexer_peek()->kind != TOK_IDENT &&
			    lexer_peek()->kind != TOK_STAR &&
			    lexer_peek()->kind != TOK_LPAREN)
				fatal_cur("'thread_local' is a reserved identifier in C23 and cannot be used as a local variable name\n");
			if (trailing_storage_class == TOK_STATIC ||
			    trailing_storage_class == TOK_EXTERN)
				fatal_cur("thread-local storage is not supported\n");
			fatal_cur("_Thread_local variables must have global storage\n");
		}
		if (trailing_storage_class == TOK_REGISTER)
			stmt_decl_register_request = 1;
		else if (trailing_storage_class == TOK_AUTO)
			;
		else if (trailing_storage_class == TOK_EXTERN)
			return parse_extern_declaration_after_base_type(decl_type);
		else if (trailing_storage_class == TOK_STATIC)
			return parse_static_declaration_after_base_type(
			    decl_type,
			    type_sizeof(decl_type) > 0 ? type_sizeof(decl_type) : 4,
			    requested_align,
			    saw_trailing_function_specifier,
			    parser_type_name_saw_trailing_noreturn_specifier());

		{
			Node *fp_decl = stmt_try_parse_function_pointer_local_after_decl_type(decl_type,
			                                                                     requested_align);
			if (fp_decl)
				return fp_decl;
		}

		const Token *name = lexer_peek();
		if (name->kind != TOK_IDENT) {
			fatal_cur("Expected identifier after declaration type\n");
		}

		lexer_next();
		if (saw_trailing_function_specifier && lexer_peek()->kind != TOK_LPAREN)
			fatal_cur("function specifier is only valid on function declarations\n");

		{
			Node *func_decl =
			    stmt_parse_function_declaration_after_name(
			        name->text, decl_type,
			        saw_trailing_function_specifier ||
			            parser_type_name_saw_trailing_noreturn_specifier());
			if (func_decl)
				return func_decl;
		}

		/* Handle array declarator: const char *azOpt[] = {...}; */
		if (lexer_peek()->kind == TOK_LBRACKET) {
			int arr_dims[MAX_ARRAY_DIMS] = {0};
			int arr_dim_count;
			Type *arr_type;
			int arr_offset;
			Node *arr_node;
			arr_dim_count = parse_array_dimensions(arr_dims, 1, 0);
			if (lexer_peek()->kind == TOK_ASSIGN && !type_is_pointer(decl_type) &&
			    arr_dims[0] == 0) {
				int init_values_cap = 0;
				int init_count = 0;
				int *init_values = NULL;
				Node **init_exprs = NULL;
				int init_exprs_cap = 0;
				unsigned char *init_seen = NULL;
				int next_init_index = 0;
				int max_init_index = -1;
				Node head = {0};
				Node *cur = &head;

				lexer_next();
				if (lexer_peek()->kind != TOK_LBRACE)
					fatal_cur("Unsupported local array initializer\n");
				lexer_next();
				stmt_parse_braced_local_array_initializer(decl_type, 0, 0, 1, 0,
					&next_init_index, &max_init_index, &init_count,
					&init_values_cap, &init_values, &init_exprs_cap, &init_exprs,
					&init_seen);
				if (init_count <= 0)
					fatal_cur("Array length must be known here\n");

				arr_dims[0] = init_count;
				arr_type = build_array_type_from_dims(clone_type(decl_type),
				                                       arr_dims, arr_dim_count);
				arr_offset = add_decl_typed_local(requested_align, name->text, arr_type);
				arr_node = new_array_decl(name->text, arr_offset, arr_dims[0]);
				arr_node->elem_size = type_elem_size(arr_type);
				arr_node->type = arr_type;
				append_stmt(&cur, arr_node);
				for (int i = 0; i < init_count; i++) {
					int value = (i < init_values_cap && init_seen && init_seen[i])
					            ? init_values[i] : 0;
					append_stmt(&cur, make_local_array_store(name->text, arr_offset,
					                                         type_sizeof(decl_type),
					                                         i, value));
				}
				head.next = stmt_append_comma_after_array_initializer(head.next,
				                                                      decl_type,
				                                                      requested_align);
				expect(TOK_SEMI);
				xfree(init_values);
				xfree(init_exprs);
				xfree(init_seen);
				return new_block(head.next);
			}
			if (lexer_peek()->kind == TOK_ASSIGN && !type_is_pointer(decl_type)) {
				Node *block;
				arr_type = build_array_type_from_dims(clone_type(decl_type),
				                                       arr_dims, arr_dim_count);
				arr_offset = add_decl_typed_local(requested_align, name->text, arr_type);
				arr_node = new_array_decl(name->text, arr_offset, arr_dims[0]);
				arr_node->elem_size = type_elem_size(arr_type);
				arr_node->type = arr_type;
				block = build_local_scalar_array_initializer_stmt(name->text, arr_offset,
				                                                  arr_type,
				                                                  type_sizeof(decl_type),
				                                                  arr_node, decl_type,
				                                                  requested_align);
				stmt_expect_decl_semi();
				return block;
			}
			if (lexer_peek()->kind == TOK_ASSIGN) {
				lexer_next();
				if (lexer_peek()->kind == TOK_LBRACE) {
					int elem_count = 0;
					int idx = 0;
					Node head2 = {0};
					Node *cur2 = &head2;
					int elem_sz = type_elem_size(decl_type);
					Node **init_exprs = NULL;
					int init_exprs_cap = 0;
					unsigned char *init_seen = NULL;
					lexer_next();
					while (lexer_peek()->kind != TOK_RBRACE) {
						Node *init_expr;
						if (lexer_peek()->kind == TOK_STRING) {
							const Token *sv = lexer_peek();
							init_expr = new_string_len_width(sv->text, sv->text_len,
							                                 parser_alloc_string_label(),
							                                 sv->string_width);
							parser_register_string_literal(init_expr->string_label,
							                              init_expr->string_value,
							                              init_expr->string_len,
							                              init_expr->string_width);
							lexer_next();
						} else {
							init_expr = parse_assignment();
						}
						stmt_local_array_init_reserve(idx + 1, &init_exprs_cap, NULL,
						                              &init_exprs, &init_seen);
						init_exprs[idx] = init_expr;
						init_seen[idx] = 1;
						idx++;
						elem_count++;
						if (lexer_peek()->kind == TOK_COMMA) lexer_next();
						else break;
					}
					expect(TOK_RBRACE);
					if (arr_dims[0] == 0) arr_dims[0] = elem_count;
					arr_type = build_array_type_from_dims(decl_type, arr_dims, arr_dim_count);
					arr_offset = add_decl_typed_local(requested_align, name->text, arr_type);
					arr_node = new_array_decl(name->text, arr_offset, arr_dims[0]);
					arr_node->elem_size = elem_sz;
					arr_node->type = arr_type;
					for (int i = 0; i < arr_dims[0]; i++) {
						Node *value = (init_seen && init_seen[i] && init_exprs[i])
						            ? init_exprs[i]
						            : new_num(0);
						append_stmt(&cur2, make_local_array_assign_expr(name->text, arr_offset,
						                                                decl_type, i, value));
					}
					xfree(init_exprs);
					xfree(init_seen);
					stmt_expect_decl_semi();
					return new_block(append_node(arr_node, head2.next));
				}
			}
			arr_type = build_array_type_from_dims(decl_type, arr_dims, arr_dim_count);
			arr_offset = add_decl_typed_local(requested_align, name->text, arr_type);
			arr_node = new_array_decl(name->text, arr_offset, arr_dims[0]);
			arr_node->type = arr_type;
			stmt_expect_decl_semi();
			return arr_node;
		}

		int offset;
		Node *node;

		if (type_is_void(decl_type))
			fatal_cur("variable cannot have type void\n");

		offset = add_decl_typed_local(requested_align, name->text, decl_type);
		parser_override_local_type(name->text, offset, decl_type, type_elem_size(decl_type));
		if (decl_type->kind == TY_PTR) {
			node = new_ptr_decl(name->text, offset);
			node->is_pointer = 1;
			if (decl_type->base && type_is_struct(decl_type->base))
				STRNCPY(node->struct_name, decl_type->base->struct_name, sizeof(node->struct_name) - 1);
		} else {
			node = new_decl(name->text, offset);
		}

		node->type = clone_type(decl_type);
		node->elem_size = type_elem_size(decl_type);

		return stmt_finish_typed_decl_statement(node, name->text, offset, decl_type,
		                                        requested_align,
		                                        STMT_COMMA_BASE_STRIP_ALL_PTRS,
		                                        0, 0, 0);
	}

	{
		Node *struct_decl = parse_struct_union_declaration_statement(token, requested_align);
		if (struct_decl)
			return struct_decl;
	}

	if (token->kind == TOK_STATIC)
		return parse_static_declaration_statement(requested_align);

	if (token->kind == TOK_EXTERN)
		return parse_extern_declaration_statement();


	{
		Node *integer_decl = parse_integer_modifier_declaration_statement(token, requested_align);
		if (integer_decl)
			return integer_decl;
	}

	{
		Node *scalar_decl = parse_scalar_declaration_statement(token, requested_align);
		if (scalar_decl)
			return scalar_decl;
	}

	return parse_non_declaration_statement(token);
}

Node *
parse_statement(void)
{
	Node *node;

	if (++stmt_parse_depth > STMT_PARSE_DEPTH_LIMIT)
		fatal_cur("Statement nesting too deep\n");
	node = parse_statement_inner();
	stmt_parse_depth--;
	return node;
}

static Node *
stmt_try_parse_function_declaration_after_name(const char *name,
                                               Type *ret_type,
                                               int is_noreturn)
{
	Node *extra_head = NULL;
	Node *extra_tail = NULL;
	Type **param_types = NULL;
	int param_count = 0;
	int is_variadic = 0;
	int fixed_params = 0;
	int has_prototype = 0;

	if (lexer_peek()->kind != TOK_LPAREN)
		return NULL;

	parse_prototype_param_list(&param_types, &param_count,
	                           &is_variadic, &fixed_params,
	                           &has_prototype, 1);
	if (lexer_peek()->kind == TOK_LBRACKET)
		fatal_cur("function return array declarators are not supported\n");
	if (lexer_peek()->kind == TOK_LPAREN)
		fatal_cur("function cannot return function type\n");
	if (lexer_peek()->kind == TOK_ASSIGN)
		fatal_cur("function declaration cannot have initializer\n");
	parser_declare_function(name, ret_type, has_prototype,
	                        param_types, param_count, is_variadic,
	                        fixed_params, is_noreturn);
	parser_note_block_scope_function_declaration(name, ret_type);

	while (lexer_peek()->kind == TOK_COMMA) {
		Type **next_param_types = NULL;
		int next_param_count = 0;
		int next_is_variadic = 0;
		int next_fixed_params = 0;
		int next_has_prototype = 0;
		int extra_stars = 0;
		const Token *next_name;
		Type *next_decl_type;

		lexer_next();
		while (lexer_peek()->kind == TOK_STAR) {
			lexer_next();
			extra_stars++;
		}

		next_name = lexer_peek();
		if (next_name->kind != TOK_IDENT)
			fatal_cur("Expected identifier in declaration list\n");
		lexer_next();

		next_decl_type = clone_type(ret_type);
		for (int i = 0; i < extra_stars; i++)
			next_decl_type = type_ptr(next_decl_type);

		if (lexer_peek()->kind == TOK_LPAREN) {
			parse_prototype_param_list(&next_param_types, &next_param_count,
			                           &next_is_variadic, &next_fixed_params,
			                           &next_has_prototype, 1);
			if (lexer_peek()->kind == TOK_LBRACKET)
				fatal_cur("function return array declarators are not supported\n");
			if (lexer_peek()->kind == TOK_LPAREN)
				fatal_cur("function cannot return function type\n");
			if (lexer_peek()->kind == TOK_ASSIGN)
				fatal_cur("function declaration cannot have initializer\n");
			parser_declare_function(next_name->text, next_decl_type,
			                        next_has_prototype, next_param_types,
			                        next_param_count, next_is_variadic,
			                        next_fixed_params, is_noreturn);
			parser_note_block_scope_function_declaration(next_name->text,
			                                             next_decl_type);
			continue;
		}

		if (lexer_peek()->kind == TOK_ASSIGN)
			fatal_cur("Expected function declarator in prototype list\n");
		if (lexer_peek()->kind == TOK_LBRACKET)
			fatal_cur("Expected function declarator in prototype list\n");

		{
			int offset = add_decl_typed_local(0, next_name->text, next_decl_type);
			Node *decl = new_decl(next_name->text, offset);

			decl->type = parser_canonicalize_decl_type(next_decl_type);
			decl->elem_size = type_elem_size(next_decl_type);
			stmt_append_node_to_tail(&extra_head, &extra_tail, decl);
		}
	}

	return new_block(extra_head);
}

static Node *
stmt_parse_function_declaration_after_name(const char *name,
                                           Type *ret_type,
                                           int is_noreturn)
{
	Node *decl =
	    stmt_try_parse_function_declaration_after_name(name, ret_type, is_noreturn);

	if (!decl)
		return NULL;
	expect(TOK_SEMI);
	return decl;
}

static Node *
parse_function_specifier_declaration_statement(void)
{
	Type *ret_type;
	const Token *name;
	int saw_leading_noreturn;

	saw_leading_noreturn = (lexer_peek()->kind == TOK_NORETURN);
	stmt_skip_type_name_noise();
	ret_type = parse_type_name();
	stmt_reject_unsupported_special_type(ret_type);
	if (type_is_complex(ret_type) &&
	    !parser_complex_function_signature_supported(ret_type))
		fatal_cur("complex function signatures are not supported yet\n");

	name = lexer_peek();
	if (name->kind != TOK_IDENT)
		fatal_cur("Expected function declaration name\n");
	lexer_next();

	if (!stmt_parse_function_declaration_after_name(
	        name->text, ret_type,
	        saw_leading_noreturn ||
	            parser_type_name_saw_trailing_noreturn_specifier()))
		fatal_cur("function specifier is only valid on function declarations\n");
	return new_block(NULL);
}

static Node *
parse_extern_declaration_statement(void)
{
	Type *base_type;

	lexer_next();

	if (lexer_peek()->kind == TOK_STATIC ||
	    lexer_peek()->kind == TOK_EXTERN ||
	    lexer_peek()->kind == TOK_AUTO ||
	    lexer_peek()->kind == TOK_REGISTER ||
	    lexer_peek()->kind == TOK_TYPEDEF)
		fatal_cur("multiple storage classes in declaration\n");
	if (lexer_peek()->kind == TOK_THREAD_LOCAL) {
		reject_thread_local_storage_specifier();
		fatal_cur("thread-local storage is not supported\n");
	}

	base_type = parse_type_name();
	stmt_reject_unsupported_special_type(base_type);

	return parse_extern_declaration_after_base_type(base_type);
}

static Node *
parse_extern_declaration_after_base_type(Type *base_type)
{
	for (;;) {
		Type *decl_type = clone_type(base_type);
		const Token *name;

		if (lexer_peek()->kind == TOK_LPAREN &&
		    lexer_peek_ahead(1)->kind == TOK_STAR) {
			char decl_name[64] = {0};
			int inner_dims[MAX_ARRAY_DIMS] = {0};
			int inner_dim_count = 0;

			lexer_next();
			lexer_next();
			skip_pointer_qualifiers();
			name = lexer_peek();
			if (name->kind != TOK_IDENT)
				fatal_cur("Expected extern declaration name\n");
			STRNCPY(decl_name, name->text ? name->text : "", sizeof(decl_name) - 1);
			lexer_next();

			if (lexer_peek()->kind == TOK_LBRACKET) {
				if (array_decl_looks_runtime_vla())
					fatal_cur("variable length array declaration cannot have 'extern' linkage\n");
				inner_dim_count = parse_array_dimensions(inner_dims, 1, 0);
			}
			decl_type = stmt_finish_parenthesized_pointer_object_type(base_type,
			                                                          inner_dims,
			                                                          inner_dim_count);
			if (stmt_type_is_variably_modified(decl_type))
				fatal_cur("variable length array declaration cannot have 'extern' linkage\n");

			if (lexer_peek()->kind == TOK_ASSIGN)
				fatal_cur("block-scope extern declaration cannot have initializer\n");

			parser_declare_extern_object(decl_name, decl_type);

			if (lexer_peek()->kind != TOK_COMMA)
				break;
			lexer_next();
			continue;
		}

		while (lexer_peek()->kind == TOK_STAR) {
			lexer_next();
			decl_type = type_ptr(decl_type);
		}
		if (lexer_peek()->kind == TOK_THREAD_LOCAL) {
			reject_thread_local_storage_specifier();
			fatal_cur("thread-local storage is not supported\n");
		}

		name = lexer_peek();
		if (name->kind != TOK_IDENT)
			fatal_cur("Expected extern declaration name\n");
		lexer_next();

		if (lexer_peek()->kind == TOK_LPAREN) {
			if (type_is_complex(decl_type) &&
			    !parser_complex_function_signature_supported(decl_type))
				fatal_cur("complex function signatures are not supported yet\n");
			Type **param_types = NULL;
			int param_count = 0;
			int is_variadic = 0;
			int fixed_params = 0;
			int has_prototype = 0;
			const Token *first_param = lexer_peek_ahead(1);

			if (first_param->kind != TOK_RPAREN &&
			    tcc_lang_is_c89_or_c90())
				fatal_cur("function prototypes are not allowed in C89/C90 mode\n");
			parse_prototype_param_list(&param_types, &param_count,
			                           &is_variadic, &fixed_params,
			                           &has_prototype, 1);
			if (lexer_peek()->kind == TOK_LBRACKET)
				fatal_cur("function return array declarators are not supported\n");
			if (lexer_peek()->kind == TOK_LPAREN)
				fatal_cur("function cannot return function type\n");
			if (lexer_peek()->kind == TOK_ASSIGN)
				fatal_cur("block-scope extern function declaration cannot have initializer\n");
			parser_declare_function(name->text, decl_type, has_prototype,
			                        param_types, param_count, is_variadic,
			                        fixed_params,
			                        parser_type_name_saw_trailing_noreturn_specifier());
			parser_note_block_scope_function_declaration(name->text, decl_type);

			if (lexer_peek()->kind != TOK_COMMA)
				break;
			lexer_next();
			continue;
		}

		if (type_is_void(decl_type))
			fatal_cur("variable cannot have type void\n");

		if (lexer_peek()->kind == TOK_LBRACKET) {
			if (array_decl_looks_runtime_vla())
				fatal_cur("variable length array declaration cannot have 'extern' linkage\n");
			int dims[MAX_ARRAY_DIMS] = {0};
			int dim_count = parse_array_dimensions(dims, 1, 0);
			decl_type = build_array_type_from_dims_allow_incomplete(decl_type,
			                                                        dims, dim_count, 1);
		}
		if (stmt_type_is_variably_modified(decl_type))
			fatal_cur("variable length array declaration cannot have 'extern' linkage\n");

		if (lexer_peek()->kind == TOK_ASSIGN)
			fatal_cur("block-scope extern declaration cannot have initializer\n");

		parser_declare_extern_object(name->text, decl_type);

		if (lexer_peek()->kind != TOK_COMMA)
			break;
		lexer_next();
	}

	expect(TOK_SEMI);
	return new_block(NULL);
}

static Type *
stmt_finish_parenthesized_pointer_object_type(Type *base_type,
	int inner_dims[MAX_ARRAY_DIMS], int inner_dim_count)
{
	int post_dims[MAX_ARRAY_DIMS] = {0};
	int post_dim_count = 0;
	Type **fp_param_types = NULL;
	int fp_param_count = 0;
	int fp_is_variadic = 0;
	int fp_fixed_params = 0;
	int fp_has_prototype = 0;
	Type *decl_type;

	expect(TOK_RPAREN);

	if (lexer_peek()->kind == TOK_LBRACKET) {
		if (array_decl_looks_runtime_vla())
			fatal_cur("variable length array declaration cannot have 'extern' linkage\n");
		post_dim_count = parse_array_dimensions(post_dims, 1, 0);
	}

	if (post_dim_count > 0)
		decl_type = type_ptr(build_array_type_from_dims_allow_incomplete(clone_type(base_type),
		                                                                post_dims,
		                                                                post_dim_count, 1));
	else if (lexer_peek()->kind == TOK_LPAREN)
		decl_type = NULL;
	else
		decl_type = type_ptr(clone_type(base_type));

	if (lexer_peek()->kind == TOK_LPAREN) {
		parse_prototype_param_list(&fp_param_types, &fp_param_count,
		                          &fp_is_variadic, &fp_fixed_params,
		                          &fp_has_prototype, 1);
		if (post_dim_count == 0 && fp_has_prototype) {
			decl_type = type_ptr(parser_make_function_type(clone_type(base_type),
			                                               fp_param_types,
			                                               fp_param_count,
			                                               fp_is_variadic,
			                                               fp_fixed_params));
		} else if (post_dim_count == 0) {
			decl_type = type_ptr(type_func(clone_type(base_type)));
		}
	}

	if (inner_dim_count > 0)
		decl_type = build_array_type_from_dims_allow_incomplete(decl_type,
		                                                       inner_dims,
		                                                       inner_dim_count, 1);

	return decl_type;
}

static Node *
parse_static_declaration_statement(int requested_align)
{
	const Token *type_tok;
	int elem_size = TCC_SIZEOF_INT;
	Type *base_type = type_int();
	int saw_function_specifier = 0;
	int saw_noreturn_specifier = 0;

	if (stmt_decl_register_request)
		fatal_cur("multiple storage classes in declaration\n");

	lexer_next();

	if (lexer_peek()->kind == TOK_EXTERN ||
	    lexer_peek()->kind == TOK_STATIC ||
	    lexer_peek()->kind == TOK_AUTO ||
	    lexer_peek()->kind == TOK_REGISTER ||
	    lexer_peek()->kind == TOK_TYPEDEF)
		fatal_cur("multiple storage classes in declaration\n");
	if (lexer_peek()->kind == TOK_THREAD_LOCAL) {
		reject_thread_local_storage_specifier();
		fatal_cur("thread-local storage is not supported\n");
	}

	if (lexer_peek()->kind == TOK_CONST || lexer_peek()->kind == TOK_VOLATILE ||
	    lexer_peek()->kind == TOK_RESTRICT || lexer_peek()->kind == TOK_ATOMIC) {
		reject_c89_c99_keyword_token(lexer_peek()->kind);
		lexer_next();
	}

	while (lexer_peek()->kind == TOK_INLINE || lexer_peek()->kind == TOK_NORETURN) {
		if (lexer_peek()->kind == TOK_INLINE && tcc_lang_is_c89_or_c90())
			fatal_cur("inline is not allowed in C89/C90 mode\n");
		reject_c89_c99_keyword_token(lexer_peek()->kind);
		saw_function_specifier = 1;
		if (lexer_peek()->kind == TOK_NORETURN)
			saw_noreturn_specifier = 1;
		lexer_next();
	}

	type_tok = lexer_peek();
	if (type_tok->kind == TOK_INT) {
		elem_size = TCC_SIZEOF_INT;
		base_type = type_int();
		lexer_next();
	} else if (type_tok->kind == TOK_CHAR) {
		elem_size = 1;
		base_type = type_char();
		lexer_next();
	} else if (type_tok->kind == TOK_UNSIGNED || type_tok->kind == TOK_SIGNED ||
	           type_tok->kind == TOK_SHORT || type_tok->kind == TOK_LONG) {
		int sl = 0, ss = 0, su = 0;
		while (lexer_peek()->kind == TOK_UNSIGNED || lexer_peek()->kind == TOK_SIGNED ||
		       lexer_peek()->kind == TOK_SHORT || lexer_peek()->kind == TOK_LONG) {
			if (tcc_lang_is_c89_or_c90() &&
			    lexer_peek()->kind == TOK_LONG &&
			    lexer_peek_ahead(1)->kind == TOK_LONG)
				fatal_cur("long long is not allowed in C89/C90 mode\n");
			if (lexer_peek()->kind == TOK_LONG) sl = 1;
			if (lexer_peek()->kind == TOK_SHORT) ss = 1;
			if (lexer_peek()->kind == TOK_UNSIGNED) su = 1;
			lexer_next();
		}
		if (lexer_peek()->kind == TOK_INT || lexer_peek()->kind == TOK_CHAR)
			lexer_next();
		elem_size = sl ? 8 : (ss ? 2 : 4);
		base_type = type_for_size_unsigned(elem_size, su);
	} else if (type_tok->kind == TOK_STRUCT || type_tok->kind == TOK_UNION) {
		char sname[64] = {0};
		char sg_varname[64] = {0};
		char sgname[64] = {0};
		int sg_is_arr = 0;
		int sg_arr_len = 0;
		int depth3 = 0;
		int ecount3 = 0;
		StructDef *sdef = NULL;
		Global *sg = NULL;
		const Token *stag;
		const Token *svar;

		lexer_next();
		stag = lexer_peek();
		if (stag->kind == TOK_IDENT) {
			STRNCPY(sname, stag->text, sizeof(sname) - 1);
			lexer_next();
		}
		svar = lexer_peek();
		if (svar->kind == TOK_LBRACE) {
			if (sname[0]) {
				StructDef *newsdef = get_or_add_forward_struct(sname);
				memset(newsdef, 0, sizeof(*newsdef));
				STRNCPY(newsdef->name, sname, sizeof(newsdef->name) - 1);
				parse_struct_body_into(newsdef);
			} else {
				StructDef *anondef;
				snprintf(sname, sizeof(sname), "__anon_static_%d",
				         ++parser_anon_struct_id);
				anondef = get_or_add_forward_struct(sname);
				memset(anondef, 0, sizeof(*anondef));
				STRNCPY(anondef->name, sname, sizeof(anondef->name) - 1);
				parse_struct_body_into(anondef);
			}
			svar = lexer_peek();
		}
		if (svar->kind != TOK_IDENT) {
			fatal_cur("Expected identifier after static struct\n");
		}
		STRNCPY(sg_varname, svar->text ? svar->text : "", sizeof(sg_varname) - 1);
		lexer_next();
		snprintf(sgname, sizeof(sgname), "__static_%.24s_%.24s",
		         parser_current_function_name(), sg_varname);
		sg = new_global_slot(sgname);
		sg->is_struct = 1;
		parser_validate_decl_alignment(requested_align, type_struct(sname, 0));
		sg->align = requested_align;
		STRNCPY(sg->struct_name, sname, sizeof(sg->struct_name) - 1);
		sg->array_len = 1;
		sdef = find_struct_or_null(sname);
		if (lexer_peek()->kind == TOK_LBRACKET) {
			lexer_next();
			sg_is_arr = 1;
			if (lexer_peek()->kind == TOK_NUM) {
				sg_arr_len = (int)lexer_peek()->value;
				lexer_next();
			}
			expect(TOK_RBRACKET);
			sg->is_array = 1;
		}
		if (sdef) {
			sg->elem_size = sdef->size;
			global_set_init_count(sg, sdef->size);
		}
		if (sg_is_arr && lexer_peek()->kind == TOK_ASSIGN) {
			lexer_next();
			if (lexer_peek()->kind == TOK_LBRACE && sdef) {
				int sg_idx = parser_global_index(sg);
				int elem_count = 0;

				expect(TOK_LBRACE);
				globals_ensure_spare(256);
				sg = parser_global_at(sg_idx);
				while (lexer_peek()->kind != TOK_RBRACE) {
					int base_offset = elem_count * sdef->size;

					if (try_parse_global_struct_compound_initializer(sg_idx, sdef,
					                                               sname, base_offset)) {
						sg = parser_global_at(sg_idx);
					} else if (lexer_peek()->kind == TOK_LBRACE) {
						lexer_next();
						parse_global_struct_initializer_body(sg_idx, sdef, base_offset);
						sg = parser_global_at(sg_idx);
						expect(TOK_RBRACE);
					} else {
						parse_global_struct_initializer_body_ex(sg_idx, sdef, base_offset, 1);
						sg = parser_global_at(sg_idx);
					}

					elem_count++;
					if (lexer_peek()->kind == TOK_COMMA) {
						lexer_next();
						if (lexer_peek()->kind == TOK_RBRACE)
							break;
					} else if (lexer_peek()->kind == TOK_RBRACE) {
						break;
					}
				}

				expect(TOK_RBRACE);
				if (sg_arr_len == 0)
					sg_arr_len = elem_count > 0 ? elem_count : 1;
				sg = parser_global_at(sg_idx);
				sg->array_len = sg_arr_len;
				sg->is_array = 1;
				sg->elem_size = sdef->size;
				global_set_init_count(sg, sg_arr_len * sdef->size);
				apply_type_to_global(sg,
				                     type_array(sdef->is_union
				                                    ? type_union(sname, sdef->size)
				                                    : type_struct(sname, sdef->size),
				                                sg_arr_len));
			} else {
				depth3 = 1;
				ecount3 = 0;
				expect(TOK_LBRACE);
				while (depth3 > 0 && lexer_peek()->kind != TOK_EOF) {
					if (lexer_peek()->kind == TOK_LBRACE) {
						depth3++;
						if (depth3 == 2)
							ecount3++;
					} else if (lexer_peek()->kind == TOK_RBRACE) {
						depth3--;
					}
					if (depth3 > 0)
						lexer_next();
				}
				if (lexer_peek()->kind == TOK_RBRACE)
					lexer_next();
				if (sg_arr_len == 0)
					sg_arr_len = ecount3 > 0 ? ecount3 : 1;
				sg->array_len = sg_arr_len;
				if (sdef)
					global_set_init_count(sg, sg_arr_len * sdef->size);
			}
			expect(TOK_SEMI);
			parser_commit_reserved_global();
			if (sdef) {
				Type *arr_type = type_array(sdef->is_union
				                            ? type_union(sname, sdef->size)
				                            : type_struct(sname, sdef->size),
				                        sg_arr_len);
				add_static_local(sg_varname, sgname, arr_type, sdef->size, 1, sg_arr_len, requested_align);
			}
			return new_block(NULL);
		}
		if (!sg_is_arr && lexer_peek()->kind == TOK_ASSIGN) {
			lexer_next();
			if (lexer_peek()->kind == TOK_LBRACE && sdef) {
				int sg_idx = parser_global_index(sg);
				lexer_next();
				globals_ensure_spare(256);
				sg = parser_global_at(sg_idx);
				parse_global_struct_initializer_body(sg_idx, sdef, 0);
				sg = parser_global_at(sg_idx);
				expect(TOK_RBRACE);
			} else if (lexer_peek()->kind == TOK_LBRACE) {
				int bd = 1;
				lexer_next();
				while (bd > 0 && lexer_peek()->kind != TOK_EOF) {
					if (lexer_peek()->kind == TOK_LBRACE) bd++;
					else if (lexer_peek()->kind == TOK_RBRACE) bd--;
					lexer_next();
				}
			}
		}
		expect(TOK_SEMI);
		parser_commit_reserved_global();
		{
			Type *st = sdef ? (sdef->is_union ? type_union(sname, sdef->size)
			                                  : type_struct(sname, sdef->size))
			                : type_for_size(4);
			add_static_local(sg_varname, sgname, st, sdef ? sdef->size : 4, 0, 1, requested_align);
		}
		return new_block(NULL);
	} else if (parser_is_typedef_name(type_tok->text)) {
		Type *td = parser_find_typedef(type_tok->text);
		base_type = td ? clone_type(td) : type_for_size(4);
		elem_size = base_type ? (type_sizeof(base_type) ? type_sizeof(base_type) : 4) : 4;
		lexer_next();
	} else {
		fatal_cur("Only int/char static locals supported for now\n");
	}

	return parse_static_declaration_after_base_type(base_type, elem_size,
	                                               requested_align,
	                                               saw_function_specifier,
	                                               saw_noreturn_specifier);
}

static Node *
parse_static_declaration_after_base_type(Type *base_type, int elem_size,
	int requested_align, int saw_function_specifier,
	int saw_noreturn_specifier)
{
	int is_ptr_type = 0;
	Type *decl_type;

	while (lexer_peek()->kind == TOK_STAR) {
		lexer_next();
		is_ptr_type = 1;
		elem_size = TCC_SIZEOF_PTR;
		while (lexer_peek()->kind == TOK_CONST || lexer_peek()->kind == TOK_VOLATILE ||
		       lexer_peek()->kind == TOK_RESTRICT || lexer_peek()->kind == TOK_ATOMIC)
			lexer_next();
	}

	decl_type = is_ptr_type ? type_ptr(clone_type(base_type)) : clone_type(base_type);
	if (lexer_peek()->kind == TOK_THREAD_LOCAL) {
		reject_thread_local_storage_specifier();
		fatal_cur("thread-local storage is not supported\n");
	}

	if (!is_ptr_type &&
	    lexer_peek()->kind == TOK_IDENT &&
	    lexer_peek_ahead(1)->kind == TOK_LPAREN)
		stmt_reject_block_scope_static_function_declaration();

	if (saw_function_specifier || saw_noreturn_specifier)
		fatal_cur("function specifier is only valid on function declarations\n");

	if (lexer_peek()->kind == TOK_LPAREN &&
	    lexer_peek_ahead(1)->kind == TOK_STAR) {
		if (stmt_looks_like_pointer_to_array_declarator()) {
			const Token *pa_name;
			char varname[64];
			char gname[64];
			Global *g;
			int dims[MAX_ARRAY_DIMS] = {0};
			int dim_count = 0;
			Type *decl_ptr_type;

			lexer_next();
			lexer_next();
			skip_pointer_qualifiers();
			pa_name = lexer_peek();
			if (pa_name->kind != TOK_IDENT)
				fatal_cur("Expected pointer-to-array name\n");
			STRNCPY(varname, pa_name->text ? pa_name->text : "", sizeof(varname) - 1);
			lexer_next();
			expect(TOK_RPAREN);

			if (array_decl_looks_runtime_vla())
				fatal_cur("variable length array declaration cannot have 'static' storage duration\n");

			dim_count = parse_array_dimensions(dims, 1, 0);
			decl_ptr_type = type_ptr(build_array_type_from_dims(clone_type(base_type),
			                                                    dims, dim_count));
			if (stmt_type_is_variably_modified(decl_ptr_type))
				fatal_cur("variable length array declaration cannot have 'static' storage duration\n");

			snprintf(gname, sizeof(gname), "__static_%.24s_%.24s",
			         parser_current_function_name(), varname);
			g = new_global_object(gname, TCC_SIZEOF_PTR);
			parser_validate_decl_alignment(requested_align, decl_ptr_type);
			g->align = requested_align;
			apply_type_to_global(g, clone_type(decl_ptr_type));

			if (lexer_peek()->kind == TOK_ASSIGN) {
				lexer_next();
				parse_pointer_global_initializer(g,
				    "Static pointer initializer must be constant address for now\n",
				    1, 1, 1);
			}

			expect(TOK_SEMI);
			parser_commit_reserved_global();
			add_static_local(varname, gname, clone_type(decl_ptr_type),
			                 TCC_SIZEOF_PTR, 0, 1, requested_align);
			return new_block(NULL);
		}

		const Token *fp_name;
		char gname[64];
		Type **fp_param_types = NULL;
		int fp_param_count = 0;
		int fp_is_variadic = 0;
		int fp_fixed_params = 0;
		int fp_has_prototype = 0;
		Type *fp_ret_type = clone_type(base_type);
		Type *fp_ptr_type;
		Type *static_type;
		Global *g;
		int array_len = 1;
		int fp_dims[MAX_ARRAY_DIMS] = {0};
		int fp_dim_count = 0;

		lexer_next();
		lexer_next();
		if (lexer_peek()->kind == TOK_LPAREN) {
			Type **outer_param_types = NULL;
			int outer_param_count = 0;
			int outer_is_variadic = 0;
			int outer_fixed_params = 0;
			int outer_has_prototype = 0;
			Type **retfp_param_types[8] = {0};
			int retfp_param_count[8] = {0};
			int retfp_is_variadic[8] = {0};
			int retfp_fixed_params[8] = {0};
			int retfp_has_prototype[8] = {0};
			int retfp_level_count = 0;
			Type *ret_type;
			Type *decl_nested_type;

			lexer_next();
			expect(TOK_STAR);
			skip_pointer_qualifiers();
			fp_name = lexer_peek();
			if (fp_name->kind != TOK_IDENT) {
				fatal_cur("Expected function pointer name\n");
			}
			lexer_next();
			expect(TOK_RPAREN);
			parse_prototype_param_list(&outer_param_types, &outer_param_count,
			                          &outer_is_variadic, &outer_fixed_params,
			                          &outer_has_prototype, 1);
			expect(TOK_RPAREN);
			while (lexer_peek()->kind == TOK_LPAREN) {
				if (retfp_level_count >= 8)
					fatal_cur("Too many nested function-pointer return declarators\n");
				parse_prototype_param_list(&retfp_param_types[retfp_level_count],
				                          &retfp_param_count[retfp_level_count],
				                          &retfp_is_variadic[retfp_level_count],
				                          &retfp_fixed_params[retfp_level_count],
				                          &retfp_has_prototype[retfp_level_count], 1);
				retfp_level_count++;
			}

			ret_type = clone_type(base_type);
			for (int level = retfp_level_count - 1; level >= 0; level--) {
				ret_type = type_ptr(retfp_has_prototype[level]
				                    ? parser_make_function_type(ret_type,
				                                                retfp_param_types[level],
				                                                retfp_param_count[level],
				                                                retfp_is_variadic[level],
				                                                retfp_fixed_params[level])
				                    : type_func(clone_type(ret_type)));
			}

			decl_nested_type = type_ptr(outer_has_prototype
			                            ? parser_make_function_type(ret_type,
			                                                        outer_param_types,
			                                                        outer_param_count,
			                                                        outer_is_variadic,
			                                                        outer_fixed_params)
			                            : type_func(clone_type(ret_type)));

			snprintf(gname, sizeof(gname), "__static_%.24s_%.24s",
			         parser_current_function_name(), fp_name->text);
			g = new_global_object(gname, TCC_SIZEOF_PTR);
			parser_validate_decl_alignment(requested_align, decl_nested_type);
			g->align = requested_align;
			apply_type_to_global(g, clone_type(decl_nested_type));

			if (lexer_peek()->kind == TOK_ASSIGN) {
				const char *symbol = NULL;
				Node *expr;

				lexer_next();
				expr = stmt_parse_function_pointer_initializer_expr(&symbol);
				validate_pointer_initializer_compatibility(decl_nested_type, expr);
				if (symbol && symbol[0])
					parser_set_global_address_initializer(g, symbol);
				else
					set_global_integer_initializer(g, 0);
			}

			expect(TOK_SEMI);
			parser_commit_reserved_global();
			add_static_local(fp_name->text, gname, clone_type(decl_nested_type),
			                 TCC_SIZEOF_PTR, 0, 1, requested_align);
			return new_block(NULL);
		}

		fp_name = lexer_peek();
		skip_pointer_qualifiers();
		fp_name = lexer_peek();
		if (fp_name->kind != TOK_IDENT) {
			fatal_cur("Expected function pointer name\n");
		}
		lexer_next();
		if (lexer_peek()->kind == TOK_LBRACKET)
			fp_dim_count = parse_array_dimensions(fp_dims, 1, 0);
		expect(TOK_RPAREN);
		parse_prototype_param_list(&fp_param_types, &fp_param_count,
		                          &fp_is_variadic, &fp_fixed_params,
		                          &fp_has_prototype, 1);

		fp_ptr_type = type_ptr(fp_has_prototype
		                       ? parser_make_function_type(fp_ret_type,
		                                                   fp_param_types,
		                                                   fp_param_count,
		                                                   fp_is_variadic,
		                                                   fp_fixed_params)
		                       : type_func(clone_type(fp_ret_type)));
		static_type = fp_dim_count > 0
		            ? build_array_type_from_dims(fp_ptr_type, fp_dims, fp_dim_count)
		            : fp_ptr_type;
		if (fp_dim_count > 0)
			array_len = fp_dims[0];

		snprintf(gname, sizeof(gname), "__static_%.24s_%.24s",
		         parser_current_function_name(), fp_name->text);
		g = new_global_object(gname, TCC_SIZEOF_PTR);
		parser_validate_decl_alignment(requested_align, static_type);
		g->align = requested_align;
		apply_type_to_global(g, clone_type(static_type));

		if (lexer_peek()->kind == TOK_ASSIGN) {
			lexer_next();
			if (fp_dim_count > 0) {
				stmt_parse_static_function_pointer_array_initializer(g, fp_ptr_type, array_len);
			} else {
				const char *symbol = NULL;
				Node *expr = stmt_parse_function_pointer_initializer_expr(&symbol);

				validate_pointer_initializer_compatibility(fp_ptr_type, expr);
				if (symbol && symbol[0])
					parser_set_global_address_initializer(g, symbol);
				else
					set_global_integer_initializer(g, 0);
			}
		}

		expect(TOK_SEMI);
		parser_commit_reserved_global();
		add_static_local(fp_name->text, gname, clone_type(static_type),
		                 TCC_SIZEOF_PTR, fp_dim_count > 0,
		                 fp_dim_count > 0 ? g->array_len : 1, requested_align);
		return new_block(NULL);
	}

	{
		const Token *name = lexer_peek();
		char varname[64];
		char gname[64];
		Global *g;
		int g_committed = 0;
		int is_array = 0;
		int array_len = 1;
		int inner_total_dim = 1;

		if (name->kind != TOK_IDENT) {
			fatal_cur("Expected identifier after static type\n");
		}
		lexer_next();

		STRNCPY(varname, name->text ? name->text : "", sizeof(varname) - 1);
		snprintf(gname, sizeof(gname), "__static_%.24s_%.24s",
		         parser_current_function_name(), varname);
		if (is_global(gname)) {
			while (lexer_peek()->kind != TOK_SEMI && lexer_peek()->kind != TOK_EOF)
				lexer_next();
			if (lexer_peek()->kind == TOK_SEMI)
				lexer_next();
			return new_block(NULL);
		}

		g = new_global_object(gname, elem_size);
		parser_validate_decl_alignment(requested_align, base_type);
		g->align = requested_align;
		if (stmt_type_is_variably_modified(base_type))
			fatal_cur("variable length array declaration cannot have 'static' storage duration\n");

		if (lexer_peek()->kind == TOK_LBRACKET) {
			if (array_decl_looks_runtime_vla())
				fatal_cur("variable length array declaration cannot have 'static' storage duration\n");
			lexer_next();
			is_array = 1;
			array_len = 0;

			if (lexer_peek()->kind == TOK_NUM) {
				const Token *len = lexer_peek();
				if (len->value <= 0) {
					fatal_cur("Static array length must be positive\n");
				}
				array_len = len->value;
				lexer_next();
			}
			expect(TOK_RBRACKET);

			while (lexer_peek()->kind == TOK_LBRACKET) {
				lexer_next();
				if (lexer_peek()->kind == TOK_NUM) {
					int dim = (int)lexer_peek()->value;
					if (dim > 0) {
						elem_size *= dim;
						inner_total_dim *= dim;
					}
					lexer_next();
				}
				expect(TOK_RBRACKET);
			}

			g->is_array = 1;
			g->array_len = array_len;
			g->elem_size = elem_size;
			if (inner_total_dim > 1) {
				Type *inner_type = type_array(clone_type(decl_type), inner_total_dim);
				apply_type_to_global(g, type_array(inner_type, array_len));
			} else {
				apply_type_to_global(g, type_array(clone_type(decl_type), array_len));
			}

			if (lexer_peek()->kind == TOK_ASSIGN) {
				lexer_next();
				if (!stmt_parse_static_struct_array_initializer(&g, &g_committed,
				                                                base_type, &array_len,
				                                                &elem_size) &&
				    lexer_peek()->kind == TOK_LBRACE && !type_is_struct(base_type)) {
					lexer_next();
					while (lexer_peek()->kind != TOK_RBRACE) {
						if (lexer_peek()->kind == TOK_LBRACE) {
							lexer_next();
							while (lexer_peek()->kind != TOK_RBRACE && lexer_peek()->kind != TOK_EOF) {
								const Token *iv = lexer_peek();
								long long v2 = 0;
								if (iv->kind == TOK_MINUS && lexer_peek_ahead(1)->kind == TOK_NUM) {
									lexer_next();
									v2 = -(long long)lexer_peek()->long_value;
									lexer_next();
								} else if (iv->kind == TOK_NUM) {
									v2 = iv->long_value;
									lexer_next();
								} else {
									v2 = parser_eval_const_int_expr();
								}
								{
									int ic = global_init_count(g);
									for (int bi = 0; bi < elem_size; bi++)
										global_set_init_byte(g, ic + bi,
										                    (int)((v2 >> (8 * bi)) & 0xff));
									global_set_init_count(g, ic + elem_size);
								}
								if (lexer_peek()->kind == TOK_COMMA)
									lexer_next();
							}
							expect(TOK_RBRACE);
							if (lexer_peek()->kind == TOK_COMMA)
								lexer_next();
							continue;
						}
						if (lexer_peek()->kind == TOK_LPAREN &&
						    is_type_start_token(lexer_peek_ahead(1)->kind, lexer_peek_ahead(1)->text)) {
							lexer_next();
							while (lexer_peek()->kind != TOK_RPAREN && lexer_peek()->kind != TOK_EOF)
								lexer_next();
							if (lexer_peek()->kind == TOK_RPAREN)
								lexer_next();
						}
						{
							const Token *value = lexer_peek();
							if (value->kind == TOK_STRING && is_ptr_type) {
								int elem_idx = global_init_count(g) / 8;
								int offset = elem_idx * 8;
								char str_gname[64];

								if (elem_idx >= 256 || (array_len > 0 && elem_idx >= array_len)) {
									fatal_cur("Too many static array initializers\n");
								}

								snprintf(str_gname, sizeof(str_gname), "__str_%d",
								         parser_alloc_string_label());
								{
									int g_idx = parser_global_index(g);
									if (!g_committed) {
										parser_commit_reserved_global();
										g_committed = 1;
									}
									{
										Global *sg = new_global_slot(str_gname);
										set_global_string_array_initializer_len(sg, value->text,
										                                       value->text_len);
										sg->is_array = 1;
										sg->elem_size = 1;
										sg->array_len = (int)value->text_len + 1;
										global_set_init_count(sg, sg->array_len);
										parser_commit_reserved_global();
									}
									g = parser_global_at(g_idx);
								}
								for (int b = 0; b < 8; b++)
									global_set_init_byte(g, offset + b, 0);
								global_set_init_sym(g, elem_idx, str_gname);
								global_set_init_count(g, offset + 8);
							} else if ((value->kind == TOK_NUM &&
							            (lexer_peek_ahead(1)->kind == TOK_PIPE ||
							             lexer_peek_ahead(1)->kind == TOK_AMP ||
							             lexer_peek_ahead(1)->kind == TOK_SHL ||
							             lexer_peek_ahead(1)->kind == TOK_SHR ||
							             lexer_peek_ahead(1)->kind == TOK_PLUS ||
							             lexer_peek_ahead(1)->kind == TOK_STAR ||
							             lexer_peek_ahead(1)->kind == TOK_SLASH)) ||
							           value->kind == TOK_MINUS || value->kind == TOK_LPAREN) {
								long long v2 = parser_eval_const_int_expr();
								int ic2 = global_init_count(g);
								for (int bi2 = 0; bi2 < elem_size; bi2++)
									global_set_init_byte(g, ic2 + bi2,
									                    (int)((v2 >> (8 * bi2)) & 0xff));
								global_set_init_count(g, ic2 + elem_size);
								if (lexer_peek()->kind == TOK_COMMA) {
									lexer_next();
									continue;
								}
								break;
							} else if (value->kind == TOK_NUM) {
								int v = (int)value->long_value;
								int ic = global_init_count(g);
								for (int bi = 0; bi < elem_size; bi++)
									global_set_init_byte(g, ic + bi, (v >> (8 * bi)) & 0xff);
								global_set_init_count(g, ic + elem_size);
							} else {
								fatal_cur("Static array initializer must contain constant integers\n");
							}
						}
						lexer_next();
						if (lexer_peek()->kind == TOK_COMMA)
							lexer_next();
						else
							break;
					}
					expect(TOK_RBRACE);
				} else if (lexer_peek()->kind == TOK_STRING) {
					const Token *value = lexer_peek();
					stmt_require_string_literal_array_match(value, elem_size,
					                                        "String literal element width does not match static array element type");
					set_global_string_array_initializer_len(g, value->text, value->text_len);
					global_set_init_count(g,
					                      stmt_string_literal_elem_width(value) > 1
					                        ? (int)(value->text_len / (size_t)stmt_string_literal_elem_width(value)) + 1
					                        : (int)value->text_len + 1);
					lexer_next();
				} else if (lexer_peek()->kind != TOK_RBRACE && lexer_peek()->kind != TOK_SEMI) {
					fatal_cur("Unsupported static array initializer\n");
				}
			}

			if (array_len == 0) {
				if (g->is_string_array)
					array_len = (int)g->string_len + 1;
				else if (is_ptr_type)
					array_len = global_init_count(g) / 8;
				else if (elem_size > 1)
					array_len = global_init_count(g) / elem_size;
				else
					array_len = global_init_count(g);
			}
			if (array_len <= 0) {
				fatal_cur("Static array length must be known\n");
			}
			g->array_len = array_len;
		} else {
			apply_type_to_global(g, clone_type(decl_type));
			if (lexer_peek()->kind == TOK_ASSIGN) {
				lexer_next();
				if (is_ptr_type) {
					parse_pointer_global_initializer(g,
					    "Static pointer initializer must be constant address for now\n",
					    1, 1, 1);
				} else if (lexer_peek()->kind == TOK_LBRACE && type_is_struct(base_type)) {
					const char *struct_tag2 = parser_resolve_struct_type_name(base_type);
					StructDef *sdef2;
					int g_idx2;

					if (!struct_tag2 || !struct_tag2[0])
						struct_tag2 = base_type->struct_name;
					sdef2 = struct_tag2 && struct_tag2[0] ? find_struct_or_null(struct_tag2) : NULL;
					if (sdef2) {
						g_idx2 = parser_global_index(g);
						if (!g_committed) {
							parser_commit_reserved_global();
							g_committed = 1;
						}
						g = parser_global_at(g_idx2);
						global_set_init_count(g, sdef2->size);
						lexer_next();
						globals_ensure_spare(256);
						g = parser_global_at(g_idx2);
						parse_global_struct_initializer_body(g_idx2, sdef2, 0);
						g = parser_global_at(g_idx2);
						expect(TOK_RBRACE);
					} else {
						int bd2 = 1;
						lexer_next();
						while (bd2 > 0 && lexer_peek()->kind != TOK_EOF) {
							if (lexer_peek()->kind == TOK_LBRACE) bd2++;
							else if (lexer_peek()->kind == TOK_RBRACE) bd2--;
							lexer_next();
						}
					}
				} else {
					int init_value = parse_global_scalar_initializer_value_or_die(
					    "Static initializer must be constant\n");
					set_global_integer_initializer(g, elem_size == 1 ? (init_value & 255) : init_value);
				}
			}
		}

		expect(TOK_SEMI);
		if (!g_committed)
			parser_commit_reserved_global();

		add_static_local(varname, gname,
		    is_array ? (inner_total_dim > 1
		                ? type_array(type_array(clone_type(decl_type), inner_total_dim), array_len)
		                : type_array(clone_type(decl_type), array_len))
		             : clone_type(decl_type),
		    elem_size, is_array, array_len, requested_align);
		return new_block(NULL);
	}
}

void 
parse_struct_definition(void)
{

	expect(TOK_STRUCT);

	const Token *name = lexer_peek();
	char struct_def_name[64];
	StructDef *def;

	if (name->kind == TOK_LBRACE) {
		/* anonymous struct definition */
		snprintf(struct_def_name, sizeof(struct_def_name), "__anon_struct_%d", ++parser_anon_struct_id);
		def = structs_push();
		memset(def, 0, sizeof(*def));
		STRNCPY(def->name, struct_def_name, sizeof(def->name) - 1);
		parse_struct_body_into(def);
	} else {
		if (name->kind != TOK_IDENT) {
			fatal_cur("Expected struct name\n");
		}
		if (getenv("TCC_TRACE_PARSE_TOPLEVEL"))
			fprintf(stderr, "tcc parse: struct-def named=%s before-forward\n", name->text);
		STRNCPY(struct_def_name, name->text, sizeof(struct_def_name) - 1);
		def = get_or_add_forward_struct(name->text);
		if (getenv("TCC_TRACE_PARSE_TOPLEVEL"))
			fprintf(stderr, "tcc parse: struct-def named=%s got-forward def=%p\n",
			        name->text, (void *)def);
		memset(def, 0, sizeof(*def));
		STRNCPY(def->name, name->text, sizeof(def->name) - 1);
		lexer_next();
		if (getenv("TCC_TRACE_PARSE_TOPLEVEL"))
			fprintf(stderr, "tcc parse: struct-def named=%s before-body\n", def->name);
		parse_struct_body_into(def);
		def = find_struct(struct_def_name);
	}

	/* "struct [Name] { ... } varname [= {...}];  or  varname[] = {...}" — global variable declaration */
	if (lexer_peek()->kind == TOK_IDENT || lexer_peek()->kind == TOK_STAR) {
		Global *g = new_global_slot(NULL);
		int ptr = 0;
		int is_arr = 0;
		int arr_len = 0;
		if (lexer_peek()->kind == TOK_STAR) {
			ptr = 1;
			lexer_next();
		}
		const Token *var = lexer_peek();
		if (var->kind != TOK_IDENT)
			fatal_cur("Expected global variable name\n");
		STRNCPY(g->name, var->text, sizeof(g->name) - 1);
		lexer_next();
		if (lexer_peek()->kind == TOK_LBRACKET) {
			lexer_next();
			is_arr = 1;
			if (lexer_peek()->kind == TOK_NUM) {
				arr_len = (int)lexer_peek()->value;
				lexer_next();
			}
			expect(TOK_RBRACKET);
		}
		if (ptr) {
			g->elem_size = TCC_SIZEOF_PTR;
		} else {
			g->is_struct = 1;
			g->elem_size = def->size;
		}
		if (is_arr) g->is_array = 1;
		g->array_len = arr_len > 0 ? arr_len : 1;
		STRNCPY(g->struct_name, struct_def_name, sizeof(g->struct_name) - 1);
		if (lexer_peek()->kind == TOK_ASSIGN) {
			lexer_next();
			if (is_arr) {
				int depth2 = 1;
				int ecount = 0;
				expect(TOK_LBRACE);
				while (depth2 > 0 && lexer_peek()->kind != TOK_EOF) {
					if (lexer_peek()->kind == TOK_LBRACE) { depth2++; if (depth2 == 2) ecount++; }
					else if (lexer_peek()->kind == TOK_RBRACE) depth2--;
					if (depth2 > 0) lexer_next();
				}
				if (lexer_peek()->kind == TOK_RBRACE) lexer_next();
				if (arr_len == 0) arr_len = ecount > 0 ? ecount : 1;
				g->array_len = arr_len;
				global_set_init_count(g, arr_len * def->size);
			} else {
			{
					int _gi2 = parser_global_index(g);
					if (!try_parse_global_struct_compound_initializer(_gi2, def, struct_def_name, 0)) {
						expect(TOK_LBRACE);
						g = parser_global_at(_gi2);
						global_set_init_count(g, def->size);
						parse_global_struct_initializer_body(_gi2, def, 0);
						g = parser_global_at(_gi2); /* re-derive: body may have reallocated globals[] */
					}
				expect(TOK_RBRACE);
			}
			} /* end else !is_arr */
		}
		expect(TOK_SEMI);
		commit_global_definition(g);
		return;
	}

	expect(TOK_SEMI);
}

Node *
append_struct_copy_from_ptr_fields_at(Node *head, int dst_base, Node *src_base, const char *struct_name, int src_base_offset)
{
	StructDef *def = find_struct(struct_name);
	Field *field = def->fields;
	Field *field_end = field + def->field_count;
	Node *tail = stmt_node_list_tail(head);

	while (field < field_end) {
		if (field->is_struct) {
			head = append_struct_copy_from_ptr_fields_at(head,
			        dst_base + field->offset, src_base, field->struct_name,
			        src_base_offset + field->offset);
			tail = stmt_node_list_tail(head);
			field++;
			continue;
		}

		/*
		 * Copy scalar/array storage in chunks.  Array fields such as
		 * unsigned char u6_addr8[16] cannot be copied with elem_size=16
		 * because scalar load/store backends only handle 1/2/4/8 directly.
		 */
		for (int off = 0; off < field->size; ) {
			int chunk = field->size - off;
			if (chunk >= TCC_STORE_WIDTH_8)
				chunk = TCC_STORE_WIDTH_8;
			else if (chunk >= TCC_STORE_WIDTH_4)
				chunk = TCC_STORE_WIDTH_4;
			else if (chunk >= TCC_STORE_WIDTH_2)
				chunk = TCC_STORE_WIDTH_2;
			else
				chunk = TCC_STORE_WIDTH_1;

			Node *lhs = new_member(field->name, dst_base + field->offset + off);
			lhs->elem_size = chunk;
			lhs->type = type_for_size(chunk);
			STRNCPY(lhs->struct_name, struct_name, sizeof(lhs->struct_name) - 1);

			Node *base = clone_node_tree(src_base);
			Node *rhs = new_member_ptr(field->name, base, src_base_offset + field->offset + off);
			rhs->elem_size = chunk;
			rhs->type = type_for_size(chunk);

			stmt_append_node_to_tail(&head, &tail, new_assign(lhs, rhs));
			off += chunk;
		}

		field++;
	}

	return head;
}

Node *
append_struct_copy_from_ptr_fields(Node *head, const char *dst_name, int dst_base, Node *src_base, const char *struct_name, int offset_base)
{
	StructDef *def = find_struct(struct_name);
	Node *tail = stmt_node_list_tail(head);

	for (int i = 0; i < def->field_count; i++) {
		Field *field = &def->fields[i];

		if (field->is_struct) {
			head = append_struct_copy_from_ptr_fields(head, dst_name, dst_base, src_base,
			       field->struct_name, offset_base + field->offset);
			tail = stmt_node_list_tail(head);
			continue;
		}

		for (int off = 0; off < field->size; ) {
			int chunk = field->size - off;
			if (chunk >= TCC_STORE_WIDTH_8)
				chunk = TCC_STORE_WIDTH_8;
			else if (chunk >= TCC_STORE_WIDTH_4)
				chunk = TCC_STORE_WIDTH_4;
			else if (chunk >= TCC_STORE_WIDTH_2)
				chunk = TCC_STORE_WIDTH_2;
			else
				chunk = TCC_STORE_WIDTH_1;

			Node *lhs = new_member(field->name, dst_base + offset_base + field->offset + off);
			lhs->elem_size = chunk;
			lhs->type = type_for_size(chunk);
			STRNCPY(lhs->struct_name, struct_name, sizeof(lhs->struct_name) - 1);

			Node *base = new_var(src_base->name, src_base->offset);
			base->is_pointer = src_base->is_pointer;
			base->elem_size = src_base->elem_size;
			base->type = src_base->type;
			STRNCPY(base->struct_name, src_base->struct_name, sizeof(base->struct_name) - 1);

			Node *rhs = new_member_ptr(field->name, base, offset_base + field->offset + off);
			rhs->elem_size = chunk;
			rhs->type = type_for_size(chunk);

			stmt_append_node_to_tail(&head, &tail, new_assign(lhs, rhs));
			off += chunk;
		}
	}

	(void)dst_name;
	return head;
}

Node *
build_struct_param_copy(const char *dst_name, const char *hidden_name, const char *struct_name)
{
	Node *hidden = make_scalar_var_node(hidden_name);
	return append_struct_copy_from_ptr_fields(NULL, dst_name, find_local(dst_name), hidden, struct_name, 0);
}
