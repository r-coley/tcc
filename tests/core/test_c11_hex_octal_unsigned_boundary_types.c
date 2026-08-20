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
	if (TYPEOF(0xffffffffU) != 2)
		return 1;
	if (TYPEOF(0x100000000U) != 4)
		return 2;
	if (TYPEOF(0xffffffffffffffffU) != 4)
		return 3;

	if (TYPEOF(037777777777U) != 2)
		return 4;
	if (TYPEOF(040000000000U) != 4)
		return 5;
	if (TYPEOF(01777777777777777777777U) != 4)
		return 6;

	if (TYPEOF(0x100000000ULL) != 6)
		return 7;
	if (TYPEOF(040000000000ULL) != 6)
		return 8;

	if (0x100000000U != 4294967296UL)
		return 9;
	if (040000000000U != 4294967296UL)
		return 10;

	return 42;
}
