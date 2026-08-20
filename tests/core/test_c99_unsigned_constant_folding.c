int
main(void)
{
	if ((42U - 43U) != 4294967295U)
		return 1;
	if (!((42U - 43U) > 0U))
		return 2;
	if ((0xffffffffU + 1U) != 0U)
		return 3;
	if ((0xffffffffU * 2U) != 4294967294U)
		return 4;
	if ((1U << 31) != 0x80000000U)
		return 5;
	if (((unsigned char)255 + (unsigned char)1) != 256)
		return 6;
	if (((unsigned short)65535 + (unsigned short)1) != 65536)
		return 7;
	if (((unsigned long)-1UL + 1UL) != 0UL)
		return 8;
	if (((unsigned long long)-1ULL + 1ULL) != 0ULL)
		return 9;

	return 42;
}
