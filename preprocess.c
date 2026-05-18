#include <ctype.h>


#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>

#include "tcc.h"
#include "preprocess.h"

static Macro macros[512];
static int macro_count;

static PreprocessTarget configured_target = PP_TARGET_X86;
static int builtins_initialized = 0;
static char tcc_include_dir[512] = {0};  /* installed include dir, set by main */
static int preprocess_emit_line_markers = 0; /* -dD: emit #line markers in -E output */
static char preprocess_file_buf[512] = "<input>";
static const char *preprocess_file = preprocess_file_buf;
int preprocess_current_line = 1;
const char *pp_line;



static const char *
pptok_name(PPTokenKind kind)
{
	switch (kind) {
	case PPTOK_IDENT:
		return "PPTOK_IDENT";
	case PPTOK_NUMBER:
		return "PPTOK_NUMBER";
	case PPTOK_STRING:
		return "PPTOK_STRING";
	case PPTOK_CHAR:
		return "PPTOK_CHAR";
	case PPTOK_PUNCT:
		return "PPTOK_PUNCT";
	case PPTOK_SPACE:
		return "PPTOK_SPACE";
	case PPTOK_NEWLINE:
		return "PPTOK_NEWLINE";
	case PPTOK_OTHER:
		return "PPTOK_OTHER";
	case PPTOK_EOF:
		return "PPTOK_EOF";
	default:
		return "PPTOK_UNKNOWN";
	}
}

static void
pptok_push(PPTokenVec *vec, PPTokenKind kind, const char *start, size_t len)
{
	Debug(5, "pptok_push %s len=%lu text=[%.*s]\n",
	      pptok_name(kind),
	      (unsigned long)len,
	      (int)len,
	      start);

	if (vec->count >= vec->cap) {
		vec->cap = vec->cap ? vec->cap * 2 : 32;
		PPToken *new_items = xrealloc(vec->items, sizeof(PPToken) * (size_t)vec->cap);
		vec->items = new_items;
	}

	PPToken *tok = &vec->items[vec->count++];
	tok->kind = kind;
	tok->text = xstrndup(start,len);
	Debug(5,"TOKTEXT [%s]\n",tok->text);
}

static void
pptok_free(PPTokenVec *vec)
{
	for (int i = 0; i < vec->count; i++)
		xfree(vec->items[i].text);

	xfree(vec->items);
	vec->items = NULL;
	vec->count = 0;
	vec->cap = 0;
}

static void
pp_tokenize_line(const char *line, PPTokenVec *out)
{
	const char *p = line;

	Debug(5,"Line: [%s]\n",line);
	while (*p) {
		const char *start = p;

		if (isspace((unsigned char)*p)) {
			while (*p && isspace((unsigned char)*p))
				p++;
			Debug(5,"pptok_push PPTOK_SPACE\n");
			pptok_push(out, PPTOK_SPACE, start, (size_t)(p - start));
			continue;
		}

		if (isalpha((unsigned char)*p) || *p == '_') {
			while (isalnum((unsigned char)*p) || *p == '_')
				p++;
			Debug(5,"pptok_push PPTOK_IDENT\n");
			pptok_push(out, PPTOK_IDENT, start, (size_t)(p - start));
			continue;
		}

		if (isdigit((unsigned char)*p)) {
			while (isalnum((unsigned char)*p) || *p == '_')
				p++;
			Debug(5,"pptok_push PPTOK_NUMBER\n");
			pptok_push(out, PPTOK_NUMBER, start, (size_t)(p - start));
			continue;
		}

		if (*p == '"') {
			Debug(5,"In p loop\n");
			p++;
			while (*p) {
				if (*p == '\\' && p[1]) {
					p += 2;
					continue;
				}

				if (*p == '"') {
					p++;
					break;
				}

				p++;
			}

			pptok_push(out, PPTOK_STRING, start, (size_t)(p - start));
			continue;
		}

		if (*p == '\'') {
			Debug(5,"In slash loop\n");
			p++;
			while (*p) {
				if (*p == '\\' && p[1]) {
					p += 2;
					continue;
				}

				if (*p == '\'') {
					p++;
					break;
				}

				p++;
			}

			pptok_push(out, PPTOK_CHAR, start, (size_t)(p - start));
			continue;
		}

		if ((p[0] == '#' && p[1] == '#') ||
		        (p[0] == '&' && p[1] == '&') ||
		        (p[0] == '|' && p[1] == '|') ||
		        (p[0] == '=' && p[1] == '=') ||
		        (p[0] == '!' && p[1] == '=') ||
		        (p[0] == '<' && p[1] == '=') ||
		        (p[0] == '>' && p[1] == '=') ||
		        (p[0] == '<' && p[1] == '<') ||
		        (p[0] == '>' && p[1] == '>')) {
			p += 2;
			Debug(5,"pptok_push PPTOK_PUNCT1\n");
			pptok_push(out, PPTOK_PUNCT, start, 2);
			continue;
		}

		p++;
		Debug(5,"pptok_push PPTOK_PUNCT\n");
		pptok_push(out, PPTOK_PUNCT, start, 1);
	}

}

static void
append_char(char **out, size_t *len, size_t *cap, char ch)
{
	if (*len + 2 >= *cap) {
		*cap *= 2;
		char *new_out = xrealloc(*out, *cap);
		*out = new_out;
	}

	(*out)[(*len)++] = ch;
	(*out)[*len] = '\0';
}

static void
append_str(char **out, size_t *len, size_t *cap, const char *s)
{
	while (*s)
		append_char(out, len, cap, *s++);
}

static void
append_line_marker(char **out, size_t *len, size_t *cap, int line, const char *file)
{
	char buf[64];
	Debug(1, "PP #line %d \"%s\"\n", line, file ? file : "<input>");
	snprintf(buf, sizeof(buf), "#line %d \"", line);
	append_str(out, len, cap, buf);
	append_str(out, len, cap, file ? file : "<input>");
	append_str(out, len, cap, "\"\n");
}

static Macro *
find_macro_entry(const char *name)
{
	if (!name)  {
		Debug(0,"find_macro_entry(NULL)\n");
		return NULL;
	}

	for (int i = macro_count - 1; i >= 0; i--) {
		if (STRCMP(macros[i].name, name) == 0)
			return &macros[i];
	}

	return NULL;
}

static const char __attribute__((unused)) *
find_macro(const char *name)
{
	if (!name)  {
		Debug(0,"find_macro_entry(NULL)\n");
		return NULL;
	}

	Macro *macro = find_macro_entry(name);
	if (!macro)
		return NULL;

	return macro->value;
}

static int
is_defined(const char *name)
{
	if (!name)  {
		Debug(0,"find_macro_entry(NULL)\n");
		return 0;
	}

	return find_macro_entry(name) != NULL;
}

static void
free_macro(Macro *m)
{
	if (!m) {
		Debug(0,"free_macro(NULL)\n");
		return;
	}
	xfree(m->name);
	xfree(m->value);

	for (int i = 0; i < m->param_count; i++)
		xfree(m->params[i]);

	memset(m,0,sizeof(*m));
}

static void
init_macro(Macro *macro, const char *name, const char *value)
{
	free_macro(macro);

	macro->name = xstrdup(name ? name : "");
	macro->value = xstrdup(value ? value : "");
}

static Macro *
get_or_add_macro(const char *name)
{
	if (!name)  {
		Debug(0,"get_or_add_macro(NULL)\n");
		return 0;
	}

	Macro *existing = find_macro_entry(name);
	if (existing)
		return existing;

	if (macro_count >= 512) {
		fatal_pp("Too many macros\n");
		exit(1);
	}

	return &macros[macro_count++];
}

static void
add_object_macro(const char *name, const char *value)
{
	Macro *macro = get_or_add_macro(name);
	init_macro(macro, name, value);
}

static void
add_function_macro(const char *name, char *params, int param_count,
                   int is_variadic, const char *value)
{
	Macro *macro = get_or_add_macro(name);
	init_macro(macro, name, value);

	macro->is_function_like = 1;
	macro->is_variadic = is_variadic;
	macro->param_count = param_count;

	for (int i = 0; i < param_count; i++)
		macro->params[i] = xstrdup(params + i * 64);
}

/* Compatibility wrapper for older code paths. */
static void __attribute__((unused))
add_macro(const char *name, const char *value)
{
	add_object_macro(name, value);
}

static void
add_builtin_object_macro(const char *name, const char *value)
{
	add_object_macro(name, value);
}

void
preprocess_set_include_dir(const char *dir)
{
	if (dir && dir[0]) {
		int i = 0;
		while (dir[i] && i < 511) {
			tcc_include_dir[i] = dir[i];
			i++;
		}
		tcc_include_dir[i] = '\0';
	}
}

void
preprocess_configure(PreprocessTarget target)
{
	configured_target = target;
	builtins_initialized = 0;
}

void
preprocess_set_line_markers(int enable)
{
	preprocess_emit_line_markers = enable;
}

const char *
preprocess_get_file(void)
{
	return preprocess_file ? preprocess_file : "<input>";
}

void
preprocess_set_file(const char *filename)
{
	const char *name = filename ? filename : "<input>";

	STRNCPY(preprocess_file_buf, name, sizeof(preprocess_file_buf) - 1);
	preprocess_file_buf[sizeof(preprocess_file_buf) - 1] = '\0';
	preprocess_file = preprocess_file_buf;

	char buf[512];
	snprintf(buf, sizeof(buf), "\"%s\"", name);
	add_object_macro("__FILE__", buf);
}

static void
init_builtin_macros(void)
{
	if (builtins_initialized)
		return;

	builtins_initialized = 1;

	add_builtin_object_macro("__tcc__", "1");
	add_builtin_object_macro("__TCC__", "1");
	add_builtin_object_macro("__TCC_VERSION__", TCC_VERSION_NUMBER_STR);
	add_builtin_object_macro("__TCC_VERSION_STRING__", "\"" TCC_VERSION "\"");
	add_builtin_object_macro("__LINE__", "0");

	/* __FILE__ is set by preprocess_set_file() before preprocess() is called;
	 * only set the default if it hasn't been set yet */
	if (!find_macro_entry("__FILE__"))
		add_builtin_object_macro("__FILE__", "\"unknown\"");

	/* __DATE__ and __TIME__ — set once at init to the compilation timestamp */
	{
		char date_buf[20];
		char time_buf[12];
		static const char *months[] = {
			"Jan","Feb","Mar","Apr","May","Jun",
			"Jul","Aug","Sep","Oct","Nov","Dec"
		};
		time_t now = time(NULL);
		struct tm *tm = localtime(&now);

		if (tm) {
			snprintf(date_buf, sizeof(date_buf), "%s %2d %4d",
			         months[tm->tm_mon], tm->tm_mday, tm->tm_year + 1900);
			snprintf(time_buf, sizeof(time_buf), "%02d:%02d:%02d",
			         tm->tm_hour, tm->tm_min, tm->tm_sec);
		} else {
			snprintf(date_buf, sizeof(date_buf), "Jan  1 1970");
			snprintf(time_buf, sizeof(time_buf), "00:00:00");
		}
		add_builtin_object_macro("__DATE__", date_buf);
		add_builtin_object_macro("__TIME__", time_buf);
	}

	switch (configured_target) {
	case PP_TARGET_X86:
		add_builtin_object_macro("i386", "1");
		add_builtin_object_macro("__i386__", "1");
		add_builtin_object_macro("__unix__", "1");
		add_builtin_object_macro("unix", "1");
		add_builtin_object_macro("__linux__", "1");
		add_builtin_object_macro("linux", "1");
		break;

	case PP_TARGET_X64:
		add_builtin_object_macro("__x86_64__", "1");
		add_builtin_object_macro("__unix__", "1");
		add_builtin_object_macro("unix", "1");
		add_builtin_object_macro("__linux__", "1");
		add_builtin_object_macro("linux", "1");
		break;

	case PP_TARGET_ARM64:
		add_builtin_object_macro("__aarch64__", "1");
		add_builtin_object_macro("__APPLE__", "1");
		add_builtin_object_macro("__MACH__", "1");
		break;

	case PP_TARGET_MIPS:
		add_builtin_object_macro("__mips__", "1");
		add_builtin_object_macro("mips", "1");
		add_builtin_object_macro("__unix__", "1");
		add_builtin_object_macro("unix", "1");
		break;
	}
}

static void
remove_macro(const char *name)
{
	if (!name) {
		Debug(0,"remove_macro(NULL)\n");
		return;
	}

	for (int i = 0; i < macro_count; i++) {
		if (STRCMP(macros[i].name, name) == 0) {
			macros[i] = macros[macro_count - 1];
			macro_count--;
			return;
		}
	}
}

static void
trim_trailing(char *s)
{
	size_t len = strlen(s);

	while (len > 0 && isspace((unsigned char)s[len - 1]))
		s[--len] = '\0';
}

static char *
read_include_file(const char *path)
{
	FILE *file = fopen(path, "rb");
	if (!file) {
		perror(path);
		exit(1);
	}

	if (fseek(file, 0, SEEK_END) != 0) {
		perror("fseek");
		fclose(file);
		exit(1);
	}

	long size = ftell(file);
	if (size < 0) {
		perror("ftell");
		fclose(file);
		exit(1);
	}

	rewind(file);

	char *buffer = xcalloc(1, (size_t)size + 1);
	size_t nread = fread(buffer, 1, (size_t)size, file);
	if (nread != (size_t)size) {
		tcc_warn("Failed to read complete include file: %s\n", path);
		xfree(buffer);
		fclose(file);
		exit(1);
	}

	fclose(file);
	return buffer;
}

static void
parse_directive_name(const char *p, char *name, size_t name_size)
{
	while (isspace((unsigned char)*p))
		p++;

	if (!(isalpha((unsigned char)*p) || *p == '_')) {
		fatal_pp("Expected preprocessor identifier\n");
		exit(1);
	}

	size_t ni = 0;
	while (isalnum((unsigned char)*p) || *p == '_') {
		if (ni + 1 < name_size)
			name[ni++] = *p;
		p++;
	}

	name[ni] = '\0';
}

static void
handle_undef(const char *line)
{
	char name[64] = {0};
	parse_directive_name(line + 6, name, sizeof(name));
	remove_macro(name);
}

static void
skip_expr_ws(const char **expr)
{
	while (isspace((unsigned char)**expr))
		(*expr)++;
}

static int
eval_if_expr(const char *expr);

static void
parse_expr_identifier(const char **expr, char *name, size_t name_size)
{
	size_t ni = 0;

	while (isalnum((unsigned char)**expr) || **expr == '_') {
		if (ni + 1 < name_size)
			name[ni++] = **expr;
		(*expr)++;
	}

	name[ni] = '\0';
}

static int
eval_defined_expr(const char **expr)
{
	const char *p = *expr;
	char name[64];
	size_t ni = 0;
	int paren = 0;

	p += 7; /* defined */

	while (isspace((unsigned char)*p))
		p++;

	if (*p == '(') {
		paren = 1;
		p++;
		while (isspace((unsigned char)*p))
			p++;
	}

	if (!(isalpha((unsigned char)*p) || *p == '_'))
		fatal_pp("Expected identifier after defined");

	memset(name, 0, sizeof(name));

	while (isalnum((unsigned char)*p) || *p == '_') {
		if (ni + 1 < sizeof(name))
			name[ni++] = *p;
		p++;
	}

	name[ni] = '\0';

	while (isspace((unsigned char)*p))
		p++;

	if (paren) {
		if (*p != ')')
			fatal_pp("Expected ')' after defined(identifier)");
		p++;
	}

	*expr = p;
	return is_defined(name);
}

static int
macro_integer_value(const char *name)
{
	if (!name) {
		Debug(0,"macro_integer_value(NULL)\n");
		return 0;
	}

	Macro *macro = find_macro_entry(name);

	if (!macro)
		return 0;

	if (macro->is_function_like)
		return 0;

	const char *p = macro->value;
	while (isspace((unsigned char)*p))
		p++;

	if (*p == '\0')
		return 1;

	return atoi(p);
}

static int
eval_primary_expr(const char **expr)
{
	skip_expr_ws(expr);

	if (**expr == '(') {
		(*expr)++;
		int value = eval_if_expr(*expr);
		int depth = 1;
		while (**expr && depth > 0) {
			if (**expr == '(') depth++;
			else if (**expr == ')') depth--;
			if (depth > 0) (*expr)++;
		}
		if (**expr == ')') (*expr)++;
		return value;
	}

	if (**expr == '~') {
		(*expr)++;
		return ~eval_primary_expr(expr);
	}

	if (strncmp(*expr, "defined", 7) == 0 &&
	        !(isalnum((unsigned char)(*expr)[7]) || (*expr)[7] == '_')) {
		return eval_defined_expr(expr);
	}

	if (isalpha((unsigned char)**expr) || **expr == '_') {
		char name[64] = {0};
		parse_expr_identifier(expr, name, sizeof(name));
		/* Unknown identifiers evaluate to 0 per C standard */
		return macro_integer_value(name);
	}

	int sign = 1;
	if (**expr == '-') {
		sign = -1;
		(*expr)++;
	} else if (**expr == '+') {
		(*expr)++;
	}

	/* Hex literals */
	if ((*expr)[0] == '0' && ((*expr)[1] == 'x' || (*expr)[1] == 'X')) {
		*expr += 2;
		unsigned int uval = 0;
		while (isxdigit((unsigned char)**expr)) {
			char c = **expr;
			int digit = isdigit((unsigned char)c) ? c - '0' :
			            (c >= 'a' ? c - 'a' + 10 : c - 'A' + 10);
			uval = uval * 16 + (unsigned int)digit;
			(*expr)++;
		}
		while (**expr == 'u' || **expr == 'U' || **expr == 'l' || **expr == 'L')
			(*expr)++;
		return sign * (int)uval;
	}

	int value = 0;
	while (isdigit((unsigned char)**expr)) {
		value = value * 10 + (**expr - '0');
		(*expr)++;
	}
	while (**expr == 'u' || **expr == 'U' || **expr == 'l' || **expr == 'L')
		(*expr)++;
	return sign * value;
}

static int
eval_unary_expr(const char **expr)
{
	skip_expr_ws(expr);

	if (**expr == '!') {
		(*expr)++;
		return !eval_unary_expr(expr);
	}

	if (**expr == '-') {
		(*expr)++;
		return -eval_unary_expr(expr);
	}

	if (**expr == '+') {
		(*expr)++;
		return eval_unary_expr(expr);
	}

	return eval_primary_expr(expr);
}

/* mul/div/mod */
static int
eval_mul_expr(const char **expr)
{
	int value = eval_unary_expr(expr);
	for (;;) {
		skip_expr_ws(expr);
		if (**expr == '*' && (*expr)[1] != '*') {
			(*expr)++;
			int rhs = eval_unary_expr(expr);
			value *= rhs;
		} else if (**expr == '/' && (*expr)[1] != '/') {
			(*expr)++;
			int rhs = eval_unary_expr(expr);
			value = rhs ? value / rhs : 0;
		} else if (**expr == '%') {
			(*expr)++;
			int rhs = eval_unary_expr(expr);
			value = rhs ? value % rhs : 0;
		} else {
			return value;
		}
	}
}

/* add/sub */
static int
eval_add_expr(const char **expr)
{
	int value = eval_mul_expr(expr);
	for (;;) {
		skip_expr_ws(expr);
		if (**expr == '+' && (*expr)[1] != '+') {
			(*expr)++;
			value += eval_mul_expr(expr);
		} else if (**expr == '-' && (*expr)[1] != '-') {
			(*expr)++;
			value -= eval_mul_expr(expr);
		} else {
			return value;
		}
	}
}

/* shift */
static int
eval_shift_expr(const char **expr)
{
	int value = eval_add_expr(expr);
	for (;;) {
		skip_expr_ws(expr);
		if ((*expr)[0] == '<' && (*expr)[1] == '<') {
			*expr += 2;
			value <<= eval_add_expr(expr);
		} else if ((*expr)[0] == '>' && (*expr)[1] == '>') {
			*expr += 2;
			value >>= eval_add_expr(expr);
		} else {
			return value;
		}
	}
}

static int
eval_compare_expr(const char **expr)
{
	int value = eval_shift_expr(expr);

	for (;;) {
		skip_expr_ws(expr);

		if ((*expr)[0] == '=' && (*expr)[1] == '=') {
			*expr += 2;
			value = value == eval_shift_expr(expr);
		} else if ((*expr)[0] == '!' && (*expr)[1] == '=') {
			*expr += 2;
			value = value != eval_shift_expr(expr);
		} else if ((*expr)[0] == '<' && (*expr)[1] == '=') {
			*expr += 2;
			value = value <= eval_shift_expr(expr);
		} else if ((*expr)[0] == '>' && (*expr)[1] == '=') {
			*expr += 2;
			value = value >= eval_shift_expr(expr);
		} else if (**expr == '<' && (*expr)[1] != '<') {
			(*expr)++;
			value = value < eval_shift_expr(expr);
		} else if (**expr == '>' && (*expr)[1] != '>') {
			(*expr)++;
			value = value > eval_shift_expr(expr);
		} else {
			return value;
		}
	}
}

static int
eval_and_expr(const char **expr)
{
	int value = eval_compare_expr(expr);
	for (;;) {
		skip_expr_ws(expr);
		if ((*expr)[0] == '&' && (*expr)[1] != '&') {
			(*expr)++;
			value &= eval_compare_expr(expr);
		} else {
			return value;
		}
	}
}

static int eval_xor_expr(const char **expr)
{
	int value = eval_and_expr(expr);
	for (;;) {
		skip_expr_ws(expr);
		if ((*expr)[0] == '^') {
			(*expr)++;
			value ^= eval_and_expr(expr);
		} else {
			return value;
		}
	}
}

static int
eval_bitor_expr(const char **expr)
{
	int value = eval_xor_expr(expr);
	for (;;) {
		skip_expr_ws(expr);
		if ((*expr)[0] == '|' && (*expr)[1] != '|') {
			(*expr)++;
			value |= eval_xor_expr(expr);
		} else {
			return value;
		}
	}
}

static int
eval_logical_and_expr(const char **expr)
{
	int value = eval_bitor_expr(expr);
	for (;;) {
		skip_expr_ws(expr);
		if ((*expr)[0] == '&' && (*expr)[1] == '&') {
			*expr += 2;
			value = value && eval_bitor_expr(expr);
		} else {
			return value;
		}
	}
}

static int
eval_logical_or_expr(const char **p)
{
	int value = eval_logical_and_expr(p);
	for (;;) {
		skip_expr_ws(p);
		if ((*p)[0] == '|' && (*p)[1] == '|') {
			*p += 2;
			value = value || eval_logical_and_expr(p);
		} else {
			return value;
		}
	}
}

static int
eval_if_expr(const char *expr)
{
	const char *p = expr;
	int cond = eval_logical_or_expr(&p);
	skip_expr_ws(&p);
	if (*p == '?') {
		p++; /* consume ? */
		int then_val = eval_logical_or_expr(&p);
		skip_expr_ws(&p);
		if (*p == ':') p++; /* consume : */
		int else_val = eval_logical_or_expr(&p);
		return cond ? then_val : else_val;
	}
	return cond;
}

static void
handle_define(const char *line)
{
	const char *p = line + 7; /* strlen("#define") */

	while (isspace((unsigned char)*p))
		p++;

	if (!(isalpha((unsigned char)*p) || *p == '_')) {
		fatal_pp("Invalid #define name\n");
		exit(1);
	}

	char name[64] = {0};
	int ni = 0;

	while (isalnum((unsigned char)*p) || *p == '_') {
		if (ni < 63)
			name[ni++] = *p;
		p++;
	}

	if (*p == '(') {
		p++;

		char *params = xcalloc(16 * 64, 1);
		int param_count = 0;
		int is_variadic = 0;

		while (*p && *p != ')') {
			while (isspace((unsigned char)*p))
				p++;

			if (*p == ')')
				break;

			if (p[0] == '.' && p[1] == '.' && p[2] == '.') {
				STRNCPY(params + param_count * 64, "__VA_ARGS__", 63 - 1);
				param_count++;
				is_variadic = 1;
				p += 3;

				while (isspace((unsigned char)*p))
					p++;

				if (*p != ')') {
					fatal_pp("Variadic macro parameter must be last\n");
					exit(1);
				}

				break;
			}

			if (!(isalpha((unsigned char)*p) || *p == '_')) {
				fatal_pp("Invalid macro parameter\n");
				exit(1);
			}

			int pi = 0;
			while (isalnum((unsigned char)*p) || *p == '_') {
				if (pi < 63)
					(params + param_count * 64)[pi++] = *p;
				p++;
			}

			param_count++;
			if (param_count >= 16) {
				fatal_pp("Too many macro parameters\n");
				exit(1);
			}

			while (isspace((unsigned char)*p))
				p++;

			if (*p == ',') {
				p++;
				continue;
			}

			if (*p != ')') {
				fatal_pp("Expected ',' or ')' in macro parameter list\n");
				exit(1);
			}
		}

		if (*p != ')') {
			fatal_pp("Unterminated macro parameter list [%s]\n",line);
			exit(1);
		}

		p++;

		while (isspace((unsigned char)*p))
			p++;

		char value[512] = {0};
		STRNCPY(value, p, sizeof(value) - 1);
		Debug(5,"Value [%s]\n",value);
		trim_trailing(value);

		add_function_macro(name, params, param_count, is_variadic, value);
		xfree(params);
		return;
	}

	while (isspace((unsigned char)*p))
		p++;

	char value[512] = {0};
	STRNCPY(value, p, sizeof(value) - 1);
	trim_trailing(value);

	add_object_macro(name, value);
}

static int
file_exists(const char *path)
{
	FILE *fp = fopen(path, "r");
	if (!fp)
		return 0;
	fclose(fp);
	return 1;
}

static char *
read_include_file_search(const char *path, int is_system)
{
	char full[512];

	if (!is_system) {
		if (file_exists(path))
			return read_include_file(path);

		snprintf(full, sizeof(full), "cc/%s", path);
		if (file_exists(full))
			return read_include_file(full);

		snprintf(full, sizeof(full), "codegen/%s", path);
		if (file_exists(full))
			return read_include_file(full);

		snprintf(full, sizeof(full), "cc/codegen/%s", path);
		if (file_exists(full))
			return read_include_file(full);

		return read_include_file(path);
	}

	snprintf(full, sizeof(full), "cc/include/%s", path);
	if (file_exists(full))
		return read_include_file(full);

	snprintf(full, sizeof(full), "include/%s", path);
	if (file_exists(full))
		return read_include_file(full);

	if (tcc_include_dir[0]) {
		int n = snprintf(full, sizeof(full), "%s/%s", tcc_include_dir, path);
		if (n < 0 || (size_t)n >= sizeof(full)) {
    			tcc_error("include path too long: %s/%s", tcc_include_dir, path);
		}
		if (file_exists(full))
			return read_include_file(full);
	}

	/*
	 * Option C: system headers are satisfied by project-local stubs when
	 * available.  If a stub is missing, treat the include as empty rather than
	 * pulling host libc headers that contain far more C than this compiler
	 * supports yet.
	 */
	tcc_warn("ignoring unsupported system include <%s>\n", path);
	char *empty = xmalloc(1);
	empty[0] = '\0';
	return empty;
}

static void
handle_include(const char *line, char **out, size_t *len, size_t *cap, int return_line)
{
	const char *p = line + 8; /* strlen("#include") */

	while (isspace((unsigned char)*p))
		p++;

	int is_system = 0;
	char end_ch = '"';

	if (*p == '"') {
		end_ch = '"';
	} else if (*p == '<') {
		is_system = 1;
		end_ch = '>';
	} else {
		fatal_pp("Expected quoted or system include filename\n");
	}

	p++;

	char path[256] = {0};
	int pi = 0;

	while (*p && *p != end_ch) {
		if (pi < 255)
			path[pi++] = *p;
		p++;
	}

	if (*p != end_ch) {
		fatal_pp("Unterminated include filename\n");
	}

	char *saved_file = xmalloc(512);
	STRNCPY(saved_file, preprocess_file, 511);
	saved_file[511] = '\0';

	char *included_source = read_include_file_search(path, is_system);

	/* preprocess_set_file(path);*/
	char *included_expanded = preprocess(path, included_source);
	preprocess_set_file(saved_file);

	append_str(out, len, cap, included_expanded);
	append_char(out, len, cap, '\n');
	if (preprocess_emit_line_markers)
		append_line_marker(out, len, cap, return_line, saved_file);

	xfree(included_expanded);
	xfree(included_source);
	xfree(saved_file);
}

static int
token_is(PPToken *tok, const char *text)
{
	return tok && STRCMP(tok->text, text) == 0;
}

static int
param_index(Macro *macro, const char *name)
{
	for (int i = 0; i < macro->param_count; i++) {
		if (STRCMP(macro->params[i], name) == 0)
			return i;
	}

	return -1;
}

static void
append_token_range(PPTokenVec *tokens, int start, int end, char *buf, size_t buf_size)
{
	size_t len = strlen(buf);

	for (int i = start; i < end; i++) {
		const char *s = tokens->items[i].text;

		while (*s && len + 1 < buf_size)
			buf[len++] = *s++;
	}

	buf[len] = '\0';
}

static int
macro_arg_token_range_is_empty(PPTokenVec *tokens, int start, int end)
{
	for (int i = start; i < end; i++) {
		if (tokens->items[i].kind != PPTOK_SPACE)
			return 0;
	}

	return 1;
}

static void
trim_macro_arg(char *buf)
{
	/* Trim leading whitespace */
	char *start = buf;
	while (*start == ' ' || *start == '\t') start++;
	if (start != buf)
		memmove(buf, start, strlen(start) + 1);
	/* Trim trailing whitespace */
	size_t len = strlen(buf);
	while (len > 0 && (buf[len-1] == ' ' || buf[len-1] == '\t'))
		buf[--len] = '\0';
}

static int
collect_macro_args(PPTokenVec *tokens, int open_index, Macro *macro,
                   char *args, int max_args, int *close_index)
{
	int depth = 0;
	int arg_start = open_index + 1;
	int arg_count = 0;
	int fixed_count = macro->is_variadic ? macro->param_count - 1 : macro->param_count;

	for (int i = open_index; i < tokens->count; i++) {
		PPToken *tok = &tokens->items[i];

		if (token_is(tok, "(")) {
			depth++;
			continue;
		}

		if (token_is(tok, ")")) {
			depth--;

			if (depth == 0) {
				/*
				 * Zero-parameter function-like macros must treat M() and M( )
				 * as zero arguments, not as one empty argument.  One-parameter
				 * macros still treat F() as one empty argument.
				 */
				if (macro->param_count == 0 &&
				        arg_count == 0 &&
				        macro_arg_token_range_is_empty(tokens, arg_start, i)) {
					*close_index = i;
					return 0;
				}

				if (arg_count < max_args) {
					if (macro->is_variadic && arg_count >= fixed_count) {
						append_token_range(tokens, arg_start, i, args + fixed_count * 512, 512);
						trim_macro_arg(args + fixed_count * 512);
					} else {
						append_token_range(tokens, arg_start, i, args + arg_count * 512, 512);
						trim_macro_arg(args + arg_count * 512);
					}
				}

				/*
				 * For variadic macros with no variadic arguments:
				 *
				 *   #define F(a, ...) ...
				 *   F(x)
				 *
				 * the fixed argument is present, and __VA_ARGS__ is empty.
				 */
				if (macro->is_variadic && arg_count + 1 >= fixed_count)
					arg_count = macro->param_count;
				else
					arg_count++;

				*close_index = i;
				return arg_count;
			}

			continue;
		}

		if (depth == 1 && token_is(tok, ",") &&
		        (!macro->is_variadic || arg_count < fixed_count)) {
			if (arg_count >= max_args) {
				fatal_pp("Too many macro arguments\n");
				exit(1);
			}

			append_token_range(tokens, arg_start, i, args + arg_count * 512, 512);
			trim_macro_arg(args + arg_count * 512);
			arg_count++;
			arg_start = i + 1;
		}
	}

	fatal_pp("Unterminated macro invocation for macro '%s' while parsing argument %d",
	         macro->name, arg_count + 1);
	exit(1);
}

static int
macro_disabled(const char *name, char *disabled, int disabled_count)
{
	if (!name)
		return 0;

	for (int i = 0; i < disabled_count; i++) {
		const char *entry = disabled + i * 64;
		if (*entry && STRCMP(name, entry) == 0)
			return 1;
	}

	return 0;
}

static void
expand_text_recursive(const char *text, char **out, size_t *len, size_t *cap,
                      int depth, char *disabled, int disabled_count);

static int
macro_arg_is_empty(const char *src)
{
	while (*src) {
		if (!isspace((unsigned char)*src))
			return 0;
		src++;
	}

	return 1;
}

static void
trim_macro_arg_text(const char *src, char *dst, size_t dst_size)
{
	while (isspace((unsigned char)*src))
		src++;

	size_t len = strlen(src);
	while (len > 0 && isspace((unsigned char)src[len - 1]))
		len--;

	if (len >= dst_size)
		len = dst_size - 1;

	memcpy(dst, src, len);
	dst[len] = '\0';
}

static void
append_macro_replacement_token(Macro *macro, PPToken *tok, char *expanded_arg_flat,
                               char *pieces, int *piece_count)
{
	if (*piece_count >= 256) {
		fatal_pp("Macro replacement too large\n");
	}

	if (tok->kind == PPTOK_IDENT) {
		int pi = param_index(macro, tok->text);
		if (pi >= 0) {
			STRNCPY((pieces + (*piece_count) * 512), (expanded_arg_flat + (pi) * 512), 511);
			(pieces + (*piece_count) * 512)[511] = '\0';
			(*piece_count)++;
			return;
		}
	}

	STRNCPY((pieces + (*piece_count) * 512), tok->text, 511);
	(pieces + (*piece_count) * 512)[511] = '\0';
	(*piece_count)++;
}

static void
stringify_macro_arg(const char *src, char *dst, size_t dst_size)
{
	size_t di = 0;

	if (di + 1 < dst_size)
		dst[di++] = '"';

	while (isspace((unsigned char)*src))
		src++;

	size_t len = strlen(src);
	while (len > 0 && isspace((unsigned char)src[len - 1]))
		len--;

	for (size_t i = 0; i < len && di + 2 < dst_size; i++) {
		unsigned char ch = (unsigned char)src[i];

		if (ch == '\\' || ch == '"') {
			if (di + 2 >= dst_size)
				break;
			dst[di++] = '\\';
			dst[di++] = (char)ch;
		} else if (ch == '\n') {
			if (di + 2 >= dst_size)
				break;
			dst[di++] = '\\';
			dst[di++] = 'n';
		} else if (ch == '\t') {
			if (di + 2 >= dst_size)
				break;
			dst[di++] = '\\';
			dst[di++] = 't';
		} else {
			dst[di++] = (char)ch;
		}
	}

	if (di + 1 < dst_size)
		dst[di++] = '"';

	dst[di] = '\0';
}

static void
append_function_macro_expansion(Macro *macro, char *args,
                                char **out, size_t *len, size_t *cap,
                                int depth, char *disabled, int disabled_count)
{
	PPTokenVec replacement;
	replacement.items = 0;
	replacement.count = 0;
	replacement.cap = 0;
	pp_tokenize_line(macro->value, &replacement);

	char *expanded_arg_text = xcalloc(16 * 512, 1);

	for (int i = 0; i < macro->param_count; i++) {
		size_t arg_cap = 1024;
		size_t arg_len = 0;
		char *arg_out = xcalloc(1, arg_cap);

		expand_text_recursive((args + (i) * 512), &arg_out, &arg_len, &arg_cap, depth + 1, disabled, disabled_count);

		STRNCPY((expanded_arg_text + (i) * 512), arg_out, 511);
		xfree(arg_out);
	}

	/*
	 * v72 token pasting:
	 *
	 * Build a small replacement-piece list first. When we see ##, concatenate
	 * the previous replacement piece with the next replacement piece, then
	 * emit the concatenated spelling. Later recursive rescanning will tokenize
	 * and expand that result if it names another macro.
	 *
	 * This is intentionally still a simplified CPP model, but it gets the
	 * important behavior right for identifiers and numbers:
	 *
	 *   #define CAT(a,b) a ## b
	 *   CAT(foo, bar) -> foobar
	 */
	char *pieces = xcalloc(256 * 512, 1);
	int piece_count = 0;

	for (int i = 0; i < replacement.count; i++) {
		PPToken *tok = &replacement.items[i];

		if (tok->kind == PPTOK_SPACE) {
			/* Preserve spaces in replacement list so "unsigned long" stays as two words */
			if (piece_count > 0) {
				/* Append space to previous piece rather than making a new piece,
				   to avoid ## treating a space-piece as a separator */
				char *prev = pieces + (piece_count - 1) * 512;
				size_t plen = strlen(prev);
				if (plen + 1 < 511)
					prev[plen] = ' ';
			}
			continue;
		}

		if (token_is(tok, "#")) {
			i++;
			while (i < replacement.count && replacement.items[i].kind == PPTOK_SPACE)
				i++;

			if (i >= replacement.count || replacement.items[i].kind != PPTOK_IDENT) {
				fatal_pp("# in macro replacement must be followed by a parameter\n");
				exit(1);
			}

			int pi = param_index(macro, replacement.items[i].text);
			if (pi < 0) {
				fatal_pp("# in macro replacement must be followed by a parameter\n");
				exit(1);
			}

			if (piece_count >= 256) {
				fatal_pp("Macro replacement too large\n");
				exit(1);
			}

			stringify_macro_arg((args + (pi) * 512), (pieces + (piece_count) * 512), 512);
			piece_count++;
			continue;
		}

		if (token_is(tok, "##")) {
			if (piece_count == 0) {
				fatal_pp("## cannot appear at start of macro replacement\n");
				exit(1);
			}

			i++;
			while (i < replacement.count && replacement.items[i].kind == PPTOK_SPACE)
				i++;

			if (i >= replacement.count) {
				fatal_pp("## cannot appear at end of macro replacement\n");
				exit(1);
			}

			/*
			 * v77: GNU-style comma swallowing for variadic macros.
			 *
			 *   #define LOG(fmt, ...) printf(fmt, ##__VA_ARGS__)
			 *
			 * If __VA_ARGS__ is empty, remove the previous comma instead of
			 * producing "printf(fmt,)".
			 */
			if (macro->is_variadic &&
			        replacement.items[i].kind == PPTOK_IDENT &&
			        STRCMP(replacement.items[i].text, "__VA_ARGS__") == 0 &&
			        piece_count > 0 &&
			        macro_arg_is_empty((expanded_arg_text + (macro->param_count - 1) * 512))) {
				/* GNU comma swallowing: if prev piece is "," (possibly with
				   trailing space from space-preservation), remove it */
				char *prev_piece = pieces + (piece_count - 1) * 512;
				size_t plen = strlen(prev_piece);
				/* strip trailing spaces to check for bare comma */
				while (plen > 0 && prev_piece[plen - 1] == ' ') plen--;
				if (plen == 1 && prev_piece[0] == ',') {
					piece_count--;
					continue;
				}
			}

			char rhs[512] = {0};

			if (replacement.items[i].kind == PPTOK_IDENT) {
				int pi = param_index(macro, replacement.items[i].text);
				if (pi >= 0)
					/* Use expanded argument text for ## RHS (consistent with LHS) */
					trim_macro_arg_text((expanded_arg_text + (pi) * 512), rhs, sizeof(rhs));
				else
					STRNCPY(rhs, replacement.items[i].text, sizeof(rhs) - 1);
			} else {
				STRNCPY(rhs, replacement.items[i].text, sizeof(rhs) - 1);
			}

			/* Trim trailing space from LHS before pasting (space may have been
			   appended by the space-preservation logic above) */
			char *lhs = pieces + (piece_count - 1) * 512;
			size_t lhs_len = strlen(lhs);
			while (lhs_len > 0 && lhs[lhs_len - 1] == ' ')
				lhs[--lhs_len] = '\0';

			strncat((pieces + (piece_count - 1) * 512), rhs,
			        511 - strlen((pieces + (piece_count - 1) * 512)));
			continue;
		}

		append_macro_replacement_token(macro, tok, expanded_arg_text, pieces, &piece_count);
	}

	for (int i = 0; i < piece_count; i++) {
		char *piece = pieces + i * 512;

		/*
		 * Replacement-list tokens that were not joined by ## must remain
		 * separate preprocessing tokens after macro substitution.  Emitting
		 * pieces back-to-back can accidentally create a different token; for
		 * example:
		 *
		 *   #define Q(A,B) A ## B+
		 *   Q(+,)3
		 *
		 * The paste produces a single '+' token from A##B.  The following
		 * replacement-list '+' is a separate token, so the result is '+ +3',
		 * not '++3'.  Insert a separator between replacement pieces unless
		 * the previous piece already ended with whitespace.  Explicit ## still
		 * works because pasted tokens are stored in the same piece.
		 */
		if (i > 0 && *piece) {
			char *prev = pieces + (i - 1) * 512;
			size_t plen = strlen(prev);
			if (plen > 0 && !isspace((unsigned char)prev[plen - 1]))
				append_str(out, len, cap, " ");
		}

		append_str(out, len, cap, piece);
	}

	pptok_free(&replacement);
}

static void
expand_tokens_recursive(PPTokenVec *tokens, char **out, size_t *len, size_t *cap,
                        int depth, char *disabled, int disabled_count)
{
	if (depth > 100) {
		fatal_pp("Macro expansion depth exceeded\n");
	}

	for (int i = 0; i < tokens->count; i++) {
		PPToken *tok = &tokens->items[i];

		if (tok->kind == PPTOK_IDENT) {
			Macro *macro = find_macro_entry(tok->text);

			if (!macro) {
				append_str(out, len, cap, tok->text);
				continue;
			}
			if (!macro->name || !macro->value) {
				append_str(out, len, cap, tok->text);
				continue;
			}

			if (macro_disabled(macro->name, disabled, disabled_count)) {
				append_str(out, len, cap, tok->text);
				continue;
			}

			if (!macro->is_function_like) {
				char *local_disabled = xcalloc(64 * 64, 1);
				int local_disabled_count = disabled_count;

				for (int d = 0; d < disabled_count && d < 64; d++)
					STRNCPY(local_disabled + d * 64, disabled + d * 64, 63);

				if (local_disabled_count < 64)
					STRNCPY(local_disabled + local_disabled_count++ * 64, macro->name, 63);

				expand_text_recursive(macro->value, out, len, cap, depth + 1, local_disabled, local_disabled_count);
				xfree(local_disabled);
				continue;
			}

			int j = i + 1;
			while (j < tokens->count && tokens->items[j].kind == PPTOK_SPACE)
				j++;

			if (j >= tokens->count || !token_is(&tokens->items[j], "(")) {
				append_str(out, len, cap, tok->text);
				continue;
			}

			char *args = xcalloc(16 * 512, 1);
			int close_index = 0;
			int arg_count = collect_macro_args(tokens, j, macro, args, 16, &close_index);

			if (arg_count != macro->param_count) {
				tcc_warn("Macro argument count [%d,%d] mismatch for %s\n", arg_count,macro->param_count,macro->name);
				xfree(args);
				exit(1);
			}

			size_t tmp_cap = 1024;
			size_t tmp_len = 0;
			char *tmp = xcalloc(1, tmp_cap);

			append_function_macro_expansion(macro, args, &tmp, &tmp_len, &tmp_cap, depth + 1, disabled, disabled_count);
			xfree(args);

			char *local_disabled = xcalloc(64 * 64, 1);
			int local_disabled_count = disabled_count;

			for (int d = 0; d < disabled_count && d < 64; d++)
				STRNCPY(local_disabled + d * 64, disabled + d * 64, 63);

			if (local_disabled_count < 64)
				STRNCPY(local_disabled + local_disabled_count++ * 64, macro->name, 63);

			/*
			 * Rescan the replacement first.  Token pasting can create the name
			 * of a function-like macro which uses the next tokens from the
			 * original input as its argument list:
			 *
			 *   CAT(A,B)(x)  ->  AB(x)  ->  CAT(x,y)  ->  xy
			 *
			 * If we rescan only CAT's replacement and then append the original
			 * suffix later, AB is seen without its following '(' and is not
			 * expanded.  Therefore, after rescanning the replacement to a small
			 * intermediate string, check whether that string ends in a
			 * function-like macro name immediately followed by an original '('.
			 * In that case, rescan the intermediate text plus just that one
			 * invocation suffix.
			 */
			size_t mid_cap = 1024;
			size_t mid_len = 0;
			char *mid = xcalloc(1, mid_cap);
			expand_text_recursive(tmp, &mid, &mid_len, &mid_cap, depth + 1,
			                      local_disabled, local_disabled_count);

			int consumed_suffix_to = close_index;
			int j2 = close_index + 1;
			while (j2 < tokens->count && tokens->items[j2].kind == PPTOK_SPACE)
				j2++;

			if (j2 < tokens->count && token_is(&tokens->items[j2], "(")) {
				PPTokenVec mid_tokens;
				mid_tokens.items = 0;
				mid_tokens.count = 0;
				mid_tokens.cap = 0;
				pp_tokenize_line(mid, &mid_tokens);

				int last = mid_tokens.count - 1;
				while (last >= 0 && mid_tokens.items[last].kind == PPTOK_SPACE)
					last--;

				if (last >= 0 && mid_tokens.items[last].kind == PPTOK_IDENT) {
					Macro *tail_macro = find_macro_entry(mid_tokens.items[last].text);
					if (tail_macro && tail_macro->is_function_like &&
					        !macro_disabled(tail_macro->name, disabled, disabled_count)) {
						int paren_depth = 0;
						int k2 = j2;
						for (; k2 < tokens->count; k2++) {
							if (token_is(&tokens->items[k2], "("))
								paren_depth++;
							else if (token_is(&tokens->items[k2], ")")) {
								paren_depth--;
								if (paren_depth == 0)
									break;
							}
						}

						if (k2 < tokens->count && paren_depth == 0) {
							size_t combo_cap = strlen(mid) + 1024;
							size_t combo_len = 0;
							char *combo = xcalloc(1, combo_cap);
							append_str(&combo, &combo_len, &combo_cap, mid);
							for (int q = close_index + 1; q <= k2; q++)
								append_str(&combo, &combo_len, &combo_cap, tokens->items[q].text);

							expand_text_recursive(combo, out, len, cap, depth + 1,
							                      disabled, disabled_count);
							consumed_suffix_to = k2;
							xfree(combo);
						}
					}
				}

				pptok_free(&mid_tokens);
			}

			if (consumed_suffix_to == close_index)
				append_str(out, len, cap, mid);

			xfree(mid);
			xfree(local_disabled);
			xfree(tmp);

			i = consumed_suffix_to;
			continue;
		}

		append_str(out, len, cap, tok->text);
	}
}

static void
expand_text_recursive(const char *text, char **out, size_t *len, size_t *cap,
                      int depth, char *disabled, int disabled_count)
{
	PPTokenVec tokens;
	tokens.items = 0;
	tokens.count = 0;
	tokens.cap = 0;
	pp_tokenize_line(text, &tokens);
	expand_tokens_recursive(&tokens, out, len, cap, depth, disabled, disabled_count);
	pptok_free(&tokens);
}

static void
expand_line(const char *line, char **out, size_t *len, size_t *cap)
{
	char *disabled_buf = xcalloc(64 * 64, 1);
	expand_text_recursive(line, out, len, cap, 0, disabled_buf, 0);
	xfree(disabled_buf);
}

static int
line_has_open_function_macro_invocation(const char *line)
{
	PPTokenVec tokens;
	tokens.items = 0;
	tokens.count = 0;
	tokens.cap = 0;
	pp_tokenize_line(line, &tokens);
	int result = 0;

	for (int i = 0; i < tokens.count; i++) {
		PPToken *tok = &tokens.items[i];

		if (tok->kind != PPTOK_IDENT)
			continue;

		Macro *macro = find_macro_entry(tok->text);
		if (!macro || !macro->is_function_like)
			continue;

		int j = i + 1;
		while (j < tokens.count && tokens.items[j].kind == PPTOK_SPACE)
			j++;

		if (j >= tokens.count || !token_is(&tokens.items[j], "("))
			continue;

		int depth = 0;
		for (int k = j; k < tokens.count; k++) {
			if (token_is(&tokens.items[k], "("))
				depth++;
			else if (token_is(&tokens.items[k], ")")) {
				depth--;
				if (depth == 0)
					break;
			}
		}

		if (depth > 0) {
			result = 1;
			break;
		}
	}

	pptok_free(&tokens);
	return result;
}

static void
append_physical_line(char **line, const char *next_line, size_t next_len)
{
	size_t old_len = strlen(*line);
	char *new_line = xrealloc(*line, old_len + next_len + 2);

	*line = new_line;
	(*line)[old_len++] = ' ';
	memcpy(*line + old_len, next_line, next_len);
	(*line)[old_len + next_len] = '\0';
}

static char *
join_line_continuations(const char *input)
{
	size_t in_len = strlen(input);
	size_t cap = in_len + 1;
	if (cap < 1024)
		cap = 1024;

	char *out = xcalloc(1, cap);
	size_t len = 0;

	for (size_t i = 0; input[i]; i++) {
		/*
		 * C translation phase 2 removes backslash-newline pairs before
		 * preprocessing directives are interpreted.  This lets us support:
		 *
		 *   #define F(x) \
		 *       ((x) + 1)
		 *
		 * Keep one separating space so tokens from adjacent physical lines do
		 * not accidentally merge unless the macro explicitly uses ##.
		 */
		if (input[i] == '\\' && input[i + 1] == '\n') {
			if (len + 2 >= cap) {
				cap *= 2;
				char *new_out = xrealloc(out, cap);
				out = new_out;
			}

			out[len++] = ' ';
			out[len] = '\0';
			i++;
			continue;
		}

		if (input[i] == '\\' && input[i + 1] == '\r' && input[i + 2] == '\n') {
			if (len + 2 >= cap) {
				cap *= 2;
				char *new_out = xrealloc(out, cap);
				out = new_out;
			}

			out[len++] = ' ';
			out[len] = '\0';
			i += 2;
			continue;
		}

		if (len + 2 >= cap) {
			cap *= 2;
			char *new_out = xrealloc(out, cap);
			out = new_out;
		}

		out[len++] = input[i];
		out[len] = '\0';
	}

	return out;
}

static void
pp_append_char(char **out, size_t *len, size_t *cap, char ch)
{
	if (*len + 2 >= *cap) {
		*cap *= 2;
		char *new_out = xrealloc(*out, *cap);
		*out = new_out;
	}

	(*out)[(*len)++] = ch;
	(*out)[*len] = '\0';
}

static char *
strip_comments(const char *input)
{
	size_t in_len = strlen(input);
	size_t cap = in_len + 1;
	if (cap < 1024)
		cap = 1024;

	char *out = xcalloc(1, cap);
	size_t len = 0;
	int in_string = 0;
	int in_char = 0;

	for (size_t i = 0; input[i]; i++) {
		char ch = input[i];

		if (in_string) {
			pp_append_char(&out, &len, &cap, ch);

			if (ch == '\\' && input[i + 1]) {
				pp_append_char(&out, &len, &cap, input[++i]);
				continue;
			}

			if (ch == '"')
				in_string = 0;

			continue;
		} else if (in_char) {
			pp_append_char(&out, &len, &cap, ch);

			if (ch == '\\' && input[i + 1]) {
				pp_append_char(&out, &len, &cap, input[++i]);
				continue;
			}

			if (ch == '\'')
				in_char = 0;

			continue;
		} else if (ch == '"') {
			in_string = 1;
			pp_append_char(&out, &len, &cap, ch);
			continue;
		} else if (ch == '\'') {
			in_char = 1;
			pp_append_char(&out, &len, &cap, ch);
			continue;
		} else if (ch == '/' && input[i + 1] == '/') {
			pp_append_char(&out, &len, &cap, ' ');

			i += 2;
			while (input[i] && input[i] != '\n')
				i++;

			if (input[i] == '\n')
				pp_append_char(&out, &len, &cap, '\n');
			else
				i--;

			continue;
		} else if (ch == '/' && input[i + 1] == '*') {
			pp_append_char(&out, &len, &cap, ' ');

			i += 2;
			int closed = 0;

			while (input[i]) {
				if (input[i] == '*' && input[i + 1] == '/') {
					i++;
					closed = 1;
					break;
				}

				if (input[i] == '\n')
					pp_append_char(&out, &len, &cap, '\n');

				i++;
			}

			if (!closed) {
				fatal_pp("Unterminated block comment\n");
				exit(1);
			}

			continue;
		}

		pp_append_char(&out, &len, &cap, ch);
	}

	return out;
}

char *
preprocess(const char *filename, const char *input)
{
	preprocess_set_file(filename);

	pp_line=0;

	init_builtin_macros();

	/*
	 * Macro definitions intentionally persist across recursive preprocess()
	 * calls. This lets #include behave like textual inclusion: macros defined
	 * in included files are visible to later lines in the including file.
	 *
	 * Before directive processing, perform translation phase 2-style
	 * backslash-newline splicing so directives and macro replacement lists can
	 * span physical source lines.
	 */
	char *joined_input = join_line_continuations(input);
	char *comment_stripped_input = strip_comments(joined_input);
	input = comment_stripped_input;

	IfState *if_stack = xcalloc(64, sizeof(IfState));
	int if_depth = 0;

	size_t cap = strlen(input) * 2 + 1024;
	if (cap < 1024)
		cap = 1024;

	char *out = xcalloc(1, cap);
	size_t len = 0;

	if (preprocess_emit_line_markers) {
		append_line_marker(&out, &len, &cap, 1, preprocess_file);
	}

	const char *p = input;
	int current_line = 1;
	preprocess_current_line = current_line;
	while (*p) {
		preprocess_current_line = current_line;
		const char *line_start = p;

		while (*p && *p != '\n')
			p++;

		size_t line_len = (size_t)(p - line_start);
		char *line = xcalloc(1, line_len + 1);

		memcpy(line, line_start, line_len);
		pp_line=line;
		Debug(5,"%d start=%p p=%p [%x] [%s]\n",current_line, line_start,(void *)p,0,"");

		const char *q = line;
		while (isspace((unsigned char)*q))
			q++;

		int skipping = if_depth > 0 && if_stack[if_depth - 1].skipping;

		if (strncmp(q, "#ifndef", 7) == 0 && isspace((unsigned char)q[7])) {
			if (if_depth >= 64) {
				fatal_pp("Too many nested preprocessor conditionals\n");
				exit(1);
			}

			char name[64] = {0};
			parse_directive_name(q + 7, name, sizeof(name));

			int parent_skipping = if_depth > 0 && if_stack[if_depth - 1].skipping;
			int taken = !is_defined(name);

			if_stack[if_depth].parent_skipping = parent_skipping;
			if_stack[if_depth].taken = taken;
			if_stack[if_depth].skipping = parent_skipping || !taken;
			if_depth++;
		} else if (strncmp(q, "#ifdef", 6) == 0 && isspace((unsigned char)q[6])) {
			if (if_depth >= 64) {
				fatal_pp("Too many nested preprocessor conditionals\n");
				exit(1);
			}

			char name[64] = {0};
			parse_directive_name(q + 6, name, sizeof(name));

			int parent_skipping = if_depth > 0 && if_stack[if_depth - 1].skipping;
			int taken = is_defined(name);

			if_stack[if_depth].parent_skipping = parent_skipping;
			if_stack[if_depth].taken = taken;
			if_stack[if_depth].skipping = parent_skipping || !taken;
			if_depth++;
		} else if (strncmp(q, "#if", 3) == 0 && isspace((unsigned char)q[3])) {
			if (if_depth >= 64) {
				fatal_pp("Too many nested preprocessor conditionals\n");
				exit(1);
			}

			const char *expr = q + 3;
			while (isspace((unsigned char)*expr))
				expr++;

			/*
			 * v60 supports only simple numeric #if conditions. Any non-zero
			 * integer is true.
			 */
			int value = eval_if_expr(expr);
			int parent_skipping = if_depth > 0 && if_stack[if_depth - 1].skipping;
			int taken = value != 0;

			if_stack[if_depth].parent_skipping = parent_skipping;
			if_stack[if_depth].taken = taken;
			if_stack[if_depth].skipping = parent_skipping || !taken;
			if_depth++;
		} else if (strncmp(q, "#elif", 5) == 0 && isspace((unsigned char)q[5])) {
			if (if_depth == 0) {
				fatal_pp("#elif without matching #if\n");
				exit(1);
			}

			IfState *state = &if_stack[if_depth - 1];

			if (state->taken || state->parent_skipping) {
				state->skipping = 1;
			} else {
				const char *expr = q + 5;
				while (isspace((unsigned char)*expr))
					expr++;

				int value = eval_if_expr(expr);

				if (value) {
					state->taken = 1;
					state->skipping = 0;
				} else {
					state->skipping = 1;
				}
			}
		} else if (strncmp(q, "#else", 5) == 0 &&
		           (q[5] == '\0' || isspace((unsigned char)q[5]))) {
			if (if_depth == 0) {
				fatal_pp("#else without matching #if\n");
				exit(1);
			}

			IfState *state = &if_stack[if_depth - 1];

			if (state->taken || state->parent_skipping) {
				state->skipping = 1;
			} else {
				state->taken = 1;
				state->skipping = 0;
				/* Re-sync line numbers: we just transitioned from skipping to emitting */
				if (preprocess_emit_line_markers) {
					int next_line = current_line + 1;
					append_line_marker(&out, &len, &cap, next_line, preprocess_file);
				}
			}
		} else if (strncmp(q, "#endif", 6) == 0 &&
		           (q[6] == '\0' || isspace((unsigned char)q[6]))) {
			if (if_depth == 0) {
				fatal_pp("#endif without matching #if\n");
				exit(1);
			}

			if_depth--;
			/* Re-sync line numbers after conditional block.
			 * Skipped lines were counted but not emitted, so emit a #line
			 * directive so the parser sees correct locations. */
			if (preprocess_emit_line_markers) {
				int next_line = current_line + 1;
				append_line_marker(&out, &len, &cap, next_line, preprocess_file);
			}
		} else if (skipping) {
			Debug(1, "PP SKIP %d [%s]: %s\n", current_line, preprocess_file ? preprocess_file : "<?>", line);
			/* ignore skipped line */
		} else if (strncmp(q, "#define", 7) == 0 && isspace((unsigned char)q[7])) {
			handle_define(q);
		} else if (strncmp(q, "#undef", 6) == 0 && isspace((unsigned char)q[6])) {
			handle_undef(q);
		} else if (strncmp(q, "#include", 8) == 0 && isspace((unsigned char)q[8])) {
			handle_include(q, &out, &len, &cap, current_line + 1);
		} else if (strncmp(q, "#error", 6) == 0 &&
		           (q[6] == '\0' || isspace((unsigned char)q[6]))) {
			fatal_pp("#error%s\n", q + 6);
			exit(1);
		} else if (strncmp(q, "#warning", 8) == 0 &&
		           (q[8] == '\0' || isspace((unsigned char)q[8]))) {
			fatal_pp("warning:%s\n", q + 8);
		} else if (strncmp(q, "#pragma", 7) == 0 &&
		           (q[7] == '\0' || isspace((unsigned char)q[7]))) {
			/* Handle #pragma pack(N) - emit sentinel for parser */
			const char *p2 = q + 7;
			while (isspace((unsigned char)*p2)) p2++;
			if (strncmp(p2, "pack", 4) == 0 && p2[4] == '(') {
				p2 += 5; /* skip "pack(" */
				while (isspace((unsigned char)*p2)) p2++;
				/* Handle pack(push, N) */
				if (strncmp(p2, "push", 4) == 0) {
					p2 += 4;
					while (isspace((unsigned char)*p2)) p2++;
					if (*p2 == ',') p2++;
					while (isspace((unsigned char)*p2)) p2++;
				}
				/* Handle pack(pop) */
				if (strncmp(p2, "pop", 3) == 0) {
					append_str(&out, &len, &cap, "__pragma_pack__(0)\n");
				} else if (isdigit((unsigned char)*p2)) {
					int n = 0;
					while (isdigit((unsigned char)*p2))
						n = n * 10 + (*p2++ - '0');
					char sentinel[64];
					snprintf(sentinel, sizeof(sentinel), "__pragma_pack__(%d)\n", n);
					append_str(&out, &len, &cap, sentinel);
				} else {
					/* pack() with no arg = reset */
					append_str(&out, &len, &cap, "__pragma_pack__(0)\n");
				}
			}
			/* other pragmas silently ignored */
		} else if (strncmp(q, "#line", 5) == 0 &&
		           (q[5] == '\0' || isspace((unsigned char)q[5]))) {
			/*
			 * #line linenum ["filename"]
			 *
			 * The tokens after #line are macro-expanded before the line number
			 * is parsed. This is required for cases such as:
			 *
			 *   #define line 1000
			 *   #line line
			 */
			const char *p2 = q + 5;
			while (isspace((unsigned char)*p2))
				p2++;

			size_t expanded_cap = strlen(p2) + 64;
			size_t expanded_len = 0;
			char *expanded = xcalloc(1, expanded_cap);
			char *disabled_buf = xcalloc(64 * 64, 1);

			expand_text_recursive(p2, &expanded, &expanded_len, &expanded_cap,
			                      0, disabled_buf, 0);
			xfree(disabled_buf);

			p2 = expanded;
			while (isspace((unsigned char)*p2))
				p2++;

			if (isdigit((unsigned char)*p2)) {
				int lineno = 0;
				while (isdigit((unsigned char)*p2))
					lineno = lineno * 10 + (*p2++ - '0');

				/* #line sets the line number for the NEXT line, so current_line
				 * will be incremented at end of loop — set to lineno-1 */
				current_line = lineno - 1;
				char line_num[32];
				snprintf(line_num, sizeof(line_num), "%d", lineno);
				add_object_macro("__LINE__", line_num);

				/* optional filename */
				while (isspace((unsigned char)*p2))
					p2++;
				if (*p2 == '"') {
					p2++;
					char fname[256];
					int fi = 0;
					while (*p2 && *p2 != '"' && fi < 254)
						fname[fi++] = *p2++;
					fname[fi] = '\0';
					char fbuf[270];
					snprintf(fbuf, sizeof(fbuf), "\"%s\"", fname);
					add_object_macro("__FILE__", fbuf);
				}
			}

			xfree(expanded);
		} else if (*q == '#' && q[1] == '\0') {
			/* empty # directive — ignore */
		} else {
			Debug(5,"Update __LINE__\n");
			/* Update __LINE__ for the first source line in this logical line. */
			{
				char line_num[32];
				snprintf(line_num, sizeof(line_num), "%d", current_line);
				add_object_macro("__LINE__", line_num);
			}

			/*
			 * Function-like macro invocations may span physical source lines:
			 *
			 *     STRNCPY(a, b,
			 *             sizeof(a) - 1);
			 *
			 * collect_macro_args() operates on one token vector, so build a
			 * logical line containing following physical lines until the macro
			 * invocation's parenthesis depth closes.
			 */
			while (line_has_open_function_macro_invocation(line) && *p == '\n') {
				p++;
				current_line++;
				preprocess_current_line = current_line;

				const char *next_start = p;
				while (*p && *p != '\n')
					p++;

				size_t next_len = (size_t)(p - next_start);
				append_physical_line(&line, next_start, next_len);
				pp_line = line;
			}

			Debug(1, "PP line %d [%s]: %s\n", current_line, preprocess_file ? preprocess_file : "<?>", line);
			expand_line(line, &out, &len, &cap);
			append_char(&out, &len, &cap, '\n');
		}

		xfree(line);

		if (*p == '\n')
			p++;

		current_line++;
		preprocess_current_line = current_line;
	}

	if (if_depth != 0) {
		fatal_pp("Unterminated preprocessor conditional\n");
		exit(1);
	}

	xfree(comment_stripped_input);
	xfree(joined_input);
	return out;
}
