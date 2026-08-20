#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <stdarg.h>
#include <time.h>
#include "tcc.h"
#include "lexer.h"
#include "parser.h"
#include "parser_internal.h"

extern double strtod(const char *nptr, char **endptr);

/* Type definition tables — structs, functions, typedefs, enum tags/constants. */
typedef struct {
	StructDef   *structs;
	int          struct_count;
	int          struct_cap;
	int          anon_struct_id;
	FuncInfo    *funcs;
	int          func_count;
	int          func_cap;
	void        *typedefs;
	int          typedef_count;
	int          typedef_cap;
	EnumTag     *enum_tags;
	int          enum_tag_count;
	int          enum_tag_cap;
	void        *enum_consts;
	int          enum_const_count;
	int          enum_const_cap;
} ParserTables;

static ParserTables ptab = {0};

ParserFunc pfunc;  /* zero-initialized; pfunc.struct_arg_temp_id etc set in parser_reset() */
int parser_anon_struct_id;
typedef struct ParserFunctionReturnInfo ParserFunctionReturnInfo;

Type *parser_canonicalize_decl_type(Type *type);
static void parser_debug_local_set_type(int offset, Type *type);
static int parser_debug_type_id_for_local_name(const char *name);
static PendingStructParam *pending_struct_params_push(void);
static Local *locals_push(void);
static Node *parse_function(void);
static void parse_generic_global_declaration(void);
void parse_union_definition(void);
int parser_emit_debug;
static int parser_try_parse_local_array_designator(int *first_index, int *last_index);
static Node *make_local_struct_decl_node(const char *var_name, const char *struct_name,
	int *offset_out, int *struct_size_out);
static Node *make_local_struct_lhs_node(const char *var_name, int offset, Type *decl_type,
	const char *struct_name, int struct_size);
static Node *make_struct_return_call_node(const char *func_name, Node *args, FuncInfo *fi);
static FuncInfo *add_func_info(const char *name, int returns_struct,
	const char *struct_name, int struct_size, int returns_pointer,
	int return_elem_size, int return_abi_class, int return_abi_reg_count);
static void parser_free_string_array(char **names, int count);
static void func_info_set_param_struct_names(FuncInfo *fi, char **param_struct_names,
	int param_count);
static void parser_mark_func_variadic(const char *name, int fixed_param_count);
static void parser_describe_function_return(Type *ret_type, ParserFunctionReturnInfo *info);
static int parser_target_is_arm64(void);
static int parser_target_is_x64(void);
static const char *parser_arm64_direct_complex_abi_name(const Type *type);
int parser_classify_aggregate_abi(Type *type, int *reg_count_out);
static int parser_current_typedef_scope_base(void);
static int parser_current_local_scope_base(void);
static int parser_find_typedef_index_optional(const char *name);
static int struct_named_field_count(const StructDef *def);
static void validate_complete_global_object_types(void);
static void consume_toplevel_prototype_comma_tail(Type *ret_type, int is_noreturn);
static StructDef *find_struct_in_current_scope_or_null(const char *name);
static EnumTag *parser_find_visible_enum_tag_or_null(const char *name);
static EnumTag *parser_find_current_scope_enum_tag_or_null(const char *name);
static int try_parse_global_addr_array_compound_literal(Global **pg);
static int try_parse_global_addr_struct_compound_literal(Global **pg);
static int try_parse_global_addr_scalar_compound_literal(Global **pg);
static int parser_type_start_is_aggregate(const Token *tok);
int parser_trace_toplevel_enabled(void);

static void
parser_reject_unsupported_special_type(const Type *type)
{
	(void)type;
}

static void
parser_reject_unsupported_complex_function_type(const Type *type)
{
	if (type && type_is_complex(type) &&
	    preprocess_get_target() != PP_TARGET_ARM64 &&
	    preprocess_get_target() != PP_TARGET_X64) {
		fatal_cur("complex function signatures are not supported on this target yet\n");
	}
	(void)type;
}

int
parser_complex_function_signature_supported(const Type *type)
{
	if (!type_is_complex(type))
		return 1;
	return preprocess_get_target() == PP_TARGET_ARM64 ||
	       preprocess_get_target() == PP_TARGET_X64;
}

static int
parser_ident_eq(const char *a, const char *b)
{
	unsigned char ca;
	unsigned char cb;

	if (a == b)
		return 1;
	if (!a || !b)
		return 0;

	for (;;) {
		ca = (unsigned char)*a++;
		cb = (unsigned char)*b++;
		if (ca != cb)
			return 0;
		if (ca == '\0')
			return 1;
	}
}

static const Token *
parser_parenthesized_pointer_declarator_name_token(const char *context)
{
	const Token *name = lexer_peek();

	while (name->kind == TOK_CONST || name->kind == TOK_VOLATILE ||
	       name->kind == TOK_RESTRICT || name->kind == TOK_ATOMIC) {
		lexer_next();
		name = lexer_peek();
	}

	parser_require_decl_identifier(name, context);
	return name;
}

static void
parser_apply_global_decl_alignment(Global *g, Type *decl_type, int requested_align)
{
	if (requested_align <= 0)
		return;
	parser_validate_decl_alignment(requested_align, decl_type);
	g->align = requested_align;
}

static void
parser_require_constant_file_scope_array_bound(void)
{
	if (lexer_peek_ahead(1)->kind != TOK_RBRACKET &&
	    (lexer_peek_ahead(1)->kind == TOK_STAR ||
	     parser_array_bound_contains_nonconstant_identifier()))
		fatal_cur("file-scope array bound must be an integer constant expression\n");
}

static int
parser_parse_file_scope_parenthesized_array_dims(int dims[MAX_ARRAY_DIMS])
{
	if (lexer_peek()->kind != TOK_LBRACKET)
		return 0;
	parser_require_constant_file_scope_array_bound();
	return parse_array_dimensions(dims, 1, 0);
}

static void
parse_prototype_param_metadata(int *is_variadic, int *fixed_params);

static void
parser_reset_param_copy_state(int reset_ids);

static Type *
parser_finish_file_scope_parenthesized_pointer_object_type(Type *base_type,
                                                           int ptr_dims[MAX_ARRAY_DIMS],
                                                           int ptr_dim_count)
{
	int post_dims[MAX_ARRAY_DIMS] = {0};
	int post_dim_count = 0;
	Type **fp_param_types = NULL;
	int fp_param_count = 0;
	int fp_is_variadic = 0;
	int fp_fixed_params = 0;
	int fp_has_prototype = 0;
	Type *decl_type;

	post_dim_count = parser_parse_file_scope_parenthesized_array_dims(post_dims);

	if (post_dim_count > 0)
		decl_type = type_ptr(build_array_type_from_dims_allow_incomplete(base_type,
		                                                                post_dims,
		                                                                post_dim_count, 1));
	else if (lexer_peek()->kind == TOK_LPAREN)
		decl_type = NULL;
	else
		decl_type = type_ptr(base_type);

	if (lexer_peek()->kind == TOK_LPAREN) {
		parse_prototype_param_list(&fp_param_types, &fp_param_count,
		                          &fp_is_variadic, &fp_fixed_params,
		                          &fp_has_prototype, 1);
		if (post_dim_count == 0 && fp_has_prototype) {
			decl_type = type_ptr(parser_make_function_type(base_type,
			                                               fp_param_types,
			                                               fp_param_count,
			                                               fp_is_variadic,
			                                               fp_fixed_params));
		} else if (post_dim_count == 0) {
			decl_type = type_ptr(type_func(clone_type(base_type)));
		}
	}

	if (ptr_dim_count > 0)
		decl_type = build_array_type_from_dims_allow_incomplete(decl_type, ptr_dims,
		                                                       ptr_dim_count, 1);

	return decl_type;
}

static Type *
parser_parse_generic_function_returning_pointer_target_type(Type *base_type)
{
	Type *ret_type;

	if (lexer_peek()->kind == TOK_LBRACKET) {
		int dims[MAX_ARRAY_DIMS] = {0};
		int dim_count = parse_array_dimensions(dims, 1, 0);
		ret_type = type_ptr(build_array_type_from_dims_allow_incomplete(clone_type(base_type),
		                                                               dims, dim_count, 1));
	} else {
		expect(TOK_LPAREN);
		skip_inline_qualifiers();
		if (lexer_peek()->kind != TOK_RPAREN)
			parse_prototype_param_metadata(NULL, NULL);
		else
			expect(TOK_RPAREN);
		skip_inline_qualifiers();
		ret_type = type_ptr(type_func(clone_type(base_type)));
	}

	return ret_type;
}

static Type *
parser_parse_returned_function_pointer_type(Type *base_type)
{
	Type **ret_param_types = NULL;
	int ret_param_count = 0;
	int ret_is_variadic = 0;
	int ret_fixed_params = 0;
	int ret_has_prototype = 0;

	if (lexer_peek()->kind != TOK_LPAREN)
		return type_ptr(base_type);

	parse_prototype_param_list(&ret_param_types, &ret_param_count,
	                          &ret_is_variadic, &ret_fixed_params,
	                          &ret_has_prototype, 1);
	return type_ptr(ret_has_prototype
	                ? parser_make_function_type(base_type,
	                                            ret_param_types,
	                                            ret_param_count,
	                                            ret_is_variadic,
	                                            ret_fixed_params)
	                : type_func(clone_type(base_type)));
}

typedef struct ParserFunctionReturnInfo {
	int returns_struct;
	int struct_size;
	int returns_pointer;
	int return_elem_size;
	int return_abi_class;
	int return_abi_reg_count;
	char struct_name[64];
} ParserFunctionReturnInfo;


typedef struct ParserDebugLocal {
	char name[64];
	int offset;
	int type_id;
	char struct_name[64];
	char pointer_struct_name[64];
	int pointer_depth;
	int array_len;
	int array_elem_type_id;
	char array_elem_struct_name[64];
} ParserDebugLocal;

/* Per-function local scope: variables, param copies, struct params, stack. */
typedef struct {
	Local             *locals;
	int                local_count;
	int                local_cap;
	int                struct_param_copy_id;
	Node              *param_copy_head;
	PendingStructParam *pending_struct_params;
	int                pending_struct_param_count;
	int                pending_struct_param_cap;
	ParserDebugLocal  *debug_locals;
	int                debug_local_count;
	int                debug_local_cap;
	int                stack_size;
} ParserScope;

static ParserScope pscope;
static int parser_pragma_pack_align = 0;
#define PARSER_PRAGMA_PACK_STACK_MAX 32
static int *parser_pragma_pack_stack_align;
static char **parser_pragma_pack_stack_name;
static int parser_pragma_pack_stack_count = 0;
static int parser_decl_align_request = 0;
static int parser_decl_register_request = 0;
static int parser_pending_decl_noreturn = 0;

/* Translation-unit level: global definitions and string labels. */
typedef struct {
	Global *globals;
	int     global_count;
	int     global_cap;
	int     next_string_label;
	ParserStringLiteral *string_literals;
	int     string_literal_cap;
} ParserUnit;

static ParserUnit punit;

static void
parser_register_string_literal_slot(int label)
{
	int old_cap;
	int new_cap;
	ParserStringLiteral *new_items;

	if (label <= 0)
		return;
	if (label < punit.string_literal_cap)
		return;

	old_cap = punit.string_literal_cap;
	new_cap = old_cap ? old_cap : 16;
	while (new_cap <= label)
		new_cap *= 2;

	new_items = xrealloc(punit.string_literals,
	                     sizeof(*new_items) * (size_t)new_cap);
	memset(new_items + old_cap, 0,
	       sizeof(*new_items) * (size_t)(new_cap - old_cap));
	punit.string_literals = new_items;
	punit.string_literal_cap = new_cap;
}

/* Parse-depth instrumentation. */
typedef struct {
	int depth;
	int max_depth;
} ParserDepth;

static ParserDepth pdepth;
static int *parser_typedef_scope_stack;
static int parser_typedef_scope_depth;
static int parser_typedef_scope_cap;
static int *parser_local_scope_stack;
static int parser_local_scope_depth;
static int parser_local_scope_cap;
static int *parser_tag_scope_stack;
static int parser_tag_scope_depth;
static int parser_tag_scope_cap;
static int *parser_enum_tag_scope_stack;
static int parser_enum_tag_scope_depth;
static int parser_enum_tag_scope_cap;
static int *parser_enum_const_scope_stack;
static int parser_enum_const_scope_depth;
static int parser_enum_const_scope_cap;

typedef struct {
	ParserProfileBucket bucket;
	double start_time;
} ParserProfileFrame;

typedef struct {
	int enabled;
	double bucket_time[PARSER_PROFILE_BUCKET_COUNT];
	unsigned long bucket_count[PARSER_PROFILE_BUCKET_COUNT];
	ParserProfileFrame stack[256];
	int depth;
} ParserProfileState;

static ParserProfileState pprofile;
int parser_profile_enabled_flag = 0;

typedef struct {
	int index;
	unsigned long epoch;
} ParserLookupCache;

#define PARSER_LOCAL_LOOKUP_BUCKETS 256
#define PARSER_GLOBAL_LOOKUP_BUCKETS 256
#define PARSER_FUNC_LOOKUP_BUCKETS 256
static unsigned long parser_local_lookup_epoch = 1;
static unsigned long parser_global_lookup_epoch = 1;
static unsigned long parser_func_lookup_epoch = 1;
static unsigned long parser_struct_lookup_epoch = 1;
static unsigned long parser_typedef_lookup_epoch = 1;
static ParserLookupCache parser_local_latest_cache = { -1, 0 };
static ParserLookupCache parser_local_first_cache = { -1, 0 };
static ParserLookupCache parser_global_cache = { -1, 0 };
static ParserLookupCache parser_func_cache = { -1, 0 };
static ParserLookupCache parser_struct_cache = { -1, 0 };
static ParserLookupCache parser_typedef_cache = { -1, 0 };
static ParserLookupCache parser_local_latest_buckets[PARSER_LOCAL_LOOKUP_BUCKETS];
static ParserLookupCache parser_global_buckets[PARSER_GLOBAL_LOOKUP_BUCKETS];
static ParserLookupCache parser_func_buckets[PARSER_FUNC_LOOKUP_BUCKETS];
static int *parser_global_hash_buckets;
static int parser_global_hash_bucket_count;
static int *parser_global_hash_next;
static int parser_global_hash_next_cap;
static int *parser_func_hash_buckets;
static int parser_func_hash_bucket_count;
static int *parser_func_hash_next;
static int parser_func_hash_next_cap;

typedef struct {
	unsigned long epoch;
	char struct_name[64];
	char field_name[64];
	Field *field;
} ParserFieldLookupCache;

static unsigned long parser_field_lookup_epoch = 1;
static ParserFieldLookupCache parser_field_cache;
static int parser_trace_toplevel_state = -1;

static int
struct_named_field_count(const StructDef *def)
{
	int count = 0;

	if (!def)
		return 0;
	for (int i = 0; i < def->field_count; i++) {
		if (def->fields[i].name[0])
			count++;
	}
	return count;
}

static void
reject_duplicate_aggregate_field(StructDef *def, const char *name)
{
	const char *a;
	const char *b;

	if (!def || !name || !name[0])
		return;
	if (parser_trace_toplevel_enabled()) {
		fprintf(stderr, "tcc parse: dup-check struct=%s name=%s field_count=%d\n",
		        def->name[0] ? def->name : "<anon>", name, def->field_count);
		for (int i = 0; i < def->field_count; i++) {
			fprintf(stderr, "tcc parse: dup-check existing[%d]=%s\n",
			        i,
			        def->fields[i].name[0] ? def->fields[i].name : "<anon>");
		}
	}
	for (int i = 0; i < def->field_count; i++) {
		if (!def->fields[i].name[0])
			continue;

		a = def->fields[i].name;
		b = name;
		while (*a && *b && *a == *b) {
			a++;
			b++;
		}
		if (*a == '\0' && *b == '\0')
			fatal_cur("duplicate member name: %s\n", name);
	}
}

int
parser_trace_toplevel_enabled(void)
{
	const char *env;

	if (parser_trace_toplevel_state >= 0)
		return parser_trace_toplevel_state;
	env = getenv("TCC_TRACE_PARSE_TOPLEVEL");
	parser_trace_toplevel_state = (env && env[0] && env[0] != '0') ? 1 : 0;
	return parser_trace_toplevel_state;
}

static void
parser_trace_toplevel(const char *phase, const Token *tok)
{
	if (!parser_trace_toplevel_enabled() || !tok)
		return;
	fprintf(stderr,
	        "tcc parse: %s kind=%s line=%d text=%s\n",
	        phase ? phase : "<phase>",
	        token_debug_name(tok->kind),
	        tok->line,
	        tok->text ? tok->text : "<null>");
}

static void
parser_bump_epoch(unsigned long *epoch)
{
	(*epoch)++;
	if (!*epoch)
		*epoch = 1;
}

static Local *
parser_last_local_if_matches(const char *name, int offset)
{
	Local *last;

	if (pscope.local_count <= 0)
		return NULL;

	last = &pscope.locals[pscope.local_count - 1];
	if (last->offset == offset && STRCMP(last->name, name) == 0)
		return last;

	return NULL;
}

static void
parser_invalidate_local_lookup_cache(void)
{
	parser_bump_epoch(&parser_local_lookup_epoch);
}

static void
parser_invalidate_global_lookup_cache(void)
{
	parser_bump_epoch(&parser_global_lookup_epoch);
}

static void
parser_global_hash_free(void)
{
	xfree(parser_global_hash_buckets);
	parser_global_hash_buckets = NULL;
	parser_global_hash_bucket_count = 0;
	xfree(parser_global_hash_next);
	parser_global_hash_next = NULL;
	parser_global_hash_next_cap = 0;
}

static void
parser_global_hash_ensure_next_capacity(int cap)
{
	int *new_next;
	int *old_next;
	int old_cap;

	if (cap <= parser_global_hash_next_cap)
		return;

	old_cap = parser_global_hash_next_cap;
	old_next = parser_global_hash_next;
	new_next = xmalloc(sizeof(int) * (size_t)cap);
	for (int i = 0; i < old_cap; i++)
		new_next[i] = old_next[i];
	for (int i = old_cap; i < cap; i++)
		new_next[i] = -1;
	xfree(old_next);
	parser_global_hash_next = new_next;
	parser_global_hash_next_cap = cap;
}

static void
parser_hash_init_buckets(int **buckets, int *bucket_cap, int bucket_count)
{
	*buckets = xcalloc((size_t)bucket_count, sizeof(int));
	*bucket_cap = bucket_count;
	for (int i = 0; i < bucket_count; i++)
		(*buckets)[i] = -1;
}

static void
parser_global_hash_insert_index(int global_index)
{
	unsigned int bucket;

	if (global_index < 0 || global_index >= punit.global_count)
		return;
	if (!punit.globals[global_index].name[0])
		return;

	if (!parser_global_hash_buckets)
		parser_hash_init_buckets(&parser_global_hash_buckets,
		                         &parser_global_hash_bucket_count,
		                         PARSER_GLOBAL_LOOKUP_BUCKETS);
	parser_global_hash_ensure_next_capacity(punit.global_cap);

	bucket = tcc_hash_string(punit.globals[global_index].name) &
	         (unsigned int)(parser_global_hash_bucket_count - 1);
	parser_global_hash_next[global_index] = parser_global_hash_buckets[bucket];
	parser_global_hash_buckets[bucket] = global_index;
}

static void
parser_global_hash_rebuild(void)
{
	int bucket_count = parser_global_hash_bucket_count ?
	                  parser_global_hash_bucket_count :
	                  PARSER_GLOBAL_LOOKUP_BUCKETS;

	while (bucket_count < punit.global_count * 2)
		bucket_count *= 2;

	xfree(parser_global_hash_buckets);
	parser_global_hash_buckets = NULL;
	parser_global_hash_bucket_count = 0;
	parser_hash_init_buckets(&parser_global_hash_buckets,
	                         &parser_global_hash_bucket_count,
	                         bucket_count);
	parser_global_hash_ensure_next_capacity(punit.global_cap);
	for (int i = 0; i < punit.global_count; i++)
		parser_global_hash_insert_index(i);
}

static void
parser_global_hash_note_new_index(int global_index)
{
	if (!parser_global_hash_buckets || global_index < 0)
		return;
	if (punit.global_count * 2 > parser_global_hash_bucket_count) {
		parser_global_hash_rebuild();
		return;
	}
	parser_global_hash_insert_index(global_index);
}

static void
parser_invalidate_func_lookup_cache(void)
{
	parser_bump_epoch(&parser_func_lookup_epoch);
}

static void
parser_func_hash_free(void)
{
	xfree(parser_func_hash_buckets);
	parser_func_hash_buckets = NULL;
	parser_func_hash_bucket_count = 0;
	xfree(parser_func_hash_next);
	parser_func_hash_next = NULL;
	parser_func_hash_next_cap = 0;
}

static void
parser_func_hash_ensure_next_capacity(int cap)
{
	int *new_next;
	int *old_next;
	int old_cap;

	if (cap <= parser_func_hash_next_cap)
		return;

	old_cap = parser_func_hash_next_cap;
	old_next = parser_func_hash_next;
	new_next = xmalloc(sizeof(int) * (size_t)cap);
	for (int i = 0; i < old_cap; i++)
		new_next[i] = old_next[i];
	xfree(old_next);
	parser_func_hash_next = new_next;
	for (int i = old_cap; i < cap; i++)
		parser_func_hash_next[i] = -1;
	parser_func_hash_next_cap = cap;
}

static void
parser_func_hash_insert_index(int func_index)
{
	unsigned int bucket;

	if (func_index < 0 || func_index >= ptab.func_count)
		return;
	if (!ptab.funcs[func_index].name[0])
		return;

	if (!parser_func_hash_buckets)
		parser_hash_init_buckets(&parser_func_hash_buckets,
		                         &parser_func_hash_bucket_count,
		                         PARSER_FUNC_LOOKUP_BUCKETS);
	parser_func_hash_ensure_next_capacity(ptab.func_cap);

	bucket = tcc_hash_string(ptab.funcs[func_index].name) &
	         (unsigned int)(parser_func_hash_bucket_count - 1);
	parser_func_hash_next[func_index] = parser_func_hash_buckets[bucket];
	parser_func_hash_buckets[bucket] = func_index;
}

static void
parser_func_hash_rebuild(void)
{
	int bucket_count = parser_func_hash_bucket_count ?
	                  parser_func_hash_bucket_count :
	                  PARSER_FUNC_LOOKUP_BUCKETS;

	while (bucket_count < ptab.func_count * 2)
		bucket_count *= 2;

	xfree(parser_func_hash_buckets);
	parser_func_hash_buckets = NULL;
	parser_func_hash_bucket_count = 0;
	parser_hash_init_buckets(&parser_func_hash_buckets,
	                         &parser_func_hash_bucket_count,
	                         bucket_count);
	parser_func_hash_ensure_next_capacity(ptab.func_cap);
	for (int i = 0; i < ptab.func_count; i++)
		parser_func_hash_insert_index(i);
}

static void
parser_func_hash_note_new_index(int func_index)
{
	if (!parser_func_hash_buckets || func_index < 0)
		return;
	if (ptab.func_count * 2 > parser_func_hash_bucket_count) {
		parser_func_hash_rebuild();
		return;
	}
	parser_func_hash_insert_index(func_index);
}

static void
parser_invalidate_struct_lookup_cache(void)
{
	parser_bump_epoch(&parser_struct_lookup_epoch);
	parser_bump_epoch(&parser_field_lookup_epoch);
}

static void
parser_invalidate_typedef_lookup_cache(void)
{
	parser_bump_epoch(&parser_typedef_lookup_epoch);
}

static int
parser_find_typedef_in_current_scope(const char *name)
{
	int scope_base = parser_current_typedef_scope_base();
	TypedefName *typedefs = (TypedefName *)ptab.typedefs;
	int match = -1;
	int index;

#define TYPEDEF_NAME_EQ(slot_name, query_name)                                     \
	({                                                                         \
		const char *_a = (slot_name);                                      \
		const char *_b = (query_name);                                     \
		int _same = 1;                                                     \
		if (!_a || !_b) {                                                  \
			_same = 0;                                                 \
		} else {                                                           \
			while (*_a || *_b) {                                       \
				if (*_a != *_b) {                                  \
					_same = 0;                                 \
					break;                                    \
				}                                                    \
				_a++;                                                \
				_b++;                                                \
			}                                                            \
		}                                                                    \
		_same;                                                               \
	})

	for (index = scope_base; index < ptab.typedef_count; index++) {
		const char *existing_name;

		existing_name = typedefs[index].name;
		if (TYPEDEF_NAME_EQ(existing_name, name))
			match = index;
	}

#undef TYPEDEF_NAME_EQ
	return match;
}

static int
parser_find_local_in_current_scope_optional(const char *name)
{
	int scope_base = parser_current_local_scope_base();

	if (!name || !name[0])
		return -1;

	for (int i = pscope.local_count - 1; i >= scope_base; i--) {
		if (parser_ident_eq(pscope.locals[i].name, name))
			return i;
	}

	return -1;
}

void
parser_note_block_scope_function_declaration(const char *name, Type *ret_type)
{
	int existing_index;
	Local *local;

	if (!name || !name[0])
		return;

	existing_index = parser_find_local_in_current_scope_optional(name);
	if (existing_index >= 0) {
		local = &pscope.locals[existing_index];
		if (!local->is_function_decl) {
			fatal_cur("identifier '%s' conflicts with an existing identifier in the same scope\n",
			          name);
		}
		return;
	}

	local = locals_push();
	STRNCPY(local->name, name, sizeof(local->name) - 1);
	local->offset = 0;
	local->is_array = 0;
	local->array_len = 0;
	local->align = 0;
	local->is_pointer = 0;
	local->is_function_decl = 1;
	local->is_vla = 0;
	local->is_vm_type = 0;
	local->elem_size = 0;
	local->is_struct = 0;
	local->struct_name[0] = '\0';
	local->vla_bound_name[0] = '\0';
	local->vla_stack_name[0] = '\0';
	local->vla_stack_offset = 0;
	local->type = ret_type ? type_func(parser_canonicalize_decl_type(ret_type)) : NULL;
	local->vla_elem_type = NULL;
	local->struct_by_ref = 0;
	local->is_static = 0;
	local->is_register = 0;
	local->static_global_name[0] = '\0';
}

static void
parser_reject_scope_typedef_name(const char *name)
{
	if (!name || !name[0])
		return;
	if (parser_find_typedef_in_current_scope(name) >= 0)
		fatal_cur("identifier '%s' conflicts with typedef name in the same scope\n",
		          name);
}

static void
parser_reject_current_scope_ordinary_identifier_for_typedef(const char *name)
{
	if (!name || !name[0])
		return;

	if (parser_find_local_in_current_scope_optional(name) >= 0) {
		fatal_cur("typedef name '%s' conflicts with an existing identifier in the same scope\n",
		          name);
	}

	if (parser_local_scope_depth <= 0 && pscope.local_count <= 0 &&
	    (find_global(name) || find_func(name))) {
		fatal_cur("typedef name '%s' conflicts with an existing identifier in the same scope\n",
		          name);
	}
}

static void
parser_ensure_typedef_capacity(void)
{
	TypedefName *typedefs = (TypedefName *)ptab.typedefs;

	if (ptab.typedef_count < ptab.typedef_cap)
		return;

	{
		TypedefName *new_items;
		int new_cap = ptab.typedef_cap ? ptab.typedef_cap * 2 : 32;

		new_items = xcalloc((size_t)new_cap, sizeof(TypedefName));
		if (typedefs && ptab.typedef_cap > 0) {
			memcpy(new_items, typedefs,
			       sizeof(TypedefName) * (size_t)ptab.typedef_cap);
		}
		xfree(ptab.typedefs);
		ptab.typedefs = new_items;
		ptab.typedef_cap = new_cap;
	}
}

static int
parser_find_local_latest_index_optional(const char *name)
{
	int index;
	unsigned int bucket;

	if (!name || !name[0])
		return -1;

	index = parser_local_latest_cache.index;
	if (parser_local_latest_cache.epoch == parser_local_lookup_epoch &&
	    index >= 0 &&
	    index < pscope.local_count &&
	    parser_ident_eq(pscope.locals[index].name, name))
		return index;

	bucket = tcc_hash_string(name) & (PARSER_LOCAL_LOOKUP_BUCKETS - 1);
	index = parser_local_latest_buckets[bucket].index;
	if (parser_local_latest_buckets[bucket].epoch == parser_local_lookup_epoch &&
	    index >= 0 &&
	    index < pscope.local_count &&
	    parser_ident_eq(pscope.locals[index].name, name)) {
		parser_local_latest_cache.index = index;
		parser_local_latest_cache.epoch = parser_local_lookup_epoch;
		return index;
	}

	for (int i = pscope.local_count - 1; i >= 0; i--) {
		if (parser_ident_eq(pscope.locals[i].name, name)) {
			parser_local_latest_cache.index = i;
			parser_local_latest_cache.epoch = parser_local_lookup_epoch;
			parser_local_latest_buckets[bucket].index = i;
			parser_local_latest_buckets[bucket].epoch = parser_local_lookup_epoch;
			return i;
		}
	}

	return -1;
}

static int
parser_find_local_first_index_optional(const char *name)
{
	int index;

	if (!name || !name[0])
		return -1;

	index = parser_local_first_cache.index;
	if (parser_local_first_cache.epoch == parser_local_lookup_epoch &&
	    index >= 0 &&
	    index < pscope.local_count &&
	    parser_ident_eq(pscope.locals[index].name, name))
		return index;

	for (int i = 0; i < pscope.local_count; i++) {
		if (parser_ident_eq(pscope.locals[i].name, name)) {
			parser_local_first_cache.index = i;
			parser_local_first_cache.epoch = parser_local_lookup_epoch;
			return i;
		}
	}

	return -1;
}

static Local *
parser_find_local_latest_optional(const char *name)
{
	int index = parser_find_local_latest_index_optional(name);
	return index >= 0 ? &pscope.locals[index] : NULL;
}

static Local *
parser_find_local_first_optional(const char *name)
{
	int index = parser_find_local_first_index_optional(name);
	return index >= 0 ? &pscope.locals[index] : NULL;
}

static Local *
parser_require_local_latest(const char *name)
{
	Local *local = parser_find_local_latest_optional(name);

	if (!local)
		fatal_cur("Undefined variable: %s\n", name);
	return local;
}

static int
parser_find_global_index_optional(const char *name)
{
	int index;
	int i;
	unsigned int hash;
	unsigned int cache_bucket;
	unsigned int hash_bucket;

#define GLOBAL_NAME_EQ(slot_name, query_name)                                      \
	({                                                                         \
		const char *_a = (slot_name);                                      \
		const char *_b = (query_name);                                     \
		int _same = 1;                                                     \
		if (!_a || !_b) {                                                  \
			_same = 0;                                                 \
		} else {                                                           \
			while (*_a || *_b) {                                       \
				if (*_a != *_b) {                                  \
					_same = 0;                                 \
					break;                                    \
				}                                                    \
				_a++;                                                \
				_b++;                                                \
			}                                                            \
		}                                                                    \
		_same;                                                               \
	})

	if (!name || !name[0])
		return -1;

	index = parser_global_cache.index;
	if (parser_global_cache.epoch == parser_global_lookup_epoch &&
	    index >= 0 &&
	    index < punit.global_count &&
	    GLOBAL_NAME_EQ(punit.globals[index].name, name))
		return index;

	hash = tcc_hash_string(name);
	cache_bucket = hash & (PARSER_GLOBAL_LOOKUP_BUCKETS - 1);
	index = parser_global_buckets[cache_bucket].index;
	if (parser_global_buckets[cache_bucket].epoch == parser_global_lookup_epoch &&
	    index >= 0 &&
	    index < punit.global_count &&
	    GLOBAL_NAME_EQ(punit.globals[index].name, name)) {
		parser_global_cache.index = index;
		parser_global_cache.epoch = parser_global_lookup_epoch;
		return index;
	}

	if (!parser_global_hash_buckets && punit.global_count > 0)
		parser_global_hash_rebuild();
	if (!parser_global_hash_buckets)
		return -1;

	hash_bucket = hash & (unsigned int)(parser_global_hash_bucket_count - 1);
	for (index = parser_global_hash_buckets[hash_bucket];
	     index >= 0;
	     index = parser_global_hash_next[index]) {
		if (GLOBAL_NAME_EQ(punit.globals[index].name, name)) {
			parser_global_cache.index = index;
			parser_global_cache.epoch = parser_global_lookup_epoch;
			parser_global_buckets[cache_bucket].index = index;
			parser_global_buckets[cache_bucket].epoch = parser_global_lookup_epoch;
			return index;
		}
	}

	for (i = 0; i < punit.global_count; i++) {
		if (GLOBAL_NAME_EQ(punit.globals[i].name, name)) {
			parser_global_cache.index = i;
			parser_global_cache.epoch = parser_global_lookup_epoch;
			parser_global_buckets[cache_bucket].index = i;
			parser_global_buckets[cache_bucket].epoch = parser_global_lookup_epoch;
			if (parser_global_hash_buckets)
				parser_global_hash_rebuild();
			return i;
		}
	}

#undef GLOBAL_NAME_EQ
	return -1;
}

static Global *
parser_find_global_optional(const char *name)
{
	int index = parser_find_global_index_optional(name);
	return index >= 0 ? &punit.globals[index] : NULL;
}

static Global *
parser_require_global(const char *name)
{
	Global *global = parser_find_global_optional(name);

	if (!global) {
		fatal_cur("Undefined global: %s\n", name);
	}
	return global;
}

static int
parser_find_func_index_optional(const char *name)
{
	int index;
	unsigned int hash;
	unsigned int cache_bucket;
	unsigned int hash_bucket;

	if (!name || !name[0])
		return -1;

	index = parser_func_cache.index;
	if (parser_func_cache.epoch == parser_func_lookup_epoch &&
	    index >= 0 &&
	    index < ptab.func_count &&
	    parser_ident_eq(ptab.funcs[index].name, name))
		return index;

	hash = tcc_hash_string(name);
	cache_bucket = hash & (PARSER_FUNC_LOOKUP_BUCKETS - 1);
	index = parser_func_buckets[cache_bucket].index;
	if (parser_func_buckets[cache_bucket].epoch == parser_func_lookup_epoch &&
	    index >= 0 &&
	    index < ptab.func_count &&
	    parser_ident_eq(ptab.funcs[index].name, name)) {
		parser_func_cache.index = index;
		parser_func_cache.epoch = parser_func_lookup_epoch;
		return index;
	}

	if (!parser_func_hash_buckets && ptab.func_count > 0)
		parser_func_hash_rebuild();
	if (!parser_func_hash_buckets)
		return -1;

	hash_bucket = hash & (unsigned int)(parser_func_hash_bucket_count - 1);
	for (index = parser_func_hash_buckets[hash_bucket];
	     index >= 0;
	     index = parser_func_hash_next[index]) {
		if (parser_ident_eq(ptab.funcs[index].name, name)) {
			parser_func_cache.index = index;
			parser_func_cache.epoch = parser_func_lookup_epoch;
			parser_func_buckets[cache_bucket].index = index;
			parser_func_buckets[cache_bucket].epoch = parser_func_lookup_epoch;
			return index;
		}
	}

	return -1;
}

static int
parser_find_struct_index_optional(const char *name)
{
	int index;
	int forward = -1;

	if (!name || !name[0])
		return -1;

	index = parser_struct_cache.index;
	if (parser_struct_cache.epoch == parser_struct_lookup_epoch &&
	    index >= 0 &&
	    index < ptab.struct_count &&
	    parser_ident_eq(ptab.structs[index].name, name))
		return index;

	index = ptab.struct_count;
	while (index > 0) {
		index--;
		if (parser_ident_eq(ptab.structs[index].name, name)) {
			if (ptab.structs[index].field_count > 0) {
				parser_struct_cache.index = index;
				parser_struct_cache.epoch = parser_struct_lookup_epoch;
				return index;
			}
			if (forward < 0)
				forward = index;
		}
	}

	if (forward >= 0) {
		parser_struct_cache.index = forward;
		parser_struct_cache.epoch = parser_struct_lookup_epoch;
	}
	return forward;
}

static int
parser_find_typedef_index_optional(const char *name)
{
	TypedefName *typedefs = (TypedefName *)ptab.typedefs;
	int index;
	int match = -1;

#define TYPEDEF_NAME_EQ(slot_name, query_name)                                     \
	({                                                                         \
		const char *_a = (slot_name);                                      \
		const char *_b = (query_name);                                     \
		int _same = 1;                                                     \
		if (!_a || !_b) {                                                  \
			_same = 0;                                                 \
		} else {                                                           \
			while (*_a || *_b) {                                       \
				if (*_a != *_b) {                                  \
					_same = 0;                                 \
					break;                                    \
				}                                                    \
				_a++;                                                \
				_b++;                                                \
			}                                                            \
		}                                                                    \
		_same;                                                               \
	})

	if (!name || !name[0])
		return -1;

	index = parser_typedef_cache.index;
	if (parser_typedef_cache.epoch == parser_typedef_lookup_epoch &&
	    index >= 0 &&
	    index < ptab.typedef_count &&
	    TYPEDEF_NAME_EQ(typedefs[index].name, name)) {
		if (parser_trace_toplevel_enabled() && STRCMP(name, "LangStandard") == 0) {
			int dump_n = ptab.typedef_count < 8 ? ptab.typedef_count : 8;
			fprintf(stderr,
			        "tcc parse: typedef-lookup cache-hit name=%s index=%d type=%p count=%d\n",
			        name, index, (void *)typedefs[index].type, ptab.typedef_count);
			for (int di = 0; di < dump_n; di++) {
				fprintf(stderr,
				        "tcc parse: typedef-slot[%d] name=%s type=%p\n",
				        di, typedefs[di].name, (void *)typedefs[di].type);
			}
		}
		return index;
	}

	for (index = 0; index < ptab.typedef_count; index++) {
		if (TYPEDEF_NAME_EQ(typedefs[index].name, name)) {
			match = index;
		}
	}

	if (match >= 0) {
		if (parser_trace_toplevel_enabled() && STRCMP(name, "LangStandard") == 0) {
			int dump_n = ptab.typedef_count < 8 ? ptab.typedef_count : 8;
			fprintf(stderr,
			        "tcc parse: typedef-lookup scan-hit name=%s index=%d type=%p count=%d\n",
			        name, match, (void *)typedefs[match].type, ptab.typedef_count);
			for (int di = 0; di < dump_n; di++) {
				fprintf(stderr,
				        "tcc parse: typedef-slot[%d] name=%s type=%p\n",
				        di, typedefs[di].name, (void *)typedefs[di].type);
			}
		}
		parser_typedef_cache.index = match;
		parser_typedef_cache.epoch = parser_typedef_lookup_epoch;
		#undef TYPEDEF_NAME_EQ
		return match;
	}

	if (parser_trace_toplevel_enabled() && STRCMP(name, "LangStandard") == 0) {
		fprintf(stderr,
		        "tcc parse: typedef-lookup miss name=%s count=%d typedefs=%p\n",
		        name, ptab.typedef_count, ptab.typedefs);
	}

#undef TYPEDEF_NAME_EQ
	return -1;
}

static void
parser_profile_reset_internal(void)
{
	int enabled = parser_profile_enabled_flag;

	memset(&pprofile, 0, sizeof(pprofile));
	pprofile.enabled = enabled;
	parser_profile_enabled_flag = enabled;
}

void
parser_profile_scope_enter_slow(ParserProfileBucket bucket)
{
	double now;
	ParserProfileFrame *frame;

	if (!pprofile.enabled)
		return;

	now = tcc_monotonic_seconds();
	if (pprofile.depth > 0) {
		frame = &pprofile.stack[pprofile.depth - 1];
		pprofile.bucket_time[frame->bucket] += now - frame->start_time;
	}
	if (pprofile.depth >= (int)(sizeof(pprofile.stack) / sizeof(pprofile.stack[0])))
		return;

	pprofile.bucket_count[bucket]++;
	pprofile.stack[pprofile.depth].bucket = bucket;
	pprofile.stack[pprofile.depth].start_time = now;
	pprofile.depth++;
}

void
parser_profile_scope_leave_slow(ParserProfileBucket bucket)
{
	double now;
	ParserProfileFrame *frame;

	if (!pprofile.enabled || pprofile.depth <= 0)
		return;

	now = tcc_monotonic_seconds();
	frame = &pprofile.stack[pprofile.depth - 1];
	pprofile.bucket_time[frame->bucket] += now - frame->start_time;
	pprofile.depth--;
	if (pprofile.depth > 0)
		pprofile.stack[pprofile.depth - 1].start_time = now;
	(void)bucket;
}

void
parser_profile_get(ParserProfile *out)
{
	if (!out)
		return;
	memcpy(out, &pprofile, sizeof(*out));
}

const char *
parser_profile_bucket_name(ParserProfileBucket bucket)
{
	switch (bucket) {
	case PARSER_PROF_TOPLEVEL: return "parser-dispatch";
	case PARSER_PROF_FUNCTION: return "parser-function";
	case PARSER_PROF_FUNCTION_HEAD: return "function-head";
	case PARSER_PROF_FUNCTION_PARAMS: return "function-params";
	case PARSER_PROF_FUNCTION_BODY: return "function-body";
	case PARSER_PROF_PARAM_LIST: return "param-list";
	case PARSER_PROF_PARAM_DECL: return "param-decl";
	case PARSER_PROF_PROTOTYPE: return "parser-prototype";
	case PARSER_PROF_GLOBAL_DECL: return "parser-global";
	case PARSER_PROF_TYPE_NAME: return "parser-type";
	case PARSER_PROF_TYPE_BASE: return "type-base";
	case PARSER_PROF_TYPE_RECORD: return "type-record";
	case PARSER_PROF_TYPE_ENUM: return "type-enum";
	case PARSER_PROF_TYPE_TYPEDEF: return "type-typedef";
	case PARSER_PROF_TYPE_POINTER: return "type-pointer";
	case PARSER_PROF_BLOCK: return "parser-block";
	case PARSER_PROF_EXPR: return "parser-expr";
	case PARSER_PROF_EXPR_ASSIGN: return "expr-assign";
	case PARSER_PROF_EXPR_COND: return "expr-cond";
	case PARSER_PROF_EXPR_UNARY: return "expr-unary";
	case PARSER_PROF_EXPR_POSTFIX: return "expr-postfix";
	case PARSER_PROF_EXPR_FACTOR: return "expr-factor";
	case PARSER_PROF_EXPR_IDENT: return "expr-ident";
	case PARSER_PROF_EXPR_IDENT_CALL: return "expr-ident-call";
	case PARSER_PROF_EXPR_IDENT_INDEX: return "expr-ident-index";
	case PARSER_PROF_EXPR_IDENT_DOT: return "expr-ident-dot";
	case PARSER_PROF_EXPR_IDENT_ARROW: return "expr-ident-arrow";
	case PARSER_PROF_EXPR_IDENT_VALUE: return "expr-ident-value";
	case PARSER_PROF_EXPR_PAREN: return "expr-paren";
	case PARSER_PROF_EXPR_INDEX: return "expr-index";
	case PARSER_PROF_EXPR_DOT: return "expr-dot";
	case PARSER_PROF_EXPR_ARROW: return "expr-arrow";
	case PARSER_PROF_EXPR_CALL: return "expr-call";
	case PARSER_PROF_FIND_FIELD: return "find-field";
	case PARSER_PROF_STRUCT_DEF: return "parser-struct";
	case PARSER_PROF_UNION_DEF: return "parser-union";
	case PARSER_PROF_ENUM_SPEC: return "parser-enum";
	default: return "parser-unknown";
	}
}

static int
try_parse_prototype_profiled(void)
{
	int ret;

	if (parser_trace_toplevel_enabled()) {
		const Token *tok = lexer_peek();
		fprintf(stderr, "tcc parse: try-prototype kind=%s line=%d text=%s\n",
		        token_debug_name(tok->kind),
		        tok->line,
		        tok->text ? tok->text : "<null>");
	}

	parser_profile_scope_enter(PARSER_PROF_PROTOTYPE);
	ret = try_parse_prototype();
	parser_profile_scope_leave(PARSER_PROF_PROTOTYPE);
	return ret;
}

static Node *
parse_function_profiled(void)
{
	Node *node;

	if (parser_trace_toplevel_enabled()) {
		const Token *tok = lexer_peek();
		fprintf(stderr, "tcc parse: enter-function kind=%s line=%d text=%s\n",
		        token_debug_name(tok->kind),
		        tok->line,
		        tok->text ? tok->text : "<null>");
	}

	parser_profile_scope_enter(PARSER_PROF_FUNCTION);
	node = parse_function();
	parser_profile_scope_leave(PARSER_PROF_FUNCTION);
	return node;
}

static void
parse_generic_global_declaration_profiled(void)
{
	if (parser_trace_toplevel_enabled()) {
		const Token *tok = lexer_peek();
		fprintf(stderr, "tcc parse: enter-global kind=%s line=%d text=%s\n",
		        token_debug_name(tok->kind),
		        tok->line,
		        tok->text ? tok->text : "<null>");
	}
	parser_profile_scope_enter(PARSER_PROF_GLOBAL_DECL);
	parse_generic_global_declaration();
	parser_profile_scope_leave(PARSER_PROF_GLOBAL_DECL);
}

static void
parse_struct_definition_profiled(void)
{
	parser_profile_scope_enter(PARSER_PROF_STRUCT_DEF);
	parse_struct_definition();
	parser_profile_scope_leave(PARSER_PROF_STRUCT_DEF);
}

static void
parse_union_definition_profiled(void)
{
	parser_profile_scope_enter(PARSER_PROF_UNION_DEF);
	parse_union_definition();
	parser_profile_scope_leave(PARSER_PROF_UNION_DEF);
}

static Type *
parse_enum_specifier_profiled(void)
{
	Type *type;

	parser_profile_scope_enter(PARSER_PROF_ENUM_SPEC);
	type = parse_enum_specifier();
	parser_profile_scope_leave(PARSER_PROF_ENUM_SPEC);
	return type;
}

static Type *
parser_make_function_type_from_canonical(Type *ret_type, Type **param_types, int param_count,
                                         int is_variadic, int fixed_param_count);

static int
parser_debug_type_id(Type *type)
{
	return type_debug_type_id(type);
}

static int
parser_target_is_x86(void)
{
	return preprocess_get_target() == PP_TARGET_X86;
}

static int
parser_target_is_arm64(void)
{
	return preprocess_get_target() == PP_TARGET_ARM64;
}

static int
parser_target_is_x64(void)
{
	return preprocess_get_target() == PP_TARGET_X64;
}

static int
parser_target_abi_size(Type *type)
{
	if (!type)
		return 4;

	if (parser_target_is_x86()) {
		if (type_is_pointer(type) || type_is_function(type) || type_is_array(type))
			return 4;
		if (type_is_struct(type) || type_is_union(type))
			return (type_sizeof(type) + 3) & ~3;
		if (type_source_kind(type) == TYPE_SOURCE_LONG ||
		    type_source_kind(type) == TYPE_SOURCE_ULONG)
			return 4;
	}

	return type_sizeof(type) ? type_sizeof(type) : 4;
}

Node *
parser_make_function_designator(const char *name)
{
	Node *node = new_func_addr(name);
	FuncInfo *fi = name ? find_func(name) : NULL;

	if (fi && fi->return_type) {
		if (fi->has_prototype) {
			node->type = type_ptr(parser_make_function_type_from_canonical(fi->return_type,
			                                                              fi->param_types,
			                                                              fi->param_type_count,
			                                                              fi->is_variadic,
			                                                              fi->fixed_param_count));
		} else {
			node->type = type_ptr(type_func(clone_type(fi->return_type)));
		}
	}

return node;
}

Type *
parser_make_function_type_build(Type *ret_type, Type **param_types, int param_count,
                                int is_variadic, int fixed_param_count,
                                int canonicalize)
{
	Type **param_copies = NULL;
	Type *ret_copy;
	int i;

	if (param_count > 0 && param_types) {
		param_copies = xcalloc((size_t)param_count, sizeof(Type *));
		for (i = 0; i < param_count; i++) {
			if (!param_types[i])
				continue;
			if (canonicalize)
				param_copies[i] = parser_canonicalize_decl_type(param_types[i]);
			else
				param_copies[i] = clone_type(param_types[i]);
		}
	}
	if (canonicalize)
		ret_copy = parser_canonicalize_decl_type(ret_type);
	else
		ret_copy = clone_type(ret_type);

	return type_func_proto(ret_copy, param_copies, param_count,
	                       is_variadic, fixed_param_count);
}

Type *
parser_make_function_type(Type *ret_type, Type **param_types, int param_count,
                          int is_variadic, int fixed_param_count)
{
	return parser_make_function_type_build(ret_type, param_types, param_count,
	                                       is_variadic, fixed_param_count, 1);
}

static Type *
parser_make_function_type_from_canonical(Type *ret_type, Type **param_types, int param_count,
                                         int is_variadic, int fixed_param_count)
{
	return parser_make_function_type_build(ret_type, param_types, param_count,
	                                       is_variadic, fixed_param_count, 0);
}

static Type *
parser_func_info_signature_type(FuncInfo *fi)
{
	if (!fi || !fi->return_type)
		return NULL;
	if (!fi->has_prototype)
		return type_func(clone_type(fi->return_type));
	return parser_make_function_type_from_canonical(fi->return_type, fi->param_types,
	                                                fi->param_type_count, fi->is_variadic,
	                                                fi->fixed_param_count);
}

static void
parser_redecl_type_metadata(const Type *type, Type ***params, int *count,
                            int *is_variadic, int *fixed_param_count,
                            int *has_prototype)
{
	*params = NULL;
	*count = 0;
	*is_variadic = 0;
	*fixed_param_count = 0;
	*has_prototype = 0;
	if (type && type_is_function(type))
		*has_prototype = type_func_metadata(type, params, count, is_variadic,
		                                    fixed_param_count);
}

static int
parser_redecl_param_oldstyle_promotable(const Type *type)
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
parser_redecl_type_compatible_impl(const Type *a, const Type *b,
                                   int ignore_top_level_qualifiers)
{
	Type **a_params;
	Type **b_params;
	int a_count;
	int b_count;
	int a_is_variadic;
	int b_is_variadic;
	int a_fixed;
	int b_fixed;
	int a_has_proto;
	int b_has_proto;
	int i;

	if (a == b)
		return 1;
	if (!a || !b)
		return 0;
	if (!ignore_top_level_qualifiers && a->qualifiers != b->qualifiers)
		return 0;
	if (a->kind != b->kind)
		return 0;

	if (type_is_struct(a) || type_is_union(a) || type_is_enum(a))
		return STRCMP(a->struct_name, b->struct_name) == 0;

	if (type_is_function(a)) {
		parser_redecl_type_metadata(a, &a_params, &a_count, &a_is_variadic,
		                            &a_fixed, &a_has_proto);
		parser_redecl_type_metadata(b, &b_params, &b_count, &b_is_variadic,
		                            &b_fixed, &b_has_proto);

		if (a_has_proto && b_has_proto) {
			if (a_count != b_count ||
			    a_is_variadic != b_is_variadic ||
			    a_fixed != b_fixed)
				return 0;
			if (!parser_redecl_type_compatible_impl(a->base, b->base, 0))
				return 0;
			for (i = 0; i < a_count; i++) {
				if (!parser_redecl_type_compatible_impl(a_params[i], b_params[i], 1))
					return 0;
			}
			return 1;
		}

		if (a_has_proto != b_has_proto) {
			Type **proto_params = a_has_proto ? a_params : b_params;
			int proto_count = a_has_proto ? a_count : b_count;
			int proto_is_variadic = a_has_proto ? a_is_variadic : b_is_variadic;

			if (proto_is_variadic)
				return 0;
			if (!parser_redecl_type_compatible_impl(a->base, b->base, 0))
				return 0;
			for (i = 0; i < proto_count; i++) {
				if (!parser_redecl_param_oldstyle_promotable(proto_params[i]))
					return 0;
			}
			return 1;
		}

		return parser_redecl_type_compatible_impl(a->base, b->base, 0);
	}

	if (type_is_pointer(a))
		return parser_redecl_type_compatible_impl(a->base, b->base, 0);

	if (type_is_array(a)) {
		if (a->array_len != b->array_len &&
		    a->array_len != 0 &&
		    b->array_len != 0)
			return 0;
		return parser_redecl_type_compatible_impl(a->base, b->base, 0);
	}

	if (a->size != b->size ||
	    a->is_unsigned != b->is_unsigned ||
	    a->array_len != b->array_len)
		return 0;

	return 1;
}

static int
parser_redecl_type_compatible(const Type *a, const Type *b)
{
	return parser_redecl_type_compatible_impl(a, b, 0);
}

static int
parser_func_info_signature_compatible(const FuncInfo *fi, Type *ret_type, int has_prototype,
                                      Type **param_types, int param_count,
                                      int is_variadic, int fixed_param_count)
{
	Type **old_params;
	int old_count;
	int old_is_variadic;
	int old_fixed;
	int old_has_proto;
	Type **proto_params;
	int proto_count;
	int proto_is_variadic;

	if (!fi || !fi->return_type)
		return 1;

	old_params = fi->param_types;
	old_count = fi->param_type_count;
	old_is_variadic = fi->is_variadic;
	old_fixed = fi->fixed_param_count;
	old_has_proto = fi->has_prototype;

	if (old_has_proto && has_prototype) {
		if (old_count != param_count ||
		    old_is_variadic != is_variadic ||
		    old_fixed != fixed_param_count)
			return 0;
		if (!parser_redecl_type_compatible_impl(fi->return_type, ret_type, 0))
			return 0;
		for (int i = 0; i < old_count; i++) {
			if (!parser_redecl_type_compatible_impl(old_params[i], param_types[i], 1))
				return 0;
		}
		return 1;
	}

	if (old_has_proto != has_prototype) {
		proto_params = old_has_proto ? old_params : param_types;
		proto_count = old_has_proto ? old_count : param_count;
		proto_is_variadic = old_has_proto ? old_is_variadic : is_variadic;

		if (proto_is_variadic)
			return 0;
		if (!parser_redecl_type_compatible_impl(fi->return_type, ret_type, 0))
			return 0;
		for (int i = 0; i < proto_count; i++) {
			if (!parser_redecl_param_oldstyle_promotable(proto_params[i]))
				return 0;
		}
		return 1;
	}

	return parser_redecl_type_compatible_impl(fi->return_type, ret_type, 0);
}

static void
parser_validate_function_redeclaration(FuncInfo *existing, const char *name,
                                       Type *ret_type, int has_prototype,
                                       Type **param_types, int param_count,
                                       int is_variadic, int fixed_param_count)
{
	if (!existing || !existing->return_type)
		return;

	if (!parser_func_info_signature_compatible(existing, ret_type,
	                                           has_prototype, param_types,
	                                           param_count, is_variadic,
	                                           fixed_param_count))
		fatal_cur("Conflicting declaration for function '%s'\n",
		          name ? name : "");
}

static void
parser_validate_function_linkage_redeclaration(FuncInfo *existing,
                                               const char *name,
                                               int new_is_static,
                                               int allow_gnu_extern_inline)
{
	if (!existing || !new_is_static)
		return;

	if (allow_gnu_extern_inline)
		return;

	if (!existing->is_static)
		fatal_cur("static declaration follows non-static declaration for '%s'\n",
		          name ? name : "");
}

static void
parser_validate_function_definition_redeclaration(FuncInfo *existing,
                                                  const char *name)
{
	if (!existing)
		return;

	if (existing->has_definition)
		fatal_cur("Function already defined: %s\n", name ? name : "");
}

static void
parser_validate_global_object_redeclaration(const Global *existing,
                                            const char *name,
                                            Type *new_type)
{
	if (!existing || !existing->type || !new_type)
		return;

	if (!parser_redecl_type_compatible(existing->type, new_type))
		fatal_cur("Conflicting declaration for global '%s'\n",
		          name ? name : "");
}

static int
parser_redecl_type_prefers_new(const Type *old_type, const Type *new_type)
{
	Type **old_params;
	Type **new_params;
	int old_count;
	int new_count;
	int old_is_variadic;
	int new_is_variadic;
	int old_fixed;
	int new_fixed;
	int old_has_proto;
	int new_has_proto;

	if (old_type == new_type || !old_type || !new_type)
		return 0;
	if (old_type->kind != new_type->kind)
		return 0;

	if (type_is_function(old_type)) {
		parser_redecl_type_metadata(old_type, &old_params, &old_count, &old_is_variadic,
		                            &old_fixed, &old_has_proto);
		parser_redecl_type_metadata(new_type, &new_params, &new_count, &new_is_variadic,
		                            &new_fixed, &new_has_proto);

		if (!old_has_proto && new_has_proto)
			return 1;
		if (old_has_proto && new_has_proto) {
			if (parser_redecl_type_prefers_new(old_type->base, new_type->base))
				return 1;
			for (int i = 0; i < old_count && i < new_count; i++) {
				if (parser_redecl_type_prefers_new(old_params[i], new_params[i]))
					return 1;
			}
		}
		return 0;
	}

	if (type_is_array(old_type)) {
		if (old_type->array_len == 0 && new_type->array_len != 0)
			return 1;
		return parser_redecl_type_prefers_new(old_type->base, new_type->base);
	}

	if (type_is_pointer(old_type))
		return parser_redecl_type_prefers_new(old_type->base, new_type->base);

	return 0;
}

static int
parser_func_info_signature_prefers_new(const FuncInfo *fi, Type *ret_type, int has_prototype,
                                       Type **param_types, int param_count,
                                       int is_variadic, int fixed_param_count)
{
	if (!fi || !fi->return_type)
		return 1;
	if (!fi->has_prototype && has_prototype)
		return 1;
	if (fi->has_prototype && has_prototype) {
		if (fi->is_variadic != is_variadic ||
		    fi->fixed_param_count != fixed_param_count ||
		    fi->param_type_count != param_count)
			return 0;
		if (parser_redecl_type_prefers_new(fi->return_type, ret_type))
			return 1;
		for (int i = 0; i < fi->param_type_count; i++) {
			if (parser_redecl_type_prefers_new(fi->param_types[i], param_types[i]))
				return 1;
		}
	}
	return 0;
}

static void
parser_update_global_object_type_metadata(Global *g, Type *type)
{
	Global meta;

	if (!g || !type)
		return;

	memset(&meta, 0, sizeof(meta));
	apply_type_to_global(&meta, type);

	g->type = meta.type;
	g->is_array = meta.is_array;
	g->array_len = meta.array_len;
	g->elem_size = meta.elem_size;
	g->is_unsigned = meta.is_unsigned;
	g->ptr_elem_size = meta.ptr_elem_size;
	g->is_struct = meta.is_struct;
	STRNCPY(g->struct_name, meta.struct_name, sizeof(g->struct_name) - 1);
}

static void
parser_merge_global_object_redeclaration(Global *existing, Type *new_type)
{
	if (!existing || !existing->type || !new_type)
		return;
	if (!parser_redecl_type_prefers_new(existing->type, new_type))
		return;

	parser_update_global_object_type_metadata(existing, new_type);
}

void
parser_declare_extern_object(const char *name, Type *type)
{
	Global *g;

	if (!name || !name[0] || !type)
		return;

	if (parser_trace_toplevel_enabled() &&
	    (parser_ident_eq(name, "tcc_lang_standard") ||
	     parser_ident_eq(name, "tcc_iso_diagnostics"))) {
		fprintf(stderr,
		        "tcc parse: declare-extern name=%s type=%p global_count=%d\n",
		        name, (void *)type, punit.global_count);
	}

	g = find_global(name);
	if (g) {
		if (parser_trace_toplevel_enabled() &&
		    (parser_ident_eq(name, "tcc_lang_standard") ||
		     parser_ident_eq(name, "tcc_iso_diagnostics"))) {
			fprintf(stderr,
			        "tcc parse: declare-extern existing name=%s g=%p\n",
			        name, (void *)g);
		}
		parser_validate_global_object_redeclaration(g, name, type);
		parser_merge_global_object_redeclaration(g, type);
		return;
	}

	g = new_global_slot(name);
	if (parser_trace_toplevel_enabled() &&
	    (parser_ident_eq(name, "tcc_lang_standard") ||
	     parser_ident_eq(name, "tcc_iso_diagnostics"))) {
		fprintf(stderr,
		        "tcc parse: declare-extern new-slot name=%s g=%p index=%d\n",
		        name, (void *)g, punit.global_count);
	}
	g->is_extern = 1;
	apply_type_to_global(g, clone_type(type));
	parser_commit_reserved_global();
	if (parser_trace_toplevel_enabled() &&
	    (parser_ident_eq(name, "tcc_lang_standard") ||
	     parser_ident_eq(name, "tcc_iso_diagnostics"))) {
		fprintf(stderr,
		        "tcc parse: declare-extern committed name=%s new_global_count=%d\n",
		        name, punit.global_count);
	}
}

static void
func_info_set_param_types(FuncInfo *fi, Type **param_types, int param_count);

static void
func_info_replace_param_types(FuncInfo *fi, Type **param_types, int param_count)
{
	int old_param_count;

	if (!fi)
		return;

	if (parser_trace_toplevel_enabled()) {
		fprintf(stderr,
		        "tcc parse: func-replace-param-types enter fi=%p old_param_types=%p old_count=%d new_param_types=%p new_count=%d\n",
		        (void *)fi, (void *)fi->param_types, fi->param_type_count,
		        (void *)param_types, param_count);
	}

	if (fi->param_types) {
		if (parser_trace_toplevel_enabled()) {
			fprintf(stderr,
			        "tcc parse: func-replace-param-types free-old fi=%p old_param_types=%p\n",
			        (void *)fi, (void *)fi->param_types);
		}
		xfree(fi->param_types);
		fi->param_types = NULL;
	}
	old_param_count = fi->param_type_count;
	if (fi->param_struct_names) {
		parser_free_string_array(fi->param_struct_names, old_param_count);
		fi->param_struct_names = NULL;
	}
	fi->param_type_count = 0;

	if (parser_trace_toplevel_enabled()) {
		fprintf(stderr,
		        "tcc parse: func-replace-param-types after-clear fi=%p param_types=%p count=%d\n",
		        (void *)fi, (void *)fi->param_types, fi->param_type_count);
	}

	if (param_count > 0 && param_types)
		func_info_set_param_types(fi, param_types, param_count);

	if (parser_trace_toplevel_enabled()) {
		fprintf(stderr,
		        "tcc parse: func-replace-param-types done fi=%p param_types=%p count=%d\n",
		        (void *)fi, (void *)fi->param_types, fi->param_type_count);
	}
}

static void
func_info_replace_param_struct_names(FuncInfo *fi, char **param_struct_names, int param_count)
{
	int old_param_count;

	if (!fi)
		return;

	old_param_count = fi->param_type_count;
	if (fi->param_struct_names) {
		parser_free_string_array(fi->param_struct_names, old_param_count);
		fi->param_struct_names = NULL;
	}

	if (param_count > 0 && param_struct_names)
		func_info_set_param_struct_names(fi, param_struct_names, param_count);
}

static void
parser_record_function_signature(FuncInfo *fi, Type *ret_type, int has_prototype,
                                 Type **param_types, int param_count,
                                 int is_variadic, int fixed_param_count,
                                 int is_noreturn)
{
	int prefer_new;

	if (!fi || !ret_type)
		return;

	if (parser_trace_toplevel_enabled()) {
		fprintf(stderr,
		        "tcc parse: record-signature state fi=%p ret_type=%p fi->return_type=%p has_proto=%d param_count=%d variadic=%d fixed=%d noreturn=%d\n",
		        (void *)fi, (void *)ret_type, (void *)fi->return_type,
		        has_prototype, param_count, is_variadic, fixed_param_count,
		        is_noreturn);
	}

	if (is_noreturn)
		fi->is_noreturn = 1;

	if (parser_trace_toplevel_enabled()) {
		fprintf(stderr, "tcc parse: record-signature before-prefer fi=%p\n",
		        (void *)fi);
	}
	prefer_new = parser_func_info_signature_prefers_new(fi, ret_type,
	                                                    has_prototype, param_types,
	                                                    param_count, is_variadic,
	                                                    fixed_param_count);
	if (parser_trace_toplevel_enabled()) {
		fprintf(stderr,
		        "tcc parse: record-signature after-prefer fi=%p prefer_new=%d fi->return_type=%p\n",
		        (void *)fi, prefer_new, (void *)fi->return_type);
	}

	if (!prefer_new)
		return;

	if (parser_trace_toplevel_enabled()) {
		fprintf(stderr, "tcc parse: record-signature before-canon fi=%p ret_type=%p\n",
		        (void *)fi, (void *)ret_type);
	}
	fi->return_type = parser_canonicalize_decl_type(ret_type);
	if (parser_trace_toplevel_enabled()) {
		fprintf(stderr, "tcc parse: record-signature after-canon fi=%p fi->return_type=%p\n",
		        (void *)fi, (void *)fi->return_type);
	}
	fi->has_prototype = has_prototype;
	fi->is_variadic = has_prototype ? is_variadic : 0;
	fi->fixed_param_count = has_prototype ? fixed_param_count : 0;

	if (has_prototype) {
		if (parser_trace_toplevel_enabled()) {
			fprintf(stderr,
			        "tcc parse: record-signature before-param-copy fi=%p param_count=%d param_types=%p\n",
			        (void *)fi, param_count, (void *)param_types);
		}
		func_info_replace_param_types(fi, param_types, param_count);
	}
	if (has_prototype && parser_trace_toplevel_enabled()) {
		fprintf(stderr,
		        "tcc parse: record-signature after-param-copy fi=%p stored_param_count=%d stored_param_types=%p\n",
		        (void *)fi, fi->param_type_count, (void *)fi->param_types);
	}
}

void
parser_declare_function(const char *name, Type *ret_type, int has_prototype,
                        Type **param_types, int param_count, int is_variadic,
                        int fixed_param_count, int is_noreturn)
{
	ParserFunctionReturnInfo ret_info;
	FuncInfo *fi;
	FuncInfo *existing;

	if (!name || !name[0] || !ret_type)
		return;

	parser_reject_scope_typedef_name(name);
	parser_describe_function_return(ret_type, &ret_info);
	existing = find_func(name);
	parser_validate_function_redeclaration(existing, name, ret_type,
	                                       has_prototype, param_types,
	                                       param_count, is_variadic,
	                                       fixed_param_count);
	parser_validate_function_linkage_redeclaration(existing, name,
	                                               pfunc.file_static, 0);

fi = add_func_info(name, ret_info.returns_struct, ret_info.struct_name,
                   ret_info.struct_size, ret_info.returns_pointer,
                   ret_info.return_elem_size, ret_info.return_abi_class,
                   ret_info.return_abi_reg_count);
	if (pfunc.file_static)
		fi->is_static = 1;
	parser_record_function_signature(fi, ret_type, has_prototype, param_types,
	                                 param_count, is_variadic,
	                                 fixed_param_count, is_noreturn);
	if (has_prototype && is_variadic)
		parser_mark_func_variadic(name, fixed_param_count);
}

static FuncInfo *
parser_register_pointer_returning_function_declaration(const char *name,
                                                       Type *ret_type,
                                                       int is_variadic,
                                                       int fixed_param_count)
{
	FuncInfo *fi;

	parser_validate_function_redeclaration(find_func(name), name, ret_type,
	                                       0, NULL, 0,
	                                       is_variadic, fixed_param_count);
	fi = add_func_info(name, 0, "", 0, 1, TCC_SIZEOF_PTR,
	                   AGGREGATE_ABI_NONE, 0);
	parser_record_function_signature(fi, ret_type, 0,
	                                 NULL, 0, 0, 0, 0);
	if (is_variadic)
		parser_mark_func_variadic(name, fixed_param_count);
	return fi;
}

static void
parser_prepare_pointer_returning_function_definition(FuncInfo *fi,
                                                     const char *name,
                                                     Type *ret_type)
{
	parser_validate_function_definition_redeclaration(fi, name);
	fi->has_definition = 1;
	pfunc.returns_struct = 0;
	pfunc.return_size = 0;
	pfunc.return_struct_name[0] = '\0';
	pfunc.returns_pointer = 1;
	pfunc.return_elem_size = TCC_SIZEOF_PTR;
	if (ret_type)
		pfunc.return_type = parser_canonicalize_decl_type(ret_type);
	STRNCPY(pfunc.function_name, name, sizeof(pfunc.function_name) - 1);
	parser_reset_param_copy_state(0);
	expect(TOK_LBRACE);
}

static Type *
parse_nested_function_pointer_object_declarator_type(Type *base_type,
                                                     char name_buf[64])
{
	Type **outer_param_types = NULL;
	int outer_param_count = 0;
	int outer_is_variadic = 0;
	int outer_fixed_params = 0;
	int outer_has_prototype = 0;
	Type **retfp_param_types[8] = {0};
	int retfp_param_count[8] = {0};
	int retfp_is_variadic[8] = {0};
	int retfp_fixed_params[8] = {0};
	int retfp_has_prototype[8] = {0};
	int retfp_level_count = 0;
	Type *ret_type;
	Type *decl_type;
	const Token *name_tok;

	name_buf[0] = '\0';

	expect(TOK_LPAREN);
	expect(TOK_STAR);

	/* Handle double-indirection: void (*(*name)(args))(void)
	 * After consuming '(*', we may see another '(*' before the name. */
	int extra_star = 0;
	if (lexer_peek()->kind == TOK_LPAREN) {
		lexer_next(); /* consume '(' */
		expect(TOK_STAR);
		extra_star = 1;
	}

	name_tok = parser_parenthesized_pointer_declarator_name_token(
	    "function pointer name");
	STRNCPY(name_buf, name_tok->text ? name_tok->text : "", 63);
	lexer_next();

	if (extra_star) {
		/* consume ')' closing '(*name', then the inner param list '(args)' */
		expect(TOK_RPAREN);
		/* inner params become the outer params for the returned function pointer */
		parse_prototype_param_list(&outer_param_types, &outer_param_count,
		                          &outer_is_variadic, &outer_fixed_params,
		                          &outer_has_prototype, 1);
		expect(TOK_RPAREN); /* closing outer '(*(' */
	} else {
		expect(TOK_RPAREN);
		parse_prototype_param_list(&outer_param_types, &outer_param_count,
		                          &outer_is_variadic, &outer_fixed_params,
		                          &outer_has_prototype, 1);
		expect(TOK_RPAREN);
	}

	while (lexer_peek()->kind == TOK_LPAREN) {
		if (retfp_level_count >= 8)
			fatal_cur("Too many nested function-pointer return declarators\n");
		parse_prototype_param_list(&retfp_param_types[retfp_level_count],
		                          &retfp_param_count[retfp_level_count],
		                          &retfp_is_variadic[retfp_level_count],
		                          &retfp_fixed_params[retfp_level_count],
		                          &retfp_has_prototype[retfp_level_count], 1);
		retfp_level_count++;
	}

	ret_type = clone_type(base_type);
	for (int level = retfp_level_count - 1; level >= 0; level--) {
		ret_type = type_ptr(retfp_has_prototype[level]
		                    ? parser_make_function_type(ret_type,
		                                                retfp_param_types[level],
		                                                retfp_param_count[level],
		                                                retfp_is_variadic[level],
		                                                retfp_fixed_params[level])
		                    : type_func(clone_type(ret_type)));
	}

	decl_type = type_ptr(outer_has_prototype
	                     ? parser_make_function_type(ret_type,
	                                                 outer_param_types,
	                                                 outer_param_count,
	                                                 outer_is_variadic,
	                                                 outer_fixed_params)
	                     : type_func(clone_type(ret_type)));
	return decl_type;
}

const char *
parser_resolve_struct_type_name(Type *type)
{
	Type *typedef_type;

	if (!type || (!type_is_struct(type) && !type_is_union(type)))
		return "";
	if (type->struct_name[0])
		return type->struct_name;
	if (!type_source_is_typedef(type) || !type_source_name(type)[0])
		return "";
	typedef_type = parser_find_typedef(type_source_name(type));
	if (typedef_type &&
	    (type_is_struct(typedef_type) || type_is_union(typedef_type)) &&
	    typedef_type->struct_name[0])
		return typedef_type->struct_name;
	return "";
}

static void
parser_zero_grown_tail(void *ptr, int old_cap, int new_cap, size_t elem_size)
{
	char *bytes;
	size_t start;
	size_t count;

	bytes = ptr;
	start = elem_size * (size_t)old_cap;
	count = elem_size * (size_t)(new_cap - old_cap);
	memset(bytes + start, 0, count);
}

static void *
parser_grow_array(void *items, int old_cap, int initial_cap, size_t elem_size, int *cap_out)
{
	int new_cap = old_cap ? old_cap * 2 : initial_cap;
	void *new_items = xmalloc(elem_size * (size_t)new_cap);

	if (parser_trace_toplevel_enabled()) {
		fprintf(stderr,
		        "tcc parse: grow_array items=%p old_cap=%d initial=%d elem=%lu new_cap=%d new_items=%p\n",
		        items, old_cap, initial_cap, (unsigned long)elem_size, new_cap, new_items);
	}

	if (items && old_cap > 0)
		memcpy(new_items, items, elem_size * (size_t)old_cap);
	xfree(items);

	parser_zero_grown_tail(new_items, old_cap, new_cap, elem_size);
	if (parser_trace_toplevel_enabled()) {
		fprintf(stderr,
		        "tcc parse: grow_array zeroed new_items=%p cap=%d\n",
		        new_items, new_cap);
	}
	*cap_out = new_cap;
	return new_items;
}

static void *
parser_push_slot(void **items, int *count, int *cap, int initial_cap, size_t elem_size)
{
	int index;
	void *slot;

	if (*count >= *cap)
		*items = parser_grow_array(*items, *cap, initial_cap, elem_size, cap);

	index = *count;
	*count = index + 1;
	slot = (char *)(*items) + ((size_t)index * elem_size);
	memset(slot, 0, elem_size);
	return slot;
}

static void
parser_set_param_name(char ***names, int *cap, int index, const char *name)
{
	int new_cap;

	if (!names || !cap || index < 0)
		return;
	if (index >= *cap) {
		char **old_names;
		char **new_names;

		new_cap = *cap ? *cap : 8;
		while (index >= new_cap)
			new_cap *= 2;
		old_names = *names;
		new_names = xmalloc(sizeof(char *) * (size_t)new_cap);
		if (old_names && *cap > 0)
			memcpy(new_names, old_names, sizeof(char *) * (size_t)(*cap));
		parser_zero_grown_tail(new_names, *cap, new_cap, sizeof(char *));
		xfree(old_names);
		*names = new_names;
		*cap = new_cap;
	}
	if (name && name[0])
		(*names)[index] = xstrdup(name);
}


static void
parser_set_param_type_id(int **ids, int *cap, int index, int type_id)
{
	int new_cap;

	if (!ids || !cap || index < 0)
		return;
	if (index >= *cap) {
		int *old_ids;
		int *new_ids;

		new_cap = *cap ? *cap : 8;
		while (index >= new_cap)
			new_cap *= 2;
		old_ids = *ids;
		new_ids = xmalloc(sizeof(int) * (size_t)new_cap);
		if (old_ids && *cap > 0)
			memcpy(new_ids, old_ids, sizeof(int) * (size_t)(*cap));
		parser_zero_grown_tail(new_ids, *cap, new_cap, sizeof(int));
		xfree(old_ids);
		*ids = new_ids;
		*cap = new_cap;
	}
	(*ids)[index] = type_id;
}

static void
parser_set_param_offset(int **offsets, int *cap, int index, int offset)
{
	int new_cap;

	if (!offsets || !cap || index < 0)
		return;
	if (index >= *cap) {
		int *old_offsets;
		int *new_offsets;

		new_cap = *cap ? *cap : 8;
		while (index >= new_cap)
			new_cap *= 2;
		old_offsets = *offsets;
		new_offsets = xmalloc(sizeof(int) * (size_t)new_cap);
		if (old_offsets && *cap > 0)
			memcpy(new_offsets, old_offsets, sizeof(int) * (size_t)(*cap));
		parser_zero_grown_tail(new_offsets, *cap, new_cap, sizeof(int));
		xfree(old_offsets);
		*offsets = new_offsets;
		*cap = new_cap;
	}
	(*offsets)[index] = offset;
}

static void
parser_set_param_struct(char ***names, int *cap, int index, const char *struct_name)
{
	int new_cap;

	if (!names || !cap || index < 0)
		return;
	if (index >= *cap) {
		char **old_names;
		char **new_names;

		new_cap = *cap ? *cap : 8;
		while (index >= new_cap)
			new_cap *= 2;
		old_names = *names;
		new_names = xmalloc(sizeof(char *) * (size_t)new_cap);
		if (old_names && *cap > 0)
			memcpy(new_names, old_names, sizeof(char *) * (size_t)(*cap));
		parser_zero_grown_tail(new_names, *cap, new_cap, sizeof(char *));
		xfree(old_names);
		*names = new_names;
		*cap = new_cap;
	}
	if ((*names)[index]) {
		xfree((*names)[index]);
		(*names)[index] = NULL;
	}
	if (struct_name && struct_name[0])
		(*names)[index] = xstrdup(struct_name);
}

static void
parser_set_param_pointer_struct(char ***names, int **depths, int *cap,
                                int index, const char *struct_name, int pointer_depth)
{
	int new_cap;

	if (!names || !depths || !cap || index < 0)
		return;
	if (index >= *cap) {
		char **old_names;
		char **new_names;
		int *old_depths;
		int *new_depths;

		new_cap = *cap ? *cap : 8;
		while (index >= new_cap)
			new_cap *= 2;
		old_names = *names;
		old_depths = *depths;
		new_names = xmalloc(sizeof(char *) * (size_t)new_cap);
		new_depths = xmalloc(sizeof(int) * (size_t)new_cap);
		if (old_names && *cap > 0)
			memcpy(new_names, old_names, sizeof(char *) * (size_t)(*cap));
		if (old_depths && *cap > 0)
			memcpy(new_depths, old_depths, sizeof(int) * (size_t)(*cap));
		parser_zero_grown_tail(new_names, *cap, new_cap, sizeof(char *));
		parser_zero_grown_tail(new_depths, *cap, new_cap, sizeof(int));
		xfree(old_names);
		xfree(old_depths);
		*names = new_names;
		*depths = new_depths;
		*cap = new_cap;
	}
	if ((*names)[index]) {
		xfree((*names)[index]);
		(*names)[index] = NULL;
	}
	if (struct_name && struct_name[0])
		(*names)[index] = xstrdup(struct_name);
	(*depths)[index] = pointer_depth;
}

static void
parser_free_string_array(char **names, int count)
{
	int i;

	if (!names)
		return;
	for (i = 0; i < count; i++)
		xfree(names[i]);
	xfree(names);
}

static int
parser_struct_pointer_info(Type *type, char *struct_name, int struct_name_size)
{
	int depth = 0;
	Type *base = type;

	if (struct_name && struct_name_size > 0)
		struct_name[0] = '\0';
	while (base && type_is_pointer(base)) {
		depth++;
		base = type_pointee(base);
	}
	if (depth > 0 && base && type_is_struct(base) && base->struct_name[0]) {
		if (struct_name && struct_name_size > 0)
			STRNCPY(struct_name, base->struct_name, (size_t)struct_name_size - 1);
		return depth;
	}
	return 0;
}

static void
parser_register_function_parameter(const char *param_name, Type *param_type,
                                   int param_is_register,
                                   int *param_count,
                                   char ***param_names, int *param_name_cap,
                                   int **param_type_ids, int *param_type_cap,
                                   int **param_offsets, int *param_offset_cap,
                                   int **param_abi_sizes, int *param_abi_cap,
                                   char ***param_struct_names, int *param_struct_cap,
                                   char ***param_pointer_struct_names,
                                   int **param_pointer_depths, int *param_pointer_cap)
{
	int param_index;
	int param_local_offset = 0;
	int param_is_ptr;
	int param_is_hfa_value = 0;
	int param_is_x64_complex_fp_value = 0;
	int param_is_x64_complex_fp_pair_value = 0;
	int param_is_struct_value;
	int param_is_struct_ptr = 0;
	int param_base_size;
	char param_struct_name[64] = {0};
	char param_name_buf[64] = {0};

	if (!param_type || !param_count)
		return;

	if (param_name && param_name[0])
		STRNCPY(param_name_buf, param_name, sizeof(param_name_buf) - 1);

	parser_set_decl_register_request(param_is_register);

	param_is_ptr = param_type->kind == TY_PTR;
	param_is_struct_value = type_is_struct(param_type);
	if (parser_target_is_arm64() &&
	    parser_direct_complex_lane_info(param_type, NULL, NULL) &&
	    !type_is_struct(param_type)) {
		const char *abi_name = parser_arm64_direct_complex_abi_name(param_type);

		if (abi_name && abi_name[0]) {
			param_is_hfa_value = 1;
			STRNCPY(param_struct_name, abi_name, sizeof(param_struct_name) - 1);
		}
	} else if (parser_target_is_x64() &&
	           type_is_complex(param_type) &&
	           type_sizeof(param_type) == 8) {
		param_is_x64_complex_fp_value = 1;
		STRNCPY(param_struct_name, "__tcc_x64_complex_float2",
		        sizeof(param_struct_name) - 1);
	} else if (parser_target_is_x64() &&
	           type_is_complex(param_type) &&
	           type_sizeof(param_type) == 16) {
		param_is_x64_complex_fp_pair_value = 1;
		STRNCPY(param_struct_name, "__tcc_x64_complex_double2",
		        sizeof(param_struct_name) - 1);
	}

	if (type_is_struct(param_type)) {
		param_is_struct_ptr = 1;
		const char *sn = param_type->struct_name[0] ? param_type->struct_name
		                 : parser_resolve_struct_type_name(param_type);
		STRNCPY(param_struct_name, sn, sizeof(param_struct_name) - 1);
	} else if (param_type->kind == TY_PTR && param_type->base && type_is_struct(param_type->base)) {
		param_is_struct_ptr = 1;
		const char *sn = param_type->base->struct_name[0] ? param_type->base->struct_name
		                 : parser_resolve_struct_type_name(param_type->base);
		STRNCPY(param_struct_name, sn, sizeof(param_struct_name) - 1);
	}

	param_base_size = type_elem_size(param_type);

	if (param_name_buf[0] == '\0') {
		/* unnamed parameter: accepted for prototype-like syntax */
	} else if (param_is_struct_ptr) {
		if (param_is_ptr) {
			int offset = add_pointer_local(param_name_buf, param_base_size);
			Local *local = parser_last_local_if_matches(param_name_buf, offset);
			param_local_offset = offset;
			if (local) {
				local->type = parser_canonicalize_decl_type(param_type);
				local->is_pointer = 1;
				local->elem_size = param_base_size;
				STRNCPY(local->struct_name, param_struct_name,
				        sizeof(local->struct_name) - 1);
			}
		} else if (parser_target_is_arm64()) {
			AggregateAbiClass abi_class = parser_classify_aggregate_abi(param_type, NULL);

			if (abi_class == AGGREGATE_ABI_INTREGS ||
			    abi_class == AGGREGATE_ABI_HFA) {
			int offset = add_struct_local(param_name_buf, param_struct_name);
			param_local_offset = offset;
			} else if (!parser_target_is_x86()) {
			char hidden_name[64];
			PendingStructParam *psp;
			int hidden_offset;

			snprintf(hidden_name, sizeof(hidden_name), "__paramptr_%d",
			         pscope.struct_param_copy_id++);
			hidden_offset = add_struct_pointer_local(hidden_name, param_struct_name);
			param_local_offset = hidden_offset;

			psp = pending_struct_params_push();
			STRNCPY(psp->param_name,  param_name_buf,   sizeof(psp->param_name) - 1);
			STRNCPY(psp->hidden_name, hidden_name,      sizeof(psp->hidden_name) - 1);
			STRNCPY(psp->struct_name, param_struct_name,sizeof(psp->struct_name) - 1);
			psp->param_index = *param_count;
			} else {
			int offset = add_struct_local(param_name_buf, param_struct_name);
			param_local_offset = offset;
			}
		}
	} else if (param_is_hfa_value) {
		int offset = add_local_sized(param_name_buf, type_sizeof(param_type), 0);
		param_local_offset = offset;
		if (pscope.local_count > 0) {
			pscope.locals[pscope.local_count - 1].type = clone_type(param_type);
			pscope.locals[pscope.local_count - 1].elem_size = type_sizeof(param_type);
			parser_debug_local_set_type(offset, pscope.locals[pscope.local_count - 1].type);
		}
	} else if (param_is_x64_complex_fp_pair_value) {
		int offset = add_local_sized(param_name_buf, type_sizeof(param_type), 0);
		param_local_offset = offset;
		if (pscope.local_count > 0) {
			pscope.locals[pscope.local_count - 1].type = clone_type(param_type);
			pscope.locals[pscope.local_count - 1].elem_size = type_sizeof(param_type);
			parser_debug_local_set_type(offset, pscope.locals[pscope.local_count - 1].type);
		}
	} else if (param_is_ptr) {
		int offset = add_pointer_local(param_name_buf, param_base_size);
		Local *local = parser_last_local_if_matches(param_name_buf, offset);
		param_local_offset = offset;
		if (local) {
			local->type = parser_canonicalize_decl_type(param_type);
			local->is_pointer = 1;
			local->elem_size = param_base_size;
			if (param_type->base && type_is_struct(param_type->base)) {
				STRNCPY(local->struct_name, param_type->base->struct_name,
				        sizeof(local->struct_name) - 1);
			} else {
				local->struct_name[0] = '\0';
			}
		}
	} else {
		int offset = add_local_sized(param_name_buf, 2, 0);
		param_local_offset = offset;
		if (pscope.local_count > 0) {
			pscope.locals[pscope.local_count - 1].type = clone_type(param_type);
			pscope.locals[pscope.local_count - 1].elem_size =
			    param_type->size ? param_type->size : 4;
			parser_debug_local_set_type(offset, pscope.locals[pscope.local_count - 1].type);
		}
	}

	param_index = *param_count;
	parser_set_param_name(param_names, param_name_cap, param_index,
	                      param_name_buf[0] ? param_name_buf : NULL);
	{
		char dbg_param_struct_name[64] = {0};
		int dbg_param_pointer_depth = 0;

		if (!param_is_struct_value && !param_is_hfa_value &&
		    !param_is_x64_complex_fp_pair_value)
			dbg_param_pointer_depth = parser_struct_pointer_info(param_type,
			        dbg_param_struct_name, sizeof(dbg_param_struct_name));
		parser_set_param_pointer_struct(param_pointer_struct_names,
		                                param_pointer_depths,
		                                param_pointer_cap,
		                                param_index,
		                                dbg_param_struct_name,
		                                dbg_param_pointer_depth);
		parser_set_param_offset(param_offsets, param_offset_cap, param_index,
		                        param_local_offset);
		parser_set_param_offset(param_abi_sizes, param_abi_cap, param_index,
		                        parser_target_abi_size(param_type));
		parser_set_param_struct(param_struct_names, param_struct_cap, param_index,
		                        (param_is_struct_value || param_is_hfa_value ||
		                         param_is_x64_complex_fp_value ||
		                         param_is_x64_complex_fp_pair_value)
		                            ? param_struct_name
		                            : NULL);
	}
	{
		int dbg_type_id;

		if (param_is_struct_value || param_is_hfa_value ||
		    param_is_x64_complex_fp_pair_value) {
			dbg_type_id = DBG_TYPE_NONE;
		} else if (!parser_emit_debug) {
			dbg_type_id = parser_debug_type_id(param_type);
		} else {
			dbg_type_id = parser_debug_type_id_for_local_name(param_name_buf);
			if (dbg_type_id == DBG_TYPE_NONE)
				dbg_type_id = parser_debug_type_id(param_type);
		}
		parser_set_param_type_id(param_type_ids, param_type_cap, param_index,
		                         dbg_type_id);
	}
	parser_clear_decl_register_request();

	*param_count = param_index + 1;
}

int
parser_alloc_string_label(void)
{
	ParserUnit *unit = &punit;
	int label = unit->next_string_label;

	if (!label)
		label = 1;
	unit->next_string_label = label + 1;
	return label;
}

void
parser_register_string_literal(int label, const char *value, size_t len, int width)
{
	ParserStringLiteral *lit;

	if (label <= 0)
		return;

	parser_register_string_literal_slot(label);
	lit = &punit.string_literals[label];
	xfree(lit->value);
	lit->value = xmalloc(len + 1);
	memcpy(lit->value, value ? value : "", len);
	lit->value[len] = '\0';
	lit->len = len;
	lit->width = width ? width : 1;
}

int
parser_lookup_string_literal(int label, const char **value_out, size_t *len_out,
                             int *width_out)
{
	ParserStringLiteral *lit;

	if (label <= 0 || label >= punit.string_literal_cap)
		return 0;

	lit = &punit.string_literals[label];
	if (!lit->value)
		return 0;

	if (value_out)
		*value_out = lit->value;
	if (len_out)
		*len_out = lit->len;
	if (width_out)
		*width_out = lit->width ? lit->width : 1;
	return 1;
}

const char *
parser_current_function_name(void)
{
	return pfunc.function_name;
}

int
parser_current_local_count(void)
{
	return pscope.local_count;
}

static void
parser_reset_local_scope_state(void)
{
	pscope.local_count = 0;
	pscope.debug_local_count = 0;
	pscope.stack_size = 0;
	parser_local_scope_depth = 0;
	parser_invalidate_local_lookup_cache();
}

static void
parser_reset_param_copy_state(int reset_ids)
{
	ParserScope *scope = &pscope;

	scope->param_copy_head = NULL;
	scope->pending_struct_param_count = 0;
	if (reset_ids)
		scope->struct_param_copy_id = 1;
}

static int
parser_current_typedef_scope_base(void)
{
	if (parser_typedef_scope_depth <= 0)
		return 0;
	return parser_typedef_scope_stack[parser_typedef_scope_depth - 1];
}

static int
parser_current_local_scope_base(void)
{
	if (parser_local_scope_depth <= 0)
		return 0;
	return parser_local_scope_stack[parser_local_scope_depth - 1];
}

static int
parser_current_tag_scope_base(void)
{
	if (parser_tag_scope_depth <= 0)
		return 0;
	return parser_tag_scope_stack[parser_tag_scope_depth - 1];
}

void
parser_mark_local_scope(ParserScopeMark *mark)
{
	if (!mark)
		return;

	if (parser_typedef_scope_depth >= parser_typedef_scope_cap) {
		int new_cap = parser_typedef_scope_cap ? parser_typedef_scope_cap * 2 : 16;
		int *old_stack = parser_typedef_scope_stack;
		int *new_stack = xmalloc(sizeof(int) * (size_t)new_cap);
		for (int i = 0; i < parser_typedef_scope_cap; i++)
			new_stack[i] = old_stack[i];
		xfree(old_stack);
		parser_typedef_scope_stack = new_stack;
		parser_typedef_scope_cap = new_cap;
	}
	if (parser_local_scope_depth >= parser_local_scope_cap) {
		int new_cap = parser_local_scope_cap ? parser_local_scope_cap * 2 : 16;
		int *old_stack = parser_local_scope_stack;
		int *new_stack = xmalloc(sizeof(int) * (size_t)new_cap);
		for (int i = 0; i < parser_local_scope_cap; i++)
			new_stack[i] = old_stack[i];
		xfree(old_stack);
		parser_local_scope_stack = new_stack;
		parser_local_scope_cap = new_cap;
	}
	if (parser_tag_scope_depth >= parser_tag_scope_cap) {
		int new_cap = parser_tag_scope_cap ? parser_tag_scope_cap * 2 : 16;
		int *old_stack = parser_tag_scope_stack;
		int *new_stack = xmalloc(sizeof(int) * (size_t)new_cap);
		for (int i = 0; i < parser_tag_scope_cap; i++)
			new_stack[i] = old_stack[i];
		xfree(old_stack);
		parser_tag_scope_stack = new_stack;
		parser_tag_scope_cap = new_cap;
	}
	if (parser_enum_tag_scope_depth >= parser_enum_tag_scope_cap) {
		int new_cap = parser_enum_tag_scope_cap ? parser_enum_tag_scope_cap * 2 : 16;
		int *old_stack = parser_enum_tag_scope_stack;
		int *new_stack = xmalloc(sizeof(int) * (size_t)new_cap);
		for (int i = 0; i < parser_enum_tag_scope_cap; i++)
			new_stack[i] = old_stack[i];
		xfree(old_stack);
		parser_enum_tag_scope_stack = new_stack;
		parser_enum_tag_scope_cap = new_cap;
	}
	if (parser_enum_const_scope_depth >= parser_enum_const_scope_cap) {
		int new_cap = parser_enum_const_scope_cap ? parser_enum_const_scope_cap * 2 : 16;
		int *old_stack = parser_enum_const_scope_stack;
		int *new_stack = xmalloc(sizeof(int) * (size_t)new_cap);
		for (int i = 0; i < parser_enum_const_scope_cap; i++)
			new_stack[i] = old_stack[i];
		xfree(old_stack);
		parser_enum_const_scope_stack = new_stack;
		parser_enum_const_scope_cap = new_cap;
	}

	mark->local_count = pscope.local_count;
	mark->typedef_count = ptab.typedef_count;
	mark->struct_count = ptab.struct_count;
	mark->enum_tag_count = ptab.enum_tag_count;
	mark->enum_const_count = ptab.enum_const_count;
	parser_local_scope_stack[parser_local_scope_depth++] = pscope.local_count;
	parser_typedef_scope_stack[parser_typedef_scope_depth++] = ptab.typedef_count;
	parser_tag_scope_stack[parser_tag_scope_depth++] = ptab.struct_count;
	parser_enum_tag_scope_stack[parser_enum_tag_scope_depth++] = ptab.enum_tag_count;
	parser_enum_const_scope_stack[parser_enum_const_scope_depth++] = ptab.enum_const_count;
}

void
parser_restore_local_scope_keep_statics(const ParserScopeMark *mark)
{
	Local *locals = pscope.locals;
	int start;
	int new_count;

	if (!mark)
		return;

	start = mark->local_count;
	new_count = start;

	for (int i = start; i < pscope.local_count; i++) {
		if (locals[i].is_static)
			locals[new_count++] = locals[i];
	}

	pscope.local_count = new_count;
	parser_invalidate_local_lookup_cache();
	parser_invalidate_struct_lookup_cache();
	parser_invalidate_typedef_lookup_cache();
	if (parser_local_scope_depth > 0)
		parser_local_scope_depth--;
	if (parser_typedef_scope_depth > 0) {
		ptab.typedef_count =
		    parser_typedef_scope_stack[parser_typedef_scope_depth - 1];
		parser_typedef_scope_depth--;
	} else {
		ptab.typedef_count = mark->typedef_count;
	}
	if (parser_tag_scope_depth > 0) {
		ptab.struct_count =
		    parser_tag_scope_stack[parser_tag_scope_depth - 1];
		parser_tag_scope_depth--;
	} else {
		ptab.struct_count = mark->struct_count;
	}
	if (parser_enum_tag_scope_depth > 0) {
		ptab.enum_tag_count =
		    parser_enum_tag_scope_stack[parser_enum_tag_scope_depth - 1];
		parser_enum_tag_scope_depth--;
	} else {
		ptab.enum_tag_count = mark->enum_tag_count;
	}
	if (parser_enum_const_scope_depth > 0) {
		ptab.enum_const_count =
		    parser_enum_const_scope_stack[parser_enum_const_scope_depth - 1];
		parser_enum_const_scope_depth--;
	} else {
		ptab.enum_const_count = mark->enum_const_count;
	}
}

#define PARSER_FREE_TABLE(slots, count, cap) \
	do {                                     \
		xfree(slots);                        \
		(slots) = NULL;                      \
		(count) = 0;                         \
		(cap) = 0;                           \
	} while (0)

int
parser_has_vla_since_local_count(int saved_local_count)
{
	int i;

	if (saved_local_count < 0)
		saved_local_count = 0;

	for (i = saved_local_count; i < pscope.local_count; i++) {
		if (pscope.locals[i].is_vm_type && !pscope.locals[i].is_static)
			return 1;
	}

	return 0;
}

int
parser_max_active_vla_local_index(void)
{
	int i;

	for (i = pscope.local_count - 1; i >= 0; i--) {
		if (pscope.locals[i].is_vm_type && !pscope.locals[i].is_static)
			return i;
	}

	return -1;
}

Node *
parser_collect_vla_scope_cleanup(int saved_local_count)
{
	Node *head = NULL;

	for (int i = pscope.local_count - 1; i >= saved_local_count; i--) {
		Node *call;

		if (!pscope.locals[i].is_vla || pscope.locals[i].is_static)
			continue;

		call = parser_make_vla_restore_call(pscope.locals[i].vla_stack_name,
		                                    pscope.locals[i].vla_stack_offset);

		if (!head) {
			head = call;
		} else {
			head = append_node(head, call);
		}
	}

	return head;
}

int
parser_snapshot_active_vlas(VLASnapshotEntry **out_entries)
{
	VLASnapshotEntry *entries;
	int count = 0;
	int i;

	if (!out_entries)
		return 0;

	for (i = 0; i < pscope.local_count; i++) {
		if (!pscope.locals[i].is_vla || pscope.locals[i].is_static)
			continue;
		count++;
	}

	if (count == 0) {
		*out_entries = NULL;
		return 0;
	}

	entries = xcalloc((size_t)count, sizeof(*entries));
	count = 0;

	for (i = 0; i < pscope.local_count; i++) {
		if (!pscope.locals[i].is_vla || pscope.locals[i].is_static)
			continue;

		STRNCPY(entries[count].name, pscope.locals[i].vla_stack_name,
		        sizeof(entries[count].name) - 1);
		entries[count].offset = pscope.locals[i].vla_stack_offset;
		entries[count].elem_size = 8;
		entries[count].local_index = i;
		count++;
	}

	*out_entries = entries;
	return count;
}

Node *
parser_make_vla_restore_call(const char *stack_name, int stack_offset)
{
	Node *arg;
	Node *call;

	arg = new_var(stack_name, stack_offset);
	arg->type = type_ptr(type_char());
	arg->is_pointer = 1;
	arg->elem_size = 1;
	arg->suppress_debug_loc = 1;

	call = new_call("__builtin_stack_restore", arg);
	call->suppress_debug_loc = 1;
	return call;
}

static void
parser_free_all_tables(void)
{
	PARSER_FREE_TABLE(ptab.structs, ptab.struct_count, ptab.struct_cap);
	parser_invalidate_struct_lookup_cache();

	PARSER_FREE_TABLE(ptab.funcs, ptab.func_count, ptab.func_cap);
	parser_func_hash_free();
	parser_invalidate_func_lookup_cache();

	PARSER_FREE_TABLE(ptab.typedefs, ptab.typedef_count, ptab.typedef_cap);
	parser_invalidate_typedef_lookup_cache();

	PARSER_FREE_TABLE(ptab.enum_tags, ptab.enum_tag_count, ptab.enum_tag_cap);

	PARSER_FREE_TABLE(ptab.enum_consts, ptab.enum_const_count,
	                  ptab.enum_const_cap);

	PARSER_FREE_TABLE(pscope.locals, pscope.local_count, pscope.local_cap);
	parser_invalidate_local_lookup_cache();

	pscope.param_copy_head = NULL;

	PARSER_FREE_TABLE(pscope.pending_struct_params,
	                  pscope.pending_struct_param_count,
	                  pscope.pending_struct_param_cap);

	PARSER_FREE_TABLE(pscope.debug_locals, pscope.debug_local_count,
	                  pscope.debug_local_cap);

	PARSER_FREE_TABLE(punit.globals, punit.global_count, punit.global_cap);
	if (punit.string_literals) {
		for (int i = 0; i < punit.string_literal_cap; i++)
			xfree(punit.string_literals[i].value);
		xfree(punit.string_literals);
		punit.string_literals = NULL;
		punit.string_literal_cap = 0;
	}
	parser_global_hash_free();
	parser_invalidate_global_lookup_cache();
}

static void
parser_reset_scalar_state(void)
{
	ParserScope *scope = &pscope;
	int *temp_ids;
	int *depth;

	memset(&pfunc, 0, sizeof(pfunc));
	temp_ids = &pfunc.struct_arg_temp_id;
	temp_ids[0] = 1;
	temp_ids[1] = 1;
	temp_ids[2] = 1;
	ptab.anon_struct_id = 0;
	parser_anon_struct_id = 0;
	parser_local_scope_depth = 0;
	parser_typedef_scope_depth = 0;
	parser_tag_scope_depth = 0;
	parser_enum_tag_scope_depth = 0;
	parser_enum_const_scope_depth = 0;
	scope->struct_param_copy_id = 0;
	scope->stack_size = 0;
	depth = &pdepth.depth;
	depth[0] = 0;
	depth[1] = 0;
}

static void
reject_c89_designated_initializer(void)
{
	if (tcc_lang_is_c89_or_c90())
		fatal_cur("designated initializers are not allowed in C89/C90 mode\n");
}


/* ---------------------------------------------------------------------------
 * Growable table helpers
 * --------------------------------------------------------------------------- */

/* Ensure punit.globals[] has room for one more entry, grow if needed. */
Global *
globals_push(void)
{
	Global *items;
	int index;

	parser_invalidate_global_lookup_cache();
	if (punit.global_count >= punit.global_cap) {
		int old_cap = punit.global_cap;
		int new_cap = old_cap ? old_cap * 2 : 64;
		Global *old_items = punit.globals;
		Global *new_items = xmalloc(sizeof(Global) * (size_t)new_cap);

		if (old_items && old_cap > 0)
			memcpy(new_items, old_items, sizeof(Global) * (size_t)old_cap);
		parser_zero_grown_tail(new_items, old_cap, new_cap, sizeof(Global));
		xfree(old_items);
		punit.globals = new_items;
		punit.global_cap = new_cap;
	}

	index = punit.global_count;
	punit.global_count = index + 1;
	items = punit.globals;
	memset(&items[index], 0, sizeof(Global));
	return &items[index];
}

void
global_init_free(Global *g)
{
	GlobalInit *init;

	if (!g || !g->init)
		return;
	init = g->init;
	xfree(init->values);
	if (init->syms) {
		for (int i = 0; i < init->sym_cap; i++)
			xfree(init->syms[i]);
		xfree(init->syms);
	}
	xfree(init);
	g->init = NULL;
}

static void
global_require_live_ptr(const Global *g, const char *op)
{
	Global *globals = punit.globals;
	int global_count = punit.global_count;
	int global_cap = punit.global_cap;
	const char *op_name = op ? op : "<unknown>";

	if (!g) {
		ICE("parser global error: %s on NULL global pointer", op_name);
	}
	if (!globals || global_cap <= 0) {
		ICE("parser global error: %s on %p with no global table (count=%d cap=%d)",
		    op_name,
		    (const void *)g,
		    global_count,
		    global_cap);
	}
	if (g < globals || g >= globals + global_cap) {
		ICE("parser global error: stale Global* in %s: ptr=%p globals=%p..%p count=%d cap=%d",
		    op_name,
		    (const void *)g,
		    (void *)globals,
		    (void *)(globals + global_cap),
		    global_count,
		    global_cap);
	}
}

static GlobalInit * __attribute__((noinline))
global_get_init_ptr(const Global *g)
{
	GlobalInit *init = NULL;

	global_require_live_ptr(g, "global_get_init_ptr");
	memcpy(&init, ((const char *)g) + offsetof(Global, init), sizeof(init));
	return init;
}

static void __attribute__((noinline))
global_put_init_ptr(Global *g, GlobalInit *init)
{
	global_require_live_ptr(g, "global_put_init_ptr");
	memcpy(((char *)g) + offsetof(Global, init), &init, sizeof(init));
}

/* Ensure g->init exists and g->init->values has room for at least `need` bytes. */
static void
global_init_reserve_bytes(Global *g, int need)
{
	GlobalInit *init;

	global_require_live_ptr(g, "global_init_reserve_bytes");
	init = global_get_init_ptr(g);
	if (!init) {
		init = xcalloc(1, sizeof(GlobalInit));
		global_put_init_ptr(g, init);
	}
	if (need > init->cap) {
		long long *old_values;
		long long *new_values;
		int new_cap = init->cap ? init->cap * 2 : 64;
		while (new_cap < need) new_cap *= 2;
		old_values = init->values;
		new_values = xmalloc(sizeof(long long) * (size_t)new_cap);
		if (old_values && init->cap > 0)
			memcpy(new_values, old_values, sizeof(long long) * (size_t)init->cap);
		parser_zero_grown_tail(new_values, init->cap, new_cap, sizeof(long long));
		xfree(old_values);
		init->values = new_values;
		init->cap = new_cap;
	}
}

/* Ensure g->init->syms has room for the given 8-byte slot index. */
static void
global_init_reserve_sym_slot(Global *g, int slot)
{
	GlobalInit *init;

	global_require_live_ptr(g, "global_init_reserve_sym_slot");
	init = global_get_init_ptr(g);
	if (!init) {
		init = xcalloc(1, sizeof(GlobalInit));
		global_put_init_ptr(g, init);
	}
	if (slot >= init->sym_cap) {
		char **old_syms;
		char **new_syms;
		int new_cap = init->sym_cap ? init->sym_cap * 2 : 16;
		while (new_cap <= slot) new_cap *= 2;
		old_syms = init->syms;
		new_syms = xmalloc(sizeof(char *) * (size_t)new_cap);
		if (old_syms && init->sym_cap > 0)
			memcpy(new_syms, old_syms, sizeof(char *) * (size_t)init->sym_cap);
		parser_zero_grown_tail(new_syms, init->sym_cap, new_cap, sizeof(char *));
		xfree(old_syms);
		init->syms = new_syms;
		init->sym_cap = new_cap;
	}
}

/* Accessor: g->init->count (0 if no init data) */
int
global_init_count(const Global *g)
{
	GlobalInit *init;

	global_require_live_ptr(g, "global_init_count");
	init = g->init;
	return init ? init->count : 0;
}

/* Accessor: set g->init->count, allocating if needed */
void
global_set_init_count(Global *g, int count)
{
	GlobalInit *init;

	global_require_live_ptr(g, "global_set_init_count");
	init = g->init;
	if (count == 0 && !init)
		return;
	global_init_reserve_bytes(g, count ? count : 1);
	init = g->init;
	init->count = count;
}

/* Accessor: read byte from init_values[idx] (0 if not initialised) */
long long
global_init_byte(const Global *g, int idx)
{
	GlobalInit *init;

	global_require_live_ptr(g, "global_init_byte");
	init = g->init;
	if (!init || idx < 0 || idx >= init->count)
		return 0;
	return init->values[idx];
}

/* Accessor: write byte to init_values[idx], growing as needed */
void
global_set_init_byte(Global *g, int idx, long long value)
{
	GlobalInit *init;

	global_require_live_ptr(g, "global_set_init_byte");
	if (idx < 0)
		ICE("global_set_init_byte with negative index %d", idx);
	init = g->init;
	if (!init) {
		init = xcalloc(1, sizeof(GlobalInit));
		g->init = init;
	}
	if (idx >= init->cap) {
		long long *old_values;
		long long *new_values;
		int new_cap = init->cap ? init->cap * 2 : 64;
		while (new_cap <= idx)
			new_cap *= 2;
		old_values = init->values;
		new_values = xmalloc(sizeof(long long) * (size_t)new_cap);
		if (old_values && init->cap > 0)
			memcpy(new_values, old_values, sizeof(long long) * (size_t)init->cap);
		parser_zero_grown_tail(new_values, init->cap, new_cap,
		                       sizeof(long long));
		xfree(old_values);
		init->values = new_values;
		init->cap = new_cap;
	}
	init->values[idx] = value;
}

/* Accessor: read sym name for 8-byte slot (NULL if none) */
const char *
global_init_sym(const Global *g, int slot)
{
	GlobalInit *init;

	global_require_live_ptr(g, "global_init_sym");
	init = g->init;
	if (!init || slot < 0 || slot >= init->sym_count)
		return NULL;
	return init->syms[slot];
}

/* Accessor: set sym name for 8-byte slot */
void
global_set_init_sym(Global *g, int slot, const char *sym)
{
	GlobalInit *init;

	global_require_live_ptr(g, "global_set_init_sym");
	global_init_reserve_sym_slot(g, slot);
	init = g->init;
	xfree(init->syms[slot]);
	init->syms[slot] = sym && sym[0] ? xstrdup(sym) : NULL;
	if (init->sym_count <= slot)
		init->sym_count = slot + 1;
}

int
parser_struct_count(void)
{
	return ptab.struct_count;
}

StructDef *
parser_struct_at(int index)
{
	if (index < 0 || index >= ptab.struct_count)
		return NULL;
	return &ptab.structs[index];
}

int
parser_has_struct_capacity(void)
{
	return 1; /* Dynamic array grows as needed via structs_push() */
}

StructDef *
structs_push(void)
{
	StructDef *items;
	int index;

	parser_invalidate_struct_lookup_cache();
	if (parser_trace_toplevel_enabled()) {
		fprintf(stderr,
		        "tcc parse: structs_push before ptr=%p count=%d cap=%d\n",
		        (void *)ptab.structs, ptab.struct_count, ptab.struct_cap);
	}
	if (ptab.struct_count >= ptab.struct_cap) {
		int old_cap = ptab.struct_cap;
		int new_cap = old_cap ? old_cap * 2 : 16;
		StructDef *old_items = ptab.structs;
		StructDef *new_items = xmalloc(sizeof(StructDef) * (size_t)new_cap);

		if (parser_trace_toplevel_enabled()) {
			fprintf(stderr,
			        "tcc parse: structs_push grow old_ptr=%p old_cap=%d new_cap=%d\n",
			        (void *)old_items, old_cap, new_cap);
		}

		if (old_items && old_cap > 0)
			memcpy(new_items, old_items, sizeof(StructDef) * (size_t)old_cap);
		parser_zero_grown_tail(new_items, old_cap, new_cap, sizeof(StructDef));
		xfree(old_items);
		ptab.structs = new_items;
		ptab.struct_cap = new_cap;
	}

	index = ptab.struct_count;
	ptab.struct_count = index + 1;
	items = ptab.structs;
	memset(&items[index], 0, sizeof(StructDef));
	if (parser_trace_toplevel_enabled()) {
		fprintf(stderr,
		        "tcc parse: structs_push after ptr=%p count=%d cap=%d index=%d\n",
		        (void *)ptab.structs, ptab.struct_count, ptab.struct_cap, index);
	}
	return &items[index];
}

static Field *
struct_field_push(StructDef *def)
{
	if (def->field_count >= def->field_cap)
		def->fields = parser_grow_array(def->fields, def->field_cap, 8,
		                                sizeof(Field), &def->field_cap);
	return &def->fields[def->field_count++];
}

static FuncInfo *
funcs_push(void)
{
	FuncInfo *items;
	int index;

	parser_invalidate_func_lookup_cache();
	if (parser_trace_toplevel_enabled()) {
		fprintf(stderr,
		        "tcc parse: funcs_push before ptr=%p count=%d cap=%d\n",
		        (void *)ptab.funcs, ptab.func_count, ptab.func_cap);
	}
	if (ptab.func_count >= ptab.func_cap) {
		int old_cap = ptab.func_cap;
		int new_cap = old_cap ? old_cap * 2 : 256;
		FuncInfo *old_items = ptab.funcs;
		FuncInfo *new_items = xmalloc(sizeof(FuncInfo) * (size_t)new_cap);

		if (old_items && old_cap > 0)
			memcpy(new_items, old_items, sizeof(FuncInfo) * (size_t)old_cap);
		parser_zero_grown_tail(new_items, old_cap, new_cap, sizeof(FuncInfo));
		xfree(old_items);
		ptab.funcs = new_items;
		ptab.func_cap = new_cap;
		if (parser_trace_toplevel_enabled()) {
			fprintf(stderr,
			        "tcc parse: funcs_push grow old_ptr=%p old_cap=%d new_ptr=%p new_cap=%d\n",
			        (void *)old_items, old_cap, (void *)new_items, new_cap);
		}
	}

	index = ptab.func_count;
	ptab.func_count = index + 1;
	items = ptab.funcs;
	memset(&items[index], 0, sizeof(FuncInfo));
	if (parser_trace_toplevel_enabled()) {
		fprintf(stderr,
		        "tcc parse: funcs_push after ptr=%p count=%d cap=%d index=%d\n",
		        (void *)ptab.funcs, ptab.func_count, ptab.func_cap, index);
	}
	return &items[index];
}

static Local *
locals_push(void)
{
	Local *items;
	int index;

	parser_invalidate_local_lookup_cache();
	if (parser_trace_toplevel_enabled()) {
		fprintf(stderr,
		        "tcc parse: locals_push before ptr=%p count=%d cap=%d\n",
		        (void *)pscope.locals, pscope.local_count, pscope.local_cap);
	}
	if (pscope.local_count >= pscope.local_cap) {
		int old_cap = pscope.local_cap;
		int new_cap = old_cap ? old_cap * 2 : 32;
		Local *old_items = pscope.locals;
		Local *new_items = xmalloc(sizeof(Local) * (size_t)new_cap);

		if (old_items && old_cap > 0)
			memcpy(new_items, old_items, sizeof(Local) * (size_t)old_cap);
		parser_zero_grown_tail(new_items, old_cap, new_cap, sizeof(Local));
		xfree(old_items);
		pscope.locals = new_items;
		pscope.local_cap = new_cap;
		if (parser_trace_toplevel_enabled()) {
			fprintf(stderr,
			        "tcc parse: locals_push grow old_ptr=%p old_cap=%d new_ptr=%p new_cap=%d\n",
			        (void *)old_items, old_cap, (void *)new_items, new_cap);
		}
	}

	index = pscope.local_count;
	pscope.local_count = index + 1;
	items = pscope.locals;
	memset(&items[index], 0, sizeof(Local));
	if (parser_trace_toplevel_enabled()) {
		fprintf(stderr,
		        "tcc parse: locals_push after ptr=%p count=%d cap=%d index=%d\n",
		        (void *)pscope.locals, pscope.local_count, pscope.local_cap, index);
	}
	return &items[index];
}

static void
parser_debug_local_add(const char *name, int offset)
{
	ParserDebugLocal *dl;

	if (!parser_emit_debug)
		return;
	if (!name || !name[0])
		return;
	if (name[0] == '_' && name[1] == '_')
		return;
	if (pscope.debug_local_count >= pscope.debug_local_cap)
		pscope.debug_locals = parser_grow_array(pscope.debug_locals,
		                                       pscope.debug_local_cap, 32,
		                                       sizeof(ParserDebugLocal),
		                                       &pscope.debug_local_cap);
	dl = &pscope.debug_locals[pscope.debug_local_count++];
	STRNCPY(dl->name, name, sizeof(dl->name) - 1);
	dl->offset = offset;
	dl->type_id = DBG_TYPE_INT;
	dl->pointer_depth = 0;
	dl->array_len = 0;
	dl->array_elem_type_id = DBG_TYPE_NONE;
	dl->array_elem_struct_name[0] = '\0';
}

static void
parser_debug_local_set_type(int offset, Type *type)
{
	int type_id = parser_debug_type_id(type);
	ParserDebugLocal *dl;

	if (!parser_emit_debug)
		return;
	for (int i = pscope.debug_local_count - 1; i >= 0; i--) {
		dl = &pscope.debug_locals[i];
		if (dl->offset == offset) {
			Type *ptr_base;
			const char *struct_type_name;

			dl->type_id = type_id;
			dl->struct_name[0] = '\0';
			dl->pointer_struct_name[0] = '\0';
			dl->pointer_depth = 0;
			dl->array_len = 0;
			dl->array_elem_type_id = DBG_TYPE_NONE;
			dl->array_elem_struct_name[0] = '\0';
			struct_type_name = parser_resolve_struct_type_name(type);
			if (struct_type_name[0])
				STRNCPY(dl->struct_name, struct_type_name,
				        sizeof(dl->struct_name) - 1);
			if (type && type_is_pointer(type)) {
				ptr_base = type;
				int pointer_depth = 0;
				while (ptr_base && type_is_pointer(ptr_base)) {
					pointer_depth++;
					ptr_base = type_pointee(ptr_base);
				}
				struct_type_name = parser_resolve_struct_type_name(ptr_base);
				if (struct_type_name[0]) {
					dl->type_id = DBG_TYPE_NONE;
					dl->pointer_depth = pointer_depth;
					STRNCPY(dl->pointer_struct_name, struct_type_name,
					        sizeof(dl->pointer_struct_name) - 1);
				}
			}
			if (type && type_is_array(type) && type_array_len(type) > 0) {
				ptr_base = type_pointee(type);
				dl->type_id = DBG_TYPE_NONE;
				dl->array_len = type_array_len(type);
				dl->array_elem_type_id = parser_debug_type_id(ptr_base);
				struct_type_name = parser_resolve_struct_type_name(ptr_base);
				if (struct_type_name[0]) {
					dl->array_elem_type_id = DBG_TYPE_NONE;
					STRNCPY(dl->array_elem_struct_name, struct_type_name,
					        sizeof(dl->array_elem_struct_name) - 1);
				} else if (dl->array_elem_type_id == DBG_TYPE_NONE)
					dl->array_elem_type_id = DBG_TYPE_INT;
			}
			return;
		}
	}
}


static int
parser_debug_type_id_for_local_name(const char *name)
{
	if (!parser_emit_debug)
		return DBG_TYPE_NONE;
	if (!name || !name[0])
		return DBG_TYPE_NONE;
	for (int i = pscope.local_count - 1; i >= 0; i--) {
		if (STRCMP(pscope.locals[i].name, name) == 0)
			return parser_debug_type_id(pscope.locals[i].type);
	}
	return DBG_TYPE_NONE;
}

static int
parser_debug_name_is_param(const char *name, char **param_names, int param_count)
{
	int i;

	if (!name || !name[0])
		return 1;
	for (i = 0; i < param_count; i++) {
		if (param_names && param_names[i] && STRCMP(param_names[i], name) == 0)
			return 1;
	}
	return 0;
}

static void
parser_attach_debug_locals(Node *func, char **param_names, int param_count)
{
	int count = 0;
	int i;

	if (!parser_emit_debug)
		return;
	if (!func || pscope.debug_local_count <= 0)
		return;
	for (i = 0; i < pscope.debug_local_count; i++) {
		if (parser_debug_name_is_param(pscope.debug_locals[i].name, param_names, param_count))
			continue;
		count++;
	}
	if (count <= 0)
		return;
	func->debug_locals = xcalloc((size_t)count, sizeof(NodeDebugLocal));
	func->debug_local_count = count;
	count = 0;
	for (i = 0; i < pscope.debug_local_count; i++) {
		ParserDebugLocal *dl = &pscope.debug_locals[i];
		NodeDebugLocal *out;
		if (parser_debug_name_is_param(dl->name, param_names, param_count))
			continue;
		out = &func->debug_locals[count++];
		out->name = xstrdup(dl->name);
		out->offset = dl->offset;
		out->type_id = dl->type_id;
		STRNCPY(out->struct_name, dl->struct_name, 63);
		STRNCPY(out->pointer_struct_name, dl->pointer_struct_name, 63);
		out->pointer_depth = dl->pointer_depth;
		out->array_len = dl->array_len;
		out->array_elem_type_id = dl->array_elem_type_id;
		STRNCPY(out->array_elem_struct_name, dl->array_elem_struct_name, 63);
	}
}

static void
parser_mark_synthetic_debug_loc(Node *node)
{
	if (!parser_emit_debug)
		return;
	for (Node *n = node; n; n = n->next) {
		n->suppress_debug_loc = 1;

		if (n->left)
			parser_mark_synthetic_debug_loc(n->left);
		if (n->right)
			parser_mark_synthetic_debug_loc(n->right);
		if (n->init)
			parser_mark_synthetic_debug_loc(n->init);
		if (n->cond)
			parser_mark_synthetic_debug_loc(n->cond);
		if (n->inc)
			parser_mark_synthetic_debug_loc(n->inc);
		if (n->then_body)
			parser_mark_synthetic_debug_loc(n->then_body);
		if (n->else_body)
			parser_mark_synthetic_debug_loc(n->else_body);
		if (n->body)
			parser_mark_synthetic_debug_loc(n->body);
		if (n->args)
			parser_mark_synthetic_debug_loc(n->args);
	}
}

static PendingStructParam *
pending_struct_params_push(void)
{
	return parser_push_slot((void **)&pscope.pending_struct_params,
	                        &pscope.pending_struct_param_count,
	                        &pscope.pending_struct_param_cap, 8,
	                        sizeof(PendingStructParam));
}

int 
parser_find_enum_const(const char *name, int *out_value)
{
	for (int i = ptab.enum_const_count - 1; i >= 0; i--) {
		EnumConst *enum_consts = (EnumConst *)ptab.enum_consts;
		const char *a = enum_consts[i].name;
		const char *b = name;
		int same = 1;

		if (!a || !b) {
			same = 0;
		} else {
			while (*a || *b) {
				if (*a != *b) {
					same = 0;
					break;
				}
				a++;
				b++;
			}
		}
		if (same) {
			if (out_value)
				*out_value = enum_consts[i].value;
			return 1;
		}
	}

	return 0;
}

void 
parser_add_enum_const(const char *name, int value)
{
	EnumConst *ec;
	EnumConst *enum_consts = (EnumConst *)ptab.enum_consts;
	int scope_start = parser_enum_const_scope_depth > 0 ?
	                  parser_enum_const_scope_stack[parser_enum_const_scope_depth - 1] : 0;

	for (int i = ptab.enum_const_count - 1; i >= scope_start; i--) {
		const char *a = enum_consts[i].name;
		const char *b = name;
		int same = 1;

		if (!a || !b) {
			same = 0;
		} else {
			while (*a || *b) {
				if (*a != *b) {
					same = 0;
					break;
				}
				a++;
				b++;
			}
		}
		if (same)
			fatal_cur("duplicate enumerator: %s\n", name ? name : "");
	}

	if (ptab.enum_const_count >= ptab.enum_const_cap) {
		EnumConst *old_items = enum_consts;
		int old_cap = ptab.enum_const_cap;
		EnumConst *new_items;
		int new_cap = old_cap ? old_cap * 2 : 32;

		new_items = xmalloc(sizeof(EnumConst) * (size_t)new_cap);
		if (old_items && old_cap > 0)
			memcpy(new_items, old_items, sizeof(EnumConst) * (size_t)old_cap);
		parser_zero_grown_tail(new_items, old_cap, new_cap, sizeof(EnumConst));
		xfree(ptab.enum_consts);
		ptab.enum_consts = new_items;
		ptab.enum_const_cap = new_cap;
		enum_consts = (EnumConst *)ptab.enum_consts;
	}

	ec = &enum_consts[ptab.enum_const_count++];
	STRNCPY(ec->name, name, sizeof(ec->name) - 1);
	ec->value = value;
}

int
parser_has_visible_enum_tag(const char *name)
{
	return parser_find_visible_enum_tag_or_null(name) != NULL;
}

int
parser_enum_tag_is_complete(const char *name)
{
	EnumTag *tag = parser_find_visible_enum_tag_or_null(name);
	return tag && tag->is_complete;
}

static EnumTag *
parser_find_visible_enum_tag_or_null(const char *name)
{
	if (!name || !name[0])
		return NULL;

	for (int i = ptab.enum_tag_count - 1; i >= 0; i--) {
		if (parser_ident_eq(ptab.enum_tags[i].name, name))
			return &ptab.enum_tags[i];
	}

	return NULL;
}

static EnumTag *
parser_find_current_scope_enum_tag_or_null(const char *name)
{
	int scope_start;

	if (!name || !name[0])
		return NULL;

	scope_start = parser_enum_tag_scope_depth > 0 ?
	              parser_enum_tag_scope_stack[parser_enum_tag_scope_depth - 1] : 0;

	for (int i = ptab.enum_tag_count - 1; i >= scope_start; i--) {
		if (parser_ident_eq(ptab.enum_tags[i].name, name))
			return &ptab.enum_tags[i];
	}
	return NULL;
}

void
parser_declare_enum_tag(const char *name)
{
	if (!name || !name[0])
		return;

	if (parser_trace_toplevel_enabled()) {
		EnumTag *existing = parser_find_current_scope_enum_tag_or_null(name);
		fprintf(stderr,
		        "tcc parse: enum-declare name=%s existing=%p complete=%d count=%d cap=%d\n",
		        name, (void *)existing, existing ? existing->is_complete : -1,
		        ptab.enum_tag_count, ptab.enum_tag_cap);
	}

	if (parser_find_current_scope_enum_tag_or_null(name))
		return;
	StructDef *conflict_struct = find_struct_in_current_scope_or_null(name);
	if (parser_trace_toplevel_enabled()) {
		fprintf(stderr,
		        "tcc parse: enum-declare conflict-check name=%s struct=%p struct_count=%d scope_base=%d\n",
		        name, (void *)conflict_struct, ptab.struct_count,
		        parser_current_tag_scope_base());
	}
	if (conflict_struct)
		fatal_cur("duplicate enum tag: %s\n", name);

	if (ptab.enum_tag_count >= ptab.enum_tag_cap) {
		EnumTag *old_items = ptab.enum_tags;
		int old_cap = ptab.enum_tag_cap;
		int new_cap = old_cap ? old_cap * 2 : 16;
		EnumTag *new_items = xmalloc(sizeof(EnumTag) * (size_t)new_cap);

		if (old_items && old_cap > 0)
			memcpy(new_items, old_items, sizeof(EnumTag) * (size_t)old_cap);
		parser_zero_grown_tail(new_items, old_cap, new_cap, sizeof(EnumTag));
		xfree(old_items);
		ptab.enum_tags = new_items;
		ptab.enum_tag_cap = new_cap;
	}

	EnumTag *tag = &ptab.enum_tags[ptab.enum_tag_count++];
	memset(tag, 0, sizeof(*tag));
	STRNCPY(tag->name, name, 63);
	if (parser_trace_toplevel_enabled()) {
		fprintf(stderr,
		        "tcc parse: enum-declare added name=%s tag=%p index=%d\n",
		        name, (void *)tag, ptab.enum_tag_count - 1);
	}
}

void
parser_define_enum_tag(const char *name)
{
	EnumTag *tag;

	if (!name || !name[0])
		return;

	tag = parser_find_current_scope_enum_tag_or_null(name);
	if (parser_trace_toplevel_enabled()) {
		fprintf(stderr,
		        "tcc parse: enum-define name=%s existing=%p complete=%d count=%d cap=%d\n",
		        name, (void *)tag, tag ? tag->is_complete : -1,
		        ptab.enum_tag_count, ptab.enum_tag_cap);
	}
	if (tag) {
		if (tag->is_complete)
			fatal_cur("duplicate enum tag: %s\n", name);
		tag->is_complete = 1;
		if (parser_trace_toplevel_enabled()) {
			fprintf(stderr,
			        "tcc parse: enum-define completed-existing name=%s tag=%p\n",
			        name, (void *)tag);
		}
		return;
	}
	StructDef *conflict_struct = find_struct_in_current_scope_or_null(name);
	if (parser_trace_toplevel_enabled()) {
		fprintf(stderr,
		        "tcc parse: enum-define conflict-check name=%s struct=%p struct_count=%d scope_base=%d\n",
		        name, (void *)conflict_struct, ptab.struct_count,
		        parser_current_tag_scope_base());
		if (conflict_struct) {
			fprintf(stderr,
			        "tcc parse: enum-define conflict-struct name=%s struct_name=%s complete=%d size=%d\n",
			        name,
			        conflict_struct->name[0] ? conflict_struct->name : "<anon>",
			        conflict_struct->is_complete, conflict_struct->size);
		}
	}
	if (conflict_struct)
		fatal_cur("duplicate enum tag: %s\n", name);

	if (ptab.enum_tag_count >= ptab.enum_tag_cap) {
		EnumTag *old_items = ptab.enum_tags;
		int old_cap = ptab.enum_tag_cap;
		int new_cap = old_cap ? old_cap * 2 : 16;
		EnumTag *new_items = xmalloc(sizeof(EnumTag) * (size_t)new_cap);

		if (old_items && old_cap > 0)
			memcpy(new_items, old_items, sizeof(EnumTag) * (size_t)old_cap);
		parser_zero_grown_tail(new_items, old_cap, new_cap, sizeof(EnumTag));
		xfree(old_items);
		ptab.enum_tags = new_items;
		ptab.enum_tag_cap = new_cap;
	}

	tag = &ptab.enum_tags[ptab.enum_tag_count++];
	memset(tag, 0, sizeof(*tag));
	STRNCPY(tag->name, name, 63);
	tag->is_complete = 1;
	if (parser_trace_toplevel_enabled()) {
		fprintf(stderr,
		        "tcc parse: enum-define added name=%s tag=%p index=%d\n",
		        name, (void *)tag, ptab.enum_tag_count - 1);
	}
}

Type *
clone_type(Type *src)
{
	Type *dst = NULL;
	StructDef *agg_def;

	if (!src)
		return NULL;

	switch (src->kind) {
	case TY_VOID:
		dst = type_void();
		break;
	case TY_INT:
		if (src->size == 8)
			dst = src->is_unsigned ? type_ulong() : type_long();
		else
			dst = src->is_unsigned ? type_uint() : type_int();
		break;
	case TY_CHAR:
		dst = src->is_unsigned ? type_uchar() : type_char();
		break;
	case TY_SHORT:
		dst = src->is_unsigned ? type_ushort() : type_short();
		break;
	case TY_FLOAT:
		dst = type_float();
		break;
	case TY_DOUBLE:
		dst = type_double();
		break;
	case TY_PTR:
		dst = type_ptr(clone_type(src->base));
		break;
	case TY_ARRAY:
		dst = type_array(clone_type(src->base), src->array_len);
		break;
	case TY_FUNC: {
		Type **func_param_types = NULL;
		int func_param_count = 0;
		int func_is_variadic = 0;
		int func_fixed_param_count = 0;

		if (type_func_metadata(src, &func_param_types, &func_param_count,
		                       &func_is_variadic, &func_fixed_param_count)) {
			dst = parser_make_function_type(src->base, func_param_types,
			                                func_param_count, func_is_variadic,
			                                func_fixed_param_count);
		} else {
			dst = type_func(clone_type(src->base));
		}
		break;
	}
	case TY_STRUCT:
		agg_def = src->struct_name[0] ? find_struct_or_null(src->struct_name) : NULL;
		dst = type_struct(src->struct_name,
		                  agg_def && agg_def->size > 0 ? agg_def->size : src->size);
		break;
	case TY_UNION:
		agg_def = src->struct_name[0] ? find_struct_or_null(src->struct_name) : NULL;
		dst = type_union(src->struct_name,
		                 agg_def && agg_def->size > 0 ? agg_def->size : src->size);
		break;
	case TY_ENUM:
		dst = type_enum(src->struct_name);
		break;
	default:
		dst = src;
		break;
	}

	if (dst && dst != src) {
		if (src->qualifiers != 0 ||
		    src->source_kind != dst->source_kind ||
		    STRCMP(src->source_name, dst->source_name) != 0) {
			Type *copy = xcalloc(1, sizeof(Type));
			*copy = *dst;
			dst = copy;
		}
		dst->qualifiers = src->qualifiers;
		dst->source_kind = src->source_kind;
		STRNCPY(dst->source_name, src->source_name, sizeof(dst->source_name) - 1);
		dst->is_vm_type = src->is_vm_type;
		if (src->vla_bound_name[0])
			STRNCPY(dst->vla_bound_name, src->vla_bound_name,
			        sizeof(dst->vla_bound_name) - 1);
		dst->vla_elem_type = src->vla_elem_type ? clone_type(src->vla_elem_type) : NULL;
	}

	return dst;
}

static void
reject_void_object_type(Type *type, const char *what)
{
	if (type && type_is_void(type))
		fatal_cur("%s cannot have type void\n", what);
}

static int
type_has_incomplete_object_aggregate(Type *type)
{
	StructDef *def;

	if (!type)
		return 0;
	if (type->kind == TY_ARRAY)
		return type_has_incomplete_object_aggregate(type->base);
	if (type->kind == TY_ENUM)
		return type->struct_name[0] && !parser_enum_tag_is_complete(type->struct_name);
	if (!type_is_struct(type))
		return 0;
	def = find_struct_or_null(type->struct_name);
	return !def || !def->is_complete;
}

static int
type_is_flexible_array_member_aggregate(Type *type)
{
	StructDef *def;

	if (!type || !type_is_struct(type))
		return 0;
	def = find_struct_or_null(type->struct_name);
	return def && def->is_complete && def->has_flexible_array_member;
}

static int
type_contains_flexible_array_member_aggregate(Type *type)
{
	if (!type)
		return 0;
	if (type_is_flexible_array_member_aggregate(type))
		return 1;
	if (type_is_array(type))
		return type_contains_flexible_array_member_aggregate(type->base);
	return 0;
}

static void
reject_flexible_array_member_field_type(Type *type)
{
	if (type_is_flexible_array_member_aggregate(type))
		fatal_cur("field cannot have type with flexible array member\n");
	if (type_is_array(type) &&
	    type_contains_flexible_array_member_aggregate(type->base))
		fatal_cur("field cannot be array of type with flexible array member\n");
}

static void
reject_flexible_array_member_array_object_type(Type *type, const char *what)
{
	if (type_is_array(type) &&
	    type_contains_flexible_array_member_aggregate(type->base))
		fatal_cur("%s cannot be array of type with flexible array member\n", what);
}

static void
reject_incomplete_object_type(Type *type, const char *what)
{
	if (type_has_incomplete_object_aggregate(type))
		fatal_cur("%s cannot have incomplete type\n", what);
}

static int
type_is_complete_object_type(Type *type)
{
	if (!type)
		return 0;
	if (type_is_void(type) || type_is_function(type))
		return 0;
	if (type_has_incomplete_object_aggregate(type))
		return 0;
	if (type_is_array(type)) {
		if (type->array_len <= 0)
			return 0;
		return type_is_complete_object_type(type->base);
	}
	return 1;
}

Type *
parser_find_typedef(const char *name)
{
	TypedefName *typedefs = (TypedefName *)ptab.typedefs;
	int index = parser_find_typedef_index_optional(name);
	return index >= 0 ? typedefs[index].type : NULL;
}

Type *
parser_find_typedef_linear(const char *name)
{
	TypedefName *typedefs = (TypedefName *)ptab.typedefs;
	int match = -1;

	if (parser_trace_toplevel_enabled() && name && STRCMP(name, "LangStandard") == 0) {
		fprintf(stderr,
		        "tcc parse: typedef-linear enter name=%s count=%d typedefs=%p\n",
		        name, ptab.typedef_count, ptab.typedefs);
	}

	if (!name || !name[0] || !typedefs)
		return NULL;

	for (int i = 0; i < ptab.typedef_count; i++) {
		const char *existing = typedefs[i].name;
		const char *a = existing;
		const char *b = name;
		int same = 1;

		if (!a || !b) {
			same = 0;
		} else {
			while (*a || *b) {
				if (*a != *b) {
					same = 0;
					break;
				}
				a++;
				b++;
			}
		}

		if (same)
			match = i;
		if (parser_trace_toplevel_enabled() && name && STRCMP(name, "LangStandard") == 0 && i < 8) {
			fprintf(stderr,
			        "tcc parse: typedef-linear slot[%d] name=%s same=%d type=%p\n",
			        i, typedefs[i].name, same, (void *)typedefs[i].type);
		}
	}

	if (parser_trace_toplevel_enabled() && name && STRCMP(name, "LangStandard") == 0) {
		fprintf(stderr,
		        "tcc parse: typedef-linear done name=%s match=%d type=%p\n",
		        name, match, match >= 0 ? (void *)typedefs[match].type : NULL);
	}

	return match >= 0 ? typedefs[match].type : NULL;
}

int 
parser_is_typedef_name(const char *name)
{
	return parser_find_typedef(name) != NULL;
}

void 
parser_add_typedef_name(const char *name, Type *type)
{
	TypedefName *td;
	int existing_index = parser_find_typedef_in_current_scope(name);

	parser_reject_current_scope_ordinary_identifier_for_typedef(name);

	if (existing_index >= 0) {
		TypedefName *existing = &((TypedefName *)ptab.typedefs)[existing_index];

		/*
		 * C permits a typedef name to be redeclared in the same scope when the
		 * redeclaration names the same type.  Reject incompatible duplicates,
		 * but continue accepting common harmless repeats such as:
		 *
		 *     #include <stdio.h>   // pulls in the stub common size_t typedef
		 *     typedef unsigned long size_t;
		 */
		if (!parser_redecl_type_compatible(existing->type, type))
			fatal_cur("Conflicting typedef for '%s'\n", name);
		return;
	}

	parser_ensure_typedef_capacity();

	td = &((TypedefName *)ptab.typedefs)[ptab.typedef_count++];
	STRNCPY(td->name, name, sizeof(td->name) - 1);
	td->type = clone_type(type);
	if (td->type && !td->type->struct_name[0]) {
		const char *tag_name = "";

		if (type_is_enum(type) && type->struct_name[0]) {
			tag_name = type->struct_name;
		} else if (type_is_struct(type) || type_is_union(type)) {
			tag_name = parser_resolve_struct_type_name(type);
		}
		if (tag_name[0]) {
			STRNCPY(td->type->struct_name, tag_name,
			        sizeof(td->type->struct_name) - 1);
		}
	}
	if (parser_trace_toplevel_enabled() && STRCMP(name, "LangStandard") == 0) {
		int dump_n = ptab.typedef_count < 8 ? ptab.typedef_count : 8;
		fprintf(stderr,
		        "tcc parse: typedef-add name=%s index=%d type=%p count=%d typedefs=%p\n",
		        name, ptab.typedef_count - 1, (void *)td->type, ptab.typedef_count,
		        ptab.typedefs);
		for (int di = 0; di < dump_n; di++) {
			TypedefName *dump = &((TypedefName *)ptab.typedefs)[di];
			fprintf(stderr,
			        "tcc parse: typedef-slot[%d] name=%s type=%p\n",
			        di, dump->name, (void *)dump->type);
		}
	}
	parser_invalidate_typedef_lookup_cache();
}

static int eval_const_array_size(void);
static int eval_const_array_size_checked(int *is_constant);
static int eval_const_array_size_impl(int allow_ternary, int *is_constant);

static void
reject_alignas_token(const Token *token)
{
	if (!token_is_alignas_keyword(token))
		return;

	if (STRCMP(token->text, "_Alignas") == 0) {
		if (!tcc_lang_at_least(LANG_C11))
			fatal_token(token, "_Alignas is not allowed before C11\n");
		return;
	}

	if (!tcc_lang_at_least(LANG_C23))
		fatal_token(token, "alignas is not allowed before C23\n");
}

int
parse_alignment_specifiers(void)
{
	int requested = 0;

	while (token_starts_alignas_specifier(lexer_peek(), lexer_peek_ahead(1))) {
		const Token *token = lexer_peek();
		int align = 0;

		reject_alignas_token(token);
		lexer_next();
		expect(TOK_LPAREN);

		if (lexer_peek()->kind != TOK_NUM &&
		    is_type_start_token(lexer_peek()->kind, lexer_peek()->text)) {
			Type *type = parse_type_name();

			while (lexer_peek()->kind == TOK_LBRACKET) {
				Node *expr;
				int len;

				lexer_next();
				if (lexer_peek()->kind == TOK_RBRACKET)
					fatal_cur("Expected numeric array length in alignas(type[n])\n");
				expr = fold_constants(parse_assignment());
				if (!expr || expr->kind != ND_NUM)
					fatal_cur("alignas(type[n]) requires an integer constant expression\n");
				len = (int)expr->long_value;
				if (len <= 0)
					fatal_cur("alignas(type[n]) array length must be positive\n");
				expect(TOK_RBRACKET);
				type = type_array(type, len);
			}

			if (token->text &&
			    STRCMP(token->text, "alignas") == 0 &&
			    type_is_void(type) &&
			    lexer_peek()->kind == TOK_RPAREN &&
			    (lexer_peek_ahead(1)->kind == TOK_LBRACE ||
			     lexer_peek_ahead(1)->kind == TOK_SEMI ||
			     lexer_peek_ahead(1)->kind == TOK_COMMA)) {
				fatal_token(token,
				            "'alignas' is a reserved identifier in C23 and cannot be used as function name\n");
			}

			if (!type_is_complete_object_type(type))
				fatal_token(token, "alignment specifier requires a complete object type\n");

			align = type_alignof(type);
		} else {
			Node *expr = fold_constants(parse_assignment());
			if (!expr || expr->kind != ND_NUM)
				fatal_token(token, "alignment specifier requires an integer constant expression\n");
			if (expr->long_value <= 0)
				fatal_token(token, "alignment must be positive\n");
			if ((expr->long_value & (expr->long_value - 1)) != 0)
				fatal_token(token, "alignment must be a power of two\n");
			if (expr->long_value > (1 << 20))
				fatal_token(token, "alignment is too large\n");
			align = (int)expr->long_value;
		}

		expect(TOK_RPAREN);
		if (align > requested)
			requested = align;
	}

	return requested;
}

void
parser_validate_decl_alignment(int requested_align, Type *type)
{
	int natural_align;

	if (requested_align <= 0 || !type)
		return;

	natural_align = type_alignof(type);
	if (natural_align > 0 && requested_align < natural_align)
		fatal_cur("alignment specifier cannot reduce natural alignment\n");
}

void
parser_set_decl_align_request(int align)
{
	parser_decl_align_request = align > 0 ? align : 0;
}

void
parser_clear_decl_align_request(void)
{
	parser_decl_align_request = 0;
}

void
parser_set_decl_register_request(int is_register)
{
	parser_decl_register_request = is_register ? 1 : 0;
}

void
parser_clear_decl_register_request(void)
{
	parser_decl_register_request = 0;
}

int
struct_alignof_name(const char *name)
{
	StructDef *def;

	if (!name || !name[0])
		return 0;

	def = find_struct_or_null(name);
	if (!def)
		return 0;
	return aggregate_align(def);
}

static int
eval_const_null_member_address(long long *out)
{
	Type *ptr_type;
	Type *agg_type;
	long long offset = 0;
	char struct_name[64] = {0};

	if (lexer_peek()->kind != TOK_LPAREN ||
	    lexer_peek_ahead(1)->kind != TOK_LPAREN ||
	    !is_type_start_token(lexer_peek_ahead(2)->kind, lexer_peek_ahead(2)->text))
		return 0;

	expect(TOK_LPAREN);
	expect(TOK_LPAREN);

	ptr_type = parse_type_name();
	while (lexer_peek()->kind == TOK_STAR) {
		lexer_next();
		ptr_type = type_ptr(ptr_type);
	}
	expect(TOK_RPAREN);

	if (lexer_peek()->kind != TOK_NUM)
		return 0;
	lexer_next();
	expect(TOK_RPAREN);

	agg_type = ptr_type;
	if (agg_type && type_is_pointer(agg_type) && type_pointee(agg_type))
		agg_type = type_pointee(agg_type);
	if (!agg_type || (!type_is_struct(agg_type) && !type_is_union(agg_type)))
		return 0;
	if (!agg_type->struct_name[0])
		return 0;
	STRNCPY(struct_name, agg_type->struct_name, sizeof(struct_name) - 1);

	while (lexer_peek()->kind == TOK_ARROW || lexer_peek()->kind == TOK_DOT) {
		TokenKind access = lexer_peek()->kind;
		const Token *field_tok;
		Field *field;
		Type *field_type;
		Type *next_agg;

		lexer_next();
		field_tok = lexer_peek();
		if (field_tok->kind != TOK_IDENT || !field_tok->text)
			return 0;

		field = find_field(struct_name, field_tok->text);
		offset += field->offset;
		lexer_next();

		field_type = field->type;
		next_agg = field_type;
		if (access == TOK_ARROW &&
		    next_agg && type_is_pointer(next_agg) && type_pointee(next_agg))
			next_agg = type_pointee(next_agg);

		if (next_agg &&
		    (type_is_struct(next_agg) || type_is_union(next_agg)) &&
		    next_agg->struct_name[0]) {
			STRNCPY(struct_name, next_agg->struct_name, sizeof(struct_name) - 1);
		} else {
			struct_name[0] = '\0';
			break;
		}
	}

	*out = offset;
	return 1;
}

static int
eval_const_array_size_impl(int allow_ternary, int *is_constant)
{
	long long val = 0;
	Type *cast_t = NULL;
	if (lexer_peek()->kind == TOK_LPAREN) {
		lexer_next();
		/* Check if this is a cast: (type)(expr) */
		if (is_type_start_token(lexer_peek()->kind, lexer_peek()->text)) {
			/* Skip the cast type -- parse and discard. */
			cast_t = parse_type_name();
			(void)cast_t;
			while (lexer_peek()->kind == TOK_STAR) lexer_next();
			expect(TOK_RPAREN);
			/* Now parse the cast target. */
			val = eval_const_array_size_impl(1, is_constant);
		} else {
			val = eval_const_array_size_impl(1, is_constant);
			expect(TOK_RPAREN);
		}
	} else if (lexer_peek()->kind == TOK_NUM) {
		val = lexer_peek()->value; lexer_next();
	} else if (lexer_peek()->kind == TOK_SIZEOF) {
		lexer_next(); /* sizeof */
		val = parse_sizeof_type_or_expr();
	} else if (lexer_peek()->kind == TOK_IDENT &&
	           lexer_peek()->text && STRCMP(lexer_peek()->text, "offsetof") == 0) {
		/* offsetof(StructType, field) */
		lexer_next(); /* offsetof */
		expect(TOK_LPAREN);
		/* type name */
		char struct_name[64] = {0};
		if (lexer_peek()->kind == TOK_STRUCT || lexer_peek()->kind == TOK_UNION)
			lexer_next();
		if (lexer_peek()->kind == TOK_IDENT) {
			STRNCPY(struct_name, lexer_peek()->text, sizeof(struct_name) - 1);
			lexer_next();
		}
		expect(TOK_COMMA);
		/* field name */
		char field_name[64] = {0};
		if (lexer_peek()->kind == TOK_IDENT) {
			STRNCPY(field_name, lexer_peek()->text, sizeof(field_name) - 1);
			lexer_next();
		}
		expect(TOK_RPAREN);
		val = 0;
		if (struct_name[0] && field_name[0]) {
			StructDef *def = find_struct_or_null(struct_name);
			if (def) {
				for (int fi = 0; fi < def->field_count; fi++) {
					if (STRCMP(def->fields[fi].name, field_name) == 0) {
						val = def->fields[fi].offset;
						break;
					}
				}
			}
		}
	} else if (lexer_peek()->kind == TOK_AMP) {
		lexer_next();
		if (!eval_const_null_member_address(&val))
			val = eval_const_array_size_impl(1, is_constant);
	} else if (lexer_peek()->kind == TOK_ALIGNOF) {
		const Token *op_token = lexer_peek();
		lexer_next();
		val = parse_alignof_type_or_expr(op_token);
	} else if (lexer_peek()->kind == TOK_IDENT) {
		int ev = 0;
		if (parser_find_enum_const(lexer_peek()->text, &ev)) {
			val = ev;
			lexer_next();
		} else {
			Node *expr = fold_constants(parse_assignment());
			if (!expr || expr->kind != ND_NUM) {
				if (is_constant)
					*is_constant = 0;
				return 1;
			}
			return (int)expr->long_value;
		}
	}
	while (lexer_peek()->kind == TOK_STAR || lexer_peek()->kind == TOK_PLUS ||
	       lexer_peek()->kind == TOK_MINUS || lexer_peek()->kind == TOK_SLASH ||
	       lexer_peek()->kind == TOK_PERCENT || lexer_peek()->kind == TOK_AMP ||
	       lexer_peek()->kind == TOK_PIPE || lexer_peek()->kind == TOK_AND ||
	       lexer_peek()->kind == TOK_OR || lexer_peek()->kind == TOK_SHL ||
	       lexer_peek()->kind == TOK_SHR || lexer_peek()->kind == TOK_EQ ||
	       lexer_peek()->kind == TOK_NE || lexer_peek()->kind == TOK_LT ||
	       lexer_peek()->kind == TOK_GT || lexer_peek()->kind == TOK_LE ||
	       lexer_peek()->kind == TOK_GE) {
		TokenKind op = lexer_peek()->kind; lexer_next();
		long long rhs = eval_const_array_size_impl(0, is_constant);
		if (op == TOK_STAR) val *= rhs;
		else if (op == TOK_PLUS) val += rhs;
		else if (op == TOK_MINUS) val -= rhs;
		else if (op == TOK_SLASH && rhs != 0) val /= rhs;
		else if (op == TOK_PERCENT && rhs != 0) val %= rhs;
		else if (op == TOK_AMP) val &= rhs;
		else if (op == TOK_PIPE) val |= rhs;
		else if (op == TOK_AND) val = (val && rhs);
		else if (op == TOK_OR) val = (val || rhs);
		else if (op == TOK_SHL) val <<= rhs;
		else if (op == TOK_SHR) val >>= rhs;
		else if (op == TOK_EQ) val = (val == rhs);
		else if (op == TOK_NE) val = (val != rhs);
		else if (op == TOK_LT) val = (val < rhs);
		else if (op == TOK_GT) val = (val > rhs);
		else if (op == TOK_LE) val = (val <= rhs);
		else if (op == TOK_GE) val = (val >= rhs);
	}
	/* Handle ternary in array sizes after binary operators, so
	 * "sizeof(int) == 4 ? A : B" tests the comparison result. */
	if (allow_ternary && lexer_peek()->kind == TOK_QUESTION) {
		long long then_val;
		long long else_val = val;
		lexer_next(); /* ? */
		then_val = eval_const_array_size_impl(1, is_constant);
		if (lexer_peek()->kind == TOK_COLON) {
			lexer_next(); /* : */
			else_val = eval_const_array_size_impl(1, is_constant);
		}
		if (val)
			val = then_val;
		else
			val = else_val;
	}
	return val;
}

static int
eval_const_array_size(void)
{
	int is_constant = 1;

	return eval_const_array_size_impl(1, &is_constant);
}

static int
eval_const_array_size_checked(int *is_constant)
{
	int local_is_constant = 1;
	int value = eval_const_array_size_impl(1, &local_is_constant);

	if (is_constant)
		*is_constant = local_is_constant;
	return value;
}

int
parser_array_bound_contains_nonconstant_identifier(void)
{
	int paren_depth = 0;

	for (int i = 0;; i++) {
		const Token *tok = lexer_peek_ahead(i);

		if (tok->kind == TOK_EOF)
			return 0;
		if (paren_depth == 0 && tok->kind == TOK_RBRACKET)
			return 0;

		if (tok->kind == TOK_SIZEOF || tok->kind == TOK_ALIGNOF ||
		    (tok->kind == TOK_IDENT && tok->text &&
		     STRCMP(tok->text, "offsetof") == 0)) {
			if (lexer_peek_ahead(i + 1)->kind == TOK_LPAREN) {
				int depth = 1;
				i += 2;
				while (depth > 0 && lexer_peek_ahead(i)->kind != TOK_EOF) {
					TokenKind k = lexer_peek_ahead(i)->kind;
					if (k == TOK_LPAREN)
						depth++;
					else if (k == TOK_RPAREN)
						depth--;
					i++;
				}
				i--;
			}
			continue;
		}

		if (tok->kind == TOK_LPAREN) {
			paren_depth++;
			continue;
		}
		if (tok->kind == TOK_RPAREN) {
			if (paren_depth > 0)
				paren_depth--;
			continue;
		}

		if (tok->kind == TOK_IDENT) {
			TokenKind prev_kind = TOK_EOF;
			TokenKind prev_prev_kind = TOK_EOF;
			int enum_value = 0;
			if (i > 0)
				prev_kind = lexer_peek_ahead(i - 1)->kind;
			if (i > 1)
				prev_prev_kind = lexer_peek_ahead(i - 2)->kind;
			if ((prev_kind == TOK_DOT && prev_prev_kind != TOK_DOT) ||
			    prev_kind == TOK_ARROW ||
			    prev_kind == TOK_STRUCT || prev_kind == TOK_UNION ||
			    prev_kind == TOK_ENUM ||
			    parser_is_typedef_name(tok->text))
				continue;
			if (!parser_find_enum_const(tok->text, &enum_value))
				return 1;
		}
	}
}

long long
parser_eval_const_int_expr(void)
{
	return eval_const_array_size();
}

long long
parser_eval_const_int_expr_checked(int *is_constant)
{
	return eval_const_array_size_checked(is_constant);
}

int
parse_array_dimensions(int dims[MAX_ARRAY_DIMS], int allow_unsized_first,
                       int allow_parameter_qualifiers)
{
	int dim_count = 0;

	while (lexer_peek()->kind == TOK_LBRACKET) {
		lexer_next();

		int len = 0;
		/* Skip qualifiers inside array parameter sizes:
		 *   int x[const 5], int x[volatile 5], int x[restrict 5],
		 *   int x[static 5], int x[const *]
		 *
		 * Older builds saw volatile/restrict as identifiers.  They are now
		 * dedicated tokens, so accept both forms here for compatibility with
		 * the existing parser paths.
		 */
		while (lexer_peek()->kind == TOK_CONST ||
		       lexer_peek()->kind == TOK_VOLATILE ||
		       lexer_peek()->kind == TOK_RESTRICT ||
		       lexer_peek()->kind == TOK_ATOMIC ||
		       lexer_peek()->kind == TOK_STATIC ||
		       (lexer_peek()->kind == TOK_IDENT && lexer_peek()->text &&
		        (STRCMP(lexer_peek()->text, "volatile") == 0 ||
		         STRCMP(lexer_peek()->text, "restrict") == 0))) {
			TokenKind q = lexer_peek()->kind;
			int ident_qual = (q == TOK_IDENT && lexer_peek()->text &&
			                  (STRCMP(lexer_peek()->text, "volatile") == 0 ||
			                   STRCMP(lexer_peek()->text, "restrict") == 0));

			if (q == TOK_STATIC && tcc_lang_is_c89_or_c90())
				fatal_cur("array parameter static bounds are not allowed in C89/C90 mode\n");
			if (tcc_lang_is_c89_or_c90() &&
			    (q == TOK_CONST || q == TOK_VOLATILE || q == TOK_RESTRICT ||
			     q == TOK_ATOMIC || ident_qual))
				fatal_cur("array parameter type qualifiers are not allowed in C89/C90 mode\n");
			if (!allow_parameter_qualifiers) {
				if (q == TOK_STATIC)
					fatal_cur("array static bounds are only allowed in function parameter declarators\n");
				fatal_cur("array type qualifiers are only allowed in function parameter declarators\n");
			}
			reject_c89_c99_keyword_token(q);
			lexer_next();
		}
		/* Also handle "*" for "int x[*]" (variable-length array marker). */
		if (lexer_peek()->kind == TOK_STAR && lexer_peek_ahead(1)->kind == TOK_RBRACKET) {
			if (tcc_lang_is_c89_or_c90())
				fatal_cur("variable length array syntax is not allowed in C89/C90 mode\n");
			lexer_next(); /* consume * */
		} else if (lexer_peek()->kind != TOK_RBRACKET) {
			/* Use the ordinary expression parser here so macro-expanded
			 * parenthesized bounds such as "(A + 1)" are consumed as a full
			 * integer constant expression rather than stopping after the first
			 * token and leaving the opening parenthesis behind. */
			Node *expr = fold_constants(parse_assignment());
			if (!expr || expr->kind != ND_NUM)
				fatal_cur("Array length must be an integer constant expression\n");
			len = (int)expr->long_value;
			if (len <= 0 && !(allow_unsized_first && dim_count == 0)) {
				fatal_cur("Array length must be positive\n");
			}
		} else if (!(allow_unsized_first && dim_count == 0)) {
			fatal_cur("Expected numeric array length\n");
		}

		expect(TOK_RBRACKET);

		if (dim_count >= MAX_ARRAY_DIMS) {
			fatal_cur("Too many array dimensions\n");
		}

		dims[dim_count++] = len;
	}

	return dim_count;
}

static void
skip_parameter_array_qualifiers(void)
{
	while (lexer_peek()->kind == TOK_CONST ||
	       lexer_peek()->kind == TOK_VOLATILE ||
	       lexer_peek()->kind == TOK_RESTRICT ||
	       lexer_peek()->kind == TOK_ATOMIC ||
	       lexer_peek()->kind == TOK_STATIC ||
	       (lexer_peek()->kind == TOK_IDENT && lexer_peek()->text &&
	        (STRCMP(lexer_peek()->text, "volatile") == 0 ||
	         STRCMP(lexer_peek()->text, "restrict") == 0))) {
		TokenKind q = lexer_peek()->kind;
		int ident_qual = (q == TOK_IDENT && lexer_peek()->text &&
		                  (STRCMP(lexer_peek()->text, "volatile") == 0 ||
		                   STRCMP(lexer_peek()->text, "restrict") == 0));

		if (q == TOK_STATIC && tcc_lang_is_c89_or_c90())
			fatal_cur("array parameter static bounds are not allowed in C89/C90 mode\n");
		if (tcc_lang_is_c89_or_c90() &&
		    (q == TOK_CONST || q == TOK_VOLATILE || q == TOK_RESTRICT ||
		     q == TOK_ATOMIC || ident_qual))
			fatal_cur("array parameter type qualifiers are not allowed in C89/C90 mode\n");
		reject_c89_c99_keyword_token(q);
		lexer_next();
	}
}

static int
consume_parameter_array_qualifiers(int *out_saw_static)
{
	int qualifiers = 0;

	if (out_saw_static)
		*out_saw_static = 0;

	while (lexer_peek()->kind == TOK_CONST ||
	       lexer_peek()->kind == TOK_VOLATILE ||
	       lexer_peek()->kind == TOK_RESTRICT ||
	       lexer_peek()->kind == TOK_ATOMIC ||
	       lexer_peek()->kind == TOK_STATIC ||
	       (lexer_peek()->kind == TOK_IDENT && lexer_peek()->text &&
	        (STRCMP(lexer_peek()->text, "volatile") == 0 ||
	         STRCMP(lexer_peek()->text, "restrict") == 0))) {
		TokenKind q = lexer_peek()->kind;
		int ident_volatile = (q == TOK_IDENT && lexer_peek()->text &&
		                      STRCMP(lexer_peek()->text, "volatile") == 0);
		int ident_restrict = (q == TOK_IDENT && lexer_peek()->text &&
		                      STRCMP(lexer_peek()->text, "restrict") == 0);

		if (q == TOK_STATIC) {
			if (tcc_lang_is_c89_or_c90())
				fatal_cur("array parameter static bounds are not allowed in C89/C90 mode\n");
			reject_c89_c99_keyword_token(q);
			if (out_saw_static)
				*out_saw_static = 1;
			lexer_next();
			continue;
		}

		if (tcc_lang_is_c89_or_c90() &&
		    (q == TOK_CONST || q == TOK_VOLATILE || q == TOK_RESTRICT ||
		     q == TOK_ATOMIC || ident_volatile || ident_restrict))
			fatal_cur("array parameter type qualifiers are not allowed in C89/C90 mode\n");

		if (q == TOK_CONST) {
			reject_c89_c99_keyword_token(q);
			qualifiers |= TYPE_QUAL_CONST;
		} else if (q == TOK_VOLATILE || ident_volatile) {
			reject_c89_c99_keyword_token(q);
			qualifiers |= TYPE_QUAL_VOLATILE;
		} else if (q == TOK_RESTRICT || ident_restrict) {
			reject_c89_c99_keyword_token(q);
			qualifiers |= TYPE_QUAL_RESTRICT;
		} else if (q == TOK_ATOMIC) {
			reject_c89_c99_keyword_token(q);
			qualifiers |= TYPE_QUAL_ATOMIC;
		}

		lexer_next();
	}

	return qualifiers;
}

static long long
parse_parameter_array_bound_value(int prototype_mode, int *is_constant)
{
	long long val = 0;

	if (lexer_peek()->kind == TOK_PLUSPLUS || lexer_peek()->kind == TOK_MINUSMINUS) {
		*is_constant = 0;
		lexer_next();
		parse_parameter_array_bound_value(prototype_mode, is_constant);
		return 1;
	} else if (lexer_peek()->kind == TOK_PLUS ||
	           lexer_peek()->kind == TOK_MINUS ||
	           lexer_peek()->kind == TOK_NOT ||
	           lexer_peek()->kind == TOK_TILDE) {
		TokenKind unary = lexer_peek()->kind;
		lexer_next();
		val = parse_parameter_array_bound_value(prototype_mode, is_constant);
		if (unary == TOK_MINUS)
			val = -val;
		else if (unary == TOK_NOT)
			val = !val;
		else if (unary == TOK_TILDE)
			val = ~val;
	} else if (lexer_peek()->kind == TOK_LPAREN) {
		lexer_next();
		val = parse_parameter_array_bound_value(prototype_mode, is_constant);
		expect(TOK_RPAREN);
	} else if (lexer_peek()->kind == TOK_NUM) {
		val = lexer_peek()->value;
		lexer_next();
	} else if (lexer_peek()->kind == TOK_IDENT) {
		int ev = 0;
		if (parser_find_enum_const(lexer_peek()->text, &ev)) {
			val = ev;
		} else if (prototype_mode) {
			*is_constant = 0;
			val = 1;
		} else {
			Node *expr = fold_constants(parse_assignment());
			if (!expr || expr->kind != ND_NUM)
				*is_constant = 0;
			return expr && expr->kind == ND_NUM ? expr->long_value : 1;
		}
		lexer_next();
		if (lexer_peek()->kind == TOK_PLUSPLUS || lexer_peek()->kind == TOK_MINUSMINUS) {
			*is_constant = 0;
			lexer_next();
			val = 1;
		}
	} else {
		if (!prototype_mode) {
			Node *expr = fold_constants(parse_assignment());
			if (!expr || expr->kind != ND_NUM)
				*is_constant = 0;
			return expr && expr->kind == ND_NUM ? expr->long_value : 1;
		}
		*is_constant = 0;
		lexer_next();
		return 1;
	}

	if (lexer_peek()->kind == TOK_QUESTION) {
		lexer_next();
		{
			long long then_val = parse_parameter_array_bound_value(prototype_mode, is_constant);
			if (lexer_peek()->kind == TOK_COLON) {
				lexer_next();
				parse_parameter_array_bound_value(prototype_mode, is_constant);
			}
			if (val)
				val = then_val;
		}
	}

	while (lexer_peek()->kind == TOK_STAR || lexer_peek()->kind == TOK_PLUS ||
	       lexer_peek()->kind == TOK_MINUS || lexer_peek()->kind == TOK_SLASH ||
	       lexer_peek()->kind == TOK_AMP || lexer_peek()->kind == TOK_PIPE ||
	       lexer_peek()->kind == TOK_AND || lexer_peek()->kind == TOK_OR ||
	       lexer_peek()->kind == TOK_SHL || lexer_peek()->kind == TOK_SHR) {
		TokenKind op = lexer_peek()->kind;
		long long rhs;
		lexer_next();
		rhs = parse_parameter_array_bound_value(prototype_mode, is_constant);
		if (op == TOK_STAR) val *= rhs;
		else if (op == TOK_PLUS) val += rhs;
		else if (op == TOK_MINUS) val -= rhs;
		else if (op == TOK_SLASH && rhs != 0) val /= rhs;
		else if (op == TOK_AMP) val &= rhs;
		else if (op == TOK_PIPE) val |= rhs;
		else if (op == TOK_AND) val = (val && rhs);
		else if (op == TOK_OR) val = (val || rhs);
		else if (op == TOK_SHL) val <<= rhs;
		else if (op == TOK_SHR) val >>= rhs;
	}

	return val;
}

static Type *
parse_parameter_array_type(Type *base_type, int prototype_mode)
{
	int tail_dims[MAX_ARRAY_DIMS] = {0};
	int tail_dim_count = 0;
	int dim_index = 0;
	int first_dim_quals = 0;

	while (lexer_peek()->kind == TOK_LBRACKET) {
		Node *expr = NULL;
		int len = 0;
		int runtime_bound = 0;
		int unsized = 0;
		int saw_static = 0;

		lexer_next();
		if (dim_index == 0)
			first_dim_quals |= consume_parameter_array_qualifiers(&saw_static);
		else
			skip_parameter_array_qualifiers();

		if (lexer_peek()->kind == TOK_STAR && lexer_peek_ahead(1)->kind == TOK_RBRACKET) {
			if (tcc_lang_is_c89_or_c90())
				fatal_cur("variable length array syntax is not allowed in C89/C90 mode\n");
			lexer_next();
			runtime_bound = 1;
		} else if (lexer_peek()->kind == TOK_RBRACKET) {
			unsized = 1;
		} else {
			if (prototype_mode) {
				int is_constant = 1;
				len = (int)parse_parameter_array_bound_value(1, &is_constant);
				if (is_constant) {
					if (len <= 0)
						fatal_cur("Array length must be positive\n");
				} else {
					runtime_bound = 1;
				}
			} else {
				expr = fold_constants(parse_assignment());
				if (expr && expr->kind == ND_NUM) {
					len = (int)expr->long_value;
					if (len <= 0)
						fatal_cur("Array length must be positive\n");
				} else {
					if (tcc_lang_is_c89_or_c90())
						fatal_cur("variable length array syntax is not allowed in C89/C90 mode\n");
					runtime_bound = 1;
				}
			}
			if (!runtime_bound && len <= 0) {
				fatal_cur("Array length must be positive\n");
			}
		}

		expect(TOK_RBRACKET);

		if (dim_index == 0 && runtime_bound && !prototype_mode && expr) {
			parser_mark_synthetic_debug_loc(expr);
			pscope.param_copy_head = append_node(pscope.param_copy_head, expr);
		}

		if (dim_index > 0) {
			if (runtime_bound || unsized)
				fatal_cur("only the first dimension of a parameter VLA may be variably modified\n");
			if (tail_dim_count >= MAX_ARRAY_DIMS)
				fatal_cur("Too many array dimensions\n");
			tail_dims[tail_dim_count++] = len;
		}

		dim_index++;
	}

	Type *adjusted;
	if (tail_dim_count > 0)
		adjusted = type_ptr(build_array_type_from_dims(base_type, tail_dims, tail_dim_count));
	else
		adjusted = type_ptr(clone_type(base_type));
	if (first_dim_quals)
		adjusted = type_with_qualifiers(adjusted,
		                                type_qualifiers(adjusted) | first_dim_quals);
	return adjusted;
}

static int
parse_parameter_parenthesized_array_dims(int dims[MAX_ARRAY_DIMS], int *dim_count,
					 int prototype_mode, int allow_runtime_first)
{
	int dim_index = 0;
	int saw_runtime_first = 0;

	*dim_count = 0;

	while (lexer_peek()->kind == TOK_LBRACKET) {
		int len = 0;
		int is_constant = 1;
		int runtime_bound = 0;
		int unsized = 0;

		lexer_next();
		skip_parameter_array_qualifiers();

		if (lexer_peek()->kind == TOK_STAR &&
		    lexer_peek_ahead(1)->kind == TOK_RBRACKET) {
			runtime_bound = 1;
			lexer_next();
		} else if (lexer_peek()->kind == TOK_RBRACKET) {
			unsized = 1;
		} else {
			len = (int)parse_parameter_array_bound_value(prototype_mode, &is_constant);
			if (!is_constant)
				runtime_bound = 1;
		}

		expect(TOK_RBRACKET);

		if (runtime_bound || unsized) {
			if (tcc_lang_is_c89_or_c90())
				fatal_cur("variable length array syntax is not allowed in C89/C90 mode\n");
			if (!allow_runtime_first || dim_index > 0)
				fatal_cur("pointer-to-array parameter bounds must be constant\n");
			saw_runtime_first = 1;
		} else {
			if (len <= 0)
				fatal_cur("Array length must be positive\n");
			if (*dim_count >= MAX_ARRAY_DIMS)
				fatal_cur("Too many array dimensions\n");
			dims[(*dim_count)++] = len;
		}

		dim_index++;
	}

	return saw_runtime_first;
}

static Type *
parse_parameter_declarator_suffix(Type *param_type, char param_name_buf[64],
                                  int prototype_mode)
{
	int leading_ptr_quals[16] = {0};
	int leading_ptr_count = 0;
	int parsed_parenthesized_param = 0;
	const Token *param;

	while (lexer_peek()->kind == TOK_STAR) {
		if (leading_ptr_count >= 16)
			fatal_cur("parameter declarator pointer nesting too deep\n");
		lexer_next();
		leading_ptr_quals[leading_ptr_count++] = consume_type_qualifiers();
	}

	/*
	 * Function-pointer parameters use a parenthesized declarator:
	 *
	 *     int call_it(int (*fn)(int), int value)
	 *
	 * The simple parameter parser used to expect the declarator name
	 * immediately after the base type and rejected the opening '('.
	 * For now we lower function-pointer parameters as ordinary pointer
	 * parameters, which matches the representation already used for
	 * local/global function-pointer variables.
	 */
	if (lexer_peek()->kind == TOK_LPAREN &&
	    lexer_peek_ahead(1)->kind == TOK_STAR) {
		int ptr_depth = 1;
		int nested_ptr_groups = 0;
		int ptr_dims[MAX_ARRAY_DIMS] = {0};
		int ptr_dim_count = 0;
		int post_dims[MAX_ARRAY_DIMS] = {0};
		int post_dim_count = 0;
		Type *decl_type = NULL;

		lexer_next(); /* ( */
		lexer_next(); /* * */

		for (;;) {
			if (lexer_peek()->kind == TOK_CONST) {
				lexer_next();
				continue;
			}
			if (lexer_peek()->kind == TOK_STAR) {
				ptr_depth++;
				lexer_next();
				continue;
			}
			break;
		}

		while (lexer_peek()->kind == TOK_LPAREN &&
		       lexer_peek_ahead(1)->kind == TOK_STAR) {
			lexer_next(); /* ( */
			lexer_next(); /* * */
			nested_ptr_groups++;
			ptr_depth++;
			for (;;) {
				if (lexer_peek()->kind == TOK_CONST) {
					lexer_next();
					continue;
				}
				if (lexer_peek()->kind == TOK_STAR) {
					ptr_depth++;
					lexer_next();
					continue;
				}
				break;
			}
		}

		const Token *fp_param = lexer_peek();
		if (fp_param->kind == TOK_IDENT) {
			parser_require_decl_identifier(fp_param, "function pointer parameter name");
			STRNCPY(param_name_buf, fp_param->text ? fp_param->text : "",
			        63);
			lexer_next();
		} else if (fp_param->kind != TOK_LBRACKET && fp_param->kind != TOK_RPAREN) {
			fatal_cur("Expected function pointer parameter name\n");
		}

		parse_parameter_parenthesized_array_dims(ptr_dims, &ptr_dim_count,
						 prototype_mode, 0);
		for (int group = 0; group < nested_ptr_groups; group++)
			expect(TOK_RPAREN);
		expect(TOK_RPAREN);
		parse_parameter_parenthesized_array_dims(post_dims, &post_dim_count,
						 prototype_mode, 0);

		if (lexer_peek()->kind == TOK_LPAREN) {
			Type **fp_param_types = NULL;
			int fp_param_count = 0;
			int fp_is_variadic = 0;
			int fp_fixed_params = 0;
			int fp_has_prototype = 0;

			parse_prototype_param_list(&fp_param_types, &fp_param_count,
			                          &fp_is_variadic, &fp_fixed_params,
			                          &fp_has_prototype, 1);
			decl_type = type_ptr(fp_has_prototype
			                     ? parser_make_function_type(param_type,
			                                                 fp_param_types,
			                                                 fp_param_count,
			                                                 fp_is_variadic,
			                                                 fp_fixed_params)
			                     : type_func(clone_type(param_type)));
		} else if (post_dim_count > 0) {
			decl_type = type_ptr(build_array_type_from_dims(param_type, post_dims,
								 post_dim_count));
		} else {
			decl_type = type_ptr(param_type);
		}

		for (int depth = 1; depth < ptr_depth; depth++)
			decl_type = type_ptr(decl_type);

		if (ptr_dim_count > 0)
			decl_type = build_array_type_from_dims(decl_type, ptr_dims, ptr_dim_count);

		param_type = decl_type;
		parsed_parenthesized_param = 1;
	}

	/* C adjusts array parameters, including typedef-named arrays, to pointers. */
	if (param_type->kind == TY_ARRAY)
		param_type = type_ptr(clone_type(param_type->base));

	/* Typedef-named function parameters adjust to function pointers too. */
	if (param_type->kind == TY_FUNC)
		param_type = type_ptr(clone_type(param_type));

	if (!parsed_parenthesized_param) {
		param = lexer_peek();
		if (param->kind == TOK_IDENT) {
			TokenKind after_name = lexer_peek_ahead(1)->kind;
			if (after_name == TOK_COMMA || after_name == TOK_RPAREN) {
				parser_require_decl_identifier(param, "parameter name");
				STRNCPY(param_name_buf, param->text ? param->text : "", 63);
				lexer_next();
				goto finalize_param_type;
			}
		} else if (param->kind == TOK_COMMA || param->kind == TOK_RPAREN) {
			goto finalize_param_type;
		}
	}

	if (!parsed_parenthesized_param &&
	    lexer_peek()->kind == TOK_LPAREN &&
	    lexer_peek_ahead(1)->kind == TOK_LBRACKET) {
		lexer_next();
		param_type = parse_parameter_array_type(param_type, prototype_mode);
		expect(TOK_RPAREN);
		parsed_parenthesized_param = 1;
	}

	if (!parsed_parenthesized_param && lexer_peek()->kind == TOK_LPAREN) {
		Type **fp_param_types = NULL;
		int fp_param_count = 0;
		int fp_is_variadic = 0;
		int fp_fixed_params = 0;
		int fp_has_prototype = 0;

		parse_prototype_param_list(&fp_param_types, &fp_param_count,
		                          &fp_is_variadic, &fp_fixed_params,
		                          &fp_has_prototype, 1);
		param_type = type_ptr(fp_has_prototype
		                      ? parser_make_function_type(param_type,
		                                                  fp_param_types,
		                                                  fp_param_count,
		                                                  fp_is_variadic,
		                                                  fp_fixed_params)
		                      : type_func(clone_type(param_type)));
	}

	if (!parsed_parenthesized_param) {
		const Token *param = lexer_peek();
		if (param->kind != TOK_IDENT) {
			/*
			 * Accept unnamed parameters in prototypes and the special
			 * single void parameter form after the common type parser.
			 */
			if (lexer_peek()->kind != TOK_COMMA &&
			    lexer_peek()->kind != TOK_RPAREN &&
			    lexer_peek()->kind != TOK_LBRACKET)
				fatal_cur("Expected parameter name\n");
		} else {
			parser_require_decl_identifier(param, "parameter name");
			STRNCPY(param_name_buf, param->text ? param->text : "", 63);
			lexer_next();
		}
	}

	if (!parsed_parenthesized_param && lexer_peek()->kind == TOK_LPAREN) {
		Type **fp_param_types = NULL;
		int fp_param_count = 0;
		int fp_is_variadic = 0;
		int fp_fixed_params = 0;
		int fp_has_prototype = 0;

		parse_prototype_param_list(&fp_param_types, &fp_param_count,
		                          &fp_is_variadic, &fp_fixed_params,
		                          &fp_has_prototype, 1);
		param_type = type_ptr(fp_has_prototype
		                      ? parser_make_function_type(param_type,
		                                                  fp_param_types,
		                                                  fp_param_count,
		                                                  fp_is_variadic,
		                                                  fp_fixed_params)
		                      : type_func(clone_type(param_type)));
	}

	if (lexer_peek()->kind == TOK_LBRACKET) {
		param_type = parse_parameter_array_type(param_type, prototype_mode);
		if (lexer_peek()->kind == TOK_LPAREN)
			fatal_cur("array elements cannot have function type\n");
	}

	/*
	 * Array parameters are adjusted to pointers by C.  This also matters
	 * when the array type arrives through a typedef, for example:
	 *
	 *     typedef int (*fptr4[4])(int);
	 *     int f4(fptr4 fp, int i) { return (*fp[i])(i); }
	 *
	 * Here fp is a pointer to the first function-pointer element, not a
	 * by-value array object.
	 */
	if (param_type->kind == TY_ARRAY)
		param_type = type_ptr(clone_type(param_type->base));

	if (param_type->kind == TY_FUNC)
		param_type = type_ptr(clone_type(param_type));

finalize_param_type:
	while (leading_ptr_count > 0) {
		param_type = type_ptr(param_type);
		leading_ptr_count--;
		if (leading_ptr_quals[leading_ptr_count])
			param_type = type_with_qualifiers(param_type,
			                                  leading_ptr_quals[leading_ptr_count]);
	}

	if (type_is_void(param_type))
		fatal_cur("parameter cannot have type void\n");

	return param_type;
}

static Type *
parse_parameter_declarator_impl(char param_name_buf[64], int prototype_mode,
                                int *out_is_register)
{
	int saw_register = 0;

	if (token_starts_alignas_specifier(lexer_peek(), lexer_peek_ahead(1))) {
		parse_alignment_specifiers();
		fatal_cur("alignment specifier cannot be applied to a function parameter\n");
	}

	if (lexer_peek()->kind == TOK_REGISTER) {
		saw_register = 1;
		lexer_next();
	}

	if (token_starts_plain_thread_local_storage_specifier(
	        lexer_peek(), lexer_peek_ahead(1), lexer_peek_ahead(2)))
		reject_plain_thread_local_keyword_before_c23(lexer_peek());

	if (lexer_peek()->kind == TOK_THREAD_LOCAL) {
		reject_thread_local_storage_specifier();
		fatal_cur("storage class is not allowed in function parameter declarations\n");
	}

	if (lexer_peek()->kind == TOK_STATIC || lexer_peek()->kind == TOK_EXTERN ||
	    lexer_peek()->kind == TOK_AUTO || lexer_peek()->kind == TOK_TYPEDEF)
		fatal_cur("storage class is not allowed in function parameter declarations\n");
	if (lexer_peek()->kind == TOK_INLINE)
		fatal_cur("function specifier is only valid on function declarations\n");
	if (lexer_peek()->kind == TOK_NORETURN)
		fatal_cur("function specifier is only valid on function declarations\n");

	if (parser_trace_toplevel_enabled()) {
		const Token *tok = lexer_peek();
		fprintf(stderr,
		        "tcc parse: param-decl before-type kind=%s text=%s prototype=%d\n",
		        token_debug_name(tok->kind),
		        tok->text ? tok->text : "<null>",
		        prototype_mode);
	}
	Type *param_type = parse_type_name();
	parser_reject_unsupported_special_type(param_type);
	parser_reject_unsupported_complex_function_type(param_type);
	if (parser_trace_toplevel_enabled()) {
		fprintf(stderr,
		        "tcc parse: param-decl after-type type=%p kind=%d next=%s text=%s\n",
		        (void *)param_type,
		        param_type ? param_type->kind : -1,
		        token_debug_name(lexer_peek()->kind),
		        lexer_peek()->text ? lexer_peek()->text : "<null>");
	}
	Type *decl_type;

	if (parser_type_name_saw_multiple_trailing_storage_classes())
		fatal_cur("storage class is not allowed in function parameter declarations\n");
	if (parser_type_name_saw_trailing_storage_class()) {
		TokenKind trailing_storage = parser_type_name_trailing_storage_class();
		if (trailing_storage == TOK_REGISTER)
			saw_register = 1;
		else if (trailing_storage == TOK_THREAD_LOCAL)
			fatal_cur("storage class is not allowed in function parameter declarations\n");
		else
			fatal_cur("storage class is not allowed in function parameter declarations\n");
	}
	if (parser_type_name_saw_thread_local_storage_specifier())
		fatal_cur("storage class is not allowed in function parameter declarations\n");
	if (parser_type_name_saw_trailing_function_specifier())
		fatal_cur("function specifier is only valid on function declarations\n");
	if (token_starts_alignas_specifier(lexer_peek(), lexer_peek_ahead(1))) {
		parse_alignment_specifiers();
		fatal_cur("alignment specifier cannot be applied to a function parameter\n");
	}

	param_name_buf[0] = '\0';
	parser_profile_scope_enter(PARSER_PROF_PARAM_DECL);
	decl_type = parse_parameter_declarator_suffix(param_type, param_name_buf,
	                                              prototype_mode);
	parser_profile_scope_leave(PARSER_PROF_PARAM_DECL);
	if (out_is_register)
		*out_is_register = saw_register;
	return decl_type;
}

static int
looks_like_oldstyle_param_name_list(void)
{
	int i = 0;
	const Token *tok = lexer_peek_ahead(i);

	if (tok->kind != TOK_IDENT)
		return 0;
	for (;;) {
		i++;
		tok = lexer_peek_ahead(i);
		if (tok->kind == TOK_COMMA) {
			i++;
			tok = lexer_peek_ahead(i);
			if (tok->kind != TOK_IDENT)
				return 0;
			continue;
		}
		return tok->kind == TOK_RPAREN;
	}
}

static int
find_oldstyle_param_index(char **names, int count, const char *name)
{
	for (int i = 0; i < count; i++) {
		if (names[i] && STRCMP(names[i], name) == 0)
			return i;
	}
	return -1;
}

static Type *
oldstyle_promoted_param_type(Type *type)
{
	Type *promoted;

	if (!type)
		return NULL;

	if (type->kind == TY_FLOAT)
		promoted = type_double();
	else if (type->kind == TY_CHAR ||
	         type->kind == TY_SHORT ||
	         type->kind == TY_ENUM)
		promoted = type_int();
	else
		promoted = clone_type(type);

	if (promoted)
		promoted->qualifiers = 0;
	return promoted;
}

static Type *
oldstyle_declarator_shared_base(Type *type)
{
	while (type && type_is_pointer(type))
		type = type_pointee(type);
	return type ? clone_type(type) : NULL;
}

static Node *
parser_append_local_struct_comma_declarators(Node *head,
                                             const char *struct_name,
                                             Type *base_type)
{
	Node *tail = head;

	while (tail && tail->next)
		tail = tail->next;

	while (lexer_peek()->kind == TOK_COMMA) {
		Type **param_types = NULL;
		int param_count = 0;
		int is_variadic = 0;
		int fixed_params = 0;
		int has_prototype = 0;
		int extra_stars = 0;
		const Token *name;
		Type *decl_type = NULL;
		Node *decl = NULL;
		int offset = 0;

		lexer_next();
		while (lexer_peek()->kind == TOK_STAR) {
			lexer_next();
			extra_stars++;
		}

		name = lexer_peek();
		if (name->kind != TOK_IDENT)
			fatal_cur("Expected identifier in declaration list\n");
		lexer_next();

		decl_type = base_type ? clone_type(base_type) : type_for_size(TCC_SIZEOF_PTR);
		for (int i = 0; i < extra_stars; i++)
			decl_type = type_ptr(decl_type);

		if (lexer_peek()->kind == TOK_LPAREN) {
			parse_prototype_param_list(&param_types, &param_count,
			                           &is_variadic, &fixed_params,
			                           &has_prototype, 1);
			if (lexer_peek()->kind == TOK_LBRACKET)
				fatal_cur("function return array declarators are not supported\n");
			if (lexer_peek()->kind == TOK_LPAREN)
				fatal_cur("function cannot return function type\n");
			if (lexer_peek()->kind == TOK_ASSIGN)
				fatal_cur("function declaration cannot have initializer\n");
			parser_declare_function(name->text, decl_type, has_prototype,
			                        param_types, param_count, is_variadic,
			                        fixed_params, 0);
			parser_note_block_scope_function_declaration(name->text, decl_type);
			continue;
		}

		if (lexer_peek()->kind == TOK_LBRACKET) {
			int dims[MAX_ARRAY_DIMS] = {0};
			int dim_count = parse_array_dimensions(dims, 1, 0);
			Type *array_type = build_array_type_from_dims(decl_type, dims, dim_count);
			offset = add_decl_typed_local(0, name->text, array_type);
			decl = new_array_decl(name->text, offset, dims[0]);
			decl->elem_size = type_elem_size(array_type);
			decl->type = array_type;
		} else {
			offset = add_decl_typed_local(0, name->text, decl_type);
			decl = new_struct_decl(name->text, offset);
			decl->type = clone_type(decl_type);
			if (extra_stars == 0 && struct_name)
				STRNCPY(decl->struct_name, struct_name,
				        sizeof(decl->struct_name) - 1);
		}

		if (tail)
			tail->next = decl;
		else
			head = decl;
		tail = decl;
	}

	return head;
}

static Type *
parse_prototype_parameter_declarator(char param_name_buf[64]) __attribute__((unused));

static Type *
parse_prototype_parameter_declarator(char param_name_buf[64])
{
	return parse_parameter_declarator_impl(param_name_buf, 1, NULL);
}

Type *
build_array_type_from_dims(Type *base_type, int *dims, int dim_count)
{
	return build_array_type_from_dims_allow_incomplete(base_type, dims, dim_count, 0);
}

Type *
build_array_type_from_dims_allow_incomplete(Type *base_type, int *dims,
                                            int dim_count,
                                            int allow_unsized_first)
{
	Type *type = parser_canonicalize_decl_type(base_type);

	if (type_is_void(type))
		fatal_cur("array elements cannot have type void\n");

	for (int i = dim_count - 1; i >= 0; i--) {
		if (dims[i] < 0 || (dims[i] == 0 && !(allow_unsized_first && i == 0))) {
			fatal_cur("Array length must be known here\n");
		}
		type = type_array(type, dims[i]);
	}

	return type;
}

int 
consume_all_zero_initializer(void)
{
	if (lexer_peek()->kind != TOK_LBRACE)
		return 0;

	int depth = 0;
	int saw_value = 0;

	while (lexer_peek()->kind == TOK_LBRACE || depth > 0) {
		const Token *tok = lexer_peek();

		if (tok->kind == TOK_LBRACE) {
			depth++;
			lexer_next();
			continue;
		}

		if (tok->kind == TOK_RBRACE) {
			depth--;
			lexer_next();
			if (depth == 0)
				break;
			continue;
		}

		if (tok->kind == TOK_COMMA) {
			lexer_next();
			continue;
		}

		if (tok->kind == TOK_NUM && tok->value == 0) {
			saw_value = 1;
			lexer_next();
			continue;
		}

		return 0;
	}

	return saw_value;
}

int 
type_elem_size(Type *type)
{
	return type_elem_sizeof(type);
}

Node *
parse_asm_statement(void)
{
	char text[256] = {0};
	int is_volatile = 0;

	if (tcc_lang_is_c89_or_c90())
		fatal_cur("asm statements are a GNU extension and are not allowed in C89/C90 mode\n");

	lexer_next(); /* asm */

	/* v98: preserve optional volatile qualifier in asm volatile ("...").
	 * Once volatile is tokenised as a keyword, accept both the old
	 * identifier form and the keyword token here.
	 */
	const Token *t = lexer_peek();
	if (t->kind == TOK_VOLATILE ||
	    (t->kind == TOK_IDENT && t->text && STRCMP(t->text, "volatile") == 0)) {
		is_volatile = 1;
		lexer_next();
	}

	if (lexer_peek()->kind == TOK_LPAREN) {
		lexer_next();

		if (lexer_peek()->kind == TOK_STRING) {
			const Token *value = lexer_peek();
			STRNCPY(text, value->text, sizeof(text) - 1);
			lexer_next();
		}

		/*
		 * Accept and skip the rest of the asm operand/clobber syntax for now:
		 * asm("..." : ... : ... : ...);
		 */
		int depth = 1;
		while (depth > 0 && lexer_peek()->kind != TOK_EOF) {
			const Token *t = lexer_peek();
			if (t->kind == TOK_LPAREN)
				depth++;
			else if (t->kind == TOK_RPAREN)
				depth--;
			lexer_next();
		}
	}

	expect(TOK_SEMI);
	return new_asm(text, is_volatile);
}

Node *
parse_struct_return_assignment_statement(void)
{
	if (lexer_peek()->kind != TOK_IDENT ||
	        lexer_peek_ahead(1)->kind != TOK_ASSIGN ||
	        lexer_peek_ahead(2)->kind != TOK_IDENT ||
	        lexer_peek_ahead(3)->kind != TOK_LPAREN ||
	        lexer_peek_ahead(4)->kind != TOK_RPAREN)
		return NULL;

	const Token *dst_tok = lexer_peek();
	if (!is_struct_local(dst_tok->text))
		return NULL;

	const Token *func_tok = lexer_peek_ahead(2);
	FuncInfo *fi = find_func(func_tok->text);
	if (!fi || !fi->returns_struct)
		return NULL;

	const char *dst_struct = struct_name_local(dst_tok->text);
	if (STRCMP(dst_struct, fi->struct_name) != 0) {
		fatal_cur("Struct return assignment type mismatch\n");
	}

	lexer_next(); /* dst */
	expect(TOK_ASSIGN);
	lexer_next(); /* func */
	expect(TOK_LPAREN);
	expect(TOK_RPAREN);
	expect(TOK_SEMI);

	StructDef *def = find_struct(dst_struct);

	Node *lhs = make_local_struct_lhs_node(dst_tok->text, find_local(dst_tok->text), NULL,
	                                       dst_struct, def->size);
	Node *call = make_struct_return_call_node(func_tok->text, NULL, fi);

	return new_assign(lhs, call);
}

Node *
parse_struct_return_discard_statement(void)
{
	if (lexer_peek()->kind != TOK_IDENT ||
	        lexer_peek_ahead(1)->kind != TOK_LPAREN ||
	        lexer_peek_ahead(2)->kind != TOK_RPAREN)
		return NULL;

	const Token *func_tok = lexer_peek();
	FuncInfo *fi = find_func(func_tok->text);
	if (!fi || !fi->returns_struct)
		return NULL;

	lexer_next();
	expect(TOK_LPAREN);
	expect(TOK_RPAREN);
	expect(TOK_SEMI);

	char temp_name[64];
	snprintf(temp_name, sizeof(temp_name), "__struct_discard_%d", pfunc.struct_arg_temp_id++);

	int offset = 0;
	int struct_size = 0;
	Node *decl = make_local_struct_decl_node(temp_name, fi->struct_name, &offset, &struct_size);

	Node *lhs = make_local_struct_lhs_node(temp_name, offset, NULL,
	                                       fi->struct_name, struct_size);
	Node *call = make_struct_return_call_node(func_tok->text, NULL, fi);

	return new_block(append_node(decl, new_assign(lhs, call)));
}

Node *
parse_return_struct_call_value(void)
{
	if (!pfunc.returns_struct)
		return NULL;

	if (lexer_peek()->kind != TOK_IDENT ||
	        lexer_peek_ahead(1)->kind != TOK_LPAREN)
		return NULL;

	const Token *func = lexer_peek();
	FuncInfo *fi = find_func(func->text);
	if (!fi || !fi->returns_struct)
		return NULL;

	if (STRCMP(fi->struct_name, pfunc.return_struct_name) != 0)
		return NULL;  /* type mismatch - don't handle here, let generic path deal with it */

	lexer_next(); /* func name */
	expect(TOK_LPAREN);
	Node *args = parse_arg_list(fi);
	expect(TOK_RPAREN);
	expect(TOK_SEMI);

	char temp_name[64];
	snprintf(temp_name, sizeof(temp_name), "__struct_return_value_%d", pfunc.struct_arg_temp_id++);

	int offset = 0;
	int struct_size = 0;
	Node *decl = make_local_struct_decl_node(temp_name, fi->struct_name, &offset, &struct_size);

	Node *lhs = make_local_struct_lhs_node(temp_name, offset, NULL,
	                                       fi->struct_name, struct_size);
	Node *call = make_struct_return_call_node(func->text, args, fi);

	Node *assign = new_assign(lhs, call);

	Node *ret_var = make_local_struct_lhs_node(temp_name, offset, NULL,
	                                           fi->struct_name, struct_size);

	Node *head = decl;
	head = append_node(head, assign);
	head = append_node(head, new_return(ret_var));

	return new_block(head);
}

static int
v124_ident_call_returns_struct_at(int ident_index)
{
	const Token *name;
	int depth;
	int i;
	TokenKind trailing;

	name = lexer_peek_ahead(ident_index);
	if (name->kind != TOK_IDENT)
		return 0;

	if (lexer_peek_ahead(ident_index + 1)->kind != TOK_LPAREN)
		return 0;

	FuncInfo *fi = find_func(name->text);
	if (!fi || !fi->returns_struct)
		return 0;

	depth = 0;
	for (i = ident_index + 1; i < ident_index + 128; i++) {
		TokenKind k = lexer_peek_ahead(i)->kind;

		if (k == TOK_EOF)
			return 0;

		if (k == TOK_LPAREN) {
			depth++;
		} else if (k == TOK_RPAREN) {
			depth--;
			if (depth == 0) {
				trailing = lexer_peek_ahead(i + 1)->kind;
				return trailing == TOK_COMMA || trailing == TOK_RPAREN;
			}
		}
	}

	return 0;
}

int v124_call_starts_with_struct_temp_arg(void)
{
	if (lexer_peek()->kind != TOK_IDENT || lexer_peek_ahead(1)->kind != TOK_LPAREN)
		return 0;

	const Token *a0 = lexer_peek_ahead(2);

	if (a0->kind == TOK_LPAREN && lexer_peek_ahead(3)->kind == TOK_STRUCT)
		return 1;

	if (v124_ident_call_returns_struct_at(2))
		return 1;

	/*
	 * Limited but useful two-argument lookahead:
	 *   fn(scalar, (struct T){...})
	 *   fn(scalar, make_struct())
	 *
	 * Do not treat every identifier second argument as a struct temporary case.
	 * Normal calls such as:
	 *
	 *   return pred(value, TARGET) || other();
	 *
	 * must remain on the ordinary expression parser path so the trailing
	 * logical operator is consumed.
	 */
	if (a0->kind == TOK_IDENT || a0->kind == TOK_NUM) {
		if (lexer_peek_ahead(3)->kind == TOK_COMMA) {
			const Token *a1 = lexer_peek_ahead(4);

			if (a1->kind == TOK_LPAREN && lexer_peek_ahead(5)->kind == TOK_STRUCT)
				return 1;

			if (v124_ident_call_returns_struct_at(4))
				return 1;
		}
	}

	return 0;
}


/* v174_parse_post_pointer_qualifiers */
void 
skip_pointer_qualifiers(void)
{
	(void)consume_type_qualifiers();
}

Node *
append_node(Node *head, Node *node)
{
	if (!head)
		return node;

	Node *tail = head;
	while (tail->next)
		tail = tail->next;
	tail->next = node;
	return head;
}

static int
align_to(int value, int align)
{
	if (align <= 1)
		return value;
	return (value + align - 1) & ~(align - 1);
}

int 
aggregate_align(StructDef *def)
{
	return def && def->align > 0 ? def->align : 4;
}

Node *
expr_value_statement(Node *setup, Node *stmt)
{
	if (!setup)
		return stmt;

	return new_block(append_node(setup, stmt));
}

int 
field_index_by_name_offset(StructDef *def, Field *field)
{
	if (!def || !field)
		return -1;

	for (int i = 0; i < def->field_count ; i++) {
		if (STRCMP(def->fields[i].name, field->name) == 0 &&
		        def->fields[i].offset == field->offset)
			return i;
	}

	return -1;
}

Node *
parse_local_scalar_initializer_expr(int target_size)
{
	int brace_depth = 0;
	Node *expr;

	while (lexer_peek()->kind == TOK_LBRACE) {
		brace_depth++;
		lexer_next();
	}

	/*
	 * Local aggregate initializers need the same function-designator handling
	 * as ordinary pointer assignment.  Without this, a field such as
	 *
	 *     struct Wrap { void *func; };
	 *     struct Wrap local_wrap[] = { inc_global };
	 *
	 * can parse the identifier through the scalar expression path and store
	 * zero instead of the address of inc_global.  That later turns p() into
	 * a call through NULL.
	 */
	if (target_size == 8) {
		if (lexer_peek()->kind == TOK_AMP &&
		    lexer_peek_ahead(1)->kind == TOK_IDENT &&
		    find_func(lexer_peek_ahead(1)->text)) {
			lexer_next(); /* & */
			const Token *fn = lexer_peek();
			lexer_next();
			expr = parser_make_function_designator(fn->text);
			goto done;
		}

		if (lexer_peek()->kind == TOK_IDENT &&
		    find_func(lexer_peek()->text) &&
		    lexer_peek_ahead(1)->kind != TOK_LPAREN) {
			const Token *fn = lexer_peek();
			lexer_next();
			expr = parser_make_function_designator(fn->text);
			goto done;
		}
	}

	expr = parse_expr();

done:
	while (brace_depth-- > 0) {
		if (lexer_peek()->kind == TOK_COMMA) {
			lexer_next();
			if (lexer_peek()->kind != TOK_RBRACE)
				fatal_cur("Too many initializers for local scalar\n");
		}
		expect(TOK_RBRACE);
	}

	return expr;
}

static void
validate_local_aggregate_pointer_initializer(Type *dst_type, Node *expr)
{
	if (!dst_type || !type_is_pointer(dst_type) || !expr)
		return;
	if (!node_is_null_pointer_constant(expr) &&
	    expr->type && type_is_integer(expr->type))
		fatal_cur("Incompatible integer to pointer conversion in initializer\n");
	if (!node_is_null_pointer_constant(expr) &&
	    (!expr->type || !type_is_pointer(expr->type)))
		return;

	if (type_pointer_assignment_compatible(dst_type,
	                                       expr->type,
	                                       node_is_null_pointer_constant(expr)))
		return;
	fatal_cur("Incompatible pointer types in initializer\n");
}

static Type *
struct_array_elem_type(Field *field)
{
	if (!field || !field->type || !type_is_array(field->type) || !field->type->base)
		return type_for_size(field ? field->elem_size : 4);
	return parser_canonicalize_decl_type(field->type->base);
}

static Node *append_struct_zero_fill(Node *head, StructDef *def, const char *struct_name, int base_offset, int *seen);

static Node *
node_list_tail(Node *node)
{
	if (!node)
		return NULL;
	while (node->next)
		node = node->next;
	return node;
}

static void
append_node_to_tail(Node **head, Node **tail, Node *node)
{
	Node *node_tail;

	if (!node)
		return;

	node_tail = node_list_tail(node);
	if (!*head)
		*head = node;
	else
		(*tail)->next = node;
	*tail = node_tail;
}

static Node *
make_struct_field_member_node(Field *field, int base_offset)
{
	Node *member = new_member(field->name, base_offset + field->offset);
	apply_field_type(member, field);
	return member;
}

static Node *
make_struct_array_elem_member_node(Field *field, int arr_offset, int elem_index)
{
	Node *elem = new_member(field->name, arr_offset + elem_index * field->elem_size);
	elem->elem_size = field->elem_size;
	elem->type = struct_array_elem_type(field);
	return elem;
}

static void
append_validated_local_member_assign_tail(Node **head, Node **tail, Node *member, Node *rhs)
{
	validate_local_aggregate_pointer_initializer(member->type, rhs);
	append_node_to_tail(head, tail, new_assign(member, rhs));
}

static void
append_local_array_designator_assign_tail(Node **head, Node **tail, Field *field,
	int arr_offset, int lo, int hi, Node *rhs)
{
	Type *elem_type = struct_array_elem_type(field);

	validate_local_aggregate_pointer_initializer(elem_type, rhs);
	for (int di = lo; di <= hi; di++) {
		Node *elem = make_struct_array_elem_member_node(field, arr_offset, di);
		Node *elem_rhs = (di == lo) ? rhs : clone_node_tree(rhs);
		append_node_to_tail(head, tail, new_assign(elem, elem_rhs));
	}
}

static int
parser_try_parse_struct_compound_literal_prefix(const char *struct_name,
	const char *mismatch_message, int *needs_outer_rparen)
{
	if (lexer_peek()->kind == TOK_LPAREN &&
	    (lexer_peek_ahead(1)->kind == TOK_STRUCT ||
	     lexer_peek_ahead(1)->kind == TOK_UNION) &&
	    lexer_peek_ahead(2)->kind == TOK_IDENT &&
	    lexer_peek_ahead(3)->kind == TOK_RPAREN &&
	    lexer_peek_ahead(4)->kind == TOK_LBRACE) {
		lexer_next(); /* ( */
		lexer_next(); /* struct/union */
		const Token *compound_struct_name = lexer_peek();
		lexer_next(); /* tag */
		expect(TOK_RPAREN);
		if (STRCMP(compound_struct_name->text, struct_name) != 0)
			fatal_cur(mismatch_message);
		expect(TOK_LBRACE);
		*needs_outer_rparen = 0;
		return 1;
	}

	if (lexer_peek()->kind == TOK_LPAREN &&
	    lexer_peek_ahead(1)->kind == TOK_LPAREN &&
	    (lexer_peek_ahead(2)->kind == TOK_STRUCT ||
	     lexer_peek_ahead(2)->kind == TOK_UNION) &&
	    lexer_peek_ahead(3)->kind == TOK_IDENT &&
	    lexer_peek_ahead(4)->kind == TOK_RPAREN &&
	    lexer_peek_ahead(5)->kind == TOK_LBRACE) {
		lexer_next(); /* outer ( */
		lexer_next(); /* inner ( */
		lexer_next(); /* struct/union */
		const Token *compound_struct_name = lexer_peek();
		lexer_next(); /* tag */
		expect(TOK_RPAREN);
		if (STRCMP(compound_struct_name->text, struct_name) != 0)
			fatal_cur(mismatch_message);
		expect(TOK_LBRACE);
		*needs_outer_rparen = 1;
		return 1;
	}

	return 0;
}

static Node *
make_local_struct_decl_node(const char *var_name, const char *struct_name,
	int *offset_out, int *struct_size_out)
{
	int offset = add_struct_local(var_name, struct_name);
	StructDef *def = find_struct(struct_name);
	Node *decl = new_struct_decl(var_name, offset);
	decl->type = type_struct(struct_name, def->size);
	if (offset_out)
		*offset_out = offset;
	if (struct_size_out)
		*struct_size_out = def->size;
	return decl;
}

static Node *
make_local_struct_lhs_node(const char *var_name, int offset, Type *decl_type,
	const char *struct_name, int struct_size)
{
	Node *lhs = new_var(var_name, offset);
	lhs->type = decl_type ? clone_type(decl_type) : type_struct(struct_name, struct_size);
	lhs->elem_size = struct_size;
	if (struct_name && struct_name[0])
		STRNCPY(lhs->struct_name, struct_name, sizeof(lhs->struct_name) - 1);
	return lhs;
}

static Node *
make_struct_return_call_node(const char *func_name, Node *args, FuncInfo *fi)
{
	Node *call = new_call(func_name, args);
	call->returns_struct = 1;
	call->aggregate_abi_class = fi->return_abi_class;
	call->aggregate_abi_reg_count = fi->return_abi_reg_count;
	call->struct_return_size = fi->struct_size;
	call->type = fi->return_type ? clone_type(fi->return_type)
	                             : type_struct(fi->struct_name, fi->struct_size);
	STRNCPY(call->return_struct_name, fi->struct_name, sizeof(call->return_struct_name) - 1);
	return call;
}

static Node *
append_local_struct_assign(Node *head, const char *var_name, int offset,
	Type *decl_type, const char *struct_name, int struct_size, Node *rhs)
{
	Node *lhs = make_local_struct_lhs_node(var_name, offset, decl_type,
	                                       struct_name, struct_size);
	return append_node(head, new_struct_assign(lhs, rhs, struct_size));
}

Node *
parse_struct_initializer_values(StructDef *def, const char *struct_name, int base_offset, Node *head)
{
	int field_index = 0;
	int *seen = xcalloc((size_t)(def->field_count ? def->field_count : 1), sizeof(int));
	Node *tail = node_list_tail(head);

	Debug(1, "STRUCT_INIT_ENTER struct=%s base=%d tok=%s text=%s next=%s next_text=%s\n",
	      struct_name ? struct_name : "<anon>",
	      base_offset,
	      token_debug_name(lexer_peek()->kind),
	      lexer_peek()->text ? lexer_peek()->text : "",
	      token_debug_name(lexer_peek_ahead(1)->kind),
	      lexer_peek_ahead(1)->text ? lexer_peek_ahead(1)->text : "");

	reject_empty_initializer_before_c23();

	while (lexer_peek()->kind != TOK_RBRACE) {
		Field *field = NULL;
		Debug(1, "GLOBAL_STRUCT_INIT loop tok=%d text=%s field_index=%d/%d\n",
		      lexer_peek()->kind,
		      lexer_peek()->text ? lexer_peek()->text : "",
		      field_index, def->field_count);

		if (lexer_peek()->kind == TOK_DOT) {
			reject_c89_designated_initializer();
			int designator_base = base_offset;
			const char *designator_struct_name = struct_name;
			StructDef *designator_def = def;
			int nested_designator = 0;

			lexer_next();

			const Token *field_name = lexer_peek();
			if (field_name->kind != TOK_IDENT) {
				fatal_cur("Expected field name after '.' in struct initializer\n");
			}

			lexer_next();
			field = find_field(designator_struct_name, field_name->text);

			/*
			 * Nested local designators, e.g.
			 *
			 *   struct SEB b = { .a.j = 5 };
			 *
			 * Walk through struct-typed designator components until the final
			 * selected field.  When a nested aggregate is first selected, zero-fill
			 * it before assigning the final scalar, so omitted fields keep the
			 * normal aggregate-initializer zero value.
			 */
			while (lexer_peek()->kind == TOK_DOT) {
				int parent_index = field_index_by_name_offset(designator_def, field);

				if (!field->is_struct || !field->struct_name[0])
					fatal_cur("Nested designator through non-struct field\n");

				if (designator_def == def && parent_index >= 0 && !seen[parent_index]) {
					StructDef *nested_zero_def = find_struct(field->struct_name);
					int nested_count = nested_zero_def ? nested_zero_def->field_count : 0;
					int *nested_seen = xcalloc((size_t)(nested_count ? nested_count : 1), sizeof(int));
					head = append_struct_zero_fill(head, nested_zero_def, field->struct_name,
					                               designator_base + field->offset, nested_seen);
					tail = node_list_tail(head);
					xfree(nested_seen);
					for (int si = 0; si < def->field_count ; si++)
						if (def->fields[si].offset == field->offset)
							seen[si] = 1;
				}

				designator_base += field->offset;
				designator_struct_name = field->struct_name;
				designator_def = find_struct(field->struct_name);
				nested_designator = 1;

				lexer_next(); /* . */
				field_name = lexer_peek();
				if (field_name->kind != TOK_IDENT)
					fatal_cur("Expected field name after '.' in nested struct initializer\n");
				lexer_next();
				field = find_field(designator_struct_name, field_name->text);
			}

			expect(TOK_ASSIGN);

			if (nested_designator) {
				Debug(1, "STRUCT_INIT_NESTED_DESIGNATOR struct=%s field=%s base=%d tok=%s next=%s\n",
				      designator_struct_name ? designator_struct_name : "<anon>",
				      field && field->name[0] ? field->name : "<anon>",
				      designator_base,
				      token_debug_name(lexer_peek()->kind),
				      token_debug_name(lexer_peek_ahead(1)->kind));

					if (field->is_struct || field->is_array)
						fatal_cur("Nested local designator currently requires a scalar target\n");

					Node *member = make_struct_field_member_node(field, designator_base);
					Node *rhs;
					rhs = parse_local_scalar_initializer_expr(member->elem_size);
					append_validated_local_member_assign_tail(&head, &tail, member, rhs);

					if (lexer_peek()->kind == TOK_COMMA) {
						lexer_next();
					if (lexer_peek()->kind == TOK_RBRACE)
						break;
					continue;
				}
				break;
			}

			{
				int di = field_index_by_name_offset(def, field);
				if (di >= 0 && field_index <= di) {
					field_index = di + 1;
					while (field_index < def->field_count &&
					       field->size > 0 &&
					       def->fields[field_index].size > 0 &&
					       def->fields[field_index].offset == field->offset)
						field_index++;
				}
			}
		} else {
			if (field_index >= def->field_count) {
				/* All fields consumed — stop; remaining values belong to outer initializer. */
				break;
			}

			field = &def->fields[field_index++];

			/*
			 * Anonymous unions are represented by promoted fields with the
			 * same offset. Positional initialization gets one slot for the
			 * anonymous union, not one slot per promoted member.
			 */
			while (field_index < def->field_count &&
			       field->size > 0 &&
			       def->fields[field_index].size > 0 &&
			       def->fields[field_index].offset == field->offset)
				field_index++;
		}

		{
			int idx = field_index_by_name_offset(def, field);
			if (idx >= 0) {
				seen[idx] = 1;

				/*
				 * Mark all aliases at this offset as seen so zero-fill does
				 * not wipe a value initialized through another union member.
				 */
				for (int si = 0; si < def->field_count ; si++)
					if (def->fields[si].offset == field->offset)
						seen[si] = 1;
			}
		}

		Debug(1, "STRUCT_INIT_FIELD struct=%s field=%s index=%d/%d off=%d size=%d is_struct=%d is_array=%d tok=%s text=%s next=%s next_text=%s\n",
		      struct_name ? struct_name : "<anon>",
		      field && field->name[0] ? field->name : "<anon>",
		      field_index,
		      def ? def->field_count : -1,
		      field ? field->offset : -1,
		      field ? field->size : -1,
		      field ? field->is_struct : -1,
		      field ? field->is_array : -1,
		      token_debug_name(lexer_peek()->kind),
		      lexer_peek()->text ? lexer_peek()->text : "",
		      token_debug_name(lexer_peek_ahead(1)->kind),
		      lexer_peek_ahead(1)->text ? lexer_peek_ahead(1)->text : "");

		if (field->is_struct) {
			if (lexer_peek()->kind == TOK_LBRACE) {
				Debug(1, "STRUCT_INIT_BRACED_STRUCT_ENTER parent=%s field=%s nested=%s field_index=%d/%d tok=%s next=%s\n",
				      struct_name ? struct_name : "<anon>",
				      field->name[0] ? field->name : "<anon>",
				      field->struct_name[0] ? field->struct_name : "<anon>",
				      field_index, def ? def->field_count : -1,
				      token_debug_name(lexer_peek()->kind),
				      token_debug_name(lexer_peek_ahead(1)->kind));
				/* Braced sub-struct: "{val, val, ...}" */
				lexer_next();
				StructDef *nested = find_struct(field->struct_name);
				head = parse_struct_initializer_values(nested, field->struct_name, base_offset + field->offset, head);
				tail = node_list_tail(head);
				Debug(1, "STRUCT_INIT_BRACED_STRUCT_BEFORE_RBRACE parent=%s field=%s nested=%s tok=%s text=%s next=%s next_text=%s\n",
				      struct_name ? struct_name : "<anon>",
				      field->name[0] ? field->name : "<anon>",
				      field->struct_name[0] ? field->struct_name : "<anon>",
				      token_debug_name(lexer_peek()->kind),
				      lexer_peek()->text ? lexer_peek()->text : "",
				      token_debug_name(lexer_peek_ahead(1)->kind),
				      lexer_peek_ahead(1)->text ? lexer_peek_ahead(1)->text : "");
				parser_expect_local_aggregate_initializer_close(nested);

				/*
				 * We have just finished a braced nested struct field.
				 * If the parent struct still has fields remaining, consume
				 * the comma that separates this field from the next parent
				 * field and continue the parent initializer loop here.
				 *
				 * This handles brace elision such as:
				 *
				 *   struct V v = {{3,4,{5,6}}, "haha", (u8)45, 46};
				 *                            ^
				 *
				 * The comma after the nested S initializer belongs to V,
				 * not to the enclosing declaration parser.
				 */
				if (lexer_peek()->kind == TOK_COMMA &&
				    field_index < def->field_count &&
				    lexer_peek_ahead(1)->kind != TOK_RBRACE) {
					Debug(1, "STRUCT_INIT_BRACED_STRUCT_CONSUME_COMMA parent=%s field=%s next_field_index=%d/%d next=%s next_text=%s\n",
					      struct_name ? struct_name : "<anon>",
					      field->name[0] ? field->name : "<anon>",
					      field_index, def ? def->field_count : -1,
					      token_debug_name(lexer_peek_ahead(1)->kind),
					      lexer_peek_ahead(1)->text ? lexer_peek_ahead(1)->text : "");
					lexer_next();
					continue;
				}
			} else {
				/*
				 * Brace elision applies recursively for aggregate members.
				 *
				 *   struct V { struct S s; struct T t; unsigned char a; };
				 *   struct V v = {{3,4,{5,6}}, "haha", (u8)45, 46};
				 *
				 * After the braced S initializer, the string starts the
				 * unbraced initializer for T, not a scalar initializer for
				 * the whole T object.  Still preserve the existing extension
				 * that lets a struct member be copied from a struct object.
				 */
				int copy_struct_object = 0;
				if (lexer_peek()->kind == TOK_IDENT &&
				    (is_struct_local(lexer_peek()->text) ||
				     is_global_struct(lexer_peek()->text)))
					copy_struct_object = 1;

				/*
				 * Whole-struct expressions must initialize the whole field, not
				 * recurse into the nested struct's scalar members.  This covers:
				 *
				 *   (struct S)w->t.s
				 *   ((struct S){7,8,{9,10}})
				 *   ((const struct W *)w)->t.t
				 *   (ls)
				 *   *pls
				 *
				 * But keep scalar-looking parenthesized values such as (1) as
				 * brace-elided subaggregate initializers.
				 */
				if (lexer_peek()->kind == TOK_STAR) {
					copy_struct_object = 1;
				}

				if (lexer_peek()->kind == TOK_LPAREN) {
					/*
					 * A parenthesized value in a struct-typed field is a whole-object
					 * initializer, not brace elision into the nested struct.  This is
					 * deliberately broad so it catches extra-parenthesized compound
					 * literals and casts, e.g.
					 *
					 *   ((struct S){7,8,{9,10}})
					 *   ((const struct W *)w)->t.t
					 *   (ls)
					 */
					copy_struct_object = 1;
				}

				/*
				 * Whole-struct compound literal initializer for a struct field.
				 *
				 * parse_expr() does not materialize a whole struct compound literal
				 * such as:
				 *
				 *   ((struct S){7,8,{9,10}})
				 *
				 * so parse it directly into this field's storage.
				 */
				if (lexer_peek()->kind == TOK_IDENT &&
				    lexer_peek_ahead(1)->kind == TOK_ARROW &&
				    lexer_peek_ahead(2)->kind == TOK_IDENT) {
					/*
					 * Struct-typed field initialized from a pointer member, e.g.
					 *
					 *   struct flowi6 flow = { .daddr = phdr->daddr };
					 *
					 * Do not parse phdr->daddr as a scalar value.  It is an
					 * aggregate object, so copy its scalar/array fields from the
					 * source pointer field into the destination aggregate.
					 */
					const Token *src_name_tok = lexer_peek();
					const Token *src_field_tok = lexer_peek_ahead(2);
					Node *src_base = make_scalar_var_node(src_name_tok->text);
					const char *src_struct_name = "";

					if (src_base->type && type_is_pointer(src_base->type) &&
					    type_pointee(src_base->type) && type_is_struct(type_pointee(src_base->type)))
						src_struct_name = type_pointee(src_base->type)->struct_name;
					else if (src_base->struct_name[0])
						src_struct_name = src_base->struct_name;

					if (!src_struct_name[0])
						fatal_cur("Struct pointer member initializer needs pointer-to-struct source\n");

					Field *src_field = find_field(src_struct_name, src_field_tok->text);
					if (!src_field->is_struct || STRCMP(src_field->struct_name, field->struct_name) != 0)
						fatal_cur("Struct pointer member initializer type mismatch\n");

					lexer_next(); /* source pointer name */
					lexer_next(); /* -> */
					lexer_next(); /* source field name */

					head = append_struct_copy_from_ptr_fields_at(head,
					        base_offset + field->offset, src_base, field->struct_name,
					        src_field->offset);
					tail = node_list_tail(head);
				} else {
					int needs_outer_rparen = 0;
					if (parser_try_parse_struct_compound_literal_prefix(
					        field->struct_name,
					        "Struct compound literal initializer type mismatch\n",
					        &needs_outer_rparen)) {
						StructDef *nested = find_struct(field->struct_name);
						head = parse_struct_initializer_values(nested, field->struct_name,
						                                       base_offset + field->offset, head);
						tail = node_list_tail(head);
						parser_expect_local_aggregate_initializer_close(nested);
						if (needs_outer_rparen)
							expect(TOK_RPAREN); /* outer ) */
					} else if (copy_struct_object) {
						Node *member = new_member(field->name, base_offset + field->offset);
						member->elem_size = field->size;
						member->type = type_for_size(field->size);
						STRNCPY(member->struct_name, struct_name, sizeof(member->struct_name) - 1);
						append_node_to_tail(&head, &tail,
						                    new_assign(member,
						                               parse_local_scalar_initializer_expr(member->elem_size)));
					} else {
						StructDef *nested = find_struct(field->struct_name);
						head = parse_struct_initializer_values(nested, field->struct_name,
						                                       base_offset + field->offset, head);
						tail = node_list_tail(head);

						/*
						 * Brace-elided nested struct initializer.
						 *
						 * When parsing the parent initializer below, the outer
						 * declaration wrapper has already consumed the first '{',
						 * so the nested field starts at the first scalar token:
						 *
						 *   struct V v = {{3,4,{5,6}}, "haha", (u8)45, 46};
						 *                  ^
						 *
						 * The recursive parse of V.s stops at the '}' that closes
						 * that nested S initializer.  If the parent still has
						 * fields left and that brace is followed by a comma, consume
						 * both and continue with the next parent field.
						 */
						if (lexer_peek()->kind == TOK_RBRACE &&
						    lexer_peek_ahead(1)->kind == TOK_COMMA &&
						    field_index < def->field_count) {
							Debug(1, "STRUCT_INIT_UNBRACED_STRUCT_CONSUME_RBRACE_COMMA parent=%s field=%s next_field_index=%d/%d next=%s next_text=%s\n",
							      struct_name ? struct_name : "<anon>",
							      field->name[0] ? field->name : "<anon>",
							      field_index, def ? def->field_count : -1,
							      token_debug_name(lexer_peek_ahead(1)->kind),
							      lexer_peek_ahead(1)->text ? lexer_peek_ahead(1)->text : "");
							lexer_next(); /* consume nested struct's } */
							lexer_next(); /* consume parent field separator comma */
							continue;
						}
					}
				}
			}
		} else if (field->is_array) {
			/* Array field: can be braced "{v0, v1}" or flat "v0, v1, ..." */
			int arr_max = (field->elem_size > 0) ? field->size / field->elem_size : 1;
			int arr_offset = base_offset + field->offset;
			int ae = 0;

			/*
			 * The local struct initializer wrapper sometimes consumes the
			 * opening brace of an array member before we get here, so a
			 * braced designated array can arrive looking like:
			 *
			 *   [1 ... 5] = 9, [6 ... 10] = elt, ... }
			 *
			 * rather than:
			 *
			 *   { [1 ... 5] = 9, [6 ... 10] = elt, ... }
			 *
			 * Handle that form directly and consume the closing brace of the
			 * array member so the parent struct sees the following comma.
			 */
			if (lexer_peek()->kind == TOK_LBRACKET) {
				while (lexer_peek()->kind == TOK_LBRACKET) {
					int lo = 0;
					int hi = 0;
					parser_try_parse_local_array_designator(&lo, &hi);

					if (lo < 0 || hi >= arr_max)
						fatal_cur("Array designator index out of range\n");
					if (hi < lo)
						fatal_cur("Invalid array designator range\n");

					Node *rhs = parse_expr();
					append_local_array_designator_assign_tail(&head, &tail, field, arr_offset, lo, hi, rhs);

					ae = hi + 1;
					if (lexer_peek()->kind == TOK_COMMA) {
						lexer_next();
						if (lexer_peek()->kind == TOK_RBRACE)
							break;
					}
				}
				expect(TOK_RBRACE);
			} else if (lexer_peek()->kind == TOK_LBRACE) {
				/* Braced array initializer: { v0, v1, ... } */
				lexer_next();
				while (lexer_peek()->kind != TOK_RBRACE && ae < arr_max) {
					/* GNU/C99 array designator: [idx] = value or [lo ... hi] = value */
					if (lexer_peek()->kind == TOK_LBRACKET) {
						int lo = 0;
						int hi = 0;
						parser_try_parse_local_array_designator(&lo, &hi);

						if (lo < 0 || hi >= arr_max)
							fatal_cur("Array designator index out of range\n");
						if (hi < lo)
							fatal_cur("Invalid array designator range\n");

						Node *rhs = parse_expr();
						append_local_array_designator_assign_tail(&head, &tail, field, arr_offset, lo, hi, rhs);

						ae = hi + 1;
						if (lexer_peek()->kind == TOK_COMMA)
							lexer_next();
						continue;
					}

						/* strip optional braces around element */
						int brace = (lexer_peek()->kind == TOK_LBRACE);
						if (brace) lexer_next();
						Node *elem = make_struct_array_elem_member_node(field, arr_offset, ae);
						Node *rhs;
						rhs = parse_local_scalar_initializer_expr(elem->elem_size);
						append_validated_local_member_assign_tail(&head, &tail, elem, rhs);
						if (brace) expect(TOK_RBRACE);
						ae++;
						if (lexer_peek()->kind == TOK_COMMA) lexer_next();
				}
				expect(TOK_RBRACE);
			} else if (field->elem_size == 1 && lexer_peek()->kind == TOK_STRING) {
				/* String literal initializes char array */
				const Token *str = lexer_peek();
				lexer_next();
				const char *s = str->text ? str->text : "";
				for (int ci = 0; ci < arr_max && s[ci]; ci++) {
					Node *elem = make_struct_array_elem_member_node(field, arr_offset, ci);
					append_node_to_tail(&head, &tail, new_assign(elem, new_num(s[ci])));
				}
			} else {
				/* Flat: consume up to arr_max values from the outer initializer list */
					while (ae < arr_max && lexer_peek()->kind != TOK_RBRACE) {
						int brace = (lexer_peek()->kind == TOK_LBRACE);
						if (brace) lexer_next();
						Node *elem = make_struct_array_elem_member_node(field, arr_offset, ae);
						Node *rhs;
						rhs = parse_local_scalar_initializer_expr(elem->elem_size);
						append_validated_local_member_assign_tail(&head, &tail, elem, rhs);
						if (brace) expect(TOK_RBRACE);
						ae++;
						if (ae < arr_max && lexer_peek()->kind == TOK_COMMA) {
						/* peek: if next value would start a new top-level field, stop */
						/* (we always consume the comma and let the outer loop re-check) */
						/* For flat arrays, keep consuming */
						const Token *nxt = lexer_peek_ahead(1);
						if (nxt->kind == TOK_RBRACE) {
Debug(1, "AFTER_INIT_RBRACE struct=%s tok=%s text=%s next=%s next_text=%s\n",
      struct_name ? struct_name : "<anon>",
      token_debug_name(lexer_peek()->kind),
      lexer_peek()->text ? lexer_peek()->text : "",
      token_debug_name(lexer_peek_ahead(1)->kind),
      lexer_peek_ahead(1)->text ? lexer_peek_ahead(1)->text : "");
							break;
						}
						lexer_next(); /* consume comma */
					}
				}
			}
			} else {
				Node *member = make_struct_field_member_node(field, base_offset);
				Node *rhs;

				/* Strip optional parentheses around scalar: { (1) } */
				rhs = parse_local_scalar_initializer_expr(member->elem_size);
				append_validated_local_member_assign_tail(&head, &tail, member, rhs);
			}

		/*
		 * Consume commas between initializer clauses.  A designated
		 * initializer may name an earlier field even after field_index has
		 * advanced to the end, e.g. { .y = 41, .x = 1 }.  Do not treat
		 * field_index >= field_count as meaning the local initializer is
		 * complete when the next clause is another designator.
		 */
		if (lexer_peek()->kind == TOK_COMMA) {
			Debug(1, "STRUCT_INIT_COMMA struct=%s field=%s field_index=%d/%d tok=%s next=%s next_text=%s\n",
			      struct_name ? struct_name : "<anon>",
			      field && field->name[0] ? field->name : "<anon>",
			      field_index, def ? def->field_count : -1,
			      token_debug_name(lexer_peek()->kind),
			      token_debug_name(lexer_peek_ahead(1)->kind),
			      lexer_peek_ahead(1)->text ? lexer_peek_ahead(1)->text : "");
			/*
			 * If this recursive initializer has consumed all of its fields,
			 * leave the separator comma for the caller.  This is needed for
			 * brace-elided nested aggregates such as:
			 *
			 *   struct V v = {{3,4,{5,6}}, "haha", (u8)45, 46};
			 *
			 * where "haha", (u8)45 initializes a nested struct T and the
			 * comma before 46 belongs to the outer struct V initializer.
			 * Still consume a true trailing comma before the closing brace,
			 * and still allow late designators such as { .y = 41, .x = 1 }.
			 */
			if (field_index >= def->field_count &&
			    lexer_peek_ahead(1)->kind != TOK_RBRACE &&
			    lexer_peek_ahead(1)->kind != TOK_DOT &&
			    lexer_peek_ahead(1)->kind != TOK_LBRACKET)
				break;

			lexer_next();
			if (lexer_peek()->kind == TOK_RBRACE) {
				Debug(1, "AFTER_INIT_RBRACE struct=%s tok=%s text=%s next=%s next_text=%s\n",
      					struct_name ? struct_name : "<anon>",
      					token_debug_name(lexer_peek()->kind),
      					lexer_peek()->text ? lexer_peek()->text : "",
      					token_debug_name(lexer_peek_ahead(1)->kind),
      					lexer_peek_ahead(1)->text ? lexer_peek_ahead(1)->text : "");
				break;
			}
			continue;
		}

		if (field_index >= def->field_count)
			break;

		break;
	}

	Debug(1, "STRUCT_INIT_LEAVE struct=%s field_index=%d/%d tok=%s text=%s next=%s next_text=%s\n",
	      struct_name ? struct_name : "<anon>",
	      field_index, def ? def->field_count : -1,
	      token_debug_name(lexer_peek()->kind),
	      lexer_peek()->text ? lexer_peek()->text : "",
	      token_debug_name(lexer_peek_ahead(1)->kind),
	      lexer_peek_ahead(1)->text ? lexer_peek_ahead(1)->text : "");
	head = append_struct_zero_fill(head, def, struct_name, base_offset, seen);
	tail = node_list_tail(head);
	xfree(seen);
	return head;
}

void
parser_expect_local_aggregate_initializer_close(StructDef *def)
{
	if (lexer_peek()->kind == TOK_COMMA)
		fatal_cur("Too many initializers for local %s\n",
		          def && def->is_union ? "union" : "struct");
	expect(TOK_RBRACE);
}

static Node *
append_struct_zero_fill(Node *head, StructDef *def, const char *struct_name, int base_offset, int *seen)
{
	Node *tail = node_list_tail(head);

	if (!def)
		return head;

	for (int i = 0; i < def->field_count ; i++) {
		if (seen[i])
			continue;

		Field *field = &def->fields[i];

		if (field->is_struct) {
			StructDef *nested = find_struct(field->struct_name);
			int *nested_seen = xcalloc((size_t)(nested && nested->field_count ? nested->field_count : 1), sizeof(int));
			head = append_struct_zero_fill(head, nested, field->struct_name, base_offset + field->offset, nested_seen);
			tail = node_list_tail(head);
			xfree(nested_seen);
			continue;
		}

		Node *member = new_member(field->name, base_offset + field->offset);
		member->elem_size = field->size;
		member->type = type_for_size(field->size);
		STRNCPY(member->struct_name, struct_name, sizeof(member->struct_name) - 1);

		append_node_to_tail(&head, &tail, new_assign(member, new_num(0)));
	}

	return head;
}

Node *
parse_struct_array_initializer_block(const char *var_name, const char *struct_name, int base_offset,
        int array_len,
        Node *decl_node)
{
	expect(TOK_ASSIGN);
	expect(TOK_LBRACE);

	StructDef *def = find_struct(struct_name);
	Node *head = decl_node;
	int index = 0;
	int infer_len = decl_node && decl_node->array_len < 0;
	int max_used = 0;

	while (lexer_peek()->kind != TOK_RBRACE) {
		if (lexer_peek()->kind == TOK_LBRACKET) {
			reject_c89_designated_initializer();
			lexer_next();

			const Token *idx_tok = lexer_peek();
			if (idx_tok->kind != TOK_NUM) {
				fatal_cur("Expected numeric designated struct array index\n");
			}

			index = idx_tok->value;
			lexer_next();
			expect(TOK_RBRACKET);
			expect(TOK_ASSIGN);
		}

		if (index >= array_len) {
			fatal_cur("Struct array initializer index out of range\n");
		}

		Debug(1, "LOCAL_STRUCT_ARRAY_ELEM name=%s struct=%s index=%d tok=%s next=%s\n",
		      var_name, struct_name ? struct_name : "<anon>", index,
		      token_debug_name(lexer_peek()->kind),
		      token_debug_name(lexer_peek_ahead(1)->kind));

		int elem_offset = base_offset + index * def->size;

		/* Automatic aggregate initialization starts from an all-zero object.
		 * The per-field assignments below then overwrite the explicitly
		 * initialized members.  This is required not only for omitted fields,
		 * but also for padding bytes and array tails, which torture/00216
		 * prints byte-for-byte. */
		head = append_local_zero_fill(head, var_name, elem_offset, def->size);

		if (lexer_peek()->kind == TOK_LBRACE) {
			lexer_next();
			head = parse_struct_initializer_values(def, struct_name,
			                                       elem_offset,
			                                       head);
			parser_expect_local_aggregate_initializer_close(def);
		} else {
			int needs_outer_rparen = 0;
			if (parser_try_parse_struct_compound_literal_prefix(
			        struct_name,
			        "Struct array compound literal initializer type mismatch\n",
			        &needs_outer_rparen)) {
				head = parse_struct_initializer_values(def, struct_name, elem_offset, head);
				parser_expect_local_aggregate_initializer_close(def);
				if (needs_outer_rparen)
					expect(TOK_RPAREN); /* outer ) */
			} else if (lexer_peek()->kind == TOK_LPAREN ||
		           lexer_peek()->kind == TOK_STAR ||
		           (lexer_peek()->kind == TOK_IDENT &&
		            (is_struct_local(lexer_peek()->text) ||
		             is_global_struct(lexer_peek()->text)))) {
				/* Whole struct expression assigned to this array element. */
				Node *rhs = parse_expr();
				head = append_local_struct_assign(head, var_name, elem_offset, NULL,
				                                  struct_name, def->size, rhs);
			} else {
				/* Brace-elided struct-array element, e.g. { inc_global, ... }. */
				head = parse_struct_initializer_values(def, struct_name, elem_offset, head);
			}
		}

		if (index + 1 > max_used)
			max_used = index + 1;

		index++;

		if (lexer_peek()->kind == TOK_COMMA) {
			lexer_next();
			if (lexer_peek()->kind == TOK_RBRACE)
				break;
		} else {
			break;
		}
	}

	expect(TOK_RBRACE);
	if (lexer_peek()->kind == TOK_COMMA) {
		Type *base_type = decl_node && decl_node->type
		                ? type_pointee(decl_node->type)
		                : NULL;
		head = parser_append_local_struct_comma_declarators(head, struct_name,
		                                                    base_type);
	}
	expect(TOK_SEMI);

	if (infer_len) {
		if (max_used <= 0)
			max_used = 1;
		decl_node->array_len = max_used;
		if (decl_node->type)
			decl_node->type->array_len = max_used;
		for (int i = 0; i < pscope.local_count; i++) {
			if (STRCMP(pscope.locals[i].name, var_name) == 0) {
				pscope.locals[i].array_len = max_used;
				if (pscope.locals[i].type)
					pscope.locals[i].type->array_len = max_used;
				break;
			}
		}
	}

	(void)var_name;
	return new_block(head);
}

Node *
parse_struct_initializer_block(const char *var_name, const char *struct_name, int base_offset, Node *decl_node)
{
	const char *resolved_struct_name = struct_name;

	if ((!resolved_struct_name || !resolved_struct_name[0]) && decl_node && decl_node->type)
		resolved_struct_name = parser_resolve_struct_type_name(decl_node->type);

	if (lexer_peek()->kind != TOK_ASSIGN) {
		/* Handle comma-separated struct declarations: DateTime d1, d2; */
		if (lexer_peek()->kind == TOK_COMMA) {
			Node *block_head = parser_append_local_struct_comma_declarators(
			    decl_node, resolved_struct_name, decl_node ? decl_node->type : NULL);
			expect(TOK_SEMI);
			return new_block(block_head);
		}
		expect(TOK_SEMI);
		return decl_node;
	}

	lexer_next();

	{
		StructDef *def = find_struct(resolved_struct_name);
		if (def && def->has_flexible_array_member && tcc_iso_diagnostics)
			fatal_cur("initializer for struct with flexible array member is not supported\n");
	}

			if (lexer_peek()->kind == TOK_IDENT &&
			        lexer_peek_ahead(1)->kind == TOK_LPAREN) {
		const Token *func = lexer_peek();
		FuncInfo *fi = find_func(func->text);
		Type *decl_type = decl_node ? decl_node->type : NULL;
		int decl_size = decl_type ? type_sizeof(decl_type) : 0;

		lexer_next(); /* consume func name */
		expect(TOK_LPAREN);
		Node *args = parse_arg_list(fi);
		expect(TOK_RPAREN);
		expect(TOK_SEMI);

		Node *lhs = make_local_struct_lhs_node(var_name, base_offset, decl_type,
		                                       resolved_struct_name, decl_size);

		Node *call;
		if (fi && fi->returns_struct) {
			call = make_struct_return_call_node(func->text, args, fi);
		} else {
			/*
			 * Bootstrap fallback: when the destination type is a known struct,
			 * allow "StructType local = helper(...)" even if helper's
			 * prototype/definition has not been recorded yet.  Treat the call
			 * as returning the destination struct type.
			 */
			call = new_call(func->text, args);
			call->returns_struct = 1;
			call->struct_return_size = decl_size;
			call->type = decl_type ? clone_type(decl_type)
			                       : type_struct(resolved_struct_name, decl_size);
			call->aggregate_abi_class = AGGREGATE_ABI_BYREF;
			if (resolved_struct_name && resolved_struct_name[0])
				STRNCPY(call->return_struct_name, resolved_struct_name,
				        sizeof(call->return_struct_name) - 1);
		}

			Node *assign = new_assign(lhs, call);
			return new_block(append_node(decl_node, assign));
		}

		{
			int needs_outer_rparen = 0;
			if (parser_try_parse_struct_compound_literal_prefix(
			        resolved_struct_name,
			        "Struct compound literal initializer type mismatch\n",
			        &needs_outer_rparen)) {
				StructDef *def = find_struct(resolved_struct_name);
				Node *head = append_local_zero_fill(decl_node, var_name,
				                                    base_offset, def->size);
				head = parse_struct_initializer_values(def, resolved_struct_name,
				                                       base_offset, head);
				parser_expect_local_aggregate_initializer_close(def);
				if (needs_outer_rparen)
					expect(TOK_RPAREN);
				expect(TOK_SEMI);
				return new_block(head);
			}
		}

		if (lexer_peek()->kind != TOK_LBRACE) {
		/* Try: struct Foo dst = src_var  (struct copy from variable) */
		if (lexer_peek()->kind == TOK_IDENT) {
			const Token *src = lexer_peek();
			/* check it's a local or global struct variable, not a function call */
				if (lexer_peek_ahead(1)->kind == TOK_SEMI || lexer_peek_ahead(1)->kind == TOK_COMMA) {
					lexer_next(); /* consume src name */
					expect(TOK_SEMI);

					Type *decl_type = decl_node ? decl_node->type : NULL;
					int decl_size = decl_type ? type_sizeof(decl_type) : 0;

					Node *rhs = make_scalar_var_node(src->text);
					if (!rhs->type || !type_is_struct(rhs->type)) {
						rhs->type = decl_type ? clone_type(decl_type)
						                     : type_struct(resolved_struct_name, decl_size);
						rhs->elem_size = decl_size;
						if (resolved_struct_name && resolved_struct_name[0])
							STRNCPY(rhs->struct_name, resolved_struct_name, sizeof(rhs->struct_name) - 1);
					}

					return new_block(append_local_struct_assign(decl_node, var_name, base_offset,
					                                           decl_type, resolved_struct_name,
					                                           decl_size, rhs));
				}
		}

		/* Try: struct Foo dst = *ptr;  (copy from pointed-to struct object) */
		if (lexer_peek()->kind == TOK_STAR && lexer_peek_ahead(1)->kind == TOK_IDENT) {
			lexer_next(); /* * */
			const Token *src = lexer_peek();
			lexer_next();
			expect(TOK_SEMI);

				Node *src_base = make_scalar_var_node(src->text);
				if (!src_base->type || !type_is_pointer(src_base->type) ||
				    !type_pointee(src_base->type) || !type_is_struct(type_pointee(src_base->type))) {
					fatal_cur("Struct initializer dereference must use pointer-to-struct source\n");
				}
				if (!decl_node || !decl_node->type ||
				    !type_equal_unqualified(type_pointee(src_base->type), decl_node->type)) {
					fatal_cur("Struct initializer type mismatch\n");
				}

				src_base->is_pointer = 1;
				src_base->elem_size = TCC_SIZEOF_PTR;
				if (resolved_struct_name && resolved_struct_name[0])
					STRNCPY(src_base->struct_name, resolved_struct_name, sizeof(src_base->struct_name) - 1);

				return new_block(append_struct_copy_from_ptr_fields(decl_node, var_name, base_offset,
				                 src_base, resolved_struct_name, 0));
		}

		/*
		 * General struct-valued initializer expression.  This is needed for
		 * stdarg.h's va_arg macro when TYPE is a struct: after preprocessing it
		 * typically has the shape
		 *
		 *     struct S x = *(struct S *)((ap) += 8, (ap) - 8);
		 *
		 * The earlier special cases handle simple identifiers, calls, and
		 * plain *ptr.  Let the normal expression parser handle casts, comma
		 * expressions, and pointer arithmetic, then copy the resulting struct
		 * value into the newly declared object.
		 */
		{
			Type *decl_type = decl_node ? decl_node->type : NULL;
			int decl_size = decl_type ? type_sizeof(decl_type) : 0;
			Node *lhs = make_local_struct_lhs_node(var_name, base_offset, decl_type,
			                                       resolved_struct_name, decl_size);

			Node *rhs = parse_expr();
			expect(TOK_SEMI);

			if (!rhs->type || !type_is_struct(rhs->type) ||
			    !type_equal_unqualified(lhs->type, rhs->type)) {
				fatal_cur("Struct initializer expression type mismatch\n");
			}

			return new_block(append_local_struct_assign(decl_node, var_name, base_offset,
			                                           decl_type, resolved_struct_name,
			                                           decl_size, rhs));
		}

	}

	lexer_next();
	if (lexer_peek()->kind == TOK_NUM &&
	    lexer_peek()->long_value == 0 &&
	    lexer_peek_ahead(1)->kind == TOK_RBRACE) {
		lexer_next();
		expect(TOK_RBRACE);
		expect(TOK_SEMI);
		return build_local_zero_fill_block(var_name, base_offset,
		                                   decl_node && decl_node->type
		                                       ? type_sizeof(decl_node->type)
		                                       : 0,
		                                   decl_node);
	}

	if (consume_all_zero_initializer()) {
		expect(TOK_SEMI);
		return build_local_zero_fill_block(var_name, base_offset,
		                                   decl_node && decl_node->type
		                                       ? type_sizeof(decl_node->type)
		                                       : 0,
		                                   decl_node);
	}

		StructDef *def = find_struct(resolved_struct_name);
		Node *head = append_local_zero_fill(decl_node, var_name, base_offset, def->size);
		head = parse_struct_initializer_values(def, resolved_struct_name, base_offset, head);

	Debug(1, "STRUCT_INIT_BLOCK_BEFORE_CLOSE name=%s struct=%s tok=%s text=%s next=%s next_text=%s\n",
	      var_name ? var_name : "<anon>",
	      struct_name ? struct_name : "<anon>",
	      token_debug_name(lexer_peek()->kind),
	      lexer_peek()->text ? lexer_peek()->text : "",
	      token_debug_name(lexer_peek_ahead(1)->kind),
	      lexer_peek_ahead(1)->text ? lexer_peek_ahead(1)->text : "");
	parser_expect_local_aggregate_initializer_close(def);
	Debug(1, "STRUCT_INIT_BLOCK_BEFORE_SEMI name=%s struct=%s tok=%s text=%s next=%s next_text=%s\n",
	      var_name ? var_name : "<anon>",
	      struct_name ? struct_name : "<anon>",
	      token_debug_name(lexer_peek()->kind),
	      lexer_peek()->text ? lexer_peek()->text : "",
	      token_debug_name(lexer_peek_ahead(1)->kind),
	      lexer_peek_ahead(1)->text ? lexer_peek_ahead(1)->text : "");
	expect(TOK_SEMI);

	return new_block(head);
}

Node *
parse_struct_compound_assignment_statement(void)
{
	const Token *var = lexer_peek();

	if (var->kind != TOK_IDENT ||
	        lexer_peek_ahead(1)->kind != TOK_ASSIGN ||
	        lexer_peek_ahead(2)->kind != TOK_LPAREN ||
	        lexer_peek_ahead(3)->kind != TOK_STRUCT)
		return NULL;

	if (!is_struct_local(var->text))
		return NULL;

	if (tcc_lang_is_c89_or_c90())
		fatal_cur("compound literals are not allowed in C89/C90 mode\n");

	lexer_next(); /* var */
	lexer_next(); /* = */
	lexer_next(); /* ( */
	lexer_next(); /* struct */

	const Token *struct_name = lexer_peek();
	if (struct_name->kind != TOK_IDENT) {
		fatal_cur("Expected struct name in compound literal\n");
	}
	lexer_next();

	expect(TOK_RPAREN);

	if (lexer_peek()->kind != TOK_LBRACE) {
		fatal_cur("Expected initializer list in compound literal\n");
	}
	lexer_next();

	const char *lhs_struct = struct_name_local(var->text);
	if (STRCMP(lhs_struct, struct_name->text) != 0) {
		fatal_cur("Compound literal assignment type mismatch\n");
	}

	StructDef *def = find_struct(struct_name->text);
	Node *head = parse_struct_initializer_values(def, struct_name->text, find_local(var->text), NULL);

	parser_expect_local_aggregate_initializer_close(def);
	expect(TOK_SEMI);

	return new_block(head);
}

Type *
type_for_size(int size)
{
	if (size == 1)
		return type_char();
	if (size == 2)
		return type_short();
	if (size == 8)
		return type_long();
	return type_int();
}

Type *
type_for_size_unsigned(int size, int is_unsigned)
{
	if (size == 1)
		return is_unsigned ? type_uchar() : type_char();
	if (size == 2)
		return is_unsigned ? type_ushort() : type_short();
	if (size == 8)
		return is_unsigned ? type_ulong() : type_long();
	return is_unsigned ? type_uint() : type_int();
}

void 
expect(TokenKind kind)
{
	const Token *token = lexer_peek();

	if (token->kind != kind)
		fatal_token(token, "Unexpected token while parsing: expected=%s actual=%s",
		            token_debug_name(kind), token_debug_name(token->kind));

	lexer_next();
}

Global *
find_global(const char *name)
{
	return parser_find_global_optional(name);
}

Global *
parser_find_global_info(const char *name)
{
	return parser_find_global_optional(name);
}

int
sizeof_identifier(const char *name)
{
	Local *local = parser_find_local_latest_optional(name);
	if (local) {
		if (local->is_array) {
			if (local->array_len == 0)
				fatal_cur("sizeof cannot be applied to incomplete type\n");
			return local->array_len * (local->elem_size ? local->elem_size : 4);
		}
		if (local->type) {
			Type *type = local->type;
			int size = type_sizeof(type);

			if (type_is_function(type))
				fatal_cur("sizeof cannot be applied to function type\n");
			if (type_is_pointer(type) && type_pointee(type) &&
			    type_is_function(type_pointee(type)))
				fatal_cur("sizeof cannot be applied to function type\n");
			if (type_is_array(type) && type_array_len(type) == 0) {
				if (local->array_len == 0)
					fatal_cur("sizeof cannot be applied to incomplete type\n");
				return local->array_len *
				       (local->elem_size ? local->elem_size : 4);
			}
			if (size > 0)
				return size;
		}
		if (local->is_pointer)
			return 8;
		return local->elem_size ? local->elem_size : 4;
	}

	{
		Global *global = parser_find_global_optional(name);
		if (global) {
			if (global->is_array) {
				if (global->array_len == 0)
					fatal_cur("sizeof cannot be applied to incomplete type\n");
				return global->array_len * global->elem_size;
			}
			if (global->type) {
				Type *type = global->type;
				int size = type_sizeof(type);

				if (type_is_function(type))
					fatal_cur("sizeof cannot be applied to function type\n");
				if (type_is_pointer(type) && type_pointee(type) &&
				    type_is_function(type_pointee(type)))
					fatal_cur("sizeof cannot be applied to function type\n");
				if (type_is_array(type) && type_array_len(type) == 0) {
					if (global->array_len == 0)
						fatal_cur("sizeof cannot be applied to incomplete type\n");
					return global->array_len *
					       (global->elem_size ? global->elem_size : 4);
				}
				if (size > 0)
					return size;
			}
			if (global->elem_size == 8)
				return 8;
			return global->elem_size ? global->elem_size : 4;
		}
	}

	return -1;
}

int
sizeof_array_elem_identifier(const char *name)
{
	Local *local = parser_find_local_latest_optional(name);
	if (local) {
		if (local->type && local->type->base)
			return type_sizeof(local->type->base);
		return local->elem_size ? local->elem_size : 4;
	}

	{
		Global *global = parser_find_global_optional(name);
		if (global)
			return global->elem_size ? global->elem_size : 4;
	}

	return -1;
}

int
alignof_identifier(const char *name)
{
	for (int i = 0; i < pscope.local_count; i++) {
		if (STRCMP(pscope.locals[i].name, name) == 0) {
			if (pscope.locals[i].is_vla) {
				Type *elem_type = pscope.locals[i].vla_elem_type;
				if (elem_type)
					return type_alignof(elem_type);
				if (pscope.locals[i].elem_size >= 8)
					return 8;
				if (pscope.locals[i].elem_size >= 4)
					return 4;
				if (pscope.locals[i].elem_size >= 2)
					return 2;
				return 1;
			}
			if (pscope.locals[i].align > 0)
				return pscope.locals[i].align;
			if (pscope.locals[i].type)
				return type_alignof(pscope.locals[i].type);
			if (pscope.locals[i].is_pointer)
				return 8;
			if (pscope.locals[i].elem_size >= 8)
				return 8;
			if (pscope.locals[i].elem_size >= 4)
				return 4;
			if (pscope.locals[i].elem_size >= 2)
				return 2;
			return 1;
		}
	}

	for (int i = 0; i < punit.global_count; i++) {
		if (STRCMP(punit.globals[i].name, name) == 0) {
			if (punit.globals[i].align > 0)
				return punit.globals[i].align;
			if (punit.globals[i].type)
				return type_alignof(punit.globals[i].type);
			if (punit.globals[i].is_array) {
				int elem_size = punit.globals[i].elem_size;
				if (elem_size >= 8)
					return 8;
				if (elem_size >= 4)
					return 4;
				if (elem_size >= 2)
					return 2;
				return 1;
			}
			if (punit.globals[i].elem_size >= 8)
				return 8;
			if (punit.globals[i].elem_size >= 4)
				return 4;
			if (punit.globals[i].elem_size >= 2)
				return 2;
			return 1;
		}
	}

	return -1;
}



Global *
new_global_slot(const char *name)
{
	/* Ensure capacity for one more entry without incrementing punit.global_count.
	 * The caller is responsible for punit.global_count++ when the entry is committed. */
	if (punit.global_count >= punit.global_cap) {
		int new_cap = punit.global_cap ? punit.global_cap * 2 : 64;
		Global *old_globals = punit.globals;
		Global *new_globals = xmalloc(sizeof(Global) * (size_t)new_cap);

		if (old_globals && punit.global_cap > 0)
			memcpy(new_globals, old_globals,
			       sizeof(Global) * (size_t)punit.global_cap);
		parser_zero_grown_tail(new_globals, punit.global_cap, new_cap, sizeof(Global));
		xfree(old_globals);
		punit.globals = new_globals;
		punit.global_cap = new_cap;
	}
	Global *g = &punit.globals[punit.global_count];
	memset(g, 0, sizeof(*g));
	if (name)
		STRNCPY(g->name, name, sizeof(g->name) - 1);
	if (parser_trace_toplevel_enabled() && name &&
	    (parser_ident_eq(name, "tcc_lang_standard") ||
	     parser_ident_eq(name, "tcc_iso_diagnostics"))) {
		fprintf(stderr,
		        "tcc parse: new-global-slot name=%s g=%p count=%d cap=%d\n",
		        name, (void *)g, punit.global_count, punit.global_cap);
	}
	return g;
}

int
parser_global_index(Global *g)
{
	if (!g)
		return -1;
	return (int)(g - punit.globals);
}

Global *
parser_global_at(int index)
{
	if (index < 0 || index >= punit.global_cap)
		return NULL;
	return &punit.globals[index];
}

int
parser_global_count(void)
{
	return punit.global_count;
}

int
parser_global_is_thread_local(const char *name)
{
	Global *g = parser_find_global_optional(name);
	return g ? g->is_thread_local : 0;
}

void
parser_commit_reserved_global(void)
{
	ParserUnit *unit = &punit;
	int index = unit->global_count;
	Global *g = &unit->globals[index];

	if (g && g->name[0])
		parser_reject_scope_typedef_name(g->name);
	if (g)
		g->is_thread_local = pfunc.file_thread_local;

	unit->global_count = index + 1;
	parser_global_hash_note_new_index(index);
	parser_invalidate_global_lookup_cache();
}

/* Pre-grow the punit.globals[] array so it has at least `spare` free slots beyond
 * punit.global_count.  Called before entering struct/array initialisers that may
 * allocate helper string punit.globals via new_global_slot: without pre-allocation
 * an xrealloc inside new_global_slot would move punit.globals[], invalidating any
 * Global* the caller holds across that call. */
void
globals_ensure_spare(int spare)
{
	int need = punit.global_count + spare;
	if (need <= punit.global_cap)
		return;
	int new_cap = punit.global_cap ? punit.global_cap * 2 : 64;
	while (new_cap < need) new_cap *= 2;
	{
		Global *old_globals = punit.globals;
		Global *new_globals = xmalloc(sizeof(Global) * (size_t)new_cap);

		if (old_globals && punit.global_cap > 0)
			memcpy(new_globals, old_globals,
			       sizeof(Global) * (size_t)punit.global_cap);
		parser_zero_grown_tail(new_globals, punit.global_cap, new_cap, sizeof(Global));
		xfree(old_globals);
		punit.globals = new_globals;
	}
	punit.global_cap = new_cap;
}

Global *
new_global_object(const char *name, int elem_size)
{
	Global *g = new_global_slot(name);
	g->elem_size = elem_size;
	g->array_len = 1;
	g->align = 0;
	return g;
}

void
apply_type_to_global(Global *g, Type *type)
{
	int base_is_named_aggregate;
	int type_is_named_aggregate;

	if (!g || !type)
		return;

	g->type = type;
	g->array_len = 1;
	g->is_unsigned = type_is_unsigned(type);
	g->is_struct = 0;
	g->struct_name[0] = '\0';

	if (type_is_pointer(type)) {
		g->elem_size = TCC_SIZEOF_PTR;
		Type *base = type_pointee(type);
		base_is_named_aggregate = base &&
		                          (type_is_struct(base) || type_is_union(base)) &&
		                          base->struct_name[0];
		g->ptr_elem_size = base && type_sizeof(base) ? type_sizeof(base) : 1;
		if (base_is_named_aggregate)
			STRNCPY(g->struct_name, base->struct_name, sizeof(g->struct_name) - 1);
		return;
	}

	if (type_is_array(type)) {
		Type *base = type_pointee(type);
		base_is_named_aggregate = base &&
		                          (type_is_struct(base) || type_is_union(base)) &&
		                          base->struct_name[0];
		g->is_array = 1;
		g->array_len = type_array_len(type);
		g->elem_size = base && type_sizeof(base) ? type_sizeof(base) : 4;
		if (base_is_named_aggregate) {
			g->is_struct = 1;
			STRNCPY(g->struct_name, base->struct_name, sizeof(g->struct_name) - 1);
		}
		return;
	}

	g->elem_size = type_sizeof(type) ? type_sizeof(type) : 4;
	type_is_named_aggregate = (type_is_struct(type) || type_is_union(type)) &&
	                          type->struct_name[0];

	if (type_is_named_aggregate) {
		g->is_struct = 1;
		STRNCPY(g->struct_name, type->struct_name, sizeof(g->struct_name) - 1);
	}
}


static void
reset_global_slot(Global *g)
{
	if (!g)
		return;
	global_init_free(g);
	xfree(g->string_value);
	char name[64];
	STRNCPY(name, g->name, sizeof(name) - 1);
	memset(g, 0, sizeof(*g));
	STRNCPY(g->name, name, sizeof(g->name) - 1);
}

static void
mark_global_initialized(Global *g)
{
	if (g)
		g->has_initializer = 1;
}

void
set_global_integer_initializer(Global *g, long long value)
{
	mark_global_initialized(g);
	g->init_value = value;
	g->is_addr = 0;
}

static void
set_global_address_initializer(Global *g, const char *name)
{
	mark_global_initialized(g);
	g->is_addr = 1;
	g->addr_name[0] = '\0';
	STRNCPY(g->addr_name, name, sizeof(g->addr_name) - 1);
}

void
parser_set_global_address_initializer(Global *g, const char *name)
{
	set_global_address_initializer(g, name);
}

void
set_global_string_initializer_len(Global *g, const char *value, size_t len)
{
	mark_global_initialized(g);
	g->is_string = 1;
	g->string_label = parser_alloc_string_label();
	xfree(g->string_value);
	g->string_value = xmalloc(len + 1);
	memcpy(g->string_value, value ? value : "", len);
	g->string_value[len] = '\0';
	g->string_len = (unsigned int)len;
}

void
set_global_string_initializer(Global *g, const char *value)
{
	set_global_string_initializer_len(g, value, value ? strlen(value) : 0);
}

void
set_global_string_array_initializer_len(Global *g, const char *value, size_t len)
{
	mark_global_initialized(g);
	g->is_string_array = 1;
	xfree(g->string_value);
	g->string_value = xmalloc(len + 1);
	memcpy(g->string_value, value ? value : "", len);
	g->string_value[len] = '\0';
	g->string_len = (unsigned int)len;
}

void
set_global_string_array_initializer(Global *g, const char *value)
{
	set_global_string_array_initializer_len(g, value, value ? strlen(value) : 0);
}

int 
is_global(const char *name)
{
	return find_global(name) != NULL;
}

void 
commit_global_definition(Global *g)
{
	if (g && g->name[0])
		parser_reject_scope_typedef_name(g->name);
	g->is_static = pfunc.file_static;
	g->is_thread_local = pfunc.file_thread_local;

	Global *existing = find_global(g->name);

	if (existing && existing != g) {
		if (existing->is_thread_local != g->is_thread_local)
			fatal_cur("Conflicting declaration for global '%s'\n", g->name);
		parser_validate_global_object_redeclaration(existing, g->name, g->type);

		if (existing->is_extern) {
			if (g->is_static)
				fatal_cur("static declaration follows non-static declaration for '%s'\n",
				          g->name);
			memcpy(existing, g, sizeof(*existing));
			existing->is_extern = 0;
			existing->is_static = g->is_static;
			g->init = NULL; /* existing now owns the init data; prevent double-free */
			reset_global_slot(g);
			return;
		}

		/* Tentative definition: "int x;" with no initializer may repeat.
		 * If new entry has an initializer and existing doesn't, update existing. */
		int new_has_init = g->has_initializer;
		int old_has_init = existing->has_initializer;
		if (!new_has_init) {
			parser_merge_global_object_redeclaration(existing, g->type);
			/* Tentative redeclaration: discard new, keep existing */
			reset_global_slot(g);
			return;
		}
		if (new_has_init && !old_has_init) {
			/* Definition after tentative: update existing with initializer */
			char saved_name[64];
			STRNCPY(saved_name, existing->name, sizeof(saved_name) - 1);
			memcpy(existing, g, sizeof(*existing));
			STRNCPY(existing->name, saved_name, sizeof(existing->name) - 1);
			g->init = NULL; /* existing now owns the init data; prevent double-free */
			reset_global_slot(g);
			return;
		}

		fatal_cur("Global already defined: %s\n", g->name);
	}

	punit.global_count++;
	parser_global_hash_note_new_index(punit.global_count - 1);
	parser_invalidate_global_lookup_cache();
}

int 
is_global_array(const char *name)
{
	Global *g = parser_find_global_optional(name);
	return g && g->is_array;
}

int 
is_global_struct(const char *name)
{
	Global *g = parser_find_global_optional(name);
	return g && g->is_struct;
}

const char *
global_struct_name(const char *name)
{
	Global *g = parser_find_global_optional(name);
	if (!g)
		return "";
	if (g->struct_name[0])
		return g->struct_name;
	if (!g->is_struct)
		return "";
	return g->struct_name;
}

int 
global_elem_size(const char *name)
{
	Global *g = parser_require_global(name);
	return g->elem_size;
}

Type *
global_array_decay_type(const char *name, int *out_elem_size)
{
	Global *g = parser_find_global_optional(name);
	const Type *base;

	if (!g || !g->is_array) {
		if (out_elem_size)
			*out_elem_size = global_elem_size(name);
		return type_ptr(type_for_size(global_elem_size(name)));
	}

	base = g->type && type_is_array(g->type) ? type_pointee(g->type) : NULL;
	if (!base) {
		Type *elem = type_for_size(g->elem_size ? g->elem_size : 4);
		if (g->array_dim_count > 1)
			elem = build_array_type_from_dims(elem, g->array_dims + 1, g->array_dim_count - 1);
		base = elem;
	}

	if (out_elem_size)
		*out_elem_size = base->size;
	return type_ptr((Type *)base);
}

int
global_array_stride_bytes_for_dim(Global *g, int dim)
{
	int stride = g && g->elem_size ? g->elem_size : 4;
	if (!g)
		return stride;
	for (int i = dim + 1; i < g->array_dim_count && i < MAX_ARRAY_DIMS; i++) {
		int d = g->array_dims[i] ? g->array_dims[i] : 1;
		stride *= d;
	}
	return stride;
}

Node *
scale_index_to_bytes(Node *idx, int stride)
{
	if (stride == 1)
		return idx;
	return new_binary(ND_MUL, idx, new_num(stride));
}

Node *
append_byte_index(Node *acc, Node *idx, int stride)
{
	Node *term = scale_index_to_bytes(idx, stride);
	if (!acc)
		return term;
	return new_binary(ND_ADD, acc, term);
}

Type *
global_array_remaining_ptr_type(Global *g, int consumed_dims, int *out_elem_size)
{
	const Type *current = g ? g->type : NULL;

	while (current && consumed_dims > 0 && type_is_array(current)) {
		current = type_pointee(current);
		consumed_dims--;
	}
	if (!current) {
		Type *elem;
		elem = type_for_size(g && g->elem_size ? g->elem_size : 4);
		if (g && consumed_dims < g->array_dim_count) {
			elem = build_array_type_from_dims(elem,
			                                  g->array_dims + consumed_dims,
			                                  g->array_dim_count - consumed_dims);
		}
		current = elem;
	}
	if (out_elem_size)
		*out_elem_size = current ? current->size : (g && g->elem_size ? g->elem_size : 4);
	return type_ptr((Type *)current);
}


int 
is_global_unsigned(const char *name)
{
	Global *g = parser_find_global_optional(name);
	return g ? g->is_unsigned : 0;
}

int 
expr_is_unsigned_for_compare(Node *node)
{
	if (!node)
		return 0;

	if (node->is_unsigned)
		return 1;

	if (node->type && node->type->is_unsigned)
		return 1;

	if ((node->kind == ND_VAR || node->kind == ND_GLOBAL || node->kind == ND_GLOBAL_INDEX) && node->name[0]) {
		if (is_global(node->name))
			return is_global_unsigned(node->name);

		Type *t = type_local(node->name);
		return t && t->is_unsigned;
	}

	return 0;
}

int 
is_global_pointer(const char *name)
{
	Global *g = parser_find_global_optional(name);
	return g &&
	       !g->is_array &&
	       !g->is_struct &&
	       ((g->type && type_is_pointer(g->type)) ||
	        g->ptr_elem_size > 0 ||
	        g->is_string);
}

int 
global_pointer_elem_size(const char *name)
{
	Global *g = parser_find_global_optional(name);
	if (!g || !g->ptr_elem_size)
		return 1;
	return g->ptr_elem_size;
}

Type *
global_type(const char *name)
{
	Global *g = parser_find_global_optional(name);
	return g ? g->type : NULL;
}

static void 
store_global_init_int(Global *g, int offset, int size, long long value)
{
	if (offset < 0) {
		fatal_cur("Global struct initializer has negative offset\n");
	}

	if (size == 1) {
		global_set_init_byte(g, offset, value & 255);

			return;
	}

	{
		unsigned long long uvalue = (unsigned long long)value;
		for (int i = 0; i < size; i++) {
			global_set_init_byte(g, offset + i, (long long)(uvalue & 255));
			uvalue >>= 8;
		}
	}

}

static void 
store_global_init_string(Global *g, int offset, int size, const char *value)
{
	if (offset < 0) {
		fatal_cur("Global struct initializer has negative offset\n");
	}

	int i = 0;
	for (; i < size && value[i]; i++)
		global_set_init_byte(g, offset + i, (unsigned char)value[i]);

	if (i < size)
		global_set_init_byte(g, offset + i++, 0);

	for (; i < size; i++)
		global_set_init_byte(g, offset + i, 0);

}

static void
store_global_init_float(Global *g, int offset, int size, double value)
{
	if (offset < 0) {
		fatal_cur("Global struct initializer has negative offset\n");
	}

	if (size == 4) {
		union { unsigned int u; unsigned char bytes[4]; } bits;
		union { double d; unsigned long long u; } dbits;
		dbits.d = value;
		bits.u = tcc_double_bits_to_float_bits(dbits.u);
		for (int i = 0; i < 4; i++)
			global_set_init_byte(g, offset + i, bits.bytes[i]);
		return;
	}

	if (size == 8) {
		union { double d; unsigned char bytes[8]; } bits;
		bits.d = value;
		for (int i = 0; i < 8; i++)
			global_set_init_byte(g, offset + i, bits.bytes[i]);
		return;
	}

	fatal_cur("floating global initializer size not supported\n");
}

static void
store_global_init_symbol(Global *g, int offset, int size, const char *sym)
{
	if (offset < 0) {
		fatal_cur("Global symbol initializer has negative offset\n");
	}

	/*
	 * init_syms[] is consumed by emit.c as an 8-byte relocation slot table:
	 * slot 0 covers byte offset 0, slot 1 covers byte offset 8, etc.
	 * This is used for pointer/function-pointer fields inside global
	 * struct aggregates and for pointer-sized global arrays.
	 */
	if ((offset & 7) != 0) {
		fatal_cur("Global symbol initializer is not pointer aligned\n");
	}

	store_global_init_int(g, offset, size >= 8 ? 8 : size, 0);

	int slot = offset / 8;
	if (slot >= 0)
		global_set_init_sym(g, slot, sym);

	if (global_init_count(g) < offset + 8)
		global_set_init_count(g, offset + 8);
}


static int try_parse_global_scalar_initializer_value(long long *out);
static Node *parse_array_compound_literal_subscript_expr(void);
static int parser_global_array_compound_literal_subscript_matches(void);
static void parser_build_global_scalar_array_compound_literal(char lit_name[64],
                                                              Type **out_array_type);

static int
parser_type_is_floating_scalar(const Type *type)
{
	return type &&
	       !type_is_complex(type) &&
	       !type_is_imaginary(type) &&
	       (type->kind == TY_FLOAT || type->kind == TY_DOUBLE);
}

static int
parser_type_is_float_storage_scalar(const Type *type)
{
	return type &&
	       !type_is_complex(type) &&
	       (type->kind == TY_FLOAT || type->kind == TY_DOUBLE);
}

static int
parser_type_is_supported_complex_scalar(const Type *type)
{
	return type && type_is_complex(type) &&
	       (type_sizeof(type) == 8 || type_sizeof(type) == 16);
}

static void
store_global_init_complex_real_zero(Global *g, int offset, const Type *type, double real_value)
{
	int total_size;
	int component_size;

	if (!g || !type || !parser_type_is_supported_complex_scalar(type))
		fatal_cur("complex global initializer size not supported\n");

	total_size = type_sizeof(type);
	component_size = total_size / 2;
	store_global_init_float(g, offset, component_size, real_value);
	for (int i = component_size; i < total_size; i++)
		global_set_init_byte(g, offset + i, 0);
}

static void
store_global_init_complex_imag_zero(Global *g, int offset, const Type *type, double imag_value)
{
	int total_size;
	int component_size;

	if (!g || !type || !parser_type_is_supported_complex_scalar(type))
		fatal_cur("complex global initializer size not supported\n");

	total_size = type_sizeof(type);
	component_size = total_size / 2;
	for (int i = 0; i < component_size; i++)
		global_set_init_byte(g, offset + i, 0);
	store_global_init_float(g, offset + component_size, component_size, imag_value);
}

static int
token_spelling_looks_floating(const Token *tok)
{
	const char *p;
	int is_hex = 0;

	if (!tok)
		return 0;

	if (tok->num_is_fp)
		return 1;

	if (!tok->text)
		return 0;

	is_hex = tok->text[0] == '0' &&
	         (tok->text[1] == 'x' || tok->text[1] == 'X');

	for (p = tok->text; *p; p++) {
		if (*p == '.')
			return 1;
		if (!is_hex && (*p == 'e' || *p == 'E'))
			return 1;
		if (is_hex && (*p == 'p' || *p == 'P'))
			return 1;
	}

	return 0;
}

static int
try_parse_global_scalar_primary(long long *out)
{
	if (lexer_peek()->kind == TOK_LPAREN) {
		/* Cast around a constant, e.g. (u8)45, (unsigned char)45,
		 * or a function-pointer cast like (void(*)(void*))0. Use proper
		 * depth counting since the cast type itself may contain parens. */
		if (is_type_start_token(lexer_peek_ahead(1)->kind, lexer_peek_ahead(1)->text)) {
			int cast_depth = 1;
			lexer_next(); /* ( */
			while (cast_depth > 0) {
				if (lexer_peek()->kind == TOK_EOF) {
					fatal_cur("Unterminated cast in global initializer\n");
				}
				if (lexer_peek()->kind == TOK_LPAREN) cast_depth++;
				else if (lexer_peek()->kind == TOK_RPAREN) cast_depth--;
				if (cast_depth > 0) lexer_next();
			}
			expect(TOK_RPAREN);
			return try_parse_global_scalar_initializer_value(out);
		}

		/* Parenthesized constant, including nested forms like (((3))). */
		/* Only proceed if the next token could be a scalar (num, ident, minus, plus, paren) */
		{
		TokenKind ahead = lexer_peek_ahead(1)->kind;
		if (ahead != TOK_NUM && ahead != TOK_IDENT && ahead != TOK_MINUS &&
		    ahead != TOK_PLUS && ahead != TOK_LPAREN)
			return 0;
		}
		lexer_next();
		if (!try_parse_global_scalar_initializer_value(out)) {
			/* Could not parse — we've consumed ( but can't un-consume.
			 * This is an error case; bail out. */
			return 0;
		}
		expect(TOK_RPAREN);
		return 1;
	}

	if (lexer_peek()->kind == TOK_MINUS) {
		lexer_next();
		if (!try_parse_global_scalar_primary(out))
			return 0;
		*out = -*out;
		return 1;
	}

	if (lexer_peek()->kind == TOK_PLUS) {
		lexer_next();
		return try_parse_global_scalar_primary(out);
	}

	if (lexer_peek()->kind == TOK_NUM) {
		*out = lexer_peek()->long_value;
		lexer_next();
		return 1;
	}

	if (lexer_peek()->kind == TOK_SIZEOF ||
	    (lexer_peek()->kind == TOK_IDENT && lexer_peek()->text &&
	     STRCMP(lexer_peek()->text, "offsetof") == 0)) {
		*out = eval_const_array_size();
		return 1;
	}

	if (lexer_peek()->kind == TOK_IDENT) {
		int enum_value = 0;
		if (tcc_lang_at_least(LANG_C23) && token_is_c23_true_keyword(lexer_peek())) {
			*out = 1;
			lexer_next();
			return 1;
		}
		if (tcc_lang_at_least(LANG_C23) && token_is_c23_false_keyword(lexer_peek())) {
			*out = 0;
			lexer_next();
			return 1;
		}
		if (parser_find_enum_const(lexer_peek()->text, &enum_value)) {
			*out = enum_value;
			lexer_next();
			return 1;
		}
	}

	return 0;
}

static int
try_parse_global_pointer_symbol_initializer(char *out_sym, int out_sym_size,
                                            int *out_addr_offset,
                                            int ptr_elem_size,
                                            int *out_explicit_addr)
{
	const Token *value = lexer_peek();

	if (out_sym && out_sym_size > 0)
		out_sym[0] = '\0';
	if (out_addr_offset)
		*out_addr_offset = 0;
	if (out_explicit_addr)
		*out_explicit_addr = 0;

	if (value->kind == TOK_AMP && lexer_peek_ahead(1)->kind == TOK_IDENT) {
		int byte_off = 0;

		if (out_explicit_addr)
			*out_explicit_addr = 1;
		lexer_next();
		value = lexer_peek();
		if (out_sym && out_sym_size > 0)
			STRNCPY(out_sym, value->text ? value->text : "", (size_t)out_sym_size - 1);
		lexer_next();
		if (lexer_peek()->kind == TOK_LBRACKET) {
			int elem_sz = ptr_elem_size > 0 ? ptr_elem_size : 1;

			lexer_next();
			if (elem_sz == 1) {
				Global *tgt = find_global(value->text);
				if (tgt && tgt->elem_size > 1)
					elem_sz = tgt->elem_size;
			}
			byte_off = (int)(eval_const_array_size() * elem_sz);
			expect(TOK_RBRACKET);
		}
		if (out_addr_offset)
			*out_addr_offset = byte_off;
		return 1;
	}

	if (value->kind == TOK_IDENT) {
		if (out_sym && out_sym_size > 0)
			STRNCPY(out_sym, value->text ? value->text : "", (size_t)out_sym_size - 1);
		lexer_next();
		return 1;
	}

	if (value->kind == TOK_LPAREN) {
		if (is_type_start_token(lexer_peek_ahead(1)->kind, lexer_peek_ahead(1)->text)) {
			int cast_depth = 1;

			lexer_next();
			while (cast_depth > 0) {
				if (lexer_peek()->kind == TOK_EOF)
					fatal_cur("Unterminated cast in global pointer initializer\n");
				if (lexer_peek()->kind == TOK_LPAREN)
					cast_depth++;
				else if (lexer_peek()->kind == TOK_RPAREN)
					cast_depth--;
				if (cast_depth > 0)
					lexer_next();
			}
			expect(TOK_RPAREN);
			return try_parse_global_pointer_symbol_initializer(out_sym, out_sym_size,
			                                                   out_addr_offset, ptr_elem_size,
			                                                   out_explicit_addr);
		}
	}

	return 0;
}

static void
validate_pointer_initializer_source_name_resolved(const char **out_name,
                                                  const char *name)
{
	const char *resolved;

	if (!out_name)
		return;
	resolved = name;
	if (name && is_static_local(name)) {
		const char *gname = static_global_name_local(name);
		if (gname && gname[0])
			resolved = gname;
	}
	*out_name = resolved;
}

static void
validate_global_pointer_symbol_initializer_compatibility(Type *dst_type,
                                                         const char *src_name,
                                                         int explicit_addr)
{
	Type *src_type = NULL;
	Type *array_addr_type = NULL;
	Type *array_decay_type = NULL;
	FuncInfo *src_func = NULL;

	if (!dst_type || !type_is_pointer(dst_type) || !src_name || !src_name[0])
		return;

	src_func = find_func(src_name);
	if (src_func)
		src_type = type_ptr(parser_func_info_signature_type(src_func));

	if (!src_type && explicit_addr && src_name && is_static_local(src_name)) {
		Type *ltype = type_local(src_name);
		if (ltype && type_is_array(ltype)) {
			array_addr_type = type_ptr(clone_type(ltype));
			array_decay_type = type_ptr(clone_type(type_pointee(ltype)));
			src_type = array_addr_type;
		} else if (ltype) {
			src_type = type_ptr(clone_type(ltype));
		}
	}

	if (!src_type && is_global(src_name)) {
		Type *gtype = global_type(src_name);
		if (gtype && type_is_array(gtype)) {
			array_addr_type = type_ptr(clone_type(gtype));
			array_decay_type = global_array_decay_type(src_name, NULL);
			src_type = explicit_addr ? array_addr_type : array_decay_type;
		} else if (gtype) {
			src_type = type_ptr(clone_type(gtype));
		}
	}

	if (!src_type || !type_is_pointer(src_type))
		return;
	if (explicit_addr && array_addr_type &&
	    type_pointer_assignment_compatible(dst_type, array_addr_type, 0))
		return;
	if (explicit_addr && array_decay_type &&
	    type_pointer_assignment_compatible(dst_type, array_decay_type, 0))
		return;
	if (explicit_addr &&
	    type_is_void(type_pointee(dst_type)) &&
	    type_pointee(src_type) &&
	    !type_is_function(type_pointee(src_type)))
		return;
	if (type_pointer_assignment_compatible(dst_type, src_type, 0))
		return;
	fatal_cur("Incompatible pointer types in initializer\n");
}

static int
try_parse_global_scalar_initializer_value(long long *out)
{
	long long val = 0;

	if (!try_parse_global_scalar_primary(&val))
		return 0;

	/*
	 * File-scope aggregate initializers need integer constant expressions, not
	 * just bare numeric tokens.  This deliberately handles the small constant
	 * expression subset already accepted elsewhere by this compiler: arithmetic,
	 * shifts, bitwise and logical operators.  It stops before aggregate
	 * delimiters such as ',', '}', and ']'.
	 */
	while (lexer_peek()->kind == TOK_STAR || lexer_peek()->kind == TOK_PLUS ||
	       lexer_peek()->kind == TOK_MINUS || lexer_peek()->kind == TOK_SLASH ||
	       lexer_peek()->kind == TOK_PERCENT || lexer_peek()->kind == TOK_AND ||
	       lexer_peek()->kind == TOK_OR || lexer_peek()->kind == TOK_AMP ||
	       lexer_peek()->kind == TOK_PIPE || lexer_peek()->kind == TOK_CARET ||
	       lexer_peek()->kind == TOK_SHL || lexer_peek()->kind == TOK_SHR ||
	       lexer_peek()->kind == TOK_EQ || lexer_peek()->kind == TOK_NE ||
	       lexer_peek()->kind == TOK_LT || lexer_peek()->kind == TOK_GT ||
	       lexer_peek()->kind == TOK_LE || lexer_peek()->kind == TOK_GE) {
		TokenKind op = lexer_peek()->kind;
		long long rhs = 0;
		lexer_next();
		if (!try_parse_global_scalar_primary(&rhs))
			return 0;

		if (op == TOK_STAR) val *= rhs;
		else if (op == TOK_PLUS) val += rhs;
		else if (op == TOK_MINUS) val -= rhs;
		else if (op == TOK_SLASH) { if (rhs) val /= rhs; }
		else if (op == TOK_PERCENT) { if (rhs) val %= rhs; }
		else if (op == TOK_AND) val = val && rhs;
		else if (op == TOK_OR) val = val || rhs;
		else if (op == TOK_AMP) val &= rhs;
		else if (op == TOK_PIPE) val |= rhs;
		else if (op == TOK_CARET) val ^= rhs;
		else if (op == TOK_SHL) val <<= rhs;
		else if (op == TOK_SHR) val >>= rhs;
		else if (op == TOK_EQ) val = (val == rhs);
		else if (op == TOK_NE) val = (val != rhs);
		else if (op == TOK_LT) val = (val < rhs);
		else if (op == TOK_GT) val = (val > rhs);
		else if (op == TOK_LE) val = (val <= rhs);
		else if (op == TOK_GE) val = (val >= rhs);
	}

	*out = val;
	return 1;
}



int is_global(const char *name);

long long
parse_global_scalar_initializer_value_or_die(const char *message)
{
	Node *expr;

	if (lexer_peek()->kind == TOK_LPAREN) {
		expr = parse_array_compound_literal_subscript_expr();
		if (!expr)
			expr = parse_struct_compound_literal_member_expr();
		if (expr)
			expr = fold_constants(expr);
		else
			expr = fold_constants(parse_assignment());
	} else {
		expr = fold_constants(parse_assignment());
	}

	if (!expr)
		fatal_cur(message);
	if (expr->kind == ND_NUM)
		return expr->long_value;
	if (expr->kind == ND_CAST && expr->left && expr->left->kind == ND_NUM)
		return expr->left->long_value;
	fatal_cur(message);
	return 0;
}

static int
eval_global_const_numeric_expr(Node *expr, double *out)
{
	double lhs = 0.0;
	double rhs = 0.0;

	if (!expr || !out)
		return 0;

	switch (expr->kind) {
	case ND_NUM:
		if (expr->is_fp_num) {
			if (expr->string_value)
				*out = strtod(expr->string_value, NULL);
			else
				*out = 0.0;
		} else {
			*out = (double)expr->long_value;
		}
		return 1;
	case ND_CAST:
		return eval_global_const_numeric_expr(expr->left, out);
	case ND_COMMA:
		if (!eval_global_const_numeric_expr(expr->left, &lhs))
			return 0;
		return eval_global_const_numeric_expr(expr->right, out);
	case ND_NEG:
		if (!eval_global_const_numeric_expr(expr->left, out))
			return 0;
		*out = -*out;
		return 1;
	case ND_NOT:
		if (!eval_global_const_numeric_expr(expr->left, out))
			return 0;
		*out = !*out;
		return 1;
	case ND_BITNOT:
		if (!eval_global_const_numeric_expr(expr->left, out))
			return 0;
		*out = (double)(~(long long)*out);
		return 1;
	case ND_ADD:
	case ND_SUB:
	case ND_MUL:
	case ND_DIV:
	case ND_EQ:
	case ND_NE:
	case ND_LT:
	case ND_LE:
	case ND_GT:
	case ND_GE:
	case ND_LOGICAL_AND:
	case ND_LOGICAL_OR:
	case ND_BITAND:
	case ND_BITOR:
	case ND_BITXOR:
	case ND_SHL:
	case ND_SHR:
		if (!eval_global_const_numeric_expr(expr->left, &lhs) ||
		    !eval_global_const_numeric_expr(expr->right, &rhs))
			return 0;
		switch (expr->kind) {
		case ND_ADD: *out = lhs + rhs; return 1;
		case ND_SUB: *out = lhs - rhs; return 1;
		case ND_MUL: *out = lhs * rhs; return 1;
		case ND_DIV:
			if (rhs == 0.0)
				fatal_cur("Division by zero in constant expression\n");
			*out = lhs / rhs;
			return 1;
		case ND_EQ: *out = (lhs == rhs); return 1;
		case ND_NE: *out = (lhs != rhs); return 1;
		case ND_LT: *out = (lhs < rhs); return 1;
		case ND_LE: *out = (lhs <= rhs); return 1;
		case ND_GT: *out = (lhs > rhs); return 1;
		case ND_GE: *out = (lhs >= rhs); return 1;
		case ND_LOGICAL_AND: *out = (lhs != 0.0) && (rhs != 0.0); return 1;
		case ND_LOGICAL_OR: *out = (lhs != 0.0) || (rhs != 0.0); return 1;
		case ND_BITAND: *out = (double)(((long long)lhs) & ((long long)rhs)); return 1;
		case ND_BITOR: *out = (double)(((long long)lhs) | ((long long)rhs)); return 1;
		case ND_BITXOR: *out = (double)(((long long)lhs) ^ ((long long)rhs)); return 1;
		case ND_SHL: *out = (double)(((long long)lhs) << ((long long)rhs)); return 1;
		case ND_SHR: *out = (double)(((long long)lhs) >> ((long long)rhs)); return 1;
		default: return 0;
		}
	case ND_COND:
		if (!eval_global_const_numeric_expr(expr->cond, &lhs))
			return 0;
		if (lhs != 0.0)
			return eval_global_const_numeric_expr(expr->then_body, out);
		return eval_global_const_numeric_expr(expr->else_body, out);
	default:
		return 0;
	}
}

static Node *
parse_global_numeric_initializer_expr_raw(void)
{
	Node *expr;

	if (lexer_peek()->kind == TOK_LPAREN) {
		expr = parse_array_compound_literal_subscript_expr();
		if (!expr)
			expr = parse_struct_compound_literal_member_expr();
		if (!expr)
			expr = parse_assignment();
	} else {
		expr = parse_assignment();
	}

	return expr;
}

static double
parse_global_floating_initializer_value_for_type_or_die(const char *message,
                                                        Type *dst_type)
{
	Node *expr;
	double value = 0.0;

	expr = parse_global_numeric_initializer_expr_raw();
	if (dst_type)
		expr = expr_coerce_value_for_type(expr, dst_type);
	expr = fold_constants(expr);

	if (eval_global_const_numeric_expr(expr, &value))
		return value;
	fatal_cur(message);
	return 0.0;
}

static void
parse_scalar_global_initializer(Global *g, const char *message)
{
	int brace_depth = 0;
	int g_idx = g ? (int)(g - punit.globals) : -1;
	int committed_early = 0;

	while (lexer_peek()->kind == TOK_LBRACE) {
		brace_depth++;
		lexer_next();
	}

	if (g_idx >= 0 &&
	    g_idx == punit.global_count &&
	    lexer_peek()->kind == TOK_LPAREN &&
	    parser_global_array_compound_literal_subscript_matches()) {
		parser_commit_reserved_global();
		committed_early = 1;
	}

	if (g && g->type && parser_type_is_float_storage_scalar(g->type)) {
		int size;
		double dvalue;

		dvalue = parse_global_floating_initializer_value_for_type_or_die(message,
		                                                               g->type);
		if (g_idx >= 0)
			g = &punit.globals[g_idx];

		size = type_sizeof(g->type);
		if (size == 4) {
			union { unsigned int u; unsigned char bytes[4]; } bits;
			union { double d; unsigned long long u; } dbits;
			dbits.d = dvalue;
			bits.u = tcc_double_bits_to_float_bits(dbits.u);
			for (int i = 0; i < 4; i++)
				global_set_init_byte(g, i, bits.bytes[i]);
			global_set_init_count(g, 4);
		} else if (size == 8) {
			union { double d; unsigned char bytes[8]; } bits;
			bits.d = dvalue;
			for (int i = 0; i < 8; i++)
				global_set_init_byte(g, i, bits.bytes[i]);
			global_set_init_count(g, 8);
		} else {
			fatal_cur("floating global initializer size not supported\n");
		}
	} else if (g && g->type && parser_type_is_supported_complex_scalar(g->type)) {
		Node *expr;
		double dvalue;
		int size;
		int src_is_imag;

		expr = parse_global_numeric_initializer_expr_raw();
		src_is_imag = expr && expr->type && type_is_imaginary(expr->type);
		expr = fold_constants(expr);
		if (!eval_global_const_numeric_expr(expr, &dvalue))
			fatal_cur(message);
		if (g_idx >= 0)
			g = &punit.globals[g_idx];

		size = type_sizeof(g->type);
		if (src_is_imag)
			store_global_init_complex_imag_zero(g, 0, g->type, dvalue);
		else
			store_global_init_complex_real_zero(g, 0, g->type, dvalue);
		global_set_init_count(g, size);
	} else {
		long long value = parse_global_scalar_initializer_value_or_die(message);
		if (g_idx >= 0)
			g = &punit.globals[g_idx];
		set_global_integer_initializer(g, value);
	}

	if (committed_early && g_idx >= 0)
		g = &punit.globals[g_idx];

	while (brace_depth-- > 0) {
		if (lexer_peek()->kind == TOK_COMMA) {
			lexer_next();
			if (lexer_peek()->kind != TOK_RBRACE)
				fatal_cur("Too many initializers for global scalar\n");
		}
		expect(TOK_RBRACE);
	}
}


static void
validate_global_pointer_initializer_compatibility(Type *dst_type, const Token *value)
{
	Type *src_type = NULL;
	Type *array_addr_type = NULL;
	Type *array_decay_type = NULL;
	int explicit_addr = 0;
	const char *src_name = NULL;
	FuncInfo *src_func = NULL;

	if (!dst_type || !type_is_pointer(dst_type) || !value)
		return;

	if (tcc_lang_at_least(LANG_C23) && token_is_c23_nullptr_keyword(value))
		return;

	if (value->kind == TOK_NUM) {
		if (!value->num_is_fp && value->long_value == 0)
			return;
		fatal_cur("Incompatible integer to pointer conversion in initializer\n");
	} else if (value->kind == TOK_STRING) {
		src_type = type_ptr(type_char());
	} else if (value->kind == TOK_AMP && lexer_peek_ahead(1)->kind == TOK_IDENT) {
		explicit_addr = 1;
		src_name = lexer_peek_ahead(1)->text;
	} else if (value->kind == TOK_IDENT) {
		src_name = value->text;
	} else {
		return;
	}

	if (src_name)
		src_func = find_func(src_name);
	if (src_func)
		src_type = type_ptr(parser_func_info_signature_type(src_func));

	if (!src_type && explicit_addr && src_name && is_static_local(src_name)) {
		Type *ltype = type_local(src_name);
		if (ltype && type_is_array(ltype)) {
			array_addr_type = type_ptr(clone_type(ltype));
			array_decay_type = type_ptr(clone_type(type_pointee(ltype)));
			src_type = array_addr_type;
		} else if (ltype) {
			src_type = type_ptr(clone_type(ltype));
		}
	}

	if (!src_type && src_name && is_global(src_name)) {
		Type *gtype = global_type(src_name);
		if (gtype && type_is_array(gtype)) {
			array_addr_type = type_ptr(clone_type(gtype));
			array_decay_type = global_array_decay_type(src_name, NULL);
			src_type = explicit_addr ? array_addr_type : array_decay_type;
		} else if (gtype) {
			src_type = type_ptr(clone_type(gtype));
		}
	}

	if (!src_type || !type_is_pointer(src_type))
		return;
	if (explicit_addr && array_addr_type &&
	    type_pointer_assignment_compatible(dst_type, array_addr_type, 0))
		return;
	if (explicit_addr && array_decay_type &&
	    type_pointer_assignment_compatible(dst_type, array_decay_type, 0))
		return;
	if (explicit_addr &&
	    type_is_void(type_pointee(dst_type)) &&
	    type_pointee(src_type) &&
	    !type_is_function(type_pointee(src_type)))
		return;
	if (type_pointer_assignment_compatible(dst_type, src_type, 0))
		return;
	fatal_cur("Incompatible pointer types in initializer\n");
}

static int
parser_try_parse_global_array_designator(int *first_index, int *last_index)
{
	if (lexer_peek()->kind != TOK_LBRACKET)
		return 0;
	reject_c89_designated_initializer();
	if (parser_array_bound_contains_nonconstant_identifier())
		fatal_cur("Expected constant index in designated initializer\n");

	lexer_next(); /* [ */
	*first_index = (int)parser_eval_const_int_expr();
	*last_index = *first_index;

	if (lexer_peek()->kind == TOK_DOT &&
	    lexer_peek_ahead(1)->kind == TOK_DOT &&
	    lexer_peek_ahead(2)->kind == TOK_DOT) {
		lexer_next();
		lexer_next();
		lexer_next();
		*last_index = (int)parser_eval_const_int_expr();
	}

	expect(TOK_RBRACKET);
	expect(TOK_ASSIGN);
	return 1;
}

static int
parser_try_parse_local_array_designator(int *first_index, int *last_index)
{
	if (lexer_peek()->kind != TOK_LBRACKET)
		return 0;
	if (parser_array_bound_contains_nonconstant_identifier())
		fatal_cur("Expected constant index in designated initializer\n");

	lexer_next(); /* [ */
	*first_index = (int)parser_eval_const_int_expr();
	*last_index = *first_index;

	if (lexer_peek()->kind == TOK_DOT &&
	    lexer_peek_ahead(1)->kind == TOK_DOT &&
	    lexer_peek_ahead(2)->kind == TOK_DOT) {
		lexer_next();
		lexer_next();
		lexer_next();
		*last_index = (int)parser_eval_const_int_expr();
	}

	expect(TOK_RBRACKET);
	expect(TOK_ASSIGN);
	return 1;
}

static int
parser_type_is_pointer_array(const Type *type)
{
	return type && type_is_array(type) && type->base && type_is_pointer(type->base);
}

static void
parse_parenthesized_global_pointer_array_initializer(Global *g, Type *decl_type)
{
	Type *elem_type = decl_type ? decl_type->base : NULL;
	int array_len = decl_type ? type_array_len(decl_type) : 0;
	int next_index = 0;
	int max_index = -1;

	if (!g || !parser_type_is_pointer_array(decl_type) || !elem_type)
		fatal_cur("Unsupported generic global array initializer\n");

	mark_global_initialized(g);

	if (lexer_peek()->kind != TOK_LBRACE) {
		fatal_cur("Unsupported generic global array initializer\n");
	}

	lexer_next(); /* { */

	while (lexer_peek()->kind != TOK_RBRACE) {
		long long init_value = 0;
		char init_sym[64] = {0};
		int first_index = next_index;
		int last_index = next_index;

		if (parser_try_parse_global_array_designator(&first_index, &last_index))
			next_index = last_index + 1;
		else
			next_index++;

		if (first_index < 0 || last_index < first_index)
			fatal_cur("Invalid global array designator range\n");
		if (array_len > 0 && last_index >= array_len)
			fatal_cur("Global array designator index out of range\n");
		/* No artificial limit — init storage is dynamic */

		if (try_parse_null_pointer_constant()) {
			init_value = 0;
		} else if (lexer_peek()->kind == TOK_AMP &&
		           lexer_peek_ahead(1)->kind == TOK_IDENT) {
			validate_global_pointer_initializer_compatibility(elem_type, lexer_peek());
			lexer_next(); /* & */
			STRNCPY(init_sym, lexer_peek()->text ? lexer_peek()->text : "",
			        sizeof(init_sym) - 1);
			lexer_next();
		} else if (lexer_peek()->kind == TOK_IDENT &&
		           (is_global(lexer_peek()->text) || find_func(lexer_peek()->text))) {
			validate_global_pointer_initializer_compatibility(elem_type, lexer_peek());
			STRNCPY(init_sym, lexer_peek()->text ? lexer_peek()->text : "",
			        sizeof(init_sym) - 1);
			lexer_next();
		} else {
			init_value = parse_global_scalar_initializer_value_or_die(
			    "Pointer array initializer must be a constant expression or symbol\n");
		}

		for (int i = first_index; i <= last_index; i++) {
			int offset = i * TCC_SIZEOF_PTR;
			if (init_sym[0])
				store_global_init_symbol(g, offset, TCC_SIZEOF_PTR, init_sym);
			else
				store_global_init_int(g, offset, TCC_SIZEOF_PTR, init_value);
		}

		if (last_index > max_index)
			max_index = last_index;

		if (lexer_peek()->kind == TOK_COMMA)
			lexer_next();
		else
			break;
	}

	expect(TOK_RBRACE);

	if (array_len == 0) {
		array_len = max_index + 1;
		apply_type_to_global(g, type_array(clone_type(elem_type), array_len));
	}
}

static void
parse_symbol_address_global_initializer(Global *g, const char *message,
    int require_known_global, int ptr_elem_size)
{
	char symbol_name[64] = {0};
	const char *resolved_symbol = NULL;
	int addr_offset = 0;
	int explicit_addr = 0;

	if (try_parse_global_pointer_symbol_initializer(symbol_name, sizeof(symbol_name),
	                                                &addr_offset, ptr_elem_size,
	                                                &explicit_addr)) {
		validate_global_pointer_symbol_initializer_compatibility(g ? g->type : NULL,
		                                                         symbol_name,
		                                                         explicit_addr);
		validate_pointer_initializer_source_name_resolved(&resolved_symbol, symbol_name);
		if (require_known_global &&
		    !is_global(resolved_symbol) &&
		    !find_func(resolved_symbol)) {
			fatal_cur(message);
		}
		if (ptr_elem_size > 0)
			g->ptr_elem_size = ptr_elem_size;
		set_global_address_initializer(g, resolved_symbol);
		g->addr_offset = addr_offset;
		return;
	}

	parse_scalar_global_initializer(g, message);
}

static void
parse_string_array_global_initializer(Global *g, const char *message);

static void
parser_handle_parenthesized_generic_global_initializer(Global **pg, Type *decl_type)
{
	Global *g = pg ? *pg : NULL;

	if (!g)
		fatal_cur("Unsupported generic global array initializer\n");

	if (type_is_pointer(decl_type) &&
	    lexer_peek()->kind == TOK_STRING &&
	    decl_type->base && decl_type->base->kind == TY_ARRAY) {
		parse_string_array_global_initializer(g,
		    "Unsupported generic global array initializer\n");
		return;
	}
	if (type_is_pointer(decl_type) &&
	    try_parse_global_addr_array_compound_literal(&g)) {
		if (pg)
			*pg = g;
		return;
	}
	if (type_is_pointer(decl_type) &&
	    try_parse_global_addr_scalar_compound_literal(&g)) {
		if (pg)
			*pg = g;
		return;
	}
	if (type_is_pointer(decl_type) &&
	    try_parse_global_addr_struct_compound_literal(&g)) {
		if (pg)
			*pg = g;
		return;
	}
	if (type_is_pointer(decl_type)) {
		parse_symbol_address_global_initializer(g,
		    "Function pointer initializer must be a constant or function name\n",
		    0, 0);
		if (pg)
			*pg = g;
		return;
	}
	if (parser_type_is_pointer_array(decl_type)) {
		parse_parenthesized_global_pointer_array_initializer(g, decl_type);
		if (pg)
			*pg = g;
		return;
	}
	if (lexer_peek()->kind == TOK_STRING &&
	    decl_type && decl_type->kind == TY_ARRAY) {
		parse_string_array_global_initializer(g,
		    "Unsupported generic global array initializer\n");
		return;
	}

	fatal_cur("Unsupported generic global array initializer\n");
}



static void
parse_required_symbol_address_global_initializer(Global *g, const char *message,
    int require_known_global, int ptr_elem_size, int allow_bare_ident)
{
	const Token *value = lexer_peek();
	const char *resolved_symbol = NULL;

	if (try_parse_null_pointer_constant()) {
		set_global_integer_initializer(g, 0);
		return;
	}

	if (value->kind == TOK_AMP && lexer_peek_ahead(1)->kind == TOK_IDENT) {
		validate_global_pointer_initializer_compatibility(g ? g->type : NULL, value);
		lexer_next();
		const Token *target = lexer_peek();
		validate_pointer_initializer_source_name_resolved(&resolved_symbol, target->text);
		if (require_known_global && !is_global(resolved_symbol)) {
			fatal_cur(message);
		}
		if (ptr_elem_size > 0)
			g->ptr_elem_size = ptr_elem_size;
		set_global_address_initializer(g, resolved_symbol);
		lexer_next();
		/* Handle &name[offset] */
		if (lexer_peek()->kind == TOK_LBRACKET) {
			lexer_next();
			int elem_sz = ptr_elem_size > 0 ? ptr_elem_size : 1;
			long long idx = eval_const_array_size();
			g->addr_offset = (int)(idx * elem_sz);
			expect(TOK_RBRACKET);
		}
		return;
	}

	if (allow_bare_ident && value->kind == TOK_IDENT) {
		validate_global_pointer_initializer_compatibility(g ? g->type : NULL, value);
		validate_pointer_initializer_source_name_resolved(&resolved_symbol, value->text);
		if (require_known_global && !is_global(resolved_symbol)) {
			fatal_cur(message);
		}
		if (ptr_elem_size > 0)
			g->ptr_elem_size = ptr_elem_size;
		set_global_address_initializer(g, resolved_symbol);
		lexer_next();
		return;
	}

	fatal_cur(message);
}




static void
parse_string_pointer_global_initializer(Global *g, const char *message)
{
	const Token *value = lexer_peek();

	if (value->kind != TOK_STRING) {
		fatal_cur(message);
	}

	validate_global_pointer_initializer_compatibility(g ? g->type : NULL, value);
	set_global_string_initializer_len(g, value->text, value->text_len);
	lexer_next();
}

static void
parse_string_array_global_initializer(Global *g, const char *message)
{
	const Token *value = lexer_peek();

	if (value->kind != TOK_STRING) {
		fatal_cur(message);
	}

	set_global_string_array_initializer_len(g, value->text, value->text_len);
	if (g->array_len == 0) {
		int w = value->string_width > 1 ? value->string_width : 1;
		/* For wide strings, text_len is raw bytes; element count = bytes/width + 1. */
		g->array_len = w > 1 ? (int)(value->text_len / (size_t)w) + 1
		                     : (int)value->text_len + 1;
	}
	lexer_next();
}

void
parse_pointer_global_initializer(Global *g, const char *message,
    int require_known_global, int allow_string, int allow_bare_ident)
{
	const Token *value = lexer_peek();

	if (allow_string && value->kind == TOK_STRING) {
		parse_string_pointer_global_initializer(g, message);
		return;
	}

	parse_required_symbol_address_global_initializer(g, message,
	    require_known_global, 0, allow_bare_ident);
}

static int
array_dim_product(const int dims[MAX_ARRAY_DIMS], int start, int dim_count)
{
	int n = 1;
	for (int i = start; i < dim_count; i++) {
		if (dims[i] > 0)
			n *= dims[i];
	}
	return n;
}

static int
try_parse_empty_global_compound_literal(void)
{
	/* Recognize exactly: (typedef_name){} or (_Bool){} etc. */
	if (lexer_peek()->kind != TOK_LPAREN)
		return 0;
	if (!is_type_start_token(lexer_peek_ahead(1)->kind, lexer_peek_ahead(1)->text))
		return 0;
	if (lexer_peek_ahead(2)->kind != TOK_RPAREN ||
	    lexer_peek_ahead(3)->kind != TOK_LBRACE ||
	    lexer_peek_ahead(4)->kind != TOK_RBRACE)
		return 0;

	lexer_next(); /* ( */
	lexer_next(); /* type name */
	expect(TOK_RPAREN);
	expect(TOK_LBRACE);
	expect(TOK_RBRACE);
	return 1;
}

void
parse_global_struct_initializer_body_ex(int g_idx, StructDef *def, int base_offset, int allow_unbraced_end)
{
	globals_ensure_spare(256);
	Global *g = &punit.globals[g_idx];
	int field_index = 0;

	reject_empty_initializer_before_c23();

	while (lexer_peek()->kind != TOK_RBRACE) {
		Field *field = NULL;

		if (lexer_peek()->kind == TOK_DOT) {
			reject_c89_designated_initializer();
			lexer_next();

			const Token *field_tok = lexer_peek();
			if (field_tok->kind != TOK_IDENT) {
				fatal_cur("Expected field name after '.' in global struct initializer\n");
			}

			lexer_next();
			expect(TOK_ASSIGN);
			field = find_field(def->name, field_tok->text);
			{
				int di = field_index_by_name_offset(def, field);
				if (di >= 0 && field_index <= di) {
					field_index = di + 1;
					while (field_index < def->field_count &&
					       field->size > 0 &&
					       def->fields[field_index].size > 0 &&
					       def->fields[field_index].offset == field->offset)
						field_index++;
				}
			}
		} else {
			if (field_index >= def->field_count) {
				if (allow_unbraced_end)
					return;
				fatal_cur("Too many initializers for global struct\n");
			}
			field = &def->fields[field_index++];

			/*
			 * Anonymous unions are flattened into multiple fields at the
			 * same offset. For positional initializers, consume only the
			 * first field in an offset run; the remaining promoted names are
			 * aliases, not extra initializer positions.
			 */
			while (field_index < def->field_count &&
			       field->size > 0 &&
			       def->fields[field_index].size > 0 &&
			       def->fields[field_index].offset == field->offset)
				field_index++;
		}

		const Token *value = lexer_peek();

		/* Optional braces around a scalar field, e.g. { (1) }. */
		if (value->kind == TOK_LBRACE && !field->is_array && !field->is_struct) {
			long long scalar_value = 0;
			double float_value = 0.0;

			lexer_next(); /* consume { */
			if (field->type && parser_type_is_float_storage_scalar(field->type)) {
				float_value = parse_global_floating_initializer_value_for_type_or_die(
				    "Global scalar initializer must contain a constant floating value\n",
				    field->type);
				store_global_init_float(g, base_offset + field->offset, field->size, float_value);
			} else {
				scalar_value = parse_global_scalar_initializer_value_or_die(
				    "Global scalar initializer must contain a constant integer\n");
				store_global_init_int(g, base_offset + field->offset, field->size, scalar_value);
			}
			expect(TOK_RBRACE);

			if (lexer_peek()->kind == TOK_COMMA) {
				lexer_next();
				if (lexer_peek()->kind == TOK_RBRACE)
					break;
			} else {
				break;
			}
			continue;
		}

		/* Nested initializer: { ... } for a struct/union or array field. */
		if (value->kind == TOK_LBRACE) {
			/* Array field: "int sub[2]" initializer "{2, 3}" */
			if (field->is_array) {
				lexer_next(); /* consume { */
				int arr_offset = base_offset + field->offset;
				int arr_elem = 0;
				int arr_max = field->size / (field->elem_size > 0 ? field->elem_size : 1);
				Type *field_elem_type = field->type && field->type->kind == TY_ARRAY
				                      ? field->type->base : NULL;
				int field_array_is_float = parser_type_is_float_storage_scalar(field_elem_type);
				while (lexer_peek()->kind != TOK_RBRACE) {
					long long av_val = 0;
					double av_float = 0.0;
					int braced_scalar = 0;
					int first_elem = arr_elem;
					int last_elem = arr_elem;
					int has_designator = 0;

					/* Designated array element: {[0] = 1, 1+1}. */
					if (parser_try_parse_global_array_designator(&first_elem, &last_elem)) {
						arr_elem = first_elem;
						has_designator = 1;
					}

					/*
					 * Accept parenthesized constants and optional braces around
					 * individual array elements in aggregate initializers, e.g.
					 *
					 *     { (((3))), {4} }
					 */
					if (lexer_peek()->kind == TOK_LBRACE) {
						braced_scalar = 1;
						lexer_next();
					}

					if (field_array_is_float) {
						av_float = parse_global_floating_initializer_value_for_type_or_die(
						    "Global array field initializer must contain constant floating values\n",
						    field_elem_type);
					} else {
						av_val = parse_global_scalar_initializer_value_or_die(
						    "Global array field initializer must contain constant integers\n");
					}

					if (braced_scalar)
						expect(TOK_RBRACE);

					if (first_elem < 0 || last_elem < first_elem)
						fatal_cur("Invalid global array designator range\n");
					if (has_designator && last_elem >= arr_max)
						fatal_cur("Global array designator index out of range\n");
					for (int di = first_elem; di <= last_elem && di < arr_max; di++) {
						if (field_array_is_float)
							store_global_init_float(g, arr_offset + di * field->elem_size,
							                        field->elem_size, av_float);
						else
							store_global_init_int(g, arr_offset + di * field->elem_size,
							                     field->elem_size, av_val);
					}
					arr_elem = last_elem + 1;
					if (lexer_peek()->kind == TOK_COMMA) lexer_next(); else break;
				}
				expect(TOK_RBRACE);
				if (lexer_peek()->kind == TOK_COMMA) {
					lexer_next();
					if (lexer_peek()->kind == TOK_RBRACE) break;
				} else break;
				continue;
			}
			/* Skip over duplicate-offset union members to find the struct field. */
			while (!field->is_struct && field_index < def->field_count) {
				Field *nf = &def->fields[field_index];
				if (nf->is_struct) { field = nf; field_index++; break; }
				field_index++;
			}
			if (field->is_struct && field->struct_name[0]) {
				StructDef *nested = find_struct_or_null(field->struct_name);
				if (nested) {
					lexer_next(); /* consume { */
					parse_global_struct_initializer_body_ex(g_idx, nested, base_offset + field->offset, 0);
					g = &punit.globals[g_idx];
					expect(TOK_RBRACE);
					if (lexer_peek()->kind == TOK_COMMA) {
						lexer_next();
						if (lexer_peek()->kind == TOK_RBRACE)
							break;
					} else {
						break;
					}
					continue;
				}
			}
		}

		/* Brace elision for nested struct fields inside aggregate initializers.
		 * Example:
		 *     struct U { unsigned char a; struct S s; unsigned char b; };
		 *     struct U gu = { 3, 5,6,7,8, 4 };
		 * The values 5,6,7,8 initialize the nested struct field. */
		if (field->is_struct && field->struct_name[0] &&
		    value->kind != TOK_LBRACE &&
		    value->kind != TOK_LPAREN &&
		    value->kind != TOK_RBRACE &&
		    value->kind != TOK_COMMA) {
			StructDef *nested = find_struct_or_null(field->struct_name);
			if (nested) {
				parse_global_struct_initializer_body_ex(g_idx, nested,
				                                        base_offset + field->offset,
				                                        1);
				g = &punit.globals[g_idx];

				if (lexer_peek()->kind == TOK_COMMA) {
					lexer_next();
					if (lexer_peek()->kind == TOK_RBRACE)
						break;
				} else {
					break;
				}
				continue;
			}
		}

		/* Brace elision for array fields inside aggregate initializers.
		 * Example:
		 *     struct S { unsigned char a, b, c[2]; };
		 *     struct S gs = { 1, 2, 3, 4 };
		 * The final two scalars initialize c[0] and c[1]. */
		if (field->is_array &&
		    !(value->kind == TOK_STRING &&
		      field->type && field->type->kind == TY_ARRAY &&
		      field->type->base && field->type->base->kind == TY_CHAR)) {
			int arr_offset = base_offset + field->offset;
			int arr_elem = 0;
			int arr_max = field->size / (field->elem_size > 0 ? field->elem_size : 1);
			int parsed_array_values = 0;
			Type *field_elem_type = field->type && field->type->kind == TY_ARRAY
			                      ? field->type->base : NULL;
			int field_array_is_float = parser_type_is_float_storage_scalar(field_elem_type);

			while (arr_elem < arr_max && lexer_peek()->kind != TOK_RBRACE) {
				long long av_val = 0;
				double av_float = 0.0;

				if (field_array_is_float) {
					av_float = parse_global_floating_initializer_value_for_type_or_die(
					    "Global array field initializer must contain constant floating values\n",
					    field_elem_type);
				} else {
					av_val = parse_global_scalar_initializer_value_or_die(
					    "Global array field initializer must contain constant integers\n");
				}

				if (field_array_is_float)
					store_global_init_float(g,
					                      arr_offset + arr_elem * field->elem_size,
					                      field->elem_size,
					                      av_float);
				else
					store_global_init_int(g,
					                      arr_offset + arr_elem * field->elem_size,
					                      field->elem_size,
					                      av_val);
				arr_elem++;
				parsed_array_values = 1;

				if (lexer_peek()->kind == TOK_COMMA) {
					/*
					 * In an unbraced aggregate initializer, a comma after the
					 * final element of an array field usually separates the next
					 * field in the same struct.  Leave it for the caller only if
					 * this array field is also the final field of the struct; then
					 * the comma separates the containing array's next struct.
					 *
					 * Example:
					 *   typedef struct { long c[4]; long b,e,k; } PT;
					 *   PT a[] = { c0,c1,c2,c3, b,e,k, ... };
					 * The comma after c3 must be consumed so b can be parsed.
					 */
					if (allow_unbraced_end && arr_elem >= arr_max &&
					    field_index >= def->field_count)
						break;
					lexer_next();
					if (lexer_peek()->kind == TOK_RBRACE)
						break;
				} else {
					break;
				}
			}

			if (parsed_array_values)
				continue;
		}

		if (field->is_struct && value->kind == TOK_LPAREN &&
		    try_parse_empty_global_compound_literal()) {
			/* Empty struct compound literal, e.g. (empty_s){}.  It contributes no bytes. */
		} else if (field->is_struct && value->kind == TOK_LPAREN && field->struct_name[0]) {
			StructDef *nested = find_struct_or_null(field->struct_name);
			if (!nested ||
			    !try_parse_global_struct_compound_initializer(g_idx, nested,
			                                           field->struct_name,
			                                           base_offset + field->offset)) {
				fatal_cur("Invalid global aggregate compound literal initializer\n");
			}
			g = &punit.globals[g_idx]; /* re-derive: compound_initializer may have grown punit.globals[] */
		} else if (try_parse_null_pointer_constant()) {
			store_global_init_int(g, base_offset + field->offset, field->size, 0);
		} else {
			long long scalar_value = 0;
			double float_value = 0.0;
			if (field->type && parser_type_is_float_storage_scalar(field->type)) {
				float_value = parse_global_floating_initializer_value_for_type_or_die(
				    "Global scalar initializer must contain a constant floating value\n",
				    field->type);
				store_global_init_float(g, base_offset + field->offset, field->size, float_value);
			} else if (field->type &&
			           field->type->kind != TY_PTR &&
			           field->type->kind != TY_ARRAY) {
				scalar_value = parse_global_scalar_initializer_value_or_die(
				    "Global struct initializer must contain constant integers or char-array strings\n");
				store_global_init_int(g, base_offset + field->offset, field->size, scalar_value);
			} else if (value->kind == TOK_IDENT) {
					/* function pointer or global address initializer — store symbol */
					validate_global_pointer_initializer_compatibility(field->type, value);
					store_global_init_symbol(g, base_offset + field->offset,
					                         field->size > 0 ? field->size : 8,
					                         value->text);
				lexer_next();
			} else if (value->kind == TOK_STRING) {
				if (field->type && field->type->kind == TY_ARRAY &&
				    field->type->base && field->type->base->kind == TY_CHAR) {
					/* char[] field: store inline */
					store_global_init_string(g, base_offset + field->offset, field->size, value->text);
				} else {
					/* char* pointer field: create anon string global, store its address.
					 * Use new_global_slot (zeroes + grows) rather than raw pointer
					 * arithmetic which would access uninitialized memory. */
					char str_gname2[64];
					snprintf(str_gname2, sizeof(str_gname2), "__str_%d", parser_alloc_string_label());
					{
						int g_idx = parser_global_index(g);
						if (g_idx == punit.global_count)
							parser_commit_reserved_global();
						globals_ensure_spare(1);
						g = parser_global_at(g_idx);
						Global *sg2 = new_global_slot(str_gname2);
						set_global_string_array_initializer_len(sg2, value->text,
						                                       value->text_len);
						sg2->elem_size = 1;
						sg2->array_len = (int)value->text_len + 1;
						global_set_init_count(sg2, sg2->array_len);
						parser_commit_reserved_global();
						g = parser_global_at(g_idx);
					}
					store_global_init_symbol(g, base_offset + field->offset, 8, str_gname2);
				}
				lexer_next();
			} else if (value->kind == TOK_AMP && lexer_peek_ahead(1)->kind == TOK_IDENT) {
				/* &symbol — pointer initializer */
				validate_global_pointer_initializer_compatibility(field->type, value);
				lexer_next(); /* consume & */
				const Token *target = lexer_peek();
			lexer_next(); /* consume symbol name */
			/* Handle optional [index] after &symbol */
			int byte_off = 0;
			if (lexer_peek()->kind == TOK_LBRACKET) {
				lexer_next();
				long long idx = eval_const_array_size();
				int esz = field->elem_size > 0 ? field->elem_size : 1;
				byte_off = (int)(idx * esz);
				expect(TOK_RBRACKET);
			}
			(void)byte_off;
			store_global_init_symbol(g, base_offset + field->offset,
			                         field->size > 0 ? field->size : 8,
			                         target->text);
		} else if (field->type && field->type->kind == TY_PTR) {
			char symbol_name[64] = {0};
			int addr_offset = 0;
			int explicit_addr = 0;
			long long scalar_value = 0;

			validate_global_pointer_initializer_compatibility(field->type, value);
			if (try_parse_global_pointer_symbol_initializer(symbol_name, sizeof(symbol_name),
			                                                &addr_offset, field->elem_size,
			                                                &explicit_addr)) {
				validate_global_pointer_symbol_initializer_compatibility(field->type,
				                                                         symbol_name,
				                                                         explicit_addr);
				store_global_init_symbol(g, base_offset + field->offset,
				                         field->size > 0 ? field->size : TCC_SIZEOF_PTR,
				                         symbol_name);
				(void)addr_offset;
			} else {
				scalar_value = parse_global_scalar_initializer_value_or_die(
				    "Global struct initializer must contain constant integers or char-array strings\n");
				store_global_init_int(g, base_offset + field->offset, field->size, scalar_value);
			}
		} else {
			fatal_cur("Global struct initializer must contain constant integers or char-array strings\n");
		}
	}

		/*
		 * For unbraced nested aggregate parsing, do not consume the
		 * comma after the nested struct's final field.  That comma is the
		 * separator in the containing initializer.
		 */
		if (allow_unbraced_end && field_index >= def->field_count)
			return;

		if (lexer_peek()->kind == TOK_COMMA) {
			lexer_next();
			if (lexer_peek()->kind == TOK_RBRACE)
				break;
		} else {
			break;
		}
	}
}

void
parse_global_struct_initializer_body(int g_idx, StructDef *def, int base_offset)
{
	parse_global_struct_initializer_body_ex(g_idx, def, base_offset, 0);
}

int 
find_local(const char *name)
{
	Local *local = parser_require_local_latest(name);
	return local->offset;
}

Local *
parser_find_local_info_latest(const char *name)
{
	return parser_find_local_latest_optional(name);
}

void
parser_configure_last_local_scalar(int elem_size, int is_unsigned)
{
	Local *local;

	if (pscope.local_count <= 0)
		ICE("no local available to configure");
	local = &pscope.locals[pscope.local_count - 1];
	local->elem_size = elem_size;
	local->type = type_for_size_unsigned(elem_size, is_unsigned);
	parser_debug_local_set_type(local->offset, local->type);
}

void
parser_configure_last_local_type(Type *type)
{
	Local *local;

	if (pscope.local_count <= 0)
		ICE("no local available to configure");
	local = &pscope.locals[pscope.local_count - 1];
	local->type = clone_type(type);
	local->elem_size = type ? type_elem_size(type) : TCC_SIZEOF_INT;
	parser_debug_local_set_type(local->offset, local->type);
}

static Type *
parser_resolve_typedef_pointee_type(const Type *base)
{
	const char *source_name;
	Type *typedef_type;

	if (!base || !type_source_is_typedef(base))
		return NULL;

	source_name = type_source_name(base);
	if (!source_name || !source_name[0])
		return NULL;

	typedef_type = parser_find_typedef(source_name);
	return typedef_type ? clone_type(typedef_type) : NULL;
}

Type *
parser_canonicalize_pointer_type(Type *type, int elem_size, const char *struct_name)
{
	Type *base;
	Type *canonical_base = NULL;
	StructDef *def;
	int qualifiers;

	if (!type || !type_is_pointer(type))
		return clone_type(type);

	base = type_pointee(type);
	if (!base)
		return clone_type(type);

	qualifiers = type_qualifiers(base);

	if (!type_is_void(base)) {
		switch (base->kind) {
		case TY_INT:
			canonical_base = type_for_size_unsigned(base->size ? base->size : TCC_SIZEOF_INT,
			                                        type_is_unsigned(base));
			break;
		case TY_CHAR:
			canonical_base = type_is_unsigned(base) ? type_uchar() : type_char();
			if (type_source_is(base, TYPE_SOURCE_SCHAR))
				canonical_base = type_with_source(canonical_base,
				                                  TYPE_SOURCE_SCHAR,
				                                  "signed char");
			break;
		case TY_SHORT:
			canonical_base = type_is_unsigned(base) ? type_ushort() : type_short();
			break;
		case TY_FLOAT:
			canonical_base = type_float();
			break;
		case TY_DOUBLE:
			canonical_base = type_double();
			break;
		case TY_STRUCT:
			canonical_base = type_struct(struct_name && struct_name[0] ? struct_name : base->struct_name,
			                             base->size);
			break;
		case TY_UNION:
			canonical_base = type_union(struct_name && struct_name[0] ? struct_name : base->struct_name,
			                            base->size);
			break;
		case TY_ENUM:
			canonical_base = type_enum(base->struct_name);
			break;
		case TY_PTR: {
			Type *nested_base = type_pointee(base);
			const char *nested_struct_name = "";
			if (nested_base && (type_is_struct(nested_base) || type_is_union(nested_base)))
				nested_struct_name = parser_resolve_struct_type_name(nested_base);
			canonical_base = parser_canonicalize_pointer_type((Type *)base,
			                                                 nested_base ? nested_base->size : 0,
			                                                 nested_struct_name);
			break;
		}
		default:
			return clone_type(type);
		}
	} else {
		if (type_is_void(base))
			return clone_type(type);

		canonical_base = parser_resolve_typedef_pointee_type(base);

		if (!canonical_base && struct_name && struct_name[0]) {
			def = find_struct_or_null(struct_name);
			if (def && def->is_union)
				canonical_base = type_union(struct_name, def->size);
			else
				canonical_base = type_struct(struct_name, elem_size > 0 ? elem_size : 0);
		} else if (!canonical_base && elem_size > 0) {
			canonical_base = type_for_size_unsigned(elem_size, type_is_unsigned(base));
		}
	}

	if (!canonical_base)
		return clone_type(type);

	canonical_base = type_with_qualifiers(canonical_base, qualifiers);
	if (!type_source_is_typedef(base) &&
	    (base->source_kind != TYPE_SOURCE_DEFAULT || base->source_name[0]))
		canonical_base = type_with_source(canonical_base,
		                                  base->source_kind,
		                                  base->source_name);
	if (type_source_is_typedef(base) && type_source_name(base)[0])
		canonical_base = type_with_source(canonical_base, TYPE_SOURCE_TYPEDEF,
		                                  type_source_name(base));
	return type_ptr(canonical_base);
}

Type *
parser_canonicalize_decl_type(Type *type)
{
	Type *base;
	Type *dst = NULL;
	Type **param_types = NULL;
	Type **param_copies = NULL;
	int param_count = 0;
	int is_variadic = 0;
	int fixed_param_count = 0;
	const char *struct_name = "";
	int elem_size = 0;

	if (!type)
		return NULL;

	switch (type->kind) {
	case TY_VOID:
		dst = type_void();
		break;
	case TY_INT:
		dst = type_for_size_unsigned(type->size ? type->size : TCC_SIZEOF_INT,
		                             type_is_unsigned(type));
		break;
	case TY_CHAR:
		dst = type_is_unsigned(type) ? type_uchar() : type_char();
		if (type_source_is(type, TYPE_SOURCE_SCHAR))
			dst = type_with_source(dst, TYPE_SOURCE_SCHAR, "signed char");
		break;
	case TY_SHORT:
		dst = type_is_unsigned(type) ? type_ushort() : type_short();
		break;
	case TY_FLOAT:
		dst = type_float();
		break;
	case TY_DOUBLE:
		dst = type_double();
		break;
	case TY_STRUCT:
		dst = type_struct(parser_resolve_struct_type_name(type), type->size);
		break;
	case TY_UNION:
		dst = type_union(parser_resolve_struct_type_name(type), type->size);
		break;
	case TY_ENUM:
		dst = type_enum(type->struct_name);
		break;
	case TY_PTR:
		base = type_pointee(type);
		if (base) {
			elem_size = base->size;
			if (type_is_struct(base) || type_is_union(base))
				struct_name = parser_resolve_struct_type_name(base);
		}
		dst = parser_canonicalize_pointer_type(type, elem_size, struct_name);
		break;
	case TY_ARRAY:
		dst = type_array(parser_canonicalize_decl_type(type->base), type->array_len);
		dst->is_vm_type = type->is_vm_type;
		if (type->vla_bound_name[0])
			STRNCPY(dst->vla_bound_name, type->vla_bound_name,
			        sizeof(dst->vla_bound_name) - 1);
		dst->vla_elem_type = type->vla_elem_type
		                   ? parser_canonicalize_decl_type(type->vla_elem_type)
		                   : NULL;
		break;
	case TY_FUNC:
		if (type_func_metadata(type, &param_types, &param_count,
		                       &is_variadic, &fixed_param_count)) {
			if (param_count > 0) {
				param_copies = xcalloc((size_t)param_count, sizeof(Type *));
				for (int i = 0; i < param_count; i++)
					param_copies[i] = parser_canonicalize_decl_type(param_types[i]);
			}
			dst = type_func_proto(parser_canonicalize_decl_type(type->base),
			                      param_copies, param_count,
			                      is_variadic, fixed_param_count);
		} else {
			dst = type_func(parser_canonicalize_decl_type(type->base));
		}
		break;
	default:
		return clone_type(type);
	}

	dst = type_with_qualifiers(dst, type->qualifiers);
	if (type->source_kind != TYPE_SOURCE_DEFAULT || type->source_name[0])
		dst = type_with_source(dst, type->source_kind, type->source_name);
	return dst;
}

static Type *
parser_rebuild_struct_array_field_type(const Type *type, const char *struct_name, int elem_size)
{
	Type *rebuilt;

	if (!type)
		return NULL;

	if (type_is_array(type)) {
		Type *base = parser_rebuild_struct_array_field_type(type->base, struct_name, elem_size);
		rebuilt = type_array(base ? base : type_struct(struct_name, elem_size), type->array_len);
	} else {
		rebuilt = type_struct(struct_name, elem_size);
	}

	if (type->qualifiers)
		rebuilt = type_with_qualifiers(rebuilt, type->qualifiers);
	if (type->source_kind != TYPE_SOURCE_DEFAULT || type->source_name[0])
		rebuilt = type_with_source(rebuilt, type->source_kind, type->source_name);
	return rebuilt;
}

int 
add_local_sized(const char *name, int slots, int is_array)
{
	int bytes = slots * 4;
	int current_scope_local;

	parser_reject_scope_typedef_name(name);
	current_scope_local = parser_find_local_in_current_scope_optional(name);
	if (current_scope_local >= 0 &&
	    pscope.locals[current_scope_local].is_function_decl) {
		fatal_cur("identifier '%s' conflicts with an existing identifier in the same scope\n",
		          name);
	}

	/* Check only within the current scope level (pscope.locals visible from saved_local_count
	 * to pscope.local_count) for duplicates. Same-name variables in sibling blocks are valid C. */
	for (int i = 0; i < pscope.local_count; i++) {
		if (STRCMP(pscope.locals[i].name, name) == 0 && !pscope.locals[i].is_static) {
			/* allow redeclaration if it's a shadow of an outer scope - only error
			 * if declared at the exact same block depth, which we can't easily track.
			 * For now, silently allow shadowing to match C semantics. */
			break;
		}
	}

	pscope.stack_size += bytes;
	if (parser_decl_align_request > 0)
		pscope.stack_size = align_to(pscope.stack_size, parser_decl_align_request);

	Local *l = locals_push();
	STRNCPY(l->name, name, sizeof(l->name) - 1);
	l->offset = -pscope.stack_size;
	l->is_array = is_array;
	l->array_len = slots;
	l->align = parser_decl_align_request > 0 ? parser_decl_align_request : 0;
	l->is_pointer = 0;
	l->is_function_decl = 0;
	l->is_vla = 0;
	l->is_vm_type = 0;
	l->elem_size = TCC_SIZEOF_INT;
	l->is_struct = 0;
	l->struct_name[0] = '\0';
	l->vla_bound_name[0] = '\0';
	l->vla_stack_name[0] = '\0';
	l->vla_stack_offset = 0;
	l->type = type_int();
	l->vla_elem_type = NULL;
	l->struct_by_ref = 0;
	l->is_static = 0;              /* clear stale flag from previous function */
	l->is_register = parser_decl_register_request;
	l->static_global_name[0] = '\0';

	parser_debug_local_add(name, l->offset);

	return -pscope.stack_size;
}

static int
add_local_sized_with_min_align(const char *name, int slots, int is_array, int min_align)
{
	int saved_align = parser_decl_align_request;

	if (min_align > parser_decl_align_request)
		parser_decl_align_request = min_align;
	{
		int offset = add_local_sized(name, slots, is_array);
		parser_decl_align_request = saved_align;
		return offset;
	}
}

int 
add_local(const char *name)
{
	return add_local_sized(name, 1, 0);
}

int 
add_static_local(const char *name, const char *global_name, Type *type, int elem_size, int is_array, int array_len, int align)
{
	int current_scope_local;

	parser_validate_decl_alignment(align, type);
	parser_reject_scope_typedef_name(name);
	current_scope_local = parser_find_local_in_current_scope_optional(name);
	if (current_scope_local >= 0 &&
	    pscope.locals[current_scope_local].is_function_decl) {
		fatal_cur("identifier '%s' conflicts with an existing identifier in the same scope\n",
		          name);
	}

	for (int i = 0; i < pscope.local_count; i++) {
		if (STRCMP(pscope.locals[i].name, name) == 0) {
			/* Same static local re-declared (e.g. in multiple branches of a
			 * large function like sqlite3Pragma) — treat as same storage. */
			return 0;
		}
	}

	Local *l = locals_push();
	STRNCPY(l->name, name, sizeof(l->name) - 1);
	l->offset = 0;
	l->is_array = is_array;
	l->array_len = array_len;
	l->align = align > 0 ? align : 0;
	l->is_pointer = type ? type_is_pointer(type) : 0;
	l->is_function_decl = 0;
	l->is_vla = 0;
	l->is_vm_type = 0;
	l->elem_size = elem_size;
	l->is_struct = type ? type_is_struct(type) : 0;
	l->struct_name[0] = '\0';
	if (type) {
		if (type_is_struct(type) && type->struct_name[0]) {
			STRNCPY(l->struct_name, type->struct_name, sizeof(l->struct_name) - 1);
		} else if (type_is_pointer(type) && type_pointee(type) &&
		           type_is_struct(type_pointee(type)) &&
		           type_pointee(type)->struct_name[0]) {
			STRNCPY(l->struct_name, type_pointee(type)->struct_name,
			        sizeof(l->struct_name) - 1);
		} else if (type_is_array(type) && type_pointee(type) &&
		           type_is_struct(type_pointee(type)) &&
		           type_pointee(type)->struct_name[0]) {
			STRNCPY(l->struct_name, type_pointee(type)->struct_name,
			        sizeof(l->struct_name) - 1);
		}
	}
	l->vla_bound_name[0] = '\0';
	l->vla_stack_name[0] = '\0';
	l->vla_stack_offset = 0;
	l->type = type ? clone_type(type) : NULL;
	l->vla_elem_type = NULL;
	l->struct_by_ref = 0;
	l->is_static = 1;
	l->is_register = 0;
	STRNCPY(l->static_global_name, global_name, sizeof(l->static_global_name) - 1);
	return 0;
}

int 
is_static_local(const char *name)
{
	Local *local = parser_find_local_latest_optional(name);
	return local && local->is_static;
}

const char *
static_global_name_local(const char *name)
{
	Local *local = parser_find_local_latest_optional(name);
	if (local && local->is_static)
		return local->static_global_name;
	return "";
}

int 
static_local_elem_size(const char *name)
{
	Local *local = parser_find_local_latest_optional(name);
	if (local && local->is_static)
		return local->elem_size ? local->elem_size : 4;
	return 4;
}

int 
is_static_array_local(const char *name)
{
	Local *local = parser_find_local_latest_optional(name);
	if (local && local->is_static)
		return local->is_array;
	return 0;
}

int 
add_char_local(const char *name)
{
	int offset = add_local_sized(name, 1, 0);

	for (int i = pscope.local_count - 1; i >= 0; i--) {
		if (STRCMP(pscope.locals[i].name, name) == 0) {
			pscope.locals[i].elem_size = 1;
			pscope.locals[i].type = type_char();
			parser_debug_local_set_type(offset, pscope.locals[i].type);
			return offset;
		}
	}

	return offset;
}

static int 
add_array_local(const char *name, int len, int elem_size)
{
	if (len <= 0) {
		fatal_cur("Array length must be positive: %s\n", name);
	}

	int bytes = len * elem_size;
	int slots = (bytes + 3) / 4;
	int offset = add_local_sized(name, slots, 1);

	for (int i = pscope.local_count - 1; i >= 0; i--) {
		if (STRCMP(pscope.locals[i].name, name) == 0) {
			pscope.locals[i].array_len = len;
			pscope.locals[i].elem_size = elem_size;
			pscope.locals[i].type = type_array(elem_size == 1 ? type_char() : type_int(), len);
			parser_debug_local_set_type(offset, pscope.locals[i].type);
			return offset;
		}
	}

	return offset;
}

int
add_pointer_local(const char *name, int elem_size)
{
	int offset = add_local_sized_with_min_align(name, 2, 0, TCC_SIZEOF_PTR);
	Local *local = parser_last_local_if_matches(name, offset);

	if (local) {
		local->is_pointer = 1;
		local->elem_size = elem_size;
		local->type = type_ptr(type_for_size(elem_size));
		parser_debug_local_set_type(offset, local->type);
		return offset;
	}

	return offset;
}

void
parser_override_local_type(const char *name, int offset, Type *type, int elem_size)
{
	for (int i = pscope.local_count - 1; i >= 0; i--) {
		if (STRCMP(pscope.locals[i].name, name) == 0 &&
		    pscope.locals[i].offset == offset) {
			pscope.locals[i].type = clone_type(type);
			if (elem_size > 0)
				pscope.locals[i].elem_size = elem_size;
			parser_debug_local_set_type(offset, pscope.locals[i].type);
			return;
		}
	}
}

void
parser_mark_local_vla(const char *name, const char *bound_name,
                      const char *stack_name, int stack_offset,
                      Type *elem_type, int elem_size)
{
	for (int i = pscope.local_count - 1; i >= 0; i--) {
		if (STRCMP(pscope.locals[i].name, name) == 0) {
			pscope.locals[i].is_vla = 1;
			pscope.locals[i].is_vm_type = 1;
			pscope.locals[i].is_array = 0;
			pscope.locals[i].is_pointer = 1;
			pscope.locals[i].elem_size = elem_size > 0 ? elem_size : 4;
			if (bound_name && bound_name[0])
				STRNCPY(pscope.locals[i].vla_bound_name, bound_name,
				        sizeof(pscope.locals[i].vla_bound_name) - 1);
			if (stack_name && stack_name[0])
				STRNCPY(pscope.locals[i].vla_stack_name, stack_name,
				        sizeof(pscope.locals[i].vla_stack_name) - 1);
			pscope.locals[i].vla_stack_offset = stack_offset;
			pscope.locals[i].vla_elem_type = elem_type ? clone_type(elem_type) : NULL;
			return;
		}
	}

	fatal_cur("Undefined variable: %s\n", name);
}

void
parser_mark_local_vm_type(const char *name, const char *bound_name,
                          Type *elem_type, int elem_size)
{
	for (int i = pscope.local_count - 1; i >= 0; i--) {
		if (STRCMP(pscope.locals[i].name, name) == 0) {
			pscope.locals[i].is_vm_type = 1;
			pscope.locals[i].elem_size = elem_size > 0 ? elem_size : 4;
			if (bound_name && bound_name[0])
				STRNCPY(pscope.locals[i].vla_bound_name, bound_name,
				        sizeof(pscope.locals[i].vla_bound_name) - 1);
			pscope.locals[i].vla_elem_type = elem_type ? clone_type(elem_type) : NULL;
			return;
		}
	}

	fatal_cur("Undefined variable: %s\n", name);
}


int add_typed_local(const char *name, Type *type)
{
	int object_size;

	if (!type)
		return add_local(name);

	reject_void_object_type(type, "variable");
	reject_incomplete_object_type(type, "variable");
	reject_flexible_array_member_array_object_type(type, "variable");
	object_size = type_sizeof(type);

	if (type->kind == TY_CHAR) {
		int offset = add_char_local(name);
		parser_override_local_type(name, offset, type, type_elem_size(type));
		return offset;
	}

	if (type->kind == TY_PTR) {
		Type *vm_base = NULL;
		int elem_size = type->base ? type_sizeof(type->base) : 4;
		int offset = add_pointer_local(name, elem_size);
		const char *pointee_struct_name = "";

		if (type->base &&
		    type->base->kind == TY_ARRAY &&
		    type->base->is_vm_type &&
		    type->base->vla_bound_name[0]) {
			vm_base = type->base;
			elem_size = type_sizeof(vm_base->vla_elem_type ? vm_base->vla_elem_type
			                                               : vm_base->base);
			if (elem_size <= 0)
				elem_size = 4;
		}

		if (type->base && type_is_struct(type->base)) {
			pointee_struct_name = type->base->struct_name[0]
			                      ? type->base->struct_name
			                      : parser_resolve_struct_type_name(type->base);
		} else if (type->base && type->base->kind == TY_PTR &&
		           type->base->base && type_is_struct(type->base->base)) {
			/* Double-pointer: Foo** — use the underlying struct name */
			pointee_struct_name = type->base->base->struct_name[0]
			                      ? type->base->base->struct_name
			                      : parser_resolve_struct_type_name(type->base->base);
		}

		/* Search backward to find the most-recently added local. */
		for (int i = pscope.local_count - 1; i >= 0; i--) {
			if (STRCMP(pscope.locals[i].name, name) == 0) {
				pscope.locals[i].type = parser_canonicalize_pointer_type(type, elem_size,
				                                                       pointee_struct_name);
				pscope.locals[i].is_pointer = 1;
				pscope.locals[i].elem_size = elem_size;
				if (vm_base) {
					parser_mark_local_vm_type(name, vm_base->vla_bound_name,
					                          vm_base->vla_elem_type ? vm_base->vla_elem_type
					                                                 : vm_base->base,
					                          elem_size);
				}
				if (type->base && type_is_struct(type->base)) {
					STRNCPY(pscope.locals[i].struct_name, pointee_struct_name, sizeof(pscope.locals[i].struct_name) - 1);
				}
				parser_debug_local_set_type(offset, pscope.locals[i].type);
				return offset;
			}
		}

		return offset;
	}

	if (type->kind == TY_ARRAY) {
		int bytes = object_size;
		int slots = (bytes + 3) / 4;
		int offset = add_local_sized_with_min_align(name, slots, 1, type_alignof(type));

		/* Search backward to find the most-recently added local. */
		for (int i = pscope.local_count - 1; i >= 0; i--) {
			if (STRCMP(pscope.locals[i].name, name) == 0) {
				Type *array_base = parser_canonicalize_decl_type(type->base);
				pscope.locals[i].type = array_base ? type_array(array_base, type->array_len)
				                                   : clone_type(type);
				pscope.locals[i].array_len = type->array_len;
				pscope.locals[i].elem_size = type->base ? type_sizeof(type->base)
				                                        : object_size;
				if (type->base && type_is_struct(type->base)) {
					int n=sizeof(pscope.locals[i].struct_name) - 1;
					STRNCPY(pscope.locals[i].struct_name, type->base->struct_name, n);
				}
				parser_debug_local_set_type(offset, pscope.locals[i].type);
				return offset;
			}
		}

		return offset;
	}

	if (type_is_struct(type)) {
		return add_struct_local(name, type->struct_name);
	}

	if (object_size > 4) {
		int offset = add_local_sized_with_min_align(name, (object_size + 3) / 4, 0,
		                                            type_alignof(type));
		for (int i = pscope.local_count - 1; i >= 0; i--) {
			if (STRCMP(pscope.locals[i].name, name) == 0) {
				pscope.locals[i].elem_size = object_size;
				pscope.locals[i].type = clone_type(type);
				parser_debug_local_set_type(offset, pscope.locals[i].type);
				return offset;
			}
		}
		return offset;
	}

	{
		int offset = add_local(name);
		for (int i = pscope.local_count - 1; i >= 0; i--) {
			if (STRCMP(pscope.locals[i].name, name) == 0) {
				pscope.locals[i].elem_size = object_size ? object_size : TCC_SIZEOF_INT;
				pscope.locals[i].type = clone_type(type);
				parser_debug_local_set_type(offset, pscope.locals[i].type);
				return offset;
			}
		}
		return offset;
	}
}

int 
add_typed_array_local(const char *name, Type *base_type, int len)
{
	reject_void_object_type(base_type, "array");
	if (type_contains_flexible_array_member_aggregate(base_type))
		fatal_cur("variable cannot be array of type with flexible array member\n");
	int elem_size = type_elem_size(base_type);
	int offset = add_array_local(name, len, elem_size);

	for (int i = pscope.local_count - 1; i >= 0; i--) {
		if (STRCMP(pscope.locals[i].name, name) == 0) {
			Type *array_base = parser_canonicalize_decl_type(base_type);
			pscope.locals[i].type = type_array(array_base ? array_base : clone_type(base_type), len);
			pscope.locals[i].elem_size = elem_size;
			parser_debug_local_set_type(offset, pscope.locals[i].type);
			return offset;
		}
	}

	return offset;
}

static int 
is_pointer_local(const char *name)
{
	for (int i = pscope.local_count - 1; i >= 0; i--) {
		if (STRCMP(pscope.locals[i].name, name) == 0)
			return pscope.locals[i].is_pointer;
	}

	fatal_cur("Undefined variable: %s\n", name);
}

int
is_pointer_local_optional(const char *name)
{
	Local *local = parser_find_local_latest_optional(name);
	return local ? local->is_pointer : 0;
}

Type *
type_local(const char *name)
{
	Local *local = parser_require_local_latest(name);
	return local->type ? local->type : type_int();
}

Type *
type_local_optional(const char *name)
{
	Local *local = parser_find_local_latest_optional(name);
	if (!local)
		return NULL;
	return local->type ? local->type : type_int();
}

int 
elem_size_local(const char *name)
{
	Local *local = parser_require_local_latest(name);
	return local->elem_size ? local->elem_size : 4;
}

int 
is_array_local(const char *name)
{
	Local *local = parser_find_local_latest_optional(name);
	return local ? local->is_array : 0;
}

int
is_register_local(const char *name)
{
	Local *local = parser_find_local_latest_optional(name);
	return local ? local->is_register : 0;
}

int
is_vla_local(const char *name)
{
	Local *local = parser_find_local_latest_optional(name);
	return local ? local->is_vla : 0;
}

int
is_vm_local(const char *name)
{
	Local *local = parser_find_local_latest_optional(name);
	return local ? local->is_vm_type : 0;
}

const char *
vla_bound_name_local(const char *name)
{
	Local *local = parser_require_local_latest(name);
	return local->vla_bound_name;
}

Type *
vla_elem_type_local(const char *name)
{
	Local *local = parser_require_local_latest(name);
	return local->vla_elem_type;
}

StructDef *
parser_builtin_abi_struct_or_null(const char *name)
{
	static StructDef arm64_complex_float2 = {
		.name = "__tcc_hfa_complex_float2",
		.fields = NULL,
		.field_count = 0,
		.field_cap = 0,
		.size = 8,
		.align = 4,
		.is_union = 0,
		.is_complete = 1,
		.has_flexible_array_member = 0
	};
	static StructDef arm64_complex_double2 = {
		.name = "__tcc_hfa_complex_double2",
		.fields = NULL,
		.field_count = 0,
		.field_cap = 0,
		.size = 16,
		.align = 8,
		.is_union = 0,
		.is_complete = 1,
		.has_flexible_array_member = 0
	};
	static StructDef x64_complex_float2 = {
		.name = "__tcc_x64_complex_float2",
		.fields = NULL,
		.field_count = 0,
		.field_cap = 0,
		.size = 8,
		.align = 4,
		.is_union = 0,
		.is_complete = 1,
		.has_flexible_array_member = 0
	};
	static StructDef x64_complex_double2 = {
		.name = "__tcc_x64_complex_double2",
		.fields = NULL,
		.field_count = 0,
		.field_cap = 0,
		.size = 16,
		.align = 8,
		.is_union = 0,
		.is_complete = 1,
		.has_flexible_array_member = 0
	};

	if (!name || !name[0])
		return NULL;
	if (STRCMP(name, "__tcc_hfa_complex_float2") == 0)
		return &arm64_complex_float2;
	if (STRCMP(name, "__tcc_hfa_complex_double2") == 0)
		return &arm64_complex_double2;
	if (STRCMP(name, "__tcc_x64_complex_float2") == 0)
		return &x64_complex_float2;
	if (STRCMP(name, "__tcc_x64_complex_double2") == 0)
		return &x64_complex_double2;
	return NULL;
}

StructDef *
find_struct(const char *name)
{
	int index = parser_find_struct_index_optional(name);

	if (index < 0) {
		StructDef *builtin = parser_builtin_abi_struct_or_null(name);

		if (builtin)
			return builtin;
	}

	if (index >= 0)
		return &ptab.structs[index];
	fatal_cur("Unknown struct: %s\n", name);
}

StructDef *
find_struct_or_null(const char *name)
{
	int index = parser_find_struct_index_optional(name);
	if (index >= 0)
		return &ptab.structs[index];
	return parser_builtin_abi_struct_or_null(name);
}

static StructDef *
find_struct_in_current_scope_or_null(const char *name)
{
	int scope_base = parser_current_tag_scope_base();
	StructDef *match = NULL;

	for (int index = scope_base; index < ptab.struct_count; index++) {
		const char *candidate = ptab.structs[index].name;
		int cmp = 1;

		if (candidate[0] == name[0] &&
		    candidate[1] == name[1] &&
		    tcc_hash_string(candidate) == tcc_hash_string(name))
			cmp = 0;
		if (parser_trace_toplevel_enabled() &&
		    name && name[0] &&
		    ((name[0] == 'L' && name[1] == 'a') ||
		     (candidate[0] == 'L' && candidate[1] == 'a'))) {
			fprintf(stderr,
			        "tcc parse: struct-scope-lookup want=%s idx=%d have=%s cmp=%d\n",
			        name, index,
			        candidate[0] ? candidate : "<anon>",
			        cmp);
		}
		if (cmp == 0)
			match = &ptab.structs[index];
	}
	return match;
}

static int
parser_struct_index_from_ptr(StructDef *def)
{
	int index = 0;

	if (!def)
		return -1;

	while (index < ptab.struct_count) {
		if (&ptab.structs[index] == def)
			return index;
		index++;
	}

	return -1;
}

int
parser_apply_pack_alignment(int align)
{
	if (align <= 1)
		return 1;
	if (parser_pragma_pack_align > 0 && align > parser_pragma_pack_align)
		return parser_pragma_pack_align;
	return align;
}

int
parser_try_consume_pragma_pack(void)
{
	const Token *tok = lexer_peek();
	int pack = 0;
	int has_pack = 0;
	char pack_name[TCC_IDENT_BUF_SIZE] = {0};

	if (!parser_pragma_pack_stack_align) {
		parser_pragma_pack_stack_align = xcalloc(PARSER_PRAGMA_PACK_STACK_MAX, sizeof(int));
		parser_pragma_pack_stack_name = xcalloc(PARSER_PRAGMA_PACK_STACK_MAX, sizeof(char *));
	}

	if (tok->kind != TOK_IDENT || !tok->text)
		return 0;

	if (STRCMP(tok->text, "__pragma_pack_push__") == 0) {
		lexer_next();
		expect(TOK_LPAREN);
		if (lexer_peek()->kind == TOK_IDENT || lexer_peek()->kind == TOK_STRING) {
			STRNCPY(pack_name, lexer_peek()->text, sizeof(pack_name) - 1);
			lexer_next();
			if (lexer_peek()->kind == TOK_COMMA)
				lexer_next();
		}
		if (lexer_peek()->kind == TOK_NUM) {
			pack = lexer_peek()->value;
			has_pack = 1;
			lexer_next();
		}
		expect(TOK_RPAREN);
		if (parser_pragma_pack_stack_count >= PARSER_PRAGMA_PACK_STACK_MAX)
			fatal_cur("pragma pack stack overflow\n");
		parser_pragma_pack_stack_align[parser_pragma_pack_stack_count] = parser_pragma_pack_align;
		xfree(parser_pragma_pack_stack_name[parser_pragma_pack_stack_count]);
		parser_pragma_pack_stack_name[parser_pragma_pack_stack_count] =
			pack_name[0] ? xstrdup(pack_name) : NULL;
		parser_pragma_pack_stack_count++;
		if (has_pack && pack > 0)
			parser_pragma_pack_align = pack;
		return 1;
	}

	if (STRCMP(tok->text, "__pragma_pack_pop__") == 0) {
		int pop_index = -1;

		lexer_next();
		if (lexer_peek()->kind == TOK_LPAREN) {
			lexer_next();
			if (lexer_peek()->kind == TOK_IDENT || lexer_peek()->kind == TOK_STRING) {
				STRNCPY(pack_name, lexer_peek()->text, sizeof(pack_name) - 1);
				lexer_next();
				if (lexer_peek()->kind == TOK_COMMA)
					lexer_next();
			}
			if (lexer_peek()->kind == TOK_NUM) {
				pack = lexer_peek()->value;
				has_pack = 1;
				lexer_next();
			}
			expect(TOK_RPAREN);
		}
		if (pack_name[0]) {
			for (int i = parser_pragma_pack_stack_count - 1; i >= 0; i--) {
				if (parser_pragma_pack_stack_name[i] &&
				    STRCMP(parser_pragma_pack_stack_name[i], pack_name) == 0) {
					pop_index = i;
					break;
				}
			}
			if (pop_index >= 0) {
				parser_pragma_pack_align = parser_pragma_pack_stack_align[pop_index];
				for (int i = pop_index; i < parser_pragma_pack_stack_count; i++) {
					xfree(parser_pragma_pack_stack_name[i]);
					parser_pragma_pack_stack_name[i] = NULL;
				}
				parser_pragma_pack_stack_count = pop_index;
			}
		} else if (parser_pragma_pack_stack_count > 0) {
			xfree(parser_pragma_pack_stack_name[parser_pragma_pack_stack_count - 1]);
			parser_pragma_pack_stack_name[parser_pragma_pack_stack_count - 1] = NULL;
			parser_pragma_pack_align =
				parser_pragma_pack_stack_align[--parser_pragma_pack_stack_count];
		} else {
			parser_pragma_pack_align = 0;
		}
		if (has_pack && pack > 0)
			parser_pragma_pack_align = pack;
		return 1;
	}

	if (STRCMP(tok->text, "__pragma_pack__") != 0)
		return 0;

	lexer_next();
	expect(TOK_LPAREN);
	if (lexer_peek()->kind == TOK_NUM) {
		pack = lexer_peek()->value;
		lexer_next();
	}
	expect(TOK_RPAREN);
	parser_pragma_pack_align = pack > 0 ? pack : 0;
	return 1;
}

StructDef *
get_or_add_forward_struct(const char *name)
{
	StructDef *existing;

	if (parser_trace_toplevel_enabled()) {
		fprintf(stderr,
		        "tcc parse: get_or_add_forward_struct name=%s ptr=%p count=%d cap=%d\n",
		        name ? name : "<null>",
		        (void *)ptab.structs,
		        ptab.struct_count,
		        ptab.struct_cap);
	}

	existing = find_struct_in_current_scope_or_null(name);
	if (existing)
		return existing;

	{
		StructDef *def = structs_push();
		STRNCPY(def->name, name, sizeof(def->name) - 1);
		def->align = 1;
		return def;
	}
}

static void __attribute__((unused)) 
skip_balanced_brace_body(void)
{
	expect(TOK_LBRACE);
	int depth = 1;
	while (depth > 0) {
		const Token *t = lexer_peek();
		if (t->kind == TOK_EOF) {
			fatal_cur("Unexpected EOF in aggregate body\n");
		}
		lexer_next();
		if (t->kind == TOK_LBRACE)
			depth++;
		else if (t->kind == TOK_RBRACE)
			depth--;
	}
}

void 
parse_struct_body_into(StructDef *def)
{
	/*
	 * Keep an index rather than trusting the incoming StructDef * for the
	 * whole parse.  Field type parsing can create nested anonymous/inline
	 * aggregates via structs_push(), which may xrealloc(ptab.structs) and move the
	 * table.  Re-derive def after such calls before touching it again.
	 */
	int def_idx = parser_struct_index_from_ptr(def);
	int is_union = def ? def->is_union : 0;

	if (def_idx < 0)
		ICE("parse_struct_body_into lost struct definition pointer");

	expect(TOK_LBRACE);

	int offset = 0;
	int max_align = 1;
	int bit_storage_offset = -1;
	int bit_storage_size = 0;
	int bit_storage_align = 1;
	int bit_storage_used = 0;

#define RESET_BITFIELD_STORAGE() \
	do { \
		bit_storage_offset = -1; \
		bit_storage_size = 0; \
		bit_storage_align = 1; \
		bit_storage_used = 0; \
	} while (0)

	while (lexer_peek()->kind != TOK_RBRACE) {
		int requested_align = parse_alignment_specifiers();
		if (parse_static_assert_declaration())
			continue;
		const Token *type = lexer_peek();
		int plain_thread_local_storage =
		    token_starts_plain_thread_local_storage_specifier(
		        type, lexer_peek_ahead(1), lexer_peek_ahead(2));
		int field_size = TCC_SIZEOF_INT;
		int field_align = TCC_SIZEOF_INT;
		int field_is_struct = 0;
		char field_struct_name[64] = {0};
		Type *field_type = NULL;

		if (type->kind == TOK_STATIC || type->kind == TOK_EXTERN ||
		    type->kind == TOK_AUTO || type->kind == TOK_REGISTER ||
		    type->kind == TOK_TYPEDEF) {
			fatal_cur("storage class is not allowed in aggregate member declarations\n");
		}
		if (plain_thread_local_storage)
			reject_plain_thread_local_keyword_before_c23(type);
		if (type->kind == TOK_THREAD_LOCAL || plain_thread_local_storage) {
			if (type->kind == TOK_THREAD_LOCAL)
				reject_thread_local_storage_specifier();
			fatal_cur("storage class is not allowed in aggregate member declarations\n");
		}
		if (type->kind == TOK_INLINE || type->kind == TOK_NORETURN) {
			fatal_cur("function specifier is only valid on function declarations\n");
		}

		if (!is_type_start_token(type->kind, type->text)) {
			fatal_cur("Only int, char, typedef, and nested aggregate fields are supported in ptab.structs for now\n");
		}

		field_type = parse_type_name();
		parser_reject_unsupported_special_type(field_type);
		if (parser_type_name_saw_trailing_storage_class() ||
		    parser_type_name_saw_thread_local_storage_specifier())
			fatal_cur("storage class is not allowed in aggregate member declarations\n");
		if (parser_type_name_saw_trailing_function_specifier())
			fatal_cur("function specifier is only valid on function declarations\n");
		def = &ptab.structs[def_idx];
		def->is_union = is_union;

		if (lexer_peek()->kind == TOK_LPAREN &&
		        lexer_peek_ahead(1)->kind == TOK_STAR) {
			lexer_next();
			lexer_next();

			/* Handle double-indirection: void (*(*name)(args))(void) */
			int extra_paren = 0;
			int extra_star = 0;
			if (lexer_peek()->kind == TOK_LPAREN &&
			    lexer_peek_ahead(1)->kind == TOK_STAR) {
				lexer_next();
				lexer_next();
				extra_paren = 1;
			}

			/* Handle void (**name)(void) — double star without extra parens */
			if (!extra_paren && lexer_peek()->kind == TOK_STAR) {
				lexer_next(); /* consume the extra * */
				extra_star = 1;
			}

			const Token *field = lexer_peek();
			parser_require_decl_identifier(field, "function pointer field name");
			char fp_name_buf[64] = {0};
			STRNCPY(fp_name_buf, field->text ? field->text : "", sizeof(fp_name_buf) - 1);
			lexer_next();
			expect(TOK_RPAREN);
			if (extra_paren) {
				Type **outer_param_types = NULL;
				int outer_param_count = 0;
				int outer_is_variadic = 0;
				int outer_fixed_params = 0;
				int outer_has_prototype = 0;
				Type **retfp_param_types = NULL;
				int retfp_param_count = 0;
				int retfp_is_variadic = 0;
				int retfp_fixed_params = 0;
				int retfp_has_prototype = 0;
				Type *ret_type;

				parse_prototype_param_list(&outer_param_types, &outer_param_count,
				                          &outer_is_variadic, &outer_fixed_params,
				                          &outer_has_prototype, 1);
				expect(TOK_RPAREN);
				parse_prototype_param_list(&retfp_param_types, &retfp_param_count,
				                          &retfp_is_variadic, &retfp_fixed_params,
				                          &retfp_has_prototype, 1);
				expect(TOK_SEMI);

				ret_type = retfp_has_prototype
				         ? parser_make_function_type(field_type, retfp_param_types,
				                                     retfp_param_count, retfp_is_variadic,
				                                     retfp_fixed_params)
				         : type_func(clone_type(field_type));
				ret_type = type_ptr(ret_type);

				offset = align_to(offset, parser_apply_pack_alignment(TCC_SIZEOF_PTR));
				if (requested_align > 0)
					offset = align_to(offset, requested_align);

				Field *f = struct_field_push(def);
				STRNCPY(f->name, fp_name_buf, sizeof(f->name) - 1);
				f->offset = offset;
				f->size = TCC_SIZEOF_PTR;
				f->is_struct = 0;
				f->type = type_ptr(outer_has_prototype
				                   ? parser_make_function_type(ret_type, outer_param_types,
				                                               outer_param_count, outer_is_variadic,
				                                               outer_fixed_params)
				                   : type_func(clone_type(ret_type)));
				offset += TCC_SIZEOF_PTR;
				if (max_align < parser_apply_pack_alignment(TCC_SIZEOF_PTR))
					max_align = parser_apply_pack_alignment(TCC_SIZEOF_PTR);
				RESET_BITFIELD_STORAGE();
				continue;
			}
			Type **fp_param_types = NULL;
			int fp_param_count = 0;
			int fp_is_variadic = 0;
			int fp_fixed_params = 0;
			int fp_has_prototype = 0;

			parse_prototype_param_list(&fp_param_types, &fp_param_count,
			                          &fp_is_variadic, &fp_fixed_params,
			                          &fp_has_prototype, 1);
			expect(TOK_SEMI);

			offset = align_to(offset, parser_apply_pack_alignment(TCC_SIZEOF_PTR));
			if (requested_align > 0)
				offset = align_to(offset, requested_align);

			Field *f = struct_field_push(def);
			STRNCPY(f->name, fp_name_buf, sizeof(f->name) - 1);
			f->offset = offset;
			f->size = TCC_SIZEOF_PTR;
			f->is_struct = 0;
			f->type = type_ptr(fp_has_prototype
			                   ? parser_make_function_type(field_type,
			                                               fp_param_types,
			                                               fp_param_count,
			                                               fp_is_variadic,
			                                               fp_fixed_params)
			                   : type_func(clone_type(field_type)));
			if (extra_star)
				f->type = type_ptr(f->type);
			offset += TCC_SIZEOF_PTR;
			if (max_align < parser_apply_pack_alignment(TCC_SIZEOF_PTR))
				max_align = parser_apply_pack_alignment(TCC_SIZEOF_PTR);
			RESET_BITFIELD_STORAGE();
			continue;
		}

		reject_void_object_type(field_type, "field");
		reject_incomplete_object_type(field_type, "field");
		reject_flexible_array_member_field_type(field_type);

		if (field_type->kind == TY_PTR) {
			field_size = TCC_SIZEOF_PTR;
			field_align = parser_apply_pack_alignment(TCC_SIZEOF_PTR);
		} else if (type_is_complex(field_type)) {
			field_size = type_sizeof(field_type);
			field_align = parser_apply_pack_alignment(type_alignof(field_type));
		} else if (field_type->kind == TY_CHAR) {
			field_size = 1;
			field_align = 1;
		} else if (field_type->kind == TY_SHORT) {
			field_size = 2;
			field_align = parser_apply_pack_alignment(2);
		} else if (field_type->kind == TY_FLOAT) {
			field_size = 4;
			field_align = parser_apply_pack_alignment(4);
		} else if (field_type->kind == TY_DOUBLE) {
			field_size = 8;
			field_align = parser_apply_pack_alignment(8);
		} else if (field_type->kind == TY_INT && field_type->size == 8) {
			/* long / unsigned long */
			field_size = TCC_SIZEOF_PTR;
			field_align = parser_apply_pack_alignment(TCC_SIZEOF_PTR);
		} else if (type_is_struct(field_type)) {
			field_size = field_type->size;
			{
				StructDef *nested = find_struct_or_null(field_type->struct_name);
				field_align = parser_apply_pack_alignment(aggregate_align(nested));
			}
			field_is_struct = 1;
			STRNCPY(field_struct_name, field_type->struct_name, sizeof(field_struct_name) - 1);
		} else {
			field_size = TCC_SIZEOF_INT;
			field_align = parser_apply_pack_alignment(TCC_SIZEOF_INT);
		}
		if (requested_align > field_align)
			field_align = requested_align;

		/* Anonymous member: "struct/union { ... };" with no field name.
		 * Flatten the nested aggregate's fields into this struct. */
		if ((lexer_peek()->kind == TOK_SEMI) &&
		    field_type && type_is_struct(field_type) && field_struct_name[0]) {
			expect(TOK_SEMI);
			StructDef *anon = find_struct_or_null(field_struct_name);
			if (anon) {
					int base = is_union ? 0 : ((offset + field_align - 1) & ~(field_align - 1));
					for (int ai = 0; ai < anon->field_count ; ai++) {
						Field *src = &anon->fields[ai];
						reject_duplicate_aggregate_field(def, src->name);
						Field *dst = struct_field_push(def);
						*dst = *src;
						dst->offset = anon->is_union ? base : (base + src->offset);
				}
				if (!is_union)
					offset = base + anon->size;
				else if (anon->size > offset)
					offset = anon->size;
				if (anon->align > max_align)
					max_align = anon->align;
			}
			RESET_BITFIELD_STORAGE();
			continue;
		}

		/* consume pointer stars: "char *p" — parse_type_name gave TY_CHAR, star here */
		while (lexer_peek()->kind == TOK_STAR) {
			lexer_next();
			field_type = type_ptr(field_type);
			field_size = TCC_SIZEOF_PTR;
			field_align = parser_apply_pack_alignment(TCC_SIZEOF_PTR);
		}

			const Token *field = lexer_peek();
			if ((field->kind != TOK_IDENT || !field->text) &&
			    field->kind != TOK_COLON) {
				fatal_token(field, "Expected struct field name\n");
			}

		/* Helper lambda (via macro pattern): commit one field then loop on comma */
			int flexible_array_member = 0;
			do {
				char field_name[64] = {0};
				if (field->kind == TOK_IDENT && field->text) {
					parser_require_decl_identifier(field, "struct field name");
					STRNCPY(field_name, field->text, sizeof(field_name) - 1);
					lexer_next();
				} else if (field->kind != TOK_COLON) {
					fatal_token(field, "Expected struct field name\n");
				}

				Type *this_field_type = parser_canonicalize_decl_type(field_type);
			int this_field_size = field_size;

			/* Collect all array dimensions first, then build type inside-out.
			 * For "char name[128][64]": dims=[128,64], built as
			 * type_array(type_array(char, 64), 128) = array[128] of array[64] of char.
			 * Left-to-right application would swap dimensions. */
			int field_dims[MAX_ARRAY_DIMS];
			int field_dim_count = 0;
			int flexible_field = 0;

			while (lexer_peek()->kind == TOK_LBRACKET) {
				lexer_next();

				if (lexer_peek()->kind == TOK_RBRACKET) {
					if (tcc_lang_is_c89_or_c90())
						fatal_cur("flexible array members are not allowed in C89/C90 mode\n");
					/*
					 * C99 flexible array member:
					 *
					 *     struct outer { int n; struct S s[]; };
					 *
					 * It contributes no size to the containing struct and must
					 * be the final member.
					 */
					if (is_union) {
						fatal_cur("Flexible array member not allowed in union\n");
					}
					if (struct_named_field_count(def) == 0) {
						fatal_cur("Flexible array member requires another named field\n");
					}
					flexible_array_member = 1;
					flexible_field = 1;
					def->has_flexible_array_member = 1;
					lexer_next();
					break;
				}

				const Token *len = lexer_peek();
				if (len->kind == TOK_LPAREN || len->kind == TOK_NUM ||
				    len->kind == TOK_IDENT) {
					int is_constant = 1;
					int dim = eval_const_array_size_checked(&is_constant);
					if (!is_constant)
						fatal_cur("field cannot have variably modified type\n");
					if (field_dim_count < MAX_ARRAY_DIMS)
						field_dims[field_dim_count++] = dim > 0 ? dim : 1;
					expect(TOK_RBRACKET);
				} else {
					fatal_cur("Expected numeric struct field array length\n");
				}
			}

			if (flexible_field) {
				this_field_type = type_array(this_field_type, 0);
				this_field_size = 0;
			} else if (field_dim_count > 0) {
				/* Apply dimensions right-to-left (innermost first) */
				for (int di = field_dim_count - 1; di >= 0; di--)
					this_field_type = type_array(this_field_type, field_dims[di]);
				this_field_size = this_field_type->size;
			}
			parser_validate_decl_alignment(requested_align, this_field_type);
			int is_bitfield = 0;
			int bit_width = 0;
			int bit_offset = 0;

				if (lexer_peek()->kind == TOK_COLON) {
					int storage_bits;
					Node *width_expr;

					if (requested_align > 0)
						fatal_cur("alignment specifier cannot be applied to a bit-field\n");

					lexer_next();
					width_expr = fold_constants(parse_conditional());
					if (!width_expr || width_expr->kind != ND_NUM || width_expr->is_fp_num)
						fatal_cur("Bit-field width must be an integer constant expression\n");
					bit_width = (int)width_expr->long_value;
					is_bitfield = 1;

				if (this_field_type->kind == TY_ARRAY || this_field_type->kind == TY_PTR || field_is_struct)
					fatal_cur("Bit-field base type must be an integer type\n");
				if (!type_is_integer(this_field_type))
					fatal_cur("Bit-field base type must be an integer type\n");
				if (flexible_field)
					fatal_cur("Bit-field cannot be a flexible array member\n");

				storage_bits = this_field_size * 8;
				if (bit_width < 0 || bit_width > storage_bits)
					fatal_cur("Bit-field width out of range for its base type\n");
				if (type_source_is(this_field_type, TYPE_SOURCE_BOOL) && bit_width > 1)
					fatal_cur("Bit-field width out of range for _Bool\n");
				if (field_name[0] && bit_width == 0)
					fatal_cur("Zero-width bit-field must be unnamed\n");
			}

			if (is_bitfield) {
				if (is_union) {
					bit_offset = 0;
				} else if (bit_width == 0) {
					if (bit_storage_offset >= 0)
						offset = bit_storage_offset + bit_storage_size;
					offset = align_to(offset, field_align);
					RESET_BITFIELD_STORAGE();
				} else {
					int need_new_storage =
						bit_storage_offset < 0 ||
						bit_storage_size != this_field_size ||
						bit_storage_align != field_align ||
						(bit_storage_used + bit_width) > (this_field_size * 8);

					if (need_new_storage) {
						if (bit_storage_offset >= 0)
							offset = bit_storage_offset + bit_storage_size;
						offset = align_to(offset, field_align);
						bit_storage_offset = offset;
						bit_storage_size = this_field_size;
						bit_storage_align = field_align;
						bit_storage_used = 0;
					}

					bit_offset = bit_storage_used;
					bit_storage_used += bit_width;
				}
			} else {
				if (bit_storage_offset >= 0)
					offset = bit_storage_offset + bit_storage_size;
				RESET_BITFIELD_STORAGE();
				offset = align_to(offset, field_align);
			}

				reject_duplicate_aggregate_field(def, field_name);
				Field *f = struct_field_push(def);
				STRNCPY(f->name, field_name, sizeof(f->name) - 1);
			if (parser_trace_toplevel_enabled()) {
				fprintf(stderr, "tcc parse: added-field struct=%s name=%s slot=%d\n",
				        def->name[0] ? def->name : "<anon>",
				        f->name,
				        def->field_count - 1);
			}
			f->offset = is_union ? 0 : (is_bitfield ? bit_storage_offset : offset);
			f->size = this_field_size;
			f->is_struct = field_is_struct;
			f->type = clone_type(this_field_type);
			f->is_bitfield = is_bitfield;
			f->bit_offset = bit_offset;
			f->bit_width = bit_width;
			f->bit_storage_size = is_bitfield ? this_field_size : 0;
			f->is_array = (this_field_type->kind == TY_ARRAY);
			f->elem_size = f->is_array ? type_elem_size(this_field_type->base ? this_field_type->base : this_field_type) : this_field_size;
			if (field_is_struct)
				STRNCPY(f->struct_name, field_struct_name, sizeof(f->struct_name) - 1);
			if (!is_union && !is_bitfield)
				offset += this_field_size;
			else if (is_union && this_field_size > offset)
				offset = this_field_size;
			else if (!is_union && is_bitfield && bit_storage_offset >= 0 &&
			         (bit_storage_used == bit_storage_size * 8))
				offset = bit_storage_offset + bit_storage_size;

			if (field_align > max_align)
				max_align = field_align;

			/* A flexible array member must be the final member declaration. */
			if (flexible_array_member && lexer_peek()->kind != TOK_SEMI) {
				fatal_cur("Flexible array member must be final struct field\n");
			}

			/* comma: next declarator of same base type, possibly with * */
			if (lexer_peek()->kind != TOK_COMMA) break;
			lexer_next(); /* consume , */
			/* Strip all pointer layers to recover the base element type */
			Type *next_type = field_type;
			while (next_type && next_type->kind == TY_PTR)
				next_type = next_type->base;
			int next_size = next_type ? (next_type->size > 0 ? next_type->size : TCC_SIZEOF_INT) : TCC_SIZEOF_INT;
			int next_align = parser_apply_pack_alignment(next_size > 8 ? 8 : (next_size < 1 ? 1 : next_size));
			/* Apply fresh pointer stars for this declarator */
			while (lexer_peek()->kind == TOK_STAR) {
				lexer_next();
				next_type = type_ptr(next_type);
				next_size = TCC_SIZEOF_PTR; next_align = TCC_SIZEOF_PTR;
			}
			field = lexer_peek();
			field_type = next_type;
			field_size = next_size;
			field_align = next_align;
		} while (1);

		expect(TOK_SEMI);
		if (flexible_array_member && lexer_peek()->kind != TOK_RBRACE) {
			fatal_cur("Flexible array member must be final struct field\n");
		}
	}

	expect(TOK_RBRACE);
	def->align = max_align;
	def->size = align_to(offset, max_align);
	def->is_complete = 1;
}

Field *
find_field(const char *struct_name, const char *field_name)
{
	StructDef *fallback_def = NULL;

	parser_profile_scope_enter(PARSER_PROF_FIND_FIELD);

	if (parser_field_cache.epoch == parser_field_lookup_epoch &&
	    parser_field_cache.field &&
	    STRCMP(parser_field_cache.struct_name, struct_name) == 0 &&
	    STRCMP(parser_field_cache.field_name, field_name) == 0) {
		parser_profile_scope_leave(PARSER_PROF_FIND_FIELD);
		return parser_field_cache.field;
	}

	for (int i = ptab.struct_count - 1; i >= 0; i--) {
		StructDef *def = &ptab.structs[i];

		if (STRCMP(def->name, struct_name) != 0)
			continue;

		if (!fallback_def)
			fallback_def = def;

		for (int j = 0; j < def->field_count; j++) {
			if (STRCMP(def->fields[j].name, field_name) == 0) {
				parser_field_cache.epoch = parser_field_lookup_epoch;
				STRNCPY(parser_field_cache.struct_name, struct_name,
				        sizeof(parser_field_cache.struct_name) - 1);
				STRNCPY(parser_field_cache.field_name, field_name,
				        sizeof(parser_field_cache.field_name) - 1);
				parser_field_cache.field = &def->fields[j];
				parser_profile_scope_leave(PARSER_PROF_FIND_FIELD);
				return &def->fields[j];
			}
		}
	}

	if (fallback_def && fallback_def->field_count == 0) {
		Field *field = struct_field_push(fallback_def);
		STRNCPY(field->name, field_name, sizeof(field->name) - 1);
		field->offset = 0;
		field->size = 4;
		parser_field_cache.epoch = parser_field_lookup_epoch;
		STRNCPY(parser_field_cache.struct_name, struct_name,
		        sizeof(parser_field_cache.struct_name) - 1);
		STRNCPY(parser_field_cache.field_name, field_name,
		        sizeof(parser_field_cache.field_name) - 1);
		parser_field_cache.field = field;
		parser_profile_scope_leave(PARSER_PROF_FIND_FIELD);
		return field;
	}

	parser_profile_scope_leave(PARSER_PROF_FIND_FIELD);
	fatal_cur("Unknown field %s.%s\n", struct_name, field_name);
}

void 
apply_field_type(Node *node, Field *field)
{
	Type *effective_field_type;

	if (!node || !field)
		return;

	node->is_array_field = field->is_array;
	node->is_bitfield = field->is_bitfield;
	node->bit_offset = field->bit_offset;
	node->bit_width = field->bit_width;
	node->bit_storage_size = field->bit_storage_size;

	if (field->type) {
		effective_field_type = field->cached_effective_type;
		if (!effective_field_type) {
			if (field->is_array && field->is_struct && field->struct_name[0] && field->elem_size > 0) {
				effective_field_type = parser_rebuild_struct_array_field_type(field->type,
				                                                             field->struct_name,
				                                                             field->elem_size);
			} else {
				effective_field_type = parser_canonicalize_decl_type(field->type);
			}
			field->cached_effective_type = effective_field_type;
		}
		node->type = effective_field_type;
		node->is_const_lvalue = type_has_qualifier(effective_field_type, TYPE_QUAL_CONST);
		/* elem_size is the STORAGE size of the field, not the pointee size.
		 * Pointer fields are always 8 bytes on arm64 (and 4 on 32-bit targets,
		 * but we use 8 throughout since we target 64-bit). */
		if (effective_field_type->kind == TY_PTR)
			node->elem_size = TCC_SIZEOF_PTR;
		else
			node->elem_size = type_elem_size(effective_field_type);
		node->is_unsigned = type_is_unsigned(effective_field_type);
		if (type_is_struct(effective_field_type)) {
			STRNCPY(node->struct_name,
			        parser_resolve_struct_type_name(effective_field_type),
			        sizeof(node->struct_name) - 1);
		} else if (type_is_pointer(effective_field_type) &&
		           type_pointee(effective_field_type) &&
		           type_is_struct(type_pointee(effective_field_type))) {
			STRNCPY(node->struct_name,
			        parser_resolve_struct_type_name(type_pointee(effective_field_type)),
			        sizeof(node->struct_name) - 1);
		} else {
			node->struct_name[0] = '\0';
		}
		return;
	}

	node->elem_size = field->size;
	if (field->is_struct) {
		StructDef *def = find_struct_or_null(field->struct_name);
		node->type = (def && def->is_union)
		           ? type_union(field->struct_name, field->size)
		           : type_struct(field->struct_name, field->size);
		node->is_const_lvalue = type_has_qualifier(node->type, TYPE_QUAL_CONST);
		STRNCPY(node->struct_name, field->struct_name, sizeof(node->struct_name) - 1);
	} else {
		node->type = type_for_size(field->size);
		node->is_const_lvalue = type_has_qualifier(node->type, TYPE_QUAL_CONST);
		node->is_unsigned = node->type && node->type->is_unsigned;
		node->struct_name[0] = '\0';
	}
}

int add_struct_local(const char *name, const char *struct_name)
{
	StructDef *def = find_struct(struct_name);
	int slots = (def->size + 3) / 4;
	int offset = add_local_sized_with_min_align(name, slots, 0, def->align);
	Local *local = parser_last_local_if_matches(name, offset);

	if (local) {
		local->is_struct = 1;
		STRNCPY(local->struct_name, struct_name, sizeof(local->struct_name) - 1);
		local->type = def->is_union
		            ? type_union(struct_name, def->size)
		            : type_struct(struct_name, def->size);
		parser_debug_local_set_type(local->offset, local->type);
		return offset;
	}

	return offset;
}

static Type *
struct_pointer_type(const char *struct_name, int struct_size, int pointer_depth)
{
	StructDef *def = find_struct(struct_name);
	Type *type = def->is_union
	           ? type_union(struct_name, struct_size)
	           : type_struct(struct_name, struct_size);

	if (pointer_depth <= 0)
		pointer_depth = 1;
	for (int i = 0; i < pointer_depth; i++)
		type = type_ptr(type);
	return type;
}

int add_struct_pointer_local_depth(const char *name, const char *struct_name, int pointer_depth)
{
	StructDef *def = find_struct(struct_name);
	int offset = add_local_sized_with_min_align(name, 2, 0, TCC_SIZEOF_PTR);
	Local *local = parser_last_local_if_matches(name, offset);

	if (pointer_depth <= 0)
		pointer_depth = 1;

	if (local) {
		local->is_pointer = 1;
		local->elem_size = pointer_depth > 1 ? TCC_SIZEOF_PTR : def->size;
		STRNCPY(local->struct_name, struct_name, sizeof(local->struct_name) - 1);
		local->type = struct_pointer_type(struct_name, def->size, pointer_depth);
		parser_debug_local_set_type(local->offset, local->type);
		return offset;
	}

	return offset;
}

int add_struct_pointer_local(const char *name, const char *struct_name)
{
	return add_struct_pointer_local_depth(name, struct_name, 1);
}

int 
add_struct_byref_param_local(const char *name, const char *struct_name)
{
	int offset = add_struct_pointer_local(name, struct_name);
	Local *local = parser_last_local_if_matches(name, offset);

	if (local) {
		local->struct_by_ref = 1;
		return offset;
	}

	return offset;
}

int 
is_struct_local(const char *name)
{
	Local *local = parser_find_local_first_optional(name);
	return local ? local->is_struct : 0;
}

int 
is_struct_by_ref_local(const char *name)
{
	Local *local = parser_find_local_first_optional(name);
	return local ? local->struct_by_ref : 0;
}

const char *
struct_name_local(const char *name)
{
	Local *local = parser_find_local_first_optional(name);
	if (local)
		return local->struct_name;
	return "";
}

const char *
struct_name_local_optional(const char *name)
{
	Local *local = parser_find_local_first_optional(name);
	if (local)
		return local->struct_name;
	return "";
}

FuncInfo *
find_func(const char *name)
{
	int index = parser_find_func_index_optional(name);
	return index >= 0 ? &ptab.funcs[index] : NULL;
}

static FuncInfo *
add_func_info(const char *name, int returns_struct, const char *struct_name,
              int struct_size, int returns_pointer, int return_elem_size,
              int return_abi_class, int return_abi_reg_count)
{
	FuncInfo *fi = find_func(name);

	if (parser_trace_toplevel_enabled()) {
		fprintf(stderr,
		        "tcc parse: add_func_info name=%s existing=%p func_count=%d func_cap=%d\n",
		        name ? name : "<null>",
		        (void *)fi,
		        ptab.func_count,
		        ptab.func_cap);
	}

	if (fi) {
		fi->returns_struct = returns_struct;
		fi->return_abi_class = return_abi_class;
		fi->return_abi_reg_count = return_abi_reg_count;
		fi->struct_size = struct_size;
		fi->returns_pointer = returns_pointer;
		fi->return_elem_size = return_elem_size;
		if (struct_name)
			STRNCPY(fi->struct_name, struct_name, sizeof(fi->struct_name) - 1);
		else
			fi->struct_name[0] = '\0';
		return fi;
	}

	fi = funcs_push();
	if (parser_trace_toplevel_enabled()) {
		fprintf(stderr, "tcc parse: add_func_info new fi=%p\n", (void *)fi);
	}
	STRNCPY(fi->name, name, sizeof(fi->name) - 1);
	fi->returns_struct = returns_struct;
	fi->return_abi_class = return_abi_class;
	fi->return_abi_reg_count = return_abi_reg_count;
	fi->struct_size = struct_size;
	fi->struct_name[0] = '\0';
	fi->returns_pointer = returns_pointer;
	fi->return_elem_size = return_elem_size;
	fi->return_type = NULL;
	fi->return_pointee_kind = 0;
	fi->return_pointee_size = 0;
	fi->return_pointee_is_unsigned = 0;
	fi->return_pointer_depth = 0;
	fi->return_pointee_source_kind = 0;
	fi->return_pointee_is_union = 0;
	fi->return_pointee_struct_name[0] = '\0';
	fi->return_pointee_source_name[0] = '\0';
	fi->is_static = 0;
	fi->is_noreturn = 0;
	fi->has_definition = 0;
	fi->has_prototype = 0;
	fi->is_variadic = 0;
	fi->fixed_param_count = 0;
	fi->param_types = NULL;
	fi->param_struct_names = NULL;
	fi->param_type_count = 0;
	parser_func_hash_note_new_index(ptab.func_count - 1);
	if (parser_trace_toplevel_enabled()) {
		fprintf(stderr, "tcc parse: add_func_info hashed index=%d\n", ptab.func_count - 1);
	}
	if (struct_name)
		STRNCPY(fi->struct_name, struct_name, sizeof(fi->struct_name) - 1);
	if (parser_trace_toplevel_enabled()) {
		fprintf(stderr, "tcc parse: add_func_info done name=%s\n",
		        fi->name[0] ? fi->name : "<anon>");
	}
return fi;
}

int
parser_func_is_noreturn(const char *name)
{
	FuncInfo *fi;

	if (!name || !name[0])
		return 0;
	fi = find_func(name);
	return fi ? fi->is_noreturn : 0;
}

void
parser_set_pending_decl_noreturn(int enabled)
{
	parser_pending_decl_noreturn = enabled ? 1 : 0;
}

int
parser_consume_pending_decl_noreturn(void)
{
	int pending = parser_pending_decl_noreturn;

	parser_pending_decl_noreturn = 0;
	return pending;
}

static void
func_info_set_param_types(FuncInfo *fi, Type **param_types, int param_count)
{
	if (!fi || param_count <= 0 || !param_types)
		return;

	fi->param_types = xcalloc((size_t)param_count, sizeof(Type *));
	fi->param_type_count = param_count;
	for (int i = 0; i < param_count; i++)
		fi->param_types[i] = param_types[i] ? clone_type(param_types[i]) : NULL;
}

static void
func_info_set_param_struct_names(FuncInfo *fi, char **param_struct_names, int param_count)
{
	if (!fi || param_count <= 0 || !param_struct_names)
		return;

	fi->param_struct_names = xcalloc((size_t)param_count, sizeof(char *));
	for (int i = 0; i < param_count; i++) {
		if (param_struct_names[i] && param_struct_names[i][0])
			fi->param_struct_names[i] = xstrdup(param_struct_names[i]);
	}
}

static void
param_type_list_append(Type ***types, int *count, int *cap, Type *type)
{
	if (*count >= *cap) {
		Type **old_types;
		Type **new_types;
		int new_cap = *cap ? *cap * 2 : 8;
		old_types = *types;
		new_types = xmalloc(sizeof(Type *) * (size_t)new_cap);
		if (old_types && *cap > 0)
			memcpy(new_types, old_types, sizeof(Type *) * (size_t)(*cap));
		parser_zero_grown_tail(new_types, *cap, new_cap, sizeof(Type *));
		xfree(old_types);
		*types = new_types;
		*cap = new_cap;
	}
	(*types)[(*count)++] = type ? parser_canonicalize_decl_type(type) : NULL;
}

void
parse_prototype_param_list(Type ***out_types, int *out_count,
                           int *is_variadic, int *fixed_params,
                           int *out_has_prototype,
                           int allow_oldstyle_empty)
{
	Type **param_types = NULL;
	int param_count = 0;
	int param_cap = 0;

	parser_profile_scope_enter(PARSER_PROF_PARAM_LIST);

	if (out_types)
		*out_types = NULL;
	if (out_count)
		*out_count = 0;
	if (is_variadic)
		*is_variadic = 0;
	if (fixed_params)
		*fixed_params = 0;
	if (out_has_prototype)
		*out_has_prototype = 1;

	expect(TOK_LPAREN);

	if (lexer_peek()->kind == TOK_RPAREN) {
		if (out_has_prototype)
			*out_has_prototype = allow_oldstyle_empty ? 0 : 1;
		expect(TOK_RPAREN);
	} else if (lexer_peek()->kind == TOK_VOID && lexer_peek_ahead(1)->kind == TOK_RPAREN) {
		lexer_next();
		expect(TOK_RPAREN);
	} else if (lexer_peek()->kind != TOK_RPAREN) {
		for (;;) {
			char param_name_buf[64];
			Type *param_type;

			if (lexer_peek()->kind == TOK_DOT &&
			    lexer_peek_ahead(1)->kind == TOK_DOT &&
			    lexer_peek_ahead(2)->kind == TOK_DOT) {
				lexer_next();
				lexer_next();
				lexer_next();
				if (is_variadic)
					*is_variadic = 1;
				if (fixed_params)
					*fixed_params = param_count;
				break;
			}

			param_type = parse_parameter_declarator_impl(param_name_buf, 1, NULL);
			param_type_list_append(&param_types, &param_count, &param_cap, param_type);

			if (lexer_peek()->kind != TOK_COMMA)
				break;

			lexer_next();
		}
		expect(TOK_RPAREN);
	} else
		expect(TOK_RPAREN);
	if (out_types)
		*out_types = param_types;
	if (out_count)
		*out_count = param_count;
	parser_profile_scope_leave(PARSER_PROF_PARAM_LIST);
}

void skip_inline_qualifiers(void);

int
consume_type_qualifiers(void)
{
	int qualifiers = 0;

	for (;;) {
		const Token *t = lexer_peek();

		if (t->kind == TOK_CONST) {
			reject_c89_c99_keyword_token(t->kind);
			qualifiers |= TYPE_QUAL_CONST;
			lexer_next();
			continue;
		}
		if (t->kind == TOK_VOLATILE) {
			reject_c89_c99_keyword_token(t->kind);
			qualifiers |= TYPE_QUAL_VOLATILE;
			lexer_next();
			continue;
		}
		if (t->kind == TOK_RESTRICT) {
			reject_c89_c99_keyword_token(t->kind);
			qualifiers |= TYPE_QUAL_RESTRICT;
			lexer_next();
			continue;
		}
		if (t->kind == TOK_ATOMIC) {
			if (lexer_peek_ahead(1)->kind == TOK_LPAREN)
				break;
			reject_c89_c99_keyword_token(t->kind);
			qualifiers |= TYPE_QUAL_ATOMIC;
			lexer_next();
			continue;
		}
		if (t->kind == TOK_IDENT && t->text &&
		        (STRCMP(t->text, "volatile") == 0 ||
		         STRCMP(t->text, "__volatile__") == 0 ||
		         STRCMP(t->text, "__volatile") == 0)) {
			qualifiers |= TYPE_QUAL_VOLATILE;
			lexer_next();
			continue;
		}
		if (t->kind == TOK_IDENT && t->text &&
		        (STRCMP(t->text, "__restrict") == 0 ||
		         STRCMP(t->text, "__restrict__") == 0)) {
			qualifiers |= TYPE_QUAL_RESTRICT;
			lexer_next();
			continue;
		}
		break;
	}

	return qualifiers;
}

static int
is_gnu_attribute_token(const Token *t)
{
	return t && t->kind == TOK_IDENT && t->text && STRCMP(t->text, "__attribute__") == 0;
}

static void
skip_one_gnu_attribute(void)
{
	if (!is_gnu_attribute_token(lexer_peek()))
		return;

	lexer_next();
	expect(TOK_LPAREN);
	expect(TOK_LPAREN);

	int depth = 1;
	while (depth > 0) {
		const Token *t = lexer_peek();
		if (t->kind == TOK_EOF) {
			fatal_cur("Unexpected EOF in __attribute__\n");
		}
		lexer_next();
		if (t->kind == TOK_LPAREN)
			depth++;
		else if (t->kind == TOK_RPAREN)
			depth--;
	}
	expect(TOK_RPAREN);
}

static void
skip_trailing_decl_attributes(void)
{
	while (is_gnu_attribute_token(lexer_peek()))
		skip_one_gnu_attribute();
}

static void
skip_decl_prefix_specifiers(void)
{
	for (;;) {
		const Token *t = lexer_peek();

		if (t->kind == TOK_INLINE || t->kind == TOK_NORETURN) {
			if (t->kind == TOK_INLINE && tcc_lang_is_c89_or_c90())
				fatal_cur("inline is not allowed in C89/C90 mode\n");
			reject_c89_c99_keyword_token(t->kind);
			lexer_next();
			continue;
		}

		if (t->kind == TOK_STATIC || t->kind == TOK_EXTERN) {
			reject_c89_c99_keyword_token(t->kind);
			lexer_next();
			continue;
		}

		if (is_gnu_attribute_token(t)) {
			skip_one_gnu_attribute();
			continue;
		}

		break;
	}
}

static int
parser_decl_prefix_has_extern_and_inline(int include_alignas)
{
	int i = 0;
	int saw_extern = 0;
	int saw_inline = 0;
	const Token *t = lexer_peek_ahead(i);

	for (;;) {
		if (t->kind == TOK_STATIC || t->kind == TOK_EXTERN ||
		    t->kind == TOK_INLINE || t->kind == TOK_NORETURN ||
		    t->kind == TOK_CONST || t->kind == TOK_VOLATILE ||
		    t->kind == TOK_RESTRICT || t->kind == TOK_ATOMIC) {
			if (t->kind == TOK_EXTERN)
				saw_extern = 1;
			else if (t->kind == TOK_INLINE)
				saw_inline = 1;
			i++;
			t = lexer_peek_ahead(i);
			continue;
		}

		if (is_gnu_attribute_token(t)) {
			int depth = 0;

			if (lexer_peek_ahead(i + 1)->kind != TOK_LPAREN ||
			    lexer_peek_ahead(i + 2)->kind != TOK_LPAREN)
				return 0;
			i += 3;
			depth = 1;
			while (depth > 0) {
				t = lexer_peek_ahead(i);
				if (t->kind == TOK_EOF)
					return 0;
				if (t->kind == TOK_LPAREN)
					depth++;
				else if (t->kind == TOK_RPAREN)
					depth--;
				i++;
			}
			t = lexer_peek_ahead(i);
			continue;
		}

		if (include_alignas &&
		    token_starts_alignas_specifier(t, lexer_peek_ahead(i + 1)) &&
		    (STRCMP(t->text, "_Alignas") == 0 ||
		     (tcc_lang_at_least(LANG_C23) &&
		      STRCMP(t->text, "alignas") == 0))) {
			int depth = 0;

			i++;
			if (lexer_peek_ahead(i)->kind != TOK_LPAREN)
				return 0;
			i++;
			depth = 1;
			while (depth > 0) {
				t = lexer_peek_ahead(i);
				if (t->kind == TOK_EOF)
					return 0;
				if (t->kind == TOK_LPAREN)
					depth++;
				else if (t->kind == TOK_RPAREN)
					depth--;
				i++;
			}
			t = lexer_peek_ahead(i);
			continue;
		}

		break;
	}

	return saw_extern && saw_inline;
}


int
try_parse_abstract_function_pointer_declarator(Type **ptype)
{
	if (lexer_peek()->kind != TOK_LPAREN)
		return 0;

	/* Check there's at least one star before consuming */
	{
	int look = 0;
	for (;;) {
		const Token *tok = lexer_peek_ahead(look + 1);
		if (tok->kind == TOK_CONST ||
		    tok->kind == TOK_VOLATILE ||
		    tok->kind == TOK_RESTRICT) {
			look++;
			continue;
		}
		if (is_gnu_attribute_token(tok)) {
			int depth = 0;
			if (lexer_peek_ahead(look + 2)->kind != TOK_LPAREN ||
			    lexer_peek_ahead(look + 3)->kind != TOK_LPAREN)
				return 0;
			look += 3;
			depth = 1;
			while (depth > 0) {
				const Token *at = lexer_peek_ahead(look);
				if (at->kind == TOK_EOF)
					return 0;
				if (at->kind == TOK_LPAREN)
					depth++;
				else if (at->kind == TOK_RPAREN)
					depth--;
				look++;
			}
			look--;
			continue;
		}
		break;
	}
	if (lexer_peek_ahead(look + 1)->kind != TOK_STAR)
		return 0;
	}
	lexer_next();
	skip_inline_qualifiers();

	if (lexer_peek()->kind == TOK_STAR &&
	    lexer_peek_ahead(1)->kind == TOK_LPAREN &&
	    lexer_peek_ahead(2)->kind == TOK_STAR &&
	    lexer_peek_ahead(3)->kind == TOK_RPAREN) {
		Type **outer_param_types = NULL;
		int outer_param_count = 0;
		int outer_is_variadic = 0;
		int outer_fixed_params = 0;
		int outer_has_prototype = 0;
		Type **retfp_param_types = NULL;
		int retfp_param_count = 0;
		int retfp_is_variadic = 0;
		int retfp_fixed_params = 0;
		int retfp_has_prototype = 0;
		Type *ret_type;

		lexer_next(); /* outer * */
		lexer_next(); /* inner ( */
		expect(TOK_STAR);
		expect(TOK_RPAREN);

		parse_prototype_param_list(&outer_param_types, &outer_param_count,
		                          &outer_is_variadic, &outer_fixed_params,
		                          &outer_has_prototype, 1);
		expect(TOK_RPAREN);

		parse_prototype_param_list(&retfp_param_types, &retfp_param_count,
		                          &retfp_is_variadic, &retfp_fixed_params,
		                          &retfp_has_prototype, 1);

		ret_type = retfp_has_prototype
		         ? parser_make_function_type(*ptype, retfp_param_types,
		                                     retfp_param_count, retfp_is_variadic,
		                                     retfp_fixed_params)
		         : type_func(clone_type(*ptype));
		ret_type = type_ptr(ret_type);
		*ptype = type_ptr(outer_has_prototype
		                  ? parser_make_function_type(ret_type, outer_param_types,
		                                              outer_param_count, outer_is_variadic,
		                                              outer_fixed_params)
		                  : type_func(clone_type(ret_type)));
		return 1;
	}

	/* Consume all stars: (*) for single pointer, (**) for pointer-to-pointer, etc. */
	while (lexer_peek()->kind == TOK_STAR) lexer_next();
	skip_inline_qualifiers();
	expect(TOK_RPAREN);

	if (lexer_peek()->kind == TOK_LPAREN) {
		Type **param_types = NULL;
		int param_count = 0;
		int is_variadic = 0;
		int fixed_params = 0;
		int has_prototype = 0;

		parse_prototype_param_list(&param_types, &param_count,
		                          &is_variadic, &fixed_params,
		                          &has_prototype, 1);
		*ptype = type_ptr(has_prototype
		                  ? parser_make_function_type(*ptype, param_types,
		                                              param_count, is_variadic,
		                                              fixed_params)
		                  : type_func(clone_type(*ptype)));
	} else {
		*ptype = type_ptr(type_func(*ptype));
	}
	return 1;
}

void 
skip_inline_qualifiers(void)
{
	for (;;) {
		const Token *t = lexer_peek();
		if (t->kind == TOK_INLINE || t->kind == TOK_NORETURN) {
			if (t->kind == TOK_INLINE && tcc_lang_is_c89_or_c90())
				fatal_cur("inline is not allowed in C89/C90 mode\n");
			reject_c89_c99_keyword_token(t->kind);
			lexer_next();
			continue;
		}

		if (t->kind == TOK_STATIC ||
		        t->kind == TOK_EXTERN) {
			reject_c89_c99_keyword_token(t->kind);
			lexer_next();
			continue;
		}

		if (consume_type_qualifiers()) {
			continue;
		}

		if (is_gnu_attribute_token(t)) {
			skip_one_gnu_attribute();
			continue;
		}
		break;
	}
}

static int
is_prototype_start(void)
{
	return lexer_is_function_prototype();
}

typedef struct ParsedFileScopeDeclarator {
	char name[64];
	Type *type;
	int is_function;
	Type **param_types;
	int param_count;
	int is_variadic;
	int fixed_params;
	int has_prototype;
} ParsedFileScopeDeclarator;

static void
parse_file_scope_declarator(Type *base_type, ParsedFileScopeDeclarator *decl,
                            const char *name_message)
{
	int ptr_quals[16] = {0};
	int ptr_count = 0;
	Type *decl_type;
	const Token *name_tok;

	memset(decl, 0, sizeof(*decl));

	if (lexer_peek()->kind == TOK_LPAREN &&
	    lexer_peek_ahead(1)->kind == TOK_STAR) {
		decl->type = parse_nested_function_pointer_object_declarator_type(base_type,
		                                                                  decl->name);
		return;
	}

	decl_type = clone_type(base_type);
	while (lexer_peek()->kind == TOK_STAR) {
		if (ptr_count >= 16)
			fatal_cur("global declarator pointer nesting too deep\n");
		lexer_next();
		ptr_quals[ptr_count++] = consume_type_qualifiers();
	}

	while (ptr_count > 0) {
		decl_type = type_ptr(decl_type);
		ptr_count--;
		if (ptr_quals[ptr_count])
			decl_type = type_with_qualifiers(decl_type,
			                                 type_qualifiers(decl_type) | ptr_quals[ptr_count]);
	}

	name_tok = lexer_peek();
	parser_require_decl_identifier(name_tok, name_message);
	STRNCPY(decl->name, name_tok->text ? name_tok->text : "", sizeof(decl->name) - 1);
	lexer_next();

	if (lexer_peek()->kind == TOK_LPAREN) {
		decl->is_function = 1;
		parser_reject_unsupported_complex_function_type(decl_type);
		decl->type = decl_type;
		parse_prototype_param_list(&decl->param_types, &decl->param_count,
		                          &decl->is_variadic, &decl->fixed_params,
		                          &decl->has_prototype, 1);
		if (lexer_peek()->kind == TOK_LBRACKET)
			fatal_cur("function return array declarators are not supported\n");
		if (lexer_peek()->kind == TOK_LPAREN)
			fatal_cur("function cannot return function type\n");
		return;
	}

	if (lexer_peek()->kind == TOK_LBRACKET) {
		int dims[MAX_ARRAY_DIMS] = {0};
		int dim_count;

		if (lexer_peek_ahead(1)->kind == TOK_STATIC)
			fatal_cur("array static bounds are only allowed in function parameter declarators\n");
		if (lexer_peek_ahead(1)->kind == TOK_CONST ||
		    lexer_peek_ahead(1)->kind == TOK_VOLATILE ||
		    lexer_peek_ahead(1)->kind == TOK_RESTRICT ||
		    lexer_peek_ahead(1)->kind == TOK_ATOMIC ||
		    (lexer_peek_ahead(1)->kind == TOK_IDENT &&
		     lexer_peek_ahead(1)->text &&
		     (STRCMP(lexer_peek_ahead(1)->text, "volatile") == 0 ||
		      STRCMP(lexer_peek_ahead(1)->text, "restrict") == 0)))
			fatal_cur("array type qualifiers are only allowed in function parameter declarators\n");
		if (parser_array_bound_contains_nonconstant_identifier())
			fatal_cur("file-scope array bound must be an integer constant expression\n");
		dim_count = parse_array_dimensions(dims, 1, 0);
		if (lexer_peek()->kind == TOK_LPAREN)
			fatal_cur("array elements cannot have function type\n");
		decl_type = build_array_type_from_dims_allow_incomplete(decl_type, dims, dim_count, 1);
	}

	decl->type = decl_type;
}

static void
parse_generic_global_initializer(Global **pg, Type *type, const char *message)
{
	Global *g = pg ? *pg : NULL;
	const Token *value = lexer_peek();
	Type *effective_type = parser_canonicalize_decl_type(type);
	const char *resolved_struct_name = "";

	if (type_is_struct(effective_type) || type_is_union(effective_type))
		resolved_struct_name = parser_resolve_struct_type_name(effective_type);

	if ((type_is_struct(effective_type) || type_is_union(effective_type)) &&
	    value->kind == TOK_LBRACE) {
		StructDef *def = resolved_struct_name[0] ? find_struct(resolved_struct_name) : NULL;
		if (def && def->has_flexible_array_member && tcc_iso_diagnostics)
			fatal_cur("initializer for %s with flexible array member is not supported\n",
			          def->is_union ? "union" : "struct");
		if (!def)
			fatal_cur("%s", message);
		mark_global_initialized(g);
		lexer_next();
		global_set_init_count(g, def->size);
		{
			int gi = (int)(g - punit.globals);
			globals_ensure_spare(256);
			g = &punit.globals[gi];
			parse_global_struct_initializer_body((int)(g - punit.globals), def, 0);
			g = &punit.globals[gi];
			if (pg)
				*pg = g;
		}
		expect(TOK_RBRACE);
		return;
	}

	if ((type_is_struct(effective_type) || type_is_union(effective_type)) &&
	    value->kind == TOK_LPAREN && resolved_struct_name[0]) {
		StructDef *def = find_struct(resolved_struct_name);
		int g_idx = (int)(g - punit.globals);
		if (!def ||
		    !try_parse_global_struct_compound_initializer(g_idx, def,
		                                                  resolved_struct_name, 0)) {
			fatal_cur("%s", message);
		}
		/*
		 * Compound-literal lowering can allocate helper globals while parsing the
		 * initializer body, which may grow punit.globals and invalidate the
		 * caller's Global * slot. Re-derive it before the outer declaration path
		 * commits the definition.
		 */
		g = &punit.globals[g_idx];
		if (pg)
			*pg = g;
		return;
	}

	if (effective_type && effective_type->kind == TY_PTR &&
	    try_parse_global_addr_array_compound_literal(&g)) {
		if (pg)
			*pg = g;
		return;
	}
	if (effective_type && effective_type->kind == TY_PTR &&
	    try_parse_global_addr_scalar_compound_literal(&g)) {
		if (pg)
			*pg = g;
		return;
	}
	if (effective_type && effective_type->kind == TY_PTR &&
	    try_parse_global_addr_struct_compound_literal(&g)) {
		if (pg)
			*pg = g;
		return;
	}

	if (value->kind == TOK_STRING) {
		if (effective_type && effective_type->kind == TY_PTR) {
			parse_string_pointer_global_initializer(g, message);
			if (pg)
				*pg = g;
			return;
		}
		if (effective_type && effective_type->kind == TY_ARRAY) {
			parse_string_array_global_initializer(g, message);
			if (pg)
				*pg = g;
			return;
		}
	}

	if (effective_type && effective_type->kind == TY_PTR &&
	    ((value->kind == TOK_AMP && lexer_peek_ahead(1)->kind == TOK_IDENT) ||
	     (value->kind == TOK_IDENT && (is_global(value->text) || find_func(value->text))))) {
		parse_symbol_address_global_initializer(g, message, 0, 0);
		if (pg)
			*pg = g;
		return;
	}

	parse_scalar_global_initializer(g, message);
	if (pg)
		*pg = g;
}

static void
parser_apply_global_decl_flags(Global *g, const char *name)
{
	if (!g)
		return;
	g->is_thread_local = pfunc.file_thread_local;
	if (name &&
	    (STRCMP(name, "__stdinp") == 0 ||
	     STRCMP(name, "__stdoutp") == 0 ||
	     STRCMP(name, "__stderrp") == 0))
		g->is_dylib = 1;
}

static void
parser_apply_extern_global_flags(Global *g, const char *name)
{
	if (!g)
		return;
	parser_apply_global_decl_flags(g, name);
	g->is_extern = 1;
}

static void
parser_register_extern_object_declaration(const char *name, Type *decl_type)
{
	Global *g = find_global(name);

	if (g) {
		if (g->is_thread_local != pfunc.file_thread_local)
			fatal_cur("Conflicting declaration for global '%s'\n", name);
		parser_validate_global_object_redeclaration(g, name, decl_type);
		parser_merge_global_object_redeclaration(g, decl_type);
		return;
	}

	g = new_global_slot(name);
	parser_apply_extern_global_flags(g, name);
	apply_type_to_global(g, decl_type);
	parser_commit_reserved_global();
}

static void
parser_define_extern_object_declaration(const char *name, Type *decl_type,
                                        const char *message)
{
	Global *g = new_global_slot(name);

	parser_apply_global_decl_flags(g, name);
	apply_type_to_global(g, decl_type);
	parse_generic_global_initializer(&g, decl_type, message);
	commit_global_definition(g);
}

static void
parser_handle_extern_object_declarator(const char *name, Type *decl_type,
                                       const char *message)
{
	reject_void_object_type(decl_type, "extern global");
	reject_flexible_array_member_array_object_type(decl_type, "extern global");
	if (lexer_peek()->kind == TOK_ASSIGN) {
		lexer_next();
		parser_define_extern_object_declaration(name, decl_type, message);
		return;
	}
	parser_register_extern_object_declaration(name, decl_type);
}

static void
parser_handle_generic_object_declarator(const char *name, Type *decl_type,
                                        const char *message)
{
	Global *g;

	reject_void_object_type(decl_type, "global");
	reject_flexible_array_member_array_object_type(decl_type, "global");
	g = new_global_slot(name);
	apply_type_to_global(g, decl_type);
	if (lexer_peek()->kind == TOK_ASSIGN) {
		lexer_next();
		parse_generic_global_initializer(&g, decl_type, message);
	}
	commit_global_definition(g);
}

static void
parser_handle_file_scope_function_declarator(ParsedFileScopeDeclarator *decl,
                                             int saw_noreturn,
                                             int skip_trailing_attributes_only,
                                             int is_static_decl)
{
	int saved_file_static = pfunc.file_static;

	pfunc.file_static = is_static_decl ? 1 : 0;
	if (skip_trailing_attributes_only)
		skip_trailing_decl_attributes();
	else
		skip_decl_prefix_specifiers();

	parser_declare_function(decl->name, decl->type, decl->has_prototype,
	                        decl->param_types, decl->param_count,
	                        decl->is_variadic, decl->fixed_params,
	                        saw_noreturn);
	pfunc.file_static = saved_file_static;
}

static void 
parse_prototype_param_metadata(int *is_variadic, int *fixed_params)
{
	if (is_variadic)
		*is_variadic = 0;
	if (fixed_params)
		*fixed_params = 0;

	expect(TOK_LPAREN);

	int depth = 1;
	int param_n = 0;

	while (depth > 0) {
		const Token *t = lexer_peek();

		if (t->kind == TOK_EOF) {
			fatal_cur("Unexpected EOF in prototype\n");
		}

		if (t->kind == TOK_LPAREN) {
			depth++;
			lexer_next();
			continue;
		}

		if (t->kind == TOK_RPAREN) {
			depth--;
			lexer_next();
			if (depth == 0)
				break;
			continue;
		}

		if (t->kind == TOK_COMMA && depth == 1) {
			param_n++;
			lexer_next();
			continue;
		}

		if (t->kind == TOK_VOID && depth == 1) {
			TokenKind next = lexer_peek_ahead(1)->kind;
			if (!((param_n == 0 && next == TOK_RPAREN) ||
			      next == TOK_STAR || next == TOK_LPAREN))
				fatal_cur("parameter cannot have type void\n");
		}

		if (t->kind == TOK_DOT && depth == 1 &&
		        lexer_peek_ahead(1)->kind == TOK_DOT) {
			if (is_variadic)
				*is_variadic = 1;
			if (fixed_params)
				*fixed_params = param_n;

			while (lexer_peek()->kind != TOK_RPAREN &&
			        lexer_peek()->kind != TOK_EOF)
				lexer_next();

			if (lexer_peek()->kind == TOK_RPAREN)
				lexer_next();

			return;
		}

		if (depth == 1 && t->kind == TOK_IDENT && lexer_peek_ahead(1)->kind == TOK_LBRACKET) {
			int dim_index = 0;

			lexer_next(); /* ident */
			while (lexer_peek()->kind == TOK_LBRACKET) {
				int runtime_bound = 0;
				int unsized = 0;
				int bracket_depth = 1;

				lexer_next(); /* [ */
				skip_parameter_array_qualifiers();

				if (lexer_peek()->kind == TOK_STAR &&
				    lexer_peek_ahead(1)->kind == TOK_RBRACKET) {
					runtime_bound = 1;
					lexer_next();
				} else if (lexer_peek()->kind == TOK_RBRACKET) {
					unsized = 1;
				} else {
					while (bracket_depth > 0) {
						const Token *bt = lexer_peek();
						if (bt->kind == TOK_EOF)
							fatal_cur("Unexpected EOF in prototype\n");
						if (bt->kind == TOK_LBRACKET) {
							bracket_depth++;
							lexer_next();
							continue;
						}
						if (bt->kind == TOK_RBRACKET) {
							bracket_depth--;
							if (bracket_depth == 0)
								break;
							lexer_next();
							continue;
						}
						if (bracket_depth == 1 && bt->kind == TOK_IDENT) {
							int enum_value = 0;
							if (!parser_find_enum_const(bt->text, &enum_value))
								runtime_bound = 1;
						}
						lexer_next();
					}
				}

				if (runtime_bound && tcc_lang_is_c89_or_c90())
					fatal_cur("variable length array syntax is not allowed in C89/C90 mode\n");
				if (dim_index > 0 && (runtime_bound || unsized))
					fatal_cur("only the first dimension of a parameter VLA may be variably modified\n");
				expect(TOK_RBRACKET);
				dim_index++;
			}
			continue;
		}

		lexer_next();
	}
}

static void  __attribute__((unused)) 
skip_param_list(void)
{
	parse_prototype_param_metadata(NULL, NULL);
}

#define reject_invalid_function_return_declarator()                                   \
	do {                                                                         \
		if (lexer_peek()->kind == TOK_LBRACKET)                              \
			fatal_cur("function return array declarators are not supported\n"); \
		if (lexer_peek()->kind == TOK_LPAREN)                                \
			fatal_cur("function cannot return function type\n");        \
	} while (0)

void 
parse_union_definition(void)
{
	expect(TOK_UNION);

	const Token *name = lexer_peek();
	parser_require_decl_identifier(name, "union name");

	lexer_next();
	expect(TOK_LBRACE);

	/* Reuse an existing forward declaration.  This matters for:
	 *
	 *   typedef union tree_node *tree;
	 *   union tree_node { struct tree_common common; };
	 *
	 * The typedef creates a forward aggregate named tree_node.  If the
	 * later union definition creates a second entry with the same name,
	 * find_struct("tree_node") can return the empty forward entry and
	 * expressions like (convs)->common.code lose the type of common.
	 */
	StructDef *def = find_struct_in_current_scope_or_null(name->text);
	if (!def) {
		def = structs_push();
	}
	int def_idx = (int)(def - ptab.structs);
	memset(def, 0, sizeof(*def));
	def->is_union = 1;
	STRNCPY(def->name, name->text, sizeof(def->name) - 1);

	int max_size = 0;
	int max_align = 1;

	while (lexer_peek()->kind != TOK_RBRACE) {
		int requested_align = parse_alignment_specifiers();
		if (parse_static_assert_declaration())
			continue;
		const Token *type = lexer_peek();
		int plain_thread_local_storage =
		    token_starts_plain_thread_local_storage_specifier(
		        type, lexer_peek_ahead(1), lexer_peek_ahead(2));
		int field_size = TCC_SIZEOF_INT;
		int field_align = TCC_SIZEOF_INT;
		int field_is_struct = 0;
		char field_struct_name[64] = {0};
		Type *field_type = NULL;
		int field_elem_size = TCC_SIZEOF_INT;

		/*
		 * Union fields need the same type handling as struct fields.  In
		 * particular, torture/00216 uses typedef aliases such as:
		 *
		 *     typedef unsigned char u8;
		 *     union U { u8 a; ... };
		 *
		 * The old union parser only accepted literal int/char/short tokens and
		 * rejected typedef scalar fields before the declarator name was reached.
		 */
		if (type->kind == TOK_STATIC || type->kind == TOK_EXTERN ||
		    type->kind == TOK_AUTO || type->kind == TOK_REGISTER ||
		    type->kind == TOK_TYPEDEF) {
			fatal_cur("storage class is not allowed in aggregate member declarations\n");
		}
		if (plain_thread_local_storage)
			reject_plain_thread_local_keyword_before_c23(type);
		if (type->kind == TOK_THREAD_LOCAL || plain_thread_local_storage) {
			if (type->kind == TOK_THREAD_LOCAL)
				reject_thread_local_storage_specifier();
			fatal_cur("storage class is not allowed in aggregate member declarations\n");
		}
		if (type->kind == TOK_INLINE || type->kind == TOK_NORETURN) {
			fatal_cur("function specifier is only valid on function declarations\n");
		}

		if (!is_type_start_token(type->kind, type->text)) {
			fatal_cur("Only int, char, typedef, and nested aggregate fields are supported in unions for now\n");
		}

		field_type = parse_type_name();
		parser_reject_unsupported_special_type(field_type);
		if (parser_type_name_saw_trailing_storage_class() ||
		    parser_type_name_saw_thread_local_storage_specifier())
			fatal_cur("storage class is not allowed in aggregate member declarations\n");
		if (parser_type_name_saw_trailing_function_specifier())
			fatal_cur("function specifier is only valid on function declarations\n");
		def = &ptab.structs[def_idx];

		if (lexer_peek()->kind == TOK_LPAREN &&
		    lexer_peek_ahead(1)->kind == TOK_STAR) {
			const Token *field;
			char fp_name_buf[64] = {0};
			int extra_paren = 0;
			int extra_star = 0;

			lexer_next();
			lexer_next();

			/* Handle double-indirection: void (*(*name)(args))(void) */
			if (lexer_peek()->kind == TOK_LPAREN &&
			    lexer_peek_ahead(1)->kind == TOK_STAR) {
				lexer_next();
				lexer_next();
				extra_paren = 1;
			}

			if (!extra_paren && lexer_peek()->kind == TOK_STAR) {
				lexer_next();
				extra_star = 1;
			}

			field = lexer_peek();
			parser_require_decl_identifier(field, "function pointer field name");
			STRNCPY(fp_name_buf, field->text ? field->text : "", sizeof(fp_name_buf) - 1);
			lexer_next();
			expect(TOK_RPAREN);
			if (extra_paren) {
				Type **outer_param_types = NULL;
				int outer_param_count = 0;
				int outer_is_variadic = 0;
				int outer_fixed_params = 0;
				int outer_has_prototype = 0;
				Type **retfp_param_types = NULL;
				int retfp_param_count = 0;
				int retfp_is_variadic = 0;
				int retfp_fixed_params = 0;
				int retfp_has_prototype = 0;
				Type *ret_type;

				parse_prototype_param_list(&outer_param_types, &outer_param_count,
				                          &outer_is_variadic, &outer_fixed_params,
				                          &outer_has_prototype, 1);
				expect(TOK_RPAREN);
				parse_prototype_param_list(&retfp_param_types, &retfp_param_count,
				                          &retfp_is_variadic, &retfp_fixed_params,
				                          &retfp_has_prototype, 1);
				expect(TOK_SEMI);

				ret_type = retfp_has_prototype
				         ? parser_make_function_type(field_type, retfp_param_types,
				                                     retfp_param_count, retfp_is_variadic,
				                                     retfp_fixed_params)
				         : type_func(clone_type(field_type));
				ret_type = type_ptr(ret_type);

				Field *f = struct_field_push(def);
				int ptr_align = parser_apply_pack_alignment(TCC_SIZEOF_PTR);
				STRNCPY(f->name, fp_name_buf, sizeof(f->name) - 1);
				f->offset = 0;
				f->size = TCC_SIZEOF_PTR;
				f->is_struct = 0;
				f->type = type_ptr(outer_has_prototype
				                   ? parser_make_function_type(ret_type, outer_param_types,
				                                               outer_param_count, outer_is_variadic,
				                                               outer_fixed_params)
				                   : type_func(clone_type(ret_type)));
				parser_validate_decl_alignment(requested_align, f->type);
				if (requested_align > ptr_align)
					ptr_align = requested_align;

				if (TCC_SIZEOF_PTR > max_size)
					max_size = TCC_SIZEOF_PTR;
				if (ptr_align > max_align)
					max_align = ptr_align;
				continue;
			}
			Type **fp_param_types = NULL;
			int fp_param_count = 0;
			int fp_is_variadic = 0;
			int fp_fixed_params = 0;
			int fp_has_prototype = 0;

			parse_prototype_param_list(&fp_param_types, &fp_param_count,
			                          &fp_is_variadic, &fp_fixed_params,
			                          &fp_has_prototype, 1);
			expect(TOK_SEMI);

			Field *f = struct_field_push(def);
			int ptr_align = parser_apply_pack_alignment(TCC_SIZEOF_PTR);
			STRNCPY(f->name, fp_name_buf, sizeof(f->name) - 1);
			f->offset = 0;
			f->size = TCC_SIZEOF_PTR;
			f->is_struct = 0;
			f->type = type_ptr(fp_has_prototype
			                   ? parser_make_function_type(field_type,
			                                               fp_param_types,
			                                               fp_param_count,
			                                               fp_is_variadic,
			                                               fp_fixed_params)
			                   : type_func(clone_type(field_type)));
			if (extra_star)
				f->type = type_ptr(f->type);
			parser_validate_decl_alignment(requested_align, f->type);
			if (requested_align > ptr_align)
				ptr_align = requested_align;

			if (TCC_SIZEOF_PTR > max_size)
				max_size = TCC_SIZEOF_PTR;
			if (ptr_align > max_align)
				max_align = ptr_align;
			continue;
		}

		reject_void_object_type(field_type, "field");
		reject_incomplete_object_type(field_type, "field");
		reject_flexible_array_member_field_type(field_type);

		if (field_type->kind == TY_PTR) {
			field_size = TCC_SIZEOF_PTR;
			field_align = parser_apply_pack_alignment(TCC_SIZEOF_PTR);
		} else if (type_is_complex(field_type)) {
			field_size = type_sizeof(field_type);
			field_align = parser_apply_pack_alignment(type_alignof(field_type));
		} else if (field_type->kind == TY_CHAR) {
			field_size = 1;
			field_align = 1;
		} else if (field_type->kind == TY_SHORT) {
			field_size = 2;
			field_align = parser_apply_pack_alignment(2);
		} else if (field_type->kind == TY_FLOAT) {
			field_size = 4;
			field_align = parser_apply_pack_alignment(4);
		} else if (field_type->kind == TY_DOUBLE) {
			field_size = 8;
			field_align = parser_apply_pack_alignment(8);
		} else if (field_type->kind == TY_INT && field_type->size == 8) {
			/* long / unsigned long */
			field_size = TCC_SIZEOF_PTR;
			field_align = parser_apply_pack_alignment(TCC_SIZEOF_PTR);
		} else if (type_is_struct(field_type)) {
			StructDef *nested = find_struct(field_type->struct_name);
			field_size = nested->size;
			field_align = parser_apply_pack_alignment(aggregate_align(nested));
			field_is_struct = 1;
			STRNCPY(field_struct_name, field_type->struct_name, sizeof(field_struct_name) - 1);
		} else {
			field_size = TCC_SIZEOF_INT;
			field_align = parser_apply_pack_alignment(TCC_SIZEOF_INT);
		}
		if (requested_align > field_align)
			field_align = requested_align;
		field_elem_size = field_size;

		/*
		 * Anonymous aggregate member inside a union:
		 *
		 *     union U {
		 *         struct { u8 a, b; };
		 *         int x;
		 *     };
		 *
		 * There is no declarator name after the anonymous struct body; the
		 * nested fields are promoted as members of the containing union.  Since
		 * this is a union, the anonymous aggregate starts at offset zero, but
		 * its promoted fields keep their offsets within the anonymous struct.
		 */
		if (field_is_struct && lexer_peek()->kind == TOK_SEMI) {
			StructDef *anon = find_struct_or_null(field_struct_name);
			parser_validate_decl_alignment(requested_align, field_type);
			expect(TOK_SEMI);

			if (anon) {
				/*
				 * Preserve the anonymous aggregate itself as the first union
				 * field.  This lets a braced initializer such as:
				 *
				 *     union UV guv = {{6,5}};
				 *
				 * initialize the anonymous struct as one aggregate.  We still
				 * promote its members below so direct access like guv.a/guv.b
				 * and designated initializers such as {.b = 8} continue to work.
				 */
				Field *agg = struct_field_push(def);
				STRNCPY(agg->name, field_struct_name, sizeof(agg->name) - 1);
				agg->offset = 0;
				agg->size = field_size;
				agg->is_struct = 1;
				STRNCPY(agg->struct_name, field_struct_name, sizeof(agg->struct_name) - 1);
				agg->type = type_struct(field_struct_name, field_size);

					for (int ai = 0; ai < anon->field_count; ai++) {
						Field *src = &anon->fields[ai];
						reject_duplicate_aggregate_field(def, src->name);
						Field *dst = struct_field_push(def);
						*dst = *src;
					/* promoted anonymous-struct members live inside the union at offset 0 */
					dst->offset = src->offset;
				}
			}

			if (field_size > max_size)
				max_size = field_size;
			if (field_align > max_align)
				max_align = field_align;
			continue;
		}

		const Token *field = lexer_peek();
		parser_require_decl_identifier(field, "union field name");

		char field_name[64] = {0};
		STRNCPY(field_name, field->text, sizeof(field_name) - 1);

		lexer_next();

		if (lexer_peek()->kind == TOK_LBRACKET) {
			lexer_next();
			if (lexer_peek()->kind == TOK_RBRACKET) {
				fatal_cur("Flexible array member not allowed in union\n");
			}
			int is_constant = 1;
			int dim = eval_const_array_size_checked(&is_constant);
			if (!is_constant)
				fatal_cur("field cannot have variably modified type\n");
			if (dim > 0) {
				field_type = type_array(field_type, dim);
				field_size *= dim;
			}
			expect(TOK_RBRACKET);
		}

			/* Handle comma-separated declarators: HashElem *prev, *next */
			while (1) {
				parser_validate_decl_alignment(requested_align, field_type);
				if (requested_align > field_align)
					field_align = requested_align;
				reject_duplicate_aggregate_field(def, field_name);
				Field *f = struct_field_push(def);
				STRNCPY(f->name, field_name, sizeof(f->name) - 1);
			f->offset = 0;
			f->size = field_size;
			f->is_struct = field_is_struct;
			f->is_array = field_type && field_type->kind == TY_ARRAY;
			f->elem_size = field_elem_size;
			if (field_is_struct) {
				STRNCPY(f->struct_name, field_struct_name, sizeof(f->struct_name) - 1);
				f->type = type_struct(field_struct_name, field_size);
			} else if (field_type) {
				f->type = parser_canonicalize_decl_type(field_type);
			} else {
				f->type = type_for_size(field_size);
			}

			if (field_size > max_size)
				max_size = field_size;
			if (field_align > max_align)
				max_align = field_align;

			if (lexer_peek()->kind != TOK_COMMA) break;
			lexer_next(); /* consume , */
			/* Next declarator: possibly with stars from base type */
			{
				Type *next_field_type = field_type;
				while (next_field_type && next_field_type->kind == TY_PTR)
					next_field_type = next_field_type->base;
				while (lexer_peek()->kind == TOK_STAR) {
					lexer_next();
					next_field_type = type_ptr(next_field_type);
					field_size = TCC_SIZEOF_PTR;
					field_align = parser_apply_pack_alignment(TCC_SIZEOF_PTR);
				}
				field_type = next_field_type;
			}
			const Token *next_field = lexer_peek();
			if (next_field->kind != TOK_IDENT) break;
			parser_require_decl_identifier(next_field, "union field name");
			STRNCPY(field_name, next_field->text ? next_field->text : "", sizeof(field_name) - 1);
			lexer_next();
		}
		expect(TOK_SEMI);
	}

	expect(TOK_RBRACE);
	def->align = max_align;
	def->size = align_to(max_size, max_align);
	def->is_complete = 1;

	/* "union Name { ... } varname [= {...}];" or "varname[] = {...}" */
	if (lexer_peek()->kind == TOK_IDENT || lexer_peek()->kind == TOK_STAR) {
		Global *g = new_global_slot(NULL);
		int ptr = 0;
		int is_arr = 0;
		int arr_len = 0;

		if (lexer_peek()->kind == TOK_STAR) {
			ptr = 1;
			lexer_next();
		}

		const Token *var = lexer_peek();
		parser_require_decl_identifier(var, "global variable name");
		STRNCPY(g->name, var->text, sizeof(g->name) - 1);
		lexer_next();

		if (lexer_peek()->kind == TOK_LBRACKET) {
			lexer_next();
			is_arr = 1;
			if (lexer_peek()->kind == TOK_NUM) {
				arr_len = (int)lexer_peek()->value;
				lexer_next();
			}
			expect(TOK_RBRACKET);
		}

		if (ptr) {
			g->elem_size = TCC_SIZEOF_PTR;
			apply_type_to_global(g, type_ptr(type_union(def->name, def->size)));
		} else {
			g->is_struct = 1;
			g->elem_size = def->size;
			apply_type_to_global(g, type_union(def->name, def->size));
		}
		if (is_arr) {
			g->is_array = 1;
			g->array_len = arr_len > 0 ? arr_len : 1;
			apply_type_to_global(g, type_array(type_union(def->name, def->size), g->array_len));
		} else {
			g->array_len = 1;
		}
		STRNCPY(g->struct_name, def->name, sizeof(g->struct_name) - 1);

		if (lexer_peek()->kind == TOK_ASSIGN) {
			lexer_next();
			if (is_arr) {
				int depth2 = 1;
				int ecount = 0;
				expect(TOK_LBRACE);
				while (depth2 > 0 && lexer_peek()->kind != TOK_EOF) {
					if (lexer_peek()->kind == TOK_LBRACE) {
						depth2++;
						if (depth2 == 2)
							ecount++;
					} else if (lexer_peek()->kind == TOK_RBRACE) {
						depth2--;
					}
					if (depth2 > 0)
						lexer_next();
				}
				if (lexer_peek()->kind == TOK_RBRACE)
					lexer_next();
				if (arr_len == 0)
					arr_len = ecount > 0 ? ecount : 1;
				g->array_len = arr_len;
				apply_type_to_global(g, type_array(type_union(def->name, def->size), g->array_len));
				global_set_init_count(g, arr_len * def->size);
			} else {
				int g_idx = parser_global_index(g);
				expect(TOK_LBRACE);
				global_set_init_count(g, def->size);
				parse_global_struct_initializer_body(g_idx, def, 0);
				g = parser_global_at(g_idx);
				expect(TOK_RBRACE);
			}
		}

		expect(TOK_SEMI);
		commit_global_definition(g);
		return;
	}

	expect(TOK_SEMI);
}

int 
try_parse_prototype(void)
{
	if (!is_prototype_start()) {
		return 0;
	}

	int leading_noreturn = parser_consume_pending_decl_noreturn() ||
	                       (lexer_peek()->kind == TOK_NORETURN);
	Type *ret_type = parse_type_name();
	parser_reject_unsupported_special_type(ret_type);
	parser_reject_unsupported_complex_function_type(ret_type);
	int saw_noreturn = leading_noreturn ||
	                   parser_type_name_saw_trailing_noreturn_specifier();
	TokenKind trailing_storage_class =
	    parser_type_name_trailing_storage_class();
	int saved_file_static = pfunc.file_static;
	/* skip __attribute__((noreturn)) etc. between return type and function name */
	if (trailing_storage_class == TOK_STATIC)
		pfunc.file_static = 1;
	skip_decl_prefix_specifiers();
	ParserFunctionReturnInfo ret_info;
	parser_describe_function_return(ret_type, &ret_info);

	const Token *name = lexer_peek();
	parser_require_decl_identifier(name, "function prototype name");
	char proto_name[64] = {0};
	STRNCPY(proto_name, name->text ? name->text : "", sizeof(proto_name) - 1);

	lexer_next();

	int proto_is_variadic = 0;
	int proto_fixed_params = 0;
	Type **proto_param_types = NULL;
	int proto_param_count = 0;
	int proto_has_prototype = 0;
	parse_prototype_param_list(&proto_param_types, &proto_param_count,
	                          &proto_is_variadic, &proto_fixed_params,
	                          &proto_has_prototype, 1);
	reject_invalid_function_return_declarator();
	skip_decl_prefix_specifiers();

	if (lexer_peek()->kind != TOK_SEMI && lexer_peek()->kind != TOK_COMMA) {
		return 0;
	}

	if (lexer_peek()->kind == TOK_SEMI) lexer_next();
	/* if comma: leave it; caller's comma loop will consume it */

	parser_reject_scope_typedef_name(proto_name);
	FuncInfo *existing = find_func(proto_name);
	parser_validate_function_redeclaration(existing, proto_name,
	                                       ret_type, proto_has_prototype,
	                                       proto_param_types, proto_param_count,
	                                       proto_is_variadic, proto_fixed_params);
	parser_validate_function_linkage_redeclaration(existing, proto_name,
	                                               pfunc.file_static, 0);

	FuncInfo *fi = add_func_info(proto_name, ret_info.returns_struct,
	                             ret_info.struct_name, ret_info.struct_size,
	                             ret_info.returns_pointer, ret_info.return_elem_size,
	                             ret_info.return_abi_class,
	                             ret_info.return_abi_reg_count);
	if (pfunc.file_static)
		fi->is_static = 1;
	parser_record_function_signature(fi, ret_type, proto_has_prototype,
	                                 proto_param_types, proto_param_count,
	                                 proto_is_variadic, proto_fixed_params,
	                                 saw_noreturn);

	if (proto_has_prototype && proto_is_variadic)
		parser_mark_func_variadic(proto_name, proto_fixed_params);

	consume_toplevel_prototype_comma_tail(ret_type, saw_noreturn);
	pfunc.file_static = saved_file_static;

	return 1;
}

static void
consume_toplevel_prototype_comma_tail(Type *ret_type, int is_noreturn)
{
	/* Supports declarations like:
	 *   int f(int), g(int), h;
	 * The first prototype has already been consumed. */
	while (lexer_peek()->kind == TOK_COMMA) {
		ParsedFileScopeDeclarator decl = {0};

		lexer_next();
		parse_file_scope_declarator(ret_type, &decl, "function prototype name");
		if (decl.is_function) {
			skip_decl_prefix_specifiers();
			if (ret_type) {
				parser_declare_function(decl.name, decl.type, decl.has_prototype,
				                        decl.param_types, decl.param_count,
				                        decl.is_variadic, decl.fixed_params,
				                        is_noreturn);
			} else {
				FuncInfo *fi = add_func_info(decl.name, 0, "", 0, 0, 0,
				                             AGGREGATE_ABI_NONE, 0);
				if (pfunc.file_static)
					fi->is_static = 1;
			}
			continue;
		}

		if (decl.type) {
			reject_void_object_type(decl.type, "global");
			Global *gv = new_global_slot(decl.name);
			apply_type_to_global(gv, decl.type);
			if (lexer_peek()->kind == TOK_ASSIGN) {
				lexer_next();
				parse_generic_global_initializer(&gv, decl.type,
				    "Unsupported generic global initializer\n");
			}
			commit_global_definition(gv);
			continue;
		}
	}

	if (lexer_peek()->kind == TOK_SEMI)
		lexer_next();
}

static void
validate_pointer_argument_compatibility(const char *callee_name,
                                        Type **param_types,
                                        int param_type_count,
                                        int is_variadic,
                                        int fixed_param_count,
                                        int arg_index,
                                        Node *arg)
{
	Type *param_type;
	int is_variadic_tail;

	if (!param_types || !arg)
		return;

	is_variadic_tail = is_variadic && arg_index >= fixed_param_count;
	if (is_variadic_tail)
		return;

	if (arg_index < 0 || arg_index >= param_type_count)
		return;

	param_type = param_types[arg_index];
	if (!param_type)
		return;
	if (type_source_is_bool_spelling(param_type))
		return;

	if (type_is_integer(param_type) && arg->type && type_is_pointer(arg->type)) {
		if (callee_name && callee_name[0]) {
			fatal_cur("Incompatible pointer to integer conversion for argument %d of '%s'\n",
			          arg_index + 1,
			          callee_name);
		}

		fatal_cur("Incompatible pointer to integer conversion for argument %d of indirect call\n",
		          arg_index + 1);
	}

	if (type_is_pointer(param_type) && !node_is_null_pointer_constant(arg) &&
	    arg->type && type_is_integer(arg->type)) {
		if (callee_name && callee_name[0]) {
			fatal_cur("Incompatible integer to pointer conversion for argument %d of '%s'\n",
			          arg_index + 1,
			          callee_name);
		}

		fatal_cur("Incompatible integer to pointer conversion for argument %d of indirect call\n",
		          arg_index + 1);
	}

	if (!type_is_pointer(param_type))
		return;

	if (!node_is_null_pointer_constant(arg) &&
	    (!arg->type || !type_is_pointer(arg->type)))
		return;

	if (type_pointer_assignment_compatible(param_type,
	                                       arg->type,
	                                       node_is_null_pointer_constant(arg)))
		return;
	if (callee_name && callee_name[0]) {
		fatal_cur("Incompatible pointer type for argument %d of '%s'\n",
		          arg_index + 1,
		          callee_name);
	}
	fatal_cur("Incompatible pointer type for argument %d of indirect call\n",
	          arg_index + 1);
}

static Node *
apply_argument_conversion(Type **param_types, int param_type_count,
                          int is_variadic, int fixed_param_count,
                          int arg_index, Node *arg)
{
	Type *param_type;
	int is_variadic_tail;
	Type *promoted;

	if (!arg || !arg->type)
		return arg;

	is_variadic_tail = is_variadic && arg_index >= fixed_param_count;
	if (is_variadic_tail) {
		if (arg->type->kind == TY_FLOAT)
			return new_cast(arg, type_double());
		if (arg->type->kind == TY_CHAR || arg->type->kind == TY_SHORT)
			return new_cast(arg, type_int());
		return arg;
	}

	if (!param_types || arg_index < 0 || arg_index >= param_type_count)
		return arg;

	param_type = param_types[arg_index];
	if (!param_type || type_is_struct(param_type) || type_is_union(param_type))
		return arg;
	if (!type_is_scalar(param_type) || !type_is_scalar(arg->type))
		return arg;
	if (type_source_is_bool_spelling(param_type))
		return expr_coerce_value_for_type(arg, param_type);
	if (type_is_pointer(param_type) || type_is_pointer(arg->type))
		return arg;
	if (type_equal_unqualified(param_type, arg->type))
		return arg;

	promoted = clone_type(param_type);
	return new_cast(arg, promoted);
}

static Node *
parse_arg_list_signature(const char *callee_name,
                         Type **param_types,
                         int param_type_count,
                         int is_variadic,
                         int fixed_param_count)
{
	Node head = {0};
	Node *cur = &head;
	int arg_index = 0;
	int x86_struct_by_value = parser_target_is_x86();
	int allow_direct_aggregate_abi = parser_target_is_arm64();

	if (lexer_peek()->kind == TOK_RPAREN)
		return NULL;

	for (;;) {
		Node *arg = parse_expr();
		validate_pointer_argument_compatibility(callee_name, param_types,
		                                       param_type_count, is_variadic,
		                                       fixed_param_count, arg_index, arg);
		arg = apply_argument_conversion(param_types, param_type_count,
		                               is_variadic, fixed_param_count,
		                               arg_index, arg);
		if (arg->type && type_is_struct(arg->type)) {
			int abi_class = parser_classify_aggregate_abi(arg->type, NULL);
			int use_direct_aggregate_abi =
			    x86_struct_by_value ||
			    (allow_direct_aggregate_abi &&
			     (abi_class == AGGREGATE_ABI_INTREGS ||
			      abi_class == AGGREGATE_ABI_HFA));

			/* If arg is a struct-returning call, spill to temp first */
			if (arg->kind == ND_CALL && arg->returns_struct) {
				char temp_name[64];
				snprintf(temp_name, sizeof(temp_name), "__struct_arg_%d", pfunc.struct_arg_temp_id++);
				const char *sname = arg->return_struct_name[0] ? arg->return_struct_name
				                    : (arg->type ? arg->type->struct_name : "");
				StructDef *def = find_struct(sname);
				int sz = def ? def->size : 8;
				int temp_off = add_struct_local(temp_name, sname);

				Node *lhs = new_var(temp_name, temp_off);
				lhs->type = type_struct(sname, sz);
				lhs->elem_size = sz;
				STRNCPY(lhs->struct_name, sname, sizeof(lhs->struct_name) - 1);

				Node *assign = new_assign(lhs, arg);

				Node *temp_ref = new_var(temp_name, temp_off);
				temp_ref->type = type_struct(sname, sz);
				temp_ref->elem_size = sz;
				STRNCPY(temp_ref->struct_name, sname, sizeof(temp_ref->struct_name) - 1);
				if (use_direct_aggregate_abi) {
					arg = new_binary(ND_COMMA, assign, temp_ref);
					arg->type = temp_ref->type;
					arg->elem_size = temp_ref->elem_size;
				} else {
					Node *addr = new_addr(temp_ref);
					addr->by_ref_arg = 1;
					arg = new_binary(ND_COMMA, assign, addr);
					arg->type = addr->type;
				}
			} else {
				if (use_direct_aggregate_abi &&
				    arg->kind != ND_VAR &&
				    arg->kind != ND_GLOBAL) {
					char temp_name[64];
					const char *sname = arg->type ? arg->type->struct_name : "";
					StructDef *def = find_struct(sname);
					int sz = def ? def->size : type_sizeof(arg->type);
					int temp_off;
					Node *lhs;
					Node *assign;
					Node *temp_ref;

					snprintf(temp_name, sizeof(temp_name),
					         "__struct_arg_%d", pfunc.struct_arg_temp_id++);
					temp_off = add_struct_local(temp_name, sname);

					lhs = new_var(temp_name, temp_off);
					lhs->type = type_struct(sname, sz);
					lhs->elem_size = sz;
					STRNCPY(lhs->struct_name, sname, sizeof(lhs->struct_name) - 1);

					assign = new_assign(lhs, arg);

					temp_ref = new_var(temp_name, temp_off);
					temp_ref->type = type_struct(sname, sz);
					temp_ref->elem_size = sz;
					STRNCPY(temp_ref->struct_name, sname, sizeof(temp_ref->struct_name) - 1);

					arg = new_binary(ND_COMMA, assign, temp_ref);
					arg->type = temp_ref->type;
					arg->elem_size = temp_ref->elem_size;
				} else if (!use_direct_aggregate_abi) {
					arg = new_addr(arg);
					arg->by_ref_arg = 1;
				}
			}
		}

		cur->next = arg;
		cur = cur->next;
		arg_index++;

		if (lexer_peek()->kind != TOK_COMMA)
			break;

		lexer_next();
	}

	return head.next;
}

Node *
parse_arg_list(FuncInfo *callee_info)
{
	const char *callee_name = "";
	Type **param_types = NULL;
	int param_type_count = 0;
	int is_variadic = 0;
	int fixed_param_count = 0;

	if (callee_info) {
		callee_name = callee_info->name;
		param_types = callee_info->param_types;
		param_type_count = callee_info->param_type_count;
		is_variadic = callee_info->is_variadic;
		fixed_param_count = callee_info->fixed_param_count;
	}

	return parse_arg_list_signature(callee_name, param_types,
	                                param_type_count, is_variadic,
	                                fixed_param_count);
}

Node *
parse_arg_list_for_type(Type *func_type, const char *callee_name)
{
	Type **param_types = NULL;
	int param_type_count = 0;
	int is_variadic = 0;
	int fixed_param_count = 0;

	if (func_type && type_is_function(func_type))
		type_func_metadata(func_type, &param_types, &param_type_count,
		                   &is_variadic, &fixed_param_count);

	return parse_arg_list_signature(callee_name, param_types,
	                                param_type_count, is_variadic,
	                                fixed_param_count);
}

static const char *
ensure_current_func_name_global(void)
{
	static char gname[128];

	if (!pfunc.func_name[0])
		return "";

	snprintf(gname, sizeof(gname), "__func__%s", pfunc.func_name);

	if (find_global(gname))
		return gname;

	Global *g = new_global_slot(gname);
	g->is_array = 1;
	g->elem_size = 1;
	set_global_string_array_initializer(g, pfunc.func_name);
	g->is_static = 1;
	global_set_init_count(g, (int)strlen(pfunc.func_name) + 1);
	g->array_len = global_init_count(g);
	apply_type_to_global(g, type_array(type_char(), g->array_len));
	punit.global_count++;
	parser_global_hash_note_new_index(punit.global_count - 1);
	parser_invalidate_global_lookup_cache();

	return gname;
}

static Type *
global_aggregate_type(const char *name)
{
	Global *g = parser_find_global_optional(name);
	const char *sname = global_struct_name(name);
	StructDef *def;
	int size;

	if (!sname[0])
		return NULL;
	size = g && g->ptr_elem_size > 0 ? g->ptr_elem_size : global_elem_size(name);
	def = find_struct_or_null(sname);
	if (def && def->is_union)
		return type_union(sname, size);
	return type_struct(sname, size);
}

static Node *
make_func_name_var_node(void)
{
	const char *gname;
	Node *node;

	if (tcc_lang_is_c89_or_c90())
		fatal_cur("__func__ is not allowed in C89/C90 mode\n");
	gname = ensure_current_func_name_global();
	node = new_func_addr(gname);
	node->is_pointer = 1;
	node->elem_size = 1;
	node->type = type_ptr(type_char());
	return node;
}

Node *
make_scalar_var_node_resolved(const char *name, Local *local, Global *global)
{
	if (local && local->is_static) {
		const char *gname = local->static_global_name;
		Type *local_type = local->type;
		if (local->is_array) {
			Node *node = new_func_addr(gname);
			Type *base_type = local_type && type_is_array(local_type) && type_pointee(local_type)
			                ? type_pointee(local_type)
			                : type_for_size(local->elem_size ? local->elem_size : 4);
			node->is_pointer = 1;
			node->elem_size = type_sizeof(base_type);
			node->type = type_ptr(clone_type(base_type));
			if (type_is_struct(base_type) && base_type->struct_name[0])
				STRNCPY(node->struct_name, base_type->struct_name, sizeof(node->struct_name) - 1);
			return node;
		}
		{
			Node *node = new_global(gname);
			node->elem_size = local->elem_size ? local->elem_size : 4;
			node->type = local_type ? clone_type(local_type) : type_for_size(node->elem_size);
			node->is_pointer = local_type && type_is_pointer(local_type);
			if (node->is_pointer && type_pointee(local_type) &&
			    type_is_struct(type_pointee(local_type)) &&
			    type_pointee(local_type)->struct_name[0]) {
				STRNCPY(node->struct_name, type_pointee(local_type)->struct_name,
				        sizeof(node->struct_name) - 1);
			} else if (local_type && type_is_struct(local_type) && local_type->struct_name[0]) {
				STRNCPY(node->struct_name, local_type->struct_name, sizeof(node->struct_name) - 1);
			}
			return node;
		}
	}

	if (local) {
		Node *node = new_var(name, local->offset);
		node->type = local->type ? local->type : type_int();
		node->is_unsigned = node->type && node->type->is_unsigned;
		node->is_const_lvalue = node->type && type_has_qualifier(node->type, TYPE_QUAL_CONST);

		if (local->is_array) {
			Node *addr = new_addr(node);
			addr->is_pointer = 1;
			addr->elem_size = local->elem_size ? local->elem_size : 4;
			if (node->type && node->type->kind == TY_ARRAY)
				addr->type = type_ptr(node->type->base);
			return addr;
		}

		node->is_pointer = local->is_pointer;
		node->elem_size = local->elem_size ? local->elem_size : 4;
		if (local->struct_name[0])
			STRNCPY(node->struct_name, local->struct_name, sizeof(node->struct_name) - 1);
		return node;
	}

	if (global) {
		Type *gt = global->type;
		int global_is_ptr = (gt && type_is_pointer(gt)) || global->ptr_elem_size > 0 || global->is_string;
		if (global->is_array && !global_is_ptr) {
			Node *node = new_func_addr(name);
			node->is_pointer = 1;
			node->type = global_array_decay_type(name, &node->elem_size);
			return node;
		}
		{
			Node *node = new_global(name);
			node->is_pointer = global_is_ptr;
			node->elem_size = global_is_ptr
			                ? (global->ptr_elem_size ? global->ptr_elem_size : 1)
			                : global->elem_size;
			node->is_unsigned = global->is_unsigned;
			if (global->is_struct) {
				node->type = global_aggregate_type(name);
				if (global->struct_name[0])
					STRNCPY(node->struct_name, global->struct_name, sizeof(node->struct_name) - 1);
			} else if (global_is_ptr && global->struct_name[0]) {
				Type *agg = global_aggregate_type(name);
				node->type = agg ? type_ptr(agg)
				                 : (gt ? gt : type_ptr(type_for_size(node->elem_size)));
				STRNCPY(node->struct_name, global->struct_name, sizeof(node->struct_name) - 1);
			} else if (gt) {
				node->type = gt;
				if ((type_is_struct(gt) || type_is_union(gt) ||
				     (type_is_pointer(gt) && type_pointee(gt) &&
				      (type_is_struct(type_pointee(gt)) || type_is_union(type_pointee(gt))))) &&
				    global->struct_name[0]) {
					STRNCPY(node->struct_name, global->struct_name, sizeof(node->struct_name) - 1);
				}
			} else {
				node->type = global_is_ptr ? type_ptr(type_for_size(node->elem_size))
				                           : type_for_size_unsigned(global->elem_size, global->is_unsigned);
			}
			node->is_const_lvalue = node->type && type_has_qualifier(node->type, TYPE_QUAL_CONST);
			return node;
		}
	}

	if (find_func(name)) {
		Node *fnode = parser_make_function_designator(name);
		fnode->is_pointer = 1;
		fnode->elem_size = TCC_SIZEOF_PTR;
		return fnode;
	}

	{
		Node *node = new_var(name, find_local(name));
		node->type = type_local(name);
		node->is_unsigned = node->type && node->type->is_unsigned;
		node->is_pointer = is_pointer_local(name);
		node->elem_size = elem_size_local(name);
		node->is_const_lvalue = node->type && type_has_qualifier(node->type, TYPE_QUAL_CONST);
		if (is_struct_local(name))
			STRNCPY(node->struct_name, struct_name_local_optional(name), sizeof(node->struct_name) - 1);
		if (node->is_pointer && struct_name_local_optional(name)[0])
			STRNCPY(node->struct_name, struct_name_local_optional(name), sizeof(node->struct_name) - 1);
		return node;
	}
}

Node *
make_var_node(const char *name)
{
	Local *local;
	Global *global;

	/* __func__ — implicitly defined per C99 §6.4.2.2.
	 * Register the backing string lazily only when __func__ is referenced. */
	if (STRCMP(name, "__func__") == 0 && pfunc.func_name[0])
		return make_func_name_var_node();

	/*
	 * A local variable or parameter must shadow a global with the same name.
	 *
	 * This matters for tests/torture/00216.c:
	 *
	 *     struct pkthdr phdr = ...;             // global object
	 *     void foo(..., struct pkthdr *phdr_) {
	 *         const struct pkthdr *phdr = phdr_; // local pointer
	 *         struct flowi6 flow = { .daddr = phdr->daddr, ... };
	 *     }
	 */
	local = parser_find_local_latest_optional(name);
	global = local ? NULL : parser_find_global_optional(name);
	return make_scalar_var_node_resolved(name, local, global);
}

Node *
make_scalar_var_node(const char *name)
{
	/* __func__ — implicitly defined per C99 §6.4.2.2 */
	if (STRCMP(name, "__func__") == 0 && pfunc.func_name[0]) {
		/* __func__ is a char[] — we want its address, not its contents.
		 * Use ND_FUNC_ADDR so the IR emits addr_global (adrp+add) not a load. */
		return make_func_name_var_node();
	}

	{
		Local *local = parser_find_local_latest_optional(name);
		Global *global = local ? NULL : parser_find_global_optional(name);
		return make_scalar_var_node_resolved(name, local, global);
	}
}

Node *
parse_struct_compound_literal_member_expr(void)
{
	Type *compound_type;
	Type *effective_type;
	const char *aggregate_name;
	StructDef *def;
	int outer_paren = 0;
	int nfields;
	Node **values;
	int field_index = 0;

	/*
	 * v112 expression subset:
	 *
	 *   ((struct Point){ .x = 1, .y = 41 }).y
	 *   ((S){ .x = 1, .y = 41 }).y
	 *   ((U){ .i = 7 }).i
	 *
	 * This supports direct member reads from a struct/union compound literal.
	 * It does not materialize a temporary aggregate; instead it returns the
	 * selected initializer expression, defaulting omitted fields to 0.
	 */
	if (lexer_peek()->kind != TOK_LPAREN)
		return NULL;
	if (lexer_peek_ahead(1)->kind == TOK_LPAREN) {
		if (!parser_type_start_is_aggregate(lexer_peek_ahead(2)))
			return NULL;
		outer_paren = 1;
	} else if (!parser_type_start_is_aggregate(lexer_peek_ahead(1))) {
		return NULL;
	}

	if (tcc_lang_is_c89_or_c90())
		fatal_cur("compound literals are not allowed in C89/C90 mode\n");

	lexer_next(); /* ( */
	if (outer_paren)
		lexer_next(); /* inner ( */

	compound_type = parse_type_name();
	parser_reject_unsupported_special_type(compound_type);
	effective_type = parser_canonicalize_decl_type(compound_type);
	if (!effective_type ||
	    (!type_is_struct(effective_type) && !type_is_union(effective_type))) {
		fatal_cur("Expected struct name in compound literal\n");
	}

	aggregate_name = parser_resolve_struct_type_name(effective_type);
	if (!aggregate_name || !aggregate_name[0])
		fatal_cur("Expected struct name in compound literal\n");
	def = find_struct(aggregate_name);
	if (!def)
		fatal_cur("Expected struct name in compound literal\n");

	expect(TOK_RPAREN);

	if (lexer_peek()->kind != TOK_LBRACE) {
		fatal_cur("Expected initializer list in compound literal\n");
	}
	lexer_next();

	nfields = def ? def->field_count : 0;
	if (nfields < 1) nfields = 1;
	values = (Node **)xcalloc((size_t)nfields, sizeof(Node *));

	while (lexer_peek()->kind != TOK_RBRACE) {
		Field *field = NULL;
		int selected = -1;

		if (lexer_peek()->kind == TOK_DOT) {
			reject_c89_designated_initializer();
			lexer_next();

			const Token *field_name = lexer_peek();
			if (field_name->kind != TOK_IDENT) {
				fatal_cur("Expected field name after '.' in compound literal\n");
			}
			lexer_next();
			expect(TOK_ASSIGN);

			field = find_field(aggregate_name, field_name->text);
			for (int i = 0; i < def->field_count; i++) {
				if (&def->fields[i] == field) {
					selected = i;
					break;
				}
			}
		} else {
			if (field_index >= def->field_count) {
				fatal_cur("Too many values in compound literal for %s\n", aggregate_name);
			}

			selected = field_index++;
			field = &def->fields[selected];
		}

		if (selected < 0 || selected >= nfields) {
			fatal_cur("Invalid compound literal field\n");
		}

		values[selected] = parse_expr();

		if (lexer_peek()->kind == TOK_COMMA) {
			lexer_next();
			if (lexer_peek()->kind == TOK_RBRACE)
				break;
		} else {
			break;
		}
	}

	expect(TOK_RBRACE);

	/*
	 * Accept both:
	 *
	 *   (struct Point){ .x = 1 }.x
	 *   ((struct Point){ .x = 1 }).x
	 *
	 * The second form arrives here with one extra ')' before the member dot.
	 */
	if (outer_paren && lexer_peek()->kind == TOK_RPAREN && lexer_peek_ahead(1)->kind == TOK_DOT)
		lexer_next();

	if (lexer_peek()->kind != TOK_DOT) {
		fatal_cur("Only direct member access on struct compound literals is supported for now\n");
	}

	lexer_next();

	const Token *member_name = lexer_peek();
	if (member_name->kind != TOK_IDENT) {
		fatal_cur("Expected field name after compound literal '.'\n");
	}
	lexer_next();

	Field *wanted = find_field(aggregate_name, member_name->text);
	int wanted_index = -1;
	for (int i = 0; i < def->field_count; i++) {
		if (&def->fields[i] == wanted) {
			wanted_index = i;
			break;
		}
	}

	if (wanted_index < 0 || wanted_index >= nfields) {
		fatal_cur("Invalid compound literal selected field\n");
	}

	if (!values[wanted_index])
		return new_num(0);

	values[wanted_index]->type = type_for_size(wanted->size);
	values[wanted_index]->elem_size = wanted->size;
	return values[wanted_index];
}


static void
parser_mark_func_variadic(const char *name, int fixed_param_count)
{
	for (int i = ptab.func_count - 1; i >= 0; i--) {
		if (STRCMP(ptab.funcs[i].name, name) == 0) {
			ptab.funcs[i].is_variadic = 1;
			ptab.funcs[i].fixed_param_count = fixed_param_count;
			break;
		}
	}
}

static void
parser_describe_function_return(Type *ret_type, ParserFunctionReturnInfo *info)
{
	const char *resolved_ret_struct_name = "";
	const char *direct_hfa_name = NULL;
	int hfa_elem_count = 0;

	memset(info, 0, sizeof(*info));
	info->return_elem_size = TCC_SIZEOF_INT;

	if (!ret_type)
		return;

	if (type_is_struct(ret_type)) {
		int reg_count = 0;

		info->returns_struct = 1;
		info->struct_size = ret_type->size;
		info->return_abi_class = parser_classify_aggregate_abi(ret_type, &reg_count);
		info->return_abi_reg_count = reg_count;
		resolved_ret_struct_name = parser_resolve_struct_type_name(ret_type);
		if (resolved_ret_struct_name[0]) {
			STRNCPY(info->struct_name, resolved_ret_struct_name,
			        sizeof(info->struct_name) - 1);
		}
		return;
	}
	if (parser_target_is_arm64() &&
	    parser_direct_complex_lane_info(ret_type, NULL, &hfa_elem_count) &&
	    !type_is_struct(ret_type)) {
		direct_hfa_name = parser_arm64_direct_complex_abi_name(ret_type);
		info->returns_struct = 1;
		info->struct_size = type_sizeof(ret_type);
		info->return_abi_class = AGGREGATE_ABI_HFA;
		info->return_abi_reg_count = hfa_elem_count;
		if (direct_hfa_name)
			STRNCPY(info->struct_name, direct_hfa_name, sizeof(info->struct_name) - 1);
		return;
	}
	if (parser_target_is_x64() &&
	    type_is_complex(ret_type) &&
	    type_sizeof(ret_type) == 8) {
		info->return_elem_size = 8;
		return;
	}
	if (parser_target_is_x64() &&
	    type_is_complex(ret_type) &&
	    type_sizeof(ret_type) == 16) {
		info->returns_struct = 1;
		info->struct_size = 16;
		info->return_abi_class = AGGREGATE_ABI_X64_COMPLEX_DOUBLE;
		info->return_abi_reg_count = 2;
		STRNCPY(info->struct_name, "__tcc_x64_complex_double2",
		        sizeof(info->struct_name) - 1);
		return;
	}

	if (ret_type->kind == TY_PTR) {
		info->returns_pointer = 1;
		info->return_elem_size = ret_type->base ? ret_type->base->size : 4;
		return;
	}

	if (ret_type->kind == TY_CHAR) {
		info->return_elem_size = 1;
		return;
	}

	info->return_elem_size = ret_type->size ? ret_type->size : 4;
}

static Node *
parse_function(void)
{
	parser_profile_scope_enter(PARSER_PROF_FUNCTION_HEAD);
	ParserScopeMark function_scope = {0};

	int leading_noreturn = parser_consume_pending_decl_noreturn() ||
	                       (lexer_peek()->kind == TOK_NORETURN);
	skip_decl_prefix_specifiers();

	/* pscope.locals[] entries are zeroed by locals_push() on allocation;
	 * resetting pscope.local_count is sufficient to retire them for the next function. */
	parser_reset_local_scope_state();
	parser_reset_param_copy_state(1);
	pfunc.returns_struct = 0;
	pfunc.return_abi_class = AGGREGATE_ABI_NONE;
	pfunc.return_abi_reg_count = 0;
	pfunc.return_size = 0;
	pfunc.return_struct_name[0] = '\0';
	pfunc.returns_pointer = 0;
	pfunc.return_elem_size = TCC_SIZEOF_INT;
	pfunc.return_type = NULL;
	pfunc.is_noreturn = 0;
	stmt_begin_function();
	parser_mark_local_scope(&function_scope);

	Type *ret_type = parse_type_name();
	parser_reject_unsupported_special_type(ret_type);
	parser_reject_unsupported_complex_function_type(ret_type);
	int saw_noreturn = leading_noreturn ||
	                   parser_type_name_saw_trailing_noreturn_specifier();
	int saw_trailing_inline =
	    parser_type_name_saw_trailing_inline_specifier();
	pfunc.is_noreturn = saw_noreturn;
	ParserFunctionReturnInfo ret_info;
	pfunc.return_type = parser_canonicalize_decl_type(ret_type);
	if (parser_type_name_trailing_storage_class() == TOK_STATIC)
		pfunc.file_static = 1;
	if (parser_type_name_trailing_storage_class() == TOK_EXTERN &&
	    saw_trailing_inline) {
		pfunc.file_static = 1;
		pfunc.gnu_extern_inline_definition = 1;
	}

	/* skip __attribute__((noreturn)) etc. between return type and function name */
	skip_decl_prefix_specifiers();
	parser_describe_function_return(ret_type, &ret_info);
	pfunc.returns_struct = ret_info.returns_struct;
	pfunc.return_abi_class = ret_info.return_abi_class;
	pfunc.return_abi_reg_count = ret_info.return_abi_reg_count;
	pfunc.return_size = ret_info.struct_size;
	pfunc.returns_pointer = ret_info.returns_pointer;
	pfunc.return_elem_size = ret_info.return_elem_size;
	if (ret_info.struct_name[0]) {
		STRNCPY(pfunc.return_struct_name, ret_info.struct_name,
		        sizeof(pfunc.return_struct_name) - 1);
	}

	const Token *name = lexer_peek();
	int parenthesized_return_pointer_decl = 0;
	if (name->kind == TOK_LPAREN &&
	    lexer_peek_ahead(1)->kind == TOK_STAR &&
	    lexer_peek_ahead(2)->kind == TOK_IDENT &&
	    lexer_peek_ahead(3)->kind == TOK_LPAREN) {
		parenthesized_return_pointer_decl = 1;
		expect(TOK_LPAREN);
		expect(TOK_STAR);
		name = lexer_peek();
	}
	parser_require_decl_identifier(name, "function name");

	/*
	 * Do not keep using name->text after parsing the function body.
	 * name points into the lexer's token ring, and body parsing performs
	 * enough lexer_peek()/lexer_next() calls to overwrite that token slot.
	 * Capture the declaration line and filename now, before lexer_next()
	 * advances past the name token.
	 */
	char function_name[64];
	STRNCPY(function_name, name->text, sizeof(function_name) - 1);
	int fn_decl_line = name->line;
	int fn_decl_filename_id = name->filename_id;

	lexer_next();

	STRNCPY(pfunc.function_name, function_name, sizeof(pfunc.function_name) - 1);
	FuncInfo *existing_func = find_func(function_name);
	if (existing_func && existing_func->is_noreturn)
		pfunc.is_noreturn = 1;
	Type *effective_ret_type = parser_canonicalize_decl_type(ret_type);

	FuncInfo *func_info = add_func_info(function_name, pfunc.returns_struct,
	                                   pfunc.return_struct_name, pfunc.return_size,
	                                   pfunc.returns_pointer, pfunc.return_elem_size,
	                                   pfunc.return_abi_class,
	                                   pfunc.return_abi_reg_count);
	if (parser_trace_toplevel_enabled()) {
		fprintf(stderr, "tcc parse: function-head done name=%s func_info=%p\n",
		        function_name, (void *)func_info);
	}
	parser_profile_scope_leave(PARSER_PROF_FUNCTION_HEAD);

	parser_profile_scope_enter(PARSER_PROF_FUNCTION_PARAMS);
	if (parser_trace_toplevel_enabled()) {
		fprintf(stderr, "tcc parse: function-params enter name=%s next=%s\n",
		        function_name,
		        token_debug_name(lexer_peek()->kind));
	}
	expect(TOK_LPAREN);

	int param_count = 0;
	int source_param_count = 0;
	char **param_names = NULL;
	int param_name_cap = 0;
	int *param_type_ids = NULL;
	int param_type_cap = 0;
	int *param_offsets = NULL;
	int param_offset_cap = 0;
	int *param_abi_sizes = NULL;
	int param_abi_cap = 0;
	Type **param_types = NULL;
	int param_types_cap = 0;
	char **param_struct_names = NULL;
	int param_struct_cap = 0;
	char **param_pointer_struct_names = NULL;
	int *param_pointer_depths = NULL;
	int param_pointer_cap = 0;
	int has_prototype = 0;
	int oldstyle_param_list = 0;
	char **oldstyle_param_names = NULL;
	int oldstyle_param_name_cap = 0;
	Type **oldstyle_promoted_types = NULL;
	int oldstyle_promoted_count = 0;
	int oldstyle_promoted_cap = 0;

	if (pfunc.returns_struct &&
	    !(parser_target_is_arm64() &&
	      (pfunc.return_abi_class == AGGREGATE_ABI_INTREGS ||
	       pfunc.return_abi_class == AGGREGATE_ABI_HFA)) &&
	    !(parser_target_is_x64() &&
	      pfunc.return_abi_class == AGGREGATE_ABI_X64_COMPLEX_DOUBLE)) {
		int ret_param_offset = add_struct_pointer_local("__ret", pfunc.return_struct_name);
		/* Keep the hidden structure-return ABI parameter unnamed in DWARF. */
		parser_set_param_name(&param_names, &param_name_cap, param_count, NULL);
		parser_set_param_type_id(&param_type_ids, &param_type_cap, param_count, DBG_TYPE_PTR_VOID);
		parser_set_param_offset(&param_offsets, &param_offset_cap, param_count, ret_param_offset);
		parser_set_param_offset(&param_abi_sizes, &param_abi_cap, param_count,
		                        parser_target_is_x86() ? 4 : TCC_SIZEOF_PTR);
		parser_set_param_struct(&param_struct_names, &param_struct_cap, param_count, NULL);
		parser_set_param_pointer_struct(&param_pointer_struct_names, &param_pointer_depths,
		                                &param_pointer_cap, param_count, NULL, 0);
		param_count++;
	}

	if (lexer_peek()->kind == TOK_VOID && lexer_peek_ahead(1)->kind == TOK_RPAREN) {
		lexer_next();
		has_prototype = 1;
	} else if (lexer_peek()->kind != TOK_RPAREN) {
		if (looks_like_oldstyle_param_name_list()) {
			oldstyle_param_list = 1;
			for (;;) {
				const Token *param = lexer_peek();
				parser_set_param_name(&oldstyle_param_names, &oldstyle_param_name_cap,
				                      source_param_count, param->text);
				param_type_list_append(&param_types, &source_param_count,
				                       &param_types_cap, NULL);
				lexer_next();
				if (lexer_peek()->kind != TOK_COMMA)
					break;
				lexer_next();
			}
		} else {
			has_prototype = 1;
			for (;;) {
				char param_name_buf[64];
				Type *param_type;
				int param_is_register = 0;

				param_type = parse_parameter_declarator_impl(param_name_buf, 0,
				                                             &param_is_register);

				parser_register_function_parameter(param_name_buf, param_type,
				                                   param_is_register,
				                                   &param_count,
				                                   &param_names, &param_name_cap,
				                                   &param_type_ids, &param_type_cap,
				                                   &param_offsets, &param_offset_cap,
				                                   &param_abi_sizes, &param_abi_cap,
				                                   &param_struct_names, &param_struct_cap,
				                                   &param_pointer_struct_names,
				                                   &param_pointer_depths, &param_pointer_cap);
				param_type_list_append(&param_types, &source_param_count,
				                       &param_types_cap, param_type);

				if (lexer_peek()->kind != TOK_COMMA)
					break;

				lexer_next();

				if (lexer_peek()->kind == TOK_DOT &&
				        lexer_peek_ahead(1)->kind == TOK_DOT &&
				        lexer_peek_ahead(2)->kind == TOK_DOT) {
					lexer_next();
					lexer_next();
					lexer_next();

					parser_mark_func_variadic(function_name, param_count);
					break;
				}
			}
		}
	}

	expect(TOK_RPAREN);
	if (parser_trace_toplevel_enabled()) {
		fprintf(stderr, "tcc parse: function-params done name=%s param_count=%d source_count=%d\n",
		        function_name, param_count, source_param_count);
	}
	if (oldstyle_param_list) {
		while (lexer_peek()->kind != TOK_LBRACE) {
			Type *decl_base = parse_type_name();
			Type *shared_base = oldstyle_declarator_shared_base(decl_base);
			int first_decl = 1;

			for (;;) {
				char param_name_buf[64];
				Type *param_type;
				int param_index;
				Type *declarator_base = first_decl ? clone_type(decl_base)
				                                   : clone_type(shared_base);

				param_name_buf[0] = '\0';
				param_type = parse_parameter_declarator_suffix(declarator_base,
				                                              param_name_buf, 0);
				if (param_name_buf[0] == '\0')
					fatal_cur("Expected parameter name\n");
				param_index = find_oldstyle_param_index(oldstyle_param_names,
				                                        source_param_count,
				                                        param_name_buf);
				if (param_index < 0)
					fatal_cur("Declaration for unknown K&R parameter '%s'\n",
					          param_name_buf);
				if (param_types[param_index])
					fatal_cur("Duplicate declaration for K&R parameter '%s'\n",
					          param_name_buf);
				param_types[param_index] = parser_canonicalize_decl_type(param_type);
				first_decl = 0;

				if (lexer_peek()->kind != TOK_COMMA)
					break;
				lexer_next();
			}
			expect(TOK_SEMI);
		}
		for (int i = 0; i < source_param_count; i++) {
			if (!param_types[i])
				param_types[i] = type_int();
			param_type_list_append(&oldstyle_promoted_types, &oldstyle_promoted_count,
			                       &oldstyle_promoted_cap,
			                       oldstyle_promoted_param_type(param_types[i]));
		}
		parser_validate_function_redeclaration(existing_func,
		                                       function_name,
		                                       effective_ret_type,
		                                       1,
		                                       oldstyle_promoted_types,
		                                       source_param_count,
		                                       0,
		                                       0);
		parser_validate_function_linkage_redeclaration(existing_func,
		                                               function_name,
		                                               pfunc.file_static,
		                                               pfunc.gnu_extern_inline_definition);
		for (int i = 0; i < source_param_count; i++) {
				parser_register_function_parameter(oldstyle_param_names[i], param_types[i],
				                                   0,
				                                   &param_count,
				                                   &param_names, &param_name_cap,
				                                   &param_type_ids, &param_type_cap,
			                                   &param_offsets, &param_offset_cap,
			                                   &param_abi_sizes, &param_abi_cap,
			                                   &param_struct_names, &param_struct_cap,
			                                   &param_pointer_struct_names,
			                                   &param_pointer_depths, &param_pointer_cap);
		}
	}
	if (parenthesized_return_pointer_decl) {
		if (parser_trace_toplevel_enabled()) {
			fprintf(stderr, "tcc parse: function-retptr-adjust enter name=%s\n",
			        function_name);
		}
		expect(TOK_RPAREN);
		if (lexer_peek()->kind == TOK_LBRACKET) {
			int dims[MAX_ARRAY_DIMS] = {0};
			int dim_count = parse_array_dimensions(dims, 0, 0);
			Type *array_type = build_array_type_from_dims(clone_type(ret_type), dims, dim_count);
			effective_ret_type = type_ptr(array_type);
		} else if (lexer_peek()->kind == TOK_LPAREN) {
			effective_ret_type = parser_parse_returned_function_pointer_type(ret_type);
		} else {
			fatal_cur("Expected pointer-return declarator suffix after function declarator\n");
		}
		pfunc.returns_struct = 0;
		pfunc.return_abi_class = AGGREGATE_ABI_NONE;
		pfunc.return_abi_reg_count = 0;
		pfunc.return_size = 0;
		pfunc.return_struct_name[0] = '\0';
		pfunc.returns_pointer = 1;
		pfunc.return_elem_size = TCC_SIZEOF_PTR;
		pfunc.return_type = parser_canonicalize_decl_type(effective_ret_type);
		if (parser_trace_toplevel_enabled()) {
			fprintf(stderr, "tcc parse: function-retptr-adjust done name=%s\n",
			        function_name);
		}
	}
	if (!oldstyle_param_list) {
		if (parser_trace_toplevel_enabled()) {
			fprintf(stderr, "tcc parse: function-validate-redecl enter name=%s\n",
			        function_name);
		}
		parser_validate_function_redeclaration(existing_func, function_name,
		                                       effective_ret_type, has_prototype,
		                                       param_types, source_param_count,
		                                       func_info->is_variadic,
		                                       func_info->fixed_param_count);
		if (parser_trace_toplevel_enabled()) {
			fprintf(stderr, "tcc parse: function-validate-redecl done name=%s\n",
			        function_name);
		}
		if (parser_trace_toplevel_enabled()) {
			fprintf(stderr, "tcc parse: function-validate-linkage enter name=%s\n",
			        function_name);
		}
		parser_validate_function_linkage_redeclaration(existing_func,
		                                               function_name,
		                                               pfunc.file_static,
		                                               pfunc.gnu_extern_inline_definition);
		if (parser_trace_toplevel_enabled()) {
			fprintf(stderr, "tcc parse: function-validate-linkage done name=%s\n",
			        function_name);
		}
	}
	if (parser_trace_toplevel_enabled()) {
		fprintf(stderr, "tcc parse: function-validate-definition enter name=%s\n",
		        function_name);
	}
	parser_validate_function_definition_redeclaration(existing_func,
	                                                  function_name);
	if (parser_trace_toplevel_enabled()) {
		fprintf(stderr, "tcc parse: function-validate-definition done name=%s\n",
		        function_name);
	}
	if (pfunc.file_static)
		func_info->is_static = 1;
	func_info->has_definition = 1;
	if (parser_trace_toplevel_enabled()) {
		fprintf(stderr, "tcc parse: function-record-signature enter name=%s\n",
		        function_name);
	}
	parser_record_function_signature(func_info, effective_ret_type, has_prototype,
	                                 param_types, source_param_count,
	                                 func_info->is_variadic,
	                                 func_info->fixed_param_count,
	                                 saw_noreturn);
	if (parser_trace_toplevel_enabled()) {
		fprintf(stderr, "tcc parse: function-record-signature done name=%s\n",
		        function_name);
	}
	if (parser_trace_toplevel_enabled()) {
		fprintf(stderr, "tcc parse: function-reject-invalid-ret enter name=%s\n",
		        function_name);
	}
	reject_invalid_function_return_declarator();
	if (parser_trace_toplevel_enabled()) {
		fprintf(stderr, "tcc parse: function-reject-invalid-ret done name=%s\n",
		        function_name);
	}
	skip_decl_prefix_specifiers();
	if (parser_trace_toplevel_enabled()) {
		fprintf(stderr, "tcc parse: function-skip-prefix done name=%s next=%s\n",
		        function_name, token_debug_name(lexer_peek()->kind));
	}

	for (int i = 0; i < pscope.pending_struct_param_count; i++) {
		Node *copy;
		add_struct_local(pscope.pending_struct_params[i].param_name,
		                 pscope.pending_struct_params[i].struct_name);

		copy = build_struct_param_copy(pscope.pending_struct_params[i].param_name,
		                               pscope.pending_struct_params[i].hidden_name,
		                               pscope.pending_struct_params[i].struct_name);
		parser_mark_synthetic_debug_loc(copy);
		pscope.param_copy_head = append_node(pscope.param_copy_head, copy);
	}
	if (parser_trace_toplevel_enabled()) {
		fprintf(stderr, "tcc parse: function-pending-struct-copies done name=%s count=%d next=%s\n",
		        function_name, pscope.pending_struct_param_count,
		        token_debug_name(lexer_peek()->kind));
	}
	parser_profile_scope_leave(PARSER_PROF_FUNCTION_PARAMS);

	if (parser_trace_toplevel_enabled()) {
		fprintf(stderr, "tcc parse: function-body-enter expect-lbrace name=%s next=%s\n",
		        function_name, token_debug_name(lexer_peek()->kind));
	}
	expect(TOK_LBRACE);

	STRNCPY(pfunc.func_name, function_name, sizeof(pfunc.func_name) - 1);
	pfunc.func_name[sizeof(pfunc.func_name) - 1] = '\0';

	parser_profile_scope_enter(PARSER_PROF_FUNCTION_BODY);
	Node *body = parse_block_contents();
	stmt_resolve_function_gotos();


	expect(TOK_RBRACE);
	parser_profile_scope_leave(PARSER_PROF_FUNCTION_BODY);

	parser_restore_local_scope_keep_statics(&function_scope);


	if (pscope.param_copy_head) {
		Node *copy_head = pscope.param_copy_head;
		Node *tail = copy_head;
			while (tail->next)
				tail = tail->next;
			tail->next = body->body;
			body->body = copy_head;
		pscope.param_copy_head = NULL;
	}


	Node *func = new_func(function_name, body, pscope.stack_size, param_count);
	/* Override the line/filename captured by new_node() — that fires after
	 * the body is parsed, so it points at the wrong token.  Use the name
	 * token's location, which is the actual function declaration line. */
	func->line        = fn_decl_line;
	func->filename_id = fn_decl_filename_id;
	node_set_param_names(func, param_names, param_count);
	node_set_param_type_ids(func, param_type_ids, param_count);
	node_set_param_offsets(func, param_offsets, param_count);
	node_set_param_abi_sizes(func, param_abi_sizes, param_count);
	node_set_param_structs(func, param_struct_names, param_count);
	node_set_param_pointer_structs(func, param_pointer_struct_names,
	                               param_pointer_depths, param_count);
	func_info_replace_param_struct_names(func_info, param_struct_names, param_count);
	parser_attach_debug_locals(func, param_names, param_count);
	parser_free_string_array(param_names, param_count);
	parser_free_string_array(oldstyle_param_names, source_param_count);
	xfree(param_type_ids);
	xfree(param_offsets);
	xfree(param_abi_sizes);
	parser_free_string_array(param_struct_names, param_count);
	parser_free_string_array(param_pointer_struct_names, param_count);
	xfree(param_pointer_depths);
	func->is_static = pfunc.file_static;
	pfunc.file_static = 0;
	func->returns_struct = pfunc.returns_struct;
	func->aggregate_abi_class = pfunc.return_abi_class;
	func->aggregate_abi_reg_count = pfunc.return_abi_reg_count;
	func->struct_return_size = pfunc.return_size;
	func->is_pointer = pfunc.returns_pointer;
	func->elem_size = pfunc.return_elem_size;
	if (pfunc.return_type)
		func->return_type = clone_type(pfunc.return_type);
	STRNCPY(func->return_struct_name, pfunc.return_struct_name, sizeof(func->return_struct_name) - 1);
	if (!func->returns_struct && func->return_type && type_is_struct(func->return_type)) {
		int reg_count = 0;

		func->returns_struct = 1;
		func->aggregate_abi_class = parser_classify_aggregate_abi(func->return_type,
		                                                          &reg_count);
		func->aggregate_abi_reg_count = reg_count;
		func->struct_return_size = type_sizeof(func->return_type);
		if (!func->return_struct_name[0]) {
			const char *resolved_name = parser_resolve_struct_type_name(func->return_type);

			if (resolved_name && resolved_name[0])
				STRNCPY(func->return_struct_name, resolved_name,
				        sizeof(func->return_struct_name) - 1);
		}
	}

	pfunc.func_name[0] = '\0';

	return func;
}

Type *
parser_current_function_return_type(void)
{
	return pfunc.return_type;
}

static int 
looks_like_function_definition_start(void)
{
	int i = 0;
	int saw_complex_or_imaginary = 0;
	const Token *t = lexer_peek_ahead(i);

	for (;;) {
		if (t->kind == TOK_STATIC || t->kind == TOK_EXTERN ||
		    t->kind == TOK_INLINE || t->kind == TOK_NORETURN ||
		    t->kind == TOK_CONST || t->kind == TOK_VOLATILE ||
		    t->kind == TOK_RESTRICT || t->kind == TOK_ATOMIC) {
			i++;
			t = lexer_peek_ahead(i);
			continue;
		}
		if (is_gnu_attribute_token(t)) {
			int depth = 0;
			if (lexer_peek_ahead(i + 1)->kind != TOK_LPAREN ||
			    lexer_peek_ahead(i + 2)->kind != TOK_LPAREN)
				return 0;
			i += 3;
			depth = 1;
			while (depth > 0) {
				t = lexer_peek_ahead(i);
				if (t->kind == TOK_EOF)
					return 0;
				if (t->kind == TOK_LPAREN)
					depth++;
				else if (t->kind == TOK_RPAREN)
					depth--;
				i++;
			}
			if (lexer_peek_ahead(i)->kind != TOK_RPAREN)
				return 0;
			i++;
			t = lexer_peek_ahead(i);
			continue;
		}
		break;
	}

	if (!is_type_start_token(t->kind, t->text)) {
		return 0;
	}

	if (t->kind == TOK_IDENT && t->text &&
	    (STRCMP(t->text, "_Complex") == 0 ||
	     STRCMP(t->text, "_Imaginary") == 0)) {
		saw_complex_or_imaginary = 1;
		i++;
		t = lexer_peek_ahead(i);
		while (t->kind == TOK_CONST || t->kind == TOK_VOLATILE ||
		       t->kind == TOK_RESTRICT || t->kind == TOK_ATOMIC) {
			i++;
			t = lexer_peek_ahead(i);
		}
	}

	if (t->kind == TOK_SIGNED || t->kind == TOK_UNSIGNED ||
	    t->kind == TOK_SHORT || t->kind == TOK_LONG) {
		while (t->kind == TOK_SIGNED || t->kind == TOK_UNSIGNED ||
		       t->kind == TOK_SHORT || t->kind == TOK_LONG) {
			i++;
			t = lexer_peek_ahead(i);
		}
		if (t->kind == TOK_CHAR || t->kind == TOK_INT || t->kind == TOK_DOUBLE)
			i++;
	} else if (t->kind == TOK_STRUCT || t->kind == TOK_UNION || t->kind == TOK_ENUM) {
		i++;
		t = lexer_peek_ahead(i);
		if (t->kind == TOK_IDENT)
			i++;
	} else {
		if (saw_complex_or_imaginary &&
		    t->kind != TOK_FLOAT &&
		    t->kind != TOK_DOUBLE)
			return 0;
		i++;
	}

	t = lexer_peek_ahead(i);
	while (t->kind == TOK_STATIC || t->kind == TOK_EXTERN ||
	       t->kind == TOK_INLINE || t->kind == TOK_NORETURN) {
		i++;
		t = lexer_peek_ahead(i);
	}
	while (token_starts_alignas_specifier(t, lexer_peek_ahead(i + 1)) &&
	       (STRCMP(t->text, "_Alignas") == 0 ||
	        (tcc_lang_at_least(LANG_C23) &&
	         STRCMP(t->text, "alignas") == 0))) {
		int depth = 0;

		i++;
		if (lexer_peek_ahead(i)->kind != TOK_LPAREN)
			return 0;
		i++;
		depth = 1;
		while (depth > 0) {
			t = lexer_peek_ahead(i);
			if (t->kind == TOK_EOF)
				return 0;
			if (t->kind == TOK_LPAREN)
				depth++;
			else if (t->kind == TOK_RPAREN)
				depth--;
			i++;
		}
		t = lexer_peek_ahead(i);
	}
	for (;;) {
		if (!is_gnu_attribute_token(t))
			break;
		if (lexer_peek_ahead(i + 1)->kind != TOK_LPAREN ||
		    lexer_peek_ahead(i + 2)->kind != TOK_LPAREN)
			return 0;
		i += 3;
		int depth = 1;
		while (depth > 0) {
			t = lexer_peek_ahead(i);
			if (t->kind == TOK_EOF) {
				if (getenv("TCC_TRACE_LOOKS_FUNC"))
					fprintf(stderr, "LOOK eof in params\n");
				return 0;
			}
			if (t->kind == TOK_LPAREN)
				depth++;
			else if (t->kind == TOK_RPAREN)
				depth--;
			i++;
		}
		if (lexer_peek_ahead(i)->kind != TOK_RPAREN)
			return 0;
		i++;
		t = lexer_peek_ahead(i);
	}
	while (t->kind == TOK_CONST || t->kind == TOK_VOLATILE ||
	       t->kind == TOK_RESTRICT || t->kind == TOK_ATOMIC) {
		i++;
		t = lexer_peek_ahead(i);
	}

	while (t->kind == TOK_STAR) {
		i++;
		t = lexer_peek_ahead(i);
		while (t->kind == TOK_CONST || t->kind == TOK_VOLATILE ||
		       t->kind == TOK_RESTRICT || t->kind == TOK_ATOMIC) {
			i++;
			t = lexer_peek_ahead(i);
		}
	}

	if (t->kind == TOK_LPAREN &&
	    lexer_peek_ahead(i + 1)->kind == TOK_STAR &&
	    lexer_peek_ahead(i + 2)->kind == TOK_IDENT &&
	    lexer_peek_ahead(i + 3)->kind == TOK_LPAREN) {
		int j = i + 4;
		int depth = 1;
		const Token *u;

		while (depth > 0) {
			u = lexer_peek_ahead(j);
			if (u->kind == TOK_EOF)
				return 0;
			if (u->kind == TOK_LPAREN)
				depth++;
			else if (u->kind == TOK_RPAREN)
				depth--;
			j++;
		}

		u = lexer_peek_ahead(j);
		if (u->kind != TOK_RPAREN)
			return 1;
		j++;
		u = lexer_peek_ahead(j);

		if (u->kind == TOK_LPAREN) {
			depth = 1;
			j++;
			while (depth > 0) {
				u = lexer_peek_ahead(j);
				if (u->kind == TOK_EOF)
					return 0;
				if (u->kind == TOK_LPAREN)
					depth++;
				else if (u->kind == TOK_RPAREN)
					depth--;
				j++;
			}
			u = lexer_peek_ahead(j);
			return u->kind == TOK_LBRACE;
		}

		if (u->kind == TOK_LBRACKET) {
			int bdepth = 1;
			j++;
			while (bdepth > 0) {
				u = lexer_peek_ahead(j);
				if (u->kind == TOK_EOF)
					return 0;
				if (u->kind == TOK_LBRACKET)
					bdepth++;
				else if (u->kind == TOK_RBRACKET)
					bdepth--;
				j++;
			}
			u = lexer_peek_ahead(j);
			return u->kind == TOK_LBRACE;
		}

		return 0;
	}

	if (t->kind == TOK_IDENT && lexer_peek_ahead(i + 1)->kind == TOK_LPAREN) {
		return !lexer_is_function_prototype();
	}

	return 0;
}


int try_parse_null_pointer_constant(void)
{
    const Token *t = lexer_peek();

    if (tcc_lang_at_least(LANG_C23) && token_is_c23_nullptr_keyword(t)) {
        lexer_next();
        return 1;
    }

    if (t->kind == TOK_NUM &&
        !t->num_is_fp &&
        !token_spelling_looks_floating(t) &&
        t->long_value == 0) {
        lexer_next();
        return 1;
    }

    if (t->kind == TOK_IDENT && t->text && STRCMP(t->text, "NULL") == 0) {
        lexer_next();
        return 1;
    }

    if (t->kind == TOK_LPAREN &&
        lexer_peek_ahead(1)->kind == TOK_VOID &&
        lexer_peek_ahead(2)->kind == TOK_STAR &&
        lexer_peek_ahead(3)->kind == TOK_RPAREN &&
        lexer_peek_ahead(4)->kind == TOK_NUM &&
        !lexer_peek_ahead(4)->num_is_fp &&
        !token_spelling_looks_floating(lexer_peek_ahead(4)) &&
        lexer_peek_ahead(4)->long_value == 0) {
        lexer_next();
        lexer_next();
        lexer_next();
        lexer_next();
        lexer_next();
        return 1;
    }

    if (t->kind == TOK_LPAREN &&
        lexer_peek_ahead(1)->kind == TOK_LPAREN &&
        lexer_peek_ahead(2)->kind == TOK_VOID &&
        lexer_peek_ahead(3)->kind == TOK_STAR &&
        lexer_peek_ahead(4)->kind == TOK_RPAREN &&
        lexer_peek_ahead(5)->kind == TOK_NUM &&
        !lexer_peek_ahead(5)->num_is_fp &&
        !token_spelling_looks_floating(lexer_peek_ahead(5)) &&
        lexer_peek_ahead(5)->long_value == 0) {
        lexer_next();
        lexer_next();
        lexer_next();
        lexer_next();
        lexer_next();
        lexer_next();
        expect(TOK_RPAREN);
        return 1;
    }

    return 0;
}

typedef struct ParserSavedGlobalAddrTarget {
	char name[64];
	char struct_name[64];
	Type *type;
	int elem_size;
	int ptr_elem_size;
} ParserSavedGlobalAddrTarget;

static void parser_save_global_addr_target(ParserSavedGlobalAddrTarget *saved, const Global *g);
static void parser_restore_global_addr_target(Global **pg, const ParserSavedGlobalAddrTarget *saved,
                                              const char *lit_name);

static Type *
type_array_root_scalar_element(Type *type)
{
	Type *elem = type;

	while (elem && type_is_array(elem))
		elem = type_pointee(elem);
	return elem;
}

static void
parse_global_array_compound_literal_postfix(Type *array_type, Type **out_selected_type,
                                            int *out_addr_offset)
{
	Type *current = array_type;
	int addr_offset = 0;

	while (lexer_peek()->kind == TOK_LBRACKET) {
		Type *elem_type;
		int stride;
		long long idx;

		if (!current || !type_is_array(current))
			fatal_cur("subscript on non-array global compound literal is not supported\n");
		elem_type = type_pointee(current);
		stride = elem_type ? type_sizeof(elem_type) : 0;
		if (stride <= 0)
			fatal_cur("Invalid subscript target in global compound literal\n");

		lexer_next();
		idx = eval_const_array_size();
		expect(TOK_RBRACKET);

		addr_offset += (int)(idx * stride);
		current = elem_type;
	}

	if (out_selected_type)
		*out_selected_type = current;
	if (out_addr_offset)
		*out_addr_offset = addr_offset;
}

static void
parse_global_struct_compound_literal_postfix(Type *aggregate_type, Type **out_selected_type,
                                             int *out_addr_offset)
{
	Type *current = aggregate_type;
	int addr_offset = 0;

	while (lexer_peek()->kind == TOK_DOT) {
		const Token *field_tok;
		Field *field;
		Type *field_type;

		if (!current || (!type_is_struct(current) && !type_is_union(current)) ||
		    !current->struct_name[0]) {
			fatal_cur("member access on non-aggregate global compound literal is not supported\n");
		}

		lexer_next();
		field_tok = lexer_peek();
		if (field_tok->kind != TOK_IDENT)
			fatal_cur("Expected field name after global compound literal '.'\n");
		lexer_next();

		field = find_field(current->struct_name, field_tok->text);
		addr_offset += field->offset;
		field_type = field->type;

		if (!field_type && field->struct_name[0]) {
			StructDef *field_def = find_struct_or_null(field->struct_name);
			if (field_def && field_def->is_union)
				field_type = type_union(field->struct_name, field->size);
			else
				field_type = type_struct(field->struct_name, field->size);
		}
		if (!field_type)
			field_type = type_for_size(field->size);
		current = field_type;
	}

	if (out_selected_type)
		*out_selected_type = current;
	if (out_addr_offset)
		*out_addr_offset = addr_offset;
}

static int
parser_global_array_compound_literal_matches(void)
{
	int lp_idx = 0;
	int scan_idx;
	int paren_depth;
	int saw_array = 0;
	const Token *type_tok;
	Type *typedef_type;
	Type *effective_type;

	if (lexer_peek()->kind == TOK_AMP) {
		lp_idx = 1;
		if (lexer_peek_ahead(lp_idx)->kind == TOK_LPAREN &&
		    lexer_peek_ahead(lp_idx + 1)->kind == TOK_LPAREN &&
		    is_type_start_token(lexer_peek_ahead(lp_idx + 2)->kind,
		                        lexer_peek_ahead(lp_idx + 2)->text))
			lp_idx++;
	} else if (lexer_peek()->kind == TOK_LPAREN &&
	           lexer_peek_ahead(1)->kind == TOK_LPAREN &&
	           is_type_start_token(lexer_peek_ahead(2)->kind,
	                               lexer_peek_ahead(2)->text)) {
		lp_idx = 1;
	}

	if (lexer_peek_ahead(lp_idx)->kind != TOK_LPAREN)
		return 0;
	if (!is_type_start_token(lexer_peek_ahead(lp_idx + 1)->kind,
	                         lexer_peek_ahead(lp_idx + 1)->text))
		return 0;
	type_tok = lexer_peek_ahead(lp_idx + 1);
	if (type_tok->kind == TOK_IDENT && type_tok->text &&
	    parser_is_typedef_name(type_tok->text)) {
		typedef_type = parser_find_typedef(type_tok->text);
		effective_type = parser_canonicalize_decl_type(typedef_type);
		if (effective_type && type_is_array(effective_type))
			return 1;
	}

	paren_depth = 1;
	for (scan_idx = lp_idx + 1; paren_depth > 0; scan_idx++) {
		const Token *tok = lexer_peek_ahead(scan_idx);

		if (tok->kind == TOK_EOF)
			return 0;
		if (tok->kind == TOK_LPAREN)
			paren_depth++;
		else if (tok->kind == TOK_RPAREN)
			paren_depth--;
		else if (tok->kind == TOK_LBRACKET && paren_depth == 1)
			saw_array = 1;
	}

	return saw_array && lexer_peek_ahead(scan_idx)->kind == TOK_LBRACE;
}

static int
parser_global_array_compound_literal_subscript_matches(void)
{
	int lp_idx = 0;
	int scan_idx;
	int paren_depth;
	int brace_depth;

	if (!parser_global_array_compound_literal_matches())
		return 0;

	if (lexer_peek()->kind == TOK_LPAREN &&
	    lexer_peek_ahead(1)->kind == TOK_LPAREN &&
	    is_type_start_token(lexer_peek_ahead(2)->kind,
	                        lexer_peek_ahead(2)->text)) {
		lp_idx = 1;
	}

	if (lexer_peek_ahead(lp_idx)->kind != TOK_LPAREN)
		return 0;

	paren_depth = 1;
	for (scan_idx = lp_idx + 1; paren_depth > 0; scan_idx++) {
		const Token *tok = lexer_peek_ahead(scan_idx);

		if (tok->kind == TOK_EOF)
			return 0;
		if (tok->kind == TOK_LPAREN)
			paren_depth++;
		else if (tok->kind == TOK_RPAREN)
			paren_depth--;
	}

	if (lexer_peek_ahead(scan_idx)->kind != TOK_LBRACE)
		return 0;

	brace_depth = 1;
	scan_idx++;
	for (; brace_depth > 0; scan_idx++) {
		const Token *tok = lexer_peek_ahead(scan_idx);

		if (tok->kind == TOK_EOF)
			return 0;
		if (tok->kind == TOK_LBRACE)
			brace_depth++;
		else if (tok->kind == TOK_RBRACE)
			brace_depth--;
	}

	if (lexer_peek_ahead(scan_idx)->kind == TOK_LBRACKET)
		return 1;
	if (lexer_peek_ahead(scan_idx)->kind == TOK_RPAREN &&
	    lexer_peek_ahead(scan_idx + 1)->kind == TOK_LBRACKET)
		return 1;
	return 0;
}

static unsigned long long
global_init_read_uint(const Global *g, int offset, int size)
{
	unsigned long long value = 0;

	for (int i = 0; i < size; i++) {
		value |= ((unsigned long long)(global_init_byte(g, offset + i) & 255))
		         << (8 * i);
	}
	return value;
}

static Node *
global_init_read_scalar_node(const Global *g, Type *type, int offset)
{
	int size;
	unsigned long long bits;

	if (!g || !type || !type_is_scalar(type))
		return NULL;

	size = type_sizeof(type);
	if (size <= 0)
		fatal_cur("Invalid scalar size in global compound literal selection\n");

	bits = global_init_read_uint(g, offset, size);

	if (parser_type_is_float_storage_scalar(type)) {
		char text[64];

		if (size == 4) {
			union { unsigned int u; float f; } fp32;
			fp32.u = (unsigned int)bits;
			snprintf(text, sizeof(text), "%.9g", fp32.f);
		} else if (size == 8) {
			union { unsigned long long u; double d; } fp64;
			fp64.u = bits;
			snprintf(text, sizeof(text), "%.17g", fp64.d);
		} else {
			fatal_cur("Unsupported floating scalar size in global compound literal selection\n");
		}

		return new_num_fp(type, text);
	}

	{
		long long value;
		Node *node;

		if (type->is_unsigned) {
			value = (long long)bits;
		} else {
			value = (long long)bits;
			if (size < 8) {
				unsigned long long sign_bit = 1ULL << (size * 8 - 1);
				if (bits & sign_bit)
					value |= (long long)(~0ULL << (size * 8));
			}
		}

		node = new_num_long(value);
		node->type = type;
		node->elem_size = size;
		node->is_unsigned = type->is_unsigned;
		return node;
	}
}

static Node *
parse_array_compound_literal_subscript_expr(void)
{
	char lit_name[64];
	Type *array_type = NULL;
	Type *selected_type = NULL;
	Global *lit;
	int addr_offset = 0;
	int outer_paren = 0;

	if (!parser_global_array_compound_literal_subscript_matches())
		return NULL;

	if (tcc_lang_is_c89_or_c90())
		fatal_cur("compound literals are not allowed in C89/C90 mode\n");

	if (lexer_peek()->kind == TOK_LPAREN &&
	    lexer_peek_ahead(1)->kind == TOK_LPAREN &&
	    is_type_start_token(lexer_peek_ahead(2)->kind,
	                        lexer_peek_ahead(2)->text)) {
		outer_paren = 1;
		lexer_next();
	}

	expect(TOK_LPAREN);
	parser_build_global_scalar_array_compound_literal(lit_name, &array_type);
	if (outer_paren && lexer_peek()->kind == TOK_RPAREN &&
	    lexer_peek_ahead(1)->kind == TOK_LBRACKET) {
		lexer_next();
	}
	parse_global_array_compound_literal_postfix(array_type, &selected_type, &addr_offset);
	if (!selected_type || !type_is_scalar(selected_type))
		fatal_cur("global array compound literal selection must resolve to a scalar value\n");

	lit = parser_require_global(lit_name);
	return global_init_read_scalar_node(lit, selected_type, addr_offset);
}

static void
parse_global_scalar_array_initializer_level(Global *g, int dims[MAX_ARRAY_DIMS], int dim_count,
                                           int level, int base_offset, Type *elem_type, int elem_size,
                                           int *max_used_offset)
{
	if (lexer_peek()->kind != TOK_LBRACE) {
		double float_value = 0.0;
		long long value = 0;
		if (base_offset < 0)
			fatal_cur("Invalid global array compound literal offset\n");
		if (parser_type_is_float_storage_scalar(elem_type)) {
			float_value = parse_global_floating_initializer_value_for_type_or_die(
			    "Global array compound literal initializer must contain constant scalar values\n",
			    elem_type);
			store_global_init_float(g, base_offset, elem_size, float_value);
		} else {
			value = parse_global_scalar_initializer_value_or_die(
			    "Global array compound literal initializer must contain constant scalar values\n");
			store_global_init_int(g, base_offset, elem_size, value);
		}
		if (base_offset + elem_size > *max_used_offset)
			*max_used_offset = base_offset + elem_size;
		return;
	}

	expect(TOK_LBRACE);
	reject_empty_initializer_before_c23();
	int cur_idx = 0;
	int span = (level + 1 < dim_count)
	         ? array_dim_product(dims, level + 1, dim_count) * elem_size
	         : elem_size;

	while (lexer_peek()->kind != TOK_RBRACE) {
		int first_idx = cur_idx;
		int last_idx = cur_idx;
		int has_designator = 0;

		if (lexer_peek()->kind == TOK_LBRACKET) {
			parser_try_parse_global_array_designator(&first_idx, &last_idx);
			cur_idx = first_idx;
			has_designator = 1;
		}

		if (first_idx < 0)
			fatal_cur("Negative array designator index\n");
		if (last_idx < first_idx)
			fatal_cur("Invalid global array designator range\n");
		if (dims[level] > 0 && last_idx >= dims[level]) {
			if (has_designator)
				fatal_cur("Global array designator index out of range\n");
			fatal_cur("Too many initializers for global array compound literal\n");
		}

		if (last_idx > first_idx && level + 1 == dim_count) {
			if (parser_type_is_float_storage_scalar(elem_type)) {
				double float_value = parse_global_floating_initializer_value_for_type_or_die(
				    "Global array compound literal initializer must contain constant scalar values\n",
				    elem_type);
				for (int di = first_idx; di <= last_idx; di++)
					store_global_init_float(g, base_offset + di * span, elem_size, float_value);
			} else {
				long long value = parse_global_scalar_initializer_value_or_die(
				    "Global array compound literal initializer must contain constant scalar values\n");
				for (int di = first_idx; di <= last_idx; di++)
					store_global_init_int(g, base_offset + di * span, elem_size, value);
			}
			if (base_offset + last_idx * span + elem_size > *max_used_offset)
				*max_used_offset = base_offset + last_idx * span + elem_size;
		} else if (last_idx > first_idx) {
			fatal_cur("range designator for nested global array compound literal is not supported\n");
		} else {
			int elem_base = base_offset + cur_idx * span;
			if (level + 1 < dim_count) {
				if (lexer_peek()->kind == TOK_LBRACE) {
					parse_global_scalar_array_initializer_level(g, dims, dim_count,
					                                           level + 1, elem_base, elem_type, elem_size,
					                                           max_used_offset);
				} else {
					parse_global_scalar_array_initializer_level(g, dims, dim_count,
					                                           dim_count - 1, elem_base, elem_type, elem_size,
					                                           max_used_offset);
				}
			} else {
				parse_global_scalar_array_initializer_level(g, dims, dim_count, level,
				                                           elem_base, elem_type, elem_size,
				                                           max_used_offset);
			}
		}

		cur_idx = last_idx + 1;
		if (lexer_peek()->kind == TOK_COMMA) {
			lexer_next();
			if (lexer_peek()->kind == TOK_RBRACE)
				break;
		} else {
			break;
		}
	}

	expect(TOK_RBRACE);
	if (dims[level] == 0 && level == 0)
		dims[level] = cur_idx;
}

static int
parse_global_scalar_array_initializer(Global *g, int dims[MAX_ARRAY_DIMS], int dim_count,
                                      Type *elem_type,
                                      int elem_size)
{
	int max_used_offset = 0;
	parse_global_scalar_array_initializer_level(g, dims, dim_count, 0, 0,
	                                            elem_type, elem_size,
	                                            &max_used_offset);
	return max_used_offset;
}

static void
parse_global_struct_array_initializer(Global *g, Type *array_type, StructDef *elem_def)
{
	int g_idx;
	int array_len;
	int next_index = 0;

	if (!g || !array_type || !elem_def)
		fatal_cur("Unsupported generic global array initializer\n");

	g_idx = (int)(g - punit.globals);
	array_len = type_array_len(array_type);
	globals_ensure_spare(256);
	g = &punit.globals[g_idx];
	g->is_struct = 1;
	g->is_array = 1;
	STRNCPY(g->struct_name, elem_def->name, sizeof(g->struct_name) - 1);
	g->elem_size = elem_def->size;
	global_set_init_count(g, elem_def->size);

	while (lexer_peek()->kind != TOK_RBRACE) {
		int first_index = next_index;
		int last_index = next_index;
		int has_designator = 0;

		if (lexer_peek()->kind == TOK_LBRACKET) {
			parser_try_parse_global_array_designator(&first_index, &last_index);
			has_designator = 1;
		}

		if (first_index < 0 || last_index < first_index)
			fatal_cur("Invalid global array designator range\n");
		if (array_len > 0 && last_index >= array_len)
			fatal_cur(has_designator
			          ? "Global array designator index out of range\n"
			          : "Too many initializers for global array\n");

		for (int elem_index = first_index; elem_index <= last_index; elem_index++) {
			int base_offset = elem_index * elem_def->size;
			if (try_parse_global_struct_compound_initializer(g_idx, elem_def,
			                                               elem_def->name,
			                                               base_offset)) {
				/* compound literal element */
			} else if (lexer_peek()->kind == TOK_LBRACE) {
				lexer_next();
				parse_global_struct_initializer_body(g_idx, elem_def, base_offset);
				expect(TOK_RBRACE);
			} else {
				parse_global_struct_initializer_body_ex(g_idx, elem_def, base_offset, 1);
			}
			g = &punit.globals[g_idx];
		}

		next_index = last_index + 1;
		if (next_index * elem_def->size > global_init_count(g))
			global_set_init_count(g, next_index * elem_def->size);

		if (lexer_peek()->kind == TOK_COMMA) {
			lexer_next();
			if (lexer_peek()->kind == TOK_RBRACE)
				break;
		} else if (lexer_peek()->kind == TOK_RBRACE) {
			break;
		}
	}

	if (array_len == 0)
		array_type->array_len = next_index;
	g = &punit.globals[g_idx];
	apply_type_to_global(g, array_type);
	if (global_init_count(g) < next_index * elem_def->size)
		global_set_init_count(g, next_index * elem_def->size);
}

static int
parse_global_flat_array_initializer(Global *g, Type *elem_type)
{
	int infer_array_len;
	int next_index;
	Type *array_type;
	int committed_early;

	if (!g || !elem_type)
		fatal_cur("Unsupported generic global array initializer\n");

	infer_array_len = (g->array_len == 0);
	next_index = 0;
	array_type = g->type;
	committed_early = 0;

	while (lexer_peek()->kind != TOK_RBRACE) {
		int first_index = next_index;
		int last_index = next_index;
		int has_designator = 0;
		double init_float = 0.0;
		int has_float_init = 0;
		long long init_value = 0;
		char init_sym[64] = {0};
		const Token *value;

		if (lexer_peek()->kind == TOK_LBRACKET) {
			parser_try_parse_global_array_designator(&first_index, &last_index);
			has_designator = 1;
		}

		if (first_index < 0 || last_index < first_index)
			fatal_cur("Invalid global array designator range\n");
		if (!infer_array_len && g->array_len > 0 &&
		    !has_designator && first_index >= g->array_len)
			fatal_cur("Too many initializers for global array\n");
		if (!infer_array_len && g->array_len > 0 && last_index >= g->array_len)
			fatal_cur(has_designator
			          ? "Global array designator index out of range\n"
			          : "Too many initializers for global array\n");

		if (try_parse_null_pointer_constant()) {
			init_value = 0;
		} else if (lexer_peek()->kind == TOK_AMP &&
		           lexer_peek_ahead(1)->kind == TOK_IDENT) {
			if (type_is_pointer(elem_type))
				validate_global_pointer_initializer_compatibility(elem_type, lexer_peek());
			lexer_next();
			STRNCPY(init_sym, lexer_peek()->text, sizeof(init_sym) - 1);
			lexer_next();
		} else if ((value = lexer_peek())->kind == TOK_STRING &&
		           (type_is_pointer(elem_type) || g->elem_size == TCC_SIZEOF_PTR)) {
			int g_idx = (int)(g - punit.globals);
			size_t init_len = value->text_len;
			char *init_text = xmalloc(init_len + 1);
			memcpy(init_text, value->text, init_len);
			init_text[init_len] = '\0';

			snprintf(init_sym, sizeof(init_sym), "__str_%d", parser_alloc_string_label());

			if (!committed_early && g_idx == punit.global_count) {
				parser_commit_reserved_global();
				committed_early = 1;
			}

			globals_ensure_spare(2);
			g = &punit.globals[g_idx];

			Global *sg = new_global_slot(init_sym);
			set_global_string_array_initializer_len(sg, init_text, init_len);
			sg->is_static = 1;
			sg->is_array = 1;
			sg->elem_size = 1;
			sg->array_len = (int)init_len + 1;
			global_set_init_count(sg, sg->array_len);
			punit.global_count++;
			parser_global_hash_note_new_index(punit.global_count - 1);
			parser_invalidate_global_lookup_cache();
			xfree(init_text);

			g = &punit.globals[g_idx];
			lexer_next();
		} else if (value->kind == TOK_IDENT &&
		           (is_global(value->text) || find_func(value->text))) {
			if (type_is_pointer(elem_type))
				validate_global_pointer_initializer_compatibility(elem_type, value);
			STRNCPY(init_sym, value->text, sizeof(init_sym) - 1);
			lexer_next();
		} else if (parser_type_is_float_storage_scalar(elem_type)) {
			init_float = parse_global_floating_initializer_value_for_type_or_die(
			    "Generic global array initializer must contain constant floating values\n",
			    elem_type);
			has_float_init = 1;
		} else {
			init_value = parse_global_scalar_initializer_value_or_die(
			    "Generic global array initializer must contain constant integers\n");
		}

		if (init_sym[0] && g->elem_size < TCC_SIZEOF_PTR)
			g->elem_size = TCC_SIZEOF_PTR;

		for (int di = first_index; di <= last_index; di++) {
			int elem_size = g->elem_size > 0 ? g->elem_size : 1;
			int byte_offset = di * elem_size;
			int count_needed = byte_offset + elem_size;

			if (init_sym[0]) {
				global_set_init_sym(g, di, init_sym);
			} else if (has_float_init) {
				store_global_init_float(g, byte_offset, elem_size, init_float);
				global_set_init_sym(g, di, "");
			} else if (elem_size == 1) {
				global_set_init_byte(g, di, init_value);
				global_set_init_sym(g, di, "");
			} else {
				store_global_init_int(g, byte_offset, elem_size, init_value);
				global_set_init_sym(g, di, "");
			}

			if (count_needed > global_init_count(g))
				global_set_init_count(g, count_needed);
		}

		if (infer_array_len && last_index + 1 > g->array_len)
			g->array_len = last_index + 1;
		next_index = last_index + 1;

		if (lexer_peek()->kind == TOK_COMMA) {
			lexer_next();
			if (lexer_peek()->kind == TOK_RBRACE)
				break;
		} else {
			break;
		}
	}

	if (infer_array_len && array_type && type_is_array(array_type)) {
		array_type->array_len = g->array_len;
		if (g->array_dim_count > 0)
			g->array_dims[0] = g->array_len;
		apply_type_to_global(g, array_type);
	}

	return committed_early;
}

static void
parser_build_global_scalar_array_compound_literal(char lit_name[64], Type **out_array_type)
{
	Type *base_type;
	Type *array_type;
	Type *effective_type;
	Type *elem_type;
	Global *lit;
	int lit_idx;
	int dims[MAX_ARRAY_DIMS] = {0};
	int dim_count;
	int total_len = 1;
	int elem_size;
	int init_count;

	base_type = parse_type_name();
	effective_type = parser_canonicalize_decl_type(base_type);
	if (lexer_peek()->kind == TOK_LBRACKET) {
		dim_count = parse_array_dimensions(dims, 1, 0);
		array_type = build_array_type_from_dims_allow_incomplete(clone_type(base_type),
		                                                         dims, dim_count, 1);
	} else if (effective_type && type_is_array(effective_type)) {
		Type *dim_type;

		array_type = clone_type(effective_type);
		dim_count = 0;
		for (dim_type = array_type;
		     dim_type && type_is_array(dim_type) && dim_count < MAX_ARRAY_DIMS;
		     dim_type = dim_type->base)
			dims[dim_count++] = dim_type->array_len;
		if (dim_count <= 0)
			fatal_cur("Expected array declarator in global compound literal\n");
	} else {
		fatal_cur("Expected array declarator in global compound literal\n");
	}
	elem_type = type_array_root_scalar_element(array_type);
	if (!elem_type || !type_is_scalar(elem_type))
		fatal_cur("Only scalar global array compound literals are supported for now\n");

	expect(TOK_RPAREN);

	elem_size = type_sizeof(elem_type);
	if (elem_size <= 0)
		fatal_cur("Invalid global array compound literal element size\n");

	snprintf(lit_name, 64, "__compound_global_%d", pfunc.global_compound_literal_id++);
	lit = new_global_slot(lit_name);
	lit_idx = (int)(lit - punit.globals);
	apply_type_to_global(lit, array_type);
	lit->is_array = 1;
	lit->elem_size = elem_size;
	lit->array_dim_count = dim_count;
	for (int d = 0; d < dim_count && d < MAX_ARRAY_DIMS; d++)
		lit->array_dims[d] = dims[d];

	init_count = parse_global_scalar_array_initializer(lit, dims, dim_count,
	                                                   elem_type, elem_size);
	lit = &punit.globals[lit_idx];
	for (int d = 0; d < dim_count; d++) {
		if (dims[d] == 0 && d != 0)
			fatal_cur("Only the first global array dimension may be omitted\n");
		if (dims[d] > 0)
			total_len *= dims[d];
	}
	lit->array_len = total_len;
	lit->array_dim_count = dim_count;
	for (int d = 0; d < dim_count && d < MAX_ARRAY_DIMS; d++)
		lit->array_dims[d] = dims[d];
	if (effective_type && type_is_array(effective_type))
		apply_type_to_global(lit, clone_type(array_type));
	else
		apply_type_to_global(lit, build_array_type_from_dims(clone_type(base_type), dims, dim_count));
	if (out_array_type)
		*out_array_type = clone_type(lit->type);
	global_set_init_count(lit, init_count);
	commit_global_definition(lit);
	punit.globals[lit_idx].is_static = 1;
}

static int
try_parse_global_addr_array_compound_literal(Global **pg)
{
	ParserSavedGlobalAddrTarget saved;
	char lit_name[64];
	Type *array_type = NULL;
	Type *selected_type = NULL;
	int addr_offset = 0;
	int explicit_addr = 0;
	int outer_paren = 0;

	if (!parser_global_array_compound_literal_matches())
		return 0;

	if (tcc_lang_is_c89_or_c90())
		fatal_cur("compound literals are not allowed in C89/C90 mode\n");

	parser_save_global_addr_target(&saved, *pg);

	if (lexer_peek()->kind == TOK_AMP) {
		explicit_addr = 1;
		lexer_next(); /* & */
		if (lexer_peek()->kind == TOK_LPAREN &&
		    lexer_peek_ahead(1)->kind == TOK_LPAREN &&
		    is_type_start_token(lexer_peek_ahead(2)->kind, lexer_peek_ahead(2)->text)) {
			outer_paren = 1;
			lexer_next(); /* outer ( */
		}
	}

	expect(TOK_LPAREN);
	parser_build_global_scalar_array_compound_literal(lit_name, &array_type);
	if (explicit_addr && outer_paren &&
	    lexer_peek()->kind == TOK_RPAREN &&
	    lexer_peek_ahead(1)->kind == TOK_LBRACKET) {
		lexer_next();
		outer_paren = 0;
	}
	parse_global_array_compound_literal_postfix(array_type, &selected_type, &addr_offset);
	if (explicit_addr && outer_paren)
		expect(TOK_RPAREN);
	parser_restore_global_addr_target(pg, &saved, lit_name);
	(*pg)->addr_offset = addr_offset;
	return 1;
}

static int
parser_global_addr_scalar_compound_literal_matches(void)
{
	const Token *tok;
	Type *typedef_type;
	Type *effective_type;

	if (lexer_peek()->kind != TOK_AMP)
		return 0;

	tok = NULL;
	if (lexer_peek_ahead(1)->kind == TOK_LPAREN &&
	    lexer_peek_ahead(2)->kind == TOK_LPAREN &&
	    is_type_start_token(lexer_peek_ahead(3)->kind, lexer_peek_ahead(3)->text))
		tok = lexer_peek_ahead(3);
	else if (lexer_peek_ahead(1)->kind == TOK_LPAREN &&
	         is_type_start_token(lexer_peek_ahead(2)->kind, lexer_peek_ahead(2)->text))
		tok = lexer_peek_ahead(2);

	if (!tok)
		return 0;
	if (tok->kind == TOK_STRUCT || tok->kind == TOK_UNION)
		return 0;
	if (tok->kind != TOK_IDENT || !tok->text || !parser_is_typedef_name(tok->text))
		return 1;

	typedef_type = parser_find_typedef(tok->text);
	effective_type = parser_canonicalize_decl_type(typedef_type);
	return effective_type && type_is_scalar(effective_type);
}

static int
parser_type_start_is_aggregate(const Token *tok)
{
	Type *typedef_type;
	Type *effective_type;

	if (!tok)
		return 0;
	if (tok->kind == TOK_STRUCT || tok->kind == TOK_UNION)
		return 1;
	if (tok->kind != TOK_IDENT || !tok->text || !parser_is_typedef_name(tok->text))
		return 0;

	typedef_type = parser_find_typedef(tok->text);
	effective_type = parser_canonicalize_decl_type(typedef_type);
	return effective_type &&
	       (type_is_struct(effective_type) || type_is_union(effective_type));
}

static int
try_parse_global_addr_scalar_compound_literal(Global **pg)
{
	ParserSavedGlobalAddrTarget saved;
	char lit_name[64];
	Type *lit_type;
	Global *lit;
	int lit_idx;
	int outer_paren = 0;

	if (!parser_global_addr_scalar_compound_literal_matches())
		return 0;

	if (tcc_lang_is_c89_or_c90())
		fatal_cur("compound literals are not allowed in C89/C90 mode\n");

	parser_save_global_addr_target(&saved, *pg);

	lexer_next(); /* & */
	if (lexer_peek()->kind == TOK_LPAREN &&
	    lexer_peek_ahead(1)->kind == TOK_LPAREN &&
	    is_type_start_token(lexer_peek_ahead(2)->kind, lexer_peek_ahead(2)->text)) {
		outer_paren = 1;
		lexer_next(); /* outer ( */
	}

	expect(TOK_LPAREN);
	lit_type = parse_type_name();
	parser_reject_unsupported_special_type(lit_type);
	if (!type_is_scalar(lit_type) && !type_is_complex(lit_type))
		fatal_cur("Only scalar global compound literals are supported here\n");
	if (lexer_peek()->kind == TOK_LBRACKET)
		fatal_cur("Expected scalar type in global compound literal\n");
	expect(TOK_RPAREN);
	if (lexer_peek()->kind != TOK_LBRACE)
		fatal_cur("Expected initializer in global compound literal\n");

	snprintf(lit_name, sizeof(lit_name), "__compound_global_%d",
	         pfunc.global_compound_literal_id++);
	lit = new_global_slot(lit_name);
	lit_idx = (int)(lit - punit.globals);
	apply_type_to_global(lit, lit_type);
	parse_scalar_global_initializer(lit,
		"Global scalar compound literal initializer must be a constant scalar value\n");
	commit_global_definition(lit);
	punit.globals[lit_idx].is_static = 1;

	if (outer_paren)
		expect(TOK_RPAREN);

	parser_restore_global_addr_target(pg, &saved, lit_name);
	(*pg)->addr_offset = 0;
	return 1;
}

static int
parser_global_addr_struct_compound_literal_matches(void)
{
	const Token *tok;

	if (lexer_peek()->kind != TOK_AMP)
		return 0;

	tok = NULL;
	if (lexer_peek_ahead(1)->kind == TOK_LPAREN &&
	    lexer_peek_ahead(2)->kind == TOK_LPAREN &&
	    is_type_start_token(lexer_peek_ahead(3)->kind,
	                        lexer_peek_ahead(3)->text))
		tok = lexer_peek_ahead(3);
	else if (lexer_peek_ahead(1)->kind == TOK_LPAREN &&
	         is_type_start_token(lexer_peek_ahead(2)->kind,
	                             lexer_peek_ahead(2)->text))
		tok = lexer_peek_ahead(2);

	return parser_type_start_is_aggregate(tok);
}

static void
parser_save_global_addr_target(ParserSavedGlobalAddrTarget *saved, const Global *g)
{
	memset(saved, 0, sizeof(*saved));
	saved->type = g->type;
	saved->elem_size = g->elem_size;
	saved->ptr_elem_size = g->ptr_elem_size;
	STRNCPY(saved->name, g->name, sizeof(saved->name) - 1);
	STRNCPY(saved->struct_name, g->struct_name, sizeof(saved->struct_name) - 1);
}

static void
parser_parse_global_aggregate_compound_type(char struct_name_buf[64], StructDef **out_def)
{
	Type *compound_type;
	Type *effective_type;
	const char *resolved_name;
	StructDef *def;

	expect(TOK_LPAREN);
	compound_type = parse_type_name();
	parser_reject_unsupported_special_type(compound_type);
	effective_type = parser_canonicalize_decl_type(compound_type);
	if (!effective_type ||
	    (!type_is_struct(effective_type) && !type_is_union(effective_type)))
		fatal_cur("Expected struct or union name in global compound literal\n");

	resolved_name = parser_resolve_struct_type_name(effective_type);
	if (!resolved_name || !resolved_name[0])
		fatal_cur("Expected struct or union name in global compound literal\n");

	def = find_struct(resolved_name);
	if (!def)
		fatal_cur("Expected struct or union name in global compound literal\n");

	STRNCPY(struct_name_buf, resolved_name, 63);
	if (out_def)
		*out_def = def;

	expect(TOK_RPAREN);
	expect(TOK_LBRACE);
}

static void
parser_build_global_struct_compound_literal(const char *struct_name_buf, char lit_name[64])
{
	StructDef *def = find_struct(struct_name_buf);
	Global *lit;
	int lit_idx;

	if (def->has_flexible_array_member && tcc_iso_diagnostics)
		fatal_cur("initializer for %s with flexible array member is not supported\n",
		          def->is_union ? "union" : "struct");

	snprintf(lit_name, 64, "__compound_global_%d", pfunc.global_compound_literal_id++);

	lit = new_global_slot(lit_name);
	lit_idx = (int)(lit - punit.globals);
	lit->array_len = 1;
	lit->elem_size = def->size;
	lit->is_struct = 1;
	global_set_init_count(lit, def->size);
	STRNCPY(lit->struct_name, struct_name_buf, sizeof(lit->struct_name) - 1);

	parse_global_struct_initializer_body(lit_idx, def, 0);
	lit = &punit.globals[lit_idx];
	expect(TOK_RBRACE);
	commit_global_definition(lit);
	punit.globals[lit_idx].is_static = 1;
}

static void
parser_restore_global_addr_target(Global **pg, const ParserSavedGlobalAddrTarget *saved,
                                  const char *lit_name)
{
	Global *ptr_g = new_global_slot(saved->name);

	STRNCPY(ptr_g->struct_name, saved->struct_name, sizeof(ptr_g->struct_name) - 1);
	if (saved->type)
		apply_type_to_global(ptr_g, clone_type(saved->type));
	else {
		ptr_g->elem_size = saved->elem_size;
		ptr_g->ptr_elem_size = saved->ptr_elem_size;
	}
	set_global_address_initializer(ptr_g, lit_name);
	*pg = ptr_g;
}

static int
try_parse_global_addr_struct_compound_literal(Global **pg)
{
	ParserSavedGlobalAddrTarget saved;
	char struct_name_buf[64];
	char lit_name[64];
	Type *aggregate_type;
	Type *selected_type = NULL;
	StructDef *def;
	int addr_offset = 0;
	int outer_paren = 0;

	/*
	 * A file-scope compound literal has static storage duration.  Lower it to an
	 * anonymous static aggregate global, then initialize the pointer with that
	 * symbol's address.
	 */
	if (!parser_global_addr_struct_compound_literal_matches())
		return 0;

	if (tcc_lang_is_c89_or_c90())
		fatal_cur("compound literals are not allowed in C89/C90 mode\n");

	parser_save_global_addr_target(&saved, *pg);
	def = NULL;

	lexer_next(); /* & */
	if (lexer_peek()->kind == TOK_LPAREN &&
	    lexer_peek_ahead(1)->kind == TOK_LPAREN &&
	    parser_type_start_is_aggregate(lexer_peek_ahead(2))) {
		outer_paren = 1;
		lexer_next(); /* outer ( */
	}
	parser_parse_global_aggregate_compound_type(struct_name_buf, &def);
	aggregate_type = def->is_union ? type_union(struct_name_buf, def->size)
	                               : type_struct(struct_name_buf, def->size);
	parser_build_global_struct_compound_literal(struct_name_buf, lit_name);
	if (lexer_peek()->kind == TOK_RPAREN && lexer_peek_ahead(1)->kind == TOK_DOT) {
		lexer_next();
		outer_paren = 0;
	}
	parse_global_struct_compound_literal_postfix(aggregate_type, &selected_type, &addr_offset);
	if (outer_paren)
		expect(TOK_RPAREN);
	parser_restore_global_addr_target(pg, &saved, lit_name);
	(*pg)->addr_offset = addr_offset;
	return 1;
}


int
try_parse_global_struct_compound_initializer(int g_idx, StructDef *def, const char *expected_struct_name, int base_offset)
{
	int outer_paren = 0;

	if (lexer_peek()->kind != TOK_LPAREN)
		return 0;

	if (lexer_peek_ahead(1)->kind == TOK_LPAREN &&
	    parser_type_start_is_aggregate(lexer_peek_ahead(2))) {
		outer_paren = 1;
		if (tcc_lang_is_c89_or_c90())
			fatal_cur("compound literals are not allowed in C89/C90 mode\n");
		lexer_next();
	} else if (!parser_type_start_is_aggregate(lexer_peek_ahead(1))) {
		return 0;
	} else if (tcc_lang_is_c89_or_c90()) {
		fatal_cur("compound literals are not allowed in C89/C90 mode\n");
	}

	{
		char resolved_struct_name[64] = {0};
		StructDef *compound_def = NULL;

		parser_parse_global_aggregate_compound_type(resolved_struct_name, &compound_def);
		if (STRCMP(resolved_struct_name, expected_struct_name) != 0 ||
		    compound_def != def)
			fatal_cur("Global compound literal aggregate type mismatch\n");
	}

	/*
	 * The same helper is used for both top-level punit.globals and nested
	 * struct fields.  For nested fields, preserve the outer object's
	 * required initializer size instead of shrinking it to the nested
	 * struct's size.
	 */
	Global *g = &punit.globals[g_idx];
	if (global_init_count(g) < base_offset + def->size)
		global_set_init_count(g, base_offset + def->size);
	parse_global_struct_initializer_body(g_idx, def, base_offset);
	g = &punit.globals[g_idx]; /* re-derive after potential realloc in body */

	expect(TOK_RBRACE);
	if (outer_paren)
		expect(TOK_RPAREN);

	return 1;
}

static void 
parse_generic_global_declaration(void)
{
	int requested_align = parse_alignment_specifiers();
	int saved_file_static = pfunc.file_static;

	Type *type = parse_type_name();
	parser_reject_unsupported_special_type(type);
	int saw_trailing_function_specifier =
	    parser_type_name_saw_trailing_function_specifier();
	TokenKind trailing_storage_class =
	    parser_type_name_trailing_storage_class();
	int saw_thread_local =
	    parser_type_name_saw_thread_local_storage_specifier();

	if (parser_type_name_saw_multiple_trailing_storage_classes())
		fatal_cur("multiple storage classes in declaration\n");

	if (token_starts_alignas_specifier(lexer_peek(), lexer_peek_ahead(1))) {
		int post_align = parse_alignment_specifiers();
		if (post_align > requested_align)
			requested_align = post_align;
	}

	if (trailing_storage_class == TOK_TYPEDEF) {
		if (saw_thread_local)
			fatal_cur("multiple storage classes in declaration\n");
		if (saw_trailing_function_specifier)
			fatal_cur("function specifier is only valid on function declarations\n");
		parse_typedef_declaration_after_base_type(type);
		return;
	}
	if (saw_thread_local)
		pfunc.file_thread_local = 1;
	if (trailing_storage_class == TOK_AUTO)
		fatal_cur("auto storage class is not allowed at file scope\n");
	if (trailing_storage_class == TOK_REGISTER)
		fatal_cur("register storage class is not allowed at file scope\n");
	int is_extern_decl = (trailing_storage_class == TOK_EXTERN);
	int decl_file_static = saved_file_static ||
	                       (trailing_storage_class == TOK_STATIC);

	/* Forward declaration or bare struct/union type: "struct T;" */
	if (lexer_peek()->kind == TOK_SEMI) {
		if (saw_trailing_function_specifier)
			fatal_cur("function specifier is only valid on function declarations\n");
		if (trailing_storage_class != TOK_EOF)
			fatal_cur("Expected global declarator name\n");
		lexer_next();
		(void)type;
		return;
	}

	/* Consume pointer stars: "enum E *e;" "const int *p;" */
	while (lexer_peek()->kind == TOK_STAR) {
		lexer_next();
		type = type_ptr(type);
	}

	const Token *name = lexer_peek();

	/* Global function pointer or function-returning-pointer:
	 *   "int (*fptr)() = 0;"              -- global fptr variable
	 *   "int (* f1(int a))(int c) { }"   -- function returning fptr */
	if (name->kind == TOK_LPAREN && lexer_peek_ahead(1)->kind == TOK_STAR) {
		int ptr_dims[MAX_ARRAY_DIMS] = {0};
		int ptr_dim_count = 0;
		int function_returning_pointer_decl = 0;
		int proto_is_variadic = 0;
		int proto_fixed_params = 0;
		int had_initializer = 0;
		Type *decl_type;
		Global *g = NULL;

		lexer_next(); /* ( */
		lexer_next(); /* * */
		if (lexer_peek()->kind == TOK_LPAREN &&
		    lexer_peek_ahead(1)->kind == TOK_STAR) {
			char nested_decl_name[64] = {0};

			decl_type = parse_nested_function_pointer_object_declarator_type(type,
			                                                                nested_decl_name);
			if (!is_extern_decl) {
				g = new_global_slot(nested_decl_name);
				apply_type_to_global(g, decl_type);
				parser_apply_global_decl_alignment(g, decl_type, requested_align);
			}

			if (lexer_peek()->kind == TOK_ASSIGN) {
				if (saw_trailing_function_specifier)
					fatal_cur("function specifier is only valid on function declarations\n");
				lexer_next();
				had_initializer = 1;
				if (is_extern_decl) {
					parser_define_extern_object_declaration(
					    nested_decl_name, decl_type,
					    "Function pointer initializer must be a constant or function name\n");
				} else {
					parse_symbol_address_global_initializer(g,
					    "Function pointer initializer must be a constant or function name\n",
					    0, 0);
				}
			}

			expect(TOK_SEMI);
			if (is_extern_decl) {
				if (!had_initializer)
					parser_register_extern_object_declaration(nested_decl_name, decl_type);
			} else {
				pfunc.file_static = decl_file_static;
				commit_global_definition(g);
				pfunc.file_static = saved_file_static;
			}
			return;
		}
		const Token *fp_name =
		    parser_parenthesized_pointer_declarator_name_token("function pointer name");
		char fp_decl_name[64] = {0};
		STRNCPY(fp_decl_name, fp_name->text, sizeof(fp_decl_name) - 1);
		lexer_next(); /* name */

		if (lexer_peek()->kind == TOK_LBRACKET) {
			if (saw_trailing_function_specifier)
				fatal_cur("function specifier is only valid on function declarations\n");
			ptr_dim_count = parser_parse_file_scope_parenthesized_array_dims(ptr_dims);
		}

		if (lexer_peek()->kind == TOK_LPAREN) {
			function_returning_pointer_decl = 1;
			parse_prototype_param_metadata(&proto_is_variadic, &proto_fixed_params);
		}

		expect(TOK_RPAREN);

		if (function_returning_pointer_decl) {
			Type *ret_type =
			    parser_parse_generic_function_returning_pointer_target_type(type);
			if (lexer_peek()->kind == TOK_SEMI) {
				lexer_next();
				parser_register_pointer_returning_function_declaration(
				    fp_decl_name, ret_type, proto_is_variadic, proto_fixed_params);
				return;
			}
			fatal_cur("function-returning-pointer declarations are only supported as prototypes in generic globals\n");
		}

		decl_type = parser_finish_file_scope_parenthesized_pointer_object_type(
		    type, ptr_dims, ptr_dim_count);

		if (!is_extern_decl) {
			g = new_global_slot(fp_decl_name);
			apply_type_to_global(g, decl_type);
			parser_apply_global_decl_alignment(g, decl_type, requested_align);
		}

		if (lexer_peek()->kind == TOK_ASSIGN) {
			lexer_next();
			had_initializer = 1;
			if (is_extern_decl) {
				parser_define_extern_object_declaration(
				    fp_decl_name, decl_type,
				    "Unsupported extern global initializer\n");
			} else {
				parser_handle_parenthesized_generic_global_initializer(&g, decl_type);
			}
		}

		expect(TOK_SEMI);
		if (is_extern_decl && !had_initializer)
			parser_register_extern_object_declaration(fp_decl_name, decl_type);
		else if (!is_extern_decl) {
			pfunc.file_static = decl_file_static;
			commit_global_definition(g);
			pfunc.file_static = saved_file_static;
		}
		return;
	}

	parser_require_decl_identifier(name, "global declarator name");
	char global_decl_name[64] = {0};
	STRNCPY(global_decl_name, name->text ? name->text : "", sizeof(global_decl_name) - 1);

	lexer_next();
	if (saw_trailing_function_specifier && lexer_peek()->kind != TOK_LPAREN)
		fatal_cur("function specifier is only valid on function declarations\n");

	Global *g = NULL;
	int g_committed_early = 0;
	int had_initializer = 0;
	if (!is_extern_decl) {
		g = new_global_slot(global_decl_name);
		apply_type_to_global(g, type);
		parser_apply_global_decl_alignment(g, type, requested_align);
	}

	if (lexer_peek()->kind == TOK_LPAREN) {
		if (requested_align > 0)
			fatal_cur("alignment specifier cannot be applied to a function declaration\n");
		ParsedFileScopeDeclarator decl = {0};

		decl.is_function = 1;
		STRNCPY(decl.name, global_decl_name, sizeof(decl.name) - 1);
		decl.type = type;
		parse_prototype_param_list(&decl.param_types, &decl.param_count,
		                          &decl.is_variadic, &decl.fixed_params,
		                          &decl.has_prototype, 1);
		reject_invalid_function_return_declarator();
		pfunc.file_static = saved_file_static ||
		                    (trailing_storage_class == TOK_STATIC);
		parser_handle_file_scope_function_declarator(
		    &decl, saw_trailing_function_specifier, 0,
		    trailing_storage_class == TOK_STATIC);
		pfunc.file_static = saved_file_static;

		while (lexer_peek()->kind == TOK_COMMA) {
			lexer_next();
			parse_file_scope_declarator(type, &decl, "global declarator name");
			if (decl.is_function) {
				pfunc.file_static = saved_file_static ||
				                    (trailing_storage_class == TOK_STATIC);
				parser_handle_file_scope_function_declarator(
				    &decl, saw_trailing_function_specifier, 0,
				    trailing_storage_class == TOK_STATIC);
				pfunc.file_static = saved_file_static;
				continue;
			}
			if (is_extern_decl)
				parser_handle_extern_object_declarator(
				    decl.name, decl.type, "Unsupported extern global initializer\n");
			else
				parser_handle_generic_object_declarator(
				    decl.name, decl.type, "Unsupported generic global initializer\n");
		}
		expect(TOK_SEMI);
		return;
	}

	reject_void_object_type(type, "global");

	if (lexer_peek()->kind == TOK_LBRACKET) {
		int dims[MAX_ARRAY_DIMS] = {0};
		int dim_count;
		Type *array_type;
		Type *elem_type;
		Type *base_elem_type;

		if (lexer_peek_ahead(1)->kind == TOK_STATIC)
			fatal_cur("array static bounds are only allowed in function parameter declarators\n");
		if (lexer_peek_ahead(1)->kind == TOK_CONST ||
		    lexer_peek_ahead(1)->kind == TOK_VOLATILE ||
		    lexer_peek_ahead(1)->kind == TOK_RESTRICT ||
		    lexer_peek_ahead(1)->kind == TOK_ATOMIC ||
		    (lexer_peek_ahead(1)->kind == TOK_IDENT &&
		     lexer_peek_ahead(1)->text &&
		     (STRCMP(lexer_peek_ahead(1)->text, "volatile") == 0 ||
		      STRCMP(lexer_peek_ahead(1)->text, "restrict") == 0)))
			fatal_cur("array type qualifiers are only allowed in function parameter declarators\n");
		if (parser_array_bound_contains_nonconstant_identifier())
			fatal_cur("file-scope array bound must be an integer constant expression\n");

		dim_count = parse_array_dimensions(dims, 1, 0);
		if (lexer_peek()->kind == TOK_LPAREN)
			fatal_cur("array elements cannot have function type\n");
		array_type = build_array_type_from_dims_allow_incomplete(clone_type(type),
		                                                       dims, dim_count, 1);
		reject_flexible_array_member_array_object_type(
		    array_type, is_extern_decl ? "extern global" : "global");
		elem_type = type_pointee(array_type);
		base_elem_type = elem_type;
		while (base_elem_type && type_is_array(base_elem_type))
			base_elem_type = type_pointee(base_elem_type);
		if (!is_extern_decl) {
			apply_type_to_global(g, array_type);
			g->array_dim_count = dim_count;
			for (int d = 0; d < dim_count && d < MAX_ARRAY_DIMS; d++)
				g->array_dims[d] = dims[d];
		}

		if (lexer_peek()->kind == TOK_ASSIGN) {
			lexer_next();
			if (is_extern_decl) {
				parser_define_extern_object_declaration(
				    global_decl_name, array_type,
				    "Unsupported extern global initializer\n");
				expect(TOK_SEMI);
				return;
			}
			mark_global_initialized(g);

			if (lexer_peek()->kind == TOK_LBRACE) {
				if (base_elem_type &&
				    (type_is_struct(base_elem_type) || type_is_union(base_elem_type)) &&
				    base_elem_type->struct_name[0]) {
					StructDef *elem_def = find_struct_or_null(base_elem_type->struct_name);
					if (elem_def) {
						int g_idx = (int)(g - punit.globals);
						lexer_next();
						reject_empty_initializer_before_c23();
						parse_global_struct_array_initializer(g, array_type, elem_def);
						g = &punit.globals[g_idx];
						expect(TOK_RBRACE);
					}
				} else if (base_elem_type &&
				           !type_is_pointer(base_elem_type) &&
				           !parser_type_is_floating_scalar(base_elem_type)) {
						int init_count = parse_global_scalar_array_initializer(g, dims, dim_count,
					                                                     base_elem_type,
					                                                     type_sizeof(base_elem_type));
						if (g->array_len == 0) {
							array_type->array_len = dims[0];
							g->array_dims[0] = dims[0];
						}
						apply_type_to_global(g, array_type);
						if (init_count > global_init_count(g))
							global_set_init_count(g, init_count);
				} else if (dim_count == 1) {
					int committed_flat_array_early = 0;
					lexer_next();
					reject_empty_initializer_before_c23();
					committed_flat_array_early =
					    parse_global_flat_array_initializer(g, base_elem_type ? base_elem_type : elem_type);
					if (committed_flat_array_early)
						g_committed_early = 1;
					expect(TOK_RBRACE);
				} else {
					lexer_next();
					reject_empty_initializer_before_c23();
					expect(TOK_RBRACE);
				}
			} else if (lexer_peek()->kind == TOK_STRING && g->is_array) {
				parse_string_array_global_initializer(g,
				    "Unsupported generic global array initializer\n");
			} else {
				fatal_cur("Unsupported generic global array initializer\n");
			}
		}

		if (is_extern_decl) {
			parser_register_extern_object_declaration(global_decl_name, array_type);
		} else if (!g_committed_early) {
			pfunc.file_static = decl_file_static;
			commit_global_definition(g);
			pfunc.file_static = saved_file_static;
		}

		while (lexer_peek()->kind == TOK_COMMA) {
			ParsedFileScopeDeclarator decl = {0};

			lexer_next();
			parse_file_scope_declarator(type, &decl, "global declarator name");
			if (decl.is_function)
				fatal_cur("function declaration is not valid in array declarator list\n");
			if (is_extern_decl)
				parser_handle_extern_object_declarator(
				    decl.name, decl.type, "Unsupported extern global initializer\n");
			else {
				pfunc.file_static = decl_file_static;
				parser_handle_generic_object_declarator(
				    decl.name, decl.type, "Unsupported generic global initializer\n");
				pfunc.file_static = saved_file_static;
			}
		}

		expect(TOK_SEMI);
		return;
	}

	if (lexer_peek()->kind == TOK_ASSIGN) {
		lexer_next();
		had_initializer = 1;
		if (is_extern_decl)
			parser_define_extern_object_declaration(
			    global_decl_name, type,
			    "Unsupported extern global initializer\n");
		else
			parse_generic_global_initializer(&g, type,
			                               "Unsupported generic global initializer\n");
	}

	if (is_extern_decl) {
		if (!had_initializer)
			parser_register_extern_object_declaration(global_decl_name, type);
	} else if (!g_committed_early) {
		pfunc.file_static = decl_file_static;
		commit_global_definition(g);
		pfunc.file_static = saved_file_static;
	}

	/* Comma-separated global declarations, including mixed object/function lists. */
	while (lexer_peek()->kind == TOK_COMMA) {
		ParsedFileScopeDeclarator decl = {0};

		lexer_next();
		parse_file_scope_declarator(type, &decl, "global declarator name");
		if (decl.is_function) {
			if (requested_align > 0)
				fatal_cur("alignment specifier cannot be applied to a function declaration\n");
			parser_handle_file_scope_function_declarator(
			    &decl, saw_trailing_function_specifier, 0,
			    saved_file_static || (trailing_storage_class == TOK_STATIC));
			continue;
		}
		if (is_extern_decl)
			parser_handle_extern_object_declarator(
			    decl.name, decl.type, "Unsupported extern global initializer\n");
		else {
			pfunc.file_static = decl_file_static;
			parser_handle_generic_object_declarator(
			    decl.name, decl.type, "Unsupported generic global initializer\n");
			pfunc.file_static = saved_file_static;
		}
	}
	expect(TOK_SEMI);
}

static void 
parse_extern_global_declaration(void)
{
	expect(TOK_EXTERN);

	if (lexer_peek()->kind == TOK_AUTO)
		fatal_cur("auto storage class is not allowed at file scope\n");
	if (lexer_peek()->kind == TOK_REGISTER)
		fatal_cur("register storage class is not allowed at file scope\n");
	if (lexer_peek()->kind == TOK_STATIC)
		fatal_cur("multiple storage classes in declaration\n");
	if (token_starts_plain_thread_local_storage_specifier(
	        lexer_peek(), lexer_peek_ahead(1), lexer_peek_ahead(2)))
		reject_plain_thread_local_keyword_before_c23(lexer_peek());

	if (lexer_peek()->kind == TOK_THREAD_LOCAL) {
		reject_thread_local_storage_specifier();
		pfunc.file_thread_local = 1;
		lexer_next();
	}

	Type *type = parse_type_name();
	parser_reject_unsupported_special_type(type);
	int saw_noreturn = parser_type_name_saw_trailing_noreturn_specifier();
	if (parser_type_name_saw_thread_local_storage_specifier())
		pfunc.file_thread_local = 1;

	const Token *name = lexer_peek();
	if (name->kind == TOK_LPAREN && lexer_peek_ahead(1)->kind == TOK_STAR) {
		int ptr_dims[MAX_ARRAY_DIMS] = {0};
		int ptr_dim_count = 0;
		Type *decl_type;
		char decl_name[64] = {0};

		lexer_next(); /* ( */
		lexer_next(); /* * */
		if (lexer_peek()->kind == TOK_LPAREN &&
		    lexer_peek_ahead(1)->kind == TOK_STAR) {
			char decl_nested_name[64] = {0};

			decl_type = parse_nested_function_pointer_object_declarator_type(type,
			                                                                decl_nested_name);

			if (lexer_peek()->kind == TOK_ASSIGN) {
				lexer_next();
				parser_define_extern_object_declaration(decl_nested_name, decl_type,
				    "Unsupported extern global initializer\n");
				expect(TOK_SEMI);
				return;
			}
			expect(TOK_SEMI);
			parser_register_extern_object_declaration(decl_nested_name, decl_type);
			return;
		}
		name = parser_parenthesized_pointer_declarator_name_token(
		    "extern parenthesized declarator name");
		STRNCPY(decl_name, name->text ? name->text : "", sizeof(decl_name) - 1);
		lexer_next(); /* name */

		if (lexer_peek()->kind == TOK_LPAREN) {
			/*
			 * Function-returning-pointer extern declaration:
			 *   void (*signal(int, void (*)(int)))(int);
			 * After the name, we have the function's own param list,
			 * then ')' closing the outer '(*', then the return-type params.
			 * Treat as an extern function declaration and register it.
			 */
			Type **fn_param_types = NULL;
			int fn_param_count = 0;
			int fn_is_variadic = 0;
			int fn_fixed_params = 0;
			int fn_has_prototype = 0;
			parse_prototype_param_list(&fn_param_types, &fn_param_count,
			                          &fn_is_variadic, &fn_fixed_params,
			                          &fn_has_prototype, 1);
			expect(TOK_RPAREN); /* close outer '(*' */
			/* Register as extern function returning a pointer */
			Type *ret_type = parser_parse_returned_function_pointer_type(type);
			Type *fn_type = parser_make_function_type(ret_type, fn_param_types,
			                                          fn_param_count, fn_is_variadic,
			                                          fn_fixed_params);
			Global *g = new_global_slot(decl_name);
			g->is_extern = 1;
			apply_type_to_global(g, type_ptr(fn_type));
			parser_commit_reserved_global();
			return;
		}

		if (lexer_peek()->kind == TOK_LBRACKET)
			ptr_dim_count = parser_parse_file_scope_parenthesized_array_dims(ptr_dims);

		expect(TOK_RPAREN);
		decl_type = parser_finish_file_scope_parenthesized_pointer_object_type(
		    type, ptr_dims, ptr_dim_count);

		if (lexer_peek()->kind == TOK_ASSIGN) {
			lexer_next();
			parser_define_extern_object_declaration(decl_name, decl_type,
			    "Unsupported extern global initializer\n");
			expect(TOK_SEMI);
			return;
		}
		expect(TOK_SEMI);
		parser_register_extern_object_declaration(decl_name, decl_type);
		return;
	}

	{
		ParsedFileScopeDeclarator decl = {0};

		parse_file_scope_declarator(type, &decl, "extern global declarator name");
		if (decl.is_function) {
			parser_handle_file_scope_function_declarator(&decl, saw_noreturn, 1, 0);
		} else {
			parser_handle_extern_object_declarator(
			    decl.name, decl.type, "Unsupported extern global initializer\n");
		}

		while (lexer_peek()->kind == TOK_COMMA) {
			lexer_next();
			parse_file_scope_declarator(type, &decl, "extern global declarator name");
			if (decl.is_function) {
				parser_handle_file_scope_function_declarator(&decl, saw_noreturn, 1, 0);
				continue;
			}
			parser_handle_extern_object_declarator(
			    decl.name, decl.type, "Unsupported extern global initializer\n");
		}
	}

	expect(TOK_SEMI);
}

void
parser_reset(void)
{
	parser_free_all_tables();
	parser_reset_scalar_state();
	if (parser_pragma_pack_stack_name) {
		for (int i = 0; i < PARSER_PRAGMA_PACK_STACK_MAX; i++) {
			xfree(parser_pragma_pack_stack_name[i]);
			parser_pragma_pack_stack_name[i] = NULL;
		}
	}
	parser_pragma_pack_align = 0;
	parser_pragma_pack_stack_count = 0;
}

Node *
parse_program(const char *filename, const char *source)
{
	parser_profile_reset_internal();
	parser_reset();
	lexer_set_filename(filename ? filename : "<input>");
	lexer_init(source);
	if (!punit.next_string_label) punit.next_string_label = 1; /* start at 1; persists across files */

	Node head = {0};
	Node *cur = &head;

	parser_profile_scope_enter(PARSER_PROF_TOPLEVEL);

	while (lexer_peek()->kind != TOK_EOF) {
		parser_trace_toplevel("toplevel", lexer_peek());
		int saw_function_specifier = 0;
		pfunc.file_static = 0;
		pfunc.file_thread_local = 0;
		pfunc.gnu_extern_inline_definition = 0;
		parser_set_pending_decl_noreturn(0);

		/* Accept empty declarations at file scope: ";" */
		if (lexer_peek()->kind == TOK_SEMI) {
			lexer_next();
			continue;
		}

		if (parser_try_consume_pragma_pack())
			continue;

		if (lexer_peek()->kind == TOK_AUTO)
			fatal_cur("auto storage class is not allowed at file scope\n");

		if (lexer_peek()->kind == TOK_REGISTER)
			fatal_cur("register storage class is not allowed at file scope\n");

		if (token_starts_plain_thread_local_storage_specifier(
		        lexer_peek(), lexer_peek_ahead(1), lexer_peek_ahead(2)))
			reject_plain_thread_local_keyword_before_c23(lexer_peek());

		if (lexer_peek()->kind == TOK_THREAD_LOCAL) {
			reject_thread_local_storage_specifier();
			pfunc.file_thread_local = 1;
			lexer_next();
		}

		/*
		 * Preserve file-scope static linkage instead of letting
		 * skip_inline_qualifiers() discard it.
		 */
		for (;;) {
			if (lexer_peek()->kind == TOK_STATIC) {
				pfunc.file_static = 1;
				lexer_next();
				continue;
			}

			if (token_starts_plain_thread_local_storage_specifier(
			        lexer_peek(), lexer_peek_ahead(1), lexer_peek_ahead(2)))
				reject_plain_thread_local_keyword_before_c23(lexer_peek());

			if (lexer_peek()->kind == TOK_THREAD_LOCAL) {
				reject_thread_local_storage_specifier();
				if (pfunc.file_thread_local)
					fatal_cur("multiple storage classes in declaration\n");
				pfunc.file_thread_local = 1;
				lexer_next();
				continue;
			}

			if (lexer_peek()->kind == TOK_INLINE || lexer_peek()->kind == TOK_NORETURN) {
				if (lexer_peek()->kind == TOK_INLINE && tcc_lang_is_c89_or_c90())
					fatal_cur("inline is not allowed in C89/C90 mode\n");
				reject_c89_c99_keyword_token(lexer_peek()->kind);
				saw_function_specifier = 1;
				if (lexer_peek()->kind == TOK_NORETURN)
					parser_set_pending_decl_noreturn(1);
				lexer_next();
				continue;
			}

			break;
		}

		if (lexer_peek()->kind == TOK_AUTO)
			fatal_cur("auto storage class is not allowed at file scope\n");

		if (lexer_peek()->kind == TOK_REGISTER)
			fatal_cur("register storage class is not allowed at file scope\n");

		if (token_starts_plain_thread_local_storage_specifier(
		        lexer_peek(), lexer_peek_ahead(1), lexer_peek_ahead(2)))
			reject_plain_thread_local_keyword_before_c23(lexer_peek());

		if (lexer_peek()->kind == TOK_THREAD_LOCAL) {
			reject_thread_local_storage_specifier();
			if (pfunc.file_thread_local)
				fatal_cur("multiple storage classes in declaration\n");
			pfunc.file_thread_local = 1;
			lexer_next();
		}

			/*
			 * Handle extern before skip_inline_qualifiers(), because that helper
			 * consumes TOK_EXTERN.  If this is an extern function prototype, let
			 * the normal prototype parser record its return metadata; otherwise
			 * parse the extern object declaration as declaration-only.
			 */
			if (lexer_peek()->kind == TOK_EXTERN) {
				if (pfunc.file_static)
					fatal_cur("multiple storage classes in declaration\n");
				if (looks_like_function_definition_start()) {
					if (parser_decl_prefix_has_extern_and_inline(1)) {
						pfunc.file_static = 1;
						pfunc.gnu_extern_inline_definition = 1;
					}
					cur->next = parse_function_profiled();
					cur = cur->next;
					continue;
				}
				if (try_parse_prototype_profiled()) {
					continue;
				}

				parse_extern_global_declaration();
				continue;
			}

		skip_decl_prefix_specifiers();

		if (lexer_peek()->kind == TOK_EOF)
			break;

		/* "int (*f1(params1))(params2);" / "{ body }"
		 * and "int (*f1(params1))[N];" / "{ body }" */
		if (is_type_start_token(lexer_peek()->kind, lexer_peek()->text) &&
		    lexer_peek_ahead(1)->kind == TOK_LPAREN &&
		    lexer_peek_ahead(2)->kind == TOK_STAR &&
		    lexer_peek_ahead(3)->kind == TOK_IDENT &&
		    lexer_peek_ahead(4)->kind == TOK_LPAREN) {
			/* consume return type */
			Type *frf_ret = parse_type_name(); (void)frf_ret;
			expect(TOK_LPAREN); /* ( */
			expect(TOK_STAR);   /* * */
			/* function name */
			const Token *frf_name_tok = lexer_peek();
			char frf_name[64] = {0};
			if (frf_name_tok->kind == TOK_IDENT)
				STRNCPY(frf_name, frf_name_tok->text, sizeof(frf_name)-1);
			lexer_next();
			/* Initialize function state before registering params */
			parser_reset_local_scope_state();
			/* consume function's own parameter list and register params */
			expect(TOK_LPAREN);
			int frf_param_n = 0;
			while (lexer_peek()->kind != TOK_RPAREN && lexer_peek()->kind != TOK_EOF) {
				Type *pt = NULL;
				if (is_type_start_token(lexer_peek()->kind, lexer_peek()->text)) {
					pt = parse_type_name();
					while (lexer_peek()->kind == TOK_STAR) {
						lexer_next();
						pt = type_ptr(pt);
					}
					/* Handle function pointer param: void (*)(int) or void (*name)(int) */
					if (lexer_peek()->kind == TOK_LPAREN &&
					    lexer_peek_ahead(1)->kind == TOK_STAR) {
						/* consume (*...) part */
						int depth = 1;
						lexer_next(); /* consume ( */
						while (depth > 0 && lexer_peek()->kind != TOK_EOF) {
							if (lexer_peek()->kind == TOK_LPAREN) depth++;
							else if (lexer_peek()->kind == TOK_RPAREN) depth--;
							lexer_next();
						}
						/* consume trailing param list (int) if present */
						if (lexer_peek()->kind == TOK_LPAREN) {
							depth = 1;
							lexer_next();
							while (depth > 0 && lexer_peek()->kind != TOK_EOF) {
								if (lexer_peek()->kind == TOK_LPAREN) depth++;
								else if (lexer_peek()->kind == TOK_RPAREN) depth--;
								lexer_next();
							}
						}
						pt = type_ptr(type_for_size(8)); /* function pointer */
					}
				}
				if (lexer_peek()->kind == TOK_IDENT) {
					const char *pname = lexer_peek()->text;
					if (frf_param_n < 8 && pt) {
						int poff = add_typed_local(pname, pt);
						/* fix up ABI offset — params arrive at positive offsets */
						for (int pi = pscope.local_count - 1; pi >= 0; pi--) {
							if (STRCMP(pscope.locals[pi].name, pname) == 0) {
								pscope.locals[pi].offset = -(frf_param_n+1)*8;
								break;
							}
						}
						(void)poff;
					} else if (frf_param_n < 8) {
						int poff = add_local_sized(pname, 1, 0);
						pscope.locals[pscope.local_count-1].offset = -(frf_param_n+1)*8;
						(void)poff;
					}
					frf_param_n++;
					lexer_next();
				}
				if (lexer_peek()->kind == TOK_COMMA) lexer_next(); else break;
			}
			/* Params above were placed at fixed 8-byte ABI slots (offset
			 * -(n+1)*8) but add_local_sized only grew stack_size by 4 each,
			 * leaving the frame too small — the first accumulator spill would
			 * then overwrite the last parameter. Reconcile stack_size with the
			 * slot layout actually used so the prologue reserves enough. */
			if (frf_param_n > 0) {
				int frf_needed = frf_param_n * 8;
				if (pscope.stack_size < frf_needed)
					pscope.stack_size = frf_needed;
			}
			expect(TOK_RPAREN);
			expect(TOK_RPAREN);
			Type *frf_ret_type;
			if (lexer_peek()->kind == TOK_LBRACKET) {
				int dims[MAX_ARRAY_DIMS] = {0};
				int dim_count = parse_array_dimensions(dims, 0, 0);
				Type *array_type = build_array_type_from_dims(clone_type(frf_ret), dims, dim_count);
				frf_ret_type = type_ptr(array_type);
			} else {
				frf_ret_type = parser_parse_returned_function_pointer_type(frf_ret);
			}
			skip_inline_qualifiers();
			FuncInfo *frf_info =
			    parser_register_pointer_returning_function_declaration(
			        frf_name, frf_ret_type, 0, 0);
			if (lexer_peek()->kind == TOK_SEMI) {
				lexer_next();
				/* Reset local param scope without touching typedef/struct counts */
				pscope.local_count = 0;
				parser_invalidate_local_lookup_cache();
				pscope.stack_size = 0;
				pfunc.function_name[0] = '\0';
				continue;
			}
			if (lexer_peek()->kind != TOK_LBRACE)
				fatal_cur("Expected ';' or function body for function-returning-pointer declaration\n");
			parser_prepare_pointer_returning_function_definition(
			    frf_info, frf_name, frf_ret_type);
			Node *frf_body = parse_block_contents();
			stmt_resolve_function_gotos();
			expect(TOK_RBRACE);
			cur->next = new_func(frf_name, frf_body, pscope.stack_size, frf_param_n);
			cur = cur->next;
			cur->is_pointer = 1;
			cur->elem_size = TCC_SIZEOF_PTR;
			cur->type = type_ptr(type_for_size(TCC_SIZEOF_PTR));
			continue;
		}

		/*
		 * Parse all function definitions, including signed/unsigned/short/long
		 * return types, through parse_function().  The integer-modifier path below
		 * is now only for file-scope object declarations, so parameter parsing and
		 * variadic metadata have a single source of truth.
		 */
		if (looks_like_function_definition_start()) {
			cur->next = parse_function_profiled();
			cur = cur->next;
			continue;
		}

		/*
		 * Try function prototypes after definition checks.  The prototype
		 * parser consumes tokens while validating parameter types, so it must
		 * not run first on real definitions.
		 */
		if (try_parse_prototype_profiled()) {
			continue;
		}

		if (saw_function_specifier)
			fatal_cur("function specifier is only valid on function declarations\n");

		if (pfunc.file_static && lexer_peek()->kind == TOK_TYPEDEF)
			fatal_cur("multiple storage classes in declaration\n");

		if (parse_typedef_declaration())
			continue;

		if (parse_static_assert_declaration())
			continue;

		if (token_starts_alignas_specifier(lexer_peek(), lexer_peek_ahead(1))) {
			parse_generic_global_declaration_profiled();
			continue;
		}

		/* Enum definition "enum E { ... };" or bare forward "enum E;" --
		 * but NOT "enum E fn(...)" which is a function with enum return type. */
		if (lexer_peek()->kind == TOK_ENUM &&
		    (lexer_peek_ahead(2)->kind == TOK_LBRACE ||
		     lexer_peek_ahead(2)->kind == TOK_SEMI ||
		     lexer_peek_ahead(1)->kind == TOK_LBRACE)) {
			parse_enum_specifier_profiled();
			expect(TOK_SEMI);
			continue;
		}

		if (lexer_peek()->kind == TOK_STRUCT && lexer_is_struct_definition()) {
			parse_struct_definition_profiled();
			continue;
		}

		if (lexer_peek()->kind == TOK_UNION &&
		        lexer_peek_ahead(1)->kind == TOK_IDENT &&
		        lexer_peek_ahead(2)->kind == TOK_LBRACE) {
			parse_union_definition_profiled();
			continue;
		}

		/* "int (*(*f1(params1))(params2))(params3);" / "{ body }" */
		if (is_type_start_token(lexer_peek()->kind, lexer_peek()->text) &&
		    lexer_peek_ahead(1)->kind == TOK_LPAREN &&
		    lexer_peek_ahead(2)->kind == TOK_STAR &&
		    lexer_peek_ahead(3)->kind == TOK_LPAREN &&
		    lexer_peek_ahead(4)->kind == TOK_STAR &&
		    lexer_peek_ahead(5)->kind == TOK_IDENT &&
		    lexer_peek_ahead(6)->kind == TOK_LPAREN) {
			Type *frf_ret = parse_type_name();
			Type **retfp_param_types[8] = {0};
			int retfp_param_count[8] = {0};
			int retfp_is_variadic[8] = {0};
			int retfp_fixed_params[8] = {0};
			int retfp_has_prototype[8] = {0};
			int retfp_level_count = 0;
			Type *ret_type;
			FuncInfo *frf_info;

			expect(TOK_LPAREN); /* outer ( */
			expect(TOK_STAR);   /* outer * */
			expect(TOK_LPAREN); /* inner ( */
			expect(TOK_STAR);   /* inner * */

			const Token *frf_name_tok = lexer_peek();
			char frf_name[64] = {0};
			if (frf_name_tok->kind == TOK_IDENT)
				STRNCPY(frf_name, frf_name_tok->text, sizeof(frf_name) - 1);
			lexer_next();

			parser_reset_local_scope_state();
			expect(TOK_LPAREN);
			int frf_param_n = 0;
			while (lexer_peek()->kind != TOK_RPAREN && lexer_peek()->kind != TOK_EOF) {
				if (is_type_start_token(lexer_peek()->kind, lexer_peek()->text)) {
					Type *pt = parse_type_name(); (void)pt;
					while (lexer_peek()->kind == TOK_STAR)
						lexer_next();
				}
				if (lexer_peek()->kind == TOK_IDENT) {
					const char *pname = lexer_peek()->text;
					if (frf_param_n < 8) {
						int poff = add_local_sized(pname, 1, 0);
						pscope.locals[pscope.local_count - 1].offset = -(frf_param_n + 1) * 8;
						(void)poff;
					}
					frf_param_n++;
					lexer_next();
				}
				if (lexer_peek()->kind == TOK_COMMA)
					lexer_next();
				else
					break;
			}
			/* See note in the other frf path: params sit at -(n+1)*8 slots,
			 * so the frame must be at least frf_param_n*8 bytes. */
			if (frf_param_n > 0) {
				int frf_needed = frf_param_n * 8;
				if (pscope.stack_size < frf_needed)
					pscope.stack_size = frf_needed;
			}
			expect(TOK_RPAREN);
			expect(TOK_RPAREN);

			parse_prototype_param_list(&retfp_param_types[retfp_level_count],
			                          &retfp_param_count[retfp_level_count],
			                          &retfp_is_variadic[retfp_level_count],
			                          &retfp_fixed_params[retfp_level_count],
			                          &retfp_has_prototype[retfp_level_count], 1);
			retfp_level_count++;
			expect(TOK_RPAREN);

			while (lexer_peek()->kind == TOK_LPAREN) {
				if (retfp_level_count >= 8)
					fatal_cur("Too many nested function-pointer return declarators\n");
				parse_prototype_param_list(&retfp_param_types[retfp_level_count],
				                          &retfp_param_count[retfp_level_count],
				                          &retfp_is_variadic[retfp_level_count],
				                          &retfp_fixed_params[retfp_level_count],
				                          &retfp_has_prototype[retfp_level_count], 1);
				retfp_level_count++;
			}
			skip_inline_qualifiers();

			ret_type = parser_canonicalize_decl_type(frf_ret);
			for (int level = retfp_level_count - 1; level >= 0; level--) {
				ret_type = type_ptr(retfp_has_prototype[level]
				                    ? parser_make_function_type(ret_type,
				                                                retfp_param_types[level],
				                                                retfp_param_count[level],
				                                                retfp_is_variadic[level],
				                                                retfp_fixed_params[level])
				                    : type_func(clone_type(ret_type)));
			}

			frf_info = parser_register_pointer_returning_function_declaration(
			    frf_name, ret_type, 0, 0);

			if (lexer_peek()->kind == TOK_SEMI) {
				lexer_next();
				continue;
			}
			if (lexer_peek()->kind != TOK_LBRACE)
				fatal_cur("Expected ';' or function body for nested function-returning-function-pointer declaration\n");
			parser_prepare_pointer_returning_function_definition(
			    frf_info, frf_name, ret_type);
			Node *frf_body = parse_block_contents();
			stmt_resolve_function_gotos();
			expect(TOK_RBRACE);
			cur->next = new_func(frf_name, frf_body, pscope.stack_size, frf_param_n);
			cur = cur->next;
			cur->is_pointer = 1;
			cur->elem_size = TCC_SIZEOF_PTR;
			cur->type = type_ptr(type_for_size(TCC_SIZEOF_PTR));
			continue;
		}

		/* "int (*f1(params1))(params2);" / "{ body }" — function returning function pointer */
		if (is_type_start_token(lexer_peek()->kind, lexer_peek()->text) &&
		    lexer_peek_ahead(1)->kind == TOK_LPAREN &&
		    lexer_peek_ahead(2)->kind == TOK_STAR &&
		    lexer_peek_ahead(3)->kind == TOK_IDENT &&
		    lexer_peek_ahead(4)->kind == TOK_LPAREN) {
			/* consume return type */
			Type *frf_ret = parse_type_name(); (void)frf_ret;
			expect(TOK_LPAREN); /* ( */
			expect(TOK_STAR);   /* * */
			/* function name */
			const Token *frf_name_tok = lexer_peek();
			char frf_name[64] = {0};
			if (frf_name_tok->kind == TOK_IDENT)
				STRNCPY(frf_name, frf_name_tok->text, sizeof(frf_name)-1);
			lexer_next();
			/* Initialize function state before registering params */
			parser_reset_local_scope_state();
			/* consume function's own parameter list and register params */
			expect(TOK_LPAREN);
			int frf_param_n = 0;
			while (lexer_peek()->kind != TOK_RPAREN && lexer_peek()->kind != TOK_EOF) {
				/* skip type qualifiers and type name */
				if (is_type_start_token(lexer_peek()->kind, lexer_peek()->text)) {
					Type *pt = parse_type_name(); (void)pt;
					while (lexer_peek()->kind == TOK_STAR) lexer_next();
				}
				/* param name */
				if (lexer_peek()->kind == TOK_IDENT) {
					const char *pname = lexer_peek()->text;
					if (frf_param_n < 8) {
						int poff = add_local_sized(pname, 1, 0);
						pscope.locals[pscope.local_count-1].offset = -(frf_param_n+1)*8;
						(void)poff;
					}
					frf_param_n++;
					lexer_next();
				}
				if (lexer_peek()->kind == TOK_COMMA) lexer_next(); else break;
			}
			/* See note in the other frf paths: params sit at -(n+1)*8 slots,
			 * so the frame must be at least frf_param_n*8 bytes. */
			if (frf_param_n > 0) {
				int frf_needed = frf_param_n * 8;
				if (pscope.stack_size < frf_needed)
					pscope.stack_size = frf_needed;
			}
			expect(TOK_RPAREN);
			/* consume ) of (*f1...) */
			expect(TOK_RPAREN);
			Type *frf_ret_type;
			if (lexer_peek()->kind == TOK_LBRACKET) {
				int dims[MAX_ARRAY_DIMS] = {0};
				int dim_count = parse_array_dimensions(dims, 0, 0);
				Type *array_type = build_array_type_from_dims(clone_type(frf_ret), dims, dim_count);
				frf_ret_type = type_ptr(array_type);
			} else {
				/* Register as a function returning a function pointer. */
				frf_ret_type = parser_parse_returned_function_pointer_type(frf_ret);
			}
			skip_inline_qualifiers();
			FuncInfo *frf_info =
			    parser_register_pointer_returning_function_declaration(
			        frf_name, frf_ret_type, 0, 0);
			if (lexer_peek()->kind == TOK_SEMI) {
				lexer_next();
				continue;
			}
			if (lexer_peek()->kind != TOK_LBRACE)
				fatal_cur("Expected ';' or function body for function-returning-function-pointer declaration\n");
			parser_prepare_pointer_returning_function_definition(
			    frf_info, frf_name, NULL);
			Node *frf_body = parse_block_contents();
			stmt_resolve_function_gotos();
			expect(TOK_RBRACE);
			cur->next = new_func(frf_name, frf_body, pscope.stack_size, frf_param_n);
			cur = cur->next;
			cur->is_pointer = 1;
			cur->elem_size = TCC_SIZEOF_PTR;
			cur->type = type_ptr(type_for_size(TCC_SIZEOF_PTR));
			continue;
		}

		if (try_parse_prototype_profiled()) {
			continue;
		}

		if (is_type_start_token(lexer_peek()->kind, lexer_peek()->text) && !looks_like_function_definition_start()) {
			parse_generic_global_declaration_profiled();
			continue;
		}

		cur->next = parse_function_profiled();
		cur = cur->next;
	}

	parser_profile_scope_leave(PARSER_PROF_TOPLEVEL);
	validate_complete_global_object_types();
	return head.next;
}

/* Returns number of fixed named params for variadic functions, -1 if not variadic */
int
func_fixed_params(const char *name)
{
	FuncInfo *fi = find_func(name);
	if (fi && fi->is_variadic)
		return fi->fixed_param_count;
	return -1;
}


int 
struct_field_count(const char *struct_name)
{
	StructDef *def = find_struct_or_null(struct_name);
	return def ? def->field_count : 0;
}

int 
struct_field_offset(const char *struct_name, int fi)
{
	StructDef *def = find_struct_or_null(struct_name);
	if (!def || fi < 0 || fi >= def->field_count) return 0;
	return def->fields[fi].offset;
}

int 
struct_field_size(const char *struct_name, int fi)
{
	StructDef *def = find_struct_or_null(struct_name);
	if (!def || fi < 0 || fi >= def->field_count) return 4;
	return def->fields[fi].size ? def->fields[fi].size : 4;
}

int
struct_field_debug_type_id(const char *struct_name, int fi)
{
	StructDef *def = find_struct_or_null(struct_name);
	if (!def || fi < 0 || fi >= def->field_count)
		return DBG_TYPE_INT;
	return type_debug_type_id(def->fields[fi].type);
}

const char *
struct_field_struct_name(const char *struct_name, int fi)
{
	StructDef *def = find_struct_or_null(struct_name);
	Type *type;
	if (!def || fi < 0 || fi >= def->field_count)
		return "";
	type = def->fields[fi].type;
	if (!type || type->kind != TY_STRUCT || !type->struct_name[0])
		return "";
	return type->struct_name;
}

int
struct_field_array_len(const char *struct_name, int fi)
{
	StructDef *def = find_struct_or_null(struct_name);
	Type *type;
	if (!def || fi < 0 || fi >= def->field_count)
		return 0;
	type = def->fields[fi].type;
	if (!type || type->kind != TY_ARRAY)
		return 0;
	return type->array_len;
}

int
struct_field_array_elem_debug_type_id(const char *struct_name, int fi)
{
	StructDef *def = find_struct_or_null(struct_name);
	Type *type;
	if (!def || fi < 0 || fi >= def->field_count)
		return DBG_TYPE_INT;
	type = def->fields[fi].type;
	if (!type || type->kind != TY_ARRAY || !type->base)
		return DBG_TYPE_INT;
	return type_debug_type_id(type->base);
}

const char *
struct_field_array_elem_struct_name(const char *struct_name, int fi)
{
	StructDef *def = find_struct_or_null(struct_name);
	Type *type;
	if (!def || fi < 0 || fi >= def->field_count)
		return "";
	type = def->fields[fi].type;
	if (!type || type->kind != TY_ARRAY || !type->base || !type_is_struct(type->base))
		return "";
	return type->base->struct_name;
}

const char *
struct_field_name(const char *struct_name, int fi)
{
	StructDef *def = find_struct_or_null(struct_name);
	if (!def || fi < 0 || fi >= def->field_count)
		return "";
	return def->fields[fi].name;
}

int 
struct_size(const char *struct_name)
{
	StructDef *def = find_struct_or_null(struct_name);
	return def ? def->size : 0;
}

int
struct_hfa_info(const char *struct_name, int *elem_size, int *elem_count)
{
	StructDef *def = find_struct_or_null(struct_name);
	int size = 0;

	if (elem_size)
		*elem_size = 0;
	if (elem_count)
		*elem_count = 0;
	if (!def || !def->is_complete || def->is_union ||
	    def->field_count <= 0 || def->field_count > 4)
		return 0;

	for (int i = 0; i < def->field_count; i++) {
		Field *field = &def->fields[i];

		if (!field->type || !type_is_floating(field->type) ||
		    field->is_array || field->is_bitfield)
			return 0;
		if (size == 0)
			size = type_sizeof(field->type);
		if (type_sizeof(field->type) != size)
			return 0;
		if (field->size != size || field->offset != i * size)
			return 0;
	}
	if (def->size != def->field_count * size)
		return 0;

	if (elem_size)
		*elem_size = size;
	if (elem_count)
		*elem_count = def->field_count;
	return 1;
}

static const char *
parser_direct_complex_target_abi_name(const Type *type)
{
	if (!type || !type_is_complex(type))
		return NULL;
	if (parser_target_is_arm64()) {
		if (type_sizeof(type) == 8)
			return "__tcc_hfa_complex_float2";
		if (type_sizeof(type) == 16)
			return "__tcc_hfa_complex_double2";
	}
	if (parser_target_is_x64()) {
		if (type_sizeof(type) == 8)
			return "__tcc_x64_complex_float2";
		if (type_sizeof(type) == 16)
			return "__tcc_x64_complex_double2";
	}
	return NULL;
}

int
parser_direct_complex_lane_info(const Type *type, int *elem_size, int *elem_count)
{
	const char *abi_name = parser_direct_complex_target_abi_name(type);

	if (elem_size)
		*elem_size = 0;
	if (elem_count)
		*elem_count = 0;
	if (!abi_name)
		return 0;
	if (type_sizeof(type) == 8) {
		if (elem_size)
			*elem_size = 4;
		if (elem_count)
			*elem_count = 2;
		return 1;
	}
	if (type_sizeof(type) == 16) {
		if (elem_size)
			*elem_size = 8;
		if (elem_count)
			*elem_count = 2;
		return 1;
	}
	return 0;
}

static const char *
parser_arm64_direct_complex_abi_name(const Type *type)
{
	const char *abi_name = parser_direct_complex_target_abi_name(type);

	if (!abi_name || !parser_target_is_arm64())
		return NULL;
	return abi_name;
}

int
parser_arm64_hfa_info_name(const char *name, int *elem_size, int *elem_count)
{
	if (elem_size)
		*elem_size = 0;
	if (elem_count)
		*elem_count = 0;
	if (!name || !name[0])
		return 0;
	if (STRCMP(name, "__tcc_hfa_complex_float2") == 0) {
		if (elem_size)
			*elem_size = 4;
		if (elem_count)
			*elem_count = 2;
		return 1;
	}
	if (STRCMP(name, "__tcc_hfa_complex_double2") == 0) {
		if (elem_size)
			*elem_size = 8;
		if (elem_count)
			*elem_count = 2;
		return 1;
	}
	return struct_hfa_info(name, elem_size, elem_count);
}

int
parser_arm64_hfa_info_type(const Type *type, int *elem_size, int *elem_count)
{
	const char *complex_name;
	const char *struct_name;

	if (elem_size)
		*elem_size = 0;
	if (elem_count)
		*elem_count = 0;
	if (!type || preprocess_get_target() != PP_TARGET_ARM64)
		return 0;
	complex_name = parser_arm64_direct_complex_abi_name(type);
	if (complex_name)
		return parser_arm64_hfa_info_name(complex_name, elem_size, elem_count);
	if (!type_is_struct(type))
		return 0;
	struct_name = type->struct_name[0] ? type->struct_name
	                                   : parser_resolve_struct_type_name((Type *)type);
	if (!struct_name || !struct_name[0])
		return 0;
	return parser_arm64_hfa_info_name(struct_name, elem_size, elem_count);
}

int
parser_classify_aggregate_abi(Type *type, int *reg_count_out)
{
	int hfa_elem_size = 0;
	int hfa_elem_count = 0;
	int size = 0;

	if (reg_count_out)
		*reg_count_out = 0;
	if (!type || (!type_is_struct(type) && !type_is_complex(type)))
		return AGGREGATE_ABI_NONE;

	size = type_sizeof(type);
	if (preprocess_get_target() == PP_TARGET_X64 &&
	    type_is_complex(type) &&
	    size == 8) {
		if (reg_count_out)
			*reg_count_out = 1;
		return AGGREGATE_ABI_X64_COMPLEX_FLOAT;
	}
	if (preprocess_get_target() == PP_TARGET_X64 &&
	    type_is_complex(type) &&
	    size == 16) {
		if (reg_count_out)
			*reg_count_out = 2;
		return AGGREGATE_ABI_X64_COMPLEX_DOUBLE;
	}
	if (preprocess_get_target() != PP_TARGET_ARM64)
		return AGGREGATE_ABI_BYREF;
	if (parser_arm64_hfa_info_type(type, &hfa_elem_size, &hfa_elem_count)) {
		if (reg_count_out)
			*reg_count_out = hfa_elem_count;
		return AGGREGATE_ABI_HFA;
	}
	if (!type_is_struct(type))
		return AGGREGATE_ABI_BYREF;
	if (size > 0 && size <= 16) {
		if (reg_count_out)
			*reg_count_out = (size + (TCC_SIZEOF_PTR - 1)) / TCC_SIZEOF_PTR;
		return AGGREGATE_ABI_INTREGS;
	}
	return AGGREGATE_ABI_BYREF;
}

static void
validate_complete_global_object_types(void)
{
	for (int i = 0; i < punit.global_count; i++) {
		Global *g = &punit.globals[i];

		if (g->is_extern || !g->type)
			continue;
		if (STRNCMP(g->name, "__static_", 9) == 0)
			continue;
		if (g->is_array && g->array_len == 0 && !g->has_initializer) {
			g->array_len = 1;
			g->array_dims[0] = 1;
			if (g->array_dim_count <= 0)
				g->array_dim_count = 1;
			if (g->type && type_is_array(g->type))
				g->type = type_array(clone_type(type_pointee(g->type)), 1);
		}
		if (type_has_incomplete_object_aggregate(g->type))
			fatal_cur("global cannot have incomplete type\n");
		}
	}
