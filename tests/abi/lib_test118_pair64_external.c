typedef struct Pair64 {
	long a;
	long b;
} Pair64;

Pair64
make_pair64(long a, long b)
{
	Pair64 p;
	p.a = a;
	p.b = b;
	return p;
}

long
sum_pair64(Pair64 p)
{
	return p.a + p.b;
}
