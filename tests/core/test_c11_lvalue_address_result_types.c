#define TYPEOF(x) _Generic((x), \
    int: 1, \
    int *: 3, \
    const int *: 4, \
    int (*)(void): 6, \
    default: 99)

static int
f(void)
{
    return 42;
}

int
main(void)
{
    int a[3] = {1, 2, 3};
    int x = 7;
    const int cx = 8;
    int *p = &x;
    const int *cp = &cx;

    if (TYPEOF(*p) != 1)
        return 1;
    if (TYPEOF(*cp) != 1)
        return 2;
    if (TYPEOF(&x) != 3)
        return 3;
    if (TYPEOF(&cx) != 4)
        return 4;
    if (TYPEOF(a[1]) != 1)
        return 5;
    if (TYPEOF(&f) != 6)
        return 6;

    *p = 35;
    if (x != 35)
        return 7;

    return f();
}
