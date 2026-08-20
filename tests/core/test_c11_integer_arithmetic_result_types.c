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
	signed char sc = -5;
	unsigned char uc = 250;
	short ss = -10;
	unsigned short us = 20;
	int si = -21;
	unsigned int ui = 7U;
	long sl = -30L;
	unsigned long ul = 9UL;
	long long sll = -40LL;
	unsigned long long ull = 11ULL;

	if (TYPEOF(sc - uc) != 1)
		return 1;
	if (TYPEOF(ss * us) != 1)
		return 2;
	if (TYPEOF(si / ui) != 2)
		return 3;
	if (TYPEOF(sl % ui) != 3)
		return 4;
	if (TYPEOF(sll * ul) != 6)
		return 5;
	if (TYPEOF(ull - ss) != 6)
		return 6;

	if ((sc - uc) != -255)
		return 7;
	if ((ss * us) != -200)
		return 8;
	if ((si / ui) != ((unsigned int)-21 / 7U))
		return 9;
	if ((sl % ui) != (-30L % 7U))
		return 10;
	if ((sll * ul) != ((unsigned long long)-40LL * 9UL))
		return 11;
	if ((ull - ss) != (11ULL - (unsigned long long)-10))
		return 12;

	return 42;
}
