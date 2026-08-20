int
main(void)
{
	signed char sc = -1;
	unsigned char uc = 255;
	unsigned short us = 65535;
	int si = -1;
	unsigned ui = 1U;
	long sl = -1L;
	unsigned long ul = 1UL;

	if (sizeof(+sc) != sizeof(int))
		return 1;
	if ((+uc) != 255)
		return 2;
	if ((us + 1) != 65536)
		return 3;
	if ((si + ui) != 0U)
		return 4;
	if ((sl + ul) != 0UL)
		return 5;
	if (sizeof((unsigned char)1 << 8) != sizeof(int))
		return 6;
	if (((unsigned long)1 << 32) != 0x100000000UL)
		return 7;
	if (((unsigned long)-1 >> 63) != 1UL)
		return 8;

	return 42;
}
