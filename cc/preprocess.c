#include <ctype.h>
#include <errno.h>


#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <unistd.h>

#include "tcc.h"
#include "preprocess.h"

static Macro *macros = NULL;
static int macro_count;
static int macro_cap;

typedef struct MacroStackEntry {
	char *name;
	int had_macro;
	Macro macro;
	void *next;
} MacroStackEntry;

typedef struct MacroHashEntry {
	int macro_index;
	void *next;
} MacroHashEntry;

static void **macro_hash_buckets = NULL;
static int macro_hash_bucket_count;
static const char pp_empty_include_source[] = "";

#define MACRO_HASH_INITIAL_BUCKETS 256
#define MACRO_DISABLED_MAX 64
#define MACRO_DISABLED_NAME_LEN 65

typedef struct MacroDisabledSet {
	char names[MACRO_DISABLED_MAX][MACRO_DISABLED_NAME_LEN];
	int count;
} MacroDisabledSet;

static void expand_text_recursive(const char *text, char **out, size_t *len,
                                  size_t *cap, int depth,
                                  const MacroDisabledSet *disabled);
static int line_needs_macro_expansion(const char *line);
static int param_index(Macro *macro, const char *name);
static int pp_source_starts_identifier(const char *p);
static char *strip_comments(const char *input);
static void parse_pp_identifier(const char **pp, char *name, size_t name_size, const char *what);
static const char *directive_body_start(const char *q);
static int is_defined(const char *name);
static char *preprocess_normalize_source(const char *input);
static char *preprocess_internal(const char *filename, const char *input,
                                 int skip_translation_phases);
int open(const char *path, int oflag, ...);

static PreprocessTarget configured_target = PP_TARGET_X86;
static AsmDialect configured_asm_dialect = ASM_DIALECT_DEFAULT;
static int builtins_initialized = 0;
static int preprocess_profile_enabled = 0;
static int preprocess_profile_depth = 0;
static PreprocessProfile preprocess_profile_data;
/* Search path list for #include "..." resolution — up to 64 -I dirs */
#define MAX_INCLUDE_DIRS 64
static const char *include_dirs[MAX_INCLUDE_DIRS];
static int include_dir_count = 0;
static int bootstrap_includes = 0;
static int stdinc_enabled = 1;
static int preprocess_emit_line_markers = 0; /* -dD: emit #line markers in -E output */
static int preamble_injected = 0; /* only inject preamble once per compilation unit */
static int internal_header_macros_initialized = 0;

typedef struct IncludeCacheEntry {
	char *path;
	int is_system;
	int start_include_dir;
	int include_dir_index;
	int found;
	char *resolved_path;
	int hash_next;
} IncludeCacheEntry;

#define INCLUDE_CACHE_MAX 512
#define INCLUDE_CACHE_BUCKETS 1024
#define INCLUDE_CACHE_BUCKET(path, is_system, start_include_dir) \
	((tcc_hash_string((path) ? (path) : "") ^ \
	  (unsigned)(is_system) * 16777619u ^ \
	  (unsigned)(start_include_dir) * 2166136261u) & \
	 (INCLUDE_CACHE_BUCKETS - 1))
static IncludeCacheEntry include_cache_entries[INCLUDE_CACHE_MAX];
static int include_cache_count = 0;
static int include_cache_buckets[INCLUDE_CACHE_BUCKETS];

typedef struct IncludeFileCacheEntry {
	char *resolved_path;
	char *source;
	char *normalized_source;
	dev_t dev;
	ino_t ino;
	int pragma_once;
	int included_once;
	char guard_macro[TCC_IDENT_BUF_SIZE];
	int path_hash_next;
	int identity_hash_next;
} IncludeFileCacheEntry;

#define INCLUDE_FILE_CACHE_MAX 512
#define INCLUDE_FILE_CACHE_PATH_BUCKETS 1024
#define INCLUDE_FILE_CACHE_ID_BUCKETS 1024
#define INCLUDE_FILE_CACHE_ID_BUCKET(dev, ino) \
	((((unsigned)(unsigned long)(dev) ^ \
	   (unsigned)(unsigned long long)(unsigned long)(ino) ^ \
	   (unsigned)((unsigned long long)(unsigned long)(ino) >> 32))) & \
	  (INCLUDE_FILE_CACHE_ID_BUCKETS - 1))
static IncludeFileCacheEntry include_file_cache_entries[INCLUDE_FILE_CACHE_MAX];
static int include_file_cache_count = 0;
static int include_file_cache_path_buckets[INCLUDE_FILE_CACHE_PATH_BUCKETS];
static int include_file_cache_identity_buckets[INCLUDE_FILE_CACHE_ID_BUCKETS];

static int
scan_guard_identifier(const char *p, char *name, size_t name_size)
{
	size_t len = 0;

	while (isspace((unsigned char)*p))
		p++;
	if (!pp_source_starts_identifier(p))
		return 0;
	parse_pp_identifier(&p, name, name_size, "preprocessor identifier");
	while (isspace((unsigned char)*p))
		p++;
	if (*p != '\0')
		return 0;
	len = strlen(name);
	return len > 0;
}

static unsigned long long
pp_monotonic_nanos(void)
{
	struct timespec ts;

	if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0)
		return 0;
	return (unsigned long long)ts.tv_sec * 1000000000ULL +
	       (unsigned long long)ts.tv_nsec;
}

static void
preprocess_profile_reset(void)
{
	memset(&preprocess_profile_data, 0, sizeof(preprocess_profile_data));
}

void
preprocess_profile_enable(int enable)
{
	preprocess_profile_enabled = enable;
	if (!enable)
		preprocess_profile_depth = 0;
}

void
preprocess_profile_get(PreprocessProfile *out)
{
	if (!out)
		return;
	memcpy(out, &preprocess_profile_data, sizeof(preprocess_profile_data));
}

static void
detect_include_once_metadata(const char *source, int *out_pragma_once,
                             char *guard_macro, size_t guard_macro_size)
{
	char *comment_stripped;
	const char *p;
	int saw_ifndef = 0;
	char ifndef_name[TCC_IDENT_BUF_SIZE];

	if (out_pragma_once)
		*out_pragma_once = 0;
	if (guard_macro && guard_macro_size > 0)
		guard_macro[0] = '\0';
	ifndef_name[0] = '\0';
	comment_stripped = strip_comments(source);
	p = comment_stripped;

	while (*p) {
		const char *line_start = p;
		const char *q;
		const char *body;
		char line[512];
		size_t line_len;

		while (*p && *p != '\n')
			p++;
		line_len = (size_t)(p - line_start);
		if (*p == '\n')
			p++;

		if (line_len >= sizeof(line))
			line_len = sizeof(line) - 1;
		memcpy(line, line_start, line_len);
		line[line_len] = '\0';

		q = line;
		while (isspace((unsigned char)*q))
			q++;
		if (*q == '\0')
			continue;

		body = directive_body_start(q);

		if (body && STRNCMP(body, "pragma", 6) == 0 && isspace((unsigned char)body[6])) {
			const char *r = body + 6;
			while (isspace((unsigned char)*r))
				r++;
			if (STRCMP(r, "once") == 0 && out_pragma_once)
				*out_pragma_once = 1;
			continue;
		}

		if (!saw_ifndef && body && STRNCMP(body, "if", 2) == 0 && isspace((unsigned char)body[2])) {
			const char *r = body + 2;
			char name[TCC_IDENT_BUF_SIZE] = {0};

			while (isspace((unsigned char)*r))
				r++;
			if (*r == '!') {
				r++;
				while (isspace((unsigned char)*r))
					r++;
				if (STRNCMP(r, "defined", 7) == 0 &&
				    !pp_source_starts_identifier(r + 7)) {
					r += 7;
					while (isspace((unsigned char)*r))
						r++;
					if (*r == '(') {
						const char *end;
						r++;
						while (isspace((unsigned char)*r))
							r++;
						if (scan_guard_identifier(r, name, sizeof(name))) {
							end = r;
							while (isalnum((unsigned char)*end) || *end == '_')
								end++;
							while (isspace((unsigned char)*end))
								end++;
							if (*end == ')')
								saw_ifndef = STRCMP(name, "") != 0;
						}
					} else if (scan_guard_identifier(r, name, sizeof(name))) {
						saw_ifndef = STRCMP(name, "") != 0;
					}
					if (saw_ifndef)
						STRNCPY(ifndef_name, name, sizeof(ifndef_name) - 1);
				}
			}
			continue;
		}

		if (!saw_ifndef && body && STRNCMP(body, "ifndef", 6) == 0 && isspace((unsigned char)body[6])) {
			if (scan_guard_identifier(body + 6, ifndef_name, sizeof(ifndef_name)))
				saw_ifndef = 1;
			continue;
		}

		if (saw_ifndef && body && STRNCMP(body, "define", 6) == 0 && isspace((unsigned char)body[6])) {
			char define_name[TCC_IDENT_BUF_SIZE];
			if (scan_guard_identifier(body + 6, define_name, sizeof(define_name)) &&
			    STRCMP(define_name, ifndef_name) == 0) {
				STRNCPY(guard_macro, ifndef_name, guard_macro_size - 1);
				guard_macro[guard_macro_size - 1] = '\0';
			}
			return;
		}

		if (body) {
			continue;
		}

		xfree(comment_stripped);
		return;
	}

	xfree(comment_stripped);
}

/* Compiler-internal preamble injected at the top of every non-boot compilation unit. */
static const char *tcc_preamble =
	"typedef char *va_list;\n"
	"typedef char *__gnuc_va_list;\n"
	"static char *__tcc_va_base(void)\n"
	"{\n"
	"#if defined(__x86_64__)\n"
	"# if defined(__TCC_ASM_ATT__)\n"
	"    asm volatile (\"movq (%rbp), %rax\");\n"
	"    asm volatile (\"addq $16, %rax\");\n"
	"# else\n"
	"    asm volatile (\"mov rax, QWORD PTR [rbp]\");\n"
	"    asm volatile (\"add rax, 16\");\n"
	"# endif\n"
	"#elif defined(__mips__)\n"
	"    asm volatile (\"lw $v0, 0($fp)\");\n"
	"    asm volatile (\"addiu $v0, $v0, 16\");\n"
	"#else\n"
	"    asm volatile (\"ldr x0, [x29]\");\n"
	"    asm volatile (\"add x0, x0, #16\");\n"
	"#endif\n"
	"}\n"
	"#define __builtin_va_list char *\n"
	"#if defined(__x86_64__)\n"
	"# define va_start(ap, last) ((ap) = __builtin_va_start((ap), (last)))\n"
	"#else\n"
	"# define va_start(ap, last) ((ap) = __tcc_va_base())\n"
	"#endif\n"
	"#define __TCC_VA_SLOT_SIZE(type) (((sizeof(type) + 7u) / 8u) * 8u)\n"
	"#define va_arg(ap, type)   (*(type *)((ap) += __TCC_VA_SLOT_SIZE(type), (ap) - __TCC_VA_SLOT_SIZE(type)))\n"
	"#define va_end(ap)         ((void)0)\n"
	"#define __printflike(a,b)\n"
	"#define __scanflike(a,b)\n"
	"#define __printf0like(a,b)\n"
	"#define __osloglike(a,b)\n"
	"#define __cold\n"
	"#define __dead2\n"
	"#define __pure2\n"
	"#define __stateful_pure\n"
	"#define __swift_unavailable(x)\n"
	"#define __swift_unavailable_from_async(x)\n"
	"#define __swift_nonisolated\n"
	"#define __swift_nonisolated_unsafe\n"
	"#define __result_use_check\n"
	"#define __abortlike\n"
	"#define __disable_tail_calls\n"
	"#define __not_tail_called\n"
	"#define __returns_nonnull\n"
	"#define __deprecated\n"
	"#define __deprecated_msg(x)\n"
	"#define __deprecated_enum_msg(x)\n"
	"#define __unavailable\n"
	"#define __BEGIN_DECLS\n"
	"#define __END_DECLS\n"
	"#define __P(protos) protos\n"
	"#define __restrict restrict\n"
	"#define __nullable\n"
	"#define __nonnull\n"
	"#define __null_unspecified\n"
	"#define _Nullable\n"
	"#define _Nonnull\n"
	"#define _Null_unspecified\n"
	/* malloc_zone stubs — malloc/malloc.h skipped due to typed enum */
	"typedef struct malloc_zone_t malloc_zone_t;\n"
	"#define malloc_zone_malloc(z,n) malloc(n)\n"
	"#define malloc_zone_free(z,p) free(p)\n"
	"#define malloc_zone_realloc(z,p,n) realloc(p,n)\n"
	"#define malloc_default_zone() ((void*)0)\n"
	"#define _MALLOC_TYPED(t,n)\n"
	"void *malloc(unsigned long);\n"
	"void *calloc(unsigned long, unsigned long);\n"
	"void *realloc(void *, unsigned long);\n"
	"void free(void *);\n"
	"void *valloc(unsigned long);\n"
	"unsigned long malloc_size(void *);\n"
	"unsigned long malloc_good_size(unsigned long);\n"
	/* math.h stubs */
	"double sqrt(double);\n"
	"double fabs(double);\n"
	"double ceil(double);\n"
	"double floor(double);\n"
	"double pow(double,double);\n"
	"double log(double);\n"
	"double log2(double);\n"
	"double exp(double);\n"
	"double fmod(double,double);\n"
	"double round(double);\n"
	"float sqrtf(float);\n"
	"float fabsf(float);\n"
	"int isnan(double);\n"
	"int isinf(double);\n"
	"int isfinite(double);\n"
	"#define HUGE_VAL (1.0/0.0)\n"
	"#define HUGE_VALF (1.0f/0.0f)\n"
	"#define NAN (0.0/0.0)\n"
	"#define INFINITY (1.0/0.0)\n"
	"struct malloc_zone_t *malloc_create_zone(unsigned long, unsigned int);\n"
	"struct malloc_zone_t *malloc_default_purgeable_zone(void);\n"
	"void malloc_set_zone_name(struct malloc_zone_t *, const char *);\n"
	"void malloc_destroy_zone(struct malloc_zone_t *);\n"
	"#ifndef NULL\n"
	"#define NULL ((void*)0)\n"
	"#endif\n";
static char preprocess_file_buf[512] = "<input>";
static const char *preprocess_file = preprocess_file_buf;
static int builtin_counter_value = 0;
static int current_include_dir_index = -1;

static unsigned
include_file_cache_path_hash_key(const char *resolved_path)
{
	return tcc_hash_string(resolved_path ? resolved_path : "") &
	       (INCLUDE_FILE_CACHE_PATH_BUCKETS - 1);
}

static void
include_cache_reset_buckets(int *buckets, int count)
{
	int i;

	for (i = 0; i < count; i++)
		buckets[i] = -1;
}

static void
include_cache_entry_clear(IncludeCacheEntry *entry)
{
	if (!entry)
		return;
	xfree(entry->path);
	xfree(entry->resolved_path);
	entry->path = NULL;
	entry->resolved_path = NULL;
	entry->hash_next = -1;
}

static void
include_file_cache_entry_clear(IncludeFileCacheEntry *entry)
{
	if (!entry)
		return;
	xfree(entry->resolved_path);
	xfree(entry->source);
	xfree(entry->normalized_source);
	entry->resolved_path = NULL;
	entry->source = NULL;
	entry->normalized_source = NULL;
	entry->guard_macro[0] = '\0';
	entry->pragma_once = 0;
	entry->included_once = 0;
	entry->path_hash_next = -1;
	entry->identity_hash_next = -1;
}

static void
include_cache_insert_index(int idx)
{
	IncludeCacheEntry *entry;
	int *bucket_head;
	unsigned bucket;

	if (idx < 0 || idx >= include_cache_count)
		return;
	entry = &include_cache_entries[idx];
	if (!entry->path)
		return;
	bucket = INCLUDE_CACHE_BUCKET(entry->path, entry->is_system,
	                              entry->start_include_dir);
	bucket_head = &include_cache_buckets[bucket];
	entry->hash_next = *bucket_head;
	*bucket_head = idx;
}

static void
include_file_cache_insert_identity_index(int idx)
{
	IncludeFileCacheEntry *entries = include_file_cache_entries;
	int *buckets = include_file_cache_identity_buckets;
	IncludeFileCacheEntry *entry;
	int *bucket_head;

	if (idx < 0 || idx >= include_file_cache_count)
		return;
	entry = &entries[idx];
	bucket_head = &buckets[INCLUDE_FILE_CACHE_ID_BUCKET(entry->dev, entry->ino)];
	entry->identity_hash_next = *bucket_head;
	*bucket_head = idx;
}

static void
include_file_cache_remove_path_index(int idx)
{
	unsigned bucket;
	int cur;
	int prev = -1;

	if (idx < 0 || idx >= include_file_cache_count ||
	    !include_file_cache_entries[idx].resolved_path)
		return;
	bucket = include_file_cache_path_hash_key(include_file_cache_entries[idx].resolved_path);
	cur = include_file_cache_path_buckets[bucket];
	while (cur >= 0) {
		if (cur == idx) {
			if (prev >= 0)
				include_file_cache_entries[prev].path_hash_next =
				    include_file_cache_entries[cur].path_hash_next;
			else
				include_file_cache_path_buckets[bucket] =
				    include_file_cache_entries[cur].path_hash_next;
			include_file_cache_entries[cur].path_hash_next = -1;
			return;
		}
		prev = cur;
		cur = include_file_cache_entries[cur].path_hash_next;
	}
}

static void
include_file_cache_remove_identity_index(int idx)
{
	IncludeFileCacheEntry *entries = include_file_cache_entries;
	int *buckets = include_file_cache_identity_buckets;
	unsigned bucket;
	int cur;
	IncludeFileCacheEntry *entry;
	IncludeFileCacheEntry *cur_entry;
	int *link;

	if (idx < 0 || idx >= include_file_cache_count)
		return;
	entry = &entries[idx];
	bucket = INCLUDE_FILE_CACHE_ID_BUCKET(entry->dev, entry->ino);
	link = &buckets[bucket];
	cur = *link;
	while (cur >= 0) {
		cur_entry = &entries[cur];
		if (cur == idx) {
			*link = cur_entry->identity_hash_next;
			cur_entry->identity_hash_next = -1;
			return;
		}
		link = &cur_entry->identity_hash_next;
		cur = *link;
	}
}

static void
include_cache_clear(void)
{
	int i;

	include_cache_reset_buckets(include_cache_buckets, INCLUDE_CACHE_BUCKETS);
	for (i = 0; i < include_cache_count; i++)
		include_cache_entry_clear(&include_cache_entries[i]);
	include_cache_count = 0;

	include_cache_reset_buckets(include_file_cache_path_buckets,
	                           INCLUDE_FILE_CACHE_PATH_BUCKETS);
	include_cache_reset_buckets(include_file_cache_identity_buckets,
	                           INCLUDE_FILE_CACHE_ID_BUCKETS);
	for (i = 0; i < include_file_cache_count; i++)
		include_file_cache_entry_clear(&include_file_cache_entries[i]);
	include_file_cache_count = 0;
}

static IncludeCacheEntry *
include_cache_find(const char *path, int is_system, int start_include_dir)
{
	int idx;
	unsigned bucket = INCLUDE_CACHE_BUCKET(path, is_system, start_include_dir);

	for (idx = include_cache_buckets[bucket]; idx >= 0;
	     idx = include_cache_entries[idx].hash_next) {
		IncludeCacheEntry *entry = &include_cache_entries[idx];
		if (entry->is_system == is_system &&
		    entry->start_include_dir == start_include_dir &&
		    STRCMP(entry->path, path) == 0)
			return entry;
	}

	return NULL;
}

static void
include_cache_store(const char *path, int is_system, int start_include_dir,
                    int found, const char *resolved_path,
                    int include_dir_index)
{
	IncludeCacheEntry *entry = include_cache_find(path, is_system, start_include_dir);
	int is_new = 0;

	if (!entry) {
		if (include_cache_count >= INCLUDE_CACHE_MAX)
			return;
		entry = &include_cache_entries[include_cache_count++];
		memset(entry, 0, sizeof(*entry));
		entry->path = xstrdup(path);
		entry->hash_next = -1;
		is_new = 1;
	}

	entry->is_system = is_system;
	entry->start_include_dir = start_include_dir;
	entry->include_dir_index = include_dir_index;
	entry->found = found;

	xfree(entry->resolved_path);
	entry->resolved_path = resolved_path ? xstrdup(resolved_path) : NULL;
	if (is_new)
		include_cache_insert_index((int)(entry - include_cache_entries));
}

static IncludeFileCacheEntry *
include_file_cache_find(const char *resolved_path)
{
	int idx;
	unsigned bucket;

	if (!resolved_path)
		return NULL;

	bucket = include_file_cache_path_hash_key(resolved_path);
	for (idx = include_file_cache_path_buckets[bucket]; idx >= 0;
	     idx = include_file_cache_entries[idx].path_hash_next) {
		IncludeFileCacheEntry *entry = &include_file_cache_entries[idx];
		if (entry->resolved_path && STRCMP(entry->resolved_path, resolved_path) == 0)
			return entry;
	}

	return NULL;
}

static IncludeFileCacheEntry *
include_file_cache_find_identity(dev_t dev, ino_t ino)
{
	IncludeFileCacheEntry *entries = include_file_cache_entries;
	int *buckets = include_file_cache_identity_buckets;
	int idx;
	unsigned bucket = INCLUDE_FILE_CACHE_ID_BUCKET(dev, ino);
	IncludeFileCacheEntry *entry;

	for (idx = buckets[bucket]; idx >= 0;
	     idx = entry->identity_hash_next) {
		entry = &entries[idx];
		if (entry->dev == dev && entry->ino == ino)
			return entry;
	}

	return NULL;
}

static IncludeFileCacheEntry *
include_file_cache_store(const char *resolved_path, const char *source,
                         dev_t dev, ino_t ino)
{
	IncludeFileCacheEntry *entry = NULL;
	int idx;
	int reuse_existing = 0;

	if (resolved_path)
		entry = include_file_cache_find(resolved_path);
	if (!entry)
		entry = include_file_cache_find_identity(dev, ino);

	if (!entry) {
		if (include_file_cache_count >= INCLUDE_FILE_CACHE_MAX)
			return NULL;
		entry = &include_file_cache_entries[include_file_cache_count];
		memset(entry, 0, sizeof(*entry));
		entry->path_hash_next = -1;
		entry->identity_hash_next = -1;
		include_file_cache_count++;
	}

	idx = (int)(entry - include_file_cache_entries);
	reuse_existing = entry->source && entry->normalized_source &&
	                 entry->dev == dev && entry->ino == ino;
	if (entry->resolved_path)
		include_file_cache_remove_path_index(idx);
	if (entry->resolved_path || entry->dev || entry->ino)
		include_file_cache_remove_identity_index(idx);

	xfree(entry->resolved_path);
	entry->resolved_path = resolved_path ? xstrdup(resolved_path) : NULL;
	entry->dev = dev;
	entry->ino = ino;

	if (reuse_existing) {
		xfree((char *)source);
	} else {
		xfree(entry->source);
		entry->source = (char *)source;
		xfree(entry->normalized_source);
		entry->normalized_source = source ? preprocess_normalize_source(source) : xstrdup("");
		entry->pragma_once = 0;
		entry->guard_macro[0] = '\0';
		entry->included_once = 0;
	}

	entry->path_hash_next = -1;
	entry->identity_hash_next = -1;
	if (idx >= 0 && idx < include_file_cache_count && entry->resolved_path) {
		unsigned bucket = include_file_cache_path_hash_key(entry->resolved_path);
		entry->path_hash_next = include_file_cache_path_buckets[bucket];
		include_file_cache_path_buckets[bucket] = idx;
	}
	include_file_cache_insert_identity_index(idx);
	if (!reuse_existing && source)
		detect_include_once_metadata(source, &entry->pragma_once,
		                             entry->guard_macro, sizeof(entry->guard_macro));
	return entry;
}

static int
include_file_cache_should_skip(IncludeFileCacheEntry *entry)
{
	if (!entry)
		return 0;
	if (entry->pragma_once && entry->included_once)
		return 1;
	if (entry->guard_macro[0] != '\0' && is_defined(entry->guard_macro))
		return 1;
	return 0;
}

static char *
include_file_cache_dup_source(IncludeFileCacheEntry *entry)
{
	if (!entry)
		return NULL;
	entry->included_once = 1;
	return xstrdup(entry->source ? entry->source : "");
}

static const char *
include_file_cache_source(IncludeFileCacheEntry *entry)
{
	if (!entry)
		return NULL;
	entry->included_once = 1;
	return entry->source ? entry->source : pp_empty_include_source;
}

int preprocess_current_line = 1;
const char *pp_line;
static void *pragma_macro_stack = NULL;

PreprocessTarget
preprocess_get_target(void)
{
	return configured_target;
}

static int
pp_is_ident_start(char c)
{
	return isalpha((unsigned char)c) || c == '_';
}

static int
pp_is_ident_char(char c)
{
	return isalnum((unsigned char)c) || c == '_';
}

static int
pp_hex_value(char ch)
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
pp_peek_ucn_codepoint(const char *p, int *out_cp, int *out_len)
{
	int digits;
	int cp = 0;

	if (*p != '\\' || (p[1] != 'u' && p[1] != 'U'))
		return 0;

	digits = (p[1] == 'u') ? 4 : 8;
	for (int i = 0; i < digits; i++) {
		int value = pp_hex_value(p[2 + i]);
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
pp_ucn_is_ident_start(int cp)
{
	if (cp == '_')
		return 1;
	if (cp >= 0x80)
		return 1;
	return isalpha((unsigned char)cp) != 0;
}

static int
pp_ucn_is_ident_char(int cp)
{
	if (cp == '_')
		return 1;
	if (cp >= 0x80)
		return 1;
	return isalnum((unsigned char)cp) != 0;
}

static int
pp_ucn_is_valid_identifier_codepoint(int cp)
{
	if (cp == 0x24 || cp == 0x40 || cp == 0x60)
		return 1;
	if (cp >= 0x00a0 && !(cp >= 0xd800 && cp <= 0xdfff) && cp <= 0x10ffff)
		return 1;
	return 0;
}

static int
pp_source_starts_identifier(const char *p)
{
	int cp = 0;
	int ucn_len = 0;

	if (pp_is_ident_start(*p))
		return 1;

	return pp_peek_ucn_codepoint(p, &cp, &ucn_len) && pp_ucn_is_ident_start(cp);
}

static void
pp_warn(const char *fmt, ...)
{
	va_list ap;

	if (!tcc_warnings_enabled())
		return;

	if (tcc_warnings_as_errors_enabled())
		fprintf(stderr, "%s:%d: error: ",
		        preprocess_get_file(),
		        preprocess_current_line);
	else
		fprintf(stderr, "%s:%d: warning: ",
		        preprocess_get_file(),
		        preprocess_current_line);

	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	fprintf(stderr, "\n");

	if (tcc_warnings_as_errors_enabled())
		tcc_exit_failure();
}

static void
parse_pp_identifier(const char **pp, char *name, size_t name_size, const char *what)
{
	const char *p = *pp;
	size_t ni = 0;

	if (!pp_source_starts_identifier(p))
		fatal_pp("Expected preprocessor identifier\n");

	while (*p) {
		int cp = 0;
		int ucn_len = 0;

		if (pp_peek_ucn_codepoint(p, &cp, &ucn_len)) {
			if (!pp_ucn_is_ident_char(cp))
				break;
			if (!pp_ucn_is_valid_identifier_codepoint(cp))
				fatal_pp("invalid universal character name in identifier");
			if (ni + (size_t)ucn_len >= name_size || ni + (size_t)ucn_len > TCC_IDENT_MAX)
				fatal_pp("%s too long (max %d chars)\n", what, TCC_IDENT_MAX);
			memcpy(name + ni, p, (size_t)ucn_len);
			ni += (size_t)ucn_len;
			p += ucn_len;
			continue;
		}

		if (!pp_is_ident_char(*p))
			break;
		if (ni >= TCC_IDENT_MAX || ni + 1 >= name_size)
			fatal_pp("%s too long (max %d chars)\n", what, TCC_IDENT_MAX);
		name[ni++] = *p++;
	}

	name[ni] = '\0';
	*pp = p;
}

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
pptok_reserve(PPTokenVec *vec)
{
	if (vec->count >= vec->cap) {
		vec->cap = vec->cap ? vec->cap * 2 : 32;
		PPToken *new_items = xrealloc(vec->items, sizeof(PPToken) * (size_t)vec->cap);
		vec->items = new_items;
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

	pptok_reserve(vec);

	PPToken *tok = &vec->items[vec->count++];
	char *text = xmalloc(len + 1);

	tok->kind = kind;
	memcpy(text, start, len);
	text[len] = '\0';
	tok->text = text;
	tok->owns_text = 1;
	Debug(5,"TOKTEXT [%s]\n",tok->text);
}

static void
pptok_push_static(PPTokenVec *vec, PPTokenKind kind, const char *text)
{
	pptok_reserve(vec);

	PPToken *tok = &vec->items[vec->count++];
	tok->kind = kind;
	tok->text = (char *)text;
	tok->owns_text = 0;
	Debug(5,"TOKTEXT [%s]\n",tok->text);
}

static void
pptok_free(PPTokenVec *vec)
{
	for (int i = 0; i < vec->count; i++)
		if (vec->items[i].owns_text)
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

		if (pp_source_starts_identifier(p)) {
			while (*p) {
				int cp = 0;
				int ucn_len = 0;

				if (pp_peek_ucn_codepoint(p, &cp, &ucn_len)) {
					if (!pp_ucn_is_ident_char(cp))
						break;
					p += ucn_len;
					continue;
				}

				if (!pp_is_ident_char(*p))
					break;
				p++;
			}
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

		if (p[0] == '%' && p[1] == ':' && p[2] == '%' && p[3] == ':') {
			p += 4;
			pptok_push_static(out, PPTOK_PUNCT, "##");
			continue;
		}

		if ((p[0] == '#' && p[1] == '#') ||
		        (p[0] == '&' && p[1] == '&') ||
		        (p[0] == '|' && p[1] == '|') ||
		        (p[0] == '=' && p[1] == '=') ||
		        (p[0] == '!' && p[1] == '=') ||
		        (p[0] == '<' && p[1] == '=') ||
		        (p[0] == '>' && p[1] == '=') ||
		        (p[0] == '+' && p[1] == '=') ||
		        (p[0] == '-' && p[1] == '=') ||
		        (p[0] == '*' && p[1] == '=') ||
		        (p[0] == '/' && p[1] == '=') ||
		        (p[0] == '%' && p[1] == '=') ||
		        (p[0] == '&' && p[1] == '=') ||
		        (p[0] == '|' && p[1] == '=') ||
		        (p[0] == '^' && p[1] == '=') ||
		        (p[0] == '+' && p[1] == '+') ||
		        (p[0] == '-' && p[1] == '-') ||
		        (p[0] == '-' && p[1] == '>') ||
		        (p[0] == '<' && p[1] == '<') ||
		        (p[0] == '>' && p[1] == '>')) {
			/* Check for 3-character operators first */
			if ((p[0] == '<' && p[1] == '<' && p[2] == '=') ||
			    (p[0] == '>' && p[1] == '>' && p[2] == '=')) {
				p += 3;
				if (start[0] == '<')
					pptok_push_static(out, PPTOK_PUNCT, "<<=");
				else
					pptok_push_static(out, PPTOK_PUNCT, ">>=");
			} else {
				p += 2;
				if (start[0] == '#' && start[1] == '#')
					pptok_push_static(out, PPTOK_PUNCT, "##");
				else if (start[0] == '&' && start[1] == '&')
					pptok_push_static(out, PPTOK_PUNCT, "&&");
				else if (start[0] == '|' && start[1] == '|')
					pptok_push_static(out, PPTOK_PUNCT, "||");
				else if (start[0] == '=' && start[1] == '=')
					pptok_push_static(out, PPTOK_PUNCT, "==");
				else if (start[0] == '!' && start[1] == '=')
					pptok_push_static(out, PPTOK_PUNCT, "!=");
				else if (start[0] == '<' && start[1] == '=')
					pptok_push_static(out, PPTOK_PUNCT, "<=");
				else if (start[0] == '>' && start[1] == '=')
					pptok_push_static(out, PPTOK_PUNCT, ">=");
				else if (start[0] == '+' && start[1] == '=')
					pptok_push_static(out, PPTOK_PUNCT, "+=");
				else if (start[0] == '-' && start[1] == '=')
					pptok_push_static(out, PPTOK_PUNCT, "-=");
				else if (start[0] == '*' && start[1] == '=')
					pptok_push_static(out, PPTOK_PUNCT, "*=");
				else if (start[0] == '/' && start[1] == '=')
					pptok_push_static(out, PPTOK_PUNCT, "/=");
				else if (start[0] == '%' && start[1] == '=')
					pptok_push_static(out, PPTOK_PUNCT, "%=");
				else if (start[0] == '&' && start[1] == '=')
					pptok_push_static(out, PPTOK_PUNCT, "&=");
				else if (start[0] == '|' && start[1] == '=')
					pptok_push_static(out, PPTOK_PUNCT, "|=");
				else if (start[0] == '^' && start[1] == '=')
					pptok_push_static(out, PPTOK_PUNCT, "^=");
				else if (start[0] == '+' && start[1] == '+')
					pptok_push_static(out, PPTOK_PUNCT, "++");
				else if (start[0] == '-' && start[1] == '-')
					pptok_push_static(out, PPTOK_PUNCT, "--");
				else if (start[0] == '-' && start[1] == '>')
					pptok_push_static(out, PPTOK_PUNCT, "->");
				else if (start[0] == '<' && start[1] == '<')
					pptok_push_static(out, PPTOK_PUNCT, "<<");
				else
					pptok_push_static(out, PPTOK_PUNCT, ">>");
			}
			continue;
		}

		if (p[0] == '%' && p[1] == ':') {
			p += 2;
			pptok_push_static(out, PPTOK_PUNCT, "#");
			continue;
		}

		if (p[0] == '<' && p[1] == '%') {
			p += 2;
			pptok_push_static(out, PPTOK_PUNCT, "{");
			continue;
		}

		if (p[0] == '%' && p[1] == '>') {
			p += 2;
			pptok_push_static(out, PPTOK_PUNCT, "}");
			continue;
		}

		if (p[0] == '<' && p[1] == ':') {
			p += 2;
			pptok_push_static(out, PPTOK_PUNCT, "[");
			continue;
		}

		if (p[0] == ':' && p[1] == '>') {
			p += 2;
			pptok_push_static(out, PPTOK_PUNCT, "]");
			continue;
		}

		p++;
		Debug(5,"pptok_push PPTOK_PUNCT\n");
		switch (start[0]) {
		case '(': pptok_push_static(out, PPTOK_PUNCT, "("); break;
		case ')': pptok_push_static(out, PPTOK_PUNCT, ")"); break;
		case '[': pptok_push_static(out, PPTOK_PUNCT, "["); break;
		case ']': pptok_push_static(out, PPTOK_PUNCT, "]"); break;
		case '{': pptok_push_static(out, PPTOK_PUNCT, "{"); break;
		case '}': pptok_push_static(out, PPTOK_PUNCT, "}"); break;
		case ',': pptok_push_static(out, PPTOK_PUNCT, ","); break;
		case ';': pptok_push_static(out, PPTOK_PUNCT, ";"); break;
		case ':': pptok_push_static(out, PPTOK_PUNCT, ":"); break;
		case '?': pptok_push_static(out, PPTOK_PUNCT, "?"); break;
		case '~': pptok_push_static(out, PPTOK_PUNCT, "~"); break;
		case '#': pptok_push_static(out, PPTOK_PUNCT, "#"); break;
		case '.': pptok_push_static(out, PPTOK_PUNCT, "."); break;
		case '+': pptok_push_static(out, PPTOK_PUNCT, "+"); break;
		case '-': pptok_push_static(out, PPTOK_PUNCT, "-"); break;
		case '*': pptok_push_static(out, PPTOK_PUNCT, "*"); break;
		case '/': pptok_push_static(out, PPTOK_PUNCT, "/"); break;
		case '%': pptok_push_static(out, PPTOK_PUNCT, "%"); break;
		case '&': pptok_push_static(out, PPTOK_PUNCT, "&"); break;
		case '|': pptok_push_static(out, PPTOK_PUNCT, "|"); break;
		case '^': pptok_push_static(out, PPTOK_PUNCT, "^"); break;
		case '!': pptok_push_static(out, PPTOK_PUNCT, "!"); break;
		case '=': pptok_push_static(out, PPTOK_PUNCT, "="); break;
		case '<': pptok_push_static(out, PPTOK_PUNCT, "<"); break;
		case '>': pptok_push_static(out, PPTOK_PUNCT, ">"); break;
		default:  pptok_push(out, PPTOK_PUNCT, start, 1); break;
		}
	}
}

static void
ensure_append_capacity(char **out, size_t *cap, size_t need_len)
{
	size_t new_cap;

	if (need_len + 1 <= *cap)
		return;

	new_cap = *cap ? *cap : 64;
	while (need_len + 1 > new_cap)
		new_cap *= 2;

	*out = xrealloc(*out, new_cap);
	*cap = new_cap;
}

static void
append_char(char **out, size_t *len, size_t *cap, char ch)
{
	ensure_append_capacity(out, cap, *len + 1);

	(*out)[(*len)++] = ch;
	(*out)[*len] = '\0';
}

static void
append_str(char **out, size_t *len, size_t *cap, const char *s)
{
	size_t n = strlen(s);

	if (n == 0)
		return;
	ensure_append_capacity(out, cap, *len + n);
	memcpy(*out + *len, s, n);
	*len += n;
	(*out)[*len] = '\0';
}

static void
append_line_marker(char **out, size_t *len, size_t *cap, int line, const char *file)
{
	char buf[64];
	const char *trace_line = getenv("TCC_TRACE_LINE");
	Debug(1, "PP #line %d \"%s\"\n", line, file ? file : "<input>");
	if (trace_line && trace_line[0])
		fprintf(stderr,
		        "TRACE_LINE marker enter out=%p *out=%p len=%lu cap=%lu line=%d file=%s\n",
		        (void *)out, out ? (void *)*out : NULL,
		        (unsigned long)(len ? *len : 0),
		        (unsigned long)(cap ? *cap : 0),
		        line, file ? file : "<input>");
	/* Ensure the marker starts on a fresh line */
	if (*len > 0 && (*out)[*len - 1] != '\n') {
		if (trace_line && trace_line[0])
			fprintf(stderr, "TRACE_LINE marker append leading newline\n");
		append_char(out, len, cap, '\n');
	}
	if (trace_line && trace_line[0])
		fprintf(stderr, "TRACE_LINE marker before snprintf\n");
	snprintf(buf, sizeof(buf), "#line %d \"", line);
	if (trace_line && trace_line[0])
		fprintf(stderr, "TRACE_LINE marker after snprintf buf=[%s]\n", buf);
	append_str(out, len, cap, buf);
	if (trace_line && trace_line[0])
		fprintf(stderr, "TRACE_LINE marker after prefix len=%lu cap=%lu\n",
		        (unsigned long)*len, (unsigned long)*cap);
	append_str(out, len, cap, file ? file : "<input>");
	if (trace_line && trace_line[0])
		fprintf(stderr, "TRACE_LINE marker after file len=%lu cap=%lu\n",
		        (unsigned long)*len, (unsigned long)*cap);
	append_str(out, len, cap, "\"\n");
	if (trace_line && trace_line[0])
		fprintf(stderr, "TRACE_LINE marker exit len=%lu cap=%lu\n",
		        (unsigned long)*len, (unsigned long)*cap);
}

static void
macro_hash_free(void)
{
	int i;
	void *entry;

	if (!macro_hash_buckets)
		return;

	for (i = 0; i < macro_hash_bucket_count; i++) {
		entry = macro_hash_buckets[i];
		while (entry) {
			void *next = ((MacroHashEntry *)entry)->next;
			xfree(entry);
			entry = next;
		}
	}

	xfree(macro_hash_buckets);
	macro_hash_buckets = NULL;
	macro_hash_bucket_count = 0;
}

static int
macro_hash_insert_index(int macro_index)
{
	unsigned bucket;
	void *entry;

	if (macro_index < 0 || macro_index >= macro_count)
		return 0;
	if (!macros[macro_index].name)
		return 0;

	if (!macro_hash_buckets) {
		macro_hash_buckets = xcalloc(MACRO_HASH_INITIAL_BUCKETS, sizeof(void *));
		macro_hash_bucket_count = MACRO_HASH_INITIAL_BUCKETS;
	}

	bucket = tcc_hash_string(macros[macro_index].name) &
	         (unsigned)(macro_hash_bucket_count - 1);
	entry = xcalloc(1, sizeof(MacroHashEntry));
	((MacroHashEntry *)entry)->macro_index = macro_index;
	((MacroHashEntry *)entry)->next = macro_hash_buckets[bucket];
	macro_hash_buckets[bucket] = entry;
	return 1;
}

static int
macro_hash_remove_index(const char *name, int macro_index)
{
	unsigned bucket;
	void **link;

	if (!macro_hash_buckets || !name || macro_index < 0)
		return 0;

	bucket = tcc_hash_string(name) & (unsigned)(macro_hash_bucket_count - 1);
	link = &macro_hash_buckets[bucket];
	while (*link) {
		MacroHashEntry *entry = (MacroHashEntry *)*link;
		if (entry->macro_index == macro_index) {
			*link = entry->next;
			xfree(entry);
			return 1;
		}
		link = &entry->next;
	}

	return 0;
}

static int
macro_hash_update_index(const char *name, int old_index, int new_index)
{
	unsigned bucket;
	void *entry;

	if (!macro_hash_buckets || !name || old_index < 0 || new_index < 0)
		return 0;

	bucket = tcc_hash_string(name) & (unsigned)(macro_hash_bucket_count - 1);
	for (entry = macro_hash_buckets[bucket]; entry; entry = ((MacroHashEntry *)entry)->next) {
		if (((MacroHashEntry *)entry)->macro_index == old_index) {
			((MacroHashEntry *)entry)->macro_index = new_index;
			return 1;
		}
	}

	return 0;
}

static void
macro_hash_rebuild(void)
{
	int i;
	int bucket_count = macro_hash_bucket_count ? macro_hash_bucket_count : MACRO_HASH_INITIAL_BUCKETS;

	while (bucket_count < macro_count * 2)
		bucket_count *= 2;

	macro_hash_free();
	macro_hash_buckets = xcalloc((size_t)bucket_count, sizeof(void *));
	macro_hash_bucket_count = bucket_count;

	for (i = 0; i < macro_count; i++)
		macro_hash_insert_index(i);
}

static int
find_macro_index(const char *name)
{
	unsigned bucket;
	void *entry;

	if (!name)  {
		Debug(0,"find_macro_entry(NULL)\n");
		return -1;
	}

	if (!macro_hash_buckets)
		macro_hash_rebuild();

	bucket = tcc_hash_string(name) & (unsigned)(macro_hash_bucket_count - 1);
	for (entry = macro_hash_buckets[bucket]; entry; entry = ((MacroHashEntry *)entry)->next) {
		int idx = ((MacroHashEntry *)entry)->macro_index;
		if (idx >= 0 && idx < macro_count && macros[idx].name &&
		    STRCMP(macros[idx].name, name) == 0)
			return idx;
	}

	return -1;
}

static Macro *
find_macro_entry(const char *name)
{
	int idx = find_macro_index(name);

	if (idx < 0)
		return NULL;

	return &macros[idx];
}

static Macro *
find_macro_entry_n(const char *name, size_t name_len)
{
	unsigned bucket;
	void *entry;

	if (!name)
		return NULL;

	if (!macro_hash_buckets)
		macro_hash_rebuild();

	bucket = 2166136261u;
	for (size_t i = 0; i < name_len; i++) {
		bucket ^= (unsigned char)name[i];
		bucket *= 16777619u;
	}
	bucket &= (unsigned)(macro_hash_bucket_count - 1);

	for (entry = macro_hash_buckets[bucket]; entry; entry = ((MacroHashEntry *)entry)->next) {
		int idx = ((MacroHashEntry *)entry)->macro_index;
		Macro *macro;
		if (idx < 0 || idx >= macro_count)
			continue;
		macro = &macros[idx];
		if (!macro->name)
			continue;
		if (strlen(macro->name) == name_len &&
		    memcmp(macro->name, name, name_len) == 0)
			return macro;
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

	if (m->params) {
		int i;
		for (i = 0; i < m->param_count; i++)
			xfree(m->params[i]);
		xfree(m->params);
	}
	if (m->replacement_tokens_ready)
		pptok_free(&m->replacement_tokens);

	memset(m,0,sizeof(*m));
}

static void
free_macro_contents_preserve_name(Macro *m)
{
	if (!m)
		return;

	xfree(m->value);
	m->value = NULL;

	if (m->params) {
		int i;
		for (i = 0; i < m->param_count; i++)
			xfree(m->params[i]);
		xfree(m->params);
	}
	m->params = NULL;

	if (m->replacement_tokens_ready)
		pptok_free(&m->replacement_tokens);

	m->is_function_like = 0;
	m->is_variadic = 0;
	m->param_count = 0;
	m->replacement_tokens_ready = 0;
}

static void
macro_prepare_replacement_tokens(Macro *macro)
{
	if (!macro || macro->replacement_tokens_ready)
		return;

	macro->replacement_tokens.items = 0;
	macro->replacement_tokens.count = 0;
	macro->replacement_tokens.cap = 0;
	pp_tokenize_line(macro->value ? macro->value : "", &macro->replacement_tokens);
	macro->replacement_tokens_ready = 1;
}

static void
copy_macro(Macro *dst, const Macro *src)
{
	memset(dst, 0, sizeof(*dst));
	if (!src)
		return;

	dst->name = xstrdup(src->name ? src->name : "");
	dst->value = xstrdup(src->value ? src->value : "");
	dst->is_function_like = src->is_function_like;
	dst->is_variadic = src->is_variadic;
	dst->param_count = src->param_count;

	if (src->param_count > 0 && src->params) {
		dst->params = xcalloc((size_t)src->param_count, sizeof(char *));
		for (int i = 0; i < src->param_count; i++)
			dst->params[i] = xstrdup(src->params[i] ? src->params[i] : "");
	}
	if (dst->is_function_like)
		macro_prepare_replacement_tokens(dst);
}

static void
init_macro(Macro *macro, const char *name, const char *value)
{
	if (macro->name && name && STRCMP(macro->name, name) == 0)
		free_macro_contents_preserve_name(macro);
	else
		free_macro(macro);

	if (!macro->name)
		macro->name = xstrdup(name ? name : "");
	macro->value = xstrdup(value ? value : "");
}

static int
get_or_add_macro_index(const char *name, int *is_new)
{
	int idx;

	if (is_new)
		*is_new = 0;

	if (!name)  {
		Debug(0,"get_or_add_macro(NULL)\n");
		return -1;
	}

	idx = find_macro_index(name);
	if (idx >= 0)
		return idx;

	if (macro_count >= macro_cap) {
		int old_cap = macro_cap;
		int new_cap = macro_cap ? macro_cap * 2 : 64;
		macros = xrealloc(macros, (size_t)new_cap * sizeof(Macro));
		memset(&macros[old_cap], 0, (size_t)(new_cap - old_cap) * sizeof(Macro));
		macro_cap = new_cap;
	}

	idx = macro_count++;
	memset(&macros[idx], 0, sizeof(Macro));
	if (is_new)
		*is_new = 1;
	return idx;
}


static void
add_object_macro(const char *name, const char *value)
{
	int is_new = 0;
	int idx = get_or_add_macro_index(name, &is_new);
	Macro *macro = idx >= 0 ? &macros[idx] : NULL;

	init_macro(macro, name, value);
	if (is_new)
		macro_hash_insert_index(idx);
}

static void
replace_macro_with_copy(const Macro *src)
{
	int is_new = 0;
	int idx;
	Macro *macro;

	if (!src || !src->name)
		return;

	idx = get_or_add_macro_index(src->name, &is_new);
	macro = idx >= 0 ? &macros[idx] : NULL;
	free_macro(macro);
	copy_macro(macro, src);
	if (is_new)
		macro_hash_insert_index(idx);
}

static void
add_function_macro(const char *name, char **params, int param_count,
                   int is_variadic, const char *value)
{
	int is_new = 0;
	int idx = get_or_add_macro_index(name, &is_new);
	Macro *macro = idx >= 0 ? &macros[idx] : NULL;

	init_macro(macro, name, value);

	macro->is_function_like = 1;
	macro->is_variadic = is_variadic;
	macro->param_count = param_count;
	macro->params = params; /* take ownership; freed by free_macro */
	macro_prepare_replacement_tokens(macro);
	if (is_new)
		macro_hash_insert_index(idx);
}

/* Compatibility wrapper for older code paths. */
static void __attribute__((unused))
add_macro(const char *name, const char *value)
{
	add_object_macro(name, value);
}

void
preprocess_set_asm_dialect(AsmDialect dialect)
{
	configured_asm_dialect = dialect;
	builtins_initialized = 0;
	internal_header_macros_initialized = 0;
}

static void
add_builtin_object_macro(const char *name, const char *value)
{
	add_object_macro(name, value);
}

void
preprocess_set_include_dir(const char *dir)
{
	int i;

	if (!dir || !dir[0]) return;
	for (i = 0; i < include_dir_count; i++) {
		if (STRCMP(include_dirs[i], dir) == 0)
			return;
	}
	if (include_dir_count >= MAX_INCLUDE_DIRS) {
		tcc_warn("too many -I directories (max %d), ignoring: %s", MAX_INCLUDE_DIRS, dir);
		return;
	}
	include_dirs[include_dir_count++] = xstrdup(dir);
	include_cache_clear();
}

void
preprocess_set_bootstrap_includes(int enable)
{
	bootstrap_includes = enable ? 1 : 0;
	include_cache_clear();
}

void
preprocess_set_stdinc(int enable)
{
	stdinc_enabled = enable ? 1 : 0;
	include_cache_clear();
}

void
preprocess_configure(PreprocessTarget target)
{
	configured_target = target;
	builtins_initialized = 0;
	internal_header_macros_initialized = 0;
	builtin_counter_value = 0;
	current_include_dir_index = -1;
	include_cache_clear();
}

void
preprocess_set_line_markers(int enable)
{
	preprocess_emit_line_markers = enable;
}

/* Reset all user-defined (and built-in) macros so the next call to preprocess()
 * starts with a clean macro table.  Call this between compiling separate input
 * files so that include guards from file N do not suppress headers in file N+1. */
void
preprocess_reset_file_macros(void)
{
	int i;
	void *entry;

	for (i = 0; i < macro_count; i++)
		free_macro(&macros[i]);
	macro_count = 0;
	macro_hash_free();
	builtins_initialized = 0;
	internal_header_macros_initialized = 0;
	builtin_counter_value = 0;
	current_include_dir_index = -1;
	preamble_injected = 0;
	for (i = 0; i < include_file_cache_count; i++)
		include_file_cache_entries[i].included_once = 0;

	while (pragma_macro_stack) {
		entry = ((MacroStackEntry *)pragma_macro_stack)->next;
		xfree(((MacroStackEntry *)pragma_macro_stack)->name);
		free_macro(&((MacroStackEntry *)pragma_macro_stack)->macro);
		xfree(pragma_macro_stack);
		pragma_macro_stack = entry;
	}
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

	/* Store __FILE__ as a quoted string-literal macro value (e.g. "\"foo.c\"")
	 * so that it expands correctly in macro bodies and #if expressions.
	 *
	 * Note: the lexer also handles the __FILE__ keyword directly in read_token()
	 * (lexer.c) for the token-stream case, where it sets kind=TOK_STRING with
	 * plain (unquoted) text.  Those two paths are intentionally separate; keep
	 * them in sync if either changes. */
	char buf[512];
	snprintf(buf, sizeof(buf), "\"%s\"", name);
	add_object_macro("__FILE__", buf);
}

static void
init_builtin_macros(void)
{
	const char *stdc_version = NULL;

	if (builtins_initialized)
		return;

	builtins_initialized = 1;

	add_builtin_object_macro("__tcc__", "1");
	add_builtin_object_macro("__TCC__", "1");
	add_builtin_object_macro("__STDC__", "1");
	add_builtin_object_macro("__STDC_HOSTED__", "0");
	add_builtin_object_macro("__STDC_NO_THREADS__", "1");

	switch (tcc_lang_standard) {
	case LANG_C99:
		stdc_version = "199901L";
		break;
	case LANG_C11:
		stdc_version = "201112L";
		break;
	case LANG_C17:
		stdc_version = "201710L";
		break;
	case LANG_C23:
		stdc_version = "202311L";
		break;
	case LANG_C89:
	case LANG_C90:
	default:
		break;
	}

	if (stdc_version)
		add_builtin_object_macro("__STDC_VERSION__", stdc_version);

	/*
	 * Assembly dialect macros are for compiler-owned headers/helpers.
	 * Arbitrary user inline asm is still emitted as user text and is not
	 * translated between Intel and AT&T syntaxes.
	 */
	if (configured_asm_dialect == ASM_DIALECT_GAS_ATT)
		add_builtin_object_macro("__TCC_ASM_ATT__", "1");
	else if (configured_asm_dialect == ASM_DIALECT_GAS_INTEL)
		add_builtin_object_macro("__TCC_ASM_GAS__", "1");
	else
		add_builtin_object_macro("__TCC_ASM_INTEL__", "1");
	add_builtin_object_macro("__TCC_VERSION__", TCC_VERSION_NUMBER_STR);
	add_builtin_object_macro("__TCC_VERSION_STRING__", "\"" TCC_VERSION "\"");
	add_builtin_object_macro("__LINE__", "0");
	add_builtin_object_macro("__COUNTER__", "0");

	/* __FILE__ is set by preprocess_set_file() before preprocess() is called;
	 * only set the default if it hasn't been set yet */
	if (!find_macro_entry("__FILE__"))
		add_builtin_object_macro("__FILE__", "\"unknown\"");

	/* __DATE__ and __TIME__ — set once at init to the compilation timestamp.
	 * C standard requires these to expand to a string-literal token, so the
	 * macro value must include the surrounding double-quotes (like __FILE__). */
	{
		char date_raw[20];
		char time_raw[12];
		char date_buf[24]; /* room for surrounding quotes */
		char time_buf[16];
		static const char *months[] = {
			"Jan","Feb","Mar","Apr","May","Jun",
			"Jul","Aug","Sep","Oct","Nov","Dec"
		};
		time_t now = time(NULL);
		struct tm *tm = localtime(&now);

		if (tm) {
			snprintf(date_raw, sizeof(date_raw), "%s %2d %4d",
			         months[tm->tm_mon], tm->tm_mday, tm->tm_year + 1900);
			snprintf(time_raw, sizeof(time_raw), "%02d:%02d:%02d",
			         tm->tm_hour, tm->tm_min, tm->tm_sec);
		} else {
			snprintf(date_raw, sizeof(date_raw), "Jan  1 1970");
			snprintf(time_raw, sizeof(time_raw), "00:00:00");
		}
		snprintf(date_buf, sizeof(date_buf), "\"%s\"", date_raw);
		snprintf(time_buf, sizeof(time_buf), "\"%s\"", time_raw);
		add_builtin_object_macro("__DATE__", date_buf);
		add_builtin_object_macro("__TIME__", time_buf);
	}

	/*
	 * Note: we intentionally do NOT define __GNUC__ / __clang__ here.
	 * Defining __GNUC__ causes macOS system headers (e.g. _stdio.h) to take
	 * inline-function-definition branches that require full struct layout and
	 * compiler built-ins TCC does not support.  The macros TCC does need for
	 * system header compatibility (__LP64__, __has_feature, __builtin_va_list,
	 * etc.) are defined separately below.
	 * sys/cdefs.h will emit an "Unsupported compiler" warning, but the
	 * architecture guard it gates on (__LP64__ / __ILP32__) is already set,
	 * so the hard #error does not fire.
	 */

	/*
	 * Note: __builtin_va_list is defined as a macro in the compilation
	 * preamble (expanding to char *) so that system header typedefs like
	 * "typedef __builtin_va_list va_list" expand to "typedef char * va_list"
	 * — an identical redeclaration compatible with the va_list already
	 * defined in the preamble.
	 */

	/*
	 * __asm__ and __asm in declaration position are used by macOS system
	 * headers to rename symbols (e.g. int printf(...) __asm__("_printf")).
	 * TCC does not implement this form.  Define them as function-like macros
	 * that swallow their argument so the declaration still parses correctly.
	 * The renamed symbol is not needed since TCC links against the dylib
	 * using the undecorated name anyway.
	 */
	{
		char **p;
		p = (char **)xmalloc(sizeof(char *)); p[0] = xstrdup("x");
		add_function_macro("__asm__", p, 1, 0, "");
		p = (char **)xmalloc(sizeof(char *)); p[0] = xstrdup("x");
		add_function_macro("__asm",   p, 1, 0, "");
	}

	/*
	 * Some libraries select GCC synchronization intrinsics from __GNUC__.
	 * Atomics are not implemented yet, so expose the full-barrier spelling as
	 * a compile-time no-op rather than as an undeclared runtime call.
	 */
	{
		char **p;
		p = NULL;
		add_function_macro("__sync_synchronize", p, 0, 0, "((void)0)");
	}

	/*
	 * __has_feature / __has_extension / __has_attribute / __has_builtin are
	 * clang extensions widely used in system headers to guard feature use.
	 * Define them as function-like macros that always return 0 so that headers
	 * compile without pulling in unsupported extensions.
	 * Each params array is heap-allocated because add_function_macro takes ownership.
	 */
	{
		char **p;
		p = (char **)xmalloc(sizeof(char *)); p[0] = xstrdup("x");
		add_function_macro("__has_feature",   p, 1, 0, "0");
		p = (char **)xmalloc(sizeof(char *)); p[0] = xstrdup("x");
		add_function_macro("__has_extension", p, 1, 0, "0");
		p = (char **)xmalloc(sizeof(char *)); p[0] = xstrdup("x");
		add_function_macro("__has_attribute", p, 1, 0, "0");
		p = (char **)xmalloc(sizeof(char *)); p[0] = xstrdup("x");
		add_function_macro("__has_builtin",   p, 1, 0, "0");
	}

	add_builtin_object_macro("__ORDER_LITTLE_ENDIAN__", "1234");
	add_builtin_object_macro("__ORDER_BIG_ENDIAN__",    "4321");
	add_builtin_object_macro("__BYTE_ORDER__",          "1234"); /* little-endian for all current targets */
	add_builtin_object_macro("__GNUC__",                "4");
	add_builtin_object_macro("__GNUC_MINOR__",          "2");
	add_builtin_object_macro("__GNUC_PATCHLEVEL__",     "1");
	add_builtin_object_macro("__GNUC_STDC_INLINE__",    "1");

	switch (configured_target) {
	case PP_TARGET_X86:
		add_builtin_object_macro("i386", "1");
		add_builtin_object_macro("__i386__", "1");
		add_builtin_object_macro("__unix__", "1");
		add_builtin_object_macro("unix", "1");
		add_builtin_object_macro("__linux__", "1");
		add_builtin_object_macro("linux", "1");
		add_builtin_object_macro("__SIZEOF_POINTER__", "4");
		add_builtin_object_macro("__SIZEOF_LONG__", "4");
		add_builtin_object_macro("__SIZEOF_INT__", "4");
		break;

	case PP_TARGET_X64:
		add_builtin_object_macro("__x86_64__", "1");
		add_builtin_object_macro("__unix__", "1");
		add_builtin_object_macro("unix", "1");
		add_builtin_object_macro("__linux__", "1");
		add_builtin_object_macro("linux", "1");
		add_builtin_object_macro("__LP64__", "1");
		add_builtin_object_macro("__SIZEOF_POINTER__", "8");
		add_builtin_object_macro("__SIZEOF_LONG__", "8");
		add_builtin_object_macro("__SIZEOF_INT__", "4");
		break;

	case PP_TARGET_ARM64:
		add_builtin_object_macro("__aarch64__", "1");
		add_builtin_object_macro("__arm64__", "1");
		add_builtin_object_macro("__APPLE__", "1");
		add_builtin_object_macro("__MACH__", "1");
		add_builtin_object_macro("__LP64__", "1");
		add_builtin_object_macro("__SIZEOF_POINTER__", "8");
		add_builtin_object_macro("__SIZEOF_LONG__", "8");
		add_builtin_object_macro("__SIZEOF_INT__", "4");
		break;

	case PP_TARGET_MIPS:
		add_builtin_object_macro("__mips__", "1");
		add_builtin_object_macro("mips", "1");
		add_builtin_object_macro("__unix__", "1");
		add_builtin_object_macro("unix", "1");
		add_builtin_object_macro("__SIZEOF_POINTER__", "4");
		add_builtin_object_macro("__SIZEOF_LONG__", "4");
		add_builtin_object_macro("__SIZEOF_INT__", "4");
		break;

	case PP_TARGET_M68K:
		add_builtin_object_macro("__m68k__", "1");
		add_builtin_object_macro("m68k", "1");
		add_builtin_object_macro("__unix__", "1");
		add_builtin_object_macro("unix", "1");
		add_builtin_object_macro("__SIZEOF_POINTER__", "4");
		add_builtin_object_macro("__SIZEOF_LONG__", "4");
		add_builtin_object_macro("__SIZEOF_INT__", "4");
		break;
	}
}

static void
init_internal_header_macros(void)
{
	if (bootstrap_includes || internal_header_macros_initialized)
		return;

	internal_header_macros_initialized = 1;

	add_object_macro("_VA_LIST_T",               "1");
	add_object_macro("_STDARG_H",                "1");
	add_object_macro("_ANSI_STDARG_H_",          "1");
	add_object_macro("__DARWIN_VA_LIST_DEFINED", "1");
	/* Skip mach/arm/_structs.h which contains __uint128_t vector fields. */
	add_object_macro("_MACH_ARM__STRUCTS_H_",    "1");
	add_object_macro("__ARM_MCONTEXT_H_",        "1");
	add_object_macro("_STRUCT_UCONTEXT",         "1");
	add_object_macro("_RUNETYPE_H_",             "1");
	add_object_macro("__MATH_H__",               "1");
	add_object_macro("__MATH__",                 "1");
	add_object_macro("_BSM_AUDIT_H",             "1");
	add_object_macro("_SYS_ATTR_H_",             "1");
	add_object_macro("_MALLOC_MALLOC_H_",        "1");
	add_object_macro("_MALLOC_UNDERSCORE_MALLOC_TYPE_H_", "1");
	add_object_macro("_MALLOC_UNDERSCORE_MALLOC_H_", "1");
	add_object_macro("_DONT_USE_CTYPE_INLINE_",  "1");
	/* Satisfy TargetConditionals.h defaults used by Darwin headers. */
	add_object_macro("TARGET_OS_MAC",            "1");
	add_object_macro("TARGET_OS_IPHONE",         "0");
	add_object_macro("TARGET_OS_IOS",            "0");
	add_object_macro("TARGET_OS_WATCH",          "0");
	add_object_macro("TARGET_OS_TV",             "0");
	add_object_macro("TARGET_CPU_ARM64",         "1");
	add_object_macro("TARGET_CPU_X86_64",        "0");
}

static void
remove_macro(const char *name)
{
	int idx;
	int last;

	if (!name) {
		Debug(0,"remove_macro(NULL)\n");
		return;
	}

	idx = find_macro_index(name);
	if (idx < 0)
		return;

	last = macro_count - 1;
	macro_hash_remove_index(macros[idx].name, idx);
	free_macro(&macros[idx]);

	if (idx != last) {
		macros[idx] = macros[last];
		memset(&macros[last], 0, sizeof(Macro));
		macro_hash_update_index(macros[idx].name, last, idx);
	} else {
		memset(&macros[last], 0, sizeof(Macro));
	}

	macro_count--;
}

static void
trim_trailing(char *s)
{
	size_t len = strlen(s);

	while (len > 0 && isspace((unsigned char)s[len - 1]))
		s[--len] = '\0';
}

static char *
try_read_include_file(const char *path, dev_t *out_dev, ino_t *out_ino)
{
	int fd;
	char *buffer;
	size_t cap;
	size_t len;
	struct stat stbuf;
	int have_stat = 0;
	IncludeFileCacheEntry *cached;

	fd = open(path, 0);
	if (fd < 0)
		return NULL;
	Debug(1, "Open path [%s]\n", path);
	if (preprocess_profile_enabled)
		preprocess_profile_data.include_files_opened++;

	if (fstat(fd, &stbuf) == 0) {
		have_stat = 1;
		if (out_dev)
			*out_dev = stbuf.st_dev;
		if (out_ino)
			*out_ino = stbuf.st_ino;
		cached = include_file_cache_find_identity(stbuf.st_dev, stbuf.st_ino);
		if (cached) {
			if (preprocess_profile_enabled)
				preprocess_profile_data.include_file_cache_hits++;
			close(fd);
			if (include_file_cache_should_skip(cached))
				return xstrdup("");
			return include_file_cache_dup_source(cached);
		}
		if (preprocess_profile_enabled)
			preprocess_profile_data.include_file_cache_misses++;
	} else {
		if (out_dev)
			*out_dev = 0;
		if (out_ino)
			*out_ino = 0;
	}

	if (have_stat && stbuf.st_size >= 0) {
		size_t target = (size_t)stbuf.st_size;

		buffer = xmalloc(target + 1);
		len = 0;

		while (len < target) {
			ssize_t nread = read(fd, buffer + len, target - len);
			if (nread < 0) {
				int err = errno;
				xfree(buffer);
				close(fd);
				fatal_pp("failed reading include '%s': %s", path, strerror(err));
			}
			if (nread == 0)
				break;
			len += (size_t)nread;
		}

		buffer[len] = '\0';
		close(fd);
		return buffer;
	}

	cap = 4096;
	len = 0;
	buffer = xmalloc(cap + 1);

	for (;;) {
		ssize_t nread;

		if (len == cap) {
			cap *= 2;
			buffer = xrealloc(buffer, cap + 1);
		}

		nread = read(fd, buffer + len, cap - len);
		if (nread < 0) {
			int err = errno;
			xfree(buffer);
			close(fd);
			fatal_pp("failed reading include '%s': %s", path, strerror(err));
		}
		if (nread == 0)
			break;
		len += (size_t)nread;
	}

	buffer[len] = '\0';
	close(fd);

	return buffer;
}

static char *
read_include_file(const char *path)
{
	char *buffer = try_read_include_file(path, NULL, NULL);
	if (!buffer)
		fatal_pp("include '%s' not found: %s", path, strerror(errno));
	return buffer;
}

static const char *
directive_body_start(const char *q)
{
	const char *p;

	if (q[0] == '%' && q[1] == ':') {
		p = q + 2;
		while (*p == ' ' || *p == '\t')
			p++;
		return p;
	}

	if (*q != '#')
		return NULL;

	p = q + 1;
	while (*p == ' ' || *p == '\t')
		p++;
	return p;
}

typedef enum PPDirectiveKind {
	PP_DIR_NONE = 0,
	PP_DIR_EMPTY,
	PP_DIR_IF,
	PP_DIR_IFDEF,
	PP_DIR_IFNDEF,
	PP_DIR_ELIF,
	PP_DIR_ELSE,
	PP_DIR_ENDIF,
	PP_DIR_DEFINE,
	PP_DIR_UNDEF,
	PP_DIR_INCLUDE,
	PP_DIR_INCLUDE_NEXT,
	PP_DIR_ERROR,
	PP_DIR_WARNING,
	PP_DIR_PRAGMA,
	PP_DIR_LINE,
	PP_DIR_OTHER
} PPDirectiveKind;

static int
directive_word_ends(const char *p, size_t n)
{
	return p[n] == '\0' || isspace((unsigned char)p[n]);
}

static PPDirectiveKind
pp_directive_kind(const char *body)
{
	const char *p;

	if (!body)
		return PP_DIR_NONE;
	if (body[0] == '\0')
		return PP_DIR_EMPTY;

	p = body;
	switch (p[0]) {
	case 'd':
		if (p[1] == 'e' && memcmp(p, "define", 6) == 0 && directive_word_ends(p, 6))
			return PP_DIR_DEFINE;
		break;
	case 'e':
		if (p[1] == 'l') {
			if (memcmp(p, "elif", 4) == 0 && directive_word_ends(p, 4))
				return PP_DIR_ELIF;
			if (memcmp(p, "else", 4) == 0 && directive_word_ends(p, 4))
				return PP_DIR_ELSE;
		} else if (p[1] == 'n') {
			if (memcmp(p, "endif", 5) == 0 && directive_word_ends(p, 5))
				return PP_DIR_ENDIF;
		} else if (p[1] == 'r') {
			if (memcmp(p, "error", 5) == 0 && directive_word_ends(p, 5))
				return PP_DIR_ERROR;
		}
		break;
	case 'i':
		if (p[1] == 'f') {
			if (p[2] == '\0' || isspace((unsigned char)p[2]))
				return PP_DIR_IF;
			if (memcmp(p, "ifdef", 5) == 0 && directive_word_ends(p, 5))
				return PP_DIR_IFDEF;
			if (memcmp(p, "ifndef", 6) == 0 && directive_word_ends(p, 6))
				return PP_DIR_IFNDEF;
		} else if (p[1] == 'n') {
			if (memcmp(p, "include_next", 12) == 0 && directive_word_ends(p, 12))
				return PP_DIR_INCLUDE_NEXT;
			if (memcmp(p, "include", 7) == 0 && directive_word_ends(p, 7))
				return PP_DIR_INCLUDE;
		}
		break;
	case 'l':
		if (memcmp(p, "line", 4) == 0 && directive_word_ends(p, 4))
			return PP_DIR_LINE;
		break;
	case 'p':
		if (memcmp(p, "pragma", 6) == 0 && directive_word_ends(p, 6))
			return PP_DIR_PRAGMA;
		break;
	case 'u':
		if (memcmp(p, "undef", 5) == 0 && directive_word_ends(p, 5))
			return PP_DIR_UNDEF;
		break;
	case 'w':
		if (memcmp(p, "warning", 7) == 0 && directive_word_ends(p, 7))
			return PP_DIR_WARNING;
		break;
	default:
		break;
	}

	return PP_DIR_OTHER;
}

static int
parse_pragma_macro_name(const char *p, char *name, size_t name_size)
{
	while (isspace((unsigned char)*p))
		p++;

	if (*p != '(')
		return 0;
	p++;
	while (isspace((unsigned char)*p))
		p++;
	if (*p != '"')
		return 0;
	p++;

	size_t ni = 0;
	while (*p && *p != '"') {
		char ch = *p++;
		if (ch == '\\' && *p) {
			char esc = *p++;
			if (esc == '"' || esc == '\\')
				ch = esc;
			else
				ch = esc;
		}
		if (!pp_is_ident_char(ch))
			return 0;
		if (ni >= TCC_IDENT_MAX || ni + 1 >= name_size)
			fatal_pp("macro name too long (max %d chars)\n", TCC_IDENT_MAX);
		name[ni++] = ch;
	}

	if (*p != '"')
		return 0;
	p++;
	while (isspace((unsigned char)*p))
		p++;
	if (*p != ')')
		return 0;
	p++;
	while (isspace((unsigned char)*p))
		p++;
	if (*p != '\0')
		return 0;
	if (ni == 0 || !pp_is_ident_start(name[0]))
		return 0;

	name[ni] = '\0';
	return 1;
}

static void
pragma_push_macro(const char *name)
{
	void *entry;
	Macro *macro;

	if (!name || !*name)
		return;

	entry = xcalloc(1, sizeof(MacroStackEntry));
	((MacroStackEntry *)entry)->name = xstrdup(name);
	macro = find_macro_entry(name);
	if (macro) {
		((MacroStackEntry *)entry)->had_macro = 1;
		copy_macro(&((MacroStackEntry *)entry)->macro, macro);
	}
	((MacroStackEntry *)entry)->next = pragma_macro_stack;
	pragma_macro_stack = entry;
}

static void
pragma_pop_macro(const char *name)
{
	void **link = &pragma_macro_stack;

	if (!name || !*name)
		return;

	while (*link) {
		MacroStackEntry *entry = (MacroStackEntry *)*link;
		if (STRCMP(entry->name, name) == 0) {
			*link = entry->next;
			remove_macro(name);
			if (entry->had_macro)
				replace_macro_with_copy(&entry->macro);
			xfree(entry->name);
			free_macro(&entry->macro);
			xfree(entry);
			return;
		}
		link = &entry->next;
	}
}

static int
handle_pragma_directive(const char *pragma_text)
{
	const char *p = pragma_text;
	char macro_name[TCC_IDENT_BUF_SIZE] = {0};

	while (isspace((unsigned char)*p))
		p++;

	if (STRNCMP(p, "push_macro", 10) == 0 && !pp_is_ident_char(p[10])) {
		if (parse_pragma_macro_name(p + 10, macro_name, sizeof(macro_name)))
			pragma_push_macro(macro_name);
		return 1;
	}

	if (STRNCMP(p, "pop_macro", 9) == 0 && !pp_is_ident_char(p[9])) {
		if (parse_pragma_macro_name(p + 9, macro_name, sizeof(macro_name)))
			pragma_pop_macro(macro_name);
		return 1;
	}

	return 0;
}

static void
append_pragma_pack_sentinel(const char *kind, const char *name, int has_pack,
                            int pack, char **out, size_t *len, size_t *cap)
{
	char buf[128];

	if (kind[0] == '\0') {
		if (has_pack)
			snprintf(buf, sizeof(buf), "__pragma_pack__(%d)", pack);
		else
			snprintf(buf, sizeof(buf), "__pragma_pack__(0)");
	} else if (name && name[0] && has_pack) {
		snprintf(buf, sizeof(buf), "__pragma_pack_%s__(\"%s\",%d)", kind, name, pack);
	} else if (name && name[0]) {
		snprintf(buf, sizeof(buf), "__pragma_pack_%s__(\"%s\")", kind, name);
	} else if (has_pack) {
		snprintf(buf, sizeof(buf), "__pragma_pack_%s__(%d)", kind, pack);
	} else {
		snprintf(buf, sizeof(buf), "__pragma_pack_%s__()", kind);
	}

	append_str(out, len, cap, buf);
}

static int
parse_pragma_pack_ident(const char **pp, char *name, size_t name_size)
{
	const char *p = *pp;
	size_t ni = 0;

	if (!pp_is_ident_start(*p))
		return 0;

	while (pp_is_ident_char(*p)) {
		if (ni >= TCC_IDENT_MAX || ni + 1 >= name_size)
			fatal_pp("pragma pack identifier too long (max %d chars)\n", TCC_IDENT_MAX);
		name[ni++] = *p++;
	}

	name[ni] = '\0';
	*pp = p;
	return 1;
}

static int
parse_pragma_pack_number(const char **pp, int *out_num)
{
	const char *p = *pp;
	int num = 0;

	if (!isdigit((unsigned char)*p))
		return 0;

	while (isdigit((unsigned char)*p))
		num = num * 10 + (*p++ - '0');

	*pp = p;
	*out_num = num;
	return 1;
}

static void
skip_pragma_pack_ws(const char **pp)
{
	while (isspace((unsigned char)**pp))
		(*pp)++;
}

static void
warn_unsupported_pragma(const char *pragma_text)
{
	const char *p = pragma_text;
	char name[TCC_IDENT_BUF_SIZE] = {0};
	size_t ni = 0;

	while (isspace((unsigned char)*p))
		p++;

	while (pp_is_ident_char(*p)) {
		if (ni + 1 >= sizeof(name))
			break;
		name[ni++] = *p++;
	}
	name[ni] = '\0';

	if (name[0])
		pp_warn("ignoring unsupported #pragma %s", name);
	else
		pp_warn("ignoring unsupported #pragma");
}

static int
append_pragma_pack_result(const char *pragma_text, char **out, size_t *len,
                          size_t *cap)
{
	const char *p = pragma_text;
	char name[TCC_IDENT_BUF_SIZE] = {0};
	int pack = 0;
	int has_pack = 0;
	int malformed = 0;

	skip_pragma_pack_ws(&p);

	if (STRNCMP(p, "pack", 4) != 0 || p[4] != '(')
		return 0;

	p += 5;
	skip_pragma_pack_ws(&p);

	if (STRNCMP(p, "push", 4) == 0 && !pp_is_ident_char(p[4])) {
		p += 4;
		skip_pragma_pack_ws(&p);
		if (*p == ')') {
			append_pragma_pack_sentinel("push", name, has_pack, pack, out, len, cap);
			return 1;
		}
		if (*p != ',')
			malformed = 1;
		else {
			p++;
			skip_pragma_pack_ws(&p);
			if (parse_pragma_pack_ident(&p, name, sizeof(name))) {
				skip_pragma_pack_ws(&p);
				if (*p == ',') {
					p++;
					skip_pragma_pack_ws(&p);
					has_pack = parse_pragma_pack_number(&p, &pack);
					if (!has_pack)
						malformed = 1;
				} else if (*p != ')') {
					malformed = 1;
				}
			} else {
				has_pack = parse_pragma_pack_number(&p, &pack);
				if (!has_pack)
					malformed = 1;
			}
		}
		skip_pragma_pack_ws(&p);
		if (*p != ')' || p[1] != '\0')
			malformed = 1;
		if (malformed)
			fatal_pp("malformed #pragma pack");
		if (has_pack && !((unsigned)pack <= 16 && ((0x10116u >> pack) & 1u) != 0))
			fatal_pp("invalid #pragma pack value");
		append_pragma_pack_sentinel("push", name, has_pack, pack, out, len, cap);
		return 1;
	}

	if (STRNCMP(p, "pop", 3) == 0 && !pp_is_ident_char(p[3])) {
		p += 3;
		skip_pragma_pack_ws(&p);
		if (*p == ')') {
			append_pragma_pack_sentinel("pop", name, has_pack, pack, out, len, cap);
			return 1;
		}
		if (*p != ',')
			malformed = 1;
		else {
			p++;
			skip_pragma_pack_ws(&p);
			if (parse_pragma_pack_ident(&p, name, sizeof(name))) {
				skip_pragma_pack_ws(&p);
				if (*p == ',') {
					p++;
					skip_pragma_pack_ws(&p);
					has_pack = parse_pragma_pack_number(&p, &pack);
					if (!has_pack)
						malformed = 1;
				} else if (*p != ')') {
					malformed = 1;
				}
			} else {
				has_pack = parse_pragma_pack_number(&p, &pack);
				if (!has_pack)
					malformed = 1;
			}
		}
		skip_pragma_pack_ws(&p);
		if (*p != ')' || p[1] != '\0')
			malformed = 1;
		if (malformed)
			fatal_pp("malformed #pragma pack");
		if (has_pack && !((unsigned)pack <= 16 && ((0x10116u >> pack) & 1u) != 0))
			fatal_pp("invalid #pragma pack value");
		append_pragma_pack_sentinel("pop", name, has_pack, pack, out, len, cap);
		return 1;
	}

	if (*p != ')') {
		has_pack = parse_pragma_pack_number(&p, &pack);
		if (!has_pack)
			malformed = 1;
	}
	skip_pragma_pack_ws(&p);
	if (*p != ')' || p[1] != '\0')
		malformed = 1;
	if (malformed)
		fatal_pp("malformed #pragma pack");
	if (has_pack && !((unsigned)pack <= 16 && ((0x10116u >> pack) & 1u) != 0))
		fatal_pp("invalid #pragma pack value");
	append_pragma_pack_sentinel("", "", has_pack, pack, out, len, cap);
	return 1;
}

static void
skip_expr_ws(const char **expr)
{
	while (isspace((unsigned char)**expr))
		(*expr)++;
}

static int
eval_if_expr(const char *expr);

static int
eval_defined_expr(const char **expr)
{
	const char *p = *expr;
	char name[TCC_IDENT_BUF_SIZE];
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

	if (!pp_source_starts_identifier(p))
		fatal_pp("Expected identifier after defined");

	memset(name, 0, sizeof(name));
	parse_pp_identifier(&p, name, sizeof(name), "preprocessor identifier");

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

	if (STRNCMP(*expr, "defined", 7) == 0 &&
	        !(isalnum((unsigned char)(*expr)[7]) || (*expr)[7] == '_')) {
		return eval_defined_expr(expr);
	}

	if (pp_source_starts_identifier(*expr)) {
		char name[TCC_IDENT_BUF_SIZE] = {0};
		parse_pp_identifier(expr, name, sizeof(name), "preprocessor identifier");
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
	const char *p = line + 6; /* strlen("define") */

	while (isspace((unsigned char)*p))
		p++;

	if (!pp_source_starts_identifier(p)) {
		fatal_pp("Invalid #define name\n");
	}

	char name[TCC_IDENT_BUF_SIZE] = {0};
	parse_pp_identifier(&p, name, sizeof(name), "macro name");

	if (*p == '(') {
		p++;

		/* Dynamic param list — no fixed limit */
		char **params = NULL;
		int param_count = 0;
		int param_cap = 0;
		int is_variadic = 0;

		while (*p && *p != ')') {
			while (isspace((unsigned char)*p))
				p++;

			if (*p == ')')
				break;

			if (p[0] == '.' && p[1] == '.' && p[2] == '.') {
				if (tcc_lang_is_c89_or_c90()) {
					fatal_pp("variadic macros are not allowed in C89/C90 mode");
				}

				if (param_count >= param_cap) {
					param_cap = param_cap ? param_cap * 2 : 8;
					params = xrealloc(params, param_cap * sizeof(char *));
				}
				params[param_count++] = xstrdup("__VA_ARGS__");
				is_variadic = 1;
				p += 3;

				while (isspace((unsigned char)*p))
					p++;

				if (*p != ')') {
					fatal_pp("Variadic macro parameter must be last\n");
				}

				break;
			}

			if (!pp_source_starts_identifier(p)) {
				fatal_pp("Invalid macro parameter\n");
			}

			/* Collect the parameter name into a local buffer, then xstrdup */
			{
				char pname[TCC_IDENT_BUF_SIZE];
				parse_pp_identifier(&p, pname, sizeof(pname), "macro parameter");

				if (param_count >= param_cap) {
					param_cap = param_cap ? param_cap * 2 : 8;
					params = xrealloc(params, param_cap * sizeof(char *));
				}
				params[param_count++] = xstrdup(pname);
			}

			while (isspace((unsigned char)*p))
				p++;

			if (*p == ',') {
				p++;
				continue;
			}

			if (*p != ')') {
				fatal_pp("Expected ',' or ')' in macro parameter list\n");
			}
		}

		if (*p != ')') {
			fatal_pp("Unterminated macro parameter list [%s]\n",line);
		}

		p++;

		while (isspace((unsigned char)*p))
			p++;

		/* xstrdup the full replacement body — no 512-byte truncation */
		char *value = xstrdup(p);
		Debug(5,"Value [%s]\n",value);
		trim_trailing(value);
		/* Strip \x01 sentinels from macro body */
		{
		char *rd = value;
		char *wr = value;
		while (*rd) { if ((unsigned char)*rd != 0x01) *wr++ = *rd; rd++; }
		*wr = '\0';
		}

		add_function_macro(name, params, param_count, is_variadic, value);
		/* params ownership transferred; value was xstrdup'd into macro->value */
		xfree(value);
		return;
	}

	while (isspace((unsigned char)*p))
		p++;

	/* xstrdup the full replacement body — no 512-byte truncation */
	{
		char *value = xstrdup(p);
		trim_trailing(value);
		/* Strip \x01 sentinels (line-count markers from join_line_continuations)
		 * so they don't appear in macro expansions */
		{
		char *rd = value;
		char *wr = value;
		while (*rd) { if ((unsigned char)*rd != 0x01) *wr++ = *rd; rd++; }
		*wr = '\0';
		}
		add_object_macro(name, value);
		xfree(value);
	}
}

static const char *
read_nonboot_compiler_stub_header(const char *path,
                                  char *resolved_path,
                                  size_t resolved_path_size,
                                  int *out_include_dir_index,
                                  IncludeFileCacheEntry **out_file_cache_entry)
{
	char full[512];
	char *buffer;
	dev_t file_dev = 0;
	ino_t file_ino = 0;
	IncludeFileCacheEntry *file_cached;
	int i;

	for (i = 0; i < include_dir_count; i++) {
		const char *dir = include_dirs[i];
		int n;
		if (!dir)
			continue;
		if (!strstr(dir, "/cc/include") && !strstr(dir, "/include/tcc"))
			continue;
		n = snprintf(full, sizeof(full), "%s/%s", dir, path);
		if (n < 0 || (size_t)n >= sizeof(full))
			tcc_error("include path too long: %s/%s", dir, path);
		file_cached = include_file_cache_find(full);
		if (file_cached) {
			if (preprocess_profile_enabled)
				preprocess_profile_data.include_file_cache_hits++;
			if (resolved_path && resolved_path_size) {
				strncpy(resolved_path, full, resolved_path_size - 1);
				resolved_path[resolved_path_size - 1] = '\0';
			}
			if (out_include_dir_index)
				*out_include_dir_index = i;
			if (out_file_cache_entry)
				*out_file_cache_entry = file_cached;
			if (include_file_cache_should_skip(file_cached))
				return pp_empty_include_source;
			return include_file_cache_source(file_cached);
		}
		buffer = try_read_include_file(full, &file_dev, &file_ino);
		if (buffer) {
			if (resolved_path && resolved_path_size) {
				strncpy(resolved_path, full, resolved_path_size - 1);
				resolved_path[resolved_path_size - 1] = '\0';
			}
			if (out_include_dir_index)
				*out_include_dir_index = i;
			file_cached = include_file_cache_store(full, buffer, file_dev, file_ino);
			if (out_file_cache_entry)
				*out_file_cache_entry = file_cached;
			return include_file_cache_source(file_cached);
		}
	}

	return NULL;
}

static const char *
read_include_file_search_from(const char *path, int is_system,
                              int start_include_dir, int *out_include_dir_index,
                              int *out_source_owned,
                              IncludeFileCacheEntry **out_file_cache_entry)
{
	char full[512];
	char *buffer;
	IncludeCacheEntry *cached;
	IncludeFileCacheEntry *file_cached;
	dev_t file_dev = 0;
	ino_t file_ino = 0;
	if (out_include_dir_index)
		*out_include_dir_index = -1;
	if (out_source_owned)
		*out_source_owned = 0;
	if (out_file_cache_entry)
		*out_file_cache_entry = NULL;

	/*
	 * In non-bootstrap mode, the compilation preamble already injects
	 * va_list, va_start, va_arg, va_end and all _STDARG_H guard macros.
	 * The system stdarg.h either doesn't exist in the SDK or is a binary
	 * precompiled file. Return empty content so the #include is a no-op.
	 */
	if (!bootstrap_includes && is_system &&
	    (STRCMP(path, "stdarg.h") == 0 || STRCMP(path, "varargs.h") == 0)) {
		return pp_empty_include_source;
	}

	/*
	 * In non-bootstrap mode, prefer TCC's own stddef.h stub over the host
	 * SDK header so language-version-gated additions such as C23 nullptr_t
	 * are available consistently across platforms.
	 */
	if (!bootstrap_includes && is_system && STRCMP(path, "stddef.h") == 0) {
		char stub_full[512];
		const char *stub_source =
			read_nonboot_compiler_stub_header(path, stub_full, sizeof(stub_full),
			                                  out_include_dir_index,
			                                  out_file_cache_entry);
		if (stub_source) {
			include_cache_store(path, is_system, start_include_dir, 1,
			                    stub_full,
			                    out_include_dir_index ? *out_include_dir_index : -1);
			return stub_source;
		}
	}

	cached = include_cache_find(path, is_system, start_include_dir);
	if (cached) {
		if (preprocess_profile_enabled)
			preprocess_profile_data.include_cache_hits++;
		if (out_include_dir_index)
			*out_include_dir_index = cached->include_dir_index;
		if (cached->found) {
			file_cached = include_file_cache_find(cached->resolved_path);
			if (file_cached && preprocess_profile_enabled)
				preprocess_profile_data.include_file_cache_hits++;
			if (out_file_cache_entry)
				*out_file_cache_entry = file_cached;
			if (include_file_cache_should_skip(file_cached))
				return pp_empty_include_source;
			return include_file_cache_source(file_cached);
		}
	} else if (preprocess_profile_enabled) {
		preprocess_profile_data.include_cache_misses++;
	}

	if (!is_system) {
		/* Direct path — works for absolute paths and relative paths when the
		 * caller's cwd matches the included file's location. */
		file_cached = include_file_cache_find(path);
		if (file_cached) {
			if (preprocess_profile_enabled)
				preprocess_profile_data.include_file_cache_hits++;
			include_cache_store(path, is_system, start_include_dir, 1, path, -1);
			if (out_file_cache_entry)
				*out_file_cache_entry = file_cached;
			if (include_file_cache_should_skip(file_cached))
				return pp_empty_include_source;
			return include_file_cache_source(file_cached);
		}
		buffer = try_read_include_file(path, &file_dev, &file_ino);
		if (buffer) {
			include_cache_store(path, is_system, start_include_dir, 1, path, -1);
			file_cached = include_file_cache_store(path, buffer, file_dev, file_ino);
			if (out_file_cache_entry)
				*out_file_cache_entry = file_cached;
			return include_file_cache_source(file_cached);
		}

		/* Search user-specified include directories (-I flags and TCC_INCLUDE_DIR).
		 * These are the correct portable mechanism for locating headers. */
		{
			int _id;
			for (_id = start_include_dir; _id < include_dir_count; _id++) {
				int n = snprintf(full, sizeof(full), "%s/%s", include_dirs[_id], path);
				if (n < 0 || (size_t)n >= sizeof(full))
					tcc_error("include path too long: %s/%s", include_dirs[_id], path);
				file_cached = include_file_cache_find(full);
				if (file_cached) {
					if (preprocess_profile_enabled)
						preprocess_profile_data.include_file_cache_hits++;
					if (out_include_dir_index)
						*out_include_dir_index = _id;
					include_cache_store(path, is_system, start_include_dir, 1, full, _id);
					if (out_file_cache_entry)
						*out_file_cache_entry = file_cached;
					if (include_file_cache_should_skip(file_cached))
						return pp_empty_include_source;
					return include_file_cache_source(file_cached);
				}
				buffer = try_read_include_file(full, &file_dev, &file_ino);
				if (buffer) {
					if (out_include_dir_index)
						*out_include_dir_index = _id;
					include_cache_store(path, is_system, start_include_dir, 1, full, _id);
					file_cached = include_file_cache_store(full, buffer, file_dev, file_ino);
					if (out_file_cache_entry)
						*out_file_cache_entry = file_cached;
					return include_file_cache_source(file_cached);
				}
			}
		}

		/* Bootstrap-only fallback paths: enabled explicitly by -boot. */
		if (bootstrap_includes) {
			snprintf(full, sizeof(full), "cc/%s", path);
			file_cached = include_file_cache_find(full);
			if (file_cached) {
				if (preprocess_profile_enabled)
					preprocess_profile_data.include_file_cache_hits++;
				include_cache_store(path, is_system, start_include_dir, 1, full, -1);
				if (out_file_cache_entry)
					*out_file_cache_entry = file_cached;
				if (include_file_cache_should_skip(file_cached))
					return pp_empty_include_source;
				return include_file_cache_source(file_cached);
			}
			buffer = try_read_include_file(full, &file_dev, &file_ino);
			if (buffer) {
				include_cache_store(path, is_system, start_include_dir, 1, full, -1);
				file_cached = include_file_cache_store(full, buffer, file_dev, file_ino);
				if (out_file_cache_entry)
					*out_file_cache_entry = file_cached;
				return include_file_cache_source(file_cached);
			}

			snprintf(full, sizeof(full), "codegen/%s", path);
			file_cached = include_file_cache_find(full);
			if (file_cached) {
				if (preprocess_profile_enabled)
					preprocess_profile_data.include_file_cache_hits++;
				include_cache_store(path, is_system, start_include_dir, 1, full, -1);
				if (out_file_cache_entry)
					*out_file_cache_entry = file_cached;
				if (include_file_cache_should_skip(file_cached))
					return pp_empty_include_source;
				return include_file_cache_source(file_cached);
			}
			buffer = try_read_include_file(full, &file_dev, &file_ino);
			if (buffer) {
				include_cache_store(path, is_system, start_include_dir, 1, full, -1);
				file_cached = include_file_cache_store(full, buffer, file_dev, file_ino);
				if (out_file_cache_entry)
					*out_file_cache_entry = file_cached;
				return include_file_cache_source(file_cached);
			}

			snprintf(full, sizeof(full), "cc/codegen/%s", path);
			file_cached = include_file_cache_find(full);
			if (file_cached) {
				if (preprocess_profile_enabled)
					preprocess_profile_data.include_file_cache_hits++;
				include_cache_store(path, is_system, start_include_dir, 1, full, -1);
				if (out_file_cache_entry)
					*out_file_cache_entry = file_cached;
				if (include_file_cache_should_skip(file_cached))
					return pp_empty_include_source;
				return include_file_cache_source(file_cached);
			}
			buffer = try_read_include_file(full, &file_dev, &file_ino);
			if (buffer) {
				include_cache_store(path, is_system, start_include_dir, 1, full, -1);
				file_cached = include_file_cache_store(full, buffer, file_dev, file_ino);
				if (out_file_cache_entry)
					*out_file_cache_entry = file_cached;
				return include_file_cache_source(file_cached);
			}
		}

		/* Last resort: try the bare path again — read_include_file will exit
		 * with a diagnostic if the file cannot be opened. */
		include_cache_store(path, is_system, start_include_dir, 0, NULL, -1);
		if (out_source_owned)
			*out_source_owned = 1;
		return read_include_file(path);
	}

	/* System include (<...>): explicit -I/-isystem directories are always
	 * searched.  Project-local stub headers are bootstrap-only and are enabled
	 * explicitly with -boot. */

	if (bootstrap_includes) {
		snprintf(full, sizeof(full), "cc/include/%s", path);
		file_cached = include_file_cache_find(full);
		if (file_cached) {
			if (preprocess_profile_enabled)
				preprocess_profile_data.include_file_cache_hits++;
			include_cache_store(path, is_system, start_include_dir, 1, full, -1);
			if (out_file_cache_entry)
				*out_file_cache_entry = file_cached;
			if (include_file_cache_should_skip(file_cached))
				return pp_empty_include_source;
			return include_file_cache_source(file_cached);
		}
		buffer = try_read_include_file(full, &file_dev, &file_ino);
		if (buffer) {
			include_cache_store(path, is_system, start_include_dir, 1, full, -1);
			file_cached = include_file_cache_store(full, buffer, file_dev, file_ino);
			if (out_file_cache_entry)
				*out_file_cache_entry = file_cached;
			return include_file_cache_source(file_cached);
		}

		snprintf(full, sizeof(full), "include/%s", path);
		file_cached = include_file_cache_find(full);
		if (file_cached) {
			if (preprocess_profile_enabled)
				preprocess_profile_data.include_file_cache_hits++;
			include_cache_store(path, is_system, start_include_dir, 1, full, -1);
			if (out_file_cache_entry)
				*out_file_cache_entry = file_cached;
			if (include_file_cache_should_skip(file_cached))
				return pp_empty_include_source;
			return include_file_cache_source(file_cached);
		}
		buffer = try_read_include_file(full, &file_dev, &file_ino);
		if (buffer) {
			include_cache_store(path, is_system, start_include_dir, 1, full, -1);
			file_cached = include_file_cache_store(full, buffer, file_dev, file_ino);
			if (out_file_cache_entry)
				*out_file_cache_entry = file_cached;
			return include_file_cache_source(file_cached);
		}
	}

	/* User-specified include directories (-I/-isystem) and configured
	 * standard include directories. */
	{
		int _id;
		for (_id = start_include_dir; _id < include_dir_count; _id++) {
			int n = snprintf(full, sizeof(full), "%s/%s", include_dirs[_id], path);
			if (n < 0 || (size_t)n >= sizeof(full))
				tcc_error("include path too long: %s/%s", include_dirs[_id], path);
			file_cached = include_file_cache_find(full);
			if (file_cached) {
				if (preprocess_profile_enabled)
					preprocess_profile_data.include_file_cache_hits++;
				if (out_include_dir_index)
					*out_include_dir_index = _id;
				include_cache_store(path, is_system, start_include_dir, 1, full, _id);
				if (out_file_cache_entry)
					*out_file_cache_entry = file_cached;
				if (include_file_cache_should_skip(file_cached))
					return pp_empty_include_source;
				return include_file_cache_source(file_cached);
			}
			buffer = try_read_include_file(full, &file_dev, &file_ino);
			if (buffer) {
				if (out_include_dir_index)
					*out_include_dir_index = _id;
				include_cache_store(path, is_system, start_include_dir, 1, full, _id);
				file_cached = include_file_cache_store(full, buffer, file_dev, file_ino);
				if (out_file_cache_entry)
					*out_file_cache_entry = file_cached;
				return include_file_cache_source(file_cached);
			}
		}
	}

	if (!stdinc_enabled) {
		include_cache_store(path, is_system, start_include_dir, 0, NULL, -1);
		fatal_pp("system include <%s> not found (-nostdinc active)", path);
	}

	include_cache_store(path, is_system, start_include_dir, 0, NULL, -1);
	fatal_pp("system include <%s> not found", path);
}

static void
parse_include_spec(const char *spec, char *path, size_t path_size, int *is_system)
{
	const char *p = spec;
	size_t pi = 0;
	char end_ch;

	while (isspace((unsigned char)*p))
		p++;

	*is_system = 0;

	if (*p == '"') {
		end_ch = '"';
	} else if (*p == '<') {
		*is_system = 1;
		end_ch = '>';
	} else {
		fatal_pp("Expected quoted or system include filename\n");
	}

	p++;
	path[0] = '\0';

	while (*p && *p != end_ch) {
		if (pi + 1 >= path_size)
			fatal_pp("include filename too long\n");
		if (*p == '\n' || *p == '\r')
			fatal_pp("Unterminated include filename\n");
		if (pi + 1 < path_size)
			path[pi++] = *p;
		p++;
	}

	if (*p != end_ch)
		fatal_pp("Unterminated include filename\n");

	path[pi] = '\0';
	p++;

	while (isspace((unsigned char)*p))
		p++;

	if (*p != '\0')
		fatal_pp("Unexpected trailing tokens after #include filename\n");
}

static char *
normalize_expanded_include_spec(const char *spec)
{
	PPTokenVec tokens = {0};
	char *normalized = NULL;
	size_t normalized_len = 0;
	size_t normalized_cap = 0;
	int i = 0;

	pp_tokenize_line(spec ? spec : "", &tokens);

	while (i < tokens.count && tokens.items[i].kind == PPTOK_SPACE)
		i++;

	if (i >= tokens.count) {
		pptok_free(&tokens);
		return xstrdup(spec ? spec : "");
	}

	if (tokens.items[i].kind == PPTOK_STRING) {
		append_str(&normalized, &normalized_len, &normalized_cap, tokens.items[i].text);
		i++;
		while (i < tokens.count && tokens.items[i].kind == PPTOK_SPACE)
			i++;
		if (i != tokens.count)
			fatal_pp("Unexpected trailing tokens after #include filename\n");
		pptok_free(&tokens);
		return normalized;
	}

	if (tokens.items[i].kind == PPTOK_PUNCT && STRCMP(tokens.items[i].text, "<") == 0) {
		append_str(&normalized, &normalized_len, &normalized_cap, "<");
		i++;
		while (i < tokens.count) {
			if (tokens.items[i].kind == PPTOK_SPACE) {
				i++;
				continue;
			}
			if (tokens.items[i].kind == PPTOK_PUNCT && STRCMP(tokens.items[i].text, ">") == 0) {
				append_str(&normalized, &normalized_len, &normalized_cap, ">");
				i++;
				while (i < tokens.count && tokens.items[i].kind == PPTOK_SPACE)
					i++;
				if (i != tokens.count)
					fatal_pp("Unexpected trailing tokens after #include filename\n");
				pptok_free(&tokens);
				return normalized;
			}
			append_str(&normalized, &normalized_len, &normalized_cap, tokens.items[i].text);
			i++;
		}
	}

	pptok_free(&tokens);
	xfree(normalized);
	return xstrdup(spec ? spec : "");
}

static void
handle_include_common(const char *line, int directive_len, int include_next,
                      char **out, size_t *len, size_t *cap, int return_line)
{
	const char *p = line + directive_len;
	const char *include_spec = p;
	unsigned long long t0 = 0;
	size_t expanded_cap = 0;
	size_t expanded_len = 0;
	char *expanded = NULL;
	char *normalized_expanded = NULL;
	char path[256];
	int is_system = 0;
	int include_dir_index = -1;
	int saved_include_dir_index;
	int included_source_owned = 0;
	IncludeFileCacheEntry *included_file_entry = NULL;
	char saved_file[512];

	/*
	 * #include first macro-expands its operand, then re-parses the resulting
	 * header-name token sequence as either "..." or <...>.
	 */
	if (line_needs_macro_expansion(p)) {
		MacroDisabledSet disabled = {0};
		expanded_cap = strlen(p) + 64;
		expanded = xcalloc(1, expanded_cap);
		expand_text_recursive(p, &expanded, &expanded_len, &expanded_cap, 0, &disabled);
		normalized_expanded = normalize_expanded_include_spec(expanded);
		include_spec = normalized_expanded;
	}
	parse_include_spec(include_spec, path, sizeof(path), &is_system);

	STRNCPY(saved_file, preprocess_file, 511);
	saved_file[511] = '\0';

	if (include_next) {
		int start_dir = current_include_dir_index >= 0 ? current_include_dir_index + 1 : 0;
		t0 = preprocess_profile_enabled ? pp_monotonic_nanos() : 0;
		const char *included_source = read_include_file_search_from(path, is_system, start_dir,
		                                                            &include_dir_index,
		                                                            &included_source_owned,
		                                                            &included_file_entry);
		if (preprocess_profile_enabled)
			preprocess_profile_data.include_lookup_time += pp_monotonic_nanos() - t0;
		if (preprocess_profile_enabled && included_source[0] == '\0')
			preprocess_profile_data.include_empty_sources++;
		if (included_source[0] == '\0') {
			append_line_marker(out, len, cap, return_line, saved_file);
			if (included_source_owned)
				xfree((char *)included_source);
			xfree(normalized_expanded);
			xfree(expanded);
			return;
		}
		saved_include_dir_index = current_include_dir_index;
		current_include_dir_index = include_dir_index;
		t0 = preprocess_profile_enabled ? pp_monotonic_nanos() : 0;
		char *included_expanded = preprocess_internal(
		    path,
		    (included_file_entry && included_file_entry->normalized_source)
		        ? included_file_entry->normalized_source
		        : included_source,
		    included_file_entry && included_file_entry->normalized_source);
		if (preprocess_profile_enabled)
			preprocess_profile_data.include_child_preprocess_time += pp_monotonic_nanos() - t0;
		current_include_dir_index = saved_include_dir_index;
		preprocess_set_file(saved_file);

		t0 = preprocess_profile_enabled ? pp_monotonic_nanos() : 0;
		append_str(out, len, cap, included_expanded);
		append_char(out, len, cap, '\n');
		append_line_marker(out, len, cap, return_line, saved_file);
		if (preprocess_profile_enabled)
			preprocess_profile_data.include_emit_time += pp_monotonic_nanos() - t0;

		xfree(included_expanded);
		if (included_source_owned)
			xfree((char *)included_source);
		xfree(normalized_expanded);
		xfree(expanded);
		return;
	}

	t0 = preprocess_profile_enabled ? pp_monotonic_nanos() : 0;
	const char *included_source = read_include_file_search_from(path, is_system, 0,
	                                                            &include_dir_index,
	                                                            &included_source_owned,
	                                                            &included_file_entry);
	if (preprocess_profile_enabled)
		preprocess_profile_data.include_lookup_time += pp_monotonic_nanos() - t0;
	if (preprocess_profile_enabled && included_source[0] == '\0')
		preprocess_profile_data.include_empty_sources++;
	if (included_source[0] == '\0') {
		append_line_marker(out, len, cap, return_line, saved_file);
		if (included_source_owned)
			xfree((char *)included_source);
		xfree(expanded);
		return;
	}
	saved_include_dir_index = current_include_dir_index;
	current_include_dir_index = include_dir_index;
	t0 = preprocess_profile_enabled ? pp_monotonic_nanos() : 0;
	char *included_expanded = preprocess_internal(
	    path,
	    (included_file_entry && included_file_entry->normalized_source)
	        ? included_file_entry->normalized_source
	        : included_source,
	    included_file_entry && included_file_entry->normalized_source);
	if (preprocess_profile_enabled)
		preprocess_profile_data.include_child_preprocess_time += pp_monotonic_nanos() - t0;
	current_include_dir_index = saved_include_dir_index;
	preprocess_set_file(saved_file);

	t0 = preprocess_profile_enabled ? pp_monotonic_nanos() : 0;
	append_str(out, len, cap, included_expanded);
	append_line_marker(out, len, cap, return_line, saved_file);
	if (preprocess_profile_enabled)
		preprocess_profile_data.include_emit_time += pp_monotonic_nanos() - t0;

	xfree(included_expanded);
	if (included_source_owned)
		xfree((char *)included_source);
	xfree(normalized_expanded);
	xfree(expanded);
}

static int
token_is(PPToken *tok, const char *text)
{
	return tok && STRCMP(tok->text, text) == 0;
}

static int
param_index(Macro *macro, const char *name)
{
	char **params = macro->params;
	int param_count = macro->param_count;

	for (int i = 0; i < param_count; i++) {
		if (STRCMP(params[i], name) == 0)
			return i;
	}

	return -1;
}

static void
append_token_range_to_str(PPTokenVec *tokens, int start, int end,
                          char **s, size_t *slen, size_t *scap)
{
	PPToken *tok = tokens->items + start;
	PPToken *tok_end = tokens->items + end;

	while (tok < tok_end) {
		append_str(s, slen, scap, tok->text);
		tok++;
	}
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
                   char **args, int *close_index)
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

				{
					size_t alen = 0;
					size_t acap = 64;
					char *abuf = xcalloc(1, acap);
					int slot = (macro->is_variadic && arg_count >= fixed_count) ? fixed_count : arg_count;
					append_token_range_to_str(tokens, arg_start, i, &abuf, &alen, &acap);
					trim_macro_arg(abuf);
					xfree(args[slot]);
					args[slot] = abuf;
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
			{
				size_t alen = 0;
				size_t acap = 64;
				char *abuf = xcalloc(1, acap);
				append_token_range_to_str(tokens, arg_start, i, &abuf, &alen, &acap);
				trim_macro_arg(abuf);
				xfree(args[arg_count]);
				args[arg_count] = abuf;
			}
			arg_count++;
			arg_start = i + 1;
		}
	}

	fatal_pp("Unterminated macro invocation for macro '%s' while parsing argument %d",
	         macro->name, arg_count + 1);
}

static int
macro_disabled(const char *name, const MacroDisabledSet *disabled)
{
	if (!name || !disabled)
		return 0;

	for (int i = 0; i < disabled->count; i++) {
		const char *entry = disabled->names[i];
		if (*entry && STRCMP(name, entry) == 0)
			return 1;
	}

	return 0;
}

static void
macro_disabled_push(MacroDisabledSet *dst, const MacroDisabledSet *src,
                    const char *name)
{
	if (src)
		memcpy(dst, src, sizeof(*dst));
	else
		dst->count = 0;

	if (!name)
		return;

	if (dst->count >= MACRO_DISABLED_MAX)
		fatal_pp("too many disabled macros during macro expansion");

	STRNCPY(dst->names[dst->count], name, MACRO_DISABLED_NAME_LEN - 1);
	dst->names[dst->count][MACRO_DISABLED_NAME_LEN - 1] = '\0';
	dst->count++;
}

static void
expand_text_recursive(const char *text, char **out, size_t *len, size_t *cap,
                      int depth, const MacroDisabledSet *disabled);

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
append_macro_replacement_token(Macro *macro, PPToken *tok, char **expanded_args,
                               char ***pieces, int *piece_count, int *piece_cap)
{
	char *text;

	if (tok->kind == PPTOK_IDENT) {
		int pi = param_index(macro, tok->text);
		text = (pi >= 0 && expanded_args[pi]) ? expanded_args[pi] : tok->text;
	} else {
		text = tok->text;
	}

	if (*piece_count >= *piece_cap) {
		*piece_cap = *piece_cap ? *piece_cap * 2 : 16;
		*pieces = xrealloc(*pieces, (size_t)*piece_cap * sizeof(char *));
	}
	(*pieces)[(*piece_count)++] = xstrdup(text);
}

static void
append_function_macro_expansion(Macro *macro, char **args,
                                char **out, size_t *len, size_t *cap,
                                int depth, const MacroDisabledSet *disabled)
{
	PPTokenVec *replacement;
	int profile_child = preprocess_profile_enabled && preprocess_profile_depth > 1;
	unsigned long long build_start = preprocess_profile_enabled ? pp_monotonic_nanos() : 0;
	unsigned long long arg_expand_time = 0;
	macro_prepare_replacement_tokens(macro);
	replacement = &macro->replacement_tokens;

	/* Expand each argument; result is a heap-allocated string per param */
	char **expanded_args = xcalloc(macro->param_count + 1, sizeof(char *));
	{
		int ei;
		for (ei = 0; ei < macro->param_count; ei++) {
			unsigned long long arg_start = preprocess_profile_enabled ? pp_monotonic_nanos() : 0;
			size_t arg_cap = 1024;
			size_t arg_len = 0;
			char *arg_out = xcalloc(1, arg_cap);
			const char *raw = args[ei] ? args[ei] : "";
			if (!line_needs_macro_expansion(raw))
				append_str(&arg_out, &arg_len, &arg_cap, raw);
			else
				expand_text_recursive(raw, &arg_out, &arg_len, &arg_cap, depth + 1, disabled);
			expanded_args[ei] = arg_out;
			if (preprocess_profile_enabled)
				arg_expand_time += pp_monotonic_nanos() - arg_start;
		}
	}
	if (preprocess_profile_enabled) {
		preprocess_profile_data.function_macro_arg_expand_time += arg_expand_time;
		if (profile_child)
			preprocess_profile_data.child_function_macro_arg_expand_time += arg_expand_time;
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
	/* Dynamic piece array — no fixed limit, no 512-byte truncation */
	char **pieces = NULL;
	int piece_count = 0;
	int piece_cap = 0;

	{
		int ri;
		for (ri = 0; ri < replacement->count; ri++) {
			PPToken *tok = &replacement->items[ri];

			if (tok->kind == PPTOK_SPACE) {
				/* Preserve spaces; append to previous piece to keep ## from seeing them */
				if (piece_count > 0) {
					char *prev = pieces[piece_count - 1];
					size_t plen = strlen(prev);
					char *grown = xrealloc(prev, plen + 2);
					grown[plen] = ' ';
					grown[plen + 1] = '\0';
					pieces[piece_count - 1] = grown;
				}
				continue;
			}

			if (token_is(tok, "#")) {
				ri++;
				while (ri < replacement->count && replacement->items[ri].kind == PPTOK_SPACE)
					ri++;

				if (ri >= replacement->count || replacement->items[ri].kind != PPTOK_IDENT) {
					fatal_pp("# in macro replacement must be followed by a parameter\n");
				}

				int pi = param_index(macro, replacement->items[ri].text);
				if (pi < 0) {
					fatal_pp("# in macro replacement must be followed by a parameter\n");
				}

				/* Stringify: build into a growable buffer */
				{
					size_t slen = 0;
					size_t scap = 64;
					char *sbuf = xcalloc(1, scap);
					const char *raw = args[pi] ? args[pi] : "";
					append_char(&sbuf, &slen, &scap, '"'  );
					/* stringify: escape backslash and double-quote */
					while (*raw) {
						unsigned char ch = (unsigned char)*raw++;
						if (ch == '\\' || ch == '"') append_char(&sbuf, &slen, &scap, '\\');
						append_char(&sbuf, &slen, &scap, (char)ch);
					}
					append_char(&sbuf, &slen, &scap, '"');
					if (piece_count >= piece_cap) {
						piece_cap = piece_cap ? piece_cap * 2 : 16;
						pieces = xrealloc(pieces, (size_t)piece_cap * sizeof(char *));
					}
					pieces[piece_count++] = sbuf;
				}
				continue;
			}

			if (token_is(tok, "##")) {
				if (piece_count == 0) {
					fatal_pp("## cannot appear at start of macro replacement\n");
				}

				ri++;
				while (ri < replacement->count && replacement->items[ri].kind == PPTOK_SPACE)
					ri++;

				if (ri >= replacement->count) {
					fatal_pp("## cannot appear at end of macro replacement\n");
				}

				/* GNU comma swallowing for ##__VA_ARGS__ */
				if (macro->is_variadic &&
				        replacement->items[ri].kind == PPTOK_IDENT &&
				        STRCMP(replacement->items[ri].text, "__VA_ARGS__") == 0 &&
				        piece_count > 0 &&
				        macro_arg_is_empty(expanded_args[macro->param_count - 1] ? expanded_args[macro->param_count - 1] : "")) {
					char *prev_piece = pieces[piece_count - 1];
					size_t plen = strlen(prev_piece);
					while (plen > 0 && prev_piece[plen - 1] == ' ') plen--;
					if (plen == 1 && prev_piece[0] == ',') {
						xfree(pieces[piece_count - 1]);
						piece_count--;
						continue;
					}
				}

				/* Build the RHS string */
				{
					const char *rhs_text;
					char *rhs_trimmed = NULL;
					if (replacement->items[ri].kind == PPTOK_IDENT) {
						int pi = param_index(macro, replacement->items[ri].text);
						if (pi >= 0 && expanded_args[pi]) {
							rhs_trimmed = xstrdup(expanded_args[pi]);
							trim_macro_arg(rhs_trimmed);
							rhs_text = rhs_trimmed;
						} else {
							rhs_text = replacement->items[ri].text;
						}
					} else {
						rhs_text = replacement->items[ri].text;
					}

					/* Trim trailing space from LHS, then concatenate RHS */
					{
						char *lhs = pieces[piece_count - 1];
						size_t lhs_len = strlen(lhs);
						while (lhs_len > 0 && lhs[lhs_len - 1] == ' ')
							lhs[--lhs_len] = '\0';
						size_t rhs_len = strlen(rhs_text);
						char *pasted = xmalloc(lhs_len + rhs_len + 1);
						memcpy(pasted, lhs, lhs_len);
						memcpy(pasted + lhs_len, rhs_text, rhs_len + 1);
						xfree(pieces[piece_count - 1]);
						pieces[piece_count - 1] = pasted;
					}
					xfree(rhs_trimmed);
				}
				continue;
			}

			append_macro_replacement_token(macro, tok, expanded_args, &pieces, &piece_count, &piece_cap);
		}
	}

	/* Emit pieces, inserting separators between non-adjacent tokens.
	 * Check pieces[pi-1] BEFORE freeing it — do not free inside the loop. */
	{
		int pi;
		for (pi = 0; pi < piece_count; pi++) {
			char *piece = pieces[pi];

			if (pi > 0 && *piece) {
				char *prev = pieces[pi - 1]; /* still valid: freed below */
				size_t plen = strlen(prev);
				if (plen > 0 && !isspace((unsigned char)prev[plen - 1]))
					append_str(out, len, cap, " ");
			}

			append_str(out, len, cap, piece);
		}
		for (pi = 0; pi < piece_count; pi++)
			xfree(pieces[pi]);
	}
	xfree(pieces);
	if (preprocess_profile_enabled) {
		unsigned long long build_elapsed = pp_monotonic_nanos() - build_start;
		preprocess_profile_data.function_macro_build_time += build_elapsed;
		if (profile_child)
			preprocess_profile_data.child_function_macro_build_time += build_elapsed;
	}

	/* Free expanded args */
	{
		int ei;
		for (ei = 0; ei < macro->param_count; ei++)
			xfree(expanded_args[ei]);
	}
	xfree(expanded_args);
}

static void
expand_tokens_recursive(PPTokenVec *tokens, char **out, size_t *len, size_t *cap,
                        int depth, const MacroDisabledSet *disabled)
{
	int profile_child = preprocess_profile_enabled && preprocess_profile_depth > 1;

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

			if (macro_disabled(macro->name, disabled)) {
				append_str(out, len, cap, tok->text);
				continue;
			}

			if (!macro->is_function_like) {
				MacroDisabledSet local_disabled;
				unsigned long long macro_start = 0;
				if (preprocess_profile_enabled)
					preprocess_profile_data.object_macro_expansions++;
				macro_start = preprocess_profile_enabled ? pp_monotonic_nanos() : 0;
				if (STRCMP(macro->name, "__COUNTER__") == 0) {
					char counter_buf[32];
					snprintf(counter_buf, sizeof(counter_buf), "%d",
					         builtin_counter_value++);
					append_str(out, len, cap, counter_buf);
					if (preprocess_profile_enabled) {
						unsigned long long elapsed = pp_monotonic_nanos() - macro_start;
						preprocess_profile_data.object_macro_time += elapsed;
						if (profile_child)
							preprocess_profile_data.child_object_macro_time += elapsed;
					}
					continue;
				}
				macro_disabled_push(&local_disabled, disabled, macro->name);

				if (!line_needs_macro_expansion(macro->value))
					append_str(out, len, cap, macro->value);
				else {
					macro_prepare_replacement_tokens(macro);
					expand_tokens_recursive(&macro->replacement_tokens, out, len, cap,
					                        depth + 1, &local_disabled);
				}
				if (preprocess_profile_enabled) {
					unsigned long long elapsed = pp_monotonic_nanos() - macro_start;
					preprocess_profile_data.object_macro_time += elapsed;
					if (profile_child)
						preprocess_profile_data.child_object_macro_time += elapsed;
				}
				continue;
			}

			int j = i + 1;
			while (j < tokens->count && tokens->items[j].kind == PPTOK_SPACE)
				j++;

			if (j >= tokens->count || !token_is(&tokens->items[j], "(")) {
				append_str(out, len, cap, tok->text);
				continue;
			}

			/* Allocate per-argument heap strings; collect_macro_args fills them */
			unsigned long long macro_start = preprocess_profile_enabled ? pp_monotonic_nanos() : 0;
			char **args = xcalloc(macro->param_count + 1, sizeof(char *));
			int close_index = 0;
			unsigned long long arg_collect_start = preprocess_profile_enabled ? pp_monotonic_nanos() : 0;
			int arg_count = collect_macro_args(tokens, j, macro, args, &close_index);
			if (preprocess_profile_enabled)
				preprocess_profile_data.function_macro_expansions++;
			if (preprocess_profile_enabled) {
				unsigned long long arg_collect_elapsed = pp_monotonic_nanos() - arg_collect_start;
				preprocess_profile_data.function_macro_arg_collect_time += arg_collect_elapsed;
				if (profile_child)
					preprocess_profile_data.child_function_macro_arg_collect_time +=
					    arg_collect_elapsed;
			}

			if (arg_count != macro->param_count) {
				{
					int _fi;
					for (_fi = 0; _fi < macro->param_count + 1; _fi++) xfree(args[_fi]);
				}
				xfree(args);
				fatal_pp("Macro argument count [%d,%d] mismatch for %s\n",
				         arg_count, macro->param_count, macro->name);
			}

			size_t tmp_cap = 1024;
			size_t tmp_len = 0;
			char *tmp = xcalloc(1, tmp_cap);

			append_function_macro_expansion(macro, args, &tmp, &tmp_len, &tmp_cap, depth + 1, disabled);
			/* args elements freed inside append_function_macro_expansion via expand */
			{
				int _fi;
				for (_fi = 0; _fi < macro->param_count + 1; _fi++) xfree(args[_fi]);
			}
			xfree(args);

			MacroDisabledSet local_disabled;
			unsigned long long rescan_start = preprocess_profile_enabled ? pp_monotonic_nanos() : 0;
			macro_disabled_push(&local_disabled, disabled, macro->name);

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
			if (!line_needs_macro_expansion(tmp))
				append_str(&mid, &mid_len, &mid_cap, tmp);
			else
				expand_text_recursive(tmp, &mid, &mid_len, &mid_cap, depth + 1,
				                      &local_disabled);

			int consumed_suffix_to = close_index;
			int j2 = close_index + 1;
			while (j2 < tokens->count && tokens->items[j2].kind == PPTOK_SPACE)
				j2++;

			if (j2 < tokens->count && token_is(&tokens->items[j2], "(")) {
				unsigned long long tail_start = preprocess_profile_enabled ? pp_monotonic_nanos() : 0;
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
					        !macro_disabled(tail_macro->name, disabled)) {
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
							                      disabled);
							consumed_suffix_to = k2;
							xfree(combo);
						}
					}
				}

				pptok_free(&mid_tokens);
				if (preprocess_profile_enabled) {
					unsigned long long tail_elapsed = pp_monotonic_nanos() - tail_start;
					preprocess_profile_data.function_macro_tail_time += tail_elapsed;
					if (profile_child)
						preprocess_profile_data.child_function_macro_tail_time +=
						    tail_elapsed;
				}
			}

			if (consumed_suffix_to == close_index)
				append_str(out, len, cap, mid);

			xfree(mid);
			xfree(tmp);
			if (preprocess_profile_enabled) {
				unsigned long long now = pp_monotonic_nanos();
				preprocess_profile_data.macro_rescan_time += now - rescan_start;
				preprocess_profile_data.function_macro_time += now - macro_start;
				if (profile_child) {
					preprocess_profile_data.child_macro_rescan_time += now - rescan_start;
					preprocess_profile_data.child_function_macro_time += now - macro_start;
				}
			}

			i = consumed_suffix_to;
			continue;
		}

		append_str(out, len, cap, tok->text);
	}
}

static void
expand_text_recursive(const char *text, char **out, size_t *len, size_t *cap,
                      int depth, const MacroDisabledSet *disabled)
{
	unsigned long long t0 = 0;
	unsigned long long tokenize_start = 0;
	PPTokenVec tokens;
	if (preprocess_profile_enabled) {
		preprocess_profile_data.macro_expand_calls++;
		preprocess_profile_data.tokenize_calls++;
		t0 = pp_monotonic_nanos();
	}
	tokens.items = 0;
	tokens.count = 0;
	tokens.cap = 0;
	tokenize_start = preprocess_profile_enabled ? pp_monotonic_nanos() : 0;
	pp_tokenize_line(text, &tokens);
	if (preprocess_profile_enabled) {
		unsigned long long tokenize_elapsed = pp_monotonic_nanos() - tokenize_start;
		preprocess_profile_data.macro_tokenize_time += tokenize_elapsed;
		if (preprocess_profile_depth > 1)
			preprocess_profile_data.child_macro_tokenize_time += tokenize_elapsed;
	}
	expand_tokens_recursive(&tokens, out, len, cap, depth, disabled);
	pptok_free(&tokens);
	if (preprocess_profile_enabled) {
		preprocess_profile_data.macro_expand_time += pp_monotonic_nanos() - t0;
		if (preprocess_profile_depth > 1)
			preprocess_profile_data.child_macro_expand_time += pp_monotonic_nanos() - t0;
	}
}

static void
expand_line(const char *line, char **out, size_t *len, size_t *cap)
{
	MacroDisabledSet disabled;
	disabled.count = 0;
	expand_text_recursive(line, out, len, cap, 0, &disabled);
}

static void
append_pragma_operator_result(const char *pragma_text, char **out,
                              size_t *len, size_t *cap)
{
	if (handle_pragma_directive(pragma_text))
		return;

	if (append_pragma_pack_result(pragma_text, out, len, cap))
		return;

	warn_unsupported_pragma(pragma_text);
}

static char *
apply_pragma_operators_owned(char *line)
{
	const char *p = line;
	size_t line_len;
	size_t out_cap;
	size_t out_len = 0;
	char *out;

	if (!strstr(line, "_Pragma"))
		return line;

	line_len = strlen(line);
	out_cap = line_len + 64;
	out = xcalloc(1, out_cap);

	while (*p) {
		if ((p == line || !pp_is_ident_char(p[-1])) &&
		    STRNCMP(p, "_Pragma", 7) == 0 &&
		    !pp_is_ident_char(p[7])) {
			if (!tcc_lang_at_least(LANG_C99))
				fatal_pp("_Pragma is not allowed in C89/C90 mode\n");
			const char *q = p + 7;
			while (isspace((unsigned char)*q))
				q++;
			if (*q == '(') {
				q++;
				while (isspace((unsigned char)*q))
					q++;
				if (*q == '"') {
					size_t pragma_cap = 64;
					size_t pragma_len = 0;
					char *pragma = xcalloc(1, pragma_cap);
					while (*q == '"') {
						q++;
						while (*q && *q != '"') {
							char ch = *q++;
							if (ch == '\\' && *q) {
								char esc = *q++;
								if (esc == '"' || esc == '\\')
									ch = esc;
								else if (esc == 'n')
									ch = '\n';
								else if (esc == 't')
									ch = '\t';
								else
									ch = esc;
							}
							append_char(&pragma, &pragma_len, &pragma_cap, ch);
						}
						if (*q == '"')
							q++;
						while (isspace((unsigned char)*q))
							q++;
					}
					while (isspace((unsigned char)*q))
						q++;
					if (*q == ')') {
						q++;
						append_pragma_operator_result(pragma, &out, &out_len, &out_cap);
						xfree(pragma);
						p = q;
						continue;
					}
					xfree(pragma);
				}
			}
		}

		append_char(&out, &out_len, &out_cap, *p++);
	}

	xfree(line);
	return out;
}

static char *
expand_if_expr_macros(const char *expr)
{
	/*
	 * #if/#elif expressions must macro-expand before evaluation, but the
	 * special defined operator must be evaluated before normal expansion so
	 * that defined(FOO) does not become defined(1).
	 */
	size_t protected_cap = strlen(expr) + 64;
	size_t protected_len = 0;
	char *protected_expr = xcalloc(1, protected_cap);
	const char *p = expr;

	while (*p) {
		if (pp_source_starts_identifier(p) &&
		    STRNCMP(p, "defined", 7) == 0 &&
		    !(isalnum((unsigned char)p[7]) || p[7] == '_')) {
			char name[TCC_IDENT_BUF_SIZE] = {0};
			int paren = 0;
			int value;
			const char *q = p + 7;

			while (isspace((unsigned char)*q))
				q++;

			if (*q == '(') {
				paren = 1;
				q++;
				while (isspace((unsigned char)*q))
					q++;
			}

			if (!pp_source_starts_identifier(q)) {
				/*
				 * Leave malformed defined expressions for eval_if_expr(),
				 * which will produce the normal diagnostic.
				 */
				append_str(&protected_expr, &protected_len, &protected_cap, "defined");
				p += 7;
				continue;
			}

			parse_pp_identifier(&q, name, sizeof(name), "preprocessor identifier");

			while (isspace((unsigned char)*q))
				q++;

			if (paren) {
				if (*q != ')') {
					append_str(&protected_expr, &protected_len, &protected_cap, "defined");
					p += 7;
					continue;
				}
				q++;
			}

			value = is_defined(name);
			append_char(&protected_expr, &protected_len, &protected_cap, value ? '1' : '0');
			p = q;
			continue;
		}

		append_char(&protected_expr, &protected_len, &protected_cap, *p++);
	}

	size_t expanded_cap = strlen(protected_expr) + 64;
	size_t expanded_len = 0;
	char *expanded = xcalloc(1, expanded_cap);
	MacroDisabledSet disabled;
	disabled.count = 0;

	expand_text_recursive(protected_expr, &expanded, &expanded_len, &expanded_cap,
	                      0, &disabled);

	xfree(protected_expr);
	return expanded;
}


static void
skip_quoted_pp_text(const char **pp, int quote)
{
	const char *p = *pp;

	if (*p == quote)
		p++;

	while (*p) {
		if (*p == '\\') {
			if (p[1])
				p += 2;
			else
				p++;
			continue;
		}

		if (*p == quote) {
			p++;
			break;
		}

		p++;
	}

	*pp = p;
}

static void
skip_quoted_pp_text_span(const char **pp, const char *end, int quote)
{
	const char *p = *pp;

	if (p < end && *p == quote)
		p++;

	while (p < end) {
		if (*p == '\\') {
			if (p + 1 < end)
				p += 2;
			else
				p++;
			continue;
		}

		if (*p == quote) {
			p++;
			break;
		}

		p++;
	}

	*pp = p;
}

static int
paren_depth_after_open(const char *p)
{
	int depth = 0;

	for (; *p; p++) {
		if (*p == '"' || *p == '\'') {
			skip_quoted_pp_text(&p, *p);
			p--;
			continue;
		}

		if (p[0] == '/' && p[1] == '/')
			break;

		if (p[0] == '/' && p[1] == '*') {
			p += 2;
			while (*p && !(p[0] == '*' && p[1] == '/'))
				p++;
			if (*p)
				p++;
			continue;
		}

		if (*p == '(')
			depth++;
		else if (*p == ')') {
			depth--;
			if (depth == 0)
				return 0;
		}
	}

	return depth > 0;
}

static int
line_has_open_function_macro_invocation(const char *line)
{
	const char *p = line;

	while (*p) {
		if (*p == '"' || *p == '\'') {
			skip_quoted_pp_text(&p, *p);
			continue;
		}

		if (p[0] == '/' && p[1] == '/')
			break;

		if (p[0] == '/' && p[1] == '*') {
			p += 2;
			while (*p && !(p[0] == '*' && p[1] == '/'))
				p++;
			if (*p)
				p += 2;
			continue;
		}

		if (pp_source_starts_identifier(p)) {
			char name[TCC_IDENT_BUF_SIZE];
			const char *name_start = p;
			Macro *macro;

			parse_pp_identifier(&p, name, sizeof(name), "macro name");

			if (name[0] == '\0')
				continue;

			macro = find_macro_entry(name);
			if (!macro || !macro->is_function_like)
				continue;

			while (*p && isspace((unsigned char)*p))
				p++;

			if (*p != '(')
				continue;

			if (paren_depth_after_open(p))
			{
				return 1;
			}

			(void)name_start;

			continue;
		}

		p++;
	}

	return 0;
}

static int
line_needs_macro_expansion_span(const char *line, const char *line_end)
{
	const char *p = line;

	while (p < line_end) {
		if ((unsigned char)*p == 0x01) {
			p++;
			continue;
		}

		if (*p == '"' || *p == '\'') {
			skip_quoted_pp_text_span(&p, line_end, *p);
			continue;
		}

		if (p + 1 < line_end && p[0] == '/' && p[1] == '/')
			break;

		if (p + 1 < line_end && p[0] == '/' && p[1] == '*') {
			p += 2;
			while (p < line_end && !(p + 1 < line_end && p[0] == '*' && p[1] == '/'))
				p++;
			if (p + 1 < line_end)
				p += 2;
			continue;
		}

		if (pp_source_starts_identifier(p)) {
			Macro *macro;
			const char *name_start = p;
			const char *scan = p;
			char name[TCC_IDENT_BUF_SIZE];
			size_t name_len = 0;
			int copied_name = 0;

			while (scan < line_end) {
				int cp = 0;
				int ucn_len = 0;

				if ((unsigned char)*scan == 0x01) {
					if (!copied_name) {
						const char *copy = name_start;
						while (copy < scan) {
							if ((unsigned char)*copy != 0x01 &&
							    name_len + 1 < sizeof(name))
								name[name_len++] = *copy;
							copy++;
						}
					}
					copied_name = 1;
					scan++;
					continue;
				}

				if (pp_peek_ucn_codepoint(scan, &cp, &ucn_len) &&
				    scan + ucn_len <= line_end) {
					if (!pp_ucn_is_ident_char(cp))
						break;
					if (!pp_ucn_is_valid_identifier_codepoint(cp))
						fatal_pp("invalid universal character name in identifier");
					if (!copied_name) {
						const char *copy = name_start;
						while (copy < scan) {
							if ((unsigned char)*copy != 0x01 &&
							    name_len + 1 < sizeof(name))
								name[name_len++] = *copy;
							copy++;
						}
					}
					if (name_len + (size_t)ucn_len >= sizeof(name) ||
					    name_len + (size_t)ucn_len > TCC_IDENT_MAX)
						fatal_pp("macro name too long (max %d chars)\n", TCC_IDENT_MAX);
					memcpy(name + name_len, scan, (size_t)ucn_len);
					name_len += (size_t)ucn_len;
					scan += ucn_len;
					copied_name = 1;
					continue;
				}

				if (!pp_is_ident_char(*scan))
					break;

				if (copied_name) {
					if (name_len + 1 >= sizeof(name) || name_len + 1 > TCC_IDENT_MAX)
						fatal_pp("macro name too long (max %d chars)\n", TCC_IDENT_MAX);
					name[name_len++] = *scan;
				}
				scan++;
			}

			p = scan;
			if (copied_name) {
				if (!name_len) {
					const char *copy = name_start;
					while (copy < scan) {
						if ((unsigned char)*copy != 0x01 &&
						    name_len + 1 < sizeof(name))
							name[name_len++] = *copy;
						copy++;
					}
				}
				name[name_len] = '\0';
				if (STRCMP(name, "_Pragma") == 0)
					return 1;
				macro = find_macro_entry(name);
			} else {
				if ((size_t)(p - name_start) == 7 &&
				    memcmp(name_start, "_Pragma", 7) == 0)
					return 1;
				macro = find_macro_entry_n(name_start, (size_t)(p - name_start));
			}

			if (!macro)
				continue;

			if (!macro->is_function_like)
				return 1;

			while (p < line_end && isspace((unsigned char)*p))
				p++;
			if (p < line_end && *p == '(')
				return 1;

			continue;
		}

		p++;
	}

	return 0;
}

static int
line_needs_macro_expansion(const char *line)
{
	return line_needs_macro_expansion_span(line, line + strlen(line));
}

static size_t
append_physical_line(char **line, const char *next_line, size_t next_len)
{
	size_t old_len = strlen(*line);
	size_t new_cap = old_len + next_len + 2;
	char *new_line = xrealloc(*line, new_cap);

	*line = new_line;
	(*line)[old_len++] = ' ';
	memcpy(*line + old_len, next_line, next_len);
	(*line)[old_len + next_len] = '\0';
	return new_cap;
}

static void
append_non_sentinel_range(char **out, size_t *len, size_t *cap,
                          const char *start, const char *end)
{
	const char *p = start;
	size_t span_len = (size_t)(end - start);

	if (!memchr(start, '\x01', span_len)) {
		if (span_len != 0) {
			ensure_append_capacity(out, cap, *len + span_len);
			memcpy(*out + *len, start, span_len);
			*len += span_len;
			(*out)[*len] = '\0';
		}
		return;
	}

	while (p < end) {
		const char *chunk = p;
		while (p < end && (unsigned char)*p != 0x01)
			p++;
		{
			size_t chunk_len = (size_t)(p - chunk);
			if (chunk_len != 0) {
				ensure_append_capacity(out, cap, *len + chunk_len);
				memcpy(*out + *len, chunk, chunk_len);
				*len += chunk_len;
				(*out)[*len] = '\0';
			}
		}
		if (p < end)
			p++;
	}
}

static void
join_line_continuations_inplace(char *input)
{
	char *rd;
	char *wr;

	if (!strchr(input, '\\'))
		return;

	rd = input;
	wr = input;
	while (*rd) {
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
		if (rd[0] == '\\' && rd[1] == '\n') {
			/* \x01 marks a removed backslash-newline for line counting */
			*wr++ = '\x01';
			rd += 2;
			continue;
		}

		if (rd[0] == '\\' && rd[1] == '\r' && rd[2] == '\n') {
			*wr++ = ' ';
			rd += 3;
			continue;
		}

		*wr++ = *rd++;
	}

	*wr = '\0';
}

static char
pp_translate_trigraph_char(char ch)
{
	switch (ch) {
	case '=': return '#';
	case '/': return '\\';
	case '\'': return '^';
	case '(': return '[';
	case ')': return ']';
	case '!': return '|';
	case '<': return '{';
	case '>': return '}';
	case '-': return '~';
	default:  return '\0';
	}
}

static void
translate_trigraphs_inplace(char *input)
{
	char *rd;
	char *wr;

	if (!strstr(input, "??"))
		return;

	rd = input;
	wr = input;
	while (*rd) {
		char mapped = '\0';

		if (rd[0] == '?' && rd[1] == '?' && rd[2]) {
			mapped = pp_translate_trigraph_char(rd[2]);
			if (mapped) {
				*wr++ = mapped;
				rd += 3;
				continue;
			}
		}

		*wr++ = *rd++;
	}

	*wr = '\0';
}

static void
strip_comments_inplace(char *input)
{
	const char *scan;
	char *rd;
	char *wr;
	char *next;

	scan = strchr(input, '/');
	while (scan) {
		next = (char *)scan + 1;
		while ((unsigned char)*next == 0x01)
			next++;
		if (*next == '/' || *next == '*')
			break;
		scan = strchr(scan + 1, '/');
	}
	if (!scan)
		return;

	rd = input;
	wr = input;
	int in_string = 0;
	int in_char = 0;

	while (*rd) {
		char ch = *rd++;

		if (in_string) {
			*wr++ = ch;

			if (ch == '\\' && *rd) {
				*wr++ = *rd++;
				continue;
			}

			if (ch == '"')
				in_string = 0;

			continue;
		} else if (in_char) {
			*wr++ = ch;

			if (ch == '\\' && *rd) {
				*wr++ = *rd++;
				continue;
			}

			if (ch == '\'')
				in_char = 0;

			continue;
		} else if (ch == '"') {
			in_string = 1;
			*wr++ = ch;
			continue;
		} else if (ch == '\'') {
			in_char = 1;
			*wr++ = ch;
			continue;
		} else if (ch == '/') {
			next = rd;
			while ((unsigned char)*next == 0x01)
				next++;

			if (*next == '/') {
				if (tcc_lang_is_c89_or_c90())
					fatal_pp("line comments are not allowed in C89/C90 mode\n");

				*wr++ = ' ';
				rd = next + 1;
				while (*rd && *rd != '\n')
					rd++;

				if (*rd == '\n')
					*wr++ = '\n';

				continue;
			}

			if (*next == '*') {
				*wr++ = ' ';
				rd = next + 1;
				int closed = 0;

				while (*rd) {
					if (*rd == '*') {
						next = rd + 1;
						while ((unsigned char)*next == 0x01)
							next++;
						if (*next == '/') {
							rd = next + 1;
							closed = 1;
							break;
						}
					}

					if (*rd == '\n')
						*wr++ = '\n';

					rd++;
				}

				if (!closed) {
					fatal_pp("Unterminated block comment\n");
				}

				continue;
			}
		}

		*wr++ = ch;
	}

	*wr = '\0';
}

static char *
strip_comments(const char *input)
{
	char *out = xstrdup(input ? input : "");
	strip_comments_inplace(out);
	return out;
}

static char *
preprocess_normalize_source(const char *input)
{
	char *normalized = xstrdup(input ? input : "");
	translate_trigraphs_inplace(normalized);
	join_line_continuations_inplace(normalized);
	strip_comments_inplace(normalized);
	return normalized;
}

char *
preprocess(const char *filename, const char *input)
{
	return preprocess_internal(filename, input, 0);
}

static char *
preprocess_internal(const char *filename, const char *input,
                    int skip_translation_phases)
{
	const char *trace_boot = getenv("TCC_TRACE_PP_BOOT");
	unsigned long long t0 = 0;
	unsigned long long directive_start = 0;
	unsigned long long normalize_start = 0;
	unsigned long long branch_start = 0;
	unsigned long original_input_len = (unsigned long)strlen(input);
	int profile_root = preprocess_profile_enabled && preprocess_profile_depth == 0;
	int profile_child = preprocess_profile_enabled && preprocess_profile_depth > 0;
	if (profile_root)
		preprocess_profile_reset();
	if (preprocess_profile_enabled) {
		preprocess_profile_depth++;
		preprocess_profile_data.files_processed++;
		preprocess_profile_data.bytes_input += original_input_len;
	}

	preprocess_set_file(filename);
	if (trace_boot && trace_boot[0]) {
		fprintf(stderr, "PPBOOT set_file file=%s\n",
		        filename ? filename : "<input>");
		fflush(stderr);
	}

	pp_line=0;

	if (trace_boot && trace_boot[0]) {
		fprintf(stderr, "PPBOOT init_builtin_macros enter\n");
		fflush(stderr);
	}
	init_builtin_macros();
	if (trace_boot && trace_boot[0]) {
		fprintf(stderr, "PPBOOT init_builtin_macros done\n");
		fflush(stderr);
	}
	if (trace_boot && trace_boot[0]) {
		fprintf(stderr, "PPBOOT init_internal_header_macros enter\n");
		fflush(stderr);
	}
	init_internal_header_macros();
	if (trace_boot && trace_boot[0]) {
		fprintf(stderr, "PPBOOT init_internal_header_macros done\n");
		fflush(stderr);
	}

	/*
	 * Prepend compiler-internal type definitions that system headers rely on.
	 * These are injected as real C declarations rather than macros so that
	 * repeated identical typedefs in different system headers are treated as
	 * compatible redeclarations rather than conflicts.
	 *
	 * __builtin_va_list: used by macOS/Linux system headers in:
	 *   typedef __builtin_va_list va_list;
	 *   typedef __builtin_va_list __gnuc_va_list;
	 * We define it as char* — TCC's internal va_list representation.
	 *
	 * The preamble is not emitted under -boot (bootstrap_includes) since
	 * the stub headers handle this themselves.
	 */
	/*
	 * Set va_list guard macros directly so the stub stdarg.h skips its
	 * typedef and va_start/va_arg/va_end definitions — these are provided
	 * by the preamble below instead.
	 */
	char *input_with_preamble = NULL;
	char *normalized_input = NULL;
	if (!bootstrap_includes && !preamble_injected) {
		char line_directive[256];
		size_t plen;
		size_t ilen;
		size_t llen;
		int pi;
		int ii;
		int li;
		int oi;
		preamble_injected = 1;
		plen = strlen(tcc_preamble);
		ilen = strlen(input);
		/* Emit "#line 1 filename" AFTER the preamble to reset line counter */
		snprintf(line_directive, sizeof(line_directive),
		         "#line 1 \"%s\"\n", filename ? filename : "<input>");
		llen = strlen(line_directive);
		input_with_preamble = xmalloc(plen + llen + ilen + 1);
		pi = 0; ii = 0; li = 0; oi = 0;
		while (tcc_preamble[pi]) input_with_preamble[oi++] = tcc_preamble[pi++];
		while (line_directive[li]) input_with_preamble[oi++] = line_directive[li++];
		while (input[ii])   input_with_preamble[oi++] = input[ii++];
		input_with_preamble[oi] = '\0';
		input = input_with_preamble;
		skip_translation_phases = 0;
	}

	/*
	 * Macro definitions intentionally persist across recursive preprocess()
	 * calls. This lets #include behave like textual inclusion: macros defined
	 * in included files are visible to later lines in the including file.
	 *
	 * Before directive processing, perform translation phase 2-style
	 * backslash-newline splicing so directives and macro replacement lists can
	 * span physical source lines.
	 */
	t0 = preprocess_profile_enabled ? pp_monotonic_nanos() : 0;
	if (!skip_translation_phases) {
		if (trace_boot && trace_boot[0]) {
			fprintf(stderr, "PPBOOT normalize enter\n");
			fflush(stderr);
		}
		normalized_input = xstrdup(input ? input : "");
		translate_trigraphs_inplace(normalized_input);
		if (preprocess_profile_enabled) {
			preprocess_profile_data.trigraph_time += pp_monotonic_nanos() - t0;
			if (profile_child)
				preprocess_profile_data.child_trigraph_time += pp_monotonic_nanos() - t0;
		}
		t0 = preprocess_profile_enabled ? pp_monotonic_nanos() : 0;
		join_line_continuations_inplace(normalized_input);
		if (preprocess_profile_enabled) {
			preprocess_profile_data.join_time += pp_monotonic_nanos() - t0;
			if (profile_child)
				preprocess_profile_data.child_join_time += pp_monotonic_nanos() - t0;
		}
		t0 = preprocess_profile_enabled ? pp_monotonic_nanos() : 0;
		strip_comments_inplace(normalized_input);
		if (preprocess_profile_enabled) {
			preprocess_profile_data.strip_comments_time += pp_monotonic_nanos() - t0;
			if (profile_child)
				preprocess_profile_data.child_strip_comments_time += pp_monotonic_nanos() - t0;
		}
		input = normalized_input;
		if (trace_boot && trace_boot[0]) {
			fprintf(stderr, "PPBOOT normalize done\n");
			fflush(stderr);
		}
	}

	IfState *if_stack = xcalloc(64, sizeof(IfState));
	int if_depth = 0;
	char *line_buf = NULL;
	size_t line_buf_cap = 0;

	size_t cap = strlen(input) * 2 + 1024;
	if (cap < 1024)
		cap = 1024;

	char *out = xcalloc(1, cap);
	size_t len = 0;

	append_line_marker(&out, &len, &cap, 1, preprocess_file);
	if (trace_boot && trace_boot[0]) {
		fprintf(stderr, "PPBOOT first_line_marker done len=%lu cap=%lu\n",
		        (unsigned long)len, (unsigned long)cap);
		fflush(stderr);
	}

	if (preprocess_emit_line_markers) {
		/* already emitted above */
		(void)0;
	}

	const char *p = input;
	int current_line = 1;
	preprocess_current_line = current_line;
	if (trace_boot && trace_boot[0]) {
		fprintf(stderr, "PPBOOT loop enter firstch=%d\n",
		        *p ? (int)(unsigned char)*p : 0);
		fflush(stderr);
	}
	while (*p) {
		if (preprocess_profile_enabled)
			preprocess_profile_data.lines_processed++;
		preprocess_current_line = current_line;
		const char *line_start = p;
		const char *raw_q;
		int raw_is_directive;
		int raw_needs_macro_expansion = 0;
		int skipping;
		PPDirectiveKind directive_kind = PP_DIR_NONE;
		char *line = NULL;
		const char *directive_body = NULL;

		while (*p && *p != '\n')
			p++;

		skipping = if_depth > 0 && if_stack[if_depth - 1].skipping;
		if (preprocess_profile_enabled && skipping)
			preprocess_profile_data.skipped_lines++;
		raw_q = line_start;
		while (raw_q < p) {
			unsigned char ch = (unsigned char)*raw_q;
			if (ch == 0x01) {
				raw_q++;
				continue;
			}
			if (!isspace(ch))
				break;
			raw_q++;
		}
		raw_is_directive = raw_q < p &&
			(*raw_q == '#' || (raw_q + 1 < p && raw_q[0] == '%' && raw_q[1] == ':'));
		if (skipping && !raw_is_directive)
			goto advance_line;
		if (!skipping && !raw_is_directive) {
			raw_needs_macro_expansion = line_needs_macro_expansion_span(line_start, p);
			if (!raw_needs_macro_expansion) {
				append_non_sentinel_range(&out, &len, &cap, line_start, p);
				append_char(&out, &len, &cap, '\n');
				goto advance_line;
			}
		}

		size_t line_len = (size_t)(p - line_start);
		if (line_len + 1 > line_buf_cap) {
			line_buf_cap = line_len + 1;
			line_buf = xrealloc(line_buf, line_buf_cap);
		}
		line = line_buf;
		memcpy(line, line_start, line_len);
		line[line_len] = '\0';
		/* Strip \x01 sentinel markers (used for line counting only) */
		if (memchr(line, '\x01', line_len)) {
			char *rd = line;
			char *wr = line;
			while (*rd) {
				if ((unsigned char)*rd != 0x01)
					*wr++ = *rd;
				rd++;
			}
			*wr = '\0';
		}
		pp_line=line;
		Debug(5,"%d start=%p p=%p [%x] [%s]\n",current_line, line_start,(void *)p,0,"");

		const char *q = line;
		while (isspace((unsigned char)*q))
			q++;
		int is_directive = (*q == '#' || (*q == '%' && q[1] == ':'));

		if (is_directive) {
			directive_body = directive_body_start(q);
			if (preprocess_profile_enabled) {
				normalize_start = pp_monotonic_nanos();
				preprocess_profile_data.directive_normalize_time +=
				    pp_monotonic_nanos() - normalize_start;
			}
		}
		if (preprocess_profile_enabled && is_directive) {
			preprocess_profile_data.directives_seen++;
			directive_start = pp_monotonic_nanos();
		}
		if (is_directive)
			directive_kind = pp_directive_kind(directive_body);

		if (directive_kind == PP_DIR_IFNDEF) {
			branch_start = preprocess_profile_enabled ? pp_monotonic_nanos() : 0;
			if (if_depth >= 64) {
				fatal_pp("Too many nested preprocessor conditionals\n");
			}

			char name[TCC_IDENT_BUF_SIZE] = {0};
			const char *p = directive_body + 6;
			while (isspace((unsigned char)*p))
				p++;
			parse_pp_identifier(&p, name, sizeof(name), "preprocessor identifier");

			int parent_skipping = if_depth > 0 && if_stack[if_depth - 1].skipping;
			int taken = !is_defined(name);

			if_stack[if_depth].parent_skipping = parent_skipping;
			if_stack[if_depth].taken = taken;
			if_stack[if_depth].skipping = parent_skipping || !taken;
			if_depth++;
			if (preprocess_profile_enabled) {
				preprocess_profile_data.conditionals_seen++;
				preprocess_profile_data.conditional_time += pp_monotonic_nanos() - branch_start;
				if (profile_child)
					preprocess_profile_data.child_conditional_time += pp_monotonic_nanos() - branch_start;
			}
		} else if (directive_kind == PP_DIR_IFDEF) {
			branch_start = preprocess_profile_enabled ? pp_monotonic_nanos() : 0;
			if (if_depth >= 64) {
				fatal_pp("Too many nested preprocessor conditionals\n");
			}

			char name[TCC_IDENT_BUF_SIZE] = {0};
			const char *p = directive_body + 5;
			while (isspace((unsigned char)*p))
				p++;
			parse_pp_identifier(&p, name, sizeof(name), "preprocessor identifier");

			int parent_skipping = if_depth > 0 && if_stack[if_depth - 1].skipping;
			int taken = is_defined(name);

			if_stack[if_depth].parent_skipping = parent_skipping;
			if_stack[if_depth].taken = taken;
			if_stack[if_depth].skipping = parent_skipping || !taken;
			if_depth++;
			if (preprocess_profile_enabled) {
				preprocess_profile_data.conditionals_seen++;
				preprocess_profile_data.conditional_time += pp_monotonic_nanos() - branch_start;
				if (profile_child)
					preprocess_profile_data.child_conditional_time += pp_monotonic_nanos() - branch_start;
			}
		} else if (directive_kind == PP_DIR_IF) {
			branch_start = preprocess_profile_enabled ? pp_monotonic_nanos() : 0;
			if (if_depth >= 64) {
				fatal_pp("Too many nested preprocessor conditionals\n");
			}

			const char *expr = directive_body + 2;
			while (isspace((unsigned char)*expr))
				expr++;

			/*
			 * Macro-expand #if expressions before evaluation. The helper
			 * evaluates defined(...) first so normal macro expansion does not
			 * rewrite its operand.
			 */
			char *expanded_expr = expand_if_expr_macros(expr);
			int value = eval_if_expr(expanded_expr);
			xfree(expanded_expr);
			int parent_skipping = if_depth > 0 && if_stack[if_depth - 1].skipping;
			int taken = value != 0;

			if_stack[if_depth].parent_skipping = parent_skipping;
			if_stack[if_depth].taken = taken;
			if_stack[if_depth].skipping = parent_skipping || !taken;
			if_depth++;
			if (preprocess_profile_enabled) {
				preprocess_profile_data.conditionals_seen++;
				preprocess_profile_data.conditional_time += pp_monotonic_nanos() - branch_start;
				if (profile_child)
					preprocess_profile_data.child_conditional_time += pp_monotonic_nanos() - branch_start;
			}
		} else if (directive_kind == PP_DIR_ELIF) {
			branch_start = preprocess_profile_enabled ? pp_monotonic_nanos() : 0;
			if (if_depth == 0) {
				fatal_pp("#elif without matching #if\n");
			}

			IfState *state = &if_stack[if_depth - 1];

			if (state->taken || state->parent_skipping) {
				state->skipping = 1;
			} else {
				const char *expr = directive_body + 4;
				while (isspace((unsigned char)*expr))
					expr++;

				char *expanded_expr = expand_if_expr_macros(expr);
				int value = eval_if_expr(expanded_expr);
				xfree(expanded_expr);

				if (value) {
					state->taken = 1;
					state->skipping = 0;
				} else {
					state->skipping = 1;
				}
			}
			if (preprocess_profile_enabled) {
				preprocess_profile_data.conditionals_seen++;
				preprocess_profile_data.conditional_time += pp_monotonic_nanos() - branch_start;
				if (profile_child)
					preprocess_profile_data.child_conditional_time += pp_monotonic_nanos() - branch_start;
			}
		} else if (directive_kind == PP_DIR_ELSE) {
			branch_start = preprocess_profile_enabled ? pp_monotonic_nanos() : 0;
			if (if_depth == 0) {
				fatal_pp("#else without matching #if\n");
			}

			IfState *state = &if_stack[if_depth - 1];

			if (state->taken || state->parent_skipping) {
				state->skipping = 1;
			} else {
				state->taken = 1;
				state->skipping = 0;
				/* Re-sync line numbers: we just transitioned from skipping to emitting */
				{
				int next_line = current_line + 1;
				if (current_line > 80 || preprocess_emit_line_markers)
					append_line_marker(&out, &len, &cap, next_line, preprocess_file);
				}
			}
			if (preprocess_profile_enabled) {
				preprocess_profile_data.conditionals_seen++;
				preprocess_profile_data.conditional_time += pp_monotonic_nanos() - branch_start;
				if (profile_child)
					preprocess_profile_data.child_conditional_time += pp_monotonic_nanos() - branch_start;
			}
		} else if (directive_kind == PP_DIR_ENDIF) {
			branch_start = preprocess_profile_enabled ? pp_monotonic_nanos() : 0;
			if (if_depth == 0) {
				fatal_pp("#endif without matching #if\n");
			}

			if_depth--;
			/* Re-sync line numbers after conditional block.
			 * Skipped lines were counted but not emitted, so emit a #line
			 * directive so the parser sees correct locations. */
			{
			int next_line = current_line + 1;
			/* Skip preamble lines to avoid spurious markers */
			if (current_line > 80 || preprocess_emit_line_markers)
				append_line_marker(&out, &len, &cap, next_line, preprocess_file);
			}
			if (preprocess_profile_enabled) {
				preprocess_profile_data.conditionals_seen++;
				preprocess_profile_data.conditional_time += pp_monotonic_nanos() - branch_start;
				if (profile_child)
					preprocess_profile_data.child_conditional_time += pp_monotonic_nanos() - branch_start;
			}
		} else if (skipping) {
			branch_start = preprocess_profile_enabled ? pp_monotonic_nanos() : 0;
			Debug(1, "PP SKIP %d [%s]: %s\n", current_line, preprocess_file ? preprocess_file : "<?>", line);
			/* ignore skipped line */
			if (preprocess_profile_enabled && is_directive) {
				preprocess_profile_data.skipped_directives_seen++;
				preprocess_profile_data.skipped_directive_time += pp_monotonic_nanos() - branch_start;
				if (profile_child)
					preprocess_profile_data.child_skipped_directive_time += pp_monotonic_nanos() - branch_start;
			}
		} else if (directive_kind == PP_DIR_DEFINE) {
			branch_start = preprocess_profile_enabled ? pp_monotonic_nanos() : 0;
			handle_define(directive_body);
			if (preprocess_profile_enabled) {
				preprocess_profile_data.defines_seen++;
				preprocess_profile_data.define_time += pp_monotonic_nanos() - branch_start;
				if (profile_child)
					preprocess_profile_data.child_define_time += pp_monotonic_nanos() - branch_start;
			}
		} else if (directive_kind == PP_DIR_UNDEF) {
			branch_start = preprocess_profile_enabled ? pp_monotonic_nanos() : 0;
			{
				const char *p = directive_body + 5;
				char name[TCC_IDENT_BUF_SIZE];

				name[0] = '\0';
				while (isspace((unsigned char)*p))
					p++;
				parse_pp_identifier(&p, name, sizeof(name), "preprocessor identifier");
				remove_macro(name);
			}
			if (preprocess_profile_enabled) {
				preprocess_profile_data.undefs_seen++;
				preprocess_profile_data.undef_time += pp_monotonic_nanos() - branch_start;
			}
		} else if (directive_kind == PP_DIR_INCLUDE) {
			branch_start = preprocess_profile_enabled ? pp_monotonic_nanos() : 0;
			handle_include_common(directive_body, 7, 0, &out, &len, &cap, current_line + 1);
			if (preprocess_profile_enabled) {
				preprocess_profile_data.includes_seen++;
				preprocess_profile_data.include_time += pp_monotonic_nanos() - branch_start;
				if (profile_child)
					preprocess_profile_data.child_include_time += pp_monotonic_nanos() - branch_start;
			}
		} else if (directive_kind == PP_DIR_INCLUDE_NEXT) {
			branch_start = preprocess_profile_enabled ? pp_monotonic_nanos() : 0;
			handle_include_common(directive_body, 12, 1, &out, &len, &cap, current_line + 1);
			if (preprocess_profile_enabled) {
				preprocess_profile_data.includes_seen++;
				preprocess_profile_data.include_time += pp_monotonic_nanos() - branch_start;
				if (profile_child)
					preprocess_profile_data.child_include_time += pp_monotonic_nanos() - branch_start;
			}
		} else if (directive_kind == PP_DIR_ERROR) {
			branch_start = preprocess_profile_enabled ? pp_monotonic_nanos() : 0;
			if (preprocess_profile_enabled) {
				preprocess_profile_data.errors_seen++;
				preprocess_profile_data.error_time += pp_monotonic_nanos() - branch_start;
			}
			fatal_pp("#error%s\n", directive_body + 5);
		} else if (directive_kind == PP_DIR_WARNING) {
			branch_start = preprocess_profile_enabled ? pp_monotonic_nanos() : 0;
			pp_warn("%s", directive_body + 7);
			if (preprocess_profile_enabled) {
				preprocess_profile_data.warnings_seen++;
				preprocess_profile_data.warning_time += pp_monotonic_nanos() - branch_start;
			}
		} else if (directive_kind == PP_DIR_PRAGMA) {
			branch_start = preprocess_profile_enabled ? pp_monotonic_nanos() : 0;
			const char *p2 = directive_body + 6;
			while (isspace((unsigned char)*p2)) p2++;
			if (handle_pragma_directive(p2)) {
				if (preprocess_profile_enabled) {
					preprocess_profile_data.pragmas_seen++;
					preprocess_profile_data.pragma_time += pp_monotonic_nanos() - branch_start;
				}
				goto directive_done;
			}
			if (append_pragma_pack_result(p2, &out, &len, &cap)) {
				append_char(&out, &len, &cap, '\n');
				if (preprocess_profile_enabled) {
					preprocess_profile_data.pragmas_seen++;
					preprocess_profile_data.pragma_time += pp_monotonic_nanos() - branch_start;
				}
				goto directive_done;
			}
			warn_unsupported_pragma(p2);
			if (preprocess_profile_enabled) {
				preprocess_profile_data.pragmas_seen++;
				preprocess_profile_data.pragma_time += pp_monotonic_nanos() - branch_start;
			}
		} else if (directive_kind == PP_DIR_LINE) {
			const char *trace_line = getenv("TCC_TRACE_LINE");
			branch_start = preprocess_profile_enabled ? pp_monotonic_nanos() : 0;
			/*
			 * #line linenum ["filename"]
			 *
			 * The tokens after #line are macro-expanded before the line number
			 * is parsed. This is required for cases such as:
			 *
			 *   #define line 1000
			 *   #line line
			 */
			const char *p2 = directive_body + 4;
			while (isspace((unsigned char)*p2))
				p2++;

			size_t expanded_cap = strlen(p2) + 64;
			size_t expanded_len = 0;
			char *expanded = xcalloc(1, expanded_cap);
			MacroDisabledSet disabled;
			disabled.count = 0;
			if (trace_line && trace_line[0])
				fprintf(stderr, "TRACE_LINE before expand: [%s]\n", p2);

			expand_text_recursive(p2, &expanded, &expanded_len, &expanded_cap,
			                      0, &disabled);
			if (trace_line && trace_line[0])
				fprintf(stderr, "TRACE_LINE after expand: [%s]\n", expanded);

			p2 = expanded;
			while (isspace((unsigned char)*p2))
				p2++;
			if (trace_line && trace_line[0])
				fprintf(stderr, "TRACE_LINE after skip ws: [%s]\n", p2);

			if (isdigit((unsigned char)*p2)) {
				int lineno = 0;
				while (isdigit((unsigned char)*p2))
					lineno = lineno * 10 + (*p2++ - '0');
				if (trace_line && trace_line[0])
					fprintf(stderr, "TRACE_LINE parsed lineno=%d\n", lineno);

				/* #line sets the line number for the NEXT line, so current_line
				 * will be incremented at end of loop — set to lineno-1 */
				current_line = lineno - 1;
				char line_num[32];
				snprintf(line_num, sizeof(line_num), "%d", lineno);
				add_object_macro("__LINE__", line_num);
				if (trace_line && trace_line[0])
					fprintf(stderr, "TRACE_LINE updated __LINE__=%s\n", line_num);

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
					if (trace_line && trace_line[0])
						fprintf(stderr, "TRACE_LINE updated __FILE__=%s\n", fbuf);
				}
			}

				/* Re-emit #line to output so lexer tracks source position */
				if (trace_line && trace_line[0])
					fprintf(stderr, "TRACE_LINE before append marker line=%d file=%s\n",
					        current_line + 1, preprocess_file ? preprocess_file : "<input>");
				append_line_marker(&out, &len, &cap, current_line + 1, preprocess_file);
				if (trace_line && trace_line[0])
					fprintf(stderr, "TRACE_LINE after append marker\n");
			xfree(expanded);
			if (trace_line && trace_line[0])
				fprintf(stderr, "TRACE_LINE after free expanded\n");
			if (preprocess_profile_enabled) {
				preprocess_profile_data.lines_seen++;
				preprocess_profile_data.line_time += pp_monotonic_nanos() - branch_start;
			}
		} else if (directive_kind == PP_DIR_EMPTY) {
			/* empty # directive — ignore */
			if (preprocess_profile_enabled && is_directive)
				preprocess_profile_data.other_directives_seen++;
		} else {
			int logical_needs_macro_expansion = raw_needs_macro_expansion;
			int line_extended = 0;
			if (preprocess_profile_enabled && is_directive)
				branch_start = pp_monotonic_nanos();
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
				line_extended = 1;
				p++;
				current_line++;
				preprocess_current_line = current_line;

				const char *next_start = p;
				while (*p && *p != '\n')
					p++;

				size_t next_len = (size_t)(p - next_start);
				size_t joined_cap = append_physical_line(&line, next_start, next_len);
				/* append_physical_line reallocated the buffer; the
				 * growable line_buf must track the new pointer AND its
				 * true allocated capacity. Syncing only when the pointer
				 * changed left line_buf_cap stale-small after an in-place
				 * realloc, so a later fill overran the block. */
				line_buf = line;
				line_buf_cap = joined_cap;
				pp_line = line;
			}

			Debug(1, "PP line %d [%s]: %s\n", current_line, preprocess_file ? preprocess_file : "<?>", line);
			if (is_directive || line_extended)
				logical_needs_macro_expansion = line_needs_macro_expansion(line);
			if (!logical_needs_macro_expansion) {
				append_str(&out, &len, &cap, line);
			} else {
				size_t expanded_cap = strlen(line) + 64;
				size_t expanded_len = 0;
				char *expanded = xcalloc(1, expanded_cap);

				expand_line(line, &expanded, &expanded_len, &expanded_cap);
				{
					char *pragma_applied = apply_pragma_operators_owned(expanded);
					append_str(&out, &len, &cap, pragma_applied);
					xfree(pragma_applied);
				}
			}
			append_char(&out, &len, &cap, '\n');
			if (preprocess_profile_enabled && is_directive) {
				preprocess_profile_data.other_directives_seen++;
				preprocess_profile_data.other_directive_time += pp_monotonic_nanos() - branch_start;
			}
		}
directive_done:
		if (preprocess_profile_enabled && is_directive) {
			preprocess_profile_data.directive_time += pp_monotonic_nanos() - directive_start;
			if (profile_child)
				preprocess_profile_data.child_directive_time += pp_monotonic_nanos() - directive_start;
		}

advance_line:
		if (*p == '\n')
			p++;

		/* Count \x01 sentinels from join_line_continuations to track
		 * physical lines consumed by backslash-newline splicing */
		{
		const char *scan = line_start;
		while (scan < p) {
			if ((unsigned char)*scan == 0x01) current_line++;
			scan++;
		}
		}

		current_line++;
		preprocess_current_line = current_line;
	}

	if (if_depth != 0) {
		xfree(if_stack);
		xfree(line_buf);
		xfree(normalized_input);
		xfree(input_with_preamble);
		fatal_pp("Unterminated preprocessor conditional\n");
	}

	xfree(if_stack);
	xfree(line_buf);
	xfree(normalized_input);
	xfree(input_with_preamble);
	/* Strip any \x01 sentinels that leaked into the output (e.g. from macro expansions) */
	{
	char *rd = out;
	char *wr = out;
	while (*rd) { if ((unsigned char)*rd != 0x01) *wr++ = *rd; rd++; }
	*wr = '\0';
	}
	if (preprocess_profile_enabled) {
		preprocess_profile_data.bytes_output += (unsigned long)strlen(out);
		preprocess_profile_depth--;
	}
	return out;
}
