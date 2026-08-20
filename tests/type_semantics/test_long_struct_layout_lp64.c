struct S {
    char a;
    long b;
    char c;
};

int main(void)
{
    struct S s;

    if (sizeof(struct S) != 24)
        return 1;
    s.a = 1;
    s.b = 0x100000003L;
    s.c = 4;
    if ((s.b >> 32) != 1)
        return 2;
    if ((s.b & 15L) != 3)
        return 3;
    if (s.a != 1 || s.c != 4)
        return 4;
    return 42;
}
