#ifndef TCC_STUB_STDDEF_H
#define TCC_STUB_STDDEF_H
#include <__tcc_stub_common.h>

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 202311L
typedef void *nullptr_t;
#endif

#define offsetof(type, member) ((size_t)&((type *)0)->member)

#endif
