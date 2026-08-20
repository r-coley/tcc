#define TYPEOF(x) _Generic((x), \
	int: 1, \
	unsigned int: 2, \
	long: 3, \
	unsigned long: 4, \
	default: 99)

int
main(void)
{
	signed char sc = -5;
	unsigned char uc = 250;
	short ss = -7;
	unsigned short us = 65530;

	if (TYPEOF(1 ? sc : uc) != 1)
		return 1;
	if (TYPEOF(0 ? ss : us) != 1)
		return 2;
	if (TYPEOF(1 ? uc : us) != 1)
		return 3;
	if (TYPEOF(0 ? sc : ss) != 1)
		return 4;

	if ((1 ? sc : uc) != -5)
		return 5;
	if ((0 ? ss : us) != 65530)
		return 6;
	if ((1 ? uc : us) != 250)
		return 7;
	if ((0 ? sc : ss) != -7)
		return 8;

	return 42;
}
