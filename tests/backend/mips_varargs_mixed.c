#include <stdarg.h>

long long mix_args(int tag, ...) {
    va_list ap;
    int a;
    long long b;
    int c;

    va_start(ap, tag);

    a = va_arg(ap, int);
    b = va_arg(ap, long long);
    c = va_arg(ap, int);

    va_end(ap);

    return (long long)tag + a + b + c;
}

int main(void) {
    long long r;

    r = mix_args(1, 5, 30LL, 6);

    return (int)r;   /* 42 */
}
