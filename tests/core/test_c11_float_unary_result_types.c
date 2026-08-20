#define TYPEOF(x) _Generic((x), \
    int: 1, \
    float: 2, \
    double: 3, \
    default: 99)

int
main(void)
{
    float f = 1.25f;
    double d = -2.5;

    if (TYPEOF(+f) != 2)
        return 1;
    if (TYPEOF(-f) != 2)
        return 2;
    if (TYPEOF(+d) != 3)
        return 3;
    if (TYPEOF(-d) != 3)
        return 4;
    if (TYPEOF(!f) != 1)
        return 5;
    if (+f != 1.25f)
        return 6;
    if (-f != -1.25f)
        return 7;
    if (+d != -2.5)
        return 8;
    if (-d != 2.5)
        return 9;
    if (!0.0f != 1)
        return 10;
    if (!f != 0)
        return 11;

    return 42;
}
