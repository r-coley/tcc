/* ABI: 64-bit two-word struct return and pass-by-value. */
typedef struct Pair64 {
    long a;
    long b;
} Pair64;

static Pair64 make_pair64(long a, long b)
{
    Pair64 p;
    p.a = a;
    p.b = b;
    return p;
}

static long sum_pair64(Pair64 p)
{
    return p.a + p.b;
}

int main(void)
{
    Pair64 p = make_pair64(11, 31);
    return (int)sum_pair64(p);
}
