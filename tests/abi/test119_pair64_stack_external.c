typedef struct Pair64 {
	long a;
	long b;
} Pair64;

extern long use_stack_pair64(long a0, long a1, long a2, long a3,
                             long a4, long a5, long a6, long a7,
                             Pair64 p);

int
main(void)
{
	Pair64 p;
	p.a = 100;
	p.b = 200;
	return use_stack_pair64(1, 2, 3, 4, 5, 6, 7, 8, p) == 336 ? 42 : 1;
}
