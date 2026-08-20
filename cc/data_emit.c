#include <stdio.h>

#include "target.h"
#include "tcc.h"
#include "parser.h"
#include "data_emit.h"

void
data_emit_label(const char *name, int is_static)
{
	if (!name || !name[0])
		return; /* skip empty-named globals */
	if (!is_static)
		printf(".global %s%s\n", TCC_ASM_SYM_PREFIX, name);
	printf("%s%s:\n", TCC_ASM_SYM_PREFIX, name);
}

void
data_emit_local_label(const char *prefix, int id)
{
	printf("%s%d:\n", prefix, id);
}

void
data_emit_symbol_ref(const char *directive, const char *name)
{
	printf("    %s %s%s\n", directive, TCC_ASM_SYM_PREFIX, name);
}

void
data_emit_local_label_ref(const char *directive, const char *prefix, int id)
{
	printf("    %s %s%d\n", directive, prefix, id);
}

static int
data_emit_pointer_size(Codegen *cg)
{
	if (cg == &x86_codegen || cg == &mips_codegen || cg == &m68k_codegen)
		return 4;
	return 8;
}

static const char *
data_emit_pointer_directive(Codegen *cg)
{
	return data_emit_pointer_size(cg) == 4 ? ".long" : ".quad";
}

static int
data_emit_global_align_bytes(const Global *g, Codegen *cg)
{
	if (g->align > 0)
		return g->align;

	if (g->type)
		return type_alignof(g->type);

	if (g->is_addr || g->is_string)
		return data_emit_pointer_size(cg);

	if (g->is_struct)
		return 4;

	if (g->elem_size >= 8)
		return 8;
	if (g->elem_size >= 4)
		return 4;
	if (g->elem_size >= 2)
		return 2;
	return 1;
}

static int
data_emit_align_log2(int bytes)
{
	int log2 = 0;

	if (bytes <= 1)
		return 0;
	while ((1 << log2) < bytes)
		log2++;
	return log2;
}

static int
data_emit_global_total_size(const Global *g)
{
	if (g->is_array)
		return g->array_len * g->elem_size;
	return g->elem_size;
}

static int
data_emit_global_has_symbol_init(const Global *g, int total_size, int ptr_size)
{
	int slot_count;
	int slot;

	if (!g || ptr_size <= 0 || total_size <= 0)
		return 0;

	slot_count = (total_size + ptr_size - 1) / ptr_size;
	for (slot = 0; slot < slot_count; slot++) {
		const char *sym = global_init_sym(g, slot);
		if (sym && sym[0])
			return 1;
	}

	return 0;
}

static int
data_emit_global_is_all_zero_bytes(const Global *g, int total_size)
{
	int i;

	for (i = 0; i < total_size; i++) {
		if ((global_init_byte(g, i) & 255) != 0)
			return 0;
	}

	return 1;
}

static void data_emit_global_init_byte_run(const Global *g, int start, int len);
static void data_emit_global_struct_bytes(const Global *g, Codegen *cg, int total_size);

static int
data_emit_global_can_zero_fill(const Global *g, Codegen *cg)
{
	int total_size;
	int ic;

	if (!g || g->is_extern || g->is_string || g->is_addr || g->is_string_array)
		return 0;

	total_size = data_emit_global_total_size(g);
	if (total_size <= 0)
		return 0;

	if (data_emit_global_has_symbol_init(g, total_size, data_emit_pointer_size(cg)))
		return 0;

	ic = global_init_count(g);
	if (ic <= 0) {
		if (!g->is_array && !g->is_struct && g->init_value != 0)
			return 0;
		return 1;
	}

	return data_emit_global_is_all_zero_bytes(g, total_size);
}

static void
data_emit_tls_init_label(const char *name)
{
	printf("%s%s$tlv$init:\n", TCC_ASM_SYM_PREFIX, name);
}

static void
data_emit_tls_align(int bytes)
{
	printf("    .p2align %d, 0x0\n", data_emit_align_log2(bytes));
}

static void
data_emit_thread_local_descriptor(const Global *g)
{
	printf(".section __DATA,__thread_vars,thread_local_variables\n");
	printf("    .p2align 3, 0x0\n");
	data_emit_label(g->name, g->is_static);
	printf("    .quad __tlv_bootstrap\n");
	printf("    .quad 0\n");
	printf("    .quad %s%s$tlv$init\n", TCC_ASM_SYM_PREFIX, g->name);
}

static void
data_emit_thread_local_payload(const Global *g, Codegen *cg, int use_backend_string_literals)
{
	int total_size = data_emit_global_total_size(g);
	int ic = global_init_count(g);

	if (data_emit_global_can_zero_fill(g, cg)) {
		printf(".tbss %s%s$tlv$init, %d, %d\n",
		       TCC_ASM_SYM_PREFIX, g->name, total_size,
		       data_emit_align_log2(data_emit_global_align_bytes(g, cg)));
		data_emit_thread_local_descriptor(g);
		return;
	}

	printf(".section __DATA,__thread_data,thread_local_regular\n");
	data_emit_tls_align(data_emit_global_align_bytes(g, cg));
	data_emit_tls_init_label(g->name);

	if (g->is_array) {
		if (g->is_struct) {
			if (ic > 0)
				data_emit_global_struct_bytes(g, cg, g->array_len * g->elem_size);
			else
				data_emit_zero(g->array_len * g->elem_size);
		} else if (g->elem_size == 1) {
			if (g->is_string_array)
				emit_raw_string_literal_len(g->string_value, g->string_len);
			else if (ic > 0)
				data_emit_global_init_byte_run(g, 0, g->array_len);
			else
				data_emit_zero(g->array_len);
		} else if (ic > 0) {
			for (int j = 0; j < g->array_len; j++) {
				long long value;
				const char *symbol = global_init_sym(g, j);
				if (ic > g->array_len) {
					value = 0;
					if (j * g->elem_size < ic) {
						int byte_base = j * g->elem_size;
						for (int i = 0; i < g->elem_size; i++)
							value |= ((long long)(global_init_byte(g, byte_base + i) & 255)) << (8 * i);
					}
				} else {
					value = j < ic ? global_init_byte(g, j) : 0;
				}
				data_emit_scalar_or_symbol(value, g->elem_size,
				                           symbol && symbol[0] ? symbol : NULL);
			}
		} else {
			data_emit_zero(g->array_len * g->elem_size);
		}
	} else if (g->is_string) {
		if (use_backend_string_literals) {
			cg->emit_string_literal(g->string_label, g->string_value, g->string_len, 1);
			printf(".section __DATA,__thread_data,thread_local_regular\n");
			data_emit_tls_align(data_emit_global_align_bytes(g, cg));
			data_emit_tls_init_label(g->name);
		} else {
			data_emit_local_label(".Lstr", g->string_label);
			emit_raw_string_literal_len(g->string_value, g->string_len);
			printf(".section __DATA,__thread_data,thread_local_regular\n");
			data_emit_tls_align(data_emit_global_align_bytes(g, cg));
			data_emit_tls_init_label(g->name);
		}
		data_emit_local_label_ref(data_emit_pointer_directive(cg), ".Lstr", g->string_label);
	} else if (g->is_struct) {
		if (ic > 0)
			data_emit_global_struct_bytes(g, cg, g->elem_size);
		else
			data_emit_zero(g->elem_size);
	} else if (g->is_addr) {
		if (g->addr_offset != 0)
			printf("    %s %s%s+%d\n", data_emit_pointer_directive(cg),
			       TCC_ASM_SYM_PREFIX, g->addr_name, g->addr_offset);
		else
			data_emit_symbol_ref(data_emit_pointer_directive(cg), g->addr_name);
	} else {
		if (g->type && (type_is_fp_scalar(g->type) || type_is_complex(g->type)) && ic > 0)
			data_emit_global_init_byte_run(g, 0, g->elem_size);
		else
			data_emit_scalar(g->init_value, g->elem_size);
	}

	data_emit_thread_local_descriptor(g);
}

static void
data_emit_zero_fill_global(const Global *g, Codegen *cg, int total_size)
{
#if defined(__APPLE__)
	int align_log2 = data_emit_align_log2(data_emit_global_align_bytes(g, cg));

	if (!g->is_static)
		printf(".global %s%s\n", TCC_ASM_SYM_PREFIX, g->name);
	printf(".zerofill __DATA,__bss,%s%s,%d,%d\n",
	       TCC_ASM_SYM_PREFIX, g->name, total_size, align_log2);
#else
	printf(".bss\n");
	if (data_emit_global_align_bytes(g, cg) > 1)
		data_emit_align(data_emit_global_align_bytes(g, cg));
	data_emit_label(g->name, g->is_static);
	data_emit_zero(total_size);
#endif
}

static void
data_emit_switch_to_data(void)
{
	printf(".data\n");
}

void
data_emit_align(int bytes)
{
	printf("    .align %d\n", bytes);
}

void
data_emit_zero(int bytes)
{
	printf("    .zero %d\n", bytes);
}

void
data_emit_byte(int value)
{
	printf("    .byte %d\n", value & 255);
}

static void
data_emit_global_init_byte_run(const Global *g, int start, int len)
{
	int i = 0;
	while (i < len) {
		int n = len - i;
		int j;
		if (n > 16)
			n = 16;

		printf("    .byte ");
		for (j = 0; j < n; j++) {
			int value = (int)(global_init_byte(g, start + i + j) & 255);
			printf(j ? ", %d" : "%d", value);
		}
		printf("\n");
		i += n;
	}
}

void
data_emit_short(int value)
{
	printf("    .short %d\n", value & 65535);
}

void
data_emit_long(int value)
{
	printf("    .long %d\n", value);
}

void
data_emit_quad(long long value)
{
	printf("    .quad %lld\n", value);
}

void
data_emit_scalar(long long value, int size)
{
	if (size == 1)
		data_emit_byte((int)value);
	else if (size == 2)
		data_emit_short((int)value);
	else if (size == 8)
		data_emit_quad(value);
	else
		data_emit_long((int)value);
}

void
data_emit_scalar_or_symbol(long long value, int size, const char *symbol)
{
	if (symbol && symbol[0]) {
		if (size == 8)
			data_emit_symbol_ref(".quad", symbol);
		else if (size == 2)
			data_emit_symbol_ref(".short", symbol);
		else if (size == 1)
			data_emit_symbol_ref(".byte", symbol);
		else
			data_emit_symbol_ref(".long", symbol);
		return;
	}
	data_emit_scalar(value, size);
}

static void
data_emit_global_string_pointer(const Global *g, Codegen *cg, int use_backend_string_literals)
{
	if (use_backend_string_literals) {
		cg->emit_string_literal(g->string_label, g->string_value, g->string_len, 1);
		printf(".data\n");
	} else {
		data_emit_local_label(".Lstr", g->string_label);
		emit_raw_string_literal_len(g->string_value, g->string_len);
	}
	data_emit_align(data_emit_global_align_bytes(g, cg));
	data_emit_label(g->name, g->is_static);
	data_emit_local_label_ref(data_emit_pointer_directive(cg), ".Lstr", g->string_label);
}

static void
data_emit_global_struct_bytes(const Global *g, Codegen *cg, int total_size)
{
	int j = 0;
	int ptr_size = data_emit_pointer_size(cg);
	const char *ptr_directive = data_emit_pointer_directive(cg);

	while (j < total_size) {
		int slot;
		int run_start;
		int run_len;
		const char *sym;

		/* Pointer/function-pointer fields are recorded in init_syms[] using
		 * target pointer-sized slots.  Emit relocations instead of raw zero
		 * bytes there. */
		slot = j / ptr_size;
		sym = (j % ptr_size) == 0 ? global_init_sym(g, slot) : NULL;
		if (sym && sym[0]) {
			data_emit_symbol_ref(ptr_directive, sym);
			j += ptr_size;
			continue;
		}

		run_start = j;
		run_len = 0;
		while (j < total_size) {
			slot = j / ptr_size;
			sym = (j % ptr_size) == 0 ? global_init_sym(g, slot) : NULL;
			if (sym && sym[0])
				break;
			j++;
			run_len++;
		}
		data_emit_global_init_byte_run(g, run_start, run_len);
	}
}

static void
data_emit_one_global(const Global *g, Codegen *cg, int use_backend_string_literals)
{
	int total_size;

	if (g->is_extern)
		return;

	if (g->is_thread_local) {
#if defined(__APPLE__)
		if (cg != &arm64_codegen)
			ICE("thread-local storage currently only emits on arm64 Darwin");
		data_emit_thread_local_payload(g, cg, use_backend_string_literals);
		return;
#else
		ICE("thread-local storage currently only emits on Darwin");
#endif
	}

	int ic = global_init_count(g);
	total_size = data_emit_global_total_size(g);

	if (data_emit_global_can_zero_fill(g, cg)) {
		data_emit_zero_fill_global(g, cg, total_size);
		return;
	}

	if (g->is_array) {
		if (g->is_struct) {
			data_emit_switch_to_data();
			data_emit_align(data_emit_global_align_bytes(g, cg));
			data_emit_label(g->name, g->is_static);
			if (ic > 0)
				data_emit_global_struct_bytes(g, cg, g->array_len * g->elem_size);
			else
				data_emit_zero(g->array_len * g->elem_size);
		} else if (g->elem_size == 1) {
			data_emit_switch_to_data();
			data_emit_label(g->name, g->is_static);
			if (g->is_string_array) {
				emit_raw_string_literal_len(g->string_value, g->string_len);
			} else if (ic > 0) {
				data_emit_global_init_byte_run(g, 0, g->array_len);
			} else {
				data_emit_zero(g->array_len);
			}
		} else if (ic > 0) {
			data_emit_switch_to_data();
			data_emit_align(data_emit_global_align_bytes(g, cg));
			data_emit_label(g->name, g->is_static);
			for (int j = 0; j < g->array_len; j++) {
				long long value;
				const char *symbol = global_init_sym(g, j);
				if (ic > g->array_len) {
					value = 0;
					if (j * g->elem_size < ic) {
						int byte_base = j * g->elem_size;
						for (int i = 0; i < g->elem_size; i++)
							value |= ((long long)(global_init_byte(g, byte_base + i) & 255)) << (8 * i);
					}
				} else {
					value = j < ic ? global_init_byte(g, j) : 0;
				}
				data_emit_scalar_or_symbol(value, g->elem_size,
				                           symbol && symbol[0] ? symbol : NULL);
			}
		} else {
			data_emit_switch_to_data();
			data_emit_align(data_emit_global_align_bytes(g, cg));
			data_emit_label(g->name, g->is_static);
			data_emit_zero(g->array_len * g->elem_size);
		}
	} else if (g->is_string) {
		data_emit_switch_to_data();
		data_emit_global_string_pointer(g, cg, use_backend_string_literals);
	} else if (g->is_struct) {
		data_emit_switch_to_data();
		data_emit_align(data_emit_global_align_bytes(g, cg));
		data_emit_label(g->name, g->is_static);
		if (ic > 0)
			data_emit_global_struct_bytes(g, cg, g->elem_size);
		else
			data_emit_zero(g->elem_size);
	} else if (g->is_addr) {
		data_emit_switch_to_data();
		data_emit_align(data_emit_global_align_bytes(g, cg));
		data_emit_label(g->name, g->is_static);
		if (g->addr_offset != 0)
			printf("    %s %s%s+%d\n", data_emit_pointer_directive(cg),
			       TCC_ASM_SYM_PREFIX, g->addr_name, g->addr_offset);
		else
			data_emit_symbol_ref(data_emit_pointer_directive(cg), g->addr_name);
	} else {
		if (g->type && (type_is_fp_scalar(g->type) || type_is_complex(g->type)) && ic > 0) {
			data_emit_switch_to_data();
			data_emit_align(data_emit_global_align_bytes(g, cg));
			data_emit_label(g->name, g->is_static);
			data_emit_global_init_byte_run(g, 0, g->elem_size);
			return;
		}
		data_emit_switch_to_data();
		if (data_emit_global_align_bytes(g, cg) > 1)
			data_emit_align(data_emit_global_align_bytes(g, cg));
		data_emit_label(g->name, g->is_static);
		data_emit_scalar(g->init_value, g->elem_size);
	}
}

void
data_emit_globals(Codegen *cg, int use_backend_string_literals)
{
	int has_data_globals = 0;
	{
		int i;
		for (i = 0; i < parser_global_count(); i++) {
			Global *g = parser_global_at(i);
			if (!g->is_extern) {
				has_data_globals = 1;
				break;
			}
		}
	}
	if (!has_data_globals)
		return;

	{
		int i;
		for (i = 0; i < parser_global_count(); i++)
			data_emit_one_global(parser_global_at(i), cg, use_backend_string_literals);
	}
	printf(".text\n");
}
