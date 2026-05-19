#ifndef TCC_STUB_SYS_RESOURCE_H
#define TCC_STUB_SYS_RESOURCE_H
#include <__tcc_stub_common.h>

#define RLIMIT_STACK 3
#define RLIM_INFINITY ((unsigned long)-1)

typedef unsigned long rlim_t;

struct rlimit {
    rlim_t rlim_cur;
    rlim_t rlim_max;
};

int getrlimit(int resource, struct rlimit *rlp);
int setrlimit(int resource, const struct rlimit *rlp);

#endif
