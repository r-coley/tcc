struct odd5 {
    char bytes[5];
};

static struct odd5
make_odd5(void)
{
    struct odd5 value;

    value.bytes[0] = 9;
    value.bytes[1] = 8;
    value.bytes[2] = 7;
    value.bytes[3] = 6;
    value.bytes[4] = 12;
    return value;
}

int
main(void)
{
    struct odd5 a = make_odd5();
    struct odd5 b;

    b = a;
    return b.bytes[0] + b.bytes[1] + b.bytes[2] + b.bytes[3] + b.bytes[4];
}
