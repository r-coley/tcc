#ifndef TCC_STUB_STDLIB_H
#define TCC_STUB_STDLIB_H
#include <__tcc_stub_common.h>

#define EXIT_SUCCESS 0
#define EXIT_FAILURE 1

void *malloc(size_t size);
void *calloc(size_t count, size_t size);
void *realloc(void *ptr, size_t size);
void  free(void *ptr);
int   atoi(const char *s);
int   atol(const char *s);
void  exit(int status);
int   system(const char *cmd);
int   getpid(void);

#endif
