#include <stdarg.h>

unsigned sum_unsigned(int count, ...)
{
    va_list ap;
    va_start(ap, count);
    unsigned total = 0;
    int i;
    for (i = 0; i < count; i++)
        total += va_arg(ap, unsigned);
    va_end(ap);
    return total;
}

int main(void)
{
    unsigned r = sum_unsigned(3, 10U, 20U, 12U);
    if (r != 42U) return 1;
    return 42;
}
