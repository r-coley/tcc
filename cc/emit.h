#ifndef EMIT_H
#define EMIT_H

#include "ast.h"
#include "codegen/codegen.h"
#include "ir.h"

typedef struct HybridEmitProfile {
	double string_literals;
	double ir_functions;
	double ast_fallback_functions;
} HybridEmitProfile;

void emit_program(Node *program, Codegen *cg);
void emit_program_hybrid(Node *program, IRProgram *ir, Codegen *cg,
                         HybridEmitProfile *profile);

#endif
