struct Bits {
	signed int s3 : 3;
	unsigned int u3 : 3;
	unsigned int u31 : 31;
};

int
main(void)
{
	struct Bits b;

	b.s3 = 3;
	if (b.s3 != 3)
		return 1;
	b.s3 = 4;
	if (b.s3 != -4)
		return 2;
	b.s3 = -1;
	if (b.s3 != -1)
		return 3;
	if ((b.s3 + 1) != 0)
		return 4;
	if (sizeof(+b.s3) != sizeof(int))
		return 5;

	b.u3 = 7;
	if (b.u3 != 7)
		return 6;
	if ((b.u3 + 1) != 8)
		return 7;
	if (sizeof(+b.u3) != sizeof(int))
		return 8;
	if ((b.u3 - 8) != -1)
		return 9;

	b.u31 = 0x7fffffffU;
	if (b.u31 != 0x7fffffffU)
		return 10;
	if ((b.u31 + 1U) != 0x80000000U)
		return 11;

	return 42;
}
