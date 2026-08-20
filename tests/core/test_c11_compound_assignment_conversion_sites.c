int
main(void)
{
	unsigned char uc = 250;
	unsigned short us = 1;
	unsigned long ul = 0xfffffffful;

	uc += 1000U;
	if (uc != 226)
		return 1;

	us += -2L;
	if (us != 65535)
		return 2;

	uc <<= 9;
	if (uc != 0)
		return 3;

	us >>= 15;
	if (us != 1)
		return 4;

	ul &= 0xffu;
	if (ul != 0xfful)
		return 5;

	uc ^= 0x1ffu;
	if (uc != 255)
		return 6;

	return 42;
}
