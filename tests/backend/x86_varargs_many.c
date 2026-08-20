#include <stdarg.h>

static int sum_mix(int count, ...)
{
    va_list ap;
    va_start(ap, count);

    int total = 0;

    for (int i = 0; i < count; i++) {
        int tag = va_arg(ap, int);

        if (tag == 1) {
            total += va_arg(ap, int);
        } else if (tag == 2) {
            total += (int)va_arg(ap, long long);
        } else if (tag == 3) {
            total += va_arg(ap, int); /* char/short promoted to int */
        }
    }

    va_end(ap);
    return total;
}

int main(void)
{
    char c = 5;
    short s = 7;

    int r = sum_mix(
        8,
        1, 10,
        2, 1000LL,
        3, c,
        3, s,
        1, -20,
        2, 40LL,
        1, 3,
        1, 4
    );

    return r - 1049;
}
