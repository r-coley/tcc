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
	if (TYPEOF(020000000000) != 2)
		return 1;
	if (TYPEOF(037777777777) != 2)
		return 2;
	if (TYPEOF(040000000000) != 3)
		return 3;
	if (TYPEOF(0777777777777777777777) != 3)
		return 4;
	if (TYPEOF(01000000000000000000000) != 4)
		return 5;
	if (TYPEOF(01777777777777777777777) != 4)
		return 6;

	if (TYPEOF(42u) != 2)
		return 7;
	if (TYPEOF(42l) != 3)
		return 8;
	if (TYPEOF(42ul) != 4)
		return 9;
	if (TYPEOF(42lu) != 4)
		return 10;
	if (TYPEOF(42ll) != 5)
		return 11;
	if (TYPEOF(42ull) != 6)
		return 12;
	if (TYPEOF(42llu) != 6)
		return 13;

	if (020000000000 != 2147483648U)
		return 14;
	if (037777777777 != 4294967295U)
		return 15;
	if (040000000000 != 4294967296UL)
		return 16;
	if (01777777777777777777777 != 18446744073709551615UL)
		return 17;

	return 42;
}
