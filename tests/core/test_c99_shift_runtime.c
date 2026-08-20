int
main(void)
{
	volatile int one = 1;
	volatile int count = 5;
	volatile unsigned int high = 0x80000000U;

	if ((one << count) != 32)
		return 1;
	if ((high >> count) != 0x04000000U)
		return 2;
	if (((unsigned long)1 << 63) != 0x8000000000000000UL)
		return 3;
	if (((unsigned long)-1 >> 63) != 1UL)
		return 4;

	return 42;
}
