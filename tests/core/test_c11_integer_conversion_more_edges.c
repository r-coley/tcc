int
main(void)
{
	unsigned char uc = 250;
	signed char sc = 120;
	unsigned short us = 65530;
	unsigned int ui = 1U;
	int si = -2;
	long l = -1L;
	unsigned int big = 4000000000U;

	uc += 10;
	if (uc != 4)
		return 1;
	sc += 10;
	if (sc != -126)
		return 2;
	us += 10;
	if (us != 4)
		return 3;
	uc *= 64;
	if (uc != 0)
		return 4;

	if (~(unsigned char)0 != -1)
		return 5;
	if (sizeof(~(unsigned char)0) != sizeof(int))
		return 6;
	if (sizeof((unsigned char)1 ? (short)2 : (unsigned short)3) != sizeof(int))
		return 7;
	if (((unsigned char)1 ? si : ui) != 4294967294U)
		return 8;
	if (((unsigned char)0 ? big : l) != -1L)
		return 9;

	return 42;
}
