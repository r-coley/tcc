int
main(void)
{
	unsigned u;
	unsigned long long ull;

	u = 0xffffffffU;
	ull = 0x100000000ULL;

	if (0x2a != 42)
		return 1;
	if (052 != 42)
		return 2;
	if (42U != 42)
		return 3;
	if (u <= 1U)
		return 4;
	if ((ull >> 32) != 1ULL)
		return 5;

	return 42;
}
