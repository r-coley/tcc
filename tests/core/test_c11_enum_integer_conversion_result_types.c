#define TYPEOF(x) _Generic((x), \
    int: 1, \
    unsigned int: 2, \
    long: 3, \
    unsigned long: 4, \
    default: 99)

enum Small {
    SMALL_NEG = -1,
    SMALL_POS = 2
};

int
main(void)
{
    enum Small e = SMALL_NEG;
    unsigned char uc = 250;
    unsigned short us = 65530;
    unsigned int u = 1U;
    long l = 1L;

    if (TYPEOF(+e) != 1)
        return 1;
    if (TYPEOF(e + 1) != 1)
        return 2;
    if (TYPEOF(e + u) != 2)
        return 3;
    if (TYPEOF(e + l) != 3)
        return 4;
    if (TYPEOF(e ? e : u) != 2)
        return 5;
    if ((e + 2) != 1)
        return 6;
    if (TYPEOF(e & u) != 2)
        return 7;
    if (TYPEOF(e << 1U) != 1)
        return 8;
    if (TYPEOF(e < u) != 1)
        return 9;
    if (TYPEOF(e == l) != 1)
        return 10;
    if (TYPEOF(e || u) != 1)
        return 11;
    if ((SMALL_POS & 1U) != 0U)
        return 12;
    if ((SMALL_POS << 2U) != 8)
        return 13;
    e = SMALL_POS;
    if (TYPEOF(e += u) != 99)
        return 14;
    e = SMALL_POS;
    e += u;
    if (e != 3)
        return 15;
    e = SMALL_POS;
    if (TYPEOF(e <<= u) != 99)
        return 16;
    e <<= u;
    if (e != 4)
        return 17;
    e = SMALL_POS;
    if (TYPEOF(e &= u) != 99)
        return 18;
    e &= u;
    if (e != 0)
        return 19;
    e = SMALL_NEG;
    if (TYPEOF(1 ? e : uc) != 1)
        return 20;
    if (TYPEOF(1 ? us : e) != 1)
        return 21;
    if ((1 ? e : uc) != -1)
        return 22;
    if ((0 ? e : uc) != 250)
        return 23;
    if ((0 ? e : us) != 65530)
        return 24;

    return 42;
}
