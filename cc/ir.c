#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tcc.h"
#include "lexer.h"
#include "ir.h"
#include "parser.h"

#define IR_LOOP_MAX 128

typedef struct IRLoop {
	int break_label;
	int continue_label;
} IRLoop;

static int ir_is_local_scalar_load(IRInst *inst);
static int ir_is_local_scalar_store(IRInst *inst);
static void ir_expr(IRProgram *ir, Node *node);
static void ir_stmt(IRProgram *ir, Node *node);
static void ir_remove_range(IRProgram *program, IRInst *prev, IRInst *first, IRInst *last);

static int next_ir_label = 1;

/* Source location for the node currently being compiled (for debug info) */
static int ir_current_src_line = 0;
static int ir_current_src_filename_id = 0;
static int ir_current_returns_struct = 0;
static int ir_current_struct_return_size = 0;
static char ir_current_struct_return_name[64];
static IRLoop loop_stack[IR_LOOP_MAX];
static int loop_depth = 0;

static void 
ir_emit_raw_string_literal(const char *value)
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

static void 
ir_emit_sym(const char *name, int is_static)
{
	if (!is_static)
		printf(".global _%s\n", name);
	printf("_%s:\n", name);
}

static int 
ir_global_is_extern(const char *name)
{
	for (int i = 0; i < global_count; i++) {
		if (STRCMP(&globals[i].name[0], name) == 0)
			return globals[i].is_dylib;  /* only dylib symbols need GOT */
	}
	return 0;
}

static int 
ir_global_elem_size(const char *name)
{
	for (int i = 0; i < global_count; i++) {
		if (STRCMP(&globals[i].name[0], name) == 0) {
			/* pointer globals and string pointer globals are 8 bytes on arm64 */
			if (globals[i].ptr_elem_size || globals[i].is_string)
				return 8;
			return globals[i].elem_size ? globals[i].elem_size : 4;
		}
	}
	return 4;
}

static void 
ir_emit_global_data(Codegen *cg)
{
	if (global_count <= 0)
		return;

	printf(".data\n");

	for (int i = 0; i < global_count; i++) {
		if (globals[i].is_extern)
			continue;
		if (globals[i].is_array) {
			if (globals[i].is_struct) {
				printf("    .align 4\n");
				ir_emit_sym(globals[i].name, globals[i].is_static);
				if (globals[i].init_count > 0) {
					int total = globals[i].array_len * globals[i].elem_size;
					for (int j = 0; j < total; j++) {
						/* Global arrays of structs may contain pointer/function-pointer
						 * fields.  The parser records relocation targets in
						 * init_syms[] using pointer-sized slots, so emit a relocation
						 * instead of eight zero bytes at those offsets.
						 */
						if ((j % 8) == 0 && (j / 8) < 128 && globals[i].init_syms[j / 8][0]) {
							printf("    .quad _%s\n", globals[i].init_syms[j / 8]);
							j += 7;
							continue;
						}
						printf("    .byte %d\n", globals[i].init_values[j] & 255);
					}
				} else {
					printf("    .zero %d\n", globals[i].array_len * globals[i].elem_size);
				}
			} else if (globals[i].elem_size == 1) {
				ir_emit_sym(globals[i].name, globals[i].is_static);
				if (globals[i].is_string_array)
					ir_emit_raw_string_literal(globals[i].string_value);
				else
					printf("    .zero %d\n", globals[i].array_len);
			} else if (globals[i].init_count > 0) {
				printf("    .align 4\n");
				ir_emit_sym(globals[i].name, globals[i].is_static);
				for (int j = 0; j < globals[i].array_len; j++) {
					int value = j < globals[i].init_count ? globals[i].init_values[j] : 0;
					if (j < 128 && globals[i].init_syms[j][0]) {
						if (globals[i].elem_size == 8)
							printf("    .quad _%s\n", globals[i].init_syms[j]);
						else if (globals[i].elem_size == 2)
							printf("    .short _%s\n", globals[i].init_syms[j]);
						else
							printf("    .word _%s\n", globals[i].init_syms[j]);
					} else if (globals[i].elem_size == 8) {
						printf("    .quad %d\n", value);
					} else if (globals[i].elem_size == 2) {
						printf("    .short %d\n", value & 65535);
					} else {
						printf("    .word %d\n", value);
					}
				}
			} else {
				printf("    .align 4\n");
				ir_emit_sym(globals[i].name, globals[i].is_static);
				printf("    .zero %d\n", globals[i].array_len * globals[i].elem_size);
			}
		} else if (globals[i].is_string) {
			cg->emit_string_literal(globals[i].string_label, globals[i].string_value);
			printf(".data\n");
			printf("    .align 8\n");
			ir_emit_sym(globals[i].name, globals[i].is_static);
			printf("    .quad Lstr%d\n", globals[i].string_label);
		} else if (globals[i].is_struct) {
			printf("    .align 8\n");
			ir_emit_sym(globals[i].name, globals[i].is_static);
			if (globals[i].init_count > 0) {
				/* emit byte-by-byte but use symbol names for pointer-sized slots */
				int j = 0;
				while (j < globals[i].elem_size) {
					/* check if this byte offset starts a symbol slot (8-byte pointer) */
					int slot = j / 8;
					if (slot < 128 && globals[i].init_syms[slot][0] != '\0' &&
					        j % 8 == 0 && j + 8 <= globals[i].elem_size) {
						printf("    .quad _%s\n", globals[i].init_syms[slot]);
						j += 8;
					} else if (slot < 128 && globals[i].init_syms[slot][0] != '\0' &&
					           j % 4 == 0 && j + 4 <= globals[i].elem_size) {
						/* 32-bit pointer platform */
						printf("    .long _%s\n", globals[i].init_syms[slot]);
						j += 4;
					} else {
						printf("    .byte %d\n", globals[i].init_values[j] & 255);
						j++;
					}
				}
			} else {
				printf("    .zero %d\n", globals[i].elem_size);
			}
		} else if (globals[i].is_addr) {
			printf("    .align 8\n");
			ir_emit_sym(globals[i].name, globals[i].is_static);
			printf("    .quad _%s\n", globals[i].addr_name);
		} else if (globals[i].elem_size == 1) {
			ir_emit_sym(globals[i].name, globals[i].is_static);
			printf("    .byte %d\n", globals[i].init_value & 255);
		} else if (globals[i].elem_size == 2) {
			ir_emit_sym(globals[i].name, globals[i].is_static);
			printf("    .short %d\n", globals[i].init_value & 65535);
		} else {
			ir_emit_sym(globals[i].name, globals[i].is_static);
			printf("    .quad %d\n", globals[i].init_value);
		}
	}

	printf(".text\n");
}

static void 
ir_mark_unsupported(IRProgram *ir, const char *what)
{
	ir->unsupported_count++;

	if (ir->unsupported_reason[0] == '\0')
		STRNCPY(ir->unsupported_reason, what, sizeof(ir->unsupported_reason) - 1);
}

int 
ir_has_unsupported(IRProgram *program)
{
	return program && program->unsupported_count > 0;
}

const char *
ir_unsupported_reason(IRProgram *program)
{
	if (!program || program->unsupported_reason[0] == '\0')
		return "";
	return program->unsupported_reason;
}

static IRInst *
ir_emit_ex(IRProgram *ir, IRKind kind, const char *op, const char *name, int value, int aux)
{
	IRInst *inst = xcalloc(1, sizeof(IRInst));
	inst->kind = kind;
	inst->id = ir->next_id++;
	inst->value = value;
	inst->aux = aux;
	inst->src_line = ir_current_src_line;
	inst->src_filename_id = ir_current_src_filename_id;

	if (op)
		STRNCPY(inst->op, op, sizeof(inst->op) - 1);
	if (name)
		STRNCPY(inst->name, name, sizeof(inst->name) - 1);

	if (!ir->head)
		ir->head = inst;
	else
		ir->tail->next = inst;

	ir->tail = inst;
	return inst;
}

static IRInst *
ir_emit(IRProgram *ir, IRKind kind, const char *op, const char *name, int value)
{
	return ir_emit_ex(ir, kind, op, name, value, 0);
}

static int 
new_ir_label(void)
{
	return next_ir_label++;
}

#define IR_USER_LABEL_MAX 256

typedef struct IRUserLabel {
	char name[128];
	int id;
} IRUserLabel;

static IRUserLabel ir_user_labels[IR_USER_LABEL_MAX];
static int ir_user_label_count = 0;

static int
ir_get_user_label_id(const char *name)
{
	if (!name)
		name = "";

	for (int i = 0; i < ir_user_label_count; i++) {
		if (STRCMP(ir_user_labels[i].name, name) == 0)
			return ir_user_labels[i].id;
	}

	if (ir_user_label_count >= IR_USER_LABEL_MAX)
		ICE("Too many user labels in one function");

	IRUserLabel *l = &ir_user_labels[ir_user_label_count++];
	STRNCPY(l->name, name, sizeof(l->name) - 1);
	l->id = new_ir_label();
	return l->id;
}

static void 
ir_add_string(IRProgram *ir, int label, const char *value)
{
	IRString *existing;
	IRString *s;

	for (existing = ir->strings; existing; existing = existing->next) {
		if (existing->label == label)
			return;
	}

	s = xcalloc(1, sizeof(IRString));
	s->label = label;
	STRNCPY(s->value, value, sizeof(s->value) - 1);
	s->next = ir->strings;
	ir->strings = s;
}

static void 
push_loop(int break_label, int continue_label)
{
	if (loop_depth >= IR_LOOP_MAX) {
		ICE("Too many nested IR loops");
	}

	loop_stack[loop_depth].break_label = break_label;
	loop_stack[loop_depth].continue_label = continue_label;
	loop_depth++;
}

static void 
pop_loop(void)
{
	if (loop_depth > 0)
		loop_depth--;
}

static int 
current_break_label(IRProgram *ir)
{
	if (loop_depth == 0) {
		ir_mark_unsupported(ir, "break outside lowered loop");
		return 0;
	}

	return loop_stack[loop_depth - 1].break_label;
}

static int 
current_continue_label(IRProgram *ir)
{
	if (loop_depth == 0) {
		ir_mark_unsupported(ir, "continue outside lowered loop");
		return 0;
	}

	return loop_stack[loop_depth - 1].continue_label;
}

static const char *
binop_name(NodeKind kind)
{
	switch (kind) {
	case ND_ADD:
		return "add";
	case ND_SUB:
		return "sub";
	case ND_MUL:
		return "mul";
	case ND_DIV:
		return "div";
	case ND_MOD:
		return "mod";
	case ND_BITAND:
		return "and";
	case ND_BITOR:
		return "or";
	case ND_BITXOR:
		return "xor";
	case ND_SHL:
		return "shl";
	case ND_SHR:
		return "shr";
	case ND_EQ:
		return "eq";
	case ND_NE:
		return "ne";
	case ND_LT:
		return "lt";
	case ND_LE:
		return "le";
	case ND_GT:
		return "gt";
	case ND_GE:
		return "ge";
	case ND_LOGICAL_AND:
		return "land";
	case ND_LOGICAL_OR:
		return "lor";
	default:
		return NULL;
	}
}

static int 
ir_lvalue_size(Node *n)
{
	if (!n)
		return 4;
	if (n->is_pointer)
		return 8;
	return n->elem_size ? n->elem_size : 4;
}

static int 
ir_incdec_delta(Node *target, int is_dec)
{
	int delta = 1;

	if (target && target->is_pointer)
		delta = target->elem_size ? target->elem_size : 1;

	return is_dec ? -delta : delta;
}

static void 
ir_store_lvalue_from_stack(IRProgram *ir, Node *target)
{
	if (!target) {
		ir_mark_unsupported(ir, "missing inc/dec lvalue");
		return;
	}

	switch (target->kind) {
	case ND_VAR:
	case ND_MEMBER:
		ir_emit_ex(ir, IR_STORE, "local", target->name, target->offset, ir_lvalue_size(target));
		return;

	case ND_GLOBAL: {
		IRInst *gi = ir_emit(ir, IR_STORE, "global", target->name, 0);
		gi->is_extern = ir_global_is_extern(target->name);
	}
	return;

	case ND_INDEX:
		/*
		 * Generic indexed stores need both index and value.  Inc/dec
		 * lowering currently handles scalar local/global/member lvalues.
		 */
		ir_mark_unsupported(ir, "inc/dec indexed lvalue");
		ir_emit(ir, IR_UNOP, "unsupported", NULL, 0);
		return;

	case ND_DEREF:
		/* *ptr = val — value is on stack, address is the pointer expr result */
		ir_emit_ex(ir, IR_STORE, "deref", NULL, ir_lvalue_size(target), 0);
		return;

	case ND_MEMBER_PTR:
		/* ptr->field = val — value on stack, need to compute ptr+offset then store */
		ir_emit_ex(ir, IR_STORE, "member_ptr", NULL, target->offset, 0);
		return;

	case ND_GLOBAL_INDEX:
		ir_mark_unsupported(ir, "inc/dec complex lvalue");
		ir_emit(ir, IR_UNOP, "unsupported", NULL, 0);
		return;

	default:
		ir_mark_unsupported(ir, "inc/dec unsupported lvalue");
		ir_emit(ir, IR_UNOP, "unsupported", NULL, 0);
		return;
	}
}

static void 
ir_incdec_expr(IRProgram *ir, Node *node)
{
	Node *target = node->left;
	int is_post = node->kind == ND_POST_INC || node->kind == ND_POST_DEC;
	int is_dec = node->kind == ND_PRE_DEC || node->kind == ND_POST_DEC;
	int delta = ir_incdec_delta(target, is_dec);

	if (!target) {
		ir_mark_unsupported(ir, "inc/dec missing target");
		ir_emit(ir, IR_UNOP, "unsupported", NULL, 0);
		return;
	}

	/*
	 * Pre:
	 *   load old
	 *   const delta
	 *   add
	 *   assign target = new
	 *   reload target   -> expression value is new
	 *
	 * Post:
	 *   load old        -> expression value remains on stack
	 *   load old
	 *   const delta
	 *   add
	 *   assign target = new
	 *
	 * This keeps stack semantics simple and avoids adding a DUP IR op yet.
	 */
	if (is_post)
		ir_expr(ir, target);

	/* For MEMBER_PTR/DEREF: compute addr into acc, save it, load value, inc, restore addr, store */
	if (target->kind == ND_MEMBER_PTR || target->kind == ND_DEREF) {
		int sz = target->elem_size ? target->elem_size : 4;
		if (target->kind == ND_MEMBER_PTR) {
			ir_expr(ir, target->left);
			ir_emit(ir, IR_UNOP, "add_offset", NULL, target->offset);
		} else {
			ir_expr(ir, target->left);
		}
		/* acc = addr (also on stack from ir_expr above — pop after saving) */
		ir_emit(ir, IR_UNOP, "acc_to_saved", NULL, 0);           /* ecx = addr; no stack change */
		ir_emit(ir, IR_POP, NULL, NULL, 0);                       /* discard pushed addr ptr */
		ir_emit_ex(ir, IR_UNOP, "load_via_saved", NULL, sz, 0);  /* acc = old → pushed (+1) */
		ir_emit(ir, IR_CONST, NULL, NULL, delta);                  /* pushed (+1) */
		ir_emit(ir, IR_BINOP, "add", NULL, 0);                    /* pops 2, pushes new (-1) */
		ir_emit_ex(ir, IR_STORE, "via_saved", NULL, sz, 0);       /* [ecx] = new; no stack */
		/* Net stack change: +1 (new on top)
		 * post-inc: old pre-pushed; pop new → net 0, old is result */
		if (is_post)
			ir_emit(ir, IR_POP, NULL, NULL, 0);
		/* pre-inc: net +1, new is result ✓ */
	} else {
		ir_expr(ir, target);
		ir_emit(ir, IR_CONST, NULL, NULL, delta);
		ir_emit(ir, IR_BINOP, "add", NULL, 0);
		ir_store_lvalue_from_stack(ir, target);
	}

	if (!is_post)
		ir_expr(ir, target);
}

static int 
is_simple_lvalue(Node *node)
{
	return node && (node->kind == ND_VAR ||
	                node->kind == ND_GLOBAL ||
	                node->kind == ND_GLOBAL_INDEX ||
	                node->kind == ND_INDEX ||
	                node->kind == ND_DEREF ||
	                node->kind == ND_MEMBER ||
	                node->kind == ND_MEMBER_PTR);
}

static void 
ir_list(IRProgram *ir, Node *node)
{
	for (Node *n = node; n; n = n->next) {
		if (n->kind == ND_BLOCK) {
			ir_list(ir,n->body);
			continue;
		}
		ir_stmt(ir, n);
	}
}

static void 
ir_stmt(IRProgram *ir, Node *n)
{
tail:
	if (!n)
		return;
	/* Track source location for debug info */
	if (n->line > 0) {
		ir_current_src_line = n->line;
		ir_current_src_filename_id = n->filename_id;
	}

	switch (n->kind) {
	case ND_LABEL:
		ir_emit_ex(ir, IR_LABEL, NULL, n->name, 0, 0);
		return;

	case ND_GOTO:
		ir_emit_ex(ir, IR_BRANCH, NULL, n->name, 0, 0);
		return;

	case ND_FUNC:
		ir_current_returns_struct = n->returns_struct;
		ir_current_struct_return_size = n->struct_return_size;
		STRNCPY(ir_current_struct_return_name,
		        n->return_struct_name[0] ? n->return_struct_name : "",
		        sizeof(ir_current_struct_return_name) - 1);

		{
			IRInst *func_inst = ir_emit_ex(ir, IR_LABEL, "func", n->name,
			                               n->stack_size, n->param_count);

			if (func_inst)
				func_inst->is_static = n->is_static;
		}

		if (n->body && n->body->kind == ND_BLOCK)
			ir_list(ir, n->body->body);
		else
			ir_list(ir, n->body);

		ir_current_returns_struct = 0;
		ir_current_struct_return_size = 0;
		ir_current_struct_return_name[0] = '\0';
		return;

	case ND_BLOCK:
		ir_list(ir, n->body);
		return;

	case ND_COMMA:
		ir_stmt(ir, n->left);
		n = n->right;
		goto tail;

	case ND_CALL: {
		if (n->returns_struct) {
			ir_mark_unsupported(ir, "struct-returning call statement");
			ir_emit(ir, IR_UNOP, "unsupported", NULL, 0);
			return;
		}

		int count = 0;
		Node *args[32] = {0};

		for (Node *arg = n->args; arg; arg = arg->next) {
			if (count >= 32) {
				ir_mark_unsupported(ir, "too many call arguments");
				ir_emit(ir, IR_UNOP, "unsupported", NULL, 0);
				return;
			}
			args[count++] = arg;
		}

		/*
		 * Call used as a statement: its return value is discarded.  Keep the
		 * same narrow fast path as expression calls for the common direct
		 * one-string-argument case, but mark the call as discard-result so
		 * codegen does not push the return value.
		 */
		if (!n->left && count == 1 && args[0]->kind == ND_STRING) {
			ir_add_string(ir, args[0]->string_label, args[0]->string_value);
			ir_emit(ir, IR_LOAD, "string_acc", args[0]->string_value, args[0]->string_label);
			ir_emit_ex(ir, IR_CALL_ACC_ARG0, NULL, n->name, 0, 1);
			return;
		}

		for (int i = count - 1; i >= 0; i--)
			ir_expr(ir, args[i]);

		if (n->left) {
			ir_expr(ir, n->left);
			ir_emit_ex(ir, IR_CALL_INDIRECT, NULL, NULL, count, 1);
		} else {
			{
				IRInst *_ci = ir_emit_ex(ir, IR_CALL, NULL, n->name, count, 1);
				if (_ci) _ci->fixed_params = func_fixed_params(n->name);
			}
		}
		return;
	}

	case ND_ASM:
		ir_emit_ex(ir, IR_ASM, n->asm_is_volatile ? "volatile" : "asm",
		           n->string_value, 0, n->asm_is_volatile);
		return;

	case ND_DECL:
	case ND_PTR_DECL:
	case ND_ARRAY_DECL:
	case ND_STRUCT_DECL:
		return;

	case ND_RETURN:
		if (ir_current_returns_struct) {
			Node *ret_val = n->left;

			/* If return value is a ND_COMMA (spilled struct call), use the right side */
			if (ret_val && ret_val->kind == ND_COMMA)
				ret_val = ret_val->right;

			if (!ret_val || ret_val->kind != ND_VAR ||
			        !ret_val->type || ret_val->type->kind != TY_STRUCT) {
				ir_mark_unsupported(ir, "struct return requires local struct");
				ir_emit(ir, IR_UNOP, "unsupported", NULL, 0);
				return;
			}

			/* emit the side-effect part of ND_COMMA if present */
			if (n->left && n->left->kind == ND_COMMA)
				ir_stmt(ir, n->left->left);

			/* Copy return struct to hidden pointer, field-by-field with correct sizes */
			int nfields = struct_field_count(ir_current_struct_return_name);
			if (nfields > 0) {
				for (int fi = 0; fi < nfields; fi++) {
					int foff = struct_field_offset(ir_current_struct_return_name, fi);
					int fsz  = struct_field_size(ir_current_struct_return_name, fi);
					ir_emit_ex(ir, IR_LOAD, "local", "__ret", -8, 8);
					if (foff)
						ir_emit(ir, IR_UNOP, "add_offset", NULL, foff);
					ir_emit_ex(ir, IR_LOAD, "local", ret_val->name,
					           ret_val->offset + foff, fsz);
					ir_emit_ex(ir, IR_STORE, "deref", NULL, fsz, 0);
				}
			} else {
				/* Fallback: word-by-word (4-byte) copy */
				for (int off = 0; off < ir_current_struct_return_size; off += 4) {
					ir_emit_ex(ir, IR_LOAD, "local", "__ret", -8, 8);
					if (off)
						ir_emit(ir, IR_UNOP, "add_offset", NULL, off);
					ir_emit_ex(ir, IR_LOAD, "local", ret_val->name,
					           ret_val->offset + off, 4);
					ir_emit_ex(ir, IR_STORE, "deref", NULL, 4, 0);
				}
			}

			ir_emit(ir, IR_CONST, NULL, NULL, 0);
			ir_emit(ir, IR_RETURN, NULL, NULL, 0);
			return;
		}

		if (n->left)
			ir_expr(ir, n->left);
		else
			ir_emit(ir, IR_CONST, NULL, NULL, 0);
		ir_emit(ir, IR_RETURN, NULL, NULL, 0);
		return;

	case ND_PRE_INC:
	case ND_PRE_DEC:
	case ND_POST_INC:
	case ND_POST_DEC:
		ir_incdec_expr(ir, n);
		ir_emit(ir, IR_POP, NULL, NULL, 0);
		return;

	case ND_ASSIGN:
		if (n->right && n->right->kind == ND_GLOBAL &&
		        n->right->type && n->right->type->kind == TY_STRUCT &&
		        n->left && n->left->kind == ND_VAR &&
		        n->left->type && n->left->type->kind == TY_STRUCT) {
			/* local_struct = global_struct -- word-by-word copy */
			int sz = n->right->type->size;
			for (int off = 0; off < sz; off += 4) {
				ir_emit(ir, IR_LOAD, "addr_global", n->right->name, 0);
				if (off)
					ir_emit(ir, IR_UNOP, "add_offset", NULL, off);
				ir_emit_ex(ir, IR_UNOP, "load_deref", NULL, 4, 0);
				ir_emit_ex(ir, IR_STORE, "local", n->left->name, n->left->offset + off, 4);
			}
			return;
		}

		if (n->right && n->right->kind == ND_CALL && n->right->returns_struct &&
		        n->left && n->left->kind == ND_VAR &&
		        n->left->type && n->left->type->kind == TY_STRUCT) {
			int count = 0;
			Node *args[32] = {0};

			for (Node *arg = n->right->args; arg; arg = arg->next) {
				if (count >= 31) {
					ir_mark_unsupported(ir, "too many struct-return call arguments");
					ir_emit(ir, IR_UNOP, "unsupported", NULL, 0);
					return;
				}
				args[count++] = arg;
			}

			for (int i = count - 1; i >= 0; i--)
				ir_expr(ir, args[i]);

			ir_emit(ir, IR_LOAD, "addr_local", n->left->name, n->left->offset);
			{
				IRInst *_ci = ir_emit_ex(ir, IR_CALL, NULL, n->right->name, count + 1, 0);
				if (_ci) _ci->fixed_params = -1; /* struct return: not variadic */
			}
			ir_emit(ir, IR_POP, NULL, NULL, 0);
			return;
		}

		if (n->left && n->left->kind == ND_GLOBAL_INDEX) {
			ir_expr(ir, n->left->left);
			ir_expr(ir, n->right);
			ir_emit_ex(ir, IR_STORE, "gidx_store", n->left->name,
			           n->left->elem_size ? n->left->elem_size : 4, 0);
			return;
		}

		if (n->left && n->left->kind == ND_INDEX) {
			/*
			 * Stack order: index, value.  The IR store_indexed emitter pops
			 * value to acc and index to tmp, matching the mature AST path.
			 */
			ir_expr(ir, n->left->left);
			ir_expr(ir, n->right);
			ir_emit_ex(ir, IR_STORE, "indexed", n->left->name,
			           n->left->offset, n->left->elem_size ? n->left->elem_size : 4);
			return;
		}

		if (n->left && n->left->kind == ND_DEREF) {
			/*
			 * Stack order: address, value.  The IR store_deref emitter pops
			 * value to acc and address to tmp.
			 * For large struct copies (> 8 bytes), use ptr_copy which keeps
			 * both src and dst pointers available for word-by-word copy.
			 */
			int deref_sz = n->left->elem_size ? n->left->elem_size : 4;
			if (deref_sz > 8 && n->right && n->right->kind == ND_DEREF) {
				/* Emit: dst=acc_to_saved, src=acc, then ptr_copy(size) */
				ir_expr(ir, n->left->left);   /* dst ptr in acc */
				ir_emit(ir, IR_UNOP, "acc_to_saved", NULL, 0);
				ir_expr(ir, n->right->left);  /* src ptr in acc */
				ir_emit_ex(ir, IR_UNOP, "ptr_copy", NULL, deref_sz, 0);
			} else {
				ir_expr(ir, n->left->left);
				ir_expr(ir, n->right);
				ir_emit_ex(ir, IR_STORE, "deref", NULL, deref_sz, 0);
			}
			return;
		}

		if (n->left && n->left->kind == ND_MEMBER_PTR) {
			ir_expr(ir, n->left->left);
			ir_emit(ir, IR_UNOP, "add_offset", NULL, n->left->offset);
			ir_expr(ir, n->right);
			ir_emit_ex(ir, IR_STORE, "deref", NULL,
			           n->left->elem_size ? n->left->elem_size : 4, 0);
			return;
		}

		ir_expr(ir, n->right);
		if (is_simple_lvalue(n->left)) {
			if (n->left->kind == ND_GLOBAL) {
				IRInst *gi = ir_emit(ir, IR_STORE, "global", n->left->name, 0);
				gi->is_extern = ir_global_is_extern(n->left->name);
			} else if (n->left->kind == ND_GLOBAL_INDEX) {
				{
					IRInst *gi = ir_emit(ir, IR_STORE, "global", n->left->name, 0);
					gi->is_extern = ir_global_is_extern(n->left->name);
				}
			} else if (n->left->kind == ND_DEREF) {
				ir_emit(ir, IR_STORE, "deref", NULL,
				        n->left->elem_size ? n->left->elem_size : 4);
			} else if (n->left->kind == ND_MEMBER_PTR) {
				ir_emit(ir, IR_STORE, "member_ptr", NULL, n->left->offset);
			} else if (n->left->kind == ND_MEMBER)
				ir_emit_ex(ir, IR_STORE, "local", n->left->name, n->left->offset,
				           n->left->elem_size ? n->left->elem_size : 4);
			else
				ir_emit_ex(ir, IR_STORE, "local", n->left->name, n->left->offset,
				           n->left->is_pointer ? 8 : (n->left->elem_size ? n->left->elem_size : 4));
		} else {
			ir_mark_unsupported(ir, "complex assignment lvalue");
			ir_emit(ir, IR_STORE, "unsupported", "<lvalue>", 0);
		}
		return;

	case ND_IF: {
		int else_label = new_ir_label();
		int end_label = new_ir_label();

		ir_expr(ir, n->cond);
		ir_emit(ir, IR_BRANCH, "jz", NULL, else_label);
		ir_list(ir, n->then_body);
		ir_emit(ir, IR_BRANCH, "jmp", NULL, end_label);
		ir_emit(ir, IR_LABEL, "L", NULL, else_label);
		ir_list(ir, n->else_body);
		ir_emit(ir, IR_LABEL, "L", NULL, end_label);
		return;
	}

	case ND_WHILE: {
		int begin_label = new_ir_label();
		int end_label = new_ir_label();

		ir_emit(ir, IR_LABEL, "L", NULL, begin_label);
		ir_expr(ir, n->cond);
		ir_emit(ir, IR_BRANCH, "jz", NULL, end_label);
		push_loop(end_label, begin_label);
		ir_list(ir, n->body);
		pop_loop();
		ir_emit(ir, IR_BRANCH, "jmp", NULL, begin_label);
		ir_emit(ir, IR_LABEL, "L", NULL, end_label);
		return;
	}

	case ND_DO_WHILE: {
		int begin_label = new_ir_label();
		int continue_label = new_ir_label();
		int end_label = new_ir_label();

		ir_emit(ir, IR_LABEL, "L", NULL, begin_label);
		push_loop(end_label, continue_label);
		ir_list(ir, n->body);
		pop_loop();
		ir_emit(ir, IR_LABEL, "L", NULL, continue_label);
		ir_expr(ir, n->cond);
		ir_emit(ir, IR_BRANCH, "jnz", NULL, begin_label);
		ir_emit(ir, IR_LABEL, "L", NULL, end_label);
		return;
	}

	case ND_FOR: {
		int begin_label = new_ir_label();
		int continue_label = new_ir_label();
		int end_label = new_ir_label();

		ir_stmt(ir, n->init);
		ir_emit(ir, IR_LABEL, "L", NULL, begin_label);

		if (n->cond) {
			ir_expr(ir, n->cond);
			ir_emit(ir, IR_BRANCH, "jz", NULL, end_label);
		}

		push_loop(end_label, continue_label);
		ir_list(ir, n->body);
		pop_loop();

		ir_emit(ir, IR_LABEL, "L", NULL, continue_label);
		if (n->inc) {
			ir_expr(ir, n->inc);
			ir_emit(ir, IR_POP, NULL, NULL, 0);
		}
		ir_emit(ir, IR_BRANCH, "jmp", NULL, begin_label);
		ir_emit(ir, IR_LABEL, "L", NULL, end_label);
		return;
	}

	case ND_SWITCH: {
		int end_label = new_ir_label();
		int default_label = end_label;
		int first_case_label = 0;
		int duff_cond_label = 0;

		for (Node *c = n->body; c; c = c->next) {
			c->offset = new_ir_label();
			if (!first_case_label && c->kind == ND_CASE)
				first_case_label = c->offset;
			if (c->kind == ND_DEFAULT)
				default_label = c->offset;
		}

		for (Node *c = n->body; c; c = c->next) {
			if (c->kind != ND_CASE)
				continue;

			ir_expr(ir, n->cond);
			ir_emit(ir, IR_CONST, NULL, NULL, c->value);
			ir_emit(ir, IR_BINOP, "eq", NULL, 0);
			ir_emit(ir, IR_BRANCH, "jnz", NULL, c->offset);
		}

		ir_emit(ir, IR_BRANCH, "jmp", NULL, default_label);

		if (n->inc) {
			duff_cond_label = new_ir_label();
			push_loop(end_label, duff_cond_label);
		} else {
			push_loop(end_label, end_label);
		}

		for (Node *c = n->body; c; c = c->next) {
			ir_emit(ir, IR_LABEL, "L", NULL, c->offset);
			ir_list(ir, c->body);
		}

		if (n->inc) {
			ir_emit(ir, IR_LABEL, "L", NULL, duff_cond_label);
			ir_expr(ir, n->inc);
			ir_emit(ir, IR_BRANCH, "jnz", NULL, first_case_label);
		}

		pop_loop();
		ir_emit(ir, IR_LABEL, "L", NULL, end_label);
		return;
	}

	case ND_CASE:
	case ND_DEFAULT:
		ir_list(ir, n->body);
		return;

	case ND_BREAK:
		ir_emit(ir, IR_BRANCH, "jmp", NULL, current_break_label(ir));
		return;

	case ND_CONTINUE:
		ir_emit(ir, IR_BRANCH, "jmp", NULL, current_continue_label(ir));
		return;

	default:
		/*
		 * Expression statement: evaluate for side effects, then discard the
		 * expression value if one is produced.  Aggregate copies such as
		 *
		 *     y = x;
		 *
		 * are lowered as ND_STRUCT_ASSIGN and emit only stores; they do not
		 * leave a scalar result on the expression stack.  Emitting IR_POP
		 * after them unbalances the stack and can corrupt the next
		 * expression/return value.
		 */
		ir_expr(ir, n);
		if (n->kind != ND_STRUCT_ASSIGN)
			ir_emit(ir, IR_POP, NULL, NULL, 0);
		return;
	}
}


static int
ir_node_can_yield_expression_value(Node *node)
{
	if (!node)
		return 0;

	switch (node->kind) {
	case ND_DECL:
	case ND_ARRAY_DECL:
	case ND_PTR_DECL:
	case ND_STRUCT_DECL:
	case ND_LABEL:
	case ND_GOTO:
	case ND_CASE:
	case ND_DEFAULT:
	case ND_BREAK:
	case ND_CONTINUE:
	case ND_RETURN:
	case ND_IF:
	case ND_WHILE:
	case ND_FOR:
	case ND_DO_WHILE:
	case ND_SWITCH:
	case ND_ASM:
		return 0;
	default:
		return 1;
	}
}

static void
ir_block_expr(IRProgram *ir, Node *block)
{
	Node *last = NULL;

	if (!block || !block->body) {
		ir_emit(ir, IR_CONST, NULL, NULL, 0);
		return;
	}

	for (Node *stmt = block->body; stmt; stmt = stmt->next)
		last = stmt;

	for (Node *stmt = block->body; stmt && stmt != last; stmt = stmt->next)
		ir_stmt(ir, stmt);

	if (ir_node_can_yield_expression_value(last)) {
		ir_expr(ir, last);
	} else {
		ir_stmt(ir, last);
		ir_emit(ir, IR_CONST, NULL, NULL, 0);
	}
}

static void 
ir_expr(IRProgram *ir, Node *node)
{
	if (!node)
		return;

	const char *op = binop_name(node->kind);
	if (op) {
		if ((node->value || node->is_unsigned) &&
		        (node->kind == ND_LT || node->kind == ND_LE || node->kind == ND_GT || node->kind == ND_GE)) {
			if (node->kind == ND_LT)
				op = "ult";
			else if (node->kind == ND_LE)
				op = "ule";
			else if (node->kind == ND_GT)
				op = "ugt";
			else if (node->kind == ND_GE)
				op = "uge";
		}

		if (node->kind == ND_LOGICAL_AND) {
			int false_label = new_ir_label();
			int end_label = new_ir_label();

			/*
			 * C logical AND must short-circuit: the right-hand side is not
			 * evaluated when the left-hand side is false.  This matters for
			 * expressions such as:
			 *
			 *     p && strcmp(p, "hello") == 0
			 *
			 * The old IR lowering evaluated both sides and then combined their
			 * truth values, which dereferenced NULL in cases like the above.
			 */
			ir_expr(ir, node->left);
			ir_emit(ir, IR_BRANCH, "jz", NULL, false_label);

			ir_expr(ir, node->right);
			ir_emit(ir, IR_BRANCH, "jz", NULL, false_label);

			ir_emit(ir, IR_CONST, NULL, NULL, 1);
			ir_emit(ir, IR_BRANCH, "jmp", NULL, end_label);

			ir_emit(ir, IR_LABEL, "L", NULL, false_label);
			ir_emit(ir, IR_CONST, NULL, NULL, 0);

			ir_emit(ir, IR_LABEL, "L", NULL, end_label);
			return;
		}

		if (node->kind == ND_LOGICAL_OR) {
			int true_label = new_ir_label();
			int end_label = new_ir_label();

			/* Logical OR short-circuits when the left-hand side is true. */
			ir_expr(ir, node->left);
			ir_emit(ir, IR_BRANCH, "jnz", NULL, true_label);

			ir_expr(ir, node->right);
			ir_emit(ir, IR_BRANCH, "jnz", NULL, true_label);

			ir_emit(ir, IR_CONST, NULL, NULL, 0);
			ir_emit(ir, IR_BRANCH, "jmp", NULL, end_label);

			ir_emit(ir, IR_LABEL, "L", NULL, true_label);
			ir_emit(ir, IR_CONST, NULL, NULL, 1);

			ir_emit(ir, IR_LABEL, "L", NULL, end_label);
			return;
		}

		ir_expr(ir, node->left);
		ir_expr(ir, node->right);

		if ((node->kind == ND_ADD || node->kind == ND_SUB) && node->is_pointer) {
			int scale = node->elem_size ? node->elem_size : 1;
			ir_emit_ex(ir, IR_BINOP, node->kind == ND_ADD ? "ptradd" : "ptrsub", NULL, 0, scale);
		} else {
			ir_emit(ir, IR_BINOP, op, NULL, 0);
		}

		return;
	}

	switch (node->kind) {
	case ND_BLOCK:
		ir_block_expr(ir, node);
		return;

	case ND_NUM:
		ir_emit(ir, IR_CONST, NULL, NULL, node->value);
		return;

	case ND_VAR:
		/* C array-to-pointer decay: local arrays used as rvalues produce
		 * their address.  Do not emit a scalar load from the first bytes.
		 */
		if (node->type && node->type->kind == TY_ARRAY) {
			ir_emit(ir, IR_LOAD, "addr_local", node->name, node->offset);
			return;
		}
		ir_emit_ex(ir, IR_LOAD, "local", node->name, node->offset,
		           node->is_pointer ? 8 : (node->elem_size ? node->elem_size : 4));
		return;

	case ND_GLOBAL: {
		IRInst *gi = ir_emit(ir, IR_LOAD, "global", node->name, 0);
		gi->is_extern = ir_global_is_extern(node->name);
	}
	return;

	case ND_STRING:
		ir_add_string(ir, node->string_label, node->string_value);
		ir_emit(ir, IR_LOAD, "string", node->string_value, node->string_label);
		return;

	case ND_FUNC_ADDR:
		ir_emit(ir, IR_LOAD, "funcaddr", node->name, 8);
		return;

	case ND_NEG:
		ir_expr(ir, node->left);
		ir_emit(ir, IR_UNOP, "neg", NULL, 0);
		return;

	case ND_BITNOT:
		ir_expr(ir, node->left);
		ir_emit(ir, IR_UNOP, "bitnot", NULL, 0);
		return;

	case ND_NOT:
		ir_expr(ir, node->left);
		ir_emit(ir, IR_UNOP, "not", NULL, 0);
		return;

	case ND_COND: {
		/* ternary: cond ? then_expr : else_expr */
		int else_label = new_ir_label();
		int end_label  = new_ir_label();

		ir_expr(ir, node->cond);
		ir_emit(ir, IR_BRANCH, "jz", NULL, else_label);
		ir_expr(ir, node->then_body);
		ir_emit(ir, IR_BRANCH, "jmp", NULL, end_label);
		ir_emit(ir, IR_LABEL, "L", NULL, else_label);
		ir_expr(ir, node->else_body);
		ir_emit(ir, IR_LABEL, "L", NULL, end_label);
		return;
	}

	case ND_COMMA:
		/* evaluate left as statement (side effect), then produce right as value */
		ir_stmt(ir, node->left);
		ir_expr(ir, node->right);
		return;

	case ND_CAST:
		ir_expr(ir, node->left);
		ir_emit(ir, IR_UNOP, "cast", NULL, node->type ? node->type->size : 4);
		return;

	case ND_CALL: {
		if (node->returns_struct) {
			ir_mark_unsupported(ir, "struct-returning call");
			ir_emit(ir, IR_UNOP, "unsupported", NULL, 0);
			return;
		}

		int count = 0;
		Node *args[32] = {0};

		for (Node *arg = node->args; arg; arg = arg->next) {
			if (count >= 32) {
				ir_mark_unsupported(ir, "too many call arguments");
				ir_emit(ir, IR_UNOP, "unsupported", NULL, 0);
				return;
			}

			args[count++] = arg;
		}

		/*
		 * Fast path for the common one-argument direct call where the
		 * argument can be loaded directly into the accumulator.  On
		 * register-argument ABIs this avoids generating:
		 *
		 *     push arg0
		 *     pop  arg0-register
		 *
		 * Keep this deliberately narrow for now; complex arguments still use
		 * the normal expression stack path below.
		 */
		if (!node->left && count == 1 && args[0]->kind == ND_STRING) {
			ir_add_string(ir, args[0]->string_label, args[0]->string_value);
			ir_emit(ir, IR_LOAD, "string_acc", args[0]->string_value, args[0]->string_label);
			ir_emit_ex(ir, IR_CALL_ACC_ARG0, NULL, node->name, 0, 0);
			return;
		}

		/*
		 * Existing backend call helpers expect the last pushed value to be
		 * arg0. Match the mature AST emitter by evaluating/pushing args
		 * right-to-left.
		 *
		 * For indirect calls, push the callee last so IR_CALL_INDIRECT can
		 * pop/save it before preparing args, without clobbering arg0.
		 */
		for (int i = count - 1; i >= 0; i--)
			ir_expr(ir, args[i]);

		if (node->left) {
			ir_expr(ir, node->left);
			ir_emit_ex(ir, IR_CALL_INDIRECT, NULL, NULL, count, 0);
		} else {
			{
				IRInst *_ci = ir_emit_ex(ir, IR_CALL, NULL, node->name, count, 0);
				if (_ci) _ci->fixed_params = func_fixed_params(node->name);
			}
		}
		return;
	}

	case ND_ASSIGN:
		ir_stmt(ir, node);
		/*
		 * Scalar assignment expressions produce the assigned value by
		 * reloading the lhs.  Struct assignment is lowered as a copy and
		 * currently has no first-class aggregate expression result in this
		 * IR, so do not reload/push a bogus scalar field for aggregate lhs.
		 */
		if (node->left &&
		        (!node->left->type || node->left->type->kind != TY_STRUCT))
			ir_expr(ir, node->left);
		return;

	case ND_PRE_INC:
	case ND_PRE_DEC:
	case ND_POST_INC:
	case ND_POST_DEC:
		ir_incdec_expr(ir, node);
		return;

	case ND_ADDR:
		if (node->left && node->left->kind == ND_VAR) {
			ir_emit(ir, IR_LOAD, "addr_local", node->left->name, node->left->offset);
			return;
		}

		if (node->left && node->left->kind == ND_GLOBAL) {
			ir_emit(ir, IR_LOAD, "addr_global", node->left->name, 0);
			return;
		}

		if (node->left && node->left->kind == ND_DEREF) {
			ir_expr(ir, node->left->left);
			return;
		}

		if (node->left && node->left->kind == ND_INDEX) {
			ir_expr(ir, node->left->left);
			ir_emit_ex(ir, IR_UNOP, "addr_indexed", node->left->name,
			           node->left->offset, node->left->elem_size ? node->left->elem_size : 4);
			return;
		}

		if (node->left && node->left->kind == ND_MEMBER) {
			ir_emit(ir, IR_LOAD, "addr_local", node->left->name, node->left->offset);
			return;
		}

		if (node->left && node->left->kind == ND_MEMBER_PTR) {
			ir_expr(ir, node->left->left);
			ir_emit(ir, IR_UNOP, "add_offset", NULL, node->left->offset);
			return;
		}

		if (node->left && node->left->kind == ND_GLOBAL_INDEX) {
			/* &arr[i] -> address of global array element */
			ir_expr(ir, node->left->left); /* index */
			ir_emit_ex(ir, IR_UNOP, "addr_gidx", node->left->name,
			           0, node->left->elem_size ? node->left->elem_size : 4);
			return;
		}

		ir_mark_unsupported(ir, "address-of complex lvalue");
		ir_emit(ir, IR_UNOP, "unsupported", NULL, 0);
		return;

	case ND_DEREF:
		ir_expr(ir, node->left);
		/* If the dereferenced type is itself an array (e.g. g->init_syms[slot]
		 * where init_syms is char[128][64], giving a char[64] element), the result
		 * decays to a pointer — leave the address on the stack, don't load. */
		if (node->type && node->type->kind == TY_ARRAY)
			return;
		ir_emit(ir, IR_UNOP, "load_deref", NULL, node->elem_size ? node->elem_size : 4);
		return;

	case ND_INDEX:
		ir_expr(ir, node->left);
		ir_emit_ex(ir, IR_UNOP, "load_indexed", node->name,
		           node->offset, node->elem_size ? node->elem_size : 4);
		return;

	case ND_GLOBAL_INDEX:
		ir_expr(ir, node->left);
		if (node->is_array_field) {
			/* Array field: emit address, not load */
			ir_emit_ex(ir, IR_UNOP, "addr_gidx", node->name, 0, 1);
		} else {
			ir_emit_ex(ir, IR_UNOP, "gidx_load", node->name,
			           node->elem_size ? node->elem_size : 4, 0);
		}
		return;

	case ND_MEMBER:
		if (node->is_array_field) {
			/* Local struct array member used as an expression decays to &member[0]. */
			ir_emit(ir, IR_LOAD, "addr_local", node->name, node->offset);
		} else {
			ir_emit_ex(ir, IR_LOAD, "local", node->name, node->offset,
			           node->elem_size ? node->elem_size : 4);
		}
		return;

	case ND_MEMBER_PTR:
		ir_expr(ir, node->left);
		if (node->offset)
			ir_emit(ir, IR_UNOP, "add_offset", NULL, node->offset);
		if (!node->is_array_field)
			ir_emit(ir, IR_UNOP, "load_deref", NULL, node->elem_size ? node->elem_size : 4);
		/* Array fields decay to pointer — just leave the address in acc */
		return;

	case ND_STRUCT_ASSIGN:
		if (!node->left || !node->right) {
			ir_mark_unsupported(ir, "non-local struct assignment");
			ir_emit(ir, IR_UNOP, "unsupported", NULL, 0);
			return;
		}

		/* Handle global = local or local = global cases */
		if ((node->left->kind == ND_VAR || node->left->kind == ND_GLOBAL) &&
		        (node->right->kind == ND_VAR || node->right->kind == ND_GLOBAL)) {
			for (int off = 0; off < node->value; off += 4) {
				/* load source word */
				if (node->right->kind == ND_GLOBAL) {
					ir_emit(ir, IR_LOAD, "addr_global", node->right->name, 0);
					if (off)
						ir_emit(ir, IR_UNOP, "add_offset", NULL, off);
					ir_emit_ex(ir, IR_UNOP, "load_deref", NULL, 4, 0);
				} else {
					ir_emit_ex(ir, IR_LOAD, "local", node->right->name,
					           node->right->offset + off, 4);
				}
				/* store to destination */
				if (node->left->kind == ND_GLOBAL) {
					/* We have value in acc. Save it, compute addr, store. */
					/* Strategy: use saved register for address, acc for value */
					/* Step 1: value is already in acc from load above */
					/* Step 2: push value to stack so we can compute addr */
					ir_emit(ir, IR_CONST, NULL, NULL, 0); /* dummy push to rebalance */
					ir_emit(ir, IR_LOAD, "addr_global", node->left->name, 0);
					if (off)
						ir_emit(ir, IR_UNOP, "add_offset", NULL, off);
					ir_emit(ir, IR_UNOP, "acc_to_saved", NULL, 0); /* save dst addr */
					/* reload value from local */
					ir_emit_ex(ir, IR_LOAD, "local", node->right->name,
					           node->right->offset + off, 4);
					ir_emit_ex(ir, IR_STORE, "via_saved", NULL, 4, 0);
				} else {
					ir_emit_ex(ir, IR_STORE, "local", node->left->name,
					           node->left->offset + off, 4);
				}
			}
			return;
		}

		if (node->left->kind != ND_VAR || node->right->kind != ND_VAR) {
			ir_mark_unsupported(ir, "non-local struct assignment");
			ir_emit(ir, IR_UNOP, "unsupported", NULL, 0);
			return;
		}

		for (int off = 0; off < node->value; off += 4) {
			ir_emit_ex(ir, IR_LOAD, "local", node->right->name,
			           node->right->offset + off, 4);
			ir_emit_ex(ir, IR_STORE, "local", node->left->name,
			           node->left->offset + off, 4);
		}
		return;

	default:
		ir_mark_unsupported(ir, "unhandled AST node in IR lowering");
		ir_emit(ir, IR_UNOP, "unsupported", NULL, 0);
		return;
	}
}

IRProgram *
ir_build(Node *program)
{
	IRProgram *ir = xcalloc(1, sizeof(IRProgram));
	next_ir_label = 1;
	loop_depth = 0;
	ir_list(ir, program);
	return ir;
}

static int 
ir_eval_binop(const char *op, int lhs, int rhs, int *out)
{
	if (STRCMP(op, "add") == 0) *out = lhs + rhs;
	else if (STRCMP(op, "sub") == 0) *out = lhs - rhs;
	else if (STRCMP(op, "mul") == 0) *out = lhs * rhs;
	else if (STRCMP(op, "div") == 0) {
		if (rhs == 0)
			return 0;
		*out = lhs / rhs;
	} else if (STRCMP(op, "and") == 0) *out = lhs & rhs;
	else if (STRCMP(op, "or") == 0) *out = lhs | rhs;
	else if (STRCMP(op, "xor") == 0) *out = lhs ^ rhs;
	else if (STRCMP(op, "shl") == 0) *out = lhs << rhs;
	else if (STRCMP(op, "shr") == 0) *out = lhs >> rhs;
	else if (STRCMP(op, "eq") == 0) *out = lhs == rhs;
	else if (STRCMP(op, "ne") == 0) *out = lhs != rhs;
	else if (STRCMP(op, "lt") == 0) *out = lhs < rhs;
	else if (STRCMP(op, "le") == 0) *out = lhs <= rhs;
	else if (STRCMP(op, "gt") == 0) *out = lhs > rhs;
	else if (STRCMP(op, "ge") == 0) *out = lhs >= rhs;
	else return 0;

	return 1;
}

static int 
ir_eval_unop(const char *op, int value, int *out)
{
	if (STRCMP(op, "neg") == 0) *out = -value;
	else if (STRCMP(op, "not") == 0) *out = !value;
	else if (STRCMP(op, "cast") == 0) *out = value;
	else return 0;

	return 1;
}

static int 
ir_is_side_effect_or_barrier(IRInst *inst)
{
	if (!inst)
		return 0;

	switch (inst->kind) {
	case IR_STORE:
	case IR_CALL:
	case IR_CALL_ACC_ARG0:
	case IR_ASM:
	case IR_BRANCH:
	case IR_RETURN:
	case IR_LABEL:
		return 1;
	default:
		return 0;
	}
}

static IRInst *
ir_prev_inst(IRProgram *program, IRInst *target)
{
	IRInst *prev = NULL;
	for (IRInst *inst = program->head; inst && inst != target; inst = inst->next)
		prev = inst;
	return prev;
}

static int 
ir_prev_is_const(IRProgram *program, IRInst *target, int *value)
{
	IRInst *prev = ir_prev_inst(program, target);
	if (!prev || prev->kind != IR_CONST)
		return 0;

	*value = prev->value;
	return 1;
}

static int 
ir_prev_is_local_load(IRProgram *program, IRInst *target, int *slot)
{
	IRInst *prev = ir_prev_inst(program, target);
	if (!prev || !ir_is_local_scalar_load(prev))
		return 0;

	*slot = prev->value;
	return 1;
}

static int 
ir_valid_copy_slot(int slot)
{
	int idx = -slot;
	return idx >= 0 && idx < 512;
}


static void 
ir_make_const(IRInst *inst, int value)
{
	inst->kind = IR_CONST;
	inst->value = value;
	inst->aux = 0;
	inst->op[0] = '\0';
	inst->name[0] = '\0';
}

static int 
ir_is_local_scalar_store(IRInst *inst)
{
	return inst &&
	       inst->kind == IR_STORE &&
	       STRCMP(inst->op, "local") == 0 &&
	       inst->aux <= 4;
}

static int 
ir_is_local_scalar_load(IRInst *inst)
{
	return inst &&
	       inst->kind == IR_LOAD &&
	       STRCMP(inst->op, "local") == 0 &&
	       inst->aux <= 4;
}

static int ir_is_pure_value_inst(IRInst *inst)
{
	if (!inst)
		return 0;

	switch (inst->kind) {
	case IR_CONST:
	case IR_LOAD:
	case IR_BINOP:
	case IR_UNOP:
		/*
		 * Address computation and loads are pure in this compiler subset.
		 * Calls, stores, branches, returns, labels, pop, and asm are not.
		 */
		return 1;
	default:
		return 0;
	}
}

static int 
ir_next_uses_value(IRInst *inst)
{
	if (!inst)
		return 0;

	switch (inst->kind) {
	case IR_BINOP:
	case IR_UNOP:
	case IR_STORE:
	case IR_RETURN:
	case IR_BRANCH:
	case IR_POP:
	case IR_CALL:
	case IR_CALL_INDIRECT:
	case IR_CALL_ACC_ARG0:
		return 1;
	default:
		return 0;
	}
}

static int 
ir_next_is_value_boundary(IRInst *inst)
{
	if (!inst)
		return 1;

	switch (inst->kind) {
	case IR_LABEL:
	case IR_ASM:
		return 1;
	default:
		return 0;
	}
}

static int 
ir_label_is_referenced(IRProgram *program, int label)
{
	for (IRInst *inst = program->head; inst; inst = inst->next) {
		if (inst->kind == IR_BRANCH && inst->value == label)
			return 1;
	}

	return 0;
}

static int 
ir_try_remove_unreferenced_label(IRProgram *program, IRInst *prev, IRInst *inst)
{
	if (!inst || inst->kind != IR_LABEL)
		return 0;

	if (STRCMP(inst->op, "func") == 0)
		return 0;

	if (ir_label_is_referenced(program, inst->value))
		return 0;

	ir_remove_range(program, prev, inst, inst);
	return 1;
}

static int 
ir_try_merge_adjacent_labels(IRProgram *program, IRInst *first, IRInst *second)
{
	if (!first || !second)
		return 0;

	if (first->kind != IR_LABEL || second->kind != IR_LABEL)
		return 0;

	if (STRCMP(first->op, "func") == 0 || STRCMP(second->op, "func") == 0)
		return 0;

	int old_label = second->value;
	int new_label = first->value;

	for (IRInst *inst = program->head; inst; inst = inst->next) {
		if (inst->kind == IR_BRANCH && inst->value == old_label)
			inst->value = new_label;
	}

	first->next = second->next;
	if (program->tail == second)
		program->tail = first;
	xfree(second);
	return 1;
}

static int 
ir_try_remove_dead_value(IRProgram *program, IRInst *prev, IRInst *inst)
{
	if (!ir_is_pure_value_inst(inst))
		return 0;

	IRInst *next = inst->next;

	/*
	 * Stack-machine IR values are consumed by the immediately following
	 * stack-consuming instruction.  If the next instruction is not a consumer,
	 * this value is dead and can be removed.  This deliberately avoids looking
	 * across labels/asm or trying to remove values before calls/stores.
	 */
	if (ir_next_uses_value(next) || ir_next_is_value_boundary(next))
		return 0;

	ir_remove_range(program, prev, inst, inst);
	return 1;
}

static int 
ir_is_dse_barrier(IRInst *inst)
{
	if (!inst)
		return 0;

	if (inst->kind == IR_LABEL ||
	        inst->kind == IR_BRANCH ||
	        inst->kind == IR_RETURN ||
	        inst->kind == IR_CALL ||
	        inst->kind == IR_CALL_INDIRECT ||
	        inst->kind == IR_CALL_ACC_ARG0 ||
	        inst->kind == IR_ASM)
		return 1;

	if (inst->kind == IR_STORE && STRCMP(inst->op, "local") != 0)
		return 1;

	return 0;
}

static void 
ir_remove_two(IRProgram *program, IRInst *prev, IRInst *first, IRInst *second)
{
	IRInst *after = second->next;

	if (prev)
		prev->next = after;
	else
		program->head = after;

	if (program->tail == second)
		program->tail = prev;

	xfree(first);
	xfree(second);
}

static void 
ir_remove_range(IRProgram *program, IRInst *prev, IRInst *first, IRInst *last)
{
	IRInst *after = last->next;

	if (prev)
		prev->next = after;
	else
		program->head = after;

	if (program->tail == last)
		program->tail = prev;

	for (IRInst *p = first; p != after;) {
		IRInst *next = p->next;
		xfree(p);
		p = next;
	}
}

static int 
ir_try_remove_redundant_store(IRProgram *program, IRInst *prev, IRInst *inst)
{
	if (!ir_is_local_scalar_store(inst))
		return 0;

	int slot = inst->value;
	IRInst *value = ir_prev_inst(program, inst);
	IRInst *next = inst->next;

	/*
	 * v108 load forwarding:
	 *
	 * const C ; store x ; load x -> const C ; store x ; const C
	 *
	 * This preserves the stack value expected by following IR, unlike deleting
	 * the load outright.
	 */
	if (next && ir_is_local_scalar_load(next) && next->value == slot) {
		if (value && value->kind == IR_CONST) {
			ir_make_const(next, value->value);
			return 1;
		}

		if (value && ir_is_local_scalar_load(value)) {
			next->value = value->value;
			return 1;
		}

		return 0;
	}

	if (next && ir_is_local_scalar_store(next) && next->value == slot) {
		ir_remove_range(program, prev, inst, inst);
		return 1;
	}

	return 0;
}

static int 
ir_try_remove_dead_local_store(IRProgram *program, IRInst *prev, IRInst *value, IRInst *store)
{
	if (!value || !store || !ir_is_local_scalar_store(store))
		return 0;

	int slot = store->value;

	for (IRInst *scan = store->next; scan; scan = scan->next) {
		if (ir_is_dse_barrier(scan))
			return 0;

		if (ir_is_local_scalar_load(scan) && scan->value == slot)
			return 0;

		if (ir_is_local_scalar_store(scan) && scan->value == slot) {
			/*
			 * The first store is overwritten before any read/barrier.  The IR
			 * value instruction immediately before it exists only to feed that
			 * store in normal assignment lowering, so remove both.
			 */
			ir_remove_range(program, prev, value, store);
			return 1;
		}
	}

	return 0;
}

static void 
ir_remove_one(IRProgram *program, IRInst *prev, IRInst *inst)
{
	IRInst *next = inst->next;

	if (prev)
		prev->next = next;
	else
		program->head = next;

	if (program->tail == inst)
		program->tail = prev;

	xfree(inst);
}

void 
ir_optimize(IRProgram *program, int level)
{
	if (!program || level <= 0)
		return;

	int changed = 1;
	while (changed) {
		changed = 0;

		/*
		 * v102 dead-store elimination:
		 *
		 * Remove a local scalar store when the same local slot is overwritten
		 * before any load or side-effect/control-flow barrier.  This is
		 * deliberately block-local and conservative.
		 */
		{
			IRInst *prev = NULL;
			IRInst *a = program->head;

			while (a && a->next) {
				IRInst *b = a->next;

				if (ir_try_remove_dead_local_store(program, prev, a, b)) {
					changed = 1;
					a = prev ? prev->next : program->head;
					continue;
				}

				prev = a;
				a = a->next;
			}
		}

		/*
		 * v103 branch simplification:
		 *
		 * Convert constant conditional branches into unconditional branches or
		 * remove them entirely:
		 *
		 *   const 0 ; jz  L -> jmp L
		 *   const 1 ; jz  L -> removed
		 *   const 0 ; jnz L -> removed
		 *   const 1 ; jnz L -> jmp L
		 *
		 * Running this before propagation/folding shrinks the CFG early and
		 * makes later CFG-DCE more effective.
		 */
		{
			IRInst *prev = NULL;
			IRInst *a = program->head;

			while (a && a->next) {
				IRInst *b = a->next;

				if (a->kind == IR_CONST && b->kind == IR_BRANCH &&
				        (STRCMP(b->op, "jz") == 0 || STRCMP(b->op, "jnz") == 0)) {
					int take_branch = (STRCMP(b->op, "jz") == 0) ? (a->value == 0) : (a->value != 0);

					if (take_branch) {
						a->kind = IR_BRANCH;
						STRNCPY(a->op, "jmp", sizeof(a->op) - 1);
						a->op[sizeof(a->op) - 1] = '\0';
						a->value = b->value;
						a->aux = 0;
						a->name[0] = '\0';
						a->next = b->next;
						if (program->tail == b)
							program->tail = a;
						xfree(b);
					} else {
						ir_remove_two(program, prev, a, b);
						a = prev ? prev->next : program->head;
					}

					changed = 1;
					continue;
				}

				prev = a;
				a = a->next;
			}
		}

		/*
		 * v104 pure dead-code elimination:
		 *
		 * Remove pure value-producing instructions whose result is not consumed
		 * by the immediately following stack instruction.  This handles dead
		 * expression statements such as:
		 *
		 *   1;
		 *   x + 2;
		 *
		 * while preserving stores, calls, returns, branches, pop, and asm.
		 */
		{
			IRInst *prev = NULL;
			IRInst *a = program->head;

			while (a) {
				if (ir_try_remove_dead_value(program, prev, a)) {
					changed = 1;
					a = prev ? prev->next : program->head;
					continue;
				}

				prev = a;
				a = a->next;
			}
		}

		/*
		 * v105 label cleanup:
		 *
		 * Collapse adjacent non-function labels and remove unreferenced normal
		 * labels.  Earlier branch simplification and CFG-DCE can leave empty
		 * label islands behind; cleaning them up reduces CFG noise and emitted
		 * assembly labels.
		 */
		{
			IRInst *prev = NULL;
			IRInst *a = program->head;

			while (a) {
				IRInst *b = a->next;

				if (b && ir_try_merge_adjacent_labels(program, a, b)) {
					changed = 1;
					continue;
				}

				if (ir_try_remove_unreferenced_label(program, prev, a)) {
					changed = 1;
					a = prev ? prev->next : program->head;
					continue;
				}

				prev = a;
				a = a->next;
			}
		}

		/*
		 * v108 redundant store/load forwarding:
		 *
		 * Remove adjacent redundant local scalar patterns:
		 *
		 *   store x ; load x  -> store x
		 *   store x ; store x -> store x
		 */
		{
			IRInst *prev = NULL;
			IRInst *a = program->head;

			while (a) {
				if (ir_try_remove_redundant_store(program, prev, a)) {
					changed = 1;
					a = prev ? prev->next : program->head;
					continue;
				}

				prev = a;
				a = a->next;
			}
		}

		/*
		 * v106 copy propagation:
		 *
		 * Track simple local-to-local copies within a basic block:
		 *
		 *   load local x
		 *   store local y
		 *   load local y
		 *
		 * becomes:
		 *
		 *   load local x
		 *
		 * when no barrier or overwrite intervenes.  This is intentionally
		 * conservative and block-local.
		 */
		{
			int copy_known[512] = {0};
			int copy_source[512] = {0};

			for (IRInst *p = program->head; p; p = p->next) {
				if (p->kind == IR_LABEL || p->kind == IR_BRANCH ||
				        p->kind == IR_RETURN || p->kind == IR_CALL || p->kind == IR_CALL_ACC_ARG0 ||
				        p->kind == IR_ASM) {
					memset(copy_known, 0, sizeof(copy_known));
					continue;
				}

				if (ir_is_local_scalar_load(p)) {
					int dst = p->value;
					int dst_idx = -dst;

					if (dst_idx >= 0 && dst_idx < 512 && copy_known[dst_idx]) {
						p->value = copy_source[dst_idx];
						changed = 1;
					}
					continue;
				}

				if (ir_is_local_scalar_store(p)) {
					int dst = p->value;
					int dst_idx = -dst;
					int src = 0;

					/*
					 * Any store to a slot invalidates copies sourced from that
					 * slot.  Otherwise later loads could incorrectly read an
					 * old value through an aliasing copy.
					 */
					for (int i = 0; i < 512; i++) {
						if (copy_known[i] && copy_source[i] == dst)
							copy_known[i] = 0;
					}

					if (dst_idx >= 0 && dst_idx < 512 &&
					        ir_prev_is_local_load(program, p, &src) &&
					        src != dst &&
					        ir_valid_copy_slot(src)) {
						copy_known[dst_idx] = 1;
						copy_source[dst_idx] = src;
					} else if (dst_idx >= 0 && dst_idx < 512) {
						copy_known[dst_idx] = 0;
					}

					continue;
				}

				if (p->kind == IR_STORE) {
					memset(copy_known, 0, sizeof(copy_known));
					continue;
				}
			}
		}

		/*
		 * v94.1 propagation-lite:
		 *
		 * Track constant stores to local stack slots until a label/control-flow
		 * boundary or unknown store/call.  Replace subsequent local loads with
		 * constants when safe.  This deliberately avoids global dataflow.
		 */
		{
			int local_const_known[512] = {0};
			int local_const_value[512] = {0};

			for (IRInst *p = program->head; p; p = p->next) {
				if (p->kind == IR_LABEL || p->kind == IR_BRANCH ||
				        p->kind == IR_CALL || p->kind == IR_CALL_ACC_ARG0 || p->kind == IR_ASM) {
					memset(local_const_known, 0, sizeof(local_const_known));
					continue;
				}

				if (p->kind == IR_STORE && STRCMP(p->op, "local") == 0) {
					int slot = -p->value;
					if (slot >= 0 && slot < 512 && p->aux <= 4) {
						/*
						 * See if the immediately preceding instruction is a
						 * constant value being stored.
						 */
						IRInst *prev2 = NULL;
						for (IRInst *q = program->head; q && q != p; q = q->next)
							prev2 = q;

						if (prev2 && prev2->kind == IR_CONST) {
							local_const_known[slot] = 1;
							local_const_value[slot] = prev2->value;
						} else {
							local_const_known[slot] = 0;
						}
					}
					continue;
				}

				if (p->kind == IR_STORE) {
					memset(local_const_known, 0, sizeof(local_const_known));
					continue;
				}

				if (p->kind == IR_LOAD && STRCMP(p->op, "local") == 0) {
					int slot = -p->value;
					if (slot >= 0 && slot < 512 && local_const_known[slot] && p->aux <= 4) {
						p->kind = IR_CONST;
						p->value = local_const_value[slot];
						p->aux = 0;
						p->op[0] = '\0';
						p->name[0] = '\0';
						changed = 1;
					}
				}
			}
		}

		/*
		 * v101 block-local constant folding / propagation:
		 *
		 * This pass tracks simple scalar local constants within a basic block.
		 * Labels, branches, calls, asm, returns, and non-local stores are
		 * barriers.  It improves on v94.1 by using the previous IR instruction
		 * as the value source for each local store, then allowing normal
		 * const/const/binop folding to collapse the resulting expressions.
		 */
		{
			int known[512] = {0};
			int values[512] = {0};

			for (IRInst *p = program->head; p; p = p->next) {
				if (ir_is_side_effect_or_barrier(p)) {
					if (!(p->kind == IR_STORE && STRCMP(p->op, "local") == 0))
						memset(known, 0, sizeof(known));
				}

				if (p->kind == IR_LOAD && STRCMP(p->op, "local") == 0 && p->aux <= 4) {
					int slot = -p->value;
					if (slot >= 0 && slot < 512 && known[slot]) {
						ir_make_const(p, values[slot]);
						changed = 1;
					}
					continue;
				}

				if (p->kind == IR_STORE && STRCMP(p->op, "local") == 0 && p->aux <= 4) {
					int slot = -p->value;
					int value = 0;

					if (slot >= 0 && slot < 512 && ir_prev_is_const(program, p, &value)) {
						known[slot] = 1;
						values[slot] = value;
					} else if (slot >= 0 && slot < 512) {
						known[slot] = 0;
					}
					continue;
				}

				if (p->kind == IR_STORE) {
					memset(known, 0, sizeof(known));
					continue;
				}
			}
		}

		IRInst *prev = NULL;
		IRInst *a = program->head;

		while (a) {
			IRInst *b = a->next;
			IRInst *c = b ? b->next : NULL;

			/*
			 * Constant fold stack-machine triples:
			 *
			 *   const A
			 *   const B
			 *   add/mul/...
			 *
			 * into:
			 *
			 *   const RESULT
			 */
			if (a->kind == IR_CONST && b && b->kind == IR_CONST && c && c->kind == IR_BINOP) {
				int result = 0;
				if (ir_eval_binop(c->op, a->value, b->value, &result)) {
					IRInst *replacement = xcalloc(1, sizeof(IRInst));

					replacement->kind = IR_CONST;
					replacement->id = program->next_id++;
					replacement->value = result;
					replacement->next = c->next;

					if (prev)
						prev->next = replacement;
					else
						program->head = replacement;

					if (program->tail == c)
						program->tail = replacement;

					xfree(a);
					xfree(b);
					xfree(c);

					changed = 1;
					a = replacement;
					continue;
				}
			}

			/*
			 * Constant fold unary pairs:
			 *
			 *   const A
			 *   neg/not/cast
			 */
			if (a->kind == IR_CONST && b && b->kind == IR_UNOP) {
				int result = 0;
				if (ir_eval_unop(b->op, a->value, &result)) {
					a->value = result;
					a->next = b->next;
					if (program->tail == b)
						program->tail = a;
					xfree(b);
					changed = 1;
					continue;
				}
			}

			/*
			 * v94 stack-machine peepholes for identity operations where one
			 * side is a literal and the other side may be an arbitrary
			 * expression result immediately before the literal.
			 *
			 * Examples:
			 *
			 *   expr
			 *   const 0
			 *   add        -> expr
			 *
			 *   expr
			 *   const 1
			 *   mul        -> expr
			 *
			 * These reduce instruction count before backend codegen, which in
			 * turn eliminates push/pop traffic in generated assembly.
			 */
			if (b && c && b->kind == IR_CONST && c->kind == IR_BINOP) {
				if ((STRCMP(c->op, "add") == 0 || STRCMP(c->op, "sub") == 0 ||
				        STRCMP(c->op, "or") == 0 || STRCMP(c->op, "xor") == 0 ||
				        STRCMP(c->op, "shl") == 0 || STRCMP(c->op, "shr") == 0) &&
				        b->value == 0) {
					a->next = c->next;
					if (program->tail == c)
						program->tail = a;
					xfree(b);
					xfree(c);
					changed = 1;
					continue;
				}

				if (STRCMP(c->op, "mul") == 0 && b->value == 1) {
					a->next = c->next;
					if (program->tail == c)
						program->tail = a;
					xfree(b);
					xfree(c);
					changed = 1;
					continue;
				}

				if ((STRCMP(c->op, "mul") == 0 || STRCMP(c->op, "and") == 0) &&
				        b->value == 0) {
					/*
					 * expr, const 0, mul/and -> const 0
					 *
					 * This is safe only for our side-effect-free expression IR
					 * prefix at this stage.  Avoid applying it after calls or
					 * stores by requiring the left instruction itself to be a
					 * pure value producer.
					 */
					if (a->kind == IR_CONST || a->kind == IR_LOAD || a->kind == IR_UNOP || a->kind == IR_BINOP) {
						a->kind = IR_CONST;
						a->value = 0;
						a->aux = 0;
						a->op[0] = '\0';
						a->name[0] = '\0';
						a->next = c->next;
						if (program->tail == c)
							program->tail = a;
						xfree(b);
						xfree(c);
						changed = 1;
						continue;
					}
				}
			}

			/*
			 * Literal-left commutative identities:
			 *
			 *   const 0
			 *   expr
			 *   add/or/xor -> expr
			 *
			 * We can remove the leading literal and binop, leaving expr.
			 */
			if (a->kind == IR_CONST && c && c->kind == IR_BINOP &&
			        (STRCMP(c->op, "add") == 0 ||
			         STRCMP(c->op, "or") == 0 ||
			         STRCMP(c->op, "xor") == 0 ||
			         STRCMP(c->op, "mul") == 0 ||
			         STRCMP(c->op, "and") == 0)) {
				if (((STRCMP(c->op, "add") == 0 || STRCMP(c->op, "or") == 0 || STRCMP(c->op, "xor") == 0) && a->value == 0) ||
				        (STRCMP(c->op, "mul") == 0 && a->value == 1)) {
					/*
					 * Drop a and c.  b becomes the first instruction in the
					 * replacement window.
					 */
					if (prev)
						prev->next = b;
					else
						program->head = b;
					b->next = c->next;
					if (program->tail == c)
						program->tail = b;
					xfree(a);
					xfree(c);
					changed = 1;
					a = b;
					continue;
				}

				if ((STRCMP(c->op, "mul") == 0 || STRCMP(c->op, "and") == 0) && a->value == 0 &&
				        (b->kind == IR_CONST || b->kind == IR_LOAD || b->kind == IR_UNOP || b->kind == IR_BINOP)) {
					a->next = c->next;
					if (program->tail == c)
						program->tail = a;
					xfree(b);
					xfree(c);
					changed = 1;
					continue;
				}
			}

			/*
			 * v96 dead-store-lite:
			 *
			 *   const A
			 *   store local X
			 *   const B
			 *   store local X
			 *
			 * -> const B ; store local X
			 */
			if (a->kind == IR_CONST && b && b->kind == IR_STORE &&
			        STRCMP(b->op, "local") == 0 && c && c->kind == IR_CONST &&
			        c->next && c->next->kind == IR_STORE &&
			        STRCMP(c->next->op, "local") == 0 &&
			        b->value == c->next->value) {
				IRInst *d = c->next;

				if (prev)
					prev->next = c;
				else
					program->head = c;

				xfree(a);
				xfree(b);
				changed = 1;
				a = c;
				(void)d;
				continue;
			}

			/*
			 * Branch peepholes:
			 *
			 *   const 0 ; jz  L  -> jmp L
			 *   const 1 ; jz  L  -> delete both
			 *   const 0 ; jnz L  -> delete both
			 *   const 1 ; jnz L  -> jmp L
			 */
			if (a->kind == IR_CONST && b && b->kind == IR_BRANCH &&
			        (STRCMP(b->op, "jz") == 0 || STRCMP(b->op, "jnz") == 0)) {
				int take_branch = (STRCMP(b->op, "jz") == 0) ? (a->value == 0) : (a->value != 0);

				if (take_branch) {
					a->kind = IR_BRANCH;
					STRNCPY(a->op, "jmp", sizeof(a->op) - 1);
					a->value = b->value;
					a->aux = 0;
					a->name[0] = '\0';
					a->next = b->next;
					if (program->tail == b)
						program->tail = a;
					xfree(b);
				} else {
					IRInst *after = b->next;
					if (prev)
						prev->next = after;
					else
						program->head = after;
					if (program->tail == b)
						program->tail = prev;
					xfree(a);
					xfree(b);
					a = after;
				}

				changed = 1;
				continue;
			}

			/*
			 * Remove jumps to immediately following labels.
			 */
			if (a->kind == IR_BRANCH && STRCMP(a->op, "jmp") == 0 &&
			        b && b->kind == IR_LABEL && STRCMP(b->op, "L") == 0 &&
			        a->value == b->value) {
				ir_remove_one(program, prev, a);
				changed = 1;
				a = b;
				continue;
			}

			/*
			 * Remove unreachable linear instructions between an unconditional
			 * jump/return and the next label.
			 */
			if ((a->kind == IR_BRANCH && STRCMP(a->op, "jmp") == 0) ||
			        a->kind == IR_RETURN) {
				IRInst *first_dead = a->next;
				IRInst *last_dead = NULL;
				IRInst *scan = first_dead;

				while (scan && scan->kind != IR_LABEL) {
					last_dead = scan;
					scan = scan->next;
				}

				if (first_dead && last_dead) {
					IRInst *after = last_dead->next;
					a->next = after;
					if (program->tail == last_dead)
						program->tail = a;

					for (IRInst *p = first_dead; p != after;) {
						IRInst *next = p->next;
						xfree(p);
						p = next;
					}

					changed = 1;
					continue;
				}
			}

			prev = a;
			a = a->next;
		}
	}

	if (level >= 2)
		ir_cfg_eliminate_unreachable(program);

}

static int 
ir_is_block_terminator(IRInst *inst)
{
	if (!inst)
		return 0;

	if (inst->kind == IR_RETURN)
		return 1;

	if (inst->kind == IR_BRANCH)
		return 1;

	return 0;
}

static int 
ir_starts_block(IRInst *inst, int need_new_block)
{
	if (!inst)
		return 0;

	if (inst->kind == IR_LABEL)
		return 1;

	return need_new_block;
}

static void 
ir_free_cfg(IRProgram *program)
{
	IRBlock *block = program->blocks;
	while (block) {
		IRBlock *next = block->next;
		xfree(block);
		block = next;
	}

	program->blocks = NULL;
	program->block_count = 0;
}

static IRBlock *
ir_new_block(IRProgram *program, IRInst *first)
{
	IRBlock *block = xcalloc(1, sizeof(IRBlock));
	block->id = program->block_count++;
	block->label = -1;
	block->first = first;

	if (first && first->kind == IR_LABEL && STRCMP(first->op, "L") == 0)
		block->label = first->value;
	else if (first && first->kind == IR_LABEL && STRCMP(first->op, "func") == 0)
		block->label = -1000 - block->id;

	if (!program->blocks)
		program->blocks = block;
	else {
		IRBlock *tail = program->blocks;
		while (tail->next)
			tail = tail->next;
		tail->next = block;
	}

	return block;
}

static IRBlock *
ir_find_block_by_label(IRProgram *program, int label)
{
	for (IRBlock *block = program->blocks; block; block = block->next) {
		if (block->label == label)
			return block;
	}

	return NULL;
}

static IRBlock *
ir_next_block(IRBlock *block)
{
	return block ? block->next : NULL;
}

void 
ir_build_cfg(IRProgram *program)
{
	if (!program)
		return;

	ir_free_cfg(program);

	IRBlock *current = NULL;
	int need_new_block = 1;

	for (IRInst *inst = program->head; inst; inst = inst->next) {
		if (!current || ir_starts_block(inst, need_new_block)) {
			current = ir_new_block(program, inst);
			need_new_block = 0;
		}

		current->last = inst;

		if (ir_is_block_terminator(inst))
			need_new_block = 1;
	}

	for (IRBlock *block = program->blocks; block; block = block->next) {
		IRInst *last = block->last;

		if (!last)
			continue;

		if (last->kind == IR_BRANCH) {
			if (STRCMP(last->op, "jmp") == 0) {
				block->branch = ir_find_block_by_label(program, last->value);
			} else if (STRCMP(last->op, "jz") == 0 || STRCMP(last->op, "jnz") == 0) {
				block->branch = ir_find_block_by_label(program, last->value);
				block->fallthrough = ir_next_block(block);
			}
		} else if (last->kind != IR_RETURN) {
			block->fallthrough = ir_next_block(block);
		}
	}
}

static void 
ir_dump_block_name(IRBlock *block)
{
	if (!block) {
		printf("none");
		return;
	}

	printf("B%d", block->id);
	if (block->label >= 0)
		printf("(L%d)", block->label);
}

static void 
ir_mark_reachable_blocks(IRBlock *block, int *reachable, int count)
{
	if (!block || block->id < 0 || block->id >= count)
		return;

	if (reachable[block->id])
		return;

	reachable[block->id] = 1;

	ir_mark_reachable_blocks(block->fallthrough, reachable, count);
	ir_mark_reachable_blocks(block->branch, reachable, count);
}

static int 
ir_block_is_function_entry(IRBlock *block)
{
	return block &&
	       block->first &&
	       block->first->kind == IR_LABEL &&
	       STRCMP(block->first->op, "func") == 0;
}

static IRInst *
ir_find_prev_inst(IRProgram *program, IRInst *target)
{
	IRInst *prev = NULL;

	for (IRInst *inst = program->head; inst && inst != target; inst = inst->next)
		prev = inst;

	return prev;
}

void 
ir_cfg_eliminate_unreachable(IRProgram *program)
{
	if (!program)
		return;

	ir_build_cfg(program);

	if (program->block_count <= 0)
		return;

	int *reachable = xcalloc((size_t)program->block_count, sizeof(int));
	/*
	 * Treat the first block and every function-entry block as roots.  The IR
	 * is a whole-program function list, not a single function body, so a return
	 * from one function should not make the next function unreachable.
	 */
	if (program->blocks)
		ir_mark_reachable_blocks(program->blocks, reachable, program->block_count);

	for (IRBlock *block = program->blocks; block; block = block->next) {
		if (ir_block_is_function_entry(block))
			ir_mark_reachable_blocks(block, reachable, program->block_count);
	}

	for (IRBlock *block = program->blocks; block; block = block->next) {
		if (block->id < 0 || block->id >= program->block_count || reachable[block->id])
			continue;

		IRInst *first = block->first;
		IRInst *last = block->last;
		IRInst *after = last ? last->next : NULL;
		IRInst *prev = ir_find_prev_inst(program, first);

		if (prev)
			prev->next = after;
		else
			program->head = after;

		if (program->tail == last)
			program->tail = prev;

		for (IRInst *inst = first; inst != after;) {
			IRInst *next = inst->next;
			xfree(inst);
			inst = next;
		}
	}

	xfree(reachable);
	ir_build_cfg(program);

}

void 
ir_dump_cfg(IRProgram *program)
{
	if (!program)
		return;

	ir_build_cfg(program);

	printf("; cfg blocks=%d\n", program->block_count);

	for (IRBlock *block = program->blocks; block; block = block->next) {
		printf("B%d", block->id);

		if (block->first && block->first->kind == IR_LABEL && STRCMP(block->first->op, "func") == 0)
			printf(" func=%s", block->first->name);
		else if (block->label >= 0)
			printf(" label=L%d", block->label);

		printf(" first=%%%d last=%%%d", block->first ? block->first->id : -1,
		       block->last ? block->last->id : -1);

		printf(" fallthrough=");
		ir_dump_block_name(block->fallthrough);

		printf(" branch=");
		ir_dump_block_name(block->branch);

		printf("\n");
	}
}

void 
ir_dump(IRProgram *program)
{
	if (program->unsupported_count)
		printf("; unsupported=%d reason=\"%s\"\n", program->unsupported_count, program->unsupported_reason);

	for (IRInst *inst = program->head; inst; inst = inst->next) {
		printf("%%%d = ", inst->id);

		switch (inst->kind) {
		case IR_CONST:
			printf("const %d", inst->value);
			break;
		case IR_LOAD:
			printf("load %s %s value=%d", inst->op, inst->name, inst->value);
			if (inst->aux)
				printf(" size=%d", inst->aux);
			break;
		case IR_STORE:
			printf("store %s %s value=%d", inst->op, inst->name, inst->value);
			if (inst->aux)
				printf(" size=%d", inst->aux);
			break;
		case IR_BINOP:
			printf("%s", inst->op);
			if (inst->aux)
				printf(" scale=%d", inst->aux);
			break;
		case IR_UNOP:
			printf("%s", inst->op);
			if (inst->value)
				printf(" value=%d", inst->value);
			if (inst->aux)
				printf(" aux=%d", inst->aux);
			break;
		case IR_RETURN:
			printf("return");
			break;
		case IR_LABEL:
			if (STRCMP(inst->op, "func") == 0)
				printf("label func %s stack=%d params=%d", inst->name, inst->value, inst->aux);
			else
				printf("label L%d", inst->value);
			break;
		case IR_BRANCH:
			printf("branch %s L%d", inst->op, inst->value);
			break;
		case IR_CALL:
			printf("call %s argc=%d", inst->name, inst->value);
			break;
		case IR_CALL_INDIRECT:
			printf("call_indirect argc=%d", inst->value);
			break;
		case IR_CALL_ACC_ARG0:
			printf("call_acc_arg0 %s", inst->name);
			break;
		case IR_ASM:
			printf("asm%s \"%s\"", inst->aux ? " volatile" : "", inst->name);
			break;
		case IR_POP:
			printf("pop");
			break;
		}

		printf("\n");
	}
}

void 
ir_free(IRProgram *program)
{
	if (!program)
		return;

	IRInst *inst = program->head;
	while (inst) {
		IRInst *next = inst->next;
		xfree(inst);
		inst = next;
	}

	IRString *str = program->strings;
	while (str) {
		IRString *next = str->next;
		xfree(str);
		str = next;
	}

	xfree(program);
}

static int __attribute__((unused)) 
ir_supported_binop(const char *op)
{
	return STRCMP(op, "add") == 0 ||
	       STRCMP(op, "sub") == 0 ||
	       STRCMP(op, "mul") == 0 ||
	       STRCMP(op, "div") == 0 ||
	       STRCMP(op, "and") == 0 ||
	       STRCMP(op, "or") == 0 ||
	       STRCMP(op, "xor") == 0 ||
	       STRCMP(op, "shl") == 0 ||
	       STRCMP(op, "shr") == 0 ||
	       STRCMP(op, "eq") == 0 ||
	       STRCMP(op, "ne") == 0 ||
	       STRCMP(op, "lt") == 0 ||
	       STRCMP(op, "le") == 0 ||
	       STRCMP(op, "gt") == 0 ||
	       STRCMP(op, "ge") == 0 ||
	       STRCMP(op, "ult") == 0 ||
	       STRCMP(op, "ule") == 0 ||
	       STRCMP(op, "ugt") == 0 ||
	       STRCMP(op, "uge") == 0 ||
	       STRCMP(op, "land") == 0 ||
	       STRCMP(op, "lor") == 0 ||
	       STRCMP(op, "ptradd") == 0 ||
	       STRCMP(op, "ptrsub") == 0;
}

static int __attribute__((unused)) 
ir_supported_unop(const char *op)
{
	return STRCMP(op, "neg") == 0 ||
	       STRCMP(op, "not") == 0 ||
	       STRCMP(op, "cast") == 0 ||
	       STRCMP(op, "load_deref") == 0 ||
	       STRCMP(op, "load_indexed") == 0 ||
	       STRCMP(op, "addr_indexed") == 0 ||
	       STRCMP(op, "gidx_load") == 0 ||
	       STRCMP(op, "addr_gidx") == 0 ||
	       STRCMP(op, "add_offset") == 0 ||
	       STRCMP(op, "bitnot") == 0 ||
	       STRCMP(op, "ptr_add") == 0 ||
	       STRCMP(op, "ptr_sub") == 0 ||
	       STRCMP(op, "acc_to_saved") == 0 ||
	       STRCMP(op, "load_via_saved") == 0;
}

int 
ir_can_codegen(IRProgram *program)
{
	return program->unsupported_count == 0;
}

static void 
ir_emit_binop(Codegen *cg, const char *op, int aux)
{
	if (STRCMP(op, "add") == 0)
		cg->emit_add();
	else if (STRCMP(op, "sub") == 0)
		cg->emit_sub();
	else if (STRCMP(op, "mul") == 0)
		cg->emit_mul();
	else if (STRCMP(op, "div") == 0)
		cg->emit_div();
	else if (STRCMP(op, "mod") == 0)
		cg->emit_mod();
	else if (STRCMP(op, "and") == 0)
		cg->emit_bitand();
	else if (STRCMP(op, "or") == 0)
		cg->emit_bitor();
	else if (STRCMP(op, "xor") == 0)
		cg->emit_bitxor();
	else if (STRCMP(op, "shl") == 0)
		cg->emit_shl();
	else if (STRCMP(op, "shr") == 0)
		cg->emit_shr();
	else if (STRCMP(op, "eq") == 0)
		cg->emit_cmp_eq();
	else if (STRCMP(op, "ne") == 0)
		cg->emit_cmp_ne();
	else if (STRCMP(op, "lt") == 0)
		cg->emit_cmp_lt();
	else if (STRCMP(op, "le") == 0)
		cg->emit_cmp_le();
	else if (STRCMP(op, "gt") == 0)
		cg->emit_cmp_gt();
	else if (STRCMP(op, "ge") == 0)
		cg->emit_cmp_ge();
	else if (STRCMP(op, "ult") == 0 && cg->emit_cmp_lt_u)
		cg->emit_cmp_lt_u();
	else if (STRCMP(op, "ule") == 0 && cg->emit_cmp_le_u)
		cg->emit_cmp_le_u();
	else if (STRCMP(op, "ugt") == 0 && cg->emit_cmp_gt_u)
		cg->emit_cmp_gt_u();
	else if (STRCMP(op, "uge") == 0 && cg->emit_cmp_ge_u)
		cg->emit_cmp_ge_u();
	else if (STRCMP(op, "land") == 0)
		cg->emit_bitand();
	else if (STRCMP(op, "lor") == 0)
		cg->emit_bitor();
	else if (STRCMP(op, "ptradd") == 0)
		cg->emit_ptr_add(aux ? aux : 1);
	else if (STRCMP(op, "ptrsub") == 0)
		cg->emit_ptr_sub(aux ? aux : 1);
	else {
		ICE("Unsupported IR binop: %s", op);
	}
}

static void 
ir_emit_unop(Codegen *cg, const char *op, int value, int aux)
{
	if (STRCMP(op, "neg") == 0) {
		cg->emit_pop_to_acc();
		cg->emit_negate();
	} else if (STRCMP(op, "not") == 0) {
		cg->emit_load_imm(0);
		cg->emit_pop_to_tmp();
		cg->emit_cmp_eq();
	} else if (STRCMP(op, "cast") == 0) {
		cg->emit_pop_to_acc();
		cg->emit_cast(value ? value : 4);
	} else if (STRCMP(op, "ptr_copy") == 0) {
		/* acc=src ptr, saved=dst ptr. Copy value bytes word by word.
		 * Calls the backend's emit_ptr_copy which has both ptrs. */
		cg->emit_ptr_copy(value);
	} else if (STRCMP(op, "load_deref") == 0) {
		cg->emit_pop_to_acc();
		cg->emit_load_deref(value ? value : 4);
	} else if (STRCMP(op, "load_indexed") == 0) {
		cg->emit_pop_to_acc();
		cg->emit_load_indexed(value, aux ? aux : 4);
	} else if (STRCMP(op, "addr_indexed") == 0) {
		cg->emit_pop_to_acc();
		cg->emit_addr_indexed(value, aux ? aux : 4);
	} else if (STRCMP(op, "gidx_load") == 0) {
		ICE("Named global indexed load must be emitted by IR_UNOP case");
	} else if (STRCMP(op, "add_offset") == 0) {
		cg->emit_pop_to_acc();
		cg->emit_add_offset(value);
	} else if (STRCMP(op, "acc_to_saved") == 0) {
		cg->emit_acc_to_saved();
	} else if (STRCMP(op, "load_via_saved") == 0) {
		cg->emit_load_via_saved(value);
	} else if (STRCMP(op, "bitnot") == 0) {
		cg->emit_pop_to_acc();
		cg->emit_bitnot();
	} else if (STRCMP(op, "ptr_add") == 0) {
		cg->emit_pop_to_acc();
		cg->emit_ptr_add(aux ? aux : 1);
	} else if (STRCMP(op, "ptr_sub") == 0) {
		cg->emit_pop_to_acc();
		cg->emit_ptr_sub(aux ? aux : 1);
	} else if (STRCMP(op, "addr_gidx") == 0) {
		/* handled in IR_UNOP named dispatch above */
		ICE("addr_gidx must be emitted by IR_UNOP named case");
	} else {
		ICE("Unsupported IR unop: %s", op);
	}
}

void 
ir_emit_program(IRProgram *program, Codegen *cg)
{
	int in_function = 0;
	const char *current_func_name = NULL;
	int last_emitted_line = 0;
	int last_emitted_filename_id = -1;

	cg->emit_preamble();

	ir_emit_global_data(cg);

	for (IRString *s = program->strings; s; s = s->next)
		cg->emit_string_literal(s->label, s->value);

	for (IRInst *inst = program->head; inst; inst = inst->next) {
		/* Emit source location if it changed and codegen supports it */
		if (cg->emit_source_loc && inst->src_line > 0 &&
		    (inst->src_line != last_emitted_line ||
		     inst->src_filename_id != last_emitted_filename_id)) {
			cg->emit_source_loc(
				lexer_filename_name(inst->src_filename_id),
				inst->src_line);
			last_emitted_line = inst->src_line;
			last_emitted_filename_id = inst->src_filename_id;
		}

		switch (inst->kind) {
		case IR_LABEL:
			if (STRCMP(inst->op, "func") == 0) {
				if (in_function) {
					if (current_func_name && STRCMP(current_func_name, "main") == 0)
						cg->emit_load_imm(0);
					cg->emit_function_end();
				}
				in_function = 1;
				current_func_name = inst->name;

				cg->emit_function_start(inst->name, inst->is_static);
				cg->emit_stack_alloc(inst->value);
				ir_user_label_count = 0;

				for (int i = 0; i < inst->aux; i++) {
					int offset = -(i + 1) * 8;
					cg->emit_store_param(i, offset);
				}
			} else if (inst->op[0] == '\0' && inst->name[0] != '\0') {
				cg->emit_label(ir_get_user_label_id(inst->name));
			} else {
				cg->emit_label(inst->value);
			}
			break;

		case IR_CONST:
			cg->emit_load_imm(inst->value);
			cg->emit_push_acc();
			break;

		case IR_LOAD:
			if (STRCMP(inst->op, "local") == 0)
				cg->emit_load_local_sized(inst->value, inst->aux ? inst->aux : 4);
			else if (STRCMP(inst->op, "addr_local") == 0)
				cg->emit_addr_local(inst->value);
			else if (STRCMP(inst->op, "addr_global") == 0)
				cg->emit_load_func_addr(inst->name); /* reuse: loads symbol address */
			else if (STRCMP(inst->op, "global") == 0) {
				int is_ext = inst->is_extern;
				if (is_ext)
					cg->emit_load_global_extern(inst->name);
				else
					cg->emit_load_global(inst->name, ir_global_elem_size(inst->name));
			} else if (STRCMP(inst->op, "string") == 0)
				cg->emit_load_string(inst->value);
			else if (STRCMP(inst->op, "string_acc") == 0) {
				cg->emit_load_string(inst->value);
				break;
			} else if (STRCMP(inst->op, "funcaddr") == 0)
				cg->emit_load_func_addr(inst->name);
			cg->emit_push_acc();
			break;

		case IR_STORE:
			if (STRCMP(inst->op, "indexed") == 0) {
				cg->emit_pop_to_acc();  /* value */
				cg->emit_pop_to_tmp();  /* index */
				cg->emit_store_indexed(inst->value, inst->aux ? inst->aux : 4);
			} else if (STRCMP(inst->op, "gidx_store") == 0) {
				cg->emit_pop_to_acc();  /* value */
				cg->emit_pop_to_tmp();  /* index */
				cg->emit_store_global_indexed(inst->name, inst->value ? inst->value : 4);
			} else if (STRCMP(inst->op, "deref") == 0) {
				cg->emit_pop_to_acc();  /* value */
				cg->emit_pop_to_tmp();  /* address */
				cg->emit_store_deref(inst->value ? inst->value : 4);
			} else if (STRCMP(inst->op, "member_ptr") == 0) {
				cg->emit_pop_to_acc();  /* value */
				cg->emit_pop_to_tmp();  /* base ptr */
				cg->emit_store_member_ptr(inst->value, inst->aux ? inst->aux : 4);
			} else if (STRCMP(inst->op, "via_saved") == 0) {
				/* *saved_reg = acc; saved_reg holds address */
				cg->emit_store_via_saved(inst->value);
			} else {
				cg->emit_pop_to_acc();
				if (STRCMP(inst->op, "local") == 0)
					cg->emit_store_local_sized(inst->value, inst->aux ? inst->aux : 4);
				else if (STRCMP(inst->op, "global") == 0) {
					if (inst->is_extern)
						cg->emit_store_global_extern(inst->name);
					else
						cg->emit_store_global(inst->name, ir_global_elem_size(inst->name));
				}
			}
			break;

		case IR_BINOP:
			cg->emit_pop_to_acc();
			cg->emit_pop_to_tmp();
			ir_emit_binop(cg, inst->op, inst->aux);
			cg->emit_push_acc();
			break;

		case IR_UNOP:
			if (STRCMP(inst->op, "gidx_load") == 0) {
				cg->emit_pop_to_acc();
				cg->emit_load_global_indexed(inst->name, inst->value ? inst->value : 4);
			} else if (STRCMP(inst->op, "addr_gidx") == 0) {
				/* ptr_add expects: x0=index, x1=base.
				 * Load base first into x0, save to x1 (tmp),
				 * then pop the index into x0. */
				cg->emit_load_func_addr(inst->name); /* x0 = base addr */
				cg->emit_acc_to_tmp();               /* x1 = base */
				cg->emit_pop_to_acc();               /* x0 = index */
				cg->emit_ptr_add(inst->aux ? inst->aux : 4); /* x0 = base + index*scale */
			} else if (STRCMP(inst->op, "acc_to_saved") == 0) {
				/* register move only — no stack change */
				cg->emit_acc_to_saved();
				break;
			} else if (STRCMP(inst->op, "acc_to_tmp") == 0) {
				/* register move only — no stack change */
				cg->emit_acc_to_tmp();
				break;
			} else if (STRCMP(inst->op, "tmp_to_acc") == 0) {
				/* move tmp→acc, then push result */
				cg->emit_tmp_to_acc();
			} else if (STRCMP(inst->op, "load_via_saved") == 0) {
				/* load from [ecx] into acc, then push */
				cg->emit_load_via_saved(inst->value);
			} else {
				ir_emit_unop(cg, inst->op, inst->value, inst->aux);
			}
			cg->emit_push_acc();
			break;

		case IR_BRANCH:
			if (STRCMP(inst->op, "jz") == 0) {
				cg->emit_pop_to_acc();
				cg->emit_branch_if_zero(inst->value);
			} else if (STRCMP(inst->op, "jnz") == 0) {
				cg->emit_pop_to_acc();
				cg->emit_branch_if_nonzero(inst->value);
			} else if (STRCMP(inst->op, "jmp") == 0) {
				cg->emit_branch(inst->value);
			} else if (inst->op[0] == '\0' && inst->name[0] != '\0') {
				cg->emit_branch(ir_get_user_label_id(inst->name));
			}
			break;

		case IR_CALL:
			cg->emit_prepare_call_args(inst->value, inst->fixed_params);
			cg->emit_call(inst->name);
			cg->emit_cleanup_call_args(inst->value, inst->fixed_params);
			if (inst->aux)
				break;
			if (inst->next && inst->next->kind == IR_POP) {
				inst = inst->next;
				break;
			}
			cg->emit_push_acc();
			break;

		case IR_CALL_ACC_ARG0:
			if (cg->emit_acc_to_arg) {
				cg->emit_acc_to_arg(0);
				cg->emit_call(inst->name);
				cg->emit_cleanup_call_args(0, -1);
			} else {
				cg->emit_push_acc();
				cg->emit_prepare_call_args(1, inst->fixed_params);
				cg->emit_call(inst->name);
				cg->emit_cleanup_call_args(1, inst->fixed_params);
			}
			if (inst->next && inst->next->kind == IR_POP) {
				inst = inst->next;
				break;
			}
			if (inst->aux)
				break;
			if (inst->next && inst->next->kind == IR_POP) {
				inst = inst->next;
				break;
			}
			cg->emit_push_acc();
			break;

		case IR_CALL_INDIRECT:
			cg->emit_pop_to_acc();      /* callee */
			cg->emit_acc_to_saved();
			cg->emit_prepare_call_args(inst->value, -1);
			cg->emit_call_saved();
			cg->emit_cleanup_call_args(inst->value, -1);
			if (inst->aux)
				break;
			if (inst->next && inst->next->kind == IR_POP) {
				inst = inst->next;
				break;
			}
			cg->emit_push_acc();
			break;

		case IR_ASM:
			cg->emit_inline_asm(inst->name);
			break;

		case IR_POP:
			cg->emit_pop_to_acc();
			break;

		case IR_RETURN:
			cg->emit_pop_to_acc();
			cg->emit_function_end();
			break;

		default:
			ICE("Unsupported IR instruction");
		}
	}

	if (in_function) {
		if (current_func_name && STRCMP(current_func_name, "main") == 0)
			cg->emit_load_imm(0);
		cg->emit_function_end();
	}
}
