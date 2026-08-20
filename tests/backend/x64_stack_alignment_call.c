static long long callee(long long a, long long b, long long c,
                        long long d, long long e, long long f,
                        long long g, long long h)
{
    return a + b * 2 + c * 3 + d * 4 + e * 5 + f * 6 + g * 7 + h * 8;
}

static long long caller(long long a, long long b, long long c,
                        long long d, long long e, long long f,
                        long long g, long long h,
                        long long i, long long j)
{
    long long local0 = i + j;
    long long local1 = a - b;
    long long local2 = c + d + e;

    return callee(
        local0,
        local1,
        local2,
        f,
        g,
        h,
        i,
        j
    );
}

int main(void)
{
    long long r = caller(10, 3, 4, 5, 6, 7, 8, 9, 11, 12);
    return (int)(r - 377);
}
