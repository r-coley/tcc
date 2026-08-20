#ifndef TCC_STUB_ERRNO_H
#define TCC_STUB_ERRNO_H
#if defined(__APPLE__)
int *__error(void);
#define errno (*__error())
#else
extern int errno;
#endif
#ifndef EINTR
#define EINTR 4
#endif
#endif
