#define TYPEOF(x) _Generic((x), \
    int: 1, \
    float: 2, \
    double: 3, \
    default: 99)

int
main(void)
{
    float f = 1.25f;
    double d = 2.5;

    if (TYPEOF(f < d) != 1)
        return 1;
    if (TYPEOF(d == f) != 1)
        return 2;
    if (TYPEOF(f && d) != 1)
        return 3;
    if (!(f < d))
        return 4;
    if (d == f)
        return 5;
    if ((0.0f || d) != 1)
        return 6;
    if ((0.0f && d) != 0)
        return 7;
    if ((f && 0.0) != 0)
        return 8;

    return 42;
}
