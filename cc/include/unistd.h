#ifndef TCC_STUB_UNISTD_H
#define TCC_STUB_UNISTD_H
#include <__tcc_stub_common.h>

int dup(int fd);
int dup2(int oldfd, int newfd);
int close(int fd);
int fileno(void *stream);

#endif
