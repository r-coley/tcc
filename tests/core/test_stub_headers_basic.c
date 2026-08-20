#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

int main(void)
{
    pid_t pid = 0;
    ssize_t n = 0;
    size_t z = sizeof(uintptr_t);
    int fd = STDOUT_FILENO;
    int status = 0;

    if (STDIN_FILENO != 0 || STDOUT_FILENO != 1 || STDERR_FILENO != 2)
        return 1;
    if (sizeof(pid) == 0 || sizeof(n) == 0 || z == 0)
        return 2;
    if (fd != 1)
        return 3;
    if (WIFEXITED(status) && WEXITSTATUS(status) == 0)
        return 42;
    return 4;
}
