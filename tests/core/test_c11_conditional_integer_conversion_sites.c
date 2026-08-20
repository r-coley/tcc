static int
take_uchar(unsigned char value)
{
	return value;
}

static int
take_schar(signed char value)
{
	return value;
}

static int
take_ushort(unsigned short value)
{
	return value;
}

static unsigned char
ret_uchar(int pick, int a, unsigned int b)
{
	return pick ? a : b;
}

static signed char
ret_schar(int pick, int a, unsigned int b)
{
	return pick ? a : b;
}

static unsigned short
ret_ushort(int pick, long a, unsigned int b)
{
	return pick ? a : b;
}

int
main(void)
{
	unsigned char uc_a = 1 ? 300 : 7U;
	unsigned char uc_b = 0 ? 300 : 260U;
	signed char sc_a = 1 ? -12 : 250U;
	unsigned short us_a = 1 ? -1L : 9U;
	unsigned short us_b = 0 ? -1L : 65537U;

	if (uc_a != 44)
		return 1;
	if (uc_b != 4)
		return 2;
	if (sc_a != -12)
		return 3;
	if (us_a != 65535)
		return 4;
	if (us_b != 1)
		return 5;

	if (take_uchar(1 ? 300 : 7U) != 44)
		return 6;
	if (take_uchar(0 ? 300 : 260U) != 4)
		return 7;
	if (take_schar(1 ? -12 : 250U) != -12)
		return 8;
	if (take_ushort(0 ? -1L : 65537U) != 1)
		return 9;

	if (ret_uchar(1, 300, 7U) != 44)
		return 10;
	if (ret_uchar(0, 300, 260U) != 4)
		return 11;
	if (ret_schar(1, -12, 250U) != -12)
		return 12;
	if (ret_ushort(1, -1L, 9U) != 65535)
		return 13;
	if (ret_ushort(0, -1L, 65537U) != 1)
		return 14;

	return 42;
}
