#include <stdarg.h>

/* Return the first vararg directly for debugging */
int first_vararg(int dummy, ...)
{
    va_list ap;
    va_start(ap, dummy);
    int r = va_arg(ap, int);
    va_end(ap);
    return r;
}

int sum(int count, ...)
{
    va_list ap;
    va_start(ap, count);
    int total = 0;
    int i;
    for (i = 0; i < count; i++)
        total += va_arg(ap, int);
    va_end(ap);
    return total;
}

int main(void)
{
    int v = first_vararg(0, 42);
    if (v != 42) return v;  /* return actual value to debug */
    if (sum(3, 10, 20, 12) != 42) return 2;
    if (sum(1, 42) != 42) return 3;
    if (sum(0) != 0) return 4;
    return 42;
}
