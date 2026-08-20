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
	if (TYPEOF(42) != 1)
		return 1;
	if (TYPEOF(42U) != 2)
		return 2;
	if (TYPEOF(42L) != 3)
		return 3;
	if (TYPEOF(42UL) != 4)
		return 4;
	if (TYPEOF(42LU) != 4)
		return 5;
	if (TYPEOF(42LL) != 5)
		return 6;
	if (TYPEOF(42ULL) != 6)
		return 7;
	if (TYPEOF(42LLU) != 6)
		return 8;

	if (TYPEOF(2147483647) != 1)
		return 9;
	if (TYPEOF(2147483648) != 3)
		return 10;
	if (TYPEOF(9223372036854775807) != 3)
		return 11;

	if (TYPEOF(0x7fffffff) != 1)
		return 12;
	if (TYPEOF(0x80000000) != 2)
		return 13;
	if (TYPEOF(0xffffffff) != 2)
		return 14;
	if (TYPEOF(0x100000000) != 3)
		return 15;
	if (TYPEOF(0x7fffffffffffffff) != 3)
		return 16;
	if (TYPEOF(0x8000000000000000) != 4)
		return 17;
	if (TYPEOF(0xffffffffffffffff) != 4)
		return 18;

	if (TYPEOF(01777777777777777777777) != 4)
		return 19;
	if (TYPEOF(18446744073709551615ULL) != 6)
		return 20;

	return 42;
}
