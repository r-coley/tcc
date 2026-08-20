struct Bits {
	int a:3;
};

int
main(void)
{
	struct Bits bits;

	bits.a = 1;

	if (++bits.a != 2)
		return 1;
	if (bits.a != 2)
		return 2;

	return 0;
}
