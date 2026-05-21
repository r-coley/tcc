#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <stdarg.h>

#include "tcc.h"
#include "ast.h"
#include "lexer.h"

static Type builtin_int    = { TY_INT, 0, 4, 0, 0, "" };
static Type builtin_uint   = { TY_INT, 0, 4, 1, 0, "" };
static Type builtin_char   = { TY_CHAR, 0, 1, 0, 0, "" };
static Type builtin_uchar  = { TY_CHAR, 0, 1, 1, 0, "" };
static Type builtin_short  = { TY_SHORT, 0, 2, 0, 0, "" };
static Type builtin_ushort = { TY_SHORT, 0, 2, 1, 0, "" };

Type *
type_int(void)
{
	return &builtin_int;
}

Type *
type_uint(void)
{
	return &builtin_uint;
}

Type *
type_char(void)
{
	return &builtin_char;
}

Type *
type_uchar(void)
{
	return &builtin_uchar;
}

Type *
type_short(void)
{
	return &builtin_short;
}

Type *
type_ushort(void)
{
	return &builtin_ushort;
}

Type *
type_ptr(Type *base)
{
	Type *t = xcalloc(1, sizeof(Type));
	t->kind = TY_PTR;
	t->base = base;
	t->size = 8;
	t->is_unsigned = 1;
	return t;
}

Type *
type_array(Type *base, int len)
{
	Type *t = xcalloc(1, sizeof(Type));
	t->kind = TY_ARRAY;
	t->base = base;
	t->array_len = len;
	t->size = base->size * len;
	t->is_unsigned = base ? base->is_unsigned : 0;
	return t;
}

Type *
type_struct(const char *name, int size)
{
	Type *t = xcalloc(1, sizeof(Type));
	t->kind = TY_STRUCT;
	t->size = size;
	STRNCPY(t->struct_name, name, sizeof(t->struct_name) - 1);
	return t;
}

static void 
capture_current_location(Node *node)
{
	const Token *token = lexer_peek();

	node->filename_id = token->filename_id;
	node->line = token->line;
	node->column = token->column;

	node->pp_filename_id = token->pp_filename_id;
	node->pp_line = token->pp_line;
	node->pp_column = token->pp_column;
}

static Node *
new_node(NodeKind kind)
{
	Node *node = xcalloc(1, sizeof(Node));
	node->kind = kind;
	node->type = type_int();
	node->elem_size = 4;
	capture_current_location(node);
	return node;
}

Node *
new_num(int value)
{
	Node *node = new_node(ND_NUM);
	node->value = value;
	return node;
}

Node *
new_var(const char *name, int offset)
{
	Node *node = new_node(ND_VAR);
	STRNCPY(node->name, name, sizeof(node->name) - 1);
	node->offset = offset;
	return node;
}

Node *
new_global(const char *name)
{
	Node *node = new_node(ND_GLOBAL);
	STRNCPY(node->name, name, sizeof(node->name) - 1);
	return node;
}

Node *
new_global_index(const char *name, Node *index, int elem_size)
{
	Node *node = new_node(ND_GLOBAL_INDEX);
	STRNCPY(node->name, name, sizeof(node->name) - 1);
	node->left = index;
	node->elem_size = elem_size;
	node->type = elem_size == 1 ? type_char() : type_int();
	return node;
}

Node *
new_member(const char *name, int offset)
{
	Node *node = new_node(ND_MEMBER);
	STRNCPY(node->name, name, sizeof(node->name) - 1);
	node->offset = offset;
	return node;
}

Node *
new_member_ptr(const char *name, Node *base, int field_offset)
{
	Node *node = new_node(ND_MEMBER_PTR);
	STRNCPY(node->name, name, sizeof(node->name) - 1);
	node->left = base;
	node->offset = field_offset;
	return node;
}

Node *
new_index(const char *name, int offset, Node *index)
{
	Node *node = new_node(ND_INDEX);
	STRNCPY(node->name, name, sizeof(node->name) - 1);
	node->offset = offset;
	node->left = index;
	return node;
}

Node *
new_addr(Node *target)
{
	Node *node = new_node(ND_ADDR);
	node->left = target;
	node->is_pointer = 1;
	if (target) {
		node->type = type_ptr(target->type);
		node->elem_size = target->elem_size;
		STRNCPY(node->struct_name, target->struct_name, sizeof(node->struct_name) - 1);
	}
	return node;
}

Node *
new_deref(Node *expr)
{
	Node *node = new_node(ND_DEREF);
	node->left = expr;
	if (expr && expr->type && expr->type->kind == TY_PTR && expr->type->base) {
		node->type = expr->type->base;
		/* elem_size = size of the dereferenced type, not the pointer itself */
		if (expr->type->base->kind == TY_CHAR)
			node->elem_size = 1;
		else if (expr->type->base->kind == TY_SHORT)
			node->elem_size = 2;
		else if (expr->type->base->kind == TY_PTR)
			node->elem_size = 8;
		else if (expr->type->base->kind == TY_STRUCT)
			node->elem_size = expr->type->base->size;
		else if (expr->type->base->kind == TY_ARRAY)
			node->elem_size = expr->type->base->size;
		else
			node->elem_size = 4;
	} else {
		node->elem_size = expr ? expr->elem_size : 4;
	}
	return node;
}

Node *
new_string(const char *value, int label)
{
	Node *node = new_node(ND_STRING);
	STRNCPY(node->string_value, value, sizeof(node->string_value) - 1);
	node->string_label = label;
	node->is_pointer = 1;
	node->type = type_ptr(type_char());
	node->elem_size = 1;
	return node;
}

static Type *
usual_arith_conversion(Type *a, Type *b)
{
	int size_a = a ? a->size : 4;
	int size_b = b ? b->size : 4;
	int size = size_a > size_b ? size_a : size_b;
	int is_unsigned = (a && a->is_unsigned) || (b && b->is_unsigned);

	/* C integer promotions: char/short become int for arithmetic. */
	if (size < 4)
		size = 4;

	if (size == 1)
		return is_unsigned ? type_uchar() : type_char();
	if (size == 2)
		return is_unsigned ? type_ushort() : type_short();

	return is_unsigned ? type_uint() : type_int();
}

Node *
new_binary(NodeKind kind, Node *left, Node *right)
{
	Node *node = new_node(kind);
	node->left = left;
	node->right = right;

	Type *result_type = usual_arith_conversion(left ? left->type : NULL,
	                    right ? right->type : NULL);

	if (kind == ND_EQ || kind == ND_NE || kind == ND_LT ||
	        kind == ND_LE || kind == ND_GT || kind == ND_GE ||
	        kind == ND_LOGICAL_AND || kind == ND_LOGICAL_OR) {
		node->type = type_int();
		node->is_unsigned = (left && left->is_unsigned) || (right && right->is_unsigned) ||
		                    (left && left->type && left->type->is_unsigned) ||
		                    (right && right->type && right->type->is_unsigned);
	} else if (kind == ND_SHL || kind == ND_SHR) {
		/*
		 * C99 6.5.7#3: shifts perform integer promotions on both
		 * operands, but the result type is the promoted left operand.
		 * Do not use the usual arithmetic conversions here; in particular
		 * a signed left operand shifted by an unsigned right operand must
		 * remain signed.
		 */
		Type *lt = left ? left->type : NULL;
		int lsize = lt && lt->size ? lt->size : 4;
		int lunsigned = (left && left->is_unsigned) ||
		                (lt && lt->is_unsigned);

		if (lsize < 4) {
			/* unsigned char/short promote to int on this target. */
			node->type = type_int();
			node->is_unsigned = 0;
			node->elem_size = 4;
		} else {
			node->type = lunsigned ? type_uint() : type_int();
			node->is_unsigned = lunsigned;
			node->elem_size = lsize;
		}
	} else {
		node->type = result_type;
		node->is_unsigned = result_type && result_type->is_unsigned;
		node->elem_size = result_type ? result_type->size : 4;
	}

	return node;
}

Node *
new_unary(NodeKind kind, Node *expr)
{
	Node *node = new_node(kind);
	node->left = expr;
	return node;
}

Node *
new_cast(Node *expr, Type *type)
{
	Node *node = new_node(ND_CAST);
	node->left = expr;
	node->type = type;
	node->is_unsigned = type && type->is_unsigned;
	node->is_pointer = type && type->kind == TY_PTR;

	if (node->is_pointer && type->base)
		node->elem_size = type->base->size;
	else
		node->elem_size = type ? type->size : 4;

	return node;
}

Node *
new_incdec(NodeKind kind, Node *target)
{
	Node *node = new_node(kind);
	node->left = target;
	node->type = target->type;
	node->elem_size = target->elem_size ? target->elem_size : 4;
	node->is_pointer = target->is_pointer;
	return node;
}

Node *
new_assign(Node *var, Node *expr)
{
	Node *node = new_node(ND_ASSIGN);
	node->left = var;
	node->right = expr;
	return node;
}

Node *
new_struct_assign(Node *dst, Node *src, int size)
{
	Node *node = new_node(ND_STRUCT_ASSIGN);
	node->left = dst;
	node->right = src;
	node->value = size;
	return node;
}

Node *
new_decl(const char *name, int offset)
{
	Node *node = new_node(ND_DECL);
	STRNCPY(node->name, name, sizeof(node->name) - 1);
	node->offset = offset;
	return node;
}

Node *
new_array_decl(const char *name, int offset, int array_len)
{
	Node *node = new_node(ND_ARRAY_DECL);
	STRNCPY(node->name, name, sizeof(node->name) - 1);
	node->offset = offset;
	node->array_len = array_len;
	return node;
}

Node *
new_ptr_decl(const char *name, int offset)
{
	Node *node = new_node(ND_PTR_DECL);
	STRNCPY(node->name, name, sizeof(node->name) - 1);
	node->offset = offset;
	node->is_pointer = 1;
	return node;
}

Node *
new_struct_decl(const char *name, int offset)
{
	Node *node = new_node(ND_STRUCT_DECL);
	STRNCPY(node->name, name, sizeof(node->name) - 1);
	node->offset = offset;
	return node;
}

Node *
new_label_stmt(const char *name)
{
	Node *node = new_node(ND_LABEL);
	STRNCPY(node->name, name, sizeof(node->name) - 1);
	return node;
}

Node *
new_goto_stmt(const char *name)
{
	Node *node = new_node(ND_GOTO);
	STRNCPY(node->name, name, sizeof(node->name) - 1);
	return node;
}

Node *
new_return(Node *expr)
{
	Node *node = new_node(ND_RETURN);
	node->left = expr;
	return node;
}

Node *
new_if(Node *cond, Node *then_body, Node *else_body)
{
	Node *node = new_node(ND_IF);
	node->cond = cond;
	node->then_body = then_body;
	node->else_body = else_body;
	return node;
}

Node *
new_conditional(Node *cond, Node *then_expr, Node *else_expr)
{
	Node *node = new_node(ND_COND);
	node->cond = cond;
	node->then_body = then_expr;
	node->else_body = else_expr;

	if (then_expr && else_expr &&
	        then_expr->type && else_expr->type &&
	        then_expr->type->kind == TY_PTR &&
	        else_expr->type->kind == TY_PTR) {
		node->type = then_expr->type;
		node->is_pointer = 1;
		node->elem_size = then_expr->elem_size;
		return node;
	}

	if (then_expr && else_expr &&
	        then_expr->type && else_expr->type &&
	        then_expr->type->kind == TY_STRUCT &&
	        else_expr->type->kind == TY_STRUCT &&
	        STRCMP(then_expr->type->struct_name, else_expr->type->struct_name) == 0) {
		node->type = then_expr->type;
		node->elem_size = then_expr->type->size;
		STRNCPY(node->struct_name, then_expr->type->struct_name,
		        sizeof(node->struct_name) - 1);
		return node;
	}

	node->type = usual_arith_conversion(then_expr ? then_expr->type : NULL,
	                                    else_expr ? else_expr->type : NULL);
	node->is_unsigned = node->type && node->type->is_unsigned;
	node->elem_size = node->type ? node->type->size : 4;
	return node;
}

Node *
new_while(Node *cond, Node *body)
{
	Node *node = new_node(ND_WHILE);
	node->cond = cond;
	node->body = body;
	return node;
}

Node *
new_for(Node *init, Node *cond, Node *inc, Node *body)
{
	Node *node = new_node(ND_FOR);
	node->init = init;
	node->cond = cond;
	node->inc = inc;
	node->body = body;
	return node;
}

Node *
new_do_while(Node *body, Node *cond)
{
	Node *node = new_node(ND_DO_WHILE);
	node->body = body;
	node->cond = cond;
	return node;
}

Node *
new_switch(Node *cond, Node *cases)
{
	Node *node = new_node(ND_SWITCH);
	node->cond = cond;
	node->body = cases;
	return node;
}

Node *
new_case(int value, Node *body)
{
	Node *node = new_node(ND_CASE);
	node->value = value;
	node->body = body;
	return node;
}

Node *
new_default(Node *body)
{
	Node *node = new_node(ND_DEFAULT);
	node->body = body;
	return node;
}

Node *
new_break(void)
{
	return new_node(ND_BREAK);
}

Node *
new_continue(void)
{
	return new_node(ND_CONTINUE);
}

Node *
new_block(Node *body)
{
	Node *node = new_node(ND_BLOCK);
	node->body = body;

	Debug(1,
	      "NEW_BLOCK node=%p body=%s bodyptr=%p name=%s loc=%s:%d:%d\n",
	      (void *)node,
	      body ? node_kind_name(body->kind) : "<null>",
	      (void *)body,
	      (body && body->name[0]) ? &body->name[0] : "<empty>",
	      lexer_filename_name(node->filename_id),
	      node->line,
	      node->column);

	return node;
}

Node *new_func(const char *name, Node *body, int stack_size, int param_count)
{
	Node *node = new_node(ND_FUNC);
	STRNCPY(node->name, name, sizeof(node->name) - 1);
	node->body = body;
	node->stack_size = stack_size;
	node->param_count = param_count;
	return node;
}

Node *
new_call(const char *name, Node *args)
{
	Node *node = new_node(ND_CALL);
	STRNCPY(node->name, name, sizeof(node->name) - 1);
	node->args = args;
	return node;
}

Node *
new_indirect_call(Node *callee, Node *args)
{
	Node *node = new_node(ND_CALL);
	node->left = callee;
	node->args = args;
	return node;
}

Node *
new_func_addr(const char *name)
{
	Node *node = new_node(ND_FUNC_ADDR);
	STRNCPY(node->name, name, sizeof(node->name) - 1);
	node->is_pointer = 1;
	node->elem_size = 4;
	return node;
}

Node *
new_asm(const char *text, int is_volatile)
{
	Node *node = new_node(ND_ASM);

	node->asm_is_volatile = is_volatile;
	if (text)
		STRNCPY(node->string_value, text, sizeof(node->string_value) - 1);
	return node;
}

void 
free_ast(Node *node)
{
	if (!node)
		return;

	free_ast(node->left);
	free_ast(node->right);
	free_ast(node->init);
	free_ast(node->cond);
	free_ast(node->inc);
	free_ast(node->then_body);
	free_ast(node->else_body);
	free_ast(node->body);
	free_ast(node->args);
	free_ast(node->next);
	xfree(node);
}

static int 
is_num(Node *node)
{
	return node && node->kind == ND_NUM;
}

static int 
fold_is_unsigned(Node *node)
{
	return node && (node->is_unsigned || (node->type && node->type->is_unsigned));
}

static int 
fold_cast_value(int value, Type *type)
{
	if (!type)
		return value;

	if (type->size == 1) {
		value &= 0xff;
		if (!type->is_unsigned && (value & 0x80))
			value |= ~0xff;
		return value;
	}

	if (type->size == 2) {
		value &= 0xffff;
		if (!type->is_unsigned && (value & 0x8000))
			value |= ~0xffff;
		return value;
	}

	return value;
}

static Node *
replace_with_num(Node *node, int value)
{
	free_ast(node->left);
	free_ast(node->right);
	free_ast(node->init);
	free_ast(node->cond);
	free_ast(node->inc);
	free_ast(node->then_body);
	free_ast(node->else_body);
	free_ast(node->body);
	free_ast(node->args);
	node->left = NULL;
	node->right = NULL;
	node->init = NULL;
	node->cond = NULL;
	node->inc = NULL;
	node->then_body = NULL;
	node->else_body = NULL;
	node->body = NULL;
	node->args = NULL;
	node->kind = ND_NUM;
	node->value = value;
	/* Preserve folded node metadata.  This matters for unsigned constant
	 * comparisons after casts, e.g. (unsigned int)1 > -1. */
	if (node->type && node->type->is_unsigned)
		node->is_unsigned = 1;
	if (!node->elem_size && node->type)
		node->elem_size = node->type->size;
	return node;
}


static Node *
preserve_replacement_next(Node *replacement, Node *original)
{
	if (!replacement || !original)
		return replacement;

	/* fold_constants() is called on statement-list nodes as well as
	 * expression trees.  When a node is replaced with one of its children
	 * (for example constant-folding a conditional expression), keep the
	 * original statement-list chain intact.  Otherwise the statement after
	 * the folded expression can be silently dropped.
	 */
	if (!replacement->next)
		replacement->next = original->next;
	return replacement;
}

Node *
fold_constants(Node *node)
{
	if (!node)
		return NULL;

	/* Recurse into children (these form trees, not lists) */
	node->left      = fold_constants(node->left);
	node->right     = fold_constants(node->right);
	node->init      = fold_constants(node->init);
	node->cond      = fold_constants(node->cond);
	node->inc       = fold_constants(node->inc);
	node->then_body = fold_constants(node->then_body);
	node->else_body = fold_constants(node->else_body);
	node->body      = fold_constants(node->body);
	node->args      = fold_constants(node->args);
	/* Iterate over ->next to avoid deep recursion on long statement lists */
	for (Node *n = node->next; n; n = n->next) {
		n->left      = fold_constants(n->left);
		n->right     = fold_constants(n->right);
		n->init      = fold_constants(n->init);
		n->cond      = fold_constants(n->cond);
		n->inc       = fold_constants(n->inc);
		n->then_body = fold_constants(n->then_body);
		n->else_body = fold_constants(n->else_body);
		n->body      = fold_constants(n->body);
		n->args      = fold_constants(n->args);
	}

	/* v169 cast folding: preserve target type metadata and apply basic
	 * char/short truncation/sign-extension. */
	if (node->kind == ND_CAST && is_num(node->left)) {
		int value = fold_cast_value(node->left->value, node->type);
		node->is_unsigned = node->type && node->type->is_unsigned;
		node->elem_size = node->type ? node->type->size : node->elem_size;
		return replace_with_num(node, value);
	}

	if (node->kind == ND_NEG && is_num(node->left))
		return replace_with_num(node, -node->left->value);

	if (node->kind == ND_NOT && is_num(node->left))
		return replace_with_num(node, node->left->value == 0);

	if (is_num(node->left) && is_num(node->right)) {
		int a = node->left->value;
		int b = node->right->value;
		int use_unsigned = node->is_unsigned || fold_is_unsigned(node->left) || fold_is_unsigned(node->right);
		unsigned ua = (unsigned)a;
		unsigned ub = (unsigned)b;

		switch (node->kind) {
		case ND_ADD:
			return replace_with_num(node, a + b);
		case ND_SUB:
			return replace_with_num(node, a - b);
		case ND_MUL:
			return replace_with_num(node, a * b);
		case ND_BITAND:
			return replace_with_num(node, a & b);
		case ND_BITOR:
			return replace_with_num(node, a | b);
		case ND_BITXOR:
			return replace_with_num(node, a ^ b);
		case ND_SHL:
			return replace_with_num(node, a << b);
		case ND_SHR:
			return replace_with_num(node, use_unsigned ? (int)(ua >> ub) : (a >> b));
		case ND_DIV:
			if (b != 0)
				return replace_with_num(node, use_unsigned ? (int)(ua / ub) : (a / b));
			return node;
		case ND_MOD:
			if (b != 0)
				return replace_with_num(node, use_unsigned ? (int)(ua % ub) : (a % b));
			return node;
		case ND_EQ:
			return replace_with_num(node, use_unsigned ? (ua == ub) : (a == b));
		case ND_NE:
			return replace_with_num(node, use_unsigned ? (ua != ub) : (a != b));
		case ND_LT:
			return replace_with_num(node, use_unsigned ? (ua < ub) : (a < b));
		case ND_LE:
			return replace_with_num(node, use_unsigned ? (ua <= ub) : (a <= b));
		case ND_GT:
			return replace_with_num(node, use_unsigned ? (ua > ub) : (a > b));
		case ND_GE:
			return replace_with_num(node, use_unsigned ? (ua >= ub) : (a >= b));
		case ND_LOGICAL_AND:
			return replace_with_num(node, (a != 0) && (b != 0));
		case ND_LOGICAL_OR:
			return replace_with_num(node, (a != 0) || (b != 0));
		default:
			return node;
		}
	}

	if (node->kind == ND_COND && node->cond && node->then_body && node->else_body &&
	        node->cond->kind == ND_NUM) {
		Node *chosen = node->cond->value ? node->then_body : node->else_body;
		return preserve_replacement_next(fold_constants(chosen), node);
	}

	/* v78 algebraic identities: safe simplifications without side effects. */
	if (node->kind == ND_ADD && is_num(node->right) && node->right->value == 0)
		return preserve_replacement_next(node->left, node);
	if (node->kind == ND_ADD && is_num(node->left) && node->left->value == 0)
		return preserve_replacement_next(node->right, node);
	if (node->kind == ND_SUB && is_num(node->right) && node->right->value == 0)
		return preserve_replacement_next(node->left, node);
	if (node->kind == ND_MUL && is_num(node->right) && node->right->value == 1)
		return preserve_replacement_next(node->left, node);
	if (node->kind == ND_MUL && is_num(node->left) && node->left->value == 1)
		return preserve_replacement_next(node->right, node);
	if (node->kind == ND_DIV && is_num(node->right) && node->right->value == 1)
		return preserve_replacement_next(node->left, node);
	if (node->kind == ND_BITOR && is_num(node->right) && node->right->value == 0)
		return preserve_replacement_next(node->left, node);
	if (node->kind == ND_BITXOR && is_num(node->right) && node->right->value == 0)
		return preserve_replacement_next(node->left, node);
	if (node->kind == ND_SHL && is_num(node->right) && node->right->value == 0)
		return preserve_replacement_next(node->left, node);
	if (node->kind == ND_SHR && is_num(node->right) && node->right->value == 0)
		return preserve_replacement_next(node->left, node);

	return node;
}


static int
contains_label(Node *node)
{
	for (; node; node = node->next) {
		if (node->kind == ND_LABEL)
			return 1;
		if (contains_label(node->left) ||
		    contains_label(node->right) ||
		    contains_label(node->init) ||
		    contains_label(node->cond) ||
		    contains_label(node->inc) ||
		    contains_label(node->then_body) ||
		    contains_label(node->else_body) ||
		    contains_label(node->args) ||
		    contains_label(node->body))
			return 1;
	}

	return 0;
}

static int is_terminal_statement(Node *node)
{
	return node && (node->kind == ND_RETURN || node->kind == ND_BREAK ||
	                node->kind == ND_CONTINUE || node->kind == ND_GOTO);
}

static Node *
eliminate_dead_code_list(Node *node)
{
	Node head = {0};
	Node *cur = &head;
	int dead = 0;

	while (node) {
		Node *next = node->next;
		node->next = NULL;

		/* A label is never dead: it may be a goto target from anywhere.
		 * Revive the dead-code state when we hit one. */
		if (dead && node->kind == ND_LABEL)
			dead = 0;

		if (dead && !contains_label(node)) {
			free_ast(node);
		} else {
			/*
			 * A goto may target a label nested inside a compound statement.
			 * Do not discard the whole block just because it follows a
			 * terminal statement; otherwise we emit the branch but drop the
			 * target label.
			 */
			node = eliminate_dead_code(node);
			cur->next = node;
			cur = node;

			if (is_terminal_statement(node))
				dead = 1;
			else if (dead && contains_label(node))
				dead = 0;
		}

		node = next;
	}

	return head.next;
}

Node *
eliminate_dead_code(Node *node)
{
	if (!node)
		return NULL;

	node->left      = eliminate_dead_code(node->left);
	node->right     = eliminate_dead_code(node->right);
	node->init      = eliminate_dead_code(node->init);
	node->cond      = eliminate_dead_code(node->cond);
	node->inc       = eliminate_dead_code(node->inc);
	node->then_body = eliminate_dead_code(node->then_body);
	node->else_body = eliminate_dead_code(node->else_body);
	node->args      = eliminate_dead_code(node->args);
	/* Iterate over next chain instead of recursing to avoid stack overflow */
	for (Node *n = node->next; n; n = n->next) {
		n->left      = eliminate_dead_code(n->left);
		n->right     = eliminate_dead_code(n->right);
		n->init      = eliminate_dead_code(n->init);
		n->cond      = eliminate_dead_code(n->cond);
		n->inc       = eliminate_dead_code(n->inc);
		n->then_body = eliminate_dead_code(n->then_body);
		n->else_body = eliminate_dead_code(n->else_body);
		n->args      = eliminate_dead_code(n->args);
		if (n->kind == ND_BLOCK || n->kind == ND_FUNC)
			n->body = eliminate_dead_code_list(n->body);
		else
			n->body = eliminate_dead_code(n->body);
	}

	if (node->kind == ND_BLOCK || node->kind == ND_FUNC)
		node->body = eliminate_dead_code_list(node->body);
	else
		node->body = eliminate_dead_code(node->body);

	return node;
}

static const char *
kind_name(NodeKind kind)
{
	switch (kind) {
	case ND_LABEL:
		return "label";
	case ND_GOTO:
		return "goto";
	case ND_NUM:
		return "NUM";
	case ND_VAR:
		return "VAR";
	case ND_GLOBAL:
		return "GLOBAL";
	case ND_GLOBAL_INDEX:
		return "GLOBAL_INDEX";
	case ND_MEMBER:
		return "MEMBER";
	case ND_MEMBER_PTR:
		return "MEMBER_PTR";
	case ND_INDEX:
		return "INDEX";
	case ND_ADDR:
		return "ADDR";
	case ND_DEREF:
		return "DEREF";
	case ND_STRING:
		return "STRING";
	case ND_FUNC_ADDR:
		return "FUNC_ADDR";
	case ND_ADD:
		return "ADD";
	case ND_SUB:
		return "SUB";
	case ND_MUL:
		return "MUL";
	case ND_DIV:
		return "DIV";
	case ND_MOD:
		return "MOD";
	case ND_BITAND:
		return "BITAND";
	case ND_BITOR:
		return "BITOR";
	case ND_BITXOR:
		return "BITXOR";
	case ND_SHL:
		return "SHL";
	case ND_SHR:
		return "SHR";
	case ND_EQ:
		return "EQ";
	case ND_NE:
		return "NE";
	case ND_LT:
		return "LT";
	case ND_LE:
		return "LE";
	case ND_GT:
		return "GT";
	case ND_GE:
		return "GE";
	case ND_LOGICAL_AND:
		return "AND";
	case ND_LOGICAL_OR:
		return "OR";
	case ND_COND:
		return "COND";
	case ND_NEG:
		return "NEG";
	case ND_BITNOT:
		return "BITNOT";
	case ND_NOT:
		return "NOT";
	case ND_CAST:
		return "CAST";
	case ND_PRE_INC:
		return "PRE_INC";
	case ND_PRE_DEC:
		return "PRE_DEC";
	case ND_POST_INC:
		return "POST_INC";
	case ND_POST_DEC:
		return "POST_DEC";
	case ND_ASSIGN:
		return "ASSIGN";
	case ND_STRUCT_ASSIGN:
		return "STRUCT_ASSIGN";
	case ND_DECL:
		return "DECL";
	case ND_ARRAY_DECL:
		return "ARRAY_DECL";
	case ND_PTR_DECL:
		return "PTR_DECL";
	case ND_STRUCT_DECL:
		return "STRUCT_DECL";
	case ND_RETURN:
		return "RETURN";
	case ND_IF:
		return "IF";
	case ND_WHILE:
		return "WHILE";
	case ND_FOR:
		return "FOR";
	case ND_DO_WHILE:
		return "DO_WHILE";
	case ND_SWITCH:
		return "SWITCH";
	case ND_CASE:
		return "CASE";
	case ND_DEFAULT:
		return "DEFAULT";
	case ND_BREAK:
		return "BREAK";
	case ND_CONTINUE:
		return "CONTINUE";
	case ND_BLOCK:
		return "BLOCK";
	case ND_FUNC:
		return "FUNC";
	case ND_CALL:
		return "CALL";
	case ND_ASM:
		return "ASM";
	case ND_COMMA:
		return "COMMA";
	}
	return "UNKNOWN";
}

static void 
pad(int indent)
{
	for (int i = 0; i < indent; i++)
		putchar(' ');
}

void 
dump_ast(Node *node, int indent)
{
	if (!node)
		return;

	for (Node *n = node; n; n = n->next) {
		pad(indent);
		printf("%s", kind_name(n->kind));

		if (n->kind == ND_NUM)
			printf(" %d", n->value);
		if (n->kind == ND_STRING)
			printf(" Lstr%d \"%s\"", n->string_label, n->string_value);
		if (n->kind == ND_VAR || n->kind == ND_MEMBER || n->kind == ND_MEMBER_PTR || n->kind == ND_INDEX || n->kind == ND_DECL || n->kind == ND_ARRAY_DECL || n->kind == ND_PTR_DECL || n->kind == ND_FUNC || n->kind == ND_CALL)
			printf(" %s", n->name);
		if (n->kind == ND_VAR || n->kind == ND_MEMBER || n->kind == ND_MEMBER_PTR || n->kind == ND_INDEX || n->kind == ND_DECL || n->kind == ND_ARRAY_DECL || n->kind == ND_PTR_DECL || n->kind == ND_STRUCT_DECL)
			printf(" offset=%d", n->offset);
		if (n->kind == ND_ARRAY_DECL)
			printf(" len=%d", n->array_len);
		if (n->kind == ND_FUNC)
			printf(" stack=%d params=%d", n->stack_size, n->param_count);

		printf("\n");

		if (n->init) {
			pad(indent + 2);
			printf("init:\n");
			dump_ast(n->init, indent + 4);
		}

		if (n->cond) {
			pad(indent + 2);
			printf("cond:\n");
			dump_ast(n->cond, indent + 4);
		}

		if (n->inc) {
			pad(indent + 2);
			printf("inc:\n");
			dump_ast(n->inc, indent + 4);
		}

		if (n->left) {
			pad(indent + 2);
			printf("left:\n");
			dump_ast(n->left, indent + 4);
		}

		if (n->right) {
			pad(indent + 2);
			printf("right:\n");
			dump_ast(n->right, indent + 4);
		}

		if (n->args) {
			pad(indent + 2);
			printf("args:\n");
			dump_ast(n->args, indent + 4);
		}

		if (n->then_body) {
			pad(indent + 2);
			printf("then:\n");
			dump_ast(n->then_body, indent + 4);
		}

		if (n->else_body) {
			pad(indent + 2);
			printf("else:\n");
			dump_ast(n->else_body, indent + 4);
		}

		if (n->body) {
			pad(indent + 2);
			printf("body:\n");
			dump_ast(n->body, indent + 4);
		}
	}
}

const char *
node_kind_name(NodeKind kind)
{
	switch (kind) {
#define X(k) case k: return #k
		X(ND_NUM);
		X(ND_VAR);
		X(ND_GLOBAL);
		X(ND_GLOBAL_INDEX);
		X(ND_MEMBER);
		X(ND_MEMBER_PTR);
		X(ND_INDEX);
		X(ND_ADDR);
		X(ND_DEREF);
		X(ND_STRING);
		X(ND_FUNC_ADDR);
		X(ND_ADD);
		X(ND_SUB);
		X(ND_MUL);
		X(ND_DIV);
		X(ND_MOD);
		X(ND_BITAND);
		X(ND_BITOR);
		X(ND_BITXOR);
		X(ND_SHL);
		X(ND_SHR);
		X(ND_EQ);
		X(ND_NE);
		X(ND_LT);
		X(ND_LE);
		X(ND_GT);
		X(ND_GE);
		X(ND_LOGICAL_AND);
		X(ND_LOGICAL_OR);
		X(ND_COND);
		X(ND_NEG);
		X(ND_BITNOT);
		X(ND_NOT);
		X(ND_CAST);
		X(ND_PRE_INC);
		X(ND_PRE_DEC);
		X(ND_POST_INC);
		X(ND_POST_DEC);
		X(ND_ASSIGN);
		X(ND_STRUCT_ASSIGN);
		X(ND_DECL);
		X(ND_ARRAY_DECL);
		X(ND_PTR_DECL);
		X(ND_STRUCT_DECL);
		X(ND_RETURN);
		X(ND_LABEL);
		X(ND_GOTO);
		X(ND_IF);
		X(ND_WHILE);
		X(ND_FOR);
		X(ND_DO_WHILE);
		X(ND_SWITCH);
		X(ND_CASE);
		X(ND_DEFAULT);
		X(ND_BREAK);
		X(ND_CONTINUE);
		X(ND_BLOCK);
		X(ND_FUNC);
		X(ND_CALL);
		X(ND_ASM);
		X(ND_COMMA);
#undef X
	default:
		return "ND_UNKNOWN";
	}
}


