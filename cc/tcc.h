#ifndef TCC_H
#define TCC_H

#include "target.h"

#include "tcc_version.h"

#define TCC_IDENT_MAX 63
#define TCC_IDENT_BUF_SIZE (TCC_IDENT_MAX + 1)

extern 	int tcc_strcmp(const char *,const char *,const char *,int,const char *);
extern 	int tcc_strncmp(const char *,const char *,size_t, const char *,int,const char *);
extern	char *tcc_strncpy(char *dst,const char *src,size_t n,const char *file,int line,const char *expr);
extern	char *xstrdup(const char *s);
extern	unsigned tcc_hash_string(const char *s);
extern	double tcc_monotonic_seconds(void);
extern	unsigned tcc_double_bits_to_float_bits(unsigned long long bits);

void *xmalloc_impl(size_t size, const char *file, int line, const char *func);
void *xcalloc_impl(size_t count, size_t size, const char *file, int line, const char *func);
void *xrealloc_impl(void *ptr, size_t size, const char *file, int line, const char *func);
void xfree_impl(void *ptr, const char *file, int line, const char *func);

#define xmalloc(size)        xmalloc_impl((size), __FILE__, __LINE__, __func__)
#define xcalloc(count, size) xcalloc_impl((count), (size), __FILE__, __LINE__, __func__)
#define xrealloc(ptr, size)  xrealloc_impl((ptr), (size), __FILE__, __LINE__, __func__)
#define xfree(ptr)           xfree_impl((ptr), __FILE__, __LINE__, __func__)

typedef enum LangStandard {
	LANG_C89 = 0,
	LANG_C90,
	LANG_C99,
	LANG_C11,
	LANG_C17,
	LANG_C23
} LangStandard;

extern LangStandard tcc_lang_standard;
extern int tcc_iso_diagnostics;

static inline int
tcc_lang_is_c89_or_c90(void)
{
	return tcc_lang_standard <= LANG_C90;
}

static inline int
tcc_lang_at_least(LangStandard std)
{
	return tcc_lang_standard >= std;
}

void 	Debug(int lvl, const char *fmt, ...);

#ifdef __TCC__
/* Under stage1 self-hosted compilation, avoid complex macro expansions with
 * stringification that the stage1 parser/lexer can't handle reliably.
 * Also avoid the 5/6-argument wrapped string helper calls while bootstrapping:
 * they have repeatedly been a source of self-hosted ABI/codegen failures.
 * Keep allocator wrappers, but route string ops straight to libc in stage1. */
#define TCC_BOOT_FUNC "<bootstrap>"
#ifndef TCC_RAW_STRING_FUNCS
#define TCC_RAW_STRING_FUNCS 1
#endif
#define STRCMP(a,b)      strcmp((a), (b))
#define STRNCMP(a,b,c)   strncmp((a), (b), (c))
#define STRNCPY(a,b,c)   strncpy((a), (b), (c))
#define xmalloc(sz)      xmalloc_impl((sz), __FILE__, __LINE__, TCC_BOOT_FUNC)
#define xcalloc(n,sz)    xcalloc_impl((n),(sz), __FILE__, __LINE__, TCC_BOOT_FUNC)
#define xrealloc(p,sz)   xrealloc_impl((p),(sz), __FILE__, __LINE__, TCC_BOOT_FUNC)
#define xfree(p)         xfree_impl((p), __FILE__, __LINE__, TCC_BOOT_FUNC)
#else
#define STRCMP(a,b)	tcc_strcmp((a), (b), __FILE__, __LINE__, #a " , " #b)
#define STRNCMP(a,b,c)	tcc_strncmp((a), (b), (c), __FILE__, __LINE__, #a " , " #b " , " #c)
#define STRNCPY(a,b,c)	tcc_strncpy((a), (b), (c),  __FILE__, __LINE__, #a " , " #b " , " #c )
#endif

/*
 * Route selected raw libc string calls through guarded helpers in compiler
 * translation units.  The helpers opt out before including this header so
 * they can call the real libc entry points directly.
 */
#ifndef TCC_RAW_STRING_FUNCS
#ifdef strcmp
#undef strcmp
#endif
#ifdef strncmp
#undef strncmp
#endif
#ifdef strncpy
#undef strncpy
#endif
#define strcmp(a,b)      STRCMP((a), (b))
#define strncmp(a,b,c)   STRNCMP((a), (b), (c))
#define strncpy(a,b,c)   STRNCPY((a), (b), (c))
#endif

void tcc_set_debug(int level);
void tcc_set_warnings(int enabled); /* 0 = suppress all warnings (-w) */
void tcc_set_warnings_as_errors(int enabled); /* 1 = -Werror */
int tcc_warnings_enabled(void);
int tcc_warnings_as_errors_enabled(void);
void tcc_exit_failure(void) __attribute__((noreturn));

/* Compiler diagnostics - all output goes to stderr */
void fatal_lex(const char *file, int line, int col, const char *fmt, ...) __attribute__((noreturn));
void tcc_warn(const char *fmt, ...);   /* warning, no exit */
void tcc_error(const char *fmt, ...) __attribute__((noreturn));  /* error + exit, no location context */

/* Shared asm output utility */
void emit_raw_string_literal(const char *value);
void emit_raw_string_literal_len(const char *value, size_t len);

/* Internal compiler error - use the ICE() macro for file/line capture */
void fatal_ice(const char *file, int line, const char *fmt, ...) __attribute__((noreturn));
#define ICE(...) fatal_ice(__FILE__, __LINE__, __VA_ARGS__)


#endif /* TCC_H */
