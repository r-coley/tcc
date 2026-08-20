struct Big {
    long long a;
    long long b;
    long long c;
};

static long long use_big(struct Big x, long long k)
{
    return x.a + x.b * 2 + x.c * 3 + k * 4;
}

static long long wrapper(long long p)
{
    struct Big b;
    b.a = p + 1;
    b.b = p + 2;
    b.c = p + 3;

    return use_big(b, p + 4);
}

int main(void)
{
    long long r = wrapper(10);
    return (int)(r - 130);
}
