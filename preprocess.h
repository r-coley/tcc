#ifndef PREPROCESS_H
#define PREPROCESS_H

typedef enum {
	PP_TARGET_X86,
	PP_TARGET_X64,
	PP_TARGET_ARM64,
	PP_TARGET_MIPS
} PreprocessTarget;

typedef struct {
	char *name;
	char *value;
	int is_function_like;
	int is_variadic;
	int param_count;
	char *params[16];
} Macro;

typedef struct {
	int skipping;
	int taken;
	int parent_skipping;
} IfState;

typedef enum {
	PPTOK_IDENT,
	PPTOK_NUMBER,
	PPTOK_STRING,
	PPTOK_CHAR,
	PPTOK_PUNCT,
	PPTOK_SPACE,
	PPTOK_OTHER,
	PPTOK_NEWLINE,
	PPTOK_EOF
} PPTokenKind;

typedef struct {
	PPTokenKind kind;
	char *text;
} PPToken;

typedef struct {
	PPToken *items;
	int count;
	int cap;
} PPTokenVec;


void preprocess_configure(PreprocessTarget target);
const char *preprocess_get_file(void);
void fatal_pp(const char *fmt, ...) __attribute__((noreturn));
void preprocess_set_include_dir(const char *dir);
void preprocess_set_file(const char *filename);
void preprocess_set_line_markers(int enable);
char *preprocess(const char *filename, const char *input);

#endif
