#include <stdarg.h>

long mix(int n, ...) {
    va_list ap;
    int a;
    long b;
    int c;

    va_start(ap, n);
    a = va_arg(ap, int);
    b = va_arg(ap, long);
    c = va_arg(ap, int);
    va_end(ap);

    return a + b + c;
}

int main(void) {
    return (int)mix(3, 10, 20L, 12);
}
