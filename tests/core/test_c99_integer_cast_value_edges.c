int
main(void)
{
	unsigned int ui = (unsigned int)-1;
	unsigned short us = (unsigned short)-1L;
	unsigned char uc = (unsigned char)-1;
	signed char sc = (signed char)255U;
	int si = (int)4294967295U;
	unsigned long ul = (unsigned long)-2;
	unsigned long long ull = (unsigned long long)-3LL;
	unsigned char uc_big = (unsigned char)511U;
	unsigned short us_big = (unsigned short)131071UL;

	if (ui != 4294967295U)
		return 1;
	if (us != 65535)
		return 2;
	if (uc != 255)
		return 3;
	if (sc != -1)
		return 4;
	if (si != -1)
		return 5;
	if (ul != (unsigned long)-2)
		return 6;
	if (ull != (unsigned long long)-3LL)
		return 7;
	if (uc_big != 255)
		return 8;
	if (us_big != 65535)
		return 9;

	if (((unsigned int)-42) != 4294967254U)
		return 10;
	if (((signed char)128U) != -128)
		return 11;
	if (((unsigned char)256U) != 0)
		return 12;
	if (((unsigned short)65536UL) != 0)
		return 13;
	if (((int)(unsigned char)255U) != 255)
		return 14;

	return 42;
}
