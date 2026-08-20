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
	if (TYPEOF(42Lu) != 4)
		return 1;
	if (TYPEOF(42lU) != 4)
		return 2;
	if (TYPEOF(42Ul) != 4)
		return 3;
	if (TYPEOF(42uL) != 4)
		return 4;

	if (TYPEOF(42LLu) != 6)
		return 5;
	if (TYPEOF(42llU) != 6)
		return 6;
	if (TYPEOF(42Ull) != 6)
		return 7;
	if (TYPEOF(42uLL) != 6)
		return 8;

	if (42Lu != 42UL)
		return 9;
	if (42lU != 42UL)
		return 10;
	if (42LLu != 42ULL)
		return 11;
	if (42uLL != 42ULL)
		return 12;

	return 42;
}
