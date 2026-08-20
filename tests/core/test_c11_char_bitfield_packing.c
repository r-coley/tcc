struct Bits {
    signed char s3 : 3;
    unsigned char u3 : 3;
    unsigned char tail : 2;
};

int main(void)
{
    struct Bits b;

    b.s3 = 3;
    if (b.s3 != 3)
        return 1;
    b.s3 = 4;
    if (b.s3 != -4)
        return 2;

    b.u3 = 7;
    if (b.u3 != 7)
        return 3;
    b.tail = 3;
    if (b.tail != 3)
        return 4;
    if (b.u3 != 7)
        return 5;
    if (b.s3 != -4)
        return 6;

    return 42;
}
