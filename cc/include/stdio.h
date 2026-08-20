#ifndef TCC_STUB_STDIO_H
#define TCC_STUB_STDIO_H
#include <__tcc_stub_common.h>

/* FILE is opaque to this compiler; all uses are FILE*.
 * Keep this as a simple scalar typedef so the self-hosted compiler
 * does not depend on struct typedef support while parsing libc stubs. */
typedef void FILE;

/* stdin/stdout/stderr are global variables exported by libc.
 * On macOS the actual symbols are __stderrp/__stdoutp/__stdinp;
 * on Linux/glibc they are stderr/stdout/stdin directly.
 * We declare the underlying symbols and macro-alias them so that
 * references in source code resolve to the correct linker symbol. */
#if defined(__APPLE__)
extern FILE *__stdinp;
extern FILE *__stdoutp;
extern FILE *__stderrp;
#define stdin  __stdinp
#define stdout __stdoutp
#define stderr __stderrp
#else
extern FILE *stdin;
extern FILE *stdout;
extern FILE *stderr;
#endif

/* Variadic function declarations — needed for correct arm64 calling convention */
int 	printf(const char *fmt, ...);
int 	fprintf(FILE *f, const char *fmt, ...);
int 	sprintf(char *buf, const char *fmt, ...);
int 	snprintf(char *buf, unsigned long n, const char *fmt, ...);

int 	putchar(int c);

/* va_list variants — needed for Debug() and similar wrappers */
int 	vfprintf(FILE *f, const char *fmt, char *ap);
int 	vsnprintf(char *buf, unsigned long n, const char *fmt, char *ap);

void 	perror(const char *s);
int 	rename(const char *old, const char *new);

FILE 	*fopen(const char *restrict path, const char *restrict mode);
int 	fclose(FILE *stream);
long 	ftell(FILE *stream);
int 	fseek(FILE *stream, long offset, int whence);
void 	rewind(FILE *stream);
size_t 	fread(void *restrict ptr, size_t size, size_t nitems, FILE *restrict stream);
size_t 	fwrite(const void *restrict ptr, size_t size, size_t nitems, FILE *restrict stream);
int 	fflush(FILE *stream);
int 	remove(const char *path);

char 	*fgets(char * restrict str, int size, FILE * restrict stream);
int 	fputs(const char *restrict s, FILE *restrict stream);
int 	puts(const char *s);
int 	fgetc(FILE *stream);
int 	getc(FILE *stream);
int 	fputc(int c, FILE *stream);
int 	putc(int c, FILE *stream);

FILE	*popen(const char *command, const char *mode);
int	pclose(FILE *stream);
int	fileno(FILE *stream);

#define EOF (-1)
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

#endif
