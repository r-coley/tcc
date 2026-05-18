#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tcc.h"
#include "emit.h"
#include "parser.h"

static void emit_statement(Node *node, Codegen *cg);
static void emit_expr(Node *node, Codegen *cg);
static void emit_statement(Node *node, Codegen *cg);
static void emit_lvalue_addr(Node *node, Codegen *cg);

static int next_label_id = 1;
static int return_label = 0;

#define LOOP_STACK_MAX 128
static int break_labels[LOOP_STACK_MAX];
static int continue_labels[LOOP_STACK_MAX];
static int loop_depth = 0;

static void 
emit_raw_string_literal(const char *value)
{
	printf("    .asciz \"");
	for (const unsigned char *p = (const unsigned char *)value; *p; p++) {
		if (*p == '\\' || *p == '"')
			printf("\\%c", *p);
		else if (*p == '\n')
			printf("\\n");
		else if (*p == '\t')
			printf("\\t");
		else
			printf("%c", *p);
	}
	printf("\"\n");
}

/*
 * v65 lvalue scaffold
 *
 * Current codegen primarily treats every expression as an rvalue.  The next
 * correctness step is to distinguish addressable expressions (lvalues) from
 * plain values (rvalues).  This scaffold is intentionally non-invasive: it
 * preserves all existing behavior while introducing the small data model that
 * later versions will use for a real lvalue-address lowering pass.
 *
 * Intended model:
 *
 *   emit_lvalue(expr) -> address of expr in accumulator
 *   emit_rvalue(expr) -> value of expr in accumulator
 *   store(address, value) for assignment/compound assignment/++/--
 *
 * This will unlock:
 *
 *   a[i] += 1;
 *   *p += 1;
 *   a[i]++;
 *   (*p)++;
 */
typedef struct CGValue {
	int is_lvalue;
	int elem_size;
	int is_pointer;
} CGValue;

static inline __attribute__((unused)) CGValue 
cgvalue_rvalue(int elem_size, int is_pointer)
{
	CGValue value;
	value.is_lvalue = 0;
	value.elem_size = elem_size;
	value.is_pointer = is_pointer;
	return value;
}

static inline __attribute__((unused)) CGValue 
cgvalue_lvalue(int elem_size, int is_pointer)
{
	CGValue value;
	value.is_lvalue = 1;
	value.elem_size = elem_size;
	value.is_pointer = is_pointer;
	return value;
}

static int __attribute__((unused)) 
is_lvalue_node(Node *node)
{
	if (!node)
		return 0;

	return node->kind == ND_VAR ||
	       node->kind == ND_GLOBAL ||
	       node->kind == ND_GLOBAL_INDEX ||
	       node->kind == ND_MEMBER ||
	       node->kind == ND_MEMBER_PTR ||
	       node->kind == ND_INDEX ||
	       node->kind == ND_DEREF;
}

static void 
push_loop(int break_label, int continue_label)
{
	if (loop_depth >= LOOP_STACK_MAX) {
		ICE("Too many nested loops");
	}

	break_labels[loop_depth] = break_label;
	continue_labels[loop_depth] = continue_label;
	loop_depth++;
}

static void 
pop_loop(void)
{
	if (loop_depth > 0)
		loop_depth--;
}

static int 
current_break_label(void)
{
	if (loop_depth == 0) {
		ICE("break used outside loop");
	}

	return break_labels[loop_depth - 1];
}

static int 
current_continue_label(void)
{
	if (loop_depth == 0) {
		ICE("continue used outside loop");
	}

	return continue_labels[loop_depth - 1];
}

static int 
new_label(void)
{
	return next_label_id++;
}

static int 
is_simple_rhs(Node *node)
{
	if (!node)
		return 0;

	return node->kind == ND_NUM ||
	       node->kind == ND_VAR ||
	       node->kind == ND_MEMBER ||
	       node->kind == ND_INDEX ||
	       node->kind == ND_STRING;
}

static int 
emit_compare_is_unsigned(Node *node)
{
	return node && (node->value || node->is_unsigned ||
	                (node->type && node->type->is_unsigned));
}

static void 
emit_binary_operands(Node *node, Codegen *cg)
{
	emit_expr(node->left, cg);

	if (is_simple_rhs(node->right)) {
		cg->emit_acc_to_tmp();
		emit_expr(node->right, cg);
		return;
	}

	cg->emit_push_acc();
	emit_expr(node->right, cg);
	cg->emit_pop_to_tmp();
}

static int 
count_list(Node *node)
{
	int count = 0;

	for (Node *n = node; n; n = n->next)
		count++;

	return count;
}

static void 
emit_string_literals(Node *node, Codegen *cg)
{
	if (!node)
		return;

	for (Node *n = node; n; n = n->next) {
		if (n->kind == ND_STRING)
			cg->emit_string_literal(n->string_label, n->string_value);

		emit_string_literals(n->left, cg);
		emit_string_literals(n->right, cg);
		emit_string_literals(n->init, cg);
		emit_string_literals(n->cond, cg);
		emit_string_literals(n->inc, cg);
		emit_string_literals(n->then_body, cg);
		emit_string_literals(n->else_body, cg);
		emit_string_literals(n->body, cg);
		emit_string_literals(n->args, cg);
	}
}

static void 
emit_lvalue_addr(Node *node, Codegen *cg)
{
	switch (node->kind) {
	case ND_VAR:
		cg->emit_addr_local(node->offset);
		return;

	case ND_INDEX:
		emit_expr(node->left, cg);
		cg->emit_addr_indexed(node->offset, node->elem_size);
		return;

	case ND_MEMBER:
		cg->emit_addr_local(node->offset);
		return;

	case ND_MEMBER_PTR:
		emit_expr(node->left, cg);
		cg->emit_add_offset(node->offset);
		return;

	case ND_DEREF:
		emit_expr(node->left, cg);
		return;

	default:
		ICE("Unsupported ++/-- lvalue");
	}
}

static void 
emit_incdec(Node *node, Codegen *cg, int is_inc, int is_postfix)
{
	int size = node->left->elem_size ? node->left->elem_size : 4;

	if (node->left->kind == ND_GLOBAL) {
		cg->emit_load_global(node->left->name, node->left->is_pointer ? 8 : (node->left->elem_size ? node->left->elem_size : 4));
		cg->emit_acc_to_saved();

		cg->emit_acc_to_tmp();
		cg->emit_load_imm(1);

		if (is_inc)
			cg->emit_add();
		else
			cg->emit_sub();

		cg->emit_store_global(node->left->name, node->left->is_pointer ? 8 : (node->left->elem_size ? node->left->elem_size : 4));

		if (is_postfix)
			cg->emit_saved_to_acc();
		return;
	}

	emit_lvalue_addr(node->left, cg);
	cg->emit_push_acc();

	cg->emit_load_deref(size);
	cg->emit_acc_to_saved();

	cg->emit_acc_to_tmp();
	cg->emit_load_imm(1);

	if (is_inc)
		cg->emit_add();
	else
		cg->emit_sub();

	cg->emit_pop_to_tmp();
	cg->emit_store_deref(size);

	if (is_postfix)
		cg->emit_saved_to_acc();
}


static void
emit_struct_lvalue_addr(Node *node, Codegen *cg)
{
	if (!node) {
		ICE("Cannot take address of null struct expression");
	}

	switch (node->kind) {
	case ND_VAR:
		cg->emit_addr_local(node->offset);
		return;

	case ND_GLOBAL:
		/* Global objects live at their symbol address.  Reuse the existing
		 * symbol-address emitter; on Mach-O this emits adrp/add for _name.
		 */
		cg->emit_load_func_addr(node->name);
		return;

	case ND_DEREF:
		emit_expr(node->left, cg);
		return;

	case ND_MEMBER:
		cg->emit_addr_local(node->offset);
		return;

	case ND_MEMBER_PTR:
		emit_expr(node->left, cg);
		if (node->offset)
			cg->emit_add_offset(node->offset);
		return;

	case ND_INDEX:
		emit_expr(node->left, cg);
		cg->emit_addr_indexed(node->offset, node->elem_size);
		return;

	case ND_GLOBAL_INDEX:
		emit_expr(node->left, cg);
		cg->emit_addr_indexed(node->offset, node->elem_size);
		return;

	default:
		ICE("Struct return requires addressable struct expression");
	}
}


static int
emit_is_struct_argument(Node *node)
{
	return node && node->type && node->type->kind == TY_STRUCT;
}

static void
emit_call_arg(Node *node, Codegen *cg)
{
	/*
	 * This compiler currently lowers by-value struct parameters as a hidden
	 * pointer on the callee side: parse_function() creates a __paramptr_N
	 * local and copies fields from that pointer into the user-visible struct
	 * parameter.  Therefore the caller must pass the address of the struct
	 * object, not the first word/byte of the object.
	 *
	 * Without this, calls such as fa_s1(s1) pass x0 = '0' and the callee
	 * then treats 0x30 as a pointer, crashing as soon as it evaluates a.x.
	 */
	if (emit_is_struct_argument(node)) {
		emit_struct_lvalue_addr(node, cg);
		return;
	}

	emit_expr(node, cg);
}

static void 
emit_expr(Node *node, Codegen *cg)
{
	switch (node->kind) {
	case ND_LABEL:
		return;

	case ND_GOTO:
		return;

	case ND_NUM:
		cg->emit_load_imm(node->value);
		return;

	case ND_STRING:
		cg->emit_load_string(node->string_label);
		return;

	case ND_FUNC_ADDR:
		cg->emit_load_func_addr(node->name);
		return;

	case ND_ASM:
		cg->emit_inline_asm(node->string_value);
		return;

	case ND_VAR:
		if (node->is_pointer)
			cg->emit_load_ptr_local(node->offset);
		else
			cg->emit_load_local_sized(node->offset, node->elem_size ? node->elem_size : 4);
		return;

	case ND_GLOBAL:
		cg->emit_load_global(node->name, node->is_pointer ? 8 : (node->elem_size ? node->elem_size : 4));
		return;

	case ND_GLOBAL_INDEX:
		emit_expr(node->left, cg);
		cg->emit_load_global_indexed(node->name, node->elem_size);
		return;

	case ND_MEMBER:
		cg->emit_load_local_sized(node->offset, node->elem_size ? node->elem_size : 4);
		return;

	case ND_MEMBER_PTR:
		emit_expr(node->left, cg);
		if (node->is_array_field) {
			/* Array field decays to pointer: emit address (base + offset) */
			if (node->offset)
				cg->emit_add_offset(node->offset);
			/* else: address is already in x0 (base ptr, offset=0) */
		} else {
			cg->emit_load_member_ptr(node->offset, node->elem_size ? node->elem_size : 4);
		}
		return;

	case ND_INDEX:
		emit_expr(node->left, cg);
		cg->emit_load_indexed(node->offset, node->elem_size);
		return;

	case ND_ADDR:
		if (node->left->kind == ND_VAR) {
			cg->emit_addr_local(node->left->offset);
			return;
		}

		if (node->left->kind == ND_GLOBAL) {
			cg->emit_load_global(node->left->name, node->left->is_pointer ? 8 : (node->left->elem_size ? node->left->elem_size : 4));  /* load address of global */
			return;
		}

		if (node->left->kind == ND_MEMBER) {
			cg->emit_addr_local(node->left->offset);
			return;
		}

		if (node->left->kind == ND_GLOBAL_INDEX) {
			emit_expr(node->left->left, cg);
			cg->emit_addr_indexed(node->left->offset, node->left->elem_size);
			return;
		}

		if (node->left->kind == ND_INDEX) {
			emit_expr(node->left->left, cg);
			cg->emit_addr_indexed(node->left->offset, node->left->elem_size);
			return;
		}

		if (node->left->kind == ND_DEREF) {
			emit_expr(node->left->left, cg);
			return;
		}

		if (node->left->kind == ND_MEMBER_PTR) {
			emit_expr(node->left->left, cg);
			cg->emit_add_offset(node->left->offset);
			return;
		}

		node_error_at(node,
		              "Cannot take address of expression "
		              "(kind=%s left=%s right=%s name=%s)",
		              node_kind_name(node->kind),
		              node->left  ? node_kind_name(node->left->kind)  : "<null>",
		              node->right ? node_kind_name(node->right->kind) : "<null>",
		              node->name[0] ? node->name : "<empty>");
		exit(1);

	case ND_DEREF:
		emit_expr(node->left, cg);
		/* (*fptr)() == fptr(): skip load if left is a call returning a pointer */
		if (node->left && node->left->kind == ND_CALL && node->left->is_pointer)
			return;
		cg->emit_load_deref(node->elem_size);
		return;

	case ND_NEG:
		cg->emit_load_imm(0);
		cg->emit_push_acc();
		emit_expr(node->left, cg);
		cg->emit_pop_to_tmp();
		cg->emit_sub();
		return;

	case ND_NOT: {
		int true_label = new_label();
		int end_label = new_label();

		emit_expr(node->left, cg);
		cg->emit_branch_if_zero(true_label);

		cg->emit_load_imm(0);
		cg->emit_branch(end_label);

		cg->emit_label(true_label);
		cg->emit_load_imm(1);

		cg->emit_label(end_label);
		return;
	}

	case ND_ADD:
		emit_binary_operands(node, cg);
		if (node->is_pointer)
			cg->emit_ptr_add(node->elem_size);
		else
			cg->emit_add();
		return;

	case ND_SUB:
		emit_binary_operands(node, cg);
		if (node->is_pointer)
			cg->emit_ptr_sub(node->elem_size);
		else
			cg->emit_sub();
		return;

	case ND_MUL:
		emit_binary_operands(node, cg);
		cg->emit_mul();
		return;

	case ND_DIV:
		emit_binary_operands(node, cg);
		cg->emit_div();
		return;

	case ND_MOD:
		emit_binary_operands(node, cg);
		cg->emit_mod();
		return;

	case ND_BITAND:
		emit_binary_operands(node, cg);
		cg->emit_bitand();
		return;

	case ND_BITOR:
		emit_binary_operands(node, cg);
		cg->emit_bitor();
		return;

	case ND_BITNOT:
		emit_binary_operands(node, cg);
		cg->emit_bitnot();
		return;

	case ND_BITXOR:
		emit_binary_operands(node, cg);
		cg->emit_bitxor();
		return;

	case ND_SHL:
		emit_binary_operands(node, cg);
		cg->emit_shl();
		return;

	case ND_SHR:
		emit_binary_operands(node, cg);
		cg->emit_shr();
		return;

	case ND_CAST:
		emit_expr(node->left, cg);
		cg->emit_cast(node->type ? node->type->size : 4);
		return;

	case ND_EQ:
		emit_binary_operands(node, cg);
		cg->emit_cmp_eq();
		return;

	case ND_NE:
		emit_binary_operands(node, cg);
		cg->emit_cmp_ne();
		return;

	case ND_LT:
		emit_binary_operands(node, cg);
		if (emit_compare_is_unsigned(node) && cg->emit_cmp_lt_u)
			cg->emit_cmp_lt_u();
		else
			cg->emit_cmp_lt();
		return;

	case ND_LE:
		emit_binary_operands(node, cg);
		if (emit_compare_is_unsigned(node) && cg->emit_cmp_le_u)
			cg->emit_cmp_le_u();
		else
			cg->emit_cmp_le();
		return;

	case ND_GT:
		emit_binary_operands(node, cg);
		if (emit_compare_is_unsigned(node) && cg->emit_cmp_gt_u)
			cg->emit_cmp_gt_u();
		else
			cg->emit_cmp_gt();
		return;

	case ND_GE:
		emit_binary_operands(node, cg);
		if (emit_compare_is_unsigned(node) && cg->emit_cmp_ge_u)
			cg->emit_cmp_ge_u();
		else
			cg->emit_cmp_ge();
		return;

	case ND_LOGICAL_AND: {
		int false_label = new_label();
		int end_label = new_label();

		emit_expr(node->left, cg);
		cg->emit_branch_if_zero(false_label);

		emit_expr(node->right, cg);
		cg->emit_branch_if_zero(false_label);

		cg->emit_load_imm(1);
		cg->emit_branch(end_label);

		cg->emit_label(false_label);
		cg->emit_load_imm(0);

		cg->emit_label(end_label);
		return;
	}

	case ND_LOGICAL_OR: {
		int true_label = new_label();
		int end_label = new_label();

		emit_expr(node->left, cg);
		cg->emit_branch_if_nonzero(true_label);

		emit_expr(node->right, cg);
		cg->emit_branch_if_nonzero(true_label);

		cg->emit_load_imm(0);
		cg->emit_branch(end_label);

		cg->emit_label(true_label);
		cg->emit_load_imm(1);

		cg->emit_label(end_label);
		return;
	}

	case ND_COND: {
		int else_label = new_label();
		int end_label = new_label();

		emit_expr(node->cond, cg);
		cg->emit_branch_if_zero(else_label);

		emit_expr(node->then_body, cg);
		cg->emit_branch(end_label);

		cg->emit_label(else_label);
		emit_expr(node->else_body, cg);

		cg->emit_label(end_label);
		return;
	}

	case ND_COMMA:
		/* evaluate left for side effects, result is right */
		emit_statement(node->left, cg);
		emit_expr(node->right, cg);
		return;

	case ND_STRUCT_ASSIGN:
		cg->emit_copy_local(node->left->offset, node->right->offset, node->value);
		return;

	case ND_PRE_INC:
		emit_incdec(node, cg, 1, 0);
		return;

	case ND_PRE_DEC:
		emit_incdec(node, cg, 0, 0);
		return;

	case ND_POST_INC:
		emit_incdec(node, cg, 1, 1);
		return;

	case ND_POST_DEC:
		emit_incdec(node, cg, 0, 1);
		return;

	case ND_ASSIGN:
		if (node->right->kind == ND_CALL && node->right->returns_struct && node->left->type && node->left->type->kind == TY_STRUCT) {
			int count = count_list(node->right->args);

			if (count + 1 > 8) {
				ICE("Only up to 8 function arguments are supported by the current call ABI");
			}

			Node *arg_nodes[8] = {0};
			int i = 0;

			for (Node *arg = node->right->args; arg; arg = arg->next)
				arg_nodes[i++] = arg;

			for (int j = count - 1; j >= 0; j--) {
				emit_expr(arg_nodes[j], cg);
				cg->emit_push_acc();
			}

			if (node->left->kind != ND_VAR) {
				ICE("Struct return assignment currently requires a local struct destination");
			}

			cg->emit_addr_local(node->left->offset);
			cg->emit_push_acc();

			cg->emit_prepare_call_args(count + 1, -1);
			cg->emit_call(node->right->name);
			cg->emit_cleanup_call_args(count + 1, -1);
			return;
		}

		if (node->left->kind == ND_GLOBAL_INDEX) {
			emit_expr(node->left->left, cg);
			cg->emit_push_acc();
			emit_expr(node->right, cg);
			cg->emit_pop_to_tmp();
			cg->emit_store_global_indexed(node->left->name, node->left->elem_size);
		} else if (node->left->kind == ND_INDEX) {
			emit_expr(node->left->left, cg);
			cg->emit_push_acc();
			emit_expr(node->right, cg);
			cg->emit_pop_to_tmp();
			cg->emit_store_indexed(node->left->offset, node->left->elem_size);
		} else if (node->left->kind == ND_DEREF) {
			emit_expr(node->left->left, cg);
			cg->emit_push_acc();
			emit_expr(node->right, cg);
			cg->emit_pop_to_tmp();
			cg->emit_store_deref(node->left->elem_size);
		} else if (node->left->kind == ND_MEMBER_PTR) {
			emit_expr(node->left->left, cg);
			cg->emit_push_acc();
			emit_expr(node->right, cg);
			cg->emit_pop_to_tmp();
			cg->emit_store_member_ptr(node->left->offset, node->left->elem_size ? node->left->elem_size : 4);
		} else {
			emit_expr(node->right, cg);
			if (node->left->kind == ND_GLOBAL)
				cg->emit_store_global(node->left->name, node->left->is_pointer ? 8 : (node->left->elem_size ? node->left->elem_size : 4));
			else if (node->left->is_pointer)
				cg->emit_store_ptr_local(node->left->offset);
			else
				cg->emit_store_local_sized(node->left->offset, node->left->elem_size ? node->left->elem_size : 4);
		}
		return;

	case ND_CALL: {
		if (node->returns_struct) {
			ICE("Struct-returning call must be assigned to a local struct");
		}

		int count = count_list(node->args);

		if (count > 32) {
			ICE("Only up to 32 function arguments are supported by the current call ABI");
		}

		if (node->left) {
			emit_expr(node->left, cg);
			cg->emit_acc_to_saved();
		}

		Node *arg_nodes[32] = {0};
		int i = 0;

		for (Node *arg = node->args; arg; arg = arg->next)
			arg_nodes[i++] = arg;

		for (int j = count - 1; j >= 0; j--) {
			emit_call_arg(arg_nodes[j], cg);
			cg->emit_push_acc();
		}

		cg->emit_prepare_call_args(count, -1);
		if (node->left)
			cg->emit_call_saved();
		else
			cg->emit_call(node->name);
		cg->emit_cleanup_call_args(count, -1);
		return;
	}

	case ND_BLOCK:
		emit_statement(node, cg);
		return;

	case ND_DECL:
	case ND_ARRAY_DECL:
	case ND_PTR_DECL:
	case ND_STRUCT_DECL:
	case ND_RETURN:
	case ND_IF:
	case ND_WHILE:
	case ND_FOR:
	case ND_DO_WHILE:
	case ND_SWITCH:
	case ND_CASE:
	case ND_DEFAULT:
	case ND_BREAK:
	case ND_CONTINUE:
	case ND_FUNC:
		break;
	}

	Debug(1, "  node=%p name=%s body=%s left=%s right=%s\n",
	        (void *)node,
	        node->name[0] ? &node->name[0] : "<null>",
	        node->body ? node_kind_name(node->body->kind) : "<null>",
	        node->left ? node_kind_name(node->left->kind) : "<null>",
	        node->right ? node_kind_name(node->right->kind) : "<null>");

	Node *b = node->body;

	Debug(1, "node=%p kind=%s body=%s bodyptr=%p next=%p\n",
	        (void *)node,
	        node_kind_name(node->kind),
	        b ? node_kind_name(b->kind) : "<null>",
	        (void *)b,
	        b ? (void *)b->next : NULL);

	if (b) {
		Debug(1, "body-name=%s left=%s right=%s\n",
		        b->name[0] ? &b->name[0] : "<empty>",
		        b->left ? node_kind_name(b->left->kind) : "<null>",
		        b->right ? node_kind_name(b->right->kind) : "<null>");
	}

	node_error_at(node, "Invalid expression node %s (%d)",
	              node_kind_name(node->kind), node->kind);
}

static void 
emit_block(Node *block, Codegen *cg)
{
	for (Node *stmt = block->body; stmt; stmt = stmt->next)
		emit_statement(stmt, cg);
}

static void 
emit_statement(Node *node, Codegen *cg)
{
	switch (node->kind) {
	case ND_LABEL:
		cg->emit_label_named(node->name);
		return;

	case ND_GOTO:
		cg->emit_branch_named(node->name);
		return;

	case ND_DECL:
	case ND_ARRAY_DECL:
	case ND_PTR_DECL:
	case ND_STRUCT_DECL:
		return;

	case ND_CALL: {
		/*
		 * Function call used as a statement: evaluate side effects only.
		 * The return value is intentionally discarded.
		 */
		if (node->returns_struct) {
			ICE("Struct-returning call statement must be handled by struct discard lowering");
		}

		int count = count_list(node->args);

		if (count > 32) {
			ICE("Only up to 32 function arguments are supported by the current call ABI");
		}

		if (node->left) {
			emit_expr(node->left, cg);
			cg->emit_acc_to_saved();
		}

		Node *arg_nodes[32] = {0};
		int i = 0;

		for (Node *arg = node->args; arg; arg = arg->next)
			arg_nodes[i++] = arg;

		for (int j = count - 1; j >= 0; j--) {
			emit_call_arg(arg_nodes[j], cg);
			cg->emit_push_acc();
		}

		cg->emit_prepare_call_args(count, -1);
		if (node->left)
			cg->emit_call_saved();
		else
			cg->emit_call(node->name);
		cg->emit_cleanup_call_args(count, -1);
		return;
	}

	case ND_ASSIGN:
	case ND_STRUCT_ASSIGN:
	case ND_GLOBAL:
	case ND_GLOBAL_INDEX:
	case ND_INDEX:
	case ND_MEMBER:
	case ND_MEMBER_PTR:
	case ND_ADDR:
	case ND_DEREF:
	case ND_STRING:
	case ND_FUNC_ADDR:
	case ND_ADD:
	case ND_SUB:
	case ND_MUL:
	case ND_DIV:
	case ND_MOD:
	case ND_BITAND:
	case ND_BITNOT:
	case ND_BITOR:
	case ND_BITXOR:
	case ND_SHL:
	case ND_SHR:
	case ND_EQ:
	case ND_NE:
	case ND_LT:
	case ND_LE:
	case ND_GT:
	case ND_GE:
	case ND_LOGICAL_AND:
	case ND_LOGICAL_OR:
	case ND_COND:
	case ND_COMMA:
	case ND_NEG:
	case ND_NOT:
	case ND_CAST:
	case ND_PRE_INC:
	case ND_PRE_DEC:
	case ND_POST_INC:
	case ND_POST_DEC:
		emit_expr(node, cg);
		return;

	case ND_ASM:
		cg->emit_inline_asm(node->string_value);
		return;

	case ND_CASE:
	case ND_DEFAULT:
		emit_block(node, cg);
		return;

	case ND_RETURN:
		if (node->left && node->left->type && node->left->type->kind == TY_STRUCT) {
			/* Struct-returning functions receive a hidden destination pointer
			 * in their first parameter slot, which the prologue stores at
			 * local offset -8.  Older code only supported `return local;`
			 * and ICEd for `return global;` as in:
			 *
			 *     struct s1 fr_s1(void) { return s1; }
			 *
			 * Copy from the address of any struct lvalue/expression into
			 * that hidden destination.
			 */
			cg->emit_load_ptr_local(-8);
			cg->emit_acc_to_saved();
			emit_struct_lvalue_addr(node->left, cg);
			cg->emit_ptr_copy(node->left->type->size);
			cg->emit_branch(return_label);
			return;
		}

		if (node->left)
			emit_expr(node->left, cg);
		cg->emit_branch(return_label);
		return;

	case ND_IF: {
		int else_label = new_label();
		int end_label = new_label();

		emit_expr(node->cond, cg);
		cg->emit_branch_if_zero(else_label);

		emit_statement(node->then_body, cg);
		cg->emit_branch(end_label);

		cg->emit_label(else_label);
		if (node->else_body)
			emit_statement(node->else_body, cg);

		cg->emit_label(end_label);
		return;
	}

	case ND_SWITCH: {
		int end_label = new_label();
		int default_label = end_label;
		int first_case_label = 0;
		int duff_cond_label = 0;

		for (Node *c = node->body; c; c = c->next) {
			c->offset = new_label();
			if (!first_case_label && c->kind == ND_CASE)
				first_case_label = c->offset;
			if (c->kind == ND_DEFAULT)
				default_label = c->offset;
		}

		for (Node *c = node->body; c; c = c->next) {
			if (c->kind != ND_CASE)
				continue;

			emit_expr(node->cond, cg);
			cg->emit_acc_to_tmp();
			cg->emit_load_imm(c->value);
			cg->emit_cmp_eq();
			cg->emit_branch_if_nonzero(c->offset);
		}

		cg->emit_branch(default_label);

		if (node->inc) {
			duff_cond_label = new_label();
			push_loop(end_label, duff_cond_label);
		} else {
			push_loop(end_label, end_label);
		}

		for (Node *c = node->body; c; c = c->next) {
			cg->emit_label(c->offset);
			emit_block(c, cg);
		}

		if (node->inc) {
			cg->emit_label(duff_cond_label);
			emit_expr(node->inc, cg);
			cg->emit_branch_if_nonzero(first_case_label);
		}

		pop_loop();
		cg->emit_label(end_label);
		return;
	}

	case ND_BREAK:
		cg->emit_branch(current_break_label());
		return;

	case ND_CONTINUE:
		cg->emit_branch(current_continue_label());
		return;

	case ND_WHILE: {
		int start_label = new_label();
		int end_label = new_label();

		push_loop(end_label, start_label);

		cg->emit_label(start_label);
		emit_expr(node->cond, cg);
		cg->emit_branch_if_zero(end_label);

		emit_statement(node->body, cg);
		cg->emit_branch(start_label);

		cg->emit_label(end_label);
		pop_loop();
		return;
	}

	case ND_DO_WHILE: {
		int start_label = new_label();
		int continue_label = new_label();
		int end_label = new_label();

		push_loop(end_label, continue_label);

		cg->emit_label(start_label);
		emit_statement(node->body, cg);

		cg->emit_label(continue_label);
		emit_expr(node->cond, cg);
		cg->emit_branch_if_nonzero(start_label);

		cg->emit_label(end_label);
		pop_loop();
		return;
	}

	case ND_FOR: {
		int start_label = new_label();
		int continue_label = new_label();
		int end_label = new_label();

		if (node->init)
			emit_expr(node->init, cg);

		push_loop(end_label, continue_label);

		cg->emit_label(start_label);

		if (node->cond) {
			emit_expr(node->cond, cg);
			cg->emit_branch_if_zero(end_label);
		}

		emit_statement(node->body, cg);

		cg->emit_label(continue_label);
		if (node->inc)
			emit_expr(node->inc, cg);

		cg->emit_branch(start_label);
		cg->emit_label(end_label);

		pop_loop();
		return;
	}

	case ND_BLOCK:
		emit_block(node, cg);
		return;

	case ND_NUM:
	case ND_VAR:
	case ND_FUNC:
		break;
	}

	node_error_at(node, "Invalid statement node %s (%d)",
	              node_kind_name(node->kind), node->kind);
}

static void 
emit_function(Node *func, Codegen *cg)
{
	return_label = new_label();

	cg->emit_function_start(func->name, func->is_static);
	cg->emit_stack_alloc(func->stack_size);

	for (int i = 0; i < func->param_count; i++) {
		int offset = -(i + 1) * 8;
		cg->emit_store_param(i, offset);
	}

	emit_block(func->body, cg);

	/*
	 * If control reaches the end of main without an explicit return, C
	 * requires it to behave as if `return 0;` had been executed.  Keep this
	 * in the emitter/function-finalization path rather than manufacturing an
	 * AST or IR return: explicit returns branch directly to return_label and
	 * therefore preserve their computed return value, while natural fallthrough
	 * from main gets the mandated zero return value before the common epilogue.
	 *
	 * For other non-void functions, falling off the end is undefined by C; we
	 * deliberately leave the accumulator unchanged but still emit the common
	 * epilogue below so the generated function always returns structurally.
	 */
	if (STRCMP(func->name, "main") == 0)
		cg->emit_load_imm(0);

	cg->emit_label(return_label);
	cg->emit_function_end();
}

static void 
emit_data_symbol(const char *name, int is_static)
{
	if (!is_static)
		printf(".global _%s\n", name);
	printf("_%s:\n", name);
}

void 
emit_program(Node *program, Codegen *cg)
{
	next_label_id = 1;
	loop_depth = 0;

	cg->emit_preamble();

	int has_data_globals = 0;
	for (int i = 0; i < global_count; i++) {
		if (!globals[i].is_extern) {
			has_data_globals = 1;
			break;
		}
	}

	if (has_data_globals) {
		printf(".data\n");
		for (int i = 0; i < global_count; i++) {
			if (globals[i].is_extern)
				continue;

			if (globals[i].is_array) {
				if (globals[i].is_struct) {
					printf("    .align 4\n");
					emit_data_symbol(globals[i].name, globals[i].is_static);
					if (globals[i].init_count > 0) {
						int total = globals[i].array_len * globals[i].elem_size;
						for (int j = 0; j < total; j++)
							printf("    .byte %d\n", globals[i].init_values[j] & 255);
					} else {
						printf("    .zero %d\n", globals[i].array_len * globals[i].elem_size);
					}
				} else if (globals[i].elem_size == 1) {
					emit_data_symbol(globals[i].name, globals[i].is_static);
					if (globals[i].is_string_array) {
						emit_raw_string_literal(globals[i].string_value);
					} else {
						printf("    .zero %d\n", globals[i].array_len);
					}
				} else if (globals[i].init_count > 0) {
					printf("    .align 4\n");
					emit_data_symbol(globals[i].name, globals[i].is_static);
					for (int j = 0; j < globals[i].array_len; j++) {
						int value = j < globals[i].init_count ? globals[i].init_values[j] : 0;
						printf("    .word %d\n", value);
					}
				} else {
					printf("    .align 4\n");
					emit_data_symbol(globals[i].name, globals[i].is_static);
					printf("    .zero %d\n", globals[i].array_len * globals[i].elem_size);
				}
			} else if (globals[i].is_string) {
				printf("Lstr%d:\n", globals[i].string_label);
				emit_raw_string_literal(globals[i].string_value);
				printf("    .align 8\n");
				emit_data_symbol(globals[i].name, globals[i].is_static);
				printf("    .quad Lstr%d\n", globals[i].string_label);
			} else if (globals[i].is_struct) {
				printf("    .align 4\n");
				emit_data_symbol(globals[i].name, globals[i].is_static);
				if (globals[i].init_count > 0) {
					Debug(1, "DBG_emit: name=%s elem_size=%d init_count=%d iv0=%d iv4=%d\n", globals[i].name, globals[i].elem_size, globals[i].init_count, globals[i].init_values[0], globals[i].init_values[4]);
					for (int j = 0; j < globals[i].elem_size; j++)
						printf("    .byte %d\n", globals[i].init_values[j] & 255);
				} else {
					printf("    .zero %d\n", globals[i].elem_size);
				}
			} else if (globals[i].is_addr) {
				printf("    .align 8\n");
				emit_data_symbol(globals[i].name, globals[i].is_static);
				printf("    .quad _%s\n", globals[i].addr_name);
			} else if (globals[i].elem_size == 1) {
				emit_data_symbol(globals[i].name, globals[i].is_static);
				printf("    .byte %d\n", globals[i].init_value & 255);
			} else if (globals[i].elem_size == 2) {
				emit_data_symbol(globals[i].name, globals[i].is_static);
				printf("    .short %d\n", globals[i].init_value & 65535);
			} else {
				emit_data_symbol(globals[i].name, globals[i].is_static);
				printf("    .quad %d\n", globals[i].init_value);
			}
		}
		printf(".text\n");
	}

	emit_string_literals(program, cg);

	for (Node *func = program; func; func = func->next) {
		if (func->kind != ND_FUNC) {
			ICE("Expected function node");
		}

		emit_function(func, cg);
	}
}
