#include <stdarg.h>

unsigned long sum_ulong(int count, ...)
{
    va_list ap;
    va_start(ap, count);
    unsigned long total = 0;
    int i;
    for (i = 0; i < count; i++)
        total += va_arg(ap, unsigned long);
    va_end(ap);
    return total;
}

int main(void)
{
    unsigned long r = sum_ulong(3, 0x100000000UL, 0x100000001UL, 1UL);
    if (r != 0x200000002UL) return 1;
    return 42;
}
