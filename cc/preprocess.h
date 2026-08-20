#ifndef PREPROCESS_H
#define PREPROCESS_H

#include "codegen/codegen.h"

typedef enum {
	PP_TARGET_X86,
	PP_TARGET_X64,
	PP_TARGET_ARM64,
	PP_TARGET_MIPS,
	PP_TARGET_M68K
} PreprocessTarget;

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
	unsigned char owns_text;
} PPToken;

typedef struct {
	PPToken *items;
	int count;
	int cap;
} PPTokenVec;

typedef struct {
	char *name;
	char *value;
	int is_function_like;
	int is_variadic;
	int param_count;
	char **params;   /* heap-allocated, sized to param_count */
	PPTokenVec replacement_tokens;
	int replacement_tokens_ready;
} Macro;

typedef struct {
	int skipping;
	int taken;
	int parent_skipping;
} IfState;

typedef struct {
	unsigned long long trigraph_time;
	unsigned long long child_trigraph_time;
	unsigned long long join_time;
	unsigned long long child_join_time;
	unsigned long long strip_comments_time;
	unsigned long long child_strip_comments_time;
	unsigned long long macro_expand_time;
	unsigned long long child_macro_expand_time;
	unsigned long long macro_tokenize_time;
	unsigned long long child_macro_tokenize_time;
	unsigned long long object_macro_time;
	unsigned long long child_object_macro_time;
	unsigned long long function_macro_time;
	unsigned long long child_function_macro_time;
	unsigned long long function_macro_arg_collect_time;
	unsigned long long child_function_macro_arg_collect_time;
	unsigned long long function_macro_arg_expand_time;
	unsigned long long child_function_macro_arg_expand_time;
	unsigned long long function_macro_build_time;
	unsigned long long child_function_macro_build_time;
	unsigned long long function_macro_tail_time;
	unsigned long long child_function_macro_tail_time;
	unsigned long long macro_rescan_time;
	unsigned long long child_macro_rescan_time;
	unsigned long long directive_time;
	unsigned long long child_directive_time;
	unsigned long long directive_normalize_time;
	unsigned long long conditional_time;
	unsigned long long child_conditional_time;
	unsigned long long define_time;
	unsigned long long child_define_time;
	unsigned long long undef_time;
	unsigned long long include_time;
	unsigned long long child_include_time;
	unsigned long long include_lookup_time;
	unsigned long long include_child_preprocess_time;
	unsigned long long include_emit_time;
	unsigned long long pragma_time;
	unsigned long long line_time;
	unsigned long long warning_time;
	unsigned long long error_time;
	unsigned long long skipped_directive_time;
	unsigned long long child_skipped_directive_time;
	unsigned long long other_directive_time;
	unsigned long files_processed;
	unsigned long lines_processed;
	unsigned long skipped_lines;
	unsigned long directives_seen;
	unsigned long conditionals_seen;
	unsigned long defines_seen;
	unsigned long undefs_seen;
	unsigned long includes_seen;
	unsigned long include_empty_sources;
	unsigned long pragmas_seen;
	unsigned long lines_seen;
	unsigned long warnings_seen;
	unsigned long errors_seen;
	unsigned long skipped_directives_seen;
	unsigned long other_directives_seen;
	unsigned long macro_expand_calls;
	unsigned long tokenize_calls;
	unsigned long object_macro_expansions;
	unsigned long function_macro_expansions;
	unsigned long include_cache_hits;
	unsigned long include_cache_misses;
	unsigned long include_file_cache_hits;
	unsigned long include_file_cache_misses;
	unsigned long include_files_opened;
	unsigned long bytes_input;
	unsigned long bytes_output;
} PreprocessProfile;

extern int preprocess_current_line;

void preprocess_configure(PreprocessTarget target);
PreprocessTarget preprocess_get_target(void);
void preprocess_set_asm_dialect(AsmDialect dialect);
const char *preprocess_get_file(void);
void fatal_pp(const char *fmt, ...) __attribute__((noreturn));
void preprocess_set_include_dir(const char *dir);
void preprocess_set_bootstrap_includes(int enable);
void preprocess_set_stdinc(int enable);
void preprocess_set_file(const char *filename);
void preprocess_set_line_markers(int enable);
void preprocess_reset_file_macros(void);
void preprocess_profile_enable(int enable);
void preprocess_profile_get(PreprocessProfile *out);
char *preprocess(const char *filename, const char *input);

#endif
