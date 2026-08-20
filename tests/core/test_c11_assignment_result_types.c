#define TYPEOF(x) _Generic((x), \
    int: 1, \
    unsigned char: 2, \
    signed char: 3, \
    unsigned short: 4, \
    unsigned int: 5, \
    long long: 6, \
    unsigned long long: 7, \
    default: 99)

int
main(void)
{
    unsigned char uc = 0;
    signed char sc = 0;
    unsigned short us = 0;
    int si = 0;
    unsigned int ui = 4000000000U;
    long long sll = 0;
    unsigned long long ull = 0;

    if (TYPEOF(uc = 300) != 2)
        return 1;
    uc = 300;
    if (uc != 44)
        return 2;

    if (TYPEOF(sc = 100U) != 3)
        return 3;
    sc = 100U;
    if (sc != 100)
        return 4;

    if (TYPEOF(us = 65537UL) != 4)
        return 5;
    us = 65537UL;
    if (us != 1)
        return 6;

    if (TYPEOF(si = ui) != 1)
        return 7;
    si = 42U;
    if (si != 42)
        return 8;

    if (TYPEOF(sll = ui) != 6)
        return 9;
    sll = 4000000000U;
    if (sll != 4000000000LL)
        return 10;

    if (TYPEOF(ull = -1LL) != 7)
        return 11;
    ull = -1LL;
    if (ull != 18446744073709551615ULL)
        return 12;

    return 42;
}
