static int got;
typedef long *LongPtr;

static void
take_long_ptr(long *p)
{
	got = (int)*p;
}

int
main(void)
{
	long good = 42;
	long bad = 7;
	LongPtr items[260];
	int i;
	int n = 2;

	for (i = 0; i < 260; i++)
		items[i] = &bad;

	items[1] = &good;
	items[257] = &bad;

	take_long_ptr(items[n - 1]);
	return got;
}
