typedef struct Pair64 {
	long a;
	long b;
} Pair64;

extern Pair64 make_pair64(long a, long b);
extern long sum_pair64(Pair64 p);

int
main(void)
{
	Pair64 p = make_pair64(11, 31);
	return (int)sum_pair64(p);
}
