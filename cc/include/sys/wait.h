#ifndef TCC_STUB_SYS_WAIT_H
#define TCC_STUB_SYS_WAIT_H
#include <__tcc_stub_common.h>

/* POSIX status interpretation macros — portable across Linux and macOS */
#define WIFEXITED(s)    (((s) & 0x7f) == 0)
#define WEXITSTATUS(s)  (((s) >> 8) & 0xff)
#define WIFSIGNALED(s)  (((s) & 0x7f) != 0 && ((s) & 0x7f) != 0x7f)
#define WTERMSIG(s)     ((s) & 0x7f)
#define WIFSTOPPED(s)   (((s) & 0xff) == 0x7f)
#define WSTOPSIG(s)     (((s) >> 8) & 0xff)

#define WNOHANG   1
#define WUNTRACED 2

pid_t waitpid(pid_t pid, int *status, int options);

#endif
