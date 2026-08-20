#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tcc.h"
#include "lexer.h"
#include "preprocess.h"

/* Token ring buffer - 65 slots (64 lookahead + 1 scratch), filled on demand */
#define TOK_RING_SIZE 65
#define TOK_RING_MASK 63
#define TOK_SCRATCH_SLOT 64  /* dedicated scratch slot for speculative scanning */

typedef struct {
	char *src;               /* current scan cursor through preprocessed input */
	char *source_base;       /* start of input (for diagnostics) */
	Token        tokens[TOK_RING_SIZE];
	char         *src_after[TOK_RING_SIZE];  /* src position AFTER each ring slot */
	char         *src_before[TOK_RING_SIZE]; /* src position BEFORE each ring slot */
	int          head;   /* index of current (peek[0]) token */
	int          fill;   /* how many valid slots are filled ahead of head */
} LexerRing;

static LexerRing lring;

static Token *
lexer_current_token(void)
{
	return &lring.tokens[lring.head];
}

/* Make internal read_token/set_token_location code transparently fill lring.tokens[lring.head] */
#define current (*lexer_current_token())
typedef struct {
	int line;
	int column;
	int pp_line;
	int pp_column;
	int filename_id;
	int pp_filename_id;
} LexerPos;

static LexerPos lpos = {1, 1, 1, 1, 0, 0};

/*
 * Token.text lifetime
 * -------------------
 *
 * The parser frequently saves Token structs by value and may continue to use
 * token.text after later lexer_peek()/lexer_next() calls.  Therefore token text
 * cannot live in the moving current token scratch object, nor can it be freed
 * when the lexer advances.
 *
 * Keep all token text allocated for the lifetime of the current lexer input.
 * lexer_init() resets this arena for the next translation unit.
 */
typedef struct TokenTextBlock {
	void *next;
	unsigned int used;
	unsigned int cap;
	char data[];
} TokenTextBlock;

typedef struct FilenameEntry {
	const char *name;
} FilenameEntry;

/* All remaining lexer state that is NOT reset between translation units.
 * lstr.token_text_blocks lives here but IS reset by lexer_init() via lexer_reset_token_texts(). */
typedef struct {
	void          *token_text_blocks;  /* arena for token text; reset per file */
	FilenameEntry   filenames[2048];    /* interned filename strings; spans files */
	int             filename_count;
} LexerStrings;

static LexerStrings lstr;

static void read_token(void);
static void scratch_read_token(void);
static char *lexer_save_token_text(char *s);
static char *lexer_save_token_text_len(char *s, size_t len);
static void lexer_reset_token_texts(void);

static void
lexer_reject_c11_unicode_literal_prefix(const char *prefix)
{
	if (tcc_lang_at_least(LANG_C11))
		return;
	fatal_lex(lexer_filename_name(lpos.filename_id), lpos.line, lpos.column,
	          "%s literal prefix is not allowed before C11\n", prefix);
}

static void
lexer_reject_c23_unicode_literal_prefix(const char *prefix)
{
	if (tcc_lang_at_least(LANG_C23))
		return;
	fatal_lex(lexer_filename_name(lpos.filename_id), lpos.line, lpos.column,
	          "%s literal prefix is not allowed before C23\n", prefix);
}

static void
lexer_reject_invalid_unicode_literal_prefix(const char *prefix)
{
	fatal_lex(lexer_filename_name(lpos.filename_id), lpos.line, lpos.column,
	          "%s literal prefix is not allowed\n", prefix);
}

static void
lexer_fill_slot_at_head(void)
{
	lring.src_before[lring.head] = lring.src;
	read_token();
	lring.src_after[lring.head] = lring.src;
}

void 
lexer_init(const char *source)
{
	lexer_reset_token_texts();
	lring.src = (char *)source;
	lring.source_base = (char *)source;
	lpos.line = 1;
	lpos.column = 1;
	lpos.pp_line = 1;
	lpos.pp_column = 1;
	lpos.filename_id = 0;
	lpos.pp_filename_id = 0;
	lring.head = 0;
	lring.fill = 0;
	/* Prime slot 0 */
	lexer_fill_slot_at_head();
	lring.fill = 1;
}

int
lexer_filename_intern(const char *name)
{
	int i;
	size_t len;
	char *copy;

	if (lstr.filename_count == 0) {
		lstr.filenames[0].name = "<input>";
		lstr.filename_count = 1;
	}

	if (name == 0)
		name = "<input>";

	for (i = 0; i < lstr.filename_count; i++) {
		if (!lstr.filenames[i].name)
			ICE("null filename entry %d/%d", i, lstr.filename_count);
		if (STRCMP(lstr.filenames[i].name, name) == 0)
			return i;
	}

	if (lstr.filename_count >= 2048) {
		ICE("too many filenames interned (limit 2048)");
	}

	len = strlen(name);
	copy = xmalloc(len + 1);
	memcpy(copy, name, len + 1);

	lstr.filenames[lstr.filename_count].name = copy;
	return lstr.filename_count++;
}


void
lexer_debug_print_pp_line(int line, int column)
{
	char *p = (char *)lring.source_base;
	char *start;
	char *end;
	int cur = 1;
	int i;

	if (!p || line <= 0)
		return;

	while (*p && cur < line) {
		if (*p++ == '\n')
			cur++;
	}

	if (cur != line)
		return;

	start = p;
	end = p;
	while (*end && *end != '\n')
		end++;

	fprintf(stderr, "\n[token stream line %d] %.*s\n",
	        line, (int)(end - start), start);

	if (column < 1)
		column = 1;
	for (i = 1; i < column; i++) {
		if (start[i - 1] == '\t')
			fputc('\t', stderr);
		else
			fputc(' ', stderr);
	}
	fprintf(stderr, "^\n");
}

const char *
lexer_filename_name(int id)
{
	if (lstr.filename_count == 0) {
		lstr.filenames[0].name = "<input>";
		lstr.filename_count = 1;
	}

	if (id < 0 || id >= lstr.filename_count)
		return "<invalid>";

	return lstr.filenames[id].name;
}

static void
lexer_reset_token_texts(void)
{
	void *block;

	block = lstr.token_text_blocks;

	while (block) {
		void *next = ((TokenTextBlock *)block)->next;
		xfree(block);
		block = next;
	}

	lstr.token_text_blocks = NULL;
}

static char *
lexer_save_token_text_len(char *s, size_t len)
{
	TokenTextBlock *block;
	char *copy;
	char *text = (char *)s;
	unsigned int need;
	unsigned int cap;

	if (text == 0) {
		text = "";
		len = 0;
	}

	need = (unsigned int)(len + 1);
	block = (TokenTextBlock *)lstr.token_text_blocks;

	if (block == NULL || block->cap - block->used < need) {
		cap = 4096;
		if (cap < need)
			cap = need;

		block = xmalloc(sizeof(*block) + cap);
		block->next = (TokenTextBlock *)lstr.token_text_blocks;
		block->used = 0;
		block->cap = cap;
		lstr.token_text_blocks = block;
	}

	copy = block->data + block->used;
	memcpy(copy, text, len);
	copy[len] = '\0';
	block->used += need;
	return copy;
}

static char *
lexer_save_token_text(char *s)
{
	char *text = (char *)s;
	return lexer_save_token_text_len(text ? text : "", text ? strlen(text) : 0);
}

static void 
token_set_text(char *s)
{
	char *text = (char *)s;
	Token *tok = lexer_current_token();
	/* Silently overwrite: double-sets occur for __FILE__ expansion. */
	tok->text = lexer_save_token_text(text ? text : "");
	tok->text_len = text ? (unsigned int)strlen(text) : 0;
}

static void
token_set_text_len(char *s, size_t len)
{
	char *text = (char *)s;
	Token *tok = lexer_current_token();
	tok->text = lexer_save_token_text_len(text ? text : "", text ? len : 0);
	tok->text_len = text ? (unsigned int)len : 0;
}

static void 
set_logical_filename(char *name)
{
	LexerPos *pos = &lpos;

	pos->filename_id = lexer_filename_intern(name);
}

static int 
try_consume_line_directive(void)
{
	char *p = (char *)lring.src;

	if (*p != '#')
		return 0;

	/* A #line directive is valid only when preceded by whitespace (or nothing)
	 * on the current physical line. */
	{
		char *q = p - 1;
		char *source_base = (char *)lring.source_base;
		while (q >= source_base && *q != '\n') {
			if (*q != ' ' && *q != '\t') {
				return 0;
			}
			q--;
		}
	}

	p++;

	if (STRNCMP(p, "line", 4) == 0 && (p[4] == ' ' || p[4] == '\t'))
		p += 4;

	while (*p == ' ' || *p == '\t')
		p++;

	if (isdigit((unsigned char)*p) == 0)
		return 0;

	int line = 0;
	while (isdigit((unsigned char)*p)) {
		line = line * 10 + (*p - '0');
		p++;
	}

	while (*p == ' ' || *p == '\t')
		p++;

	if (*p == '"') {
		p++;
		char name[256];
		int ni = 0;

		while (*p && *p != '"') {
			if (ni + 1 < (int)sizeof(name))
				name[ni++] = *p;
			p++;
		}

		name[ni] = '\0';
		if (*p == '"')
			p++;
		set_logical_filename((char *)name);
	}

	while (*p && *p != '\n')
		p++;

	if (*p == '\n') {
		p++;
		lpos.pp_line++;
	}

	lring.src = p;
	lpos.line = line;
	lpos.column = 1;
	lpos.pp_column = 1;
	return 1;
}

static void 
advance_char(void)
{
	if (*lring.src == '\n') {
		lpos.line++;
		lpos.column = 1;
		lpos.pp_line++;
		lpos.pp_column = 1;
	} else if (*lring.src) {
		lpos.column++;
		lpos.pp_column++;
	}
	lring.src++;
}

static void 
set_token_location(void)
{
	current.filename_id = lpos.filename_id;
	current.line = lpos.line;
	current.column = lpos.column;

	current.pp_filename_id = lpos.pp_filename_id;
	current.pp_line = lpos.pp_line;
	current.pp_column = lpos.pp_column;
}


static void
lexer_string_append_codepoint(char **buf, size_t *len, size_t *cap, int cp, int width)
{
	int i;
	if (width != 2 && width != 4)
		width = 1;
	while (*len + (size_t)width + 4 >= *cap) {
		*cap *= 2;
		*buf = xrealloc(*buf, *cap);
	}
	for (i = 0; i < width; i++)
		(*buf)[(*len)++] = (char)((cp >> (8 * i)) & 0xff);
}

static int
lexer_hex_value(char ch)
{
	if (ch >= '0' && ch <= '9')
		return ch - '0';
	if (ch >= 'a' && ch <= 'f')
		return ch - 'a' + 10;
	if (ch >= 'A' && ch <= 'F')
		return ch - 'A' + 10;
	return -1;
}

static int
lexer_peek_ucn_codepoint(int *out_cp, int *out_len)
{
	int digits;
	int cp = 0;
	int i;

	if (*lring.src != '\\' || (lring.src[1] != 'u' && lring.src[1] != 'U'))
		return 0;

	digits = (lring.src[1] == 'u') ? 4 : 8;
	for (i = 0; i < digits; i++) {
		int value = lexer_hex_value(lring.src[2 + i]);
		if (value < 0)
			return 0;
		cp = (cp << 4) | value;
	}

	if (out_cp)
		*out_cp = cp;
	if (out_len)
		*out_len = digits + 2;
	return 1;
}

static int
lexer_ucn_is_ident_start(int cp)
{
	if (cp == '_')
		return 1;
	if (cp >= 0x80)
		return 1;
	return isalpha((unsigned char)cp) != 0;
}

static int
lexer_ucn_is_ident_char(int cp)
{
	if (cp == '_')
		return 1;
	if (cp >= 0x80)
		return 1;
	return isalnum((unsigned char)cp) != 0;
}

static int
lexer_ucn_is_valid_identifier_codepoint(int cp)
{
	if (cp == 0x24 || cp == 0x40 || cp == 0x60)
		return 1;
	if (cp >= 0x00a0 && !(cp >= 0xd800 && cp <= 0xdfff) && cp <= 0x10ffff)
		return 1;
	return 0;
}

static int
lexer_source_starts_identifier(void)
{
	int cp = 0;
	int ucn_len = 0;

	if (isalpha((unsigned char)*lring.src) || *lring.src == '_')
		return 1;

	return lexer_peek_ucn_codepoint(&cp, &ucn_len) && lexer_ucn_is_ident_start(cp);
}

static int
lexer_source_starts_identifier_char(void)
{
	int cp = 0;
	int ucn_len = 0;

	if (isalnum((unsigned char)*lring.src) || *lring.src == '_')
		return 1;

	return lexer_peek_ucn_codepoint(&cp, &ucn_len) && lexer_ucn_is_ident_char(cp);
}

static int
lexer_ulong_mul_add_overflows(unsigned long value, unsigned int base,
                              unsigned int digit)
{
	unsigned long max = (unsigned long)-1;
	return value > (max - digit) / base;
}

static unsigned long
lexer_uint_max_value(void)
{
	return (unsigned long)(unsigned int)-1;
}

static unsigned long
lexer_int_max_value(void)
{
	return lexer_uint_max_value() >> 1;
}

static unsigned long
lexer_ulong_max_value(void)
{
	return (unsigned long)-1;
}

static unsigned long
lexer_long_max_value(void)
{
	return lexer_ulong_max_value() >> 1;
}

static int
lexer_append_ident_codepoint(char *text, int i, int cap, int cp)
{
	char escaped[12];
	int n;

	if (!lexer_ucn_is_valid_identifier_codepoint(cp))
		fatal_lex(lexer_filename_name(lpos.filename_id), lpos.line,
		          lpos.column, "invalid universal character name in identifier");

	n = snprintf(escaped, sizeof(escaped), "_U%06X", cp);
	if (n < 0 || i + n >= cap)
		fatal_lex(lexer_filename_name(lpos.filename_id), lpos.line,
		          lpos.column, "identifier too long (max %d chars)",
		          TCC_IDENT_MAX);
	memcpy(text + i, escaped, (size_t)n);
	return i + n;
}

static int
lexer_read_escape_value(void)
{
	int value = 0;
	advance_char(); /* consume backslash */

	if (*lring.src >= '0' && *lring.src <= '7') {
		value = *lring.src - '0';
		advance_char();
		if (*lring.src >= '0' && *lring.src <= '7') {
			value = value * 8 + (*lring.src - '0');
			advance_char();
			if (*lring.src >= '0' && *lring.src <= '7') {
				value = value * 8 + (*lring.src - '0');
				advance_char();
			}
		}
		return value;
	}

	if (*lring.src == 'x' || *lring.src == 'X') {
		int n = 0;
		advance_char();
		while (isxdigit((unsigned char)*lring.src)) {
			value = value * 16 + (isdigit((unsigned char)*lring.src)
				? (*lring.src - '0')
				: (tolower((unsigned char)*lring.src) - 'a' + 10));
			advance_char();
			n++;
		}
		if (n == 0)
			fatal_lex(lexer_filename_name(lpos.filename_id), lpos.line, 0,
			          "Hex escape sequence requires at least one hex digit\n");
		return value;
	}

	/* \uXXXX — 4 hex digits, Unicode codepoint up to U+FFFF */
	if (*lring.src == 'u') {
		int n;
		advance_char();
		for (n = 0; n < 4 && isxdigit((unsigned char)*lring.src); n++) {
			value = value * 16 + (isdigit((unsigned char)*lring.src)
				? (*lring.src - '0')
				: (tolower((unsigned char)*lring.src) - 'a' + 10));
			advance_char();
		}
		if (n != 4)
			fatal_lex(lexer_filename_name(lpos.filename_id), lpos.line, 0,
			          "Incomplete universal character name escape\n");
		if (value > 0x10ffff || (value >= 0xd800 && value <= 0xdfff))
			fatal_lex(lexer_filename_name(lpos.filename_id), lpos.line, 0,
			          "Invalid universal character name escape\n");
		return value;
	}

	/* \UXXXXXXXX — 8 hex digits, full Unicode codepoint */
	if (*lring.src == 'U') {
		int n;
		advance_char();
		for (n = 0; n < 8 && isxdigit((unsigned char)*lring.src); n++) {
			value = value * 16 + (isdigit((unsigned char)*lring.src)
				? (*lring.src - '0')
				: (tolower((unsigned char)*lring.src) - 'a' + 10));
			advance_char();
		}
		if (n != 8)
			fatal_lex(lexer_filename_name(lpos.filename_id), lpos.line, 0,
			          "Incomplete universal character name escape\n");
		if (value > 0x10ffff || (value >= 0xd800 && value <= 0xdfff))
			fatal_lex(lexer_filename_name(lpos.filename_id), lpos.line, 0,
			          "Invalid universal character name escape\n");
		return value;
	}

	switch (*lring.src) {
	case 'n': value = '\n'; break;
	case 'r': value = '\r'; break;
	case 't': value = '\t'; break;
	case 'a': value = '\a'; break;
	case 'b': value = '\b'; break;
	case 'f': value = '\f'; break;
	case 'v': value = '\v'; break;
	case '\\': value = '\\'; break;
	case '"': value = '"'; break;
	case '\'': value = '\''; break;
	case '\0': value = '\0'; break;
	default: value = *lring.src; break;
	}
	if (*lring.src)
		advance_char();
	return value;
}

static void
lexer_lex_string_literal_width(int width)
{
	size_t cap = 256;
	size_t len = 0;
	char *buf = xmalloc(cap);

	if (width != 2 && width != 4)
		width = 1;

	advance_char(); /* opening quote */

	while (*lring.src && *lring.src != '"') {
		int cp;
		if (*lring.src == '\\' && lring.src[1])
			cp = lexer_read_escape_value();
		else {
			cp = (unsigned char)*lring.src;
			advance_char();
		}
		lexer_string_append_codepoint(&buf, &len, &cap, cp, width);
	}

	if (*lring.src != '"') {
		xfree(buf);
		fatal_lex(lexer_filename_name(lpos.filename_id), lpos.line, 0,
		          "Unterminated string literal\n");
	}
	advance_char();

	/* Adjacent string literal concatenation for the same element width. */
	while (1) {
		int next_width = 0;
		for (;;) {
			while (isspace((unsigned char)*lring.src))
				advance_char();
			if (try_consume_line_directive() == 0)
				break;
		}
		if (*lring.src == '"')
			next_width = 1;
		else if (lring.src[0] == 'L' && lring.src[1] == '"')
			next_width = 4;
		else if (lring.src[0] == 'u' && lring.src[1] == '8' && lring.src[2] == '"')
			next_width = 1;
		else if (lring.src[0] == 'u' && lring.src[1] == '"')
			next_width = 2;
		else if (lring.src[0] == 'U' && lring.src[1] == '"')
			next_width = 4;
		else
			break;

		if (next_width != width)
			break;

		if (*lring.src == 'L' || *lring.src == 'U')
			advance_char();
		else if (lring.src[0] == 'u' && lring.src[1] == '8') {
			lexer_reject_c23_unicode_literal_prefix("u8 string");
			advance_char();
			advance_char();
		} else if (*lring.src == 'u') {
			lexer_reject_c11_unicode_literal_prefix("u string");
			advance_char();
		}
		advance_char(); /* opening quote */

		while (*lring.src && *lring.src != '"') {
			int cp;
			if (*lring.src == '\\' && lring.src[1])
				cp = lexer_read_escape_value();
			else {
				cp = (unsigned char)*lring.src;
				advance_char();
			}
			lexer_string_append_codepoint(&buf, &len, &cap, cp, width);
		}
		if (*lring.src != '"') {
			xfree(buf);
			fatal_lex(lexer_filename_name(lpos.filename_id), lpos.line, 0,
			          "Unterminated string literal\n");
		}
		advance_char();
	}

	/* Existing emitters append one final zero byte.  Store the other bytes of
	 * the wide terminator here so sizeof/string storage sees a full null element. */
	if (width > 1) {
		int i;
		for (i = 1; i < width; i++)
			lexer_string_append_codepoint(&buf, &len, &cap, 0, 1);
	}

	buf[len] = '\0';
	token_set_text_len(buf, len);
	xfree(buf);
	current.kind = TOK_STRING;
	current.string_width = width;
}

static void
lexer_lex_prefixed_char_literal(void)
{
	int value;
	advance_char(); /* opening quote */
	if (*lring.src == '\\' && lring.src[1])
		value = lexer_read_escape_value();
	else {
		value = (unsigned char)*lring.src;
		if (*lring.src)
			advance_char();
	}
	if (*lring.src != '\'')
		fatal_lex(lexer_filename_name(lpos.filename_id), lpos.line, 0,
		          "Unterminated char literal\n");
	advance_char();
	current.kind = TOK_NUM;
	current.value = value;
	current.long_value = (long)value;
}

static void 
read_token(void)
{
	/* Initialise current token */
	memset(&current, 0, sizeof(Token));

	for (;;) {
		while (isspace((unsigned char)*lring.src))
			advance_char();

		if (try_consume_line_directive() == 0)
			break;
	}

	set_token_location();

	if (*lring.src == '\0') {
		current.kind = TOK_EOF;
		return;
	}

	if (*lring.src == '~') {
		current.kind = TOK_TILDE;
		advance_char();
		return;
	}

	if (*lring.src == '\'') {
		advance_char(); /* consume opening quote */

		int value;

		if (*lring.src == '\\') {
			/* lexer_read_escape_value() consumes the backslash itself and
			 * the full escape sequence, matching the logic used by string
			 * literals via lexer_lex_string_literal_width(). */
			value = lexer_read_escape_value();
		} else {
			value = (unsigned char)*lring.src;
			advance_char();
		}

		/* Multi-character literals are implementation-defined (C11 §6.4.4.4).
		 * Consume any extra characters (keeping the last value) and warn once,
		 * rather than issuing a fatal error that rejects valid (if unportable) code. */
		if (*lring.src != '\'') {
			tcc_warn("%s:%d: multi-character character constant",
			         lexer_filename_name(lpos.filename_id), lpos.line);
			while (*lring.src && *lring.src != '\'') {
				if (*lring.src == '\\' && lring.src[1])
					advance_char(); /* skip backslash; next advance below takes the escaped char */
				value = (unsigned char)*lring.src;
				advance_char();
			}
			if (*lring.src != '\'') {
				fatal_lex(lexer_filename_name(lpos.filename_id), lpos.line, 0,
				          "Unterminated char literal\n");
			}
		}
		advance_char();

		current.kind = TOK_NUM;
		current.value = value;
		current.long_value = (long)value;
		return;
	}

	if (*lring.src == '"') {
		lexer_lex_string_literal_width(1);
		return;
	}



		if (isdigit((unsigned char)*lring.src)) {
			char *number_start = (char *)lring.src;
			unsigned long value = 0;
			int digit_count = 0;
			int saw_unsigned_suffix = 0;
			int long_suffix_count = 0;
			int saw_fraction = 0;
			int saw_exponent = 0;
			int saw_float_suffix = 0;
			int integer_overflow = 0;
			int is_hex = 0;
			int is_octal = 0;

			if (lring.src[0] == '0' && (lring.src[1] == 'x' || lring.src[1] == 'X')) {
				is_hex = 1;
				advance_char(); /* 0 */
				advance_char(); /* x */
				while (isxdigit((unsigned char)*lring.src)) {
					int digit;
					if (*lring.src >= '0' && *lring.src <= '9')
						digit = *lring.src - '0';
					else if (*lring.src >= 'a' && *lring.src <= 'f')
						digit = *lring.src - 'a' + 10;
					else
						digit = *lring.src - 'A' + 10;
					if (!integer_overflow) {
						if (lexer_ulong_mul_add_overflows(value, 16, (unsigned int)digit))
							integer_overflow = 1;
						else
							value = value * 16 + (unsigned long)digit;
					}
					advance_char();
					digit_count++;
				}
			} else if (lring.src[0] == '0') {
				/* C integer constants with a leading 0 are octal. */
				is_octal = 1;
				advance_char(); /* consume the leading 0 */
				while (*lring.src >= '0' && *lring.src <= '7') {
					unsigned int digit = (unsigned int)(*lring.src - '0');
					if (!integer_overflow) {
						if (lexer_ulong_mul_add_overflows(value, 8, digit))
							integer_overflow = 1;
						else
							value = value * 8 + (unsigned long)digit;
					}
					advance_char();
				}
				if (*lring.src == '8' || *lring.src == '9')
					fatal_cur("invalid digit in octal constant\n");
			} else {
				while (isdigit((unsigned char)*lring.src)) {
					unsigned int digit = (unsigned int)(*lring.src - '0');
					if (!integer_overflow) {
						if (lexer_ulong_mul_add_overflows(value, 10, digit))
							integer_overflow = 1;
						else
							value = value * 10 + (unsigned long)digit;
					}
					advance_char();
				}
			}

			if (is_hex) {
				/* Hex floating constants: 0x1p+1, 0x1.8p+1, 0x1p-1f. */
				if (*lring.src == '.') {
					saw_fraction = 1;
					advance_char();
					while (isxdigit((unsigned char)*lring.src)) {
						advance_char();
						digit_count++;
					}
				}
				if (digit_count == 0)
					fatal_cur("hexadecimal constant requires at least one digit\n");
				if (*lring.src == 'p' || *lring.src == 'P') {
					int exponent_digits = 0;
					saw_exponent = 1;
					advance_char();
					if (*lring.src == '+' || *lring.src == '-')
						advance_char();
					while (isdigit((unsigned char)*lring.src)) {
						advance_char();
						exponent_digits++;
					}
					if (exponent_digits == 0)
						fatal_cur("exponent has no digits\n");
				}
				if (saw_fraction && !saw_exponent)
					fatal_cur("hexadecimal floating constant requires a binary exponent\n");
			} else {
				/* consume float fractional part: 100.0, 3.14 */
				if (*lring.src == '.') {
					saw_fraction = 1;
					advance_char(); /* consume . */
					while (isdigit((unsigned char)*lring.src))
						advance_char();
				}
				/* consume float exponent: 1e10, 2.5E-3 */
				if (*lring.src == 'e' || *lring.src == 'E') {
					int exponent_digits = 0;
					saw_exponent = 1;
					advance_char();
					if (*lring.src == '+' || *lring.src == '-')
						advance_char();
					while (isdigit((unsigned char)*lring.src)) {
						advance_char();
						exponent_digits++;
					}
					if (exponent_digits == 0)
						fatal_cur("exponent has no digits\n");
				}
			}

			if ((is_hex && saw_exponent) || (!is_hex && (saw_fraction || saw_exponent))) {
				if (*lring.src == 'f' || *lring.src == 'F') {
					saw_float_suffix = 1;
					advance_char();
				} else if (*lring.src == 'l' || *lring.src == 'L') {
					/*
					 * The front-end currently models long double with the same
					 * runtime representation as double. Accept the standard
					 * literal suffix so conforming code parses cleanly.
					 */
					advance_char();
				}
				if (*lring.src == 'u' || *lring.src == 'U')
					fatal_cur("invalid suffix on floating constant\n");
				if (lexer_source_starts_identifier_char())
					fatal_cur("invalid suffix on floating constant\n");

				current.kind = TOK_NUM;
				current.value = 0;
				current.long_value = 0;
				current.num_is_fp = 1;
				current.num_is_float = saw_float_suffix;
				current.num_is_unsigned = 0;
				current.num_size = saw_float_suffix ? 4 : 8;
				token_set_text_len(number_start, (size_t)(lring.src - number_start));
				return;
			}

			/* consume integer suffixes: u, U, l, L, ll, LL etc. */
			while (*lring.src == 'u' || *lring.src == 'U' || *lring.src == 'l' || *lring.src == 'L') {
				if (*lring.src == 'u' || *lring.src == 'U')
					saw_unsigned_suffix++;
				else if (*lring.src == 'l' || *lring.src == 'L')
					long_suffix_count++;
				if (saw_unsigned_suffix > 1 || long_suffix_count > 2)
					fatal_cur("invalid suffix on integer constant\n");
				advance_char();
				if ((long_suffix_count == 2 && (*lring.src == 'l' || *lring.src == 'L')) ||
				    (saw_unsigned_suffix && long_suffix_count == 1 &&
				     (*lring.src == 'l' || *lring.src == 'L') &&
				     (lring.src[-1] == 'u' || lring.src[-1] == 'U')))
					fatal_cur("invalid suffix on integer constant\n");
			}
			if (lexer_source_starts_identifier_char())
				fatal_cur("invalid suffix on integer constant\n");
			if (integer_overflow)
				fatal_cur("integer constant is too large\n");

			current.kind = TOK_NUM;
			current.value = (int)value;
			current.long_value = (long)value;
			current.num_is_fp = 0;
			current.num_is_float = 0;
			current.num_is_unsigned = saw_unsigned_suffix;
			current.num_rank = 3;
			current.num_size = 4;
			if (long_suffix_count == 2) {
				current.num_rank = 5;
				current.num_size = 8;
			} else if (long_suffix_count == 1) {
				current.num_rank = 4;
				current.num_size = 8;
			} else if (saw_unsigned_suffix) {
				if (value > lexer_uint_max_value()) {
					current.num_rank = 4;
					current.num_size = 8;
				}
			} else {
				if (is_hex || is_octal) {
					if (value <= lexer_int_max_value()) {
						current.num_rank = 3;
						current.num_size = 4;
					} else if (value <= lexer_uint_max_value()) {
						current.num_is_unsigned = 1;
						current.num_rank = 3;
						current.num_size = 4;
					} else if (value <= lexer_long_max_value()) {
						current.num_rank = 4;
						current.num_size = 8;
					} else {
						current.num_is_unsigned = 1;
						current.num_rank = 4;
						current.num_size = 8;
					}
				} else if (value > lexer_int_max_value()) {
					if (value > lexer_long_max_value())
						fatal_cur("integer constant is too large\n");
					current.num_rank = 4;
					current.num_size = 8;
				}
			}
			token_set_text_len(number_start, (size_t)(lring.src - number_start));
			return;
		}

		if (lring.src[0] == '.' && isdigit((unsigned char)lring.src[1])) {
			char *number_start = (char *)lring.src;
			int saw_float_suffix = 0;

			advance_char(); /* consume . */
			while (isdigit((unsigned char)*lring.src))
				advance_char();
			if (*lring.src == 'e' || *lring.src == 'E') {
				int exponent_digits = 0;
				advance_char();
				if (*lring.src == '+' || *lring.src == '-')
					advance_char();
				while (isdigit((unsigned char)*lring.src)) {
					advance_char();
					exponent_digits++;
				}
				if (exponent_digits == 0)
					fatal_cur("exponent has no digits\n");
			}
			if (*lring.src == 'f' || *lring.src == 'F') {
				saw_float_suffix = 1;
				advance_char();
			} else if (*lring.src == 'l' || *lring.src == 'L') {
				/*
				 * The front-end currently models long double with the same
				 * runtime representation as double. Accept the standard
				 * literal suffix so conforming code parses cleanly.
				 */
				advance_char();
			}
			if (*lring.src == 'u' || *lring.src == 'U')
				fatal_cur("invalid suffix on floating constant\n");
			if (lexer_source_starts_identifier_char())
				fatal_cur("invalid suffix on floating constant\n");

			current.kind = TOK_NUM;
			current.value = 0;
			current.long_value = 0;
			current.num_is_fp = 1;
			current.num_is_float = saw_float_suffix;
			current.num_is_unsigned = 0;
			current.num_size = saw_float_suffix ? 4 : 8;
			token_set_text_len(number_start, (size_t)(lring.src - number_start));
			return;
		}

		if (lexer_source_starts_identifier()) {
                  /* Parser internal structs Field, Local, StructDef, FuncInfo,
                   * TypedefName, EnumConst all store names in char name[64],
                   * copied with STRNCPY at sizeof-1 = 63 chars.  Silently
                   * truncating here would produce confusing "undefined" errors
                   * at link time.  Enforce the limit here where we have source
                   * location context and can give a clear diagnostic. */
                  char text[TCC_IDENT_BUF_SIZE];
                  int i = 0;

                  while (1) {
                    int cp = 0;
                    int ucn_len = 0;

                    if (lexer_peek_ucn_codepoint(&cp, &ucn_len)) {
                      if (!lexer_ucn_is_ident_char(cp))
                        break;
                      i = lexer_append_ident_codepoint(text, i,
                                                       (int)sizeof(text), cp);
                      while (ucn_len-- > 0)
                        advance_char();
                      continue;
                    }

                    if (!(isalnum((unsigned char)*lring.src) ||
                          *lring.src == '_'))
                      break;

                    if (i < (int)sizeof(text) - 1)
                      text[i++] = *lring.src;
                    else
                      fatal_lex(lexer_filename_name(lpos.filename_id),
                                lpos.line, lpos.column,
                                "identifier too long (max %d chars)",
                                TCC_IDENT_MAX);
                    advance_char();
                  }

                  text[i] = '\0';
                  token_set_text(text);

                  /* Prefixed character/string literals: L, u, U, and u8 strings. */
                  if ((i == 1 &&
                       (text[0] == 'L' || text[0] == 'u' || text[0] == 'U')) ||
                      (i == 2 && text[0] == 'u' && text[1] == '8')) {
                    int width = 1;
                    if (i == 1 && text[0] == 'u')
                      width = 2;
                    else if (i == 1 && (text[0] == 'L' || text[0] == 'U'))
                      width = 4;

                    if (*lring.src == '\'') {
                      if (i == 1 && text[0] == 'u')
                        lexer_reject_c11_unicode_literal_prefix("u character");
                      else if (i == 1 && text[0] == 'U')
                        lexer_reject_c11_unicode_literal_prefix("U character");
                      else if (i == 2 && text[0] == 'u' && text[1] == '8')
                        lexer_reject_invalid_unicode_literal_prefix("u8 character");
                      lexer_lex_prefixed_char_literal();
                      return;
                    }
                    if (*lring.src == '"') {
                      if (i == 1 && text[0] == 'u')
                        lexer_reject_c11_unicode_literal_prefix("u string");
                      else if (i == 1 && text[0] == 'U')
                        lexer_reject_c11_unicode_literal_prefix("U string");
                      else if (i == 2 && text[0] == 'u' && text[1] == '8')
                        lexer_reject_c23_unicode_literal_prefix("u8 string");
                      lexer_lex_string_literal_width(width);
                      return;
                    }
                  }

/* Keyword dispatch: length then leading chars narrow candidates to 1-2,
 * a final STRCMP confirms.  This avoids the 35-branch pure-STRCMP chain
 * while being correct for any identifier (e.g. size_t ≠ sizeof). */
#define KW(str, tok)                                                           \
  do {                                                                         \
    if (STRCMP(text, str) == 0)                                                \
      current.kind = (tok);                                                    \
  } while (0)

    switch (i) {
    case 2:
      if (text[0] == 'i') {
        KW("if", TOK_IF);
      } else {
        KW("do", TOK_DO);
      }
      break;
    case 3:
      if (text[0] == 'i') {
        KW("int", TOK_INT);
      } else if (text[0] == 'f') {
        KW("for", TOK_FOR);
      } else if (text[0] == 'a') {
        KW("asm", TOK_ASM);
      }
      break;
    case 4:
      if (text[0] == 'c') {
        if (text[1] == 'h') {
          KW("char", TOK_CHAR);
        } else {
          KW("case", TOK_CASE);
        }
      } else if (text[0] == 'e') {
        if (text[1] == 'l') {
          KW("else", TOK_ELSE);
        } else {
          KW("enum", TOK_ENUM);
        }
      } else if (text[0] == 'g') {
        KW("goto", TOK_GOTO);
      } else if (text[0] == 'a') {
        if (text[1] == 'u') {
          KW("auto", TOK_AUTO);
        } else {
          KW("asm", TOK_ASM);
        }
      } else if (text[0] == 'l') {
        KW("long", TOK_LONG);
      } else if (text[0] == 'v') {
        KW("void", TOK_VOID);
      }
      break;
    case 5:
      switch (text[0]) {
      case 'b':
        KW("break", TOK_BREAK);
        break;
      case 'c':
        KW("const", TOK_CONST);
        break;
      case 'f':
        KW("float", TOK_FLOAT);
        break;
      case 's':
        KW("short", TOK_SHORT);
        break;
      case 'u':
        KW("union", TOK_UNION);
        break;
      case 'w':
        KW("while", TOK_WHILE);
        break;
      case '_':
        if (text[1] == 'B') {
          KW("_Bool", TOK_BOOL);
        } else {
          KW("__asm", TOK_ASM);
        }
        break;
      }
      break;
    case 6:
      switch (text[0]) {
      case 'd':
        KW("double", TOK_DOUBLE);
        break;
      case 'e':
        KW("extern", TOK_EXTERN);
        break;
      case 'i':
        KW("inline", TOK_INLINE);
        break;
      case 'r':
        KW("return", TOK_RETURN);
        break;
      case 's':
        if (text[1] == 'i') {
          if (text[2] == 'g') {
            KW("signed", TOK_SIGNED);
          } else {
            KW("sizeof", TOK_SIZEOF);
          }
        } else if (text[1] == 't') {
          if (text[2] == 'a') {
            KW("static", TOK_STATIC);
          } else {
            KW("struct", TOK_STRUCT);
          }
        } else if (text[1] == 'w') {
          KW("switch", TOK_SWITCH);
        }
        break;
      }
      break;
    case 7:
      if (text[0] == 't') {
        KW("typedef", TOK_TYPEDEF);
      } else if (text[0] == 'd') {
        KW("default", TOK_DEFAULT);
      } else if (text[0] == 'a') {
        if (tcc_lang_at_least(LANG_C23))
          KW("alignof", TOK_ALIGNOF);
      } else if (text[0] == '_') {
        if (text[1] == 'A') {
          KW("_Atomic", TOK_ATOMIC);
        } else {
          KW("__asm__", TOK_ASM);
        }
      }
      break;
    case 8:
      if (text[0] == 'c') {
        KW("continue", TOK_CONTINUE);
        break;
      }
      if (text[0] == 'r') {
        if (text[2] == 'g') {
          KW("register", TOK_REGISTER);
        } else {
          KW("restrict", TOK_RESTRICT);
        }
        break;
      }
      if (text[0] == 'u') {
        KW("unsigned", TOK_UNSIGNED);
        break;
      }
      if (text[0] == 'v') {
        KW("volatile", TOK_VOLATILE);
        break;
      }
      if (text[0] == '_' && text[1] == 'A') {
        KW("_Alignof", TOK_ALIGNOF);
        break;
      }
      if (text[0] == '_' && text[1] == '_') {
        if (text[2] == 'i') {
          KW("__inline", TOK_INLINE);
          break;
        }
        if (text[2] == 'L') { /* __LINE__ */
          if (STRCMP(text, "__LINE__") == 0) {
            current.kind = TOK_NUM;
            current.value = preprocess_current_line;
          }
          break;
        }
        if (text[2] == 'F') { /* __FILE__ */
          if (STRCMP(text, "__FILE__") == 0) {
            /* Lexer-side __FILE__ expansion: sets kind=TOK_STRING and
             * stores the plain filename as text (no surrounding quotes).
             * The TOK_STRING kind tells the parser this is already a
             * decoded string value, so no quote-stripping occurs.
             *
             * The preprocessor also defines __FILE__ as a macro whose
             * value IS quoted (e.g. "\"foo.c\"") via preprocess_set_file()
             * in preprocess.c.  These two paths are intentionally
             * independent: the preprocessor path handles __FILE__ in macro
             * bodies; this path handles __FILE__ in the token stream after
             * preprocessing.  Keep them in sync if either changes. */
            current.kind = TOK_STRING;
            current.text = 0;
            token_set_text((char *)lexer_filename_name(lpos.filename_id));
          }
          break;
        }
      }
      break;
    case 9:
      if (text[0] == '_' && text[1] == 'N') {
        KW("_Noreturn", TOK_NORETURN);
      }
      break;
    case 10:
      if (text[0] == '_' && text[2] == 'i') {
        KW("__inline__", TOK_INLINE);
      }
      break;
    case 12:
      if (text[0] == 't' && tcc_lang_at_least(LANG_C23)) {
        KW("thread_local", TOK_THREAD_LOCAL);
      }
      break;
    case 13:
      if (text[0] == '_' && text[1] == 'T') {
        KW("_Thread_local", TOK_THREAD_LOCAL);
      }
      break;
    }

#undef KW

		if (current.kind == 0)
			current.kind = TOK_IDENT;

		return;
  }

        if (lring.src[0] == '+' && lring.src[1] == '+') {
		advance_char();
		advance_char();
		current.kind = TOK_PLUSPLUS;
		return;
	}

	if (lring.src[0] == '-' && lring.src[1] == '-') {
		advance_char();
		advance_char();
		current.kind = TOK_MINUSMINUS;
		return;
	}

	if (lring.src[0] == '+' && lring.src[1] == '=') {
		advance_char();
		advance_char();
		current.kind = TOK_PLUSEQ;
		return;
	}

	if (lring.src[0] == '-' && lring.src[1] == '=') {
		advance_char();
		advance_char();
		current.kind = TOK_MINUSEQ;
		return;
	}

	if (lring.src[0] == '*' && lring.src[1] == '=') {
		advance_char();
		advance_char();
		current.kind = TOK_MULEQ;
		return;
	}

	if (lring.src[0] == '/' && lring.src[1] == '=') {
		advance_char();
		advance_char();
		current.kind = TOK_DIVEQ;
		return;
	}

	if (lring.src[0] == '%' && lring.src[1] == '=') {
		advance_char();
		advance_char();
		current.kind = TOK_MODEQ;
		return;
	}

	if (lring.src[0] == '<' && lring.src[1] == '<' && lring.src[2] == '=') {
		advance_char();
		advance_char();
		advance_char();
		current.kind = TOK_SHLEQ;
		return;
	}

	if (lring.src[0] == '>' && lring.src[1] == '>' && lring.src[2] == '=') {
		advance_char();
		advance_char();
		advance_char();
		current.kind = TOK_SHREQ;
		return;
	}

	if (lring.src[0] == '<' && lring.src[1] == '<') {
		advance_char();
		advance_char();
		current.kind = TOK_SHL;
		return;
	}

	if (lring.src[0] == '>' && lring.src[1] == '>') {
		advance_char();
		advance_char();
		current.kind = TOK_SHR;
		return;
	}

	if (lring.src[0] == '&' && lring.src[1] == '=') {
		advance_char();
		advance_char();
		current.kind = TOK_ANDEQ;
		return;
	}

	if (lring.src[0] == '|' && lring.src[1] == '=') {
		advance_char();
		advance_char();
		current.kind = TOK_OREQ;
		return;
	}

	if (lring.src[0] == '^' && lring.src[1] == '=') {
		advance_char();
		advance_char();
		current.kind = TOK_XOREQ;
		return;
	}

	if (lring.src[0] == '=' && lring.src[1] == '=') {
		advance_char();
		advance_char();
		current.kind = TOK_EQ;
		return;
	}

	if (lring.src[0] == '!' && lring.src[1] == '=') {
		advance_char();
		advance_char();
		current.kind = TOK_NE;
		return;
	}

	if (lring.src[0] == '<' && lring.src[1] == '=') {
		advance_char();
		advance_char();
		current.kind = TOK_LE;
		return;
	}

	if (lring.src[0] == '>' && lring.src[1] == '=') {
		advance_char();
		advance_char();
		current.kind = TOK_GE;
		return;
	}

	if (lring.src[0] == '&' && lring.src[1] == '&') {
		advance_char();
		advance_char();
		current.kind = TOK_AND;
		return;
	}

	if (lring.src[0] == '|' && lring.src[1] == '|') {
		advance_char();
		advance_char();
		current.kind = TOK_OR;
		return;
	}

	if (lring.src[0] == '-' && lring.src[1] == '>') {
		advance_char();
		advance_char();
		current.kind = TOK_ARROW;
		return;
	}

	/* C digraph punctuators. Preprocessor digraphs such as %: are a separate
	 * slice; this only covers the parser-visible bracket/brace spellings. */
	if (lring.src[0] == '<' && lring.src[1] == '%') {
		advance_char();
		advance_char();
		current.kind = TOK_LBRACE;
		return;
	}

	if (lring.src[0] == '%' && lring.src[1] == '>') {
		advance_char();
		advance_char();
		current.kind = TOK_RBRACE;
		return;
	}

	if (lring.src[0] == '<' && lring.src[1] == ':') {
		advance_char();
		advance_char();
		current.kind = TOK_LBRACKET;
		return;
	}

	if (lring.src[0] == ':' && lring.src[1] == '>') {
		advance_char();
		advance_char();
		current.kind = TOK_RBRACKET;
		return;
	}

	char ch = *lring.src;
	advance_char();
	switch (ch) {
	case '(':
		current.kind = TOK_LPAREN;
		return;
	case ')':
		current.kind = TOK_RPAREN;
		return;
	case '{':
		current.kind = TOK_LBRACE;
		return;
	case '}':
		current.kind = TOK_RBRACE;
		return;
	case '[':
		current.kind = TOK_LBRACKET;
		return;
	case ']':
		current.kind = TOK_RBRACKET;
		return;
	case ';':
		current.kind = TOK_SEMI;
		return;
	case ',':
		current.kind = TOK_COMMA;
		return;
	case ':':
		current.kind = TOK_COLON;
		return;
	case '?':
		current.kind = TOK_QUESTION;
		return;
	case '.':
		current.kind = TOK_DOT;
		return;
	case '=':
		current.kind = TOK_ASSIGN;
		return;
	case '+':
		current.kind = TOK_PLUS;
		return;
	case '-':
		current.kind = TOK_MINUS;
		return;
	case '*':
		current.kind = TOK_STAR;
		return;
	case '/':
		current.kind = TOK_SLASH;
		return;
	case '%':
		current.kind = TOK_PERCENT;
		return;
	case '<':
		current.kind = TOK_LT;
		return;
	case '>':
		current.kind = TOK_GT;
		return;
	case '&':
		current.kind = TOK_AMP;
		return;
	case '|':
		current.kind = TOK_PIPE;
		return;
	case '^':
		current.kind = TOK_CARET;
		return;
	case '!':
		current.kind = TOK_NOT;
		return;
	default:
		fatal_lex(lexer_filename_name(lpos.filename_id), lpos.line, lpos.column, "Unexpected character: '%c'\n", ch);
	}
}

void 
lexer_set_filename(const char *name)
{
	int interned = lexer_filename_intern(name);

	lpos.filename_id = interned;
	lpos.pp_filename_id = interned;
}

/* ----------- Ring buffer helpers ----------- */

/*
 * fill_ahead: ensure lring.tokens[(lring.head + n) & MASK] is valid.
 *
 * HOW IT WORKS — lring.head redirect trick
 * ---------------------------------------
 * read_token() always writes its result into lring.tokens[lring.head] via the
 * `current` macro (#define current lring.tokens[lring.head]).  To fill a slot
 * AHEAD of the real head without disturbing it, we temporarily move lring.head
 * to the target slot, call read_token(), then restore lring.head.  The ring
 * slots between real_head+1 and real_head+n are filled in order.
 *
 * WHY lring.src IS NOT RESTORED
 * -----------------------
 * lring.src tracks the read cursor through the preprocessed token stream.  After
 * fill_ahead returns, lring.src points just past the last filled token — exactly
 * where the next lexer_next() / fill_ahead() call needs to start.  Restoring
 * lring.src would lose that progress and re-lex the same tokens on the next call.
 * lring.src_before[]/lring.src_after[] record the lring.src positions around each slot
 * so that speculative scanners (lexer_is_function_prototype) can rewind
 * without disturbing the ring.
 *
 * NON-REENTRANCY WARNING
 * ----------------------
 * fill_ahead is not re-entrant.  It mutates lring.head as a side-channel to
 * redirect read_token().  Calling fill_ahead from within read_token() (e.g.
 * from a future keyword handler) would corrupt lring.head and the ring.
 * The only safe speculative caller is lexer_is_function_prototype, which uses
 * scratch_read_token() instead and never calls fill_ahead.
 */
static void
fill_ahead(int n)
{
	int i;
	if (n < lring.fill)
		return; /* already filled */

	int real_head = lring.head;
	for (i = lring.fill; i <= n; i++) {
		lring.head = (real_head + i) & TOK_RING_MASK;
		lexer_fill_slot_at_head();
	}
	lring.head = real_head;
	lring.fill = n + 1;
}

/* ----------- Public lexer API ----------- */

const Token *
lexer_peek(void)
{
	return &lring.tokens[lring.head];
}

const Token *
lexer_peek_ahead(int n)
{
	/* n=0 is current, n=1 is next, etc. */
	if (n >= lring.fill)
		fill_ahead(n);
	return &lring.tokens[(lring.head + n) & TOK_RING_MASK];
}

const Token *
lexer_next(void)
{
	const Token *old = &lring.tokens[lring.head];
	/* Advance head */
	lring.head = (lring.head + 1) & TOK_RING_MASK;
	lring.fill--;
	if (lring.fill < 0) lring.fill = 0;
	/* If ring is empty at new head, fill it */
	if (lring.fill == 0) {
		lexer_fill_slot_at_head();
		lring.fill = 1;
	}
	return old;
}


int
lexer_is_struct_definition(void)
{
	/* Current token is TOK_STRUCT.
	 * Check if the token stream looks like: STRUCT IDENT {
	 * Use the ring: peek ahead to get the next two tokens. */
	const Token *t1 = lexer_peek_ahead(1);
	const Token *t2 = lexer_peek_ahead(2);
	/* struct NAME { */
	if (t1->kind == TOK_IDENT && t2->kind == TOK_LBRACE)
		return 1;
	/* struct { (anonymous) */
	if (t1->kind == TOK_LBRACE)
		return 1;
	return 0;
}

/* scratch_read_token: reads the next token into TOK_SCRATCH_SLOT without
 * disturbing lring.head or the ring fill count.  Used exclusively by
 * lexer_is_function_prototype for speculative scanning. */
static void
scratch_read_token(void)
{
	int *head = &lring.head;
	int real_head = *head;
	*head = TOK_SCRATCH_SLOT;
	read_token();
	*head = real_head;
}

#define SCRATCH lring.tokens[TOK_SCRATCH_SLOT]

/* lexer_is_function_prototype: scans the raw source stream speculatively.
 * Saves/restores lring.src and lexer position state, using a private token buffer.
 * Never touches lring.tokens or lring.fill — the ring stays pristine.
 *
 * PRECONDITION: must be called with lring.head pointing at the current (un-
 * consumed) token, i.e. immediately after lexer_peek() and before any call
 * to lexer_next().  The speculative scan seeds its position state from
 * lring.tokens[lring.head], so advancing lring.head before calling this function
 * would cause the scratch scan to start from the wrong source position and
 * produce incorrect diagnostic locations. */
int
lexer_is_function_prototype(void)
{
	int saw_complex_or_imaginary = 0;

	/* Save the real lexer state first.
	 *
	 * lring.src normally points after the farthest token already materialized in the
	 * lookahead ring.  The speculative scan must start at the current token's
	 * source position, but restore lring.src to the real runtime position when done. */
	int saved_head = lring.head;
	int saved_fill = lring.fill;
	char *scan_src                 = (char *)lring.src_before[saved_head];
	char *saved_runtime_src        = (char *)lring.src;
	int saved_line                 = lpos.line;
	int saved_column               = lpos.column;
	int saved_pp_line              = lpos.pp_line;
	int saved_pp_column            = lpos.pp_column;
	int saved_filename_id          = lpos.filename_id;
	int saved_pp_filename_id       = lpos.pp_filename_id;

	/* read_token() derives the token location from the current lexer location.
	 * Seed the scratch scan from the buffered current token so diagnostics and
	 * #line-adjusted state remain coherent during speculative lexing. */
	int scan_line                  = lring.tokens[saved_head].line;
	int scan_column                = lring.tokens[saved_head].column;
	int scan_pp_line               = lring.tokens[saved_head].pp_line;
	int scan_pp_column             = lring.tokens[saved_head].pp_column;
	int scan_filename_id           = lring.tokens[saved_head].filename_id;
	int scan_pp_filename_id        = lring.tokens[saved_head].pp_filename_id;

	int result = 0;
	int failed = 0;

	/* Seed scratch from the current token's saved source position. */
	lring.src = scan_src;
	lpos.line = scan_line;
	lpos.column = scan_column;
	lpos.pp_line = scan_pp_line;
	lpos.pp_column = scan_pp_column;
	lpos.filename_id = scan_filename_id;
	lpos.pp_filename_id = scan_pp_filename_id;
	scratch_read_token();

	/* Skip storage/qualifier prefixes */
	while (SCRATCH.kind == TOK_STATIC || SCRATCH.kind == TOK_EXTERN ||
	        SCRATCH.kind == TOK_CONST || SCRATCH.kind == TOK_VOLATILE ||
	        SCRATCH.kind == TOK_RESTRICT || SCRATCH.kind == TOK_ATOMIC ||
	        SCRATCH.kind == TOK_INLINE || SCRATCH.kind == TOK_NORETURN)
		scratch_read_token();

	if (SCRATCH.kind == TOK_IDENT && SCRATCH.text &&
	    (STRCMP(SCRATCH.text, "_Complex") == 0 ||
	     STRCMP(SCRATCH.text, "_Imaginary") == 0)) {
		saw_complex_or_imaginary = 1;
		scratch_read_token();
		while (SCRATCH.kind == TOK_CONST || SCRATCH.kind == TOK_VOLATILE ||
		       SCRATCH.kind == TOK_RESTRICT || SCRATCH.kind == TOK_ATOMIC)
			scratch_read_token();
	}

	/* Match return type.  Keep this in sync with parser.c:parse_type_name().
	 * The original speculative prototype recognizer only knew about integer
	 * return types, so a header declaration such as:
	 *
	 *     double sin(double);
	 *
	 * was not recognized as a prototype.  The main parser then entered the
	 * function-definition path and reported "expected {, got ;".
	 */
	if (SCRATCH.kind == TOK_SIGNED || SCRATCH.kind == TOK_UNSIGNED ||
	        SCRATCH.kind == TOK_SHORT || SCRATCH.kind == TOK_LONG) {
		int saw_long = 0;
		while (SCRATCH.kind == TOK_SIGNED || SCRATCH.kind == TOK_UNSIGNED ||
		        SCRATCH.kind == TOK_SHORT || SCRATCH.kind == TOK_LONG) {
			if (SCRATCH.kind == TOK_LONG)
				saw_long = 1;
			scratch_read_token();
		}
		if (SCRATCH.kind == TOK_INT || SCRATCH.kind == TOK_CHAR ||
		        SCRATCH.kind == TOK_VOID || SCRATCH.kind == TOK_FLOAT ||
		        SCRATCH.kind == TOK_DOUBLE)
			scratch_read_token();
		else if (saw_long)
			; /* plain long */
		/* else: plain unsigned/signed/short — valid modifier-only type */
	} else if (SCRATCH.kind == TOK_STRUCT || SCRATCH.kind == TOK_UNION ||
	           SCRATCH.kind == TOK_ENUM) {
		scratch_read_token();
		if (SCRATCH.kind == TOK_IDENT)
			scratch_read_token();
	} else if (SCRATCH.kind == TOK_INT || SCRATCH.kind == TOK_CHAR ||
	           SCRATCH.kind == TOK_VOID || SCRATCH.kind == TOK_BOOL ||
	           SCRATCH.kind == TOK_FLOAT ||
	           SCRATCH.kind == TOK_DOUBLE || SCRATCH.kind == TOK_IDENT) {
		if (saw_complex_or_imaginary &&
		    SCRATCH.kind != TOK_FLOAT &&
		    SCRATCH.kind != TOK_DOUBLE)
			failed = 1;
		scratch_read_token();
	} else {
		failed = 1;
	}

	/* Skip pointer/const qualifiers */
	while (!failed && (SCRATCH.kind == TOK_STAR || SCRATCH.kind == TOK_CONST ||
	       SCRATCH.kind == TOK_VOLATILE || SCRATCH.kind == TOK_RESTRICT ||
	       SCRATCH.kind == TOK_ATOMIC))
		scratch_read_token();

	/* Accept reordered declaration specifiers between the base type and the
	 * function name, for example:
	 *
	 *   int inline f(void);
	 *   int _Noreturn g(void);
	 *   int extern h(void);
	 */
	while (!failed) {
		if (SCRATCH.kind == TOK_THREAD_LOCAL) {
			failed = 1;
			break;
		}

		if (SCRATCH.kind == TOK_STATIC || SCRATCH.kind == TOK_EXTERN ||
		    SCRATCH.kind == TOK_INLINE || SCRATCH.kind == TOK_NORETURN) {
			scratch_read_token();
			continue;
		}

		if (SCRATCH.kind == TOK_IDENT && SCRATCH.text &&
		    (STRCMP(SCRATCH.text, "_Alignas") == 0 ||
		     (tcc_lang_at_least(LANG_C23) &&
		      STRCMP(SCRATCH.text, "alignas") == 0))) {
			scratch_read_token();
			if (SCRATCH.kind != TOK_LPAREN) {
				failed = 1;
				break;
			}
			scratch_read_token();
			int adepth = 1;
			while (adepth > 0) {
				if (SCRATCH.kind == TOK_EOF) {
					failed = 1;
					break;
				}
				if (SCRATCH.kind == TOK_LPAREN)
					adepth++;
				else if (SCRATCH.kind == TOK_RPAREN)
					adepth--;
				scratch_read_token();
			}
			continue;
		}

		break;
	}

	/* Skip __attribute__((noreturn)) etc. between return type and function name */
	while (!failed && SCRATCH.kind == TOK_IDENT && SCRATCH.text &&
	        STRCMP(SCRATCH.text, "__attribute__") == 0) {
		scratch_read_token(); /* skip __attribute__ */
		if (SCRATCH.kind == TOK_LPAREN) {
			scratch_read_token(); /* skip first ( */
			int adepth = 1;
			while (adepth > 0) {
				if (SCRATCH.kind == TOK_EOF) {
					failed = 1;
					break;
				}
				if (SCRATCH.kind == TOK_LPAREN) adepth++;
				else if (SCRATCH.kind == TOK_RPAREN) adepth--;
				scratch_read_token();
			}
		}
	}

	/* Must see function name */
	if (!failed) {
		if (SCRATCH.kind != TOK_IDENT)
			failed = 1;
		else
			scratch_read_token();
	}

	/* Must see opening paren */
	if (!failed) {
		if (SCRATCH.kind != TOK_LPAREN)
			failed = 1;
		else
			scratch_read_token();
	}

	/* Skip to matching close paren */
	if (!failed) {
		int depth = 1;
		while (depth > 0) {
			if (SCRATCH.kind == TOK_EOF) {
				failed = 1;
				break;
			}
			if (SCRATCH.kind == TOK_LPAREN) depth++;
			else if (SCRATCH.kind == TOK_RPAREN) depth--;
			scratch_read_token();
		}
	}

	/* Skip __attribute__ */
	while (!failed && SCRATCH.kind == TOK_IDENT && SCRATCH.text &&
	        (STRCMP(SCRATCH.text, "__attribute__") == 0 ||
	         STRCMP(SCRATCH.text, "__attribute") == 0)) {
		scratch_read_token();
		if (SCRATCH.kind != TOK_LPAREN)
			break;
		scratch_read_token();
		int adepth = 1;
		while (adepth > 0) {
			if (SCRATCH.kind == TOK_EOF) {
				failed = 1;
				break;
			}
			if (SCRATCH.kind == TOK_LPAREN) adepth++;
			else if (SCRATCH.kind == TOK_RPAREN) adepth--;
			scratch_read_token();
		}
	}

	/* Accept comma too: "int f(int a), g(int a), a;" — comma-chained prototypes */
	if (!failed)
		result = (SCRATCH.kind == TOK_SEMI || SCRATCH.kind == TOK_COMMA);

	/* Restore everything — ring is untouched, lring.head was never changed */
	lring.head          = saved_head;
	lring.fill          = saved_fill;
	lring.src               = saved_runtime_src;
	lpos.line        = saved_line;
	lpos.column      = saved_column;
	lpos.pp_line     = saved_pp_line;
	lpos.pp_column   = saved_pp_column;
	lpos.filename_id    = saved_filename_id;
	lpos.pp_filename_id = saved_pp_filename_id;
	return result;
}

const char *
token_debug_name(TokenKind kind)
{
	switch (kind) {
	case TOK_EOF:
		return "TOK_EOF";
	case TOK_INT:
		return "TOK_INT";
	case TOK_CHAR:
		return "TOK_CHAR";
	case TOK_SHORT:
		return "TOK_SHORT";
	case TOK_LONG:
		return "TOK_LONG";
	case TOK_SIGNED:
		return "TOK_SIGNED";
	case TOK_UNSIGNED:
		return "TOK_UNSIGNED";
	case TOK_VOID:
		return "TOK_VOID";
	case TOK_BOOL:
		return "TOK_BOOL";
	case TOK_STRUCT:
		return "TOK_STRUCT";
	case TOK_UNION:
		return "TOK_UNION";
	case TOK_ENUM:
		return "TOK_ENUM";
	case TOK_SIZEOF:
		return "TOK_SIZEOF";
	case TOK_ALIGNOF:
		return "TOK_ALIGNOF";
	case TOK_NORETURN:
		return "TOK_NORETURN";
	case TOK_RETURN:
		return "TOK_RETURN";
	case TOK_IF:
		return "TOK_IF";
	case TOK_ELSE:
		return "TOK_ELSE";
	case TOK_WHILE:
		return "TOK_WHILE";
	case TOK_FOR:
		return "TOK_FOR";
	case TOK_DO:
		return "TOK_DO";
	case TOK_BREAK:
		return "TOK_BREAK";
	case TOK_CONTINUE:
		return "TOK_CONTINUE";
	case TOK_TYPEDEF:
		return "TOK_TYPEDEF";
	case TOK_AUTO:
		return "TOK_AUTO";
	case TOK_STATIC:
		return "TOK_STATIC";
	case TOK_EXTERN:
		return "TOK_EXTERN";
	case TOK_REGISTER:
		return "TOK_REGISTER";
	case TOK_THREAD_LOCAL:
		return "TOK_THREAD_LOCAL";
	case TOK_CONST:
		return "TOK_CONST";
	case TOK_VOLATILE:
		return "TOK_VOLATILE";
	case TOK_RESTRICT:
		return "TOK_RESTRICT";
	case TOK_ATOMIC:
		return "TOK_ATOMIC";
	case TOK_GOTO:
		return "TOK_GOTO";
	case TOK_INLINE:
		return "TOK_INLINE";
	case TOK_ASM:
		return "TOK_ASM";
	case TOK_SWITCH:
		return "TOK_SWITCH";
	case TOK_CASE:
		return "TOK_CASE";
	case TOK_DEFAULT:
		return "TOK_DEFAULT";
	case TOK_IDENT:
		return "TOK_IDENT";
	case TOK_NUM:
		return "TOK_NUM";
	case TOK_STRING:
		return "TOK_STRING";
	case TOK_LPAREN:
		return "TOK_LPAREN";
	case TOK_RPAREN:
		return "TOK_RPAREN";
	case TOK_LBRACE:
		return "TOK_LBRACE";
	case TOK_RBRACE:
		return "TOK_RBRACE";
	case TOK_LBRACKET:
		return "TOK_LBRACKET";
	case TOK_RBRACKET:
		return "TOK_RBRACKET";
	case TOK_SEMI:
		return "TOK_SEMI";
	case TOK_COMMA:
		return "TOK_COMMA";
	case TOK_COLON:
		return "TOK_COLON";
	case TOK_QUESTION:
		return "TOK_QUESTION";
	case TOK_DOT:
		return "TOK_DOT";
	case TOK_ARROW:
		return "TOK_ARROW";
	case TOK_ASSIGN:
		return "TOK_ASSIGN";
	case TOK_PLUSEQ:
		return "TOK_PLUSEQ";
	case TOK_MINUSEQ:
		return "TOK_MINUSEQ";
	case TOK_MULEQ:
		return "TOK_MULEQ";
	case TOK_DIVEQ:
		return "TOK_DIVEQ";
	case TOK_MODEQ:
		return "TOK_MODEQ";
	case TOK_ANDEQ:
		return "TOK_ANDEQ";
	case TOK_OREQ:
		return "TOK_OREQ";
	case TOK_XOREQ:
		return "TOK_XOREQ";
	case TOK_SHLEQ:
		return "TOK_SHLEQ";
	case TOK_SHREQ:
		return "TOK_SHREQ";
	case TOK_PLUSPLUS:
		return "TOK_PLUSPLUS";
	case TOK_MINUSMINUS:
		return "TOK_MINUSMINUS";
	case TOK_PLUS:
		return "TOK_PLUS";
	case TOK_MINUS:
		return "TOK_MINUS";
	case TOK_STAR:
		return "TOK_STAR";
	case TOK_SLASH:
		return "TOK_SLASH";
	case TOK_PERCENT:
		return "TOK_PERCENT";
	case TOK_PIPE:
		return "TOK_PIPE";
	case TOK_CARET:
		return "TOK_CARET";
	case TOK_SHL:
		return "TOK_SHL";
	case TOK_SHR:
		return "TOK_SHR";
	case TOK_EQ:
		return "TOK_EQ";
	case TOK_NE:
		return "TOK_NE";
	case TOK_LT:
		return "TOK_LT";
	case TOK_LE:
		return "TOK_LE";
	case TOK_GT:
		return "TOK_GT";
	case TOK_GE:
		return "TOK_GE";
	case TOK_AND:
		return "TOK_AND";
	case TOK_OR:
		return "TOK_OR";
	case TOK_AMP:
		return "TOK_AMP";
	case TOK_NOT:
		return "TOK_NOT";
	default:
		return "TOK_UNKNOWN";
	}
}
