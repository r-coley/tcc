int
main(void)
{
	volatile int a = -17;
	volatile int b = 5;
	volatile unsigned int ua = 17U;
	volatile unsigned int ub = 5U;
	volatile long la = -123456789L;
	volatile long lb = 97L;

	if ((a / b) != -3)
		return 1;
	if ((a % b) != -2)
		return 2;
	if ((ua / ub) != 3U)
		return 3;
	if ((ua % ub) != 2U)
		return 4;
	if ((la / lb) != -1272750L)
		return 5;
	if ((la % lb) != -39L)
		return 6;

	return 42;
}
