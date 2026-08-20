struct Bits {
	unsigned int value:4;
};

int
main(void)
{
	struct Bits bits;
	struct Bits *ptr = &bits;

	ptr->value = 7;

	if (++ptr->value != 8)
		return 1;
	if (ptr->value-- != 8)
		return 2;
	if (ptr->value != 7)
		return 3;

	return 0;
}
