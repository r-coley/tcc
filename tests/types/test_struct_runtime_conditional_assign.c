struct Pair {
	int a;
	int b;
};

int
main(void)
{
	struct Pair x = {1, 2};
	struct Pair y = {3, 4};
	struct Pair z = {0, 0};
	volatile int cond = 0;

	z = cond ? x : y;
	if (z.a != 3 || z.b != 4)
		return 1;

	cond = 1;
	z = cond ? x : y;
	if (z.a != 1 || z.b != 2)
		return 2;

	return 42;
}
