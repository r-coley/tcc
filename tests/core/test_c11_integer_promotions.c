int
main(void)
{
	signed char sc;
	unsigned char uc;
	short ss;
	unsigned short us;

	sc = -5;
	uc = 250;
	ss = -1;
	us = 1;

	if ((int)(uc + 1) != 251)
		return 1;
	if ((int)(sc + uc) != 245)
		return 2;
	if ((int)(ss + us) != 0)
		return 3;
	if ((int)sizeof(uc + us) != (int)sizeof(int))
		return 4;

	return 42;
}
