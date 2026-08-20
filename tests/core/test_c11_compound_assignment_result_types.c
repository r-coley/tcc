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
    unsigned char uc = 250;
    signed char sc = 100;
    unsigned short us = 65530;
    unsigned int ui = 1U;
    long long sll = -5LL;
    unsigned long long ull = 10ULL;
    long long sll_shift = 3LL;
    unsigned long long ull_shift = 0xFULL;
    long long sll_mask = -1LL;
    unsigned long long ull_mask = 0xF0ULL;

    if (TYPEOF(uc += 10U) != 2)
        return 1;
    uc += 10U;
    if (uc != 4)
        return 2;

    if (TYPEOF(sc += 10U) != 3)
        return 3;
    sc += 10U;
    if (sc != 110)
        return 4;

    if (TYPEOF(us *= ui + 9U) != 4)
        return 5;
    us *= ui + 9U;
    if (us != 65476)
        return 6;

    if (TYPEOF(sll += 4000000000U) != 6)
        return 7;
    sll += 4000000000U;
    if (sll != 3999999995LL)
        return 8;

    if (TYPEOF(ull -= -2LL) != 7)
        return 9;
    ull -= -2LL;
    if (ull != 12ULL)
        return 10;

    if (TYPEOF(sll_shift <<= 4U) != 6)
        return 11;
    sll_shift <<= 4U;
    if (sll_shift != 48LL)
        return 12;

    if (TYPEOF(ull_shift >>= 1U) != 7)
        return 13;
    ull_shift >>= 1U;
    if (ull_shift != 0x7ULL)
        return 14;

    if (TYPEOF(sll_mask &= 0xFFULL) != 6)
        return 15;
    sll_mask &= 0xFFULL;
    if (sll_mask != 255LL)
        return 16;

    if (TYPEOF(ull_mask ^= -1LL) != 7)
        return 17;
    ull_mask ^= -1LL;
    if (ull_mask != (0xF0ULL ^ 18446744073709551615ULL))
        return 18;

    return 42;
}
