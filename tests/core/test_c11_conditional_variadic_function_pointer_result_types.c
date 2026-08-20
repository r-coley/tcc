#define TYPEOF(x) _Generic((x), \
	int (*)(int, ...): 1, \
	default: 99)

static int
inc(int x, ...)
{
	return x + 1;
}

static int
dec(int x, ...)
{
	return x - 1;
}

int
main(void)
{
	int (*a)(int, ...) = inc;
	int (*b)(int, ...) = dec;

	if (TYPEOF(1 ? a : b) != 1)
		return 1;
	if ((1 ? a : b)(41, 99) != 42)
		return 2;

	if (TYPEOF(0 ? a : b) != 1)
		return 3;
	if ((0 ? a : b)(43, 99) != 42)
		return 4;

	if (TYPEOF(1 ? a : 0) != 1)
		return 5;
	if ((1 ? a : 0)(41, 7) != 42)
		return 6;

	if (TYPEOF(0 ? 0 : b) != 1)
		return 7;
	if ((0 ? 0 : b)(43, 7) != 42)
		return 8;

	return 42;
}
