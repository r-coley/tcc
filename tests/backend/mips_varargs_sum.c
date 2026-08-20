#include <stdarg.h>

int sum_ints(int n, ...) {
    va_list ap;
    int total;
    int i;

    va_start(ap, n);

    total = 0;
    for (i = 0; i < n; i = i + 1) {
        total = total + va_arg(ap, int);
    }

    va_end(ap);

    return total;
}

int main(void) {
    return sum_ints(4, 5, 10, 20, 7);   /* 42 */
}
