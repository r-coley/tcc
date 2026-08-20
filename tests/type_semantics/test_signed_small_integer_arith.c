int
main(void)
{
	unsigned char uc = 255;
	signed char sc = -1;
	unsigned short us = 65535;
	short ss = -1;

	if ((uc + sc) != 254)
		return 1;
	if ((us + ss) != 65534)
		return 2;
	if ((uc < sc) != 0)
		return 3;
	if ((us < ss) != 0)
		return 4;
	if (((unsigned char)1 << 8) != 256)
		return 5;
	return 0;
}
