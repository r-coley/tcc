struct Big {
    int a;
    int b;
    int c;
    int d;
    int e;
};

static int use_big(struct Big x, int k)
{
    return x.a + x.b * 2 + x.c * 3 + x.d * 4 + x.e * 5 + k * 6;
}

static int wrapper(int p)
{
    struct Big b;
    b.a = p + 1;
    b.b = p + 2;
    b.c = p + 3;
    b.d = p + 4;
    b.e = p + 5;

    return use_big(b, p + 6);
}

int main(void)
{
    return wrapper(10) - 301;
}
