#ifndef DATA_EMIT_H
#define DATA_EMIT_H

#include "codegen/codegen.h"

void data_emit_label(const char *name, int is_static);
void data_emit_local_label(const char *prefix, int id);
void data_emit_symbol_ref(const char *directive, const char *name);
void data_emit_local_label_ref(const char *directive, const char *prefix, int id);
void data_emit_align(int bytes);
void data_emit_zero(int bytes);
void data_emit_byte(int value);
void data_emit_short(int value);
void data_emit_long(int value);
void data_emit_quad(long long value);
void data_emit_scalar(long long value, int size);
void data_emit_scalar_or_symbol(long long value, int size, const char *symbol);
void data_emit_globals(Codegen *cg, int use_backend_string_literals);

#endif
