#define TYPEOF(x) _Generic((x), \
    float: 1, \
    double: 2, \
    enum Small: 3, \
    int: 4, \
    default: 99)

enum Small {
    NEG = -1,
    TWO = 2,
    BIG = 300
};

static float
take_float(float value)
{
    return value;
}

static double
take_double(double value)
{
    return value;
}

static float
ret_float_from_enum(enum Small value)
{
    return value;
}

static enum Small
ret_enum_from_float(float value)
{
    return value;
}

int
main(void)
{
    enum Small e = TWO;
    float f = e;
    double d = e;

    if (f != 2.0f)
        return 1;
    if (d != 2.0)
        return 2;
    if (TYPEOF(e + 0.5f) != 1)
        return 3;
    if (TYPEOF(e + 0.5) != 2)
        return 4;
    if (TYPEOF(1 ? e : 0.5f) != 1)
        return 5;
    if (TYPEOF(1 ? e : 0.5) != 2)
        return 6;
    if (take_float(e) != 2.0f)
        return 7;
    if (take_double(e) != 2.0)
        return 8;
    if (ret_float_from_enum(BIG) != 300.0f)
        return 9;
    if (ret_enum_from_float(2.75f) != TWO)
        return 10;
    if (TYPEOF((float)e) != 1)
        return 11;
    if ((float)e != 2.0f)
        return 12;
    if (TYPEOF((enum Small)2.75f) != 3)
        return 13;
    if ((enum Small)2.75f != TWO)
        return 14;

    return 42;
}
