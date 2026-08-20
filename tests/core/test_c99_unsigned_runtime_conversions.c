int
main(void)
{
	volatile int si = -1;
	volatile unsigned int ui = 1U;
	volatile unsigned int umax = 0xffffffffU;
	volatile long sl = -1L;
	volatile unsigned long ul = 1UL;
	volatile unsigned long ulmax = (unsigned long)-1L;
	volatile long long sll = -1LL;
	volatile unsigned long long ull = 1ULL;
	volatile unsigned long long ullmax = (unsigned long long)-1LL;
	volatile unsigned int shift = 0x80000000U;

	if (!(si > ui))
		return 1;
	if ((umax + ui) != 0U)
		return 2;
	if ((umax * 2U) != 0xfffffffeU)
		return 3;
	if ((umax / 2U) != 0x7fffffffU)
		return 4;
	if ((umax % 2U) != 1U)
		return 5;
	if (!(sl > ul))
		return 6;
	if ((ulmax + ul) != 0UL)
		return 7;
	if (!(sll > ull))
		return 8;
	if ((ullmax + ull) != 0ULL)
		return 9;
	if ((shift >> 31) != 1U)
		return 10;

	return 42;
}
