int
main(void)
{
	unsigned int ui = 4000000000U;
	long sl = -1L;
	unsigned long ul = 0xFFFFFFFFFFFFFFFFUL;
	long long sll = -1LL;
	unsigned int one = 1U;
	long lneg = -2L;

	if (sizeof(long) != 8)
		return 100;

	/* unsigned int promotes into signed long on LP64. */
	if ((ui + sl) != 3999999999L)
		return 1;
	if ((ui < sl) != 0)
		return 2;

	/* signed long still wins when it can represent unsigned int. */
	if ((one + lneg) != -1L)
		return 3;
	if ((one < lneg) != 0)
		return 4;

	/* unsigned long and signed long long of equal width convert to unsigned long long. */
	if ((ul + sll) != 0xFFFFFFFFFFFFFFFEULL)
		return 5;
	if ((ul < sll) != 0)
		return 6;

	return 0;
}
