#ifndef LEXER_H
#define LEXER_H

typedef enum {
	TOK_EOF,

	TOK_INT,
	TOK_CHAR,
	TOK_SHORT,
	TOK_LONG,
	TOK_DOUBLE,
	TOK_FLOAT,
	TOK_SIGNED,
	TOK_UNSIGNED,
	TOK_VOID,
	TOK_BOOL,
	TOK_STRUCT,
	TOK_UNION,
	TOK_ENUM,
	TOK_SIZEOF,
	TOK_RETURN,
	TOK_IF,
	TOK_ELSE,
	TOK_WHILE,
	TOK_FOR,
	TOK_DO,
	TOK_BREAK,
	TOK_CONTINUE,
	TOK_TYPEDEF,
	TOK_STATIC,
	TOK_EXTERN,
	TOK_CONST,
	TOK_GOTO,
	TOK_INLINE,
	TOK_ASM,
	TOK_SWITCH,
	TOK_CASE,
	TOK_DEFAULT,

	TOK_IDENT,
	TOK_NUM,
	TOK_STRING,

	TOK_LPAREN,
	TOK_RPAREN,
	TOK_LBRACE,
	TOK_RBRACE,
	TOK_TILDE,
	TOK_LBRACKET,
	TOK_RBRACKET,
	TOK_SEMI,
	TOK_COMMA,
	TOK_COLON,
	TOK_QUESTION,
	TOK_DOT,
	TOK_ARROW,

	TOK_ASSIGN,
	TOK_PLUSEQ,
	TOK_MINUSEQ,
	TOK_MULEQ,
	TOK_DIVEQ,
	TOK_MODEQ,
	TOK_ANDEQ,
	TOK_OREQ,
	TOK_XOREQ,
	TOK_SHLEQ,
	TOK_SHREQ,
	TOK_PLUSPLUS,
	TOK_MINUSMINUS,

	TOK_PLUS,
	TOK_MINUS,
	TOK_STAR,
	TOK_SLASH,
	TOK_PERCENT,
	TOK_PIPE,
	TOK_CARET,
	TOK_SHL,
	TOK_SHR,

	TOK_EQ,
	TOK_NE,
	TOK_LT,
	TOK_LE,
	TOK_GT,
	TOK_GE,

	TOK_AND,
	TOK_OR,
	TOK_AMP,
	TOK_NOT
} TokenKind;

typedef struct {
	TokenKind 	kind;
	int 	value;
	char 	*text;

	/* Logical source location.  This follows #line directives emitted by
	 * the preprocessor and is what diagnostics should normally display. */
	int     filename_id;
	int 	line;
	int 	column;

	/* Physical/preprocessed location.  This never follows #line; it tracks
	 * where the token appeared in the token stream being lexed. */
	int     pp_filename_id;
	int 	pp_line;
	int 	pp_column;
} Token;

void 	lexer_init(const char *source);
void 	lexer_set_filename(const char *name);
int     lexer_filename_intern(const char *name);
const char *lexer_filename_name(int id);
const char *token_debug_name(TokenKind kind);
const Token *lexer_peek(void);
const Token *lexer_peek_ahead(int n);
const Token *lexer_peek_next(void);
const Token *lexer_peek_after_next(void);
const Token *lexer_peek_third(void);
const Token *lexer_peek_fourth(void);
const Token *lexer_peek_fifth(void);
int 	lexer_is_struct_definition(void);
int 	lexer_is_function_prototype(void);
const Token *lexer_next(void);

/* Parser/helper diagnostic functions that operate on tokens */
void print_source_caret(const char *file, int line, int column);
void lexer_debug_print_pp_line(int line, int column);
void fatal_token(const Token *token, const char *fmt, ...) __attribute__((noreturn));
void fatal_cur(const char *fmt, ...) __attribute__((noreturn));

#endif
