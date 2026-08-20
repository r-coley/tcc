int
main(void)
{
	if (sizeof(2147483647) != 4)
		return 1;
	if (sizeof(2147483648) != 8)
		return 2;
	if (sizeof(0x7fffffff) != 4)
		return 3;
	if (sizeof(0x80000000) != 4)
		return 4;
	if (!((0x80000000) > 0))
		return 5;
	if (sizeof(0xffffffff) != 4)
		return 6;
	if (!(-1 == 0xffffffff))
		return 7;
	if (sizeof(0x100000000) != 8)
		return 8;
	if (sizeof(42L) != 8)
		return 9;
	if (sizeof(42UL) != 8)
		return 10;
	if (sizeof(42LL) != 8)
		return 11;
	if (sizeof(42ULL) != 8)
		return 12;

	return 42;
}
