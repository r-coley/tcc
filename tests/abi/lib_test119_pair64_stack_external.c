typedef struct Pair64 {
	long a;
	long b;
} Pair64;

long
use_stack_pair64(long a0, long a1, long a2, long a3,
                 long a4, long a5, long a6, long a7,
                 Pair64 p)
{
	return a0 + a1 + a2 + a3 + a4 + a5 + a6 + a7 + p.a + p.b;
}
