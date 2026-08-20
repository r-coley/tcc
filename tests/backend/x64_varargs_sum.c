#include <stdarg.h>

int sum(int n, ...) {
    va_list ap;
    int i;
    int r;

    va_start(ap, n);
    r = 0;
    i = 0;
    while (i < n) {
        r = r + va_arg(ap, int);
        i = i + 1;
    }
    va_end(ap);

    return r;
}

int main(void) {
    return sum(4, 10, 20, 30, 40);
}
