#ifndef TCC_STUB_UNISTD_H
#define TCC_STUB_UNISTD_H
#include <__tcc_stub_common.h>

#define STDIN_FILENO  0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2

pid_t	fork(void);
pid_t	getpid(void);
int 	execvp(const char *file, char *const argv[]);
int 	unlink(const char *path);
int 	access(const char *path, int mode);
int 	dup(int fd);
int 	dup2(int oldfd, int newfd);
int 	close(int fd);
int 	fileno(void *stream);
void 	_exit(int status);
int 	mkstemp(char *template);
char 	*getcwd(char *buf, size_t size);
ssize_t write(int fd, const void *buf, size_t count);
ssize_t read(int fd, const void *buf, size_t count);
ssize_t readlink(const char *path, char *buf, size_t bufsiz);

#endif
