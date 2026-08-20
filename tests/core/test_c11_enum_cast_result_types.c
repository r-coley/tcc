#define TYPEOF(x) _Generic((x), \
    enum Small: 1, \
    int: 2, \
    unsigned char: 3, \
    default: 99)

enum Small {
    NEG = -1,
    ZERO = 0,
    BIG = 300
};

int
main(void)
{
    enum Small e = BIG;

    if (TYPEOF((enum Small)300) != 1)
        return 1;
    if ((enum Small)300 != BIG)
        return 2;
    if (TYPEOF((int)e) != 2)
        return 3;
    if ((int)e != 300)
        return 4;
    if (TYPEOF((unsigned char)e) != 3)
        return 5;
    if ((unsigned char)e != 44)
        return 6;
    if ((enum Small)-1 != NEG)
        return 7;

    return 42;
}
