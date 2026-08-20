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
	unsigned char uc = 7;
	unsigned short us = 9;
	int si = -3;
	unsigned int ui = 11U;
	long sl = -5L;
	unsigned long ul = 13UL;
	long long sll = -7LL;
	unsigned long long ull = 15ULL;

	if (TYPEOF(1 ? sc : uc) != 1)
		return 1;
	if (TYPEOF(0 ? us : ui) != 2)
		return 2;
	if (TYPEOF(1 ? sc : ul) != 4)
		return 3;
	if (TYPEOF(1 ? sl : ui) != 3)
		return 4;
	if (TYPEOF(1 ? sll : ul) != 6)
		return 5;
	if (TYPEOF(1 ? ull : sl) != 6)
		return 6;

	if ((1 ? sc : uc) != -1)
		return 7;
	if ((0 ? us : ui) != 11U)
		return 8;
	if ((1 ? sc : ul) != (unsigned long)-1)
		return 9;
	if ((1 ? sl : ui) != -5L)
		return 10;
	if ((0 ? sll : ul) != 13UL)
		return 11;
	if ((1 ? ull : sl) != 15ULL)
		return 12;

	if (si ? 0 : 1)
		return 13;

	return 42;
}
