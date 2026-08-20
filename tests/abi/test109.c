/* ABI: return and pass a large struct by value.
   On 64-bit targets this exercises the aggregate return path that is too
   large for the simple two-register struct return case covered by test107. */

typedef struct Big24 {
    long a;
    long b;
    long c;
} Big24;

static Big24 make_big24(long a, long b, long c)
{
    Big24 v;
    v.a = a;
    v.b = b;
    v.c = c;
    return v;
}

static long sum_big24(Big24 v)
{
    return v.a + v.b + v.c;
}

int main(void)
{
    Big24 v = make_big24(7, 11, 24);
    return sum_big24(v) == 42 ? 42 : 1;
}
