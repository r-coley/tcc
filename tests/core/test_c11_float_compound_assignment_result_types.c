#define TYPEOF(x) _Generic((x), \
    float: 1, \
    double: 2, \
    int: 3, \
    default: 99)

int
main(void)
{
    float f = 1.5f;
    double d = 2.0;

    if (TYPEOF(f += d) != 1)
        return 1;
    f = 1.5f;
    f += d;
    if (f != 3.5f)
        return 2;
    if (TYPEOF(d *= f) != 2)
        return 3;
    d = 2.0;
    d *= f;
    if (d != 7.0)
        return 4;
    if (TYPEOF(f /= 2) != 1)
        return 5;
    f = 7.0f;
    f /= 2;
    if (f != 3.5f)
        return 6;

    return 42;
}
