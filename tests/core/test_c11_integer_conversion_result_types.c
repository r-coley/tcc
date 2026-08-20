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
	unsigned long long ull = 1ULL;

	if (TYPEOF(sc + uc) != 1)
		return 1;
	if (TYPEOF(ss + us) != 1)
		return 2;
	if (TYPEOF(si + ui) != 2)
		return 3;
	if (TYPEOF(sl + ui) != 3)
		return 4;
	if (TYPEOF(sl + ul) != 4)
		return 5;
	if (TYPEOF(sll + ul) != 6)
		return 6;
	if (TYPEOF(si ? ui : sl) != 3)
		return 7;
	if (TYPEOF(si ? ull : sll) != 6)
		return 8;
	if (TYPEOF(+uc) != 1)
		return 9;

	return 42;
}
