#define TYPEOF(x) _Generic((x), \
    int (*)(void): 1, \
    int (*)(int): 2, \
    void *: 3, \
    default: 99)

static int
f(void)
{
    return 10;
}

static int
g(void)
{
    return 20;
}

static int
h(int x)
{
    return x + 1;
}

int
main(void)
{
    int (*fp)(void) = f;
    int (*gp)(void) = g;
    int (*hp)(int) = h;

    if (TYPEOF(1 ? fp : gp) != 1)
        return 1;
    if ((1 ? fp : gp)() != 10)
        return 2;

    if (TYPEOF(0 ? fp : gp) != 1)
        return 3;
    if ((0 ? fp : gp)() != 20)
        return 4;

    if (TYPEOF(1 ? fp : 0) != 1)
        return 5;
    if ((1 ? fp : 0)() != 10)
        return 6;

    if (TYPEOF(1 ? 0 : hp) != 2)
        return 7;
    if ((1 ? hp : 0)(41) != 42)
        return 8;

    return 42;
}
