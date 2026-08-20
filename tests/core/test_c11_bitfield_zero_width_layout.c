struct Bits {
	unsigned int a : 3;
	unsigned int : 0;
	unsigned char c;
};

int
main(void)
{
	struct Bits bits;
	char *base = (char *)&bits;
	char *cptr = (char *)&bits.c;

	bits.a = 7;
	bits.c = 42;

	if (bits.a != 7)
		return 1;
	if (bits.c != 42)
		return 2;
	if ((int)(cptr - base) != (int)sizeof(unsigned int))
		return 3;

	return 42;
}
