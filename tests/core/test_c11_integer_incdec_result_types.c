#define TYPEOF(x) _Generic((x), \
	unsigned char: 1, \
	signed char: 2, \
	unsigned short: 3, \
	short: 4, \
	default: 99)

int
main(void)
{
	unsigned char uc = 1;
	signed char sc = 1;
	unsigned short us = 1;
	short ss = 1;

	if (TYPEOF(++uc) != 1)
		return 1;
	if (TYPEOF(uc++) != 1)
		return 2;
	if (TYPEOF(--sc) != 2)
		return 3;
	if (TYPEOF(sc--) != 2)
		return 4;
	if (TYPEOF(++us) != 3)
		return 5;
	if (TYPEOF(us--) != 3)
		return 6;
	if (TYPEOF(++ss) != 4)
		return 7;
	if (TYPEOF(ss--) != 4)
		return 8;

	uc = 255;
	++uc;
	if (uc != 0)
		return 9;

	sc = 127;
	++sc;
	if (sc != -128)
		return 10;

	us = 65535;
	++us;
	if (us != 0)
		return 11;

	ss = 32767;
	++ss;
	if (ss != -32768)
		return 12;

	return 42;
}
