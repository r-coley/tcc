#include <stdarg.h>

int pick4(int marker, ...) {
    va_list ap;
    int a;
    int b;
    int c;
    int d;

    va_start(ap, marker);
    a = va_arg(ap, int);
    b = va_arg(ap, int);
    c = va_arg(ap, int);
    d = va_arg(ap, int);
    va_end(ap);

    return marker + a + b + c + d;
}

int main(void) {
    return pick4(1, 2, 3, 4, 5);
}
