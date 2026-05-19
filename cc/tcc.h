#ifndef VERSION_H
#define VERSION_H

extern 	int tcc_strcmp(const char *,const char *,const char *,int,const char *);
extern	char *tcc_strcpy(char *dst,const char *src,const char *file,int line,const char *expr);
extern	char *tcc_strncpy(char *dst,const char *src,size_t n,const char *file,int line,const char *expr);
extern	char *xstrndup(const char *s, size_t n);
extern	char *xstrdup(const char *s);

void *xmalloc_impl(size_t size, const char *file, int line, const char *func);
void *xcalloc_impl(size_t count, size_t size, const char *file, int line, const char *func);
void *xrealloc_impl(void *ptr, size_t size, const char *file, int line, const char *func);
void xfree_impl(void *ptr, const char *file, int line, const char *func);

#define xmalloc(size)        xmalloc_impl((size), __FILE__, __LINE__, __func__)
#define xcalloc(count, size) xcalloc_impl((count), (size), __FILE__, __LINE__, __func__)
#define xrealloc(ptr, size)  xrealloc_impl((ptr), (size), __FILE__, __LINE__, __func__)
#define xfree(ptr)           xfree_impl((ptr), __FILE__, __LINE__, __func__)
void 	Debug(int lvl, const char *fmt, ...);

#ifdef __TCC__
/* Under stage1 self-hosted compilation, avoid complex macro expansions with
 * __FILE__, __LINE__, and stringification that the stage1 parser/lexer can't
 * handle reliably. Use the plain standard library functions instead. */
#define STRCMP(a,b)      strcmp((a),(b))
#define STRNCPY(a,b,c)   strncpy((a),(b),(c))
#define xmalloc(sz)      malloc(sz)
#define xcalloc(n,sz)    calloc((n),(sz))
#define xrealloc(p,sz)   realloc((p),(sz))
#define xfree(p)         free(p)
#else
#define STRCMP(a,b)	tcc_strcmp((a), (b), __FILE__, __LINE__, #a " , " #b)
#define STRNCPY(a,b,c)	tcc_strncpy((a), (b), (c),  __FILE__, __LINE__, #a " , " #b " , " #c )
#endif

extern int Debug_lvl;

/* Compiler diagnostics - all output goes to stderr */
void fatal_lex(const char *file, int line, int col, const char *fmt, ...) __attribute__((noreturn));
void tcc_warn(const char *fmt, ...);   /* warning, no exit */
void tcc_error(const char *fmt, ...) __attribute__((noreturn));  /* error + exit, no location context */

/* Internal compiler error - use the ICE() macro for file/line capture */
void fatal_ice(const char *file, int line, const char *fmt, ...) __attribute__((noreturn));
#define ICE(fmt, ...) fatal_ice(__FILE__, __LINE__, fmt, ##__VA_ARGS__)

#define TCC_VERSION "0.83"
#define TCC_VERSION_NUMBER 83
#define TCC_VERSION_NUMBER_STR "83"


#endif
