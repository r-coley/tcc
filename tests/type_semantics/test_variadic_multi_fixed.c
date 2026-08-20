#include <stdarg.h>

int sum_after_two_fixed(int base, int count, ...)
{
    va_list ap;
    va_start(ap, count);
    int total = base;
    int i;
    for (i = 0; i < count; i++)
        total += va_arg(ap, int);
    va_end(ap);
    return total;
}

long sum_long_after_two_fixed(long base, int count, ...)
{
    va_list ap;
    va_start(ap, count);
    long total = base;
    int i;
    for (i = 0; i < count; i++)
        total += va_arg(ap, long);
    va_end(ap);
    return total;
}

int main(void)
{
    if (sum_after_two_fixed(2, 3, 10, 20, 10) != 42) return 1;
    if (sum_long_after_two_fixed(0x100000000L, 2, 0x100000001L, 1L) != 0x200000002L) return 2;
    return 42;
}
