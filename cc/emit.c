#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tcc.h"
#include "target.h"
#include "data_emit.h"
#include "emit.h"

extern double tcc_monotonic_seconds(void);
#include "parser_internal.h"

extern double strtod(const char *nptr, char **endptr);

static void emit_statement(Node *node, Codegen *cg);
static void emit_expr(Node *node, Codegen *cg);
static void emit_lvalue_addr(Node *node, Codegen *cg);
static void emit_struct_lvalue_addr(Node *node, Codegen *cg);
static void emit_block(Node *block, Codegen *cg);
static int count_list(Node *node);
static int emit_target_is_x86(Codegen *cg);
static int emit_target_is_arm64(Codegen *cg);
static int emit_node_contains_call(Node *node);
static int emit_arm64_can_omit_frame(Node *func, Codegen *cg);
static int emit_x86_param_stack_words(const Type *type);
static int emit_push_call_arg(Node *arg, Codegen *cg);
static int emit_push_call_arg_typed(Node *arg, const Type *arg_type, const char *abi_name, Codegen *cg);
static int emit_push_call_args_reverse(Node *arg, Codegen *cg);
static int emit_expr_is_floating(Node *node);
static int emit_type_is_floating_scalar(const Type *type);
static void emit_apply_integer_load_cast(Node *node, Codegen *cg);
static unsigned long long emit_fp_literal_bits(const Node *node);
static void emit_arm64_fp_push(int size);
static void emit_arm64_fp_pop_to_tmp(int size);
static void emit_arm64_fp_load_addr(const char *addr_reg, int size);
static void emit_arm64_fp_store_addr(const char *addr_reg, int size);
static void emit_arm64_fp_store_incoming_param(Codegen *cg, int fp_reg, int offset, int size);
static int emit_arm64_hfa_info(const Type *type, int *elem_size_out, int *elem_count_out);
static void emit_arm64_load_hfa_return_value(Node *value, Codegen *cg);
static void emit_arm64_store_hfa_return_lvalue(Node *dst, Type *type, Codegen *cg);
static void emit_arm64_load_intreg_chunk(const char *base_reg, int offset, int chunk, int dst_reg);
static void emit_arm64_fp_lvalue_addr(Node *node, Codegen *cg);
static void emit_arm64_fp_load_local(Node *node, Codegen *cg);
static int emit_arm64_direct_small_gpr_call(Node *node, Codegen *cg);
static int emit_arm64_fixed_fp_call(Node *node, Codegen *cg);
static int emit_arm64_direct_intreg_aggregate_call(Node *node, Codegen *cg);
static int emit_arm64_direct_sret_small_gpr_call(Node *dst, Node *call, Codegen *cg);
static int emit_arm64_direct_sret_small_gpr_call_to_saved(Node *call, Codegen *cg);
static int emit_arm64_fixed_fp_sret_call(Node *dst, Node *call, Codegen *cg);
static int emit_arm64_fixed_fp_sret_call_to_saved(Node *call, Codegen *cg);
static Node *emit_arm64_prepare_direct_aggregate_arg(Node *arg, Codegen *cg);
static int emit_collect_hidden_struct_param_offsets(Node *node, int *offsets, int count, int max_count);
static const char *emit_function_param_struct_name(Node *func, int param_index, int offset);
static void emit_global_index_addr(Node *node, Codegen *cg);
static void emit_binary_operands(Node *node, Codegen *cg);
static int emit_arm64_try_emit_immediate_binary(Node *node, Codegen *cg);
static int emit_arm64_try_emit_immediate_compare(Node *node, Codegen *cg);
static int emit_arm64_try_branch_if_zero(Node *node, int label, Codegen *cg);
static int emit_arm64_try_branch_if_nonzero(Node *node, int label, Codegen *cg);
static int emit_arm64_try_branch_on_signed_zero_compare(Node *expr, const char *cond,
                                                        int label, Codegen *cg);
static int emit_arm64_try_emit_ptr_add_deref_load(Node *node, Codegen *cg);
static int emit_arm64_try_emit_local_update_assign(Node *node, Codegen *cg);
static int emit_arm64_try_emit_indirect_update_assign(Node *node, Codegen *cg);
static int emit_arm64_try_emit_indirect_assign(Node *node, Codegen *cg);
static int emit_arm64_try_emit_local_incdec(Node *node, Codegen *cg, int is_inc, int is_postfix);
static int emit_arm64_try_emit_dense_switch(Node *node, int default_label, int end_label, Codegen *cg);
static int emit_arm64_can_use_leaf_sp_frame(Node *func, Codegen *cg);
static void emit_trace_call_abi(const char *phase, Node *node);

static int next_label_id = 1;
static int return_label = 0;
static Node *current_emit_function = NULL;

#define new_label() (next_label_id++)

#define USER_LABEL_MAX 1024
static const char *user_label_names[USER_LABEL_MAX];
static int         user_label_ids[USER_LABEL_MAX];
static int         user_label_count = 0;

static int
get_user_label_id(const char *name)
{
	int i;
	int id;
	if (!name || !name[0])
		return new_label();

	for (i = 0; i < user_label_count; i++) {
		if (STRCMP(user_label_names[i], name) == 0)
			return user_label_ids[i];
	}

	if (user_label_count >= USER_LABEL_MAX)
		ICE("Too many user labels in one function");

	id = new_label();
	user_label_names[user_label_count] = name;
	user_label_ids[user_label_count] = id;
	user_label_count++;
	return id;
}

#define LOOP_STACK_MAX 128
static int break_labels[LOOP_STACK_MAX];
static int continue_labels[LOOP_STACK_MAX];
static int loop_depth = 0;

static void 
push_loop(int break_label, int continue_label)
{
	int depth = loop_depth;

	if (depth >= LOOP_STACK_MAX) {
		ICE("Too many nested loops");
	}

	break_labels[depth] = break_label;
	continue_labels[depth] = continue_label;
	loop_depth = depth + 1;
}

static void 
pop_loop(void)
{
	int *depth = &loop_depth;
	if (*depth > 0)
		(*depth)--;
}

static int 
current_break_label(void)
{
	int depth = loop_depth - 1;

	if (depth < 0) {
		ICE("break used outside loop");
	}

	return break_labels[depth];
}

static int 
current_continue_label(void)
{
	int depth = loop_depth - 1;

	if (depth < 0) {
		ICE("continue used outside loop");
	}

	return continue_labels[depth];
}

static int 
emit_compare_is_unsigned(Node *node)
{
	return node && (node->value || node->is_unsigned ||
	                (node->type && node->type->is_unsigned));
}

static int
emit_target_is_x86(Codegen *cg)
{
	return cg == &x86_codegen;
}

static int
emit_target_is_arm64(Codegen *cg)
{
	return cg == &arm64_codegen;
}

static void
emit_trace_call_abi(const char *phase, Node *node)
{
	FuncInfo *fi;
	const char *trace;

	trace = getenv("TCC_TRACE_CALL_ABI");
	if (!trace || !trace[0] || !node || node->kind != ND_CALL || !node->name[0])
		return;

	fi = find_func(node->name);
	fprintf(stderr,
	        "tcc emit: %s call=%s node_ret=%d node_abi=%d node_regs=%d node_size=%d"
	        " node_type_kind=%d fi=%p fi_ret=%d fi_abi=%d fi_regs=%d fi_size=%d fi_type=%p fi_param_count=%d\n",
	        phase ? phase : "?",
	        node->name,
	        node->returns_struct,
	        node->aggregate_abi_class,
	        node->aggregate_abi_reg_count,
	        node->struct_return_size,
	        node->type ? node->type->kind : -1,
	        (void *)fi,
	        fi ? fi->returns_struct : -1,
	        fi ? fi->return_abi_class : -1,
	        fi ? fi->return_abi_reg_count : -1,
	        fi ? fi->struct_size : -1,
	        fi ? (void *)fi->return_type : NULL,
	        fi ? fi->param_type_count : -1);
}

static int
emit_node_contains_call(Node *node)
{
	for (; node; node = node->next) {
		if (node->kind == ND_CALL)
			return 1;
		if (emit_node_contains_call(node->left) ||
		    emit_node_contains_call(node->right) ||
		    emit_node_contains_call(node->init) ||
		    emit_node_contains_call(node->cond) ||
		    emit_node_contains_call(node->inc) ||
		    emit_node_contains_call(node->then_body) ||
		    emit_node_contains_call(node->else_body) ||
		    emit_node_contains_call(node->body) ||
		    emit_node_contains_call(node->args))
			return 1;
	}
	return 0;
}

static int
emit_arm64_is_trivial_leaf_expr(Node *node)
{
	if (!node)
		return 1;

	switch (node->kind) {
	case ND_NUM:
	case ND_STRING:
	case ND_GLOBAL:
	case ND_MEMBER:
	case ND_FUNC_ADDR:
		return 1;
	case ND_CAST:
		return emit_arm64_is_trivial_leaf_expr(node->left);
	case ND_MEMBER_PTR:
	case ND_INDEX:
	case ND_GLOBAL_INDEX:
		return emit_arm64_is_trivial_leaf_expr(node->left);
	case ND_ADDR:
		if (!node->left)
			return 0;
		switch (node->left->kind) {
		case ND_GLOBAL:
		case ND_MEMBER:
			return 1;
		case ND_MEMBER_PTR:
		case ND_INDEX:
		case ND_GLOBAL_INDEX:
		case ND_DEREF:
		case ND_CAST:
			return emit_arm64_is_trivial_leaf_expr(node->left);
		default:
			return 0;
		}
	default:
		return 0;
	}
}

static int
emit_arm64_can_omit_frame(Node *func, Codegen *cg)
{
	Node *body;

	if (!emit_target_is_arm64(cg) || !func)
		return 0;
	if (func->returns_struct)
		return 0;
	if (func->stack_size != 0)
		return 0;
	if (func->param_count != 0)
		return 0;
	if (emit_node_contains_call(func->body))
		return 0;

	body = func->body;
	if (!body)
		return 1;
	if (body->next)
		return 0;
	if (body->kind != ND_RETURN)
		return 0;
	return emit_arm64_is_trivial_leaf_expr(body->left);
}

static int
emit_arm64_can_use_leaf_sp_frame(Node *func, Codegen *cg)
{
	(void)func;
	(void)cg;
	/*
	 * Disabled until the backend keeps a stable local-frame base that is
	 * independent from expression temp pushes/pops on sp.
	 */
	return 0;
}

static int
emit_type_is_floating_scalar(const Type *type)
{
	return type && type_is_fp_scalar(type);
}

static int
emit_expr_is_floating(Node *node)
{
	return node && node->type && type_is_fp_scalar(node->type);
}

static int
emit_arm64_direct_gpr_arg_ok_typed(Node *arg, const Type *arg_type, const char *abi_name)
{
	Type *type;
	int hfa_elem_size;
	int hfa_elem_count;

	for (;;) {
		if (!arg)
			return 0;
		if (arg->kind == ND_CAST && arg->left) {
			arg = arg->left;
			continue;
		}
		if (arg->kind == ND_COMMA && arg->right) {
			arg = arg->right;
			continue;
		}
		break;
	}

	type = arg_type ? (Type *)arg_type : arg->type;
	if (type) {
		if (emit_type_is_floating_scalar(type) ||
		    type_is_struct(type) || type_is_union(type) ||
		    emit_arm64_hfa_info(type, &hfa_elem_size, &hfa_elem_count))
			return 0;
		if (parser_arm64_hfa_info_name(abi_name, &hfa_elem_size, &hfa_elem_count))
			return 0;
		return 1;
	}

	if (parser_arm64_hfa_info_name(abi_name, &hfa_elem_size, &hfa_elem_count))
		return 0;
	if (arg->returns_struct || arg->is_fp_num)
		return 0;
	return 1;
}

static Node *
emit_arm64_prepare_direct_aggregate_arg(Node *arg, Codegen *cg)
{
	for (;;) {
		if (!arg)
			return NULL;
		if (arg->kind == ND_CAST && arg->left) {
			arg = arg->left;
			continue;
		}
		if (arg->kind == ND_COMMA && arg->right) {
			emit_statement(arg->left, cg);
			arg = arg->right;
			continue;
		}
		return arg;
	}
}

static int
emit_is_integer_literal(Node *node)
{
	return node && node->kind == ND_NUM && !node->is_fp_num;
}

static int
emit_arm64_is_lowbit_mask_imm(unsigned long imm)
{
	if (imm == 0)
		return 1;
	if ((imm & (imm + 1ul)) == 0)
		return 1;
	return (imm & (imm - 1ul)) == 0;
}

static int
emit_arm64_supports_small_int_fastpath(Node *node)
{
	if (!node || node->is_pointer || !node->type)
		return 0;
	if (type_is_fp_scalar(node->type))
		return 0;
	return node->type->size > 0 && node->type->size <= 4;
}

static int
emit_arm64_supports_immediate_compare_fastpath(Node *node)
{
	if (!node)
		return 0;
	if (node->is_pointer)
		return 1;
	if (!node->type || type_is_fp_scalar(node->type))
		return 0;
	if (type_is_struct(node->type) || type_is_union(node->type) ||
	    type_is_array(node->type) || type_is_function(node->type))
		return 0;
	return node->type->size > 0 && node->type->size <= 8;
}

static int
emit_arm64_try_branch_on_signed_zero_compare(Node *expr, const char *cond, int label, Codegen *cg)
{
	int sign_bit;
	char reg;
	const char *op;

	if (!expr || !cond || !cg || !emit_target_is_arm64(cg) || label <= 0)
		return 0;
	if (expr->is_pointer || !expr->type || expr->type->is_unsigned)
		return 0;
	if (type_is_fp_scalar(expr->type) || type_is_struct(expr->type) ||
	    type_is_union(expr->type) || type_is_array(expr->type) ||
	    type_is_function(expr->type))
		return 0;
	if (expr->type->size != 4 && expr->type->size != 8)
		return 0;

	if (STRCMP(cond, "lt") == 0)
		op = "tbnz";
	else if (STRCMP(cond, "ge") == 0)
		op = "tbz";
	else
		return 0;

	emit_expr(expr, cg);
	reg = expr->type->size >= 8 ? 'x' : 'w';
	sign_bit = expr->type->size * 8 - 1;
	printf("    %s %c0, #%d, L%d\n", op, reg, sign_bit, label);
	return 1;
}

static int
emit_arm64_is_simple_local_scalar(Node *node)
{
	if (!node)
		return 0;
	if (!(node->kind == ND_VAR || node->kind == ND_MEMBER))
		return 0;
	if (node->is_pointer || !node->type || type_is_fp_scalar(node->type))
		return 0;
	if (node->is_bitfield || (node->type && node->type->kind == TY_ARRAY))
		return 0;
	return node->elem_size > 0 && node->elem_size <= 4;
}

static int
emit_arm64_same_simple_local(Node *a, Node *b)
{
	if (!emit_arm64_is_simple_local_scalar(a) || !emit_arm64_is_simple_local_scalar(b))
		return 0;
	return a->kind == b->kind && a->offset == b->offset;
}

static int
emit_arm64_local_update_size(Node *node)
{
	return (node && node->elem_size) ? node->elem_size : TCC_SIZEOF_INT;
}

static int
emit_arm64_indirect_store_size(Node *node)
{
	if (!node)
		return TCC_SIZEOF_INT;
	if (node->is_pointer)
		return TCC_SIZEOF_PTR;
	return node->elem_size ? node->elem_size : TCC_SIZEOF_INT;
}

static int
emit_arm64_expr_preserves_saved_reg(Node *node)
{
	if (!node)
		return 0;
	if (node->returns_struct)
		return 0;
	if (node->type &&
	    (type_is_struct(node->type) || type_is_union(node->type) ||
	     type_is_array(node->type) || type_is_function(node->type) ||
	     type_is_fp_scalar(node->type)))
		return 0;

	switch (node->kind) {
	case ND_NUM:
	case ND_VAR:
	case ND_GLOBAL:
	case ND_MEMBER:
	case ND_STRING:
	case ND_FUNC_ADDR:
		return 1;

	case ND_MEMBER_PTR:
	case ND_ADDR:
	case ND_DEREF:
	case ND_NEG:
	case ND_BITNOT:
	case ND_NOT:
	case ND_CAST:
		return emit_arm64_expr_preserves_saved_reg(node->left);

	default:
		return 0;
	}
}

static int
emit_arm64_same_expr(Node *a, Node *b)
{
	if (a == b)
		return 1;
	if (!a || !b || a->kind != b->kind)
		return 0;

	switch (a->kind) {
	case ND_NUM:
		return a->value == b->value &&
		       a->long_value == b->long_value &&
		       a->is_fp_num == b->is_fp_num;

	case ND_VAR:
	case ND_MEMBER:
		return a->offset == b->offset &&
		       a->elem_size == b->elem_size &&
		       a->is_pointer == b->is_pointer;

	case ND_GLOBAL:
		return STRCMP(a->name, b->name) == 0 &&
		       a->elem_size == b->elem_size &&
		       a->is_pointer == b->is_pointer;

	case ND_GLOBAL_INDEX:
		return STRCMP(a->name, b->name) == 0 &&
		       a->offset == b->offset &&
		       a->elem_size == b->elem_size &&
		       a->is_pointer == b->is_pointer &&
		       emit_arm64_same_expr(a->left, b->left);

	case ND_INDEX:
		return a->offset == b->offset &&
		       a->elem_size == b->elem_size &&
		       a->is_pointer == b->is_pointer &&
		       emit_arm64_same_expr(a->left, b->left);

	case ND_DEREF:
	case ND_ADDR:
	case ND_CAST:
	case ND_NEG:
	case ND_BITNOT:
	case ND_NOT:
		return emit_arm64_same_expr(a->left, b->left);

	case ND_MEMBER_PTR:
		return a->offset == b->offset &&
		       a->elem_size == b->elem_size &&
		       a->is_pointer == b->is_pointer &&
		       emit_arm64_same_expr(a->left, b->left);

	default:
		return 0;
	}
}

static int
emit_arm64_try_get_small_signed_imm(Node *node, long *imm_out)
{
	long value;

	if (!imm_out || !emit_is_integer_literal(node))
		return 0;
	if ((long)node->value != node->long_value)
		return 0;
	value = node->long_value;
	if (value < -4095 || value > 4095)
		return 0;
	*imm_out = value;
	return 1;
}

static int
emit_arm64_scale_shift_local(int scale)
{
	int shift = 0;

	if (scale <= 0 || (scale & (scale - 1)) != 0)
		return -1;
	while ((scale >>= 1) != 0)
		shift++;
	return shift;
}

static void
emit_arm64_print_sxtw_suffix_local(int shift)
{
	if (shift > 0)
		printf(", sxtw #%d", shift);
	else
		printf(", sxtw");
}

static int
emit_arm64_expr_is_noncall_scalar(Node *node)
{
	if (!node || node->returns_struct)
		return 0;
	if (node->type &&
	    (type_is_struct(node->type) || type_is_union(node->type) ||
	     type_is_array(node->type) || type_is_function(node->type) ||
	     type_is_fp_scalar(node->type)))
		return 0;

	switch (node->kind) {
	case ND_NUM:
	case ND_VAR:
	case ND_GLOBAL:
	case ND_MEMBER:
	case ND_STRING:
	case ND_FUNC_ADDR:
		return 1;

	case ND_GLOBAL_INDEX:
	case ND_INDEX:
	case ND_MEMBER_PTR:
	case ND_ADDR:
	case ND_DEREF:
	case ND_NEG:
	case ND_BITNOT:
	case ND_NOT:
	case ND_CAST:
		return emit_arm64_expr_is_noncall_scalar(node->left);

	case ND_ADD:
	case ND_SUB:
	case ND_MUL:
	case ND_DIV:
	case ND_MOD:
	case ND_BITAND:
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
		return emit_arm64_expr_is_noncall_scalar(node->left) &&
		       emit_arm64_expr_is_noncall_scalar(node->right);

	default:
		return 0;
	}
}

static int
emit_arm64_try_decompose_ptr_add(Node *node, Node **base_out, Node **index_out, int *scale_out)
{
	int left_is_ptr;
	int right_is_ptr;

	if (!node || node->kind != ND_ADD || !node->is_pointer ||
	    !base_out || !index_out || !scale_out)
		return 0;

	left_is_ptr = node->left &&
	              (node->left->is_pointer ||
	               (node->left->type && type_is_pointer(node->left->type)));
	right_is_ptr = node->right &&
	               (node->right->is_pointer ||
	                (node->right->type && type_is_pointer(node->right->type)));

	if (left_is_ptr && !right_is_ptr) {
		*base_out = node->left;
		*index_out = node->right;
	} else if (!left_is_ptr && right_is_ptr) {
		*base_out = node->right;
		*index_out = node->left;
	} else {
		return 0;
	}

	*scale_out = node->elem_size ? node->elem_size : TCC_SIZEOF_INT;
	return 1;
}

static int
emit_arm64_try_get_small_unsigned_imm(Node *node, unsigned long *imm_out)
{
	if (!imm_out || !emit_is_integer_literal(node))
		return 0;
	if ((long)node->value != node->long_value)
		return 0;
	if (node->long_value < 0 || node->long_value > 4095)
		return 0;
	*imm_out = (unsigned long)node->long_value;
	return 1;
}

static int
emit_arm64_local_const_index_info(Node *node, int *offset_out, int *size_out)
{
	long index;
	int elem_size;

	if (offset_out)
		*offset_out = 0;
	if (size_out)
		*size_out = 0;
	if (!node || node->kind != ND_INDEX || !node->left || !emit_is_integer_literal(node->left))
		return 0;
	if (node->is_array_field || (node->type && type_is_array(node->type)))
		return 0;
	if (node->type && type_is_fp_scalar(node->type))
		return 0;
	if ((long)node->left->value != node->left->long_value)
		return 0;

	index = node->left->long_value;
	elem_size = node->is_pointer ? 8 : (node->elem_size ? node->elem_size : TCC_SIZEOF_INT);
	if (!(elem_size == 1 || elem_size == 2 || elem_size == 4 || elem_size == 8))
		return 0;

	if (offset_out)
		*offset_out = node->offset + (int)(index * elem_size);
	if (size_out)
		*size_out = elem_size;
	return 1;
}

static int
emit_arm64_try_emit_const_local_index_load(Node *node, Codegen *cg)
{
	int offset;
	int size;

	if (!cg || !emit_target_is_arm64(cg))
		return 0;
	if (!emit_arm64_local_const_index_info(node, &offset, &size))
		return 0;

	cg->emit_load_local_sized(offset, size);
	emit_apply_integer_load_cast(node, cg);
	return 1;
}

static int
emit_arm64_try_emit_const_local_index_store(Node *node, Codegen *cg)
{
	int offset;
	int size;

	if (!cg || !emit_target_is_arm64(cg) || !node || !node->left || !node->right)
		return 0;
	if (!emit_arm64_local_const_index_info(node->left, &offset, &size))
		return 0;

	emit_expr(node->right, cg);
	cg->emit_store_local_sized(offset, size);
	return 1;
}

static int
emit_arm64_try_get_lowbit_mask(Node *node, unsigned long *imm_out)
{
	if (!emit_arm64_try_get_small_unsigned_imm(node, imm_out))
		return 0;
	return emit_arm64_is_lowbit_mask_imm(*imm_out);
}

static const char *
emit_arm64_swap_compare_cond(const char *cond)
{
	if (!cond)
		return cond;
	if (STRCMP(cond, "eq") == 0 || STRCMP(cond, "ne") == 0)
		return cond;
	if (STRCMP(cond, "lt") == 0)
		return "gt";
	if (STRCMP(cond, "le") == 0)
		return "ge";
	if (STRCMP(cond, "gt") == 0)
		return "lt";
	if (STRCMP(cond, "ge") == 0)
		return "le";
	if (STRCMP(cond, "lo") == 0)
		return "hi";
	if (STRCMP(cond, "ls") == 0)
		return "hs";
	if (STRCMP(cond, "hi") == 0)
		return "lo";
	if (STRCMP(cond, "hs") == 0)
		return "ls";
	return cond;
}

static void
emit_arm64_emit_add_sub_imm(long imm)
{
	if (imm > 0)
		printf("    add x0, x0, #%ld\n", imm);
	else if (imm < 0)
		printf("    sub x0, x0, #%ld\n", -imm);
}

static void
emit_arm64_emit_sub_add_imm(long imm)
{
	if (imm > 0)
		printf("    sub x0, x0, #%ld\n", imm);
	else if (imm < 0)
		printf("    add x0, x0, #%ld\n", -imm);
}

static int
emit_arm64_try_emit_immediate_binary(Node *node, Codegen *cg)
{
	Node *expr;
	unsigned long mask;
	long imm;

	if (!node || !cg || !emit_target_is_arm64(cg) || !node->right ||
	    emit_expr_is_floating(node))
		return 0;

	switch (node->kind) {
	case ND_ADD:
		if (!emit_arm64_try_get_small_signed_imm(node->right, &imm))
			return 0;
		if (!emit_arm64_supports_small_int_fastpath(node))
			return 0;
		expr = node->left;
		emit_expr(expr, cg);
		emit_arm64_emit_add_sub_imm(imm);
		return 1;

	case ND_SUB:
		if (!emit_arm64_try_get_small_signed_imm(node->right, &imm))
			return 0;
		if (!emit_arm64_supports_small_int_fastpath(node))
			return 0;
		expr = node->left;
		emit_expr(expr, cg);
		emit_arm64_emit_sub_add_imm(imm);
		return 1;

	case ND_BITAND:
		if (!emit_arm64_try_get_lowbit_mask(node->right, &mask) ||
		    !emit_arm64_supports_small_int_fastpath(node))
			return 0;
		emit_expr(node->left, cg);
		if (mask == 0)
			printf("    movz x0, #0\n");
		else
			printf("    and x0, x0, #%lu\n", mask);
		return 1;

	default:
		return 0;
	}
}

static int
emit_arm64_try_emit_immediate_compare(Node *node, Codegen *cg)
{
	Node *expr;
	Node *masked_expr;
	unsigned long imm;
	const char *cond;
	char reg;
	int swapped = 0;
	int is_unsigned;

	if (!node || !cg || !emit_target_is_arm64(cg) || !node->left || !node->right)
		return 0;
	if (emit_expr_is_floating(node->left) || emit_expr_is_floating(node->right))
		return 0;

	is_unsigned = emit_compare_is_unsigned(node);
	switch (node->kind) {
	case ND_EQ:
		cond = "eq";
		break;
	case ND_NE:
		cond = "ne";
		break;
	case ND_LT:
		cond = is_unsigned ? "lo" : "lt";
		break;
	case ND_LE:
		cond = is_unsigned ? "ls" : "le";
		break;
	case ND_GT:
		cond = is_unsigned ? "hi" : "gt";
		break;
	case ND_GE:
		cond = is_unsigned ? "hs" : "ge";
		break;
	default:
		return 0;
	}

	expr = NULL;
	if (emit_arm64_try_get_small_unsigned_imm(node->right, &imm))
		expr = node->left;
	else if (emit_arm64_try_get_small_unsigned_imm(node->left, &imm)) {
		expr = node->right;
		swapped = 1;
	} else
		return 0;
	if (!emit_arm64_supports_immediate_compare_fastpath(expr))
		return 0;
	reg = (expr->is_pointer || (expr->type && expr->type->size >= 8)) ? 'x' : 'w';

	if (!swapped && imm == 0 && expr && expr->kind == ND_BITAND &&
	    (node->kind == ND_EQ || node->kind == ND_NE)) {
		masked_expr = NULL;
		if (emit_arm64_try_get_lowbit_mask(expr->right, &imm))
			masked_expr = expr->left;
		else if (emit_arm64_try_get_lowbit_mask(expr->left, &imm))
			masked_expr = expr->right;
		if (masked_expr) {
			emit_expr(masked_expr, cg);
			if (imm == 0) {
				printf("    movz x0, #%d\n", node->kind == ND_EQ ? 1 : 0);
			} else {
				printf("    tst x0, #%lu\n", imm);
				printf("    cset w0, %s\n", cond);
			}
			return 1;
		}
	}

	if (swapped) {
		switch (node->kind) {
		case ND_LT:
			cond = is_unsigned ? "hi" : "gt";
			break;
		case ND_LE:
			cond = is_unsigned ? "hs" : "ge";
			break;
		case ND_GT:
			cond = is_unsigned ? "lo" : "lt";
			break;
		case ND_GE:
			cond = is_unsigned ? "ls" : "le";
			break;
		default:
			break;
		}
	}

	emit_expr(expr, cg);
	printf("    cmp %c0, #%lu\n", reg, imm);
	printf("    cset w0, %s\n", cond);
	return 1;
}

static int
emit_arm64_cmp_branch_use_xregs(Node *node)
{
	if (!node)
		return 0;
	if (node->is_pointer)
		return 1;
	return node->type && node->type->size >= 8;
}

static int
emit_arm64_try_branch_on_compare(Node *node, const char *cond, int label, Codegen *cg)
{
	Node *expr;
	unsigned long imm;
	const char *branch_cond;
	char reg;
	int size;

	if (!node || !cond || !cg || !emit_target_is_arm64(cg) || label <= 0)
		return 0;

	if (node->left && node->right &&
	    emit_expr_is_floating(node->left) && emit_expr_is_floating(node->right)) {
		size = node->left->type ? node->left->type->size : 8;
		emit_expr(node->left, cg);
		emit_arm64_fp_push(size);
		emit_expr(node->right, cg);
		emit_arm64_fp_pop_to_tmp(size);
		printf("    fcmp %c1, %c0\n", size == 4 ? 's' : 'd', size == 4 ? 's' : 'd');
		printf("    b.%s L%d\n", cond, label);
		return 1;
	}

	expr = NULL;
	branch_cond = cond;
	if (node->left && emit_arm64_try_get_small_unsigned_imm(node->right, &imm))
		expr = node->left;
	else if (node->right && emit_arm64_try_get_small_unsigned_imm(node->left, &imm)) {
		expr = node->right;
		branch_cond = emit_arm64_swap_compare_cond(cond);
	}
	if (expr && emit_arm64_supports_immediate_compare_fastpath(expr)) {
		if (imm == 0 &&
		    emit_arm64_try_branch_on_signed_zero_compare(expr, branch_cond, label, cg))
			return 1;
		emit_expr(expr, cg);
		reg = (expr->is_pointer || (expr->type && expr->type->size >= 8)) ? 'x' : 'w';
		printf("    cmp %c0, #%lu\n", reg, imm);
		printf("    b.%s L%d\n", branch_cond, label);
		return 1;
	}

	emit_binary_operands(node, cg);
	reg = emit_arm64_cmp_branch_use_xregs(node) ? 'x' : 'w';
	printf("    cmp %c1, %c0\n", reg, reg);
	printf("    b.%s L%d\n", cond, label);
	return 1;
}

static int
emit_arm64_try_branch_if_zero(Node *node, int label, Codegen *cg)
{
	int skip_label;

	if (!node || !cg || !emit_target_is_arm64(cg) || label <= 0)
		return 0;

	switch (node->kind) {
	case ND_NOT:
		return emit_arm64_try_branch_if_nonzero(node->left, label, cg);

	case ND_LOGICAL_AND:
		if (emit_arm64_try_branch_if_zero(node->left, label, cg)) {
			if (emit_arm64_try_branch_if_zero(node->right, label, cg))
				return 1;
			emit_expr(node->right, cg);
			cg->emit_branch_if_zero(label);
			return 1;
		}
		break;

	case ND_LOGICAL_OR:
		skip_label = new_label();
		if (emit_arm64_try_branch_if_nonzero(node->left, skip_label, cg)) {
			if (emit_arm64_try_branch_if_zero(node->right, label, cg)) {
				cg->emit_label(skip_label);
				return 1;
			}
			emit_expr(node->right, cg);
			cg->emit_branch_if_zero(label);
			cg->emit_label(skip_label);
			return 1;
		}
		break;

	case ND_EQ:
		return emit_arm64_try_branch_on_compare(node, "ne", label, cg);
	case ND_NE:
		return emit_arm64_try_branch_on_compare(node, "eq", label, cg);
	case ND_LT:
		return emit_arm64_try_branch_on_compare(node,
			emit_compare_is_unsigned(node) ? "hs" : "ge", label, cg);
	case ND_LE:
		return emit_arm64_try_branch_on_compare(node,
			emit_compare_is_unsigned(node) ? "hi" : "gt", label, cg);
	case ND_GT:
		return emit_arm64_try_branch_on_compare(node,
			emit_compare_is_unsigned(node) ? "ls" : "le", label, cg);
	case ND_GE:
		return emit_arm64_try_branch_on_compare(node,
			emit_compare_is_unsigned(node) ? "lo" : "lt", label, cg);
	default:
		break;
	}

	return 0;
}

static int
emit_arm64_try_emit_dense_switch(Node *node, int default_label, int end_label, Codegen *cg)
{
	Node *c;
	long min_value = 0;
	long max_value = 0;
	long span = 0;
	int case_count = 0;
	int compare_size = 4;
	int table_label;
	int base_label;
	int *targets;

	if (!emit_target_is_arm64(cg) || !node || node->kind != ND_SWITCH || node->inc)
		return 0;

	for (c = node->body; c; c = c->next) {
		if (c->kind != ND_CASE)
			continue;
		if (case_count == 0) {
			min_value = max_value = c->value;
		} else {
			if (c->value < min_value)
				min_value = c->value;
			if (c->value > max_value)
				max_value = c->value;
		}
		case_count++;
	}

	if (case_count < 8)
		return 0;

	span = max_value - min_value + 1;
	if (span <= 0 || span > 4096 || span > case_count * 2)
		return 0;

	if (node->cond && node->cond->type) {
		compare_size = type_sizeof(node->cond->type);
		if (compare_size <= 0)
			compare_size = 4;
		if (compare_size > 8)
			compare_size = 8;
	}

	targets = xcalloc((size_t)span, sizeof(*targets));
	for (long i = 0; i < span; i++)
		targets[i] = default_label;
	for (c = node->body; c; c = c->next) {
		if (c->kind != ND_CASE)
			continue;
		targets[c->value - min_value] = c->offset;
	}

	table_label = new_label();
	base_label = new_label();

	emit_expr(node->cond, cg);
	printf("    mov x1, x0\n");
	if (min_value != 0) {
		cg->emit_load_imm(min_value);
		printf("    sub x1, x1, x0\n");
	}
	cg->emit_load_imm(span);
	if (compare_size <= 4)
		printf("    cmp w1, w0\n");
	else
		printf("    cmp x1, x0\n");
	printf("    b.hs L%d\n", default_label);
	printf("    adrp x9, L%d@PAGE\n", table_label);
	printf("    add x9, x9, L%d@PAGEOFF\n", table_label);
	printf("    adr x10, L%d\n", base_label);
	printf("    ldrsw x11, [x9, w1, uxtw #2]\n");
	printf("    add x10, x10, x11\n");
	printf("    br x10\n");

	printf("L%d:\n", base_label);

	for (c = node->body; c; c = c->next) {
		cg->emit_label(c->offset);
		emit_block(c, cg);
	}

	cg->emit_branch(end_label);
	printf("    .p2align 2\n");
	printf("L%d:\n", table_label);
	for (long i = 0; i < span; i++)
		printf("    .word L%d-L%d\n", targets[i], base_label);

	xfree(targets);
	return 1;
}

static int
emit_arm64_try_emit_local_update_assign(Node *node, Codegen *cg)
{
	Node *expr;
	long imm;
	int size;

	if (!node || !cg || !emit_target_is_arm64(cg) || !node->left || !node->right)
		return 0;
	if (!emit_arm64_is_simple_local_scalar(node->left))
		return 0;
	if (!node->right || !(node->right->kind == ND_ADD || node->right->kind == ND_SUB))
		return 0;

	expr = node->right;
	if (expr->kind == ND_ADD) {
		if (emit_arm64_same_simple_local(node->left, expr->left) &&
		    emit_arm64_try_get_small_signed_imm(expr->right, &imm)) {
			size = emit_arm64_local_update_size(node->left);
			cg->emit_load_local_sized(node->left->offset, size);
			emit_apply_integer_load_cast(node->left, cg);
			emit_arm64_emit_add_sub_imm(imm);
			cg->emit_store_local_sized(node->left->offset, size);
			return 1;
		}
		if (emit_arm64_same_simple_local(node->left, expr->right) &&
		    emit_arm64_try_get_small_signed_imm(expr->left, &imm)) {
			size = emit_arm64_local_update_size(node->left);
			cg->emit_load_local_sized(node->left->offset, size);
			emit_apply_integer_load_cast(node->left, cg);
			emit_arm64_emit_add_sub_imm(imm);
			cg->emit_store_local_sized(node->left->offset, size);
			return 1;
		}
		return 0;
	}

	if (emit_arm64_same_simple_local(node->left, expr->left) &&
	    emit_arm64_try_get_small_signed_imm(expr->right, &imm)) {
		size = emit_arm64_local_update_size(node->left);
		cg->emit_load_local_sized(node->left->offset, size);
		emit_apply_integer_load_cast(node->left, cg);
		emit_arm64_emit_sub_add_imm(imm);
		cg->emit_store_local_sized(node->left->offset, size);
		return 1;
	}

	return 0;
}

static int
emit_arm64_try_emit_indirect_assign(Node *node, Codegen *cg)
{
	int size;

	if (!node || !cg || !emit_target_is_arm64(cg) || !node->left || !node->right)
		return 0;
	if (!emit_arm64_expr_preserves_saved_reg(node->right))
		return 0;

	size = emit_arm64_indirect_store_size(node->left);

	switch (node->left->kind) {
	case ND_GLOBAL_INDEX:
		emit_global_index_addr(node->left, cg);
		break;

	case ND_INDEX:
		emit_expr(node->left->left, cg);
		cg->emit_addr_indexed(node->left->offset, node->left->elem_size);
		break;

	case ND_DEREF:
		emit_expr(node->left->left, cg);
		break;

	case ND_MEMBER_PTR:
		emit_expr(node->left->left, cg);
		if (node->left->offset)
			cg->emit_add_offset(node->left->offset);
		break;

	default:
		return 0;
	}

	cg->emit_acc_to_saved();
	emit_expr(node->right, cg);
	cg->emit_store_via_saved(size);
	return 1;
}

static int
emit_arm64_try_emit_indirect_update_assign(Node *node, Codegen *cg)
{
	Node *rhs;
	Node *other;
	int size;
	int commutative = 0;

	if (!node || !cg || !emit_target_is_arm64(cg) || !node->left || !node->right)
		return 0;
	if (node->left->is_bitfield)
		return 0;
	if (emit_expr_is_floating(node->left))
		return 0;
	if (node->left->type &&
	    (type_is_struct(node->left->type) || type_is_union(node->left->type) ||
	     type_is_array(node->left->type) || type_is_function(node->left->type)))
		return 0;

	rhs = node->right;
	switch (rhs->kind) {
	case ND_ADD:
	case ND_BITAND:
	case ND_BITOR:
	case ND_BITXOR:
		commutative = 1;
		break;

	case ND_SUB:
	case ND_SHL:
	case ND_SHR:
		commutative = 0;
		break;

	default:
		return 0;
	}

	if (emit_arm64_same_expr(node->left, rhs->left) &&
	    emit_arm64_expr_preserves_saved_reg(rhs->right)) {
		other = rhs->right;
	} else if (commutative &&
	           emit_arm64_same_expr(node->left, rhs->right) &&
	           emit_arm64_expr_preserves_saved_reg(rhs->left)) {
		other = rhs->left;
	} else {
		return 0;
	}

	size = node->left->is_pointer ? TCC_SIZEOF_PTR :
	       (node->left->elem_size ? node->left->elem_size : TCC_SIZEOF_INT);

	switch (node->left->kind) {
	case ND_GLOBAL_INDEX:
		emit_global_index_addr(node->left, cg);
		break;

	case ND_INDEX:
		emit_expr(node->left->left, cg);
		cg->emit_addr_indexed(node->left->offset, node->left->elem_size);
		break;

	case ND_DEREF:
		emit_expr(node->left->left, cg);
		break;

	case ND_MEMBER_PTR:
		emit_expr(node->left->left, cg);
		if (node->left->offset)
			cg->emit_add_offset(node->left->offset);
		break;

	default:
		return 0;
	}

	cg->emit_acc_to_saved();
	cg->emit_load_via_saved(size);
	emit_apply_integer_load_cast(node->left, cg);
	cg->emit_push_acc();
	emit_expr(other, cg);
	cg->emit_pop_to_tmp();

	switch (rhs->kind) {
	case ND_ADD:
		cg->emit_add();
		break;
	case ND_SUB:
		cg->emit_sub();
		break;
	case ND_BITAND:
		cg->emit_bitand();
		break;
	case ND_BITOR:
		cg->emit_bitor();
		break;
	case ND_BITXOR:
		cg->emit_bitxor();
		break;
	case ND_SHL:
		cg->emit_shl();
		break;
	case ND_SHR:
		cg->emit_shr();
		break;
	default:
		return 0;
	}

	cg->emit_store_via_saved(size);
	return 1;
}

static int
emit_arm64_try_emit_ptr_add_deref_load(Node *node, Codegen *cg)
{
	Node *base;
	Node *index;
	int scale;
	int shift;
	int load_size;

	if (!node || !cg || !emit_target_is_arm64(cg) || node->kind != ND_DEREF || !node->left)
		return 0;
	if (emit_expr_is_floating(node))
		return 0;
	if (node->type &&
	    (type_is_struct(node->type) || type_is_union(node->type) || type_is_array(node->type)))
		return 0;
	if (!emit_arm64_try_decompose_ptr_add(node->left, &base, &index, &scale))
		return 0;
	if (!emit_arm64_expr_is_noncall_scalar(index))
		return 0;

	shift = emit_arm64_scale_shift_local(scale);
	if (shift < 0 || shift > 4)
		return 0;

	load_size = node->is_pointer ? TCC_SIZEOF_PTR :
	            (node->elem_size ? node->elem_size : TCC_SIZEOF_INT);

	emit_expr(base, cg);
	cg->emit_acc_to_saved();
	emit_expr(index, cg);

	if (load_size == 1) {
		printf("    ldrb w0, [x17, w0");
		emit_arm64_print_sxtw_suffix_local(shift);
		printf("]\n");
	} else if (load_size == 2) {
		printf("    ldrh w0, [x17, w0");
		emit_arm64_print_sxtw_suffix_local(shift);
		printf("]\n");
	} else if (load_size == 8) {
		printf("    ldr x0, [x17, w0");
		emit_arm64_print_sxtw_suffix_local(shift);
		printf("]\n");
	} else {
		printf("    ldrsw x0, [x17, w0");
		emit_arm64_print_sxtw_suffix_local(shift);
		printf("]\n");
	}

	emit_apply_integer_load_cast(node, cg);
	return 1;
}

static int
emit_arm64_try_emit_local_incdec(Node *node, Codegen *cg, int is_inc, int is_postfix)
{
	int size;

	if (!node || !cg || !emit_target_is_arm64(cg) || !node->left)
		return 0;
	if (!emit_arm64_is_simple_local_scalar(node->left))
		return 0;

	size = emit_arm64_local_update_size(node->left);
	cg->emit_load_local_sized(node->left->offset, size);
	emit_apply_integer_load_cast(node->left, cg);

	if (is_postfix)
		cg->emit_acc_to_saved();

	if (is_inc)
		printf("    add x0, x0, #1\n");
	else
		printf("    sub x0, x0, #1\n");

	cg->emit_store_local_sized(node->left->offset, size);

	if (is_postfix)
		cg->emit_saved_to_acc();

	return 1;
}

static int
emit_arm64_try_emit_dead_incdec_stmt(Node *node, Codegen *cg)
{
	int is_inc;
	int size;

	if (!node || !cg || !emit_target_is_arm64(cg) || !node->left)
		return 0;

	switch (node->kind) {
	case ND_PRE_INC:
	case ND_POST_INC:
		is_inc = 1;
		break;
	case ND_PRE_DEC:
	case ND_POST_DEC:
		is_inc = 0;
		break;
	default:
		return 0;
	}

	if (node->left->is_bitfield)
		return 0;

	if (emit_arm64_is_simple_local_scalar(node->left)) {
		int local_size = emit_arm64_local_update_size(node->left);

		cg->emit_load_local_sized(node->left->offset, local_size);
		emit_apply_integer_load_cast(node->left, cg);
		if (is_inc)
			printf("    add x0, x0, #1\n");
		else
			printf("    sub x0, x0, #1\n");
		cg->emit_store_local_sized(node->left->offset, local_size);
		return 1;
	}

	if (node->left->kind == ND_GLOBAL) {
		int global_size = node->left->is_pointer ? TCC_SIZEOF_PTR :
		                  (node->left->elem_size ? node->left->elem_size : TCC_SIZEOF_INT);

		cg->emit_load_global(node->left->name, global_size);
		if (is_inc)
			printf("    add x0, x0, #1\n");
		else
			printf("    sub x0, x0, #1\n");
		cg->emit_store_global(node->left->name, global_size);
		return 1;
	}

	size = node->left->is_pointer ? TCC_SIZEOF_PTR :
	       (node->left->elem_size ? node->left->elem_size : TCC_SIZEOF_INT);

	if (node->left->kind == ND_MEMBER_PTR) {
		emit_expr(node->left->left, cg);
		if (node->left->offset)
			cg->emit_add_offset(node->left->offset);
		cg->emit_acc_to_saved();
		cg->emit_load_via_saved(size);
		cg->emit_acc_to_tmp();
		cg->emit_load_imm(1);
		if (is_inc)
			cg->emit_add();
		else
			cg->emit_sub();
		cg->emit_store_via_saved(size);
		return 1;
	}

	if (node->left->kind == ND_DEREF && cg->emit_incdec_deref) {
		emit_expr(node->left->left, cg);
		cg->emit_acc_to_saved();
		cg->emit_load_via_saved(size);
		cg->emit_acc_to_tmp();
		cg->emit_load_imm(1);
		if (is_inc)
			cg->emit_add();
		else
			cg->emit_sub();
		cg->emit_store_via_saved(size);
		return 1;
	}

	return 0;
}

static int
emit_arm64_try_branch_if_nonzero(Node *node, int label, Codegen *cg)
{
	int skip_label;

	if (!node || !cg || !emit_target_is_arm64(cg) || label <= 0)
		return 0;

	switch (node->kind) {
	case ND_NOT:
		return emit_arm64_try_branch_if_zero(node->left, label, cg);

	case ND_LOGICAL_OR:
		if (emit_arm64_try_branch_if_nonzero(node->left, label, cg)) {
			if (emit_arm64_try_branch_if_nonzero(node->right, label, cg))
				return 1;
			emit_expr(node->right, cg);
			cg->emit_branch_if_nonzero(label);
			return 1;
		}
		break;

	case ND_LOGICAL_AND:
		skip_label = new_label();
		if (emit_arm64_try_branch_if_zero(node->left, skip_label, cg)) {
			if (emit_arm64_try_branch_if_nonzero(node->right, label, cg)) {
				cg->emit_label(skip_label);
				return 1;
			}
			emit_expr(node->right, cg);
			cg->emit_branch_if_nonzero(label);
			cg->emit_label(skip_label);
			return 1;
		}
		break;

	case ND_EQ:
		return emit_arm64_try_branch_on_compare(node, "eq", label, cg);
	case ND_NE:
		return emit_arm64_try_branch_on_compare(node, "ne", label, cg);
	case ND_LT:
		return emit_arm64_try_branch_on_compare(node,
			emit_compare_is_unsigned(node) ? "lo" : "lt", label, cg);
	case ND_LE:
		return emit_arm64_try_branch_on_compare(node,
			emit_compare_is_unsigned(node) ? "ls" : "le", label, cg);
	case ND_GT:
		return emit_arm64_try_branch_on_compare(node,
			emit_compare_is_unsigned(node) ? "hi" : "gt", label, cg);
	case ND_GE:
		return emit_arm64_try_branch_on_compare(node,
			emit_compare_is_unsigned(node) ? "hs" : "ge", label, cg);
	default:
		break;
	}

	return 0;
}

static void
emit_apply_integer_load_cast(Node *node, Codegen *cg)
{
	Type *type;
	int size;
	int is_unsigned;

	if (!node || !cg || !cg->emit_cast)
		return;

	type = node->type;
	if (node->is_pointer)
		return;
	if (type && (type_is_array(type) || type_is_struct(type) || type_is_union(type) ||
	             type_is_function(type) || type_is_fp_scalar(type)))
		return;
	if (type && !type_is_integer(type))
		return;
	size = (type && type->size > 0) ? type->size : node->elem_size;
	if (size <= 0 || size >= 8)
		return;
	is_unsigned = type ? type_is_unsigned(type) : node->is_unsigned;
	if (emit_target_is_arm64(cg) && size == 4 && !is_unsigned)
		return;

	cg->emit_cast(size, is_unsigned);
}

static unsigned long long
emit_fp_literal_bits(const Node *node)
{
	double value;

	if (!node || !node->string_value)
		return 0;

	value = strtod(node->string_value, NULL);
	if (node->type && node->type->kind == TY_FLOAT) {
		union {
			double d;
			unsigned long long u;
		} bits;
		bits.d = value;
		return tcc_double_bits_to_float_bits(bits.u);
	} else {
		union {
			double d;
			unsigned long long u;
		} bits;
			bits.d = value;
			return bits.u;
		}
}

static void
emit_arm64_fp_push(int size)
{
	printf("    sub sp, sp, #16\n");
	if (size == 4)
		printf("    str s0, [sp]\n");
	else
		printf("    str d0, [sp]\n");
}

static void
emit_arm64_fp_pop_to_tmp(int size)
{
	if (size == 4)
		printf("    ldr s1, [sp]\n");
	else
		printf("    ldr d1, [sp]\n");
	printf("    add sp, sp, #16\n");
}

static void
emit_arm64_fp_load_addr(const char *addr_reg, int size)
{
	printf("    ldr %c0, [%s]\n", size == 4 ? 's' : 'd', addr_reg);
}

static void
emit_arm64_fp_store_addr(const char *addr_reg, int size)
{
	printf("    str %c0, [%s]\n", size == 4 ? 's' : 'd', addr_reg);
}

static int
emit_arm64_hfa_info(const Type *type, int *elem_size_out, int *elem_count_out)
{
	return parser_arm64_hfa_info_type(type, elem_size_out, elem_count_out);
}

static void
emit_arm64_load_hfa_return_value(Node *value, Codegen *cg)
{
	int elem_size;
	int elem_count;
	int i;

	if (!emit_arm64_hfa_info(value ? value->type : NULL, &elem_size, &elem_count))
		ICE("arm64 HFA load requires HFA-typed value");

	emit_struct_lvalue_addr(value, cg);
	cg->emit_acc_to_saved();
	for (i = 0; i < elem_count; i++) {
		printf("    ldr %c%d, [x17, #%d]\n",
		       elem_size == 4 ? 's' : 'd',
		       i,
		       i * elem_size);
	}
}

static void
emit_arm64_store_hfa_return_lvalue(Node *dst, Type *type, Codegen *cg)
{
	int elem_size;
	int elem_count;
	int i;

	if (!emit_arm64_hfa_info(type, &elem_size, &elem_count))
		ICE("arm64 HFA store requires HFA-typed destination");

	emit_struct_lvalue_addr(dst, cg);
	cg->emit_acc_to_saved();
	for (i = 0; i < elem_count; i++) {
		printf("    str %c%d, [x17, #%d]\n",
		       elem_size == 4 ? 's' : 'd',
		       i,
		       i * elem_size);
	}
}

static void
emit_arm64_int_to_fp(const Type *src_type, const Type *dst_type)
{
	int dst_size;
	int is_unsigned;

	if (!dst_type)
		ICE("missing floating destination type");

	dst_size = dst_type->size ? dst_type->size : 8;
	is_unsigned = src_type ? type_is_unsigned(src_type) : 0;
	printf("    %s %c0, x0\n",
	       is_unsigned ? "ucvtf" : "scvtf",
	       dst_size == 4 ? 's' : 'd');
}

static void
emit_arm64_fp_store_incoming_param(Codegen *cg, int fp_reg, int offset, int size)
{
	if (cg->emit_store_fp_param) {
		cg->emit_store_fp_param(fp_reg, offset, size);
		return;
	}
	cg->emit_addr_local(offset);
	if (size == 4)
		printf("    str s%d, [x0]\n", fp_reg);
	else
		printf("    str d%d, [x0]\n", fp_reg);
}

static void
emit_arm64_fp_lvalue_addr(Node *node, Codegen *cg)
{
	if (!node)
		ICE("missing floating lvalue");

	switch (node->kind) {
	case ND_VAR:
	case ND_MEMBER:
		cg->emit_addr_local(node->offset);
		return;

	case ND_GLOBAL:
		cg->emit_load_func_addr(node->name);
		return;

	case ND_DEREF:
		if (node->left && node->left->kind == ND_INDEX) {
			emit_expr(node->left->left, cg);
			cg->emit_addr_indexed(node->left->offset, node->left->elem_size);
		} else if (node->left && node->left->kind == ND_GLOBAL_INDEX) {
			emit_global_index_addr(node->left, cg);
		} else {
			emit_expr(node->left, cg);
		}
		return;

	case ND_MEMBER_PTR:
		if (node->left && node->left->kind == ND_INDEX) {
			emit_expr(node->left->left, cg);
			cg->emit_addr_indexed(node->left->offset, node->left->elem_size);
		} else if (node->left && node->left->kind == ND_GLOBAL_INDEX) {
			emit_global_index_addr(node->left, cg);
		} else {
			emit_expr(node->left, cg);
		}
		if (node->offset)
			cg->emit_add_offset(node->offset);
		return;

	case ND_INDEX:
		emit_expr(node->left, cg);
		cg->emit_addr_indexed(node->offset, node->elem_size);
		return;

	default:
		node_error_at(node, "unsupported floating lvalue kind %s",
		              node_kind_name(node->kind));
	}
}

static void
emit_arm64_fp_load_local(Node *node, Codegen *cg)
{
	int size;

	if (!node || !node->type)
		ICE("missing floating local");

	size = node->type->size ? node->type->size : 8;
	cg->emit_load_local_sized(node->offset, size);
	if (size == 4)
		printf("    fmov s0, w0\n");
	else
		printf("    fmov d0, x0\n");
}

static int
emit_x86_param_stack_words(const Type *type)
{
	int size;

	if (!type)
		return 1;
	if (type_is_pointer(type) || type_is_function(type) || type_is_array(type))
		return 1;
	if (type_source_kind(type) == TYPE_SOURCE_LONG ||
	    type_source_kind(type) == TYPE_SOURCE_ULONG)
		return 1;

	size = type_sizeof(type);
	if (size <= 0)
		size = 4;
	return (size + 3) / 4;
}

static int
emit_push_call_arg_typed(Node *arg, const Type *arg_type, const char *abi_name, Codegen *cg)
{
	int aggregate_regs = 0;
	int hfa_elem_size;
	int hfa_elem_count;

	if (emit_target_is_arm64(cg) &&
	    arg && arg_type && type_is_struct(arg_type) &&
	    parser_classify_aggregate_abi((Type *)arg_type, &aggregate_regs) == AGGREGATE_ABI_INTREGS) {
		int size;
		Node *value;

		if (aggregate_regs < 1 || aggregate_regs > 2)
			ICE("unsupported arm64 INTREGS aggregate register count");
		value = emit_arm64_prepare_direct_aggregate_arg(arg, cg);
		if (!value || (value->kind != ND_VAR && value->kind != ND_GLOBAL))
			ICE("unsupported arm64 INTREGS aggregate call argument");
		size = type_sizeof((Type *)arg_type);
		emit_struct_lvalue_addr(value, cg);
		cg->emit_acc_to_saved();
		for (int i = aggregate_regs - 1; i >= 0; i--) {
			int off = i * (int)TCC_SIZEOF_PTR;
			int chunk = size - off;

			if (chunk <= 0)
				continue;
			emit_arm64_load_intreg_chunk("x17", off, chunk, 0);
			cg->emit_push_acc();
		}
		return aggregate_regs;
	}

	if (emit_target_is_arm64(cg) &&
	    (emit_arm64_hfa_info((Type *)arg_type, &hfa_elem_size, &hfa_elem_count) ||
	     parser_arm64_hfa_info_name(abi_name, &hfa_elem_size, &hfa_elem_count))) {
		int i;

		emit_struct_lvalue_addr(arg, cg);
		cg->emit_acc_to_saved();
		for (i = hfa_elem_count - 1; i >= 0; i--) {
			printf("    ldr %c0, [x17, #%d]\n",
			       hfa_elem_size == 4 ? 's' : 'd',
			       i * hfa_elem_size);
			emit_arm64_fp_push(hfa_elem_size);
		}
		return hfa_elem_count;
	}

	if (emit_target_is_arm64(cg) && emit_expr_is_floating(arg)) {
		emit_expr(arg, cg);
		emit_arm64_fp_push(arg_type ? arg_type->size : (arg->type ? arg->type->size : 8));
		return 1;
	}

	if (emit_target_is_x86(cg) &&
	    arg && arg->type && type_is_struct(arg->type) &&
	    cg->emit_push_struct_arg) {
		emit_struct_lvalue_addr(arg, cg);
		cg->emit_push_struct_arg(type_sizeof(arg->type));
		return emit_x86_param_stack_words(arg->type);
	}

	emit_expr(arg, cg);
	cg->emit_push_acc();
	return 1;
}

static int
emit_push_call_arg(Node *arg, Codegen *cg)
{
	return emit_push_call_arg_typed(arg, arg ? arg->type : NULL, NULL, cg);
}

static void
emit_arm64_load_intreg_chunk(const char *base_reg, int offset, int chunk, int dst_reg)
{
	if (chunk >= (int)TCC_SIZEOF_PTR) {
		printf("    ldr x%d, [%s, #%d]\n", dst_reg, base_reg, offset);
		return;
	}
	if (chunk >= 4) {
		printf("    ldr w%d, [%s, #%d]\n", dst_reg, base_reg, offset);
		if (chunk > 4) {
			int tail = chunk - 4;

			if (tail >= 2) {
				printf("    ldrh w9, [%s, #%d]\n", base_reg, offset + 4);
				printf("    orr x%d, x%d, x9, lsl #32\n", dst_reg, dst_reg);
				if (tail > 2) {
					printf("    ldrb w9, [%s, #%d]\n", base_reg, offset + 6);
					printf("    orr x%d, x%d, x9, lsl #48\n", dst_reg, dst_reg);
				}
			} else {
				printf("    ldrb w9, [%s, #%d]\n", base_reg, offset + 4);
				printf("    orr x%d, x%d, x9, lsl #32\n", dst_reg, dst_reg);
			}
		}
		return;
	}
	if (chunk >= 2) {
		printf("    ldrh w%d, [%s, #%d]\n", dst_reg, base_reg, offset);
		if (chunk > 2) {
			printf("    ldrb w9, [%s, #%d]\n", base_reg, offset + 2);
			printf("    orr x%d, x%d, x9, lsl #16\n", dst_reg, dst_reg);
		}
		return;
	}
	printf("    ldrb w%d, [%s, #%d]\n", dst_reg, base_reg, offset);
}

static int
emit_push_call_args_reverse(Node *arg, Codegen *cg)
{
	if (!arg)
		return 0;
	return emit_push_call_args_reverse(arg->next, cg) + emit_push_call_arg(arg, cg);
}

static int
emit_arm64_load_intreg_aggregate_arg(Node *arg, int start_reg, Codegen *cg)
{
	int size;
	int reg_count;
	Node *value;

	if (!emit_target_is_arm64(cg) || !arg || !arg->type || !type_is_struct(arg->type))
		return 0;
	if (parser_classify_aggregate_abi(arg->type, &reg_count) != AGGREGATE_ABI_INTREGS)
		return 0;
	if (start_reg < 0 || start_reg + reg_count > 8)
		return 0;
	value = emit_arm64_prepare_direct_aggregate_arg(arg, cg);
	if (!value || (value->kind != ND_VAR && value->kind != ND_GLOBAL))
		return 0;

	size = type_sizeof(arg->type);
	emit_struct_lvalue_addr(value, cg);
	cg->emit_acc_to_saved();
	for (int i = 0; i < reg_count; i++) {
		int off = i * (int)TCC_SIZEOF_PTR;
		int chunk = size - off;

		if (chunk <= 0)
			break;
		emit_arm64_load_intreg_chunk("x17", off, chunk, start_reg + i);
	}
	return reg_count;
}

static int
emit_arm64_store_intreg_aggregate_stack_arg(Node *arg, int stack_offset, Codegen *cg)
{
	int size;
	int reg_count;
	Node *value;

	if (!emit_target_is_arm64(cg) || !arg || !arg->type || !type_is_struct(arg->type))
		return 0;
	if (parser_classify_aggregate_abi(arg->type, &reg_count) != AGGREGATE_ABI_INTREGS)
		return 0;
	if (reg_count < 1 || reg_count > 2)
		return 0;
	value = emit_arm64_prepare_direct_aggregate_arg(arg, cg);
	if (!value || (value->kind != ND_VAR && value->kind != ND_GLOBAL))
		return 0;

	size = type_sizeof(arg->type);
	if (size <= 0 || size > 16)
		return 0;

	emit_struct_lvalue_addr(value, cg);
	cg->emit_acc_to_saved();
	for (int i = 0; i < reg_count; i++) {
		int off = i * (int)TCC_SIZEOF_PTR;
		int chunk = size - off;

		if (chunk <= 0)
			break;
		emit_arm64_load_intreg_chunk("x17", off, chunk, 16);
		printf("    str x16, [sp, #%d]\n", stack_offset + off);
	}
	return 1;
}

static int
emit_arm64_direct_intreg_aggregate_call(Node *node, Codegen *cg)
{
	Node **arg_nodes;
	int *arg_regs;
	int *arg_stack_offsets;
	Node *arg;
	int count;
	int fixed_params;
	int reg_index = 0;
	int stack_bytes = 0;
	int total_stack_bytes;
	int i;

	if (!emit_target_is_arm64(cg) || !node || node->kind != ND_CALL ||
	    node->left || !node->name[0])
		return 0;

	fixed_params = func_fixed_params(node->name);
	if (fixed_params >= 0 && count_list(node->args) > fixed_params)
		return 0;

	count = count_list(node->args);
	arg_nodes = xcalloc((size_t)(count > 0 ? count : 1), sizeof(Node *));
	arg_regs = xcalloc((size_t)(count > 0 ? count : 1), sizeof(int));
	arg_stack_offsets = xcalloc((size_t)(count > 0 ? count : 1), sizeof(int));
	arg = node->args;
	for (i = 0; i < count; i++, arg = arg->next) {
		arg_nodes[i] = arg;
		arg_regs[i] = -1;
		arg_stack_offsets[i] = -1;
	}

	for (i = 0; i < count; i++) {
		Node *cur = arg_nodes[i];
		int regs = 0;

		if (!(cur && cur->type && type_is_struct(cur->type))) {
			xfree(arg_regs);
			xfree(arg_stack_offsets);
			xfree(arg_nodes);
			return 0;
		}

		if (parser_classify_aggregate_abi(cur->type, &regs) != AGGREGATE_ABI_INTREGS)
			regs = 0;
		if (regs <= 0) {
			xfree(arg_regs);
			xfree(arg_stack_offsets);
			xfree(arg_nodes);
			return 0;
		}
		if (reg_index + regs <= 8) {
			arg_regs[i] = reg_index;
			reg_index += regs;
		} else {
			int size = type_sizeof(cur->type);
			int slot_size;

			if (size <= 0 || size > 16) {
				xfree(arg_regs);
				xfree(arg_stack_offsets);
				xfree(arg_nodes);
				return 0;
			}
			slot_size = (size + 7) & ~7;
			arg_stack_offsets[i] = stack_bytes;
			stack_bytes += slot_size;
		}
	}

	total_stack_bytes = (stack_bytes + 15) & ~15;
	if (total_stack_bytes > 0)
		cg->emit_stack_alloc(total_stack_bytes);

	for (i = count - 1; i >= 0; i--) {
		Node *cur = arg_nodes[i];

		if (arg_stack_offsets[i] >= 0) {
			if (cur && cur->type && type_is_struct(cur->type)) {
				if (!emit_arm64_store_intreg_aggregate_stack_arg(cur,
				                                                 arg_stack_offsets[i],
				                                                 cg)) {
					xfree(arg_regs);
					xfree(arg_stack_offsets);
					xfree(arg_nodes);
					return 0;
				}
				continue;
			}

			emit_expr(cur, cg);
			printf("    str x0, [sp, #%d]\n", arg_stack_offsets[i]);
			continue;
		}

		if (!emit_arm64_load_intreg_aggregate_arg(cur, arg_regs[i], cg)) {
			xfree(arg_regs);
			xfree(arg_stack_offsets);
			xfree(arg_nodes);
			return 0;
		}
	}

	cg->emit_call(node->name);
	if (total_stack_bytes > 0)
		printf("    add sp, sp, #%d\n", total_stack_bytes);
	xfree(arg_regs);
	xfree(arg_stack_offsets);
	xfree(arg_nodes);
	return 1;
}

static int
emit_arm64_store_intreg_aggregate_return_local(Node *dst, Node *call, Codegen *cg)
{
	int size;
	int reg_count;

	if (!emit_target_is_arm64(cg) || !dst || !call || !dst->type ||
	    !type_is_struct(dst->type) || !call->returns_struct)
		return 0;
	if (call->aggregate_abi_class != AGGREGATE_ABI_INTREGS)
		return 0;
	if (dst->kind != ND_VAR)
		return 0;

	size = type_sizeof(dst->type);
	if (size <= 0 || size > 16)
		return 0;
	reg_count = call->aggregate_abi_reg_count;
	if (reg_count <= 0)
		reg_count = (size + (int)TCC_SIZEOF_PTR - 1) / (int)TCC_SIZEOF_PTR;
	if (reg_count < 1 || reg_count > 2)
		return 0;

	printf("    sub x9, x29, #%d\n", -dst->offset);
	if (size >= 8) {
		printf("    str x0, [x9, #0]\n");
	} else if (size >= 4) {
		printf("    str w0, [x9, #0]\n");
		if (size > 4) {
			int tail = size - 4;
			printf("    lsr x10, x0, #32\n");
			if (tail >= 2) {
				printf("    strh w10, [x9, #4]\n");
				if (tail > 2) {
					printf("    lsr x10, x10, #16\n");
					printf("    strb w10, [x9, #6]\n");
				}
			} else {
				printf("    strb w10, [x9, #4]\n");
			}
		}
	} else if (size >= 2) {
		printf("    strh w0, [x9, #0]\n");
		if (size > 2) {
			printf("    lsr x10, x0, #16\n");
			printf("    strb w10, [x9, #2]\n");
		}
	} else {
		printf("    strb w0, [x9, #0]\n");
	}
	if (reg_count == 2) {
		int second_size = size - (int)TCC_SIZEOF_PTR;

		if (second_size > 0) {
			if (second_size >= 8) {
				printf("    str x1, [x9, #%zu]\n", (size_t)TCC_SIZEOF_PTR);
			} else if (second_size >= 4) {
				printf("    str w1, [x9, #%zu]\n", (size_t)TCC_SIZEOF_PTR);
				if (second_size > 4) {
					int tail = second_size - 4;
					printf("    lsr x10, x1, #32\n");
					if (tail >= 2) {
						printf("    strh w10, [x9, #%zu]\n", (size_t)TCC_SIZEOF_PTR + 4);
						if (tail > 2) {
							printf("    lsr x10, x10, #16\n");
							printf("    strb w10, [x9, #%zu]\n", (size_t)TCC_SIZEOF_PTR + 6);
						}
					} else {
						printf("    strb w10, [x9, #%zu]\n", (size_t)TCC_SIZEOF_PTR + 4);
					}
				}
			} else if (second_size >= 2) {
				printf("    strh w1, [x9, #%zu]\n", (size_t)TCC_SIZEOF_PTR);
				if (second_size > 2) {
					printf("    lsr x10, x1, #16\n");
					printf("    strb w10, [x9, #%zu]\n", (size_t)TCC_SIZEOF_PTR + 2);
				}
			} else {
				printf("    strb w1, [x9, #%zu]\n", (size_t)TCC_SIZEOF_PTR);
			}
		}
	}
	return 1;
}

static int
emit_arm64_load_intreg_aggregate_return_value(Node *value, Codegen *cg)
{
	int size;

	if (!emit_target_is_arm64(cg) || !value || !value->type || !type_is_struct(value->type))
		return 0;
	if (parser_classify_aggregate_abi(value->type, NULL) != AGGREGATE_ABI_INTREGS)
		return 0;

	size = type_sizeof(value->type);
	if (size <= 0 || size > 16)
		return 0;

	emit_struct_lvalue_addr(value, cg);
	cg->emit_acc_to_saved();
	if (size >= (int)TCC_SIZEOF_PTR) {
		printf("    ldr x0, [x17, #0]\n");
	} else if (size >= 4) {
		printf("    ldr w0, [x17, #0]\n");
		if (size > 4) {
			int tail = size - 4;
			if (tail >= 2) {
				printf("    ldrh w9, [x17, #4]\n");
				printf("    orr x0, x0, x9, lsl #32\n");
				if (tail > 2) {
					printf("    ldrb w9, [x17, #6]\n");
					printf("    orr x0, x0, x9, lsl #48\n");
				}
			} else {
				printf("    ldrb w9, [x17, #4]\n");
				printf("    orr x0, x0, x9, lsl #32\n");
			}
		}
	} else if (size >= 2) {
		printf("    ldrh w0, [x17, #0]\n");
		if (size > 2) {
			printf("    ldrb w9, [x17, #2]\n");
			printf("    orr x0, x0, x9, lsl #16\n");
		}
	} else {
		printf("    ldrb w0, [x17, #0]\n");
	}
	if (size > (int)TCC_SIZEOF_PTR) {
		int second_size = size - (int)TCC_SIZEOF_PTR;
		if (second_size >= (int)TCC_SIZEOF_PTR)
			printf("    ldr x1, [x17, #%zu]\n", (size_t)TCC_SIZEOF_PTR);
		else if (second_size >= 4) {
			printf("    ldr w1, [x17, #%zu]\n", (size_t)TCC_SIZEOF_PTR);
			if (second_size > 4) {
				int tail = second_size - 4;
				if (tail >= 2) {
					printf("    ldrh w9, [x17, #%zu]\n", (size_t)TCC_SIZEOF_PTR + 4);
					printf("    orr x1, x1, x9, lsl #32\n");
					if (tail > 2) {
						printf("    ldrb w9, [x17, #%zu]\n", (size_t)TCC_SIZEOF_PTR + 6);
						printf("    orr x1, x1, x9, lsl #48\n");
					}
				} else {
					printf("    ldrb w9, [x17, #%zu]\n", (size_t)TCC_SIZEOF_PTR + 4);
					printf("    orr x1, x1, x9, lsl #32\n");
				}
			}
		} else if (second_size >= 2) {
			printf("    ldrh w1, [x17, #%zu]\n", (size_t)TCC_SIZEOF_PTR);
			if (second_size > 2) {
				printf("    ldrb w9, [x17, #%zu]\n", (size_t)TCC_SIZEOF_PTR + 2);
				printf("    orr x1, x1, x9, lsl #16\n");
			}
		} else {
			printf("    ldrb w1, [x17, #%zu]\n", (size_t)TCC_SIZEOF_PTR);
		}
	}
	return 1;
}

static int
emit_arm64_direct_sret_small_gpr_call(Node *dst, Node *call, Codegen *cg)
{
	Node **arg_nodes;
	Node *arg;
	FuncInfo *fi;
	Type **param_types;
	char **param_struct_names;
	int param_count;
	int count;
	int fixed_params;
	int i;

	if (!emit_target_is_arm64(cg) || !dst || !call || dst->kind != ND_VAR ||
	    call->kind != ND_CALL || call->left || !call->name[0] || !call->returns_struct)
		return 0;
	if (call->aggregate_abi_class == AGGREGATE_ABI_INTREGS)
		return 0;

	fixed_params = func_fixed_params(call->name);
	if (fixed_params >= 0 && count_list(call->args) > fixed_params)
		return 0;

	count = count_list(call->args);
	if (count > 8)
		return 0;
	fi = find_func(call->name);
	param_types = fi ? fi->param_types : NULL;
	param_struct_names = fi ? fi->param_struct_names : NULL;
	param_count = fi ? fi->param_type_count : 0;

	arg_nodes = xcalloc((size_t)(count > 0 ? count : 1), sizeof(Node *));
	arg = call->args;
	for (i = 0; i < count; i++, arg = arg->next) {
		Type *abi_type = (i < param_count && param_types && param_types[i])
		               ? param_types[i]
		               : (arg ? arg->type : NULL);
		const char *abi_name = (i < param_count && param_struct_names && param_struct_names[i])
		                     ? param_struct_names[i]
		                     : NULL;
		arg_nodes[i] = arg;
		if (!emit_arm64_direct_gpr_arg_ok_typed(arg, abi_type, abi_name)) {
			xfree(arg_nodes);
			return 0;
		}
	}

	for (i = count - 1; i >= 1; i--) {
		emit_expr(arg_nodes[i], cg);
		cg->emit_acc_to_arg(i);
	}

	if (count > 0) {
		emit_expr(arg_nodes[0], cg);
		cg->emit_acc_to_saved();
	}

	cg->emit_addr_local(dst->offset);
	printf("    mov x8, x0\n");
	if (count > 0)
		printf("    mov x0, x17\n");
	cg->emit_call(call->name);
	xfree(arg_nodes);
	return 1;
}

static int
emit_arm64_direct_sret_small_gpr_call_to_saved(Node *call, Codegen *cg)
{
	Node **arg_nodes;
	Node *arg;
	FuncInfo *fi;
	Type **param_types;
	char **param_struct_names;
	int param_count;
	int count;
	int fixed_params;
	int i;

	if (!emit_target_is_arm64(cg) || !call || call->kind != ND_CALL ||
	    call->left || !call->name[0] || !call->returns_struct)
		return 0;
	if (call->aggregate_abi_class == AGGREGATE_ABI_INTREGS)
		return 0;

	fixed_params = func_fixed_params(call->name);
	if (fixed_params >= 0 && count_list(call->args) > fixed_params)
		return 0;

	count = count_list(call->args);
	if (count > 8)
		return 0;
	fi = find_func(call->name);
	param_types = fi ? fi->param_types : NULL;
	param_struct_names = fi ? fi->param_struct_names : NULL;
	param_count = fi ? fi->param_type_count : 0;

	arg_nodes = xcalloc((size_t)(count > 0 ? count : 1), sizeof(Node *));
	arg = call->args;
	for (i = 0; i < count; i++, arg = arg->next) {
		Type *abi_type = (i < param_count && param_types && param_types[i])
		               ? param_types[i]
		               : (arg ? arg->type : NULL);
		const char *abi_name = (i < param_count && param_struct_names && param_struct_names[i])
		                     ? param_struct_names[i]
		                     : NULL;
		arg_nodes[i] = arg;
		if (!emit_arm64_direct_gpr_arg_ok_typed(arg, abi_type, abi_name)) {
			xfree(arg_nodes);
			return 0;
		}
	}

	for (i = count - 1; i >= 0; i--) {
		emit_expr(arg_nodes[i], cg);
		cg->emit_acc_to_arg(i);
	}

	printf("    mov x8, x17\n");
	cg->emit_call(call->name);
	xfree(arg_nodes);
	return 1;
}

static int
emit_arm64_fixed_fp_sret_call(Node *dst, Node *call, Codegen *cg)
{
	Node **arg_nodes;
	int *arg_slots;
	int count;
	int int_reg = 0;
	int fp_reg = 0;
	int has_fp_param = 0;
	int total_slots = 0;
	int outgoing_stack_slots = 0;

	if (!emit_target_is_arm64(cg) || !dst || !call || dst->kind != ND_VAR ||
	    call->kind != ND_CALL || call->left || !call->name[0] || !call->returns_struct)
		return 0;
	if (call->aggregate_abi_class == AGGREGATE_ABI_INTREGS)
		return 0;

	count = count_list(call->args);
	arg_nodes = xcalloc((size_t)(count > 0 ? count : 1), sizeof(Node *));
	arg_slots = xcalloc((size_t)(count > 0 ? count : 1), sizeof(int));
	{
		Node *arg = call->args;
		int idx = 0;
		for (; arg; arg = arg->next)
			arg_nodes[idx++] = arg;
	}

	for (int i = 0; i < count; i++) {
		Type *arg_type = arg_nodes[i] ? arg_nodes[i]->type : NULL;
		int aggregate_regs = 0;
		int hfa_elem_size;
		int hfa_elem_count;

		if (arg_type && type_is_struct(arg_type) &&
		    parser_classify_aggregate_abi(arg_type, &aggregate_regs) == AGGREGATE_ABI_INTREGS) {
			arg_slots[i] = aggregate_regs;
		} else if (emit_arm64_hfa_info(arg_type, &hfa_elem_size, &hfa_elem_count)) {
			has_fp_param = 1;
			arg_slots[i] = hfa_elem_count;
		} else if (emit_type_is_floating_scalar(arg_type)) {
			has_fp_param = 1;
			arg_slots[i] = 1;
		} else {
			arg_slots[i] = 1;
		}
		total_slots += arg_slots[i];
	}
	if (!has_fp_param) {
		xfree(arg_slots);
		xfree(arg_nodes);
		return 0;
	}

	cg->emit_addr_local(dst->offset);
	cg->emit_acc_to_saved();

	for (int j = count - 1; j >= 0; j--)
		emit_push_call_arg(arg_nodes[j], cg);

	for (int i = 0, stack_slot = 0; i < count; i++) {
		Type *arg_type = arg_nodes[i] ? arg_nodes[i]->type : NULL;
		int size = (arg_type && arg_type->size) ? arg_type->size : 8;
		int aggregate_regs = 0;
		int hfa_elem_size;
		int hfa_elem_count;

		if (arg_type && type_is_struct(arg_type) &&
		    parser_classify_aggregate_abi(arg_type, &aggregate_regs) == AGGREGATE_ABI_INTREGS) {
			for (int j = 0; j < aggregate_regs; j++) {
				int src_off = (stack_slot + j) * 16;
				int dst_off = outgoing_stack_slots * 8;

				if (int_reg < 8) {
					printf("    ldr x%d, [sp, #%d]\n", int_reg, src_off);
					int_reg++;
				} else {
					printf("    ldr x9, [sp, #%d]\n", src_off);
					printf("    str x9, [sp, #%d]\n", dst_off);
					outgoing_stack_slots++;
				}
			}
		} else if (emit_arm64_hfa_info(arg_type, &hfa_elem_size, &hfa_elem_count)) {
			for (int j = 0; j < hfa_elem_count; j++) {
				int src_off = (stack_slot + j) * 16;
				int dst_off = outgoing_stack_slots * 8;
				char fp_kind = hfa_elem_size == 4 ? 's' : 'd';

				if (fp_reg < 8) {
					printf("    ldr %c%d, [sp, #%d]\n", fp_kind, fp_reg, src_off);
					fp_reg++;
				} else {
					printf("    ldr %c16, [sp, #%d]\n", fp_kind, src_off);
					printf("    str %c16, [sp, #%d]\n", fp_kind, dst_off);
					outgoing_stack_slots++;
				}
			}
		} else if (emit_type_is_floating_scalar(arg_type)) {
			char fp_kind = size == 4 ? 's' : 'd';
			int src_off = stack_slot * 16;
			int dst_off = outgoing_stack_slots * 8;

			if (fp_reg < 8) {
				printf("    ldr %c%d, [sp, #%d]\n", fp_kind, fp_reg, src_off);
				fp_reg++;
			} else {
				printf("    ldr %c16, [sp, #%d]\n", fp_kind, src_off);
				printf("    str %c16, [sp, #%d]\n", fp_kind, dst_off);
				outgoing_stack_slots++;
			}
		} else {
			int src_off = stack_slot * 16;
			int dst_off = outgoing_stack_slots * 8;

			if (int_reg < 8) {
				printf("    ldr x%d, [sp, #%d]\n", int_reg, src_off);
				int_reg++;
			} else {
				printf("    ldr x9, [sp, #%d]\n", src_off);
				printf("    str x9, [sp, #%d]\n", dst_off);
				outgoing_stack_slots++;
			}
		}
		stack_slot += arg_slots[i];
	}

	printf("    mov x8, x17\n");
	cg->emit_call(call->name);
	if (total_slots > 0)
		printf("    add sp, sp, #%d\n", total_slots * 16);
	xfree(arg_slots);
	xfree(arg_nodes);
	return 1;
}

static int
emit_arm64_fixed_fp_sret_call_to_saved(Node *call, Codegen *cg)
{
	Node **arg_nodes;
	int *arg_slots;
	int count;
	int int_reg = 0;
	int fp_reg = 0;
	int has_fp_param = 0;
	int total_slots = 0;
	int outgoing_stack_slots = 0;

	if (!emit_target_is_arm64(cg) || !call || call->kind != ND_CALL ||
	    call->left || !call->name[0] || !call->returns_struct)
		return 0;
	if (call->aggregate_abi_class == AGGREGATE_ABI_INTREGS)
		return 0;

	count = count_list(call->args);
	arg_nodes = xcalloc((size_t)(count > 0 ? count : 1), sizeof(Node *));
	arg_slots = xcalloc((size_t)(count > 0 ? count : 1), sizeof(int));
	{
		Node *arg = call->args;
		int idx = 0;
		for (; arg; arg = arg->next)
			arg_nodes[idx++] = arg;
	}

	for (int i = 0; i < count; i++) {
		Type *arg_type = arg_nodes[i] ? arg_nodes[i]->type : NULL;
		int aggregate_regs = 0;
		int hfa_elem_size;
		int hfa_elem_count;

		if (arg_type && type_is_struct(arg_type) &&
		    parser_classify_aggregate_abi(arg_type, &aggregate_regs) == AGGREGATE_ABI_INTREGS) {
			arg_slots[i] = aggregate_regs;
		} else if (emit_arm64_hfa_info(arg_type, &hfa_elem_size, &hfa_elem_count)) {
			has_fp_param = 1;
			arg_slots[i] = hfa_elem_count;
		} else if (emit_type_is_floating_scalar(arg_type)) {
			has_fp_param = 1;
			arg_slots[i] = 1;
		} else {
			arg_slots[i] = 1;
		}
		total_slots += arg_slots[i];
	}
	if (!has_fp_param) {
		xfree(arg_slots);
		xfree(arg_nodes);
		return 0;
	}

	for (int j = count - 1; j >= 0; j--)
		emit_push_call_arg(arg_nodes[j], cg);

	for (int i = 0, stack_slot = 0; i < count; i++) {
		Type *arg_type = arg_nodes[i] ? arg_nodes[i]->type : NULL;
		int size = (arg_type && arg_type->size) ? arg_type->size : 8;
		int aggregate_regs = 0;
		int hfa_elem_size;
		int hfa_elem_count;

		if (arg_type && type_is_struct(arg_type) &&
		    parser_classify_aggregate_abi(arg_type, &aggregate_regs) == AGGREGATE_ABI_INTREGS) {
			for (int j = 0; j < aggregate_regs; j++) {
				int src_off = (stack_slot + j) * 16;
				int dst_off = outgoing_stack_slots * 8;

				if (int_reg < 8) {
					printf("    ldr x%d, [sp, #%d]\n", int_reg, src_off);
					int_reg++;
				} else {
					printf("    ldr x9, [sp, #%d]\n", src_off);
					printf("    str x9, [sp, #%d]\n", dst_off);
					outgoing_stack_slots++;
				}
			}
		} else if (emit_arm64_hfa_info(arg_type, &hfa_elem_size, &hfa_elem_count)) {
			for (int j = 0; j < hfa_elem_count; j++) {
				int src_off = (stack_slot + j) * 16;
				int dst_off = outgoing_stack_slots * 8;
				char fp_kind = hfa_elem_size == 4 ? 's' : 'd';

				if (fp_reg < 8) {
					printf("    ldr %c%d, [sp, #%d]\n", fp_kind, fp_reg, src_off);
					fp_reg++;
				} else {
					printf("    ldr %c16, [sp, #%d]\n", fp_kind, src_off);
					printf("    str %c16, [sp, #%d]\n", fp_kind, dst_off);
					outgoing_stack_slots++;
				}
			}
		} else if (emit_type_is_floating_scalar(arg_type)) {
			char fp_kind = size == 4 ? 's' : 'd';
			int src_off = stack_slot * 16;
			int dst_off = outgoing_stack_slots * 8;

			if (fp_reg < 8) {
				printf("    ldr %c%d, [sp, #%d]\n", fp_kind, fp_reg, src_off);
				fp_reg++;
			} else {
				printf("    ldr %c16, [sp, #%d]\n", fp_kind, src_off);
				printf("    str %c16, [sp, #%d]\n", fp_kind, dst_off);
				outgoing_stack_slots++;
			}
		} else {
			int src_off = stack_slot * 16;
			int dst_off = outgoing_stack_slots * 8;

			if (int_reg < 8) {
				printf("    ldr x%d, [sp, #%d]\n", int_reg, src_off);
				int_reg++;
			} else {
				printf("    ldr x9, [sp, #%d]\n", src_off);
				printf("    str x9, [sp, #%d]\n", dst_off);
				outgoing_stack_slots++;
			}
		}
		stack_slot += arg_slots[i];
	}

	printf("    mov x8, x17\n");
	cg->emit_call(call->name);
	if (total_slots > 0)
		printf("    add sp, sp, #%d\n", total_slots * 16);
	xfree(arg_slots);
	xfree(arg_nodes);
	return 1;
}

static int
emit_arm64_direct_small_gpr_call(Node *node, Codegen *cg)
{
	Node **arg_nodes;
	Node *arg;
	FuncInfo *fi;
	Type **param_types;
	char **param_struct_names;
	int param_count;
	int count;
	int fixed_params;
	int reg_count;
	int overflow_count;
	int overflow_bytes;
	int i;

	if (!emit_target_is_arm64(cg) || !node || node->kind != ND_CALL || node->left ||
	    !node->name[0])
		return 0;
	emit_trace_call_abi("direct-small-gpr-enter", node);
	if (node->returns_struct &&
	    node->aggregate_abi_class == AGGREGATE_ABI_HFA)
		return 0;
	count = count_list(node->args);
	if (count <= 0)
		return 0;
	fi = find_func(node->name);
	param_types = fi ? fi->param_types : NULL;
	param_struct_names = fi ? fi->param_struct_names : NULL;
	param_count = fi ? fi->param_type_count : 0;

	fixed_params = func_fixed_params(node->name);
	if (fixed_params >= 0 && count > fixed_params)
		return 0;

	arg_nodes = xcalloc((size_t)count, sizeof(Node *));
	arg = node->args;
	for (i = 0; i < count; i++, arg = arg->next) {
		Type *abi_type = (i < param_count && param_types && param_types[i])
		               ? param_types[i]
		               : (arg ? arg->type : NULL);
		const char *abi_name = (i < param_count && param_struct_names && param_struct_names[i])
		                     ? param_struct_names[i]
		                     : NULL;
		arg_nodes[i] = arg;
		if (!emit_arm64_direct_gpr_arg_ok_typed(arg, abi_type, abi_name)) {
			xfree(arg_nodes);
			return 0;
		}
	}

	reg_count = count > 8 ? 8 : count;
	overflow_count = count - reg_count;
	overflow_bytes = overflow_count * 16;
	if (overflow_bytes > 0)
		cg->emit_stack_alloc(overflow_bytes);

	for (i = count - 1; i >= reg_count; i--) {
		emit_expr(arg_nodes[i], cg);
		printf("    str x0, [sp, #%d]\n", (i - reg_count) * 16);
	}

	for (i = reg_count - 1; i >= 1; i--) {
		emit_expr(arg_nodes[i], cg);
		cg->emit_push_acc();
	}

	emit_expr(arg_nodes[0], cg);
	for (i = 1; i < reg_count; i++)
		printf("    ldr x%d, [sp, #%d]\n", i, (i - 1) * 16);
	cg->emit_call(node->name);
	if (reg_count > 1)
		printf("    add sp, sp, #%d\n", (reg_count - 1) * 16);
	if (overflow_bytes > 0)
		printf("    add sp, sp, #%d\n", overflow_bytes);
	xfree(arg_nodes);
	return 1;
}

static int
emit_arm64_direct_small_variadic_gpr_call(Node *node, Codegen *cg)
{
	Node **arg_nodes;
	Node *arg;
	FuncInfo *fi;
	Type **param_types;
	char **param_struct_names;
	int param_count;
	int count;
	int fixed_params;
	int var_count;
	int var_bytes;
	int fixed_bytes;
	int total_bytes;
	int i;

	if (!emit_target_is_arm64(cg) || !node || node->kind != ND_CALL || node->left ||
	    !node->name[0])
		return 0;
	emit_trace_call_abi("direct-variadic-gpr-enter", node);
	if (node->returns_struct &&
	    node->aggregate_abi_class == AGGREGATE_ABI_HFA)
		return 0;

	count = count_list(node->args);
	if (count <= 0 || count > 8)
		return 0;

	fixed_params = func_fixed_params(node->name);
	if (fixed_params < 0 || count <= fixed_params || fixed_params > 8)
		return 0;
	fi = find_func(node->name);
	param_types = fi ? fi->param_types : NULL;
	param_struct_names = fi ? fi->param_struct_names : NULL;
	param_count = fi ? fi->param_type_count : 0;

	arg_nodes = xcalloc((size_t)count, sizeof(Node *));
	arg = node->args;
	for (i = 0; i < count; i++, arg = arg->next) {
		Type *abi_type = (i < param_count && param_types && param_types[i])
		               ? param_types[i]
		               : (arg ? arg->type : NULL);
		const char *abi_name = (i < param_count && param_struct_names && param_struct_names[i])
		                     ? param_struct_names[i]
		                     : NULL;
		arg_nodes[i] = arg;
		if (!emit_arm64_direct_gpr_arg_ok_typed(arg, abi_type, abi_name)) {
			xfree(arg_nodes);
			return 0;
		}
	}

	var_count = count - fixed_params;
	var_bytes = ((var_count * 8) + 15) & ~15;
	fixed_bytes = fixed_params * 8;
	total_bytes = (var_bytes + fixed_bytes + 15) & ~15;
	if (total_bytes > 0)
		cg->emit_stack_alloc(total_bytes);

	for (i = count - 1; i >= fixed_params; i--) {
		emit_expr(arg_nodes[i], cg);
		printf("    str x0, [sp, #%d]\n", (i - fixed_params) * 8);
	}

	for (i = fixed_params - 1; i >= 0; i--) {
		emit_expr(arg_nodes[i], cg);
		printf("    str x0, [sp, #%d]\n", var_bytes + i * 8);
	}

	for (i = 1; i < fixed_params; i++)
		printf("    ldr x%d, [sp, #%d]\n", i, var_bytes + i * 8);
	if (fixed_params > 0)
		printf("    ldr x0, [sp, #%d]\n", var_bytes);

	cg->emit_call(node->name);
	if (total_bytes > 0)
		printf("    add sp, sp, #%d\n", total_bytes);
	xfree(arg_nodes);
	return 1;
}

static int
emit_arm64_fixed_fp_call(Node *node, Codegen *cg)
{
	Node **arg_nodes;
	int *arg_slots;
	FuncInfo *fi;
	Type *func_type = NULL;
	Type **param_types;
	char **param_struct_names;
	int param_count;
	int count;
	int fixed_params;
	int is_variadic = 0;
	int has_prototype = 0;
	int int_reg = 0;
	int fp_reg = 0;
	int has_fp_param = 0;
	int needs_hfa_return = 0;
	int total_slots = 0;
	int outgoing_stack_slots = 0;
	if (!emit_target_is_arm64(cg) || !node || node->kind != ND_CALL)
		return 0;
	emit_trace_call_abi("fixed-fp-enter", node);

	if (node->returns_struct && node->aggregate_abi_class == AGGREGATE_ABI_HFA)
		needs_hfa_return = 1;

	count = count_list(node->args);
	fixed_params = -1;
	param_types = NULL;
	param_struct_names = NULL;
	param_count = 0;
	fi = NULL;

	if (!node->left && node->name[0]) {
		fixed_params = func_fixed_params(node->name);
		if (fixed_params >= 0 && count > fixed_params)
			return 0;
		fi = find_func(node->name);
		param_types = fi ? fi->param_types : NULL;
		param_struct_names = fi ? fi->param_struct_names : NULL;
		param_count = fi ? fi->param_type_count : 0;
	} else if (node->left &&
	           node->left->type &&
	           type_is_pointer(node->left->type) &&
	           node->left->type->base &&
	           type_is_function(node->left->type->base)) {
		int fixed_param_count = 0;
		func_type = node->left->type->base;
		has_prototype = type_func_metadata(func_type, &param_types, &param_count,
		                                   &is_variadic, &fixed_param_count);
		if (has_prototype) {
			fixed_params = is_variadic ? fixed_param_count : param_count;
			if (fixed_params >= 0 && count > fixed_params)
				return 0;
		}
	}
	arg_nodes = xcalloc((size_t)(count > 0 ? count : 1), sizeof(Node *));
	arg_slots = xcalloc((size_t)(count > 0 ? count : 1), sizeof(int));
	{
		Node *arg = node->args;
		int idx = 0;
		for (; arg; arg = arg->next)
			arg_nodes[idx++] = arg;
	}

	for (int i = 0; i < count; i++) {
		Type *arg_type = arg_nodes[i] ? arg_nodes[i]->type : NULL;
		Type *abi_type = (i < param_count && param_types && param_types[i])
		               ? param_types[i]
		               : arg_type;
		const char *abi_name = (i < param_count && param_struct_names && param_struct_names[i])
		                     ? param_struct_names[i]
		                     : NULL;
		int aggregate_regs = 0;
		int hfa_elem_size;
		int hfa_elem_count;

		if (abi_type && type_is_struct(abi_type) &&
		    parser_classify_aggregate_abi(abi_type, &aggregate_regs) == AGGREGATE_ABI_INTREGS) {
			arg_slots[i] = aggregate_regs;
		} else if (emit_arm64_hfa_info(abi_type, &hfa_elem_size, &hfa_elem_count) ||
		           parser_arm64_hfa_info_name(abi_name, &hfa_elem_size, &hfa_elem_count)) {
			has_fp_param = 1;
			arg_slots[i] = hfa_elem_count;
		} else if (emit_type_is_floating_scalar(abi_type)) {
			has_fp_param = 1;
			arg_slots[i] = 1;
		} else {
			arg_slots[i] = 1;
		}
		total_slots += arg_slots[i];
	}
	if (!has_fp_param && !needs_hfa_return) {
		xfree(arg_slots);
		xfree(arg_nodes);
		return 0;
	}

	for (int j = count - 1; j >= 0; j--) {
		Type *arg_type = arg_nodes[j] ? arg_nodes[j]->type : NULL;
		Type *abi_type = (j < param_count && param_types && param_types[j])
		               ? param_types[j]
		               : arg_type;
		const char *abi_name = (j < param_count && param_struct_names && param_struct_names[j])
		                     ? param_struct_names[j]
		                     : NULL;
		emit_push_call_arg_typed(arg_nodes[j], abi_type, abi_name, cg);
	}

	if (node->left) {
		emit_expr(node->left, cg);
		cg->emit_acc_to_saved();
	}

	for (int i = 0, stack_slot = 0; i < count; i++) {
		Type *arg_type = arg_nodes[i] ? arg_nodes[i]->type : NULL;
		Type *abi_type = (i < param_count && param_types && param_types[i])
		               ? param_types[i]
		               : arg_type;
		const char *abi_name = (i < param_count && param_struct_names && param_struct_names[i])
		                     ? param_struct_names[i]
		                     : NULL;
		int size = (arg_type && arg_type->size) ? arg_type->size : 8;
		int aggregate_regs = 0;
		int hfa_elem_size;
		int hfa_elem_count;

		if (abi_type && type_is_struct(abi_type) &&
		    parser_classify_aggregate_abi(abi_type, &aggregate_regs) == AGGREGATE_ABI_INTREGS) {
			for (int j = 0; j < aggregate_regs; j++) {
				int src_off = (stack_slot + j) * 16;
				int dst_off = outgoing_stack_slots * 8;

				if (int_reg < 8) {
					printf("    ldr x%d, [sp, #%d]\n", int_reg, src_off);
					int_reg++;
				} else {
					printf("    ldr x9, [sp, #%d]\n", src_off);
					printf("    str x9, [sp, #%d]\n", dst_off);
					outgoing_stack_slots++;
				}
			}
		} else if (emit_arm64_hfa_info(abi_type, &hfa_elem_size, &hfa_elem_count) ||
		           parser_arm64_hfa_info_name(abi_name, &hfa_elem_size, &hfa_elem_count)) {
			int j;

			for (j = 0; j < hfa_elem_count; j++) {
				int src_off = (stack_slot + j) * 16;
				int dst_off = outgoing_stack_slots * 8;
				char fp_kind = hfa_elem_size == 4 ? 's' : 'd';

				if (fp_reg < 8) {
					printf("    ldr %c%d, [sp, #%d]\n",
					       fp_kind, fp_reg, src_off);
					fp_reg++;
				} else {
					printf("    ldr %c16, [sp, #%d]\n", fp_kind, src_off);
					printf("    str %c16, [sp, #%d]\n", fp_kind, dst_off);
					outgoing_stack_slots++;
				}
			}
		} else if (emit_type_is_floating_scalar(abi_type)) {
			char fp_kind = size == 4 ? 's' : 'd';
			int src_off = stack_slot * 16;
			int dst_off = outgoing_stack_slots * 8;

			if (fp_reg < 8) {
				printf("    ldr %c%d, [sp, #%d]\n",
				       fp_kind, fp_reg, src_off);
				fp_reg++;
			} else {
				printf("    ldr %c16, [sp, #%d]\n", fp_kind, src_off);
				printf("    str %c16, [sp, #%d]\n", fp_kind, dst_off);
				outgoing_stack_slots++;
			}
		} else {
			int src_off = stack_slot * 16;
			int dst_off = outgoing_stack_slots * 8;

			if (int_reg < 8) {
				printf("    ldr x%d, [sp, #%d]\n", int_reg, src_off);
				int_reg++;
			} else {
				printf("    ldr x9, [sp, #%d]\n", src_off);
				printf("    str x9, [sp, #%d]\n", dst_off);
				outgoing_stack_slots++;
			}
		}
		stack_slot += arg_slots[i];
	}

	if (node->left) {
		cg->emit_call_saved();
	} else {
		cg->emit_call(node->name);
	}
	if (total_slots > 0)
		printf("    add sp, sp, #%d\n", total_slots * 16);
	xfree(arg_slots);
	xfree(arg_nodes);
	return 1;
}

static int
emit_builtin_stack_call_expr(Node *node, Codegen *cg)
{
	if (!node || node->kind != ND_CALL || node->left || !node->name[0])
		return 0;

	if (STRCMP(node->name, "__builtin_stack_save") == 0) {
		if (count_list(node->args) != 0)
			ICE("__builtin_stack_save expects no arguments");
		cg->emit_stack_save_acc();
		return 1;
	}

	if (STRCMP(node->name, "__builtin_stack_alloc") == 0) {
		if (count_list(node->args) != 1)
			ICE("__builtin_stack_alloc expects one argument");
		emit_expr(node->args, cg);
		cg->emit_stack_alloc_acc();
		return 1;
	}

	return 0;
}

static int
emit_builtin_stack_call_stmt(Node *node, Codegen *cg)
{
	if (!node || node->kind != ND_CALL || node->left || !node->name[0])
		return 0;

	if (STRCMP(node->name, "__builtin_stack_restore") == 0) {
		if (count_list(node->args) != 1)
			ICE("__builtin_stack_restore expects one argument");
		emit_expr(node->args, cg);
		cg->emit_stack_restore_acc();
		return 1;
	}

	if (emit_builtin_stack_call_expr(node, cg))
		return 1;

	return 0;
}

static unsigned long
bitfield_low_mask(int width)
{
	if (width <= 0)
		return 0UL;
	if (width >= (int)(sizeof(unsigned long) * 8))
		return ~0UL;
	return ((1UL << width) - 1UL);
}

static void
emit_apply_bitfield_extract(Node *node, Codegen *cg)
{
	int storage_bits;
	unsigned long storage_mask;
	unsigned long field_mask;
	int sign_shift;

	if (!node || !node->is_bitfield)
		return;

	storage_bits = (node->bit_storage_size ? node->bit_storage_size : node->elem_size) * 8;
	storage_mask = bitfield_low_mask(storage_bits);
	field_mask = bitfield_low_mask(node->bit_width);

	if (storage_bits < (int)(sizeof(unsigned long) * 8)) {
		cg->emit_acc_to_tmp();
		cg->emit_load_imm((long)storage_mask);
		cg->emit_bitand();
	}

	if (node->bit_offset > 0) {
		cg->emit_acc_to_tmp();
		cg->emit_load_imm(node->bit_offset);
		if (node->is_unsigned && cg->emit_ushr)
			cg->emit_ushr();
		else
			cg->emit_shr();
	}

	if (node->bit_width < storage_bits) {
		cg->emit_acc_to_tmp();
		cg->emit_load_imm((long)field_mask);
		cg->emit_bitand();
	}

	if (!node->is_unsigned && node->bit_width > 0 && node->bit_width < storage_bits) {
		sign_shift = storage_bits - node->bit_width;
		cg->emit_acc_to_tmp();
		cg->emit_load_imm(sign_shift);
		cg->emit_shl();
		cg->emit_acc_to_tmp();
		cg->emit_load_imm(sign_shift);
		cg->emit_shr();
	}
}

static void
emit_assign_bitfield(Node *node, Codegen *cg)
{
	unsigned long field_mask;
	unsigned long shifted_mask;
	unsigned long clear_mask;
	int storage_size;

	field_mask = bitfield_low_mask(node->left->bit_width);
	shifted_mask = node->left->bit_offset >= (int)(sizeof(unsigned long) * 8)
		? 0UL
		: (field_mask << node->left->bit_offset);
	clear_mask = ~shifted_mask;
	storage_size = node->left->bit_storage_size ? node->left->bit_storage_size : node->left->elem_size;

	if (node->left->kind == ND_MEMBER_PTR) {
		emit_expr(node->left->left, cg);
		if (node->left->offset)
			cg->emit_add_offset(node->left->offset);
		cg->emit_acc_to_saved();

		emit_expr(node->right, cg);
		cg->emit_acc_to_tmp();
		cg->emit_load_imm((long)field_mask);
		cg->emit_bitand();
		cg->emit_push_acc();

		cg->emit_load_via_saved(storage_size);
		cg->emit_acc_to_tmp();
		cg->emit_load_imm((long)clear_mask);
		cg->emit_bitand();
		cg->emit_push_acc();

		cg->emit_pop_to_tmp();
		cg->emit_pop_to_acc();
		if (node->left->bit_offset > 0) {
			cg->emit_acc_to_tmp();
			cg->emit_load_imm(node->left->bit_offset);
			cg->emit_shl();
		}
		cg->emit_bitor();
		cg->emit_store_via_saved(storage_size);

		cg->emit_load_via_saved(storage_size);
		emit_apply_bitfield_extract(node->left, cg);
		return;
	}

	emit_expr(node->right, cg);
	cg->emit_acc_to_tmp();
	cg->emit_load_imm((long)field_mask);
	cg->emit_bitand();
	cg->emit_push_acc();

	cg->emit_load_local_sized(node->left->offset, storage_size);
	cg->emit_acc_to_tmp();
	cg->emit_load_imm((long)clear_mask);
	cg->emit_bitand();
	cg->emit_push_acc();

	cg->emit_pop_to_tmp();
	cg->emit_pop_to_acc();
	if (node->left->bit_offset > 0) {
		cg->emit_acc_to_tmp();
		cg->emit_load_imm(node->left->bit_offset);
		cg->emit_shl();
	}
	cg->emit_bitor();
	cg->emit_store_local_sized(node->left->offset, storage_size);
	cg->emit_load_local_sized(node->left->offset, storage_size);
	emit_apply_bitfield_extract(node->left, cg);
}

static void
emit_binary_operands(Node *node, Codegen *cg)
{
	/*
	 * Binary backend helpers expect:
	 *   tmp = left operand
	 *   acc = right operand
	 *
	 * Do not keep the left operand only in the backend temporary register
	 * while evaluating the right side.  Some apparently simple rvalues, such
	 * as array indexing, use that same temporary internally for address/index
	 * calculations.  That clobbered the saved left operand in expressions like
	 *
	 *   y * 10 + a[0]
	 *
	 * where evaluating a[0] reused x64 r10 before the final add.  Saving the
	 * left operand on the real stack protects it across arbitrary right-hand
	 * expression codegen and restores it to tmp immediately before the operator.
	 */
	emit_expr(node->left, cg);
	cg->emit_push_acc();
	emit_expr(node->right, cg);
	cg->emit_pop_to_tmp();
}

static void
emit_global_index_addr(Node *node, Codegen *cg)
{
	int elem_size;

	if (!node || node->kind != ND_GLOBAL_INDEX)
		ICE("emit_global_index_addr expects ND_GLOBAL_INDEX");

	elem_size = node->is_array_field ? 1 : (node->elem_size ? node->elem_size : 1);
	cg->emit_load_func_addr(node->name);
	cg->emit_push_acc();
	emit_expr(node->left, cg);
	cg->emit_pop_to_tmp();
	cg->emit_ptr_add(elem_size);
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
emit_string_literals_walk(Node *node, Codegen *cg, int include_next)
{
	if (!node)
		return;

	for (Node *n = node; n; n = include_next ? n->next : NULL) {
		if (n->kind == ND_STRING)
			cg->emit_string_literal(n->string_label, n->string_value, n->string_len, n->string_width ? n->string_width : 1);

		emit_string_literals_walk(n->left, cg, 1);
		emit_string_literals_walk(n->right, cg, 1);
		emit_string_literals_walk(n->init, cg, 1);
		emit_string_literals_walk(n->cond, cg, 1);
		emit_string_literals_walk(n->inc, cg, 1);
		emit_string_literals_walk(n->then_body, cg, 1);
		emit_string_literals_walk(n->else_body, cg, 1);
		emit_string_literals_walk(n->body, cg, 1);
		emit_string_literals_walk(n->args, cg, 1);
	}
}

static void
emit_string_literals(Node *node, Codegen *cg)
{
	emit_string_literals_walk(node, cg, 1);
}

static void
emit_function_string_literals(Node *func, Codegen *cg)
{
	emit_string_literals_walk(func, cg, 0);
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
	/* For pointer nodes: elem_size = stride (1 for char*), but load/store = 8 bytes */
	int stride = node->left->elem_size ? node->left->elem_size : 4;
	int size = node->left->is_pointer ? 8 : stride;

	if (emit_arm64_try_emit_local_incdec(node, cg, is_inc, is_postfix))
		return;

	if (node->left->is_bitfield &&
	    (node->left->kind == ND_MEMBER || node->left->kind == ND_MEMBER_PTR)) {
		unsigned long field_mask = bitfield_low_mask(node->left->bit_width);
		unsigned long shifted_mask = node->left->bit_offset >= (int)(sizeof(unsigned long) * 8)
			? 0UL
			: (field_mask << node->left->bit_offset);
		unsigned long clear_mask = ~shifted_mask;
		int storage_size = node->left->bit_storage_size ? node->left->bit_storage_size : node->left->elem_size;

		if (node->left->kind == ND_MEMBER) {
			cg->emit_addr_local(node->left->offset);
		} else {
			emit_expr(node->left->left, cg);
			if (node->left->offset)
				cg->emit_add_offset(node->left->offset);
		}
		cg->emit_acc_to_saved();

		cg->emit_load_via_saved(storage_size);
		emit_apply_bitfield_extract(node->left, cg);

		if (is_postfix)
			cg->emit_push_acc();

		cg->emit_push_acc();
		cg->emit_load_imm(1);
		cg->emit_pop_to_tmp();
		if (is_inc)
			cg->emit_add();
		else
			cg->emit_sub();

		cg->emit_push_acc();
		cg->emit_load_imm((long)field_mask);
		cg->emit_pop_to_tmp();
		cg->emit_bitand();

		if (node->left->bit_offset > 0) {
			cg->emit_acc_to_tmp();
			cg->emit_load_imm(node->left->bit_offset);
			cg->emit_shl();
		}

		cg->emit_push_acc();
		cg->emit_load_via_saved(storage_size);
		cg->emit_push_acc();
		cg->emit_load_imm((long)clear_mask);
		cg->emit_pop_to_tmp();
		cg->emit_bitand();
		cg->emit_pop_to_tmp();
		cg->emit_bitor();
		cg->emit_store_via_saved(storage_size);

		if (is_postfix) {
			cg->emit_pop_to_acc();
		} else {
			cg->emit_load_via_saved(storage_size);
			emit_apply_bitfield_extract(node->left, cg);
		}
		return;
	}

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

	if (node->left->kind == ND_DEREF && cg->emit_incdec_deref) {
		emit_expr(node->left->left, cg);
		cg->emit_incdec_deref(size, is_inc, is_postfix);
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

	case ND_COND:
		/* Conditional expression returning struct — use then-branch address.
		 * ND_COND uses then_body/else_body, not left/right. */
		if (node->then_body)
			emit_struct_lvalue_addr(node->then_body, cg);
		else if (node->else_body)
			emit_struct_lvalue_addr(node->else_body, cg);
		return;

	case ND_COMMA:
		/* Struct temporaries are normalized as `(tmp = expr, tmp)` in a few
		 * parser/statement lowering paths. Preserve the setup side effect,
		 * then take the address of the resulting temp expression.
		 */
		emit_expr(node->left, cg);
		emit_struct_lvalue_addr(node->right, cg);
		return;

	default:
		node_error_at(node, "Struct return requires addressable struct expression kind %s",
		              node_kind_name(node->kind));
	}
}


static int
node_can_yield_expression_value(Node *node)
{
	if (!node)
		return 0;
	if (node->kind <= ND_STRUCT_ASSIGN)
		return 1;
	if (node->kind >= ND_DECL && node->kind <= ND_STRUCT_DECL)
		return 0;
	if (node->kind >= ND_RETURN && node->kind <= ND_CONTINUE)
		return 0;
	if (node->kind == ND_ASM)
		return 0;
	return 1;
}

static void
emit_block_expr(Node *block, Codegen *cg)
{
	Node *last = NULL;

	if (!block || !block->body) {
		cg->emit_load_imm(0);
		return;
	}

	for (Node *stmt = block->body; stmt; stmt = stmt->next)
		last = stmt;

	for (Node *stmt = block->body; stmt && stmt != last; stmt = stmt->next)
		emit_statement(stmt, cg);

	if (node_can_yield_expression_value(last)) {
		emit_expr(last, cg);
	} else {
		emit_statement(last, cg);
		cg->emit_load_imm(0);
	}
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
		if (emit_target_is_arm64(cg) && node->is_fp_num) {
			unsigned long long bits = emit_fp_literal_bits(node);
			cg->emit_load_imm((long)bits);
			if (node->type && node->type->kind == TY_FLOAT)
				printf("    fmov s0, w0\n");
			else
				printf("    fmov d0, x0\n");
			return;
		}
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
		if (emit_target_is_arm64(cg) && emit_expr_is_floating(node)) {
			emit_arm64_fp_load_local(node, cg);
			return;
		}
		/* C array-to-pointer decay: a local array used as an rvalue yields
		 * the address of its first element, not the scalar stored at [array].
		 * This is the real fix for calls such as token_set_text(text), where
		 * text is a local char[].
		 */
		if (node->type && node->type->kind == TY_ARRAY) {
			cg->emit_addr_local(node->offset);
			return;
		}
		if (node->is_pointer)
			cg->emit_load_ptr_local(node->offset);
		else
			cg->emit_load_local_sized(node->offset, node->elem_size ? node->elem_size : TCC_SIZEOF_INT);
		emit_apply_integer_load_cast(node, cg);
		return;

	case ND_GLOBAL:
		if (emit_target_is_arm64(cg) && emit_expr_is_floating(node)) {
			cg->emit_load_func_addr(node->name);
			emit_arm64_fp_load_addr("x0", node->type->size);
			return;
		}
		cg->emit_load_global(node->name, node->is_pointer ? 8 : (node->elem_size ? node->elem_size : TCC_SIZEOF_INT));
		emit_apply_integer_load_cast(node, cg);
		return;

	case ND_GLOBAL_INDEX:
		emit_global_index_addr(node, cg);
		if (!node->is_array_field) {
			cg->emit_load_deref(node->is_pointer ? 8 : (node->elem_size ? node->elem_size : TCC_SIZEOF_INT));
			emit_apply_integer_load_cast(node, cg);
		}
		return;

	case ND_MEMBER:
		if (emit_target_is_arm64(cg) && emit_expr_is_floating(node)) {
			emit_arm64_fp_load_local(node, cg);
			return;
		}
		if (node->is_array_field) {
			/* Local struct array member used as an expression decays to &member[0]. */
			cg->emit_addr_local(node->offset);
		} else {
			cg->emit_load_local_sized(node->offset,
			                          node->is_bitfield ? node->bit_storage_size :
			                          (node->elem_size ? node->elem_size : TCC_SIZEOF_INT));
			if (node->is_bitfield)
				emit_apply_bitfield_extract(node, cg);
			else
				emit_apply_integer_load_cast(node, cg);
		}
		return;

	case ND_MEMBER_PTR:
		if (emit_target_is_arm64(cg) && emit_expr_is_floating(node)) {
			int size = node->type && node->type->size ? node->type->size : 8;
			emit_expr(node->left, cg);
			if (node->offset)
				printf("    add x0, x0, #%d\n", node->offset);
			if (size == 4) {
				printf("    ldr w0, [x0]\n");
				printf("    fmov s0, w0\n");
			} else {
				printf("    ldr x0, [x0]\n");
				printf("    fmov d0, x0\n");
			}
			return;
		}
		emit_expr(node->left, cg);
		if (node->is_array_field) {
			/* Array field decays to pointer: emit address (base + offset) */
			if (node->offset)
				cg->emit_add_offset(node->offset);
			/* else: address is already in x0 (base ptr, offset=0) */
		} else {
			cg->emit_load_member_ptr(node->offset,
			                         node->is_bitfield ? node->bit_storage_size :
			                         (node->elem_size ? node->elem_size : TCC_SIZEOF_INT));
			if (node->is_bitfield)
				emit_apply_bitfield_extract(node, cg);
			else
				emit_apply_integer_load_cast(node, cg);
		}
		return;

	case ND_INDEX:
		if (emit_arm64_try_emit_const_local_index_load(node, cg))
			return;
		emit_expr(node->left, cg);
		cg->emit_load_indexed(node->offset, node->elem_size);
		emit_apply_integer_load_cast(node, cg);
		return;

	case ND_ADDR:
		if (node->left->kind == ND_VAR) {
			cg->emit_addr_local(node->left->offset);
			return;
		}

		if (node->left->kind == ND_GLOBAL) {
			/* &global yields the symbol address, not the contents stored at it. */
			cg->emit_load_func_addr(node->left->name);
			return;
		}

		if (node->left->kind == ND_MEMBER) {
			cg->emit_addr_local(node->left->offset);
			return;
		}

	if (node->left->kind == ND_GLOBAL_INDEX) {
		emit_global_index_addr(node->left, cg);
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

	if (node->left->kind == ND_COMMA && node->left->right) {
		emit_expr(node->left->left, cg);
		emit_expr(new_addr(node->left->right), cg);
		return;
	}

	node_error_at(node,
	              "Cannot take address of expression "
		              "(kind=%s left=%s right=%s name=%s)",
		              node_kind_name(node->kind),
		              node->left  ? node_kind_name(node->left->kind)  : "<null>",
		              node->right ? node_kind_name(node->right->kind) : "<null>",
		              node->name[0] ? node->name : "<empty>");

	case ND_DEREF:
		if (emit_target_is_arm64(cg) && emit_expr_is_floating(node)) {
			emit_arm64_fp_lvalue_addr(node, cg);
			emit_arm64_fp_load_addr("x0", node->type->size);
			return;
		}
		if (emit_arm64_try_emit_ptr_add_deref_load(node, cg))
			return;
		emit_expr(node->left, cg);
		/* (*fptr)() == fptr(): skip load if left is a call returning a pointer */
		if (node->left && node->left->kind == ND_CALL && node->left->is_pointer)
			return;
		/* For pointer-typed deref nodes (e.g. *pp where pp is char**),
		 * is_pointer=1 and elem_size=1 (the char stride). The load width
		 * must be the pointer size (8), not the stride. */
		cg->emit_load_deref(node->is_pointer ? 8 : (node->elem_size ? node->elem_size : TCC_SIZEOF_INT));
		emit_apply_integer_load_cast(node, cg);
		return;

	case ND_NEG:
		if (emit_target_is_arm64(cg) && emit_expr_is_floating(node)) {
			emit_expr(node->left, cg);
			printf("    fneg %c0, %c0\n",
			       node->type && node->type->size == 4 ? 's' : 'd',
			       node->type && node->type->size == 4 ? 's' : 'd');
			return;
		}
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
		if (emit_arm64_try_emit_immediate_binary(node, cg))
			return;
		if (emit_target_is_arm64(cg) && emit_expr_is_floating(node)) {
			int size = node->type ? node->type->size : 8;
			emit_expr(node->left, cg);
			emit_arm64_fp_push(size);
			emit_expr(node->right, cg);
			emit_arm64_fp_pop_to_tmp(size);
			printf("    fadd %c0, %c1, %c0\n",
			       size == 4 ? 's' : 'd',
			       size == 4 ? 's' : 'd',
			       size == 4 ? 's' : 'd');
			return;
		}
		emit_binary_operands(node, cg);
		if (node->is_pointer)
			cg->emit_ptr_add(node->elem_size);
		else
			cg->emit_add();
		return;

	case ND_SUB:
		if (emit_arm64_try_emit_immediate_binary(node, cg))
			return;
		if (emit_target_is_arm64(cg) && emit_expr_is_floating(node)) {
			int size = node->type ? node->type->size : 8;
			emit_expr(node->left, cg);
			emit_arm64_fp_push(size);
			emit_expr(node->right, cg);
			emit_arm64_fp_pop_to_tmp(size);
			printf("    fsub %c0, %c1, %c0\n",
			       size == 4 ? 's' : 'd',
			       size == 4 ? 's' : 'd',
			       size == 4 ? 's' : 'd');
			return;
		}
		emit_binary_operands(node, cg);
		if (node->is_pointer)
			cg->emit_ptr_sub(node->elem_size);
		else
			cg->emit_sub();
		return;

	case ND_MUL:
		if (emit_target_is_arm64(cg) && emit_expr_is_floating(node)) {
			int size = node->type ? node->type->size : 8;
			emit_expr(node->left, cg);
			emit_arm64_fp_push(size);
			emit_expr(node->right, cg);
			emit_arm64_fp_pop_to_tmp(size);
			printf("    fmul %c0, %c1, %c0\n",
			       size == 4 ? 's' : 'd',
			       size == 4 ? 's' : 'd',
			       size == 4 ? 's' : 'd');
			return;
		}
		emit_binary_operands(node, cg);
		cg->emit_mul();
		return;

	case ND_DIV:
		if (emit_target_is_arm64(cg) && emit_expr_is_floating(node)) {
			int size = node->type ? node->type->size : 8;
			emit_expr(node->left, cg);
			emit_arm64_fp_push(size);
			emit_expr(node->right, cg);
			emit_arm64_fp_pop_to_tmp(size);
			printf("    fdiv %c0, %c1, %c0\n",
			       size == 4 ? 's' : 'd',
			       size == 4 ? 's' : 'd',
			       size == 4 ? 's' : 'd');
			return;
		}
		emit_binary_operands(node, cg);
		cg->emit_div();
		return;

	case ND_MOD:
		emit_binary_operands(node, cg);
		cg->emit_mod();
		return;

	case ND_BITAND:
		if (emit_arm64_try_emit_immediate_binary(node, cg))
			return;
		emit_binary_operands(node, cg);
		cg->emit_bitand();
		return;

	case ND_BITOR:
		emit_binary_operands(node, cg);
		cg->emit_bitor();
		return;

	case ND_BITNOT:
		emit_expr(node->left, cg);
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
	{
		int dst_size = node->type ? node->type->size : 4;
		int dst_unsigned = node->type ? type_is_unsigned(node->type) : 0;
		int src_size = (node->left && node->left->type) ? node->left->type->size : dst_size;
		int src_unsigned = (node->left && node->left->type) ? type_is_unsigned(node->left->type) : dst_unsigned;

		if (emit_target_is_arm64(cg) &&
		    ((node->type && type_is_fp_scalar(node->type)) ||
		     (node->left && node->left->type && type_is_fp_scalar(node->left->type)))) {
			emit_expr(node->left, cg);

			if (node->type && type_is_fp_scalar(node->type)) {
				if (node->left && node->left->type && type_is_fp_scalar(node->left->type)) {
					if (dst_size == src_size)
						return;
					printf("    fcvt %c0, %c0\n", dst_size == 4 ? 's' : 'd',
					       src_size == 4 ? 's' : 'd');
					return;
				}

				if (dst_unsigned)
					printf("    ucvtf %c0, x0\n", dst_size == 4 ? 's' : 'd');
				else
					printf("    scvtf %c0, x0\n", dst_size == 4 ? 's' : 'd');
				return;
			}

			if (src_unsigned)
				printf("    fcvtzu x0, %c0\n", src_size == 4 ? 's' : 'd');
			else
				printf("    fcvtzs x0, %c0\n", src_size == 4 ? 's' : 'd');
			if (dst_size < 8)
				cg->emit_cast(dst_size, dst_unsigned);
			return;
		}

		emit_expr(node->left, cg);

		/* Match IR lowering: widening casts extend according to the
		 * source width/signedness, not the destination width. */
		if (dst_size > src_size && src_size < 8)
			cg->emit_cast(src_size, src_unsigned);
		else
			cg->emit_cast(dst_size, dst_unsigned);
		return;
	}

	case ND_EQ:
		if (emit_arm64_try_emit_immediate_compare(node, cg))
			return;
		if (emit_target_is_arm64(cg) && node->left && node->right &&
		    emit_expr_is_floating(node->left) && emit_expr_is_floating(node->right)) {
			int size = node->left->type ? node->left->type->size : 8;
			emit_expr(node->left, cg);
			emit_arm64_fp_push(size);
			emit_expr(node->right, cg);
			emit_arm64_fp_pop_to_tmp(size);
			printf("    fcmp %c1, %c0\n", size == 4 ? 's' : 'd', size == 4 ? 's' : 'd');
			printf("    cset w0, eq\n");
			return;
		}
		emit_binary_operands(node, cg);
		cg->emit_cmp_eq();
		return;

	case ND_NE:
		if (emit_arm64_try_emit_immediate_compare(node, cg))
			return;
		if (emit_target_is_arm64(cg) && node->left && node->right &&
		    emit_expr_is_floating(node->left) && emit_expr_is_floating(node->right)) {
			int size = node->left->type ? node->left->type->size : 8;
			emit_expr(node->left, cg);
			emit_arm64_fp_push(size);
			emit_expr(node->right, cg);
			emit_arm64_fp_pop_to_tmp(size);
			printf("    fcmp %c1, %c0\n", size == 4 ? 's' : 'd', size == 4 ? 's' : 'd');
			printf("    cset w0, ne\n");
			return;
		}
		emit_binary_operands(node, cg);
		cg->emit_cmp_ne();
		return;

	case ND_LT:
		if (emit_target_is_arm64(cg) && node->left && node->right &&
		    emit_expr_is_floating(node->left) && emit_expr_is_floating(node->right)) {
			int size = node->left->type ? node->left->type->size : 8;
			emit_expr(node->left, cg);
			emit_arm64_fp_push(size);
			emit_expr(node->right, cg);
			emit_arm64_fp_pop_to_tmp(size);
			printf("    fcmp %c1, %c0\n", size == 4 ? 's' : 'd', size == 4 ? 's' : 'd');
			printf("    cset w0, lt\n");
			return;
		}
		emit_binary_operands(node, cg);
		if (emit_compare_is_unsigned(node) && cg->emit_cmp_lt_u)
			cg->emit_cmp_lt_u();
		else
			cg->emit_cmp_lt();
		return;

	case ND_LE:
		if (emit_target_is_arm64(cg) && node->left && node->right &&
		    emit_expr_is_floating(node->left) && emit_expr_is_floating(node->right)) {
			int size = node->left->type ? node->left->type->size : 8;
			emit_expr(node->left, cg);
			emit_arm64_fp_push(size);
			emit_expr(node->right, cg);
			emit_arm64_fp_pop_to_tmp(size);
			printf("    fcmp %c1, %c0\n", size == 4 ? 's' : 'd', size == 4 ? 's' : 'd');
			printf("    cset w0, le\n");
			return;
		}
		emit_binary_operands(node, cg);
		if (emit_compare_is_unsigned(node) && cg->emit_cmp_le_u)
			cg->emit_cmp_le_u();
		else
			cg->emit_cmp_le();
		return;

	case ND_GT:
		if (emit_target_is_arm64(cg) && node->left && node->right &&
		    emit_expr_is_floating(node->left) && emit_expr_is_floating(node->right)) {
			int size = node->left->type ? node->left->type->size : 8;
			emit_expr(node->left, cg);
			emit_arm64_fp_push(size);
			emit_expr(node->right, cg);
			emit_arm64_fp_pop_to_tmp(size);
			printf("    fcmp %c1, %c0\n", size == 4 ? 's' : 'd', size == 4 ? 's' : 'd');
			printf("    cset w0, gt\n");
			return;
		}
		emit_binary_operands(node, cg);
		if (emit_compare_is_unsigned(node) && cg->emit_cmp_gt_u)
			cg->emit_cmp_gt_u();
		else
			cg->emit_cmp_gt();
		return;

	case ND_GE:
		if (emit_target_is_arm64(cg) && node->left && node->right &&
		    emit_expr_is_floating(node->left) && emit_expr_is_floating(node->right)) {
			int size = node->left->type ? node->left->type->size : 8;
			emit_expr(node->left, cg);
			emit_arm64_fp_push(size);
			emit_expr(node->right, cg);
			emit_arm64_fp_pop_to_tmp(size);
			printf("    fcmp %c1, %c0\n", size == 4 ? 's' : 'd', size == 4 ? 's' : 'd');
			printf("    cset w0, ge\n");
			return;
		}
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
		if (node->cond && node->cond->kind == ND_NUM) {
			emit_expr(node->cond->value ? node->then_body : node->else_body, cg);
			return;
		}

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
		if (!node->left || !node->right)
			ICE("Struct assignment requires both operands");
		if (emit_target_is_arm64(cg)) {
			if (node->right->kind == ND_COND &&
			    node->right->cond &&
			    node->right->then_body &&
			    node->right->else_body) {
				int else_label = new_label();
				int end_label = new_label();

				emit_expr(node->right->cond, cg);
				cg->emit_branch_if_zero(else_label);

				emit_struct_lvalue_addr(node->left, cg);
				cg->emit_push_acc();
				emit_struct_lvalue_addr(node->right->then_body, cg);
				cg->emit_pop_to_tmp();
				printf("    add x17, x1, #0\n");
				cg->emit_ptr_copy(node->value);
				cg->emit_branch(end_label);

				cg->emit_label(else_label);
				emit_struct_lvalue_addr(node->left, cg);
				cg->emit_push_acc();
				emit_struct_lvalue_addr(node->right->else_body, cg);
				cg->emit_pop_to_tmp();
				printf("    add x17, x1, #0\n");
				cg->emit_ptr_copy(node->value);

				cg->emit_label(end_label);
				return;
			}
			if (node->right->kind == ND_CALL &&
			    node->right->returns_struct &&
			    node->right->aggregate_abi_class == AGGREGATE_ABI_HFA) {
				if (emit_arm64_fixed_fp_call(node->right, cg)) {
					emit_arm64_store_hfa_return_lvalue(node->left, node->right->type, cg);
					return;
				}
				ICE("arm64 HFA struct assign requires fixed-fp call lowering");
			}
			if (node->right->kind == ND_CALL &&
			    node->right->returns_struct &&
			    node->right->aggregate_abi_class == AGGREGATE_ABI_INTREGS) {
				if (emit_arm64_direct_intreg_aggregate_call(node->right, cg) ||
				    emit_arm64_fixed_fp_call(node->right, cg) ||
				    emit_arm64_direct_small_variadic_gpr_call(node->right, cg) ||
				    emit_arm64_direct_small_gpr_call(node->right, cg)) {
					if (!emit_arm64_store_intreg_aggregate_return_local(node->left,
					                                                    node->right, cg))
						ICE("arm64 INTREGS struct assign requires local destination");
					return;
				}
				ICE("arm64 INTREGS struct assign requires direct call lowering");
			}
			if (node->right->kind == ND_CALL &&
			    node->right->returns_struct &&
			    emit_arm64_fixed_fp_sret_call_to_saved(node->right, cg)) {
				return;
			}
			if (node->right->kind == ND_CALL &&
			    node->right->returns_struct &&
			    emit_arm64_direct_sret_small_gpr_call_to_saved(node->right, cg)) {
				return;
			}
			if (node->right->kind == ND_CALL &&
			    node->right->returns_struct) {
				int count = count_list(node->right->args);
				int cleanup_words;
				int arm64_arg_slots;

				if (count > 8)
					ICE("Only up to 8 function arguments are supported by the current call ABI");
				cleanup_words = emit_push_call_args_reverse(node->right->args, cg);
				arm64_arg_slots = cleanup_words;
				cg->emit_prepare_call_args(count, -1);
				printf("    mov x8, x17\n");
				cg->emit_call(node->right->name);
				cg->emit_cleanup_call_args(arm64_arg_slots, -1);
				(void)cleanup_words;
				return;
			}
			if (node->right->kind == ND_COMMA &&
			    node->right->left &&
			    node->right->left->kind == ND_ASSIGN &&
			    node->right->left->right &&
			    node->right->left->right->kind == ND_CALL &&
			    node->right->left->right->returns_struct &&
			    emit_arm64_direct_sret_small_gpr_call_to_saved(node->right->left->right,
			                                                   cg)) {
				return;
			}
			emit_struct_lvalue_addr(node->left, cg);
			cg->emit_push_acc();
			emit_struct_lvalue_addr(node->right, cg);
			cg->emit_pop_to_tmp();
			printf("    add x17, x1, #0\n");
			cg->emit_ptr_copy(node->value);
			return;
		}
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
		if (emit_arm64_try_emit_local_update_assign(node, cg))
			return;
		if (emit_arm64_try_emit_indirect_update_assign(node, cg))
			return;
		if (node->left && node->right &&
		    node->left->type && node->right->type &&
		    type_is_struct(node->left->type) &&
		    type_is_struct(node->right->type) &&
		    !(node->right->kind == ND_CALL && node->right->returns_struct)) {
			int copy_size = type_sizeof(node->left->type);

			if (copy_size <= 0)
				ICE("struct assignment requires complete type");
			emit_struct_lvalue_addr(node->left, cg);
			cg->emit_push_acc();
			emit_struct_lvalue_addr(node->right, cg);
			cg->emit_pop_to_tmp();
			printf("    add x17, x1, #0\n");
			cg->emit_ptr_copy(copy_size);
			return;
		}
		if (emit_target_is_arm64(cg) && node->left && node->left->type &&
		    type_is_fp_scalar(node->left->type)) {
			emit_arm64_fp_lvalue_addr(node->left, cg);
			cg->emit_push_acc();
			emit_expr(node->right, cg);
			if (!emit_expr_is_floating(node->right)) {
				emit_arm64_int_to_fp(node->right ? node->right->type : NULL,
				                     node->left->type);
			} else if (node->right->type && node->left->type &&
			           node->right->type->size != node->left->type->size) {
				printf("    fcvt %c0, %c0\n",
				       node->left->type->size == 4 ? 's' : 'd',
				       node->right->type->size == 4 ? 's' : 'd');
			}
			cg->emit_pop_to_tmp();
			emit_arm64_fp_store_addr("x1", node->left->type->size);
			return;
		}
		if (node->left && node->left->is_bitfield &&
		    (node->left->kind == ND_MEMBER || node->left->kind == ND_MEMBER_PTR)) {
			emit_assign_bitfield(node, cg);
			return;
		}
		if (emit_arm64_try_emit_indirect_assign(node, cg))
			return;

		if (node->right->kind == ND_CALL && node->right->returns_struct && node->left->type &&
		    emit_target_is_arm64(cg) &&
		    parser_classify_aggregate_abi(node->left->type, NULL) == AGGREGATE_ABI_HFA) {
			if (node->right->aggregate_abi_class == AGGREGATE_ABI_HFA) {
				if (emit_arm64_fixed_fp_call(node->right, cg)) {
					emit_arm64_store_hfa_return_lvalue(node->left, node->right->type, cg);
					return;
				}
				ICE("arm64 HFA aggregate return assignment requires fixed-fp call lowering");
			}
		}
		if (node->right->kind == ND_CALL && node->right->returns_struct && node->left->type && node->left->type->kind == TY_STRUCT) {
			if (emit_target_is_arm64(cg) &&
			    node->right->aggregate_abi_class == AGGREGATE_ABI_INTREGS) {
				if (emit_builtin_stack_call_expr(node->right, cg))
					ICE("builtin stack call cannot return INTREGS aggregate");
				if (emit_arm64_direct_intreg_aggregate_call(node->right, cg) ||
				    emit_arm64_fixed_fp_call(node->right, cg) ||
				    emit_arm64_direct_small_variadic_gpr_call(node->right, cg) ||
				    emit_arm64_direct_small_gpr_call(node->right, cg)) {
					if (!emit_arm64_store_intreg_aggregate_return_local(node->left,
					                                                    node->right, cg)) {
						ICE("arm64 INTREGS aggregate return assignment requires local struct destination");
					}
					return;
				}
				ICE("arm64 INTREGS aggregate return assignment requires direct call lowering");
			}
			if (emit_target_is_arm64(cg) &&
			    emit_arm64_fixed_fp_sret_call(node->left, node->right, cg)) {
				return;
			}
			if (emit_target_is_arm64(cg) &&
			    emit_arm64_direct_sret_small_gpr_call(node->left, node->right, cg)) {
				return;
			}
			int count = count_list(node->right->args);
			int cleanup_words = 1;
			int arm64_arg_slots;

			if (count + 1 > 8) {
				ICE("Only up to 8 function arguments are supported by the current call ABI");
			}

			cleanup_words += emit_push_call_args_reverse(node->right->args, cg);
			arm64_arg_slots = cleanup_words - 1;

			if (node->left->kind != ND_VAR) {
				ICE("Struct return assignment currently requires a local struct destination");
			}

			if (emit_target_is_arm64(cg)) {
				cg->emit_addr_local(node->left->offset);
				cg->emit_acc_to_saved();
				cg->emit_prepare_call_args(arm64_arg_slots, -1);
				printf("    mov x8, x17\n");
				cg->emit_call(node->right->name);
				cg->emit_cleanup_call_args(arm64_arg_slots, -1);
				return;
			}

			cg->emit_addr_local(node->left->offset);
			cg->emit_push_acc();

			cg->emit_prepare_call_args(emit_target_is_x86(cg) ? cleanup_words : (count + 1), -1);
			cg->emit_call(node->right->name);
			cg->emit_cleanup_call_args(emit_target_is_x86(cg) ? cleanup_words : (count + 1), -1);
			return;
		}

		if (node->left->kind == ND_GLOBAL_INDEX) {
			emit_expr(node->left->left, cg);
			cg->emit_push_acc();
			emit_expr(node->right, cg);
			cg->emit_pop_to_tmp();
			cg->emit_store_global_indexed(node->left->name, node->left->elem_size);
		} else if (node->left->kind == ND_INDEX) {
			if (emit_arm64_try_emit_const_local_index_store(node, cg))
				return;
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
			cg->emit_store_deref(node->left->is_pointer ? 8 : (node->left->elem_size ? node->left->elem_size : 4));
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
		if (emit_builtin_stack_call_expr(node, cg))
			return;
		if (emit_arm64_direct_intreg_aggregate_call(node, cg))
			return;
		if (emit_arm64_direct_small_variadic_gpr_call(node, cg))
			return;
		if (emit_arm64_direct_small_gpr_call(node, cg))
			return;
		if (emit_arm64_fixed_fp_call(node, cg))
			return;

		int count = count_list(node->args);
		int cleanup_words = emit_push_call_args_reverse(node->args, cg);

		if (node->left) {
			/*
			 * For indirect calls, keep the callee evaluation immediately
			 * before the call setup. Otherwise argument evaluation can
			 * clobber the backend scratch register used by emit_call_saved().
			 */
			emit_expr(node->left, cg);
			cg->emit_acc_to_saved();
		}

		int fixed_params = -1;
		if (!node->left && node->name[0])
			fixed_params = func_fixed_params(node->name);

		cg->emit_prepare_call_args((emit_target_is_x86(cg) || emit_target_is_arm64(cg))
		                               ? cleanup_words
		                               : count,
		                           fixed_params);
		if (node->left)
			cg->emit_call_saved();
		else
			cg->emit_call(node->name);
		cg->emit_cleanup_call_args((emit_target_is_x86(cg) || emit_target_is_arm64(cg))
		                               ? cleanup_words
		                               : count,
		                           fixed_params);
		return;
	}

	case ND_BLOCK:
		emit_block_expr(node, cg);
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
		cg->emit_label(get_user_label_id(node->name));
		return;

	case ND_GOTO:
		cg->emit_branch(get_user_label_id(node->name));
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
		if (emit_builtin_stack_call_stmt(node, cg))
			return;
		if (emit_arm64_direct_intreg_aggregate_call(node, cg))
			return;
		if (emit_arm64_direct_small_variadic_gpr_call(node, cg))
			return;
		if (emit_arm64_direct_small_gpr_call(node, cg))
			return;
		if (emit_arm64_fixed_fp_call(node, cg))
			return;

		int count = count_list(node->args);
		int cleanup_words = emit_push_call_args_reverse(node->args, cg);

		if (node->left) {
			emit_expr(node->left, cg);
			cg->emit_acc_to_saved();
		}

		int fixed_params = -1;
		if (!node->left && node->name[0])
			fixed_params = func_fixed_params(node->name);

		cg->emit_prepare_call_args((emit_target_is_x86(cg) || emit_target_is_arm64(cg))
		                               ? cleanup_words
		                               : count,
		                           fixed_params);
		if (node->left)
			cg->emit_call_saved();
		else
			cg->emit_call(node->name);
		cg->emit_cleanup_call_args((emit_target_is_x86(cg) || emit_target_is_arm64(cg))
		                               ? cleanup_words
		                               : count,
		                           fixed_params);
		return;
	}

	case ND_PRE_INC:
	case ND_PRE_DEC:
	case ND_POST_INC:
	case ND_POST_DEC:
		if (emit_arm64_try_emit_dead_incdec_stmt(node, cg))
			return;
		emit_expr(node, cg);
		return;

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
		if (node->left && node->left->type &&
		    emit_target_is_arm64(cg) &&
		    parser_classify_aggregate_abi(node->left->type, NULL) == AGGREGATE_ABI_HFA) {
			if (node->left->kind == ND_CALL) {
				if (!emit_arm64_fixed_fp_call(node->left, cg))
					ICE("arm64 HFA return call requires fixed-fp lowering");
			} else {
				emit_arm64_load_hfa_return_value(node->left, cg);
			}
			cg->emit_branch(return_label);
			return;
		}
		if (node->left && node->left->type && node->left->type->kind == TY_STRUCT) {
			if (emit_target_is_arm64(cg) &&
			    parser_classify_aggregate_abi(node->left->type, NULL) ==
			        AGGREGATE_ABI_INTREGS) {
				if (!emit_arm64_load_intreg_aggregate_return_value(node->left, cg))
					ICE("arm64 direct aggregate return requires loadable struct value");
				cg->emit_branch(return_label);
				return;
			}
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
			cg->emit_push_acc();
			emit_struct_lvalue_addr(node->left, cg);
			cg->emit_pop_to_tmp();
			printf("    add x17, x1, #0\n");
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

		if (!emit_arm64_try_branch_if_zero(node->cond, else_label, cg)) {
			emit_expr(node->cond, cg);
			cg->emit_branch_if_zero(else_label);
		}

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
		int continue_label = end_label;

		for (Node *c = node->body; c; c = c->next) {
			c->offset = new_label();
			if (!first_case_label && c->kind == ND_CASE)
				first_case_label = c->offset;
			if (c->kind == ND_DEFAULT)
				default_label = c->offset;
		}

		if (node->inc) {
			duff_cond_label = new_label();
			push_loop(end_label, duff_cond_label);
		} else {
			if (loop_depth > 0)
				continue_label = current_continue_label();
			push_loop(end_label, continue_label);
		}

		if (emit_arm64_try_emit_dense_switch(node, default_label, end_label, cg)) {
			pop_loop();
			cg->emit_label(end_label);
			return;
		}

		emit_expr(node->cond, cg);
		cg->emit_acc_to_tmp();

		for (Node *c = node->body; c; c = c->next) {
			if (c->kind != ND_CASE)
				continue;

			cg->emit_load_imm(c->value);
			if (cg->emit_cmp_branch)
				cg->emit_cmp_branch("je", TCC_SIZEOF_INT, c->offset);
			else {
				cg->emit_cmp_eq();
				cg->emit_branch_if_nonzero(c->offset);
			}
		}

		cg->emit_branch(default_label);

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
		if (!emit_arm64_try_branch_if_zero(node->cond, end_label, cg)) {
			emit_expr(node->cond, cg);
			cg->emit_branch_if_zero(end_label);
		}

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
		if (!emit_arm64_try_branch_if_nonzero(node->cond, start_label, cg)) {
			emit_expr(node->cond, cg);
			cg->emit_branch_if_nonzero(start_label);
		}

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
			if (!emit_arm64_try_branch_if_zero(node->cond, end_label, cg)) {
				emit_expr(node->cond, cg);
				cg->emit_branch_if_zero(end_label);
			}
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
		return; /* no-op constant expression statement */
	case ND_VAR:
		return; /* no-op variable reference statement */
	case ND_FUNC:
		return; /* no-op function reference statement */
	}

node_error_at(node, "Invalid statement node %s (%d)",
	              node_kind_name(node->kind), node->kind);
}

static int
emit_collect_hidden_struct_param_offsets(Node *node, int *offsets, int count, int max_count)
{
	if (!node || count >= max_count)
		return count;

	if (node->kind == ND_VAR &&
	    STRNCMP(node->name, "__paramptr_", 11) == 0) {
		int seen = 0;
		for (int i = 0; i < count; i++) {
			if (offsets[i] == node->offset) {
				seen = 1;
				break;
			}
		}
		if (!seen && count < max_count)
			offsets[count++] = node->offset;
	}

	count = emit_collect_hidden_struct_param_offsets(node->init, offsets, count, max_count);
	count = emit_collect_hidden_struct_param_offsets(node->cond, offsets, count, max_count);
	count = emit_collect_hidden_struct_param_offsets(node->inc, offsets, count, max_count);
	count = emit_collect_hidden_struct_param_offsets(node->left, offsets, count, max_count);
	count = emit_collect_hidden_struct_param_offsets(node->right, offsets, count, max_count);
	count = emit_collect_hidden_struct_param_offsets(node->args, offsets, count, max_count);
	count = emit_collect_hidden_struct_param_offsets(node->then_body, offsets, count, max_count);
	count = emit_collect_hidden_struct_param_offsets(node->else_body, offsets, count, max_count);
	count = emit_collect_hidden_struct_param_offsets(node->body, offsets, count, max_count);
	count = emit_collect_hidden_struct_param_offsets(node->next, offsets, count, max_count);
	return count;
}

static const char *
emit_function_param_struct_name(Node *func, int param_index, int offset)
{
	if (func && func->param_struct_names &&
	    param_index >= 0 && param_index < func->param_count &&
	    func->param_struct_names[param_index] &&
	    func->param_struct_names[param_index][0])
		return func->param_struct_names[param_index];

	if (func && func->debug_locals) {
		int i;

		for (i = 0; i < func->debug_local_count; i++) {
			if (func->debug_locals[i].offset == offset &&
			    func->debug_locals[i].struct_name[0])
				return func->debug_locals[i].struct_name;
		}
	}

	return NULL;
}

static void 
emit_function(Node *func, Codegen *cg)
{
	int leaf_sp_frame = 0;

	current_emit_function = func;
	return_label = new_label();
	user_label_count = 0;

	if (emit_target_is_arm64(cg)) {
		arm64_clear_live_param_regs();
		arm64_simple_leaf = emit_arm64_can_omit_frame(func, cg) ? 1 : 0;
		leaf_sp_frame = emit_arm64_can_use_leaf_sp_frame(func, cg) ? func->stack_size : 0;
		arm64_leaf_sp_frame_size = leaf_sp_frame > 0 ? ((leaf_sp_frame + 15) & ~15) : 0;
		if (!arm64_simple_leaf && !leaf_sp_frame && func->stack_size > 0)
			arm64_begin_fixed_frame();
	}
	cg->emit_function_start(func->name, func->is_static);
	if (!leaf_sp_frame)
		cg->emit_stack_alloc(func->stack_size);

	if (emit_target_is_arm64(cg)) {
		int int_reg = 0;
		int fp_reg = 0;
		int param_base = 0;
		int keep_full_param_slots = func_fixed_params(func->name) >= 0;
		int hidden_struct_offsets[64];
		int hidden_struct_count = emit_collect_hidden_struct_param_offsets(
		    func->body, hidden_struct_offsets, 0, 64);
		int hidden_struct_index = 0;

		if (func->param_count > 0 &&
		    (!func->param_names || !func->param_names[0] || !func->param_names[0][0])) {
			int hidden_offset = func->param_offsets ? func->param_offsets[0] : -8;
			/*
			 * AArch64 uses x8 for the hidden by-reference struct return
			 * destination. User integer parameters still begin in x0.
			 */
			printf("    mov x9, x8\n");
			printf("    stur x9, [x29, #%d]\n", hidden_offset);
			int_reg = 0;
			param_base = 1;
		}

		for (int i = param_base; i < func->param_count; i++) {
			int offset = func->param_offsets ? func->param_offsets[i] : (-(i + 1) * 8);
			int param_abi_size = (func->param_abi_sizes && i < func->param_count &&
			                      func->param_abi_sizes[i] > 0)
			                   ? func->param_abi_sizes[i]
			                   : TCC_SIZEOF_PTR;
			const char *param_struct_name = emit_function_param_struct_name(func, i, offset);
			if (keep_full_param_slots && !emit_target_is_arm64(cg))
				param_abi_size = TCC_SIZEOF_PTR;
			int param_type_id = (func->param_type_ids && i < func->param_count)
			                  ? func->param_type_ids[i]
			                  : DBG_TYPE_NONE;
			int fp_size = 0;

			if (param_type_id == DBG_TYPE_FLOAT)
				fp_size = 4;
			else if (param_type_id == DBG_TYPE_DOUBLE)
				fp_size = 8;

			if (param_struct_name && param_struct_name[0]) {
				int hfa_elem_size;
				int hfa_elem_count;

				if (parser_arm64_hfa_info_name(param_struct_name,
				                               &hfa_elem_size, &hfa_elem_count)) {
					int j;

					for (j = 0; j < hfa_elem_count; j++)
						emit_arm64_fp_store_incoming_param(cg, fp_reg + j,
						                                   offset + j * hfa_elem_size,
						                                   hfa_elem_size);
					fp_reg += hfa_elem_count;
					continue;
				}
			}

			if (fp_size > 0) {
				emit_arm64_fp_store_incoming_param(cg, fp_reg, offset, fp_size);
				fp_reg++;
			} else if (param_abi_size > (int)TCC_SIZEOF_PTR &&
			           param_abi_size <= 16) {
				int second_size = param_abi_size - (int)TCC_SIZEOF_PTR;

				if (cg->emit_store_param_sized)
					cg->emit_store_param_sized(int_reg, offset, (int)TCC_SIZEOF_PTR);
				else
					cg->emit_store_param(int_reg, offset);
				if (cg->emit_store_param_sized)
					cg->emit_store_param_sized(int_reg + 1,
					                           offset + (int)TCC_SIZEOF_PTR,
					                           second_size);
				else
					cg->emit_store_param(int_reg + 1,
					                     offset + (int)TCC_SIZEOF_PTR);
				int_reg += 2;
			} else if (param_struct_name && param_struct_name[0] &&
			           hidden_struct_index < hidden_struct_count) {
				if (cg->emit_store_param_sized)
					cg->emit_store_param_sized(int_reg,
					                           hidden_struct_offsets[hidden_struct_index++],
					                           TCC_SIZEOF_PTR);
				else
					cg->emit_store_param(int_reg, hidden_struct_offsets[hidden_struct_index++]);
				int_reg++;
			} else {
				if (cg->emit_store_param_sized)
					cg->emit_store_param_sized(int_reg, offset, param_abi_size);
				else
					cg->emit_store_param(int_reg, offset);
				int_reg++;
			}
		}
	} else if (emit_target_is_x86(cg) && func->param_abi_sizes && cg->emit_copy_incoming_param) {
		int stack_offset = 8;
		for (int i = 0; i < func->param_count; i++) {
			int offset = func->param_offsets ? func->param_offsets[i] : (-(i + 1) * 8);
			int size = func->param_abi_sizes[i] > 0 ? func->param_abi_sizes[i] : 4;
			cg->emit_copy_incoming_param(stack_offset, offset, size);
			stack_offset += ((size + 3) & ~3);
		}
	} else {
		for (int i = 0; i < func->param_count; i++) {
			int offset = -(i + 1) * 8;
			cg->emit_store_param(i, offset);
		}
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
	current_emit_function = NULL;
	if (emit_target_is_arm64(cg)) {
		arm64_simple_leaf = 0;
		arm64_leaf_sp_frame_size = 0;
	}
}

void 
emit_program(Node *program, Codegen *cg)
{
	next_label_id = 1;
	loop_depth = 0;

	cg->emit_preamble();

	data_emit_globals(cg, 0);

	emit_string_literals(program, cg);

	for (Node *func = program; func; func = func->next) {
		if (func->kind != ND_FUNC) {
			ICE("Expected function node");
		}
		emit_function(func, cg);
	}
}

void
emit_program_hybrid(Node *program, IRProgram *ir, Codegen *cg,
                    HybridEmitProfile *profile)
{
	if (!program || !ir || !cg)
		ICE("emit_program_hybrid requires program, ir, and codegen");

	next_label_id = ir_program_label_limit(ir);
	if (next_label_id < 1)
		next_label_id = 1;
	loop_depth = 0;

	cg->emit_preamble();
	data_emit_globals(cg, 1);

	for (Node *func = program; func; func = func->next) {
		double t0;

		if (func->kind != ND_FUNC)
			ICE("Expected function node");

		t0 = profile ? tcc_monotonic_seconds() : 0.0;
		emit_function_string_literals(func, cg);
		if (profile)
			profile->string_literals += tcc_monotonic_seconds() - t0;

		if (ir_function_is_supported(ir, func->name)) {
			t0 = profile ? tcc_monotonic_seconds() : 0.0;
			ir_emit_named_function(ir, cg, func->name);
			if (profile)
				profile->ir_functions += tcc_monotonic_seconds() - t0;
		} else {
			t0 = profile ? tcc_monotonic_seconds() : 0.0;
			emit_function(func, cg);
			if (profile)
				profile->ast_fallback_functions += tcc_monotonic_seconds() - t0;
		}
	}
}
