#define TYPEOF(x) _Generic((x), \
	int: 1, \
	unsigned int: 2, \
	long: 3, \
	unsigned long: 4, \
	default: 99)

enum Small {
	NEG = -1,
	POS = 2
};

int
main(void)
{
	signed char sc = -5;
	unsigned char uc = 1;
	unsigned short us = 2;
	unsigned int ui = 3U;
	long sl = -4L;
	enum Small e = NEG;

	if (TYPEOF(+sc) != 1)
		return 1;
	if (TYPEOF(-sc) != 1)
		return 2;
	if (TYPEOF(~uc) != 1)
		return 3;
	if (TYPEOF(+us) != 1)
		return 4;
	if (TYPEOF(-ui) != 2)
		return 5;
	if (TYPEOF(+ui) != 2)
		return 6;
	if (TYPEOF(~ui) != 2)
		return 7;
	if (TYPEOF(-sl) != 3)
		return 8;
	if (TYPEOF(+e) != 1)
		return 9;
	if (TYPEOF(-e) != 1)
		return 10;
	if (TYPEOF(~e) != 1)
		return 11;
	if (TYPEOF(!uc) != 1)
		return 12;

	if (+sc != -5)
		return 13;
	if (-sc != 5)
		return 14;
	if (~uc != -2)
		return 15;
	if (+us != 2)
		return 16;
	if ((unsigned int)-ui != (unsigned int)-3)
		return 17;
	if (+ui != 3U)
		return 18;
	if (~ui != ~3U)
		return 19;
	if (-sl != 4L)
		return 20;
	if (+e != -1)
		return 21;
	if (-e != 1)
		return 22;
	if (~e != 0)
		return 23;
	if (!uc != 0)
		return 24;

	return 42;
}
