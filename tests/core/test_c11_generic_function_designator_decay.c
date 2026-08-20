#define TYPEOF(x) _Generic((x), \
	int (*)(void): 10, \
	int (*)(int): 20, \
	default: 99)

static int
f(void)
{
	return 1;
}

static int
g(int x)
{
	return x + 1;
}

int
main(void)
{
	int (*pf)(void) = f;
	int total = 0;

	total += TYPEOF(f);
	total += TYPEOF((f));
	total += TYPEOF(&f);
	total += TYPEOF(g);
	total += TYPEOF((g));
	total += TYPEOF(&g);
	total += TYPEOF(*pf);

	return total == (10 + 10 + 10 + 20 + 20 + 20 + 10) ? 42 : total;
}
