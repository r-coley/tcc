static int take_uchar(unsigned char value)
{
	return value;
}

static int take_ushort(unsigned short value)
{
	return value;
}

static int take_schar(signed char value)
{
	return value;
}

static unsigned char ret_uchar(int value)
{
	return value;
}

static unsigned short ret_ushort(unsigned long value)
{
	return value;
}

static signed char ret_schar(int value)
{
	return value;
}

int main(void)
{
	unsigned char uc = 300;
	unsigned short us = 65537UL;
	signed char sc = -12;
	short ss = -1234;

	if (uc != 44)
		return 1;
	if (us != 1)
		return 2;
	if (sc != -12)
		return 3;
	if (ss != -1234)
		return 4;
	if (take_uchar(300) != 44)
		return 5;
	if (take_ushort(65537UL) != 1)
		return 6;
	if (take_schar(-12) != -12)
		return 7;
	if (ret_uchar(300) != 44)
		return 8;
	if (ret_ushort(65537UL) != 1)
		return 9;
	if (ret_schar(-12) != -12)
		return 10;

	return 42;
}
