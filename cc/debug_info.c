#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tcc.h"
#include "target.h"
#include "debug_info.h"

const char *
debug_unit_intern_string(DebugUnit *du, const char *text)
{
	DebugString *entry;
	int new_cap;

	if (!text)
		return NULL;
	if (!du)
		return text;

	for (int i = 0; i < du->string_count; i++) {
		if (du->strings[i].text && strcmp(du->strings[i].text, text) == 0)
			return du->strings[i].text;
	}

	if (du->string_count >= du->string_cap) {
		new_cap = du->string_cap ? du->string_cap * 2 : 16;
		du->strings = xrealloc(du->strings,
		                       sizeof(DebugString) * (size_t)new_cap);
		memset(du->strings + du->string_cap, 0,
		       sizeof(DebugString) * (size_t)(new_cap - du->string_cap));
		du->string_cap = new_cap;
	}

	entry = &du->strings[du->string_count++];
	entry->text = xstrdup(text);
	return entry->text;
}

void
debug_unit_init(DebugUnit *du, const char *source_file, const char *producer)
{
	if (!du)
		return;
	memset(du, 0, sizeof(*du));
	du->source_file = debug_unit_intern_string(du, source_file);
	du->producer = debug_unit_intern_string(du, producer);
}

void
debug_unit_free(DebugUnit *du)
{
	if (!du)
		return;
	for (int i = 0; i < du->string_count; i++)
		xfree(du->strings[i].text);
	xfree(du->strings);
	du->strings = NULL;
	du->string_count = 0;
	du->string_cap = 0;
	for (int i = 0; i < du->function_count; i++) {
		xfree(du->functions[i].param_names);
		xfree(du->functions[i].param_type_ids);
		xfree(du->functions[i].param_offsets);
		xfree(du->functions[i].param_regs);
		xfree(du->functions[i].param_struct_names);
		xfree(du->functions[i].param_pointer_struct_names);
		xfree(du->functions[i].param_pointer_depths);
		if (du->functions[i].locals) {
			for (int j = 0; j < du->functions[i].local_count; j++)
				xfree(du->functions[i].locals[j].name);
			xfree(du->functions[i].locals);
		}
	}
	xfree(du->functions);
	du->functions = NULL;
	du->function_count = 0;
	du->function_cap = 0;
	for (int i = 0; i < du->struct_count; i++)
		xfree(du->structs[i].members);
	xfree(du->structs);
	du->structs = NULL;
	du->struct_count = 0;
	du->struct_cap = 0;
	xfree(du->arrays);
	du->arrays = NULL;
	du->array_count = 0;
	du->array_cap = 0;
	xfree(du->struct_ptrs);
	du->struct_ptrs = NULL;
	du->struct_ptr_count = 0;
	du->struct_ptr_cap = 0;
}

void
debug_unit_set_source(DebugUnit *du, const char *source_file)
{
	if (!du)
		return;
	du->source_file = debug_unit_intern_string(du, source_file);
}

void
debug_unit_set_comp_dir(DebugUnit *du, const char *comp_dir)
{
	if (!du)
		return;
	du->comp_dir = debug_unit_intern_string(du, comp_dir);
}

DebugFunction *
debug_unit_add_function(DebugUnit *du, const char *name, const char *file, int line)
{
	DebugFunction *fn;
	int new_cap;

	if (!du)
		return NULL;
	if (du->function_count >= du->function_cap) {
		new_cap = du->function_cap ? du->function_cap * 2 : 16;
		du->functions = xrealloc(du->functions,
		                         sizeof(DebugFunction) * (size_t)new_cap);
		memset(du->functions + du->function_cap, 0,
		       sizeof(DebugFunction) * (size_t)(new_cap - du->function_cap));
		du->function_cap = new_cap;
	}

	fn = &du->functions[du->function_count++];
	memset(fn, 0, sizeof(*fn));
	fn->name = debug_unit_intern_string(du, name);
	fn->file = debug_unit_intern_string(du, file);
	fn->line = line;
	return fn;
}

void
debug_function_set_range(DebugFunction *fn, int low_pc_label, int high_pc_label)
{
	if (!fn)
		return;
	fn->low_pc_label = low_pc_label;
	fn->high_pc_label = high_pc_label;
}

void
debug_function_set_params(DebugFunction *fn, int param_count, int stack_size)
{
	if (!fn)
		return;
	fn->param_count = param_count;
	fn->stack_size = stack_size;
	fn->frame_base_reg = 29;
	fn->frame_base_bias = 0;
}

void
debug_function_set_frame_base(DebugFunction *fn, int dwarf_reg, int bias)
{
	if (!fn)
		return;
	fn->frame_base_reg = dwarf_reg;
	fn->frame_base_bias = bias;
}

void
debug_function_set_param_names(DebugUnit *du, DebugFunction *fn,
                                char **param_names, int param_count)
{
	int i;

	if (!du || !fn || param_count <= 0)
		return;
	fn->param_names = xcalloc((size_t)param_count, sizeof(char *));
	for (i = 0; i < param_count; i++) {
		if (param_names && param_names[i] && param_names[i][0])
			fn->param_names[i] = debug_unit_intern_string(du, param_names[i]);
	}
}

void
debug_function_set_param_types(DebugFunction *fn, const int *type_ids, int param_count)
{
	int i;

	if (!fn || param_count <= 0)
		return;
	fn->param_type_ids = xcalloc((size_t)param_count, sizeof(int));
	for (i = 0; i < param_count; i++)
		fn->param_type_ids[i] = type_ids ? type_ids[i] : DBG_TYPE_NONE;
}

void
debug_function_set_param_offsets(DebugFunction *fn, const int *offsets, int param_count)
{
	int i;

	if (!fn || param_count <= 0)
		return;
	fn->param_offsets = xcalloc((size_t)param_count, sizeof(int));
	for (i = 0; i < param_count; i++)
		fn->param_offsets[i] = offsets ? offsets[i] : 0;
}

void
debug_function_set_param_regs(DebugFunction *fn, const int *regs, int param_count)
{
	int i;

	if (!fn || param_count <= 0)
		return;
	fn->param_regs = xcalloc((size_t)param_count, sizeof(int));
	for (i = 0; i < param_count; i++)
		fn->param_regs[i] = regs ? regs[i] : -1;
}

void
debug_function_set_param_structs(DebugUnit *du, DebugFunction *fn,
                                      char **struct_names, int param_count)
{
	int i;

	if (!du || !fn || param_count <= 0)
		return;
	fn->param_struct_names = xcalloc((size_t)param_count, sizeof(char *));
	for (i = 0; i < param_count; i++) {
		if (struct_names && struct_names[i] && struct_names[i][0])
			fn->param_struct_names[i] = debug_unit_intern_string(du, struct_names[i]);
	}
}

void
debug_function_set_param_pointer_structs(DebugUnit *du, DebugFunction *fn,
                                         char **struct_names, const int *depths,
                                         int param_count)
{
	int i;

	if (!du || !fn || param_count <= 0)
		return;
	fn->param_pointer_struct_names = xcalloc((size_t)param_count, sizeof(char *));
	fn->param_pointer_depths = xcalloc((size_t)param_count, sizeof(int));
	for (i = 0; i < param_count; i++) {
		if (struct_names && struct_names[i] && struct_names[i][0])
			fn->param_pointer_struct_names[i] = debug_unit_intern_string(du, struct_names[i]);
		fn->param_pointer_depths[i] = depths ? depths[i] : 0;
	}
}


void
debug_function_set_locals(DebugUnit *du, DebugFunction *fn,
                           const NodeDebugLocal *locals, int local_count)
{
	int i;

	if (!du || !fn || !locals || local_count <= 0)
		return;
	fn->locals = xcalloc((size_t)local_count, sizeof(NodeDebugLocal));
	fn->local_count = local_count;
	for (i = 0; i < local_count; i++) {
		if (locals[i].name && locals[i].name[0])
			fn->locals[i].name = xstrdup(debug_unit_intern_string(du, locals[i].name));
		fn->locals[i].offset = locals[i].offset;
		fn->locals[i].type_id = locals[i].type_id;
		if (locals[i].struct_name[0])
			STRNCPY(fn->locals[i].struct_name, locals[i].struct_name,
			        sizeof(fn->locals[i].struct_name) - 1);
		if (locals[i].pointer_struct_name[0])
			STRNCPY(fn->locals[i].pointer_struct_name, locals[i].pointer_struct_name,
			        sizeof(fn->locals[i].pointer_struct_name) - 1);
		fn->locals[i].pointer_depth = locals[i].pointer_depth;
		fn->locals[i].array_len = locals[i].array_len;
		fn->locals[i].array_elem_type_id = locals[i].array_elem_type_id;
		if (locals[i].array_elem_struct_name[0])
			STRNCPY(fn->locals[i].array_elem_struct_name, locals[i].array_elem_struct_name,
			        sizeof(fn->locals[i].array_elem_struct_name) - 1);
	}
}


DebugStructType *
debug_unit_add_struct_type(DebugUnit *du, const char *name, int byte_size)
{
	DebugStructType *st;
	int new_cap;

	if (!du || !name || !name[0])
		return NULL;
	for (int i = 0; i < du->struct_count; i++) {
		if (du->structs[i].name && strcmp(du->structs[i].name, name) == 0)
			return &du->structs[i];
	}
	if (du->struct_count >= du->struct_cap) {
		new_cap = du->struct_cap ? du->struct_cap * 2 : 8;
		du->structs = xrealloc(du->structs,
		                      sizeof(DebugStructType) * (size_t)new_cap);
		memset(du->structs + du->struct_cap, 0,
		       sizeof(DebugStructType) * (size_t)(new_cap - du->struct_cap));
		du->struct_cap = new_cap;
	}
	st = &du->structs[du->struct_count++];
	memset(st, 0, sizeof(*st));
	st->name = debug_unit_intern_string(du, name);
	st->byte_size = byte_size;
	return st;
}

static DebugStructMember *
debug_struct_type_push_member(DebugStructType *st)
{
	int new_cap;

	if (st->member_count >= st->member_cap) {
		new_cap = st->member_cap ? st->member_cap * 2 : 8;
		st->members = xrealloc(st->members,
		                     sizeof(DebugStructMember) * (size_t)new_cap);
		memset(st->members + st->member_cap, 0,
		       sizeof(DebugStructMember) * (size_t)(new_cap - st->member_cap));
		st->member_cap = new_cap;
	}
	return &st->members[st->member_count++];
}

void
debug_struct_type_add_member(DebugUnit *du, DebugStructType *st, const char *name,
                             int offset, int type_id)
{
	DebugStructMember *m;

	if (!du || !st || !name || !name[0])
		return;
	m = debug_struct_type_push_member(st);
	m->name = debug_unit_intern_string(du, name);
	m->offset = offset;
	m->type_id = type_id == DBG_TYPE_NONE ? DBG_TYPE_INT : type_id;
}

void
debug_struct_type_add_struct_member(DebugUnit *du, DebugStructType *st, const char *name,
                                    int offset, const char *struct_name)
{
	DebugStructMember *m;

	if (!du || !st || !name || !name[0] || !struct_name || !struct_name[0])
		return;
	m = debug_struct_type_push_member(st);
	m->name = debug_unit_intern_string(du, name);
	m->offset = offset;
	m->type_id = DBG_TYPE_NONE;
	m->struct_name = debug_unit_intern_string(du, struct_name);
}

void
debug_struct_type_add_array_member(DebugUnit *du, DebugStructType *st, const char *name,
                                      int offset, int elem_type_id,
                                      const char *elem_struct_name, int count)
{
	DebugStructMember *m;
	int new_cap;

	if (!du || !st || !name || !name[0] || count <= 0)
		return;
	if (elem_struct_name && elem_struct_name[0])
		elem_type_id = DBG_TYPE_NONE;
	else if (elem_type_id == DBG_TYPE_NONE)
		elem_type_id = DBG_TYPE_INT;
	debug_unit_add_array_type(du, elem_type_id, elem_struct_name, count);
	if (st->member_count >= st->member_cap) {
		new_cap = st->member_cap ? st->member_cap * 2 : 8;
		st->members = xrealloc(st->members,
		                     sizeof(DebugStructMember) * (size_t)new_cap);
		memset(st->members + st->member_cap, 0,
		       sizeof(DebugStructMember) * (size_t)(new_cap - st->member_cap));
		st->member_cap = new_cap;
	}
	m = &st->members[st->member_count++];
	m->name = debug_unit_intern_string(du, name);
	m->offset = offset;
	m->type_id = DBG_TYPE_NONE;
	m->array_len = count;
	m->array_elem_type_id = elem_type_id;
	if (elem_struct_name && elem_struct_name[0])
		m->array_elem_struct_name = debug_unit_intern_string(du, elem_struct_name);
}

void
debug_unit_add_array_type(DebugUnit *du, int elem_type_id, const char *elem_struct_name, int count)
{
	DebugArrayType *at;
	int new_cap;

	if (!du || count <= 0)
		return;
	if (elem_struct_name && elem_struct_name[0])
		elem_type_id = DBG_TYPE_NONE;
	else if (elem_type_id == DBG_TYPE_NONE)
		elem_type_id = DBG_TYPE_INT;
	for (int i = 0; i < du->array_count; i++) {
		int same_struct = 0;
		if (!du->arrays[i].elem_struct_name && (!elem_struct_name || !elem_struct_name[0]))
			same_struct = 1;
		else if (du->arrays[i].elem_struct_name && elem_struct_name &&
		         strcmp(du->arrays[i].elem_struct_name, elem_struct_name) == 0)
			same_struct = 1;
		if (same_struct && du->arrays[i].elem_type_id == elem_type_id &&
		    du->arrays[i].count == count)
			return;
	}
	if (du->array_count >= du->array_cap) {
		new_cap = du->array_cap ? du->array_cap * 2 : 8;
		du->arrays = xrealloc(du->arrays,
		                       sizeof(DebugArrayType) * (size_t)new_cap);
		memset(du->arrays + du->array_cap, 0,
		       sizeof(DebugArrayType) * (size_t)(new_cap - du->array_cap));
		du->array_cap = new_cap;
	}
	at = &du->arrays[du->array_count++];
	at->elem_type_id = elem_type_id;
	if (elem_struct_name && elem_struct_name[0])
		at->elem_struct_name = debug_unit_intern_string(du, elem_struct_name);
	at->count = count;
}

void
debug_unit_add_struct_pointer_type_depth(DebugUnit *du, const char *struct_name, int pointer_depth)
{
	DebugStructPointerType *pt;
	int new_cap;

	if (!du || !struct_name || !struct_name[0])
		return;
	if (pointer_depth <= 0)
		pointer_depth = 1;
	for (int depth = 1; depth <= pointer_depth; depth++) {
		int found = 0;
		for (int i = 0; i < du->struct_ptr_count; i++) {
			if (du->struct_ptrs[i].struct_name &&
			    strcmp(du->struct_ptrs[i].struct_name, struct_name) == 0 &&
			    du->struct_ptrs[i].pointer_depth == depth) {
				found = 1;
				break;
			}
		}
		if (found)
			continue;
		if (du->struct_ptr_count >= du->struct_ptr_cap) {
			new_cap = du->struct_ptr_cap ? du->struct_ptr_cap * 2 : 8;
			du->struct_ptrs = xrealloc(du->struct_ptrs,
			                           sizeof(DebugStructPointerType) * (size_t)new_cap);
			memset(du->struct_ptrs + du->struct_ptr_cap, 0,
			       sizeof(DebugStructPointerType) * (size_t)(new_cap - du->struct_ptr_cap));
			du->struct_ptr_cap = new_cap;
		}
		pt = &du->struct_ptrs[du->struct_ptr_count++];
		pt->struct_name = debug_unit_intern_string(du, struct_name);
		pt->pointer_depth = depth;
	}
}

void
debug_unit_add_struct_pointer_type(DebugUnit *du, const char *struct_name)
{
	debug_unit_add_struct_pointer_type_depth(du, struct_name, 1);
}

int
debug_unit_string_offset(const DebugUnit *du, const char *text)
{
	int offset = 0;

	if (!du || !text)
		return -1;
	for (int i = 0; i < du->string_count; i++) {
		const char *entry = du->strings[i].text;
		if (entry && strcmp(entry, text) == 0)
			return offset;
		offset += entry ? (int)strlen(entry) + 1 : 1;
	}
	return -1;
}

int
debug_unit_debug_str_size(const DebugUnit *du)
{
	int size = 0;

	if (!du)
		return 0;
	for (int i = 0; i < du->string_count; i++) {
		const char *entry = du->strings[i].text;
		size += entry ? (int)strlen(entry) + 1 : 1;
	}
	return size;
}

void
debug_unit_debug_dump_strings(const DebugUnit *du)
{
	int offset = 0;

	if (!du)
		return;
	for (int i = 0; i < du->string_count; i++) {
		const char *entry = du->strings[i].text;
		Debug(1, "DWARF string[%d] off=%d text=%s\n",
		      i, offset, entry ? entry : "");
		offset += entry ? (int)strlen(entry) + 1 : 1;
	}
}

int
debug_unit_debug_abbrev_size(void)
{
	/*
	 * Abbrev table byte count:
	 *   Abbrev 1: compile_unit (children=yes, 6 attrs + terminators)
	 *   Abbrev 2: subprogram   (children=yes, 6 attrs + terminators)
	 *   Abbrev 3: formal_param (children=no,  1 attr  + terminators)
	 *   Abbrev 4: formal_param (children=no,  2 attrs + terminators)
	 *   Abbrev 5: local_var    (children=no,  2 attrs + terminators)
	 *   End of table: 1 byte (0)
	 */
	return 119;
}

static void
debug_emit_string_bytes(const char *text)
{
	const unsigned char *p = (const unsigned char *)text;

	printf("    .byte ");
	if (p) {
		while (*p) {
			printf("%u, ", (unsigned)*p);
			p++;
		}
	}
	printf("0\n");
}

static const char *
debug_type_name(int type_id)
{
	switch (type_id) {
	case DBG_TYPE_INT: return "int";
	case DBG_TYPE_UINT: return "unsigned int";
	case DBG_TYPE_CHAR: return "char";
	case DBG_TYPE_UCHAR: return "unsigned char";
	case DBG_TYPE_SHORT: return "short";
	case DBG_TYPE_USHORT: return "unsigned short";
	case DBG_TYPE_FLOAT: return "float";
	case DBG_TYPE_DOUBLE: return "double";
	default: return NULL;
	}
}

static int
debug_type_is_pointer(int type_id)
{
	return type_id >= DBG_TYPE_PTR_VOID && type_id < DBG_TYPE_COUNT;
}

static int
debug_pointer_base_type(int type_id)
{
	switch (type_id) {
	case DBG_TYPE_PTR_INT: return DBG_TYPE_INT;
	case DBG_TYPE_PTR_UINT: return DBG_TYPE_UINT;
	case DBG_TYPE_PTR_CHAR: return DBG_TYPE_CHAR;
	case DBG_TYPE_PTR_UCHAR: return DBG_TYPE_UCHAR;
	case DBG_TYPE_PTR_SHORT: return DBG_TYPE_SHORT;
	case DBG_TYPE_PTR_USHORT: return DBG_TYPE_USHORT;
	default: return DBG_TYPE_NONE;
	}
}

static int
debug_base_type_byte_size(int type_id)
{
	switch (type_id) {
	case DBG_TYPE_CHAR:
	case DBG_TYPE_UCHAR:
		return 1;
	case DBG_TYPE_SHORT:
	case DBG_TYPE_USHORT:
		return 2;
	case DBG_TYPE_INT:
	case DBG_TYPE_UINT:
	case DBG_TYPE_FLOAT:
		return 4;
	case DBG_TYPE_DOUBLE:
		return 8;
	default:
		return 0;
	}
}

void
debug_unit_intern_builtin_type_strings(DebugUnit *du)
{
	for (int i = DBG_TYPE_INT; i <= DBG_TYPE_DOUBLE; i++) {
		const char *name = debug_type_name(i);
		if (name)
			debug_unit_intern_string(du, name);
	}
}

static int
debug_type_die_size(int type_id)
{
	if (type_id >= DBG_TYPE_INT && type_id <= DBG_TYPE_USHORT)
		return 7; /* abbrev + name(strp) + encoding + byte_size */
	if (debug_type_is_pointer(type_id)) {
		if (type_id == DBG_TYPE_PTR_VOID)
			return 2; /* abbrev + byte_size; void* has no DW_AT_type */
		return 6; /* abbrev + type(ref4) + byte_size */
	}
	return 0;
}

static int
debug_type_offset(int type_id)
{
	int off = 12 + 23; /* unit header(12) + compile_unit DIE(23) */

	if (type_id <= DBG_TYPE_NONE || type_id >= DBG_TYPE_COUNT)
		return 0;
	for (int i = DBG_TYPE_INT; i < type_id; i++)
		off += debug_type_die_size(i);
	return off;
}


static int
debug_builtin_types_size(void)
{
	int off = 0;
	for (int i = DBG_TYPE_INT; i < DBG_TYPE_COUNT; i++)
		off += debug_type_die_size(i);
	return off;
}

static int
debug_struct_die_size(const DebugStructType *st)
{
	if (!st)
		return 0;
	return 10 + st->member_count * 13;
}

static int
debug_struct_type_offset(const DebugUnit *du, const char *name)
{
	int off = 12 + 23 + debug_builtin_types_size();
	const DebugStructType *st;
	const DebugStructType *end;

	if (!du || !name || !name[0])
		return 0;
	st = du->structs;
	end = st + du->struct_count;
	while (st < end) {
		if (st->name && strcmp(st->name, name) == 0)
			return off;
		off += debug_struct_die_size(st);
		st++;
	}
	return 0;
}

static int
debug_array_die_size(void)
{
	return 1 + 4 + 1 + 4 + 1;
}

static int
debug_array_type_offset(const DebugUnit *du, int elem_type_id, const char *elem_struct_name, int count)
{
	int off = 12 + 23 + debug_builtin_types_size();

	if (!du || count <= 0)
		return 0;
	if (elem_struct_name && elem_struct_name[0])
		elem_type_id = DBG_TYPE_NONE;
	else if (elem_type_id == DBG_TYPE_NONE)
		elem_type_id = DBG_TYPE_INT;
	for (int i = 0; i < du->struct_count; i++)
		off += debug_struct_die_size(&du->structs[i]);
	for (int i = 0; i < du->array_count; i++) {
		int same_struct = 0;
		if (!du->arrays[i].elem_struct_name && (!elem_struct_name || !elem_struct_name[0]))
			same_struct = 1;
		else if (du->arrays[i].elem_struct_name && elem_struct_name &&
		         strcmp(du->arrays[i].elem_struct_name, elem_struct_name) == 0)
			same_struct = 1;
		if (same_struct && du->arrays[i].elem_type_id == elem_type_id &&
		    du->arrays[i].count == count)
			return off;
		off += debug_array_die_size();
	}
	return 0;
}

static int
debug_struct_pointer_die_size(void)
{
	return 1 + 4 + 1;
}

static int
debug_struct_pointer_type_offset(const DebugUnit *du, const char *struct_name, int pointer_depth)
{
	int off = 12 + 23 + debug_builtin_types_size();

	if (!du || !struct_name || !struct_name[0])
		return 0;
	if (pointer_depth <= 0)
		pointer_depth = 1;
	for (int i = 0; i < du->struct_count; i++)
		off += debug_struct_die_size(&du->structs[i]);
	for (int i = 0; i < du->array_count; i++)
		off += debug_array_die_size();
	for (int i = 0; i < du->struct_ptr_count; i++) {
		if (du->struct_ptrs[i].struct_name &&
		    strcmp(du->struct_ptrs[i].struct_name, struct_name) == 0 &&
		    du->struct_ptrs[i].pointer_depth == pointer_depth)
			return off;
		off += debug_struct_pointer_die_size();
	}
	return 0;
}

void
debug_unit_emit_debug_str_section(const DebugUnit *du)
{
	if (!du || du->string_count <= 0)
		return;

	printf("    %s\n", TCC_ASM_DEBUG_STR_SECTION);
	for (int i = 0; i < du->string_count; i++)
		debug_emit_string_bytes(du->strings[i].text);
	printf("    .text\n");
}

static void
debug_emit_abbrev_attr(unsigned attr, unsigned form)
{
	printf("    .uleb128 %u\n", attr);
	printf("    .uleb128 %u\n", form);
}

void
debug_unit_emit_debug_abbrev_section(void)
{
	printf("    %s\n", TCC_ASM_DEBUG_ABBREV_SECTION);

	/* Abbrev 1: DW_TAG_compile_unit, DW_CHILDREN_yes. */
	printf("    .uleb128 1\n");
	printf("    .uleb128 0x11\n");  /* DW_TAG_compile_unit */
	printf("    .byte 1\n");         /* DW_CHILDREN_yes */
	debug_emit_abbrev_attr(0x25, 0x0e); /* DW_AT_producer, DW_FORM_strp */
	debug_emit_abbrev_attr(0x03, 0x0e); /* DW_AT_name, DW_FORM_strp */
	debug_emit_abbrev_attr(0x1b, 0x0e); /* DW_AT_comp_dir, DW_FORM_strp */
	debug_emit_abbrev_attr(0x11, 0x1b); /* DW_AT_low_pc, DW_FORM_addrx */
	debug_emit_abbrev_attr(0x12, 0x1b); /* DW_AT_high_pc, DW_FORM_addrx */
	debug_emit_abbrev_attr(0x10, 0x17); /* DW_AT_stmt_list, DW_FORM_sec_offset */
	debug_emit_abbrev_attr(0x73, 0x17); /* DW_AT_addr_base, DW_FORM_sec_offset */
	printf("    .uleb128 0\n");
	printf("    .uleb128 0\n");

	/* Abbrev 2: DW_TAG_subprogram, DW_CHILDREN_yes (for params). */
	printf("    .uleb128 2\n");
	printf("    .uleb128 0x2e\n");  /* DW_TAG_subprogram */
	printf("    .byte 1\n");         /* DW_CHILDREN_yes */
	debug_emit_abbrev_attr(0x03, 0x0e); /* DW_AT_name, DW_FORM_strp */
	debug_emit_abbrev_attr(0x3a, 0x0b); /* DW_AT_decl_file, DW_FORM_data1 */
	debug_emit_abbrev_attr(0x3b, 0x05); /* DW_AT_decl_line, DW_FORM_data2 */
	debug_emit_abbrev_attr(0x11, 0x1b); /* DW_AT_low_pc, DW_FORM_addrx */
	debug_emit_abbrev_attr(0x12, 0x1b); /* DW_AT_high_pc, DW_FORM_addrx */
	debug_emit_abbrev_attr(0x40, 0x18); /* DW_AT_frame_base, DW_FORM_exprloc */
	printf("    .uleb128 0\n");
	printf("    .uleb128 0\n");

	/* Abbrev 3: DW_TAG_formal_parameter, DW_CHILDREN_no, unnamed. */
	printf("    .uleb128 3\n");
	printf("    .uleb128 0x05\n");  /* DW_TAG_formal_parameter */
	printf("    .byte 0\n");         /* DW_CHILDREN_no */
	debug_emit_abbrev_attr(0x02, 0x18); /* DW_AT_location, DW_FORM_exprloc */
	debug_emit_abbrev_attr(0x49, 0x13); /* DW_AT_type, DW_FORM_ref4 */
	printf("    .uleb128 0\n");
	printf("    .uleb128 0\n");

	/* Abbrev 4: DW_TAG_formal_parameter, DW_CHILDREN_no, named. */
	printf("    .uleb128 4\n");
	printf("    .uleb128 0x05\n");  /* DW_TAG_formal_parameter */
	printf("    .byte 0\n");         /* DW_CHILDREN_no */
	debug_emit_abbrev_attr(0x03, 0x0e); /* DW_AT_name, DW_FORM_strp */
	debug_emit_abbrev_attr(0x02, 0x18); /* DW_AT_location, DW_FORM_exprloc */
	debug_emit_abbrev_attr(0x49, 0x13); /* DW_AT_type, DW_FORM_ref4 */
	printf("    .uleb128 0\n");
	printf("    .uleb128 0\n");

	/* Abbrev 5: DW_TAG_variable, DW_CHILDREN_no, named local. */
	printf("    .uleb128 5\n");
	printf("    .uleb128 0x34\n");  /* DW_TAG_variable */
	printf("    .byte 0\n");         /* DW_CHILDREN_no */
	debug_emit_abbrev_attr(0x03, 0x0e); /* DW_AT_name, DW_FORM_strp */
	debug_emit_abbrev_attr(0x02, 0x18); /* DW_AT_location, DW_FORM_exprloc */
	printf("    .uleb128 0\n");
	printf("    .uleb128 0\n");

	/* Abbrev 6: DW_TAG_base_type. */
	printf("    .uleb128 6\n");
	printf("    .uleb128 0x24\n");
	printf("    .byte 0\n");
	debug_emit_abbrev_attr(0x03, 0x0e); /* DW_AT_name, DW_FORM_strp */
	debug_emit_abbrev_attr(0x3e, 0x0b); /* DW_AT_encoding, DW_FORM_data1 */
	debug_emit_abbrev_attr(0x0b, 0x0b); /* DW_AT_byte_size, DW_FORM_data1 */
	printf("    .uleb128 0\n");
	printf("    .uleb128 0\n");

	/* Abbrev 7: DW_TAG_pointer_type. */
	printf("    .uleb128 7\n");
	printf("    .uleb128 0x0f\n");
	printf("    .byte 0\n");
	debug_emit_abbrev_attr(0x49, 0x13); /* DW_AT_type, DW_FORM_ref4 */
	debug_emit_abbrev_attr(0x0b, 0x0b); /* DW_AT_byte_size, DW_FORM_data1 */
	printf("    .uleb128 0\n");
	printf("    .uleb128 0\n");

	/* Abbrev 8: DW_TAG_variable, DW_CHILDREN_no, named local with type. */
	printf("    .uleb128 8\n");
	printf("    .uleb128 0x34\n");
	printf("    .byte 0\n");
	debug_emit_abbrev_attr(0x03, 0x0e); /* DW_AT_name, DW_FORM_strp */
	debug_emit_abbrev_attr(0x02, 0x18); /* DW_AT_location, DW_FORM_exprloc */
	debug_emit_abbrev_attr(0x49, 0x13); /* DW_AT_type, DW_FORM_ref4 */
	printf("    .uleb128 0\n");
	printf("    .uleb128 0\n");

	/* Abbrev 9: DW_TAG_structure_type. */
	printf("    .uleb128 9\n");
	printf("    .uleb128 0x13\n");
	printf("    .byte 1\n");
	debug_emit_abbrev_attr(0x03, 0x0e); /* DW_AT_name, DW_FORM_strp */
	debug_emit_abbrev_attr(0x0b, 0x06); /* DW_AT_byte_size, DW_FORM_data4 */
	printf("    .uleb128 0\n");
	printf("    .uleb128 0\n");

	/* Abbrev 10: DW_TAG_member. */
	printf("    .uleb128 10\n");
	printf("    .uleb128 0x0d\n");
	printf("    .byte 0\n");
	debug_emit_abbrev_attr(0x03, 0x0e); /* DW_AT_name, DW_FORM_strp */
	debug_emit_abbrev_attr(0x49, 0x13); /* DW_AT_type, DW_FORM_ref4 */
	debug_emit_abbrev_attr(0x38, 0x06); /* DW_AT_data_member_location, DW_FORM_data4 */
	printf("    .uleb128 0\n");
	printf("    .uleb128 0\n");

	/* Abbrev 11: DW_TAG_array_type. */
	printf("    .uleb128 11\n");
	printf("    .uleb128 0x01\n");
	printf("    .byte 1\n");
	debug_emit_abbrev_attr(0x49, 0x13); /* DW_AT_type, DW_FORM_ref4 */
	printf("    .uleb128 0\n");
	printf("    .uleb128 0\n");

	/* Abbrev 12: DW_TAG_subrange_type. */
	printf("    .uleb128 12\n");
	printf("    .uleb128 0x21\n");
	printf("    .byte 0\n");
	debug_emit_abbrev_attr(0x37, 0x06); /* DW_AT_count, DW_FORM_data4 */
	printf("    .uleb128 0\n");
	printf("    .uleb128 0\n");

	/* Abbrev 13: DW_TAG_pointer_type to an emitted structure DIE. */
	printf("    .uleb128 13\n");
	printf("    .uleb128 0x0f\n");
	printf("    .byte 0\n");
	debug_emit_abbrev_attr(0x49, 0x13); /* DW_AT_type, DW_FORM_ref4 */
	debug_emit_abbrev_attr(0x0b, 0x0b); /* DW_AT_byte_size, DW_FORM_data1 */
	printf("    .uleb128 0\n");
	printf("    .uleb128 0\n");

	/* Abbrev 14: DW_TAG_pointer_type to void / unspecified type. */
	printf("    .uleb128 14\n");
	printf("    .uleb128 0x0f\n");
	printf("    .byte 0\n");
	debug_emit_abbrev_attr(0x0b, 0x0b); /* DW_AT_byte_size, DW_FORM_data1 */
	printf("    .uleb128 0\n");
	printf("    .uleb128 0\n");

	/* End of abbrev table. */
	printf("    .byte 0\n");
	printf("    .text\n");
}

/* Emit a SLEB128-encoded signed integer. */
static void
debug_emit_sleb128(int value)
{
	int more = 1;
	printf("    .sleb128 %d\n", value);
	(void)more;
}

/* Compute the size in bytes of a SLEB128-encoded value. */
static int
sleb128_size(int value)
{
	int size = 0;
	int v = value;
	do {
		v >>= 7;
		size++;
	} while (v != 0 && v != -1);
	return size;
}

/*
 * Emit a DW_AT_location exprloc using DW_OP_fbreg(offset).
 * DW_OP_fbreg = 0x91, followed by a SLEB128 offset from the frame base.
 * The frame base is the CFA (stack pointer at function entry on arm64).
 * Parameters on arm64 are stored at [fp - 8], [fp - 16], etc.
 * Since DW_AT_frame_base = fp (x29), and params are at negative offsets:
 * param 0: DW_OP_fbreg(-8), param 1: DW_OP_fbreg(-16), etc.
 */
static void 
debug_emit_fbreg_location(int frame_offset)
{
	int sleb_sz = sleb128_size(frame_offset);
	int expr_len = 1 + sleb_sz;  /* DW_OP_fbreg byte + SLEB128 */

	/* DW_FORM_exprloc: ULEB128 length, then expression bytes */
	printf("    .uleb128 %d\n", expr_len);
	printf("    .byte 0x91\n");        /* DW_OP_fbreg */
	debug_emit_sleb128(frame_offset);
}

#ifdef UNUSED
/*
 * Emit DW_AT_location for a source-level struct-by-value parameter that is
 * lowered internally as a hidden pointer.  At function entry the copied struct
 * local may not have been materialized yet, but the hidden ABI slot contains a
 * pointer to the source value.  For DWARF location expressions, the final stack
 * value is the object address, so: fbreg(hidden_slot), deref -> struct address.
 */
static void
debug_emit_fbreg_deref_location(int frame_offset)
{
	int sleb_sz = sleb128_size(frame_offset);
	int expr_len = 2 + sleb_sz;  /* DW_OP_fbreg + SLEB128 + DW_OP_deref */

	printf("    .uleb128 %d\n", expr_len);
	printf("    .byte 0x91\n");        /* DW_OP_fbreg */
	debug_emit_sleb128(frame_offset);
	printf("    .byte 0x06\n");        /* DW_OP_deref */
}
#endif

/*
 * Emit DW_AT_frame_base exprloc: DW_OP_reg29 (x29 = fp on arm64).
 * This tells the debugger that the frame base is the fp register.
 */
static void
debug_emit_frame_base_reg(int dwarf_reg)
{
	/* DW_FORM_exprloc: length=1, DW_OP_regN = 0x50+N. */
	printf("    .uleb128 1\n");
	printf("    .byte 0x%x\n", 0x50 + dwarf_reg);
}

static void
debug_emit_reg_location(int dwarf_reg)
{
	printf("    .uleb128 1\n");
	printf("    .byte 0x%x\n", 0x50 + dwarf_reg);
}

static int
debug_function_frame_offset(const DebugFunction *fn, int raw_offset)
{
	if (!fn)
		return raw_offset;
	return raw_offset + fn->frame_base_bias;
}

int
debug_unit_debug_info_size(const DebugUnit *du)
{
	int size = 0;
	int i;

	if (!du)
		return 0;

	/* DWARF 5 unit header: length(4) + version(2) + unit_type(1) + addr_size(1) + abbrev_off(4) = 12 */
	size += 12;

	/* Compile-unit DIE:
	 *   abbrev(1) + producer(strp=4) + name(strp=4) + comp_dir(strp=4)
	 *   + low_pc(addrx=1) + high_pc(addrx=1) + stmt_list(sec_offset=4)
	 *   + addr_base(sec_offset=4) = 23 */
	size += 23;

	for (int t = DBG_TYPE_INT; t < DBG_TYPE_COUNT; t++)
		size += debug_type_die_size(t);
	for (int st = 0; st < du->struct_count; st++)
		size += debug_struct_die_size(&du->structs[st]);
	for (int at = 0; at < du->array_count; at++)
		size += debug_array_die_size();
	for (int pt = 0; pt < du->struct_ptr_count; pt++)
		size += debug_struct_pointer_die_size();

	for (i = 0; i < du->function_count; i++) {
		const DebugFunction *fn = &du->functions[i];
		/* Subprogram DIE:
		 *   abbrev(1) + name(strp=4) + decl_file(data1=1) + decl_line(data2=2)
		 *   + low_pc(addrx=1) + high_pc(addrx=1) + frame_base(exprloc=2) = 12 */
		size += 12;

		/* Formal parameter DIEs: abbrev + optional name(strp) + location(exprloc) + type. */
		for (int p = 0; p < fn->param_count; p++) {
			int reg = (fn->param_regs && p < fn->param_count)
			        ? fn->param_regs[p]
			        : -1;
			int offset = (fn->param_offsets && fn->param_offsets[p])
			           ? fn->param_offsets[p]
			           : -(p + 1) * 8;
			offset = debug_function_frame_offset(fn, offset);
			int expr_len = reg >= 0 ? 1 : (1 + sleb128_size(offset)); /* regN or fbreg(offset) */


			if (fn->param_names && fn->param_names[p])
				size += 4; /* DW_AT_name strp */
			size += 1 + 1 + expr_len + 4; /* abbrev + exprloc length + expression + DW_AT_type */
		}

		/* Local variable DIEs: abbrev + name(strp) + location(exprloc) + optional type. */
		for (int l = 0; l < fn->local_count; l++) {
			size += 1 + 4 + 1 + 1 +
			        sleb128_size(debug_function_frame_offset(fn, fn->locals[l].offset));
			if (fn->locals[l].type_id != DBG_TYPE_NONE || fn->locals[l].struct_name[0] ||
			    fn->locals[l].pointer_struct_name[0] || fn->locals[l].array_len > 0)
				size += 4;
		}

		/* End-of-children marker for subprogram */
		size += 1;
	}

	/* End-of-children for compile_unit */
	size += 1;

	return size;
}

void
debug_unit_debug_dump_info(const DebugUnit *du)
{
	if (!du)
		return;

	Debug(1, "DWARF info: size=%d functions=%d addr_size=8 abbrev_offset=0 stmt_list=0\n",
	      debug_unit_debug_info_size(du), du->function_count);
	for (int i = 0; i < du->function_count; i++) {
		const DebugFunction *fn = &du->functions[i];
		Debug(1, "DWARF info: subprogram[%d] name=%s line=%d params=%d locals=%d range=L%d..L%d\n",
		      i, fn->name ? fn->name : "<anon>", fn->line,
		      fn->param_count, fn->local_count, fn->low_pc_label, fn->high_pc_label);
	}
}

static void
debug_emit_u32(unsigned value)
{
	printf("    .long %u\n", value);
}

static void
debug_emit_u16(unsigned value)
{
	printf("    .short %u\n", value);
}

/* Index into __debug_addr table for the current compile unit. */
static int g_addr_index = 0;

void
debug_reset_addr_index(void)
{
	g_addr_index = 0;
}

static void
debug_emit_type_die(const DebugUnit *du, int type_id)
{
	if (type_id >= DBG_TYPE_INT && type_id <= DBG_TYPE_USHORT) {
		int encoding = 0x05; /* DW_ATE_signed */

		printf("    .uleb128 6\n");
		debug_emit_u32((unsigned)debug_unit_string_offset(du, debug_type_name(type_id)));
		if (type_id == DBG_TYPE_UCHAR ||
		    type_id == DBG_TYPE_USHORT ||
		    type_id == DBG_TYPE_UINT)
			encoding = 0x07; /* DW_ATE_unsigned */
		else if (type_id == DBG_TYPE_FLOAT || type_id == DBG_TYPE_DOUBLE)
			encoding = 0x04; /* DW_ATE_float */
		printf("    .byte %d\n", encoding);
		printf("    .byte %d\n", debug_base_type_byte_size(type_id));
		return;
	}
	if (debug_type_is_pointer(type_id)) {
		int base = debug_pointer_base_type(type_id);

		if (type_id == DBG_TYPE_PTR_VOID || base == DBG_TYPE_NONE) {
			printf("    .uleb128 14\n");
			printf("    .byte 8\n");
			return;
		}

		printf("    .uleb128 7\n");
		debug_emit_u32((unsigned)debug_type_offset(base));
		printf("    .byte 8\n");
	}
}

void
debug_unit_emit_debug_info_section(const DebugUnit *du)
{
	int low_label = 0;
	int high_label = 0;
	int i;

	if (!du)
		return;
	/* low_label/high_label used only for __debug_addr emission */
	if (du->function_count > 0) {
		low_label  = du->functions[0].low_pc_label;
		high_label = du->functions[du->function_count - 1].high_pc_label;
	}

	/* Emit __debug_addr section first.  It holds the absolute function
	 * addresses as .quad entries, which keeps absolute relocations out of
	 * __debug_info (where Mach-O ld/dsymutil rejects them).
	 * __debug_info references these via DW_FORM_addrx indices.
	 *
	 * DWARF 5 __debug_addr header:
	 *   length(4) + version(2) + addr_size(1) + seg_selector_size(1) = 8 bytes
	 * Then one .quad per function. */
	printf("    %s\n", TCC_ASM_DEBUG_ADDR_SECTION);
	printf("Ltcc_debug_addr_start:\n");
	/* DWARF 5 __debug_addr contribution header:
	 *   length(4) + version(2) + addr_size(1) + seg_selector_size(1) = 8 bytes
	 * followed by one .quad per address entry.
	 * All absolute address references go here so __debug_info stays relocation-free. */
	printf("    .long Ltcc_debug_addr_end - Ltcc_debug_addr_hdr_start\n");
	printf("Ltcc_debug_addr_hdr_start:\n");
	printf("    .short 5\n");   /* DWARF version */
	printf("    .byte 8\n");    /* address size */
	printf("    .byte 0\n");    /* segment selector size */
	printf("Ltcc_addr_table_base:\n");
	/* index 0: compile unit low_pc — use first function's symbol name so
	 * dsymutil can match the address to a known symbol. */
	if (du->function_count > 0 && du->functions[0].name)
		printf("    .quad %s%s\n", TCC_ASM_SYM_PREFIX, du->functions[0].name);
	else if (low_label > 0)
		printf("    .quad L%d\n", low_label);
	else
		printf("    .quad 0\n");
	/* index 1: compile unit high_pc — use last function's end label */
	if (high_label > 0)
		printf("    .quad L%d\n", high_label);
	else
		printf("    .quad 0\n");
	/* indices 2*i+2, 2*i+3: per-function low_pc (symbol) and high_pc (end label) */
	for (i = 0; i < du->function_count; i++) {
		const DebugFunction *fn = &du->functions[i];
		/* Use the function symbol so dsymutil can map DIE to symbol */
		if (fn->name)
			printf("    .quad %s%s\n", TCC_ASM_SYM_PREFIX, fn->name);
		else if (fn->low_pc_label > 0)
			printf("    .quad L%d\n", fn->low_pc_label);
		else
			printf("    .quad 0\n");
		printf("    .quad L%d\n", fn->high_pc_label > 0 ? fn->high_pc_label : 0);
	}
	printf("Ltcc_debug_addr_end:\n");
	printf("    .text\n");

	/* Reset the addrx index counter before emitting __debug_info. */
	debug_reset_addr_index();

	printf("    %s\n", TCC_ASM_DEBUG_INFO_SECTION);

	/* DWARF 5 unit header:
	 *   unit_length excludes the length field itself.  Let the assembler
	 *   compute this from labels so expression-size accounting cannot drift
	 *   from the bytes emitted below. */
	printf("    .long Ltcc_debug_info_end - Ltcc_debug_info_start\n");
	printf("Ltcc_debug_info_start:\n");
	debug_emit_u16(5);       /* DWARF version 5 */
	printf("    .byte 1\n"); /* DW_UT_compile */
	printf("    .byte 8\n"); /* address size = 8 */
	debug_emit_u32(0);       /* .debug_abbrev offset */

	/* compile_unit DIE (abbrev 1) */
	printf("    .uleb128 1\n");
	debug_emit_u32((unsigned)debug_unit_string_offset(du, du->producer));
	debug_emit_u32((unsigned)debug_unit_string_offset(du, du->source_file));
	debug_emit_u32((unsigned)debug_unit_string_offset(du, du->comp_dir ? du->comp_dir : ""));
	printf("    .uleb128 %d\n", g_addr_index++); /* DW_AT_low_pc:  index 0 in addr table */
	printf("    .uleb128 %d\n", g_addr_index++); /* DW_AT_high_pc: index 1 in addr table */
	debug_emit_u32(0);       /* .debug_line offset = 0 (assembler generates it) */
	/* DW_AT_addr_base: offset from start of .debug_addr section to the
	 * address table entries.  Our __debug_addr header is always 8 bytes
	 * (length=4, version=2, addr_size=1, seg_selector_size=1), so the
	 * table always starts at offset 8.  Use a constant to avoid emitting
	 * a cross-section relocation into __debug_info. */
	debug_emit_u32(8);

	for (int t = DBG_TYPE_INT; t < DBG_TYPE_COUNT; t++)
		debug_emit_type_die(du, t);
	for (int st = 0; st < du->struct_count; st++) {
		const DebugStructType *dst = &du->structs[st];
		printf("    .uleb128 9\n");
		debug_emit_u32((unsigned)debug_unit_string_offset(du, dst->name));
		debug_emit_u32((unsigned)dst->byte_size);
		for (int m = 0; m < dst->member_count; m++) {
			const DebugStructMember *dm = &dst->members[m];
			printf("    .uleb128 10\n");
			debug_emit_u32((unsigned)debug_unit_string_offset(du, dm->name));
			if (dm->array_len > 0)
				debug_emit_u32((unsigned)debug_array_type_offset(du,
				               dm->array_elem_type_id,
				               dm->array_elem_struct_name,
				               dm->array_len));
			else if (dm->struct_name && dm->struct_name[0])
				debug_emit_u32((unsigned)debug_struct_type_offset(du, dm->struct_name));
			else
				debug_emit_u32((unsigned)debug_type_offset(dm->type_id));
			debug_emit_u32((unsigned)dm->offset);
		}
		printf("    .byte 0\n");
	}
	for (int at = 0; at < du->array_count; at++) {
		int elem_type = du->arrays[at].elem_type_id == DBG_TYPE_NONE
		              ? DBG_TYPE_INT : du->arrays[at].elem_type_id;
		printf("    .uleb128 11\n");
		if (du->arrays[at].elem_struct_name && du->arrays[at].elem_struct_name[0])
			debug_emit_u32((unsigned)debug_struct_type_offset(du, du->arrays[at].elem_struct_name));
		else
			debug_emit_u32((unsigned)debug_type_offset(elem_type));
		printf("    .uleb128 12\n");
		debug_emit_u32((unsigned)du->arrays[at].count);
		printf("    .byte 0\n");
	}
	for (int pt = 0; pt < du->struct_ptr_count; pt++) {
		printf("    .uleb128 13\n");
		if (du->struct_ptrs[pt].pointer_depth <= 1)
			debug_emit_u32((unsigned)debug_struct_type_offset(du, du->struct_ptrs[pt].struct_name));
		else
			debug_emit_u32((unsigned)debug_struct_pointer_type_offset(du,
			               du->struct_ptrs[pt].struct_name,
			               du->struct_ptrs[pt].pointer_depth - 1));
		printf("    .byte 8\n");
	}

	for (i = 0; i < du->function_count; i++) {
		const DebugFunction *fn = &du->functions[i];
		int p;

		/* subprogram DIE (abbrev 2, children=yes) */
		printf("    .uleb128 2\n");
		debug_emit_u32((unsigned)debug_unit_string_offset(du, fn->name));
		printf("    .byte 1\n");   /* DW_AT_decl_file = 1 */
		printf("    .short %d\n", fn->line > 0 ? fn->line : 0); /* DW_AT_decl_line */
		printf("    .uleb128 %d\n", g_addr_index++); /* DW_AT_low_pc:  next index in addr table */
		printf("    .uleb128 %d\n", g_addr_index++); /* DW_AT_high_pc: next index in addr table */
		debug_emit_frame_base_reg(fn->frame_base_reg ? fn->frame_base_reg : 29);

		/* formal_parameter DIEs — one per parameter. */
		for (p = 0; p < fn->param_count; p++) {
			int reg = (fn->param_regs && p < fn->param_count)
			        ? fn->param_regs[p]
			        : -1;
			int offset = (fn->param_offsets && fn->param_offsets[p])
			           ? fn->param_offsets[p]
			           : -(p + 1) * 8;
			offset = debug_function_frame_offset(fn, offset);

			if (fn->param_names && fn->param_names[p]) {
				printf("    .uleb128 4\n");
				debug_emit_u32((unsigned)debug_unit_string_offset(du, fn->param_names[p]));
			} else {
				printf("    .uleb128 3\n");
			}

			if (fn->param_struct_names &&
			    fn->param_struct_names[p] &&
			    fn->param_struct_names[p][0]) {
				/*
				 * Struct-by-value parameters are materialised in the stack frame.
				 * DW_OP_fbreg describes the address of that stack object.  Do not
				 * append DW_OP_deref here: for an aggregate value that makes LLDB
				 * treat the first word of the struct as a pointer, yielding bogus
				 * member values at a function-name breakpoint.
				 */
				if (reg >= 0)
					debug_emit_reg_location(reg);
				else
					debug_emit_fbreg_location(offset);
				debug_emit_u32((unsigned)debug_struct_type_offset(du,
				               fn->param_struct_names[p]));
			} else if (fn->param_pointer_struct_names &&
			           fn->param_pointer_struct_names[p] &&
			           fn->param_pointer_struct_names[p][0]) {
				if (reg >= 0)
					debug_emit_reg_location(reg);
				else
					debug_emit_fbreg_location(offset);
				debug_emit_u32((unsigned)debug_struct_pointer_type_offset(du,
				               fn->param_pointer_struct_names[p],
				               fn->param_pointer_depths ? fn->param_pointer_depths[p] : 1));
			} else {
				if (reg >= 0)
					debug_emit_reg_location(reg);
				else
					debug_emit_fbreg_location(offset);
				debug_emit_u32((unsigned)debug_type_offset(fn->param_type_ids ? fn->param_type_ids[p] : DBG_TYPE_INT));
			}
		}

		/* local variable DIEs — one per named stack local. */
		for (int l = 0; l < fn->local_count; l++) {
			if (!fn->locals[l].name)
				continue;
			if (fn->locals[l].type_id != DBG_TYPE_NONE || fn->locals[l].struct_name[0] ||
			    fn->locals[l].pointer_struct_name[0] || fn->locals[l].array_len > 0)
				printf("    .uleb128 8\n");
			else
				printf("    .uleb128 5\n");
			debug_emit_u32((unsigned)debug_unit_string_offset(du, fn->locals[l].name));
			debug_emit_fbreg_location(debug_function_frame_offset(fn, fn->locals[l].offset));
			if (fn->locals[l].struct_name[0])
				debug_emit_u32((unsigned)debug_struct_type_offset(du, fn->locals[l].struct_name));
			else if (fn->locals[l].pointer_struct_name[0])
				debug_emit_u32((unsigned)debug_struct_pointer_type_offset(du,
				               fn->locals[l].pointer_struct_name,
				               fn->locals[l].pointer_depth));
			else if (fn->locals[l].array_len > 0)
				debug_emit_u32((unsigned)debug_array_type_offset(du,
                                fn->locals[l].array_elem_type_id,
                                fn->locals[l].array_elem_struct_name,
                                fn->locals[l].array_len));
			else if (fn->locals[l].type_id != DBG_TYPE_NONE)
				debug_emit_u32((unsigned)debug_type_offset(fn->locals[l].type_id));
		}

		/* End-of-children for subprogram */
		printf("    .byte 0\n");
	}

	/* End-of-children for compile_unit */
	printf("    .byte 0\n");
	printf("Ltcc_debug_info_end:\n");
	printf("    .text\n");
}

void
debug_unit_debug_dump(const DebugUnit *du)
{
	if (!du)
		return;

	Debug(1, "DWARF model: source=%s producer=%s functions=%d strings=%d\n",
	      du->source_file ? du->source_file : "<unknown>",
	      du->producer ? du->producer : "<unknown>",
	      du->function_count,
	      du->string_count);

	for (int i = 0; i < du->function_count; i++) {
		const DebugFunction *fn = &du->functions[i];
		Debug(1, "DWARF model: function[%d] name=%s file=%s line=%d params=%d range=L%d..L%d\n",
		      i,
		      fn->name ? fn->name : "<anonymous>",
		      fn->file ? fn->file : "<unknown>",
		      fn->line,
		      fn->param_count,
		      fn->low_pc_label,
		      fn->high_pc_label);
	}

	debug_unit_debug_dump_strings(du);
	Debug(1, "DWARF abbrev: code=1 tag=compile_unit children=yes "
	         "attrs=producer,name,low_pc,high_pc(data8),stmt_list\n");
	Debug(1, "DWARF abbrev: code=2 tag=subprogram children=yes "
	         "attrs=name,decl_file,decl_line,low_pc,high_pc(data8),frame_base\n");
	Debug(1, "DWARF abbrev: code=3 tag=formal_parameter children=no "
	         "attrs=location(exprloc)\n");
	Debug(1, "DWARF abbrev: code=4 tag=formal_parameter children=no "
	         "attrs=name,location(exprloc)\n");
	Debug(1, "DWARF abbrev: code=5 tag=variable children=no "
	         "attrs=name,location(exprloc)\n");
	Debug(1, "DWARF abbrev: size=%d\n", debug_unit_debug_abbrev_size());
	debug_unit_debug_dump_info(du);
}
