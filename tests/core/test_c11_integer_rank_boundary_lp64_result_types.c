#define TYPEOF(x) _Generic((x), \
	int: 1, \
	unsigned int: 2, \
	long: 3, \
	unsigned long: 4, \
	long long: 5, \
	unsigned long long: 6, \
	default: 99)

int
main(void)
{
	long sl = -2L;
	unsigned long ul = 5UL;
	long long sll = -3LL;
	unsigned long long ull = 7ULL;

	if (sizeof(long) != 8 || sizeof(long long) != 8)
		return 100;

	if (TYPEOF(sl + ull) != 6)
		return 1;
	if (TYPEOF(ull + sl) != 6)
		return 2;
	if (TYPEOF(sll + ul) != 6)
		return 3;
	if (TYPEOF(1 ? sl : ull) != 6)
		return 4;

	if ((sl + ull) != 5ULL)
		return 5;
	if ((ull + sl) != 5ULL)
		return 6;
	if ((sll + ul) != 2ULL)
		return 7;
	if ((1 ? sl : ull) != (unsigned long long)-2L)
		return 8;

	if ((sl < ull) != 0)
		return 9;
	if ((ull < sl) != 1)
		return 10;
	if ((sll < ul) != 0)
		return 11;

	return 42;
}
