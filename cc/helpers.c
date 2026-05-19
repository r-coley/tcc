#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>

#include "tcc.h"
#include "ast.h"
#include "lexer.h"
#include "preprocess.h"

extern int preprocess_current_line;

int	Debug_lvl=0;

void __attribute__((noreturn))
fatal_lex(const char *file, int line, int col, const char *fmt, ...)
{
	va_list ap;
	if (col > 0)
		fprintf(stderr, "%s:%d:%d: error: ", file, line, col);
	else
		fprintf(stderr, "%s:%d: error: ", file, line);
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	fprintf(stderr, "\n");
	exit(1);
}

void __attribute__((noreturn))
fatal_pp(const char *fmt, ...)
{
	va_list ap;
	fprintf(stderr, "%s:%d: error: ",
	        preprocess_get_file(),
	        preprocess_current_line);
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	fprintf(stderr, "\n");
	exit(1);
}

void
tcc_warn(const char *fmt, ...)
{
	va_list ap;
	fprintf(stderr, "warning: ");
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
}

void
tcc_error(const char *fmt, ...)
{
	va_list ap;
	fprintf(stderr, "error: ");
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	exit(1);
}

void
fatal_ice(const char *file, int line, const char *fmt, ...)
{
	va_list ap;
	fprintf(stderr, "%s:%d: internal compiler error: ", file, line);
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	fprintf(stderr, "\n");
	fprintf(stderr, "Please report this bug.\n");
	exit(1);
}

#define TCC_ALLOC_MAGIC 0x54434341UL
#define TCC_ALLOC_FREED 0x54434652UL
#define TCC_ALLOC_MAX 1073741824UL

typedef struct AllocHeader {
	unsigned long magic;
	size_t size;
} AllocHeader;

static AllocHeader *
alloc_header_from_user(void *ptr)
{
	if (!ptr)
		return NULL;
	return ((AllocHeader *)ptr) - 1;
}

static void
alloc_fail_overflow(const char *kind, size_t count, size_t size,
                    const char *file, int line, const char *func)
{
	fprintf(stderr,
	        "%s:%d: fatal: allocation overflow in %s: %s(count=%lu, size=%lu)\n",
	        file ? file : "<unknown>",
	        line,
	        func ? func : "<unknown>",
	        kind ? kind : "alloc",
	        (unsigned long)count,
	        (unsigned long)size);
	exit(1);
}

static void
alloc_fail_oom(const char *kind,
               size_t count,
               size_t size,
               size_t payload,
               size_t total,
               const char *file,
               int line,
               const char *func)
{
	fprintf(stderr,
	        "%s:%d: fatal: out of memory in %s: %s(count=%lu, size=%lu, payload=%lu, total=%lu)\n",
	        file ? file : "<unknown>",
	        line,
	        func ? func : "<unknown>",
	        kind ? kind : "alloc",
	        (unsigned long)count,
	        (unsigned long)size,
	        (unsigned long)payload,
	        (unsigned long)total);
	exit(1);
}

void *
xcalloc_impl(size_t count, size_t size, const char *file, int line, const char *func)
{
	AllocHeader *h;
	void *user;
	size_t payload;
	size_t total;

	if (count == 0)
		count = 1;
	if (size == 0)
		size = 1;

	if (count > TCC_ALLOC_MAX)
		alloc_fail_overflow("calloc", count, size, file, line, func);
	if (size > TCC_ALLOC_MAX)
		alloc_fail_overflow("calloc", count, size, file, line, func);
	if (count != 0 && size > TCC_ALLOC_MAX / count)
		alloc_fail_overflow("calloc", count, size, file, line, func);

	payload = count * size;
	if (payload > TCC_ALLOC_MAX - sizeof(AllocHeader))
		alloc_fail_overflow("calloc+header", payload, sizeof(AllocHeader), file, line, func);

	total = sizeof(AllocHeader) + payload;
	h = calloc(1, total);
	if (!h)
		alloc_fail_oom("calloc", count, size, payload, total, file, line, func);

	h->magic = TCC_ALLOC_MAGIC;
	h->size = payload;
	user = (void *)(h + 1);

	Debug(2, "ALLOC calloc %s:%d %s count=%lu size=%lu payload=%lu total=%lu -> %p\n",
	      file ? file : "<unknown>",
	      line,
	      func ? func : "<unknown>",
	      (unsigned long)count,
	      (unsigned long)size,
	      (unsigned long)payload,
	      (unsigned long)total,
	      user);

	return user;
}

void *
xmalloc_impl(size_t size, const char *file, int line, const char *func)
{
	return xcalloc_impl(1, size, file, line, func);
}

void *
xrealloc_impl(void *ptr, size_t size, const char *file, int line, const char *func)
{
	AllocHeader *old_h;
	AllocHeader *new_h;
	void *user;
	size_t old_size;
	size_t total;

	if (size == 0)
		size = 1;
	if (size > TCC_ALLOC_MAX)
		alloc_fail_overflow("realloc", 1, size, file, line, func);
	if (size > TCC_ALLOC_MAX - sizeof(AllocHeader))
		alloc_fail_overflow("realloc+header", size, sizeof(AllocHeader), file, line, func);

	if (!ptr)
		return xcalloc_impl(1, size, file, line, func);

	old_h = alloc_header_from_user(ptr);
	if (old_h->magic != TCC_ALLOC_MAGIC) {
		fprintf(stderr,
		        "%s:%d: fatal: invalid realloc in %s: ptr=%p bad magic=%lx\n",
		        file ? file : "<unknown>",
		        line,
		        func ? func : "<unknown>",
		        ptr,
		        old_h->magic);
		abort();
	}

	old_size = old_h->size;
	total = sizeof(AllocHeader) + size;
	new_h = realloc(old_h, total);
	if (!new_h)
		alloc_fail_oom("realloc", 1, size, size, total, file, line, func);

	new_h->magic = TCC_ALLOC_MAGIC;
	if (size > old_size) {
		char *base = (char *)(new_h + 1);
		memset(base + old_size, 0, size - old_size);
	}
	new_h->size = size;
	user = (void *)(new_h + 1);

	Debug(2, "ALLOC realloc %s:%d %s old=%lu new=%lu total=%lu %p -> %p\n",
	      file ? file : "<unknown>",
	      line,
	      func ? func : "<unknown>",
	      (unsigned long)old_size,
	      (unsigned long)size,
	      (unsigned long)total,
	      ptr,
	      user);

	return user;
}

void
xfree_impl(void *ptr, const char *file, int line, const char *func)
{
	AllocHeader *h;

	if (!ptr)
		return;

	h = alloc_header_from_user(ptr);
	if (h->magic != TCC_ALLOC_MAGIC) {
		fprintf(stderr,
		        "%s:%d: fatal: invalid free in %s: ptr=%p bad magic=%lx\n",
		        file ? file : "<unknown>",
		        line,
		        func ? func : "<unknown>",
		        ptr,
		        h->magic);
		abort();
	}

	Debug(2, "ALLOC free %s:%d %s size=%lu ptr=%p\n",
	      file ? file : "<unknown>",
	      line,
	      func ? func : "<unknown>",
	      (unsigned long)h->size,
	      ptr);

	h->magic = TCC_ALLOC_FREED;
	h->size = 0;
	free(h);
}

int 
tcc_strcmp(const char *a, const char *b, const char *file, int line, const char *expr)
{
	unsigned long pa = (unsigned long)a;
	unsigned long pb = (unsigned long)b;

	if (pa < 4096 || pb < 4096) {
		fprintf(stderr,
		        "%s:%d - STRCMP bad args: a=%p b=%p expr=[%s]\n",
		        file,
		        line,
		        (void *)a,
		        (void *)b,
		        expr);
		abort();
	}

	return strcmp(a, b);
}

char *
tcc_strcpy(char *dst, const char *src, const char *file, int line, const char *expr)
{
	char *ret;
	size_t i;

	if (!dst || !src) {
		fprintf(stderr,
		        "%s:%d - STRNCPY bad args: dst=%p src=%p expr=[%s]\n",
		        file,
		        line,
		        (void *)dst,
		        (void *)src,
		        expr ? expr : "<null>");
		abort();
	}

	ret = dst;

	for (i = 0; src[i]; i++)
		dst[i] = src[i];

	dst[i] = 0;

	return ret;
}

char *
tcc_strncpy(char *dst, const char *src, size_t n, const char *file, int line, const char *expr)
{
	size_t i;

	if (!dst || !src) {
		fprintf(stderr,
		        "%s:%d - STRNCPY bad args: dst=%p src=%p n=%lu expr=[%s]\n",
		        file,
		        line,
		        (void *)dst,
		        (void *)src,
		        (unsigned long)n,
		        expr ? expr : "<null>");
		abort();
	}

	for (i = 0; i < n && src[i]; i++)
		dst[i] = src[i];

	while (i < n)
		dst[i++] = '\0';

	return dst;
}

char *
xstrndup(const char *s, size_t n)
{
	char *p;

	p = xmalloc(n + 1);

	memcpy(p, s, n);
	p[n] = '\0';

	return p;
}

char *
xstrdup(const char *s)
{
	size_t len;
	char *p;

	if (!s)
		s = "";

	len = strlen(s);

	p = xmalloc(len + 1);
	memcpy(p, s, len + 1);

	return p;
}

void
node_print_location(FILE *out, const Node *node)
{
	const char *file;
	const char *pp_file;

	if (!out)
		out = stderr;

	if (!node) {
		fprintf(out, "<unknown>:0:0");
		return;
	}

	file = lexer_filename_name(node->filename_id);
	pp_file = lexer_filename_name(node->pp_filename_id);

	fprintf(out, "%s:%d:%d", file, node->line, node->column);

	if (node->pp_line > 0 &&
	        (STRCMP(pp_file, file) != 0 ||
	         node->pp_line != node->line ||
	         node->pp_column != node->column)) {
		fprintf(out, " (preprocessed %s:%d:%d)",
		        pp_file, node->pp_line, node->pp_column);
	}
}

void __attribute__((noreturn))
node_error_at(const Node *node, const char *fmt, ...)
{
	va_list ap;

	node_print_location(stderr, node);
	fprintf(stderr, ": error: ");

	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);

	if (node) {
		fprintf(stderr, "\n");
		node_print_location(stderr, node);
		fprintf(stderr, ": note: AST node %s (%d)",
		        node_kind_name(node->kind), node->kind);
		if (node->name[0])
			fprintf(stderr, " name=\"%s\"", node->name);
		if (node->struct_name[0])
			fprintf(stderr, " struct=\"%s\"", node->struct_name);
	}

	fprintf(stderr, "\n");
	exit(1);
}

static int
read_source_line_from_path(const char *path, int wanted_line, char *buf, size_t bufsz)
{
	FILE *f;
	int line = 1;
	size_t len;

	if (!path || !path[0] || !buf || bufsz == 0 || wanted_line <= 0)
		return 0;

	f = fopen(path, "r");
	if (!f)
		return 0;

	buf[0] = '\0';

	while (fgets(buf, (int)bufsz, f)) {
		if (line == wanted_line) {
			len = strlen(buf);
			while (len > 0 && (buf[len - 1] == '\n' || buf[len - 1] == '\r'))
				buf[--len] = '\0';
			fclose(f);
			return 1;
		}
		line++;
	}

	fclose(f);
	buf[0] = '\0';
	return 0;
}

static int
read_source_line(const char *file, int line, char *buf, size_t bufsz)
{
	char path[512];

	if (read_source_line_from_path(file, line, buf, bufsz))
		return 1;

	if (file && strchr(file, '/') == NULL) {
		snprintf(path, sizeof(path), "cc/include/%s", file);
		if (read_source_line_from_path(path, line, buf, bufsz))
			return 1;

		snprintf(path, sizeof(path), "include/%s", file);
		if (read_source_line_from_path(path, line, buf, bufsz))
			return 1;
	}

	return 0;
}

void
print_source_caret(const char *file, int line, int column)
{
	char buf[1024];
	int i;

	if (!read_source_line(file, line, buf, sizeof(buf)))
		return;

	fprintf(stderr, "\n%s\n", buf);

	if (column < 1)
		column = 1;

	for (i = 1; i < column; i++) {
		if (buf[i - 1] == '\t')
			fputc('\t', stderr);
		else
			fputc(' ', stderr);
	}

	fprintf(stderr, "^\n");
}

void __attribute__((noreturn))
fatal_token(const Token *token, const char *fmt, ...)
{
	va_list ap;
	const char *file = lexer_filename_name(token->filename_id);
	const char *pp_file = lexer_filename_name(token->pp_filename_id);

	fprintf(stderr, "%s:%d:%d: error: ", file, token->line, token->column);

	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);

	fprintf(stderr, "\n");
	print_source_caret(file, token->line, token->column);
	fprintf(stderr, "%s:%d:%d: note: near %s",
	        file, token->line, token->column, token_debug_name(token->kind));

	if (token->kind == TOK_IDENT || token->kind == TOK_STRING)
		fprintf(stderr, " text=\"%s\"", token->text ? token->text : "<null>");
	else if (token->kind == TOK_NUM)
		fprintf(stderr, " value=%d", token->value);

	if (token->pp_line > 0 &&
	        (STRCMP(pp_file, file) != 0 || token->pp_line != token->line)) {
		fprintf(stderr, "\n%s:%d:%d: note: preprocessed location",
		        pp_file, token->pp_line, token->pp_column);
		if (Debug_lvl > 0)
			lexer_debug_print_pp_line(token->pp_line, token->pp_column);
	}

	fprintf(stderr, "\n");
	exit(1);
}

/* Convenience: fatal error at the current (peek) token position. */
void __attribute__((noreturn))
fatal_cur(const char *fmt, ...)
{
	const Token *_cur = lexer_peek();
	va_list ap;
	const char *file = lexer_filename_name(_cur->filename_id);
	const char *pp_file = lexer_filename_name(_cur->pp_filename_id);

	fprintf(stderr, "%s:%d:%d: error: ", file, _cur->line, _cur->column);

	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);

	fprintf(stderr, "\n");
	print_source_caret(file, _cur->line, _cur->column);
	fprintf(stderr, "%s:%d:%d: note: near %s",
	        file, _cur->line, _cur->column, token_debug_name(_cur->kind));

	if (_cur->kind == TOK_IDENT || _cur->kind == TOK_STRING)
		fprintf(stderr, " text=\"%s\"", _cur->text ? _cur->text : "<null>");
	else if (_cur->kind == TOK_NUM)
		fprintf(stderr, " value=%d", _cur->value);

	if (_cur->pp_line > 0 &&
	        (STRCMP(pp_file, file) != 0 || _cur->pp_line != _cur->line)) {
		fprintf(stderr, "\n%s:%d:%d: note: preprocessed location",
		        pp_file, _cur->pp_line, _cur->pp_column);
		if (Debug_lvl > 0)
			lexer_debug_print_pp_line(_cur->pp_line, _cur->pp_column);
	}

	fprintf(stderr, "\n");
	exit(1);
}

void
Debug(int lvl, const char *fmt, ...)
{
	va_list ap;

	if (Debug_lvl >= lvl) {
		va_start(ap, fmt);
		vfprintf(stderr, fmt, ap);
		va_end(ap);
	}
}
