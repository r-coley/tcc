#ifndef IR_H
#define IR_H

#include "ast.h"
#include "codegen/codegen.h"

typedef enum {
	IR_CONST,
	IR_LOAD,
	IR_STORE,
	IR_BINOP,
	IR_UNOP,
	IR_RETURN,
	IR_LABEL,
	IR_BRANCH,
	IR_CALL,
	IR_CALL_INDIRECT,
	IR_CALL_ACC_ARG0,
	IR_ASM,
	IR_POP
} IRKind;

typedef struct IRInst {
	IRKind kind;
	int id;
	int value;
	int aux;
	int is_static;
	int is_extern;
	int fixed_params;  /* for IR_CALL: fixed named param count (-1 = not variadic) */
	int src_line;       /* source line number for debug info (0 = unknown) */
	int src_filename_id; /* lexer filename id for debug info */
	char op[16];
	char name[64];
	struct IRInst *next;
} IRInst;

typedef struct IRString {
	int label;
	char value[4096];
	struct IRString *next;
} IRString;

typedef struct IRBlock {
	int id;
	int label;
	IRInst *first;
	IRInst *last;
	struct IRBlock *fallthrough;
	struct IRBlock *branch;
	struct IRBlock *next;
} IRBlock;

typedef struct IRProgram {
	IRInst *head;
	IRInst *tail;
	int next_id;
	IRString *strings;
	IRBlock *blocks;
	int block_count;
	int unsupported_count;
	char unsupported_reason[256];
} IRProgram;

IRProgram *ir_build(Node *program);
void ir_dump(IRProgram *program);
void ir_build_cfg(IRProgram *program);
void ir_dump_cfg(IRProgram *program);
void ir_cfg_eliminate_unreachable(IRProgram *program);
void ir_optimize(IRProgram *program, int level);
int ir_can_codegen(IRProgram *program);
int ir_has_unsupported(IRProgram *program);
const char *ir_unsupported_reason(IRProgram *program);
void ir_emit_program(IRProgram *program, Codegen *cg);
void ir_free(IRProgram *program);

#endif
