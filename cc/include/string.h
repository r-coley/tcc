#ifndef TCC_STUB_STRING_H
#define TCC_STUB_STRING_H
#include <__tcc_stub_common.h>

int    strcmp(const char *a, const char *b);
int    strncmp(const char *a, const char *b, size_t n);
size_t strlen(const char *s);
char  *strcpy(char *dst, const char *src);
char  *strncpy(char *dst, const char *src, size_t n);
char  *strcat(char *dst, const char *src);
char  *strchr(const char *s, int c);
char  *strrchr(const char *s, int c);
char  *strstr(const char *haystack, const char *needle);
void  *memcpy(void *dst, const void *src, size_t n);
void  *memmove(void *dst, const void *src, size_t n);
void  *memset(void *s, int c, size_t n);
int    memcmp(const void *a, const void *b, size_t n);
char  *strerror(int errnum);

#endif /* TCC_STUB_STRING_H */
