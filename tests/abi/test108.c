/* ABI: two-word 64-bit struct argument after integer register exhaustion. */
typedef struct Pair64 {
    long a;
    long b;
} Pair64;

static long use_stack_pair64(long a0, long a1, long a2, long a3,
                             long a4, long a5, long a6, long a7,
                             Pair64 p)
{
    return a0 + a1 + a2 + a3 + a4 + a5 + a6 + a7 + p.a + p.b;
}

int main(void)
{
    Pair64 p;
    p.a = 100;
    p.b = 200;

    return use_stack_pair64(1, 2, 3, 4, 5, 6, 7, 8, p) == 336 ? 42 : 1;
}
