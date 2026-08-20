#define TYPEOF(x) _Generic((x), \
	unsigned char: 1, \
	signed char: 2, \
	short: 3, \
	unsigned short: 4, \
	default: 99)

int
main(void)
{
	unsigned long ul = 65535UL;
	unsigned int ui = 65535U;
	int minus_one = -1;
	long minus_two = -2L;

	if (TYPEOF((short)ui) != 3)
		return 1;
	if ((short)ui != -1)
		return 2;
	if (TYPEOF((short)ul) != 3)
		return 3;
	if ((short)ul != -1)
		return 4;
	if (TYPEOF((unsigned char)minus_one) != 1)
		return 5;
	if ((unsigned char)minus_one != 255)
		return 6;
	if (TYPEOF((unsigned short)minus_two) != 4)
		return 7;
	if ((unsigned short)minus_two != 65534)
		return 8;

	return 42;
}
