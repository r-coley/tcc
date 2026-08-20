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
	signed char sc = -1;
	unsigned char uc = 1;
	short ss = -1;
	unsigned short us = 1;
	int si = -1;
	unsigned int ui = 1U;
	long sl = -1L;
	unsigned long ul = 1UL;
	long long sll = -1LL;
	unsigned long long ull = 0xF0ULL;

	/* Bitwise operators use the usual arithmetic conversions. */
	if (TYPEOF(sc & uc) != 1)
		return 1;
	if (TYPEOF(ss | us) != 1)
		return 2;
	if (TYPEOF(si ^ ui) != 2)
		return 3;
	if (TYPEOF(sl & ui) != 3)
		return 4;
	if (TYPEOF(sll | ul) != 6)
		return 5;
	if (TYPEOF(ull ^ sll) != 6)
		return 6;
	if ((ull ^ sll) != (0xF0ULL ^ 18446744073709551615ULL))
		return 7;

	/* Shift operators promote each operand; result type is the promoted left operand. */
	if (TYPEOF(uc << ul) != 1)
		return 8;
	if (TYPEOF(ui << sll) != 2)
		return 9;
	if (TYPEOF(ss >> ui) != 1)
		return 10;
	if (TYPEOF(sll << ui) != 5)
		return 11;
	if ((sll << ui) != -2LL)
		return 12;
	if (TYPEOF(ull >> ui) != 6)
		return 13;
	if ((ull >> ui) != 0x78ULL)
		return 14;

	/* Relational, equality, and logical operators always produce int. */
	if (TYPEOF(ul < sll) != 1)
		return 15;
	if (TYPEOF(ui == sl) != 1)
		return 16;
	if (TYPEOF(ul && sll) != 1)
		return 17;
	if (TYPEOF(ull == sll) != 1)
		return 18;
	if ((ull == sll) != 0)
		return 19;

	return 42;
}
