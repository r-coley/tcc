#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <stdarg.h>

#include "tcc.h"
#include "ast.h"
#include "lexer.h"

extern int struct_alignof_name(const char *name);

static Type builtin_void   = { TY_VOID, 0, 0, 0, 0, 0, "", TYPE_SOURCE_VOID, "void", 0, "", NULL };
static Type builtin_int    = { TY_INT, 0, 4, 0, 0, 0, "", TYPE_SOURCE_DEFAULT, "", 0, "", NULL };
static Type builtin_uint   = { TY_INT, 0, 4, 1, 0, 0, "", TYPE_SOURCE_DEFAULT, "", 0, "", NULL };
static Type builtin_char   = { TY_CHAR, 0, 1, 0, 0, 0, "", TYPE_SOURCE_DEFAULT, "", 0, "", NULL };
static Type builtin_uchar  = { TY_CHAR, 0, 1, 1, 0, 0, "", TYPE_SOURCE_DEFAULT, "", 0, "", NULL };
static Type builtin_short  = { TY_SHORT, 0, 2, 0, 0, 0, "", TYPE_SOURCE_DEFAULT, "", 0, "", NULL };
static Type builtin_ushort = { TY_SHORT, 0, 2, 1, 0, 0, "", TYPE_SOURCE_DEFAULT, "", 0, "", NULL };
static Type builtin_float  = { TY_FLOAT, 0, 4, 0, 0, 0, "", TYPE_SOURCE_FLOAT, "float", 0, "", NULL };
static Type builtin_double = { TY_DOUBLE, 0, 8, 0, 0, 0, "", TYPE_SOURCE_DOUBLE, "double", 0, "", NULL };
static Type builtin_long   = { TY_INT, 0, 8, 0, 0, 0, "", TYPE_SOURCE_LONG,  "long", 0, "", NULL };
static Type builtin_ulong  = { TY_INT, 0, 8, 1, 0, 0, "", TYPE_SOURCE_ULONG, "unsigned long", 0, "", NULL };
static Type builtin_llong  = { TY_INT, 0, 8, 0, 0, 0, "", TYPE_SOURCE_LLONG, "long long", 0, "", NULL };
static Type builtin_ullong = { TY_INT, 0, 8, 1, 0, 0, "", TYPE_SOURCE_ULLONG,"unsigned long long", 0, "", NULL };

typedef struct FuncTypeMeta {
	const Type *owner;
	Type **param_types;
	int param_count;
	int is_variadic;
	int fixed_param_count;
	int has_prototype;
	struct FuncTypeMeta *next;
} FuncTypeMeta;

static FuncTypeMeta *func_type_meta_head;

typedef struct PtrTypeCacheEntry {
	Type *base;
	Type *ptr_type;
	struct PtrTypeCacheEntry *next;
} PtrTypeCacheEntry;

#define PTR_TYPE_CACHE_BUCKETS 4096

static PtrTypeCacheEntry *ptr_type_cache_buckets[PTR_TYPE_CACHE_BUCKETS];

static FuncTypeMeta *
find_func_type_meta(const Type *type)
{
	for (FuncTypeMeta *meta = func_type_meta_head; meta; meta = meta->next) {
		if (meta->owner == type)
			return meta;
	}
	return NULL;
}

static void
copy_func_type_meta(const Type *src_owner, Type *dst_owner)
{
	FuncTypeMeta *src_meta;
	FuncTypeMeta *dst_meta;

	if (!src_owner || !dst_owner)
		return;

	src_meta = find_func_type_meta(src_owner);
	if (!src_meta)
		return;

	dst_meta = xcalloc(1, sizeof(FuncTypeMeta));
	dst_meta->owner = dst_owner;
	dst_meta->param_types = src_meta->param_types;
	dst_meta->param_count = src_meta->param_count;
	dst_meta->is_variadic = src_meta->is_variadic;
	dst_meta->fixed_param_count = src_meta->fixed_param_count;
	dst_meta->has_prototype = src_meta->has_prototype;
	dst_meta->next = func_type_meta_head;
	func_type_meta_head = dst_meta;
}

static int
type_is_object_pointer_conditional(const Type *type)
{
	Type *base;

	if (!type || !type_is_pointer(type))
		return 0;
	base = type_pointee(type);
	return base && !type_is_function(base);
}

static int
type_contains_prototype_function(const Type *type)
{
	if (!type)
		return 0;

	if (type_is_function(type)) {
		if (type_func_metadata(type, NULL, NULL, NULL, NULL))
			return 1;
		return type_contains_prototype_function(type->base);
	}

	if (type_is_pointer(type) || type_is_array(type))
		return type_contains_prototype_function(type->base);

	return 0;
}

static int
type_contains_function(const Type *type)
{
	if (!type)
		return 0;

	if (type_is_function(type))
		return 1;

	if (type_is_pointer(type) || type_is_array(type))
		return type_contains_function(type->base);

	return 0;
}

static int
type_signature_component_compatible_impl(const Type *a, const Type *b,
                                         int include_qualifiers,
                                         int ignore_top_level_qualifiers);

Type *
type_pointer_conditional_result(Type *a, Type *b)
{
	Type *a_base;
	Type *b_base;
	int a_from_b;
	int b_from_a;
	int a_has_proto;
	int b_has_proto;

	if (!a || !b || !type_is_pointer(a) || !type_is_pointer(b))
		return NULL;

	a_base = type_pointee(a);
	b_base = type_pointee(b);

	if ((type_is_void(a_base) || type_is_void(b_base)) &&
	    type_is_object_pointer_conditional(a) &&
	    type_is_object_pointer_conditional(b)) {
		int quals = type_qualifiers(a_base) | type_qualifiers(b_base);
		return type_ptr(type_with_qualifiers(type_void(), quals));
	}

	if (a_base && b_base &&
	    !type_is_function(a_base) && !type_is_function(b_base) &&
	    !type_contains_function(a_base) && !type_contains_function(b_base) &&
	    type_signature_component_compatible_impl(a_base, b_base, 1, 1)) {
		int quals = type_qualifiers(a_base) | type_qualifiers(b_base);
		return type_ptr(type_with_qualifiers(a_base, quals));
	}

	a_from_b = type_pointer_assignment_compatible(a, b, 0);
	b_from_a = type_pointer_assignment_compatible(b, a, 0);

	a_has_proto = type_contains_prototype_function(a);
	b_has_proto = type_contains_prototype_function(b);

	if (a_from_b && !b_from_a)
		return a;
	if (b_from_a && !a_from_b)
		return b;
	if (a_from_b && b_from_a) {
		if (a_has_proto && !b_has_proto)
			return a;
		if (b_has_proto && !a_has_proto)
			return b;
		return a;
	}
	return NULL;
}

Type *
type_void(void)
{
	return &builtin_void;
}

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
type_float(void)
{
	return &builtin_float;
}

Type *
type_double(void)
{
	return &builtin_double;
}

Type *
type_long(void)
{
	return &builtin_long;
}

Type *
type_ulong(void)
{
	return &builtin_ulong;
}

Type *
type_llong(void)
{
	return &builtin_llong;
}

Type *
type_ullong(void)
{
	return &builtin_ullong;
}

Type *
type_ptr(Type *base)
{
	unsigned long key = (unsigned long)(size_t)base;
	unsigned long bucket;

	/* Mix pointer entropy from higher bits before bucket selection. */
	key ^= key >> 17;
	key ^= key >> 9;
	bucket = key & (PTR_TYPE_CACHE_BUCKETS - 1);

	for (PtrTypeCacheEntry *entry = ptr_type_cache_buckets[bucket]; entry; entry = entry->next) {
		if (entry->base == base)
			return entry->ptr_type;
	}

	Type *t = xcalloc(1, sizeof(Type));
	PtrTypeCacheEntry *entry = xcalloc(1, sizeof(PtrTypeCacheEntry));
	t->kind = TY_PTR;
	t->base = base;
	t->size = 8;
	t->is_unsigned = 1;
	entry->base = base;
	entry->ptr_type = t;
	entry->next = ptr_type_cache_buckets[bucket];
	ptr_type_cache_buckets[bucket] = entry;
	return t;
}

Type *
type_array(Type *base, int len)
{
	Type *t = xcalloc(1, sizeof(Type));
	t->kind = TY_ARRAY;
	t->base = base;
	t->array_len = len;
	if (!base)
		fatal_cur("Array type requires an element type\n");
	if (len > 0 && base->size > 2147483647 / len)
		fatal_cur("Array size overflow\n");
	t->size = len > 0 ? base->size * len : 0;
	t->is_unsigned = base ? base->is_unsigned : 0;
	return t;
}

Type *
type_func(Type *ret_type)
{
	Type *t = xcalloc(1, sizeof(Type));
	t->kind = TY_FUNC;
	t->base = ret_type;
	t->size = 0;
	return t;
}

Type *
type_func_proto(Type *ret_type, Type **param_types, int param_count,
                int is_variadic, int fixed_param_count)
{
	Type *t = type_func(ret_type);
	FuncTypeMeta *meta = xcalloc(1, sizeof(FuncTypeMeta));

	meta->owner = t;
	meta->param_types = param_types;
	meta->param_count = param_count;
	meta->is_variadic = is_variadic;
	meta->fixed_param_count = fixed_param_count;
	meta->has_prototype = 1;
	meta->next = func_type_meta_head;
	func_type_meta_head = meta;

	return t;
}

int
type_func_metadata(const Type *type, Type ***out_param_types,
                   int *out_param_count, int *out_is_variadic,
                   int *out_fixed_param_count)
{
	FuncTypeMeta *meta;

	if (out_param_types)
		*out_param_types = NULL;
	if (out_param_count)
		*out_param_count = 0;
	if (out_is_variadic)
		*out_is_variadic = 0;
	if (out_fixed_param_count)
		*out_fixed_param_count = 0;
	if (!type || !type_is_function(type))
		return 0;

	meta = find_func_type_meta(type);
	if (!meta || !meta->has_prototype)
		return 0;

	if (out_param_types)
		*out_param_types = meta->param_types;
	if (out_param_count)
		*out_param_count = meta->param_count;
	if (out_is_variadic)
		*out_is_variadic = meta->is_variadic;
	if (out_fixed_param_count)
		*out_fixed_param_count = meta->fixed_param_count;
	return 1;
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

Type *
type_union(const char *name, int size)
{
	Type *t = xcalloc(1, sizeof(Type));
	t->kind = TY_UNION;
	t->size = size;
	STRNCPY(t->struct_name, name, sizeof(t->struct_name) - 1);
	return t;
}

Type *
type_enum(const char *name)
{
	Type *t = xcalloc(1, sizeof(Type));
	t->kind = TY_ENUM;
	t->size = 4;
	if (name && name[0])
		STRNCPY(t->struct_name, name, sizeof(t->struct_name) - 1);
	return t;
}

Type *
type_with_source(Type *type, int source_kind, const char *source_name)
{
	Type *copy;

	if (!type)
		return NULL;

	copy = xcalloc(1, sizeof(Type));
	*copy = *type;
	copy_func_type_meta(type, copy);
	copy->source_kind = source_kind;
	copy->source_name[0] = '\0';
	if (source_name && source_name[0])
		STRNCPY(copy->source_name, source_name, sizeof(copy->source_name) - 1);
	return copy;
}

Type *
type_with_qualifiers(Type *type, int qualifiers)
{
	Type *copy;

	if (!type)
		return NULL;
	if (!qualifiers)
		return type;

	copy = xcalloc(1, sizeof(Type));
	*copy = *type;
	copy_func_type_meta(type, copy);
	copy->qualifiers |= qualifiers;
	return copy;
}

int
type_sizeof(const Type *type)
{
	if (!type)
		return 0;
	if (type->kind == TY_VOID)
		return 0;
	if (type_is_complex(type))
		return type->size * 2;
	return type->size;
}

int
type_alignof(const Type *type)
{
	if (type_is_complex(type))
		return type->size >= 8 ? 8 : type->size;

	if (type_is_array(type) && type->base)
		return type_alignof(type->base);

	if (type_is_struct(type) && type->struct_name[0]) {
		int align = struct_alignof_name(type->struct_name);
		if (align > 0)
			return align;
	}

	int size = type_sizeof(type);

	if (size >= 8)
		return 8;
	if (size >= 4)
		return 4;
	if (size >= 2)
		return 2;
	return 1;
}

int
type_is_void(const Type *type)
{
	return type && type->kind == TY_VOID;
}

int
type_is_integer(const Type *type)
{
	return type &&
	       (type->kind == TY_INT || type->kind == TY_CHAR ||
	        type->kind == TY_SHORT || type->kind == TY_ENUM);
}

int
type_is_floating(const Type *type)
{
	return type &&
	       !type_is_complex(type) &&
	       !type_is_imaginary(type) &&
	       (type->kind == TY_FLOAT || type->kind == TY_DOUBLE);
}

int
type_is_fp_scalar(const Type *type)
{
	return type &&
	       !type_is_complex(type) &&
	       (type->kind == TY_FLOAT || type->kind == TY_DOUBLE);
}

int
type_is_complex(const Type *type)
{
	return type && type->source_kind == TYPE_SOURCE_COMPLEX;
}

int
type_is_imaginary(const Type *type)
{
	return type && type->source_kind == TYPE_SOURCE_IMAGINARY;
}

int
type_is_pointer(const Type *type)
{
	return type && type->kind == TY_PTR;
}

int
type_is_array(const Type *type)
{
	return type && type->kind == TY_ARRAY;
}

int
type_is_function(const Type *type)
{
	return type && type->kind == TY_FUNC;
}

int
type_is_struct(const Type *type)
{
	return type && (type->kind == TY_STRUCT || type->kind == TY_UNION);
}

int
type_is_union(const Type *type)
{
	return type && type->kind == TY_UNION;
}

int
type_is_enum(const Type *type)
{
	return type && type->kind == TY_ENUM;
}

int
type_is_scalar(const Type *type)
{
	return type_is_integer(type) || type_is_floating(type) ||
	       type_is_complex(type) || type_is_imaginary(type) ||
	       type_is_pointer(type);
}

int
type_is_unsigned(const Type *type)
{
	return type && type->is_unsigned;
}

Type *
type_pointee(const Type *type)
{
	return type_is_pointer(type) || type_is_array(type) ? type->base : NULL;
}

int
type_array_len(const Type *type)
{
	return type_is_array(type) ? type->array_len : 0;
}

int
type_elem_sizeof(const Type *type)
{
	Type *base = type_pointee(type);

	return base ? type_sizeof(base) : type_sizeof(type);
}

int
type_debug_type_id(const Type *type)
{
	if (!type)
		return DBG_TYPE_NONE;
	if (type_is_pointer(type)) {
		Type *base = type_pointee(type);
		if (!base)
			return DBG_TYPE_PTR_VOID;
		switch (base->kind) {
		case TY_VOID:  return DBG_TYPE_PTR_VOID;
		case TY_INT:   return type_is_unsigned(base) ? DBG_TYPE_PTR_UINT : DBG_TYPE_PTR_INT;
		case TY_CHAR:  return type_is_unsigned(base) ? DBG_TYPE_PTR_UCHAR : DBG_TYPE_PTR_CHAR;
		case TY_SHORT: return type_is_unsigned(base) ? DBG_TYPE_PTR_USHORT : DBG_TYPE_PTR_SHORT;
		default:       return DBG_TYPE_PTR_VOID;
		}
	}
	switch (type->kind) {
	case TY_ENUM:  return DBG_TYPE_INT;
	case TY_INT:   return type_is_unsigned(type) ? DBG_TYPE_UINT : DBG_TYPE_INT;
	case TY_CHAR:  return type_is_unsigned(type) ? DBG_TYPE_UCHAR : DBG_TYPE_CHAR;
	case TY_SHORT: return type_is_unsigned(type) ? DBG_TYPE_USHORT : DBG_TYPE_SHORT;
	case TY_FLOAT: return DBG_TYPE_FLOAT;
	case TY_DOUBLE:return DBG_TYPE_DOUBLE;
	default:       return DBG_TYPE_NONE;
	}
}

int
type_source_kind(const Type *type)
{
	return type ? type->source_kind : TYPE_SOURCE_DEFAULT;
}

const char *
type_source_name(const Type *type)
{
	return type && type->source_name[0] ? type->source_name : NULL;
}

int
type_has_source(const Type *type)
{
	return type_source_kind(type) != TYPE_SOURCE_DEFAULT;
}

int
type_source_is(const Type *type, int source_kind)
{
	return type_source_kind(type) == source_kind;
}

int
type_source_is_typedef(const Type *type)
{
	return type_source_is(type, TYPE_SOURCE_TYPEDEF);
}

int
type_source_is_void_spelling(const Type *type)
{
	return type_is_void(type) || type_source_is(type, TYPE_SOURCE_VOID);
}

int
type_source_is_bool_spelling(const Type *type)
{
	return type_source_is(type, TYPE_SOURCE_BOOL);
}

int
type_source_is_integer_spelling(const Type *type)
{
	int kind = type_source_kind(type);

	return kind >= TYPE_SOURCE_BOOL && kind <= TYPE_SOURCE_ULLONG;
}

int
type_source_is_floating_spelling(const Type *type)
{
	return type_source_is(type, TYPE_SOURCE_FLOAT) ||
	       type_source_is(type, TYPE_SOURCE_DOUBLE) ||
	       type_source_is(type, TYPE_SOURCE_LONG_DOUBLE);
}

int
type_source_is_collapsed_scalar(const Type *type)
{
	return type_source_is_void_spelling(type) ||
	       type_source_is_integer_spelling(type) ||
	       type_source_is_floating_spelling(type);
}

const char *
type_source_display_name(const Type *type)
{
	const char *name = type_source_name(type);

	if (name)
		return name;
	switch (type_source_kind(type)) {
	case TYPE_SOURCE_VOID:    return "void";
	case TYPE_SOURCE_BOOL:    return "_Bool";
	case TYPE_SOURCE_SCHAR:   return "signed char";
	case TYPE_SOURCE_LONG:    return "long";
	case TYPE_SOURCE_ULONG:   return "unsigned long";
	case TYPE_SOURCE_LLONG:   return "long long";
	case TYPE_SOURCE_ULLONG:  return "unsigned long long";
	case TYPE_SOURCE_FLOAT:   return "float";
	case TYPE_SOURCE_DOUBLE:  return "double";
	case TYPE_SOURCE_LONG_DOUBLE: return "long double";
	case TYPE_SOURCE_COMPLEX: return "_Complex";
	case TYPE_SOURCE_IMAGINARY: return "_Imaginary";
	case TYPE_SOURCE_TYPEDEF: return "typedef";
	default:                  return NULL;
	}
}

int
type_qualifiers(const Type *type)
{
	return type ? type->qualifiers : 0;
}

int
type_has_qualifier(const Type *type, int qualifier)
{
	return type && (type->qualifiers & qualifier) != 0;
}

static int
type_equal_impl(const Type *a, const Type *b, int include_qualifiers)
{
	int a_source_kind;
	int b_source_kind;

	if (a == b)
		return 1;
	if (!a || !b)
		return 0;
	if (((a->kind == TY_STRUCT || a->kind == TY_UNION) &&
	     (b->kind == TY_STRUCT || b->kind == TY_UNION)) &&
	    STRCMP(a->struct_name, b->struct_name) == 0) {
		if (include_qualifiers && a->qualifiers != b->qualifiers)
			return 0;
		return 1;
	}
	if (a->kind != b->kind)
		return 0;
	if (include_qualifiers && a->qualifiers != b->qualifiers)
		return 0;
	if (type_is_struct(a) || type_is_union(a) || type_is_enum(a))
		return STRCMP(a->struct_name, b->struct_name) == 0;
	if (a->size != b->size ||
	    a->is_unsigned != b->is_unsigned ||
	    a->array_len != b->array_len)
		return 0;
	if (a->kind == TY_CHAR &&
	    type_source_is(a, TYPE_SOURCE_SCHAR) != type_source_is(b, TYPE_SOURCE_SCHAR))
		return 0;
	a_source_kind = type_source_kind(a);
	b_source_kind = type_source_kind(b);
	if (a_source_kind == TYPE_SOURCE_TYPEDEF)
		a_source_kind = TYPE_SOURCE_DEFAULT;
	if (b_source_kind == TYPE_SOURCE_TYPEDEF)
		b_source_kind = TYPE_SOURCE_DEFAULT;
	if (a_source_kind != b_source_kind) {
		if (a_source_kind == TYPE_SOURCE_DEFAULT || b_source_kind == TYPE_SOURCE_DEFAULT) {
			switch (a_source_kind == TYPE_SOURCE_DEFAULT ? b_source_kind : a_source_kind) {
			case TYPE_SOURCE_LONG:
			case TYPE_SOURCE_ULONG:
			case TYPE_SOURCE_LLONG:
			case TYPE_SOURCE_ULLONG:
			case TYPE_SOURCE_BOOL:
			case TYPE_SOURCE_COMPLEX:
			case TYPE_SOURCE_IMAGINARY:
			case TYPE_SOURCE_LONG_DOUBLE:
				return 0;
			default:
				break;
			}
		} else {
			return 0;
		}
	}
	if (type_is_function(a)) {
		Type **a_params = NULL;
		Type **b_params = NULL;
		int a_count = 0;
		int b_count = 0;
		int a_is_variadic = 0;
		int b_is_variadic = 0;
		int a_fixed = 0;
		int b_fixed = 0;
		int a_has_proto = type_func_metadata(a, &a_params, &a_count, &a_is_variadic, &a_fixed);
		int b_has_proto = type_func_metadata(b, &b_params, &b_count, &b_is_variadic, &b_fixed);

		if (a_has_proto != b_has_proto)
			return 0;
		if (a_has_proto &&
		    (a_count != b_count ||
		     a_is_variadic != b_is_variadic ||
		     a_fixed != b_fixed))
			return 0;
		for (int i = 0; i < a_count; i++) {
			if (!type_equal_impl(a_params[i], b_params[i],
			                     include_qualifiers))
				return 0;
		}
	}
	return type_equal_impl(a->base, b->base, include_qualifiers);
}

static int
type_is_oldstyle_promotable_parameter(const Type *type)
{
	if (!type)
		return 0;

	if (type_is_pointer(type) || type_is_function(type))
		return 1;

	if (type->kind == TY_CHAR ||
	    type->kind == TY_SHORT ||
	    type->kind == TY_FLOAT ||
	    type->kind == TY_ENUM)
		return 0;

	return 1;
}

static int
type_is_named_aggregate_or_enum(const Type *type)
{
	if (!type)
		return 0;
	return type->kind == TY_STRUCT ||
	       type->kind == TY_UNION ||
	       type->kind == TY_ENUM;
}

static int
type_function_compatible_impl(const Type *a, const Type *b,
                              int include_qualifiers);

static int
type_signature_component_compatible_impl(const Type *a, const Type *b,
                                         int include_qualifiers,
                                         int ignore_top_level_qualifiers)
{
	if (a == b)
		return 1;
	if (!a || !b)
		return 0;
	if (include_qualifiers &&
	    !ignore_top_level_qualifiers &&
	    a->qualifiers != b->qualifiers)
		return 0;
	if (type_is_named_aggregate_or_enum(a) || type_is_named_aggregate_or_enum(b)) {
		if (a->kind != b->kind)
			return 0;
		return STRCMP(a->struct_name, b->struct_name) == 0;
	}
	if (a->kind != b->kind)
		return 0;
	if (a->size != b->size ||
	    a->is_unsigned != b->is_unsigned ||
	    a->array_len != b->array_len)
		return 0;
	if (a->kind == TY_CHAR &&
	    type_source_is(a, TYPE_SOURCE_SCHAR) != type_source_is(b, TYPE_SOURCE_SCHAR))
		return 0;
	if (type_is_function(a))
		return type_function_compatible_impl(a, b, include_qualifiers);

	if (type_is_pointer(a) || type_is_array(a))
		return type_signature_component_compatible_impl(a->base, b->base,
		                                                include_qualifiers, 0);

	return 1;
}

static int
type_function_compatible_impl(const Type *a, const Type *b, int include_qualifiers)
{
	Type **a_params = NULL;
	Type **b_params = NULL;
	int a_count = 0;
	int b_count = 0;
	int a_is_variadic = 0;
	int b_is_variadic = 0;
	int a_fixed = 0;
	int b_fixed = 0;
	int a_has_proto;
	int b_has_proto;

	if (!type_is_function(a) || !type_is_function(b))
		return 0;

	a_has_proto = type_func_metadata(a, &a_params, &a_count, &a_is_variadic, &a_fixed);
	b_has_proto = type_func_metadata(b, &b_params, &b_count, &b_is_variadic, &b_fixed);

	if (a_has_proto && b_has_proto) {
		if (a_count != b_count ||
		    a_is_variadic != b_is_variadic ||
		    a_fixed != b_fixed)
			return 0;
		for (int i = 0; i < a_count; i++) {
			if (!type_signature_component_compatible_impl(a_params[i], b_params[i],
			                                              include_qualifiers, 1))
				return 0;
		}
		return type_signature_component_compatible_impl(a->base, b->base,
		                                                include_qualifiers, 0);
	}

	if (a_has_proto != b_has_proto) {
		Type **proto_params = a_has_proto ? a_params : b_params;
		int proto_count = a_has_proto ? a_count : b_count;
		int proto_is_variadic = a_has_proto ? a_is_variadic : b_is_variadic;

		if (proto_is_variadic)
			return 0;
		for (int i = 0; i < proto_count; i++) {
			if (!type_is_oldstyle_promotable_parameter(proto_params[i]))
				return 0;
		}
		return type_signature_component_compatible_impl(a->base, b->base,
		                                                include_qualifiers, 0);
	}

	return type_signature_component_compatible_impl(a->base, b->base,
	                                                include_qualifiers, 0);
}

int
type_function_compatible_unqualified(const Type *a, const Type *b)
{
	return type_function_compatible_impl(a, b, 0);
}

int
type_function_compatible_qualified(const Type *a, const Type *b)
{
	return type_function_compatible_impl(a, b, 1);
}

int
type_equal_unqualified(const Type *a, const Type *b)
{
	return type_equal_impl(a, b, 0);
}

int
type_equal_qualified(const Type *a, const Type *b)
{
	return type_equal_impl(a, b, 1);
}

static int
type_is_object_pointer(const Type *type)
{
	Type *base;

	if (!type_is_pointer(type))
		return 0;
	base = type_pointee(type);
	return base && !type_is_function(base);
}

#define type_assignment_relevant_qualifiers(qualifiers) \
	((qualifiers) & (TYPE_QUAL_CONST | TYPE_QUAL_VOLATILE | TYPE_QUAL_RESTRICT))

static int
type_pointed_to_compatible(const Type *dst, const Type *src, int nested);

static int
type_array_assignment_compatible(const Type *dst, const Type *src, int nested)
{
	if (!type_is_array(dst) || !type_is_array(src))
		return 0;
	if (dst->array_len != src->array_len &&
	    dst->array_len != 0 &&
	    src->array_len != 0)
		return 0;
	return type_pointed_to_compatible(dst->base, src->base, nested);
}

static int
type_pointed_to_compatible(const Type *dst, const Type *src, int nested)
{
	int dst_quals;
	int src_quals;

	if (!dst || !src)
		return 0;

	dst_quals = type_assignment_relevant_qualifiers(type_qualifiers(dst));
	src_quals = type_assignment_relevant_qualifiers(type_qualifiers(src));
	if ((dst_quals & src_quals) != src_quals)
		return 0;

	if (type_is_pointer(dst) || type_is_pointer(src)) {
		if (!type_is_pointer(dst) || !type_is_pointer(src))
			return 0;
		return type_pointed_to_compatible(type_pointee(dst), type_pointee(src), 1);
	}

	if (type_is_array(dst) || type_is_array(src)) {
		if (!type_is_array(dst) || !type_is_array(src))
			return 0;
		return type_array_assignment_compatible(dst, src, nested);
	}

	if (type_is_function(dst) || type_is_function(src))
		return type_is_function(dst) &&
		       type_is_function(src) &&
		       type_function_compatible_impl(dst, src, 1);

	if (!nested && (type_is_void(dst) || type_is_void(src)))
		return 1;

	if (!nested &&
	    ((type_is_enum(dst) && type_is_integer(src)) ||
	     (type_is_integer(dst) && type_is_enum(src))) &&
	    type_sizeof(dst) == type_sizeof(src))
		return 1;

	if (nested)
		return type_equal_qualified(dst, src);

	return type_equal_unqualified(dst, src);
}

int
type_pointer_assignment_compatible(const Type *dst, const Type *src,
                                   int src_is_null_pointer_constant)
{
	Type *dst_base;
	Type *src_base;

	if (!type_is_pointer(dst))
		return 0;
	if (src_is_null_pointer_constant)
		return 1;
	if (!type_is_pointer(src))
		return 0;
	dst_base = type_pointee(dst);
	src_base = type_pointee(src);
	if (!tcc_iso_diagnostics && dst_base && src_base &&
	    ((type_is_void(dst_base) && type_is_function(src_base)) ||
	     (type_is_void(src_base) && type_is_function(dst_base)))) {
		int dst_quals = type_assignment_relevant_qualifiers(type_qualifiers(dst_base));
		int src_quals = type_assignment_relevant_qualifiers(type_qualifiers(src_base));
		return (dst_quals & src_quals) == src_quals;
	}
	if ((type_is_void(dst_base) || type_is_void(src_base)) &&
	    type_is_object_pointer(dst) && type_is_object_pointer(src)) {
		int dst_quals = type_assignment_relevant_qualifiers(type_qualifiers(dst_base));
		int src_quals = type_assignment_relevant_qualifiers(type_qualifiers(src_base));
		return (dst_quals & src_quals) == src_quals;
	}
	if (dst_base && src_base &&
	    dst_base->kind == TY_CHAR && src_base->kind == TY_CHAR) {
		int dst_quals = type_assignment_relevant_qualifiers(type_qualifiers(dst_base));
		int src_quals = type_assignment_relevant_qualifiers(type_qualifiers(src_base));
		return (dst_quals & src_quals) == src_quals;
	}
	if (((type_is_struct(dst_base) && type_is_struct(src_base)) ||
	     (type_is_union(dst_base) && type_is_union(src_base))) &&
	    dst_base->struct_name[0] && src_base->struct_name[0] &&
	    STRCMP(dst_base->struct_name, src_base->struct_name) == 0)
		return 1;
	return type_pointed_to_compatible(dst_base, src_base, 0);
}

int
node_is_null_pointer_constant(const Node *node)
{
	if (!node)
		return 0;
	if (node->kind == ND_NUM)
		return node->long_value == 0;
	if (node->kind == ND_CAST && node->left)
		return node_is_null_pointer_constant(node->left);
	if (node->kind == ND_COND && node->then_body && node->else_body)
		return node_is_null_pointer_constant(node->then_body) &&
		       node_is_null_pointer_constant(node->else_body);
	return 0;
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
	node->long_value = (long)value;
	node->type = type_int();
	node->elem_size = TCC_SIZEOF_INT;
	return node;
}

Node *
new_num_long(long value)
{
	Node *node = new_node(ND_NUM);
	node->value = (int)value;
	node->long_value = value;
	node->type = type_int();
	node->elem_size = TCC_SIZEOF_INT;
	return node;
}

Node *
new_num_fp(Type *type, const char *text)
{
	Node *node = new_node(ND_NUM);
	node->value = 0;
	node->long_value = 0;
	node->is_fp_num = 1;
	node->type = type ? type : type_double();
	node->elem_size = node->type ? node->type->size : 8;
	node->is_unsigned = 0;
	if (text) {
		size_t len = strlen(text);
		node->string_value = xmalloc(len + 1);
		memcpy(node->string_value, text, len + 1);
		node->string_len = len;
	}
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
	if (target &&
	    target->kind == ND_MEMBER_PTR &&
	    target->left &&
	    node_is_null_pointer_constant(target->left)) {
		Node *node = new_num(target->offset);
		node->long_value = target->offset;
		node->value = target->offset;
		node->type = target->type ? type_ptr(target->type) : type_ptr(type_int());
		node->is_pointer = 1;
		node->elem_size = target->elem_size ? target->elem_size : TCC_SIZEOF_PTR;
		if (target->struct_name[0])
			STRNCPY(node->struct_name, target->struct_name, sizeof(node->struct_name) - 1);
		return node;
	}

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
	if (expr && type_is_pointer(expr->type) && type_pointee(expr->type)) {
		Type *pointee_type = expr->type->base;

		node->type = expr->type->base;
		node->is_const_lvalue = type_has_qualifier(node->type, TYPE_QUAL_CONST);
		/* elem_size = size of the dereferenced type, not the pointer itself */
		if (pointee_type->kind == TY_CHAR)
			node->elem_size = 1;
		else if (pointee_type->kind == TY_SHORT)
			node->elem_size = 2;
		else if (pointee_type->kind == TY_PTR) {
			/* Dereferencing T** yields T*.
			 * elem_size = size of the element that the RESULTING pointer points to.
			 * e.g. char** -> deref -> char* has elem_size = 1 (sizeof(char)).
			 * e.g. int**  -> deref -> int*  has elem_size = 4 (sizeof(int)).
			 * Without this, (*expr)++ on char** would advance by 8 (ptr size)
			 * instead of 1 (char size), breaking preprocessor eval functions. */
			if (pointee_type->base) {
				switch (pointee_type->base->kind) {
				case TY_CHAR:  node->elem_size = 1; break;
				case TY_SHORT: node->elem_size = 2; break;
				case TY_PTR:   node->elem_size = TCC_SIZEOF_PTR; break;
				case TY_STRUCT: node->elem_size = type_sizeof(pointee_type->base); break;
				default:
					node->elem_size = type_sizeof(pointee_type->base)
					                ? type_sizeof(pointee_type->base)
					                : 4;
					break;
				}
			} else {
				node->elem_size = TCC_SIZEOF_PTR; /* unknown base; assume pointer-sized */
			}
			/* Mark as pointer so parse_postfix can apply further indexing
			 * ((*pp)[n]) without relying solely on the Type* chain surviving
			 * self-hosted compilation intact. */
			node->is_pointer = 1;
		} else if (type_is_struct(pointee_type))
			node->elem_size = type_sizeof(pointee_type);
		else if (pointee_type->kind == TY_ARRAY)
			node->elem_size = type_sizeof(pointee_type);
		else
			node->elem_size = type_sizeof(pointee_type) ? type_sizeof(pointee_type) : 4;
	} else {
		node->elem_size = expr ? expr->elem_size : 4;
	}
	return node;
}

Node *
new_string_len_width(const char *value, size_t len, int label, int width)
{
	Node *node = new_node(ND_STRING);
	if (width != 2 && width != 4)
		width = 1;
	node->string_value = xmalloc(len + 1);
	memcpy(node->string_value, value ? value : "", len);
	node->string_value[len] = '\0';
	node->string_len = len;
	node->string_width = width;
	node->string_label = label;
	node->is_pointer = 1;
	node->type = type_ptr(width == 1 ? type_char() : (width == 2 ? type_short() : type_int()));
	node->elem_size = width;
	return node;
}

Node *
new_string(const char *value, int label)
{
	return new_string_len_width(value, value ? strlen(value) : 0, label, 1);
}

static Type *
integer_type_for_rank(int rank, int is_unsigned)
{
	switch (rank) {
	case 1:
		return is_unsigned ? type_uchar() : type_char();
	case 2:
		return is_unsigned ? type_ushort() : type_short();
	case 4:
		return is_unsigned ? type_ulong() : type_long();
	case 5:
		return is_unsigned ? type_ullong() : type_llong();
	case 3:
	default:
		return is_unsigned ? type_uint() : type_int();
	}
}

static int
integer_rank(const Type *type)
{
	if (!type)
		return 3;

	switch (type->kind) {
	case TY_CHAR:
		return 1;
	case TY_SHORT:
		return 2;
	case TY_ENUM:
		return 3;
	case TY_INT:
		switch (type_source_kind(type)) {
		case TYPE_SOURCE_LONG:
		case TYPE_SOURCE_ULONG:
			return 4;
		case TYPE_SOURCE_LLONG:
		case TYPE_SOURCE_ULLONG:
			return 5;
		default:
			return type->size >= 8 ? 4 : 3;
		}
	default:
		return 3;
	}
}

static Type *
integer_promotion(Type *type)
{
	if (!type || !type_is_integer(type))
		return type;
	if (type->kind == TY_ENUM)
		return type_int();
	if (type->size < TCC_SIZEOF_INT)
		return type_int();
	return type;
}

static Type *
integer_promotion_for_node(Node *node)
{
	Type *type = node ? node->type : NULL;

	if (!type || !type_is_integer(type))
		return type;
	if (!node || !node->is_bitfield)
		return integer_promotion(type);

	if (type->size <= TCC_SIZEOF_INT) {
		if (!type_is_unsigned(type))
			return type_int();
		if (node->bit_width > 0 && node->bit_width < TCC_SIZEOF_INT * 8)
			return type_int();
		return type_uint();
	}

	return integer_promotion(type);
}

static Type *
usual_arith_conversion(Type *a, Type *b)
{
	if (type_is_floating(a) || type_is_floating(b)) {
		if ((a && a->kind == TY_DOUBLE) || (b && b->kind == TY_DOUBLE)) {
			if ((a && type_source_is(a, TYPE_SOURCE_LONG_DOUBLE)) ||
			    (b && type_source_is(b, TYPE_SOURCE_LONG_DOUBLE)))
				return type_with_source(type_double(), TYPE_SOURCE_LONG_DOUBLE,
				                        "long double");
			return type_double();
		}
		return type_float();
	}

	a = integer_promotion(a);
	b = integer_promotion(b);

	if (!a && !b)
		return type_int();
	if (!a)
		return b;
	if (!b)
		return a;
	if (!type_is_integer(a) || !type_is_integer(b))
		return a;

	{
		int rank_a = integer_rank(a);
		int rank_b = integer_rank(b);
		int unsigned_a = type_is_unsigned(a);
		int unsigned_b = type_is_unsigned(b);

		if (unsigned_a == unsigned_b)
			return integer_type_for_rank(rank_a > rank_b ? rank_a : rank_b, unsigned_a);

		{
			Type *unsigned_type = unsigned_a ? a : b;
			Type *signed_type = unsigned_a ? b : a;
			int unsigned_rank = unsigned_a ? rank_a : rank_b;
			int signed_rank = unsigned_a ? rank_b : rank_a;

			if (unsigned_rank >= signed_rank)
				return integer_type_for_rank(unsigned_rank, 1);
			if (signed_type->size > unsigned_type->size)
				return integer_type_for_rank(signed_rank, 0);
			return integer_type_for_rank(signed_rank, 1);
		}
	}
}

static Type *
usual_arith_component_type(Type *type)
{
	if (!type)
		return NULL;
	if (type->kind == TY_FLOAT)
		return type_float();
	if (type->kind == TY_DOUBLE && type_source_is(type, TYPE_SOURCE_LONG_DOUBLE))
		return type_with_source(type_double(), TYPE_SOURCE_LONG_DOUBLE,
		                        "long double");
	if (type->kind == TY_DOUBLE)
		return type_double();
	return type;
}

static Type *
usual_arith_rewrap_type(Type *real_type, int source_kind)
{
	if (!real_type)
		return NULL;
	if (source_kind == TYPE_SOURCE_COMPLEX) {
		if (real_type->kind == TY_FLOAT)
			return type_with_source(type_float(), TYPE_SOURCE_COMPLEX,
			                        "_Complex float");
		if (real_type->kind == TY_DOUBLE &&
		    type_source_is(real_type, TYPE_SOURCE_LONG_DOUBLE))
			return type_with_source(type_double(), TYPE_SOURCE_COMPLEX,
			                        "_Complex long double");
		return type_with_source(type_double(), TYPE_SOURCE_COMPLEX,
		                        "_Complex double");
	}
	if (source_kind == TYPE_SOURCE_IMAGINARY) {
		if (real_type->kind == TY_FLOAT)
			return type_with_source(type_float(), TYPE_SOURCE_IMAGINARY,
			                        "_Imaginary float");
		if (real_type->kind == TY_DOUBLE &&
		    type_source_is(real_type, TYPE_SOURCE_LONG_DOUBLE))
			return type_with_source(type_double(), TYPE_SOURCE_IMAGINARY,
			                        "_Imaginary long double");
		return type_with_source(type_double(), TYPE_SOURCE_IMAGINARY,
		                        "_Imaginary double");
	}
	return real_type;
}

static Type *
usual_arith_conversion_nodes(Node *left, Node *right)
{
	Type *a = left ? left->type : NULL;
	Type *b = right ? right->type : NULL;
	Type *real_a;
	Type *real_b;
	Type *real_result;
	int result_source_kind;

	if (type_is_complex(a) || type_is_complex(b) ||
	    type_is_imaginary(a) || type_is_imaginary(b)) {
		real_a = usual_arith_component_type(a);
		real_b = usual_arith_component_type(b);
		real_result = usual_arith_conversion(real_a, real_b);
		result_source_kind = (type_is_complex(a) || type_is_complex(b))
		                   ? TYPE_SOURCE_COMPLEX
		                   : TYPE_SOURCE_IMAGINARY;
		return usual_arith_rewrap_type(real_result, result_source_kind);
	}

	if (type_is_floating(a) || type_is_floating(b))
		return usual_arith_conversion(a, b);

	a = integer_promotion_for_node(left);
	b = integer_promotion_for_node(right);
	return usual_arith_conversion(a, b);
}

Node *
new_binary(NodeKind kind, Node *left, Node *right)
{
	Node *node = new_node(kind);
	Type *arith_result = usual_arith_conversion_nodes(left, right);

	if ((kind == ND_ADD || kind == ND_SUB || kind == ND_MUL || kind == ND_DIV) &&
	    arith_result && type_is_floating(arith_result)) {
		if (left && left->type && !type_equal_unqualified(left->type, arith_result))
			left = new_cast(left, arith_result);
		if (right && right->type && !type_equal_unqualified(right->type, arith_result))
			right = new_cast(right, arith_result);
	}

	if ((kind == ND_EQ || kind == ND_NE || kind == ND_LT ||
	     kind == ND_LE || kind == ND_GT || kind == ND_GE) &&
	    arith_result && type_is_floating(arith_result)) {
		if (left && left->type && !type_equal_unqualified(left->type, arith_result))
			left = new_cast(left, arith_result);
		if (right && right->type && !type_equal_unqualified(right->type, arith_result))
			right = new_cast(right, arith_result);
	}

	node->left = left;
	node->right = right;

	if (kind == ND_COMMA) {
		if (right) {
			node->type = right->type;
			node->is_unsigned = right->is_unsigned;
			node->is_pointer = right->is_pointer;
			node->elem_size = right->elem_size ? right->elem_size
			                                   : (right->type ? right->type->size : 4);
		}
		return node;
	}

	if (kind == ND_EQ || kind == ND_NE || kind == ND_LT ||
	        kind == ND_LE || kind == ND_GT || kind == ND_GE ||
	        kind == ND_LOGICAL_AND || kind == ND_LOGICAL_OR) {
		node->type = type_int();
		node->is_unsigned = arith_result && type_is_unsigned(arith_result);
		/* Carry the operand size so codegen uses the right register width.
		 * Pointers need 8-byte comparison (cmp x1,x0 not cmp w1,w0).
		 * Chars/shorts are integer-promoted to int before comparison.
		 * Only use 8 bytes for pointers or 8-byte integer types. */
		{
			int lptr = left && left->is_pointer;
			int rptr = right && right->is_pointer;
			int lsz;
			int rsz;
			if (left && type_is_floating(left->type))
				lsz = left->type->size;
			else if (lptr)
				lsz = TCC_SIZEOF_PTR;
			else if (left && left->elem_size >= TCC_SIZEOF_LONG)
				lsz = TCC_SIZEOF_LONG;
			else
				lsz = TCC_SIZEOF_INT;
			if (right && type_is_floating(right->type))
				rsz = right->type->size;
			else if (rptr)
				rsz = TCC_SIZEOF_PTR;
			else if (right && right->elem_size >= TCC_SIZEOF_LONG)
				rsz = TCC_SIZEOF_LONG;
			else
				rsz = TCC_SIZEOF_INT;
			node->elem_size = lsz > rsz ? lsz : rsz;
		}
	} else if (kind == ND_SHL || kind == ND_SHR) {
		/*
		 * C99 6.5.7#3: shifts perform integer promotions on both
		 * operands, but the result type is the promoted left operand.
		 */
		Type *promoted_left = integer_promotion_for_node(left);
		node->type = promoted_left;
		node->is_unsigned = promoted_left && promoted_left->is_unsigned;
		node->elem_size = promoted_left ? promoted_left->size : 4;
	} else {
		node->type = arith_result;
		node->is_unsigned = arith_result && arith_result->is_unsigned;
		node->elem_size = arith_result ? arith_result->size : 4;
	}

	return node;
}

Node *
new_unary(NodeKind kind, Node *expr)
{
	Node *node = new_node(kind);
	node->left = expr;

	if (!expr)
		return node;

	switch (kind) {
	case ND_NEG:
	case ND_BITNOT:
		if (expr->type && type_is_integer(expr->type)) {
			Type *promoted = integer_promotion_for_node(expr);
			node->type = promoted ? promoted : expr->type;
			node->elem_size = node->type ? node->type->size : expr->elem_size;
			node->is_unsigned = node->type && type_is_unsigned(node->type);
		} else {
			node->type = expr->type;
			node->elem_size = expr->elem_size;
			node->is_unsigned = expr->is_unsigned;
		}
		node->is_pointer = expr->is_pointer;
		break;
	case ND_NOT:
		node->type = type_int();
		node->elem_size = TCC_SIZEOF_INT;
		node->is_unsigned = 0;
		node->is_pointer = 0;
		break;
	default:
		break;
	}

	return node;
}

Node *
new_cast(Node *expr, Type *type)
{
	Node *node = new_node(ND_CAST);

	if (type_source_is_bool_spelling(type) && expr) {
		Node *zero = new_num(0);
		zero->type = type_int();
		zero->elem_size = TCC_SIZEOF_INT;
		node = new_binary(ND_NE, expr, zero);
		node->type = type;
		node->elem_size = type ? type->size : 1;
		node->is_unsigned = 1;
		node->is_pointer = 0;
		return node;
	}

	node->left = expr;
	node->type = type;
	node->is_unsigned = type_is_unsigned(type);
	node->is_pointer = type_is_pointer(type);

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
	Node *rhs = expr;

	if (var && var->type && type_source_is_bool_spelling(var->type) && expr) {
		Node *zero = new_num(0);
		zero->type = type_int();
		zero->elem_size = 4;
		rhs = new_binary(ND_NE, expr, zero);
		rhs->type = type_with_source(type_uchar(), TYPE_SOURCE_BOOL, "_Bool");
		rhs->elem_size = 1;
		rhs->is_unsigned = 1;
	}

	if (var && var->type && rhs && rhs->type &&
	    type_is_scalar(var->type) && type_is_scalar(rhs->type) &&
	    !type_is_pointer(var->type) && !type_is_pointer(rhs->type) &&
	    !type_equal_unqualified(var->type, rhs->type)) {
		rhs = new_cast(rhs, var->type);
	}

	node->left = var;
	node->right = rhs;
	if (var) {
		node->type = var->type ? var->type : node->type;
		node->elem_size = var->elem_size ? var->elem_size
		                                 : (node->type ? type_sizeof(node->type) : node->elem_size);
		node->is_pointer = var->is_pointer || (node->type && type_is_pointer(node->type));
		node->is_unsigned = node->type ? type_is_unsigned(node->type) : 0;
		if (var->struct_name[0])
			STRNCPY(node->struct_name, var->struct_name, sizeof(node->struct_name) - 1);
		else if (node->type &&
		         (type_is_struct(node->type) || type_is_union(node->type)) &&
		         node->type->struct_name[0])
			STRNCPY(node->struct_name, node->type->struct_name, sizeof(node->struct_name) - 1);
	}
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
	        type_is_void(then_expr->type) &&
	        type_is_void(else_expr->type)) {
		node->type = type_void();
		node->elem_size = 0;
		return node;
	}

	if (then_expr && else_expr &&
	        then_expr->type && else_expr->type &&
	        then_expr->type->kind == TY_PTR &&
	        else_expr->type->kind == TY_PTR) {
		Type *merged = type_pointer_conditional_result(then_expr->type, else_expr->type);
		node->type = merged ? merged : then_expr->type;
		node->is_pointer = 1;
		node->elem_size = type_elem_sizeof(node->type);
		return node;
	}

	if (then_expr && else_expr &&
	        then_expr->type && type_is_pointer(then_expr->type) &&
	        node_is_null_pointer_constant(else_expr)) {
		node->type = then_expr->type;
		node->is_pointer = 1;
		node->elem_size = then_expr->elem_size;
		return node;
	}

	if (then_expr && else_expr &&
	        else_expr->type && type_is_pointer(else_expr->type) &&
	        node_is_null_pointer_constant(then_expr)) {
		node->type = else_expr->type;
		node->is_pointer = 1;
		node->elem_size = else_expr->elem_size;
		return node;
	}

	if (then_expr && else_expr &&
	        then_expr->type && else_expr->type &&
	        type_is_struct(then_expr->type) &&
	        type_is_struct(else_expr->type) &&
	        STRCMP(then_expr->type->struct_name, else_expr->type->struct_name) == 0) {
		node->type = then_expr->type;
		node->elem_size = then_expr->type->size;
		STRNCPY(node->struct_name, then_expr->type->struct_name,
		        sizeof(node->struct_name) - 1);
		return node;
	}

	node->type = usual_arith_conversion_nodes(then_expr, else_expr);
	node->is_unsigned = type_is_unsigned(node->type);
	node->elem_size = type_sizeof(node->type);
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


void
node_set_param_names(Node *node, char **param_names, int param_count)
{
	int i;

	if (!node || param_count <= 0)
		return;
	node->param_names = xcalloc((size_t)param_count, sizeof(char *));
	for (i = 0; i < param_count; i++) {
		if (param_names && param_names[i] && param_names[i][0])
			node->param_names[i] = xstrdup(param_names[i]);
	}
}

void
node_set_param_type_ids(Node *node, const int *type_ids, int param_count)
{
	int i;

	if (!node || param_count <= 0)
		return;
	node->param_type_ids = xcalloc((size_t)param_count, sizeof(int));
	for (i = 0; i < param_count; i++)
		node->param_type_ids[i] = type_ids ? type_ids[i] : DBG_TYPE_NONE;
}


void
node_set_param_offsets(Node *node, const int *offsets, int param_count)
{
	int i;

	if (!node || param_count <= 0)
		return;
	node->param_offsets = xcalloc((size_t)param_count, sizeof(int));
	for (i = 0; i < param_count; i++)
		node->param_offsets[i] = offsets ? offsets[i] : 0;
}

void
node_set_param_abi_sizes(Node *node, const int *sizes, int param_count)
{
	int i;

	if (!node || param_count <= 0)
		return;
	node->param_abi_sizes = xcalloc((size_t)param_count, sizeof(int));
	for (i = 0; i < param_count; i++)
		node->param_abi_sizes[i] = sizes ? sizes[i] : 0;
}

void
node_set_param_structs(Node *node, char **struct_names, int param_count)
{
	int i;

	if (!node || param_count <= 0)
		return;
	node->param_struct_names = xcalloc((size_t)param_count, sizeof(char *));
	for (i = 0; i < param_count; i++) {
		if (struct_names && struct_names[i] && struct_names[i][0])
			node->param_struct_names[i] = xstrdup(struct_names[i]);
	}
}

void
node_set_param_pointer_structs(Node *node, char **struct_names,
                               const int *depths, int param_count)
{
	int i;

	if (!node || param_count <= 0)
		return;
	node->param_pointer_struct_names = xcalloc((size_t)param_count, sizeof(char *));
	node->param_pointer_depths = xcalloc((size_t)param_count, sizeof(int));
	for (i = 0; i < param_count; i++) {
		if (struct_names && struct_names[i] && struct_names[i][0])
			node->param_pointer_struct_names[i] = xstrdup(struct_names[i]);
		node->param_pointer_depths[i] = depths ? depths[i] : 0;
	}
}

void
node_set_debug_locals(Node *node, const char **names, const int *offsets,
                      const int *type_ids, const char **struct_names,
                      const char **pointer_struct_names,
                      const int *pointer_depths,
                      const int *array_lens, const int *array_elem_type_ids,
                      const char **array_elem_struct_names, int count)
{
	int i;

	if (!node || count <= 0)
		return;
	node->debug_locals = xcalloc((size_t)count, sizeof(NodeDebugLocal));
	node->debug_local_count = count;
	for (i = 0; i < count; i++) {
		if (names && names[i] && names[i][0])
			node->debug_locals[i].name = xstrdup(names[i]);
		node->debug_locals[i].offset = offsets ? offsets[i] : 0;
		node->debug_locals[i].type_id = type_ids ? type_ids[i] : DBG_TYPE_NONE;
		if (struct_names && struct_names[i] && struct_names[i][0])
			STRNCPY(node->debug_locals[i].struct_name, struct_names[i],
			        sizeof(node->debug_locals[i].struct_name) - 1);
		if (pointer_struct_names && pointer_struct_names[i] && pointer_struct_names[i][0])
			STRNCPY(node->debug_locals[i].pointer_struct_name, pointer_struct_names[i],
			        sizeof(node->debug_locals[i].pointer_struct_name) - 1);
		node->debug_locals[i].pointer_depth = pointer_depths ? pointer_depths[i] : 0;
		node->debug_locals[i].array_len = array_lens ? array_lens[i] : 0;
		node->debug_locals[i].array_elem_type_id = array_elem_type_ids ? array_elem_type_ids[i] : DBG_TYPE_NONE;
		if (array_elem_struct_names && array_elem_struct_names[i] && array_elem_struct_names[i][0])
			STRNCPY(node->debug_locals[i].array_elem_struct_name, array_elem_struct_names[i],
			        sizeof(node->debug_locals[i].array_elem_struct_name) - 1);
	}
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
	node->elem_size = TCC_SIZEOF_PTR;
	node->type = type_ptr(type_func(type_int()));
	return node;
}

Node *
new_asm(const char *text, int is_volatile)
{
	Node *node = new_node(ND_ASM);

	node->asm_is_volatile = is_volatile;
	if (text) {
		node->string_len = strlen(text);
		node->string_value = xstrdup(text);
	}
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
	/* Function/debug metadata arrays currently participate in non-unique
	 * ownership during self-host bootstrap.  Avoid tearing them down here
	 * until that ownership model is made explicit. */
	xfree(node->string_value);
	xfree(node);
}

static int
is_num(Node *node)
{
	return node && node->kind == ND_NUM && !node->is_fp_num;
}

static long
fold_cast_value(long val, int sz, int is_unsigned)
{
	if (sz == 1) {
		long v = val & 0xff;
		if (!is_unsigned && (v & 0x80))
			v = v - 0x100;
		return v;
	}

	if (sz == 2) {
		long v = val & 0xffff;
		if (!is_unsigned && (v & 0x8000))
			v = v - 0x10000;
		return v;
	}

	if (sz == 4) {
		long v = val & 0xffffffff;
		if (!is_unsigned && (v & 0x80000000L))
			v = v - 0x100000000L;
		return v;
	}

	return val;
}

static unsigned long
fold_unsigned_value(long val, int sz)
{
	if (sz == 1)
		return (unsigned long)((unsigned char)val);
	if (sz == 2)
		return (unsigned long)((unsigned short)val);
	if (sz == 4)
		return (unsigned long)((unsigned int)val);
	return (unsigned long)val;
}

static long
fold_unsigned_result(unsigned long val, int sz)
{
	if (sz == 1)
		return (long)((unsigned char)val);
	if (sz == 2)
		return (long)((unsigned short)val);
	if (sz == 4)
		return (long)((unsigned int)val);
	return (long)val;
}

static int
fold_shift_width(Node *node)
{
	Type *left_type;
	Type *promoted;

	if (!node || !node->left)
		return TCC_SIZEOF_INT * 8;
	left_type = node->left->type;
	promoted = integer_promotion(left_type);
	if (promoted && promoted->size > 0)
		return promoted->size * 8;
	if (node->left->elem_size > 0)
		return node->left->elem_size * 8;
	return TCC_SIZEOF_INT * 8;
}

static void
fold_validate_shift_count(Node *node, long count)
{
	int width;

	if (count < 0)
		fatal_cur("Shift count is negative\n");
	width = fold_shift_width(node);
	if (count >= width)
		fatal_cur("Shift count is too large for promoted left operand\n");
}

static Node *
replace_with_num(Node *node, long value)
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
	node->value = (int)value;
	node->long_value = value;
	/* Preserve folded node metadata.  This matters for unsigned constant
	 * comparisons after casts, e.g. (unsigned int)1 > -1. */
	if (type_is_unsigned(node->type))
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
	if (node->kind == ND_CAST && is_num(node->left) &&
	    node->type && !type_is_fp_scalar(node->type)) {
		long value;

		/*
		 * Enums are modeled as int-sized signed integers in this compiler.
		 * Spell that path directly here instead of routing through the
		 * cached unsignedness bit, which a self-hosted compiler build has
		 * previously miscompiled for TY_ENUM casts.
		 */
		if (node->type->kind == TY_ENUM)
			value = (long)(int)node->left->long_value;
		else
			value = fold_cast_value(node->left->long_value,
			                        node->type ? node->type->size : 0,
			                        node->type ? node->type->is_unsigned : 0);
		node->is_unsigned = type_is_unsigned(node->type);
		node->elem_size = node->type ? node->type->size : node->elem_size;
		return replace_with_num(node, value);
	}

	if (node->kind == ND_NEG && is_num(node->left))
		return replace_with_num(node, -node->left->long_value);

	if (node->kind == ND_NOT && is_num(node->left))
		return replace_with_num(node, node->left->long_value == 0);

	if (node->kind == ND_BITNOT && is_num(node->left))
		return replace_with_num(node, ~node->left->long_value);

	if (is_num(node->left) && is_num(node->right)) {
		long a = node->left->long_value;
		long b = node->right->long_value;
		int use_unsigned = node->is_unsigned || type_is_unsigned(node->type);
		unsigned long ua = fold_unsigned_value(a, node->elem_size);
		unsigned long ub = fold_unsigned_value(b, node->elem_size);

		switch (node->kind) {
		case ND_ADD:
			return replace_with_num(node, use_unsigned ?
			                        fold_unsigned_result(ua + ub, node->elem_size) :
			                        a + b);
		case ND_SUB:
			return replace_with_num(node, use_unsigned ?
			                        fold_unsigned_result(ua - ub, node->elem_size) :
			                        a - b);
		case ND_MUL:
			return replace_with_num(node, use_unsigned ?
			                        fold_unsigned_result(ua * ub, node->elem_size) :
			                        a * b);
		case ND_BITAND:
			return replace_with_num(node, use_unsigned ?
			                        fold_unsigned_result(ua & ub, node->elem_size) :
			                        a & b);
		case ND_BITOR:
			return replace_with_num(node, use_unsigned ?
			                        fold_unsigned_result(ua | ub, node->elem_size) :
			                        a | b);
		case ND_BITXOR:
			return replace_with_num(node, use_unsigned ?
			                        fold_unsigned_result(ua ^ ub, node->elem_size) :
			                        a ^ b);
		case ND_SHL:
			fold_validate_shift_count(node, b);
			/* Always shift via unsigned to avoid signed-overflow UB (C11 6.5.7).
			 * The bit pattern is the same for non-negative values; for negative
			 * or overflowing cases the result is implementation-defined anyway. */
			return replace_with_num(node, fold_unsigned_result(ua << ub, node->elem_size));
		case ND_SHR:
			fold_validate_shift_count(node, b);
			return replace_with_num(node, use_unsigned ? (long)(ua >> ub) : (a >> b));
		case ND_DIV:
			if (b != 0)
				return replace_with_num(node, use_unsigned ? (long)(ua / ub) : (a / b));
			fatal_cur("Division by zero in constant expression\n");
		case ND_MOD:
			if (b != 0)
				return replace_with_num(node, use_unsigned ? (long)(ua % ub) : (a % b));
			fatal_cur("Modulo by zero in constant expression\n");
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
	if (node->kind == ND_ADD && is_num(node->left) && node->left->long_value == 0)
		return preserve_replacement_next(node->right, node);
	if (node->kind == ND_SUB && is_num(node->right) && node->right->value == 0)
		return preserve_replacement_next(node->left, node);
	if (node->kind == ND_MUL && is_num(node->right) && node->right->value == 1)
		return preserve_replacement_next(node->left, node);
	if (node->kind == ND_MUL && is_num(node->left) && node->left->long_value == 1)
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

	if (node->kind == ND_IF && node->cond && node->cond->kind == ND_NUM) {
		Node *chosen = node->cond->long_value ? node->then_body : node->else_body;
		Node *discarded = node->cond->long_value ? node->else_body : node->then_body;

		/* Even when the condition is constant, a discarded arm that contains
		 * labels must remain in the tree so gotos can still target it. */
		if (discarded && contains_label(discarded))
			return node;

		if (!chosen)
			chosen = new_block(NULL);
		return preserve_replacement_next(chosen, node);
	}

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
			printf(" Lstr%d \"%s\"", n->string_label, n->string_value ? n->string_value : "");
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
