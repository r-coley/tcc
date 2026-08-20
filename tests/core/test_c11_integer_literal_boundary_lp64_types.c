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
	if (TYPEOF(4294967295U) != 2)
		return 1;
	if (TYPEOF(4294967296U) != 4)
		return 2;
	if (TYPEOF(9223372036854775807LL) != 5)
		return 3;
	if (TYPEOF(9223372036854775808ULL) != 6)
		return 4;
	if (TYPEOF(9223372036854775808UL) != 4)
		return 5;
	if (TYPEOF(18446744073709551615UL) != 4)
		return 6;
	if (TYPEOF(0xffffffffffffffffULL) != 6)
		return 7;

	if (4294967295U != 4294967295U)
		return 8;
	if (4294967296U != 4294967296UL)
		return 9;
	if (9223372036854775808UL != 9223372036854775808UL)
		return 10;
	if (18446744073709551615UL != 18446744073709551615UL)
		return 11;

	return 42;
}
