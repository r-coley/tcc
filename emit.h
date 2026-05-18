#ifndef EMIT_H
#define EMIT_H

#include "ast.h"
#include "codegen/codegen.h"

void emit_program(Node *program, Codegen *cg);

#endif
