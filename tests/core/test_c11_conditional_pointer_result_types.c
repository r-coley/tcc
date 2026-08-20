#define TYPEOF(x) _Generic((x), \
    int *: 1, \
    const int *: 2, \
    volatile int *: 3, \
    const volatile int *: 4, \
    void *: 5, \
    const void *: 6, \
    volatile void *: 7, \
    const volatile void *: 8, \
    default: 99)

int
main(void)
{
    int i = 0;
    int *ip = &i;
    const int *cip = &i;
    volatile int *vip = &i;
    const volatile int *cvip = &i;
    void *vp = &i;
    const void *cvp = &i;
    volatile void *vvp = &i;

    if (TYPEOF(1 ? ip : cip) != 2)
        return 1;
    if (TYPEOF(1 ? cip : ip) != 2)
        return 14;
    if (TYPEOF(1 ? cip : vip) != 4)
        return 2;
    if (TYPEOF(1 ? vip : cip) != 4)
        return 15;
    if (TYPEOF(1 ? vip : cvip) != 4)
        return 3;
    if (TYPEOF(1 ? cvip : vip) != 4)
        return 16;
    if (TYPEOF(1 ? vp : ip) != 5)
        return 20;
    if (TYPEOF(1 ? ip : vp) != 5)
        return 21;
    if (TYPEOF(1 ? vp : cip) != 6)
        return 4;
    if (TYPEOF(1 ? cip : vp) != 6)
        return 17;
    if (TYPEOF(1 ? vip : vp) != 7)
        return 5;
    if (TYPEOF(1 ? vp : vip) != 7)
        return 18;
    if (TYPEOF(1 ? cvip : cvp) != 8)
        return 6;
    if (TYPEOF(1 ? cvp : cvip) != 8)
        return 19;
    if (TYPEOF(1 ? cvp : vvp) != 8)
        return 7;
    if (TYPEOF(1 ? vvp : cvp) != 8)
        return 8;
    if (TYPEOF(1 ? cip : 0) != 2)
        return 9;
    if (TYPEOF(1 ? 0 : vip) != 3)
        return 10;
    if (TYPEOF(1 ? cvip : 0) != 4)
        return 11;
    if (TYPEOF(1 ? 0 : cvp) != 6)
        return 12;
    if (TYPEOF(1 ? 0 : (const volatile void *)0) != 8)
        return 13;

    return 42;
}
