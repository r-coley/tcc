/*
 * expr.c — extracted from parser.c
 */
#include "parser_internal.h"

#define EXPR_PARSE_DEPTH_LIMIT 1024
#define EXPR_PARSE_DEPTH_ENTER() do { \
	if (++expr_parse_depth > EXPR_PARSE_DEPTH_LIMIT) \
		fatal_cur("Expression nesting too deep\n"); \
} while (0)
#define EXPR_PARSE_DEPTH_LEAVE() do { expr_parse_depth--; } while (0)

static int expr_parse_depth;

static Node *parse_scalar_compound_literal(Type *type);
static Node *parse_array_compound_literal_expr(Type *type);
static Node *parse_aggregate_compound_literal_expr(Type *type);
static Node *parse_scalar_compound_literal_address(Type *type);
static Node *parse_compound_literal_address(Type *type);
static Node *expr_parse_array_compound_literal_leaf_value(Type *type);
static int expr_try_parse_local_array_designator(int *out_lo, int *out_hi);
static void expr_array_init_reserve(int needed, int *cap, Node ***exprs,
                                    unsigned char **seen);
static void expr_copy_array_init_span(int dst_base_index, int span_len,
                                      int *dst_exprs_cap, Node ***dst_exprs,
                                      unsigned char **dst_seen,
                                      Node **src_exprs, int src_exprs_cap,
                                      const unsigned char *src_seen, int src_seen_cap,
                                      int *max_init_index, int *init_count);
static int expr_array_flat_len(Type *type);
static Type *expr_array_leaf_elem_type(Type *type);
static void expr_parse_multidim_array_compound_literal_initializer(Type *array_type,
                                                                   int base_index,
                                                                   int *init_exprs_cap,
                                                                   Node ***init_exprs,
                                                                   unsigned char **init_seen,
                                                                   int *max_init_index,
                                                                   int *init_count);
static void validate_cast_operand(Type *dst_type, Node *expr);
static void validate_integer_unary_operand(Node *node);
static void validate_integer_binary_operands(Node *lhs, Node *rhs);
static int sizeof_type_array_bound_is_const_expr(void);
static int expr_type_contains_source_kind(const Type *type, int source_kind);
static int expr_special_type_token_source_kind(const Token *token);
static int expr_type_is_pointer_to_complex_object(const Type *type);
static int expr_node_has_complex_value(const Node *node);
static int expr_node_has_imaginary_value(const Node *node);
static int expr_type_uses_x64_complex_float_abi(const Type *type);
static Type *expr_complex_real_component_type(const Type *type);
static Type *expr_imaginary_real_component_type(const Type *type);
static Type *expr_make_complex_type_from_real(Type *type);
static Type *expr_make_imaginary_type_from_real(Type *type);
static Type *expr_fp_usual_arith_type(Type *a, Type *b);
static int expr_type_is_real_scalar_type(const Type *type);
static int expr_node_is_real_scalar(const Node *node);
static int expr_node_is_simple_lvalue(const Node *node);
static Node *expr_extract_addressable_temp(Node *node, Node **setup_out);
static Node *expr_build_complex_from_imaginary(Node *operand, Type *dst_type);
static Node *expr_build_complex_value_cast(Node *operand, Type *dst_type);
static Node *expr_build_complex_bool_cast(Node *operand, Type *dst_type);
static Node *expr_build_complex_real_extract_cast(Node *operand, Type *dst_type);
static Node *expr_build_imaginary_real_extract_cast(Node *operand, Type *dst_type);
static Node *expr_build_complex_imaginary_extract_cast(Node *operand, Type *dst_type);
static Node *expr_build_complex_complex_cast(Node *operand, Type *dst_type);
static Node *expr_build_complex_additive(Node *left, Node *right, NodeKind kind);
static Node *expr_build_complex_multiplicative(Node *left, Node *right, NodeKind kind);
static Node *expr_build_complex_unary_neg(Node *operand);
static Node *expr_build_real_imag_additive(Node *left, Node *right, NodeKind kind);
static Node *expr_build_complex_imag_additive(Node *left, Node *right, NodeKind kind);
static Node *expr_build_imaginary_additive(Node *left, Node *right, NodeKind kind);
static Node *expr_build_imaginary_multiplicative(Node *left, Node *right, NodeKind kind);

static const char *
expr_type_display_name(const Type *type)
{
	const char *name;

	name = type_source_display_name(type);
	if (name)
		return name;
	if (!type)
		return "unknown";
	if (type_is_pointer(type))
		return "pointer";
	if (type_is_array(type))
		return "array";
	if (type_is_function(type))
		return "function";
	if (type_is_union(type))
		return "union";
	if (type_is_struct(type))
		return "struct";
	if (type->kind == TY_CHAR)
		return type_is_unsigned(type) ? "unsigned char" : "char";
	if (type->kind == TY_SHORT)
		return type_is_unsigned(type) ? "unsigned short" : "short";
	if (type->kind == TY_INT)
		return type_is_unsigned(type) ? "unsigned int" : "int";
	return "unknown";
}

static const char *
expr_node_type_display_name(const Node *node)
{
	return expr_type_display_name(node ? node->type : NULL);
}

static Type *
expr_pointer_context_type(Type *type)
{
	if (!type)
		return NULL;
	if (type_is_array(type) && type_pointee(type))
		return type_ptr(type_pointee(type));
	if (type_is_function(type))
		return type_ptr(type);
	return type;
}

static int
expr_type_contains_source_kind(const Type *type, int source_kind)
{
	Type **params = NULL;
	int param_count = 0;

	if (!type)
		return 0;
	if (type_source_is(type, source_kind))
		return 1;
	if ((type_is_pointer(type) || type_is_array(type)) &&
	    expr_type_contains_source_kind(type_pointee(type), source_kind))
		return 1;
	if (type_is_function(type)) {
		if (expr_type_contains_source_kind(type_pointee(type), source_kind))
			return 1;
		if (type_func_metadata((Type *)type, &params, &param_count, NULL, NULL)) {
			for (int i = 0; i < param_count; i++) {
				if (expr_type_contains_source_kind(params[i], source_kind))
					return 1;
			}
		}
	}
	return 0;
}

static int
expr_special_type_token_source_kind(const Token *token)
{
	if (!token || token->kind != TOK_IDENT || !token->text)
		return TYPE_SOURCE_DEFAULT;
	if (STRCMP(token->text, "_Complex") == 0)
		return TYPE_SOURCE_COMPLEX;
	if (STRCMP(token->text, "_Imaginary") == 0)
		return TYPE_SOURCE_IMAGINARY;
	return TYPE_SOURCE_DEFAULT;
}

static int
expr_type_is_pointer_to_complex_object(const Type *type)
{
	Type *ptr_type;
	Type *base_type;

	ptr_type = expr_pointer_context_type((Type *)type);
	if (!ptr_type || !type_is_pointer(ptr_type))
		return 0;
	base_type = type_pointee(ptr_type);
	if (!base_type || type_is_function(base_type))
		return 0;
	return expr_type_contains_source_kind(base_type, TYPE_SOURCE_COMPLEX);
}

static int
expr_node_has_complex_value(const Node *node)
{
	return node && node->type && type_is_complex(node->type);
}

static int
expr_node_has_imaginary_value(const Node *node)
{
	return node && node->type && type_is_imaginary(node->type);
}

static int
expr_type_uses_x64_complex_float_abi(const Type *type)
{
	return type && type_is_complex(type) &&
	       parser_classify_aggregate_abi((Type *)type, NULL) ==
	           AGGREGATE_ABI_X64_COMPLEX_FLOAT;
}

static Type *
expr_complex_real_component_type(const Type *type)
{
	const char *source_name;

	if (!type || !type_is_complex(type))
		return NULL;

	if (type->kind == TY_FLOAT)
		return type_with_source(type_float(), TYPE_SOURCE_FLOAT, "float");

	source_name = type_source_name(type);
	if (source_name && strstr(source_name, "long double"))
		return type_with_source(type_double(), TYPE_SOURCE_LONG_DOUBLE, "long double");

	if (type->kind == TY_DOUBLE)
		return type_with_source(type_double(), TYPE_SOURCE_DOUBLE, "double");

	return NULL;
}

static Type *
expr_imaginary_real_component_type(const Type *type)
{
	const char *source_name;

	if (!type || !type_is_imaginary(type))
		return NULL;

	if (type->kind == TY_FLOAT)
		return type_with_source(type_float(), TYPE_SOURCE_FLOAT, "float");

	source_name = type_source_name(type);
	if (source_name && strstr(source_name, "long double"))
		return type_with_source(type_double(), TYPE_SOURCE_LONG_DOUBLE, "long double");

	if (type->kind == TY_DOUBLE)
		return type_with_source(type_double(), TYPE_SOURCE_DOUBLE, "double");

	return NULL;
}

static Type *
expr_make_imaginary_type_from_real(Type *type)
{
	if (!type)
		return NULL;
	if (type->kind == TY_FLOAT)
		return type_with_source(type_float(), TYPE_SOURCE_IMAGINARY, "_Imaginary float");
	if (type->kind == TY_DOUBLE && type_source_is(type, TYPE_SOURCE_LONG_DOUBLE))
		return type_with_source(type_double(), TYPE_SOURCE_IMAGINARY, "_Imaginary long double");
	if (type->kind == TY_DOUBLE)
		return type_with_source(type_double(), TYPE_SOURCE_IMAGINARY, "_Imaginary double");
	return NULL;
}

static Type *
expr_make_complex_type_from_real(Type *type)
{
	if (!type)
		return NULL;
	if (type->kind == TY_FLOAT)
		return type_with_source(type_float(), TYPE_SOURCE_COMPLEX, "_Complex float");
	if (type->kind == TY_DOUBLE && type_source_is(type, TYPE_SOURCE_LONG_DOUBLE))
		return type_with_source(type_double(), TYPE_SOURCE_COMPLEX, "_Complex long double");
	if (type->kind == TY_DOUBLE)
		return type_with_source(type_double(), TYPE_SOURCE_COMPLEX, "_Complex double");
	return NULL;
}

static Type *
expr_fp_usual_arith_type(Type *a, Type *b)
{
	if (!a && !b)
		return NULL;
	if (!a)
		return clone_type(b);
	if (!b)
		return clone_type(a);

	if ((a->kind == TY_DOUBLE && type_source_is(a, TYPE_SOURCE_LONG_DOUBLE)) ||
	    (b->kind == TY_DOUBLE && type_source_is(b, TYPE_SOURCE_LONG_DOUBLE)))
		return type_with_source(type_double(), TYPE_SOURCE_LONG_DOUBLE, "long double");
	if (a->kind == TY_DOUBLE || b->kind == TY_DOUBLE)
		return type_with_source(type_double(), TYPE_SOURCE_DOUBLE, "double");
	return type_with_source(type_float(), TYPE_SOURCE_FLOAT, "float");
}

static int
expr_type_is_real_scalar_type(const Type *type)
{
	return type &&
	       !type_is_complex(type) &&
	       !type_is_imaginary(type) &&
	       !type_is_pointer(type) &&
	       (type_is_integer(type) || type_is_floating(type));
}

static int
expr_node_is_real_scalar(const Node *node)
{
	Type *type;

	if (!node || !node->type)
		return 0;
	type = node->type;
	return expr_type_is_real_scalar_type(type);
}

static int
expr_node_is_simple_lvalue(const Node *node)
{
	if (!node)
		return 0;
	switch (node->kind) {
	case ND_VAR:
	case ND_GLOBAL:
	case ND_MEMBER:
	case ND_DEREF:
	case ND_MEMBER_PTR:
	case ND_INDEX:
	case ND_GLOBAL_INDEX:
		return 1;
	default:
		return 0;
	}
}

static Node *
expr_extract_addressable_temp(Node *node, Node **setup_out)
{
	if (!node)
		return NULL;
	if (node->kind == ND_COMMA &&
	    node->left &&
	    node->right &&
	    expr_node_is_simple_lvalue(node->right)) {
		if (setup_out)
			*setup_out = append_node(*setup_out, node->left);
		return node->right;
	}
	return NULL;
}

static Node *
expr_build_complex_from_imaginary(Node *operand, Type *dst_type)
{
	char temp_name[64];
	int offset;
	int object_size;
	int lane_size;
	Type *dst_real_type;
	Type *src_real_type;
	Node *head;
	Node *complex_lhs;
	Node *complex_value;
	Node *imag_addr;
	Node *imag_ptr;
	Node *imag_lhs;
	Node *imag_rhs;
	Node *store_imag;
	Node *expr;

	if (!operand || !operand->type || !dst_type || !type_is_complex(dst_type))
		return NULL;
	if (!expr_node_has_imaginary_value(operand))
		return NULL;

	dst_real_type = expr_complex_real_component_type(dst_type);
	src_real_type = expr_imaginary_real_component_type(operand->type);
	if (!dst_real_type || !src_real_type)
		return NULL;

	snprintf(temp_name, sizeof(temp_name), "__complex_from_imag_%d",
	         parser_alloc_compound_arg_temp_id());
	offset = add_typed_local(temp_name, dst_type);
	object_size = type_sizeof(dst_type);
	lane_size = type_sizeof(dst_real_type);

	head = append_local_zero_fill(NULL, temp_name, offset, object_size);

	complex_lhs = new_var(temp_name, offset);
	complex_lhs->type = clone_type(dst_type);
	complex_lhs->elem_size = object_size;

	imag_addr = new_addr(complex_lhs);
	imag_addr = new_binary(ND_ADD, imag_addr, new_num(lane_size));
	imag_ptr = new_cast(imag_addr, type_ptr(dst_real_type));
	imag_lhs = new_deref(imag_ptr);
	imag_lhs->type = clone_type(dst_real_type);
	imag_lhs->elem_size = lane_size;

	imag_rhs = operand;
	if (!type_equal_unqualified(operand->type, dst_real_type))
		imag_rhs = new_cast(imag_rhs, dst_real_type);
	store_imag = new_assign(imag_lhs, imag_rhs);
	head = append_node(head, store_imag);

	complex_value = new_var(temp_name, offset);
	complex_value->type = clone_type(dst_type);
	complex_value->elem_size = object_size;

	expr = new_binary(ND_COMMA, new_block(head), complex_value);
	expr->type = clone_type(dst_type);
	expr->elem_size = object_size;
	return expr;
}

static Node *
expr_build_complex_value_cast(Node *operand, Type *dst_type)
{
	char temp_name[64];
	int offset;
	int object_size;
	Type *real_type;
	Node *head;
	Node *complex_lhs;
	Node *complex_value;
	Node *real_addr;
	Node *real_ptr;
	Node *real_lhs;
	Node *real_rhs;
	Node *store_real;
	Node *expr;

	if (!operand || !dst_type || !type_is_complex(dst_type))
		return NULL;

	real_type = expr_complex_real_component_type(dst_type);
	if (!real_type)
		fatal_cur("complex types are not supported\n");
	if (!expr_node_is_real_scalar(operand))
		fatal_cur("complex value casts are not supported yet\n");

	snprintf(temp_name, sizeof(temp_name), "__complex_cast_%d",
	         parser_alloc_compound_arg_temp_id());
	offset = add_typed_local(temp_name, dst_type);
	object_size = type_sizeof(dst_type);

	head = append_local_zero_fill(NULL, temp_name, offset, object_size);

	complex_lhs = new_var(temp_name, offset);
	complex_lhs->type = clone_type(dst_type);
	complex_lhs->elem_size = object_size;

	real_addr = new_addr(complex_lhs);
	real_ptr = new_cast(real_addr, type_ptr(real_type));
	real_lhs = new_deref(real_ptr);
	real_lhs->type = clone_type(real_type);
	real_lhs->elem_size = type_sizeof(real_type);

	real_rhs = operand;
	if (!type_equal_unqualified(real_rhs->type, real_type))
		real_rhs = new_cast(real_rhs, real_type);
	store_real = new_assign(real_lhs, real_rhs);
	head = append_node(head, store_real);

	complex_value = new_var(temp_name, offset);
	complex_value->type = clone_type(dst_type);
	complex_value->elem_size = object_size;

	expr = new_binary(ND_COMMA, new_block(head), complex_value);
	expr->type = clone_type(dst_type);
	expr->elem_size = object_size;
	return expr;
}

static Node *
expr_build_complex_bool_cast(Node *operand, Type *dst_type)
{
	Type *component_type;
	Node *zero;
	Node *result;

	if (!operand || !operand->type || !dst_type ||
	    !type_source_is_bool_spelling(dst_type))
		return NULL;

	if (type_is_imaginary(operand->type)) {
		component_type = expr_imaginary_real_component_type(operand->type);
		if (!component_type)
			return NULL;
		if (!type_equal_unqualified(operand->type, component_type))
			operand = new_cast(operand, component_type);
		zero = new_num_fp(clone_type(component_type), "0.0");
		result = new_binary(ND_NE, operand, zero);
		return new_cast(result, dst_type);
	}

	if (type_is_complex(operand->type)) {
		char temp_name[64];
		int offset;
		int object_size;
		int lane_size;
		Node *complex_lhs;
		Node *complex_value;
		Node *assign;
		Node *head;
		Node *base_addr;
		Node *real_ptr;
		Node *imag_addr;
		Node *imag_ptr;
		Node *real_value;
		Node *imag_value;
		Node *real_nonzero;
		Node *imag_nonzero;

		component_type = expr_complex_real_component_type(operand->type);
		if (!component_type)
			return NULL;

		snprintf(temp_name, sizeof(temp_name), "__complex_bool_%d",
		         parser_alloc_compound_arg_temp_id());
		offset = add_typed_local(temp_name, operand->type);
		object_size = type_sizeof(operand->type);
		lane_size = type_sizeof(component_type);

		complex_lhs = new_var(temp_name, offset);
		complex_lhs->type = clone_type(operand->type);
		complex_lhs->elem_size = object_size;
		assign = new_struct_assign(complex_lhs, operand, object_size);
		head = new_block(assign);

		complex_value = new_var(temp_name, offset);
		complex_value->type = clone_type(operand->type);
		complex_value->elem_size = object_size;
		base_addr = new_addr(complex_value);

		real_ptr = new_cast(clone_node_tree(base_addr), type_ptr(component_type));
		real_value = new_deref(real_ptr);
		real_value->type = clone_type(component_type);
		real_value->elem_size = lane_size;

		imag_addr = new_binary(ND_ADD, base_addr, new_num(lane_size));
		imag_ptr = new_cast(imag_addr, type_ptr(component_type));
		imag_value = new_deref(imag_ptr);
		imag_value->type = clone_type(component_type);
		imag_value->elem_size = lane_size;

		zero = new_num_fp(clone_type(component_type), "0.0");
		real_nonzero = new_binary(ND_NE, real_value, zero);

		zero = new_num_fp(clone_type(component_type), "0.0");
		imag_nonzero = new_binary(ND_NE, imag_value, zero);
		result = new_binary(ND_LOGICAL_OR, real_nonzero, imag_nonzero);
		result = new_binary(ND_COMMA, head, result);
		return new_cast(result, dst_type);
	}

	return NULL;
}

Node *
expr_coerce_scalar_condition(Node *value)
{
	Type *bool_type;

	if (!value || !value->type)
		return value;
	if (!type_is_scalar(value->type))
		fatal_cur("controlling expression must have scalar type\n");
	if (!type_is_complex(value->type) && !type_is_imaginary(value->type))
		return value;

	bool_type = type_with_source(type_uchar(), TYPE_SOURCE_BOOL, "_Bool");
	return expr_build_complex_bool_cast(value, bool_type);
}

static Node *
expr_build_complex_real_extract_cast(Node *operand, Type *dst_type)
{
	char temp_name[64];
	int offset;
	int object_size;
	Type *src_type;
	Type *real_type;
	Node *head;
	Node *complex_lhs;
	Node *complex_value;
	Node *assign;
	Node *real_addr;
	Node *real_ptr;
	Node *real_value;
	Node *expr;

	if (!operand || !operand->type || !dst_type)
		return NULL;
	src_type = operand->type;
	if (!type_is_complex(src_type) || !expr_type_is_real_scalar_type(dst_type))
		return NULL;
	if (type_source_is_bool_spelling(dst_type))
		return expr_build_complex_bool_cast(operand, dst_type);

	real_type = expr_complex_real_component_type(src_type);
	if (!real_type)
		fatal_cur("complex value casts are not supported yet\n");

	snprintf(temp_name, sizeof(temp_name), "__complex_extract_%d",
	         parser_alloc_compound_arg_temp_id());
	offset = add_typed_local(temp_name, src_type);
	object_size = type_sizeof(src_type);

	complex_lhs = new_var(temp_name, offset);
	complex_lhs->type = clone_type(src_type);
	complex_lhs->elem_size = object_size;
	assign = new_struct_assign(complex_lhs, operand, object_size);
	head = new_block(assign);

	complex_value = new_var(temp_name, offset);
	complex_value->type = clone_type(src_type);
	complex_value->elem_size = object_size;

	real_addr = new_addr(complex_value);
	real_ptr = new_cast(real_addr, type_ptr(real_type));
	real_value = new_deref(real_ptr);
	real_value->type = clone_type(real_type);
	real_value->elem_size = type_sizeof(real_type);

	expr = new_binary(ND_COMMA, head, real_value);
	expr->type = clone_type(real_type);
	expr->elem_size = type_sizeof(real_type);

	if (!type_equal_unqualified(real_type, dst_type))
		expr = new_cast(expr, dst_type);

	return expr;
}

static Node *
expr_build_imaginary_real_extract_cast(Node *operand, Type *dst_type)
{
	Node *zero;
	Node *expr;

	if (!operand || !operand->type || !dst_type)
		return NULL;
	if (!type_is_imaginary(operand->type) || !expr_type_is_real_scalar_type(dst_type))
		return NULL;
	if (type_source_is_bool_spelling(dst_type))
		return expr_build_complex_bool_cast(operand, dst_type);

	zero = new_num(0);
	zero->type = type_int();
	zero->elem_size = TCC_SIZEOF_INT;
	if (!type_equal_unqualified(zero->type, dst_type))
		zero = new_cast(zero, dst_type);

	expr = new_binary(ND_COMMA, operand, zero);
	expr->type = clone_type(dst_type);
	expr->elem_size = type_sizeof(dst_type);
	return expr;
}

static Node *
expr_build_complex_imaginary_extract_cast(Node *operand, Type *dst_type)
{
	char temp_name[64];
	int offset;
	int object_size;
	int lane_size;
	Type *src_type;
	Type *src_real_type;
	Type *dst_real_type;
	Node *head;
	Node *complex_lhs;
	Node *complex_value;
	Node *assign;
	Node *imag_addr;
	Node *imag_ptr;
	Node *imag_value;
	Node *expr;

	if (!operand || !operand->type || !dst_type)
		return NULL;
	src_type = operand->type;
	if (!type_is_complex(src_type) || !type_is_imaginary(dst_type))
		return NULL;

	src_real_type = expr_complex_real_component_type(src_type);
	dst_real_type = expr_imaginary_real_component_type(dst_type);
	if (!src_real_type || !dst_real_type)
		return NULL;

	snprintf(temp_name, sizeof(temp_name), "__complex_imag_extract_%d",
	         parser_alloc_compound_arg_temp_id());
	offset = add_typed_local(temp_name, src_type);
	object_size = type_sizeof(src_type);
	lane_size = type_sizeof(src_real_type);

	complex_lhs = new_var(temp_name, offset);
	complex_lhs->type = clone_type(src_type);
	complex_lhs->elem_size = object_size;
	assign = new_struct_assign(complex_lhs, operand, object_size);
	head = new_block(assign);

	complex_value = new_var(temp_name, offset);
	complex_value->type = clone_type(src_type);
	complex_value->elem_size = object_size;

	imag_addr = new_addr(complex_value);
	imag_addr = new_binary(ND_ADD, imag_addr, new_num(lane_size));
	imag_ptr = new_cast(imag_addr, type_ptr(src_real_type));
	imag_value = new_deref(imag_ptr);
	imag_value->type = clone_type(src_real_type);
	imag_value->elem_size = lane_size;

	expr = new_binary(ND_COMMA, head, imag_value);
	expr->type = clone_type(src_real_type);
	expr->elem_size = lane_size;

	if (!type_equal_unqualified(src_real_type, dst_real_type))
		expr = new_cast(expr, dst_real_type);
	if (!type_equal_unqualified(expr->type, dst_type))
		expr = new_cast(expr, dst_type);

	return expr;
}

static Node *
expr_build_complex_complex_cast(Node *operand, Type *dst_type)
{
	char src_temp_name[64];
	char dst_temp_name[64];
	int src_offset;
	int dst_offset;
	int src_object_size;
	int dst_object_size;
	Type *src_type;
	Type *src_real_type;
	Type *dst_real_type;
	Node *head;
	Node *src_complex_lhs;
	Node *src_assign;
	Node *dst_zero_fill;
	Node *src_complex_value;
	Node *dst_complex_lhs;
	Node *dst_complex_value;
	Node *src_addr;
	Node *src_byte_ptr;
	Node *dst_addr;
	Node *dst_byte_ptr;
	Node *src_lane_addr;
	Node *dst_lane_addr;
	Node *src_lane_ptr;
	Node *dst_lane_ptr;
	Node *src_lane_value;
	Node *dst_lane_lhs;
	Node *lane_rhs;
	Node *store_lane;
	Node *expr;
	int src_lane_offset;
	int dst_lane_offset;

	if (!operand || !operand->type || !dst_type)
		return NULL;
	src_type = operand->type;
	if (!type_is_complex(src_type) || !type_is_complex(dst_type))
		return NULL;

	src_real_type = expr_complex_real_component_type(src_type);
	dst_real_type = expr_complex_real_component_type(dst_type);
	if (!src_real_type || !dst_real_type)
		fatal_cur("complex value casts are not supported yet\n");

	head = NULL;
	src_complex_value = operand;
	if (!expr_node_is_simple_lvalue(operand)) {
		snprintf(src_temp_name, sizeof(src_temp_name), "__complex_src_%d",
		         parser_alloc_compound_arg_temp_id());
		src_offset = add_typed_local(src_temp_name, src_type);
		src_object_size = type_sizeof(src_type);

		src_complex_lhs = new_var(src_temp_name, src_offset);
		src_complex_lhs->type = clone_type(src_type);
		src_complex_lhs->elem_size = src_object_size;
		src_assign = new_struct_assign(src_complex_lhs, operand, src_object_size);
		head = src_assign;

		src_complex_value = new_var(src_temp_name, src_offset);
		src_complex_value->type = clone_type(src_type);
		src_complex_value->elem_size = src_object_size;
	}

	snprintf(dst_temp_name, sizeof(dst_temp_name), "__complex_dst_%d",
	         parser_alloc_compound_arg_temp_id());
	dst_offset = add_typed_local(dst_temp_name, dst_type);
	dst_object_size = type_sizeof(dst_type);
	dst_zero_fill = append_local_zero_fill(NULL, dst_temp_name, dst_offset, dst_object_size);
	head = append_node(head, dst_zero_fill);

	dst_complex_lhs = new_var(dst_temp_name, dst_offset);
	dst_complex_lhs->type = clone_type(dst_type);
	dst_complex_lhs->elem_size = dst_object_size;

	src_addr = new_addr(src_complex_value);
	src_byte_ptr = new_cast(src_addr, type_ptr(type_char()));
	dst_addr = new_addr(dst_complex_lhs);
	dst_byte_ptr = new_cast(dst_addr, type_ptr(type_char()));

	for (int lane = 0; lane < 2; lane++) {
		src_lane_offset = lane * type_sizeof(src_real_type);
		dst_lane_offset = lane * type_sizeof(dst_real_type);
		src_lane_addr = clone_node_tree(src_byte_ptr);
		dst_lane_addr = clone_node_tree(dst_byte_ptr);
		if (src_lane_offset != 0)
			src_lane_addr = new_binary(ND_ADD, src_lane_addr, new_num(src_lane_offset));
		if (dst_lane_offset != 0)
			dst_lane_addr = new_binary(ND_ADD, dst_lane_addr, new_num(dst_lane_offset));
		src_lane_ptr = new_cast(src_lane_addr, type_ptr(src_real_type));
		dst_lane_ptr = new_cast(dst_lane_addr, type_ptr(dst_real_type));

		src_lane_value = new_deref(src_lane_ptr);
		src_lane_value->type = clone_type(src_real_type);
		src_lane_value->elem_size = type_sizeof(src_real_type);

		dst_lane_lhs = new_deref(dst_lane_ptr);
		dst_lane_lhs->type = clone_type(dst_real_type);
		dst_lane_lhs->elem_size = type_sizeof(dst_real_type);

		lane_rhs = src_lane_value;
		if (!type_equal_unqualified(src_real_type, dst_real_type))
			lane_rhs = new_cast(lane_rhs, dst_real_type);
		store_lane = new_assign(dst_lane_lhs, lane_rhs);
		head = append_node(head, store_lane);
	}

	dst_complex_value = new_var(dst_temp_name, dst_offset);
	dst_complex_value->type = clone_type(dst_type);
	dst_complex_value->elem_size = dst_object_size;

	expr = new_binary(ND_COMMA, new_block(head), dst_complex_value);
	expr->type = clone_type(dst_type);
	expr->elem_size = dst_object_size;
	return expr;
}

static Node *
expr_build_complex_additive(Node *left, Node *right, NodeKind kind)
{
	char left_temp_name[64];
	char right_temp_name[64];
	char dst_temp_name[64];
	Type *type;
	Type *real_type;
	Node *head = NULL;
	Node *left_value = left;
	Node *right_value = right;
	Node *dst_value;
	int left_offset = 0;
	int right_offset = 0;
	int dst_offset;
	int object_size;
	int lane_size;

	if (!left || !right || !left->type || !right->type)
		return NULL;
	if (!type_is_complex(left->type) || !type_is_complex(right->type))
		return NULL;
	if (!type_equal_unqualified(left->type, right->type))
		return NULL;
	if (!(kind == ND_ADD || kind == ND_SUB))
		return NULL;

	type = left->type;
	real_type = expr_complex_real_component_type(type);
	if (!real_type)
		fatal_cur("complex arithmetic is not supported yet\n");

	object_size = type_sizeof(type);
	lane_size = type_sizeof(real_type);

	left_value = expr_extract_addressable_temp(left, &head);
	if (!left_value && !expr_node_is_simple_lvalue(left)) {
		Node *left_lhs;
		Node *left_assign;

		snprintf(left_temp_name, sizeof(left_temp_name), "__complex_lhs_%d",
		         parser_alloc_compound_arg_temp_id());
		left_offset = add_typed_local(left_temp_name, type);
		left_lhs = new_var(left_temp_name, left_offset);
		left_lhs->type = clone_type(type);
		left_lhs->elem_size = object_size;
		left_assign = new_struct_assign(left_lhs, left, object_size);
		head = append_node(head, left_assign);

		left_value = new_var(left_temp_name, left_offset);
		left_value->type = clone_type(type);
		left_value->elem_size = object_size;
	}

	if (!left_value)
		left_value = left;

	right_value = expr_extract_addressable_temp(right, &head);
	if (!right_value && !expr_node_is_simple_lvalue(right)) {
		Node *right_lhs;
		Node *right_assign;

		snprintf(right_temp_name, sizeof(right_temp_name), "__complex_rhs_%d",
		         parser_alloc_compound_arg_temp_id());
		right_offset = add_typed_local(right_temp_name, type);
		right_lhs = new_var(right_temp_name, right_offset);
		right_lhs->type = clone_type(type);
		right_lhs->elem_size = object_size;
		right_assign = new_struct_assign(right_lhs, right, object_size);
		head = append_node(head, right_assign);

		right_value = new_var(right_temp_name, right_offset);
		right_value->type = clone_type(type);
		right_value->elem_size = object_size;
	}

	if (!right_value)
		right_value = right;

	snprintf(dst_temp_name, sizeof(dst_temp_name), "__complex_add_%d",
	         parser_alloc_compound_arg_temp_id());
	dst_offset = add_typed_local(dst_temp_name, type);
	head = append_node(head, append_local_zero_fill(NULL, dst_temp_name,
	                                                dst_offset, object_size));

	for (int lane = 0; lane < 2; lane++) {
		int lane_offset = lane * lane_size;
		Node *left_addr = new_addr(clone_node_tree(left_value));
		Node *right_addr = new_addr(clone_node_tree(right_value));
		Node *dst_addr = new_addr(new_var(dst_temp_name, dst_offset));
		Node *left_byte_ptr = new_cast(left_addr, type_ptr(type_uchar()));
		Node *right_byte_ptr = new_cast(right_addr, type_ptr(type_uchar()));
		Node *dst_byte_ptr = new_cast(dst_addr, type_ptr(type_uchar()));
		Node *left_lane_addr;
		Node *right_lane_addr;
		Node *dst_lane_addr;
		Node *left_lane_ptr;
		Node *right_lane_ptr;
		Node *dst_lane_ptr;
		Node *left_lane_value;
		Node *right_lane_value;
		Node *dst_lane_lhs;
		Node *lane_value;
		Node *store_lane;

		left_lane_addr = clone_node_tree(left_byte_ptr);
		right_lane_addr = clone_node_tree(right_byte_ptr);
		dst_lane_addr = clone_node_tree(dst_byte_ptr);
		if (lane_offset != 0) {
			left_lane_addr = new_binary(ND_ADD, left_lane_addr, new_num(lane_offset));
			right_lane_addr = new_binary(ND_ADD, right_lane_addr, new_num(lane_offset));
			dst_lane_addr = new_binary(ND_ADD, dst_lane_addr, new_num(lane_offset));
		}

		left_lane_ptr = new_cast(left_lane_addr, type_ptr(real_type));
		right_lane_ptr = new_cast(right_lane_addr, type_ptr(real_type));
		dst_lane_ptr = new_cast(dst_lane_addr, type_ptr(real_type));

		left_lane_value = new_deref(left_lane_ptr);
		left_lane_value->type = clone_type(real_type);
		left_lane_value->elem_size = lane_size;

		right_lane_value = new_deref(right_lane_ptr);
		right_lane_value->type = clone_type(real_type);
		right_lane_value->elem_size = lane_size;

		dst_lane_lhs = new_deref(dst_lane_ptr);
		dst_lane_lhs->type = clone_type(real_type);
		dst_lane_lhs->elem_size = lane_size;

		lane_value = new_binary(kind, left_lane_value, right_lane_value);
		store_lane = new_assign(dst_lane_lhs, lane_value);
		head = append_node(head, store_lane);
	}

	dst_value = new_var(dst_temp_name, dst_offset);
	dst_value->type = clone_type(type);
	dst_value->elem_size = object_size;

	{
		Node *expr = new_binary(ND_COMMA, new_block(head), dst_value);
		expr->type = clone_type(type);
		expr->elem_size = object_size;
		return expr;
	}
}

static Node *
expr_build_real_imag_additive(Node *left, Node *right, NodeKind kind)
{
	Type *left_real_type;
	Type *right_real_type;
	Type *result_real_type;
	Type *result_complex_type;
	Node *real_expr;
	Node *imag_expr;
	Node *head;
	Node *complex_lhs;
	Node *complex_value;
	Node *real_addr;
	Node *imag_addr;
	Node *real_ptr;
	Node *imag_ptr;
	Node *real_lhs;
	Node *imag_lhs;
	Node *store_real;
	Node *store_imag;
	Node *expr;
	char temp_name[64];
	int offset;
	int object_size;
	int lane_size;
	int left_is_real;
	int right_is_real;
	int left_is_imag;
	int right_is_imag;

	if (!left || !right || !left->type || !right->type)
		return NULL;
	if (!(kind == ND_ADD || kind == ND_SUB))
		return NULL;

	left_is_real = expr_node_is_real_scalar(left);
	right_is_real = expr_node_is_real_scalar(right);
	left_is_imag = expr_node_has_imaginary_value(left);
	right_is_imag = expr_node_has_imaginary_value(right);

	if (!((left_is_real && right_is_imag) || (left_is_imag && right_is_real)))
		return NULL;

	left_real_type = left_is_imag ? expr_imaginary_real_component_type(left->type)
	                              : left->type;
	right_real_type = right_is_imag ? expr_imaginary_real_component_type(right->type)
	                                : right->type;
	if (!left_real_type || !right_real_type)
		return NULL;

	result_real_type = expr_fp_usual_arith_type(left_real_type, right_real_type);
	if (!result_real_type || !type_is_fp_scalar(result_real_type))
		return NULL;
	result_complex_type = expr_make_complex_type_from_real(result_real_type);
	if (!result_complex_type)
		return NULL;

	if (left_is_real) {
		real_expr = left;
		if (!type_equal_unqualified(left->type, result_real_type))
			real_expr = new_cast(real_expr, result_real_type);

		imag_expr = new_cast(right, result_real_type);
		if (kind == ND_SUB)
			imag_expr = new_unary(ND_NEG, imag_expr);
	} else {
		real_expr = right;
		if (!type_equal_unqualified(right->type, result_real_type))
			real_expr = new_cast(real_expr, result_real_type);
		if (kind == ND_SUB)
			real_expr = new_unary(ND_NEG, real_expr);

		imag_expr = new_cast(left, result_real_type);
	}

	snprintf(temp_name, sizeof(temp_name), "__complex_mix_%d",
	         parser_alloc_compound_arg_temp_id());
	offset = add_typed_local(temp_name, result_complex_type);
	object_size = type_sizeof(result_complex_type);
	lane_size = type_sizeof(result_real_type);

	head = append_local_zero_fill(NULL, temp_name, offset, object_size);

	complex_lhs = new_var(temp_name, offset);
	complex_lhs->type = clone_type(result_complex_type);
	complex_lhs->elem_size = object_size;

	real_addr = new_addr(clone_node_tree(complex_lhs));
	real_ptr = new_cast(real_addr, type_ptr(result_real_type));
	real_lhs = new_deref(real_ptr);
	real_lhs->type = clone_type(result_real_type);
	real_lhs->elem_size = lane_size;
	store_real = new_assign(real_lhs, real_expr);
	head = append_node(head, store_real);

	imag_addr = new_addr(complex_lhs);
	imag_addr = new_binary(ND_ADD, imag_addr, new_num(lane_size));
	imag_ptr = new_cast(imag_addr, type_ptr(result_real_type));
	imag_lhs = new_deref(imag_ptr);
	imag_lhs->type = clone_type(result_real_type);
	imag_lhs->elem_size = lane_size;
	store_imag = new_assign(imag_lhs, imag_expr);
	head = append_node(head, store_imag);

	complex_value = new_var(temp_name, offset);
	complex_value->type = clone_type(result_complex_type);
	complex_value->elem_size = object_size;

	expr = new_binary(ND_COMMA, new_block(head), complex_value);
	expr->type = clone_type(result_complex_type);
	expr->elem_size = object_size;
	return expr;
}

static Node *
expr_build_complex_imag_additive(Node *left, Node *right, NodeKind kind)
{
	Type *complex_type;
	Type *imag_type;
	Type *complex_real_type;
	Type *imag_real_type;
	Type *result_real_type;
	Type *result_complex_type;
	Node *complex_expr;
	Node *imag_expr;
	Node *complex_value;
	Node *head = NULL;
	Node *complex_addr;
	Node *complex_byte_ptr;
	Node *real_addr;
	Node *imag_addr;
	Node *real_ptr;
	Node *imag_ptr;
	Node *real_src;
	Node *imag_src;
	Node *real_lhs;
	Node *imag_lhs;
	Node *store_real;
	Node *store_imag;
	Node *expr;
	char temp_name[64];
	int offset;
	int object_size;
	int lane_size;
	int left_complex;
	int right_complex;
	int left_imag;
	int right_imag;

	if (!left || !right || !left->type || !right->type)
		return NULL;
	if (!(kind == ND_ADD || kind == ND_SUB))
		return NULL;

	left_complex = expr_node_has_complex_value(left);
	right_complex = expr_node_has_complex_value(right);
	left_imag = expr_node_has_imaginary_value(left);
	right_imag = expr_node_has_imaginary_value(right);

	if (!((left_complex && right_imag) || (left_imag && right_complex)))
		return NULL;

	complex_type = left_complex ? left->type : right->type;
	imag_type = left_imag ? left->type : right->type;
	complex_real_type = expr_complex_real_component_type(complex_type);
	imag_real_type = expr_imaginary_real_component_type(imag_type);
	if (!complex_real_type || !imag_real_type)
		return NULL;

	result_real_type = expr_fp_usual_arith_type(complex_real_type, imag_real_type);
	if (!result_real_type || !type_is_fp_scalar(result_real_type))
		return NULL;
	result_complex_type = expr_make_complex_type_from_real(result_real_type);
	if (!result_complex_type)
		return NULL;

	complex_expr = left_complex ? left : right;
	if (!type_equal_unqualified(complex_type, result_complex_type))
		complex_expr = expr_build_complex_complex_cast(complex_expr, result_complex_type);

	complex_value = expr_extract_addressable_temp(complex_expr, &head);
	if (!complex_value && !expr_node_is_simple_lvalue(complex_expr)) {
		char complex_temp_name[64];
		int complex_offset;
		Node *complex_lhs;
		Node *complex_assign;

		snprintf(complex_temp_name, sizeof(complex_temp_name), "__complex_imag_src_%d",
		         parser_alloc_compound_arg_temp_id());
		complex_offset = add_typed_local(complex_temp_name, result_complex_type);
		complex_lhs = new_var(complex_temp_name, complex_offset);
		complex_lhs->type = clone_type(result_complex_type);
		complex_lhs->elem_size = type_sizeof(result_complex_type);
		complex_assign = new_struct_assign(complex_lhs, complex_expr,
		                                   type_sizeof(result_complex_type));
		head = append_node(head, complex_assign);

		complex_value = new_var(complex_temp_name, complex_offset);
		complex_value->type = clone_type(result_complex_type);
		complex_value->elem_size = type_sizeof(result_complex_type);
	}
	if (!complex_value)
		complex_value = complex_expr;

	imag_expr = left_imag ? left : right;
	if (!type_equal_unqualified(imag_expr->type, result_real_type))
		imag_expr = new_cast(imag_expr, result_real_type);

	snprintf(temp_name, sizeof(temp_name), "__complex_imag_%d",
	         parser_alloc_compound_arg_temp_id());
	offset = add_typed_local(temp_name, result_complex_type);
	object_size = type_sizeof(result_complex_type);
	lane_size = type_sizeof(result_real_type);

	head = append_node(head, append_local_zero_fill(NULL, temp_name, offset, object_size));

	complex_addr = new_addr(clone_node_tree(complex_value));
	complex_byte_ptr = new_cast(complex_addr, type_ptr(type_uchar()));

	real_addr = clone_node_tree(complex_byte_ptr);
	real_ptr = new_cast(real_addr, type_ptr(result_real_type));
	real_src = new_deref(real_ptr);
	real_src->type = clone_type(result_real_type);
	real_src->elem_size = lane_size;
	if (left_imag && right_complex && kind == ND_SUB)
		real_src = new_unary(ND_NEG, real_src);

	imag_addr = new_binary(ND_ADD, clone_node_tree(complex_byte_ptr), new_num(lane_size));
	imag_ptr = new_cast(imag_addr, type_ptr(result_real_type));
	imag_src = new_deref(imag_ptr);
	imag_src->type = clone_type(result_real_type);
	imag_src->elem_size = lane_size;
	if (left_complex && right_imag) {
		imag_src = new_binary(kind, imag_src, imag_expr);
	} else if (kind == ND_ADD) {
		imag_src = new_binary(ND_ADD, imag_expr, imag_src);
	} else {
		imag_src = new_binary(ND_SUB, imag_expr, imag_src);
	}

	real_addr = new_addr(new_var(temp_name, offset));
	real_ptr = new_cast(real_addr, type_ptr(result_real_type));
	real_lhs = new_deref(real_ptr);
	real_lhs->type = clone_type(result_real_type);
	real_lhs->elem_size = lane_size;
	store_real = new_assign(real_lhs, real_src);
	head = append_node(head, store_real);

	imag_addr = new_addr(new_var(temp_name, offset));
	imag_addr = new_binary(ND_ADD, imag_addr, new_num(lane_size));
	imag_ptr = new_cast(imag_addr, type_ptr(result_real_type));
	imag_lhs = new_deref(imag_ptr);
	imag_lhs->type = clone_type(result_real_type);
	imag_lhs->elem_size = lane_size;
	store_imag = new_assign(imag_lhs, imag_src);
	head = append_node(head, store_imag);

	complex_value = new_var(temp_name, offset);
	complex_value->type = clone_type(result_complex_type);
	complex_value->elem_size = object_size;

	expr = new_binary(ND_COMMA, new_block(head), complex_value);
	expr->type = clone_type(result_complex_type);
	expr->elem_size = object_size;
	return expr;
}

static Node *
expr_build_imaginary_additive(Node *left, Node *right, NodeKind kind)
{
	Type *left_real_type;
	Type *right_real_type;
	Type *result_real_type;
	Type *result_imag_type;
	Node *left_real;
	Node *right_real;
	Node *expr;

	if (!left || !right || !left->type || !right->type)
		return NULL;
	if (!(kind == ND_ADD || kind == ND_SUB))
		return NULL;
	if (!expr_node_has_imaginary_value(left) || !expr_node_has_imaginary_value(right))
		return NULL;

	left_real_type = expr_imaginary_real_component_type(left->type);
	right_real_type = expr_imaginary_real_component_type(right->type);
	if (!left_real_type || !right_real_type)
		return NULL;

	result_real_type = expr_fp_usual_arith_type(left_real_type, right_real_type);
	if (!result_real_type || !type_is_fp_scalar(result_real_type))
		return NULL;
	result_imag_type = expr_make_imaginary_type_from_real(result_real_type);
	if (!result_imag_type)
		return NULL;

	left_real = left;
	if (!type_equal_unqualified(left->type, left_real_type))
		left_real = new_cast(left, left_real_type);
	if (!type_equal_unqualified(left_real_type, result_real_type))
		left_real = new_cast(left_real, result_real_type);

	right_real = right;
	if (!type_equal_unqualified(right->type, right_real_type))
		right_real = new_cast(right, right_real_type);
	if (!type_equal_unqualified(right_real_type, result_real_type))
		right_real = new_cast(right_real, result_real_type);

	expr = new_binary(kind, left_real, right_real);
	return new_cast(expr, result_imag_type);
}

static Node *
expr_build_complex_unary_neg(Node *operand)
{
	char src_temp_name[64];
	char dst_temp_name[64];
	Type *type;
	Type *real_type;
	Node *head = NULL;
	Node *src_value = operand;
	Node *dst_value;
	int src_offset = 0;
	int dst_offset;
	int object_size;
	int lane_size;

	if (!operand || !operand->type || !type_is_complex(operand->type))
		return NULL;

	type = operand->type;
	real_type = expr_complex_real_component_type(type);
	if (!real_type)
		fatal_cur("complex arithmetic is not supported yet\n");

	object_size = type_sizeof(type);
	lane_size = type_sizeof(real_type);

	if (!expr_node_is_simple_lvalue(operand)) {
		Node *src_lhs;
		Node *src_assign;

		snprintf(src_temp_name, sizeof(src_temp_name), "__complex_neg_src_%d",
		         parser_alloc_compound_arg_temp_id());
		src_offset = add_typed_local(src_temp_name, type);
		src_lhs = new_var(src_temp_name, src_offset);
		src_lhs->type = clone_type(type);
		src_lhs->elem_size = object_size;
		src_assign = new_struct_assign(src_lhs, operand, object_size);
		head = append_node(head, src_assign);

		src_value = new_var(src_temp_name, src_offset);
		src_value->type = clone_type(type);
		src_value->elem_size = object_size;
	}

	snprintf(dst_temp_name, sizeof(dst_temp_name), "__complex_neg_dst_%d",
	         parser_alloc_compound_arg_temp_id());
	dst_offset = add_typed_local(dst_temp_name, type);
	head = append_node(head, append_local_zero_fill(NULL, dst_temp_name,
	                                                dst_offset, object_size));

	for (int lane = 0; lane < 2; lane++) {
		int lane_offset = lane * lane_size;
		Node *src_addr = new_addr(clone_node_tree(src_value));
		Node *dst_addr = new_addr(new_var(dst_temp_name, dst_offset));
		Node *src_byte_ptr = new_cast(src_addr, type_ptr(type_uchar()));
		Node *dst_byte_ptr = new_cast(dst_addr, type_ptr(type_uchar()));
		Node *src_lane_addr = clone_node_tree(src_byte_ptr);
		Node *dst_lane_addr = clone_node_tree(dst_byte_ptr);
		Node *src_lane_ptr;
		Node *dst_lane_ptr;
		Node *src_lane_value;
		Node *dst_lane_lhs;
		Node *lane_value;
		Node *store_lane;

		if (lane_offset != 0) {
			src_lane_addr = new_binary(ND_ADD, src_lane_addr, new_num(lane_offset));
			dst_lane_addr = new_binary(ND_ADD, dst_lane_addr, new_num(lane_offset));
		}

		src_lane_ptr = new_cast(src_lane_addr, type_ptr(real_type));
		dst_lane_ptr = new_cast(dst_lane_addr, type_ptr(real_type));

		src_lane_value = new_deref(src_lane_ptr);
		src_lane_value->type = clone_type(real_type);
		src_lane_value->elem_size = lane_size;

		dst_lane_lhs = new_deref(dst_lane_ptr);
		dst_lane_lhs->type = clone_type(real_type);
		dst_lane_lhs->elem_size = lane_size;

		lane_value = new_unary(ND_NEG, src_lane_value);
		store_lane = new_assign(dst_lane_lhs, lane_value);
		head = append_node(head, store_lane);
	}

	dst_value = new_var(dst_temp_name, dst_offset);
	dst_value->type = clone_type(type);
	dst_value->elem_size = object_size;

	{
		Node *expr = new_binary(ND_COMMA, new_block(head), dst_value);
		expr->type = clone_type(type);
		expr->elem_size = object_size;
		return expr;
	}
}

static Node *
expr_build_complex_multiplicative(Node *left, Node *right, NodeKind kind)
{
	char left_temp_name[64];
	char right_temp_name[64];
	char dst_temp_name[64];
	Type *type;
	Type *real_type;
	Node *head = NULL;
	Node *left_value = left;
	Node *right_value = right;
	Node *dst_value;
	int left_offset = 0;
	int right_offset = 0;
	int dst_offset;
	int object_size;
	int lane_size;
	Node *left_real_value;
	Node *left_imag_value;
	Node *right_real_value;
	Node *right_imag_value;
	Node *real_expr;
	Node *imag_expr;

	if (!left || !right || !left->type || !right->type)
		return NULL;
	if (!type_is_complex(left->type) || !type_is_complex(right->type))
		return NULL;
	if (!type_equal_unqualified(left->type, right->type))
		return NULL;
	if (!(kind == ND_MUL || kind == ND_DIV))
		return NULL;

	type = left->type;
	real_type = expr_complex_real_component_type(type);
	if (!real_type)
		fatal_cur("complex arithmetic is not supported yet\n");

	object_size = type_sizeof(type);
	lane_size = type_sizeof(real_type);

	left_value = expr_extract_addressable_temp(left, &head);
	if (!left_value && !expr_node_is_simple_lvalue(left)) {
		Node *left_lhs;
		Node *left_assign;

		snprintf(left_temp_name, sizeof(left_temp_name), "__complex_mul_lhs_%d",
		         parser_alloc_compound_arg_temp_id());
		left_offset = add_typed_local(left_temp_name, type);
		left_lhs = new_var(left_temp_name, left_offset);
		left_lhs->type = clone_type(type);
		left_lhs->elem_size = object_size;
		left_assign = new_struct_assign(left_lhs, left, object_size);
		head = append_node(head, left_assign);

		left_value = new_var(left_temp_name, left_offset);
		left_value->type = clone_type(type);
		left_value->elem_size = object_size;
	}

	if (!left_value)
		left_value = left;

	right_value = expr_extract_addressable_temp(right, &head);
	if (!right_value && !expr_node_is_simple_lvalue(right)) {
		Node *right_lhs;
		Node *right_assign;

		snprintf(right_temp_name, sizeof(right_temp_name), "__complex_mul_rhs_%d",
		         parser_alloc_compound_arg_temp_id());
		right_offset = add_typed_local(right_temp_name, type);
		right_lhs = new_var(right_temp_name, right_offset);
		right_lhs->type = clone_type(type);
		right_lhs->elem_size = object_size;
		right_assign = new_struct_assign(right_lhs, right, object_size);
		head = append_node(head, right_assign);

		right_value = new_var(right_temp_name, right_offset);
		right_value->type = clone_type(type);
		right_value->elem_size = object_size;
	}

	if (!right_value)
		right_value = right;

	snprintf(dst_temp_name, sizeof(dst_temp_name), "__complex_mul_dst_%d",
	         parser_alloc_compound_arg_temp_id());
	dst_offset = add_typed_local(dst_temp_name, type);
	head = append_node(head, append_local_zero_fill(NULL, dst_temp_name,
	                                                dst_offset, object_size));

	{
		Node *left_addr = new_addr(clone_node_tree(left_value));
		Node *right_addr = new_addr(clone_node_tree(right_value));
		Node *left_byte_ptr = new_cast(left_addr, type_ptr(type_uchar()));
		Node *right_byte_ptr = new_cast(right_addr, type_ptr(type_uchar()));
		Node *left_real_addr = clone_node_tree(left_byte_ptr);
		Node *left_imag_addr = new_binary(ND_ADD, clone_node_tree(left_byte_ptr),
		                                  new_num(lane_size));
		Node *right_real_addr = clone_node_tree(right_byte_ptr);
		Node *right_imag_addr = new_binary(ND_ADD, clone_node_tree(right_byte_ptr),
		                                   new_num(lane_size));
		Node *left_real_ptr = new_cast(left_real_addr, type_ptr(real_type));
		Node *left_imag_ptr = new_cast(left_imag_addr, type_ptr(real_type));
		Node *right_real_ptr = new_cast(right_real_addr, type_ptr(real_type));
		Node *right_imag_ptr = new_cast(right_imag_addr, type_ptr(real_type));

		left_real_value = new_deref(left_real_ptr);
		left_real_value->type = clone_type(real_type);
		left_real_value->elem_size = lane_size;

		left_imag_value = new_deref(left_imag_ptr);
		left_imag_value->type = clone_type(real_type);
		left_imag_value->elem_size = lane_size;

		right_real_value = new_deref(right_real_ptr);
		right_real_value->type = clone_type(real_type);
		right_real_value->elem_size = lane_size;

		right_imag_value = new_deref(right_imag_ptr);
		right_imag_value->type = clone_type(real_type);
		right_imag_value->elem_size = lane_size;
	}

	if (kind == ND_MUL) {
		real_expr = new_binary(
		    ND_SUB,
		    new_binary(ND_MUL, clone_node_tree(left_real_value),
		               clone_node_tree(right_real_value)),
		    new_binary(ND_MUL, clone_node_tree(left_imag_value),
		               clone_node_tree(right_imag_value)));
		imag_expr = new_binary(
		    ND_ADD,
		    new_binary(ND_MUL, clone_node_tree(left_real_value),
		               clone_node_tree(right_imag_value)),
		    new_binary(ND_MUL, clone_node_tree(left_imag_value),
		               clone_node_tree(right_real_value)));
	} else {
		Node *denom = new_binary(
		    ND_ADD,
		    new_binary(ND_MUL, clone_node_tree(right_real_value),
		               clone_node_tree(right_real_value)),
		    new_binary(ND_MUL, clone_node_tree(right_imag_value),
		               clone_node_tree(right_imag_value)));
		real_expr = new_binary(
		    ND_DIV,
		    new_binary(
		        ND_ADD,
		        new_binary(ND_MUL, clone_node_tree(left_real_value),
		                   clone_node_tree(right_real_value)),
		        new_binary(ND_MUL, clone_node_tree(left_imag_value),
		                   clone_node_tree(right_imag_value))),
		    clone_node_tree(denom));
		imag_expr = new_binary(
		    ND_DIV,
		    new_binary(
		        ND_SUB,
		        new_binary(ND_MUL, clone_node_tree(left_imag_value),
		                   clone_node_tree(right_real_value)),
		        new_binary(ND_MUL, clone_node_tree(left_real_value),
		                   clone_node_tree(right_imag_value))),
		    denom);
	}

	for (int lane = 0; lane < 2; lane++) {
		int lane_offset = lane * lane_size;
		Node *dst_addr = new_addr(new_var(dst_temp_name, dst_offset));
		Node *dst_byte_ptr = new_cast(dst_addr, type_ptr(type_uchar()));
		Node *dst_lane_addr = clone_node_tree(dst_byte_ptr);
		Node *dst_lane_ptr;
		Node *dst_lane_lhs;
		Node *store_lane;
		Node *lane_value = lane == 0 ? real_expr : imag_expr;

		if (lane_offset != 0)
			dst_lane_addr = new_binary(ND_ADD, dst_lane_addr, new_num(lane_offset));
		dst_lane_ptr = new_cast(dst_lane_addr, type_ptr(real_type));
		dst_lane_lhs = new_deref(dst_lane_ptr);
		dst_lane_lhs->type = clone_type(real_type);
		dst_lane_lhs->elem_size = lane_size;
		store_lane = new_assign(dst_lane_lhs, lane_value);
		head = append_node(head, store_lane);
	}

	dst_value = new_var(dst_temp_name, dst_offset);
	dst_value->type = clone_type(type);
	dst_value->elem_size = object_size;

	{
		Node *expr = new_binary(ND_COMMA, new_block(head), dst_value);
		expr->type = clone_type(type);
		expr->elem_size = object_size;
		return expr;
	}
}

static int
expr_integer_rank_for_generic(const Type *type)
{
	if (!type)
		return 3;
	if (type_source_is(type, TYPE_SOURCE_BOOL))
		return 0;
	if (type_source_is(type, TYPE_SOURCE_LLONG) ||
	    type_source_is(type, TYPE_SOURCE_ULLONG))
		return 5;
	if (type_source_is(type, TYPE_SOURCE_LONG) ||
	    type_source_is(type, TYPE_SOURCE_ULONG))
		return 4;
	if (type->kind == TY_CHAR)
		return 1;
	if (type->kind == TY_SHORT)
		return 2;
	return 3;
}

static Type *
expr_generic_sig_type(int kind, int size, int is_unsigned, int qualifiers,
                      const char *struct_name)
{
	Type *type = NULL;

	switch (kind) {
	case TY_VOID:
		type = type_void();
		break;
	case TY_CHAR:
		type = is_unsigned ? type_uchar() : type_char();
		break;
	case TY_SHORT:
		type = is_unsigned ? type_ushort() : type_short();
		break;
	case TY_FLOAT:
		type = type_float();
		break;
	case TY_DOUBLE:
		type = type_double();
		break;
	case TY_STRUCT:
		type = type_struct(struct_name ? struct_name : "", size);
		break;
	case TY_UNION:
		type = type_union(struct_name ? struct_name : "", size);
		break;
	case TY_ENUM:
		type = type_enum(struct_name ? struct_name : "");
		break;
	default:
		type = type_for_size_unsigned(size ? size : TCC_SIZEOF_INT, is_unsigned);
		break;
	}

	if (qualifiers)
		type = type_with_qualifiers(type, qualifiers);
	return type;
}

static Type *
expr_generic_assoc_base_type(int kind, int size, int is_unsigned,
                             int qualifiers, const char *struct_name,
                             int source_kind)
{
	Type *type = expr_generic_sig_type(kind, size, is_unsigned,
	                                   qualifiers, struct_name);

	if (source_kind == TYPE_SOURCE_BOOL)
		type = type_with_source(type_uchar(), TYPE_SOURCE_BOOL, "_Bool");
	else if (source_kind == TYPE_SOURCE_SCHAR)
		type = type_with_source(type, TYPE_SOURCE_SCHAR, "signed char");
	else if (source_kind == TYPE_SOURCE_LONG)
		type = type_with_source(type, TYPE_SOURCE_LONG, "long");
	else if (source_kind == TYPE_SOURCE_ULONG)
		type = type_with_source(type, TYPE_SOURCE_ULONG, "unsigned long");
	else if (source_kind == TYPE_SOURCE_LLONG)
		type = type_with_source(type, TYPE_SOURCE_LLONG, "long long");
	else if (source_kind == TYPE_SOURCE_ULLONG)
		type = type_with_source(type, TYPE_SOURCE_ULLONG, "unsigned long long");
	else if (source_kind == TYPE_SOURCE_LONG_DOUBLE)
		type = type_with_source(type_double(), TYPE_SOURCE_LONG_DOUBLE,
		                        "long double");
	else if (source_kind == TYPE_SOURCE_COMPLEX)
		type = type_with_source(type, TYPE_SOURCE_COMPLEX,
		                        kind == TY_FLOAT ? "_Complex float"
		                                         : "_Complex double");
	else if (source_kind == TYPE_SOURCE_IMAGINARY)
		type = type_with_source(type, TYPE_SOURCE_IMAGINARY,
		                        kind == TY_FLOAT ? "_Imaginary float"
		                                         : "_Imaginary double");
	return type;
}

static int
expr_generic_source_kind_matches(int kind, int lhs_source_kind, int rhs_source_kind)
{
	if (lhs_source_kind == TYPE_SOURCE_BOOL || rhs_source_kind == TYPE_SOURCE_BOOL)
		return lhs_source_kind == rhs_source_kind;
	if (lhs_source_kind == TYPE_SOURCE_LONG || rhs_source_kind == TYPE_SOURCE_LONG)
		return lhs_source_kind == rhs_source_kind;
	if (lhs_source_kind == TYPE_SOURCE_ULONG || rhs_source_kind == TYPE_SOURCE_ULONG)
		return lhs_source_kind == rhs_source_kind;
	if (lhs_source_kind == TYPE_SOURCE_LLONG || rhs_source_kind == TYPE_SOURCE_LLONG)
		return lhs_source_kind == rhs_source_kind;
	if (lhs_source_kind == TYPE_SOURCE_ULLONG || rhs_source_kind == TYPE_SOURCE_ULLONG)
		return lhs_source_kind == rhs_source_kind;
	if (lhs_source_kind == TYPE_SOURCE_COMPLEX || rhs_source_kind == TYPE_SOURCE_COMPLEX)
		return lhs_source_kind == rhs_source_kind;
	if (lhs_source_kind == TYPE_SOURCE_IMAGINARY || rhs_source_kind == TYPE_SOURCE_IMAGINARY)
		return lhs_source_kind == rhs_source_kind;
	if (lhs_source_kind == TYPE_SOURCE_LONG_DOUBLE || rhs_source_kind == TYPE_SOURCE_LONG_DOUBLE)
		return lhs_source_kind == rhs_source_kind;
	if (kind == TY_CHAR)
		return lhs_source_kind == rhs_source_kind;
	return 1;
}

static int
expr_generic_types_compatible(const Type *a, const Type *b, int ignore_top_qualifiers)
{
	if (a == b)
		return 1;
	if (!a || !b)
		return 0;
	if (!ignore_top_qualifiers && type_qualifiers(a) != type_qualifiers(b))
		return 0;
	if (type_is_complex(a) || type_is_complex(b) ||
	    type_is_imaginary(a) || type_is_imaginary(b)) {
		if (type_source_kind(a) != type_source_kind(b))
			return 0;
	}
	if (type_is_struct(a) || type_is_union(a) || type_is_enum(a) ||
	    type_is_struct(b) || type_is_union(b) || type_is_enum(b)) {
		if (a->kind != b->kind)
			return 0;
		return STRCMP(a->struct_name, b->struct_name) == 0;
	}
	if (a->kind != b->kind)
		return 0;
	if (a->size != b->size ||
	    a->is_unsigned != b->is_unsigned ||
	    a->array_len != b->array_len)
		return 0;
	if (a->kind == TY_CHAR &&
	    type_source_is(a, TYPE_SOURCE_SCHAR) != type_source_is(b, TYPE_SOURCE_SCHAR))
		return 0;
	if (a->kind == TY_INT || a->kind == TY_CHAR || a->kind == TY_SHORT) {
		int a_source_kind = type_source_kind(a);
		int b_source_kind = type_source_kind(b);

		if (a_source_kind == TYPE_SOURCE_TYPEDEF)
			a_source_kind = TYPE_SOURCE_DEFAULT;
		if (b_source_kind == TYPE_SOURCE_TYPEDEF)
			b_source_kind = TYPE_SOURCE_DEFAULT;
		if (a_source_kind != b_source_kind) {
			switch (a_source_kind == TYPE_SOURCE_DEFAULT ? b_source_kind : a_source_kind) {
			case TYPE_SOURCE_LONG:
			case TYPE_SOURCE_ULONG:
			case TYPE_SOURCE_LLONG:
			case TYPE_SOURCE_ULLONG:
			case TYPE_SOURCE_BOOL:
				return 0;
			default:
				break;
			}
		}
	}
	if (a->kind == TY_DOUBLE &&
	    type_source_is(a, TYPE_SOURCE_LONG_DOUBLE) !=
	        type_source_is(b, TYPE_SOURCE_LONG_DOUBLE))
		return 0;
	if ((type_is_complex(a) || type_is_imaginary(a)) &&
	    (type_source_name(a)[0] || type_source_name(b)[0])) {
		int a_is_long_double = strstr(type_source_name(a), "long double") != NULL;
		int b_is_long_double = strstr(type_source_name(b), "long double") != NULL;
		if (a_is_long_double != b_is_long_double)
			return 0;
	}
	if (type_is_function(a))
		return type_function_compatible_qualified(a, b);
	if (type_is_pointer(a) || type_is_array(a))
		return expr_generic_types_compatible(type_pointee(a), type_pointee(b), 0);
	return 1;
}

static int
expr_generic_type_has_char_leaf(const Type *type)
{
	Type **params = NULL;
	int count = 0;

	if (!type)
		return 0;
	if (type->kind == TY_CHAR)
		return 1;
	if (type_is_function(type)) {
		if (expr_generic_type_has_char_leaf(type_pointee(type)))
			return 1;
		if (type_func_metadata(type, &params, &count, NULL, NULL)) {
			for (int i = 0; i < count; i++) {
				if (expr_generic_type_has_char_leaf(params[i]))
					return 1;
			}
		}
		return 0;
	}
	if (type_is_pointer(type) || type_is_array(type))
		return expr_generic_type_has_char_leaf(type_pointee(type));
	return 0;
}

static int
expr_is_explicit_function_pointer_cast(const Node *rhs)
{
	FuncInfo *fi;
	Type *declared_type;

	if (!rhs || rhs->kind != ND_FUNC_ADDR || !rhs->name[0] ||
	    !rhs->type || !type_is_pointer(rhs->type))
		return 0;

	fi = find_func(rhs->name);
	if (!fi || !fi->return_type)
		return 0;

	if (fi->has_prototype) {
		declared_type = type_ptr(parser_make_function_type(fi->return_type,
		                                                   fi->param_types,
		                                                   fi->param_type_count,
		                                                   fi->is_variadic,
		                                                   fi->fixed_param_count));
	} else {
		declared_type = type_ptr(type_func(clone_type(fi->return_type)));
	}

	return !type_equal_qualified(rhs->type, declared_type);
}

static int
expr_type_is_const_qualified(const Type *type)
{
	return type && type_has_qualifier(type, TYPE_QUAL_CONST);
}

static const char *
expr_resolve_struct_type_name(const Type *type)
{
	if (!type || !type_is_struct(type))
		return "";
	return parser_resolve_struct_type_name((Type *)type);
}

static const char *
expr_generic_tag_name(const Type *type)
{
	if (!type)
		return "";
	if (type_is_enum(type))
		return type->struct_name;
	if (type_is_union(type))
		return type->struct_name;
	if (type_is_struct(type))
		return parser_resolve_struct_type_name((Type *)type);
	return "";
}

static void
expr_apply_call_result_type(Node *call, Type *ret_type)
{
	Type *base;
	Type *effective_type;

	if (!call || !ret_type)
		return;

	effective_type = type_is_pointer(ret_type)
	               ? parser_canonicalize_pointer_type(ret_type, 0, "")
	               : ret_type;

	if (type_is_pointer(effective_type)) {
		Type *effective_base = type_pointee(effective_type);
		const char *effective_struct_name = "";
		if (effective_base && (type_is_struct(effective_base) || type_is_union(effective_base)))
			effective_struct_name = parser_resolve_struct_type_name(effective_base);
		call->type = parser_canonicalize_pointer_type(effective_type,
		                                             effective_base ? effective_base->size : 0,
		                                             effective_struct_name);
	} else {
		call->type = clone_type(effective_type);
	}
	call->is_unsigned = type_is_unsigned(effective_type);
	call->is_pointer = type_is_pointer(effective_type);

	if (type_is_struct(effective_type)) {
		int reg_count = 0;

		call->returns_struct = 1;
		call->aggregate_abi_class = parser_classify_aggregate_abi(effective_type,
		                                                          &reg_count);
		call->aggregate_abi_reg_count = reg_count;
		call->struct_return_size = effective_type->size;
		call->elem_size = effective_type->size;
		STRNCPY(call->return_struct_name, expr_resolve_struct_type_name(effective_type),
		        sizeof(call->return_struct_name) - 1);
		return;
	}
	if (parser_arm64_hfa_info_type(effective_type, NULL, &call->aggregate_abi_reg_count) &&
	    !type_is_struct(effective_type)) {
		call->returns_struct = 1;
		call->aggregate_abi_class = AGGREGATE_ABI_HFA;
		call->struct_return_size = type_sizeof(effective_type);
		call->elem_size = call->struct_return_size;
		if (type_sizeof(effective_type) == 8)
			STRNCPY(call->return_struct_name, "__tcc_hfa_complex_float2",
			        sizeof(call->return_struct_name) - 1);
		else if (type_sizeof(effective_type) == 16)
			STRNCPY(call->return_struct_name, "__tcc_hfa_complex_double2",
			        sizeof(call->return_struct_name) - 1);
		return;
	}

	if (type_is_pointer(effective_type)) {
		base = type_pointee(effective_type);
		call->elem_size = base && base->size ? base->size : TCC_SIZEOF_PTR;
		if (base && type_is_struct(base))
			STRNCPY(call->struct_name, expr_resolve_struct_type_name(base),
			        sizeof(call->struct_name) - 1);
		return;
	}

	call->elem_size = effective_type->size ? effective_type->size : TCC_SIZEOF_INT;
}

static int
expr_global_is_pointer_like(const Global *global)
{
	return global &&
	       !global->is_array &&
	       !global->is_struct &&
	       ((global->type && type_is_pointer(global->type)) ||
	        global->ptr_elem_size > 0 ||
	        global->is_string);
}

static Node *
make_member_read_from_struct_temp(Node *temp, const char *struct_name);

static Node *
mark_size_t_result(Node *node)
{
	if (!node)
		return NULL;
	node->type = type_ulong();
	node->is_unsigned = 1;
	node->is_pointer = 0;
	node->elem_size = type_ulong()->size;
	return node;
}

static Node *
new_size_t_num(long value)
{
	return mark_size_t_result(new_num_long(value));
}

static Node *
build_runtime_vm_array_sizeof_type_expr(const Type *type)
{
	Type *elem_type;
	Node *len_expr;
	int elem_size;

	if (!type || type->kind != TY_ARRAY || !type->is_vm_type || !type->vla_bound_name[0])
		return NULL;

	elem_type = type->vla_elem_type ? type->vla_elem_type : type->base;
	elem_size = type_sizeof(elem_type);
	if (elem_size <= 0)
		return NULL;

	len_expr = make_scalar_var_node(type->vla_bound_name);
	return mark_size_t_result(new_binary(ND_MUL, len_expr, new_size_t_num(elem_size)));
}

static Node *
build_runtime_vla_sizeof_expr(const char *name)
{
	Node *len_expr;
	Type *elem_type;
	int elem_size;
	const char *bound_name;

	if (!name || !is_vla_local(name))
		return NULL;

	bound_name = vla_bound_name_local(name);
	elem_type = vla_elem_type_local(name);
	elem_size = elem_size_local(name);

	len_expr = make_scalar_var_node(bound_name);
	return mark_size_t_result(new_binary(ND_MUL, len_expr,
	                                     new_size_t_num(elem_type ? elem_type->size : elem_size)));
}

static Type *
build_local_vla_array_pointer_type(const char *name)
{
	Type *elem_type;
	Type *array_type;
	const char *bound_name;

	if (!name || !is_vla_local(name))
		return NULL;

	elem_type = vla_elem_type_local(name);
	bound_name = vla_bound_name_local(name);
	if (!elem_type || !bound_name || !bound_name[0])
		return NULL;

	array_type = type_array(clone_type(elem_type), 0);
	array_type->is_vm_type = 1;
	STRNCPY(array_type->vla_bound_name, bound_name,
	        sizeof(array_type->vla_bound_name) - 1);
	array_type->vla_elem_type = clone_type(elem_type);
	return type_ptr(array_type);
}

static Type *
expr_vm_pointer_array_type(Type *type)
{
	Type *base;

	if (!type || !type_is_pointer(type))
		return NULL;
	base = type_pointee(type);
	if (!base || base->kind != TY_ARRAY || !base->is_vm_type || !base->vla_bound_name[0])
		return NULL;
	return base;
}

static Node *
expr_build_vm_stride_expr(Type *ptr_type)
{
	Type *vm_array;
	Type *elem_type;
	Node *bound_expr;
	int elem_size;

	vm_array = expr_vm_pointer_array_type(ptr_type);
	if (!vm_array)
		return NULL;

	elem_type = vm_array->vla_elem_type ? vm_array->vla_elem_type : vm_array->base;
	elem_size = type_sizeof(elem_type);
	if (elem_size <= 0)
		return NULL;

	bound_expr = make_scalar_var_node(vm_array->vla_bound_name);
	return mark_size_t_result(new_binary(ND_MUL, bound_expr, new_size_t_num(elem_size)));
}

static Node *
expr_build_vm_pointer_arith(NodeKind kind, Node *ptr, Node *index)
{
	Node *stride_expr;
	Node *scaled_index;
	Node *byte_ptr;
	Node *addr;
	Node *result;

	if (!ptr || !index || !(kind == ND_ADD || kind == ND_SUB))
		return NULL;

	stride_expr = expr_build_vm_stride_expr(ptr->type);
	if (!stride_expr)
		return NULL;

	scaled_index = new_binary(ND_MUL, index, stride_expr);
	byte_ptr = new_cast(ptr, type_ptr(type_char()));
	byte_ptr->is_pointer = 1;
	byte_ptr->elem_size = 1;
	addr = new_binary(kind, byte_ptr, scaled_index);
	addr->is_pointer = 1;
	addr->elem_size = 1;
	addr->type = type_ptr(type_char());
	result = new_cast(addr, ptr->type);
	result->is_pointer = 1;
	result->type = ptr->type;
	result->elem_size = ptr->elem_size ? ptr->elem_size : TCC_SIZEOF_PTR;
	return result;
}

Node *
parse_statement_expression(void)
{
	Node head = {0};
	Node *cur = &head;
	Node *block;
	Node *tail;

	/* GNU C statement expression: ({ statements; expr; })
	 * The final expression statement leaves its value in the accumulator when
	 * the block is emitted as an expression.  This is enough for constructs such
	 * as:
	 *
	 *     int x = ({ int y = 1; y; });
	 *
	 * and for statement expressions in the inactive arm of ?: expressions. */
	expect(TOK_LPAREN);
	expect(TOK_LBRACE);

	while (lexer_peek()->kind != TOK_RBRACE) {
		Node *stmt;

		if (lexer_peek()->kind == TOK_EOF) {
			fatal_cur("Unterminated statement expression\n");
		}
		stmt = parse_statement();
		cur->next = stmt;
		cur = stmt;
	}

	expect(TOK_RBRACE);
	expect(TOK_RPAREN);

	block = new_block(head.next);
	tail = cur != &head ? cur : NULL;
	if (tail &&
	    tail->type &&
	    !type_is_void(tail->type) &&
	    tail->kind != ND_DECL &&
	    tail->kind != ND_ARRAY_DECL &&
	    tail->kind != ND_PTR_DECL &&
	    tail->kind != ND_STRUCT_DECL &&
	    tail->kind != ND_RETURN &&
	    tail->kind != ND_LABEL &&
	    tail->kind != ND_GOTO &&
	    tail->kind != ND_IF &&
	    tail->kind != ND_WHILE &&
	    tail->kind != ND_FOR &&
	    tail->kind != ND_DO_WHILE &&
	    tail->kind != ND_SWITCH &&
	    tail->kind != ND_CASE &&
	    tail->kind != ND_DEFAULT &&
	    tail->kind != ND_BREAK &&
	    tail->kind != ND_CONTINUE &&
	    tail->kind != ND_BLOCK &&
	    tail->kind != ND_FUNC &&
	    tail->kind != ND_ASM) {
		block->type = tail->type;
		block->is_unsigned = tail->is_unsigned;
		block->is_pointer = tail->is_pointer;
		block->elem_size = tail->elem_size ? tail->elem_size
		                                   : type_sizeof(tail->type);
	} else {
		block->type = type_void();
		block->elem_size = 0;
	}
	return block;
}


Node *parse_generic_selection(void)
{
	/* _Generic(controlling-expression, type1: val1, ..., default: valdn)
	 * C11 generic selection. We parse and do simple type matching. */
	expect(TOK_LPAREN);
	Node *ctrl = parse_expr();
	expect(TOK_COMMA);

	/* Determine the type of the controlling expression */
	int ctrl_is_ptr = 0;    /* is a pointer type */
	int ctrl_size = 4;      /* size in bytes */
	int ctrl_is_str = 0;    /* string literal (const char *) */
	int ctrl_is_long = 0;   /* long int */
	int ctrl_is_unsigned = 0;
	int ctrl_rank = 3;
	int ctrl_kind = TY_INT;
	int ctrl_source_kind = TYPE_SOURCE_DEFAULT;
	Type *ctrl_type = NULL;
	char ctrl_struct[64] = {0};  /* struct name if struct */

	if (ctrl) {
		if (ctrl->kind == ND_STRING) { ctrl_is_ptr=1; ctrl_is_str=1; ctrl_size=8; ctrl_kind=TY_PTR; }
		else if (ctrl->type) {
			Type *t = expr_pointer_context_type(ctrl->type);
			ctrl_type = t;
			ctrl_kind = t->kind;
			ctrl_source_kind = type_source_kind(t);
			if (t->kind == TY_PTR) { ctrl_is_ptr=1; ctrl_size=8; }
			else if (type_is_struct(t) || type_is_union(t) || type_is_enum(t)) {
				ctrl_size = t->size;
				STRNCPY(ctrl_struct, expr_generic_tag_name(t), sizeof(ctrl_struct) - 1);
			}
			else if (type_is_floating(t)) {
				ctrl_size = t->size ? t->size : 8;
			}
			else { ctrl_size=t->size ? t->size : 4; ctrl_is_unsigned=t->is_unsigned; ctrl_rank = expr_integer_rank_for_generic(t); }
			ctrl_is_long = (t->size == 8 && t->kind != TY_PTR && !type_is_struct(t));
		} else {
			ctrl_size = ctrl->elem_size ? ctrl->elem_size : 4;
			ctrl_is_ptr = ctrl->is_pointer;
			ctrl_is_long = (ctrl_size == 8 && !ctrl_is_ptr);
		}
	}
	if (!ctrl_type && ctrl && ctrl->type)
		ctrl_type = expr_pointer_context_type(ctrl->type);
	(void)ctrl_is_str;

	Node *result = NULL;
	Node *default_result = NULL;
	int matched = 0;
	int saw_default = 0;
	int assoc_count = 0;
	struct {
		int is_ptr;
		int kind;
		int size;
		int rank;
		int is_unsigned;
		int is_const;
		int source_kind;
		int has_top_ptr_qualifiers;
		Type *type;
		char struct_name[64];
	} assoc_types[128];

	while (lexer_peek()->kind != TOK_RPAREN && lexer_peek()->kind != TOK_EOF) {
		/* Parse type specifier or "default" */
		int is_default = 0;
		int arm_match = 0;
		int arm_is_ptr = 0;
		int arm_size = 4;
		int arm_is_unsigned = 0;
		int arm_rank = 3;
		int arm_kind = TY_INT;
		int arm_base_qualifiers = 0;
		int arm_source_kind = TYPE_SOURCE_DEFAULT;
		Type *arm_type = NULL;
		char arm_struct[64] = {0};

		if (lexer_peek()->kind == TOK_DEFAULT ||
		    (lexer_peek()->kind == TOK_IDENT &&
		     lexer_peek()->text && STRCMP(lexer_peek()->text, "default") == 0)) {
			lexer_next();
			is_default = 1;
			if (saw_default)
				fatal_cur("duplicate default association in generic selection\n");
			saw_default = 1;
		} else {
			/* Parse type specifier to match against ctrl */
			/* We do a lightweight parse: consume qualifiers + base type + modifiers */
			int arm_is_const = 0, arm_is_long = 0, arm_is_arr = 0;
			int arm_has_top_ptr_qualifiers = 0;
			int arm_saw_sign_specifier = 0;
			int arm_saw_signed_specifier = 0;
			int arm_saw_base_type = 0;
			int arm_special_atomic_specifier = 0;

			if (lexer_peek()->kind == TOK_ATOMIC &&
			    lexer_peek_ahead(1)->kind == TOK_LPAREN) {
				reject_c89_c99_keyword_token(lexer_peek()->kind);
				lexer_next();
				expect(TOK_LPAREN);
				arm_type = parse_type_name();
				expect(TOK_RPAREN);
				if (arm_type && arm_type->kind == TY_ARRAY)
					fatal_cur("atomic type specifier cannot be applied to array type\n");
				if (arm_type && arm_type->kind == TY_FUNC)
					fatal_cur("atomic type specifier cannot be applied to function type\n");
				if (arm_type && (type_qualifiers(arm_type) & TYPE_QUAL_ATOMIC))
					fatal_cur("atomic type specifier cannot be applied to atomic type\n");
				arm_type = type_with_qualifiers(arm_type,
				                               type_qualifiers(arm_type) | TYPE_QUAL_ATOMIC);
				arm_is_ptr = type_is_pointer(arm_type) ? 1 : 0;
				arm_kind = arm_type->kind;
				arm_size = arm_type->size ? arm_type->size
				         : (arm_is_ptr ? TCC_SIZEOF_PTR : 4);
				arm_is_unsigned = type_is_unsigned(arm_type);
				arm_rank = expr_integer_rank_for_generic(arm_type);
				arm_source_kind = type_source_kind(arm_type);
				if (type_is_struct(arm_type) || type_is_union(arm_type) ||
				    type_is_enum(arm_type))
					STRNCPY(arm_struct, expr_generic_tag_name(arm_type),
					        sizeof(arm_struct) - 1);
				arm_saw_base_type = 1;
				arm_special_atomic_specifier = 1;
			}

			/* Consume qualifiers and sign specifiers */
			if (!arm_special_atomic_specifier) {
				while (lexer_peek()->kind == TOK_CONST ||
				       lexer_peek()->kind == TOK_VOLATILE ||
				       lexer_peek()->kind == TOK_RESTRICT ||
				       lexer_peek()->kind == TOK_ATOMIC ||
				       lexer_peek()->kind == TOK_UNSIGNED ||
				       lexer_peek()->kind == TOK_SIGNED ||
				       (lexer_peek()->kind == TOK_IDENT && lexer_peek()->text &&
				        STRCMP(lexer_peek()->text, "volatile") == 0)) {
					reject_c89_c99_keyword_token(lexer_peek()->kind);
					if (lexer_peek()->kind == TOK_CONST) { arm_is_const=1; arm_base_qualifiers |= TYPE_QUAL_CONST; }
					if (lexer_peek()->kind == TOK_VOLATILE ||
					    (lexer_peek()->kind == TOK_IDENT && lexer_peek()->text &&
					     STRCMP(lexer_peek()->text, "volatile") == 0))
						arm_base_qualifiers |= TYPE_QUAL_VOLATILE;
					if (lexer_peek()->kind == TOK_RESTRICT) arm_base_qualifiers |= TYPE_QUAL_RESTRICT;
					if (lexer_peek()->kind == TOK_ATOMIC) arm_base_qualifiers |= TYPE_QUAL_ATOMIC;
					if (lexer_peek()->kind == TOK_UNSIGNED) { arm_is_unsigned=1; arm_saw_sign_specifier=1; }
					if (lexer_peek()->kind == TOK_SIGNED) { arm_saw_sign_specifier=1; arm_saw_signed_specifier=1; }
					lexer_next();
				}
			}

			/* Base type */
			if (arm_special_atomic_specifier) { /* already parsed */ }
			else if (expr_special_type_token_source_kind(lexer_peek()) != TYPE_SOURCE_DEFAULT) {
				arm_type = parser_canonicalize_decl_type(parse_type_name());
				arm_size = arm_type && arm_type->size ? arm_type->size
				         : (type_is_pointer(arm_type) ? TCC_SIZEOF_PTR : 4);
				arm_is_unsigned = type_is_unsigned(arm_type);
				arm_rank = expr_integer_rank_for_generic(arm_type);
				arm_is_ptr = type_is_pointer(arm_type) ? 1 : 0;
				arm_kind = arm_type ? arm_type->kind : TY_INT;
				arm_source_kind = type_source_kind(arm_type);
				if (arm_type &&
				    (type_is_struct(arm_type) || type_is_union(arm_type) ||
				     type_is_enum(arm_type)))
					STRNCPY(arm_struct, expr_generic_tag_name(arm_type),
					        sizeof(arm_struct) - 1);
				arm_saw_base_type = 1;
				goto generic_assoc_type_parsed;
			}
			else if (lexer_peek()->kind == TOK_VOID) { arm_kind=TY_VOID; arm_size=0; arm_rank=0; arm_saw_base_type=1; lexer_next(); }
			else if (lexer_peek()->kind == TOK_INT) { arm_kind=TY_INT; arm_size=4; arm_rank=3; arm_saw_base_type=1; lexer_next(); }
			else if (lexer_peek()->kind == TOK_BOOL) {
				reject_c89_c99_keyword_token(lexer_peek()->kind);
				arm_kind=TY_CHAR;
				arm_size=1;
				arm_rank=0;
				arm_is_unsigned=1;
				arm_source_kind = TYPE_SOURCE_BOOL;
				arm_saw_base_type=1;
				lexer_next();
			}
			else if (lexer_peek()->kind == TOK_CHAR) {
				arm_kind=TY_CHAR; arm_size=1; arm_rank=1; arm_saw_base_type=1;
				if (arm_saw_signed_specifier && !arm_is_unsigned)
					arm_source_kind = TYPE_SOURCE_SCHAR;
				lexer_next();
			}
			else if (lexer_peek()->kind == TOK_LONG) {
				arm_kind=TY_INT;
				arm_size=8; arm_rank=4; arm_is_long=1; lexer_next();
				arm_saw_base_type=1;
				if (lexer_peek()->kind == TOK_DOUBLE) {
					lexer_next();
					arm_kind = TY_DOUBLE;
					arm_size = 8;
					arm_rank = 0;
					arm_is_unsigned = 0;
					arm_source_kind = TYPE_SOURCE_LONG_DOUBLE;
				}
				if (lexer_peek()->kind == TOK_LONG) {
					if (tcc_lang_is_c89_or_c90())
						fatal_cur("long long is not allowed in C89/C90 mode\n");
					lexer_next(); arm_size=8; arm_rank=5;
					if (arm_kind != TY_DOUBLE)
						arm_source_kind = arm_is_unsigned ? TYPE_SOURCE_ULLONG
						                                  : TYPE_SOURCE_LLONG;
				} else if (arm_kind != TY_DOUBLE) {
					arm_source_kind = arm_is_unsigned ? TYPE_SOURCE_ULONG
					                                  : TYPE_SOURCE_LONG;
				} /* long long */
				if (arm_kind != TY_DOUBLE && lexer_peek()->kind == TOK_INT) lexer_next();
			} else if (lexer_peek()->kind == TOK_SHORT) { arm_kind=TY_SHORT; arm_size=2; arm_rank=2; arm_saw_base_type=1; lexer_next();
				if (lexer_peek()->kind == TOK_INT) lexer_next();
			} else if (lexer_peek()->kind == TOK_FLOAT) { arm_kind=TY_FLOAT; arm_size=4; arm_saw_base_type=1; lexer_next();
			} else if (lexer_peek()->kind == TOK_DOUBLE) { arm_kind=TY_DOUBLE; arm_size=8; arm_saw_base_type=1; lexer_next();
			} else if (lexer_peek()->kind == TOK_STRUCT || lexer_peek()->kind == TOK_UNION ||
			           lexer_peek()->kind == TOK_ENUM) {
				arm_kind = lexer_peek()->kind == TOK_ENUM ? TY_ENUM :
				           lexer_peek()->kind == TOK_UNION ? TY_UNION : TY_STRUCT;
				arm_saw_base_type=1;
				lexer_next();
				if (lexer_peek()->kind == TOK_IDENT)
					{ STRNCPY(arm_struct, lexer_peek()->text, sizeof(arm_struct)-1); lexer_next(); }
			} else if (lexer_peek()->kind == TOK_IDENT && lexer_peek()->text) {
				/* typedef name */
				Type *td = parser_find_typedef(lexer_peek()->text);
				if (td) {
					arm_type = parser_canonicalize_decl_type(parse_type_name());
					arm_size = arm_type && arm_type->size ? arm_type->size
					         : (type_is_pointer(arm_type) ? TCC_SIZEOF_PTR : 4);
					arm_is_unsigned = type_is_unsigned(arm_type);
					arm_rank = expr_integer_rank_for_generic(arm_type);
					arm_is_ptr = type_is_pointer(arm_type) ? 1 : 0;
					arm_kind = arm_type ? arm_type->kind : td->kind;
					arm_source_kind = type_source_kind(arm_type);
					if (arm_type &&
					    (type_is_struct(arm_type) || type_is_union(arm_type) ||
					     type_is_enum(arm_type)))
						STRNCPY(arm_struct, expr_generic_tag_name(arm_type),
						        sizeof(arm_struct) - 1);
					arm_saw_base_type=1;
					goto generic_assoc_type_parsed;
				}
				else if (!arm_saw_sign_specifier) { /* unknown -- consume it */ lexer_next(); }
			} else if (!arm_saw_sign_specifier) { lexer_next(); } /* skip anything else */
			if (arm_saw_sign_specifier && !arm_saw_base_type) {
				arm_kind=TY_INT;
				arm_size=4;
				arm_rank=3;
			}
			if (expr_special_type_token_source_kind(lexer_peek()) != TYPE_SOURCE_DEFAULT) {
				arm_source_kind = expr_special_type_token_source_kind(lexer_peek());
				lexer_next();
			}

			/* Qualifiers after base type */
			if (!arm_special_atomic_specifier) {
				while (lexer_peek()->kind == TOK_CONST ||
				       lexer_peek()->kind == TOK_VOLATILE ||
				       lexer_peek()->kind == TOK_RESTRICT ||
				       lexer_peek()->kind == TOK_ATOMIC ||
				       (lexer_peek()->kind == TOK_IDENT && lexer_peek()->text &&
				        STRCMP(lexer_peek()->text, "volatile") == 0)) {
					if (lexer_peek()->kind == TOK_CONST) { arm_is_const=1; arm_base_qualifiers |= TYPE_QUAL_CONST; }
					if (lexer_peek()->kind == TOK_VOLATILE ||
					    (lexer_peek()->kind == TOK_IDENT && lexer_peek()->text &&
					     STRCMP(lexer_peek()->text, "volatile") == 0))
						arm_base_qualifiers |= TYPE_QUAL_VOLATILE;
					if (lexer_peek()->kind == TOK_RESTRICT) arm_base_qualifiers |= TYPE_QUAL_RESTRICT;
					if (lexer_peek()->kind == TOK_ATOMIC) arm_base_qualifiers |= TYPE_QUAL_ATOMIC;
					lexer_next();
				}
			}

			/* Pointer stars */
			while (lexer_peek()->kind == TOK_STAR) {
				int ptr_quals = 0;

				arm_is_ptr++; lexer_next();
				if (!arm_type)
					arm_type = type_ptr(expr_generic_assoc_base_type(arm_kind,
					                                                  arm_size,
					                                                  arm_is_unsigned,
					                                                  arm_base_qualifiers,
					                                                  arm_struct,
					                                                  arm_source_kind));
				else
					arm_type = type_ptr(arm_type);
				while (lexer_peek()->kind == TOK_CONST ||
				       lexer_peek()->kind == TOK_VOLATILE ||
				       lexer_peek()->kind == TOK_RESTRICT ||
				       lexer_peek()->kind == TOK_ATOMIC) {
					if (lexer_peek()->kind == TOK_CONST)
						ptr_quals |= TYPE_QUAL_CONST;
					else if (lexer_peek()->kind == TOK_VOLATILE)
						ptr_quals |= TYPE_QUAL_VOLATILE;
					else if (lexer_peek()->kind == TOK_RESTRICT)
						ptr_quals |= TYPE_QUAL_RESTRICT;
					else if (lexer_peek()->kind == TOK_ATOMIC)
						ptr_quals |= TYPE_QUAL_ATOMIC;
					arm_has_top_ptr_qualifiers = 1;
					lexer_next();
				}
				if (ptr_quals)
					arm_type = type_with_qualifiers(arm_type,
					                               type_qualifiers(arm_type) | ptr_quals);
			}

			/* Abstract function-pointer type-name: int (*)(void) */
			if (lexer_peek()->kind == TOK_LPAREN &&
			    lexer_peek_ahead(1)->kind == TOK_STAR) {
				Type **fp_param_types = NULL;
				int fp_param_count = 0;
				int fp_is_variadic = 0;
				int fp_fixed_params = 0;
				int fp_has_prototype = 0;
				int fp_pointer_depth = 0;
				int fp_pointer_quals[MAX_ARRAY_DIMS] = {0};
				Type *ret_type;
				Type *func_type;

				lexer_next();
				while (lexer_peek()->kind == TOK_STAR) {
					int ptr_quals = 0;

					fp_pointer_depth++;
					lexer_next();
					while (lexer_peek()->kind == TOK_CONST ||
					       lexer_peek()->kind == TOK_VOLATILE ||
					       lexer_peek()->kind == TOK_RESTRICT ||
					       lexer_peek()->kind == TOK_ATOMIC) {
						if (lexer_peek()->kind == TOK_CONST)
							ptr_quals |= TYPE_QUAL_CONST;
						else if (lexer_peek()->kind == TOK_VOLATILE)
							ptr_quals |= TYPE_QUAL_VOLATILE;
						else if (lexer_peek()->kind == TOK_RESTRICT)
							ptr_quals |= TYPE_QUAL_RESTRICT;
						else if (lexer_peek()->kind == TOK_ATOMIC)
							ptr_quals |= TYPE_QUAL_ATOMIC;
						arm_has_top_ptr_qualifiers = 1;
						lexer_next();
					}
					if (fp_pointer_depth <= MAX_ARRAY_DIMS)
						fp_pointer_quals[fp_pointer_depth - 1] = ptr_quals;
				}
				if (lexer_peek()->kind == TOK_LPAREN &&
				    lexer_peek_ahead(1)->kind == TOK_STAR) {
					int dims[MAX_ARRAY_DIMS] = {0};
					int dim_count = 0;
					int elem_pointer_depth = 0;
					Type *elem_type = arm_type ? clone_type(arm_type) :
					                  expr_generic_assoc_base_type(arm_kind,
					                                               arm_size,
					                                               arm_is_unsigned,
					                                               arm_base_qualifiers,
					                                               arm_struct,
					                                               arm_source_kind);

					lexer_next();
					while (lexer_peek()->kind == TOK_STAR) {
						int ptr_quals = 0;

						elem_pointer_depth++;
						lexer_next();
						while (lexer_peek()->kind == TOK_CONST ||
						       lexer_peek()->kind == TOK_VOLATILE ||
						       lexer_peek()->kind == TOK_RESTRICT ||
						       lexer_peek()->kind == TOK_ATOMIC) {
							if (lexer_peek()->kind == TOK_CONST)
								ptr_quals |= TYPE_QUAL_CONST;
							else if (lexer_peek()->kind == TOK_VOLATILE)
								ptr_quals |= TYPE_QUAL_VOLATILE;
							else if (lexer_peek()->kind == TOK_RESTRICT)
								ptr_quals |= TYPE_QUAL_RESTRICT;
							else if (lexer_peek()->kind == TOK_ATOMIC)
								ptr_quals |= TYPE_QUAL_ATOMIC;
							arm_has_top_ptr_qualifiers = 1;
							lexer_next();
						}
						if (ptr_quals && elem_type)
							elem_type = type_with_qualifiers(elem_type,
							                                type_qualifiers(elem_type) | ptr_quals);
					}
					expect(TOK_RPAREN);
					while (lexer_peek()->kind == TOK_LBRACKET) {
						long long len;

						lexer_next();
						if (lexer_peek()->kind == TOK_RBRACKET)
							fatal_cur("generic association type must be a complete object type\n");
						if (lexer_peek()->kind == TOK_STAR ||
						    !sizeof_type_array_bound_is_const_expr())
							fatal_cur("generic association type cannot be variably modified\n");
						len = parser_eval_const_int_expr();
						if (len <= 0)
							fatal_cur("Array length must be positive\n");
						if (dim_count >= MAX_ARRAY_DIMS)
							fatal_cur("Too many array dimensions\n");
						dims[dim_count++] = (int)len;
						expect(TOK_RBRACKET);
					}
					expect(TOK_RPAREN);
					if (lexer_peek()->kind == TOK_LPAREN) {
						Type **array_fn_param_types = NULL;
						int array_fn_param_count = 0;
						int array_fn_is_variadic = 0;
						int array_fn_fixed_params = 0;
						int array_fn_has_prototype = 0;

						parse_prototype_param_list(&array_fn_param_types,
						                           &array_fn_param_count,
						                           &array_fn_is_variadic,
						                           &array_fn_fixed_params,
						                           &array_fn_has_prototype, 1);
						elem_type = array_fn_has_prototype
						            ? parser_make_function_type(elem_type,
						                                        array_fn_param_types,
						                                        array_fn_param_count,
						                                        array_fn_is_variadic,
						                                        array_fn_fixed_params)
						            : type_func(clone_type(elem_type));
					}
					while (elem_pointer_depth-- > 0)
						elem_type = type_ptr(elem_type);
					arm_type = build_array_type_from_dims(elem_type, dims, dim_count);
					for (int i = 0; i < fp_pointer_depth; i++) {
						arm_type = type_ptr(arm_type);
						if (fp_pointer_quals[i])
							arm_type = type_with_qualifiers(
							    arm_type,
							    type_qualifiers(arm_type) |
							        fp_pointer_quals[i]);
					}
					arm_is_ptr = type_is_pointer(arm_type);
					arm_kind = arm_type->kind;
					arm_size = arm_type->size ? arm_type->size : TCC_SIZEOF_PTR;
					goto generic_assoc_type_parsed;
				}
				expect(TOK_RPAREN);
				if (lexer_peek()->kind == TOK_LBRACKET) {
					int dims[MAX_ARRAY_DIMS] = {0};
					int dim_count = 0;
					Type *elem_type = arm_type ? clone_type(arm_type) :
					                  expr_generic_assoc_base_type(arm_kind,
					                                               arm_size,
					                                               arm_is_unsigned,
					                                               arm_base_qualifiers,
					                                               arm_struct,
					                                               arm_source_kind);
					while (lexer_peek()->kind == TOK_LBRACKET) {
						long long len;

						lexer_next();
						if (lexer_peek()->kind == TOK_RBRACKET)
							fatal_cur("generic association type must be a complete object type\n");
						if (lexer_peek()->kind == TOK_STAR ||
						    !sizeof_type_array_bound_is_const_expr())
							fatal_cur("generic association type cannot be variably modified\n");
						len = parser_eval_const_int_expr();
						if (len <= 0)
							fatal_cur("Array length must be positive\n");
						if (dim_count >= MAX_ARRAY_DIMS)
							fatal_cur("Too many array dimensions\n");
						dims[dim_count++] = (int)len;
						expect(TOK_RBRACKET);
					}
					if (lexer_peek()->kind == TOK_LPAREN) {
						Type **array_fn_param_types = NULL;
						int array_fn_param_count = 0;
						int array_fn_is_variadic = 0;
						int array_fn_fixed_params = 0;
						int array_fn_has_prototype = 0;

						parse_prototype_param_list(&array_fn_param_types,
						                           &array_fn_param_count,
						                           &array_fn_is_variadic,
						                           &array_fn_fixed_params,
						                           &array_fn_has_prototype, 1);
						elem_type = type_ptr(array_fn_has_prototype
						                     ? parser_make_function_type(elem_type,
						                                                 array_fn_param_types,
						                                                 array_fn_param_count,
						                                                 array_fn_is_variadic,
						                                                 array_fn_fixed_params)
						                     : type_func(clone_type(elem_type)));
					}
					arm_type = build_array_type_from_dims(elem_type, dims, dim_count);
					while (fp_pointer_depth-- > 0)
						arm_type = type_ptr(arm_type);
					arm_is_ptr = type_is_pointer(arm_type);
					arm_kind = arm_type->kind;
					arm_size = arm_type->size ? arm_type->size : TCC_SIZEOF_PTR;
					goto generic_assoc_type_parsed;
				}
				parse_prototype_param_list(&fp_param_types, &fp_param_count,
				                           &fp_is_variadic, &fp_fixed_params,
				                           &fp_has_prototype, 1);

				ret_type = arm_type ? clone_type(arm_type) :
				           expr_generic_assoc_base_type(arm_kind, arm_size,
				                                        arm_is_unsigned,
				                                        arm_base_qualifiers,
				                                        arm_struct,
				                                        arm_source_kind);
				func_type = fp_has_prototype
				            ? parser_make_function_type(ret_type, fp_param_types,
				                                        fp_param_count,
				                                        fp_is_variadic,
				                                        fp_fixed_params)
				            : type_func(clone_type(ret_type));
				arm_type = func_type;
				for (int i = 0; i < fp_pointer_depth; i++) {
					arm_type = type_ptr(arm_type);
					if (fp_pointer_quals[i])
						arm_type = type_with_qualifiers(
						    arm_type,
						    type_qualifiers(arm_type) |
						        fp_pointer_quals[i]);
				}
				arm_is_ptr = type_is_pointer(arm_type);
				arm_kind = arm_type->kind;
				arm_size = arm_type->size ? arm_type->size : TCC_SIZEOF_PTR;
			}
generic_assoc_type_parsed:
			if (lexer_peek()->kind == TOK_LPAREN)
				fatal_cur("generic association type must be a complete object type\n");

			/* Array subscript: int[4] */
			if (lexer_peek()->kind == TOK_LBRACKET) {
				arm_is_arr=1;
				lexer_next();
				if (lexer_peek()->kind == TOK_RBRACKET)
					fatal_cur("generic association type must be a complete object type\n");
				if (lexer_peek()->kind == TOK_STAR ||
				    !sizeof_type_array_bound_is_const_expr())
					fatal_cur("generic association type cannot be variably modified\n");
				while (lexer_peek()->kind != TOK_RBRACKET && lexer_peek()->kind != TOK_EOF)
					lexer_next();
				if (lexer_peek()->kind == TOK_RBRACKET) lexer_next();
			}
			(void)arm_is_const; (void)arm_is_arr;

			if (!arm_is_ptr && arm_kind == TY_VOID)
				fatal_cur("generic association type must be a complete object type\n");

			if (!arm_type &&
			    (arm_kind == TY_STRUCT || arm_kind == TY_UNION || arm_kind == TY_ENUM)) {
				arm_type = expr_generic_assoc_base_type(arm_kind, arm_size,
				                                        arm_is_unsigned,
				                                        arm_base_qualifiers,
				                                        arm_struct,
				                                        arm_source_kind);
			}
			if (!arm_type &&
			    !arm_is_ptr &&
			    arm_kind != TY_VOID &&
			    arm_kind != TY_STRUCT &&
			    arm_kind != TY_UNION &&
			    arm_kind != TY_ENUM) {
				arm_type = expr_generic_assoc_base_type(arm_kind, arm_size,
				                                        arm_is_unsigned,
				                                        arm_base_qualifiers,
				                                        arm_struct,
				                                        arm_source_kind);
			}

			if (arm_type && !expr_generic_type_has_char_leaf(arm_type)) {
				for (int i = 0; i < assoc_count; i++) {
					int compat = assoc_types[i].type &&
					             !expr_generic_type_has_char_leaf(assoc_types[i].type) &&
					             expr_generic_types_compatible(assoc_types[i].type, arm_type, 1);
					if (compat)
						fatal_cur("duplicate type association in generic selection\n");
				}
			}
			if (!arm_is_ptr) {
				for (int i = 0; i < assoc_count; i++) {
					if (assoc_types[i].is_ptr == arm_is_ptr &&
				    assoc_types[i].kind == arm_kind &&
				    assoc_types[i].size == arm_size &&
				    assoc_types[i].rank == arm_rank &&
				    assoc_types[i].is_unsigned == arm_is_unsigned &&
				    expr_generic_source_kind_matches(arm_kind,
				                                     assoc_types[i].source_kind,
				                                     arm_source_kind) &&
				    STRCMP(assoc_types[i].struct_name, arm_struct) == 0)
						fatal_cur("duplicate type association in generic selection\n");
				}
			}
			if (assoc_count >= (int)(sizeof(assoc_types) / sizeof(assoc_types[0])))
				fatal_cur("too many generic associations\n");
			assoc_types[assoc_count].is_ptr = arm_is_ptr;
			assoc_types[assoc_count].kind = arm_kind;
			assoc_types[assoc_count].size = arm_size;
			assoc_types[assoc_count].rank = arm_rank;
			assoc_types[assoc_count].is_unsigned = arm_is_unsigned;
			assoc_types[assoc_count].is_const = arm_is_const;
			assoc_types[assoc_count].source_kind = arm_source_kind;
			assoc_types[assoc_count].has_top_ptr_qualifiers = arm_has_top_ptr_qualifiers;
			assoc_types[assoc_count].type = arm_type;
			STRNCPY(assoc_types[assoc_count].struct_name, arm_struct,
			        sizeof(assoc_types[assoc_count].struct_name) - 1);
			assoc_count++;

			/* Match */
			if (!matched) {
				if (arm_struct[0]) {
					arm_match = (ctrl_struct[0] && STRCMP(ctrl_struct, arm_struct) == 0);
				} else if (arm_is_ptr) {
					arm_match = ctrl_is_ptr && ctrl_type && arm_type &&
					            expr_generic_types_compatible(ctrl_type, arm_type, 1);
				} else if (ctrl_type && arm_type &&
				           (type_is_complex(ctrl_type) || type_is_complex(arm_type) ||
				            type_is_imaginary(ctrl_type) || type_is_imaginary(arm_type))) {
					arm_match = expr_generic_types_compatible(ctrl_type, arm_type, 1);
				} else if (arm_kind == TY_FLOAT || arm_kind == TY_DOUBLE) {
					arm_match = (!ctrl_is_ptr && !ctrl_struct[0] && ctrl_kind == arm_kind);
				} else if (arm_is_long) {
					arm_match = (!ctrl_is_ptr && !ctrl_struct[0] && ctrl_rank == arm_rank &&
					             ctrl_is_unsigned == arm_is_unsigned);
				} else {
					arm_match = (!ctrl_is_ptr && !ctrl_struct[0] && !ctrl_is_long &&
					             (ctrl_kind == TY_INT || ctrl_kind == TY_CHAR ||
					              ctrl_kind == TY_SHORT) &&
					             ctrl_size == arm_size &&
					             ctrl_rank == arm_rank &&
					             ctrl_is_unsigned == arm_is_unsigned &&
					             expr_generic_source_kind_matches(arm_kind,
					                                              ctrl_source_kind,
					                                              arm_source_kind));
				}
			}
		}

		expect(TOK_COLON);
		Node *arm_val = parse_expr();

		if (is_default) {
			default_result = arm_val;
		} else if (arm_match && !matched) {
			result = arm_val;
			matched = 1;
		}

		if (lexer_peek()->kind == TOK_COMMA) lexer_next();
	}

	expect(TOK_RPAREN);

	if (!result) result = default_result;
	if (!result)
		fatal_cur("generic selection has no matching association\n");
	return result;
}

Node *
parse_factor(void)
{
	ParserProfileBucket ident_bucket = PARSER_PROFILE_BUCKET_COUNT;
#define RETURN_FACTOR_NODE(expr) \
	do { \
		Node *_node = (expr); \
		parser_profile_scope_leave(PARSER_PROF_EXPR_FACTOR); \
		return _node; \
	} while (0)
#define RETURN_IDENT_NODE(expr) \
	do { \
		Node *_node = (expr); \
		if (ident_bucket != PARSER_PROFILE_BUCKET_COUNT) \
			parser_profile_scope_leave(ident_bucket); \
		parser_profile_scope_leave(PARSER_PROF_EXPR_IDENT); \
		parser_profile_scope_leave(PARSER_PROF_EXPR_FACTOR); \
		return _node; \
	} while (0)
#define ENTER_IDENT_BUCKET(bucket) \
	do { \
		ident_bucket = (bucket); \
		parser_profile_scope_enter(bucket); \
	} while (0)
#define RETURN_PAREN_NODE(expr) \
	do { \
		Node *_node = (expr); \
		parser_profile_scope_leave(PARSER_PROF_EXPR_PAREN); \
		parser_profile_scope_leave(PARSER_PROF_EXPR_FACTOR); \
		return _node; \
	} while (0)

	parser_profile_scope_enter(PARSER_PROF_EXPR_FACTOR);
	const Token *token = lexer_peek();

	if (token->kind == TOK_NUM) {
		Node *n;
		lexer_next();
		if (token->num_is_fp) {
			n = new_num_fp(token->num_is_float ? type_float() : type_double(),
			               token->text);
			RETURN_FACTOR_NODE(n);
		}
		n = new_num_long(token->long_value);
		if (token->num_rank == 5) {
			n->type = token->num_is_unsigned ? type_ullong() : type_llong();
			n->is_unsigned = token->num_is_unsigned;
			n->elem_size = 8;
		} else if (token->num_rank == 4) {
			n->type = token->num_is_unsigned ? type_ulong() : type_long();
			n->is_unsigned = token->num_is_unsigned;
			n->elem_size = 8;
		} else if (token->num_is_unsigned) {
			n->type = type_uint();
			n->is_unsigned = 1;
			n->elem_size = 4;
		}
		RETURN_FACTOR_NODE(n);
	}

	if (token->kind == TOK_STRING) {
		int width = token->string_width ? token->string_width : 1;
		size_t cap = token->text_len + 1;
		const char *first_text = token->text;
		unsigned int first_len = token->text_len;
		lexer_next();
		/* concatenate adjacent string literals with the same element width */
		size_t len = 0;
		char *buf = xmalloc(cap + 1);
		memcpy(buf + len, first_text, first_len);
		len += first_len;
		while (lexer_peek()->kind == TOK_STRING) {
			const Token *next = lexer_peek();
			int next_width = next->string_width ? next->string_width : 1;
			if (next_width != width)
				fatal_cur("cannot concatenate string literals with different element widths\n");
			lexer_next();
			if (len + next->text_len + 1 > cap) {
				cap = (len + next->text_len + 1) * 2;
				buf = xrealloc(buf, cap + 1);
			}
			memcpy(buf + len, next->text, next->text_len);
			len += next->text_len;
		}
		buf[len] = '\0';
		Node *n = new_string_len_width(buf, len, parser_alloc_string_label(), width);
		parser_register_string_literal(n->string_label, n->string_value,
		                              n->string_len, n->string_width);
		xfree(buf);
		RETURN_FACTOR_NODE(n);
	}

	if (token->kind == TOK_IDENT && token->text &&
	    STRCMP(token->text, "_Generic") == 0) {
		if (!tcc_lang_at_least(LANG_C11))
			fatal_cur("_Generic is not allowed before C11\n");
		lexer_next();
		RETURN_FACTOR_NODE(parse_generic_selection());
	}

	if (tcc_lang_at_least(LANG_C23) && token_is_c23_true_keyword(token)) {
		Node *node = new_num(1);
		node->type = type_with_source(type_uchar(), TYPE_SOURCE_BOOL, "bool");
		node->elem_size = 1;
		node->is_unsigned = 1;
		lexer_next();
		RETURN_FACTOR_NODE(node);
	}

	if (tcc_lang_at_least(LANG_C23) && token_is_c23_false_keyword(token)) {
		Node *node = new_num(0);
		node->type = type_with_source(type_uchar(), TYPE_SOURCE_BOOL, "bool");
		node->elem_size = 1;
		node->is_unsigned = 1;
		lexer_next();
		RETURN_FACTOR_NODE(node);
	}

	if (tcc_lang_at_least(LANG_C23) && token_is_c23_nullptr_keyword(token)) {
		Node *node = new_num(0);
		node->type = type_ptr(type_void());
		node->elem_size = TCC_SIZEOF_PTR;
		node->is_pointer = 1;
		node->is_unsigned = 1;
		lexer_next();
		RETURN_FACTOR_NODE(node);
	}

	if (token->kind == TOK_IDENT) {
		parser_profile_scope_enter(PARSER_PROF_EXPR_IDENT);
		char ident_name[64];
		STRNCPY(ident_name, token->text, sizeof(ident_name) - 1);
		/* lexer_next may recycle the current token storage. Preserve the
		 * identifier before advancing so builtin dispatch is deterministic. */
		lexer_next();

		if (STRCMP(ident_name, "__func__") == 0 && tcc_lang_is_c89_or_c90())
			fatal_cur("__func__ is not allowed in C89/C90 mode\n");

		if (lexer_peek()->kind == TOK_LPAREN) {
			ENTER_IDENT_BUCKET(PARSER_PROF_EXPR_IDENT_CALL);
			if (STRCMP(ident_name, "__builtin_va_arg") == 0) {
				Node *ap;
				Node *call;
				Type *result_type;
				lexer_next(); /* consume ( */
				ap = parse_assignment();
				expect(TOK_COMMA);
				result_type = parse_type_name();
				expect(TOK_RPAREN);
				call = new_call("__builtin_va_arg", ap);
				call->type = result_type;
				call->elem_size = type_sizeof(result_type);
				call->is_unsigned = type_is_unsigned(result_type);
				RETURN_IDENT_NODE(call);
			}
			/* offsetof(Type, field) builtin */
			if (STRCMP(ident_name, "offsetof") == 0) {
				char sname[64] = {0};
				char fname[64] = {0};
				int off = 0;
				StructDef *sd;
				Type *td;
				int fi2;
				lexer_next(); /* consume ( */
				if (lexer_peek()->kind == TOK_STRUCT || lexer_peek()->kind == TOK_UNION)
					lexer_next();
				if (lexer_peek()->kind == TOK_IDENT) {
					STRNCPY(sname, lexer_peek()->text ? lexer_peek()->text : "", sizeof(sname)-1);
					lexer_next();
				}
				expect(TOK_COMMA);
				if (lexer_peek()->kind == TOK_IDENT) {
					STRNCPY(fname, lexer_peek()->text ? lexer_peek()->text : "", sizeof(fname)-1);
					lexer_next();
				}
				expect(TOK_RPAREN);
				sd = sname[0] ? find_struct_or_null(sname) : NULL;
				if (!sd && sname[0]) {
					td = parser_find_typedef(sname);
					if (td && td->struct_name[0])
						sd = find_struct_or_null(td->struct_name);
				}
				if (sd && fname[0]) {
					for (fi2 = 0; fi2 < sd->field_count; fi2++) {
						if (STRCMP(sd->fields[fi2].name, fname) == 0) {
							off = sd->fields[fi2].offset;
							break;
						}
					}
				}
				RETURN_IDENT_NODE(new_num(off));
			}
				Local *local_info = parser_find_local_info_latest(ident_name);
				FuncInfo *fi;
				Node *args;

				/*
				 * A call expression whose callee name denotes a function-pointer
				 * object must be emitted as an indirect call.  This applies to both
				 * locals/parameters and globals, e.g.:
				 *
				 *     int (*fprintfptr)(FILE *, const char *, ...) = &fprintf;
				 *     fprintfptr(stderr, "%d\\n", 42);
				 *
				 * Do not turn that into a direct branch to the data symbol
				 * _fprintfptr; load the pointer value and BLR through it.
				 */
				if (local_info && local_info->is_pointer) {
					Global *global_info = NULL;
					Node *callee = make_scalar_var_node_resolved(ident_name, local_info, global_info);
					Type *func_type = NULL;

					lexer_next();
					if (callee->type &&
					    type_is_pointer(callee->type) &&
					    callee->type->base &&
					    type_is_function(callee->type->base)) {
						func_type = callee->type->base;
					}
					args = parse_arg_list_for_type(func_type, ident_name);
					expect(TOK_RPAREN);
					Node *call = new_indirect_call(callee, args);
					if (func_type && func_type->base)
						expr_apply_call_result_type(call, func_type->base);
					RETURN_IDENT_NODE(call);
				}

				fi = find_func(ident_name);
				lexer_next();
				if (STRCMP(ident_name, "__builtin_va_arg") == 0) {
					Node *ap = parse_assignment();
					Node *call;
					Type *result_type;
					expect(TOK_COMMA);
					result_type = parse_type_name();
					expect(TOK_RPAREN);
					call = new_call("__builtin_va_arg", ap);
					call->type = result_type;
					call->elem_size = type_sizeof(result_type);
					call->is_unsigned = type_is_unsigned(result_type);
					RETURN_IDENT_NODE(call);
				}

				if (!fi) {
					Global *global_info = parser_find_global_info(ident_name);
					if (expr_global_is_pointer_like(global_info)) {
						Node *callee = make_scalar_var_node_resolved(ident_name, NULL, global_info);
						Type *func_type = NULL;

						if (callee->type &&
						    type_is_pointer(callee->type) &&
						    callee->type->base &&
						    type_is_function(callee->type->base)) {
							func_type = callee->type->base;
						}
						args = parse_arg_list_for_type(func_type, ident_name);
						expect(TOK_RPAREN);
						Node *call = new_indirect_call(callee, args);
						if (func_type && func_type->base)
							expr_apply_call_result_type(call, func_type->base);
						RETURN_IDENT_NODE(call);
					}
				}

				args = parse_arg_list(fi);
				expect(TOK_RPAREN);

				/*
				 * GCC/Clang branch prediction hint.  It has no required runtime
				 * semantics beyond evaluating and returning the first argument, so
				 * lower __builtin_expect(expr, expected) to expr and deliberately
				 * discard the expected-value argument.
				 */
				if (STRCMP(ident_name, "__builtin_expect") == 0) {
					if (!args)
						fatal_cur("__builtin_expect requires at least one argument\n");
					RETURN_IDENT_NODE(args);
				}

				if (!fi &&
				    STRCMP(ident_name, "__builtin_stack_save") != 0 &&
				    STRCMP(ident_name, "__builtin_stack_restore") != 0 &&
			    STRCMP(ident_name, "__builtin_stack_alloc") != 0 &&
			    STRCMP(ident_name, "__builtin_va_start") != 0 &&
			    STRCMP(ident_name, "__builtin_va_arg") != 0 &&
			    STRCMP(ident_name, "__builtin_va_copy") != 0 &&
			    tcc_lang_at_least(LANG_C99)) {
					fatal_cur("implicit function declaration of '%s' is not allowed in C99 or later\n", ident_name);
				}

			Node *call = new_call(ident_name, args);
			if (STRCMP(ident_name, "__builtin_stack_save") == 0 ||
			    STRCMP(ident_name, "__builtin_stack_alloc") == 0 ||
			    STRCMP(ident_name, "__builtin_va_start") == 0 ||
			    STRCMP(ident_name, "__builtin_va_copy") == 0) {
				call->is_pointer = 1;
				call->elem_size = 1;
				/* va_start returns the same opaque cursor-pointer type as its
				 * destination. This preserves type checking for the x64 va_list
				 * macro while the backend supplies the cursor address. */
				call->type = STRCMP(ident_name, "__builtin_va_start") == 0 && args && args->type
					? clone_type(args->type) :
					(STRCMP(ident_name, "__builtin_va_copy") == 0 && args && args->next && args->next->type
						? clone_type(args->next->type) : type_ptr(type_char()));
				RETURN_IDENT_NODE(call);
			}
			if (fi && fi->return_type)
				expr_apply_call_result_type(call, fi->return_type);
			if (fi && fi->returns_struct) {
				call->returns_struct = 1;
				call->aggregate_abi_class = fi->return_abi_class;
				call->aggregate_abi_reg_count = fi->return_abi_reg_count;
				call->struct_return_size = fi->struct_size;
				if (fi->return_type)
					call->type = clone_type(fi->return_type);
				else if (fi->struct_name[0])
					call->type = fi->return_type ? clone_type(fi->return_type)
					                             : type_struct(fi->struct_name, fi->struct_size);
				if (fi->struct_name[0]) {
					STRNCPY(call->return_struct_name, fi->struct_name,
					        sizeof(call->return_struct_name) - 1);
				}
			}
			RETURN_IDENT_NODE(call);
		}

		if (lexer_peek()->kind == TOK_DOT && is_global_struct(ident_name)) {
			ENTER_IDENT_BUCKET(PARSER_PROF_EXPR_IDENT_DOT);
			lexer_next();

			const Token *field_tok = lexer_peek();
			if (field_tok->kind != TOK_IDENT) {
				fatal_cur("Expected field name after global struct '.'\n");
			}
			lexer_next();

			StructDef *def = find_struct(global_struct_name(ident_name));
			Field *field = find_field(def->name, field_tok->text);

			if (field->is_struct && field->struct_name[0] && lexer_peek()->kind == TOK_DOT) {
				/* Nested struct: v.s.a — get byte address of v.s, then access .a */
				/* Use addr_gidx with byte offset to get &v.s, then chain dot access */
				Node *base = new_global_index(ident_name, new_num(field->offset), 1);
				base->is_array_field = 1; /* use addr_gidx to get pointer */
				base->is_pointer = 1;
				base->elem_size = field->size;
				STRNCPY(base->struct_name, field->struct_name, sizeof(base->struct_name) - 1);
				base->type = type_struct(field->struct_name, field->size);
				/* Now handle .next_field via member_ptr */
				lexer_next(); /* consume . */
				const Token *inner_tok = lexer_peek();
				if (inner_tok->kind != TOK_IDENT)
					fatal_cur("Expected field name after nested global struct '.'\n");
				lexer_next();
				StructDef *inner_def = find_struct(field->struct_name);
				Field *inner_field = find_field(inner_def->name, inner_tok->text);
				Node *member = new_member_ptr(inner_tok->text, base, inner_field->offset);
				member->elem_size = inner_field->size;
				apply_field_type(member, inner_field);
				member->is_const_lvalue = expr_type_is_const_qualified(base->type);
				RETURN_IDENT_NODE(member);
			}

			/* Use addr_gidx (byte offset, scale=1) + load_deref for correct access.
			 * This avoids the elem_size multiply in gidx_load giving wrong byte offsets. */
			Node *addr = new_global_index(ident_name, new_num(field->offset), 1);
			addr->is_array_field = 1; /* emit addr_gidx to get pointer */
			addr->is_pointer = 1;
			addr->elem_size = field->is_array && field->elem_size > 0 ? field->elem_size : field->size;
			if (field->is_array && field->type && type_is_array(field->type) && type_pointee(field->type)) {
				/*
				 * Preserve the full array type for sizeof/alignof. ND_GLOBAL_INDEX
				 * already decays array-field expressions to an address at emit time
				 * when is_array_field is set, so we do not need to erase the array
				 * type here.
				 */
				apply_field_type(addr, field);
				addr->elem_size = 1; /* byte index: field->offset is already scaled */
				addr->is_pointer = 0;
				return addr;
			}
			Node *member = new_deref(addr);
			member->elem_size = field->size;
			apply_field_type(member, field);
			member->is_const_lvalue = expr_type_is_const_qualified(global_type(ident_name));
			STRNCPY(member->struct_name, def->name, sizeof(member->struct_name) - 1);
			RETURN_IDENT_NODE(member);
		}

		if (lexer_peek()->kind == TOK_LBRACKET) {
			ENTER_IDENT_BUCKET(PARSER_PROF_EXPR_IDENT_INDEX);
			lexer_next();
			Node *index = parse_expr();
			expect(TOK_RBRACKET);

			if (is_global_array(ident_name) && is_global_struct(ident_name) &&
			        lexer_peek()->kind == TOK_DOT) {
				lexer_next();

				const Token *field_tok = lexer_peek();
				if (field_tok->kind != TOK_IDENT) {
					fatal_cur("Expected field name after global struct array '.'\n");
				}
				lexer_next();

				StructDef *def = find_struct(global_struct_name(ident_name));
				Field *field = find_field(def->name, field_tok->text);

				/* Compute byte offset: index * sizeof(struct) + field->offset.
				 * Then use addr_gidx with elem_size=1 to get byte address,
				 * and load/store with field->size for correct width. */
				Node *byte_idx = new_binary(ND_MUL, index, new_num(def->size));
				if (field->offset != 0)
					byte_idx = new_binary(ND_ADD, byte_idx, new_num(field->offset));

				if (field->is_array) {
					/* Array field: produce address of element, not a load.
					 * For entries[i].name (char[32]), we want &entries[i].name[0]. */
					int byte_scale = def->size;
					Node *array_byte_idx = index;
					if (byte_scale != 1)
						array_byte_idx = new_binary(ND_MUL, array_byte_idx, new_num(byte_scale));
					if (field->offset != 0)
						array_byte_idx = new_binary(ND_ADD, array_byte_idx, new_num(field->offset));
					Node *member = new_global_index(ident_name, array_byte_idx, 1);
					member->is_array_field = 1;
					apply_field_type(member, field);
					member->is_const_lvalue = expr_type_is_const_qualified(global_type(ident_name));
					member->elem_size = 1; /* byte-indexed: array_byte_idx is already in bytes */
					STRNCPY(member->struct_name, def->name, sizeof(member->struct_name) - 1);
					RETURN_IDENT_NODE(member);
				}

				/* Non-array field: compute byte address then dereference. */
				Node *gidx = new_global_index(ident_name, byte_idx, 1);
				gidx->is_array_field = 1; /* use addr_gidx, not gidx_load */
				Node *member = new_deref(gidx);
				member->elem_size = field->size;
				apply_field_type(member, field);
				member->is_const_lvalue = expr_type_is_const_qualified(global_type(ident_name));
				STRNCPY(member->struct_name, def->name, sizeof(member->struct_name) - 1);
				RETURN_IDENT_NODE(member);
			}

			if (STRCMP(ident_name, "__func__") == 0 && !tcc_lang_is_c89_or_c90()) {
				Node *base = make_scalar_var_node(ident_name);
				Node *addr = new_binary(ND_ADD, base, index);
				addr->is_pointer = 1;
				addr->elem_size = base->elem_size;
				if (base->type && type_is_pointer(base->type))
					addr->type = base->type;
				RETURN_IDENT_NODE(new_deref(addr));
			}

			if (is_static_local(ident_name) && is_static_array_local(ident_name)) {
				const char *gname = static_global_name_local(ident_name);
				Type *arr_type = type_local(ident_name);
				Node *gidx = new_global_index(gname, index, static_local_elem_size(ident_name));
				if (arr_type && type_is_array(arr_type) && type_pointee(arr_type)) {
					Type *elem_type = type_pointee(arr_type);
					gidx->type = clone_type(elem_type);
					gidx->is_const_lvalue = expr_type_is_const_qualified(elem_type);
					gidx->is_unsigned = type_is_unsigned(elem_type);
					gidx->is_pointer = type_is_pointer(elem_type);
					if (type_is_struct(elem_type)) {
						STRNCPY(gidx->struct_name, expr_resolve_struct_type_name(elem_type),
						        sizeof(gidx->struct_name) - 1);
					} else if (type_is_pointer(elem_type) &&
					           type_pointee(elem_type) &&
					           type_is_struct(type_pointee(elem_type))) {
						STRNCPY(gidx->struct_name,
						        expr_resolve_struct_type_name(type_pointee(elem_type)),
						        sizeof(gidx->struct_name) - 1);
					} else {
						gidx->struct_name[0] = '\0';
					}
				}
				RETURN_IDENT_NODE(gidx);
			}

			if (is_global_array(ident_name)) {
				Global *gg = find_global(ident_name);
				if (gg && gg->array_dim_count > 1) {
					/*
					 * Multi-dimensional global array indexing is lowered here using
					 * the declaration's stored dimension table, not the TY_ARRAY chain.
					 * This avoids the stage1 self-host bug where Type.size metadata in
					 * the chained postfix path produced corrupt strides such as
					 * 20 << 24 for arr[][3][5].  Build a byte offset directly:
					 *
					 *   arr[i][j][k] -> *(base + ((i*D1*D2 + j*D2 + k) * elem_size))
					 */
					int dim = 0;
					Node *byte_index = append_byte_index(NULL, index,
					                                     global_array_stride_bytes_for_dim(gg, dim));
					dim++;

					while (dim < gg->array_dim_count && lexer_peek()->kind == TOK_LBRACKET) {
						lexer_next();
						Node *next_index = parse_expr();
						expect(TOK_RBRACKET);
						byte_index = append_byte_index(byte_index, next_index,
						                               global_array_stride_bytes_for_dim(gg, dim));
						dim++;
					}

					Node *addr = new_global_index(ident_name, byte_index, 1);
					addr->is_array_field = 1; /* byte index: produce address */
					addr->is_pointer = 1;

					if (dim >= gg->array_dim_count) {
						Node *deref = new_deref(addr);
						deref->elem_size = gg->elem_size ? gg->elem_size : 4;
						deref->type = type_for_size(deref->elem_size);
						if (gg->type && type_is_array(gg->type) && type_pointee(gg->type))
							deref->is_const_lvalue = expr_type_is_const_qualified(type_pointee(gg->type));
						RETURN_IDENT_NODE(deref);
					}

					addr->type = global_array_remaining_ptr_type(gg, dim, &addr->elem_size);
					RETURN_IDENT_NODE(addr);
				}
				Node *gidx = new_global_index(ident_name, index, global_elem_size(ident_name));
				Type *gt = global_type(ident_name);
				if (gt && type_is_array(gt) && type_pointee(gt)) {
					gidx->type = type_pointee(gt);
					gidx->is_const_lvalue = expr_type_is_const_qualified(type_pointee(gt));
				}
				RETURN_IDENT_NODE(gidx);
			}

			if (is_global_pointer(ident_name) ||
			    (global_type(ident_name) && type_is_pointer(global_type(ident_name)))) {
				Node *base = make_scalar_var_node(ident_name);
				Node *addr = new_binary(ND_ADD, base, index);
				addr->is_pointer = 1;
				addr->elem_size = base->elem_size;
				if (base->type && type_is_pointer(base->type))
					addr->type = base->type;
				RETURN_IDENT_NODE(new_deref(addr));
			}

			if (is_global(ident_name)) {
				Node *base = make_scalar_var_node(ident_name);
				if ((base->type && type_is_pointer(base->type)) || base->is_pointer) {
					Node *addr = new_binary(ND_ADD, base, index);
					addr->is_pointer = 1;
					addr->elem_size = base->elem_size;
					if (base->type && type_is_pointer(base->type))
						addr->type = base->type;
					RETURN_IDENT_NODE(new_deref(addr));
				}
			}

			if (is_array_local(ident_name)) {
				Type *arr_type = type_local(ident_name);
				int elem_size = elem_size_local(ident_name);

				Node *idx_node = new_index(ident_name, find_local(ident_name), index);
				idx_node->elem_size = elem_size;

				if (arr_type && type_is_array(arr_type) &&
				        type_pointee(arr_type) && type_is_struct(type_pointee(arr_type))) {
					idx_node->type = type_pointee(arr_type);
					idx_node->is_const_lvalue = expr_type_is_const_qualified(type_pointee(arr_type));
					STRNCPY(idx_node->struct_name, expr_resolve_struct_type_name(type_pointee(arr_type)),
					        sizeof(idx_node->struct_name) - 1);

					if (lexer_peek()->kind == TOK_DOT) {
						lexer_next();

						const Token *field = lexer_peek();
						if (field->kind != TOK_IDENT) {
							fatal_cur("Expected field name after struct array '.'\n");
						}
						lexer_next();

						const char *current_struct = expr_resolve_struct_type_name(type_pointee(arr_type));
						Field *f = find_field(current_struct, field->text);
						int field_offset = f->offset;

						while (f->is_struct && lexer_peek()->kind == TOK_DOT) {
							lexer_next();

							const Token *nested = lexer_peek();
							if (nested->kind != TOK_IDENT) {
								fatal_cur("Expected nested field name after '.'\n");
							}
							lexer_next();

							current_struct = f->struct_name;
							Field *nf = find_field(current_struct, nested->text);
							field_offset += nf->offset;
							f = nf;
						}

						Node *addr = new_addr(idx_node);
						Node *member = new_member_ptr(field->text, addr, field_offset);
						apply_field_type(member, f);
						member->is_const_lvalue = idx_node->is_const_lvalue;
						RETURN_IDENT_NODE(member);
					}

					RETURN_IDENT_NODE(idx_node);
				}

				if (arr_type && type_pointee(arr_type) && !type_is_array(type_pointee(arr_type))) {
					idx_node->type = type_pointee(arr_type);
					idx_node->is_const_lvalue = expr_type_is_const_qualified(type_pointee(arr_type));
					idx_node->elem_size = type_sizeof(idx_node->type);
				} else {
					idx_node->type = type_for_size(idx_node->elem_size);
				}

				/* For multi-dimensional arrays, the element type may itself be
				 * an array — preserve the type so a second [idx] can work.
				 * Do NOT overwrite elem_size: it must remain the ROW size (e.g. 4
				 * for char[2][4]) so that emit_addr_indexed scales correctly. */
				if (arr_type && type_pointee(arr_type) && type_is_array(type_pointee(arr_type))) {
					idx_node->type = type_pointee(arr_type);
					idx_node->is_const_lvalue = expr_type_is_const_qualified(type_pointee(arr_type));
					/* elem_size stays as row_size (elem_size_local), NOT inner elem size */
				}

				RETURN_IDENT_NODE(idx_node);
			}

			if (is_pointer_local_optional(ident_name)) {
				Local *local_info = parser_find_local_info_latest(ident_name);
				Node *base = make_scalar_var_node_resolved(ident_name, local_info, NULL);
				Node *addr = new_binary(ND_ADD, base, index);
				addr->is_pointer = 1;
				addr->elem_size = base->elem_size;
				if (base->type && type_is_pointer(base->type))
					addr->type = base->type;

				if (base->struct_name[0] && lexer_peek()->kind == TOK_DOT) {
					lexer_next();

					const Token *field = lexer_peek();
					if (field->kind != TOK_IDENT) {
						fatal_cur("Expected field name after pointer index '.'\n");
					}
					lexer_next();

					const char *current_struct = base->struct_name;
					Field *f = find_field(current_struct, field->text);
					int field_offset = f->offset;

					while (f->is_struct && lexer_peek()->kind == TOK_DOT) {
						lexer_next();

						const Token *nested = lexer_peek();
						if (nested->kind != TOK_IDENT) {
							fatal_cur("Expected nested field name after '.'\n");
						}
						lexer_next();

						current_struct = f->struct_name;
						Field *nf = find_field(current_struct, nested->text);
						field_offset += nf->offset;
						f = nf;
					}

					Node *member = new_member_ptr(field->text, addr, field_offset);
					apply_field_type(member, f);
					member->is_const_lvalue = expr_type_is_const_qualified(base->type ? type_pointee(base->type) : NULL);
					RETURN_IDENT_NODE(member);
				}

				RETURN_IDENT_NODE(new_deref(addr));
			}

			fatal_cur("Indexed object is not an array or pointer: %s\n", ident_name);
		}

		if (lexer_peek()->kind == TOK_DOT) {
			ENTER_IDENT_BUCKET(PARSER_PROF_EXPR_IDENT_DOT);
			if (!is_struct_local(ident_name) && !is_struct_by_ref_local(ident_name)) {
				fatal_cur("Member access on non-struct: %s (type %s)\n", ident_name, expr_type_display_name(type_local(ident_name)));
			}

			lexer_next();

			const Token *field = lexer_peek();
			if (field->kind != TOK_IDENT) {
				fatal_cur("Expected field name after '.'\n");
			}

			lexer_next();

			if (is_struct_by_ref_local(ident_name)) {
				Node *base = make_scalar_var_node(ident_name);
				Field *f = find_field(base->struct_name, field->text);
				Node *node = new_member_ptr(field->text, base, f->offset);
				apply_field_type(node, f);
				node->is_const_lvalue = expr_type_is_const_qualified(base->type ? type_pointee(base->type) : NULL);
				RETURN_IDENT_NODE(node);
			}

			Field *f = find_field(struct_name_local(ident_name), field->text);
			int member_offset = find_local(ident_name) + f->offset;
			char current_struct[64] = {0};
			if (f->is_struct)
				STRNCPY(current_struct, f->struct_name, sizeof(current_struct) - 1);

			while (f->is_struct && lexer_peek()->kind == TOK_DOT) {
				lexer_next();

				const Token *nested_field = lexer_peek();
				if (nested_field->kind != TOK_IDENT) {
					fatal_cur("Expected nested field name after '.'\n");
				}
				lexer_next();

				Field *nf = find_field(current_struct, nested_field->text);
				member_offset += nf->offset;
				f = nf;
				if (f->is_struct)
					STRNCPY(current_struct, f->struct_name, sizeof(current_struct) - 1);
			}

			Node *node = new_member(field->text, member_offset);
			apply_field_type(node, f);
			node->is_const_lvalue = expr_type_is_const_qualified(type_local(ident_name));
			RETURN_IDENT_NODE(node);
		}

		if (lexer_peek()->kind == TOK_ARROW) {
			ENTER_IDENT_BUCKET(PARSER_PROF_EXPR_IDENT_ARROW);
			Local *local_info = parser_find_local_info_latest(ident_name);
			Global *global_info = local_info ? NULL : parser_find_global_info(ident_name);
			int arrow_ok = 0;
			const char *arrow_struct = "";

			if (local_info && local_info->is_pointer && local_info->struct_name[0]) {
				arrow_ok = 1;
				arrow_struct = local_info->struct_name;
			} else if (local_info && local_info->is_pointer) {
				/* Pointer local without tracked struct_name — try via Type */
				Type *lt = local_info->type;
				if (lt && lt->kind == TY_PTR && lt->base &&
				    lt->base->struct_name[0]) {
					arrow_ok = 1;
					arrow_struct = lt->base->struct_name;
				}
			} else if (global_info &&
			           (((global_info->type && type_is_pointer(global_info->type)) ||
			             global_info->ptr_elem_size > 0 || global_info->is_string)) &&
			           global_info->struct_name[0]) {
				arrow_ok = 1;
				arrow_struct = global_info->struct_name;
			} else if (global_info && global_info->is_array && global_info->is_struct &&
			           global_info->struct_name[0]) {
				/*
				 * C array-to-pointer decay also applies before ->.
				 * For a global array of ptab.structs, e.g.
				 *
				 *     PT cases[];
				 *     sizeof(cases->c)
				 *
				 * treat cases as &cases[0], so cases->c is
				 * equivalent to (&cases[0])->c.
				 */
				arrow_ok = 1;
				arrow_struct = global_info->struct_name;
			} else if (local_info && local_info->is_array) {
				Type *lt = local_info->type;
				if (lt && lt->kind == TY_ARRAY && lt->base &&
				    type_is_struct(lt->base) && expr_resolve_struct_type_name(lt->base)[0]) {
					arrow_ok = 1;
					arrow_struct = expr_resolve_struct_type_name(lt->base);
				}
			}

			if (!arrow_ok) {
				Type *diag_type = local_info ? local_info->type : (global_info ? global_info->type : NULL);
				fatal_cur("Arrow access on non-struct pointer: %s (type %s)\n", ident_name, expr_type_display_name(diag_type));
			}

			lexer_next();

			const Token *field = lexer_peek();
			if (field->kind != TOK_IDENT) {
				fatal_cur("Expected field name after '->'\n");
			}

			lexer_next();

			Node *base = make_scalar_var_node_resolved(ident_name, local_info, global_info);
			const char *current_struct = arrow_struct[0] ? arrow_struct : base->struct_name;
			Field *f = find_field(current_struct, field->text);
			int field_offset = f->offset;

			while (f->is_struct && lexer_peek()->kind == TOK_DOT) {
				lexer_next();

				const Token *nested_field = lexer_peek();
				if (nested_field->kind != TOK_IDENT) {
					fatal_cur("Expected nested field name after '.'\n");
				}
				lexer_next();

				current_struct = f->struct_name;
				Field *nf = find_field(current_struct, nested_field->text);
				field_offset += nf->offset;
				f = nf;
			}

			Node *node = new_member_ptr(field->text, base, field_offset);
			apply_field_type(node, f);
			node->is_const_lvalue = expr_type_is_const_qualified(base->type ? type_pointee(base->type) : NULL);
			RETURN_IDENT_NODE(node);
		}

		{
			ENTER_IDENT_BUCKET(PARSER_PROF_EXPR_IDENT_VALUE);
			int enum_value = 0;
			if (parser_find_enum_const(ident_name, &enum_value))
				RETURN_IDENT_NODE(new_num(enum_value));
		}

		RETURN_IDENT_NODE(make_var_node(ident_name));
	}

	if (token->kind == TOK_LPAREN) {
		parser_profile_scope_enter(PARSER_PROF_EXPR_PAREN);
		if (lexer_peek_ahead(1)->kind == TOK_LBRACE)
			RETURN_PAREN_NODE(parse_statement_expression());

		lexer_next();

		if (is_type_start_token(lexer_peek()->kind, lexer_peek()->text)) {
			Type *type = parse_type_name();
			int dims[MAX_ARRAY_DIMS];
			int dim_count = 0;

			/* Accept GNU attributes and abstract function-pointer declarators
			 * inside casts, for example:
			 *   ((ATTR int (*)(void)) p)()
			 *   ((int (ATTR *)(void)) p)()
			 * The attributes are syntax-only for now; the result type is a
			 * pointer-sized function pointer.
			 */
			skip_inline_qualifiers();
			if (lexer_peek()->kind == TOK_LPAREN)
				try_parse_abstract_function_pointer_declarator(&type);
			skip_inline_qualifiers();
			if (lexer_peek()->kind == TOK_LBRACKET) {
				dim_count = parse_array_dimensions(dims, 1, 0);
				type = build_array_type_from_dims_allow_incomplete(clone_type(type),
				                                                   dims, dim_count, 1);
			}
			expect(TOK_RPAREN);
			if (lexer_peek()->kind == TOK_LBRACE && tcc_lang_is_c89_or_c90())
				fatal_cur("compound literals are not allowed in C89/C90 mode\n");
			if (lexer_peek()->kind == TOK_LBRACE) {
				if (type_is_struct(type) || type_is_union(type))
					RETURN_PAREN_NODE(parse_aggregate_compound_literal_expr(type));
				if (type_is_array(type))
					RETURN_PAREN_NODE(parse_array_compound_literal_expr(type));
				RETURN_PAREN_NODE(parse_scalar_compound_literal(type));
			}
			Node *operand = parse_unary();
			if (type_is_complex(type) && expr_node_has_imaginary_value(operand))
				RETURN_PAREN_NODE(expr_build_complex_from_imaginary(operand, type));
			if (type_is_complex(type) && expr_node_is_real_scalar(operand))
				RETURN_PAREN_NODE(expr_build_complex_value_cast(operand, type));
			if (type_is_complex(type) && expr_node_has_complex_value(operand))
				RETURN_PAREN_NODE(expr_build_complex_complex_cast(operand, type));
			if (type_is_imaginary(type) && expr_node_has_complex_value(operand))
				RETURN_PAREN_NODE(expr_build_complex_imaginary_extract_cast(operand, type));
			if (expr_node_has_complex_value(operand) &&
			    expr_type_is_real_scalar_type(type))
				RETURN_PAREN_NODE(expr_build_complex_real_extract_cast(operand, type));
			if (expr_node_has_imaginary_value(operand) &&
			    expr_type_is_real_scalar_type(type))
				RETURN_PAREN_NODE(expr_build_imaginary_real_extract_cast(operand, type));
			validate_cast_operand(type, operand);
			RETURN_PAREN_NODE(new_cast(operand, type));
		}

		/*
		 * Parenthesized expressions may contain the comma operator.  Keep
		 * parse_expr() itself as an assignment-expression because callers such
		 * as parse_arg_list() use commas as separators, not operators.
		 */
		Node *node = parse_assignment();
		while (lexer_peek()->kind == TOK_COMMA) {
			lexer_next();
			Node *rhs = parse_assignment();
			node = new_binary(ND_COMMA, node, rhs);
		}
		expect(TOK_RPAREN);
		RETURN_PAREN_NODE(node);
	}

	fatal_token(token, "Expected expression factor");
#undef RETURN_PAREN_NODE
#undef RETURN_IDENT_NODE
#undef ENTER_IDENT_BUCKET
#undef RETURN_FACTOR_NODE
	return 0;
}

Node *
parse_postfix(void)
{
	Node *node;

	parser_profile_scope_enter(PARSER_PROF_EXPR_POSTFIX);
	node = parse_factor();

	for (;;) {
		TokenKind op = lexer_peek()->kind;

		if (op == TOK_LBRACKET) {
			parser_profile_scope_enter(PARSER_PROF_EXPR_INDEX);
			lexer_next();
			Node *index = parse_expr();
			expect(TOK_RBRACKET);

			if (node->type && type_is_array(node->type) && type_pointee(node->type)) {
				/*
				 * Keep this path deliberately simple for the self-hosted compiler:
				 * older stage1 builds have miscompiled a local Type *elem_type in
				 * this large function, corrupting elem_size and therefore array
				 * indexing strides.  Use node->type->base directly instead.
				 */
				Node *base_target = node;
				Node *base;

				if (node->kind == ND_COMMA && node->right)
					base_target = node->right;

				base = new_addr(base_target);
				base->is_pointer = 1;
				base->elem_size = type_sizeof(type_pointee(node->type));
				base->type = type_ptr(type_pointee(node->type));

				Node *addr = new_binary(ND_ADD, base, index);
				addr->is_pointer = 1;
				addr->elem_size = type_sizeof(type_pointee(node->type));
				addr->type = type_ptr(type_pointee(node->type));

				{
					Node *indexed = new_deref(addr);
					indexed->type = type_pointee(addr->type);
					indexed->elem_size = indexed->type ? type_sizeof(indexed->type) : addr->elem_size;
					indexed->is_const_lvalue = expr_type_is_const_qualified(indexed->type);
					if (indexed->type && type_is_struct(indexed->type))
						STRNCPY(indexed->struct_name, expr_resolve_struct_type_name(indexed->type),
						        sizeof(indexed->struct_name) - 1);
					if (node->kind == ND_COMMA && node->right) {
						node = new_binary(ND_COMMA, node->left, indexed);
						node->type = indexed->type;
						node->elem_size = indexed->elem_size;
						node->is_const_lvalue = indexed->is_const_lvalue;
						if (indexed->struct_name[0])
							STRNCPY(node->struct_name, indexed->struct_name,
							        sizeof(node->struct_name) - 1);
						else
							node->struct_name[0] = '\0';
					} else {
						node = indexed;
					}
				}
				parser_profile_scope_leave(PARSER_PROF_EXPR_INDEX);
				continue;
			}

			if ((node->type && type_is_pointer(node->type) && type_pointee(node->type)) || node->is_pointer) {
				Type *ptr_base_type = (node->type && type_is_pointer(node->type) && type_pointee(node->type))
				                      ? type_pointee(node->type)
				                      : type_for_size(node->elem_size ? node->elem_size : 4);

				Node *addr = new_binary(ND_ADD, node, index);
				addr->is_pointer = 1;
				addr->elem_size = ptr_base_type->size;
				addr->type = type_ptr(ptr_base_type);

				node = new_deref(addr);
				node->type = type_pointee(addr->type);
				node->elem_size = node->type ? type_sizeof(node->type) : addr->elem_size;
				node->is_const_lvalue = expr_type_is_const_qualified(node->type);
				if (node->type && type_is_struct(node->type))
					STRNCPY(node->struct_name, expr_resolve_struct_type_name(node->type),
					        sizeof(node->struct_name) - 1);
				parser_profile_scope_leave(PARSER_PROF_EXPR_INDEX);
				continue;
			}

			/*
			 * Fallback for (*ptr)[n] where ptr is a pointer-to-pointer.
			 * In self-hosted (stage1) compilation the Type* chain on the
			 * ND_DEREF node may not survive intact, so the TY_PTR check
			 * above fails even though the deref of a T** is clearly a
			 * pointer.  The ast.c fix sets is_pointer=1 for future stage0
			 * builds; this fallback covers the current stage1 binary by
			 * checking the structural facts directly: a ND_DEREF node
			 * whose operand was a pointer (is_pointer or 8-byte elem_size)
			 * produces a pointer result that can be indexed.
			 */
			if (node->kind == ND_DEREF && node->left &&
			        (node->left->is_pointer || node->left->elem_size == 8)) {
				int elem = node->elem_size ? node->elem_size : 1;
				Node *addr = new_binary(ND_ADD, node, index);
				addr->is_pointer = 1;
				addr->elem_size = elem;
				node = new_deref(addr);
				node->elem_size = elem;
				parser_profile_scope_leave(PARSER_PROF_EXPR_INDEX);
				continue;
			}

			parser_profile_scope_leave(PARSER_PROF_EXPR_INDEX);
			fatal_cur("Indexed expression is not an array or pointer\n");
		}

		if (op == TOK_DOT) {
			parser_profile_scope_enter(PARSER_PROF_EXPR_DOT);
			const char *dot_struct_name = "";
			if (node->type && (type_is_struct(node->type) || type_is_union(node->type)))
				dot_struct_name = expr_resolve_struct_type_name(node->type);
			else if (node->struct_name[0])
				dot_struct_name = node->struct_name;

			if (!dot_struct_name || !dot_struct_name[0]) {
				fatal_cur("Member access on non-struct expression (type %s)\n", expr_node_type_display_name(node));
			}

			lexer_next();

			const Token *field = lexer_peek();
			if (field->kind != TOK_IDENT) {
				fatal_cur("Expected field name after '.'\n");
			}
			lexer_next();

			Field *f = find_field(dot_struct_name, field->text);

			if (node->kind == ND_VAR || node->kind == ND_MEMBER) {
				int base_const = node->is_const_lvalue;
				node = new_member(field->text, node->offset + f->offset);
				node->is_const_lvalue = base_const;
			} else if (node->kind == ND_COMMA && node->right) {
				Node *member;

				if (node->right->kind == ND_VAR || node->right->kind == ND_MEMBER) {
					member = new_member(field->text, node->right->offset + f->offset);
					member->is_const_lvalue = node->right->is_const_lvalue;
				} else {
					Node *base = new_addr(node->right);
					member = new_member_ptr(field->text, base, f->offset);
					member->is_const_lvalue = node->right->is_const_lvalue;
				}

				apply_field_type(member, f);

				node = new_binary(ND_COMMA, node->left, member);
				node->type = member->type;
				node->elem_size = member->elem_size;
				node->is_const_lvalue = member->is_const_lvalue;
				if (member->struct_name[0])
					STRNCPY(node->struct_name, member->struct_name, sizeof(node->struct_name) - 1);
				else
					node->struct_name[0] = '\0';
				continue;
			} else if (node->kind == ND_CALL && node->returns_struct) {
				/* call().field — spill call result to temp, access field on temp.
				 * Emit as: (temp = call(), temp.field) via ND_COMMA. */
				char temp_name[64];
				snprintf(temp_name, sizeof(temp_name), "__struct_dot_%d", parser_alloc_struct_arg_temp_id());
				const char *sname = node->type ? expr_resolve_struct_type_name(node->type) : "";
				if (!sname[0]) sname = node->return_struct_name;
				StructDef *def = find_struct(sname);
				int sz = def ? def->size : 8;
				int temp_off = add_struct_local(temp_name, sname);

				Node *lhs = new_var(temp_name, temp_off);
				lhs->type = type_struct(sname, sz);
				lhs->elem_size = sz;
				STRNCPY(lhs->struct_name, sname, sizeof(lhs->struct_name) - 1);

				Node *assign = new_assign(lhs, node);

				Node *member = new_member(field->text, temp_off + f->offset);
				apply_field_type(member, f);

				/* ND_COMMA: eval assign as side-effect, result is member */
				node = new_binary(ND_COMMA, assign, member);
				node->type = member->type;
				node->elem_size = member->elem_size;
				continue;
			} else {
				Node *base = new_addr(node);
				node = new_member_ptr(field->text, base, f->offset);
				node->is_const_lvalue = base->left ? base->left->is_const_lvalue : 0;
			}

			apply_field_type(node, f);
			if (node->kind == ND_MEMBER_PTR && node->left && node->left->kind == ND_ADDR && node->left->left)
				node->is_const_lvalue = node->left->left->is_const_lvalue;
			parser_profile_scope_leave(PARSER_PROF_EXPR_DOT);
			continue;
		}

		if (op == TOK_ARROW) {
			parser_profile_scope_enter(PARSER_PROF_EXPR_ARROW);
			const char *struct_name = "";
			if (node->type && type_is_pointer(node->type) && type_pointee(node->type) &&
			        type_is_struct(type_pointee(node->type)))
				struct_name = expr_resolve_struct_type_name(type_pointee(node->type));
			else if (node->struct_name[0])
				struct_name = node->struct_name;
			else if (node->type && (type_is_struct(node->type) || type_is_union(node->type)))
				struct_name = expr_resolve_struct_type_name(node->type);
			else {
				/* Try to determine struct from field name by peeking ahead */
				const Token *peek_field = lexer_peek_ahead(1); /* token after '->' */
				if (peek_field->kind == TOK_IDENT) {
					/* Search all known ptab.structs for this field */
					for (int si = 0; si < parser_struct_count(); si++) {
						StructDef *candidate = parser_struct_at(si);
						if (!candidate)
							continue;
						for (int fi2 = 0; fi2 < candidate->field_count; fi2++) {
							if (STRCMP(candidate->fields[fi2].name, peek_field->text) == 0) {
								struct_name = candidate->name;
								goto found_struct;
							}
						}
					}
				}
				fatal_cur("Arrow access on non-struct pointer expression (type %s)\n", expr_node_type_display_name(node));
found_struct:;
			}

			lexer_next();

			const Token *field = lexer_peek();
			if (field->kind != TOK_IDENT) {
				fatal_cur("Expected field name after '->'\n");
			}
			lexer_next();

			Field *f = find_field(struct_name, field->text);
			node = new_member_ptr(field->text, node, f->offset);
			apply_field_type(node, f);
			node->is_const_lvalue = expr_type_is_const_qualified(node->left && node->left->type
			                                                    ? type_pointee(node->left->type)
			                                                    : NULL);
			parser_profile_scope_leave(PARSER_PROF_EXPR_ARROW);
			continue;
		}

		if (op == TOK_PLUSPLUS || op == TOK_MINUSMINUS) {
			lexer_next();
			node = make_incdec(node, op, 1);
			continue;
		}

			if (op == TOK_LPAREN) {
				parser_profile_scope_enter(PARSER_PROF_EXPR_CALL);
				/* indirect call through a function pointer expression, e.g. cg->emit_fn() */
				Node *callee = node;
				Type *func_type = NULL;
				if (callee->kind == ND_DEREF && callee->type &&
			    type_is_function(callee->type) && callee->left) {
				/*
				 * Calling through (*fp)(...) does not dereference function
				 * code as data when the dereference result is itself a
				 * function type. Keep real data loads intact for cases like
				 * (*pp)(...), where pp is pointer-to-pointer-to-function.
					 */
					callee = callee->left;
				}
				if (callee->type && type_is_function(callee->type))
					func_type = callee->type;
				else if (callee->type && type_is_pointer(callee->type) &&
				         type_pointee(callee->type) &&
				         type_is_function(type_pointee(callee->type)))
					func_type = type_pointee(callee->type);
				lexer_next();
				Node *args = parse_arg_list_for_type(func_type, NULL);
				expect(TOK_RPAREN);
				Node *icall = new_indirect_call(callee, args);
				if (func_type && func_type->base)
					expr_apply_call_result_type(icall, func_type->base);
				node = icall;
				parser_profile_scope_leave(PARSER_PROF_EXPR_CALL);
			continue;
		}

		parser_profile_scope_leave(PARSER_PROF_EXPR_POSTFIX);
		return node;
	}
}

static int
type_has_incomplete_aggregate(const Type *type)
{
	StructDef *def;

	if (!type)
		return 0;
	if (type_is_array(type))
		return type_has_incomplete_aggregate(type->base);
	if (!type_is_struct(type))
		return 0;
	def = find_struct_or_null(type->struct_name);
	return !def || def->size <= 0;
}

static void
validate_cast_operand(Type *dst_type, Node *expr)
{
	Type *src_type = expr ? expr->type : NULL;
	Type *dst_ptr_type;
	Type *src_ptr_type;
	int dst_complex = expr_type_contains_source_kind(dst_type, TYPE_SOURCE_COMPLEX);
	int src_complex = expr_type_contains_source_kind(src_type, TYPE_SOURCE_COMPLEX);

	if (!dst_type || !src_type)
		return;
	if (type_is_void(dst_type))
		return;
	dst_ptr_type = expr_pointer_context_type(dst_type);
	src_ptr_type = expr_pointer_context_type(src_type);
	if (dst_complex || src_complex) {
		int allow_pointer_cast =
		    type_is_pointer(dst_ptr_type) &&
		    type_is_pointer(src_ptr_type) &&
		    (expr_type_is_pointer_to_complex_object(dst_type) ||
		     expr_type_is_pointer_to_complex_object(src_type));

		if (!allow_pointer_cast)
			fatal_cur("complex types are not supported\n");
	}
	if ((type_is_struct(dst_type) || type_is_union(dst_type)) &&
	    (type_is_struct(src_type) || type_is_union(src_type)) &&
	    type_equal_unqualified(dst_type, src_type))
		return;
	if (type_is_struct(dst_type) || type_is_union(dst_type) ||
	    type_is_array(dst_type) || type_is_function(dst_type))
		fatal_cur("Invalid cast target type\n");
	if (type_is_struct(src_type) || type_is_union(src_type))
		fatal_cur("Invalid cast operand type\n");

	if (type_is_pointer(dst_ptr_type) && type_is_pointer(src_ptr_type)) {
		Type *dst_base = type_pointee(dst_ptr_type);
		Type *src_base = type_pointee(src_ptr_type);
		int dst_is_func = dst_base && type_is_function(dst_base);
		int src_is_func = src_base && type_is_function(src_base);

		if (tcc_iso_diagnostics && dst_is_func != src_is_func)
			fatal_cur("Invalid cast between function and object pointer\n");
	}
}

static Node *
parse_scalar_compound_literal(Type *type)
{
	int brace_depth = 0;
	Node *value;

	if (!type_is_scalar(type) && !type_is_complex(type))
		fatal_cur("Only scalar and complex compound literals are supported in expressions\n");

	expect(TOK_LBRACE);
	brace_depth = 1;
	while (lexer_peek()->kind == TOK_LBRACE) {
		brace_depth++;
		lexer_next();
	}

	if (lexer_peek()->kind == TOK_RBRACE)
		fatal_cur("Expected initializer in compound literal\n");

	value = parse_assignment();
	while (brace_depth-- > 0) {
		if (lexer_peek()->kind == TOK_COMMA) {
			lexer_next();
			if (lexer_peek()->kind != TOK_RBRACE)
				fatal_cur("Too many initializers in scalar compound literal\n");
		}
		expect(TOK_RBRACE);
	}

	return expr_coerce_value_for_type(value, type);
}

static Node *
expr_parse_array_compound_literal_leaf_value(Type *type)
{
	Node *value;

	if (lexer_peek()->kind == TOK_LBRACE)
		return parse_scalar_compound_literal(type);

	value = parse_assignment();
	validate_cast_operand(type, value);
	if (value->type && !type_equal_unqualified(value->type, type))
		value = new_cast(value, type);
	return value;
}

Node *
expr_coerce_value_for_type(Node *value, Type *type)
{
	if (!value || !type)
		return value;
	if (value->type && type_equal_unqualified(value->type, type))
		return value;

	if (type_is_complex(type) && expr_node_has_imaginary_value(value))
		return expr_build_complex_from_imaginary(value, type);
	if (type_is_complex(type) && expr_node_is_real_scalar(value))
		return expr_build_complex_value_cast(value, type);
	if (type_is_complex(type) && expr_node_has_complex_value(value))
		return expr_build_complex_complex_cast(value, type);
	if (type_is_imaginary(type) && expr_node_has_complex_value(value))
		return expr_build_complex_imaginary_extract_cast(value, type);
	if (expr_node_has_complex_value(value) &&
	    expr_type_is_real_scalar_type(type))
		return expr_build_complex_real_extract_cast(value, type);
	if (expr_node_has_imaginary_value(value) &&
	    expr_type_is_real_scalar_type(type))
		return expr_build_imaginary_real_extract_cast(value, type);

	validate_cast_operand(type, value);
	if (value->type && !type_equal_unqualified(value->type, type))
		value = new_cast(value, type);
	return value;
}

static int
expr_try_parse_local_array_designator(int *out_lo, int *out_hi)
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

static Node *
expr_array_leaf_lvalue(const char *name, int offset, Type *array_type, int flat_index)
{
	Type *elem_type = expr_array_leaf_elem_type(array_type);
	Node *lhs = new_index(name, offset, new_num(flat_index));

	lhs->type = clone_type(elem_type);
	lhs->elem_size = type_sizeof(elem_type);
	lhs->is_pointer = type_is_pointer(elem_type);
	if (type_is_pointer(elem_type) &&
	    type_pointee(elem_type) &&
	    type_is_struct(type_pointee(elem_type))) {
		STRNCPY(lhs->struct_name, type_pointee(elem_type)->struct_name,
		        sizeof(lhs->struct_name) - 1);
	}
	return lhs;
}

static void
expr_array_init_reserve(int needed, int *cap, Node ***exprs, unsigned char **seen)
{
	int old_cap = *cap;
	int new_cap = old_cap ? old_cap : 8;

	if (needed <= old_cap)
		return;
	while (new_cap < needed)
		new_cap *= 2;

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

static int
expr_array_flat_len(Type *type)
{
	int count = 1;

	for (Type *t = type; t && t->kind == TY_ARRAY; t = t->base) {
		if (t->array_len <= 0)
			return count;
		count *= t->array_len;
	}

	return count;
}

static Type *
expr_array_leaf_elem_type(Type *type)
{
	Type *t = type;

	while (t && t->kind == TY_ARRAY)
		t = parser_canonicalize_decl_type(t->base);
	return t ? t : type;
}

static void
expr_copy_array_init_span(int dst_base_index, int span_len,
                          int *dst_exprs_cap, Node ***dst_exprs,
                          unsigned char **dst_seen,
                          Node **src_exprs, int src_exprs_cap,
                          const unsigned char *src_seen, int src_seen_cap,
                          int *max_init_index, int *init_count)
{
	for (int i = 0; i < span_len; i++) {
		int dst_index = dst_base_index + i;
		int present = src_seen && i < src_seen_cap && src_seen[i];

		if (!present)
			continue;

		expr_array_init_reserve(dst_index + 1, dst_exprs_cap, dst_exprs, dst_seen);
		(*dst_exprs)[dst_index] =
		    (src_exprs && i < src_exprs_cap && src_exprs[i])
		        ? clone_node_tree(src_exprs[i])
		        : NULL;
		(*dst_seen)[dst_index] = 1;
		if (dst_index > *max_init_index)
			*max_init_index = dst_index;
	}

	*init_count = *max_init_index + 1;
}

static void
expr_parse_multidim_array_compound_literal_initializer(Type *array_type,
                                                       int base_index,
                                                       int *init_exprs_cap,
                                                       Node ***init_exprs,
                                                       unsigned char **init_seen,
                                                       int *max_init_index,
                                                       int *init_count)
{
	int braced = 0;
	Type *elem_type;
	int elem_span;
	int next_init_index = 0;

	if (!array_type || array_type->kind != TY_ARRAY)
		fatal_cur("internal error: expected array type in compound literal\n");

	elem_type = parser_canonicalize_decl_type(array_type->base);
	elem_span = expr_array_flat_len(array_type->base);

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

		has_designator = expr_try_parse_local_array_designator(&lo, &hi);
		if (lo < 0)
			fatal_cur("Array designator index out of range\n");
		if (hi < lo)
			fatal_cur("Invalid array designator range\n");
		if (array_type->array_len > 0 && hi >= array_type->array_len && has_designator)
			fatal_cur("Array designator index out of range\n");
		if (array_type->array_len > 0 && hi >= array_type->array_len)
			fatal_cur("Too many initializers in array compound literal\n");

		if (elem_type && elem_type->kind == TY_ARRAY) {
			int tmp_cap = 0;
			int tmp_init_count = 0;
			int tmp_max_init_index = -1;
			Node **tmp_exprs = NULL;
			unsigned char *tmp_seen = NULL;

			expr_parse_multidim_array_compound_literal_initializer(elem_type,
			                                                       0,
			                                                       &tmp_cap,
			                                                       &tmp_exprs,
			                                                       &tmp_seen,
			                                                       &tmp_max_init_index,
			                                                       &tmp_init_count);
			for (int di = lo; di <= hi; di++) {
				expr_copy_array_init_span(base_index + di * elem_span, elem_span,
				                          init_exprs_cap, init_exprs,
				                          init_seen,
				                          tmp_exprs, tmp_cap,
				                          tmp_seen, tmp_max_init_index + 1,
				                          max_init_index, init_count);
			}

			xfree(tmp_exprs);
			xfree(tmp_seen);
		} else {
			Node *value = expr_parse_array_compound_literal_leaf_value(elem_type);

			expr_array_init_reserve(base_index + hi * elem_span + 1,
			                        init_exprs_cap, init_exprs, init_seen);
			for (int di = lo; di <= hi; di++) {
				int dst_index = base_index + di * elem_span;

				(*init_exprs)[dst_index] = (di == lo)
				                         ? value : clone_node_tree(value);
				(*init_seen)[dst_index] = 1;
				if (dst_index > *max_init_index)
					*max_init_index = dst_index;
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
parse_aggregate_compound_literal_expr(Type *type)
{
	char temp_name[64];
	const char *struct_name;
	StructDef *def;
	Node *head;
	Node *temp_ref;
	Node *expr;
	int temp_off;

	if (!type_is_struct(type) && !type_is_union(type))
		fatal_cur("Only aggregate compound literals are supported in expressions\n");

	struct_name = expr_resolve_struct_type_name(type);
	if (!struct_name)
		fatal_cur("Anonymous aggregate compound literals are not supported in expressions\n");

	def = find_struct(struct_name);
	if (!def)
		fatal_cur("Unknown aggregate '%s' in compound literal\n", struct_name);

	expect(TOK_LBRACE);

	snprintf(temp_name, sizeof(temp_name), "__compound_agg_%d",
	         parser_alloc_struct_arg_temp_id());
	temp_off = add_struct_local(temp_name, struct_name);
	head = append_local_zero_fill(NULL, temp_name, temp_off, def->size);
	head = parse_struct_initializer_values(def, struct_name, temp_off, head);
	parser_expect_local_aggregate_initializer_close(def);

	temp_ref = new_var(temp_name, temp_off);
	temp_ref->type = def->is_union ? type_union(struct_name, def->size)
	                               : type_struct(struct_name, def->size);
	snprintf(temp_ref->struct_name, sizeof(temp_ref->struct_name), "%s",
	         struct_name);
	temp_ref->elem_size = def->size;

	expr = new_binary(ND_COMMA, new_block(head), temp_ref);
	expr->type = def->is_union ? type_union(struct_name, def->size)
	                           : type_struct(struct_name, def->size);
	snprintf(expr->struct_name, sizeof(expr->struct_name), "%s",
	         struct_name);
	expr->elem_size = def->size;
	return expr;
}

static Node *
expr_build_imaginary_multiplicative(Node *left, Node *right, NodeKind kind)
{
	Type *left_real_type;
	Type *right_real_type;
	Type *result_real_type;
	Type *result_imag_type;
	Node *left_real;
	Node *right_real;
	Node *expr;

	if (!left || !right || !left->type || !right->type)
		return NULL;
	if (!(kind == ND_MUL || kind == ND_DIV))
		return NULL;

	left_real_type = type_is_imaginary(left->type)
	               ? expr_imaginary_real_component_type(left->type)
	               : left->type;
	right_real_type = type_is_imaginary(right->type)
	                ? expr_imaginary_real_component_type(right->type)
	                : right->type;
	if (!left_real_type || !right_real_type)
		return NULL;

	if (!expr_node_is_real_scalar(left) && !expr_node_has_imaginary_value(left))
		return NULL;
	if (!expr_node_is_real_scalar(right) && !expr_node_has_imaginary_value(right))
		return NULL;

	result_real_type = expr_fp_usual_arith_type(left_real_type, right_real_type);
	if (!result_real_type || !type_is_fp_scalar(result_real_type))
		return NULL;

	left_real = left;
	if (!type_equal_unqualified(left->type, left_real_type))
		left_real = new_cast(left, left_real_type);
	right_real = right;
	if (!type_equal_unqualified(right->type, right_real_type))
		right_real = new_cast(right, right_real_type);

	if (expr_node_has_imaginary_value(left) && expr_node_has_imaginary_value(right)) {
		expr = new_binary(kind, left_real, right_real);
		if (kind == ND_MUL)
			expr = new_unary(ND_NEG, expr);
		return expr;
	}

	if (expr_node_has_imaginary_value(left) && expr_node_is_real_scalar(right)) {
		expr = new_binary(kind, left_real, right_real);
		result_imag_type = expr_make_imaginary_type_from_real(result_real_type);
		return result_imag_type ? new_cast(expr, result_imag_type) : NULL;
	}

	if (expr_node_is_real_scalar(left) && expr_node_has_imaginary_value(right)) {
		expr = new_binary(kind, left_real, right_real);
		if (kind == ND_DIV)
			expr = new_unary(ND_NEG, expr);
		result_imag_type = expr_make_imaginary_type_from_real(result_real_type);
		return result_imag_type ? new_cast(expr, result_imag_type) : NULL;
	}

	return NULL;
}

static Node *
parse_array_compound_literal_expr(Type *type)
{
	char temp_name[64];
	Type *elem_type;
	Type *leaf_elem_type;
	Type *array_type = NULL;
	Node *head;
	Node *temp_ref;
	Node *expr;
	Node **init_exprs = NULL;
	unsigned char *init_seen = NULL;
	int init_cap = 0;
	int init_count = 0;
	int array_len;
	int total_len;
	int temp_off;
	int elem_size;
	int leaf_elem_size;
	int dim_count = 0;
	int max_init_index = -1;
	int i;

	if (!type_is_array(type))
		fatal_cur("Only array compound literals are supported in expressions\n");

	elem_type = type_pointee(type);
	leaf_elem_type = expr_array_leaf_elem_type(type);
	if (!leaf_elem_type || !type_is_scalar(leaf_elem_type))
		fatal_cur("Only scalar array compound literals are supported in expressions\n");

	for (Type *t = type; t && t->kind == TY_ARRAY; t = t->base)
		dim_count++;

	if (dim_count > 1) {
		expr_parse_multidim_array_compound_literal_initializer(type, 0,
		                                                       &init_cap, &init_exprs,
		                                                       &init_seen,
		                                                       &max_init_index,
		                                                       &init_count);
	} else {
		expect(TOK_LBRACE);
		reject_empty_initializer_before_c23();

		while (lexer_peek()->kind != TOK_RBRACE) {
			int lo = init_count;
			int hi = init_count;
			int has_designator;
			Node *value;

			has_designator = expr_try_parse_local_array_designator(&lo, &hi);
			if (lo < 0)
				fatal_cur("Array designator index out of range\n");
			if (hi < lo)
				fatal_cur("Invalid array designator range\n");
			if (type->array_len > 0 && hi >= type->array_len && has_designator)
				fatal_cur("Array designator index out of range\n");
			if (type->array_len > 0 && hi >= type->array_len)
				fatal_cur("Too many initializers in array compound literal\n");

			value = expr_parse_array_compound_literal_leaf_value(leaf_elem_type);
			expr_array_init_reserve(hi + 1, &init_cap, &init_exprs, &init_seen);
			for (int di = lo; di <= hi; di++) {
				init_exprs[di] = (di == lo) ? value : clone_node_tree(value);
				init_seen[di] = 1;
			}
			if (hi > max_init_index)
				max_init_index = hi;
			init_count = max_init_index + 1;

			if (lexer_peek()->kind == TOK_COMMA) {
				lexer_next();
				if (lexer_peek()->kind == TOK_RBRACE)
					break;
			} else {
				break;
			}
		}

		expect(TOK_RBRACE);
	}

	array_len = type->array_len;
	if (array_len <= 0)
		array_len = init_count;
	if (array_len <= 0)
		fatal_cur("Array compound literal length must be positive\n");

	array_type = type_array(clone_type(elem_type), array_len);
	total_len = expr_array_flat_len(array_type);
	if (init_count > total_len)
		fatal_cur("Too many initializers in array compound literal\n");
	elem_size = type_sizeof(elem_type);
	leaf_elem_size = type_sizeof(leaf_elem_type);

	snprintf(temp_name, sizeof(temp_name), "__compound_array_%d",
	         parser_alloc_compound_arg_temp_id());
	temp_off = add_typed_local(temp_name, array_type);
	head = append_local_zero_fill(NULL, temp_name, temp_off, array_len * elem_size);

	for (i = 0; i < total_len; i++) {
		if (!init_seen || !init_seen[i])
			continue;
		Node *lhs = expr_array_leaf_lvalue(temp_name, temp_off, array_type, i);
		lhs->elem_size = leaf_elem_size;
		head = append_node(head, new_assign(lhs, init_exprs[i]));
	}

	temp_ref = new_var(temp_name, temp_off);
	temp_ref->type = array_type;
	temp_ref->elem_size = elem_size;

	expr = new_binary(ND_COMMA, new_block(head), temp_ref);
	expr->type = array_type;
	expr->elem_size = elem_size;

	xfree(init_seen);
	xfree(init_exprs);
	return expr;
}

static Node *
parse_compound_literal_address(Type *type)
{
	Node *value;
	Node *addr_expr;

	if (type_is_scalar(type) || type_is_complex(type))
		return parse_scalar_compound_literal_address(type);
	if (type_is_struct(type) || type_is_union(type))
		value = parse_aggregate_compound_literal_expr(type);
	else if (type_is_array(type))
		value = parse_array_compound_literal_expr(type);
	else
		fatal_cur("Unsupported compound literal address type\n");

	if (value && value->kind == ND_COMMA && value->right) {
		addr_expr = new_binary(ND_COMMA, value->left, new_addr(value->right));
		addr_expr->type = type_ptr(value->right->type);
		addr_expr->elem_size = value->right->elem_size;
		addr_expr->is_pointer = 1;
		if (value->right->struct_name[0])
			STRNCPY(addr_expr->struct_name, value->right->struct_name,
			        sizeof(addr_expr->struct_name) - 1);
		return addr_expr;
	}

	return new_addr(value);
}

static Node *
parse_scalar_compound_literal_address(Type *type)
{
	char temp_name[64];
	int offset;
	Node *value;
	Node *lhs;
	Node *temp;
	Node *assign;

	value = parse_scalar_compound_literal(type);
	if (value && value->kind == ND_COMMA && value->right) {
		Node *addr_expr = new_binary(ND_COMMA, value->left, new_addr(value->right));
		addr_expr->type = type_ptr(value->right->type);
		addr_expr->elem_size = value->right->elem_size;
		addr_expr->is_pointer = 1;
		if (value->right->struct_name[0])
			STRNCPY(addr_expr->struct_name, value->right->struct_name,
			        sizeof(addr_expr->struct_name) - 1);
		return addr_expr;
	}

	snprintf(temp_name, sizeof(temp_name), "__compound_scalar_%d",
	         parser_alloc_compound_arg_temp_id());
	offset = add_typed_local(temp_name, type);

	lhs = new_var(temp_name, offset);
	lhs->type = clone_type(type);
	lhs->elem_size = type_sizeof(type);
	lhs->is_pointer = type_is_pointer(type);

	temp = new_var(temp_name, offset);
	temp->type = clone_type(type);
	temp->elem_size = type_sizeof(type);
	temp->is_pointer = type_is_pointer(type);

	assign = new_assign(lhs, value);
	return new_binary(ND_COMMA, assign, new_addr(temp));
}

static void
validate_sizeof_operand_type(Type *type)
{
	if (type_is_void(type))
		fatal_cur("sizeof cannot be applied to void type\n");
	if (type_is_function(type))
		fatal_cur("sizeof cannot be applied to function type\n");
	if (type_has_incomplete_aggregate(type))
		fatal_cur("sizeof cannot be applied to incomplete type\n");
}

static void
validate_alignof_operand_type(Type *type)
{
	if (type_is_void(type))
		fatal_cur("alignof cannot be applied to void type\n");
	if (type_is_function(type))
		fatal_cur("alignof cannot be applied to function type\n");
	if (type_has_incomplete_aggregate(type))
		fatal_cur("alignof cannot be applied to incomplete type\n");
}

int
sizeof_node(Node *node)
{
	if (!node)
		return 4;

	if (node->kind == ND_ADDR && node->left &&
	    node->left->type && type_is_array(node->left->type))
		return type_sizeof(node->left->type);

	if (node->kind == ND_FUNC_ADDR && node->name[0]) {
		Type *global = global_type(node->name);
		if (global && type_is_array(global))
			return type_sizeof(global);
	}


	/*
	 * Array subscripting nodes carry the actual element width in elem_size.
	 * Do this before the generic type check: helper constructors for indexed
	 * scalar loads historically default node->type to int, so relying on
	 * node->type first makes sizeof(table[0]) return 4 even when table is an
	 * array of pointer-sized elements.  test 00216 exposes this with
	 *
	 *     const fptr table[3];
	 *     sizeof(table) / sizeof(table[0])
	 *
	 * where table[0] must be 8 bytes on 64-bit targets, not 4.
	 */
	if (node->kind == ND_INDEX || node->kind == ND_GLOBAL_INDEX) {
		/* If the indexed element is itself an array (e.g. arr[i].name where
		 * name is char[64]), return the full array size, not the element size. */
		if (node->type && type_is_array(node->type))
			return type_sizeof(node->type);
		if (node->type && type_sizeof(node->type) > 0)
			return type_sizeof(node->type);
		return node->elem_size ? node->elem_size : 4;
	}

	if (node->kind == ND_DEREF) {
		/* If the dereferenced node has an array type (e.g. global struct
		 * member access for char[64]), return the full array size. */
		if (node->type && type_is_array(node->type))
			return type_sizeof(node->type);
		if (node->type && type_sizeof(node->type) > 0)
			return type_sizeof(node->type);
		return node->elem_size ? node->elem_size : 4;
	}

	if (node->kind == ND_STRING)
		return (int)node->string_len + 1;

	if (node->type) {
		validate_sizeof_operand_type(node->type);
		return type_sizeof(node->type);
	}

	if (node->is_pointer || node->kind == ND_ADDR)
		return 8;

	return 4;
}

int
parse_sizeof_type_or_expr(void)
{
	if (lexer_peek()->kind != TOK_LPAREN) {
		Node *expr = parse_unary();
		return sizeof_node(expr);
	}

	expect(TOK_LPAREN);

	/*
	 * v154: use the real type parser for sizeof(type), instead of the older
	 * hand-written partial recognizer. This covers ptab.typedefs, pointers,
	 * ptab.structs, unions, enums, and pointer chains uniformly.
	 */
	if (is_type_start_token(lexer_peek()->kind, lexer_peek()->text)) {
		Type *type = parse_type_name();

		while (lexer_peek()->kind == TOK_LBRACKET) {
			lexer_next();

			const Token *len = lexer_peek();
			if (len->kind != TOK_NUM) {
				fatal_cur("Expected numeric array length in sizeof(type[n])\n");
			}

			lexer_next();
			expect(TOK_RBRACKET);

			type = type_array(type, len->value);
		}

		expect(TOK_RPAREN);
		validate_sizeof_operand_type(type);
		return type_sizeof(type);
	}

	Node *expr = parse_expr();
	expect(TOK_RPAREN);

	return sizeof_node(expr);
}

static int
sizeof_type_array_bound_is_const_expr(void)
{
	int paren_depth = 0;

	for (int i = 0;; i++) {
		const Token *tok = lexer_peek_ahead(i);
		int enum_value = 0;

		if (tok->kind == TOK_EOF)
			return 0;
		if (paren_depth == 0 && tok->kind == TOK_RBRACKET)
			return 1;

		if (tok->kind == TOK_LPAREN) {
			paren_depth++;
			continue;
		}
		if (tok->kind == TOK_RPAREN) {
			if (paren_depth > 0)
				paren_depth--;
			continue;
		}

		if (tok->kind == TOK_PLUSPLUS || tok->kind == TOK_MINUSMINUS ||
		    tok->kind == TOK_ASSIGN)
			return 0;

		if (tok->kind == TOK_IDENT &&
		    tok->text &&
		    STRCMP(tok->text, "offsetof") != 0 &&
		    !parser_find_enum_const(tok->text, &enum_value))
			return 0;
	}
}

static Node *
parse_sizeof_type_or_expr_node(void)
{
	Type *type;
	Node *runtime_size = NULL;
	Node *expr;
	int const_factor;

	if (lexer_peek()->kind != TOK_LPAREN) {
		if (lexer_peek()->kind == TOK_IDENT && lexer_peek()->text) {
			const char *name = lexer_peek()->text;
			int size;

			if (find_func(name))
				fatal_cur("sizeof cannot be applied to function type\n");
			size = sizeof_identifier(name);
			if (size >= 0) {
				lexer_next();
				return new_size_t_num(size);
			}
		}
		expr = parse_unary();
		runtime_size = build_runtime_vm_array_sizeof_type_expr(expr ? expr->type : NULL);
		if (runtime_size)
			return runtime_size;
		return new_size_t_num(sizeof_node(expr));
	}

	if (!is_type_start_token(lexer_peek_ahead(1)->kind, lexer_peek_ahead(1)->text)) {
		expect(TOK_LPAREN);
		if (lexer_peek()->kind == TOK_IDENT &&
		    lexer_peek()->text &&
		    lexer_peek_ahead(1)->kind == TOK_RPAREN) {
			const char *name = lexer_peek()->text;
			int size;

			if (find_func(name))
				fatal_cur("sizeof cannot be applied to function type\n");
			size = sizeof_identifier(name);
			if (size >= 0) {
				lexer_next();
				expect(TOK_RPAREN);
				return new_size_t_num(size);
			}
		}
		expr = parse_expr();
		expect(TOK_RPAREN);
		runtime_size = build_runtime_vm_array_sizeof_type_expr(expr ? expr->type : NULL);
		if (runtime_size)
			return runtime_size;
		return new_size_t_num(sizeof_node(expr));
	}

	expect(TOK_LPAREN);
	type = parse_type_name();
	validate_sizeof_operand_type(type);
	runtime_size = build_runtime_vm_array_sizeof_type_expr(type);
	const_factor = type_sizeof(type);

	while (lexer_peek()->kind == TOK_LBRACKET) {
		lexer_next();

		if (lexer_peek()->kind == TOK_NUM) {
			if (lexer_peek()->value <= 0)
				fatal_cur("Array length must be positive\n");
			const_factor *= lexer_peek()->value;
			lexer_next();
		} else if (lexer_peek()->kind == TOK_MINUS &&
		           lexer_peek_ahead(1)->kind == TOK_NUM) {
			fatal_cur("Array length must be positive\n");
		} else if (sizeof_type_array_bound_is_const_expr()) {
			long long dim_value = parser_eval_const_int_expr();
			if (dim_value <= 0)
				fatal_cur("Array length must be positive\n");
			const_factor *= dim_value;
		} else {
			Node *dim = parse_expr();
			if (tcc_lang_is_c89_or_c90())
				fatal_cur("variable length array syntax is not allowed in C89/C90 mode\n");
			runtime_size = runtime_size ? new_binary(ND_MUL, runtime_size, dim) : dim;
		}

		expect(TOK_RBRACKET);
	}

	expect(TOK_RPAREN);
	if (!runtime_size)
		return new_size_t_num(const_factor);
	if (const_factor <= 0)
		return runtime_size;
	return mark_size_t_result(new_binary(ND_MUL, runtime_size, new_size_t_num(const_factor)));
}

static void
reject_alignof_token(const Token *token)
{
	const char *text;

	text = token && token->text ? token->text : "";
	if (STRCMP(text, "_Alignof") == 0) {
		if (!tcc_lang_at_least(LANG_C11))
			fatal_token(token, "_Alignof is not allowed before C11\n");
		return;
	}

	if (STRCMP(text, "alignof") == 0) {
		if (!tcc_lang_at_least(LANG_C23))
			fatal_token(token, "alignof is not allowed before C23\n");
		return;
	}
}

static int
alignof_node(Node *node)
{
	if (!node)
		return 4;

	if (node->kind == ND_ADDR && node->left &&
	    node->left->type && type_is_array(node->left->type))
		return type_alignof(node->left->type);

	if (node->kind == ND_FUNC_ADDR && node->name[0]) {
		Type *global = global_type(node->name);
		if (global && type_is_array(global))
			return type_alignof(global);
	}

	if ((node->kind == ND_INDEX || node->kind == ND_GLOBAL_INDEX ||
	     node->kind == ND_DEREF) &&
	    node->type && type_is_array(node->type)) {
		return type_alignof(node->type);
	}

	if (node->kind == ND_STRING)
		return 1;

	if (node->type) {
		validate_alignof_operand_type(node->type);
		return type_alignof(node->type);
	}

	if (node->is_pointer || node->kind == ND_ADDR)
		return 8;

	if (node->elem_size >= 8)
		return 8;
	if (node->elem_size >= 4)
		return 4;
	if (node->elem_size >= 2)
		return 2;

	return 1;
}

int
parse_alignof_type_or_expr(const Token *op_token)
{
	reject_alignof_token(op_token);

	if (lexer_peek()->kind != TOK_LPAREN) {
		Node *expr = parse_unary();
		return alignof_node(expr);
	}

	expect(TOK_LPAREN);

	if (is_type_start_token(lexer_peek()->kind, lexer_peek()->text)) {
		Type *type = parse_type_name();

		while (lexer_peek()->kind == TOK_LBRACKET) {
			lexer_next();

			if (lexer_peek()->kind == TOK_NUM) {
				if (lexer_peek()->value <= 0)
					fatal_cur("Array length must be positive\n");
				type = type_array(type, lexer_peek()->value);
				lexer_next();
			} else if (lexer_peek()->kind == TOK_MINUS &&
			           lexer_peek_ahead(1)->kind == TOK_NUM) {
				fatal_cur("Array length must be positive\n");
			} else if (sizeof_type_array_bound_is_const_expr()) {
				long long dim_value = parser_eval_const_int_expr();
				if (dim_value <= 0)
					fatal_cur("Array length must be positive\n");
				type = type_array(type, dim_value);
			} else {
				parse_expr();
				if (tcc_lang_is_c89_or_c90())
					fatal_cur("variable length array syntax is not allowed in C89/C90 mode\n");
				type = type_array(type, 1);
			}

			expect(TOK_RBRACKET);
		}

		expect(TOK_RPAREN);
		validate_alignof_operand_type(type);
		return type_alignof(type);
	}

	if (lexer_peek()->kind == TOK_IDENT && lexer_peek_ahead(1)->kind == TOK_RPAREN) {
		int align = alignof_identifier(lexer_peek()->text);
		if (align >= 0) {
			lexer_next();
			expect(TOK_RPAREN);
			return align;
		}
		if (find_func(lexer_peek()->text))
			fatal_cur("alignof cannot be applied to function type\n");
	}

	Node *expr = parse_expr();
	expect(TOK_RPAREN);

	return alignof_node(expr);
}

static int
alignof_identifier_starts_keyword_form(void)
{
	const Token *token = lexer_peek();
	const Token *next = lexer_peek_ahead(1);
	const Token *after_lparen = lexer_peek_ahead(2);
	const Token *after_operand = lexer_peek_ahead(3);

	if (token->kind != TOK_IDENT || !token->text || STRCMP(token->text, "alignof") != 0)
		return 0;
	if (next->kind != TOK_LPAREN)
		return 0;
	if (is_type_start_token(after_lparen->kind, after_lparen->text))
		return 1;
	if (after_lparen->kind == TOK_IDENT && after_operand->kind == TOK_RPAREN) {
		if (alignof_identifier(after_lparen->text) >= 0)
			return 1;
		if (find_func(after_lparen->text))
			return 1;
	}

	return 0;
}

Node *
parse_unary(void)
{
#define RETURN_UNARY_NODE(expr) \
	do { \
		Node *_node = (expr); \
		parser_profile_scope_leave(PARSER_PROF_EXPR_UNARY); \
		return _node; \
	} while (0)

	parser_profile_scope_enter(PARSER_PROF_EXPR_UNARY);
	if (lexer_peek()->kind == TOK_TILDE) {
		Node *operand;

		lexer_next();
		operand = parse_unary();
		validate_integer_unary_operand(operand);
		RETURN_UNARY_NODE(new_unary(ND_BITNOT, operand));
	}

	const Token *token = lexer_peek();

	if (token->kind == TOK_PLUSPLUS || token->kind == TOK_MINUSMINUS) {
		lexer_next();
		RETURN_UNARY_NODE(make_incdec(parse_unary(), token->kind, 0));
	}

	if (token->kind == TOK_SIZEOF) {
		Node *vla_size_expr;

		lexer_next();

		if (lexer_peek()->kind != TOK_LPAREN) {
			if (lexer_peek()->kind == TOK_IDENT &&
			    lexer_peek()->text &&
			    is_vla_local(lexer_peek()->text)) {
				const Token *name = lexer_peek();
				lexer_next();
				vla_size_expr = build_runtime_vla_sizeof_expr(name->text);
				if (vla_size_expr)
					RETURN_UNARY_NODE(vla_size_expr);
			}
		} else if (lexer_peek_ahead(1)->kind == TOK_IDENT &&
		           lexer_peek_ahead(2)->kind == TOK_RPAREN &&
		           lexer_peek_ahead(1)->text &&
		           is_vla_local(lexer_peek_ahead(1)->text)) {
			const Token *name = lexer_peek_ahead(1);
			expect(TOK_LPAREN);
			lexer_next();
			expect(TOK_RPAREN);
			vla_size_expr = build_runtime_vla_sizeof_expr(name->text);
			if (vla_size_expr)
				RETURN_UNARY_NODE(vla_size_expr);
		}

		RETURN_UNARY_NODE(parse_sizeof_type_or_expr_node());
	}

	if (token->kind == TOK_ALIGNOF) {
		lexer_next();
		RETURN_UNARY_NODE(new_size_t_num(parse_alignof_type_or_expr(token)));
	}

	if (alignof_identifier_starts_keyword_form()) {
		lexer_next();
		RETURN_UNARY_NODE(new_size_t_num(parse_alignof_type_or_expr(token)));
	}

	if (token->kind == TOK_MINUS) {
		lexer_next();
		{
			Node *operand = parse_unary();
			if (expr_node_has_complex_value(operand))
				RETURN_UNARY_NODE(expr_build_complex_unary_neg(operand));
			RETURN_UNARY_NODE(new_unary(ND_NEG, operand));
		}
	}

	if (token->kind == TOK_NOT) {
		Node *operand;

		lexer_next();
		operand = expr_coerce_scalar_condition(parse_unary());
		RETURN_UNARY_NODE(new_unary(ND_NOT, operand));
	}

	/* Unary plus: +expr is a no-op (integer promotion only) */
	if (token->kind == TOK_PLUS) {
		lexer_next();
		{
			Node *operand = parse_unary();
			if (expr_node_has_complex_value(operand) ||
			    expr_node_has_imaginary_value(operand))
				RETURN_UNARY_NODE(operand);
			RETURN_UNARY_NODE(new_binary(ND_ADD, operand, new_num(0)));
		}
	}

	if (token->kind == TOK_STAR) {
		lexer_next();
		Node *deref_inner = parse_unary();
		/* In C, dereferencing a function pointer yields a function designator.
		 * Keep real data loads for T** and object pointers, but treat any
		 * pointer-to-function expression as a no-op under unary '*'. */
		if (deref_inner && deref_inner->type &&
		    type_is_pointer(deref_inner->type) &&
		    type_pointee(deref_inner->type) &&
		    type_is_function(type_pointee(deref_inner->type)))
			RETURN_UNARY_NODE(deref_inner);
		if (deref_inner && deref_inner->type && type_is_pointer(deref_inner->type) &&
		    type_pointee(deref_inner->type) && type_is_void(type_pointee(deref_inner->type))) {
			fatal_cur("cannot dereference void *\n");
		}
		RETURN_UNARY_NODE(new_deref(deref_inner));
	}

	if (token->kind == TOK_AMP) {
		lexer_next();
		if (lexer_peek()->kind == TOK_LPAREN &&
		    is_type_start_token(lexer_peek_ahead(1)->kind,
		                        lexer_peek_ahead(1)->text)) {
			Type *type;
			int dims[MAX_ARRAY_DIMS];
			int dim_count = 0;

			expect(TOK_LPAREN);
			type = parse_type_name();
			skip_inline_qualifiers();
			if (lexer_peek()->kind == TOK_LPAREN)
				try_parse_abstract_function_pointer_declarator(&type);
			skip_inline_qualifiers();
			if (lexer_peek()->kind == TOK_LBRACKET) {
				dim_count = parse_array_dimensions(dims, 1, 0);
				type = build_array_type_from_dims_allow_incomplete(clone_type(type),
				                                                   dims, dim_count, 1);
			}
			expect(TOK_RPAREN);
			if (lexer_peek()->kind != TOK_LBRACE)
				fatal_cur("Expected initializer list in compound literal\n");
			if (tcc_lang_is_c89_or_c90())
				fatal_cur("compound literals are not allowed in C89/C90 mode\n");
			RETURN_UNARY_NODE(parse_compound_literal_address(type));
		}
		if (lexer_peek()->kind == TOK_IDENT &&
		    lexer_peek()->text &&
		    lexer_peek_ahead(1)->kind != TOK_LBRACKET &&
		    lexer_peek_ahead(1)->kind != TOK_DOT &&
		    lexer_peek_ahead(1)->kind != TOK_ARROW &&
		    lexer_peek_ahead(1)->kind != TOK_LPAREN &&
		    lexer_peek_ahead(1)->kind != TOK_PLUSPLUS &&
		    lexer_peek_ahead(1)->kind != TOK_MINUSMINUS) {
			const char *name = lexer_peek()->text;
			if (is_register_local(name))
				fatal_cur("Cannot take address of register object\n");
			if (is_vla_local(name)) {
				Node *base = new_var(name, find_local(name));
				Type *ptr_type = build_local_vla_array_pointer_type(name);
				base->type = ptr_type ? ptr_type : type_local(name);
				base->is_pointer = 1;
				base->elem_size = elem_size_local(name);
				lexer_next();
				return base;
			}
			if (is_array_local(name)) {
				Node *base = new_var(name, find_local(name));
				base->type = type_local(name);
				base->elem_size = elem_size_local(name);
				lexer_next();
				return new_addr(base);
			}
			if (is_global_array(name)) {
				Node *base = new_global(name);
				base->type = global_type(name);
				base->elem_size = global_elem_size(name);
				lexer_next();
				return new_addr(base);
			}
		}
		Node *operand = parse_unary();
		/* &funcname is the same as funcname for function-pointer contexts */
		if (operand && operand->kind == ND_FUNC_ADDR)
			RETURN_UNARY_NODE(operand);
		if (operand && operand->is_bitfield)
			fatal_cur("Cannot take address of bit-field\n");
		if (operand && operand->kind == ND_VAR && operand->name[0] &&
		    is_register_local(operand->name))
			fatal_cur("Cannot take address of register object\n");
		RETURN_UNARY_NODE(new_addr(operand));
	}

	RETURN_UNARY_NODE(parse_postfix());
#undef RETURN_UNARY_NODE
}

Node *
parse_term(void)
{
	Node *node = parse_unary();

	for (;;) {
		const Token *token = lexer_peek();

		if (token->kind == TOK_STAR) {
			Node *rhs;
			lexer_next();
			rhs = parse_unary();
			if ((expr_node_has_complex_value(node) && expr_node_has_imaginary_value(rhs)) ||
			    (expr_node_has_imaginary_value(node) && expr_node_has_complex_value(rhs))) {
				Node *lhs_complex = node;
				Node *rhs_complex = rhs;

				if (expr_node_has_imaginary_value(node))
					lhs_complex = expr_build_complex_from_imaginary(node, rhs->type);
				if (expr_node_has_imaginary_value(rhs))
					rhs_complex = expr_build_complex_from_imaginary(rhs, node->type);
				if (!lhs_complex || !rhs_complex)
					fatal_cur("complex arithmetic is not supported yet\n");
				node = expr_build_complex_multiplicative(lhs_complex, rhs_complex, ND_MUL);
				if (!node)
					fatal_cur("complex arithmetic is not supported yet\n");
				continue;
			}
			if (expr_node_has_imaginary_value(node) || expr_node_has_imaginary_value(rhs)) {
				Node *imag_expr = expr_build_imaginary_multiplicative(node, rhs, ND_MUL);
				if (!imag_expr)
					fatal_cur("imaginary arithmetic is not supported yet\n");
				node = imag_expr;
				continue;
			}
			if (expr_node_has_complex_value(node) || expr_node_has_complex_value(rhs)) {
				if (expr_node_has_complex_value(node) && expr_node_is_real_scalar(rhs)) {
					Node *rhs_complex = expr_build_complex_value_cast(rhs, node->type);
					Node *complex_expr = expr_build_complex_multiplicative(node, rhs_complex,
					                                                      ND_MUL);
					if (complex_expr) {
						node = complex_expr;
						continue;
					}
				}
				if (expr_node_is_real_scalar(node) && expr_node_has_complex_value(rhs)) {
					Node *lhs_complex = expr_build_complex_value_cast(node, rhs->type);
					Node *complex_expr = expr_build_complex_multiplicative(lhs_complex, rhs,
					                                                      ND_MUL);
					if (complex_expr) {
						node = complex_expr;
						continue;
					}
				}
				Node *complex_expr = expr_build_complex_multiplicative(node, rhs, ND_MUL);
				if (!complex_expr)
					fatal_cur("complex arithmetic is not supported yet\n");
				node = complex_expr;
				continue;
			}
			node = new_binary(ND_MUL, node, rhs);
		} else if (token->kind == TOK_SLASH) {
			Node *rhs;
			lexer_next();
			rhs = parse_unary();
			if ((expr_node_has_complex_value(node) && expr_node_has_imaginary_value(rhs)) ||
			    (expr_node_has_imaginary_value(node) && expr_node_has_complex_value(rhs))) {
				Node *lhs_complex = node;
				Node *rhs_complex = rhs;

				if (expr_node_has_imaginary_value(node))
					lhs_complex = expr_build_complex_from_imaginary(node, rhs->type);
				if (expr_node_has_imaginary_value(rhs))
					rhs_complex = expr_build_complex_from_imaginary(rhs, node->type);
				if (!lhs_complex || !rhs_complex)
					fatal_cur("complex arithmetic is not supported yet\n");
				node = expr_build_complex_multiplicative(lhs_complex, rhs_complex, ND_DIV);
				if (!node)
					fatal_cur("complex arithmetic is not supported yet\n");
				continue;
			}
			if (expr_node_has_imaginary_value(node) || expr_node_has_imaginary_value(rhs)) {
				Node *imag_expr = expr_build_imaginary_multiplicative(node, rhs, ND_DIV);
				if (!imag_expr)
					fatal_cur("imaginary arithmetic is not supported yet\n");
				node = imag_expr;
				continue;
			}
			if (expr_node_has_complex_value(node) || expr_node_has_complex_value(rhs)) {
				if (expr_node_has_complex_value(node) && expr_node_is_real_scalar(rhs)) {
					Node *rhs_complex = expr_build_complex_value_cast(rhs, node->type);
					Node *complex_expr = expr_build_complex_multiplicative(node, rhs_complex,
					                                                      ND_DIV);
					if (complex_expr) {
						node = complex_expr;
						continue;
					}
				}
				if (expr_node_is_real_scalar(node) && expr_node_has_complex_value(rhs)) {
					Node *lhs_complex = expr_build_complex_value_cast(node, rhs->type);
					Node *complex_expr = expr_build_complex_multiplicative(lhs_complex, rhs,
					                                                      ND_DIV);
					if (complex_expr) {
						node = complex_expr;
						continue;
					}
				}
				Node *complex_expr = expr_build_complex_multiplicative(node, rhs, ND_DIV);
				if (!complex_expr)
					fatal_cur("complex arithmetic is not supported yet\n");
				node = complex_expr;
				continue;
			}
			node = new_binary(ND_DIV, node, rhs);
		} else if (token->kind == TOK_PERCENT) {
			Node *rhs;

			lexer_next();
			rhs = parse_unary();
			validate_integer_binary_operands(node, rhs);
			node = new_binary(ND_MOD, node, rhs);
		} else {
			return node;
		}
	}
}

Node *
make_additive(NodeKind kind, Node *left, Node *right)
{
	int left_ptr = left->is_pointer || (left->type && type_is_pointer(left->type));
	int right_ptr = right->is_pointer || (right->type && type_is_pointer(right->type));
	int left_imag = expr_node_has_imaginary_value(left);
	int right_imag = expr_node_has_imaginary_value(right);

	if (left_imag || right_imag) {
		if (left_imag && right_imag) {
			Node *imag_expr = expr_build_imaginary_additive(left, right, kind);
			if (imag_expr)
				return imag_expr;
		}
		{
			Node *complex_imag_expr = expr_build_complex_imag_additive(left, right, kind);
			if (complex_imag_expr)
				return complex_imag_expr;
		}
		{
			Node *mixed_complex_expr = expr_build_real_imag_additive(left, right, kind);
			if (mixed_complex_expr)
				return mixed_complex_expr;
		}
		fatal_cur("imaginary arithmetic is not supported yet\n");
	}

	if (expr_node_has_complex_value(left) || expr_node_has_complex_value(right)) {
		if (expr_node_has_complex_value(left) && expr_node_is_real_scalar(right)) {
			Node *right_complex = expr_build_complex_value_cast(right, left->type);
			Node *complex_expr = expr_build_complex_additive(left, right_complex, kind);
			if (complex_expr)
				return complex_expr;
		}
		if (expr_node_is_real_scalar(left) && expr_node_has_complex_value(right)) {
			Node *left_complex = expr_build_complex_value_cast(left, right->type);
			Node *complex_expr = expr_build_complex_additive(left_complex, right, kind);
			if (complex_expr)
				return complex_expr;
		}
		Node *complex_expr = expr_build_complex_additive(left, right, kind);
		if (complex_expr)
			return complex_expr;
		fatal_cur("complex arithmetic is not supported yet\n");
	}

	if (kind == ND_ADD) {
		if (left_ptr && right_ptr) {
			fatal_cur("Cannot add two pointers\n");
		}

		if (right_ptr && !left_ptr) {
			Node *tmp = left;
			left = right;
			right = tmp;
			left_ptr = 1;
			right_ptr = 0;
		}

		if (left_ptr && left->type && type_is_pointer(left->type) &&
		    type_pointee(left->type) && type_is_void(type_pointee(left->type))) {
			fatal_cur("cannot perform pointer arithmetic on void *\n");
		}

		if (left_ptr && expr_vm_pointer_array_type(left->type)) {
			Node *vm_add = expr_build_vm_pointer_arith(kind, left, right);
			if (vm_add)
				return vm_add;
		}

		Node *node = new_binary(kind, left, right);
		node->is_pointer = left_ptr;
		/* For pointer arithmetic, scale = size of the pointed-to element,
		 * not the size of the pointer itself (which is what elem_size tracks
		 * for load purposes on pointer-typed nodes). */
		if (left->type && type_is_pointer(left->type) && type_pointee(left->type)) {
			Type *base = type_pointee(left->type);
			if (base->kind == TY_ARRAY && base->is_vm_type && left->elem_size > 0)
				node->elem_size = left->elem_size;
			else if (base->kind == TY_CHAR)
				node->elem_size = 1;
			else if (base->kind == TY_SHORT)
				node->elem_size = 2;
			else if (base->kind == TY_PTR)
				node->elem_size = TCC_SIZEOF_PTR;
			else if (type_is_struct(base))
				node->elem_size = base->size;
			else
				node->elem_size = base->size ? base->size : 4;
			node->type = left->type;
		} else {
			node->elem_size = left->elem_size ? left->elem_size : 4;
			if (left->type && type_is_pointer(left->type))
				node->type = left->type;
		}
		return node;
	}

	if (kind == ND_SUB) {
		Node *node = new_binary(kind, left, right);

		if ((left_ptr && left->type && type_is_pointer(left->type) &&
		     type_pointee(left->type) && type_is_void(type_pointee(left->type))) ||
		    (right_ptr && right->type && type_is_pointer(right->type) &&
		     type_pointee(right->type) && type_is_void(type_pointee(right->type)))) {
			fatal_cur("cannot perform pointer arithmetic on void *\n");
		}

		if (left_ptr && right_ptr) {
			/* ptr - ptr = (addr_diff) / sizeof(pointee) */
			node->is_pointer = 0;
			node->type = type_int();
			/* Determine element size of the pointee */
			int ptrdiff_scale = 4; /* default: int */
			Node *runtime_divisor = NULL;
			if (left->type && type_is_pointer(left->type) && type_pointee(left->type)) {
				Type *base = type_pointee(left->type);
				if (base->kind == TY_ARRAY && base->is_vm_type)
					runtime_divisor = expr_build_vm_stride_expr(left->type);
				else if (base->kind == TY_CHAR) ptrdiff_scale = 1;
				else if (base->kind == TY_SHORT) ptrdiff_scale = 2;
				else if (type_is_pointer(base)) ptrdiff_scale = 8;
				else if (base->size > 0) ptrdiff_scale = base->size;
			} else if (left->elem_size > 0) {
				ptrdiff_scale = left->elem_size;
			}
			node->elem_size = runtime_divisor ? 1 : ptrdiff_scale;
			if (runtime_divisor) {
				Node *div = new_binary(ND_DIV, node, runtime_divisor);
				div->type = type_int();
				div->elem_size = TCC_SIZEOF_INT;
				return div;
			}
			if (ptrdiff_scale > 1) {
				/* wrap: (left - right) / scale */
				Node *divisor = new_num(ptrdiff_scale);
				divisor->type = type_int();
				Node *div = new_binary(ND_DIV, node, divisor);
				div->type = type_int();
				div->elem_size = TCC_SIZEOF_INT;
				return div;
			}
		} else {
			if (left_ptr && expr_vm_pointer_array_type(left->type)) {
				Node *vm_sub = expr_build_vm_pointer_arith(kind, left, right);
				if (vm_sub)
					return vm_sub;
			}
			node->is_pointer = left_ptr && !right_ptr;
			node->elem_size = left->elem_size ? left->elem_size : 4;
			if (node->is_pointer && left->type && type_is_pointer(left->type))
				node->type = left->type;
		}

		return node;
	}

	return new_binary(kind, left, right);
}

Node *
parse_additive(void)
{
	Node *node = parse_term();

	for (;;) {
		const Token *token = lexer_peek();

		if (token->kind == TOK_PLUS) {
			lexer_next();
			node = make_additive(ND_ADD, node, parse_term());
		} else if (token->kind == TOK_MINUS) {
			lexer_next();
			node = make_additive(ND_SUB, node, parse_term());
		} else {
			return node;
		}
	}
}

static int
parse_struct_return_member_setup_value(Node **setup_out, Node **value_out)
{
	const Token *func;
	FuncInfo *fi;
	int depth;
	int i;
	Node *args = NULL;
	char temp_name[64];
	int offset;
	StructDef *def;
	Node *decl;
	Node *lhs;
	Node *call;
	Node *assign;
	Node *temp;
	Node *member;

	if (setup_out)
		*setup_out = NULL;
	if (value_out)
		*value_out = NULL;

	if (lexer_peek()->kind != TOK_IDENT || lexer_peek_ahead(1)->kind != TOK_LPAREN)
		return 0;

	func = lexer_peek();
	fi = find_func(func->text);
	if (!fi || !fi->returns_struct)
		return 0;

	depth = 0;
	for (i = 1; i < 128; i++) {
		TokenKind kind = lexer_peek_ahead(i)->kind;

		if (kind == TOK_EOF)
			return 0;
		if (kind == TOK_LPAREN) {
			depth++;
			continue;
		}
		if (kind == TOK_RPAREN) {
			depth--;
			if (depth == 0)
				break;
		}
	}
	if (depth != 0 || lexer_peek_ahead(i + 1)->kind != TOK_DOT)
		return 0;

	lexer_next(); /* function name */
	expect(TOK_LPAREN);
	if (lexer_peek()->kind != TOK_RPAREN)
		args = parse_arg_list(fi);
	expect(TOK_RPAREN);

	snprintf(temp_name, sizeof(temp_name), "__struct_member_%d", parser_alloc_struct_arg_temp_id());
	offset = add_struct_local(temp_name, fi->struct_name);
	def = find_struct(fi->struct_name);

	decl = new_struct_decl(temp_name, offset);
	decl->type = type_struct(fi->struct_name, def->size);

	lhs = new_var(temp_name, offset);
	lhs->type = type_struct(fi->struct_name, def->size);
	lhs->elem_size = def->size;
	STRNCPY(lhs->struct_name, fi->struct_name, sizeof(lhs->struct_name) - 1);

	call = new_call(func->text, args);
	call->returns_struct = 1;
	call->aggregate_abi_class = fi->return_abi_class;
	call->aggregate_abi_reg_count = call->aggregate_abi_class == AGGREGATE_ABI_INTREGS
	                                 ? fi->return_abi_reg_count
	                                 : 0;
	call->struct_return_size = fi->struct_size;
	call->type = fi->return_type ? clone_type(fi->return_type)
	                             : type_struct(fi->struct_name, fi->struct_size);
	STRNCPY(call->return_struct_name, fi->struct_name, sizeof(call->return_struct_name) - 1);

	assign = new_assign(lhs, call);

	temp = new_var(temp_name, offset);
	temp->type = type_struct(fi->struct_name, def->size);
	temp->elem_size = def->size;
	STRNCPY(temp->struct_name, fi->struct_name, sizeof(temp->struct_name) - 1);

	member = make_member_read_from_struct_temp(temp, fi->struct_name);

	if (setup_out)
		*setup_out = append_node(decl, assign);
	if (value_out)
		*value_out = member;
	return 1;
}

static int
parse_struct_member_setup_value(Node **setup_out, Node **value_out)
{
	Node *setup = NULL;
	Node *value = NULL;

	if (parse_struct_return_member_setup_value(&setup, &value)) {
		if (setup_out)
			*setup_out = setup;
		if (value_out)
			*value_out = value;
		return 1;
	}

	value = parse_struct_assign_member_expr_core(&setup);
	if (!value) {
		if (setup_out)
			*setup_out = NULL;
		if (value_out)
			*value_out = NULL;
		return 0;
	}

	if (setup_out)
		*setup_out = setup;
	if (value_out)
		*value_out = value;
	return 1;
}

static int
parse_struct_assign_member_setup_value(Node **setup_out, Node **value_out)
{
	Node *setup = NULL;
	Node *expr = NULL;
	Node *rhs_setup = NULL;
	Node *rhs = NULL;

	if (!parse_struct_member_setup_value(&setup, &expr)) {
		if (setup_out)
			*setup_out = NULL;
		if (value_out)
			*value_out = NULL;
		return 0;
	}

	for (;;) {
		TokenKind op = lexer_peek()->kind;
		NodeKind kind = ND_ADD;
		int use_additive = 0;

		if (op == TOK_STAR) {
			kind = ND_MUL;
		} else if (op == TOK_SLASH) {
			kind = ND_DIV;
		} else if (op == TOK_PLUS) {
			kind = ND_ADD;
			use_additive = 1;
		} else if (op == TOK_MINUS) {
			kind = ND_SUB;
			use_additive = 1;
		} else if (op == TOK_SHL) {
			kind = ND_SHL;
		} else if (op == TOK_SHR) {
			kind = ND_SHR;
		} else if (op == TOK_AMP) {
			kind = ND_BITAND;
		} else if (op == TOK_CARET) {
			kind = ND_BITXOR;
		} else if (op == TOK_PIPE) {
			kind = ND_BITOR;
		} else if (op == TOK_AND) {
			kind = ND_LOGICAL_AND;
		} else if (op == TOK_OR) {
			kind = ND_LOGICAL_OR;
		} else {
			break;
		}

			lexer_next();

			rhs_setup = NULL;
			rhs = NULL;
			if (!parse_struct_member_setup_value(&rhs_setup, &rhs))
				rhs = parse_unary();

			if (rhs_setup)
				setup = append_node(setup, rhs_setup);

			if (use_additive)
				expr = make_additive(kind, expr, rhs);
			else
				expr = new_binary(kind, expr, rhs);
		}

	if (setup_out)
		*setup_out = setup;
	if (value_out)
		*value_out = expr;
	return 1;
}

Node *
parse_struct_assign_member_sum(Node **setup_out)
{
	Node *setup = NULL;
	Node *value = NULL;

	if (!parse_struct_assign_member_setup_value(&setup, &value)) {
		if (setup_out)
			*setup_out = NULL;
		return NULL;
	}

	if (setup_out)
		*setup_out = setup;

	return value;
}

Node *
make_member_read_from_struct_temp(Node *temp, const char *struct_name)
{
	int offset = temp->offset;
	Field *field = NULL;

	if (lexer_peek()->kind != TOK_DOT) {
		fatal_cur("Expected member access after struct return expression\n");
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

		if (field->is_struct && lexer_peek()->kind == TOK_DOT) {
			struct_name = field->struct_name;
			continue;
		}

		if (lexer_peek()->kind == TOK_DOT && !field->is_struct)
			fatal_cur("Nested member access through non-struct field\n");

		Node *member = new_member(field->name, offset);
		apply_field_type(member, field);
		return member;
	}

	return temp;
}

Node *
parse_shift(void)
{
	Node *node = parse_additive();

	for (;;) {
		const Token *token = lexer_peek();

		if (token->kind == TOK_SHL) {
			Node *rhs;

			lexer_next();
			rhs = parse_additive();
			validate_integer_binary_operands(node, rhs);
			node = new_binary(ND_SHL, node, rhs);
		} else if (token->kind == TOK_SHR) {
			Node *rhs;

			lexer_next();
			rhs = parse_additive();
			validate_integer_binary_operands(node, rhs);
			node = new_binary(ND_SHR, node, rhs);
		} else {
			return node;
		}
	}
}

static void
validate_pointer_comparison(Node *lhs, Node *rhs, int is_relational)
{
	int lhs_null;
	int rhs_null;
	Type *lhs_type;
	Type *rhs_type;

	if (!lhs || !rhs)
		return;

	lhs_null = node_is_null_pointer_constant(lhs);
	rhs_null = node_is_null_pointer_constant(rhs);
	lhs_type = expr_pointer_context_type(lhs->type);
	rhs_type = expr_pointer_context_type(rhs->type);

	if (!lhs_null && !rhs_null &&
	    (!lhs_type || !type_is_pointer(lhs_type)) &&
	    (!rhs_type || !type_is_pointer(rhs_type)))
		return;

	if (lhs_null && rhs_null)
		return;

	if (lhs_null) {
		if (rhs_type && type_is_pointer(rhs_type)) {
			if (is_relational)
				fatal_cur("Incompatible pointer types in comparison\n");
			return;
		}
		return;
	}

	if (rhs_null) {
		if (lhs_type && type_is_pointer(lhs_type)) {
			if (is_relational)
				fatal_cur("Incompatible pointer types in comparison\n");
			return;
		}
		return;
	}

	if (!lhs_type || !rhs_type ||
	    !type_is_pointer(lhs_type) || !type_is_pointer(rhs_type)) {
		fatal_cur("Incompatible pointer types in comparison\n");
	}

	if (type_pointer_assignment_compatible(lhs_type, rhs_type, 0) ||
	    type_pointer_assignment_compatible(rhs_type, lhs_type, 0))
		return;

	fatal_cur("Incompatible pointer types in comparison\n");
}

Node *
parse_relational(void)
{
	Node *node = parse_shift();

	for (;;) {
		const Token *token = lexer_peek();

		if (token->kind == TOK_LT) {
			lexer_next();
			Node *lhs = node;
			Node *rhs = parse_shift();
			validate_pointer_comparison(lhs, rhs, 1);
			node = new_binary(ND_LT, lhs, rhs);
		} else if (token->kind == TOK_LE) {
			lexer_next();
			Node *lhs = node;
			Node *rhs = parse_shift();
			validate_pointer_comparison(lhs, rhs, 1);
			node = new_binary(ND_LE, lhs, rhs);
		} else if (token->kind == TOK_GT) {
			lexer_next();
			Node *lhs = node;
			Node *rhs = parse_shift();
			validate_pointer_comparison(lhs, rhs, 1);
			node = new_binary(ND_GT, lhs, rhs);
		} else if (token->kind == TOK_GE) {
			lexer_next();
			Node *lhs = node;
			Node *rhs = parse_shift();
			validate_pointer_comparison(lhs, rhs, 1);
			node = new_binary(ND_GE, lhs, rhs);
		} else {
			return node;
		}
	}
}

Node *
make_struct_scalar_member(Node *base, Field *field, int offset_base)
{
	if (base->kind != ND_VAR) {
		fatal_cur("Struct comparison currently requires local struct operands\n");
	}

	Node *node = new_member(field->name, base->offset + offset_base + field->offset);
	node->elem_size = field->size;
	node->type = type_for_size(field->size);
	STRNCPY(node->struct_name, base->struct_name, sizeof(node->struct_name) - 1);
	return node;
}

Node *
append_struct_field_comparisons(Node *expr, Node *left, Node *right, const char *struct_name, int offset_base)
{
	StructDef *def = find_struct(struct_name);

	for (int i = 0; i < def->field_count; i++) {
		Field *field = &def->fields[i];

		if (field->is_struct) {
			expr = append_struct_field_comparisons(expr, left, right, field->struct_name,
			                                       offset_base + field->offset);
			continue;
		}

		Node *lhs = make_struct_scalar_member(left, field, offset_base);
		Node *rhs = make_struct_scalar_member(right, field, offset_base);
		Node *cmp = new_binary(ND_EQ, lhs, rhs);

		if (!expr)
			expr = cmp;
		else
			expr = new_binary(ND_LOGICAL_AND, expr, cmp);
	}

	return expr ? expr : new_num(1);
}

Node *
build_struct_equality_expr(Node *left, Node *right, int invert)
{
	if (!left || !right ||
	        !left->type || !right->type ||
	        !type_is_struct(left->type) || !type_is_struct(right->type))
		return NULL;

	if (STRCMP(expr_resolve_struct_type_name(left->type),
	           expr_resolve_struct_type_name(right->type)) != 0) {
		fatal_cur("Cannot compare different struct types\n");
	}

	Node *expr = append_struct_field_comparisons(NULL, left, right,
	                                             expr_resolve_struct_type_name(left->type), 0);
	if (invert)
		expr = new_unary(ND_NOT, expr);

	return expr;
}

Node *
parse_equality(void)
{
	Node *node = parse_relational();

	for (;;) {
		const Token *token = lexer_peek();

		if (token->kind == TOK_EQ) {
			lexer_next();
			Node *right = parse_relational();
			Node *struct_cmp = build_struct_equality_expr(node, right, 0);
			if (!struct_cmp)
				validate_pointer_comparison(node, right, 0);
			node = struct_cmp ? struct_cmp : new_binary(ND_EQ, node, right);
		} else if (token->kind == TOK_NE) {
			lexer_next();
			Node *right = parse_relational();
			Node *struct_cmp = build_struct_equality_expr(node, right, 1);
			if (!struct_cmp)
				validate_pointer_comparison(node, right, 0);
			node = struct_cmp ? struct_cmp : new_binary(ND_NE, node, right);
		} else {
			return node;
		}
	}
}

Node *
parse_bitand(void)
{
	Node *node = parse_equality();

	while (lexer_peek()->kind == TOK_AMP) {
		Node *rhs;

		lexer_next();
		rhs = parse_equality();
		validate_integer_binary_operands(node, rhs);
		node = new_binary(ND_BITAND, node, rhs);
	}
	return node;
}

Node *
parse_bitxor(void)
{
	Node *node = parse_bitand();

	while (lexer_peek()->kind == TOK_CARET) {
		Node *rhs;

		lexer_next();
		rhs = parse_bitand();
		validate_integer_binary_operands(node, rhs);
		node = new_binary(ND_BITXOR, node, rhs);
	}
	return node;
}

Node *
parse_bitor(void)
{
	Node *node = parse_bitxor();

	while (lexer_peek()->kind == TOK_PIPE) {
		Node *rhs;

		lexer_next();
		rhs = parse_bitxor();
		validate_integer_binary_operands(node, rhs);
		node = new_binary(ND_BITOR, node, rhs);
	}
	return node;
}

Node *
parse_logical_and(void)
{
	Node *node;

	node = parse_bitor();
	while (lexer_peek()->kind == TOK_AND) {
		Node *right;

		lexer_next();
		right = parse_bitor();
		node = new_binary(ND_LOGICAL_AND,
		                  expr_coerce_scalar_condition(node),
		                  expr_coerce_scalar_condition(right));
	}

	return node;
}

Node *
parse_logical_or(void)
{
	Node *node;

	node = parse_logical_and();
	while (lexer_peek()->kind == TOK_OR) {
		Node *right;

		lexer_next();
		right = parse_logical_and();
		node = new_binary(ND_LOGICAL_OR,
		                  expr_coerce_scalar_condition(node),
		                  expr_coerce_scalar_condition(right));
	}
	return node;
}

Node *
parse_conditional(void)
{
	Node *node;
	int const_cond;
	Node *chosen;
	Node *dead_arm;

	parser_profile_scope_enter(PARSER_PROF_EXPR_COND);
	node = parse_logical_or();
	if (lexer_peek()->kind != TOK_QUESTION) {
		parser_profile_scope_leave(PARSER_PROF_EXPR_COND);
		return node;
	}

	lexer_next();
	node = expr_coerce_scalar_condition(node);

	Node *then_expr = parse_assignment();
	/* ?: middle operand allows comma operator */
	while (lexer_peek()->kind == TOK_COMMA) {
		lexer_next();
		then_expr = new_binary(ND_COMMA, then_expr, parse_assignment());
	}
	expect(TOK_COLON);
	Node *else_expr = parse_conditional();
	const_cond = node && node->kind == ND_NUM;
	chosen = const_cond && node->value ? then_expr : else_expr;
	dead_arm = const_cond && node->value ? else_expr : then_expr;

	if (const_cond &&
	    dead_arm &&
	    dead_arm->kind == ND_BLOCK &&
	    chosen &&
	    chosen->type) {
		parser_profile_scope_leave(PARSER_PROF_EXPR_COND);
		return chosen;
	}

	if (then_expr && else_expr && then_expr->type && else_expr->type) {
		int then_void = type_is_void(then_expr->type);
		int else_void = type_is_void(else_expr->type);
		if (then_void != else_void)
			fatal_cur("Incompatible operand types in conditional expression\n");
	}

	if (then_expr && else_expr &&
	    then_expr->type && else_expr->type &&
	    (type_is_struct(then_expr->type) || type_is_union(then_expr->type) ||
	     type_is_struct(else_expr->type) || type_is_union(else_expr->type))) {
		if (!type_equal_unqualified(then_expr->type, else_expr->type))
			fatal_cur("Incompatible operand types in conditional expression\n");
	}

	if (then_expr && else_expr &&
	    then_expr->type && else_expr->type &&
	    type_is_pointer(then_expr->type) &&
	    type_is_pointer(else_expr->type) &&
	    !node_is_null_pointer_constant(then_expr) &&
	    !node_is_null_pointer_constant(else_expr)) {
		if (!type_pointer_conditional_result(then_expr->type, else_expr->type))
			fatal_cur("Incompatible pointer types in conditional expression\n");
	}

	if (then_expr && else_expr &&
	    then_expr->type && type_is_pointer(then_expr->type) &&
	    else_expr->type && type_is_integer(else_expr->type) &&
	    !node_is_null_pointer_constant(else_expr))
		fatal_cur("Incompatible pointer and integer types in conditional expression\n");

	if (then_expr && else_expr &&
	    else_expr->type && type_is_pointer(else_expr->type) &&
	    then_expr->type && type_is_integer(then_expr->type) &&
	    !node_is_null_pointer_constant(then_expr))
		fatal_cur("Incompatible pointer and integer types in conditional expression\n");

	Node *tmp= new_conditional(node, then_expr, else_expr);
	if (tmp && tmp->type && then_expr && else_expr &&
	    (type_is_complex(tmp->type) ||
	     type_is_imaginary(tmp->type) ||
	     expr_type_is_real_scalar_type(tmp->type))) {
		tmp->then_body = expr_coerce_value_for_type(tmp->then_body, tmp->type);
		tmp->else_body = expr_coerce_value_for_type(tmp->else_body, tmp->type);
	}
	parser_profile_scope_leave(PARSER_PROF_EXPR_COND);
	return tmp;
}

int
is_assignable(Node *node)
{
	return node->kind == ND_VAR ||
	       node->kind == ND_GLOBAL ||
	       node->kind == ND_GLOBAL_INDEX ||
	       node->kind == ND_MEMBER ||
	       node->kind == ND_MEMBER_PTR ||
	       node->kind == ND_INDEX ||
	       node->kind == ND_DEREF;
}

static int
is_modifiable_lvalue(Node *node)
{
	Type *base_type;

	if (!node || !is_assignable(node))
		return 0;
	if (node->kind == ND_INDEX || node->kind == ND_GLOBAL_INDEX) {
		base_type = NULL;
		if (node->name[0] && is_array_local(node->name))
			base_type = type_local(node->name);
		else if (node->name[0] && is_global_array(node->name))
			base_type = global_type(node->name);

		if (base_type && type_is_array(base_type) && type_pointee(base_type))
			return !expr_type_is_const_qualified(type_pointee(base_type));
	}
	if (node->type && (type_is_array(node->type) || type_is_function(node->type)))
		return 0;
	if (node->is_const_lvalue)
		return 0;
	return 1;
}

static int
is_nonmodifiable_designator(Node *node)
{
	Type *type;

	if (!node)
		return 0;
	if (node->kind == ND_ADDR && node->left) {
		type = node->left->type;
		return type && (type_is_array(type) || type_is_function(type));
	}
	if (node->kind == ND_FUNC_ADDR && node->name[0]) {
		type = global_type(node->name);
		return type && (type_is_array(type) || type_is_function(type));
	}
	return 0;
}

static void
validate_integer_unary_operand(Node *node)
{
	if (!node || !node->type || !type_is_integer(node->type))
		fatal_cur("operator requires integer operand\n");
}

static void
validate_integer_binary_operands(Node *lhs, Node *rhs)
{
	if (!lhs || !rhs ||
	    !lhs->type || !rhs->type ||
	    !type_is_integer(lhs->type) ||
	    !type_is_integer(rhs->type))
		fatal_cur("operator requires integer operands\n");
}

static void
validate_pointer_assignment(Node *lhs, Node *rhs)
{
	if (!lhs || !lhs->type || !type_is_pointer(lhs->type))
		return;
	if (rhs && rhs->kind == ND_CAST && rhs->type && type_is_pointer(rhs->type))
		return;
	if (expr_is_explicit_function_pointer_cast(rhs))
		return;
	if (!node_is_null_pointer_constant(rhs) &&
	    rhs && rhs->type && type_is_integer(rhs->type))
		fatal_cur("Incompatible integer to pointer conversion in assignment\n");
	if (!node_is_null_pointer_constant(rhs) &&
	    (!rhs || !rhs->type || !type_is_pointer(rhs->type)))
		return;
	if (type_pointer_assignment_compatible(lhs->type,
	                                       rhs ? rhs->type : NULL,
	                                       node_is_null_pointer_constant(rhs)))
		return;
	fatal_cur("Incompatible pointer types in assignment\n");
}

static void
validate_pointer_to_integer_assignment(Node *lhs, Node *rhs)
{
	if (!lhs || !lhs->type || !type_is_integer(lhs->type) || !rhs || !rhs->type)
		return;
	if (!type_is_pointer(rhs->type))
		return;
	if (type_source_is_bool_spelling(lhs->type))
		return;
	fatal_cur("Incompatible pointer to integer conversion in assignment\n");
}

Node *
clone_node_tree(Node *node)
{
	if (!node)
		return NULL;

	Node *copy = xcalloc(1, sizeof(Node));
	*copy = *node;

	copy->left = clone_node_tree(node->left);
	copy->right = clone_node_tree(node->right);
	copy->init = clone_node_tree(node->init);
	copy->cond = clone_node_tree(node->cond);
	copy->inc = clone_node_tree(node->inc);
	copy->then_body = clone_node_tree(node->then_body);
	copy->else_body = clone_node_tree(node->else_body);
	copy->body = clone_node_tree(node->body);
	copy->args = clone_node_tree(node->args);

	/*
	 * A read copy must own its expression children, but it must not inherit
	 * statement-list links from the original node.
	 */
	copy->next = NULL;

	return copy;
}

Node *
clone_lvalue_for_read(Node *node)
{
	return clone_node_tree(node);
}

Node *
make_incdec(Node *node, TokenKind op, int is_postfix)
{
	if (is_nonmodifiable_designator(node)) {
		fatal_cur("Operand of ++/-- must be a modifiable lvalue\n");
	}
	if (!is_assignable(node)) {
		fatal_cur("Operand of ++/-- must be assignable\n");
	}
	if (!is_modifiable_lvalue(node)) {
		fatal_cur("Operand of ++/-- must be a modifiable lvalue\n");
	}
	if (op == TOK_PLUSPLUS)
		return new_incdec(is_postfix ? ND_POST_INC : ND_PRE_INC, node);

	return new_incdec(is_postfix ? ND_POST_DEC : ND_PRE_DEC, node);
}

int
is_struct_assign_node(Node *node)
{
	return node &&
	       node->kind == ND_STRUCT_ASSIGN &&
	       node->left &&
	       node->right &&
	       node->left->type &&
	       type_is_struct(node->left->type);
}

Node *
make_struct_assign_chain(Node *dst, Node *rhs_assign)
{
	/*
	 * v126.1: chained struct assignment:
	 *
	 *   a = b = c;
	 *
	 * Lower to:
	 *
	 *   b = c;
	 *   a = b;
	 *
	 * The value of the RHS assignment is its destination object after the copy.
	 * Copying from rhs_assign->left preserves the normal struct-copy path and
	 * avoids treating aggregates as scalar values.
	 */
	if (!is_struct_assign_node(rhs_assign))
		return NULL;

	if (!dst || !dst->type || !type_is_struct(dst->type)) {
		fatal_cur("Struct assignment chain requires struct destination\n");
	}

	if (!rhs_assign->left->type ||
	        !type_is_struct(rhs_assign->left->type) ||
	        STRCMP(expr_resolve_struct_type_name(dst->type),
	               expr_resolve_struct_type_name(rhs_assign->left->type)) != 0) {
		fatal_cur("Struct assignment chain type mismatch\n");
	}

	Node *src_after_rhs = clone_lvalue_for_read(rhs_assign->left);
	Node *copy_to_dst = new_struct_assign(dst, src_after_rhs, type_sizeof(dst->type));

	return new_block(append_node(rhs_assign, copy_to_dst));
}

Node *
parse_assignment(void)
{
	Node *node;

	EXPR_PARSE_DEPTH_ENTER();
	parser_profile_scope_enter(PARSER_PROF_EXPR_ASSIGN);
	node = parse_conditional();

	TokenKind op = lexer_peek()->kind;

	if (op == TOK_ASSIGN ||
	        op == TOK_PLUSEQ ||
	        op == TOK_MINUSEQ ||
	        op == TOK_MULEQ ||
	        op == TOK_DIVEQ ||
	        op == TOK_MODEQ ||
	        op == TOK_ANDEQ ||
	        op == TOK_OREQ  ||
	        op == TOK_XOREQ ||
	        op == TOK_SHLEQ ||
	        op == TOK_SHREQ) {
		lexer_next();

		if (is_nonmodifiable_designator(node)) {
			fatal_cur("Left side of assignment must be a modifiable lvalue\n");
		}
		if (!is_assignable(node)) {
			fatal_cur("Left side of assignment must be assignable\n");
		}
		if (!is_modifiable_lvalue(node)) {
			fatal_cur("Left side of assignment must be a modifiable lvalue\n");
		}
		Node *rhs;
		if (op == TOK_ASSIGN &&
		        node->is_pointer &&
		        lexer_peek()->kind == TOK_IDENT &&
		        find_func(lexer_peek()->text) &&
		        lexer_peek_ahead(1)->kind != TOK_LPAREN) {
			const Token *fn = lexer_peek();
			lexer_next();
			rhs = parser_make_function_designator(fn->text);
		} else {
			rhs = parse_assignment();
		}

		if (op == TOK_ASSIGN) {
			if (is_struct_assign_node(rhs) &&
			        node->type && type_is_struct(node->type)) {
				node = make_struct_assign_chain(node, rhs);
			} else if (node->type && rhs->type &&
			           type_is_struct(node->type) && type_is_struct(rhs->type) &&
			           !(rhs->kind == ND_CALL && rhs->returns_struct)) {
				if (STRCMP(expr_resolve_struct_type_name(node->type),
				           expr_resolve_struct_type_name(rhs->type)) != 0) {
					fatal_cur("Struct assignment type mismatch\n");
				}

				node = new_struct_assign(node, rhs, type_sizeof(node->type));
			} else {
				if (node->is_pointer && rhs->kind == ND_VAR && find_func(rhs->name))
					rhs = parser_make_function_designator(rhs->name);
				if (node->type && type_is_complex(node->type) &&
				    rhs && rhs->type) {
					if (expr_node_has_imaginary_value(rhs))
						rhs = expr_build_complex_from_imaginary(rhs, node->type);
					else if (expr_node_is_real_scalar(rhs))
						rhs = expr_build_complex_value_cast(rhs, node->type);
				} else if (node->type && type_is_imaginary(node->type) &&
				           rhs && rhs->type && expr_node_has_complex_value(rhs)) {
					rhs = expr_build_complex_imaginary_extract_cast(rhs, node->type);
				} else if (node->type && expr_type_is_real_scalar_type(node->type) &&
				           rhs && rhs->type) {
					if (expr_node_has_complex_value(rhs))
						rhs = expr_build_complex_real_extract_cast(rhs, node->type);
					else if (expr_node_has_imaginary_value(rhs))
						rhs = expr_build_imaginary_real_extract_cast(rhs, node->type);
				}
				validate_pointer_assignment(node, rhs);
				validate_pointer_to_integer_assignment(node, rhs);
				if (node->type && rhs->type &&
				    type_is_complex(node->type) &&
				    type_is_complex(rhs->type) &&
				    type_equal_unqualified(node->type, rhs->type) &&
				    !expr_type_uses_x64_complex_float_abi(node->type))
					node = new_struct_assign(node, rhs, type_sizeof(node->type));
				else
					node = new_assign(node, rhs);
			}
		} else {
			/*
			 * v66 allows compound assignment for all assignment-capable
			 * lvalues. The existing assignment emitter has explicit store
			 * paths for locals, globals, indexed locals, dereferences, and
			 * pointer members. The RHS uses a separate read copy of the lvalue.
			 */
			NodeKind binop = ND_ADD;
			Node *read_lhs = clone_lvalue_for_read(node);

			if (op == TOK_MINUSEQ)
				binop = ND_SUB;
			else if (op == TOK_MULEQ)
				binop = ND_MUL;
			else if (op == TOK_DIVEQ)
				binop = ND_DIV;
			else if (op == TOK_MODEQ)
				binop = ND_MOD;
			else if (op == TOK_ANDEQ)
				binop = ND_BITAND;
			else if (op == TOK_OREQ)
				binop = ND_BITOR;
			else if (op == TOK_XOREQ)
				binop = ND_BITXOR;
			else if (op == TOK_SHLEQ)
				binop = ND_SHL;
			else if (op == TOK_SHREQ)
				binop = ND_SHR;

			Node *expr;
			if (binop == ND_ADD || binop == ND_SUB)
				expr = make_additive(binop, read_lhs, rhs);
			else if (binop == ND_MUL || binop == ND_DIV) {
				if ((expr_node_has_complex_value(read_lhs) && expr_node_has_imaginary_value(rhs)) ||
				    (expr_node_has_imaginary_value(read_lhs) && expr_node_has_complex_value(rhs))) {
					Node *lhs_complex = read_lhs;
					Node *rhs_complex = rhs;

					if (expr_node_has_imaginary_value(read_lhs))
						lhs_complex = expr_build_complex_from_imaginary(read_lhs, rhs->type);
					if (expr_node_has_imaginary_value(rhs))
						rhs_complex = expr_build_complex_from_imaginary(rhs, read_lhs->type);
					expr = (lhs_complex && rhs_complex)
					     ? expr_build_complex_multiplicative(lhs_complex, rhs_complex, binop)
					     : NULL;
				} else if (expr_node_has_complex_value(read_lhs) && expr_node_is_real_scalar(rhs)) {
					Node *rhs_complex = expr_build_complex_value_cast(rhs, read_lhs->type);
					expr = rhs_complex
					     ? expr_build_complex_multiplicative(read_lhs, rhs_complex, binop)
					     : NULL;
				} else if (expr_node_is_real_scalar(read_lhs) && expr_node_has_complex_value(rhs)) {
					Node *lhs_complex = expr_build_complex_value_cast(read_lhs, rhs->type);
					expr = lhs_complex
					     ? expr_build_complex_multiplicative(lhs_complex, rhs, binop)
					     : NULL;
				} else {
					expr = expr_build_complex_multiplicative(read_lhs, rhs, binop);
				}
				if (!expr)
					expr = expr_build_imaginary_multiplicative(read_lhs, rhs, binop);
				if (!expr)
					expr = new_binary(binop, read_lhs, rhs);
			}
			else
				expr = new_binary(binop, read_lhs, rhs);

			if (node->type && expr->type &&
			    type_is_complex(node->type) &&
			    type_is_complex(expr->type) &&
			    type_equal_unqualified(node->type, expr->type) &&
			    !expr_type_uses_x64_complex_float_abi(node->type))
				node = new_struct_assign(node, expr, type_sizeof(node->type));
			else
				node = new_assign(node, expr);
		}
	}
	parser_profile_scope_leave(PARSER_PROF_EXPR_ASSIGN);
	EXPR_PARSE_DEPTH_LEAVE();
	return node;
}

Node *
parse_comma_expr(void)
{
    Node *node = parse_expr();

    while (lexer_peek()->kind == TOK_COMMA) {
        lexer_next();
        node = new_binary(ND_COMMA, node, parse_expr());
    }

    return node;
}

Node *
parse_expr(void)
{
	Node *tmp;

	parser_profile_scope_enter(PARSER_PROF_EXPR);
	tmp = parse_assignment();
	parser_profile_scope_leave(PARSER_PROF_EXPR);
	return tmp;
}
