#include <stdarg.h>

int sum3(int count, ...) {
    va_list ap;
    int total;
    int i;

    total = 0;
    va_start(ap, count);
    for (i = 0; i < count; i = i + 1)
        total = total + va_arg(ap, int);
    va_end(ap);

    return total;
}

int main(void) {
    return sum3(3, 10, 20, 30);
}
