#include <stdarg.h>

long sum_long(int count, ...)
{
    va_list ap;
    va_start(ap, count);
    long total = 0;
    int i;
    for (i = 0; i < count; i++)
        total += va_arg(ap, long);
    va_end(ap);
    return total;
}

int main(void)
{
    /* 0x100000000 + 0x100000001 + 1 = 0x200000002 */
    long r = sum_long(3, 0x100000000L, 0x100000001L, 1L);
    if (r != 0x200000002L) return 1;
    return 42;
}
