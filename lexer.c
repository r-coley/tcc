#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tcc.h"
#include "lexer.h"
extern int preprocess_current_line;

static const char *src;
/* Token ring buffer - 8 slots, filled on demand, never copies tokens by value */
#define TOK_RING_SIZE 65
#define TOK_RING_MASK 63
#define TOK_SCRATCH_SLOT 64  /* dedicated scratch slot for speculative scanning */
static Token tok_ring[TOK_RING_SIZE];
static const char *tok_src_after[TOK_RING_SIZE];  /* src position AFTER each ring slot */
static const char *tok_src_before[TOK_RING_SIZE]; /* src position BEFORE each ring slot */
static int   tok_head = 0;   /* index of current (peek[0]) token */
static int   tok_fill = 0;   /* how many valid slots are filled ahead of head */

/* Make internal read_token/set_token_location code transparently fill tok_ring[tok_head] */
#define current tok_ring[tok_head]
static int lexer_line = 1;
static int lexer_column = 1;
static int lexer_pp_line = 1;
static int lexer_pp_column = 1;
static int lexer_filename_id = 0;
static int lexer_pp_filename_id = 0;

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
static char **token_text_arena = NULL;
static int token_text_count = 0;
static int token_text_cap = 0;

static void read_token(void);
static char *lexer_save_token_text(const char *s);
static void lexer_reset_token_texts(void);

typedef struct FilenameEntry {
	const char *name;
} FilenameEntry;

static FilenameEntry interned_filenames[2048];
static int interned_filename_count = 0;

static void
init_filename_table(void)
{
	if (interned_filename_count == 0) {
		interned_filenames[0].name = "<input>";
		interned_filename_count = 1;
	}
}

void 
lexer_init(const char *source)
{
	lexer_reset_token_texts();
	src = source;
	lexer_line = 1;
	lexer_column = 1;
	lexer_pp_line = 1;
	lexer_pp_column = 1;
	tok_head = 0;
	tok_fill = 0;
	/* Prime slot 0 */
	tok_src_before[tok_head] = src;
	read_token();
	tok_src_after[tok_head] = src;
	tok_fill = 1;
}

int
lexer_filename_intern(const char *name)
{
	int i;
	size_t len;
	char *copy;

	init_filename_table();

	if (name == 0)
		name = "<input>";

	for (i = 0; i < interned_filename_count; i++) {
		if (STRCMP(interned_filenames[i].name, name) == 0)
			return i;
	}

	if (interned_filename_count >= 2048) {
		ICE("too many filenames interned (limit 2048)");
	}

	len = strlen(name);
	copy = xmalloc(len + 1);
	memcpy(copy, name, len + 1);

	interned_filenames[interned_filename_count].name = copy;
	return interned_filename_count++;
}

const char *
lexer_filename_name(int id)
{
	init_filename_table();

	if (id < 0 || id >= interned_filename_count)
		return "<invalid>";

	return interned_filenames[id].name;
}

static void
lexer_reset_token_texts(void)
{
	for (int i = 0; i < token_text_count; i++)
		xfree(token_text_arena[i]);

	token_text_count = 0;
}

static char *
lexer_save_token_text(const char *s)
{
	char *copy;

	if (s == 0)
		s = "";

	if (token_text_count >= token_text_cap) {
		int new_cap = token_text_cap ? token_text_cap * 2 : 1024;
		char **new_arena = xrealloc(token_text_arena, sizeof(char *) * (size_t)new_cap);
		token_text_arena = new_arena;
		token_text_cap = new_cap;
	}

	copy = xstrdup(s);
	token_text_arena[token_text_count++] = copy;
	return copy;
}

static void 
token_set_text(const char *s)
{
	/* Silently overwrite: double-sets occur for __FILE__ expansion. */
	current.text = lexer_save_token_text(s ? s : "");
}

static void 
set_logical_filename(const char *name)
{
	lexer_filename_id = lexer_filename_intern(name);
}

static int 
try_consume_line_directive(void)
{
	const char *p = src;

	if (*p != '#' || lexer_column != 1)
		return 0;

	p++;
	while (*p == ' ' || *p == '\t')
		p++;

	if (strncmp(p, "line", 4) == 0 && (p[4] == ' ' || p[4] == '\t'))
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
		set_logical_filename(name);
	}

	while (*p && *p != '\n')
		p++;

	if (*p == '\n') {
		p++;
		lexer_pp_line++;
	}

	src = p;
	lexer_line = line;
	lexer_column = 1;
	lexer_pp_column = 1;
	return 1;
}

static void 
advance_char(void)
{
	if (*src == '\n') {
		lexer_line++;
		lexer_column = 1;
		lexer_pp_line++;
		lexer_pp_column = 1;
	} else if (*src) {
		lexer_column++;
		lexer_pp_column++;
	}
	src++;
}

static void 
set_token_location(void)
{
	current.filename_id = lexer_filename_id;
	current.line = lexer_line;
	current.column = lexer_column;

	current.pp_filename_id = lexer_pp_filename_id;
	current.pp_line = lexer_pp_line;
	current.pp_column = lexer_pp_column;
}

static void 
read_token(void)
{
	/* Initialse current token */
	memset(&current,0,sizeof(current));
	/*current.text = 0;
	current.value = 0;*/

	for (;;) {
		while (isspace((unsigned char)*src))
			advance_char();

		if (try_consume_line_directive() == 0)
			break;
	}

	set_token_location();

	if (*src == '\0') {
		current.kind = TOK_EOF;
		return;
	}

	if (*src == '~') {
		current.kind = TOK_TILDE;
		advance_char();
		return;
	}

	if (*src == '\'') {
		advance_char();

		int value;

		if (*src == '\\') {
			advance_char();
			switch (*src) {
			case 'n':
				value = '\n';
				break;
			case 'r':
				value = '\r';
				break;
			case 't':
				value = '\t';
				break;
			case '0': case '1': case '2': case '3':
			case '4': case '5': case '6': case '7': {
				/* octal escape: src points at first digit (already advanced past \) */
				int _ov = *src - '0';
				advance_char();
				if (*src >= '0' && *src <= '7') {
					_ov = _ov * 8 + (*src - '0'); advance_char();
					if (*src >= '0' && *src <= '7') {
						_ov = _ov * 8 + (*src - '0'); advance_char();
					}
				}
				value = _ov;
				goto char_esc_done;
			}
			case 'x': case 'X': {
				/* hex escape: \xNN -- src is at 'x', already advanced past \ */
				advance_char(); /* skip 'x' */
				int _hv = 0;
				while ((*src >= '0' && *src <= '9') ||
				       (*src >= 'a' && *src <= 'f') ||
				       (*src >= 'A' && *src <= 'F')) {
					_hv *= 16;
					if (*src >= '0' && *src <= '9') _hv += *src - '0';
					else if (*src >= 'a') _hv += *src - 'a' + 10;
					else _hv += *src - 'A' + 10;
					advance_char();
				}
				value = _hv;
				goto char_esc_done;
			}
			case 'a':
				value = '\a';
				break;
			case 'b':
				value = '\b';
				break;
			case 'f':
				value = '\f';
				break;
			case 'v':
				value = '\v';
				break;
			case '\\':
				value = '\\';
				break;
			case '\'':
				value = '\'';
				break;
			case '"':
				value = '"';
				break;
			default:
				value = *src;
				break;
			}
			advance_char();
			char_esc_done: ;
		} else {
			value = *src;
			advance_char();
		}

		if (*src != '\'') {
			fatal_lex(lexer_filename_name(lexer_filename_id), lexer_line, 0, "Unterminated char literal\n");
		}
		advance_char();

		current.kind = TOK_NUM;
		current.value = value;
		return;
	}

	if (*src == '"') {
		char text[4096];
		int i = 0;
		advance_char();

		while (*src && '"' != *src) {
			if (*src == '\\' && src[1]) {
				char esc;
				advance_char();
				switch (*src) {
				case 'n':
					esc = '\n';
					break;
				case 'r':
					esc = '\r';
					break;
				case 't':
					esc = '\t';
					break;
				case '0': case '1': case '2': case '3':
				case '4': case '5': case '6': case '7': {
					/* octal: \0, \1, \12, \123 */
					int _ov = *src - '0';
					advance_char();
					if (*src >= '0' && *src <= '7') {
						_ov = _ov * 8 + (*src - '0'); advance_char();
						if (*src >= '0' && *src <= '7') {
							_ov = _ov * 8 + (*src - '0'); advance_char();
						}
					}
					if (i < (int)sizeof(text) - 1) text[i++] = (char)_ov;
					continue; /* skip the normal esc handling below */
				}
				case 'a':
					esc = '\a';
					break;
				case 'b':
					esc = '\b';
					break;
				case 'f':
					esc = '\f';
					break;
				case 'v':
					esc = '\v';
					break;
				case '\\':
					esc = '\\';
					break;
				case '"':
					esc = '"';
					break;
				case '\'':
					esc = '\'';
					break;
				default:
					esc = *src;
					break;
				}
				if (i < (int)sizeof(text) - 1)
					text[i++] = esc;
				advance_char();
				continue;
			}

			if (i < (int)sizeof(text) - 1)
				text[i++] = *src;
			advance_char();
		}

		if (*src != '"') {
			fatal_lex(lexer_filename_name(lexer_filename_id), lexer_line, 0, "Unterminated string literal\n");
		}

		advance_char();
		text[i] = '\0';

		/* Adjacent string literal concatenation: "a" "b" -> "ab" */
		while (1) {
			/* skip whitespace between string tokens */
			const char *peek = src;
			while (*peek && (*peek == ' ' || *peek == '\t' ||
			                *peek == '\n' || *peek == '\r'))
				peek++;
			/* handle line directives between strings */
			if (*peek != '"') break;
			/* advance src to the next " */
			while (src != peek) advance_char();
			advance_char(); /* consume opening " */
			while (*src && *src != '"') {
				if (*src == '\\' && src[1]) {
					char esc2;
					advance_char();
					switch (*src) {
					case 'n': esc2 = '\n'; break;
					case 'r': esc2 = '\r'; break;
					case 't': esc2 = '\t'; break;
					case '0': case '1': case '2': case '3':
					case '4': case '5': case '6': case '7': {
						int _ov2 = *src - '0'; advance_char();
						if (*src >= '0' && *src <= '7') {
							_ov2 = _ov2*8 + (*src-'0'); advance_char();
							if (*src >= '0' && *src <= '7') {
								_ov2 = _ov2*8 + (*src-'0'); advance_char();
							}
						}
						if (i < (int)sizeof(text) - 2) text[i++] = (char)_ov2;
						continue;
					}
					case '\\': esc2 = '\\'; break;
				case '"': esc2 = '"'; break;
				case '\'': esc2 = '\''; break;
					default: esc2 = *src; break;
					}
					if (i < (int)sizeof(text) - 2) text[i++] = esc2;
					advance_char();
				} else {
					if (i < (int)sizeof(text) - 2) text[i++] = *src;
					advance_char();
				}
			}
			if (*src == '"') advance_char(); /* consume closing " */
		}
		text[i] = '\0';

		token_set_text(text);
		current.kind = TOK_STRING;
		return;
	}

	if (isdigit((unsigned char)*src)) {
		int value = 0;

		if (src[0] == '0' && (src[1] == 'x' || src[1] == 'X')) {
			advance_char(); /* 0 */
			advance_char(); /* x */
			while (isxdigit((unsigned char)*src)) {
				int digit;
				if (*src >= '0' && *src <= '9')
					digit = *src - '0';
				else if (*src >= 'a' && *src <= 'f')
					digit = *src - 'a' + 10;
				else
					digit = *src - 'A' + 10;
				value = value * 16 + digit;
				advance_char();
			}
		} else {
			while (isdigit((unsigned char)*src)) {
				value = value * 10 + (*src - '0');
				advance_char();
			}
		}

		/* consume float fractional part: 100.0, 3.14 */
		if (*src == '.') {
			advance_char(); /* consume . */
			while (isdigit((unsigned char)*src))
				advance_char();
		}
		/* consume float exponent: 1e10, 2.5E-3 */
		if (*src == 'e' || *src == 'E') {
			advance_char();
			if (*src == '+' || *src == '-') advance_char();
			while (isdigit((unsigned char)*src)) advance_char();
		}
		/* consume float/int suffixes: f, F, u, U, l, L, ll, LL etc. */
		while (*src == 'u' || *src == 'U' || *src == 'l' || *src == 'L' ||
		       *src == 'f' || *src == 'F')
			advance_char();

		current.kind = TOK_NUM;
		current.value = value;
		return;
	}

	if (isalpha((unsigned char)*src) || *src == '_') {
		char text[256];
		int i = 0;

		while (isalnum((unsigned char)*src) || *src == '_') {
			if (i < (int)sizeof(text) - 1)
				text[i++] = *src;
			advance_char();
		}

		text[i] = '\0';
		token_set_text(text);

		/* Wide character/string literals: L'x' and L"str" — treat as regular */
		if (i == 1 && text[0] == 'L') {
			if (*src == '\'') {
				/* L'\0' etc. — lex as regular char literal */
				advance_char(); /* consume ' */
				int value = 0;
				if (*src == '\\' && src[1]) {
					advance_char();
					switch (*src) {
						case 'n': value = '\n'; break;
						case 't': value = '\t'; break;
						case 'r': value = '\r'; break;
						case '0': value = '\0'; break;
						case '\\': value = '\\'; break;
						case '\'': value = '\''; break;
						case 'a': value = '\a'; break;
						case 'b': value = '\b'; break;
						default: value = *src; break;
					}
					advance_char();
				} else {
					value = *src;
					advance_char();
				}
				advance_char(); /* consume closing ' */
				current.kind = TOK_NUM;
				current.value = value;
				return;
			} else if (*src == '"') {
				/* L"str" — lex as regular string literal */
				/* Fall through to string literal code by resetting src back... 
				 * Actually just lex the string here: */
				advance_char(); /* consume " */
				static char str_buf[4096];
				int si = 0;
				while (*src && *src != '"') {
					if (*src == '\\' && src[1]) {
						advance_char();
						char c;
						switch (*src) {
							case 'n': c = '\n'; break;
							case 't': c = '\t'; break;
							case 'r': c = '\r'; break;
							case '0': c = '\0'; break;
							case '\\': c = '\\'; break;
							case '"': c = '"'; break;
							default: c = *src; break;
						}
						if (si < 4095) str_buf[si++] = c;
					} else {
						if (si < 4095) str_buf[si++] = *src;
					}
					advance_char();
				}
				if (*src == '"') advance_char();
				str_buf[si] = '\0';
				current.kind = TOK_STRING;
				token_set_text(str_buf);
				return;
			}
		}

		if (STRCMP(current.text, "int") == 0)
			current.kind = TOK_INT;
		else if (STRCMP(current.text, "char") == 0)
			current.kind = TOK_CHAR;
		else if (STRCMP(current.text, "short") == 0)
			current.kind = TOK_SHORT;
		else if (STRCMP(current.text, "long") == 0)
			current.kind = TOK_LONG;
		else if (STRCMP(current.text, "double") == 0)
			current.kind = TOK_DOUBLE;
		else if (STRCMP(current.text, "float") == 0)
			current.kind = TOK_FLOAT;
		else if (STRCMP(current.text, "signed") == 0)
			current.kind = TOK_SIGNED;
		else if (STRCMP(current.text, "unsigned") == 0)
			current.kind = TOK_UNSIGNED;
		else if (STRCMP(current.text, "void") == 0)
			current.kind = TOK_VOID;
		else if (STRCMP(current.text, "_Bool") == 0)
			current.kind = TOK_BOOL;
		else if (STRCMP(current.text, "struct") == 0)
			current.kind = TOK_STRUCT;
		else if (STRCMP(current.text, "union") == 0)
			current.kind = TOK_UNION;
		else if (STRCMP(current.text, "enum") == 0)
			current.kind = TOK_ENUM;
		else if (STRCMP(current.text, "sizeof") == 0)
			current.kind = TOK_SIZEOF;
		else if (STRCMP(current.text, "return") == 0)
			current.kind = TOK_RETURN;
		else if (STRCMP(current.text, "if") == 0)
			current.kind = TOK_IF;
		else if (STRCMP(current.text, "else") == 0)
			current.kind = TOK_ELSE;
		else if (STRCMP(current.text, "while") == 0)
			current.kind = TOK_WHILE;
		else if (STRCMP(current.text, "for") == 0)
			current.kind = TOK_FOR;
		else if (STRCMP(current.text, "do") == 0)
			current.kind = TOK_DO;
		else if (STRCMP(current.text, "break") == 0)
			current.kind = TOK_BREAK;
		else if (STRCMP(current.text, "continue") == 0)
			current.kind = TOK_CONTINUE;
		else if (STRCMP(current.text, "typedef") == 0)
			current.kind = TOK_TYPEDEF;
		else if (STRCMP(current.text, "static") == 0)
			current.kind = TOK_STATIC;
		else if (STRCMP(current.text, "extern") == 0)
			current.kind = TOK_EXTERN;
		else if (STRCMP(current.text, "const") == 0)
			current.kind = TOK_CONST;
		else if (STRCMP(current.text, "inline") == 0)
			current.kind = TOK_INLINE;
		else if (STRCMP(current.text, "goto") == 0)
			current.kind = TOK_GOTO;
		else if (STRCMP(current.text, "__inline") == 0 ||
		         STRCMP(current.text, "__inline__") == 0)
			current.kind = TOK_INLINE;
		else if (STRCMP(current.text, "asm") == 0 ||
		         STRCMP(current.text, "__asm") == 0 || STRCMP(current.text, "__asm__") == 0)
			current.kind = TOK_ASM;
		else if (STRCMP(current.text, "switch") == 0)
			current.kind = TOK_SWITCH;
		else if (STRCMP(current.text, "case") == 0)
			current.kind = TOK_CASE;
		else if (STRCMP(current.text, "default") == 0)
			current.kind = TOK_DEFAULT;
		else if (STRCMP(current.text, "__LINE__") == 0) {
			current.kind = TOK_NUM;
			current.value = preprocess_current_line;
			/*current.value = current.line; RICH*/
		} else if (STRCMP(current.text, "__FILE__") == 0) {
			current.kind = TOK_STRING;
			current.text = 0; /* clear before re-setting for __FILE__ expansion */
			token_set_text(lexer_filename_name(lexer_filename_id));
		} else
			current.kind = TOK_IDENT;

		return;
	}

	if (src[0] == '+' && src[1] == '+') {
		advance_char();
		advance_char();
		current.kind = TOK_PLUSPLUS;
		return;
	}

	if (src[0] == '-' && src[1] == '-') {
		advance_char();
		advance_char();
		current.kind = TOK_MINUSMINUS;
		return;
	}

	if (src[0] == '+' && src[1] == '=') {
		advance_char();
		advance_char();
		current.kind = TOK_PLUSEQ;
		return;
	}

	if (src[0] == '-' && src[1] == '=') {
		advance_char();
		advance_char();
		current.kind = TOK_MINUSEQ;
		return;
	}

	if (src[0] == '*' && src[1] == '=') {
		advance_char();
		advance_char();
		current.kind = TOK_MULEQ;
		return;
	}

	if (src[0] == '/' && src[1] == '=') {
		advance_char();
		advance_char();
		current.kind = TOK_DIVEQ;
		return;
	}

	if (src[0] == '%' && src[1] == '=') {
		advance_char();
		advance_char();
		current.kind = TOK_MODEQ;
		return;
	}

	if (src[0] == '<' && src[1] == '<' && src[2] == '=') {
		advance_char();
		advance_char();
		advance_char();
		current.kind = TOK_SHLEQ;
		return;
	}

	if (src[0] == '>' && src[1] == '>' && src[2] == '=') {
		advance_char();
		advance_char();
		advance_char();
		current.kind = TOK_SHREQ;
		return;
	}

	if (src[0] == '<' && src[1] == '<') {
		advance_char();
		advance_char();
		current.kind = TOK_SHL;
		return;
	}

	if (src[0] == '>' && src[1] == '>') {
		advance_char();
		advance_char();
		current.kind = TOK_SHR;
		return;
	}

	if (src[0] == '&' && src[1] == '=') {
		advance_char();
		advance_char();
		current.kind = TOK_ANDEQ;
		return;
	}

	if (src[0] == '|' && src[1] == '=') {
		advance_char();
		advance_char();
		current.kind = TOK_OREQ;
		return;
	}

	if (src[0] == '^' && src[1] == '=') {
		advance_char();
		advance_char();
		current.kind = TOK_XOREQ;
		return;
	}

	if (src[0] == '=' && src[1] == '=') {
		advance_char();
		advance_char();
		current.kind = TOK_EQ;
		return;
	}

	if (src[0] == '!' && src[1] == '=') {
		advance_char();
		advance_char();
		current.kind = TOK_NE;
		return;
	}

	if (src[0] == '<' && src[1] == '=') {
		advance_char();
		advance_char();
		current.kind = TOK_LE;
		return;
	}

	if (src[0] == '>' && src[1] == '=') {
		advance_char();
		advance_char();
		current.kind = TOK_GE;
		return;
	}

	if (src[0] == '&' && src[1] == '&') {
		advance_char();
		advance_char();
		current.kind = TOK_AND;
		return;
	}

	if (src[0] == '|' && src[1] == '|') {
		advance_char();
		advance_char();
		current.kind = TOK_OR;
		return;
	}

	if (src[0] == '-' && src[1] == '>') {
		advance_char();
		advance_char();
		current.kind = TOK_ARROW;
		return;
	}

	char ch = *src;
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
		fatal_lex(lexer_filename_name(lexer_filename_id), lexer_line, lexer_column, "Unexpected character: '%c'\n", ch);
	}
}

void 
lexer_set_filename(const char *name)
{
	int interned = lexer_filename_intern(name);

	lexer_filename_id = interned;
	lexer_pp_filename_id = interned;
}

/* ----------- Ring buffer helpers ----------- */

/*
 * fill_ahead: ensure tok_ring[(tok_head + n) & MASK] is valid.
 * Reads tokens into ring slots ahead of tok_head without advancing tok_head.
 * Uses a temporary save/restore of lexer state because read_token fills
 * tok_ring[tok_head] via the `current` macro - we need to temporarily
 * redirect it to the target slot.
 */
static void
fill_ahead(int n)
{
	int i;
	if (n < tok_fill)
		return; /* already filled */

	/* Read (n - tok_fill + 1) more tokens into ring slots ahead of tok_head.
	 * read_token fills tok_ring[tok_head] via the `current` macro, so we
	 * temporarily redirect tok_head to each target slot.
	 * We do NOT restore src/line/col — after filling, src stays pointing
	 * just after the last filled token, ready for the next lexer_next() call. */
	int real_head = tok_head;
	for (i = tok_fill; i <= n; i++) {
		tok_head = (real_head + i) & TOK_RING_MASK;
		tok_src_before[tok_head] = src;
		read_token();
		tok_src_after[tok_head] = src;
	}
	tok_head = real_head;
	tok_fill = n + 1;
}

/* ----------- Public lexer API ----------- */

const Token *
lexer_peek(void)
{
	return &tok_ring[tok_head];
}

const Token *
lexer_peek_ahead(int n)
{
	/* n=0 is current, n=1 is next, etc. */
	if (n >= tok_fill)
		fill_ahead(n);
	return &tok_ring[(tok_head + n) & TOK_RING_MASK];
}

const Token *
lexer_next(void)
{
	const Token *old = &tok_ring[tok_head];
	/* Advance head */
	tok_head = (tok_head + 1) & TOK_RING_MASK;
	tok_fill--;
	if (tok_fill < 0) tok_fill = 0;
	/* If ring is empty at new head, fill it */
	if (tok_fill == 0) {
		tok_src_before[tok_head] = src;
		read_token();
		tok_src_after[tok_head] = src;
		tok_fill = 1;
	}
	return old;
}

/* Convenience wrappers kept for compatibility */
const Token *lexer_peek_next(void)       { return lexer_peek_ahead(1); }
const Token *lexer_peek_after_next(void) { return lexer_peek_ahead(2); }
const Token *lexer_peek_third(void)      { return lexer_peek_ahead(3); }
const Token *lexer_peek_fourth(void)     { return lexer_peek_ahead(4); }
const Token *lexer_peek_fifth(void)      { return lexer_peek_ahead(5); }

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

/* lexer_is_function_prototype: scans the raw source stream speculatively.
 * Saves/restores src and lexer position state, using a private token buffer.
 * Never touches tok_ring or tok_fill — the ring stays pristine. */
int
lexer_is_function_prototype(void)
{
	/* Save the real lexer state first.
	 *
	 * src normally points after the farthest token already materialized in the
	 * lookahead ring.  The speculative scan must start at the current token's
	 * source position, but restore src to the real runtime position when done. */
	int saved_head = tok_head;
	int saved_fill = tok_fill;
	const char *scan_src           = tok_src_before[saved_head];
	const char *saved_runtime_src  = src;
	int saved_line                 = lexer_line;
	int saved_column               = lexer_column;
	int saved_pp_line              = lexer_pp_line;
	int saved_pp_column            = lexer_pp_column;
	int saved_filename_id          = lexer_filename_id;
	int saved_pp_filename_id       = lexer_pp_filename_id;

	/* read_token() derives the token location from the current lexer location.
	 * Seed the scratch scan from the buffered current token so diagnostics and
	 * #line-adjusted state remain coherent during speculative lexing. */
	int scan_line                  = tok_ring[saved_head].line;
	int scan_column                = tok_ring[saved_head].column;
	int scan_pp_line               = tok_ring[saved_head].pp_line;
	int scan_pp_column             = tok_ring[saved_head].pp_column;
	int scan_filename_id           = tok_ring[saved_head].filename_id;
	int scan_pp_filename_id        = tok_ring[saved_head].pp_filename_id;

	/* Use a scratch slot that is NOT part of the ring.
	 * Temporarily redirect tok_head to this slot for read_token() calls. */
	tok_head = TOK_SCRATCH_SLOT; /* dedicated scratch slot, never in normal ring */
	tok_fill = 0;

#undef current
#define current tok_ring[tok_head]

	/* Seed scratch from the current token's saved source position. */
	src = scan_src;
	lexer_line = scan_line;
	lexer_column = scan_column;
	lexer_pp_line = scan_pp_line;
	lexer_pp_column = scan_pp_column;
	lexer_filename_id = scan_filename_id;
	lexer_pp_filename_id = scan_pp_filename_id;
	read_token();
	Debug(1, "IS_PROTO: scratch start: kind=%d text=%s\n",
	      (int)current.kind, current.text ? current.text : "<null>");

	int result = 0;

	/* Skip storage/qualifier prefixes */
	while (current.kind == TOK_STATIC || current.kind == TOK_EXTERN ||
	        current.kind == TOK_CONST || current.kind == TOK_INLINE)
		read_token();

	/* Match return type.  Keep this in sync with parser.c:parse_type_name().
	 * The original speculative prototype recognizer only knew about integer
	 * return types, so a header declaration such as:
	 *
	 *     double sin(double);
	 *
	 * was not recognized as a prototype.  The main parser then entered the
	 * function-definition path and reported "expected {, got ;".
	 */
	if (current.kind == TOK_SIGNED || current.kind == TOK_UNSIGNED ||
	        current.kind == TOK_SHORT || current.kind == TOK_LONG) {
		int saw_long = 0;
		while (current.kind == TOK_SIGNED || current.kind == TOK_UNSIGNED ||
		        current.kind == TOK_SHORT || current.kind == TOK_LONG) {
			if (current.kind == TOK_LONG)
				saw_long = 1;
			read_token();
		}
		if (current.kind == TOK_INT || current.kind == TOK_CHAR ||
		        current.kind == TOK_VOID || current.kind == TOK_FLOAT ||
		        current.kind == TOK_DOUBLE)
			read_token();
		else if (saw_long)
			; /* plain long */
		else
			goto done;
	} else if (current.kind == TOK_STRUCT || current.kind == TOK_UNION ||
	           current.kind == TOK_ENUM) {
		read_token();
		if (current.kind == TOK_IDENT)
			read_token();
	} else if (current.kind == TOK_INT || current.kind == TOK_CHAR ||
	           current.kind == TOK_VOID || current.kind == TOK_FLOAT ||
	           current.kind == TOK_DOUBLE || current.kind == TOK_IDENT) {
		read_token();
	} else {
		goto done;
	}

	/* Skip pointer/const qualifiers */
	while (current.kind == TOK_STAR || current.kind == TOK_CONST)
		read_token();

	/* Skip __attribute__((noreturn)) etc. between return type and function name */
	while (current.kind == TOK_IDENT && current.text &&
	        STRCMP(current.text, "__attribute__") == 0) {
		read_token(); /* skip __attribute__ */
		if (current.kind == TOK_LPAREN) {
			read_token(); /* skip first ( */
			int adepth = 1;
			while (adepth > 0) {
				if (current.kind == TOK_EOF) goto done;
				if (current.kind == TOK_LPAREN) adepth++;
				else if (current.kind == TOK_RPAREN) adepth--;
				read_token();
			}
		}
	}

	/* Must see function name */
	if (current.kind != TOK_IDENT)
		goto done;
	read_token();

	/* Must see opening paren */
	if (current.kind != TOK_LPAREN)
		goto done;
	read_token();

	/* Skip to matching close paren */
	{
		int depth = 1;
		while (depth > 0) {
			if (current.kind == TOK_EOF)
				goto done;
			if (current.kind == TOK_LPAREN) depth++;
			else if (current.kind == TOK_RPAREN) depth--;
			read_token();
		}
	}

	/* Skip __attribute__ */
	while (current.kind == TOK_IDENT && current.text &&
	        (STRCMP(current.text, "__attribute__") == 0 ||
	         STRCMP(current.text, "__attribute") == 0)) {
		read_token();
		if (current.kind != TOK_LPAREN)
			break;
		read_token();
		int adepth = 1;
		while (adepth > 0) {
			if (current.kind == TOK_EOF) goto done;
			if (current.kind == TOK_LPAREN) adepth++;
			else if (current.kind == TOK_RPAREN) adepth--;
			read_token();
		}
	}

	/* Accept comma too: "int f(int a), g(int a), a;" — comma-chained prototypes */
	result = (current.kind == TOK_SEMI || current.kind == TOK_COMMA);
	Debug(1, "IS_PROTO: result=%d (current kind=%d)\n", result, (int)current.kind);

done:
	/* Restore everything — ring is untouched */
	tok_head          = saved_head;
	tok_fill          = saved_fill;
	src               = saved_runtime_src;
	lexer_line        = saved_line;
	lexer_column      = saved_column;
	lexer_pp_line     = saved_pp_line;
	lexer_pp_column   = saved_pp_column;
	lexer_filename_id    = saved_filename_id;
	lexer_pp_filename_id = saved_pp_filename_id;

#undef current
#define current tok_ring[tok_head]
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
	case TOK_STATIC:
		return "TOK_STATIC";
	case TOK_EXTERN:
		return "TOK_EXTERN";
	case TOK_CONST:
		return "TOK_CONST";
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

