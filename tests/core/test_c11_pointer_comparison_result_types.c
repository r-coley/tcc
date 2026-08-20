#define TYPEOF(x) _Generic((x), \
    int: 1, \
    unsigned int: 2, \
    int *: 3, \
    int (*)(void): 4, \
    default: 99)

static int
f(void)
{
    return 1;
}

static int
g(void)
{
    return 2;
}

int
main(void)
{
    int a = 0;
    int b = 0;
    int *pa = &a;
    int *pb = &b;
    const int *cpa = &a;
    int (*fp)(void) = f;
    int (*gp)(void) = g;

    if (TYPEOF(pa == pb) != 1)
        return 1;
    if (TYPEOF(pa != cpa) != 1)
        return 2;
    if (TYPEOF(pa < pb) != 1)
        return 3;
    if (TYPEOF(fp == gp) != 1)
        return 4;
    if (TYPEOF(fp != 0) != 1)
        return 5;

    if ((pa == pb) != 0)
        return 6;
    if ((fp == fp) != 1)
        return 7;

    return 42;
}
