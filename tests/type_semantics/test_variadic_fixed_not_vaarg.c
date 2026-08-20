#include <stdarg.h>

int first_vararg_after_one_fixed(int fixed, ...)
{
    va_list ap;
    va_start(ap, fixed);
    int r = va_arg(ap, int);
    va_end(ap);
    return r;
}

int first_vararg_after_two_fixed(int a, int b, ...)
{
    va_list ap;
    va_start(ap, b);
    int r = va_arg(ap, int);
    va_end(ap);
    return r;
}

long first_long_vararg_after_fixed(long fixed, ...)
{
    va_list ap;
    va_start(ap, fixed);
    long r = va_arg(ap, long);
    va_end(ap);
    return r;
}

int main(void)
{
    if (first_vararg_after_one_fixed(99, 42) != 42) return 1;
    if (first_vararg_after_two_fixed(77, 88, 42) != 42) return 2;
    if (first_long_vararg_after_fixed(0x100000000L, 42L) != 42L) return 3;
    return 42;
}
