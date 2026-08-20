#define TYPEOF(x) _Generic((x), \
    int: 1, \
    unsigned char: 2, \
    signed char: 3, \
    unsigned short: 4, \
    unsigned int: 5, \
    long: 6, \
    unsigned long: 7, \
    long long: 8, \
    unsigned long long: 9, \
    default: 99)

int
main(void)
{
    unsigned int ui = 300U;
    unsigned long ul = 65537UL;
    long long ll = 42LL;

    if (TYPEOF((unsigned char)ui) != 2)
        return 1;
    if ((unsigned char)ui != 44)
        return 2;

    if (TYPEOF((signed char)100U) != 3)
        return 3;
    if ((signed char)100U != 100)
        return 4;

    if (TYPEOF((unsigned short)ul) != 4)
        return 5;
    if ((unsigned short)ul != 1)
        return 6;

    if (TYPEOF((int)ll) != 1)
        return 7;
    if ((int)ll != 42)
        return 8;

    if (TYPEOF((unsigned long long)42) != 9)
        return 9;
    if ((unsigned long long)42 != 42ULL)
        return 10;

    return 42;
}
