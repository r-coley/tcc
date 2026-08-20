enum Small {
    NEG = -1,
    ZERO = 0,
    BIG = 300
};

static unsigned char
take_uchar(unsigned char value)
{
    return value;
}

static enum Small
take_enum(enum Small value)
{
    return value;
}

static unsigned char
ret_uchar_from_enum(enum Small value)
{
    return value;
}

static enum Small
ret_enum_from_int(int value)
{
    return value;
}

int
main(void)
{
    enum Small e = BIG;
    unsigned char uc = BIG;
    int si = e;

    if (uc != 44)
        return 1;
    if (si != 300)
        return 2;
    if (take_uchar(e) != 44)
        return 3;
    if (take_enum(300) != BIG)
        return 4;
    if (ret_uchar_from_enum(BIG) != 44)
        return 5;
    if (ret_enum_from_int(300) != BIG)
        return 6;
    e = NEG;
    if (e != -1)
        return 7;

    return 42;
}
